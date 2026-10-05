// vr_gfx_gl.cpp -- vr_gfx.hpp on Ironwail's OpenGL renderer: one shader (a program per shade) for the
// module's triangles, framebuffer-backed targets, and Ironwail's 2D functions (Draw_*, glcanvas) pointed
// at a target. The only GL in the module besides the stereo view setup (vr_stereo.cpp), the
// OpenXR graphics binding and the engine's own entity hooks.

#include "vr_gfx.hpp"
#include "vr_engine.hpp"
#include "vr_cvars.hpp"
#include "vr_retro.h"
#include "vr_retro.hpp"

#include "Zancle/Algorithm/Find.hpp"
#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Base/Strcmp.hpp"
#include "Zancle/Container/AnkerlUnorderedDense.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/String/StringView.hpp"
#include "Zancle/String/ToString.hpp"
#include "Zancle/Vocabulary/Span.hpp"
#include "vr_zancle.hpp"

#include <string.h>

namespace qvr::gfx
{
namespace
{

constexpr const char* vertexShader = R"(#version 430
layout(location = 0) uniform mat4 MVP;
layout(location = 0) in vec3 Pos;
layout(location = 1) in vec2 UV;
layout(location = 2) in vec4 Color;
layout(location = 3) in float Soft;
out vec2 uv;
out vec4 color;
out float soft;
out float viewDepth;
out vec3 worldPos; // (RETRO: in the scene, MVP the scene's)
void main()
{
    uv = UV;
    color = Color;
    soft = Soft;
    worldPos = Pos;
    gl_Position = MVP * vec4(Pos, 1.0);
    viewDepth = gl_Position.w; // the distance along the view (a perspective projection's w)
}
)";

// MODE: the Shade, one program each (compiled with "#define MODE n" and BLENDED, programFor): each gets only the
// registers its own shade needs (the screen's glow needs many; a particle's texture read few), no branch on the
// mode. BLENDED (every blend but Opaque): a fragment that adds nothing is discarded (all four zero: with every
// blend it leaves the target as it was), so the blend unit skips its read and write (the transparent part of a
// particle's or decal's quad), and a soft one reads the scene's distances only where it shows. Params: the shade's
// settings (State::params).
//
// Mode 4, a screen (the wrist gadget's, the ammo screens'): the texture's brightness (between its
// luminance and its brightest channel, so that the status bar's gold numbers and the face read as
// bright as the text) in the phosphor's colour, and with CRT strength k: scanlines and a faint
// aperture grille (one a pixel of its virtual screen, Size: fading out where they would be finer
// than the eye's pixels), a darker rim, a slight flicker, a soft bar rolling down, faint static a
// pixel at a time; while it glitches (g), bands torn sideways, the colours split and the static
// thick. Seeded by time only: both eyes see the same. With glow s (Params.w, vr_screen_text_glow):
// the lit strokes' cores whitish and a soft halo round them (glow(): from the texture's mipmaps),
// under the scanlines and torn with the rest.
constexpr const char* fragmentShader = R"(
layout(location = 2) uniform vec4 Params;
layout(location = 3) uniform vec3 Size; // Mode 4's virtual screen: pixels across, down, scanlines a pixel
layout(location = 4) uniform int SoftOn; // State::sceneDistances on unit 1
layout(binding = 0) uniform sampler2D Tex;
layout(binding = 1) uniform sampler2D SceneDistances;
in vec2 uv;
in vec4 color;
in float soft;
in float viewDepth;
in vec3 worldPos;
out vec4 result;
#if RETRO
layout(location = 9) uniform int RetroSet; // retro textures' set (vr_retro.h; 0 none): blocks in world units
#endif
#ifdef PARTICLE_RETRO_GRID
flat in vec3 particleGridScale;
// A planar particle's UV density is constant. The vertex shader derives it from the final
// stretched/pulled quad; keep the view-dependent edge filtering and colour conversion here.
void RetroBeginParticle(vec2 duvdx, vec2 duvdy, vec3 dpdx, vec3 dpdy)
{
    // Near an eighth-octave rounding boundary, retain the original derivative arithmetic.
    // This avoids choosing the opposite grid when floating-point cancellation nudges the density.
    if(particleGridScale.z != 0.0)
    {
        RetroBegin(float(RetroSet), vec2(textureSize(Tex, 0)), duvdx, duvdy,
            dpdx, dpdy, normalize(cross(dpdx, dpdy)));
        return;
    }
    int s = RetroSet;
    vec2 lq = vec2(textureSize(Tex, 0));
    if(s <= 0 || s >= 64 || RetroInfo.x <= 0.0 || RetroSets[s * 3 + 2].w > 0.5)
        return;
    RetroP0 = RetroSets[s * 3];
    RetroP1 = RetroSets[s * 3 + 1];
    RetroP2 = RetroSets[s * 3 + 2];
    RetroGrid = lq / max(RetroP0.x, 0.01);
    if(RetroP2.y > 0.0)
        RetroGrid *= particleGridScale.xy;
    vec2 w = (abs(duvdx) + abs(duvdy)) * RetroGrid;
#ifdef PARTICLE_RETRO_CENTRE_NEAR
    RetroFar = 0.0;
#else
    float fade = RetroP0.z;
    RetroFar = fade > 0.0 ? smoothstep(0.5 * fade, fade, sqrt(w.x * w.y)) : 0.0;
#endif
    Retro = s;
}
#endif
float hash(vec2 p)
{
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}
float lit(vec2 at, float lod)
{
    vec3 c = textureLod(Tex, at, lod).rgb;
    return 0.5 * max(c.r, max(c.g, c.b)) + 0.6 * dot(c, vec3(0.2126, 0.7152, 0.0722));
}
float phosphor(vec2 at)
{
    // Its own texels, sharp, however small it is in the eye (as without mipmaps); only where they
    // would be much finer than the eye's pixels (a map's board across a room) a level a step sharper
    // than the usual, so that far text does not shimmer.
    return lit(at, max(textureQueryLod(Tex, uv).y - 1.0, 0.0));
}
// Mode 4's glow round the lit strokes: the texture's brightness blurred over about one pixel of the
// virtual screen (inner: 4 taps) and two and a half (outer: 8 taps), each tap a mipmap level about
// as coarse as the ring is wide, less the face's own. Near the strokes it is whitish, further out
// the phosphor's colour.
vec3 glow(vec2 at, float s)
{
    float perPixel = float(textureSize(Tex, 0).x) / Size.x; // texels a pixel of the virtual screen
    vec2 px = 1.0 / Size.xy;
    float lodInner = log2(perPixel), lodOuter = log2(perPixel * 2.0);
    float inner = 0.0, outer = 0.0;
    for(int i = 0; i < 4; i++)
    {
        float a = 1.5707963 * float(i) + 0.7853982;
        inner += lit(at + vec2(cos(a), sin(a)) * px * 0.9, lodInner);
    }
    for(int i = 0; i < 8; i++)
    {
        float a = 0.7853982 * float(i) + 0.3926991;
        outer += lit(at + vec2(cos(a), sin(a)) * px * 2.5, lodOuter);
    }
    const float face = 0.14; // about the face's brightness: it does not glow
    inner = max(inner * 0.25 - face, 0.0) / (1.0 - face);
    outer = max(outer * 0.125 - face, 0.0) / (1.0 - face);
    return s * (mix(color.rgb, vec3(1.0), 0.5) * (0.9 * inner) + color.rgb * (1.2 * outer));
}
vec4 screen()
{
    float t = Params.x, k = Params.y, g = Params.z;
    float tick = floor(t * 30.0);
    float seed = fract(tick * 0.618034) * 512.0;
    vec2 at = uv;
    if(g > 0.0)
    {
        float band = floor(at.y * 14.0 + hash(vec2(seed, 3.0)) * 4.0);
        if(hash(vec2(band, seed)) > 1.0 - 0.6 * g)
            at.x += (hash(vec2(band, seed + 7.0)) - 0.5) * 0.15 * g;
        at.y += (hash(vec2(seed, 11.0)) - 0.5) * 0.02 * g;
    }
    float split = (0.144 * k + 1.44 * g) / Size.x;
    float lum = phosphor(at);
    vec3 rgb = color.rgb * vec3(phosphor(at - vec2(split, 0.0)), lum, phosphor(at + vec2(split, 0.0)));
    float s = Params.w;
    if(s > 0.0)
    {
        // The lit strokes' core pushed towards white, a little over (for the bloom), and their glow.
        float core = s * 0.8 * smoothstep(0.3, 0.9, lum);
        rgb = mix(rgb, vec3(1.2 * lum), min(core, 0.9));
        rgb += glow(at, s);
    }
    rgb *= 1.0 - 0.3 * g;

    float y = uv.y * Size.y * Size.z;
    float scanFade = clamp(1.0 - (fwidth(y) - 0.25) * 2.5, 0.0, 1.0);
    float scan = 1.0 - 0.3 * k * scanFade * (0.5 + 0.5 * cos(y * 6.2831853));
    float x = uv.x * Size.x;
    float grilleFade = clamp(1.0 - (fwidth(x) - 0.25) * 2.5, 0.0, 1.0);
    float grille = 1.0 - 0.12 * k * grilleFade * (0.5 + 0.5 * cos(x * 6.2831853));
    vec2 p = uv * 2.0 - 1.0;
    float rim = 1.0 - 0.15 * k * dot(p, p);
    float flicker = 1.0 - 0.025 * k * hash(vec2(floor(t * 24.0) * 0.618034, 5.0));
    rgb *= scan * grille * rim * flicker;

    float d = fract(uv.y + fract(t * 0.12) + 0.5) - 0.5;
    rgb += color.rgb * (0.05 * k * exp(-d * d * 400.0));
    float noise = hash(floor(uv * Size.xy) + vec2(seed, seed * 0.37));
    rgb += color.rgb * (noise * k * (0.025 + 0.25 * g));
    return vec4(rgb, color.a); // (opaque but for the tips' fading screens over the scene: text3d::queueOverlayScreen)
}
// Mode 5, the wrist gadget's hologram (premultiplied; the vertex colour's alpha: how shown). Params: time, the effect's
// strength k, glitch g, and 1 for the beam. The text: its lit strokes (the texture's brightness) in the vertex colour,
// their cores whitish and a soft halo round them (glow(): Size and the mipmaps as mode 4's), a faint dark haze round
// them (the alpha) so that it reads against a bright wall; with k, scanlines drifting up, a bright band now and then,
// a flicker and a tiny shake; while it glitches, bands torn sideways and the colours split. The beam: the vertex colour
// fading across (uv.x -1..1) and up (uv.y 0 at the gadget's screen, 1 at the text), in slow streaks, scanlines rising
// through it; added (alpha 0).
vec4 hologram()
{
    float t = Params.x, k = Params.y, g = Params.z;
    float flicker = 1.0 - 0.1 * k * hash(vec2(floor(t * 20.0) * 0.618034, 9.0));
    if(Params.w > 0.5)
    {
        float across = max(1.0 - uv.x * uv.x, 0.0);
        float up = clamp(uv.y, 0.0, 1.0);
        float along = smoothstep(0.0, 0.2, up) * mix(1.0, 0.2, up);
        float streaks = 0.7 + 0.3 * sin(uv.x * 13.0 + t * 1.7) * sin(uv.x * 4.0 - t * 1.1);
        float scan = 1.0 - 0.35 * min(k, 1.0) * (0.5 + 0.5 * sin((up * 10.0 - t * 1.5) * 6.2831853));
        return vec4(color.rgb * (color.a * across * across * along * streaks * scan * flicker), 0.0);
    }
    float tick = floor(t * 30.0);
    float seed = fract(tick * 0.618034) * 512.0;
    vec2 at = uv;
    at.y += k * 0.3 / Size.y * sin(t * 7.3) * sin(t * 2.9);
    if(g > 0.0)
    {
        float band = floor(at.y * 12.0 + hash(vec2(seed, 3.0)) * 4.0);
        if(hash(vec2(band, seed)) > 1.0 - 0.6 * g)
            at.x += (hash(vec2(band, seed + 7.0)) - 0.5) * 0.08 * g;
        at.y += (hash(vec2(seed, 11.0)) - 0.5) * 0.02 * g;
    }
    float split = (0.4 * k + 2.0 * g) / Size.x;
    float lum = phosphor(at);
    vec3 rgb = color.rgb * vec3(phosphor(at - vec2(split, 0.0)), lum, phosphor(at + vec2(split, 0.0)));
    rgb = mix(rgb, vec3(1.1 * lum), 0.3 * smoothstep(0.3, 0.9, lum));
    rgb += glow(at, 1.0);
    float haze = lit(at, log2(float(textureSize(Tex, 0).x) / Size.x * 4.0));

    float y = uv.y * Size.y * Size.z;
    float scanFade = clamp(1.0 - (fwidth(y) - 0.25) * 2.5, 0.0, 1.0);
    float scan = 1.0 - 0.4 * min(k, 1.5) * scanFade * (0.5 + 0.5 * cos((y - t * 1.5) * 6.2831853));
    float d = fract(uv.y - t * 0.3) - 0.5;
    float band = 1.0 + 0.4 * k * exp(-d * d * 150.0);
    rgb *= scan * band * flicker * (1.0 - 0.3 * g);
    float a = clamp(haze * 2.0 + lum * 0.4, 0.0, 0.6);
    return vec4(rgb, a) * color.a;
}
void main()
{
#if MODE == 2
    // Derivatives before any discard. Half-resolution has no hardware scene-depth test;
    // reject covered fragments before their retro texture/palette work.
    vec2 duvdx = dFdx(uv), duvdy = dFdy(uv);
#if RETRO
    vec3 dpdx = dFdx(worldPos), dpdy = dFdy(worldPos);
#endif
#ifdef HALFRES
    float halfDistance = texelFetch(SceneDistances, ivec2(gl_FragCoord.xy), 0).r;
    if(halfDistance < viewDepth)
        discard;
#endif
#endif
#if MODE == 1
    float falloff = clamp(1.0 - dot(uv, uv), 0.0, 1.0);
    result = vec4(color.rgb, color.a * falloff);
#elif MODE == 2
#if RETRO
#ifdef PARTICLE_RETRO_GRID
    RetroBeginParticle(duvdx, duvdy, dpdx, dpdy);
#else
    RetroBegin(float(RetroSet), vec2(textureSize(Tex, 0)), duvdx, duvdy, dpdx, dpdy, normalize(cross(dpdx, dpdy)));
#endif
    if(Retro > 0)
    {
#ifdef PARTICLE_RETRO_CENTRE_NEAR
        // Selected only when the effective set has these exact values. A settings edit
        // selects the general variant on the next draw, without an atlas rebuild.
        RetroP2.x = 0.0;
        RetroP1.x = 1.0;
        RetroFar = 0.0;
        result = RetroQuantPremul(RetroBlocks(Tex, uv, duvdx, duvdy, false) * color, floor(uv * RetroGrid));
#else
        result = RetroQuantPremul(RetroSample(Tex, uv, duvdx, duvdy, false) * color, floor(uv * RetroGrid));
#endif
    }
    else
        result = texture(Tex, uv) * color;
#else
#ifdef HALFRES
    result = textureGrad(Tex, uv, duvdx, duvdy) * color;
#else
    result = texture(Tex, uv) * color;
#endif
#endif
#elif MODE == 3
    vec4 c = texture(Tex, uv);
    if(c.a < 0.666)
        discard;
    result = vec4(c.rgb * color.rgb, 1.0);
#elif MODE == 4
    result = screen();
#elif MODE == 5
    result = hologram();
#else
    result = color;
#endif
#if BLENDED
    if(result == vec4(0.0))
        discard;
#endif
#ifdef HALFRES
    // Drawn at half the scene's size (drawParticlesHalf), the size of its distances: with no depth buffer, hidden
    // behind them instead (each texel the nearest of its four pixels).
    {
        if(SoftOn != 0 && soft > 0.0)
        {
            float f = clamp((halfDistance - viewDepth) / soft, 0.0, 1.0);
            result *= f * f * (3.0 - 2.0 * f);
        }
    }
#else
    // Soft: fading out as the opaque scene comes close behind (premultiplied: all four). The distances are half the
    // target's size, each the nearest of its four pixels.
    if(SoftOn != 0 && soft > 0.0)
    {
        ivec2 p = min(ivec2(gl_FragCoord.xy) >> 1, textureSize(SceneDistances, 0) - 1);
        float f = clamp((texelFetch(SceneDistances, p, 0).r - viewDepth) / soft, 0.0, 1.0);
        result *= f * f * (3.0 - 2.0 * f);
    }
#endif
}
)";

