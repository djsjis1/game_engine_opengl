#version 330 core
in VS_OUT {
    vec3 FragPos;
    vec2 TexCoord;
    mat3 TBN;
} fs_in;
out vec4 FragColor;

uniform sampler2D uNormalMap;  // 法线贴图
uniform vec3 uLightPos;        // 点光源位置
uniform vec3 uViewPos;
uniform bool uShowNormalOnly;  // 仅可视化法线

void main()
{
    // 从法线贴图采样，变换到 [0,1] → [-1,1]
    vec3 normal = texture(uNormalMap, fs_in.TexCoord).rgb;
    normal = normalize(normal * 2.0 - 1.0);

    if (uShowNormalOnly)
    {
        FragColor = vec4(normal * 0.5 + 0.5, 1.0); // 可视化法线颜色
        return;
    }

    // 【知识点】TBN 矩阵将切线空间法线转世界空间
    vec3 worldNormal = normalize(fs_in.TBN * normal);

    vec3 lightColor = vec3(1.0);
    vec3 ambient = 0.15 * lightColor;
    vec3 lightDir = normalize(uLightPos - fs_in.FragPos);
    float diff = max(dot(worldNormal, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;
    vec3 viewDir = normalize(uViewPos - fs_in.FragPos);
    vec3 reflectDir = reflect(-lightDir, worldNormal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 64.0);
    vec3 specular = 0.5 * spec * lightColor;

    // 衰减
    float dist = length(uLightPos - fs_in.FragPos);
    float attenuation = 1.0 / (1.0 + 0.09 * dist + 0.032 * dist * dist);

    // 砖墙基础色（混合贴图）
    vec3 brickColor = vec3(0.7, 0.3, 0.2); // 红砖色
    vec3 result = (ambient + (diffuse + specular) * attenuation) * brickColor;
    FragColor = vec4(result, 1.0);
}
