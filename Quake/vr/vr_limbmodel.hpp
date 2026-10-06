// vr_limbmodel.hpp -- limb gore's limb models (ROUND21.md, "Limb gore"): a monster's limb cut off, as a model of its own,
// made as it loads from the player's own copy of the monster's .mdl (nothing of id's written anywhere).
//
// "<monster model>#limb<bone>" (progs/soldier.mdl#limb4): the triangles whose three corners are on the bones a cut at
// <bone> takes off (ragdoll::limbBones: the bone and every bone on it), in the ragdoll rig's rest pose (the .mdl's first
// frame), centred on ragdoll::limbMiddle; the cut capped in blood: each triangle that crossed the cut with two corners
// on the limb gives a cap triangle from those two to the cap's middle, its texels a strip of blood reds appended under
// each skin. Flags EF_GIB (a blood trail; a zombie's EF_ZOMGIB). One frame.

#pragma once

#include "vr_engine.hpp"

namespace qvr::limbmodel
{

inline constexpr const char* suffix = "#limb";

// "<base>#limb<bone>" into `out` ("<base>#limb<bone>m<bones>" when `bones`, bits, is not all a cut at <bone> takes: a
// limb whose own end was cut off before, the hand gone from the forearm); false if it doesn't fit.
bool name(const char* base, int bone, uint32_t bones, char* out, size_t size);

// The same name kept for good (a QC string, a precache's): the same pointer for the same name.
[[nodiscard]] const char* keptName(const char* base, int bone, uint32_t bones);

// Whether "<base>#limb<bone>" (of `bones`, 0 all) can be made: `base` has a ragdoll rig (loaded now if need be) and
// `bone` is a limb joint of it (ragdoll::limbJoint) with vertices of its own. Cheap after the first call (the rig is kept).
[[nodiscard]] bool available(const char* base, int bone, uint32_t bones = 0);

// Mod_LoadModel (VR_DerivedModelFile): the .mdl file of a limb model named so (allocated with VR_HeapMalloc), or null
// if `name` isn't one or it can't be made.
[[nodiscard]] byte* derivedFile(const char* name, unsigned int* path_id);

// vr_limb_models [<model>]: the limbs of a monster's model (the nearest dead or living monster's, else the grunt's),
// each one's triangles, cap and size, made now.
void info_f();
// SV_SpawnServer, the map's entities spawned (VR_OnSpawnServerSpawned): the whole limbs of every kind of monster the
// map has, made and precached now (vr_limbs_prebuild), not at the first cut (its frame: ~5 ms).
void prebuild();

} // namespace qvr::limbmodel
