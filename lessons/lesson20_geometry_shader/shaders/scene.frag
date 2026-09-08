#version 330 core
in vec3 gNormal;
in vec3 gPos;
out vec4 FragColor;
uniform vec3 uLightPos;
uniform vec3 uViewPos;
uniform vec3 uColor;
void main()
{
    vec3 ambient = 0.15 * uColor;
    vec3 norm = normalize(gNormal);
    vec3 lightDir = normalize(uLightPos - gPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * uColor;
    vec3 viewDir = normalize(uViewPos - gPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
    vec3 specular = 0.5 * spec * vec3(1.0);
    FragColor = vec4(ambient + diffuse + specular, 1.0);
}
