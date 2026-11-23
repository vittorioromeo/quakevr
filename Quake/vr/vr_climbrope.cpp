// vr_climbrope.cpp -- see vr_climbrope.hpp; docs/vr-port/ROPES.md.
//
// The server's ropes, by entity number: each a chain of points from its top (pinned at the entity's origin) to its
// bottom (free, or pinned at its target's origin or where it first hung), `pieces` pieces of length / pieces units each.
// A frame cut into steps of at most 1/120 s: each step's fall and pull back to the pieces' lengths (verlet::fall,
// holdLengths: the grappling hook rope's and the magnetic holsters' steps), then the frame's pushes (the bodies and the
// hands, as boxes and spheres the points are put out of, taking their speed), then the world (verlet::collide, through
// the server's world: the level, doors and lifts). Explosions and shots add speed to the points they reach (kicks, at
// the next step). Sent to every client where a point moved more than sentMoved since the last send (or every
// refreshTime at rest), unreliably; a rope gone, reliably.
//
// The client keeps each rope's last points and draws them as a coil::Cord (a plain cable for a rope, the magnetic
// holsters' chain of links for a chain), through a Catmull-Rom curve over the points.

#include "vr_climbrope.hpp"

#include "vr_avatar.hpp"
#include "vr_body.hpp"
#include "vr_box3d.hpp"
#include "vr_climb.hpp"
#include "vr_coil.hpp"
#include "vr_cvars.hpp"
#include "vr_fatigue.hpp"
#include "vr_geom.hpp"
#include "vr_hull.hpp"
#include "vr_gfx.hpp"
#include "vr_mem.hpp"
#include "vr_move.hpp"
#include "vr_nav.hpp"
#include "vr_physsound.hpp"
#include "vr_profile.hpp"
#include "vr_progs.hpp"
#include "vr_protocol.hpp"
#include "vr_server.hpp"
#include "vr_testkit.hpp"
#include "vr_trace.hpp"
#include "vr_units.hpp"
#include "vr_verlet.hpp"
#include "vr_view.hpp"

#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Base/Strcmp.hpp"
#include "Zancle/Base/Strstr.hpp"
#include "Zancle/Container/AnkerlUnorderedDense.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Acos.hpp"
#include "Zancle/Math/Ceil.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Fabs.hpp"
#include "Zancle/Math/Lround.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/Math/Sqrt.hpp"
#include "vr_api.h"

using namespace qvr;

