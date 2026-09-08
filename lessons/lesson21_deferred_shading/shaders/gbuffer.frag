#version 330 core
// 【知识点】MRT: 一次渲染输出3个纹理（G-Buffer）
layout(location = 0) out vec3 gPosition;   // 世界坐标
layout(location = 1) out vec3 gNormal;     // 法线
layout(location = 2) out vec4 gAlbedoSpec; // RGB=颜色 A=镜面强度
in vec3 FragPos;
in vec3 Normal;
in float Metallic;
uniform vec3 uColor;
void main()
{
    // 终极诊断：硬编码红色，忽略所有输入
    gPosition = vec3(1.0, 0.0, 0.0);
    gNormal = vec3(0.0, 0.0, 1.0);
    gAlbedoSpec = vec4(1.0, 0.0, 0.0, 1.0);
}
