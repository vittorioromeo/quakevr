#pragma once

// vr_ad_monsters.hpp -- Arcane Dimensions' monsters with Quake VR's treatment, the engine's side (docs/vr-port/
// ARCANE_DIMENSIONS.md, section 17; QC vr_ad_monsters.qc): their ragdoll rigs (vr_ad_rigs.inc, read by vr_ragdoll.cpp's
// seed tables) and the Quake VR monster each one is tuned as (its ragdoll settings, its hull width, armour's sinking).
// The engine's files call it at the few places marked "AD support".

namespace qvr::ad
{

// The Quake VR class whose per-class settings an AD monster class takes (its ragdoll's Ragdoll Settings page, its hull
// width, armour): "monster_hell_knight" for AD's death knight. `classname` itself for every other class.
[[nodiscard]] const char* vrClass(const char* classname);

// The Quake VR class an AD head model's monster is tuned as (vrClass of the class that throws it: "monster_hell_knight"
// for h_dknight.mdl), so that its prop's Mass -1 weighs a share of that ragdoll's (box3d::headPropMass); nullptr for
// any other model.
[[nodiscard]] const char* headVrClass(const char* model);

} // namespace qvr::ad
