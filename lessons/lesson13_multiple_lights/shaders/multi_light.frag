#version 330 core
in vec3 FragPos;
in vec3 Normal;
out vec4 FragColor;

// 方向光
struct DirLight {
    vec3 direction;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};
// 点光源
struct PointLight {
    vec3 position;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    float constant;
    float linear;
    float quadratic;
};

#define NR_POINT_LIGHTS 4
uniform DirLight uDirLight;
uniform PointLight uPointLights[NR_POINT_LIGHTS];
uniform vec3 uViewPos;

// 材质（固定）
vec3 matAmbient  = vec3(0.25, 0.20, 0.20);
vec3 matDiffuse  = vec3(0.4, 0.4, 0.4);
vec3 matSpecular = vec3(0.5, 0.5, 0.5);
float matShininess = 32.0;

vec3 calcDirLight(DirLight light, vec3 normal, vec3 viewDir)
{
    vec3 lightDir = normalize(-light.direction);
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), matShininess);
    return light.ambient * matAmbient +
           light.diffuse * diff * matDiffuse +
           light.specular * spec * matSpecular;
}

vec3 calcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir)
{
    vec3 lightDir = normalize(light.position - fragPos);
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), matShininess);
    float dist = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * dist + light.quadratic * dist * dist);
    return (light.ambient * matAmbient +
            light.diffuse * diff * matDiffuse +
            light.specular * spec * matSpecular) * attenuation;
}

void main()
{
    vec3 norm = normalize(Normal);
    vec3 viewDir = normalize(uViewPos - FragPos);

    // 方向光
    vec3 result = calcDirLight(uDirLight, norm, viewDir);

    // 逐个点光源累加
    for (int i = 0; i < NR_POINT_LIGHTS; i++)
        result += calcPointLight(uPointLights[i], norm, FragPos, viewDir);

    FragColor = vec4(result, 1.0);
}
