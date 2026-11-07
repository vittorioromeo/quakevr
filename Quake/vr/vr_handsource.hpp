// vr_handsource.hpp -- where each hand's motion comes from this frame, and the one rule that decides which motion the
// motion-triggered gestures read.
//
// A hand moves because the player moved a controller (tracked), because the game moved it for him (synthesized: the
// flat-screen hands following the mouse look, the wall tilt raising a gun out of a wall, a lean, a slide, recoil, any
// camera-derived motion), or because the game plays a motion for an explicit button action whose rules are the gesture's
// own (an action: flat-screen V's punch, which the melee rules judge as a real blow). A gesture that fires on motion (a
// melee swing, a slap, a bash or a shove by the hands, the revolver's cylinder flick, the super shotgun's flick and pry,
// the flashlight's flick, a flick of the force grab, the velocity-driven reload tricks) must never fire on synthesized
// motion: a mouse flick is not a swing. Buttons (V, reload, the shove key) are explicit and not gestures.
//
// The layer:
//   - hands::update calls beginFrame before the hands are placed: every hand starts the frame Tracked in the headset,
//     Synthesized otherwise (flat-screen hands are made up from the view).
//   - Whatever moves a hand marks it (mark(h, Source::Synthesized, "lean")): the strongest mark wins, Action over
//     Synthesized over Tracked. A feature that adds camera-derived or made-up motion to a hand (lean, slide, wall
//     tilt, recoil) marks the hands it moves, every frame it moves them, before vr_client.cpp builds the move.
//   - gestures(h) is the rule (Tracked or Action). Client-side detectors ask it (vr_flick.cpp, vr_flashlight.cpp,
//     vr_chainsaw.cpp); the move carries it to the server (vr_client.cpp handMove: a hand that fails it sends no
//     velocity, and its bit in .handsynth), where QC VR_HandMotionTracked is the same rule for the server's detectors
//     (vr_melee.qc VR_Melee_Track, vr_revolver.qc VR_Revolver_Flick; the velocity-driven ones read the zero velocity).
//   - vr_handsource_debug 1 prints each hand's source when it changes (Debug > Flatscreen Play).
//
// docs/vr-port/ROUND24.md, "Hand motion source".

#pragma once

#include "Zancle/Base/IntTypes.hpp"

namespace qvr::handsource
{

enum class Source : za::U8
{
    Tracked = 0,     // a controller's own motion
    Synthesized = 1, // made by the game or derived from the camera: never a gesture
    Action = 2,      // the game's motion for an explicit button action, judged by the gesture rules (V's punch)
};

// hands::update, before the hands are placed: every hand Tracked (a headset) or Synthesized (the flat screen).
void beginFrame(bool tracked);

// Something moved hand `h` this frame (`why`: a short name for vr_handsource_debug, a string literal). The strongest
// mark of the frame is kept: Action > Synthesized > Tracked.
void mark(int h, Source s, const char* why);

// This frame's source of hand `h`'s motion, and the mark that set it.
[[nodiscard]] Source source(int h);
[[nodiscard]] const char* why(int h);

// THE rule: whether motion-triggered gestures may read hand `h`'s motion this frame (Tracked or Action).
[[nodiscard]] bool gestures(int h);

// The move's bits for the server (.handsynth): 1 the off hand's motion is not for gestures, 2 the main hand's.
[[nodiscard]] za::U8 moveBits();

[[nodiscard]] const char* name(Source s);

} // namespace qvr::handsource
