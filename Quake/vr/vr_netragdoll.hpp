// vr_netragdoll.hpp -- the server's ragdolls drawn by a remote client as the server has them (ragsync, 2026-11-21;
// MP_PLAN.md, "Ragdolls and physics bodies in multiplayer").
//
// Box3D runs on the server only (MP_PLAN 2.2, item 1). Everything it moves that is an entity (props, crates, barrels,
// gibs, heads, cut limbs, dropped and thrown weapons, embedded blades, grenades, explosion chunks, rubble, pushable
// corpses) reaches a client as the entity's origin and angles in the snapshot, interpolated with its turn slerped
// (vr_netsnap.cpp). A ragdoll's parts are not entities: the listen host's client draws the server's published parts
// (ragdoll::publish) and a remote client used to draw the corpse's death animation (or, an active ragdoll's, its frame)
// instead. Now each ragdoll in a remote client's sight is a snapshot record (netsnap::ragKey + its entity:
// QVR_SVC_RAGPOSE, its parts' places and turns quantized), delta-coded with the others (a settled one's record is
// unchanged: it costs nothing), its moving ones' bytes under vr_net_ragdoll_bytes by priority (netsnap holdRagdolls).
// The client keeps each one's last poses with their server times and draws it between the two around the clock its
// entities are drawn at (the interpolation buffer's), parts slerped, through the listen host's drawing
// (ragdoll::publishNet, swapModels).

#pragma once

#include "vr_engine.hpp"

namespace qvr::netrag
{

// Server (VR_SnapEnd): ragdoll `num`'s record into the client's snapshot being made, if it is one (vr_net_ragdolls).
void serverRecord(int num);
// Its message's bytes (also the selftest's): false if `num` has no ragdoll published.
[[nodiscard]] bool serverWrite(int num, sizebuf_t* msg);

// Client. clientParse: QVR_SVC_RAGPOSE (its snapshot record). clientSnapshotBegin/End: around a snapshot's records (the
// ones not in it: gone). clientFrame: once a frame before the ragdolls are drawn (their poses around the clock).
void clientParse();
void clientSnapshotBegin();
void clientSnapshotEnd();
void clientFrame();
void clientClear();
// After the ragdolls' drawing (VR_RagdollSwap): vr_mp_bodylog's frame.
void logFrame();
[[nodiscard]] int clientCount(); // the remote server's ragdolls drawn now (vr_mp_status)

void init(); // vr_mp_bodylog, vr_netrag_selftest

} // namespace qvr::netrag
