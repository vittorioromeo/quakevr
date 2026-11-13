// vr_gorepresets.hpp -- the Gore preset (VR Settings > Gore: Gore Preset; ROUND25.md, "Gore preset").
//
// One table (vr_gorepresets.cpp, rows) lists every gore setting a preset sets and its value at Minimal and Off; Full is
// each cvar's default (default_string: vr_defaults.cfg's included), the game as shipped. The same table applies a choice
// and tells which one the settings match now (Custom, 0, when none: a setting changed by hand). vr_gore_preset is the
// choice shown, not a saved setting (the settings are): 1 Full, 2 Minimal, 3 Off; setting it applies it.
//
// Console: vr_gore_preset_status prints the state and, when Custom, the rows that differ from the nearest choice;
// vr_gore_preset_test is the headless check (prints "gorepreset: N passed, M failed").
//
// The machinery is vr_presets.cpp's (the Graphics and Gameplay presets' too): the first choice applied keeps the
// settings as they were as My Settings (<gamedir>/gore_my_settings.cfg), which restoreMine sets back (Classic Quake's
// Back to My Settings; ROUND25.md, "Gameplay preset").

#pragma once

namespace qvr::presetcore
{
class Kind;
}

namespace qvr::gorepresets
{

// The machinery's state for the gore table (Classic Quake, tests).
[[nodiscard]] presetcore::Kind& kind();

inline constexpr int choiceCustom = 0;
inline constexpr int choiceFull = 1;
inline constexpr int choiceMinimal = 2;
inline constexpr int choiceOff = 3;

void init();

// Sets every row to the choice's value (1..3).
void apply(int choice);

// The choice the settings match exactly (1..3), or 0 (Custom).
[[nodiscard]] int matching();

// My Settings set back (false: none kept), and vr_gore_preset synced.
bool restoreMine();

// "Full", "Minimal (My Settings)", "Custom"...
[[nodiscard]] const char* stateName();

// vr_gore_preset set to what the settings match now (the menu's draw).
void sync();

} // namespace qvr::gorepresets
