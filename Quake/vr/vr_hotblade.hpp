// vr_hotblade.hpp -- incandescent blades as the client draws them (QC vr_hotblade.qc; docs/vr-port/ROUND22.md,
// "Incandescent blades" and "Incandescent blades: his tweaks"). The server says how much each glows: the hands' weapons
// (STAT_QVR_HOTBLADE), the holsters' (STAT_QVR_HOTBLADEHOLSTER) and the weapons lying about or flying (U_QVR_WEAPONUID's
// top bits). Each glowing weapon, wherever it is drawn:
// - glows orange-red on its blade only (the alias shader's QVR_ALIAS_FS_GLOW, masked by a plane across the blade:
//   bladePlane), the heated part the melee strikes with (the QC's layout: an axe's head, a sword's blade past the guard,
//   the crowbar's bar past the hand, the chainsaw's bar);
// - casts an orange-red light from that part (vr_hotblade_light, vr_hotblade_light_radius), following its glow;
// - sheds embers off that part's surface (vr_hotblade_embers a puff, vr_hotblade_ember_rate puffs a second);
// - swung (a hand's) or flying (thrown), leaves fire and sparks in the air behind that part (vr_hotblade_trail,
//   vr_hotblade_trail_linger, vr_hotblade_trail_speed).
// The blade is the weapon's line from the grip (the hand: the entity's origin as a hand holds it) to its muzzle anchor
// (the melee's tip), from the share of it where the QC's striking points start, worked out in the model's pose space so
// that a holstered or lying weapon's is the same part.

#pragma once

#include "vr_engine.hpp"
#include "vr_hands.hpp"

namespace qvr::hotblade
{

// Once a frame, before the eyes: the hands' glow eased, the lights, the embers and the trails.
void queue(const hands::State& s);

// The glow of `e` if it is a glowing blade (a hand's, a holster's, one lying about), coded for the alias shader (4 + 2 x
// its colour + its strength 0..2), else 0.
[[nodiscard]] float entityGlow(const entity_t* e);

// The alias instance's blade mask (vraliasinstance_t's hotblade) of `e` drawn by `matrix` (its pose's space to the
// world): a plane in the pose's space, 0 at the blade's root rising to 1 a little past it; all zero (all of it glows)
// for anything but a glowing blade of a known weapon.
void bladePlane(const entity_t* e, const float matrix[16], float out[4]);

// vr_hotblade_report: the glowing blades this frame (where, their blade's ends, their light), the embers and trail
// particles made in the last second (Debug > Tests > Incandescent Blades).
void report_f();

} // namespace qvr::hotblade
