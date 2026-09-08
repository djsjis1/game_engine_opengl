#version 330 core
// Lesson 06 顶点着色器 - 变换矩阵
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aTexCoord;

out vec2 TexCoord;

uniform mat4 uModel;       // 模型矩阵（new!）

void main()
{
    // 顶点位置左乘模型矩阵，完成变换
    gl_Position = uModel * vec4(aPos, 1.0);
    TexCoord = aTexCoord;
}
