#version 330 core

// ============================================================
// default.vert — 默认 Phong 光照顶点着色器
//
// 输入：位置(0) + 法线(1) + 纹理坐标(2)
// 输出：世界空间位置、法线、纹理坐标到片段着色器
// ============================================================

layout(location=0)in vec3 aPos;
layout(location=1)in vec3 aNormal;
layout(location=2)in vec2 aTexCoord;

// MVP 矩阵
uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform mat3 uNormalMatrix;// 法线变换矩阵（model 的逆转置）

// 输出到片段着色器（光栅化阶段自动插值）
out vec3 vFragPos;// 世界空间片段位置
out vec3 vNormal;// 世界空间法线
out vec2 vTexCoord;// 纹理坐标

void main()
{
    // 世界空间位置
    vec4 worldPos=uModel*vec4(aPos,1.);
    vFragPos=worldPos.xyz;
    
    // 世界空间法线（使用法线矩阵处理非均匀缩放）
    vNormal=normalize(uNormalMatrix*aNormal);
    
    // 纹理坐标直传
    vTexCoord=aTexCoord;
    
    // 裁剪空间位置（MVP 变换）
    gl_Position=uProjection*uView*worldPos;
}
