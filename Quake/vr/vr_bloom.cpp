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


namespace qvr::bloom
{
namespace
{

using Target = gfx::RenderTarget;

// The chain: down[0] the bright pass at a quarter of the scene, down[1..3] an eighth, a sixteenth,
// a thirty-second; up[2..0] back up to a quarter, each the level below it spread out plus its own
// down level; mean: how much of the view glows, one texel. vr_bloom_fast stops at the sixteenth
// (fastLevels), which takes the thirty-second's weight too.
constexpr int levels = 4;
constexpr int fastLevels = 3;
// One for the eyes, one for the window's spectator camera (vr_window.cpp): each keeps its size.
struct Chain
{
    Target down[levels];
    Target up[levels - 1];
    Target mean;
};
Chain chains[2];
GLuint brightProgram = 0;
GLuint downProgram = 0;
GLuint upProgram = 0;
GLuint meanProgram = 0;
bool failed = false;

// This eye's result, for GL_PostProcess (VR_PostProcessBloom).
GLuint resultTex = 0;
bool resultValid = false;

// How much each level adds: the quarter a tight halo, the smaller ones a wide soft glow.
constexpr float levelWeight[levels] = {0.5f, 0.8f, 1.f, 0.4f};

// vr_profile_detail 2: each pass timed apart.
constexpr const char* downNames[levels] = {"", "bloom down 1", "bloom down 2", "bloom down 3"};
constexpr const char* upNames[levels - 1] = {"bloom up 0", "bloom up 1", "bloom up 2"};

constexpr const char* fullscreenVs = R"(#version 430
void main()
{
    ivec2 v = ivec2(gl_VertexID & 1, gl_VertexID >> 1);
    gl_Position = vec4(vec2(v) * 4.0 - 1.0, 0.0, 1.0);
}
)";

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
    float bright = max(c.r, max(c.g, c.b));
    c *= min(bright, 2.0) / max(bright, 1e-3); // the float scene (vr_tonemap) glows by at most 2: no flicker from specks
    bright = min(bright, 2.0);
    float x = clamp((bright - Params.x) / max(1.0 - Params.x, 1e-3), 0.0, 1.0);
    float saturation = (bright - min(c.r, min(c.g, c.b))) / max(bright, 1e-3);
    float weight = mix(Weights.x, Weights.y, smoothstep(0.15, 0.6, saturation));
    Out = vec4(c * (x * x * 4.0 * weight), 1.0);
}
)";

