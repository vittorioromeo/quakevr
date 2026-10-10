#pragma once

// vr_prepare.hpp -- the installer's preparation run: `ironwail.exe ... -prepare <progress file> [-preparerelight]`.
//
// The work a first start would otherwise do in the headset, done by the installer (Installer/, GamePreparation.cs) with
// the game hidden: Quake VR's first maps loaded once each (vrcalibration, vrtutorial, vrstart: their compiled hulls,
// ambient occlusion and normal maps written to the disk caches, cache/; vrstart's hulls are 10 s of a cold load), then,
// with -preparerelight, every map relit (vr_relight_batch everything, as the installer's first-start marker did: the
// same files, so the game's first start finds them current and relights nothing). Then it quits. Not the hub, the
// tutorial or the first-start relight: quake.rc's vr_startgame starts this instead (VR_StartGame_f). No tips either
// (vr_tips 0: one shown to the mock headset would be marked seen for the player).
//
// The installer starts it as the motion review starts its copies (-vrmock -noconfigwrite -noautoexec -nosound: no
// headset, no config written, no player's autoexec), with QVR_TEST_HIDDEN, QVR_TEST_BACKGROUND and QVR_NO_ERROR_DIALOG
// (no window, no network, no dialog). Its progress goes to the file, a line each, flushed:
//   step maps <i> <n> <map>        a map loading (i from 1)
//   map <map> ok <seconds>         loaded (or "map <map> failed <seconds>": not loaded in time)
//   step relight                   the relight started (with -preparerelight)
//   relight <percent> <status>     every half second while it runs
//   result relight ended=<0|1> maps=<n> relit=<n> skipped=<n> failed=<n> cancelled=<n> own=<n> | <status>
//   done                           then it quits
// The installer kills it (and its light processes) to cancel.

namespace qvr::prepare
{

// Started with -prepare.
[[nodiscard]] bool active();

// From vr_startgame (instead of the hub): the steps begin.
void start();

// Each host frame's end: the steps advanced.
void frame();

} // namespace qvr::prepare
