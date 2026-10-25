// vr_adweapons.hpp -- AD support: Arcane Dimensions' three upgraded player weapons (the Shadow Axe, the Widowmaker, the
// Plasma Gun: QC vr_ad_bridge.qc's upgrades) as VR weapon models, each made as it loads from the player's own copy of
// AD's flat-screen view model (nothing of AD's written anywhere), as the AD soldiers' guns are (vr_adguns.hpp).
//
// "<AD view model>#vr" (progs/v_shadaxe3.mdl#vr, progs/v_shot3.mdl#vr, progs/v_plasma.mdl#vr): AD's model in one pose,
// turned, scaled and moved into the frame of the Quake VR weapon whose settings it inherits (the axe's, the super
// shotgun's, the lightning gun's: vr_weapons.inc slots 33-35), packed from that model's corner (the weapon settings scale
// about it), so the same Offsets, hotspots and holsters hold it. What a flat-screen view model lacks is made up: the
// arm and hand that held the axe taken off; the parts behind the hand cut off (the Plasma Gun's stock, which would sit in
// the forearm); the Quake VR weapon's own grip, trigger guard and trigger put on the guns (their texels copied below
// AD's skin); every hole left (the open bottoms and backs, the cut) closed with a fan of dark triangles. Nine frames of
// the one pose (the weapon's animations' frame numbers stay valid). AD's skin, as the upgrade's own (v_shadaxe3: the
// glowing blade). The Widowmaker's barrels turn (a Gatling): 27 spin frames after the nine, drawn blended (spinFrame).

#pragma once

#include "vr_engine.hpp"
#include "vr_view.hpp"

namespace qvr::adweapons
{

inline constexpr const char* suffix = "#vr";

// Mod_LoadModel (VR_DerivedModelFile): the .mdl file of a model named so (allocated with VR_HeapMalloc), or null. Not
// made (another release of AD's model, AD's data missing): the Quake VR weapon's own model under its name.
[[nodiscard]] byte* derivedFile(const char* name, unsigned int* path_id);

// vr_adweapon_models: each model made now: triangles, holes left, box, the corner kept, its muzzle (the blade's edge).
void info_f();

// The Widowmaker's barrels turning (a Gatling auto shotgun, his notes 2026-10-19): `hand`'s gun fired (vr_weaponfx.cpp).
void fired(int hand);

// Every frame, after the guns are placed: a hand's Widowmaker drawn with its barrels turned (its model's spin frames
// blended: ViewEntity::poseFrame), spinning up while its trigger is held or it fires, down after, at rest a barrel on top.
void spinFrame(view::ViewEntity (&weapons)[2]);

void clear();
void prepare(); // the barrels' motor sound (vr/widowmaker_motor.wav)
void registerCommands(); // vr_ad_widowmaker_spin_info

} // namespace qvr::adweapons
