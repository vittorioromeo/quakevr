// vr_cellcord.hpp -- the cell cords: a weapon that draws cells, held, has a coiled cord (vr_coil.cpp's, the flashlight's
// Coiled) from its bottom into a cell standing in the ammo pouch, "as if draining the pouch" (the author,
// vrfiringrange_2026-10-08_14-33-07): the laser cannon (vr_cellcord_laser), Mjolnir (vr_cellcord_hammer), the Super
// Axe (vr_cellcord_superaxe). Held with its cord on, the pouch gives and shows cells (QC vr_reload.qc
// VR_Reload_Corded: the lightning gun's, one a 36 up to 3, view::ammoPouchCells), and the cord's plug sits on a cell's
// copper contact (two such weapons: two cords, into the first and the last cell when two or more show). No cells
// left: the plug comes off and the cord hangs loose off the weapon, swinging with it; cells again (picked up), the plug
// flies back onto one. No pouch drawn (reloading not Immersive, the pouch hidden): no cord.
//
// The weapon's end: the middle of its model's lowest vertices (frame 0, +z up: the laser cannon's underside, the end of
// a hammer's or an axe's handle), moved by vr_cellcord_<weapon>_x/y/z (the model's units), the cord leaving it
// downwards. Drawn lit and depth-tested in each eye's opaque scene, as the flashlight's.

#pragma once

#include "vr_hands.hpp"


namespace qvr::cellcord
{

void init(); // vr_cellcord_info

// Once a frame, as the view is set up, after the weapons and the ammo pouch are placed: the cords' ends, their lines.
void setupView(const hands::State& s);

// The cords and their plugs, in each eye's opaque scene.
void drawOpaque();

} // namespace qvr::cellcord