namespace
{

constexpr int maxPieces = 64;
constexpr float sentMoved = 0.25f;     // units a point moves before the rope is sent again
constexpr double refreshTime = 0.5;    // seconds between two sends of a rope at rest (a lost datagram)
constexpr float longestStep = 1.f / 120.f;
constexpr int maxSteps = 8;            // a frame's steps at most (a long frame: longer steps)
constexpr float handRadius = 3.f;      // units: a hand's sphere pushing the rope
constexpr float shotReach = 2.f;       // units beyond the rope's thickness a shot pushes it from
constexpr float maxKick = 400.f;       // units/s a shot gives a point at most
constexpr float chainHitGrow = 1.f;    // (ropes8c) units beyond a chain's thickness its links stop a shot, a nail or a blow
constexpr float maxHandSpeed = 1000.f; // units/s a hand's speed counts at most (a teleport: none)
constexpr int flagChain = 1;           // QVR_SVC_CLIMBROPE's flags: drawn as a chain
constexpr int flagGone = 2;            // the rope is gone (no points follow)
constexpr int flagPole = 4;            // (ropes7) a climbing pole: drawn as a straight pole
constexpr int flagMetal = 8;           // ... of metal (else wood)
constexpr int flagMonkey = 16;         // (ropes7) traversed hanging under it (a monkey traverse): the legs hang
constexpr float poleThickness = 3.f;   // units across: a pole's (vr_pole) unless its rope_thickness
constexpr float poleThickest = 12.f;   // units across at most (a rope's: 8)
constexpr float polePiece = 32.f;      // units: a pole's points this far apart (it never bends)
constexpr float strungHeight = 80.f;   // units over the ghost the toolgun strings a rope from (overhead: a monkey traverse)
constexpr float monkeyEase = 60.f;     // units/s a monkey rider's weight moves along the rope to his hands' middle at most
constexpr float riderBelow = 24.f;     // units: a climber's weight this far below his highest hand (the swing's radius)
constexpr float maxRiderSpeed = 800.f; // units/s a rider's bob moves at most
constexpr float topClear = 4.f;        // units: no hold this close to the rope's top (its anchor)
constexpr float pickGrow = 2.f;        // units round a rope's surface the toolgun's aim meets it (as a box's grown 1)

// A rope's settings: its map keys (QC vr_rope, vr_chain: rope_*), or the toolgun's spawn panel's (vr_rope_spawn_*).
struct Params
{
    float length{256.f};   // units
    float thickness{1.5f}; // units across
    int pieces{16};
    float mass{5.f};       // kg, the whole rope
    float stiffness{1.f};  // 0..1: each pass's share of a stretched piece's correction
    float damping{0.5f};   // per second, of its speed lost to the air
    bool chain{false};
    bool pinned{false};    // the bottom held where it first hung (or at the target)
    // (ropes7) A climbing pole (vr_pole): straight from its top to its bottom (its length down to the floor, or to its
    // target or rope_end), every point held: it never moves (but with its entity), climbed as a pinned rope is.
    bool pole{false};
    bool metal{false};     // a pole's look, sounds and haptics: metal (else wood)
};

// The player climbing a rope (P2, ROPES.md "Climbing"): his weight hangs on it as a bob swinging round its top, the rope
// straight from the top through his hands (the pieces above his lowest hand laid on that line, pinned for the step); the
// rest hangs free below him. One rider a rope: another climber on it follows its points.
struct Rider
{
    int client{-1};            // -1: none
    int frame{-1};             // host_framecount it was last told of (climb::ride: each frame he holds it)
    float sHigh{0.f}, dHigh{0.f}; // the highest holding hand: along the rope from its top, and straight from it
    float sLow{0.f}, dLow{0.f};   // the lowest
    glm::vec3 bob{0.f}, bobPrev{0.f};
    float radius{0.f};         // the bob's distance from the top: dHigh + riderBelow
    bool started{false};
    glm::vec3 grabVel{0.f};    // the body's velocity as it took hold (the first frame's swing)
    glm::vec3 pump{0.f};       // the pump's push (units/s/s, level: P3)
    // (ropes7) A monkey traverse's rider (a rope pinned at both ends, slanted past vr_rope_monkey_angle): his weight a bob
    // at his hands, `sMid` along the rope (eased to their middle), the rope taut from its top through the bob to its
    // bottom; the bob sways round the line between its ends (the rope pulls, never pushes).
    bool monkey{false};
    float sMid{0.f};
    float stretch{1.f}; // its length as it lay when taken over its rest length (taken where it hangs: no jump up)
};

struct Rope
{
    Params params;
    glm::vec3 top{0.f};
    glm::vec3 bottom{0.f};
    za::Vector<glm::vec3> p, prev, from, sent, kick;
    za::Vector<float> rest;
    za::Vector<char> pinned;
    double sentAt{-1.0};
    float lastStep{0.f}; // the last step's length (seconds; 0: none yet)
    float speed{0.f};    // the fastest point's speed over the last frame (units/s)
    float jostle{0.f};   // (ropes8b) how hard its links knock (units/s; step): against their neighbours, climbed, kicked
    float handsWas[2]{-1.f, -1.f}; // (ropes8b) its rider's highest and lowest hands along it last frame (-1: none)
    bool monkey{false};  // (ropes7) traversed hanging under it: pinned at both ends, slanted past vr_rope_monkey_angle
    Rider rider;
    za::Vector<int> solids;  // (ropes8a) a pole's boxes (QC vr_pole_solid: placeSolids)
    int solidOwner{0};       // the player let through them (passSolids), 0 none
    za::U64 solidPass{0};    // (ropes8d) every player let through them (bit client - 1: VR_PoleSolidPasses), the owner too
    za::U64 solidIn{0};      // (ropes8e) ... of them, those whose box is in one of them (vr_rope_info: passing it, not near)
    glm::vec3 solidsAt{0.f}; // the pole's top where they were placed
};

ankerl::unordered_dense::map<int, Rope> ropes;

// The players' hands as last seen (their speed: the move since), [client][0 off, 1 main].
struct HandSeen
{
    glm::vec3 at{0.f};
    bool valid{false};
};
HandSeen handsSeen[MAX_SCOREBOARD][2];

// This frame's pushing hands (a sphere each, with its speed).
struct Pusher
{
    glm::vec3 at{0.f};
    glm::vec3 vel{0.f};
};

struct RopeScratch
{
    za::Vector<Pusher> hands;
    za::Vector<glm::vec3> smooth; // the client's curve through a rope's points
    za::Vector<gfx::TubeRing> rings;
    auto members() { return qvr::mem::list(hands, smooth, rings); }
};
mem::Scratch<RopeScratch> scratch{"climbrope"};

// The client's ropes.
struct ClientRope
{
    za::Vector<glm::vec3> p;
    za::Vector<glm::vec3> prev;  // the message's before (P3: drawn between the two, as the entities are: shownOf)
    za::Vector<glm::vec3> shown; // the points as drawn this frame
    double msgTime{-1.0};        // the server's time of the message that last sent it (cl.mtime[0] then)
    double shownTime{-1.0};      // the client time it was made for (-1: to make again)
    float thickness{1.5f};
    bool chain{false};
    bool pole{false}, metal{false}; // (ropes7) a climbing pole, of metal or wood
    bool monkey{false};             // (ropes7) traversed hanging under it: the legs hang (vr_avatar_climb.cpp ropeFoot)
    coil::Cord cord;
    gfx::TubeBatch batch;
    int sides{0};
};
ankerl::unordered_dense::map<int, ClientRope> clientRopes;
int builtFrame = -1;
bool boxHullReady = false; // (ropes8d) world.c's box hull made on a remote client (clientSolidTrace)
const glm::vec3 keyLight = glm::normalize(glm::vec3{0.3f, 0.2f, 1.f}); // (the magnetic holsters' chains')

[[nodiscard]] glm::vec3 vecOf(const float* v)
{
    return glm::vec3{v[0], v[1], v[2]};
}

// ---------------------------------------------------------------------------------------------------------------------
// The server's ropes.

// ---------------------------------------------------------------------------------------------------------------------
// (ropes8a) A pole's solid: boxes along it (QC vr_pole_solid: SOLID_BBOX, takedamage, never dealt any), each the bounds
// of its piece of the pole grown by half its thickness, as many as keep a slanted pole's boxes within its thickness of
// it (at most maxSolids). Bodies meet them (players, monsters), shots, nails and blows hit them (sparks off metal,
// splinters off wood: QC VR_MaterialOf), the navmesh cuts them out (vr_nav.cpp). The climber on it passes through: their
// owner (passSolids). "goalentity" its pole (a save's are found again and made afresh), "rope_style" 1 metal.

constexpr int maxSolids = 32;
constexpr const char* solidClass = "vr_pole_solid";
constexpr float solidNear = 24.f;   // units round a pole a climber holding a rope is let through it from (he takes it)
constexpr float solidInside = 0.5f; // units a body must be in its box to be let out of it (stuck: made over him)

[[nodiscard]] bool isSolidOf(const edict_t* s, int pole)
{
    return !s->free && !ZA_STRCMP(PR_GetString(s->v.classname), solidClass) && s->v.goalentity == EDICT_TO_PROG(EDICT_NUM(pole));
}

// Every box of pole `pole` freed (its own, and any a save brought back).
void freeSolids(Rope& r, int pole)
{
    for(int i = svs.maxclients + 1; i < sv.qcvm.num_edicts; i++)
    {
        if(edict_t* s = EDICT_NUM(i); isSolidOf(s, pole))
        {
            ED_Free(s);
        }
    }
    r.solids.clear();
    r.solidOwner = 0;
    r.solidPass = 0;
    r.solidIn = 0;
}

// (ropes8d) How many boxes a pole from `a` to `a` + `d` of `thickness` has (placeSolids; the clients' prediction makes
// the same from the pole as sent: solidTrace).
[[nodiscard]] int solidCount(const glm::vec3& d, float thickness)
{
    const float len = glm::length(d);
    const glm::vec3 u = len > 1e-3f ? glm::abs(d / len) : glm::vec3{0.f, 0.f, 1.f};
    const float across = u.x >= u.y && u.x >= u.z ? za::max(u.y, u.z) : u.y >= u.z ? za::max(u.x, u.z) : za::max(u.x, u.y);
    return za::clamp(static_cast<int>(za::ceil(len * across / za::max(thickness, 1.f))), 1, maxSolids);
}

// ... and its box `k` of `n`: the bounds of its piece grown by half its thickness.
void solidBounds(const glm::vec3& a, const glm::vec3& d, float thickness, int k, int n, glm::vec3& lo, glm::vec3& hi)
{
    const glm::vec3 p0 = a + d * (static_cast<float>(k) / static_cast<float>(n));
    const glm::vec3 p1 = a + d * (static_cast<float>(k + 1) / static_cast<float>(n));
    lo = glm::min(p0, p1) - glm::vec3{thickness * 0.5f};
    hi = glm::max(p0, p1) + glm::vec3{thickness * 0.5f};
}

// Pole `pole`'s boxes placed along it (made, or made again where one went).
void placeSolids(Rope& r, int pole)
{
    if(r.p.size() < 2)
    {
        return;
    }
    const glm::vec3 a = r.p.front(), d = r.p.back() - a;
    const int n = solidCount(d, r.params.thickness);
    bool whole = static_cast<int>(r.solids.size()) == n;
    for(const int s : r.solids)
    {
        whole = whole && s > 0 && s < sv.qcvm.num_edicts && isSolidOf(EDICT_NUM(s), pole);
    }
    if(whole && a == r.solidsAt)
    {
        return; // (where they were)
    }
    if(!whole)
    {
        freeSolids(r, pole);
        for(int k = 0; k < n; k++)
        {
            edict_t* s = box3d::spawnClass(solidClass, a, glm::vec3{0.f});
            if(!s)
            {
                break;
            }
            s->v.goalentity = EDICT_TO_PROG(EDICT_NUM(pole));
            if(const progs::FieldOffsets& f = progs::fields(); f.rope_style >= 0)
            {
                progs::fieldFloat(s, f.rope_style) = r.params.metal ? 1.f : 0.f;
            }
            r.solids.pushBack(NUM_FOR_EDICT(s));
        }
    }
    const int made = static_cast<int>(r.solids.size());
    for(int k = 0; k < made; k++)
    {
        glm::vec3 lo, hi;
        solidBounds(a, d, r.params.thickness, k, made, lo, hi);
        const glm::vec3 mid = (lo + hi) * 0.5f;
        edict_t* s = EDICT_NUM(r.solids[static_cast<za::SizeT>(k)]);
        for(int c = 0; c < 3; c++)
        {
            s->v.origin[c] = mid[c];
            s->v.mins[c] = lo[c] - mid[c];
            s->v.maxs[c] = hi[c] - mid[c];
            s->v.size[c] = hi[c] - lo[c];
        }
        SV_LinkEdict(s, false);
    }
    r.solidsAt = a;
}

// (ropes8e) Player `p`'s box as his moves meet other entities' boxes (hull::entBox: vr_hull_ent_width, crouched), in the
// world: `plo`, `phi`. His whole box (absmin/absmax) is wider: with it, walking into a pole put that box in one of its
// boxes before his narrower one met it, and he was let through (passSolids' "in a box").
void entBoxOf(const edict_t* p, glm::vec3& plo, glm::vec3& phi)
{
    float mn[3], mx[3];
    if(!hull::entBox(p, p->v.mins, p->v.maxs, mn, mx))
    {
        for(int c = 0; c < 3; c++)
        {
            mn[c] = p->v.mins[c];
            mx[c] = p->v.maxs[c];
        }
    }
    plo = geom::vec(p->v.origin) + geom::vec(mn);
    phi = geom::vec(p->v.origin) + geom::vec(mx);
}

// Whether box (`plo`, `phi`) is in box (`lo`, `hi`) by more than `by` units every way.
[[nodiscard]] bool boxIn(const glm::vec3& plo, const glm::vec3& phi, const glm::vec3& lo, const glm::vec3& hi, float by)
{
    for(int c = 0; c < 3; c++)
    {
        if(plo[c] > hi[c] - by || phi[c] < lo[c] + by)
        {
            return false;
        }
    }
    return true;
}

// The player let through pole `r`'s boxes (their owner: Quake's moves skip what the mover owns): one holding a climbable
// rope (hanging) within solidNear of it (he took it: the climb holds his body round it), or one in a box (made over him,
// or let go of it, still in it), until he is out; the one let through before first. None: everyone meets it.
// (ropes8d) Every other player let through likewise (`solidPass`, world.c's VR_PoleSolidPasses): two climbers on one pole.
void passSolids(Rope& r)
{
    if(r.solids.empty())
    {
        return;
    }
    const float half = r.params.thickness * 0.5f;
    const glm::vec3 lo = glm::min(r.p.front(), r.p.back()) - glm::vec3{half}, hi = glm::max(r.p.front(), r.p.back()) + glm::vec3{half};
    int owner = 0;
    za::U64 all = 0, inside = 0;
    for(int i = 1; i <= svs.maxclients; i++)
    {
        edict_t* pl = EDICT_NUM(i);
        if(!svs.clients[i - 1].active || pl->free)
        {
            continue;
        }
        glm::vec3 plo, phi;
        entBoxOf(pl, plo, phi); // (ropes8e: the box his moves meet them with, as the clients' prediction's)
        bool in = false;
        for(za::SizeT k = 0; k < r.solids.size() && !in; k++)
        {
            const edict_t* s = EDICT_NUM(r.solids[k]);
            // (ropes8e) its box itself: absmin / absmax are a unit wider each way (SV_LinkEdict), so a body stopped at its face
            // was "in" it by 0.97 and let through
            in = boxIn(plo, phi, geom::vec(s->v.origin) + geom::vec(s->v.mins), geom::vec(s->v.origin) + geom::vec(s->v.maxs), solidInside);
        }
        const bool pass = in || (climb::ropeHands(pl) > 0 && boxIn(plo, phi, lo - glm::vec3{solidNear}, hi + glm::vec3{solidNear}, 0.f));
        if(in && i <= 64)
        {
            inside |= za::U64{1} << (i - 1);
        }
        if(pass && (owner == 0 || i == r.solidOwner))
        {
            owner = i;
        }
        if(pass && i <= 64)
        {
            all |= za::U64{1} << (i - 1);
        }
    }
    r.solidOwner = owner;
    r.solidPass = all;
    r.solidIn = inside;
    for(const int s : r.solids)
    {
        EDICT_NUM(s)->v.owner = owner ? EDICT_TO_PROG(EDICT_NUM(owner)) : 0;
    }
}

[[nodiscard]] Params readParams(edict_t* e)
{
    const progs::FieldOffsets& f = progs::fields();
    Params pr;
    pr.pole = !ZA_STRCMP(PR_GetString(e->v.classname), "vr_pole");
    const bool styled = progs::fieldFloatOr(e, f.rope_style, 0.f) >= 1.f;
    pr.metal = pr.pole && styled;
    pr.chain = !pr.pole && (!ZA_STRCMP(PR_GetString(e->v.classname), "vr_chain") || styled);
    const float length = progs::fieldFloatOr(e, f.rope_length, 0.f);
    pr.length = length > 0.f ? za::clamp(length, 16.f, 2048.f) : 256.f;
    const float thickness = progs::fieldFloatOr(e, f.rope_thickness, 0.f);
    pr.thickness = thickness > 0.f ? za::clamp(thickness, 0.25f, pr.pole ? poleThickest : 8.f) : pr.pole ? poleThickness : 1.5f;
    const float pieces = progs::fieldFloatOr(e, f.rope_segments, 0.f);
    pr.pieces = za::clamp(static_cast<int>(za::lround(pieces > 0.f ? pieces : pr.length / 16.f)), 2, maxPieces);
    const float mass = progs::fieldFloatOr(e, f.rope_mass, 0.f);
    pr.mass = mass > 0.f ? za::clamp(mass, 0.1f, 1000.f) : (pr.chain ? 20.f : 5.f);
    const float stiffness = progs::fieldFloatOr(e, f.rope_stiffness, 0.f);
    pr.stiffness = stiffness > 0.f ? za::clamp(stiffness, 0.05f, 1.f) : 1.f;
    const float damping = progs::fieldFloatOr(e, f.rope_damping, 0.f);
    pr.damping = damping > 0.f ? za::clamp(damping, 0.f, 20.f) : 0.5f;
    pr.pinned = progs::fieldFloatOr(e, f.rope_pinned, 0.f) != 0.f;
    return pr;
}

// The rope of entity `e` (number `num`) made: hanging straight down from its origin (lying on the floor below where it
// is longer than the drop), or straight to its target.
void make(edict_t* e, int num)
{
    Rope& r = ropes[num];
    r = Rope{};
    r.params = readParams(e);
    Params& pr = r.params;
    r.top = vecOf(e->v.origin);
    bool targeted = false;
    if(const char* target = PR_GetString(e->v.target); target[0])
    {
        for(int i = 1; i < sv.qcvm.num_edicts && !targeted; i++)
        {
            edict_t* t = EDICT_NUM(i);
            if(!t->free && t != e && !ZA_STRCMP(PR_GetString(t->v.targetname), target))
            {
                r.bottom = vecOf(t->v.origin);
                targeted = true;
            }
        }
    }
    // (ropes7) Or its rope_end: its bottom pinned at that point (the toolgun's strung rope, a map's).
    const progs::FieldOffsets& f = progs::fields();
    if(const glm::vec3 to = f.rope_end >= 0 ? progs::fieldVec(e, f.rope_end) : glm::vec3{0.f}; !targeted && to != glm::vec3{0.f})
    {
        r.bottom = to;
        targeted = true;
    }
    // (ropes7) Strung between two points: its length the straight line's, its rope_slack longer (unless only its
    // rope_length is given); a pole's the line's.
    const float slack = za::clamp(progs::fieldFloatOr(e, f.rope_slack, 0.f), 0.f, 2.f);
    if(targeted && (pr.pole || slack > 0.f || progs::fieldFloatOr(e, f.rope_length, 0.f) <= 0.f))
    {
        pr.length = za::clamp(glm::distance(r.top, r.bottom) * (1.f + (pr.pole ? 0.f : slack)), 16.f, 2048.f);
    }
    r.params.pinned = pr.pinned || targeted || pr.pole;
    if(pr.pole && !targeted && progs::fieldFloatOr(e, f.rope_length, 0.f) <= 0.f)
    {
        pr.length = 2048.f; // (a pole with no length: down to the floor, found below)
    }
    glm::vec3 end = targeted ? r.bottom : r.top - glm::vec3{0.f, 0.f, pr.length};
    // (A free rope longer than the drop: the points below the floor lie on it. A pole stands on the floor there.)
    float floorZ = -1e9f;
    if(!targeted)
    {
        const trace_t tr = worldtrace::serverLine(r.top, end, nullptr);
        if(tr.fraction < 1.f)
        {
            floorZ = tr.endpos[2] + pr.thickness * 0.5f;
            if(pr.pole)
            {
                end = vecOf(tr.endpos);
                pr.length = za::max(16.f, r.top.z - end.z);
            }
        }
    }
    if(progs::fieldFloatOr(e, f.rope_segments, 0.f) <= 0.f)
    {
        pr.pieces = za::clamp(static_cast<int>(za::lround(pr.length / (pr.pole ? polePiece : 16.f))), 2, maxPieces);
    }
    const za::SizeT n = static_cast<za::SizeT>(pr.pieces + 1);
    r.p.resize(n);
    r.rest.resize(n - 1);
    r.pinned.resize(n);
    r.kick.resize(n);
    for(za::SizeT i = 0; i < n; i++)
    {
        const float t = static_cast<float>(i) / static_cast<float>(n - 1);
        glm::vec3 q = r.top + (end - r.top) * t;
        q.z = za::max(q.z, za::min(floorZ, r.top.z));
        r.p[i] = q;
        r.pinned[i] = 0;
        r.kick[i] = glm::vec3{0.f};
    }
    for(za::SizeT j = 0; j + 1 < n; j++)
    {
        r.rest[j] = pr.length / static_cast<float>(pr.pieces);
    }
    r.pinned[0] = 1;
    if(r.params.pinned)
    {
        r.pinned[n - 1] = 1;
        r.bottom = r.p[n - 1];
    }
    if(pr.pole)
    {
        for(za::SizeT i = 0; i < n; i++)
        {
            r.p[i] = r.top + (end - r.top) * (static_cast<float>(i) / static_cast<float>(n - 1)); // (no floor clamp: straight)
            r.pinned[i] = 1;
        }
        r.bottom = end;
    }
    r.prev = r.p;
    r.from = r.p;
    if(pr.pole)
    {
        freeSolids(r, num); // (ropes8a: a save's, or the last make's)
        placeSolids(r, num);
    }
    if(vr_rope_debug.value > 0.f)
    {
        Con_Printf("rope: %d made (%s, %.0f long, %d pieces, %.1f thick, %.1f kg%s)\n", num,
            pr.pole ? (pr.metal ? "metal pole" : "wooden pole") : pr.chain ? "chain" : "rope", pr.length, pr.pieces,
            pr.thickness, pr.mass, r.params.pinned ? ", pinned" : "");
    }
}

// Point `i` of `r` put out of the box (`mins`, `maxs` grown by the rope's thickness), along its shortest way out, taking
// the box's speed `vel` along that way (`h`: the last step's length).
void pushOutOfBox(Rope& r, za::SizeT i, const glm::vec3& mins, const glm::vec3& maxs, const glm::vec3& vel, float h)
{
    const float pad = r.params.thickness * 0.5f;
    glm::vec3& q = r.p[i];
    const glm::vec3 lo = mins - glm::vec3{pad}, hi = maxs + glm::vec3{pad};
    if(q.x <= lo.x || q.x >= hi.x || q.y <= lo.y || q.y >= hi.y || q.z <= lo.z || q.z >= hi.z)
    {
        return;
    }
    int axis = 0;
    float best = 1e9f, to = 0.f;
    for(int k = 0; k < 3; k++)
    {
        if(q[k] - lo[k] < best)
        {
            best = q[k] - lo[k];
            axis = k;
            to = lo[k];
        }
        if(hi[k] - q[k] < best)
        {
            best = hi[k] - q[k];
            axis = k;
            to = hi[k];
        }
    }
    glm::vec3 v = (q - r.prev[i]) / h;
    q[axis] = to;
    const float out = to > (lo[axis] + hi[axis]) * 0.5f ? 1.f : -1.f;
    if(v[axis] * out < vel[axis] * out)
    {
        v[axis] = vel[axis];
    }
    r.prev[i] = q - v * h;
}

// Point `i` of `r` put out of a hand's sphere, taking its speed along the way out.
void pushOutOfSphere(Rope& r, za::SizeT i, const Pusher& hand, float h)
{
    const float rad = handRadius + r.params.thickness * 0.5f;
    glm::vec3& q = r.p[i];
    const glm::vec3 d = q - hand.at;
    const float len = glm::length(d);
    if(len >= rad)
    {
        return;
    }
    const glm::vec3 nrm = len > 1e-3f ? d / len : glm::vec3{0.f, 0.f, 1.f};
    glm::vec3 v = (q - r.prev[i]) / h;
    q = hand.at + nrm * rad;
    const float vn = glm::dot(hand.vel, nrm);
    if(glm::dot(v, nrm) < vn)
    {
        v += nrm * (vn - glm::dot(v, nrm));
    }
    r.prev[i] = q - v * h;
}

// The players' hands this frame (their spheres and speeds), into scratch.hands.
void gatherHands(float dt)
{
    scratch.hands.clear();
    for(int c = 0; c < svs.maxclients && c < MAX_SCOREBOARD; c++)
    {
        const client_t& client = svs.clients[c];
        for(int h = 0; h < 2; h++)
        {
            HandSeen& seen = handsSeen[c][h];
            const int ofs = progs::bindings().hands[h].pos;
            if(!client.active || !client.edict || ofs < 0 || client.edict->v.health <= 0.f || vr_rope_hands.value <= 0.f ||
                climb::ropeHands(client.edict) > 0) // (a climber's hands hold the rope, they don't push it)
            {
                seen.valid = false;
                continue;
            }
            const glm::vec3 at = progs::fieldVec(client.edict, ofs);
            glm::vec3 vel = seen.valid && dt > 0.f ? (at - seen.at) / dt : glm::vec3{0.f};
            if(glm::length(vel) > maxHandSpeed)
            {
                vel = glm::vec3{0.f};
            }
            seen = HandSeen{at, true};
            scratch.hands.pushBack(Pusher{at, vel});
        }
    }
}

// What meets `r` this frame: the bodies (players, monsters, props: their boxes) and the hands.
void disturb(Rope& r, float h)
{
    const za::SizeT n = r.p.size();
    glm::vec3 lo{1e9f}, hi{-1e9f};
    for(const glm::vec3& q : r.p)
    {
        lo = glm::min(lo, q);
        hi = glm::max(hi, q);
    }
    const float pad = r.params.thickness * 0.5f + 1.f;
    lo -= glm::vec3{pad};
    hi += glm::vec3{pad};
    for(int num = 1; num < sv.qcvm.num_edicts; num++)
    {
        edict_t* e = EDICT_NUM(num);
        if(e->free || e->v.absmax[0] < lo.x || e->v.absmin[0] > hi.x || e->v.absmax[1] < lo.y || e->v.absmin[1] > hi.y ||
            e->v.absmax[2] < lo.z || e->v.absmin[2] > hi.z)
        {
            continue;
        }
        const int solid = static_cast<int>(e->v.solid);
        if(solid != SOLID_BBOX && solid != SOLID_SLIDEBOX && !box3d::isBox3DProp(num))
        {
            continue;
        }
        const glm::vec3 mins = vecOf(e->v.absmin), maxs = vecOf(e->v.absmax), vel = vecOf(e->v.velocity);
        for(za::SizeT i = 0; i < n; i++)
        {
            if(!r.pinned[i])
            {
                pushOutOfBox(r, i, mins, maxs, vel, h);
            }
        }
    }
    for(const Pusher& hand : scratch.hands)
    {
        if(hand.at.x < lo.x - handRadius || hand.at.x > hi.x + handRadius || hand.at.y < lo.y - handRadius ||
            hand.at.y > hi.y + handRadius || hand.at.z < lo.z - handRadius || hand.at.z > hi.z + handRadius)
        {
            continue;
        }
        for(za::SizeT i = 0; i < n; i++)
        {
            if(!r.pinned[i])
            {
                pushOutOfSphere(r, i, hand, h);
            }
        }
    }
}

// The point `s` units along `r` from its top (its pieces' rest lengths), and its speed over the last step.
[[nodiscard]] glm::vec3 pointAtArc(const Rope& r, float s, glm::vec3* vel = nullptr)
{
    const za::SizeT n = r.p.size();
    za::SizeT j = 0;
    float at = 0.f;
    while(j + 2 < n && at + r.rest[j] < s)
    {
        at += r.rest[j];
        j++;
    }
    const float t = za::clamp((s - at) / za::max(r.rest[j], 1e-3f), 0.f, 1.f);
    if(vel)
    {
        const float h = r.lastStep > 0.f ? r.lastStep : longestStep;
        *vel = glm::mix(r.p[j] - r.prev[j], r.p[j + 1] - r.prev[j + 1], t) / h;
    }
    return glm::mix(r.p[j], r.p[j + 1], t);
}

[[nodiscard]] float playerMass()
{
    return za::max(vr_box3d_player_mass.value, 1.f);
}

[[nodiscard]] glm::vec3 riderDir(const Rope& r, const glm::vec3& bob)
{
    const glm::vec3 d = bob - r.top;
    const float len = glm::length(d);
    return len > 1e-3f ? d / len : glm::vec3{0.f, 0.f, -1.f};
}

[[nodiscard]] float ropeLength(const Rope& r)
{
    float length = 0.f;
    for(const float piece : r.rest)
    {
        length += piece;
    }
    return length;
}

// (ropes7) Whether `r` is traversed hanging under it: pinned at both ends (or a pole), the line between them slanted
// past vr_rope_monkey_angle from upright.
[[nodiscard]] bool monkeyOf(const Rope& r)
{
    const glm::vec3 chord = r.bottom - r.top;
    const float len = glm::length(chord);
    const float angle = glm::radians(za::clamp(vr_rope_monkey_angle.value, 5.f, 90.f));
    return r.params.pinned && len > 1.f && za::abs(chord.z) < len * za::cos(angle);
}

// (ropes7) A monkey rider's hold `s` along the rope: on the taut line from its top through his bob (at sMid along it)
// to its bottom.
[[nodiscard]] glm::vec3 monkeyAt(const Rope& r, const glm::vec3& bob, float s)
{
    const float length = ropeLength(r), sm = r.rider.sMid;
    if(s <= sm)
    {
        return r.top + (bob - r.top) * (s / za::max(sm, 1e-3f));
    }
    return bob + (r.bottom - bob) * za::min(1.f, (s - sm) / za::max(length - sm, 1e-3f));
}

// (ropes7) The bob kept within the rope's reach of both its ends (its top sMid along it, its bottom the rest): the rope
// pulls, it doesn't push.
void monkeyHold(const Rope& r, glm::vec3& bob)
{
    const float k = za::max(r.rider.stretch, 1.f); // (as stretched as it lay when taken)
    const float toTop = r.rider.sMid * k, toBottom = (ropeLength(r) - r.rider.sMid) * k;
    for(int pass = 0; pass < 8; pass++)
    {
        if(const glm::vec3 d = bob - r.top; glm::length(d) > toTop)
        {
            bob = r.top + d * (toTop / glm::length(d));
        }
        if(const glm::vec3 d = bob - r.bottom; glm::length(d) > toBottom)
        {
            bob = r.bottom + d * (toBottom / glm::length(d));
        }
    }
}

[[nodiscard]] float monkeyMid(const Rope& r)
{
    const float length = ropeLength(r);
    return za::clamp((r.rider.sHigh + r.rider.sLow) * 0.5f, topClear, za::max(topClear, length - topClear));
}

// A new rider's bob: on the line from the top through his lowest hand's point, his swing the rope's there and his own
// as he took hold, shared by their weights (a jump onto a rope swings it), across the rope only. (ropes7) A monkey
// traverse's: at his hands' middle on the rope, with his swing and the rope's there shared as well.
void startRider(Rope& r, float h)
{
    Rider& rd = r.rider;
    if(rd.monkey)
    {
        if(rd.started)
        {
            return;
        }
        rd.started = true;
        // (Its length as it lies now, stretched a little by its weight: taken where it hangs, it doesn't jump up.)
        float lying = 0.f;
        for(za::SizeT i = 0; i + 1 < r.p.size(); i++)
        {
            lying += glm::distance(r.p[i], r.p[i + 1]);
        }
        rd.stretch = za::clamp(lying / za::max(ropeLength(r), 1.f), 1.f, 1.25f);
        rd.sMid = monkeyMid(r);
        glm::vec3 ropeVel{0.f};
        rd.bob = pointAtArc(r, rd.sMid, &ropeVel);
        monkeyHold(r, rd.bob);
        const float m = playerMass(), mr = r.params.mass;
        glm::vec3 v = (rd.grabVel * m + ropeVel * mr) / (m + mr);
        v.z = za::min(v.z, 0.f); // (a jump up to it doesn't throw the rope up: the hands meet it at the jump's top)
        if(glm::length(v) > maxRiderSpeed)
        {
            v *= maxRiderSpeed / glm::length(v);
        }
        rd.bobPrev = rd.bob - v * h;
        return;
    }
    rd.radius = rd.dHigh + riderBelow;
    if(rd.started)
    {
        return;
    }
    rd.started = true;
    glm::vec3 ropeVel{0.f};
    const glm::vec3 low = pointAtArc(r, rd.sLow, &ropeVel);    const glm::vec3 dir = riderDir(r, low);
    rd.bob = r.top + dir * rd.radius;
    const float m = playerMass(), mr = r.params.mass;
    glm::vec3 v = (rd.grabVel * m + ropeVel * mr) / (m + mr) * (rd.radius / za::max(rd.dLow, 1.f));
    v -= dir * glm::dot(v, dir);
    if(glm::length(v) > maxRiderSpeed)
    {
        v *= maxRiderSpeed / glm::length(v);
    }
    rd.bobPrev = r.params.pinned ? rd.bob : rd.bob - v * h;
}

// The kicks on a rider's laid points: his swing's, shared with his weight.
void riderKick(Rope& r, float lastH)
{
    Rider& rd = r.rider;
    glm::vec3 sum{0.f};
    float arc = 0.f;
    for(za::SizeT i = 1; i < r.p.size() && (rd.monkey || arc + r.rest[i - 1] <= rd.sLow + 1e-3f); i++)
    {
        arc += r.rest[i - 1];
        sum += r.kick[i] * (r.params.mass / static_cast<float>(r.p.size()));
    }
    if(rd.monkey)
    {
        rd.bobPrev -= sum / (playerMass() + r.params.mass) * lastH; // (ropes7: the bob at his hands)
    }
    else if(!r.params.pinned)
    {
        rd.bobPrev -= sum / (playerMass() + r.params.mass) * (rd.radius / za::max(rd.dLow, 1.f)) * lastH;
    }
}

// (ropes7) A step of a monkey traverse's rider: his bob's middle eased along the rope to his hands' (monkeyEase a
// second), the bob swung (gravity, the pump, the drag) and held within the rope's reach of its ends, then every piece laid
// on the taut line from the top through it to the bottom, pinned for the step.
void layMonkey(Rope& r, float h)
{
    Rider& rd = r.rider;
    const float want = monkeyMid(r);
    rd.sMid += za::clamp(want - rd.sMid, -monkeyEase * h, monkeyEase * h);
    glm::vec3 v = rd.bob - rd.bobPrev;
    if(r.lastStep > 0.f)
    {
        v *= h / r.lastStep;
    }
    v *= za::max(0.f, 1.f - za::max(vr_rope_swing_drag.value, 0.f) * h);
    if(glm::length(v) > maxRiderSpeed * h)
    {
        v *= maxRiderSpeed * h / glm::length(v);
    }
    glm::vec3 pump = rd.pump;
    if(const float p = glm::length(pump); p > 1e-3f)
    {
        const float along = glm::dot(v / h, pump / p);
        if(along < -1.f || along >= za::max(vr_rope_swing_cap.value, 0.f))
        {
            pump = glm::vec3{0.f};
        }
    }
    rd.bobPrev = rd.bob;
    rd.bob += v + (glm::vec3{0.f, 0.f, -sv_gravity.value} + pump) * (h * h);
    monkeyHold(r, rd.bob);
    float arc = 0.f;
    for(za::SizeT i = 1; i + 1 < r.p.size(); i++)
    {
        arc += r.rest[i - 1];
        r.p[i] = monkeyAt(r, rd.bob, arc);
        r.prev[i] = monkeyAt(r, rd.bobPrev, arc);
        r.pinned[i] = 1;
    }
}

// A step of a rider: his bob swung round the top (gravity, the pump; never further than its radius: the rope pulls, it
// doesn't push), then the pieces above his lowest hand laid on the line from the top through it, pinned for the step (a
// pinned rope's rider doesn't swing).
void layRider(Rope& r, float h)
{
    Rider& rd = r.rider;
    if(!r.params.pinned)
    {
        glm::vec3 v = rd.bob - rd.bobPrev;
        if(r.lastStep > 0.f)
        {
            v *= h / r.lastStep;
        }
        v *= za::max(0.f, 1.f - za::max(vr_rope_swing_drag.value, 0.f) * h);
        if(glm::length(v) > maxRiderSpeed * h)
        {
            v *= maxRiderSpeed * h / glm::length(v);
        }
        // The pump (P3): pushes his swing its way while it swings that way (not back: held, each swing out builds on the
        // last, as a swing pumped), up to vr_rope_swing_cap at his hands.
        glm::vec3 pump = rd.pump;
        if(const float p = glm::length(pump); p > 1e-3f)
        {
            const float along = glm::dot(v / h, pump / p) * (rd.dLow / za::max(rd.radius, 1.f));
            if(along < -1.f || along >= za::max(vr_rope_swing_cap.value, 0.f))
            {
                pump = glm::vec3{0.f};
            }
        }
        rd.bobPrev = rd.bob;
        rd.bob += v + (glm::vec3{0.f, 0.f, -sv_gravity.value} + pump) * (h * h);
        const glm::vec3 d = rd.bob - r.top;
        if(const float len = glm::length(d); len > rd.radius)
        {
            rd.bob = r.top + d * (rd.radius / len);
        }
    }
    const glm::vec3 dir = riderDir(r, rd.bob), dirPrev = riderDir(r, rd.bobPrev);
    const float scale = rd.sLow > 1e-3f ? rd.dLow / rd.sLow : 1.f;
    float arc = 0.f;
    for(za::SizeT i = 1; i < r.p.size() && arc + r.rest[i - 1] <= rd.sLow + 1e-3f; i++)
    {
        arc += r.rest[i - 1];
        r.p[i] = r.top + dir * (arc * scale);
        r.prev[i] = r.top + dirPrev * (arc * scale);
        r.pinned[i] = 1;
    }
}

[[nodiscard]] Rope* riddenBy(int client)
{
    for(auto& kv : ropes)
    {
        if(kv.second.rider.client == client)
        {
            return &kv.second;
        }
    }
    return nullptr;
}

constexpr float climbKnock = 90.f; // (ropes8b) units/s: the most a hold taken along it knocks a chain's links (step)

// A frame of `dt` seconds for `r`.
void step(Rope& r, float dt)
{
    const za::SizeT n = r.p.size();
    const Params& pr = r.params;
    r.from = r.p;
    if(pr.pole)
    {
        // (ropes7) A pole never moves but with its entity (a mover, the physgun): every point along by its top's move.
        const glm::vec3 moved = r.top - r.p[0];
        for(za::SizeT i = 0; i < n; i++)
        {
            r.p[i] += moved;
            r.kick[i] = glm::vec3{0.f};
        }
        r.bottom += moved;
        r.prev = r.p;
        r.lastStep = dt;
        r.speed = dt > 0.f ? glm::length(moved) / dt : 0.f;
        return;
    }
    // The kicks (explosions, shots) since the last frame: speed added to their points (a rider's: to his swing, shared
    // with his weight).
    const float lastH = r.lastStep > 0.f ? r.lastStep : dt;
    float kicked = 0.f; // (ropes8b: the hardest kick, units/s: its links knocked, ridden or not)
    for(za::SizeT i = 0; i < n; i++)
    {
        kicked = za::max(kicked, glm::length(r.kick[i]));
    }
    if(r.rider.client >= 0 && r.rider.started)
    {
        riderKick(r, lastH);
    }
    for(za::SizeT i = 0; i < n; i++)
    {
        if(!r.pinned[i])
        {
            r.prev[i] -= r.kick[i] * lastH;
        }
        r.kick[i] = glm::vec3{0.f};
    }
    const int steps = za::clamp(static_cast<int>(za::ceil(dt / longestStep)), 1, maxSteps);
    const float h = dt / static_cast<float>(steps);
    const int iterations = za::clamp(static_cast<int>(vr_rope_iterations.value), 1, 64);
    if(r.rider.client >= 0)
    {
        startRider(r, h);
    }
    for(int s = 0; s < steps; s++)
    {
        if(r.rider.client >= 0 && r.rider.monkey)
        {
            layMonkey(r, h);
        }
        else if(r.rider.client >= 0)
        {
            layRider(r, h);
        }
        verlet::fall(r.p.data(), r.prev.data(), r.pinned.data(), n, h, r.lastStep, pr.damping, sv_gravity.value);
        r.lastStep = h;
        r.p[0] = r.top;
        if(pr.pinned)
        {
            r.p[n - 1] = r.bottom;
        }
        verlet::holdLengths(r.p.data(), r.pinned.data(), n, r.rest.data(), iterations, pr.stiffness);
    }
    disturb(r, h);
    verlet::collide(r.p.data(), r.prev.data(), r.from.data(), r.pinned.data(), n, pr.thickness * 0.5f, 0.6f, nullptr,
        nullptr, true);
    for(za::SizeT i = 1; i < n; i++)
    {
        r.pinned[i] = pr.pinned && i + 1 == n ? 1 : 0; // (a rider's laid points: pinned for this frame's steps only)
    }
    float fastest = 0.f;
    for(za::SizeT i = 0; i < n; i++)
    {
        fastest = za::max(fastest, glm::length(r.p[i] - r.from[i]));
    }
    r.speed = dt > 0.f ? fastest / dt : 0.f;
    // (ropes8b, his flat note 2026-11-23: a chain clinked on and on, held still) How hard its links knock: each point's
    // move against the mean of its neighbours' (the moves' second difference along it), the most. A swing (the chain
    // turning about its top, a rider on it carried round) moves them alike: none; a push, a shot, a hand taking it or
    // pulling him up along it, the end flicking: they do.
    // Ridden, its free end under his hands is left out: it trails the swing he rides (whipping round with it, held
    // still or not), and the part he hangs on lies straight; his hands going along it (climbing: a hold taken higher
    // or lower) knock it as a firm push does (climbKnock at most), as do the kicks (shots, explosions).
    const bool ridden = r.rider.client >= 0 && r.rider.started && !r.rider.monkey;
    float climbed = 0.f;
    if(ridden && r.handsWas[0] >= 0.f)
    {
        climbed = za::fabs(r.rider.sHigh - r.handsWas[0]) + za::fabs(r.rider.sLow - r.handsWas[1]);
    }
    r.handsWas[0] = ridden ? r.rider.sHigh : -1.f;
    r.handsWas[1] = r.rider.sLow;
    float knock = 0.f, s = 0.f;
    for(za::SizeT i = 1; i + 1 < n; i++)
    {
        s += r.rest[i - 1];
        if(ridden && s + r.rest[i] > r.rider.sLow)
        {
            break;
        }
        const glm::vec3 m = r.p[i] - r.from[i], a = r.p[i - 1] - r.from[i - 1], b = r.p[i + 1] - r.from[i + 1];
        knock = za::max(knock, glm::length(m - 0.5f * (a + b)));
    }
    r.jostle = za::max(dt > 0.f ? za::max(knock / dt, za::min(climbed / dt, climbKnock)) : 0.f, kicked);
}

// Writes rope `num` to `msg` (as it is, or gone).
void write(sizebuf_t* msg, int num, const Rope* r)
{
    const int n = r ? static_cast<int>(r->p.size()) : 0;
    MSG_WriteByte(msg, protocol::svc_quakevr);
    MSG_WriteByte(msg, protocol::QVR_SVC_CLIMBROPE);
    MSG_WriteShort(msg, num);
    MSG_WriteByte(msg, r ? (r->params.chain ? flagChain : 0) | (r->params.pole ? flagPole : 0) |
                               (r->params.metal ? flagMetal : 0) | (r->monkey ? flagMonkey : 0)
                         : flagGone);
    MSG_WriteByte(msg, r ? za::clamp(static_cast<int>(za::lround(r->params.thickness * 16.f)), 1, 255) : 0);
    MSG_WriteByte(msg, n);
    glm::vec3 at{0.f};
    for(int i = 0; i < n; i++)
    {
        const glm::vec3& q = r->p[static_cast<za::SizeT>(i)];
        for(int k = 0; k < 3; k++)
        {
            if(i == 0)
            {
                MSG_WriteCoord(msg, q[k], sv.protocolflags);
                at[k] = q[k];
            }
            else
            {
                // From the one before as the client has it (no error adds up): eighths of a unit.
                const int d = za::clamp(static_cast<int>(za::lround((q[k] - at[k]) * 8.f)), -32767, 32767);
                MSG_WriteShort(msg, d);
                at[k] += static_cast<float>(d) / 8.f;
            }
        }
    }
}

[[nodiscard]] int writeSize(const Rope& r)
{
    return 8 + 12 + 6 * static_cast<int>(r.p.size());
}

void send(int num, Rope& r)
{
    bool changed = r.sent.size() != r.p.size();
    for(za::SizeT i = 0; i < r.p.size() && !changed; i++)
    {
        changed = glm::length(r.p[i] - r.sent[i]) > sentMoved;
    }
    const bool due = sv.qcvm.time - r.sentAt >= refreshTime || sv.qcvm.time < r.sentAt;
    if((!changed && !due) || sv.datagram.cursize + writeSize(r) > MAX_DATAGRAM - 64)
    {
        return;
    }
    write(&sv.datagram, num, &r);
    VR_BroadcastMessageEnd(); // a boundary (vr_server.cpp)
    r.sent = r.p;
    r.sentAt = sv.qcvm.time;
}

// ---------------------------------------------------------------------------------------------------------------------
// vr_rope_test: the ropes' physics and sync, in a game (docs/vr-port/ROPES.md, "Tests"). A state machine run by the
// server's frame: frames counted, never the clock.

struct RopeTest
{
    int stage{0}; // 0: not running
    int frames{0};
    testkit::Tally tally;
    int ropeA{0}; // a free rope 150 long, 10 pieces, hanging to a hand's breadth above the floor
    int chainB{0}; // a pinned chain made as the toolgun makes one
    int thing{0};  // the prop or monster pushing A
    za::Vector<glm::vec3> before;
    glm::vec3 feet{0.f};
    glm::vec3 fwd{1.f, 0.f, 0.f};
    int kept{0};         // the ropes Clear Props should keep (made as the map loaded)
    float chainBorn{0.f}; // B's .vr_born
};
RopeTest test;
constexpr const char* testName = "vr_rope_test";
constexpr float sceneLoaded = 1.5f; // QC VR_SCENE_LOADED (vr_cheats.qc): what's made later is Clear Props'

void testCheck(bool ok, const char* what)
{
    test.tally.check(ok, testName, what);
    if(ok)
    {
        Con_Printf("%s: PASS %s\n", testName, what);
    }
}

[[nodiscard]] Rope* ropeOf(int num)
{
    const auto it = ropes.find(num);
    return it == ropes.end() ? nullptr : &it->second;
}

[[nodiscard]] float displacement(const Rope& r, const za::Vector<glm::vec3>& was)
{
    float most = 0.f;
    for(za::SizeT i = 0; i < r.p.size() && i < was.size(); i++)
    {
        most = za::max(most, glm::length(r.p[i] - was[i]));
    }
    return most;
}

void placeWith(edict_t* e, const glm::vec3& at, float length, float thickness, int segments, float mass, float stiffness,
    float damping, bool pinned)
{
    const progs::FieldOffsets& f = progs::fields();
    const auto set = [e](int ofs, float v) {
        if(ofs >= 0)
        {
            progs::fieldFloat(e, ofs) = v;
        }
    };
    set(f.rope_length, length);
    set(f.rope_thickness, thickness);
    set(f.rope_segments, static_cast<float>(segments));
    set(f.rope_mass, mass);
    set(f.rope_stiffness, stiffness);
    set(f.rope_damping, damping);
    set(f.rope_pinned, pinned ? 1.f : 0.f);
    // Its top: the ceiling above the ghost, or its length (and a little) up.
    const float len = length > 0.f ? length : 256.f;
    const glm::vec3 start = at + glm::vec3{0.f, 0.f, 1.f};
    const trace_t tr = worldtrace::serverLine(start, start + glm::vec3{0.f, 0.f, len + 8.f}, e);
    const glm::vec3 top = vecOf(tr.endpos) - glm::vec3{0.f, 0.f, tr.fraction < 1.f ? 1.f : 0.f};
    for(int k = 0; k < 3; k++)
    {
        e->v.origin[k] = top[k];
    }
    SV_LinkEdict(e, false);
    ropes.erase(NUM_FOR_EDICT(e)); // (made afresh from these at the next frame)
}

// (ropes7) Rope entity `e` strung from `start` to `end`, `slack` longer than the line (a test's).
void stringBetween(edict_t* e, const glm::vec3& start, const glm::vec3& end, float slack)
{
    const progs::FieldOffsets& f = progs::fields();
    if(f.rope_end >= 0)
    {
        float* v = progs::fieldPtr(e, f.rope_end);
        v[0] = end.x;
        v[1] = end.y;
        v[2] = end.z;
    }
    if(f.rope_slack >= 0)
    {
        progs::fieldFloat(e, f.rope_slack) = slack;
    }
    if(f.rope_length >= 0)
    {
        progs::fieldFloat(e, f.rope_length) = 0.f;
    }
    for(int k = 0; k < 3; k++)
    {
        e->v.origin[k] = start[k];
    }
    SV_LinkEdict(e, false);
    ropes.erase(NUM_FOR_EDICT(e));
}

void testFrame()
{
    RopeTest& t = test;
    t.frames++;
    Rope* a = ropeOf(t.ropeA);
    Rope* b = ropeOf(t.chainB);
    const auto next = [](int stage) {
        RopeTest& t = test;
        t.stage = stage;
        t.frames = 0;
    };
    const auto mid = [](const Rope& r) { return r.p[r.p.size() / 2]; };
    switch(t.stage)
    {
    case 1:
    {
        edict_t* player = progs::playerEdict();
        const float yaw = glm::radians(player->v.angles[1]);
        t.fwd = glm::vec3{za::cos(yaw), za::sin(yaw), 0.f};
        t.feet = vecOf(player->v.origin) + glm::vec3{0.f, 0.f, player->v.mins[2]};
        edict_t* ea = box3d::spawnClass("vr_rope", t.feet + t.fwd * 128.f + glm::vec3{0.f, 0.f, 160.f}, glm::vec3{0.f});
        const glm::vec3 side{-t.fwd.y, t.fwd.x, 0.f};
        edict_t* eb = box3d::spawnClass("vr_chain", t.feet + t.fwd * 128.f + side * 64.f, glm::vec3{0.f});
        if(!ea || !eb)
        {
            testCheck(false, "the test's rope and chain spawned");
            next(99);
            break;
        }
        const progs::FieldOffsets& f = progs::fields();
        progs::fieldFloat(ea, f.rope_length) = 150.f;
        progs::fieldFloat(ea, f.rope_segments) = 10.f;
        placeWith(eb, t.feet + t.fwd * 128.f + side * 64.f, 96.f, 2.f, 6, 0.f, 0.f, 0.f, true); // (the toolgun's way)
        t.ropeA = NUM_FOR_EDICT(ea);
        t.chainB = NUM_FOR_EDICT(eb);
        next(2);
        break;
    }
    case 2:
    {
        testCheck(a && a->p.size() == 11 && !a->params.chain && !a->params.pinned, "a map-keyed rope made: 10 pieces, free");
        testCheck(b && b->p.size() == 7 && b->params.chain && b->params.pinned && b->params.thickness == 2.f,
            "a toolgun-placed chain made: 6 pieces, pinned, 2 thick");
        if(!ZA_STRCMP(sv.name, "vrropes"))
        {
            int count = 0, chains = 0, targeted = 0, poles = 0, monkeys = 0;
            for(const auto& kv : ropes)
            {
                if(kv.first == t.ropeA || kv.first == t.chainB)
                {
                    continue;
                }
                count++;
                chains += kv.second.params.chain ? 1 : 0;
                targeted += kv.second.params.pinned ? 1 : 0;
                poles += kv.second.params.pole ? 1 : 0;      // (ropes7: the wood and metal poles; ropes8a: the level and slanted)
                monkeys += monkeyOf(kv.second) ? 1 : 0;       // (ropes7: the level and the diagonal rope; ropes8a: the level pole)
            }
            char what[128];
            q_snprintf(what, sizeof(what), "vrropes' own ropes made (%d, %d chains, %d pinned, %d poles, %d monkey traverses)", count,
                chains, targeted, poles, monkeys);
            testCheck(count >= 13 && chains >= 3 && targeted >= 7 && poles == 4 && monkeys == 3, what);
        }
        next(3);
        break;
    }
    case 3: // settling (counted frames)
        if(t.frames < 360 || !a || !b)
        {
            if(!a || !b)
            {
                testCheck(false, "the test's ropes still there");
                next(99);
            }
            break;
        }
        {
            const glm::vec3 drop = a->p.front() - a->p.back();
            const float horiz = glm::length(glm::vec2{drop.x, drop.y});
            char what[128];
            q_snprintf(what, sizeof(what), "the free rope hangs straight (drop %.1f of 150, %.1f aside, %.1f u/s)", drop.z, horiz,
                a->speed);
            testCheck(drop.z > 142.f && drop.z < 151.f && horiz < 4.f && a->speed < 5.f, what);
            testCheck(glm::length(b->p.back() - b->bottom) < 0.5f && glm::length(b->p.front() - b->top) < 0.5f,
                "the pinned chain's ends held");
            // The client's copy (QVR_SVC_CLIMBROPE through the loopback): the server's points.
            const auto it = clientRopes.find(t.ropeA);
            float worst = 1e9f;
            if(it != clientRopes.end() && it->second.p.size() == a->p.size())
            {
                worst = 0.f;
                for(za::SizeT i = 0; i < a->p.size(); i++)
                {
                    worst = za::max(worst, glm::length(it->second.p[i] - a->p[i]));
                }
            }
            q_snprintf(what, sizeof(what), "the client has the rope's points (worst %.2f units off)", worst);
            testCheck(worst < 0.5f, what);
            const auto cb = clientRopes.find(t.chainB);
            testCheck(cb != clientRopes.end() && cb->second.chain && za::fabs(cb->second.thickness - 2.f) < 0.1f,
                "the client draws the chain as a chain, 2 thick");
            t.before = a->p;
            climbrope::blast(mid(*a) + t.fwd * 24.f, 100.f);
            next(4);
        }
        break;
    case 4:
        if(t.frames >= 8)
        {
            const float moved = a ? displacement(*a, t.before) : 0.f;
            char what[96];
            q_snprintf(what, sizeof(what), "an explosion throws it (%.1f units)", moved);
            testCheck(moved > 8.f, what);
            next(5);
        }
        break;
    case 5:
        if(t.frames >= 360 && a)
        {
            t.before = a->p;
            const glm::vec3 m = mid(*a);
            climbrope::shot(m - t.fwd * 64.f, m + t.fwd * 64.f, t.fwd * 400.f, 0.2f);
            next(6);
        }
        break;
    case 6:
        if(t.frames >= 8)
        {
            const float moved = a ? displacement(*a, t.before) : 0.f;
            char what[96];
            q_snprintf(what, sizeof(what), "a shot through it pushes it (%.1f units)", moved);
            testCheck(moved > 2.f, what);
            next(7);
        }
        break;
    case 7:
        if(t.frames >= 360 && a)
        {
            t.before = a->p;
            const glm::vec3 low = a->p.back();
            edict_t* crate = box3d::spawnClass("vr_crate", glm::vec3{low.x, low.y, t.feet.z + 1.f} + t.fwd * 6.f, glm::vec3{0.f});
            t.thing = crate ? NUM_FOR_EDICT(crate) : 0;
            next(8);
        }
        break;
    case 8:
        if(t.frames >= 30)
        {
            const float moved = a ? displacement(*a, t.before) : 0.f;
            char what[96];
            q_snprintf(what, sizeof(what), "a prop set down on its end pushes it aside (%.1f units)", moved);
            testCheck(t.thing && moved > 3.f, what);
            if(t.thing && !EDICT_NUM(t.thing)->free)
            {
                ED_Free(EDICT_NUM(t.thing));
            }
            t.thing = 0;
            next(9);
        }
        break;
    case 9:
        if(t.frames >= 360 && a)
        {
            t.before = a->p;
            const glm::vec3 low = a->p.back();
            edict_t* dog = box3d::spawnClass("monster_dog", glm::vec3{low.x, low.y, t.feet.z + 25.f} + t.fwd * 10.f,
                glm::vec3{0.f});
            t.thing = dog ? NUM_FOR_EDICT(dog) : 0;
            next(10);
        }
        break;
    case 10:
        if(t.frames >= 30)
        {
            const float moved = a ? displacement(*a, t.before) : 0.f;
            char what[96];
            q_snprintf(what, sizeof(what), "a monster standing in it pushes it aside (%.1f units)", moved);
            testCheck(t.thing && moved > 3.f, what);
            if(t.thing && !EDICT_NUM(t.thing)->free)
            {
                ED_Free(EDICT_NUM(t.thing));
            }
            t.thing = 0;
            if(t.ropeA && !EDICT_NUM(t.ropeA)->free)
            {
                ED_Free(EDICT_NUM(t.ropeA)); // (gone: the clients told)
            }
            next(11);
        }
        break;
    case 11:
        if(t.frames >= 6)
        {
            testCheck(!ropeOf(t.ropeA) && clientRopes.find(t.ropeA) == clientRopes.end(),
                "a rope removed is gone from the server and the client");
            // The toolgun's aim (Remove, the physgun) meets the chain; Clear Props (vr_scene_clean props) removes it.
            float d = 0.f;
            int aimed = 0;
            if(b)
            {
                d = 1000.f;
                aimed = climbrope::pick(mid(*b) - t.fwd * 100.f, t.fwd, 1000.f, d);
            }
            char what[96];
            q_snprintf(what, sizeof(what), "the toolgun's aim meets the chain (%d at %.0f units)", aimed, d);
            testCheck(aimed == t.chainB && d > 95.f && d < 101.f, what);
            // A rope made now (after the map's first 1.5 s: VR_Scene_IsRuntimeProp's), for Clear Props.
            edict_t* ec = box3d::spawnClass("vr_rope", t.feet + t.fwd * 128.f - glm::vec3{-t.fwd.y, t.fwd.x, 0.f} * 64.f,
                glm::vec3{0.f});
            t.thing = ec ? NUM_FOR_EDICT(ec) : 0;
            next(12);
        }
        break;
    case 12:
        if(t.frames >= 2)
        {
            testCheck(t.thing && ropeOf(t.thing), "a rope made for Clear Props");
            // What Clear Props should keep: the ropes made as the map loaded (QC VR_SCENE_LOADED, vr_cheats.qc: the map's
            // own, born before its first 1.5 s). The test's chain is one of them only when the test began that early
            // (`map vrropes; wait60; vr_rope_test` begins it at 2 s: then it goes too).
            t.before.clear();
            t.before.resize(ropes.size()); // (the count before)
            t.kept = 0;
            const int bornOfs = progs::fields().vr_born;
            const auto bornOf = [bornOfs](int ent) { return bornOfs >= 0 ? progs::fieldFloat(EDICT_NUM(ent), bornOfs) : 0.f; };
            for(const auto& kv : ropes)
            {
                t.kept += bornOf(kv.first) <= sceneLoaded ? 1 : 0;
            }
            t.chainBorn = b ? bornOf(t.chainB) : 0.f;
            Cbuf_AddText("vr_scene_clean props\n");
            next(13);
        }
        break;
    case 13:
        if(t.frames >= 6)
        {
            testCheck(t.thing && !ropeOf(t.thing) && clientRopes.find(t.thing) == clientRopes.end(),
                "Clear Props removes a rope made since the map loaded");
            char what[128];
            const bool chainEarly = t.chainBorn <= sceneLoaded;
            q_snprintf(what, sizeof(what),
                "Clear Props keeps the map's and the early ones (%d of %d, want %d; the chain born at %.2f s %s)",
                static_cast<int>(ropes.size()), static_cast<int>(t.before.size()), t.kept, t.chainBorn,
                chainEarly ? "kept" : "gone");
            testCheck(static_cast<int>(ropes.size()) == t.kept && (ropeOf(t.chainB) != nullptr) == chainEarly, what);
            t.thing = 0;
            next(99);
        }
        break;
    default:
        t.tally.verdict(testName);
        t.stage = 0;
        break;
    }
}

void test_f()
{
    if(!sv.active || svs.maxclients < 1 || !progs::bindings().isVrProgs || progs::fields().rope_length < 0)
    {
        Con_Printf("%s: FAIL a game with Quake VR's progs\n", testName);
        testkit::verdict(testName, 0, 1);
        return;
    }
    test = RopeTest{};
    test.stage = 1;
}

// vr_rope_info: the server's ropes (rope_sv lines) and the client's copies (rope_cl), then "rope_info: done" (the MP
// test's report: Misc/quakevr/mp/mp_test.py ropes).
void info_f()
{
    Con_Printf("climbable ropes: %d on the server, %d on the client\n", sv.active ? static_cast<int>(ropes.size()) : 0,
        static_cast<int>(clientRopes.size()));
    for(const auto& kv : ropes)
    {
        const Rope& r = kv.second;
        if(!sv.active)
        {
            break;
        }
        Con_Printf("rope_sv: ent=%d n=%d chain=%d last=%.1f,%.1f,%.1f speed=%.1f (%.0f long, %.1f thick, %.1f kg, stiffness %.2f, "
                   "damping %.2f%s; top %.0f %.0f %.0f)\n",
            kv.first, static_cast<int>(r.p.size()), r.params.chain ? 1 : 0, r.p.back().x, r.p.back().y, r.p.back().z, r.speed,
            r.params.length, r.params.thickness, r.params.mass, r.params.stiffness, r.params.damping,
            r.params.pinned ? ", pinned" : "", r.top.x, r.top.y, r.top.z);
        if(!r.solids.empty())
        {
            // (ropes8d) a pole's boxes and who passes them (passSolids: the owner, every climber on it; mp_test.py pole)
            Con_Printf("rope_solid: ent=%d boxes=%d owner=%d pass=%llu in=%llu\n", kv.first, static_cast<int>(r.solids.size()), r.solidOwner,
                static_cast<unsigned long long>(r.solidPass), static_cast<unsigned long long>(r.solidIn));
        }
        if(r.rider.client >= 0)
        {
            Con_Printf("rope_rider: ent=%d client=%d hands %.0f..%.0f along, swing radius %.0f\n", kv.first, r.rider.client,
                r.rider.sHigh, r.rider.sLow, r.rider.radius);
        }
    }
    for(const auto& kv : clientRopes)
    {
        const ClientRope& c = kv.second;
        if(!c.p.empty())
        {
            Con_Printf("rope_cl: ent=%d n=%d chain=%d last=%.1f,%.1f,%.1f thick=%.2f\n", kv.first, static_cast<int>(c.p.size()),
                c.chain ? 1 : 0, c.p.back().x, c.p.back().y, c.p.back().z, c.thickness);
        }
    }
    Con_Printf("rope_info: done\n");
}

// vr_rope_spawn [chain] [length]: a rope (1: a chain) hung 96 units ahead of the first player, as the toolgun hangs one
// (from the ceiling above, or its length up), with the spawn panel's other settings (a test aid; the MP test's).
void spawn_f()
{
    if(!sv.active)
    {
        Con_Printf("vr_rope_spawn: no game running\n");
        return;
    }
    const progs::ServerVm vm;
    edict_t* player = nullptr;
    for(int c = 0; c < svs.maxclients && !player; c++)
    {
        player = svs.clients[c].active ? svs.clients[c].edict : nullptr;
    }
    if(!player)
    {
        Con_Printf("vr_rope_spawn: no player\n");
        return;
    }
    const float yaw = glm::radians(player->v.angles[1]);
    const glm::vec3 at = vecOf(player->v.origin) + glm::vec3{za::cos(yaw) * 96.f, za::sin(yaw) * 96.f, player->v.mins[2]};
    const bool chain = Cmd_Argc() > 1 && Q_atoi(Cmd_Argv(1)) != 0;
    edict_t* e = box3d::spawnClass(chain ? "vr_chain" : "vr_rope", at, glm::vec3{0.f});
    if(!e)
    {
        Con_Printf("vr_rope_spawn: not made\n");
        return;
    }
    const float length = Cmd_Argc() > 2 ? za::clamp(static_cast<float>(Q_atof(Cmd_Argv(2))), 16.f, 2048.f)
                                        : za::clamp(vr_rope_spawn_length.value, 16.f, 2048.f);
    placeWith(e, at, length, za::clamp(vr_rope_spawn_thickness.value, 0.25f, 8.f),
        za::clamp(static_cast<int>(vr_rope_spawn_segments.value), 0, maxPieces), za::max(vr_rope_spawn_mass.value, 0.f),
        za::clamp(vr_rope_spawn_stiffness.value, 0.f, 1.f), za::max(vr_rope_spawn_damping.value, 0.f),
        vr_rope_spawn_pinned.value != 0.f);
    Con_Printf("vr_rope_spawn: %d %s, %.0f long, top %.0f %.0f %.0f\n", NUM_FOR_EDICT(e), chain ? "chain" : "rope", length,
        e->v.origin[0], e->v.origin[1], e->v.origin[2]);
}

// vr_rope_push <x> <y> <z>: every rope's free points given that speed (units/s) at the next step (a test aid).
void push_f()
{
    const glm::vec3 v{static_cast<float>(Q_atof(Cmd_Argv(1))), static_cast<float>(Q_atof(Cmd_Argv(2))),
        static_cast<float>(Q_atof(Cmd_Argv(3)))};
    for(auto& kv : ropes)
    {
        for(glm::vec3& k : kv.second.kick)
        {
            k += v;
        }
    }
    Con_Printf("vr_rope_push: %d ropes pushed\n", static_cast<int>(ropes.size()));
}

// ---------------------------------------------------------------------------------------------------------------------
// vr_rope_climb_test <play>: climbing a rope with the mock hands (ROPES.md, "Tests"). Once a player is in the game, a
// rope 470 long is hung half a metre ahead of him; settled (72 counted frames), the mock play <play> starts
// (Misc/quakevr/climb/rope_plays.py writes it): its "cmd vr_rope_climb_check <what> [<n>]" lines check, at its own
// times, what came of the hands' motion so far. The rope's speed at the lowest hand is kept each frame he rides it (the
// let-go's check). A safety give-up after 60 s of frames.

struct ClimbTest
{
    int stage{0}; // 0 off, 1 waiting for a player, 2 settling, 3 playing
    int frames{0};
    testkit::Tally tally;
    int rope{0};
    char play[256]{};
    glm::vec3 mark{0.f};
    glm::vec3 handVel{0.f}; // the rope's velocity at the lowest hand, the last frame he rode it
    float fastest{0.f};     // its fastest level speed since the last mark
    float freeFastest{0.f}; // (ropes6) the ridden rope's free end's fastest level speed since the last mark
    int grunts{0};          // (ropes6) his pull grunts at the last mark
    float length{470.f};    // the rope's (vrropes' ceiling 480 over the floor: to near the floor; ropes6: shorter)
    int kind{0};            // (ropes7) 0 a rope; 1 a wooden pole, 2 a metal one (standing on the floor); 3 a strung rope
    za::Vector<glm::vec3> made; // (ropes7) a pole's points as made (it never moves)
    glm::vec3 along{0.f};       // (ropes7) a strung rope's way (level, unit)
    bool raised{false};         // (ropes7) strung again higher by its sag
    float floorZ{0.f};          // (ropes7) his feet as the test began (on the floor)
};
ClimbTest climbTest;
constexpr float climbTestStrungHeight = 2.0f; // m over the feet: a strung rope's (the play's hands reach it there)
constexpr float climbTestSlack = 0.003f;      // a strung rope's slack: hung from him it sags about as much as settled (12 units)
constexpr const char* climbTestKinds[] = {"rope", "wood", "metal", "strung", "strungpole"}; // (ropes8a: 4 a level pole)
constexpr const char* climbTestName = "vr_rope_climb_test";
constexpr int climbTestGiveUp = 72 * 60;

// The test's player: the local one, in the game (alive). His client slot: NUM_FOR_EDICT less 1.
[[nodiscard]] edict_t* firstPlayer()
{
    edict_t* p = progs::localPlayer();
    return p && p->v.health > 0.f ? p : nullptr;
}

// (ropes8b, his flat note 2026-11-23: under a level rope the arms reached up off the shoulders, the hands twisted) The
// drawn hands under a monkey traverse's rope: each drawn fully on it, the fist's front (the knuckles) up (`up`: the
// least of their front's height, of 1), the palm's middle under the rope's axis (`under`: the least, units), and the
// body's arms reaching them (`gap`: the most cm a wrist was past the drawn arm's reach, avatar::embedArmGapCm).
// Returns the hands measured.
int monkeyHands(float& up, float& under, float& gap)
{
    int on = 0;
    up = 1.f;
    under = 1e9f;
    gap = 0.f;
    for(int h = 0; h < 2; h++)
    {
        glm::vec3 back{0.f}, front{0.f}, palm{0.f}, at{0.f}, way{0.f};
        float thick = 0.f, w = 0.f;
        if(climb::drawnShare(h) < 0.99f || !climb::drawnBack(h, back, &front) || !climb::drawnPalm(h, palm) ||
            !climb::ropeLine(h, at, way, thick, w))
        {
            continue;
        }
        on++;
        up = za::min(up, front.z);
        under = za::min(under, at.z - palm.z);
        gap = za::max(gap, avatar::embedArmGapCm(h));
    }
    return on;
}

// (ropes8b, his flat note 2026-11-23: climbing a rope the holsters floated off the hips) How far each hip holster (as
// placed this frame: body::holsterPositions, where it is drawn and found) stays on the hips: how far it is from
// its hip joint as posed (`off`, cm of the body, the worse of the two) and how far the gun's way up on it (its plate's
// up) is turned off the trunk's (the pelvis to the head bone: `tilt`, degrees, the worse). False: no body with legs.
[[nodiscard]] bool holstersOnHips(float& off, float& tilt)
{
    avatar::Skeleton k;
    if(!avatar::skeleton(k) || !k.legs || k.m2w <= 0.f)
    {
        return false;
    }
    body::HolsterPlates plates;
    const body::HolsterPositions h = body::holsterPositions(hands::current(), &plates);
    const glm::vec3 trunk = glm::normalize(k.headJoint - k.pelvis + glm::vec3{0.f, 0.f, 1e-4f});
    off = tilt = 0.f;
    for(int side = 0; side < 2; side++)
    {
        const int which = side == 0 ? body::LeftHip : body::RightHip;
        off = za::max(off, glm::distance(h[which], k.hip[side]) / k.m2w * 100.f);
        if(glm::length(plates[which].up) > 0.5f)
        {
            tilt = za::max(tilt, glm::degrees(za::acos(za::clamp(glm::dot(glm::normalize(plates[which].up), trunk), -1.f, 1.f))));
        }
    }
    return true;
}

za::Vector<glm::vec4> sinkVerts; // (ropes8a: handsSink's drawn hand vertices, their parts)
za::Vector<view::HandPart> sinkParts;

// (ropes8a) How deep the drawn hands on ropes are in them: each hand drawn fully on one, its mesh's vertices as drawn
// within a hand's length of its hold along the rope, the deepest's depth under the rope's surface (units; 0 none in it).
// `held`: hands measured; `part`: the deepest vertex's part of the hand (view::handPartName).
[[nodiscard]] float handsSink(int& held, const char*& part, float& gap)
{
    held = 0;
    part = "-";
    gap = 1e9f;
    float worst = 0.f;
    za::Vector<glm::vec4>& verts = sinkVerts;
    za::Vector<view::HandPart>& parts = sinkParts;
    for(int h = 0; h < 2; h++)
    {
        glm::vec3 at, up;
        float thick = 0.f, w = 0.f;
        if(!climb::ropeLine(h, at, up, thick, w) || w < 0.99f || !view::drawnHandSurface(h, verts, &parts))
        {
            continue;
        }
        held++;
        for(za::SizeT i = 0; i < verts.size(); i++)
        {
            const glm::vec3 d = glm::vec3{verts[i]} - at;
            const float along = glm::dot(d, up);
            const float in = thick * 0.5f - glm::length(d - up * along);
            gap = za::abs(along) < 6.f ? za::min(gap, -in) : gap; // (its nearest to the surface, out of it)
            if(za::abs(along) < 6.f && in > worst)
            {
                worst = in;
                part = i < parts.size() ? view::handPartName(parts[i]) : "-";
            }
        }
    }
    return worst;
}

void climbTestFrame()
{
    ClimbTest& t = climbTest;
    t.frames++;
    edict_t* player = firstPlayer();
    if(t.frames > climbTestGiveUp)
    {
        t.tally.check(false, climbTestName, "timed out");
        t.tally.verdict(climbTestName);
        t.stage = 0;
        return;
    }
    switch(t.stage)
    {
    case 1:
    {
        if(!player)
        {
            return;
        }
        const float yaw = glm::radians(player->v.angles[1]);
        const glm::vec3 at = vecOf(player->v.origin) + glm::vec3{za::cos(yaw), za::sin(yaw), 0.f} * (0.5f * units::metresToUnits()) +
                             glm::vec3{0.f, 0.f, player->v.mins[2]};
        if(t.kind == 3 || t.kind == 4)
        {
            // (ropes7) Strung level overhead (climbTestStrungHeight over the feet), half a metre ahead, across him (his
            // left to his right), taut (the mock hands meet it where the play reaches).
            const glm::vec3 right{za::sin(yaw), -za::cos(yaw), 0.f};
            const glm::vec3 mid = at + glm::vec3{za::cos(yaw), za::sin(yaw), climbTestStrungHeight} * glm::vec3{0.15f, 0.15f, 1.f} * units::metresToUnits(); // (0.65 m ahead: clear of his box)
            edict_t* e = box3d::spawnClass(t.kind == 4 ? "vr_pole" : "vr_rope", mid - right * 160.f, glm::vec3{0.f}); // (ropes8a: or a pole)
            if(!e)
            {
                t.tally.check(false, climbTestName, "the rope made");
                t.tally.verdict(climbTestName);
                t.stage = 0;
                return;
            }
            stringBetween(e, mid - right * 160.f, mid + right * 160.f, t.kind == 4 ? 0.f : climbTestSlack);
            if(progs::fields().rope_damping >= 0)
            {
                progs::fieldFloat(e, progs::fields().rope_damping) = 4.f; // (it settles soon)
            }
            t.rope = NUM_FOR_EDICT(e);
            t.along = right;
            t.floorZ = player->v.origin[2] + player->v.mins[2];
            Con_Printf("%s: strung rope %d across at %.0f %.0f, %.0f high\n", climbTestName, t.rope, mid.x, mid.y, mid.z);
            t.stage = 2;
            t.frames = 0;
            return;
        }
        const bool pole = t.kind == 1 || t.kind == 2;
        edict_t* e = box3d::spawnClass(pole ? "vr_pole" : "vr_rope", at, glm::vec3{0.f});
        if(!e)
        {
            t.tally.check(false, climbTestName, "the rope made");
            t.tally.verdict(climbTestName);
            t.stage = 0;
            return;
        }
        placeWith(e, at, 470.f, pole ? poleThickness : 1.5f, 0, 0.f, 1.f, 0.5f, false);
        if(progs::fields().rope_length >= 0)
        {
            progs::fieldFloat(e, progs::fields().rope_length) = t.length; // (from the same ceiling: shorter, ropes6)
        }
        if(pole && progs::fields().rope_style >= 0)
        {
            progs::fieldFloat(e, progs::fields().rope_style) = t.kind == 2 ? 1.f : 0.f;
        }
        t.rope = NUM_FOR_EDICT(e);
        Con_Printf("%s: %s %d hung at %.0f %.0f, top %.0f\n", climbTestName, climbTestKinds[t.kind], t.rope, at.x, at.y,
            e->v.origin[2]);
        t.stage = 2;
        t.frames = 0;
        return;
    }
    case 2:
        if(t.kind == 3 && t.frames < 216)
        {
            return; // (a strung rope: 3 s to settle, its bounce died down: damping 4 a second)
        }
        if(t.frames >= 72 && t.kind == 3 && !t.raised)
        {
            // (ropes7) A strung rope settled sags (its pieces' pull back each step, under its weight): strung again that
            // much higher, so its middle settles where the play's hands meet it.
            if(const Rope* r = ropeOf(t.rope); r && r->p.size() > 2)
            {
                const float sag = r->top.z - r->p[r->p.size() / 2].z;
                const glm::vec3 up{0.f, 0.f, sag};
                stringBetween(EDICT_NUM(t.rope), r->top + up, r->bottom + up, climbTestSlack);
                Con_Printf("%s: strung again %.1f higher (its sag)\n", climbTestName, sag);
            }
            t.raised = true;
            t.frames = 0;
            return;
        }
        if(t.frames >= 72)
        {
            if(const Rope* r = ropeOf(t.rope))
            {
                t.made = r->p;
            }
            Cbuf_AddText(va("vr_mock_play \"%s\"\n", t.play));
            t.stage = 3;
        }
        return;
    default:
    {
        int ent = 0;
        glm::vec3 v{0.f};
        if(player && climbrope::riding(NUM_FOR_EDICT(player) - 1, ent, v))
        {
            t.handVel = v;
            t.fastest = za::max(t.fastest, glm::length(glm::vec2{v.x, v.y}));
            // (The frame's step is later in serverFrame: this is the last frame's move of its bottom point.)
            if(const Rope* r = ropeOf(ent); r && r->p.size() == r->from.size() && !r->p.empty() && host_frametime > 0.0)
            {
                const glm::vec3 d = r->p.back() - r->from.back();
                t.freeFastest = za::max(t.freeFastest, glm::length(glm::vec2{d.x, d.y}) / static_cast<float>(host_frametime));
            }
        }
        return;
    }
    }
}

void climbTest_f()
{
    if(!sv.active || !progs::bindings().isVrProgs || progs::fields().rope_length < 0 || Cmd_Argc() < 2)
    {
        Con_Printf("%s: FAIL usage: vr_rope_climb_test <play> [length] [rope|wood|metal|strung] in a game with Quake VR's progs\n",
            climbTestName);
        testkit::verdict(climbTestName, 0, 1);
        return;
    }
    climbTest = ClimbTest{};
    q_strlcpy(climbTest.play, Cmd_Argv(1), sizeof(climbTest.play));
    if(Cmd_Argc() > 2 && Q_atof(Cmd_Argv(2)) > 0.0)
    {
        climbTest.length = za::clamp(static_cast<float>(Q_atof(Cmd_Argv(2))), 32.f, 2048.f);
    }
    for(int k = 0; k < 5 && Cmd_Argc() > 3; k++)
    {
        climbTest.kind = !ZA_STRCMP(Cmd_Argv(3), climbTestKinds[k]) ? k : climbTest.kind;
    }
    climbTest.stage = 1;
}

void climbCheck_f()
{
    ClimbTest& t = climbTest;
    edict_t* player = firstPlayer();
    if(t.stage != 3 || !player)
    {
        Con_Printf("vr_rope_climb_check: no vr_rope_climb_test playing\n");
        return;
    }
    const progs::ServerVm vm;
    const char* what = Cmd_Argv(1);
    const float n = static_cast<float>(Q_atof(Cmd_Argv(2)));
    const glm::vec3 o = vecOf(player->v.origin), v = vecOf(player->v.velocity);
    const int hands = climb::ropeHands(player);
    const auto check = [&](bool ok, const char* line) {
        if(t.tally.check(ok, climbTestName, line))
        {
            Con_Printf("%s: PASS %s\n", climbTestName, line);
        }
    };
    const glm::vec2 hv{v.x, v.y}, hh{t.handVel.x, t.handVel.y};
    if(!ZA_STRCMP(what, "held"))
    {
        check(hands == static_cast<int>(n), va("%d hands on the rope (%d)", static_cast<int>(n), hands));
    }
    else if(!ZA_STRCMP(what, "mark"))
    {
        t.mark = o;
        t.fastest = 0.f;
        t.freeFastest = 0.f;
        t.grunts = climb::ropePullGrunts(player);
        Con_Printf("%s: mark at %.1f %.1f %.1f\n", climbTestName, o.x, o.y, o.z);
    }
    else if(!ZA_STRCMP(what, "swing"))
    {
        check(t.fastest >= n && t.fastest <= vr_rope_swing_cap.value + 25.f,
            va("pumped the swing to %.0f units/s at the hands (at least %.0f, the cap %.0f)", t.fastest, n, vr_rope_swing_cap.value));
    }
    else if(!ZA_STRCMP(what, "rose"))
    {
        check(o.z - t.mark.z >= n, va("climbed up %.1f units (at least %.0f)", o.z - t.mark.z, n));
    }
    else if(!ZA_STRCMP(what, "lowered"))
    {
        check(t.mark.z - o.z >= n, va("climbed down %.1f units (at least %.0f)", t.mark.z - o.z, n));
    }
    else if(!ZA_STRCMP(what, "nudge"))
    {
        // A sideways push on his swing (a test aid; the pump is P3's): n units/s at the hands, along +x.
        Rope* r = riddenBy(NUM_FOR_EDICT(player) - 1);
        if(r && r->rider.started)
        {
            const float h = r->lastStep > 0.f ? r->lastStep : longestStep;
            r->rider.bobPrev -= glm::vec3{n, 0.f, 0.f} * (h * r->rider.radius / za::max(r->rider.dLow, 1.f));
        }
        check(r != nullptr, "the rope ridden (nudged)");
    }
    else if(!ZA_STRCMP(what, "moved"))
    {
        const float d = glm::length(glm::vec2{o.x - t.mark.x, o.y - t.mark.y});
        check(d >= n, va("swung %.1f units with the rope (at least %.0f)", d, n));
    }
    else if(!ZA_STRCMP(what, "flung"))
    {
        check(hands == 0 && glm::length(hh) >= n && glm::dot(hv, hh) > 0.f && glm::length(hv) >= 0.6f * glm::length(hh),
            va("let go at %.0f units/s level, the rope's at the hands %.0f (at least %.0f)", glm::length(hv), glm::length(hh), n));
    }
    else if(!ZA_STRCMP(what, "jumped"))
    {
        check(hands == 0 && v.z >= t.handVel.z + 0.6f * vr_rope_jump.value && glm::dot(hv, hh) >= 0.f,
            va("jumped off at %.0f up, %.0f level (the rope's %.0f up, %.0f level)", v.z, glm::length(hv), t.handVel.z,
                glm::length(hh)));
    }
    else if(!ZA_STRCMP(what, "calm"))
    {
        // (ropes6) Climbed up close to it: its swing at the hands at most n units/s since the mark, its free end at most
        // the second number's.
        const float freeMost = static_cast<float>(Q_atof(Cmd_Argv(3)));
        check(t.fastest <= n && t.freeFastest <= freeMost,
            va("calm: swing %.1f u/s at the hands (max %.0f), free end %.1f (max %.0f)", t.fastest, n, t.freeFastest,
                freeMost));
    }
    else if(!ZA_STRCMP(what, "grunts"))
    {
        // (ropes6) His pull grunts since the mark: n to the second number (a pull up grunts, a grab or a climb down not).
        const int most = Q_atoi(Cmd_Argv(3)), got = climb::ropePullGrunts(player) - t.grunts;
        check(got >= static_cast<int>(n) && got <= most, va("pull grunts since the mark: %d (%d to %d)", got, static_cast<int>(n), most));
    }
    else if(!ZA_STRCMP(what, "legs"))
    {
        // (ropes6) Hanging from a short rope taken overhead (its end near his legs): the feet hang under the pelvis, at
        // least n units under it, never reaching up for the rope's end. (The body as last posed: single player.)
        avatar::Skeleton k;
        const bool posed = avatar::skeleton(k);
        const float high = posed ? za::max(k.ankle[0].z, k.ankle[1].z) - k.pelvis.z : 0.f;
        check(posed && high <= -n, va("the feet hang: the higher ankle %.1f units from the pelvis's height (at most %.0f)", high, -n));
    }
    else if(!ZA_STRCMP(what, "grip"))
    {
        // (ropes6) The drawn hands on the rope gripping it, the knuckles outwards: the back of the right hand to his
        // right, the left's to his left (vr_rope_hand_grip 2, VR too). n: hands that must be drawn on it.
        const float yaw = glm::radians(player->v.angles[1]);
        const glm::vec3 right{za::sin(yaw), -za::cos(yaw), 0.f};
        int on = 0;
        float worst = 1.f;
        for(int h = 0; h < 2; h++)
        {
            glm::vec3 back{0.f};
            if(climb::drawnShare(h) >= 0.99f && climb::drawnBack(h, back))
            {
                on++;
                worst = za::min(worst, glm::dot(back, right) * (h == 1 ? 1.f : -1.f));
            }
        }
        check(on >= static_cast<int>(n) && worst >= 0.7f,
            va("%d hands drawn gripping it (%d), the knuckles outwards (worst %.2f of 1)", static_cast<int>(n), on, worst));
    }
    else if(!ZA_STRCMP(what, "monkeygrip"))
    {
        // (ropes8b) Under a monkey traverse's rope: n hands drawn on it, the knuckles up, the palms under it, the arms
        // reaching them (monkeyHands).
        float up = 0.f, under = 0.f, gap = 0.f;
        const int on = monkeyHands(up, under, gap);
        check(on >= static_cast<int>(n) && up >= 0.7f && under > 0.f && gap <= 1.f,
            va("%d hands drawn under it (%d): the knuckles up (least %.2f of 1), the palms %.1f under its axis, the arms "
               "reaching them (%.1f cm past, at most 1)",
                static_cast<int>(n), on, up, under, gap));
    }
    else if(!ZA_STRCMP(what, "sink"))
    {
        // (ropes8a) The drawn hands on the rope round it, not through it: one or both drawn fully on it, the deepest
        // vertex at most n units under its surface.
        int held = 0;
        const char* part = "-";
        float gap = 0.f;
        const float sink = handsSink(held, part, gap);
        check(held >= 1 && sink <= n && gap <= 0.5f, va("the hands round it, not in it: %d drawn on it, the deepest %.2f units in (%s; at most %.2f), on it (%.2f off at the nearest)",
                                          held, sink, part, n, za::max(gap, 0.f)));
    }
    else if(!ZA_STRCMP(what, "where"))
    {
        // (ropes7) A test writer's aid: the hands and the feet now.
        if(const VrMove* m = server::clientMove(player))
        {
            Con_Printf("%s: where: off hand %.1f %.1f %.1f, main %.1f %.1f %.1f, feet %.1f\n", climbTestName, m->hands[0].pos.x,
                m->hands[0].pos.y, m->hands[0].pos.z, m->hands[1].pos.x, m->hands[1].pos.y, m->hands[1].pos.z, o.z + player->v.mins[2]);
            if(climbrope::Grab g; climbrope::findGrab(m->hands[0].pos, 64.f, g))
            {
                Con_Printf("%s: where: the rope nearest the off hand at %.1f %.1f %.1f\n", climbTestName, g.at.x, g.at.y, g.at.z);
            }
        }
    }
    else if(!ZA_STRCMP(what, "monkey"))
    {
        // (ropes7) Hanging under a strung rope: ridden as a monkey traverse, the feet off the floor by n units at least.
        const Rope* r = riddenBy(NUM_FOR_EDICT(player) - 1);
        const Rope* pole = t.kind == 4 && hands > 0 ? ropeOf(t.rope) : nullptr; // (ropes8a: a pole has no rider)
        const bool hung = (r && r->rider.monkey) || (pole && pole->monkey);
        const float off = o.z + player->v.mins[2] - t.floorZ;
        check(hung && off >= n, va("hanging under it, a monkey traverse (%s), the feet up %.1f (at least %.0f)",
                                                   hung ? "yes" : "no", off, n));
    }
    else if(!ZA_STRCMP(what, "along"))
    {
        // (ropes7) Gone along the strung rope n units at least since the mark (either way), still hanging.
        const float d = za::abs(glm::dot(o - t.mark, t.along));
        check(hands > 0 && d >= n, va("went along the rope %.1f units (at least %.0f), %d hands on it", d, n, hands));
    }
    else if(!ZA_STRCMP(what, "dropped"))
    {
        // (ropes7) Let go: off it, falling (or down), n units under the mark at least.
        check(hands == 0 && t.mark.z - o.z >= n, va("let go: off it, dropped %.1f units (at least %.0f)", t.mark.z - o.z, n));
    }
    else if(!ZA_STRCMP(what, "fixed"))
    {
        // (ropes7) A pole never moves: its points where they were made.
        const Rope* r = ropeOf(t.rope);
        const float moved = r && r->params.pole && r->p.size() == t.made.size() ? displacement(*r, t.made) : 1e9f;
        check(moved < 0.01f, va("the pole never moved (%.3f units)", moved));
    }
    else if(!ZA_STRCMP(what, "sound"))
    {
        // (ropes7) The rope's last grip sound: its recording's name has `Cmd_Argv(2)` in it (wood, metal, soft).
        int count = 0;
        const char* last = physsound::ropeSoundInfo(t.rope, count);
        check(count > 0 && ZA_STRSTR(last, Cmd_Argv(2)) != nullptr, va("its grip sounds of %s (%d, the last %s)", Cmd_Argv(2), count, last));
    }
    else if(!ZA_STRCMP(what, "wrapped"))
    {
        // (ropes7) The feet round it as round a rope: both ankles within n units of it (the body as last posed).
        avatar::Skeleton k;
        const bool posed = avatar::skeleton(k);
        const float far = posed ? za::max(climbrope::clientRopeDistance(k.ankle[0]), climbrope::clientRopeDistance(k.ankle[1])) : 1e9f;
        check(posed && far <= n, va("the feet round it: the further ankle %.1f units from it (at most %.0f)", far, n));
    }
    else if(!ZA_STRCMP(what, "done"))
    {
        t.tally.verdict(climbTestName);
        t.stage = 0;
        Cbuf_AddText("vr_mock_play\n");
    }
    else
    {
        Con_Printf("vr_rope_climb_check: unknown check \"%s\"\n", what);
    }
}

// vr_rope_flat_test (P3, ROPES.md "Tests"; flat-screen play, -flatscreenmode, in vrropes; vr_climb.cpp flatRope): a rope
// the test hangs to near the floor walked into with forward held (taken, both hands), climbed looking up (the feet up 80
// units, the hands hand over hand: their holds' serials change, both drawn on it), climbed down looking down (off it
// standing on the floor); then rope to rope (Tarzan) on the map's swinging row: set by its first rope in the air (taken),
// the forward key pumps its swing towards the second (192 on), let go with jump at the swing's forward peak, the second
// caught in the air. Counted server frames and the swing's own state; the frame limits a safety net (FAIL timed out).
struct FlatTest
{
    int stage{0}; // 0 off
    int frames{0};
    testkit::Tally tally;
    int rope{0};             // the test's rope (near the floor), and the row's first two
    int first{0}, second{0};
    int ropesBefore{0};      // climb::flatRopes before a take
    float feet0{0.f};
    int lastSerial{-1};
    int serials{0};
    int handBits{0};
    float fastest{0.f};      // the swing's fastest level speed at the hands
    float fastEarly{0.f};    // (ropes9) its fastest along the view in the walk in's first half second
    float lastVx{0.f};
    float pitch{0.f}, yaw{0.f};
    bool pole{false};        // (ropes7) `vr_rope_flat_test pole`: a wooden pole instead of the rope, no rope to rope
    float holstersStanding[2]{0.f, 0.f}; // (ropes8b) holstersOnHips standing before the rope: cm, degrees
    bool chain{false};       // (ropes8b) `vr_rope_flat_test chain`: a chain, climbed (it clinks) and held still (quiet)
    int clinks{0};           // (ropes8b) the chain's clinks at the stage's start (physsound::ropeSoundInfo)
    bool shots{false};       // (ropes8b) `vr_rope_flat_test <mode> shots`: screenshots of the body from outside while hanging
    bool wall{false};        // (ropes8e) `vr_rope_flat_test wall`: vr_flat_rope 0, walked into the pole: stopped by its boxes
    float flatRopeWas{1.f};  // (ropes8e) vr_flat_rope before it (put back at the end)
    glm::vec3 wallDir{1.f, 0.f, 0.f}; // (ropes8e) the way he walks into it (level)
    za::Vector<glm::vec3> made; // (ropes7) the pole's points as made (it never moves)
};
FlatTest flatTest;
constexpr const char* flatTestName = "vr_rope_flat_test";
constexpr glm::vec3 flatTestRope{200.f, -400.f, 0.f}; // (vrropes: open floor south-east of the row)
constexpr glm::vec3 flatTestRow[2] = {{-288.f, 0.f, 0.f}, {-96.f, 0.f, 0.f}};
constexpr float flatStrungHeight = 110.f; // (ropes7) units over the floor: the strung rope (a jump up to it)
constexpr float flatPoleHeight = 85.f;    // (ropes8a) a level pole's (as high as the strung rope settles: no sag)

void flatTestNext(FlatTest& t, int stage)
{
    t.stage = stage;
    t.frames = 0;
}

void flatTestPlace(edict_t* player, const glm::vec3& at, float yaw)
{
    for(int k = 0; k < 3; k++)
    {
        player->v.origin[k] = player->v.oldorigin[k] = at[k];
        player->v.velocity[k] = 0.f;
    }
    SV_LinkEdict(player, false);
    flatTest.yaw = yaw;
}

void flatTestEnd(FlatTest& t, bool timedOut, const char* what)
{
    if(timedOut)
    {
        t.tally.check(false, flatTestName, va("timed out: %s", what));
    }
    Cbuf_AddText("-forward\n-back\n-moveright\n-jump\n");
    if(t.wall)
    {
        Cvar_SetValueQuick(&vr_flat_rope, t.flatRopeWas);
    }
    t.tally.verdict(flatTestName);
    t.stage = 0;
}

void flatTestFrame()
{
    FlatTest& t = flatTest;
    t.frames++;
    edict_t* player = firstPlayer();
    if(!player)
    {
        return;
    }
    const int client = NUM_FOR_EDICT(player) - 1;
    cl.viewangles[PITCH] = t.pitch;
    cl.viewangles[YAW] = t.yaw;
    bool climbing = false;
    const int taken = climb::flatRopes(player, climbing);
    const float feet = player->v.origin[2] + player->v.mins[2];
    const bool ground = (static_cast<int>(player->v.flags) & FL_ONGROUND) != 0;
    if(climbing)
    {
        const int bits = cl.stats[protocol::STAT_QVR_CLIMB];
        const int serial = (bits >> 2) & 4095;
        t.serials += t.lastSerial >= 0 && serial != t.lastSerial ? 1 : 0;
        t.lastSerial = serial;
        t.handBits |= bits & 3;
    }
    switch(t.stage)
    {
    case 1: // the rope hung, the player set before it
    {
        const glm::vec3 at = flatTestRope + glm::vec3{0.f, 0.f, 1.f};
        edict_t* e = box3d::spawnClass(t.pole ? "vr_pole" : t.chain ? "vr_chain" : "vr_rope", at, glm::vec3{0.f});
        if(!e)
        {
            t.tally.check(false, flatTestName, "the rope made");
            flatTestEnd(t, false, "");
            return;
        }
        placeWith(e, at, 480.f, t.pole ? poleThickness : t.chain ? 2.f : 1.5f, 0, 0.f, 1.f, 0.5f, false);
        t.rope = NUM_FOR_EDICT(e);
        t.pitch = 0.f;
        flatTestPlace(player, flatTestRope + glm::vec3{-80.f, 0.f, -player->v.mins[2] + 1.f}, 0.f);
        flatTestNext(t, 2);
        return;
    }
    case 2: // settled (the rope at rest, him on the floor): forward
        if(t.frames >= 108)
        {
            if(const Rope* r = ropeOf(t.rope))
            {
                t.made = r->p;
            }
            t.ropesBefore = taken;
            if(t.wall)
            {
                // (ropes8e) Set back from it along the way forward walks him (a headset's: its view's yaw, v_angle).
                const float yaw = player->v.v_angle[YAW] * (glm::pi<float>() / 180.f);
                t.wallDir = glm::vec3{za::cos(yaw), za::sin(yaw), 0.f};
                const glm::vec3 at = glm::vec3{flatTestRope.x, flatTestRope.y, player->v.origin[2]} - t.wallDir * 80.f;
                for(int k = 0; k < 3; k++)
                {
                    player->v.origin[k] = player->v.oldorigin[k] = at[k];
                    player->v.velocity[k] = 0.f;
                }
                SV_LinkEdict(player, false);
            }
            if(!holstersOnHips(t.holstersStanding[0], t.holstersStanding[1]))
            {
                t.holstersStanding[0] = t.holstersStanding[1] = 0.f;
            }
            Cbuf_AddText("+forward\n");
            flatTestNext(t, 3);
        }
        return;
    case 3: // walked into it
        if(t.wall)
        {
            // (ropes8e) Not taking it (vr_flat_rope 0): forward held 2 s into the pole, his box stopped by its boxes
            // short of it (its near face less his half width against entities), never through, not climbing.
            if(t.frames < 144)
            {
                return;
            }
            const Rope* r = ropeOf(t.rope);
            const glm::vec3 to = glm::vec3{flatTestRope.x, flatTestRope.y, player->v.origin[2]} - geom::vec(player->v.origin);
            const float gap = glm::dot(to, t.wallDir) - poleThickness * 0.5f;
            const bool ok = r && !r->solids.empty() && gap > 0.f && gap <= -player->v.mins[0] + 0.5f && !climbing &&
                            taken == t.ropesBefore;
            const char* what = va("walked into the pole not taking it (vr_flat_rope 0): stopped %.1f units short of its face "
                                  "(his box %.1f), its %d boxes, not taken",
                gap, -player->v.mins[0], r ? static_cast<int>(r->solids.size()) : 0);
            t.tally.check(ok, flatTestName, what);
            if(ok)
            {
                Con_Printf("%s: PASS %s\n", flatTestName, what);
            }
            flatTestEnd(t, false, "");
            return;
        }
        if(taken > t.ropesBefore && climbing)
        {
            t.tally.check(true, flatTestName, "walked into the rope: taken");
            Con_Printf("%s: PASS walked into the rope: taken (feet at %.1f)\n", flatTestName, feet);
            t.feet0 = feet;
            t.serials = 0;
            t.lastSerial = -1;
            t.handBits = 0;
            // (ropes9, his note 2026-11-23: a pole climbed looking level, forward held; a rope pumped looking level
            // first, then climbed looking up; a chain climbed looking up from still: its clinks as he climbs)
            t.pitch = t.pole ? 0.f : -50.f;
            t.fastest = t.fastEarly = 0.f;
            int sounds = 0;
            (void)physsound::ropeSoundInfo(t.rope, sounds, &t.clinks); // (ropes8b: a chain's clinks from here)
            if(!t.pole && !t.chain)
            {
                t.pitch = 0.f;
            }
            flatTestNext(t, t.pole || t.chain ? 4 : 30);
        }
        else if(t.frames > 72 * 4)
        {
            flatTestEnd(t, true, "walking into the rope (flat-screen play, -flatscreenmode?)");
        }
        return;
    case 30: // (ropes9) a rope looking level, forward held 3 s: it pumps the swing along the view, no climb
    {
        int ent = 0;
        glm::vec3 v{0.f};
        if(climbing && climbrope::riding(client, ent, v))
        {
            // (Its swing along the view: the walk in's in the first half second, the pump's in the last second.)
            const float along = za::abs(v.x);
            if(t.frames <= 36)
            {
                t.fastEarly = za::max(t.fastEarly, along);
            }
            else
            {
                t.fastest = za::max(t.fastest, along);
            }
        }
        // (Pumped to near vr_rope_swing_cap, past the walk in's swing, which only dies down unpumped.)
        const float want = za::max(0.85f * za::max(vr_rope_swing_cap.value, 0.f), t.fastEarly + 40.f);
        if(!climbing || (t.frames > 36 && t.fastest >= want) || t.frames > 72 * 8)
        {
            const bool ok = climbing && za::abs(feet - t.feet0) < 8.f && t.fastest >= want;
            const char* what = va("level forward pumps: feet %.1f->%.1f, swing %.0f u/s in %.1f s (walk-in %.0f)", t.feet0, feet,
                t.fastest, t.frames / 72.f, t.fastEarly);
            t.tally.check(ok, flatTestName, what);
            if(ok)
            {
                Con_Printf("%s: PASS %s\n", flatTestName, what);
            }
            t.feet0 = feet;
            t.serials = 0;
            t.lastSerial = -1;
            t.handBits = 0;
            t.pitch = -50.f; // (looking up: forward climbs)
            flatTestNext(t, 4);
        }
        return;
    }
    case 4: // climbing up
        if(!climbing || feet >= t.feet0 + 80.f || t.frames > 72 * 6)
        {
            const char* how = t.pitch >= 0.f ? "looking level (a pole: only climbed)" : "looking up";
            const bool ok = climbing && feet >= t.feet0 + 80.f && t.serials >= 4 && t.handBits == 3;
            t.tally.check(ok, flatTestName, va("climbed up %s: the feet from %.1f to %.1f, %d hand changes, hands drawn off %d main %d",
                                              how, t.feet0, feet, t.serials, t.handBits & 1, (t.handBits >> 1) & 1));
            if(ok)
            {
                Con_Printf("%s: PASS climbed up %s: the feet from %.1f to %.1f, %d hand changes, both hands drawn\n", flatTestName,
                    how, t.feet0, feet, t.serials);
            }
            if(t.chain)
            {
                // (ropes8b) Climbed, the chain clinks (the hands taking it, pulling him up it, knock its links).
                int sounds = 0, clinks = 0;
                (void)physsound::ropeSoundInfo(t.rope, sounds, &clinks);
                const char* what = va("the chain clinks as he climbs it: %d clinks (at least 1)", clinks - t.clinks);
                t.tally.check(clinks - t.clinks >= 1, flatTestName, what);
                if(clinks - t.clinks >= 1)
                {
                    Con_Printf("%s: PASS %s\n", flatTestName, what);
                }
            }
            Cbuf_AddText("-forward\n"); // (ropes8a: hanging still, the hands looked at; ropes8b: a rope's too, the holsters)
            flatTestNext(t, 40);
        }
        return;
    case 40: // (ropes8a, his note 2026-11-23) hanging still on the pole (half a second): both hands round it, not through it
        if(t.frames == 12 && t.shots)
        {
            Cbuf_AddText("vr_flat_orbit 150 80 -5\nwait\nwait\nscreenshot\nvr_flat_orbit 80 70 -5\nwait\nwait\nscreenshot\nvr_flat_orbit\n");
        }
        if(t.frames == 36)
        {
            // (ropes8b) The hip holsters stay on the hips as the thighs wrap the rope: as far from the hip joints as
            // standing (within 3 cm), the guns' way up within 25 degrees of standing's off the trunk.
            float off = 0.f, tilt = 0.f;
            const bool posed = holstersOnHips(off, tilt);
            const bool on = posed && off <= t.holstersStanding[0] + 3.f && tilt <= t.holstersStanding[1] + 25.f;
            const char* what = va("the hip holsters on the hips hanging on it: %.1f cm from the hip joints (standing %.1f), the "
                                  "guns %.0f degrees off the trunk (standing %.0f)",
                posed ? off : -1.f, t.holstersStanding[0], tilt, t.holstersStanding[1]);
            t.tally.check(on, flatTestName, what);
            if(on)
            {
                Con_Printf("%s: PASS %s\n", flatTestName, what);
            }
        }
        if(t.chain)
        {
            // (ropes8b, his flat note 2026-11-23) Held still on a chain (2 s, from half a second on): no clink.
            int sounds = 0, clinks = 0;
            (void)physsound::ropeSoundInfo(t.rope, sounds, &clinks);
            if(t.frames == 36)
            {
                t.clinks = clinks;
            }
            else if(t.frames == 36 + 144)
            {
                const bool quiet = climbing && clinks == t.clinks;
                const char* what = va("held still on the chain for 2 s: %d clinks (none)", clinks - t.clinks);
                t.tally.check(quiet, flatTestName, what);
                if(quiet)
                {
                    Con_Printf("%s: PASS %s\n", flatTestName, what);
                }
                flatTestEnd(t, false, "");
            }
            return;
        }
        if(t.frames == 36 && t.pole)
        {
            int held = 0;
            const char* part = "-";
            float gap = 0.f;
            const float sink = handsSink(held, part, gap);
            const bool round = held == 2 && sink <= 0.5f && gap <= 0.5f; // (flat play's fingertips 0.44 in: ropes8a)
            t.tally.check(round, flatTestName, va("the hands round the pole: %d drawn on it, the deepest %.2f in (%s), %.2f off at the nearest",
                                                held, sink, part, za::max(gap, 0.f)));
            if(round)
            {
                Con_Printf("%s: PASS the hands round the pole: both on it, the deepest %.2f units in (%s), %.2f off at the nearest\n",
                    flatTestName, sink, part, za::max(gap, 0.f));
            }
        }
        else if(t.frames >= 60)
        {
            t.pitch = t.pole ? -50.f : 50.f; // (back held: down, looking up or (ropes9) down past vr_flat_rope_climb_pitch)
            Cbuf_AddText("+back\n");
            t.serials = 0;
            t.lastSerial = -1;
            flatTestNext(t, 5);
        }
        return;
    case 5: // climbing down to the floor
        if(!climbing && ground)
        {
            const bool ok = za::abs(feet) < 2.f && t.serials >= 2;
            const char* look = t.pitch < 0.f ? "up" : "down";
            t.tally.check(ok, flatTestName, va("climbed down looking %s (back held): off it standing, the feet at %.1f, %d hand changes", look, feet, t.serials));
            if(ok)
            {
                Con_Printf("%s: PASS climbed down looking %s (back held): off it standing, the feet at %.1f, %d hand changes\n", flatTestName, look,
                    feet, t.serials);
            }
            Cbuf_AddText("-back\n");
            t.pitch = 0.f;
            if(t.pole)
            {
                // (ropes7) Climbed up and down it: the pole stood where it was made.
                const Rope* r = ropeOf(t.rope);
                const float moved = r && r->params.pole && r->p.size() == t.made.size() ? displacement(*r, t.made) : 1e9f;
                t.tally.check(moved < 0.01f, flatTestName, va("the pole never moved (%.3f units)", moved));
                if(moved < 0.01f)
                {
                    Con_Printf("%s: PASS the pole never moved (%.3f units)\n", flatTestName, moved);
                }
                flatTestEnd(t, false, "");
                return;
            }
            climbrope::Grab g;
            t.first = climbrope::findGrab(flatTestRow[0] + glm::vec3{0.f, 0.f, 300.f}, 8.f, g) ? g.ent : 0;
            t.second = climbrope::findGrab(flatTestRow[1] + glm::vec3{0.f, 0.f, 300.f}, 8.f, g) ? g.ent : 0;
            flatTestNext(t, 6);
        }
        else if(t.frames > 72 * 10)
        {
            flatTestEnd(t, true, "climbing down");
        }
        return;
    case 6: // rope to rope: set in the air by the row's first rope, facing the second
        if(t.frames < 36)
        {
            return;
        }
        if(!t.first || !t.second)
        {
            t.tally.check(false, flatTestName, "the map's swinging row (vrropes)");
            flatTestEnd(t, false, "");
            return;
        }
        t.ropesBefore = taken;
        flatTestPlace(player, flatTestRow[0] + glm::vec3{0.f, -12.f, 170.f}, 90.f); // (ropes8a: facing across the row, the strafe keys pump along it)
        flatTestNext(t, 7);
        return;
    case 7: // taken in the air
    {
        int ent = 0;
        glm::vec3 v{0.f};
        if(climbing && climbrope::riding(client, ent, v) && ent == t.first)
        {
            t.tally.check(true, flatTestName, "set in the air by a rope: taken");
            Con_Printf("%s: PASS set in the air by a rope: taken (rope %d, the hands at %.1f)\n", flatTestName, ent, player->v.origin[2]);
            t.fastest = 0.f;
            t.lastVx = 0.f;
            Cbuf_AddText("+moveright\n"); // (ropes8a: the strafe key pumps, facing across the row: along it)
            flatTestNext(t, 8);
        }
        else if(t.frames > 72 * 2)
        {
            flatTestEnd(t, true, "taking the first rope in the air");
        }
        return;
    }
    case 8: // pumping towards the second, until near the cap; then let go at the forward peak
    {
        int ent = 0;
        glm::vec3 v{0.f};
        if(!climbing || !climbrope::riding(client, ent, v))
        {
            t.tally.check(false, flatTestName, va("pumping: fell off the rope (feet at %.1f)", feet));
            flatTestEnd(t, false, "");
            return;
        }
        t.fastest = za::max(t.fastest, glm::length(glm::vec2{v.x, v.y}));
        // (ropes8a: let go flying out past the next rope's reach line, still swinging its way: caught as it is passed)
        const bool peak = v.x > 0.f && player->v.origin[0] >= flatTestRow[1].x - 40.f;
        t.lastVx = v.x;
        if(peak && t.fastest >= 0.85f * za::max(vr_rope_swing_cap.value, 0.f))
        {
            t.tally.check(true, flatTestName, "pumped the swing");
            Con_Printf("%s: PASS pumped the swing to %.0f units/s at the hands (cap %.0f) in %.1f s; let go flying past the next, %.0f on\n",
                flatTestName, t.fastest, vr_rope_swing_cap.value, t.frames / 72.f, player->v.origin[0] - flatTestRow[0].x);
            Cbuf_AddText("-moveright\n+jump\n");
            flatTestNext(t, 9);
        }
        else if(t.frames > 72 * 40)
        {
            flatTestEnd(t, true, va("pumping (fastest %.0f units/s)", t.fastest));
        }
        return;
    }
    case 9: // in the air: the second caught
    {
        if(t.frames == 6)
        {
            Cbuf_AddText("-jump\n");
        }
        int ent = 0;
        glm::vec3 v{0.f};
        if(climbing && climbrope::riding(client, ent, v))
        {
            const bool ok = ent == t.second;
            t.tally.check(ok, flatTestName, va("rope to rope: caught rope %d (the next %d) after %.2f s, feet at %.1f", ent, t.second,
                                              t.frames / 72.f, feet));
            if(ok)
            {
                Con_Printf("%s: PASS rope to rope: caught the next rope after %.2f s, feet at %.1f\n", flatTestName, t.frames / 72.f, feet);
            }
            flatTestEnd(t, false, "");
        }
        else if(ground || t.frames > 72 * 3)
        {
            t.tally.check(false, flatTestName, va("rope to rope: not caught (%s, at %.1f %.1f %.1f)", ground ? "landed" : "3 s",
                                               player->v.origin[0], player->v.origin[1], player->v.origin[2]));
            flatTestEnd(t, false, "");
        }
        return;
    }
    // (ropes7) `vr_rope_flat_test strung`: a rope strung level overhead along his way (1% slack), jumped up to (taken,
    // a monkey traverse, sagging under him), along it with forward, back with back, dropped from with the move-down key.
    case 20:
    {
        const float high = t.pole ? flatPoleHeight : flatStrungHeight;
        const glm::vec3 a = flatTestRope + glm::vec3{-200.f, 0.f, high}, b = flatTestRope + glm::vec3{200.f, 0.f, high};
        edict_t* e = box3d::spawnClass(t.pole ? "vr_pole" : "vr_rope", a, glm::vec3{0.f});
        if(!e)
        {
            t.tally.check(false, flatTestName, "the rope made");
            flatTestEnd(t, false, "");
            return;
        }
        stringBetween(e, a, b, t.pole ? 0.f : 0.01f);
        if(progs::fields().rope_damping >= 0)
        {
            progs::fieldFloat(e, progs::fields().rope_damping) = 4.f; // (it settles soon)
        }
        t.rope = NUM_FOR_EDICT(e);
        t.pitch = 0.f;
        flatTestPlace(player, flatTestRope + glm::vec3{-60.f, 0.f, -player->v.mins[2] + 1.f}, 0.f);
        flatTestNext(t, 21);
        return;
    }
    case 21: // settled (3 s, damping 4): jump up to it
        if(t.frames >= 216)
        {
            const Rope* r = ropeOf(t.rope);
            climbrope::Grab g;
            t.feet0 = r && climbrope::findGrab(glm::vec3{player->v.origin[0], flatTestRope.y, t.pole ? flatPoleHeight : flatStrungHeight}, 96.f, g) ? g.at.z : 0.f;
            t.ropesBefore = taken;
            Cbuf_AddText("+jump\n");
            flatTestNext(t, 22);
        }
        return;
    case 22: // taken in the air: a monkey traverse
    {
        const Rope* r = ropeOf(t.rope);
        const bool monkeyHang = r && (t.pole ? r->monkey : r->rider.monkey && r->rider.started); // (a pole: no rider)
        if(taken > t.ropesBefore && climbing && monkeyHang)
        {
            Cbuf_AddText("-jump\n");
            t.lastVx = player->v.origin[0];
            t.serials = 0;
            t.lastSerial = -1;
            t.handBits = 0;
            flatTestNext(t, 23);
        }
        else if(t.frames > 72 * 2)
        {
            flatTestEnd(t, true, "jumping up to the strung rope");
        }
        return;
    }
    case 23: // hanging: sagged under him; then forward along it
        if(t.frames == 24 && t.shots)
        {
            Cbuf_AddText("vr_flat_orbit 150 90 -12\nwait\nwait\nscreenshot\nvr_flat_orbit 80 80 -12\nwait\nwait\nscreenshot\nvr_flat_orbit\n");
        }
        if(t.frames == 60)
        {
            // (ropes8b) The hands under it overhand, the knuckles up, the body's arms reaching them (monkeyHands).
            float up = 0.f, under = 0.f, gap = 0.f;
            const int on = monkeyHands(up, under, gap);
            const bool ok = on == 2 && up >= 0.7f && under > 0.f && gap <= 1.f;
            const char* what = va("hanging: %d hands drawn under it, the knuckles up (least %.2f of 1), the palms %.1f under "
                                  "its axis, the arms reaching them (%.1f cm past, at most 1)",
                on, up, under, gap);
            t.tally.check(ok, flatTestName, what);
            if(ok)
            {
                Con_Printf("%s: PASS %s\n", flatTestName, what);
            }
        }
        if(t.frames == 72)
        {
            climbrope::Grab g;
            const float now = climbrope::findGrab(glm::vec3{player->v.origin[0], flatTestRope.y, t.pole ? flatPoleHeight : flatStrungHeight}, 96.f, g) ? g.at.z : 0.f;
            // (ropes8a: a level pole doesn't sag)
            const bool ok = climbing && (t.pole ? za::abs(now - t.feet0) < 0.5f : now <= t.feet0 - 2.f);
            const char* how = t.pole ? "the pole level" : "sagging under him";
            t.tally.check(ok, flatTestName, va("jumped up to the strung %s: taken, a monkey traverse, %s from %.1f to %.1f",
                                              t.pole ? "pole" : "rope", how, t.feet0, now));
            if(ok)
            {
                Con_Printf("%s: PASS jumped up to the strung %s: taken, a monkey traverse, %s from %.1f to %.1f\n", flatTestName,
                    t.pole ? "pole" : "rope", how, t.feet0, now);
            }
            t.lastVx = player->v.origin[0];
            t.serials = 0;
            t.lastSerial = -1;
            Cbuf_AddText("+forward\n");
            flatTestNext(t, 24);
        }
        return;
    case 24: // along it forward
    case 25: // back
    {
        const float gone = (player->v.origin[0] - t.lastVx) * (t.stage == 24 ? 1.f : -1.f);
        const float want = t.stage == 24 ? 80.f : 40.f;
        if(!climbing || gone >= want || t.frames > 72 * 6)
        {
            const bool ok = climbing && gone >= want && t.serials >= 3;
            const char* way = t.stage == 24 ? "forward" : "back";
            t.tally.check(ok, flatTestName, va("along it %s: %.1f units, %d hand changes", way, gone, t.serials));
            if(ok)
            {
                Con_Printf("%s: PASS along it %s: %.1f units, %d hand changes\n", flatTestName, way, gone, t.serials);
            }
            t.lastVx = player->v.origin[0];
            t.serials = 0;
            t.lastSerial = -1;
            Cbuf_AddText(t.stage == 24 ? "-forward\n+back\n" : "-back\n+movedown\n");
            flatTestNext(t, t.stage + 1);
        }
        return;
    }
    case 26: // the move-down key: dropped
        if(!climbing)
        {
            Cbuf_AddText("-movedown\n");
            t.tally.check(true, flatTestName, "dropped from it with the move-down key");
            Con_Printf("%s: PASS dropped from it with the move-down key (feet at %.1f)\n", flatTestName, feet);
            flatTestEnd(t, false, "");
        }
        else if(t.frames > 72)
        {
            Cbuf_AddText("-movedown\n");
            flatTestEnd(t, true, "dropping with the move-down key");
        }
        return;
    default:
        return;
    }
}

void flatTest_f()
{
    if(!sv.active || !progs::bindings().isVrProgs || progs::fields().rope_length < 0)
    {
        Con_Printf("%s: FAIL usage: vr_rope_flat_test in flat-screen play (-flatscreenmode), map vrropes\n", flatTestName);
        testkit::verdict(flatTestName, 0, 1);
        return;
    }
    flatTest = FlatTest{};
    // (ropes8a) `strungpole`: the strung run on a level pole (no sag).
    const char* mode = Cmd_Argc() > 1 ? Cmd_Argv(1) : "";
    flatTest.wall = !ZA_STRCMP(mode, "wall");
    flatTest.pole = !ZA_STRCMP(mode, "pole") || !ZA_STRCMP(mode, "strungpole") || flatTest.wall;
    if(flatTest.wall)
    {
        flatTest.flatRopeWas = vr_flat_rope.value;
        Cvar_SetValueQuick(&vr_flat_rope, 0.f);
    }
    flatTest.shots = Cmd_Argc() > 2 && !ZA_STRCMP(Cmd_Argv(2), "shots");
    flatTest.chain = !ZA_STRCMP(mode, "chain");
    flatTest.stage = !ZA_STRCMP(mode, "strung") || !ZA_STRCMP(mode, "strungpole") ? 20 : 1;
}

// ---------------------------------------------------------------------------------------------------------------------
// vr_rope_hands_test (ropes5, ROPES.md "Tests"; flat-screen play, -flatscreenmode, in vrropes): the drawn hands on the
// row's first rope, its first chain and the heavy chain, each taken in the air, hanging still (2 s) then pumped (3 s).
// Each drawn frame (clientFrame) each hand drawn on its hold is sampled: the palm's second difference frame to frame
// (smooth motion ~0; a step some frames and not others, the jitter), the arm's (palm less shoulder: the drawn arm's
// vibration), and how far the palm's middle is off the rope as drawn (from its drawn pieces, less its half thickness;
// < 0 in it). Best with the client faster than the server (vr_fixed_frames_rate 144), as a monitor or a headset.
// Counted server frames; the limits a safety net (FAIL timed out).
struct HandStat
{
    int n{0};
    float jitSum{0.f}, jitMax{0.f}; // the palm's second difference (units)
    float armSum{0.f}, armMax{0.f}; // the arm's (palm less shoulder)
    float gapSum{0.f}, gapMax{0.f}; // off the drawn rope (units)
    float shakeMax{0.f};            // the tired arms' shake in the drawn hand (vr_fatigue; units)
};
struct HandSampler
{
    bool on{false};
    int frame{-1};
    HandStat stat;
    glm::vec3 palm[2][2]{}; // each hand's last two frames' palm and arm
    glm::vec3 arm[2][2]{};
    int have[2]{0, 0};
};
HandSampler handSampler;

const za::Vector<glm::vec3>& shownOf(ClientRope& c); // (below: the client's rope as drawn this frame)

struct HandsTest
{
    int stage{0}; // 0 off
    int frames{0};
    int target{0};
    testkit::Tally tally;
    float pitch{0.f};
};
HandsTest handsTest;
constexpr const char* handsTestName = "vr_rope_hands_test";
// (vrropes: the row's first rope, its first chain (1.5 thick), the heavy chain (3 thick, 40 kg); where he is set by each)
constexpr glm::vec3 handsTestAt[3] = {{-300.f, 0.f, 170.f}, {-108.f, 0.f, 170.f}, {-12.f, 400.f, 380.f}};
constexpr const char* handsTestWhat[3] = {"rope", "chain", "heavy chain"};
constexpr float handsJitterMost = 1.f;  // units: a palm's or arm's second difference a frame, at most (smooth: well under)
constexpr float handsGapMost = 1.f;     // units: the palm's middle off the drawn rope's surface at most (in it is fine)
// Swinging, pumped: the means (were 1.9 and 5.5 at 144 Hz; now about 0.06 and 0.2); a rare hand change on the heavy
// chain while pumped jumps (its maxima reported, not checked: ROPES.md "Known limits").
constexpr float handsSwingJitterMean = 0.3f;
constexpr float handsSwingGapMean = 0.5f;

void handSamplerStart()
{
    handSampler = HandSampler{};
    handSampler.on = true;
}

// Client, each frame drawn (clientFrame): the drawn hands on their holds sampled.
void handSamplerFrame()
{
    HandSampler& s = handSampler;
    if(!s.on || cls.signon != SIGNONS || s.frame == host_framecount)
    {
        return;
    }
    s.frame = host_framecount;
    for(int h = 0; h < 2; h++)
    {
        glm::vec3 palm{0.f};
        avatar::Shoulder sh;
        if(climb::drawnShare(h) < 0.999f || !climb::drawnPalm(h, palm) || !avatar::shoulder(h, sh))
        {
            s.have[h] = 0;
            continue;
        }
        glm::vec3 shakePos{0.f}, shakeAngles{0.f};
        fatigue::shake(h, shakePos, shakeAngles);
        palm += shakePos; // (as drawn: vr_view_hands.cpp's placeController)
        s.stat.shakeMax = za::max(s.stat.shakeMax, glm::length(shakePos));
        const glm::vec3 arm = palm - sh.joint;
        if(s.have[h] >= 2)
        {
            const float jit = glm::length(palm - 2.f * s.palm[h][0] + s.palm[h][1]);
            const float armJit = glm::length(arm - 2.f * s.arm[h][0] + s.arm[h][1]);
            float thick = 1.5f;
            float best = 1e9f;
            for(auto& kv : clientRopes)
            {
                const za::Vector<glm::vec3>& q = shownOf(kv.second);
                for(za::SizeT j = 0; j + 1 < q.size(); j++)
                {
                    float ta = 0.f, tb = 0.f;
                    if(const float d = geom::segmentsDistance(palm, palm, q[j], q[j + 1], ta, tb); d < best)
                    {
                        best = d;
                        thick = kv.second.thickness;
                    }
                }
            }
            const float gap = best - 0.5f * thick;
            s.stat.n++;
            s.stat.jitSum += jit;
            s.stat.jitMax = za::max(s.stat.jitMax, jit);
            s.stat.armSum += armJit;
            s.stat.armMax = za::max(s.stat.armMax, armJit);
            s.stat.gapSum += gap;
            s.stat.gapMax = za::max(s.stat.gapMax, gap);
        }
        s.palm[h][1] = s.palm[h][0];
        s.palm[h][0] = palm;
        s.arm[h][1] = s.arm[h][0];
        s.arm[h][0] = arm;
        s.have[h] = za::min(s.have[h] + 1, 2);
    }
}

// The sampler's numbers since handSamplerStart checked: `what` (rope, chain), `how` (hanging, swinging).
void handsTestReport(HandsTest& t, const char* what, const char* how, bool swinging)
{
    const HandStat& st = handSampler.stat;
    handSampler.on = false;
    const float n = static_cast<float>(za::max(st.n, 1));
    const bool ok = st.n >= 72 && (swinging ? st.jitSum / n <= handsSwingJitterMean && st.armSum / n <= handsSwingJitterMean &&
                                                  st.gapSum / n <= handsSwingGapMean
                                            : st.jitMax <= handsJitterMost && st.armMax <= handsJitterMost && st.gapMax <= handsGapMost);
    const char* line = va("%s %s: %d samples; palm jitter mean %.3f max %.3f, arm jitter mean %.3f max %.3f, off the drawn %s "
                          "mean %.2f max %.2f (shake at most %.2f)",
        what, how, st.n, st.jitSum / n, st.jitMax, st.armSum / n, st.armMax, what, st.gapSum / n, st.gapMax, st.shakeMax);
    t.tally.check(ok, handsTestName, line);
    if(ok)
    {
        Con_Printf("%s: PASS %s\n", handsTestName, line);
    }
}

void handsTestEnd(HandsTest& t, bool timedOut, const char* what)
{
    if(timedOut)
    {
        t.tally.check(false, handsTestName, va("timed out: %s", what));
    }
    Cbuf_AddText("-forward\n-moveright\n-jump\n");
    handSampler.on = false;
    t.tally.verdict(handsTestName);
    t.stage = 0;
}

void handsTestFrame()
{
    HandsTest& t = handsTest;
    t.frames++;
    edict_t* player = firstPlayer();
    if(!player)
    {
        return;
    }
    const int client = NUM_FOR_EDICT(player) - 1;
    cl.viewangles[PITCH] = t.pitch;
    cl.viewangles[YAW] = 0.f;
    bool climbing = false;
    (void)climb::flatRopes(player, climbing);
    int ent = 0;
    glm::vec3 v{0.f};
    const bool riding = climbing && climbrope::riding(client, ent, v);
    const char* what = handsTestWhat[t.target];
    switch(t.stage)
    {
    case 1: // off any rope: set by the next one in the air
        if(t.frames < 36 || (climbing && t.frames < 72 * 3))
        {
            return;
        }
        flatTestPlace(player, handsTestAt[t.target], 0.f);
        t.stage = 2;
        t.frames = 0;
        return;
    case 2: // taken
        if(riding)
        {
            t.stage = 3;
            t.frames = 0;
        }
        else if(t.frames > 72 * 2)
        {
            handsTestEnd(t, true, va("taking the %s in the air", what));
        }
        return;
    case 3: // eased onto it (the hands' pins in, the take's swing damped a little): sampled hanging
        t.pitch = -40.f; // (looking up at the hands, no key held: no climb)
        if(t.frames == 36)
        {
            handSamplerStart();
        }
        else if(t.frames == 108)
        {
            Cbuf_AddText("screenshot\n"); // (the grip seen: the kit's -Out composes it)
        }
        else if(t.frames >= 36 + 144)
        {
            t.pitch = 0.f; // (level: the move keys pump)
            handsTestReport(t, what, "hanging", false);
            handSamplerStart();
            Cbuf_AddText("+moveright\n"); // (ropes8a: the strafe key pumps)
            t.stage = 4;
            t.frames = 0;
        }
        return;
    case 4: // pumped
        if(!riding)
        {
            t.tally.check(false, handsTestName, va("%s swinging: fell off", what));
            handsTestEnd(t, false, "");
        }
        else if(t.frames >= 216)
        {
            handsTestReport(t, what, va("swinging (%.0f units/s at the hands)", glm::length(glm::vec2{v.x, v.y})), true);
            Cbuf_AddText("-moveright\n+jump\n");
            t.stage = 5;
            t.frames = 0;
        }
        return;
    case 5: // let go
        if(t.frames == 6)
        {
            Cbuf_AddText("-jump\n");
        }
        else if(t.frames >= 12)
        {
            if(++t.target >= 3)
            {
                handsTestEnd(t, false, "");
                return;
            }
            t.stage = 1;
            t.frames = 0;
        }
        return;
    default:
        return;
    }
}

void handsTest_f()
{
    if(!sv.active || !progs::bindings().isVrProgs || progs::fields().rope_length < 0)
    {
        Con_Printf("%s: FAIL usage: vr_rope_hands_test in flat-screen play (-flatscreenmode), map vrropes\n", handsTestName);
        testkit::verdict(handsTestName, 0, 1);
        return;
    }
    handsTest = HandsTest{};
    handsTest.stage = 1;
}

// ---------------------------------------------------------------------------------------------------------------------
// vr_rope_pause_test (ropes5, ROPES.md "Tests"; flat-screen play, -flatscreenmode, in vrropes, single player): the map's
// ropes pushed swinging; paused (`pause`, F8's), their points (the server's and as the client draws them) stand still
// over a second; unpaused, they swing on; the menu open (single player's pause), still again. Counted drawn frames
// (the server's stand still while paused).
struct PauseTest
{
    int stage{0}; // 0 off
    int frames{0};
    testkit::Tally tally;
    za::Vector<glm::vec3> mark; // the points at the mark: the server's, then the client's as drawn
};
PauseTest pauseTest;
constexpr const char* pauseTestName = "vr_rope_pause_test";

void pauseTestPoints(za::Vector<glm::vec3>& out)
{
    out.clear();
    for(auto& kv : ropes)
    {
        for(const glm::vec3& p : kv.second.p)
        {
            out.pushBack(p);
        }
    }
    for(auto& kv : clientRopes)
    {
        for(const glm::vec3& p : shownOf(kv.second))
        {
            out.pushBack(p);
        }
    }
}

// The most any point moved since the mark (1e9: the ropes changed).
float pauseTestMoved(const PauseTest& t)
{
    za::Vector<glm::vec3> now;
    pauseTestPoints(now);
    if(now.size() != t.mark.size() || now.empty())
    {
        return 1e9f;
    }
    float most = 0.f;
    for(za::SizeT i = 0; i < now.size(); i++)
    {
        most = za::max(most, glm::distance(now[i], t.mark[i]));
    }
    return most;
}

void pauseTestEnd(PauseTest& t)
{
    t.tally.verdict(pauseTestName);
    t.stage = 0;
}

// Client, each frame drawn.
void pauseTestFrame()
{
    PauseTest& t = pauseTest;
    if(cls.signon != SIGNONS)
    {
        return;
    }
    t.frames++;
    const auto next = [](int stage) {
        pauseTest.stage = stage;
        pauseTest.frames = 0;
    };
    switch(t.stage)
    {
    case 1: // pushed: swinging
        if(t.frames == 1)
        {
            Cbuf_AddText("vr_rope_push 0 400 0\n");
        }
        else if(t.frames >= 72)
        {
            Cbuf_AddText("pause\n");
            next(2);
        }
        return;
    case 2: // paused: still
    case 4: // the menu open: still
        if(t.frames == 12)
        {
            pauseTestPoints(t.mark);
        }
        else if(t.frames >= 12 + 72)
        {
            const float moved = pauseTestMoved(t);
            const bool ok = moved <= 0.01f;
            const char* how = t.stage == 2 ? "paused (pause, F8)" : "the menu open";
            t.tally.check(ok, pauseTestName, va("%s: the ropes' points moved at most %.3f units in a second (%d points)", how, moved,
                                                 static_cast<int>(t.mark.size())));
            if(ok)
            {
                Con_Printf("%s: PASS %s: the ropes' points moved at most %.3f units in a second\n", pauseTestName, how, moved);
            }
            pauseTestPoints(t.mark);
            Cbuf_AddText(t.stage == 2 ? "pause\n" : "togglemenu\n");
            next(t.stage + 1);
        }
        return;
    case 3: // unpaused: swinging on
    case 5: // the menu closed
        if(t.frames >= 36)
        {
            const float moved = pauseTestMoved(t);
            const bool ok = moved >= 0.5f && moved < 1e8f;
            t.tally.check(ok, pauseTestName, va("%s: swinging on (moved %.2f units in half a second)",
                                                 t.stage == 3 ? "unpaused" : "the menu closed", moved));
            if(t.stage == 5)
            {
                pauseTestEnd(t);
                return;
            }
            Cbuf_AddText("togglemenu\n");
            next(4);
        }
        return;
    default:
        return;
    }
}

void pauseTest_f()
{
    if(!sv.active || svs.maxclients > 1 || ropes.empty())
    {
        Con_Printf("%s: FAIL usage: vr_rope_pause_test in single player, map vrropes\n", pauseTestName);
        testkit::verdict(pauseTestName, 0, 1);
        return;
    }
    pauseTest = PauseTest{};
    pauseTest.stage = 1;
}

// ---------------------------------------------------------------------------------------------------------------------
// vr_rope_sound_test (ropes5, ROPES.md "Tests"; single player, best in vrropes): a free rope and a free chain made ahead,
// settled; still, both quiet; a headset's grip on each (the rope silent, the chain's metal grip), a flat-screen one (the
// rope's slap, the chain's metal grip; a second at once on the chain held back); both pushed swinging, the chain's links
// clink (rate-limited), the rope stays quiet. Counted server frames; the sounds as physsound sent them (ropeSoundInfo).
struct SoundTest
{
    int stage{0}; // 0 off
    int frames{0};
    testkit::Tally tally;
    int rope{0}, chain{0};
    int ropeCount{0}, chainCount{0}; // their sounds at the mark
};
SoundTest soundTest;
constexpr const char* soundTestName = "vr_rope_sound_test";

void soundTestFrame()
{
    SoundTest& t = soundTest;
    t.frames++;
    edict_t* player = firstPlayer();
    const auto next = [](int stage) {
        soundTest.stage = stage;
        soundTest.frames = 0;
    };
    const auto check = [](bool ok, const char* what) {
        soundTest.tally.check(ok, soundTestName, what);
        if(ok)
        {
            Con_Printf("%s: PASS %s\n", soundTestName, what);
        }
    };
    Rope* r = ropeOf(t.rope);
    Rope* c = ropeOf(t.chain);
    int rn = 0, cn = 0;
    const char* rLast = physsound::ropeSoundInfo(t.rope, rn);
    const char* cLast = physsound::ropeSoundInfo(t.chain, cn);
    const auto mark = [rn, cn]() {
        soundTest.ropeCount = rn;
        soundTest.chainCount = cn;
    };
    const auto mid = [](const Rope* x) { return x->p[x->p.size() / 2]; };
    switch(t.stage)
    {
    case 1:
    {
        const float yaw = glm::radians(player->v.angles[1]);
        const glm::vec3 fwd{za::cos(yaw), za::sin(yaw), 0.f};
        const glm::vec3 side{-fwd.y, fwd.x, 0.f};
        const glm::vec3 feet = vecOf(player->v.origin) + glm::vec3{0.f, 0.f, player->v.mins[2]};
        edict_t* er = box3d::spawnClass("vr_rope", feet + fwd * 128.f + side * 48.f, glm::vec3{0.f});
        edict_t* ec = box3d::spawnClass("vr_chain", feet + fwd * 128.f - side * 48.f, glm::vec3{0.f});
        if(!er || !ec)
        {
            check(false, "the test's rope and chain spawned");
            next(99);
            break;
        }
        placeWith(er, feet + fwd * 128.f + side * 48.f, 150.f, 0.f, 8, 0.f, 0.f, 0.f, false);
        placeWith(ec, feet + fwd * 128.f - side * 48.f, 150.f, 0.f, 8, 0.f, 0.f, 0.f, false);
        t.rope = NUM_FOR_EDICT(er);
        t.chain = NUM_FOR_EDICT(ec);
        next(2);
        break;
    }
    case 2: // settling
        if(t.frames >= 144)
        {
            mark();
            next(3);
        }
        break;
    case 3: // still for a second: quiet; then a headset's grips
        if(t.frames >= 72 && r && c)
        {
            check(rn == t.ropeCount && cn == t.chainCount,
                va("still for a second, both quiet (rope %d, chain %d sounds; fastest %.1f and %.1f u/s)", rn - t.ropeCount,
                    cn - t.chainCount, r->speed, c->speed));
            mark();
            qvr::climbrope::grabbed(player, t.rope, mid(r));
            qvr::climbrope::grabbed(player, t.chain, mid(c));
            next(4);
        }
        else if(t.frames >= 72)
        {
            check(false, "the rope and the chain made");
            next(99);
        }
        break;
    case 4:
        check(rn == t.ropeCount + 1 && ZA_STRSTR(rLast, "soft_m") && cn == t.chainCount + 1 && ZA_STRSTR(cLast, "grab_metal"),
            va("a grip: the rope's soft thud (%d: %s), the chain's metal grip (%d: %s)", rn - t.ropeCount, rLast,
                cn - t.chainCount, cLast));
        mark();
        next(5);
        break;
    case 5: // a grip on each again (after the grips' gap), and at once another on the chain
        if(t.frames >= 18)
        {
            qvr::climbrope::grabbed(player, t.rope, mid(r));
            qvr::climbrope::grabbed(player, t.chain, mid(c));
            qvr::climbrope::grabbed(player, t.chain, mid(c));
            next(6);
        }
        break;
    case 6:
        check(rn == t.ropeCount + 1 && ZA_STRSTR(rLast, "soft_m"),
            va("a grip on the rope again: its soft thud (%d: %s)", rn - t.ropeCount, rLast));
        check(cn == t.chainCount + 1 && ZA_STRSTR(cLast, "grab_metal"),
            va("on the chain: its metal grip, the second at once held back (%d: %s)", cn - t.chainCount, cLast));
        mark();
        next(7);
        break;
    case 7: // swinging for two seconds
        if(t.frames == 18)
        {
            // Both pushed sideways (as a shove: the speed added to every point but the top), after the grips' gap
            // (ropes8b: a sound a rope within 0.15 s of its last is held back).
            for(Rope* x : {r, c})
            {
                for(za::SizeT i = 1; i < x->p.size(); i++)
                {
                    x->kick[i] += glm::vec3{180.f, 180.f, 0.f} * (static_cast<float>(i) / static_cast<float>(x->p.size() - 1));
                }
            }
        }
        if(t.frames >= 18 + 144)
        {
            // (ropes8b, his note: a chain clinks as it is knocked, not on and on as it swings) The shove knocks it.
            const int clinks = cn - t.chainCount;
            check(clinks >= 1 && clinks <= 9 && ZA_STRSTR(cLast, "metal_l"),
                va("shoved, the chain's links clink: %d in 2 s (1 to 9: as knocked, rate-limited), %s", clinks, cLast));
            check(rn == t.ropeCount, va("swung, the rope stays quiet (%d)", rn - t.ropeCount));
            next(99);
        }
        break;
    default:
        for(const int e : {t.rope, t.chain})
        {
            if(e > 0 && !EDICT_NUM(e)->free)
            {
                ED_Free(EDICT_NUM(e));
            }
        }
        t.tally.verdict(soundTestName);
        t.stage = 0;
        break;
    }
}

void soundTest_f()
{
    if(!sv.active || svs.maxclients > 1 || !progs::bindings().isVrProgs || progs::fields().rope_length < 0 || !firstPlayer())
    {
        Con_Printf("%s: FAIL usage: vr_rope_sound_test in single player with Quake VR's progs\n", soundTestName);
        testkit::verdict(soundTestName, 0, 1);
        return;
    }
    soundTest = SoundTest{};
    soundTest.stage = 1;
}

// ---------------------------------------------------------------------------------------------------------------------
// vr_pole_solid_test (ropes8a, ROPES.md "Tests"; in vrropes: its four poles): each pole's boxes made, on it (each box's
// middle within half a unit of its surface), the slanted one in several; a player's box swept across each pole is
// stopped by them, and a shot's line meets them, their material (QC VR_MaterialOf) the pole's: metal 3 (sparks), wood 4
// (splinters); walking under the level pole meets nothing; then (counted frames: the navmesh's rebuilds done) the
// navmesh's obstacles count every box. (ropes8c) The chains: chainChecks, and a nail fired at one. 12 checks.
struct SolidTest
{
    int stage{0};
    int frames{0};
    int boxes{0};
    int nail{0};                // (ropes8c) the nail fired at a chain (its edict), until it is gone
    glm::vec3 nailAt{0.f};      // where it was last
    glm::vec3 chainAt{0.f};     // the chain's middle it was fired at
    glm::vec3 nailDir{0.f};     // its way (level)
    float metalHits{0.f};       // QC vr_metal_hits before it
    testkit::Tally tally;
};
SolidTest solidTest;
constexpr const char* solidTestName = "vr_pole_solid_test";
constexpr int solidTestGiveUp = 72 * 40;

void solidCheck(bool ok, const char* what)
{
    solidTest.tally.check(ok, solidTestName, what);
    if(ok)
    {
        Con_Printf("%s: PASS %s\n", solidTestName, what);
    }
}

[[nodiscard]] bool isPoleSolid(int num)
{
    glm::vec3 lo, hi;
    return num > 0 && climbrope::solidBox(num, lo, hi);
}

// A player's box (or a point: a shot) swept from `a` to `b` (MOVE_NORMAL, no pass entity): the entity it met (0 nothing
// or the world).
[[nodiscard]] int sweepHits(const glm::vec3& a, const glm::vec3& b, bool point)
{
    float s[3], mn[3], mx[3], e[3];
    for(int c = 0; c < 3; c++)
    {
        s[c] = a[c];
        e[c] = b[c];
        mn[c] = point ? 0.f : c < 2 ? -16.f : -24.f;
        mx[c] = point ? 0.f : c < 2 ? 16.f : 32.f;
    }
    const trace_t tr = SV_Move(s, mn, mx, e, MOVE_NORMAL, nullptr);
    return tr.fraction < 1.f && tr.ent && tr.ent != sv.qcvm.edicts ? NUM_FOR_EDICT(tr.ent) : 0;
}

// QC VR_MaterialOf(e) (-1: no such function).
[[nodiscard]] float materialOfQc(edict_t* e)
{
    const func_t fn = progs::findFunction("VR_MaterialOf");
    if(!fn)
    {
        return -1.f;
    }
    G_INT(OFS_PARM0) = EDICT_TO_PROG(e);
    PR_ExecuteProgram(fn);
    return G_FLOAT(OFS_RETURN);
}

// A trace of `type` (SV_Move's, with its QVR flags) from `a` to `b`, a point, or a player's box: the entity it met (0
// nothing or the world) and where.
[[nodiscard]] int traceHits(const glm::vec3& a, const glm::vec3& b, int type, bool box, glm::vec3* at = nullptr)
{
    float s[3], mn[3], mx[3], e[3];
    for(int c = 0; c < 3; c++)
    {
        s[c] = a[c];
        e[c] = b[c];
        mn[c] = !box ? 0.f : c < 2 ? -16.f : -24.f;
        mx[c] = !box ? 0.f : c < 2 ? 16.f : 32.f;
    }
    const trace_t tr = SV_Move(s, mn, mx, e, type, nullptr);
    if(at)
    {
        *at = vecOf(tr.endpos);
    }
    return tr.fraction < 1.f && tr.ent && tr.ent != sv.qcvm.edicts ? NUM_FOR_EDICT(tr.ent) : 0;
}

// (ropes8c) The chains: a gun's shot, a melee blow's line and a missile's across each one's middle (level, from 40 to
// one side to 40 to the other) meet it, within a unit of its links, metal (QC VR_MaterialOf 3), taking damage (so the
// shots' hits are its: sparks); a player's box, a sight line (nomonsters) and a plain point trace pass it (as a rope's);
// then a nail (QC launch_spike, fired by the player) at the first one's middle (the frames after: solidTestFrame).
void chainChecks(SolidTest& t)
{
    int chains = 0, met = 0, near = 0, metal = 0, hurt = 0, passed = 0;
    float worst = 0.f;
    const Rope* first = nullptr;
    for(auto& kv : ropes)
    {
        const Rope& r = kv.second;
        if(!r.params.chain || r.params.pole || r.p.size() < 2)
        {
            continue;
        }
        chains++;
        const glm::vec3 m = r.p[r.p.size() / 2];
        const glm::vec3 d = glm::normalize(r.p[r.p.size() / 2 + 1 < r.p.size() ? r.p.size() / 2 + 1 : r.p.size() / 2 - 1] - m);
        const glm::vec3 side = za::abs(d.z) > 0.99f ? glm::vec3{1.f, 0.f, 0.f} : glm::normalize(glm::cross(d, glm::vec3{0.f, 0.f, 1.f}));
        constexpr int types[3] = {MOVE_NORMAL | MOVE_HITMODEL, MOVE_HITGIBS | MOVE_HITMODEL | (2 << MOVE_HITMODEL_CLASS_SHIFT), MOVE_MISSILE};
        int all = 0;
        for(const int type : types)
        {
            glm::vec3 at{0.f};
            if(traceHits(m - side * 40.f, m + side * 40.f, type, false, &at) == kv.first)
            {
                all++;
                const float off = glm::length(at - m) - r.params.thickness * 0.5f;
                worst = za::max(worst, off);
            }
        }
        met += all == 3 ? 1 : 0;
        edict_t* e = EDICT_NUM(kv.first);
        metal += materialOfQc(e) == 3.f ? 1 : 0;
        hurt += e->v.takedamage > 0.f ? 1 : 0;
        passed += traceHits(m - side * 40.f, m + side * 40.f, MOVE_NORMAL, true) != kv.first &&
                          traceHits(m - side * 40.f, m + side * 40.f, MOVE_NOMONSTERS, false) != kv.first &&
                          traceHits(m - side * 40.f, m + side * 40.f, MOVE_NORMAL, false) != kv.first
                      ? 1
                      : 0;
        if(!first && worldtrace::serverLine(m - side * 60.f, m, nullptr).fraction >= 1.f)
        {
            first = &r;
            t.chainAt = m;
            t.nailDir = side;
        }
    }
    near = worst <= chainHitGrow + 0.1f ? met : 0;
    solidCheck(chains >= 1 && met == chains, va("a shot's, a blow's and a nail's line across each chain meet it (%d of %d)", met, chains));
    solidCheck(near == chains, va("each where its links are (at most %.2f units off them)", worst));
    solidCheck(metal == chains && hurt == chains, va("its material metal (sparks), it takes damage (%d, %d of %d)", metal, hurt, chains));
    solidCheck(passed == chains, va("a player's box, a sight line and a plain trace pass each chain (%d of %d)", passed, chains));
    edict_t* player = firstPlayer();
    const func_t launch = progs::findFunction("launch_spike");
    if(!first || !launch || !player)
    {
        solidCheck(false, "a chain with a clear line to fire a nail at, and QC launch_spike");
        return;
    }
    t.metalHits = PR_ServerGlobalFloat("vr_metal_hits");
    pr_global_struct->self = EDICT_TO_PROG(player);
    const glm::vec3 from = t.chainAt - t.nailDir * 60.f;
    for(int c = 0; c < 3; c++)
    {
        G_FLOAT(OFS_PARM0 + c) = from[c];
        G_FLOAT(OFS_PARM1 + c) = t.nailDir[c];
    }
    PR_ExecuteProgram(launch);
    t.nail = NUM_FOR_EDICT(PROG_TO_EDICT(G_INT(OFS_RETURN)));
    t.nailAt = from;
}

void solidTestFrame()
{
    SolidTest& t = solidTest;
    t.frames++;
    if(t.stage == 1)
    {
        int poles = 0, slanted = 0, stopped = 0, shot = 0, rightStuff = 0, under = -1;
        float worstOff = 0.f;
        for(auto& kv : ropes)
        {
            const Rope& r = kv.second;
            if(!r.params.pole || r.p.size() < 2)
            {
                continue;
            }
            poles++;
            t.boxes += static_cast<int>(r.solids.size());
            const glm::vec3 a = r.p.front(), b = r.p.back(), d = glm::normalize(b - a);
            const bool level = za::abs(d.z) < 0.1f, upright = za::abs(d.z) > 0.99f;
            slanted = !level && !upright ? static_cast<int>(r.solids.size()) : slanted;
            for(const int s : r.solids)
            {
                const edict_t* e = EDICT_NUM(s);
                const glm::vec3 mid = (geom::vec(e->v.absmin) + geom::vec(e->v.absmax)) * 0.5f;
                const glm::vec3 off = (mid - a) - d * glm::dot(mid - a, d);
                worstOff = za::max(worstOff, glm::length(off) - r.params.thickness * 0.5f);
            }
            // Across its middle, level: a player's box and a shot's line from 40 to one side to 40 to the other (the level
            // pole's at his chest).
            const glm::vec3 m = (a + b) * 0.5f;
            const glm::vec3 side = upright ? glm::vec3{1.f, 0.f, 0.f} : glm::normalize(glm::cross(d, glm::vec3{0.f, 0.f, 1.f}));
            const glm::vec3 at = level ? m - glm::vec3{0.f, 0.f, 10.f} : m;
            stopped += isPoleSolid(sweepHits(at - side * 40.f, at + side * 40.f, false)) ? 1 : 0;
            if(const int line = sweepHits(m - side * 40.f, m + side * 40.f, true); isPoleSolid(line))
            {
                shot++;
                rightStuff += materialOfQc(EDICT_NUM(line)) == (r.params.metal ? 3.f : 4.f) ? 1 : 0;
            }
            if(level)
            {
                // Walked under it: his box on the floor under its middle, across.
                const trace_t down = worldtrace::serverLine(m, m - glm::vec3{0.f, 0.f, 1000.f}, nullptr);
                const glm::vec3 feet = vecOf(down.endpos) + glm::vec3{0.f, 0.f, 25.f};
                under = sweepHits(feet - side * 40.f, feet + side * 40.f, false);            }
        }
        solidCheck(poles >= 4 && slanted >= 2, va("%d poles made solid: %d boxes, the slanted one in %d", poles, t.boxes, slanted));
        solidCheck(worstOff <= 0.5f, va("each box's middle on its pole (at most %.2f units off its surface)", worstOff));
        solidCheck(stopped == poles, va("a player's box swept across each pole stopped by it (%d of %d)", stopped, poles));
        solidCheck(shot == poles, va("a shot's line across each pole meets it (%d of %d)", shot, poles));
        solidCheck(rightStuff == poles, va("its material the pole's: metal sparks, wood splinters (%d of %d)", rightStuff, poles));
        solidCheck(under == 0, va("walking under the level pole meets nothing (met %d)", under));
        chainChecks(t);
        t.stage = 2;
        t.frames = 0;
        return;
    }
    // (ropes8c) The nail at the chain: it stops at its links (sparks: a metal hit counted), not beyond.
    if(t.nail > 0)
    {
        edict_t* n = EDICT_NUM(t.nail);
        if(!n->free && ZA_STRCMP(PR_GetString(n->v.classname), "spike") == 0)
        {
            t.nailAt = vecOf(n->v.origin);
            if(t.frames <= solidTestGiveUp)
            {
                return;
            }
        }
        const float sparks = PR_ServerGlobalFloat("vr_metal_hits") - t.metalHits;
        // (Its last place a frame's flight short: the chain the first thing on its way on from there.)
        const float past = glm::dot(t.nailAt - t.chainAt, t.nailDir);
        const int next = traceHits(t.nailAt, t.nailAt + t.nailDir * (glm::length(t.chainAt - t.nailAt) + 8.f), MOVE_MISSILE, false);
        solidCheck(sparks >= 1.f && past < 0.f && ropeOf(next) && ropeOf(next)->params.chain,
            va("a nail fired at a chain stops at it with sparks (%.0f metal hits; last %.1f units short of it, it next)", sparks, -past));
        t.nail = 0;
    }
    // The navmesh: every box an obstacle, its tiles rebuilt.
    const float pending = nav::obstacles(0), marked = nav::obstacles(1);
    if(pending == 0.f && marked >= static_cast<float>(t.boxes) && t.boxes > 0)
    {
        solidCheck(true, va("the navmesh's obstacles: %.0f marked (%d boxes), rebuilt after %.1f s", marked, t.boxes, t.frames / 72.f));
        t.tally.verdict(solidTestName);
        t.stage = 0;
    }
    else if(t.frames > solidTestGiveUp)
    {
        t.tally.check(false, solidTestName, va("timed out: the navmesh's obstacles (%.0f marked of %d boxes, %.0f pending)", marked, t.boxes, pending));
        t.tally.verdict(solidTestName);
        t.stage = 0;
    }
}

void solidTest_f()
{
    if(!sv.active || !progs::bindings().isVrProgs || progs::fields().rope_length < 0)
    {
        Con_Printf("%s: FAIL usage: vr_pole_solid_test in single player, map vrropes\n", solidTestName);
        testkit::verdict(solidTestName, 0, 1);
        return;
    }
    solidTest = SolidTest{};
    solidTest.stage = 1;
}

void serverReset()
{
    ropes.clear();
    solidTest = SolidTest{};
    soundTest = SoundTest{};
    climbTest = ClimbTest{};
    flatTest = FlatTest{};
    handsTest = HandsTest{};
    handSampler = HandSampler{};
    pauseTest = PauseTest{};
    for(auto& client : handsSeen)
    {
        for(HandSeen& h : client)
        {
            h.valid = false;
        }
    }
    test = RopeTest{};
}

void clientReset()
{
    clientRopes.clear();
}

mem::Reset serverWorldReset{"climbrope", mem::ResetEvent::ServerWorld, mem::ResetPhase::State, &serverReset};
mem::Reset clientStateReset{"climbrope", mem::ResetEvent::ClientState, mem::ResetPhase::State, &clientReset};

// The client's rope as drawn this frame (and held, the climbers' feet round it): between the last two messages' points
// as the entities are drawn (CL_LerpPoint's fraction: the server's time between them), so a player hanging on it, drawn
// there too, stays on it as it swings (P3, ROPES.md "Multiplayer"); a rope the latest message didn't move stands as sent.
// Made again whenever the client's time moved (ropes5: it was kept a host frame, and the frame's first ask can come
// before its time moves on (the server's tick), so a client drawing faster than the server drew it a tick at a time:
// the hands on it shook).
const za::Vector<glm::vec3>& shownOf(ClientRope& c)
{
    if(c.shownTime == cl.time)
    {
        return c.shown;
    }
    c.shownTime = cl.time;
    const double span = cl.mtime[0] - cl.mtime[1];
    const bool lerp = c.msgTime == cl.mtime[0] && c.prev.size() == c.p.size() && span > 1e-4 && span < 0.25;
    const float f = lerp ? static_cast<float>(za::clamp((cl.time - cl.mtime[1]) / span, 0.0, 1.0)) : 1.f;
    c.shown.resize(c.p.size());
    for(za::SizeT i = 0; i < c.p.size(); i++)
    {
        c.shown[i] = lerp ? glm::mix(c.prev[i], c.p[i], f) : c.p[i];
    }
    return c.shown;
}

// The client's curve through the points: each piece cut in four along a Catmull-Rom spline.
void smoothInto(const za::Vector<glm::vec3>& p, za::Vector<glm::vec3>& out)
{
    out.clear();
    const za::SizeT n = p.size();
    constexpr int cuts = 4;
    for(za::SizeT j = 0; j + 1 < n; j++)
    {
        const glm::vec3& p0 = p[j > 0 ? j - 1 : j];
        const glm::vec3& p1 = p[j];
        const glm::vec3& p2 = p[j + 1];
        const glm::vec3& p3 = p[j + 2 < n ? j + 2 : j + 1];
        for(int c = 0; c < cuts; c++)
        {
            const float t = static_cast<float>(c) / cuts, t2 = t * t, t3 = t2 * t;
            out.pushBack(0.5f * ((2.f * p1) + (-p0 + p2) * t + (2.f * p0 - 5.f * p1 + 4.f * p2 - p3) * t2 +
                                    (-p0 + 3.f * p1 - 3.f * p2 + p3) * t3));
        }
    }
    if(n)
    {
        out.pushBack(p[n - 1]);
    }
}

[[nodiscard]] coil::Style styleOf(const ClientRope& c)
{
    coil::Style s;
    const float m = c.thickness / units::metresToUnits(); // across, metres
    if(c.chain)
    {
        s.chain = true;
        s.rigid = true;
        s.wireRadius = m * 0.16f;
        s.linkLength = m * 1.3f;
        s.linkWidth = m * 0.55f;
        s.albedo = glm::vec3{0.3f, 0.3f, 0.32f};
    }
    else
    {
        s.wireRadius = m * 0.5f;
        // (ropes7) A pole: dark varnished wood, or bare steel.
        s.albedo = !c.pole ? glm::vec3{0.42f, 0.32f, 0.2f} : c.metal ? glm::vec3{0.52f, 0.53f, 0.56f} : glm::vec3{0.33f, 0.2f, 0.1f};
    }
    return s;
}

} // namespace

