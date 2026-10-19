#include "Render/AsyncAssetLoader.h"
#include "Render/ObjLoader.h"
#include <QString>
#include <exception>

void AsyncAssetLoader::enqueueImage(uint32_t handle, uint64_t generation,
                                    size_t frame, const std::string &path)
{
    const uint64_t epoch = m_epoch.load(std::memory_order_acquire);
    m_jobs.submit([this, handle, generation, frame, path, epoch]
    {
        if (epoch != m_epoch.load(std::memory_order_acquire)) return;
        Result result;
        result.kind = Kind::Image;
        result.handle = handle;
        result.generation = generation;
        result.frame = frame;
        result.path = path;
        try
        {
            QImage image(QString::fromUtf8(path.c_str()));
            if (!image.isNull())
            {
                result.image = image.convertToFormat(QImage::Format_RGBA8888).mirrored();
                result.success = !result.image.isNull();
            }
        }
        catch (const std::exception &)
        {
            result.success = false;
        }
        publish(std::move(result), epoch);
    });
}

void AsyncAssetLoader::enqueueMesh(uint32_t handle, uint64_t generation,
                                   const std::string &path)
{
    const uint64_t epoch = m_epoch.load(std::memory_order_acquire);
    m_jobs.submit([this, handle, generation, path, epoch]
    {
        if (epoch != m_epoch.load(std::memory_order_acquire)) return;
        Result result;
        result.kind = Kind::Mesh;
        result.handle = handle;
        result.generation = generation;
        result.path = path;
        try
        {
            result.success = ObjLoader::loadFile(path, result.vertices);
            if (result.success && !result.vertices.empty())
            {
                const Vertex &first = result.vertices.front();
                result.boundsMin = result.boundsMax =
                    glm::vec3(first.px, first.py, first.pz);
                for (const Vertex &vertex : result.vertices)
                {
                    const glm::vec3 point(vertex.px, vertex.py, vertex.pz);
                    result.boundsMin = glm::min(result.boundsMin, point);
                    result.boundsMax = glm::max(result.boundsMax, point);
                }
            }
        }
        catch (const std::exception &)
        {
            result.success = false;
            result.vertices.clear();
        }
        publish(std::move(result), epoch);
    });
}

bool AsyncAssetLoader::tryTake(Result &result)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_results.empty()) return false;
    result = std::move(m_results.front());
    m_results.pop();
    m_spaceAvailable.notify_one();
    return true;
}

void AsyncAssetLoader::invalidate()
{
    m_epoch.fetch_add(1, std::memory_order_acq_rel);
    std::lock_guard<std::mutex> lock(m_mutex);
    std::queue<Result> empty;
    m_results.swap(empty);
    m_spaceAvailable.notify_all();
}

void AsyncAssetLoader::publish(Result &&result, uint64_t epoch)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    m_spaceAvailable.wait(lock, [this, epoch]
    {
        return epoch != m_epoch.load(std::memory_order_acquire) ||
               m_results.size() < kMaxDecodedResults;
    });
    if (epoch == m_epoch.load(std::memory_order_acquire))
        m_results.push(std::move(result));
}
