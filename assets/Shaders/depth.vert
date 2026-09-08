#version 330 core

// 阴影 Pass 1：从光源视角渲染深度
// 只需把顶点变换到"光空间"，深度自动写入深度缓冲，无需片元逻辑。

layout(location=0)in vec3 aPos;

uniform mat4 uLightSpaceMatrix;// 光源的 proj * view
uniform mat4 uModel;

void main()
{
    gl_Position=uLightSpaceMatrix*uModel*vec4(aPos,1.);
}
