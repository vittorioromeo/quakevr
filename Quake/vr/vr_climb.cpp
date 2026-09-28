// vr_climb.cpp -- climbing: holds taken with either hand or both, hand over hand, shimmying and mantling (vr_climb,
// experimental; server side, and the drawn hands on their holds, client side).
//
// Holds. An empty hand can only take hold of an EDGE -- a walkable top surface (normal z > 0.7) just under the hand,
// with room above it for the hand, and a drop of at least `minDrop` units within `edgeReach` of the hand (a ledge, a
// rung, a beam; not open floor, a wall or a stair). A step down on the way to the drop is passed over (a rung above
// another, a ledge with a trim under its lip). The grip must be pressed at the hold (a press elsewhere, dragged onto
// one, does nothing), with no weapon, carried object, locked force grab or flashlight in that hand, and not at a
// holster that holds a weapon while standing (the grip draws it; an empty holster, or any while hanging, gives way to
// the hold). The hold is on the edge's line: the hand's place along the edge, on the top, `holdInset` units behind the
// edge; the hand is drawn there (its palm's middle, moved by vr_climb_hand_out/up/side: display only) while it holds,
// whatever the tracked hand does.
//
// Leniency (vr_climb_leniency, cm). A grip that takes no hold where the hand is (above) looks round it: the same rule
// tried from points on shells round the hand, up to that far from it. Of the holds found, the one taken is the nearest
// to the hand (a hold the hand moves or reaches towards counts as up to `reachFavour` nearer), not behind the head
// (on the far side from the hand), not lower under the hand than the rule itself reaches (`surfaceBelow`), at most
// the leniency plus `lenientReach` from the hand, and in sight of the hand (a line from the hand, or from the head for
// a hand in a wall, to just over the lip). The hold's place along the edge is the hand's own, as at a hold. A hold
// taken where the hand is always wins: leniency only adds holds where there were none. Nothing moves the body when a
// hold is taken (the pull is the hands' motion since); the drawn hand eases onto it (slower the further it is).
//
// Hanging. While a hand holds, the player hangs: no gravity, no stick, no room-scale walk. Each hand holds on to its
// hold: the body moves by the hands' pull, what the holding hands moved relative to the body since the last frame,
// the other way (pulling a hand down lifts the body). Two holding hands pull together, each weighted by how far it
// moved: the hand that pulls carries the body, a hand held still is not dragged back against it, and two hands
// pulling alike move it by their average. A hand taking hold or letting go changes only which hands pull, never where
// the body is: hand over hand, up, down or sideways, with no jump at the hand-off. A hold on a moving brush model
// carries the body with it. The body moves through the world by player-box traces (sliding along what blocks it), at
// most `maxHangSpeed` units a second, and neither rises with its feet above the highest hold nor strays more than
// `maxHangReach` units (horizontally) from any hold; each limit only stops the body going further past it (letting
// go of the higher hand never drops the body back under the lower one's limit).
//
// Mantle: once a hand has pulled down by `mantlePull` units since it took hold and the head is `mantleHead` units
// above its ledge, and the player's box fits on top (with floor under it) and the way there (straight up, then over)
// is clear, the body is carried there in `mantleTime` seconds, and stands.
//
// Letting go of everything: the player falls, flung by the hands' release (vr_climb_fling, capped). No new hold for
// `regrabDelay` seconds after letting go of everything or mantling; holds lower than vr_climb_min_height above the
// feet are ignored.
//
// The QC never sees the grip of a hand that holds (its grab bits are masked in .vrbits0 from the press until the
// release), so the press picks nothing up; instead .vrbits0 bits 15 (off hand) and 16 (main hand) tell it the hand
// holds (QVR_VRBITS0_*HAND_CLIMBING: no melee blows). The client gets the holds as stats (STAT_QVR_CLIMB*).

#include "vr_climb.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_hands.hpp"
#include "vr_move.hpp"
#include "vr_progs.hpp"
#include "vr_protocol.hpp"
#include "vr_server.hpp"
#include "vr_units.hpp"

#include <algorithm>
#include <cmath>
#include <optional>
#include <vector>

using namespace qvr;
using namespace qvr::progs;

