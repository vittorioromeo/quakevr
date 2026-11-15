// vr_mptest.cpp -- the multiplayer test aids (docs/vr-port/MP_PLAN.md, "Test harness"; Misc/quakevr/mp/mp_harness.py).
//
// - `-cmdinbox <file>` (command line): every host frame, the lines appended to that file since the last frame are run as
//   console commands (whole lines only). The harness drives several headless games (a dedicated server, listen hosts,
//   clients) through their inboxes: one writer, append-only; the game only reads it.
// - `vr_mp_watch 1` (Debug > Reports > Network: Watch Players): the client prints, with a wall-clock time in
//   milliseconds (comparable between processes on one machine), when it has joined a map ("mp_event: ... joined"), left
//   it, first sees another player's entity ("mp_seen"), sees a player start moving from rest ("mp_move": farther than
//   vr_mp_watch_dist from where he stood still) and every 32 units of travel after ("mp_pos").
// - `vr_mp_mark <label>`: prints "mp_mark: t=<ms> <label> org=<own origin>" (a scenario's step, timestamped).
// - `vr_mp_status` (Debug > Reports > Network: Players Seen): this client's view of the players (and on a server, its
//   clients): who is in, where, whether this frame's message carried them.
// - `vr_mp_avatars` (Debug > Reports > Network: Player Avatars; vr_mpavatar.cpp): the other players' heads and hands this
//   client has (QVR_SVC_AVATAR: "mp_avatar:") and, hosting, the server's (from their moves: "mp_avatar_sv:").

#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_geom.hpp"
#include "vr_mpavatar.hpp"
#include "vr_netmove.hpp"
#include "vr_predict.hpp"
#include "vr_netsnap.hpp"
#include "vr_progs.hpp"
#include "vr_clientprefs.hpp"
#include "vr_server.hpp"
#include "vr_teleport.hpp"
#include "vr_trace.hpp"
#include "vr_lagcomp.hpp"

#include <chrono>
#include <stdio.h>
#include <string.h>

