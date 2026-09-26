// vr_upscale.cpp -- see vr_upscale.hpp.
//
// FSR 1 (vr_upscale 1): EASU, a fragment pass over the lens circle's bounding box into a texture of the image's size,
// then a fragment pass over the whole image into the eye's image: RCAS on the EASU result within the circle, a
// bilinear tap of the rendered image outside it, the two blended over a thin band at the circle's edge.
// NIS (vr_upscale 2): NVScaler, a compute pass over the box's 32x24 blocks into the same texture (its sharpening is
// built in), then the same fragment pass copying it within the circle.
// Both headers are compiled unchanged: ffx_fsr1.h's GLSL path after a small stand-in for ffx_a.h (the types and helpers
// it uses, as ffx_a.h defines them for A_GLSL), NIS_Scaler.h's GLSL path after our declarations of the resources it
// names. The constants are made on the CPU: FsrEasuCon's and FsrRcasCon's formulas here, NIS_Config.h's
// NVScalerUpdateConfig and coefficient tables as they are.

#include "vr_upscale.hpp"
#include "vr_backend.hpp"
#include "vr_cvars.hpp"
#include "vr_main.hpp"

#include "nis/NIS_Config.h"

#include <algorithm>
#include <iterator>
#include <cmath>
#include <cstdint>
#include <string>

#ifndef GL_TEXTURE_FETCH_BARRIER_BIT
#define GL_TEXTURE_FETCH_BARRIER_BIT 0x00000008
#endif
#ifndef GL_COMPUTE_SHADER
#define GL_COMPUTE_SHADER 0x91B9
#endif

namespace qvr::upscale
{
namespace
{

// ---------------------------------------------------------------------------------------------------------------------
// Shaders

constexpr const char* fullscreenVs = R"(#version 430
void main()
{
    ivec2 v = ivec2(gl_VertexID & 1, gl_VertexID >> 1);
    gl_Position = vec4(vec2(v) * 4.0 - 1.0, 0.0, 1.0);
}
)";

// What ffx_fsr1.h needs of ffx_a.h (A_GPU, A_GLSL, 32-bit only): the same definitions, trimmed to those it uses.
constexpr const char* ffxA = R"(
#define A_GPU 1
#define A_GLSL 1
#define AP1 bool
#define AF1 float
#define AF2 vec2
#define AF3 vec3
#define AF4 vec4
#define AU1 uint
#define AU2 uvec2
#define AU3 uvec3
#define AU4 uvec4
#define ASU1 int
#define ASU2 ivec2
#define AF1_(a) AF1(a)
#define AF2_(a) AF2(a)
#define AF3_(a) AF3(a)
#define AF4_(a) AF4(a)
#define AU1_(a) AU1(a)
#define AU3_(a) AU3(a)
#define AF1_AU1(x) uintBitsToFloat(AU1(x))
#define AF2_AU2(x) uintBitsToFloat(AU2(x))
#define AF3_AU3(x) uintBitsToFloat(AU3(x))
#define AU1_AF1(x) floatBitsToUint(AF1(x))
#define AU3_AF3(x) floatBitsToUint(AF3(x))
#define AU1_AH2_AF2 packHalf2x16
#define A_STATIC
#define outAU4 out AU4
#define varAF2(x) AF2 x
#define initAF2(x, y) AF2(x, y)
#define A_INFP_F uintBitsToFloat(0x7f800000u)
AF1 ARcpF1(AF1 x) { return AF1_(1.0) / x; }
AF1 AExp2F1(AF1 x) { return exp2(x); }
AF1 AFractF1(AF1 x) { return fract(x); }
AF1 ASatF1(AF1 x) { return clamp(x, AF1_(0.0), AF1_(1.0)); }
AF3 ASatF3(AF3 x) { return clamp(x, AF3_(0.0), AF3_(1.0)); }
AF1 AMin3F1(AF1 x, AF1 y, AF1 z) { return min(x, min(y, z)); }
AF1 AMax3F1(AF1 x, AF1 y, AF1 z) { return max(x, max(y, z)); }
AF3 AMin3F3(AF3 x, AF3 y, AF3 z) { return min(x, min(y, z)); }
AF3 AMax3F3(AF3 x, AF3 y, AF3 z) { return max(x, max(y, z)); }
AF1 APrxLoRcpF1(AF1 a) { return AF1_AU1(AU1_(0x7ef07ebb) - AU1_AF1(a)); }
AF1 APrxLoRsqF1(AF1 a) { return AF1_AU1(AU1_(0x5f347d74) - (AU1_AF1(a) >> AU1_(1))); }
AF1 APrxMedRcpF1(AF1 a) { AF1 b = AF1_AU1(AU1_(0x7ef19fff) - AU1_AF1(a)); return b * (-b * a + AF1_(2.0)); }
AF3 APrxMedRcpF3(AF3 a) { AF3 b = AF3_AU3(AU3_(0x7ef19fff) - AU3_AF3(a)); return b * (-b * a + AF3_(2.0)); }
AF3 AGtZeroF3(AF3 m) { return ASatF3(m * AF3_(A_INFP_F)); }
)";

