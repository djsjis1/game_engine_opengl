#version 330 core

// 天空盒片元着色器：程序化渐变天空（不需要 6 张贴图）
// 按视线方向的 y 分量在"地平线色→天顶色"之间插值，
// 模拟真实天空（越往上越深蓝，地平线泛白）。对标 Godot ProceduralSky。

in vec3 vDir;
out vec4 fragColor;

uniform vec3 uHorizonColor;// 地平线颜色（浅）
uniform vec3 uZenithColor;// 天顶颜色（深）
uniform vec3 uGroundColor;// 地面以下颜色

void main()
{
    // 视线方向归一化后的 y：+1 正上方，0 地平线，-1 正下方
    float y=normalize(vDir).y;
    vec3 color;
    if(y>0.)
    // 天空：地平线→天顶，用 sqrt 让过渡更集中在地平线附近（更真实）
    color=mix(uHorizonColor,uZenithColor,sqrt(y));
    else
    // 地平线以下：渐变到地面色
    color=mix(uHorizonColor,uGroundColor,min(-y*4.,1.));
    fragColor=vec4(color,1.);
}
