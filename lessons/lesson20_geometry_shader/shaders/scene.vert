#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
out vec3 vPos;
out vec3 vNormal;
uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
void main()
{
    vPos = vec3(uModel * vec4(aPos, 1.0));
    vNormal = normalize(mat3(uModel) * aNormal);
    gl_Position = uProjection * uView * vec4(vPos, 1.0);
}
