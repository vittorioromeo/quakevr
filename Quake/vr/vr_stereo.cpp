// vr_stereo.cpp -- renders each eye with Ironwail's normal pipeline.
//
// For each eye, an eye-sized set of Ironwail's framebuffers is swapped in, the view is moved
// to the eye (vr_view.cpp) with the eye's asymmetric projection (VR_OverrideProjection), the
// usual V_RenderView runs, and Ironwail's post-process pass (gamma, contrast, dithering)
// writes into the backend's eye image instead of the window (at a vr_render_scale other than 1,
// into a texture of the scaled size, resampled into the image: bilinear, FSR 1 or NIS, vr_upscale.cpp).
// The scene is shaded coarser away from the lens centre with vr_foveated (vr_foveated.cpp). The eye's
// UI is drawn over the final image at its full size. The eye the window shows (or both: vr_window_view) is then
// mirrored to it, where the 2D layer is drawn as usual.

#include "vr_bench.hpp"
#include "vr_fgfx.hpp"
#include "vr_flatlook.hpp"
#include "vr_gfx.hpp"
#include "vr_glutil.hpp"
#include "vr_bloom.hpp"
#include "vr_bullettime.hpp"
#include "vr_body.hpp"
#include "vr_envmap.hpp"
#include "vr_foveated.hpp"
#include "vr_engine.hpp"
#include "vr_crosshair.hpp"
#include "vr_cvars.hpp"
#include "vr_dynres.hpp"
#include "vr_gfxpresets.hpp"
#include "vr_flatplay.hpp"
#include "vr_hands.hpp"
#include "vr_haze.hpp"
#include "vr_lines.hpp"
#include "vr_loading.hpp"
#include "vr_main.hpp"
#include "vr_meleehud.hpp"
#include "vr_menuui.hpp"
#include "vr_panel.hpp"
#include "vr_portals.hpp"
#include "vr_profile.hpp"
#include "vr_stereo.hpp"
#include "vr_stereo_single.hpp"
#include "vr_text3d.hpp"
#include "vr_takes.hpp"
#include "vr_temporal.hpp"
#include "vr_testkit.hpp"
#include "vr_tonemap.hpp"

#if defined(SDL_FRAMEWORK) || defined(NO_SDL_CONFIG)
#include <SDL2/SDL.h>
#else
#include "SDL.h"
#endif
#include "vr_viewuv_glsl.h"
#include "vr_tracy.h"
#include "vr_tunables.hpp"
#include "vr_upscale.hpp"
#include "vr_water.hpp"
#include "vr_window.hpp"

#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Base/Memcmp.hpp"
#include "Zancle/Base/Strcmp.hpp"
#include "Zancle/Base/Swap.hpp"
#include "Zancle/Container/Array.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Lround.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Tan.hpp"
#include "vr_api.h"
#include "vr_api_render.h"


namespace qvr::stereo
{
namespace
{

// A set of Ironwail's framebuffers of one size: the eyes', and the window's spectator camera's (vr_window.cpp).
// Allocated at allocatedWidth x allocatedHeight (what vid.width/vid.height say while they are drawn into); a frame draws
// their bottom left drawnWidth x drawnHeight (useView), the rest left as it was.
struct SceneTargets
{
    glframebufs_t fb{};
    int allocatedWidth = 0;
    int allocatedHeight = 0;
    int drawnWidth = 0; // the part the last frame drew
    int drawnHeight = 0;
    float fsaa = 0.f;    // the MSAA they were made with (vid_fsaa; the spectator camera's: vr_spectator_aa)
    unsigned format = 0; // their scene colour format (vr_tonemap: float)
    unsigned depth = 0;  // their depth/stencil format (vr_depth_float)
    unsigned generation = 0; // bumped each time they are made (single-pass stereo's framebuffers follow)
    bool foveated = false;   // their scene colour foveated (vr_gl_foveation)
    int foveatedParts = 0;   // which of their attachments (vr_gl_foveation_parts)
};
SceneTargets eyeTargets;
// vr_eye_targets_split: the right eye's own targets, swapped into eyeTargets while it is drawn (QUEST_PLAN.md 6o).
SceneTargets rightEyeTargets;
SceneTargets spectatorTargets;
// The views through teleporters: one set of scene targets (each view is drawn and copied out before the next begins:
// a view's own views before it, vr_portals_recursion) and a texture array for each depth of view (1..4), whose layers
// the cameras one depth up show.
struct PortalTargets
{
    SceneTargets scene;
    GLuint fbo = 0;
    za::Array<GLuint, 4> array{};
    za::Array<int, 4> layers{};
};
PortalTargets eyePortals, spectatorPortals; // separate sizes avoid reallocating every camera each frame
bool creatingSceneTargets = false; // VR_SceneColorFormat, VR_SceneSamples
unsigned creatingFormat = 0;
unsigned creatingDepth = 0;
bool flatFloatDepth = false; // the window's targets were made with the eyes' float depth for flatscreen play (VR_SceneDepthFormat)
float creatingFsaa = 0.f;
bool creatingFoveated = false; // VR_SceneAttachmentFoveation: the eyes' scene colour foveated
int creatingFoveatedParts = 0;  // and which attachments (vr_gl_foveation_parts)
GLenum foveationIncomplete = 0; // VR_SceneFoveationIncomplete: a foveated eye framebuffer's status while they are made
GLuint targetFbo = 0;
// vr_xr_direct 2 (C5's probe, QUEST_PLAN.md 6w; qfov2): the eye image with a depth/stencil renderbuffer of its own (no
// texture: a foveated colour beside an unfoveated depth texture keeps the driver from foveating), as Meta's GLES samples
// make it: a framebuffer per swapchain image, made once with the image attached for good (never attached and detached
// each frame), and the depth at the image's own size. qfov's form 2 had one framebuffer whose colour was swapped every
// eye and a depth at the drawn size (0.65 of the image): with foveated images the runtime crashed in the driver at the
// first xrEndFrame (SIGSEGV 0x30). Each image also has a colour-only framebuffer, the UI's (as the shared one's).
struct DirectImage
{
    GLuint image{0}; // the swapchain's GL texture (0: the slot is free)
    int eye{0};
    GLuint fbo{0};   // the image and its eye's depth: the scene's
    GLuint uiFbo{0}; // the image alone: the UI's, the eyeshots'
};
struct DirectDepth
{
    GLuint depth{0}; // shared by the eye's images (drawn one after another; invalidated after the scene)
    int width{0};
    int height{0};
};
constexpr int directImageSlots = 8; // (the Quest's swapchains have 3 images an eye)
struct DirectTargets
{
    za::Array<DirectImage, directImageSlots> images{};
    za::Array<DirectDepth, 2> depths{};
    unsigned serial{0}; // the backend's eyeImagesSerial they were made for
    bool made{false};
};
DirectTargets directTargets;

void deleteDirectTargets()
{
    using DeleteFn = void(APIENTRY*)(GLsizei, const GLuint*);
    for(DirectImage& d : directTargets.images)
    {
        if(d.fbo)
        {
            GL_DeleteFramebuffersFunc(1, &d.fbo);
            GL_DeleteFramebuffersFunc(1, &d.uiFbo);
        }
        d = DirectImage{};
    }
    for(DirectDepth& d : directTargets.depths)
    {
        if(d.depth)
        {
            reinterpret_cast<DeleteFn>(SDL_GL_GetProcAddress("glDeleteRenderbuffers"))(1, &d.depth);
        }
        d = DirectDepth{};
    }
    directTargets.made = false;
}

// The eye image's own framebuffers (made the first time the image is acquired), or null when there is no free slot.
const DirectImage* directImage(int eye, GLuint image, int width, int height, unsigned serial)
{
    using GenFn = void(APIENTRY*)(GLsizei, GLuint*);
    using BindFn = void(APIENTRY*)(GLenum, GLuint);
    using StorageFn = void(APIENTRY*)(GLenum, GLenum, GLsizei, GLsizei);
    using AttachFn = void(APIENTRY*)(GLenum, GLenum, GLenum, GLuint);
    DirectDepth& depth = directTargets.depths[eye];
    if(directTargets.made && (directTargets.serial != serial || (depth.depth && (depth.width != width || depth.height != height))))
    {
        deleteDirectTargets(); // (new swapchains, or the images' size: everything made again)
    }
    directTargets.made = true;
    directTargets.serial = serial;
    DirectImage* free = nullptr;
    for(DirectImage& d : directTargets.images)
    {
        if(d.image == image && d.eye == eye)
        {
            return &d;
        }
        free = !free && !d.image ? &d : free;
    }
    if(!free)
    {
        return nullptr;
    }
    // (the log's GL error: this set-up's own; one the drawing since the last glGetError left pending, printed apart:
    // beta3check's 0x502 came from the eye drawn before, most likely the in-tile pass: vr_tonemap.cpp drawInTile)
    const GLenum pending = glGetError();
    for(int i = 0; i < 16 && glGetError() != GL_NO_ERROR; i++)
    {
    }
    if(!depth.depth)
    {
        depth.width = width;
        depth.height = height;
        reinterpret_cast<GenFn>(SDL_GL_GetProcAddress("glGenRenderbuffers"))(1, &depth.depth);
        reinterpret_cast<BindFn>(SDL_GL_GetProcAddress("glBindRenderbuffer"))(GL_RENDERBUFFER, depth.depth);
        reinterpret_cast<StorageFn>(SDL_GL_GetProcAddress("glRenderbufferStorage"))(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
        reinterpret_cast<BindFn>(SDL_GL_GetProcAddress("glBindRenderbuffer"))(GL_RENDERBUFFER, 0);
    }
    DirectImage& d = *free;
    d.image = image;
    d.eye = eye;
    GL_GenFramebuffersFunc(1, &d.fbo);
    GL_GenFramebuffersFunc(1, &d.uiFbo);
    GL_BindFramebufferFunc(GL_FRAMEBUFFER, d.uiFbo);
    GL_FramebufferTexture2DFunc(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, image, 0);
    GL_BindFramebufferFunc(GL_FRAMEBUFFER, d.fbo);
    GL_FramebufferTexture2DFunc(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, image, 0);
    reinterpret_cast<AttachFn>(SDL_GL_GetProcAddress("glFramebufferRenderbuffer"))(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
        GL_RENDERBUFFER, depth.depth);
    // What the driver has on the image (the runtime's XR_FB_foveation sets its QCOM_texture_foveated bits): once an image
    GLint bits = -1, points = 0;
    GLfloat minDensity = -1.f;
    if(SDL_GL_ExtensionSupported("GL_QCOM_texture_foveated"))
    {
        GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D, image);
        glGetTexParameteriv(GL_TEXTURE_2D, 0x8BFB, &bits);       // GL_TEXTURE_FOVEATED_FEATURE_BITS_QCOM
        glGetTexParameterfv(GL_TEXTURE_2D, 0x8BFC, &minDensity); // GL_TEXTURE_FOVEATED_MIN_PIXEL_DENSITY_QCOM
        glGetTexParameteriv(GL_TEXTURE_2D, 0x8BFE, &points);     // GL_TEXTURE_FOVEATED_NUM_FOCAL_POINTS_QUERY_QCOM
        GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D, 0);
    }
    const GLenum status = GL_CheckFramebufferStatusFunc(GL_FRAMEBUFFER);
    Con_Printf("VR: eye %d image %u: its own framebuffer %u (%d x %d, depth renderbuffer %u): status 0x%x, foveation bits %d, "
               "min density %g, focal points %d, GL error 0x%x (pending before 0x%x)\n", eye, image, d.fbo, width, height,
        depth.depth, status, bits, static_cast<double>(minDensity), points, glGetError(), pending);
    return &d;
}

// vr_render_scale: the eyes are rendered at the scaled size, post-processed into this texture of
// that size, and resampled (upscale::resample) into the eye image, which keeps the runtime's size.
// At scale 1 too with vr_upscale_sharpen_native (sharpened into the image).
gfx::RenderTarget resample;
bool resampling = false; // this frame's eyes are

bool renderingEye = false;
int directForm = 0; // this eye drawn straight into its image (directEye): 0 no, 1 the composite's depth, 2 its own
int directNoted = 0; // the form the console last said the eyes are drawn with (-1 wanted but not possible)
int partNoted = -1;  // the part's place the console last said (vr_eye_rect_place: x * 65536 + y; -1 none said)
bool spectatorView = false; // rendering the window's spectator camera (as an eye)
int currentEye = 0;
bool firstEye = false; // no other eye rendered before it this frame

void ensureResampleTarget(int width, int height)
{
    if(!resample.ensure(width, height,
           gfx::RenderTargetFormat{.internalFormat = GL_RGBA8, .clampToEdge = false, .maxLevel0 = true, .fboName = "vr resample"},
           "stereo resample target"))
    {
        Con_Warning("VR: the stereo resample target's framebuffer is incomplete (%d x %d)\n", width, height);
    }
}

// The spectator camera's MSAA (vr_spectator_aa): the window's (vid_fsaa), or none.
[[nodiscard]] float spectatorFsaa()
{
    return vr_spectator_aa.value > 0.f ? vid_fsaa.value : 0.f;
}

// The eyes' depth/stencil format: 32-bit float with reversed Z (clip control), for the small near plane's precision
// far away (vr_nearclip); else the window's 24-bit fixed point.
[[nodiscard]] unsigned sceneDepthFormat()
{
    return gl_clipcontrol_able && vr_depth_float.value != 0.f ? GL_DEPTH32F_STENCIL8 : GL_DEPTH24_STENCIL8;
}

// vr_gl_foveation: GL_QCOM_texture_foveated (the Quest's Adreno) on the eyes' scene colours: the driver renders the
// tiles away from each eye's centre at a lower resolution (fixed foveated rendering into our own targets; the runtime's
// XR_FB_foveation, vr_xr_foveation, foveates only its swapchain, which takes just the post-process). QUEST_PLAN.md 6o.
constexpr GLint foveationEnable = 0x1 | 0x2;   // GL_FOVEATION_ENABLE_BIT_QCOM | GL_FOVEATION_SCALED_BIN_METHOD_BIT_QCOM
using FoveationParamsFn = void(APIENTRY*)(GLuint texture, GLuint layer, GLuint focalPoint, GLfloat focalX, GLfloat focalY,
    GLfloat gainX, GLfloat gainY, GLfloat foveaArea);
FoveationParamsFn foveationParams = nullptr;
bool foveationLooked = false; // the extension looked up (once a context: the eyes' targets live with it)
GLint foveationPoints = 1;    // the focal points the driver gives a foveated texture (each one set: an unset one's
                              // density may be full everywhere, the densest point winning)
unsigned foveationRefused = 0; // the vr_gl_foveation_parts the driver refused (an incomplete framebuffer), a bit each

// vr_gl_foveation_parts: which of the eyes' attachments are foveated (1 the scene colours, 2 their depth/stencil, 4 the
// OIT targets); the colours always.
[[nodiscard]] int foveationParts()
{
    return (za::clamp(static_cast<int>(vr_gl_foveation_parts.value), 0, 7) | 1);
}

[[nodiscard]] int foveationLevel()
{
    if(!foveationLooked)
    {
        foveationLooked = true;
        if(SDL_GL_ExtensionSupported("GL_QCOM_texture_foveated"))
        {
            foveationParams = reinterpret_cast<FoveationParamsFn>(SDL_GL_GetProcAddress("glTextureFoveationParametersQCOM"));
        }
    }
    const bool refused = (foveationRefused & (1u << foveationParts())) != 0;
    return foveationParams && !refused ? za::clamp(static_cast<int>(vr_gl_foveation.value), 0, 3) : 0;
}

// The eye's foveated attachments (t.foveatedParts), for their parameters.
[[nodiscard]] za::Array<GLuint, 6> foveatedTextures(const SceneTargets& t)
{
    const int p = t.foveatedParts;
    return {t.fb.composite.color_tex, t.fb.scene.color_tex, (p & 2) ? t.fb.composite.depth_stencil_tex : 0u,
        (p & 2) ? t.fb.scene.depth_stencil_tex : 0u, (p & 4) ? t.fb.oit.accum_tex : 0u, (p & 4) ? t.fb.oit.revealage_tex : 0u};
}

// Whether `t` is already made for this size, MSAA and format (and foveation: its bits can't be taken off a texture).
[[nodiscard]] bool sceneTargetsFit(const SceneTargets& t, int allocatedWidth, int allocatedHeight, float fsaa, bool foveate = false)
{
    return t.allocatedWidth == allocatedWidth && t.allocatedHeight == allocatedHeight && t.fsaa == fsaa &&
           t.format == tonemap::sceneFormat() && t.depth == sceneDepthFormat() &&
           t.foveated == (foveate && fsaa < 2.f && foveationLevel() > 0) &&
           (!t.foveated || t.foveatedParts == foveationParts());
}

// `t` made for allocatedWidth x allocatedHeight (kept when it already is).
void ensureSceneTargets(SceneTargets& t, int allocatedWidth, int allocatedHeight, float fsaa, bool foveate = false)
{
    const unsigned format = tonemap::sceneFormat();
    if(sceneTargetsFit(t, allocatedWidth, allocatedHeight, fsaa, foveate))
    {
        return;
    }
    t.fsaa = fsaa;
    t.format = format;
    t.depth = sceneDepthFormat();
    t.foveated = foveate && fsaa < 2.f && foveationLevel() > 0;
    t.foveatedParts = t.foveated ? foveationParts() : 0;

    const glframebufs_t windowFramebufs = framebufs;
    const int windowWidth = vid.width;
    const int windowHeight = vid.height;

    if(t.allocatedWidth)
    {
        framebufs = t.fb;
        GL_DeleteFrameBuffers();
    }
    else
    {
        // A set's first making: from nothing, as after GL_DeleteFrameBuffers (which zeroes it). GL_CreateFrameBuffers
        // leaves oit.fbo_composite alone with MSAA, so the set would keep the current set's and delete it with its own.
        framebufs = glframebufs_t{};
    }

    vid.width = allocatedWidth; // (GL_CreateFBOAttachment's size)
    vid.height = allocatedHeight;
    creatingSceneTargets = true;
    creatingFormat = format;
    creatingDepth = t.depth;
    creatingFsaa = fsaa;
    creatingFoveated = t.foveated;
    creatingFoveatedParts = t.foveatedParts;
    foveationIncomplete = 0;
    GL_CreateFrameBuffers();
    creatingSceneTargets = false;
    creatingFoveated = false;
    t.fb = framebufs;

    framebufs = windowFramebufs;
    vid.width = windowWidth;
    vid.height = windowHeight;

    t.allocatedWidth = allocatedWidth;
    t.allocatedHeight = allocatedHeight;
    t.drawnWidth = t.drawnHeight = 0;
    ++t.generation;

    if(t.foveated)
    {
        // (the scene's colours, drawn into without MSAA: t.foveated asks for none; their bits set before their storage
        // by VR_SceneAttachmentFoveation, never cleared: the targets are made anew when it is turned off)
        GL_BindFramebufferFunc(GL_FRAMEBUFFER, t.fb.composite.fbo);
        const GLenum status = foveationIncomplete ? foveationIncomplete : GL_CheckFramebufferStatusFunc(GL_FRAMEBUFFER);
        GL_BindFramebufferFunc(GL_FRAMEBUFFER, 0);
        foveationIncomplete = 0;
        if(status != GL_FRAMEBUFFER_COMPLETE)
        {
            Con_Printf("VR: the eyes' foveated scene framebuffer is incomplete (0x%x, parts %d): no foveation\n", status,
                t.foveatedParts);
            foveationRefused |= 1u << t.foveatedParts; // (made anew without it next frame)
        }
        else
        {
            // (C4o: what the driver kept: the bits set, the features it offers, its focal points)
            GLint bits = 0, offered = 0, points = 0;
            GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D, t.fb.composite.color_tex);
            glGetTexParameteriv(GL_TEXTURE_2D, 0x8BFB, &bits);    // GL_TEXTURE_FOVEATED_FEATURE_BITS_QCOM
            glGetTexParameteriv(GL_TEXTURE_2D, 0x8BFD, &offered); // GL_TEXTURE_FOVEATED_FEATURE_QUERY_QCOM
            glGetTexParameteriv(GL_TEXTURE_2D, 0x8BFE, &points);  // GL_TEXTURE_FOVEATED_NUM_FOCAL_POINTS_QUERY_QCOM
            GLfloat minDensity = -1.f;
            glGetTexParameterfv(GL_TEXTURE_2D, 0x8BFC, &minDensity); // GL_TEXTURE_FOVEATED_MIN_PIXEL_DENSITY_QCOM
            GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D, 0);
            Con_Printf("VR: the eyes' scene foveated (%d x %d, parts %d, format 0x%x): bits %d, offered %d, focal points %d, "
                       "min density %.3f, error 0x%x\n",
                allocatedWidth, allocatedHeight, t.foveatedParts, format, bits, offered, points, minDensity, glGetError());
            foveationPoints = za::clamp(points, 1, 4);
        }
    }

    if(!targetFbo)
    {
        GL_GenFramebuffersFunc(1, &targetFbo);
        GL_LabelNewFramebuffer(targetFbo, "vr eye target");
    }
}

