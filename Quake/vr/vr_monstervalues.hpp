// vr_monstervalues.hpp -- the monsters' per-class values by name (vr_monsterdata.inc): include it only where one is
// read, so an edit of the table recompiles those files alone. Their loading: vr_monsterdata.hpp.

#pragma once

extern "C" {
#include "q_stdinc.h"
#include "cvar.h"
}

namespace qvr
{

// The monsters' per-class values (design review 3, step 3: quakevr/data/monsters.txt, vr_monsterdata.cpp): no cvars,
// each a stand-in cvar_t in monsterValues[] named as its old cvar (vr_mhull_army, vr_ragdoll_army_mass, ...), read as
// one (name.value, &name in a menu row), reached by that name by the config, the console and the QC.
enum MonsterValueId : int
{
#define QVR_MONSTER_VALUE(name, classname, key) monsterValueId_##name,
#include "vr_monsterdata.inc"
#undef QVR_MONSTER_VALUE
    monsterValueCount
};
extern cvar_t monsterValues[monsterValueCount]; // (vr_monsterdata.cpp)
#define QVR_MONSTER_VALUE(name, classname, key) inline constexpr cvar_t& name = monsterValues[monsterValueId_##name];
#include "vr_monsterdata.inc"
#undef QVR_MONSTER_VALUE

} // namespace qvr
