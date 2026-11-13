// vr_gameplaypresets.hpp -- the Gameplay preset (VR Settings > Gameplay Presets) and Classic Quake (VR Settings' first
// button, the hub pavilion's wall button). ROUND25.md, "Gameplay preset"; the table and its taste calls:
// docs/vr-port/GAMEPLAY_PRESET.md.
//
// One table (vr_gameplaypresets.cpp, rows) on vr_presets.cpp's machinery (the Graphics preset's): every gameplay switch a
// preset sets and its value at Classic (as close to Quake's monsters, combat and movement as the mod allows), Light
// (Quake's rules, with the mod's vanilla-but-better things: seamless teleporters, reloading, melee, physics props) and
// Unleashed (each setting's default: the game as shipped). Custom when the settings match none; My Settings kept on the
// first apply (<gamedir>/gameplay_my_settings.cfg). Never comfort, calibration, the body, controls, sound, graphics or
// gore; the hands, grabbing and holsters are the same at every level.
//
// Classic Quake: Graphics Off (Quake), Gore Minimal and Gameplay Classic at once; Back to My Settings sets all three
// kinds' My Settings back.
//
// Console: vr_gameplay_preset 0 Classic, 1 Light, 2 Unleashed (3: My Settings) applies it and goes back to -1 (a
// button, as vr_graphics_preset); vr_gameplay_preset_save is Save as My Settings; vr_gameplay_preset_status prints the
// state and, when Custom, the rows that differ from the nearest level. vr_classic_quake 1 is Classic Quake, 0 Back to My
// Settings (it shows 1 while all three match it). vr_gameplay_preset_test is the headless check ("gameplaypreset: N
// passed, M failed").

#pragma once

namespace qvr::presetcore
{
class Kind;
}

namespace qvr::gameplaypresets
{

inline constexpr int levelClassic = 0;
inline constexpr int levelLight = 1;
inline constexpr int levelUnleashed = 2; // the shipped defaults
inline constexpr int levelCount = 3;
inline constexpr int levelMine = 3; // vr_gameplay_preset's My Settings

void init();

// The machinery's state for the gameplay table (the run setup, tests).
[[nodiscard]] presetcore::Kind& kind();

[[nodiscard]] const char* levelName(int level);

// The menu's lines and buttons (as the Graphics Presets page's).
[[nodiscard]] const char* menuStateLine();
[[nodiscard]] const char* menuNoteLine();
void menuApply(int level);
void menuRestoreMine();
void menuSaveMine();

// Classic Quake: the three presets at once, what it changed said on the line under it (classicLine); Back to My
// Settings: the three kinds' My Settings back.
void classicApply();
void classicBack();
[[nodiscard]] bool classicActive();
[[nodiscard]] const char* classicLine();

// vr_classic_quake set to classicActive() (the menu's draw, the pavilion's screen).
void syncClassic();

} // namespace qvr::gameplaypresets
