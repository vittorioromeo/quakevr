// Temporal anti-aliasing in the eyes (docs/vr-port/TEMPORAL.md, stages 2a and 2b).
//
// Motion vectors (uv one frame ago minus uv now, from unjittered view-projections):
// - the camera's: each pixel's point from the depth and the eye's inverse view-projection, projected by the eye's
//   view-projection one frame ago (the resolve's compute pass, below);
// - the models' and the moving brush models' own: their shading writes a motion output (the motion target, colour
//   attachment 1 of the eye's scene framebuffers, RGBA32F: xy the motion, z the fragment's depth, w the eye's stamp)
//   while VR_TemporalMotionWrites has it on. Alias models' vertex shader keeps each vertex's world position in a slot of
//   a buffer (two halves, by frame), so the previous frame's position is exactly what was drawn, whatever the vertex
//   path (smoothing, rebuilt swings, dents, the recoil's zero blend); brush models carry their previous matrix. The
//   hands, weapons and body are ordinary alias models in world space, and so is the eye's view-projection: a hand held
//   still in front of the eyes keeps its place in both frames, so head motion does not smear it. A motion texel is
//   used only where its stamp is this eye's and its depth the final depth (nothing opaque drawn over it later).
// - the reactive mask: OIT's stencil bit 2, and bit 4 for the VR particles and the heat haze (VR_TemporalReactive);
//   liquids' motion texels carry a flag (their animated warp): the resolve trusts the current frame more there.
//
// The resolve (VR_TemporalResolve, after the scene, before the water warp, the view blend, bloom and the tone curve):
// one compute pass per eye: the velocity of the nearest pixel of the 3x3 (Karis 2014), the history at uv + motion
// (Catmull-Rom), rejected off screen or where the previous frame's depth there is nearer than this pixel was expected
// (disocclusion), clipped to the 3x3's variance box in YCoCg (Salvi), blended in a reversible tone curve's space
// (Karis' luminance weighting). Then a fullscreen pass copies it into the scene with the optional sharpening
// (vr_taa_sharpen). The jitter (Halton 2,3, 8 phases, the same in both eyes) moves only the scene's projection: the
// layers submitted to the runtime keep the unjittered pose and field of view.
//
// Debug views (vr_temporal_debug): 1 the vectors, 2 the mask and the rejected pixels, 3 the reprojection test (the
// previous frame through the vectors); vr_temporal_error prints the reprojection's error.

#include "vr_temporal.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_profile.hpp"
#include "vr_stereo.hpp"

#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/String/String.hpp"

#include <cstdint>
#include <cstring>

#ifndef GL_COMPUTE_SHADER
#define GL_COMPUTE_SHADER 0x91B9
#endif
#ifndef GL_DEPTH_STENCIL_TEXTURE_MODE
#define GL_DEPTH_STENCIL_TEXTURE_MODE 0x90EA
#endif
#ifndef GL_STENCIL_INDEX
#define GL_STENCIL_INDEX 0x1901
#endif

