// vr_lootbeams.hpp -- loot beams (vr_lootbeams; ROUND22.md, "Loot beams"): a soft pillar of light rising from the middle
// of a thing lying about to pick up, as Diablo's and Path of Exile's, so loot stands out among the corpses and gibs. Each
// category has its toggle and hue (vr_lootbeam_<category>, vr_lootbeam_hue_<category>): weapons (dropped, thrown, a
// map's; the expansions' and Arcane Dimensions' guns, the crossbow: any weapon prop) and magazines fuller than
// vr_lootbeam_mag_min on by default; ammo, health, armour, power-ups, keys and runes, backpacks off. Never while held,
// holstered (both drawn by the view, not lying about), carried (the server says so: vr_server.cpp lootByte) or moving
// (thrown, falling, flying home): it fades in once the thing has lain still vr_lootbeam_settle seconds. Opacity, length
// and thickness are settings; it fades out as your head comes near it (vr_lootbeam_near), and far away if wanted. A
// dropped toolgun (a weapon prop drawn as progs/v_toolgun.mdl) has its own hue and a longer beam (vr_lootbeam_hue_toolgun,
// vr_lootbeam_toolgun_length: magenta, twice as high), found among many weapons.
//
// Cost: the things are found once a frame (the visible entities), each eye draws all the beams in one draw: two soft
// crossed shells turned towards the eye about the vertical (a core and its glow) and a small flare at the base, 42
// vertices a beam.

#pragma once

namespace qvr::lootbeams
{

void init(); // vr_lootbeams_report, vr_lootbeam_test

// The scene's translucent pass (VR_DrawSceneTranslucent), each view: the beams. The first view of a frame finds them.
void drawTranslucent();

} // namespace qvr::lootbeams