constexpr const char* const fsrHeader[] = {
#include "fsr1/ffx_fsr1.glsl.inc"
};

constexpr const char* const nisHeader[] = {
#include "nis/NIS_Scaler.glsl.inc"
};

// EASU into the upscaled texture, within the lens circle (Reach, a tangent: beyond it, nothing reads the pixel).
constexpr const char* easuPrelude = R"(#version 430
layout(binding = 0) uniform sampler2D Source;
layout(location = 0) uniform vec4 Con0; // FsrEasuCon's constants (floats' bits)
layout(location = 1) uniform vec4 Con1;
layout(location = 2) uniform vec4 Con2;
layout(location = 3) uniform vec4 Con3;
layout(location = 4) uniform vec4 Lens; // the projection centre (pixels), 1 / pixels per tangent
layout(location = 5) uniform float Reach; // <= 0: everywhere
layout(location = 0) out vec4 Out;
#define FSR_EASU_F 1
)";

constexpr const char* easuMain = R"(
AF4 FsrEasuRF(AF2 p) { return textureGather(Source, p, 0); }
AF4 FsrEasuGF(AF2 p) { return textureGather(Source, p, 1); }
AF4 FsrEasuBF(AF2 p) { return textureGather(Source, p, 2); }
void main()
{
    if(Reach > 0.0 && length((gl_FragCoord.xy - Lens.xy) * Lens.zw) > Reach)
    {
        Out = vec4(0.0);
        return;
    }
    AF3 c;
    FsrEasuF(c, AU2(gl_FragCoord.xy), floatBitsToUint(Con0), floatBitsToUint(Con1), floatBitsToUint(Con2),
        floatBitsToUint(Con3));
    Out = vec4(c, 1.0);
}
)";

// The eye's image: the upscaled texture within the circle (Mode 1: RCAS on it, 0: as it is), the rendered image's
// bilinear tap outside, blended between the tangents Radius.y and Radius.x.
constexpr const char* composePrelude = R"(#version 430
layout(binding = 0) uniform sampler2D Source;
layout(binding = 1) uniform sampler2D Up;
layout(location = 0) uniform vec4 Lens;
layout(location = 1) uniform vec2 Radius; // x <= 0: everywhere
layout(location = 2) uniform vec2 InvSize; // 1 / the image's size
layout(location = 3) uniform float RcasSharpness; // FsrRcasCon's con[0]
layout(location = 4) uniform int Mode;
layout(location = 0) out vec4 Out;
#define FSR_RCAS_F 1
)";

