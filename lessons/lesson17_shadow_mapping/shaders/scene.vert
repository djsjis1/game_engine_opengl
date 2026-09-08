#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;

out vec3 FragPos;
out vec3 Normal;
out vec4 FragPosLightSpace; // 光源空间坐标（用于阴影计算）

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform mat4 uLightSpace;

void main()
{
    FragPos = vec3(uModel * vec4(aPos, 1.0));
    Normal = mat3(uModel) * aNormal;
    FragPosLightSpace = uLightSpace * vec4(FragPos, 1.0);
    gl_Position = uProjection * uView * vec4(FragPos, 1.0);
}
