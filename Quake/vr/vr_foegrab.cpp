// vr_foegrab.cpp -- holding enemies: see vr_foegrab.hpp.

#include "vr_foegrab.hpp"

#include "vr_api.h"
#include "vr_box3d.hpp"
#include "vr_climb.hpp"
#include "vr_cvars.hpp"
#include "vr_grip.hpp"
#include "vr_hands.hpp"
#include "vr_held.hpp"
#include "vr_hitmodel.hpp"
#include "vr_mem.hpp"
#include "vr_move.hpp"
#include "vr_profile.hpp"
#include "vr_progs.hpp"
#include "vr_protocol.hpp"
#include "vr_server.hpp"
#include "vr_units.hpp"

#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Acos.hpp"
#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Exp.hpp"
#include "Zancle/Math/Lround.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Round.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/Math/Sqrt.hpp"
#include "vr_zancle.hpp"

using namespace qvr;
using namespace qvr::progs;

namespace
{

constexpr int grabBit[2] = {1 << 1, 1 << 3};     // QVR_VRBITS0_*HAND_GRABBING
constexpr int prevGrabBit[2] = {1 << 2, 1 << 4}; // QVR_VRBITS0_*HAND_PREVGRABBING
constexpr int climbingBit[2] = {1 << 15, 1 << 16}; // QVR_VRBITS0_*HAND_CLIMBING (vr_climb.cpp, this frame)
constexpr float statScale = 8.f;      // the spots in the stats: eighths of a unit
constexpr double lateGrab = 0.2;      // s: a grip pressed this long before the fist touches the enemy still takes hold
constexpr float palmRadiusCm = 3.5f;  // the palm's sphere (alone where the fist isn't known: another player's, a dedicated server)
constexpr float maxInsideCm = 25.f;   // a fist's sphere at most this deep in the model holds (at the surface nearest it)
constexpr float searchReach = 96.f;   // units a model is found out of its box (hitmodel's VR_HITMODEL_REACH)
constexpr float teleportStep = 64.f;  // units: a held enemy moved further in a frame (teleported) is let go
constexpr float pinInTime = 0.05f;    // s: the drawn hand settles on the spot
constexpr float pinOutTime = 0.12f;   // and goes back to the tracked hand after letting go
constexpr double breakGrace = 0.25;  // s: a hand pulled past vr_foegrab_break lets go after this long past it,
constexpr float breakFar = 2.f;       // or at once this many times as far
constexpr float animTime = 0.12f;     // s: the held spot eases to where the model's animation has it
constexpr float animMaxCm = 20.f;     // and is at most this far from where the body alone (origin, yaw) carries it
constexpr float followMaxSpeed = 480.f; // units/s: a held enemy follows its hands' own move at most this fast
constexpr double throwSettle = 0.15;  // s: both hands on it this long before a turn throws (the reach to take hold is none)
constexpr double throwRetry = 0.5;    // s: after a turn that didn't throw (too strong, away from you), the next try
constexpr float throwMinArm = 0.1f;   // m: a hand nearer the enemy's middle counts as this far (the turn's measure)
constexpr float throwAwayCos = 0.7071f; // a throw within 45 degrees of straight away from you is a shove's (vr_foegrab_throw_away 0)
constexpr float throwWayTime = 0.04f; // s: the turn's way smoothed over about this long (a frame's jitter doesn't pick the side)
constexpr float softReach = 16.f;     // units: a hand holds on to the ragdoll's limb nearest it within this (handOver)
constexpr float softStuck = 72.f;     // units: and lets go of it when the limb lags this far off for half a second
constexpr float carryGrace = 0.3f;    // s: a ragdoll held on passes through the hands (box3d::ragdollGrace, kept up)
constexpr float liftHertz = 8.f;      // Hz: a lifted one's hold's spring (a ragdoll limb's usual: carried, it sags little)
constexpr float neckSmooth = 0.03f;   // s: the hands' twist of a head smoothed over about this long (neckStep)
constexpr float neckRollArm = 0.08f;  // m: the wrists' roll of a head counts as the hands this far out turning it (neckStep)
constexpr float neckMinArm = 0.04f;   // m: hands nearer together count as this far apart (the twist's measure)
constexpr float neckGrip = 6.f;       // units: a palm this near a ragdoll's neck's line holds its neck (ragdollNeckCheck)
constexpr float neckFloor = 0.4f;     // a twist slower than this share of vr_foegrab_necksnap_speed ends the stroke (neckStep)
constexpr float neckPullShare = 0.75f; // a twist: each palm's speed about their middle at least this share of their mean velocity

// Why a hand let go (QC VR_FoeGrab_Released's xWhy).
enum class Why : int
{
    Released = 0, // the grip let go
    Pulled = 1,   // the hand pulled too far from the spot
    BrokeFree = 2, // its shove broke the hold (vr_foegrab_shove 1)
    Lost = 3,     // the enemy died, was knocked down, moved away; the option off; the hand took something else
    Thrown = 4,   // both hands threw it down (throwCheck)
    Lifted = 5,   // both hands lifted it off its feet (liftCheck)
    Snapped = 6,  // both hands snapped its neck (neckCheck)
};

struct Hold
{
    bool active{false};
    int ent{0};            // the monster's entity number
    int modelindex{0};     // and its model (another: let go)
    int tri{-1};           // the spot: the triangle of its model and where on it (hitmodel::anchorFrame)
    float u{0.f}, v{0.f};
    glm::vec3 local{0.f};   // the spot when it took hold, in the body's frame (its drawn origin, turned with its yaw)
    glm::vec3 palmOff{0.f}; // the palm from the spot then, in the body's frame
    glm::vec3 anim{0.f};    // the model's animation's move of the spot since (world), eased (animTime) and capped (animMax)
    glm::vec3 palmAt{0.f}; // where the palm is held now (world)
    float strength{0.f};   // this hand's hold on it (QC VR_FoeGrab_Strength)
    float stretch{0.f};    // units from the tracked palm to palmAt (last frame)
    double since{0.0};
    double overSince{-1.0}; // the stretch past vr_foegrab_break since (-1: not)
    glm::vec3 lastPalm{0.f}; // the tracked palm at the end of the last server frame (world): its move since is followed
};

struct Holder
{
    Hold holds[2];
    bool pressedLast[2]{false, false};
    double pressedAt[2]{-1.0, -1.0}; // the grip's press (-1: not pressed)
    bool owned[2]{false, false};     // the grip is the hold's (hidden from the QC) until it is released
    bool ownedLast[2]{false, false};
    double lastTime{-1.0};
    glm::vec3 lastOrigin{0.f}; // the player's at the end of the last frame (serverFrame: a shove's slide carries the held)
    bool originKnown{false};
    // The two-hand throw (throwCheck): no try before throwAgain; both hands' hold's hardest turn so far (and the hands'
    // speed then), for tuning (vr_foegrab_debug, vr_foegrab_status).
    double throwAgain{0.0};
    bool twoHanded{false};
    float peakTwist{0.f};
    float peakSpeed{0.f};
    glm::vec3 way{0.f}; // the turn's way (level: where it tips its top, rad/s), smoothed (throwWayTime)
    // The enemy held on as a ragdoll after a throw or a lift (handOver, carryStep): its entity (0: none), since when; a
    // lift's (`lifted`), when it breaks free (breakAt).
    int carried{0};
    double carriedSince{0.0};
    bool lifted{false};
    bool snapped{false};
    double breakAt{-1.0};
    // The neck snap (neckStep, neckCheck): the enemy both hands hold by the head (0: none), the hands' twist of it (rad/s,
    // smoothed: neckSmooth), how far it has turned one way so far (rad) and about which axis, its fastest twist (deg/s:
    // the debug's), the hands' turns at the last frame (their wrists' roll), no snap before neckAgain.
    int neckEnt{0};
    glm::vec3 neckSpin{0.f};
    float neckTurned{0.f};
    glm::vec3 neckAxis{0.f};
    float neckPeak{0.f};
    float neckStroke{0.f}; // the fastest twist of the stroke now (rad/s)
    glm::mat3 neckRot[2]{glm::mat3{1.f}, glm::mat3{1.f}};
    double neckAgain{0.0};
    double neckLast{-1.0};
};

Holder holders[MAX_SCOREBOARD];

// The held monsters: where each was at the end of the last frame (its own movement since is what is slowed).
struct Foe
{
    int ent{0};
    glm::vec3 last{0.f};
    float lastYaw{0.f};
    bool fresh{true}; // taken this frame: no movement of its own yet
};
constexpr int maxFoes = 2 * MAX_SCOREBOARD;
Foe foes[maxFoes];
int numFoes = 0;

// vr_foegrab_walk_test: the monster walked away from the player each frame.
struct WalkTest
{
    int ent{0};
    float speed{0.f};
    float seconds{0.f};
    double until{-1.0};
    glm::vec3 from{0.f};
    bool reported{true};
};
WalkTest walkTest;

// The thrown ragdoll's fall (vr_foegrab_throw_test, vr_foegrab_debug): every 0.1 s for a second, how far its torso (pelvis
// to head) leans from upright, how far its feet moved, where its head went along the throw.
struct ThrowTrace
{
    int ent{0};
    double start{0.0};
    int printed{0};
    glm::vec3 dir{0.f};
    glm::vec3 feet0{0.f};
    float maxTilt{0.f};
    float tiltHalf{-1.f};
    float maxFeet{0.f};
    glm::vec3 pelvis0{0.f};
    const char* kind{"throw"}; // (or "shove": a shove's knockdown, shoveTopple)
    // Its torso's whole turn (degrees, every frame's added: two turns over are 720, where its tilt is at most 180), and
    // when its head first dropped below its pelvis's height at the start and when its feet first moved 8 units along.
    glm::vec3 lastTorso{0.f};
    float turned{0.f};
    float headDown{-1.f};
    float feetGo{-1.f};
    glm::vec3 viewer{0.f}; // the first player's origin at the start: where it lies is told from his side (the tests)
};
ThrowTrace throwTrace;

struct FoeGrabScratch
{
    za::Vector<glm::vec4> fist; // the hand's fist in the world (tryTake)
    auto members() { return qvr::mem::list(fist); }
};
mem::Scratch<FoeGrabScratch> scratch{"foegrab"};

// The server's QuakeC VM for a console command's while (its edicts).
struct VmScope
{
    qcvm_t* old{nullptr};
    VmScope() { PR_PushQCVM(&sv.qcvm, &old); }
    ~VmScope() { PR_PopQCVM(old); }
};

// Client: the drawn hands' pins.
struct Pin
{
    float weight{0.f};
    glm::vec3 at{0.f};
    double last{-1.0};
};
Pin pins[2];

[[nodiscard]] glm::vec3 vec(const float* v)
{
    return glm::vec3{v[0], v[1], v[2]};
}

// Where `player` faces, level (his yaw).
[[nodiscard]] glm::vec3 forwardFlat(edict_t* player)
{
    const float yaw = glm::radians(player->v.v_angle[1]);
    return glm::vec3{za::cos(yaw), za::sin(yaw), 0.f};
}

void setVec(float* out, const glm::vec3& v)
{
    out[0] = v.x;
    out[1] = v.y;
    out[2] = v.z;
}

[[nodiscard]] const char* handName(int h)
{
    return h == 1 ? "main" : "off";
}

[[nodiscard]] float cm()
{
    return 0.01f * units::metresToUnits();
}

[[nodiscard]] Holder* holderOf(edict_t* ent)
{
    const int client = NUM_FOR_EDICT(ent) - 1;
    if(client < 0 || client >= za::min(svs.maxclients, static_cast<int>(MAX_SCOREBOARD)))
    {
        return nullptr;
    }
    return &holders[client];
}

[[nodiscard]] bool debug(int level = 1)
{
    return vr_foegrab_debug.value >= static_cast<float>(level);
}

// `m`'s body: its origin and its axes turned with its yaw (the server's: what the model as drawn eases to, the client's
// lerp of a walking monster's steps lagging a step at most; the animation's ease takes up the difference).
bool bodyFrame(edict_t* m, glm::vec3& origin, glm::mat3& axes)
{
    origin = vec(m->v.origin);
    const float yaw[3]{0.f, m->v.angles[1], 0.f};
    axes = held::axesFromAngles(yaw, false);
    return true;
}

// The palm's middle of `ent`'s hand `h` (the move's place and turn, the hand's measured frame: grip::handFrame).
[[nodiscard]] glm::vec3 palmOf(edict_t* ent, const VrMove& move, int h)
{
    const grip::HandFrame f = grip::handFrame(h, h == 0, NUM_FOR_EDICT(ent) == 1);
    return move.hands[h].pos + held::axesFromAngles(&move.hands[h].rot[0], true) * f.palm;
}

// The same from the controller's own point (VrHandMove::tracked: before the level held the drawn hand out of it).
[[nodiscard]] glm::vec3 trackedPalmOf(edict_t* ent, const VrMove& move, int h)
{
    const grip::HandFrame f = grip::handFrame(h, h == 0, NUM_FOR_EDICT(ent) == 1);
    return move.hands[h].tracked + held::axesFromAngles(&move.hands[h].rot[0], true) * f.palm;
}

// Whether `m` can be held: a living monster, standing in the world (not knocked down: its ragdoll), its model the one
// the hold was taken on (modelindex > 0), the precise hits' target.
[[nodiscard]] bool canHold(edict_t* m, int modelindex = 0)
{
    if(!m || m->free || !(static_cast<int>(m->v.flags) & FL_MONSTER) || m->v.health <= 0.f || m->v.takedamage == 0.f)
    {
        return false;
    }
    const int solid = static_cast<int>(m->v.solid);
    if(solid != SOLID_SLIDEBOX && solid != SOLID_BBOX)
    {
        return false;
    }
    if(modelindex > 0 && static_cast<int>(m->v.modelindex) != modelindex)
    {
        return false;
    }
    return hitmodel::target(m);
}

// A QC function of the monster (with the player as self): its float return (`fallback` without the function).
float callQc(const char* name, edict_t* player, edict_t* m, float hand, float extra, float fallback)
{
    const func_t fn = findFunction(name);
    if(!fn)
    {
        return fallback;
    }
    const int oldSelf = pr_global_struct->self, oldOther = pr_global_struct->other;
    pr_global_struct->time = qcvm->time;
    pr_global_struct->self = EDICT_TO_PROG(player);
    pr_global_struct->other = EDICT_TO_PROG(m);
    G_INT(OFS_PARM0) = EDICT_TO_PROG(m);
    G_FLOAT(OFS_PARM1) = hand;
    G_FLOAT(OFS_PARM2) = extra;
    PR_ExecuteProgram(fn);
    const float r = G_FLOAT(OFS_RETURN);
    pr_global_struct->self = oldSelf;
    pr_global_struct->other = oldOther;
    return r;
}

[[nodiscard]] Foe* foeOf(int ent)
{
    for(int i = 0; i < numFoes; i++)
    {
        if(foes[i].ent == ent)
        {
            return &foes[i];
        }
    }
    return nullptr;
}

void addFoe(edict_t* m)
{
    const int num = NUM_FOR_EDICT(m);
    if(foeOf(num) || numFoes >= maxFoes)
    {
        return;
    }
    foes[numFoes++] = Foe{num, vec(m->v.origin), m->v.angles[1], true};
}

void release(edict_t* player, int h, Why why, const char* text)
{
    Holder* hp = holderOf(player);
    if(!hp || !hp->holds[h].active)
    {
        return;
    }
    Hold& g = hp->holds[h];
    g.active = false;
    edict_t* m = g.ent > 0 && g.ent < qcvm->num_edicts ? EDICT_NUM(g.ent) : nullptr;
    if(debug())
    {
        Con_Printf("foegrab: %s hand lets go of %s (%d): %s (held %.2f s, stretch %.1f cm)\n", handName(h),
            m && !m->free ? PR_GetString(m->v.classname) : "?", g.ent, text, qcvm->time - g.since, g.stretch / cm());
    }
    if(m && !m->free && bindings().isVrProgs)
    {
        (void)callQc("VR_FoeGrab_Released", player, m, static_cast<float>(h), static_cast<float>(why), 0.f);
    }
}

// The hand `h` grips: the living monster its fist touches (the least gap), its spot and hold taken. False: none.
bool tryTake(edict_t* player, Holder& hd, int h, const VrMove& move)
{
    const glm::vec3 palm = palmOf(player, move, h);
    za::Vector<glm::vec4>& fist = scratch.fist;
    fist.clear();
    if(NUM_FOR_EDICT(player) == 1 && cls.state != ca_dedicated)
    {
        held::fistInWorld(h, move.hands[h].pos, move.hands[h].rot, fist);
    }
    fist.pushBack(glm::vec4{palm, palmRadiusCm * cm()}); // (the palm's middle too: the fist's spheres are its outside)
    glm::vec3 lo{1e30f}, hi{-1e30f};
    for(const glm::vec4& s : fist)
    {
        lo = glm::min(lo, glm::vec3{s} - s.w);
        hi = glm::max(hi, glm::vec3{s} + s.w);
    }
    const float lenient = za::max(0.f, vr_foegrab_leniency.value) * cm();
    const float maxInside = maxInsideCm * cm();

    edict_t* best = nullptr;
    hitmodel::Hit bestHit;
    float bestGap = 1e30f;
    float nearestGap = 1e30f; // (the debug's: the nearest model's gap, taken or not)
    for(int num = svs.maxclients + 1; num < qcvm->num_edicts; num++)
    {
        edict_t* m = EDICT_NUM(num);
        if(!canHold(m))
        {
            continue;
        }
        bool close = true;
        for(int k = 0; k < 3; k++)
        {
            close = close && lo[k] <= m->v.absmax[k] + searchReach && hi[k] >= m->v.absmin[k] - searchReach;
        }
        if(!close)
        {
            continue;
        }
        for(const glm::vec4& s : fist)
        {
            const glm::vec3 c{s};
            hitmodel::Hit hit;
            float dist = 0.f;
            if(!hitmodel::nearest(m, c, hit, dist))
            {
                break;
            }
            const bool inside = glm::dot(c - hit.surface, hit.normal) < 0.f;
            if(inside && dist > maxInside)
            {
                continue;
            }
            const float gap = (inside ? -dist : dist) - s.w;
            nearestGap = za::min(nearestGap, gap);
            if(gap <= lenient && gap < bestGap)
            {
                bestGap = gap;
                best = m;
                bestHit = hit;
            }
        }
    }
    if(!best)
    {
        if(debug(fabs(qcvm->time - hd.pressedAt[h]) < 1e-6 ? 1 : 3))
        {
            Con_Printf("foegrab: %s hand grips at (%.1f %.1f %.1f): no living enemy within %.1f cm of its fist (%d spheres; the nearest %.1f cm)\n",
                handName(h), palm.x, palm.y, palm.z, lenient / cm(), static_cast<int>(fist.size()), nearestGap / cm());
        }
        return false;
    }
    glm::vec3 point{0.f}, origin{0.f};
    glm::mat3 axes{1.f}, body{1.f};
    if(!hitmodel::anchorFrame(best, bestHit.tri, bestHit.u, bestHit.v, point, axes) || !bodyFrame(best, origin, body))
    {
        return false; // (its triangle squashed flat in this pose: the next frame's press may take it)
    }
    Hold& g = hd.holds[h];
    g = Hold{};
    g.active = true;
    g.ent = NUM_FOR_EDICT(best);
    g.modelindex = static_cast<int>(best->v.modelindex);
    g.tri = bestHit.tri;
    g.u = bestHit.u;
    g.v = bestHit.v;
    g.local = glm::transpose(body) * (point - origin);
    g.palmOff = glm::transpose(body) * (palm - point);
    g.palmAt = palm;
    g.lastPalm = palm;
    g.since = qcvm->time;
    g.strength = za::clamp(callQc("VR_FoeGrab_Strength", player, best, static_cast<float>(h), 0.f, 0.5f), 0.f, 1.f);
    setFieldFloat(best, fields().vr_foegrab_letgo, 0.f);
    addFoe(best);
    if(debug())
    {
        Con_Printf("foegrab: %s hand takes hold of %s (%d) at (%.1f %.1f %.1f), %.1f cm from its fist, hold %.2f\n",
            handName(h), PR_GetString(best->v.classname), g.ent, point.x, point.y, point.z, bestGap / cm(), g.strength);
    }
    (void)callQc("VR_FoeGrab_Taken", player, best, static_cast<float>(h), 0.f, 0.f);
    return true;
}

// Whether `name` is one of the words of `list` (spaces or commas between them); "infected" stands for Dawn of the
// Machine's infected monsters (`infected`: this one is).
[[nodiscard]] bool inList(const char* list, const char* name, bool infected)
{
    char word[64];
    const char* p = list;
    while(*p)
    {
        while(*p == ' ' || *p == ',' || *p == '\t')
        {
            p++;
        }
        int n = 0;
        while(*p && *p != ' ' && *p != ',' && *p != '\t')
        {
            if(n < 63)
            {
                word[n++] = *p;
            }
            p++;
        }
        word[n] = 0;
        if(n > 0 && (!q_strcasecmp(word, name) || (infected && !q_strcasecmp(word, "infected"))))
        {
            return true;
        }
    }
    return false;
}

// The throw's kind of `m` (QC VR_FoeGrab_Throw's xTier): 0 always thrown, 1 only when hurt, 2 never (by the class lists:
// vr_foegrab_throw_always, _when_hurt); -1 by its mass (vr_foegrab_throw_by_mass: the QC's).
[[nodiscard]] float throwTier(edict_t* m)
{
    if(vr_foegrab_throw_by_mass.value != 0.f)
    {
        return -1.f;
    }
    const char* c = PR_GetString(m->v.classname);
    const bool infected = fieldFloatOr(m, fields().vr_mg3_infected, 0.f) != 0.f;
    if(inList(vr_foegrab_throw_always.string, c, infected))
    {
        return 0.f;
    }
    return inList(vr_foegrab_throw_when_hurt.string, c, infected) ? 1.f : 2.f;
}

[[nodiscard]] const char* throwResult(float r)
{
    return r >= 2.f ? "the training dummy: would be thrown (it stands)" :
           r >= 1.f ? "thrown down" :
           r >= 0.f ? "never thrown (its kind)" :
           r >= -1.f ? "not hurt enough" : "can't be knocked down now (no ragdoll or get-up, no room, down already)";
}

// The fall of `m`'s ragdoll, thrown or shoved down towards level `dir`, traced from now (ThrowTrace, `kind` "throw" or
// "shove"). False: no ragdoll.
bool startTrace(edict_t* m, const glm::vec3& dir, const char* kind)
{
    glm::vec3 pelvis, head, feet;
    if(!box3d::ragdollStance(NUM_FOR_EDICT(m), pelvis, head, feet))
    {
        return false;
    }
    throwTrace = ThrowTrace{NUM_FOR_EDICT(m), qcvm->time, 0, dir, feet};
    throwTrace.pelvis0 = pelvis;
    throwTrace.lastTorso = head - pelvis;
    throwTrace.kind = kind;
    throwTrace.viewer = svs.maxclients >= 1 ? vec(EDICT_NUM(1)->v.origin) : feet;
    return true;
}

// `player`'s two-hand throw of `m` towards level `dir` (the QC's VR_FoeGrab_Throw: its result), the hands turning it about
// the vertical at `twist` rad/s. Knocked down (1), its ragdoll is turned over about its feet towards `dir`, a sweep
// (box3d::ragdollTopple: vr_foegrab_throw_topple), its feet swept back (_feet_speed), spun with the hands' twist (vr_foegrab_throw_spin);
// `trace`: its fall printed (ThrowTrace).
float throwDown(edict_t* player, edict_t* m, const glm::vec3& dir, float twist, bool trace)
{
    const float yaw = glm::degrees(za::atan2(dir.y, dir.x));
    const float result = callQc("VR_FoeGrab_Throw", player, m, yaw, throwTier(m), 0.f);
    // His grunt as he throws it (his note vrfiringrange_2026-10-09_10-56; the training dummy's "would be thrown" too).
    const bool grunted = result >= 1.f && climb::grunt(player, vr_foegrab_throw_grunt_sound.value, vr_foegrab_throw_grunt.value);
    if(grunted && debug())
    {
        Con_Printf("foegrab: throw grunt %s %.2f\n", climb::gruntSample(vr_foegrab_throw_grunt_sound.value),
            za::clamp(vr_foegrab_throw_grunt.value, 0.f, 1.f));
    }
    if(result != 1.f)
    {
        return result;
    }
    constexpr float maxSpin = 4.f * 3.14159265f; // rad/s (720 degrees/s)
    const float spin = za::clamp(twist * za::max(vr_foegrab_throw_spin.value, 0.f), -maxSpin, maxSpin);
    const float topple = glm::radians(za::clamp(vr_foegrab_throw_topple.value, 0.f, 2000.f));
    const bool toppled = topple > 0.f &&
                         box3d::ragdollTopple(m, dir, topple, spin, za::clamp(vr_foegrab_throw_feet_speed.value, 0.f, 1000.f));
    // Its fall (vr_ragdoll_active_stagger): slack where the hands held it (they let go now; the test's, none: its upper
    // arms, where two hands take an enemy), the rest curling, its arms bracing.
    glm::vec3 palms[2];
    int held = 0;
    const int c = NUM_FOR_EDICT(player) - 1;
    for(int h = 0; h < 2 && c >= 0 && c < static_cast<int>(MAX_SCOREBOARD); h++)
    {
        const Hold& g = holders[c].holds[h];
        if(g.active && g.ent == NUM_FOR_EDICT(m))
        {
            palms[held++] = g.palmAt;
        }
    }
    if(held == 0)
    {
        const glm::vec3 lo = vec(m->v.absmin), hi = vec(m->v.absmax);
        const glm::vec3 side = glm::cross(glm::vec3{0.f, 0.f, 1.f}, dir) * (0.3f * (hi.x - lo.x));
        const glm::vec3 chest{(lo.x + hi.x) * 0.5f, (lo.y + hi.y) * 0.5f, lo.z + 0.7f * (hi.z - lo.z)};
        palms[held++] = chest + side;
        palms[held++] = chest - side;
    }
    (void)box3d::ragdollThrown(m, dir, palms, held);
    // The hands that held it are in it as it goes, still moving (the pull's follow-through), and he stands where a throw
    // towards him lands it: they knocked it back up and over the other way. It passes through them a moment.
    (void)box3d::ragdollGrace(m, NUM_FOR_EDICT(player), za::clamp(vr_foegrab_throw_grace.value, 0.f, 5.f));
    if(trace && startTrace(m, dir, "throw"))
    {
        Con_Printf("throw trace: %s (%d) %s towards yaw %.0f, spin %.0f deg/s (hands' twist %.0f deg/s)\n",
            PR_GetString(m->v.classname), throwTrace.ent, toppled ? "toppled over its feet" : "pushed whole (no topple)", yaw,
            toppled ? glm::degrees(spin) : 0.f, glm::degrees(twist));
    }
    return result;
}

// The throw trace's step (after a throw it follows: ThrowTrace).
void throwTraceStep()
{
    ThrowTrace& t = throwTrace;
    if(t.ent <= 0)
    {
        return;
    }
    glm::vec3 pelvis, head, feet;
    const double age = qcvm->time - t.start;
    if(t.ent >= qcvm->num_edicts || EDICT_NUM(t.ent)->free || !box3d::ragdollStance(t.ent, pelvis, head, feet))
    {
        Con_Printf("throw trace: its ragdoll gone at %.2f s\n", age);
        t.ent = 0;
        return;
    }
    const glm::vec3 torso = head - pelvis;
    const float tilt = glm::degrees(za::acos(za::clamp(torso.z / za::max(glm::length(torso), 1e-3f), -1.f, 1.f)));
    const float feetMoved = glm::length(glm::vec2{feet - t.feet0});
    t.maxTilt = za::max(t.maxTilt, tilt);
    t.maxFeet = za::max(t.maxFeet, feetMoved);
    t.headDown = t.headDown < 0.f && head.z < t.pelvis0.z ? static_cast<float>(age) : t.headDown;
    t.feetGo = t.feetGo < 0.f && glm::dot(feet - t.feet0, t.dir) > 8.f ? static_cast<float>(age) : t.feetGo;
    if(t.tiltHalf < 0.f && age >= 0.5)
    {
        t.tiltHalf = tilt;
    }
    if(age < 0.1 * (t.printed + 1))
    {
        return;
    }
    t.printed++;
    // (Its turn summed a tenth of a second at a time: a frame's jitter isn't counted; 1800 deg/s is still caught.)
    if(glm::length(torso) > 1e-3f && glm::length(t.lastTorso) > 1e-3f)
    {
        t.turned += glm::degrees(
            za::acos(za::clamp(glm::dot(glm::normalize(t.lastTorso), glm::normalize(torso)), -1.f, 1.f)));
    }
    t.lastTorso = torso;
    const glm::vec3 side = glm::cross(glm::vec3{0.f, 0.f, 1.f}, t.dir);
    Con_Printf("%s trace: %.2f s tilt %.0f deg (turned %.0f), feet moved %.1f (%.1f along the %s, %.1f up), pelvis "
               "%.1f along, head %.1f along the %s from the feet (%.1f aside), %.1f up; pelvis %.1f up\n",
        t.kind, age, tilt, t.turned, feetMoved, glm::dot(feet - t.feet0, t.dir), t.kind, feet.z - t.feet0.z,
        glm::dot(pelvis - t.pelvis0, t.dir), glm::dot(head - feet, t.dir), t.kind, glm::dot(head - feet, side),
        head.z - feet.z, pelvis.z - feet.z);
    if(t.printed >= 15)
    {
        Con_Printf("%s trace done: tilt at 0.5 s %.0f deg, most %.0f, turned %.0f in all; feet moved %.1f at most, %.1f at "
                   "the end (%.1f along the %s), its pelvis %.1f along it; its head below its pelvis's start at %.2f s, "
                   "its feet 8 units on at %.2f s; lies %s (head %.1f along the %s)\n",
            t.kind, t.tiltHalf, t.maxTilt, t.turned, t.maxFeet, feetMoved, glm::dot(feet - t.feet0, t.dir), t.kind,
            glm::dot(pelvis - t.pelvis0, t.dir), t.headDown, t.feetGo,
            glm::dot(head - feet, t.dir) > 0.f ? "towards it" : "against it", glm::dot(head - feet, t.dir), t.kind);
        // Where it lies from the first player's side: its head from its feet (the way it lies: a throw goes over its head
        // and its legs flop on over or back as they land, a low one's head 5 or 20 units on of its feet), and its head
        // from where its feet stood (where the throw took its top: steadier).
        glm::vec3 ahead = t.feet0 - t.viewer;
        ahead.z = 0.f;
        ahead = glm::length(ahead) > 1e-3f ? glm::normalize(ahead) : glm::vec3{1.f, 0.f, 0.f};
        const glm::vec3 left = glm::cross(glm::vec3{0.f, 0.f, 1.f}, ahead);
        const glm::vec3 lies = head - feet;
        const glm::vec3 went = head - t.feet0;
        Con_Printf("%s trace done: lies %.1f ahead, %.1f to your left (its head from its feet); its head %.1f ahead, "
                   "%.1f to your left of where it stood\n",
            t.kind, glm::dot(lies, ahead), glm::dot(lies, left), glm::dot(went, ahead), glm::dot(went, left));
        t.ent = 0;
    }
}

// The two-hand throw (vr_foegrab_throw; QC vr_foegrab_throw.qc): both hands of player `c` holding one enemy, turning it
// over. The turn's way: the level way it tips its top (turn x up; the hands' own velocities, not the player's; metres,
// m/s), two parts added:
// - the hands turning it between them as a wheel, by their vertical difference alone: -dv.z d' / |d|^2, d from the off
//   palm to the main one (at least throwMinArm), d' its level part, dv the main hand's velocity less the off hand's
//   (one shoulder pushed down, the other pulled up: it tips towards the lower). (Their level difference with the
//   hands at different heights, a twist's or a pull's unevenness, is no wheel: it picked the side by which hand was
//   higher.)
// - both together toppling it over its feet (a standing body's pivot; about its box's middle, hands held below the
//   middle flipped the side): (r.z v' - v.z r') / (|r| |m|), r from under its middle at its feet to the palms' middle,
//   v their mean velocity, ' the level parts (its top pulled where they pull it; pulled down at its front, towards
//   you), m from its box's middle to the palms' (at least throwMinArm: the lever the turn was measured with before).
// In degrees per second, at vr_foegrab_throw_twist or more with the hands' mean speed at vr_foegrab_throw_speed or more,
// it goes down that way, smoothed over throwWayTime (a reversal: the new way at once): to a side, towards you; straight
// away from you (within 45 degrees) is a shove's, unless vr_foegrab_throw_away. A twist about the vertical is no throw
// (its spin: the old turn's vertical part). The QC decides by its kind and health; thrown, both hands let go.
// `m`, thrown (or lifted: `lift`), its ragdoll just made: each hand of `player` holding it holds on to its ragdoll's limb
// nearest the hand, softly (box3d::ragdollHoldSoft: at most `strength` times its weight per hand at `hertz`, not glued:
// it falls, and the hands steer and drag it; a grunt's ragdoll weighs about 2100 N), as the QC's ragdoll holds are
// (VR_FoeGrab_HandOver: the hand's held field; the QC's VR_Ragdoll_HandFrame lets go of it as any, with the hand's throw),
// the grip passed on to the QC; the holds let go (`why`: Why::Thrown, Why::Lifted, Why::Snapped: its neck snapped, a body
// held by its head: each hand holds its head's part).
// How many hands hold on (0: none could, the holds kept).
int handOver(edict_t* player, edict_t* m, Holder& hd, float strength, float hertz, Why why)
{
    const bool lift = why == Why::Lifted;
    const float force = za::clamp(strength, 0.f, 10.f) * box3d::ragdollWeight(m);
    int bits = 0;
    for(int h = 0; h < 2; h++)
    {
        const Hold& g = hd.holds[h];
        if(!g.active || g.ent != NUM_FOR_EDICT(m) ||
            !box3d::ragdollHoldSoft(m, player, h, softReach, force, za::clamp(hertz, 0.5f, 30.f), softStuck,
                why == Why::Snapped))
        {
            continue;
        }
        if(callQc("VR_FoeGrab_HandOver", player, m, static_cast<float>(h), 0.f, 0.f) >= 1.f)
        {
            bits |= 1 << h;
        }
        else
        {
            box3d::ragdollRelease(player, h, glm::vec3{0.f});
        }
    }
    if(bits == 0)
    {
        return 0;
    }
    for(int h = 0; h < 2; h++)
    {
        release(player, h, why, (bits & (1 << h)) ? "held on as a ragdoll" : why == Why::Snapped ? "its neck snapped" : "thrown");
        if(bits & (1 << h))
        {
            hd.owned[h] = hd.ownedLast[h] = false; // (the grip is the QC's ragdoll hold's now)
        }
    }
    hd.carried = NUM_FOR_EDICT(m);
    hd.carriedSince = qcvm->time;
    hd.lifted = lift;
    hd.snapped = why == Why::Snapped;
    hd.breakAt = -1.0;
    const int hands = (bits & 1) + ((bits >> 1) & 1);
    if(debug())
    {
        Con_Printf("foegrab: %s (%d) %s, held on as a ragdoll by %d hand%s (%.0f N each, %.1f Hz; it weighs %.0f N)\n",
            PR_GetString(m->v.classname), hd.carried, lift ? "lifted" : hd.snapped ? "snapped" : "thrown", hands, hands == 1 ? "" : "s", force, hertz,
            box3d::ragdollWeight(m));
    }
    return hands;
}

// The hands of player `c` holding the ragdoll handOver gave them (the QC's held field and the engine's hold), 0: none.
[[nodiscard]] int carryingHands(int c, edict_t* m)
{
    edict_t* player = EDICT_NUM(c + 1);
    int hands = 0;
    for(int h = 0; h < 2 && m && !m->free; h++)
    {
        const int heldOfs = h ? fields().mainhand_held : fields().offhand_held;
        hands += heldOfs >= 0 && fieldInt(player, heldOfs) == EDICT_TO_PROG(m) && box3d::ragdollHeld(player, h) == 1;
    }
    return hands;
}

// A lifted enemy of player `c` breaking free (vr_foegrab_lift_time): the hands holding it lose it (the engine's holds;
// the QC's held fields), its flail ends, it shoves him back vr_foegrab_lift_shove units and gets up as soon as it lies
// (QC VR_FoeGrab_BreakFree).
void breakFree(int c, edict_t* m)
{
    Holder& hd = holders[c];
    edict_t* player = EDICT_NUM(c + 1);
    for(int h = 0; h < 2; h++)
    {
        const int heldOfs = h ? fields().mainhand_held : fields().offhand_held;
        if(heldOfs >= 0 && fieldInt(player, heldOfs) == EDICT_TO_PROG(m))
        {
            box3d::ragdollRelease(player, h, glm::vec3{0.f});
        }
    }
    (void)box3d::ragdollFlail(m, false);
    const float shove = za::clamp(vr_foegrab_lift_shove.value, 0.f, 500.f);
    (void)callQc("VR_FoeGrab_BreakFree", player, m, shove, 0.f, 0.f);
    if(debug())
    {
        Con_Printf("foegrab: %s (%d) breaks free after %.2f s lifted: shoves you back %.0f units\n",
            PR_GetString(m->v.classname), hd.carried, qcvm->time - hd.carriedSince, shove);
    }
    hd.carried = 0;
}

// Each frame, the enemy player `c` holds on as a ragdoll (handOver), while a hand holds it: it passes through his hands
// (carryGrace: they hold it, they don't knock it); lifted, it breaks free at breakAt (breakFree), its flail over once let
// go of or dead. Let go of (the grips, its get-up, a hold left behind): over.
void carryStep(int c)
{
    Holder& hd = holders[c];
    if(hd.carried <= 0)
    {
        return;
    }
    edict_t* m = hd.carried < qcvm->num_edicts ? EDICT_NUM(hd.carried) : nullptr;
    if(carryingHands(c, m) == 0)
    {
        if(debug())
        {
            Con_Printf("foegrab: %s (%d) let go of after %.2f s held on\n", m && !m->free ? PR_GetString(m->v.classname) : "?",
                hd.carried, qcvm->time - hd.carriedSince);
        }
        if(m && !m->free && hd.lifted)
        {
            (void)box3d::ragdollFlail(m, false);
        }
        hd.carried = 0;
        return;
    }
    (void)box3d::ragdollGrace(m, c + 1, carryGrace);
    if(hd.lifted && m->v.health <= 0.f)
    {
        hd.lifted = false; // (killed in the hands: a body held, its flail a dying one's: box3d's livingSwim)
    }
    if(hd.lifted && hd.breakAt >= 0.0 && qcvm->time >= hd.breakAt)
    {
        breakFree(c, m);
    }
}

// A QC function of the monster taking a point (with the player as self): float f(entity m, vector at, float extra).
float callQcAt(const char* name, edict_t* player, edict_t* m, const glm::vec3& at, float extra, float fallback)
{
    const func_t fn = findFunction(name);
    if(!fn)
    {
        return fallback;
    }
    const int oldSelf = pr_global_struct->self, oldOther = pr_global_struct->other;
    pr_global_struct->time = qcvm->time;
    pr_global_struct->self = EDICT_TO_PROG(player);
    pr_global_struct->other = EDICT_TO_PROG(m);
    G_INT(OFS_PARM0) = EDICT_TO_PROG(m);
    setVec(G_VECTOR(OFS_PARM1), at);
    G_FLOAT(OFS_PARM2) = extra;
    PR_ExecuteProgram(fn);
    const float r = G_FLOAT(OFS_RETURN);
    pr_global_struct->self = oldSelf;
    pr_global_struct->other = oldOther;
    return r;
}

// The spot hold `g` holds (world): on its model as its body carries it, moved by the animation's eased move of it.
[[nodiscard]] glm::vec3 spotOf(const Hold& g)
{
    glm::vec3 origin{0.f};
    glm::mat3 body{1.f};
    (void)bodyFrame(EDICT_NUM(g.ent), origin, body);
    return origin + body * g.local + g.anim;
}

// The neck snap (vr_foegrab_necksnap; QC VR_FoeGrab_NeckSnap): both hands of a player holding a humanoid by the head
// (neckCheck: the holds' spots in its head zone, standing; ragdollNeckCheck: its ragdoll's head, knocked down, staggering,
// lifted, thrown and held on) twist it round: its neck snaps, it dies on the spot, its body limp in the hands.
struct NeckTwist
{
    float speed{0.f};    // the hands' twist of the head now, degrees/s (smoothed)
    float turned{0.f};   // how far this stroke has turned it one way, degrees
    float peak{0.f};     // the stroke's fastest twist, degrees/s
    bool twisting{false}; // the hands twist it more than they pull it together (no throw, no lift)
};

// Each frame both hands of player `c` hold `m` by the head, `neck` its neck's line (unit, world: up standing; a ragdoll's
// from its neck's joint to its head, as it lies): the hands' twist of it, from their places and from their turns.
// - The palms: the line between them (d, off palm to main palm, metres) swept round, d x (v1 - v0) / |d|^2 (v their
//   velocities, m/s: opposite motion across d). About the neck it is the angle d's shadow across the neck sweeps,
//   n.(d x (v1 - v0)) / |d across n|^2 (at least neckMinArm and half |d|): one hand pushed forward and the other pulled back, the hands
//   either side of its head, turns it about its neck however they are turned.
// - The wrists: both hands turning the same way (each hand's turn since the last frame, from its angles; about each of
//   the neck, the line between the hands and the third axis the lesser of the two when they agree, else none): the
//   wrists rolled or snapped round.
// Each axis takes the larger of the two (a hand gripping the head turns as much as the line between the hands sweeps:
// either way measures the same turn), smoothed over neckSmooth. It is a twist when each palm's speed about their middle
// (the wrists' turn at neckRollArm) is at least neckPullShare of their mean velocity: a pull together is not. A stroke
// adds up its turn one way while it twists at neckFloor of vr_foegrab_necksnap_speed or more (another way, more than
// 60 degrees off, starts another); slower, it ends. Its fastest twist is kept (vr_foegrab_necksnap_speed: neckCheck).
NeckTwist neckStep(int c, edict_t* m, const VrMove& move, const glm::vec3& neck)
{
    Holder& hd = holders[c];
    edict_t* player = EDICT_NUM(c + 1);
    const int ent = NUM_FOR_EDICT(m);
    const double time = qcvm->time;
    const float dt = static_cast<float>(za::clamp(host_frametime, 0.0, 0.1));
    const bool fresh = hd.neckEnt != ent || time - hd.neckLast > 0.1;
    const float toMetres = 1.f / units::metresToUnits();
    // (The controllers' own points, as their velocities are: the drawn hands, held out of the level or a ragdoll's
    // limbs, kept the line between them short while the controllers moved on, its turn read up to half again too fast.)
    const glm::vec3 d = (trackedPalmOf(player, move, 1) - trackedPalmOf(player, move, 0)) * toMetres;
    const glm::vec3 &v0 = move.hands[0].vel, &v1 = move.hands[1].vel;
    const glm::vec3 n = glm::length(neck) > 1e-4f ? glm::normalize(neck) : glm::vec3{0.f, 0.f, 1.f};
    const float len2 = za::max(glm::dot(d, d), neckMinArm * neckMinArm);
    const glm::vec3 across = d - n * glm::dot(n, d);
    // (Hands nearly along the neck can't show its turn: at most twice the arm's own measure, 30 degrees off it.)
    const float acrossLen2 = za::max(glm::dot(across, across), za::max(neckMinArm * neckMinArm, 0.25f * len2));
    const glm::vec3 sweep = glm::cross(d, v1 - v0);
    const float aboutNeck = glm::dot(n, sweep) / acrossLen2;
    // (Never faster than the line between the palms turns, |d x (v1 - v0)| / |d|^2: its shadow across a neck that
    // wobbles as a ragdoll lies, the line nearly along it, would add up to that again.)
    const float lineTurn = glm::length(sweep) / len2;
    glm::vec3 palms = (sweep - n * glm::dot(n, sweep)) / len2 + n * aboutNeck;
    if(glm::length(palms) > lineTurn)
    {
        palms *= lineTurn / glm::length(palms);
    }
    // (The axes: the neck, the line between the hands across it, the third.)
    const glm::vec3 side = glm::length(across) > 1e-4f ? glm::normalize(across) : glm::vec3{0.f};
    const glm::vec3 axes[3]{n, side, glm::cross(n, side)};
    glm::vec3 turn[2]{glm::vec3{0.f}, glm::vec3{0.f}};
    for(int h = 0; h < 2; h++)
    {
        const glm::mat3 rot = held::axesFromAngles(&move.hands[h].rot[0], true);
        if(!fresh && dt > 0.f)
        {
            // (A small turn's axis times its angle: the skew part of the turn since the last frame.)
            const glm::mat3 dr = rot * glm::transpose(hd.neckRot[h]);
            turn[h] = glm::vec3{dr[1][2] - dr[2][1], dr[2][0] - dr[0][2], dr[0][1] - dr[1][0]} * (0.5f / dt);
        }
        hd.neckRot[h] = rot;
    }
    glm::vec3 wrists{0.f}, spin{0.f};
    for(const glm::vec3& a : axes)
    {
        const float w0 = glm::dot(turn[0], a), w1 = glm::dot(turn[1], a);
        const float w = w0 * w1 > 0.f ? (za::abs(w0) < za::abs(w1) ? w0 : w1) : 0.f;
        const float p = glm::dot(palms, a);
        wrists += a * w;
        spin += a * (za::abs(p) >= za::abs(w) ? p : w);
    }
    if(fresh)
    {
        hd.neckSpin = spin;
        hd.neckTurned = 0.f;
        hd.neckAxis = glm::vec3{0.f};
        hd.neckPeak = 0.f;
        hd.neckStroke = 0.f;
    }
    else
    {
        hd.neckSpin = glm::mix(hd.neckSpin, spin, 1.f - za::exp(-dt / neckSmooth));
    }
    hd.neckEnt = ent;
    hd.neckLast = time;
    const float rate = glm::length(hd.neckSpin);
    const float tangential = za::max(glm::length(palms) * 0.5f * za::sqrt(len2), glm::length(wrists) * neckRollArm);
    const float common = glm::length(0.5f * (v0 + v1));
    NeckTwist t;
    t.speed = glm::degrees(rate);
    t.twisting = tangential >= neckPullShare * common && tangential > 0.05f;
    const float floor = glm::radians(za::max(vr_foegrab_necksnap_speed.value, 1.f)) * neckFloor;
    if(t.twisting && rate >= floor)
    {
        const glm::vec3 way = hd.neckSpin / rate;
        if(glm::dot(way, hd.neckAxis) > 0.5f)
        {
            hd.neckTurned += rate * dt;
            hd.neckStroke = za::max(hd.neckStroke, rate);
        }
        else
        {
            hd.neckTurned = rate * dt;
            hd.neckStroke = rate;
        }
        hd.neckAxis = way;
    }
    else
    {
        hd.neckTurned = 0.f;
        hd.neckAxis = glm::vec3{0.f};
        hd.neckStroke = 0.f;
    }
    hd.neckPeak = za::max(hd.neckPeak, t.speed);
    t.turned = glm::degrees(hd.neckTurned);
    t.peak = glm::degrees(hd.neckStroke);
    if(debug(2))
    {
        Con_Printf("foegrab: both hands on %s (%d)'s head: twist %.0f deg/s (about its neck: the palms %.0f, the wrists "
                   "%.0f; the palms' line %.0f, %.2f m long), stroke %.0f deg, fastest %.0f deg/s; each palm %.2f m/s "
                   "about their middle, together %.2f m/s%s\n",
            PR_GetString(m->v.classname), ent, t.speed, glm::degrees(glm::dot(palms, n)), glm::degrees(glm::dot(wrists, n)),
            glm::degrees(lineTurn), za::sqrt(glm::dot(d, d)), t.turned, t.peak, tangential, common,
            t.twisting ? " (a twist)" : "");
    }
    return t;
}

// Both hands of player `c` twisted `m`'s head far enough (neckStep): a humanoid's (vr_foegrab_lift_classes: never an
// ogre, a fiend, a shambler, a vore) neck snaps (QC VR_FoeGrab_NeckSnap: it dies on the spot, killed by him, whole, the
// crack loud; standing, it is knocked down first, its ragdoll made). Its body stays in his hands, limp: standing
// (`ragdoll` false), both holds hand over to its ragdoll (handOver at vr_foegrab_necksnap_strength of its weight a hand,
// a lift's spring: carried until the grips let go); a ragdoll already, the hands' ragdoll holds go on.
void neckSnap(int c, edict_t* m, const NeckTwist& t, bool ragdoll)
{
    Holder& hd = holders[c];
    edict_t* player = EDICT_NUM(c + 1);
    const double time = qcvm->time;
    hd.neckAgain = time + throwRetry;
    hd.neckTurned = 0.f;
    hd.neckAxis = glm::vec3{0.f};
    const bool dummy = isTrainingDummy(m);
    const char* name = dummy ? "the training dummy" : PR_GetString(m->v.classname);
    const int num = NUM_FOR_EDICT(m);
    const bool infected = fieldFloatOr(m, fields().vr_mg3_infected, 0.f) != 0.f;
    if(!dummy && !inList(vr_foegrab_lift_classes.string, PR_GetString(m->v.classname), infected))
    {
        if(debug())
        {
            Con_Printf("foegrab: both hands twist %s (%d)'s head %.0f deg at %.0f deg/s: no neck snap, not a humanoid "
                       "(vr_foegrab_lift_classes)\n",
                name, num, t.turned, t.peak);
        }
        return;
    }
    const float result = callQc("VR_FoeGrab_NeckSnap", player, m, za::clamp(vr_foegrab_necksnap_volume.value, 0.f, 1.f),
        ragdoll ? 1.f : 0.f, 0.f);
    if(debug() || dummy)
    {
        Con_Printf("foegrab: both hands snap %s (%d)'s neck, twisted %.0f deg at %.0f deg/s (%s): %s\n", name, num, t.turned,
            t.peak, ragdoll ? "a ragdoll" : "standing",
            result >= 3.f ? "killed, no ragdoll to hold" : result >= 2.f ? "the training dummy (it stands)" :
            result >= 1.f ? "killed" : "it can't die now");
    }
    if(result < 1.f || result == 2.f)
    {
        return;
    }
    if(ragdoll)
    {
        (void)box3d::ragdollFlail(m, false); // (lifted: limp now)
        hd.carried = num;
        hd.carriedSince = time;
        hd.lifted = false;
        hd.snapped = true;
        hd.breakAt = -1.0;
        return;
    }
    if(result == 1.f && handOver(player, m, hd, vr_foegrab_necksnap_strength.value, vr_foegrab_necksnap_hertz.value,
                            Why::Snapped) > 0)
    {
        return;
    }
    for(int h = 0; h < 2; h++)
    {
        release(player, h, Why::Snapped, "its neck snapped, nothing to hold on to");
    }
}

// Whether a stroke of the hands' twist (neckStep) snaps the neck: turned vr_foegrab_necksnap_angle one way, at
// vr_foegrab_necksnap_speed at its fastest, not just after a snap.
[[nodiscard]] bool neckSnapDue(const Holder& hd, const NeckTwist& t)
{
    return t.turned >= za::max(vr_foegrab_necksnap_angle.value, 1.f) &&
           t.peak >= za::max(vr_foegrab_necksnap_speed.value, 1.f) && qcvm->time >= hd.neckAgain;
}

// Both hands' holds of player `c` on `m` (twoHandCheck): at its head, the headshots' zone (QC VR_FoeGrab_OnHead: each
// hold's spot as on its model standing, within its head sphere grown by vr_foegrab_necksnap_margin: down to the neck's
// base), the twist measured about its up (neckStep) and its neck snapped (neckSnapDue, neckSnap). True: both hands at its
// head, twisting or not: the neck snap is the only move then (the lift and the throw aren't tried; Neck Snap off, they
// are).
bool neckCheck(int c, edict_t* m, const VrMove& move)
{
    Holder& hd = holders[c];
    if(vr_foegrab_necksnap.value == 0.f)
    {
        return false;
    }
    edict_t* player = EDICT_NUM(c + 1);
    const float margin = za::clamp(vr_foegrab_necksnap_margin.value, -8.f, 32.f);
    bool onHead[2]{false, false};
    for(int h = 0; h < 2; h++)
    {
        onHead[h] = callQcAt("VR_FoeGrab_OnHead", player, m, spotOf(hd.holds[h]), margin, 0.f) >= 1.f;
    }
    if(!onHead[0] || !onHead[1])
    {
        if(debug(2))
        {
            // (The spots in its frame: forward, left, up from its origin.)
            glm::vec3 origin{0.f}, s[2];
            glm::mat3 body{1.f};
            (void)bodyFrame(m, origin, body);
            for(int h = 0; h < 2; h++)
            {
                s[h] = glm::transpose(body) * (spotOf(hd.holds[h]) - origin);
            }
            Con_Printf("foegrab: both hands on %s (%d), at its head: the off hand %s (%.1f %.1f %.1f), the main %s (%.1f "
                       "%.1f %.1f)\n",
                PR_GetString(m->v.classname), NUM_FOR_EDICT(m), onHead[0] ? "yes" : "no", s[0].x, s[0].y, s[0].z,
                onHead[1] ? "yes" : "no", s[1].x, s[1].y, s[1].z);
        }
        return false;
    }
    const NeckTwist t = neckStep(c, m, move, glm::vec3{0.f, 0.f, 1.f});
    if(neckSnapDue(hd, t))
    {
        neckSnap(c, m, t, false);
    }
    return true;
}

// Each frame, for player `c` without the foegrab's holds: both his hands holding one living monster's ragdoll (the QC's
// ragdoll holds: VR_Ragdoll_Handtouch's, a knocked-down or staggering one's; handOver's, lifted or thrown and held on) by
// its head or neck (the part each holds one of its head's, or the palm nearest its head's part, or within
// vr_foegrab_necksnap_margin of its neck's joint, or within neckGrip of its neck: vr_foegrab_necksnap_neck units from the
// joint down to its body): the twist measured about its neck as
// it lies (box3d::ragdollNeck; neckStep) and its neck snapped (neckSnapDue, neckSnap).
void ragdollNeckCheck(int c)
{
    Holder& hd = holders[c];
    edict_t* player = EDICT_NUM(c + 1);
    const VrMove* move = server::clientMove(player);
    const int offOfs = fields().offhand_held, mainOfs = fields().mainhand_held;
    if(vr_foegrab_necksnap.value == 0.f || !move || hd.holds[0].active || hd.holds[1].active || offOfs < 0 || mainOfs < 0)
    {
        return;
    }
    const int held = fieldInt(player, offOfs);
    if(held == 0 || fieldInt(player, mainOfs) != held)
    {
        return;
    }
    edict_t* m = PROG_TO_EDICT(held);
    const int num = NUM_FOR_EDICT(m);
    if(m->free || !(static_cast<int>(m->v.flags) & FL_MONSTER) || m->v.health <= 0.f || !box3d::isRagdoll(num))
    {
        return;
    }
    const uint32_t headBones = box3d::limbInfo(m, 1, 0);
    const float margin = za::clamp(vr_foegrab_necksnap_margin.value, 0.f, 32.f);
    const float neckLength = za::clamp(vr_foegrab_necksnap_neck.value, 0.f, 32.f);
    glm::vec3 joint{0.f}, neck{0.f, 0.f, 1.f}, head{0.f};
    if(!box3d::ragdollNeck(num, joint, neck, head))
    {
        return;
    }
    // (A stroke under way goes on while both hands hold it: the twist itself sweeps a palm held at its neck out of the zone,
    // the stroke cut at 27 of the 30 degrees.)
    const bool stroking = hd.neckEnt == num && hd.neckTurned > 0.f && qcvm->time - hd.neckLast <= 0.1;
    for(int h = 0; h < 2; h++)
    {
        box3d::RagdollHold rh;
        if(box3d::ragdollHeld(player, h) != 1 || !box3d::ragdollHold(c + 1, h, rh) || rh.num != num)
        {
            return;
        }
        const bool part = stroking || (rh.part >= 0 && rh.part < 32 && (headBones & (1u << rh.part)) != 0u);
        const glm::vec3 palm = palmOf(player, *move, h);
        const float along = za::clamp(glm::dot(joint - palm, neck), 0.f, neckLength);
        const bool onNeck = glm::distance(palm, joint - neck * along) <= neckGrip;
        if(!part && !onNeck && box3d::ragdollHeadAt(num, palm, margin) != 1)
        {
            return;
        }
    }
    const NeckTwist t = neckStep(c, m, *move, neck);
    if(neckSnapDue(hd, t))
    {
        neckSnap(c, m, t, true);
    }
}

// Both hands of one player on one enemy, as twoHandCheck measures them each frame (the throw's, the lift's): metres, m/s.
struct TwoHands
{
    edict_t* player{nullptr};
    edict_t* m{nullptr};
    glm::vec3 middle{0.f}; // its box's middle (units)
    glm::vec3 vm{0.f};     // the hands' mean velocity
    float speed{0.f};      // their mean speed
    glm::vec3 way{0.f};    // the turn's way now (level, rad/s: where it tips its top)
    float twist{0.f};      // its size, degrees/s
    float spin{0.f};       // the hands' turn of it about the vertical, rad/s
};

bool liftCheck(int c, const TwoHands& t);
void throwCheck(int c, const TwoHands& t);

// Each frame, for player `c`: both his hands on one enemy measured (TwoHands), then its two-hand moves tried in turn: the
// lift (liftCheck: an upward pull is never a throw), the throw (throwCheck).
void twoHandCheck(int c)
{
    Holder& hd = holders[c];
    const Hold& a = hd.holds[0];
    const Hold& b = hd.holds[1];
    edict_t* player = EDICT_NUM(c + 1);
    const VrMove* move = server::clientMove(player);
    if(!move || !a.active || !b.active || a.ent != b.ent)
    {
        if(hd.twoHanded && debug())
        {
            Con_Printf("foegrab: both hands' hold ended: its hardest turn %.0f deg/s, the hands at %.2f m/s (throws at %.0f "
                       "deg/s, %.2f m/s)\n",
                hd.peakTwist, hd.peakSpeed, vr_foegrab_throw_twist.value, vr_foegrab_throw_speed.value);
        }
        hd.twoHanded = false;
        return;
    }
    if(!hd.twoHanded)
    {
        hd.twoHanded = true;
        hd.peakTwist = hd.peakSpeed = 0.f;
        hd.way = glm::vec3{0.f};
    }
    const double time = qcvm->time;
    if(time - za::max(a.since, b.since) < throwSettle)
    {
        return;
    }
    edict_t* m = EDICT_NUM(a.ent);
    const float toMetres = 1.f / units::metresToUnits();
    const glm::vec3 middle = (vec(m->v.absmin) + vec(m->v.absmax)) * 0.5f;
    glm::vec3 r[2];
    for(int h = 0; h < 2; h++)
    {
        r[h] = (palmOf(player, *move, h) - middle) * toMetres;
    }
    const glm::vec3 &v0 = move->hands[0].vel, &v1 = move->hands[1].vel;
    const float speed = 0.5f * (glm::length(v0) + glm::length(v1));
    const glm::vec3 d = r[1] - r[0];
    const glm::vec3 wheel = glm::cross(d, v1 - v0) / za::max(glm::dot(d, d), throwMinArm * throwMinArm);
    const glm::vec3 mid = 0.5f * (r[0] + r[1]);
    const glm::vec3 vm = 0.5f * (v0 + v1);
    const glm::vec3 topple = glm::cross(mid, vm) / za::max(glm::dot(mid, mid), throwMinArm * throwMinArm);
    const glm::vec3 overFeet = mid + glm::vec3{0.f, 0.f, (middle.z - m->v.absmin[2]) * toMetres};
    const float rise = za::max(overFeet.z, throwMinArm);
    const glm::vec3 vmLevel{vm.x, vm.y, 0.f}, feetLevel{overFeet.x, overFeet.y, 0.f}, dLevel{d.x, d.y, 0.f};
    const glm::vec3 wayTopple = (rise * vmLevel - vm.z * feetLevel) /
                                (za::max(glm::length(overFeet), throwMinArm) * za::max(glm::length(mid), throwMinArm));
    const glm::vec3 wayWheel = -(v1.z - v0.z) * dLevel / za::max(glm::dot(d, d), throwMinArm * throwMinArm);
    const glm::vec3 way = wayTopple + wayWheel;
    const float dt = static_cast<float>(za::clamp(host_frametime, 0.0, 0.1));
    hd.way = glm::mix(hd.way, way, 1.f - za::exp(-dt / throwWayTime));
    const float twist = glm::degrees(glm::length(way));
    if(twist > hd.peakTwist)
    {
        hd.peakTwist = twist;
        hd.peakSpeed = speed;
    }
    if(debug(2))
    {
        Con_Printf("foegrab: both hands on %s (%d): turn %.0f deg/s, hands %.2f m/s (off at %.2f %.2f %.2f m, moving %.2f "
                   "%.2f %.2f; main at %.2f %.2f %.2f, moving %.2f %.2f %.2f)\n",
            PR_GetString(m->v.classname), a.ent, twist, speed, r[0].x, r[0].y, r[0].z, v0.x, v0.y, v0.z, r[1].x, r[1].y,
            r[1].z, v1.x, v1.y, v1.z);
    }
    TwoHands t;
    t.player = player;
    t.m = m;
    t.middle = middle;
    t.vm = vm;
    t.speed = speed;
    t.way = way;
    t.twist = twist;
    t.spin = wheel.z + topple.z;
    if(neckCheck(c, m, *move) || liftCheck(c, t))
    {
        return;
    }
    throwCheck(c, t);
}

// The lift (vr_foegrab_lift; QC VR_FoeGrab_Lift): both hands pulling the enemy up, their mean velocity up at
// vr_foegrab_lift_speed or more and within 45 degrees of straight up, a humanoid (vr_foegrab_lift_classes: no ogre, fiend,
// shambler or vore) and the player within vr_foegrab_lift_reach of it (walked up close). It is knocked down (its ragdoll
// launched up at vr_foegrab_lift_up), held on by both hands (handOver at vr_foegrab_lift_strength of its weight a hand: carried, thrown
// down by letting go with a throw), flailing (box3d::ragdollFlail), and breaks free after vr_foegrab_lift_time (carryStep:
// breakFree, you shoved back). True: an upward pull, lifted or not (the throw isn't tried then).
bool liftCheck(int c, const TwoHands& t)
{
    Holder& hd = holders[c];
    const double time = qcvm->time;
    const float up = t.vm.z;
    if(vr_foegrab_lift.value == 0.f || up < za::max(vr_foegrab_lift_speed.value, 0.1f) || up < glm::length(glm::vec2{t.vm}))
    {
        return false;
    }
    if(time < hd.throwAgain)
    {
        return true;
    }
    hd.throwAgain = time + throwRetry;
    edict_t* player = t.player;
    edict_t* m = t.m;
    const bool dummy = isTrainingDummy(m);
    const char* name = dummy ? "the training dummy" : PR_GetString(m->v.classname);
    const bool infected = fieldFloatOr(m, fields().vr_mg3_infected, 0.f) != 0.f;
    const float off = glm::length(glm::vec2{vec(m->v.origin) - vec(player->v.origin)});
    const char* no = !dummy && !inList(vr_foegrab_lift_classes.string, PR_GetString(m->v.classname), infected)
                         ? "not a humanoid (vr_foegrab_lift_classes)"
                     : off > za::max(vr_foegrab_lift_reach.value, 0.f) ? "too far from you (vr_foegrab_lift_reach)"
                                                                      : nullptr;
    if(no)
    {
        if(debug() || dummy)
        {
            Con_Printf("foegrab: both hands pull %s (%d) up at %.2f m/s, %.0f units from you: no lift, %s\n", name,
                NUM_FOR_EDICT(m), up, off, no);
        }
        return true;
    }
    const float hold = za::clamp(vr_foegrab_lift_time.value, 0.1f, 60.f);
    const float result =
        callQc("VR_FoeGrab_Lift", player, m, za::clamp(vr_foegrab_lift_up.value, 0.f, 1000.f), hold + 1.f, 0.f);
    if(result >= 1.f)
    {
        (void)climb::grunt(player, vr_foegrab_throw_grunt_sound.value, vr_foegrab_throw_grunt.value);
    }
    if(debug() || dummy)
    {
        Con_Printf("foegrab: both hands lift %s (%d) at %.2f m/s up, %.0f units from you: %s\n", name, NUM_FOR_EDICT(m),
            up, off, result >= 2.f ? "would be lifted (it stands)" : result >= 1.f ? "lifted" : "it can't be knocked down now");
    }
    if(result != 1.f)
    {
        return true;
    }
    if(handOver(player, m, hd, vr_foegrab_lift_strength.value, liftHertz, Why::Lifted) == 0)
    {
        for(int h = 0; h < 2; h++)
        {
            release(player, h, Why::Lifted, "lifted, nothing to hold on to");
        }
        return true;
    }
    hd.breakAt = time + hold;
    const bool flails = box3d::ragdollFlail(m, true);
    if(debug())
    {
        Con_Printf("foegrab: %s (%d) lifted, %s, breaks free in %.1f s\n", name, NUM_FOR_EDICT(m),
            flails ? "flailing" : "not flailing (not driven alive)", hold);
    }
    return true;
}

// The two-hand throw (see above twoHandCheck): `t` measured.
void throwCheck(int c, const TwoHands& t)
{
    Holder& hd = holders[c];
    const double time = qcvm->time;
    edict_t* player = t.player;
    edict_t* m = t.m;
    const float twist = t.twist, speed = t.speed;
    const glm::vec3& way = t.way;
    if(vr_foegrab_throw.value == 0.f || time < hd.throwAgain || twist < vr_foegrab_throw_twist.value ||
        speed < vr_foegrab_throw_speed.value)
    {
        return;
    }
    hd.throwAgain = time + throwRetry;
    const glm::vec3 dir = glm::normalize(glm::dot(hd.way, way) > 0.f ? hd.way : way);
    const glm::vec3& middle = t.middle;
    glm::vec3 away = middle - vec(player->v.origin);
    away.z = 0.f;
    away = glm::length(away) > 1e-3f ? glm::normalize(away) : dir;
    const float along = glm::dot(dir, away);
    const char* wayName = along > throwAwayCos ? "away from you" : along < -throwAwayCos ? "towards you" :
                      glm::cross(away, dir).z > 0.f ? "to your left" : "to your right";
    const bool dummy = isTrainingDummy(m);
    const char* name = dummy ? "the training dummy" : PR_GetString(m->v.classname);
    if(along > throwAwayCos && vr_foegrab_throw_away.value == 0.f)
    {
        if(debug() || dummy)
        {
            Con_Printf("foegrab: both hands turn %s %.0f deg/s at %.2f m/s, away from you: no throw (a shove's: "
                       "vr_foegrab_throw_away)\n",
                name, twist, speed);
        }
        return;
    }
    const float yaw = glm::degrees(za::atan2(dir.y, dir.x));
    const float result = throwDown(player, m, dir, t.spin, debug());
    if(debug() || dummy)
    {
        Con_Printf("foegrab: both hands turn %s (%d) %.0f deg/s at %.2f m/s, %s (yaw %.0f): %s\n", name, NUM_FOR_EDICT(m),
            twist, speed, wayName, yaw, throwResult(result));
    }
    if(result >= 1.f)
    {
        // Held on (vr_foegrab_throw_hold): its ragdoll stays in the hands, softly (it falls, toppled, and the hands steer
        // and drag it); else let go.
        if(vr_foegrab_throw_hold.value == 0.f ||
            handOver(player, m, hd, vr_foegrab_throw_hold_strength.value, vr_foegrab_throw_hold_hertz.value, Why::Thrown) == 0)
        {
            for(int h = 0; h < 2; h++)
            {
                release(player, h, Why::Thrown, "thrown");
            }
        }
    }
}

// Where the palm held by `g` is now: on its spot as the body carries it (its drawn origin and yaw), moved by the
// animation's eased move of it (g.anim). With `dt` > 0 the ease steps on towards where the model as drawn has the spot
// now: a limb's swing is followed, a jump of a pose (or a triangle thrown about) glides, at most animMax from the body's.
bool spotNow(Hold& g, float dt, glm::vec3& palmAt)
{
    edict_t* m = EDICT_NUM(g.ent);
    glm::vec3 origin{0.f};
    glm::mat3 body{1.f};
    if(!bodyFrame(m, origin, body))
    {
        return false;
    }
    const glm::vec3 rigid = origin + body * g.local;
    glm::vec3 point{0.f};
    glm::mat3 axes{1.f};
    if(dt > 0.f && hitmodel::anchorFrame(m, g.tri, g.u, g.v, point, axes))
    {
        g.anim += (point - rigid - g.anim) * (1.f - za::exp(-dt / animTime));
        const float most = animMaxCm * cm();
        if(glm::length(g.anim) > most)
        {
            g.anim *= most / glm::length(g.anim);
        }
    }
    palmAt = rigid + g.anim + body * g.palmOff;
    return true;
}

// `m` moved flat towards `to` as a monster steps (the engine's SV_movestep: up a step, down onto the floor, not off a
// ledge), linked; blocked, half the way. False: it couldn't move at all.
bool moveFlat(edict_t* m, const glm::vec3& to)
{
    glm::vec3 d = to - vec(m->v.origin);
    d.z = 0.f;
    for(int tries = 0; tries < 2; tries++, d *= 0.5f)
    {
        vec3_t move;
        setVec(move, d);
        if(SV_movestep(m, move, true))
        {
            return true;
        }
    }
    if(debug(3))
    {
        Con_Printf("foegrab: %s (%d) can't move %.2f units\n", PR_GetString(m->v.classname), NUM_FOR_EDICT(m),
            2.f * glm::length(d));
    }
    return false;
}

[[nodiscard]] bool enabledFor(edict_t* ent, const VrMove* move)
{
    return vr_foegrab.value != 0.f && bindings().isVrProgs && hitmodel::enabled() && move &&
           (move->buttons & protocol::QVR_BUTTON_HANDSTRACKED) && static_cast<int>(ent->v.movetype) == MOVETYPE_WALK &&
           ent->v.health > 0.f;
}

// The walk test's step (before the holds slow it): the monster walked straight away from the first player.
void walkTestStep(double dt)
{
    WalkTest& w = walkTest;
    if(w.ent <= 0 || w.ent >= qcvm->num_edicts)
    {
        return;
    }
    edict_t* m = EDICT_NUM(w.ent);
    if(m->free || qcvm->time >= w.until)
    {
        if(!w.reported)
        {
            w.reported = true;
            const glm::vec3 d = m->free ? glm::vec3{0.f} : vec(m->v.origin) - w.from;
            Con_Printf("foegrab walk test: %s (%d) moved %.1f units flat (asked %.1f), hold %.2f\n",
                m->free ? "?" : PR_GetString(m->v.classname), w.ent, glm::length(glm::vec2{d}), w.speed * w.seconds,
                m->free ? 0.f : fieldFloatOr(m, fields().vr_foegrab_hold, 0.f));
            w.ent = 0;
        }
        return;
    }
    edict_t* player = EDICT_NUM(1);
    glm::vec3 away = vec(m->v.origin) - vec(player->v.origin);
    away.z = 0.f;
    if(glm::length(away) < 1e-3f)
    {
        return;
    }
    const glm::vec3 from = vec(m->v.origin);
    const glm::vec3 to = from + glm::normalize(away) * (w.speed * static_cast<float>(dt));
    (void)moveFlat(m, to);
}

// The live monster nearest the first player (null: none).
[[nodiscard]] edict_t* nearestMonster()
{
    edict_t* player = EDICT_NUM(1);
    edict_t* best = nullptr;
    float bestDist = 1e30f;
    for(int num = svs.maxclients + 1; num < qcvm->num_edicts; num++)
    {
        edict_t* m = EDICT_NUM(num);
        if(m->free || !(static_cast<int>(m->v.flags) & FL_MONSTER) || m->v.health <= 0.f)
        {
            continue;
        }
        const float d = glm::distance(vec(m->v.origin), vec(player->v.origin));
        if(d < bestDist)
        {
            bestDist = d;
            best = m;
        }
    }
    return best;
}

// vr_foegrab_hurt <share>: the enemy the first player's hands hold (else the live monster nearest him) is left with that
// share of its full health (its .max_health): for trying the two-hand throw on a hurt one (vr_foegrab_throw_hurt). The
// training dummy's health is vr_dummy_health's. For tests (Debug > Tests > Holding Enemies).
void hurt_f()
{
    if(!sv.active || svs.maxclients < 1 || Cmd_Argc() < 2)
    {
        Con_Printf("usage: vr_foegrab_hurt <share of its full health, 0.01..1>\n");
        return;
    }
    const VmScope vm;
    edict_t* m = nullptr;
    for(int h = 0; h < 2 && !m; h++)
    {
        const Hold& g = holders[0].holds[h];
        m = g.active && g.ent < qcvm->num_edicts && !EDICT_NUM(g.ent)->free ? EDICT_NUM(g.ent) : nullptr;
    }
    m = m ? m : nearestMonster();
    if(!m || isTrainingDummy(m))
    {
        Con_Printf("vr_foegrab_hurt: %s\n", m ? "the training dummy's health is vr_dummy_health's" : "no live monster");
        return;
    }
    const float share = za::clamp(static_cast<float>(Q_atof(Cmd_Argv(1))), 0.01f, 1.f);
    const float full = m->v.max_health > 0.f ? m->v.max_health : m->v.health;
    m->v.health = za::max(1.f, za::round(full * share));
    Con_Printf("vr_foegrab_hurt: %s (%d) at %.0f of %.0f health\n", PR_GetString(m->v.classname), NUM_FOR_EDICT(m),
        m->v.health, full);
}

// vr_foegrab_walk_test <speed> <seconds>: the live monster nearest the first player walks straight away from him at
// `speed` units/s for `seconds` (moved each server frame, before the holds slow it): how far it got is printed (held or
// not). For tests (Debug > Tests > Holding Enemies).
void walkTest_f()
{
    if(!sv.active || svs.maxclients < 1 || Cmd_Argc() < 3)
    {
        Con_Printf("usage: vr_foegrab_walk_test <speed units/s> <seconds>\n");
        return;
    }
    const VmScope vm;
    edict_t* best = nearestMonster();
    if(!best)
    {
        Con_Printf("vr_foegrab_walk_test: no live monster\n");
        return;
    }
    const float seconds = static_cast<float>(Q_atof(Cmd_Argv(2)));
    walkTest = WalkTest{NUM_FOR_EDICT(best), static_cast<float>(Q_atof(Cmd_Argv(1))), seconds, qcvm->time + seconds,
        vec(best->v.origin), false};
    Con_Printf("foegrab walk test: %s (%d) walks away at %.0f units/s for %.2f s\n", PR_GetString(best->v.classname),
        walkTest.ent, walkTest.speed, seconds);
}

// vr_foegrab_throw_test [way] [twist] [edict]: the live monster nearest the first player thrown as both his hands' turn would
// (its kind and health decide; no hands needed): way 0 to his left, 1 to his right, 2 towards him, 3 away; `twist` the
// hands' turn of it about the vertical (degrees/s, + to the left). Its fall is traced (ThrowTrace). For tests (Debug >
// Tests > Holding Enemies).
void throwTest_f()
{
    if(!sv.active || svs.maxclients < 1)
    {
        Con_Printf("usage: vr_foegrab_throw_test [way: 0 left, 1 right, 2 towards you, 3 away] [twist deg/s] (in a game)\n");
        return;
    }
    const VmScope vm;
    edict_t* player = EDICT_NUM(1);
    // (A third argument: that edict instead of the nearest, if it is a live monster: vr_ragdoll_act's.)
    const int only = Cmd_Argc() > 3 ? Q_atoi(Cmd_Argv(3)) : 0;
    edict_t* m = only > svs.maxclients && only < qcvm->num_edicts ? EDICT_NUM(only) : nearestMonster();
    if(m && (m->free || !(static_cast<int>(m->v.flags) & FL_MONSTER) || m->v.health <= 0.f))
    {
        m = nullptr;
    }
    if(!m)
    {
        Con_Printf("vr_foegrab_throw_test: no live monster\n");
        return;
    }
    glm::vec3 away = vec(m->v.origin) - vec(player->v.origin);
    away.z = 0.f;
    away = glm::length(away) > 1e-3f ? glm::normalize(away) : glm::vec3{1.f, 0.f, 0.f};
    const glm::vec3 left = glm::cross(glm::vec3{0.f, 0.f, 1.f}, away);
    const int way = Cmd_Argc() > 1 ? Q_atoi(Cmd_Argv(1)) : 0;
    const glm::vec3 dir = way == 1 ? -left : way == 2 ? -away : way == 3 ? away : left;
    const float twist = glm::radians(Cmd_Argc() > 2 ? static_cast<float>(Q_atof(Cmd_Argv(2))) : 0.f);
    const float result = throwDown(player, m, dir, twist, true);
    Con_Printf("vr_foegrab_throw_test: %s (%d): %s\n", PR_GetString(m->v.classname), NUM_FOR_EDICT(m), throwResult(result));
}

// vr_foegrab_status: each player's holds.
void status_f()
{
    if(!sv.active)
    {
        Con_Printf("vr_foegrab_status: no server\n");
        return;
    }
    const VmScope vm;
    int count = 0;
    for(int c = 0; c < za::min(svs.maxclients, static_cast<int>(MAX_SCOREBOARD)); c++)
    {
        for(int h = 0; h < 2; h++)
        {
            const Hold& g = holders[c].holds[h];
            if(!g.active)
            {
                continue;
            }
            edict_t* m = EDICT_NUM(g.ent);
            Con_Printf("foegrab status: player %d %s hand holds %s (%d): hand %.2f, held %.2f, stretch %.1f cm, %.2f s\n",
                c + 1, handName(h), PR_GetString(m->v.classname), g.ent, g.strength,
                fieldFloatOr(m, fields().vr_foegrab_hold, 0.f), g.stretch / cm(), qcvm->time - g.since);
            count++;
        }
    }
    if(count == 0)
    {
        Con_Printf("foegrab status: no hand holds an enemy\n");
    }
    for(int c = 0; c < za::min(svs.maxclients, static_cast<int>(MAX_SCOREBOARD)); c++)
    {
        if(holders[c].twoHanded)
        {
            Con_Printf("foegrab status: player %d holds it with both hands: its hardest turn %.0f deg/s, the hands at %.2f "
                       "m/s (throws at %.0f deg/s, %.2f m/s)\n",
                c + 1, holders[c].peakTwist, holders[c].peakSpeed, vr_foegrab_throw_twist.value,
                vr_foegrab_throw_speed.value);
        }
    }
    // An enemy held on as a ragdoll (handOver): its pelvis from the player (ahead, to his left, up).
    for(int c = 0; c < za::min(svs.maxclients, static_cast<int>(MAX_SCOREBOARD)); c++)
    {
        const Holder& hd = holders[c];
        Con_Printf("foegrab status: player %d at %.1f %.1f %.1f\n", c + 1, EDICT_NUM(c + 1)->v.origin[0],
            EDICT_NUM(c + 1)->v.origin[1], EDICT_NUM(c + 1)->v.origin[2]);
        edict_t* m = hd.carried > 0 && hd.carried < qcvm->num_edicts ? EDICT_NUM(hd.carried) : nullptr;
        glm::vec3 pelvis, head, feet;
        if(!m || m->free || !box3d::ragdollStance(hd.carried, pelvis, head, feet))
        {
            continue;
        }
        edict_t* player = EDICT_NUM(c + 1);
        const glm::vec3 ahead = forwardFlat(player);
        const glm::vec3 d = pelvis - vec(player->v.origin);
        Con_Printf("foegrab status: player %d holds %s (%d) on as a ragdoll (%s) with %d hand(s), %.2f s: its pelvis %.1f "
                   "ahead, %.1f to his left, %.1f up; the held limbs %.1f and %.1f units from the off and main hands\n",
            c + 1, PR_GetString(m->v.classname), hd.carried, hd.lifted ? "lifted" : hd.snapped ? "snapped" : "thrown", carryingHands(c, m),
            qcvm->time - hd.carriedSince, glm::dot(d, ahead), glm::dot(d, glm::cross(glm::vec3{0.f, 0.f, 1.f}, ahead)), d.z,
            box3d::ragdollHandReach(player, 0), box3d::ragdollHandReach(player, 1));
    }
    // The client's drawn hands (the stats as the view last put them on their spots).
    for(int h = 0; h < 2; h++)
    {
        if(pins[h].weight > 0.f)
        {
            Con_Printf("foegrab status: drawn %s hand %.2f on its spot (%.1f %.1f %.1f)\n", handName(h), pins[h].weight,
                pins[h].at.x, pins[h].at.y, pins[h].at.z);
        }
    }
}

} // namespace