namespace
{

using qvr::geom::vec;

// The inbox (-cmdinbox): its path and how far it was read.
struct Inbox
{
    char path[MAX_OSPATH]{};
    long offset{0};
    bool on{false};
};
Inbox inbox;

// One player's entity as this client watched it.
struct Watched
{
    bool seen{false};
    bool moving{false};
    glm::vec3 rest{0.f};     // where it stood still
    double restSince{0.0};   // since when (realtime)
    glm::vec3 printed{0.f};  // the last mp_pos / mp_move printed
    glm::vec3 last{0.f};     // last frame's origin
};
constexpr int watchedMost = MAX_SCOREBOARD + 1;
Watched watched[watchedMost];
bool joined = false; // signed on to a map (the last frame)

// (B11) The game's score as this client last printed it: each scoreboard slot's name and frags ("mp_frags"), the
// intermission and its own health ("mp_event: intermission", "died", "respawned").
struct Score
{
    bool shown{false};
    int frags{0};
    char name[32]{};
};
struct ScoreWatch
{
    Score slots[MAX_SCOREBOARD];
    int intermission{0};
    bool alive{true};
};
ScoreWatch scoreWatch;

// vr_mp_trace <ent> <frames>: each frame, another player's copy as this client drew it (his entity and both hands: B7's
// mp_loss, their steps frame by frame), for that many frames, then "mp_trace: done".
struct Trace
{
    int ent{0};
    int left{0};
    int lastFrame{-1};
};
Trace trace;

constexpr double restTime = 0.25;  // s still before a move counts as one from rest
constexpr float stillEps = 0.25f;  // units a frame that still counts as still
constexpr float posStep = 32.f;    // units of travel between mp_pos lines

[[nodiscard]] long long wallMs()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

// The new whole lines of the inbox, to the command buffer.
void pollInbox()
{
    FILE* f = fopen(inbox.path, "rb");
    if(!f)
    {
        return;
    }
    char buf[4096];
    if(fseek(f, inbox.offset, SEEK_SET) == 0)
    {
        const size_t got = fread(buf, 1, sizeof(buf) - 1, f);
        size_t end = got;
        while(end > 0 && buf[end - 1] != '\n')
        {
            end--; // (a line still being written waits for the next frame)
        }
        if(end > 0)
        {
            buf[end] = 0;
            Cbuf_AddText(buf);
            inbox.offset += static_cast<long>(end);
        }
    }
    fclose(f);
}

[[nodiscard]] bool signedOn()
{
    return cls.state == ca_connected && cls.signon == SIGNONS && cl_entities && cl.maxclients > 0;
}

[[nodiscard]] bool inMessage(const entity_t& e)
{
    return e.model && e.msgtime == cl.mtime[0];
}

// (B11) The scoreboard's changes (a player or bot named, his frags), the intermission's start and end and this
// client's own deaths and respawns, as they happen: deathmatch's events for mp_dm.
void watchScore()
{
    const int most = q_min(cl.maxclients, MAX_SCOREBOARD);
    for(int i = 0; cl.scores && i < most; i++)
    {
        const scoreboard_t& sc = cl.scores[i];
        Score& w = scoreWatch.slots[i];
        const bool named = sc.name[0] != 0;
        if(named == w.shown && (!named || (sc.frags == w.frags && strcmp(sc.name, w.name) == 0)))
        {
            continue;
        }
        w.shown = named;
        w.frags = sc.frags;
        q_strlcpy(w.name, sc.name, sizeof(w.name));
        Con_Printf("mp_frags: t=%lld slot=%d named=%d name=\"%s\" frags=%d\n", wallMs(), i, named ? 1 : 0, sc.name,
            sc.frags);
    }
    if(cl.intermission != scoreWatch.intermission)
    {
        scoreWatch.intermission = cl.intermission;
        Con_Printf("mp_event: t=%lld intermission=%d\n", wallMs(), cl.intermission);
    }
    const bool alive = cl.stats[STAT_HEALTH] > 0;
    if(alive != scoreWatch.alive)
    {
        scoreWatch.alive = alive;
        const glm::vec3 org = cl.viewentity > 0 && cl.viewentity < cl.num_entities ? vec(cl_entities[cl.viewentity].origin)
                                                                                    : glm::vec3{0.f};
        Con_Printf("mp_event: t=%lld %s org=%.1f %.1f %.1f\n", wallMs(), alive ? "respawned" : "died", org.x, org.y, org.z);
    }
}

void watchFrame()
{
    const bool now = signedOn();
    if(now != joined)
    {
        joined = now;
        for(Watched& w : watched)
        {
            w = Watched{};
        }
        scoreWatch = ScoreWatch{};
        if(now)
        {
            Con_Printf("mp_event: t=%lld joined map=%s self=%d maxclients=%d\n", wallMs(), cl.mapname, cl.viewentity,
                cl.maxclients);
        }
        else
        {
            Con_Printf("mp_event: t=%lld left\n", wallMs());
        }
    }
    if(!now || qvr::vr_mp_watch.value == 0.f)
    {
        return;
    }
    watchScore();
    const float dist = q_max(qvr::vr_mp_watch_dist.value, 0.1f);
    const int most = q_min(cl.maxclients, watchedMost - 1);
    for(int i = 1; i <= most; i++)
    {
        const entity_t& e = cl_entities[i];
        Watched& w = watched[i];
        const glm::vec3 org = vec(e.origin);
        const bool self = i == cl.viewentity;
        if(!w.seen)
        {
            if(!inMessage(e))
            {
                continue;
            }
            w.seen = true;
            w.rest = w.printed = w.last = org;
            w.restSince = realtime;
            Con_Printf("mp_seen: t=%lld ent=%d self=%d name=\"%s\" org=%.1f %.1f %.1f\n", wallMs(), i, self ? 1 : 0,
                cl.scores ? cl.scores[i - 1].name : "", org.x, org.y, org.z);
            continue;
        }
        if(glm::distance(org, w.last) > stillEps)
        {
            if(!w.moving && glm::distance(org, w.rest) > dist)
            {
                w.moving = true;
                w.printed = org;
                Con_Printf("mp_move: t=%lld ent=%d self=%d org=%.1f %.1f %.1f\n", wallMs(), i, self ? 1 : 0, org.x,
                    org.y, org.z);
            }
            else if(w.moving && glm::distance(org, w.printed) >= posStep)
            {
                w.printed = org;
                Con_Printf("mp_pos: t=%lld ent=%d self=%d org=%.1f %.1f %.1f\n", wallMs(), i, self ? 1 : 0, org.x,
                    org.y, org.z);
            }
            if(!w.moving)
            {
                w.restSince = realtime; // (creeping under the distance: not a rest yet)
            }
        }
        else if(realtime - w.restSince > restTime || w.moving)
        {
            if(w.moving)
            {
                w.moving = false;
                w.restSince = realtime;
                Con_Printf("mp_stop: t=%lld ent=%d self=%d org=%.1f %.1f %.1f\n", wallMs(), i, self ? 1 : 0, org.x,
                    org.y, org.z);
            }
            w.rest = org;
        }
        w.last = org;
    }
}

void mark_f()
{
    const char* label = Cmd_Argc() > 1 ? Cmd_Args() : "";
    glm::vec3 org{0.f};
    if(signedOn() && cl.viewentity > 0 && cl.viewentity < cl.num_entities)
    {
        org = vec(cl_entities[cl.viewentity].origin);
    }
    Con_Printf("mp_mark: t=%lld %s org=%.1f %.1f %.1f\n", wallMs(), label, org.x, org.y, org.z);
}

void trace_f()
{
    if(Cmd_Argc() < 3)
    {
        Con_Printf("vr_mp_trace <player entity> <frames>: each frame, his copy as drawn here (entity, hands)\n");
        return;
    }
    trace.ent = Q_atoi(Cmd_Argv(1));
    trace.left = q_max(Q_atoi(Cmd_Argv(2)), 0);
    trace.lastFrame = -1;
}

void traceFrame()
{
    if(trace.left <= 0)
    {
        return;
    }
    glm::vec3 body, hands[2];
    int frame = -1;
    if(!qvr::mpavatar::drawnPlaces(trace.ent, body, hands, frame) || frame == trace.lastFrame)
    {
        return; // (not drawn this frame: counted when it is)
    }
    trace.lastFrame = frame;
    double st = 0.0;
    float so[3]{};
    if(!qvr::netsnap::interpSample(trace.ent, 0, st, so))
    {
        st = -1.0;
    }
    Con_Printf("mp_trace: t=%lld ent=%d f=%d ct=%.4f org=%.2f %.2f %.2f h0=%.2f %.2f %.2f h1=%.2f %.2f %.2f rt=%.4f "
               "nw=%.4f dl=%.1f rl=%.4f st=%.4f so=%.2f %.2f %.2f\n",
        wallMs(), trace.ent, frame, cl.time, body.x, body.y, body.z, hands[0].x, hands[0].y, hands[0].z, hands[1].x,
        hands[1].y, hands[1].z, qvr::netsnap::interpTime(), qvr::netsnap::interpNewest(), qvr::netsnap::interpDelayMs(),
        realtime, st, so[0], so[1], so[2]);
    if(--trace.left == 0)
    {
        Con_Printf("mp_trace: done\n");
    }
}

void status_f()
{
    // (intermission, health: this client's view, cl.intermission and its health stat: the coop exit and death, B4b)
    Con_Printf("mp_status: t=%lld dedicated=%d server=%d clients=%d connected=%d map=%s self=%d intermission=%d health=%d\n",
        wallMs(), isDedicated ? 1 : 0, sv.active ? 1 : 0, sv.active ? svs.maxclients : 0, signedOn() ? 1 : 0,
        signedOn() ? cl.mapname : "-", signedOn() ? cl.viewentity : 0, signedOn() ? cl.intermission : 0,
        signedOn() ? cl.stats[STAT_HEALTH] : 0);
    // (a remote server's snapshots read, undecodable ones, records; the interpolation buffer: on, its delay, the frames
    // its time passed the newest message; B7b)
    const qvr::netsnap::ClientCounts cs = qvr::netsnap::clientCounts();
    Con_Printf("mp_snapcl: decoded=%d undecodable=%d bad=%d records=%d bytes=%lld interp=%d delay_ms=%.1f holds=%d\n",
        cs.decoded, cs.undecodable, cs.bad, cs.records, cs.bytes, qvr::netsnap::interpActive() ? 1 : 0,
        qvr::netsnap::interpDelayMs(), qvr::netsnap::interpHolds());
    qvr::predict::report(); // (B8: our own walk predicted)
    if(sv.active)
    {
        int spawned = 0;
        for(int i = 0; i < svs.maxclients; i++)
        {
            const client_t& c = svs.clients[i];
            if(!c.active)
            {
                continue;
            }
            spawned += c.spawned ? 1 : 0;
            const float* o = c.edict ? c.edict->v.origin : vec3_origin;
            // (inv: his health, armour, items, shells, nails, rockets, cells and weapon: the coop campaign's checks, B4)
            const entvars_t* v = c.edict ? &c.edict->v : nullptr;
            // (moves: how the server has him move, as his avatar tells the others: mpavatar::movesByte, B5b; merged: his
            // moves that came in a frame with one of his already, server::mergedMoves, B5d)
            // (run, lost, recovered, undecodable, bytes: a remote client's QVR_CLC_MOVES as the server read them, B7)
            const qvr::netmove::ServerCounts nm = qvr::netmove::serverCounts(i);
            Con_Printf("mp_netmoves: slot=%d run=%d lost=%d recovered=%d undecodable=%d bytes=%lld\n", i, nm.run, nm.lost,
                nm.recovered, nm.undecodable, nm.bytes);
            // (sent, full, delta, cut, bytes: his snapshots, whole or against the one he acknowledged; B7b)
            const qvr::netsnap::ServerCounts ns = qvr::netsnap::serverCounts(i);
            // (B12: his rate, the changed records that waited for it and the snapshots with any)
            // (B12c: the cosmetic fragments' entries' bytes and their records that waited for their budget)
            Con_Printf("mp_netsnap: slot=%d sent=%d full=%d delta=%d cut=%d bytes=%lld seq=%u acked=%u rate=%d waited=%lld "
                       "waitframes=%d cosmetic=%lld cwaited=%lld\n",
                i, ns.sent, ns.full, ns.delta, ns.cut, ns.bytes, ns.seq, ns.acked, ns.rate, ns.waited, ns.waitFrames,
                ns.cosmeticBytes, ns.cosmeticWaited);
            // (then his main hand's gun's magazine: its weapon record's .wi_clip; B8h, rounds loaded by hand)
            const qvr::progs::FieldOffsets& pf = qvr::progs::fields();
            int clip = 0;
            if(c.edict && pf.weaponinst >= 0 && pf.wi_clip >= 0)
            {
                const int ref = qvr::progs::fieldInt(c.edict, pf.weaponinst);
                edict_t* inst = ref > 0 ? reinterpret_cast<edict_t*>(reinterpret_cast<byte*>(sv.qcvm.edicts) + ref) : nullptr;
                clip = inst && !inst->free ? static_cast<int>(qvr::progs::fieldFloatOr(inst, pf.wi_clip, 0.f)) : 0;
            }
            Con_Printf("mp_client: slot=%d name=\"%s\" spawned=%d org=%.1f %.1f %.1f inv=%d,%d,%d,%d,%d,%d,%d,%d,%d moves=%d "
                       "merged=%d\n",
                i, c.name, c.spawned ? 1 : 0, o[0], o[1], o[2], v ? static_cast<int>(v->health) : 0,
                v ? static_cast<int>(v->armorvalue) : 0, v ? static_cast<int>(v->items) : 0,
                v ? static_cast<int>(v->ammo_shells) : 0, v ? static_cast<int>(v->ammo_nails) : 0,
                v ? static_cast<int>(v->ammo_rockets) : 0, v ? static_cast<int>(v->ammo_cells) : 0,
                v ? static_cast<int>(v->weapon) : 0, clip, c.edict ? qvr::mpavatar::movesByte(c.edict, false) : 0,
                qvr::server::mergedMoves(i));
        }
        Con_Printf("mp_server: map=%s spawned=%d time=%.3f\n", sv.name, spawned, sv.qcvm.time); // (time: the game's clock)
    }
    if(!signedOn())
    {
        return;
    }
    // This client's teleport aim, and its own trace straight down from its player (worldtrace::move: the local server's
    // world hosting, else the client's own map: B3d's client-side traces; "none": no trace).
    const qvr::teleport::Status tp = qvr::teleport::status();
    Con_Printf("mp_teleport: aiming=%d valid=%d sent=%d impact=%.1f %.1f %.1f\n", tp.aiming ? 1 : 0, tp.valid ? 1 : 0, tp.sent,
        tp.impact.x, tp.impact.y, tp.impact.z);
    if(cl.viewentity > 0 && cl.viewentity < cl.num_entities)
    {
        const glm::vec3 from = vec(cl_entities[cl.viewentity].origin);
        const za::Optional<trace_t> tr =
            qvr::worldtrace::move(from, glm::vec3{0.f}, glm::vec3{0.f}, from - glm::vec3{0.f, 0.f, 256.f}, MOVE_NOMONSTERS);
        if(tr)
        {
            Con_Printf("mp_trace: hosted=%d frac=%.3f floor=%.1f normal=%.2f\n", sv.active ? 1 : 0, tr->fraction, tr->endpos[2],
                tr->plane.normal[2]);
        }
        else
        {
            Con_Printf("mp_trace: none\n");
        }
    }
    for(int i = 1; i <= cl.maxclients && i < cl.num_entities; i++)
    {
        const entity_t& e = cl_entities[i];
        if(!e.model && !(cl.scores && cl.scores[i - 1].name[0]))
        {
            continue;
        }
        Con_Printf("mp_player: ent=%d self=%d name=\"%s\" inmsg=%d org=%.1f %.1f %.1f\n", i, i == cl.viewentity ? 1 : 0,
            cl.scores ? cl.scores[i - 1].name : "", inMessage(e) ? 1 : 0, e.origin[0], e.origin[1], e.origin[2]);
    }
}

// The player a host's debug command acts on: the client in slot Cmd_Argv(`arg`) (0, or none given: the first), in the
// game and alive; null (said why) otherwise.
edict_t* slotPlayer(const char* command, int arg)
{
    const int slot = Cmd_Argc() > arg ? Q_atoi(Cmd_Argv(arg)) : 0;
    if(!sv.active || slot < 0 || slot >= svs.maxclients || !svs.clients[slot].active || !svs.clients[slot].spawned ||
        !svs.clients[slot].edict)
    {
        Con_Printf("%s: %s\n", command, sv.active ? "no player in that slot" : "not hosting a game");
        return nullptr;
    }
    if(deathmatch.value != 0.f)
    {
        Con_Printf("%s: not in deathmatch\n", command);
        return nullptr;
    }
    return svs.clients[slot].edict;
}

// The level an exit leads to (its QC field .map), "?" without one.
const char* nextMap(edict_t* exit)
{
    const eval_t* v = GetEdictFieldValueByName(exit, "map");
    return v && v->string ? PR_GetString(v->string) : "?";
}

// vr_goto_exit [<slot>]: the player (hosting: the one in that slot) put in the level's exit (its first
// trigger_changelevel's middle), touching it as if he walked in: the intermission, or the next level (Debug > Tests >
// Cheats; the coop campaign's exit, mp_flow).
void gotoExit_f()
{
    edict_t* player = slotPlayer("vr_goto_exit", 1);
    if(!player || player->v.health <= 0.f)
    {
        return;
    }
    const qvr::progs::ServerVm vm;
    edict_t* exit = nullptr;
    for(int i = svs.maxclients + 1; i < qcvm->num_edicts && !exit; i++)
    {
        edict_t* e = EDICT_NUM(i);
        exit = !e->free && !strcmp(PR_GetString(e->v.classname), "trigger_changelevel") ? e : nullptr;
    }
    if(!exit)
    {
        Con_Printf("vr_goto_exit: no exit (trigger_changelevel) in this level\n");
        return;
    }
    for(int k = 0; k < 3; k++)
    {
        player->v.origin[k] = (exit->v.absmin[k] + exit->v.absmax[k]) * 0.5f;
    }
    pr_global_struct->time = qcvm->time;
    SV_LinkEdict(player, true); // (its touch: changelevel_touch)
    Con_Printf("vr_goto_exit: %s in the exit to %s at %.0f %.0f %.0f\n", PR_GetString(player->v.netname),
        nextMap(exit), player->v.origin[0], player->v.origin[1], player->v.origin[2]);
}

// vr_hurt <damage> [<slot>]: the player (hosting: the one in that slot) takes that much damage from the world (QC's
// T_Damage, as a fall's): a real death, not `kill`'s respawn at once (Debug > Tests > Cheats; mp_flow's death).
void hurt_f()
{
    if(Cmd_Argc() < 2)
    {
        Con_Printf("usage: vr_hurt <damage> [<slot>]\n");
        return;
    }
    edict_t* player = slotPlayer("vr_hurt", 2);
    if(!player || player->v.health <= 0.f)
    {
        return;
    }
    const qvr::progs::ServerVm vm;
    const func_t fn = qvr::progs::findFunction("T_Damage");
    if(!fn)
    {
        Con_Printf("vr_hurt: no T_Damage in the game's progs\n");
        return;
    }
    pr_global_struct->time = qcvm->time;
    pr_global_struct->self = EDICT_TO_PROG(player);
    G_INT(OFS_PARM0) = EDICT_TO_PROG(player);
    G_INT(OFS_PARM1) = EDICT_TO_PROG(qcvm->edicts);
    G_INT(OFS_PARM2) = EDICT_TO_PROG(qcvm->edicts);
    G_FLOAT(OFS_PARM3) = static_cast<float>(Q_atof(Cmd_Argv(1)));
    PR_ExecuteProgram(fn);
    Con_Printf("vr_hurt: %s at %.0f health%s\n", PR_GetString(player->v.netname), player->v.health,
        player->v.health <= 0.f ? ", dead" : "");
}

} // namespace

