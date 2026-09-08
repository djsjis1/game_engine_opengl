#version 330 core
in vec3 FragPos;
in vec3 Normal;
out vec4 FragColor;
uniform samplerCube uSkybox;
uniform vec3 uViewPos;
uniform float uReflect; // 1=全反射, 0=全折射
void main()
{
    vec3 I = normalize(FragPos - uViewPos);
    vec3 R;
    if (uReflect > 0.5)
        R = reflect(I, normalize(Normal));       // 反射
    else
        R = refract(I, normalize(Normal), 0.67); // 折射（玻璃 ≈ 1/1.52）
    FragColor = texture(uSkybox, R);
}
