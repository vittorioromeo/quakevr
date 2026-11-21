// vr_foveated.cpp -- see vr_foveated.hpp.

#include "vr_foveated.hpp"
#include "vr_cvars.hpp"
#include "vr_tunables.hpp"
#include "vr_gfx.hpp"
#include "vr_glutil.hpp"
#include "vr_profile.hpp"
#include "vr_upscale.hpp"
#include "vr_main.hpp"
#include "vr_testkit.hpp"
#include "vr_api_render.h"
#include "vr_api.h"

#if defined(SDL_FRAMEWORK) || defined(NO_SDL_CONFIG)
#include <SDL2/SDL.h>
#else
#include "SDL.h"
#endif

#include "Zancle/Base/GetArraySize.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Base/Strcmp.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Tan.hpp"


#ifndef GL_SHADING_RATE_IMAGE_NV
#define GL_SHADING_RATE_IMAGE_NV 0x9563
#endif
#ifndef GL_SHADING_RATE_1_INVOCATION_PER_PIXEL_NV
#define GL_SHADING_RATE_1_INVOCATION_PER_PIXEL_NV 0x9565
#define GL_SHADING_RATE_1_INVOCATION_PER_2X2_PIXELS_NV 0x9568
#define GL_SHADING_RATE_1_INVOCATION_PER_4X4_PIXELS_NV 0x956B
#endif
#ifndef GL_SHADING_RATE_IMAGE_TEXEL_WIDTH_NV
#define GL_SHADING_RATE_IMAGE_TEXEL_WIDTH_NV 0x955C
#define GL_SHADING_RATE_IMAGE_TEXEL_HEIGHT_NV 0x955D
#endif
#ifndef GL_NUM_EXTENSIONS
#define GL_NUM_EXTENSIONS 0x821D
#endif
#ifndef GL_RED_INTEGER
#define GL_RED_INTEGER 0x8D94
#endif