// A foveated texture's focal points on the eye's projection centre (off the image's centre in each eye). Drawn into the
// texture's bottom left sx x sy part (dynamic resolution, vr_eye_subimage): the focal point and the fall-off in that
// part's -1..1, mapped into the whole texture's (the point scaled towards the bottom left corner, the gain by the
// inverse: the same fall-off over the part drawn).
void focusTexture(GLuint texture, GLint points, const Fov& fov, float sx, float sy, float gain, float area)
{
    const float l = za::tan(fov.left), r = za::tan(fov.right), u = za::tan(fov.up), d = za::tan(fov.down);
    const float focalX = -1.f + (1.f - (r + l) / (r - l)) * sx;
    const float focalY = -1.f + (1.f - (u + d) / (u - d)) * sy;
    for(GLint point = 0; texture && point < points; point++)
    {
        foveationParams(texture, 0, static_cast<GLuint>(point), focalX, focalY, gain / sx, gain / sy, area);
    }
}

// The eye's foveation (vr_gl_foveation), before its scene: full resolution round the point the eye looks straight at
// (its projection's centre, off the image's centre in each eye), less and less outside. The driver's pixel density at
// a point d away (in the image's -1..1) is 1 / (gain^2 d^2 - area): full within sqrt(1 + area) / gain of it.
// (C4n: stronger than C4h's 1/1.5/2, which showed no change: the resolution's fall-off rate and the full-resolution
// area around the focal point, in the QCOM extension's units)
constexpr float foveationGains[] = {0.f, 2.f, 4.f, 6.f};
constexpr float foveationAreas[] = {0.f, 0.5f, 0.25f, 0.1f};

// vr_gl_foveation's zones on the eye's scene now (vr_foveated_debug; none while it is off): the focal point on the
// projection's centre in the drawn part's -1..1, as focusTexture sets it.
[[nodiscard]] foveated::DensityZones glFoveationZones(const Fov& fov)
{
    foveated::DensityZones z;
    if(!eyeTargets.foveated || foveationLevel() == 0)
    {
        return z;
    }
    const float l = za::tan(fov.left), r = za::tan(fov.right), u = za::tan(fov.up), d = za::tan(fov.down);
    z.focalX = -(r + l) / (r - l);
    z.focalY = -(u + d) / (u - d);
    z.gain = foveationGains[foveationLevel()];
    z.area = foveationAreas[foveationLevel()];
    return z;
}

void foveateEye(const Fov& fov)
{
    if(!eyeTargets.foveated || foveationLevel() == 0) // (none: the targets made again without it, an incomplete framebuffer)
    {
        return;
    }
    const int level = foveationLevel();
    const SceneTargets& t = eyeTargets;
    const float sx = t.allocatedWidth > 0 && t.drawnWidth > 0 ? static_cast<float>(t.drawnWidth) / t.allocatedWidth : 1.f;
    const float sy = t.allocatedHeight > 0 && t.drawnHeight > 0 ? static_cast<float>(t.drawnHeight) / t.allocatedHeight : 1.f;
    for(const GLuint texture : foveatedTextures(t))
    {
        focusTexture(texture, foveationPoints, fov, sx, sy, foveationGains[level], foveationAreas[level]);
    }
}

// Frees a set (releaseTargets): made from nothing at its next ensureSceneTargets.
void deleteSceneTargets(SceneTargets& t)
{
    if(!t.allocatedWidth)
    {
        return;
    }
    const glframebufs_t windowFramebufs = framebufs;
    framebufs = t.fb;
    GL_DeleteFrameBuffers();
    framebufs = windowFramebufs;
    const unsigned generation = t.generation;
    t = SceneTargets{};
    t.generation = generation + 1; // (single-pass stereo's arrays follow)
}

// The setting -1 (auto): under the ES profile (the Quest's tiled GPU), where it pays.
[[nodiscard]] bool autoOn(float setting)
{
    return setting > 0.f || (setting < 0.f && (VR_GLESCompat() || VR_Android()));
}

// vr_eye_targets_split: the right eye drawn into its own targets, so that its clear never touches what the left eye's
// post-process (and mirror) still read. On a tiler (the Quest 3's Adreno) the clear of targets still in use waited for
// or copied them: ~10 ms of the right eye's "scene setup" a frame (QUEST_PLAN.md 6o). Not with single-pass stereo (its
// own layered targets) nor TAA (its history follows one set of targets).
[[nodiscard]] bool splitEyeTargets(int samples)
{
    return autoOn(vr_eye_targets_split.value) && !stereosingle::wanted(samples) && !temporal::wanted();
}

// vr_gl_invalidate: what a tiler need not write out or read in (glInvalidateFramebuffer).
[[nodiscard]] bool invalidating()
{
    return autoOn(vr_gl_invalidate.value);
}

