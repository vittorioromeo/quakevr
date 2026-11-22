// vr_predict.cpp -- a remote client's own walk predicted (vr_predict.hpp; MP_PLAN.md, B8).

#include "vr_predict.hpp"
#include "vr_seat.hpp"
#include "vr_moveguard.hpp"
#include "vr_units.hpp"
#include "vr_api.h"
#include "vr_climb.hpp"
#include "vr_clientprefs.hpp"
#include "vr_server.hpp"
#include "vr_slide.hpp"
#include "vr_cvars.hpp"
#include "vr_hull.hpp"
#include "vr_netsnap.hpp"
#include "vr_physics.hpp"
#include "vr_progs.hpp"
#include "vr_protocol.hpp"
#include "vr_trace.hpp"
#include "vr_mem.hpp"

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

// vr_mp_predict_tracelog <move>: every trace the moves make for that move number (the client's replays of it, the
// server's run of it), bit for bit ("mp_tracelog:"), to find where a replay leaves the server's result (B14b). 0 off.
struct TraceLog
{
    za::U32 seq{0};
    bool now{false};
    const char* side{""};
};
// seat-shared: mp_tracelog's own (a test's)
TraceLog traceLog;

void logTrace(const char* what, const float* start, const float* end, const trace_t& t)
{
    if(!traceLog.now)
    {
        return;
    }
    Con_Printf("mp_tracelog: %s %s s=%.9g %.9g %.9g e=%.9g %.9g %.9g f=%.9g ep=%.9g %.9g %.9g n=%.6g %.6g %.6g ss=%d as=%d\n",
        traceLog.side, what, start[0], start[1], start[2], end[0], end[1], end[2], t.fraction, t.endpos[0], t.endpos[1],
        t.endpos[2], t.plane.normal[0], t.plane.normal[1], t.plane.normal[2], t.startsolid ? 1 : 0, t.allsolid ? 1 : 0);
}
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

// The water's type in the state record's two bits (B11b part 2): none, water (a current too), slime, lava.
[[nodiscard]] constexpr int waterCode(int type)
{
    return type == CONTENTS_SLIME ? 2 : type == CONTENTS_LAVA ? 3 : type <= CONTENTS_WATER ? 1 : 0;
}
[[nodiscard]] constexpr int waterOfCode(int code)
{
    return code == 1 ? CONTENTS_WATER : code == 2 ? CONTENTS_SLIME : code == 3 ? CONTENTS_LAVA : CONTENTS_EMPTY;
}

struct ServerSlot
{
    pmove::Input queue[maxQueued];
    int n{0};
    float credit{creditMost};
    bool plain{false};  // this frame's walk runs move by move (VR_PredictThink)
    bool walked{false}; // ... and it did (VR_PredictPhysics: no hanging, climbing or teleport took the frame)
    float botMove[3]{0.f, 0.f, 0.f}; // a bot's stick this frame (botThink: QC's .movevect)
    pmove::MoveNotes notes;          // the walls and floors this frame's moves met (VR_PredictPhysics: a bot's obstruction)
    pmove::Rules rules;
    vec3_t groundNormal{0.f, 0.f, 1.f};
    float groundAge{1e6f};
    float crawl[2]{0.f, 0.f}; // the crawl's own speed after the frame (physics::crawlState's)
    pmove::Grapple grapple;   // the grapple's rope in his moves after the frame (B8j part 4; not on: none) ...
    int grappleHook{0};       // ... its hook's edict (the reel's ramp goes on while it is the same)
    bool slideSeen{false};    // his moves' slide input seen since he joined (B14: the slide's numbers in his state)
    double slideClock{0.0};   // the slide's clock after his last frame's moves (his state's times are against it)
    float staminaLeft{0.f};   // ... his stamina then (a slide's start in his replay: too tired for its first part)
    bool heldNow{false};      // a move of his last frame held to the server's clock (his state's bit: B14)
    // The water's level and type his last move found (the next move's walk or swim goes by them: B11b part 2); not
    // walked last frame: the edict's (the last frame's SV_CheckWater).
    int waterLevel{0}, waterType{CONTENTS_EMPTY};
    double waterNext{-1.0}; // ... the server's time of the frame after the one his moves found them in (a server frame
                            // may skip a host frame or a host frame run two: its own clock, SV_Physics's increment)
};
// seat-shared: the server's, each player slot's
ServerSlot serverSlots[MAX_SCOREBOARD];

// The grapple's rope from QuakeC's fields: the hook's length, reel speed (the stamina's share), payout and taut; the
// player's corner and path beyond, shortest, reeling hands and gun's hand (`rope`: x beyond, y shortest, z the drivers'
// bits 0-1 and the hand plus one in bits 2-3), as VR_Grapple_ServiceAnchor left them for his moves (B8j part 4).
void grappleOf(edict_t* hook, pmove::Grapple& g)
{
    const progs::FieldOffsets& f = progs::fields();
    g.on = true;
    g.len = progs::fieldFloatOr(hook, f.gh_len, 0.f);
    g.speed = progs::fieldFloatOr(hook, f.gh_speed, 0.f) * progs::fieldFloatOr(hook, f.gh_stamina_mult, 1.f);
    g.payout = progs::fieldFloatOr(hook, f.gh_payout, 0.f);
    g.taut = progs::fieldFloatOr(hook, f.gh_taut, 0.f) != 0.f;
}
void grappleStore(edict_t* hook, const pmove::Grapple& g)
{
    progs::setFieldFloat(hook, progs::fields().gh_len, g.len);
    progs::setFieldFloat(hook, progs::fields().gh_taut, g.taut ? 1.f : 0.f);
}
// The hook his moves hold him by this frame (QC's .vr_movegrapple), or null.
[[nodiscard]] edict_t* moveGrappleHook(edict_t* ent)
{
    const int ofs = progs::fields().vr_movegrapple;
    const int num = ofs >= 0 ? progs::fieldInt(ent, ofs) : 0;
    if(num <= 0)
    {
        return nullptr;
    }
    edict_t* hook = PROG_TO_EDICT(num);
    return hook && !hook->free ? hook : nullptr; // (num 0: the world, none)
}

[[nodiscard]] ServerSlot* slotOfEdict(const edict_t* ent)
{
    if(!sv.active || !ent)
    {
        return nullptr;
    }
    const int num = NUM_FOR_EDICT(const_cast<edict_t*>(ent)) - 1;
    return num >= 0 && num < svs.maxclients && num < MAX_SCOREBOARD ? &serverSlots[num] : nullptr;
}

// A client whose moves come one by one: a remote one (QVR_CLC_MOVES; his datagrams carry the state, QVR_SVC_PREDICT).
[[nodiscard]] bool remoteMoves(int slot)
{
    return vr_net_permove.value != 0.f && slot >= 0 && slot < MAX_SCOREBOARD && netmove::serverSends(slot);
}
// This game's own player walked through the moves (vr_pmove_own, B11b): one move a frame, his clc_move's.
[[nodiscard]] bool ownMoves(int slot)
{
    return vr_pmove_own.value != 0.f && slot >= 0 && slot == clientprefs::ownSlot();
}
// A bot walked through the moves (vr_pmove_bots, B11 part 3 part 3): FrikBot's, its slot's entity a player with no
// client; one move a frame, its stick's (botThink).
[[nodiscard]] bool botMoves(int slot)
{
    return vr_pmove_bots.value != 0.f && slot >= 0 && slot < svs.maxclients && !svs.clients[slot].active;
}
// A client whose walk runs move by move: a remote one, this game's own (vr_pmove_own), or a bot (vr_pmove_bots).
[[nodiscard]] bool movesOneByOne(int slot)
{
    return remoteMoves(slot) || ownMoves(slot) || botMoves(slot);
}

// A bot's move this frame (botMoves): its stick (QC's .movevect, FrikBot's key emulation in clc_move's units), its aim
// steering it (the head's and the view's: a bot's are one), its jump key; no hands holding anything (a bot climbs nothing).
[[nodiscard]] pmove::Input botInput(const edict_t* ent, const float* move)
{
    pmove::Input in;
    in.dt = host_frametime;
    in.forward = move[0];
    in.side = move[1];
    in.up = move[2];
    for(int i = 0; i < 3; i++)
    {
        in.steer[i] = ent->v.v_angle[i];
        in.view[i] = ent->v.v_angle[i];
    }
    in.jump = ent->v.button2 != 0.f;
    in.grabs = 0;
    in.headZ = ent->v.view_ofs[2];
    in.head[2] = ent->v.view_ofs[2];
    return in;
}

