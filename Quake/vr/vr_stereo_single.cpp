// vr_stereo_single.cpp -- see vr_stereo_single.hpp and docs/vr-port/STEREO.md.
//
// The frame (VR_RenderView): both eyes' V_SetupView first (captureEye keeps each eye's r_refdef and matrices), then the
// multiview pass (render: gl_rmain.c R_RenderStereoSingle) with eye 0's r_refdef current: R_SetupView with a frustum
// holding both eyes' (VR_StereoSingleFrustum), both eyes' PVS (r_world.c) and light clusters (gl_rlight.c), then the
// ported passes drawn into the 2-layer arrays through the multiview framebuffer, every program swapped for its multiview
// twin (gl_shaders.c). Then each eye's own V_RenderView as before (its V_SetupView skipped: done), whose scene target
// is its layer's texture views (R_SetupGL: VR_StereoSingleEyeTargets), not cleared (VR_StereoSingleSceneBegun), with
// the pass's lights and clusters (R_PushDlights), skipping what the pass drew.

#include "vr_stereo_single.hpp"

#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_foveated.hpp"
#include "vr_main.hpp"
#include "vr_portals.hpp"
#include "vr_stereo.hpp"

#include "Zancle/Base/Strcmp.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Tan.hpp"

#ifndef GL_NUM_EXTENSIONS
#define GL_NUM_EXTENSIONS 0x821D
#endif

namespace qvr::stereosingle
{
namespace
{

using MultiviewFn = void(APIENTRY*)(GLenum target, GLenum attachment, GLuint texture, GLint level, GLint baseView,
    GLsizei numViews);
using TextureViewFn = void(APIENTRY*)(GLuint texture, GLenum target, GLuint origTexture, GLenum format,
    GLuint minLevel, GLuint numLevels, GLuint minLayer, GLuint numLayers);
using StorageMultisampleFn = void(APIENTRY*)(GLenum target, GLsizei samples, GLenum format, GLsizei width,
    GLsizei height, GLsizei depth, GLboolean fixedLocations);

// What an eye's V_SetupView left (captureEye).
struct EyeView
{
    float origin[3] = {};
    float angles[3] = {};
    float fovX = 0.f, fovY = 0.f;
    float proj[16] = {};
    float view[16] = {};
    float viewProj[16] = {};
};

// The views' uniform block (vr_glsl.h QVR_STEREO_VIEWS, binding 5).
struct ViewsBlock
{
    float viewProj[2][16];
    float eyePos[2][4];
};

constexpr GLuint viewsBinding = 5;

struct State
{
    int support = -1; // -1 not checked yet, 0 no, 1 yes
    MultiviewFn multiview = nullptr;
    TextureViewFn textureView = nullptr;

    // The arrays (the eyes' scene size and formats) and their framebuffers: both layers (multiview); each layer's
    // texture views (colour, depth/stencil), which each eye renders on into as its own scene target (eyeTargets), with
    // their framebuffers: the scene, the OIT's (the eyes' accum and revealage textures with the layer's depth).
    GLuint color = 0, depth = 0;
    GLuint fboMulti = 0;
    GLuint viewColor[2] = {}, viewDepth[2] = {};
    GLuint fboEye[2] = {}, fboOit[2] = {};
    unsigned eyeGeneration = 0; // the eyes' targets' (stereo::eyeTargetsGeneration) the OIT framebuffers were made for
    int width = 0, height = 0;
    int samples = 1; // the eyes' MSAA (multisample arrays)
    int frameSamples = 1; // this frame's (wanted)
    unsigned colorFormat = 0, depthFormat = 0;
    bool targetsFailed = false;
    StorageMultisampleFn storageMultisample = nullptr; // (null: no MSAA arrays: two-pass with MSAA)
    GLuint clusters = 0; // the right eye's light clusters (gl_rlight.c's image's twin)
    bool parallel = false; // GL_KHR_parallel_shader_compile: the twins compiled in the background
    int compiling = 0;     // twins still compiling (the frames two-pass meanwhile)
    double compileStart = 0.0;

