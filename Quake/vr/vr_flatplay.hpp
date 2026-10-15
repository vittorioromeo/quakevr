// vr_flatplay.hpp -- flatscreen play (-flatscreenmode): the VR game played on the desktop screen with the mouse and
// the keyboard (docs/vr-port/FLATPLAY.md).
//
// The mode is the flag plus no headset session (VR_IsActive() == 0). Without the flag, or in a headset, nothing here
// runs: the game is exactly as it was. With it, the flat player's hands are marked as tracked, so the VR gameplay --
// held weapons, the force grab, melee, body interactions -- runs on poses the game makes up from the view angles, the
// weapon in each hand and the player's own movement, instead of a headset's tracking.
//
// -flatscreenmode implies no VR backend: the mode needs no headset session to begin with, so the flag stands in for
// `vr_backend none; vr_restart` (vr_main.cpp's startBackend asks flatplay::flagGiven, and flatplay::noteBackendImplied
// says once what the flag implied). Start the game without the flag to play in a headset.

#pragma once

#include "vr_hands.hpp"

namespace qvr::flatplay
{

// The mode runs now: -flatscreenmode was given and no headset session is up.
[[nodiscard]] bool active();

// -flatscreenmode was given, whatever the headset: vr_main.cpp's backend selection, which the flag implies no backend
// for (noteBackendImplied).
[[nodiscard]] bool flagGiven();

// The one console line for that implication (vr_main.cpp asks before it starts a backend).
void noteBackendImplied();

// VR_Init: reads the flag, registers vr_flatplay_status.
void init();

// VR_BeginFrame, before the hands are posed: the keys (E, V), the cvars the mode stands in for (reloading off, the
// flashlight worn and always lit, body interactions on), vr_flatplay itself.
void frame();

// vr_hands.cpp's flat-screen branch: where each hand goes and how it is turned, in world space. The straight
// out-in-front pose stands when the mode is off.
void poseHands(hands::State& s, const glm::vec3& aim);

// vr_flatplay_status: what the mode is, and its pose.
void status_f();

} // namespace qvr::flatplay
