#version 330 core

// 从顶点着色器接收
in vec3 vFragPos;
in vec3 vNormal;
in vec2 vUV;

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

// 精灵帧纹理（图片动画）：uHasTexture=0 时完全走原来的纯颜色光照路径
uniform sampler2D uTexture;
uniform int uHasTexture;

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
    
    // 有纹理：颜色乘到光照结果上（颜色轨道动画仍可叠加染色）
    if(uHasTexture==1){
        vec4 tex=texture(uTexture,vUV);
        if(tex.a<.1)discard;// 透明像素不画（精灵镞空）
        result*=tex.rgb;
        fragColor=vec4(result,tex.a);
    }else{
        fragColor=vec4(result,1.);
    }
}
