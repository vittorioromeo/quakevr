// vr_bloom.cpp -- see vr_bloom.hpp.

#include "vr_bloom.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_gfx.hpp"
#include "vr_glutil.hpp"
#include "vr_profile.hpp"
#include "vr_stereo.hpp"

#include "Zancle/Base/InitializerList.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "vr_api_render.h"


namespace qvr::bloom
{
namespace
{

using Target = gfx::RenderTarget;

// The chain: down[0] the bright pass at a quarter of the scene, down[1..2] an eighth and a sixteenth;
// up[1..0] back up to a quarter, each the level below it spread out plus its own down level; mean: how
// much of the view glows, one texel (ROUND22.md, "Cheaper bloom": a thirty-second level once, its
// weight now the sixteenth's).
constexpr int levels = 3;
// One for the eyes, one for the window's spectator camera (vr_window.cpp), one for flatscreen play's window
// (vr_flatlook.cpp): each keeps its size.
struct Chain
{
    Target down[levels];
    Target up[levels - 1];
    Target mean;
};
Chain chains[3];
GLuint brightProgram = 0;
GLuint downProgram = 0;
GLuint upProgram = 0;
GLuint meanProgram = 0;
// The bright pass's sampler: its four taps are bilinear by design (each on a texel corner, the 4x4 block in four
// reads), and the scene it reads is made GL_NEAREST (gl_rmain.c's composite): without it each tap read one texel.
GLuint linearSampler = 0;
bool failed = false;

// This eye's result, for GL_PostProcess (VR_PostProcessBloom), and its sizes (drawn width, height, its texture's).
GLuint resultTex = 0;
glm::vec4 resultView{1.f};
bool resultValid = false;

// How much each level adds: the quarter a tight halo, the smaller ones a wide soft glow (the sixteenth 1 + 0.4: the
// thirty-second's that was).
constexpr float levelWeight[levels] = {0.5f, 0.8f, 1.4f};

// vr_profile_detail 2: each pass timed apart.
constexpr const char* downNames[levels] = {"", "bloom down 1", "bloom down 2"};
constexpr const char* upNames[levels - 1] = {"bloom up 0", "bloom up 1"};

constexpr const char* fullscreenVs = gfx::fullscreenTriangleVs;

// Down to a quarter: 16 texels by four bilinear taps, keeping what is over the threshold. The scene
// is low dynamic range (a lamp is 1, not 10; with vr_tonemap up to 8, taken as at most 2 here), so
// the response rises steeply towards white: lamps and glowing panels glow, merely bright walls
// hardly. White and pale light glows by vr_bloom_white, coloured light (red buttons, blue panels) by
// vr_bloom_color, blended by how saturated it is.
constexpr const char* brightFs = R"(#version 430
layout(binding = 0) uniform sampler2D Scene;
layout(location = 0) uniform vec4 Params; // threshold, 0, 1 / scene width, 1 / scene height
layout(location = 1) uniform vec2 Weights; // white, coloured
layout(location = 0) out vec4 Out;
void main()
{
    vec2 uv = (gl_FragCoord.xy * 4.0) * Params.zw;
    vec2 t = Params.zw;
    vec3 c = max(max(texture(Scene, uv + vec2(-t.x, -t.y)).rgb, texture(Scene, uv + vec2(t.x, -t.y)).rgb),
                 max(texture(Scene, uv + vec2(-t.x, t.y)).rgb, texture(Scene, uv + vec2(t.x, t.y)).rgb));
    // An Inf texel (the float scene overflowed: RGBA16F or R11F_G11F_B10F, many additive lights on one spot) made finite
    // first: below, Inf * (2 / Inf) and Inf / Inf are NaN, which no clamp is defined to remove (GLSL). 65504: half's largest.
    c = min(c, vec3(65504.0));
    float bright = max(c.r, max(c.g, c.b));
    c *= min(bright, 2.0) / max(bright, 1e-3); // the float scene (vr_tonemap) glows by at most 2: no flicker from specks
    bright = min(bright, 2.0);
    float x = clamp((bright - Params.x) / max(1.0 - Params.x, 1e-3), 0.0, 1.0);
    float saturation = (bright - min(c.r, min(c.g, c.b))) / max(bright, 1e-3);
    float weight = mix(Weights.x, Weights.y, smoothstep(0.15, 0.6, saturation));
    Out = vec4(clamp(c * (x * x * 4.0 * weight), 0.0, 1e4), 1.0); // no Inf spreads down the chain
}
)";

// Half the size, dual-filter style (Bjorge, "Bandwidth-efficient rendering", SIGGRAPH 2015): the
// centre and four diagonal bilinear taps, half a source texel out (times the spread).
constexpr const char* downFs = R"(#version 430
layout(binding = 0) uniform sampler2D Source;
layout(location = 0) uniform vec4 Params; // spread, 0, 1 / source width, 1 / source height (its texture's)
layout(location = 1) uniform vec4 Drawn;  // the source's drawn part's texels' centres, in its uv (lo xy, hi xy)
layout(location = 0) out vec4 Out;
vec3 Tap(vec2 uv) { return texture(Source, clamp(uv, Drawn.xy, Drawn.zw)).rgb; } // (the drawn part's edge, as clamp-to-edge)
void main()
{
    vec2 uv = (gl_FragCoord.xy * 2.0) * Params.zw;
    vec2 o = Params.zw * Params.x;
    vec3 c = Tap(uv) * 4.0;
    c += Tap(uv + vec2(-o.x, -o.y)) + Tap(uv + vec2(o.x, -o.y)) + Tap(uv + vec2(-o.x, o.y)) + Tap(uv + vec2(o.x, o.y));
    Out = vec4(clamp(c * 0.125, 0.0, 1e4), 1.0);
}
)";

