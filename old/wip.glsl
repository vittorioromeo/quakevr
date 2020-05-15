#version 430 core

layout(location = 0) in vec3  pOrg;
layout(location = 1) in float pAngle;
layout(location = 2) in float pScale;
layout(location = 3) in vec4  pColor;
layout(location = 4) in int   pAtlasIdx;

out VS_OUT {
    float opAngle;
    float opScale;
    vec4  opColor;
    int   opAtlasIdx;
} vs_out;

void main()
{
    gl_Position = vec4(pOrg, 1.0);
    vs_out.opAngle = pAngle;
    vs_out.opScale = pScale;
    vs_out.opColor = pColor;
    vs_out.opAtlasIdx = pAtlasIdx;
}
