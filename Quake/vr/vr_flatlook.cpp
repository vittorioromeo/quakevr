// vr_flatlook.cpp -- see vr_flatlook.hpp.

#include "vr_flatlook.hpp"
#include "vr_bloom.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_flatplay.hpp"
#include "vr_flatsight.hpp"
#include "vr_gfx.hpp"
#include "vr_glutil.hpp"
#include "vr_profile.hpp"
#include "vr_stereo.hpp"
#include "vr_tonemap.hpp" // QVR_TONE_GLSL and its knee and white point: the eyes' tone curve, used here too

#include "Zancle/Math/Clamp.hpp"
#include "vr_api_render.h"

namespace qvr::flatlook
{
namespace
{

constexpr const char* fullscreenVs = gfx::fullscreenTriangleVs;

// The view's copy (Scene, at 0,0) back into the window's target at Rect.xy, the glow added, graded and vignetted. The
// colours are Quake's as shown (gamma-encoded; the window's own gamma and contrast come after, in GL_PostProcess).
// Tone mapping (vr_flat_tonemap: Tone.x 1, a float scene up to tonemap::sceneMax): the eyes' curve with their knee and
// white point rolls the top off, once: at the grade's exposure in the grade, at 1 in what the grade's strength blends
// from. Without it (Quake's clamp: an 8-bit scene) the grade's own roll-off of what its exposure pushes past 1.
// The grade: exposure (its top rolled off by the eyes' tone curve, QvrTonemap); shadows (a black point raised: crushed; or the blacks lifted); an S curve about mid grey of
// slope `contrast` there, black and white kept; a split tone by brightness (highlights towards amber, shadows towards
// blue, luma about kept); saturation about the luma; all of it blended in by its strength. The vignette darkens the
// corners, its edge the screen's shape (the middles of the sides get the part of it the size reaches).
// The zoom aim's visor look (Visor.x > 0: vr_flat_zoom_visor while a gun with no sights zooms; flatsight::zoomVisor): the
// lens's edge, red and blue a pixel or two apart (more as it engages: Visor.y, 1 falling to 0), darkened towards the visor's
// cool tint and lined like its panels (vr_flat_visor_crt's scanlines); four thin corner brackets round the middle; as it
// engages, a scan line sweeping down and a shimmer of lines.
constexpr const char* lookFs = R"(#version 430
layout(binding = 0) uniform sampler2D Scene;
layout(binding = 1) uniform sampler2D Glow;
layout(location = 0) uniform vec4 Grade;  // strength (0: none), warmth, saturation, contrast
layout(location = 1) uniform vec4 Grade2; // shadows, exposure, vignette (0: none), vignette size
layout(location = 2) uniform vec4 Rect;   // the view's corner in the target, 1 / its width, 1 / its height
layout(location = 3) uniform float GlowOn; // 1: Glow added (the strength in it)
layout(location = 4) uniform vec4 Tone;    // 1: the float scene tone-mapped (vr_flat_tonemap), knee, white point
layout(location = 5) uniform vec4 Visor;   // the zoom aim's visor look: strength (0: none), shimmer (1 engaging .. 0), -, width / height
layout(location = 0) out vec4 Out;
)" QVR_TONE_GLSL R"(
const vec3 VisorTint = vec3(0.55, 0.95, 1.0);
void main()
{
    vec2 p = gl_FragCoord.xy - Rect.xy;
    vec2 uv = p * Rect.zw;
    vec3 c = texelFetch(Scene, ivec2(p), 0).rgb;
    vec2 q = (uv * 2.0 - 1.0) * vec2(Visor.w, 1.0); // the screen's height 2, 0 at the middle
    float edge = 0.0;
    if (Visor.x > 0.0)
    {
        edge = smoothstep(0.45, 1.0, length(q) / length(vec2(Visor.w, 1.0))); // 1 in the corners
        float f = (1.5 * edge + 4.0 * Visor.y * (0.3 + edge)) * Visor.x;      // the fringe, pixels
        if (f > 0.05)
        {
            vec2 dir = normalize(uv * 2.0 - 1.0 + vec2(1e-4));
            ivec2 lim = ivec2(1.0 / Rect.zw) - 1;
            c.r = texelFetch(Scene, clamp(ivec2(p + dir * f), ivec2(0), lim), 0).r;
            c.b = texelFetch(Scene, clamp(ivec2(p - dir * f), ivec2(0), lim), 0).b;
        }
    }
    if (GlowOn > 0.0)
        c += texture(Glow, uv).rgb;
    vec3 c0 = Tone.x > 0.0 ? QvrTonemap(max(c, 0.0), Tone.yz) : clamp(c, 0.0, 1.0); // as shown without the grade
    if (Grade.x > 0.0)
    {
        vec2 kw = Tone.x > 0.0 ? vec2(Tone.y, max(Tone.z, 1.25 * Grade2.y)) : vec2(0.7, max(1.5, 1.25 * Grade2.y));
        vec3 g = QvrTonemap(max(c, 0.0) * Grade2.y, kw); // brightened, its top rolled off
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
        c0 = mix(c0, g, Grade.x);
    }
    c = c0;
    if (Grade2.z > 0.0)
    {
        vec2 v = uv * 2.0 - 1.0;
        float d = dot(v, v) * 0.5; // 1 in the corners, 0.5 at the sides' middles
        c *= 1.0 - Grade2.z * smoothstep(1.0 - Grade2.w, 1.0, d);
    }
    if (Visor.x > 0.0)
    {
        float lines = 0.5 + 0.5 * sin(gl_FragCoord.y * 1.5708); // a line every 4 pixels
        c = mix(c, c * vec3(0.72, 0.9, 1.0), edge * 0.6 * Visor.x);
        c *= 1.0 - edge * 0.25 * Visor.x * lines;
        // The frame: four corner brackets round the middle, a pixel and a bit thick.
        float px = 2.0 * Rect.w;
        vec2 a = abs(q);
        const vec2 box = vec2(0.5, 0.34);
        const float arm = 0.08;
        float onX = 1.0 - smoothstep(0.6 * px, 1.4 * px, abs(a.x - box.x));
        float onY = 1.0 - smoothstep(0.6 * px, 1.4 * px, abs(a.y - box.y));
        float frame = max(onX * step(box.y - arm, a.y) * step(a.y, box.y + px),
                          onY * step(box.x - arm, a.x) * step(a.x, box.x + px));
        c = mix(c, VisorTint, frame * 0.7 * Visor.x);
        if (Visor.y > 0.0)
        {
            c += VisorTint * (0.2 * Visor.x * Visor.y) * exp(-abs(uv.y - Visor.y) * 60.0); // the scan, top to bottom
            c *= 1.0 + 0.08 * Visor.x * Visor.y * sin(gl_FragCoord.y * 0.7 + Visor.y * 30.0);
        }
    }
    Out = vec4(c, 1.0);
}
)";

GLuint program = 0;
bool failed = false;
gfx::RenderTarget copy; // the view, copied: what the pass reads
GLenum copyFormat = 0;   // its format: the window's scene's (float with vr_flat_tonemap)
long long passCount = 0;
bool windowFloat = false; // the window's targets were last made float (windowSceneFormat)

// vr_flat_tonemap changed: the window's targets made again in the format it asks for (as vid_fsaa's change does).
void onToneChanged(cvar_t* /*var*/)
{
    if(!host_initialized || !flatplay::flagGiven() || windowFloat == (vr_flat_tonemap.value > 0.f))
    {
        return;
    }
    GL_DeleteFrameBuffers();
    GL_CreateFrameBuffers();
}

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
    Con_Printf("vr_flatlook_info: passes %lld, active %d, flatscreen play %d, float scene %d\n", passCount,
        active() ? 1 : 0, flatplay::active() ? 1 : 0, floatScene() ? 1 : 0);
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
    const bool tone = floatScene();
    const GLenum format = tone ? GL_RGBA16F : GL_RGB10_A2; // (the float scene's top kept for the curve and the glow)
    if(copyFormat != format)
    {
        copy.destroy();
        copyFormat = format;
    }
    if(!copy.ensure(w, h, gfx::RenderTargetFormat{.internalFormat = format}, "vr flat look copy"))
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
    GL_Uniform4fFunc(4, tone ? 1.f : 0.f, tonemap::knee, tonemap::white, 0.f);
    float shimmer = 0.f;
    const float visor = flatsight::zoomVisor(shimmer);
    GL_Uniform4fFunc(5, visor, shimmer, 0.f, static_cast<float>(w) / static_cast<float>(h));
    glDrawArrays(GL_TRIANGLES, 0, 3);
    GL_EndGroup();
    ++passCount;
}

} // namespace

bool active()
{
    float shimmer = 0.f;
    return flatplay::active() && (gradeStrength() > 0.f || vignetteStrength() > 0.f || vr_flat_bloom.value > 0.f ||
                                     floatScene() || flatsight::zoomVisor(shimmer) > 0.f);
}

unsigned windowSceneFormat(const unsigned format)
{
    windowFloat = flatplay::flagGiven() && vr_flat_tonemap.value > 0.f;
    return windowFloat ? GL_RGBA16F : format;
}

bool floatScene()
{
    return windowFloat && flatplay::active();
}

long long passes()
{
    return passCount;
}

void init()
{
    Cmd_AddCommand("vr_flatlook_info", info_f);
    Cvar_SetCallback(&vr_flat_tonemap, onToneChanged);
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
