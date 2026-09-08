#version 330 core
// Lesson 07 顶点着色器 - MVP矩阵变换
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aTexCoord;

out vec2 TexCoord;

uniform mat4 uModel;       // 模型矩阵
uniform mat4 uView;        // 视图矩阵（摄像机）
uniform mat4 uProjection;  // 投影矩阵（透视/正交）

void main()
{
    // 【知识点】MVP变换
    // 裁剪空间位置 = Projection * View * Model * 局部坐标
    // 先Model，再View，最后Projection（从右到左读）
    gl_Position = uProjection * uView * uModel * vec4(aPos, 1.0);
    TexCoord = aTexCoord;
}