bool qvr::foegrab::shoveTopple(edict_t* m, const glm::vec3& dir, float strength, bool ledge, float reach)
{
    glm::vec3 level{dir.x, dir.y, 0.f};
    if(!m || glm::length(level) < 1e-3f)
    {
        return false;
    }
    level = glm::normalize(level);
    const float k = za::clamp(strength, 0.f, 3.f);
    box3d::RagdollShove p;
    p.ledge = ledge;
    p.maxSpin = glm::radians(za::clamp(vr_knockdown_shove_max_spin.value, 10.f, 2000.f));
    p.angle = glm::radians(za::clamp(vr_knockdown_shove_topple_angle.value, 0.f, 180.f));
    if(ledge)
    {
        p.topple = glm::radians(za::clamp(vr_knockdown_shove_ledge_topple.value, 0.f, 2000.f)) * k;
        p.keep = za::clamp(vr_knockdown_shove_ledge_push.value, 0.f, 3.f);
        p.reach = za::max(reach, 0.f);
    }
    else
    {
        p.travel = za::max(reach, 0.f) * za::clamp(vr_knockdown_shove_travel.value, 0.f, 3.f);
        p.time = za::clamp(vr_knockdown_shove_topple_time.value, 0.05f, 3.f);
        p.lag = za::clamp(vr_knockdown_shove_feet_lag.value, 0.f, 1.f);
    }
    const bool driven = box3d::ragdollShove(m, level, p);
    if((vr_knockdown_debug.value != 0.f || debug()) && startTrace(m, level, "shove"))
    {
        Con_Printf("shove trace: %s (%d) %s%s towards yaw %.0f, strength %.2f, a standing one's shove %.0f units\n",
            PR_GetString(m->v.classname), throwTrace.ent, driven ? "driven" : "not driven (no ragdoll)",
            ledge ? " over a ledge" : "", glm::degrees(za::atan2(level.y, level.x)), k, reach);
    }
    return driven;
}

