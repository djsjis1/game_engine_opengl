#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in mat4 uInstanceModel; // 每个实例的模型矩阵

out vec3 FragPos;
out vec3 Normal;
out vec3 InstanceColor;

uniform mat4 uView;
uniform mat4 uProjection;

void main()
{
    mat4 mvp = uProjection * uView * uInstanceModel;
    FragPos = vec3(uInstanceModel * vec4(aPos, 1.0));
    Normal = mat3(uInstanceModel) * aNormal;

    // 从模型矩阵里提取颜色（用第1行第1,2,3列，因为每个实例已经预处理了颜色值）
    InstanceColor = vec3(uInstanceModel[1][0], uInstanceModel[1][1], uInstanceModel[1][2]);
    gl_Position = mvp * vec4(aPos, 1.0);
}
