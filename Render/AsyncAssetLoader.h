#ifndef ASYNC_ASSET_LOADER_H
#define ASYNC_ASSET_LOADER_H

#include "Core/JobSystem.h"
#include "Render/MeshFactory.h"
#include <QImage>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <queue>
#include <string>
#include <vector>

// CPU-only asset preparation. Workers never touch Flecs, ResourceManager or GL;
// the render thread validates each result and performs the GPU upload.
class AsyncAssetLoader
{
public:
    enum class Kind { Image, Mesh };
    struct Result
    {
        Kind kind = Kind::Image;
        uint32_t handle = 0;
        uint64_t generation = 0;
        size_t frame = 0;
        std::string path;
        bool success = false;
        QImage image;
        std::vector<Vertex> vertices;
        glm::vec3 boundsMin{-0.5f};
        glm::vec3 boundsMax{0.5f};
    };

    ~AsyncAssetLoader() { invalidate(); }
    void enqueueImage(uint32_t handle, uint64_t generation, size_t frame,
                      const std::string &path);
    void enqueueMesh(uint32_t handle, uint64_t generation, const std::string &path);
    bool tryTake(Result &result);
    void invalidate();

private:
    void publish(Result &&result, uint64_t epoch);

    static constexpr size_t kMaxDecodedResults = 8;
    std::mutex m_mutex;
    std::condition_variable m_spaceAvailable;
    std::queue<Result> m_results;
    std::atomic<uint64_t> m_epoch{1};
    // Declared last: its destructor joins workers before the queue/mutex die.
    JobSystem m_jobs{2};
};

#endif