namespace qvr::temporal
{
namespace
{

// ---------------------------------------------------------------------------------------------------------------------
// Shaders

constexpr const char* resolveCs = R"(#version 430
layout(local_size_x = 16, local_size_y = 16) in;
layout(binding = 0) uniform sampler2D SceneColor;
layout(binding = 1) uniform sampler2D SceneDepth;
layout(binding = 2) uniform sampler2D Motion;    // xy the motion, z the depth it was written at, w the stamp (+ 0.5: a liquid)
layout(binding = 3) uniform sampler2D Mask;      // the reactive mask (stencil bits 2, 4)
layout(binding = 4) uniform sampler2D History;   // the previous frame's result (linear filtered)
layout(binding = 5) uniform sampler2D PrevDepth; // the previous frame's distances along the view
layout(binding = 0, rgba16f) uniform writeonly image2D HistoryOut;
layout(binding = 1, r32f) uniform writeonly image2D DepthOut;
layout(binding = 2, rgba16f) uniform writeonly image2D Display;
layout(std430, binding = 15) buffer StatsBuffer { uint Stats[]; };
layout(location = 0) uniform mat4 InvViewProj; // this frame's, unjittered
layout(location = 1) uniform mat4 PrevViewProj; // the previous frame's, unjittered
layout(location = 2) uniform vec4 Jitter; // xy the jitter in uv, z the stamp, w 1: reversed Z (the depth is clip z)
layout(location = 3) uniform vec4 Params; // x the current frame's weight, y the clip's gamma, z the reactive pixels' weight, w 1: no history
layout(location = 4) uniform int Mode; // 0 TAA, 1 vectors, 2 mask, 3 reprojection test
layout(location = 5) uniform int Eye;
shared uint sErr, sIdent, sCount;

bool Reversed() { return Jitter.w > 0.5; }
bool Hidden(float d) { return Reversed() ? d >= 1.0 : d <= 0.0; } // the lenses' hidden area, at the near plane
bool Nearer(float a, float b) { return Reversed() ? a > b : a < b; }
float MaxOf(vec3 c) { return max(c.r, max(c.g, c.b)); }
vec3 Tonemap(vec3 c) { return c / (1.0 + MaxOf(c)); } // reversible (Karis)
vec3 Untonemap(vec3 c) { return c / max(1.0 - MaxOf(c), 1e-4); }
float Luma(vec3 c) { return dot(c, vec3(0.25, 0.5, 0.25)); }
vec3 ToYCoCg(vec3 c) { return vec3(dot(c, vec3(0.25, 0.5, 0.25)), dot(c, vec3(0.5, 0.0, -0.5)), dot(c, vec3(-0.25, 0.5, -0.25))); }
vec3 FromYCoCg(vec3 c) { return vec3(c.x + c.y - c.z, c.x + c.z, c.x - c.y - c.z); }

// Pixel q's motion (uv one frame ago minus uv now) at depth d; obj 0 the camera's, 1 its own, 2 a liquid's; w its
// distance along the view now, pw the distance expected one frame ago.
vec2 MotionAt(ivec2 q, float d, vec2 texel, out float obj, out float w, out float pw)
{
    vec2 uv = (vec2(q) + 0.5) * texel - Jitter.xy;
    vec4 h = InvViewProj * vec4(uv * 2.0 - 1.0, Reversed() ? d : d * 2.0 - 1.0, 1.0);
    w = 1.0 / h.w;
    vec4 c = PrevViewProj * vec4(h.xyz / h.w, 1.0);
    pw = c.w;
    vec2 mv = c.xy / c.w * 0.5 + 0.5 - uv;
    obj = 0.0;
    vec4 m = texelFetch(Motion, q, 0);
    if (abs(floor(m.w) - Jitter.z) < 0.5 && abs(m.z - d) <= max(abs(d) * 2e-5, 1.5e-7))
    {
        mv = m.xy;
        obj = fract(m.w) > 0.25 ? 2.0 : 1.0;
        pw = w;
    }
    return mv;
}

vec4 SampleCatmullRom(vec2 uv, vec2 size)
{
    vec2 pos = uv * size;
    vec2 t1 = floor(pos - 0.5) + 0.5;
    vec2 f = pos - t1;
    vec2 w0 = f * (-0.5 + f * (1.0 - 0.5 * f));
    vec2 w1 = 1.0 + f * f * (-2.5 + 1.5 * f);
    vec2 w2 = f * (0.5 + f * (2.0 - 1.5 * f));
    vec2 w3 = f * f * (-0.5 + 0.5 * f);
    vec2 w12 = w1 + w2;
    vec2 t0 = (t1 - 1.0) / size, t3 = (t1 + 2.0) / size, t12 = (t1 + w2 / w12) / size;
    vec4 r = textureLod(History, vec2(t12.x, t0.y), 0.0) * (w12.x * w0.y);
    r += textureLod(History, vec2(t0.x, t12.y), 0.0) * (w0.x * w12.y);
    r += textureLod(History, t12, 0.0) * (w12.x * w12.y);
    r += textureLod(History, vec2(t3.x, t12.y), 0.0) * (w3.x * w12.y);
    r += textureLod(History, vec2(t12.x, t3.y), 0.0) * (w12.x * w3.y);
    float total = w12.x * w0.y + w0.x * w12.y + w12.x * w12.y + w3.x * w12.y + w12.x * w3.y;
    return max(r / total, vec4(0.0));
}

// q clipped towards the box's centre (Playdead's INSIDE)
vec3 ClipAABB(vec3 lo, vec3 hi, vec3 q)
{
    vec3 c = 0.5 * (hi + lo), e = 0.5 * (hi - lo) + 1e-5;
    vec3 v = q - c;
    vec3 a = abs(v / e);
    float m = max(a.x, max(a.y, a.z));
    return m > 1.0 ? c + v / m : q;
}

void Resolve(ivec2 p, ivec2 size, vec2 texel)
{
    vec4 cur = texelFetch(SceneColor, p, 0);
    float d = texelFetch(SceneDepth, p, 0).r;
    if (Hidden(d))
    {
        imageStore(HistoryOut, p, vec4(0.0, 0.0, 0.0, cur.a));
        imageStore(DepthOut, p, vec4(0.0));
        if (Mode != 0)
            imageStore(Display, p, cur);
        return;
    }

    // The 3x3: the colours' moments (tone mapped, YCoCg) and the nearest pixel
    vec3 m1 = vec3(0.0), m2 = vec3(0.0);
    ivec2 nearest = p;
    float nd = d;
    for (int y = -1; y <= 1; y++)
        for (int x = -1; x <= 1; x++)
        {
            ivec2 q = clamp(p + ivec2(x, y), ivec2(0), size - 1);
            vec3 c = ToYCoCg(Tonemap(texelFetch(SceneColor, q, 0).rgb));
            m1 += c;
            m2 += c * c;
            float qd = texelFetch(SceneDepth, q, 0).r;
            if (!Hidden(qd) && Nearer(qd, nd))
            {
                nd = qd;
                nearest = q;
            }
        }

    float obj, w, pw, nobj, nw, npw;
    vec2 mv = MotionAt(p, d, texel, obj, w, pw);
    vec2 dmv = nearest == p ? mv : MotionAt(nearest, nd, texel, nobj, nw, npw);
    vec2 uv = (vec2(p) + 0.5) * texel;
    vec2 huv = uv + dmv; // the history's (the nearest pixel's motion: silhouettes carry theirs)
    vec2 ouv = uv + mv;  // this pixel's own
    bool rejected = Params.w > 0.5 || any(lessThan(ouv, vec2(0.0))) || any(greaterThan(ouv, vec2(1.0)));
    if (!rejected) // the surface not in view there one frame ago: the previous depth (the farthest of the 2x2) nearer
    {
        ivec2 b = ivec2(floor(ouv * vec2(size) - 0.5));
        float far = 0.0;
        for (int i = 0; i < 4; i++)
            far = max(far, texelFetch(PrevDepth, clamp(b + ivec2(i & 1, i >> 1), ivec2(0), size - 1), 0).r);
        rejected = far < pw * (obj > 0.5 ? 0.9 : 0.97) - 1.0;
    }
    float mask = texelFetch(Mask, p, 0).r;
    float reactive = max(mask, obj > 1.5 ? 0.5 : 0.0);

    if (Mode == 0)
    {
        vec3 now = Tonemap(cur.rgb);
        vec3 hist = Tonemap(SampleCatmullRom(huv, vec2(size)).rgb);
        vec3 mean = m1 / 9.0;
        vec3 sigma = sqrt(max(m2 / 9.0 - mean * mean, vec3(0.0)));
        hist = FromYCoCg(ClipAABB(mean - Params.y * sigma, mean + Params.y * sigma, ToYCoCg(hist)));
        float a = rejected ? 1.0 : max(Params.x, Params.z * reactive);
        vec3 res = Untonemap(clamp(mix(hist, now, a), vec3(0.0), vec3(0.9999)));
        if (any(isnan(res)) || any(isinf(res)))
            res = cur.rgb;
        imageStore(HistoryOut, p, vec4(res, cur.a));
    }
    else
    {
        imageStore(HistoryOut, p, cur); // the frame as it is: the reprojection test's next previous frame
        vec4 show = cur;
        float grey = Luma(Tonemap(cur.rgb)) * 0.5;
        if (Mode == 1) // red/green: x/y motion (0.05 per pixel), blue: a model's own, half blue: a liquid's
        {
            vec2 px = mv * vec2(size);
            show = vec4(clamp(vec3(0.5 + px.x * 0.05, 0.5 + px.y * 0.05, obj > 1.5 ? 0.5 : obj), 0.0, 1.0), 1.0);
        }
        else if (Mode == 2) // red: reactive (translucency, particles), green: rejected (disoccluded), blue: liquid
            show = vec4(max(vec3(mask, rejected ? 1.0 : 0.0, obj > 1.5 ? 1.0 : 0.0), vec3(grey)), 1.0);
        else if (Mode == 3) // the previous frame through the vectors (rejected pixels tinted red)
        {
            vec4 prev = textureLod(History, ouv, 0.0);
            show = rejected ? mix(prev, vec4(1.0, 0.0, 0.0, 1.0), 0.35) : prev;
            if (!rejected && reactive == 0.0)
            {
                float e = abs(Luma(Tonemap(prev.rgb)) - Luma(Tonemap(cur.rgb)));
                float s = abs(Luma(Tonemap(texelFetch(History, p, 0).rgb)) - Luma(Tonemap(cur.rgb)));
                atomicAdd(sErr, uint(e * 1020.0 + 0.5)); // (0-255, times 4)
                atomicAdd(sIdent, uint(s * 1020.0 + 0.5));
                atomicAdd(sCount, 1u);
            }
        }
        imageStore(Display, p, show);
    }
    imageStore(DepthOut, p, vec4(w));
}

void main()
{
    ivec2 p = ivec2(gl_GlobalInvocationID.xy);
    ivec2 size = textureSize(SceneColor, 0);
    if (gl_LocalInvocationIndex == 0u)
    {
        sErr = 0u;
        sIdent = 0u;
        sCount = 0u;
    }
    barrier();
    if (all(lessThan(p, size)))
        Resolve(p, size, 1.0 / vec2(size));
    barrier();
    if (gl_LocalInvocationIndex == 0u && sCount > 0u)
    {
        atomicAdd(Stats[Eye * 4 + 0], sErr);
        atomicAdd(Stats[Eye * 4 + 1], sIdent);
        atomicAdd(Stats[Eye * 4 + 2], sCount);
    }
}
)";

// The reactive mask from the stencil (the depth/stencil texture read as its stencil).
constexpr const char* maskCs = R"(#version 430
layout(local_size_x = 16, local_size_y = 16) in;
layout(binding = 0) uniform usampler2D Stencil;
layout(binding = 3, r8) uniform writeonly image2D MaskOut;
void main()
{
    ivec2 p = ivec2(gl_GlobalInvocationID.xy);
    if (any(greaterThanEqual(p, textureSize(Stencil, 0))))
        return;
    uint s = texelFetch(Stencil, p, 0).r;
    imageStore(MaskOut, p, vec4((s & 6u) != 0u ? 1.0 : 0.0));
}
)";

