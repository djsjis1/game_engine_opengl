#ifndef JOBSYSTEM_H
#define JOBSYSTEM_H

#include <vector>
#include <thread>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <queue>

// ============================================================
// JobSystem — 游戏引擎工作线程池
//
// 主线程提交任务 → 工作线程执行 → 结果通过信号队列返回
// ============================================================
class JobSystem
{
public:
    explicit JobSystem(int numThreads = 4)
    {
        for (int i = 0; i < numThreads; ++i)
        {
            m_threads.emplace_back([this]
                                   {
                while (true)
                {
                    std::function<void()> job;
                    {
                        std::unique_lock<std::mutex> lock(m_mutex);
                        m_cv.wait(lock, [this] { return !m_running || !m_jobs.empty(); });
                        if (!m_running && m_jobs.empty()) return;
                        job = std::move(m_jobs.front());
                        m_jobs.pop();
                    }
                    if (job) job();
                } });
        }
    }

    ~JobSystem()
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_running = false;
        }
        m_cv.notify_all();
        for (auto &t : m_threads)
            if (t.joinable())
                t.join();
    }

    void submit(std::function<void()> job)
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_jobs.push(std::move(job));
        }
        m_cv.notify_one();
    }

    size_t pending() const { return m_jobs.size(); }

private:
    std::vector<std::thread> m_threads;
    std::queue<std::function<void()>> m_jobs;
    mutable std::mutex m_mutex;
    std::condition_variable m_cv;
    std::atomic<bool> m_running{true};
};

#endif
