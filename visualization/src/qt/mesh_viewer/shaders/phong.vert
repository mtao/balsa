#version  460
in highp vec3 posAttr;
//attribute lowp vec4 colAttr;
out lowp vec4 col;
uniform highp mat4 MVP;
void main() {
    //   col = colAttr;
    //   col = MVP * vec4(posAttr,1.0);
    //   gl_Position = MVP * vec4(posAttr + vec3(r,g,b),1.0);
    gl_Position = MVP * vec4(posAttr,1.0);
    col = vec4(posAttr,1.0);
    gl_PointSize = 50.f;
    //   gl_Position = matrix * posAttr;
}
