// vr_gearlights.hpp -- the light of the player's own gear, and the wrist gadget's side button that turns it down (ROUND21.md,
// "The gadget's side button: gear lights"). The button on the gadget's lower edge (the inner one), pressed by the other
// hand's fingertip (vr_gadget_button; or the bindable vr_gear_lights_toggle), clicks and toggles vr_gear_lights: off, the
// lights the gadget's and the guns' ammo screens cast (which the stealth AI's light on the player counts: vr_stealth.cpp)
// and their glows fall to vr_gear_lights_dim, their faces (text, the hologram) to vr_gear_lights_screen_dim, for
// sneaking about in the dark. The flashlight keeps its own switch. The button's hit volume can be moved and sized
// (vr_gadget_button_x/y/z, vr_gadget_button_size) and shown (vr_debug_gadget_button); a press after one is ignored for
// vr_gadget_button_cooldown.

#pragma once

#include <glm/glm.hpp>

namespace qvr::hands
{
struct State;
}

namespace qvr::gearlights
{

void init(); // vr_gear_lights_toggle, vr_gear_lights_info

// VR_BeginFrame: the lights' level eased.
void frame();

// The view (VR_SetupViewEntities), once a frame, right after the gadget is placed: the button pressed by the other
// hand's fingertip, both as drawn this frame (`s`: the hands as the view draws them; after the move and the turn).
void viewFrame(const hands::State& s);

// The share of their light the gear's screens cast and glow with now (1 on .. vr_gear_lights_dim off, eased): their
// dynamic lights and their glows.
[[nodiscard]] float light();

// The share of their brightness the gear's screens' faces show now (1 on .. vr_gear_lights_screen_dim off, eased).
[[nodiscard]] float screen();

// Toggles the lights with the button's click and tick (`hand`: the hand that pressed it, or -1).
void toggle(int hand);

// The button's hit volume (world): its middle, the way its face points (the cut: only from its side) and its radius
// (units); false while the gadget isn't shown.
[[nodiscard]] bool button(glm::vec3& at, glm::vec3& out, float& radius);

// Where `hand`'s place (hands::State::pos) must be for its fingertip to be on the button, or `units` off its face
// (vr_mock_hand_to ... button [<units>]).
[[nodiscard]] bool buttonHandTarget(int hand, float units, glm::vec3& out);

// vr_debug_gadget_button: the button's hit volume (and with 2 the screen tap's zone) drawn this frame, as the view tests
// them (after the gadget is placed: `s` the hands as drawn).
void debugDraw(const hands::State& s);

} // namespace qvr::gearlights
