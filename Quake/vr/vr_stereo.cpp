// vr_stereo.cpp -- renders each eye with Ironwail's normal pipeline.
//
// For each eye, an eye-sized set of Ironwail's framebuffers is swapped in, the view is moved
// to the eye (vr_view.cpp) with the eye's asymmetric projection (VR_OverrideProjection), the
// usual V_RenderView runs, and Ironwail's post-process pass (gamma, contrast, dithering)
// writes into the backend's eye image instead of the window (at a vr_render_scale other than 1,
// into a texture of the scaled size, resampled into the image). The left eye is then mirrored to
// the window, where the 2D layer is drawn as usual.

#include "vr_fgfx.hpp"
#include "vr_gfx.hpp"
#include "vr_bloom.hpp"
#include "vr_body.hpp"
#include "vr_envmap.hpp"
#include "vr_engine.hpp"
#include "vr_crosshair.hpp"
#include "vr_cvars.hpp"
#include "vr_hands.hpp"
#include "vr_lines.hpp"
#include "vr_main.hpp"
#include "vr_panel.hpp"
#include "vr_profile.hpp"
#include "vr_stereo.hpp"
#include "vr_text3d.hpp"
#include "vr_tonemap.hpp"

#include <cmath>
#include <cstdint>
#include <vector>

