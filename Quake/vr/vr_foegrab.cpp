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
#include "vr_lines.hpp"
#include "vr_mem.hpp"
#include "vr_motion.hpp"
#include "vr_move.hpp"
#include "vr_profile.hpp"
#include "vr_progs.hpp"
#include "vr_protocol.hpp"
#include "vr_server.hpp"
#include "vr_text3d.hpp"
#include "vr_timescale.hpp"
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
constexpr float neckMinArm = 0.04f;   // m: hands nearer together count as this far apart (the twist's measure)
constexpr float headSurface = 3.5f;   // units: a head's surface lies up to this beyond its sphere (QC VR_HEAD_MARGIN)
constexpr float headJaw = 0.35f;      // the head zone's sphere counts down to this share of its radius under its middle (HeadZone)
constexpr float headWide = 0.9f;      // and no further across its axis than this share of its radius (shoulder pads beside it)
constexpr float neckWide = 0.75f;     // under it, its neck: no further across than this share of the head's radius
constexpr float palmSlack = 2.f;      // units: a ragdoll's zone grown for the palm (its middle, off the skin; ragdollNeckCheck)
constexpr float strokeSlack = 6.f;    // units: and more while a twist is under way (it sweeps the palms round)

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

// A sample of the hands on a head (neckWindowStep): when, the line between the palms (play space, metres), the head's
// reference frame then (world from its body: a standing one's yaw, a ragdoll's chest), the wrists' roll so far (rad).
struct NeckSample
{
    double t{0.0};
    glm::vec3 line{0.f};
    glm::mat3 frame{1.f};
    float roll{0.f};
};
constexpr int neckSamples = 128;        // a second at neckSampleStep
constexpr double neckSampleStep = 0.008; // s: samples at most this often

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
    bool flailing{false}; // its lift's flail begun (box3d::ragdollFlail): ended when let go of, even killed in the hands
    bool snapped{false};
    double breakAt{-1.0};
    // The neck snap (neckStep, neckCheck): the enemy both hands hold by the head (0: none), how far it has turned within
    // the window (rad: a third of Twist Angle or more, else 0: a turn under way), the hands' turns at the last frame
    // (play space: their wrists' roll), no snap before neckAgain.
    int neckEnt{0};
    float neckTurned{0.f};
    glm::mat3 neckRot[2]{glm::mat3{1.f}, glm::mat3{1.f}};
    double neckAgain{0.0};
    double neckLast{-1.0};
    // The hand line (neckStep): the last second's samples (a ring, the newest at neckHead - 1), the wrists' common roll
    // about the neck so far (rad), the largest turn within the window this hold (deg: the status's) and the last one.
    NeckSample neckRing[neckSamples];
    int neckHead{0};
    int neckCount{0};
    float neckRoll{0.f};
    float neckWinPeak{0.f};
    float neckWinLast{0.f};
    int neckWhere{0};    // where both standing holds were last (neckCheck's log: printed as it changes)
    int neckRagWhere{0}; // and both palms on a ragdoll (ragdollNeckCheck's)
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

// The hands' gestures (the two-hand lift and throw, the neck snap) are the player's, measured on his own clock, the same
// in slow motion as not (vr_timescale, bullet time): their times (the settle, the retry, the window, the break's grace)
// real seconds (times gameSeconds() in the game's time), their speeds real (the hands' velocities times
// timescale::handScale(): in slow motion they are given in the game's time, 1 over the scale faster; Sandevistan's are
// real). Before, a 0.3 slow motion took the settle 0.5 s of the player's, the snap's twist and the lift's pull read 3.3
// times as fast.
[[nodiscard]] double gameSeconds(double real)
{
    return real * static_cast<double>(timescale::current());
}

// This frame's real length (`dt` the game's).
[[nodiscard]] float realFrame(float dt)
{
    return dt / za::max(timescale::current(), 0.01f);
}

// A hand's velocity (the move's) in real metres a second.
[[nodiscard]] glm::vec3 realVel(const glm::vec3& v)
{
    return v * timescale::handScale();
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

// Manhandling (vr_foegrab_manhandle_stagger, _health; QC VR_FoeGrab_Manhandle): hand `h` of `player` just took hold of
// `m`, a humanoid (the lift's classes) staggered a moment ago or weak: knocked down where it stands, its ragdoll held on
// by that hand as a lifted one's (handOver at vr_foegrab_lift_strength, flailing; it breaks free after
// vr_foegrab_lift_time, carryStep). Not while the hands carry one already.
void manhandle(edict_t* player, Holder& hd, int h, edict_t* m)
{
    if((vr_foegrab_manhandle_stagger.value <= 0.f && vr_foegrab_manhandle_health.value <= 0.f) || hd.carried > 0 ||
        isTrainingDummy(m) ||
        !inList(vr_foegrab_lift_classes.string, PR_GetString(m->v.classname),
            fieldFloatOr(m, fields().vr_mg3_infected, 0.f) != 0.f))
    {
        return;
    }
    const float hold = za::clamp(vr_foegrab_lift_time.value, 0.1f, 60.f);
    const float result = callQc("VR_FoeGrab_Manhandle", player, m, static_cast<float>(h), hold + 1.f, 0.f);
    if(result < 1.f)
    {
        return;
    }
    const char* why = result >= 2.f ? "weak" : "staggered";
    if(handOver(player, m, hd, vr_foegrab_lift_strength.value, liftHertz, Why::Lifted) == 0)
    {
        release(player, h, Why::Lifted, "manhandled, nothing to hold on to");
        return;
    }
    hd.breakAt = qcvm->time + hold;
    hd.flailing = box3d::ragdollFlail(m, true);
    if(debug() || vr_knockdown_debug.value != 0.f)
    {
        Con_Printf("manhandle: %s (%d) taken by the %s hand %s: knocked down, held on as a ragdoll, breaks free in %.1f s\n",
            PR_GetString(m->v.classname), NUM_FOR_EDICT(m), handName(h), why, hold);
    }
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
    hd.flailing = false;
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
        if(m && !m->free && (hd.lifted || hd.flailing))
        {
            (void)box3d::ragdollFlail(m, false);
        }
        hd.carried = 0;
        hd.flailing = false;
        return;
    }
    (void)box3d::ragdollGrace(m, c + 1, carryGrace);
    if(hd.lifted && m->v.health <= 0.f)
    {
        hd.lifted = false; // (killed in the hands: a body held, no break free; its flail ended when let go of)
    }
    if(hd.lifted && hd.breakAt >= 0.0 && qcvm->time >= hd.breakAt)
    {
        breakFree(c, m);
    }
}

// A QC function of the monster taking a point (with the player as self): float f(entity m, vector at, float extra).
// Where a point is in a head zone (HeadZone::where).
enum class Zone : int
{
    Out = 0,      // nowhere near
    Shoulder = 1, // beside its neck, too far out (its shoulders, its shoulder pads)
    Low = 2,      // under its neck (its chest)
    Neck = 3,
    Head = 4,
};

[[nodiscard]] const char* zoneName(Zone z)
{
    switch(z)
    {
    case Zone::Head: return "head";
    case Zone::Neck: return "neck";
    case Zone::Shoulder: return "shoulder";
    case Zone::Low: return "low";
    default: return "out";
    }
}

// The hands' head zone (neckCheck, ragdollNeckCheck, the diagnostic): positional damage's head sphere (QC
// VR_FoeGrab_HeadZone: the headshots'), then a column as narrow as its neck under it, in its head's frame: `centre` the
// sphere's middle, `axis` its up (standing: the vertical, on its model standing; a ragdoll's: from its neck's joint to its
// head). A point at its head: no lower than headJaw of the radius under the middle, within the sphere grown by its surface
// (headSurface) and vr_foegrab_necksnap_margin, no further across the axis than headWide of the radius and the margin (an
// enforcer's or a hell knight's shoulder pads rise beside its head); lower, at its neck: down to vr_foegrab_necksnap_neck
// under the sphere, within neckWide of the radius and the margin across (a grunt's neck is 5 units across its axis, its
// shoulders 9-11: the sphere alone took them, the old zone).
struct HeadZone
{
    bool valid{false};
    glm::vec3 centre{0.f};
    glm::vec3 axis{0.f, 0.f, 1.f};
    float radius{0.f}; // the head sphere's
    float margin{0.f}; // vr_foegrab_necksnap_margin
    float neck{0.f};   // vr_foegrab_necksnap_neck

    [[nodiscard]] float sphere() const { return radius + headSurface + margin; }
    [[nodiscard]] float headAcross() const { return radius * headWide + margin; }
    [[nodiscard]] float jaw() const { return -radius * headJaw; } // (along the axis from the middle)
    [[nodiscard]] float neckAcross() const { return za::max(radius * neckWide + margin, 0.5f); }
    [[nodiscard]] float bottom() const { return -(radius + neck); }

    // Where `p` is, the zone grown by `slack` units every way.
    [[nodiscard]] Zone where(const glm::vec3& p, float slack = 0.f) const
    {
        if(!valid)
        {
            return Zone::Out;
        }
        const glm::vec3 d = p - centre;
        const float along = glm::dot(d, axis);
        const float across = glm::length(d - axis * along);
        if(along >= jaw())
        {
            if(glm::length(d) > sphere() + slack)
            {
                return Zone::Out;
            }
            return across <= headAcross() + slack ? Zone::Head : Zone::Shoulder;
        }
        if(along < bottom() - slack)
        {
            return across <= sphere() + slack ? Zone::Low : Zone::Out;
        }
        if(across <= neckAcross() + slack)
        {
            return Zone::Neck;
        }
        return across <= 2.f * sphere() + slack ? Zone::Shoulder : Zone::Out;
    }
};

[[nodiscard]] bool atHead(Zone z)
{
    return z == Zone::Head || z == Zone::Neck;
}

