// vr_runsetup.hpp -- the run setup: a run's difficulty, presets and mutators (docs/vr-port/MUTATORS.md: how to add one;
// ROUND25.md, "runsetup").
//
// A run is a game from its start (New Game, the Map Library's Play, an Official Campaign, the hub's teleporter) to the
// next. Its choices are one registry (vr_runsetup.cpp, `registry`): each an id, a label, its options, its default, a
// help line, its section on the Run Setup screen, and what it does:
// - vr_runpick_<id> (saved in the config): the screen's choice, selected next time;
// - vr_run_<id> (not in the config): the run's own, set from the pick when the run starts (start()), written into every
//   saved game (the `// qvr_run` line: saveLine()) and set back from it when one is loaded (readSaveLines(),
//   onLoadGame()); a changelevel keeps it (a cvar). The QC reads it (vr_run.qc's hooks, a file per mutator);
// - apply(value, phase): what a choice sets beyond its cvar (the presets, the armed and AI switches, Bloody Nightmare);
// - forced(): a rule another choice imposes (Melee Only holds Starting Weapon at the axe; Bloody Nightmare holds the
//   difficulty at Nightmare), the screen showing it held.
// Presets never overwrite the player's settings without keeping them: the Gameplay and Gore presets' first apply keeps
// My Settings (vr_presets.cpp), and "My Settings" is a Style and Gore choice of its own.

#pragma once

struct cvar_s;

namespace qvr::runsetup
{

// What starts the run, for the screen's Start (the menu's side: vr_menu_runsetup.inc).
enum class Launch
{
    None,
    Library,  // the Map Library's Play (a package, a map)
    NewGame,  // the flat main menu's New Game (map start)
    Campaign, // Official Campaigns' row
    Hub,      // the hub's teleporter to the start map
};

enum class Section
{
    Difficulty,
    Presets,
    Mutators,
};

enum class Phase
{
    Start, // a run begins (the screen's Start, `vr_run start`)
    Load,  // a saved game of a run is loaded
};

struct Mutator
{
    const char* id;      // vr_runpick_<id>, vr_run_<id>, the save line's key
    const char* label;   // the screen's row
    Section section;
    const char* const* options;
    int optionCount;
    int def;             // vr_runpick_<id>'s default (vr_cvars.inc says the same)
    int oldSave;         // a saved game without the run line: this value (-1: left as it is now)
    const char* help;
    cvar_s* pick;
    cvar_s* active;
    void (*apply)(int value, Phase phase); // nullptr: only the cvar (the QC reads it)
    int (*forced)();                       // the value another choice holds this one at, or -1; nullptr: never
    bool (*shown)();                       // nullptr: always shown
    const char* heldBy;                    // the choice whose rule holds it (the screen: "held by ..."), or nullptr
};

[[nodiscard]] int count();
[[nodiscard]] const Mutator& at(int i);
[[nodiscard]] int find(const char* id); // -1: none

// The value a row starts the run with: its forced one, else its pick.
[[nodiscard]] int pickValue(int i);
[[nodiscard]] int activeValue(int i);
[[nodiscard]] int forcedValue(int i); // -1: free

// The run starts: every vr_run_<id> from its pick (forced ones held), each apply(Start).
void start();

// The hub (a VR map, not a run): the mutators back to their defaults (the presets stay as they are).
void endRun();

// "Hard / Unleashed / Pistol start": the picks (the screen) or the run's.
[[nodiscard]] const char* summary(bool picks);

// SaveData_Fill (VR_SaveContext): "// qvr_run id=v id=v ...\n".
[[nodiscard]] const char* saveLine();

// VR_ReadSaveInfo: the loading save's line kept; onLoadGame (VR_OnBeginLoadGame) sets the run from it, or the old-save
// values with a console line when it has none.
void readSaveLines(const char* text);
void onLoadGame();

// Ironman is on in this run (vr_run_ironman): no saving (C: VR_RunIronman, VR_RunIronmanRefusesSave).
[[nodiscard]] int ironman();

// A key that changes when a row is held or let go, or shown or hidden (the screen rebuilt then).
[[nodiscard]] int pageKey();

void init(); // the console commands (vr_run, vr_runsetup_test)

} // namespace qvr::runsetup
