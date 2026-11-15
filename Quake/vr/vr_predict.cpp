// vr_predict.cpp -- a remote client's own walk predicted (vr_predict.hpp; MP_PLAN.md, B8).

#include "vr_predict.hpp"
#include "vr_api.h"
#include "vr_climb.hpp"
#include "vr_server.hpp"
#include "vr_cvars.hpp"
#include "vr_hull.hpp"
#include "vr_netsnap.hpp"
#include "vr_physics.hpp"
#include "vr_progs.hpp"
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
constexpr int maxMovers = 6;          // movers in motion sent with a state at most (the nearest by edict order)
constexpr float moverNear = 1024.f;   // ... within this of him, units
constexpr float moverNoStop = 1000.f; // ... the time to its stop when it has no think ahead, seconds

enum StateBits
{
    bitWalked = 1,      // this frame's walk ran move by move (else: not predicted)
    bitOnGround = 2,
    bitWaterJump = 4,
    bitSlopeWalk = 8,
    bitNoStep = 16,
    bitWaterDrag = 32,        // QC WaterMove's drag in the moves (B8e: the water's level is each move's own)
    bitRoomscale = 128,
    bitHullBrushModels = 256, // the narrowed box meets brush models too (vr_hull_brushmodels)
    bitEdgeProbe = 512,       // the ledge test from the narrowed box's edge (vr_hull_edge_probe)
    bitJumpReleased = 1024,   // FL_JUMPRELEASED (the jump key let go since his last jump)
    bitWaterJumpOut = 2048,   // Quake's jump out of the water in the moves (B8f: QC's .vr_movewaterjump)
    bitCrawl = 4096,          // lying down, crawling with his arms: the stick's share of the crawl in the moves (B8g)
    bitLying = 8192,          // the lying box (hull::hasLyingBox: its height is the rules' box)
    bitMantle = 64,           // mantling: the mantle's way and time follow (B8h: his moves carry him along it)
    bitHang = 16384,          // hanging: the holds, the hands' last places, the owed follow (B8i: his moves' hands pull)
    bitTake = 32768,          // climbing on: the take's rules and press memory follow (B8j part 2: his moves take holds)
};

constexpr float jumpSpeed = 270.f; // QC PlayerJump's
constexpr int chanBody = 4;        // QC's CHAN_BODY

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
    float crawl[2]{0.f, 0.f}; // the crawl's own speed after the frame (physics::crawlState's)
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

    float teleportSeen{0.f}; // (.teleport_time when holdLeft was taken from it: QuakeC setting it again starts afresh)

    explicit EdictBody(edict_t* e) : ent{e} { load(); }

    void load()
    {
        VectorCopy(ent->v.origin, origin);
        VectorCopy(ent->v.velocity, velocity);
        flags = static_cast<int>(ent->v.flags);
        jumpDir[0] = ent->v.movedir[0];
        jumpDir[1] = ent->v.movedir[1];
        if(ent->v.teleport_time != teleportSeen || teleportSeen == 0.f)
        {
            teleportSeen = ent->v.teleport_time;
            holdLeft = q_max(static_cast<float>(ent->v.teleport_time - qcvm->time), -1.f);
        }
    }
    void store()
    {
        VectorCopy(origin, ent->v.origin);
        VectorCopy(velocity, ent->v.velocity);
        if((static_cast<int>(ent->v.flags) & FL_WATERJUMP) && !(flags & FL_WATERJUMP))
        {
            ent->v.teleport_time = teleportSeen = 0.f; // (SV_WaterJump's end)
        }
        ent->v.flags = static_cast<float>(flags);
    }

    int contents(const float* p) override { return SV_PointContents(const_cast<float*>(p)); }

    trace_t trace(const float* start, const float* end) override
    {
        store();
        return SV_Move(const_cast<float*>(start), ent->v.mins, ent->v.maxs, const_cast<float*>(end), MOVE_NORMAL, ent);
    }
    trace_t line(const float* start, const float* end) override
    {
        return SV_Move(const_cast<float*>(start), vec3_origin, vec3_origin, const_cast<float*>(end), MOVE_NOMONSTERS, ent);
    }
    trace_t traceWorld(const float* start, const float* end) override
    {
        return SV_Move(const_cast<float*>(start), ent->v.mins, ent->v.maxs, const_cast<float*>(end), MOVE_NOMONSTERS, ent);
    }
    void waterJumped(const float* movedir) override
    {
        VectorCopy(movedir, ent->v.movedir);
        ent->v.teleport_time = teleportSeen = static_cast<float>(qcvm->time) + holdLeft;
    }
    bool climbedNow{false}; // (vr_mp_watch's trace)
    void waterClimbed(int hand, float top, float rise) override
    {
        climbedNow = true;
        store();
        progs::waterClimbed(ent, hand, top, rise); // (the feel, the print)
        after();
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
    bool jumpedNow{false}; // (vr_mp_watch's trace)
    void jumped() override
    {
        jumpedNow = true;
        store();
        SV_StartSound(ent, chanBody, "player/plyrjmp8.wav", 255, 1.f); // (PlayerJump's: ATTN_NORM)
        ent->v.button2 = 0.f;
    }

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
    r.stickScale = VR_CarryMoveScale(ent);
    physics::swimRules(ent, r.shallowStick, r.wadeStick, r.swimStick, r.glide, r.swimMost);
    r.swimMax = sv_maxspeed.value * r.stickScale; // (SV_WaterMove's carrymax)
    r.waterDrag = true; // (a frame walked move by move: its .vr_moveswim set, QC WaterMove leaves the drag to the moves)
    r.feetZ = ent->v.mins[2] + 1.f;
    r.waistZ = VR_HullWaistZ(ent) - ent->v.origin[2];
    r.eyeZ = ent->v.view_ofs[2];
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
    r.boxHeight = hull::crouchedHeight(ent);
    r.maxsZ = ent->v.maxs[2];
    r.minsZ = ent->v.mins[2];
    // Lying down, crawling with his arms (B8g): the stick's share of the crawl in each move (VR_AfterCrawlMove's).
    r.crawl = physics::crawlState(ent) != nullptr;
    r.crawlTop = r.crawl ? physics::crawlTop(ent) : 0.f;
    r.crawlFriction = vr_prone_crawl_friction.value;
    r.crawlMost = vr_prone_crawl_max_speed.value;
}

// ---------------------------------------------------------------------------------------------------------------------
// Client