// One program per shade and whether it blends without writing depth (programFor): 0 not made yet.
constexpr int shadeCount = static_cast<int>(Shade::Hologram) + 1;
GLuint programs[shadeCount][2][2]{}; // [shade][blended][retro: Shade::Texture's with retro textures]
bool programFailed[shadeCount][2][2]{};

[[nodiscard]] GLuint compile(GLenum type, const char* source, const char* name)
{
    const GLuint shader = GL_CreateShaderFunc(type);
    GL_ShaderSourceFunc(shader, 1, &source, nullptr);
    GL_CompileShaderFunc(shader);

    GLint ok = 0;
    GL_GetShaderivFunc(shader, GL_COMPILE_STATUS, &ok);
    if(!ok)
    {
        char log[1024];
        GL_GetShaderInfoLogFunc(shader, sizeof(log), nullptr, log);
        Con_Warning("VR: %s shader failed to compile:\n%s\n", name, log);
    }
    return shader;
}

// The particles' quads (ParticleInstance, drawParticles): six vertices each, read from the frame's records (no vertex
// attributes), the same arithmetic as the CPU's quads were made with (vr_particles.cpp, before round 21).
constexpr const char* particleVertexShader = R"(#version 430
layout(location = 0) uniform mat4 MVP;
layout(location = 5) uniform vec3 Eye;
layout(location = 6) uniform vec3 Right;
layout(location = 7) uniform vec3 Up;
layout(location = 8) uniform int Pull;
layout(location = 10) uniform int Pass;           // ParticlePass: 0 all, 1 the small ones only, 2 the large ones only
layout(location = 11) uniform float PixelScale;   // the scene target's pixels across a unit at distance 1
#ifdef PARTICLE_TRIM
layout(location = 13) uniform vec4 TrimView; // actual target width, height, atlas width, height
layout(location = 14) uniform vec2 Trim; // enable, world block size (0 plain)
layout(location = 20) uniform vec4 Support[16]; // guarded base-level support in atlas UVs
#endif
layout(location = 12) uniform float LargePixels;  // large: half across at least this many of them
struct Particle
{
    vec4 orgHalf;   // org, half size
    vec4 color;     // premultiplied
    vec4 velStreak; // velocity, streak seconds
    vec4 csSoft;    // cos, sin, soft fade, pull
    vec4 uv;        // the cell: u0, v0, u1, v1
    vec4 flags;     // x: flat
};
layout(std430, binding = 0) readonly buffer Particles
{
    Particle particles[];
};
out vec2 uv;
out vec4 color;
out float soft;
out float viewDepth;
out vec3 worldPos;
#ifdef PARTICLE_RETRO_GRID
layout(binding = 0) uniform sampler2D Tex;
flat out vec3 particleGridScale;
#endif
// The six corners (two triangles): down left, up left, up right, down left, up right, down right.
const vec2 signs[6] = vec2[6](vec2(-1.0, -1.0), vec2(1.0, -1.0), vec2(1.0, 1.0), vec2(-1.0, -1.0), vec2(1.0, 1.0), vec2(-1.0, 1.0));
void main()
{
    Particle p = particles[gl_VertexID / 6];
    int corner = gl_VertexID % 6;
    vec3 org = p.orgHalf.xyz;
    float h = p.orgHalf.w;
    float c = p.csSoft.x, s = p.csSoft.y;
    vec3 pr = p.flags.x != 0.0 ? vec3(1.0, 0.0, 0.0) : Right;
    vec3 pu = p.flags.x != 0.0 ? vec3(0.0, 1.0, 0.0) : Up;
    vec3 r = (pr * c + pu * s) * h;
    vec3 u = (pu * c - pr * s) * h;
    float streak = p.velStreak.w;
    if(streak > 0.0)
    {
        vec3 vel = p.velStreak.xyz;
        vec3 ray = normalize(org - Eye);
        vec3 across = vel - ray * dot(vel, ray);
        float speed = length(across);
        if(speed > 1.0)
        {
            vec3 along = across / speed;
            r = cross(along, ray) * h;
            u = along * (h + min(streak * speed, 8.0));
        }
    }
    vec3 o = org;
    float pull = p.csSoft.w;
    if(Pull != 0 && pull > 0.0)
    {
        vec3 forward = cross(Up, Right);
        float w = dot(org - Eye, forward);
        if(w > 0.0)
        {
            float k = max(w - pull, min(w, 8.0)) / w;
            o = Eye + (org - Eye) * k;
            r *= k;
            u *= k;
        }
    }
    if(Pass != 0)
    {
        // Large or small by its size in the scene's pixels (the same in both passes: each drawn in one).
        vec4 mid = MVP * vec4(o, 1.0);
        bool large = mid.w > 0.0 && max(length(r), length(u)) * PixelScale >= LargePixels * mid.w;
        if(large != (Pass == 2))
        {
            gl_Position = vec4(2.0, 2.0, 2.0, 1.0); // nothing: its six corners at one point, outside the view
            uv = vec2(0.0);
            color = vec4(0.0);
            soft = 0.0;
            worldPos = o;
            viewDepth = 0.0;
            return;
        }
    }
#ifdef PARTICLE_RETRO_GRID
    // UV.x follows u, UV.y follows r. Use the dual basis rather than assuming perpendicular
    // axes: this also handles stretched streaks and any non-orthogonal camera basis.
    float area = length(cross(u, r));
    vec2 density = abs(p.uv.zw - p.uv.xy) * vec2(length(r), length(u)) /
        max(2.0 * area, 1e-20) * vec2(textureSize(Tex, 0));
    density = max(density, vec2(1e-4));
    vec2 octave = log2(density) * 8.0 + 0.5;
    bool boundary = any(lessThan(min(fract(octave), 1.0 - fract(octave)), vec2(0.005)));
    particleGridScale = vec3(1.0 / exp2(floor(octave) * 0.125), boundary || area < 1e-12 ? 1.0 : 0.0);
#endif
    vec2 sg = signs[corner];
    vec2 sampleUV = vec2(sg.x > 0.0 ? p.uv.z : p.uv.x, sg.y > 0.0 ? p.uv.w : p.uv.y);
#ifdef PARTICLE_TRIM
    if(Trim.x > 0.0 && p.flags.y >= 0.0)
    {
        vec4 support = Support[clamp(int(p.flags.y), 0, 15)];
        vec4 mid = MVP * vec4(o, 1.0);
        vec4 a = MVP * vec4(u, 0.0), b = MVP * vec4(r, 0.0);
        float variation = abs(a.w) + abs(b.w);
        // Nearly parallel to the image plane; tilted/near-clipped geometry keeps its full quad.
        if(mid.w > 0.0 && variation < mid.w * 0.001 && mid.z - abs(a.z) - abs(b.z) > 0.0)
        {
            vec2 ax = (a.xy * mid.w - mid.xy * a.w) / (mid.w * mid.w) * TrimView.xy * 0.5;
            vec2 bx = (b.xy * mid.w - mid.xy * b.w) / (mid.w * mid.w) * TrimView.xy * 0.5;
            float det = ax.x * bx.y - ax.y * bx.x;
            vec2 range = (p.uv.zw - p.uv.xy) * TrimView.zw * 0.5;
            vec2 dx = abs(vec2(bx.y, ax.y) * range / det);
            vec2 dy = abs(vec2(bx.x, ax.x) * range / det);
            // Upper bound for both texture derivatives, with a margin for perspective,
            // helper pixels and anisotropic sampling. Level 0..2 are power-of-two
            // reductions of this atlas; coarser footprints keep the original quad.
            float footprint = length(dx + dy) * 1.1;
            float level = ceil(log2(max(footprint, 1.0)));
            if(abs(det) > 1e-8 && level <= 2.0)
            {
                vec2 blockTexels = vec2(0.0);
                bool safe = true;
#ifdef PARTICLE_RETRO_GRID
                blockTexels = Trim.y / particleGridScale.xy;
                safe = particleGridScale.z == 0.0;
#endif
                // Both block-centre taps can lie up to 1.5 blocks away. Filtering plus
                // helper pixels are included even with hard edges; preserve original UVs/grid.
                vec2 margin = vec2(2.0 + 2.0 * exp2(level)) + 1.5 * blockTexels;
                if(safe && all(lessThanEqual(margin, vec2(15.0))))
                {
                    vec4 bounds = support;
                    vec2 lo = max(p.uv.xy, bounds.xy - margin / TrimView.zw);
                    vec2 hi = min(p.uv.zw, bounds.zw + margin / TrimView.zw);
                    sampleUV = mix(lo, max(lo, hi), sg * 0.5 + 0.5);
                    sg = (sampleUV - p.uv.xy) / (p.uv.zw - p.uv.xy) * 2.0 - 1.0;
                }
            }
        }
    }
#endif
    vec3 pos = o + u * sg.x + r * sg.y;
    uv = sampleUV;
    color = p.color;
    soft = p.csSoft.z;
    worldPos = pos;
    gl_Position = MVP * vec4(pos, 1.0);
    viewDepth = gl_Position.w;
}
)";

