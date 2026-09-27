// vr_gfx_gl.cpp -- vr_gfx.hpp on Ironwail's OpenGL renderer: one shader (a program per shade) for the
// module's triangles, framebuffer-backed targets, and Ironwail's 2D functions (Draw_*, glcanvas) pointed
// at a target. The only GL in the module besides the stereo view setup (vr_stereo.cpp), the
// OpenXR graphics binding and the engine's own entity hooks.

#include "vr_gfx.hpp"
#include "vr_engine.hpp"

#include <algorithm>
#include <cstring>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace qvr::gfx
{
namespace
{

constexpr const char* vertexShader = R"(#version 430
layout(location = 0) uniform mat4 MVP;
layout(location = 0) in vec3 Pos;
layout(location = 1) in vec2 UV;
layout(location = 2) in vec4 Color;
layout(location = 3) in float Soft;
out vec2 uv;
out vec4 color;
out float soft;
out float viewDepth;
void main()
{
    uv = UV;
    color = Color;
    soft = Soft;
    gl_Position = MVP * vec4(Pos, 1.0);
    viewDepth = gl_Position.w; // the distance along the view (a perspective projection's w)
}
)";

// MODE: the Shade, one program each (compiled with "#define MODE n" and BLENDED, programFor): each gets only the
// registers its own shade needs (the screen's glow needs many; a particle's texture read few), no branch on the
// mode. BLENDED (every blend but Opaque): a fragment that adds nothing is discarded (all four zero: with every
// blend it leaves the target as it was), so the blend unit skips its read and write (the transparent part of a
// particle's or decal's quad), and a soft one reads the scene's distances only where it shows. Params: the shade's
// settings (State::params).
//
// Mode 4, a screen (the wrist gadget's, the ammo screens'): the texture's brightness (between its
// luminance and its brightest channel, so that the status bar's gold numbers and the face read as
// bright as the text) in the phosphor's colour, and with CRT strength k: scanlines and a faint
// aperture grille (one a pixel of its virtual screen, Size: fading out where they would be finer
// than the eye's pixels), a darker rim, a slight flicker, a soft bar rolling down, faint static a
// pixel at a time; while it glitches (g), bands torn sideways, the colours split and the static
// thick. Seeded by time only: both eyes see the same. With glow s (Params.w, vr_screen_text_glow):
// the lit strokes' cores whitish and a soft halo round them (glow(): from the texture's mipmaps),
// under the scanlines and torn with the rest.
constexpr const char* fragmentShader = R"(
layout(location = 2) uniform vec4 Params;
layout(location = 3) uniform vec3 Size; // Mode 4's virtual screen: pixels across, down, scanlines a pixel
layout(location = 4) uniform int SoftOn; // State::sceneDistances on unit 1
layout(binding = 0) uniform sampler2D Tex;
layout(binding = 1) uniform sampler2D SceneDistances;
in vec2 uv;
in vec4 color;
in float soft;
in float viewDepth;
out vec4 result;
float hash(vec2 p)
{
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}
float lit(vec2 at, float lod)
{
    vec3 c = textureLod(Tex, at, lod).rgb;
    return 0.5 * max(c.r, max(c.g, c.b)) + 0.6 * dot(c, vec3(0.2126, 0.7152, 0.0722));
}
float phosphor(vec2 at)
{
    // Its own texels, sharp, however small it is in the eye (as without mipmaps); only where they
    // would be much finer than the eye's pixels (a map's board across a room) a level a step sharper
    // than the usual, so that far text does not shimmer.
    return lit(at, max(textureQueryLod(Tex, uv).y - 1.0, 0.0));
}
// Mode 4's glow round the lit strokes: the texture's brightness blurred over about one pixel of the
// virtual screen (inner: 4 taps) and two and a half (outer: 8 taps), each tap a mipmap level about
// as coarse as the ring is wide, less the face's own. Near the strokes it is whitish, further out
// the phosphor's colour.
vec3 glow(vec2 at, float s)
{
    float perPixel = float(textureSize(Tex, 0).x) / Size.x; // texels a pixel of the virtual screen
    vec2 px = 1.0 / Size.xy;
    float lodInner = log2(perPixel), lodOuter = log2(perPixel * 2.0);
    float inner = 0.0, outer = 0.0;
    for(int i = 0; i < 4; i++)
    {
        float a = 1.5707963 * float(i) + 0.7853982;
        inner += lit(at + vec2(cos(a), sin(a)) * px * 0.9, lodInner);
    }
    for(int i = 0; i < 8; i++)
    {
        float a = 0.7853982 * float(i) + 0.3926991;
        outer += lit(at + vec2(cos(a), sin(a)) * px * 2.5, lodOuter);
    }
    const float face = 0.14; // about the face's brightness: it does not glow
    inner = max(inner * 0.25 - face, 0.0) / (1.0 - face);
    outer = max(outer * 0.125 - face, 0.0) / (1.0 - face);
    return s * (mix(color.rgb, vec3(1.0), 0.5) * (0.9 * inner) + color.rgb * (1.2 * outer));
}
vec4 screen()
{
    float t = Params.x, k = Params.y, g = Params.z;
    float tick = floor(t * 30.0);
    float seed = fract(tick * 0.618034) * 512.0;
    vec2 at = uv;
    if(g > 0.0)
    {
        float band = floor(at.y * 14.0 + hash(vec2(seed, 3.0)) * 4.0);
        if(hash(vec2(band, seed)) > 1.0 - 0.6 * g)
            at.x += (hash(vec2(band, seed + 7.0)) - 0.5) * 0.15 * g;
        at.y += (hash(vec2(seed, 11.0)) - 0.5) * 0.02 * g;
    }
    float split = (0.144 * k + 1.44 * g) / Size.x;
    float lum = phosphor(at);
    vec3 rgb = color.rgb * vec3(phosphor(at - vec2(split, 0.0)), lum, phosphor(at + vec2(split, 0.0)));
    float s = Params.w;
    if(s > 0.0)
    {
        // The lit strokes' core pushed towards white, a little over (for the bloom), and their glow.
        float core = s * 0.8 * smoothstep(0.3, 0.9, lum);
        rgb = mix(rgb, vec3(1.2 * lum), min(core, 0.9));
        rgb += glow(at, s);
    }
    rgb *= 1.0 - 0.3 * g;

    float y = uv.y * Size.y * Size.z;
    float scanFade = clamp(1.0 - (fwidth(y) - 0.25) * 2.5, 0.0, 1.0);
    float scan = 1.0 - 0.3 * k * scanFade * (0.5 + 0.5 * cos(y * 6.2831853));
    float x = uv.x * Size.x;
    float grilleFade = clamp(1.0 - (fwidth(x) - 0.25) * 2.5, 0.0, 1.0);
    float grille = 1.0 - 0.12 * k * grilleFade * (0.5 + 0.5 * cos(x * 6.2831853));
    vec2 p = uv * 2.0 - 1.0;
    float rim = 1.0 - 0.15 * k * dot(p, p);
    float flicker = 1.0 - 0.025 * k * hash(vec2(floor(t * 24.0) * 0.618034, 5.0));
    rgb *= scan * grille * rim * flicker;

    float d = fract(uv.y + fract(t * 0.12) + 0.5) - 0.5;
    rgb += color.rgb * (0.05 * k * exp(-d * d * 400.0));
    float noise = hash(floor(uv * Size.xy) + vec2(seed, seed * 0.37));
    rgb += color.rgb * (noise * k * (0.025 + 0.25 * g));
    return vec4(rgb, 1.0);
}
// Mode 5, the wrist gadget's hologram (premultiplied; the vertex colour's alpha: how shown). Params: time, the effect's
// strength k, glitch g, and 1 for the beam. The text: its lit strokes (the texture's brightness) in the vertex colour,
// their cores whitish and a soft halo round them (glow(): Size and the mipmaps as mode 4's), a faint dark haze round
// them (the alpha) so that it reads against a bright wall; with k, scanlines drifting up, a bright band now and then,
// a flicker and a tiny shake; while it glitches, bands torn sideways and the colours split. The beam: the vertex colour
// fading across (uv.x -1..1) and up (uv.y 0 at the gadget's screen, 1 at the text), in slow streaks, scanlines rising
// through it; added (alpha 0).
vec4 hologram()
{
    float t = Params.x, k = Params.y, g = Params.z;
    float flicker = 1.0 - 0.1 * k * hash(vec2(floor(t * 20.0) * 0.618034, 9.0));
    if(Params.w > 0.5)
    {
        float across = max(1.0 - uv.x * uv.x, 0.0);
        float up = clamp(uv.y, 0.0, 1.0);
        float along = smoothstep(0.0, 0.2, up) * mix(1.0, 0.2, up);
        float streaks = 0.7 + 0.3 * sin(uv.x * 13.0 + t * 1.7) * sin(uv.x * 4.0 - t * 1.1);
        float scan = 1.0 - 0.35 * min(k, 1.0) * (0.5 + 0.5 * sin((up * 10.0 - t * 1.5) * 6.2831853));
        return vec4(color.rgb * (color.a * across * across * along * streaks * scan * flicker), 0.0);
    }
    float tick = floor(t * 30.0);
    float seed = fract(tick * 0.618034) * 512.0;
    vec2 at = uv;
    at.y += k * 0.3 / Size.y * sin(t * 7.3) * sin(t * 2.9);
    if(g > 0.0)
    {
        float band = floor(at.y * 12.0 + hash(vec2(seed, 3.0)) * 4.0);
        if(hash(vec2(band, seed)) > 1.0 - 0.6 * g)
            at.x += (hash(vec2(band, seed + 7.0)) - 0.5) * 0.08 * g;
        at.y += (hash(vec2(seed, 11.0)) - 0.5) * 0.02 * g;
    }
    float split = (0.4 * k + 2.0 * g) / Size.x;
    float lum = phosphor(at);
    vec3 rgb = color.rgb * vec3(phosphor(at - vec2(split, 0.0)), lum, phosphor(at + vec2(split, 0.0)));
    rgb = mix(rgb, vec3(1.1 * lum), 0.3 * smoothstep(0.3, 0.9, lum));
    rgb += glow(at, 1.0);
    float haze = lit(at, log2(float(textureSize(Tex, 0).x) / Size.x * 4.0));

    float y = uv.y * Size.y * Size.z;
    float scanFade = clamp(1.0 - (fwidth(y) - 0.25) * 2.5, 0.0, 1.0);
    float scan = 1.0 - 0.4 * min(k, 1.5) * scanFade * (0.5 + 0.5 * cos((y - t * 1.5) * 6.2831853));
    float d = fract(uv.y - t * 0.3) - 0.5;
    float band = 1.0 + 0.4 * k * exp(-d * d * 150.0);
    rgb *= scan * band * flicker * (1.0 - 0.3 * g);
    float a = clamp(haze * 2.0 + lum * 0.4, 0.0, 0.6);
    return vec4(rgb, a) * color.a;
}
void main()
{
#if MODE == 1
    float falloff = clamp(1.0 - dot(uv, uv), 0.0, 1.0);
    result = vec4(color.rgb, color.a * falloff);
#elif MODE == 2
    result = texture(Tex, uv) * color;
#elif MODE == 3
    vec4 c = texture(Tex, uv);
    if(c.a < 0.666)
        discard;
    result = vec4(c.rgb * color.rgb, 1.0);
#elif MODE == 4
    result = screen();
#elif MODE == 5
    result = hologram();
#else
    result = color;
#endif
#if BLENDED
    if(result == vec4(0.0))
        discard;
#endif
    // Soft: fading out as the opaque scene comes close behind (premultiplied: all four). The distances are half the
    // target's size, each the nearest of its four pixels.
    if(SoftOn != 0 && soft > 0.0)
    {
        ivec2 p = min(ivec2(gl_FragCoord.xy) >> 1, textureSize(SceneDistances, 0) - 1);
        float f = clamp((texelFetch(SceneDistances, p, 0).r - viewDepth) / soft, 0.0, 1.0);
        result *= f * f * (3.0 - 2.0 * f);
    }
}
)";

