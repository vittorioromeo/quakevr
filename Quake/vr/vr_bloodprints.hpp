// vr_bloodprints.hpp -- bloody footprints and handprints (vr_footprints*, vr_handprints*; docs/vr-port/ROUND22.md,
// "Bloody footprints and handprints").
//
// Feet: stepping in a pool of blood (the pools goresound keeps: notePool, poolUnder) or onto a bloody body (a ragdoll
// with blood painted on it, vr_wounds) bloodies your feet: the next vr_footprint_count steps leave a bloody boot print
// each, fading (vr_footprint_fade). Moving, a step is a person's step length at your speed walked on the ground (0.7 m
// at a walk to 2.2 m at a run, over vr_footprint_rate; the walk's legs step fewer times: their cadence is capped), the
// feet in turn, beside your way as the posed legs stand apart, along the posed foot; standing, a step is the posed
// legs' own (avatar::Skeleton::steps: turning on the spot). The print lies on the floor, as long as the foot
// (vr_footprint_size times), the left one mirrored. The pool and body checks run at a step only (a few dozen
// distances; the entities near you once), never per frame.
//
// Hands: a hand's blood from the gore (wounds' smearHand: a gib taken or held, a bloody blow, vr_gore_hands_test)
// gathers to 1 (soaked full); while some is left, each frame one short line from that hand's palm through its palm's
// way finds what it presses on (a wall, the floor, a ledge it grips). A new touch (or one slid along) leaves a bloody
// handprint there, along the hand, the thumb on its side, fainter as the blood runs out (each takes 1 /
// vr_handprint_count), at most one every 0.3 s a hand; over an edge (a ledge's top, a wall's foot) a second print on
// the face beyond, the first unfolded onto it. On a prop lying about (vr_handprint_props) its own blood mask takes the
// print (wounds::palmTouch, handprintOn); what the hand holds takes its blood where it grips, every 0.75 s
// (vr_handprint_gear; wounds::gripPrint). Water washes hands and feet.
//
// The prints are the decals' (decals::Mark::Footprint, Handprint: the blood's colours, its bucket and permanent layer).

#pragma once

#include "vr_avatar.hpp"

namespace qvr::bloodprints
{

// The gore's blood smeared over `hand` (0 the off hand, 1 the main hand): `amount` its blots times their size (a gib
// taken: 4).
void bloodyHand(int hand, float amount);

// Once a frame, after the body is posed (vr_view.cpp's dripBlood): the drawn hands (null where not drawn).
void frame(const hands::State& s, const avatar::HandPose* const drawnHands[2]);

// All forgotten (a new map).
void clear();

// vr_bloodprints_test, vr_bloodprints_info.
void registerCommands();

} // namespace qvr::bloodprints