namespace qvr::climbrope
{

void init()
{
    Cmd_AddCommand("vr_rope_info", info_f);
    Cmd_AddCommand("vr_rope_test", test_f);
    Cmd_AddCommand("vr_rope_spawn", spawn_f);
    Cmd_AddCommand("vr_rope_push", push_f);
    Cmd_AddCommand("vr_rope_climb_test", climbTest_f);
    Cmd_AddCommand("vr_rope_climb_check", climbCheck_f);
    Cmd_AddCommand("vr_rope_flat_test", flatTest_f);
    Cmd_AddCommand("vr_rope_hands_test", handsTest_f);
    Cmd_AddCommand("vr_rope_pause_test", pauseTest_f);
    Cmd_AddCommand("vr_rope_sound_test", soundTest_f);
    Cmd_AddCommand("vr_pole_solid_test", solidTest_f);
}

void clientFrame()
{
    handSamplerFrame();
    if(pauseTest.stage)
    {
        pauseTestFrame();
    }
}

bool solidBox(int num, glm::vec3& lo, glm::vec3& hi)
{
    const edict_t* s = num > svs.maxclients && num < sv.qcvm.num_edicts ? EDICT_NUM(num) : nullptr;
    if(!s || s->free || s->v.modelindex != 0.f || static_cast<int>(s->v.solid) != SOLID_BBOX ||
        ZA_STRCMP(PR_GetString(s->v.classname), solidClass))
    {
        return false;
    }
    lo = geom::vec(s->v.absmin);
    hi = geom::vec(s->v.absmax);
    return true;
}

bool isRopeClass(const char* classname)
{
    return !ZA_STRCMP(classname, "vr_rope") || !ZA_STRCMP(classname, "vr_chain") || !ZA_STRCMP(classname, "vr_pole");
}

bool monkey(int ent)
{
    const Rope* r = ropeOf(ent);
    return r && (r->monkey || r->rider.monkey);
}

bool pole(int ent)
{
    const Rope* r = ropeOf(ent);
    return r && r->params.pole;
}

physsound::RopeMaterial material(int ent)
{
    const Rope* r = ropeOf(ent);
    using M = physsound::RopeMaterial;
    return !r ? M::Rope : r->params.pole ? (r->params.metal ? M::MetalPole : M::WoodPole) : r->params.chain ? M::Chain : M::Rope;
}

void serverFrame(bool worldRan)
{
    if(!sv.active || sv.paused || !progs::bindings().isVrProgs || progs::fields().rope_length < 0)
    {
        return;
    }
    QVR_PROFILE("climbable ropes");
    const progs::ServerVm vm;
    if(test.stage)
    {
        testFrame();
    }
    if(climbTest.stage)
    {
        climbTestFrame();
    }
    if(flatTest.stage)
    {
        flatTestFrame();
    }
    if(handsTest.stage)
    {
        handsTestFrame();
    }
    if(soundTest.stage && worldRan)
    {
        soundTestFrame();
    }
    if(solidTest.stage && worldRan)
    {
        solidTestFrame();
    }
    // New ropes (a map's, the toolgun's): rope entities with no rope yet.
    for(int num = svs.maxclients + 1; num < sv.qcvm.num_edicts; num++)
    {
        edict_t* e = EDICT_NUM(num);
        if(!e->free && e->v.modelindex == 0.f && static_cast<int>(e->v.solid) == SOLID_NOT && isRopeClass(PR_GetString(e->v.classname)) &&
            ropes.find(num) == ropes.end())
        {
            make(e, num);
        }
    }
    if(ropes.empty())
    {
        return;
    }
    const float dt = worldRan ? za::clamp(static_cast<float>(host_frametime), 0.f, 0.1f) : 0.f; // (paused: they stand, ropes5)
    if(dt <= 0.f)
    {
        return;
    }
    gatherHands(dt);
    for(auto& kv : ropes)
    {
        Rope& r = kv.second;
        // (Its top follows the entity: a rope moved by the toolgun's physgun or a map's mover.)
        r.top = vecOf(EDICT_NUM(kv.first)->v.origin);
        if(r.rider.client >= 0 && r.rider.frame != host_framecount)
        {
            r.rider = Rider{}; // (let go: the rope swings on as he left it)
        }
        r.monkey = monkeyOf(r);
        step(r, dt);
        if(r.params.pole)
        {
            placeSolids(r, kv.first); // (ropes8a: made again where one went; moved with the pole)
            passSolids(r);
        }
        if(r.params.chain)
        {
            physsound::chainMoves(kv.first, r.p[r.p.size() / 2], r.speed, r.jostle); // its links' clink (ropes5, ropes8b)
            if(vr_rope_debug.value >= 2.f && host_framecount % 18 == 0)
            {
                Con_Printf("chainjostle %d: t %.2f speed %.1f jostle %.1f rider %d\n", kv.first, qcvm->time, r.speed, r.jostle, r.rider.client);
            }
        }
        send(kv.first, r);
    }
}

void grabbed(edict_t* player, int ent, const glm::vec3& at)
{
    physsound::ropeGrab(player, at, ent, material(ent));
}

void serverWriteAll(sizebuf_t* msg)
{
    for(auto& kv : ropes)
    {
        if(msg->cursize + writeSize(kv.second) < msg->maxsize - 64)
        {
            write(msg, kv.first, &kv.second);
        }
    }
}

void forget(int num)
{
    if(auto it = ropes.find(num); it != ropes.end() && it->second.params.pole && sv.active)
    {
        freeSolids(it->second, num); // (ropes8a: its boxes go with it)
    }
    if(ropes.erase(num) && sv.active && sv.reliable_datagram.cursize + 16 < sv.reliable_datagram.maxsize)
    {
        write(&sv.reliable_datagram, num, nullptr);
    }
}

void blast(const glm::vec3& at, float damage)
{
    if(damage <= 0.f)
    {
        return;
    }
    const float reach = damage + 40.f; // (T_RadiusDamage's)
    for(auto& kv : ropes)
    {
        Rope& r = kv.second;
        const float scale = za::clamp(5.f / r.params.mass, 0.25f, 2.f);
        for(za::SizeT i = 0; i < r.p.size(); i++)
        {
            const glm::vec3 d = r.p[i] - at;
            const float len = glm::length(d);
            if(len < reach && !r.pinned[i])
            {
                const glm::vec3 dir = len > 1.f ? d / len : glm::vec3{0.f, 0.f, 1.f};
                r.kick[i] += dir * (damage * 4.f * (1.f - len / reach) * scale);
            }
        }
    }
}

void shot(const glm::vec3& start, const glm::vec3& end, const glm::vec3& velocity, float mass)
{
    if(mass <= 0.f)
    {
        return;
    }
    for(auto& kv : ropes)
    {
        Rope& r = kv.second;
        const float pointMass = r.params.mass / static_cast<float>(r.p.size());
        glm::vec3 dv = velocity * (mass / za::max(pointMass, 0.01f));
        if(glm::length(dv) > maxKick)
        {
            dv = glm::normalize(dv) * maxKick;
        }
        for(za::SizeT j = 0; j + 1 < r.p.size(); j++)
        {
            float ta = 0.f, tb = 0.f;
            if(geom::segmentsDistance(start, end, r.p[j], r.p[j + 1], ta, tb) < r.params.thickness * 0.5f + shotReach)
            {
                r.kick[j] += dv * (1.f - tb);
                r.kick[j + 1] += dv * tb;
                break; // (the first piece it meets)
            }
        }
    }
}

int chainClip(const glm::vec3& start, const glm::vec3& end, float radius, const edict_t* shooter, float& fraction,
    glm::vec3& normal)
{
    const glm::vec3 d = end - start;
    const float len = glm::length(d);
    if(len < 1e-3f || ropes.empty())
    {
        return 0;
    }
    const glm::vec3 lo = glm::min(start, end), hi = glm::max(start, end);
    const bool climbing = shooter && climb::ropeHands(const_cast<edict_t*>(shooter)) > 0;
    int best = 0;
    for(const auto& kv : ropes)
    {
        const Rope& r = kv.second;
        if(!r.params.chain || r.params.pole || r.p.size() < 2)
        {
            continue;
        }
        const float reach = r.params.thickness * 0.5f + chainHitGrow + radius;
        glm::vec3 rlo = r.p.front(), rhi = r.p.front();
        for(const glm::vec3& q : r.p)
        {
            rlo = glm::min(rlo, q);
            rhi = glm::max(rhi, q);
        }
        if(glm::any(glm::greaterThan(lo, rhi + glm::vec3{reach})) || glm::any(glm::lessThan(hi, rlo - glm::vec3{reach})))
        {
            continue;
        }
        if(climbing && glm::all(glm::greaterThanEqual(geom::vec(shooter->v.absmax), rlo - glm::vec3{solidNear})) &&
            glm::all(glm::lessThanEqual(geom::vec(shooter->v.absmin), rhi + glm::vec3{solidNear})))
        {
            continue; // (the one climbing it shoots and strikes past it: its links are in his hands)
        }
        for(za::SizeT j = 0; j + 1 < r.p.size(); j++)
        {
            float ta = 0.f, tb = 0.f;
            const float dist = geom::segmentsDistance(start, end, r.p[j], r.p[j + 1], ta, tb);
            if(dist >= reach)
            {
                continue;
            }
            // Where the line comes within `reach` of the link (its cylinder): back from the nearest point by the chord
            // over the move's speed across the link.
            const glm::vec3 e = r.p[j + 1] - r.p[j];
            const float elen = glm::length(e);
            const glm::vec3 across = elen > 1e-3f ? d - e * (glm::dot(d, e) / (elen * elen)) : d;
            const float rate = glm::length(across);
            if(rate < 1e-3f)
            {
                continue; // (along the link: met at its neighbour)
            }
            const float t = ta - za::sqrt(reach * reach - dist * dist) / rate;
            if(t <= 0.f || t >= fraction)
            {
                continue; // (from in it: not a hit on it; a nearer hit first)
            }
            const glm::vec3 at = start + d * t, on = r.p[j] + e * tb;
            const glm::vec3 out = at - on;
            normal = glm::length(out) > 1e-3f ? glm::normalize(out) : -d / len;
            fraction = t;
            best = kv.first;
        }
    }
    return best;
}

int pick(const glm::vec3& from, const glm::vec3& dir, float range, float& distance)
{
    int best = 0;
    const glm::vec3 to = from + dir * range;
    for(const auto& kv : ropes)
    {
        const Rope& r = kv.second;
        for(za::SizeT j = 0; j + 1 < r.p.size(); j++)
        {
            float ta = 0.f, tb = 0.f;
            if(geom::segmentsDistance(from, to, r.p[j], r.p[j + 1], ta, tb) <= r.params.thickness * 0.5f + pickGrow &&
                ta > 0.f && ta * range < distance)
            {
                distance = ta * range;
                best = kv.first;
            }
        }
    }
    return best;
}

bool findGrab(const glm::vec3& hand, float reach, Grab& out)
{
    float best = 1e9f;
    for(const auto& kv : ropes)
    {
        const Rope& r = kv.second;
        float arc = 0.f;
        for(za::SizeT j = 0; j + 1 < r.p.size(); arc += r.rest[j], j++)
        {
            const glm::vec3 a = r.p[j], ab = r.p[j + 1] - a;
            const float len2 = glm::dot(ab, ab);
            const float t = len2 > 1e-6f ? za::clamp(glm::dot(hand - a, ab) / len2, 0.f, 1.f) : 0.f;
            const glm::vec3 at = a + ab * t;
            const float dist = glm::distance(hand, at) - r.params.thickness * 0.5f;
            const float s = arc + r.rest[j] * t;
            if(dist <= reach && dist < best && s >= topClear)
            {
                best = dist;
                out = Grab{kv.first, s, glm::distance(at, r.top), at, r.params.chain};
            }
        }
    }
    return best < 1e9f;
}

bool grabAlong(int ent, int client, float s, Grab& out)
{
    const Rope* r = ropeOf(ent);
    if(!r || r->p.size() < 2)
    {
        return false;
    }
    float length = 0.f;
    for(const float piece : r->rest)
    {
        length += piece;
    }
    s = za::clamp(s, topClear, za::max(topClear, length - topClear));
    const Rider& rd = r->rider;
    float d = 0.f;
    glm::vec3 at{0.f};
    if(rd.client == client && rd.started && rd.monkey)
    {
        at = monkeyAt(*r, rd.bob, s); // (ropes7: on his taut line through the bob)
        d = glm::distance(at, r->top);
    }
    else if(rd.client == client && rd.started)
    {
        // (On his line: straight from the top as the line's other holds are, layRider's scale.)
        d = s * (rd.sLow > 1e-3f ? rd.dLow / rd.sLow : 1.f);
        at = r->top + riderDir(*r, rd.bob) * d;
    }
    else
    {
        at = pointAtArc(*r, s);
        d = glm::distance(at, r->top);
    }
    out = Grab{ent, s, d, at, r->params.chain};
    return true;
}

bool holdAt(int ent, int client, float s, float d, glm::vec3& at)
{
    const Rope* r = ropeOf(ent);
    if(!r || r->p.size() < 2)
    {
        return false;
    }
    const Rider& rd = r->rider;
    at = rd.client == client && rd.started ? (rd.monkey ? monkeyAt(*r, rd.bob, s) : r->top + riderDir(*r, rd.bob) * d)
                                           : pointAtArc(*r, s);
    return true;
}

glm::vec3 holdVelocity(int ent, int client, float s, float d)
{
    const Rope* r = ropeOf(ent);
    if(!r || r->p.size() < 2)
    {
        return glm::vec3{0.f};
    }
    const Rider& rd = r->rider;
    if(rd.client == client && rd.started)
    {
        const float h = r->lastStep > 0.f ? r->lastStep : longestStep;
        if(rd.monkey)
        {
            return (monkeyAt(*r, rd.bob, s) - monkeyAt(*r, rd.bobPrev, s)) / h;
        }
        return (riderDir(*r, rd.bob) - riderDir(*r, rd.bobPrev)) * (d / h);
    }
    glm::vec3 vel{0.f};
    (void)pointAtArc(*r, s, &vel);
    return vel;
}

bool ride(int client, int ent, float sHigh, float dHigh, float sLow, float dLow, const glm::vec3& grabVel)
{
    Rope* r = ropeOf(ent);
    if(!r || r->params.pole)
    {
        return false; // (a pole: no weight on it, its holds on its fixed points)
    }
    Rider& rd = r->rider;
    if(rd.client >= 0 && rd.client != client && rd.frame >= host_framecount - 1)
    {
        return false; // (another's: this climber follows its points)
    }
    if(Rope* other = riddenBy(client); other && other != r)
    {
        other->rider = Rider{}; // (onto another rope: the last one let go)
    }
    if(rd.client != client)
    {
        rd = Rider{};
        rd.client = client;
        rd.grabVel = grabVel;
        rd.monkey = r->monkey; // (ropes7: kept while he rides it)
    }
    else if(rd.started && !rd.monkey && za::abs(dHigh + riderBelow - rd.radius) > 1e-3f)
    {
        // The highest hand changed (hand over hand): the bob kept at its angle and its swing, at the new radius.
        const float k = (dHigh + riderBelow) / za::max(rd.radius, 1.f);
        rd.bob = r->top + (rd.bob - r->top) * k;
        rd.bobPrev = r->top + (rd.bobPrev - r->top) * k;
        rd.radius = dHigh + riderBelow;
    }
    rd.frame = host_framecount;
    rd.sHigh = sHigh;
    rd.dHigh = dHigh;
    rd.sLow = za::max(sLow, sHigh);
    rd.dLow = za::max(dLow, dHigh);
    return true;
}

void riderBlocked(int client, const glm::vec3& missed)
{
    Rope* r = riddenBy(client);
    const float len = glm::length(missed);
    if(!r || !r->rider.started || len < 0.25f)
    {
        return;
    }
    Rider& rd = r->rider;
    // The body couldn't go where the rope's swing carried it (a wall, the floor): the bob back by as much, its speed that
    // way gone. (ropes6: the bob moved back with its last place, its speed kept but for that way. Moved alone, the
    // shift was a kick away: a pull's miss, the body a little short of where the hands took it with the rope still,
    // swung it 30-60 units/s at each regrab.)
    const glm::vec3 n = missed / len;
    glm::vec3 v = rd.bob - rd.bobPrev;
    if(const float into = glm::dot(v, n); into > 0.f)
    {
        v -= n * into;
    }
    rd.bob -= missed * (rd.monkey ? 1.f : rd.radius / za::max(rd.dLow, 1.f)); // (ropes7: a monkey's bob at his hands)
    rd.bobPrev = rd.bob - v;
}

void setPump(int client, const glm::vec3& accel)
{
    if(Rope* r = riddenBy(client))
    {
        r->rider.pump = accel;
    }
}

bool riding(int client, int& ent, glm::vec3& handVel)
{
    for(const auto& kv : ropes)
    {
        const Rope& r = kv.second;
        const Rider& rd = r.rider;
        if(rd.client == client && rd.started)
        {
            ent = kv.first;
            const float h = r.lastStep > 0.f ? r.lastStep : longestStep;
            handVel = rd.monkey ? (rd.bob - rd.bobPrev) / h : (riderDir(r, rd.bob) - riderDir(r, rd.bobPrev)) * (rd.dLow / h);
            return true;
        }
    }
    return false;
}

void toolgunPlaced(edict_t* e, const glm::vec3& at, float yaw)
{
    const bool pole = !ZA_STRCMP(PR_GetString(e->v.classname), "vr_pole");
    const float length = za::clamp(vr_rope_spawn_length.value, 16.f, 2048.f);
    placeWith(e, at, length,
        pole ? za::clamp(vr_rope_spawn_pole_thickness.value, 0.25f, poleThickest) : za::clamp(vr_rope_spawn_thickness.value, 0.25f, 8.f),
        za::clamp(static_cast<int>(vr_rope_spawn_segments.value), 0, maxPieces), za::max(vr_rope_spawn_mass.value, 0.f),
        za::clamp(vr_rope_spawn_stiffness.value, 0.f, 1.f), za::max(vr_rope_spawn_damping.value, 0.f),
        vr_rope_spawn_pinned.value != 0.f);
    const progs::FieldOffsets& f = progs::fields();
    const glm::vec3 start = at + glm::vec3{0.f, 0.f, 1.f};
    glm::vec3 top{0.f}, end{0.f};
    if(pole && f.rope_style >= 0)
    {
        progs::fieldFloat(e, f.rope_style) = vr_rope_spawn_metal.value != 0.f ? 1.f : 0.f;
    }
    // (ropes8a) With a Span a pole is strung as a rope is (level or slanted: past vr_rope_monkey_angle a monkey traverse,
    // else climbed), taut; without, it stands from the floor.
    if(pole && !(vr_rope_spawn_span.value > 0.f && f.rope_end >= 0))
    {
        // (ropes7) A pole: from the floor up to the ceiling, or its length (it stands on the floor: make()).
        const trace_t tr = worldtrace::serverLine(start, start + glm::vec3{0.f, 0.f, length}, e);
        top = vecOf(tr.endpos) - glm::vec3{0.f, 0.f, tr.fraction < 1.f ? 1.f : 0.f};
        if(f.rope_length >= 0)
        {
            progs::fieldFloat(e, f.rope_length) = za::max(16.f, top.z - at.z);
        }
    }
    else if(vr_rope_spawn_span.value > 0.f && f.rope_end >= 0)
    {
        // (ropes7) Strung: from strungHeight over the ghost (or under the ceiling) to its span the ghost's way and its rise
        // up (short of a wall), its slack longer than the line between.
        const trace_t up = worldtrace::serverLine(start, at + glm::vec3{0.f, 0.f, strungHeight}, e);
        top = vecOf(up.endpos) - glm::vec3{0.f, 0.f, up.fraction < 1.f ? 1.f : 0.f};
        const float r = glm::radians(yaw + 180.f); // (the ghost faces the player: away from him)
        const glm::vec3 dir{za::cos(r), za::sin(r), 0.f};
        const glm::vec3 want = top + dir * za::min(vr_rope_spawn_span.value, 2048.f) + glm::vec3{0.f, 0.f, vr_rope_spawn_rise.value};
        const trace_t across = worldtrace::serverLine(top, want, e);
        end = vecOf(across.endpos);
        if(across.fraction < 1.f && glm::length(want - top) > 1.f)
        {
            end -= glm::normalize(want - top) * 2.f;
        }
        float* v = progs::fieldPtr(e, f.rope_end);
        v[0] = end.x;
        v[1] = end.y;
        v[2] = end.z;
        progs::fieldFloat(e, f.rope_length) = 0.f;
        if(f.rope_slack >= 0)
        {
            progs::fieldFloat(e, f.rope_slack) = za::clamp(vr_rope_spawn_slack.value, 0.f, 2.f);
        }
    }
    else
    {
        return;
    }
    for(int k = 0; k < 3; k++)
    {
        e->v.origin[k] = top[k];
    }
    SV_LinkEdict(e, false);
}

void parse()
{
    const int num = MSG_ReadShort();
    const int flags = MSG_ReadByte();
    const int thick = MSG_ReadByte();
    const int n = MSG_ReadByte();
    if(n < 0 || thick < 0 || flags < 0) // (a message cut short)
    {
        return;
    }
    if(flags & flagGone)
    {
        clientRopes.erase(num);
        return;
    }
    ClientRope& c = clientRopes[num];
    c.chain = (flags & flagChain) != 0;
    c.pole = (flags & flagPole) != 0;
    c.metal = (flags & flagMetal) != 0;
    c.monkey = (flags & flagMonkey) != 0;
    c.thickness = static_cast<float>(thick) / 16.f;
    // (The last message's points kept to draw between: only when it came in the message just before this one.)
    const bool again = c.msgTime == cl.mtime[0]; // (sent twice in one message: the first's prev kept)
    const bool follows = again || (c.msgTime == cl.mtime[1] && c.p.size() == static_cast<za::SizeT>(n));
    if(!again)
    {
        c.prev = c.p;
    }
    c.msgTime = cl.mtime[0];
    c.shownTime = -1.0;
    c.p.resize(static_cast<za::SizeT>(n));
    glm::vec3 at{0.f};
    for(int i = 0; i < n; i++)
    {
        for(int k = 0; k < 3; k++)
        {
            at[k] = i == 0 ? MSG_ReadCoord(cl.protocolflags) : at[k] + static_cast<float>(MSG_ReadShort()) / 8.f;
        }
        c.p[static_cast<za::SizeT>(i)] = at;
    }
    if(!follows)
    {
        c.prev = c.p; // (a rope at rest until now, or a new one: no lerp from a stale place)
    }
}

float clientRopeDistance(const glm::vec3& p)
{
    float best = 1e9f;
    for(auto& kv : clientRopes)
    {
        const za::Vector<glm::vec3>& q = shownOf(kv.second);
        for(za::SizeT j = 0; j + 1 < q.size(); j++)
        {
            float ta = 0.f, tb = 0.f;
            best = za::min(best, geom::segmentsDistance(p, p, q[j], q[j + 1], ta, tb));
        }
    }
    return best;
}

bool clientSolidTrace(const float* start, const float* mins, const float* maxs, const float* end, trace_t& best)
{
    if(sv.active || clientRopes.empty())
    {
        return false; // (a server here: its own player isn't predicted)
    }
    if(!boxHullReady)
    {
        SV_InitBoxHull(); // (a remote client never cleared a server's world: SV_ClearWorld makes it)
        boxHullReady = true;
    }
    bool holding = false;
    for(int hand = 0; hand < 2 && !holding; hand++)
    {
        glm::vec3 at, up;
        float thick = 0.f, w = 0.f;
        holding = climb::ropeLine(hand, at, up, thick, w) && w > 0.f;
    }
    const glm::vec3 s = vecOf(start), slo = s + vecOf(mins), shi = s + vecOf(maxs);
    const auto overlaps = [&](const glm::vec3& lo, const glm::vec3& hi, float by)
    {
        for(int c = 0; c < 3; c++)
        {
            if(slo[c] > hi[c] - by || shi[c] < lo[c] + by)
            {
                return false;
            }
        }
        return true;
    };
    bool met = false;
    for(const auto& kv : clientRopes)
    {
        const ClientRope& c = kv.second;
        if(!c.pole || c.p.size() < 2)
        {
            continue;
        }
        const glm::vec3 a = c.p.front(), d = c.p.back() - a;
        const glm::vec3 grow{c.thickness * 0.5f + solidNear};
        if(holding && overlaps(glm::min(a, c.p.back()) - grow, glm::max(a, c.p.back()) + grow, 0.f))
        {
            continue; // (passSolids: he holds a rope by it)
        }
        const int n = solidCount(d, c.thickness);
        bool inside = false;
        for(int k = 0; k < n && !inside; k++)
        {
            glm::vec3 lo, hi;
            solidBounds(a, d, c.thickness, k, n, lo, hi);
            inside = overlaps(lo, hi, solidInside);
        }
        if(inside)
        {
            continue; // (... or he is in one of its boxes)
        }
        for(int k = 0; k < n; k++)
        {
            glm::vec3 lo, hi;
            solidBounds(a, d, c.thickness, k, n, lo, hi);
            vec3_t hullMins, hullMaxs;
            for(int i = 0; i < 3; i++)
            {
                hullMins[i] = lo[i] - maxs[i];
                hullMaxs[i] = hi[i] - mins[i];
            }
            hull_t* h = SV_HullForBox(hullMins, hullMaxs);
            trace_t tr;
            ZA_MEMSET(&tr, 0, sizeof(tr));
            tr.fraction = 1.f;
            tr.allsolid = true;
            VectorCopy(end, tr.endpos);
            SV_RecursiveHullCheck(h, h->firstclipnode, 0.f, 1.f, const_cast<float*>(start), const_cast<float*>(end), &tr);
            if(tr.allsolid || tr.startsolid || tr.fraction < best.fraction) // (SV_ClipToLinks' choice)
            {
                const bool wasStuck = best.startsolid;
                best = tr;
                best.startsolid = best.startsolid || wasStuck;
                best.ent = nullptr;
                met = true;
            }
        }
    }
    return met;
}

bool clientRopeMonkey(const glm::vec3& near, float within)
{
    const ClientRope* best = nullptr;
    float bestDist = within;
    for(auto& kv : clientRopes)
    {
        const za::Vector<glm::vec3>& q = shownOf(kv.second);
        for(za::SizeT j = 0; j + 1 < q.size(); j++)
        {
            float ta = 0.f, tb = 0.f;
            if(const float d = geom::segmentsDistance(near, near, q[j], q[j + 1], ta, tb); d < bestDist)
            {
                bestDist = d;
                best = &kv.second;
            }
        }
    }
    return best && best->monkey;
}

bool clientRopeAt(const glm::vec3& near, float within, float z, glm::vec3& at, float* bottom)
{
    const za::Vector<glm::vec3>* best = nullptr;
    float bestDist = within;
    for(auto& kv : clientRopes)
    {
        const za::Vector<glm::vec3>& q = shownOf(kv.second);
        for(za::SizeT j = 0; j + 1 < q.size(); j++)
        {
            float ta = 0.f, tb = 0.f;
            if(const float d = geom::segmentsDistance(near, near, q[j], q[j + 1], ta, tb); d < bestDist)
            {
                bestDist = d;
                best = &q;
            }
        }
    }
    if(!best)
    {
        return false;
    }
    // Down the rope from its top to where it crosses `z` (or its bottom).
    at = best->back();
    if(bottom)
    {
        *bottom = at.z; // (its lowest point: a free end flung up still reaches that low)
        for(const glm::vec3& q : *best)
        {
            *bottom = za::min(*bottom, q.z);
        }
    }
    for(za::SizeT j = 0; j + 1 < best->size(); j++)
    {
        const glm::vec3 a = (*best)[j], b = (*best)[j + 1];
        if((a.z - z) * (b.z - z) <= 0.f && za::abs(a.z - b.z) > 1e-3f)
        {
            at = glm::mix(a, b, (a.z - z) / (a.z - b.z));
            break;
        }
    }
    return true;
}

bool clientRopeNearest(const glm::vec3& near, float within, glm::vec3& at, glm::vec3& up, float& thickness)
{
    ClientRope* best = nullptr;
    float bestDist = within;
    for(auto& kv : clientRopes)
    {
        const za::Vector<glm::vec3>& q = shownOf(kv.second);
        for(za::SizeT j = 0; j + 1 < q.size(); j++)
        {
            float ta = 0.f, tb = 0.f;
            if(const float d = geom::segmentsDistance(near, near, q[j], q[j + 1], ta, tb); d < bestDist)
            {
                bestDist = d;
                best = &kv.second;
            }
        }
    }
    if(!best)
    {
        return false;
    }
    // On its curve as drawn (drawOpaque's: through the points as shown this frame), as far straight from its top as
    // `near` is: a hold keeps that while it hangs (on the rider's line, straight from the top), so the point found moves
    // as smoothly as the drawn rope does (the nearest point slid along a bent curve as `near`, a tick ahead, moved).
    smoothInto(shownOf(*best), scratch.smooth);
    const za::Vector<glm::vec3>& q = scratch.smooth;
    if(q.size() < 2)
    {
        return false;
    }
    const float want = glm::distance(near, q[0]);
    za::SizeT j = 0;
    while(j + 2 < q.size() && glm::distance(q[j + 1], q[0]) < want)
    {
        j++;
    }
    const float da = glm::distance(q[j], q[0]), db = glm::distance(q[j + 1], q[0]);
    at = glm::mix(q[j], q[j + 1], db - da > 1e-4f ? za::clamp((want - da) / (db - da), 0.f, 1.f) : 1.f);
    const glm::vec3 piece = q[j] - q[j + 1]; // (the points go from its top down)
    up = glm::length(piece) > 1e-4f ? glm::normalize(piece) : glm::vec3{0.f, 0.f, 1.f};
    thickness = best->thickness;
    return true;
}

void drawOpaque()
{
    if(clientRopes.empty())
    {
        return;
    }
    const bool build = builtFrame != host_framecount;
    builtFrame = host_framecount;
    for(auto& kv : clientRopes)
    {
        ClientRope& c = kv.second;
        if(c.p.size() < 2)
        {
            continue;
        }
        if(build)
        {
            QVR_PROFILE("climbable ropes build");
            smoothInto(shownOf(c), scratch.smooth);
            c.cord.shape(scratch.smooth.data(), scratch.smooth.size(), styleOf(c));
            const bool built = c.cord.build(vecOf(r_refdef.vieworg), scratch.rings, c.sides);
            c.batch = built ? gfx::uploadTube(scratch.rings) : gfx::TubeBatch{};
        }
        if(c.batch.count)
        {
            QVR_GPU_PROFILE("climbable ropes draw");
            gfx::drawTube(c.batch, c.sides, c.cord.albedo(), keyLight, c.cord.rust(), c.cord.flat(), 1.f,
                c.pole ? (c.metal ? 2 : 1) : 0); // (ropes8a: a pole's wood or metal surface)
        }
    }
}

} // namespace qvr::climbrope

