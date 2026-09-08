#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
out vec3 FragPos;
out vec3 Normal;
uniform mat4 uModel, uView, uProjection;
void main()
{
    Normal = mat3(uModel) * aNormal;
    FragPos = vec3(uModel * vec4(aPos, 1.0));
    gl_Position = uProjection * uView * vec4(FragPos, 1.0);
}
