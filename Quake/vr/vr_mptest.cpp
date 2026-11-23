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
// - `vr_mp_near <ent>` (Debug > Reports > Network: Drawn Near Me; the sync audit, MP_PLAN.md): the models, lights and
//   sounds this client has round a player (near_f).
// - The lockstep (B14b: runs that repeat bit for bit): a dedicated server with `vr_mp_lockstep N` steps its world only
//   once N clients are in and each one's next move (by number) is read, one move of each a step (vr_fixed_frames 1: a
//   step 1/72 s; sv_random_seed); `vr_mp_lockstep_until S` holds it after step S ("mp_lockstep: held step=S"), for the
//   harness's commands (hashes, spawns) at a known step. Its clients (`vr_mp_lockstep_ahead K`) make at most K moves
//   the server has not acknowledged; their frames wait meanwhile (no time passes), so a command a waiting client runs
//   lands on a known move. `vr_mp_lockstep_status`: "mp_lockstep:", "mp_lockstep_slot:", "mp_lockstep_cl:".

#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_box3d.hpp"
#include "vr_geom.hpp"
#include "vr_mpavatar.hpp"
#include "vr_netmove.hpp"
#include "vr_predict.hpp"
#include "vr_netsnap.hpp"
#include "vr_progs.hpp"
#include "vr_clientprefs.hpp"
#include "vr_moveguard.hpp"
#include "vr_nethud.hpp"
#include "vr_serverbrowser.hpp"
#include "vr_server.hpp"
#include "vr_teleport.hpp"
#include "vr_trace.hpp"
#include "vr_view.hpp"
#include "vr_lagcomp.hpp"
#include "vr_wounds.hpp"

#include "Zancle/Base/Memchr.hpp"
#include "Zancle/Base/Strcmp.hpp"
#include "Zancle/Chrono/Clock.hpp"
#include "Zancle/Chrono/Time.hpp"
#include "Zancle/Concurrency/Thread.hpp"
#include "Zancle/Container/Vector.hpp"

#include <stdio.h>

