// vr_server.hpp -- server-side Quake VR state shared with other VR sources.

#pragma once

namespace qvr
{
struct VrMove;
}

namespace qvr::server
{

// Called when a server finishes spawning: model precaches made so far are known to clients.
void onSpawnServerAfterLoad();

void init(); // registers commands

// The latest VR move of the client `player` is (its head, hands, muzzles, VR bits, teleport and
// room-scale moves), or nullptr for a client that sent none. Whatever the progs: the engine's own
// VR physics work from these, not from QC fields.
[[nodiscard]] const VrMove* clientMove(struct edict_s* player);

// Forgets every client's VR move (a new server).
void resetClients();

// Head angles of `player`'s latest move, as a vec3_t (nullptr without one).
[[nodiscard]] float* clientHeadAngles(struct edict_s* player);

// `haptic(hand, delay, duration, frequency, amplitude)` from QC, sent to `player` (a client).
void sendHaptic(struct edict_s* player, int hand, float delay, float duration, float frequency, float amplitude);

// `handimpact(hand, strength, dir)` from QC: knocks `player`'s drawn hand (a parried blow).
void sendHandImpact(struct edict_s* player, int hand, float strength, const float dir[3]);

} // namespace qvr::server
