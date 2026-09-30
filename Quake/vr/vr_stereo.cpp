// vr_stereo.cpp -- renders each eye with Ironwail's normal pipeline.
//
// For each eye, an eye-sized set of Ironwail's framebuffers is swapped in, the view is moved
// to the eye (vr_view.cpp) with the eye's asymmetric projection (VR_OverrideProjection), the
// usual V_RenderView runs, and Ironwail's post-process pass (gamma, contrast, dithering)
// writes into the backend's eye image instead of the window (at a vr_render_scale other than 1,
// into a texture of the scaled size, resampled into the image: bilinear, FSR 1 or NIS, vr_upscale.cpp).
// The scene is shaded coarser away from the lens centre with vr_foveated (vr_foveated.cpp). The eye's
// UI is drawn over the final image at its full size. The left eye is then mirrored to the window, where
// the 2D layer is drawn as usual.

#include "vr_fgfx.hpp"
#include "vr_gfx.hpp"
#include "vr_bloom.hpp"
#include "vr_body.hpp"
#include "vr_envmap.hpp"
#include "vr_foveated.hpp"
#include "vr_engine.hpp"
#include "vr_crosshair.hpp"
#include "vr_cvars.hpp"
#include "vr_hands.hpp"
#include "vr_lines.hpp"
#include "vr_main.hpp"
#include "vr_meleehud.hpp"
#include "vr_panel.hpp"
#include "vr_profile.hpp"
#include "vr_stereo.hpp"
#include "vr_text3d.hpp"
#include "vr_tonemap.hpp"
#include "vr_upscale.hpp"
#include "vr_water.hpp"
#include "vr_window.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace qvr::stereo
{
namespace
{

// A set of Ironwail's framebuffers of one size: the eyes', and the window's spectator camera's (vr_window.cpp).
struct SceneTargets
{
    glframebufs_t fb{};
    int width = 0;
    int height = 0;
    float fsaa = 0.f;    // the MSAA they were made with (vid_fsaa; the spectator camera's: vr_spectator_aa)
    unsigned format = 0; // their scene colour format (vr_tonemap: float)
};
SceneTargets eyeTargets;
SceneTargets spectatorTargets;
bool creatingSceneTargets = false; // VR_SceneColorFormat, VR_SceneSamples
unsigned creatingFormat = 0;
float creatingFsaa = 0.f;
GLuint targetFbo = 0;

// vr_render_scale: the eyes are rendered at the scaled size, post-processed into this texture of
// that size, and resampled (upscale::resample) into the eye image, which keeps the runtime's size.
// At scale 1 too with vr_upscale_sharpen_native (sharpened into the image).
GLuint resampleFbo = 0;
GLuint resampleTex = 0;
int resampleWidth = 0;
int resampleHeight = 0;
bool resampling = false; // this frame's eyes are

bool renderingEye = false;
bool spectatorView = false; // rendering the window's spectator camera (as an eye)
int currentEye = 0;
bool firstEye = false; // no other eye rendered before it this frame

void ensureResampleTarget(int width, int height)
{
    if(resampleTex && resampleWidth == width && resampleHeight == height)
    {
        return;
    }
    if(resampleTex)
    {
        GL_DeleteFramebuffersFunc(1, &resampleFbo);
        GL_DeleteNativeTexture(resampleTex);
    }
    glGenTextures(1, &resampleTex);
    GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D, resampleTex);
    GL_TexStorage2DFunc(GL_TEXTURE_2D, 1, GL_RGBA8, width, height);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D, 0);
    GL_GenFramebuffersFunc(1, &resampleFbo);
    GL_BindFramebufferFunc(GL_FRAMEBUFFER, resampleFbo);
    GL_FramebufferTexture2DFunc(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, resampleTex, 0);
    resampleWidth = width;
    resampleHeight = height;
}

// The spectator camera's MSAA (vr_spectator_aa): the window's (vid_fsaa), or none.
[[nodiscard]] float spectatorFsaa()
{
    return vr_spectator_aa.value > 0.f ? vid_fsaa.value : 0.f;
}

// Whether `t` is already made for this size, MSAA and format.
[[nodiscard]] bool sceneTargetsFit(const SceneTargets& t, int width, int height, float fsaa)
{
    return t.width == width && t.height == height && t.fsaa == fsaa && t.format == tonemap::sceneFormat();
}

void ensureSceneTargets(SceneTargets& t, int width, int height, float fsaa)
{
    const unsigned format = tonemap::sceneFormat();
    if(sceneTargetsFit(t, width, height, fsaa))
    {
        return;
    }
    t.fsaa = fsaa;
    t.format = format;

    const glframebufs_t windowFramebufs = framebufs;
    const int windowWidth = vid.width;
    const int windowHeight = vid.height;

    if(t.width)
    {
        framebufs = t.fb;
        GL_DeleteFrameBuffers();
    }

    vid.width = width;
    vid.height = height;
    creatingSceneTargets = true;
    creatingFormat = format;
    creatingFsaa = fsaa;
    GL_CreateFrameBuffers();
    creatingSceneTargets = false;
    t.fb = framebufs;

    framebufs = windowFramebufs;
    vid.width = windowWidth;
    vid.height = windowHeight;

    t.width = width;
    t.height = height;

    if(!targetFbo)
    {
        GL_GenFramebuffersFunc(1, &targetFbo);
    }
}

