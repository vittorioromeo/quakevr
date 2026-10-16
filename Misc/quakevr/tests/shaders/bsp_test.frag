#version 450

layout(push_constant) uniform DrawParams
{
    vec4 transform;
    vec4 payload;
} params;

layout(location = 0) out vec4 outValue;

void main()
{
    outValue = params.payload;
}