constexpr const char* composeMain = R"(
AF4 FsrRcasLoadF(ASU2 p) { return texelFetch(Up, clamp(p, ivec2(0), textureSize(Up, 0) - 1), 0); }
void FsrRcasInputF(inout AF1 r, inout AF1 g, inout AF1 b) {}
void main()
{
    float w = 1.0;
    if(Radius.x > 0.0)
        w = 1.0 - smoothstep(Radius.y, Radius.x, length((gl_FragCoord.xy - Lens.xy) * Lens.zw));
    vec3 c = vec3(0.0);
    if(w < 1.0)
        c = textureLod(Source, gl_FragCoord.xy * InvSize, 0.0).rgb;
    if(w > 0.0)
    {
        vec3 u;
        if(Mode == 1)
            FsrRcasF(u.r, u.g, u.b, AU2(gl_FragCoord.xy), AU4(floatBitsToUint(RcasSharpness), 0u, 0u, 0u));
        else
            u = texelFetch(Up, ivec2(gl_FragCoord.xy), 0).rgb;
        c = mix(c, u, w);
    }
    Out = vec4(c, 1.0);
}
)";

// NVScaler (NIS_Config.h's NVIDIA block sizes), over the blocks from BlockOffset.
constexpr const char* nisPrelude = R"(#version 430
#define NIS_SCALER 1
#define NIS_HDR_MODE 0
#define NIS_GLSL 1
#define NIS_BLOCK_WIDTH 32
#define NIS_BLOCK_HEIGHT 24
#define NIS_THREAD_GROUP_SIZE 128
layout(local_size_x = NIS_THREAD_GROUP_SIZE) in;
layout(binding = 0) uniform sampler2D in_texture;
layout(binding = 1) uniform sampler2D coef_scaler;
layout(binding = 2) uniform sampler2D coef_usm;
layout(binding = 7, rgba8) uniform writeonly image2D out_texture;
layout(location = 0) uniform float kDetectRatio;
layout(location = 1) uniform float kDetectThres;
layout(location = 2) uniform float kMinContrastRatio;
layout(location = 3) uniform float kRatioNorm;
layout(location = 4) uniform float kContrastBoost;
layout(location = 5) uniform float kEps;
layout(location = 6) uniform float kSharpStartY;
layout(location = 7) uniform float kSharpScaleY;
layout(location = 8) uniform float kSharpStrengthMin;
layout(location = 9) uniform float kSharpStrengthScale;
layout(location = 10) uniform float kSharpLimitMin;
layout(location = 11) uniform float kSharpLimitScale;
layout(location = 12) uniform float kScaleX;
layout(location = 13) uniform float kScaleY;
layout(location = 14) uniform float kDstNormX;
layout(location = 15) uniform float kDstNormY;
layout(location = 16) uniform float kSrcNormX;
layout(location = 17) uniform float kSrcNormY;
layout(location = 18) uniform int BlockOffsetX;
layout(location = 19) uniform int BlockOffsetY;
// The header's GLSL path is Vulkan's (a texture and a sampler combined where used): here the textures sample as they
// are (bilinear, clamped: the sampler it names).
#define sampler2D(texture, sampler) texture
)";

constexpr const char* nisMain = R"(
void main()
{
    NVScaler(gl_WorkGroupID.xy + uvec2(BlockOffsetX, BlockOffsetY), gl_LocalInvocationID.x);
}
)";

[[nodiscard]] GLuint compileShader(GLenum type, const std::string& source, const char* name)
{
    const GLuint shader = GL_CreateShaderFunc(type);
    const char* text = source.c_str();
    GL_ShaderSourceFunc(shader, 1, &text, nullptr);
    GL_CompileShaderFunc(shader);
    GLint ok = 0;
    GL_GetShaderivFunc(shader, GL_COMPILE_STATUS, &ok);
    if(!ok)
    {
        char log[2048];
        GL_GetShaderInfoLogFunc(shader, sizeof(log), nullptr, log);
        Con_Warning("VR: %s shader failed to compile:\n%s\n", name, log);
        GL_DeleteShaderFunc(shader);
        return 0;
    }
    return shader;
}

