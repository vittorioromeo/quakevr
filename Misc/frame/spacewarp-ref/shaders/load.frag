#version 450
#extension GL_GOOGLE_include_directive : require
#include "common.glsl"
// Artificial GPU load: a data-dependent loop of texture fetches plus ALU whose
// trip count comes from a push constant. The result feeds the output colour, so
// nothing can be eliminated. Drawn before the scene as the sky/background.
layout(location = 0) in vec2 vNdc;
layout(location = 0) out vec4 outColor;
void main()
{
    vec4 acc = vec4(gl_FragCoord.xy * (1.0 / 512.0), 0.37, 0.71);
    for(uint i = 0u; i < pc.iters; ++i)
    {
        vec4 t = textureLod(noiseTex, acc.xy + acc.zw * 0.5, 0.0);
        acc = fract(acc * 1.0137 + t.yzwx * 0.731 + vec4(0.123, 0.456, 0.789, 0.321));
        acc.xy = mat2(0.8, -0.6, 0.6, 0.8) * acc.xy + acc.wz * 0.01;
    }
    vec3 sky = mix(vec3(0.20, 0.30, 0.45), vec3(0.05, 0.08, 0.15), vNdc.y * 0.5 + 0.5);
    outColor = vec4(sky + acc.rgb * 0.003, 1.0);
}