glm::vec4 particleSupport[16]{};
glm::vec2 particleAtlasSize{1.f};

GLuint particleProgram[11]{}; // 0 plain, 1 reference, 2 plain half, 3/4 general fast full/half, 5/6 centre-near full/half
bool particleProgramFailed[11]{};

// drawParticlesHalf's: its target (two, for the eyes' size and the spectator camera's), and the pass blending it into
// the scene.
struct HalfTarget
{
    GLuint texture = 0;
    GLuint fbo = 0;
    int width = 0, height = 0;
};
HalfTarget halfTargets[2];
int halfTargetNext = 0;
GLuint halfCompositeProgram = 0;
bool halfCompositeFailed = false;

constexpr const char* halfCompositeVs = R"(#version 430
void main()
{
    vec2 v = vec2(gl_VertexID & 1, gl_VertexID >> 1);
    gl_Position = vec4(v * 4.0 - 1.0, 0.0, 1.0);
}
)";
// Premultiplied: over the scene as the particles themselves would have been. Each texel is the scene's pixels 2x, 2y
// .. 2x + 1, 2y + 1 (the scene's viewport halved).
constexpr const char* halfCompositeFs = R"(#version 430
layout(binding = 0) uniform sampler2D Half;
layout(location = 0) uniform vec2 Scale;
out vec4 result;
void main()
{
    result = texture(Half, gl_FragCoord.xy * Scale);
    if(result == vec4(0.0))
        discard;
}
)";

// The fragment shader for a shade, blended or not, with retro textures or not (Shade::Texture).
[[nodiscard]] za::String fragmentFor(int shade, bool blended, bool retro)
{
    za::String f = "#version 430\n#define MODE " + za::toString(shade) + "\n#define BLENDED " + (blended ? "1" : "0") +
                   "\n#define RETRO " + (retro ? "1" : "0") + "\n";
    if(retro)
    {
        f += QVR_RETRO_GLSL(QS_STRINGIFY(QVR_RETRO_LUT_UNIT_GFX));
    }
    return f + fragmentShader;
}

