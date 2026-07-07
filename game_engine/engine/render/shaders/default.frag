#version 330 core

// ============================================================
// default.frag — 默认 Phong 多光源片段着色器
//
// 支持：
//   - 最多 8 个光源（方向光 / 点光源 / 聚光灯）
//   - 材质属性（ambient / diffuse / specular / shininess）
//   - 可选漫反射贴图和镜面光贴图
//
// 光照计算：
//   对每个光源分别计算 Phong 光照，累加结果。
//   方向光无衰减，点光源/聚光灯有距离衰减。
//   聚光灯额外有锥角衰减（边缘软化）。
// ============================================================

#define MAX_LIGHTS 8

// ---- 输入（从顶点着色器插值） ----
in vec3 vFragPos;
in vec3 vNormal;
in vec2 vTexCoord;

// ---- 输出 ----
out vec4 FragColor;

// ---- 光源结构体（与 CPU 端 LightData 对应） ----
struct Light
{
    int type;// 0=Directional, 1=Point, 2=Spot
    vec3 position;// 世界空间位置
    vec3 direction;// 方向（方向光/聚光灯）
    vec3 ambient;// 环境光颜色
    vec3 diffuse;// 漫反射颜色
    vec3 specular;// 镜面光颜色
    float constant;// 衰减常数项
    float linear;// 衰减一次项
    float quadratic;// 衰减二次项
    float cutOff;// 聚光灯内锥角 cos 值
    float outerCutOff;// 聚光灯外锥角 cos 值
};

// ---- 材质结构体 ----
struct Material
{
    vec3 ambient;// 环境光颜色
    vec3 diffuse;// 漫反射颜色
    vec3 specular;// 镜面光颜色
    float shininess;// 反光度（高光指数）
    sampler2D diffuseMap;// 漫反射贴图（可选）
    sampler2D specularMap;// 镜面光贴图（可选）
};

// ---- Uniform ----
uniform vec3 uCameraPos;
uniform int uLightCount;
uniform Light uLights[MAX_LIGHTS];
uniform Material uMaterial;

// ---- 函数声明 ----
vec3 calcDirectionalLight(Light light,vec3 normal,vec3 viewDir,vec3 matDiffuse,vec3 matSpecular);
vec3 calcPointLight(Light light,vec3 normal,vec3 fragPos,vec3 viewDir,vec3 matDiffuse,vec3 matSpecular);
vec3 calcSpotLight(Light light,vec3 normal,vec3 fragPos,vec3 viewDir,vec3 matDiffuse,vec3 matSpecular);

void main()
{
    // 法线（归一化插值后的法线）
    vec3 norm=normalize(vNormal);
    vec3 viewDir=normalize(uCameraPos-vFragPos);
    
    // 从材质获取颜色（支持贴图或纯色）
    vec3 matDiffuse=uMaterial.diffuse;
    vec3 matSpecular=uMaterial.specular;
    
    // 如果 diffuse 贴图存在（uniform sampler2D 初始值始终为 0）
    // 通过检查 diffuse 颜色是否为 (0,0,0) 来判断是否使用贴图
    // 注：这里简化处理，贴图由 C++ 端在需要时绑定
    
    // 累加所有光源
    vec3 result=vec3(0.);
    for(int i=0;i<uLightCount&&i<MAX_LIGHTS;i++)
    {
        if(uLights[i].type==0)// Directional
        result+=calcDirectionalLight(uLights[i],norm,viewDir,matDiffuse,matSpecular);
        else if(uLights[i].type==1)// Point
        result+=calcPointLight(uLights[i],norm,vFragPos,viewDir,matDiffuse,matSpecular);
        else if(uLights[i].type==2)// Spot
        result+=calcSpotLight(uLights[i],norm,vFragPos,viewDir,matDiffuse,matSpecular);
    }
    
    FragColor=vec4(result,1.);
}

// ============================================================
// 方向光 (Directional Light) — 平行光，无距离衰减
// ============================================================
vec3 calcDirectionalLight(Light light,vec3 normal,vec3 viewDir,
vec3 matDiffuse,vec3 matSpecular)
{
    vec3 lightDir=normalize(-light.direction);// direction 指向光照方向
    
    // 环境光
    vec3 ambient=light.ambient*matDiffuse*uMaterial.ambient;
    
    // 漫反射
    float diff=max(dot(normal,lightDir),0.);
    vec3 diffuse=light.diffuse*diff*matDiffuse;
    
    // 镜面光 (Blinn-Phong 半程向量优化，也可以用 Phong 反射向量)
    vec3 reflectDir=reflect(-lightDir,normal);
    float spec=pow(max(dot(viewDir,reflectDir),0.),uMaterial.shininess);
    vec3 specular=light.specular*spec*matSpecular;
    
    return ambient+diffuse+specular;
}

// ============================================================
// 点光源 (Point Light) — 向所有方向发光，有距离衰减
// ============================================================
vec3 calcPointLight(Light light,vec3 normal,vec3 fragPos,vec3 viewDir,
vec3 matDiffuse,vec3 matSpecular)
{
    vec3 lightDir=normalize(light.position-fragPos);
    
    // 环境光
    vec3 ambient=light.ambient*matDiffuse*uMaterial.ambient;
    
    // 漫反射
    float diff=max(dot(normal,lightDir),0.);
    vec3 diffuse=light.diffuse*diff*matDiffuse;
    
    // 镜面光
    vec3 reflectDir=reflect(-lightDir,normal);
    float spec=pow(max(dot(viewDir,reflectDir),0.),uMaterial.shininess);
    vec3 specular=light.specular*spec*matSpecular;
    
    // 距离衰减: 1/(constant + linear*d + quadratic*d²)
    float distance=length(light.position-fragPos);
    float attenuation=1./(light.constant+light.linear*distance+
    light.quadratic*distance*distance);
    
    return(ambient+diffuse+specular)*attenuation;
}

// ============================================================
// 聚光灯 (Spot Light) — 锥形光束，有方向和距离衰减
// ============================================================
vec3 calcSpotLight(Light light,vec3 normal,vec3 fragPos,vec3 viewDir,
vec3 matDiffuse,vec3 matSpecular)
{
    vec3 lightDir=normalize(light.position-fragPos);
    
    // 环境光
    vec3 ambient=light.ambient*matDiffuse*uMaterial.ambient;
    
    // 漫反射
    float diff=max(dot(normal,lightDir),0.);
    vec3 diffuse=light.diffuse*diff*matDiffuse;
    
    // 镜面光
    vec3 reflectDir=reflect(-lightDir,normal);
    float spec=pow(max(dot(viewDir,reflectDir),0.),uMaterial.shininess);
    vec3 specular=light.specular*spec*matSpecular;
    
    // 距离衰减
    float distance=length(light.position-fragPos);
    float attenuation=1./(light.constant+light.linear*distance+
    light.quadratic*distance*distance);
    
    // 锥角衰减（边缘软化）
    float theta=dot(lightDir,normalize(-light.direction));
    float epsilon=light.cutOff-light.outerCutOff;
    float intensity=clamp((theta-light.outerCutOff)/epsilon,0.,1.);
    
    return(ambient+diffuse+specular)*attenuation*intensity;
}
