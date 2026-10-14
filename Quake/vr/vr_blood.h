// GLSL shared by viewblend and warpscale. Clear centre, bounded combined alpha.
#pragma once
#define QVR_BLOOD_UNIFORMS \
"layout(location=2) uniform vec4 Blood; // hit floor, hit horizontal/vertical bias, health pulse\n" \
"layout(location=3) uniform vec4 BloodViewport; // origin and reciprocal size\n" \
"float QvrBloodAlpha()\n" \
"{\n" \
"    vec2 p = (gl_FragCoord.xy - BloodViewport.xy) * BloodViewport.zw * 2.0 - 1.0;\n" \
"    float edge = smoothstep(0.36, 1.35, dot(p, p));\n" \
"    vec2 direction = p / max(length(p), 0.001);\n" \
"    float hit = clamp(Blood.x + dot(Blood.yz, direction), 0.0, 0.8);\n" \
"    return edge * min(0.8, 1.0 - (1.0 - hit) * (1.0 - Blood.w));\n" \
"}\n"