constexpr const char* applyVs = R"(#version 430
void main()
{
    ivec2 v = ivec2(gl_VertexID & 1, gl_VertexID >> 1);
    gl_Position = vec4(vec2(v) * 4.0 - 1.0, 0.0, 1.0);
}
)";

// The result into the scene, sharpened (vr_taa_sharpen): an unsharp mask of the cross, limited to its range (no halos).
constexpr const char* applyFs = R"(#version 430
layout(binding = 0) uniform sampler2D Src;
layout(location = 0) uniform float Sharpen;
out vec4 result;
void main()
{
    ivec2 p = ivec2(gl_FragCoord.xy);
    ivec2 last = textureSize(Src, 0) - 1;
    vec4 c = texelFetch(Src, p, 0);
    result = c;
    if (Sharpen > 0.0)
    {
        vec3 n = texelFetch(Src, min(p + ivec2(0, 1), last), 0).rgb, s = texelFetch(Src, max(p - ivec2(0, 1), ivec2(0)), 0).rgb;
        vec3 e = texelFetch(Src, min(p + ivec2(1, 0), last), 0).rgb, w = texelFetch(Src, max(p - ivec2(1, 0), ivec2(0)), 0).rgb;
        vec3 lo = min(c.rgb, min(min(n, s), min(e, w))), hi = max(c.rgb, max(max(n, s), max(e, w)));
        result.rgb = clamp(c.rgb + (4.0 * c.rgb - n - s - e - w) * (0.25 * Sharpen), lo, hi);
    }
}
)";

// ---------------------------------------------------------------------------------------------------------------------
// State

// An entity's previous frame: an alias model's vertex slots, a brush model's matrix.
struct Slot
{
    const entity_t* ent{nullptr};
    const qmodel_t* model{nullptr};
    long frame{-1};
    int verts{0};
    int base{-1};     // this frame's slot (vertices), in this frame's half
    int prevBase{-1}; // the previous frame's, in the other half (-1 none)
    float mat[12]{};  // a brush model's matrix this frame (transposed 4x3, as its instance)
    float prev[12]{};
};

