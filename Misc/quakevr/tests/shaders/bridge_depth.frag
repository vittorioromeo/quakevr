#version 450
layout(set = 0, binding = 0) uniform sampler2D StagedDepth;
void main()
{
    ivec2 pixel = ivec2(gl_FragCoord.xy);
    pixel.y = textureSize(StagedDepth, 0).y - 1 - pixel.y;
    gl_FragDepth = texelFetch(StagedDepth, pixel, 0).r;
}
