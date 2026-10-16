#version 450
#extension GL_GOOGLE_include_directive : require
#include "common.glsl"
// Clip positions are linear in object space, so perspective-correct
// interpolation yields the exact surface point under this pixel; dividing
// per fragment gives exact current and previous Vulkan NDC (x, y, depth 0..1).
layout(location = 0) in vec4 vCur;
layout(location = 1) in vec4 vPrev;
layout(location = 0) out vec4 outMotion;
void main()
{
    vec3 cur = vCur.xyz / vCur.w;
    vec3 prev = vPrev.xyz / vPrev.w;
    outMotion = encodeMotion(cur - prev);
}