// (ropes8c) SV_Move's shots, nails and blows against the chains' links (world.c SV_ClipToChains).
extern "C" int VR_ChainShotClip(const float* start, const float* mins, const float* maxs, const float* end, edict_t* pass, trace_t* trace)
{
    if(!sv.active || !progs::bindings().isVrProgs)
    {
        return 0;
    }
    float radius = 0.f;
    for(int c = 0; c < 3; c++)
    {
        radius = za::max(radius, za::max(-mins[c], maxs[c]));
    }
    const edict_t* shooter = pass;
    if(pass && !(static_cast<int>(pass->v.flags) & FL_CLIENT) && pass->v.owner)
    {
        shooter = PROG_TO_EDICT(pass->v.owner); // (a nail: its shooter's)
    }
    float fraction = trace->fraction;
    glm::vec3 normal{0.f};
    const glm::vec3 a = geom::vec(start), b = geom::vec(end);
    const int ent = climbrope::chainClip(a, b, radius, shooter, fraction, normal);
    if(!ent)
    {
        return 0;
    }
    const glm::vec3 at = a + (b - a) * fraction;
    trace->fraction = fraction;
    trace->allsolid = false;
    trace->startsolid = false;
    for(int c = 0; c < 3; c++)
    {
        trace->endpos[c] = at[c];
        trace->plane.normal[c] = normal[c];
    }
    trace->plane.dist = glm::dot(normal, at);
    trace->ent = EDICT_NUM(ent);
    return 1;
}

