// vr_moveguard.hpp -- a remote player's own values bounded (MP_DESIGN P2-6, MP_PLAN 2.2 item 2; REVIEW_MP_2026-11-18
// finding 1). The owner's body is the owner's: his client sends his head, hands, muzzles, loading ports and their speeds,
// his room-scale walk and his fists, and the server writes them into his edict for QC to shoot, swing, grab and reload
// from. Each is held within what a real body does, generously (long two-handed weapons held at arm's length, tall
// players, tracking glitches): a place within an arm's reach and a weapon's length of the move's origin, a hand's or a
// throw's speed a real arm's, the room-scale walk a walking head's over time, a fist's spheres a hand's size. It stops
// the gross abuses (shots from across the map, a speed hack through the room-scale walk, a 500-unit fist), not aim
// assistance or wall-seeing. The server bounds the move at the top of applyVrMove (the funnel of every move); his
// client's prediction bounds the values it replays (predict::inputOf) with the same functions, so an honest player
// never sees a correction from them. Each value held is counted per client (vr_mp_status's "mp_guard:").

#pragma once

#include "vr_move.hpp"

namespace qvr::moveguard
{

// The bounds, in metres (times unitsPerMetre: the player's world scale) and seconds.
inline constexpr float reachMetres = 3.f;     // a place (hand, muzzle, port, head, lamp...) from the move's origin
inline constexpr float scaleFloor = 1.5f;     // ... at a world scale of at least this (vr_world_scale 1.2 by default)
inline constexpr float boxMetres = 1.f;       // a magazine's or the ammo pouch's half-axis at most
inline constexpr float launchMetres = 1.5f;   // a projectile's start from its muzzle (the AD crossbow's bolt)
inline constexpr float speedMost = 40.f;      // m/s: a hand's, a throw's, the head's
inline constexpr float spinMost = 150.f;      // rad/s: a hand's turn (a throw's spin)
inline constexpr float walkMost = 6.f;        // m/s: the room-scale walk over time (a walking head's, at 2x Room Scale)
inline constexpr float walkBurst = 2.f;       // m: ... and at once (a lean recentred, the body swung by a turn)
inline constexpr float fistReachMetres = 0.3f; // a fist sphere's centre from the hand's own point
inline constexpr float fistRadiusMetres = 0.1f; // ... and its radius
inline constexpr double rewindMost = 0.55;    // s: how far behind his newest message a client says he drew the others
                                              // (his interpolation buffer's delay, at most 0.25 s, as far again before
                                              // its clock is set -- vr_netsnap.cpp delayMost, snapAfter -- and a few
                                              // frames)

// Quake units per metre for a player at world scale `worldScale` (at least scaleFloor).
[[nodiscard]] float unitsPerMetre(float worldScale);

// `v` with its length at most `most` (not finite: zero); `held` counts one when it changed.
[[nodiscard]] glm::vec3 within(const glm::vec3& v, float most, int& held);
[[nodiscard]] float within(float v, float most, int& held);

// What the prediction replays (predict::inputOf), bounded as the server bounds the move: place `p` (world) relative to
// the move's origin (a place at the world's origin is none: as sent), a speed (m/s).
[[nodiscard]] glm::vec3 relative(const glm::vec3& p, const glm::vec3& origin, float upm);
[[nodiscard]] glm::vec3 speed(const glm::vec3& v);

// The values one move held, by kind.
struct Held
{
    int places{0};
    int speeds{0};
};

// Server: move `m` bounded, its places from `base` (the move's origin; the server's own of him when the move has none),
// for a player at `upm` units a metre.
[[nodiscard]] Held guard(VrMove& m, const glm::vec3& base, float upm, int slot = -1); // (slot: vr_mp_guard_debug's lines)

// Server: client `slot`'s world scale as his avatar calibration says (vr_mpavatar.cpp; scaleFloor until it came).
[[nodiscard]] float slotScale(int slot);

// Server: client `slot`'s room-scale walk `v` (units a second, x and y) over `dt` seconds within his budget (walkMost a
// second, walkBurst at most saved up): what is past it taken off and counted.
void roomscale(int slot, float v[3], double dt, float upm);
// The budget's rule (roomscale's, and his replay's on a copy of the server's, MP_PLAN review B6): `walkLeft` (units
// saved up; -1 full) refilled for `dt`, then `v` cut to what is left and that taken off -> whether it was cut.
bool budget(float& walkLeft, float v[3], double dt, float upm);
// Server: client `slot`'s budget left after his newest move queued (-1: full; QVR_SVC_PREDICT carries it).
[[nodiscard]] float walkLeft(int slot);

// Server: each client's counts (vr_mp_status).
struct Counts
{
    int moves{0};   // moves bounded
    int places{0};  // places held within reach
    int speeds{0};  // speeds held
    int walks{0};   // room-scale moves cut to the budget
    int fists{0};   // fist spheres held (vr_rig)
    int spots{0};   // grab-spot claims refused (out of the hand's reach)
    int rewinds{0}; // lag compensation's rewinds held
};
void count(int slot, const Held& h);
void countFist(int slot, int n);
void countSpot(int slot);
void countRewind(int slot);
[[nodiscard]] Counts counts(int slot);
void serverReset(int slot);

// Client (vr_mp_test_cheat, the harness's mp_guard): move `m` made to lie as `bits` say (the cvar's comment), its time
// and view's delay too.
void cheatForTest(int bits, VrMove& m, float& time, float& viewBack);

// vr_moveguard_selftest: the bounds on made-up moves ("moveguard: N passed, M failed").
void init();

} // namespace qvr::moveguard
