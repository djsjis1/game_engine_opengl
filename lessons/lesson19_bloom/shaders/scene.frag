#version 330 core
in vec3 FragPos;
in vec3 Normal;
out vec4 FragColor;
uniform vec3 uLightPos;
uniform vec3 uViewPos;
uniform vec3 uColor; // 发光颜色
void main()
{
    vec3 lightColor = vec3(1.0);
    vec3 ambient = 0.05 * lightColor;
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(uLightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;
    vec3 viewDir = normalize(uViewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 128.0);
    vec3 specular = 0.8 * spec * lightColor;
    float dist = length(uLightPos - FragPos);
    float attenuation = 1.0 / (1.0 + 0.14 * dist + 0.07 * dist * dist);
    vec3 result = (ambient + (diffuse + specular) * attenuation) * uColor;
    FragColor = vec4(result, 1.0); // HDR: 值可 > 1.0
}
