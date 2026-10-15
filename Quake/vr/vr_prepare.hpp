#pragma once

// vr_prepare.hpp -- the installer's preparation run: `ironwail.exe ... -prepare <progress file> [-preparerelight]`.
//
// -preparerelight implies -prepare: the relight is a step of the preparation run, so the flag alone runs the whole
// preparation (the progress file needs -prepare's path, so an implied run prints its lines to the console only).
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
//   result maps ok=<n> failed=<n>
//   step guns                      on the hub: every weapon a monster can drop, dropped (their convex pieces cut)
//   result guns read=<n> cut=<n> written=<n> <seconds>
//   step models                    on the hub: every model and monster limb precached (their skins' normal maps)
//   result models <models> <limbs> <seconds>
//   step campaigns <i> <n> <folder>  an installed campaign with a game folder of its own (dopa, mg1, mg3) made active
//   campaign <folder> ok <seconds> models=<n> limbs=<n>   its start map loaded, its own models and limbs as above
//                                  (or "campaign <folder> none <seconds>": no model of its own, no map loaded; or
//                                  "campaign <folder> failed <seconds>": not selectable, or its start map not in time)
//   result campaigns ok=<n> none=<n> failed=<n>   (when there was one; Quake's campaign is active again)
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

// vr_prepare_models (single player, a map in play; Setup's preparation run's last map, Debug > Tests): every alias
// model of the game's folders (progs/*.mdl: the paks' and the folders', the first of each name as the game finds it)
// precached now, then the whole limbs of each monster's model with a ragdoll rig (as vr_limbs_prebuild makes a map's
// monsters'): their skins' normal maps are made and kept on disk (cache/normalmaps) here, not at a first visit's load.
// `vr_prepare_models <folder>`: that game folder's models only (progs/**.mdl, its paks' and loose files') and their limbs;
// `vr_prepare_models campaign`: the active campaign's folder (the preparation's campaigns pass).
void precacheModels_f();

} // namespace qvr::prepare
