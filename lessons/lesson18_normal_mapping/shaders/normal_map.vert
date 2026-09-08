#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;
layout(location = 3) in vec3 aTangent;  // 切线（用于 TBN 矩阵）

out VS_OUT {
    vec3 FragPos;
    vec2 TexCoord;
    mat3 TBN; // 切线空间 → 世界空间矩阵
} vs_out;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;

void main()
{
    vs_out.FragPos = vec3(uModel * vec4(aPos, 1.0));
    vs_out.TexCoord = aTexCoord;

    // 构建 TBN 矩阵
    vec3 T = normalize(mat3(uModel) * aTangent);
    vec3 N = normalize(mat3(uModel) * aNormal);
    T = normalize(T - dot(T, N) * N); // 施密特正交化
    vec3 B = cross(N, T);
    vs_out.TBN = mat3(T, B, N);

    gl_Position = uProjection * uView * uModel * vec4(aPos, 1.0);
}
