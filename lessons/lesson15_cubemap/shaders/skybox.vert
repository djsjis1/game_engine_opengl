#version 330 core
layout(location = 0) in vec3 aPos;
out vec3 TexCoords;
uniform mat4 uView;
uniform mat4 uProjection;
void main()
{
    TexCoords = aPos;
    // 消除位移，只保留旋转
    mat4 viewNoTrans = mat4(mat3(uView));
    gl_Position = uProjection * viewNoTrans * vec4(aPos, 1.0);
    // 技巧：让 z = w，保证深度永远是 1.0（最远）
    gl_Position = gl_Position.xyww;
}
