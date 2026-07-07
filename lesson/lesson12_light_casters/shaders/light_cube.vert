#version 330 core
layout(location = 0) in vec3 aPos;
uniform mat4 uModel, uView, uProjection;
void main() { gl_Position = uProjection * uView * uModel * vec4(aPos, 1.0); }
