#include <gtest/gtest.h>
#include "Render/AsyncAssetLoader.h"
#include <QTemporaryDir>
#include <QImage>
#include <QColor>
#include <QString>
#include <chrono>
#include <fstream>
#include <thread>

namespace
{
    bool waitFor(AsyncAssetLoader &loader, AsyncAssetLoader::Result &result)
    {
        for (int i = 0; i < 400; ++i)
        {
            if (loader.tryTake(result)) return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        return false;
    }
}

TEST(AsyncAssetLoaderTest, ImageDecode_FlipsPixelsAndPreservesGeneration)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath("frame.bmp");
    QImage source(2, 2, QImage::Format_RGBA8888);
    source.fill(Qt::black);
    source.setPixelColor(0, 0, Qt::red);
    source.setPixelColor(0, 1, Qt::blue);
    ASSERT_TRUE(source.save(path));

    AsyncAssetLoader loader;
    loader.enqueueImage(7, 42, 3, path.toStdString());
    AsyncAssetLoader::Result result;
    ASSERT_TRUE(waitFor(loader, result));
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.kind, AsyncAssetLoader::Kind::Image);
    EXPECT_EQ(result.handle, 7u);
    EXPECT_EQ(result.generation, 42u);
    EXPECT_EQ(result.frame, 3u);
    EXPECT_EQ(result.image.format(), QImage::Format_RGBA8888);
    EXPECT_EQ(result.image.pixelColor(0, 0), QColor(Qt::blue));
}

TEST(AsyncAssetLoaderTest, MeshParse_RunsOffThreadAndReturnsVertices)
{
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath("triangle.obj");
    {
        std::ofstream file(path.toStdString());
        file << "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n";
    }
    AsyncAssetLoader loader;
    loader.enqueueMesh(9, 64, path.toStdString());
    AsyncAssetLoader::Result result;
    ASSERT_TRUE(waitFor(loader, result));
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.kind, AsyncAssetLoader::Kind::Mesh);
    EXPECT_EQ(result.handle, 9u);
    EXPECT_EQ(result.generation, 64u);
    EXPECT_EQ(result.vertices.size(), 3u);
    EXPECT_FLOAT_EQ(result.boundsMin.x, 0.0f);
    EXPECT_FLOAT_EQ(result.boundsMin.y, 0.0f);
    EXPECT_FLOAT_EQ(result.boundsMax.x, 1.0f);
    EXPECT_FLOAT_EQ(result.boundsMax.y, 1.0f);
}

TEST(AsyncAssetLoaderTest, Invalidate_DiscardsCompletedAndInFlightWork)
{
    AsyncAssetLoader loader;
    loader.enqueueImage(1, 1, 0, "missing-image.bmp");
    loader.invalidate();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    AsyncAssetLoader::Result result;
    EXPECT_FALSE(loader.tryTake(result));
}