// `m`'s head zone's size (QC VR_FoeGrab_HeadZone: its head sphere, '<forward> <up> <radius>' from its origin standing);
// false: none.
bool headSphereOf(edict_t* m, glm::vec3& out)
{
    const func_t fn = findFunction("VR_FoeGrab_HeadZone");
    if(!fn)
    {
        return false;
    }
    const int oldSelf = pr_global_struct->self;
    pr_global_struct->time = qcvm->time;
    pr_global_struct->self = EDICT_TO_PROG(m);
    G_INT(OFS_PARM0) = EDICT_TO_PROG(m);
    PR_ExecuteProgram(fn);
    out = vec(G_VECTOR(OFS_RETURN));
    pr_global_struct->self = oldSelf;
    return out.z > 0.f;
}

// A standing `m`'s head zone: on its model standing (hitmodel's rest pose), at its origin, turned with its yaw.
[[nodiscard]] HeadZone headZoneOf(edict_t* m)
{
    HeadZone z;
    glm::vec3 s{0.f};
    if(!headSphereOf(m, s))
    {
        return z;
    }
    const float yaw = glm::radians(m->v.angles[1]);
    z.valid = true;
    z.centre = vec(m->v.origin) + glm::vec3{za::cos(yaw), za::sin(yaw), 0.f} * s.x + glm::vec3{0.f, 0.f, s.y};
    z.radius = s.z;
    z.margin = za::clamp(vr_foegrab_necksnap_margin.value, -8.f, 32.f);
    z.neck = za::clamp(vr_foegrab_necksnap_neck.value, 0.f, 32.f);
    return z;
}

// A ragdoll's head zone (box3d::ragdollNeck): round its head's middle, along its neck as it lies; `joint` its neck's.
[[nodiscard]] HeadZone ragdollZoneOf(edict_t* m, glm::vec3& joint)
{
    HeadZone z;
    glm::vec3 s{0.f}, head{0.f}, neck{0.f, 0.f, 1.f};
    if(!box3d::ragdollNeck(NUM_FOR_EDICT(m), joint, neck, head) || !headSphereOf(m, s))
    {
        return z;
    }
    z.valid = true;
    z.centre = head;
    z.axis = neck;
    z.radius = s.z;
    z.margin = za::clamp(vr_foegrab_necksnap_margin.value, 0.f, 32.f);
    z.neck = za::clamp(vr_foegrab_necksnap_neck.value, 0.f, 32.f);
    return z;
}

// `p` on `m`'s model as on its model standing (the head zone's frame; p itself without precise hits).
[[nodiscard]] glm::vec3 restOf(edict_t* m, const glm::vec3& p)
{
    glm::vec3 out = p;
    if(hitmodel::target(m))
    {
        (void)hitmodel::restPoint(m, p, out);
    }
    return out;
}

// The spot hold `g` holds (world): on its model as its body carries it, moved by the animation's eased move of it.
[[nodiscard]] glm::vec3 spotOf(const Hold& g)
{
    glm::vec3 origin{0.f};
    glm::mat3 body{1.f};
    (void)bodyFrame(EDICT_NUM(g.ent), origin, body);
    return origin + body * g.local + g.anim;
}

/// The neck snap (vr_foegrab_necksnap; QC VR_FoeGrab_NeckSnap): both hands of a player holding a humanoid by the head
// (neckCheck: the holds' spots in its head zone, standing; ragdollNeckCheck: its ragdoll's head, knocked down, staggering,
// lifted, thrown and held on) twist it round: its neck snaps, it dies on the spot, its body limp in the hands.
struct NeckTwist
{
    float turned{0.f}; // how far the hands have turned it one way within the window, degrees (the larger of the two)
    // Its two measures (the diagnostic's): the line between the palms' turn and the wrists' within the window (degrees).
    float line{0.f};
    float wrist{0.f};
};

// The neck snap's diagnostic (vr_foegrab_necksnap_debug; Neck Snap > Show Diagnostic): the first player's, each server
// frame (neckDiagFrame; drawn by neckDebugDraw on the listen server's client). Its enemy: the one both hands hold by the
// head (neckCheck, ragdollNeckCheck: `measured`), else the one a hand holds, else the humanoid nearest the hands.
struct NeckDiag
{
    int ent{0}; // 0: none
    bool ragdoll{false};
    bool measured{false};    // both hands at its head this frame: the twist measured (`twist`)
    double measuredAt{-1.0}; // the server's time of the last frame measured
    double since{-1.0};      // both hands at its head since (-1: not)
    HeadZone zone;
    glm::vec3 point[2]{glm::vec3{0.f}, glm::vec3{0.f}}; // each hand's place as judged (a hold's spot on its model
                                                        // standing; a ragdoll's: the palm), world
    glm::vec3 palm[2]{glm::vec3{0.f}, glm::vec3{0.f}};  // the tracked palms (world)
    Zone where[2]{Zone::Out, Zone::Out};
    bool holds[2]{false, false}; // each hand holds it
    NeckTwist twist;
    float best{0.f}; // the largest turn since both hands took its head, degrees
    // The last hold's outcome, shown a while after (the panel; printed: neckDiagEnd): snapped, or its largest turn.
    double outcomeAt{-1.0};
    bool snapped{false};
    float outcomeTurned{0.f};
};
NeckDiag neckDiag;

[[nodiscard]] bool neckDiagOn()
{
    return vr_foegrab_necksnap_debug.value != 0.f;
}

[[nodiscard]] float neckWindow()
{
    return za::clamp(vr_foegrab_necksnap_window.value, 0.1f, 1.f);
}

// Both hands of the first player at `m`'s head this frame, measured (`t`): the diagnostic's (with it on, or the log's).
void neckDiagMeasured(int c, edict_t* m, bool ragdoll, const HeadZone& z, const glm::vec3* point, const Zone* where,
    const NeckTwist& t)
{
    if(c != 0)
    {
        return;
    }
    NeckDiag& d = neckDiag;
    const double time = qcvm->time;
    const int num = NUM_FOR_EDICT(m);
    if(d.ent != num || d.since < 0.0 || time - d.measuredAt > 0.1)
    {
        d.since = time;
        d.best = 0.f;
    }
    d.ent = num;
    d.ragdoll = ragdoll;
    d.measured = true;
    d.measuredAt = time;
    d.zone = z;
    for(int h = 0; h < 2; h++)
    {
        d.point[h] = point[h];
        d.where[h] = where[h];
        d.holds[h] = true;
    }
    d.twist = t;
    d.best = za::max(d.best, t.turned);
}

// The first player's hands left `m`'s head (or snapped it): its largest turn printed (the diagnostic on, or
// vr_foegrab_debug), kept for the panel.
void neckDiagEnd(bool snapped)
{
    NeckDiag& d = neckDiag;
    if(d.since < 0.0)
    {
        return;
    }
    if(neckDiagOn() || debug())
    {
        edict_t* m = EDICT_NUM(d.ent);
        Con_Printf("necksnap diag: %s (%d) %s, Hand Line: both hands at its head %.2f s, its largest turn %.0f deg within "
                   "%.2f s (snaps at %.0f): %s\n",
            m->free ? "?" : PR_GetString(m->v.classname), d.ent, d.ragdoll ? "a ragdoll" : "standing",
            qcvm->time - d.since, d.best, neckWindow(), za::max(vr_foegrab_necksnap_angle.value, 1.f),
            snapped ? "snapped" : "no snap");
    }
    d.outcomeAt = qcvm->time;
    d.snapped = snapped;
    d.outcomeTurned = d.best;
    d.since = -1.0;
}

