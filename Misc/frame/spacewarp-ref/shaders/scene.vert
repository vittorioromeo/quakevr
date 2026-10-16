#version 450
#extension GL_GOOGLE_include_directive : require
#include "common.glsl"
layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUv;
layout(location = 3) in vec3 inColor;
layout(location = 0) out vec3 vNormal;
layout(location = 1) out vec2 vUv;
layout(location = 2) out vec3 vColor;
void main()
{
    Draw d = fr.draws[pc.drawIndex];
    gl_Position = d.mvp * vec4(inPos, 1.0);
    vNormal = mat3(d.model) * inNormal;
    vUv = inUv * d.params.x;
    vColor = inColor;
}