extern "C" void VR_FoeGrabPreThink(edict_t* ent)
{
    VR_ProfileBegin("foegrab");
    foegrab::preThink(ent);
    VR_ProfileEnd();
}

void qvr::foegrab::init()
{
    Cmd_AddCommand("vr_foegrab_status", status_f);
    Cmd_AddCommand("vr_foegrab_walk_test", walkTest_f);
    Cmd_AddCommand("vr_foegrab_hurt", hurt_f);
    Cmd_AddCommand("vr_foegrab_throw_test", throwTest_f);
    // vr_foegrab_throw_grunt_test: the throw's grunt as set, heard here (the menu's Hear Throw Grunt).
    Cmd_AddCommand("vr_foegrab_throw_grunt_test", [] { S_LocalSound(climb::gruntSample(vr_foegrab_throw_grunt_sound.value)); });
}

void qvr::foegrab::reset()
{
    for(Holder& h : holders)
    {
        h = Holder{};
    }
    numFoes = 0;
    walkTest = WalkTest{};
    throwTrace = ThrowTrace{};
    // A game saved while hands held: its monsters held and players holding no more.
    if(sv.active && qcvm == &sv.qcvm && bindings().isVrProgs)
    {
        for(int num = 1; num < qcvm->num_edicts; num++)
        {
            edict_t* e = EDICT_NUM(num);
            if(!e->free)
            {
                setFieldFloat(e, fields().vr_foegrab_hold, 0.f);
                setFieldFloat(e, fields().vr_foegrab_letgo, 0.f);
                setFieldFloat(e, fields().vr_foegrab_hands, 0.f);
            }
        }
    }
}

