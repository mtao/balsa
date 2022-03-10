#version 450 core

layout(location = 0)
#if defined(TWO_DIMENSIONS)
uniform mat3 transformationProjectionMatrix;
#elif defined(THREE_DIMENSIONS)
uniform mat4 transformationProjectionMatrix;
#else
#error
#endif


void main()
{
#if defined(TWO_DIMENSIONS)
    gl_Position.xywz = vec4(transformationProjectionMatrix * vec3(position, 1.0), 0.0);
#elif defined(THREE_DIMENSIONS)
    gl_Position = transformationProjectionMatrix * vec4(position, 1.0);
#else
#error
#endif
}
