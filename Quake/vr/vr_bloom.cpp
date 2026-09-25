// vr_bloom.cpp -- see vr_bloom.hpp.

#include "vr_bloom.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_gfx.hpp"

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

// Three levels (a quarter, an eighth, a sixteenth of the scene), each with a second target for the
// blur's other direction.
constexpr int levels = 3;
Target targets[levels][2];
GLuint brightProgram = 0;
GLuint downProgram = 0;
GLuint blurProgram = 0;
GLuint addProgram = 0;
bool failed = false;

constexpr const char* fullscreenVs = R"(#version 430
void main()
{
    ivec2 v = ivec2(gl_VertexID & 1, gl_VertexID >> 1);
    gl_Position = vec4(vec2(v) * 4.0 - 1.0, 0.0, 1.0);
}
)";

// Down to a quarter: 16 texels averaged by four bilinear taps, keeping what is over the threshold.
// The scene is low dynamic range (a lamp is 1, not 10), so the response rises steeply towards white:
// lamps and glowing panels glow, merely bright walls hardly. White and pale light glows by
// vr_bloom_white, coloured light (red buttons, blue panels) by vr_bloom_color, blended by how
// saturated it is.
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

// Half the size: four bilinear taps (16 texels).
constexpr const char* downFs = R"(#version 430
layout(binding = 0) uniform sampler2D Source;
layout(location = 0) uniform vec4 Params; // 0, 0, 1 / source width, 1 / source height
layout(location = 0) out vec4 Out;
void main()
{
    vec2 uv = (gl_FragCoord.xy * 2.0) * Params.zw;
    vec2 t = Params.zw;
    vec3 c = texture(Source, uv + vec2(-t.x, -t.y)).rgb + texture(Source, uv + vec2(t.x, -t.y)).rgb +
             texture(Source, uv + vec2(-t.x, t.y)).rgb + texture(Source, uv + vec2(t.x, t.y)).rgb;
    Out = vec4(c * 0.25, 1.0);
}
)";

// A 9-tap Gaussian along one direction, five bilinear taps.
constexpr const char* blurFs = R"(#version 430
layout(binding = 0) uniform sampler2D Source;
layout(location = 0) uniform vec4 Params; // direction (texels), 1 / width, 1 / height
layout(location = 0) out vec4 Out;
void main()
{
    vec2 texel = Params.zw;
    vec2 uv = gl_FragCoord.xy * texel;
    vec2 d = Params.xy * texel;
    vec3 c = texture(Source, uv).rgb * 0.2270270270;
    c += (texture(Source, uv + d * 1.3846153846).rgb + texture(Source, uv - d * 1.3846153846).rgb) * 0.3162162162;
    c += (texture(Source, uv + d * 3.2307692308).rgb + texture(Source, uv - d * 3.2307692308).rgb) * 0.0702702703;
    Out = vec4(c, 1.0);
}
)";

// The levels added onto the scene (blended one, one), upsampled bilinearly: the small one a tight
// halo, the larger ones a wide soft glow. Weaker the more of the view glows (vr_bloom_adapt): lamps
// in a dark room keep their full glow, a brightly lit map (all of it over the threshold) is not
// washed out. How much glows is the widest level's mean, from a fixed 4 x 4 grid of its texels.
constexpr const char* addFs = R"(#version 430
layout(binding = 0) uniform sampler2D Glow0;
layout(binding = 1) uniform sampler2D Glow1;
layout(binding = 2) uniform sampler2D Glow2;
layout(location = 0) uniform vec4 Params; // strength, adapt, 1 / scene width, 1 / scene height
layout(location = 0) out vec4 Out;
void main()
{
    vec2 uv = gl_FragCoord.xy * Params.zw;
    float mean = 0.0;
    for(int y = 0; y < 4; y++)
        for(int x = 0; x < 4; x++)
        {
            vec3 s = texture(Glow2, (vec2(x, y) + 0.5) * 0.25).rgb;
            mean += max(s.r, max(s.g, s.b));
        }
    mean *= 1.0 / 16.0;
    vec3 g = texture(Glow0, uv).rgb * 0.5 + texture(Glow1, uv).rgb * 0.8 + texture(Glow2, uv).rgb * 1.0;
    Out = vec4(g * (Params.x / (1.0 + Params.y * mean)), 0.0);
}
)";

