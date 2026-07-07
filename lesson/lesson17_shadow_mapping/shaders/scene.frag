#version 330 core
in vec3 FragPos;
in vec3 Normal;
in vec4 FragPosLightSpace;
out vec4 FragColor;

uniform sampler2D uShadowMap;
uniform vec3 uViewPos;
uniform vec3 uLightDir; // 方向光
uniform vec3 uObjectColor;

// 【知识点】PCF 百分比渐近滤波 - 多重采样软化阴影边缘
float shadowCalculation(vec4 fragPosLightSpace)
{
    // 透视除法：变换到 NDC [-1,1]
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    // 变换到深度贴图坐标 [0,1]
    projCoords = projCoords * 0.5 + 0.5;

    if (projCoords.z > 1.0)
        return 0.0; // 超出远平面，无阴影

    float currentDepth = projCoords.z;
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(uShadowMap, 0);

    // PCF: 3x3 采样
    for (int x = -1; x <= 1; ++x)
        for (int y = -1; y <= 1; ++y)
        {
            float pcfDepth = texture(uShadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += currentDepth - 0.001 > pcfDepth ? 1.0 : 0.0;
        }
    return shadow / 9.0;
}

void main()
{
    vec3 lightColor = vec3(1.0);
    vec3 ambient = 0.15 * lightColor;
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(-uLightDir);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;
    vec3 viewDir = normalize(uViewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 64.0);
    vec3 specular = 0.5 * spec * lightColor;

    // 阴影计算
    float shadow = shadowCalculation(FragPosLightSpace);

    vec3 result = (ambient + (1.0 - shadow) * (diffuse + specular)) * uObjectColor;
    FragColor = vec4(result, 1.0);
}
