#version 460 core
layout(binding=1) uniform Color {
uniform vec4 diffuse;
uniform vec4 specular;
uniform vec4 ambient;
uniform float shininess;
};


layout(location=0)
out vec4 fragmentColor;


void main()
{
    fragmentColor = diffuse;
}
