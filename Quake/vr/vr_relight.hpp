#pragma once

// vr_relight.hpp -- relighting the map you are in, in the game (vr_relight.cpp; VR Settings > Advanced VR Options >
// Graphics > Relighting): ericw-tools' light run in the background on a copy of the map, with the lamps, glowing
// textures and liquids lit as Misc/quakevr/relight_maps.py lights them, then the map reloaded where you are. Or many
// maps one after another (an episode, a game, the Map Library's, every one: vr_relight_batch), a few side by side.

namespace qvr::relight
{

// vr_relight, vr_relight_batch, vr_relight_cancel, vr_relight_revert, vr_relight_status, vr_relight_defaults,
// vr_relight_lights, vr_relight_vispatch.
void registerCommands();

// VR_HostFrameEnd: the light processes' progress read, their results taken when they end, the next maps started, the
// map in play reloaded.
void poll();

// VR_Shutdown: the light processes still running stopped, their half-made files removed (the maps done are kept).
void shutdown();

// A relighting (one map or a batch) is running.
[[nodiscard]] bool running();

// How far the relighting running (or the last one) is, 0..1, the maps weighed by their size; -1: none this session.
[[nodiscard]] float progress();

// The Relighting page's lines (valid until the same function's next call): what is running or last happened (the
// maps done of how many, the time so far), the maps being lit (their stage and percentage), the progress bar's text
// (its percentage and the time left), how the map in play is lit, and the light tool found.
[[nodiscard]] const char* statusLine();
[[nodiscard]] const char* detailLine();
[[nodiscard]] const char* progressText();
[[nodiscard]] const char* mapLine();
[[nodiscard]] const char* toolLine();

// Whether a light.exe is found (toolLine's look, at most every 2 s; at once after a download finished).
[[nodiscard]] bool toolFound();
// Whether VisPatch's data files are found (See-Through Liquids can patch id's maps; looked for with the tool).
[[nodiscard]] bool seeThroughAvailable();
// What the page shows of the tool (found, a download running, its result) and of VisPatch's data (found), as a number:
// a change rebuilds the page.
[[nodiscard]] int toolPageState();

// The compact line shown outside the menu while a relighting runs (the wrist gadget's screen, the flat screen's
// corner): "RELIGHT 3/12 45% 2:10"; null when none runs (or vr_relight_indicator 0).
[[nodiscard]] const char* indicator();

} // namespace qvr::relight