    // This frame.
    EyeView eyes[2];
    mplane_t frustum[4] = {}; // holding both eyes' (unionFrustum)
    bool captured = false;  // both eyes' views set up before the loop (captureEye)
    bool active = false;    // the pass ran: the eyes' views follow it
    bool inPass = false;    // R_RenderStereoSingle runs
    bool viewReady = false; // an eye's own V_RenderView runs (its V_SetupView done before the pass)
    bool scenePending[2] = {}; // the eye's scene not begun yet (its layer cleared and drawn in the pass: no clear)

    // vr_stereo_single_info: since the last call.
    int frames = 0, framesSingle = 0;
    const char* reason = "";
};
State state;

bool supported()
{
    if(state.support >= 0)
    {
        return state.support == 1;
    }
    state.support = 0;
    GLint count = 0;
    glGetIntegerv(GL_NUM_EXTENSIONS, &count);
    bool found = false, multisample = false, parallelKhr = false, parallelArb = false;
    for(GLint i = 0; i < count; i++)
    {
        const auto* name = reinterpret_cast<const char*>(GL_GetStringiFunc(GL_EXTENSIONS, static_cast<GLuint>(i)));
        found = found || (name && !ZA_STRCMP(name, "GL_OVR_multiview2"));
        multisample = multisample || (name && !ZA_STRCMP(name, "GL_EXT_multiview_texture_multisample"));
        parallelKhr = parallelKhr || (name && !ZA_STRCMP(name, "GL_KHR_parallel_shader_compile"));
        parallelArb = parallelArb || (name && !ZA_STRCMP(name, "GL_ARB_parallel_shader_compile"));
    }
    if(parallelKhr || parallelArb) // (the driver's choice of threads)
    {
        using MaxThreadsFn = void(APIENTRY*)(GLuint count);
        const auto maxThreads = reinterpret_cast<MaxThreadsFn>(
            SDL_GL_GetProcAddress(parallelKhr ? "glMaxShaderCompilerThreadsKHR" : "glMaxShaderCompilerThreadsARB"));
        if(maxThreads)
        {
            maxThreads(0xFFFFFFFFu);
            state.parallel = true;
        }
    }
    if(multisample)
    {
        state.storageMultisample =
            reinterpret_cast<StorageMultisampleFn>(SDL_GL_GetProcAddress("glTexStorage3DMultisample"));
    }
    if(found)
    {
        state.multiview = reinterpret_cast<MultiviewFn>(SDL_GL_GetProcAddress("glFramebufferTextureMultiviewOVR"));
        state.textureView = reinterpret_cast<TextureViewFn>(SDL_GL_GetProcAddress("glTextureView"));
        state.support = state.multiview && state.textureView ? 1 : 0;
    }
    Con_DPrintf("VR: single-pass stereo (GL_OVR_multiview2) %s\n", state.support ? "available" : "not available");
    return state.support == 1;
}

void deleteTargets()
{
    if(state.fboMulti)
    {
        GL_DeleteFramebuffersFunc(1, &state.fboMulti);
        GL_DeleteFramebuffersFunc(2, state.fboEye);
        GL_DeleteFramebuffersFunc(2, state.fboOit);
        for(int eye = 0; eye < 2; eye++)
        {
            GL_DeleteNativeTexture(state.viewColor[eye]);
            GL_DeleteNativeTexture(state.viewDepth[eye]);
            state.viewColor[eye] = state.viewDepth[eye] = state.fboEye[eye] = state.fboOit[eye] = 0;
        }
        GL_DeleteNativeTexture(state.color);
        GL_DeleteNativeTexture(state.depth);
    }
    state.fboMulti = state.color = state.depth = 0;
    state.width = state.height = 0;
}

// Layer `layer` of `array` as a texture of its own (glTextureView: the same memory, so what the pass drew stays
// compressed for what each eye draws after it).
GLuint makeLayerView(GLuint array, unsigned format, int samples, int layer)
{
    GLuint tex = 0;
    glGenTextures(1, &tex);
    const GLenum target = samples > 1 ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
    state.textureView(tex, target, array, format, 0, 1, static_cast<GLuint>(layer), 1);
    if(samples <= 1) // (as GL_CreateFBOAttachment's)
    {
        GL_BindNative(GL_TEXTURE0, target, tex);
        glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(target, GL_TEXTURE_MAX_LEVEL, 0);
        GL_BindNative(GL_TEXTURE0, target, 0);
    }
    return tex;
}

// A framebuffer of `colors` (count) and the depth/stencil `depth`, all `samples`' textures. False: not complete.
bool makeFbo(GLuint& fbo, const GLuint* colors, int count, GLuint depth, int samples)
{
    const GLenum target = samples > 1 ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
    GL_GenFramebuffersFunc(1, &fbo);
    GL_BindFramebufferFunc(GL_FRAMEBUFFER, fbo);
    GLenum buffers[2] = {};
    for(int i = 0; i < count; i++)
    {
        GL_FramebufferTexture2DFunc(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, target, colors[i], 0);
        buffers[i] = GL_COLOR_ATTACHMENT0 + i;
    }
    GL_DrawBuffersFunc(count, buffers);
    GL_FramebufferTexture2DFunc(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, target, depth, 0);
    return GL_CheckFramebufferStatusFunc(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
}

GLuint makeArray(unsigned format, int width, int height, int samples, const char* label)
{
    GLuint tex = 0;
    glGenTextures(1, &tex);
    if(samples > 1) // (the eyes' MSAA: GL_EXT_multiview_texture_multisample)
    {
        GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D_MULTISAMPLE_ARRAY, tex);
        state.storageMultisample(GL_TEXTURE_2D_MULTISAMPLE_ARRAY, samples, format, width, height, 2, GL_FALSE);
        GL_ObjectLabelFunc(GL_TEXTURE, tex, -1, label);
        GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D_MULTISAMPLE_ARRAY, 0);
        return tex;
    }
    GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D_ARRAY, tex);
    GL_TexStorage3DFunc(GL_TEXTURE_2D_ARRAY, 1, format, width, height, 2);
    GL_ObjectLabelFunc(GL_TEXTURE, tex, -1, label); // (vr_vram_report)
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_LEVEL, 0);
    GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D_ARRAY, 0);
    return tex;
}

