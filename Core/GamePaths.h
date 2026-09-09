#ifndef ENGINE_GAMEPATHS_H
#define ENGINE_GAMEPATHS_H

#include <QCoreApplication>
#include <QDir>
#include <QString>

// ============================================================
// GamePaths — 运行时资源路径解析（导出/开发双模式）
//
// 【为什么需要】ASSET_DIR/SHADER_DIR 是编译期注入的绝对路径，
//   开发机上没问题，但导出的游戏拷到别人电脑上就全断了。
//   解决：运行时判定 —— exe 旁边有 assets/ 目录 = 导出模式，
//   资源从 assets/ 读；没有 = 开发模式，回退编译期路径。
//   （Godot 同思路：导出包里资源跟着 exe 走，编辑器里用项目路径）
//
// 【路径重定位】场景 JSON 里存的是开发机绝对路径
//   （如 D:/code/.../scripts/foo.lua）。导出模式下 rebase() 把
//   "scripts/" 及之后的部分截出来，拼到运行时资源根上。
// ============================================================
namespace GamePaths
{
    /// 是否运行在"导出包"模式（exe 旁存在 assets/ 目录）
    inline bool isPackaged()
    {
        static const bool packaged =
            QDir(QCoreApplication::applicationDirPath() + "/assets").exists();
        return packaged;
    }

    /// 资源根目录（以 / 结尾）：导出=exe旁assets/，开发=ASSET_DIR
    inline QString root()
    {
#ifdef ASSET_DIR
        static const QString r = isPackaged()
                                     ? QCoreApplication::applicationDirPath() + "/assets/"
                                     : QString(ASSET_DIR);
#else
        static const QString r = QCoreApplication::applicationDirPath() + "/assets/";
#endif
        return r;
    }

    /// Shader 目录（以 / 结尾）：导出=assets/Shaders/，开发=SHADER_DIR
    inline QString shaderDir()
    {
#ifdef SHADER_DIR
        static const QString s = isPackaged() ? root() + "Shaders/"
                                              : QString(SHADER_DIR);
#else
        static const QString s = root() + "Shaders/";
#endif
        return s;
    }

    /// 把开发机绝对路径重定位到运行时资源根（仅导出模式需要）。
    /// 例：D:/code/.../assets/scripts/foo.lua → <exe>/assets/scripts/foo.lua
    /// 原理：找到已知资源目录名（scripts/anims/sprites/scenes/prefabs），
    /// 从它开始截断，拼到 root() 上。找不到标记则原样返回。
    inline QString rebase(const QString &devPath)
    {
        if (!isPackaged() || devPath.isEmpty())
            return devPath;
        QString p = devPath;
        p.replace('\\', '/'); // 统一分隔符再找标记
        static const char *markers[] = {"scripts/", "anims/", "sprites/",
                                        "scenes/", "prefabs/", "models/"};
        for (const char *m : markers)
        {
            int idx = p.lastIndexOf(m);
            if (idx >= 0)
                return root() + p.mid(idx);
        }
        return devPath; // 无已知标记：原样（可能已是相对路径）
    }
}

#endif