// The eye's depth and stencil after its scene: nothing reads them after it (the post-process, the UI and the mirror
// read colours), so a tiler need not write them out to memory. Both scene framebuffers (the MSAA one and the composite
// the scene resolves into, or draws into without MSAA).
void invalidateSceneDepth()
{
    if(!invalidating())
    {
        return;
    }
    constexpr GLenum depthStencil[] = {GL_DEPTH_ATTACHMENT, GL_STENCIL_ATTACHMENT};
    const GLuint bound = GL_BoundDrawFramebuffer();
    if(framebufs.scene.samples > 1) // (else the scene drew into the composite: its MSAA framebuffer unused)
    {
        GL_BindFramebufferFunc(GL_DRAW_FRAMEBUFFER, framebufs.scene.fbo);
        GL_InvalidateFramebufferFunc(GL_DRAW_FRAMEBUFFER, 2, depthStencil);
    }
    GL_BindFramebufferFunc(GL_DRAW_FRAMEBUFFER, framebufs.composite.fbo);
    GL_InvalidateFramebufferFunc(GL_DRAW_FRAMEBUFFER, 2, depthStencil);
    GL_BindFramebufferFunc(GL_DRAW_FRAMEBUFFER, bound);
}

// The view about to be drawn into `t` (an eye, the spectator camera): framebufs its targets; vid.width/vid.height their
// allocated size (the engine reads its framebuffers' size there: R_WarpScaleView's smax/tmax); glx/gly 0 and
// glwidth/glheight and r_refdef.vrect the bottom left drawnWidth x drawnHeight of them, the part drawn (R_SetupGL's and
// GL_PostProcess's viewports). The passes read both through viewSize(), never vid.width for the drawn size.
void useView(SceneTargets& t, int drawnWidth, int drawnHeight)
{
    framebufs = t.fb;
    vid.width = t.allocatedWidth;
    vid.height = t.allocatedHeight;
    glx = gly = 0;
    glwidth = drawnWidth;
    glheight = drawnHeight;
    r_refdef.vrect.x = r_refdef.vrect.y = 0;
    r_refdef.vrect.width = drawnWidth;
    r_refdef.vrect.height = drawnHeight;
    t.drawnWidth = drawnWidth;
    t.drawnHeight = drawnHeight;
}

void deletePortalTargets(PortalTargets& p)
{
    deleteSceneTargets(p.scene);
    for(int d = 0; d < 4; d++)
    {
        if(p.array[d])
        {
            GL_DeleteNativeTexture(p.array[d]);
        }
        p.array[d] = 0;
        p.layers[d] = 0;
    }
    if(p.fbo)
    {
        GL_DeleteFramebuffersFunc(1, &p.fbo);
        p.fbo = 0;
    }
}

// The window's view (vr_window.cpp): a scene's colours (the eye's before its post-processing, which writes into the
// headset's image, or the spectator camera's) with its glow added, the tone curve and the grade as the
// post-processing does them (vr_bloom.cpp, vr_tonemap.cpp), so the window shows what the headset does (the window's
// own post-processing then applies the desktop's gamma and contrast). Each window pixel reads the scene through Map:
// the window's rectangle (-1..1) to the scene's uv, homogeneous (a crop, or the smoothed mirror's turn, a
// homography). An eye as it is (Params.x 0) reads the nearest texel as it always has; the smoothed mirror and
// the spectator camera read it filtered (Catmull-Rom: sharp at any sub-pixel offset, so a slowly turning view does not
// pulse between sharp and soft as a bilinear read would; bilinear from a spectator camera larger than the window),
// black outside the scene, with the eyes' underwater wobble and blur (gl_shaders.h's post-process).
GLuint mirrorProgram = 0;
bool mirrorFailed = false;
GLuint linearSampler = 0; // unit 0, while the window's view reads a scene filtered

constexpr const char* mirrorVs = gfx::fullscreenTriangleVs;

constexpr const char* mirrorFs = R"(#version 430
layout(binding = 0) uniform sampler2D Scene;
layout(binding = 1) uniform sampler2D Bloom;
layout(binding = 3) uniform sampler3D GradeLUT;
layout(location = 1) uniform vec4 Dest;   // the window's rectangle: x0, y0, 1 / width, 1 / height
layout(location = 2) uniform float BloomStrength;
layout(location = 3) uniform vec4 Tone;   // as the post-process's (vr_tonemap.hpp: tonemap::bind)
layout(location = 4) uniform vec4 Params; // x: 0 nearest (an eye as it is), 1 Catmull-Rom, 2 bilinear
layout(location = 5) uniform vec4 WaterParams; // the post-process's: time, wobble, blur (both 0: not under water)
layout(location = 6) uniform vec4 WaterProj;   // ndc x = x + y * left / forward, ndc y = z + w * up / forward
layout(location = 7) uniform vec3 WaterFwd;    // the scene's view axes in the world
layout(location = 8) uniform vec3 WaterLeft;
layout(location = 9) uniform vec3 WaterUp;
layout(location = 10) uniform vec3 Map0;  // Map's columns: the window (-1..1, 1) to the scene's uv (homogeneous)
layout(location = 11) uniform vec3 Map1;
layout(location = 12) uniform vec3 Map2;
layout(location = 13) uniform vec4 SlowLook; // bullet time: strength, desaturation, vignette, unused
layout(location = 14) uniform vec3 SlowTint;
layout(location = 15) uniform vec2 SceneSize; // the scene's drawn part's size (its texture can be larger: dynamic resolution's ceiling)
layout(location = 16) uniform vec4 SceneMap;  // QvrViewUV's map of it: uv over the drawn part to the texture's
layout(location = 17) uniform vec4 BloomMap;  // ... of the glow
layout(location = 0) out vec4 Out;
)" QVR_TONE_GLSL QVR_VIEW_UV_GLSL R"(
vec3 SceneTap(vec2 uv) { return texture(Scene, QvrViewUV(uv, SceneMap)).rgb; } // (uv over the drawn part)

// Catmull-Rom in nine bilinear reads (the middle two texels of each axis in one).
vec3 CatmullRom(vec2 uv)
{
    vec2 size = SceneSize;
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
    vec3 c = (SceneTap(vec2(t0.x, t0.y)) * w0.x + SceneTap(vec2(t12.x, t0.y)) * w12.x +
              SceneTap(vec2(t3.x, t0.y)) * w3.x) * w0.y +
             (SceneTap(vec2(t0.x, t12.y)) * w0.x + SceneTap(vec2(t12.x, t12.y)) * w12.x +
              SceneTap(vec2(t3.x, t12.y)) * w3.x) * w12.y +
             (SceneTap(vec2(t0.x, t3.y)) * w0.x + SceneTap(vec2(t12.x, t3.y)) * w12.x +
              SceneTap(vec2(t3.x, t3.y)) * w3.x) * w3.y;
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
        c = SceneTap(uv);
    }
    else if(h.z <= 0.0 || any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0))))
    {
        c = vec3(0.0);
    }
    else if(WaterParams.y + WaterParams.z > 0.0)
    {
        // Under water: the post-process's wobble of the direction looked in, in the world, and its blur.
        vec2 size = SceneSize;
        vec2 sndc = uv * 2.0 - 1.0;
        vec3 dir = normalize(WaterFwd + WaterLeft * ((sndc.x - WaterProj.x) / WaterProj.y) +
                             WaterUp * ((sndc.y - WaterProj.z) / WaterProj.w));
        float t = WaterParams.x;
        vec2 w = vec2(sin(dot(dir, vec3(6.1, 2.3, 4.7)) + t * 1.1) + 0.5 * sin(dot(dir, vec3(-3.7, 8.3, 2.9)) - t * 1.6),
                      sin(dot(dir, vec3(2.9, -5.9, 6.3)) + t * 0.9) + 0.5 * sin(dot(dir, vec3(7.7, 1.9, -4.9)) + t * 1.4));
        vec2 aspect = vec2(size.y / size.x, 1.0);
        uv = clamp(uv + w * (WaterParams.y * aspect), vec2(0.0), vec2(1.0));
        vec2 b = max(WaterParams.z, 0.5 / size.y) * aspect;
        c = (SceneTap(uv + b) + SceneTap(uv - b) + SceneTap(uv + vec2(b.x, -b.y)) + SceneTap(uv + vec2(-b.x, b.y))) * 0.25;
    }
    else if(Params.x == 1.0)
    {
        c = CatmullRom(uv);
    }
    else
    {
        c = SceneTap(uv);
    }
    if(BloomStrength > 0.0) // by one bilinear tap, the smoothing done at the quarter (vr_bloom.cpp)
    {
        c += texture(Bloom, QvrViewUV(uv, BloomMap)).rgb * BloomStrength;
    }
    if(Tone.x > 0.0)
        c = QvrTonemap(c * Tone.x, Tone.yz);
    if(SlowLook.x > 0.0)
    {
        // The headset's look (vr_glsl.h), centred on this camera's rectangle, regardless of its resolution or FOV.
        float luma = dot(c, vec3(0.299, 0.587, 0.114));
        c = mix(c, vec3(luma), SlowLook.x * SlowLook.y);
        c *= mix(vec3(1.0), SlowTint, SlowLook.x);
        c *= 1.0 - SlowLook.x * SlowLook.z * smoothstep(0.35, 1.7, dot(ndc, ndc));
    }
    if(Tone.w > 0.0)
        c = QvrGrade(GradeLUT, c, Tone.w);
    Out = vec4(c, 1.0);
}
)";

enum class Sampling
{
    Nearest,    // an eye as it is
    CatmullRom, // the smoothed mirror, the spectator camera at the window's size or below
    Bilinear,   // the spectator camera above the window's size
};

// What the window's view adds to a scene's colours from the view it was rendered from: its glow (0: none) and its
// view axes (the underwater wobble's). Taken right after that view is rendered (sceneLook), as the next view rendered
// replaces both: the spectator camera keeps its own for the frames it shows its last image again (NOTES.md
// vrfiringrange_2026-10-02_01-32-14: with the eyes' glow there instead, the hands and the HUD's text showed through
// faintly, offset, turning with the head).
struct SceneLook
{
    GLuint glow = 0;
    glm::vec4 glowView{1.f}; // its drawn part's size, its texture's (bloom::result)
    glm::vec4 proj{0.f};
    glm::vec3 forward{0.f}, left{0.f}, up{0.f};
};

[[nodiscard]] SceneLook sceneLook()
{
    SceneLook l;
    unsigned glow = 0;
    l.glow = bloom::result(glow, l.glowView) ? glow : 0;
    l.proj = {r_matproj[0], r_matproj[4], r_matproj[1], r_matproj[9]};
    l.forward = {vpn[0], vpn[1], vpn[2]};
    l.left = {-vright[0], -vright[1], -vright[2]};
    l.up = {vup[0], vup[1], vup[2]};
    return l;
}

// Draws the scene `source` (its colours, width x height) into the window's rectangle dx0..dx1 by windowHeight through
// `map`, with its view's glow and underwater wobble (`look`).
void drawToWindow(const SceneTargets& source, const SceneLook& look, const glm::mat3& map, Sampling sampling,
    GLuint windowTarget, int dx0, int dx1, int windowHeight)
{
    if(!mirrorProgram && !mirrorFailed)
    {
        mirrorProgram = gfx::glProgram(mirrorVs, mirrorFs, "vr mirror");
        mirrorFailed = !mirrorProgram;
        gfx::ownDeferred(mirrorProgram, &mirrorFailed);
    }
    if(mirrorFailed)
    {
        // The crop Map gives the corners of (a turn is not a blit: then the eye as it is).
        const glm::vec3 a = map * glm::vec3{-1.f, -1.f, 1.f}, b = map * glm::vec3{1.f, 1.f, 1.f};
        GL_BindFramebufferFunc(GL_READ_FRAMEBUFFER, source.fb.composite.fbo);
        GL_BindFramebufferFunc(GL_DRAW_FRAMEBUFFER, windowTarget);
        GL_BlitFramebufferFunc(static_cast<int>(a.x / a.z * source.drawnWidth), static_cast<int>(a.y / a.z * source.drawnHeight),
            static_cast<int>(b.x / b.z * source.drawnWidth), static_cast<int>(b.y / b.z * source.drawnHeight), dx0, 0, dx1,
            windowHeight, GL_COLOR_BUFFER_BIT, GL_LINEAR);
        GL_BindFramebufferFunc(GL_FRAMEBUFFER, windowTarget);
        return;
    }

    const bool hasGlow = look.glow != 0;
    GL_BindFramebufferFunc(GL_FRAMEBUFFER, windowTarget);
    glViewport(dx0, 0, dx1 - dx0, windowHeight);
    GL_SetState(GLS_BLEND_OPAQUE | GLS_NO_ZTEST | GLS_NO_ZWRITE | GLS_CULL_NONE | GLS_ATTRIBS(0));
    GL_UseProgram(mirrorProgram);
    GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D, source.fb.composite.color_tex);
    GL_BindNative(GL_TEXTURE1, GL_TEXTURE_2D, look.glow);
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
    GL_Uniform4fFunc(6, look.proj.x, look.proj.y, look.proj.z, look.proj.w);
    GL_Uniform3fFunc(7, look.forward.x, look.forward.y, look.forward.z);
    GL_Uniform3fFunc(8, look.left.x, look.left.y, look.left.z);
    GL_Uniform3fFunc(9, look.up.x, look.up.y, look.up.z);
    GL_Uniform3fFunc(10, map[0].x, map[0].y, map[0].z);
    GL_Uniform3fFunc(11, map[1].x, map[1].y, map[1].z);
    GL_Uniform3fFunc(12, map[2].x, map[2].y, map[2].z);
    // Only the actual spectator scene: toggling this setting never changes either mirror mode or the headset.
    // This also follows the live fade while the spectator repeats a retained scene between camera frames.
    const bullettime::Look slow = &source == &spectatorTargets && vr_spectator_bullettime_fx.value != 0.f ?
        bullettime::look() : bullettime::Look{};
    GL_Uniform4fFunc(13, slow.strength, slow.desaturate, slow.vignette, 0.f);
    GL_Uniform3fFunc(14, slow.tint.x, slow.tint.y, slow.tint.z);
    GL_Uniform2fFunc(15, static_cast<float>(source.drawnWidth), static_cast<float>(source.drawnHeight));
    const glm::vec4 sceneMap = viewUvMap(source.drawnWidth, source.drawnHeight, source.allocatedWidth, source.allocatedHeight);
    GL_Uniform4fFunc(16, sceneMap.x, sceneMap.y, sceneMap.z, sceneMap.w);
    const glm::vec4 glowMap = viewUvMap(static_cast<int>(look.glowView.x), static_cast<int>(look.glowView.y),
        static_cast<int>(look.glowView.z), static_cast<int>(look.glowView.w));
    GL_Uniform4fFunc(17, glowMap.x, glowMap.y, glowMap.z, glowMap.w);
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