// Each frame both hands of player `c` hold `m` by the head, `neck` its neck's line (unit, world: up standing; a ragdoll's
// from its neck's joint to its head, as it lies), `frame` its head's reference frame (world from its body: a standing
// one's yaw, its own turn not counted; a ragdoll's none: it turns whole with the hands): the hands' twist of it, the hand
// line (the author's idea). Each frame the line between the palms `d` (world, metres) is put in the head's frame and laid
// across its neck; the turn of that line about the neck since each sample of the last vr_foegrab_necksnap_window seconds
// (the old samples turned with the stick's turn since: the play space's yaw isn't the hands'), the largest, is the
// twist; so is the wrists' common roll about the neck (each hand's turn since the last frame about the neck, the lesser
// of the two when they turn the same way, else none) added up over the same window. Turned vr_foegrab_necksnap_angle
// either way within the window: a snap (neckSnapDue). Slow shuffles never add up; a tilt (one hand up, one down) is not
// a turn about the neck.
NeckTwist neckStep(int c, edict_t* m, const VrMove& move, const glm::vec3& neck, const glm::mat3& frame)
{
    Holder& hd = holders[c];
    edict_t* player = EDICT_NUM(c + 1);
    const int ent = NUM_FOR_EDICT(m);
    const double time = qcvm->time;
    // (On the player's clock: real seconds; gameSeconds.)
    const float dt = realFrame(static_cast<float>(za::clamp(host_frametime, 0.0, 0.1)));
    const bool fresh = hd.neckEnt != ent || time - hd.neckLast > gameSeconds(0.1);
    const float toMetres = 1.f / units::metresToUnits();
    // (The controllers' own points: the drawn hands, held out of the level or a ragdoll's limbs, kept the line between
    // them short while the controllers moved on.)
    const glm::vec3 d = (trackedPalmOf(player, move, 1) - trackedPalmOf(player, move, 0)) * toMetres;
    const glm::vec3 n = glm::length(neck) > 1e-4f ? glm::normalize(neck) : glm::vec3{0.f, 0.f, 1.f};
    // (Each hand's turn relative to the player: its angles are the world's, the stick's turn (smooth or snap: the play
    // space's yaw, move.vrYaw) in them; taken out here, it would read as both wrists turning the same way about the
    // vertical. The turn since the last frame is measured in play space and put back in the world's axes.)
    const float playYaw[3]{0.f, move.vrYaw, 0.f};
    const glm::mat3 playTurn = held::axesFromAngles(playYaw, true);
    float turn[2]{0.f, 0.f}; // each wrist's turn about the neck, rad/s
    for(int h = 0; h < 2; h++)
    {
        const glm::mat3 rot = glm::transpose(playTurn) * held::axesFromAngles(&move.hands[h].rot[0], true);
        if(!fresh && dt > 0.f)
        {
            // (A small turn's axis times its angle: the skew part of the turn since the last frame.)
            const glm::mat3 dr = rot * glm::transpose(hd.neckRot[h]);
            const glm::vec3 w =
                playTurn * glm::vec3{dr[1][2] - dr[2][1], dr[2][0] - dr[0][2], dr[0][1] - dr[1][0]} * (0.5f / dt);
            turn[h] = glm::dot(w, n);
        }
        hd.neckRot[h] = rot;
    }
    const float roll = turn[0] * turn[1] > 0.f ? (za::abs(turn[0]) < za::abs(turn[1]) ? turn[0] : turn[1]) : 0.f;
    const float window = neckWindow();
    const double windowGame = gameSeconds(window); // (real seconds, the player's: in slow motion as long)
    if(fresh)
    {
        hd.neckCount = 0;
        hd.neckRoll = 0.f;
        hd.neckWinPeak = 0.f;
    }
    hd.neckRoll += roll * dt;
    const glm::mat3 toBody = glm::transpose(frame);
    const glm::vec3 nb = toBody * n;
    const auto across = [&](const glm::vec3& v) { return v - nb * glm::dot(nb, v); };
    const glm::vec3 now = across(toBody * d);
    float line = 0.f, wrist = 0.f;
    for(int i = 0; i < hd.neckCount; i++)
    {
        const NeckSample& s = hd.neckRing[(hd.neckHead - 1 - i + neckSamples) % neckSamples];
        if(time - s.t > windowGame)
        {
            break;
        }
        // (The old line as the play space is turned now, in the head's frame then.)
        const glm::vec3 then = across(glm::transpose(s.frame) * (playTurn * s.line));
        if(glm::length(then) > neckMinArm * 0.5f && glm::length(now) > neckMinArm * 0.5f)
        {
            const float a = za::atan2(glm::dot(nb, glm::cross(then, now)), glm::dot(then, now));
            line = za::abs(a) > za::abs(line) ? a : line;
        }
        const float w = hd.neckRoll - s.roll;
        wrist = za::abs(w) > za::abs(wrist) ? w : wrist;
    }
    if(hd.neckCount == 0 ||
        time - hd.neckRing[(hd.neckHead - 1 + neckSamples) % neckSamples].t >= gameSeconds(neckSampleStep))
    {
        NeckSample& s = hd.neckRing[hd.neckHead];
        s.t = time;
        s.line = glm::transpose(playTurn) * d;
        s.frame = frame;
        s.roll = hd.neckRoll;
        hd.neckHead = (hd.neckHead + 1) % neckSamples;
        hd.neckCount = za::min(hd.neckCount + 1, neckSamples);
    }
    hd.neckEnt = ent;
    hd.neckLast = time;
    NeckTwist t;
    t.turned = glm::degrees(za::max(za::abs(line), za::abs(wrist)));
    t.line = glm::degrees(line);
    t.wrist = glm::degrees(wrist);
    // (A turn under way keeps a ragdoll's hold at its head: ragdollNeckCheck's stroke.)
    hd.neckTurned = t.turned >= za::max(vr_foegrab_necksnap_angle.value, 1.f) / 3.f ? glm::radians(t.turned) : 0.f;
    hd.neckWinLast = glm::degrees(line);
    hd.neckWinPeak = za::max(hd.neckWinPeak, t.turned);
    if(debug(2))
    {
        Con_Printf("foegrab: both hands on %s (%d)'s head (hand line): turned %.0f deg within %.2f s (the line %.0f, "
                   "the wrists %.0f; the line %.2f m across the neck), largest %.0f\n",
            PR_GetString(m->v.classname), ent, t.turned, window, t.line, t.wrist, glm::length(now), hd.neckWinPeak);
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
    hd.neckAgain = time + gameSeconds(throwRetry);
    hd.neckTurned = 0.f;
    hd.neckCount = 0;
    const bool dummy = isTrainingDummy(m);
    const char* name = dummy ? "the training dummy" : PR_GetString(m->v.classname);
    const int num = NUM_FOR_EDICT(m);
    const bool infected = fieldFloatOr(m, fields().vr_mg3_infected, 0.f) != 0.f;
    if(!dummy && !inList(vr_foegrab_lift_classes.string, PR_GetString(m->v.classname), infected))
    {
        if(debug())
        {
            Con_Printf("foegrab: both hands twist %s (%d)'s head %.0f deg within %.2f s: no neck snap, not a humanoid "
                       "(vr_foegrab_lift_classes)\n",
                name, num, t.turned, neckWindow());
        }
        return;
    }
    const float result = callQc("VR_FoeGrab_NeckSnap", player, m, za::clamp(vr_foegrab_necksnap_volume.value, 0.f, 2.f),
        ragdoll ? 1.f : 0.f, 0.f);
    if(debug() || dummy)
    {
        Con_Printf("foegrab: both hands snap %s (%d)'s neck, twisted %.0f deg within %.2f s (%s): %s\n", name, num,
            t.turned, neckWindow(), ragdoll ? "a ragdoll" : "standing",
            result >= 3.f ? "killed, no ragdoll to hold" : result >= 2.f ? "the training dummy (it stands)" :
            result >= 1.f ? "killed" : "it can't die now");
    }
    if(c == 0 && result >= 1.f)
    {
        neckDiagEnd(true);
        // The motion recorder's "necksnap" event (a take being recorded or replayed; MOTIONS.md): the twist detector
        // that snapped it (the hand line; "velocity" in older takes), the turn.
        qvr::motion::qcEvent("necksnap", "handline", -1, t.turned, nullptr, m,
            va("%.0f deg within %.2f s %s", t.turned, neckWindow(), ragdoll ? "ragdoll" : "standing"));
    }
    if(result < 1.f || result == 2.f)
    {
        return;
    }
    // Its ragdoll's neck loose from now (vr_foegrab_necksnap_loose): its head flops on the broken vertebrae.
    (void)box3d::ragdollNeckSnapped(m);
    if(ragdoll)
    {
        (void)box3d::ragdollFlail(m, false); // (lifted: limp now)
        hd.flailing = false;
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

// Whether the hands' twist (neckStep) snaps the neck: turned vr_foegrab_necksnap_angle one way within the window, not
// just after a snap.
[[nodiscard]] bool neckSnapDue(const Holder& hd, const NeckTwist& t)
{
    return t.turned >= za::max(vr_foegrab_necksnap_angle.value, 1.f) && qcvm->time >= hd.neckAgain;
}

// Both hands' holds of player `c` on `m` (twoHandCheck): at its head or neck (HeadZone: each hold's spot as on its model
// standing; never its shoulders), the twist measured about its up (neckStep) and its neck snapped (neckSnapDue, neckSnap).
// True: both hands at its head, twisting or not: the neck snap is the only move then (the lift and the throw aren't
// tried; Neck Snap off, they are).
bool neckCheck(int c, edict_t* m, const VrMove& move)
{
    Holder& hd = holders[c];
    if(vr_foegrab_necksnap.value == 0.f)
    {
        return false;
    }
    const HeadZone z = headZoneOf(m);
    glm::vec3 spot[2];
    Zone where[2]{Zone::Out, Zone::Out};
    for(int h = 0; h < 2; h++)
    {
        spot[h] = restOf(m, spotOf(hd.holds[h]));
        where[h] = z.where(spot[h]);
    }
    // (Logged as it changes: where each hand holds, unless both at its head.)
    const int whereNow = 1 + static_cast<int>(where[0]) * 8 + static_cast<int>(where[1]);
    if(debug() && whereNow != hd.neckWhere && (!atHead(where[0]) || !atHead(where[1])))
    {
        // (The spots from its head's middle: forward, left, up.)
        glm::vec3 origin{0.f}, s[2];
        glm::mat3 body{1.f};
        (void)bodyFrame(m, origin, body);
        for(int h = 0; h < 2; h++)
        {
            s[h] = glm::transpose(body) * (spot[h] - z.centre);
        }
        Con_Printf("foegrab: both hands on %s (%d): the off hand at its %s, the main at its %s: no neck snap (the spots "
                   "%.1f %.1f %.1f and %.1f %.1f %.1f from its head's middle, radius %.1f)\n",
            PR_GetString(m->v.classname), NUM_FOR_EDICT(m), zoneName(where[0]), zoneName(where[1]), s[0].x, s[0].y,
            s[0].z, s[1].x, s[1].y, s[1].z, z.radius);
    }
    hd.neckWhere = whereNow;
    if(!atHead(where[0]) || !atHead(where[1]))
    {
        if(debug(2))
        {
            // (The spots from its head's middle: forward, left, up.)
            glm::vec3 origin{0.f}, s[2];
            glm::mat3 body{1.f};
            (void)bodyFrame(m, origin, body);
            for(int h = 0; h < 2; h++)
            {
                s[h] = glm::transpose(body) * (spot[h] - z.centre);
            }
            Con_Printf("foegrab: both hands on %s (%d), at its head: the off hand %s (%.1f %.1f %.1f), the main %s (%.1f "
                       "%.1f %.1f)\n",
                PR_GetString(m->v.classname), NUM_FOR_EDICT(m), zoneName(where[0]), s[0].x, s[0].y, s[0].z,
                zoneName(where[1]), s[1].x, s[1].y, s[1].z);
        }
        hd.neckCount = 0; // (a hand off its head: the hand line's window starts again)
        return false;
    }
    glm::vec3 origin{0.f};
    glm::mat3 body{1.f};
    (void)bodyFrame(m, origin, body);
    const NeckTwist t = neckStep(c, m, move, glm::vec3{0.f, 0.f, 1.f}, body);
    neckDiagMeasured(c, m, false, z, spot, where, t);
    if(neckSnapDue(hd, t))
    {
        neckSnap(c, m, t, false);
    }
    return true;
}

// Each frame, for player `c` without the foegrab's holds: both his hands holding one living monster's ragdoll (the QC's
// ragdoll holds: VR_Ragdoll_Handtouch's, a knocked-down or staggering one's; handOver's, lifted or thrown and held on) by
// its head or neck (where each hold grips it, box3d::ragdollHoldPoint, in its head zone as it lies, ragdollZoneOf, the
// limb held its head's or the one its head hangs from: never its shoulders or arms, wherever its flailing head swings;
// the palm grown by palmSlack without the hold's point): the twist measured about its neck as it lies (box3d::ragdollNeck; neckStep)
// and its neck snapped (neckSnapDue, neckSnap).
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
    glm::vec3 joint{0.f};
    const HeadZone z = ragdollZoneOf(m, joint);
    if(!z.valid)
    {
        return;
    }
    // (A stroke under way goes on with the zone grown by strokeSlack: the twist itself sweeps a palm held at its neck out
    // of the zone, the stroke cut at 27 of the 30 degrees.)
    const bool stroking = hd.neckEnt == num && hd.neckTurned > 0.f && qcvm->time - hd.neckLast <= gameSeconds(0.1);
    glm::vec3 palm[2];
    Zone where[2]{Zone::Out, Zone::Out};
    int part[2]{-1, -1};
    const uint32_t headBones = box3d::limbInfo(m, 1, 0);
    const int neckParent = static_cast<int>(box3d::limbInfo(m, 5, static_cast<int>(box3d::limbInfo(m, 3, 0))));
    for(int h = 0; h < 2; h++)
    {
        box3d::RagdollHold rh;
        if(box3d::ragdollHeld(player, h) != 1 || !box3d::ragdollHold(c + 1, h, rh) || rh.num != num)
        {
            hd.neckCount = 0; // (a hand off its head: the hand line's window starts again)
            hd.neckRagWhere = 0;
            return;
        }
        part[h] = rh.part;
        // (Where the hold grips it, not the palm: a lifted one flails, its head swinging past palms on its shoulders.)
        const bool anchored = box3d::ragdollHoldPoint(c + 1, h, palm[h]);
        if(!anchored)
        {
            palm[h] = trackedPalmOf(player, *move, h);
        }
        where[h] = z.where(palm[h], (anchored ? 0.f : palmSlack) + (stroking ? strokeSlack : 0.f));
        // (And the limb it holds its head's, or the one its head hangs from (at its neck): never an arm, a hold on its
        // shoulder; a lifted one's are its arms', their anchors swinging round its neck as it flails.)
        const bool headPart = part[h] >= 0 && part[h] < 32 && (headBones & (1u << part[h])) != 0u;
        if(atHead(where[h]) && !headPart && part[h] != neckParent)
        {
            where[h] = Zone::Shoulder;
        }
    }
    // (Logged as it changes: where each palm is, unless both at its head.)
    const int whereNow = 1 + static_cast<int>(where[0]) * 8 + static_cast<int>(where[1]);
    if(debug() && whereNow != hd.neckRagWhere && (!atHead(where[0]) || !atHead(where[1])))
    {
        const glm::vec3 d0 = palm[0] - z.centre, d1 = palm[1] - z.centre;
        Con_Printf("foegrab: both hands on %s (%d)'s ragdoll: the off hand at its %s, the main at its %s: no neck snap "
                   "(the holds %.1f and %.1f units along its neck from its head's middle, %.1f and %.1f across; radius "
                   "%.1f; parts %d and %d, its head %d)\n",
            PR_GetString(m->v.classname), num, zoneName(where[0]), zoneName(where[1]), glm::dot(d0, z.axis),
            glm::dot(d1, z.axis), glm::length(d0 - z.axis * glm::dot(d0, z.axis)),
            glm::length(d1 - z.axis * glm::dot(d1, z.axis)), z.radius, part[0], part[1],
            static_cast<int>(box3d::limbInfo(m, 3, 0)));
    }
    hd.neckRagWhere = whereNow;
    if(!atHead(where[0]) || !atHead(where[1]))
    {
        hd.neckCount = 0;
        return;
    }
    // (Not its chest's frame: a ragdoll held by the head turns whole with the hands' twist, its chest with it, the twist
    // read a third of what the hands did; the hands' own line, in play space, is the measure.)
    const NeckTwist t = neckStep(c, m, *move, z.axis, glm::mat3{1.f});
    neckDiagMeasured(c, m, true, z, palm, where, t);
    if(neckSnapDue(hd, t))
    {
        if(debug())
        {
            const glm::vec3 d0 = palm[0] - z.centre, d1 = palm[1] - z.centre;
            Con_Printf("foegrab: both hands on %s (%d)'s ragdoll at its %s and %s (the holds %.1f and %.1f along, %.1f and "
                       "%.1f across; parts %d and %d, its head %d)\n",
                PR_GetString(m->v.classname), num, zoneName(where[0]), zoneName(where[1]), glm::dot(d0, z.axis),
                glm::dot(d1, z.axis), glm::length(d0 - z.axis * glm::dot(d0, z.axis)),
                glm::length(d1 - z.axis * glm::dot(d1, z.axis)), part[0], part[1],
                static_cast<int>(box3d::limbInfo(m, 3, 0)));
        }
        neckSnap(c, m, t, true);
    }
}

/// Both hands of one player on one enemy, as twoHandCheck measures them each frame (the throw's, the lift's): metres, m/s.
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

// The hands of `player` on `m` measured (twoHandCheck; the diagnostics' grabDiagFrame, held or not): each palm from its
// box's middle (metres) and its velocity (m/s, the player's clock); the hands' turn of it (the wheel: one hand up, the
// other down; the topple: its top pulled over) and its way. `log`: printed (vr_foegrab_debug 2).
TwoHands twoHandsOf(edict_t* player, const VrMove& move, edict_t* m, bool log)
{
    const float toMetres = 1.f / units::metresToUnits();
    const glm::vec3 middle = (vec(m->v.absmin) + vec(m->v.absmax)) * 0.5f;
    glm::vec3 r[2];
    for(int h = 0; h < 2; h++)
    {
        r[h] = (palmOf(player, move, h) - middle) * toMetres;
    }
    const glm::vec3 v0 = realVel(move.hands[0].vel), v1 = realVel(move.hands[1].vel); // (the player's clock)
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
    TwoHands t;
    t.player = player;
    t.m = m;
    t.middle = middle;
    t.vm = vm;
    t.speed = speed;
    t.way = wayTopple + wayWheel;
    t.twist = glm::degrees(glm::length(t.way));
    t.spin = wheel.z + topple.z;
    if(log)
    {
        Con_Printf("foegrab: both hands on %s (%d): turn %.0f deg/s, hands %.2f m/s (off at %.2f %.2f %.2f m, moving %.2f "
                   "%.2f %.2f; main at %.2f %.2f %.2f, moving %.2f %.2f %.2f)\n",
            PR_GetString(m->v.classname), NUM_FOR_EDICT(m), t.twist, speed, r[0].x, r[0].y, r[0].z, v0.x, v0.y, v0.z,
            r[1].x, r[1].y, r[1].z, v1.x, v1.y, v1.z);
    }
    return t;
}

// A humanoid (vr_foegrab_lift_classes; the training dummy as one): the lift's and the neck snap's.
[[nodiscard]] bool liftable(edict_t* m)
{
    const bool infected = fieldFloatOr(m, fields().vr_mg3_infected, 0.f) != 0.f;
    return isTrainingDummy(m) || inList(vr_foegrab_lift_classes.string, PR_GetString(m->v.classname), infected);
}

[[nodiscard]] float liftSpeedNeed()
{
    return za::max(vr_foegrab_lift_speed.value, 0.1f);
}

[[nodiscard]] float liftReach()
{
    return za::max(vr_foegrab_lift_reach.value, 0.f);
}

// The lift's and the judo throw's diagnostics (vr_foegrab_lift_debug, vr_foegrab_throw_debug; Lift / Two-Hand Throw >
// Show Diagnostic): the first player's, each server frame. twoHandCheck, liftCheck and throwCheck note what they measured
// and what stops each move now (`liftNow`, `throwNow`: short) and each try's result (liftTry, throwTry: printed);
// grabDiagFrame fills in the frames they didn't measure (one hand or none on it, the grip settling: the enemy a hand
// holds, else the monster nearest the palms, as it would read) and prints a two-hand hold's best when it ends untried;
// grabDebugDraw draws them on the listen server's client.
struct GrabDiag
{
    int ent{0}; // 0: none
    bool measured{false};    // both hands on it this frame, measured (twoHandCheck)
    double measuredAt{-1.0}; // the server's time of the last frame measured
    double since{-1.0};      // both hands measured on it since (-1: not)
    bool holds[2]{false, false};
    glm::vec3 palm[2]{glm::vec3{0.f}, glm::vec3{0.f}}; // the tracked palms (world)
    TwoHands two;            // the hands' measures (way: the throw's smoothed one while measured)
    glm::vec3 origin{0.f};   // its origin, its box's bottom and top (world)
    float bottom{0.f};
    float top{0.f};
    glm::vec3 you{0.f};      // the player's origin
    float off{0.f};          // units from you to it, level
    bool humanoid{false};
    const char* liftNow{""}; // what stops the lift now
    const char* throwNow{""};
    float liftBest{0.f};     // the fastest pull up since both hands took it (m/s)
    float throwBest{0.f};    // the hardest turn since (deg/s), the hands' speed then (m/s)
    float throwBestSpeed{0.f};
    bool liftTried{false};   // a lift tried since (its result printed)
    bool throwTried{false};
    int tryEnt{0};           // the enemy last tried
    double liftAt{-1.0};     // the last try: when, its result (the panel's "last", a while)
    bool lifted{false};
    char liftLast[48]{};
    double throwAt{-1.0};
    bool thrown{false};
    char throwLast[48]{};
};
GrabDiag grabDiag;

[[nodiscard]] bool liftDiagOn()
{
    return vr_foegrab_lift_debug.value != 0.f;
}

[[nodiscard]] bool throwDiagOn()
{
    return vr_foegrab_throw_debug.value != 0.f;
}

// `m`'s place and yours, as the diagnostics show them.
void grabDiagPlace(GrabDiag& g, edict_t* player, edict_t* m)
{
    g.origin = vec(m->v.origin);
    g.bottom = m->v.absmin[2];
    g.top = m->v.absmax[2];
    g.you = vec(player->v.origin);
    g.off = glm::length(glm::vec2{g.origin - g.you});
    g.humanoid = liftable(m);
}

// Player `c`'s lift (throw) stopped now by `why` (the first player's: the diagnostic's).
void liftNote(int c, const char* why)
{
    if(c == 0)
    {
        grabDiag.liftNow = why;
    }
}

void throwNote(int c, const char* why)
{
    if(c == 0)
    {
        grabDiag.throwNow = why;
    }
}

// Both hands of the first player measured on `t.m` this frame (twoHandCheck), `way` the turn's smoothed way.
void grabDiagMeasured(int c, const TwoHands& t, const glm::vec3& way)
{
    if(c != 0)
    {
        return;
    }
    GrabDiag& g = grabDiag;
    const double time = qcvm->time;
    const int num = NUM_FOR_EDICT(t.m);
    if(g.ent != num || g.since < 0.0 || time - g.measuredAt > 0.1)
    {
        g.since = time;
        g.liftBest = g.throwBest = g.throwBestSpeed = 0.f;
        g.liftTried = g.throwTried = false;
    }
    g.ent = num;
    g.measured = true;
    g.measuredAt = time;
    g.holds[0] = g.holds[1] = true;
    g.two = t;
    g.two.way = way;
    grabDiagPlace(g, t.player, t.m);
    g.liftBest = za::max(g.liftBest, t.vm.z);
    if(t.twist > g.throwBest)
    {
        g.throwBest = t.twist;
        g.throwBestSpeed = t.speed;
    }
    g.liftNow = g.throwNow = "";
}

// The first player's lift tried on `m`: `ok` lifted, `last` the panel's result; printed with the diagnostic on.
void liftTry(int c, edict_t* m, bool ok, const char* last, float up, float off)
{
    if(c != 0)
    {
        return;
    }
    GrabDiag& g = grabDiag;
    g.liftTried = true;
    g.tryEnt = NUM_FOR_EDICT(m);
    g.liftAt = qcvm->time;
    g.lifted = ok;
    q_strlcpy(g.liftLast, last, sizeof(g.liftLast));
    if(liftDiagOn())
    {
        Con_Printf("lift diag: %s (%d): pulled up %.2f m/s (lifts at %.2f, within 45 deg), %.0f units from you (reach "
                   "%.0f): %s\n",
            PR_GetString(m->v.classname), NUM_FOR_EDICT(m), up, liftSpeedNeed(), off, liftReach(), last);
    }
}

// The first player's judo throw tried on `m`: `ok` thrown, `last` the panel's result; printed with the diagnostic on.
void throwTry(int c, edict_t* m, bool ok, const char* last, float twist, float speed, const char* wayName)
{
    if(c != 0)
    {
        return;
    }
    GrabDiag& g = grabDiag;
    g.throwTried = true;
    g.tryEnt = NUM_FOR_EDICT(m);
    g.throwAt = qcvm->time;
    g.thrown = ok;
    q_strlcpy(g.throwLast, last, sizeof(g.throwLast));
    if(throwDiagOn())
    {
        Con_Printf("throw diag: %s (%d): turned %.0f deg/s (throws at %.0f) at %.2f m/s (at %.2f), %s: %s\n",
            PR_GetString(m->v.classname), NUM_FOR_EDICT(m), twist, vr_foegrab_throw_twist.value, speed,
            vr_foegrab_throw_speed.value, wayName, last);
    }
}

// The first player's two-hand hold measured until the last frame ended: each move not tried, its best printed.
void grabDiagEnd()
{
    GrabDiag& g = grabDiag;
    if(g.since < 0.0)
    {
        return;
    }
    edict_t* m = EDICT_NUM(g.ent);
    const char* name = m->free ? "?" : PR_GetString(m->v.classname);
    const double held = qcvm->time - g.since;
    if(liftDiagOn() && !g.liftTried)
    {
        Con_Printf("lift diag: %s (%d): both hands on it %.2f s, its fastest pull up %.2f m/s (lifts at %.2f, within 45 "
                   "deg): no lift\n",
            name, g.ent, held, g.liftBest, liftSpeedNeed());
    }
    if(throwDiagOn() && !g.throwTried)
    {
        Con_Printf("throw diag: %s (%d): both hands on it %.2f s, its hardest turn %.0f deg/s at %.2f m/s (throws at %.0f "
                   "deg/s, %.2f m/s): no throw\n",
            name, g.ent, held, g.throwBest, g.throwBestSpeed, vr_foegrab_throw_twist.value, vr_foegrab_throw_speed.value);
    }
    g.since = -1.0;
}

bool liftCheck(int c, const TwoHands& t);
void throwCheck(int c, const TwoHands& t);

// Each frame, for player `c`: both his hands on one enemy measured (TwoHands), then its two-hand moves tried in turn: the
// neck snap (neckCheck: both hands at its head, the only move there), the lift (liftCheck: an upward pull is never a
// throw), the throw (throwCheck).
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
        hd.neckWhere = 0;
        return;
    }
    if(!hd.twoHanded)
    {
        hd.twoHanded = true;
        hd.peakTwist = hd.peakSpeed = 0.f;
        hd.way = glm::vec3{0.f};
    }
    const double time = qcvm->time;
    if(time - za::max(a.since, b.since) < gameSeconds(throwSettle))
    {
        return;
    }
    edict_t* m = EDICT_NUM(a.ent);
    const TwoHands t = twoHandsOf(player, *move, m, debug(2));
    const float dt = realFrame(static_cast<float>(za::clamp(host_frametime, 0.0, 0.1)));
    hd.way = glm::mix(hd.way, t.way, 1.f - za::exp(-dt / throwWayTime));
    if(t.twist > hd.peakTwist)
    {
        hd.peakTwist = t.twist;
        hd.peakSpeed = t.speed;
    }
    grabDiagMeasured(c, t, hd.way);
    if(neckCheck(c, m, *move))
    {
        liftNote(c, "at its head: neck snap's");
        throwNote(c, "at its head: neck snap's");
        return;
    }
    if(liftCheck(c, t))
    {
        throwNote(c, "pulling up: the lift's");
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
    if(vr_foegrab_lift.value == 0.f || up < liftSpeedNeed() || up < glm::length(glm::vec2{t.vm}))
    {
        liftNote(c, vr_foegrab_lift.value == 0.f ? "Lift off" : up < liftSpeedNeed() ? "pull up faster" :
                                                                                      "pull straighter up");
        return false;
    }
    if(time < hd.throwAgain)
    {
        liftNote(c, "just tried: wait");
        return true;
    }
    hd.throwAgain = time + gameSeconds(throwRetry);
    edict_t* player = t.player;
    edict_t* m = t.m;
    const bool dummy = isTrainingDummy(m);
    const char* name = dummy ? "the training dummy" : PR_GetString(m->v.classname);
    const float off = glm::length(glm::vec2{vec(m->v.origin) - vec(player->v.origin)});
    const bool humanoid = liftable(m);
    const char* no = !humanoid ? "not a humanoid (vr_foegrab_lift_classes)" :
                     off > liftReach() ? "too far from you (vr_foegrab_lift_reach)" : nullptr;
    if(no)
    {
        if(debug() || dummy)
        {
            Con_Printf("foegrab: both hands pull %s (%d) up at %.2f m/s, %.0f units from you: no lift, %s\n", name,
                NUM_FOR_EDICT(m), up, off, no);
        }
        liftNote(c, humanoid ? "too far: step closer" : "not a humanoid");
        liftTry(c, m, false, humanoid ? "no: too far" : "no: not a humanoid", up, off);
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
    liftNote(c, result >= 1.f ? "lifted" : "can't knock it down now");
    liftTry(c, m, result >= 1.f, result >= 2.f ? "dummy: would lift" : result >= 1.f ? "LIFTED" : "no: can't knock it down",
        up, off);
    if(result >= 1.f)
    {
        // The motion recorder's "lift" event (MOTIONS.md, "Lift and judo throw takes"): the hands' pull up; sub says
        // whether it was knocked down or the training dummy would have been (it stands).
        qvr::motion::qcEvent("lift", result >= 2.f ? "standing" : "knocked_down", -1, up, nullptr, m,
            va("%.2f m/s up, %.0f units from you", up, off));
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
    hd.flailing = flails;
    if(debug())
    {
        Con_Printf("foegrab: %s (%d) lifted, %s, breaks free in %.1f s\n", name, NUM_FOR_EDICT(m),
            flails ? "flailing" : "not flailing (not driven alive)", hold);
    }
    return true;
}

// Where a level way `dir` points, seen from `player` facing the enemy at `middle` (the judo throw's; its log's, the
// motion recorder's): away from you, towards you, to your left or right; `sub` the short word.
const char* throwWayName(const glm::vec3& dir, const glm::vec3& middle, const glm::vec3& you, const char** sub)
{
    glm::vec3 away = middle - you;
    away.z = 0.f;
    away = glm::length(away) > 1e-3f ? glm::normalize(away) : dir;
    const float along = glm::dot(dir, away);
    const bool left = glm::cross(away, dir).z > 0.f;
    if(sub)
    {
        *sub = along > throwAwayCos ? "away" : along < -throwAwayCos ? "towards" : left ? "left" : "right";
    }
    return along > throwAwayCos ? "away from you" : along < -throwAwayCos ? "towards you" :
           left                 ? "to your left" : "to your right";
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
        throwNote(c, vr_foegrab_throw.value == 0.f            ? "Two-Hand Throw off" :
                     time < hd.throwAgain                     ? "just tried: wait" :
                     twist < vr_foegrab_throw_twist.value      ? "turn it over faster" :
                                                                 "hands too slow");
        return;
    }
    hd.throwAgain = time + gameSeconds(throwRetry);
    const glm::vec3 dir = glm::normalize(glm::dot(hd.way, way) > 0.f ? hd.way : way);
    const glm::vec3& middle = t.middle;
    const char* sub = "";
    const char* wayName = throwWayName(dir, middle, vec(player->v.origin), &sub);
    const bool awayFromYou = sub[0] == 'a';
    const bool dummy = isTrainingDummy(m);
    const char* name = dummy ? "the training dummy" : PR_GetString(m->v.classname);
    if(awayFromYou && vr_foegrab_throw_away.value == 0.f)
    {
        if(debug() || dummy)
        {
            Con_Printf("foegrab: both hands turn %s %.0f deg/s at %.2f m/s, away from you: no throw (a shove's: "
                       "vr_foegrab_throw_away)\n",
                name, twist, speed);
        }
        throwNote(c, "away from you: a shove's");
        throwTry(c, m, false, "no: away (a shove's)", twist, speed, wayName);
        return;
    }
    const float yaw = glm::degrees(za::atan2(dir.y, dir.x));
    const float result = throwDown(player, m, dir, t.spin, debug());
    if(debug() || dummy)
    {
        Con_Printf("foegrab: both hands turn %s (%d) %.0f deg/s at %.2f m/s, %s (yaw %.0f): %s\n", name, NUM_FOR_EDICT(m),
            twist, speed, wayName, yaw, throwResult(result));
    }
    throwNote(c, result >= 1.f ? "thrown" : result >= 0.f ? "never thrown: its kind" : result >= -1.f ? "not hurt enough" :
                                                                                     "can't knock it down now");
    throwTry(c, m, result >= 1.f,
        result >= 2.f ? "dummy: would throw" : result >= 1.f ? va("THROWN %s", sub) : result >= 0.f ? "no: its kind" :
        result >= -1.f ? "no: not hurt enough" : "no: can't knock it down",
        twist, speed, wayName);
    if(result >= 1.f)
    {
        // The motion recorder's "judothrow" event: the two-hand throw's way (sub), the turn.
        qvr::motion::qcEvent("judothrow", sub, -1, twist, nullptr, m, va("%.0f deg/s at %.2f m/s, %s", twist, speed, wayName));
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

// vr_foegrab_necksnap_test: the live monster nearest the first player has its neck snapped as both his hands would
// (neckSnap: a humanoid's; standing, knocked down first; its ragdoll's neck loose, vr_foegrab_necksnap_loose), with no
// hands holding it. vr_foegrab_neck_angle <edict>: its ragdoll's head against its chest now (NECKANGLE: swing and twist,
// degrees; no edict: the last one vr_foegrab_necksnap_test snapped). For tests (Debug > Tests > Holding Enemies).
int neckTestNum = 0; // the edict vr_foegrab_necksnap_test snapped last
void neckSnapTest_f()
{
    if(!sv.active || svs.maxclients < 1)
    {
        Con_Printf("vr_foegrab_necksnap_test: in a game\n");
        return;
    }
    const VmScope vm;
    edict_t* m = nearestMonster();
    if(!m || m->v.health <= 0.f)
    {
        Con_Printf("vr_foegrab_necksnap_test: no live monster\n");
        return;
    }
    NeckTwist t;
    t.turned = za::max(vr_foegrab_necksnap_angle.value, 1.f);
    t.line = t.turned;
    const int num = NUM_FOR_EDICT(m);
    const bool ragdoll = box3d::isRagdoll(num);
    neckTestNum = num;
    neckSnap(0, m, t, ragdoll);
    Con_Printf("vr_foegrab_necksnap_test: %s (%d) %s, health %.0f, ragdoll %d\n", PR_GetString(m->v.classname), num,
        ragdoll ? "a ragdoll" : "standing", m->v.health, box3d::isRagdoll(num) ? 1 : 0);
}

void neckAngle_f()
{
    float swing = 0.f, twist = 0.f;
    const int num = Cmd_Argc() > 1 ? Q_atoi(Cmd_Argv(1)) : neckTestNum;
    if(!sv.active || !box3d::ragdollNeckAngle(num, swing, twist))
    {
        Con_Printf("NECKANGLE %d none\n", num);
        return;
    }
    Con_Printf("NECKANGLE %d swing %.1f twist %.1f\n", num, swing, twist);
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
        if(holders[c].neckEnt != 0)
        {
            Con_Printf("foegrab status: player %d neck snap (hand line): the line's last turn %.0f deg, the largest "
                       "within %.2f s %.0f deg (snaps at %.0f)\n",
                c + 1, holders[c].neckWinLast, neckWindow(), holders[c].neckWinPeak, vr_foegrab_necksnap_angle.value);
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

// Each server frame, after the checks: a measured hold's end (neckDiagEnd); with the diagnostic on and no hands measuring,
// its enemy (a hand's hold, the foegrab's or a ragdoll's; else the humanoid nearest the palms within neckDiagReach) and
// where each palm would hold it.
constexpr float neckDiagReach = 64.f; // units

void neckDiagFrame()
{
    NeckDiag& d = neckDiag;
    const double time = qcvm->time;
    d.measured = d.measuredAt == time;
    if(d.since >= 0.0 && !d.measured)
    {
        neckDiagEnd(false);
    }
    if(!neckDiagOn() || svs.maxclients < 1)
    {
        return;
    }
    edict_t* player = EDICT_NUM(1);
    const VrMove* move = server::clientMove(player);
    if(!move)
    {
        d.ent = 0;
        return;
    }
    for(int h = 0; h < 2; h++)
    {
        d.palm[h] = trackedPalmOf(player, *move, h);
    }
    if(d.measured)
    {
        return;
    }
    d.ent = 0;
    const Holder& hd = holders[0];
    int ent = 0;
    for(int h = 0; h < 2; h++)
    {
        d.holds[h] = false;
        ent = ent == 0 && hd.holds[h].active ? hd.holds[h].ent : ent;
    }
    const int ofs[2]{fields().offhand_held, fields().mainhand_held};
    for(int h = 0; h < 2 && ent == 0; h++)
    {
        const int held = ofs[h] >= 0 ? fieldInt(player, ofs[h]) : 0;
        edict_t* e = held != 0 ? PROG_TO_EDICT(held) : nullptr;
        ent = e && !e->free && (static_cast<int>(e->v.flags) & FL_MONSTER) && e->v.health > 0.f ? NUM_FOR_EDICT(e) : 0;
    }
    const glm::vec3 middle = 0.5f * (d.palm[0] + d.palm[1]);
    float nearest = neckDiagReach;
    for(int num = 1; num < qcvm->num_edicts && ent == 0; num++)
    {
        edict_t* e = EDICT_NUM(num);
        if(e->free || !(static_cast<int>(e->v.flags) & FL_MONSTER) || e->v.health <= 0.f)
        {
            continue;
        }
        const bool infected = fieldFloatOr(e, fields().vr_mg3_infected, 0.f) != 0.f;
        if(!isTrainingDummy(e) && !inList(vr_foegrab_lift_classes.string, PR_GetString(e->v.classname), infected))
        {
            continue;
        }
        const glm::vec3 box = glm::clamp(middle, vec(e->v.absmin), vec(e->v.absmax));
        if(glm::distance(box, middle) < nearest)
        {
            nearest = glm::distance(box, middle);
            d.ent = num;
        }
    }
    ent = ent != 0 ? ent : d.ent;
    if(ent == 0)
    {
        return;
    }
    edict_t* m = EDICT_NUM(ent);
    glm::vec3 joint{0.f};
    d.ent = ent;
    d.ragdoll = box3d::isRagdoll(ent);
    d.zone = d.ragdoll ? ragdollZoneOf(m, joint) : headZoneOf(m);
    d.twist = NeckTwist{};
    for(int h = 0; h < 2; h++)
    {
        const Hold& g = hd.holds[h];
        d.holds[h] = (g.active && g.ent == ent) || (ofs[h] >= 0 && fieldInt(player, ofs[h]) == EDICT_TO_PROG(m));
        box3d::RagdollHold rh;
        const bool ragHeld = d.ragdoll && box3d::ragdollHold(1, h, rh) && rh.num == ent;
        d.point[h] = d.palm[h];
        if(ragHeld)
        {
            (void)box3d::ragdollHoldPoint(1, h, d.point[h]);
        }
        else if(!d.ragdoll)
        {
            d.point[h] = restOf(m, g.active && g.ent == ent ? spotOf(g) : d.palm[h]);
        }
        d.where[h] = d.zone.where(d.point[h], d.ragdoll && !ragHeld ? palmSlack : 0.f);
    }
}

// Each server frame, after the checks: a measured two-hand hold's end (grabDiagEnd); with the lift's or the throw's
// diagnostic on and both hands not measuring, its enemy (a hand's hold; else the live monster nearest the palms within
// neckDiagReach, standing) as the hands would read on it now, and what stops each move.
void grabDiagFrame()
{
    GrabDiag& g = grabDiag;
    const double time = qcvm->time;
    g.measured = g.measuredAt == time;
    if(g.since >= 0.0 && !g.measured)
    {
        grabDiagEnd();
    }
    if((!liftDiagOn() && !throwDiagOn()) || svs.maxclients < 1)
    {
        return;
    }
    edict_t* player = EDICT_NUM(1);
    const VrMove* move = server::clientMove(player);
    if(!move)
    {
        g.ent = 0;
        return;
    }
    for(int h = 0; h < 2; h++)
    {
        g.palm[h] = trackedPalmOf(player, *move, h);
    }
    if(g.measured)
    {
        return;
    }
    const Holder& hd = holders[0];
    int ent = 0;
    for(int h = 0; h < 2 && ent == 0; h++)
    {
        ent = hd.holds[h].active ? hd.holds[h].ent : 0;
    }
    const glm::vec3 middle = 0.5f * (g.palm[0] + g.palm[1]);
    float nearest = neckDiagReach;
    for(int num = svs.maxclients + 1; num < qcvm->num_edicts && ent == 0; num++)
    {
        edict_t* e = EDICT_NUM(num);
        if(e->free || !(static_cast<int>(e->v.flags) & FL_MONSTER) || e->v.health <= 0.f || box3d::isRagdoll(num))
        {
            continue;
        }
        const glm::vec3 box = glm::clamp(middle, vec(e->v.absmin), vec(e->v.absmax));
        if(glm::distance(box, middle) < nearest)
        {
            nearest = glm::distance(box, middle);
            g.ent = num;
        }
    }
    g.ent = ent != 0 ? ent : nearest < neckDiagReach ? g.ent : 0;
    // (None: the one last tried, a while (lifted or thrown, a ragdoll now): its result stays in view.)
    const bool recent = time - za::max(g.liftAt, g.throwAt) < 4.0;
    if(g.ent == 0 && recent && g.tryEnt > 0 && g.tryEnt < qcvm->num_edicts && !EDICT_NUM(g.tryEnt)->free)
    {
        g.ent = g.tryEnt;
    }
    if(g.ent == 0)
    {
        return;
    }
    edict_t* m = EDICT_NUM(g.ent);
    int hands = 0;
    for(int h = 0; h < 2; h++)
    {
        g.holds[h] = hd.holds[h].active && hd.holds[h].ent == g.ent;
        hands += g.holds[h] ? 1 : 0;
    }
    g.two = twoHandsOf(player, *move, m, false);
    grabDiagPlace(g, player, m);
    const char* why = hands == 2 ? "settling the grip" : hands == 1 ? "one hand: use both" : "not held: grab it";
    g.liftNow = vr_foegrab_lift.value == 0.f ? "Lift off" : why;
    g.throwNow = vr_foegrab_throw.value == 0.f ? "Two-Hand Throw off" : why;
}

// A circle round `axis` (unit) at `centre`.
void diagCircle(const glm::vec3& centre, const glm::vec3& axis, float radius, const glm::vec4& colour, float width)
{
    const glm::vec3 u = glm::normalize(glm::abs(axis.z) < 0.9f ? glm::cross(axis, glm::vec3{0.f, 0.f, 1.f}) :
                                                                 glm::cross(axis, glm::vec3{1.f, 0.f, 0.f}));
    const glm::vec3 v = glm::cross(axis, u);
    constexpr int sides = 24;
    for(int i = 0; i < sides; i++)
    {
        const float a0 = glm::two_pi<float>() * static_cast<float>(i) / sides;
        const float a1 = glm::two_pi<float>() * static_cast<float>(i + 1) / sides;
        lines::line(centre + radius * (u * za::cos(a0) + v * za::sin(a0)),
            centre + radius * (u * za::cos(a1) + v * za::sin(a1)), width, colour, colour);
    }
}

// An arc round `axis` from `from` (unit, across the axis), `degrees` long.
void diagArc(const glm::vec3& centre, const glm::vec3& axis, const glm::vec3& from, float radius, float degrees,
    const glm::vec4& colour, float width)
{
    const glm::vec3 v = glm::cross(axis, from);
    const int steps = za::max(1, static_cast<int>(za::abs(degrees) / 10.f) + 1);
    for(int i = 0; i < steps; i++)
    {
        const float a0 = glm::radians(degrees) * static_cast<float>(i) / static_cast<float>(steps);
        const float a1 = glm::radians(degrees) * static_cast<float>(i + 1) / static_cast<float>(steps);
        lines::line(centre + radius * (from * za::cos(a0) + v * za::sin(a0)),
            centre + radius * (from * za::cos(a1) + v * za::sin(a1)), width, colour, colour);
    }
}

[[nodiscard]] glm::vec4 whereColour(Zone z)
{
    return atHead(z) ? glm::vec4{0.2f, 1.f, 0.3f, 1.f} : z == Zone::Shoulder ? glm::vec4{1.f, 0.6f, 0.1f, 1.f} :
                                                                            glm::vec4{1.f, 0.15f, 0.15f, 1.f};
}

// The diagnostics' panels (neckDebugDraw, grabDebugDraw): a value met (green) or not yet (orange); a bar's tick (its
// threshold, at the middle of its track).
[[nodiscard]] glm::vec4 diagColour(bool met)
{
    return met ? glm::vec4{0.2f, 1.f, 0.3f, 0.9f} : glm::vec4{1.f, 0.55f, 0.1f, 0.9f};
}
const glm::vec4 diagTick{1.f, 1.f, 1.f, 1.f};

// A diagnostic's panel: `text` beside `centre`, `side` units on the viewer's right, `up` higher (the hands and the enemy
// stay clear), turned to face the eye.
void diagPanel(const char* text, const glm::vec3& centre, float side, float up, const glm::vec3& eye,
    za::Span<const text3d::OverlayBar> bars)
{
    const glm::vec3 look = centre - eye;
    const glm::vec3 right = glm::length(glm::vec2{look}) > 0.1f ?
                                glm::normalize(glm::vec3{look.y, -look.x, 0.f}) : glm::vec3{0.f, -1.f, 0.f};
    const glm::vec3 at = centre + right * side + glm::vec3{0.f, 0.f, up};
    const glm::vec3 toPanel = at - eye;
    const float yaw = glm::degrees(za::atan2(toPanel.y, toPanel.x));
    text3d::queueOverlay(text, at, glm::vec3{0.f, yaw, 0.f}, 0.06f, bars);
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
    Cmd_AddCommand("vr_foegrab_necksnap_test", neckSnapTest_f);
    Cmd_AddCommand("vr_foegrab_neck_angle", neckAngle_f);
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
    neckDiag = NeckDiag{};
    grabDiag = GrabDiag{};
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
            manhandle(ent, hd, h, EDICT_NUM(hd.holds[h].ent)); // (handed over: the grip the QC's again)
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
    neckDiagFrame();
    grabDiagFrame();

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
            if(g.overSince >= 0.0 && (qcvm->time - g.overSince >= gameSeconds(breakGrace) || g.stretch > breakAt * breakFar))
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

bool qvr::foegrab::holds(const edict_t* player, const edict_t* m)
{
    const Holder* hd = holderOf(const_cast<edict_t*>(player));
    if(!hd || !m)
    {
        return false;
    }
    const int e = NUM_FOR_EDICT(const_cast<edict_t*>(m));
    return (hd->holds[0].active && hd->holds[0].ent == e) || (hd->holds[1].active && hd->holds[1].ent == e);
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

bool qvr::foegrab::headZoneCentre(edict_t* m, glm::vec3& centre)
{
    glm::vec3 joint{0.f};
    const HeadZone z = box3d::isRagdoll(NUM_FOR_EDICT(m)) ? ragdollZoneOf(m, joint) : headZoneOf(m);
    centre = z.centre;
    return z.valid;
}

void qvr::foegrab::neckDebugDraw(const glm::vec3& eye)
{
    const NeckDiag& d = neckDiag;
    if(!sv.active || !neckDiagOn() || d.ent <= 0 || d.ent >= sv.qcvm.num_edicts || !d.zone.valid)
    {
        return;
    }
    const HeadZone& z = d.zone;
    const bool both = atHead(d.where[0]) && atHead(d.where[1]);
    constexpr float width = 0.15f;
    const glm::vec4 zoneColour{both ? glm::vec4{0.2f, 1.f, 0.3f, 0.8f} : glm::vec4{0.3f, 0.8f, 1.f, 0.7f}};
    // The zone: the head (circles where it is cut: the jaw, its middle, its widest across), the neck's column under it.
    const glm::vec3 n = z.axis;
    const float headAt = za::min(za::sqrt(za::max(z.sphere() * z.sphere() - z.jaw() * z.jaw(), 0.f)), z.headAcross());
    diagCircle(z.centre + n * z.jaw(), n, headAt, zoneColour, width);
    diagCircle(z.centre, n, za::min(z.sphere(), z.headAcross()), zoneColour, width);
    const float topAlong = za::sqrt(za::max(z.sphere() * z.sphere() - z.headAcross() * z.headAcross(), 0.f));
    diagCircle(z.centre + n * topAlong, n, z.headAcross(), zoneColour, width);
    diagCircle(z.centre + n * z.jaw(), n, z.neckAcross(), zoneColour, width);
    diagCircle(z.centre + n * z.bottom(), n, z.neckAcross(), zoneColour, width);
    const glm::vec3 u = glm::normalize(glm::abs(n.z) < 0.9f ? glm::cross(n, glm::vec3{0.f, 0.f, 1.f}) :
                                                             glm::cross(n, glm::vec3{1.f, 0.f, 0.f}));
    const glm::vec3 v = glm::cross(n, u);
    for(const glm::vec3& side : {u, v, -u, -v})
    {
        lines::line(z.centre + n * z.bottom() + side * z.neckAcross(), z.centre + n * z.jaw() + side * z.neckAcross(),
            width, zoneColour, zoneColour);
        lines::line(z.centre + n * z.jaw() + side * headAt, z.centre + n * topAlong + side * z.headAcross(), width,
            zoneColour, zoneColour);
    }
    // Its neck's axis (magenta), through the top.
    const glm::vec4 axisColour{1.f, 0.3f, 1.f, 0.9f};
    lines::line(z.centre + n * z.bottom(), z.centre + n * (z.sphere() + 4.f), width, axisColour, axisColour);
    // The hands: where each holds (or would), from its palm.
    for(int h = 0; h < 2; h++)
    {
        const glm::vec4 c = whereColour(d.where[h]);
        lines::point(d.point[h], d.holds[h] ? 1.6f : 1.f, c);
        lines::line(d.palm[h], d.point[h], 0.08f, glm::vec4{1.f, 1.f, 1.f, 0.5f}, c);
    }
    // The line between the palms (yellow), and the twist's arc round the neck: Twist Angle (grey), the turn now (orange,
    // green once it is enough), from the line's way across the neck.
    const glm::vec4 lineColour{1.f, 0.95f, 0.2f, 0.9f};
    lines::line(d.palm[0], d.palm[1], width, lineColour, lineColour);
    const float need = za::max(vr_foegrab_necksnap_angle.value, 1.f);
    glm::vec3 from = d.palm[1] - d.palm[0];
    from -= n * glm::dot(from, n);
    from = glm::length(from) > 0.01f ? glm::normalize(from) : u;
    const float arcR = z.sphere() + 3.f;
    diagArc(z.centre, n, from, arcR, za::min(need, 360.f), glm::vec4{0.7f, 0.7f, 0.7f, 0.6f}, width);
    if(d.measured && d.twist.turned > 0.f)
    {
        diagArc(z.centre, n, from, arcR + 0.6f, -za::min(d.twist.turned, 360.f),
            d.twist.turned >= need ? glm::vec4{0.2f, 1.f, 0.3f, 1.f} : glm::vec4{1.f, 0.55f, 0.1f, 1.f}, 0.3f);
    }

    // The panel: the hand line's numbers against Twist Angle.
    const NeckTwist& t = d.twist;
    const double time = sv.qcvm.time;
    char text[512];
    char last[40] = "";
    if(d.outcomeAt >= 0.0 && time - d.outcomeAt < 4.0)
    {
        q_snprintf(last, sizeof(last), d.snapped ? "last: SNAPPED %.0f deg" : "last: no snap, %.0f of %.0f",
            d.outcomeTurned, need);
    }
    char title[40], turn[40], parts[40], hands[40], best[40];
    q_snprintf(title, sizeof(title), "NECK SNAP: Hand Line %.2f s", neckWindow());
    q_snprintf(turn, sizeof(turn), "turn %4.0f / %.0f deg", t.turned, need);
    q_snprintf(parts, sizeof(parts), "line %4.0f wrists %4.0f", t.line, t.wrist);
    q_snprintf(hands, sizeof(hands), "off %-8s main %-8s", zoneName(d.where[0]), zoneName(d.where[1]));
    q_snprintf(best, sizeof(best), "best %3.0f deg", d.best);
    q_snprintf(text, sizeof(text), "%-30s\n%-30s\n%-30s\n%-30s\n%-30s\n%-30s\n%-30s", title, turn, "", parts, hands, best,
        last);
    constexpr float track = 24.f;
    const text3d::OverlayBar bars[2]{
        {2, 3.f, track * za::min(t.turned / (2.f * need), 1.f), track, diagColour(t.turned >= need)},
        {2, 3.f + track * 0.5f, 0.15f, 0.15f, diagTick},
    };
    diagPanel(text, z.centre, z.sphere() + 9.f, 6.f, eye, za::Span<const text3d::OverlayBar>{bars, 2});
}

void qvr::foegrab::grabDebugDraw(const glm::vec3& eye)
{
    const GrabDiag& g = grabDiag;
    const bool liftOn = liftDiagOn(), throwOn = throwDiagOn();
    if(!sv.active || (!liftOn && !throwOn) || g.ent <= 0 || g.ent >= sv.qcvm.num_edicts)
    {
        return;
    }
    constexpr float width = 0.15f;
    constexpr float track = 24.f;
    const glm::vec3 up{0.f, 0.f, 1.f};
    const glm::vec4 grey{0.7f, 0.7f, 0.7f, 0.6f};
    const TwoHands& t = g.two;
    const double time = sv.qcvm.time;
    const glm::vec3 top{t.middle.x, t.middle.y, g.top};
    // The palms: green on it, grey not.
    for(int h = 0; h < 2; h++)
    {
        lines::point(g.palm[h], g.holds[h] ? 1.6f : 1.f, g.holds[h] ? diagColour(true) : grey);
    }
    // (Both panels: the lift's on the viewer's right, the throw's on the left.)
    constexpr float side = 20.f;
    if(liftOn)
    {
        // The hands' pull from between them (12 units a m/s): green once at Lift Speed (the cone's top) and within 45
        // degrees of straight up (inside the cone).
        const float need = liftSpeedNeed();
        const float lift = t.vm.z, level = glm::length(glm::vec2{t.vm});
        // (Still hands: no way, shown as level.)
        const float angle = glm::length(t.vm) < 0.05f ? 90.f : glm::degrees(za::atan2(level, lift));
        const bool fast = lift >= need, steep = lift >= level;
        constexpr float perMs = 12.f;
        const glm::vec3 from = 0.5f * (g.palm[0] + g.palm[1]);
        const glm::vec3 coneTop = from + up * (need * perMs);
        diagCircle(coneTop, up, need * perMs, grey, width);
        for(const glm::vec3& s : {glm::vec3{1.f, 0.f, 0.f}, glm::vec3{-1.f, 0.f, 0.f}, glm::vec3{0.f, 1.f, 0.f},
                glm::vec3{0.f, -1.f, 0.f}})
        {
            lines::line(from, coneTop + s * (need * perMs), width, grey, grey);
        }
        const glm::vec4 pull = diagColour(fast && steep);
        lines::line(from, from + t.vm * perMs, 0.3f, pull, pull);
        // Lift Reach round its feet: green with you inside.
        const float reach = liftReach();
        diagCircle(glm::vec3{g.origin.x, g.origin.y, g.bottom + 1.f}, up, za::max(reach, 1.f), diagColour(g.off <= reach),
            width);
        char text[640], title[40], speed[40], ang[40], dist[40], who[40], now[48], best[40], last[56] = "";
        q_snprintf(title, sizeof(title), "LIFT");
        q_snprintf(speed, sizeof(speed), "up %5.2f / %.2f m/s", lift, need);
        q_snprintf(ang, sizeof(ang), "angle %3.0f / 45 deg", angle);
        q_snprintf(dist, sizeof(dist), "reach %3.0f / %.0f units", g.off, reach);
        q_snprintf(who, sizeof(who), "hands %d, %s", (g.holds[0] ? 1 : 0) + (g.holds[1] ? 1 : 0),
            g.humanoid ? "a humanoid" : "not a humanoid");
        q_snprintf(now, sizeof(now), "now: %s", g.liftNow[0] ? g.liftNow : "-");
        q_snprintf(best, sizeof(best), "best up %.2f m/s", g.liftBest);
        if(g.liftAt >= 0.0 && time - g.liftAt < 4.0)
        {
            q_snprintf(last, sizeof(last), "last: %s", g.liftLast);
        }
        q_snprintf(text, sizeof(text), "%-30s\n%-30s\n%-30s\n%-30s\n%-30s\n%-30s\n%-30s\n%-30s\n%-30s\n%-30s\n%-30s", title,
            speed, "", ang, "", dist, "", who, now, best, last);
        const text3d::OverlayBar bars[6]{
            {2, 3.f, track * za::clamp(lift / (2.f * need), 0.f, 1.f), track, diagColour(fast)},
            {2, 3.f + track * 0.5f, 0.15f, 0.15f, diagTick},
            {4, 3.f, track * za::clamp(angle / 90.f, 0.f, 1.f), track, diagColour(steep)},
            {4, 3.f + track * 0.5f, 0.15f, 0.15f, diagTick},
            {6, 3.f, track * za::clamp(g.off / (2.f * za::max(reach, 1.f)), 0.f, 1.f), track, diagColour(g.off <= reach)},
            {6, 3.f + track * 0.5f, 0.15f, 0.15f, diagTick},
        };
        diagPanel(text, top, side, 0.f, eye, za::Span<const text3d::OverlayBar>{bars, 6});
    }
    if(throwOn)
    {
        // The turn's way from its top (0.08 units a deg/s: where its top would go), the circle Turn to Throw: green
        // once past it with the hands fast enough.
        const float need = vr_foegrab_throw_twist.value, speedNeed = vr_foegrab_throw_speed.value;
        constexpr float perDeg = 0.08f;
        const bool turned = t.twist >= need, fast = t.speed >= speedNeed;
        diagCircle(top, up, za::max(need, 1.f) * perDeg, grey, width);
        const glm::vec3 dir = glm::length(t.way) > 1e-4f ? glm::normalize(t.way) : glm::vec3{0.f};
        const glm::vec4 turn = diagColour(turned && fast);
        lines::line(top, top + dir * (t.twist * perDeg), 0.3f, turn, turn);
        const char* wayName = t.twist >= 10.f ? throwWayName(dir, t.middle, g.you, nullptr) : "-";
        char text[640], title[40], twist[40], speed[40], way[40], who[40], now[48], best[48], last[56] = "";
        q_snprintf(title, sizeof(title), "JUDO THROW");
        q_snprintf(twist, sizeof(twist), "turn %4.0f / %.0f deg/s", t.twist, need);
        q_snprintf(speed, sizeof(speed), "hands %4.2f / %.2f m/s", t.speed, speedNeed);
        q_snprintf(way, sizeof(way), "way: %s", wayName);
        q_snprintf(who, sizeof(who), "hands %d on it", (g.holds[0] ? 1 : 0) + (g.holds[1] ? 1 : 0));
        q_snprintf(now, sizeof(now), "now: %s", g.throwNow[0] ? g.throwNow : "-");
        q_snprintf(best, sizeof(best), "best %.0f deg/s at %.2f m/s", g.throwBest, g.throwBestSpeed);
        if(g.throwAt >= 0.0 && time - g.throwAt < 4.0)
        {
            q_snprintf(last, sizeof(last), "last: %s", g.throwLast);
        }
        q_snprintf(text, sizeof(text), "%-30s\n%-30s\n%-30s\n%-30s\n%-30s\n%-30s\n%-30s\n%-30s\n%-30s\n%-30s", title, twist,
            "", speed, "", way, who, now, best, last);
        const text3d::OverlayBar bars[4]{
            {2, 3.f, track * za::clamp(t.twist / (2.f * za::max(need, 1.f)), 0.f, 1.f), track, diagColour(turned)},
            {2, 3.f + track * 0.5f, 0.15f, 0.15f, diagTick},
            {4, 3.f, track * za::clamp(t.speed / (2.f * za::max(speedNeed, 0.01f)), 0.f, 1.f), track, diagColour(fast)},
            {4, 3.f + track * 0.5f, 0.15f, 0.15f, diagTick},
        };
        diagPanel(text, top, liftOn ? -side : side, 0.f, eye, za::Span<const text3d::OverlayBar>{bars, 4});
    }
}