namespace qvr::foveated
{
namespace
{

using BindShadingRateImageFn = void(APIENTRY*)(GLuint texture);
using ShadingRateImagePaletteFn = void(APIENTRY*)(GLuint viewport, GLuint first, GLsizei count, const GLenum* rates);
BindShadingRateImageFn bindShadingRateImage = nullptr;
ShadingRateImagePaletteFn shadingRateImagePalette = nullptr;

int support = -1; // unknown
int tileWidth = 16;
int tileHeight = 16;

// The palette: a texel's value is an index into it.
constexpr GLenum palette[] = {GL_SHADING_RATE_1_INVOCATION_PER_PIXEL_NV, GL_SHADING_RATE_1_INVOCATION_PER_2X2_PIXELS_NV,
    GL_SHADING_RATE_1_INVOCATION_PER_4X4_PIXELS_NV};

// Each eye's shading-rate image and what it was made for.
struct RateImage
{
    GLuint texture{0};
    int width{0}, height{0}; // the scene's, in pixels
    int tilesX{0}, tilesY{0};
    upscale::Lens lens;
    float inner{0.f}, outer{0.f};
    glm::vec2 centre{0.f}; // the full-rate centre (image pixels): the lens centre, or the gaze's (vr_foveated_gaze)
};
RateImage images[2];
za::Vector<unsigned char> rateTexels; // rateImage's texels before the upload (kept: remade as the gaze moves)

// The full-rate centre of the eye's width x height image (pixels; GL: row 0 the bottom): where the gaze meets it
// (vr_foveated_gaze, while the eye tracker tracks it; `gazed` true), the middle of its tile so the rates are remade
// only when it moves a tile; else (no tracker, a blink, a gaze off the image) the lens centre.
glm::vec2 fovealCentre(int eye, const upscale::Lens& lens, int width, int height, bool& gazed)
{
    gazed = false;
    const FrameState& fs = frameState();
    if(vr_foveated_gaze.value == 0.f || !fs.gaze.valid)
    {
        return lens.centre;
    }
    const glm::vec3 d =
        glm::inverse(fs.eyes[eye & 1].pose.orientation) * (fs.gaze.orientation * glm::vec3{0.f, 0.f, -1.f});
    if(d.z > -0.2f) // (over 78 degrees off the eye's axis)
    {
        return lens.centre;
    }
    const glm::vec2 p = lens.centre + glm::vec2{d.x / -d.z, d.y / -d.z} * lens.pixelsPerTangent;
    if(p.x < 0.f || p.y < 0.f || p.x >= static_cast<float>(width) || p.y >= static_cast<float>(height))
    {
        return lens.centre;
    }
    gazed = true;
    return {(static_cast<float>(static_cast<int>(p.x) / tileWidth) + 0.5f) * static_cast<float>(tileWidth),
        (static_cast<float>(static_cast<int>(p.y) / tileHeight) + 0.5f) * static_cast<float>(tileHeight)};
}

// Each tw x th tile's rate (index into the palette) in a width x height image, from its point nearest `centre`: full
// rate within `inner` degrees of the view through `centre`, 2x2 to `outer`, 4x4 beyond. `pixels`: each rate's pixels.
// At the lens centre, the angle from the view axis (its tangent's length); elsewhere (the gaze's), the angle between
// the two directions.
void computeRates(int width, int height, int tw, int th, const upscale::Lens& lens, glm::vec2 centre, float inner,
    float outer, unsigned char* texels, double pixels[3])
{
    const int tilesX = (width + tw - 1) / tw;
    const int tilesY = (height + th - 1) / th;
    const float tanInner = za::tan(glm::radians(inner));
    const float tanOuter = za::tan(glm::radians(outer));
    const float cosInner = za::cos(glm::radians(inner));
    const float cosOuter = za::cos(glm::radians(outer));
    const bool atLens = centre == lens.centre;
    const glm::vec3 axis = glm::normalize(glm::vec3{(centre - lens.centre) / lens.pixelsPerTangent, 1.f});
    pixels[0] = pixels[1] = pixels[2] = 0.0;
    for(int ty = 0; ty < tilesY; ty++)
    {
        for(int tx = 0; tx < tilesX; tx++)
        {
            const float x0 = static_cast<float>(tx * tw), y0 = static_cast<float>(ty * th);
            const float x1 = za::min(x0 + tw, static_cast<float>(width));
            const float y1 = za::min(y0 + th, static_cast<float>(height));
            const glm::vec2 nearest{za::clamp(centre.x, x0, x1), za::clamp(centre.y, y0, y1)};
            unsigned char rate = 0;
            if(atLens)
            {
                const float t = glm::length((nearest - lens.centre) / lens.pixelsPerTangent);
                rate = t < tanInner ? 0 : t < tanOuter ? 1 : 2;
            }
            else
            {
                const float c =
                    glm::dot(axis, glm::normalize(glm::vec3{(nearest - lens.centre) / lens.pixelsPerTangent, 1.f}));
                rate = c > cosInner ? 0 : c > cosOuter ? 1 : 2;
            }
            texels[static_cast<za::SizeT>(ty) * tilesX + tx] = rate;
            pixels[rate] += static_cast<double>(x1 - x0) * (y1 - y0);
        }
    }
}

// The angles (degrees from the view axis) within which the shading is full rate, and 2x2; 4x4 beyond.
void radii(float& inner, float& outer)
{
    // conservative, balanced, aggressive
    constexpr float presets[3][2] = {{45.f, 60.f}, {35.f, 50.f}, {25.f, 40.f}};
    const int preset = za::clamp(static_cast<int>(vr_foveated.value), 1, 3) - 1;
    inner = vr_foveated_inner.value > 0.f ? vr_foveated_inner.value : presets[preset][0];
    outer = vr_foveated_outer.value > 0.f ? vr_foveated_outer.value : presets[preset][1];
    inner = za::clamp(inner, 5.f, 89.f);
    outer = za::clamp(outer, inner, 89.f);
}

[[nodiscard]] bool sameLens(const upscale::Lens& a, const upscale::Lens& b)
{
    return a.centre == b.centre && a.pixelsPerTangent == b.pixelsPerTangent;
}

// The eye's image for a width x height scene, made again when the size, the field of view or the radii change.
RateImage& rateImage(int eye, int width, int height)
{
    RateImage& img = images[eye & 1];
    const upscale::Lens lens = upscale::lens(eye, width, height);
    float inner = 0.f, outer = 0.f;
    radii(inner, outer);
    bool gazed = false;
    const glm::vec2 centre = fovealCentre(eye, lens, width, height, gazed);
    const bool sameSetup = img.texture && img.width == width && img.height == height && sameLens(img.lens, lens) &&
                           img.inner == inner && img.outer == outer;
    if(sameSetup && img.centre == centre)
    {
        return img;
    }

    const int tilesX = (width + tileWidth - 1) / tileWidth;
    const int tilesY = (height + tileHeight - 1) / tileHeight;
    if(!img.texture || img.tilesX != tilesX || img.tilesY != tilesY)
    {
        if(img.texture)
        {
            GL_DeleteNativeTexture(img.texture);
        }
        glGenTextures(1, &img.texture);
        GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D, img.texture);
        GL_TexStorage2DFunc(GL_TEXTURE_2D, 1, GL_R8UI, tilesX, tilesY);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
        GL_ObjectLabelFunc(GL_TEXTURE, img.texture, -1, eye ? "vr shading rates R" : "vr shading rates L");
    }