// One program per shade and whether it blends without writing depth (programFor): 0 not made yet.
constexpr int shadeCount = static_cast<int>(Shade::Hologram) + 1;
GLuint programs[shadeCount][2]{};
bool programFailed[shadeCount][2]{};

[[nodiscard]] GLuint compile(GLenum type, const char* source, const char* name)
{
    const GLuint shader = GL_CreateShaderFunc(type);
    GL_ShaderSourceFunc(shader, 1, &source, nullptr);
    GL_CompileShaderFunc(shader);

    GLint ok = 0;
    GL_GetShaderivFunc(shader, GL_COMPILE_STATUS, &ok);
    if(!ok)
    {
        char log[1024];
        GL_GetShaderInfoLogFunc(shader, sizeof(log), nullptr, log);
        Con_Warning("VR: %s shader failed to compile:\n%s\n", name, log);
    }
    return shader;
}

// The program for a shade; blended: a blend other than Opaque, writing no depth (zero fragments discarded). 0 if it
// does not build.
GLuint programFor(Shade shade, bool blended)
{
    const int s = static_cast<int>(shade);
    GLuint& p = programs[s][blended];
    if(p || programFailed[s][blended])
    {
        return p;
    }
    const std::string fragment = "#version 430\n#define MODE " + std::to_string(s) + "\n#define BLENDED " +
                                 (blended ? "1" : "0") + "\n" + fragmentShader;
    p = glProgram(vertexShader, fragment.c_str(), "vr triangles");
    programFailed[s][blended] = !p;
    return p;
}

