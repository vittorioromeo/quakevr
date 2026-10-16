// Shared declarations (included by every shader via GL_GOOGLE_include_directive).
// Must match GpuFrame / PushConstants in src/main.cpp (std140, 16-byte aligned).
struct Draw
{
    mat4 mvp;      // current: proj * view * appFromWorld * model
    mat4 prevMvp;  // previous frame: prevProj * prevView * appFromWorld * prevModel
    mat4 model;    // world-from-object (for lighting only)
    vec4 params;   // x = uv tiles, y = object id
};
layout(set = 0, binding = 0, std140) uniform FrameBlock
{
    mat4 skyReproj[2]; // per eye: prevVP_rotationOnly * inverse(curVP_rotationOnly)
    vec4 eyeParams[2]; // reserved
    Draw draws[8];     // eye * 4 + object
} fr;
layout(set = 0, binding = 1) uniform sampler2D noiseTex;
layout(push_constant) uniform PushBlock
{
    uint drawIndex;  // index into fr.draws (or eye index for fullscreen passes)
    uint iters;      // load-shader loop iterations for this draw
    float gain;      // motion vector gain (1 = exact)
    uint flags;      // bit0: 2D motion (z = 0), bit1: clamp encoded to [0,1] (UNORM/sRGB storage)
    vec4 encScale;   // stored = (motion - encOffset) / encScale; runtime decodes rgb * scale + offset
    vec4 encOffset;
} pc;

vec4 encodeMotion(vec3 mv)
{
    mv *= pc.gain;
    if((pc.flags & 1u) != 0u) mv.z = 0.0;
    vec3 e = (mv - pc.encOffset.xyz) / pc.encScale.xyz;
    if((pc.flags & 2u) != 0u) e = clamp(e, vec3(0.0), vec3(1.0));
    return vec4(e, 1.0);
}
