#version 450
#extension GL_GOOGLE_include_directive : require
#include "common.glsl"
layout(location = 0) in vec3 vNormal;
layout(location = 1) in vec2 vUv;
layout(location = 2) in vec3 vColor;
layout(location = 0) out vec4 outColor;
void main()
{
    vec3 n = normalize(vNormal);
    float light = 0.45 + 0.55 * max(dot(n, normalize(vec3(0.35, 0.85, 0.4))), 0.0);
    vec2 cell = floor(vUv);
    float checker = mod(cell.x + cell.y, 2.0) < 0.5 ? 1.0 : 0.5;
    vec2 g = abs(fract(vUv) - 0.5);
    float line = max(g.x, g.y) > 0.46 ? 0.3 : 1.0;
    float detail = texture(noiseTex, vUv * 0.5).r;
    outColor = vec4(vColor * light * checker * line * (0.7 + 0.6 * detail), 1.0);
}
