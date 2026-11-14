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

// Client.
void parseState();  // QVR_SVC_PREDICT's body
void clientClear(); // a new connection or map
void init();        // vr_mp_predict_report, vr_mp_predict_probe
void report();      // "mp_predict: ..." (vr_mp_status, vr_mp_predict_report)

} // namespace qvr::predict