    // Each tile's rate from its point nearest the full-rate centre (so no pixel is shaded coarser than its angle asks).
    // (Remade each tile the gaze moves: the texels' buffer kept.)
    rateTexels.resize(static_cast<za::SizeT>(tilesX) * tilesY);
    unsigned char* texels = rateTexels.data();
    double pixels[3] = {0.0, 0.0, 0.0};
    computeRates(width, height, tileWidth, tileHeight, lens, centre, inner, outer, texels, pixels);
    GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D, img.texture);
    // (no glGet of the alignment: each tile the gaze moves would read GL state back; 4 is the engine's, as vr_detail and vr_water)
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, tilesX, tilesY, GL_RED_INTEGER, GL_UNSIGNED_BYTE, texels);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

    img.width = width;
    img.height = height;
    img.tilesX = tilesX;
    img.tilesY = tilesY;
    img.lens = lens;
    img.inner = inner;
    img.outer = outer;
    img.centre = centre;

    if(vr_foveated_debug.value != 0.f && !sameSetup) // (not each tile the gaze moves)
    {
        const double all = static_cast<double>(width) * height;
        Con_Printf("VR: foveated %c eye %dx%d, %dx%d tiles, full rate within %.0f deg, 2x2 to %.0f: 1x1 %.0f%%, 2x2 "
                   "%.0f%%, 4x4 %.0f%% of the pixels; %.0f%% of the shading\n",
            eye ? 'R' : 'L', width, height, tileWidth, tileHeight, inner, outer, 100.0 * pixels[0] / all,
            100.0 * pixels[1] / all, 100.0 * pixels[2] / all,
            100.0 * (pixels[0] + pixels[1] / 4.0 + pixels[2] / 16.0) / all);
    }
    return img;
}

// While the scene renders: the framebuffer binding, watched, so the rates apply to the eye's scene framebuffers only.
using BindFramebufferFn = decltype(GL_BindFramebufferFunc);
BindFramebufferFn realBindFramebuffer = nullptr;
GLuint sceneFbos[2] = {0, 0};
// Single-pass stereo's multiview pass (beginSceneLayered): both eyes' rate images as the layers of one array, each view
// shaded by its own layer.
struct LayeredRates
{
    GLuint texture = 0;
    int tilesX = 0, tilesY = 0;
};
LayeredRates layered;
using CopyImageFn = void(APIENTRY*)(GLuint, GLenum, GLint, GLint, GLint, GLint, GLuint, GLenum, GLint, GLint, GLint,
    GLint, GLsizei, GLsizei, GLsizei);
CopyImageFn copyImage = nullptr;
bool active = false; // between beginScene and endScene
bool enabled = false; // GL_SHADING_RATE_IMAGE_NV

