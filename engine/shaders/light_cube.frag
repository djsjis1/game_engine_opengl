#version 330 core

// ============================================================
// light_cube.frag — 光源可视化片段着色器
//
// 始终输出白色，表示光源位置。
// 也可从 uniform 获取光源颜色以显示不同颜色的光。
// ============================================================

out vec4 FragColor;

uniform vec3 uLightColor=vec3(1.,1.,1.);

void main()
{
    FragColor=vec4(uLightColor,1.);
}
