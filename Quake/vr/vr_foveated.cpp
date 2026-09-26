// vr_foveated.cpp -- see vr_foveated.hpp.

#include "vr_foveated.hpp"
#include "vr_cvars.hpp"
#include "vr_gfx.hpp"
#include "vr_upscale.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iterator>
#include <vector>

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
};
RateImage images[2];

// The angles (degrees from the view axis) within which the shading is full rate, and 2x2; 4x4 beyond.
void radii(float& inner, float& outer)
{
    // conservative, balanced, aggressive
    constexpr float presets[3][2] = {{45.f, 60.f}, {35.f, 50.f}, {25.f, 40.f}};
    const int preset = std::clamp(static_cast<int>(vr_foveated.value), 1, 3) - 1;
    inner = vr_foveated_inner.value > 0.f ? vr_foveated_inner.value : presets[preset][0];
    outer = vr_foveated_outer.value > 0.f ? vr_foveated_outer.value : presets[preset][1];
    inner = std::clamp(inner, 5.f, 89.f);
    outer = std::clamp(outer, inner, 89.f);
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
    if(img.texture && img.width == width && img.height == height && sameLens(img.lens, lens) && img.inner == inner &&
        img.outer == outer)
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

    // Each tile's rate from its point nearest the lens centre (so no pixel is shaded coarser than its angle asks).
    const float tanInner = std::tan(glm::radians(inner));
    const float tanOuter = std::tan(glm::radians(outer));
    std::vector<unsigned char> texels(static_cast<std::size_t>(tilesX) * tilesY);
    double pixels[3] = {0.0, 0.0, 0.0};
    for(int ty = 0; ty < tilesY; ty++)
    {
        for(int tx = 0; tx < tilesX; tx++)
        {
            const float x0 = static_cast<float>(tx * tileWidth), y0 = static_cast<float>(ty * tileHeight);
            const float x1 = std::min(x0 + tileWidth, static_cast<float>(width));
            const float y1 = std::min(y0 + tileHeight, static_cast<float>(height));
            const glm::vec2 nearest{std::clamp(lens.centre.x, x0, x1), std::clamp(lens.centre.y, y0, y1)};
            const float t = glm::length((nearest - lens.centre) / lens.pixelsPerTangent);
            const unsigned char rate = t < tanInner ? 0 : t < tanOuter ? 1 : 2;
            texels[static_cast<std::size_t>(ty) * tilesX + tx] = rate;
            pixels[rate] += static_cast<double>(x1 - x0) * (y1 - y0);
        }
    }
    GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D, img.texture);
    GLint alignment = 4;
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &alignment);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, tilesX, tilesY, GL_RED_INTEGER, GL_UNSIGNED_BYTE, texels.data());
    glPixelStorei(GL_UNPACK_ALIGNMENT, alignment);

    img.width = width;
    img.height = height;
    img.tilesX = tilesX;
    img.tilesY = tilesY;
    img.lens = lens;
    img.inner = inner;
    img.outer = outer;

    if(vr_foveated_debug.value != 0.f)
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

constexpr const char* debugVs = R"(#version 430
void main()
{
    ivec2 v = ivec2(gl_VertexID & 1, gl_VertexID >> 1);
    gl_Position = vec4(vec2(v) * 4.0 - 1.0, 0.0, 1.0);
}
)";

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
        found = name && !std::strcmp(name, "GL_NV_shading_rate_image");
    }
    if(found)
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
    const RateImage& img = rateImage(eye, width, height);
    bindShadingRateImage(img.texture);
    shadingRateImagePalette(0, 0, static_cast<GLsizei>(std::size(palette)), palette);

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
    GLint bound = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &bound);
    setEnabled(bound != 0 && (static_cast<GLuint>(bound) == sceneFbos[0] || static_cast<GLuint>(bound) == sceneFbos[1]));
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

} // namespace qvr::foveated