void setEnabled(bool on)
{
    if(on != enabled)
    {
        enabled = on;
        if(on)
        {
            glEnable(GL_SHADING_RATE_IMAGE_NV);
        }
        else
        {
            glDisable(GL_SHADING_RATE_IMAGE_NV);
        }
    }
}

void APIENTRY watchedBindFramebuffer(GLenum target, GLuint framebuffer)
{
    realBindFramebuffer(target, framebuffer);
    if(target != GL_READ_FRAMEBUFFER)
    {
        setEnabled(framebuffer != 0 && (framebuffer == sceneFbos[0] || framebuffer == sceneFbos[1]));
    }
}

GLuint debugProgram = 0;
bool debugFailed = false;

constexpr const char* debugVs = gfx::fullscreenTriangleVs;

constexpr const char* debugFs = R"(#version 430
layout(binding = 0) uniform usampler2D Rates;
layout(location = 0) uniform vec4 Map;  // scene pixels per image pixel (xy), 1 / the tile's size (zw)
layout(location = 1) uniform vec4 Lens; // the projection centre (image pixels), 1 / pixels per tangent
layout(location = 2) uniform vec2 Ring; // vr_upscale's radius and the ring's half width, as tangents (x 0: none)
layout(location = 3) uniform int HasRates;
layout(location = 0) out vec4 Out;
void main()
{
    vec4 c = vec4(0.0);
    if(HasRates != 0)
    {
        ivec2 t = clamp(ivec2(gl_FragCoord.xy * Map.xy * Map.zw), ivec2(0), textureSize(Rates, 0) - 1);
        uint r = texelFetch(Rates, t, 0).r;
        if(r == 1u)
            c = vec4(1.0, 0.9, 0.0, 0.22);
        else if(r >= 2u)
            c = vec4(1.0, 0.15, 0.0, 0.35);
    }
    if(Ring.x > 0.0 && abs(length((gl_FragCoord.xy - Lens.xy) * Lens.zw) - Ring.x) < Ring.y)
        c = vec4(0.0, 1.0, 1.0, 0.8);
    if(c.a <= 0.0)
        discard;
    Out = c;
}
)";

} // namespace

bool supported()
{
    if(support >= 0)
    {
        return support != 0;
    }
    support = 0;
    GLint count = 0;
    glGetIntegerv(GL_NUM_EXTENSIONS, &count);
    bool found = false;
    for(GLint i = 0; i < count && !found; i++)
    {
        const auto* name = reinterpret_cast<const char*>(GL_GetStringiFunc(GL_EXTENSIONS, static_cast<GLuint>(i)));
        found = name && !ZA_STRCMP(name, "GL_NV_shading_rate_image");
    }
    if(found && !VR_GLESForm(VR_GLES_FORM_FOVEATION)) // (-glescompat: no shading-rate image on OpenGL ES)
    {
        bindShadingRateImage =
            reinterpret_cast<BindShadingRateImageFn>(SDL_GL_GetProcAddress("glBindShadingRateImageNV"));
        shadingRateImagePalette =
            reinterpret_cast<ShadingRateImagePaletteFn>(SDL_GL_GetProcAddress("glShadingRateImagePaletteNV"));
        GLint w = 0, h = 0;
        glGetIntegerv(GL_SHADING_RATE_IMAGE_TEXEL_WIDTH_NV, &w);
        glGetIntegerv(GL_SHADING_RATE_IMAGE_TEXEL_HEIGHT_NV, &h);
        if(bindShadingRateImage && shadingRateImagePalette && w > 0 && h > 0)
        {
            tileWidth = w;
            tileHeight = h;
            support = 1;
        }
    }
    Con_Printf("VR: foveated rendering (GL_NV_shading_rate_image) %s", support ? "available" : "not available\n");
    if(support)
    {
        Con_Printf(", %dx%d pixel tiles\n", tileWidth, tileHeight);
    }
    return support != 0;
}

