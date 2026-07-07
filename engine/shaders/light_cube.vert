#version 330 core

// ============================================================
// light_cube.vert — 光源可视化顶点着色器
//
// 用于渲染表示光源位置的小立方体，始终为纯色。
// ============================================================

layout(location=0)in vec3 aPos;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

void main()
{
    gl_Position=uProjection*uView*uModel*vec4(aPos,1.);
}
