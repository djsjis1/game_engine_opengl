#version 330 core

// 从顶点着色器接收
in vec3 vFragPos;
in vec3 vNormal;
in vec2 vUV;
in vec4 vFragPosLightSpace;// 阴影：片元在光空间坐标（顶点着色器算好）

// ============================================================
// 多光源（对标 Godot：方向光/点光/聚光灯混合）
//   type: 0=方向光 1=点光 2=聚光灯
//   方向光：dir 有效，pos 忽略，不衰减
//   点光源：pos 有效，dir 忽略，随距离衰减
//   聚光灯：pos+dir 都用，锥形范围 + 距离衰减
// ============================================================
#define MAX_LIGHTS 8

struct Light{
    int type;
    vec3 position;// 点光/聚光的世界位置
    vec3 direction;// 方向光/聚光的照射方向
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    float range;// 点光/聚光衰减半径
    float spotCos;// 聚光锥角余弦（cos(半角)），聚光才用
};
uniform Light uLights[MAX_LIGHTS];
uniform int uLightCount;

// 材质（PBR 金属度/粗糙度工作流，对标 Godot StandardMaterial3D）
struct Material{
    vec3 albedo;// 基础色
    float metallic;// 0=非金属 1=金属
    float roughness;// 0=镜面 1=粗糙
    float ao;// 环境光遮蔽
};
uniform Material uMaterial;

uniform vec3 uCameraPos;

// 精灵帧纹理
uniform sampler2D uTexture;
uniform int uHasTexture;

// 阴影：光源深度贴图 + 开关。方向光/聚光共用同一张图和同一套采样，
// uShadowLightType/uShadowLightPos 标记"当前是哪盏光在投影"
uniform sampler2D uShadowMap;
uniform int uShadowEnabled;
uniform int uShadowLightType;// 0=方向光 2=聚光灯
uniform vec3 uShadowLightPos;// 聚光位置（多盏聚光时匹配"这盏"）

// 点光阴影（cubemap 深度，存"线性距离/far"）：只支持首个点光源
uniform samplerCube uPointShadowMap;
uniform int uPointShadowEnabled;
uniform vec3 uPointShadowLightPos;// 投影的那盏点光位置
uniform float uPointShadowFar;// 阴影覆盖半径（写入端同值）

// 距离雾（环境组件驱动；uFogDensity=0 时关闭）
uniform vec3 uFogColor;
uniform float uFogDensity;

out vec4 fragColor;

// 阴影因子：0=全阴影 1=全亮。采样光源深度贴图，比较当前片元深度。
// 用 PCF（3x3 采样平均）软化锯齿边缘。
float calcShadow(vec4 fragPosLightSpace,vec3 norm,vec3 lightDir)
{
    // 透视除法 → NDC [-1,1] → 映射到 [0,1] 纹理坐标
    vec3 proj=fragPosLightSpace.xyz/fragPosLightSpace.w;
    proj=proj*.5+.5;
    if(proj.z>1.)
    return 1.;// 超出光锥远平面：不投影
    // 斜面偏移（防"阴影痤疮"self-shadowing 条纹）：法线与光夹角越大偏移越大
    float bias=max(.002*(1.-dot(norm,lightDir)),.0005);
    // PCF：周围 3x3 texel 采样求平均，软化边缘
    float shadow=0.;
    vec2 texel=1./textureSize(uShadowMap,0);
    for(int x=-1;x<=1;++x)
    for(int y=-1;y<=1;++y)
    {
        float pcfDepth=texture(uShadowMap,proj.xy+vec2(x,y)*texel).r;
        shadow+=(proj.z-bias>pcfDepth)?0.:1.;
    }
    return shadow/9.;
}

// 点光阴影因子：cubemap 方向采样（片元→光源向量直接当纹理坐标，
// 这正是 cubemap 的天然优势——不需要任何矩阵变换）
float calcPointShadow(vec3 fragPos)
{
    vec3 toFrag=fragPos-uPointShadowLightPos;
    float dist=length(toFrag);
    if(dist>uPointShadowFar)
    return 1.;// 超出阴影半径：不遮挡
    float closest=texture(uPointShadowMap,toFrag).r*uPointShadowFar;
    float bias=max(.05,.02*dist);// 距离越远偏移越大（线性深度精度均匀）
    return(dist-bias>closest)?0.:1.;
}

// ============================================================
// Cook-Torrance BRDF（PBR 核心，三个经典子项）
//   D — GGX 法线分布：粗糙度决定高光斑的大小/锐利
//   F — Fresnel-Schlick：掠射角反射增强（万物皆有菲涅尔）
//   G — Smith 几何遮蔽：微表面互相挡光（粗糙表面更明显）
// 参考 LearnOpenGL PBR 章 / Godot 同款模型
// ============================================================
const float PI=3.14159265359;

float distributionGGX(vec3 N,vec3 H,float roughness)
{
    float a=roughness*roughness;// Disney 惯例：粗糙度平方，手感更线性
    float a2=a*a;
    float NdotH=max(dot(N,H),0.);
    float denom=NdotH*NdotH*(a2-1.)+1.;
    return a2/(PI*denom*denom);
}

