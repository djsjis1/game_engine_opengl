#version 330 core
// 【知识点】几何着色器：为三角形的每个顶点画一条法线
layout(triangles) in;
layout(line_strip, max_vertices = 6) out;
out vec3 gColor;
in vec3 vPos[];
in vec3 vNormal[];
uniform mat4 uView;
uniform mat4 uProjection;
uniform float uNormalLen;
void main()
{
    for (int i = 0; i < 3; ++i)
    {
        // 顶点位置 → 沿法线方向延伸
        gl_Position = gl_in[i].gl_Position;
        gColor = vec3(1.0, 0.0, 0.0); // 红色：起点
        EmitVertex();

        gl_Position = uProjection * uView * vec4(vPos[i] + vNormal[i] * uNormalLen, 1.0);
        gColor = vec3(1.0, 1.0, 0.0); // 黄色：法线末端
        EmitVertex();

        EndPrimitive();
    }
}