void qvr::foegrab::preThink(edict_t* ent)
{
    Holder* hp = holderOf(ent);
    if(!hp)
    {
        return;
    }
    Holder& hd = *hp;
    const double time = qcvm->time;
    if(hd.lastTime < 0.0 || za::abs(time - hd.lastTime) > 1.0) // a new map, a loaded game
    {
        hd = Holder{};
    }
    hd.lastTime = time;

    const VrMove* move = server::clientMove(ent);
    const bool enabled = enabledFor(ent, move);
    const int vrbits = fields().vrbits0;
    int bits = vrbits >= 0 ? static_cast<int>(fieldFloat(ent, vrbits)) : 0; // (the climb's holds already in)
    for(int h = 0; h < 2; h++)
    {
        const bool pressed = move && (move->vrBits0 & grabBit[h]);
        if(pressed && !hd.pressedLast[h])
        {
            hd.pressedAt[h] = time;
            if(debug() && (!enabled || !climb::handFree(ent, h)))
            {
                Con_Printf("foegrab: %s hand grips: %s\n", handName(h),
                    vr_foegrab.value == 0.f ? "off" : !hitmodel::enabled() ? "precise hits off (vr_hit_precise)" :
                    !(move->buttons & protocol::QVR_BUTTON_HANDSTRACKED) ? "hands not tracked" :
                    !enabled ? "the player can't (dead, noclip, other progs)" : "the hand isn't free");
            }
        }
        if(!pressed)
        {
            hd.pressedAt[h] = -1.0;
        }
        hd.pressedLast[h] = pressed;
        hd.ownedLast[h] = hd.owned[h];
        if(!pressed)
        {
            hd.owned[h] = false;
        }
        const bool climbing = (bits & climbingBit[h]) != 0;

        Hold& g = hd.holds[h];
        if(g.active)
        {
            if(!pressed)
            {
                release(ent, h, Why::Released, "the grip let go");
            }
            else if(!enabled)
            {
                release(ent, h, Why::Lost, "off (or the player can't)");
            }
            else if(climbing || !climb::handFree(ent, h, false))
            {
                release(ent, h, Why::Lost, "the hand holds something else");
            }
            else if(!canHold(EDICT_NUM(g.ent), g.modelindex))
            {
                release(ent, h, Why::Lost, "the enemy can't be held now");
            }
        }
        if(!enabled || !pressed || g.active || hd.owned[h] || climbing || hd.pressedAt[h] < 0.0 ||
            time - hd.pressedAt[h] > lateGrab || !climb::handFree(ent, h))
        {
            continue;
        }
        if(tryTake(ent, hd, h, *move))
        {
            hd.owned[h] = true;
        }
    }

    // The QC: the holding hands' grips hidden; which hands hold.
    if(vrbits >= 0)
    {
        int handBits = 0;
        for(int h = 0; h < 2; h++)
        {
            if(hd.owned[h])
            {
                bits &= ~grabBit[h];
            }
            if(hd.owned[h] || hd.ownedLast[h])
            {
                bits &= ~prevGrabBit[h];
            }
            handBits |= hd.holds[h].active ? (1 << h) : 0;
        }
        fieldFloat(ent, vrbits) = static_cast<float>(bits);
        setFieldFloat(ent, fields().vr_foegrab_hands, static_cast<float>(handBits)); // bit 1 the off hand, 2 the main
    }
}

