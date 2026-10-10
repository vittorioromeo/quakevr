// vr_touch.hpp -- the touchscreen: the other hand's index fingertip on a screen in the world (the wrist gadget's, or a
// floating panel) as touches, and a small immediate-mode widget kit (buttons, a drag stick) drawn over the screen's
// image. Its first user is Doom on the gadget (vr_doom.cpp); docs/vr-port/DOOM.md, "The touchscreen", says how to use it
// for another app.
//
// Each frame an app that wants touches asks for them (request: the gadget's screen, or a panel of its own, and the
// screen's size in its pixels); the view tests the fingertip against that screen right after it places the gadget
// (viewFrame, from vr_view.cpp's gadgetTouches: the hands and the gadget as drawn this frame), and the app reads the
// result in its next frame (contact()). No request: nothing is tested, nothing costs.
//
// A touch: the fingertip (gearlights::pressingFingertip: the drawn index fingertip, tuned by vr_gadget_fingertip_*)
// within vr_touch_press cm of the screen's face (or a little through it, down to vr_touch_depth), over the screen
// (vr_touch_margin round its edge), having come from in front of it (a finger poked through from behind, or resting
// there as the screen moves to it, never touches); it ends when the fingertip is vr_touch_release cm off the face (or
// past the depth, or off the edge). A new touch no sooner than vr_touch_debounce s after the last ended. Pressing
// through the face by vr_touch_hard cm is a hard press (a second "button" under a single finger: Doom's fire while
// steering). A touch's start buzzes the touching hand (vr_touch_haptic). Within vr_touch_hover cm in front: hovering
// (the widgets light up under it, a cursor shows where it is).
//
// Priority on the gadget: while an app takes the gadget's screen (requests it), the screen tap (bullet time,
// vr_bullettime_tap) is off: a touch is the app's; the gadget's side button (the gear lights) keeps working, it is off
// the screen.

#pragma once

#include "vr_hands.hpp"

#include <glm/glm.hpp>

namespace qvr::touch
{

// A flat screen in the world: its middle, unit right, up and out (towards the viewer), half its width and height (units).
struct Surface
{
    glm::vec3 centre{0.f};
    glm::vec3 right{1.f, 0.f, 0.f};
    glm::vec3 up{0.f, 0.f, 1.f};
    glm::vec3 normal{0.f, 1.f, 0.f};
    glm::vec2 halfSize{0.f};
};

// What an app wants touched this frame: the gadget's screen, or `panel`; its size in the app's pixels (touch positions
// are in them, from the top left).
struct Request
{
    bool gadget{true};
    Surface panel;
    glm::vec2 pixels{320.f, 200.f};
};
void request(const Request& r);

// Whether an app asked for the gadget's screen this frame or the last (the screen tap gives way).
[[nodiscard]] bool takesGadgetScreen();

// The fingertip against the requested screen, as last tested.
struct Contact
{
    bool valid{false};     // a screen and a fingertip were there
    bool hover{false};     // in front of the screen, within vr_touch_hover (not touching)
    bool down{false};      // touching
    bool pressed{false};   // the touch began at this test
    bool released{false};  // it ended at this test
    bool hard{false};      // touching, pressed through by vr_touch_hard or more
    glm::vec2 pos{0.f};    // where (the app's pixels, from the top left; clamped to the screen)
    glm::vec2 downPos{0.f};// where the touch began
    float height{0.f};     // cm in front of the face (negative: through it)
    double downAt{-1.0};   // realtime the touch began
    int serial{0};         // tests so far (an app sees each once: Ui::begin)
};
[[nodiscard]] const Contact& contact();

// Once a frame, from the view, after the gadget is placed (vr_view.cpp gadgetTouches).
void viewFrame(const hands::State& s);

// vr_mock_hand_to <hand> touch <x> <y> [<cm>]: where `hand` must be for its fingertip to be `cm` in front of the
// requested screen at its pixel (x, y). False when none is requested or shown.
[[nodiscard]] bool handTarget(int hand, float x, float y, float cm, glm::vec3& out);

// The requested screen, as last tested (false: none).
[[nodiscard]] bool lastSurface(Surface& out);

// ---- Widgets ---------------------------------------------------------------------------------------------------------
//
// Immediate mode: each frame the app begins the frame's widgets with the latest contact, declares each (its id, its
// box in the app's pixels) and gets its state back; the touch is captured by the widget it began on until it ends (a
// finger sliding off a held button keeps it held; a stick follows it anywhere). The widgets declared are drawn by
// draw(), over the app's image in its 2D pass (gfx::begin2D on the app's target, its pixels as the virtual screen).

struct Rect
{
    float x{0.f}, y{0.f}, w{0.f}, h{0.f};
    [[nodiscard]] bool contains(glm::vec2 p) const { return p.x >= x && p.x < x + w && p.y >= y && p.y < y + h; }
};

struct Button
{
    bool held{false};     // the touch is on it (began on it, not yet ended)
    bool pressed{false};  // it began on it now
    bool released{false}; // it ended now (it began on it)
    bool clicked{false};  // ended now over it (a tap)
    bool hover{false};
};

struct Stick
{
    bool held{false};
    glm::vec2 value{0.f}; // -1 .. 1 each way (x right, y down), from where the touch began
    bool hard{false};     // pressed hard while held
};

enum class Look
{
    Normal,
    On,      // a toggle that is on
    Accent,  // the main action (fire)
    Quiet,   // only an outline until hovered (a zone)
};

class Ui
{
public:
    // The frame's widgets begin; `alpha` how strongly they are drawn (0: not at all; touches still count).
    void begin(const Contact& c, glm::vec2 pixels, float alpha);
    Button button(int id, const Rect& r, const char* label, Look look = Look::Normal);
    // A drag stick in `zone`: its value is the touch's offset from where it began over `radius` pixels.
    Stick stick(int id, const Rect& zone, float radius, const char* label);
    // A touch that began on no widget: where it is (the app's own: a tap on its image).
    [[nodiscard]] bool loose(Button& out) const;
    void draw() const; // in the app's 2D pass

    [[nodiscard]] int active() const { return activeId; }

private:
    struct Drawn
    {
        Rect r;
        const char* label{nullptr};
        Look look{Look::Normal};
        bool held{false}, hover{false}, stick{false};
        glm::vec2 from{0.f}, to{0.f}; // a held stick's start and the touch
    };
    static constexpr int maxDrawn = 32;
    Drawn drawn[maxDrawn];
    int count{0};
    Contact c;
    glm::vec2 size{320.f, 200.f};
    float alpha{1.f};
    int activeId{-1};   // the widget the touch began on (-1 none; 0 none of them: loose)
    int seen{-1};       // the contact's serial last begun with
    bool fresh{false};  // this frame's contact is new
    void add(const Drawn& d);
};

} // namespace qvr::touch
