// vr_server.cpp -- server side of the Quake VR protocol extensions (see vr_protocol.hpp).

#include "vr_rigseq.hpp"
#include "vr_lagcomp.hpp"
#include "vr_disarmviz.hpp"
#include "vr_mem.hpp"
#include "vr_rigstudio.hpp"
#include "vr_fatigue.hpp"
#include "vr_axestick.hpp"
#include "vr_agarmor.hpp"
#include "vr_arenaxray.hpp"
#include "vr_humlook.hpp"
#include "vr_climbrope.hpp"
#include "vr_box3d.hpp"
#include "vr_heldpose.hpp"
#include "vr_hitmodel.hpp"
#include "vr_limbmodel.hpp"
#include "vr_hull.hpp"
#include "vr_climb.hpp"
#include "vr_bodycontact.hpp"
#include "vr_foegrab.hpp"
#include "vr_legsweep.hpp"
#include "vr_cvars.hpp"
#include "vr_explosiondebris.hpp"
#include "vr_ledges.hpp"
#include "vr_move.hpp"
#include "vr_moveguard.hpp"
#include "vr_mpavatar.hpp"
#include "vr_voicechat.hpp"
#include "vr_clientprefs.hpp"
#include "vr_motion.hpp"
#include "vr_engine.hpp"
#include "vr_flatplay.hpp"
#include "vr_progs.hpp"
#include "vr_protocol.hpp"
#include "vr_server.hpp"
#include "vr_serverrules.hpp"
#include "vr_tracy.h"
#include "vr_seated.hpp"
#include "vr_shock.hpp"
#include "vr_units.hpp"
#include "vr_view.hpp"
#include "vr_portals.hpp"
#include "vr_testkit.hpp"
#include "vr_tips.hpp"
#include "vr_worldtext.hpp"
#include "vr_slide.hpp"

#include "Zancle/Base/GetArraySize.hpp"
#include "Zancle/Base/Memcmp.hpp"
#include "Zancle/Base/Memcpy.hpp"
#include "Zancle/Container/InPlaceVector.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Vocabulary/Optional.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "vr_api.h"
#include "vr_netmove.hpp"
#include "vr_predict.hpp"
#include "vr_netsnap.hpp"
#include "vr_netragdoll.hpp"


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
    int moves{0}; // moves read since the server's last frame (VR_MoveMerges): after the first, their bits are ORed
    int merged{0}; // moves merged since he connected (server::mergedMoves)
};

za::Vector<ClientBits> clientBits;

// Per client: the latest VR move (see server::clientMove).
struct ClientMove
{
    bool valid{false};
    VrMove move;
    glm::vec3 base{0.f}; // the player origin the hand fields are placed from (see rebaseHands)
};
za::Vector<ClientMove> clientMoves;

// (B28) Per client slot: a guest's say, say_team and tell flood guard (QuakeWorld's fp_messages: more than talkBurst
// messages in talkWindow seconds silences him for talkSilence; realtime): his last messages' times, a ring
constexpr int talkBurst = 4;
constexpr double talkWindow = 8.0;
constexpr double talkSilence = 10.0;
struct TalkGuard
{
    double times[talkBurst]{};
    int next{0};
    int count{0};
    double silentUntil{0.0};
};
za::Vector<TalkGuard> talkGuards;

// Per client slot: his string commands' budget (VR_ClientCommandBudget; realtime). Each command takes one, cmdRate come
// back a second, at most cmdBurst saved up: a client's own reliable state (his fists' vr_rig chunks, his palm frames,
// preferences, avatar calibration, held poses, grab spots: each sent when it changes, the first all at once as he spawns)
// stays far under it; a flood of them (vr_avatarcal is passed on to every client, reliably), of name or colour changes
// or of the harness's test shots is dropped past it (review 2026-11-18, finding 9; MP_PLAN B29).
constexpr double cmdBurst = 64.0;
constexpr double cmdRate = 32.0;
constexpr double cmdNoteEvery = 1.0; // s: the server's console told at most this often a client
struct CommandBudget
{
    double tokens{-1.0}; // (-1: full, a new player)
    double last{0.0};
    double least{cmdBurst}; // the fewest left after a command (how near his own state came: vr_mp_status)
    int used{0}, dropped{0};
    double noteAt{0.0};
};
za::Vector<CommandBudget> commandBudgets;

// Per client slot: its avatar as last sent (writeAvatars: an unchanged one waits for its keyframe); and the counts.
constexpr int avatarMaxBytes = 520; // (2 + 2 + 1 + 12 + 8 + 2 * 47 + 8 + 2 * 7 holds + 3 + 6 * 16 holsters + 1 = 241; the guns' looks and the torch 2 * 12 + 12 + 19: 296; the worn gear 4 + 11 * 17 + 2 (the ammo pouch's rounds): 489 at most)
struct AvatarSent
{
    byte bytes[avatarMaxBytes]{};
    int size{0};
    int unsent{0}; // frames since it was last sent
};
za::Vector<AvatarSent> avatarsSent;
struct AvatarStats
{
    int sent{0}, skipped{0};
    long long bytes{0};
};
AvatarStats avatarStats;

[[nodiscard]] ClientMove* clientMoveOf(edict_t* player)
{
    const int client = progs::clientSlot(player);
    if(client < 0 || client >= static_cast<int>(clientMoves.size()))
    {
        return nullptr;
    }
    ClientMove& m = clientMoves[client];
    return m.valid ? &m : nullptr;
}

// The reliable message buffer of the client `player` is, or nullptr for an inactive one.
[[nodiscard]] sizebuf_t* clientMessage(edict_t* player)
{
    const int client = progs::clientSlot(player);
    if(client < 0 || !svs.clients[client].active)
    {
        return nullptr;
    }
    return &svs.clients[client].message;
}

// The move's teleport bit (vr_client.cpp's VRBITS0_TELEPORTING: the spot in VrMove::teleportTarget).
constexpr int vrBitsTeleporting = 1 << 0;

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

// A late precache is also repeated at the head of each client's datagrams for a moment (VR_WriteRecentPrecaches): the
// datagram is sent before the reliable message (and a reliable one waits for the last one's ack), so the entity made in
// the same frame, with the new model, or the sound started on it would reach the client before the name and be lost (a
// model drawn as nothing until its next update; the sound never heard). The repeats are harmless (the client knows it).
struct RecentPrecache
{
    int subcmd{0};
    int index{0};
    const char* name{nullptr};
    double until{0.0}; // (the server's time)
};
constexpr double recentPrecacheTime = 0.5;
za::InPlaceVector<RecentPrecache, 64> recentPrecaches;

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
        if(recentPrecaches.size() == recentPrecaches.capacity())
        {
            recentPrecaches.erase(recentPrecaches.begin()); // (the oldest: its reliable copy is on its way)
        }
        recentPrecaches.pushBack(RecentPrecache{subcmd, known, list[known], qcvm->time + recentPrecacheTime});
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

// ----------------------------------------------------------------------------
// The weapons' records (QC vr_weaponinst.qc): each weapon is an entity of its own (no model) with its id (.wi_uid) and
// its magazine (.wi_clip), which the hand, the holster or the prop that has the weapon refers to (.weaponinst,
// .weaponinst2, .holsterweaponinst0..5). Moving a weapon moves the reference; a record nothing refers to at the end of a
// frame (the weapon gone: discarded, replaced, its prop removed) is freed here, and the clients told its id is gone
// (QVR_SVC_WEAPONGONE: its blood forgotten, vr_wounds.cpp).

za::Vector<unsigned char> instReferenced; // per edict, this frame's sweep: something refers to it

// The record `ent`'s field `refOfs` refers to, or null.
[[nodiscard]] edict_t* weaponInst(edict_t* ent, int refOfs)
{
    if(refOfs < 0 || fields().wi_uid < 0)
    {
        return nullptr;
    }
    const int v = fieldInt(ent, refOfs);
    if(v <= 0)
    {
        return nullptr;
    }
    edict_t* r = PROG_TO_EDICT(v);
    return r->free ? nullptr : r;
}

[[nodiscard]] int weaponUid(edict_t* inst)
{
    return inst ? static_cast<int>(fieldFloat(inst, fields().wi_uid)) : 0;
}

// The magazine size of weapon `wid` whose record is `inst` (QC VR_ClipSizeOf: the kind's own, the Widowmaker's tube), 0
// when unknown: every gun's, held or not, for its ammo screen's warning colour.
[[nodiscard]] int clipSizeOf(float wid, edict_t* inst)
{
    const func_t fn = bindings().ClipSizeOf;
    if(!fn || wid <= 0.f || !sv.active)
    {
        return 0;
    }
    qcvm_t* old = nullptr;
    PR_PushQCVM(&sv.qcvm, &old);
    G_FLOAT(OFS_PARM0) = wid;
    G_INT(OFS_PARM1) = inst ? EDICT_TO_PROG(inst) : 0;
    PR_ExecuteProgram(fn);
    const int size = static_cast<int>(G_FLOAT(OFS_RETURN));
    PR_PopQCVM(old);
    return za::clamp(size, 0, 255);
}

// Whether the run refuses `ent` (QC VR_Run_RefusesEnt; Melee Only: a gun, ammunition, a backpack with rounds): no loot
// beam. Asked only while a mutator that refuses things is on (Melee Only, the only one): no VM call per entity otherwise.
[[nodiscard]] bool runRefuses(edict_t* ent)
{
    const func_t fn = bindings().RunRefusesEnt;
    if(!fn || !sv.active || vr_run_meleeonly.value == 0.f)
    {
        return false;
    }
    progs::ServerVm vm;
    G_INT(OFS_PARM0) = EDICT_TO_PROG(ent);
    PR_ExecuteProgram(fn);
    return G_FLOAT(OFS_RETURN) != 0.f;
}

void sweepWeaponInsts()
{
    const FieldOffsets& f = fields();
    if(!bindings().isVrProgs || f.wi_uid < 0 || f.weaponinst < 0 || !sv.active)
    {
        return;
    }
    const int refs[] = {f.weaponinst, f.weaponinst2, f.holsterweaponinst0, f.holsterweaponinst1, f.holsterweaponinst2,
        f.holsterweaponinst3, f.holsterweaponinst4, f.holsterweaponinst5};
    const int count = qcvm->num_edicts;
    instReferenced.clear();
    instReferenced.resize(static_cast<za::SizeT>(count), 0);
    for(int i = 1; i < count; i++)
    {
        edict_t* e = EDICT_NUM(i);
        if(e->free)
        {
            continue;
        }
        for(const int ofs : refs)
        {
            if(edict_t* r = weaponInst(e, ofs))
            {
                const int n = NUM_FOR_EDICT(r);
                if(n > 0 && n < count)
                {
                    instReferenced[static_cast<za::SizeT>(n)] = 1;
                }
            }
        }
    }
    for(int i = 1; i < count; i++)
    {
        edict_t* e = EDICT_NUM(i);
        const float uid = e->free ? 0.f : fieldFloat(e, f.wi_uid);
        if(uid <= 0.f || instReferenced[static_cast<za::SizeT>(i)])
        {
            continue;
        }
        Con_DPrintf("weapon #%d gone (its record %d freed: nothing has it)\n", static_cast<int>(uid), i);
        ED_Free(e);
        if(vrProtocol() && sv.reliable_datagram.cursize + 6 <= sv.reliable_datagram.maxsize)
        {
            MSG_WriteByte(&sv.reliable_datagram, svc_quakevr);
            MSG_WriteByte(&sv.reliable_datagram, QVR_SVC_WEAPONGONE);
            MSG_WriteLong(&sv.reliable_datagram, static_cast<int>(uid));
        }
    }
}

// The unreliable broadcast's room (sv.datagram; ROUND21.md, "A full broadcast drops whole messages").
//
// sv.datagram (particles, sounds, temp entities, QuakeC's MSG_BROADCAST writes, the VR events) goes to each client
// after its entities. The engine's writers check its room and skip a message that does not fit; QuakeC's writes could
// not, and a frame of many shotgun pellets (TE_GUNSHOT each) overflowed it into a Host_Error. A message cut short
// garbles the rest of the stream, so its message boundaries are kept: the start of each QuakeC run from the engine (not
// one nested in a builtin), each point where QuakeC writes after the engine has (the engine writes whole messages, and
// only between QuakeC's: one in the middle of QuakeC's would garble it anyway), and the end of each temp entity QuakeC
// writes from a boundary (their lengths are the client's, CL_ParseTEnt; a run of TE_GUNSHOTs has no other boundaries;
// another message QuakeC writes leaves the next boundary to the others). A QuakeC write that does not fit
// takes the buffer back to the last boundary, and QuakeC's broadcast writes then go nowhere until the next one. A
// client's datagram takes the longest part of it that ends on a boundary and fits after its entities (it was all of it
// or none).
// (B13c) A remote client's share of the broadcast of the frames he got no datagram (vr_net_sendhz), for his next one.
struct HeldBroadcast
{
    za::U8 data[DATAGRAM_MTU];
    int size{0};
};
HeldBroadcast heldBroadcast[MAX_SCOREBOARD];
struct HeldCounts
{
    za::U64 held{0}, dropped{0}; // (vr_net_broadcast_report)
};
HeldCounts heldCounts;

struct BroadcastRoom
{
    static constexpr int maxMarks = 1024; // full: every other one is dropped (spread evenly, the last kept)

    int marks[maxMarks]; // ascending boundaries past 0 (0 and the end at sending are boundaries too)
    int numMarks;
    int qcEnd;  // where QuakeC's last write ended (-1: none since the last boundary QuakeC's run made)
    int dropAt; // >= 0: QuakeC's writes go to the sink while sv.datagram stays this long (-1: they go to it)
    int msgAt;  // where QuakeC's message being written began, at a boundary (-1: not known)

    sizebuf_t sink; // a dropped write's bytes (cleared before each)
    byte sinkBuf[MAX_DATAGRAM];

    // vr_debug_net: this frame's
    int qcDropped; // bytes QuakeC wrote that went nowhere (and those taken back)
    int qcDrops;   // times QuakeC's writes overflowed
    int unsent;    // bytes a client's datagram had no room for (each client's)
    int entities;  // the most a client's datagram had before it (svc_time, the client's data, the entities)
    // ... and the most in the second
    double since;
    int peakSize, peakReliable, peakEntities;

    // Whole messages the engine's console commands broadcast (vr_physics_blast's explosion: server::queueBroadcast). A
    // command runs before the server frame, whose SV_ClearDatagram would drop what it wrote; they open the next frame's.
    static constexpr int maxQueued = 16;
    byte queued[512];
    int queuedEnds[maxQueued]; // each message's end in queued
    int numQueued;
};

BroadcastRoom broadcastRoom{};

// Messages about one entity (an armed humanoid's pose, QVR_SVC_ARMPOSE: server::scopedBegin) this frame, written after the
// broadcast into the datagram of each client whose entity update carried that entity (its PVS: VR_NoteEntityInSight)
// instead of into every client's (they were about 1.2 KB of each 1.4 KB datagram on e1m1, wherever the client was:
// MP_PLAN.md, B2). The loopback client (single player, a listen host) gets them all, as before.
struct ScopedRoom
{
    static constexpr int maxMsgs = 1024;
    static constexpr int msgRoom = 256; // a message's most (a pose with every block is about 150 B)

    sizebuf_t room;
    byte roomBuf[48 * 1024];
    sizebuf_t sink; // a message with no room left: dropped (as a full broadcast drops one)
    byte sinkBuf[msgRoom];
    byte poseJoin[4 * msgRoom]; // an entity's poses of one frame as one snapshot record (VR_SnapEnd; one, as a rule)
    int ents[maxMsgs];
    int ends[maxMsgs]; // each message's end in roomBuf (the first begins at 0)
    int kinds[maxMsgs]; // ScopedKind: an avatar goes in a snapshot client's snapshot (B7b), an unchanged one to no other
    int numMsgs;
    int openEnt; // the message being written: its entity (-1: none)

    // the entities the client being sent to has in sight: seen[e] == stamp (one stamp a client's datagram)
    za::Vector<int> seen;
    int stamp;
    int dropped; // messages with no room this frame (vr_debug_net)
};

ScopedRoom scopedRoom{};

// The length of the temp entity QuakeC began at `at`, as CL_ParseTEnt reads it (0: not one, not known, or its type not
// written yet).
[[nodiscard]] int tempEntityLength(int at)
{
    if(sv.datagram.cursize - at < 2 || sv.datagram.data[at] != svc_temp_entity)
    {
        return 0;
    }
    int coord = 0; // a coordinate's bytes in the protocol
    {
        byte bytes[8];
        sizebuf_t probe{};
        probe.data = bytes;
        probe.maxsize = sizeof(bytes);
        MSG_WriteCoord(&probe, 0.f, sv.protocolflags);
        coord = probe.cursize;
    }
    switch(sv.datagram.data[at + 1])
    {
        case TE_SPIKE:
        case TE_SUPERSPIKE:
        case TE_GUNSHOT:
        case TE_EXPLOSION:
        case TE_TAREXPLOSION:
        case TE_WIZSPIKE:
        case TE_KNIGHTSPIKE:
        case TE_LAVASPLASH:
        case TE_TELEPORT: return 2 + 3 * coord;              // where
        case TE_EXPLOSION2: return 2 + 3 * coord + 2;        // where, the colours
        case TE_LIGHTNING1:
        case TE_LIGHTNING2:
        case TE_LIGHTNING3:
        case TE_BEAM:                                         // the entity (and Quake VR's beam id: VR_ParseBeamEntity), from, to
            return 2 + 2 + ((sv.protocolflags & PRFL_QUAKEVR_PROGS) ? 1 : 0) + 6 * coord;
        default: return 0;
    }
}

