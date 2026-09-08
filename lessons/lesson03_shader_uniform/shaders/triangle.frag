#version 330 core
// ============================================================
// Lesson 03 片段着色器
// 新知识点：
//   in变量  - 从前一阶段(顶点着色器)接收插值后的数据
//   uniform - 全局变量，CPU→GPU传递，所有顶点/片段共享同一值
//
// 【重要概念】插值(Interpolation):
//   三角形三个顶点分别是红、绿、蓝
//   在三角形内部的片段颜色会自动在两两顶点之间进行插值
//   产生平滑的渐变色效果！
// ============================================================

in vec3 vertexColor;      // 接收插值后的顶点颜色
uniform vec3 uColor;      // uniform变量：统一颜色（外部可修改）
uniform float uMixRatio;  // 混合比例：控制vertexColor和uColor的混合

out vec4 FragColor;

void main()
{
    // 使用mix函数混合顶点颜色和uniform颜色
    // mix(x, y, a) = x*(1-a) + y*a
    // uMixRatio=0 → 纯顶点渐变色
    // uMixRatio=1 → 纯uniform颜色
    vec3 finalColor = mix(vertexColor, uColor, uMixRatio);
    FragColor = vec4(finalColor, 1.0);
}
