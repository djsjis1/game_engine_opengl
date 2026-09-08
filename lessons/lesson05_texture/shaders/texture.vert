#version 330 core
// Lesson 05 顶点着色器 - 纹理坐标
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aTexCoord;  // 纹理坐标（2D）

out vec2 TexCoord;  // 传递给片段着色器

void main()
{
    gl_Position = vec4(aPos, 1.0);
    TexCoord = aTexCoord;
}