// The own player's move this frame (ownMoves): his clc_move (host_client->cmd, read this frame) and his VR block (the
// server's copy, server::clientMove) as a remote client's WireMove would carry them, for this server frame's time.
[[nodiscard]] pmove::Input ownInput(edict_t* ent)
{
    netmove::WireMove m;
    m.frameTime = static_cast<float>(host_frametime);
    const usercmd_t& cmd = svs.clients[NUM_FOR_EDICT(ent) - 1].cmd;
    m.viewAngles = glm::vec3{ent->v.v_angle[0], ent->v.v_angle[1], ent->v.v_angle[2]};
    m.forward = static_cast<int>(cmd.forwardmove);
    m.side = static_cast<int>(cmd.sidemove);
    m.up = static_cast<int>(cmd.upmove);
    m.buttons = (ent->v.button0 != 0.f ? 1 : 0) | (ent->v.button2 != 0.f ? 2 : 0);
    if(const VrMove* vr = server::clientMove(ent))
    {
        m.vr = *vr;
    }
    pmove::Input in = inputOf(m, moveguard::unitsPerMetre(units::worldScale())); // (this game's own player, a bot)
    in.dt = host_frametime; // (the frame's own, a double: sv_user.c's and sv_phys.c's rounding, B11b part 2)
    return in;
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
        const trace_t t = SV_Move(const_cast<float*>(start), ent->v.mins, ent->v.maxs, const_cast<float*>(end), MOVE_NORMAL, ent);
        logTrace("box", start, end, t);
        return t;
    }
    trace_t line(const float* start, const float* end) override
    {
        const trace_t t = SV_Move(const_cast<float*>(start), vec3_origin, vec3_origin, const_cast<float*>(end), MOVE_NOMONSTERS, ent);
        logTrace("line", start, end, t);
        return t;
    }
    trace_t traceWorld(const float* start, const float* end) override
    {
        const trace_t t = SV_Move(const_cast<float*>(start), ent->v.mins, ent->v.maxs, const_cast<float*>(end), MOVE_NOMONSTERS, ent);
        logTrace("world", start, end, t);
        return t;
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
    edict_t* grappleHook{nullptr}; // (the hook whose rope holds him: B8j part 4)
    void grappleTaut(int hand, float speed) override
    {
        store();
        progs::grappleTaut(ent, grappleHook, hand, speed); // (the twang, the jolt)
        after();
    }
    float slidePaid(float part) override
    {
        store();
        const float share = progs::staminaEffort(ent, part, "a slide"); // (the pool QuakeC keeps: VR_Stamina_Effort)
        after();
        staminaLeft = progs::staminaLeft(ent);
        return share;
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
    // A headset player's slide (vr_slide, B14): in his moves on the server's numbers (a flatscreen player's is SV_AirMove's).
    r.slide = !hull::isFlat(ent) && !hull::isProne(ent);
    r.slideTuning = slide::tuning();
}

// ---------------------------------------------------------------------------------------------------------------------
// Client

struct Past
{
    za::U32 seq{0};
    vec3_t org{0.f, 0.f, 0.f};
    vec3_t vel{0.f, 0.f, 0.f};    // (B14: his move's result, compared bit for bit with the server's)
    int on{0};                    // the mover he stood on after it (0: none) ...
    vec3_t liftAt{0.f, 0.f, 0.f}; // ... where the replay had it
    bool contact{false};          // this move landed him on a moving mover or one pushed him (B18)
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
        vec3_t at{0.f, 0.f, 0.f}; // (B24) its exact origin at the state
        vec3_t vel{0.f, 0.f, 0.f};
        float left{0.f};
    };
    Mover movers[maxMovers]; // the movers near him in motion (B8e)
    int numMovers{0};
    // (B24) the server's tick, seconds: a mover goes on by one a move in his replay (the server's move buffer runs one of
    // his moves a tick), not by his move's own time (his frames' clock: a few percent off the server's, a lift's step
    // with it); 0: an older state, the move's time
    float serverTick{0.f};
    float holdLeft{0.f};  // .teleport_time less the message's time
    float jumpDir[2]{0.f, 0.f}; // .movedir's x, y (a water jump's push)
    int waterLevel{0}, waterType{CONTENTS_EMPTY}; // the water his last move found (B11b part 2)
    float crawl[2]{0.f, 0.f};   // the crawl's own speed (lying down: B8g)
    slide::Slide slide;         // a headset's slide at the state (B14: the times against the replay's clock, 0 there)
    int slideBits{0};           // ... its record's bits (a snapshot's same record)
    float staminaLeft{1e9f};    // ... his stamina at the state
    float walkLeft{-1.f};       // his room-scale budget at the state (vr_moveguard.hpp; -1 full: review B6)
    pmove::Mantle mantle;      // mantling: its way and time at the state (B8h)
    pmove::Hang hang;           // hanging: the holds, the hands' last places, the owed at the state (B8i)
    climb::TakeRules take;      // the take's rules (not enabled: none) and press memory at the state (B8j part 2)
    climb::TakeMemory takeMem;
    pmove::Grapple grapple;     // the grapple's rope in his moves at the state (B8j part 4; not on: none)
    bool grappleOffNow{false};  // this state has the hook come off that the last one held him by (not predicted)
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
    int contactN{0};                           // (B18: corrections of a move that met a mover, apart) ...
    float contactMax{0.f};                     // ... the longest
    int swimN{0}, wjN{0}, landMovingN{0}, pushN{0}; // (states swimming, in a water jump; replays landing him on a
                                                 // brush model still moving; replayed moves a mover pushed him)
    int crawlN{0};                               // (states lying down: the lying box, B8g)
    int slideN{0};                               // (states sliding: a headset's slide, B14)
    int wclimbN{0};                              // (replays climbing him out of the water by hand: B8j part 3)
    int grappleN{0};                             // (states with the grapple's rope in his moves: B8j part 4) ...
    float grappleMax{0.f};                       // ... the longest correction of one
    int grappleOffN{0};                          // (corrections at the hook's coming off) ...
    float grappleOffMax{0.f};                    // ... the longest
    int mantleN{0};                          // (states mantling: B8h)
    int hangN{0};                                // (states hanging, walked move by move: B8i) ...
    float hangZ0{0.f}, hangRise{0.f};            // ... the first one's height since the reset, the most above it
    int transN{0};                               // (corrections at a hang's mantle starting: B8i) ...
    float transMax{0.f};                         // ... the longest
    float swimMax{0.f}, liftMax{0.f}; // (liftMax: the lift's own predicted place moved under him, a state)
    float corrMax{0.f}, corrSum{0.f}, flatMax{0.f}, airMax{0.f}, errMax{0.f};
    // B14: the determinism of his moves -- his replay's result for the move a new state is after, against the server's,
    // bit for bit (origin; velocity apart: the server's frame may change it after his moves), and FNV-1a hashes of both.
    int exactN{0}, exactMiss{0}, velMiss{0}, exactHeld{0};
    float exactMax{0.f}; // (the farthest a result missed by)
    za::U64 hashSv{14695981039346656037ull}, hashCl{14695981039346656037ull};

    // vr_mp_predict_probe.
    bool probing{false};
    char probeLabel[32]{};
    int probeFrame{0};
    za::U32 probeMove{0}; // the newest move sent when armed
    za::U32 jumpTraced{0}; // vr_mp_watch: the move whose replayed jump was printed last
    double probeTime{0.0};
    vec3_t probeOrg{0.f, 0.f, 0.f};
};
struct SeatPredict // the seat's (seat::Part::Predict; splitscreen phase 5)
{
    ClientPredict cp;
};
SeatPredict& seatPredict()
{
    return qvr::seat::part<SeatPredict>(qvr::seat::current(), qvr::seat::Part::Predict);
}

// (B13) The corrections since the game started and their lengths' sum (never reset: the net HUD's rate over its window);
// only those of half a unit or more (a state's replay off by less is not seen: most states' are 0 to a few hundredths)
constexpr float hudCorrectionMin = 0.5f;
struct CorrectionTotals
{
    za::U64 n{0};
    double sum{0.0};
};
struct SeatPredictTotals // the seat's (seat::Part::PredictTotals; splitscreen phase 5)
{
    CorrectionTotals corrTotals;
};
SeatPredictTotals& seatPredictTotals()
{
    return qvr::seat::part<SeatPredictTotals>(qvr::seat::current(), qvr::seat::Part::PredictTotals);
}

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
    for(int k = 0; k < seatPredict().cp.numMovers; k++)
    {
        if(seatPredict().cp.movers[k].num == num)
        {
            VectorCopy(seatPredict().cp.movers[k].vel, out);
            return seatPredict().cp.movers[k].left;
        }
    }
    VectorCopy(vec3_origin, out);
    return 0.f;
}

