// vr_motion.hpp -- the motion recorder (docs/vr-port/MOTIONS.md): takes of the player's motions for
// tuning the melee, recorded in the headset and played back in the mock headset.
//
// Recording (vr_motion.cpp): with the recorder armed (VR Settings > Advanced VR Options > Motion
// Recorder, or vr_motion_armed 1), a click of the off hand's stick starts a take of the category chosen
// there (named by the result expected: slash, stab, no hit, bash, parry pose, ...; vr_motion_category,
// with an optional detail, vr_motion_detail) and the next click ends it and saves it as
// quakevr/motions/<label>_<date>_<time>.csv: every host frame, the head's and the hands' tracking as the
// runtime gave it and as the game placed it (world, the player's frame and the dummy's), the buttons,
// the fingers, the weapons and their striking points, the player, the nearest monster (the training
// dummy), and the melee events the game registered. vr_motion_record <label> / vr_motion_stop do the
// same from the console; vr_motion_list counts the takes.
//
// Playback (vr_motion_play.cpp): vr_motion_play <file> drives the mock headset from a take, frame by
// frame at its recorded times, in front of the training dummy (or a monster) as it was recorded;
// vr_motion_eval <folder or pattern> plays many and checks each one's events against
// quakevr/motions/expect.cfg.

#pragma once

#include "vr_backend.hpp"

#include <string>

struct edict_s;

namespace qvr::motion
{

void init();     // commands
void shutdown(); // a take being recorded or written is saved

// VR_BeginFrame, right after the backend's tracking (before the grip filter and the keys): the
// recorder keeps the runtime's tracking; a playback replaces it (the mock backend only).
void afterTracking(TrackingState& tracking, FrameState& frame);

// VR_BeginFrame, after the frame's world texts were cleared: the recorder's indicator.
void frame();

// The input's stick click of `hand`: true if the recorder takes it (armed, its button), so that its
// key is left alone.
[[nodiscard]] bool stickClick(int hand, bool pressed);

// VR_ServerFrameEnd: a server frame's sample, while recording, armed or playing.
void serverFrame();

// The end of a host frame (after the screen): a row.
void hostFrameEnd();

// Playback (vr_motion_play.cpp): the host frame's time (its recorded time while a take plays: the
// same frames at any speed), and whether its server frame runs (-1: as usual; 0: no; 1: yes, for
// `frametime` seconds).
[[nodiscard]] double hostFrameTime(double time);
[[nodiscard]] bool gameClockFixed(); // hostFrameTime ignores the wall clock (vr_fixed_frames, a take's playback not in watch mode)
[[nodiscard]] int serverFrameOverride(double& frametime);
[[nodiscard]] bool playing();
[[nodiscard]] bool evaluating(); // vr_motion_eval runs (its takes' maps loading or playing)

// The QC's builtins (vr_builtins.cpp): an event, a striking point, a value. Kept only while a take is
// recorded or played.
void qcEvent(const char* kind, const char* sub, int hand, float value, const float* at, struct edict_s* targ,
    const char* detail);
void qcPoint(int hand, const float* at, const char* name);
void qcValue(const char* key, const float* value);

// For the menu (vr_menu.cpp): the chosen label's takes so far, the button, the last saved file.
[[nodiscard]] const char* labelStatus();
[[nodiscard]] const char* lastSaved();
void discardLast();

} // namespace qvr::motion