// Twice the size, progressively (Jimenez, "Next generation post processing in Call of Duty: Advanced
// Warfare", SIGGRAPH 2014): the smaller level spread by a 3 x 3 tent of bilinear taps, plus this
// level's own down level by its weight. The last one (to a quarter) also takes the strength, less
// the more of the view glows (vr_bloom_adapt: the mean texel), so a brightly lit map is not washed
// out while lamps in dark rooms keep their glow.
constexpr const char* upFs = R"(#version 430
layout(binding = 0) uniform sampler2D Smaller;
layout(binding = 1) uniform sampler2D Own;
layout(binding = 2) uniform sampler2D Mean;
layout(location = 0) uniform vec4 Params; // spread, own weight, 1 / smaller width, 1 / smaller height
layout(location = 1) uniform vec3 Extra;  // the smaller level's weight, strength (0: not the last), adapt
layout(location = 2) uniform float Soft;  // the last level: the side weight of a [Soft, 1 - 2 Soft, Soft] blur
layout(location = 3) uniform vec2 SmallerScale; // Own's uv to Smaller's (their drawn parts over their textures: 1 1 when whole)
layout(location = 4) uniform vec4 SmallerDrawn; // Smaller's drawn part's texels' centres, in its uv (lo xy, hi xy)
layout(location = 5) uniform vec4 OwnDrawn;     // ... Own's
layout(location = 0) out vec4 Out;
vec3 TapSmaller(vec2 uv) { return texture(Smaller, clamp(uv, SmallerDrawn.xy, SmallerDrawn.zw)).rgb; }
vec3 TapOwn(vec2 uv) { return texture(Own, clamp(uv, OwnDrawn.xy, OwnDrawn.zw)).rgb; }
void main()
{
    vec2 uv = gl_FragCoord.xy / vec2(textureSize(Own, 0));
    vec2 su = uv * SmallerScale;
    vec2 o = Params.zw * Params.x;
    if(Soft > 0.0) // the tent wider by the blur's variance (2 Soft own texels squared; the tent's: 2 spread squared)
        o *= sqrt(1.0 + Soft / (Params.x * Params.x));
    vec3 c = TapSmaller(su) * 4.0;
    c += (TapSmaller(su + vec2(-o.x, 0.0)) + TapSmaller(su + vec2(o.x, 0.0)) + TapSmaller(su + vec2(0.0, -o.y)) +
          TapSmaller(su + vec2(0.0, o.y))) * 2.0;
    c += TapSmaller(su + vec2(-o.x, -o.y)) + TapSmaller(su + vec2(o.x, -o.y)) + TapSmaller(su + vec2(-o.x, o.y)) +
         TapSmaller(su + vec2(o.x, o.y));
    vec3 own;
    if(Soft > 0.0) // four bilinear taps 2 Soft texels out: the blur, in both directions
    {
        vec2 t = 2.0 * Soft / vec2(textureSize(Own, 0));
        own = (TapOwn(uv + vec2(-t.x, -t.y)) + TapOwn(uv + vec2(t.x, -t.y)) + TapOwn(uv + vec2(-t.x, t.y)) +
               TapOwn(uv + vec2(t.x, t.y))) * 0.25;
    }
    else
    {
        own = texelFetch(Own, ivec2(gl_FragCoord.xy), 0).rgb;
    }
    c = c * (Extra.x / 16.0) + own * Params.y;
    if(Extra.y > 0.0)
    {
        c *= Extra.y / (1.0 + Extra.z * texelFetch(Mean, ivec2(0), 0).r);
    }
    Out = vec4(clamp(c, 0.0, 1e4), 1.0);
}
)";