// The arrays for the eyes' scene of `width` x `height` (their formats: the eyes' targets'). False: they could not be
// made complete (the frame renders two-pass).
bool ensureTargets(int width, int height, int samples)
{
    unsigned colorFormat = 0, depthFormat = 0;
    stereo::eyeSceneFormats(colorFormat, depthFormat);
    const unsigned generation = stereo::eyeTargetsGeneration();
    if(state.fboMulti && state.width == width && state.height == height && state.colorFormat == colorFormat &&
        state.depthFormat == depthFormat && state.samples == samples && state.eyeGeneration == generation)
    {
        return true;
    }
    if(state.targetsFailed && state.width == width && state.height == height && state.samples == samples &&
        state.eyeGeneration == generation)
    {
        return false;
    }
    state.eyeGeneration = generation;
    deleteTargets();
    state.width = width;
    state.height = height;
    state.samples = samples;
    state.colorFormat = colorFormat;
    state.depthFormat = depthFormat;
    state.color = makeArray(colorFormat, width, height, samples, "single-pass stereo colors");
    state.depth = makeArray(depthFormat, width, height, samples, "single-pass stereo depth/stencil");

    const GLuint bound = GL_BoundDrawFramebuffer();
    GL_GenFramebuffersFunc(1, &state.fboMulti);
    GL_BindFramebufferFunc(GL_FRAMEBUFFER, state.fboMulti);
    state.multiview(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, state.color, 0, 0, 2);
    state.multiview(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, state.depth, 0, 0, 2);
    bool complete = GL_CheckFramebufferStatusFunc(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    const glframebufs_t& eyes = stereo::eyeFramebufs();
    for(int eye = 0; eye < 2; eye++)
    {
        state.viewColor[eye] = makeLayerView(state.color, colorFormat, samples, eye);
        state.viewDepth[eye] = makeLayerView(state.depth, depthFormat, samples, eye);
        complete = makeFbo(state.fboEye[eye], &state.viewColor[eye], 1, state.viewDepth[eye], samples) && complete;
        complete = makeFbo(state.fboOit[eye], eyes.oit.mrt, 2, state.viewDepth[eye], samples) && complete;
    }
    GL_BindFramebufferFunc(GL_FRAMEBUFFER, bound);
    if(!complete)
    {
        Con_Printf("vr_stereo_single: the %dx%d arrays are not a complete framebuffer: two-pass\n", width, height);
        deleteTargets();
        state.width = width;
        state.height = height;
        state.targetsFailed = true;
        return false;
    }
    state.targetsFailed = false;
    return true;
}

// The frustum holding both eyes' (VR_StereoSingleFrustum): eye 0's axes; each side as wide as the widest eye's corner
// ray along it; its apex on eye 0's forward axis through the eyes' midpoint, moved back until both eyes are inside.
// It holds each eye's frustum (that eye's apex inside it, each of its edge rays within its sides): it never culls what
// an eye sees. False: no such frustum (an eye's corner at or behind the side: the eyes' own frusta stay).
bool unionFrustum(mplane_t* frustum)
{
    vec3_t forward, right, up;
    AngleVectors(state.eyes[0].angles, forward, right, up);
    float xmin = 1e9f, xmax = -1e9f, ymin = 1e9f, ymax = -1e9f;
    for(int eye = 0; eye < 2; eye++)
    {
        vec3_t f, r, u;
        AngleVectors(state.eyes[eye].angles, f, r, u);
        const Fov& fov = frameState().eyes[eye].fov;
        const float tx[2] = {za::tan(fov.left), za::tan(fov.right)};
        const float ty[2] = {za::tan(fov.down), za::tan(fov.up)};
        for(const float x : tx)
        {
            for(const float y : ty)
            {
                vec3_t d;
                for(int i = 0; i < 3; i++)
                {
                    d[i] = f[i] + x * r[i] + y * u[i];
                }
                const float df = DotProduct(d, forward);
                if(df <= 1e-3f)
                {
                    return false;
                }
                xmin = za::min(xmin, DotProduct(d, right) / df);
                xmax = za::max(xmax, DotProduct(d, right) / df);
                ymin = za::min(ymin, DotProduct(d, up) / df);
                ymax = za::max(ymax, DotProduct(d, up) / df);
            }
        }
    }
    if(xmin >= -1e-3f || xmax <= 1e-3f || ymin >= -1e-3f || ymax <= 1e-3f)
    {
        return false;
    }

    vec3_t mid;
    for(int i = 0; i < 3; i++)
    {
        mid[i] = 0.5f * (state.eyes[0].origin[i] + state.eyes[1].origin[i]);
    }
    float back = 0.f;
    for(int eye = 0; eye < 2; eye++)
    {
        vec3_t p;
        VectorSubtract(state.eyes[eye].origin, mid, p);
        const float px = DotProduct(p, right), py = DotProduct(p, up), pz = DotProduct(p, forward);
        back = za::max(back, (px > 0.f ? px / xmax : px / xmin) - pz);
        back = za::max(back, (py > 0.f ? py / ymax : py / ymin) - pz);
    }
    back += 0.05f; // (a margin in units)
    vec3_t apex;
    VectorMA(mid, -back, forward, apex);

    // Inward normals, as R_SetFrustum's order: right, left, bottom, top.
    const auto plane = [&](mplane_t& out, const vec3_t a, float sa, const vec3_t b, float sb)
    {
        for(int i = 0; i < 3; i++)
        {
            out.normal[i] = sa * a[i] + sb * b[i];
        }
        VectorNormalize(out.normal);
        out.dist = DotProduct(out.normal, apex);
        out.type = PLANE_ANYZ;
        out.signbits = 0;
        for(int i = 0; i < 3; i++)
        {
            if(out.normal[i] < 0.f)
            {
                out.signbits |= 1 << i;
            }
        }
    };
    plane(frustum[0], forward, xmax, right, -1.f);
    plane(frustum[1], right, 1.f, forward, -xmin);
    plane(frustum[2], up, 1.f, forward, -ymin);
    plane(frustum[3], forward, ymax, up, -1.f);
    return true;
}

void info_f()
{
    int twins = 0, missing = 0;
    GL_MultiviewStats(&twins, &missing);
    Con_Printf("vr_stereo_single %g: multiview %s; %d of %d frames single-pass since the last time%s%s; %d multiview "
               "programs made, %d programs drawn in the pass without one; arrays %dx%d\n",
        static_cast<double>(vr_stereo_single.value), supported() ? "available" : "not available", state.framesSingle,
        state.frames, state.framesSingle < state.frames ? " (else: " : "",
        state.framesSingle < state.frames ? va("%s)", state.reason) : "", twins, missing, state.width, state.height);
    state.frames = state.framesSingle = 0;
}

} // namespace

