#version 330 core

// 阴影 Pass 1 片元着色器：空的。
// 深度值由 GPU 自动写入深度缓冲，我们不需要颜色输出。
// （某些驱动要求 FBO 有 fragment shader，故保留空 main）

void main()
{
}