void beginScene(int eye, int width, int height)
{
    endScene(); // one left open (an error during the last eye's scene)
    if(vr_foveated.value < 1.f || r_refdef.scale != 1 || width <= 0 || height <= 0 || !supported())
    {
        return;
    }
    QVR_PROFILE("foveation");
    const RateImage& img = rateImage(eye, width, height);
    bindShadingRateImage(img.texture);
    shadingRateImagePalette(0, 0, static_cast<GLsizei>(za::getArraySize(palette)), palette);

    if(GL_NeedsSceneEffects())
    {
        sceneFbos[0] = framebufs.scene.fbo;
        sceneFbos[1] = framebufs.oit.fbo_scene;
    }
    else
    {
        sceneFbos[0] = framebufs.composite.fbo;
        sceneFbos[1] = framebufs.oit.fbo_composite;
    }
    if(GL_BindFramebufferFunc != watchedBindFramebuffer)
    {
        realBindFramebuffer = GL_BindFramebufferFunc;
        GL_BindFramebufferFunc = watchedBindFramebuffer;
    }
    active = true;
    // What is bound: R_SetupGL's target, as R_RenderScene has just bound it (VR_DrawHiddenArea, after R_Clear). Not read
    // back: a glGet waits for the driver's thread, 0.2-0.4 ms of the CPU an eye (the profiler's "foveation").
    GLuint color = 0, depth = 0;
    int samples = 1;
    int viewport[4]{};
    const GLuint bound = VR_SceneTarget(&color, &depth, &samples, viewport);
    setEnabled(bound != 0 && (bound == sceneFbos[0] || bound == sceneFbos[1]));
}

void beginSceneLayered(unsigned fbo, int width, int height)
{
    endScene();
    if(vr_foveated.value < 1.f || r_refdef.scale != 1 || width <= 0 || height <= 0 || !supported())
    {
        return;
    }
    QVR_PROFILE("foveation");
    const RateImage& left = rateImage(0, width, height);
    const RateImage& right = rateImage(1, width, height);
    if(!copyImage)
    {
        copyImage = reinterpret_cast<CopyImageFn>(SDL_GL_GetProcAddress("glCopyImageSubData"));
    }
    if(!copyImage || left.tilesX != right.tilesX || left.tilesY != right.tilesY)
    {
        return;
    }
    if(!layered.texture || layered.tilesX != left.tilesX || layered.tilesY != left.tilesY)
    {
        if(layered.texture)
        {
            GL_DeleteNativeTexture(layered.texture);
        }
        glGenTextures(1, &layered.texture);
        GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D_ARRAY, layered.texture);
        GL_TexStorage3DFunc(GL_TEXTURE_2D_ARRAY, 1, GL_R8UI, left.tilesX, left.tilesY, 2);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D_ARRAY, 0);
        layered.tilesX = left.tilesX;
        layered.tilesY = left.tilesY;
    }
    // (each frame: a few hundred bytes; the eyes' images are remade when the lens or size changes)
    copyImage(left.texture, GL_TEXTURE_2D, 0, 0, 0, 0, layered.texture, GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, left.tilesX,
        left.tilesY, 1);
    copyImage(right.texture, GL_TEXTURE_2D, 0, 0, 0, 0, layered.texture, GL_TEXTURE_2D_ARRAY, 0, 0, 0, 1, left.tilesX,
        left.tilesY, 1);
    bindShadingRateImage(layered.texture);
    shadingRateImagePalette(0, 0, static_cast<GLsizei>(za::getArraySize(palette)), palette);
    sceneFbos[0] = sceneFbos[1] = fbo;
    if(GL_BindFramebufferFunc != watchedBindFramebuffer)
    {
        realBindFramebuffer = GL_BindFramebufferFunc;
        GL_BindFramebufferFunc = watchedBindFramebuffer;
    }
    active = true;
    setEnabled(true); // (the multiview framebuffer is bound)
}

void shadeBoundAsScene(bool on)
{
    setEnabled(on && active);
}

void endScene()
{
    if(!active)
    {
        return;
    }
    active = false;
    setEnabled(false);
    if(GL_BindFramebufferFunc == watchedBindFramebuffer)
    {
        GL_BindFramebufferFunc = realBindFramebuffer;
    }
    bindShadingRateImage(0);
}

