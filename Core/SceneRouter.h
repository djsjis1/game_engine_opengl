#ifndef ENGINE_SCENEROUTER_H
#define ENGINE_SCENEROUTER_H

#include <string>

// ============================================================
// SceneRouter — 运行时场景切换的"调度台"（多场景游戏的地基）
//
// 【为什么必须延迟切换】脚本调 change_scene 时，引擎正在遍历实体
//   执行脚本——此刻销毁所有实体 = 遍历中删容器 = 崩溃。
//   所以 Lua API 只"登记请求"，真正的加载由帧循环在【帧末】执行
//   （所有 System 跑完、没人再碰实体时）。Godot change_scene 同款 deferred。
//
// 【路径约定】相对资源根的路径（"scenes/level2.json"）：
//   开发模式拼 ASSET_DIR、导出包拼 exe/assets/ —— 存档里存它跨环境有效
// ============================================================
class SceneRouter
{
public:
    static SceneRouter &instance()
    {
        static SceneRouter s;
        return s;
    }

    /// 脚本请求切场景（帧末生效）；重复请求以最后一次为准
    void requestChange(const std::string &relPath) { m_pending = relPath; }

    /// 帧循环查询：有待切换的场景吗
    bool hasPending() const { return !m_pending.empty(); }

    /// 帧循环取走请求（取完即清，防止重复加载）
    std::string takePending()
    {
        std::string p = std::move(m_pending);
        m_pending.clear();
        return p;
    }

    /// 当前场景（相对路径）——脚本 get_scene() 判断自己在哪关
    void setCurrent(const std::string &relPath) { m_current = relPath; }
    const std::string &current() const { return m_current; }

    /// Play 期间是否切过场景（编辑器退出 Play 时判断要不要恢复编辑场景）
    void markChanged() { m_changedInPlay = true; }
    bool changedInPlay() const { return m_changedInPlay; }
    void resetChanged() { m_changedInPlay = false; }

private:
    SceneRouter() = default;
    std::string m_pending;        // 待切换（空=无请求）
    std::string m_current;        // 当前场景相对路径
    bool m_changedInPlay = false; // Play 期间发生过切换
};

#endif
