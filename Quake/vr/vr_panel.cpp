// vr_panel.cpp -- Ironwail's 2D layer (menus, console, HUD) in the headset.
//
// While the eyes are rendered, the 2D pass draws into an offscreen canvas instead of the
// window; the canvas is then composited over the desktop mirror, and shown in each eye:
// - while a menu or the console is open, as a panel floating in front of the player;
// - in game, the status bar (the classic HUD's CANVAS_SBAR rectangle) attached to a hand, as
//   the old engine's VR_DrawSbar (vr_sbar_mode, vr_sbar_offset_*, vr_hud_scale), and the rest
//   (centre prints, notify lines) on a panel that follows the head.
// The screen-space crosshair is left out: in VR the hands aim.
// TODO VR: (P6) menu laser pointer.

#include "vr_cvars.hpp"
#include "vr_gadget.hpp"
#include "vr_hands.hpp"
#include "vr_main.hpp"
#include "vr_panel.hpp"

#include <glm/gtc/matrix_transform.hpp>

using namespace qvr;

namespace
{

// The quad's (0..1, 0..1) corners map to UvRect (u0, v0, u1, v1) of the canvas; texels inside
// Mask (same layout, empty when u1 <= u0) are left out.
constexpr const char* vertexShader = R"(#version 430
layout(location = 0) uniform mat4 MVP;
layout(location = 1) uniform vec4 UvRect;
out vec2 uv;
void main()
{
    const vec2 corners[6] = vec2[](vec2(0, 0), vec2(1, 0), vec2(1, 1), vec2(0, 0), vec2(1, 1), vec2(0, 1));
    vec2 c = corners[gl_VertexID];
    uv = mix(UvRect.xy, UvRect.zw, c);
    gl_Position = MVP * vec4(c, 0.0, 1.0);
}
)";

constexpr const char* fragmentShader = R"(#version 430
layout(binding = 0) uniform sampler2D Canvas;
layout(location = 2) uniform vec4 Mask;
in vec2 uv;
out vec4 color;
void main()
{
    if(all(greaterThan(uv, Mask.xy)) && all(lessThan(uv, Mask.zw)))
        discard;
    color = texture(Canvas, uv);
}
)";

constexpr glm::vec4 wholeCanvas{0.f, 0.f, 1.f, 1.f};
constexpr glm::vec4 noMask{0.f, 0.f, 0.f, 0.f};

GLuint program = 0;
GLuint canvasTexture = 0;
GLuint canvasFbo = 0;
int canvasWidth = 0;
int canvasHeight = 0;
bool drawingToCanvas = false;
bool stereoThisFrame = false;

// Panel placement, frozen when it appears: in front of the head, turning with the player.
bool panelWasVisible = false;
float panelYawOffset = 0.f;

// The in-game HUD panel follows the head, smoothly.
bool hudAnglesValid = false;
glm::vec2 hudAngles{0.f}; // pitch, yaw
double hudAnglesTime = 0.0;

// Crosshair setting, while the 2D pass draws into the canvas without it.
float savedCrosshair = 0.f;

[[nodiscard]] GLuint compile(GLenum type, const char* source)
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
        Con_Warning("VR: panel shader: %s\n", log);
    }
    return shader;
}

bool ensureProgram()
{
    if(program)
    {
        return true;
    }

    const GLuint vs = compile(GL_VERTEX_SHADER, vertexShader);
    const GLuint fs = compile(GL_FRAGMENT_SHADER, fragmentShader);
    program = GL_CreateProgramFunc();
    GL_AttachShaderFunc(program, vs);
    GL_AttachShaderFunc(program, fs);
    GL_LinkProgramFunc(program);
    GL_DeleteShaderFunc(vs);
    GL_DeleteShaderFunc(fs);

    GLint ok = 0;
    GL_GetProgramivFunc(program, GL_LINK_STATUS, &ok);
    if(!ok)
    {
        Con_Warning("VR: panel shader failed to link\n");
        GL_DeleteProgramFunc(program);
        program = 0;
        return false;
    }

    return true;
}

void ensureCanvas(int width, int height)
{
    if(canvasTexture && canvasWidth == width && canvasHeight == height)
    {
        return;
    }

    if(canvasTexture)
    {
        glDeleteTextures(1, &canvasTexture);
    }
    if(!canvasFbo)
    {
        GL_GenFramebuffersFunc(1, &canvasFbo);
    }

    glGenTextures(1, &canvasTexture);
    GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D, canvasTexture);
    GL_TexStorage2DFunc(GL_TEXTURE_2D, 1, GL_RGBA8, width, height);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    GL_BindFramebufferFunc(GL_FRAMEBUFFER, canvasFbo);
    GL_FramebufferTexture2DFunc(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, canvasTexture, 0);

    canvasWidth = width;
    canvasHeight = height;
}

