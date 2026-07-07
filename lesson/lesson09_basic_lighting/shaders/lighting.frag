#version 330 core
// Lesson 09 片段着色器 - Phong光照模型
// Phong = 环境光(Ambient) + 漫反射(Diffuse) + 镜面反射(Specular)

in vec3 FragPos;
in vec3 Normal;
out vec4 FragColor;

uniform vec3 uObjectColor;
uniform vec3 uLightColor;
uniform vec3 uLightPos;
uniform vec3 uViewPos;

void main()
{
    // 环境光：模拟间接光照，最暗的颜色
    float ambientStrength = 0.2;
    vec3 ambient = ambientStrength * uLightColor;

    // 漫反射：法线与光线夹角越小越亮
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(uLightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * uLightColor;

    // 镜面反射：高光亮点
    float specularStrength = 0.5;
    vec3 viewDir = normalize(uViewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
    vec3 specular = specularStrength * spec * uLightColor;

    vec3 result = (ambient + diffuse + specular) * uObjectColor;
    FragColor = vec4(result, 1.0);
}