// A whole temp entity QuakeC wrote at `at`: an explosion's (TE_EXPLOSION, TE_EXPLOSION2, TE_TAREXPLOSION) launches its
// chunks at the frame's end (vr_explosiondebris.cpp), where the client draws it.
void noteExplosion(int at)
{
    const byte type = sv.datagram.data[at + 1];
    if(type != TE_EXPLOSION && type != TE_EXPLOSION2 && type != TE_TAREXPLOSION)
    {
        return;
    }
    const byte* p = sv.datagram.data + at + 2;
    const auto i16 = [](const byte* q) { return static_cast<int16_t>(q[0] | (q[1] << 8)); };
    const auto i32 = [](const byte* q) { return static_cast<int32_t>(static_cast<uint32_t>(q[0]) | (static_cast<uint32_t>(q[1]) << 8) | (static_cast<uint32_t>(q[2]) << 16) | (static_cast<uint32_t>(q[3]) << 24)); };
    glm::vec3 org{0.f};
    for(int k = 0; k < 3; k++)
    {
        // As MSG_ReadCoord reads it (common.c), in the server's protocol.
        if(sv.protocolflags & PRFL_FLOATCOORD)
        {
            const int32_t bits = i32(p);
            float f;
            memcpy(&f, &bits, sizeof(f));
            org[k] = f;
            p += 4;
        }
        else if(sv.protocolflags & PRFL_INT32COORD)
        {
            org[k] = static_cast<float>(i32(p)) * (1.f / 16.f);
            p += 4;
        }
        else if(sv.protocolflags & PRFL_24BITCOORD)
        {
            org[k] = static_cast<float>(i16(p)) + static_cast<float>(p[2]) * (1.f / 255.f);
            p += 3;
        }
        else
        {
            org[k] = static_cast<float>(i16(p)) * (1.f / 8.f);
            p += 2;
        }
    }
    qvr::explosiondebris::noteBroadcast(org);
}

void broadcastMark(int at)
{
    BroadcastRoom& b = broadcastRoom;
    b.msgAt = at;
    if(at <= 0 || (b.numMarks > 0 && b.marks[b.numMarks - 1] >= at))
    {
        return;
    }
    if(b.numMarks == BroadcastRoom::maxMarks)
    {
        for(int i = 0; i < BroadcastRoom::maxMarks / 2; i++)
        {
            b.marks[i] = b.marks[2 * i + 1];
        }
        b.numMarks = BroadcastRoom::maxMarks / 2;
    }
    b.marks[b.numMarks++] = at;
}

// ----------------------------------------------------------------------------
// The looping sounds the server started (a sample with a cue point: the chainsaw's idle, run and cut, a flamethrower's,
// a fire's crackle): kept per entity and channel, so that what ends one is never lost and none plays on after its
// entity (his note vrtesthall_2026-11-16_00-47-47: a knight's chainsaw idling for ever after it died). A sound that
// replaces a loop (misc/null.wav, the stall) goes with the reliable messages (the broadcast's sounds are dropped when it
// is full: a big fight's); an entity freed with a loop on it has the loop stopped (VR_SoundLoopsFreed), reliably too.
struct SoundLoops
{
    za::Vector<za::U16> mask;   // per entity number: its channels playing a loop (a bit each, 0 .. SND_MAX_CHANNEL)
    za::Vector<int> stopFrame;  // per entity number: host_framecount of its last reliable stop (a loop again then: reliable)
    za::Vector<signed char> isLoop; // per sound number: -1 not read yet, 0, 1 (this map's)
    const qmodel_t* world{nullptr}; // the map they are of (a new one: all forgotten)
    char map[64]{};
    qvr::testkit::Tally tally;       // vr_snd_loops check's
};
SoundLoops soundLoops;

void soundLoopsFor()
{
    SoundLoops& s = soundLoops;
    if(s.world != sv.worldmodel || strcmp(s.map, sv.name) != 0)
    {
        s.world = sv.worldmodel;
        q_strlcpy(s.map, sv.name, sizeof(s.map));
        s.mask.clear();
        s.stopFrame.clear();
        s.isLoop.clear();
    }
    const auto edicts = static_cast<za::SizeT>(za::max(qcvm->max_edicts, 1));
    if(s.mask.size() < edicts)
    {
        s.mask.resize(edicts, 0);
        s.stopFrame.resize(edicts, -1);
    }
    if(s.isLoop.size() < static_cast<za::SizeT>(MAX_SOUNDS))
    {
        s.isLoop.resize(static_cast<za::SizeT>(MAX_SOUNDS), -1);
    }
}

// Whether precached sound `num` loops (its file has a cue point: GetWavinfo's loopstart), read once a map.
bool soundLoops_(int num)
{
    signed char& known = soundLoops.isLoop[static_cast<za::SizeT>(num)];
    if(known < 0)
    {
        known = 0;
        unsigned int pathId = 0;
        if(byte* data = COM_LoadMallocFile(va("sound/%s", sv.sound_precache[num]), &pathId))
        {
            known = GetWavinfo(sv.sound_precache[num], data, com_filesize).loopstart >= 0 ? 1 : 0;
            free(data);
        }
    }
    return known > 0;
}

// vr_snd_loops: the loops playing, by the server's account. `vr_snd_loops check <label> [most] [least]`: PASS when
// `least` (0) to `most` (0) play (tallied); `vr_snd_loops end <name>`: "<name>: N passed, M failed" (the tests' verdict).
void soundLoops_f()
{
    if(!sv.active)
    {
        Con_Printf("vr_snd_loops: no server\n");
        return;
    }
    qvr::progs::ServerVm serverVm;
    soundLoopsFor();
    SoundLoops& s = soundLoops;
    const char* const what = Cmd_Argc() > 1 ? Cmd_Argv(1) : "";
    if(!strcmp(what, "end"))
    {
        s.tally.verdict(Cmd_Argc() > 2 ? Cmd_Argv(2) : "sndloops");
        s.tally.reset();
        return;
    }
    int n = 0;
    za::String list;
    for(za::SizeT i = 1; i < s.mask.size() && static_cast<int>(i) < qcvm->num_edicts; i++)
    {
        for(int c = 0; c <= SND_MAX_CHANNEL; c++)
        {
            if(s.mask[i] & (1u << c))
            {
                n++;
                edict_t* const e = EDICT_NUM(static_cast<int>(i));
                list += va(" %d:%s/%d", static_cast<int>(i), e->free ? "(freed)" : PR_GetString(e->v.classname), c);
            }
        }
    }
    if(!strcmp(what, "check"))
    {
        const int most = Cmd_Argc() > 3 ? Q_atoi(Cmd_Argv(3)) : 0;
        const int least = Cmd_Argc() > 4 ? Q_atoi(Cmd_Argv(4)) : 0;
        const bool ok = s.tally.check(n <= most && n >= least);
        Con_Printf("sndloops %s: %s: %d loops playing (%d .. %d)%s\n", Cmd_Argc() > 2 ? Cmd_Argv(2) : "", ok ? "PASS" : "FAIL", n,
            least, most, list.cStr());
        return;
    }
    Con_Printf("vr_snd_loops: %d loops playing%s\n", n, list.cStr());
}

} // namespace

// SV_StartSoundPitch, before its message: sound `num` started on entity `ent`'s `channel`. True when it must go with the
// reliable messages: it ends a loop there (it is no loop itself), or it is a loop again after such an end this frame
// (the reliable messages are read after the broadcast: it would be ended by the end before it).
extern "C" int VR_SoundLoopNote(int ent, int channel, int num)
{
    if(ent <= 0 || channel < 0 || channel > SND_MAX_CHANNEL || num <= 0 || num >= MAX_SOUNDS)
    {
        return 0;
    }
    soundLoopsFor();
    SoundLoops& s = soundLoops;
    if(static_cast<za::SizeT>(ent) >= s.mask.size())
    {
        return 0;
    }
    const auto bit = static_cast<za::U16>(1u << channel);
    za::U16& m = s.mask[static_cast<za::SizeT>(ent)];
    if(soundLoops_(num))
    {
        m = static_cast<za::U16>(m | bit);
        return s.stopFrame[static_cast<za::SizeT>(ent)] == host_framecount ? 1 : 0;
    }
    if(!(m & bit))
    {
        return 0;
    }
    m = static_cast<za::U16>(m & ~bit);
    s.stopFrame[static_cast<za::SizeT>(ent)] = host_framecount;
    return 1;
}

// ED_Free: the loops still playing on it stopped (misc/null.wav on each channel, reliably), whatever freed it.
extern "C" void VR_SoundLoopsFreed(edict_t* ed)
{
    if(!sv.active || qcvm != &sv.qcvm)
    {
        return;
    }
    soundLoopsFor();
    const int ent = NUM_FOR_EDICT(ed);
    SoundLoops& s = soundLoops;
    if(ent <= 0 || static_cast<za::SizeT>(ent) >= s.mask.size() || !s.mask[static_cast<za::SizeT>(ent)])
    {
        return;
    }
    int null = 0;
    for(int i = 1; i < MAX_SOUNDS && sv.sound_precache[i]; i++)
    {
        if(!strcmp(sv.sound_precache[i], "misc/null.wav"))
        {
            null = i;
            break;
        }
    }
    vec3_t origin;
    for(int i = 0; i < 3; i++)
    {
        origin[i] = ed->v.origin[i] + 0.5f * (ed->v.mins[i] + ed->v.maxs[i]);
    }
    for(int c = 0; c <= SND_MAX_CHANNEL; c++)
    {
        if(!(s.mask[static_cast<za::SizeT>(ent)] & (1u << c)))
        {
            continue;
        }
        if(null)
        {
            SV_WriteSound(&sv.reliable_datagram, ent, c, null, 255, DEFAULT_SOUND_PACKET_ATTENUATION, origin, 1.f);
        }
        else if(c < 8 && ent < 8192)
        {
            MSG_WriteByte(&sv.reliable_datagram, svc_stopsound);
            MSG_WriteShort(&sv.reliable_datagram, (ent << 3) | c);
        }
        VR_SoundLog(ent, c, "(its loop stopped: freed)", 255, 0);
    }
    s.mask[static_cast<za::SizeT>(ent)] = 0;
}

// SV_ClearDatagram: the frame's broadcast starts empty (vr_debug_net: the last frame's numbers).
// Debug > Tests > Log Sounds (vr_debug_sound_log): each sound the server starts printed (its entity, channel, sample,
// volume; dropped ones: why), the tests' record of what a weapon's shot, reload or click played. 2: only monsters' and
// their weapons' (not the players').
extern "C" void VR_SoundLog(int ent, int channel, const char* sample, int volume, int dropped)
{
    if(!qvr::vr_debug_sound_log.value)
    {
        return;
    }
    edict_t* const e = EDICT_NUM(ent);
    const char* const klass = e->free ? "" : PR_GetString(e->v.classname);
    if(qvr::vr_debug_sound_log.value >= 2.f && (static_cast<int>(e->v.flags) & FL_CLIENT))
    {
        return;
    }
    static constexpr const char* whyDropped[4] = {"", " DROPPED: not precached", " DROPPED: datagram full", " DROPPED: muted"};
    Con_Printf("sndlog: t %.3f ent %d %s chan %d %s vol %.2f%s\n", qcvm->time, ent, klass, channel, sample,
        volume / 255.0, whyDropped[dropped < 0 || dropped > 3 ? 0 : dropped]);
}

extern "C" void VR_BroadcastClear()
{
    BroadcastRoom& b = broadcastRoom;
    if(qvr::vr_debug_net.value && (b.qcDrops || b.unsent))
    {
        Con_Printf("vr_debug_net: broadcast full (%d bytes, %d boundaries): QuakeC's writes dropped %d times, %d bytes; "
                   "%d not sent (%d before it)\n",
            sv.datagram.cursize, b.numMarks, b.qcDrops, b.qcDropped, b.unsent, b.entities);
    }
    if(qvr::vr_debug_net.value >= 2.f)
    {
        b.peakSize = q_max(b.peakSize, sv.datagram.cursize);
        b.peakEntities = q_max(b.peakEntities, b.entities);
        if(realtime - b.since >= 1.0)
        {
            Con_Printf("vr_debug_net: the second's most: broadcast %d, reliable %d, a client's datagram before it %d (of %d)\n",
                b.peakSize, b.peakReliable, b.peakEntities, MAX_DATAGRAM);
            b.since = realtime;
            b.peakSize = b.peakReliable = b.peakEntities = 0;
        }
    }

    b.numMarks = 0;
    b.qcEnd = -1;
    b.dropAt = -1;
    b.msgAt = 0;
    b.qcDropped = b.qcDrops = b.unsent = b.entities = 0;
    SZ_Clear(&sv.datagram);

    ScopedRoom& r = scopedRoom;
    if(qvr::vr_debug_net.value && r.dropped)
    {
        Con_Printf("vr_debug_net: %d armed humanoids' poses had no room (%d B this frame)\n", r.dropped, r.room.cursize);
    }
    r.room.data = r.roomBuf;
    r.room.maxsize = sizeof(r.roomBuf);
    r.room.allowoverflow = true; // (never: each message has its room before it begins)
    SZ_Clear(&r.room);
    r.numMsgs = 0;
    r.openEnt = -1;
    r.dropped = 0;

    // The messages queued since the last frame (server::queueBroadcast) open this one, each whole (an explosion's
    // launches its chunks, as QuakeC's does).
    int from = 0;
    for(int i = 0; i < b.numQueued; i++)
    {
        const int at = sv.datagram.cursize;
        const int len = b.queuedEnds[i] - from;
        SZ_Write(&sv.datagram, b.queued + from, len);
        if(tempEntityLength(at) == len)
        {
            noteExplosion(at);
        }
        broadcastMark(sv.datagram.cursize);
        from = b.queuedEnds[i];
    }
    b.numQueued = 0;
}

sizebuf_t* qvr::server::scopedBegin(int ent, bool always)
{
    ScopedRoom& r = scopedRoom;
    if((!always && qvr::vr_net_cullposes.value == 0.f) || !sv.active || !r.room.data)
    {
        return nullptr;
    }
    if(r.numMsgs >= ScopedRoom::maxMsgs || r.room.cursize + ScopedRoom::msgRoom > r.room.maxsize)
    {
        r.sink.data = r.sinkBuf;
        r.sink.maxsize = sizeof(r.sinkBuf);
        r.sink.allowoverflow = true;
        SZ_Clear(&r.sink);
        r.dropped++;
        r.openEnt = -1;
        return &r.sink;
    }
    r.openEnt = ent;
    return &r.room;
}

void qvr::server::scopedEnd(int kind)
{
    ScopedRoom& r = scopedRoom;
    if(r.openEnt < 0)
    {
        return;
    }
    r.ents[r.numMsgs] = r.openEnt;
    r.ends[r.numMsgs] = r.room.cursize;
    r.kinds[r.numMsgs] = kind;
    r.numMsgs++;
    r.openEnt = -1;
}

// SV_WriteEntitiesToClient: a client's walk of the entities begins; each one in its sight (its PVS) is noted.
extern "C" void VR_NoteEntitiesBegin()
{
    ScopedRoom& r = scopedRoom;
    r.stamp++;
    if(static_cast<int>(r.seen.size()) < qcvm->max_edicts)
    {
        r.seen.resize(static_cast<za::SizeT>(qcvm->max_edicts), 0);
    }
}

extern "C" void VR_NoteEntityInSight(int ent)
{
    ScopedRoom& r = scopedRoom;
    if(ent >= 0 && ent < static_cast<int>(r.seen.size()))
    {
        r.seen[static_cast<za::SizeT>(ent)] = r.stamp;
    }
}

namespace
{

[[nodiscard]] bool scopedInSight(const ScopedRoom& r, int e)
{
    return e >= 0 && e < static_cast<int>(r.seen.size()) && r.seen[static_cast<za::SizeT>(e)] == r.stamp;
}

} // namespace

// SV_SendClientDatagram, after the broadcast: this frame's messages about entities, those in this client's sight (all of
// them to the loopback client, `local`), each whole while it fits. An avatar unchanged since last sent goes to none (its
// keyframe's wait), and to a snapshot client (`snap`) none: they went in its snapshot (VR_SnapEnd).
extern "C" void VR_WriteScoped(sizebuf_t* msg, int local, int snap)
{
    const ScopedRoom& r = scopedRoom;
    const bool cull = !local && qvr::vr_net_cullposes.value != 0.f;
    int from = 0;
    for(int i = 0; i < r.numMsgs; i++)
    {
        const int len = r.ends[i] - from;
        const int at = from;
        from = r.ends[i];
        const int e = r.ents[i];
        // (a snapshot client's avatars and poses went in its snapshot: VR_SnapEnd)
        if(r.kinds[i] == qvr::server::scopedAvatarUnchanged || (snap && r.kinds[i] == qvr::server::scopedAvatar) ||
            (snap && r.kinds[i] == qvr::server::scopedOther && cull && qvr::vr_net_snapposes.value != 0.f))
        {
            continue;
        }
        if(cull && !scopedInSight(r, e))
        {
            continue;
        }
        if(msg->cursize + len > msg->maxsize - 1)
        {
            continue;
        }
        SZ_Write(msg, r.roomBuf + at, len);
    }
    if(snap)
    {
        qvr::netsnap::serverCharge(msg->cursize); // (the datagram whole: off his rate, B12)
    }
}

// A remote client's snapshot (vr_netsnap.cpp; MP_PLAN.md, B7b).
extern "C" int VR_SnapBegin(client_t* client)
{
    return qvr::netsnap::serverBegin(client) ? 1 : 0;
}

