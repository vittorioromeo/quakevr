// vr_bullettime.hpp -- bullet time for play (ROUND21.md, "Slow motion: bullet time and Sandevistan"): a physical
// button on the wrist gadget (the inner of the two on its lower edge; or the bindable vr_bullettime) slows the world to
// vr_bullettime_scale for as long as its meter lasts (vr_bullettime_duration real seconds when full), then a cooldown,
// then the meter fills again (vr_bullettime_recharge). The gadget's screen shows the meter. While it runs, the eyes get
// its look (vr_bullettime_fx: desaturated, tinted, vignetted; never with the recording's vr_timescale). With
// vr_bullettime_sandevistan the player runs in its own time in the slowed world (vr_timescale.cpp).

#pragma once

#include <glm/glm.hpp>

namespace qvr::bullettime
{

void init(); // vr_bullettime

// VR_AdvanceTime (real seconds): the meter drains while on, the cooldown runs, the meter fills; the look eases.
void advance(double dt);

// VR_BeginFrame, after the input: the gadget's button pressed by the other hand's fingertip.
void frame();

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

// The gadget's button (world): its middle and the way its face points; false while the gadget isn't shown.
[[nodiscard]] bool button(glm::vec3& at, glm::vec3& out);

// Where `hand`'s place (hands::State::pos) must be for its fingertip to be on the button (vr_mock_hand_to ... button).
[[nodiscard]] bool buttonHandTarget(int hand, glm::vec3& out);

// Starts it (if the meter allows) or stops it: the button's and vr_bullettime's action.
void toggle();

} // namespace qvr::bullettime