// begin2D() / end2D().
struct Saved2D
{
    bool active{false};
    glcanvas_t canvas{};
    GLuint mipmapped{0}; // the target's texture, if its mipmaps are to be rebuilt
};
Saved2D saved2D;

// Where the engine draws its 2D pass: the post-processing composite, or the window. Set rather than
// read back: glGet* makes the CPU wait for the driver (about half a millisecond a call with a
// threaded driver).
void bindWindow()
{
    GL_BindFramebufferFunc(GL_FRAMEBUFFER, GL_NeedsPostprocess() ? framebufs.composite.fbo : 0);
    glViewport(glx, gly, glwidth, glheight);
}

// By name; looked up by a const char* without making a std::string of it (a transparent hash).
struct NameHash
{
    using is_transparent = void;
    [[nodiscard]] std::size_t operator()(std::string_view s) const { return std::hash<std::string_view>{}(s); }
};
std::unordered_map<std::string, qpic_t*, NameHash, std::equal_to<>> pics;

[[nodiscard]] qpic_t* picNamed(const char* name)
{
    const auto it = pics.find(std::string_view{name});
    if(it != pics.end())
    {
        return it->second;
    }
    qpic_t* p = Draw_PicFromWad(name);
    pics.emplace(name, p);
    return p;
}