// The window's view of the eye just rendered: the shown eye (or each, side by side: vr_window_view) cropped to the
// window's aspect ratio, or the smoothed mirror of the shown eye.
void mirrorToWindow(int eye, GLuint windowTarget, int windowWidth, int windowHeight)
{
    // The eye's own targets (framebufs: with single-pass stereo, its layer's views; else the eyes' targets).
    SceneTargets source = eyeTargets;
    source.fb = framebufs;
    const window::View view = window::view();
    const int shown = window::eye();
    if(view == window::View::Smoothed)
    {
        if(eye != shown)
        {
            return;
        }
        QVR_GPU_PROFILE("mirror");
        Backend* be = backend();
        const HiddenArea* hidden = be && vr_visibility_mask.value != 0.f ? be->hiddenArea(eye) : nullptr;
        if(hidden && hidden->indices.empty())
        {
            hidden = nullptr;
        }
        const glm::mat3 map = window::mirrorMap(frameState().eyes[eye].fov,
            static_cast<float>(windowWidth) / static_cast<float>(windowHeight), hidden);
        drawToWindow(source, sceneLook(), map, Sampling::CatmullRom, windowTarget, 0, windowWidth, windowHeight);
        return;
    }

    if(view != window::View::Raw || (shown >= 0 && eye != shown))
    {
        return;
    }
    QVR_GPU_PROFILE("mirror");

    int dx0 = 0, dx1 = windowWidth;
    if(shown < 0)
    {
        dx0 = eye * windowWidth / 2;
        dx1 = dx0 + windowWidth / 2;
    }

    // The eye's drawn part (its targets may be larger: dynamic resolution's ceiling).
    const int drawnWidth = eyeTargets.drawnWidth, drawnHeight = eyeTargets.drawnHeight;
    const float eyeAspect = static_cast<float>(drawnWidth) / drawnHeight;
    const float windowAspect = static_cast<float>(dx1 - dx0) / windowHeight;

    int x0 = 0, y0 = 0, x1 = drawnWidth, y1 = drawnHeight;
    if(windowAspect > eyeAspect)
    {
        const int h = static_cast<int>(drawnWidth / windowAspect);
        y0 = (drawnHeight - h) / 2;
        y1 = y0 + h;
    }
    else
    {
        const int w = static_cast<int>(drawnHeight * windowAspect);
        x0 = (drawnWidth - w) / 2;
        x1 = x0 + w;
    }
    const float ew = static_cast<float>(drawnWidth);
    const float eh = static_cast<float>(drawnHeight);
    drawToWindow(source, sceneLook(), cropMap(x0 / ew, y0 / eh, (x1 - x0) / ew, (y1 - y0) / eh), Sampling::Nearest, windowTarget,
        dx0, dx1, windowHeight);
}

// The UI in an eye, into `fbo` (width x height): the lasers, the HUD panel or the menu (with its pointer), the wrist
// gadget's log. Not depth tested: over whatever the fbo holds. Without `headText`, not the head-locked text (centre
// prints, notify lines): the spectator camera's and the mirror's choice (vr_spectator_hide_hud_text,
// vr_mirror_hide_hud_text), for recording. `headset`: the headset's own eye (not the mirror's copy nor the spectator
// camera): the takes' recording light and rating prompt (vr_takes.cpp), never in what OBS records.
void drawUi(const glm::vec3& viewOrigin, GLuint fbo, int width, int height, bool headText = true, bool headset = false,
    int x = 0, int y = 0)
{
    QVR_GPU_PROFILE("ui");
    GL_BindFramebufferFunc(GL_FRAMEBUFFER, fbo);
    glViewport(x, y, width, height); // (x, y: the eye's part in its image, vr_eye_rect_place)
    lines::drawInEye(viewOrigin);
    panel::drawInEye(hands::current(), headText);
    text3d::drawOverlay();
    if(headset)
    {
        takes::drawInEye(hands::current());
    }
}

// Whether the window mirrors this eye (mirrorToWindow).
[[nodiscard]] bool mirrored(int eye)
{
    switch(window::view())
    {
    case window::View::Raw:
        return window::eye() < 0 || window::eye() == eye;
    case window::View::Smoothed:
        return window::eye() == eye;
    default:
        return false;
    }
}

// The lenses' hidden area (vr_visibility_mask): the runtime's hidden triangles, from the eye's
// tangent space to clip space by the eye's field of view, drawn black at the near plane (depth
// 1 with Ironwail's reversed Z, 0 without) right after the scene's clear: every later depth-tested
// fragment there fails early, so the world's, models' and particles' shaders skip those pixels,
// and bloom and the mirror see black.
// Each eye's triangles are kept in a vertex buffer, made again only when what they are made from changes: the
// runtime's mesh (a visibility-mask event gives a new one), the eye's field of view, the depth convention. Not at every
// scene clear (each eye, each view through a teleporter, each layer of single-pass stereo).
struct HiddenMesh
{
    gfx::StaticTriangles triangles;
    // What they were made from (made: false, nothing yet).
    bool made = false;
    Fov fov;
    float z = 0.f;
    za::Vector<glm::vec2> vertices; // the runtime's mesh, copied
    za::Vector<za::U32> indices;
};
za::Array<HiddenMesh, 2> hiddenMeshes;
za::Vector<gfx::Vertex> hiddenTriangles; // (a mesh's triangles before their upload)

[[nodiscard]] bool sameFov(const Fov& a, const Fov& b)
{
    return a.left == b.left && a.right == b.right && a.up == b.up && a.down == b.down;
}

template <typename T>
[[nodiscard]] bool sameContents(const za::Vector<T>& a, const za::Vector<T>& b)
{
    return a.size() == b.size() && (a.empty() || ZA_MEMCMP(a.data(), b.data(), a.size() * sizeof(T)) == 0);
}

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
    const float l = za::tan(fov.left), r = za::tan(fov.right), u = za::tan(fov.up), d = za::tan(fov.down);
    if(r - l <= 0.f || u - d <= 0.f)
    {
        return;
    }
    const float z = gl_clipcontrol_able ? 1.f : -1.f;
    HiddenMesh& mesh = hiddenMeshes[currentEye & 1];
    if(!mesh.made || !sameFov(mesh.fov, fov) || mesh.z != z || !sameContents(mesh.vertices, h->vertices) ||
        !sameContents(mesh.indices, h->indices))
    {
        hiddenTriangles.clear();
        for(const za::U32 i : h->indices)
        {
            const glm::vec2 t = h->vertices[i];
            gfx::Vertex v;
            v.pos = {(2.f * t.x - (r + l)) / (r - l), (2.f * t.y - (u + d)) / (u - d), z};
            v.color = glm::vec4{0.f};
            hiddenTriangles.pushBack(v);
        }
        gfx::upload(mesh.triangles, hiddenTriangles);
        mesh.made = true;
        mesh.fov = fov;
        mesh.z = z;
        mesh.vertices = h->vertices;
        mesh.indices = h->indices;
        Con_DPrintf("VR: the hidden area of eye %d made (%d triangles)\n", currentEye, static_cast<int>(h->indices.size() / 3));
    }

    gfx::State state;
    state.shade = gfx::Shade::Color;
    state.blend = gfx::Blend::Opaque;
    state.depthTest = true; // passes against the clear (GL skips depth writes without the test)
    state.depthWrite = true;
    gfx::draw(mesh.triangles, glm::mat4{1.f}, state);
}

void renderPortal(const ViewSize& size, int depth);

// The views a camera of `depth` sees through gates (r_refdef: that camera), each drawn (with its own views first), into
// targets allocated as the camera's (`size`), the same part of them drawn.
void renderPortals(const ViewSize& size, int depth)
{
    portals::update(r_refdef.vieworg, r_refdef.viewangles, depth);
    for(int i = 0; i < portals::viewCount(depth) && portals::moreViews(depth); ++i)
    {
        portals::selectView(i, depth);
        if(portals::wantedForView()) { renderPortal(size, depth + 1); }
    }
}

// The view through a teleporter of `depth` (1: seen by the eye; 2 seen in such a view, ...: vr_portals_recursion) for the
// eye about to be drawn (vr_portals.cpp): first the views seen in it (its camera carried through the gate), then the
// scene from that eye carried through the gate (VR_PortalView, VR_PortalClip), all but the gate's box on screen skipped
// (VR_DrawPortalMask), into targets of the eye's size (the per-view caches, sized as the eye's, aren't made anew)
// without MSAA (it is only read, by the gate's pixels), at fewer pixels deeper (portals::scaleAt: r_refdef.scale, as
// r_scale draws). Its scene colour is the teleport faces' then (VR_PortalTexture), from its depth's array.
void renderPortal(const ViewSize& size, int depth)
{
    QVR_GPU_PROFILE("portal");
    const glframebufs_t savedTargets = framebufs;
    const refdef_t savedView = r_refdef;
    if(depth < portals::maxDepth())
    {
        portals::carryCamera(depth);
        renderPortals(size, depth);
        r_refdef = savedView;
    }
    PortalTargets& target = spectatorView ? spectatorPortals : eyePortals;
    const int slot = za::clamp(depth, 1, 4) - 1;
    const int layers = portals::viewLimit(depth - 1);
    const bool same = sceneTargetsFit(target.scene, size.allocatedWidth, size.allocatedHeight, 0.f);
    ensureSceneTargets(target.scene, size.allocatedWidth, size.allocatedHeight, 0.f);
    GLuint& array = target.array[slot];
    const bool made = !same || target.layers[slot] != layers || !array;
    if(made)
    {
        for(int d = 0; d < 4; d++) // (all of them: another size)
        {
            if(target.array[d] && (d == slot || !same))
            {
                GL_DeleteNativeTexture(target.array[d]);
                target.array[d] = 0;
                target.layers[d] = 0;
            }
        }
        glGenTextures(1, &array);
        GL_BindNative(GL_TEXTURE17, GL_TEXTURE_2D_ARRAY, array);
        GL_TexStorage3DFunc(GL_TEXTURE_2D_ARRAY, 1, target.scene.format, size.allocatedWidth, size.allocatedHeight, layers);
        GL_ObjectLabelFunc(GL_TEXTURE, array, -1, "stereo layered scene target"); // (vr_vram_report)
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        target.layers[slot] = layers;
    }
    if(!target.fbo) { GL_GenFramebuffersFunc(1, &target.fbo); GL_LabelNewFramebuffer(target.fbo, "portal view array"); }
    framebufs = target.scene.fb;
    r_refdef.scale = za::clamp(savedView.scale * portals::scaleAt(depth), 1, za::max(vid.maxscale, 1));
    const int layer = portals::layer(depth);
    portals::beginView(depth);
    R_RenderView(); // the caller already set up this camera and its entities
    foveated::endScene();
    GL_BindFramebufferFunc(GL_READ_FRAMEBUFFER, target.scene.fb.composite.fbo);
    GL_BindFramebufferFunc(GL_DRAW_FRAMEBUFFER, target.fbo);
    GL_FramebufferTextureLayerFunc(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, array, 0, layer);
    // Checked when the array is made (its other layers are alike): the check makes the driver validate the target
    // anew each view.
    if(made && GL_CheckFramebufferStatusFunc(GL_DRAW_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        Sys_Error("portal view array framebuffer is incomplete");
    }
    // (the part drawn: its composite's, r_refdef.vrect, whatever r_refdef.scale drew the scene at)
    GL_BlitFramebufferFunc(0, 0, size.drawnWidth, size.drawnHeight, 0, 0, size.drawnWidth, size.drawnHeight, GL_COLOR_BUFFER_BIT,
        GL_NEAREST);
    portals::endView(array, depth);
    if(portals::shotWanted())
    {
        portals::takeShot(target.scene.fb.composite.fbo, size.drawnWidth, size.drawnHeight);
    }
    framebufs = savedTargets;
    r_refdef = savedView;
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
    SceneLook look;          // that image's glow and view axes
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
    p.owed = za::clamp(p.owed - period, 0.0, period);
    return true;
}

// The menus' preview of the spectator camera (vr_spectator_preview; vr_menuui.cpp places it, bottom left, and draws it
// in the eyes): the camera's image as the window shows it (its glow, tone, wobble and bullet time look), small, with
// mipmaps for the panel's distance. Made from each new image while a menu shows it: one pass of its pixels.
struct SpectatorPreview
{
    gfx::RenderTarget target;
    int images = 0;        // the camera's images so far
    int made = -1;         // the image it was made from
    double madeAt = -1.0;  // realtime when it was last made or found current
};
SpectatorPreview previewState;
constexpr int previewWidth = 384;

void makePreview(int windowWidth, int windowHeight)
{
    SpectatorPreview& p = previewState;
    if(!menuui::spectatorPreviewWanted())
    {
        return;
    }
    const int height = za::max(16, static_cast<int>(za::lround(static_cast<double>(previewWidth) * windowHeight / windowWidth)));
    if(p.made != p.images || p.target.width != previewWidth || p.target.height != height)
    {
        QVR_GPU_PROFILE("spectator preview");
        if(!p.target.ensure(previewWidth, height, gfx::RenderTargetFormat{.mipmaps = true, .fboName = "vr canvas"},
               "spectator preview"))
        {
            Con_Warning("VR: the spectator preview's framebuffer is incomplete\n");
            return;
        }
        drawToWindow(spectatorTargets, spectatorPace.look, cropMap(0.f, 0.f, 1.f, 1.f), Sampling::Bilinear,
            p.target.fbo, 0, p.target.width, p.target.height);
        GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D, p.target.tex);
        GL_GenerateMipmapFunc(GL_TEXTURE_2D);
        p.made = p.images;
    }
    p.madeAt = realtime;
}

