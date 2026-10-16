#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inUv;

layout(push_constant) uniform DrawParams
{
    vec4 transform; // center.xy, half-size.xy in NDC
    vec4 payload;
} params;

layout(location = 0) out vec2 uv;

void main()
{
    gl_Position = vec4(inPosition.xy * params.transform.zw + params.transform.xy, inPosition.z, 1.0);
    uv = inUv;
}