// ... where it will be `t` seconds of the server's ticks after the newest message (B8e: a lift or door goes on to its
// stop; the server runs a move before the movers of its frame, so the replay's first move meets them where the message
// has them). (B24) From its exact place in the state (the entity update's is to 1/16 unit: a replay's lift up to 1/32
// off, his place on it with it).
void moverAt(int num, float t, vec3_t out)
{
    for(int k = 0; k < seatPredict().cp.numMovers; k++)
    {
        if(seatPredict().cp.movers[k].num == num)
        {
            VectorMA(seatPredict().cp.movers[k].at, q_min(t, seatPredict().cp.movers[k].left), seatPredict().cp.movers[k].vel, out);
            return;
        }
    }
    VectorCopy(cl_entities[num].msg_origins[0], out);
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
        const bool narrow = (seatPredict().cp.bits & bitHullBrushModels) != 0;
        vec3_t s, t;
        VectorSubtract(start, at, s);
        VectorSubtract(end, at, t);
        trace_t tr;
        memset(&tr, 0, sizeof(tr));
        tr.fraction = 1.f;
        tr.allsolid = true;
        VectorCopy(end, tr.endpos);
        if(!narrow || !hull::clientPlayerTrace(cl.worldmodel, e.model, seatPredict().cp.hullWidth, seatPredict().cp.rules.boxHeight, s, t, tr))
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
        for(const int k : worldtrace::brushModelEntities()) // (the message's brush models: mpreview B3)
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
                pushedBy = k;
            }
            for(int i = 0; i < 3; i++)
            {
                origin[i] += now[i] - was[i];
            }
        }
        return on;
    }
    bool pushedNow{false}; // (vr_mp_watch's trace)
    int pushedBy{0};       // (B18) ... the mover that pushed him in this move

    trace_t trace(const float* start, const float* end) override
    {
        trace_t best;
        memset(&best, 0, sizeof(best));
        best.fraction = 1.f;
        best.allsolid = true;
        VectorCopy(end, best.endpos);
        if(!hull::clientPlayerTrace(cl.worldmodel, cl.worldmodel, seatPredict().cp.hullWidth, seatPredict().cp.rules.boxHeight, start, end, best))
        {
            hull_t* hull = &cl.worldmodel->hulls[1];
            SV_RecursiveHullCheck(hull, hull->firstclipnode, 0.f, 1.f, const_cast<float*>(start), const_cast<float*>(end), &best);
        }
        hit = 0;
        for(const int k : worldtrace::brushModelEntities()) // (the message's brush models: mpreview B3)
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
        logTrace("box", start, end, best);
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
        for(const int k : worldtrace::brushModelEntities()) // (the message's brush models: mpreview B3)
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
        logTrace("line", start, end, best);
        return best;
    }
    bool overDropoff(const float* vel, float speed) override
    {
        // (VR_HullOverDropoff: a point at the box's leading edge, its feet, finds no floor within 34 units; with
        // vr_hull_edge_probe the narrowed box's edge, and a point in solid has floor)
        const bool fix = (seatPredict().cp.bits & bitEdgeProbe) != 0;
        const float edge = fix && seatPredict().cp.hullWidth > 0.f ? q_min(16.f, za::clamp(seatPredict().cp.hullWidth, 8.f, 32.f) * 0.5f) : 16.f;
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
        for(const int k : worldtrace::brushModelEntities())
        {
            if(fraction != 1.f)
            {
                break;
            }
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
    return vr_net_predict.value != 0.f && seatPredict().cp.have && (seatPredict().cp.bits & bitWalked) && netmove::clientUses() &&
           cls.signon == SIGNONS && !cls.demoplayback && cl.worldmodel && cl.viewentity > 0 &&
           cl.viewentity < cl.num_entities;
}

[[nodiscard]] float dist(const float* a, const float* b)
{
    vec3_t d;
    VectorSubtract(a, b, d);
    return VectorLength(d);
}

// FNV-1a over a vector's bytes (B14's move results).
[[nodiscard]] za::U64 hashOf(za::U64 h, const float* v)
{
    unsigned char bytes[sizeof(vec3_t)];
    memcpy(bytes, v, sizeof(bytes));
    for(const unsigned char c : bytes)
    {
        h = (h ^ c) * 1099511628211ull;
    }
    return h;
}

// The moves after the server's state replayed: the predicted origin (false: the moves are gone from the ring).
bool replay(vec3_t out, vec3_t before)
{
    ClientBody b;
    VectorCopy(seatPredict().cp.origin, b.origin);
    VectorCopy(seatPredict().cp.velocity, b.velocity);
    b.flags = (seatPredict().cp.bits & bitOnGround ? FL_ONGROUND : 0) | (seatPredict().cp.bits & bitWaterJump ? FL_WATERJUMP : 0) |
              (seatPredict().cp.bits & bitJumpReleased ? FL_JUMPRELEASED : 0);
    VectorCopy(seatPredict().cp.groundNormal, b.groundNormal);
    b.groundAge = seatPredict().cp.groundAge;
    b.holdLeft = seatPredict().cp.holdLeft;
    b.jumpDir[0] = seatPredict().cp.jumpDir[0];
    b.jumpDir[1] = seatPredict().cp.jumpDir[1];
    b.waterLevel = b.thinkLevel = seatPredict().cp.waterLevel;
    b.waterType = seatPredict().cp.waterType;
    b.crawl[0] = seatPredict().cp.crawl[0];
    b.crawl[1] = seatPredict().cp.crawl[1];
    b.slide = seatPredict().cp.slide; // (his moves slide as on the server: B14)
    b.slideClock = 0.0;
    b.staminaLeft = seatPredict().cp.staminaLeft;
    b.mantle = seatPredict().cp.mantle;
    b.hang = seatPredict().cp.hang;
    b.grapple = seatPredict().cp.grapple; // (the grapple's rope holds and reels him as on the server: B8j part 4)
    climb::TakeRules take = seatPredict().cp.take; // (his moves take holds as the server's do: B8j part 2)
    climb::TakeMemory takeMem = seatPredict().cp.takeMem;
    const za::U32 newest = netmove::clientNewest();
    if(newest < seatPredict().cp.seq || newest - seatPredict().cp.seq >= static_cast<za::U32>(pastRing))
    {
        return false;
    }
    if(seatPredict().cp.fresh)
    {
        seatPredict().cp.swimN += seatPredict().cp.waterLevel >= 2 ? 1 : 0;
        seatPredict().cp.crawlN += seatPredict().cp.bits & bitLying ? 1 : 0;
    }
    pmove::MoveNotes notes;
    netmove::WireMove m;
    bool landedMoving = false; // (a move landed him on a lift or door still moving)
    int depth = 0;
    float lastDt = 0.f;
    float walkLeft = seatPredict().cp.walkLeft; // (his room-scale budget, the server's at the state: his moves cut as there, review B6)
    VectorCopy(b.origin, before);
    for(za::U32 s = seatPredict().cp.seq + 1; s <= newest; s++)
    {
        if(!netmove::clientMoveAt(s, m))
        {
            return false;
        }
        VectorCopy(b.origin, before);
        b.jumpedNow = false;
        const float upm = moveguard::unitsPerMetre(units::worldScale());
        pmove::Input in = inputOf(m, upm);
        (void)moveguard::budget(walkLeft, in.roomscale, in.dt, upm);
        const bool inAir = !(b.flags & FL_ONGROUND);
        const bool hangWas = b.hang.on;
        b.climbedNow = false;
        traceLog.now = traceLog.seq != 0 && s == traceLog.seq;
        traceLog.side = "cl";
        if(traceLog.now)
        {
            Con_Printf("mp_tracelog: cl move=%u o=%.9g %.9g %.9g v=%.9g %.9g %.9g fl=%d dt=%.9g\n", s, b.origin[0],
                b.origin[1], b.origin[2], b.velocity[0], b.velocity[1], b.velocity[2], b.flags, in.dt);
        }
        pmove::playerMove(b, in, seatPredict().cp.rules, &notes);
        if(traceLog.now)
        {
            Con_Printf("mp_tracelog: cl after o=%.9g %.9g %.9g v=%.9g %.9g %.9g\n", b.origin[0], b.origin[1], b.origin[2],
                b.velocity[0], b.velocity[1], b.velocity[2]);
        }
        traceLog.now = false;
        if(b.climbedNow && seatPredict().cp.fresh)
        {
            seatPredict().cp.wclimbN++;
            if(vr_mp_watch.value != 0.f)
            {
                Con_Printf("mp_predict_wclimb: replay s=%u seq=%u z=%.2f vz=%.1f\n", s, seatPredict().cp.seq, b.origin[2], b.velocity[2]);
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
            if(b.hang.on && !hangBefore && seatPredict().cp.fresh && vr_mp_watch.value != 0.f)
            {
                Con_Printf("mp_predict_take: replay s=%u seq=%u z=%.2f\n", s, seatPredict().cp.seq, b.origin[2]);
            }
        }
        // The lift under him goes on after his move, carrying him; one moving into him pushes him.
        vec3_t liftAt;
        b.pushedNow = false;
        b.pushedBy = 0;
        const float step = seatPredict().cp.serverTick > 0.f ? seatPredict().cp.serverTick : in.dt; // (B24: the movers go on by the server's tick)
        const int on = b.moversAfter(step, liftAt);
        seatPredict().cp.pushN += b.pushedNow && seatPredict().cp.fresh ? 1 : 0;
        vec3_t onVel;
        landedMoving = landedMoving || (inAir && on > 0 && moverVel(on, onVel) > b.moveTime && VectorLength(onVel) > 0.f);
        lastDt = step;
        b.moveTime += step;
        if(b.jumpedNow && s != seatPredict().cp.jumpTraced && vr_mp_watch.value != 0.f)
        {
            seatPredict().cp.jumpTraced = s; // (each move's jump once: a replay that moves it to another move prints that one)
            Con_Printf("mp_predict_jump: s=%u seq=%u bits=%d z=%.2f\n", s, seatPredict().cp.seq, seatPredict().cp.bits, seatPredict().cp.origin[2]);
        }
        if(vr_mp_watch.value >= 2.f && seatPredict().cp.fresh)
        {
            vec3_t gv;
            const float gl = b.ground > 0 ? moverVel(b.ground, gv) : 0.f;
            Con_Printf("mp_predict_m: s=%u seq=%u y=%.2f z=%.2f vz=%.1f fl=%d dt=%.4f box=%.1f on=%d ground=%d lift=%.2f "
                "left=%.3f t=%.4f nm=%d\n", s, seatPredict().cp.seq, b.origin[1], b.origin[2], b.velocity[2], b.flags, in.dt,
                seatPredict().cp.rules.boxHeight, on, b.ground, liftAt[2], gl, b.moveTime, seatPredict().cp.numMovers);
        }
        Past& p = seatPredict().cp.cur[s % pastRing];
        p.seq = s;
        VectorCopy(b.origin, p.org);
        VectorCopy(b.velocity, p.vel);
        p.on = on;
        VectorCopy(liftAt, p.liftAt);
        // (B18) his move's meeting with a mover: landed on one moving or pushed by one (see the correction's use)
        p.contact = (inAir && on > 0 && VectorLength(onVel) > 0.f) || b.pushedBy > 0;
        depth++;
    }
    VectorCopy(b.origin, out);
    seatPredict().cp.depthSum += depth;
    const int ridden = (b.flags & FL_ONGROUND) && b.ground < cl.num_entities ? b.ground : 0;
    // The lift he stands on, drawn where his replay has it at his newest move (B8e: it goes on in the replay, carrying
    // him; B8d's carry between states, its newest place since the last, is that replay's own now).
    const bool liftMoved = ridden > 0 && ridden == seatPredict().cp.ridden && !VectorCompare(cl_entities[ridden].msg_origins[0], seatPredict().cp.riddenAt);
    seatPredict().cp.ridden = ridden;
    VectorCopy(vec3_origin, seatPredict().cp.riddenVel);
    if(ridden > 0)
    {
        VectorCopy(cl_entities[ridden].msg_origins[0], seatPredict().cp.riddenAt);
        seatPredict().cp.riddenLeft = moverVel(ridden, seatPredict().cp.riddenVel);
        seatPredict().cp.riddenAfter = b.moveTime;
        seatPredict().cp.riddenBefore = b.moveTime - lastDt;
    }
    seatPredict().cp.landMovingN += seatPredict().cp.fresh && landedMoving ? 1 : 0;

    if(seatPredict().cp.fresh)
    {
        // His move's result against the server's (B14): the last replay's for the move this state is after, bit for bit.
        const Past& mine = seatPredict().cp.past[seatPredict().cp.seq % pastRing];
        seatPredict().cp.exactHeld += seatPredict().cp.slideBits & 128 ? 1 : 0; // (a move held to the server's clock: his time, not his moves')
        if(mine.seq == seatPredict().cp.seq && (seatPredict().cp.bits & bitWalked) && !(seatPredict().cp.slideBits & 128))
        {
            const bool same = memcmp(mine.org, seatPredict().cp.origin, sizeof(vec3_t)) == 0;
            seatPredict().cp.exactN += same ? 1 : 0;
            seatPredict().cp.exactMiss += same ? 0 : 1;
            seatPredict().cp.exactMax = q_max(seatPredict().cp.exactMax, dist(mine.org, seatPredict().cp.origin));
            if(!same && vr_mp_watch.value != 0.f)
            {
                Con_Printf("mp_predict_exact_miss: seq=%u mine=%.6f %.6f %.6f sv=%.6f %.6f %.6f states=%d\n", seatPredict().cp.seq,
                    mine.org[0], mine.org[1], mine.org[2], seatPredict().cp.origin[0], seatPredict().cp.origin[1], seatPredict().cp.origin[2], seatPredict().cp.states);
            }
            seatPredict().cp.velMiss += memcmp(mine.vel, seatPredict().cp.velocity, sizeof(vec3_t)) == 0 ? 0 : 1;
            seatPredict().cp.hashSv = hashOf(seatPredict().cp.hashSv, seatPredict().cp.origin);
            seatPredict().cp.hashCl = hashOf(seatPredict().cp.hashCl, mine.org);
        }
        // The correction: the newest move both this replay and the last one predicted, where each put him.
        for(za::U32 s = newest; s > seatPredict().cp.seq; s--)
        {
            Past was = seatPredict().cp.past[s % pastRing];
            const Past& now = seatPredict().cp.cur[s % pastRing];
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
                seatPredict().cp.liftMax = q_max(seatPredict().cp.liftMax, VectorLength(lift));
            }
            vec3_t rel;
            for(int i = 0; i < 3; i++)
            {
                rel[i] = was.org[i] - now.org[i] - lift[i];
            }
            const float c = VectorLength(rel);
            if(c > (vr_mp_watch.value >= 3.f ? 0.05f : 1.f) && vr_mp_watch.value != 0.f)
            {
                Con_Printf("mp_predict_corr: c=%.2f s=%u seq=%u newest=%u bits=%d was=%.2f %.2f %.2f now=%.2f %.2f %.2f sv=%.2f %.2f %.2f vel=%.1f %.1f %.1f notes=%d%d%d\n", c, s, seatPredict().cp.seq, newest, seatPredict().cp.bits, was.org[0], was.org[1], was.org[2], now.org[0], now.org[1], now.org[2], seatPredict().cp.origin[0], seatPredict().cp.origin[1], seatPredict().cp.origin[2], seatPredict().cp.velocity[0], seatPredict().cp.velocity[1], seatPredict().cp.velocity[2], notes.wall ? 1 : 0, notes.stepped ? 1 : 0, notes.air ? 1 : 0);
            }
            if(seatPredict().cp.transNow || seatPredict().cp.grappleOffNow)
            {
                // (the hang's mantle begun by the server's test, once a frame: the replay hung on until it heard; the
                // grapple's hook come off, B8j part 4: the server's trigger, quick release or loosening)
                if(seatPredict().cp.grappleOffNow)
                {
                    seatPredict().cp.grappleOffN++;
                    seatPredict().cp.grappleOffMax = q_max(seatPredict().cp.grappleOffMax, c);
                }
                else
                {
                    seatPredict().cp.transN++;
                    seatPredict().cp.transMax = q_max(seatPredict().cp.transMax, c);
                }
                if(c < snapAt)
                {
                    for(int i = 0; i < 3; i++)
                    {
                        seatPredict().cp.error[i] += was.org[i] - now.org[i];
                        seatPredict().cp.transError[i] += was.org[i] - now.org[i];
                    }
                }
                break;
            }
            seatPredict().cp.corrN++;
            seatPredict().cp.corrSum += c;
            if(c >= hudCorrectionMin)
            {
                seatPredictTotals().corrTotals.n++;
                seatPredictTotals().corrTotals.sum += c;
            }
            // (B18) The move that met a mover (landed on it moving, or pushed by it) in either replay: the server runs
            // the movers once a tick and his moves as they come, so one of his moves bunched with another in a tick
            // (or a tick without one) inside the unacknowledged window puts the mover a step off where the replay met
            // it -- a land against a push, up to one step of it (2.08 units for e1m1's lift at 72 Hz). Measured apart.
            const bool contact = was.contact || now.contact;
            if(contact)
            {
                seatPredict().cp.contactN++;
                seatPredict().cp.contactMax = q_max(seatPredict().cp.contactMax, c);
            }
            seatPredict().cp.corrMax = contact ? seatPredict().cp.corrMax : q_max(seatPredict().cp.corrMax, c);
            if(seatPredict().cp.grapple.on)
            {
                seatPredict().cp.grappleMax = q_max(seatPredict().cp.grappleMax, c); // (the grapple's rope in his moves: B8j part 4)
            }
            const bool flat = !notes.wall && !notes.stepped && !notes.air && !notes.slope && (seatPredict().cp.bits & bitOnGround);
            if(flat)
            {
                seatPredict().cp.flatN++;
                seatPredict().cp.flatMax = q_max(seatPredict().cp.flatMax, c);
            }
            if(notes.air)
            {
                seatPredict().cp.airN++;
                seatPredict().cp.airMax = contact ? seatPredict().cp.airMax : q_max(seatPredict().cp.airMax, c);
            }
            if(liftMoved)
            {
                seatPredict().cp.rideN++;
            }
            if(notes.swim)
            {
                seatPredict().cp.swimMax = q_max(seatPredict().cp.swimMax, c);
            }
            if(c >= snapAt)
            {
                VectorCopy(vec3_origin, seatPredict().cp.error);
                VectorCopy(vec3_origin, seatPredict().cp.liftError);
                VectorCopy(vec3_origin, seatPredict().cp.transError);
            }
            else
            {
                for(int i = 0; i < 3; i++)
                {
                    seatPredict().cp.error[i] += was.org[i] - now.org[i];
                    seatPredict().cp.liftError[i] += lift[i];
                }
            }
            break;
        }
        seatPredict().cp.fresh = false;
        seatPredict().cp.transNow = seatPredict().cp.grappleOffNow = false;
    }
    memcpy(seatPredict().cp.past, seatPredict().cp.cur, sizeof(seatPredict().cp.past));
    return true;
}

void probeCheck(const float* origin)
{
    if(!seatPredict().cp.probing || dist(origin, seatPredict().cp.probeOrg) <= 0.05f)
    {
        return;
    }
    seatPredict().cp.probing = false;
    Con_Printf("mp_predict_moved: label=%s moves=%u frames=%d ms=%d predicted=%d\n", seatPredict().cp.probeLabel, netmove::clientNewest() - seatPredict().cp.probeMove, host_framecount - seatPredict().cp.probeFrame,
        static_cast<int>((realtime - seatPredict().cp.probeTime) * 1000.0), seatPredict().cp.drawing ? 1 : 0);
}

void probe_f()
{
    q_strlcpy(seatPredict().cp.probeLabel, Cmd_Argc() > 1 ? Cmd_Argv(1) : "-", sizeof(seatPredict().cp.probeLabel));
    if(cls.signon != SIGNONS || cl.viewentity <= 0 || cl.viewentity >= cl.num_entities)
    {
        Con_Printf("vr_mp_predict_probe: not in a game\n");
        return;
    }
    seatPredict().cp.probing = true;
    seatPredict().cp.probeFrame = host_framecount;
    seatPredict().cp.probeMove = netmove::clientNewest();
    seatPredict().cp.probeTime = realtime;
    VectorCopy(cl_entities[cl.viewentity].origin, seatPredict().cp.probeOrg);
}

} // namespace