constexpr int slotTableSize = 4096; // a power of two
constexpr int vertsPerHalf = 1 << 20;

struct EyeState
{
    GLuint history[2]{0, 0}; // RGBA16F, ping-pong
    GLuint depth[2]{0, 0};   // R32F, the distances along the view
    int cur{0};
    bool valid{false};      // history[cur ^ 1] holds the previous frame
    long frame{-1};         // the frame it was last resolved
    float viewProj[16]{};   // this frame's, unjittered
    float prevViewProj[16]{};
    bool havePrev{false};
    vec3_t origin{}, angles{}, prevOrigin{}, prevAngles{};
};

struct State
{
    bool frameActive{false};   // the eyes collect this frame
    bool eyeActive{false};     // between beginEye and endEye, collecting
    bool resolvedEye{false};
    int eye{0};
    long frameIndex{0};
    int width{0}, height{0};
    float jitter[2]{};         // this frame's, in pixels
    float stamp{0.f};
    GLuint sceneFbo{0}, compositeFbo{0}; // the eye targets' framebuffers, with the motion target attached
    GLuint attachedScene{0}, attachedComposite{0}, attachedTex{0};
    GLuint motionTex{0};       // RGBA32F, the eye size (shared by the eyes)
    GLuint maskTex{0};         // R8
    GLuint displayTex{0};      // RGBA16F: the debug views
    int texWidth{0}, texHeight{0};
    EyeState eyes[2];
    GLuint posBuffer{0};       // the alias models' vertices, two halves
    int posUsed{0};
    GLuint statsBuffer{0};
    GLuint zeroUbo{0};
    GLuint resolveProgram{0}, maskProgram{0}, applyProgram{0};
    bool failed{false};
    bool taaWas{false};
    int debugWas{0};
    const qmodel_t* worldmodel{nullptr};
    za::Vector<Slot> slots;
    bool writes{false};
    bool reactive{false};
};
State state;

struct TemporalBlock // std140: QVR_TEMPORAL_BLOCK (vr_glsl.h)
{
    float prevViewProj[16];
    float jitter[4]; // xy the jitter in uv, z the stamp, w 1 while motion vectors are written
    float size[4];   // xy 1 / the eye's size
};

using GetBufferSubDataFn = void(APIENTRY*)(GLenum, GLintptr, GLsizeiptr, void*);
using ClearTexImageFn = void(APIENTRY*)(GLuint, GLint, GLenum, GLenum, const void*);
GetBufferSubDataFn getBufferSubData = nullptr;
ClearTexImageFn clearTexImage = nullptr;

// ---------------------------------------------------------------------------------------------------------------------
// Helpers

void mul4(const float* a, const float* b, float* out) // out = a * b, column-major (MatrixMultiply's)
{
    for(int c = 0; c < 4; c++)
    {
        for(int r = 0; r < 4; r++)
        {
            out[c * 4 + r] = a[0 * 4 + r] * b[c * 4 + 0] + a[1 * 4 + r] * b[c * 4 + 1] + a[2 * 4 + r] * b[c * 4 + 2] +
                             a[3 * 4 + r] * b[c * 4 + 3];
        }
    }
}

bool invert4(const float* m, float* out)
{
    float inv[16];
    inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] + m[9] * m[7] * m[14] + m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
    inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] - m[8] * m[7] * m[14] - m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
    inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] + m[8] * m[7] * m[13] + m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
    inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] - m[8] * m[6] * m[13] - m[12] * m[5] * m[10] + m[12] * m[6] * m[9];
    inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] - m[9] * m[3] * m[14] - m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
    inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] + m[8] * m[3] * m[14] + m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
    inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] - m[8] * m[3] * m[13] - m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
    inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] + m[8] * m[2] * m[13] + m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
    inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] + m[5] * m[3] * m[14] + m[13] * m[2] * m[7] - m[13] * m[3] * m[6];
    inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] - m[4] * m[3] * m[14] - m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
    inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] + m[4] * m[3] * m[13] + m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
    inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] - m[4] * m[2] * m[13] - m[12] * m[1] * m[6] + m[12] * m[2] * m[5];
    inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] - m[5] * m[3] * m[10] - m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
    inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] + m[4] * m[3] * m[10] + m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
    inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] - m[4] * m[3] * m[9] - m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
    inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] + m[4] * m[2] * m[9] + m[8] * m[1] * m[6] - m[8] * m[2] * m[5];
    const double det = static_cast<double>(m[0]) * inv[0] + static_cast<double>(m[1]) * inv[4] +
                       static_cast<double>(m[2]) * inv[8] + static_cast<double>(m[3]) * inv[12];
    if(det == 0.0)
    {
        return false;
    }
    const float r = static_cast<float>(1.0 / det);
    for(int i = 0; i < 16; i++)
    {
        out[i] = inv[i] * r;
    }
    return true;
}

[[nodiscard]] float halton(int index, int base)
{
    float f = 1.f, r = 0.f;
    while(index > 0)
    {
        f /= static_cast<float>(base);
        r += f * static_cast<float>(index % base);
        index /= base;
    }
    return r;
}

[[nodiscard]] bool taaOn()
{
    return vr_taa.value != 0.f && vr_temporal_debug.value == 0.f;
}

[[nodiscard]] GLuint compile(GLenum type, const char* source, const char* name)
{
    const GLuint shader = GL_CreateShaderFunc(type);
    GL_ShaderSourceFunc(shader, 1, &source, nullptr);
    GL_CompileShaderFunc(shader);
    GLint ok = 0;
    GL_GetShaderivFunc(shader, GL_COMPILE_STATUS, &ok);
    if(!ok)
    {
        char log[2048];
        GL_GetShaderInfoLogFunc(shader, sizeof(log), nullptr, log);
        Con_Warning("VR: %s shader failed to compile:\n%s\n", name, log);
        GL_DeleteShaderFunc(shader);
        return 0;
    }
    return shader;
}