extern "C" void VR_SnapTake(sizebuf_t* msg, int at, int key)
{
    qvr::netsnap::serverTake(msg, at, key);
}

// ... the avatars in its sight (unchanged or not: the snapshot codes each against the one it acknowledged), then written.
extern "C" void VR_SnapEnd(sizebuf_t* msg)
{
    const ScopedRoom& r = scopedRoom;
    const bool cull = qvr::vr_net_cullposes.value != 0.f;
    int from = 0;
    for(int i = 0; i < r.numMsgs; i++)
    {
        const int at = from;
        from = r.ends[i];
        if((r.kinds[i] == qvr::server::scopedAvatar || r.kinds[i] == qvr::server::scopedAvatarUnchanged) &&
            (!cull || scopedInSight(r, r.ents[i])))
        {
            qvr::netsnap::serverRecord(qvr::netsnap::avatarKey + r.ents[i], r.roomBuf + at, r.ends[i] - at);
        }
    }
    // The armed humanoids' poses in its sight (B12; vr_net_cullposes 0: every pose to every client in the broadcast, as
    // before): a record each, an entity's poses of this frame together (one a frame, as a rule).
    if(cull && qvr::vr_net_snapposes.value != 0.f)
    {
        ScopedRoom& w = scopedRoom;
        from = 0;
        for(int i = 0; i < r.numMsgs; i++)
        {
            const int at = from;
            from = r.ends[i];
            const int e = r.ents[i];
            if(r.kinds[i] != qvr::server::scopedOther || !scopedInSight(r, e))
            {
                continue;
            }
            bool earlier = false; // (merged into the record of its entity's first already)
            for(int j = 0; j < i && !earlier; j++)
            {
                earlier = r.kinds[j] == qvr::server::scopedOther && r.ents[j] == e;
            }
            if(earlier)
            {
                continue;
            }
            int len = r.ends[i] - at;
            bool more = false;
            for(int j = i + 1; j < r.numMsgs && !more; j++)
            {
                more = r.kinds[j] == qvr::server::scopedOther && r.ents[j] == e;
            }
            if(!more)
            {
                qvr::netsnap::serverRecord(qvr::netsnap::poseKey + e, r.roomBuf + at, len);
                continue;
            }
            len = 0;
            int jfrom = 0;
            for(int j = 0; j < r.numMsgs; j++)
            {
                const int jat = jfrom;
                jfrom = r.ends[j];
                const int jlen = r.ends[j] - jat;
                if(j >= i && r.kinds[j] == qvr::server::scopedOther && r.ents[j] == e &&
                    len + jlen <= static_cast<int>(sizeof(w.poseJoin)))
                {
                    memcpy(w.poseJoin + len, r.roomBuf + jat, static_cast<size_t>(jlen));
                    len += jlen;
                }
            }
            qvr::netsnap::serverRecord(qvr::netsnap::poseKey + e, w.poseJoin, len);
        }
    }
    // (ragsync) The ragdolls in its sight: a record each, their parts as the server last stepped them (vr_netragdoll.cpp).
    if(qvr::vr_net_ragdolls.value != 0.f)
    {
        for(int e = 1; e < qcvm->num_edicts; e++)
        {
            if(scopedInSight(r, e))
            {
                qvr::netrag::serverRecord(e);
            }
        }
    }
    qvr::netsnap::serverEnd(msg);
}

void qvr::server::queueBroadcast(const byte* data, int len)
{
    BroadcastRoom& b = broadcastRoom;
    const int used = b.numQueued > 0 ? b.queuedEnds[b.numQueued - 1] : 0;
    if(len <= 0 || b.numQueued >= BroadcastRoom::maxQueued || used + len > static_cast<int>(sizeof(b.queued)))
    {
        return; // (unreliable, as the datagram is: none when full)
    }
    memcpy(b.queued + used, data, static_cast<size_t>(len));
    b.queuedEnds[b.numQueued++] = used + len;
}

void qvr::server::commandBroadcastBegin()
{
    BroadcastRoom& b = broadcastRoom;
    b.numMarks = 0;
    b.qcEnd = -1;
    b.dropAt = -1;
    b.msgAt = 0;
    SZ_Clear(&sv.datagram);
}

void qvr::server::commandBroadcastEnd()
{
    const BroadcastRoom& b = broadcastRoom;
    const int used = b.numQueued > 0 ? b.queuedEnds[b.numQueued - 1] : 0;
    if(sv.datagram.cursize + used <= static_cast<int>(sizeof(b.queued)))
    {
        queueBroadcast(sv.datagram.data, sv.datagram.cursize); // (all of it in one: its messages whole, one after another)
        return;
    }
    int from = 0;
    for(int i = 0; i <= b.numMarks && from < sv.datagram.cursize; i++)
    {
        const int end = i < b.numMarks ? q_min(b.marks[i], sv.datagram.cursize) : sv.datagram.cursize;
        if(end > from)
        {
            queueBroadcast(sv.datagram.data + from, end - from);
            from = end;
        }
    }
}

// PR_ExecuteProgram, a run of the server's QuakeC from the engine (not nested in a builtin): QuakeC is between messages.
extern "C" void VR_BroadcastQCRun()
{
    broadcastRoom.dropAt = -1;
    broadcastMark(sv.datagram.cursize);
}

// WriteDest's MSG_BROADCAST (pr_cmds.c, vr_builtins.cpp): where a QuakeC write of at most `len` bytes goes, sv.datagram
// or the sink. VR_BroadcastWritten after it.
extern "C" sizebuf_t* VR_BroadcastDest(int len)
{
    BroadcastRoom& b = broadcastRoom;
    sizebuf_t& dg = sv.datagram;
    if(b.dropAt >= 0)
    {
        if(dg.cursize == b.dropAt)
        {
            b.qcDropped += len;
            b.sink.data = b.sinkBuf;
            b.sink.maxsize = sizeof(b.sinkBuf);
            SZ_Clear(&b.sink);
            return &b.sink;
        }
        b.dropAt = -1; // the engine wrote since: QuakeC is between messages
    }
    if(dg.cursize != b.qcEnd)
    {
        broadcastMark(dg.cursize); // the engine wrote since QuakeC's last write (or this run's first)
    }
    if(dg.cursize + len > dg.maxsize)
    {
        const int back = b.numMarks > 0 ? b.marks[b.numMarks - 1] : 0;
        b.qcDropped += dg.cursize - back + len;
        b.qcDrops++;
        dg.cursize = back;
        b.dropAt = back;
        b.qcEnd = -1;
        b.msgAt = -1;
        b.sink.data = b.sinkBuf;
        b.sink.maxsize = sizeof(b.sinkBuf);
        SZ_Clear(&b.sink);
        return &b.sink;
    }
    return &dg;
}

extern "C" void VR_BroadcastWritten(sizebuf_t* dest)
{
    BroadcastRoom& b = broadcastRoom;
    if(dest != &sv.datagram)
    {
        return;
    }
    b.qcEnd = sv.datagram.cursize;
    if(b.msgAt >= 0)
    {
        const int len = tempEntityLength(b.msgAt);
        if(len > 0 && b.qcEnd - b.msgAt == len)
        {
            noteExplosion(b.msgAt); // (an explosion's chunks: vr_explosiondebris.cpp)
            broadcastMark(b.qcEnd); // a whole temp entity: the next message begins here
        }
        else if((len > 0 && b.qcEnd - b.msgAt > len) || (len == 0 && b.qcEnd - b.msgAt >= 2))
        {
            b.msgAt = -1; // not a temp entity known (or not as known): the next boundary is the others'
        }
    }
}

extern "C" void VR_BroadcastMessageEnd()
{
    broadcastMark(sv.datagram.cursize);
}

// SV_SendClientDatagram: how much of sv.datagram goes into a client's datagram with `room` bytes left (`before`: its
// bytes so far): all of it, or the longest part ending on a boundary.
extern "C" int VR_BroadcastSendable(int before, int room)
{
    BroadcastRoom& b = broadcastRoom;
    const int size = sv.datagram.cursize;
    b.entities = q_max(b.entities, before);
    if(size <= room)
    {
        return size;
    }
    // the last mark <= room
    int lo = 0;
    int hi = b.numMarks; // marks[lo..hi) unsearched; the answer is marks[lo - 1]
    while(lo < hi)
    {
        const int mid = (lo + hi) / 2;
        if(b.marks[mid] <= room)
        {
            lo = mid + 1;
        }
        else
        {
            hi = mid;
        }
    }
    const int sendable = lo > 0 ? b.marks[lo - 1] : 0;
    b.unsent += size - sendable;
    return sendable;
}

// ----------------------------------------------------------------------------
// (B13b, MP_PLAN) The broadcast culled by sight: NetQuake sent every client the whole unreliable broadcast (sv.datagram:
// particles, temp entities, sounds, Quake VR's effects) wherever it was; with 16 players most of each remote client's
// broadcast was fights he could neither see nor hear. Each message whose place is known goes to a remote client only when
// he could see it (its point in his fat PVS, SV_WriteEntitiesToClient's) or, a sound, hear it (nearer than Quake's
// falloff reaches at half his vr_snd_falloff: 2000 units at ATTN_NORM, 667 at ATTN_STATIC; ATTN_NONE everywhere). A
// message not known (and what follows it to the next boundary) goes to everyone, as before. The loopback client (single
// player, a listen host) gets it all: vr_net_cullbroadcast 0 sends everyone all of it, as before.
namespace
{

struct BroadcastSight
{
    const edict_t* clent{nullptr}; // the client whose entities were last written this frame (SV_WriteEntitiesToClient)
    double time{-1.0};
    glm::vec3 eye{0.f};
    za::Vector<byte> pvs;
    // vr_net_broadcast_report: the bytes of known messages sent and culled, and those not known (sent), all clients
    za::U64 sent{0}, culled{0}, unknown{0};
};
BroadcastSight broadcastSight{};

constexpr float soundReachScale = 0.5f; // a sound culled only past where it fades out at vr_snd_falloff 0.5
constexpr float soundMargin = 64.f;     // units more

[[nodiscard]] int coordBytes()
{
    byte bytes[8];
    sizebuf_t probe{};
    probe.data = bytes;
    probe.maxsize = sizeof(bytes);
    MSG_WriteCoord(&probe, 0.f, sv.protocolflags);
    return probe.cursize;
}

// The coordinates at `p` in sv.datagram, as MSG_ReadCoord reads them in the server's protocol.
[[nodiscard]] glm::vec3 coordsAt(const byte* p)
{
    const auto i16 = [](const byte* q) { return static_cast<int16_t>(q[0] | (q[1] << 8)); };
    const auto i32 = [](const byte* q) { return static_cast<int32_t>(static_cast<uint32_t>(q[0]) | (static_cast<uint32_t>(q[1]) << 8) | (static_cast<uint32_t>(q[2]) << 16) | (static_cast<uint32_t>(q[3]) << 24)); };
    glm::vec3 org{0.f};
    for(int k = 0; k < 3; k++)
    {
        if(sv.protocolflags & PRFL_FLOATCOORD)
        {
            const int32_t bits = i32(p);
            memcpy(&org[k], &bits, sizeof(float));
            p += 4;
        }
        else if(sv.protocolflags & PRFL_INT32COORD)
        {
            org[k] = static_cast<float>(i32(p)) * (1.f / 16.f);
            p += 4;
        }
        else if(sv.protocolflags & PRFL_24BITCOORD)
        {
            org[k] = static_cast<float>(i16(p)) + static_cast<float>(p[2]) * (1.f / 255.f);
            p += 3;
        }
        else
        {
            org[k] = static_cast<float>(i16(p)) * (1.f / 8.f);
            p += 2;
        }
    }
    return org;
}

[[nodiscard]] bool pointSeen(const glm::vec3& p)
{
    const BroadcastSight& s = broadcastSight;
    vec3_t at{p.x, p.y, p.z};
    const mleaf_t* leaf = Mod_PointInLeaf(at, sv.worldmodel);
    const int n = leaf ? static_cast<int>(leaf - sv.worldmodel->leafs) - 1 : -1;
    if(n < 0 || (n >> 3) >= static_cast<int>(s.pvs.size()))
    {
        return true; // (in the solid, or not known: sent)
    }
    return (s.pvs[static_cast<za::SizeT>(n >> 3)] & (1 << (n & 7))) != 0;
}

[[nodiscard]] bool soundHeard(const glm::vec3& p, float attenuation)
{
    if(attenuation <= 0.f)
    {
        return true;
    }
    return glm::length(p - broadcastSight.eye) < 1000.f / (attenuation * soundReachScale) + soundMargin;
}

// The broadcast's message at `at` (before `end`): its length and whether this client sees or hears it. False: not one
// known (or not whole before `end`).
[[nodiscard]] bool knownMessage(int at, int end, int& len, bool& seen)
{
    const byte* d = sv.datagram.data;
    const int c = coordBytes();
    len = 0;
    seen = true;
    switch(d[at])
    {
        case svc_particle: // [coord3][char3][byte count][byte color]
            len = 1 + 3 * c + 5;
            if(at + len > end) return false;
            seen = pointSeen(coordsAt(d + at + 1));
            return true;
        case svc_temp_entity:
        {
            len = tempEntityLength(at);
            if(len <= 0 || at + len > end) return false;
            const byte type = d[at + 1];
            if(type == TE_LIGHTNING1 || type == TE_LIGHTNING2 || type == TE_LIGHTNING3 || type == TE_BEAM)
            {
                const int from = at + 4 + ((sv.protocolflags & PRFL_QUAKEVR_PROGS) ? 1 : 0);
                seen = pointSeen(coordsAt(d + from)) || pointSeen(coordsAt(d + from + 3 * c));
            }
            else
            {
                seen = pointSeen(coordsAt(d + at + 2));
            }
            return true;
        }
        case svc_sound: // (SV_WriteSound's layout)
        {
            int p = at + 1;
            if(p >= end) return false;
            const int mask = d[p++];
            float attenuation = DEFAULT_SOUND_PACKET_ATTENUATION;
            p += (mask & SND_LOUD) ? 2 : (mask & SND_VOLUME) ? 1 : 0;
            if(mask & SND_ATTENUATION)
            {
                if(p >= end) return false;
                attenuation = static_cast<float>(d[p++]) / 64.f;
            }
            p += (mask & SND_PITCH) ? 2 : 0;
            p += (mask & SND_LARGEENTITY) ? 3 : 2;
            p += (mask & SND_LARGESOUND) ? 2 : 1;
            len = p + 3 * c - at;
            if(at + len > end) return false;
            seen = soundHeard(coordsAt(d + p), attenuation);
            return true;
        }
        case svc_quakevr:
            if(at + 2 > end) return false;
            switch(d[at + 1])
            {
                case QVR_SVC_PARTICLE2: // [coord3 org][char3 dir][byte preset][short count]
                    len = 2 + 3 * c + 3 + 1 + 2;
                    if(at + len > end) return false;
                    seen = pointSeen(coordsAt(d + at + 2));
                    return true;
                case QVR_SVC_TRACER: // [short entity][byte hand][coord3 from][coord3 to]
                    len = 2 + 2 + 1 + 6 * c;
                    if(at + len > end) return false;
                    seen = pointSeen(coordsAt(d + at + 5)) || pointSeen(coordsAt(d + at + 5 + 3 * c));
                    return true;
                case QVR_SVC_GORESOUND: // [byte kind][short entity][coord3 at][byte strength]: a sound at ATTN_NORM
                    len = 2 + 1 + 2 + 3 * c + 1;
                    if(at + len > end) return false;
                    seen = soundHeard(coordsAt(d + at + 5), 1.f);
                    return true;
                default: return false;
            }
        default: return false;
    }
}

void broadcastReport_f()
{
    const BroadcastSight& s = broadcastSight;
    Con_Printf("mp_broadcast: sent=%llu culled=%llu unknown=%llu cull=%g held=%llu held_dropped=%llu\n",
        static_cast<unsigned long long>(s.sent), static_cast<unsigned long long>(s.culled),
        static_cast<unsigned long long>(s.unknown), vr_net_cullbroadcast.value,
        static_cast<unsigned long long>(heldCounts.held), static_cast<unsigned long long>(heldCounts.dropped));
    if(Cmd_Argc() > 1 && !strcmp(Cmd_Argv(1), "reset"))
    {
        broadcastSight.sent = broadcastSight.culled = broadcastSight.unknown = 0;
        heldCounts = HeldCounts{};
    }
}

} // namespace

// SV_WriteEntitiesToClient: the client's eye and fat PVS (its teleporters' too), for his broadcast (VR_WriteBroadcast).
extern "C" void VR_NoteClientSight(const edict_t* clent, const byte* pvs, const float* eye)
{
    BroadcastSight& s = broadcastSight;
    s.clent = clent;
    s.time = qcvm->time;
    s.eye = {eye[0], eye[1], eye[2]};
    const int bytes = (sv.worldmodel->numleafs + 7) >> 3;
    s.pvs.resize(static_cast<za::SizeT>(bytes));
    memcpy(s.pvs.data(), pvs, static_cast<size_t>(bytes));
}