pmove::Input inputOf(const netmove::WireMove& m, float upm)
{
    pmove::Input in;
    in.dt = static_cast<double>(m.frameTime > 0.f ? (m.frameTime < moveMost ? m.frameTime : moveMost) : 0.f);
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
        // (the integers sent: the server's exactly; bounded as the server bounds them, vr_moveguard.hpp)
        const glm::vec3 rel = moveguard::relative(m.vr.hands[h].pos, m.vr.origin, upm);
        in.hands[h][0] = rel.x;
        in.hands[h][1] = rel.y;
        in.hands[h][2] = rel.z;
    }
    in.grabs = ((m.vr.vrBits0 & (1 << 1)) ? 1 : 0) | ((m.vr.vrBits0 & (1 << 3)) ? 2 : 0); // (vr_climb.cpp's grabBit)
    const glm::vec3 head = moveguard::relative(m.vr.headPos, m.vr.origin, upm); // (as the hands)
    in.headZ = head.z;
    for(int h = 0; h < 2; h++) // (a hold's take, B8j part 2: the same numbers on the server and in his replay)
    {
        const glm::vec3 tracked = moveguard::relative(m.vr.hands[h].tracked, m.vr.origin, upm);
        const glm::vec3 throwVel = moveguard::speed(m.vr.hands[h].throwVel);
        const glm::vec3 handVel = moveguard::speed(m.vr.hands[h].vel);
        for(int i = 0; i < 3; i++)
        {
            in.tracked[h][i] = tracked[i];
            in.throwVel[h][i] = throwVel[i];
            in.handVel[h][i] = handVel[i]; // (the water climb's pull, B8j part 3)
        }
        in.hotspots[h] = m.vr.hotspots[h];
    }
    for(int i = 0; i < 3; i++)
    {
        in.head[i] = head[i];
    }
    in.buttons = m.vr.buttons;
    in.reels = (m.vr.buttons & protocol::QVR_BUTTON_OFFHANDSECONDARY ? 1 : 0) |
               (m.vr.buttons & protocol::QVR_BUTTON_MAINHANDSECONDARY ? 2 : 0); // (the grapple's reel, B8j part 4)
    in.slide = (m.vr.vrBits0 & protocol::moveSlideBit) != 0; // (vr_slide's gesture: a headset's slide, B14)
    return in;
}

