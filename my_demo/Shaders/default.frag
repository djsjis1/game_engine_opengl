#version 330 core

// 从顶点着色器接收
in vec3 vFragPos;
in vec3 vNormal;

// 方向光
struct DirLight{
    vec3 direction;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};
uniform DirLight uLight;

// 材质
struct Material{
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    float shininess;
};
uniform Material uMaterial;

uniform vec3 uCameraPos;

out vec4 fragColor;

void main()
{
    // 环境光
    vec3 ambient=uLight.ambient*uMaterial.ambient;
    
    // 漫反射（Lambert）
    vec3 norm=normalize(vNormal);
    vec3 lightDir=normalize(-uLight.direction);// 方向光：朝向光源
    float diff=max(dot(norm,lightDir),0.);
    vec3 diffuse=uLight.diffuse*diff*uMaterial.diffuse;
    
    // 镜面反射（Blinn-Phong）
    vec3 viewDir=normalize(uCameraPos-vFragPos);
    vec3 halfwayDir=normalize(lightDir+viewDir);
    float spec=pow(max(dot(norm,halfwayDir),0.),uMaterial.shininess);
    vec3 specular=uLight.specular*spec*uMaterial.specular;
    
    vec3 result=ambient+diffuse+specular;
    fragColor=vec4(result,1.);
}
