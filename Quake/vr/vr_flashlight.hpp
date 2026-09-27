// vr_flashlight.hpp -- the chest flashlight (vr_flashlight): a straight tactical torch (round 21; a
// right-angle one before) clipped to the chest on the off hand's side, pointing where the torso faces.
// A hand at it with the trigger switches it on or off; an empty hand's grip takes it, held in the
// fist like a real torch (the tube through the curled fingers, lighting along it: out of the thumb's
// side in the low grip, out of the little finger's in the overhead one; the upper face button, B or
// Y, turns it round in the fist); let go, it flies back to the chest on its retracting cord.
// Held near the gun in the other hand, B or Y clips it along the gun's barrel (under it, or beside a
// bulky gun): it lights where the gun aims until the free hand takes it off again (at the lamp, B or
// Y) or the gun leaves the hand (holstered, dropped, thrown, switched), when it goes back to the chest.
//
// Client-side only: its model is a VR view entity (progs/vrflashlight.mdl, make_flashlight.py) and
// its beam a spot light at the lens (lighting::dlightSpot: per pixel on the world and on models,
// with its own perspective shadow map), a faint wider spill cone and a faint glow at the lamp; and
// a soft cone of light in the air (vr_flashlight_beam), added onto each eye's scene.

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

enum class Button
{
    Trigger,
    Grip,
    Secondary // the upper face button: B on the right controller, Y on the left
};

// A controller's trigger, grip or upper face button pressed or released (vr_input.cpp). True when
// the flashlight takes it (switching it, holding it, turning it round in the fist, clipping it to a
// gun or taking it off): the game does not see it. A press it takes, it also takes the release of.
[[nodiscard]] bool button(int hand, Button b, bool pressed);

// After a Secondary press it took: true (once) when the flashlight went into `hand` whose grip the
// game had seen pressed (a hand on the gun's foregrip taking the lamp off): the game must see the
// grip let go, the flashlight has it now.
[[nodiscard]] bool tookGrip(int hand);

// A new game, a map started afresh (the map command, the menus) or a save loaded, not a level
// changed in the game: the flashlight switched off, on the chest.
void reset();

// Whether `hand` holds the flashlight: that hand does not force grab (its move tells the server,
// QVR_BUTTON_*HANDBUSY, and its aim beam is not drawn).
[[nodiscard]] bool holds(int hand);

// Round 21: while `hand` holds the flashlight, how open its finger `finger` (vr_view.cpp's: 1 the thumb .. 5 the
// little finger) stays at most, 0..1, so that the fist closes round the tube instead of through it (the grip limits
// held weapons have); -1 when it does not hold it.
[[nodiscard]] float fingerOpen(int hand, int finger);

} // namespace qvr::flashlight