namespace qvr::stereo
{
namespace
{

glframebufs_t eyeFramebufs{};
int eyeFramebufsWidth = 0;
int eyeFramebufsHeight = 0;
float eyeFramebufsFsaa = 0.f; // vid_fsaa they were made with
unsigned eyeFramebufsFormat = 0; // their scene colour format (vr_tonemap: float)
bool creatingEyeFramebufs = false; // VR_SceneColorFormat
GLuint targetFbo = 0;

// vr_render_scale: the eyes are rendered at the scaled size, post-processed into this texture of
// that size, and resampled (a linear blit) into the eye image, which keeps the runtime's size.
GLuint resampleFbo = 0;
GLuint resampleTex = 0;
int resampleWidth = 0;
int resampleHeight = 0;
bool resampling = false; // this frame's eyes are

bool renderingEye = false;
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

void ensureEyeFramebuffers(int width, int height)
{
    const unsigned format = tonemap::sceneFormat();
    if(eyeFramebufsWidth == width && eyeFramebufsHeight == height && eyeFramebufsFsaa == vid_fsaa.value &&
        eyeFramebufsFormat == format)
    {
        return;
    }
    eyeFramebufsFsaa = vid_fsaa.value;
    eyeFramebufsFormat = format;

    const glframebufs_t windowFramebufs = framebufs;
    const int windowWidth = vid.width;
    const int windowHeight = vid.height;

    if(eyeFramebufsWidth)
    {
        framebufs = eyeFramebufs;
        GL_DeleteFrameBuffers();
    }

    vid.width = width;
    vid.height = height;
    creatingEyeFramebufs = true;
    GL_CreateFrameBuffers();
    creatingEyeFramebufs = false;
    eyeFramebufs = framebufs;

    framebufs = windowFramebufs;
    vid.width = windowWidth;
    vid.height = windowHeight;

    eyeFramebufsWidth = width;
    eyeFramebufsHeight = height;

    if(!targetFbo)
    {
        GL_GenFramebuffersFunc(1, &targetFbo);
    }
}

// The mirror: the eye's scene (before its post-processing, which writes into the headset's image)
// with the eye's glow added, the tone curve and the grade as the post-processing does them (vr_bloom.cpp,
// vr_tonemap.cpp), so the window shows what the headset does (the window's own post-processing then
// applies the desktop's gamma and contrast).
GLuint mirrorProgram = 0;
bool mirrorFailed = false;

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
layout(location = 0) uniform vec4 Source; // the eye's rectangle in uv: x0, y0, width, height
layout(location = 1) uniform vec4 Dest;   // the window's rectangle: x0, y0, 1 / width, 1 / height
layout(location = 2) uniform float BloomStrength;
layout(location = 3) uniform vec4 Tone;   // as the post-process's (vr_tonemap.hpp: tonemap::bind)
layout(location = 0) out vec4 Out;
)" QVR_TONE_GLSL R"(
void main()
{
    vec2 uv = Source.xy + (gl_FragCoord.xy - Dest.xy) * Dest.zw * Source.zw;
    vec3 c = texture(Scene, uv).rgb;
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

// Copies the eye just rendered into (its part of) the window, cropped to the aspect ratio.
void mirrorToWindow(int eye, GLuint windowTarget, int windowWidth, int windowHeight)
{
    QVR_GPU_PROFILE("mirror");
    const int mode = static_cast<int>(vr_mirror.value);
    if(mode <= 0 || (mode == 1 && eye != 0))
    {
        return;
    }

    int dx0 = 0, dx1 = windowWidth;
    if(mode >= 2)
    {
        dx0 = eye * windowWidth / 2;
        dx1 = dx0 + windowWidth / 2;
    }

    const float eyeAspect = static_cast<float>(eyeFramebufsWidth) / eyeFramebufsHeight;
    const float windowAspect = static_cast<float>(dx1 - dx0) / windowHeight;

    int x0 = 0, y0 = 0, x1 = eyeFramebufsWidth, y1 = eyeFramebufsHeight;
    if(windowAspect > eyeAspect)
    {
        const int h = static_cast<int>(eyeFramebufsWidth / windowAspect);
        y0 = (eyeFramebufsHeight - h) / 2;
        y1 = y0 + h;
    }
    else
    {
        const int w = static_cast<int>(eyeFramebufsHeight * windowAspect);
        x0 = (eyeFramebufsWidth - w) / 2;
        x1 = x0 + w;
    }

    if(!mirrorProgram && !mirrorFailed)
    {
        mirrorProgram = gfx::glProgram(mirrorVs, mirrorFs, "vr mirror");
        mirrorFailed = !mirrorProgram;
    }
    if(mirrorFailed)
    {
        GL_BindFramebufferFunc(GL_READ_FRAMEBUFFER, eyeFramebufs.composite.fbo);
        GL_BindFramebufferFunc(GL_DRAW_FRAMEBUFFER, windowTarget);
        GL_BlitFramebufferFunc(x0, y0, x1, y1, dx0, 0, dx1, windowHeight, GL_COLOR_BUFFER_BIT, GL_LINEAR);
        GL_BindFramebufferFunc(GL_FRAMEBUFFER, windowTarget);
        return;
    }

    unsigned glow = 0;
    const bool hasGlow = bloom::result(glow);
    GL_BindFramebufferFunc(GL_FRAMEBUFFER, windowTarget);
    glViewport(dx0, 0, dx1 - dx0, windowHeight);
    GL_SetState(GLS_BLEND_OPAQUE | GLS_NO_ZTEST | GLS_NO_ZWRITE | GLS_CULL_NONE | GLS_ATTRIBS(0));
    GL_UseProgram(mirrorProgram);
    GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D, eyeFramebufs.composite.color_tex);
    GL_BindNative(GL_TEXTURE1, GL_TEXTURE_2D, hasGlow ? glow : 0);
    const float ew = static_cast<float>(eyeFramebufsWidth);
    const float eh = static_cast<float>(eyeFramebufsHeight);
    GL_Uniform4fFunc(0, x0 / ew, y0 / eh, (x1 - x0) / ew, (y1 - y0) / eh);
    GL_Uniform4fFunc(1, static_cast<float>(dx0), 0.f, 1.f / (dx1 - dx0), 1.f / windowHeight);
    GL_Uniform1fFunc(2, hasGlow ? 1.f : 0.f);
    const glm::vec4 tone = tonemap::bind(3);
    GL_Uniform4fFunc(3, tone.x, tone.y, tone.z, tone.w);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

// The UI in an eye, into `fbo` (width x height): the lasers, the HUD panel or the menu (with its pointer), the wrist
// gadget's log. Not depth tested: over whatever the fbo holds.
void drawUi(int eye, GLuint fbo, int width, int height)
{
    QVR_GPU_PROFILE("ui");
    GL_BindFramebufferFunc(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, width, height);
    lines::drawInEye(hands::current().eyeOrigin[eye]);
    panel::drawInEye(hands::current());
    text3d::drawOverlay();
}

// Whether the window mirrors this eye (mirrorToWindow).
[[nodiscard]] bool mirrored(int eye)
{
    const int mode = static_cast<int>(vr_mirror.value);
    return mode >= 2 || (mode == 1 && eye == 0);
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

} // namespace

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
    stereo::resampling = width != imageWidth || height != imageHeight;

    stereo::ensureEyeFramebuffers(width, height);
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

    crosshair::queue(hands::current());
    fgfx::queue(hands::current());
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

        framebufs = stereo::eyeFramebufs;
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
        bloom::apply(framebufs.composite.color_tex, width, height); // added by GL_PostProcess

        profile::begin("postprocess", true);
        GL_PostProcess(); // into the image, or the resample target (VR_PostProcessTarget)
        tonemap::eyeshot(eye, VR_PostProcessTarget(), framebufs.composite.fbo, width, height); // vr_eyeshot
        if(stereo::resampling)
        {
            GL_BindFramebufferFunc(GL_READ_FRAMEBUFFER, stereo::resampleFbo);
            GL_BindFramebufferFunc(GL_DRAW_FRAMEBUFFER, stereo::targetFbo);
            GL_BlitFramebufferFunc(0, 0, width, height, 0, 0, imageWidth, imageHeight, GL_COLOR_BUFFER_BIT, GL_LINEAR);
            GL_BindFramebufferFunc(GL_FRAMEBUFFER, stereo::targetFbo);
        }
        profile::end();

        // The UI over the eye's final image, at its full size: after the post-processing, it is not warped or blurred
        // under water (vr_water.cpp), the glow is not added over it, nor the eye's gamma. The wrist gadget and all
        // else in the world are in the scene. Over the scene's colours too, for the mirror.
        stereo::drawUi(eye, stereo::targetFbo, imageWidth, imageHeight);
        if(stereo::mirrored(eye))
        {
            stereo::drawUi(eye, framebufs.composite.fbo, width, height);
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

    // Both eyes, or no projection layer at all (an eye's image could not be acquired).
    profile::begin("xr submit", true); // xrEndFrame
    be->endFrame(eyesRendered == 2);
    profile::end();
    panel::setStereoThisFrame(eyesRendered == 2);
    return eyesRendered == 2;
}

extern "C" int VR_RenderingEye()
{
    return stereo::renderingEye;
}

extern "C" void VR_DrawHiddenArea()
{
    if(stereo::renderingEye)
    {
        stereo::drawHiddenArea();
    }
}

extern "C" unsigned VR_SceneColorFormat(unsigned format)
{
    return stereo::creatingEyeFramebufs ? stereo::eyeFramebufsFormat : format;
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

    const Fov& fov = frameState().eyes[stereo::currentEye].fov;
    const float l = std::tan(fov.left);
    const float r = std::tan(fov.right);
    const float u = std::tan(fov.up);
    const float d = std::tan(fov.down);

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
