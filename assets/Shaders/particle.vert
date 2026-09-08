#version 330 core
// ============================================================
// particle.vert — 粒子 billboard 实例化顶点着色器
//
// 每实例 = 一个粒子：单位 quad（aCorner ∈ [-0.5,0.5]²）用相机
// right/up 轴展开成永远面向相机的方块（billboard）。
// 实例属性由 CPU 每帧上传（位置/大小/颜色/透明度）。
// ============================================================
layout(location=0)in vec2 aCorner;// 单位 quad 角点（共享，4 顶点）
layout(location=1)in vec3 iPos;// 实例：粒子世界位置
layout(location=2)in float iSize;// 实例：边长
layout(location=3)in vec4 iColor;// 实例：rgb=颜色(已乘强度) a=透明度

uniform mat4 uView;
uniform mat4 uProjection;
uniform vec3 uCamRight;// 相机右轴（view 矩阵转置取行）
uniform vec3 uCamUp;// 相机上轴

out vec4 vColor;
out vec2 vUv;

void main()
{
    // billboard：quad 在相机平面内展开（永远正对镜头）
    vec3 world=iPos+(uCamRight*aCorner.x+uCamUp*aCorner.y)*iSize;
    gl_Position=uProjection*uView*vec4(world,1.);
    vColor=iColor;
    vUv=aCorner+.5;// [0,1]²
}