// The window's view (vr_window.cpp): a scene's colours (the eye's before its post-processing, which writes into the
// headset's image, or the spectator camera's) with its glow added, the tone curve and the grade as the
// post-processing does them (vr_bloom.cpp, vr_tonemap.cpp), so the window shows what the headset does (the window's
// own post-processing then applies the desktop's gamma and contrast). Each window pixel reads the scene through Map:
// the window's rectangle (-1..1) to the scene's uv, homogeneous (a crop, or the smoothed mirror's turn, a
// homography). The left eye as it is (Params.x 0) reads the nearest texel as it always has; the smoothed mirror and
// the spectator camera read it filtered (Catmull-Rom: sharp at any sub-pixel offset, so a slowly turning view does not
// pulse between sharp and soft as a bilinear read would; bilinear from a spectator camera larger than the window),
// black outside the scene, with the eyes' underwater wobble and blur (gl_shaders.h's post-process).
GLuint mirrorProgram = 0;
bool mirrorFailed = false;
GLuint linearSampler = 0; // unit 0, while the window's view reads a scene filtered

constexpr const char* mirrorVs = R"(#version 430
void main()
{
    ivec2 v = ivec2(gl_VertexID & 1, gl_VertexID >> 1);
    gl_Position = vec4(vec2(v) * 4.0 - 1.0, 0.0, 1.0);
}
)";

constexpr const char* mirrorFs = R"(#version 430
layout(binding = 0) uniform sampler2D Scene;
layout(binding = 1) uniform sampler2D Bloom;
layout(binding = 3) uniform sampler3D GradeLUT;
layout(location = 1) uniform vec4 Dest;   // the window's rectangle: x0, y0, 1 / width, 1 / height
layout(location = 2) uniform float BloomStrength;
layout(location = 3) uniform vec4 Tone;   // as the post-process's (vr_tonemap.hpp: tonemap::bind)
layout(location = 4) uniform vec4 Params; // x: 0 nearest (the left eye as it is), 1 Catmull-Rom, 2 bilinear
layout(location = 5) uniform vec4 WaterParams; // the post-process's: time, wobble, blur (both 0: not under water)
layout(location = 6) uniform vec4 WaterProj;   // ndc x = x + y * left / forward, ndc y = z + w * up / forward
layout(location = 7) uniform vec3 WaterFwd;    // the scene's view axes in the world
layout(location = 8) uniform vec3 WaterLeft;
layout(location = 9) uniform vec3 WaterUp;
layout(location = 10) uniform vec3 Map0;  // Map's columns: the window (-1..1, 1) to the scene's uv (homogeneous)
layout(location = 11) uniform vec3 Map1;
layout(location = 12) uniform vec3 Map2;
layout(location = 0) out vec4 Out;
)" QVR_TONE_GLSL R"(
// Catmull-Rom in nine bilinear reads (the middle two texels of each axis in one).
vec3 CatmullRom(vec2 uv)
{
    vec2 size = vec2(textureSize(Scene, 0));
    vec2 p = uv * size;
    vec2 t1 = floor(p - 0.5) + 0.5;
    vec2 f = p - t1;
    vec2 w0 = f * (-0.5 + f * (1.0 - 0.5 * f));
    vec2 w1 = 1.0 + f * f * (-2.5 + 1.5 * f);
    vec2 w2 = f * (0.5 + f * (2.0 - 1.5 * f));
    vec2 w3 = f * f * (-0.5 + 0.5 * f);
    vec2 w12 = w1 + w2;
    vec2 t0 = (t1 - 1.0) / size;
    vec2 t3 = (t1 + 2.0) / size;
    vec2 t12 = (t1 + w2 / w12) / size;
    vec3 c = (texture(Scene, vec2(t0.x, t0.y)).rgb * w0.x + texture(Scene, vec2(t12.x, t0.y)).rgb * w12.x +
              texture(Scene, vec2(t3.x, t0.y)).rgb * w3.x) * w0.y +
             (texture(Scene, vec2(t0.x, t12.y)).rgb * w0.x + texture(Scene, vec2(t12.x, t12.y)).rgb * w12.x +
              texture(Scene, vec2(t3.x, t12.y)).rgb * w3.x) * w12.y +
             (texture(Scene, vec2(t0.x, t3.y)).rgb * w0.x + texture(Scene, vec2(t12.x, t3.y)).rgb * w12.x +
              texture(Scene, vec2(t3.x, t3.y)).rgb * w3.x) * w3.y;
    return max(c, vec3(0.0));
}

void main()
{
    vec2 ndc = (gl_FragCoord.xy - Dest.xy) * Dest.zw * 2.0 - 1.0;
    vec3 h = mat3(Map0, Map1, Map2) * vec3(ndc, 1.0);
    vec2 uv = h.xy / h.z;
    vec3 c;
    if(Params.x == 0.0)
    {
        c = texture(Scene, uv).rgb;
    }
    else if(h.z <= 0.0 || any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0))))
    {
        c = vec3(0.0);
    }
    else if(WaterParams.y + WaterParams.z > 0.0)
    {
        // Under water: the post-process's wobble of the direction looked in, in the world, and its blur.
        vec2 size = vec2(textureSize(Scene, 0));
        vec2 sndc = uv * 2.0 - 1.0;
        vec3 dir = normalize(WaterFwd + WaterLeft * ((sndc.x - WaterProj.x) / WaterProj.y) +
                             WaterUp * ((sndc.y - WaterProj.z) / WaterProj.w));
        float t = WaterParams.x;
        vec2 w = vec2(sin(dot(dir, vec3(6.1, 2.3, 4.7)) + t * 1.1) + 0.5 * sin(dot(dir, vec3(-3.7, 8.3, 2.9)) - t * 1.6),
                      sin(dot(dir, vec3(2.9, -5.9, 6.3)) + t * 0.9) + 0.5 * sin(dot(dir, vec3(7.7, 1.9, -4.9)) + t * 1.4));
        vec2 aspect = vec2(size.y / size.x, 1.0);
        uv = clamp(uv + w * (WaterParams.y * aspect), vec2(0.0), vec2(1.0));
        vec2 b = max(WaterParams.z, 0.5 / size.y) * aspect;
        c = (texture(Scene, uv + b).rgb + texture(Scene, uv - b).rgb + texture(Scene, uv + vec2(b.x, -b.y)).rgb +
             texture(Scene, uv + vec2(-b.x, b.y)).rgb) * 0.25;
    }
    else if(Params.x == 1.0)
    {
        c = CatmullRom(uv);
    }
    else
    {
        c = texture(Scene, uv).rgb;
    }
    if(BloomStrength > 0.0)
    {
        vec2 bt = 0.5 / vec2(textureSize(Bloom, 0));
        c += (texture(Bloom, uv + vec2(-bt.x, -bt.y)).rgb + texture(Bloom, uv + vec2(bt.x, -bt.y)).rgb +
              texture(Bloom, uv + vec2(-bt.x, bt.y)).rgb + texture(Bloom, uv + vec2(bt.x, bt.y)).rgb) *
             (0.25 * BloomStrength);
    }
    if(Tone.x > 0.0)
        c = QvrTonemap(c * Tone.x, Tone.yz);
    if(Tone.w > 0.0)
        c = QvrGrade(GradeLUT, c, Tone.w);
    Out = vec4(c, 1.0);
}
)";

