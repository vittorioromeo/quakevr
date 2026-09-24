// vr_server.hpp -- server-side Quake VR state shared with other VR sources.

#pragma once

namespace qvr::server
{

// Called when a server finishes spawning: model precaches made so far are known to clients.
void onSpawnServerAfterLoad();

void init(); // registers commands

// `haptic(hand, delay, duration, frequency, amplitude)` from QC, sent to `player` (a client).
void sendHaptic(struct edict_s* player, int hand, float delay, float duration, float frequency, float amplitude);

} // namespace qvr::server
