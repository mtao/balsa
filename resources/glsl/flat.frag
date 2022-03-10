#version 450 core

uniform lowp vec4 color = vec4(1.0);
layout(location = 0) 
//layout(location = COLOR_OUTPUT_ATTRIBUTE_LOCATION) 
out lowp vec4 fragmentColor;


void main()
{
    fragmentColor = color;
}