// vr_spectator_skip_hidden: nothing of the window to see (minimized or hidden: qvr::windowNotVisible), and no menu
// showing the camera's preview: its whole frame skipped (the scene, its culling, glow, UI and the window pass). The
// window keeps what it last showed; the frame it shows again draws the camera anew. The changes on the console
// (developer 1), with the frames skipped.
struct SpectatorSkip
{
    bool skipping = false;
    int frames = 0; // frames skipped since it began
};
SpectatorSkip spectatorSkip;

[[nodiscard]] bool spectatorSkipped()
{
    SpectatorSkip& s = spectatorSkip;
    const bool skip = vr_spectator_skip_hidden.value != 0.f && windowNotVisible() && !menuui::spectatorPreviewWanted();
    if(skip != s.skipping)
    {
        if(skip)
        {
            Con_DPrintf("spectator camera: skipped, the window not visible\n");
        }
        else
        {
            Con_DPrintf("spectator camera: drawn again, the window visible (%d frames skipped)\n", s.frames);
        }
        s.skipping = skip;
        s.frames = 0;
    }
    if(skip)
    {
        s.frames++;
    }
    return skip;
}

void renderSpectator(GLuint windowTarget, int windowWidth, int windowHeight)
{
    const float scale = window::spectatorScale();
    const int width = za::max(16, static_cast<int>(za::lround(windowWidth * scale)));
    const int height = za::max(16, static_cast<int>(za::lround(windowHeight * scale)));
    const float fsaa = spectatorFsaa();
    if(!spectatorDue(sceneTargetsFit(spectatorTargets, width, height, fsaa)))
    {
        // Its last image again.
        makePreview(windowWidth, windowHeight);
        QVR_GPU_PROFILE("window view");
        drawToWindow(spectatorTargets, spectatorPace.look, cropMap(0.f, 0.f, 1.f, 1.f),
            scale > 1.f ? Sampling::Bilinear : Sampling::CatmullRom, windowTarget, 0, windowWidth, windowHeight);
        return;
    }
    QVR_GPU_PROFILE("spectator");
    ensureSceneTargets(spectatorTargets, width, height, fsaa);
    spectatorPace.shown = true;
    const window::Camera& camera =
        window::spectator(static_cast<float>(windowWidth) / static_cast<float>(windowHeight));

    useView(spectatorTargets, width, height); // (drawn whole: its size is the window's, not dynamic resolution's)

    renderingEye = true;
    spectatorView = true;
    currentEye = 0;
    firstEye = false; // the eyes' entities, as they set them up
    V_RenderView();
    bloom::apply(framebufs.composite.color_tex, viewSize());
    spectatorPace.look = sceneLook();
    drawUi(camera.origin, framebufs.composite.fbo, width, height, vr_spectator_hide_hud_text.value == 0.f && !recordingClean());
    spectatorView = false;
    renderingEye = false;
    previewState.images++;
    makePreview(windowWidth, windowHeight);

    QVR_GPU_PROFILE("window view");
    drawToWindow(spectatorTargets, spectatorPace.look, cropMap(0.f, 0.f, 1.f, 1.f),
        scale > 1.f ? Sampling::Bilinear : Sampling::CatmullRom, windowTarget, 0, windowWidth, windowHeight);
}

} // namespace

bool isSpectator()
{
    return spectatorView;
}

ViewSize viewSize()
{
    return {vid.width, vid.height, glwidth, glheight};
}

glm::vec4 viewUvMap(int drawnWidth, int drawnHeight, int allocatedWidth, int allocatedHeight)
{
    const double w = za::max(allocatedWidth, 1), h = za::max(allocatedHeight, 1);
    return {static_cast<float>(drawnWidth / w), static_cast<float>(drawnHeight / h), static_cast<float>(0.5 / w),
        static_cast<float>(0.5 / h)};
}

unsigned spectatorPreview(float& aspect)
{
    const SpectatorPreview& p = previewState;
    if(!p.target.tex || p.made < 0 || p.madeAt < 0.0 || realtime - p.madeAt > 0.25)
    {
        return 0;
    }
    aspect = static_cast<float>(p.target.height) / static_cast<float>(p.target.width);
    return p.target.tex;
}

bool isRenderingEye()
{
    return renderingEye;
}

int directEye()
{
    return renderingEye ? directForm : 0;
}

int eye()
{
    return currentEye;
}

bool isFirstEye()
{
    return firstEye;
}

void setCurrentEye(int eye)
{
    currentEye = eye;
}

void drawHiddenAreaOf(int eye)
{
    const int saved = currentEye;
    currentEye = eye;
    drawHiddenArea();
    currentEye = saved;
}

// Debug > Profiling > LRZ Probe (vr_debug_lrz_probe; qlrz): opaque quads over the middle quarter of the eye, drawn as
// the hidden area is (the vertex colour, no discard, depth tested and written, every colour channel): 0 near (half the
// depth range), 1 behind it, 2 far behind the scene. Made once per depth convention.
za::Array<gfx::StaticTriangles, 3> lrzProbeQuads;
za::Vector<gfx::Vertex> lrzProbeVertices;
float lrzProbeMadeFor = 0.f; // 1: reversed depth (clip control), -1: OpenGL's; 0: not made yet

void drawLrzProbe(int point)
{
    const int probe = static_cast<int>(vr_debug_lrz_probe.value);
    if(probe == 0 || (probe < 0 ? -probe : probe) != point)
    {
        return;
    }
    const float convention = gl_clipcontrol_able ? 1.f : -1.f;
    if(lrzProbeMadeFor != convention)
    {
        static constexpr float nearness[3] = {0.5f, 0.25f, 0.0005f}; // window depth from the far plane (0) to the near one (1)
        static constexpr float corners[6][2] = {{-0.5f, -0.5f}, {0.5f, -0.5f}, {0.5f, 0.5f}, {-0.5f, -0.5f}, {0.5f, 0.5f}, {-0.5f, 0.5f}};
        for(int q = 0; q < 3; q++)
        {
            lrzProbeVertices.clear();
            for(const auto& c : corners)
            {
                gfx::Vertex v;
                v.pos = {c[0], c[1], gl_clipcontrol_able ? nearness[q] : 1.f - 2.f * nearness[q]};
                v.color = q == 0 ? glm::vec4{1.f, 0.f, 1.f, 1.f} : glm::vec4{0.f, 1.f, 0.f, 1.f};
                lrzProbeVertices.pushBack(v);
            }
            gfx::upload(lrzProbeQuads[q], lrzProbeVertices);
        }
        lrzProbeMadeFor = convention;
    }
    gfx::State state;
    state.shade = gfx::Shade::Color;
    state.blend = gfx::Blend::Opaque;
    state.depthTest = true;
    state.depthWrite = true;
    if(probe > 0)
    {
        gfx::draw(lrzProbeQuads[0], glm::mat4{1.f}, state);
        gfx::draw(lrzProbeQuads[1], glm::mat4{1.f}, state);
    }
    else
    {
        gfx::draw(lrzProbeQuads[2], glm::mat4{1.f}, state);
    }
}

void eyeSceneFormats(unsigned& color, unsigned& depth)
{
    color = eyeTargets.format;
    depth = eyeTargets.depth;
}

const glframebufs_t& eyeFramebufs()
{
    return eyeTargets.fb;
}

unsigned eyeTargetsGeneration()
{
    return eyeTargets.generation;
}


// Its GL programs made now (gfx::precompilePrograms, at start-up), as their first use would: no compile in play.
void precompilePrograms()
{
    if(!mirrorProgram && !mirrorFailed)
    {
        mirrorProgram = gfx::glProgram(mirrorVs, mirrorFs, "vr mirror");
        mirrorFailed = !mirrorProgram;
        gfx::ownDeferred(mirrorProgram, &mirrorFailed);
    }
}

void releaseTargets()
{
    const bool held = eyeTargets.allocatedWidth || rightEyeTargets.allocatedWidth || spectatorTargets.allocatedWidth ||
        eyePortals.scene.allocatedWidth || spectatorPortals.scene.allocatedWidth;
    deleteSceneTargets(eyeTargets);
    deleteSceneTargets(rightEyeTargets);
    deleteSceneTargets(spectatorTargets);
    deletePortalTargets(eyePortals);
    deletePortalTargets(spectatorPortals);
    resample.destroy();
    if(targetFbo)
    {
        GL_DeleteFramebuffersFunc(1, &targetFbo);
        targetFbo = 0;
    }
    deleteDirectTargets(); // (vr_xr_direct 2's)
    previewState.target.destroy();
    previewState.made = -1;
    spectatorPace.shown = false;
    for(HiddenMesh& mesh : hiddenMeshes)
    {
        if(mesh.triangles.buffer)
        {
            GL_DeleteBuffer(mesh.triangles.buffer);
        }
        mesh = HiddenMesh{};
    }
    stereosingle::releaseTargets();
    gfx::releaseSceneTargets();
    temporal::releaseTargets();
    bloom::releaseTargets();
    haze::releaseTargets();
    water::releaseTargets();
    upscale::releaseTargets();
    GL_BindFramebufferFunc(GL_FRAMEBUFFER, 0); // (the window's: GL_DeleteFrameBuffers left none bound)
    if(held)
    {
        Con_DPrintf("VR: the eyes' targets released\n");
    }
}