[[nodiscard]] bool ensurePrograms()
{
    if(brightProgram || failed)
    {
        return !failed;
    }
    brightProgram = gfx::glProgram(fullscreenVs, brightFs, "vr bloom bright pass");
    downProgram = gfx::glProgram(fullscreenVs, downFs, "vr bloom downsample");
    blurProgram = gfx::glProgram(fullscreenVs, blurFs, "vr bloom blur");
    addProgram = gfx::glProgram(fullscreenVs, addFs, "vr bloom add");
    failed = !brightProgram || !downProgram || !blurProgram || !addProgram;
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
    GL_TexStorage2DFunc(GL_TEXTURE_2D, 1, GL_RGBA16F, width, height);
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

void apply(GLuint sceneFbo, GLuint sceneTex, int width, int height)
{
    const float strength = vr_bloom.value;
    if(strength <= 0.f || width < 64 || height < 64 || !ensurePrograms())
    {
        return;
    }
    for(int l = 0; l < levels; l++)
    {
        const int w = std::max(1, width >> (2 + l));
        const int h = std::max(1, height >> (2 + l));
        if(!ensure(targets[l][0], w, h, "vr bloom") || !ensure(targets[l][1], w, h, "vr bloom blur"))
        {
            return;
        }
    }

    GL_BeginGroup("VR bloom");
    GL_SetState(GLS_BLEND_OPAQUE | GLS_NO_ZTEST | GLS_NO_ZWRITE | GLS_CULL_NONE | GLS_ATTRIBS(0));

    const float threshold = std::clamp(vr_bloom_threshold.value, 0.f, 0.99f);
    const float spread = std::clamp(vr_bloom_radius.value, 0.25f, 4.f);
    for(int l = 0; l < levels; l++)
    {
        Target& t = targets[l][0];
        Target& u = targets[l][1];
        if(l == 0)
        {
            GL_UseProgram(brightProgram);
            GL_Uniform2fFunc(1, std::max(0.f, vr_bloom_white.value), std::max(0.f, vr_bloom_color.value));
            pass(t, brightProgram, sceneTex, threshold, 0.f, 1.f / width, 1.f / height);
        }
        else
        {
            const Target& previous = targets[l - 1][0];
            pass(t, downProgram, previous.tex, 0.f, 0.f, 1.f / previous.width, 1.f / previous.height);
        }
        pass(u, blurProgram, t.tex, spread, 0.f, 1.f / t.width, 1.f / t.height);
        pass(t, blurProgram, u.tex, 0.f, spread, 1.f / t.width, 1.f / t.height);
    }

    GL_BindFramebufferFunc(GL_FRAMEBUFFER, sceneFbo);
    glViewport(0, 0, width, height);
    GL_UseProgram(addProgram);
    for(int l = 0; l < levels; l++)
    {
        GL_BindNative(GL_TEXTURE0 + l, GL_TEXTURE_2D, targets[l][0].tex);
    }
    GL_Uniform4fFunc(0, strength, std::max(0.f, vr_bloom_adapt.value), 1.f / width, 1.f / height);
    glBlendFunc(GL_ONE, GL_ONE);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBlendFunc(GL_ONE, GL_ZERO); // GLS_BLEND_OPAQUE, as the state cache has it

    GL_EndGroup();
}

void shutdown()
{
    for(auto& level : targets)
    {
        destroy(level[0]);
        destroy(level[1]);
    }
    for(GLuint* p : {&brightProgram, &downProgram, &blurProgram, &addProgram})
    {
        if(*p)
        {
            GL_DeleteProgramFunc(*p);
            *p = 0;
        }
    }
    failed = false;
}

} // namespace qvr::bloom