namespace
{

using qvr::geom::vec;

// The inbox (-cmdinbox): its path and how far it was read.
struct Inbox
{
    char path[MAX_OSPATH]{};
    long offset{0};
    bool on{false};
    bool skipping{false}; // (in a line longer than the read: dropped up to its end, mpreview D8)
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

// The harness's lockstep (vr_mp_lockstep, MP_PLAN B14b). Server: whether this frame steps the world, the steps since
// the map's load (its clock going back: a new map), the step whose hold was printed. Client: frames that waited.
struct Lockstep
{
    bool stepping{true};
    za::U64 step{0};
    double lastTime{0.0};
    za::U64 heldPrinted{~0ull};
    long long waits{0};
    bool greeted[MAX_SCOREBOARD]{}; // (B14e) the server: sent his one datagram since he came in (the next at a step)
};
Lockstep lockstep;

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

// vr_mp_near's sounds: the last ones started on this game (S_StartSoundPitch, sound on), whoever started them.
struct HeardSound
{
    double at{0.0}; // realtime
    int ent{0};
    int chan{0};
    char name[48]{};
    glm::vec3 org{0.f};
};
constexpr int heardMost = 96;
struct Heard
{
    HeardSound ring[heardMost];
    int next{0};
};
Heard heard;

constexpr double restTime = 0.25;  // s still before a move counts as one from rest
constexpr float stillEps = 0.25f;  // units a frame that still counts as still
constexpr float posStep = 32.f;    // units of travel between mp_pos lines

[[nodiscard]] long long wallMs() // epoch milliseconds: compared across processes (za::Clock::now's epoch is unspecified)
{
    return za::Clock::nowUnixNanoseconds() / 1'000'000; // (system_clock's, truncated as duration_cast did)
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
        if(inbox.skipping)
        {
            // (the rest of a too-long line: dropped up to its newline, the lines after it read from the next frame)
            const void* nl = ZA_MEMCHR(buf, '\n', got);
            const size_t drop = nl ? static_cast<size_t>(static_cast<const char*>(nl) - buf) + 1 : got;
            inbox.offset += static_cast<long>(drop);
            inbox.skipping = nl == nullptr;
            fclose(f);
            return;
        }
        size_t end = got;
        while(end > 0 && buf[end - 1] != '\n')
        {
            end--; // (a line still being written waits for the next frame)
        }
        if(end == 0 && got == sizeof(buf) - 1)
        {
            // A line longer than the read would stall the inbox for good: dropped, with a word (mpreview D8).
            Con_Printf("vr_mptest: an inbox line longer than %d bytes dropped\n", static_cast<int>(sizeof(buf) - 1));
            inbox.offset += static_cast<long>(got);
            inbox.skipping = true;
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
        if(named == w.shown && (!named || (sc.frags == w.frags && ZA_STRCMP(sc.name, w.name) == 0)))
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
        if(qvr::vr_mp_watch.value == 0.f)
        {
            // (the harness's lines: it sets vr_mp_watch on every game; a player's console never shows them, mpreview D2)
        }
        else if(now)
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

// (B13) vr_mp_after <seconds> <label>: "mp_after: t=<ms> <label>" once the server's game time has gone that far (the
// soak's window: a counted stretch of the server's frames, whatever their wall-clock pace).
struct After
{
    bool pending{false};
    double at{0.0};
    char label[64]{};
};
After after;

void after_f()
{
    if(Cmd_Argc() < 3 || !sv.active)
    {
        Con_Printf("vr_mp_after <seconds> <label>: a server's\n");
        return;
    }
    after.pending = true;
    after.at = sv.qcvm.time + Q_atof(Cmd_Argv(1));
    q_strlcpy(after.label, Cmd_Argv(2), sizeof(after.label));
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
    Con_Printf("mp_status: t=%lld dedicated=%d server=%d clients=%d connected=%d map=%s self=%d intermission=%d health=%d "
               "menu=%d\n",
        wallMs(), isDedicated ? 1 : 0, sv.active ? 1 : 0, sv.active ? svs.maxclients : 0, signedOn() ? 1 : 0,
        signedOn() ? cl.mapname : "-", signedOn() ? cl.viewentity : 0, signedOn() ? cl.intermission : 0,
        signedOn() ? cl.stats[STAT_HEALTH] : 0, key_dest == key_menu ? 1 : 0);
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
            // (B19: the move buffer's ticks running one, more, none; the moves left waiting after them, summed)
            Con_Printf("mp_netmoves: slot=%d run=%d lost=%d recovered=%d undecodable=%d bytes=%lld tick_one=%d tick_more=%d "
                       "tick_none=%d wait_sum=%lld tick_gap=%d depth=%d depth_max=%d spread_max=%d\n",
                i, nm.run, nm.lost, nm.recovered, nm.undecodable, nm.bytes, nm.ticksOne, nm.ticksMore, nm.ticksNone,
                nm.waitSum, nm.ticksGap, nm.depth, nm.depthMax, nm.spreadMax); // (B20: the gap waits, the depth)
            // (sent, full, delta, cut, bytes: his snapshots, whole or against the one he acknowledged; B7b)
            const qvr::netsnap::ServerCounts ns = qvr::netsnap::serverCounts(i);
            // (B12: his rate, the changed records that waited for it and the snapshots with any)
            // (B12c: the cosmetic fragments' entries' bytes and their records that waited for their budget)
            Con_Printf("mp_netsnap: slot=%d sent=%d full=%d delta=%d cut=%d bytes=%lld seq=%u acked=%u rate=%d waited=%lld "
                       "waitframes=%d cosmetic=%lld cwaited=%lld rwaited=%lld\n",
                i, ns.sent, ns.full, ns.delta, ns.cut, ns.bytes, ns.seq, ns.acked, ns.rate, ns.waited, ns.waitFrames,
                ns.cosmeticBytes, ns.cosmeticWaited, ns.ragWaited); // (ragsync2: the ragdolls' records among them)
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
            // (his own values held to a body's bounds, MP_DESIGN P2-6: vr_moveguard.hpp; an honest player's all 0)
            const qvr::moveguard::Counts g = qvr::moveguard::counts(i);
            Con_Printf("mp_guard: slot=%d moves=%d places=%d speeds=%d walks=%d fists=%d spots=%d rewinds=%d\n", i, g.moves,
                g.places, g.speeds, g.walks, g.fists, g.spots, g.rewinds);
            // (his string commands' budget, VR_ClientCommandBudget: an honest player's dropped 0)
            const qvr::server::CommandCounts cc = qvr::server::commandCounts(i);
            Con_Printf("mp_cmds: slot=%d used=%d dropped=%d least=%d\n", i, cc.used, cc.dropped, cc.least);
        }
        // (own: this game's own player's entity, progs::localPlayer, 0 none; ownslot: clientprefs::ownSlot, -1 none:
        // the host's own client in slot 0 whatever a loaded coop save holds, REVIEW_MP_2026-11-18 finding 3)
        const edict_t* own = qvr::progs::localPlayer();
        Con_Printf("mp_server: map=%s spawned=%d time=%.3f own=%d ownslot=%d\n", sv.name, spawned, sv.qcvm.time,
            own ? static_cast<int>((reinterpret_cast<const byte*>(own) - reinterpret_cast<const byte*>(sv.qcvm.edicts)) /
                      sv.qcvm.edict_size)
                : 0,
            qvr::clientprefs::ownSlot()); // (time: the game's clock)
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

// (B15) vr_mp_world: this client's world as the server's messages built it, for comparing two clients' (a player joining
// a game in progress sees what those there from its start see): each entity after the players' in the last message
// ("mp_went: n= m= f= sk= o="), then the precaches, static entities and light styles ("mp_world: ...").
void world_f()
{
    if(!signedOn())
    {
        Con_Printf("mp_world: not connected\n");
        return;
    }
    int ents = 0;
    for(int i = cl.maxclients + 1; i < cl.num_entities; i++)
    {
        const entity_t& e = cl_entities[i];
        if(!e.model || !inMessage(e))
        {
            continue;
        }
        ents++;
        Con_Printf("mp_went: n=%d m=%s f=%d sk=%d o=%.1f %.1f %.1f\n", i, e.model->name, e.frame, e.skinnum, e.origin[0],
            e.origin[1], e.origin[2]);
    }
    int models = 0, sounds = 0;
    for(int i = 1; i < MAX_MODELS && cl.model_precache[i]; i++)
    {
        models++;
    }
    for(int i = 1; i < MAX_SOUNDS && cl.sound_precache[i]; i++)
    {
        sounds++;
    }
    za::U32 styles = 2166136261u; // (FNV-1a over every light style's string)
    for(int i = 0; i < MAX_LIGHTSTYLES; i++)
    {
        for(const char* p = cl_lightstyle[i].map; *p; ++p)
        {
            styles = (styles ^ static_cast<za::U8>(*p)) * 16777619u;
        }
        styles = (styles ^ 0xffu) * 16777619u;
    }
    Con_Printf("mp_world: ents=%d models=%d sounds=%d statics=%d styles=%08x\n", ents, models, sounds, cl.num_statics,
        static_cast<unsigned>(styles));
}

// (B13) vr_mp_edicts: the server's edicts in use and the most common classnames ("mp_edicts: used=N of M top: class=n
// ..."): what piles up in a long game (mp_soak).
void edicts_f()
{
    if(!sv.active)
    {
        return;
    }
    const qvr::progs::ServerVm vm;
    constexpr int maxKinds = 256;
    struct Kind
    {
        const char* name;
        int n;
    };
    za::Vector<Kind> kinds;
    int used = 0, gibs = 0;
    for(int i = 1; i < qcvm->num_edicts; i++)
    {
        edict_t* e = EDICT_NUM(i);
        if(e->free)
        {
            continue;
        }
        used++;
        gibs += qvr::progs::fieldFloatOr(e, qvr::progs::fields().vr_gib, 0.f) != 0.f ? 1 : 0; // (B13b: deathmatch's cap)
        const char* name = PR_GetString(e->v.classname);
        Kind* k = nullptr;
        for(Kind& c : kinds)
        {
            if(!ZA_STRCMP(c.name, name))
            {
                k = &c;
                break;
            }
        }
        if(k)
        {
            k->n++;
        }
        else if(static_cast<int>(kinds.size()) < maxKinds)
        {
            kinds.pushBack(Kind{name, 1});
        }
    }
    Con_Printf("mp_edicts: used=%d of %d gibs=%d top:", used, qcvm->max_edicts, gibs);
    for(int shown = 0; shown < 12; shown++)
    {
        Kind* best = nullptr;
        for(Kind& c : kinds)
        {
            if(c.n > 0 && (!best || c.n > best->n))
            {
                best = &c;
            }
        }
        if(!best)
        {
            break;
        }
        Con_Printf(" %s=%d", best->name[0] ? best->name : "(none)", best->n);
        best->n = -best->n;
    }
    Con_Printf("\n");
    int bodies = 0, awake = 0, contacts = 0;
    qvr::box3d::profileCounts(bodies, awake, contacts);
    qvr::box3d::AwakeKinds k;
    qvr::box3d::profileAwakeKinds(k);
    Con_Printf("mp_box3d: bodies=%d awake=%d contacts=%d ragdolls_living=%d ragdolls_dead=%d rag_living_awake=%d "
               "rag_dead_awake=%d gibs_awake=%d props_awake=%d other_awake=%d\n",
        bodies, awake, contacts, k.ragdollsLiving, k.ragdollsDead, k.ragLivingAwake, k.ragDeadAwake, k.gibsAwake,
        k.propsAwake, k.otherAwake);
    qvr::box3d::printMemory("mp_box3dmem"); // (B21: the soak's Box3D memory)
}

// vr_mp_near <ent> [radius] [seconds] (Debug > Reports > Network: Drawn Near a Player; MP_PLAN.md, "Sync audit
// 2026-11-21"): what this client draws, lights and hears round player entity `ent` (0: its own), for a scenario to
// compare a player's own view of himself with another client's view of him. Each model drawn within `radius` (96) of
// his entity's origin ("mp_near_ent:": whose -- this client's own view entity, another player's copy, an entity -- the
// model, distance, alpha, effects, skin, frame), each dynamic light ("mp_near_light:") and each sound started in the
// last `seconds` (2) by him or within the radius ("mp_near_snd:"; sound on: not with -nosound), then "mp_near: done".
void near_f()
{
    if(cls.state != ca_connected || cl.viewentity <= 0)
    {
        Con_Printf("mp_near: not connected\nmp_near: done\n");
        return;
    }
    int ent = Cmd_Argc() > 1 ? Q_atoi(Cmd_Argv(1)) : 0;
    ent = ent > 0 ? ent : cl.viewentity;
    const float radius = Cmd_Argc() > 2 ? Q_atof(Cmd_Argv(2)) : 96.f;
    const double seconds = Cmd_Argc() > 3 ? Q_atof(Cmd_Argv(3)) : 2.0;
    if(ent >= cl_max_edicts)
    {
        Con_Printf("mp_near: no entity %d\nmp_near: done\n", ent);
        return;
    }
    const glm::vec3 at = vec(cl_entities[ent].origin);
    Con_Printf("mp_near: ent=%d self=%d at=%.1f %.1f %.1f radius=%.0f\n", ent, ent == cl.viewentity ? 1 : 0, at.x, at.y,
        at.z, radius);
    for(int i = 0; i < cl_numvisedicts; i++)
    {
        const entity_t* e = cl_visedicts[i];
        if(!e || !e->model || e->model->type == mod_brush || e == &cl_entities[0])
        {
            continue;
        }
        const float d = glm::distance(vec(e->origin), at);
        if(d > radius)
        {
            continue;
        }
        char whose[24];
        if(qvr::view::find(e))
        {
            q_strlcpy(whose, "own", sizeof(whose));
        }
        else if(qvr::mpavatar::find(e))
        {
            q_strlcpy(whose, "copy", sizeof(whose));
        }
        else if(e >= cl_entities && e < cl_entities + cl_max_edicts)
        {
            q_snprintf(whose, sizeof(whose), "ent%d", static_cast<int>(e - cl_entities));
        }
        else
        {
            q_strlcpy(whose, "other", sizeof(whose));
        }
        Con_Printf("mp_near_ent: whose=%s model=%s d=%.1f alpha=%d fx=%d skin=%d frame=%d glow=%.2f wound=%d\n", whose,
            COM_SkipPath(e->model->name), d, e->alpha, e->effects, e->skinnum, e->frame, qvr::view::powerupGlow(e),
            qvr::wounds::bloody(e) ? 1 : 0); // (wound: a painted wounds mask on it, syncfix5)
    }
    for(const dlight_t& l : cl_dlights)
    {
        if(l.radius <= 0.f || l.die < cl.time)
        {
            continue;
        }
        const float d = glm::distance(vec(l.origin), at);
        if(d <= radius)
        {
            Con_Printf("mp_near_light: key=%d radius=%.0f d=%.1f color=%.2f %.2f %.2f\n", l.key, l.radius, d, l.color[0],
                l.color[1], l.color[2]);
        }
    }
    for(const HeardSound& h : heard.ring)
    {
        if(!h.name[0] || realtime - h.at > seconds)
        {
            continue;
        }
        const float d = glm::distance(h.org, at);
        if(h.ent == ent || d <= radius)
        {
            Con_Printf("mp_near_snd: name=%s ent=%d chan=%d ago=%.2f d=%.1f\n", h.name, h.ent, h.chan, realtime - h.at, d);
        }
    }
    Con_Printf("mp_near: done\n");
}

// The lockstep's players: the clients in the game that send moves (a bot has no connection; a listen host's own client
// sends none: the lockstep is a dedicated server's).
[[nodiscard]] bool lockstepPlayer(const client_t& c)
{
    return c.active && c.spawned && c.netconnection;
}

// vr_mp_lockstep_status: a server's step, whether it holds, each player's newest move run and whether his next is read;
// a client's moves ahead of the server's acknowledgement and whether its frames wait.
void lockstepStatus_f()
{
    if(sv.active)
    {
        int in = 0;
        for(int i = 0; i < svs.maxclients; i++)
        {
            if(lockstepPlayer(svs.clients[i]))
            {
                in++;
                Con_Printf("mp_lockstep_slot: slot=%d last=%u next=%d\n", i, qvr::netmove::serverLast(i),
                    qvr::netmove::serverHasNext(i) ? 1 : 0);
            }
        }
        Con_Printf("mp_lockstep: step=%llu held=%d in=%d time=%.4f\n", static_cast<unsigned long long>(lockstep.step),
            lockstep.stepping ? 0 : 1, in, sv.qcvm.time);
    }
    if(qvr::netmove::clientUses())
    {
        const za::U32 pending = qvr::netmove::clientPending();
        const float ahead = qvr::vr_mp_lockstep_ahead.value;
        Con_Printf("mp_lockstep_cl: newest=%u pending=%u waiting=%d waits=%lld\n", qvr::netmove::clientNewest(), pending,
            ahead >= 1.f && pending >= static_cast<za::U32>(ahead) ? 1 : 0, lockstep.waits);
    }
}

} // namespace

extern "C" void VR_LockstepFrameBegin(void)
{
    Lockstep& l = lockstep;
    const int want = static_cast<int>(qvr::vr_mp_lockstep.value);
    l.stepping = true;
    if(want <= 0 || !sv.active)
    {
        return;
    }
    if(sv.qcvm.time < l.lastTime)
    {
        l.step = 0; // (a new map)
        l.heldPrinted = ~0ull;
    }
    l.lastTime = sv.qcvm.time;
    int in = 0;
    bool ready = true;
    for(int i = 0; i < svs.maxclients; i++)
    {
        client_t& c = svs.clients[i];
        if(i < MAX_SCOREBOARD && !(c.active && c.spawned))
        {
            l.greeted[i] = false;
        }
        if(lockstepPlayer(c))
        {
            in++;
            // (B14e) his next move read, and every reliable message sent him acknowledged: a step's reliable messages go
            // right after its datagram, so he reads them before the next step's, every run
            ready = ready && qvr::netmove::serverHasNext(i) && c.message.cursize == 0 && NET_CanSendMessage(c.netconnection);
        }
    }
    const za::U64 until = static_cast<za::U64>(q_max(qvr::vr_mp_lockstep_until.value, 0.f));
    if(in >= want && until != 0 && l.step >= until)
    {
        l.stepping = false;
        if(l.heldPrinted != l.step)
        {
            l.heldPrinted = l.step;
            Con_Printf("mp_lockstep: held step=%llu time=%.4f\n", static_cast<unsigned long long>(l.step), sv.qcvm.time);
        }
        return;
    }
    l.stepping = in >= want && ready;
    l.step += l.stepping ? 1 : 0;
}

extern "C" int VR_LockstepHolds(void)
{
    return qvr::vr_mp_lockstep.value >= 1.f && sv.active && !lockstep.stepping ? 1 : 0;
}

extern "C" int VR_LockstepSkipsDatagram(int slot)
{
    if(lockstep.stepping || slot < 0 || slot >= MAX_SCOREBOARD || !lockstepPlayer(svs.clients[slot]))
    {
        return 0;
    }
    if(!lockstep.greeted[slot])
    {
        lockstep.greeted[slot] = true; // (his first: its entities end his signon)
        return 0;
    }
    return 1;
}

namespace
{
// (B14e) A lockstep client in the game: its moves go by the server's acknowledgements.
[[nodiscard]] bool lockstepIn()
{
    return qvr::vr_mp_lockstep_ahead.value >= 1.f && qvr::netmove::clientUses() && cls.signon == SIGNONS;
}
} // namespace

extern "C" int VR_LockstepReadHeld(void)
{
    return lockstepIn() ? 1 : 0;
}

extern "C" int VR_LockstepReadsMore(void)
{
    if(qvr::vr_mp_lockstep_ahead.value < 1.f)
    {
        return 1;
    }
    return lockstepIn() && qvr::netmove::clientPending() < static_cast<za::U32>(qvr::vr_mp_lockstep_ahead.value) ? 0 : 1;
}

extern "C" int VR_LockstepWaits(void)
{
    const float ahead = qvr::vr_mp_lockstep_ahead.value;
    if(ahead < 1.f || (lockstepIn() && qvr::netmove::clientPending() < static_cast<za::U32>(ahead)))
    {
        return 0;
    }
    // (B14e) also until it is in the game: the frames from its start to its first move, as many as the machine's speed
    // made them, moved its hands' and body's eased states (the throw's, the ammo pouch's) apart between runs
    lockstep.waits++;
    za::ThisThread::sleepFor(za::microseconds(200)); // (the server's step and its acknowledgement are a round trip away)
    return 1;
}

extern "C" void VR_MpTestInit(void)
{
    const int i = COM_CheckParm("-cmdinbox");
    if(i && i + 1 < com_argc)
    {
        q_strlcpy(inbox.path, com_argv[i + 1], sizeof(inbox.path));
        inbox.on = true; // (from its start: the harness makes it empty before the game starts, then writes at once)
    }
    Cmd_AddCommand("vr_mp_mark", mark_f);
    Cmd_AddCommand("vr_mp_after", after_f); // (B13)
    Cmd_AddCommand("vr_mp_edicts", edicts_f); // (B13)
    Cmd_AddCommand("vr_mp_status", status_f);
    Cmd_AddCommand("vr_mp_world", world_f); // (B15)
    Cmd_AddCommand("vr_mp_trace", trace_f);
    Cmd_AddCommand("vr_mp_lockstep_status", lockstepStatus_f);
    Cmd_AddCommand("vr_mp_near", near_f); // (the sync audit, 2026-11-21)
    qvr::netmove::init(); // vr_netmove_selftest
    qvr::predict::init(); // vr_mp_predict_report, vr_mp_predict_probe
    qvr::netsnap::init(); // vr_netsnap_selftest
    qvr::lagcomp::init(); // vr_mp_shoot, the "vr_lagcomp_shot" client command
    // (vr_goto_exit, vr_hurt: vr_cheats.cpp, their bots' and attackers' parts for the harness there)
    qvr::mpavatar::init(); // vr_mp_avatars
    qvr::clientprefs::init(); // vr_mp_prefs, the "vr_prefs" client command
    qvr::moveguard::init();   // vr_moveguard_selftest
    qvr::browser::init();     // (B13) vr_mp_browse, vr_mp_favourite
    qvr::nethud::init();      // (B13) vr_net_hud_report
}

extern "C" void VR_MpHeardSound(int entnum, int entchannel, const char* name, const float* origin)
{
    HeardSound& h = heard.ring[heard.next];
    heard.next = (heard.next + 1) % heardMost;
    h.at = realtime;
    h.ent = entnum;
    h.chan = entchannel;
    q_strlcpy(h.name, name ? name : "?", sizeof(h.name));
    h.org = origin ? glm::vec3{origin[0], origin[1], origin[2]} : glm::vec3{0.f};
    if(cl_entities && entnum > 0 && entnum < cl_max_edicts && (!origin || (origin[0] == 0.f && origin[1] == 0.f && origin[2] == 0.f)))
    {
        h.org = vec(cl_entities[entnum].origin); // (a local sound has no place of its own: its entity's)
    }
}

extern "C" void VR_MpTestFrame(void)
{
    if(inbox.on)
    {
        pollInbox();
    }
    if(after.pending && sv.active && sv.qcvm.time >= after.at)
    {
        after.pending = false;
        Con_Printf("mp_after: t=%lld %s sv_time=%.3f\n", wallMs(), after.label, sv.qcvm.time);
    }
    if(!isDedicated)
    {
        watchFrame();
        traceFrame();
    }
}
