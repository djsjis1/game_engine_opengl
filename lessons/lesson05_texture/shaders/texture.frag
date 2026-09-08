#version 330 core
// Lesson 05 片段着色器 - 纹理采样
in vec2 TexCoord;                       // 纹理坐标
out vec4 FragColor;

uniform sampler2D uTexture1;            // 纹理采样器1
uniform sampler2D uTexture2;            // 纹理采样器2
uniform float     uMixRatio;            // 两张纹理的混合比例

void main()
{
    // texture() 是GLSL内置函数，根据纹理坐标采样纹理颜色
    vec4 color1 = texture(uTexture1, TexCoord);
    vec4 color2 = texture(uTexture2, TexCoord);

    // 混合两张纹理
    FragColor = mix(color1, color2, uMixRatio);
}