void drawDebug(int eye, GLuint fbo, int width, int height, int sceneWidth, int sceneHeight)
{
    if(vr_foveated_debug.value == 0.f)
    {
        return;
    }
    if(!debugProgram && !debugFailed)
    {
        debugProgram = gfx::glProgram(debugVs, debugFs, "vr foveated debug");
        debugFailed = !debugProgram;
        gfx::ownDeferred(debugProgram, &debugFailed);
    }
    if(!debugProgram)
    {
        return;
    }
    const RateImage& img = images[eye & 1];
    const bool hasRates = vr_foveated.value >= 1.f && img.texture && img.width == sceneWidth && img.height == sceneHeight;
    const upscale::Lens lens = upscale::lens(eye, width, height);
    const bool ring = vr_upscale.value >= 1.f && upscale::radiusTangent() > 0.f;

    GL_BindFramebufferFunc(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, width, height);
    GL_SetState(GLS_BLEND_ALPHA | GLS_NO_ZTEST | GLS_NO_ZWRITE | GLS_CULL_NONE | GLS_ATTRIBS(0));
    GL_UseProgram(debugProgram);
    GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D, hasRates ? img.texture : 0);
    GL_Uniform4fFunc(0, static_cast<float>(sceneWidth) / width, static_cast<float>(sceneHeight) / height,
        1.f / tileWidth, 1.f / tileHeight);
    GL_Uniform4fFunc(1, lens.centre.x, lens.centre.y, 1.f / lens.pixelsPerTangent.x, 1.f / lens.pixelsPerTangent.y);
    GL_Uniform2fFunc(2, ring ? upscale::radiusTangent() : 0.f, 1.5f / lens.pixelsPerTangent.x);
    GL_Uniform1iFunc(3, hasRates ? 1 : 0);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}


// Its GL programs made now (gfx::precompilePrograms, at start-up), as their first use would: no compile in play.
void precompilePrograms()
{
    if(!debugProgram && !debugFailed)
    {
        debugProgram = gfx::glProgram(debugVs, debugFs, "vr foveated debug");
        debugFailed = !debugProgram;
        gfx::ownDeferred(debugProgram, &debugFailed);
    }
}

const char* gazeStatus()
{
    if(vr_foveated.value == 0.f)
    {
        return "Foveation: off";
    }
    if(support == 0)
    {
        return "Foveation: needs an NVIDIA graphics card";
    }
    const Backend* const be = backend();
    if(!be)
    {
        return "Foveation centre: shown in VR";
    }
    if(vr_foveated_gaze.value == 0.f)
    {
        return "Foveation centre: the lens centre (eye tracking off)";
    }
    switch(be->eyeTracking())
    {
        case EyeTracking::NotOffered: return "Foveation centre: the lens centre (no eye tracking in this headset)";
        case EyeTracking::NotAsked: return "Foveation centre: the lens centre (eye tracking from the next VR start)";
        case EyeTracking::NoTracker: return "Foveation centre: the lens centre (no eye tracker shared with the PC)";
        case EyeTracking::Ready: break;
    }
    return frameState().gaze.valid ? "Foveation centre: your eyes (eye tracking)"
                                   : "Foveation centre: the lens centre (your eyes not tracked now)";
}