struct Past
{
    za::U32 seq{0};
    vec3_t org{0.f, 0.f, 0.f};
    int on{0};                    // the mover he stood on after it (0: none) ...
    vec3_t liftAt{0.f, 0.f, 0.f}; // ... where the replay had it
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
    struct Mover
    {
        int num{0};
        vec3_t vel{0.f, 0.f, 0.f};
        float left{0.f};
    };
    Mover movers[maxMovers]; // the movers near him in motion (B8e)
    int numMovers{0};
    float holdLeft{0.f};  // .teleport_time less the message's time
    float jumpDir[2]{0.f, 0.f}; // .movedir's x, y (a water jump's push)
    float crawl[2]{0.f, 0.f};   // the crawl's own speed (lying down: B8g)
    pmove::Mantle mantle;       // mantling: its way and time at the state (B8h)
    pmove::Hang hang;           // hanging: the holds, the hands' last places, the owed at the state (B8i)
    climb::TakeRules take;      // the take's rules (not enabled: none) and press memory at the state (B8j part 2)
    climb::TakeMemory takeMem;
    bool transNow{false};       // this state begins a mantle the last one hung (the server's test: not predicted)
    vec3_t transError{0.f, 0.f, 0.f}; // the part of `error` that is such a transition's (eased in, apart)
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
    int ridden{0}; // the brush entity he stands on (drawn where his replay has it) ...
    vec3_t riddenAt{0.f, 0.f, 0.f}; // ... its newest place at the last replay
    vec3_t riddenVel{0.f, 0.f, 0.f}; // ... its motion between the newest messages
    float riddenBefore{0.f}, riddenAfter{0.f}; // ... the replay's time before and after its newest move
    float drawFrac{1.f}; // how far through his newest move he is drawn
    float riddenLeft{0.f}; // the time to its stop
    vec3_t liftError{0.f, 0.f, 0.f}; // the part of `error` that is the lift's own (its place's prediction moved): it
                                     // eases in under him with him, the lift drawn with it

    // vr_mp_predict_report.
    int states{0}, frames{0}, unpredicted{0}, unWalked{0};
    long long depthSum{0};
    int corrN{0}, flatN{0}, airN{0}, rideN{0}; // (rideN: states a lift under him moved since the last)
    int swimN{0}, wjN{0}, landMovingN{0}, pushN{0}; // (states swimming, in a water jump; replays landing him on a
                                                 // brush model still moving; replayed moves a mover pushed him)
    int crawlN{0};                               // (states lying down: the lying box, B8g)
    int wclimbN{0};                              // (replays climbing him out of the water by hand: B8j part 3)
    int mantleN{0};                              // (states mantling: B8h)
    int hangN{0};                                // (states hanging, walked move by move: B8i) ...
    float hangZ0{0.f}, hangRise{0.f};            // ... the first one's height since the reset, the most above it
    int transN{0};                               // (corrections at a hang's mantle starting: B8i) ...
    float transMax{0.f};                         // ... the longest
    float swimMax{0.f}, liftMax{0.f}; // (liftMax: the lift's own predicted place moved under him, a state)
    float corrMax{0.f}, corrSum{0.f}, flatMax{0.f}, airMax{0.f}, errMax{0.f};

    // vr_mp_predict_probe.
    bool probing{false};
    char probeLabel[32]{};
    int probeFrame{0};
    za::U32 probeMove{0}; // the newest move sent when armed
    za::U32 jumpTraced{0}; // vr_mp_watch: the move whose replayed jump was printed last
    double probeTime{0.0};
    vec3_t probeOrg{0.f, 0.f, 0.f};
};
ClientPredict cp;

// A brush model of the map (a door, a lift) in the newest message, unturned: a move meets it there.
[[nodiscard]] bool brushMover(const entity_t& e)
{
    return e.model && e.model->type == mod_brush && e.model->name[0] == '*' && e.msgtime == cl.mtime[0] &&
           e.msg_angles[0][0] == 0.f && e.msg_angles[0][1] == 0.f && e.msg_angles[0][2] == 0.f;
}

// Brush entity `num`'s motion as the newest state sent it, units a second (0: still or not near him) -> the time to its
// stop, seconds.
float moverVel(int num, vec3_t out)
{
    for(int k = 0; k < cp.numMovers; k++)
    {
        if(cp.movers[k].num == num)
        {
            VectorCopy(cp.movers[k].vel, out);
            return cp.movers[k].left;
        }
    }
    VectorCopy(vec3_origin, out);
    return 0.f;
}

// ... where it will be `t` seconds of moves after the newest message (B8e: a lift or door goes on to its stop; the
// server runs a move before the movers of its frame, so the replay's first move meets them where the message has them).
void moverAt(int num, float t, vec3_t out)
{
    vec3_t v;
    const float left = moverVel(num, v);
    VectorMA(cl_entities[num].msg_origins[0], q_min(t, left), v, out);
}

// The client's world as a move sees it: the map's hull 1 and the brush models (doors, lifts) where they will be.
// (The narrowed box, the server's vr_hull_width, through vr_hull.cpp's brushes rebuilt from this map: as SV_Move meets it.)
struct ClientBody final : pmove::Body
{
    int hit{0};    // the brush entity the last trace met (0: the world or none)
    int ground{0}; // ... the floor last met
    float moveTime{0.f}; // the replay's time before this move (the movers' motion)

    int contents(const float* p) override
    {
        // (SV_PointContents: the world's hull 0, a current as water)
        const int cont = SV_HullPointContents(&cl.worldmodel->hulls[0], 0, const_cast<float*>(p));
        return cont <= CONTENTS_CURRENT_0 && cont >= CONTENTS_CURRENT_DOWN ? CONTENTS_WATER : cont;
    }

    // His box from `start` to `end` against brush entity `k` placed at `at`.
    static trace_t moverTrace(int k, const float* at, const float* start, const float* end)
    {
        const entity_t& e = cl_entities[k];
        const bool narrow = (cp.bits & bitHullBrushModels) != 0;
        vec3_t s, t;
        VectorSubtract(start, at, s);
        VectorSubtract(end, at, t);
        trace_t tr;
        memset(&tr, 0, sizeof(tr));
        tr.fraction = 1.f;
        tr.allsolid = true;
        VectorCopy(end, tr.endpos);
        if(!narrow || !hull::clientPlayerTrace(cl.worldmodel, e.model, cp.hullWidth, cp.rules.boxHeight, s, t, tr))
        {
            hull_t* h = &e.model->hulls[1];
            SV_RecursiveHullCheck(h, h->firstclipnode, 0.f, 1.f, s, t, &tr);
        }
        if(tr.fraction != 1.f)
        {
            VectorAdd(tr.endpos, at, tr.endpos);
        }
        return tr;
    }

    // The movers going on after his move (the server runs them after the players in its frame): the one he stands on
    // carries him, and one moving into him pushes him by its move (SV_PushMove: a box left inside the pusher's new
    // place -- the fall onto a rising lift that meets it between his moves; B8f) -> the one he stands on (0: none),
    // `liftAt` its new place.
    int moversAfter(float dt, vec3_t liftAt)
    {
        VectorCopy(vec3_origin, liftAt);
        const int on = (flags & FL_ONGROUND) && ground > 0 && ground < cl.num_entities ? ground : 0;
        for(int k = 1; k < cl.num_entities; k++)
        {
            vec3_t v;
            if(!brushMover(cl_entities[k]) || (k != on && moverVel(k, v) <= 0.f))
            {
                continue;
            }
            vec3_t was, now;
            moverAt(k, moveTime, was);
            moverAt(k, moveTime + dt, now);
            if(k == on)
            {
                VectorCopy(now, liftAt);
            }
            else if(VectorCompare(was, now) || !moverTrace(k, now, origin, origin).startsolid)
            {
                continue;
            }
            else
            {
                pushedNow = true;
            }
            for(int i = 0; i < 3; i++)
            {
                origin[i] += now[i] - was[i];
            }
        }
        return on;
    }
    bool pushedNow{false}; // (vr_mp_watch's trace)

