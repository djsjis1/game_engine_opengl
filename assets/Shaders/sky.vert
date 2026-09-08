#version 330 core

// 天空盒顶点着色器
// 关键技巧：去掉 view 矩阵的平移分量（只保留旋转），让天空盒
// 永远跟随相机（相机走到哪天空都在无限远处），且把 z 强制设为 w
// 使深度值恒为 1.0（最远），天空只填充没有物体的像素。

layout(location=0)in vec3 aPos;

uniform mat4 uView;// 已去平移的视图矩阵（CPU 侧处理）
uniform mat4 uProjection;

out vec3 vDir;// 传给片元：这个顶点在世界空间的方向（用于采样天空色）

void main()
{
    vDir=aPos;// 立方体本地坐标即方向向量（中心在原点）
    vec4 pos=uProjection*uView*vec4(aPos,1.);
    gl_Position=pos.xyww;// z=w → 透视除法后深度恒为 1.0（最远处）
}
