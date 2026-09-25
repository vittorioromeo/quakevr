// vr_flashlight.hpp -- the chest flashlight (vr_flashlight), after The Walking Dead VR's: a
// right-angle torch clipped to the chest on the off hand's side, lighting where the torso faces.
// A hand at it with the trigger switches it on or off; an empty hand's grip takes it, and it then
// lights where the hand points; let go, it flies back to the chest on its retracting cord.
//
// Client-side only: its model is a VR view entity (progs/vrflashlight.mdl, make_flashlight.py) and
// its beam a few dynamic lights (there is no spotlight): a pool of light where the beam lands, a
// dimmer one along the way, a faint glow at the lamp; and a soft cone of light in the air
// (vr_flashlight_beam), added onto each eye's scene.

#pragma once

#include "vr_engine.hpp"
#include "vr_hands.hpp"
#include "vr_view.hpp"

namespace qvr::flashlight
{

void init();

// Once per frame, as the view is set up: moves the lamp, places its model in `ve` and lights its
// beam.
void setupView(const hands::State& s, view::ViewEntity& ve);

// The visible beam, in each eye's scene after the translucent pass (VR_DrawSceneTranslucent):
// depth-tested, added onto the scene.
void drawTranslucent();

// A controller's trigger or grip pressed or released (vr_input.cpp). True when the flashlight
// takes it (switching it, holding it): the game does not see it. A press it takes, it also takes
// the release of.
[[nodiscard]] bool button(int hand, bool grip, bool pressed);

} // namespace qvr::flashlight
