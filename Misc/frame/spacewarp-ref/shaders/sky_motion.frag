#version 450
#extension GL_GOOGLE_include_directive : require
#include "common.glsl"
// Background (points at infinity): only view rotation moves them. skyReproj maps
// the current pixel's ray into the previous frame's rotation-only clip space.
layout(location = 0) in vec2 vNdc;
layout(location = 0) out vec4 outMotion;
void main()
{
    vec4 p = fr.skyReproj[pc.drawIndex] * vec4(vNdc, 0.5, 1.0);
    vec2 prev = p.xy / p.w;
    outMotion = encodeMotion(vec3(vNdc - prev, 0.0));
}
