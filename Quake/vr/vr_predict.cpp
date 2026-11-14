// vr_predict.cpp -- a remote client's own walk predicted (vr_predict.hpp; MP_PLAN.md, B8).

#include "vr_predict.hpp"
#include "vr_api.h"
#include "vr_cvars.hpp"
#include "vr_hull.hpp"
#include "vr_netsnap.hpp"
#include "vr_protocol.hpp"

#include "Zancle/Math/Clamp.hpp"

#include <math.h>
#include <string.h>

extern "C" void VR_PortalClientCross(edict_t* ent); // vr_portals_gates.cpp

namespace qvr::predict
{
namespace
{

constexpr int maxQueued = 32;         // moves a server frame runs at most (a burst after a stall: the oldest dropped)
constexpr float moveMost = 0.1f;      // a move's time at most, seconds
constexpr float creditMost = 0.25f;   // the server's time a client's moves may use up ahead of its frames, seconds
constexpr float creditRate = 1.05f;   // ... earned at this much a second of the server's (a clock a little fast still fits)
constexpr int stateKey = netsnap::predictKey; // the snapshot record's key (after the entities and the avatars)
constexpr int pastRing = 64;          // predictions kept by move number (netmove::ring)
constexpr float snapAt = 64.f;        // a correction this long is a teleport: drawn at once, not eased
constexpr float easeTime = 0.1f;      // a correction's offset decays by e in this long

enum StateBits
{
    bitWalked = 1,      // this frame's walk ran move by move (else: not predicted)
    bitOnGround = 2,
    bitWaterJump = 4,
    bitSlopeWalk = 8,
    bitNoStep = 16,
    bitGravity = 32,
    bitWading = 64,
    bitRoomscale = 128,
    bitHullBrushModels = 256, // the narrowed box meets brush models too (vr_hull_brushmodels)
    bitEdgeProbe = 512,       // the ledge test from the narrowed box's edge (vr_hull_edge_probe)
};

// ---------------------------------------------------------------------------------------------------------------------
// Server

struct ServerSlot
{
    pmove::Input queue[maxQueued];
    int n{0};
    float credit{creditMost};
    bool plain{false};  // this frame's walk runs move by move (VR_PredictThink)
    bool walked{false}; // ... and it did (VR_PredictPhysics: no hanging, climbing or teleport took the frame)
    pmove::Rules rules;
    vec3_t groundNormal{0.f, 0.f, 1.f};
    float groundAge{1e6f};
};
ServerSlot serverSlots[MAX_SCOREBOARD];

[[nodiscard]] ServerSlot* slotOfEdict(const edict_t* ent)
{
    if(!sv.active || !ent)
    {
        return nullptr;
    }
    const int num = NUM_FOR_EDICT(const_cast<edict_t*>(ent)) - 1;
    return num >= 0 && num < svs.maxclients && num < MAX_SCOREBOARD ? &serverSlots[num] : nullptr;
}

// A client whose moves can run one by one: a remote one (QVR_CLC_MOVES).
[[nodiscard]] bool movesOneByOne(int slot)
{
    return vr_net_permove.value != 0.f && slot >= 0 && slot < MAX_SCOREBOARD && netmove::serverSends(slot);
}

// The player's edict as a move sees him: the module's state copied in and out around every hook that runs QuakeC or
// the VR code (touches, props, the slope's record), as SV_FlyMove and SV_WalkMove keep it in the edict.
struct EdictBody final : pmove::Body
{
    edict_t* ent;

    explicit EdictBody(edict_t* e) : ent{e} { load(); }

    void load()
    {
        VectorCopy(ent->v.origin, origin);
        VectorCopy(ent->v.velocity, velocity);
        flags = static_cast<int>(ent->v.flags);
    }
    void store()
    {
        VectorCopy(origin, ent->v.origin);
        VectorCopy(velocity, ent->v.velocity);
        ent->v.flags = static_cast<float>(flags);
    }