// SV_SendClientDatagram: the broadcast into this client's datagram (`local`: the loopback client's, all of it).
extern "C" void VR_WriteBroadcast(sizebuf_t* msg, int local, const edict_t* clent)
{
    BroadcastSight& s = broadcastSight;
    if(local || vr_net_cullbroadcast.value == 0.f || s.clent != clent || s.time != qcvm->time || !sv.worldmodel)
    {
        const int sendable = VR_BroadcastSendable(msg->cursize, msg->maxsize - msg->cursize - 1);
        if(sendable > 0)
        {
            SZ_Write(msg, sv.datagram.data, sendable);
        }
        return;
    }
    BroadcastRoom& b = broadcastRoom;
    b.entities = q_max(b.entities, msg->cursize);
    int from = 0;
    for(int i = 0; i <= b.numMarks; i++)
    {
        const int end = i < b.numMarks ? b.marks[i] : sv.datagram.cursize;
        // its messages one by one while they are known; from one that is not, the rest to the boundary whole
        for(int at = from; at < end;)
        {
            int len = 0;
            bool seen = true;
            const bool known = knownMessage(at, end, len, seen);
            if(!known)
            {
                len = end - at;
            }
            if(!seen)
            {
                s.culled += static_cast<za::U64>(len);
            }
            else if(msg->cursize + len > msg->maxsize - 1)
            {
                b.unsent += len; // (no room: as the whole broadcast's tail had none)
            }
            else
            {
                SZ_Write(msg, sv.datagram.data + at, len);
                (known ? s.sent : s.unknown) += static_cast<za::U64>(len);
            }
            at += len;
        }
        from = q_max(from, end);
    }
}

// (B13c) vr_net_sendhz: a remote snapshot client's frames without a datagram keep his share of each frame's broadcast
// (culled by his sight as it would have been) for his next datagram, in order before its own frame's (heldBroadcast).
extern "C" int VR_SendsDatagramNow(client_t* client)
{
    return qvr::netsnap::serverSendsNow(client) ? 1 : 0;
}

extern "C" void VR_HoldBroadcast(client_t* client)
{
    const int slot = static_cast<int>(client - svs.clients);
    if(slot < 0 || slot >= MAX_SCOREBOARD || !client->edict)
    {
        return;
    }
    HeldBroadcast& h = heldBroadcast[slot];
    sizebuf_t buf{};
    buf.data = h.data;
    buf.maxsize = static_cast<int>(sizeof(h.data));
    buf.cursize = h.size;
    const int entities = broadcastRoom.entities; // (its stat: a datagram's entities, not this buffer's)
    VR_WriteBroadcast(&buf, 0, client->edict);
    broadcastRoom.entities = entities;
    heldCounts.held += static_cast<za::U64>(buf.cursize - h.size);
    h.size = buf.cursize;
}

extern "C" void VR_WriteHeldBroadcast(sizebuf_t* msg, client_t* client)
{
    const int slot = static_cast<int>(client - svs.clients);
    if(slot < 0 || slot >= MAX_SCOREBOARD || heldBroadcast[slot].size == 0)
    {
        return;
    }
    HeldBroadcast& h = heldBroadcast[slot];
    if(msg->cursize + h.size <= msg->maxsize - 1)
    {
        SZ_Write(msg, h.data, h.size);
    }
    else
    {
        heldCounts.dropped += static_cast<za::U64>(h.size); // (no room after the entities: as a broadcast's tail)
    }
    h.size = 0;
}

extern "C" int VR_HeldBroadcastBytes(int slot)
{
    return slot >= 0 && slot < MAX_SCOREBOARD ? heldBroadcast[slot].size : 0;
}

extern "C" void VR_ClearHeldBroadcast(int slot)
{
    if(slot >= 0 && slot < MAX_SCOREBOARD)
    {
        heldBroadcast[slot].size = 0;
    }
}

// SV_SendClientMessages, before sv.reliable_datagram is copied to the clients (vr_debug_net 2).
// ----------------------------------------------------------------------------
// vr_net_stats: what each client's datagrams carry of the entities (the debris' cost in multiplayer: MP_DESIGN.md).

namespace
{
struct NetStat
{
    int sent{0}, insight{0}, bytes{0}, maxsize{0};
    int peakBytes{0}, peakSent{0}, frames{0}, overflowFrames{0};
    double sumBytes{0.0};
};
NetStat netStats[MAX_SCOREBOARD];

// vr_net_reliable_test <entity> <bytes>: a reliable message of about that many bytes to that player's client (nops, then
// "mp_reliable: <bytes> bytes" printed there when it arrives): the reliable window's resends measured (mp_test.py
// retransmit, B12b).
void netReliableTest_f()
{
    if(!sv.active || Cmd_Argc() < 3)
    {
        Con_Printf("vr_net_reliable_test <player entity> <bytes>: a reliable message of that size to his client\n");
        return;
    }
    const int e = atoi(Cmd_Argv(1));
    client_t* const c = e >= 1 && e <= svs.maxclients ? &svs.clients[e - 1] : nullptr;
    if(!c || !c->active)
    {
        Con_Printf("vr_net_reliable_test: no player %d\n", e);
        return;
    }
    const char* const mark = va("mp_reliable: %d bytes\n", atoi(Cmd_Argv(2)));
    const int room = c->message.maxsize - c->message.cursize - static_cast<int>(strlen(mark)) - 2;
    const int n = za::max(0, za::min(atoi(Cmd_Argv(2)), room));
    for(int i = 0; i < n; i++)
    {
        MSG_WriteByte(&c->message, svc_nop);
    }
    MSG_WriteByte(&c->message, svc_print);
    MSG_WriteString(&c->message, mark);
    Con_Printf("vr_net_reliable_test: %d bytes to player %d\n", n, e);
}

void netStats_f()
{
    if(Cmd_Argc() > 1 && !strcmp(Cmd_Argv(1), "reset"))
    {
        for(NetStat& n : netStats)
        {
            n = NetStat{};
        }
        Con_Printf("vr_net_stats: reset\n");
        return;
    }
    if(!sv.active)
    {
        Con_Printf("vr_net_stats: no server here\n");
        return;
    }
    qcvm_t* oldVm = nullptr;
    PR_PushQCVM(&sv.qcvm, &oldVm);
    int used = 0;
    for(int i = 1; i < qcvm->num_edicts; i++)
    {
        used += EDICT_NUM(i)->free ? 0 : 1;
    }
    const int most = qcvm->max_edicts;
    PR_PopQCVM(oldVm);
    Con_Printf("vr_net_stats: %d entities in use (of %d)\n", used, most);
    for(int i = 0; i < svs.maxclients && i < MAX_SCOREBOARD; i++)
    {
        const client_t& c = svs.clients[i];
        if(!c.active)
        {
            continue;
        }
        const NetStat& n = netStats[i];
        const char* where = c.netconnection ? NET_QSocketGetAddressString(c.netconnection) : "?";
        Con_Printf("  client %d (%s): entities %d of %d in sight, %d B (room %d); peak %d B, %d sent; mean %.0f B over %d "
                   "frames, %d full\n",
            i + 1, where, n.sent, n.insight, n.bytes, n.maxsize, n.peakBytes, n.peakSent,
            n.frames ? n.sumBytes / n.frames : 0.0, n.frames, n.overflowFrames);
    }
}
} // namespace

extern "C" void VR_NetStatsEntities(edict_t* clent, int sent, int insight, int bytes, int maxsize)
{
    const int i = progs::clientSlot(clent);
    if(i < 0)
    {
        return;
    }
    NetStat& n = netStats[i];
    n.sent = sent;
    n.insight = insight;
    n.bytes = bytes;
    n.maxsize = maxsize;
    n.peakBytes = za::max(n.peakBytes, bytes);
    n.peakSent = za::max(n.peakSent, sent);
    n.frames++;
    n.overflowFrames += sent < insight ? 1 : 0;
    n.sumBytes += bytes;
}

extern "C" void VR_ReliableSent()
{
    broadcastRoom.peakReliable = q_max(broadcastRoom.peakReliable, sv.reliable_datagram.cursize);
}

namespace
{

// The magazine box of hand `h`'s gun (VrMove::magBox) into its fields (.magbox*, .offmagbox*), carried through a portal
// along the hand as its loading port is (all zero: none).
void setMagBox(edict_t* ent, int h, const glm::vec3& origin, const glm::vec3& handPos, const glm::vec3 (&box)[4])
{
    const FieldOffsets& f = fields();
    const bool none = box[1] == glm::vec3{0.f};
    const portals::Reach gate = portals::reachAlong(origin, handPos, box[0]);
    setFieldVec(ent, h ? f.magboxpos : f.offmagboxpos, none ? glm::vec3{0.f} : gate.position);
    setFieldVec(ent, h ? f.magboxx : f.offmagboxx, none ? glm::vec3{0.f} : gate.turn * box[1]);
    setFieldVec(ent, h ? f.magboxy : f.offmagboxy, none ? glm::vec3{0.f} : gate.turn * box[2]);
    setFieldVec(ent, h ? f.magboxz : f.offmagboxz, none ? glm::vec3{0.f} : gate.turn * box[3]);
}

// The ammo pouch's rounds (VrMove::ammoPouch) into .ammopouchpos, .ammopouchx/y/z (on the body: never through a portal;
// all zero: none).
// Hand `h`'s fields of `ent` (1: the main hand) from its move, through the portal it reaches through from `origin` (its
// place, turn and motion carried through the gate): the move's (VR_ReadMoveExtras), and again after walking (rebaseHands).
void writeHandFields(edict_t* ent, const glm::vec3& origin, const VrHandMove& hand, int h)
{
    const FieldOffsets& f = fields();
    const portals::Reach gate = portals::reach(origin, hand.pos);
    glm::vec3 angles = hand.rot;
    angles.y = anglemod(angles.y + gate.yaw);
    setFieldVec(ent, progs::handFields(h).pos, gate.position);
    setFieldVec(ent, progs::handFields(h).rot, angles);
    setFieldVec(ent, h ? f.handvel : f.offhandvel, gate.turn * hand.vel);
    setFieldVec(ent, progs::handFields(h).avel, gate.turn * hand.angVel);
    setFieldVec(ent, progs::handFields(h).throwVel, gate.turn * hand.throwVel);
    setFieldVec(ent, h ? f.handthrowpos : f.offhandthrowpos, gate.turn * (hand.throwPos - gate.from) + gate.to);
    setFieldVec(ent, h ? f.handthrowflick : f.offhandthrowflick, gate.turn * hand.throwFlick);
}

void setAmmoPouch(edict_t* ent, const glm::vec3 (&box)[4])
{
    const FieldOffsets& f = fields();
    setFieldVec(ent, f.ammopouchpos, box[0]);
    setFieldVec(ent, f.ammopouchx, box[1]);
    setFieldVec(ent, f.ammopouchy, box[2]);
    setFieldVec(ent, f.ammopouchz, box[3]);
}

} // namespace

extern "C" int VR_MoveMerges(client_t* client)
{
    const int clientNum = static_cast<int>(client - svs.clients);
    if(clientNum < 0 || clientNum >= svs.maxclients)
    {
        return 0;
    }
    if(clientNum >= static_cast<int>(clientBits.size()))
    {
        clientBits.resize(clientNum + 1);
    }
    ClientBits& bits = clientBits[clientNum];
    const bool merges = bits.moves++ > 0;
    bits.merged += merges ? 1 : 0;
    return merges ? 1 : 0;
}

// SV_ConnectClient into a loaded game (after VR_ClientReset): the grips and buttons held when the game was saved (its
// player's saved .vrbits0, current bits) count as held the frame before, so a grip still held across the load is no new
// press. Else the first move's grip was a rising edge: an empty hand at a holster drew its weapon (QC W_Frame's
// startedHandGrabbing). A flatscreen player's hands always grip (vr_client.cpp gripping): his off hand, posed by the
// hip holster as the stance takes it to the gun, drew the axe or the toolgun there after every load (2026-11-15).
extern "C" void VR_ClientLoadedBits(client_t* client)
{
    const int clientNum = static_cast<int>(client - svs.clients);
    const int ofs = fields().vrbits0;
    if(clientNum < 0 || clientNum >= svs.maxclients || ofs < 0 || !sv.active)
    {
        return;
    }
    if(clientNum >= static_cast<int>(clientBits.size()))
    {
        clientBits.resize(clientNum + 1);
    }
    const int saved = static_cast<int>(fieldFloatOr(EDICT_NUM(clientNum + 1), ofs, 0.f));
    // (received too: a server frame before his first move makes it the previous frame's)
    clientBits[clientNum].previousFrame = clientBits[clientNum].received = saved & currentBitsMask;
}

extern "C" void VR_ClientReset(client_t* client)
{
    const int clientNum = static_cast<int>(client - svs.clients);
    if(clientNum >= 0 && clientNum < static_cast<int>(clientBits.size()))
    {
        clientBits[clientNum] = {};
    }
    if(clientNum >= 0 && clientNum < static_cast<int>(clientMoves.size()))
    {
        clientMoves[clientNum] = {};
    }
    if(clientNum >= 0 && clientNum < static_cast<int>(avatarsSent.size()))
    {
        avatarsSent[clientNum] = {};
    }
    netmove::serverReset(clientNum); // (his moves' sequence and bases: a new connection's start again)
    moveguard::serverReset(clientNum); // (his values held, his room-scale budget)
    predict::serverReset(clientNum); // (his queued moves and their clock, B8)
    netsnap::serverReset(clientNum); // (and his snapshots: the next whole until he acknowledges one, B7b)
    mpavatar::serverReset(clientNum); // (his avatar calibration gone; the clients told)
    voicechat::serverReset(clientNum); // (his voice frames and budget, B10v)
    clientprefs::serverReset(clientNum); // (and his preferences: the server's until the next player's come)
    if(clientNum >= 0 && clientNum < static_cast<int>(talkGuards.size()))
    {
        talkGuards[clientNum] = {}; // (B28: a new player's talk starts clean)
    }
    if(clientNum >= 0 && clientNum < static_cast<int>(commandBudgets.size()))
    {
        commandBudgets[clientNum] = {}; // (and his commands' budget full)
    }
}

namespace
{

// (B28) The listen host's own client: the loopback's, whose server side's address is "LOCAL" (vr_netsnap.cpp's test).
bool isHostsOwn(const client_t* client)
{
    return SV_IsLocalClient(client); // (NET_QSocketIsLocal: a client without a connection, a bot, is not)
}

} // namespace

extern "C" int VR_GuestMay(const char* what)
{
    // (the server's console and the listen host's own player always; a guest -- any remote player, a dedicated server's
    // every one -- only when the host allows: NetQuake let any coop player kick the others, a room-code stranger too)
    if(cmd_source != src_client || !host_client || isHostsOwn(host_client) || vr_mp_guest_admin.value != 0.f)
    {
        return 1;
    }
    SV_ClientPrintf("%s: refused, only the host may (this server's vr_mp_guest_admin is 0)\n", what);
    Con_Printf("%s tried to %s: refused, a guest (vr_mp_guest_admin 0)\n", host_client->name, what);
    return 0;
}

extern "C" int VR_GuestMayTalk()
{
    if(cmd_source != src_client || !host_client || isHostsOwn(host_client))
    {
        return 1;
    }
    const int slot = static_cast<int>(host_client - svs.clients);
    if(slot < 0 || slot >= svs.maxclientslimit)
    {
        return 1;
    }
    if(slot >= static_cast<int>(talkGuards.size()))
    {
        talkGuards.resize(slot + 1);
    }
    TalkGuard& g = talkGuards[slot];
    if(realtime < g.silentUntil)
    {
        SV_ClientPrintf("You can't talk for %d more seconds.\n", static_cast<int>(ceil(g.silentUntil - realtime)));
        return 0;
    }
    if(g.count >= talkBurst && realtime - g.times[g.next] < talkWindow)
    {
        g.silentUntil = realtime + talkSilence;
        SV_ClientPrintf("You can't talk for %d more seconds (more than %d messages in %d seconds).\n",
            static_cast<int>(talkSilence), talkBurst, static_cast<int>(talkWindow));
        Con_Printf("%s silenced for %d seconds (talk flood)\n", host_client->name, static_cast<int>(talkSilence));
        return 0;
    }
    g.times[g.next] = realtime;
    g.next = (g.next + 1) % talkBurst;
    g.count = za::min(g.count + 1, talkBurst);
    return 1;
}

extern "C" int VR_ClientCommandBudget(const char* s)
{
    // (the listen host's own, and joining: prespawn, spawn, begin, which every client sends once, always)
    if(!host_client || !host_client->netconnection || isHostsOwn(host_client) || !q_strncasecmp(s, "prespawn", 8) ||
        !q_strncasecmp(s, "spawn", 5) || !q_strncasecmp(s, "begin", 5))
    {
        return 1;
    }
    const int slot = static_cast<int>(host_client - svs.clients);
    if(slot < 0 || slot >= svs.maxclientslimit)
    {
        return 1;
    }
    if(slot >= static_cast<int>(commandBudgets.size()))
    {
        commandBudgets.resize(slot + 1);
    }
    CommandBudget& b = commandBudgets[slot];
    b.tokens = b.tokens < 0.0 ? cmdBurst : za::min(cmdBurst, b.tokens + (realtime - b.last) * cmdRate);
    b.last = realtime;
    if(b.tokens < 1.0)
    {
        b.dropped++;
        if(realtime >= b.noteAt)
        {
            b.noteAt = realtime + cmdNoteEvery;
            Con_Printf("%s: commands dropped (more than %d at once or %d a second; %d so far)\n", host_client->name,
                static_cast<int>(cmdBurst), static_cast<int>(cmdRate), b.dropped);
        }
        return 0;
    }
    b.tokens -= 1.0;
    b.used++;
    b.least = za::min(b.least, b.tokens);
    return 1;
}