extern "C" void VR_MpTestInit(void)
{
    const int i = COM_CheckParm("-cmdinbox");
    if(i && i + 1 < com_argc)
    {
        q_strlcpy(inbox.path, com_argv[i + 1], sizeof(inbox.path));
        inbox.on = true; // (from its start: the harness makes it empty before the game starts, then writes at once)
    }
    Cmd_AddCommand("vr_mp_mark", mark_f);
    Cmd_AddCommand("vr_mp_status", status_f);
    Cmd_AddCommand("vr_mp_trace", trace_f);
    qvr::netmove::init(); // vr_netmove_selftest
    qvr::predict::init(); // vr_mp_predict_report, vr_mp_predict_probe
    qvr::netsnap::init(); // vr_netsnap_selftest
    qvr::lagcomp::init(); // vr_mp_shoot, the "vr_lagcomp_shot" client command
    Cmd_AddCommand("vr_goto_exit", gotoExit_f);
    Cmd_AddCommand("vr_hurt", hurt_f);
    qvr::mpavatar::init(); // vr_mp_avatars
    qvr::clientprefs::init(); // vr_mp_prefs, the "vr_prefs" client command
}

extern "C" void VR_MpTestFrame(void)
{
    if(inbox.on)
    {
        pollInbox();
    }
    if(!isDedicated)
    {
        watchFrame();
        traceFrame();
    }
}
