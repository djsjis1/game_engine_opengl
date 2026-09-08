#version 330 core

// 顶点属性
layout(location=0)in vec3 aPos;// 位置
layout(location=1)in vec3 aNormal;// 法线
layout(location=2)in vec2 aUV;// 纹理坐标（精灵帧动画贴图用）

// Uniform — 矩阵变换
uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform mat3 uNormalMatrix;
uniform mat4 uLightSpaceMatrix;// 阴影：世界→光空间（Pass2 采样深度贴图用）

// 输出到片段着色器
out vec3 vFragPos;// 世界空间位置（用于光照计算）
out vec3 vNormal;// 世界空间法线
out vec2 vUV;// 纹理坐标
out vec4 vFragPosLightSpace;// 阴影：片元在光空间的坐标

void main()
{
    vec4 worldPos=uModel*vec4(aPos,1.);
    vFragPos=vec3(worldPos);
    vNormal=normalize(uNormalMatrix*aNormal);
    vUV=aUV;
    vFragPosLightSpace=uLightSpaceMatrix*worldPos;// 供片元做阴影比较
    
    gl_Position=uProjection*uView*worldPos;
}