namespace
{

// A move's VR block run on client `client` (clc_move's, read by VR_ReadMoveExtras; or a remote client's from
// QVR_CLC_MOVES: vr_netmove.cpp).
void applyVrMove(client_t* client, const VrMove& sent)
{
    const int clientNum = static_cast<int>(client - svs.clients);
    edict_t* ent = client->edict;
    // A remote player's own values first bounded (vr_moveguard.hpp; MP_DESIGN P2-6): every place within reach of the
    // move's origin (the server's of him when it has none), every speed a real arm's; what was held counted
    // (vr_mp_status's mp_guard). The listen host's own and single player's: as they come.
    VrMove bounded = sent;
    if(client->netconnection && !isHostsOwn(client))
    {
        const glm::vec3 base = sent.origin != glm::vec3{0.f} ? sent.origin
                                                             : glm::vec3{ent->v.origin[0], ent->v.origin[1], ent->v.origin[2]};
        moveguard::count(clientNum, moveguard::guard(bounded, base, moveguard::unitsPerMetre(moveguard::slotScale(clientNum)), clientNum));
    }
    const VrMove& move = bounded;
    if(clientNum >= static_cast<int>(clientMoves.size()))
    {
        clientMoves.resize(clientNum + 1);
    }
    // A teleport asked in an earlier move of this server frame is kept with its spot: VR_ClientTeleport reads the stored
    // move once a frame, and a later move of the same frame without the bit overwrote it (the teleport lost: two moves
    // in a frame, a headset faster than the tick or a remote client's packets bunched; mp_teleport).
    VrMove kept = move;
    if(const ClientMove& prev = clientMoves[clientNum]; prev.valid && clientNum < static_cast<int>(clientBits.size()) &&
                                                       clientBits[clientNum].moves > 1 &&
                                                       (prev.move.vrBits0 & vrBitsTeleporting) &&
                                                       !(move.vrBits0 & vrBitsTeleporting))
    {
        kept.vrBits0 |= vrBitsTeleporting;
        kept.teleportTarget = prev.move.teleportTarget;
    }
    clientMoves[clientNum] = {true, kept, move.origin};

    const FieldOffsets& f = fields();

    setFieldVec(ent, f.v_viewangle, move.headAngles);
    setFieldFloat(ent, f.vryaw, move.vrYaw);
    setFieldFloat(ent, f.vr_posetime, move.poseClock);

    for(int h = 0; h < 2; h++)
    {
        writeHandFields(ent, move.origin, move.hands[h], h);
        setFieldFloat(ent, h ? f.handvelmag : f.offhandvelmag, move.hands[h].velMag);
        setFieldFloat(ent, h ? f.handthrowage : f.offhandthrowage, move.hands[h].throwAge);
    }

    setFieldVec(ent, f.headvel, move.headVel);
    setFieldVec(ent, f.headpos, move.headPos);
    for(int h = 0; h < 2; h++)
    {
        const portals::Reach gate = portals::reachAlong(move.origin, move.hands[h].pos, move.muzzlePos[h]);
        glm::vec3 angles = move.shotRot[h];
        angles.y = anglemod(angles.y + gate.yaw);
        setFieldVec(ent, progs::handFields(h).muzzle, gate.position);
        setFieldVec(ent, progs::handFields(h).shotRot, angles);
        const portals::Reach port = portals::reachAlong(move.origin, move.hands[h].pos, move.loadPort[h]);
        setFieldVec(ent, progs::handFields(h).loadPortPos, port.position);
        setFieldVec(ent, progs::handFields(h).loadPortAxis, port.turn * move.loadPortAxis[h]);
        setFieldVec(ent, progs::handFields(h).loadPortFace, port.turn * move.loadPortFace[h]);
        setFieldVec(ent, h ? f.launchofs : f.offlaunchofs, move.launchOfs[h]); // (the shot's frame: no turn)
        setMagBox(ent, h, move.origin, move.hands[h].pos, move.magBox[h]);
    }
    setAmmoPouch(ent, move.ammoPouch);
    if(clientNum >= static_cast<int>(clientBits.size()))
    {
        clientBits.resize(clientNum + 1);
    }
    ClientBits& bits = clientBits[clientNum];
    // Every move since the server's last frame counts: a press (and release) between two frames is seen this frame,
    // the bits ORed (the last move won: MP_PLAN.md, B2). VR_MoveMerges counted this move.
    const int movedBits = move.vrBits0 & ~(protocol::moveFlatplayBit | protocol::moveFlatCrouchBit);
    bits.received = bits.moves > 1 ? (bits.received | movedBits) : movedBits;
    // This player plays flatscreen (-flatscreenmode, his own client's: vr_flatplay.cpp): the QC's per-player gate
    // (VR_FlatPlay: no reloading, E's grab, 1 and 2 cycling; the hands' touch below).
    setFieldFloat(ent, f.vr_flatscreen, (move.vrBits0 & protocol::moveFlatplayBit) ? 1.f : 0.f);
    // Bit 14 (QC QVR_VRBITS0_HANDSTRACKED): this client's hands come from real tracking, so the QC
    // can tell a VR player from a flat-screen one (and from a bot).
    const int tracked = (move.buttons & QVR_BUTTON_HANDSTRACKED) ? (1 << 14) : 0;
    // Bits 17 and 18 (QVR_VRBITS0_OFFHAND_BUSY, _MAINHAND_BUSY): the hand holds the flashlight.
    const int busy = ((move.buttons & QVR_BUTTON_OFFHANDBUSY) ? (1 << 17) : 0) |
                     ((move.buttons & QVR_BUTTON_MAINHANDBUSY) ? (1 << 18) : 0);
    // Bits 19 and 20 (QVR_VRBITS0_OFFHAND_SECONDARY, _MAINHAND_SECONDARY): the hand's B/Y held (the grapple's reel).
    const int secondary = ((move.buttons & QVR_BUTTON_OFFHANDSECONDARY) ? (1 << 19) : 0) |
                          ((move.buttons & QVR_BUTTON_MAINHANDSECONDARY) ? (1 << 20) : 0);
    // Bits 21 and 22 (QVR_VRBITS0_OFFHAND_PRIMARY, _MAINHAND_PRIMARY): the hand's A/X held (the grapple's unreel).
    const int primary = ((move.buttons & QVR_BUTTON_OFFHANDPRIMARY) ? (1 << 21) : 0) |
                        ((move.buttons & QVR_BUTTON_MAINHANDPRIMARY) ? (1 << 22) : 0);
    // Bit 23 (QVR_VRBITS0_CROUCHED): the player has his crouched box (vr_hull.cpp). Flatscreen play
    // (his own client: vr_flatplay.cpp) crouches with a key, not with a head, and his eyes stand under the tallest
    // crouched box's top either way -- so his client's word for it (protocol::moveFlatCrouchBit: Left Ctrl held; let go,
    // standing if it fits, else the tallest crouched box that does, and his view rises only as far as that box: hull::
    // localEyeCap) is what vr_hull.cpp is asked with. A headset player is
    // asked as before, with his own head deciding (true).
    const bool flat = (move.vrBits0 & protocol::moveFlatplayBit) != 0;
    const bool flatCrouch = flat && (move.vrBits0 & protocol::moveFlatCrouchBit) != 0;
    // A headset client's bit 14 is its slide gesture (vr_slide: protocol::moveSlideBit; the shared slide, vr_slide.cpp).
    slide::setHeadsetInput(ent, !flat && (move.vrBits0 & protocol::moveSlideBit) != 0);
    // A remote client's head over his feet as his own client had it (from the origin he placed it from: his prediction,
    // a round trip ahead of this server's -- a jump's rise read as a crouch, a shorter box in the air than his replay's:
    // B8d). The loopback's and single player's are the server's own origin: as before.
    glm::vec3 head = move.headPos;
    if(netmove::serverSends(clientNum) && move.origin != glm::vec3{0.f} && head != glm::vec3{0.f})
    {
        head += glm::vec3{ent->v.origin[0], ent->v.origin[1], ent->v.origin[2]} - move.origin;
    }
    hull::updateCrouch(ent, head, !flat || flatCrouch);
    const int crouched = hull::isCrouched(ent) ? (1 << 23) : 0;
    // Lying down (vr_prone; his crouched box then the lowest, vr_prone_height): QC's .vr_prone (no jumping: PlayerJump).
    setFieldFloat(ent, f.vr_prone, hull::isProne(ent) ? 1.f : 0.f);
    setFieldFloat(ent, f.vrbits0, static_cast<float>(
        withPreviousBits(bits.received, bits.previousFrame) | tracked | busy | secondary | primary | crouched));
    // The head's height over the floor, a share of his standing eye height (vr_height_calibration; 1 standing): how the
    // world places it (vr_hands.cpp: the head over the box's origin less vr_floor_offset, in metres times the units). Seated
    // or flatscreen: 1. QC's shoulder charge asks for him standing tall (vr_kdtrig_tackle_stand).
    const float standing = units::eyeHeight() * units::metresToUnits();
    // Flatscreen: his eyes stand at Quake's viewheight over his box, not at the height the calibration gives a headset
    // player's head, so the share would read under 1 whatever he does -- it stays 1, and only the crouch his client says
    // he holds (Left Ctrl, vr_flat_crouch_drop; vr_flatplay.cpp, vr_hands.cpp lowers his head with it) lowers it, by how far
    // his head is under his view height, of his standing height: not a step's easing, nor vr_floor_offset. A headset
    // player takes the line below exactly as before (flatDrop is 0 for him).
    // (Held, or let go and his box kept crouched under a low ceiling, his view held down with it: hull::localEyeCap.)
    const float flatDrop = (flatCrouch || (flat && hull::isCrouched(ent)))
                               ? za::max(0.f, ent->v.view_ofs[2] - (move.headPos.z - ent->v.origin[2]))
                               : 0.f;
    const bool upright = (!flat && seated::active()) || (flat && flatDrop <= 0.f) || move.headPos == glm::vec3{0.f} ||
                         standing <= 0.f;
    setFieldFloat(ent, f.vr_stand,
        upright ? 1.f
                : flat ? za::clamp(1.f - flatDrop / standing, 0.f, 1.f)
                       : (move.headPos.z - ent->v.origin[2] - vr_floor_offset.value) / standing);
    setFieldFloat(ent, f.offhand_hotspot, move.hotspots[0]);
    setFieldFloat(ent, f.sawcord, move.sawCord);
    setFieldFloat(ent, f.handdrop, move.handDrop);
    setFieldFloat(ent, f.handsynth, move.handSynth); // (vr_handsource.cpp; QC VR_HandMotionTracked)
    setFieldFloat(ent, f.mainhand_hotspot, move.hotspots[1]);
    setFieldFloat(ent, f.button3, (move.buttons & QVR_BUTTON_OFFHANDATTACK) ? 1.f : 0.f);
}

// QVR_CLC_MOVES: each move not run yet, its NetQuake part then its VR block (as a clc_move is).
void applyNetMove(client_t* client, const netmove::WireMove& m, bool /*newest*/)
{
    VR_ApplyClientMove(client, m.time, &m.viewAngles[0], m.forward, m.side, m.up, m.buttons, m.impulse);
    lagcomp::noteMove(static_cast<int>(client - svs.clients), m.time, m.viewBack); // (what he saw: his shots', B8b)
    applyVrMove(client, m.vr);
    predict::serverQueue(client, m); // (his walk runs move by move in the frame's physics, B8)
}

} // namespace

extern "C" void VR_ReadMoveExtras(client_t* client)
{
    if(!vrProtocol())
    {
        return;
    }

    const za::Optional<VrMove> read = readVrMove();
    if(!read)
    {
        Con_DPrintf("VR: client %d sent a move that is not finite: dropped\n", static_cast<int>(client - svs.clients));
        return;
    }
    applyVrMove(client, *read);
}

extern "C" int VR_ReadQvrMoves(client_t* client)
{
    if(!vrProtocol() || !client->edict)
    {
        return 0;
    }
    return netmove::serverRead(client, applyNetMove) ? 1 : 0;
}

extern "C" int VR_AcceptsStaleMoves(client_t* client)
{
    return vrProtocol() && client->edict && netmove::serverTakesStale(static_cast<int>(client - svs.clients)) ? 1 : 0;
}

extern "C" void VR_ReadStaleQvrMoves(client_t* client)
{
    if(vrProtocol() && client->edict)
    {
        (void)netmove::serverRead(client, applyNetMove, true); // (a malformed overtaken one: dropped, as NetQuake did)
    }
}

extern "C" void VR_WriteMoveAck(sizebuf_t* msg, client_t* client)
{
    netmove::serverWriteAck(msg, static_cast<int>(client - svs.clients));
}

extern "C" void VR_LockstepRunMove(client_t* client)
{
    // (the harness's lockstep, vr_mptest.cpp: the stepping frame found every client's next move read)
    if(vr_mp_lockstep.value < 1.f || VR_LockstepHolds() || !vrProtocol() || !client->edict || !client->netconnection)
    {
        return;
    }
    (void)netmove::serverRunNext(client, applyNetMove);
}

extern "C" void VR_RunBufferedMoves(client_t* client)
{
    // (B19: a remote client's moves one a tick, in order, as the buffer kept them; vr_net_movebuffer)
    if(!vrProtocol() || !client->edict)
    {
        return;
    }
    netmove::serverRunBuffered(client, applyNetMove);
}

// The status bar's ammo count (STAT_AMMO). With the Quake VR progs .currentammo is the main hand's ammo *type* (AID_*:
// 1 shells, 2 nails, ...), and the classic client data message wrote it as the count: the status bar read 1 with a
// shotgun in hand and 17 shells, 2 with the nail gun (his report 2026-11-05: "the HUD seems to report wrong ammunition
// count for the held weapons"). VR_CalcStats already sent the counter, but only the custom stats path uses those; the
// client data message every protocol sends is fixed here (SV_WriteClientdataToMessage). The main hand's counter: what
// QC's updateAmmoCounter keeps (the reserve of the gun's ammo, or the clip of a gun whose clip is its only ammo).
extern "C" int VR_AmmoStat(edict_t* ent)
{
    if(!bindings().isVrProgs)
    {
        return static_cast<int>(ent->v.currentammo);
    }
    return static_cast<int>(fieldFloatOr(ent, fields().ammocounter, ent->v.currentammo));
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
    return wid >= 0 && wid < static_cast<int>(za::getArraySize(itemBits)) ? itemBits[wid] : 0;
}

