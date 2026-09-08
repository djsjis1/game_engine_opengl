#version 330 core

// 后处理顶点着色器：全屏四边形（不需要 VBO，用 gl_VertexID 生成）
// 覆盖整个屏幕的两个三角形，把场景纹理贴上去做全屏加工。

out vec2 vUV;

void main()
{
    // 用顶点 ID 生成全屏三角形的 UV 和位置（经典无 VBO 技巧）
    vUV = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    gl_Position = vec4(vUV * 2.0 - 1.0, 0.0, 1.0);
}
