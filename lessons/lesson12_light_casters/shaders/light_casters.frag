#version 330 core
in vec3 FragPos;
in vec3 Normal;
out vec4 FragColor;

struct Light {
    int type;            // 0=Directional, 1=Point, 2=Spot
    vec3 position;       // 点光源/聚光灯位置
    vec3 direction;      // 方向光/聚光灯方向
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    // 衰减（仅 Point/Spot 使用）
    float constant;
    float linear;
    float quadratic;
    // 聚光灯切角
    float cutOff;        // 内切角 cos
    float outerCutOff;   // 外切角 cos
};
uniform Light uLight;
uniform vec3 uViewPos;

// 材质固定
vec3 matAmbient  = vec3(0.2, 0.5, 0.2);
vec3 matDiffuse  = vec3(0.3, 0.7, 0.3);
vec3 matSpecular = vec3(0.4, 0.4, 0.4);
float matShininess = 64.0;

void main()
{
    vec3 ambient  = uLight.ambient * matAmbient;
    vec3 norm     = normalize(Normal);
    vec3 viewDir  = normalize(uViewPos - FragPos);

    // ----- 方向光：统一方向，无衰减 -----
    if (uLight.type == 0)
    {
        vec3 lightDir = normalize(-uLight.direction);
        float diff = max(dot(norm, lightDir), 0.0);
        vec3 diffuse = uLight.diffuse * diff * matDiffuse;
        vec3 reflectDir = reflect(-lightDir, norm);
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), matShininess);
        vec3 specular = uLight.specular * spec * matSpecular;
        FragColor = vec4(ambient + diffuse + specular, 1.0);
        return;
    }

    // ----- 点光源 / 聚光灯：有位置和衰减 -----
    vec3 lightDir = normalize(uLight.position - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = uLight.diffuse * diff * matDiffuse;
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), matShininess);
    vec3 specular = uLight.specular * spec * matSpecular;

    // 衰减
    float dist = length(uLight.position - FragPos);
    float attenuation = 1.0 / (uLight.constant + uLight.linear * dist + uLight.quadratic * dist * dist);

    // 聚光灯边缘软化
    float intensity = 1.0;
    if (uLight.type == 2)
    {
        float theta = dot(lightDir, normalize(-uLight.direction));
        float epsilon = uLight.cutOff - uLight.outerCutOff;
        intensity = clamp((theta - uLight.outerCutOff) / epsilon, 0.0, 1.0);
    }

    ambient  *= attenuation;
    diffuse  *= attenuation * intensity;
    specular *= attenuation * intensity;

    FragColor = vec4(ambient + diffuse + specular, 1.0);
}
