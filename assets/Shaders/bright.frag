#version 330 core
// ============================================================
// bright.frag — 亮度提取（Bloom 第 1 步）
// 只留超过阈值的高亮部分（HDR 场景里 >1.0 的火焰/发光粒子），
// 其余置黑。半分辨率渲染（省带宽，模糊后看不出差别）。
// ============================================================
in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uScene;
uniform float uThreshold;// 亮度阈值（默认 1.0：只有 HDR 溢出才泛光）

void main()
{
    vec3 color=texture(uScene,vUV).rgb;
    // 感知亮度（Rec.709 权重）
    float brightness=dot(color,vec3(.2126,.7152,.0722));
    FragColor=(brightness>uThreshold)?vec4(color,1.)
    :vec4(0.,0.,0.,1.);
}
