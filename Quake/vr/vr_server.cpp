// vr_server.cpp -- server side of the Quake VR protocol extensions (see vr_protocol.hpp).

#include "vr_move.hpp"
#include "vr_engine.hpp"
#include "vr_physics.hpp"
#include "vr_progs.hpp"
#include "vr_protocol.hpp"
#include "vr_server.hpp"
#include "vr_worldtext.hpp"

#include <iterator>
#include <vector>

using namespace qvr;
using namespace qvr::progs;
using namespace qvr::protocol;

namespace
{

// Per client: the VR bits in effect during the previous server frame, and those received
// in moves this frame (current state only; see VrBits0 in vr_client.cpp).
struct ClientBits
{
    int previousFrame{0};
    int received{0};
};

std::vector<ClientBits> clientBits;

// vrbits0 pairs a "current" bit with the "previous" bit just above it.
constexpr int currentBitsMask = (1 << 1) | (1 << 3) | (1 << 6) | (1 << 8) | (1 << 10) | (1 << 12);

[[nodiscard]] int withPreviousBits(int current, int previousFrame)
{
    return current | ((previousFrame & currentBitsMask) << 1);
}

// Precaches the clients already know about (from serverinfo or a previous broadcast).
int broadcastModelCount = 0;
int broadcastSoundCount = 0;

template <int N>
[[nodiscard]] int precacheCount(const char* const (&list)[N])
{
    int count = 0;
    while(count < N && list[count])
    {
        count++;
    }
    return count;
}

template <int N>
void broadcastNewPrecaches(const char* const (&list)[N], int& known, int subcmd)
{
    const int count = precacheCount(list);
    for(; known < count; known++)
    {
        MSG_WriteByte(&sv.reliable_datagram, svc_quakevr);
        MSG_WriteByte(&sv.reliable_datagram, subcmd);
        MSG_WriteShort(&sv.reliable_datagram, known);
        MSG_WriteString(&sv.reliable_datagram, list[known]);
    }
}

[[nodiscard]] bool vrProtocol()
{
    return (sv.protocolflags & PRFL_QUAKEVR) != 0;
}

[[nodiscard]] int modelIndexOfField(edict_t* ent, int ofs)
{
    if(ofs < 0 || !fieldInt(ent, ofs))
    {
        return 0;
    }

    return SV_ModelIndex(PR_GetString(fieldInt(ent, ofs)));
}

} // namespace

extern "C" void VR_ReadMoveExtras(client_t* client)
{
    if(!vrProtocol())
    {
        return;
    }

    const VrMove move = readVrMove();

    edict_t* ent = client->edict;
    const FieldOffsets& f = fields();

    setFieldVec(ent, f.v_viewangle, move.headAngles);
    setFieldFloat(ent, f.vryaw, move.vrYaw);

    const auto setHand = [&](const VrHandMove& hand, int pos, int rot, int vel,
                             int throwVel, int velMag, int angVel, int throwPos, int throwAge) {
        setFieldVec(ent, pos, hand.pos);
        setFieldVec(ent, rot, hand.rot);
        setFieldVec(ent, vel, hand.vel);
        setFieldVec(ent, throwVel, hand.throwVel);
        setFieldFloat(ent, velMag, hand.velMag);
        setFieldVec(ent, angVel, hand.angVel);
        setFieldVec(ent, throwPos, hand.throwPos);
        setFieldFloat(ent, throwAge, hand.throwAge);
    };

    setHand(move.hands[0], f.offhandpos, f.offhandrot, f.offhandvel,
        f.offhandthrowvel, f.offhandvelmag, f.offhandavel, f.offhandthrowpos, f.offhandthrowage);
    setHand(move.hands[1], f.handpos, f.handrot, f.handvel, f.handthrowvel,
        f.handvelmag, f.handavel, f.handthrowpos, f.handthrowage);

    setFieldVec(ent, f.headvel, move.headVel);
    setFieldVec(ent, f.offmuzzlepos, move.muzzlePos[0]);
    setFieldVec(ent, f.muzzlepos, move.muzzlePos[1]);
    const int clientNum = static_cast<int>(client - svs.clients);
    if(clientNum >= static_cast<int>(clientBits.size()))
    {
        clientBits.resize(clientNum + 1);
    }
    ClientBits& bits = clientBits[clientNum];
    bits.received = move.vrBits0;
    setFieldFloat(ent, f.vrbits0, static_cast<float>(withPreviousBits(bits.received, bits.previousFrame)));
    setFieldVec(ent, f.teleport_target, move.teleportTarget);
    setFieldFloat(ent, f.offhand_hotspot, move.hotspots[0]);
    setFieldFloat(ent, f.mainhand_hotspot, move.hotspots[1]);
    setFieldVec(ent, f.roomscalemove, move.roomscaleMove);
    setFieldFloat(ent, f.button3, (move.buttons & QVR_BUTTON_OFFHANDATTACK) ? 1.f : 0.f);
    physics::setClientHandsTracked(clientNum, (move.buttons & QVR_BUTTON_HANDSTRACKED) != 0);
}