// Draws the canvas as a quad; `mvp` maps the quad's (0..1, 0..1) to clip space. The canvas
// holds colours already multiplied by their alpha (2D was drawn over transparent black).
void drawCanvas(const glm::mat4& mvp, const glm::vec4& uvRect = wholeCanvas, const glm::vec4& mask = noMask)
{
    if(!canvasTexture || !ensureProgram())
    {
        return;
    }

    GL_UseProgram(program);
    GL_Uniform4fvFunc(1, 1, &uvRect[0]);
    GL_Uniform4fvFunc(2, 1, &mask[0]);
    GL_SetState(GLS_BLEND_ALPHA | GLS_NO_ZTEST | GLS_NO_ZWRITE | GLS_CULL_NONE | GLS_ATTRIBS(0));
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D, canvasTexture);
    GL_UniformMatrix4fvFunc(0, 1, GL_FALSE, &mvp[0][0]);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // what GLS_BLEND_ALPHA expects
}

// An opaque texture on a quad in the world, hidden behind what is in front of it (the gadget's
// screen: a hand can pass in front of it).
void drawSurface(GLuint texture, const glm::mat4& mvp)
{
    if(!texture || !ensureProgram())
    {
        return;
    }

    GL_UseProgram(program);
    GL_Uniform4fvFunc(1, 1, &wholeCanvas[0]);
    GL_Uniform4fvFunc(2, 1, &noMask[0]);
    GL_SetState(GLS_BLEND_OPAQUE | GLS_NO_ZWRITE | GLS_CULL_NONE | GLS_ATTRIBS(0));
    GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D, texture);
    GL_UniformMatrix4fvFunc(0, 1, GL_FALSE, &mvp[0][0]);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

[[nodiscard]] bool panelVisible()
{
    return key_dest != key_game || con_forcedup || scr_drawloading || cl.intermission;
}

[[nodiscard]] glm::mat4 viewProjection()
{
    glm::mat4 viewProj;
    memcpy(&viewProj[0][0], r_matviewproj, sizeof(r_matviewproj));
    return viewProj;
}

// The quad's model matrix: corner (0, 0) at `origin`, spanning `xAxis` and `yAxis`.
[[nodiscard]] glm::mat4 quad(const glm::vec3& origin, const glm::vec3& xAxis, const glm::vec3& yAxis)
{
    glm::mat4 model{1.f};
    model[0] = glm::vec4{xAxis, 0.f};
    model[1] = glm::vec4{yAxis, 0.f};
    model[2] = glm::vec4{0.f};
    model[3] = glm::vec4{origin, 1.f};
    return model;
}

// The status bar's rectangle in the canvas (u0, v0, u1, v1; v up), and how many of its 48
// rows are drawn. Empty without a classic status bar.
struct SbarRect
{
    glm::vec4 uv{0.f};
    float rows{0.f};
};

[[nodiscard]] SbarRect sbarRect()
{
    // As SCR_CalcRefdef's sb_lines, which is also 0 for a translucent status bar (it is drawn
    // all the same, only not given its own screen lines).
    SbarRect r;
    if(hudstyle != HUD_CLASSIC || cl.intermission || scr_viewsize.value >= 120.f || cl.qcvm.extfuncs.CSQC_DrawHud)
    {
        return r;
    }
    r.rows = scr_viewsize.value >= 110.f ? 24.f : 48.f;

    drawtransform_t t;
    Draw_GetCanvasTransform(CANVAS_SBAR, &t);

    const auto toUv = [](float ndc) { return (ndc + 1.f) * 0.5f; };
    r.uv = {toUv(t.offset[0]), toUv(48.f * t.scale[1] + t.offset[1]), toUv(320.f * t.scale[0] + t.offset[0]),
        toUv((48.f - r.rows) * t.scale[1] + t.offset[1])};
    return r;
}