    trace_t trace(const float* start, const float* end) override
    {
        store();
        return SV_Move(const_cast<float*>(start), ent->v.mins, ent->v.maxs, const_cast<float*>(end), MOVE_NORMAL, ent);
    }
    bool metBsp(const trace_t& tr) override { return tr.ent && static_cast<int>(tr.ent->v.solid) == SOLID_BSP; }
    bool overDropoff(const float* vel, float speed) override
    {
        store();
        return VR_HullOverDropoff(ent, origin, vel, speed) != 0;
    }
    bool standsOn(const trace_t& tr) override
    {
        store();
        return tr.ent && VR_StandsOn(ent, tr.ent, tr.plane.normal) != 0;
    }
    void floorMet(const trace_t& tr) override
    {
        if(tr.ent)
        {
            ent->v.groundentity = EDICT_TO_PROG(tr.ent);
        }
        VR_GroundPlaneMet(ent, tr.plane.normal);
    }
    void bumped(const trace_t& tr) override
    {
        if(tr.ent)
        {
            store();
            VR_PlayerBumps(ent, tr.ent, tr.plane.normal);
        }
    }
    void impact(const trace_t& tr) override
    {
        store();
        VR_FindRadius_Dirty(ent);
        if(tr.ent)
        {
            SV_Impact(ent, tr.ent);
        }
        after();
    }
    void pushed(const trace_t& tr) override
    {
        store();
        SV_LinkEdict(ent, true);
        if(tr.ent && !ent->free && !tr.ent->free)
        {
            SV_Impact(ent, tr.ent);
        }
        after();
    }
    void steppedOnto(const trace_t& tr) override { VR_GroundPlaneMet(ent, tr.plane.normal); }
    void closeQuarters(float* wishvel) override
    {
        store();
        VR_CloseQuartersMove(ent, wishvel);
    }
    bool backHeld() override { return qcvm->time < ent->v.teleport_time; }

    // After QuakeC ran: removed, or its changes taken.
    void after()
    {
        if(ent->free)
        {
            removed = true;
        }
        else
        {
            load();
        }
    }
};

void computeRules(edict_t* ent, pmove::Rules& r)
{
    float maxspeed = sv_maxspeed.value * VR_StaminaSpeedScale(ent) * VR_CarryMoveScale(ent) * VR_HeavyMoveScale(ent);
    r.maxspeed = VR_CrouchSpeedMax(ent, maxspeed); // (once a server frame: its easing's step, as SV_AirMove)
    r.stickScale = VR_WaterStickScale(ent, false) * VR_CarryMoveScale(ent);
    r.accelerate = sv_accelerate.value;
    r.friction = sv_friction.value;
    r.stopspeed = sv_stopspeed.value;
    r.edgefriction = sv_edgefriction.value;
    const eval_t* g = GetEdictFieldValueByName(ent, "gravity");
    r.gravity = (g && g->_float ? g->_float : 1.f) * sv_gravity.value;
    r.maxvelocity = sv_maxvelocity.value;
    r.stepsize = VR_StepSize(18.f);
    const double speedup = VR_PlayerMoveSpeedup();
    r.roomscaleScale = speedup > 0.0 ? static_cast<float>(1.0 / speedup) : 1.f;
    r.slopeWalk = vr_slope_walk.value != 0.f;
    r.nostep = sv_nostep.value != 0.f;
    r.roomscaleOn = hull::heightHold(ent) == 0;
}

// ---------------------------------------------------------------------------------------------------------------------
// Client

struct Past
{
    za::U32 seq{0};
    vec3_t org{0.f, 0.f, 0.f};
};

struct ClientPredict
{
    // The server's newest state (QVR_SVC_PREDICT).
    bool have{false};
    za::U32 seq{0};
    int bits{0};
    vec3_t origin{}, velocity{}, groundNormal{0.f, 0.f, 1.f};
    float groundAge{1e6f};
    pmove::Rules rules;
    float hullWidth{0.f}; // the server's vr_hull_width (0: Quake's hull 1)
    bool fresh{false};    // a new state since the last replay

    // The replay.
    int frame{-1};               // host_framecount of the last one
    Past past[pastRing], cur[pastRing];
    vec3_t error{0.f, 0.f, 0.f}; // the eased correction drawn on top
    vec3_t drawn{0.f, 0.f, 0.f};
    bool drawing{false};
    za::U32 lastNewest{0};       // the newest move sent at the last frame ...
    double lastNewTime{0.0};     // ... when it was first seen (realtime), and its frame's time
    float lastDt{0.f};
    double lastRelink{0.0};      // realtime of the last frame drawn (the offset's decay)
    int ridden{0}; // the brush entity he stands on (drawn at its newest place)

    // vr_mp_predict_report.
    int states{0}, frames{0}, unpredicted{0};
    long long depthSum{0};
    int corrN{0}, flatN{0};
    float corrMax{0.f}, corrSum{0.f}, flatMax{0.f};

