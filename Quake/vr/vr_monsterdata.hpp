// vr_monsterdata.hpp -- the monsters' per-class values as data (design review 3, step 3; ROUND23.md, "Monsters as
// data"): quakevr/data/monsters.txt, a `monster <classname> { key "value" ... }` block per classname; which values each
// classname has and their old cvar names: vr_monsterdata.inc. Each value is a stand-in cvar_t of monsterValues[]
// (vr_cvars.hpp) named as its old cvar (vr_mhull_army, vr_smallgibs_mult_grunt, vr_ragdoll_army_mass, ...), never
// registered: the C++ and the menus read it as before, the config, the console and the QC's cvar handles reach it by
// that name (VR_CvarStandIn, VR_StandInSet; vr_cvars.cpp), the player's changes are saved as the config's lines of it.

#pragma once

#include "vr_engine.hpp"

#include "Zancle/Vocabulary/Span.hpp"

namespace qvr::monsterdata
{

// Reads quakevr/data/monsters.txt into monsterValues[] (their defaults; the config's lines go over them) and adds
// vr_monster_dump.
void registerCvars();

[[nodiscard]] cvar_t* findStandIn(const char* name); // the value by its old cvar name (any case), or null
bool setStandIn(cvar_t* var, const char* value);     // false: `var` is not one of monsterValues[]
[[nodiscard]] za::Span<cvar_t> standIns();

} // namespace qvr::monsterdata