// The status bar on a hand, placed as the old engine's VR_DrawSbar: model space is the status
// bar's pixels (x right, y down), scaled by vr_hud_scale.
void drawSbar(const hands::State& s, const SbarRect& r)
{
    const float scale = vr_hud_scale.value;
    glm::mat4 m{1.f};

    if(static_cast<int>(vr_sbar_mode.value) == 0) // main hand
    {
        const glm::vec3& rot = s.rot[HAND_MAIN];
        glm::vec3 fwd, right, up;
        hands::angleVectors(rot, fwd, right, up);

        m = glm::translate(m, s.pos[HAND_MAIN] - right * 5.f);
        m = glm::rotate(m, glm::radians(rot.y - 90.f), glm::vec3{0.f, 0.f, 1.f});
        m = glm::rotate(m, glm::radians(90.f + 45.f + rot.x), glm::vec3{-1.f, 0.f, 0.f});
        m = glm::translate(m, glm::vec3{-(320.f * scale / 2.f), 0.f, 10.f});
    }
    else // off hand
    {
        glm::vec3 fwd, right, up;
        hands::angleVectors(s.rot[HAND_OFF], fwd, right, up);

        glm::quat q = glm::quatLookAt(fwd, up);
        q = glm::rotate(q, vr_sbar_offset_pitch.value, glm::vec3{1.f, 0.f, 0.f});
        q = glm::rotate(q, vr_sbar_offset_yaw.value, glm::vec3{0.f, 1.f, 0.f});
        q = glm::rotate(q, vr_sbar_offset_roll.value, glm::vec3{0.f, 0.f, 1.f});

        m = glm::translate(m, s.pos[HAND_OFF]);
        m = m * glm::mat4_cast(glm::normalize(q));
        m = glm::translate(m, glm::vec3{vr_sbar_offset_x.value, vr_sbar_offset_y.value, vr_sbar_offset_z.value});
    }
    m = glm::scale(m, glm::vec3{scale});

    // Rows 48 - rows .. 48 of the status bar are drawn; the quad's v goes up the canvas.
    const glm::vec3 origin = m * glm::vec4{0.f, 48.f, 0.f, 1.f};
    const glm::vec3 xAxis = m * glm::vec4{320.f, 0.f, 0.f, 0.f};
    const glm::vec3 yAxis = m * glm::vec4{0.f, -r.rows, 0.f, 0.f};
    drawCanvas(viewProjection() * quad(origin, xAxis, yAxis), r.uv);
}

// Everything else of the in-game 2D layer, on a panel following the head.
void drawHud(const hands::State& s, const glm::vec4& mask)
{
    const glm::vec2 head{s.headAngles.x, s.headAngles.y};
    const float dt = static_cast<float>(CLAMP(0.0, realtime - hudAnglesTime, 0.1));
    hudAnglesTime = realtime;

    if(!hudAnglesValid)
    {
        hudAngles = head;
        hudAnglesValid = true;
    }
    else
    {
        const float t = 1.f - std::exp(-dt * 10.f);
        hudAngles.x += (head.x - hudAngles.x) * t;
        hudAngles.y += std::remainder(head.y - hudAngles.y, 360.f) * t;
    }

    glm::vec3 fwd, right, up;
    hands::angleVectors({hudAngles.x, hudAngles.y, 0.f}, fwd, right, up);

    const float height = 200.f * vr_menu_scale.value;
    const float width = height * static_cast<float>(canvasWidth) / canvasHeight;
    const glm::vec3 centre = s.head + fwd * vr_menu_distance.value;
    const glm::vec3 corner = centre - right * (width * 0.5f) - up * (height * 0.5f);

    drawCanvas(viewProjection() * quad(corner, right * width, up * height), wholeCanvas, mask);
}

GLuint panelFbo = 0;

// glBlendFuncSeparate (GL 1.4), which Ironwail does not load.
using BlendFuncSeparateFn = void(APIENTRY*)(GLenum, GLenum, GLenum, GLenum);
BlendFuncSeparateFn blendFuncSeparate = nullptr;

// The canvas into the backend's panel image, when it has one.
void copyToRuntimePanel()
{
    Backend* be = backend();
    const unsigned image = be ? be->acquirePanelImage(canvasWidth, canvasHeight) : 0;
    if(!image)
    {
        return;
    }

    if(!panelFbo)
    {
        GL_GenFramebuffersFunc(1, &panelFbo);
    }

    GLint drawFbo = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFbo);

    GL_BindFramebufferFunc(GL_DRAW_FRAMEBUFFER, panelFbo);
    GL_FramebufferTexture2DFunc(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, image, 0);
    GL_BindFramebufferFunc(GL_READ_FRAMEBUFFER, canvasFbo);
    GL_BlitFramebufferFunc(0, 0, canvasWidth, canvasHeight, 0, 0, canvasWidth, canvasHeight, GL_COLOR_BUFFER_BIT,
        GL_NEAREST);
    GL_FramebufferTexture2DFunc(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);

    GL_BindFramebufferFunc(GL_READ_FRAMEBUFFER, static_cast<GLuint>(drawFbo));
    GL_BindFramebufferFunc(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(drawFbo));

    be->releasePanelImage();
}

} // namespace