    trace_t trace(const float* start, const float* end) override
    {
        trace_t best;
        memset(&best, 0, sizeof(best));
        best.fraction = 1.f;
        best.allsolid = true;
        VectorCopy(end, best.endpos);
        if(!hull::clientPlayerTrace(cl.worldmodel, cl.worldmodel, cp.hullWidth, cp.rules.boxHeight, start, end, best))
        {
            hull_t* hull = &cl.worldmodel->hulls[1];
            SV_RecursiveHullCheck(hull, hull->firstclipnode, 0.f, 1.f, const_cast<float*>(start), const_cast<float*>(end), &best);
        }
        hit = 0;
        for(int k = 1; k < cl.num_entities; k++)
        {
            const entity_t& e = cl_entities[k];
            if(!brushMover(e))
            {
                continue;
            }
            vec3_t at;
            moverAt(k, moveTime, at);
            const trace_t tr = moverTrace(k, at, start, end);
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
    trace_t line(const float* start, const float* end) override
    {
        // (the world's hull 0, then the brush models where they will be: MOVE_NOMONSTERS)
        trace_t best;
        memset(&best, 0, sizeof(best));
        best.fraction = 1.f;
        best.allsolid = true;
        VectorCopy(end, best.endpos);
        hull_t* hull = &cl.worldmodel->hulls[0];
        SV_RecursiveHullCheck(hull, hull->firstclipnode, 0.f, 1.f, const_cast<float*>(start), const_cast<float*>(end), &best);
        for(int k = 1; k < cl.num_entities; k++)
        {
            if(!brushMover(cl_entities[k]))
            {
                continue;
            }
            vec3_t at, s, t;
            moverAt(k, moveTime, at);
            VectorSubtract(start, at, s);
            VectorSubtract(end, at, t);
            trace_t tr;
            memset(&tr, 0, sizeof(tr));
            tr.fraction = 1.f;
            tr.allsolid = true;
            VectorCopy(t, tr.endpos);
            hull_t* h = &cl_entities[k].model->hulls[0];
            SV_RecursiveHullCheck(h, h->firstclipnode, 0.f, 1.f, s, t, &tr);
            if(tr.allsolid || tr.startsolid || tr.fraction < best.fraction)
            {
                VectorAdd(tr.endpos, at, tr.endpos);
                best = tr;
            }
        }
        return best;
    }
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
        bool solid = tr.startsolid;
        float fraction = tr.fraction;
        // (and the brush models: MOVE_NOMONSTERS meets them -- the lift or door he walks on, B8d)
        for(int k = 1; k < cl.num_entities && fraction == 1.f; k++)
        {
            const entity_t& e = cl_entities[k];
            if(!brushMover(e))
            {
                continue;
            }
            vec3_t at, s, t;
            moverAt(k, moveTime, at);
            VectorSubtract(start, at, s);
            VectorSubtract(stop, at, t);
            trace_t et;
            memset(&et, 0, sizeof(et));
            et.fraction = 1.f;
            et.allsolid = true;
            VectorCopy(t, et.endpos);
            hull_t* h = &e.model->hulls[0];
            SV_RecursiveHullCheck(h, h->firstclipnode, 0.f, 1.f, s, t, &et);
            if(et.allsolid || et.startsolid || et.fraction < fraction)
            {
                fraction = et.fraction;
                solid = solid || et.startsolid;
            }
        }
        return fraction == 1.f && !(fix && solid);
    }
    void floorMet(const trace_t&) override { ground = hit; }
    bool jumpedNow{false}; // (vr_mp_watch's trace)
    void jumped() override { jumpedNow = true; }
    bool climbedNow{false}; // (... and the report's: a climb out of the water by hand replayed)
    void waterClimbed(int, float, float) override { climbedNow = true; }
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
    b.flags = (cp.bits & bitOnGround ? FL_ONGROUND : 0) | (cp.bits & bitWaterJump ? FL_WATERJUMP : 0) |
              (cp.bits & bitJumpReleased ? FL_JUMPRELEASED : 0);
    VectorCopy(cp.groundNormal, b.groundNormal);
    b.groundAge = cp.groundAge;
    b.holdLeft = cp.holdLeft;
    b.jumpDir[0] = cp.jumpDir[0];
    b.jumpDir[1] = cp.jumpDir[1];
    b.crawl[0] = cp.crawl[0];
    b.crawl[1] = cp.crawl[1];
    b.mantle = cp.mantle;
    b.hang = cp.hang;
    climb::TakeRules take = cp.take; // (his moves take holds as the server's do: B8j part 2)
    climb::TakeMemory takeMem = cp.takeMem;
    const za::U32 newest = netmove::clientNewest();
    if(newest < cp.seq || newest - cp.seq >= static_cast<za::U32>(pastRing))
    {
        return false;
    }
    if(cp.fresh)
    {
        pmove::checkWater(b, cp.rules);
        cp.swimN += b.waterLevel >= 2 ? 1 : 0;
        cp.crawlN += cp.bits & bitLying ? 1 : 0;
    }
    pmove::MoveNotes notes;
    netmove::WireMove m;
    bool landedMoving = false; // (a move landed him on a lift or door still moving)
    int depth = 0;
    float lastDt = 0.f;
    VectorCopy(b.origin, before);
    for(za::U32 s = cp.seq + 1; s <= newest; s++)
    {
        if(!netmove::clientMoveAt(s, m))
        {
            return false;
        }
        VectorCopy(b.origin, before);
        b.jumpedNow = false;
        const pmove::Input in = inputOf(m);
        const bool inAir = !(b.flags & FL_ONGROUND);
        const bool hangWas = b.hang.on;
        b.climbedNow = false;
        pmove::playerMove(b, in, cp.rules, &notes);
        if(b.climbedNow && cp.fresh)
        {
            cp.wclimbN++;
            if(vr_mp_watch.value != 0.f)
            {
                Con_Printf("mp_predict_wclimb: replay s=%u seq=%u z=%.2f vz=%.1f\n", s, cp.seq, b.origin[2], b.velocity[2]);
            }
        }
        if(take.enabled)
        {
            if((hangWas && !b.hang.on && !b.mantle.on) || b.mantledNow)
            {
                take.noGrabLeft = climb::regrabDelay; // (the server's climb lets go, or the mantle ends: its regrab delay)
            }
            const bool hangBefore = b.hang.on;
            climb::replayTake(b, takeMem, take, in);
            if(b.hang.on && !hangBefore && cp.fresh && vr_mp_watch.value != 0.f)
            {
                Con_Printf("mp_predict_take: replay s=%u seq=%u z=%.2f\n", s, cp.seq, b.origin[2]);
            }
        }
        // The lift under him goes on after his move, carrying him; one moving into him pushes him.
        vec3_t liftAt;
        b.pushedNow = false;
        const int on = b.moversAfter(in.dt, liftAt);
        cp.pushN += b.pushedNow && cp.fresh ? 1 : 0;
        vec3_t onVel;
        landedMoving = landedMoving || (inAir && on > 0 && moverVel(on, onVel) > b.moveTime && VectorLength(onVel) > 0.f);
        lastDt = in.dt;
        b.moveTime += in.dt;
        if(b.jumpedNow && s != cp.jumpTraced && vr_mp_watch.value != 0.f)
        {
            cp.jumpTraced = s; // (each move's jump once: a replay that moves it to another move prints that one)
            Con_Printf("mp_predict_jump: s=%u seq=%u bits=%d z=%.2f\n", s, cp.seq, cp.bits, cp.origin[2]);
        }
        if(vr_mp_watch.value >= 2.f && cp.fresh)
        {
            vec3_t gv;
            const float gl = b.ground > 0 ? moverVel(b.ground, gv) : 0.f;
            Con_Printf("mp_predict_m: s=%u seq=%u y=%.2f z=%.2f vz=%.1f fl=%d dt=%.4f box=%.1f on=%d ground=%d lift=%.2f "
                "left=%.3f t=%.4f nm=%d\n", s, cp.seq, b.origin[1], b.origin[2], b.velocity[2], b.flags, in.dt,
                cp.rules.boxHeight, on, b.ground, liftAt[2], gl, b.moveTime, cp.numMovers);
        }
        Past& p = cp.cur[s % pastRing];
        p.seq = s;
        VectorCopy(b.origin, p.org);
        p.on = on;
        VectorCopy(liftAt, p.liftAt);
        depth++;
    }
    VectorCopy(b.origin, out);
    cp.depthSum += depth;
    const int ridden = (b.flags & FL_ONGROUND) && b.ground < cl.num_entities ? b.ground : 0;
    // The lift he stands on, drawn where his replay has it at his newest move (B8e: it goes on in the replay, carrying
    // him; B8d's carry between states, its newest place since the last, is that replay's own now).
    const bool liftMoved = ridden > 0 && ridden == cp.ridden && !VectorCompare(cl_entities[ridden].msg_origins[0], cp.riddenAt);
    cp.ridden = ridden;
    VectorCopy(vec3_origin, cp.riddenVel);
    if(ridden > 0)
    {
        VectorCopy(cl_entities[ridden].msg_origins[0], cp.riddenAt);
        cp.riddenLeft = moverVel(ridden, cp.riddenVel);
        cp.riddenAfter = b.moveTime;
        cp.riddenBefore = b.moveTime - lastDt;
    }
    cp.landMovingN += cp.fresh && landedMoving ? 1 : 0;

    if(cp.fresh)
    {
        // The correction: the newest move both this replay and the last one predicted, where each put him.
        for(za::U32 s = newest; s > cp.seq; s--)
        {
            Past was = cp.past[s % pastRing];
            const Past& now = cp.cur[s % pastRing];
            if(was.seq != s || now.seq != s)
            {
                continue;
            }
            // (on the same mover in both: his correction is his place on it; the mover's own predicted place moved by
            // its frames' timing, which the replay cannot know -- a move run a server frame sooner or later -- and eases
            // in under him, the two drawn together)
            vec3_t lift{0.f, 0.f, 0.f};
            if(was.on > 0 && was.on == now.on)
            {
                VectorSubtract(was.liftAt, now.liftAt, lift);
                cp.liftMax = q_max(cp.liftMax, VectorLength(lift));
            }
            vec3_t rel;
            for(int i = 0; i < 3; i++)
            {
                rel[i] = was.org[i] - now.org[i] - lift[i];
            }
            const float c = VectorLength(rel);
            if(c > (vr_mp_watch.value >= 3.f ? 0.05f : 1.f) && vr_mp_watch.value != 0.f)
            {
                Con_Printf("mp_predict_corr: c=%.2f s=%u seq=%u newest=%u bits=%d was=%.2f %.2f %.2f now=%.2f %.2f %.2f sv=%.2f %.2f %.2f vel=%.1f %.1f %.1f notes=%d%d%d\n", c, s, cp.seq, newest, cp.bits, was.org[0], was.org[1], was.org[2], now.org[0], now.org[1], now.org[2], cp.origin[0], cp.origin[1], cp.origin[2], cp.velocity[0], cp.velocity[1], cp.velocity[2], notes.wall ? 1 : 0, notes.stepped ? 1 : 0, notes.air ? 1 : 0);
            }
            if(cp.transNow)
            {
                // (the hang's mantle begun by the server's test, once a frame: the replay hung on until it heard)
                cp.transN++;
                cp.transMax = q_max(cp.transMax, c);
                if(c < snapAt)
                {
                    for(int i = 0; i < 3; i++)
                    {
                        cp.error[i] += was.org[i] - now.org[i];
                        cp.transError[i] += was.org[i] - now.org[i];
                    }
                }
                break;
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
            if(notes.air)
            {
                cp.airN++;
                cp.airMax = q_max(cp.airMax, c);
            }
            if(liftMoved)
            {
                cp.rideN++;
            }
            if(notes.swim)
            {
                cp.swimMax = q_max(cp.swimMax, c);
            }
            if(c >= snapAt)
            {
                VectorCopy(vec3_origin, cp.error);
                VectorCopy(vec3_origin, cp.liftError);
                VectorCopy(vec3_origin, cp.transError);
            }
            else
            {
                for(int i = 0; i < 3; i++)
                {
                    cp.error[i] += was.org[i] - now.org[i];
                    cp.liftError[i] += lift[i];
                }
            }
            break;
        }
        cp.fresh = false;
        cp.transNow = false;
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
    in.up = static_cast<float>(m.up);
    for(int i = 0; i < 3; i++)
    {
        in.steer[i] = m.vr.headAngles[i];
        in.view[i] = m.viewAngles[i];
        in.roomscale[i] = m.vr.roomscaleMove[i];
    }
    in.jump = (m.buttons & 2) != 0;
    for(int h = 0; h < 2; h++)
    {
        const glm::vec3 rel = m.vr.hands[h].pos - m.vr.origin; // (the integers sent: the server's exactly)
        in.hands[h][0] = rel.x;
        in.hands[h][1] = rel.y;
        in.hands[h][2] = rel.z;
    }
    in.grabs = ((m.vr.vrBits0 & (1 << 1)) ? 1 : 0) | ((m.vr.vrBits0 & (1 << 3)) ? 2 : 0); // (vr_climb.cpp's grabBit)
    in.headZ = m.vr.headPos.z - m.vr.origin.z; // (as the hands: the integers sent)
    for(int h = 0; h < 2; h++) // (a hold's take, B8j part 2: the same numbers on the server and in his replay)
    {
        const glm::vec3 tracked = m.vr.hands[h].tracked - m.vr.origin;
        for(int i = 0; i < 3; i++)
        {
            in.tracked[h][i] = tracked[i];
            in.throwVel[h][i] = m.vr.hands[h].throwVel[i];
            in.handVel[h][i] = m.vr.hands[h].vel[i]; // (the water climb's pull, B8j part 3)
        }
        in.hotspots[h] = m.vr.hotspots[h];
    }
    const glm::vec3 head = m.vr.headPos - m.vr.origin;
    for(int i = 0; i < 3; i++)
    {
        in.head[i] = head[i];
    }
    in.buttons = m.vr.buttons;
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
    const int climbBits = MSG_ReadByte(); // (the hands' climb out: B8j part 3)
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
    const float teleportTime = MSG_ReadFloat();
    float jumpDir[2];
    jumpDir[0] = MSG_ReadFloat();
    jumpDir[1] = MSG_ReadFloat();
    float crawl[2];
    crawl[0] = MSG_ReadFloat();
    crawl[1] = MSG_ReadFloat();
    pmove::Mantle mantle;
    if(bits & bitMantle)
    {
        mantle.on = true;
        for(float* p : {mantle.from, mantle.mid, mantle.to})
        {
            for(int i = 0; i < 3; i++)
            {
                p[i] = MSG_ReadFloat();
            }
        }
        mantle.at = MSG_ReadFloat();
        mantle.time = MSG_ReadFloat();
    }
    pmove::Hang hang;
    if(bits & bitHang)
    {
        hang.on = true;
        const int hb = MSG_ReadByte();
        hang.slipping = (hb & 16) != 0;
        for(int k = 0; k < 2; k++)
        {
            pmove::HangGrip& g = hang.grips[k];
            g.active = (hb & (1 << (k * 2))) != 0;
            g.light = (hb & (2 << (k * 2))) != 0;
            g.missed = (hb & (32 << k)) != 0;
            for(float* v : {g.hold, g.lastRel})
            {
                for(int i = 0; i < 3; i++)
                {
                    v[i] = MSG_ReadFloat();
                }
            }
            g.top = MSG_ReadFloat();
            for(float* f : {&g.out[0], &g.out[1], &g.relAtGrabZ, &g.depth[0], &g.depth[1], &g.depth[2], &g.depth[3], &g.depth[4]})
            {
                *f = MSG_ReadFloat();
            }
            if(g.missed)
            {
                for(float* f : {&g.missAge, &g.missOrigin[0], &g.missOrigin[1], &g.missOrigin[2], &g.missHold[0],
                        &g.missHold[1], &g.missHold[2], &g.missTop})
                {
                    *f = MSG_ReadFloat();
                }
            }
        }
        for(int i = 0; i < 3; i++)
        {
            hang.owed[i] = MSG_ReadFloat();
        }
        for(float* f : {&hang.minsZ, &hang.maxsXY[0], &hang.maxsXY[1], &hang.mantleTime, &hang.mantleHeight, &hang.mantleSlope})
        {
            *f = MSG_ReadFloat();
        }
    }
    climb::TakeRules take; // (not enabled: no take block)
    climb::TakeMemory takeMem;
    if(bits & bitTake)
    {
        const int tb = MSG_ReadByte();
        take.enabled = true;
        takeMem.pressedLast[0] = (tb & 1) != 0;
        takeMem.pressedLast[1] = (tb & 2) != 0;
        take.tired = (tb & 4) != 0;
        take.busy[0] = (tb & 8) != 0;
        take.busy[1] = (tb & 16) != 0;
        take.holster[0] = static_cast<unsigned>(MSG_ReadLong());
        take.holster[1] = static_cast<unsigned>(MSG_ReadLong());
        for(float* f : {&takeMem.missAge[0], &takeMem.missAge[1], &take.noGrabLeft, &take.airGrabTime, &take.leniency,
                &take.leniencyAir, &take.minHeight, &take.propMinHeight, &take.minsZ, &take.maxsX, &take.maxsY,
                &take.mantleTime, &take.mantleHeight, &take.mantleSlope})
        {
            *f = MSG_ReadFloat();
        }
    }
    pmove::Rules r;
    for(float* f : {&r.maxspeed, &r.stickScale, &r.accelerate, &r.friction, &r.stopspeed, &r.edgefriction, &r.gravity,
            &r.maxvelocity, &r.stepsize, &r.roomscaleScale, &r.jumpSpeed, &r.boxHeight, &r.feetZ, &r.waistZ, &r.eyeZ,
            &r.shallowStick, &r.wadeStick, &r.swimStick, &r.swimMax, &r.glide, &r.swimMost, &r.maxsZ, &r.crawlTop,
            &r.crawlFriction, &r.crawlMost, &r.minsZ})
    {
        *f = MSG_ReadFloat();
    }
    const float width = MSG_ReadFloat();
    ClientPredict::Mover movers[maxMovers];
    const int sentMovers = MSG_ReadByte(); // (q_min is a macro: read once)
    const int numMovers = sentMovers < maxMovers ? sentMovers : maxMovers;
    for(int k = 0; k < numMovers; k++)
    {
        movers[k].num = MSG_ReadShort();
        for(int i = 0; i < 3; i++)
        {
            movers[k].vel[i] = MSG_ReadFloat();
        }
        movers[k].left = MSG_ReadFloat();
        if(movers[k].num <= 0 || movers[k].num >= cl.num_entities)
        {
            movers[k].num = 0;
        }
    }
    if(msg_badread || (cp.have && seq < cp.seq))
    {
        return; // (an older one: the snapshot's base kept past the room)
    }
    // (A snapshot parses its unchanged records again: the same state is not a new one.)
    if(cp.have && seq == cp.seq && bits == cp.bits && VectorCompare(o, cp.origin) && VectorCompare(v, cp.velocity) &&
        crawl[0] == cp.crawl[0] && crawl[1] == cp.crawl[1] &&
        memcmp(hang.owed, cp.hang.owed, sizeof(hang.owed)) == 0 &&
        memcmp(hang.grips[0].lastRel, cp.hang.grips[0].lastRel, sizeof(hang.grips[0].lastRel)) == 0 &&
        memcmp(hang.grips[1].lastRel, cp.hang.grips[1].lastRel, sizeof(hang.grips[1].lastRel)) == 0)
    {
        return;
    }
    r.slopeWalk = (bits & bitSlopeWalk) != 0;
    r.nostep = (bits & bitNoStep) != 0;
    r.waterDrag = (bits & bitWaterDrag) != 0;
    r.waterJumpOut = (bits & bitWaterJumpOut) != 0;
    r.roomscaleOn = (bits & bitRoomscale) != 0;
    r.crawl = (bits & bitCrawl) != 0;
    r.waterClimb = (climbBits & 1) != 0;
    r.climbHands = (climbBits >> 1) & 3;
    cp.have = true;
    cp.crawl[0] = crawl[0];
    cp.crawl[1] = crawl[1];
    cp.mantle = mantle;
    cp.mantleN += mantle.on ? 1 : 0;
    // (a hold taken, or the hang's mantle begun: the server's searches, once a frame: the replay learns of them now)
    cp.transNow = (mantle.on && cp.hang.on) || (hang.on && !cp.hang.on);
    cp.hang = hang;
    cp.take = take;
    cp.takeMem = takeMem;
    if(hang.on && (bits & bitWalked))
    {
        cp.hangZ0 = cp.hangN == 0 ? o[2] : cp.hangZ0;
        cp.hangN++;
        cp.hangRise = q_max(cp.hangRise, o[2] - cp.hangZ0);
    }
    cp.holdLeft = q_max(static_cast<float>(teleportTime - cl.mtime[0]), -1.f);
    cp.jumpDir[0] = jumpDir[0];
    cp.jumpDir[1] = jumpDir[1];
    cp.numMovers = numMovers;
    memcpy(cp.movers, movers, sizeof(movers));
    if(bits & bitWaterJump)
    {
        cp.wjN++;
    }
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
    cp.unWalked = keep.unWalked;
    cp.depthSum = keep.depthSum;
    cp.corrN = keep.corrN;
    cp.flatN = keep.flatN;
    cp.corrMax = keep.corrMax;
    cp.corrSum = keep.corrSum;
    cp.flatMax = keep.flatMax;
    cp.airN = keep.airN;
    cp.rideN = keep.rideN;
    cp.airMax = keep.airMax;
    cp.errMax = keep.errMax;
    cp.swimN = keep.swimN;
    cp.swimMax = keep.swimMax;
    cp.liftMax = keep.liftMax;
    cp.wjN = keep.wjN;
    cp.landMovingN = keep.landMovingN;
    cp.pushN = keep.pushN;
    cp.crawlN = keep.crawlN;
    cp.mantleN = keep.mantleN;
    cp.hangN = keep.hangN;
    cp.hangZ0 = keep.hangZ0;
    cp.hangRise = keep.hangRise;
    cp.transN = keep.transN;
    cp.transMax = keep.transMax;
}

void report()
{
    Con_Printf("mp_predict: on=%d plain=%d states=%d frames=%d unpredicted=%d unwalked=%d depth_mean=%.1f corr_n=%d "
               "corr_max=%.3f corr_mean=%.3f flat_n=%d flat_max=%.3f air_n=%d air_max=%.3f ride_n=%d err=%.3f "
               "err_max=%.3f maxspeed=%.1f box=%.1f jump=%.0f swim_n=%d swim_max=%.3f wj_n=%d land_moving=%d "
               "lift_max=%.3f pushed=%d lying=%d arms=%d crawl_n=%d mantle_n=%d hang_n=%d hang_rise=%.2f trans_n=%d trans_max=%.3f "
               "wclimb=%d wclimb_n=%d seq=%u newest=%u\n",
        vr_net_predict.value != 0.f && netmove::clientUses() ? 1 : 0, cp.bits & bitWalked ? 1 : 0, cp.states, cp.frames,
        cp.unpredicted, cp.unWalked, cp.frames > 0 ? static_cast<double>(cp.depthSum) / cp.frames : 0.0, cp.corrN, cp.corrMax,
        cp.corrN > 0 ? cp.corrSum / cp.corrN : 0.f, cp.flatN, cp.flatMax, cp.airN, cp.airMax, cp.rideN, VectorLength(cp.error),
        cp.errMax, cp.rules.maxspeed, cp.rules.boxHeight, cp.rules.jumpSpeed, cp.swimN, cp.swimMax, cp.wjN,
        cp.landMovingN, cp.liftMax, cp.pushN, cp.bits & bitLying ? 1 : 0, cp.rules.crawl ? 1 : 0, cp.crawlN, cp.mantleN, cp.hangN, cp.hangRise, cp.transN, cp.transMax,
        cp.rules.waterClimb ? 1 + cp.rules.climbHands : 0, cp.wclimbN, cp.seq,
        netmove::clientNewest());
    if(Cmd_Argc() > 1 && !q_strcasecmp(Cmd_Argv(0), "vr_mp_predict_report") && !q_strcasecmp(Cmd_Argv(1), "reset"))
    {
        // (vr_mp_predict_report reset: the counts from here, a test's setup left out; the states read go on)
        cp.frames = cp.unpredicted = cp.unWalked = cp.corrN = cp.flatN = cp.airN = cp.rideN = 0;
        cp.swimN = cp.wjN = cp.landMovingN = cp.pushN = cp.crawlN = cp.mantleN = cp.hangN = cp.wclimbN = 0;
        cp.hangRise = cp.transMax = 0.f;
        cp.transN = 0;
        cp.depthSum = 0;
        cp.corrMax = cp.corrSum = cp.flatMax = cp.airMax = cp.errMax = cp.swimMax = cp.liftMax = 0.f;
    }
}

void init()
{
    Cmd_AddCommand("vr_mp_predict_report", report);
    Cmd_AddCommand("vr_mp_predict_probe", probe_f);
}

int waterClimbFrame(edict_t* ent, int hands)
{
    if(!ent || ent->free)
    {
        return 0;
    }
    // One move of the frame's hands (B9's one path: a remote client's moves run the same function each).
    pmove::Input in;
    in.grabs = hands & 3;
    const progs::FieldOffsets& f = progs::fields();
    for(int h = 0; h < 2; h++)
    {
        const glm::vec3 p = progs::fieldVec(ent, progs::handFields(h).pos);
        const glm::vec3 v = progs::fieldVec(ent, h ? f.handvel : f.offhandvel);
        for(int i = 0; i < 3; i++)
        {
            in.hands[h][i] = p[i] - ent->v.origin[i];
            in.handVel[h][i] = v[i];
        }
    }
    pmove::Rules r;
    const eval_t* g = GetEdictFieldValueByName(ent, "gravity");
    r.gravity = (g && g->_float ? g->_float : 1.f) * sv_gravity.value;
    r.maxsZ = ent->v.maxs[2];
    r.minsZ = ent->v.mins[2];
    r.waterClimb = true;
    r.climbHands = hands & 3;
    EdictBody b{ent};
    if(!pmove::waterClimbTry(b, in, r))
    {
        return 0;
    }
    if(!b.removed)
    {
        b.store();
    }
    return 1;
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
        if(client->edict)
        {
            progs::setFieldFloat(client->edict, progs::fields().vr_movejump, 0.f); // (QuakeC's jump until it does)
            progs::setFieldFloat(client->edict, progs::fields().vr_moveswim, 0.f);
            progs::setFieldFloat(client->edict, progs::fields().vr_movewaterjump, 0.f);
            progs::setFieldFloat(client->edict, progs::fields().vr_movewaterclimb, 0.f);
        }
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
    // (lying down too, B8g: the lying box is the rules' box, the stick's share of the crawl the moves')
    s->plain = movesOneByOne(slot) && static_cast<int>(ent->v.movetype) == MOVETYPE_WALK && ent->v.health > 0.f &&
               !hull::isFlat(ent);
    if(s->plain)
    {
        computeRules(ent, s->rules);
    }
    // (PlayerPreThink leaves his jump to his moves: lying down or AD's boots, QuakeC's; the field clears for the boots)
    progs::setFieldFloat(ent, progs::fields().vr_movejump, s->plain && !hull::isProne(ent) ? 1.f : 0.f);
    progs::setFieldFloat(ent, progs::fields().vr_moveswim, s->plain ? 1.f : 0.f); // (WaterMove leaves its drag to them)
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
    SV_CheckWater(ent); // (the frame's, for QuakeC and the water's effects; each move finds its own)
    r.jumpSpeed = progs::fieldFloatOr(ent, progs::fields().vr_movejump, 0.f) != 0.f ? jumpSpeed : 0.f;
    r.waterJumpOut = progs::fieldFloatOr(ent, progs::fields().vr_movewaterjump, 0.f) != 0.f; // (PlayerPreThink's)
    // (the hands' climb out, B8j part 3: PlayerPreThink's bit 0 on, bits 1-2 the empty hands)
    const int wc = static_cast<int>(progs::fieldFloatOr(ent, progs::fields().vr_movewaterclimb, 0.f));
    r.waterClimb = (wc & 1) != 0;
    r.climbHands = r.waterClimb ? (wc >> 1) & 3 : 0;
    pmove::Mantle mantle;
    const bool mantling = climb::mantlePath(ent, mantle); // (B8h: the mantle's way, run by his moves)
    pmove::Hang hang;
    bool hanging = !mantling && climb::hangPath(ent, hang); // (B8i: the hang's steps, run by his moves' hands)
    climb::TakeRules take;     // (B8j part 2: a hold taken in a move, the moves after it hang)
    climb::TakeMemory takeMem;
    const bool takes = climb::takesInMoves(ent) && climb::takeState(ent, take, takeMem);
    if(!mantling && !hanging)
    {
        SV_CheckStuck(ent); // (not on the mantle's way nor hanging: single player's climb frames do not either)
    }
    if(hanging)
    {
        server::rebaseHands(ent); // (the newest move's hands from where he is before the moves: the frame's part's)
    }

    EdictBody b{ent};
    b.mantle = mantle;
    b.hang = hang;
    pmove::HangSums hangSums;
    b.hangSums = hanging ? &hangSums : nullptr;
    VectorCopy(s->groundNormal, b.groundNormal);
    b.groundAge = s->groundAge;
    float* crawl = r.crawl ? physics::crawlState(ent) : nullptr; // (the crawl's own speed: his moves carry it)
    if(crawl)
    {
        b.crawl[0] = crawl[0];
        b.crawl[1] = crawl[1];
    }
    s->credit = q_min(s->credit + static_cast<float>(host_frametime) * creditRate, creditMost);
    const int slot = static_cast<int>(s - serverSlots);
    pmove::Input newest;
    for(int k = 0; k < s->n && !b.removed; k++)
    {
        pmove::Input in = s->queue[k];
        newest = in;
        const bool held = in.dt > s->credit;
        if(held)
        {
            in.dt = s->credit; // (a client running ahead of the server's clock: held to it)
        }
        s->credit -= in.dt;
        b.jumpedNow = false;
        b.climbedNow = false;
        pmove::playerMove(b, in, r);
        if(b.climbedNow && vr_mp_watch.value != 0.f)
        {
            Con_Printf("mp_predict_wclimb: slot=%d s=%u z=%.2f vz=%.1f\n", slot,
                netmove::serverLast(slot) - (s->n - 1 - k), b.origin[2], b.velocity[2]);
        }
        int took = 0;
        if(takes && !b.removed)
        {
            b.store(); // (the take's search and its hold read his edict: where this move left him)
            took = climb::moveTake(ent, b, take, in);
            if(took == 1 && !hanging)
            {
                hanging = true; // (the moves after it hang; the frame's part after them: climb::hangMoved)
                b.hangSums = &hangSums;
            }
        }
        if((b.jumpedNow || held || vr_mp_watch.value >= 2.f) && vr_mp_watch.value != 0.f)
        {
            Con_Printf("mp_predict_sv: slot=%d s=%u%s%s y=%.2f z=%.2f vz=%.1f fl=%d dt=%.4f box=%.1f ge=%d k=%d/%d\n", slot,
                netmove::serverLast(slot) - (s->n - 1 - k), b.jumpedNow ? " jumped" : "", held ? " held" : "",
                b.origin[1], b.origin[2], b.velocity[2], b.flags, in.dt, hull::crouchedHeight(ent),
                ent->v.groundentity, k, s->n);
        }
        if(took != 0 && vr_mp_watch.value != 0.f)
        {
            Con_Printf("mp_predict_take: slot=%d s=%u z=%.2f hang=%d\n", slot, netmove::serverLast(slot) - (s->n - 1 - k),
                b.origin[2], b.hang.on ? 1 : 0);
        }
        if(took == 2)
        {
            break; // (a hold that moves: his frame's hang from the next frame, the rest of these moves its)
        }
    }
    if(!b.removed)
    {
        b.store();
        if(mantling)
        {
            climb::mantleMoved(ent, b.mantle, !b.mantle.on); // (its time kept; at its end he stands, the climb lets go)
        }
        if(hanging)
        {
            climb::hangMoved(ent, b.hang, hangSums, b.mantle); // (the mantle begun by a move, else the frame's part)
        }
        if(b.waterLevel >= 2 && !(b.flags & FL_WATERJUMP) && s->n > 0 && !mantling && !hanging)
        {
            // The hands' strokes (VR_AfterWaterMove's, less the glide the moves ran), once a frame on the newest move's
            // stick: the server's alone (his replay corrects to them, eased in).
            const float scale = r.stickScale * (b.waterLevel >= 3 || !(b.flags & FL_ONGROUND) ? r.swimStick : r.wadeStick);
            physics::swimStrokes(ent, newest.forward * scale, newest.side * scale, newest.up * scale);
        }
        if(crawl)
        {
            crawl[0] = b.crawl[0];
            crawl[1] = b.crawl[1];
            if(b.waterLevel < 2 && s->n > 0 && !mantling && !hanging)
            {
                // The hands' pulls (VR_AfterCrawlMove's, the stick's want with them), once a frame on the newest move's
                // stick: the server's alone (his replay corrects to them, eased in).
                const float scale = r.stickScale * (b.waterLevel >= 1 ? r.shallowStick : 1.f);
                physics::crawlHands(ent, newest.forward * scale, newest.side * scale);
            }
        }
        VR_FindRadius_Dirty(ent);
    }
    s->crawl[0] = crawl ? crawl[0] : 0.f;
    s->crawl[1] = crawl ? crawl[1] : 0.f;
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
    if(!movesOneByOne(slot) || !client->edict || msg->cursize + 700 > msg->maxsize)
    {
        return;
    }
    const ServerSlot& s = serverSlots[slot];
    const edict_t* ent = client->edict;
    const pmove::Rules& r = s.rules;
    const int flags = static_cast<int>(ent->v.flags);
    const int bits = (s.walked ? bitWalked : 0) | (flags & FL_ONGROUND ? bitOnGround : 0) |
                     (flags & FL_WATERJUMP ? bitWaterJump : 0) | (r.slopeWalk ? bitSlopeWalk : 0) |
                     (r.nostep ? bitNoStep : 0) | (r.waterDrag ? bitWaterDrag : 0) | (r.roomscaleOn ? bitRoomscale : 0) | (vr_hull_brushmodels.value != 0.f ? bitHullBrushModels : 0) |
                     (vr_hull_edge_probe.value != 0.f ? bitEdgeProbe : 0) |
                     (flags & FL_JUMPRELEASED ? bitJumpReleased : 0) | (r.waterJumpOut ? bitWaterJumpOut : 0) |
                     (r.crawl ? bitCrawl : 0) | (hull::hasLyingBox(ent) ? bitLying : 0);
    pmove::Mantle mantle;
    const bool mantling = climb::mantlePath(const_cast<edict_t*>(ent), mantle);
    pmove::Hang hang;
    const bool hanging = !mantling && s.walked && climb::hangPath(const_cast<edict_t*>(ent), hang);
    climb::TakeRules take;
    climb::TakeMemory takeMem;
    const bool takes = !mantling && s.walked && climb::takeState(const_cast<edict_t*>(ent), take, takeMem) && take.enabled;
    const int at = msg->cursize;
    MSG_WriteByte(msg, protocol::svc_quakevr);
    MSG_WriteByte(msg, protocol::QVR_SVC_PREDICT);
    MSG_WriteShort(msg, bits | (mantling ? bitMantle : 0) | (hanging ? bitHang : 0) | (takes ? bitTake : 0));
    MSG_WriteByte(msg, (r.waterClimb ? 1 : 0) | (r.climbHands << 1)); // (the hands' climb out in the moves: B8j part 3)
    MSG_WriteLong(msg, static_cast<int>(netmove::serverLast(slot)));
    for(const float* p : {ent->v.origin, ent->v.velocity, static_cast<const float*>(s.groundNormal)})
    {
        for(int i = 0; i < 3; i++)
        {
            MSG_WriteFloat(msg, p[i]);
        }
    }
    MSG_WriteFloat(msg, s.groundAge);
    MSG_WriteFloat(msg, ent->v.teleport_time); // (its time left read against the message's time: an idle record stays)
    MSG_WriteFloat(msg, ent->v.movedir[0]);
    MSG_WriteFloat(msg, ent->v.movedir[1]);
    MSG_WriteFloat(msg, s.crawl[0]);
    MSG_WriteFloat(msg, s.crawl[1]);
    if(mantling)
    {
        for(const float* p : {mantle.from, mantle.mid, mantle.to})
        {
            for(int i = 0; i < 3; i++)
            {
                MSG_WriteFloat(msg, p[i]);
            }
        }
        MSG_WriteFloat(msg, mantle.at);
        MSG_WriteFloat(msg, mantle.time);
    }
    if(hanging)
    {
        int hb = hang.slipping ? 16 : 0;
        for(int k = 0; k < 2; k++)
        {
            hb |= (hang.grips[k].active ? 1 << (k * 2) : 0) | (hang.grips[k].light ? 2 << (k * 2) : 0) |
                  (hang.grips[k].missed ? 32 << k : 0);
        }
        MSG_WriteByte(msg, hb);
        for(const pmove::HangGrip& g : hang.grips)
        {
            for(const float* v : {g.hold, g.lastRel})
            {
                for(int i = 0; i < 3; i++)
                {
                    MSG_WriteFloat(msg, v[i]);
                }
            }
            MSG_WriteFloat(msg, g.top);
            // (the mantle's test, B8j: the ledge, the pull down's start, the depths; the search that found no room)
            for(const float f : {g.out[0], g.out[1], g.relAtGrabZ, g.depth[0], g.depth[1], g.depth[2], g.depth[3], g.depth[4]})
            {
                MSG_WriteFloat(msg, f);
            }
            if(g.missed)
            {
                for(const float f : {g.missAge, g.missOrigin[0], g.missOrigin[1], g.missOrigin[2], g.missHold[0],
                        g.missHold[1], g.missHold[2], g.missTop})
                {
                    MSG_WriteFloat(msg, f);
                }
            }
        }
        for(int i = 0; i < 3; i++)
        {
            MSG_WriteFloat(msg, hang.owed[i]);
        }
        for(const float f : {hang.minsZ, hang.maxsXY[0], hang.maxsXY[1], hang.mantleTime, hang.mantleHeight, hang.mantleSlope})
        {
            MSG_WriteFloat(msg, f);
        }
    }
    if(takes)
    {
        // (B8j part 2: his replay takes holds as his moves do on the server, on these)
        MSG_WriteByte(msg, (takeMem.pressedLast[0] ? 1 : 0) | (takeMem.pressedLast[1] ? 2 : 0) | (take.tired ? 4 : 0) |
                               (take.busy[0] ? 8 : 0) | (take.busy[1] ? 16 : 0));
        MSG_WriteLong(msg, static_cast<int>(take.holster[0]));
        MSG_WriteLong(msg, static_cast<int>(take.holster[1]));
        for(const float f : {takeMem.missAge[0], takeMem.missAge[1], take.noGrabLeft, take.airGrabTime, take.leniency,
                take.leniencyAir, take.minHeight, take.propMinHeight, take.minsZ, take.maxsX, take.maxsY, take.mantleTime,
                take.mantleHeight, take.mantleSlope})
        {
            MSG_WriteFloat(msg, f);
        }
    }
    for(const float f : {r.maxspeed, r.stickScale, r.accelerate, r.friction, r.stopspeed, r.edgefriction, r.gravity,
            r.maxvelocity, r.stepsize, r.roomscaleScale, r.jumpSpeed, r.boxHeight, r.feetZ, r.waistZ, r.eyeZ,
            r.shallowStick, r.wadeStick, r.swimStick, r.swimMax, r.glide, r.swimMost, r.maxsZ, r.crawlTop,
            r.crawlFriction, r.crawlMost, r.minsZ})
    {
        MSG_WriteFloat(msg, f);
    }
    MSG_WriteFloat(msg, vr_hull_width.value > 0.f ? vr_hull_width.value : 0.f);
    // The movers near him in motion (B8e): their velocity and the time to their stop (SV_PushMove ends a move at its
    // think: SUB_CalcMove's), so his replay meets a lift or door where it will be, and no further than it goes.
    int nums[maxMovers];
    int n = 0;
    for(int e = svs.maxclients + 1; e < qcvm->num_edicts && n < maxMovers; e++)
    {
        const edict_t* m = EDICT_NUM(e);
        if(m->free || static_cast<int>(m->v.movetype) != MOVETYPE_PUSH || VectorCompare(m->v.velocity, vec3_origin))
        {
            continue;
        }
        vec3_t mid;
        for(int i = 0; i < 3; i++)
        {
            mid[i] = (m->v.absmin[i] + m->v.absmax[i]) * 0.5f;
        }
        if(dist(mid, ent->v.origin) <= moverNear)
        {
            nums[n++] = e;
        }
    }
    MSG_WriteByte(msg, n);
    for(int k = 0; k < n; k++)
    {
        const edict_t* m = EDICT_NUM(nums[k]);
        MSG_WriteShort(msg, nums[k]);
        for(int i = 0; i < 3; i++)
        {
            MSG_WriteFloat(msg, m->v.velocity[i]);
        }
        MSG_WriteFloat(msg, m->v.nextthink > m->v.ltime ? m->v.nextthink - m->v.ltime : moverNoStop);
    }
    VR_SnapTake(msg, at, stateKey); // (a snapshot record: an idle player's costs a few bytes)
}

extern "C" void VR_PredictRelink(int num, float* origin)
{
    if(num != cl.viewentity)
    {
        if(num == cp.ridden && num > 0 && cp.drawing)
        {
            // (the lift he stands on: where his replay has it, between his newest move's start and end as he is drawn)
            const float t = cp.riddenBefore + cp.drawFrac * (cp.riddenAfter - cp.riddenBefore);
            VectorMA(cl_entities[num].msg_origins[0], q_min(t, cp.riddenLeft), cp.riddenVel, origin);
            VectorAdd(origin, cp.liftError, origin);
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
            cp.unWalked += cp.have && !(cp.bits & bitWalked) ? 1 : 0; // (the server's frame not run move by move)
        }
        VectorCopy(vec3_origin, cp.error);
        VectorCopy(vec3_origin, cp.transError);
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
    cp.drawFrac = frac;
    const float k = expf(-static_cast<float>(since) / easeTime);
    VectorScale(cp.error, k, cp.error);
    VectorScale(cp.liftError, cp.ridden > 0 ? k : 0.f, cp.liftError);
    VectorScale(cp.transError, k, cp.transError);
    vec3_t own; // (his offset on what he stands on, less a mantle's start: the transition's, counted apart)
    VectorSubtract(cp.error, cp.liftError, own);
    VectorSubtract(own, cp.transError, own);
    cp.errMax = q_max(cp.errMax, VectorLength(own));
    for(int i = 0; i < 3; i++)
    {
        cp.drawn[i] = before[i] + frac * (predicted[i] - before[i]) + cp.error[i];
    }
    VectorCopy(cp.drawn, origin);
    probeCheck(origin);
}
