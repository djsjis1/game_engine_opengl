#version 330 core
// ============================================================
// Lesson 03 顶点着色器
// 新知识点：
//   out变量 - 从顶点着色器向片段着色器传递数据
//   layout(location=1) - 多个顶点属性（位置+颜色）
// ============================================================

layout(location = 0) in vec3 aPos;      // 位置属性
layout(location = 1) in vec3 aColor;    // 颜色属性（新增！）

out vec3 vertexColor;  // 传递给片段着色器的颜色（新增！）

void main()
{
    gl_Position = vec4(aPos, 1.0);
    vertexColor = aColor;  // 将颜色原样传递给片段着色器
}
