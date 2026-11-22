// vr_lagcomp.hpp -- lag compensation (MP_PLAN.md, B8b): a remote client's shots judged against the world as he saw it.
//
// A client on a remote server draws the others behind the server: the players (and every non-stepping thing) at his
// interpolation buffer's time (vr_netsnap.cpp: the newest message's time less its delay), a stepping monster at the
// newest message's time on NetQuake's lerp; and that newest message is itself half a round trip old when he presses.
// Each of his moves says what he saw at (netmove::WireMove: `time`, the newest message's server time, and `viewBack`,
// how far behind that the others were drawn). The server keeps the last frames' places, angles, frames and hit model
// tracks (vr_hitmodel.cpp's lerp state) of the players and monsters; a trace of his that meets models (MOVE_HITMODEL:
// the shots, PF_traceline) is run with each of them put back where he saw it (vr_net_lagcomp_most at most), then put
// back again. Single player and the listen host's own client: nothing (their moves are clc_move's, and their view is
// the server's). Only targets move back: the world, doors and lifts stay as they are.

#pragma once

#include "vr_engine.hpp"

namespace qvr::lagcomp
{

// The frame's targets kept (VR_ServerFrameEnd, after the hit model tracks); none while no remote client plays.
void serverFrame();
// What client slot `slot`'s newest move saw: the newest message's server time and how far behind it the others were.
// `cameAt`/`quiet`: when the server first read the move and the silence allowance it came with (netmove::serverCame):
// its time's age is judged at its arrival, not after the server's own move buffer kept it.
void noteMove(int slot, float time, float viewBack, double cameAt, double quiet);
void reset(); // a new map (the history and the clients' times)

// The trace's scope: `shooter` (a client edict) puts the targets back where he saw them; false: nothing moved (off, not a
// remote client, no history). end puts them back.
[[nodiscard]] bool begin(edict_t* shooter, double most = -1.0); // (most: seconds back at most; < 0: vr_net_lagcomp_most)
void end();

void init(); // cvars' commands: vr_lagcomp_shot / _punch (a client's test shot or punch, reported), vr_mp_shoot / _punch

// QC: float lagcompbegin(entity e), void lagcompend() (B8c): melee's sweeps, a bash's and a headbutt's finds and a
// parry of another player's blow judged as e saw the others (their places, poses and hands), as his shots are.
void PF_lagcompbegin();
void PF_lagcompend();
void PF_lagcompcount();

} // namespace qvr::lagcomp
