#pragma once

// vr_relight.hpp -- relighting the map you are in, in the game (vr_relight.cpp; VR Settings > Advanced VR Options >
// Graphics > Relighting): ericw-tools' light run in the background on a copy of the map, with the lamps, glowing
// textures and liquids lit as Misc/quakevr/relight_maps.py lights them, then the map reloaded where you are.

namespace qvr::relight
{

// vr_relight, vr_relight_cancel, vr_relight_revert, vr_relight_status, vr_relight_defaults, vr_relight_lights.
void registerCommands();

// VR_HostFrameEnd: the light process's progress read, its result taken when it ends, the map reloaded.
void poll();

// VR_Shutdown: a light process still running stopped (its half-made files are left in the work folder).
void shutdown();

// The Relighting page's lines (valid until the same function's next call): what is running or last happened, how the
// map in play is lit, and the light tool found.
[[nodiscard]] const char* statusLine();
[[nodiscard]] const char* mapLine();
[[nodiscard]] const char* toolLine();

} // namespace qvr::relight