enum class Sampling
{
    Nearest,    // the left eye as it is
    CatmullRom, // the smoothed mirror, the spectator camera at the window's size or below
    Bilinear,   // the spectator camera above the window's size
};

// Draws the scene `source` (its colours, width x height) into the window's rectangle dx0..dx1 by windowHeight through
// `map`, with this view's glow (bloom::result) and underwater wobble.
void drawToWindow(const SceneTargets& source, const glm::mat3& map, Sampling sampling, GLuint windowTarget, int dx0,
    int dx1, int windowHeight)
{
    if(!mirrorProgram && !mirrorFailed)
    {
        mirrorProgram = gfx::glProgram(mirrorVs, mirrorFs, "vr mirror");
        mirrorFailed = !mirrorProgram;
    }
    if(mirrorFailed)
    {
        // The crop Map gives the corners of (a turn is not a blit: then the eye as it is).
        const glm::vec3 a = map * glm::vec3{-1.f, -1.f, 1.f}, b = map * glm::vec3{1.f, 1.f, 1.f};
        GL_BindFramebufferFunc(GL_READ_FRAMEBUFFER, source.fb.composite.fbo);
        GL_BindFramebufferFunc(GL_DRAW_FRAMEBUFFER, windowTarget);
        GL_BlitFramebufferFunc(static_cast<int>(a.x / a.z * source.width), static_cast<int>(a.y / a.z * source.height),
            static_cast<int>(b.x / b.z * source.width), static_cast<int>(b.y / b.z * source.height), dx0, 0, dx1,
            windowHeight, GL_COLOR_BUFFER_BIT, GL_LINEAR);
        GL_BindFramebufferFunc(GL_FRAMEBUFFER, windowTarget);
        return;
    }

    unsigned glow = 0;
    const bool hasGlow = bloom::result(glow);
    GL_BindFramebufferFunc(GL_FRAMEBUFFER, windowTarget);
    glViewport(dx0, 0, dx1 - dx0, windowHeight);
    GL_SetState(GLS_BLEND_OPAQUE | GLS_NO_ZTEST | GLS_NO_ZWRITE | GLS_CULL_NONE | GLS_ATTRIBS(0));
    GL_UseProgram(mirrorProgram);
    GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D, source.fb.composite.color_tex);
    GL_BindNative(GL_TEXTURE1, GL_TEXTURE_2D, hasGlow ? glow : 0);
    const bool filtered = sampling != Sampling::Nearest;
    if(filtered)
    {
        if(!linearSampler)
        {
            GL_GenSamplersFunc(1, &linearSampler);
            GL_SamplerParameteriFunc(linearSampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            GL_SamplerParameteriFunc(linearSampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            GL_SamplerParameteriFunc(linearSampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            GL_SamplerParameteriFunc(linearSampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        }
        GL_BindSamplerFunc(0, linearSampler);
    }
    GL_Uniform4fFunc(1, static_cast<float>(dx0), 0.f, 1.f / (dx1 - dx0), 1.f / windowHeight);
    GL_Uniform1fFunc(2, hasGlow ? 1.f : 0.f);
    const glm::vec4 tone = tonemap::bind(3);
    GL_Uniform4fFunc(3, tone.x, tone.y, tone.z, tone.w);
    GL_Uniform4fFunc(4, sampling == Sampling::Nearest ? 0.f : sampling == Sampling::CatmullRom ? 1.f : 2.f, 0.f, 0.f, 0.f);
    const glm::vec3 water = filtered ? water::viewWobble() : glm::vec3{0.f};
    GL_Uniform4fFunc(5, water.x, water.y, water.z, 0.f);
    GL_Uniform4fFunc(6, r_matproj[0], r_matproj[4], r_matproj[1], r_matproj[9]);
    GL_Uniform3fFunc(7, vpn[0], vpn[1], vpn[2]);
    GL_Uniform3fFunc(8, -vright[0], -vright[1], -vright[2]);
    GL_Uniform3fFunc(9, vup[0], vup[1], vup[2]);
    GL_Uniform3fFunc(10, map[0].x, map[0].y, map[0].z);
    GL_Uniform3fFunc(11, map[1].x, map[1].y, map[1].z);
    GL_Uniform3fFunc(12, map[2].x, map[2].y, map[2].z);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    if(filtered)
    {
        GL_BindSamplerFunc(0, 0);
    }
}

// The map of a crop: the window's rectangle (-1..1) onto the scene's uv rectangle x0, y0, width, height.
[[nodiscard]] glm::mat3 cropMap(float x0, float y0, float w, float h)
{
    return glm::mat3{glm::vec3{0.5f * w, 0.f, 0.f}, glm::vec3{0.f, 0.5f * h, 0.f},
        glm::vec3{x0 + 0.5f * w, y0 + 0.5f * h, 1.f}};
}

// The window's view of the eye just rendered: the left eye (or each, vr_mirror 2) cropped to the window's aspect
// ratio, or the smoothed mirror of the left eye.
void mirrorToWindow(int eye, GLuint windowTarget, int windowWidth, int windowHeight)
{
    const window::View view = window::view();
    if(view == window::View::Smoothed)
    {
        if(eye != 0)
        {
            return;
        }
        QVR_GPU_PROFILE("mirror");
        Backend* be = backend();
        const HiddenArea* hidden = be && vr_visibility_mask.value != 0.f ? be->hiddenArea(0) : nullptr;
        if(hidden && hidden->indices.empty())
        {
            hidden = nullptr;
        }
        const glm::mat3 map = window::mirrorMap(frameState().eyes[0].fov,
            static_cast<float>(windowWidth) / static_cast<float>(windowHeight), hidden);
        drawToWindow(eyeTargets, map, Sampling::CatmullRom, windowTarget, 0, windowWidth, windowHeight);
        return;
    }

    const int mode = static_cast<int>(vr_mirror.value);
    if(view != window::View::Raw || mode <= 0 || (mode == 1 && eye != 0))
    {
        return;
    }
    QVR_GPU_PROFILE("mirror");

    int dx0 = 0, dx1 = windowWidth;
    if(mode >= 2)
    {
        dx0 = eye * windowWidth / 2;
        dx1 = dx0 + windowWidth / 2;
    }

    const float eyeAspect = static_cast<float>(eyeTargets.width) / eyeTargets.height;
    const float windowAspect = static_cast<float>(dx1 - dx0) / windowHeight;

    int x0 = 0, y0 = 0, x1 = eyeTargets.width, y1 = eyeTargets.height;
    if(windowAspect > eyeAspect)
    {
        const int h = static_cast<int>(eyeTargets.width / windowAspect);
        y0 = (eyeTargets.height - h) / 2;
        y1 = y0 + h;
    }
    else
    {
        const int w = static_cast<int>(eyeTargets.height * windowAspect);
        x0 = (eyeTargets.width - w) / 2;
        x1 = x0 + w;
    }
    const float ew = static_cast<float>(eyeTargets.width);
    const float eh = static_cast<float>(eyeTargets.height);
    drawToWindow(eyeTargets, cropMap(x0 / ew, y0 / eh, (x1 - x0) / ew, (y1 - y0) / eh), Sampling::Nearest, windowTarget,
        dx0, dx1, windowHeight);
}

// The UI in an eye, into `fbo` (width x height): the lasers, the HUD panel or the menu (with its pointer), the wrist
// gadget's log. Not depth tested: over whatever the fbo holds.
void drawUi(const glm::vec3& viewOrigin, GLuint fbo, int width, int height)
{
    QVR_GPU_PROFILE("ui");
    GL_BindFramebufferFunc(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, width, height);
    lines::drawInEye(viewOrigin);
    panel::drawInEye(hands::current());
    text3d::drawOverlay();
}

// Whether the window mirrors this eye (mirrorToWindow).
[[nodiscard]] bool mirrored(int eye)
{
    switch(window::view())
    {
    case window::View::Raw:
    {
        const int mode = static_cast<int>(vr_mirror.value);
        return mode >= 2 || (mode == 1 && eye == 0);
    }
    case window::View::Smoothed:
        return eye == 0;
    default:
        return false;
    }
}

// The lenses' hidden area (vr_visibility_mask): the runtime's hidden triangles, from the eye's
// tangent space to clip space by the eye's field of view, drawn black at the near plane (depth
// 1 with Ironwail's reversed Z, 0 without) right after the scene's clear: every later depth-tested
// fragment there fails early, so the world's, models' and particles' shaders skip those pixels,
// and bloom and the mirror see black.
std::vector<gfx::Vertex> hiddenTriangles;

void drawHiddenArea()
{
    Backend* be = backend();
    const HiddenArea* h = be && vr_visibility_mask.value != 0.f ? be->hiddenArea(currentEye) : nullptr;
    if(!h || h->indices.empty())
    {
        return;
    }
    QVR_GPU_PROFILE("hidden area");

    const Fov& fov = frameState().eyes[currentEye].fov;
    const float l = std::tan(fov.left), r = std::tan(fov.right), u = std::tan(fov.up), d = std::tan(fov.down);
    if(r - l <= 0.f || u - d <= 0.f)
    {
        return;
    }
    const float z = gl_clipcontrol_able ? 1.f : -1.f;
    hiddenTriangles.clear();
    for(const std::uint32_t i : h->indices)
    {
        const glm::vec2 t = h->vertices[i];
        gfx::Vertex v;
        v.pos = {(2.f * t.x - (r + l)) / (r - l), (2.f * t.y - (u + d)) / (u - d), z};
        v.color = glm::vec4{0.f};
        hiddenTriangles.push_back(v);
    }

    gfx::State state;
    state.shade = gfx::Shade::Color;
    state.blend = gfx::Blend::Opaque;
    state.depthTest = true; // passes against the clear (GL skips depth writes without the test)
    state.depthWrite = true;
    gfx::draw(hiddenTriangles, glm::mat4{1.f}, state);
}

// The window's spectator camera (vr_window.cpp): the scene a third time, after the eyes (their images already given to
// the runtime), from the steadied head with the camera's field of view, at the window's size times
// vr_spectator_scale, as the eyes render it (the same entities, lights, shadow maps, particles, glow and UI: the
// hands, weapons, body, flashlight, lasers, HUD panel and menu where they are in the world), then drawn into the
// window. The window's state (framebufs, vid, the viewport) is the caller's to put back.
// The spectator camera's pace (vr_spectator_rate): drawn every frame, every 2nd or 3rd, or at most so many times a
// second; between, the window shows its last image again (the window pass alone). The steadied head it is drawn from
// follows the head every frame (window::update), so each image is where the camera is at its time.
struct SpectatorPace
{
    double lastFrame = -1.0; // realtime at the last VR frame that showed the spectator camera
    double owed = 0.0;       // seconds since the last image, less the cap's period (at most one period)
    int frames = 0;          // frames since the last image (every 2nd or 3rd)
    bool shown = false;      // the targets hold an image of this window view (made since the view came back)
};
SpectatorPace spectatorPace;

// Whether this frame draws the camera anew. `fits`: its targets are made for this frame's size and MSAA (else it
// must, as they are made again).
[[nodiscard]] bool spectatorDue(bool fits)
{
    SpectatorPace& p = spectatorPace;
    const double now = realtime;
    const bool continued = p.shown && fits && p.lastFrame >= 0.0 && now - p.lastFrame < 0.25;
    const double dt = continued ? now - p.lastFrame : 0.0;
    p.lastFrame = now;
    const int rate = static_cast<int>(vr_spectator_rate.value);
    if(!continued || rate <= 1)
    {
        p.owed = 0.0;
        p.frames = 0;
        return true;
    }
    if(rate <= 3)
    {
        if(++p.frames < rate)
        {
            return false;
        }
        p.frames = 0;
        return true;
    }
    // A cap: an image once a period has passed (half a millisecond early is on time: a frame's jitter must not skip
    // it), the time over kept for the next (at most a period: no burst after a hitch).
    const double period = 1.0 / static_cast<double>(rate);
    p.owed += dt;
    if(p.owed < period - 0.0005)
    {
        return false;
    }
    p.owed = std::clamp(p.owed - period, 0.0, period);
    return true;
}

void renderSpectator(GLuint windowTarget, int windowWidth, int windowHeight)
{
    const float scale = window::spectatorScale();
    const int width = std::max(16, static_cast<int>(std::lround(windowWidth * scale)));
    const int height = std::max(16, static_cast<int>(std::lround(windowHeight * scale)));
    const float fsaa = spectatorFsaa();
    if(!spectatorDue(sceneTargetsFit(spectatorTargets, width, height, fsaa)))
    {
        // Its last image again.
        QVR_GPU_PROFILE("window view");
        drawToWindow(spectatorTargets, cropMap(0.f, 0.f, 1.f, 1.f), scale > 1.f ? Sampling::Bilinear : Sampling::CatmullRom,
            windowTarget, 0, windowWidth, windowHeight);
        return;
    }
    QVR_GPU_PROFILE("spectator");
    ensureSceneTargets(spectatorTargets, width, height, fsaa);
    spectatorPace.shown = true;
    const window::Camera& camera =
        window::spectator(static_cast<float>(windowWidth) / static_cast<float>(windowHeight));

    framebufs = spectatorTargets.fb;
    vid.width = width;
    vid.height = height;
    glx = gly = 0;
    glwidth = width;
    glheight = height;
    r_refdef.vrect.x = r_refdef.vrect.y = 0;
    r_refdef.vrect.width = width;
    r_refdef.vrect.height = height;

    renderingEye = true;
    spectatorView = true;
    currentEye = 0;
    firstEye = false; // the eyes' entities, as they set them up
    V_RenderView();
    bloom::apply(framebufs.composite.color_tex, width, height);
    drawUi(camera.origin, framebufs.composite.fbo, width, height);
    spectatorView = false;
    renderingEye = false;

    QVR_GPU_PROFILE("window view");
    drawToWindow(spectatorTargets, cropMap(0.f, 0.f, 1.f, 1.f), scale > 1.f ? Sampling::Bilinear : Sampling::CatmullRom,
        windowTarget, 0, windowWidth, windowHeight);
}

} // namespace

bool isSpectator()
{
    return spectatorView;
}

bool isRenderingEye()
{
    return renderingEye;
}

int eye()
{
    return currentEye;
}

bool isFirstEye()
{
    return firstEye;
}

} // namespace qvr::stereo

using namespace qvr;

extern "C" int VR_RenderView()
{
    Backend* be = backend();
    const FrameState& frame = frameState();

    // frameActive: not again after the frame was finished (a loading plaque redraws the screen).
    if(!be || !frame.shouldRender || !be->frameActive() || cls.state != ca_connected || cls.signon != SIGNONS ||
        !cl.worldmodel || con_forcedup || !hands::current().valid)
    {
        return 0; // the backend ends the frame without layers
    }

    // The eye images' size (the runtime's, fixed for the session), and the size the eyes are
    // rendered at (vr_render_scale times it), resampled into the images when it differs.
    int imageWidth = 0, imageHeight = 0;
    be->eyeResolution(imageWidth, imageHeight);
    if(imageWidth <= 0 || imageHeight <= 0)
    {
        return 0;
    }
    const EyeSizes sizes = be->eyeSizes();
    const int width = scaledEyeSize(imageWidth, sizes.maxWidth);
    const int height = scaledEyeSize(imageHeight, sizes.maxHeight);
    stereo::resampling = width != imageWidth || height != imageHeight || upscale::sharpenAtNative();

    stereo::ensureSceneTargets(stereo::eyeTargets, width, height, vid_fsaa.value);
    if(stereo::resampling)
    {
        stereo::ensureResampleTarget(width, height);
    }

    const glframebufs_t windowFramebufs = framebufs;
    const int windowWidth = vid.width, windowHeight = vid.height;
    const GLuint windowTarget = GL_NeedsPostprocess() ? framebufs.composite.fbo : 0;
    const int savedGlx = glx, savedGly = gly, savedGlwidth = glwidth, savedGlheight = glheight;
    const vrect_t savedVrect = r_refdef.vrect;
    const float savedFovX = r_refdef.fov_x, savedFovY = r_refdef.fov_y;

    window::update(frame, hands::current()); // what the window shows, the steadied head
    crosshair::queue(hands::current());
    fgfx::queue(hands::current());
    meleehud::queue(hands::current()); // the counter glow (vr_counter_glow)
    body::queueDebug(hands::current());
    envmap::update(); // the weapons' reflections: a face of the cube, once for both eyes (vr_envmap.cpp)

    int eyesRendered = 0;
    for(int eye = 0; eye < 2; eye++)
    {
        // The runtime's calls are GPU scopes too: the GPU time between the eyes' own scopes
        // (the runtime's work on this context, and the GPU idle while the CPU waits in them).
        profile::begin("xr acquire", true); // xrWaitSwapchainImage
        const unsigned image = be->acquireEyeImage(eye);
        profile::end();
        if(!image)
        {
            continue;
        }

        GL_BindFramebufferFunc(GL_FRAMEBUFFER, stereo::targetFbo);
        GL_FramebufferTexture2DFunc(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, image, 0);

        framebufs = stereo::eyeTargets.fb;
        vid.width = width;
        vid.height = height;
        glx = gly = 0;
        glwidth = width;
        glheight = height;
        r_refdef.vrect.x = r_refdef.vrect.y = 0;
        r_refdef.vrect.width = width;
        r_refdef.vrect.height = height;

        stereo::renderingEye = true;
        stereo::currentEye = eye;
        stereo::firstEye = eyesRendered == 0;
        QVR_GPU_PROFILE(eye == 0 ? "eye L" : "eye R");

        V_RenderView();
        foveated::endScene(); // begun after the scene's clear (VR_DrawHiddenArea)
        bloom::apply(framebufs.composite.color_tex, width, height); // added by GL_PostProcess

        // vr_eyeshot 1 takes the eye's final image (after the resample and vr_foveated_debug, before the UI); 2 the
        // rendered one (before the resample) with its float scene.
        const bool shotFinal = vr_eyeshot.value > 0.f && vr_eyeshot.value < 2.f;
        profile::begin("postprocess", true);
        GL_PostProcess(); // into the image, or the resample target (VR_PostProcessTarget)
        if(!shotFinal)
        {
            tonemap::eyeshot(eye, VR_PostProcessTarget(), framebufs.composite.fbo, width, height);
        }
        profile::end();
        if(stereo::resampling)
        {
            // After the tone curve, grade, gamma and dither (FSR and NIS want the display's colours), before the UI.
            QVR_GPU_PROFILE("upscale");
            upscale::resample(eye, stereo::resampleTex, stereo::resampleFbo, width, height, stereo::targetFbo,
                imageWidth, imageHeight);
        }
        foveated::drawDebug(eye, stereo::targetFbo, imageWidth, imageHeight, width, height); // vr_foveated_debug
        if(stereo::mirrored(eye))
        {
            foveated::drawDebug(eye, framebufs.composite.fbo, width, height, width, height);
        }
        if(shotFinal)
        {
            tonemap::eyeshot(eye, stereo::targetFbo, framebufs.composite.fbo, imageWidth, imageHeight);
        }

        // The UI over the eye's final image, at its full size: after the post-processing, it is not warped or blurred
        // under water (vr_water.cpp), the glow is not added over it, nor the eye's gamma. The wrist gadget and all
        // else in the world are in the scene. Over the scene's colours too, for the mirror.
        stereo::drawUi(hands::current().eyeOrigin[eye], stereo::targetFbo, imageWidth, imageHeight);
        if(stereo::mirrored(eye))
        {
            stereo::drawUi(hands::current().eyeOrigin[eye], framebufs.composite.fbo, width, height);
        }

        stereo::renderingEye = false;
        profile::begin("xr release", true); // xrReleaseSwapchainImage
        be->releaseEyeImage(eye);
        profile::end();
        ++eyesRendered;

        // Released images belong to the runtime again: do not keep them attached.
        GL_BindFramebufferFunc(GL_FRAMEBUFFER, stereo::targetFbo);
        GL_FramebufferTexture2DFunc(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);

        stereo::mirrorToWindow(eye, windowTarget, windowWidth, windowHeight);
    }

    GL_BindFramebufferFunc(GL_FRAMEBUFFER, windowTarget);

    // Both eyes, or no projection layer at all (an eye's image could not be acquired).
    profile::begin("xr submit", true); // xrEndFrame
    be->endFrame(eyesRendered == 2);
    profile::end();

    // The window's spectator camera, once the headset has its images: it does not delay them.
    if(window::view() == window::View::Spectator && eyesRendered == 2)
    {
        stereo::renderSpectator(windowTarget, windowWidth, windowHeight);
    }
    else
    {
        stereo::spectatorPace.shown = false; // (drawn anew when it comes back)
    }

    framebufs = windowFramebufs;
    vid.width = windowWidth;
    vid.height = windowHeight;
    glx = savedGlx;
    gly = savedGly;
    glwidth = savedGlwidth;
    glheight = savedGlheight;
    r_refdef.vrect = savedVrect;
    r_refdef.fov_x = savedFovX;
    r_refdef.fov_y = savedFovY;
    GL_BindFramebufferFunc(GL_FRAMEBUFFER, windowTarget);
    panel::setStereoThisFrame(eyesRendered == 2);
    return eyesRendered == 2;
}

// vr_mock_fast 2 (VR_SkipScreen): what VR_RenderView does that the game reads, without the GL: each eye's view set
// up (V_SetupView: the refdef, and with the first eye the view entities, the weapons' and hands' poses: the melee's
// samples), at the eyes' sizes. Not the drawing's queues (crosshair, HUD glow...) nor the window: nothing is drawn.
extern "C" void VR_HeadlessView()
{
    Backend* be = backend();
    const FrameState& frame = frameState();
    if(!be || !frame.shouldRender || !be->frameActive() || cls.state != ca_connected || cls.signon != SIGNONS ||
        !cl.worldmodel || con_forcedup || !hands::current().valid)
    {
        return;
    }
    int imageWidth = 0, imageHeight = 0;
    be->eyeResolution(imageWidth, imageHeight);
    if(imageWidth <= 0 || imageHeight <= 0)
    {
        return;
    }
    const EyeSizes sizes = be->eyeSizes();
    const int width = scaledEyeSize(imageWidth, sizes.maxWidth);
    const int height = scaledEyeSize(imageHeight, sizes.maxHeight);

    const int windowWidth = vid.width, windowHeight = vid.height;
    const int savedGlx = glx, savedGly = gly, savedGlwidth = glwidth, savedGlheight = glheight;
    const vrect_t savedVrect = r_refdef.vrect;
    const float savedFovX = r_refdef.fov_x, savedFovY = r_refdef.fov_y;
    for(int eye = 0; eye < 2; eye++)
    {
        vid.width = width;
        vid.height = height;
        glx = gly = 0;
        glwidth = width;
        glheight = height;
        r_refdef.vrect.x = r_refdef.vrect.y = 0;
        r_refdef.vrect.width = width;
        r_refdef.vrect.height = height;
        stereo::renderingEye = true;
        stereo::currentEye = eye;
        stereo::firstEye = eye == 0;
        V_SetupView();
        stereo::renderingEye = false;
    }
    vid.width = windowWidth;
    vid.height = windowHeight;
    glx = savedGlx;
    gly = savedGly;
    glwidth = savedGlwidth;
    glheight = savedGlheight;
    r_refdef.vrect = savedVrect;
    r_refdef.fov_x = savedFovX;
    r_refdef.fov_y = savedFovY;
}

extern "C" int VR_RenderingEye()
{
    return stereo::renderingEye;
}

extern "C" void VR_DrawHiddenArea()
{
    if(stereo::renderingEye && !stereo::spectatorView) // no lenses in front of the spectator camera
    {
        stereo::drawHiddenArea();
        foveated::beginScene(stereo::currentEye, vid.width, vid.height); // vr_foveated, until the scene ends
    }
}

extern "C" unsigned VR_SceneColorFormat(unsigned format)
{
    return stereo::creatingSceneTargets ? stereo::creatingFormat : format;
}

extern "C" int VR_SceneSamples(int samples)
{
    return stereo::creatingSceneTargets ? Q_nextPow2(static_cast<int>(q_max(1.f, stereo::creatingFsaa))) : samples;
}

extern "C" unsigned VR_PostProcessTarget()
{
    if(!stereo::renderingEye)
    {
        return 0;
    }
    return stereo::resampling ? stereo::resampleFbo : stereo::targetFbo;
}

// Replaces the symmetric projection with the eye's asymmetric one. Ironwail's projection maps
// Quake view space (x forward, y left, z up) to clip space; only the terms producing clip x/y
// change, the (reversed-Z) depth terms are kept.
extern "C" void VR_OverrideProjection(float matrix[16])
{
    if(!stereo::renderingEye)
    {
        return;
    }

    float l, r, u, d;
    if(stereo::spectatorView)
    {
        const window::Camera& c = window::spectatorCamera();
        l = -c.tanX;
        r = c.tanX;
        u = c.tanY;
        d = -c.tanY;
    }
    else
    {
        const Fov& fov = frameState().eyes[stereo::currentEye].fov;
        l = std::tan(fov.left);
        r = std::tan(fov.right);
        u = std::tan(fov.up);
        d = std::tan(fov.down);
    }

    matrix[1 * 4 + 0] = -2.f / (r - l);   // clip x from -y (right)
    matrix[0 * 4 + 0] = -(r + l) / (r - l); // off-axis shift, times depth (x)
    matrix[2 * 4 + 1] = 2.f / (u - d);    // clip y from z (up)
    matrix[0 * 4 + 1] = -(u + d) / (u - d);

    // Hands, weapons and the body come much closer to the eyes than to a monitor's view: the
    // desktop's near plane (up to 4 units, 12 cm) cut them open. Reversed Z keeps the precision.
    const float n = CLAMP(0.1f, vr_nearclip.value, 4.f);
    const float f = gl_farclip.value;
    if(gl_clipcontrol_able)
    {
        matrix[0 * 4 + 2] = -n / (f - n);
        matrix[3 * 4 + 2] = f * n / (f - n);
    }
    else
    {
        matrix[0 * 4 + 2] = (f + n) / (f - n);
        matrix[3 * 4 + 2] = -2.f * f * n / (f - n);
    }
}
