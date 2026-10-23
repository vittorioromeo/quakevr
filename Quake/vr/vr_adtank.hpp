// vr_adtank.hpp -- AD support: the flamethrowers' fuel (the pyro's flamethrower, the fumigator's poison sprayer: our burst
// rifle's variants, vr_adgunart.cpp) held, a small tank on the player's back and a hose from it to the gun, as AD's pyro
// and fumigator carry theirs (vr_adgun_hose; ROUND22.md, "AD guns as burst rifle variants").
//
// The tank stands upright on the middle of the back (carried by the chest, vr_adgun_tank_x/y/z; two such guns: one a
// hand, side by side, the main hand's on the body's right), in the gun's colour with a hazard band. The hose leaves its
// bottom and runs to the gun's inlet (the fuel canister's back, vr_adgunart.cpp hoseInlet) by the body's side: through a
// clip at the hip (vr_adgun_hose_route 0) or over the shoulder (1), on the gun's hand's side. It is a chain of points
// stepped by Verlet (vr_verlet.cpp: falling, held to its length, pushed out of the torso, carried by the body's
// movement; the gun's swings swing it), drawn as the flashlight's plain cord (vr_coil.cpp), lit, depth-tested in each
// eye's opaque scene; also in the body's mirror image while a menu page tweaks the body (vr_bodyclone.cpp), reflected.

#pragma once

#include "vr_hands.hpp"

namespace qvr::adtank
{

void init(); // vr_adgun_hose_info

// Once a frame, as the view is set up (after the weapons are placed): each hand's hose stepped, the tanks placed.
void setupView(const hands::State& s);

// The tanks and hoses, in each eye's opaque scene (and their mirror image).
void drawOpaque();

// Once a frame at the end of CL_RelinkEntities (VR_RelinkWornTanks, after the armed monsters' weapons are on their drawn
// hands): the armed monsters' back tanks (QC vr_adtankworn.qc) found and carried onto their drawn bodies (a listen
// server's), for the hoses to their guns.
void relinkWorn();

} // namespace qvr::adtank
