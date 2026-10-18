// vr_admelee.hpp -- AD support: Arcane Dimensions' melee monsters' weapons as Quake VR's melee weapons (QC
// vr_ad_melee.qc: the death fury's two swords, the death knight's, the zombie knight's and the death guard's swords, the
// hunter ogres' hammer and mace; WID_SWORD with a kind in its weapon flags), each a model of its own made as it loads
// from the player's own copy of AD's weapon model (its gib: progs/w_dknightsword.mdl...; nothing of AD's written
// anywhere), as the AD soldiers' guns are (vr_adguns.hpp).
//
// "<AD weapon model>#melee" (progs/w_dknightsword.mdl#melee): AD's weapon, whole, laid as Quake VR's knight's sword is
// (v_ksword.mdl, make_swords.py): the hand's place at the knight sword's grip, the handle and blade along its blade's
// line (the hammer's and mace's handles likewise, the hand near the handle's end), the guard (a head's width) along its
// guard, 2.6 times AD's size as the knight's sword is the knight's (the weapon draws at 0.34): so its weapon slot
// (vr_weapons.inc, slots 41-46) is the knight sword's with its own length. Every frame re-packed (one, AD's gibs).
// Without AD's data (or another release's file that doesn't fit) the knight's own sword's file stands in.

#pragma once

#include "vr_engine.hpp"

namespace qvr::admelee
{

inline constexpr const char* suffix = "#melee";

// Mod_LoadModel (VR_DerivedModelFile): the .mdl file of a weapon model named so (allocated with VR_HeapMalloc), or null.
[[nodiscard]] byte* derivedFile(const char* name, unsigned int* path_id);

// vr_admelee_models: each weapon made now, its triangles, box and tip (the weapon settings' anchors).
void info_f();

} // namespace qvr::admelee
