#ifndef ENGINE_SAVEGAME_H
#define ENGINE_SAVEGAME_H

#include <QCoreApplication>
#include <QDir>
#include <fstream>
#include <string>
#include <nlohmann/json.hpp>

// ============================================================
// SaveGame — 玩家存档（键值对 + 槽位落盘）
//
// 【存档 vs 场景保存的分工】
//   SceneSerializer：开发者的"设计态"（关卡长什么样）
//   SaveGame：      玩家的"运行时进度"（血量/金币/位置/过关标记）
//   只存会变的游戏状态，不存整个场景（Godot 同哲学：
//   引擎只给存取工具，存什么由游戏脚本自己决定——机制与策略分离）
//
// 【工作流】脚本 save_set 写内存 → save_write(槽位) 落盘；
//   读档 save_load(槽位) → save_get 取值恢复状态。
//   内存表是"待保存的草稿"，玩家没按保存就退出 = 丢草稿（各游戏惯例）。
//
// 【存档位置】exe 旁 saves/ 目录（便携式，Godot user:// 的简化版）：
//   开发模式 = build/.../Debug/saves/，导出包 = 游戏目录/saves/
//   不依赖工作目录（绝对路径规范），玩家数据不混进 assets/（游戏资源）
// ============================================================
class SaveGame
{
public:
    static SaveGame &instance()
    {
        static SaveGame s;
        return s;
    }

    // ---- 内存键值（脚本 save_set/save_get）----
    void set(const std::string &key, const nlohmann::json &v) { m_data[key] = v; }

    nlohmann::json get(const std::string &key) const
    {
        auto it = m_data.find(key);
        return it == m_data.end() ? nlohmann::json() : *it;
    }

    // ---- 落盘 / 读档 / 查询（slot 从 1 开始）----
    bool write(int slot)
    {
        QDir().mkpath(dirPath()); // 首次保存自动建目录
        std::ofstream f(filePath(slot).toStdString());
        if (!f.is_open())
            return false;
        f << m_data.dump(2); // nlohmann dump 输出天然 UTF-8（编码规范）
        f.flush();
        return f.good(); // 检查写入成功（磁盘满/IO 错时返回 false，别谎报成功）
    }

    bool load(int slot)
    {
        std::ifstream f(filePath(slot).toStdString());
        if (!f.is_open())
            return false;
        nlohmann::json j = nlohmann::json::parse(f, nullptr, false);
        if (j.is_discarded())
            return false; // 存档损坏：不动内存数据（别用坏档覆盖进度）
        m_data = std::move(j);
        return true;
    }

    bool exists(int slot) const
    {
        return QDir().exists(filePath(slot));
    }

    void clear() { m_data.clear(); } // 新游戏：清空内存草稿

private:
    SaveGame() = default;

    static QString dirPath()
    {
        return QCoreApplication::applicationDirPath() + "/saves";
    }
    static QString filePath(int slot)
    {
        return dirPath() + QString("/save%1.json").arg(slot);
    }

    nlohmann::json m_data; // 内存中的"待保存草稿"
};

#endif
