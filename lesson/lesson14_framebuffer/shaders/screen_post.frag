#version 330 core
in vec2 TexCoord;
out vec4 FragColor;
uniform sampler2D uScreenTexture;
uniform int uEffect; // 0=原图 1=反相 2=灰度 3=锐化 4=模糊 5=边缘检测

const float offset = 1.0 / 300.0; // 像素偏移（近似）

vec3 applyKernel(mat3 kernel)
{
    vec2 offsets[9] = vec2[9](
        vec2(-offset,  offset), vec2(0,  offset), vec2( offset,  offset),
        vec2(-offset,  0),     vec2(0,  0),     vec2( offset,  0),
        vec2(-offset, -offset), vec2(0, -offset), vec2( offset, -offset)
    );
    float k[9] = float[9](
        kernel[0][0], kernel[0][1], kernel[0][2],
        kernel[1][0], kernel[1][1], kernel[1][2],
        kernel[2][0], kernel[2][1], kernel[2][2]
    );
    vec3 sampleTex[9];
    for (int i = 0; i < 9; i++)
        sampleTex[i] = texture(uScreenTexture, TexCoord.st + offsets[i]).rgb;
    vec3 col = vec3(0.0);
    for (int i = 0; i < 9; i++)
        col += sampleTex[i] * k[i];
    return col;
}

void main()
{
    vec3 color = texture(uScreenTexture, TexCoord).rgb;

    if (uEffect == 1)
        color = vec3(1.0) - color;                          // 反相
    else if (uEffect == 2)
    {
        float gray = dot(color, vec3(0.2126, 0.7152, 0.0722)); // 灰度
        color = vec3(gray);
    }
    else if (uEffect == 3)
    {
        mat3 sharpen = mat3( 0,-1, 0, -1, 5,-1,  0,-1, 0);   // 锐化
        color = applyKernel(sharpen);
    }
    else if (uEffect == 4)
    {
        mat3 blur = mat3(1,2,1, 2,4,2, 1,2,1) / 16.0;        // 模糊
        color = applyKernel(blur);
    }
    else if (uEffect == 5)
    {
        mat3 edge = mat3( 1, 1, 1,  1,-8, 1,  1, 1, 1);      // 边缘检测
        color = applyKernel(edge);
    }

    FragColor = vec4(color, 1.0);
}
