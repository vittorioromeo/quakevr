#pragma once

// vr_takes.hpp -- takes and clips for trailers and shorts (docs/vr-port/TRAILER.md, "Takes and clips"): OBS's
// recordings (through obs-websocket: vr_obs.cpp) as takes of a scene, each named <date>_<scene>_t<NN>_<rating>.<ext>
// in OBS's recording folder and rated keep / maybe / reject (reject: moved to <folder>/rejected/, never deleted).
//
// - A take: vr_take starts OBS's recording and stops it (any recording OBS makes while vr_takes is on is a take, OBS's
//   own hotkey's too). After it, the rating prompt: vr_take_keep, vr_take_maybe, vr_take_reject (the last take or clip).
// - A clip: vr_clip saves OBS's replay buffer (its last seconds, as OBS's Replay Buffer settings keep), kept running
//   while a take session is open; named as a take, rated "clip" until rated otherwise.
// - In and out marks: vr_take_in, vr_take_out during a take, its times from the take's start (and OBS's chapters, where
//   its output takes them: CreateRecordChapter).
// - A take session (vr_takes_session start / end): opened by the first take or clip; while open, the wrist gadget's
//   screen shows the takes' touch buttons (vr_takes_gadget) and OBS's connection is kept.
//
// The light while a take runs and the rating prompt: in the headset only (not the mirror or the spectator camera:
// drawInEye from the eye's UI pass), a small corner of the window in flat play (flatIndicator).

#include "Zancle/Base/SizeT.hpp"

namespace qvr::hands
{
struct State;
}

namespace qvr::takes
{

void registerCommands(); // vr_take, vr_clip, vr_take_keep/maybe/reject/rate, vr_take_in/out, vr_takes_session, ...
void frame();            // each frame, after obs::poll (VR_HostFrameEnd): OBS's answers and events, pending moves

// The headset's light and rating prompt, in an eye's UI pass (never the mirror's or the spectator camera's).
void drawInEye(const hands::State& s);

// Flat play: the corner's text (null: nothing shown) and whether its light is lit (a take runs).
[[nodiscard]] const char* flatIndicator(bool& light);

// The wrist gadget: whether its screen shows the takes' page now, and that page drawn (its 2D pass, its virtual
// screen's size in pixels) with its touch buttons acted on.
[[nodiscard]] bool gadgetPage();
void drawGadgetPage(float width, float height);

// A controller button changed (vr_input.cpp, before the rest takes it): the in/out marks' chord (vr_takes_chord).
// `button`: posing::Button's order (0 trigger, 1 grip, 2 primary, 3 secondary, 4 stick click). True: taken.
[[nodiscard]] bool controllerButton(int hand, int button, bool down);

// A voice note began (vr_voicenotes.cpp): `base` its files' path without the extension. Kept with the running take.
void voiceNote(const char* base);

// The keyboard's take keys, bound once where unbound (vr_checkbindings: after the config).
void defaultKeys();

// The Recording page's status line.
[[nodiscard]] const char* statusLine();

} // namespace qvr::takes
