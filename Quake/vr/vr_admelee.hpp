// vr_admelee.hpp -- AD support: Arcane Dimensions' melee monsters' weapons as Quake VR's melee weapons (QC
// vr_ad_melee.qc: the death fury's two swords, the death knight's, the zombie knight's and the death guard's swords, the
// hunter ogres' hammer and mace; WID_SWORD with a kind in its weapon flags), each a model of its own made as it loads
// from the player's own copy of AD's weapon model (its gib: progs/w_dknightsword.mdl...; nothing of AD's written
// anywhere), as the AD soldiers' guns are (vr_adguns.hpp).
//
// "<AD weapon model>#melee" (progs/w_dknightsword.mdl#melee): AD's weapon laid as Quake VR's knight's sword is
// (v_ksword.mdl, make_swords.py): the hand's place at the knight sword's grip, the handle and blade along its blade's
// line (the hammer's and mace's handles likewise, the hand near the handle's end), the guard (a head's width) along its
// guard, 2.6 times AD's size as the knight's sword is the knight's (the weapon draws at 0.34): so its weapon slot
// (vr_weapons.inc, slots 41-46) is the knight sword's with its own length. AD's gibs are too thick and coarse held: the
// swords are made anew (a slim bevelled blade along AD's blade's spine, a guard with finials, a turned pommel and
// wrapped grip), the hammer's and mace's handles too (AD's head kept, its holes closed), each surface skinned from
// AD's own skin where AD's model had that part; the fury's sword and the hammer turned half round (the curve, the
// spike, away from the player). One frame (AD's gibs have one). Without AD's data (or another release's file that
// doesn't fit) the knight's own sword's file stands in.

#pragma once

#include "vr_engine.hpp"

namespace qvr::admelee
{

inline constexpr const char* suffix = "#melee";
// "<the death sergeant's flail>#flailhead" (progs/w_blueflail.mdl#flailhead): its spiked head alone, its ball's middle at
// the origin and +x towards the ring its chain holds, at the held weapons' drawn size (drawn at scale 1: vr_flail.cpp).
inline constexpr const char* flailHeadSuffix = "#flailhead";
inline constexpr const char* flailModel = "progs/w_blueflail.mdl#melee";
inline constexpr const char* flailHeadModel = "progs/w_blueflail.mdl#flailhead";
inline constexpr float flailRingReach = 3.5f; // units: the head's ball's middle (its origin) to the ring its chain holds (+x)

// Mod_LoadModel (VR_DerivedModelFile): the .mdl file of a weapon model named so (allocated with VR_HeapMalloc), or null.
[[nodiscard]] byte* derivedFile(const char* name, unsigned int* path_id);

// vr_admelee_models [write]: each weapon made now, its triangles, box and tip (the weapon settings' anchors).
void info_f();

// A curved sword's blade (QC admeleebend, VR_Melee_At: the melee's points follow it): sword kind `kind`'s (QC
// VR_ADMELEE_*, 1 the death fury's) spine at share `f` of the straight line from the hand to its tip, off that line:
// x along the line's normal in the blade's plane (the model's +y crossed with the line), y along the model's +y, both
// as shares of the line's length. 0 0 for a straight blade (under half a unit off), a heavy weapon, or none made.
[[nodiscard]] glm::vec2 bend(int kind, float f);

// A hammer's or mace's head (QC admeleehead, VR_Melee_Sweep: the head's volume strikes): heavy kind `kind`'s head's box
// in the frame bend() uses, as shares of the line's length: x along the line from the hand, y along its normal in the
// weapon's plane, z along the model's +y (even about the line). False for a sword, or none made (AD's data not there).
[[nodiscard]] bool head(int kind, glm::vec3& lo, glm::vec3& hi);

// The spine's point at share `f` of the line, in the model's frame, of the made model named `model` (".../...#melee";
// vr_admelee_spine shows it on the hand's weapon). False if it is not a sword of these, or not made.
[[nodiscard]] bool spineAt(const char* model, float f, glm::vec3& out, glm::vec3& line); // line: the straight line's point there

// The far end's middle (the slot's MuzzleAnchor: a blade's tip, the flail's handle's end where its chain hangs), and the
// hand's place, in the model's frame, of the made model named `model` (".../...#melee"). False if none is made.
[[nodiscard]] bool tipOf(const char* model, glm::vec3& tip, glm::vec3& hand);

} // namespace qvr::admelee