namespace
{

// QC/vr_defs.qc QVR_VRBITS0_*: [0] off hand, [1] main hand.
constexpr int grabBit[2] = {1 << 1, 1 << 3};
constexpr int prevGrabBit[2] = {1 << 2, 1 << 4};
constexpr int climbingBit[2] = {1 << 15, 1 << 16};
constexpr int busyButton[2] = {protocol::QVR_BUTTON_OFFHANDBUSY, protocol::QVR_BUTTON_MAINHANDBUSY};

// QVR_HS_* (vr_body.hpp): the grip spots the client reports per hand.
constexpr int HS_LEFT_SHOULDER_HOLSTER = 3;
constexpr int HS_HAND_SWITCH = 7;
constexpr int HS_RIGHT_UPPER_HOLSTER = 9;

constexpr float surfaceAbove = 12.f;  // the hold's top may be this far above the hand (the hand sunk into it),
constexpr float surfaceBelow = 10.f;  // or this far below it
constexpr float handRoom = 8.f;       // clear space above the top for the hand
constexpr float edgeReach = 16.f;     // the drop must be this close to the hand
constexpr float edgeSlack = 6.f;      // a hand this far short of the edge still takes hold
constexpr float minDrop = 32.f;       // and this deep (a stair step is not a ledge)
constexpr float holdInset = 2.f;      // the hold (the palm's middle) is this far behind the edge,
constexpr float holdLift = 1.f;       // and this far above the top
constexpr float regrabDelay = 0.4f;   // seconds without new holds after letting go of everything or mantling
constexpr float maxHangSpeed = 500.f; // units / second the body follows the hands at
constexpr float maxHangReach = 48.f;  // the body stays this close (horizontally) to each hold
constexpr float mantlePull = 8.f;     // units a hand must have pulled down before a mantle
constexpr float mantleHead = 8.f;     // and the head must be this far above the ledge
constexpr float mantleTime = 0.3f;    // seconds the mantle takes
constexpr float flingMax = 200.f;     // units / second the release flings at most,
constexpr float flingMaxUp = 150.f;   // and upwards
constexpr float statScale = 8.f;      // hold coordinates in the stats: eighths of a unit
constexpr float lenientStep = 2.f;    // units at most between the lenient search's shells round the hand
constexpr float reachFavour = 0.3f;   // a hold the hand moves (or reaches) towards counts as this much nearer
constexpr float movingHand = 0.3f;    // m/s: a hand this fast reaches the way it moves (slower: from the head)
constexpr float lenientReach = edgeSlack + holdInset; // a lenient hold is at most the leniency plus this from the hand

[[nodiscard]] glm::vec3 vec(const float* v)
{
    return {v[0], v[1], v[2]};
}

void setVec(float* out, const glm::vec3& v)
{
    out[0] = v.x;
    out[1] = v.y;
    out[2] = v.z;
}

[[nodiscard]] trace_t traceBox(const glm::vec3& start, const glm::vec3& mins, const glm::vec3& maxs,
    const glm::vec3& end, int type, edict_t* pass)
{
    vec3_t s{start.x, start.y, start.z}, mi{mins.x, mins.y, mins.z}, ma{maxs.x, maxs.y, maxs.z},
        e{end.x, end.y, end.z};
    return SV_Move(s, mi, ma, e, type, pass);
}

// A line through the world and brush models (a hand's probe: Quake's point hull).
[[nodiscard]] trace_t traceLine(const glm::vec3& start, const glm::vec3& end, edict_t* pass)
{
    return traceBox(start, glm::vec3{0.f}, glm::vec3{0.f}, end, MOVE_NOMONSTERS, pass);
}

// The player's box swept from `start` to `end`.
[[nodiscard]] trace_t tracePlayer(edict_t* ent, const glm::vec3& start, const glm::vec3& end)
{
    return traceBox(start, vec(ent->v.mins), vec(ent->v.maxs), end, MOVE_NORMAL, ent);
}

[[nodiscard]] bool debug()
{
    return vr_climb_debug.value != 0.f;
}

struct Ledge
{
    float top{0.f};             // the surface's height
    glm::vec3 out{0.f};         // horizontal, from the ledge towards the drop
    glm::vec3 hold{0.f};        // where the hand holds: on the edge's line (see the top of the file)
    edict_t* ent{nullptr};      // the brush model it belongs to (nullptr: the world)
};

// Whether the point `p` (just above the top) is over the top at `top` (not past its edge).
[[nodiscard]] bool overTop(edict_t* player, const glm::vec3& p, float top)
{
    const trace_t t = traceLine(p, p - glm::vec3{0.f, 0.f, 5.f}, player);
    return t.fraction < 1.f && t.endpos[2] >= top - 3.f;
}

// Whether the hand at `hand` is at a hold (see the top of the file); `along`: where the hand is along the edge (the
// hand itself, when `hand` is a point looked from further on). `lenient` (the leniency's points): a top looked for from
// above the point that starts in something (the next rung up) is looked for from the point itself.
[[nodiscard]] std::optional<Ledge> findLedgeAt(edict_t* player, const glm::vec3& hand, const glm::vec3& along, bool lenient)
{
    trace_t down = traceLine(hand + glm::vec3{0.f, 0.f, surfaceAbove}, hand - glm::vec3{0.f, 0.f, surfaceBelow}, player);
    if(lenient && down.startsolid && !traceLine(hand, hand, player).startsolid)
    {
        down = traceLine(hand, hand - glm::vec3{0.f, 0.f, surfaceBelow}, player);
    }
    if(down.startsolid || down.allsolid || down.fraction >= 1.f || down.plane.normal[2] < 0.7f)
    {
        return std::nullopt;
    }

    Ledge ledge;
    ledge.top = down.endpos[2];
    ledge.ent = down.ent && down.ent != qcvm->edicts ? down.ent : nullptr;

    const glm::vec3 onTop{hand.x, hand.y, ledge.top + 2.f};
    if(traceLine(onTop, onTop + glm::vec3{0.f, 0.f, handRoom}, player).fraction < 1.f)
    {
        return std::nullopt; // no room for the hand
    }

    // The drop: in eight directions, the first towards the body. A step down on the way is passed over.
    const glm::vec3 toBody = vec(player->v.origin) - hand;
    const float bodyYaw = std::atan2(toBody.y, toBody.x);
    constexpr float turns[8] = {0.f, 1.f, -1.f, 2.f, -2.f, 3.f, -3.f, 4.f};
    for(const float turn : turns)
    {
        const float yaw = bodyYaw + turn * glm::radians(45.f);
        const glm::vec3 dir{std::cos(yaw), std::sin(yaw), 0.f};
        float lastOnTop = 0.f; // the furthest probe still over the top
        for(float r = 4.f; r <= edgeReach; r += 4.f)
        {
            const glm::vec3 p = onTop + dir * r;
            if(traceLine(onTop, p, player).fraction < 1.f)
            {
                break; // a wall
            }
            const trace_t fall = traceLine(p, p - glm::vec3{0.f, 0.f, minDrop + 2.f}, player);
            if(fall.fraction < 1.f)
            {
                if(fall.endpos[2] >= ledge.top - 3.f)
                {
                    lastOnTop = r; // still the top
                }
                continue; // the top, or a step down
            }
            // The edge, between the last probe over the top and the next: found to a quarter of a unit.
            float lo = lastOnTop, hi = lastOnTop + 4.f;
            for(int i = 0; i < 4; i++)
            {
                const float mid = 0.5f * (lo + hi);
                (overTop(player, onTop + dir * mid, ledge.top) ? lo : hi) = mid;
            }
            // The edge's line: the face under it, met from the drop (its normal is the way out, square to the edge),
            // however slanting the way the drop was found (a hand reaching aside: the out was that way, and the hold
            // shifted along the edge); the hold is where the hand is along it. A face too slanted to tell (a slope):
            // the way the drop was found.
            ledge.out = dir;
            glm::vec3 edge = onTop + dir * lo;
            const glm::vec3 below{edge.x, edge.y, ledge.top - 1.f};
            const trace_t face = traceLine(below + dir * 4.f, below - dir * 4.f, player);
            if(face.fraction < 1.f && !face.startsolid && std::abs(face.plane.normal[2]) < 0.3f)
            {
                const glm::vec3 n = glm::normalize(glm::vec3{face.plane.normal[0], face.plane.normal[1], 0.f});
                if(glm::dot(n, dir) > 0.f)
                {
                    ledge.out = n;
                    const glm::vec3 from{along.x, along.y, onTop.z};
                    edge = from + n * glm::dot(vec(face.endpos) - from, n);
                }
            }
            ledge.hold = edge - ledge.out * holdInset;
            ledge.hold.z = ledge.top + holdLift;
            return ledge;
        }
    }

    return std::nullopt;
}

// A hand hovering just in front of the edge (there is nothing to stop it) counts: the ledge is
// also looked for `edgeSlack` units further from the body.
[[nodiscard]] std::optional<Ledge> findLedge(edict_t* player, const glm::vec3& hand, const glm::vec3& along, bool lenient = false)
{
    if(std::optional<Ledge> ledge = findLedgeAt(player, hand, along, lenient))
    {
        return ledge;
    }
    glm::vec3 away = hand - vec(player->v.origin);
    away.z = 0.f;
    if(!(glm::length(away) >= 1.f))
    {
        return std::nullopt;
    }
    // Just under the lip, against the face under it: the lip is looked for from as high as a hold may be above a hand.
    for(const float lift : {0.f, 0.5f * surfaceAbove, surfaceAbove})
    {
        const glm::vec3 raised = hand + glm::vec3{0.f, 0.f, lift};
        if(lift > 0.f && traceLine(hand, raised, player).fraction < 1.f)
        {
            break;
        }
        const glm::vec3 further = raised + glm::normalize(away) * edgeSlack;
        if(traceLine(raised, further, player).fraction < 1.f)
        {
            continue;
        }
        if(std::optional<Ledge> ledge = findLedgeAt(player, further, along, lenient); ledge && ledge->top <= hand.z + surfaceAbove)
        {
            return ledge;
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<Ledge> findLedge(edict_t* player, const glm::vec3& hand)
{
    return findLedge(player, hand, hand);
}

// The 26 ways from a cube's middle to its faces, edges and corners (the lenient search's points on each shell).
[[nodiscard]] const std::vector<glm::vec3>& shellDirections()
{
    static const std::vector<glm::vec3> dirs = [] {
        std::vector<glm::vec3> d;
        for(int x = -1; x <= 1; x++)
        {
            for(int y = -1; y <= 1; y++)
            {
                for(int z = -1; z <= 1; z++)
                {
                    if(x || y || z)
                    {
                        d.push_back(glm::normalize(glm::vec3{static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)}));
                    }
                }
            }
        }
        return d;
    }();
    return dirs;
}

// What the lenient search tried and turned down (vr_climb_try).
struct LenientStats
{
    int points{0}, found{0}, low{0}, below{0}, tooFar{0}, behind{0}, unseen{0};
};

// Leniency (see the top of the file): the nearest hold the hand at `hand` would take from a point up to `radius` units
// from it. `vel`: the hand's velocity (m/s); `headPos`: the head.
[[nodiscard]] std::optional<Ledge> findLenient(edict_t* player, const glm::vec3& hand, const glm::vec3& vel,
    const glm::vec3& headPos, float radius, LenientStats* stats = nullptr)
{
    LenientStats st;
    const float feet = player->v.origin[2] + player->v.mins[2];
    glm::vec3 reach = glm::length(vel) > movingHand ? vel : hand - headPos;
    reach = glm::length(reach) > 1e-3f ? glm::normalize(reach) : glm::vec3{0.f};
    const glm::vec2 side{hand.x - headPos.x, hand.y - headPos.y}; // the hand's side of the head
    const bool handInWall = traceLine(hand, hand, player).startsolid;
    const glm::vec3 sight = handInWall ? headPos : hand;

    std::optional<Ledge> best;
    float bestScore = 0.f;
    const int shells = std::max(1, static_cast<int>(std::ceil(radius / lenientStep - 1e-3f)));
    for(int k = 1; k <= shells; k++)
    {
        const float r = radius * static_cast<float>(k) / static_cast<float>(shells);
        for(const glm::vec3& dir : shellDirections())
        {
            const glm::vec3 p = hand + dir * r;
            st.points++;
            // The hold where the hand is along the edge; where that is off the ledge (past its end), where the point is.
            std::optional<Ledge> l = findLedge(player, p, hand, true);
            if(l && !overTop(player, l->hold, l->top))
            {
                l = findLedge(player, p, p, true);
            }
            if(!l)
            {
                continue;
            }
            st.found++;
            if(l->top < feet + vr_climb_min_height.value)
            {
                st.low++;
                continue; // too low for a hold (Lowest Ledge)
            }
            if(l->top < hand.z - surfaceBelow)
            {
                st.below++;
                continue; // lower under the hand than a hold may be
            }
            const glm::vec3 d = l->hold - hand;
            const float dist = glm::length(d);
            if(dist > radius + lenientReach)
            {
                st.tooFar++;
                continue; // further than a hold taken where the hand is, just short of it, plus the leniency
            }
            const glm::vec2 fromHead{l->hold.x - headPos.x, l->hold.y - headPos.y};
            if(glm::length(side) >= 4.f && glm::dot(fromHead, side) < 0.f)
            {
                st.behind++;
                continue; // behind the head, from the hand
            }
            const glm::vec3 lip = glm::vec3{l->hold.x, l->hold.y, l->top + 1.f} + l->out * (holdInset + 1.f);
            const trace_t seen = traceLine(sight, lip, player);
            if(seen.fraction < 1.f || seen.startsolid)
            {
                st.unseen++;
                continue; // through a wall
            }
            const float score = dist * (1.f - reachFavour * (dist > 1e-3f ? glm::dot(d / dist, reach) : 0.f));
            if(!best || score < bestScore)
            {
                best = l;
                bestScore = score;
            }
        }
    }
    if(stats)
    {
        *stats = st;
    }
    return best;
}

[[nodiscard]] float leniencyUnits()
{
    return std::max(0.f, vr_climb_leniency.value) * 0.01f * units::metresToUnits();
}

struct Grip
{
    bool active{false};
    glm::vec3 hold{0.f};       // where the hand holds (world, when it took hold)
    glm::vec3 lastHold{0.f};   // where the hold was last frame (it moves with its brush model)
    glm::vec3 relAtGrab{0.f};  // the hand relative to the body when it took hold
    glm::vec3 lastRel{0.f};    // and last frame
    Ledge ledge;
    int entNum{0};             // the hold's brush model (0: the world), which may move
    glm::vec3 entOrigin{0.f};  // and where it was
    bool owned{false};         // the grip's press belongs to the hold (hidden from the QC) until let go
    bool ownedLastFrame{false};
    int serial{0};             // counts the holds taken (the client tells a new hold from the last)
};

struct Climber
{
    Grip grips[2];
    bool mantling{false};
    double mantleStart{0.0};
    glm::vec3 mantleFrom{0.f}, mantleMid{0.f}, mantleTo{0.f};
    double noGrabUntil{0.0};
    double lastTime{-1.0};
    bool pressedLastFrame[2]{false, false};
    glm::vec3 lastOrigin{0.f}; // where the climb put the body (moved otherwise: let go)
    bool hanging() const
    {
        return grips[0].active || grips[1].active;
    }
};

// A fixed array (as the swimmers'): a climber is held by reference across SV_RunThink, which a growing vector could
// move.
Climber climbers[MAX_SCOREBOARD];

[[nodiscard]] Climber* climberOf(edict_t* ent)
{
    const int client = NUM_FOR_EDICT(ent) - 1;
    if(client < 0 || client >= std::min(svs.maxclients, static_cast<int>(MAX_SCOREBOARD)))
    {
        return nullptr;
    }
    return &climbers[client];
}

// The hand `h` relative to the body, from the latest move (already moved along with the body).
[[nodiscard]] glm::vec3 handRel(edict_t* ent, const VrMove& move, int h)
{
    return move.hands[h].pos - vec(ent->v.origin);
}

[[nodiscard]] edict_t* entityField(edict_t* ent, int ofs)
{
    const int v = ofs >= 0 ? fieldInt(ent, ofs) : 0;
    return v ? PROG_TO_EDICT(v) : nullptr;
}

// Nothing in the hand: no weapon, no carried object, no locked force grab, not the flashlight.
[[nodiscard]] bool handEmpty(edict_t* ent, const VrMove* move, int h)
{
    if(move && (move->buttons & busyButton[h]))
    {
        return false; // the flashlight (client-side)
    }
    const FieldOffsets& f = fields();
    if(!bindings().isVrProgs)
    {
        return h == 0; // another mod's progs: the main hand always holds its weapon
    }
    const float weapon = h == 1 ? ent->v.weapon : fieldFloatOr(ent, f.weapon2, 0.f);
    if(weapon != 0.f)
    {
        return false;
    }
    const edict_t* held = entityField(ent, h == 1 ? f.mainhand_held : f.offhand_held);
    if(held && !held->free)
    {
        return false;
    }
    if(fieldFloatOr(ent, h == 1 ? f.mainhand_fglocked : f.offhand_fglocked, 0.f) != 0.f)
    {
        return false;
    }
    const edict_t* pulled = entityField(ent, h == 1 ? f.mainhand_fgpulled : f.offhand_fgpulled);
    return !pulled || pulled->free;
}

// Whether the grip at `hotspot` is for a holster rather than a hold: one that holds a weapon (the grip draws it),
// while standing; the carried gun's handle always (the grip takes it). An empty holster does nothing with an empty
// hand, and hanging, a hand reaching past a shoulder is reaching for the next hold.
[[nodiscard]] bool holsterWins(edict_t* ent, int hotspot, bool hanging)
{
    if(hotspot < HS_LEFT_SHOULDER_HOLSTER || hotspot == HS_HAND_SWITCH)
    {
        return false;
    }
    if(hotspot > HS_RIGHT_UPPER_HOLSTER)
    {
        return true; // the carried gun's handle
    }
    if(hanging)
    {
        return false;
    }
    if(!bindings().isVrProgs)
    {
        return true; // (the holsters' contents unknown)
    }
    // QVR_HS_* 3, 4, 5, 6, 8, 9 -> .holsterweapon0..5 (weapons.qc setHolsterWeapon).
    const FieldOffsets& f = fields();
    const int holster[] = {f.holsterweapon0, f.holsterweapon1, f.holsterweapon2, f.holsterweapon3, -1,
        f.holsterweapon4, f.holsterweapon5};
    const int ofs = holster[hotspot - HS_LEFT_SHOULDER_HOLSTER];
    return ofs >= 0 && fieldFloatOr(ent, ofs, 0.f) != 0.f; // WID_FIST (0): empty
}

// Where grip `g`'s hold is now: it moves with its brush model.
[[nodiscard]] glm::vec3 holdNow(const Grip& g)
{
    if(g.entNum <= 0 || g.entNum >= qcvm->num_edicts)
    {
        return g.hold;
    }
    edict_t* e = EDICT_NUM(g.entNum);
    return g.hold + (vec(e->v.origin) - g.entOrigin);
}

// The top of grip `g`'s ledge now.
[[nodiscard]] float topNow(const Grip& g)
{
    return g.ledge.top + (holdNow(g).z - g.hold.z);
}

void letGoAll(Climber& c)
{
    c.grips[0].active = c.grips[1].active = false;
    c.mantling = false;
}

// The mantle: a spot on top of grip `g`'s ledge where the box fits, over floor, reachable straight up (or up and
// along the edge a little) and then over: straight in from the hold, else along the edge (near a ledge's end, where
// the box would stick out past it).
[[nodiscard]] bool findMantle(edict_t* ent, const Grip& g, glm::vec3& mid, glm::vec3& to)
{
    const glm::vec3 origin = vec(ent->v.origin);
    const glm::vec3 hold = holdNow(g);
    const float z = topNow(g) - ent->v.mins[2] + 1.f;
    if(z < origin.z)
    {
        return false;
    }
    const glm::vec3 along{-g.ledge.out.y, g.ledge.out.x, 0.f};
    for(const float side : {0.f, 8.f, -8.f, 16.f, -16.f})
    {
        mid = glm::vec3{origin.x, origin.y, z} + along * side;
        if(tracePlayer(ent, origin, mid).fraction < 1.f)
        {
            continue;
        }
        for(const float k : {20.f, 28.f, 36.f})
        {
            to = glm::vec3{hold.x, hold.y, z} - g.ledge.out * k + along * side;
            if(tracePlayer(ent, to, to).startsolid)
            {
                continue;
            }
            if(traceBox(to, vec(ent->v.mins), vec(ent->v.maxs), to - glm::vec3{0.f, 0.f, 8.f}, MOVE_NOMONSTERS, ent).fraction >= 1.f)
            {
                continue; // nothing to stand on
            }
            if(tracePlayer(ent, mid, to).fraction < 1.f)
            {
                continue;
            }
            return true;
        }
    }
    return false;
}

// Moves the body towards `target` through the world: the whole way, else up/down then across, else
// across then up/down (sliding along a wall or a ledge's face).
void moveBody(edict_t* ent, const glm::vec3& target)
{
    const glm::vec3 origin = vec(ent->v.origin);
    trace_t tr = tracePlayer(ent, origin, target);
    if(tr.allsolid)
    {
        return;
    }
    glm::vec3 best = vec(tr.endpos);
    if(tr.fraction < 1.f)
    {
        const glm::vec3 vertical{origin.x, origin.y, target.z};
        const glm::vec3 horizontal{target.x, target.y, origin.z};
        for(const glm::vec3& corner : {vertical, horizontal})
        {
            const trace_t a = tracePlayer(ent, origin, corner);
            if(a.allsolid)
            {
                continue;
            }
            const trace_t b = tracePlayer(ent, vec(a.endpos), target);
            const glm::vec3 end = b.allsolid ? vec(a.endpos) : vec(b.endpos);
            if(glm::distance(end, target) < glm::distance(best, target))
            {
                best = end;
            }
        }
    }
    setVec(ent->v.origin, best);
}

const char* handName(int h)
{
    return h ? "main" : "off";
}

// vr_climb_debug 2: a line a frame, for tracing a climb (the body, and each hand: holding or free, where it is).
void traceFrame(edict_t* ent, const Climber& c, const VrMove* move, const char* what, const glm::vec3& wanted,
    const glm::vec3& moved)
{
    if(vr_climb_debug.value < 2.f)
    {
        return;
    }
    char hands[2][96];
    for(int h = 0; h < 2; h++)
    {
        const glm::vec3 p = move ? move->hands[h].pos : glm::vec3{0.f};
        const glm::vec3 hold = holdNow(c.grips[h]);
        if(c.grips[h].active)
        {
            q_snprintf(hands[h], sizeof(hands[h]), "H %.2f %.2f %.2f @ %.2f %.2f %.2f", p.x, p.y, p.z, hold.x, hold.y, hold.z);
        }
        else
        {
            q_snprintf(hands[h], sizeof(hands[h]), "- %.2f %.2f %.2f", p.x, p.y, p.z);
        }
    }
    Con_Printf("climbtrace %.4f %s org %.3f %.3f %.3f vel %.1f %.1f %.1f want %.3f %.3f %.3f moved %.3f %.3f %.3f | off %s | main %s\n",
        qcvm->time, what, ent->v.origin[0], ent->v.origin[1], ent->v.origin[2], ent->v.velocity[0], ent->v.velocity[1],
        ent->v.velocity[2], wanted.x, wanted.y, wanted.z, moved.x, moved.y, moved.z, hands[0], hands[1]);
}

} // namespace

// SV_Physics_Client, before PlayerPreThink: takes hold and lets go (per the grips of the latest move), and hides the
// holding hands' grips from the QC.
extern "C" void VR_ClimbPreThink(edict_t* ent)
{
    Climber* cp = climberOf(ent);
    if(!cp)
    {
        return;
    }
    Climber& c = *cp;
    const double time = qcvm->time;
    if(c.lastTime < 0.0 || std::abs(time - c.lastTime) > 1.0) // a new map, a loaded game
    {
        c = Climber{};
    }
    c.lastTime = time;

    server::rebaseHands(ent);
    const VrMove* move = server::clientMove(ent);
    const bool tracked = move && (move->buttons & protocol::QVR_BUTTON_HANDSTRACKED);
    const bool enabled = vr_climb.value != 0.f && tracked && static_cast<int>(ent->v.movetype) == MOVETYPE_WALK &&
                         ent->v.health > 0.f;

    // Moved by something else (a teleporter, a respawn, setorigin), disabled, dead, noclipping, the
    // hold's brush model gone: let go, and fall as whatever moved the body has it.
    bool forced = !enabled;
    if((c.hanging() || c.mantling) && !(glm::distance(vec(ent->v.origin), c.lastOrigin) <= 2.f)) // (a NaN: let go too)
    {
        if(debug())
        {
            Con_Printf("climb: moved away, letting go\n");
        }
        forced = true;
    }
    for(const Grip& g : c.grips)
    {
        if(g.active && g.entNum > 0 && (g.entNum >= qcvm->num_edicts || EDICT_NUM(g.entNum)->free))
        {
            forced = true;
        }
    }
    if(forced && (c.hanging() || c.mantling))
    {
        letGoAll(c);
        c.noGrabUntil = time + regrabDelay;
    }

    const bool wasHanging = c.hanging();
    const bool held[2] = {c.grips[0].active, c.grips[1].active};
    for(int h = 0; h < 2; h++)
    {
        Grip& g = c.grips[h];
        const bool pressed = move && (move->vrBits0 & grabBit[h]);
        const bool pressEdge = pressed && !c.pressedLastFrame[h];
        c.pressedLastFrame[h] = pressed;
        g.ownedLastFrame = g.owned;
        if(!pressed)
        {
            g.owned = false;
        }

        if(g.active && (!pressed || !handEmpty(ent, move, h)))
        {
            g.active = false;
            if(debug())
            {
                Con_Printf("climb: %s hand lets go at (%.1f %.1f %.1f)\n", handName(h), ent->v.origin[0], ent->v.origin[1],
                    ent->v.origin[2]);
            }
        }

        if(!enabled || !pressEdge || g.active || c.mantling)
        {
            continue;
        }
        const bool hanging = c.grips[1 - h].active;
        const int hotspot = move->hotspots[h];
        const bool atHolster = holsterWins(ent, hotspot, hanging);
        if(time < c.noGrabUntil || atHolster || !handEmpty(ent, move, h))
        {
            if(debug())
            {
                Con_Printf("climb: %s hand grips: %s\n", handName(h),
                    time < c.noGrabUntil ? "too soon" : atHolster ? "at a holster" : "not empty");
            }
            continue;
        }

        const glm::vec3 hand = move->hands[h].pos;
        std::optional<Ledge> ledge = findLedge(ent, hand);
        const float feet = ent->v.origin[2] + ent->v.mins[2];
        if(ledge && ledge->top < feet + vr_climb_min_height.value)
        {
            if(debug())
            {
                Con_Printf("climb: hold at %.1f too low (feet %.1f)\n", ledge->top, feet);
            }
            ledge.reset();
        }
        const char* how = "";
        if(const float radius = leniencyUnits(); !ledge && radius > 0.f)
        {
            // Missed: the nearest hold round the hand (leniency).
            const double t0 = Sys_DoubleTime();
            LenientStats st;
            ledge = findLenient(ent, hand, move->hands[h].throwVel, move->headPos, radius, &st);
            if(debug() && vr_climb_debug.value >= 3.f)
            {
                Con_Printf("climb: lenient search: %d points in %.3f ms\n", st.points, (Sys_DoubleTime() - t0) * 1000.0);
            }
            how = ledge ? ", lenient" : "";
        }
        if(!ledge)
        {
            if(debug())
            {
                Con_Printf("climb: %s hand at (%.1f %.1f %.1f): no hold\n", handName(h), hand.x, hand.y, hand.z);
            }
            continue;
        }

        g.active = true;
        g.owned = true;
        g.serial++;
        g.hold = g.lastHold = ledge->hold;
        g.relAtGrab = g.lastRel = handRel(ent, *move, h);
        g.ledge = *ledge;
        g.entNum = ledge->ent ? NUM_FOR_EDICT(ledge->ent) : 0;
        g.entOrigin = ledge->ent ? vec(ledge->ent->v.origin) : glm::vec3{0.f};
        c.lastOrigin = vec(ent->v.origin);
        server::sendHaptic(ent, h, 0.f, 0.06f, 80.f, 0.6f);
        if(debug())
        {
            Con_Printf("climb: %s hand holds at %.1f (out %.2f %.2f; hand %.1f %.1f %.1f, hold %.1f %.1f %.1f) from (%.1f %.1f %.1f)%s%s\n",
                handName(h), ledge->top, ledge->out.x, ledge->out.y, hand.x, hand.y, hand.z, ledge->hold.x, ledge->hold.y,
                ledge->hold.z, ent->v.origin[0], ent->v.origin[1], ent->v.origin[2], hanging ? ", both hands" : "", how);
        }
    }

    // Let go of everything: fall, flung by the hands' release.
    if(wasHanging && !c.hanging() && !c.mantling)
    {
        c.noGrabUntil = time + regrabDelay;
        glm::vec3 fling{0.f};
        if(move)
        {
            const int hands = (held[0] ? 1 : 0) + (held[1] ? 1 : 0);
            for(int h = 0; h < 2; h++)
            {
                if(held[h])
                {
                    fling -= move->hands[h].throwVel / static_cast<float>(hands);
                }
            }
            fling *= units::metresToUnits() * std::max(0.f, vr_climb_fling.value);
            fling.z = std::min(fling.z, flingMaxUp);
            if(glm::length(fling) > flingMax)
            {
                fling *= flingMax / glm::length(fling);
            }
        }
        setVec(ent->v.velocity, fling);
        if(debug())
        {
            Con_Printf("climb: falls, flung at (%.0f %.0f %.0f)\n", fling.x, fling.y, fling.z);
        }
    }
    if(enabled && !c.hanging() && !c.mantling)
    {
        traceFrame(ent, c, move, "free", glm::vec3{0.f}, glm::vec3{0.f});
    }

    // The QC: the holding hands' grips hidden; which hands hold.
    const int vrbits = fields().vrbits0;
    if(vrbits >= 0)
    {
        int bits = static_cast<int>(fieldFloat(ent, vrbits));
        for(int h = 0; h < 2; h++)
        {
            const Grip& g = c.grips[h];
            if(g.owned)
            {
                bits &= ~grabBit[h];
            }
            if(g.owned || g.ownedLastFrame)
            {
                bits &= ~prevGrabBit[h];
            }
            bits = g.active ? bits | climbingBit[h] : bits & ~climbingBit[h];
        }
        fieldFloat(ent, vrbits) = static_cast<float>(bits);
    }
}

// SV_Physics_Client, in place of the move: the body hangs from the hands, or mantles.
// 0: not climbing (move as usual); 1: moved; -1: the entity was freed by its think.
extern "C" int VR_ClientClimb(edict_t* ent)
{
    Climber* cp = climberOf(ent);
    if(!cp || (!cp->hanging() && !cp->mantling))
    {
        return 0;
    }
    Climber& c = *cp;
    if(!SV_RunThink(ent))
    {
        return -1;
    }

    const double time = qcvm->time;
    if(c.mantling)
    {
        const float up = glm::distance(c.mantleFrom, c.mantleMid);
        const float over = glm::distance(c.mantleMid, c.mantleTo);
        const float t = CLAMP(0.f, static_cast<float>((time - c.mantleStart) / mantleTime), 1.f);
        const float along = t * (up + over);
        const glm::vec3 pos = along <= up && up > 0.f
                                  ? glm::mix(c.mantleFrom, c.mantleMid, along / up)
                                  : glm::mix(c.mantleMid, c.mantleTo, over > 0.f ? (along - up) / over : 1.f);
        setVec(ent->v.origin, pos);
        setVec(ent->v.velocity, glm::vec3{0.f});
        ent->v.flags = static_cast<float>(static_cast<int>(ent->v.flags) & ~FL_ONGROUND);
        if(t >= 1.f)
        {
            c.mantling = false;
            c.noGrabUntil = time + regrabDelay;
            ent->v.flags = static_cast<float>(static_cast<int>(ent->v.flags) | FL_ONGROUND);
            if(debug())
            {
                Con_Printf("climb: mantled onto (%.1f %.1f %.1f)\n", pos.x, pos.y, pos.z);
            }
        }
        c.lastOrigin = pos;
        SV_CheckWater(ent);
        return 1;
    }

    server::rebaseHands(ent);
    const VrMove* move = server::clientMove(ent);
    if(!move)
    {
        letGoAll(c);
        return 0;
    }

    // The pull: what the holding hands moved relative to the body since the last frame, the other way, each weighted
    // by how far it moved; and the holds' own motion (a moving brush model carries the body).
    const glm::vec3 rel[2] = {handRel(ent, *move, 0), handRel(ent, *move, 1)}; // before the body moves
    glm::vec3 pull{0.f}, carry{0.f};
    float weights = 0.f;
    float top = -1e9f;
    int count = 0;
    for(int h = 0; h < 2; h++)
    {
        const Grip& g = c.grips[h];
        if(!g.active)
        {
            continue;
        }
        const glm::vec3 moved = rel[h] - g.lastRel;
        const float w = glm::length(moved);
        if(std::isfinite(w))
        {
            pull += moved * w;
            weights += w;
        }
        carry += holdNow(g) - g.lastHold;
        top = std::max(top, topNow(g));
        count++;
    }
    const glm::vec3 wanted = carry / static_cast<float>(count) - (weights > 1e-6f ? pull / weights : glm::vec3{0.f});

    // The limits, each stopping the body only from going further past it: the feet no higher than the highest hold
    // (the mantle does that), the body within reach of each hold.
    const glm::vec3 origin = vec(ent->v.origin);
    glm::vec3 target = origin + wanted;
    const float highest = top - ent->v.mins[2] + 2.f;
    if(target.z > highest)
    {
        target.z = std::max(highest, std::min(target.z, origin.z));
    }
    for(int pass = 0; pass < 2; pass++)
    {
        for(const Grip& g : c.grips)
        {
            if(!g.active)
            {
                continue;
            }
            const glm::vec3 hold = holdNow(g);
            glm::vec2 fromHold{target.x - hold.x, target.y - hold.y};
            const float reach = std::max(maxHangReach, glm::length(glm::vec2{origin.x - hold.x, origin.y - hold.y}));
            if(glm::length(fromHold) > reach)
            {
                fromHold *= reach / glm::length(fromHold);
                target.x = hold.x + fromHold.x;
                target.y = hold.y + fromHold.y;
            }
        }
    }

    glm::vec3 step = target - origin;
    const float maxStep = maxHangSpeed * static_cast<float>(host_frametime);
    if(glm::length(step) > maxStep)
    {
        step *= maxStep / glm::length(step);
    }
    moveBody(ent, origin + step);
    setVec(ent->v.velocity, glm::vec3{0.f});
    ent->v.flags = static_cast<float>(static_cast<int>(ent->v.flags) & ~FL_ONGROUND);
    c.lastOrigin = vec(ent->v.origin);
    for(int h = 0; h < 2; h++)
    {
        Grip& g = c.grips[h];
        g.lastRel = rel[h];
        g.lastHold = holdNow(g);
    }
    SV_CheckWater(ent);
    traceFrame(ent, c, move, count > 1 ? "hang2" : "hang1", wanted, vec(ent->v.origin) - origin);

    // The mantle: pulled down, the head over the ledge, and room on top.
    const float headZ = move->headPos.z;
    for(int h = 0; h < 2; h++)
    {
        const Grip& g = c.grips[h];
        if(!g.active)
        {
            continue;
        }
        const float pulled = g.relAtGrab.z - rel[h].z;
        const float headAbove = headZ + (ent->v.origin[2] - origin.z) - topNow(g);
        if(pulled < mantlePull || headAbove < mantleHead)
        {
            continue;
        }
        glm::vec3 mid, to;
        if(!findMantle(ent, g, mid, to))
        {
            if(debug() && vr_climb_debug.value >= 3.f)
            {
                Con_Printf("climb: no room to mantle\n");
            }
            continue;
        }
        c.mantling = true;
        c.mantleStart = time;
        c.mantleFrom = vec(ent->v.origin);
        c.mantleMid = mid;
        c.mantleTo = to;
        c.grips[0].active = c.grips[1].active = false;
        if(debug())
        {
            Con_Printf("climb: mantle from (%.1f %.1f %.1f) up to %.1f, onto (%.1f %.1f %.1f)\n", c.mantleFrom.x,
                c.mantleFrom.y, c.mantleFrom.z, mid.z, to.x, to.y, to.z);
        }
        break;
    }

    return 1;
}

namespace
{

// vr_climb_probe [yaw]: the holds in front of the player, or towards `yaw` (debugging: where a hand could take hold).
void probe(edict_t* ent, float yaw)
{
    vec3_t fwd, right, up;
    vec3_t yawOnly{0.f, yaw, 0.f};
    AngleVectors(yawOnly, fwd, right, up);
    const glm::vec3 origin = vec(ent->v.origin);
    const float feet = origin.z + ent->v.mins[2];
    int found = 0;
    for(float d = 16.f; d <= 64.f && found < 12; d += 4.f)
    {
        for(float z = feet + 16.f; z <= feet + 128.f && found < 12; z += 4.f)
        {
            const glm::vec3 p{origin.x + fwd[0] * d, origin.y + fwd[1] * d, z};
            if(const std::optional<Ledge> l = findLedge(ent, p))
            {
                Con_Printf("hold: hand (%.0f %.0f %.0f) top %.1f (%.0f above the feet) out (%.2f %.2f) at (%.1f %.1f %.1f)\n",
                    p.x, p.y, p.z, l->top, l->top - feet, l->out.x, l->out.y, l->hold.x, l->hold.y, l->hold.z);
                found++;
                z = l->top + surfaceBelow; // the next hold up
            }
        }
    }
    if(!found)
    {
        Con_Printf("no hold within 64 units ahead\n");
    }
}

// vr_climb_try <x> <y> <z> [off|main] [<vx> <vy> <vz>]: what a grip of the main (or off) hand at that point would take,
// with the current settings, the head as it is and the hand's motion as it is (or that velocity, m/s): the leniency's
// tests. Takes nothing.
void try_f()
{
    if(Cmd_Argc() < 4 || !sv.active || svs.maxclients < 1 || !svs.clients[0].active || !svs.clients[0].edict)
    {
        Con_Printf("vr_climb_try <x> <y> <z> [off|main] [<vx> <vy> <vz>]\n");
        return;
    }
    qcvm_t* oldvm = nullptr;
    PR_PushQCVM(&sv.qcvm, &oldvm);
    edict_t* ent = svs.clients[0].edict;
    const VrMove* move = server::clientMove(ent);
    const int h = Cmd_Argc() > 4 && !q_strcasecmp(Cmd_Argv(4), "off") ? 0 : 1;
    const glm::vec3 hand{Q_atof(Cmd_Argv(1)), Q_atof(Cmd_Argv(2)), Q_atof(Cmd_Argv(3))};
    const glm::vec3 head = move ? move->headPos : vec(ent->v.origin) + vec(ent->v.view_ofs);
    const glm::vec3 vel = Cmd_Argc() > 7 ? glm::vec3{Q_atof(Cmd_Argv(5)), Q_atof(Cmd_Argv(6)), Q_atof(Cmd_Argv(7))}
                          : move ? move->hands[h].throwVel : glm::vec3{0.f};
    const float feet = ent->v.origin[2] + ent->v.mins[2];
    std::optional<Ledge> l = findLedge(ent, hand);
    const char* how = "exact";
    if(l && l->top < feet + vr_climb_min_height.value)
    {
        l.reset();
    }
    LenientStats st;
    double ms = 0.0;
    if(!l && leniencyUnits() > 0.f)
    {
        const double t0 = Sys_DoubleTime();
        l = findLenient(ent, hand, vel, head, leniencyUnits(), &st);
        ms = (Sys_DoubleTime() - t0) * 1000.0;
        how = "lenient";
    }
    if(l)
    {
        Con_Printf("climbtry %.2f %.2f %.2f %s: %s hold %.2f %.2f %.2f top %.1f out %.2f %.2f, %.2f units from the hand (%.1f cm)",
            hand.x, hand.y, hand.z, handName(h), how, l->hold.x, l->hold.y, l->hold.z, l->top, l->out.x, l->out.y,
            glm::distance(l->hold, hand), glm::distance(l->hold, hand) / units::metresToUnits() * 100.f);
    }
    else
    {
        Con_Printf("climbtry %.2f %.2f %.2f %s: none (leniency %.1f cm)", hand.x, hand.y, hand.z, handName(h),
            vr_climb_leniency.value);
    }
    Con_Printf("; %d points, %d holds seen, turned down: %d low, %d below, %d far, %d behind, %d through a wall; %.3f ms\n",
        st.points, st.found, st.low, st.below, st.tooFar, st.behind, st.unseen, ms);
    PR_PopQCVM(oldvm);
}

void probe_f()
{
    if(!sv.active || svs.maxclients < 1 || !svs.clients[0].active || !svs.clients[0].edict)
    {
        return;
    }
    qcvm_t* oldvm = nullptr;
    PR_PushQCVM(&sv.qcvm, &oldvm);
    edict_t* ent = svs.clients[0].edict;
    probe(ent, Cmd_Argc() > 1 ? Q_atof(Cmd_Argv(1)) : ent->v.angles[1]);
    PR_PopQCVM(oldvm);
}

// Client side: each hand's drawn place eased onto its hold, from one hold to the next, and off it again.
struct Pin
{
    float weight{0.f};     // 0: the tracked hand, 1: on the hold
    int serial{-1};        // the hold's (a new hold: glide from where the hand is drawn)
    glm::vec3 from{0.f}, to{0.f};
    float along{1.f};      // from `from` to `to`
    float inTime{0.06f};   // seconds this hold's ease takes (longer for a hold further from the hand)
    glm::vec3 offset{0.f}; // the looks' offset on the hold (vr_climb_hand_*)
    double last{-1.0};
    [[nodiscard]] glm::vec3 hold() const
    {
        return glm::mix(from, to, along * along * (3.f - 2.f * along));
    }
};
Pin pins[2];

constexpr float pinInTime = 0.06f;  // seconds the drawn hand takes to settle on its hold (or glide to a new one),
constexpr float pinInMax = 0.2f;    // at most, for a hold further than
constexpr float pinInSpeed = 2.f;   // m/s allows (a lenient grab's far hold: no pop)
constexpr float pinOutTime = 0.12f; // and to go back to the tracked hand after letting go
constexpr float yawSteps = 256.f;   // the holds' ways out in the stats: a byte of yaw each

// The time the drawn hand takes to ease `dist` units.
[[nodiscard]] float easeTime(float dist)
{
    return CLAMP(pinInTime, dist / (pinInSpeed * units::metresToUnits()), pinInMax);
}

// The drawn hand's place on a hold (see the top of the file: vr_climb_hand_*, cm), from the hold's way out (towards the
// drop, horizontal): towards the player, up, and along the edge outwards (away from the other hand).
[[nodiscard]] glm::vec3 handOffset(const glm::vec3& out, int hand)
{
    const float cm = 0.01f * units::metresToUnits();
    const bool right = (hand == 1) == (vr_lefthanded.value == 0.f);
    const glm::vec3 rightward{-out.y, out.x, 0.f}; // facing the ledge (looking along -out), to the right
    return (out * vr_climb_hand_out.value + glm::vec3{0.f, 0.f, vr_climb_hand_up.value} +
               rightward * (right ? vr_climb_hand_side.value : -vr_climb_hand_side.value)) *
           cm;
}

} // namespace

void qvr::climb::init()
{
    Cmd_AddCommand("vr_climb_probe", probe_f);
    Cmd_AddCommand("vr_climb_try", try_f);
}

void qvr::climb::calcStats(edict_t* ent, int* statsi)
{
    using namespace protocol;
    const Climber* c = climberOf(ent);
    int bits = 0;
    for(int h = 0; h < 2; h++)
    {
        const int first = h ? STAT_QVR_CLIMBMAINX : STAT_QVR_CLIMBOFFX;
        glm::vec3 hold{0.f};
        if(c && c->grips[h].active)
        {
            const glm::vec3& out = c->grips[h].ledge.out;
            const int yaw = static_cast<int>(std::lround(std::atan2(out.y, out.x) / glm::two_pi<float>() * yawSteps)) & 255;
            bits |= (1 << h) | ((c->grips[h].serial & 63) << (2 + 6 * h)) | (yaw << (14 + 8 * h));
            hold = holdNow(c->grips[h]);
        }
        for(int i = 0; i < 3; i++)
        {
            statsi[first + i] = static_cast<int>(std::lround(hold[i] * statScale));
        }
    }
    statsi[STAT_QVR_CLIMB] = bits;
}

void qvr::climb::drawnHand(const hands::State& s, int hand, glm::vec3& pos, glm::vec3& lightShift)
{
    using namespace protocol;
    lightShift = glm::vec3{0.f};
    if(hand < 0 || hand > 1)
    {
        return;
    }
    Pin& pin = pins[hand];
    const float dt = pin.last >= 0.0 ? static_cast<float>(CLAMP(0.0, realtime - pin.last, 0.1)) : 0.f;
    pin.last = realtime;
    const int bits = cl.stats[STAT_QVR_CLIMB];
    if(bits & (1 << hand))
    {
        const int first = hand ? STAT_QVR_CLIMBMAINX : STAT_QVR_CLIMBOFFX;
        const float yaw = static_cast<float>((bits >> (14 + 8 * hand)) & 255) / yawSteps * glm::two_pi<float>();
        pin.offset = handOffset(glm::vec3{std::cos(yaw), std::sin(yaw), 0.f}, hand);
        const glm::vec3 hold = glm::vec3{static_cast<float>(cl.stats[first]), static_cast<float>(cl.stats[first + 1]),
                                   static_cast<float>(cl.stats[first + 2])} /
                                   statScale +
                               pin.offset;
        const int serial = (bits >> (2 + 6 * hand)) & 63;
        if(pin.weight <= 0.f)
        {
            pin.from = pin.to = hold;
            pin.along = 1.f;
            const float dist = glm::distance(hold, hands::palmPoint(s, hand));
            pin.inTime = easeTime(dist);
            if(vr_climb_debug.value >= 3.f)
            {
                Con_Printf("climb: drawn %s hand eases onto its hold in %.3f s (%.1f cm)\n", handName(hand), pin.inTime,
                    dist / units::metresToUnits() * 100.f);
            }
        }
        else if(serial != pin.serial)
        {
            pin.from = pin.hold(); // taken again before the hand was back: on to the new hold
            pin.along = 0.f;
            pin.inTime = easeTime(glm::distance(hold, pin.from));
        }
        pin.to = hold; // (a hold on a moving brush model moves)
        pin.serial = serial;
        pin.along = std::min(1.f, pin.along + dt / pin.inTime);
        pin.weight = std::min(1.f, pin.weight + dt / pin.inTime);
    }
    else
    {
        pin.weight = std::max(0.f, pin.weight - dt / pinOutTime);
        pin.along = std::min(1.f, pin.along + dt / pinInTime);
    }
    if(pin.weight <= 0.f)
    {
        return;
    }
    // The palm's middle on the hold.
    const glm::vec3 pinned = pin.hold() - (hands::palmPoint(s, hand) - s.pos[hand]);
    const float w = pin.weight * pin.weight * (3.f - 2.f * pin.weight);
    pos = glm::mix(pos, pinned, w);
    lightShift = -w * pin.offset;
}