// The status bar highlights the active weapon by its item bit; with the Quake VR progs .weapon
// holds a weapon ID (QC WID_*). Weapons without a classic slot highlight nothing.
extern "C" int VR_ActiveWeaponStat(edict_t* ent)
{
    if(!bindings().isVrProgs)
    {
        return static_cast<int>(ent->v.weapon);
    }

    static constexpr int itemBits[] = {
        0,                   // fist
        0,                   // grappling hook
        IT_AXE,              // axe
        0,                   // Mjolnir
        IT_SHOTGUN,          // shotgun
        IT_SUPER_SHOTGUN,    // super shotgun
        IT_NAILGUN,          // nailgun
        IT_SUPER_NAILGUN,    // super nailgun
        IT_GRENADE_LAUNCHER, // grenade launcher
        IT_GRENADE_LAUNCHER, // proximity gun
        IT_ROCKET_LAUNCHER,  // rocket launcher
        IT_LIGHTNING,        // lightning gun
        0,                   // laser cannon
    };
    const int wid = static_cast<int>(ent->v.weapon);
    return wid >= 0 && wid < static_cast<int>(std::size(itemBits)) ? itemBits[wid] : 0;
}

extern "C" void VR_CalcStats(client_t* client, int* statsi, float* statsf)
{
    if(!bindings().isVrProgs)
    {
        return;
    }

    edict_t* ent = client->edict;
    const FieldOffsets& f = fields();

    const auto stat = [&](int index, int ofs) { statsf[index] = fieldFloatOr(ent, ofs, 0.f); };

    statsf[STAT_QVR_WEAPON] = ent->v.weapon;

    statsf[STAT_ACTIVEWEAPON] = static_cast<float>(VR_ActiveWeaponStat(ent));
    stat(STAT_QVR_WEAPON2, f.weapon2);
    statsi[STAT_QVR_WEAPONMODEL2] = modelIndexOfField(ent, f.weaponmodel2);
    stat(STAT_QVR_WEAPONFRAME2, f.weaponframe2);
    stat(STAT_QVR_WEAPONFLAGS, f.weaponflags);
    stat(STAT_QVR_WEAPONFLAGS2, f.weaponflags2);
    stat(STAT_QVR_AMMO2, f.currentammo2);
    stat(STAT_QVR_AMMOCOUNTER, f.ammocounter);
    stat(STAT_QVR_AMMOCOUNTER2, f.ammocounter2);
    stat(STAT_QVR_WEAPONCLIP, f.weaponclip);
    stat(STAT_QVR_WEAPONCLIP2, f.weaponclip2);
    stat(STAT_QVR_WEAPONCLIPSIZE, f.weaponclipsize);
    stat(STAT_QVR_WEAPONCLIPSIZE2, f.weaponclipsize2);

    const int holsterWeapon[numHolsters] = {f.holsterweapon0, f.holsterweapon1,
        f.holsterweapon2, f.holsterweapon3, f.holsterweapon4, f.holsterweapon5};
    const int holsterModel[numHolsters] = {f.holsterweaponmodel0,
        f.holsterweaponmodel1, f.holsterweaponmodel2, f.holsterweaponmodel3,
        f.holsterweaponmodel4, f.holsterweaponmodel5};
    const int holsterFlags[numHolsters] = {f.holsterweaponflags0,
        f.holsterweaponflags1, f.holsterweaponflags2, f.holsterweaponflags3,
        f.holsterweaponflags4, f.holsterweaponflags5};
    const int holsterClip[numHolsters] = {f.holsterweaponclip0, f.holsterweaponclip1,
        f.holsterweaponclip2, f.holsterweaponclip3, f.holsterweaponclip4,
        f.holsterweaponclip5};

    for(int i = 0; i < numHolsters; i++)
    {
        stat(STAT_QVR_HOLSTERWEAPON0 + i, holsterWeapon[i]);
        statsi[STAT_QVR_HOLSTERWEAPONMODEL0 + i] = modelIndexOfField(ent, holsterModel[i]);
        stat(STAT_QVR_HOLSTERWEAPONFLAGS0 + i, holsterFlags[i]);
        stat(STAT_QVR_HOLSTERWEAPONCLIP0 + i, holsterClip[i]);
    }
}

