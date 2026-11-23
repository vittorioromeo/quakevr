// vr_navfollow.cpp -- the navmesh follower and vr_nav_follow (vr_navfollow.hpp; docs/vr-port/NAVMESH.md, section 22).

#include "vr_navfollow.hpp"

#include "vr_cvars.hpp"
#include "vr_geom.hpp"
#include "vr_hands.hpp"
#include "vr_lines.hpp"
#include "vr_main.hpp"
#include "vr_progs.hpp"
#include "vr_protocol.hpp"
#include "vr_server.hpp"
#include "vr_tunables.hpp"

#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Base/Strcmp.hpp"
#include "Zancle/Base/Strlen.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Remainder.hpp"
#include "Zancle/Math/Sin.hpp"

namespace qvr::navfollow
{
namespace
{

constexpr float reachedUnits = 12.f;   // a corner this near (flat) is reached: on to the next
constexpr float passedSegment = 24.f;  // ... or passed: beyond it along the next leg, this near that leg's line
constexpr float changedUnits = 4.f;    // a plan's corner moved this far: sent again
constexpr float stuckUnits = 8.f;      // walking, moved under this in stuckSeconds: a hop
constexpr double stuckSeconds = 0.5;
constexpr float turnRate = 540.f;      // the view's turn toward the next corner (degrees a second)
constexpr int maxSlots = MAX_SCOREBOARD;

glm::vec3 feetOf(const edict_t* e)
{
    return {e->v.origin[0], e->v.origin[1], e->v.origin[2] + e->v.mins[2]};
}

float flatDist(const glm::vec3& a, const glm::vec3& b)
{
    return glm::length(glm::vec2{a.x - b.x, a.y - b.y});
}

float yawTo(const glm::vec3& from, const glm::vec3& to)
{
    return geom::yawOf(to - from);
}

// ---- Server: who follows whom -----------------------------------------------------------------------------------------

struct Follow
{
    int target = 0; // the followed player's edict number (0: none)
    Plan plan;
    bool dirty = false; // a state to send now (begun, ended)
};
Follow follows[maxSlots];     // seat-shared: the server's, one per player slot
double serverTimeSeen = -1.0; // seat-shared: the server's (a new server's clock starts again: everything off)

// ---- Server: the bots' plans (vr_bot_navmesh, botStep) -----------------------------------------------------------------

constexpr float botStopPlayer = 48.f; // a bot walking to a player stands this far short of him (along the path); an item: on it

struct Bot
{
    int goal = 0; // the goal's edict number its plan is for (0: none)
    Plan plan;
    Steer steer;
    za::U32 serial = 0; // its plan's (a new one: the steering starts again)
    Input last;
    float left = 0.f; // the path left from its feet at its last step
};
// seat-shared: the server's bots (one per player slot)
Bot bots[maxSlots];
double botTimeSeen = -1.0; // (seat-shared: the server's; a new server's clock starts again: every plan dropped)

// ---- Server: the bots' paths to the clients drawing them (vr_nav_debug on: "vr_nav_botpaths 1") ------------------------

struct BotPath
{
    int next = 0;
    int count = 0; // (0: no path)
    nav::PathCorner corners[maxCorners];
};
// seat-shared: the server's, by client slot
BotPath botSent[maxSlots];      // each bot's path as last sent (by its client slot)
// seat-shared: the server's, by client slot
bool botPathsWanted[maxSlots];  // the client slots whose player draws them
// seat-shared: the server's, by client slot
bool botPathsFresh[maxSlots];   // ... and has just asked: every path sent him now

bool samePath(const BotPath& a, const BotPath& b)
{
    if(a.count != b.count || a.next != b.next)
    {
        return false;
    }
    for(int i = 0; i < a.count; ++i)
    {
        if(a.corners[i].link != b.corners[i].link || a.corners[i].at != b.corners[i].at)
        {
            return false;
        }
    }
    return true;
}

void writeBotPath(sizebuf_t* msg, int slot, const BotPath& p)
{
    MSG_WriteByte(msg, protocol::svc_quakevr);
    MSG_WriteByte(msg, protocol::QVR_SVC_BOTPATH);
    MSG_WriteByte(msg, slot);
    MSG_WriteByte(msg, p.next);
    MSG_WriteByte(msg, p.count);
    for(int i = 0; i < p.count; ++i)
    {
        MSG_WriteByte(msg, static_cast<int>(p.corners[i].link));
        for(int k = 0; k < 3; ++k)
        {
            MSG_WriteCoord(msg, p.corners[i].at[k], sv.protocolflags);
        }
    }
}

// ---- Client: the bots' paths as the server sent them ----------------------------------------------------------------------

// seat-shared: a debug view's: the paths drawn (phase 7's per-seat render)
BotPath botPaths[maxSlots];
// seat-shared: with botPaths (debug)
bool botPathsAsked = false; // "vr_nav_botpaths 1" sent this connection
// seat-shared: the debug draw's count
int botLinesDrawn = 0;      // the bots' path lines the last debug draw drew

// The path's length from `feet` on from corner `next`.
float pathLeft(const nav::PathCorner* corners, int count, int next, const glm::vec3& feet)
{
    if(count < 2 || next >= count)
    {
        return 0.f;
    }
    float left = flatDist(feet, corners[next].at);
    for(int i = next; i + 1 < count; ++i)
    {
        left += glm::length(corners[i + 1].at - corners[i].at);
    }
    return left;
}

// ---- Client: what the server sent, the steering, the hand-back ----------------------------------------------------------

struct Client
{
    int state = stateOff;
    bool partial = false;
    int target = 0;
    nav::PathCorner corners[maxCorners];
    int count = 0;
    za::U32 serial = 0;
    Steer steer;
    Input last;
    bool driving = false, paused = false, jump = false, haveLook = false;
    double pausedUntil = 0.0;
    float lookYaw = 0.f, lookPitch = 0.f; // the view as the follower left it (a change since: his own look)
};
Client client;

void writeState(sizebuf_t* msg, const Follow& f)
{
    MSG_WriteByte(msg, protocol::svc_quakevr);
    MSG_WriteByte(msg, protocol::QVR_SVC_NAVFOLLOW);
    MSG_WriteByte(msg, f.target ? f.plan.state : stateOff);
    MSG_WriteByte(msg, f.plan.partial ? 1 : 0);
    MSG_WriteShort(msg, f.target);
    const int n = f.target && f.plan.state == stateOn ? f.plan.count : 0;
    MSG_WriteByte(msg, n);
    for(int i = 0; i < n; ++i)
    {
        MSG_WriteByte(msg, static_cast<int>(f.plan.corners[i].link));
        for(int k = 0; k < 3; ++k)
        {
            MSG_WriteCoord(msg, f.plan.corners[i].at[k], sv.protocolflags);
        }
    }
}

const char* nameOfNum(int num)
{
    const int slot = progs::clientSlotOfNum(num);
    return slot >= 0 ? svs.clients[slot].name : "?";
}

bool playing(int slot)
{
    return slot >= 0 && slot < svs.maxclients && svs.clients[slot].active && svs.clients[slot].spawned && svs.clients[slot].edict;
}

// The player `arg` names, for the client in `self`: a slot as `status` numbers them (1 up), a name (exact, else the
// first it begins), next (the next other player after the one followed, none past the last) -> his slot, -1 none, -2 off.
int resolve(int self, const char* arg)
{
    if(!ZA_STRCMP(arg, "0") || !q_strcasecmp(arg, "off"))
    {
        return -2;
    }
    if(!q_strcasecmp(arg, "next"))
    {
        const int now = follows[self].target ? progs::clientSlotOfNum(follows[self].target) : -1;
        for(int s = now + 1; s < svs.maxclients; ++s)
        {
            if(s != self && playing(s))
            {
                return s;
            }
        }
        return -2;
    }
    char* end = nullptr;
    const long n = strtol(arg, &end, 10);
    if(end && end != arg && *end == '\0')
    {
        return n >= 1 && n <= svs.maxclients && playing(static_cast<int>(n) - 1) ? static_cast<int>(n) - 1 : -1;
    }
    for(int pass = 0; pass < 2; ++pass)
    {
        for(int s = 0; s < svs.maxclients; ++s)
        {
            if(playing(s) && (pass == 0 ? !q_strcasecmp(svs.clients[s].name, arg)
                                        : !q_strncasecmp(svs.clients[s].name, arg, static_cast<int>(ZA_STRLEN(arg)))))
            {
                return s;
            }
        }
    }
    return -1;
}

// "vr_nav_follow_ask <arg>": a client's vr_nav_follow, on the server.
void ask_f()
{
    if(cmd_source != src_client || !sv.active || !host_client || Cmd_Argc() != 2)
    {
        return;
    }
    const int self = static_cast<int>(host_client - svs.clients);
    if(self < 0 || self >= maxSlots)
    {
        return;
    }
    if(deathmatch.value != 0.f)
    {
        SV_ClientPrintf("vr_nav_follow: not in deathmatch\n");
        return;
    }
    const progs::ServerVm vm;
    Follow& f = follows[self];
    const int slot = resolve(self, Cmd_Argv(1));
    if(slot == -1 || slot == self)
    {
        SV_ClientPrintf("vr_nav_follow: no other player \"%s\" (a name, a slot as status numbers them, next, or 0)\n",
            Cmd_Argv(1));
        return;
    }
    f = {};
    f.dirty = true;
    if(slot >= 0)
    {
        f.target = NUM_FOR_EDICT(svs.clients[slot].edict);
        SV_ClientPrintf("vr_nav_follow: following %s (entity %d)\n", svs.clients[slot].name, f.target);
    }
    else
    {
        SV_ClientPrintf("vr_nav_follow: off\n");
    }
}

// "vr_nav_botpaths <0 | 1>": a client draws the bots' navmesh paths (his vr_nav_debug), on the server.
void botPaths_f()
{
    if(cmd_source != src_client || !sv.active || !host_client || Cmd_Argc() != 2)
    {
        return;
    }
    const int self = static_cast<int>(host_client - svs.clients);
    if(self < 0 || self >= maxSlots)
    {
        return;
    }
    botPathsWanted[self] = botPathsFresh[self] = atoi(Cmd_Argv(1)) != 0;
}

// Server: each bot's path sent, when it changed, to the clients drawing them; every one to a client that just asked.
void serverBotPaths()
{
    bool anyone = false;
    for(int v = 0; v < svs.maxclients && v < maxSlots; ++v)
    {
        if(botPathsWanted[v] && !playing(v))
        {
            botPathsWanted[v] = botPathsFresh[v] = false;
        }
        anyone = anyone || botPathsWanted[v];
    }
    if(!anyone)
    {
        return;
    }
    for(int s = 0; s < svs.maxclients && s < maxSlots; ++s)
    {
        const Bot& b = bots[s];
        BotPath now;
        if(server::isBot(s) && b.goal && b.plan.state == stateOn && b.plan.count >= 2)
        {
            now.count = b.plan.count;
            now.next = za::clamp(b.steer.next, 0, b.plan.count - 1);
            for(int i = 0; i < now.count; ++i)
            {
                now.corners[i] = b.plan.corners[i];
            }
        }
        const bool changed = !samePath(now, botSent[s]);
        botSent[s] = now;
        for(int v = 0; v < svs.maxclients && v < maxSlots; ++v)
        {
            if(botPathsWanted[v] && (changed || (botPathsFresh[v] && now.count)))
            {
                writeBotPath(&svs.clients[v].message, s, now);
            }
        }
    }
    for(bool& f : botPathsFresh)
    {
        f = false;
    }
}

// Client: the bots' paths asked of the server while vr_nav_debug is on (and forgotten when off or on a new connection).
void askBotPaths()
{
    const bool connected = cls.state == ca_connected && cls.signon == SIGNONS && !cls.demoplayback;
    const bool want = connected && vr_nav_debug.value != 0.f;
    if(want != botPathsAsked)
    {
        if(connected)
        {
            MSG_WriteByte(&cls.message, clc_stringcmd);
            MSG_WriteString(&cls.message, va("vr_nav_botpaths %d", want ? 1 : 0));
        }
        botPathsAsked = want;
    }
    if(!want)
    {
        for(BotPath& p : botPaths)
        {
            p.count = 0;
        }
    }
}

// ---- Client side --------------------------------------------------------------------------------------------------------

void forward_f()
{
    if(Cmd_Argc() != 2)
    {
        Con_Printf("vr_nav_follow <name | slot | next | 0>: walk after that player along the navmesh (slots as status numbers "
                   "them); your own move or look hands control back, it resumes after vr_nav_follow_idle s\n");
        return;
    }
    if(cls.state != ca_connected || cls.signon != SIGNONS)
    {
        Con_Printf("vr_nav_follow: not in a game\n");
        return;
    }
    MSG_WriteByte(&cls.message, clc_stringcmd);
    MSG_WriteString(&cls.message, va("vr_nav_follow_ask \"%s\"", Cmd_Argv(1)));
}

void status_f()
{
    const Client& c = client;
    const glm::vec3 f = geom::vec(cl_entities[cl.viewentity].origin);
    Con_Printf("navfollow: state=%d target=%d partial=%d corners=%d next=%d driving=%d paused=%d arrived=%d at=%.0f %.0f %.0f\n",
        c.state, c.target, c.partial ? 1 : 0, c.count, c.steer.next, c.driving ? 1 : 0, c.paused ? 1 : 0,
        c.last.arrived ? 1 : 0, f.x, f.y, f.z);
    int paths = 0, corners = 0;
    for(const BotPath& p : botPaths)
    {
        paths += p.count >= 2 ? 1 : 0;
        corners += p.count >= 2 ? p.count : 0;
    }
    Con_Printf("navbots: asked=%d paths=%d corners=%d drawn=%d\n", botPathsAsked ? 1 : 0, paths, corners, botLinesDrawn);
}

const char* stateName(int s)
{
    return s == stateOn ? "following" : s == stateBuilding ? "the navmesh for your size is building" : s == stateNoPath ? "no path (off the navmesh)" : "off";
}

} // namespace

// ---- The follower ---------------------------------------------------------------------------------------------------------

bool replan(Plan& p, const edict_t* self, const edict_t* goal, double now)
{
    if(now < p.repathAt && p.state != stateOff)
    {
        return false;
    }
    p.repathAt = now + static_cast<double>(za::max(vr_nav_follow_repath.value, 0.05f));
    Plan q = p;
    bool partial = false;
    const int n = nav::followPath(self, feetOf(self), feetOf(goal), q.corners, maxCorners, partial);
    q.state = n < 0 ? stateBuilding : n < 2 ? stateNoPath : stateOn;
    q.count = n >= 2 ? n : 0;
    q.partial = partial;
    bool changed = q.state != p.state || q.partial != p.partial || q.count != p.count;
    for(int i = 1; i < q.count && !changed; ++i)
    {
        changed = q.corners[i].link != p.corners[i].link || glm::length(q.corners[i].at - p.corners[i].at) > changedUnits;
    }
    if(changed || q.state != stateOn)
    {
        p = q;
    }
    return changed;
}

Input steer(Steer& s, const nav::PathCorner* corners, int count, za::U32 serial, const glm::vec3& feet, double now, float stop)
{
    Input in;
    if(count < 2)
    {
        s.stuckSince = -1.0;
        in.standing = true;
        return in;
    }
    if(serial != s.serial)
    {
        s.serial = serial;
        s.next = 1; // (the first is where the plan found it)
    }
    // On past the corners reached, or passed (beyond one along its next leg and near that leg: the plan was made where
    // the server had it a round trip ago). A teleporter's or lift's corners are walked to, never cut.
    while(s.next < count - 1)
    {
        const nav::PathCorner& c = corners[s.next];
        if(flatDist(feet, c.at) <= reachedUnits)
        {
            if(c.link == nav::LinkKind::ride && s.next > 0 && corners[s.next - 1].link == nav::LinkKind::ride)
            {
                break; // (on the lift's middle: stands, till a plan from wherever it carries him)
            }
            ++s.next;
            continue;
        }
        const glm::vec2 leg{corners[s.next + 1].at.x - c.at.x, corners[s.next + 1].at.y - c.at.y};
        const glm::vec2 off{feet.x - c.at.x, feet.y - c.at.y};
        const float len = glm::length(leg);
        if(c.link == nav::LinkKind::none && len > 1.f && glm::dot(off, leg) > 0.f &&
            za::abs(off.x * leg.y - off.y * leg.x) / len < passedSegment && za::abs(feet.z - c.at.z) < 48.f)
        {
            ++s.next;
            continue;
        }
        break;
    }
    const nav::PathCorner& to = corners[s.next];
    // How far along the path to its end.
    float left = flatDist(feet, to.at);
    for(int i = s.next; i + 1 < count; ++i)
    {
        left += glm::length(corners[i + 1].at - corners[i].at);
    }
    const bool onLift = to.link == nav::LinkKind::ride && s.next > 0 && corners[s.next - 1].link == nav::LinkKind::ride &&
                        flatDist(feet, to.at) <= reachedUnits;
    in.arrived = left <= stop || (s.next == count - 1 && flatDist(feet, to.at) <= reachedUnits);
    s.arrived = in.arrived;
    in.yaw = yawTo(feet, in.arrived ? corners[count - 1].at : to.at);
    if(in.arrived && flatDist(feet, corners[count - 1].at) < 1.f)
    {
        in.yaw = yawTo(feet, to.at);
    }
    in.move = !in.arrived && !onLift;
    in.standing = !in.move;
    // Stuck while walking (a lip, a corner's edge, a closed door opening): a hop, as a human would.
    if(!in.move)
    {
        s.stuckSince = -1.0;
    }
    else if(s.stuckSince < 0.0 || glm::length(feet - s.stuckAt) > stuckUnits)
    {
        s.stuckAt = feet;
        s.stuckSince = now;
    }
    else if(now - s.stuckSince > stuckSeconds)
    {
        in.jump = true;
        s.stuckAt = feet;
        s.stuckSince = now;
    }
    return in;
}

// ---- Bots ---------------------------------------------------------------------------------------------------------------

glm::vec3 botStep(const edict_t* bot, const edict_t* goal, int what)
{
    const glm::vec3 feet = feetOf(bot);
    const int slot = NUM_FOR_EDICT(const_cast<edict_t*>(bot)) - 1;
    if(slot < 0 || slot >= svs.maxclients || slot >= maxSlots)
    {
        return what == 0 ? feet : glm::vec3{0.f};
    }
    const double now = qcvm->time;
    if(now < botTimeSeen)
    {
        for(Bot& b : bots)
        {
            b = {};
        }
    }
    botTimeSeen = now;
    Bot& b = bots[slot];
    if(what == 1)
    {
        const int flags = (b.last.move ? botWalk : 0) | (b.last.jump ? botJump : 0) | (b.last.arrived ? botArrived : 0) |
                          (b.plan.partial ? botPartial : 0);
        return {static_cast<float>(b.plan.state), static_cast<float>(flags), b.left};
    }
    if(what == 2)
    {
        b = {};
        return glm::vec3{0.f};
    }
    const int goalNum = NUM_FOR_EDICT(const_cast<edict_t*>(goal));
    if(what == 3)
    {
        nav::PathCorner corners[maxCorners];
        bool partial = false;
        const glm::vec3 to = feetOf(goal);
        const int n = goalNum ? nav::followPath(bot, feet, to, corners, maxCorners, partial) : 0;
        if(n < 2)
        {
            return {n < 0 ? -1.f : 0.f, 0.f, 0.f};
        }
        // (a path of maxCorners may be cut short: its length then a lower bound, on in a line to the goal)
        const float len = pathLeft(corners, n, 0, feet) + (n == maxCorners ? glm::length(to - corners[n - 1].at) : 0.f);
        return {partial ? 0.f : 1.f, len, static_cast<float>(n)};
    }
    if(!goalNum)
    {
        b = {};
        return feet;
    }
    if(goalNum != b.goal)
    {
        b = {};
        b.goal = goalNum;
    }
    if(replan(b.plan, bot, goal, now))
    {
        ++b.serial;
    }
    if(b.plan.state != stateOn)
    {
        b.last = {};
        b.left = 0.f;
        return feet;
    }
    const float stop = (static_cast<int>(goal->v.flags) & FL_CLIENT) ? botStopPlayer : 0.f;
    b.last = steer(b.steer, b.plan.corners, b.plan.count, b.serial, feet, now, stop);
    b.left = pathLeft(b.plan.corners, b.plan.count, b.steer.next, feet);
    return b.last.arrived ? feetOf(goal) : b.plan.corners[b.steer.next].at;
}

// ---- vr_nav_follow ----------------------------------------------------------------------------------------------------

void init()
{
    Cmd_AddCommand("vr_nav_follow", forward_f);
    Cmd_AddCommand_ClientCommand("vr_nav_follow_ask", ask_f);
    Cmd_AddCommand("vr_nav_follow_status", status_f);
    Cmd_AddCommand_ClientCommand("vr_nav_botpaths", botPaths_f);
}

void serverFrame()
{
    if(!sv.active)
    {
        return;
    }
    const double now = sv.qcvm.time;
    if(now < serverTimeSeen)
    {
        for(Follow& f : follows)
        {
            f = {};
        }
        for(BotPath& p : botSent)
        {
            p = {};
        }
    }
    serverTimeSeen = now;
    const progs::ServerVm vm;
    for(int s = 0; s < svs.maxclients && s < maxSlots; ++s)
    {
        Follow& f = follows[s];
        if(!f.target && !f.dirty)
        {
            continue;
        }
        if(!playing(s))
        {
            f = {};
            continue;
        }
        sizebuf_t* msg = &svs.clients[s].message;
        const int ts = progs::clientSlotOfNum(f.target);
        if(f.target && !playing(ts))
        {
            MSG_WriteByte(msg, svc_print);
            MSG_WriteString(msg, va("vr_nav_follow: %s left: off\n", nameOfNum(f.target)));
            f = {};
            f.dirty = true;
        }
        bool send = f.dirty;
        if(f.target)
        {
            send = replan(f.plan, svs.clients[s].edict, svs.clients[ts].edict, now) || send;
        }
        if(send)
        {
            writeState(msg, f);
            f.dirty = false;
        }
    }
    serverBotPaths();
}

void parseBotPath()
{
    const int slot = MSG_ReadByte();
    const int next = MSG_ReadByte();
    const int n = MSG_ReadByte();
    BotPath p;
    for(int i = 0; i < n; ++i)
    {
        const int link = MSG_ReadByte();
        glm::vec3 at;
        for(int k = 0; k < 3; ++k)
        {
            at[k] = MSG_ReadCoord(cl.protocolflags);
        }
        if(p.count < maxCorners && link >= 0 && link <= static_cast<int>(nav::LinkKind::tele))
        {
            p.corners[p.count++] = {at, static_cast<nav::LinkKind>(link)};
        }
    }
    if(msg_badread || slot < 0 || slot >= maxSlots)
    {
        return;
    }
    p.next = za::clamp(next, 0, za::max(p.count - 1, 0));
    botPaths[slot] = p;
}

void parse()
{
    Client& c = client;
    const int state = MSG_ReadByte();
    const int flags = MSG_ReadByte();
    const int target = MSG_ReadShort();
    const int n = MSG_ReadByte();
    int kept = 0;
    for(int i = 0; i < n; ++i)
    {
        const int link = MSG_ReadByte();
        glm::vec3 at;
        for(int k = 0; k < 3; ++k)
        {
            at[k] = MSG_ReadCoord(cl.protocolflags);
        }
        if(kept < maxCorners && link >= 0 && link <= static_cast<int>(nav::LinkKind::tele))
        {
            c.corners[kept++] = {at, static_cast<nav::LinkKind>(link)};
        }
    }
    if(msg_badread)
    {
        return;
    }
    if(state != c.state)
    {
        Con_Printf("vr_nav_follow: %s\n", stateName(state));
    }
    c.state = state >= stateOff && state <= stateNoPath ? state : stateOff;
    c.partial = (flags & 1) != 0;
    c.target = target;
    c.count = c.state == stateOn ? kept : 0;
    ++c.serial;
    if(c.state == stateOff)
    {
        c.driving = c.paused = c.jump = false;
        c.last = {};
    }
}

bool clientMove(float* forwardmove, float* sidemove, float* upmove, bool stick)
{
    Client& c = client;
    c.jump = false;
    if(cls.state != ca_connected || cls.signon != SIGNONS)
    {
        if(c.state != stateOff)
        {
            c = {};
        }
        return false;
    }
    if(c.state == stateOff || cl.intermission)
    {
        c.driving = false;
        return false;
    }
    const bool headset = vrActive();
    const double now = cl.time;
    // His own input: a move key or stick, the jump, or (flat) a look since the follower turned the view.
    const bool looked = !headset && c.haveLook &&
                        (za::abs(za::remainder(cl.viewangles[YAW] - c.lookYaw, 360.f)) > 0.5f ||
                            za::abs(cl.viewangles[PITCH] - c.lookPitch) > 0.5f);
    const bool own = *forwardmove != 0.f || *sidemove != 0.f || *upmove != 0.f || stick || (CL_SeatInput()->jump.state & 1) || looked;
    c.lookYaw = cl.viewangles[YAW];
    c.lookPitch = cl.viewangles[PITCH];
    c.haveLook = true;
    if(own)
    {
        if(!c.paused)
        {
            Con_Printf("vr_nav_follow: yours (your own input); it walks on after %.1f s without any\n",
                static_cast<double>(vr_nav_follow_idle.value));
        }
        c.paused = true;
        c.driving = false;
        c.pausedUntil = now + static_cast<double>(za::max(vr_nav_follow_idle.value, 0.f));
        return false;
    }
    if(c.paused && now < c.pausedUntil)
    {
        return false;
    }
    if(c.paused)
    {
        Con_Printf("vr_nav_follow: walking on\n");
        c.paused = false;
    }
    const glm::vec3 org = geom::vec(cl_entities[cl.viewentity].origin);
    const glm::vec3 feet{org.x, org.y, org.z - 24.f};
    c.last = steer(c.steer, c.corners, c.count, c.serial, feet, now, za::max(vr_nav_follow_stop.value, 0.f));
    c.driving = true;
    // Turn toward it as a human would, no faster than turnRate: the play space (a headset), else the view's yaw.
    const hands::State& hs = hands::current();
    const float ref = headset && hs.valid ? hs.headAngles.y : cl.viewangles[YAW];
    const float dt = za::clamp(static_cast<float>(host_frametime), 0.f, 0.1f);
    const float delta = za::remainder(c.last.yaw - ref, 360.f);
    const float turn = za::clamp(delta, -turnRate * dt, turnRate * dt);
    if(c.count >= 2)
    {
        if(headset)
        {
            hands::addTurn(turn);
        }
        else
        {
            cl.viewangles[YAW] = za::remainder(cl.viewangles[YAW] + turn, 360.f);
            c.lookYaw = cl.viewangles[YAW];
        }
    }
    // Forward along the wanted way, at his walking pace (the run key's, if he always runs), in the move's own frame: the
    // view's yaw as it is sent (flat: after this frame's turn; a headset's play space turns from the next frame).
    *forwardmove = *sidemove = *upmove = 0.f;
    if(c.last.move)
    {
        const float speed = cl_forwardspeed.value * (cl_alwaysrun.value != 0.f ? cl_movespeedkey.value : 1.f);
        const float rad = glm::radians(headset ? delta : delta - turn);
        *forwardmove = za::cos(rad) * speed;
        *sidemove = -za::sin(rad) * speed;
    }
    c.jump = c.last.jump;
    return true;
}

bool jumpNow()
{
    return client.jump;
}

void debugDraw()
{
    askBotPaths();
    botLinesDrawn = 0;
    if(vr_nav_debug.value == 0.f)
    {
        return;
    }
    const glm::vec3 up{0.f, 0.f, 6.f};
    const glm::vec4 yellow{1.f, 0.9f, 0.1f, 1.f}, cyan{0.1f, 0.9f, 0.95f, 1.f}, white{1.f, 1.f, 1.f, 1.f};
    const glm::vec4 magenta{1.f, 0.2f, 0.9f, 1.f};
    // Each bot's navmesh path (vr_bot_navmesh), as the server sent it: magenta, a teleporter's jump cyan, its next corner a
    // white post.
    for(const BotPath& p : botPaths)
    {
        if(p.count < 2)
        {
            continue;
        }
        for(int i = 0; i + 1 < p.count; ++i)
        {
            const bool jump = p.corners[i].link == nav::LinkKind::tele && i > 0 && p.corners[i - 1].link == nav::LinkKind::tele;
            lines::sceneLine(p.corners[i].at + up, p.corners[i + 1].at + up, 2.f, jump ? cyan : magenta, jump ? cyan : magenta);
            ++botLinesDrawn;
        }
        const glm::vec3 n = p.corners[p.next].at + up;
        lines::sceneLine(n, n + glm::vec3{0.f, 0.f, 24.f}, 3.f, white, white);
    }
    const Client& c = client;
    if(c.state != stateOn || c.count < 2)
    {
        return;
    }
    for(int i = 0; i + 1 < c.count; ++i)
    {
        // (a teleporter's trigger middle to its destination: the jump, cyan)
        const bool jump = c.corners[i].link == nav::LinkKind::tele && i > 0 && c.corners[i - 1].link == nav::LinkKind::tele;
        lines::sceneLine(c.corners[i].at + up, c.corners[i + 1].at + up, 2.f, jump ? cyan : yellow, jump ? cyan : yellow);
    }
    const glm::vec3 n = c.corners[za::clamp(c.steer.next, 0, c.count - 1)].at + up;
    lines::sceneLine(n, n + glm::vec3{0.f, 0.f, 24.f}, 3.f, white, white);
}

} // namespace qvr::navfollow
