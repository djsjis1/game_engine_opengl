#ifndef RAYCAST_H
#define RAYCAST_H

#include <glm/glm.hpp>

// ============================================================
// Raycast — 射线拾取数学（纯函数，零 Qt/GL 依赖，可独立单元测试）
//
// 【核心思想：渲染管线倒着走】
//   渲染（正向）：世界坐标 → view → proj → NDC → 屏幕像素
//   拾取（逆向）：屏幕像素 → NDC → inverse(proj*view) → 世界射线
//
// 【为什么是"射线"不是"点"】
//   屏幕上一个像素对应 3D 里一条线（近平面点到远平面点的连线）——
//   深度信息在投影时丢了，拾取就是问"这条线先打中谁"。
//
// 【OBB 拾取技巧】旋转/缩放过的立方体不用世界空间算斜包围盒：
//   把射线用 inverse(worldMatrix) 变换到实体局部空间，
//   在局部空间与"单位 AABB（±0.5）"求交 —— 变换归变换，求交归求交。
// ============================================================

struct Ray
{
    glm::vec3 origin;
    glm::vec3 dir; // 单位向量
};

/// 屏幕像素坐标 → 世界空间射线
/// mouseX/mouseY：像素坐标（左上原点，Qt 惯例）
/// width/height：视口尺寸
/// invViewProj：inverse(projection * view)
inline Ray screenToRay(float mouseX, float mouseY, float width, float height,
                       const glm::mat4 &invViewProj)
{
    // ① 像素 → NDC（-1..1；注意 Y 翻转：屏幕 Y 向下，NDC Y 向上）
    float ndcX = (2.0f * mouseX) / width - 1.0f;
    float ndcY = 1.0f - (2.0f * mouseY) / height;

    // ② NDC 近/远平面两点 → 世界空间（透视除法：除以 w！）
    glm::vec4 nearPt = invViewProj * glm::vec4(ndcX, ndcY, -1.0f, 1.0f);
    glm::vec4 farPt = invViewProj * glm::vec4(ndcX, ndcY, 1.0f, 1.0f);
    nearPt /= nearPt.w;
    farPt /= farPt.w;

    // ③ 两点连线 = 射线
    Ray r;
    r.origin = glm::vec3(nearPt);
    r.dir = glm::normalize(glm::vec3(farPt - nearPt));
    return r;
}

/// 射线 vs AABB 求交（slab 法：三对平行平面分别裁剪射线参数区间）
/// 命中返回 true，tOut = 命中距离（射线参数，>=0）
/// 起点在盒内也算命中（tOut=0）
inline bool rayIntersectAABB(const Ray &ray,
                             const glm::vec3 &boxMin, const glm::vec3 &boxMax,
                             float &tOut)
{
    float tMin = 0.0f;  // 进入时间（三轴取最大）
    float tMax = 1e30f; // 离开时间（三轴取最小）

    for (int i = 0; i < 3; ++i)
    {
        if (glm::abs(ray.dir[i]) < 1e-8f)
        {
            // 射线平行于该轴的两平面：起点不在夹层内 → 永不相交
            if (ray.origin[i] < boxMin[i] || ray.origin[i] > boxMax[i])
                return false;
        }
        else
        {
            float inv = 1.0f / ray.dir[i];
            float t1 = (boxMin[i] - ray.origin[i]) * inv;
            float t2 = (boxMax[i] - ray.origin[i]) * inv;
            if (t1 > t2)
            {
                float tmp = t1;
                t1 = t2;
                t2 = tmp;
            }
            tMin = glm::max(tMin, t1);
            tMax = glm::min(tMax, t2);
            if (tMin > tMax)
                return false; // 区间空了：不相交
        }
    }
    tOut = tMin;
    return true;
}

/// 射线 vs 变换后的单位立方体（边长1，中心原点 —— 即引擎的 cube 网格）
/// worldMatrix：实体的世界矩阵（含平移/旋转/缩放/父级链）
/// 命中返回 true，distOut = 世界空间中命中点到射线起点的距离
inline bool rayIntersectUnitCube(const Ray &worldRay,
                                 const glm::mat4 &worldMatrix,
                                 float &distOut)
{
    // 射线变换到实体局部空间（方向不 normalize：保持参数 t 的映射关系）
    glm::mat4 inv = glm::inverse(worldMatrix);
    Ray local;
    local.origin = glm::vec3(inv * glm::vec4(worldRay.origin, 1.0f));
    local.dir = glm::vec3(inv * glm::vec4(worldRay.dir, 0.0f));

    // 归一化局部方向做求交（slab 法要求方向有意义即可），
    // 记住缩放比例以便把局部 t 换算回世界距离
    float dirLen = glm::length(local.dir);
    if (dirLen < 1e-8f)
        return false;
    local.dir /= dirLen;

    float tLocal = 0;
    if (!rayIntersectAABB(local, glm::vec3(-0.5f), glm::vec3(0.5f), tLocal))
        return false;

    // 局部命中点 → 世界空间 → 世界距离（非均匀缩放下 t 不能直接比，距离才公平）
    glm::vec3 localHit = local.origin + local.dir * tLocal;
    glm::vec3 worldHit = glm::vec3(worldMatrix * glm::vec4(localHit, 1.0f));
    distOut = glm::length(worldHit - worldRay.origin);
    return true;
}

#endif