extern "C" int VR_EntityUpdateBits(edict_t* ent)
{
    if(!vrProtocol())
    {
        return 0;
    }

    // Baselines don't carry these (they are zero), so any non-zero value is sent.
    const FieldOffsets& f = fields();
    int bits = 0;
    if(fieldVec(ent, f.model_scale) != glm::vec3{0.f})
    {
        bits |= U_QVR_SCALE;
    }
    if(fieldVec(ent, f.model_scale_origin) != glm::vec3{0.f})
    {
        bits |= U_QVR_SCALEORIGIN;
    }
    if(fieldVec(ent, f.model_offset) != glm::vec3{0.f})
    {
        bits |= U_QVR_OFFSET;
    }
    if(fieldFloatOr(ent, f.vr_rigid, 0.f) != 0.f)
    {
        bits |= U_QVR_NOROTATE;
    }
    return bits;
}

extern "C" void VR_WriteEntityUpdate(sizebuf_t* msg, edict_t* ent, int bits)
{
    const FieldOffsets& f = fields();

    if(bits & U_QVR_SCALE)
    {
        const glm::vec3 v = fieldVec(ent, f.model_scale);
        for(int i = 0; i < 3; i++)
        {
            MSG_WriteFloat(msg, v[i]);
        }
    }

    const auto writeCoords = [&](int ofs) {
        const glm::vec3 v = fieldVec(ent, ofs);
        for(int i = 0; i < 3; i++)
        {
            MSG_WriteCoord(msg, v[i], sv.protocolflags);
        }
    };

    if(bits & U_QVR_SCALEORIGIN)
    {
        writeCoords(f.model_scale_origin);
    }
    if(bits & U_QVR_OFFSET)
    {
        writeCoords(f.model_offset);
    }
}

extern "C" void VR_WriteClientSpawnState(sizebuf_t* msg)
{
    if(vrProtocol())
    {
        worldtext::serverWriteAll(msg);
    }
}

extern "C" void VR_ServerFrameEnd()
{
    if(!vrProtocol())
    {
        return;
    }

    // This frame's VR bits become next frame's "previous" bits.
    for(ClientBits& bits : clientBits)
    {
        bits.previousFrame = bits.received;
    }

    // Late precaches (setmodel on an unprecached model, precache_* after load).
    broadcastNewPrecaches(sv.model_precache, broadcastModelCount, QVR_SVC_PRECACHE_MODEL);
    broadcastNewPrecaches(sv.sound_precache, broadcastSoundCount, QVR_SVC_PRECACHE_SOUND);
}

namespace qvr::server
{

// vr_dumpplayer: prints the local player's VR fields as the server sees them.
void dumpPlayer_f()
{
    if(!sv.active || !bindings().isVrProgs)
    {
        Con_Printf("vr_dumpplayer: no server running Quake VR progs\n");
        return;
    }

    edict_t* ent = svs.clients[0].edict;
    const FieldOffsets& f = fields();
    const auto vec = [&](const char* name, int ofs) {
        const glm::vec3 v = fieldVec(ent, ofs);
        Con_Printf("  %-14s %8.2f %8.2f %8.2f\n", name, v.x, v.y, v.z);
    };

    Con_Printf("origin         %8.2f %8.2f %8.2f\n", ent->v.origin[0], ent->v.origin[1],
        ent->v.origin[2]);
    vec("v_viewangle", f.v_viewangle);
    vec("handpos", f.handpos);
    vec("handrot", f.handrot);
    vec("handvel", f.handvel);
    vec("muzzlepos", f.muzzlepos);
    vec("offhandpos", f.offhandpos);
    vec("offmuzzlepos", f.offmuzzlepos);
    Con_Printf("  vrbits0 %d  button3 %g  weapon %g  weapon2 %g\n",
        static_cast<int>(fieldFloatOr(ent, f.vrbits0, 0.f)), fieldFloatOr(ent, f.button3, 0.f),
        ent->v.weapon, fieldFloatOr(ent, f.weapon2, 0.f));
}

void sendHaptic(edict_t* player, int hand, float delay, float duration, float frequency, float amplitude)
{
    const int client = NUM_FOR_EDICT(player) - 1;
    if(client < 0 || client >= svs.maxclients || !svs.clients[client].active)
    {
        return;
    }

    sizebuf_t* msg = &svs.clients[client].message;
    MSG_WriteByte(msg, svc_quakevr);
    MSG_WriteByte(msg, QVR_SVC_HAPTIC);
    MSG_WriteByte(msg, hand);
    MSG_WriteFloat(msg, delay);
    MSG_WriteFloat(msg, duration);
    MSG_WriteFloat(msg, frequency);
    MSG_WriteFloat(msg, amplitude);
}

void init()
{
    Cmd_AddCommand("vr_dumpplayer", dumpPlayer_f);
}

// Everything precached while loading is in the serverinfo that clients receive.
void onSpawnServerAfterLoad()
{
    broadcastModelCount = precacheCount(sv.model_precache);
    broadcastSoundCount = precacheCount(sv.sound_precache);
}

} // namespace qvr::server