namespace qvr::panel
{

void setStereoThisFrame(bool stereo)
{
    stereoThisFrame = stereo;
}

void drawInEye(const hands::State& s)
{
    const bool visible = panelVisible();
    if(visible && !panelWasVisible)
    {
        panelYawOffset = s.headAngles.y - hands::playSpaceYaw();
    }
    panelWasVisible = visible;

    if(!canvasTexture)
    {
        return;
    }

    if(!visible)
    {
        // In game: the status bar on a hand (or the wrist gadget's screen instead), the rest in
        // front of the head.
        const SbarRect sbar = sbarRect();
        if(gadget::active())
        {
            const gadget::Pose& g = gadget::pose();
            if(g.valid)
            {
                glm::vec3 corner;
                glm::vec2 size;
                gadget::screenRect(corner, size);
                const glm::vec3 origin = g.origin + g.axes * (corner * g.scale);
                drawSurface(gadget::screenTexture(),
                    viewProjection() * quad(origin, g.axes[0] * (size.x * g.scale), g.axes[1] * (size.y * g.scale)));
            }
        }
        else if(sbar.rows > 0.f)
        {
            drawSbar(s, sbar);
        }
        drawHud(s, sbar.rows > 0.f ? sbar.uv : noMask);
        return;
    }
    hudAnglesValid = false;

    const float yaw = panelYawOffset + hands::playSpaceYaw();
    glm::vec3 fwd, right, up;
    hands::angleVectors({0.f, yaw, 0.f}, fwd, right, up);

    const float height = 200.f * vr_menu_scale.value;
    const float width = height * static_cast<float>(canvasWidth) / canvasHeight;
    const glm::vec3 centre = s.head + fwd * vr_menu_distance.value;
    const glm::vec3 corner = centre - right * (width * 0.5f) - up * (height * 0.5f);

    drawCanvas(viewProjection() * quad(corner, right * width, up * height));
}

} // namespace qvr::panel

// While drawing into the canvas, alpha blending must also build up the alpha channel as
// "covered so far" (ONE, ONE_MINUS_SRC_ALPHA): with the colour's blend applied to alpha too, a
// translucent pixel would store alpha squared, and the canvas would let too much through
// where it is composited (in the eyes, on the runtime's panel).
extern "C" int VR_CanvasBlend()
{
    if(!drawingToCanvas)
    {
        return 0;
    }
    if(!blendFuncSeparate)
    {
        blendFuncSeparate = reinterpret_cast<BlendFuncSeparateFn>(SDL_GL_GetProcAddress("glBlendFuncSeparate"));
        if(!blendFuncSeparate)
        {
            return 0;
        }
    }
    blendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    return 1;
}

extern "C" void VR_Begin2D()
{
    // With VR active the 2D layer always goes to the canvas: into the eyes when they were
    // rendered, else onto the runtime's panel (menus before a map, the console, loading).
    drawingToCanvas = stereoThisFrame || vrActive();
    if(!drawingToCanvas)
    {
        return;
    }

    savedCrosshair = crosshair.value;
    crosshair.value = 0.f;

    ensureCanvas(vid.width, vid.height);
    GL_ResetState(); // re-applies the blend with VR_CanvasBlend in effect
    GL_BindFramebufferFunc(GL_FRAMEBUFFER, canvasFbo);
    glViewport(0, 0, vid.width, vid.height);
    glClearColor(0.f, 0.f, 0.f, 0.f);
    glClear(GL_COLOR_BUFFER_BIT);
}

extern "C" void VR_End2D()
{
    if(!drawingToCanvas)
    {
        return;
    }
    drawingToCanvas = false;
    GL_ResetState(); // back to Ironwail's usual blend
    crosshair.value = savedCrosshair;

    gadget::renderScreen(); // shown in the eyes next frame, as the canvas

    // Back to the window, with the 2D layer over the mirrored eye.
    GL_BindFramebufferFunc(GL_FRAMEBUFFER, GL_NeedsPostprocess() ? framebufs.composite.fbo : 0);
    glViewport(glx, gly, glwidth, glheight);

    glm::mat4 toNdc{1.f};
    toNdc[0][0] = 2.f;
    toNdc[1][1] = 2.f;
    toNdc[3] = glm::vec4{-1.f, -1.f, 0.f, 1.f};
    drawCanvas(toNdc);

    if(!stereoThisFrame)
    {
        copyToRuntimePanel();
    }
    stereoThisFrame = false;
}
