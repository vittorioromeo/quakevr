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

// The client places `player`'s hands (and muzzles) from where it last saw the player, which
// lags the server by a frame or more: moves the hand fields (and the stored move) along by
// however far the player has moved since, so that they sit where the hands are relative to the
// player now (a held box keeps up with a running player, shots leave the gun).
void rebaseHands(struct edict_s* player);

// Forgets every client's VR move (a new server).
void resetClients();

// Head angles of `player`'s latest move, as a vec3_t (nullptr without one).
[[nodiscard]] float* clientHeadAngles(struct edict_s* player);

// `haptic(hand, delay, duration, frequency, amplitude)` from QC, sent to `player` (a client).
void sendHaptic(struct edict_s* player, int hand, float delay, float duration, float frequency, float amplitude);

// `handimpact(hand, strength, dir)` from QC: knocks `player`'s drawn hand (a parried blow).
void sendHandImpact(struct edict_s* player, int hand, float strength, const float dir[3]);

// `catchblend(hand, e, org, ang)` from QC: `player`'s `hand` caught entity `ent` (a force grab) at `origin`, turned
// `angles`; its client eases the weapon from there into the hand.
void sendCatchBlend(struct edict_s* player, int hand, int ent, const float origin[3], const float angles[3]);

// `ejectcasings(hand, kind, count, delay, flags)` from QC: `count` spent casings of `kind` (0 shotgun
// shells) out of the weapon in `player`'s `hand`, `delay` seconds from now (flags: 1 a flick reload).
void sendEject(struct edict_s* player, int hand, int kind, int count, int flags, float delay);

// `watershock(kind, org, radius, duration)` from QC (the lightning gun in water, vr_shock.cpp): kind 0 (`player` shocked)
// to `player` alone, the others (arcs on or in a liquid round `org`) to every client, unreliable.
void sendShock(struct edict_s* player, int kind, const float org[3], float radius, float duration);

// Weapon effects (vr_weaponfx.cpp), to every client (the datagram: a lost one is a flash or a tracer less): `shooter`
// fired the weapon in `hand` (-1: a monster's gun), and a hitscan pellet of its went from `from` to `to`.
void sendFired(struct edict_s* shooter, int hand);
void sendTracer(struct edict_s* shooter, int hand, const float from[3], const float to[3]);

} // namespace qvr::server
