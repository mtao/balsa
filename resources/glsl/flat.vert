#version 460 core

layout(location = 0)
#if defined(TWO_DIMENSIONS)
in vec2 position;
#elif defined(THREE_DIMENSIONS)
in vec3 position;
#else
#error
#endif

layout(binding = 0) uniform Transformation {
#if defined(TWO_DIMENSIONS)
mat3 transformationProjectionMatrix;
#elif defined(THREE_DIMENSIONS)
mat4 transformationProjectionMatrix;
#else
#error
#endif
};


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
