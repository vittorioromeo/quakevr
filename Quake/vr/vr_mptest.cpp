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
#include "vr_clientprefs.hpp"
#include "vr_teleport.hpp"
#include "vr_trace.hpp"

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

void status_f()
{
    Con_Printf("mp_status: t=%lld dedicated=%d server=%d clients=%d connected=%d map=%s self=%d\n", wallMs(),
        isDedicated ? 1 : 0, sv.active ? 1 : 0, sv.active ? svs.maxclients : 0, signedOn() ? 1 : 0,
        signedOn() ? cl.mapname : "-", signedOn() ? cl.viewentity : 0);
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
            Con_Printf("mp_client: slot=%d name=\"%s\" spawned=%d org=%.1f %.1f %.1f inv=%d,%d,%d,%d,%d,%d,%d,%d\n", i,
                c.name, c.spawned ? 1 : 0, o[0], o[1], o[2], v ? static_cast<int>(v->health) : 0,
                v ? static_cast<int>(v->armorvalue) : 0, v ? static_cast<int>(v->items) : 0,
                v ? static_cast<int>(v->ammo_shells) : 0, v ? static_cast<int>(v->ammo_nails) : 0,
                v ? static_cast<int>(v->ammo_rockets) : 0, v ? static_cast<int>(v->ammo_cells) : 0,
                v ? static_cast<int>(v->weapon) : 0);
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
    }
}
