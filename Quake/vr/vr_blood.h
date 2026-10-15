// GLSL shared by viewblend and warpscale. Clear centre, bounded combined alpha.
#pragma once
#define QVR_BLOOD_UNIFORMS \
"layout(location=2) uniform vec4 Blood; // hit floor, hit horizontal/vertical bias, health pulse\n" \
"layout(location=3) uniform vec4 BloodViewport; // origin and reciprocal size\n" \
"layout(location=4) uniform vec4 BloodSize; // independent hit and health coverage, 0..1\n" \
"float QvrBloodEdge(vec2 p, float size)\n" \
"{\n" \
"    if (size <= 0.0) return 0.0;\n" \
"    float inner = mix(0.9, 0.15, size);\n" \
"    float outer = mix(1.4, 0.55, size);\n" \
"    return smoothstep(inner * inner, outer * outer, dot(p, p));\n" \
"}\n" \
"float QvrBloodAlpha()\n" \
"{\n" \
"    vec2 p = (gl_FragCoord.xy - BloodViewport.xy) * BloodViewport.zw * 2.0 - 1.0;\n" \
"    vec2 direction = p / max(length(p), 0.001);\n" \
"    float hit = clamp(Blood.x + dot(Blood.yz, direction), 0.0, 0.8);\n" \
"    hit *= QvrBloodEdge(p, BloodSize.x);\n" \
"    float health = Blood.w * QvrBloodEdge(p, BloodSize.y);\n" \
"    return min(0.8, 1.0 - (1.0 - hit) * (1.0 - health));\n" \
"}\n"