    // vr_mp_predict_probe.
    bool probing{false};
    char probeLabel[32]{};
    int probeFrame{0};
    za::U32 probeMove{0}; // the newest move sent when armed
    double probeTime{0.0};
    vec3_t probeOrg{0.f, 0.f, 0.f};
};
ClientPredict cp;

// The client's world as a move sees it: the map's hull 1 and the brush models (doors, lifts) at their newest places.
// (The narrowed box, the server's vr_hull_width, through vr_hull.cpp's brushes rebuilt from this map: as SV_Move meets it.)
struct ClientBody final : pmove::Body
{
    int hit{0};    // the brush entity the last trace met (0: the world or none)
    int ground{0}; // ... the floor last met

    trace_t trace(const float* start, const float* end) override
    {
        trace_t best;
        memset(&best, 0, sizeof(best));
        best.fraction = 1.f;
        best.allsolid = true;
        VectorCopy(end, best.endpos);
        if(!hull::clientPlayerTrace(cl.worldmodel, cl.worldmodel, cp.hullWidth, start, end, best))
        {
            hull_t* hull = &cl.worldmodel->hulls[1];
            SV_RecursiveHullCheck(hull, hull->firstclipnode, 0.f, 1.f, const_cast<float*>(start), const_cast<float*>(end), &best);
        }
        hit = 0;
        for(int k = 1; k < cl.num_entities; k++)
        {
            const entity_t& e = cl_entities[k];
            if(!e.model || e.model->type != mod_brush || e.model->name[0] != '*' || e.msgtime != cl.mtime[0] ||
                e.msg_angles[0][0] != 0.f || e.msg_angles[0][1] != 0.f || e.msg_angles[0][2] != 0.f)
            {
                continue;
            }
            const bool narrow = (cp.bits & bitHullBrushModels) != 0;
            vec3_t s, t;
            VectorSubtract(start, e.msg_origins[0], s);
            VectorSubtract(end, e.msg_origins[0], t);
            trace_t tr;
            memset(&tr, 0, sizeof(tr));
            tr.fraction = 1.f;
            tr.allsolid = true;
            VectorCopy(end, tr.endpos);
            if(!narrow || !hull::clientPlayerTrace(cl.worldmodel, e.model, cp.hullWidth, s, t, tr))
            {
                hull_t* h = &e.model->hulls[1];
                SV_RecursiveHullCheck(h, h->firstclipnode, 0.f, 1.f, s, t, &tr);
            }
            if(tr.fraction != 1.f)
            {
                VectorAdd(tr.endpos, e.msg_origins[0], tr.endpos);
            }
            // (SV_ClipToLinks' choice)
            if(tr.allsolid || tr.startsolid || tr.fraction < best.fraction)
            {
                const bool wasStuck = best.startsolid;
                best = tr;
                best.startsolid = best.startsolid || wasStuck;
                hit = k;
            }
            else if(tr.startsolid)
            {
                best.startsolid = true;
            }
        }
        return best;
    }
    bool metBsp(const trace_t&) override { return true; }
    bool overDropoff(const float* vel, float speed) override
    {
        // (VR_HullOverDropoff: a point at the box's leading edge, its feet, finds no floor within 34 units; with
        // vr_hull_edge_probe the narrowed box's edge, and a point in solid has floor)
        const bool fix = (cp.bits & bitEdgeProbe) != 0;
        const float edge = fix && cp.hullWidth > 0.f ? q_min(16.f, za::clamp(cp.hullWidth, 8.f, 32.f) * 0.5f) : 16.f;
        vec3_t start, stop;
        start[0] = stop[0] = origin[0] + vel[0] / speed * edge;
        start[1] = stop[1] = origin[1] + vel[1] / speed * edge;
        start[2] = origin[2] - 24.f;
        stop[2] = start[2] - 34.f;
        trace_t tr;
        memset(&tr, 0, sizeof(tr));
        tr.fraction = 1.f;
        tr.allsolid = true;
        VectorCopy(stop, tr.endpos);
        hull_t* hull = &cl.worldmodel->hulls[0];
        SV_RecursiveHullCheck(hull, hull->firstclipnode, 0.f, 1.f, start, stop, &tr);
        return tr.fraction == 1.f && !(fix && tr.startsolid);
    }
    void floorMet(const trace_t&) override { ground = hit; }
    void steppedOnto(const trace_t&) override { ground = hit; }
};

[[nodiscard]] bool clientActive()
{
    return vr_net_predict.value != 0.f && cp.have && (cp.bits & bitWalked) && netmove::clientUses() &&
           cls.signon == SIGNONS && !cls.demoplayback && cl.worldmodel && cl.viewentity > 0 &&
           cl.viewentity < cl.num_entities;
}

[[nodiscard]] float dist(const float* a, const float* b)
{
    vec3_t d;
    VectorSubtract(a, b, d);
    return VectorLength(d);
}

// The moves after the server's state replayed: the predicted origin (false: the moves are gone from the ring).
bool replay(vec3_t out, vec3_t before)
{
    ClientBody b;
    VectorCopy(cp.origin, b.origin);
    VectorCopy(cp.velocity, b.velocity);
    b.flags = (cp.bits & bitOnGround ? FL_ONGROUND : 0) | (cp.bits & bitWaterJump ? FL_WATERJUMP : 0);
    VectorCopy(cp.groundNormal, b.groundNormal);
    b.groundAge = cp.groundAge;
    const za::U32 newest = netmove::clientNewest();
    if(newest < cp.seq || newest - cp.seq >= static_cast<za::U32>(pastRing))
    {
        return false;
    }
    pmove::MoveNotes notes;
    netmove::WireMove m;
    int depth = 0;
    VectorCopy(b.origin, before);
    for(za::U32 s = cp.seq + 1; s <= newest; s++)
    {
        if(!netmove::clientMoveAt(s, m))
        {
            return false;
        }
        VectorCopy(b.origin, before);
        pmove::playerMove(b, inputOf(m), cp.rules, &notes);
        Past& p = cp.cur[s % pastRing];
        p.seq = s;
        VectorCopy(b.origin, p.org);
        depth++;
    }
    VectorCopy(b.origin, out);
    cp.depthSum += depth;
    cp.ridden = (b.flags & FL_ONGROUND) ? b.ground : 0;

    if(cp.fresh)
    {
        // The correction: the newest move both this replay and the last one predicted, where each put him.
        for(za::U32 s = newest; s > cp.seq; s--)
        {
            const Past& was = cp.past[s % pastRing];
            const Past& now = cp.cur[s % pastRing];
            if(was.seq != s || now.seq != s)
            {
                continue;
            }
            const float c = dist(was.org, now.org);
            if(c > 1.f && vr_mp_watch.value != 0.f)
            {
                Con_Printf("mp_predict_corr: c=%.2f s=%u seq=%u newest=%u bits=%d was=%.2f %.2f %.2f now=%.2f %.2f %.2f sv=%.2f %.2f %.2f vel=%.1f %.1f %.1f notes=%d%d%d\n", c, s, cp.seq, newest, cp.bits, was.org[0], was.org[1], was.org[2], now.org[0], now.org[1], now.org[2], cp.origin[0], cp.origin[1], cp.origin[2], cp.velocity[0], cp.velocity[1], cp.velocity[2], notes.wall ? 1 : 0, notes.stepped ? 1 : 0, notes.air ? 1 : 0);
            }
            cp.corrN++;
            cp.corrSum += c;
            cp.corrMax = q_max(cp.corrMax, c);
            const bool flat = !notes.wall && !notes.stepped && !notes.air && !notes.slope && (cp.bits & bitOnGround);
            if(flat)
            {
                cp.flatN++;
                cp.flatMax = q_max(cp.flatMax, c);
            }
            if(c >= snapAt)
            {
                VectorCopy(vec3_origin, cp.error);
            }
            else
            {
                for(int i = 0; i < 3; i++)
                {
                    cp.error[i] += was.org[i] - now.org[i];
                }
            }
            break;
        }
        cp.fresh = false;
    }
    memcpy(cp.past, cp.cur, sizeof(cp.past));
    return true;
}

void probeCheck(const float* origin)
{
    if(!cp.probing || dist(origin, cp.probeOrg) <= 0.05f)
    {
        return;
    }
    cp.probing = false;
    Con_Printf("mp_predict_moved: label=%s moves=%u frames=%d ms=%d predicted=%d\n", cp.probeLabel, netmove::clientNewest() - cp.probeMove, host_framecount - cp.probeFrame,
        static_cast<int>((realtime - cp.probeTime) * 1000.0), cp.drawing ? 1 : 0);
}

void probe_f()
{
    q_strlcpy(cp.probeLabel, Cmd_Argc() > 1 ? Cmd_Argv(1) : "-", sizeof(cp.probeLabel));
    if(cls.signon != SIGNONS || cl.viewentity <= 0 || cl.viewentity >= cl.num_entities)
    {
        Con_Printf("vr_mp_predict_probe: not in a game\n");
        return;
    }
    cp.probing = true;
    cp.probeFrame = host_framecount;
    cp.probeMove = netmove::clientNewest();
    cp.probeTime = realtime;
    VectorCopy(cl_entities[cl.viewentity].origin, cp.probeOrg);
}

} // namespace

pmove::Input inputOf(const netmove::WireMove& m)
{
    pmove::Input in;
    in.dt = m.frameTime > 0.f ? (m.frameTime < moveMost ? m.frameTime : moveMost) : 0.f;
    in.forward = static_cast<float>(m.forward);
    in.side = static_cast<float>(m.side);
    for(int i = 0; i < 3; i++)
    {
        in.steer[i] = m.vr.headAngles[i];
        in.view[i] = m.viewAngles[i];
        in.roomscale[i] = m.vr.roomscaleMove[i];
    }
    return in;
}

void serverQueue(client_t* client, const netmove::WireMove& m)
{
    const int slot = static_cast<int>(client - svs.clients);
    if(!movesOneByOne(slot))
    {
        return;
    }
    ServerSlot& s = serverSlots[slot];
    if(s.n == maxQueued)
    {
        memmove(&s.queue[0], &s.queue[1], sizeof(s.queue[0]) * (maxQueued - 1));
        s.n--;
    }
    s.queue[s.n++] = inputOf(m);
}

void serverReset(int slot)
{
    if(slot >= 0 && slot < MAX_SCOREBOARD)
    {
        serverSlots[slot] = ServerSlot{};
    }
}

void parseState()
{
    const int bits = MSG_ReadShort() & 0xffff;
    const za::U32 seq = static_cast<za::U32>(MSG_ReadLong());
    vec3_t o, v, n;
    for(float* p : {o, v, n})
    {
        for(int i = 0; i < 3; i++)
        {
            p[i] = MSG_ReadFloat();
        }
    }
    const float age = MSG_ReadFloat();
    pmove::Rules r;
    for(float* f : {&r.maxspeed, &r.stickScale, &r.accelerate, &r.friction, &r.stopspeed, &r.edgefriction, &r.gravity,
            &r.maxvelocity, &r.stepsize, &r.roomscaleScale})
    {
        *f = MSG_ReadFloat();
    }
    const float width = MSG_ReadFloat();
    if(msg_badread || (cp.have && seq < cp.seq))
    {
        return; // (an older one: the snapshot's base kept past the room)
    }
    // (A snapshot parses its unchanged records again: the same state is not a new one.)
    if(cp.have && seq == cp.seq && bits == cp.bits && VectorCompare(o, cp.origin) && VectorCompare(v, cp.velocity))
    {
        return;
    }
    r.slopeWalk = (bits & bitSlopeWalk) != 0;
    r.nostep = (bits & bitNoStep) != 0;
    r.gravityOn = (bits & bitGravity) != 0;
    r.wading = (bits & bitWading) != 0;
    r.roomscaleOn = (bits & bitRoomscale) != 0;
    cp.have = true;
    cp.seq = seq;
    cp.bits = bits;
    VectorCopy(o, cp.origin);
    VectorCopy(v, cp.velocity);
    VectorCopy(n, cp.groundNormal);
    cp.groundAge = age;
    cp.rules = r;
    cp.hullWidth = width;
    cp.fresh = true;
    cp.states++;
}

void clientClear()
{
    const ClientPredict keep = cp; // (the report's counts and a probe span connections)
    cp = ClientPredict{};
    cp.states = keep.states;
    cp.frames = keep.frames;
    cp.unpredicted = keep.unpredicted;
    cp.depthSum = keep.depthSum;
    cp.corrN = keep.corrN;
    cp.flatN = keep.flatN;
    cp.corrMax = keep.corrMax;
    cp.corrSum = keep.corrSum;
    cp.flatMax = keep.flatMax;
}

void report()
{
    Con_Printf("mp_predict: on=%d plain=%d states=%d frames=%d unpredicted=%d depth_mean=%.1f corr_n=%d corr_max=%.3f "
               "corr_mean=%.3f flat_n=%d flat_max=%.3f err=%.3f seq=%u newest=%u\n",
        vr_net_predict.value != 0.f && netmove::clientUses() ? 1 : 0, cp.bits & bitWalked ? 1 : 0, cp.states, cp.frames,
        cp.unpredicted, cp.frames > 0 ? static_cast<double>(cp.depthSum) / cp.frames : 0.0, cp.corrN, cp.corrMax,
        cp.corrN > 0 ? cp.corrSum / cp.corrN : 0.f, cp.flatN, cp.flatMax, VectorLength(cp.error), cp.seq,
        netmove::clientNewest());
    if(Cmd_Argc() > 1 && !q_strcasecmp(Cmd_Argv(0), "vr_mp_predict_report") && !q_strcasecmp(Cmd_Argv(1), "reset"))
    {
        // (vr_mp_predict_report reset: the counts from here, a test's setup left out; the states read go on)
        cp.frames = cp.unpredicted = cp.corrN = cp.flatN = 0;
        cp.depthSum = 0;
        cp.corrMax = cp.corrSum = cp.flatMax = 0.f;
    }
}

void init()
{
    Cmd_AddCommand("vr_mp_predict_report", report);
    Cmd_AddCommand("vr_mp_predict_probe", probe_f);
}

} // namespace qvr::predict

