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

} // namespace qvr::blowline
