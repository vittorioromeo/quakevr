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

#include <glm/glm.hpp>

#include <cstddef>
#include <span>

namespace qvr::gfx
{

// A texture of the engine's renderer (for OpenGL, a texture name). 0 is none.
using Texture = unsigned;

struct Vertex
{
    glm::vec3 pos;
    glm::vec2 uv{0.f};
    glm::vec4 color{1.f};
};

enum class Shade
{
    Color,         // the vertex colour
    SoftEdge,      // the vertex colour, its alpha fading out with |uv| (1 - |uv|^2): lines, discs
    Texture,       // the texture, times the vertex colour
    TextureCutout, // the texture's colour times the vertex colour, opaque, where its alpha is at least 2/3 (the font)
};

enum class Blend
{
    Opaque,
    Alpha,         // colours with a separate alpha
    Premultiplied, // colours already multiplied by their alpha (the 2D canvas)
    Modulate,      // the scene times the colour (premultiplied: dst * (colour + 1 - alpha)), decals
};

struct State
{
    Shade shade{Shade::Color};
    Blend blend{Blend::Alpha};
    bool depthTest{true};
    bool depthWrite{false};
};

// Triangles (three vertices each), transformed by `mvp` to clip space. No culling.
void draw(std::span<const Vertex> triangles, const glm::mat4& mvp, const State& state, Texture texture = 0);

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
};

// (Re)creates `target` at the given size; nothing when it already is.
void ensureTarget(Target& target, int width, int height);

// Draws into `target` with the engine's 2D functions (the 2D pass's own blend and state), on a
// virtual screen of `virtualWidth` x `virtualHeight` covering it, until end2D() restores the 2D
// canvas and goes back to where the engine draws its 2D pass (the window: begin2D is used from it).
void begin2D(const Target& target, int virtualWidth, int virtualHeight);
void end2D();

// 2D drawing between begin2D() and end2D(), in virtual screen coordinates (y down).
namespace draw2D
{
void color(const glm::vec4& rgba);                         // tints what follows
void fill(float x, float y, float w, float h, const glm::vec3& rgb);
void pic(float x, float y, const char* wadName, float scale = 1.f); // a gfx.wad picture
void text(float x, float y, float size, const char* text);  // the console font
} // namespace draw2D

// The engine's 2D pass (menus, console, HUD) redirected into `canvas`, cleared to transparent
// black; endCanvas() points the 2D pass back where the engine draws it (the window).
void beginCanvas(Target& canvas, int width, int height);
void endCanvas();

// Alpha blending for the 2D pass while it draws into the canvas: colours as usual, alpha built
// up as coverage (ONE, ONE_MINUS_SRC_ALPHA), so that the canvas holds premultiplied colours.
// False if the renderer cannot (the canvas is then only a little too transparent).
[[nodiscard]] bool applyCanvasBlend();

// Copies all of `target` into `image`, a texture of the same size (a runtime's swapchain image).
void copy(const Target& target, Texture image);

// RGBA8 colour textures: the mock backend's eye images (no data), or images such as the particle
// atlas (`rgba`, rows top first; linear filtering, clamped).
[[nodiscard]] Texture createTexture(int width, int height, const void* rgba = nullptr);
void destroyTexture(Texture texture);

// OpenGL only, for the module's own GL passes (vr_bloom.cpp, vr_lighting.cpp): a program from GLSL
// sources (no `fragment`: depth only), labelled `name`; 0, with a warning, if it does not build.
[[nodiscard]] unsigned glProgram(const char* vertex, const char* fragment, const char* name);

} // namespace qvr::gfx
