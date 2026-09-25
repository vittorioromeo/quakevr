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
#include "vr_engine.hpp"
#include "vr_gadget.hpp"
#include "vr_gfx.hpp"
#include "vr_hands.hpp"
#include "vr_main.hpp"
#include "vr_panel.hpp"
#include "vr_profile.hpp"

#include <glm/gtc/matrix_transform.hpp>

#include <utility>
#include <vector>

using namespace qvr;

namespace
{

constexpr glm::vec4 wholeCanvas{0.f, 0.f, 1.f, 1.f};
constexpr glm::vec4 noMask{0.f, 0.f, 0.f, 0.f};

gfx::Target canvas;
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

// Draws the canvas as a quad: `mvp` maps the quad's (0..1, 0..1) to clip space, and its corners
// to `uvRect` (u0, v0, u1, v1) of the canvas; texels inside `mask` (same layout, empty when
// u1 <= u0) are left out. The canvas holds colours already multiplied by their alpha (2D was
// drawn over transparent black).
void drawCanvas(const glm::mat4& mvp, const glm::vec4& uvRect = wholeCanvas, const glm::vec4& mask = noMask)
{
    if(!canvas.texture)
    {
        return;
    }

    static std::vector<gfx::Vertex> vertices;
    vertices.clear();

    const glm::vec2 uv0{uvRect.x, uvRect.y};
    const glm::vec2 uv1{uvRect.z, uvRect.w};
    const auto rect = [&](float x0, float y0, float x1, float y1) {
        if(x1 <= x0 || y1 <= y0)
        {
            return;
        }
        for(const auto& [x, y] : {std::pair{x0, y0}, {x1, y0}, {x1, y1}, {x0, y0}, {x1, y1}, {x0, y1}})
        {
            vertices.push_back({{x, y, 0.f}, glm::mix(uv0, uv1, glm::vec2{x, y})});
        }
    };

    if(mask.z <= mask.x)
    {
        rect(0.f, 0.f, 1.f, 1.f);
    }
    else
    {
        // The quad around the mask's hole (in the quad's coordinates).
        const glm::vec2 m0 = (glm::vec2{mask.x, mask.y} - uv0) / (uv1 - uv0);
        const glm::vec2 m1 = (glm::vec2{mask.z, mask.w} - uv0) / (uv1 - uv0);
        const glm::vec2 lo = glm::clamp(glm::min(m0, m1), 0.f, 1.f);
        const glm::vec2 hi = glm::clamp(glm::max(m0, m1), 0.f, 1.f);
        rect(0.f, 0.f, 1.f, lo.y);
        rect(0.f, hi.y, 1.f, 1.f);
        rect(0.f, lo.y, lo.x, hi.y);
        rect(hi.x, lo.y, 1.f, hi.y);
    }

    gfx::draw(vertices, mvp,
        {.shade = gfx::Shade::Texture, .blend = gfx::Blend::Premultiplied, .depthTest = false, .depthWrite = false},
        canvas.texture);
}

// An opaque texture on a quad in the world (`mvp` as drawCanvas's), hidden behind what is in
// front of it: the gadget's screen, which a hand can pass in front of.
void drawSurface(gfx::Texture texture, const glm::mat4& mvp)
{
    if(!texture)
    {
        return;
    }

    const gfx::Vertex c[4] = {{{0.f, 0.f, 0.f}, {0.f, 0.f}}, {{1.f, 0.f, 0.f}, {1.f, 0.f}}, {{1.f, 1.f, 0.f}, {1.f, 1.f}},
        {{0.f, 1.f, 0.f}, {0.f, 1.f}}};
    const gfx::Vertex quad[6] = {c[0], c[1], c[2], c[0], c[2], c[3]};
    gfx::draw(quad, mvp, {.shade = gfx::Shade::Texture, .blend = gfx::Blend::Opaque, .depthTest = true, .depthWrite = false},
        texture);
}

[[nodiscard]] bool panelVisible()
{
    return key_dest != key_game || con_forcedup || scr_drawloading || cl.intermission;
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

// The canvas on a panel vr_menu_distance in front of the head, the way `angles` look (`mask` as
// drawCanvas's): the menus' and the in-game HUD's.
void drawFacing(const hands::State& s, const glm::vec3& angles, const glm::vec4& mask = noMask)
{
    glm::vec3 fwd, right, up;
    hands::angleVectors(angles, fwd, right, up);

    const float height = 200.f * vr_menu_scale.value;
    const float width = height * static_cast<float>(canvas.width) / canvas.height;
    const glm::vec3 centre = s.head + fwd * vr_menu_distance.value;
    const glm::vec3 corner = centre - right * (width * 0.5f) - up * (height * 0.5f);

    drawCanvas(gfx::sceneViewProjection() * quad(corner, right * width, up * height), wholeCanvas, mask);
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
    drawCanvas(gfx::sceneViewProjection() * quad(origin, xAxis, yAxis), r.uv);
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

    drawFacing(s, {hudAngles.x, hudAngles.y, 0.f}, mask);
}

// The canvas into the backend's panel image, when it has one.
void copyToRuntimePanel()
{
    Backend* be = backend();
    const unsigned image = be ? be->acquirePanelImage(canvas.width, canvas.height) : 0;
    if(!image)
    {
        return;
    }
    gfx::copy(canvas, image);
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
    QVR_GPU_PROFILE("hud panel");
    const bool visible = panelVisible();
    if(visible && !panelWasVisible)
    {
        panelYawOffset = s.headAngles.y - hands::playSpaceYaw();
    }
    panelWasVisible = visible;

    if(!canvas.texture)
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
                const glm::mat4 model = quad(origin, g.axes[0] * (size.x * g.scale), g.axes[1] * (size.y * g.scale));
                drawSurface(gadget::screenTexture(), gfx::sceneViewProjection() * model);
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

    drawFacing(s, {0.f, panelYawOffset + hands::playSpaceYaw(), 0.f});
}

} // namespace qvr::panel

// While drawing into the canvas, alpha blending must also build up the alpha channel as
// "covered so far" (ONE, ONE_MINUS_SRC_ALPHA): with the colour's blend applied to alpha too, a
// translucent pixel would store alpha squared, and the canvas would let too much through
// where it is composited (in the eyes, on the runtime's panel).
extern "C" int VR_CanvasBlend()
{
    return drawingToCanvas && gfx::applyCanvasBlend();
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

    gfx::beginCanvas(canvas, vid.width, vid.height);
}

extern "C" void VR_End2D()
{
    if(!drawingToCanvas)
    {
        return;
    }
    drawingToCanvas = false;
    crosshair.value = savedCrosshair;

    // Back to the window, with the 2D layer over the mirrored eye.
    gfx::endCanvas();
    gadget::renderScreen(); // shown in the eyes next frame, as the canvas

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
