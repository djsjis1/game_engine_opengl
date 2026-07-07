#version 330 core
in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;
out vec4 FragColor;

// Material 中 diffuse 与 specular 改为采样器（贴图）
struct Material {
    sampler2D diffuse;
    sampler2D specular;
    float shininess;
};
struct Light {
    vec3 position;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

uniform Material uMaterial;
uniform Light uLight;
uniform vec3 uViewPos;

void main()
{
    // 从贴图取值
    vec3 diffuseTex = texture(uMaterial.diffuse, TexCoord).rgb;
    vec3 specTex    = texture(uMaterial.specular, TexCoord).rgb;

    // ambient
    vec3 ambient = uLight.ambient * diffuseTex;

    // diffuse
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(uLight.position - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = uLight.diffuse * diff * diffuseTex;

    // specular（被 specular map 调制）
    vec3 viewDir = normalize(uViewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), uMaterial.shininess);
    vec3 specular = uLight.specular * spec * specTex;

    FragColor = vec4(ambient + diffuse + specular, 1.0);
}
