// vr_gfx.hpp -- the little rendering the VR module does itself, over the engine's renderer:
// triangles in the scene or on panels (lines, text, shadows, the 2D layer's panels), offscreen
// colour targets (the 2D canvas, the wrist gadget's screen), and 2D drawing onto them with the
// engine's own 2D functions.
//
// This interface is engine-free; vr_gfx_gl.cpp implements it on Ironwail's OpenGL renderer.
// Porting the module to another renderer (vkQuake's Vulkan) means implementing this file again,
// along with the stereo view setup (vr_stereo.cpp) and the entity hooks (vr_api_render.h):
// see docs/vr-port/PORTING.md.

#pragma once

#include "Zancle/Base/SizeT.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/Vocabulary/Span.hpp"

#include <glm/glm.hpp>


namespace qvr::gfx
{

// A texture of the engine's renderer (for OpenGL, a texture name). 0 is none.
using Texture = unsigned;

struct Vertex
{
    glm::vec3 pos;
    glm::vec2 uv{0.f};
    glm::vec4 color{1.f};
    float soft{0.f}; // with State::sceneDistances: how close in front of the scene it fades out, in units (0: never)
};

enum class Shade
{
    Color,         // the vertex colour
    SoftEdge,      // the vertex colour, its alpha fading out with |uv| (1 - |uv|^2): lines, discs
    Texture,       // the texture, times the vertex colour
    TextureCutout, // the texture's colour times the vertex colour, opaque, where its alpha is at least 2/3 (the font)
    Screen,        // a screen (the wrist gadget's, the ammo screens'): the texture in one phosphor colour
                   // (the vertex colour), as a small CRT (State::params: time in seconds, CRT strength,
                   // glitch 0..1, the lit strokes' glow; State::screen: its virtual screen's size in
                   // pixels and its scanlines per pixel). The glow needs the texture's mipmaps (a
                   // target made with them). Its alpha: the vertex colour's. With State::trueColor (the
                   // wrist gadget's) the texture's own colours instead, the vertex colour only the static's.
    Hologram,      // the wrist gadget's hologram (premultiplied): with State::params.w 0 its text, the texture's
                   // lit strokes in the vertex colour (its alpha: how shown), glowing, with a faint haze round
                   // them, scanlines, a flicker and glitches (params: time, effect strength, glitch 0..1;
                   // State::screen as Screen's; mipmaps); with params.w 1 the beam of light up to it, the vertex
                   // colour fading across (uv.x -1..1) and up (uv.y 0..1), added.
};

enum class Blend
{
    Opaque,
    Alpha,         // colours with a separate alpha
    Premultiplied, // colours already multiplied by their alpha (the 2D canvas)
    Modulate,      // the scene times the colour (premultiplied: dst * (colour + 1 - alpha)), decals
    Additive,      // the colour times its alpha added to the scene (glows)
};

struct State
{
    Shade shade{Shade::Color};
    Blend blend{Blend::Alpha};
    bool depthTest{true};
    bool depthWrite{false};
    glm::vec4 params{0.f}; // the shade's own settings (Shade::Screen's)
    glm::vec3 screen{240.f, 150.f, 0.5f}; // Shade::Screen's pixels across, down, and scanlines a pixel
    bool trueColor{false}; // Shade::Screen: the texture's own colours instead of its brightness in the vertex colour (the wrist gadget's)
    // Soft (premultiplied blends): the opaque scene's distances along the view (vr_water.hpp's opaqueSceneDistances,
    // half the target's size); each vertex's colour times how far in front of the scene it is over its Vertex::soft
    // (0 to 1, smoothly). 0: not soft.
    Texture sceneDistances{0};
    // Shade::Texture in the scene (decals, particles): retro textures' set (vr_retro.hpp categorySet; 0 none), read in
    // world units: blocks fixed to the texture, palette and dither on the colour (premultiplied).
    int retro{0};
};

// Triangles (three vertices each), transformed by `mvp` to clip space. No culling. They are copied into the frame's
// upload buffer at each call.
void draw(za::Span<const Vertex> triangles, const glm::mat4& mvp, const State& state, Texture texture = 0);

// Triangles kept in a vertex buffer of their own, for ones that stay the same over many frames (the settled decals):
// uploaded when they change, then drawn from it in both eyes and every frame until they change again.
struct StaticTriangles
{
    unsigned buffer{0};
    za::SizeT capacity{0}; // bytes
    za::SizeT count{0};    // vertices
    long long uploads{0};    // so far (vr_decal_count)
    long long uploadedBytes{0};
};
void upload(StaticTriangles& triangles, za::Span<const Vertex> vertices);
// Only `vertices`' `count` from `first` sent, the others as they were uploaded (it fits the buffer: no larger than when
// last uploaded, or its room to grow).
void update(StaticTriangles& triangles, za::Span<const Vertex> vertices, za::SizeT first, za::SizeT count);
void draw(const StaticTriangles& triangles, const glm::mat4& mvp, const State& state, Texture texture = 0);

// A shader storage buffer (std430) an engine shader reads (the decals on the world: vr_decals.cpp), uploaded when its
// contents change (orphaned: frames still reading the old contents keep them).
struct StorageBuffer
{
    unsigned buffer{0};
    za::SizeT capacity{0}; // bytes
    za::SizeT size{0};     // bytes uploaded
};
void upload(StorageBuffer& buffer, const void* data, za::SizeT bytes);
// Bound to `binding` (GL_SHADER_STORAGE_BUFFER), as uploaded; nothing if empty.
void bindStorage(unsigned binding, const StorageBuffer& buffer);
// A 2D texture on texture unit `unit`.
void bindTexture(unsigned unit, Texture texture);

// Camera-facing particles (vr_particles.cpp), made into quads on the GPU: one record each, uploaded once a frame and
// drawn from the same buffer in every view (each view's quads built in the vertex shader from its own camera), rather
// than six vertices each made and uploaded again for each eye. The quad: 2 x `half` across, turned by (cos, sin) about
// the view direction (flat: lying on the horizontal plane); streaked (streak > 0): along its velocity as seen from the
// eye, `half` wide and half + min(streak x speed, 8) long; with `pull` (Shade::Texture, soft): moved towards the eye
// along its rays by `pull`, not nearer than 8 units. The corners' texture coordinates: the cell's (u0, v0, u1, v1).
struct ParticleInstance
{
    glm::vec3 org;
    float half;
    glm::vec4 color; // premultiplied
    glm::vec3 vel;
    float streak;
    float cos, sin;
    float soft; // as Vertex::soft
    float pull;
    glm::vec4 uv;
    float flat; // 1: lying on the horizontal plane
    float cell; // atlas cell for conservative support bounds; -1: full support
    float pad[2];
};
static_assert(sizeof(ParticleInstance) == 96);

// Particles uploaded for this frame (uploadParticles), valid until the frame ends.
struct ParticleBatch
{
    unsigned buffer{0};
    za::SizeT offset{0};
    za::SizeT count{0};
    bool trim{false}; // at least one sprite has useful transparent margins
    unsigned serial{0}; // the frame it was uploaded in (gl_frameres_serial): drawn in another, vr_debug_glstate says so
};
// Base-level nonzero RGBA bounds, including a 16-texel atlas guard; in atlas UVs.
void particleSupportBounds(za::Span<const glm::vec4> bounds, int width, int height);
[[nodiscard]] ParticleBatch uploadParticles(za::Span<const ParticleInstance> particles, bool trim);
// Which of them a draw is for: all, or those small or large in the view (ParticleSplit).
enum class ParticlePass : int
{
    All,
    Small,
    Large,
};
// Large: half across at least `largePixels` of the scene target's pixels, at `pixelScale` pixels a unit at distance 1.
struct ParticleSplit
{
    float pixelScale{0.f};
    float largePixels{0.f};
};
// Draws them in the scene view (sceneViewProjection, sceneCamera); `pull`: moved towards the eye by their pull.
// `reverse` (vr_particle_saturate): composited in reverse order into a target of their own, the pixels already opaque
// skipped by the ones under them, then blended over the scene (the same image, less fill), when the scene's target
// allows it.
void drawParticles(const ParticleBatch& batch, bool pull, const State& state, Texture texture,
    ParticlePass pass = ParticlePass::All, const ParticleSplit& split = {}, bool reverse = false);
// The large ones (split) drawn at half the scene's size into a target of their own, hidden behind the scene's
// `distances` (width x height, water::opaqueSceneDistances; `soft`: fading against them too), then blended into the
// scene in one pass. A quarter of their fragments, a little softer. `retro`: their effective retro set (0 off). `viewport`: the
// scene's (R_SceneViewport); `restore` binds the scene's framebuffer again. False if it could not (drawn nothing).
[[nodiscard]] bool drawParticlesHalf(const ParticleBatch& batch, bool pull, Texture texture, const ParticleSplit& split,
    Texture distances, int width, int height, const int viewport[4], bool soft, int retro, void (*restore)());

// A lit tube made on the GPU from one record a ring (the flashlight's chain, the chainsaw's cord: vr_coil.cpp): `sides`
// vertices round each ring, consecutive rings joined; opaque, depth-tested and written, in the scene view. Each vertex is lit as the
// cord's CPU shading was: the ring's ambient light shaded by the normal against `key` (0.6 .. 1.4), its lamps' light by
// its angle to where it comes from, and a sheen towards the eye (each eye's own); times `albedo`. A ring's ambient.w
// mixes its albedo towards `rust` (and takes its sheen away), its lamp.w darkens it (grime): the flashlight's chain's
// links (vr_coil.cpp); 0 for a plain tube.
struct TubeRing
{
    glm::vec4 mid;     // xyz the ring's middle, w the tube's radius
    glm::vec4 across;  // xyz a unit axis across the ring (the other: along x across)
    glm::vec4 along;   // xyz the tube's unit direction there
    glm::vec4 ambient; // rgb the world's light (1: Quake's full light), w its rust (0 .. 1)
    glm::vec4 lamp;    // rgb the dynamic lights' light reaching it, w its grime (0 .. 1)
    glm::vec4 lampDir; // xyz the unit direction it comes from
};
static_assert(sizeof(TubeRing) == 96);
struct TubeBatch
{
    unsigned buffer{0};
    za::SizeT offset{0};
    za::SizeT count{0}; // rings
    unsigned serial{0}; // (as ParticleBatch's)
};
// Into the frame's upload buffer, valid until the frame ends (drawn from it in both eyes).
[[nodiscard]] TubeBatch uploadTube(za::Span<const TubeRing> rings);
// `flat`: faceted (the low-poly chain's square bar): the sides turned half a side (a face, not a corner, along `across`)
// and each quad lit by its own face's normal.
void drawTube(const TubeBatch& batch, int sides, const glm::vec3& albedo, const glm::vec3& key,
    const glm::vec3& rust = glm::vec3{0.f}, bool flat = false);

// A model's mesh bent along a curve, copy after copy (the grappling hook's rope, vr_rope.cpp: Rogue's chain links laid
// end to end along the hanging rope, each bent with it, so that it is drawn in one piece): made on the GPU from one
// frame record (uploadBent: vec4s) holding the mesh, BentVertex each, and the curve, CurveSample each. Copy k's vertex
// at model x lies (k x period + x) x scale along the curve (its arc length, found by a binary search of the samples';
// before the first sample and past the last, along the ends' straight lines), across it at its y and z times scale
// along the curve's side and up there (interpolated between the samples). Opaque, back faces culled (the model's
// winding), depth-tested and written, in the scene view; shaded as an alias model's skin with its light (an
// ALPHABRIGHT skin: its fullbright texels unlit), plus a fullbright texture if it has one, clamped to the scene's
// brightest and fogged as the scene's models are.
struct BentVertex
{
    glm::vec4 pos; // xyz the model's coordinates (units: its scale applied), w 0
    glm::vec4 uv;  // xy the skin's texture coordinates
};
struct CurveSample
{
    glm::vec4 pos;  // xyz on the curve, w its arc length there (from the curve's start; increasing)
    glm::vec4 side; // xyz the unit direction the model's +y goes there (its left, for a Quake model)
    glm::vec4 up;   // xyz its +z
};
static_assert(sizeof(BentVertex) == 32 && sizeof(CurveSample) == 48);
struct BentBatch
{
    unsigned buffer{0};
    za::SizeT offset{0};
    za::SizeT count{0}; // vec4s
    unsigned serial{0}; // (as ParticleBatch's)
};
[[nodiscard]] BentBatch uploadBent(za::Span<const glm::vec4> data);
struct BentDraw
{
    int meshFirst{0};    // the mesh's first vec4 in the batch
    int meshVertices{0}; // its vertices (triangles: three each)
    int curveFirst{0};   // the curve's first vec4
    int samples{0};      // its samples (at least 2)
    int copies{0};
    float period{30.f}; // model units from one copy to the next
    float scale{1.f};
    Texture skin{0};
    Texture fullbright{0}; // 0: none
    glm::vec3 light{1.f};  // the light on its lit texels (1: Quake's full light)
};
void drawBent(const BentBatch& batch, const BentDraw& draw);

// The scene view's world-to-clip transform, while the scene (or an eye) is rendered.
[[nodiscard]] glm::mat4 sceneViewProjection();

// The scene view's position and its right and up directions (for camera-facing sprites).
void sceneCamera(glm::vec3& origin, glm::vec3& right, glm::vec3& up);

// The console font: its texture, and a character's texture rectangle (u0, v0, u1, v1).
[[nodiscard]] Texture fontTexture();
[[nodiscard]] glm::vec4 fontGlyph(unsigned char c);

// An offscreen colour target (RGBA8, linear filtering, clamped).
struct Target
{
    Texture texture{0};
    unsigned framebuffer{0};
    int width{0};
    int height{0};
    int levels{1}; // with a mipmap chain when more than 1 (for Shade::Screen's glow)
};

// (Re)creates `target` at the given size; nothing when it already is. `mipmaps`: with a mipmap
// chain, rebuilt by end2D() (filtered trilinearly). `name` (a literal): what it is, for the count of
// targets (re)made by name (vr_memstats, and a developer line each time).
void ensureTarget(Target& target, int width, int height, bool mipmaps = false, const char* name = "target");
// Frees `target`'s texture and framebuffer (nothing when it has none); ensureTarget makes it again.
void releaseTarget(Target& target);

// How many times targets of each name were (re)made so far, as "name:count" words.
[[nodiscard]] za::String targetsMadeByName();

// Draws into `target` with the engine's 2D functions (the 2D pass's own blend and state), on a
// virtual screen of `virtualWidth` x `virtualHeight` covering it, until end2D() restores the 2D
// canvas and goes back to where the engine draws its 2D pass (the window: begin2D is used from it),
// and rebuilds its mipmaps if it has them.
void begin2D(const Target& target, int virtualWidth, int virtualHeight);
void end2D();

// The game directory changed (VR_OnGameDirChanged): Mod_ResetAll reuses the models' slots for other models, and the
// files are another game's; the gfx.wad pictures looked up by name (Draw_NewGame loaded them again).
void onGameDirChanged();

// 2D drawing between begin2D() and end2D(), in virtual screen coordinates (y down).
namespace draw2D
{
void color(const glm::vec4& rgba);                         // tints what follows
void fill(float x, float y, float w, float h, const glm::vec3& rgb);
void pic(float x, float y, const char* wadName, float scale = 1.f); // a gfx.wad picture
void text(float x, float y, float size, const char* text);  // the console font
} // namespace draw2D

// The engine's 2D pass (menus, console, HUD) redirected into `canvas`, cleared to transparent
// black; endCanvas() points the 2D pass back where the engine draws it (the window). With `mipmaps` the canvas has a
// mipmap chain, rebuilt by endCanvas(), sampled trilinearly with anisotropy and `lodBias` (negative: sharper).
void beginCanvas(Target& canvas, int width, int height, bool mipmaps = false, float lodBias = 0.f);
void endCanvas();

// Alpha blending for the 2D pass while it draws into the canvas: colours as usual, alpha built
// up as coverage (ONE, ONE_MINUS_SRC_ALPHA), so that the canvas holds premultiplied colours.
// False if the renderer cannot (the canvas is then only a little too transparent).
[[nodiscard]] bool applyCanvasBlend();

// Copies all of `target` into `image`, a texture of the same size (a runtime's swapchain image).
void copy(const Target& target, Texture image);

// RGBA8 colour textures: the mock backend's eye images (no data), or images such as the particle
// atlas (`rgba`, rows top first; linear filtering, clamped). `mipmaps`: with a full mipmap chain
// built from `rgba`, filtered trilinearly and anisotropically (the decals' atlas).
[[nodiscard]] Texture createTexture(int width, int height, const void* rgba = nullptr, bool mipmaps = false);
void destroyTexture(Texture texture);

// OpenGL only, for the module's own GL passes (vr_bloom.cpp, vr_lighting.cpp): a program from GLSL
// sources (no `fragment`: depth only), labelled `name`; 0, with a warning, if it does not build.
[[nodiscard]] unsigned glProgram(const char* vertex, const char* fragment, const char* name);

} // namespace qvr::gfx
