#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
out vec3 FragPos;
out vec3 Normal;
out float Metallic; // 金属度（用于 specular）
uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform float uMetallic;
void main()
{
    FragPos = vec3(uModel * vec4(aPos, 1.0));
    Normal = normalize(mat3(uModel) * aNormal);
    Metallic = uMetallic;
    gl_Position = uProjection * uView * vec4(FragPos, 1.0);
}
