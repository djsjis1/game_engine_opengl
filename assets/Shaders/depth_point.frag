#version 330 core
// ============================================================
// depth_point.frag — 点光阴影：把"到光源的线性距离"写入深度
//
// 为什么不用透视深度：cubemap 六个面各有各的投影，非线性深度
// 没法跨面比较；统一存 线性距离/far ∈ [0,1] 采样时乘回 far 即可。
// ============================================================
in vec3 vWorldPos;

uniform vec3 uLightPos;// 点光位置
uniform float uFarPlane;// 阴影覆盖半径

void main()
{
    float dist=length(vWorldPos-uLightPos);
    gl_FragDepth=clamp(dist/uFarPlane,0.,1.);
}