// How much of the view glows: the brightest channel of the smallest level, averaged over an 8 x 8
// grid of its drawn part (one texel, once per eye).
constexpr const char* meanFs = R"(#version 430
layout(binding = 0) uniform sampler2D Source;
layout(location = 0) uniform vec4 Params; // xy: the source's drawn part over its texture (1 1 when whole)
layout(location = 0) out vec4 Out;
void main()
{
    float sum = 0.0;
    for(int y = 0; y < 8; y++)
        for(int x = 0; x < 8; x++)
        {
            vec3 s = texture(Source, ((vec2(x, y) + 0.5) * 0.125) * Params.xy).rgb;
            sum += max(s.r, max(s.g, s.b));
        }
    Out = vec4(clamp(sum * (1.0 / 64.0), 0.0, 1e4), 0.0, 0.0, 1.0);
}
)";

[[nodiscard]] bool ensurePrograms()
{
    if(brightProgram || failed)
    {
        return !failed;
    }
    brightProgram = gfx::glProgram(fullscreenVs, brightFs, "vr bloom bright pass");
    downProgram = gfx::glProgram(fullscreenVs, downFs, "vr bloom down");
    upProgram = gfx::glProgram(fullscreenVs, upFs, "vr bloom up");
    meanProgram = gfx::glProgram(fullscreenVs, meanFs, "vr bloom mean");
    failed = !brightProgram || !downProgram || !upProgram || !meanProgram;
    for(GLuint* p : {&brightProgram, &downProgram, &upProgram, &meanProgram})
    {
        gfx::ownDeferred(*p, &failed);
    }
    return !failed;
}

void destroy(Target& t)
{
    t.destroy();
}

[[nodiscard]] bool ensure(Target& t, int width, int height, const char* name)
{
    if(!t.ensure(width, height, gfx::RenderTargetFormat{.internalFormat = GL_R11F_G11F_B10F}, name))
    {
        Con_Warning("VR bloom: framebuffer incomplete\n");
        failed = true;
        return false;
    }
    return true;
}

// A level's drawn part: its bottom left w x h of the target (all of it but under dynamic resolution, whose eyes'
// targets, and so the chain, are allocated at its ceiling: stereo::ViewSize).
struct Level
{
    const Target* target = nullptr;
    int w = 0, h = 0;
};

// Uniform `location`: the drawn part's texels' centres in the level's uv (lo xy, hi xy): a tap clamped to them reads
// what clamp-to-edge read of the whole texture when they were one.
void drawnUniform(GLint location, const Level& l)
{
    const float tw = static_cast<float>(l.target->width), th = static_cast<float>(l.target->height);
    GL_Uniform4fFunc(location, 0.5f / tw, 0.5f / th, (static_cast<float>(l.w) - 0.5f) / tw, (static_cast<float>(l.h) - 0.5f) / th);
}

// One full-screen triangle of prog into the drawn part of `level`, source on unit 0; Params (location 0) set.
void pass(const Level& level, GLuint prog, GLuint source, float a, float b, float c, float d)
{
    GL_BindFramebufferFunc(GL_FRAMEBUFFER, level.target->fbo);
    glViewport(0, 0, level.w, level.h);
    GL_UseProgram(prog);
    GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D, source);
    GL_Uniform4fFunc(0, a, b, c, d);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

// The blur the last level takes for the eyes' one tap (GL_PostProcess), its side weight. The four
// taps' kernel (a texel's centre weighs 1/2 wherever the pixel lies in it, its neighbours the rest by distance) is
// matched closest, in squares, by a [0.2, 0.6, 0.2] blur followed by the bilinear tap.
constexpr float oneTapSoft = 0.2f;