[[nodiscard]] GLuint program(const char* vs, GLenum type, const char* source, const char* name)
{
    const GLuint v = vs ? compile(GL_VERTEX_SHADER, vs, name) : 0;
    const GLuint s = compile(type, source, name);
    if(!s || (vs && !v))
    {
        return 0;
    }
    const GLuint p = GL_CreateProgramFunc();
    if(v)
    {
        GL_AttachShaderFunc(p, v);
    }
    GL_AttachShaderFunc(p, s);
    GL_LinkProgramFunc(p);
    if(v)
    {
        GL_DeleteShaderFunc(v);
    }
    GL_DeleteShaderFunc(s);
    GLint ok = 0;
    GL_GetProgramivFunc(p, GL_LINK_STATUS, &ok);
    if(!ok)
    {
        char log[2048];
        GL_GetProgramInfoLogFunc(p, sizeof(log), nullptr, log);
        Con_Warning("VR: %s shader failed to link:\n%s\n", name, log);
        GL_DeleteProgramFunc(p);
        return 0;
    }
    GL_ObjectLabelFunc(GL_PROGRAM, p, -1, name);
    return p;
}

[[nodiscard]] bool ensurePrograms()
{
    if(state.failed)
    {
        return false;
    }
    if(!state.resolveProgram)
    {
        state.resolveProgram = program(nullptr, GL_COMPUTE_SHADER, resolveCs, "vr taa resolve");
        state.maskProgram = program(nullptr, GL_COMPUTE_SHADER, maskCs, "vr taa mask");
        state.applyProgram = program(applyVs, GL_FRAGMENT_SHADER, applyFs, "vr taa apply");
        getBufferSubData = reinterpret_cast<GetBufferSubDataFn>(SDL_GL_GetProcAddress("glGetBufferSubData"));
        clearTexImage = reinterpret_cast<ClearTexImageFn>(SDL_GL_GetProcAddress("glClearTexImage"));
        state.failed = !state.resolveProgram || !state.maskProgram || !state.applyProgram;
    }
    return !state.failed;
}

GLuint makeTexture(GLenum format, int width, int height, GLint filter, const char* label)
{
    GLuint tex = 0;
    glGenTextures(1, &tex);
    GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D, tex);
    GL_TexStorage2DFunc(GL_TEXTURE_2D, 1, format, width, height);
    GL_ObjectLabelFunc(GL_TEXTURE, tex, -1, label); // (vr_vram_report)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D, 0);
    return tex;
}

void deleteTexture(GLuint& tex)
{
    if(tex)
    {
        GL_DeleteNativeTexture(tex);
        tex = 0;
    }
}

void ensureTextures(int width, int height)
{
    if(state.motionTex && state.texWidth == width && state.texHeight == height)
    {
        return;
    }
    deleteTexture(state.motionTex);
    deleteTexture(state.maskTex);
    deleteTexture(state.displayTex);
    for(EyeState& e : state.eyes)
    {
        for(int i = 0; i < 2; i++)
        {
            deleteTexture(e.history[i]);
            deleteTexture(e.depth[i]);
        }
        e.valid = false;
    }
    state.motionTex = makeTexture(GL_RGBA32F, width, height, GL_NEAREST, "taa motion");
    state.maskTex = makeTexture(GL_R8, width, height, GL_NEAREST, "taa reactive mask");
    state.displayTex = makeTexture(GL_RGBA16F, width, height, GL_NEAREST, "taa debug view");
    for(EyeState& e : state.eyes)
    {
        for(int i = 0; i < 2; i++)
        {
            e.history[i] = makeTexture(GL_RGBA16F, width, height, GL_LINEAR, "taa history");
            e.depth[i] = makeTexture(GL_R32F, width, height, GL_NEAREST, "taa history depth");
        }
    }
    if(clearTexImage) // (no stale stamp can match)
    {
        const float none[4] = {0.f, 0.f, 0.f, -1.f};
        clearTexImage(state.motionTex, 0, GL_RGBA, GL_FLOAT, none);
    }
    state.texWidth = width;
    state.texHeight = height;
}

void ensureBuffers()
{
    if(!state.posBuffer)
    {
        GL_GenBuffersFunc(1, &state.posBuffer);
        GL_BindBuffer(GL_SHADER_STORAGE_BUFFER, state.posBuffer);
        GL_BufferDataFunc(GL_SHADER_STORAGE_BUFFER, static_cast<GLsizeiptr>(2) * vertsPerHalf * 16, nullptr, GL_DYNAMIC_DRAW);
        GL_ObjectLabelFunc(GL_BUFFER, state.posBuffer, -1, "taa model positions");
        GL_GenBuffersFunc(1, &state.statsBuffer);
        GL_BindBuffer(GL_SHADER_STORAGE_BUFFER, state.statsBuffer);
        const std::uint32_t zero[8] = {};
        GL_BufferDataFunc(GL_SHADER_STORAGE_BUFFER, sizeof(zero), zero, GL_DYNAMIC_READ);
        GL_BindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }
}

void bindZeroBlock()
{
    if(!state.zeroUbo)
    {
        const TemporalBlock zero{};
        GL_GenBuffersFunc(1, &state.zeroUbo);
        GL_BindBuffer(GL_UNIFORM_BUFFER, state.zeroUbo);
        GL_ObjectLabelFunc(GL_BUFFER, state.zeroUbo, -1, "taa zero ubo");
        GL_BufferDataFunc(GL_UNIFORM_BUFFER, sizeof(zero), &zero, GL_STATIC_DRAW);
        GL_BindBuffer(GL_UNIFORM_BUFFER, 0);
    }
    GL_BindBufferRange(GL_UNIFORM_BUFFER, 7, state.zeroUbo, 0, sizeof(TemporalBlock));
}