// A program of a vertex and a fragment shader, or of a compute shader (vertex null). 0 if it fails.
[[nodiscard]] GLuint buildProgram(const char* vertex, GLenum type, const std::string& source, const char* name)
{
    const GLuint vs = vertex ? compileShader(GL_VERTEX_SHADER, vertex, name) : 0;
    const GLuint s = compileShader(type, source, name);
    if(!s || (vertex && !vs))
    {
        if(vs)
        {
            GL_DeleteShaderFunc(vs);
        }
        if(s)
        {
            GL_DeleteShaderFunc(s);
        }
        return 0;
    }
    const GLuint p = GL_CreateProgramFunc();
    if(vs)
    {
        GL_AttachShaderFunc(p, vs);
    }
    GL_AttachShaderFunc(p, s);
    GL_LinkProgramFunc(p);
    if(vs)
    {
        GL_DeleteShaderFunc(vs);
    }
    GL_DeleteShaderFunc(s);
    GLint ok = 0;
    GL_GetProgramivFunc(p, GL_LINK_STATUS, &ok);
    if(!ok)
    {
        char log[2048];
        GL_GetProgramInfoLogFunc(p, sizeof(log), nullptr, log);
        Con_Warning("VR: %s shader failed to link:\n%s\n", name, log);
        GL_DeleteProgramFunc(p);
        return 0;
    }
    GL_ObjectLabelFunc(GL_PROGRAM, p, -1, name);
    return p;
}

template <std::size_t N>
[[nodiscard]] std::string joined(const char* const (&pieces)[N])
{
    std::string s;
    for(const char* p : pieces)
    {
        s += p;
    }
    return s;
}

struct Program
{
    GLuint id{0};
    bool failed{false};
};
Program easuProgram, composeProgram, nisProgram;

[[nodiscard]] GLuint program(Program& p, const char* vertex, GLenum type, const char* prelude, const std::string& header,
    const char* main, const char* name)
{
    if(!p.id && !p.failed)
    {
        const std::string source = std::string{prelude} + ffxA + header + main;
        p.id = buildProgram(vertex, type, source, name);
        p.failed = !p.id;
    }
    return p.id;
}

[[nodiscard]] GLuint easu()
{
    return program(easuProgram, fullscreenVs, GL_FRAGMENT_SHADER, easuPrelude, joined(fsrHeader), easuMain, "vr fsr easu");
}

[[nodiscard]] GLuint compose()
{
    return program(composeProgram, fullscreenVs, GL_FRAGMENT_SHADER, composePrelude, joined(fsrHeader), composeMain,
        "vr upscale compose");
}

[[nodiscard]] GLuint nis()
{
    if(!nisProgram.id && !nisProgram.failed)
    {
        const std::string source = std::string{nisPrelude} + joined(nisHeader) + nisMain;
        nisProgram.id = buildProgram(nullptr, GL_COMPUTE_SHADER, source, "vr nis");
        nisProgram.failed = !nisProgram.id;
    }
    return nisProgram.id;
}

// ---------------------------------------------------------------------------------------------------------------------
// Resources

// The upscaler's output, the image's size (RGBA8: what the eye images hold).
GLuint upTex = 0;
GLuint upFbo = 0;
int upWidth = 0;
int upHeight = 0;

void ensureUpTarget(int width, int height)
{
    if(upTex && upWidth == width && upHeight == height)
    {
        return;
    }
    if(upTex)
    {
        GL_DeleteFramebuffersFunc(1, &upFbo);
        GL_DeleteNativeTexture(upTex);
    }
    glGenTextures(1, &upTex);
    GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D, upTex);
    GL_TexStorage2DFunc(GL_TEXTURE_2D, 1, GL_RGBA8, width, height);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    GL_ObjectLabelFunc(GL_TEXTURE, upTex, -1, "vr upscaled eye");
    GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D, 0);
    GL_GenFramebuffersFunc(1, &upFbo);
    GL_BindFramebufferFunc(GL_FRAMEBUFFER, upFbo);
    GL_FramebufferTexture2DFunc(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, upTex, 0);
    upWidth = width;
    upHeight = height;
}