// (ropes8d) SV_ClipToLinks: whether `mover` (a player) is let through pole box `touch` (passSolids' solidPass): every
// climber on a pole passes it, not only its owner.
extern "C" int VR_PoleSolidPasses(edict_t* touch, edict_t* mover)
{
    if(touch->v.modelindex != 0.f || !touch->v.goalentity || !sv.active)
    {
        return 0;
    }
    const int pn = NUM_FOR_EDICT(mover);
    if(pn < 1 || pn > svs.maxclients || pn > 64)
    {
        return 0;
    }
    const Rope* r = ropeOf(NUM_FOR_EDICT(PROG_TO_EDICT(touch->v.goalentity)));
    if(!r)
    {
        return 0;
    }
    const int tn = NUM_FOR_EDICT(touch);
    bool ours = false;
    for(const int s : r->solids)
    {
        ours = ours || s == tn;
    }
    if(!ours)
    {
        return 0;
    }
    if((r->solidPass >> (pn - 1)) & 1u)
    {
        return 1;
    }
    // (ropes8e) In it now (set there, or a box made over him since passSolids ran: SV_CheckStuck would put him back
    // where he was), let out as passSolids would: his box against entities in its box by more than solidInside.
    glm::vec3 plo, phi;
    entBoxOf(mover, plo, phi);
    return boxIn(plo, phi, geom::vec(touch->v.origin) + geom::vec(touch->v.mins), geom::vec(touch->v.origin) + geom::vec(touch->v.maxs),
               solidInside)
               ? 1
               : 0;
}