// The tube (TubeRing, drawTube): six vertices a quad, `Sides` quads round each pair of consecutive rings, read from the
// frame's records (no vertex attributes), lit per vertex.
constexpr const char* tubeVertexShader = R"(#version 430
layout(location = 0) uniform mat4 MVP;
layout(location = 5) uniform vec3 Eye;
layout(location = 6) uniform vec3 Albedo;
layout(location = 7) uniform vec3 Key;
layout(location = 8) uniform int Sides;
layout(location = 9) uniform vec3 Rust;
layout(location = 10) uniform int Flat; // faceted: the sides turned half a side, each quad lit by its face's normal
struct Ring
{
    vec4 mid;
    vec4 across;
    vec4 along;
    vec4 ambient;
    vec4 lamp;
    vec4 lampDir;
};
layout(std430, binding = 0) readonly buffer Rings
{
    Ring rings[];
};
out vec2 uv;
out vec4 color;
out float soft;
out float viewDepth;
// The six corners (two triangles) of a quad: (ring, side) steps.
const ivec2 corners[6] = ivec2[6](ivec2(0, 0), ivec2(1, 0), ivec2(1, 1), ivec2(0, 0), ivec2(1, 1), ivec2(0, 1));
void main()
{
    int quad = gl_VertexID / 6;
    ivec2 c = corners[gl_VertexID % 6];
    Ring g = rings[quad / Sides + c.x];
    float turn = Flat != 0 ? 0.5 : 0.0;
    float th = 6.2831853 * (float(quad % Sides + c.y) + turn) / float(Sides);
    vec3 other = cross(g.along.xyz, g.across.xyz);
    vec3 n = cos(th) * g.across.xyz + sin(th) * other;
    vec3 pos = g.mid.xyz + n * g.mid.w;
    if (Flat != 0)
    {
        float face = 6.2831853 * float(quad % Sides + 1) / float(Sides); // (its corners at k + 0.5, k + 1.5)
        n = cos(face) * g.across.xyz + sin(face) * other;
    }
    vec3 hv = normalize(Key + normalize(Eye - pos));
    vec3 light = g.ambient.rgb * (1.0 + 0.4 * dot(n, Key)) + g.lamp.rgb * (0.2 + 0.8 * max(dot(n, g.lampDir.xyz), 0.0));
    float x = max(dot(n, hv), 0.0);
    x *= x; // to the 16th
    x *= x;
    x *= x;
    x *= x;
    float rust = clamp(g.ambient.w, 0.0, 1.0); // (a chain's links: rusted that much, grimy by lamp.w)
    float sheen = 0.45 * (1.0 - 0.85 * rust) * (1.0 - g.lamp.w) * x * dot(g.ambient.rgb + g.lamp.rgb, vec3(0.3333));
    vec3 albedo = mix(Albedo, Rust, rust) * (1.0 - g.lamp.w);
    color = vec4(clamp(albedo * light + vec3(sheen), 0.0, 1.0), 1.0);
    uv = vec2(0.0);
    soft = 0.0;
    gl_Position = MVP * vec4(pos, 1.0);
    viewDepth = gl_Position.w;
}
)";

GLuint tubeProgram = 0;
bool tubeProgramFailed = false;

// The bent mesh (BentDraw, drawBent): gl_VertexID's copy and mesh vertex; the curve's sample at its arc length by a
// binary search (the samples' pos.w), interpolated, extrapolated along the end segments past the ends.
constexpr const char* bentVertexShader = R"(#version 430
layout(location = 0) uniform mat4 MVP;
layout(location = 1) uniform vec3 Eye;
layout(location = 2) uniform vec4 Counts; // the mesh's first vec4, its vertices, the curve's first vec4, its samples (whole)
layout(location = 3) uniform vec2 Along;   // period, scale
layout(std430, binding = 0) readonly buffer Data
{
    vec4 data[];
};
out vec2 uv;
out vec3 fromEye;
void main()
{
    ivec4 n = ivec4(Counts + 0.5);
    int copy = gl_VertexID / n.y;
    int k = n.x + 2 * (gl_VertexID % n.y);
    vec4 mp = data[k];
    float s = (float(copy) * Along.x + mp.x) * Along.y;
    int lo = 0, hi = n.w - 1; // the segment lo .. lo + 1 holding s
    while(hi - lo > 1)
    {
        int mid = (lo + hi) / 2;
        if(data[n.z + 3 * mid].w <= s)
            lo = mid;
        else
            hi = mid;
    }
    int a = n.z + 3 * lo, b = a + 3;
    float t = (s - data[a].w) / max(data[b].w - data[a].w, 1e-4);
    float tc = clamp(t, 0.0, 1.0);
    vec3 side = normalize(mix(data[a + 1].xyz, data[b + 1].xyz, tc));
    vec3 up = normalize(mix(data[a + 2].xyz, data[b + 2].xyz, tc));
    vec3 pos = mix(data[a].xyz, data[b].xyz, t) + (side * mp.y + up * mp.z) * Along.y;
    uv = data[k + 1].xy;
    fromEye = pos - Eye;
    gl_Position = MVP * vec4(pos, 1.0);
}
)";

// As the alias models' (gl_shaders.h): an ALPHABRIGHT skin's lit texels (alpha 1) times the light, its fullbright ones
// (alpha 0) as they are, a fullbright texture added; clamped to the scene's brightest (vr_tonemap), fogged.
constexpr const char* bentFragmentShader = R"(#version 430
layout(location = 4) uniform vec3 Light;
layout(location = 5) uniform vec4 Fog; // rgb, density (the frame's)
layout(location = 6) uniform float Tone;
layout(location = 7) uniform int HasFullbright;
layout(binding = 0) uniform sampler2D Skin;
layout(binding = 1) uniform sampler2D Fullbright;
in vec2 uv;
in vec3 fromEye;
out vec4 result;
void main()
{
    vec4 c = texture(Skin, uv);
    vec3 rgb = mix(c.rgb, c.rgb * Light, c.a);
    if(HasFullbright != 0)
        rgb += texture(Fullbright, uv).rgb;
    rgb = clamp(rgb, vec3(0.0), vec3(Tone));
    float fog = clamp(exp2(-abs(Fog.w) * dot(fromEye, fromEye)), 0.0, 1.0);
    result = vec4(mix(Fog.rgb, rgb, fog), 1.0);
}
)";

GLuint bentProgram = 0;
bool bentProgramFailed = false;

// The program for a shade; blended: a blend other than Opaque, writing no depth (zero fragments discarded). 0 if it
// does not build.
GLuint programFor(Shade shade, bool blended, bool retro)
{
    const int s = static_cast<int>(shade);
    retro = retro && shade == Shade::Texture;
    GLuint& p = programs[s][blended][retro];
    if(p || programFailed[s][blended][retro])
    {
        return p;
    }
    p = glProgram(vertexShader, fragmentFor(s, blended, retro).cStr(), retro ? "vr triangles (retro)" : "vr triangles");
    programFailed[s][blended][retro] = !p;
    return p;
}

// Retro textures' block and palette table for a draw with set `set` (0: none).
void bindRetro(int set)
{
    if(set > 0)
    {
        retro::bindForDraw(QVR_RETRO_LUT_UNIT_GFX);
        GL_Uniform1iFunc(9, set);
    }
}

// begin2D() / end2D().
struct Saved2D
{
    bool active{false};
    glcanvas_t canvas{};
    GLuint mipmapped{0}; // the target's texture, if its mipmaps are to be rebuilt
};
Saved2D saved2D;

// Where the engine draws its 2D pass: the post-processing composite, or the window. Set rather than
// read back: glGet* makes the CPU wait for the driver (about half a millisecond a call with a
// threaded driver).
void bindWindow()
{
    GL_BindFramebufferFunc(GL_FRAMEBUFFER, GL_NeedsPostprocess() ? framebufs.composite.fbo : 0);
    glViewport(glx, gly, glwidth, glheight);
}

// By name; looked up by a const char* without making a za::String of it (a transparent hash).
struct NameHash
{
    using is_transparent = void;
    [[nodiscard]] za::U64 operator()(za::StringView s) const { return ankerl::unordered_dense::hash<za::StringView>{}(s); }
};
struct NameEqual
{
    using is_transparent = void;
    [[nodiscard]] bool operator()(za::StringView a, za::StringView b) const { return a == b; }
};
ankerl::unordered_dense::map<za::String, qpic_t*, NameHash, NameEqual> pics;

[[nodiscard]] qpic_t* picNamed(const char* name)
{
    const auto it = pics.find(za::StringView{name});
    if(it != pics.end())
    {
        return it->second;
    }
    qpic_t* p = Draw_PicFromWad(name);
    pics.emplace(name, p);
    return p;
}

GLuint copyFbo = 0;

// glBlendFuncSeparate (GL 1.4), which Ironwail does not load.
using BlendFuncSeparateFn = void(APIENTRY*)(GLenum, GLenum, GLenum, GLenum);
BlendFuncSeparateFn blendFuncSeparate = nullptr;

} // namespace

void onGameDirChanged()
{
    pics.clear();
}