// The motion target as colour attachment 1 of the eye targets' framebuffers (none: detached).
void attach(GLuint tex)
{
    if(state.attachedScene == state.sceneFbo && state.attachedComposite == state.compositeFbo && state.attachedTex == tex)
    {
        return;
    }
    const GLuint bound = GL_BoundDrawFramebuffer();
    const auto set = [](GLuint fbo, GLuint t)
    {
        if(fbo)
        {
            GL_BindFramebufferFunc(GL_FRAMEBUFFER, fbo);
            GL_FramebufferTexture2DFunc(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, t, 0);
        }
    };
    if(state.attachedTex && (state.attachedScene != state.sceneFbo || state.attachedComposite != state.compositeFbo))
    {
        set(state.attachedScene, 0); // (the old framebuffers: maybe deleted, then a no-op)
        set(state.attachedComposite, 0);
    }
    set(state.sceneFbo, tex);
    set(state.compositeFbo, tex);
    GL_BindFramebufferFunc(GL_FRAMEBUFFER, bound);
    state.attachedScene = state.sceneFbo;
    state.attachedComposite = state.compositeFbo;
    state.attachedTex = tex;
}

// The entity's slot this frame (shifting the last frame's into prev); nullptr if the table is full.
Slot* slotFor(const entity_t* e)
{
    if(state.slots.size() != slotTableSize)
    {
        state.slots.resize(slotTableSize);
    }
    const std::uintptr_t key = reinterpret_cast<std::uintptr_t>(e);
    int i = static_cast<int>(((key >> 4) * 2654435761u) & (slotTableSize - 1));
    Slot* reuse = nullptr;
    for(int n = 0; n < slotTableSize; n++, i = (i + 1) & (slotTableSize - 1))
    {
        Slot& s = state.slots[static_cast<za::SizeT>(i)];
        if(s.ent == e)
        {
            return &s;
        }
        if(!s.ent)
        {
            break;
        }
        if(!reuse && s.frame < state.frameIndex - 1)
        {
            reuse = &s;
        }
    }
    if(!reuse)
    {
        reuse = &state.slots[static_cast<za::SizeT>(i)];
        if(reuse->ent)
        {
            return nullptr;
        }
    }
    *reuse = Slot{};
    reuse->ent = e;
    return reuse;
}

} // namespace

// ---------------------------------------------------------------------------------------------------------------------

bool wanted()
{
    return (vr_taa.value != 0.f || vr_temporal_debug.value != 0.f) && vr_stereo_single.value == 0.f;
}

float eyeFsaa(float fsaa)
{
    return wanted() ? 0.f : fsaa;
}

void beginFrame(unsigned sceneFbo, unsigned compositeFbo, int width, int height, int samples)
{
    state.frameIndex++;
    state.sceneFbo = sceneFbo;
    state.compositeFbo = compositeFbo;
    const bool want = wanted() && samples <= 1 && ensurePrograms();
    if(!want)
    {
        if(state.frameActive || state.attachedTex)
        {
            attach(0);
        }
        state.frameActive = false;
        return;
    }
    if(cl.worldmodel != state.worldmodel) // a new map: no entity's previous frame, no history
    {
        state.worldmodel = cl.worldmodel;
        state.slots.clear();
        for(EyeState& e : state.eyes)
        {
            e.valid = false;
            e.havePrev = false;
        }
    }
    const bool taa = taaOn();
    if(taa != state.taaWas || static_cast<int>(vr_temporal_debug.value) != state.debugWas)
    {
        for(EyeState& e : state.eyes)
        {
            e.valid = false;
        }
        state.taaWas = taa;
        state.debugWas = static_cast<int>(vr_temporal_debug.value);
    }
    ensureTextures(width, height);
    ensureBuffers();
    attach(state.motionTex);
    state.width = width;
    state.height = height;
    state.frameActive = true;

    // Halton (2, 3), 8 phases, centred: the same offset in both eyes (TEMPORAL.md 6); only with TAA resolving
    const int phase = static_cast<int>(state.frameIndex % 8) + 1;
    state.jitter[0] = taa ? halton(phase, 2) - 0.5f : 0.f;
    state.jitter[1] = taa ? halton(phase, 3) - 0.5f : 0.f;

    // The models' positions: this frame's half written, the other read; last frame's writes visible
    GL_MemoryBarrierFunc(GL_SHADER_STORAGE_BARRIER_BIT);
    state.posUsed = 0;
    const GLsizeiptr half = static_cast<GLsizeiptr>(vertsPerHalf) * 16;
    const int cur = static_cast<int>(state.frameIndex & 1);
    GL_BindBufferRange(GL_SHADER_STORAGE_BUFFER, 13, state.posBuffer, cur * half, half);
    GL_BindBufferRange(GL_SHADER_STORAGE_BUFFER, 14, state.posBuffer, (cur ^ 1) * half, half);
    if(vr_temporal_debug.value == 3.f)
    {
        const std::uint32_t zero[8] = {};
        GL_BindBuffer(GL_SHADER_STORAGE_BUFFER, state.statsBuffer);
        GL_BufferSubDataFunc(GL_SHADER_STORAGE_BUFFER, 0, sizeof(zero), zero);
        GL_BindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }
}

void beginEye(int eye)
{
    state.eye = eye;
    state.eyeActive = state.frameActive;
    state.resolvedEye = false;
}

void endEye()
{
    state.eyeActive = false;
    state.writes = false;
    bindZeroBlock(); // the spectator camera, the window: no motion vectors
}

