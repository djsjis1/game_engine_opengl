#ifndef RESOURCEMANAGER_H
#define RESOURCEMANAGER_H

#include "Render/MeshFactory.h"
#include "Render/ObjLoader.h" // .obj 模型加载（名字分发在 get()）
#include "Core/GamePaths.h"   // 模型相对路径 → 运行时绝对路径
#include <QImage>
#include <QDir>
#include <vector>
#include <map>
#include <string>
#include <cstdint>

// ============================================================
// ResourceManager — 资源管理器（句柄 + 缓存 + 引用计数 + 延迟 GPU 操作）
//
// 【核心思想 1：句柄取代裸指针】（Unreal/Unity 同款）
//     实体A ─┐
//     实体B ─┼─ meshHandle=1 ──→ ["cube" refCount=3, VAO=5]
//     实体C ─┘                    ↑ 100 个立方体只占 1 份显存
//
// 【核心思想 2：延迟 GPU 操作 —— 解除 GL 上下文时序耦合】
//   acquireMesh()  = 纯登记（不碰 GL！任何时机/调用方都安全）
//   GPU 数据创建   = 推迟到首次 get()（渲染循环里天然有上下文）
//   GPU 数据销毁   = refCount 归零只挂"待删列表"，
//                    flushPendingDeletes() 在每帧渲染前统一执行
//
//   于是：命令系统/序列化/Observer 调 acquire/release 都不需要
//   makeCurrent 包裹 —— 上层代码与渲染上下文彻底解耦
//
// 【生命周期自动化】配合 Engine 里的 MeshComponent OnRemove Observer：
//   实体无论怎么死（destruct/级联删除/移除组件）→ 自动 release
//   → "记得释放"不再是任何调用方的责任
// ============================================================
class ResourceManager
{
public:
    static ResourceManager &instance()
    {
        static ResourceManager rm;
        return rm;
    }

    /// 获取网格资源句柄（同名共享缓存）。纯登记，不碰 GL，任何时机可调
    /// name: "cube"（未来扩展 "sphere"/"plane"/模型文件路径）
    uint32_t acquireMesh(const std::string &name)
    {
        // 缓存命中 → 引用计数 +1，共享同一份资源
        auto it = m_byName.find(name);
        if (it != m_byName.end())
        {
            m_entries[it->second - 1].refCount++;
            return it->second;
        }

        // 未命中 → 只登记（GPU 数据等首次 get() 时在渲染线程创建）
        MeshEntry entry;
        entry.name = name;
        entry.refCount = 1;
        entry.loaded = false;

        // 优先复用空槽（防句柄数组无限增长）
        uint32_t handle = 0;
        for (size_t i = 0; i < m_entries.size(); ++i)
        {
            if (m_entries[i].refCount == 0 && !m_entries[i].loaded)
            {
                m_entries[i] = entry;
                handle = static_cast<uint32_t>(i + 1);
                break;
            }
        }
        if (!handle)
        {
            m_entries.push_back(entry);
            handle = static_cast<uint32_t>(m_entries.size());
        }
        m_byName[name] = handle;
        return handle;
    }

    /// 引用计数 +1（复制实体等场景手动增引用）
    void addRef(uint32_t handle)
    {
        if (isValidHandle(handle))
            m_entries[handle - 1].refCount++;
    }

    /// 释放引用。纯记账，不碰 GL（归零时 GPU 数据挂入待删列表）
    void releaseMesh(uint32_t handle)
    {
        if (!isValidHandle(handle))
            return;
        auto &e = m_entries[handle - 1];
        if (e.refCount <= 0)
            return; // 已释放，防重复 release
        if (--e.refCount > 0)
            return; // 还有人在用
        // 归零：GPU 数据移入待删列表（延迟到有 GL 上下文时真正删除）
        if (e.loaded)
        {
            m_pendingDelete.push_back(e.gpu);
            e.gpu = GpuMesh{};
            e.loaded = false;
        }
        m_byName.erase(e.name);
    }

