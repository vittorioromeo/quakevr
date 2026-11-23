// vr_server.hpp -- server-side Quake VR state shared with other VR sources.

#pragma once

namespace qvr
{
struct VrMove;
}
namespace qvr::view
{
struct HeldPlace;
}
namespace qvr::playerhit
{
struct Pose;
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
// Holster `slot` (body::Holster's order) of the client `player` as his client drew it, carried with his entity since
// (MP_PLAN.md, B6b: QC's holsterpos for a remote player). False: none sent.
[[nodiscard]] bool holsterPoint(struct edict_s* player, int slot, glm::vec3& out);

// (bots3) Whether client slot `slot`'s entity is a bot (FrikBot: a "player" whose QC is not a human's, the slot itself not
// active; MP_PLAN.md B11 part 3).
[[nodiscard]] bool isBot(int slot);
// A bot's weapon in `hand` (0 off, 1 main) placed as every client draws it (view::placeHeld at its QC handpos, handrot;
// its avatar sends this place): false none (an empty hand, a fist).
[[nodiscard]] bool botHeld(struct edict_s* bot, int hand, view::HeldPlace& out);
// (hitzones) What client slot `slot`'s body is drawn from, as his avatar sends it (writeAvatars: his head, his body's yaw,
// his hands, from his entity's origin; his body's size, mpavatar::serverBody): his shots' shape (vr_playerhit.cpp). False:
// no avatar (no move from him yet), or his body is id's ranger (his box is what is drawn).
[[nodiscard]] bool avatarPose(int slot, playerhit::Pose& out);

// How many of client `clientNum`'s moves came in a server frame that had one of his already (merged: their VR bits ORed,
// the hands' last pose kept) since he connected: vr_mp_status `merged=` (MP_PLAN.md, B5d: melee at jitter).
[[nodiscard]] int mergedMoves(int clientNum);
// His string commands since he connected: run, dropped over his budget (VR_ClientCommandBudget), and the fewest his
// budget had left after one (vr_mp_status's mp_cmds).
struct CommandCounts
{
    int used, dropped, least;
};
[[nodiscard]] CommandCounts commandCounts(int clientNum);

// The client places `player`'s hands (and muzzles) from where it last saw the player, which
// lags the server by a frame or more: moves the hand fields (and the stored move) along by
// however far the player has moved since, so that they sit where the hands are relative to the
// player now (a held box keeps up with a running player, shots leave the gun).
void rebaseHands(struct edict_s* player);

// Forgets every client's VR move (a new server).
void resetClients();

// The avatars (QVR_SVC_AVATAR) this server has sent, those left unsent as unchanged (vr_net_avatar_keyframe), their bytes.
void avatarCounts(int& sent, int& skipped, long long& bytes);
// Every sound precached so far read for a loop (its cue point) now, while the map loads (VR_OnSpawnServerSpawned), not at
// its first play in the game (a whole file read from disk there). A sound precached later still reads at its first play.
void learnSoundLoops();

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
// shells, 1 the revolver's blue slugs) out of the weapon in `player`'s `hand`, `delay` seconds from now (flags: 1 a flick reload).
void sendEject(struct edict_s* player, int hand, int kind, int count, int flags, float delay);

// `watershock(kind, org, radius, duration)` from QC (the lightning gun in water, vr_shock.cpp): kind 0 (`player` shocked)
// to `player` alone, the others (arcs on or in a liquid round `org`) to every client, unreliable.
void sendShock(struct edict_s* player, int kind, const float org[3], float radius, float duration);

// `viewmessage(player, text, seconds)` from QC (target_vr_message): `text` in the middle of `player`'s view (at eye level,
// as VR Calibration's text; not in the wrist's hologram) for `seconds`; without a headset, a centre print held that long
// (vr_worldtext.cpp). To that player alone, reliable. `arena`: the Survival Arena's (QVR_SVC_ARENAMESSAGE: the wrist
// gadget's hologram shows it instead with vr_arena_wrist).
void sendViewMessage(struct edict_s* player, const char* text, float seconds, bool arena = false);

// `collectfx(hand, e, hotspot, modelindex, origin, angles)` from QC (VR_CollectFx_Send): `player`'s `hand` put away entity
// `ent` (model `modelIndex`, there, so turned) at `hotspot` (a holster's, a pouch's): its client draws it going in
// (vr_collectfx.cpp). To that player alone, reliable.
void sendCollect(struct edict_s* player, int hand, int hotspot, int ent, int modelIndex, const float origin[3],
    const float angles[3]);

// The same to every client whose view can see the entity `seen` (its PVS): a round into a gun lying about (QVR_CFX_INTO_PROP,
// `ent` the gun), seen sliding in by everyone, by a hand or a loose one by contact alike.
void sendCollectSeen(struct edict_s* seen, int hand, int hotspot, int ent, int modelIndex, const float origin[3],
    const float angles[3]);

// Weapon effects (vr_weaponfx.cpp), to every client (the datagram: a lost one is a flash or a tracer less): `shooter`
// fired the weapon in `hand` (-1: a monster's gun), and a hitscan pellet of its went from `from` to `to`.
void sendFired(struct edict_s* shooter, int hand);
void sendTracer(struct edict_s* shooter, int hand, const float from[3], const float to[3]);

// A whole message for every client's datagram from an engine console command (vr_physics_blast's explosion), which runs
// before the server frame: written at the next frame's start, after SV_ClearDatagram (sv.datagram itself would be
// cleared before it was sent). An explosion's temp entity launches its chunks (vr_explosiondebris.cpp) as QuakeC's does.
void queueBroadcast(const unsigned char* data, int len);

// An engine console command's run of the server's QuakeC whose broadcast must reach the clients (vr_hurt's T_Damage:
// its wound, its sounds): begin empties sv.datagram (the last frame's broadcast, sent already; the frame's
// SV_ClearDatagram would drop it anyway), so the run writes into the whole room; end queues what it wrote for the next
// frame (queueBroadcast), whole messages at a time (unreliable: what has no room in the queue goes nowhere).
void commandBroadcastBegin();
void commandBroadcastEnd();

// A message about entity `ent` (an armed humanoid's pose) for the datagrams of the clients that have it in sight this frame
// (vr_net_cullposes; the loopback client gets them all): written into the buffer returned, then scopedEnd. nullptr: culling
// is off, write to the broadcast instead.
struct sizebuf_s* scopedBegin(int ent, bool always = false); // always: whatever vr_net_cullposes says (a player's avatar)
// scopedEnd's kind: a player's avatar goes in a snapshot client's snapshot (vr_netsnap.cpp, B7b); one unchanged since it was
// last sent, there only (the others wait for its keyframe).
inline constexpr int scopedOther = 0;
inline constexpr int scopedAvatar = 1;
inline constexpr int scopedAvatarUnchanged = 2;
void scopedEnd(int kind = scopedOther);

} // namespace qvr::server
