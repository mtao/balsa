#version 450

layout(location=0) in vec3 inCol;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(inCol, 1.0);
}