// NIS's coefficient tables (NIS_Config.h), as 2 x 64 RGBA32F textures: row = phase, 8 taps in two texels.
GLuint coefScaler = 0;
GLuint coefUsm = 0;

[[nodiscard]] GLuint coefficientTexture(const float (&table)[kPhaseCount][kFilterSize], const char* label)
{
    GLuint tex = 0;
    glGenTextures(1, &tex);
    GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D, tex);
    GL_TexStorage2DFunc(GL_TEXTURE_2D, 1, GL_RGBA32F, 2, static_cast<GLsizei>(kPhaseCount));
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 2, static_cast<GLsizei>(kPhaseCount), GL_RGBA, GL_FLOAT, table);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    GL_ObjectLabelFunc(GL_TEXTURE, tex, -1, label);
    GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D, 0);
    return tex;
}

// ---------------------------------------------------------------------------------------------------------------------

// FsrEasuCon (ffx_fsr1.h): viewport = the whole input.
void easuConstants(float inW, float inH, float outW, float outH, float con[16])
{
    const auto u = [](float f) { return f; }; // AU1_AF1: the shader takes the floats' bits
    con[0] = u(inW / outW);
    con[1] = u(inH / outH);
    con[2] = u(0.5f * inW / outW - 0.5f);
    con[3] = u(0.5f * inH / outH - 0.5f);
    con[4] = u(1.f / inW);
    con[5] = u(1.f / inH);
    con[6] = u(1.f / inW);
    con[7] = u(-1.f / inH);
    con[8] = u(-1.f / inW);
    con[9] = u(2.f / inH);
    con[10] = u(1.f / inW);
    con[11] = u(2.f / inH);
    con[12] = u(0.f);
    con[13] = u(4.f / inH);
    con[14] = con[15] = 0.f;
}

// vr_upscale_sharpness (0 none .. 1 most) as RCAS's stops of reduction (0 = RCAS's most): 1 -> 0, 0.5 -> 1, 0.1 -> 1.8.
[[nodiscard]] float rcasConstant(float sharpness)
{
    const float stops = 2.f * (1.f - std::clamp(sharpness, 0.f, 1.f));
    return std::exp2(-stops); // FsrRcasCon's con[0] (as its float)
}

[[nodiscard]] float sharpness()
{
    return std::clamp(vr_upscale_sharpness.value, 0.f, 1.f);
}

void blit(GLuint sourceFbo, int width, int height, GLuint target, int imageWidth, int imageHeight)
{
    GL_BindFramebufferFunc(GL_READ_FRAMEBUFFER, sourceFbo);
    GL_BindFramebufferFunc(GL_DRAW_FRAMEBUFFER, target);
    GL_BlitFramebufferFunc(0, 0, width, height, 0, 0, imageWidth, imageHeight, GL_COLOR_BUFFER_BIT, GL_LINEAR);
    GL_BindFramebufferFunc(GL_FRAMEBUFFER, target);
}

// Bilinear and clamped: EASU's gathers and NIS's taps at the edges, the bilinear outside the circle.
void sourceSampling(GLuint source)
{
    GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D, source);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

bool warned = false;

} // namespace

Lens lens(int eye, int width, int height)
{
    const Fov& fov = frameState().eyes[eye & 1].fov;
    const float l = std::tan(fov.left), r = std::tan(fov.right), u = std::tan(fov.up), d = std::tan(fov.down);
    Lens out;
    if(r - l <= 0.f || u - d <= 0.f)
    {
        out.centre = {0.5f * width, 0.5f * height};
        out.pixelsPerTangent = {0.5f * width, 0.5f * height};
        return out;
    }
    out.pixelsPerTangent = {width / (r - l), height / (u - d)};
    out.centre = {-l * out.pixelsPerTangent.x, -d * out.pixelsPerTangent.y}; // GL: row 0 is the bottom (down)
    return out;
}