void serverQueue(client_t* client, const netmove::WireMove& m)
{
    const int slot = static_cast<int>(client - svs.clients);
    if(!remoteMoves(slot))
    {
        return;
    }
    ServerSlot& s = serverSlots[slot];
    if(s.n == maxQueued)
    {
        memmove(&s.queue[0], &s.queue[1], sizeof(s.queue[0]) * (maxQueued - 1));
        s.n--;
    }
    pmove::Input in = inputOf(m, moveguard::unitsPerMetre(moveguard::slotScale(slot)));
    moveguard::roomscale(slot, in.roomscale, in.dt, moveguard::unitsPerMetre(moveguard::slotScale(slot))); // (his budget)
    s.queue[s.n++] = in;
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
    const float walkLeft = MSG_ReadFloat(); // (his room-scale budget after the move: review B6)
    // A headset's slide (B14): its state, the server's numbers once his slide input reached it, its times.
    const int slideBits = MSG_ReadByte();
    slide::Slide sl;
    sl.on = (slideBits & 1) != 0;
    sl.held = (slideBits & 2) != 0;
    sl.grounded = (slideBits & 4) != 0;
    sl.airborne = (slideBits & 8) != 0;
    sl.lastZ = o[2];
    slide::Tuning slideTuning = slide::cvarTuning(); // (none sent yet: his own settings', no stamina)
    float staminaLeft = 1e9f;
    if(slideBits & 32)
    {
        sl.lastZ = MSG_ReadFloat();
        staminaLeft = MSG_ReadFloat();
        for(float* f : {&slideTuning.least, &slideTuning.top, &slideTuning.boost, &slideTuning.end, &slideTuning.slope,
                &slideTuning.steer, &slideTuning.friction, &slideTuning.cooldown, &slideTuning.staminaPart,
                &slideTuning.staminaStep})
        {
            *f = MSG_ReadFloat();
        }
        slideTuning.staminaParts = MSG_ReadByte();
    }
    if(slideBits & 64)
    {
        sl.partsPaid = MSG_ReadByte();
        sl.cooldownUntil = MSG_ReadFloat(); // (his replay's slide clock starts at 0 at the state)
        sl.startedAt = -MSG_ReadFloat();
    }
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
    pmove::Grapple grapple; // (not on: no grapple block; B8j part 4)
    if(climbBits & 8)
    {
        const int gb = MSG_ReadByte();
        grapple.on = true;
        grapple.drivers = gb & 3;
        grapple.hand = ((gb >> 2) & 3) - 1;
        grapple.taut = (gb & 16) != 0;
        for(float* f : {&grapple.at[0], &grapple.at[1], &grapple.at[2], &grapple.beyond, &grapple.len, &grapple.speed,
                &grapple.reelAge, &grapple.shortest, &grapple.payout})
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
            movers[k].at[i] = MSG_ReadFloat();
        }
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
    const float serverTick = MSG_ReadFloat(); // (B24)
    if(msg_badread || (seatPredict().cp.have && seq < seatPredict().cp.seq))
    {
        return; // (an older one: the snapshot's base kept past the room)
    }
    // (A snapshot parses its unchanged records again: the same state is not a new one.)
    if(seatPredict().cp.have && seq == seatPredict().cp.seq && bits == seatPredict().cp.bits && VectorCompare(o, seatPredict().cp.origin) && VectorCompare(v, seatPredict().cp.velocity) &&
        crawl[0] == seatPredict().cp.crawl[0] && crawl[1] == seatPredict().cp.crawl[1] && grapple.len == seatPredict().cp.grapple.len && slideBits == seatPredict().cp.slideBits && sl.lastZ == seatPredict().cp.slide.lastZ &&
        memcmp(hang.owed, seatPredict().cp.hang.owed, sizeof(hang.owed)) == 0 &&
        memcmp(hang.grips[0].lastRel, seatPredict().cp.hang.grips[0].lastRel, sizeof(hang.grips[0].lastRel)) == 0 &&
        memcmp(hang.grips[1].lastRel, seatPredict().cp.hang.grips[1].lastRel, sizeof(hang.grips[1].lastRel)) == 0)
    {
        return;
    }
    r.slopeWalk = (bits & bitSlopeWalk) != 0;
    r.nostep = (bits & bitNoStep) != 0;
    r.waterDrag = (bits & bitWaterDrag) != 0;
    r.waterJumpOut = (bits & bitWaterJumpOut) != 0;
    r.roomscaleOn = (bits & bitRoomscale) != 0;
    r.crawl = (bits & bitCrawl) != 0;
    r.slide = (slideBits & 16) != 0; // (a headset's slide in his moves: B14)
    r.slideTuning = slideTuning;
    seatPredict().cp.slide = sl;
    seatPredict().cp.slideBits = slideBits;
    seatPredict().cp.staminaLeft = staminaLeft;
    seatPredict().cp.walkLeft = walkLeft;
    seatPredict().cp.slideN += sl.on ? 1 : 0;
    r.waterClimb = (climbBits & 1) != 0;
    r.climbHands = (climbBits >> 1) & 3;
    seatPredict().cp.have = true;
    seatPredict().cp.crawl[0] = crawl[0];
    seatPredict().cp.crawl[1] = crawl[1];
    seatPredict().cp.mantle = mantle;
    seatPredict().cp.mantleN += mantle.on ? 1 : 0;
    // (a hold taken, or the hang's mantle begun: the server's searches, once a frame: the replay learns of them now)
    // (B19: until a replay takes it -- two states read in one frame, the second already hanging, lost the take's)
    seatPredict().cp.transNow = seatPredict().cp.transNow || (mantle.on && seatPredict().cp.hang.on) || (hang.on && !seatPredict().cp.hang.on);
    seatPredict().cp.hang = hang;
    seatPredict().cp.take = take;
    seatPredict().cp.takeMem = takeMem;
    seatPredict().cp.grappleOffNow = seatPredict().cp.grappleOffNow || (seatPredict().cp.grapple.on && !grapple.on); // (the hook come off: the server's, once a frame)
    seatPredict().cp.grapple = grapple;
    seatPredict().cp.grappleN += grapple.on ? 1 : 0;
    if(hang.on && (bits & bitWalked))
    {
        seatPredict().cp.hangZ0 = seatPredict().cp.hangN == 0 ? o[2] : seatPredict().cp.hangZ0;
        seatPredict().cp.hangN++;
        seatPredict().cp.hangRise = q_max(seatPredict().cp.hangRise, o[2] - seatPredict().cp.hangZ0);
    }
    seatPredict().cp.holdLeft = q_max(static_cast<float>(teleportTime - cl.mtime[0]), -1.f);
    seatPredict().cp.jumpDir[0] = jumpDir[0];
    seatPredict().cp.jumpDir[1] = jumpDir[1];
    seatPredict().cp.waterLevel = (climbBits >> 4) & 3; // (his next move's walk or swim goes by them: B11b part 2)
    seatPredict().cp.waterType = waterOfCode((climbBits >> 6) & 3);
    seatPredict().cp.numMovers = numMovers;
    memcpy(seatPredict().cp.movers, movers, sizeof(movers));
    seatPredict().cp.serverTick = serverTick > 0.f && serverTick < 0.1f ? serverTick : 0.f;
    if(bits & bitWaterJump)
    {
        seatPredict().cp.wjN++;
    }
    seatPredict().cp.seq = seq;
    seatPredict().cp.bits = bits;
    VectorCopy(o, seatPredict().cp.origin);
    VectorCopy(v, seatPredict().cp.velocity);
    VectorCopy(n, seatPredict().cp.groundNormal);
    seatPredict().cp.groundAge = age;
    seatPredict().cp.rules = r;
    seatPredict().cp.hullWidth = width;
    seatPredict().cp.fresh = true;
    seatPredict().cp.states++;
}

void clientClear()
{
    const ClientPredict keep = seatPredict().cp; // (the report's counts and a probe span connections)
    seatPredict().cp = ClientPredict{};
    seatPredict().cp.states = keep.states;
    seatPredict().cp.frames = keep.frames;
    seatPredict().cp.unpredicted = keep.unpredicted;
    seatPredict().cp.unWalked = keep.unWalked;
    seatPredict().cp.depthSum = keep.depthSum;
    seatPredict().cp.corrN = keep.corrN;
    seatPredict().cp.flatN = keep.flatN;
    seatPredict().cp.corrMax = keep.corrMax;
    seatPredict().cp.corrSum = keep.corrSum;
    seatPredict().cp.flatMax = keep.flatMax;
    seatPredict().cp.airN = keep.airN;
    seatPredict().cp.rideN = keep.rideN;
    seatPredict().cp.airMax = keep.airMax;
    seatPredict().cp.contactN = keep.contactN;
    seatPredict().cp.contactMax = keep.contactMax;
    seatPredict().cp.errMax = keep.errMax;
    seatPredict().cp.swimN = keep.swimN;
    seatPredict().cp.swimMax = keep.swimMax;
    seatPredict().cp.liftMax = keep.liftMax;
    seatPredict().cp.wjN = keep.wjN;
    seatPredict().cp.landMovingN = keep.landMovingN;
    seatPredict().cp.pushN = keep.pushN;
    seatPredict().cp.crawlN = keep.crawlN;
    seatPredict().cp.slideN = keep.slideN;
    seatPredict().cp.exactN = keep.exactN;
    seatPredict().cp.exactMiss = keep.exactMiss;
    seatPredict().cp.velMiss = keep.velMiss;
    seatPredict().cp.exactHeld = keep.exactHeld;
    seatPredict().cp.exactMax = keep.exactMax;
    seatPredict().cp.hashSv = keep.hashSv;
    seatPredict().cp.hashCl = keep.hashCl;
    seatPredict().cp.mantleN = keep.mantleN;
    seatPredict().cp.hangN = keep.hangN;
    seatPredict().cp.hangZ0 = keep.hangZ0;
    seatPredict().cp.hangRise = keep.hangRise;
    seatPredict().cp.transN = keep.transN;
    seatPredict().cp.transMax = keep.transMax;
    seatPredict().cp.wclimbN = keep.wclimbN;
    seatPredict().cp.grappleN = keep.grappleN;
    seatPredict().cp.grappleMax = keep.grappleMax;
}

namespace
{
// Forgotten when the client's state is cleared (vr_mem.hpp's resets): no state from the server: his own walk drawn as it comes until one, B8.
// seat-shared: the reset hook (it clears the current seat's)
mem::Reset clientStateReset{"predict", mem::ResetEvent::ClientState, mem::ResetPhase::State, &clientClear};
} // namespace

void correctionTotals(za::U64& n, double& sum)
{
    n = seatPredictTotals().corrTotals.n;
    sum = seatPredictTotals().corrTotals.sum;
}