void registerCommands()
{
    Cmd_AddCommand("vr_temporal_error",
        []
        {
            if(!state.statsBuffer || !getBufferSubData)
            {
                Con_Printf("vr_temporal_error: no data (vr_temporal_debug 3 in the headset first)\n");
                return;
            }
            std::uint32_t stats[8] = {};
            GL_BindBuffer(GL_SHADER_STORAGE_BUFFER, state.statsBuffer);
            getBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(stats), stats);
            GL_BindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
            for(int eye = 0; eye < 2; eye++)
            {
                const std::uint32_t* s = stats + eye * 4;
                const double n = s[2] > 0 ? static_cast<double>(s[2]) : 1.0;
                Con_Printf("vr_temporal_error %s: reprojected %.3f, not reprojected %.3f (mean |luma| 0-255), %u pixels, "
                           "%.1f%% of the eye\n",
                    eye == 0 ? "L" : "R", s[0] / (4.0 * n), s[1] / (4.0 * n), s[2],
                    100.0 * s[2] / q_max(1.0, static_cast<double>(state.width) * state.height));
            }
        });
}

} // namespace qvr::temporal

using namespace qvr;
using temporal::state;

// R_SetFrustum: the eye's unjittered view-projection kept, the jitter added to its projection, and the motion
// vectors' block for the shaders (the previous frame's view-projection).
extern "C" void VR_TemporalProjection(float proj[16], const float view[16])
{
    if(!state.eyeActive || !stereo::isRenderingEye() || stereo::isSpectator())
    {
        temporal::bindZeroBlock();
        return;
    }
    temporal::EyeState& e = state.eyes[state.eye];
    temporal::mul4(proj, view, e.viewProj);
    VectorCopy(r_refdef.vieworg, e.origin);
    VectorCopy(r_refdef.viewangles, e.angles);

    const float dx = 2.f * state.jitter[0] / static_cast<float>(state.width);
    const float dy = 2.f * state.jitter[1] / static_cast<float>(state.height);
    for(int c = 0; c < 4; c++) // clip xy += offset * clip w
    {
        proj[c * 4 + 0] += dx * proj[c * 4 + 3];
        proj[c * 4 + 1] += dy * proj[c * 4 + 3];
    }

    state.stamp = static_cast<float>(((state.frameIndex * 2 + state.eye) & 0xFFFFF) + 1);
    temporal::TemporalBlock block{};
    std::memcpy(block.prevViewProj, e.havePrev ? e.prevViewProj : e.viewProj, sizeof(block.prevViewProj));
    block.jitter[0] = state.jitter[0] / static_cast<float>(state.width);
    block.jitter[1] = state.jitter[1] / static_cast<float>(state.height);
    block.jitter[2] = state.stamp;
    block.jitter[3] = 1.f;
    block.size[0] = 1.f / static_cast<float>(state.width);
    block.size[1] = 1.f / static_cast<float>(state.height);
    block.size[2] = static_cast<float>(state.width);
    block.size[3] = static_cast<float>(state.height);
    GLuint buf;
    GLbyte* ofs;
    GL_Upload(GL_UNIFORM_BUFFER, &block, sizeof(block), &buf, &ofs);
    GL_BindBufferRange(GL_UNIFORM_BUFFER, 7, buf, reinterpret_cast<GLintptr>(ofs), sizeof(block));
}

// An alias instance's vertex slots: x where its vertices are kept this frame, y where they were last frame (-1 none).
extern "C" void VR_TemporalAliasMotion(const entity_t* e, int totalverts, int standard, int out[4])
{
    out[0] = out[1] = -1;
    out[2] = out[3] = 0;
    if(!state.frameActive || !standard || !e || totalverts <= 0)
    {
        return;
    }
    temporal::Slot* s = temporal::slotFor(e);
    if(!s)
    {
        return;
    }
    if(s->frame != state.frameIndex)
    {
        const bool continues = s->frame == state.frameIndex - 1 && s->model == e->model && s->verts == totalverts && s->base >= 0;
        s->prevBase = continues ? s->base : -1;
        s->base = -1;
        if(state.posUsed + totalverts <= temporal::vertsPerHalf)
        {
            s->base = state.posUsed;
            state.posUsed += totalverts;
        }
        s->frame = state.frameIndex;
        s->model = e->model;
        s->verts = totalverts;
    }
    out[0] = s->base;
    out[1] = s->base >= 0 ? s->prevBase : -1;
}

// A brush instance's previous matrix (transposed 4x3, as `world`): last frame's, or `world` itself.
extern "C" void VR_TemporalBrushPrev(const entity_t* e, const float world[12], float prev[12])
{
    std::memcpy(prev, world, 12 * sizeof(float));
    if(!state.frameActive || !e || e == &cl_entities[0])
    {
        return;
    }
    temporal::Slot* s = temporal::slotFor(e);
    if(!s)
    {
        return;
    }
    if(s->frame != state.frameIndex)
    {
        const bool continues = s->frame == state.frameIndex - 1 && s->model == e->model;
        std::memcpy(s->prev, continues ? s->mat : world, sizeof(s->prev));
        std::memcpy(s->mat, world, sizeof(s->mat));
        s->frame = state.frameIndex;
        s->model = e->model;
    }
    std::memcpy(prev, s->prev, 12 * sizeof(float));
}

// The draws that follow write the motion target (colour attachment 1) when the bound framebuffer is the eye's scene.
extern "C" void VR_TemporalMotionWrites(int on)
{
    const bool want = on && state.eyeActive;
    if(want == state.writes)
    {
        return;
    }
    const GLuint bound = GL_BoundDrawFramebuffer();
    if(want && bound != state.sceneFbo && bound != state.compositeFbo)
    {
        return;
    }
    static constexpr GLenum both[2] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
    GL_DrawBuffersFunc(want ? 2 : 1, both);
    state.writes = want;
}