using namespace qvr;
using namespace qvr::predict;

extern "C" void VR_PredictBeginRead(client_t* client)
{
    const int slot = static_cast<int>(client - svs.clients);
    if(slot >= 0 && slot < MAX_SCOREBOARD)
    {
        serverSlots[slot].n = 0; // (moves a frame not walked one by one dropped: it ran the newest, as NetQuake)
        serverSlots[slot].walked = false;
        serverSlots[slot].plain = false; // (VR_PredictThink says again: a dead or paused frame does not think)
    }
}

extern "C" int VR_PredictThink(edict_t* ent)
{
    ServerSlot* s = slotOfEdict(ent);
    if(!s)
    {
        return 0;
    }
    const int slot = static_cast<int>(s - serverSlots);
    const int flags = static_cast<int>(ent->v.flags);
    s->plain = movesOneByOne(slot) && static_cast<int>(ent->v.movetype) == MOVETYPE_WALK && ent->v.health > 0.f &&
               !(flags & FL_WATERJUMP) && ent->v.waterlevel < 2.f && !hull::hasLyingBox(ent) && !hull::isFlat(ent);
    if(s->plain)
    {
        computeRules(ent, s->rules);
    }
    return s->plain ? 1 : 0;
}

extern "C" int VR_PredictWalks(edict_t* ent)
{
    const ServerSlot* s = slotOfEdict(ent);
    return s && s->plain ? 1 : 0;
}