void qvr::foegrab::serverFrame()
{
    if(!sv.active)
    {
        return;
    }
    VR_ProfileBegin("foegrab");
    const float dt = static_cast<float>(za::clamp(host_frametime, 0.0, 0.1));
    walkTestStep(dt);
    throwTraceStep();

    const int players = za::min(svs.maxclients, static_cast<int>(MAX_SCOREBOARD));
    // Holds that can't go on: the monster dead, knocked down, gone; one that broke free (the QC's .vr_foegrab_letgo).
    for(int c = 0; c < players; c++)
    {
        edict_t* player = EDICT_NUM(c + 1);
        for(int h = 0; h < 2; h++)
        {
            const Hold& g = holders[c].holds[h];
            if(!g.active)
            {
                continue;
            }
            edict_t* m = EDICT_NUM(g.ent);
            if(!canHold(m, g.modelindex))
            {
                release(player, h, Why::Lost, "the enemy can't be held now");
            }
            else if(fieldFloatOr(m, fields().vr_foegrab_letgo, 0.f) != 0.f)
            {
                release(player, h, Why::BrokeFree, "it broke free");
            }
        }
    }

    for(int c = 0; c < players; c++)
    {
        twoHandCheck(c);
        ragdollNeckCheck(c);
        carryStep(c);
    }

    for(int i = 0; i < numFoes;)
    {
        Foe& foe = foes[i];
        edict_t* m = foe.ent < qcvm->num_edicts ? EDICT_NUM(foe.ent) : nullptr;
        // Its hold: every hand's on it together.
        float free = 1.f;
        int hands = 0;
        for(int c = 0; c < players; c++)
        {
            for(int h = 0; h < 2; h++)
            {
                const Hold& g = holders[c].holds[h];
                if(g.active && g.ent == foe.ent)
                {
                    free *= 1.f - g.strength;
                    hands++;
                }
            }
        }
        if(m && !m->free)
        {
            setFieldFloat(m, fields().vr_foegrab_letgo, 0.f);
        }
        if(hands == 0 || !m || m->free)
        {
            if(m && !m->free)
            {
                setFieldFloat(m, fields().vr_foegrab_hold, 0.f);
            }
            foes[i] = foes[--numFoes];
            continue;
        }
        const float hold = 1.f - free;
        setFieldFloat(m, fields().vr_foegrab_hold, hold);
        if(isTrainingDummy(m))
        {
            // The training dummy stands on its spot (QC vr_dummy_think puts it back there): held, never moved.
            foe.fresh = false;
            foe.last = vec(m->v.origin);
            foe.lastYaw = m->v.angles[1];
            i++;
            continue;
        }

        const glm::vec3 cur = vec(m->v.origin);
        const glm::vec3 last = foe.fresh ? cur : foe.last;
        const float lastYaw = foe.fresh ? m->v.angles[1] : foe.lastYaw;
        foe.fresh = false;
        const glm::vec3 moved = cur - last;
        if(glm::length(glm::vec2{moved}) > teleportStep)
        {
            for(int c = 0; c < players; c++)
            {
                for(int h = 0; h < 2; h++)
                {
                    if(holders[c].holds[h].active && holders[c].holds[h].ent == foe.ent)
                    {
                        release(EDICT_NUM(c + 1), h, Why::Lost, "it was moved away");
                    }
                }
            }
            foe.last = cur;
            i++;
            continue;
        }
        // Its own movement since the last frame, flat, cut by its hold.
        const float keep = 1.f - za::clamp(vr_foegrab_slow.value, 0.f, 1.f) * hold;
        glm::vec3 target = last + glm::vec3{moved.x * keep, moved.y * keep, moved.z};
        // A player sliding from an enemy's shove (QC .vr_eshove_speed, a resisted one: vr_foegrab_shove 0) carries what he
        // holds along by the hold (the shove drags it with him rather than tearing it from his hand).
        glm::vec3 carry{0.f};
        int carriers = 0;
        for(int c = 0; c < players; c++)
        {
            edict_t* p = EDICT_NUM(c + 1);
            if(!holders[c].originKnown || fieldFloatOr(p, fields().vr_eshove_speed, 0.f) <= 0.f)
            {
                continue;
            }
            for(int h = 0; h < 2; h++)
            {
                if(holders[c].holds[h].active && holders[c].holds[h].ent == foe.ent)
                {
                    carry += vec(p->v.origin) - holders[c].lastOrigin;
                    carriers++;
                    break;
                }
            }
        }
        const float follow = za::clamp(vr_foegrab_follow.value, 0.f, 1.f);
        if(carriers > 0)
        {
            carry.z = 0.f;
            target += carry / static_cast<float>(carriers) * hold * (1.f - follow); // (the hands' follow below carries the rest)
        }
        // It follows its hands' own move since the last frame (flat; a step, a pull, the player walking off with it), by
        // vr_foegrab_follow times its hold: a grunt comes along, an ogre a little, a shambler hardly.
        glm::vec3 handMove{0.f};
        int moving = 0;
        for(int c = 0; c < players && follow > 0.f; c++)
        {
            const VrMove* move = server::clientMove(EDICT_NUM(c + 1));
            for(int h = 0; h < 2 && move; h++)
            {
                const Hold& g = holders[c].holds[h];
                if(!g.active || g.ent != foe.ent || qcvm->time - g.since < 1e-6)
                {
                    continue;
                }
                glm::vec3 d = palmOf(EDICT_NUM(c + 1), *move, h) - g.lastPalm;
                d.z = 0.f;
                if(glm::length(d) < teleportStep)
                {
                    handMove += d;
                    moving++;
                }
            }
        }
        if(moving > 0)
        {
            glm::vec3 step = handMove / static_cast<float>(moving) * (follow * hold);
            const float most = followMaxSpeed * dt;
            if(glm::length(step) > most)
            {
                step *= most / glm::length(step);
            }
            target += step;
        }
        // The hands pull their spots to them (flat): the mean of their stretches, eased by the hold.
        glm::vec3 pull{0.f};
        int pulling = 0;
        for(int c = 0; c < players; c++)
        {
            const VrMove* move = server::clientMove(EDICT_NUM(c + 1));
            for(int h = 0; h < 2 && move; h++)
            {
                const Hold& g = holders[c].holds[h];
                glm::vec3 at{0.f};
                if(!g.active || g.ent != foe.ent || !spotNow(holders[c].holds[h], 0.f, at))
                {
                    continue;
                }
                at += target - cur; // (where the slowing puts it)
                pull += palmOf(EDICT_NUM(c + 1), *move, h) - at;
                pulling++;
            }
        }
        if(pulling > 0)
        {
            pull /= static_cast<float>(pulling);
            pull.z = 0.f;
            glm::vec3 step = pull * (1.f - za::exp(-za::max(0.f, vr_foegrab_drag.value) * hold * dt));
            const float most = za::max(0.f, vr_foegrab_drag_speed.value) * dt;
            if(glm::length(step) > most)
            {
                step *= most / glm::length(step);
            }
            target += step;
        }
        if(glm::length(glm::vec2{target - cur}) > 0.01f)
        {
            (void)moveFlat(m, target);
        }
        if(debug(2))
        {
            Con_Printf("foegrab: %s (%d) hold %.2f: moved %.2f, kept %.2f, pulled %.2f units\n",
                PR_GetString(m->v.classname), foe.ent, hold, glm::length(glm::vec2{moved}),
                glm::length(glm::vec2{vec(m->v.origin) - last}), glm::length(pull));
        }
        // Its turning, cut as its movement (held, it can't turn round easily: the spot held stays nearer the hand).
        float turned = m->v.angles[1] - lastYaw;
        turned -= 360.f * za::round(turned / 360.f);
        if(za::abs(turned) < 90.f) // (more in a frame: set, not turned)
        {
            m->v.angles[1] = lastYaw + turned * keep;
        }
        foe.last = vec(m->v.origin);
        foe.lastYaw = m->v.angles[1];
        i++;
    }
    for(int c = 0; c < players; c++)
    {
        edict_t* player = EDICT_NUM(c + 1);
        holders[c].lastOrigin = vec(player->v.origin);
        holders[c].originKnown = true;
        const VrMove* move = server::clientMove(player);
        for(int h = 0; h < 2 && move; h++)
        {
            if(holders[c].holds[h].active)
            {
                holders[c].holds[h].lastPalm = palmOf(player, *move, h);
            }
        }
    }
    VR_ProfileEnd();
}

