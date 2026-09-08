#ifndef MESHCOMPONENT_H
#define MESHCOMPONENT_H

#include <cstdint>

// ============================================================
// MeshComponent — 网格组件（纯数据，句柄设计）
//
// 不再直接持有 VAO/VBO 等 GPU 资源！只存：
//   meshHandle : ResourceManager 的句柄（运行时，多实体共享同一份 GPU 数据）
//   meshType   : 资源名（序列化用——句柄不能存盘，名字可以）
//
// 获取真正的 GPU 数据：
//   const GpuMesh *gm = ResourceManager::instance().get(meshHandle);
// ============================================================
struct MeshComponent
{
    uint32_t meshHandle = 0;     // 0 = 未加载
    char meshType[128] = "cube"; // 资源名："cube" 或模型相对路径 "models/tree.obj"

    bool isValid() const { return meshHandle != 0; }
};

#endif
