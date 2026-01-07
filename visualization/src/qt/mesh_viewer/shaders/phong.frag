#version  460
layout (location = 0) out lowp vec4 frag_color;
in highp vec4 position;
in highp vec3 normal;

uniform highp vec3 light_direction;
uniform highp vec3 camera_position;


#ifdef PER_FRAGMENT_COLORS 
in highp vec4 ambient_color;
in highp vec4 diffuse_color;
in highp vec4 specular_color;
#else
uniform highp vec4 ambient_color;
uniform highp vec4 diffuse_color;
uniform highp vec4 specular_color;
#endif

#ifdef PER_FRAGMENT_SPECULAR_ALPHA
in highp float specular_alpha;
#else
uniform highp float specular_alpha;
#endif


float dot(vec3 a,vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;

}

vec4 ambient() {
    return ambient_color;
}

vec4 diffuse(vec3 to_camera) {
    return diffuse_color * dot(light_direction, to_camera);
}

vec4 specular(vec3 to_camera) {
    vec3 reflect_dir = light_direction - 2. * normal.dot(light_direction) * normal;
    vec3 spec = dot(reflect_dir, to_camera);
    return specular_color * pow(spec,alpha);
}

void main() {
    //   frag_color = vec4(value,1.0);
    frag_color = rgba;
    frag_color = col;

    vec3 to_camera = 
        normalize((camera_position.xyz * position.w -
        position.xyz * camera_position.w) );

    frag_color = ambient() + diffuse(to_camera) + specular(to_camera);
}