void report()
{
    Con_Printf("mp_predict: on=%d plain=%d states=%d frames=%d unpredicted=%d unwalked=%d depth_mean=%.1f corr_n=%d "
               "corr_max=%.3f corr_mean=%.3f flat_n=%d flat_max=%.3f air_n=%d air_max=%.3f ride_n=%d err=%.3f "
               "err_max=%.3f maxspeed=%.1f box=%.1f jump=%.0f swim_n=%d swim_max=%.3f wj_n=%d land_moving=%d "
               "lift_max=%.3f pushed=%d lying=%d arms=%d crawl_n=%d slide_n=%d mantle_n=%d hang_n=%d hang_rise=%.2f trans_n=%d trans_max=%.3f "
               "wclimb=%d wclimb_n=%d grapple=%d grapple_n=%d grapple_max=%.3f grapple_off_n=%d grapple_off_max=%.3f "
               "exact_n=%d exact_miss=%d exact_max=%.6f vel_miss=%d exact_held=%d contact_n=%d contact_max=%.3f "
               "hash_sv=%016llx hash_cl=%016llx seq=%u newest=%u\n",
        vr_net_predict.value != 0.f && netmove::clientUses() ? 1 : 0, seatPredict().cp.bits & bitWalked ? 1 : 0, seatPredict().cp.states, seatPredict().cp.frames,
        seatPredict().cp.unpredicted, seatPredict().cp.unWalked, seatPredict().cp.frames > 0 ? static_cast<double>(seatPredict().cp.depthSum) / seatPredict().cp.frames : 0.0, seatPredict().cp.corrN, seatPredict().cp.corrMax,
        seatPredict().cp.corrN > 0 ? seatPredict().cp.corrSum / seatPredict().cp.corrN : 0.f, seatPredict().cp.flatN, seatPredict().cp.flatMax, seatPredict().cp.airN, seatPredict().cp.airMax, seatPredict().cp.rideN, VectorLength(seatPredict().cp.error),
        seatPredict().cp.errMax, seatPredict().cp.rules.maxspeed, seatPredict().cp.rules.boxHeight, seatPredict().cp.rules.jumpSpeed, seatPredict().cp.swimN, seatPredict().cp.swimMax, seatPredict().cp.wjN,
        seatPredict().cp.landMovingN, seatPredict().cp.liftMax, seatPredict().cp.pushN, seatPredict().cp.bits & bitLying ? 1 : 0, seatPredict().cp.rules.crawl ? 1 : 0, seatPredict().cp.crawlN, seatPredict().cp.slideN, seatPredict().cp.mantleN, seatPredict().cp.hangN, seatPredict().cp.hangRise, seatPredict().cp.transN, seatPredict().cp.transMax,
        seatPredict().cp.rules.waterClimb ? 1 + seatPredict().cp.rules.climbHands : 0, seatPredict().cp.wclimbN, seatPredict().cp.grapple.on ? 1 : 0, seatPredict().cp.grappleN, seatPredict().cp.grappleMax, seatPredict().cp.grappleOffN, seatPredict().cp.grappleOffMax, seatPredict().cp.exactN, seatPredict().cp.exactMiss, seatPredict().cp.exactMax, seatPredict().cp.velMiss, seatPredict().cp.exactHeld,
        seatPredict().cp.contactN, seatPredict().cp.contactMax, static_cast<unsigned long long>(seatPredict().cp.hashSv), static_cast<unsigned long long>(seatPredict().cp.hashCl), seatPredict().cp.seq,
        netmove::clientNewest());
    if(Cmd_Argc() > 1 && !q_strcasecmp(Cmd_Argv(0), "vr_mp_predict_report") && !q_strcasecmp(Cmd_Argv(1), "reset"))
    {
        // (vr_mp_predict_report reset: the counts from here, a test's setup left out; the states read go on)
        seatPredict().cp.frames = seatPredict().cp.unpredicted = seatPredict().cp.unWalked = seatPredict().cp.corrN = seatPredict().cp.flatN = seatPredict().cp.airN = seatPredict().cp.rideN = 0;
        seatPredict().cp.swimN = seatPredict().cp.wjN = seatPredict().cp.landMovingN = seatPredict().cp.pushN = seatPredict().cp.crawlN = seatPredict().cp.slideN = seatPredict().cp.mantleN = seatPredict().cp.hangN = seatPredict().cp.wclimbN = seatPredict().cp.grappleN = seatPredict().cp.grappleOffN = 0;
        seatPredict().cp.hangRise = seatPredict().cp.transMax = seatPredict().cp.grappleMax = seatPredict().cp.grappleOffMax = 0.f;
        seatPredict().cp.transN = seatPredict().cp.exactN = seatPredict().cp.exactMiss = seatPredict().cp.velMiss = seatPredict().cp.exactHeld = seatPredict().cp.contactN = 0;
        seatPredict().cp.contactMax = 0.f;
        seatPredict().cp.hashSv = seatPredict().cp.hashCl = 14695981039346656037ull;
        seatPredict().cp.exactMax = 0.f;
        seatPredict().cp.depthSum = 0;
        seatPredict().cp.corrMax = seatPredict().cp.corrSum = seatPredict().cp.flatMax = seatPredict().cp.airMax = seatPredict().cp.errMax = seatPredict().cp.swimMax = seatPredict().cp.liftMax = 0.f;
    }
}

void init()
{
    Cmd_AddCommand("vr_mp_predict_report", report);
    Cmd_AddCommand("vr_mp_predict_probe", probe_f);
    Cmd_AddCommand("vr_mp_predict_tracelog", [] {
        traceLog.seq = Cmd_Argc() > 1 ? static_cast<za::U32>(Q_atoi(Cmd_Argv(1))) : 0u;
        Con_Printf("vr_mp_predict_tracelog: %s\n", traceLog.seq != 0 ? va("move %u", traceLog.seq) : "off");
    });
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

void grappleFrame(edict_t* ent, edict_t* hook, int hand, const float* at, float beyond, bool reeling, float shortest)
{
    if(!ent || ent->free || !hook || hook->free)
    {
        return;
    }
    // One step of the frame (B9's one path: a remote client's moves run the same function each).
    pmove::Grapple g;
    grappleOf(hook, g);
    g.hand = hand;
    for(int i = 0; i < 3; i++)
    {
        g.at[i] = at[i];
    }
    g.beyond = beyond;
    g.shortest = shortest;
    g.reelAge = reeling ? static_cast<float>(qcvm->time) - progs::fieldFloatOr(hook, progs::fields().gh_reel, 0.f) : -1.f;
    EdictBody b{ent};
    b.grappleHook = hook;
    pmove::grappleStep(b, g, reeling, pr_global_struct->frametime);
    if(!b.removed)
    {
        b.store();
        SV_LinkEdict(ent, false); // (QC's setorigin)
    }
    if(!hook->free)
    {
        grappleStore(hook, g);
    }
}

} // namespace qvr::predict

using namespace qvr;
using namespace qvr::predict;

namespace qvr::predict
{
// A frame's start for the slot's walk (a client's message read, a bot's frame).
void beginFrame(int slot, edict_t* ent)
{
    serverSlots[slot].n = 0; // (moves a frame not walked one by one dropped: it ran the newest, as NetQuake)
    serverSlots[slot].walked = false;
    serverSlots[slot].plain = false; // (VR_PredictThink says again: a dead or paused frame does not think)
    if(ent)
    {
        progs::setFieldFloat(ent, progs::fields().vr_movejump, 0.f); // (QuakeC's jump until it does)
        progs::setFieldFloat(ent, progs::fields().vr_moveswim, 0.f);
        progs::setFieldFloat(ent, progs::fields().vr_movewaterjump, 0.f);
        progs::setFieldFloat(ent, progs::fields().vr_movewaterclimb, 0.f);
    }
}
} // namespace qvr::predict

