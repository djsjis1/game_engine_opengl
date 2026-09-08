#version 330 core
// ============================================================
// blur.frag — 高斯模糊（Bloom 第 2 步，水平/垂直两趟 ping-pong）
// 5 权重核（LearnOpenGL Bloom 章标准配置），uHorizontal 切方向
// ============================================================
in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uImage;
uniform int uHorizontal;// 1=水平趟 0=垂直趟

void main()
{
    float weight[5]=float[](.227027,.1945946,.1216216,.054054,.016216);
    vec2 texel=1./textureSize(uImage,0);
    vec3 result=texture(uImage,vUV).rgb*weight[0];
    for(int i=1;i<5;++i)
    {
        vec2 off=(uHorizontal==1)?vec2(texel.x*i,0.)
        :vec2(0.,texel.y*i);
        result+=texture(uImage,vUV+off).rgb*weight[i];
        result+=texture(uImage,vUV-off).rgb*weight[i];
    }
    FragColor=vec4(result,1.);
}