    /// 句柄 → GPU 数据。首次访问时现场创建（须在 GL 上下文内，渲染循环天然满足）
    const GpuMesh *get(uint32_t handle)
    {
        if (!isValidHandle(handle))
            return nullptr;
        auto &e = m_entries[handle - 1];
        if (e.refCount <= 0)
            return nullptr; // 已释放的句柄 → 安全返回空
        if (!e.loaded)
        {
            // 延迟创建：第一次被渲染时才真正上传 GPU。
            // 名字分发：".obj" 结尾 = 模型文件（相对资源根，如 "models/tree.obj"）；
            // 其余 = 程序化几何（"cube"）。加载失败回退 cube（错路径可见方块提示）
            if (e.name.size() > 4 &&
                e.name.compare(e.name.size() - 4, 4, ".obj") == 0)
            {
                std::vector<Vertex> verts;
                std::string abs =
                    (GamePaths::root() + QString::fromUtf8(e.name.c_str())).toStdString();
                if (ObjLoader::loadFile(abs, verts))
                    factory().createFromVertices(e.gpu, verts);
                else
                    factory().createCube(e.gpu); // 回退：至少看得见
            }
            else
                factory().createCube(e.gpu);
            e.loaded = true;
        }
        return &e.gpu;
    }

    /// 执行延迟删除（每帧渲染前调用，须在 GL 上下文内）
    void flushPendingDeletes()
    {
        for (auto &gpu : m_pendingDelete)
            factory().destroyMesh(gpu);
        m_pendingDelete.clear();
        for (auto &tex : m_pendingDeleteTex)
            factory().destroyTexture(tex);
        m_pendingDeleteTex.clear();
    }

    // ============================================================
    // 精灵帧集（SpriteFrames，对标 Godot）—— 与 Mesh 完全同模式：
    //   acquire = 纯登记（扫目录收集图片路径，不碰 GL）
    //   getFrame = 首次访问时 QImage 解码 + 上传 GPU（渲染循环内）
    //   release = 纯记账，归零挂待删
    //   配合 SpriteComponent OnRemove Observer 自动释放
    // ============================================================

    /// 登记精灵帧集：扫描目录下全部图片（按文件名排序 = 帧序）
    /// 返回句柄；目录无效/无图返回 0
    uint32_t acquireSprite(const std::string &folder)
    {
        auto it = m_spriteByName.find(folder);
        if (it != m_spriteByName.end())
        {
            m_sprites[it->second - 1].refCount++;
            return it->second;
        }
        // 扫目录（纯文件系统操作，不碰 GL）
        QDir dir(QString::fromUtf8(folder.c_str()));
        if (!dir.exists())
            return 0;
        QStringList files = dir.entryList(
            {"*.png", "*.jpg", "*.jpeg", "*.bmp"}, QDir::Files, QDir::Name);
        if (files.isEmpty())
            return 0;

        SpriteEntry entry;
        entry.folder = folder;
        entry.refCount = 1;
        for (auto &f : files)
            entry.framePaths.push_back(dir.absoluteFilePath(f).toStdString());
        entry.textures.resize(entry.framePaths.size()); // 全部未加载

        uint32_t handle = 0;
        for (size_t i = 0; i < m_sprites.size(); ++i)
            if (m_sprites[i].refCount == 0)
            {
                m_sprites[i] = std::move(entry);
                handle = static_cast<uint32_t>(i + 1);
                break;
            }
        if (!handle)
        {
            m_sprites.push_back(std::move(entry));
            handle = static_cast<uint32_t>(m_sprites.size());
        }
        m_spriteByName[folder] = handle;
        return handle;
    }

    /// 帧数（编辑器显示/动画打帧参考）
    int spriteFrameCount(uint32_t handle) const
    {
        if (handle < 1 || handle > m_sprites.size())
            return 0;
        return (int)m_sprites[handle - 1].framePaths.size();
    }

    /// 取指定帧的纹理（首次访问时解码+上传，须在 GL 上下文内）
    /// frame 越界自动钳制到有效范围（动画插值精度不影响安全）
    const GpuTexture *getSpriteFrame(uint32_t handle, int frame)
    {
        if (handle < 1 || handle > m_sprites.size())
            return nullptr;
        auto &e = m_sprites[handle - 1];
        if (e.refCount <= 0 || e.framePaths.empty())
            return nullptr;
        if (frame < 0)
            frame = 0;
        if (frame >= (int)e.framePaths.size())
            frame = (int)e.framePaths.size() - 1;

        GpuTexture &tex = e.textures[frame];
        if (tex.id == 0)
        {
            // 延迟加载：QImage 解码 → RGBA → 上传 GPU
            QImage img(QString::fromUtf8(e.framePaths[frame].c_str()));
            if (img.isNull())
                return nullptr;
            QImage rgba = img.convertToFormat(QImage::Format_RGBA8888)
                              .mirrored(); // GL 纹理原点在左下，图片在左上 → 翻转
            factory().createTexture(tex, rgba.constBits(),
                                    rgba.width(), rgba.height());
        }
        return &tex;
    }

