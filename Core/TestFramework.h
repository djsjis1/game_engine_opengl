#ifndef TESTFRAMEWORK_H
#define TESTFRAMEWORK_H

#include <atomic>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

// ============================================================
// TestFramework — 引擎运行时自检/统计框架（AI Agent 排障接口）
//
// 【为什么存在】单元测试（GoogleTest）只能验证"预期行为"；用户报告
//   的 bug（拖拽不动、相机不飞）需要的是"运行时状态快照"——
//   Agent 让用户执行一条命令，拿到的数据即可定位 90% 的问题。
//
// 【三个层次】
//   ① 断言宏 CHECK / EXPECT：代码内嵌不变量检查（仅测试构建编译）
//   ② 统计计数器：DrawCall/uniform/实体数等（atomic，无锁零阻塞）
//   ③ 报告：statsReport() 生成多行文本快照（Lua API 可读）
//
// 【零开销开关】ENABLE_ENGINE_TESTING=0 时所有宏展开为空、
//   统计接口为 inline 空操作，Release 构建无任何运行时成本。
// ============================================================

// ---- 编译期开关：CMake option(ENABLE_ENGINE_TESTING) 注入 ----
#ifndef ENABLE_ENGINE_TESTING
#define ENABLE_ENGINE_TESTING 0 // 默认关：生产构建零开销
#endif

namespace EngineTest
{

    // ============================================================
    // 统计计数器（atomic：渲染线程写、Agent 读，无需互斥锁）
    // ============================================================
    struct Stats
    {
        static constexpr std::size_t kFrameTimingSamples = 120;
        std::atomic<uint64_t> drawCalls{0};     // 本帧 DrawCall 次数（每帧归零）
        std::atomic<uint64_t> uniformSets{0};   // 本帧 uniform 设置次数
        std::atomic<uint64_t> shadowPasses{0};  // 本帧阴影深度 Pass 次数
        std::atomic<uint64_t> textureBinds{0};  // 本帧纹理绑定次数
        std::atomic<uint64_t> frameCounter{0};  // 累计帧数（不归零）
        std::array<std::atomic<uint64_t>, kFrameTimingSamples> paintCpuUs{};
        std::atomic<uint64_t> paintSampleCount{0};
        std::atomic<uint64_t> paintLastUs{0};
        std::atomic<uint64_t> entityCount{0};   // 最近一次统计的实体总数
        std::atomic<uint64_t> checkFailures{0}; // 断言失败累计
        std::atomic<uint64_t> checkPasses{0};   // 断言通过累计
    };

    /// 全局单例（Meyers Singleton，线程安全初始化）
    Stats &stats();

    /// 每帧开始调用：把"本帧"计数归零（累计值保留）
    void beginFrame();

    /// 记录一次 paintGL CPU 提交耗时；仅在 ENABLE_ENGINE_TESTING 构建中生效。
    void recordPaintCpuTime(uint64_t microseconds);

    /// 生成多行文本快照（Lua engine_stats() / Agent 排障用）
    std::string statsReport();

    /// 运行全部自检项，返回 "PASS n / FAIL m" 摘要（engine_self_check() 用）
    std::string runSelfCheck();

} // namespace EngineTest

// ============================================================
// 断言宏（仅 ENABLE_ENGINE_TESTING=1 时展开）
// ============================================================
#if ENABLE_ENGINE_TESTING
/// CHECK：失败时返回 false 并计数（表达式内联在调用点，便于设断点）
#define ET_CHECK(expr, what) \
    (::EngineTest::checkImpl((expr), #expr, (what), __FILE__, __LINE__))
#else
#define ET_CHECK(expr, what) (true) // 生产构建：表达式不评估（零开销）
#endif

namespace EngineTest
{
    /// CHECK 实现（测试构建）：记录结果并返回 expr 的值
    bool checkImpl(bool ok, const char *expr, const char *what,
                   const char *file, int line);

    /// 帧统计快捷接口（RenderWidget 调用；宏关闭时为空操作）
    inline void countDrawCall()
    {
        if (ENABLE_ENGINE_TESTING)
            ++stats().drawCalls;
    }
    inline void countUniform()
    {
        if (ENABLE_ENGINE_TESTING)
            ++stats().uniformSets;
    }
    inline void countTextureBind()
    {
        if (ENABLE_ENGINE_TESTING)
            ++stats().textureBinds;
    }
    inline void countShadowPass()
    {
        if (ENABLE_ENGINE_TESTING)
            ++stats().shadowPasses;
    }
} // namespace EngineTest

#endif