void init()
{
    Cmd_AddCommand("vr_stereo_single_info", info_f);
}

bool wanted(int samples)
{
    ++state.frames;
    state.active = false;
    if(vr_stereo_single.value == 0.f)
    {
        state.reason = "off";
        return false;
    }
    if(!supported())
    {
        state.reason = "no GL_OVR_multiview2";
        return false;
    }
    if(samples > 1 && !state.storageMultisample)
    {
        state.reason = "MSAA without GL_EXT_multiview_texture_multisample";
        return false;
    }
    state.frameSamples = za::max(samples, 1);
    // The ported passes' twins begun now (turned on, or on at start; again after the programs are made again), not
    // mid-game the first time each draws: compiled by the driver's threads (about a second each for the world's), the
    // frames two-pass until they are all done.
    const double t0 = Sys_DoubleTime();
    if(const int made = GL_MultiviewPrepare(state.parallel ? 1 : 0))
    {
        state.compileStart = t0;
        Con_DPrintf("vr_stereo_single: %d multiview programs begun (%s, %.0f ms)\n", made,
            state.parallel ? "in the background" : "waited for", (Sys_DoubleTime() - t0) * 1000.0);
    }
    if(const int left = GL_MultiviewPending())
    {
        state.reason = "multiview programs compiling";
        state.compiling = left;
        return false;
    }
    if(state.compiling)
    {
        state.compiling = 0;
        Con_DPrintf("vr_stereo_single: multiview programs ready after %.1f s\n", Sys_DoubleTime() - state.compileStart);
    }
    return true;
}

