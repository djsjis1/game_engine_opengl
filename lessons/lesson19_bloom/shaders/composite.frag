#version 330 core
in vec2 TexCoord;
out vec4 FragColor;
uniform sampler2D uScene;
uniform sampler2D uBloom;
uniform float uIntensity; // bloom 强度
void main()
{
    vec3 scene = texture(uScene, TexCoord).rgb;
    vec3 bloom = texture(uBloom, TexCoord).rgb;
    // HDR+Bloom: 场景 + 泛光 → tonemapping
    vec3 hdr = scene + bloom * uIntensity;
    // Reinhard 色调映射
    vec3 mapped = hdr / (1.0 + hdr);
    // Gamma 校正
    mapped = pow(mapped, vec3(1.0 / 2.2));
    FragColor = vec4(mapped, 1.0);
}
