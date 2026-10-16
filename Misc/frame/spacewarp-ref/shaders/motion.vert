#version 450
#extension GL_GOOGLE_include_directive : require
#include "common.glsl"
layout(location = 0) in vec3 inPos;
layout(location = 0) out vec4 vCur;
layout(location = 1) out vec4 vPrev;
void main()
{
    Draw d = fr.draws[pc.drawIndex];
    vec4 cur = d.mvp * vec4(inPos, 1.0);
    gl_Position = cur;
    vCur = cur;
    vPrev = d.prevMvp * vec4(inPos, 1.0);
}
