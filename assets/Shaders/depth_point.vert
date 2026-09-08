#version 330 core
// ============================================================
// depth_point.vert — 点光阴影 Pass：顶点变换 + 世界坐标传片元
// 每帧对 cubemap 的 6 个面各渲一遍（uLightSpaceMatrix = 该面的 proj*view）
// ============================================================
layout(location=0)in vec3 aPos;

uniform mat4 uLightSpaceMatrix;// 当前面的 proj * view
uniform mat4 uModel;

out vec3 vWorldPos;// 片元世界坐标（算到光源的线性距离用）

void main()
{
    vec4 world=uModel*vec4(aPos,1.);
    vWorldPos=world.xyz;
    gl_Position=uLightSpaceMatrix*world;
}
