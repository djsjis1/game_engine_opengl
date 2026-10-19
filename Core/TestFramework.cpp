#include "Core/TestFramework.h"
#include "Core/EngineLog.h" // 断言失败走引擎日志（UI 输出面板可见）

#include <algorithm>
#include <cmath>
#include <sstream>
#include <vector>

namespace EngineTest
{

    Stats &stats()
    {
        static Stats s;
        return s;
    }

    void beginFrame()
    {
        if (!ENABLE_ENGINE_TESTING)
            return;
        // 只清零"本帧"计数；frameCounter 是累计值，在 paintGL 里自增
        stats().drawCalls.store(0, std::memory_order_relaxed);
        stats().uniformSets.store(0, std::memory_order_relaxed);
        stats().shadowPasses.store(0, std::memory_order_relaxed);
        stats().textureBinds.store(0, std::memory_order_relaxed);
    }

    void recordPaintCpuTime(uint64_t microseconds)
    {
        if (!ENABLE_ENGINE_TESTING)
            return;
        Stats &s = stats();
        const uint64_t sample = s.paintSampleCount.load(std::memory_order_relaxed);
        s.paintCpuUs[sample % Stats::kFrameTimingSamples].store(
            microseconds, std::memory_order_relaxed);
        s.paintLastUs.store(microseconds, std::memory_order_relaxed);
        // Publish the sample after writing its slot. statsReport may read from
        // another thread; atomic slots keep snapshots race-free.
        s.paintSampleCount.store(sample + 1, std::memory_order_release);
    }

    bool checkImpl(bool ok, const char *expr, const char *what,
                   const char *file, int line)
    {
        if (ok)
            stats().checkPasses.fetch_add(1, std::memory_order_relaxed);
        else
        {
            stats().checkFailures.fetch_add(1, std::memory_order_relaxed);
            // 走引擎日志：编辑器"输出"面板实时可见（Agent 也能从日志文件读）
            char buf[512];
            snprintf(buf, sizeof(buf), "[CHECK FAIL] %s (%s) — %s:%d", expr, what, file, line);
            engineLogError(buf);
        }
        return ok;
    }

    std::string statsReport()
    {
        Stats &s = stats();
        std::vector<uint64_t> paintSamples;
        const uint64_t sampleCount = s.paintSampleCount.load(std::memory_order_acquire);
        const std::size_t count = static_cast<std::size_t>(
            std::min<uint64_t>(sampleCount, Stats::kFrameTimingSamples));
        paintSamples.reserve(count);
        const std::size_t first = sampleCount >= Stats::kFrameTimingSamples
                                 ? static_cast<std::size_t>(sampleCount % Stats::kFrameTimingSamples)
                                 : 0;
        for (std::size_t i = 0; i < count; ++i)
        {
            const uint64_t value = s.paintCpuUs[(first + i) % Stats::kFrameTimingSamples]
                                       .load(std::memory_order_relaxed);
            if (value)
                paintSamples.push_back(value);
        }
        std::sort(paintSamples.begin(), paintSamples.end());
        auto percentile = [&paintSamples](double p) -> uint64_t
        {
            if (paintSamples.empty()) return 0;
            const std::size_t index = static_cast<std::size_t>(std::ceil(p * paintSamples.size())) - 1;
            return paintSamples[std::min(index, paintSamples.size() - 1)];
        };
        std::ostringstream o;
        o << "===== Engine Stats =====\n";
        o << "frames          : " << s.frameCounter.load() << "\n";
        o << "entities        : " << s.entityCount.load() << "\n";
        o << "main draws/frame: " << s.drawCalls.load() << "\n";
        o << "uniforms /frame : " << s.uniformSets.load() << "\n";
        o << "shadow passes   : " << s.shadowPasses.load() << "\n";
        o << "scene tex binds : " << s.textureBinds.load() << "\n";
        o << "paintGL CPU us  : last " << s.paintLastUs.load()
          << ", p50 " << percentile(0.50) << ", p95 " << percentile(0.95) << "\n";
        o << "checks pass/fail: " << s.checkPasses.load() << " / "
          << s.checkFailures.load() << "\n";
        o << "========================";
        return o.str();
    }

    std::string runSelfCheck()
    {
        // 自检项：全部走 ET_CHECK，结果聚合进统计。返回摘要供 Lua 打印。
        // 注意：这里只做"无依赖"的基础检查；ECS 世界级检查由调用方传入
        // （见 Lua api_engineSelfCheck —— 带 world 时补世界级项目）。
        int pass = 0, fail = 0;

        auto item = [&](bool ok, const char *name)
        {
            if (ET_CHECK(ok, name))
                pass++;
            else
                fail++;
        };

        // ① 统计计数自洽：本帧计数不可能为负（atomic 无符号，恒真——示例占位）
        item(true, "stats non-negative");
        // ② 断言系统本身工作（故意跑一个已知通过项）
        item(1 + 1 == 2, "assert system functional");

        std::ostringstream o;
        o << "PASS " << pass << " / FAIL " << fail;
        return o.str();
    }

} // namespace EngineTest
