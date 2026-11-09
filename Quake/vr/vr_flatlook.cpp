// vr_flatlook.cpp -- see vr_flatlook.hpp.

#include "vr_flatlook.hpp"
#include "vr_bloom.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_flatplay.hpp"
#include "vr_gfx.hpp"
#include "vr_glutil.hpp"
#include "vr_profile.hpp"
#include "vr_stereo.hpp"
#include "vr_tonemap.h" // QVR_TONE_GLSL: the eyes' tone curve, the exposure's roll-off here

#include "Zancle/Math/Clamp.hpp"

namespace qvr::flatlook
{
namespace
{

constexpr const char* fullscreenVs = R"(#version 430
void main()
{
    ivec2 v = ivec2(gl_VertexID & 1, gl_VertexID >> 1);
    gl_Position = vec4(vec2(v) * 4.0 - 1.0, 0.0, 1.0);
}
)";

// The view's copy (Scene, at 0,0) back into the window's target at Rect.xy, the glow added, graded and vignetted. The
// colours are Quake's as shown (gamma-encoded; the window's own gamma and contrast come after, in GL_PostProcess).
// The grade: exposure (its top rolled off by the eyes' tone curve, QvrTonemap); shadows (a black point raised: crushed; or the blacks lifted); an S curve about mid grey of
// slope `contrast` there, black and white kept; a split tone by brightness (highlights towards amber, shadows towards
// blue, luma about kept); saturation about the luma; all of it blended in by its strength. The vignette darkens the
// corners, its edge the screen's shape (the middles of the sides get the part of it the size reaches).
constexpr const char* lookFs = R"(#version 430
layout(binding = 0) uniform sampler2D Scene;
layout(binding = 1) uniform sampler2D Glow;
layout(location = 0) uniform vec4 Grade;  // strength (0: none), warmth, saturation, contrast
layout(location = 1) uniform vec4 Grade2; // shadows, exposure, vignette (0: none), vignette size
layout(location = 2) uniform vec4 Rect;   // the view's corner in the target, 1 / its width, 1 / its height
layout(location = 3) uniform float GlowOn; // 1: Glow added (the strength in it)
layout(location = 0) out vec4 Out;
)" QVR_TONE_GLSL R"(
void main()
{
    vec2 p = gl_FragCoord.xy - Rect.xy;
    vec2 uv = p * Rect.zw;
    vec3 c = texelFetch(Scene, ivec2(p), 0).rgb;
    if (GlowOn > 0.0)
        c += texture(Glow, uv).rgb;
    if (Grade.x > 0.0)
    {
        vec3 c0 = clamp(c, 0.0, 1.0);
        vec3 g = QvrTonemap(c * Grade2.y, vec2(0.7, max(1.5, 1.25 * Grade2.y))); // brightened, its top rolled off
        g = clamp(g, 0.0, 1.0);
        float s = Grade2.x * 0.08;
        g = s < 0.0 ? max(g + s, 0.0) / (1.0 + s) : g + s * (1.0 - g);
        const float pivot = 0.45;
        vec3 lo = pivot * pow(g / pivot, vec3(Grade.w));
        vec3 hi = 1.0 - (1.0 - pivot) * pow(max((1.0 - g) / (1.0 - pivot), 0.0), vec3(Grade.w));
        g = mix(lo, hi, step(pivot, g));
        float l = dot(g, vec3(0.2126, 0.7152, 0.0722));
        vec3 tint = mix(vec3(0.95, 0.99, 1.07), vec3(1.09, 1.0, 0.86), smoothstep(0.15, 0.7, l));
        g *= mix(vec3(1.0), tint, Grade.y);
        l = dot(g, vec3(0.2126, 0.7152, 0.0722));
        g = max(mix(vec3(l), g, Grade.z), 0.0);
        c = mix(c0, g, Grade.x);
    }
    if (Grade2.z > 0.0)
    {
        vec2 v = uv * 2.0 - 1.0;
        float d = dot(v, v) * 0.5; // 1 in the corners, 0.5 at the sides' middles
        c *= 1.0 - Grade2.z * smoothstep(1.0 - Grade2.w, 1.0, d);
    }
    Out = vec4(c, 1.0);
}
)";

GLuint program = 0;
bool failed = false;
gfx::RenderTarget copy; // the view, copied: what the pass reads
long long passCount = 0;

