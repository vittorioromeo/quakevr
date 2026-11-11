// vr_arenadirector.hpp -- the Survival Arena's budget director (docs/vr-port/ARENA_DIRECTOR.md; vr_arena_director 1).
//
// Every wave gets a threat budget and buys its foes off one price list (quakevr/data/arena_director.txt): a foe costs its
// body, its weapon and its extras (armour, a second weapon, an infection, a boss mark). A theme rolled per wave biases
// the picks and sets the wave's cadence (the gap between spawn groups, their size, its surges); recently seen items are
// less likely, long-unseen ones more; every few waves a foe carries a gun the waves have not offered yet. A separate
// budget buys the player's supplies after a clear, leaning on how he is doing (vr_arena_supply_reactive).
//
// The planner is here; QC's director (QC/vr_arena.qc VR_ARENA_DirectorPlan) reads a wave's plan through the builtins
// below and spawns it with its own machinery (places, the alive pool, armour plates, the boss's bulk). A run's plans
// depend only on its seed and the waves before (never on the player), so a wave is planned by replaying the run from
// wave 1 when it is not the next one: nothing of the director is saved.
//
// The QC builtins (QC/builtins.qc):
//   arenadirstart(skill, seed, rowsOk)   a run begins: rowsOk one '0'/'1' per weapon ladder row (VR_ARENA_GunOk); 1: ok
//   arenadirplan(wave)                   the wave's plan; its foes' count (0: no data)
//   arenadirget(what, i, field)          a number of it (`What`, `FoeField`, `WaveField` below)
//   arenadirname(what, i)                its theme's word (what 0), its boss's name (what 1)
//   arenadirsupply(wave, threat, hp, dom) a clear's supplies: how many items (arenadirget(WhatSupply, i, 0): its code)

#pragma once

namespace qvr::arenadirector
{

enum What
{
    WhatFoe = 0,    // arenadirget(0, foe, FoeField)
    WhatWave = 1,   // arenadirget(1, 0, WaveField)
    WhatSupply = 2, // arenadirget(2, item, 0): the item's code (SupplyCode)
};

enum FoeField
{
    FoeKind = 0,     // QC VR_WAVE_KINDS row
    FoeRow = 1,      // its weapon: a ladder row (0..33), -1 its fists, -2 none (a plain monster)
    FoeRow2 = 2,     // its left hand's: a row, -2 none
    FoeArmour = 3,   // 0 none, 1 green, 2 yellow, 3 red
    FoePlates = 4,   // its armour's plates (2..10)
    FoeInfected = 5, // 1: infected
    FoeBoss = 6,     // 0, 1 the boss, 2 Nightmare's second
    FoeSurge = 7,    // 0 the wave's own, k > 0: its k-th surge
    FoeCost = 8,     // its threat points
};

enum WaveField
{
    WaveTheme = 0,      // the theme's index
    WaveBudget = 1,     // threat points it had
    WaveSpent = 2,      // threat points its plan holds
    WaveSurges = 3,     // its surges
    WaveGap = 4,        // s between two spawn groups at its base (Normal: the skill divides it in QC)
    WaveGroupMin = 5,   // a spawn group's foes: at least
    WaveGroupMax = 6,   // ... at most
    WaveStretch = 7,    // the gap times 1 + this x the threat up / the wave's threat
    WaveSoft = 8,       // threat up past this share of the wave's: the gap doubles again
    WaveSpread = 9,     // 1: its groups come from places spread round the player
    WaveForced = 10,    // 1: the progression guarantee put a gun in
    WaveQuickTime = 11, // s a foe killed this soon after it came in is a quick kill (0: none)
    WaveQuickPay = 12,  // ... its score times 1 + this
    WaveSurgeWarn = 13, // s a surge is announced before it comes in
    WaveAmbush = 14,    // 1: its surges are ambushes (behind the player first)
};

enum SupplyCode
{
    SupplySmallMedkit = 1,
    SupplyMedkit = 2,
    SupplyMega = 3,
    SupplyShard = 4,
    SupplyGreen = 5,
    SupplyYellow = 6,
    SupplyRed = 7,
    SupplyAmmo = 8,
    SupplyQuad = 9,
    SupplyRing = 10,
    SupplyPent = 11,
};

void registerCommands();

void pfStart();
void pfPlan();
void pfGet();
void pfName();
void pfSupply();

} // namespace qvr::arenadirector