extern "C" void VR_CalcStats(client_t* client, int* statsi, float* statsf)
{
    climb::calcStats(client->edict, statsi); // any progs
    foegrab::calcStats(client->edict, statsi);
    axestick::calcStats(client->edict, statsi);
    if(!bindings().isVrProgs)
    {
        return;
    }

    edict_t* ent = client->edict;
    const FieldOffsets& f = fields();

    const auto stat = [&](int index, int ofs) { statsf[index] = fieldFloatOr(ent, ofs, 0.f); };
    // A weapon's record (QC vr_weaponinst.qc), from the field referring to it: its magazine, its id.
    const auto inst = [&](int refOfs) -> edict_t* { return weaponInst(ent, refOfs); };
    const auto clip = [&](int refOfs) { edict_t* r = inst(refOfs); return r ? fieldFloatOr(r, f.wi_clip, 0.f) : 0.f; };

    statsf[STAT_QVR_WEAPON] = ent->v.weapon;

    // The VR progs keep the main hand's ammo type (AID_*) in .currentammo; the classic status bar
    // shows STAT_AMMO as a count.
    statsf[STAT_AMMO] = fieldFloatOr(ent, f.ammocounter, ent->v.currentammo);

    statsf[STAT_ACTIVEWEAPON] = static_cast<float>(VR_ActiveWeaponStat(ent));
    stat(STAT_QVR_WEAPON2, f.weapon2);
    statsi[STAT_QVR_WEAPONMODEL2] = modelIndexOfField(ent, f.weaponmodel2);
    stat(STAT_QVR_WEAPONFRAME2, f.weaponframe2);
    stat(STAT_QVR_WEAPONFLAGS, f.weaponflags);
    stat(STAT_QVR_WEAPONFLAGS2, f.weaponflags2);
    stat(STAT_QVR_AMMO2, f.currentammo2);
    statsf[STAT_QVR_AMMOTYPE] = ent->v.currentammo; // (the VR progs' main hand's ammo type: VRGetCurrentAmmo)
    stat(STAT_QVR_AMMOCOUNTER, f.ammocounter);
    stat(STAT_QVR_AMMOCOUNTER2, f.ammocounter2);
    statsf[STAT_QVR_WEAPONCLIP] = clip(f.weaponinst);
    statsf[STAT_QVR_WEAPONCLIP2] = clip(f.weaponinst2);
    statsi[STAT_QVR_WEAPONUID] = weaponUid(inst(f.weaponinst));
    statsi[STAT_QVR_WEAPONUID2] = weaponUid(inst(f.weaponinst2));
    stat(STAT_QVR_WEAPONCLIPSIZE, f.weaponclipsize);
    stat(STAT_QVR_WEAPONCLIPSIZE2, f.weaponclipsize2);
    stat(STAT_QVR_MELEE, f.vr_melee_hud);
    stat(STAT_QVR_HOTBLADE, f.vr_hot_hud); // incandescent blades' glow (vr_meleehud.cpp)
    stat(STAT_QVR_HOTBLADEHOLSTER, f.vr_hot_hud2);
    stat(STAT_QVR_MAGOUT, f.vr_mag_out); // the magazines sliding out of the guns in the hands (vr_view.cpp magSlideStep)
    stat(STAT_QVR_REVOLVER, f.vr_rv_hud); // the heavy revolvers in the hands (vr_view.cpp setupRevolverParts)
    stat(STAT_QVR_REVOLVER2, f.vr_rv_hud2);
    stat(STAT_QVR_REVOLVERCYL, f.vr_rv_cyl);
    stat(STAT_QVR_REVOLVERCYL2, f.vr_rv_cyl2);
    // The highest of the global mode and the guns' own (vr_reload_mode_<gun>; -1 follows the global): the clients draw the
    // pouch and the magazines if any gun reloads by hand.
    // A flat-screen player (.vr_flatscreen) reloads at the hip holsters whatever the headset's mode (vr_flat_reload; QC
    // VRIsWeaponReloadingEnabled): 2, so his ammo screens show the clip and no pouch or magazine is drawn for him.
    const bool flatPlayer = fieldFloatOr(ent, f.vr_flatscreen, 0.f) != 0.f;
    statsi[STAT_QVR_RELOADMODE] = vr_holster_mode.value != 0.f ? 0 : flatPlayer ? (vr_flat_reload.value != 0.f ? 2 : 0) : reloadModeBest();
    stat(STAT_QVR_FLATRELOAD, f.vr_flatrl);
    stat(STAT_QVR_FLATGREN, f.vr_flatgren); // the quick grenade's phase and hand (vr_flatgrenade.cpp)
    stat(STAT_QVR_FLATUNLOADFULL, f.vr_flatrl_full); // the holster menu's UNLOAD greyed, that ammo full up (vr_flatholster.cpp)
    stat(STAT_QVR_POUCHKIND, f.vr_pouch_kind);
    stat(STAT_QVR_POUCHCOUNT, f.vr_pouch_count);
    stat(STAT_QVR_AMMOLAVA, f.ammo_lava_nails); // Rogue's ammo: the flat-screen visor's ammo grid
    stat(STAT_QVR_AMMOMULTI, f.ammo_multi_rockets);
    stat(STAT_QVR_AMMOPLASMA, f.ammo_plasma);
    statsf[STAT_QVR_STEALTH] = fieldFloatOr(ent, f.stl_hud, -1.f);
    stat(STAT_QVR_ADHUD, f.vr_ad_hud); // AD support: AD's row on the wrist gadget (vr_gadget.cpp)
    if(vr_ad_mounted.value != 0.f) // (and whether AD's data is in play here: a remote client's AD page, ad::clientDataInPlay)
    {
        statsf[STAT_QVR_ADHUD] = static_cast<float>(static_cast<int>(statsf[STAT_QVR_ADHUD]) | ADHUD_DATA);
    }
    stat(STAT_QVR_ADKEYS, f.vr_ad_hudkeys);
    stat(STAT_QVR_ADTIME, f.vr_ad_hudtime);
    stat(STAT_QVR_ADTINT, f.vr_ad_tint); // AD support: AD's screen tint (vr_ad.cpp VR_AD_TintBlend)
    stat(STAT_QVR_ADTINTDEN, f.vr_ad_tintden);
    stat(STAT_QVR_ADART1, f.vr_ad_hudart1); // AD support: the gadget's AD page (vr_gadget.cpp adPage)
    stat(STAT_QVR_ADART2, f.vr_ad_hudart2);
    stat(STAT_QVR_ADDEBUFF, f.vr_ad_huddebuff);
    stat(STAT_QVR_ADUPGRADES, f.vr_ad_hudup);
    stat(STAT_QVR_DWPOWER, f.vr_dw_hud); // Dwell support: Haste, Berserk, the Wetsuit on the wrist gadget (vr_gadget.cpp)
    stat(STAT_QVR_ADSKYFOG, f.vr_ad_skyfog); // AD support: AD's sky fog (vr_ad.cpp VR_AD_SkyFog)

    const int holsterWeapon[numHolsters] = {f.holsterweapon0, f.holsterweapon1,
        f.holsterweapon2, f.holsterweapon3, f.holsterweapon4, f.holsterweapon5};
    const int holsterModel[numHolsters] = {f.holsterweaponmodel0,
        f.holsterweaponmodel1, f.holsterweaponmodel2, f.holsterweaponmodel3,
        f.holsterweaponmodel4, f.holsterweaponmodel5};
    const int holsterFlags[numHolsters] = {f.holsterweaponflags0,
        f.holsterweaponflags1, f.holsterweaponflags2, f.holsterweaponflags3,
        f.holsterweaponflags4, f.holsterweaponflags5};
    const int holsterInst[numHolsters] = {f.holsterweaponinst0, f.holsterweaponinst1,
        f.holsterweaponinst2, f.holsterweaponinst3, f.holsterweaponinst4,
        f.holsterweaponinst5};

    // Force grab, per hand: what it points at (1), is locked on to (2), or pulls in flight (3).
    const auto entityField = [&](edict_t* e, int ofs) -> edict_t* {
        const int v = ofs >= 0 ? fieldInt(e, ofs) : 0;
        return v ? PROG_TO_EDICT(v) : nullptr;
    };
    const auto forcegrab = [&](int targetOfs, int lockedOfs, int pulledOfs, float hand) {
        if(edict_t* p = entityField(ent, pulledOfs); p && !p->free && fieldFloatOr(p, f.fg_state, 0.f) == 1.f &&
                                                      entityField(p, f.fg_player) == ent &&
                                                      fieldFloatOr(p, f.fg_hand, -1.f) == hand)
        {
            return NUM_FOR_EDICT(p) * 4 + 3;
        }
        if(edict_t* t = entityField(ent, targetOfs); t && !t->free)
        {
            return NUM_FOR_EDICT(t) * 4 + (fieldFloatOr(ent, lockedOfs, 0.f) != 0.f ? 2 : 1);
        }
        return 0;
    };
    statsi[STAT_QVR_FGMAIN] = forcegrab(f.mainhand_fgtarget, f.mainhand_fglocked, f.mainhand_fgpulled, 1.f);
    statsi[STAT_QVR_FGOFF] = forcegrab(f.offhand_fgtarget, f.offhand_fglocked, f.offhand_fgpulled, 0.f);

    // Carrying, per hand: the object (the client draws it in the hand, vr_held.cpp).
    const auto carried = [&](int ofs) {
        edict_t* e = entityField(ent, ofs);
        return e && !e->free && !box3d::isRagdoll(NUM_FOR_EDICT(e)) ? NUM_FOR_EDICT(e) : 0; // (a ragdoll's limb: not drawn in the hand)
    };
    statsi[STAT_QVR_CARRYMAIN] = carried(f.mainhand_held);
    statsi[STAT_QVR_CARRYOFF] = carried(f.offhand_held);
    statsi[STAT_QVR_CARRYITEM] = static_cast<int>(fieldFloatOr(ent, f.vr_carry_items, 0.f)); // (their zones: vr_holsterzones.cpp)

    for(int i = 0; i < numHolsters; i++)
    {
        stat(STAT_QVR_HOLSTERWEAPON0 + i, holsterWeapon[i]);
        statsi[STAT_QVR_HOLSTERWEAPONMODEL0 + i] = modelIndexOfField(ent, holsterModel[i]);
        stat(STAT_QVR_HOLSTERWEAPONFLAGS0 + i, holsterFlags[i]);
        statsf[STAT_QVR_HOLSTERWEAPONCLIP0 + i] = clip(holsterInst[i]);
        statsi[STAT_QVR_HOLSTERCLIPSIZE0 + i] =
            clipSizeOf(fieldFloatOr(ent, holsterWeapon[i], 0.f), inst(holsterInst[i]));
        statsi[STAT_QVR_HOLSTERWEAPONUID0 + i] = weaponUid(inst(holsterInst[i])); // (its blood drawn holstered: vr_wounds.cpp)
    }

    // Magnetic holsters: each holster's bound weapon's id (QC vr_magholster.qc; the chains: vr_magholster.cpp).
    const int magBind[numHolsters] = {f.vr_magbind0, f.vr_magbind1, f.vr_magbind2, f.vr_magbind3, f.vr_magbind4, f.vr_magbind5};
    for(int i = 0; i < numHolsters; i++)
    {
        statsi[STAT_QVR_MAGBIND0 + i] = static_cast<int>(fieldFloatOr(ent, magBind[i], 0.f));
    }
}

// QC's QVR_WPNFLAG_NOMAG (vr_defs.qc): a magazine gun with no magazine in (vr_reload.qc).
constexpr int weaponFlagNoMag = 16;
// QC's QVR_WPNFLAG_SSG_OPEN (vr_defs.qc): a super shotgun broken open (vr_reload.qc).
constexpr int weaponFlagSsgOpen = 32;

// A stepping monster in the air (a dog's leap, a fall off a ledge, a knock: not on the ground, not a flyer or a
// swimmer; SV_Physics_Step's free fall) moves every server frame, not at its thinks: its move is drawn over the frame
// (the U_LERPFINISH byte, in 255ths of a second), not to its next think. (Drawn to the think, each frame's move began
// before the last was drawn: with vr_monster_lerp_continue 1 the drawn dog fell behind, ~26 units by the leap's end,
// and caught up in a frame or two as its think came: a 9.7-unit snap on landing; Quake's drawing snapped each frame.)
// -1: Quake's rule (the next think), and always with vr_monster_lerp_continue 0.
extern "C" int VR_StepLerpInterval(edict_t* ent)
{
    if(static_cast<int>(ent->v.movetype) != MOVETYPE_STEP || vr_monster_lerp_continue.value == 0.f ||
       (static_cast<int>(ent->v.flags) & (FL_ONGROUND | FL_FLY | FL_SWIM)) != 0)
    {
        return -1;
    }
    const int interval = static_cast<int>(host_frametime * 255.0 + 0.5);
    return interval < 1 ? 1 : interval > 25 ? 25 : interval;
}

// A stepping monster standing on a moving lift (a MOVETYPE_PUSH it rides, moving): the lift moves it every server
// frame, so it is sent without U_STEP and drawn as the lift is (between the last two messages), not along its steps to
// its next think. Drawn to its think it fell ~13 units behind the drawn lift riding e1m1's plat up, and ~1.8 drawn over
// a frame (the step lerp's finish counts from the message's time, its start from the client's: vr_debug_lift; his note
// of 2026-11-21, riders "compressed by the lift"). Its own steps on a moving lift are drawn over the frame too.
extern "C" int VR_StepRidesLift(edict_t* ent)
{
    if(static_cast<int>(ent->v.movetype) != MOVETYPE_STEP || !(static_cast<int>(ent->v.flags) & FL_ONGROUND) ||
       ent->v.groundentity <= 0)
    {
        return 0;
    }
    const edict_t* g = PROG_TO_EDICT(ent->v.groundentity);
    return !g->free && static_cast<int>(g->v.movetype) == MOVETYPE_PUSH &&
           (g->v.velocity[0] != 0.f || g->v.velocity[1] != 0.f || g->v.velocity[2] != 0.f);
}

namespace
{
// What a thing that is not a weapon prop tells its loot beam (vr_lootbeams.cpp), sent with U_QVR_WEAPONUID and weapon id
// 0 (the clip's byte): bit 7 carried in a hand (no beam), bits 0-6 a magazine's fullness in percent (127: not a
// magazine). -1: nothing to tell (not carried, not a magazine), nothing sent.
[[nodiscard]] int lootByte(edict_t* ent)
{
    const FieldOffsets& f = fields();
    const bool carried = (f.carry_player >= 0 && fieldInt(ent, f.carry_player) != 0) || runRefuses(ent); // (no beam)
    const float wid = fieldFloatOr(ent, f.vr_ammo_wid, 0.f);
    if(!carried && wid == 0.f)
    {
        return -1;
    }
    int full = 127;
    if(wid != 0.f)
    {
        const float count = fieldFloatOr(ent, f.vr_ammo_count, 0.f);
        const float cap = fieldFloatOr(ent, f.vr_ammo_cap, 0.f);
        const float share = cap > 0.f ? count / cap : (count > 0.f ? 1.f : 0.f); // (an older save's: no capacity)
        full = CLAMP(0, static_cast<int>(share * 100.f + 0.5f), 100);
    }
    return (carried ? 0x80 : 0) | full;
}
} // namespace

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
    const float packState = fieldFloatOr(ent, f.vr_wp_state, 0.f);
    if(fieldFloatOr(ent, f.vr_rigid, 0.f) != 0.f || packState == 1.f || packState == 2.f)
    {
        bits |= U_QVR_NOROTATE; // (a rigid body; a worn backpack, placed on its wearer's back: QC vr_wornpack.qc)
    }
    else if(fieldFloatOr(ent, f.vr_pickup_spin, 0.f) != 0.f)
    {
        bits |= U_QVR_SPIN; // a weapon pickup drawn as its prop (QC vr_pickup_prop_models): spins as the g_ models do
    }
    if(NUM_FOR_EDICT(ent) > svs.maxclients && weaponUid(weaponInst(ent, f.weaponinst)) != 0)
    {
        bits |= U_QVR_WEAPONUID; // a weapon prop (not a player: his .weaponinst is his main hand's, sent as a stat)
        if(static_cast<int>(fieldFloatOr(ent, f.weaponflags, 0.f)) & weaponFlagNoMag)
        {
            bits |= U_QVR_NOMAG; // its magazine out (immersive reloading: the client draws none on it)
        }
        if(static_cast<int>(fieldFloatOr(ent, f.weaponflags, 0.f)) & weaponFlagSsgOpen)
        {
            bits |= U_QVR_SSGOPEN; // a super shotgun broken open (the client draws it open, its chambers loaded)
        }
    }
    else if(NUM_FOR_EDICT(ent) > svs.maxclients && lootByte(ent) >= 0)
    {
        bits |= U_QVR_WEAPONUID; // weapon id 0: a magazine's fullness or a carried thing, for its loot beam (lootByte)
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
    if(bits & U_QVR_WEAPONUID)
    {
        edict_t* const inst = weaponInst(ent, f.weaponinst);
        if(weaponUid(inst) == 0)
        {
            // Not a weapon prop: id 0, then the loot beam's byte (lootByte).
            MSG_WriteLong(msg, 0);
            MSG_WriteByte(msg, za::max(lootByte(ent), 0));
            return;
        }
        // Its id (records' ids stay far below 2^24), an incandescent blade's glow in bits 24-30 (0-127: QC .vr_hot_glow,
        // vr_hotblade.cpp draws it; 0 in older demos), bit 31 a spent single-use enemy gun (QC .vr_depleted: its loot
        // beam, vr_lootbeams.cpp; 0 in older demos).
        const int hot = CLAMP(0, static_cast<int>(fieldFloatOr(ent, f.vr_hot_glow, 0.f) * 127.f + 0.5f), 127);
        const uint32_t spent =
            (fieldFloatOr(ent, f.vr_depleted, 0.f) != 0.f || runRefuses(ent)) ? 0x80000000u : 0u; // (Melee Only: no beam)
        MSG_WriteLong(msg, static_cast<int>((static_cast<uint32_t>(weaponUid(inst)) & 0xFFFFFFu) |
                                            (static_cast<uint32_t>(hot) << 24) | spent));
        // Its clip: the rounds in it, shown on its ammo screen as a held gun's (vr_view.cpp setupWorldWeapons).
        MSG_WriteByte(msg, CLAMP(0, static_cast<int>(inst ? fieldFloatOr(inst, f.wi_clip, 0.f) : 0.f), 255));
        // Its magazine's size (its kind's own: the ammo screen's low and empty colours, known before it was ever held).
        MSG_WriteByte(msg, clipSizeOf(ent->v.weapon, inst));
    }
    if(bits & U_QVR_SSGOPEN)
    {
        edict_t* const inst = weaponInst(ent, f.weaponinst);
        const float clip = inst ? fieldFloatOr(inst, f.wi_clip, 0.f) : 0.f;
        MSG_WriteByte(msg, CLAMP(0, static_cast<int>(clip), 2)); // its chambers loaded
    }
}

extern "C" void VR_WriteClientSpawnState(sizebuf_t* msg)
{
    // (his client cleared its snapshots for this map: the next goes whole, B7b; the counts kept)
    netsnap::serverReset(static_cast<int>(host_client - svs.clients), true);
    if(vrProtocol())
    {
        worldtext::serverWriteAll(msg);
        tips::serverWriteAll(msg);
        serverrules::serverWriteAll(msg);
        agarmor::serverWriteAll(msg); // (the armed humanoids' armour: vr_agarmor.cpp)
        arenaxray::serverWriteAll(msg); // (the arena's last foes, seen through walls: vr_arenaxray.cpp)
        humlook::serverWriteAll(msg); // (the armed humanoids' recolours by weapon: vr_humlook.cpp)
        climbrope::serverWriteAll(msg); // (the climbable ropes: vr_climbrope.cpp)
        mpavatar::serverWriteAll(msg); // (the players' avatar calibrations: vr_mpavatar.cpp; multiplayer)
    }
}

// SV_SendClientDatagram, after svc_time, before the entities and the frame's sounds: the precaches made in the last
// half second (recentPrecaches), so what uses them in the same datagram finds them.
extern "C" void VR_WriteRecentPrecaches(sizebuf_t* msg)
{
    while(!recentPrecaches.empty() && recentPrecaches[0].until < qcvm->time)
    {
        recentPrecaches.erase(recentPrecaches.begin());
    }
    for(const RecentPrecache& p : recentPrecaches)
    {
        if(msg->cursize + 4 + static_cast<int>(strlen(p.name)) + 1 > msg->maxsize / 2)
        {
            break; // (the entities first: the reliable copy follows anyway)
        }
        MSG_WriteByte(msg, svc_quakevr);
        MSG_WriteByte(msg, p.subcmd);
        MSG_WriteShort(msg, p.index);
        MSG_WriteString(msg, p.name);
    }
}