void qvr::foegrab::afterPoses()
{
    if(!sv.active)
    {
        return;
    }
    const float dt = static_cast<float>(za::clamp(host_frametime, 0.0, 0.1));
    const float breakAt = za::max(0.f, vr_foegrab_break.value) * cm();
    const int players = za::min(svs.maxclients, static_cast<int>(MAX_SCOREBOARD));
    for(int c = 0; c < players; c++)
    {
        edict_t* player = EDICT_NUM(c + 1);
        const VrMove* move = server::clientMove(player);
        for(int h = 0; h < 2; h++)
        {
            Hold& g = holders[c].holds[h];
            if(!g.active || !move)
            {
                continue;
            }
            glm::vec3 at{0.f};
            if(spotNow(g, dt, at))
            {
                g.palmAt = at;
            }
            g.stretch = glm::distance(palmOf(player, *move, h), g.palmAt);
            if(debug(2))
            {
                edict_t* m = EDICT_NUM(g.ent);
                const glm::vec3 p = palmOf(player, *move, h);
                Con_Printf("foegrab: %s hand on %s: stretch %.1f cm, its yaw %.0f, frame %.0f, origin (%.1f %.1f %.1f), "
                           "spot (%.1f %.1f %.1f), palm (%.1f %.1f %.1f)\n",
                    handName(h), PR_GetString(m->v.classname), g.stretch / cm(), m->v.angles[1], m->v.frame,
                    m->v.origin[0], m->v.origin[1], m->v.origin[2], g.palmAt.x, g.palmAt.y, g.palmAt.z, p.x, p.y, p.z);
            }
            // Past the break for a moment (a step up a stair, a lurch: the drag may bring it back), or far past it.
            g.overSince = g.stretch <= breakAt ? -1.0 : g.overSince < 0.0 ? qcvm->time : g.overSince;
            if(g.overSince >= 0.0 && (qcvm->time - g.overSince >= breakGrace || g.stretch > breakAt * breakFar))
            {
                release(player, h, Why::Pulled, "pulled too far from the spot");
            }
        }
    }
}