bool sharpenAtNative()
{
    return vr_upscale_sharpen_native.value != 0.f && vr_upscale.value >= 1.f && sharpness() > 0.f;
}

float radiusTangent()
{
    const float degrees = vr_upscale_radius.value;
    if(degrees <= 0.f || degrees >= 89.f)
    {
        return 0.f;
    }
    return std::tan(glm::radians(degrees));
}

void resample(int eye, GLuint source, GLuint sourceFbo, int width, int height, GLuint target, int imageWidth,
    int imageHeight)
{
    int mode = std::clamp(static_cast<int>(vr_upscale.value), 0, 2);
    const bool upscaling = width < imageWidth && height < imageHeight;
    const bool native = width == imageWidth && height == imageHeight;
    if(!(upscaling && mode > 0) && !(native && sharpenAtNative()))
    {
        blit(sourceFbo, width, height, target, imageWidth, imageHeight);
        return;
    }

    if(native)
    {
        mode = 1; // sharpening alone: RCAS, whichever upscaler is chosen
    }
    NISConfig config{};
    // NIS's own slider at half ours: its 0.5 oversharpens (dark halos by text) where FSR's RCAS at our 0.5 doesn't.
    if(mode == 2 &&
        (!NVScalerUpdateConfig(config, 0.5f * sharpness(), 0, 0, width, height, width, height, 0, 0, imageWidth, imageHeight,
             imageWidth, imageHeight) ||
            !nis()))
    {
        mode = 1; // NIS scales by 1 to 2 per side only (or its shader failed): FSR instead
    }
    const GLuint composeId = compose();
    if(!composeId || (upscaling && mode == 1 && !easu()))
    {
        if(!warned)
        {
            warned = true;
            Con_Warning("VR: vr_upscale: the shaders failed; resampling bilinearly\n");
        }
        blit(sourceFbo, width, height, target, imageWidth, imageHeight);
        return;
    }

    // The circle (and a band of 8% of its radius fading to bilinear), and the box around it that the upscaler covers,
    // with room for RCAS's neighbours.
    const Lens l = lens(eye, imageWidth, imageHeight);
    const float radius = radiusTangent();
    const glm::vec2 invPpt = 1.f / l.pixelsPerTangent;
    const float margin = 3.f * std::max(invPpt.x, invPpt.y); // 3 pixels, as a tangent
    int x0 = 0, y0 = 0, x1 = imageWidth, y1 = imageHeight;
    if(radius > 0.f)
    {
        const glm::vec2 half = (radius + margin) * l.pixelsPerTangent;
        x0 = std::clamp(static_cast<int>(std::floor(l.centre.x - half.x)), 0, imageWidth);
        x1 = std::clamp(static_cast<int>(std::ceil(l.centre.x + half.x)), 0, imageWidth);
        y0 = std::clamp(static_cast<int>(std::floor(l.centre.y - half.y)), 0, imageHeight);
        y1 = std::clamp(static_cast<int>(std::ceil(l.centre.y + half.y)), 0, imageHeight);
    }

    sourceSampling(source);
    GLuint upscaled = source; // at render scale 1: RCAS straight on the post-processed image
    if(upscaling && x1 > x0 && y1 > y0)
    {
        ensureUpTarget(imageWidth, imageHeight);
        upscaled = upTex;
        if(mode == 1)
        {
            float con[16];
            easuConstants(static_cast<float>(width), static_cast<float>(height), static_cast<float>(imageWidth),
                static_cast<float>(imageHeight), con);
            GL_BindFramebufferFunc(GL_FRAMEBUFFER, upFbo);
            glViewport(0, 0, imageWidth, imageHeight);
            glEnable(GL_SCISSOR_TEST);
            glScissor(x0, y0, x1 - x0, y1 - y0);
            GL_SetState(GLS_BLEND_OPAQUE | GLS_NO_ZTEST | GLS_NO_ZWRITE | GLS_CULL_NONE | GLS_ATTRIBS(0));
            GL_UseProgram(easu());
            GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D, source);
            for(int i = 0; i < 4; i++)
            {
                GL_Uniform4fvFunc(i, 1, &con[i * 4]);
            }
            GL_Uniform4fFunc(4, l.centre.x, l.centre.y, invPpt.x, invPpt.y);
            GL_Uniform1fFunc(5, radius > 0.f ? radius + margin : 0.f);
            glDrawArrays(GL_TRIANGLES, 0, 3);
            glDisable(GL_SCISSOR_TEST);
        }
        else
        {
            if(!coefScaler)
            {
                coefScaler = coefficientTexture(coef_scale, "vr nis scaler coefficients");
                coefUsm = coefficientTexture(coef_usm, "vr nis usm coefficients");
            }
            constexpr int blockWidth = 32, blockHeight = 24;
            const int bx0 = x0 / blockWidth, by0 = y0 / blockHeight;
            const int bx1 = (x1 + blockWidth - 1) / blockWidth, by1 = (y1 + blockHeight - 1) / blockHeight;
            GL_UseProgram(nis());
            GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D, source);
            GL_BindNative(GL_TEXTURE1, GL_TEXTURE_2D, coefScaler);
            GL_BindNative(GL_TEXTURE2, GL_TEXTURE_2D, coefUsm);
            GL_BindImageTextureFunc(7, upTex, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA8);
            const float values[] = {config.kDetectRatio, config.kDetectThres, config.kMinContrastRatio,
                config.kRatioNorm, config.kContrastBoost, config.kEps, config.kSharpStartY, config.kSharpScaleY,
                config.kSharpStrengthMin, config.kSharpStrengthScale, config.kSharpLimitMin, config.kSharpLimitScale,
                config.kScaleX, config.kScaleY, config.kDstNormX, config.kDstNormY, config.kSrcNormX, config.kSrcNormY};
            for(int i = 0; i < static_cast<int>(std::size(values)); i++)
            {
                GL_Uniform1fFunc(i, values[i]);
            }
            GL_Uniform1iFunc(18, bx0);
            GL_Uniform1iFunc(19, by0);
            GL_DispatchComputeFunc(static_cast<GLuint>(bx1 - bx0), static_cast<GLuint>(by1 - by0), 1);
            GL_MemoryBarrierFunc(GL_TEXTURE_FETCH_BARRIER_BIT);
        }
    }

    // RCAS (FSR, or sharpening alone at scale 1), or NIS's result as it is; bilinear outside the circle.
    const bool rcas = mode == 1 && sharpness() > 0.f;
    GL_BindFramebufferFunc(GL_FRAMEBUFFER, target);
    glViewport(0, 0, imageWidth, imageHeight);
    GL_SetState(GLS_BLEND_OPAQUE | GLS_NO_ZTEST | GLS_NO_ZWRITE | GLS_CULL_NONE | GLS_ATTRIBS(0));
    GL_UseProgram(composeId);
    GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D, source);
    GL_BindNative(GL_TEXTURE1, GL_TEXTURE_2D, upscaled);
    GL_Uniform4fFunc(0, l.centre.x, l.centre.y, invPpt.x, invPpt.y);
    GL_Uniform2fFunc(1, radius, radius * 0.92f);
    GL_Uniform2fFunc(2, 1.f / imageWidth, 1.f / imageHeight);
    GL_Uniform1fFunc(3, rcasConstant(sharpness()));
    GL_Uniform1iFunc(4, rcas ? 1 : 0);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

} // namespace qvr::upscale