unsigned glProgram(const char* vertex, const char* fragment, const char* name)
{
    const GLuint vs = compile(GL_VERTEX_SHADER, vertex, name);
    const GLuint fs = fragment ? compile(GL_FRAGMENT_SHADER, fragment, name) : 0;
    const GLuint p = GL_CreateProgramFunc();
    GL_AttachShaderFunc(p, vs);
    if(fs)
    {
        GL_AttachShaderFunc(p, fs);
    }
    GL_LinkProgramFunc(p);
    GL_DeleteShaderFunc(vs);
    if(fs)
    {
        GL_DeleteShaderFunc(fs);
    }

    GLint ok = 0;
    GL_GetProgramivFunc(p, GL_LINK_STATUS, &ok);
    if(!ok)
    {
        Con_Warning("VR: %s shader failed to link\n", name);
        GL_DeleteProgramFunc(p);
        return 0;
    }
    GL_ObjectLabelFunc(GL_PROGRAM, p, -1, name);
    return p;
}

namespace
{

// The program and state for `state`, its uniforms and textures set; false if there is no program.
[[nodiscard]] bool beginDraw(const glm::mat4& mvp, const State& state, Texture texture)
{
    const bool retro = state.retro > 0 && state.shade == Shade::Texture;
    const GLuint program = programFor(state.shade, state.blend != Blend::Opaque && !state.depthWrite, retro);
    if(!program)
    {
        return false;
    }

    unsigned flags = GLS_CULL_NONE | GLS_ATTRIBS(4);
    flags |= state.blend == Blend::Opaque ? GLS_BLEND_OPAQUE : GLS_BLEND_ALPHA;
    if(!state.depthTest)
    {
        flags |= GLS_NO_ZTEST;
    }
    if(!state.depthWrite)
    {
        flags |= GLS_NO_ZWRITE;
    }

    GL_UseProgram(program);
    GL_SetState(flags);
    if(state.blend == Blend::Premultiplied)
    {
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    }
    else if(state.blend == Blend::Modulate)
    {
        glBlendFunc(GL_DST_COLOR, GL_ONE_MINUS_SRC_ALPHA);
    }
    else if(state.blend == Blend::Additive)
    {
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    }
    GL_UniformMatrix4fvFunc(0, 1, GL_FALSE, &mvp[0][0]);
    if(state.shade == Shade::Screen || state.shade == Shade::Hologram)
    {
        GL_Uniform4fFunc(2, state.params.x, state.params.y, state.params.z, state.params.w);
        GL_Uniform3fFunc(3, state.screen.x, state.screen.y, state.screen.z);
    }
    GL_Uniform1iFunc(4, state.sceneDistances ? 1 : 0);
    if(retro)
    {
        bindRetro(state.retro);
    }
    if(texture)
    {
        GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D, texture);
    }
    if(state.sceneDistances)
    {
        GL_BindNative(GL_TEXTURE1, GL_TEXTURE_2D, state.sceneDistances);
    }
    return true;
}

// `count` vertices from `buffer` at `offset`, then the blend put back as GLS_BLEND_ALPHA expects it.
void drawVertices(GLuint buffer, const GLbyte* offset, za::SizeT count, const State& state)
{
    GL_BindBuffer(GL_ARRAY_BUFFER, buffer);
    GL_VertexAttribPointerFunc(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), offset + offsetof(Vertex, pos));
    GL_VertexAttribPointerFunc(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), offset + offsetof(Vertex, uv));
    GL_VertexAttribPointerFunc(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), offset + offsetof(Vertex, color));
    GL_VertexAttribPointerFunc(3, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex), offset + offsetof(Vertex, soft));
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(count));
    GL_BindBuffer(GL_ARRAY_BUFFER, 0);

    if(state.blend == Blend::Premultiplied || state.blend == Blend::Modulate || state.blend == Blend::Additive)
    {
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // what GLS_BLEND_ALPHA expects
    }
}

} // namespace

void draw(za::Span<const Vertex> triangles, const glm::mat4& mvp, const State& state, Texture texture)
{
    if(triangles.empty() || !beginDraw(mvp, state, texture))
    {
        return;
    }
    // Into the frame's upload buffer, like Ironwail's own dynamic geometry.
    GLuint buf = 0;
    GLbyte* ofs = nullptr;
    GL_Upload(GL_ARRAY_BUFFER, triangles.data(), triangles.sizeBytes(), &buf, &ofs);
    drawVertices(buf, ofs, triangles.size(), state);
}

void upload(StaticTriangles& t, za::Span<const Vertex> triangles)
{
    t.count = triangles.size();
    t.uploads++;
    t.uploadedBytes += triangles.sizeBytes();
    if(triangles.empty())
    {
        return;
    }
    if(!t.buffer || triangles.sizeBytes() > t.capacity)
    {
        if(t.buffer)
        {
            GL_DeleteBuffer(t.buffer);
        }
        t.capacity = triangles.sizeBytes() + triangles.sizeBytes() / 2; // room to grow
        t.buffer = GL_CreateBuffer(GL_ARRAY_BUFFER, GL_DYNAMIC_DRAW, "vr static triangles", t.capacity, nullptr);
    }
    else
    {
        // Orphaned: the frames still drawing from its old contents keep them, the driver gives it new storage.
        GL_BindBuffer(GL_ARRAY_BUFFER, t.buffer);
        GL_BufferDataFunc(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(t.capacity), nullptr, GL_DYNAMIC_DRAW);
    }
    GL_BindBuffer(GL_ARRAY_BUFFER, t.buffer);
    GL_BufferSubDataFunc(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(triangles.sizeBytes()), triangles.data());
    GL_BindBuffer(GL_ARRAY_BUFFER, 0);
}

void update(StaticTriangles& t, za::Span<const Vertex> triangles, za::SizeT first, za::SizeT count)
{
    if(!t.buffer || triangles.sizeBytes() > t.capacity)
    {
        upload(t, triangles);
        return;
    }
    t.count = triangles.size();
    if(count == 0)
    {
        return;
    }
    t.uploads++;
    t.uploadedBytes += static_cast<long long>(count * sizeof(Vertex));
    GL_BindBuffer(GL_ARRAY_BUFFER, t.buffer);
    GL_BufferSubDataFunc(GL_ARRAY_BUFFER, static_cast<GLintptr>(first * sizeof(Vertex)),
        static_cast<GLsizeiptr>(count * sizeof(Vertex)), triangles.data() + first);
    GL_BindBuffer(GL_ARRAY_BUFFER, 0);
}

void draw(const StaticTriangles& t, const glm::mat4& mvp, const State& state, Texture texture)
{
    if(!t.buffer || t.count == 0 || !beginDraw(mvp, state, texture))
    {
        return;
    }
    drawVertices(t.buffer, nullptr, t.count, state);
}