namespace
{

// vr_stereo_targets_test (Misc/quakevr/stereo_targets_test.sh): what the eyes' GL objects do over frames. `begin` notes
// the counts, `steady` (frames later, the view unchanged) checks nothing was made again, `released` (after VR stopped)
// that the eye-sized objects are gone; `verdict` prints the line.
struct TargetsTest
{
    testkit::Tally tally;
    int frame = 0;
    long long hiddenUploads[2] = {};
    int underMade = 0;
    unsigned eyeGeneration = 0; // the eye targets' at begin (made again: another)
    int renderTargetsMade = 0;  // gfx::targetsMade at begin (the resample target, the glow's chain, the particles')
    int dynresChanges = 0;
    int drawnWidth = 0;
};
TargetsTest targetsTest;
constexpr const char* targetsTestName = "stereo_targets";

void targetsCheck(bool ok, const char* what)
{
    targetsTest.tally.check(ok);
    Con_Printf("%s: %s %s\n", targetsTestName, ok ? "PASS" : "FAIL", what);
}

void targetsTest_f()
{
    TargetsTest& t = targetsTest;
    const char* step = Cmd_Argc() > 1 ? Cmd_Argv(1) : "";
    if(!q_strcasecmp(step, "begin"))
    {
        t.tally.reset();
        t.frame = host_framecount;
        for(int eye = 0; eye < 2; eye++)
        {
            t.hiddenUploads[eye] = hiddenMeshes[eye].triangles.uploads;
        }
        t.underMade = gfx::underTargetsMade();
        t.eyeGeneration = eyeTargets.generation;
        t.renderTargetsMade = gfx::targetsMade;
        t.dynresChanges = dynres::changes();
        t.drawnWidth = eyeTargets.drawnWidth;
        Con_Printf("%s: begun (hidden area uploads %lld %lld, reverse-order framebuffers %d)\n", targetsTestName,
            t.hiddenUploads[0], t.hiddenUploads[1], t.underMade);
    }
    else if(!q_strcasecmp(step, "steady"))
    {
        targetsCheck(host_framecount - t.frame >= 10 && eyeTargets.drawnWidth > 0, va("eyes drawn since begin (%d frames, %dx%d)",
            host_framecount - t.frame, eyeTargets.drawnWidth, eyeTargets.drawnHeight));
        if(vr_visibility_mask.value != 0.f && vr_mock_hidden_area.value != 0.f)
        {
            for(int eye = 0; eye < 2; eye++)
            {
                const long long uploads = hiddenMeshes[eye].triangles.uploads;
                targetsCheck(hiddenMeshes[eye].made && uploads >= 1, va("eye %d's hidden area made (%lld uploads)", eye, uploads));
                targetsCheck(uploads == t.hiddenUploads[eye], va("eye %d's hidden area not uploaded again (%lld at begin, %lld now)",
                    eye, t.hiddenUploads[eye], uploads));
            }
        }
        if(vr_particle_saturate.value != 0.f)
        {
            targetsCheck(gfx::underTargetsMade() > 0 && gfx::underTargetsMade() == t.underMade,
                va("reverse-order particles' framebuffers not made again (%d at begin, %d now)", t.underMade,
                    gfx::underTargetsMade()));
        }
    }
    else if(!q_strcasecmp(step, "stepped"))
    {
        // (after dynamic resolution stepped: vr_dynres 2 and a load, vr_dynres_test_load)
        targetsCheck(dynres::changes() > t.dynresChanges, va("dynamic resolution stepped since begin (%d steps)",
            dynres::changes() - t.dynresChanges));
        targetsCheck(eyeTargets.drawnWidth != t.drawnWidth && eyeTargets.drawnWidth < eyeTargets.allocatedWidth,
            va("the eyes drawn at another size, a part of their targets (%dx%d of %dx%d; %d wide at begin)", eyeTargets.drawnWidth,
                eyeTargets.drawnHeight, eyeTargets.allocatedWidth, eyeTargets.allocatedHeight, t.drawnWidth));
        targetsCheck(eyeTargets.generation == t.eyeGeneration, va("the eyes' targets not made again (generation %u at begin, %u now)",
            t.eyeGeneration, eyeTargets.generation));
        targetsCheck(gfx::targetsMade == t.renderTargetsMade, va("no render target made again (%d made at begin, %d now: %s)",
            t.renderTargetsMade, gfx::targetsMade, gfx::targetsMadeByName().cStr()));
    }
    else if(!q_strcasecmp(step, "released"))
    {
        // (after VR stopped: vr_enabled 0)
        targetsCheck(backend() == nullptr, "VR stopped");
        targetsCheck(!eyeTargets.allocatedWidth && !eyeTargets.fb.scene.fbo && !eyeTargets.fb.composite.fbo,
            va("the eyes' targets released (%dx%d, scene framebuffer %u)", eyeTargets.allocatedWidth, eyeTargets.allocatedHeight,
                eyeTargets.fb.scene.fbo));
        targetsCheck(!spectatorTargets.allocatedWidth && !spectatorTargets.fb.scene.fbo, "the spectator camera's targets released");
        bool portals = true;
        for(const PortalTargets* p : {&eyePortals, &spectatorPortals})
        {
            portals = portals && !p->scene.allocatedWidth && !p->fbo;
            for(const GLuint array : p->array)
            {
                portals = portals && !array;
            }
        }
        targetsCheck(portals, "the views through teleporters' targets and arrays released");
        targetsCheck(!resample.tex && !targetFbo, "the resample target and the eye image's framebuffer released");
        targetsCheck(!hiddenMeshes[0].triangles.buffer && !hiddenMeshes[1].triangles.buffer, "the hidden areas' buffers released");
    }
    else if(!q_strcasecmp(step, "verdict"))
    {
        t.tally.verdict(targetsTestName);
    }
    else
    {
        Con_Printf("vr_stereo_targets_test begin | steady | stepped | released | verdict\n");
    }
}

} // namespace

void init()
{
    Cmd_AddCommand("vr_stereo_targets_test", targetsTest_f);
}

} // namespace qvr::stereo

using namespace qvr;

// V_RenderView has already established the exact camera (including its projection override).
// Each view prepares its own portal; R_RenderView directly avoids recursion and a second entity setup.
extern "C" void VR_RenderPortalForView()
{
    stereo::renderPortals(stereo::viewSize(), 0);
}

// The flat screen's one view (SCR_UpdateScreen when VR_RenderView drew no eyes: flatscreen play, vr_enabled 0) set up as
// the eyes' first: its models take the shadow pass's lerp and matrices (vr_shadow_setup_share; r_alias.c
// R_AliasTransformShared) instead of working them out again, as the headset's first eye does (perf 2026-11-09: the
// flat view set every model up twice). Kept for no second eye; vr_alias_eye_cache 0 neither.
extern "C" void VR_WindowViewBegin()
{
    R_AliasEyeCacheBegin(vr_alias_eye_cache.value > 0.f ? 1 : 0);
}