GLuint copyFbo = 0;

// glBlendFuncSeparate (GL 1.4), which Ironwail does not load.
using BlendFuncSeparateFn = void(APIENTRY*)(GLenum, GLenum, GLenum, GLenum);
BlendFuncSeparateFn blendFuncSeparate = nullptr;

} // namespace

void onGameDirChanged()
{
    pics.clear();
}

unsigned glProgram(const char* vertex, const char* fragment, const char* name)
{
    const GLuint vs = compile(GL_VERTEX_SHADER, vertex, name);
    const GLuint fs = fragment ? compile(GL_FRAGMENT_SHADER, fragment, name) : 0;
    const GLuint p = GL_CreateProgramFunc();
    GL_AttachShaderFunc(p, vs);
    if(fs)
    {
        GL_AttachShaderFunc(p, fs);
    }
    GL_LinkProgramFunc(p);
    GL_DeleteShaderFunc(vs);
    if(fs)
    {
        GL_DeleteShaderFunc(fs);
    }

    GLint ok = 0;
    GL_GetProgramivFunc(p, GL_LINK_STATUS, &ok);
    if(!ok)
    {
        Con_Warning("VR: %s shader failed to link\n", name);
        GL_DeleteProgramFunc(p);
        return 0;
    }
    GL_ObjectLabelFunc(GL_PROGRAM, p, -1, name);
    return p;
}

