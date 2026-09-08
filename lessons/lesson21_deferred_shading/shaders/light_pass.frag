#version 330 core
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D gPosition;

void main()
{
    // 输出G-Buffer里的世界坐标，看看几何阶段有没有写进去
    vec3 pos = texture(gPosition, TexCoord).rgb;
    FragColor = vec4(pos, 1.0);
}