extern "C" void VR_PredictBeginRead(client_t* client)
{
    const int slot = static_cast<int>(client - svs.clients);
    if(slot >= 0 && slot < MAX_SCOREBOARD)
    {
        beginFrame(slot, client->edict);
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
               !hull::isFlat(ent); // (a headset's slide too: his moves', B14)
    if(s->plain)
    {
        computeRules(ent, s->rules);
        if(ownMoves(slot))
        {
            s->n = 0; // (his one move: this frame's clc_move, for the frame's time)
            s->queue[s->n++] = ownInput(ent);
        }
        else if(botMoves(slot))
        {
            s->n = 0; // (its one move: its stick this frame, botThink's)
            s->queue[s->n++] = botInput(ent, s->botMove);
        }
    }
    // (PlayerPreThink leaves his jump to his moves: lying down or AD's boots, QuakeC's; the field clears for the boots)
    progs::setFieldFloat(ent, progs::fields().vr_movejump, s->plain && !hull::isProne(ent) ? 1.f : 0.f);
    progs::setFieldFloat(ent, progs::fields().vr_moveswim, s->plain ? 1.f : 0.f); // (WaterMove leaves its drag to them)
    // (what else QuakeC leaves to his moves: bit 0 the hands' climb out of the water, B8j part 3; bit 1 the grapple's
    // rope, B8j part 4 -- vr_net_permove_skip's bits left to QuakeC, once a frame)
    const int skip = static_cast<int>(vr_net_permove_skip.value);
    progs::setFieldFloat(ent, progs::fields().vr_movetakes, s->plain ? static_cast<float>(3 & ~skip) : 0.f);
    if(progs::fields().vr_movegrapple >= 0)
    {
        progs::fieldInt(ent, progs::fields().vr_movegrapple) = 0; // (VR_Grapple_ServiceAnchor sets it again)
    }
    return s->plain ? 1 : 0;
}

extern "C" int VR_PredictWalks(edict_t* ent)
{
    const ServerSlot* s = slotOfEdict(ent);
    return s && s->plain ? 1 : 0;
}

extern "C" void VR_PmoveTrace(edict_t* ent)
{
    if(vr_pmove_trace.value == 0.f || ent->free)
    {
        return;
    }
    // (the server's time, the walk's state after the physics: two runs' lines compared one by one, Misc/quakevr/mp/pmtrace_cmp.py)
    Con_Printf("pmtrace: e=%d t=%.4f o=%.4f %.4f %.4f v=%.3f %.3f %.3f fl=%d wl=%d ge=%d m=%d\n", NUM_FOR_EDICT(ent),
        qcvm->time, ent->v.origin[0], ent->v.origin[1], ent->v.origin[2], ent->v.velocity[0], ent->v.velocity[1],
        ent->v.velocity[2], static_cast<int>(ent->v.flags), static_cast<int>(ent->v.waterlevel),
        static_cast<int>(ent->v.groundentity), VR_PredictWalks(ent));}

extern "C" void VR_PredictPhysics(edict_t* ent)
{
    ServerSlot* s = slotOfEdict(ent);
    if(!s)
    {
        return;
    }
    pmove::Rules& r = s->rules;
    if(s->waterNext != qcvm->time)
    {
        s->waterLevel = static_cast<int>(ent->v.waterlevel); // (the last frame's SV_CheckWater: his walk or swim's)
        s->waterType = static_cast<int>(ent->v.watertype);
    }
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
    b.waterLevel = b.thinkLevel = s->waterLevel;
    b.waterType = s->waterType;
    b.mantle = mantle;
    b.hang = hang;
    // The grapple's rope (B8j part 4: QC VR_Grapple_ServiceAnchor left its hold and reel to his moves this frame).
    edict_t* hook = moveGrappleHook(ent);
    if(hook)
    {
        const int num = NUM_FOR_EDICT(hook);
        const float age = num == s->grappleHook && s->grapple.on ? s->grapple.reelAge : -1.f;
        s->grapple = pmove::Grapple{};
        grappleOf(hook, s->grapple);
        const glm::vec3 at = progs::fieldVec(ent, progs::fields().vr_movegrapple_at);
        const glm::vec3 rope = progs::fieldVec(ent, progs::fields().vr_movegrapple_rope);
        for(int i = 0; i < 3; i++)
        {
            s->grapple.at[i] = at[i];
        }
        s->grapple.beyond = rope.x;
        s->grapple.shortest = rope.y;
        s->grapple.drivers = static_cast<int>(rope.z) & 3;
        s->grapple.hand = ((static_cast<int>(rope.z) >> 2) & 3) - 1;
        s->grapple.reelAge = age;
        s->grappleHook = num;
        b.grapple = s->grapple;
        b.grappleHook = hook;
    }
    else
    {
        s->grapple.on = false;
        s->grappleHook = 0;
    }
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
    // A headset's slide (B14: slide::move in his walking moves, as SV_AirMove's for flatscreen play): its state the
    // server's for his client number, its clock the server's time.
    const int slideNum = slide::clientSlot(ent);
    slide::Slide* slid = slideNum >= 0 ? &slide::state(slideNum) : nullptr;
    if(slid)
    {
        if(qcvm->time < slid->lastTime)
        {
            *slid = slide::Slide{}; // (a new map: the clock starts again)
        }
        slid->lastTime = qcvm->time;
        b.slide = *slid;
        b.slideClock = qcvm->time;
        b.staminaLeft = r.slideTuning.staminaPart > 0.f ? progs::staminaLeft(ent) : 0.f;
    }
    s->credit = q_min(s->credit + static_cast<float>(host_frametime) * creditRate, creditMost);
    const int slot = static_cast<int>(s - serverSlots);
    pmove::Input newest;
    s->notes = pmove::MoveNotes{};
    s->heldNow = false;
    for(int k = 0; k < s->n && !b.removed; k++)
    {
        pmove::Input in = s->queue[k];
        newest = in;
        s->slideSeen = s->slideSeen || in.slide;
        const bool held = in.dt > s->credit;
        s->heldNow = s->heldNow || held;
        if(held)
        {
            in.dt = s->credit; // (a client running ahead of the server's clock: held to it)
        }
        s->credit -= in.dt;
        b.jumpedNow = false;
        b.climbedNow = false;
        const za::U32 seqNow = netmove::serverLast(slot) - static_cast<za::U32>(s->n - 1 - k);
        traceLog.now = traceLog.seq != 0 && seqNow == traceLog.seq;
        traceLog.side = "sv";
        if(traceLog.now)
        {
            Con_Printf("mp_tracelog: sv move=%u o=%.9g %.9g %.9g v=%.9g %.9g %.9g fl=%d dt=%.9g\n", seqNow, b.origin[0],
                b.origin[1], b.origin[2], b.velocity[0], b.velocity[1], b.velocity[2], b.flags, in.dt);
        }
        pmove::playerMove(b, in, r, &s->notes);
        if(traceLog.now)
        {
            Con_Printf("mp_tracelog: sv after o=%.9g %.9g %.9g v=%.9g %.9g %.9g\n", b.origin[0], b.origin[1], b.origin[2],
                b.velocity[0], b.velocity[1], b.velocity[2]);
        }
        traceLog.now = false;
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
    if(hook)
    {
        s->grapple = b.grapple;
        if(!hook->free)
        {
            grappleStore(hook, b.grapple); // (the rope's length and taut after his moves: the hook's)
        }
        if(vr_mp_watch.value >= 2.f)
        {
            Con_Printf("mp_predict_grapple: slot=%d s=%u len=%.2f reel=%.3f taut=%d z=%.2f\n", slot,
                netmove::serverLast(slot), b.grapple.len, b.grapple.reelAge, b.grapple.taut ? 1 : 0, b.origin[2]);
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
        if(b.thinkLevel >= 2 && !(b.flags & FL_WATERJUMP) && s->n > 0 && !mantling && !hanging)
        {
            // The hands' strokes (VR_AfterWaterMove's, less the glide the moves ran), once a frame on the newest move's
            // stick: the server's alone (his replay corrects to them, eased in). By the level its swim went by.
            const float scale = r.stickScale * (b.thinkLevel >= 3 || !(b.flags & FL_ONGROUND) ? r.swimStick : r.wadeStick);
            physics::swimStrokes(ent, newest.forward * scale, newest.side * scale, newest.up * scale);
        }
        if(crawl)
        {
            crawl[0] = b.crawl[0];
            crawl[1] = b.crawl[1];
            if(b.thinkLevel < 2 && s->n > 0 && !mantling && !hanging)
            {
                // The hands' pulls (VR_AfterCrawlMove's, the stick's want with them), once a frame on the newest move's
                // stick: the server's alone (his replay corrects to them, eased in).
                const float scale = r.stickScale * (b.thinkLevel >= 1 ? r.shallowStick : 1.f);
                physics::crawlHands(ent, newest.forward * scale, newest.side * scale);
            }
        }
        VR_FindRadius_Dirty(ent);
    }
    if(slid)
    {
        *slid = b.slide;
        s->slideClock = b.slideClock;
        s->staminaLeft = b.staminaLeft;
        if(!ent->free)
        {
            // (QC's word of the slide on the ground, as VR_SlideMove's: its charge into a monster)
            progs::setFieldFloat(ent, progs::fields().vr_sliding, slid->on && !slid->airborne ? 1.f : 0.f);
        }
    }
    s->crawl[0] = crawl ? crawl[0] : 0.f;
    s->crawl[1] = crawl ? crawl[1] : 0.f;
    VectorCopy(b.groundNormal, s->groundNormal);
    s->groundAge = b.groundAge;
    s->waterLevel = b.waterLevel;
    s->waterType = b.waterType;
    s->waterNext = qcvm->time + host_frametime; // (as SV_Physics advances it)
    s->n = 0;
    s->walked = true;
    if(!ent->free)
    {
        VR_PortalClientCross(ent); // (VR_ClientRoomscaleMove's, after the move: the room-scale walk ran in the moves)
    }
}

namespace qvr::predict
{

// A bot just off its floor: lava or slime where its box lands straight down (at most 1024 units).
[[nodiscard]] bool dropsIntoHarm(edict_t* ent)
{
    vec3_t end;
    VectorCopy(ent->v.origin, end);
    end[2] -= 1024.f;
    const trace_t t = SV_Move(ent->v.origin, ent->v.mins, ent->v.maxs, end, MOVE_NOMONSTERS, ent);
    vec3_t feet;
    VectorCopy(t.endpos, feet);
    feet[2] += ent->v.mins[2] + 1.f;
    const int c = SV_PointContents(feet);
    return c == CONTENTS_LAVA || c == CONTENTS_SLIME;
}

int botThink(edict_t* ent, const float* move)
{
    ServerSlot* s = slotOfEdict(ent);
    if(!s || ent->free)
    {
        return 0;
    }
    const int slot = static_cast<int>(s - serverSlots);
    beginFrame(slot, ent);
    for(int i = 0; i < 3; i++)
    {
        s->botMove[i] = move[i];
    }
    return VR_PredictThink(ent);
}

void botWalk(edict_t* ent, float* obstructed)
{
    obstructed[0] = obstructed[1] = obstructed[2] = 0.f;
    ServerSlot* s = slotOfEdict(ent);
    if(!s || !s->plain || ent->free)
    {
        return;
    }
    const pmove::Input in = s->n > 0 ? s->queue[s->n - 1] : pmove::Input{};
    const glm::vec3 from{ent->v.origin[0], ent->v.origin[1], ent->v.origin[2]};
    const glm::vec2 velBefore{ent->v.velocity[0], ent->v.velocity[1]};
    const bool groundBefore = (static_cast<int>(ent->v.flags) & FL_ONGROUND) != 0;
    // (MP B11 part 4) SV_Physics_Client's premove, as a client's: its hands' and weapons' touches (a gun or a backpack its
    // hand closes on taken by the touch's grip, as a player's), its hands in the water.
    VR_ClientPreMove(ent);
    if(ent->free)
    {
        return;
    }
    VR_PredictPhysics(ent);
    if(ent->free)
    {
        return;
    }
    // (B14d) A walk off a ledge into lava or slime (not a jump, not thrown): held at the edge as walkmove holds a monster,
    // told to its AI as a wall (its way along its speed). FrikBot's look ahead (frik_obstacles: one line 32 units ahead)
    // misses a stair's or a narrow ledge's side; walked through the players' moves its bots went into dm4's lava every
    // few seconds.
    if(groundBefore && !in.jump && !(static_cast<int>(ent->v.flags) & FL_ONGROUND) && ent->v.velocity[2] <= 0.f &&
        ent->v.health > 0.f && dropsIntoHarm(ent))
    {
        VectorCopy(&from[0], ent->v.origin);
        ent->v.velocity[0] = ent->v.velocity[1] = ent->v.velocity[2] = 0.f;
        ent->v.flags = static_cast<float>(static_cast<int>(ent->v.flags) | FL_ONGROUND);
        SV_LinkEdict(ent, false);
        VR_PmoveTrace(ent);
        const float speed = glm::length(velBefore);
        if(speed > 1e-3f)
        {
            obstructed[0] = velBefore.x * static_cast<float>(in.dt);
            obstructed[1] = velBefore.y * static_cast<float>(in.dt);
        }
        return;
    }
    SV_LinkEdict(ent, true); // (SV_Physics_Client's: the triggers it stands in touched)
    VR_PmoveTrace(ent);      // (a bot's walk runs from QuakeC's frame, not SV_Physics_Client: its line here)
    // Held by a wall: how far short of its stick's want along the wish (SV_AirMove's wish on its yaw, at most the rules'
    // speed, reached as SV_Accelerate reaches it from its speed before along that way) the frame's walk took it, as
    // FrikBot's PostPhysics measured its own (a walkmove to its physics' place failing: a wall). Along the wish only
    // (B14d): its speed before as a length, or the way across it, counted a turn or a stop as a wall, and the hold told
    // each frame cleared the ledge's obstruction frik_obstacles had just set (the bots walked off dm4's ledges into
    // the lava: frik_obstructed with no danger clears a held AI_OBSTRUCTED).
    if(!s->notes.wall || s->notes.swim || (in.forward == 0.f && in.side == 0.f))
    {
        return;
    }
    const float yaw = DEG2RAD(in.steer[YAW]);
    const glm::vec2 fwd{cosf(yaw), sinf(yaw)}, right{sinf(yaw), -cosf(yaw)};
    const glm::vec2 wish = fwd * in.forward + right * in.side;
    const float wishSpeed = glm::length(wish);
    if(wishSpeed < 1e-3f)
    {
        return;
    }
    const glm::vec2 way = wish / wishSpeed;
    const pmove::Rules& r = s->rules;
    const float top = q_min(wishSpeed * r.stickScale, r.maxspeed);
    const float along = glm::dot(velBefore, way);
    const float want = along >= top ? along : q_min(top, along + r.accelerate * top * static_cast<float>(in.dt));
    const glm::vec2 moved{ent->v.origin[0] - from.x, ent->v.origin[1] - from.y};
    const float shortBy = want * static_cast<float>(in.dt) - glm::dot(moved, way);
    if(want <= 0.f || shortBy <= 0.f)
    {
        return;
    }
    obstructed[0] = way.x * shortBy;
    obstructed[1] = way.y * shortBy;
}

} // namespace qvr::predict

extern "C" void VR_WritePredict(sizebuf_t* out, client_t* client)
{
    const int slot = static_cast<int>(client - svs.clients);
    if(!remoteMoves(slot) || !client->edict)
    {
        return;
    }
    // The record is written into its own buffer and goes only if all of it fits the datagram (mpreview B1: a fixed
    // reserve was smaller than the record's largest, and a datagram's overflow ends the server). Its size is whatever
    // the fields below add up to, so a field added later can't overflow the datagram either.
    byte recBuf[4096];
    sizebuf_t rec{};
    rec.data = recBuf;
    rec.maxsize = sizeof(recBuf);
    rec.allowoverflow = true; // (a record past 4 KB: dropped, never written in part)
    sizebuf_t* const msg = &rec;
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
    MSG_WriteByte(msg, protocol::svc_quakevr);
    MSG_WriteByte(msg, protocol::QVR_SVC_PREDICT);
    MSG_WriteShort(msg, bits | (mantling ? bitMantle : 0) | (hanging ? bitHang : 0) | (takes ? bitTake : 0));
    const bool grappled = s.walked && s.grapple.on; // (the grapple's rope in his moves: B8j part 4)
    // (the hands' climb out: B8j part 3; the water his last move found, his next move's walk or swim's: B11b part 2)
    const bool waterOwn = s.waterNext == sv.qcvm.time; // (after SV_Physics; else the next frame's start from the edict's)
    const int waterLevel = waterOwn ? s.waterLevel : static_cast<int>(ent->v.waterlevel);
    const int waterType = waterOwn ? s.waterType : static_cast<int>(ent->v.watertype);
    MSG_WriteByte(msg, (r.waterClimb ? 1 : 0) | (r.climbHands << 1) | (grappled ? 8 : 0) | ((waterLevel & 3) << 4) |
                           (waterCode(waterType) << 6));
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
    MSG_WriteFloat(msg, moveguard::walkLeft(slot)); // (his room-scale budget after the move: his replay's own, review B6)
    // A headset's slide (B14): its state; once his moves sent its input, its numbers (the server's: his replay slides
    // as his moves do here) and, sliding or cooling down, its times against the slide's clock after his moves. An idle
    // record stays the same. Bit 128: a move of his frame held to the server's clock (his results' check skips it).
    const int slideNum = slide::clientSlot(ent);
    const slide::Slide sl = slideNum >= 0 ? slide::state(slideNum) : slide::Slide{};
    const double clock = s.walked ? s.slideClock : sv.qcvm.time;
    const bool slideTimes = s.slideSeen && (sl.on || sl.cooldownUntil > clock);
    MSG_WriteByte(msg, (sl.on ? 1 : 0) | (sl.held ? 2 : 0) | (sl.grounded ? 4 : 0) | (sl.airborne ? 8 : 0) |
                           (r.slide ? 16 : 0) | (s.slideSeen ? 32 : 0) | (slideTimes ? 64 : 0) | (s.heldNow ? 128 : 0));
    if(s.slideSeen)
    {
        const slide::Tuning& t = r.slideTuning;
        for(const float f : {sl.lastZ, s.staminaLeft, t.least, t.top, t.boost, t.end, t.slope, t.steer, t.friction,
                t.cooldown, t.staminaPart, t.staminaStep})
        {
            MSG_WriteFloat(msg, f);
        }
        MSG_WriteByte(msg, t.staminaParts);
    }
    if(slideTimes)
    {
        MSG_WriteByte(msg, sl.partsPaid);
        MSG_WriteFloat(msg, static_cast<float>(sl.cooldownUntil - clock));
        MSG_WriteFloat(msg, static_cast<float>(clock - sl.startedAt));
    }
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
    if(grappled)
    {
        // (B8j part 4: his replay holds and reels him as his moves do on the server, on these)
        const pmove::Grapple& g = s.grapple;
        MSG_WriteByte(msg, g.drivers | ((g.hand + 1) << 2) | (g.taut ? 16 : 0));
        for(const float f : {g.at[0], g.at[1], g.at[2], g.beyond, g.len, g.speed, g.reelAge, g.shortest, g.payout})
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
            MSG_WriteFloat(msg, m->v.origin[i]);
        }
        for(int i = 0; i < 3; i++)
        {
            MSG_WriteFloat(msg, m->v.velocity[i]);
        }
        MSG_WriteFloat(msg, m->v.nextthink > m->v.ltime ? m->v.nextthink - m->v.ltime : moverNoStop);
    }
    // (B24) the server's tick (its last: a dedicated server's are all host_netinterval, a listen host's follow his
    // headset's frames by a little)
    MSG_WriteFloat(msg, n > 0 ? static_cast<float>(host_frametime) : 0.f);
    if(rec.overflowed || out->cursize + rec.cursize > out->maxsize)
    {
        return; // (no room this datagram: the next one's record has the newer state anyway)
    }
    const int at = out->cursize;
    SZ_Write(out, rec.data, rec.cursize);
    VR_SnapTake(out, at, stateKey); // (a snapshot record: an idle player's costs a few bytes)
}

extern "C" void VR_PredictRelink(int num, float* origin)
{
    if(num != cl.viewentity)
    {
        if(num == seatPredict().cp.ridden && num > 0 && seatPredict().cp.drawing)
        {
            // (the lift he stands on: where his replay has it, between his newest move's start and end as he is drawn)
            const float t = seatPredict().cp.riddenBefore + seatPredict().cp.drawFrac * (seatPredict().cp.riddenAfter - seatPredict().cp.riddenBefore);
            moverAt(num, t, origin);
            VectorAdd(origin, seatPredict().cp.liftError, origin);
        }
        return;
    }
    if(seatPredict().cp.frame == host_framecount)
    {
        if(seatPredict().cp.drawing)
        {
            VectorCopy(seatPredict().cp.drawn, origin);
        }
        return;
    }
    seatPredict().cp.frame = host_framecount;
    const double since = seatPredict().cp.lastRelink > 0.0 ? q_min(realtime - seatPredict().cp.lastRelink, 0.25) : 0.0;
    seatPredict().cp.lastRelink = realtime;
    const za::U32 newest = netmove::clientNewest();
    if(newest != seatPredict().cp.lastNewest)
    {
        netmove::WireMove m;
        seatPredict().cp.lastNewest = newest;
        seatPredict().cp.lastNewTime = realtime;
        seatPredict().cp.lastDt = netmove::clientMoveAt(newest, m) ? inputOf(m, moveguard::unitsPerMetre(units::worldScale())).dt : 0.f;
    }
    vec3_t predicted, before;
    seatPredict().cp.drawing = clientActive() && replay(predicted, before);
    if(!seatPredict().cp.drawing)
    {
        if(netmove::clientUses() && cls.signon == SIGNONS)
        {
            seatPredict().cp.unpredicted++;
            seatPredict().cp.unWalked += seatPredict().cp.have && !(seatPredict().cp.bits & bitWalked) ? 1 : 0; // (the server's frame not run move by move)
        }
        VectorCopy(vec3_origin, seatPredict().cp.error);
        VectorCopy(vec3_origin, seatPredict().cp.transError);
        seatPredict().cp.ridden = 0;
        for(Past& p : seatPredict().cp.past)
        {
            p.seq = 0;
        }
        probeCheck(origin);
        return;
    }
    seatPredict().cp.frames++;
    // Frames drawn faster than moves are made (a 90 Hz headset on 72 Hz ticks, an uncapped window): between the place
    // before the newest move and after it, by the time since it was made over its frame's (no step a move, no
    // overshoot at a stop; a move's time behind).
    const float frac =
        seatPredict().cp.lastDt > 0.f ? q_min(q_max(static_cast<float>((realtime - seatPredict().cp.lastNewTime) / seatPredict().cp.lastDt), 0.f), 1.f) : 1.f;
    seatPredict().cp.drawFrac = frac;
    const float k = expf(-static_cast<float>(since) / easeTime);
    VectorScale(seatPredict().cp.error, k, seatPredict().cp.error);
    VectorScale(seatPredict().cp.liftError, seatPredict().cp.ridden > 0 ? k : 0.f, seatPredict().cp.liftError);
    VectorScale(seatPredict().cp.transError, k, seatPredict().cp.transError);
    vec3_t own; // (his offset on what he stands on, less a mantle's start: the transition's, counted apart)
    VectorSubtract(seatPredict().cp.error, seatPredict().cp.liftError, own);
    VectorSubtract(own, seatPredict().cp.transError, own);
    seatPredict().cp.errMax = q_max(seatPredict().cp.errMax, VectorLength(own));
    for(int i = 0; i < 3; i++)
    {
        seatPredict().cp.drawn[i] = before[i] + frac * (predicted[i] - before[i]) + seatPredict().cp.error[i];
    }
    VectorCopy(seatPredict().cp.drawn, origin);
    probeCheck(origin);
}