void captureEye(int eye)
{
    EyeView& v = state.eyes[eye];
    VectorCopy(r_refdef.vieworg, v.origin);
    VectorCopy(r_refdef.viewangles, v.angles);
    v.fovX = r_refdef.fov_x;
    v.fovY = r_refdef.fov_y;
    R_StereoViewMatrices(v.proj, v.view, v.viewProj);
    state.captured = true;
}

bool render(int width, int height)
{
    if(!unionFrustum(state.frustum))
    {
        state.reason = "the eyes' frusta";
        return false;
    }
    // A teleporter's view in sight (either eye): two-pass. Each eye's views through the gates are drawn before its
    // scene, which hides what is behind the gates it shows (the frame data's gates: VR_PortalFrameData); the pass would
    // draw that before any eye's are known. (portals::update again in each eye's renderPortals.)
    for(int eye = 0; eye < 2; eye++)
    {
        portals::update(state.eyes[eye].origin, state.eyes[eye].angles, 0);
        if(portals::viewCount(0) > 0)
        {
            state.reason = "a teleporter's view";
            return false;
        }
    }
    if(!ensureTargets(width, height, state.frameSamples))
    {
        state.reason = "no arrays";
        return false;
    }
    // Eye 0's view current (its projection: VR_OverrideProjection), as its own scene would have it.
    stereo::setCurrentEye(0);
    VectorCopy(state.eyes[0].origin, r_refdef.vieworg);
    VectorCopy(state.eyes[0].angles, r_refdef.viewangles);
    r_refdef.fov_x = state.eyes[0].fovX;
    r_refdef.fov_y = state.eyes[0].fovY;

    R_AliasEyeCacheBegin(0); // (each model set up once here: no second eye to keep it for)
    state.inPass = true;
    R_RenderStereoSingle();
    state.inPass = false;
    state.active = true;
    state.scenePending[0] = state.scenePending[1] = true;
    ++state.framesSingle;
    return true;
}