float geometrySchlickGGX(float NdotV,float roughness)
{
    float r=roughness+1.;
    float k=r*r/8.;// 直接光的 k 映射
    return NdotV/(NdotV*(1.-k)+k);
}

float geometrySmith(vec3 N,vec3 V,vec3 L,float roughness)
{
    return geometrySchlickGGX(max(dot(N,V),0.),roughness)
    *geometrySchlickGGX(max(dot(N,L),0.),roughness);
}

vec3 fresnelSchlick(float cosTheta,vec3 F0)
{
    return F0+(1.-F0)*pow(clamp(1.-cosTheta,0.,1.),5.);
}

// 单个光源的 PBR 贡献（albedo/F0 由 main 算好传入，纹理已乘进 albedo）
vec3 calcLight(Light light,vec3 N,vec3 V,vec3 albedo,vec3 F0)
{
    // ① 光线方向 + 衰减系数（按光型区分，沿用原逻辑）
    vec3 L;
    float attenuation=1.;
    if(light.type==0){
        L=normalize(-light.direction);// 方向光：全场同向不衰减
    }else{
        vec3 toLight=light.position-vFragPos;
        float dist=length(toLight);
        L=normalize(toLight);
        attenuation=clamp(1.-dist/light.range,0.,1.);
        attenuation*=attenuation;
        if(light.type==2){
            float theta=dot(L,normalize(-light.direction));
            attenuation*=clamp((theta-light.spotCos)/.05,0.,1.);
        }
    }
    
    // ② Cook-Torrance：radiance（光的能量）× BRDF × NdotL
    vec3 radiance=light.diffuse*attenuation;// light.diffuse 当光色×强度
    vec3 H=normalize(V+L);
    float NDF=distributionGGX(N,H,uMaterial.roughness);
    float G=geometrySmith(N,V,L,uMaterial.roughness);
    vec3 F=fresnelSchlick(max(dot(H,V),0.),F0);
    float NdotL=max(dot(N,L),0.);
    float NdotV=max(dot(N,V),0.);
    vec3 specular=(NDF*G*F)/(4.*NdotV*NdotL+.0001);
    // 能量守恒：反射掉的(F)不再漫反射；金属无漫反射
    vec3 kD=(vec3(1.)-F)*(1.-uMaterial.metallic);
    
    // ③ 阴影：方向光/聚光走 2D 深度图（谁在投影由 uShadowLightType 标记，
    //   聚光还要位置匹配"这盏"）；首个点光走 cubemap
    float shadow=1.;
    if(uShadowEnabled==1&&light.type==uShadowLightType
        &&(light.type==0||distance(light.position,uShadowLightPos)<.01))
        shadow=calcShadow(vFragPosLightSpace,N,L);
        else if(light.type==1&&uPointShadowEnabled==1
            &&distance(light.position,uPointShadowLightPos)<.01)
            shadow=calcPointShadow(vFragPos);
            
            // ④ 环境光：light.ambient 当环境辐照，albedo 调色，ao 压缝隙
            vec3 ambient=light.ambient*albedo*uMaterial.ao;
            
            return ambient*attenuation
            +(kD*albedo/PI+specular)*radiance*NdotL*shadow;
        }
        
        void main()
        {
            vec3 N=normalize(vNormal);
            vec3 V=normalize(uCameraPos-vFragPos);
            
            // 纹理乘进 albedo（PBR 正确姿势：纹理是表面属性，参与 BRDF，
            // 而不是乘在光照结果上——高光颜色才会正确）
            vec3 albedo=uMaterial.albedo;
            float alpha=1.;
            if(uHasTexture==1){
                vec4 tex=texture(uTexture,vUV);
                if(tex.a<.1)discard;
                albedo*=tex.rgb;
                alpha=tex.a;
            }
            // F0 基础反射率：非金属统一 4%（物理测量值），金属用 albedo 本色
            vec3 F0=mix(vec3(.04),albedo,uMaterial.metallic);
            
            // 累加所有光源贡献
            vec3 result=vec3(0.);
            for(int i=0;i<uLightCount&&i<MAX_LIGHTS;++i)
            result+=calcLight(uLights[i],N,V,albedo,F0);
            
            // 无光源时给个保底环境光（防全黑）
            if(uLightCount==0)
            result=albedo*.3;
            
            // 距离雾：离相机越远越接近雾色（指数雾，密度 0 时跳过）
            if(uFogDensity>0.){
                float dist=length(uCameraPos-vFragPos);
                float fog=1.-exp(-uFogDensity*dist);
                result=mix(result,uFogColor,clamp(fog,0.,1.));
            }
            
            // 注：PBR 输出的是线性 HDR 值（可以 >1），由后处理 pass 做
            // ACES 色调映射 + 伽马校正——这正是第5阶后处理存在的意义
            fragColor=vec4(result,alpha);
        }
        
        