void qvr::foegrab::calcStats(edict_t* ent, int* statsi)
{
    using namespace protocol;
    const Holder* hd = holderOf(ent);
    int bits = 0;
    for(int h = 0; h < 2; h++)
    {
        const int first = h ? STAT_QVR_FOEGRABMAINX : STAT_QVR_FOEGRABOFFX;
        glm::vec3 at{0.f};
        if(hd && hd->holds[h].active)
        {
            bits |= 1 << h;
            at = hd->holds[h].palmAt;
        }
        for(int i = 0; i < 3; i++)
        {
            statsi[first + i] = static_cast<int>(za::lround(at[i] * statScale));
        }
    }
    statsi[STAT_QVR_FOEGRAB] = bits;
}

bool qvr::foegrab::holding(int hand)
{
    return hand >= 0 && hand <= 1 && (cl.stats[protocol::STAT_QVR_FOEGRAB] & (1 << hand)) != 0;
}

float qvr::foegrab::drawnHand(const hands::State& s, int hand, glm::vec3& pos, const glm::vec3& rot)
{
    using namespace protocol;
    if(hand < 0 || hand > 1)
    {
        return 0.f;
    }
    Pin& pin = pins[hand];
    const float dt = pin.last >= 0.0 ? static_cast<float>(CLAMP(0.0, vr_gametime - pin.last, 0.1)) : 0.f;
    pin.last = vr_gametime;
    if(holding(hand))
    {
        const int first = hand ? STAT_QVR_FOEGRABMAINX : STAT_QVR_FOEGRABOFFX;
        pin.at = glm::vec3{static_cast<float>(cl.stats[first]), static_cast<float>(cl.stats[first + 1]),
                     static_cast<float>(cl.stats[first + 2])} /
                 statScale;
        pin.weight = za::min(1.f, pin.weight + dt / pinInTime);
    }
    else
    {
        pin.weight = za::max(0.f, pin.weight - dt / pinOutTime);
    }
    if(pin.weight <= 0.f)
    {
        return 0.f;
    }
    const float w = pin.weight * pin.weight * (3.f - 2.f * pin.weight);
    const glm::vec3 palm = s.palmValid[hand] ? hands::redirect(s.palmLocal[hand], rot) : glm::vec3{0.f};
    pos = glm::mix(pos, pin.at - palm, w);
    return w;
}