extern "C" void VR_WindowViewEnd()
{
    R_AliasEyeCacheEnd();
}

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

    // Ironwail's r_scale (the 3D view drawn at 1/N) is the flat screen's: the eyes' size is vr_render_scale's (and dynres),
    // and r_scale would shrink them on top of it and turn foveated rendering off. The frame's view at full size, for the
    // eyes, the gates and the spectator (vr_spectator_scale); SCR_UpdateScreen sets it from r_scale again next frame.
    r_refdef.scale = 1;

    // The runtime's menu has the focus (vr_xr_unfocused): its last frame shown again, the GPU left to the menu. The
    // window keeps its last image.
    if(frame.hold)
    {
        profile::begin("xr submit", true); // xrEndFrame
        be->endFrame(true);
        profile::end();
        return 1;
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
    dynres::beginFrame(); // (the scale this frame is drawn at, for its GPU time read back later)
    gfxpresets::frame();  // (a graphics pick too slow to use: put back, questcrash)
    // The size the eyes are drawn at this frame, and the size their targets are allocated at.
    const int drawnWidth = scaledEyeSize(imageWidth, sizes.maxWidth);
    const int drawnHeight = scaledEyeSize(imageHeight, sizes.maxHeight);
    // Under dynamic resolution the ceiling's (vr_dynres.hpp): allocated once, its bucket's part drawn; else the same.
    const int allocatedWidth = za::max(drawnWidth, dynres::eyeSize(imageWidth, sizes.maxWidth, dynres::allocationScale()));
    const int allocatedHeight = za::max(drawnHeight, dynres::eyeSize(imageHeight, sizes.maxHeight, dynres::allocationScale()));
    stereo::resampling = drawnWidth != imageWidth || drawnHeight != imageHeight || upscale::sharpenAtNative();
    // vr_eye_subimage (QUEST_PLAN.md C2g): an eye drawn smaller and resampled bilinearly is post-processed straight
    // into its image's lower left part instead, and the layer shows that part: the compositor's own sampling scales it
    // (it resamples the image anyway), one full-screen read and write an eye fewer (a tiler's bandwidth). The part's
    // pixels are the resample target's; the UI is drawn into it at the eye's size.
    const int subImageMode = static_cast<int>(vr_eye_subimage.value);
    bool subImage = stereo::resampling && drawnWidth <= imageWidth && drawnHeight <= imageHeight &&
                    (subImageMode >= 2 || (subImageMode == 1 && (VR_GLESCompat() || VR_Android()))) &&
                    upscale::resamplesBilinearly(drawnWidth, drawnHeight, imageWidth, imageHeight);
    subImage = be->setEyeImageRect(0, subImage ? drawnWidth : 0, subImage ? drawnHeight : 0) && subImage;
    subImage = be->setEyeImageRect(1, subImage ? drawnWidth : 0, subImage ? drawnHeight : 0) && subImage;
    if(subImage)
    {
        stereo::resampling = false;
    }
    else
    {
        be->setEyeImageRect(0, 0, 0); // (the left eye's set before the right's refused)
    }
    const int shownWidth = subImage ? drawnWidth : imageWidth; // the eye's part of its image: the UI's, the final eyeshot's
    const int shownHeight = subImage ? drawnHeight : imageHeight;
    // (profiling builds: the eyes' drawn width over the image's, vr_render_scale's result, and the width itself)
    VR_TracyPlot("Render scale", static_cast<double>(drawnWidth) / imageWidth);
    VR_TracyPlot("Eye render width", drawnWidth);

    stereo::ensureSceneTargets(stereo::eyeTargets, allocatedWidth, allocatedHeight, temporal::eyeFsaa(vid_fsaa.value), true); // (no MSAA under TAA)
    const bool splitTargets = stereo::splitEyeTargets(stereo::eyeTargets.fb.scene.samples);
    if(splitTargets)
    {
        stereo::ensureSceneTargets(stereo::rightEyeTargets, allocatedWidth, allocatedHeight, temporal::eyeFsaa(vid_fsaa.value), true);
    }
    else
    {
        stereo::deleteSceneTargets(stereo::rightEyeTargets);
    }
    if(stereo::resampling)
    {
        stereo::ensureResampleTarget(allocatedWidth, allocatedHeight);
    }
    // Temporal AA (vr_temporal.cpp): this frame's slots for the models' positions, the motion target on the eye targets
    temporal::beginFrame(stereo::eyeTargets.fb.scene.fbo, stereo::eyeTargets.fb.composite.fbo,
        stereo::ViewSize{allocatedWidth, allocatedHeight, drawnWidth, drawnHeight}, stereo::eyeTargets.fb.scene.samples);

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

    // The eye's image from the runtime, attached to the target framebuffer (the post-processing's, the UI's). The
    // runtime's calls are GPU scopes too: the GPU time between the eyes' own scopes (the runtime's work on this context,
    // and the GPU idle while the CPU waits in them).
    const auto acquire = [be](int eye, bool keepBinding, bool attach = true)
    {
        profile::begin("xr acquire", true); // xrWaitSwapchainImage
        const unsigned image = be->acquireEyeImage(eye);
        profile::end();
        if(image && attach) // (not for an image with framebuffers of its own: vr_xr_direct 2)
        {
            // The engine's own record of the binding, not glGetIntegerv: a glGet waits for NVIDIA's threaded driver to
            // run all queued so far, 0.2-0.3 ms of the CPU an eye (VRTRACE 2026-10-16, B7).
            if(keepBinding && VR_DebugGLState())
            {
                GL_CheckBoundFramebuffers("late acquire");
            }
            const GLuint bound = keepBinding ? GL_BoundDrawFramebuffer() : 0;
            GL_BindFramebufferFunc(GL_FRAMEBUFFER, stereo::targetFbo);
            GL_FramebufferTexture2DFunc(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, image, 0);
            if(keepBinding)
            {
                GL_BindFramebufferFunc(GL_FRAMEBUFFER, bound);
            }
        }
        return image;
    };
    // vr_xr_late_acquire: only once the scene is drawn (it writes into the eye's own targets), so the runtime's wait
    // for the image overlaps the scene's drawing.
    const bool lateAcquire = vr_xr_late_acquire.value != 0.f;

    // Single-pass stereo (vr_stereo_single.cpp, vr_stereo_single): both eyes' views set up first and the passes ported so
    // far drawn once into both; each eye below draws the rest of its scene from its layer.
    const bool singlePass = stereosingle::wanted(stereo::eyeTargets.fb.scene.samples);
    // C5's first path (vr_xr_direct, QUEST_PLAN.md 6w; a probe, the ES profile only): the eye's scene drawn straight
    // into its swapchain image, the image in place of the composite target's colour, no post-process (no tone curve:
    // colours over 1 clip). Only where the eye is drawn at the image's size or into its lower left part (vr_eye_subimage),
    // one pass an eye (not single-pass stereo), and the image acquired before the scene.
    // The Quest's runtime crashed in xrEndFrame (C4p, QUEST_PLAN.md 6w) when foveated eye images (vr_xr_foveation) were
    // acquired after the scene, or their old colours invalidated: with foveation they are acquired first, not invalidated.
    // (vr_xr_foveation_probe 1: late acquire and the invalidate allowed anyway, to narrow the crash: C4q, 6x)
    const int fovProbe = static_cast<int>(vr_xr_foveation_probe.value);
    // (no longer behind -xrfoveation: qfov3.) The images' own level, not vr_xr_foveation's: a level changed after the
    // frame began (the menu's Quest Foveation, the console) is on the images only from the next frame's new swapchains.
    // Read from the cvar, Off after High took the post-process path (late acquire, the image's invalidate) on the still
    // foveated images, and the runtime crashed in xrEndFrame (SIGSEGV 0x30 in libGLESv2_adreno, as C4p: qfovcrash).
    // (vr_xr_foveation_probe 4: from the cvar, the crash's check)
    const bool xrFoveation = VR_Android() && ((fovProbe & 4) ? vr_xr_foveation.value > 0.f : be->eyeImagesFoveated());
    // questcrash2: on the Quest the post-process path takes that order with plain images too (Quest Foveation Off, OIT,
    // bloom). His config's eyes went through the post-process into plain images (late acquire, the invalidate) and every
    // worn start crashed in xrEndFrame as C4p did (unworn runs never draw enough frames to show it); the order the
    // foveated images take there (under water in Standalone) has never crashed. (probe 1: the late order, the A/B)
    const bool xrEarlyOrder = VR_Android() && !(fovProbe & 1);
    // qfov: the runtime's foveation reaches the scene only drawn straight into the images, so with it the eyes are, with
    // framebuffers of each image's own and its depth renderbuffer (2; the tiler keeps it on chip, as Meta's samples do;
    // form 1's unfoveated depth texture keeps the driver from foveating), unless vr_xr_direct says
    // otherwise. Drawn there the scene's shaders roll their colours off themselves (VR_SceneTone) and the in-tile pass
    // grades and dithers (tonemap::drawInTile): only while that is all the post-process would do (tonemap::postInScene;
    // no bloom, no OIT, not under water) and the eye fits in its image (at its size, or its lower left part:
    // vr_eye_subimage, dynamic resolution's buckets).
    const int directWanted = vr_xr_direct.value < 0.f ? 0 // (-1: never, foveation or not: the post-process path's A/B)
                             : vr_xr_direct.value != 0.f ? (vr_xr_direct.value >= 2.f ? 2 : 1) : xrFoveation ? 2 : 0;
    const glm::vec3 wobble = water::viewWobble(); // (the last frame's liquid: the eye's own is known in its view)
    const char* const notDirect = directWanted <= 0                       ? ""
                                  : !VR_GLESForm(VR_GLES_FORM_GLSL)       ? "not OpenGL ES"
                                  : singlePass                            ? "single-pass stereo"
                                  : splitTargets                          ? "split eye targets"
                                  : stereo::resampling                    ? "resampled into the image"
                                  : drawnWidth > imageWidth || drawnHeight > imageHeight ? "larger than the image"
                                  : !tonemap::postInScene()               ? "an exposure, the headset gamma or contrast, or no in-tile grade"
                                  : vr_bloom.value != 0.f                 ? "bloom"
                                  : R_GetEffectiveAlphaMode() == ALPHAMODE_OIT ? "OIT"
                                  : wobble.y + wobble.z > 0.f             ? "under water"
                                                                          : nullptr;
    const bool direct = notDirect == nullptr;
    if(const int noted = direct ? directWanted : directWanted > 0 ? -1 : 0; noted != stereo::directNoted)
    {
        stereo::directNoted = noted;
        Con_Printf("VR: eyes %s%s%s\n", noted > 0 ? va("drawn straight into the headset's images (form %d)", noted)
                                         : noted < 0 ? "through the post-process (straight into the images wanted, not now: "
                                                     : "through the post-process",
            noted < 0 ? notDirect : "", noted < 0 ? ")" : "");
    }
    const bool late = lateAcquire && !direct && !xrEarlyOrder;
    // vr_eye_rect_place (qeye): an eye drawn straight into a part of its image with framebuffers of the image's own (form
    // 2: its depth at the image's size) placed in it by its lower left corner. The runtime's foveation reduces whole bins
    // of the image (864 x 448 of the Quest 3's 1680 x 1760) round the rect submitted: in the corner the part's bottom rows
    // stayed full at every level (qfov4). The scene's viewport (glx, gly: R_SetupGL, R_WarpScaleView), the in-tile pass,
    // the UI, the eyeshots and the rect submitted all take the corner. Measured (qeye): the middle is slower (the full
    // bins round the rect's centre then straddle the grid's middle lines: 4 an eye at 0.65 instead of 2), so 0 stays.
    const float place = subImage && direct && directWanted >= 2 ? za::clamp(vr_eye_rect_place.value, 0.f, 2.f) * 0.5f : 0.f;
    const int partX = static_cast<int>(place * static_cast<float>(imageWidth - drawnWidth) + 0.5f);
    const int partY = static_cast<int>(place * static_cast<float>(imageHeight - drawnHeight) + 0.5f);
    if(const int noted = place > 0.f ? partX * 65536 + partY : -1; noted != stereo::partNoted)
    {
        stereo::partNoted = noted;
        Con_DPrintf("VR: the eyes' %dx%d part at %d %d of their %dx%d images (vr_eye_rect_place)\n", drawnWidth, drawnHeight,
            partX, partY, imageWidth, imageHeight);
    }
    // The images' own framebuffers (vr_xr_direct 2) let go as soon as their images are gone (new swapchains: a level
    // changed) or the eyes no longer use them: a framebuffer kept with a destroyed foveated image attached crashed the
    // driver in xrEndFrame (qfov2: the post-process path after foveation was turned off mid-game).
    if(stereo::directTargets.made &&
        (!(direct && directWanted >= 2) || stereo::directTargets.serial != be->eyeImagesSerial()))
    {
        stereo::deleteDirectTargets();
    }
    if(singlePass)
    {
        stereo::useView(stereo::eyeTargets, drawnWidth, drawnHeight);
        for(int eye = 0; eye < 2; eye++)
        {
            stereo::renderingEye = true;
            stereo::currentEye = eye;
            stereo::firstEye = eye == 0;
            V_SetupView();
            stereosingle::captureEye(eye);
        }
        QVR_GPU_PROFILE("eyes: single pass");
        stereosingle::render(stereo::viewSize());
        stereo::renderingEye = false;
    }

    int eyesRendered = 0;
    bool rightTargetsIn = false; // vr_eye_targets_split: rightEyeTargets swapped into eyeTargets for the right eye
    for(int eye = 0; eye < 2; eye++)
    {
        if(splitTargets && eye == 1)
        {
            za::genericSwap(stereo::eyeTargets, stereo::rightEyeTargets);
            rightTargetsIn = true;
        }
        unsigned eyeImage = 0;
        const bool ownWanted = direct && directWanted >= 2; // (direct: never a late acquire)
        if(!late && !(eyeImage = acquire(eye, false, !ownWanted)))
        {
            continue;
        }
        // vr_xr_direct 2: the image's own framebuffers (the scene's with the eye's depth; the UI's), for the eye
        const stereo::DirectImage* const own =
            ownWanted ? stereo::directImage(eye, eyeImage, imageWidth, imageHeight, be->eyeImagesSerial()) : nullptr;
        if(ownWanted && !own) // (no free slot: the shared framebuffers, form 1)
        {
            GL_BindFramebufferFunc(GL_FRAMEBUFFER, stereo::targetFbo);
            GL_FramebufferTexture2DFunc(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, eyeImage, 0);
        }
        const GLuint sharedTargetFbo = stereo::targetFbo;

        stereo::useView(stereo::eyeTargets, drawnWidth, drawnHeight);
        const bool directOwn = own != nullptr;
        stereo::directForm = direct && eyeImage ? (directOwn ? 2 : 1) : 0;
        // The eye's part in its image (vr_eye_rect_place: form 2 alone; the corner else, as useView left it)
        const int eyeX = directOwn ? partX : 0, eyeY = directOwn ? partY : 0;
        if(subImage)
        {
            be->setEyeImageRect(eye, drawnWidth, drawnHeight, eyeX, eyeY);
        }
        glx = eyeX;
        gly = eyeY;
        if(directOwn)
        {
            framebufs.composite.fbo = own->fbo;
            stereo::targetFbo = own->uiFbo; // (until the image's release)
        }
        else if(direct && eyeImage)
        {
            GL_BindFramebufferFunc(GL_FRAMEBUFFER, framebufs.composite.fbo);
            GL_FramebufferTexture2DFunc(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, eyeImage, 0);
        }

        stereo::renderingEye = true;
        stereo::currentEye = eye;
        stereo::firstEye = eyesRendered == 0;
        QVR_GPU_PROFILE(eye == 0 ? "eye L" : "eye R");

        // The models' set-up the same for both eyes: the first eye's kept for the second (vr_alias_eye_cache; r_alias.c)
        const int eyeCache = static_cast<int>(vr_alias_eye_cache.value);
        R_AliasEyeCacheBegin(eyeCache <= 0 ? 0 : stereo::firstEye ? 1 : eyeCache >= 2 ? 3 : 2);
        stereosingle::beginEye(eye);
        temporal::beginEye(eye);
        tonemap::glProbeScene(true); // (vr_gl_probe)
        stereo::foveateEye(frame.eyes[eye].fov); // (vr_gl_foveation; the runtime's own, vr_xr_foveation, follows the rect)
        tonemap::sceneDithered = false; // (VR_SceneDither: set by the eye's view)
        V_RenderView();
        if(stereo::directForm) // (no GPU scope of its own: in the scene's render pass, the tile still on chip)
        {
            tonemap::drawInTile(framebufs.composite.fbo, eyeX, eyeY, drawnWidth, drawnHeight); // (the image's framebuffer, as drawn)
        }
        glx = gly = 0; // (the eye's part: the passes after the scene are given it)
        tonemap::glProbeScene(false);
        stereo::invalidateSceneDepth(); // (vr_gl_invalidate)
        temporal::endEye();
        stereosingle::endEye();
        R_AliasEyeCacheBegin(0);
        // vr_xr_direct: the scene is in the image either way: drawn there when it needed no scene target (R_SetupGL),
        // else put there by R_WarpScaleView (its target, the composite's framebuffer, has the image attached: an eye
        // always post-processes, GL_NeedsPostprocess). Neither is post-processed (direct's design); the composite's
        // own colour wasn't drawn this frame, so the post-process reading it would put an old picture over the image.
        // The composite target gets its own colour back.
        const bool drawnDirect = direct && eyeImage;
        stereo::directForm = 0; // (the scene's: not the UI's, nor the spectator camera's after the eyes)
        if(directOwn)
        {
            framebufs.composite.fbo = stereo::eyeTargets.fb.composite.fbo; // (the image stays in its own framebuffers)
        }
        else if(direct && eyeImage)
        {
            GL_BindFramebufferFunc(GL_FRAMEBUFFER, framebufs.composite.fbo);
            GL_FramebufferTexture2DFunc(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, framebufs.composite.color_tex, 0);
        }
        tonemap::glProbe(eye, 0, framebufs.scene.fbo, framebufs.composite.fbo, stereo::targetFbo, drawnWidth, drawnHeight, shownWidth,
            shownHeight); // (vr_gl_probe: Debug > Startup)
        dynres::testLoad(eye, framebufs.composite.fbo, drawnWidth, drawnHeight); // (vr_dynres_test_load: Debug > Profiling)
        {
            QVR_TRACY_SCOPE(zoneFoveated, "eye: foveated end"); // (profiling builds only: the eye's own steps)
            foveated::endScene(); // begun after the scene's clear (VR_DrawHiddenArea)
        }
        if(!drawnDirect)
        {
            bloom::apply(framebufs.composite.color_tex, stereo::viewSize()); // added by GL_PostProcess
        }
        if(late && !acquire(eye, true))
        {
            stereo::renderingEye = false;
            continue;
        }

        // vr_eyeshot 1 takes the eye's final image (after the resample and vr_foveated_debug, before the UI); 2 the
        // rendered one (before the resample) with its float scene; 3 the final image with the UI (the HUD panel, the
        // menus), as the headset shows it.
        const bool shotFinal = vr_eyeshot.value > 0.f && vr_eyeshot.value < 2.f;
        const bool shotUi = vr_eyeshot.value >= 3.f;
        profile::begin("postprocess", true);
        if(!stereo::resampling && stereo::invalidating() && !drawnDirect && !xrEarlyOrder) // (the Quest: C4p, questcrash2)
        {
            // The image's old colours: the post-process writes over all of the eye's part of it, the rest unshown.
            constexpr GLenum color0[] = {GL_COLOR_ATTACHMENT0};
            GL_BindFramebufferFunc(GL_DRAW_FRAMEBUFFER, stereo::targetFbo);
            GL_InvalidateFramebufferFunc(GL_DRAW_FRAMEBUFFER, 1, color0);
        }
        if(!drawnDirect)
        {
            GL_PostProcess(); // into the image, or the resample target (VR_PostProcessTarget)
        }
        if(!shotFinal && !shotUi)
        {
            tonemap::eyeshot(eye, VR_PostProcessTarget(), framebufs.composite.fbo, drawnWidth, drawnHeight, eyeX, eyeY);
        }
        profile::end();
        if(stereo::resampling)
        {
            // After the tone curve, grade, gamma and dither (FSR and NIS want the display's colours), before the UI.
            QVR_GPU_PROFILE("upscale");
            upscale::resample(eye, stereo::resample.tex, stereo::resample.fbo, drawnWidth, drawnHeight, stereo::resample.width,
                stereo::resample.height, stereo::targetFbo, imageWidth, imageHeight);
        }
        {
            QVR_TRACY_SCOPE(zoneFoveatedDebug, "eye: foveated debug");
            // (questrender) The Quest's foveation: the GL one on our scene (vr_gl_foveation), else the runtime's on the
            // eye images (vr_xr_foveation, modelled; vr_foveated_debug 2: its zones at that level on any headset)
            foveated::DensityZones questZones;
            if(vr_foveated_debug.value != 0.f)
            {
                questZones = drawnDirect ? foveated::DensityZones{} : stereo::glFoveationZones(frame.eyes[eye].fov);
                if(questZones.gain <= 0.f && (xrFoveation || vr_foveated_debug.value >= 2.f))
                {
                    questZones = foveated::runtimeZones(za::max(1, static_cast<int>(vr_xr_foveation.value)));
                }
            }
            foveated::drawDebug(eye, stereo::targetFbo, shownWidth, shownHeight, drawnWidth, drawnHeight, questZones); // vr_foveated_debug
            if(stereo::mirrored(eye))
            {
                foveated::drawDebug(eye, framebufs.composite.fbo, drawnWidth, drawnHeight, drawnWidth, drawnHeight, questZones);
            }
        }
        if(shotFinal)
        {
            tonemap::eyeshot(eye, stereo::targetFbo, framebufs.composite.fbo, shownWidth, shownHeight, eyeX, eyeY);
        }
        tonemap::glProbe(eye, 1, 0, framebufs.composite.fbo, stereo::targetFbo, drawnWidth, drawnHeight, shownWidth, shownHeight);

        // The UI over the eye's final image, at its full size: after the post-processing, it is not warped or blurred
        // under water (vr_water.cpp), the glow is not added over it, nor the eye's gamma. The wrist gadget and all
        // else in the world are in the scene. Over the scene's colours too, for the mirror.
        const Fov& eyeFov = frame.eyes[eye].fov; // (the canvas's size: vr_menu_resolution)
        panel::noteEyeImage(static_cast<float>(shownHeight) / za::max(1e-3f, za::tan(eyeFov.up) - za::tan(eyeFov.down)));
        {
            QVR_TRACY_SCOPE(zoneUi, "eye: UI");
            stereo::drawUi(hands::current().eyeOrigin[eye], stereo::targetFbo, shownWidth, shownHeight, !recordingClean(), true, eyeX,
                eyeY); // (recording mode: no head text)
        }
        if(shotUi)
        {
            tonemap::eyeshot(eye, stereo::targetFbo, framebufs.composite.fbo, shownWidth, shownHeight, eyeX, eyeY);
        }
        tonemap::glProbe(eye, 2, 0, framebufs.composite.fbo, stereo::targetFbo, drawnWidth, drawnHeight, shownWidth, shownHeight);
        if(stereo::mirrored(eye))
        {
            stereo::drawUi(hands::current().eyeOrigin[eye], framebufs.composite.fbo, drawnWidth, drawnHeight,
                vr_mirror_hide_hud_text.value == 0.f && !recordingClean());
        }

        stereo::renderingEye = false;

        // Released images belong to the runtime again: do not keep them attached. Detached (and no framebuffer of ours
        // bound) before the release, as Meta's samples do (C4q: the foveated images' crash, QUEST_PLAN.md 6x;
        // vr_xr_foveation_probe 2: after it, as before; the PC keeps the old order).
        const bool detachAfter = !VR_Android() || (fovProbe & 2) != 0;
        if(detachAfter)
        {
            profile::begin("xr release", true);
            be->releaseEyeImage(eye);
            profile::end();
        }
        if(!directOwn) // (an image's own framebuffers keep it: Meta's samples')
        {
            GL_BindFramebufferFunc(GL_FRAMEBUFFER, stereo::targetFbo);
            GL_FramebufferTexture2DFunc(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
        }
        stereo::targetFbo = sharedTargetFbo;
        if(!detachAfter)
        {
            GL_BindFramebufferFunc(GL_FRAMEBUFFER, 0);
            profile::begin("xr release", true); // xrReleaseSwapchainImage
            be->releaseEyeImage(eye);
            profile::end();
        }
        ++eyesRendered;

        QVR_TRACY_SCOPE(zoneMirror, "eye: mirror to window");
        stereo::mirrorToWindow(eye, windowTarget, windowWidth, windowHeight);
    }
    if(rightTargetsIn)
    {
        za::genericSwap(stereo::eyeTargets, stereo::rightEyeTargets);
    }

    GL_BindFramebufferFunc(GL_FRAMEBUFFER, windowTarget);
    R_AliasEyeCacheEnd();
    stereosingle::endFrame();

    // Both eyes, or no projection layer at all (an eye's image could not be acquired).
    profile::begin("xr submit", true); // xrEndFrame
    be->endFrame(eyesRendered == 2);
    profile::end();
    bench::submitted();
    loading::submitted(eyesRendered == 2); // (the level change's notice was in them: vr_loading.cpp)

    // The window's spectator camera, once the headset has its images: it does not delay them. Not while nothing of the
    // window can be seen (minimized or hidden: vr_spectator_skip_hidden), unless the menus' preview shows it.
    const bool spectatorOn = window::view() == window::View::Spectator && eyesRendered == 2;
    const bool spectatorSkipped = spectatorOn && stereo::spectatorSkipped();
    if(spectatorOn && !spectatorSkipped)
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
    const int width = scaledEyeSize(imageWidth, sizes.maxWidth); // (drawn; allocated as VR_RenderView's: vid)
    const int height = scaledEyeSize(imageHeight, sizes.maxHeight);
    const int allocatedWidth = za::max(width, dynres::eyeSize(imageWidth, sizes.maxWidth, dynres::allocationScale()));
    const int allocatedHeight = za::max(height, dynres::eyeSize(imageHeight, sizes.maxHeight, dynres::allocationScale()));

    const int windowWidth = vid.width, windowHeight = vid.height;
    const int savedGlx = glx, savedGly = gly, savedGlwidth = glwidth, savedGlheight = glheight;
    const vrect_t savedVrect = r_refdef.vrect;
    const float savedFovX = r_refdef.fov_x, savedFovY = r_refdef.fov_y;
    for(int eye = 0; eye < 2; eye++)
    {
        vid.width = allocatedWidth;
        vid.height = allocatedHeight;
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

extern "C" void VR_LrzProbe(int point)
{
    if(stereo::renderingEye && !stereo::spectatorView && vr_debug_lrz_probe.value != 0.f)
    {
        stereo::drawLrzProbe(point);
    }
}

extern "C" void VR_DrawHiddenArea()
{
    if(stereo::renderingEye && !stereo::spectatorView) // no lenses in front of the spectator camera
    {
        stereo::drawHiddenArea();
        tonemap::glProbeDraw(false); // vr_gl_probe 2: blue under the scene
        const stereo::ViewSize size = stereo::viewSize(); // (the lens's place in the part drawn)
        foveated::beginScene(stereo::currentEye, size.drawnWidth, size.drawnHeight); // vr_foveated, until the scene ends
    }
}

extern "C" int VR_SceneAttachmentFoveation(const char* name)
{
    // (the eyes' scene colours without MSAA: "composite colors", gl_rmain.c; QCOM_texture_foveated wants its bits set
    // before the storage)
    // (C4n: the depth foveated too makes the framebuffer incomplete on the Quest 3, 0x8BFF: the colour alone)
    // (C4o: the scene fbo's colour too: the eye's scene is drawn there with scene effects, the blood's or the water's;
    // their depth and the OIT targets by vr_gl_foveation_parts)
    if(!stereo::creatingSceneTargets || !stereo::creatingFoveated)
    {
        return 0;
    }
    const int parts = stereo::creatingFoveatedParts;
    const bool colour = !ZA_STRCMP(name, "composite colors") || !ZA_STRCMP(name, "scene colors");
    const bool depth = !ZA_STRCMP(name, "composite depth/stencil") || !ZA_STRCMP(name, "scene depth/stencil");
    const bool oit = !ZA_STRCMP(name, "oit accum") || !ZA_STRCMP(name, "oit revealage");
    return colour || (depth && (parts & 2)) || (oit && (parts & 4)) ? stereo::foveationEnable : 0;
}

extern "C" int VR_SceneFoveationIncomplete(unsigned status, const char* name)
{
    // (GL_CreateFBO: a foveated eye framebuffer the driver won't take, 0x8BFF GL_FRAMEBUFFER_INCOMPLETE_FOVEATION_QCOM:
    // kept for ensureSceneTargets, which makes them again without foveation, instead of an error)
    if(!stereo::creatingSceneTargets || !stereo::creatingFoveated)
    {
        return 0;
    }
    Con_Printf("VR: %s incomplete with foveation (0x%x, parts %d)\n", name, status, stereo::creatingFoveatedParts);
    stereo::foveationIncomplete = status;
    return 1;
}

extern "C" unsigned VR_SceneColorFormat(unsigned format)
{
    // The window's own targets: float in flatscreen play with vr_flat_tonemap (vr_flatlook.cpp).
    return stereo::creatingSceneTargets ? stereo::creatingFormat : flatlook::windowSceneFormat(format);
}

extern "C" unsigned VR_SceneDepthFormat(unsigned format)
{
    if(stereo::creatingSceneTargets)
    {
        return stereo::creatingDepth;
    }
    // The window's own targets. Flatscreen play draws the VR body at a monitor's camera, so it takes the eyes' depth too
    // (VR_FlatNearPlane); a config's vr_depth_float comes after the first of these, and applies when they are next made.
    stereo::flatFloatDepth = flatplay::flagGiven() && stereo::sceneDepthFormat() == GL_DEPTH32F_STENCIL8;
    return stereo::flatFloatDepth ? GL_DEPTH32F_STENCIL8 : format;
}

// Flatscreen play's near plane, the headset's (vr_nearclip), with the window's targets holding the eyes' float depth: the
// desktop's near plane of up to 4 units (12 cm) cuts open the hands, the arms and the weapons the mode holds at the camera
// (a shouldered gun at the ironsight, a hand raised to the face). Everything takes the one projection, the world too, so
// the model's shadows, outlines and the overlays on it line up as they do in a headset, and its depth against nearby world
// is the true one. Reversed Z in float depth loses nothing by it: a relative 2^-24 at any near plane and distance, against
// the 24-bit depth's z/(4 * 2^24) at the desktop's. 0: the desktop's plane is kept (no clip control, or vr_depth_float 0:
// 24 bits have no precision to spare, so held items are only drawn at the plane, VR_AliasNearEye).
extern "C" float VR_FlatNearPlane()
{
    return stereo::flatFloatDepth && flatplay::active() ? CLAMP(0.02f, vr_nearclip.value, 4.f) : 0.f;
}

extern "C" int VR_SceneSamples(int samples)
{
    // (the window's: none under flat-screen play's TAA, vr_flat_taa)
    return stereo::creatingSceneTargets ? Q_nextPow2(static_cast<int>(q_max(1.f, stereo::creatingFsaa))) : temporal::windowSamples(samples);
}

extern "C" void VR_PostProcessView()
{
    const stereo::ViewSize size = stereo::viewSize(); // (the window's: its targets drawn whole)
    GL_Uniform4fFunc(12, static_cast<float>(size.drawnWidth), static_cast<float>(size.drawnHeight), 0.f, 0.f);
    const glm::vec4 map = stereo::viewUvMap(size.drawnWidth, size.drawnHeight, size.allocatedWidth, size.allocatedHeight);
    GL_Uniform4fFunc(14, map.x, map.y, map.z, map.w); // (SceneMap)
    GL_Uniform4fFunc(13, 1.f, 1.f, 0.f, 0.f); // (BloomMap: VR_PostProcessBloom's when an eye's glow is added)
}

extern "C" unsigned VR_PostProcessTarget()
{
    if(!stereo::renderingEye)
    {
        return 0;
    }
    return stereo::resampling ? stereo::resample.fbo : stereo::targetFbo;
}

// The projection's depth terms for near plane n.
static void setProjectionNear(float matrix[16], const float n)
{
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

// Replaces the symmetric projection with the eye's asymmetric one. Ironwail's projection maps
// Quake view space (x forward, y left, z up) to clip space; only the terms producing clip x/y
// change, the (reversed-Z) depth terms are kept.
extern "C" void VR_OverrideProjection(float matrix[16])
{
    if(!stereo::renderingEye)
    {
        if(const float flatNear = VR_FlatNearPlane(); flatNear > 0.f)
        {
            setProjectionNear(matrix, flatNear);
        }
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
        l = za::tan(fov.left);
        r = za::tan(fov.right);
        u = za::tan(fov.up);
        d = za::tan(fov.down);
    }

    matrix[1 * 4 + 0] = -2.f / (r - l);   // clip x from -y (right)
    matrix[0 * 4 + 0] = -(r + l) / (r - l); // off-axis shift, times depth (x)
    matrix[2 * 4 + 1] = 2.f / (u - d);    // clip y from z (up)
    matrix[0 * 4 + 1] = -(u + d) / (u - d);

    // Hands, weapons and the body come much closer to the eyes than to a monitor's view: the
    // desktop's near plane (up to 4 units, 12 cm) cut them open, and 1 unit (3 cm) still cut a shouldered gun's
    // stock and receiver. Reversed Z in a float depth buffer keeps the precision far away at any near plane; a
    // 24-bit one loses it below 1 unit (held items are never cut anyway: vr_nearclip_held).
    const unsigned depth = (stereo::spectatorView ? stereo::spectatorTargets : stereo::eyeTargets).depth;
    setProjectionNear(matrix, CLAMP(depth == GL_DEPTH32F_STENCIL8 ? 0.02f : 1.f, vr_nearclip.value, 4.f));
}
