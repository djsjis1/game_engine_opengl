// ============================================================
// ResourceManager 集成测试 — 句柄/引用计数/缓存逻辑
//
// 覆盖：
//   acquireMesh 句柄分配与缓存命中
//   releaseMesh 引用计数递减与归零清理
//   addRef 手动增引用
//   无效句柄安全处理
//   重复释放防护
//   空槽复用
//
// 注意：只测试纯记账逻辑（acquire/release/addRef），
//   不调用 get()（需要 GL 上下文）。
//   ResourceManager 是单例，每个测试用唯一名字避免互相干扰。
// ============================================================
#include <gtest/gtest.h>
#include <string>
#include "Render/ResourceManager.h"

class ResourceManagerTest : public ::testing::Test
{
protected:
    /// 生成唯一资源名（避免测试间干扰——单例无 reset）
    std::string uniqueName(const char *base)
    {
        static int counter = 0;
        return std::string(base) + "_" + std::to_string(++counter);
    }
};

// ---- acquireMesh 基本逻辑 ----

TEST_F(ResourceManagerTest, AcquireMesh_ReturnsValidHandle)
{
    std::string name = uniqueName("cube");
    uint32_t handle = ResourceManager::instance().acquireMesh(name);
    EXPECT_GT(handle, 0u);
    // 清理
    ResourceManager::instance().releaseMesh(handle);
}

TEST_F(ResourceManagerTest, AcquireMesh_SameName_SameHandle)
{
    std::string name = uniqueName("sphere");
    uint32_t h1 = ResourceManager::instance().acquireMesh(name);
    uint32_t h2 = ResourceManager::instance().acquireMesh(name);
    EXPECT_EQ(h1, h2); // 缓存命中，返回同一句柄
    // 清理（释放两次引用）
    ResourceManager::instance().releaseMesh(h1);
    ResourceManager::instance().releaseMesh(h2);
}

TEST_F(ResourceManagerTest, AcquireMesh_DiffNames_DiffHandles)
{
    std::string name1 = uniqueName("obj_a");
    std::string name2 = uniqueName("obj_b");
    uint32_t h1 = ResourceManager::instance().acquireMesh(name1);
    uint32_t h2 = ResourceManager::instance().acquireMesh(name2);
    EXPECT_NE(h1, h2);
    ResourceManager::instance().releaseMesh(h1);
    ResourceManager::instance().releaseMesh(h2);
}

// ---- 引用计数 ----

TEST_F(ResourceManagerTest, ReleaseMesh_RefCountDecrement_SharedResource)
{
    std::string name = uniqueName("shared");
    // 三个实体共享同一资源
    uint32_t h1 = ResourceManager::instance().acquireMesh(name);
    uint32_t h2 = ResourceManager::instance().acquireMesh(name);
    uint32_t h3 = ResourceManager::instance().acquireMesh(name);
    EXPECT_EQ(h1, h2);
    EXPECT_EQ(h2, h3);

    // 释放两次后仍有引用（第三次 acquire 的还在）
    ResourceManager::instance().releaseMesh(h1);
    ResourceManager::instance().releaseMesh(h2);

    // 资源仍在缓存中（再次 acquire 同名应返回同一句柄）
    uint32_t h4 = ResourceManager::instance().acquireMesh(name);
    EXPECT_EQ(h4, h1);

    // 全部释放
    ResourceManager::instance().releaseMesh(h3);
    ResourceManager::instance().releaseMesh(h4);
}

TEST_F(ResourceManagerTest, ReleaseMesh_ToZero_RemovesFromCache)
{
    std::string name = uniqueName("temp");
    uint32_t h1 = ResourceManager::instance().acquireMesh(name);
    ResourceManager::instance().releaseMesh(h1); // refCount → 0

    // 归零后再 acquire 同名 → 应分配新句柄（旧条目已清除）
    uint32_t h2 = ResourceManager::instance().acquireMesh(name);
    EXPECT_GT(h2, 0u);
    ResourceManager::instance().releaseMesh(h2);
}

// ---- addRef ----

TEST_F(ResourceManagerTest, AddRef_IncrementsRefCount)
{
    std::string name = uniqueName("ref_test");
    uint32_t handle = ResourceManager::instance().acquireMesh(name); // ref=1
    ResourceManager::instance().addRef(handle);                      // ref=2

    // 释放一次后仍有效（ref=1）
    ResourceManager::instance().releaseMesh(handle);
    uint32_t again = ResourceManager::instance().acquireMesh(name);
    EXPECT_EQ(again, handle); // 还在缓存

    // 再释放（ref→0）
    ResourceManager::instance().releaseMesh(handle);
    ResourceManager::instance().releaseMesh(again);
}

// ---- 无效句柄安全 ----

TEST_F(ResourceManagerTest, ReleaseMesh_InvalidHandle_NoCrash)
{
    // 不应崩溃
    ResourceManager::instance().releaseMesh(0);
    ResourceManager::instance().releaseMesh(99999);
}

TEST_F(ResourceManagerTest, AddRef_InvalidHandle_NoCrash)
{
    ResourceManager::instance().addRef(0);
    ResourceManager::instance().addRef(99999);
}

// ---- 重复释放防护 ----

TEST_F(ResourceManagerTest, DoubleRelease_Safe)
{
    std::string name = uniqueName("double");
    uint32_t handle = ResourceManager::instance().acquireMesh(name); // ref=1
    ResourceManager::instance().releaseMesh(handle);                 // ref→0
    ResourceManager::instance().releaseMesh(handle);                 // 已归零，安全返回
    // 不应崩溃，不应产生负引用计数
}

// ---- 句柄复用（空槽） ----

TEST_F(ResourceManagerTest, EmptySlot_Reuse)
{
    // 创建并释放一个资源 → 产生空槽
    std::string name1 = uniqueName("slot_old");
    uint32_t h1 = ResourceManager::instance().acquireMesh(name1);
    ResourceManager::instance().releaseMesh(h1); // 空出槽位

    // 新资源可能复用该空槽（句柄值可能等于 h1）
    // 只验证新句柄有效即可（具体复用哪个槽是实现细节）
    std::string name2 = uniqueName("slot_new");
    uint32_t h2 = ResourceManager::instance().acquireMesh(name2);
    EXPECT_GT(h2, 0u);
    ResourceManager::instance().releaseMesh(h2);
}

// ---- 大量 acquire/release 压力 ----

TEST_F(ResourceManagerTest, ManyAcquireRelease_NoLeak)
{
    // 模拟频繁创建/销毁（场景切换等）
    for (int i = 0; i < 50; ++i)
    {
        std::string name = uniqueName("stress");
        uint32_t h = ResourceManager::instance().acquireMesh(name);
        EXPECT_GT(h, 0u);
        ResourceManager::instance().releaseMesh(h);
    }
    // 不崩溃即通过（引用计数平衡）
}
