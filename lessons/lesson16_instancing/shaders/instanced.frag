#version 330 core
in vec3 FragPos;
in vec3 Normal;
in vec3 InstanceColor;
out vec4 FragColor;

uniform vec3 uViewPos;
uniform vec3 uLightDir; // 方向光

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
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
    vec3 specular = 0.5 * spec * lightColor;
    FragColor = vec4((ambient + diffuse + specular) * InstanceColor, 1.0);
}