void upload(StorageBuffer& b, const void* data, za::SizeT bytes)
{
    b.size = bytes;
    if(bytes == 0)
    {
        return;
    }
    if(!b.buffer || bytes > b.capacity)
    {
        if(b.buffer)
        {
            GL_DeleteBuffer(b.buffer);
        }
        b.capacity = bytes + bytes / 2; // room to grow
        b.buffer = GL_CreateBuffer(GL_SHADER_STORAGE_BUFFER, GL_DYNAMIC_DRAW, "vr storage", b.capacity, nullptr);
    }
    else
    {
        GL_BindBuffer(GL_SHADER_STORAGE_BUFFER, b.buffer);
        GL_BufferDataFunc(GL_SHADER_STORAGE_BUFFER, static_cast<GLsizeiptr>(b.capacity), nullptr, GL_DYNAMIC_DRAW);
    }
    GL_BindBuffer(GL_SHADER_STORAGE_BUFFER, b.buffer);
    GL_BufferSubDataFunc(GL_SHADER_STORAGE_BUFFER, 0, static_cast<GLsizeiptr>(bytes), data);
    GL_BindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void bindStorage(unsigned binding, const StorageBuffer& b)
{
    if(b.buffer && b.size > 0)
    {
        GL_BindBufferRange(GL_SHADER_STORAGE_BUFFER, binding, b.buffer, 0, static_cast<GLsizeiptr>(b.size));
    }
}

void bindTexture(unsigned unit, Texture texture)
{
    GL_BindNative(GL_TEXTURE0 + unit, GL_TEXTURE_2D, texture);
}

void particleSupportBounds(za::Span<const glm::vec4> bounds, int width, int height)
{
    particleAtlasSize = {static_cast<float>(width), static_cast<float>(height)};
    for(za::SizeT i = 0; i < bounds.size() && i < 16; i++) particleSupport[i] = bounds[i];
}

ParticleBatch uploadParticles(za::Span<const ParticleInstance> particles, bool trim)
{
    if(particles.empty())
    {
        return {};
    }
    GLuint buf = 0;
    GLbyte* ofs = nullptr;
    GL_Upload(GL_SHADER_STORAGE_BUFFER, particles.data(), particles.sizeBytes(), &buf, &ofs);
    return {buf, reinterpret_cast<za::SizeT>(ofs), particles.size(), trim};
}

namespace
{

// Lazy, bounded variants: reference, general fast and centre-near fast retro, plus nonretro full/half.
[[nodiscard]] GLuint particleProgramFor(int which)
{
    const int variant = which;
    constexpr int base[11] = {0, 1, 2, 3, 4, 5, 6, 0, 2, 5, 6};
    which = base[variant];
    GLuint& program = particleProgram[variant];
    if(!program && !particleProgramFailed[variant])
    {
        za::String fs = fragmentFor(static_cast<int>(Shade::Texture), true, which == 1 || which >= 3);
        za::String vs = particleVertexShader;
        if(variant >= 7) vs.insert(vs.find('\n') + 1, "#define PARTICLE_TRIM 1\n");
        if(which >= 3)
        {
            fs.insert(fs.find('\n') + 1, "#define PARTICLE_RETRO_GRID 1\n");
            vs.insert(vs.find('\n') + 1, "#define PARTICLE_RETRO_GRID 1\n");
        }
        if(which >= 5)
        {
            fs.insert(fs.find('\n') + 1, "#define PARTICLE_RETRO_CENTRE_NEAR 1\n");
        }
        if(which == 2 || which == 4 || which == 6)
        {
            fs.insert(fs.find('\n') + 1, "#define HALFRES 1\n");
        }
        program = glProgram(vs.cStr(), fs.cStr(),
            which == 0 ? "vr particles" : which == 1 ? "vr particles (retro reference)" :
            which == 2 ? "vr particles (half size)" : which == 3 ? "vr particles (retro fast)" :
            which == 4 ? "vr particles (retro fast half size)" : which == 5 ? "vr particles (retro centre near)" :
            "vr particles (retro centre near half size)");
        particleProgramFailed[variant] = !program;
    }
    return program;
}

// Draws them with `program` into what is bound (blended over it, premultiplied). Not shaded per sample with MSAA
// (vid_fsaamode 1): the textures are soft, and their edges are their alpha.
void drawParticlesWith(GLuint program, const ParticleBatch& batch, bool pull, bool depthTest, int retro,
    Texture texture, ParticlePass pass, const ParticleSplit& split, Texture distances, bool soft, bool half = false)
{
    GL_UseProgram(program);
    GL_SetState(GLS_CULL_NONE | GLS_ATTRIBS(0) | GLS_BLEND_ALPHA | (depthTest ? 0 : GLS_NO_ZTEST) | GLS_NO_ZWRITE);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA); // premultiplied
    glDisable(GL_SAMPLE_SHADING);
    const glm::mat4 mvp = sceneViewProjection();
    glm::vec3 eye, right, up;
    sceneCamera(eye, right, up);
    GL_UniformMatrix4fvFunc(0, 1, GL_FALSE, &mvp[0][0]);
    GL_Uniform1iFunc(4, soft ? 1 : 0);
    GL_Uniform3fFunc(5, eye.x, eye.y, eye.z);
    GL_Uniform3fFunc(6, right.x, right.y, right.z);
    GL_Uniform3fFunc(7, up.x, up.y, up.z);
    GL_Uniform1iFunc(8, pull ? 1 : 0);
    GL_Uniform1iFunc(10, static_cast<int>(pass));
    GL_Uniform1fFunc(11, split.pixelScale);
    GL_Uniform1fFunc(12, split.largePixels);
    const bool trimmed = program == particleProgram[7] || program == particleProgram[8] ||
        program == particleProgram[9] || program == particleProgram[10];
    if(trimmed)
    {
        unsigned trimColor = 0, trimDepth = 0;
        int trimSamples = 0, trimViewport[4];
        VR_SceneTarget(&trimColor, &trimDepth, &trimSamples, trimViewport);
        const int width = half ? (trimViewport[0] + trimViewport[2] + 1) / 2 - trimViewport[0] / 2 : trimViewport[2];
        const int height = half ? (trimViewport[1] + trimViewport[3] + 1) / 2 - trimViewport[1] / 2 : trimViewport[3];
        GL_Uniform4fFunc(13, static_cast<float>(width), static_cast<float>(height), particleAtlasSize.x, particleAtlasSize.y);
        const float block = retro > 0 ? retro::particleTrimBlock(retro) : 0.f;
        GL_Uniform2fFunc(14, block >= 0.f ? 1.f : 0.f, block);
        GL_Uniform4fvFunc(20, 16, &particleSupport[0][0]);
    }
    bindRetro(retro);
    if(texture)
    {
        GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D, texture);
    }
    if(distances)
    {
        GL_BindNative(GL_TEXTURE1, GL_TEXTURE_2D, distances);
    }
    // Binding 0 borrowed (the scene's lights, R_UploadFrameData): put back for what the view draws after.
    GLuint savedBuffer = 0;
    GLintptr savedOffset = 0;
    GLsizeiptr savedSize = 0;
    const bool saved = GL_GetShaderStorageRange(0, &savedBuffer, &savedOffset, &savedSize);
    GL_BindBufferRange(GL_SHADER_STORAGE_BUFFER, 0, batch.buffer, static_cast<GLintptr>(batch.offset),
        static_cast<GLsizeiptr>(batch.count * sizeof(ParticleInstance)));
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(batch.count * 6));
    if(saved && savedBuffer)
    {
        GL_BindBufferRange(GL_SHADER_STORAGE_BUFFER, 0, savedBuffer, savedOffset, savedSize);
    }
    glEnable(GL_SAMPLE_SHADING);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // what GLS_BLEND_ALPHA expects
}

} // namespace

void drawParticles(const ParticleBatch& batch, bool pull, const State& state, Texture texture, ParticlePass pass,
    const ParticleSplit& split)
{
    if(batch.count == 0 || !batch.buffer)
    {
        return;
    }
    const int r = state.retro > 0 ? (vr_particle_retro_fast.value != 0.f ?
        (retro::particleCentreNear(state.retro) ? 5 : 3) : 1) : 0;
    const int trimmed = vr_particle_trim.value != 0.f && batch.trim ? (r == 0 ? 7 : r == 5 ? 9 : r) : r;
    GLuint program = particleProgramFor(trimmed);
    if(!program && trimmed >= 7) program = particleProgramFor(r);
    if(!program && r >= 3)
    {
        program = particleProgramFor(1);
    }
    if(!program)
    {
        return;
    }
    drawParticlesWith(program, batch, pull, state.depthTest, state.retro, texture, pass, split, state.sceneDistances,
        state.sceneDistances != 0);
}

bool drawParticlesHalf(const ParticleBatch& batch, bool pull, Texture texture, const ParticleSplit& split,
    Texture distances, int width, int height, const int viewport[4], bool soft, int retro, void (*restore)())
{
    if(batch.count == 0 || !batch.buffer || !distances || width <= 0 || height <= 0)
    {
        return false;
    }
    const int original = retro > 0 ? (retro::particleCentreNear(retro) ? 6 : 4) : 2;
    const int trimmed = vr_particle_trim.value != 0.f && batch.trim ? (original == 2 ? 8 : original == 6 ? 10 : original) : original;
    GLuint program = particleProgramFor(trimmed);
    if(!program && trimmed >= 7) program = particleProgramFor(original);
    if(!halfCompositeProgram && !halfCompositeFailed)
    {
        halfCompositeProgram = glProgram(halfCompositeVs, halfCompositeFs, "vr particles (half size, blended in)");
        halfCompositeFailed = !halfCompositeProgram;
    }
    if(!program || !halfCompositeProgram)
    {
        return false;
    }

    // Its target: the distances' size (a texel each).
    HalfTarget* target = nullptr;
    for(HalfTarget& t : halfTargets)
    {
        if(t.texture && t.width == width && t.height == height)
        {
            target = &t;
        }
    }
    if(!target)
    {
        target = &halfTargets[halfTargetNext];
        halfTargetNext ^= 1;
        if(target->texture)
        {
            GL_DeleteFramebuffersFunc(1, &target->fbo);
            GL_DeleteNativeTexture(target->texture);
        }
        glGenTextures(1, &target->texture);
        GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D, target->texture);
        GL_TexStorage2DFunc(GL_TEXTURE_2D, 1, GL_RGBA16F, width, height);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
        GL_ObjectLabelFunc(GL_TEXTURE, target->texture, -1, "vr particles (half size)");
        GL_GenFramebuffersFunc(1, &target->fbo);
        GL_BindFramebufferFunc(GL_FRAMEBUFFER, target->fbo);
        GL_FramebufferTexture2DFunc(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, target->texture, 0);
        target->width = width;
        target->height = height;
    }

    // The large ones into it, over nothing.
    GL_BindFramebufferFunc(GL_FRAMEBUFFER, target->fbo);
    glViewport(viewport[0] / 2, viewport[1] / 2,
        (viewport[0] + viewport[2] + 1) / 2 - viewport[0] / 2,
        (viewport[1] + viewport[3] + 1) / 2 - viewport[1] / 2);
    const GLfloat clear[4] = {0.f, 0.f, 0.f, 0.f};
    GL_ClearBufferfvFunc(GL_COLOR, 0, clear);
    drawParticlesWith(program, batch, pull, false, retro, texture, ParticlePass::Large, split, distances, soft, true);
    restore();

    // Blended into the scene.
    GL_UseProgram(halfCompositeProgram);
    GL_SetState(GLS_CULL_NONE | GLS_ATTRIBS(0) | GLS_BLEND_ALPHA | GLS_NO_ZTEST | GLS_NO_ZWRITE);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_SAMPLE_SHADING);
    GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D, target->texture);
    GL_Uniform2fFunc(0, 0.5f / static_cast<float>(width), 0.5f / static_cast<float>(height));
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glEnable(GL_SAMPLE_SHADING);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    return true;
}

