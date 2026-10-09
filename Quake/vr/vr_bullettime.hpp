// vr_bullettime.hpp -- bullet time for play (ROUND21.md, "Slow motion: bullet time and Sandevistan"): a hard,
// deliberate tap on the wrist gadget's screen, by the other hand or the butt of the gun it holds (or the bindable
// vr_bullettime), slows the world to vr_bullettime_scale for as long as its meter lasts (vr_bullettime_duration real
// seconds when full), then a cooldown, then the meter fills again (vr_bullettime_recharge). The gadget's screen shows the
// meter. While it runs, the eyes get its look (vr_bullettime_fx: desaturated, tinted, vignetted; never with the
// recording's vr_timescale). With vr_bullettime_sandevistan the player runs in its own time in the slowed world
// (vr_timescale.cpp). The gadget's side button is the gear lights' (vr_gearlights.hpp).

#pragma once

#include <glm/glm.hpp>

namespace qvr::hands
{
struct State;
}

namespace qvr::bullettime
{

void init(); // vr_bullettime

// VR_AdvanceTime (real seconds): the meter drains while on, the cooldown runs, the meter fills; the look eases.
void advance(double dt);

// VR_BeginFrame (vr_debug_gadget_button 3: where the tapping hand is from the screen at the frame's start, for the view's
// check: frameStartOffset).
void frame();

// The view (VR_SetupViewEntities), once a frame, right after the gadget is placed: the gadget's screen tapped hard by the
// other hand or its gun's butt (vr_bullettime_tap), tested against the gadget and the hands as drawn this frame (`s`: the
// hands as the view draws them; after the move and the turn, on lifts alike), never a frame behind or ahead.
void viewFrame(const hands::State& s);

// vr_debug_gadget_button 3: the screen's middle from the tapping hand's place at the frame's start (frame); false: none.
[[nodiscard]] bool frameStartOffset(glm::vec3& out);

// How far `hand` (hands::State::pos) moved since the view tested the tap (the player moving and turning): the mock's
// targets (tapHandTarget, gearlights::buttonHandTarget) are taken from the gadget as drawn then.
[[nodiscard]] glm::vec3 movedSinceView(const hands::State& s, int hand);

// The time scale bullet time asks for: vr_bullettime_scale while on, else 1.
[[nodiscard]] float scale();

// On, with the player in its own time (vr_bullettime_sandevistan).
[[nodiscard]] bool sandevistan();

struct Meter
{
    bool enabled{false}; // vr_bullettime_enabled, a local single-player game
    bool active{false};
    bool cooling{false}; // after it ran: no start yet
    bool ready{false};   // a press would start it
    float level{1.f};    // 0 .. 1
};
[[nodiscard]] Meter meter();

// The look's strength now (0 .. 1: eased in and out, times vr_bullettime_fx), and its desaturation, vignette and tint.
struct Look
{
    float strength{0.f};
    float desaturate{0.f};
    float vignette{0.f};
    glm::vec3 tint{1.f};
};
[[nodiscard]] Look look();

// The gadget's screen (world): its middle on its face, its right, up and normal (out of it), its half width and height
// (units); false while the gadget isn't shown.
struct Screen
{
    glm::vec3 centre{0.f};
    glm::vec3 right{1.f, 0.f, 0.f};
    glm::vec3 up{0.f, 1.f, 0.f};
    glm::vec3 normal{0.f, 0.f, 1.f};
    glm::vec2 halfSize{0.f};
};
[[nodiscard]] bool screen(Screen& out);

// The screen tap's zone: the screen moved (vr_bullettime_tap_x/y/z, cm along its right, up and normal) and sized
// (vr_bullettime_tap_width, _height: shares of its own; vr_bullettime_tap_margin is added round it as it is tested).
[[nodiscard]] bool tapZone(Screen& out);

// The striking points of `hand` that tap the screen (its palm's middle; holding a gun, with vr_bullettime_tap_butt, the
// gun's butt): their count (1 or 2).
[[nodiscard]] int strikingPoints(const hands::State& s, int hand, glm::vec3 (&out)[2]);

// A screen tap under way (coming at the screen fast) or one that counted half a second ago: the side button waits.
[[nodiscard]] bool tapping();

// Where `hand`'s place (hands::State::pos) must be for its striking point (its middle, hands::palmPoint; or with `butt`,
// its gun's butt: view::heldWeaponButt) to be `cm` over the screen's middle and `sideCm` along its right
// (vr_mock_hand_to ... screen <cm> [<side cm>], screenbutt: steps along its normal make a tap, along its right a swing
// across). False with no gadget shown (or no gun, for `butt`).
[[nodiscard]] bool tapHandTarget(int hand, float cm, float sideCm, bool butt, glm::vec3& out);

// vr_input, a controller's stick pressed (`now`) or let go: true if bullet time takes it (vr_bullettime_trigger 1: the
// left stick, HAND_OFF; 2: the right, HAND_MAIN; in the game only), so its key (LTHUMB, RTHUMB) never reaches the game.
// A release is taken when its press was.
[[nodiscard]] bool stickPress(int hand, bool now);

// Starts it (if the meter allows) or stops it: the screen tap's and vr_bullettime's action.
void toggle();

} // namespace qvr::bullettime