namespace
{

// The program and state for `state`, its uniforms and textures set; false if there is no program.
[[nodiscard]] bool beginDraw(const glm::mat4& mvp, const State& state, Texture texture)
{
    const GLuint program = programFor(state.shade, state.blend != Blend::Opaque && !state.depthWrite);
    if(!program)
    {
        return false;
    }

    unsigned flags = GLS_CULL_NONE | GLS_ATTRIBS(4);
    flags |= state.blend == Blend::Opaque ? GLS_BLEND_OPAQUE : GLS_BLEND_ALPHA;
    if(!state.depthTest)
    {
        flags |= GLS_NO_ZTEST;
    }
    if(!state.depthWrite)
    {
        flags |= GLS_NO_ZWRITE;
    }

    GL_UseProgram(program);
    GL_SetState(flags);
    if(state.blend == Blend::Premultiplied)
    {
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    }
    else if(state.blend == Blend::Modulate)
    {
        glBlendFunc(GL_DST_COLOR, GL_ONE_MINUS_SRC_ALPHA);
    }
    else if(state.blend == Blend::Additive)
    {
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    }
    GL_UniformMatrix4fvFunc(0, 1, GL_FALSE, &mvp[0][0]);
    if(state.shade == Shade::Screen || state.shade == Shade::Hologram)
    {
        GL_Uniform4fFunc(2, state.params.x, state.params.y, state.params.z, state.params.w);
        GL_Uniform3fFunc(3, state.screen.x, state.screen.y, state.screen.z);
    }
    GL_Uniform1iFunc(4, state.sceneDistances ? 1 : 0);
    if(texture)
    {
        GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D, texture);
    }
    if(state.sceneDistances)
    {
        GL_BindNative(GL_TEXTURE1, GL_TEXTURE_2D, state.sceneDistances);
    }
    return true;
}

// `count` vertices from `buffer` at `offset`, then the blend put back as GLS_BLEND_ALPHA expects it.
void drawVertices(GLuint buffer, const GLbyte* offset, std::size_t count, const State& state)
{
    GL_BindBuffer(GL_ARRAY_BUFFER, buffer);
    GL_VertexAttribPointerFunc(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), offset + offsetof(Vertex, pos));
    GL_VertexAttribPointerFunc(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), offset + offsetof(Vertex, uv));
    GL_VertexAttribPointerFunc(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), offset + offsetof(Vertex, color));
    GL_VertexAttribPointerFunc(3, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex), offset + offsetof(Vertex, soft));
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(count));
    GL_BindBuffer(GL_ARRAY_BUFFER, 0);

    if(state.blend == Blend::Premultiplied || state.blend == Blend::Modulate || state.blend == Blend::Additive)
    {
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // what GLS_BLEND_ALPHA expects
    }
}

} // namespace

void draw(std::span<const Vertex> triangles, const glm::mat4& mvp, const State& state, Texture texture)
{
    if(triangles.empty() || !beginDraw(mvp, state, texture))
    {
        return;
    }
    // Into the frame's upload buffer, like Ironwail's own dynamic geometry.
    GLuint buf = 0;
    GLbyte* ofs = nullptr;
    GL_Upload(GL_ARRAY_BUFFER, triangles.data(), triangles.size_bytes(), &buf, &ofs);
    drawVertices(buf, ofs, triangles.size(), state);
}

void upload(StaticTriangles& t, std::span<const Vertex> triangles)
{
    t.count = triangles.size();
    t.uploads++;
    t.uploadedBytes += triangles.size_bytes();
    if(triangles.empty())
    {
        return;
    }
    if(!t.buffer || triangles.size_bytes() > t.capacity)
    {
        if(t.buffer)
        {
            GL_DeleteBuffer(t.buffer);
        }
        t.capacity = triangles.size_bytes() + triangles.size_bytes() / 2; // room to grow
        t.buffer = GL_CreateBuffer(GL_ARRAY_BUFFER, GL_DYNAMIC_DRAW, "vr static triangles", t.capacity, nullptr);
    }
    else
    {
        // Orphaned: the frames still drawing from its old contents keep them, the driver gives it new storage.
        GL_BindBuffer(GL_ARRAY_BUFFER, t.buffer);
        GL_BufferDataFunc(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(t.capacity), nullptr, GL_DYNAMIC_DRAW);
    }
    GL_BindBuffer(GL_ARRAY_BUFFER, t.buffer);
    GL_BufferSubDataFunc(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(triangles.size_bytes()), triangles.data());
    GL_BindBuffer(GL_ARRAY_BUFFER, 0);
}