namespace
{

// The avatar as last sent per client slot (writeAvatars: an unchanged one waits for its keyframe), and the counts
// (vr_mp_avatars: sent, skipped as unchanged, bytes): avatarsSent, avatarStats (above).

// A short of `v` in eighths of a unit, clamped (an avatar's offsets from the player's origin: +-4095 units).
void writeEighths(sizebuf_t* msg, const glm::vec3& v)
{
    for(int i = 0; i < 3; i++)
    {
        MSG_WriteShort(msg, static_cast<int>(za::clamp(std::round(v[i] * 8.f), -32767.f, 32767.f)));
    }
}

void writeAngles16(sizebuf_t* msg, const glm::vec3& a)
{
    for(int i = 0; i < 3; i++)
    {
        MSG_WriteAngle16(msg, a[i], sv.protocolflags);
    }
}

// A player's guns as the server has them (QC's fields: what his own client's stats say of them), for the others to draw
// them as his client does (MP_PLAN.md, "Held weapons' state"): each hand's look (VrGunLook: [0] off, [1] main), then each
// holstered weapon's flags (their low byte: no magazine, broken open) and clip, the holsters `holsters` has drawn.
void writeGunLooks(sizebuf_t* msg, edict_t* ent, const VrDrawnHolsters& holsters)
{
    const FieldOffsets& f = fields();
    const bool vrProgs = bindings().isVrProgs;
    const bool flat = vrProgs && fieldFloatOr(ent, f.vr_flatscreen, 0.f) != 0.f;
    // (as VR_CalcStats' STAT_QVR_RELOADMODE for him, and the client's magazinesShown, ssgBreaks)
    const int mode = vr_holster_mode.value != 0.f ? 0 : flat ? (vr_flat_reload.value != 0.f ? 2 : 0) : reloadModeBest();
    const int magOut = static_cast<int>(fieldFloatOr(ent, f.vr_mag_out, 0.f));
    const auto clipOf = [&](edict_t* r) { return r ? CLAMP(0, static_cast<int>(fieldFloatOr(r, f.wi_clip, 0.f)), 255) : 0; };
    for(int h = 0; h < 2; h++)
    {
        const bool main = h == 1;
        VrGunLook l;
        l.bits = (mode == 3 || flat ? VrGunLook::magsShownBit : 0) | (mode == 0 ? VrGunLook::clipFullBit : 0) |
                 ((mode == 3 && vr_reload_ssg_break.value != 0.f) || flat ? VrGunLook::ssgBreaksBit : 0);
        edict_t* const inst = vrProgs ? weaponInst(ent, main ? f.weaponinst : f.weaponinst2) : nullptr;
        const int flags = static_cast<int>(fieldFloatOr(ent, main ? f.weaponflags : f.weaponflags2, 0.f));
        const int rv = static_cast<int>(fieldFloatOr(ent, main ? f.vr_rv_hud : f.vr_rv_hud2, 0.f));
        if(vrProgs && (inst || flags || rv))
        {
            l.bits |= VrGunLook::gunBit | ((magOut & (main ? 2 : 1)) ? VrGunLook::magOutBit : 0) |
                      (rv ? VrGunLook::revolverBit : 0);
            l.flags = static_cast<za::U16>(flags & 0xFFFF);
            l.clip = static_cast<za::U8>(clipOf(inst));
            l.clipSize = static_cast<za::U8>(CLAMP(0, static_cast<int>(fieldFloatOr(ent, main ? f.weaponclipsize : f.weaponclipsize2, 0.f)), 255));
            l.revolver = rv;
            l.cylinder = fieldFloatOr(ent, main ? f.vr_rv_cyl : f.vr_rv_cyl2, 0.f);
        }
        writeGunLook(msg, l);
    }
    const int holsterFlags[numHolsters] = {f.holsterweaponflags0, f.holsterweaponflags1, f.holsterweaponflags2,
        f.holsterweaponflags3, f.holsterweaponflags4, f.holsterweaponflags5};
    const int holsterInst[numHolsters] = {f.holsterweaponinst0, f.holsterweaponinst1, f.holsterweaponinst2,
        f.holsterweaponinst3, f.holsterweaponinst4, f.holsterweaponinst5};
    for(int i = 0; i < VrDrawnHolsters::count && i < numHolsters; i++)
    {
        if(holsters.valid && (holsters.weapons & (1 << i)))
        {
            MSG_WriteByte(msg, vrProgs ? static_cast<int>(fieldFloatOr(ent, holsterFlags[i], 0.f)) & 255 : 0);
            MSG_WriteByte(msg, vrProgs ? clipOf(weaponInst(ent, holsterInst[i])) : 0);
        }
    }
}

// A bot's holstered weapons (syncfix; the voice note 2026-11-21: "so that I can see what they're actually carrying"): its
// QC holsters' models, frames 0, places left to each client (VrDrawnGear::solveBit: on its body as drawn).
[[nodiscard]] VrDrawnHolsters botHolstered(edict_t* ent)
{
    const FieldOffsets& f = fields();
    const int holsterModel[numHolsters] = {f.holsterweaponmodel0, f.holsterweaponmodel1, f.holsterweaponmodel2,
        f.holsterweaponmodel3, f.holsterweaponmodel4, f.holsterweaponmodel5};
    VrDrawnHolsters h;
    h.valid = true;
    for(int i = 0; i < VrDrawnHolsters::count && i < numHolsters; i++)
    {
        const int model = holsterModel[i] >= 0 ? modelIndexOfField(ent, holsterModel[i]) : 0;
        if(model > 0)
        {
            h.weapons = static_cast<za::U8>(h.weapons | (1 << i));
            h.model[i] = static_cast<za::U16>(model);
        }
    }
    return h;
}

// Each VR player's avatar this frame (QVR_SVC_AVATAR: his head and hands as his last move placed them, his weapons) to
// the other clients that see him (server::scopedBegin; his own client does not: his entity is not noted for himself).
// Multiplayer only: a single player's own hands are his client's (MP_PLAN.md, B2; vr_avatar.cpp draws nothing yet).
void writeAvatars()
{
    if(svs.maxclients <= 1)
    {
        return;
    }
    const FieldOffsets& f = fields();
    const VrDrawnHolsters noHolsters{};
    const auto v3 = [](const float* v) { return glm::vec3{v[0], v[1], v[2]}; };
    for(int i = 0; i < svs.maxclients; i++)
    {
        const client_t& c = svs.clients[i];
        const ClientMove* const m = i < static_cast<int>(clientMoves.size()) && clientMoves[i].valid ? &clientMoves[i] : nullptr;
        // A bot (FrikBot: its client slot's entity a "player" whose QC is not a human's, the slot itself not active; MP_PLAN.md,
        // B11 part 3) has no client to draw its hands: its controllers (QC's handpos, handrot: bot_setvrfields) and
        // weapons go instead, for each client to place its hands and weapons on as it places its own
        // (VrDrawnHand::solveBit, view::solveHands); its head at its eyes, its body a standing player's of its eye height.
        edict_t* const ent = c.active ? c.edict : EDICT_NUM(i + 1);
        const bool bot = server::isBot(i);
        if(!bot && (!c.active || !c.spawned || !c.edict || !m))
        {
            continue;
        }
        VrBodyPose body;
        VrDrawnHand drawn[2];
        if(bot)
        {
            body.valid = true;
            body.yaw = ent->v.v_angle[1];
            // (its eye height, metres: the default player's, drawn at the default world scale (units::botEyeUnits; his
            // notes 2026-11-21: "the same height as the player"); a dead bot's lowered view is not its height)
            body.headHeight = body.standingHeight = units::modelEyeHeight;
            for(int h = 0; h < 2; h++)
            {
                VrDrawnHand& d = drawn[h];
                d.flags = VrDrawnHand::solveBit;
                d.handOrigin = fieldVec(ent, handFields(h).pos);
                d.handAngles = fieldVec(ent, handFields(h).rot);
                d.weaponModel = static_cast<za::U16>(h ? SV_ModelIndex(PR_GetString(ent->v.weaponmodel)) : modelIndexOfField(ent, f.weaponmodel2));
                d.weaponFrame = static_cast<za::U8>(za::clamp(static_cast<int>(h ? ent->v.weaponframe : fieldFloatOr(ent, f.weaponframe2, 0.f)), 0, 255));
                // (bots3) its weapon where the server has it (its disarm volume's, its muzzle's): every client draws it there
                if(view::HeldPlace p; server::botHeld(ent, h, p))
                {
                    d.flags |= VrDrawnHand::weaponBit;
                    d.weaponOrigin = p.origin;
                    d.weaponAngles = p.angles;
                }
            }
        }
        // (offsets from the origin his client placed them from: the hands as drawn on his body, wherever the server has it)
        const glm::vec3 origin = m ? m->move.origin : v3(ent->v.origin);
        // Written here first: unchanged since the last sent (an idle player), it goes only every vr_net_avatar_keyframe
        // frames (what a lost one or a player just come into sight waits at most).
        byte data[avatarMaxBytes];
        sizebuf_t tmp{};
        tmp.data = data;
        tmp.maxsize = avatarMaxBytes;
        MSG_WriteByte(&tmp, svc_quakevr);
        MSG_WriteByte(&tmp, QVR_SVC_AVATAR);
        MSG_WriteShort(&tmp, i + 1);
        MSG_WriteByte(&tmp, mpavatar::movesByte(ent, m && (m->move.vrBits0 & protocol::moveFlatplayBit) != 0));
        // (rebaseHands moves the head and its base along together)
        // (a bot's head: its eyes raised to the default player's height, units::botEyeRaise)
        writeEighths(&tmp, m ? m->move.headPos - m->base
                             : v3(ent->v.view_ofs) + glm::vec3{0.f, 0.f,
                                   units::botEyeRaise(static_cast<float>(DEFAULT_VIEWHEIGHT) - ent->v.mins[2])});
        writeAngles16(&tmp, m ? m->move.headAngles : v3(ent->v.v_angle));
        writeBodyPose(&tmp, m ? m->move.body : body); // (his body's yaw, crouch and lean, as his client poses it: B2d)
        const VrDrawnHand* const hands = m ? m->move.drawn : drawn;
        for(int h = 0; h < 2; h++)
        {
            writeDrawnHand(&tmp, hands[h], origin);
        }
        MSG_WriteLong(&tmp, static_cast<int>(ent->v.weapon));
        MSG_WriteLong(&tmp, static_cast<int>(fieldFloatOr(ent, f.weapon2, 0.f)));
        mpavatar::writeHolds(&tmp, ent, origin); // (his climbing holds, as the moves byte says: B5c)
        // (his holstered weapons as he drew them: B6b; a bot's, what it carries, placed by each client: syncfix)
        const VrDrawnHolsters botHolsters = bot ? botHolstered(ent) : noHolsters;
        writeDrawnHolsters(&tmp, m ? m->move.holsters : botHolsters, origin, false);
        MSG_WriteByte(&tmp, mpavatar::gearByte(ent));        // (the armour he wears: his pauldrons' colour)
        // (his guns as the server has them, and his torch as he drew it: wpnsync)
        writeGunLooks(&tmp, ent, m ? m->move.holsters : botHolsters);
        writeDrawnLamp(&tmp, m ? m->move.lamp : VrDrawnLamp{}, origin);
        // (his worn gear as he drew it; a bot's placed by each client on its body: syncfix)
        VrDrawnGear gear;
        gear.shown = VrDrawnGear::solveBit;
        if(m)
        {
            gear = m->move.gear;
            // (what his ammo pouch holds, the server's word: your STAT_QVR_POUCHKIND and _POUCHCOUNT's fields, syncfix2)
            gear.pouchKind = static_cast<za::U8>(za::clamp(static_cast<int>(fieldFloatOr(ent, f.vr_pouch_kind, 0.f)), 0, 255));
            gear.pouchCount = static_cast<za::U8>(za::clamp(static_cast<int>(fieldFloatOr(ent, f.vr_pouch_count, 0.f)), 0, 255));
        }
        const VrBodyPose& wroteBody = m ? m->move.body : body; // (the yaw as the client reads it back: writeBodyPose)
        writeDrawnGear(&tmp, gear, origin, wroteBody.valid ? wroteBody.yaw : 0.f, hands);

        if(static_cast<int>(avatarsSent.size()) < svs.maxclients)
        {
            avatarsSent.resize(svs.maxclients);
        }
        AvatarSent& last = avatarsSent[i];
        const int keyframe = za::max(static_cast<int>(vr_net_avatar_keyframe.value), 1);
        const bool same = last.size == tmp.cursize && ZA_MEMCMP(last.bytes, data, static_cast<size_t>(tmp.cursize)) == 0;
        const bool waits = same && ++last.unsent < keyframe;
        // (queued either way: a snapshot client's snapshot has it every frame, B7b; to the others a waiting one goes not)
        sizebuf_t* msg = qvr::server::scopedBegin(i + 1, true);
        if(!msg)
        {
            continue;
        }
        SZ_Write(msg, data, tmp.cursize);
        qvr::server::scopedEnd(waits ? qvr::server::scopedAvatarUnchanged : qvr::server::scopedAvatar);
        if(waits)
        {
            avatarStats.skipped++;
            continue;
        }
        ZA_MEMCPY(last.bytes, data, static_cast<size_t>(tmp.cursize));
        last.size = tmp.cursize;
        last.unsent = 0;
        avatarStats.sent++;
        avatarStats.bytes += tmp.cursize;
    }
}

} // namespace

extern "C" void VR_ServerFrameBegin()
{
    // This game's own player's drawn weapon poses, ground spots and holster presses into his slot, as a remote player's
    // messages put his: the server's readers ask the slot alone (MP_PLAN B11a).
    qvr::heldpose::publishOwn();
    qvr::heldpose::placeBots(); // (bots3: each bot's weapons as every client draws them)
}

extern "C" void VR_ServerFrameEnd()
{
    QVR_TRACY_SCOPE(zoneFrameEnd, "VR server frame end"); // (profiling builds only, as the steps' zones)
    QVR_TRACY_CALL("sv end: weapon insts", sweepWeaponInsts()); // the weapons' records nothing has any more: freed, their ids gone
    QVR_TRACY_CALL("sv end: tips", tips::serverFrame()); // the map tips whose entity is gone (before its slot is reused)

    QVR_TRACY_CALL("sv end: leg sweep", qvr::legsweep::serverFrame()); // a staff swept through a humanoid's legs knocks it down (its poses not yet kept)
    QVR_TRACY_CALL("sv end: foe grab", qvr::foegrab::serverFrame()); // held enemies slowed and dragged (before their poses are kept)
    QVR_TRACY_CALL("sv end: body contact", qvr::bodycontact::serverFrame()); // a player and a monster overlapping (a hold let go of) pushed apart
    QVR_TRACY_CALL("sv end: hit model poses", qvr::hitmodel::serverFrame()); // precise hits: the client's lerp of the monsters' poses and steps, kept
    QVR_TRACY_CALL("sv end: lag compensation", qvr::lagcomp::serverFrame()); // the players' and monsters' places and poses kept (a remote client's shots)
    qvr::disarmviz::serverFrame(); // Show Disarm Volumes: the fists measured against the armed humanoids' weapons (off: nothing)
    QVR_TRACY_CALL("sv end: axe stick", qvr::axestick::serverFrame()); // thrown axes stuck in things go with them (after the poses above)
    QVR_TRACY_CALL("sv end: foe grab spots", qvr::foegrab::afterPoses()); // the held spots placed (after the poses above); a hand pulled too far lets go
    QVR_TRACY_CALL("sv end: explosion debris", qvr::explosiondebris::serverFrame()); // the explosions' chunks: new ones launched, the ended gone, the fades
    QVR_TRACY_CALL("sv end: limb models", qvr::limbmodel::spawnFrame()); // a kind of monster first spawned now: its limbs made ahead of its first cut
    progs::loadNoticeFrame(); // a loaded save's warning (another build's), once the player is in
    QVR_TRACY_CALL("sv end: rig sequences", qvr::rigseq::serverFrame()); // QC's Rig Studio sequences: started, their events sent to QC (vr_rigseq.cpp)
    QVR_TRACY_CALL("sv end: armour", qvr::agarmor::serverFrame()); // the armed humanoids' armour sent where it changed (vr_agarmor.cpp)
    qvr::arenaxray::serverFrame(); // the arena's last foes marked or not, sent where it changed (vr_arenaxray.cpp)
    qvr::humlook::serverFrame(); // the armed humanoids' recolours by weapon, sent where they changed (vr_humlook.cpp)
    QVR_TRACY_CALL("sv end: climbable ropes", qvr::climbrope::serverFrame()); // the climbable ropes stepped, pushed, sent (vr_climbrope.cpp)
    qvr::rigstudio::serverFrame(); // the Rig Studio's scene: its subject made and held (nothing while closed)

    // The next move read starts the next frame's buttons and bits (VR_MoveMerges; any protocol: the buttons too).
    for(ClientBits& bits : clientBits)
    {
        bits.moves = 0;
    }

    if(!vrProtocol())
    {
        return;
    }

    // This frame's VR bits become next frame's "previous" bits. The player's own .vrbits0 too: a frame with no move from
    // his client (a dedicated server's frame between two of his moves, bunched moves) keeps it as it was, and an edge
    // (a grip pressed or let go) was seen again every such frame (a let-go noted twice: QC's grip times moved on).
    const int vrbitsOfs = fields().vrbits0;
    for(size_t i = 0; i < clientBits.size(); i++)
    {
        ClientBits& bits = clientBits[i];
        bits.previousFrame = bits.received;
        // (a bot has no connection and no moves: its own QC sets its bits)
        const bool mover = static_cast<int>(i) < svs.maxclients && svs.clients[i].active && svs.clients[i].netconnection;
        edict_t* const ent = mover ? svs.clients[i].edict : nullptr;
        if(ent && vrbitsOfs >= 0)
        {
            const int now = static_cast<int>(fieldFloatOr(ent, vrbitsOfs, 0.f));
            setFieldFloat(ent, vrbitsOfs, static_cast<float>(
                (now & ~(currentBitsMask << 1)) | ((bits.received & currentBitsMask) << 1)));
        }
    }

    writeAvatars(); // the other players' heads and hands to each client that sees them

    // Late precaches (setmodel on an unprecached model, precache_* after load).
    broadcastNewPrecaches(sv.model_precache, broadcastModelCount, QVR_SVC_PRECACHE_MODEL);
    broadcastNewPrecaches(sv.sound_precache, broadcastSoundCount, QVR_SVC_PRECACHE_SOUND);
    serverrules::serverFrame(); // a server rule changed: every client gets the new value

    qvr::motion::serverFrame(); // the motion recorder's sample of this server frame
    qvr::fatigue::serverFrame(); // vr_debug_stamina_hold
}