void beginEye(int eye)
{
    if(!state.captured) // (captured, the pass not run: the eyes as before, their views not set up again)
    {
        return;
    }
    const EyeView& v = state.eyes[eye];
    VectorCopy(v.origin, r_refdef.vieworg);
    VectorCopy(v.angles, r_refdef.viewangles);
    r_refdef.fov_x = v.fovX;
    r_refdef.fov_y = v.fovY;
    state.viewReady = true;
}

void endEye()
{
    state.viewReady = false;
}

void endFrame()
{
    state.active = false;
    state.captured = false;
    state.viewReady = false;
    state.scenePending[0] = state.scenePending[1] = false;
}

} // namespace qvr::stereosingle

using namespace qvr;

extern "C" int VR_StereoSinglePhase()
{
    if(stereosingle::state.inPass)
    {
        return 1;
    }
    const stereosingle::State& s = stereosingle::state;
    return s.active && s.viewReady && !VR_PortalDrawing() && !stereo::isSpectator() ? 2 : 0;
}

extern "C" int VR_StereoSingleViewReady()
{
    return stereosingle::state.viewReady;
}

extern "C" void VR_StereoSingleFrustum(mplane_t* frustum)
{
    memcpy(frustum, stereosingle::state.frustum, sizeof(stereosingle::state.frustum));
}

extern "C" void VR_StereoSingleEyeOrigin(int eye, float out[3])
{
    VectorCopy(stereosingle::state.eyes[eye & 1].origin, out);
}

extern "C" void VR_StereoSingleEyeMatrices(int eye, float proj[16], float view[16])
{
    const stereosingle::EyeView& v = stereosingle::state.eyes[eye & 1];
    memcpy(proj, v.proj, sizeof(v.proj));
    memcpy(view, v.view, sizeof(v.view));
}

extern "C" unsigned VR_StereoSingleClusterTexture()
{
    GLuint& tex = stereosingle::state.clusters;
    if(!tex)
    {
        glGenTextures(1, &tex);
        GL_BindNative(GL_TEXTURE0, GL_TEXTURE_3D, tex);
        GL_ObjectLabelFunc(GL_TEXTURE, tex, -1, "light clusters (right eye)");
        GL_TexImage3DFunc(GL_TEXTURE_3D, 0, GL_RG32UI, LIGHT_TILES_X, LIGHT_TILES_Y, LIGHT_TILES_Z, 0, GL_RG_INTEGER,
            GL_UNSIGNED_INT, nullptr);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAX_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        GL_BindNative(GL_TEXTURE0, GL_TEXTURE_3D, 0);
    }
    return tex;
}

extern "C" unsigned VR_StereoSingleEyeClusters(unsigned usual)
{
    return (stereo::eye() & 1) && stereosingle::state.clusters ? stereosingle::state.clusters : usual;
}

