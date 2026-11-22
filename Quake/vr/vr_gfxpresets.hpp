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
// Standalone (the Quest's preset, listed on every platform): the Low preset, then a second table of its own
// (standaloneRows: resolution, passes, light paths, particle and gore caps) at one of three levels (Quest 3 72 Hz, Quest
// 3 90 Hz, Quest 2 72 Hz); its own rows' values from before are kept in <gamedir>/standalone_my_settings.cfg (was
// quest_my_settings.cfg, moved on first use) and set back when another preset or My Settings is picked from it.
//
// Console: vr_graphics_preset 0..4 applies a preset (5: My Settings, 6: Standalone) and goes back to -1 (it is a button, not a state);
// vr_graphics_preset_save is Save as My Settings; vr_graphics_preset_status prints the state and, when Custom, the rows
// that differ from the nearest preset.
//
// The machinery (applying, matching, My Settings, the menu's lines) is vr_presets.cpp's, shared with the Gore and
// Gameplay presets (vr_gorepresets.cpp, vr_gameplaypresets.cpp).

#pragma once

namespace qvr::presetcore
{
class Kind;
}

namespace qvr::gfxpresets
{

// The machinery's state for the graphics table (Classic Quake, tests).
[[nodiscard]] presetcore::Kind& kind();

inline constexpr int levelCount = 5; // Off, Low, Medium, High, Ultra
inline constexpr int levelHigh = 3;  // the shipped defaults
inline constexpr int levelMine = 5;  // vr_graphics_preset's My Settings
inline constexpr int levelStandalone = 6; // vr_graphics_preset's Standalone

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

// The Standalone preset: the Low preset, then its own rows at one of its levels (0 Quest 3 72 Hz, the menu's).
void applyStandalone(int level);
void menuApplyStandalone();
[[nodiscard]] int standaloneLevel(); // the Standalone level the settings match now, or -1

} // namespace qvr::gfxpresets