extern "C" void VR_PredictPhysics(edict_t* ent)
{
    ServerSlot* s = slotOfEdict(ent);
    if(!s)
    {
        return;
    }
    pmove::Rules& r = s->rules;
    const bool water = SV_CheckWater(ent);
    r.gravityOn = !water && !(static_cast<int>(ent->v.flags) & FL_WATERJUMP);
    r.wading = ent->v.waterlevel != 0.f;
    SV_CheckStuck(ent);

    EdictBody b{ent};
    VectorCopy(s->groundNormal, b.groundNormal);
    b.groundAge = s->groundAge;
    s->credit = q_min(s->credit + static_cast<float>(host_frametime) * creditRate, creditMost);
    for(int k = 0; k < s->n && !b.removed; k++)
    {
        pmove::Input in = s->queue[k];
        if(in.dt > s->credit)
        {
            in.dt = s->credit; // (a client running ahead of the server's clock: held to it)
        }
        s->credit -= in.dt;
        pmove::playerMove(b, in, r);
    }
    if(!b.removed)
    {
        b.store();
        VR_FindRadius_Dirty(ent);
    }
    VectorCopy(b.groundNormal, s->groundNormal);
    s->groundAge = b.groundAge;
    s->n = 0;
    s->walked = true;
    if(!ent->free)
    {
        VR_PortalClientCross(ent); // (VR_ClientRoomscaleMove's, after the move: the room-scale walk ran in the moves)
    }
}

