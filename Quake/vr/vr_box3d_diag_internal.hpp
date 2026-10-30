#pragma once
// vr_box3d_diag_internal.hpp -- what vr_box3d_diag.cpp (the physics commands' registration and the rest) shares with
// its command groups: vr_box3d_diag_piles.cpp, vr_box3d_diag_perf.cpp, vr_box3d_diag_ragdoll.cpp and
// vr_box3d_diag_player.cpp. Internal: included by those alone.
#include "vr_box3d_internal.hpp"

namespace box3d::detail
{

// What the physics stress tests spawn carries this bit in its spawnflags (no map sets it; the spawn functions read only
// their low bits), so that vr_physics_clearpiles takes away theirs and nothing the map placed. 2^20: exact as a float.
constexpr int stressTagBit = 1 << 20;

// vr_box3d_diag.cpp: the entities `which` names: a number, a classname (every one), or "props" (every rigid body).
[[nodiscard]] za::Vector<edict_t*> entitiesNamed(const char* which);

// The commands (registerCommands).
void loose_f();
void stack_f();
void pyramid_f();
void pile_f();
void bigPile_f();
void clearPiles_f();
void blast_f();
void steptime_f();
void frametime_f();
void mtbench_f();
void ragdollArmTest_f();
void ragdollSpeeds_f();
void ragdollList_f();
void ragdollActiveList_f();
void ragdollEnvHit_f();
void ragdollBlastTest_f();
void corpseList_f();
void corpsePoses_f();
void corpseDrop_f();
void player_f();
void inLevel_f();
void approach_f();
void inside_f();
void shotBench_f();
void fire_f();
void shockCheck_f();
void knockdownTest_f();

} // namespace box3d::detail