// vr_profile_detail 2: each pass of the chain timed apart (Debug > Profiling).
bool detailScopes = false;

void scopeBegin(const char* name)
{
    if(detailScopes)
    {
        profile::begin(name, true);
    }
}

void scopeEnd()
{
    if(detailScopes)
    {
        profile::end();
    }
}

// The chain's passes for sceneTex, whose texture is allocatedWidth x allocatedHeight and of which the bottom left
// drawnWidth x drawnHeight is the view, with `strength`: the glow (a quarter of the drawn part, in a texture of a quarter
// of the allocated size: `view` their sizes), 0 for none. The chain is made for the allocated size (not made again as
// dynamic resolution's drawn part changes) and draws the drawn part's.
[[nodiscard]] GLuint run(Chain& chain, GLuint sceneTex, const stereo::ViewSize& size, float strength, glm::vec4& view)
{
    if(strength <= 0.f || size.drawnWidth < 128 || size.drawnHeight < 128 || !ensurePrograms())
    {
        return 0;
    }
    Target* const down = chain.down;
    Target* const up = chain.up;
    Target& mean = chain.mean;
    Level downs[levels], ups[levels - 1];
    for(int l = 0; l < levels; l++)
    {
        const int w = za::max(1, size.allocatedWidth >> (2 + l));
        const int h = za::max(1, size.allocatedHeight >> (2 + l));
        if(!ensure(down[l], w, h, "vr bloom down") || (l < levels - 1 && !ensure(up[l], w, h, "vr bloom up")))
        {
            return 0;
        }
        const int dw = za::max(1, size.drawnWidth >> (2 + l));
        const int dh = za::max(1, size.drawnHeight >> (2 + l));
        downs[l] = {&down[l], dw, dh};
        if(l < levels - 1)
        {
            ups[l] = {&up[l], dw, dh};
        }
    }
    if(!ensure(mean, 1, 1, "vr bloom mean"))
    {
        return 0;
    }

    detailScopes = vr_profile_detail.value >= 2.f;
    GL_BeginGroup("VR bloom");
    GL_SetState(GLS_BLEND_OPAQUE | GLS_NO_ZTEST | GLS_NO_ZWRITE | GLS_CULL_NONE | GLS_ATTRIBS(0));

    const float threshold = za::clamp(vr_bloom_threshold.value, 0.f, 0.99f);
    const float spread = za::clamp(vr_bloom_radius.value, 0.25f, 4.f);

    scopeBegin("bloom bright");
    if(!linearSampler)
    {
        GL_GenSamplersFunc(1, &linearSampler);
        GL_SamplerParameteriFunc(linearSampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        GL_SamplerParameteriFunc(linearSampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        GL_SamplerParameteriFunc(linearSampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        GL_SamplerParameteriFunc(linearSampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    GL_UseProgram(brightProgram);
    GL_Uniform2fFunc(1, za::max(0.f, vr_bloom_white.value), za::max(0.f, vr_bloom_color.value));
    GL_BindSamplerFunc(0, linearSampler);
    // (the scene's texels by their own size: its taps stay within the drawn part's quarters)
    pass(downs[0], brightProgram, sceneTex, threshold, 0.f, 1.f / size.allocatedWidth, 1.f / size.allocatedHeight);
    GL_BindSamplerFunc(0, 0);
    scopeEnd();
    for(int l = 1; l < levels; l++)
    {
        const Level& source = downs[l - 1];
        scopeBegin(downNames[l]);
        GL_UseProgram(downProgram);
        drawnUniform(1, source);
        pass(downs[l], downProgram, source.target->tex, spread, 0.f, 1.f / source.target->width, 1.f / source.target->height);
        scopeEnd();
    }
    scopeBegin("bloom mean");
    const Level& smallest = downs[levels - 1];
    pass({&mean, 1, 1}, meanProgram, smallest.target->tex, static_cast<float>(smallest.w) / static_cast<float>(smallest.target->width),
        static_cast<float>(smallest.h) / static_cast<float>(smallest.target->height), 0.f, 0.f);
    scopeEnd();

    GL_BindNative(GL_TEXTURE2, GL_TEXTURE_2D, mean.tex);
    // The smallest level has no up level of its own: it comes in by its weight in the first up pass.
    for(int l = levels - 2; l >= 0; l--)
    {
        const bool first = l == levels - 2;
        const Level& smaller = first ? downs[levels - 1] : ups[l + 1];
        const Level& own = ups[l];
        GL_UseProgram(upProgram);
        GL_BindNative(GL_TEXTURE1, GL_TEXTURE_2D, down[l].tex);
        GL_Uniform3fFunc(1, first ? levelWeight[levels - 1] : 1.f, l == 0 ? strength : 0.f, za::max(0.f, vr_bloom_adapt.value));
        GL_Uniform1fFunc(2, l == 0 ? oneTapSoft : 0.f);
        // Own's uv to Smaller's: own allocated / own drawn * smaller drawn / smaller allocated (1 when both are whole)
        const auto scale = [&](int ownAllocated, int ownDrawn, int smallerDrawn, int smallerAllocated)
        {
            return static_cast<float>((static_cast<double>(ownAllocated) * smallerDrawn) /
                                      (static_cast<double>(ownDrawn) * smallerAllocated));
        };
        GL_Uniform2fFunc(3, scale(own.target->width, own.w, smaller.w, smaller.target->width),
            scale(own.target->height, own.h, smaller.h, smaller.target->height));
        drawnUniform(4, smaller);
        drawnUniform(5, downs[l]); // (Own: this level's down texture, the same size as the up one)
        scopeBegin(upNames[l]);
        pass(own, upProgram, smaller.target->tex, spread, levelWeight[l], 1.f / smaller.target->width, 1.f / smaller.target->height);
        scopeEnd();
    }

    GL_EndGroup();
    const Level& result = ups[0];
    view = {static_cast<float>(result.w), static_cast<float>(result.h), static_cast<float>(result.target->width),
        static_cast<float>(result.target->height)};
    return up[0].tex;
}

} // namespace

void apply(GLuint sceneTex, const stereo::ViewSize& size)
{
    QVR_GPU_PROFILE("bloom");
    resultTex = run(chains[stereo::isSpectator() ? 1 : 0], sceneTex, size, vr_bloom.value, resultView);
    resultValid = resultTex != 0;
}

GLuint applyFlat(GLuint sceneTex, int width, int height, float strength)
{
    QVR_GPU_PROFILE("flat bloom");
    glm::vec4 view{0.f};
    return run(chains[2], sceneTex, stereo::ViewSize{width, height, width, height}, strength, view);
}

bool result(unsigned& texture, glm::vec4& view)
{
    texture = resultTex;
    view = resultView;
    return resultValid;
}

void shutdown()
{
    for(Chain& chain : chains)
    {
        for(Target& t : chain.down)
        {
            destroy(t);
        }
        for(Target& t : chain.up)
        {
            destroy(t);
        }
        destroy(chain.mean);
    }
    for(GLuint* p : {&brightProgram, &downProgram, &upProgram, &meanProgram})
    {
        if(*p)
        {
            GL_DeleteProgramFunc(*p);
            *p = 0;
        }
    }
    if(linearSampler)
    {
        GL_DeleteSamplersFunc(1, &linearSampler);
        linearSampler = 0;
    }
    resultValid = false;
    failed = false;
}

void releaseTargets()
{
    for(int c = 0; c < 2; c++) // (not flatscreen play's window's)
    {
        for(Target& t : chains[c].down)
        {
            destroy(t);
        }
        for(Target& t : chains[c].up)
        {
            destroy(t);
        }
        destroy(chains[c].mean);
    }
    resultValid = false;
}


// Its GL programs made now (gfx::precompilePrograms, at start-up), as their first use would: no compile in play.
void precompilePrograms()
{
    (void)ensurePrograms();
}

} // namespace qvr::bloom

// GL_PostProcess: binds this eye's glow to texture unit 2 and gives how much of it to add (0: none,
// the window's own post-processing; 1: by one tap).
extern "C" float VR_PostProcessBloom()
{
    unsigned texture = 0;
    glm::vec4 view{1.f};
    if(!VR_RenderingEye() || !qvr::bloom::result(texture, view))
    {
        return 0.f;
    }
    const glm::vec4 map = qvr::stereo::viewUvMap(static_cast<int>(view.x), static_cast<int>(view.y), static_cast<int>(view.z),
        static_cast<int>(view.w));
    GL_Uniform4fFunc(13, map.x, map.y, map.z, map.w); // (BloomMap)
    GL_BindNative(GL_TEXTURE2, GL_TEXTURE_2D, texture);
    return 1.f;
}