    /// 释放精灵帧集引用（纯记账，归零挂待删）
    void releaseSprite(uint32_t handle)
    {
        if (handle < 1 || handle > m_sprites.size())
            return;
        auto &e = m_sprites[handle - 1];
        if (e.refCount <= 0)
            return;
        if (--e.refCount > 0)
            return;
        for (auto &tex : e.textures)
            if (tex.id)
                m_pendingDeleteTex.push_back(tex);
        e.textures.clear();
        e.framePaths.clear();
        m_spriteByName.erase(e.folder);
    }

    // ============================================================
    // 单张纹理（材质 albedo 贴图）—— 复用精灵帧集机制：
    //   一张贴图 = 一个"只有 1 帧的精灵帧集"，getSpriteFrame(h,0) 取它。
    //   材质贴图和精灵帧共享同一份缓存/延迟加载/引用计数逻辑。
    // ============================================================

    /// 登记单张纹理（绝对路径）。返回句柄；文件无效返回 0
    uint32_t acquireTexture(const std::string &absPath)
    {
        auto it = m_spriteByName.find(absPath);
        if (it != m_spriteByName.end())
        {
            m_sprites[it->second - 1].refCount++;
            return it->second;
        }
        QImage probe(QString::fromUtf8(absPath.c_str()));
        if (probe.isNull())
            return 0; // 文件不存在/非图片
        SpriteEntry entry;
        entry.folder = absPath; // 复用 folder 字段当缓存 key
        entry.refCount = 1;
        entry.framePaths.push_back(absPath); // 单帧
        entry.textures.resize(1);
        uint32_t handle = 0;
        for (size_t i = 0; i < m_sprites.size(); ++i)
            if (m_sprites[i].refCount == 0)
            {
                m_sprites[i] = std::move(entry);
                handle = static_cast<uint32_t>(i + 1);
                break;
            }
        if (!handle)
        {
            m_sprites.push_back(std::move(entry));
            handle = static_cast<uint32_t>(m_sprites.size());
        }
        m_spriteByName[absPath] = handle;
        return handle;
    }

    /// 取纹理（材质贴图用；就是取该句柄的第 0 帧）
    const GpuTexture *getTexture(uint32_t handle)
    {
        return getSpriteFrame(handle, 0);
    }

    /// 程序退出时清理全部资源（须在 GL 上下文销毁前调用，如 cleanupGL）
    void releaseAll()
    {
        flushPendingDeletes();
        for (auto &e : m_entries)
            if (e.loaded)
                factory().destroyMesh(e.gpu);
        m_entries.clear();
        m_byName.clear();
        for (auto &s : m_sprites)
            for (auto &tex : s.textures)
                if (tex.id)
                    factory().destroyTexture(tex);
        m_sprites.clear();
        m_spriteByName.clear();
        // ★ 必须在此销毁 MeshFactory（含 QOpenGLFunctions 基类）！
        // ResourceManager 是静态单例，若等静态析构阶段才销毁，
        // GL 上下文早已不在 → Qt 引用计数断言崩溃（Debug Error!）
        delete m_factory;
        m_factory = nullptr;
    }

private:
    ResourceManager() = default;

    /// MeshFactory 惰性创建（堆分配，生命周期手控，避开静态析构陷阱）
    MeshFactory &factory()
    {
        if (!m_factory)
            m_factory = new MeshFactory;
        return *m_factory;
    }

    /// 一条缓存记录：GPU 数据 + 引用计数 + 名字 + 是否已上传
    struct MeshEntry
    {
        GpuMesh gpu;
        int refCount = 0;
        bool loaded = false; // GPU 数据是否已创建（延迟加载标志）
        std::string name;
    };

    bool isValidHandle(uint32_t h) const
    {
        return h >= 1 && h <= m_entries.size();
    }

    std::vector<MeshEntry> m_entries;         // 句柄-1 = 下标
    std::map<std::string, uint32_t> m_byName; // 名字 → 句柄（缓存查找）
    std::vector<GpuMesh> m_pendingDelete;     // 待删列表（延迟 glDelete）
    MeshFactory *m_factory = nullptr;         // 堆分配！releaseAll 时销毁（见上）

    /// 精灵帧集记录：帧路径列表 + 逐帧延迟加载的纹理
    struct SpriteEntry
    {
        std::string folder;
        std::vector<std::string> framePaths; // 按文件名排序 = 帧序
        std::vector<GpuTexture> textures;    // 与 framePaths 对齐，id==0 未加载
        int refCount = 0;
    };
    std::vector<SpriteEntry> m_sprites;
    std::map<std::string, uint32_t> m_spriteByName;
    std::vector<GpuTexture> m_pendingDeleteTex;
};

#endif
