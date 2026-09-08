#version 330 core
in vec2 TexCoord;
out vec4 FragColor;
uniform sampler2D uImage;
uniform bool uHorizontal;  // true=水平模糊, false=垂直模糊
void main()
{
    // 5-tap 高斯核权重：[1,4,6,4,1]/16
    float weight[5] = float[](0.0625, 0.25, 0.375, 0.25, 0.0625);
    vec2 texelSize = 1.0 / textureSize(uImage, 0);
    vec3 result = vec3(0.0);
    for (int i = -2; i <= 2; ++i)
    {
        vec2 offset = uHorizontal ? vec2(i * texelSize.x, 0) : vec2(0, i * texelSize.y);
        result += texture(uImage, TexCoord + offset).rgb * weight[i + 2];
    }
    FragColor = vec4(result, 1.0);
}
