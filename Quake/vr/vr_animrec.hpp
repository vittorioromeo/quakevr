// vr_animrec.hpp -- the animation recorder (docs/vr-port/ANIMATIONS.md): motions recorded in the headset (a melee
// strike, a reload) to be played later on the flat-screen player's hands. Separate from the motion recorder
// (vr_motion.cpp, which records takes for the melee's tests); it shares its record button choices and the countdown
// (vr_recbutton.hpp).
//
// Recording: Debug > Animation Recorder (Start/Stop), vr_ar_record [name] [seconds] / vr_ar_stop, or, armed
// (vr_ar_armed), the record button (vr_ar_button). A countdown (vr_ar_countdown) and then every host frame: the head
// and each hand relative to the head and to the body, the fingers, grip and trigger, the weapon in each hand, and the
// weapons' events (a shot, the pump, a reload's steps). Saved as quakevr/anims/<name>.anim (text).
// Smoothing: vr_ar_smooth [name] writes <name>.smooth.anim beside it (Misc/quakevr/anim_smooth.py does it offline).
// Preview: vr_ar_play [name] plays one on the flat-screen hands (or as a ghost in the headset), blended in and out of
// the game's own pose, with the vr_ar_play_* tweaks.

#pragma once

#include "vr_backend.hpp"


namespace qvr::hands
{
struct State;
}

namespace qvr::animrec
{

void init();     // commands
void shutdown(); // a recording going on is saved

// VR_BeginFrame, after the frame's world texts were cleared (the headset only): the countdown and REC in view, the
// preview's ghost.
void frame();

// The end of a host frame: the countdown, a recording's frame, the preview's clock.
void hostFrameEnd();

// A button of `hand` pressed or let go: true if the recorder takes it (armed, its button: vr_ar_button).
[[nodiscard]] bool button(int hand, bool HandInput::*which, bool pressed);

// A weapon's event while recording (else nothing): `hand` 0 off, 1 main, -1 both or neither. From the engine (a shot,
// the auto pump, a magazine's rounds going up) and the QC (the builtin animevent: a reload's steps).
void event(const char* kind, int hand, const char* detail);

// Flat-screen play: the preview's pose put on the hands (vr_hands.cpp, after flatplay::poseHands), blended with it.
void applyPreview(hands::State& s);

[[nodiscard]] bool recording();  // counting down or recording
[[nodiscard]] bool previewing();

// For the menu (Debug > Animation Recorder).
void toggleRecording();
void playLast();
void smoothLast();
void stopPreview();
[[nodiscard]] const char* statusLine();
[[nodiscard]] const char* lastLine();

} // namespace qvr::animrec
