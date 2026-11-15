// vr_predict.hpp -- a remote client's own walk predicted (MP_PLAN.md, B8).
//
// Server: a remote client's moves (QVR_CLC_MOVES) are queued as they are read; a server frame whose walk is plain (on
// foot, not swimming, lying, flat-screen sliding, dead, hanging or climbing) runs them one by one through vr_pmove.cpp,
// each for its own frame's time, instead of SV_AirMove and SV_WalkMove once with the newest. His datagram then carries
// the state after the newest move run and the rules it ran by (QVR_SVC_PREDICT, a snapshot record).
//
// Client: from that state the moves sent after it are replayed each frame with the same code, and his own entity (the
// view, the hands, the body) is drawn there; a new state that moves the prediction eases in (an offset decaying over
// about 0.1 s). A door or lift he stands on is drawn at its newest place, as the replay met it. Single player and a
// listen host's own client never come here (they send clc_move).

#pragma once

#include "vr_engine.hpp"
#include "vr_netmove.hpp"
#include "vr_pmove.hpp"

namespace qvr::predict
{

// Both sides: a move's walk input (the clamps the same on both).
[[nodiscard]] pmove::Input inputOf(const netmove::WireMove& m);

// Server.
void serverQueue(client_t* client, const netmove::WireMove& m); // a move just run (applyNetMove)
void serverReset(int slot);
// Single player's (and a listen host's own client's) climb out of the water by hand, once a frame (QC CheckWaterJump's
// builtin vr_waterclimb): pmove::waterClimbTry on `ent`'s hands now, `hands` those gripping and empty (bit 0 the off
// hand, 1 the main) -> 1: lifted out (its hook told QuakeC), 0 not. One function with a remote client's moves (B9).
int waterClimbFrame(edict_t* ent, int hands);
// ... the grapple's rope holding him, once a frame (QC VR_Grapple_ServiceAnchor's builtin grapplehold): pmove::grappleStep
// on `ent` for `hook` (its .gh_len, .gh_speed, .gh_reel, .gh_payout, .gh_taut read and its length and taut written back),
// the corner `at` and the path beyond it, `reeling` (B/Y held), the rope's shortest; `hand` the gun's (-1 a holster).
// One function with a remote client's moves (B9).
void grappleFrame(edict_t* ent, edict_t* hook, int hand, const float* at, float beyond, bool reeling, float shortest);

// Client.
void parseState();  // QVR_SVC_PREDICT's body
void clientClear(); // a new connection or map
void init();        // vr_mp_predict_report, vr_mp_predict_probe
void report();      // "mp_predict: ..." (vr_mp_status, vr_mp_predict_report)

} // namespace qvr::predict