void draw(const StaticTriangles& t, const glm::mat4& mvp, const State& state, Texture texture)
{
    if(!t.buffer || t.count == 0 || !beginDraw(mvp, state, texture))
    {
        return;
    }
    drawVertices(t.buffer, nullptr, t.count, state);
}

glm::mat4 sceneViewProjection()
{
    glm::mat4 viewProj;
    memcpy(&viewProj[0][0], r_matviewproj, sizeof(r_matviewproj));
    return viewProj;
}

void sceneCamera(glm::vec3& origin, glm::vec3& right, glm::vec3& up)
{
    origin = {r_refdef.vieworg[0], r_refdef.vieworg[1], r_refdef.vieworg[2]};
    right = {vright[0], vright[1], vright[2]};
    up = {vup[0], vup[1], vup[2]};
}

Texture fontTexture()
{
    return char_texture ? char_texture->texnum : 0;
}

// Ironwail's console font atlas: 16 x 16 cells of 10 x 10 texels, each an 8 x 8 glyph with a
// one-texel border.
glm::vec4 fontGlyph(unsigned char c)
{
    constexpr float atlas = 160.f;
    const float u0 = ((c & 15) * 10 + 1) / atlas;
    const float v0 = ((c >> 4) * 10 + 1) / atlas;
    return {u0, v0, u0 + 8.f / atlas, v0 + 8.f / atlas};
}

int targetsMade = 0; // targets' textures (re)made so far (vr_memstats: steady in play, not a frame)
std::vector<std::pair<const char*, int>> targetsMadeNamed; // by name, in the order first made

std::string targetsMadeByName()
{
    std::string out;
    for(const auto& [name, count] : targetsMadeNamed)
    {
        out += (out.empty() ? "" : " ") + std::string{name} + ":" + std::to_string(count);
    }
    return out;
}

void ensureTarget(Target& target, int width, int height, bool mipmaps, const char* name)
{
    int levels = 1;
    while(mipmaps && std::max(width, height) >> levels)
    {
        levels++;
    }
    if(target.texture && target.width == width && target.height == height && target.levels == levels)
    {
        return;
    }
    targetsMade++;
    auto named = std::find_if(targetsMadeNamed.begin(), targetsMadeNamed.end(),
        [&](const auto& n) { return std::strcmp(n.first, name) == 0; });
    if(named == targetsMadeNamed.end())
    {
        targetsMadeNamed.emplace_back(name, 0);
        named = targetsMadeNamed.end() - 1;
    }
    named->second++;
    Con_DPrintf("VR: render target \"%s\" made at %dx%d (was %dx%d), %d times so far\n", name, width, height,
        target.width, target.height, named->second);

    GLuint texture = target.texture;
    if(texture)
    {
        glDeleteTextures(1, &texture);
    }
    GLuint fbo = target.framebuffer;
    if(!fbo)
    {
        GL_GenFramebuffersFunc(1, &fbo);
    }

    glGenTextures(1, &texture);
    GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D, texture);
    GL_TexStorage2DFunc(GL_TEXTURE_2D, levels, GL_RGBA8, width, height);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, levels > 1 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    GLint previous = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &previous);
    GL_BindFramebufferFunc(GL_FRAMEBUFFER, fbo);
    GL_FramebufferTexture2DFunc(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
    GL_BindFramebufferFunc(GL_FRAMEBUFFER, static_cast<GLuint>(previous));

    target = {texture, fbo, width, height, levels};
}

void begin2D(const Target& target, int virtualWidth, int virtualHeight)
{
    Draw_Flush();

    saved2D.active = true;
    saved2D.canvas = glcanvas;
    saved2D.mipmapped = target.levels > 1 ? target.texture : 0;

    GL_BindFramebufferFunc(GL_FRAMEBUFFER, target.framebuffer);
    glViewport(0, 0, target.width, target.height);

    // A canvas of its own: the virtual screen over the whole target, y down.
    glcanvas.type = CANVAS_INVALID;
    glcanvas.transform.scale[0] = 2.f / virtualWidth;
    glcanvas.transform.scale[1] = -2.f / virtualHeight;
    glcanvas.transform.offset[0] = -1.f;
    glcanvas.transform.offset[1] = 1.f;
    Draw_GetTransformBounds(&glcanvas.transform, &glcanvas.left, &glcanvas.top, &glcanvas.right, &glcanvas.bottom);
    glcanvas.blendmode = GLS_BLEND_ALPHA;
    GL_SetCanvasColor(1.f, 1.f, 1.f, 1.f);
}