// The draws that follow (VR particles, heat haze) mark their pixels reactive: stencil bit 4.
extern "C" void VR_TemporalReactive(int on)
{
    if(!state.eyeActive)
    {
        return;
    }
    if(on)
    {
        glEnable(GL_STENCIL_TEST);
        glStencilMask(4);
        glStencilFunc(GL_ALWAYS, 4, 4);
        glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
    }
    else
    {
        glStencilMask(~0u);
        glStencilFunc(GL_ALWAYS, 0, ~0u);
        glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
        glDisable(GL_STENCIL_TEST);
    }
}

// R_RenderView, after the scene: the eye's temporal pass (TAA, or a debug view) into its scene colour.
extern "C" void VR_TemporalResolve(unsigned fbo, unsigned color, unsigned depth)
{
    if(!state.eyeActive || state.resolvedEye || !stereo::isRenderingEye() || stereo::isSpectator())
    {
        return;
    }
    state.resolvedEye = true;
    VR_TemporalMotionWrites(0);
    temporal::EyeState& e = state.eyes[state.eye];
    QVR_GPU_PROFILE("taa");

    // History resets: a gap in the frames, a jump (teleport, respawn) or a large turn (snap turn): one frame aliased
    // rather than a ghost of the old view (TEMPORAL.md 12, question 6: the conservative answer)
    bool reset = !e.valid || e.frame != state.frameIndex - 1 || !e.havePrev;
    if(!reset)
    {
        vec3_t d;
        VectorSubtract(e.origin, e.prevOrigin, d);
        float turn = 0.f;
        for(int i = 0; i < 3; i++)
        {
            float a = za::abs(e.angles[i] - e.prevAngles[i]);
            a = a > 180.f ? 360.f - a : a;
            turn = q_max(turn, a);
        }
        reset = VectorLength(d) > 40.f || turn > 25.f;
    }

    float inv[16];
    if(!temporal::invert4(e.viewProj, inv))
    {
        return;
    }
    const int mode = static_cast<int>(za::clamp(vr_temporal_debug.value, 0.f, 3.f));
    const int prev = e.cur;
    const int next = e.cur ^ 1;
    const GLuint groupsX = static_cast<GLuint>((state.width + 15) / 16), groupsY = static_cast<GLuint>((state.height + 15) / 16);

    { // the reactive mask from the stencil
        QVR_GPU_PROFILE("taa mask");
        GL_UseProgram(state.maskProgram);
        GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D, depth);
        glTexParameteri(GL_TEXTURE_2D, GL_DEPTH_STENCIL_TEXTURE_MODE, GL_STENCIL_INDEX);
        GL_BindImageTextureFunc(3, state.maskTex, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R8);
        GL_DispatchComputeFunc(groupsX, groupsY, 1);
        glTexParameteri(GL_TEXTURE_2D, GL_DEPTH_STENCIL_TEXTURE_MODE, GL_DEPTH_COMPONENT);
        GL_MemoryBarrierFunc(GL_TEXTURE_FETCH_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    }

    {
        QVR_GPU_PROFILE("taa resolve");
        GL_UseProgram(state.resolveProgram);
        GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D, color);
        GL_BindNative(GL_TEXTURE1, GL_TEXTURE_2D, depth);
        GL_BindNative(GL_TEXTURE2, GL_TEXTURE_2D, state.motionTex);
        GL_BindNative(GL_TEXTURE3, GL_TEXTURE_2D, state.maskTex);
        GL_BindNative(GL_TEXTURE4, GL_TEXTURE_2D, e.history[prev]);
        GL_BindNative(GL_TEXTURE5, GL_TEXTURE_2D, e.depth[prev]);
        GL_BindImageTextureFunc(0, e.history[next], 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
        GL_BindImageTextureFunc(1, e.depth[next], 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);
        GL_BindImageTextureFunc(2, state.displayTex, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
        GL_BindBufferRange(GL_SHADER_STORAGE_BUFFER, 15, state.statsBuffer, 0, 32);
        GL_UniformMatrix4fvFunc(0, 1, GL_FALSE, inv);
        GL_UniformMatrix4fvFunc(1, 1, GL_FALSE, e.prevViewProj);
        GL_Uniform4fFunc(2, state.jitter[0] / static_cast<float>(state.width), state.jitter[1] / static_cast<float>(state.height),
            state.stamp, gl_clipcontrol_able ? 1.f : 0.f);
        GL_Uniform4fFunc(3, za::clamp(vr_taa_blend.value, 0.01f, 1.f), za::clamp(vr_taa_clip.value, 0.25f, 4.f),
            za::clamp(vr_taa_reactive.value, 0.f, 1.f), reset ? 1.f : 0.f);
        GL_Uniform1iFunc(4, mode);
        GL_Uniform1iFunc(5, state.eye);
        GL_DispatchComputeFunc(groupsX, groupsY, 1);
        GL_MemoryBarrierFunc(GL_TEXTURE_FETCH_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);
    }

    { // into the scene
        QVR_GPU_PROFILE("taa apply");
        GL_BindFramebufferFunc(GL_FRAMEBUFFER, fbo);
        glViewport(0, 0, state.width, state.height);
        GL_UseProgram(state.applyProgram);
        GL_SetState(GLS_BLEND_OPAQUE | GLS_NO_ZTEST | GLS_NO_ZWRITE | GLS_CULL_NONE | GLS_ATTRIBS(0));
        GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D, mode != 0 ? state.displayTex : e.history[next]);
        GL_Uniform1fFunc(0, mode == 0 ? za::clamp(vr_taa_sharpen.value, 0.f, 2.f) : 0.f);
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }

    e.cur = next;
    e.valid = true;
    e.frame = state.frameIndex;
    std::memcpy(e.prevViewProj, e.viewProj, sizeof(e.prevViewProj));
    e.havePrev = true;
    VectorCopy(e.origin, e.prevOrigin);
    VectorCopy(e.angles, e.prevAngles);
}