namespace qvr::server
{

bool isBot(int slot)
{
    if(!sv.active || slot < 0 || slot >= svs.maxclients || svs.clients[slot].active || slot + 1 >= sv.qcvm.num_edicts)
    {
        return false;
    }
    const FieldOffsets& f = fields();
    edict_t* const ent = EDICT_NUM(slot + 1);
    return !ent->free && ent->v.modelindex != 0.f && f.ishuman >= 0 && fieldFloatOr(ent, f.ishuman, 1.f) == 0.f &&
           !strcmp(PR_GetString(ent->v.classname), "player");
}

bool botHeld(edict_t* bot, int hand, view::HeldPlace& out)
{
    const FieldOffsets& f = fields();
    const int index = hand ? SV_ModelIndex(PR_GetString(bot->v.weaponmodel)) : modelIndexOfField(bot, f.weaponmodel2);
    qmodel_t* const model = index > 0 && index < MAX_MODELS ? sv.models[index] : nullptr;
    const progs::HandFields& hf = progs::handFields(hand);
    if(!model || hf.pos < 0 || hf.rot < 0)
    {
        return false;
    }
    return view::placeHeld(model, hand, fieldVec(bot, hf.pos), fieldVec(bot, hf.rot), out);
}

// vr_dumpplayer: prints the local player's VR fields as the server sees them.
void dumpPlayer_f()
{
    if(!sv.active || !bindings().isVrProgs)
    {
        Con_Printf("vr_dumpplayer: no server running Quake VR progs\n");
        return;
    }

    // vr_dumpplayer [client]: 1 (the default) is the first client (a listen server's own player).
    const int client = Cmd_Argc() > 1 ? Q_atoi(Cmd_Argv(1)) : 1;
    if(client < 1 || client > svs.maxclients || !svs.clients[client - 1].active)
    {
        Con_Printf("vr_dumpplayer: no active client %d\n", client);
        return;
    }
    edict_t* ent = svs.clients[client - 1].edict;
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
    vec("headpos", f.headpos);
    vec("muzzlepos", f.muzzlepos);
    vec("offhandpos", f.offhandpos);
    vec("offmuzzlepos", f.offmuzzlepos);
    Con_Printf("  vrbits0 %d  button3 %g  weapon %g  weapon2 %g\n",
        static_cast<int>(fieldFloatOr(ent, f.vrbits0, 0.f)), fieldFloatOr(ent, f.button3, 0.f),
        ent->v.weapon, fieldFloatOr(ent, f.weapon2, 0.f));
}

void learnSoundLoops()
{
    const double t0 = Sys_DoubleTime();
    soundLoopsFor();
    for(int num = 1; num < MAX_SOUNDS && sv.sound_precache[num]; num++)
    {
        if(soundLoops.isLoop[static_cast<za::SizeT>(num)] < 0)
        {
            (void)soundLoops_(num);
        }
    }
    VR_TimeAdd("  the sounds read for their loops (vr_snd_loops)", Sys_DoubleTime() - t0); // (a fixed label: summed by it)
}

void resetClients()
{
    clientMoves.clear();
    avatarsSent.clear();
    // The sound loops (by entity and sound number) forgotten too: a restart, a loaded game or a changelevel of the same map
    // keeps sv.worldmodel and sv.name, but its sounds' precache order may differ (skill, Randomized Enemies).
    soundLoops.world = nullptr;
}

void avatarCounts(int& sent, int& skipped, long long& bytes)
{
    sent = avatarStats.sent;
    skipped = avatarStats.skipped;
    bytes = avatarStats.bytes;
}

namespace
{
// Forgotten with the server's world (vr_mem.hpp's resets): the clients' moves, the avatars sent, the sounds' loops.
mem::Reset serverWorldReset{"server", mem::ResetEvent::ServerWorld, mem::ResetPhase::State, &resetClients};
} // namespace

const VrMove* clientMove(edict_t* player)
{
    const ClientMove* m = clientMoveOf(player);
    return m ? &m->move : nullptr;
}

bool holsterPoint(edict_t* player, int slot, glm::vec3& out)
{
    const ClientMove* m = clientMoveOf(player);
    if(!m || !m->move.holsters.valid || slot < 0 || slot >= VrDrawnHolsters::count)
    {
        return false;
    }
    out = glm::vec3{player->v.origin[0], player->v.origin[1], player->v.origin[2]} + m->move.holsters.at[slot] - m->move.origin;
    return true;
}

CommandCounts commandCounts(int clientNum)
{
    if(clientNum < 0 || clientNum >= static_cast<int>(commandBudgets.size()))
    {
        return {0, 0, static_cast<int>(cmdBurst)};
    }
    const CommandBudget& b = commandBudgets[clientNum];
    return {b.used, b.dropped, static_cast<int>(b.least)};
}

int mergedMoves(int clientNum)
{
    return clientNum >= 0 && clientNum < static_cast<int>(clientBits.size()) ? clientBits[clientNum].merged : 0;
}

void rebaseHands(edict_t* player)
{
    ClientMove* m = clientMoveOf(player);
    if(!m || m->base == glm::vec3{0.f})
    {
        return;
    }
    const glm::vec3 origin{player->v.origin[0], player->v.origin[1], player->v.origin[2]};
    const glm::vec3 delta = origin - m->base;
    if(delta == glm::vec3{0.f})
    {
        return;
    }
    m->base = origin;

    VrMove& move = m->move;
    const FieldOffsets& f = fields();
    setFieldVec(player, f.headpos, fieldVec(player, f.headpos) + delta);
    for(VrHandMove& hand : move.hands)
    {
        hand.pos += delta;
        hand.tracked += delta;
        hand.throwPos += delta;
    }
    move.lampLens += delta; // (the stealth AI's view of his flashlight: vr_stealth.cpp)
    move.muzzlePos[0] += delta;
    move.muzzlePos[1] += delta;
    move.loadPort[0] += delta;
    move.loadPort[1] += delta;
    for(auto& box : move.magBox)
    {
        if(box[1] != glm::vec3{0.f})
        {
            box[0] += delta;
        }
    }
    move.headPos += delta;
    if(move.ammoPouch[3] != glm::vec3{0.f})
    {
        move.ammoPouch[0] += delta; // (the ammo pouch goes with the body)
        setAmmoPouch(player, move.ammoPouch);
    }
    // Re-evaluate crossing after walking, including a crossing on a release frame.
    for(int h = 0; h < 2; h++)
    {
        const auto& hand = move.hands[h];
        writeHandFields(player, origin, hand, h);
        const auto muzzle = portals::reachAlong(origin, hand.pos, move.muzzlePos[h]);
        glm::vec3 angles = move.shotRot[h];
        angles.y = anglemod(angles.y + muzzle.yaw);
        setFieldVec(player, progs::handFields(h).muzzle, muzzle.position);
        setFieldVec(player, progs::handFields(h).shotRot, angles);
        const portals::Reach port = portals::reachAlong(origin, hand.pos, move.loadPort[h]);
        setFieldVec(player, progs::handFields(h).loadPortPos, port.position);
        setFieldVec(player, progs::handFields(h).loadPortAxis, port.turn * move.loadPortAxis[h]);
        setFieldVec(player, progs::handFields(h).loadPortFace, port.turn * move.loadPortFace[h]);
        setMagBox(player, h, origin, hand.pos, move.magBox[h]);
    }
}

float* clientHeadAngles(edict_t* player)
{
    ClientMove* m = clientMoveOf(player);
    return m ? &m->move.headAngles.x : nullptr;
}

void sendHaptic(edict_t* player, int hand, float delay, float duration, float frequency, float amplitude)
{
    sizebuf_t* msg = clientMessage(player);
    if(!msg)
    {
        return;
    }

    MSG_WriteByte(msg, svc_quakevr);
    MSG_WriteByte(msg, QVR_SVC_HAPTIC);
    MSG_WriteByte(msg, hand);
    MSG_WriteFloat(msg, delay);
    MSG_WriteFloat(msg, duration);
    MSG_WriteFloat(msg, frequency);
    MSG_WriteFloat(msg, amplitude);
}

// A knock on a player's drawn hand (a parried blow): `strength` units along `dir`, dying out.
void sendHandImpact(edict_t* player, int hand, float strength, const float dir[3])
{
    sizebuf_t* msg = clientMessage(player);
    if(!msg)
    {
        return;
    }

    MSG_WriteByte(msg, svc_quakevr);
    MSG_WriteByte(msg, QVR_SVC_HANDIMPACT);
    MSG_WriteByte(msg, hand);
    MSG_WriteFloat(msg, strength);
    for(int i = 0; i < 3; i++)
    {
        MSG_WriteFloat(msg, dir[i]);
    }
}

// A weapon a player's hand caught from a force grab, and where it was (vr_drawblend.cpp eases it into the hand).
void sendCatchBlend(edict_t* player, int hand, int ent, const float origin[3], const float angles[3])
{
    sizebuf_t* msg = clientMessage(player);
    if(!msg)
    {
        return;
    }

    MSG_WriteByte(msg, svc_quakevr);
    MSG_WriteByte(msg, QVR_SVC_CATCHBLEND);
    MSG_WriteByte(msg, hand);
    MSG_WriteShort(msg, ent);
    for(int i = 0; i < 3; i++)
    {
        MSG_WriteFloat(msg, origin[i]);
    }
    for(int i = 0; i < 3; i++)
    {
        MSG_WriteFloat(msg, angles[i]);
    }
}

void sendCollect(edict_t* player, int hand, int hotspot, int ent, int modelIndex, const float origin[3],
    const float angles[3])
{
    sizebuf_t* msg = clientMessage(player);
    if(!msg || msg->cursize > msg->maxsize - 64)
    {
        return;
    }

    MSG_WriteByte(msg, svc_quakevr);
    MSG_WriteByte(msg, QVR_SVC_COLLECT);
    MSG_WriteByte(msg, CLAMP(0, hand, 255));
    MSG_WriteByte(msg, CLAMP(0, hotspot, 255));
    MSG_WriteShort(msg, ent);
    MSG_WriteShort(msg, modelIndex);
    for(int i = 0; i < 3; i++)
    {
        MSG_WriteFloat(msg, origin[i]);
    }
    for(int i = 0; i < 3; i++)
    {
        MSG_WriteFloat(msg, angles[i]);
    }
}

void sendCollectSeen(edict_t* seen, int hand, int hotspot, int ent, int modelIndex, const float origin[3],
    const float angles[3])
{
    if(!seen || seen->free || !sv.worldmodel)
    {
        return;
    }
    for(int i = 0; i < svs.maxclients; ++i)
    {
        const client_t& c = svs.clients[i];
        if(!c.active || !c.spawned || !c.edict || !SV_VisibleToClient(c.edict, seen, sv.worldmodel))
        {
            continue;
        }
        sendCollect(c.edict, hand, hotspot, ent, modelIndex, origin, angles);
    }
}

// Spent casings out of the weapon in a player's hand (vr_shells.cpp): only that player's client
// draws them, from its own weapon model.
void sendEject(edict_t* player, int hand, int kind, int count, int flags, float delay)
{
    if(count <= 0 || !player)
    {
        return;
    }
    const auto write = [&](sizebuf_t* msg, bool other) {
        MSG_WriteByte(msg, svc_quakevr);
        MSG_WriteByte(msg, other ? QVR_SVC_EJECTOTHER : QVR_SVC_EJECT);
        if(other)
        {
            MSG_WriteShort(msg, NUM_FOR_EDICT(player));
        }
        MSG_WriteByte(msg, hand);
        MSG_WriteByte(msg, kind);
        MSG_WriteByte(msg, CLAMP(0, count, 255));
        MSG_WriteByte(msg, flags);
        MSG_WriteByte(msg, CLAMP(0, static_cast<int>(delay * 100.f + 0.5f), 255));
    };
    if(sizebuf_t* msg = clientMessage(player))
    {
        write(msg, false);
    }
    // (syncfix2) The others who see him: his casings out of his copy's weapon (8 B a shot each).
    if(svs.maxclients <= 1 || !sv.worldmodel)
    {
        return;
    }
    for(int i = 0; i < svs.maxclients; ++i)
    {
        const client_t& c = svs.clients[i];
        if(!c.active || !c.spawned || !c.edict || c.edict == player || !SV_VisibleToClient(c.edict, player, sv.worldmodel))
        {
            continue;
        }
        if(sizebuf_t* msg = clientMessage(c.edict))
        {
            write(msg, true);
        }
    }
}

void sendViewMessage(edict_t* player, const char* text, float seconds, bool arena)
{
    sizebuf_t* msg = clientMessage(player);
    if(!msg)
    {
        return;
    }
    MSG_WriteByte(msg, svc_quakevr);
    MSG_WriteByte(msg, arena ? QVR_SVC_ARENAMESSAGE : QVR_SVC_VIEWMESSAGE);
    MSG_WriteShort(msg, CLAMP(0, static_cast<int>(seconds * 10.f + 0.5f), 32767));
    MSG_WriteString(msg, text);
}

void sendShock(edict_t* player, int kind, const float org[3], float radius, float duration)
{
    sizebuf_t* msg = nullptr;
    if(shock::toOneClient(kind))
    {
        msg = clientMessage(player);
    }
    else if(sv.datagram.cursize <= MAX_DATAGRAM - 24)
    {
        msg = &sv.datagram;
    }
    if(!msg)
    {
        return;
    }

    MSG_WriteByte(msg, svc_quakevr);
    MSG_WriteByte(msg, QVR_SVC_SHOCK);
    MSG_WriteByte(msg, CLAMP(0, kind, 255));
    for(int i = 0; i < 3; i++)
    {
        MSG_WriteCoord(msg, org[i], sv.protocolflags);
    }
    MSG_WriteShort(msg, CLAMP(0, static_cast<int>(radius), 32767));
    // (In 1/50 s: 5 s at most; a body's lasting shock, kind 4, and a fire's smoulder, 5 and 6, in 1/4 s: 63 s.)
    MSG_WriteByte(msg, CLAMP(0, static_cast<int>(duration * (shock::quarterSeconds(kind) ? 4.f : 50.f) + 0.5f), 255));
    if(msg == &sv.datagram)
    {
        VR_BroadcastMessageEnd(); // a boundary
    }
}

void sendFired(edict_t* shooter, int hand)
{
    if(sv.datagram.cursize > MAX_DATAGRAM - 8)
    {
        return;
    }
    MSG_WriteByte(&sv.datagram, svc_quakevr);
    MSG_WriteByte(&sv.datagram, QVR_SVC_FIRED);
    MSG_WriteShort(&sv.datagram, NUM_FOR_EDICT(shooter));
    // (HAND_OFF, HAND_MAIN; 2 and 3 a stream weapon's flame from that hand + 2: vr_weaponfx.cpp streamed)
    // (4: a monster's own rocket tube, QC QVR_FIRED_ROCKET: AD's rocket soldier; else 255, a monster's gun)
    MSG_WriteByte(&sv.datagram, hand >= 0 && hand <= 4 ? hand : 255);
    VR_BroadcastMessageEnd(); // a boundary
}

void sendTracer(edict_t* shooter, int hand, const float from[3], const float to[3])
{
    // A shotgun's 14 pellets are 14 of these: a full datagram keeps its room for the rest.
    if(sv.datagram.cursize > MAX_DATAGRAM - 64)
    {
        return;
    }
    MSG_WriteByte(&sv.datagram, svc_quakevr);
    MSG_WriteByte(&sv.datagram, QVR_SVC_TRACER);
    MSG_WriteShort(&sv.datagram, NUM_FOR_EDICT(shooter));
    MSG_WriteByte(&sv.datagram, hand == 0 || hand == 1 ? hand : 255); // (HAND_OFF, HAND_MAIN)
    for(int i = 0; i < 3; i++)
    {
        MSG_WriteCoord(&sv.datagram, from[i], sv.protocolflags);
    }
    for(int i = 0; i < 3; i++)
    {
        MSG_WriteCoord(&sv.datagram, to[i], sv.protocolflags);
    }
    VR_BroadcastMessageEnd(); // a boundary
}

void init()
{
    Cmd_AddCommand("vr_dumpplayer", dumpPlayer_f);
    Cmd_AddCommand("vr_net_stats", netStats_f);
    Cmd_AddCommand("vr_net_broadcast_report", broadcastReport_f); // [reset]: the broadcast's bytes sent and culled (B13b)
    Cmd_AddCommand("vr_net_reliable_test", netReliableTest_f);
    Cmd_AddCommand("vr_snd_loops", soundLoops_f);
    climb::init();
    foegrab::init();
    legsweep::init();
    bodycontact::init();
    ledges::init();
}

// Everything precached while loading is in the serverinfo that clients receive.
void onSpawnServerAfterLoad()
{
    broadcastModelCount = precacheCount(sv.model_precache);
    broadcastSoundCount = precacheCount(sv.sound_precache);
    recentPrecaches.clear();
    serverrules::serverReset(); // (the clients get every rule with their spawn state)
}

} // namespace qvr::server