[[nodiscard]] bool ensureProgram()
{
    if(program || failed)
    {
        return !failed;
    }
    program = gfx::glProgram(fullscreenVs, lookFs, "vr flat look");
    failed = !program;
    return !failed;
}

[[nodiscard]] float gradeStrength()
{
    return za::clamp(vr_flat_grade.value, 0.f, 1.f);
}

[[nodiscard]] float vignetteStrength()
{
    return za::clamp(vr_flat_vignette.value, 0.f, 1.f);
}

void info_f()
{
    Con_Printf("vr_flatlook_info: passes %lld, active %d, flatscreen play %d\n", passCount, active() ? 1 : 0,
        flatplay::active() ? 1 : 0);
}

// The pass, after the view is drawn into the window's target (R_RenderView, after R_WarpScaleView): x, y, w, h the
// view's rectangle in that target.
void apply(int x, int y, int w, int h)
{
    if(w < 16 || h < 16 || !ensureProgram())
    {
        return;
    }
    QVR_GPU_PROFILE("flat look");
    if(!copy.ensure(w, h, gfx::RenderTargetFormat{.internalFormat = GL_RGB10_A2}, "vr flat look copy"))
    {
        Con_Warning("VR flat look: framebuffer incomplete\n");
        failed = true;
        return;
    }
    GL_BeginGroup("VR flat look");
    GL_BindFramebufferFunc(GL_READ_FRAMEBUFFER, framebufs.composite.fbo);
    GL_BindFramebufferFunc(GL_DRAW_FRAMEBUFFER, copy.fbo);
    GL_BlitFramebufferFunc(x, y, x + w, y + h, 0, 0, w, h, GL_COLOR_BUFFER_BIT, GL_NEAREST);

    const GLuint glow = bloom::applyFlat(copy.tex, w, h, za::clamp(vr_flat_bloom.value, 0.f, 2.f));

    GL_BindFramebufferFunc(GL_FRAMEBUFFER, framebufs.composite.fbo);
    glViewport(x, y, w, h);
    GL_UseProgram(program);
    GL_SetState(GLS_BLEND_OPAQUE | GLS_NO_ZTEST | GLS_NO_ZWRITE | GLS_CULL_NONE | GLS_ATTRIBS(0));
    GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D, copy.tex);
    if(glow)
    {
        GL_BindNative(GL_TEXTURE1, GL_TEXTURE_2D, glow);
    }
    GL_Uniform4fFunc(0, gradeStrength(), za::clamp(vr_flat_grade_warmth.value, -2.f, 2.f),
        za::clamp(vr_flat_grade_saturation.value, 0.f, 3.f), za::clamp(vr_flat_grade_contrast.value, 0.25f, 3.f));
    GL_Uniform4fFunc(1, za::clamp(vr_flat_grade_shadows.value, -1.f, 1.f), za::clamp(vr_flat_grade_exposure.value, 0.25f, 4.f),
        vignetteStrength(), za::clamp(vr_flat_vignette_size.value, 0.05f, 1.f));
    GL_Uniform4fFunc(2, static_cast<float>(x), static_cast<float>(y), 1.f / w, 1.f / h);
    GL_Uniform1fFunc(3, glow ? 1.f : 0.f);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    GL_EndGroup();
    ++passCount;
}

} // namespace

bool active()
{
    return flatplay::active() && (gradeStrength() > 0.f || vignetteStrength() > 0.f || vr_flat_bloom.value > 0.f);
}

long long passes()
{
    return passCount;
}

void init()
{
    Cmd_AddCommand("vr_flatlook_info", info_f);
}

} // namespace qvr::flatlook

using namespace qvr;

// GL_NeedsPostprocess: flatscreen play's look draws into the post-process's target.
extern "C" int VR_FlatLook(void)
{
    return flatlook::active() ? 1 : 0;
}

// R_RenderView, after R_WarpScaleView: the window's view (never an eye's or a teleporter's offscreen camera) graded.
extern "C" void VR_FlatLookApply(void)
{
    if(!flatlook::active() || stereo::isRenderingEye() || VR_PortalDrawing() || !GL_NeedsPostprocess())
    {
        return;
    }
    const int x = glx + r_refdef.vrect.x;
    const int y = gly + glheight - r_refdef.vrect.y - r_refdef.vrect.height;
    flatlook::apply(x, y, r_refdef.vrect.width, r_refdef.vrect.height);
}
