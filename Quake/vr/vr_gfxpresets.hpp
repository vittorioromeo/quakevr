// vr_gfxpresets.hpp -- graphics presets (VR Settings > Graphics Presets; ROUND22.md, "Graphics presets").
//
// One table (vr_gfxpresets.cpp, presetRows) lists every setting a preset sets and its value at each level: Off (Quake's
// own look), Low, Medium, High (the shipped defaults: each cvar's default_string, vr_defaults.cfg's included) and Ultra.
// The same table applies a preset, tells which one the settings match now (Custom when none), and is what My Settings
// keeps: the first time a preset is applied, the values of every row as they were are written to
// <gamedir>/graphics_my_settings.cfg (once; switching presets never writes it again), and My Settings sets them back
// exactly. Save as My Settings writes the current values over it. Only graphics settings are in the table: never
// comfort, gameplay, the body, calibration or sound.
//
// Console: vr_graphics_preset 0..4 applies a preset (5: My Settings) and goes back to -1 (it is a button, not a state);
// vr_graphics_preset_save is Save as My Settings; vr_graphics_preset_status prints the state and, when Custom, the rows
// that differ from the nearest preset.

#pragma once

namespace qvr::gfxpresets
{

inline constexpr int levelCount = 5; // Off, Low, Medium, High, Ultra
inline constexpr int levelHigh = 3;  // the shipped defaults
inline constexpr int levelMine = 5;  // vr_graphics_preset's My Settings

void init();

// The level's name: "Off (Quake)", "Low", "Medium", "High", "Ultra".
[[nodiscard]] const char* levelName(int level);

// Sets every row to the level's value (0..4). The first time (no My Settings kept in this game folder), keeps the
// values as they were first.
void apply(int level);

// My Settings set back; false: none kept.
bool restoreMine();

// Save as My Settings: the current values of every row kept (written over what was kept).
void saveMine();

[[nodiscard]] bool hasMine();

// The level the settings match exactly (0..4), or -1.
[[nodiscard]] int matchingLevel();

// Whether they match My Settings (false when none are kept).
[[nodiscard]] bool matchesMine();

// "High", "Custom", "My Settings", or "High (My Settings)" when both.
[[nodiscard]] const char* stateName();

// The menu's lines: "Now: <stateName>", and what the last press did (empty a few seconds after).
[[nodiscard]] const char* menuStateLine();
[[nodiscard]] const char* menuNoteLine();

// The menu's buttons.
void menuApply(int level);
void menuRestoreMine();
void menuSaveMine(); // pressed twice within 3 seconds (it replaces what was kept)

} // namespace qvr::gfxpresets