extern "C" void VR_WritePredict(sizebuf_t* msg, client_t* client)
{
    const int slot = static_cast<int>(client - svs.clients);
    if(!movesOneByOne(slot) || !client->edict || msg->cursize + 112 > msg->maxsize)
    {
        return;
    }
    const ServerSlot& s = serverSlots[slot];
    const edict_t* ent = client->edict;
    const pmove::Rules& r = s.rules;
    const int flags = static_cast<int>(ent->v.flags);
    const int bits = (s.walked ? bitWalked : 0) | (flags & FL_ONGROUND ? bitOnGround : 0) |
                     (flags & FL_WATERJUMP ? bitWaterJump : 0) | (r.slopeWalk ? bitSlopeWalk : 0) |
                     (r.nostep ? bitNoStep : 0) | (r.gravityOn ? bitGravity : 0) | (r.wading ? bitWading : 0) |
                     (r.roomscaleOn ? bitRoomscale : 0) | (vr_hull_brushmodels.value != 0.f ? bitHullBrushModels : 0) |
                     (vr_hull_edge_probe.value != 0.f ? bitEdgeProbe : 0);
    const int at = msg->cursize;
    MSG_WriteByte(msg, protocol::svc_quakevr);
    MSG_WriteByte(msg, protocol::QVR_SVC_PREDICT);
    MSG_WriteShort(msg, bits);
    MSG_WriteLong(msg, static_cast<int>(netmove::serverLast(slot)));
    for(const float* p : {ent->v.origin, ent->v.velocity, static_cast<const float*>(s.groundNormal)})
    {
        for(int i = 0; i < 3; i++)
        {
            MSG_WriteFloat(msg, p[i]);
        }
    }
    MSG_WriteFloat(msg, s.groundAge);
    for(const float f : {r.maxspeed, r.stickScale, r.accelerate, r.friction, r.stopspeed, r.edgefriction, r.gravity,
            r.maxvelocity, r.stepsize, r.roomscaleScale})
    {
        MSG_WriteFloat(msg, f);
    }
    MSG_WriteFloat(msg, vr_hull_width.value > 0.f ? vr_hull_width.value : 0.f);
    VR_SnapTake(msg, at, stateKey); // (a snapshot record: an idle player's costs a few bytes)
}

