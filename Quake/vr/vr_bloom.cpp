// vr_bloom.cpp -- see vr_bloom.hpp.

#include "vr_bloom.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_gfx.hpp"
#include "vr_profile.hpp"

#include <algorithm>

namespace qvr::bloom
{
namespace
{

struct Target
{
    GLuint tex{0};
    GLuint fbo{0};
    int width{0};
    int height{0};
};

// The chain: down[0] the bright pass at a quarter of the scene, down[1..3] an eighth, a sixteenth,
// a thirty-second; up[2..0] back up to a quarter, each the level below it spread out plus its own
// down level; mean: how much of the view glows, one texel.
constexpr int levels = 4;
Target down[levels];
Target up[levels - 1];
Target mean;
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

constexpr const char* fullscreenVs = R"(#version 430
void main()
{
    ivec2 v = ivec2(gl_VertexID & 1, gl_VertexID >> 1);
    gl_Position = vec4(vec2(v) * 4.0 - 1.0, 0.0, 1.0);
}
)";

// Down to a quarter: 16 texels by four bilinear taps, keeping what is over the threshold. The scene
// is low dynamic range (a lamp is 1, not 10), so the response rises steeply towards white: lamps and
// glowing panels glow, merely bright walls hardly. White and pale light glows by vr_bloom_white,
// coloured light (red buttons, blue panels) by vr_bloom_color, blended by how saturated it is.
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
layout(location = 0) out vec4 Out;
void main()
{
    vec2 uv = gl_FragCoord.xy / vec2(textureSize(Own, 0));
    vec2 o = Params.zw * Params.x;
    vec3 c = texture(Smaller, uv).rgb * 4.0;
    c += (texture(Smaller, uv + vec2(-o.x, 0.0)).rgb + texture(Smaller, uv + vec2(o.x, 0.0)).rgb +
          texture(Smaller, uv + vec2(0.0, -o.y)).rgb + texture(Smaller, uv + vec2(0.0, o.y)).rgb) * 2.0;
    c += texture(Smaller, uv + vec2(-o.x, -o.y)).rgb + texture(Smaller, uv + vec2(o.x, -o.y)).rgb +
         texture(Smaller, uv + vec2(-o.x, o.y)).rgb + texture(Smaller, uv + vec2(o.x, o.y)).rgb;
    c = c * (Extra.x / 16.0) + texelFetch(Own, ivec2(gl_FragCoord.xy), 0).rgb * Params.y;
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
    if(t.fbo)
    {
        GL_DeleteFramebuffersFunc(1, &t.fbo);
    }
    if(t.tex)
    {
        glDeleteTextures(1, &t.tex);
    }
    t = Target{};
}

[[nodiscard]] bool ensure(Target& t, int width, int height, const char* name)
{
    if(t.tex && t.width == width && t.height == height)
    {
        return true;
    }
    destroy(t);
    glGenTextures(1, &t.tex);
    GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D, t.tex);
    GL_TexStorage2DFunc(GL_TEXTURE_2D, 1, GL_R11F_G11F_B10F, width, height);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D, 0);
    GL_ObjectLabelFunc(GL_TEXTURE, t.tex, -1, name);

    GL_GenFramebuffersFunc(1, &t.fbo);
    GL_BindFramebufferFunc(GL_FRAMEBUFFER, t.fbo);
    GL_FramebufferTexture2DFunc(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, t.tex, 0);
    const bool ok = GL_CheckFramebufferStatusFunc(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    if(!ok)
    {
        Con_Warning("VR bloom: framebuffer incomplete\n");
        destroy(t);
        failed = true;
        return false;
    }
    t.width = width;
    t.height = height;
    return true;
}

void pass(const Target& target, GLuint prog, GLuint source, float a, float b, float c, float d)
{
    GL_BindFramebufferFunc(GL_FRAMEBUFFER, target.fbo);
    glViewport(0, 0, target.width, target.height);
    GL_UseProgram(prog);
    GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D, source);
    GL_Uniform4fFunc(0, a, b, c, d);
    glDrawArrays(GL_TRIANGLES, 0, 3);
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
    for(int l = 0; l < levels; l++)
    {
        const int w = std::max(1, width >> (2 + l));
        const int h = std::max(1, height >> (2 + l));
        if(!ensure(down[l], w, h, "vr bloom down") || (l < levels - 1 && !ensure(up[l], w, h, "vr bloom up")))
        {
            return;
        }
    }
    if(!ensure(mean, 1, 1, "vr bloom mean"))
    {
        return;
    }

    GL_BeginGroup("VR bloom");
    GL_SetState(GLS_BLEND_OPAQUE | GLS_NO_ZTEST | GLS_NO_ZWRITE | GLS_CULL_NONE | GLS_ATTRIBS(0));

    const float threshold = std::clamp(vr_bloom_threshold.value, 0.f, 0.99f);
    const float spread = std::clamp(vr_bloom_radius.value, 0.25f, 4.f);

    GL_UseProgram(brightProgram);
    GL_Uniform2fFunc(1, std::max(0.f, vr_bloom_white.value), std::max(0.f, vr_bloom_color.value));
    pass(down[0], brightProgram, sceneTex, threshold, 0.f, 1.f / width, 1.f / height);
    for(int l = 1; l < levels; l++)
    {
        const Target& source = down[l - 1];
        pass(down[l], downProgram, source.tex, spread, 0.f, 1.f / source.width, 1.f / source.height);
    }
    pass(mean, meanProgram, down[levels - 1].tex, 0.f, 0.f, 0.f, 0.f);

    GL_BindNative(GL_TEXTURE2, GL_TEXTURE_2D, mean.tex);
    for(int l = levels - 2; l >= 0; l--)
    {
        // The smallest level has no up level of its own: it comes in by its weight here.
        const bool first = l == levels - 2;
        const Target& smaller = first ? down[levels - 1] : up[l + 1];
        GL_UseProgram(upProgram);
        GL_BindNative(GL_TEXTURE1, GL_TEXTURE_2D, down[l].tex);
        GL_Uniform3fFunc(1, first ? levelWeight[levels - 1] : 1.f, l == 0 ? strength : 0.f,
            std::max(0.f, vr_bloom_adapt.value));
        pass(up[l], upProgram, smaller.tex, spread, levelWeight[l], 1.f / smaller.width, 1.f / smaller.height);
    }

    resultTex = up[0].tex;
    resultValid = true;
    GL_EndGroup();
}

bool result(unsigned& texture)
{
    texture = resultTex;
    return resultValid;
}

void shutdown()
{
    for(Target& t : down)
    {
        destroy(t);
    }
    for(Target& t : up)
    {
        destroy(t);
    }
    destroy(mean);
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
// the window's own post-processing).
extern "C" float VR_PostProcessBloom()
{
    unsigned texture = 0;
    if(!VR_RenderingEye() || !qvr::bloom::result(texture))
    {
        return 0.f;
    }
    GL_BindNative(GL_TEXTURE2, GL_TEXTURE_2D, texture);
    return 1.f;
}