// Half the size, dual-filter style (Bjorge, "Bandwidth-efficient rendering", SIGGRAPH 2015): the
// centre and four diagonal bilinear taps, half a source texel out (times the spread).
constexpr const char* downFs = R"(#version 430
layout(binding = 0) uniform sampler2D Source;
layout(location = 0) uniform vec4 Params; // spread, 0, 1 / source width, 1 / source height
layout(location = 0) out vec4 Out;
void main()
{
    vec2 uv = (gl_FragCoord.xy * 2.0) * Params.zw;
    vec2 o = Params.zw * Params.x;
    vec3 c = texture(Source, uv).rgb * 4.0;
    c += texture(Source, uv + vec2(-o.x, -o.y)).rgb + texture(Source, uv + vec2(o.x, -o.y)).rgb +
         texture(Source, uv + vec2(-o.x, o.y)).rgb + texture(Source, uv + vec2(o.x, o.y)).rgb;
    Out = vec4(c * 0.125, 1.0);
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
layout(location = 2) uniform float Soft;  // vr_bloom_fast, the last level: the side weight of a [Soft, 1 - 2 Soft, Soft] blur
layout(location = 0) out vec4 Out;
void main()
{
    vec2 uv = gl_FragCoord.xy / vec2(textureSize(Own, 0));
    vec2 o = Params.zw * Params.x;
    if(Soft > 0.0) // the tent wider by the blur's variance (2 Soft own texels squared; the tent's: 2 spread squared)
        o *= sqrt(1.0 + Soft / (Params.x * Params.x));
    vec3 c = texture(Smaller, uv).rgb * 4.0;
    c += (texture(Smaller, uv + vec2(-o.x, 0.0)).rgb + texture(Smaller, uv + vec2(o.x, 0.0)).rgb +
          texture(Smaller, uv + vec2(0.0, -o.y)).rgb + texture(Smaller, uv + vec2(0.0, o.y)).rgb) * 2.0;
    c += texture(Smaller, uv + vec2(-o.x, -o.y)).rgb + texture(Smaller, uv + vec2(o.x, -o.y)).rgb +
         texture(Smaller, uv + vec2(-o.x, o.y)).rgb + texture(Smaller, uv + vec2(o.x, o.y)).rgb;
    vec3 own;
    if(Soft > 0.0) // four bilinear taps 2 Soft texels out: the blur, in both directions
    {
        vec2 t = 2.0 * Soft / vec2(textureSize(Own, 0));
        own = (texture(Own, uv + vec2(-t.x, -t.y)).rgb + texture(Own, uv + vec2(t.x, -t.y)).rgb +
               texture(Own, uv + vec2(-t.x, t.y)).rgb + texture(Own, uv + vec2(t.x, t.y)).rgb) * 0.25;
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
    Out = vec4(c, 1.0);
}
)";

// How much of the view glows: the brightest channel of the smallest level, averaged over an 8 x 8
// grid of it (one texel, once per eye).
constexpr const char* meanFs = R"(#version 430
layout(binding = 0) uniform sampler2D Source;
layout(location = 0) out vec4 Out;
void main()
{
    float sum = 0.0;
    for(int y = 0; y < 8; y++)
        for(int x = 0; x < 8; x++)
        {
            vec3 s = texture(Source, (vec2(x, y) + 0.5) * 0.125).rgb;
            sum += max(s.r, max(s.g, s.b));
        }
    Out = vec4(sum * (1.0 / 64.0), 0.0, 0.0, 1.0);
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

// One full-screen triangle of prog into target, source on unit 0; Params (location 0) set unless withParams is false
// (the mean shader has no uniforms: a glUniform to a location the program lacks is a GL error).
void pass(const Target& target, GLuint prog, GLuint source, float a, float b, float c, float d, bool withParams = true)
{
    GL_BindFramebufferFunc(GL_FRAMEBUFFER, target.fbo);
    glViewport(0, 0, target.width, target.height);
    GL_UseProgram(prog);
    GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D, source);
    if(withParams)
        GL_Uniform4fFunc(0, a, b, c, d);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

// vr_bloom_fast: the blur the last level takes for the eyes' one tap (GL_PostProcess), its side weight. The four
// taps' kernel (a texel's centre weighs 1/2 wherever the pixel lies in it, its neighbours the rest by distance) is
// matched closest, in squares, by a [0.2, 0.6, 0.2] blur followed by the bilinear tap.
constexpr float oneTapSoft = 0.2f;

// vr_profile_detail 2: each pass of the chain timed apart (Debug > Profiling).
bool detailScopes = false;

// This eye's glow is added by one tap (vr_bloom_fast), for GL_PostProcess and the window's mirror.
bool resultOneTap = false;

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

} // namespace

void apply(GLuint sceneTex, int width, int height)
{
    QVR_GPU_PROFILE("bloom");
    resultValid = false;
    const float strength = vr_bloom.value;
    if(strength <= 0.f || width < 128 || height < 128 || !ensurePrograms())
    {
        return;
    }
    // vr_bloom_fast (ROUND22.md, "Cheaper bloom"): three levels instead of four (two passes fewer), the glow added by
    // one tap a pixel instead of four. 0: the chain as before.
    const bool fastChain = vr_bloom_fast.value > 0.f;
    const int used = fastChain ? fastLevels : levels;

    Chain& chain = chains[stereo::isSpectator() ? 1 : 0];
    Target* const down = chain.down;
    Target* const up = chain.up;
    Target& mean = chain.mean;
    for(int l = 0; l < used; l++)
    {
        const int w = za::max(1, width >> (2 + l));
        const int h = za::max(1, height >> (2 + l));
        if(!ensure(down[l], w, h, "vr bloom down") || (l < used - 1 && !ensure(up[l], w, h, "vr bloom up")))
        {
            return;
        }
    }
    if(!ensure(mean, 1, 1, "vr bloom mean"))
    {
        return;
    }

    detailScopes = vr_profile_detail.value >= 2.f;
    GL_BeginGroup("VR bloom");
    GL_SetState(GLS_BLEND_OPAQUE | GLS_NO_ZTEST | GLS_NO_ZWRITE | GLS_CULL_NONE | GLS_ATTRIBS(0));

    const float threshold = za::clamp(vr_bloom_threshold.value, 0.f, 0.99f);
    const float spread = za::clamp(vr_bloom_radius.value, 0.25f, 4.f);

    scopeBegin("bloom bright");
    GL_UseProgram(brightProgram);
    GL_Uniform2fFunc(1, za::max(0.f, vr_bloom_white.value), za::max(0.f, vr_bloom_color.value));
    pass(down[0], brightProgram, sceneTex, threshold, 0.f, 1.f / width, 1.f / height);
    scopeEnd();
    for(int l = 1; l < used; l++)
    {
        const Target& source = down[l - 1];
        scopeBegin(downNames[l]);
        pass(down[l], downProgram, source.tex, spread, 0.f, 1.f / source.width, 1.f / source.height);
        scopeEnd();
    }
    scopeBegin("bloom mean");
    pass(mean, meanProgram, down[used - 1].tex, 0.f, 0.f, 0.f, 0.f, false);
    scopeEnd();

    GL_BindNative(GL_TEXTURE2, GL_TEXTURE_2D, mean.tex);
    // The smallest level has no up level of its own: it comes in by its weight in the first up pass (without the
    // thirty-second, the sixteenth takes its weight too).
    const float smallestWeight = levelWeight[used - 1] + (used < levels ? levelWeight[levels - 1] : 0.f);
    for(int l = used - 2; l >= 0; l--)
    {
        const bool first = l == used - 2;
        const Target& smaller = first ? down[used - 1] : up[l + 1];
        GL_UseProgram(upProgram);
        GL_BindNative(GL_TEXTURE1, GL_TEXTURE_2D, down[l].tex);
        GL_Uniform3fFunc(1, first ? smallestWeight : 1.f, l == 0 ? strength : 0.f, za::max(0.f, vr_bloom_adapt.value));
        GL_Uniform1fFunc(2, l == 0 && fastChain ? oneTapSoft : 0.f);
        scopeBegin(upNames[l]);
        pass(up[l], upProgram, smaller.tex, spread, levelWeight[l], 1.f / smaller.width, 1.f / smaller.height);
        scopeEnd();
    }

    resultTex = up[0].tex;
    resultOneTap = fastChain;
    resultValid = true;
    GL_EndGroup();
}

bool fast()
{
    return resultOneTap;
}

bool result(unsigned& texture)
{
    texture = resultTex;
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
    resultValid = false;
    failed = false;
}

} // namespace qvr::bloom

// GL_PostProcess: binds this eye's glow to texture unit 2 and gives how much of it to add (0: none,
// the window's own post-processing; -1: by one tap, vr_bloom_fast).
extern "C" float VR_PostProcessBloom()
{
    unsigned texture = 0;
    if(!VR_RenderingEye() || !qvr::bloom::result(texture))
    {
        return 0.f;
    }
    GL_BindNative(GL_TEXTURE2, GL_TEXTURE_2D, texture);
    return qvr::bloom::fast() ? -1.f : 1.f;
}
