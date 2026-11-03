// vr_arenawaves.hpp -- the Survival Arena's wave presets (docs/vr-port/ROUND23.md, "Arena wave editor"; the editor:
// vr_waveeditor.hpp; the director: QC/vr_arena.qc). A preset is a small text file, quakevr/arena_waves/<name>.txt:
//
//     arena_waves 1
//     after end
//     wave pool=0
//       entry kind=grunt main="Super Shotgun" off=none armour=green pieces=full infected=none count=3 delay=0 boss=0
//       entry kind=zombie main=none off=none armour=none pieces=full infected=none count=4 delay=5 boss=0
//
// `after`: end (the last wave's clear ends the run), last (the last wave again and again), loop (from the first again).
// `pool`: its foes up at once at most (0: the arena's own number for that wave). `kind`: a row of QC's VR_WAVE_KINDS.
// `main`, `off`: a weapon's name (the toolgun's armed page's), none, or (main only) random from the pool; `armour`: none
// green yellow red random; `pieces`: full, random, or chest helmet shoulders forearms thighs shins joined by +;
// `infected`: none or a row of QC's VR_INFECT_ROWS. `delay`: seconds into the wave before they may come in. Anything the
// file doesn't say has the line above's default; comments (// or #) and blank lines are not kept when it is saved again.
//
// Which preset plays is keyed by map and difficulty, nothing else: arena_waves/assign.cfg has lines `<map>.<skill> <name>`
// (skill 0 Easy .. 3 Nightmare). No line for a map and skill: Default, the director's own waves, exactly as ever. A map
// named there is an arena map (QC VR_ARENA_On), so a new arena needs only its name there.
//
// The engine reads and writes the files and checks what it can alone (the file's form, armour, pieces); what depends on
// the game's tables (the kinds, weapons, infections) QC checks as the map starts, reporting through arenawavesreport.

#pragma once

#include "Zancle/Container/Vector.hpp"
#include "Zancle/String/String.hpp"

namespace qvr::arenawaves
{

constexpr int maxWaves = 200;
constexpr int maxEntries = 64; // (QC AR_ENTRY_MAX)
constexpr int maxCount = 40;   // (QC AR_PLAN_MAX: a wave's foes at most)
constexpr int skillCount = 4;
constexpr const char* const skillNames[skillCount] = {"Easy", "Normal", "Hard", "Nightmare"};

struct Entry
{
    char kind[32]{"grunt"};
    char mainHand[40]{"random"};
    char off[40]{"none"};
    char armour[12]{"none"};
    char pieces[48]{"full"};
    char infected[24]{"none"};
    int count{1};
    float delay{0.f};
    int boss{0};
};

struct Wave
{
    int pool{0};
    za::Vector<Entry> entries;
};

enum class After
{
    End,
    Last,
    Loop
};

struct Preset
{
    After after{After::End};
    za::Vector<Wave> waves;
};

// The text <-> the preset. parse: false when it isn't a preset at all (no header, a version from the future); anything
// odd inside is said in `report` (lines) and skipped.
[[nodiscard]] bool parse(const char* text, Preset& out, za::String& report);
[[nodiscard]] za::String write(const Preset& p);

// Why this game can't take an entry, as far as the engine can tell ("": it can).
[[nodiscard]] const char* badReason(const Entry& e);

// [A-Za-z0-9_-], 1..48 characters: a preset's name is a file's.
[[nodiscard]] bool validName(const char* name);

// The folder's files (com_gamedir/vr_arena_waves_dir). load, save: false on failure, said in `report`.
[[nodiscard]] bool load(const char* name, Preset& out, za::String& report);
[[nodiscard]] bool save(const char* name, const Preset& p, za::String& report);
bool removeFile(const char* name);
[[nodiscard]] za::Vector<za::String> list();

// assign.cfg: the preset for a map and skill ("": Default), and setting it ("" clears it; the file rewritten sorted).
[[nodiscard]] za::String assigned(const char* map, int skill);
void setAssigned(const char* map, int skill, const char* name);
[[nodiscard]] bool mapListed(const char* map);
[[nodiscard]] za::Vector<za::String> listedMaps();

// The editor's Play: `p` plays in `map` from wave `wave` (the arena's jump) until another map loads or playOff.
void play(const char* map, const char* name, const Preset& p, int wave);
void playOff();
[[nodiscard]] bool playing();

// The lines QC reported while the map started (the editor shows them).
[[nodiscard]] const za::String& lastReport();

void registerCommands();

// QC's: arenawavesload, arenawavesfield, arenawavesreport (vr_builtins.cpp's table).
void pfLoad();
void pfField();
void pfReport();

} // namespace qvr::arenawaves
