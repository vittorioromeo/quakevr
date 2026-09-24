// vr_protocol.hpp -- Quake VR extensions to Ironwail's PROTOCOL_RMQ.
//
// A server running Quake VR progs sets PRFL_QUAKEVR (vr_api.h) in the RMQ protocol flags.
// Both ends then add:
//   - clc_move: a VR block after the vanilla fields (head, hands, muzzles, VR bits, ...);
//   - entity updates: model scale / scale origin / offset in update bits 24..26;
//   - svc_quakevr (39) + sub-command for particles, late precaches and world text;
//   - an extra "beam id" byte in TE_LIGHTNING1-3 / TE_BEAM, so one entity can own two beams;
//   - VR stats (weapons in both hands, holsters, clips) in stat slots 64+, sent through
//     Ironwail's generic stat channel.

#pragma once

namespace qvr::protocol
{

// Entity update bits (above U_EXTEND2, so only present when bits >= 1 << 24).
inline constexpr int U_QVR_SCALE = 1 << 24;        // 3 floats
inline constexpr int U_QVR_SCALEORIGIN = 1 << 25;  // 3 coords
inline constexpr int U_QVR_OFFSET = 1 << 26;       // 3 coords
inline constexpr int U_QVR_ANY = U_QVR_SCALE | U_QVR_SCALEORIGIN | U_QVR_OFFSET;

// Server -> client.
inline constexpr int svc_quakevr = 39;

enum SvcQuakeVr : int
{
    QVR_SVC_PARTICLE2 = 1,      // [coord3 org][char3 dir*16][byte preset][short count]
    QVR_SVC_PRECACHE_MODEL = 2, // [short index][string name]
    QVR_SVC_WORLDTEXT_MAKE = 3, // [short handle]
    QVR_SVC_WORLDTEXT_TEXT = 4, // [short handle][string]
    QVR_SVC_WORLDTEXT_POS = 5,  // [short handle][coord3]
    QVR_SVC_WORLDTEXT_ANGLES = 6, // [short handle][float3]
    QVR_SVC_WORLDTEXT_HALIGN = 7, // [short handle][byte 0 left, 1 centre, 2 right]
    QVR_SVC_WORLDTEXT_SCALE = 8,  // [short handle][float]
};

// Client -> server: clc_move VR block button bits.
inline constexpr int QVR_BUTTON_OFFHANDATTACK = 1 << 0; // -> .button3

// VR stats (cl.stats / cl.statsf indices).
enum Stat : int
{
    STAT_QVR_FIRST = 64,

    STAT_QVR_WEAPON = STAT_QVR_FIRST, // main hand weapon id (.weapon)
    STAT_QVR_WEAPON2,                 // off-hand weapon id
    STAT_QVR_WEAPONMODEL2,            // model index
    STAT_QVR_WEAPONFRAME2,
    STAT_QVR_WEAPONFLAGS,
    STAT_QVR_WEAPONFLAGS2,
    STAT_QVR_AMMO2,                   // .currentammo2
    STAT_QVR_AMMOCOUNTER,
    STAT_QVR_AMMOCOUNTER2,
    STAT_QVR_WEAPONCLIP,
    STAT_QVR_WEAPONCLIP2,
    STAT_QVR_WEAPONCLIPSIZE,
    STAT_QVR_WEAPONCLIPSIZE2,
    STAT_QVR_HOLSTERWEAPON0,          // 6 each: weapon id, model index, flags, clip
    STAT_QVR_HOLSTERWEAPONMODEL0 = STAT_QVR_HOLSTERWEAPON0 + 6,
    STAT_QVR_HOLSTERWEAPONFLAGS0 = STAT_QVR_HOLSTERWEAPONMODEL0 + 6,
    STAT_QVR_HOLSTERWEAPONCLIP0 = STAT_QVR_HOLSTERWEAPONFLAGS0 + 6,

    STAT_QVR_END = STAT_QVR_HOLSTERWEAPONCLIP0 + 6
};

inline constexpr int numHolsters = 6;

} // namespace qvr::protocol
