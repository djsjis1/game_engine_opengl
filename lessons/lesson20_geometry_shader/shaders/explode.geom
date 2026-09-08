#version 330 core
// 【知识点】几何着色器：输入三角形，沿面法线炸开
layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

in vec3 vPos[];
in vec3 vNormal[];
out vec3 gNormal;
out vec3 gPos;

uniform float uTime;
uniform float uExplodeFactor;
uniform bool uExplode;

void main()
{
    // 计算三角形面法线
    vec3 edge1 = vPos[1] - vPos[0];
    vec3 edge2 = vPos[2] - vPos[0];
    vec3 faceNormal = normalize(cross(edge1, edge2));

    float displace = 0.0;
    if (uExplode)
    {
        // 随时间振荡的炸裂偏移
        displace = uExplodeFactor * (sin(uTime * 2.0 + vPos[0].x * 2.0) * 0.5 + 0.5);
    }

    for (int i = 0; i < 3; ++i)
    {
        gPos = vPos[i] + faceNormal * displace;
        gNormal = faceNormal;
        gl_Position = gl_in[i].gl_Position + vec4(faceNormal * displace, 0.0);
        EmitVertex();
    }
    EndPrimitive();
}
