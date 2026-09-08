#version 330 core
// ============================================================
// particle.frag — 粒子片元：软圆点（无纹理，程序化径向渐变）
//
// 距 quad 中心越远越透明 → 圆形软粒子（火/烟/光效通用）。
// 输出线性 HDR 颜色（intensity>1 时后处理 ACES 会给出辉光感）。
// ============================================================
in vec4 vColor;
in vec2 vUv;

out vec4 FragColor;

void main()
{
    // 径向衰减：中心 1 → 边缘 0（平方衰减更柔）
    vec2 d=vUv-vec2(.5);
    float r2=dot(d,d)*4.;// 中心 0，边缘 1
    float falloff=clamp(1.-r2,0.,1.);
    falloff*=falloff;
    
    float alpha=vColor.a*falloff;
    if(alpha<.005)
    discard;// 全透明片元不写（省混合带宽）
    FragColor=vec4(vColor.rgb,alpha);
}
