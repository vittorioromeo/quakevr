// vr_bodycontact.hpp -- the player's body against monsters' (ROUND22.md, "Closer to enemies"): his hands couldn't reach
// an enemy's neck or head before the two boxes met.
//
// - vr_body_monster_inset (units, 0..12; Combat > Holding Enemies > Body Inset): where a player's move meets a monster's
//   box, or a monster's move a player's, each box is that much narrower on each level side (both: twice as close face to
//   face). Only those two, each with its own box (a QC trace with another box is Quake's): the map, brush models, props,
//   corpses, monsters against monsters, shots, missiles, precise hits and melee keep their boxes.
// - vr_foegrab_step_in (Combat > Holding Enemies > Step In): while a hand of a player holds an enemy (vr_foegrab.cpp),
//   his body and that enemy's don't meet at all, both ways: he steps in and brings the other hand to its head.
// - Either on, a player and a monster already overlapping (a hold let go of, the inset made smaller, a monster stepped
//   into) never stick: a move of either that doesn't take them deeper in (from where it starts, along all of it) passes
//   (Quake's start-solid would have stopped every move, SV_CheckStuck lifted him), and serverFrame pushes them apart,
//   the monster first (pushSpeed units/s), the player for what it can't take (a wall behind it, a ledge).
//
// Server; world.c calls through vr_api.h (VR_BodyContact*).

#pragma once

#include "vr_engine.hpp"

namespace qvr::bodycontact
{

void init(); // registers vr_body_contact_status

// Server: a player and a monster overlapping (neither held by the other, stepped in) pushed apart a little
// (VR_ServerFrameEnd).
void serverFrame();

} // namespace qvr::bodycontact
