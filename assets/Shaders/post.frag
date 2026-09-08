#version 330 core

// 后处理片元着色器（对标 Godot Environment 调色）
//   ① HDR 色调映射（Reinhard/ACES 近似）：把超过 1.0 的亮度压回可显示范围
//   ② 曝光调节
//   ③ 伽马校正：线性空间 → sRGB 显示空间（不做的话画面偏暗）
//   ④ 暗角(vignette)：边缘压暗，聚焦中心（电影感）

in vec2 vUV;
out vec4 fragColor;

uniform sampler2D uScene;// 离屏渲染的场景纹理
uniform sampler2D uBloom;// 模糊后的高亮纹理（Bloom 叠加）
uniform int uBloomOn;// Bloom 是否可用
uniform float uExposure;// 曝光倍率（默认 1.0）
uniform int uToneMap;// 是否色调映射
uniform float uVignette;// 暗角强度（0=关）

// ACES 电影级色调映射近似（比 Reinhard 更有对比度，游戏业界常用）
vec3 acesToneMap(vec3 x)
{
    const float a=2.51,b=.03,c=2.43,d=.59,e=.14;
    return clamp((x*(a*x+b))/(x*(c*x+d)+e),0.,1.);
}

void main()
{
    vec3 color=texture(uScene,vUV).rgb;
    
    // ⓪ Bloom 叠加：高亮溢出的柔光（火焰/发光粒子真正"发光"）
    //   在色调映射【之前】加——让泛光也参与 HDR→LDR 压缩，过渡自然
    if(uBloomOn==1)
    color+=texture(uBloom,vUV).rgb;
    
    // ① 曝光
    color*=uExposure;
    
    // ② 色调映射（HDR→LDR）
    if(uToneMap==1)
    color=acesToneMap(color);
    
    // ③ 伽马校正（线性→sRGB，gamma 2.2）
    color=pow(color,vec3(1./2.2));
    
    // ④ 暗角：离中心越远越暗
    if(uVignette>0.)
    {
        vec2 d=vUV-.5;
        float vig=1.-dot(d,d)*uVignette;
        color*=clamp(vig,0.,1.);
    }
    
    fragColor=vec4(color,1.);
}