extern "C" unsigned VR_StereoSingleBeginScene()
{
    stereosingle::State& s = stereosingle::state;
    // Each layer cleared and its lens's hidden area drawn, as each eye's R_RenderScene does (R_Clear, VR_DrawHiddenArea).
    for(int eye = 0; eye < 2; eye++)
    {
        GL_BindFramebufferFunc(GL_FRAMEBUFFER, s.fboEye[eye]);
        glViewport(0, 0, s.width, s.height);
        R_Clear();
        stereo::drawHiddenAreaOf(eye);
    }
    GL_BindFramebufferFunc(GL_FRAMEBUFFER, s.fboMulti);
    glViewport(0, 0, s.width, s.height);

    stereosingle::ViewsBlock block;
    for(int eye = 0; eye < 2; eye++)
    {
        memcpy(block.viewProj[eye], s.eyes[eye].viewProj, sizeof(block.viewProj[eye]));
        block.eyePos[eye][0] = s.eyes[eye].origin[0];
        block.eyePos[eye][1] = s.eyes[eye].origin[1];
        block.eyePos[eye][2] = s.eyes[eye].origin[2];
        block.eyePos[eye][3] = 1.f;
    }
    GLuint buf = 0;
    GLbyte* ofs = nullptr;
    GL_Upload(GL_UNIFORM_BUFFER, &block, sizeof(block), &buf, &ofs);
    GL_BindBufferRange(GL_UNIFORM_BUFFER, stereosingle::viewsBinding, buf, reinterpret_cast<GLintptr>(ofs), sizeof(block));
    gl_multiview_draw = true;
    foveated::beginSceneLayered(s.fboMulti, s.width, s.height); // vr_foveated: each view its eye's rates
    return s.fboMulti;
}

extern "C" void VR_StereoSingleEndScene()
{
    gl_multiview_draw = false;
    foveated::endScene();
}

extern "C" void VR_StereoSingleEyeTargets()
{
    const stereosingle::State& s = stereosingle::state;
    const int eye = stereo::eye() & 1;
    if(VR_StereoSinglePhase() != 2 || !s.fboEye[eye])
    {
        return;
    }
    // The eye's targets with its layer's views as the scene's (MSAA, or a scene effect: the scene target; else the
    // composite, which the scene is then drawn into). Chosen here, as R_SetupGL chooses, from the eyes' own targets.
    framebufs = stereo::eyeFramebufs();
    if(GL_NeedsSceneEffects())
    {
        framebufs.scene.color_tex = s.viewColor[eye];
        framebufs.scene.depth_stencil_tex = s.viewDepth[eye];
        framebufs.scene.fbo = s.fboEye[eye];
        framebufs.oit.fbo_scene = s.fboOit[eye];
    }
    else
    {
        framebufs.composite.color_tex = s.viewColor[eye];
        framebufs.composite.depth_stencil_tex = s.viewDepth[eye];
        framebufs.composite.fbo = s.fboEye[eye];
        framebufs.oit.fbo_composite = s.fboOit[eye];
    }
}

extern "C" int VR_StereoSingleSceneBegun()
{
    stereosingle::State& s = stereosingle::state;
    const int eye = stereo::eye() & 1;
    if(VR_StereoSinglePhase() != 2 || !s.scenePending[eye])
    {
        return 0;
    }
    s.scenePending[eye] = false;
    foveated::beginScene(eye, vid.width, vid.height); // vr_foveated, as after VR_DrawHiddenArea
    return 1;
}

extern "C" int VR_StereoSingleTakesAlias(const entity_t* e)
{
    // The view entities (hands, weapons, the body) per eye: their near-plane handling is decided per eye
    // (VR_AliasNearEye); cl.viewent too (its gun offsets follow the eye's axes).
    return (static_cast<int>(vr_stereo_single_passes.value) & 2) && e != &cl.viewent && !VR_IsViewEntity(e);
}

extern "C" int VR_StereoSingleTakesBrush()
{
    return static_cast<int>(vr_stereo_single_passes.value) & 1;
}