TubeBatch uploadTube(za::Span<const TubeRing> rings)
{
    if(rings.size() < 2)
    {
        return {};
    }
    GLuint buf = 0;
    GLbyte* ofs = nullptr;
    GL_Upload(GL_SHADER_STORAGE_BUFFER, rings.data(), rings.sizeBytes(), &buf, &ofs);
    return {buf, reinterpret_cast<za::SizeT>(ofs), rings.size()};
}

void drawTube(const TubeBatch& batch, int sides, const glm::vec3& albedo, const glm::vec3& key, const glm::vec3& rust, bool flat)
{
    if(batch.count < 2 || !batch.buffer || sides < 3)
    {
        return;
    }
    if(!tubeProgram && !tubeProgramFailed)
    {
        const za::String fragment = "#version 430\n#define MODE " + za::toString(static_cast<int>(Shade::Color)) +
                                     "\n#define BLENDED 0\n" + fragmentShader;
        tubeProgram = glProgram(tubeVertexShader, fragment.cStr(), "vr tube");
        tubeProgramFailed = !tubeProgram;
    }
    if(!tubeProgram)
    {
        return;
    }
    GL_UseProgram(tubeProgram);
    GL_SetState(GLS_CULL_NONE | GLS_ATTRIBS(0) | GLS_BLEND_OPAQUE);
    const glm::mat4 mvp = sceneViewProjection();
    glm::vec3 eye, right, up;
    sceneCamera(eye, right, up);
    GL_UniformMatrix4fvFunc(0, 1, GL_FALSE, &mvp[0][0]);
    GL_Uniform1iFunc(4, 0);
    GL_Uniform3fFunc(5, eye.x, eye.y, eye.z);
    GL_Uniform3fFunc(6, albedo.x, albedo.y, albedo.z);
    GL_Uniform3fFunc(7, key.x, key.y, key.z);
    GL_Uniform1iFunc(8, sides);
    GL_Uniform3fFunc(9, rust.x, rust.y, rust.z);
    GL_Uniform1iFunc(10, flat ? 1 : 0);
    // Binding 0 borrowed (the scene's lights, R_UploadFrameData): put back for what the view draws after.
    GLuint savedBuffer = 0;
    GLintptr savedOffset = 0;
    GLsizeiptr savedSize = 0;
    const bool saved = GL_GetShaderStorageRange(0, &savedBuffer, &savedOffset, &savedSize);
    GL_BindBufferRange(GL_SHADER_STORAGE_BUFFER, 0, batch.buffer, static_cast<GLintptr>(batch.offset),
        static_cast<GLsizeiptr>(batch.count * sizeof(TubeRing)));
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>((batch.count - 1) * sides * 6));
    if(saved && savedBuffer)
    {
        GL_BindBufferRange(GL_SHADER_STORAGE_BUFFER, 0, savedBuffer, savedOffset, savedSize);
    }
}

BentBatch uploadBent(za::Span<const glm::vec4> data)
{
    if(data.empty())
    {
        return {};
    }
    GLuint buf = 0;
    GLbyte* ofs = nullptr;
    GL_Upload(GL_SHADER_STORAGE_BUFFER, data.data(), data.sizeBytes(), &buf, &ofs);
    return {buf, reinterpret_cast<za::SizeT>(ofs), data.size()};
}

void drawBent(const BentBatch& batch, const BentDraw& d)
{
    if(!batch.buffer || d.samples < 2 || d.meshVertices < 3 || d.copies < 1 || !d.skin)
    {
        return;
    }
    if(!bentProgram && !bentProgramFailed)
    {
        bentProgram = glProgram(bentVertexShader, bentFragmentShader, "vr bent mesh");
        bentProgramFailed = !bentProgram;
    }
    if(!bentProgram)
    {
        return;
    }
    GL_UseProgram(bentProgram);
    GL_SetState(GLS_CULL_BACK | GLS_ATTRIBS(0) | GLS_BLEND_OPAQUE);
    const glm::mat4 mvp = sceneViewProjection();
    glm::vec3 eye, right, up;
    sceneCamera(eye, right, up);
    GL_UniformMatrix4fvFunc(0, 1, GL_FALSE, &mvp[0][0]);
    GL_Uniform3fFunc(1, eye.x, eye.y, eye.z);
    GL_Uniform4fFunc(2, static_cast<float>(d.meshFirst), static_cast<float>(d.meshVertices), static_cast<float>(d.curveFirst),
        static_cast<float>(d.samples));
    GL_Uniform2fFunc(3, d.period, d.scale);
    GL_Uniform3fFunc(4, d.light.x, d.light.y, d.light.z);
    GL_Uniform4fvFunc(5, 1, r_framedata.fogdata);
    GL_Uniform1fFunc(6, r_framedata.scenetone[0] > 0.f ? r_framedata.scenetone[0] : 1.f);
    GL_Uniform1iFunc(7, d.fullbright ? 1 : 0);
    GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D, d.skin);
    if(d.fullbright)
    {
        GL_BindNative(GL_TEXTURE1, GL_TEXTURE_2D, d.fullbright);
    }
    // Binding 0 borrowed (the scene's lights, R_UploadFrameData): put back for what the view draws after.
    GLuint savedBuffer = 0;
    GLintptr savedOffset = 0;
    GLsizeiptr savedSize = 0;
    const bool saved = GL_GetShaderStorageRange(0, &savedBuffer, &savedOffset, &savedSize);
    GL_BindBufferRange(GL_SHADER_STORAGE_BUFFER, 0, batch.buffer, static_cast<GLintptr>(batch.offset),
        static_cast<GLsizeiptr>(batch.count * sizeof(glm::vec4)));
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(d.copies * d.meshVertices));
    if(saved && savedBuffer)
    {
        GL_BindBufferRange(GL_SHADER_STORAGE_BUFFER, 0, savedBuffer, savedOffset, savedSize);
    }
}

glm::mat4 sceneViewProjection()
{
    glm::mat4 viewProj;
    memcpy(&viewProj[0][0], r_matviewproj, sizeof(r_matviewproj));
    return viewProj;
}

void sceneCamera(glm::vec3& origin, glm::vec3& right, glm::vec3& up)
{
    origin = {r_refdef.vieworg[0], r_refdef.vieworg[1], r_refdef.vieworg[2]};
    right = {vright[0], vright[1], vright[2]};
    up = {vup[0], vup[1], vup[2]};
}

Texture fontTexture()
{
    return char_texture ? char_texture->texnum : 0;
}

// Ironwail's console font atlas: 16 x 16 cells of 10 x 10 texels, each an 8 x 8 glyph with a
// one-texel border.
glm::vec4 fontGlyph(unsigned char c)
{
    constexpr float atlas = 160.f;
    const float u0 = ((c & 15) * 10 + 1) / atlas;
    const float v0 = ((c >> 4) * 10 + 1) / atlas;
    return {u0, v0, u0 + 8.f / atlas, v0 + 8.f / atlas};
}

int targetsMade = 0; // targets' textures (re)made so far (vr_memstats: steady in play, not a frame)
struct TargetMade
{
    const char* name;
    int count;
};
za::Vector<TargetMade> targetsMadeNamed; // by name, in the order first made

za::String targetsMadeByName()
{
    za::String out;
    for(const auto& [name, count] : targetsMadeNamed)
    {
        out += (out.empty() ? "" : " ") + za::String{name} + ":" + za::toString(count);
    }
    return out;
}

void releaseTarget(Target& target)
{
    GLuint texture = target.texture;
    if(texture)
    {
        glDeleteTextures(1, &texture);
    }
    GLuint fbo = target.framebuffer;
    if(fbo)
    {
        GL_DeleteFramebuffersFunc(1, &fbo);
    }
    target = Target{};
}

void ensureTarget(Target& target, int width, int height, bool mipmaps, const char* name)
{
    int levels = 1;
    while(mipmaps && za::max(width, height) >> levels)
    {
        levels++;
    }
    if(target.texture && target.width == width && target.height == height && target.levels == levels)
    {
        return;
    }
    targetsMade++;
    auto named = za::findIf(targetsMadeNamed.begin(), targetsMadeNamed.end(),
        [&](const TargetMade& n) { return ZA_STRCMP(n.name, name) == 0; });
    if(named == targetsMadeNamed.end())
    {
        targetsMadeNamed.pushBack({name, 0});
        named = targetsMadeNamed.end() - 1;
    }
    named->count++;
    Con_DPrintf("VR: render target \"%s\" made at %dx%d (was %dx%d), %d times so far\n", name, width, height,
        target.width, target.height, named->count);

    GLuint texture = target.texture;
    if(texture)
    {
        glDeleteTextures(1, &texture);
    }
    GLuint fbo = target.framebuffer;
    if(!fbo)
    {
        GL_GenFramebuffersFunc(1, &fbo);
    }

    glGenTextures(1, &texture);
    GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D, texture);
    GL_TexStorage2DFunc(GL_TEXTURE_2D, levels, GL_RGBA8, width, height);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, levels > 1 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    GLint previous = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &previous);
    GL_BindFramebufferFunc(GL_FRAMEBUFFER, fbo);
    GL_FramebufferTexture2DFunc(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
    GL_BindFramebufferFunc(GL_FRAMEBUFFER, static_cast<GLuint>(previous));

    target = {texture, fbo, width, height, levels};
}

