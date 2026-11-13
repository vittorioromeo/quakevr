// vr_presets.hpp -- the preset machinery the Graphics, Gore and Gameplay presets share (vr_gfxpresets.cpp,
// vr_gorepresets.cpp, vr_gameplaypresets.cpp; ROUND22.md, "Graphics presets"; ROUND25.md, "Gameplay preset").
//
// A kind of preset is one table (Table): every setting it sets and its value at each level (nullptr, D: the setting's
// default_string, vr_defaults.cfg's included). The same table applies a level, tells which level the settings match now
// (Custom when none), and is what My Settings keeps: the first time a level is applied, the value of every row as it
// was is written to <gamedir>/<what>_my_settings.cfg (once; switching levels never writes it again), and My Settings
// sets them back exactly. Save as My Settings writes the current values over it.

#pragma once

#include "Zancle/Container/Vector.hpp"
#include "Zancle/String/String.hpp"

struct cvar_s;

namespace qvr::presetcore
{

inline constexpr int maxLevels = 5;

// A row: a setting and its value at each level (nullptr: its default).
struct Row
{
    cvar_s* var;
    const char* values[maxLevels];
};

inline constexpr const char* D = nullptr;

struct Table
{
    const char* what;     // "graphics", "gore", "gameplay": messages, and the My Settings file's name
    const char* menuPath; // where its My Settings button is, for the file's comment
    const Row* rows;
    int rowCount;
    const char* const* levelNames;
    int levelCount;
};

// One kind's state: its My Settings (read the first time it is asked for in a game folder) and its menu's note.
class Kind
{
public:
    explicit Kind(const Table& table) : t{table} {}

    [[nodiscard]] const Table& table() const { return t; }

    // The level's name (clamped to the table's levels).
    [[nodiscard]] const char* levelName(int level) const;

    // A row's value at a level.
    [[nodiscard]] const char* target(const Row& row, int level) const;

    // Sets every row to the level's value. The first time (no My Settings kept in this game folder), keeps the values
    // as they were first.
    void apply(int level);

    // My Settings set back; false: none kept.
    bool restoreMine();

    // Save as My Settings: the current values of every row kept (written over what was kept).
    void saveMine();

    [[nodiscard]] bool hasMine();

    // My Settings kept now if none are yet (before a single row is changed outside a level: the run setup's).
    void keepMine();

    // The rows that differ from the level (printed, indented, when `print`).
    [[nodiscard]] int differing(int level, bool print) const;

    // The level the settings match exactly, or -1.
    [[nodiscard]] int matchingLevel() const;

    // Whether they match My Settings (false when none are kept).
    [[nodiscard]] bool matchesMine();

    // "Classic", "Custom", "My Settings", or "Classic (My Settings)" when both.
    [[nodiscard]] const char* stateName();

    // <what>_preset_status: the state and, when Custom, the rows that differ from the nearest level.
    void printStatus();

    // The menu's lines: "Now: <stateName>", and what the last press did (empty a few seconds after).
    [[nodiscard]] const char* menuStateLine();
    [[nodiscard]] const char* menuNoteLine() const;

    // The menu's buttons.
    void menuApply(int level);
    void menuRestoreMine();
    void menuSaveMine(); // pressed twice within 3 seconds (it replaces what was kept)

    [[nodiscard]] za::String minePath() const;

    // Tests: My Settings in <what>_my_settings_test.cfg (emptied) instead of the player's, until called with false.
    void useTestFile(bool on);

private:
    struct MineEntry
    {
        za::String name;
        za::String value;
    };

    void loadMine();
    void writeMine();
    void captureMine();
    void setNote(const char* text);
    [[nodiscard]] const Row* findRow(const char* name) const;

    const Table& t;
    za::String mineFrom; // the file My Settings were read from (another game folder: read again)
    bool testFile{false};
    bool mineLoaded{false};
    bool minePresent{false};
    za::Vector<MineEntry> mine;
    char note[160]{};
    double noteTime{-100.0};
    double saveArmedTime{-100.0};
    char line[96]{};
};

// Two values the same: as numbers when both are ("1" and "1.0"), else as text.
[[nodiscard]] bool same(const char* a, const char* b);

} // namespace qvr::presetcore
