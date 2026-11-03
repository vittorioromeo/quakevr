// vr_blowline.hpp -- a monster's melee blow as a line, read from its model's own baked frames: see vr_blowline.cpp and
// docs/vr-port/ROUND23.md, "Directional parry: the only parry".

#pragma once

namespace qvr::blowline
{

// vr_blowline_dump <model> <frame function>... : what the model's frames give along that animation, frame by frame.
void registerCommands();

// float(entity m, float tau, float which) blowline: the monster m's blow at its frame now: how many lines it has (0: none:
// its limb goes along itself, it's too short, or nothing of it sweeps), and striker `which` (0 the fastest) as drawn at
// tau s from now, into the QC globals vr_blow_a (its near end), vr_blow_b (its far end), vr_blow_more (x how far ahead of
// the monster's origin the striker gets in its animation).
void PF_blowline();

// float(entity m) blowlunge: whether m's frame now is in a run of frames its block in quakevr/data/monsters.txt calls a lunge.
void PF_blowlunge();

// Your weapons' lines of late, for a parry's timing (QC VR_DRP_BecameValid). void(entity pl, float hand, vector a, vector b)
// handlineadd: this server frame's (again in one frame: the last stands; a == b: no weapon). float(entity pl, float hand,
// float back) handlineat: the one `back` frames ago into vr_blow_a, vr_blow_b, and when (-1: none).
void PF_handlineadd();
void PF_handlineat();

} // namespace qvr::blowline