extern "C" void VR_PredictRelink(int num, float* origin)
{
    if(num != cl.viewentity)
    {
        if(num == cp.ridden && num > 0 && cp.drawing)
        {
            VectorCopy(cl_entities[num].msg_origins[0], origin); // (the lift he stands on: where his replay met it)
        }
        return;
    }
    if(cp.frame == host_framecount)
    {
        if(cp.drawing)
        {
            VectorCopy(cp.drawn, origin);
        }
        return;
    }
    cp.frame = host_framecount;
    const double since = cp.lastRelink > 0.0 ? q_min(realtime - cp.lastRelink, 0.25) : 0.0;
    cp.lastRelink = realtime;
    const za::U32 newest = netmove::clientNewest();
    if(newest != cp.lastNewest)
    {
        netmove::WireMove m;
        cp.lastNewest = newest;
        cp.lastNewTime = realtime;
        cp.lastDt = netmove::clientMoveAt(newest, m) ? inputOf(m).dt : 0.f;
    }
    vec3_t predicted, before;
    cp.drawing = clientActive() && replay(predicted, before);
    if(!cp.drawing)
    {
        if(netmove::clientUses() && cls.signon == SIGNONS)
        {
            cp.unpredicted++;
        }
        VectorCopy(vec3_origin, cp.error);
        cp.ridden = 0;
        for(Past& p : cp.past)
        {
            p.seq = 0;
        }
        probeCheck(origin);
        return;
    }
    cp.frames++;
    // Frames drawn faster than moves are made (a 90 Hz headset on 72 Hz ticks, an uncapped window): between the place
    // before the newest move and after it, by the time since it was made over its frame's (no step a move, no
    // overshoot at a stop; a move's time behind).
    const float frac =
        cp.lastDt > 0.f ? q_min(q_max(static_cast<float>((realtime - cp.lastNewTime) / cp.lastDt), 0.f), 1.f) : 1.f;
    const float k = expf(-static_cast<float>(since) / easeTime);
    VectorScale(cp.error, k, cp.error);
    for(int i = 0; i < 3; i++)
    {
        cp.drawn[i] = before[i] + frac * (predicted[i] - before[i]) + cp.error[i];
    }
    VectorCopy(cp.drawn, origin);
    probeCheck(origin);
}