void end2D()
{
    if(!saved2D.active)
    {
        return;
    }
    Draw_Flush();
    saved2D.active = false;

    glcanvas = saved2D.canvas;
    bindWindow();

    // Its mipmaps, for Shade::Screen's glow (a few small textures: next to nothing).
    if(saved2D.mipmapped)
    {
        GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D, saved2D.mipmapped);
        GL_GenerateMipmapFunc(GL_TEXTURE_2D);
        saved2D.mipmapped = 0;
    }
}

namespace draw2D
{

void color(const glm::vec4& rgba)
{
    GL_SetCanvasColor(rgba.r, rgba.g, rgba.b, rgba.a);
}

void fill(float x, float y, float w, float h, const glm::vec3& rgb)
{
    const float c[3] = {rgb.r, rgb.g, rgb.b};
    Draw_FillEx(x, y, w, h, c, 1.f);
}

void pic(float x, float y, const char* wadName, float scale)
{
    if(qpic_t* p = picNamed(wadName))
    {
        Draw_SubPic(x, y, p->width * scale, p->height * scale, p, 0.f, 0.f, 1.f, 1.f, rgb_white, 1.f);
    }
}

void text(float x, float y, float size, const char* str)
{
    Draw_StringEx(x, y, size, str);
}

} // namespace draw2D

void beginCanvas(Target& canvas, int width, int height)
{
    ensureTarget(canvas, width, height, false, "panel canvas");
    GL_ResetState(); // re-applies the blend, now with applyCanvasBlend() (VR_CanvasBlend)
    GL_BindFramebufferFunc(GL_FRAMEBUFFER, canvas.framebuffer);
    glViewport(0, 0, width, height);
    glClearColor(0.f, 0.f, 0.f, 0.f);
    glClear(GL_COLOR_BUFFER_BIT);
}

void endCanvas()
{
    GL_ResetState(); // back to Ironwail's usual blend
    bindWindow();
}

bool applyCanvasBlend()
{
    if(!blendFuncSeparate)
    {
        blendFuncSeparate = reinterpret_cast<BlendFuncSeparateFn>(SDL_GL_GetProcAddress("glBlendFuncSeparate"));
        if(!blendFuncSeparate)
        {
            return false;
        }
    }
    blendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    return true;
}

void copy(const Target& target, Texture image)
{
    if(!copyFbo)
    {
        GL_GenFramebuffersFunc(1, &copyFbo);
    }

    GL_BindFramebufferFunc(GL_DRAW_FRAMEBUFFER, copyFbo);
    GL_FramebufferTexture2DFunc(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, image, 0);
    GL_BindFramebufferFunc(GL_READ_FRAMEBUFFER, target.framebuffer);
    GL_BlitFramebufferFunc(0, 0, target.width, target.height, 0, 0, target.width, target.height, GL_COLOR_BUFFER_BIT,
        GL_NEAREST);
    GL_FramebufferTexture2DFunc(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);

    bindWindow(); // it is called with the window's 2D pass bound
}

Texture createTexture(int width, int height, const void* rgba, bool mipmaps)
{
    mipmaps = mipmaps && rgba;
    int levels = 1;
    while(mipmaps && std::max(width, height) >> levels)
    {
        levels++;
    }

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    GL_TexStorage2DFunc(GL_TEXTURE_2D, levels, GL_RGBA8, width, height);
    if(rgba)
    {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
        if(mipmaps)
        {
            GL_GenerateMipmapFunc(GL_TEXTURE_2D);
            if(gl_max_anisotropy > 1.f) // else not supported
            {
                glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, std::min(16.f, gl_max_anisotropy));
            }
        }
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, mipmaps ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    GL_ClearBindings();
    return tex;
}

void destroyTexture(Texture texture)
{
    GLuint tex = texture;
    glDeleteTextures(1, &tex);
    GL_ClearBindings();
}

} // namespace qvr::gfx