void begin2D(const Target& target, int virtualWidth, int virtualHeight)
{
    Draw_Flush();

    saved2D.active = true;
    saved2D.canvas = glcanvas;
    saved2D.mipmapped = target.levels > 1 ? target.texture : 0;

    GL_BindFramebufferFunc(GL_FRAMEBUFFER, target.framebuffer);
    glViewport(0, 0, target.width, target.height);

    // A canvas of its own: the virtual screen over the whole target, y down.
    glcanvas.type = CANVAS_INVALID;
    glcanvas.transform.scale[0] = 2.f / virtualWidth;
    glcanvas.transform.scale[1] = -2.f / virtualHeight;
    glcanvas.transform.offset[0] = -1.f;
    glcanvas.transform.offset[1] = 1.f;
    Draw_GetTransformBounds(&glcanvas.transform, &glcanvas.left, &glcanvas.top, &glcanvas.right, &glcanvas.bottom);
    glcanvas.blendmode = GLS_BLEND_ALPHA;
    GL_SetCanvasColor(1.f, 1.f, 1.f, 1.f);
}

void end2D()
{
    if(!saved2D.active)
    {
        return;
    }
    Draw_Flush();
    saved2D.active = false;

    glcanvas = saved2D.canvas;
    bindWindow();

    // Its mipmaps, for Shade::Screen's glow (a few small textures: next to nothing).
    if(saved2D.mipmapped)
    {
        GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D, saved2D.mipmapped);
        GL_GenerateMipmapFunc(GL_TEXTURE_2D);
        saved2D.mipmapped = 0;
    }
}

namespace draw2D
{

void color(const glm::vec4& rgba)
{
    GL_SetCanvasColor(rgba.r, rgba.g, rgba.b, rgba.a);
}

void fill(float x, float y, float w, float h, const glm::vec3& rgb)
{
    const float c[3] = {rgb.r, rgb.g, rgb.b};
    Draw_FillEx(x, y, w, h, c, 1.f);
}

void pic(float x, float y, const char* wadName, float scale)
{
    if(qpic_t* p = picNamed(wadName))
    {
        Draw_SubPic(x, y, p->width * scale, p->height * scale, p, 0.f, 0.f, 1.f, 1.f, rgb_white, 1.f);
    }
}

void text(float x, float y, float size, const char* str)
{
    Draw_StringEx(x, y, size, str);
}

} // namespace draw2D

void beginCanvas(Target& canvas, int width, int height)
{
    ensureTarget(canvas, width, height, false, "panel canvas");
    GL_ResetState(); // re-applies the blend, now with applyCanvasBlend() (VR_CanvasBlend)
    GL_BindFramebufferFunc(GL_FRAMEBUFFER, canvas.framebuffer);
    glViewport(0, 0, width, height);
    glClearColor(0.f, 0.f, 0.f, 0.f);
    glClear(GL_COLOR_BUFFER_BIT);
}

void endCanvas()
{
    GL_ResetState(); // back to Ironwail's usual blend
    bindWindow();
}

bool applyCanvasBlend()
{
    if(!blendFuncSeparate)
    {
        blendFuncSeparate = reinterpret_cast<BlendFuncSeparateFn>(SDL_GL_GetProcAddress("glBlendFuncSeparate"));
        if(!blendFuncSeparate)
        {
            return false;
        }
    }
    blendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    return true;
}

void copy(const Target& target, Texture image)
{
    if(!copyFbo)
    {
        GL_GenFramebuffersFunc(1, &copyFbo);
    }

    GL_BindFramebufferFunc(GL_DRAW_FRAMEBUFFER, copyFbo);
    GL_FramebufferTexture2DFunc(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, image, 0);
    GL_BindFramebufferFunc(GL_READ_FRAMEBUFFER, target.framebuffer);
    GL_BlitFramebufferFunc(0, 0, target.width, target.height, 0, 0, target.width, target.height, GL_COLOR_BUFFER_BIT,
        GL_NEAREST);
    GL_FramebufferTexture2DFunc(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);

    bindWindow(); // it is called with the window's 2D pass bound
}

Texture createTexture(int width, int height, const void* rgba, bool mipmaps)
{
    mipmaps = mipmaps && rgba;
    int levels = 1;
    while(mipmaps && za::max(width, height) >> levels)
    {
        levels++;
    }

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    GL_TexStorage2DFunc(GL_TEXTURE_2D, levels, GL_RGBA8, width, height);
    if(rgba)
    {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
        if(mipmaps)
        {
            GL_GenerateMipmapFunc(GL_TEXTURE_2D);
            if(gl_max_anisotropy > 1.f) // else not supported
            {
                glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, za::min(16.f, gl_max_anisotropy));
            }
        }
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, mipmaps ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    GL_ClearBindings();
    return tex;
}

void destroyTexture(Texture texture)
{
    GLuint tex = texture;
    glDeleteTextures(1, &tex);
    GL_ClearBindings();
}

} // namespace qvr::gfx

namespace
{

// VR_BindOpaqueScene's resolve: the r_framecount it is of (once a view).
struct OpaqueSceneResolve
{
    int frame = -1;
};
OpaqueSceneResolve opaqueSceneResolve;

} // namespace

// The opaque scene's colours, which translucent liquids read to bend what is behind them (vr_water.cpp): only while
// they draw into the OIT buffers (the scene's colours are not a target then). With multisampling, the resolved scene's
// texture: VR_BindOpaqueScene resolves the scene into it before they read it.
extern "C" unsigned VR_OpaqueSceneTexture(void)
{
    if(R_GetEffectiveAlphaMode() != ALPHAMODE_OIT)
    {
        return 0;
    }
    if(framebufs.scene.samples > 1)
    {
        return framebufs.resolved_scene.color_tex;
    }
    if(GL_NeedsSceneEffects())
    {
        return framebufs.scene.color_tex;
    }
    return GL_NeedsPostprocess() ? framebufs.composite.color_tex : 0;
}

// VR_OpaqueSceneTexture on unit 6, for the translucent liquids' refraction (r_world.c), readable: with multisampling
// and the refraction on (vr_water_refraction: r_framedata.water[2]) the scene is resolved into it first, once a view
// (the translucent pass draws into the OIT buffers, so it stays the opaque scene; R_WarpScaleView's resolve of the
// whole scene may reuse the texture after the pass), and the translucent pass's target bound again.
extern "C" void VR_BindOpaqueScene(void)
{
    const GLuint tex = VR_OpaqueSceneTexture();
    if(tex && framebufs.scene.samples > 1 && r_framedata.water[2] > 0.f && opaqueSceneResolve.frame != r_framecount)
    {
        GLuint color, depth;
        int samples, viewport[4];
        VR_ProfileBeginGPU("refraction resolve");
        GL_BeginGroup("MSAA resolve (refraction)");
        GL_BindFramebufferFunc(GL_READ_FRAMEBUFFER, VR_SceneTarget(&color, &depth, &samples, viewport));
        GL_BindFramebufferFunc(GL_DRAW_FRAMEBUFFER, framebufs.resolved_scene.fbo);
        GL_BlitFramebufferFunc(0, 0, viewport[2], viewport[3], 0, 0, viewport[2], viewport[3], GL_COLOR_BUFFER_BIT,
            GL_NEAREST);
        R_RestoreTranslucentTarget();
        GL_EndGroup();
        VR_ProfileEnd();
        opaqueSceneResolve.frame = r_framecount;
    }
    GL_BindNative(GL_TEXTURE6, GL_TEXTURE_2D, tex);
}

// The framebuffer the scene is drawn into (R_SetupGL's), its colour and depth/stencil textures (multisampled with
// samples > 1) and its viewport: vr_water.cpp's scene distances (the shoreline foam: any alpha mode, with
// multisampling too), vr_haze.cpp's copy of the scene and vr_foveated.cpp. 0 and no textures: the window's own.
extern "C" unsigned VR_SceneTarget(unsigned* color, unsigned* depth, int* samples, int viewport[4])
{
    if(GL_NeedsSceneEffects())
    {
        *color = framebufs.scene.color_tex;
        *depth = framebufs.scene.depth_stencil_tex;
        *samples = framebufs.scene.samples;
        viewport[0] = viewport[1] = 0;
        viewport[2] = r_refdef.vrect.width / r_refdef.scale;
        viewport[3] = r_refdef.vrect.height / r_refdef.scale;
        return framebufs.scene.fbo;
    }
    *samples = 1;
    viewport[0] = glx + r_refdef.vrect.x;
    viewport[1] = gly + glheight - r_refdef.vrect.y - r_refdef.vrect.height;
    viewport[2] = r_refdef.vrect.width;
    viewport[3] = r_refdef.vrect.height;
    if(!GL_NeedsPostprocess())
    {
        *color = *depth = 0;
        return 0;
    }
    *color = framebufs.composite.color_tex;
    *depth = framebufs.composite.depth_stencil_tex;
    return framebufs.composite.fbo;
}