void gazeTest_f()
{
    constexpr const char* name = "vr_foveated_gaze_test";
    testkit::Tally tally;
    // A 1600x1600 eye of 90 degrees (800 pixels a tangent), 16x16 tiles, full rate within 25 degrees, 2x2 to 40.
    constexpr int size = 1600, tile = 16, tiles = size / tile;
    upscale::Lens lens;
    lens.centre = {800.f, 800.f};
    lens.pixelsPerTangent = {800.f, 800.f};
    za::Vector<unsigned char> texels(static_cast<za::SizeT>(tiles) * tiles);
    double pixels[3];
    auto rateAt = [&](float x, float y) {
        return texels[static_cast<za::SizeT>(y / tile) * tiles + static_cast<za::SizeT>(x / tile)];
    };
    const float tan30 = za::tan(glm::radians(30.f)) * 800.f;

    // At the lens centre (no gaze): the rates as ever, from the view axis.
    computeRates(size, size, tile, tile, lens, lens.centre, 25.f, 40.f, texels.data(), pixels);
    tally.check(rateAt(800.f, 800.f) == 0, name, "lens centre: full rate in the middle");
    tally.check(rateAt(800.f + tan30, 800.f) == 1, name, "lens centre: 2x2 at 30 degrees");
    tally.check(rateAt(1.f, 1.f) == 2, name, "lens centre: 4x4 in the corner");
    const double centred = pixels[0];

    // The gaze 30 degrees right: full rate there, 2x2 in the middle (30 degrees off), 4x4 30 degrees left (60 off).
    computeRates(size, size, tile, tile, lens, {800.f + tan30, 800.f}, 25.f, 40.f, texels.data(), pixels);
    tally.check(rateAt(800.f + tan30, 800.f) == 0, name, "gaze right: full rate at the gaze");
    tally.check(rateAt(800.f, 800.f) == 1, name, "gaze right: 2x2 in the middle");
    tally.check(rateAt(800.f - tan30, 800.f) == 2, name, "gaze right: 4x4 opposite");
    tally.check(rateAt(800.f + tan30, 800.f + tan30) == 1 || rateAt(800.f + tan30, 800.f + tan30) == 0, name,
        "gaze right: not 4x4 just above it");
    Con_Printf("%s: full-rate pixels %.0f%% at the lens centre, %.0f%% with the gaze 30 degrees right\n", name,
        100.0 * centred / (size * size), 100.0 * pixels[0] / (size * size));

    // The live gaze (the mock's vr_mock_gaze "right up"): the centre where it meets the eye's image, within a tile.
    if(vr_mock_gaze.string[0])
    {
        char* end = nullptr;
        const float right = strtof(vr_mock_gaze.string, &end);
        const float up = end ? strtof(end, nullptr) : 0.f;
        const upscale::Lens live = upscale::lens(0, size, size);
        const glm::vec2 want =
            live.centre + glm::vec2{za::tan(glm::radians(right)), za::tan(glm::radians(up))} * live.pixelsPerTangent;
        const float was = vr_foveated_gaze.value;
        bool gazed = false;
        const Backend* const be = backend();
        const bool tracker = be && be->eyeTracking() == EyeTracking::Ready;
        Cvar_SetValueQuick(&vr_foveated_gaze, 0.f);
        const glm::vec2 off = fovealCentre(0, live, size, size, gazed);
        tally.check(!gazed && off == live.centre, name, "vr_foveated_gaze 0: the lens centre");
        Cvar_SetValueQuick(&vr_foveated_gaze, 1.f);
        const glm::vec2 on = fovealCentre(0, live, size, size, gazed);
        Cvar_SetValueQuick(&vr_foveated_gaze, was);
        Con_Printf("%s: mock gaze %s: centre %.0f %.0f (want %.0f %.0f; the lens's %.0f %.0f)\n", name,
            vr_mock_gaze.string, on.x, on.y, want.x, want.y, live.centre.x, live.centre.y);
        Con_Printf("%s: eye tracker %d, status \"%s\"\n", name, tracker ? 1 : 0, gazeStatus());
        if(tracker)
        {
            tally.check(gazed && za::abs(on.x - want.x) <= static_cast<float>(tileWidth) &&
                            za::abs(on.y - want.y) <= static_cast<float>(tileHeight),
                name, "mock gaze: the centre follows it");
            tally.check(strstr(gazeStatus(), "your eyes") != nullptr, name, "the menu line says the eyes");
        }
        else
        {
            // (vr_mock_eye_tracking 0: the runtime offers none; the same frame's foveation, nothing restarted.)
            tally.check(!gazed && on == live.centre, name, "no eye tracker: the lens centre, the gaze ignored");
            tally.check(strstr(gazeStatus(), "no eye tracking") != nullptr, name, "the menu line says the lens centre");
        }
    }
    tally.verdict(name);
}

} // namespace qvr::foveated
