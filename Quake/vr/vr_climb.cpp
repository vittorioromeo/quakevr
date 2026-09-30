// vr_climb.cpp -- climbing: holds taken with either hand or both, hand over hand, shimmying and mantling (vr_climb,
// experimental; server side, and the drawn hands on their holds, client side).
//
// Holds. An empty hand can only take hold of a LEDGE of the ledge map (vr_ledges.cpp, made once from the BSP for each
// brush model: the lip of a walkable top with room over it for the hand and a drop of at least ledges::minDrop beyond
// it, starting at most ledges::edgeReach out, steps down on the way passed over: a ledge, a rung, a beam, a trimmed
// lip; not open floor, a wall or a stair). The hand is AT a ledge when it touches it (Touch: `touchCm`, the only
// distance there is at Grab Leniency 0): along the lip, between the hold's inset and the touch under its top (sunk into
// it, or into a rung) and the touch over it, and either over the top, in from the lip no further than the top goes and
// the drop is within edgeReach (edgeReach less how far out the drop starts), or in front of the face under the lip, at
// most the touch out, when the lip faces the body (the hand between the body and the ledge). The hand is the drawn
// hand's point, or the controller's own where the level holds the drawn hand out of it (gripHold: a hand reaching up
// under a rung stops with its fingers against the rung's underside).
// Of the ledges the hand is at, it takes (takenBefore) one it is over the top of before one it is in front of,
// the higher top (a rung above before the one below), the lip facing the body more (a rung's front, not its end; the
// near side of a thin wall), the nearer hold. The grip must be pressed at the hold (a press elsewhere, dragged onto
// one, does nothing), with no weapon, carried object, locked force grab or flashlight in that hand, and not at a
// holster that holds a weapon while standing (the grip draws it; an empty holster, or any while hanging, gives way to
// the hold). The hold is on the lip's line: the hand's place along it, on the top, `holdInset` units in from the
// lip; the hand is drawn there (its palm's middle, moved by vr_climb_hand_out/up/side: display only) while it holds,
// whatever the tracked hand does, and turned to face the hold (the knuckles towards the ledge and tilted up over its
// lip by vr_climb_hand_pitch/yaw/roll, mirrored for the off hand; vr_climb_hand_turn_blend of the way to the
// controller's turn: display only).
//
// Checks when a hold is taken (check(): the ledge map has each model's own solid only): room for the hand over the
// hold now (nothing else over it: a door, another brush), the drop still there where a brush model is near (a plat
// level with the floor is no ledge), and the top reached from where the hand is: over the top, a line from
// the sink depth over the hand down to the top is clear (the hand may be sunk into the ledge, not under something
// else); in front of the face, a line up from the hand to just over the top and in over the lip is (no fence between).
//
// Leniency (vr_climb_leniency, cm: the one control of how far a grab reaches; 0: touching only). A grip at no ledge
// takes the nearest one it is within that far of being at: the
// point of the place above nearest the hand, where that ledge would be the one taken (not where another comes first:
// the place less the others' that come first, freePoint). The hold is at that point's place along the lip. Of those, the
// one taken is the nearest to the hand (a hold the hand moves or reaches towards counts as up to `reachFavour` nearer),
// not behind the head (on the far side from the hand), not lower under the hand than the touch and the leniency reach,
// at most the leniency plus `lenientReach` from the hand, passes the checks above from that point,
// and is in sight of the hand (a line from the hand, or from the head for a hand in a wall, to just over the lip). A
// hold the hand is at always wins: leniency only adds holds where there were none. Nothing moves the body when a hold
// is taken (the pull is the hands' motion since); the drawn hand eases onto it (slower the further it is).
//
// Holds on brush models (plats, trains, doors). The hold is kept as the model's entity number and its place in the
// model's space; each frame it is where the model is now (Quake's traces move a brush model's collision with its
// origin, never its angles, so the holds do too; the mission packs' rotating brushes collide as translating
// func_movewall boxes). The player hanging from one rides it in SV_PushMove as one standing on it does, in the same push,
// exactly (VR_ClimbHangsFrom, VR_ClimbCarried). A ride that the body can't make (carried into something) makes the
// hands on it let go (vr_climb_mover_crush 0), or blocks the mover as anything in its way does (1: Quake's rules, its
// blocked function: a train's damage, a door or plat going back; the hands hold on) (VR_ClimbCarryBlocked). A hold
// lets go when its model is freed, stops being solid or changes model. A mantle onto one follows it (its way in the
// model's space) and ends standing on it (its groundentity).
//
// Hanging. While a hand holds, the player hangs: no gravity, no stick, no room-scale walk. Each hand holds on to its
// hold: the body moves by the hands' pull, what the holding hands moved relative to the body since the last frame,
// the other way (pulling a hand down lifts the body). Two holding hands pull together, each weighted by how far it
// moved: the hand that pulls carries the body, a hand held still is not dragged back against it, and two hands
// pulling alike move it by their average. A hand taking hold or letting go changes only which hands pull, never where
// the body is: hand over hand, up, down or sideways, with no jump at the hand-off. A hold on a moving brush model
// carries the body with it (above). The body moves through the world by player-box traces (sliding along what blocks it), at
// most `maxHangSpeed` units a second, and neither rises with its feet above the highest hold nor strays more than
// `maxHangReach` units (horizontally) from any hold; each limit only stops the body going further past it (letting
// go of the higher hand never drops the body back under the lower one's limit).
//
// Staying within reach. The body stays within an arm's reach of each hold: from the shoulder (the server's estimate:
// the head's neck pivot, the shoulders under it and out to the side, as the arm IK poses the default body standing,
// moved by Body Calibration's shoulders) to the hold, at most the arm (Body Calibration's measured upper arm and forearm,
// or the default body's times Arm Length, with the tweaks), the wrist to the palm's middle, Shoulder Reach and 5 cm.
// A hold taken further (a hand reaching past the model's arm, a lenient hold) may stay that far until the body comes
// closer. Pushing away from the wall moves the body back until the arms are straight, no further. A hold the body is
// pulled away from by the other hand, while its own hand hardly moves (under `passiveShare` of the pull), is torn off
// instead (it lets go: hand over hand, the lower hand needn't be let go of by the player before the pull); a hold the
// body couldn't be kept within reach of (the body blocked) lets go too. The pull into the face under the holds that the
// body couldn't make (it is against the face) is owed: a later pull away from the face makes it up first, before the
// body moves (pull into the wall, then push: the body leaves the wall only once the hands are back where the arms met
// it, as a real body would). Only that: a hand raised while standing on the floor, or pulled down past the highest
// hold, owes nothing (the next pull moves the body at once). It never moves the body by itself (no pop).
//
// Under an overhang (a floating platform, a slab with nothing under its lip): the box stands far higher over the
// shoulders and further in front of the eyes than a head and a chest, so a body hanging with its box under the
// platform can't get out from under it within an arm's reach (out means up, round the hold, and up is into the
// platform). When the way back within reach is blocked straight up, below a hold above the box, the arms stretch
// instead, up to vr_climb_overhang_stretch units past the reach: pushing away moves the body out from under the
// platform (still holding), and the reach comes back as the body comes closer (as a hold taken further does); pushing
// down then mantles as anywhere else.
//
// The hands in sync along the ledge (vr_climb_slide): the drawn hand is on its hold, the tracked hand where the body
// carries it; they stay together as long as the body follows the pull. Where it can't (the arm's reach, the 48 units,
// a wall beside the body, the other hand pulling it along while this one is held still), the hold slides along its lip
// to keep the tracked hand as far from it as when it took hold, within the lip's ends and no further from the shoulder
// than the reach. Past the reach the tracked hand still gets ahead of the drawn one (no arm reaches it), but only by
// how far it is past the reach: coming back, the hold slides with it until they meet, instead of the two staying apart.
// Only along the lip: the reach's pull up and in (the body swung round the hold) and a pull down past the highest hold
// still leave the tracked hand that much lower or further in than the drawn one.
//
// Mantle: once a hand has pulled down by `mantlePull` units since it took hold and the head is `mantleHead` units
// above its ledge, and the player's box fits on top (with floor under it) and the way there (straight up, then over)
// is clear, the body is carried there in `mantleTime` seconds, and stands. The ledge map knows how deep the top is at
// each place along the lip: a spot whose middle is over the top needs only the sweep over to it (and the sweep up,
// shared by the spots on its side); elsewhere the traces below the box as before. The spot is 22 to 38 units in from the lip;
// on a narrower top (a wall's, a beam's: at least `minFooting` deep), over its middle, or further out, as long as the
// box's middle is over it (it stands on the top, not on its edge over the drop). With no room on top (a ceiling, a top
// too thin), it hangs on, and the holding hands feel a soft buzz as the pull over the top starts; the push over the top
// that follows doesn't push it back off the face (the pull that brought the hands in was owed, above), and at most
// straightens the arms.
//
// Letting go of everything: the player falls, flung by the hands' release (vr_climb_fling, capped). No new hold for
// `regrabDelay` seconds after letting go of everything or mantling; holds lower than vr_climb_min_height above the
// feet are ignored.
//
// The QC never sees the grip of a hand that holds (its grab bits are masked in .vrbits0 from the press until the
// release), so the press picks nothing up; instead .vrbits0 bits 15 (off hand) and 16 (main hand) tell it the hand
// holds (QVR_VRBITS0_*HAND_CLIMBING: no melee blows). The client gets the holds as stats (STAT_QVR_CLIMB*).

#include "vr_climb.hpp"
#include "vr_avatar.hpp"
#include "vr_bodycal.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_hands.hpp"
#include "vr_ledges.hpp"
#include "vr_mem.hpp"
#include "vr_move.hpp"
#include "vr_physsound.hpp"
#include "vr_progs.hpp"
#include "vr_protocol.hpp"
#include "vr_server.hpp"
#include "vr_units.hpp"

#include <algorithm>
#include <functional>
#include <cmath>
#include <optional>
#include <string>
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
constexpr int HS_GRENADE_POUCH = 11;

constexpr float touchCm = 4.5f;       // cm: the hand's point this close to a ledge touches it (Grab Leniency 0)
using ledges::holdInset;              // the hold (the palm's middle) is this far behind the lip (vr_ledges.hpp)
constexpr float regrabDelay = 0.4f;   // seconds without new holds after letting go of everything or mantling
constexpr float maxHangSpeed = 500.f; // units / second the body follows the hands at
constexpr float maxHangReach = 48.f;  // the body stays this close (horizontally) to each hold
constexpr float mantlePull = 8.f;     // units a hand must have pulled down before a mantle
constexpr float mantleHead = 8.f;     // and the head must be this far above the ledge
constexpr float mantleTime = 0.3f;    // seconds the mantle takes
constexpr float flingMax = 200.f;     // units / second the release flings at most,
constexpr float flingMaxUp = 150.f;   // and upwards
constexpr float statScale = 8.f;      // hold coordinates in the stats: eighths of a unit
constexpr float reachFavour = 0.3f;   // a hold the hand moves (or reaches) towards counts as this much nearer
constexpr float movingHand = 0.3f;    // m/s: a hand this fast reaches the way it moves (slower: from the head)
// The arms' reach (see "Staying within reach" at the top of the file), real metres of the default body (times its scale):
constexpr float eyeToNeckFwd = 0.08f;  // the neck's pivot behind the eyes (along the head's forward),
constexpr float eyeToNeckDown = 0.08f; // and under them (along the head's up)
constexpr float neckToShoulderDown = 0.15f; // the shoulder joints under the pivot,
constexpr float neckToShoulderOut = 0.19f;  // and out to the side (the model's, as the arm IK poses them standing)
constexpr float palmReach = 0.08f;     // the wrist to the palm's middle (the hold is where the palm's middle goes)
constexpr float reachSlack = 0.05f;    // and a little more (the estimate's error; the drawn arm stretches more)
constexpr float passiveShare = 0.25f;  // a holding hand moving less than this share of the pull doesn't hold the body back
constexpr float passiveSlack = 2.f;    // units: such a hand's hold that far past its reach lets go,
constexpr float brokenSlack = 4.f;     // and any hold this far past it (the body couldn't be kept within reach)
constexpr float minFooting = 3.f;      // units of a ledge's top (from the lip in) the box must stand on to mantle onto it
constexpr double mantleRetry = 0.15;   // seconds a search for the mantle that found no room holds (the body and hold still),
constexpr float mantleRetryMove = 1.f; // unless the body moved this far

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

// Traces made by the searches here (vr_climb_debug 3 and 4, vr_climb_try: their cost).
int traceCount = 0;

[[nodiscard]] trace_t traceBox(const glm::vec3& start, const glm::vec3& mins, const glm::vec3& maxs,
    const glm::vec3& end, int type, edict_t* pass)
{
    traceCount++;
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
    float top{0.f};             // the surface's height (at the hold)
    glm::vec3 out{0.f};         // horizontal, from the ledge towards the drop
    glm::vec3 hold{0.f};        // where the hand holds: on the edge's line (see the top of the file)
    edict_t* ent{nullptr};      // the brush model it belongs to (nullptr: the world)
    const ledges::Map* map{nullptr}; // its ledge in the ledge map: the model's map, the ledge,
    int edge{-1};
    float t{0.f};                    // and the hold's place along it
};

// How near a ledge the hand's point must be to be AT it (Grab Leniency 0; the leniency adds its distance round that):
// touching it, `touchCm` from its top or from the face under its lip, or in it (sunk into its top, or into a rung) no
// deeper than the hold's inset and that; over the top as far in as the ledge map's ledge goes (edgeReach from the
// drop).
struct Touch
{
    float over{0.f};  // above the top
    float front{0.f}; // in front of the face under the lip (that faces the body)
    float sink{0.f};  // under the top (sunk into the ledge or a rung, in front of its face)
};

[[nodiscard]] Touch touch()
{
    const float t = touchCm * 0.01f * units::metresToUnits();
    return Touch{t, t, holdInset + t};
}

// Where a hand (in the ledge's model's space) is from the place it takes a hold on ledge `e` from (see the top of the
// file: over the top within reach of the drop, or in front of the face under the lip).
struct Place
{
    float dist{0.f};        // how far from it (0: in it)
    bool over{false};       // over the top (else in front of the face)
    float top{0.f};         // the top's height at the hold
    float facing{0.f};      // how much the lip faces the body (1: straight at it)
    float t{0.f};           // the hold's place along the ledge
    glm::vec3 hold{0.f};    // the hold
    float holdDist{0.f};    // from the hand
    glm::vec3 nearest{0.f}; // the place's point nearest the hand
};

// `toBody`: from the hand to the body, horizontal (unit, or zero).
[[nodiscard]] Place placeOf(const ledges::Map& m, const ledges::Edge& e, const glm::vec3& hand, const glm::vec2& toBody)
{
    Place p;
    const float raw = glm::dot(hand - e.a, e.dir);
    p.t = std::clamp(raw, 0.f, e.len);
    const glm::vec3 lip = e.point(p.t);
    const ledges::Sample& s = m.sampleAt(e, p.t);
    const glm::vec2 out{e.out};
    p.facing = glm::dot(out, toBody);
    // In from the lip as far as the top goes and the drop is within reach; out in front of it when it faces the body.
    const Touch k = touch();
    const float in = std::max(0.f, std::min(s.depth, ledges::edgeReach - s.dropOut));
    const float front = p.facing > 0.f ? k.front : 0.f;
    const float across = glm::dot(glm::vec2{hand} - glm::vec2{lip}, out);
    const float across1 = std::clamp(across, -in, front);
    const glm::vec3 overAt = lip + e.out * std::min(across1, 0.f);
    const float surface = e.topAt(overAt); // the top under the hand (the lip's height in front of it)
    const float up = hand.z - surface;
    const float up1 = std::clamp(up, -k.sink, k.over);
    const float da = raw - p.t, ds = across - across1, dz = up - up1;
    p.dist = std::sqrt(da * da + ds * ds + dz * dz);
    p.over = across <= 0.f;
    p.hold = lip - e.out * ledges::holdInset;
    p.top = e.topAt(p.hold);
    p.hold.z = p.top + ledges::holdLift;
    p.holdDist = glm::distance(p.hold, hand);
    p.nearest = lip + e.out * across1;
    p.nearest.z = surface + up1;
    return p;
}

// Of two ledges a hand is at (their places, from the same point), whether `a` is the one it takes: over the top before
// in front of a face, the higher top (a rung above before the one below), the lip facing the body more (a rung's front,
// not its end; the near side of a thin wall), the nearer hold.
[[nodiscard]] bool takenBefore(const Place& a, const Place& b)
{
    if(a.over != b.over)
    {
        return a.over;
    }
    if(std::abs(a.top - b.top) > 0.01f)
    {
        return a.top > b.top;
    }
    if(std::abs(a.facing - b.facing) > 1e-3f)
    {
        return a.facing > b.facing;
    }
    return a.holdDist < b.holdDist;
}

// A ledge near the hand: its model (`ent`, nullptr the world; at `offset`: the world is its space plus that), and the
// hand's place from it.
struct Candidate
{
    const ledges::Map* map{nullptr};
    int edge{-1};
    edict_t* ent{nullptr};
    glm::vec3 offset{0.f};
    Place place;

    [[nodiscard]] const ledges::Edge& ledge() const
    {
        return map->edges[static_cast<size_t>(edge)];
    }
    [[nodiscard]] Ledge toLedge() const
    {
        Ledge l;
        l.top = place.top + offset.z;
        l.out = ledge().out;
        l.hold = place.hold + offset;
        l.ent = ent;
        l.map = map;
        l.edge = edge;
        l.t = place.t;
        return l;
    }
};

[[nodiscard]] glm::vec2 towardsBody(edict_t* player, const glm::vec3& hand)
{
    const glm::vec2 d{player->v.origin[0] - hand.x, player->v.origin[1] - hand.y};
    return glm::length(d) >= 1.f ? glm::normalize(d) : glm::vec2{0.f};
}

// A lenient candidate (findHold): its score (the lower the nearer), and where the hand would take it.
struct Scored
{
    float score;
    Candidate c;
    glm::vec3 at;
};

// The hold search's buffers (the server's frame: the main thread).
struct ClimbScratch
{
    std::vector<int> nearby;             // the ledges of one model near the hand (gather)
    std::vector<edict_t*> areaEdicts;    // the entities round the hand (gather)
    std::vector<Candidate> found;        // the ledges near the hand (findHold)
    std::vector<edict_t*> movers;        // the brush models round it (findHold)
    std::vector<const Candidate*> order; // those the hand is at, the first taken first (findHold)
    std::vector<Scored> scored;          // the lenient ones (findHold)
    auto members() { return std::tie(nearby, areaEdicts, found, movers, order, scored); }
};
mem::Scratch<ClimbScratch> scratch{"climb"};

// What the search tried and turned down (vr_climb_try).
struct LenientStats
{
    int points{0}, found{0}, low{0}, below{0}, tooFar{0}, behind{0}, unseen{0}, covered{0}, hidden{0};
};

// The ledges the hand at `hand` is within `radius` of being at (see the top of the file), in the world and the brush
// models round it (`movers`: those, SOLID_BSP, their boxes near).
void gather(edict_t* player, const glm::vec3& hand, float radius, std::vector<Candidate>& out, std::vector<edict_t*>& movers,
    LenientStats& st)
{
    const glm::vec2 toBody = towardsBody(player, hand);
    const glm::vec3 reach{radius + ledges::edgeReach + 1.f};
    const glm::vec3 lo = hand - reach, hi = hand + reach;
    std::vector<int>& nearby = scratch.nearby;
    const auto from = [&](const ledges::Map& m, edict_t* ent, const glm::vec3& offset) {
        nearby.clear();
        m.nearby(lo - offset, hi - offset, nearby);
        for(const int i : nearby)
        {
            st.points++;
            const Place p = placeOf(m, m.edges[static_cast<size_t>(i)], hand - offset, toBody);
            if(p.dist <= radius + 1e-3f)
            {
                out.push_back(Candidate{&m, i, ent, offset, p});
            }
        }
    };
    if(const ledges::Map* world = ledges::of(sv.worldmodel))
    {
        from(*world, nullptr, glm::vec3{0.f});
    }
    std::vector<edict_t*>& list = scratch.areaEdicts;
    list.resize(static_cast<size_t>(std::max(1, qcvm->num_edicts)));
    int count = 0;
    vec3_t mins{lo.x, lo.y, lo.z}, maxs{hi.x, hi.y, hi.z};
    SV_AreaEdicts(mins, maxs, list.data(), &count, static_cast<int>(list.size()));
    for(int i = 0; i < count; i++)
    {
        edict_t* e = list[static_cast<size_t>(i)];
        const int index = static_cast<int>(e->v.modelindex);
        if(e == player || e->free || static_cast<int>(e->v.solid) != SOLID_BSP || index <= 0 || index >= MAX_MODELS)
        {
            continue;
        }
        movers.push_back(e);
        if(const ledges::Map* m = ledges::of(sv.models[index]))
        {
            from(*m, e, vec(e->v.origin));
        }
    }
}

enum class Check
{
    ok,
    covered,   // no room over the hold, or no drop now (another model's solid: a door, the world round a plat)
    unreached, // the top not reached from where the hand takes it (another solid over it; a wall between the hand and
               // the lip)
    unseen,    // (a lenient hold) not in sight of the hand
};

// The checks made when a hold is taken (see the top of the file), with every model's solid (the ledge map has its own
// model's only):
// - the room over the hold (anything over it now, or a piece of the lip narrower than the map's samples) and the drop
//   (now: for the world's ledges, only where a brush model is near; the world's own was found when the map was made);
// - from `at`, where the hand takes it (the hand; for a lenient hold, the point of the place nearest it): over the top,
//   a line from the sink depth (Touch) over it down to the top, clear (nothing else over the top there: the hand may be
//   sunk into the ledge, or into a rung); in front of the face, a line up from it to just over the top, and from there in
//   over the lip (a hand under the lip reaches up past it; no fence between it and the face);
// - a lenient hold: in sight of the hand (a line from it, or from the head for a hand inside a wall, to just over the
//   lip: no grabbing through a wall).
[[nodiscard]] Check check(edict_t* player, const Candidate& c, const glm::vec3& at, bool lenient, const glm::vec3& hand,
    const glm::vec3& headPos, const std::vector<edict_t*>& movers)
{
    const ledges::Edge& e = c.ledge();
    const glm::vec3 up{0.f, 0.f, 1.f};
    const glm::vec3 hold = c.place.hold + c.offset;
    const float top = c.place.top + c.offset.z;
    const auto clear = [&](const glm::vec3& a, const glm::vec3& b) {
        const trace_t t = traceLine(a, b, player);
        return t.fraction >= 1.f && !t.startsolid;
    };
    const glm::vec3 onTop{hold.x, hold.y, top + 2.f};
    if(!clear(onTop, onTop + up * ledges::handRoom))
    {
        return Check::covered;
    }
    if(c.ent || !movers.empty())
    {
        const glm::vec3 lip = e.point(c.place.t) + c.offset;
        const glm::vec3 column = lip + e.out * c.map->sampleAt(e, c.place.t).dropOut;
        if(!clear(column + up * 2.f, glm::vec3{column.x, column.y, lip.z - ledges::minDrop}))
        {
            return Check::covered;
        }
    }
    if(c.place.over)
    {
        const float surface = e.topAt(at - c.offset) + c.offset.z;
        if(!clear(glm::vec3{at.x, at.y, std::max(at.z + touch().sink, surface + 0.5f)}, glm::vec3{at.x, at.y, surface + 0.25f}))
        {
            return Check::unreached;
        }
    }
    else
    {
        const glm::vec3 lip = e.point(c.place.t) + c.offset;
        const glm::vec3 over{at.x, at.y, std::max(at.z, lip.z + 1.f)};
        const glm::vec3 in = glm::vec3{lip.x, lip.y, over.z} - e.out * 1.f;
        if((over.z > at.z && !clear(at, over)) || !clear(over, in))
        {
            return Check::unreached;
        }
    }
    if(lenient)
    {
        const bool handInWall = traceLine(hand, hand, player).startsolid;
        const glm::vec3 lip = glm::vec3{hold.x, hold.y, top + 1.f} + e.out * (ledges::holdInset + 1.f);
        if(!clear(handInWall ? headPos : hand, lip))
        {
            return Check::unseen;
        }
    }
    return Check::ok;
}

// A candidate's place (placeOf) as a box in a ledge's frame: along the ledge from its end `a`, across it (out), and
// up from its lip's height; the place's depth in is taken where the hand's hold is (it varies along a ledge only where
// the top does).
struct Frame
{
    glm::vec3 origin, along, out;

    [[nodiscard]] glm::vec3 to(const glm::vec3& p) const
    {
        const glm::vec3 d = p - origin;
        return {glm::dot(d, along), glm::dot(d, out), d.z};
    }
    [[nodiscard]] glm::vec3 from(const glm::vec3& q) const
    {
        return origin + along * q.x + out * q.y + glm::vec3{0.f, 0.f, q.z};
    }
};

struct Box
{
    glm::vec3 lo, hi;
};

[[nodiscard]] Frame frameOf(const Candidate& c)
{
    const ledges::Edge& e = c.ledge();
    return Frame{e.a + c.offset, e.dir, e.out};
}

[[nodiscard]] Box boxOf(const Candidate& c)
{
    const ledges::Edge& e = c.ledge();
    const ledges::Sample& s = c.map->sampleAt(e, c.place.t);
    const Touch k = touch();
    const float in = std::max(0.f, std::min(s.depth, ledges::edgeReach - s.dropOut));
    return Box{{0.f, -in, -k.sink}, {e.len, c.place.facing > 0.f ? k.front : 0.f, k.over}};
}

// Where, of candidate `c`'s place, the hand at `hand` would take `c`'s ledge (no other ledge taken before it there,
// takenBefore), nearest the hand: the place less the others' places that come first, as boxes in `c`'s frame (those
// square or parallel to it: rungs above and below, a rung's front and its ends, a thin wall's two sides; any other is
// only checked at the point found). False: nowhere.
[[nodiscard]] bool freePoint(const Candidate& c, const std::vector<Candidate>& all, const glm::vec3& hand, const glm::vec2& toBody,
    glm::vec3& point)
{
    const ledges::Edge& e = c.ledge();
    const Frame f = frameOf(c);
    const auto placeAt = [&](const Candidate& k, const glm::vec3& world) {
        return placeOf(*k.map, k.ledge(), world - k.offset, toBody);
    };
    // Whether another ledge is taken before `c` at `world` (in its place).
    const auto takenFirst = [&](const Candidate& k, const glm::vec3& world) {
        const Place other = placeAt(k, world);
        return other.dist <= 1e-3f && takenBefore(other, placeAt(c, world));
    };
    struct Blocker
    {
        Box box;
        const Candidate* k;
    };
    std::vector<Blocker> blockers;
    const bool flat = std::abs(e.dir.z) < 1e-3f;
    if(flat)
    {
        for(const Candidate& k : all)
        {
            const ledges::Edge& ke = k.ledge();
            const float cosine = std::abs(glm::dot(ke.dir, e.dir));
            if(&k == &c || std::abs(ke.dir.z) >= 1e-3f || (cosine > 1e-3f && cosine < 1.f - 1e-3f))
            {
                continue;
            }
            const Frame kf = frameOf(k);
            const Box kb = boxOf(k);
            Box b{glm::vec3{1e9f}, glm::vec3{-1e9f}};
            for(int i = 0; i < 8; i++)
            {
                const glm::vec3 corner{(i & 1) ? kb.hi.x : kb.lo.x, (i & 2) ? kb.hi.y : kb.lo.y, (i & 4) ? kb.hi.z : kb.lo.z};
                const glm::vec3 q = f.to(kf.from(corner));
                b.lo = glm::min(b.lo, q);
                b.hi = glm::max(b.hi, q);
            }
            blockers.push_back(Blocker{b, &k});
        }
    }
    const glm::vec3 h = f.to(hand);
    float best = 1e30f;
    bool found = false;
    // The nearest point of `box` not in a blocker that comes first there: split round the first such blocker met.
    const std::function<void(const Box&, int)> search = [&](const Box& box, int depth) {
        const glm::vec3 p = glm::clamp(h, box.lo, box.hi);
        const float d = glm::distance(p, h);
        if(d >= best)
        {
            return;
        }
        const glm::vec3 world = f.from(p);
        for(const Blocker& b : blockers)
        {
            const bool inside = p.x > b.box.lo.x - 1e-3f && p.x < b.box.hi.x + 1e-3f && p.y > b.box.lo.y - 1e-3f &&
                                p.y < b.box.hi.y + 1e-3f && p.z > b.box.lo.z - 1e-3f && p.z < b.box.hi.z + 1e-3f;
            if(!inside || !takenFirst(*b.k, world))
            {
                continue;
            }
            if(depth >= 4)
            {
                return;
            }
            for(int axis = 0; axis < 3; axis++)
            {
                Box below = box, above = box;
                below.hi[axis] = std::min(box.hi[axis], b.box.lo[axis] - 2e-3f);
                above.lo[axis] = std::max(box.lo[axis], b.box.hi[axis] + 2e-3f);
                if(below.lo[axis] <= below.hi[axis])
                {
                    search(below, depth + 1);
                }
                if(above.lo[axis] <= above.hi[axis])
                {
                    search(above, depth + 1);
                }
            }
            return;
        }
        // Free of the boxes: checked against every other ledge's place itself.
        for(const Candidate& k : all)
        {
            if(&k != &c && takenFirst(k, world))
            {
                return;
            }
        }
        best = d;
        point = world;
        found = true;
    };
    if(flat)
    {
        search(boxOf(c), 0);
    }
    else
    {
        // (a sloping lip: its place's point nearest the hand)
        const glm::vec3 world = c.place.nearest + c.offset;
        found = std::none_of(all.begin(), all.end(), [&](const Candidate& k) { return &k != &c && takenFirst(k, world); });
        point = world;
    }
    return found;
}

// The hold a grip of the hand at `hand` takes (see the top of the file): one the hand is at, else (leniency, `radius`
// units) the nearest it is within `radius` of being at. `vel`: the hand's velocity (m/s); `headPos`: the head.
// `lenient`: whether the hold is a lenient one.
[[nodiscard]] std::optional<Ledge> findHold(edict_t* player, const glm::vec3& hand, const glm::vec3& vel,
    const glm::vec3& headPos, float radius, bool& lenient, LenientStats* stats = nullptr)
{
    LenientStats st;
    lenient = false;
    std::vector<Candidate>& found = scratch.found;
    std::vector<edict_t*>& movers = scratch.movers;
    found.clear();
    movers.clear();
    gather(player, hand, radius, found, movers, st);
    const float feet = player->v.origin[2] + player->v.mins[2];
    const float lowest = feet + vr_climb_min_height.value;
    const auto finish = [&](const Candidate* c) -> std::optional<Ledge> {
        if(stats)
        {
            *stats = st;
        }
        return c ? std::optional<Ledge>{c->toLedge()} : std::nullopt;
    };

    // At a hold: the one taken first (takenBefore) of those it is at, that passes the checks.
    std::vector<const Candidate*>& order = scratch.order;
    order.clear();
    for(const Candidate& c : found)
    {
        if(c.place.dist <= 1e-3f)
        {
            order.push_back(&c);
        }
    }
    std::sort(order.begin(), order.end(), [](const Candidate* a, const Candidate* b) { return takenBefore(a->place, b->place); });
    for(const Candidate* c : order)
    {
        if(c->place.top + c->offset.z < lowest)
        {
            st.low++;
            continue; // too low for a hold (Lowest Ledge)
        }
        if(check(player, *c, hand, false, hand, headPos, movers) == Check::ok)
        {
            return finish(c);
        }
    }
    if(radius <= 0.f)
    {
        return finish(nullptr);
    }

    // Leniency: the nearest of the others (a hold the hand moves or reaches towards counts as up to `reachFavour`
    // nearer).
    glm::vec3 reach = glm::length(vel) > movingHand ? vel : hand - headPos;
    reach = glm::length(reach) > 1e-3f ? glm::normalize(reach) : glm::vec3{0.f};
    const glm::vec2 side{hand.x - headPos.x, hand.y - headPos.y}; // the hand's side of the head
    const glm::vec2 toBody = towardsBody(player, hand);
    // A lenient hold is at most the leniency plus this from the hand: the furthest the hold is from a hand at the ledge
    // (in over the top as far as it reaches, or in front of its face, and under the lip as far as touching reaches).
    const Touch k = touch();
    const float lenientReach = std::hypot(std::max(ledges::edgeReach, holdInset + k.front), ledges::holdLift + k.sink) + 1e-3f;
    std::vector<Scored>& scored = scratch.scored;
    scored.clear();
    for(const Candidate& c : found)
    {
        if(c.place.dist <= 1e-3f)
        {
            continue; // (at it: turned down above)
        }
        st.found++;
        // Where in its place the hand would take it (not where another ledge comes first: the near side of a thin
        // wall, a rung's front at its end, the rung above), nearest the hand; the hold is there along the ledge.
        glm::vec3 at;
        if(!freePoint(c, found, hand, toBody, at) || glm::distance(at, hand) > radius + 1e-3f)
        {
            st.hidden++;
            continue;
        }
        Candidate taken = c;
        taken.place = placeOf(*c.map, c.ledge(), at - c.offset, toBody);
        const Ledge l = taken.toLedge();
        if(l.top < lowest)
        {
            st.low++;
            continue; // too low for a hold (Lowest Ledge)
        }
        if(l.top < hand.z - touch().over - radius - 1e-3f)
        {
            st.below++;
            continue; // lower under the hand than a hold may be
        }
        const glm::vec3 d = l.hold - hand;
        const float dist = glm::length(d);
        if(dist > radius + lenientReach)
        {
            st.tooFar++;
            continue; // further than a hold taken at it, just short of it, plus the leniency
        }
        const glm::vec2 fromHead{l.hold.x - headPos.x, l.hold.y - headPos.y};
        if(glm::length(side) >= 4.f && glm::dot(fromHead, side) < 0.f)
        {
            st.behind++;
            continue; // behind the head, from the hand
        }
        const float score = dist * (1.f - reachFavour * (dist > 1e-3f ? glm::dot(d / dist, reach) : 0.f));
        scored.push_back(Scored{score, taken, at});
    }
    std::sort(scored.begin(), scored.end(), [](const Scored& a, const Scored& b) { return a.score < b.score; });
    for(const Scored& sc : scored)
    {
        switch(check(player, sc.c, sc.at, true, hand, headPos, movers))
        {
        case Check::ok: lenient = true; return finish(&sc.c);
        case Check::covered: st.covered++; break;
        case Check::unreached: st.covered++; break;
        case Check::unseen: st.unseen++; break;
        }
    }
    return finish(nullptr);
}

[[nodiscard]] float leniencyUnits()
{
    return std::max(0.f, vr_climb_leniency.value) * 0.01f * units::metresToUnits();
}

// The hold a grip of hand `h` takes (findHold): from the drawn hand, or from the controller's own point where the level
// holds the drawn hand out of it (vr_handpose.cpp: a hand reaching up under a rung stops with its fingers against the
// rung's underside, its point a hand's height below it). A hold either point is at comes first (the drawn hand's
// before the controller's), then the drawn hand's lenient one, then the controller's. `tracked`: found from the
// controller's point.
[[nodiscard]] std::optional<Ledge> gripHold(edict_t* ent, const VrMove& move, int h, bool& lenient, bool& tracked, LenientStats* st)
{
    constexpr float maxHeldOut = 64.f; // units: a controller's point further from the drawn hand is not believed
    const VrHandMove& m = move.hands[h];
    const float radius = leniencyUnits();
    tracked = false;
    std::optional<Ledge> drawn = findHold(ent, m.pos, m.throwVel, move.headPos, radius, lenient, st);
    const float heldOut = glm::distance(m.tracked, m.pos);
    if((drawn && !lenient) || heldOut < 0.25f || heldOut > maxHeldOut)
    {
        return drawn;
    }
    bool trackedLenient = false;
    LenientStats st2;
    std::optional<Ledge> own = findHold(ent, m.tracked, m.throwVel, move.headPos, radius, trackedLenient, &st2);
    if(st)
    {
        st->points += st2.points;
    }
    if(own && (!trackedLenient || !drawn))
    {
        tracked = true;
        lenient = trackedLenient;
        return own;
    }
    return drawn;
}

struct Grip
{
    bool active{false};
    glm::vec3 hold{0.f};       // where the hand holds (world, when it took hold)
    glm::vec3 lastHold{0.f};   // where the hold was last frame (it moves with its brush model)
    glm::vec3 relAtGrab{0.f};  // the hand relative to the body when it took hold
    glm::vec3 lastRel{0.f};    // and last frame
    glm::vec3 grabOffset{0.f}; // the tracked hand from the hold when it took hold (vr_climb_debug 2's "sync")
    Ledge ledge;
    int entNum{0};             // the hold's brush model (0: the world), which may move
    glm::vec3 entOrigin{0.f};  // and where it was
    bool owned{false};         // the grip's press belongs to the hold (hidden from the QC) until let go
    bool ownedLastFrame{false};
    int serial{0};             // counts the holds taken (the client tells a new hold from the last)
    float allowed{0.f};        // how far the hold may be from the shoulder (the reach, or further if taken further)
    int entModel{0};           // its model (a hold lets go if it changes)
    int mapGeneration{-1};     // ledge.map's (ledges::generation: the map is gone if it changed)
    // The mantle's search (findMantle), kept for the hold: the last search that found no room (not searched again, the
    // body hanging still, until `mantleRetry`).
    bool missed{false};
    double missTime{0.0};
    glm::vec3 missOrigin{0.f}, missHold{0.f}, missMins{0.f}, missMaxs{0.f};
    float missTop{0.f};
};

struct Climber
{
    Grip grips[2];
    bool mantling{false};
    double mantleStart{0.0};
    glm::vec3 mantleFrom{0.f}, mantleMid{0.f}, mantleTo{0.f};
    int mantleEnt{0};           // the brush model mantled onto (0: the world): the way follows it,
    glm::vec3 mantleBase{0.f};  // from where it was at the start
    glm::vec3 mantleShift{0.f}; // (how far it has moved since)
    double noGrabUntil{0.0};
    double lastTime{-1.0};
    bool pressedLastFrame[2]{false, false};
    glm::vec3 lastOrigin{0.f}; // where the climb put the body (moved otherwise: let go)
    glm::vec3 owed{0.f};       // the pull the body couldn't make (made up first by a pull the other way)
    bool noRoom{false};        // pulling over the top of a ledge with no room to mantle (felt once)
    // Climbing stamina (see "Climbing stamina" below).
    bool draining{false};      // this frame's hang spent stamina
    bool slipping{false};      // none left, vr_climb_stamina_slip > 0: the hands slip off at slipStart + that
    double slipStart{0.0};
    bool wasLow{false};        // the pool was low (the gadget's blink) at the last hanging frame: the breath once as it gets low
    double tiredAt{-1.0};      // a grip refused for want of stamina: its buzz (at most every 0.5 s)
    std::string handsLine[2];  // vr_debug_hands: the lines last printed
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

// What keeps hand `h` from taking hold (nullptr: nothing, the hand is empty): a weapon, a carried object, a locked force
// grab or one flying to it now, the flashlight.
//
// Round 21 ("Hands: both work"): a force grab's .*hand_fgpulled keeps naming the last thing that hand pulled after it
// arrived (the QC reads it only with .fg_state 1: VR_Forcegrab_IsPulling), so it counts only while it flies to this
// hand, as the stats' (vr_server.cpp). It used to count while the thing existed: a hand that had force grabbed a box, a
// brick or a gib and put it down never took hold again, until the thing was taken or removed.
[[nodiscard]] const char* handBusy(edict_t* ent, const VrMove* move, int h)
{
    if(move && (move->buttons & busyButton[h]))
    {
        return "the flashlight"; // (client-side)
    }
    const FieldOffsets& f = fields();
    if(!bindings().isVrProgs)
    {
        return h == 0 ? nullptr : "its weapon"; // another mod's progs: the main hand always holds its weapon
    }
    const float weapon = h == 1 ? ent->v.weapon : fieldFloatOr(ent, f.weapon2, 0.f);
    if(weapon != 0.f)
    {
        return "a weapon";
    }
    const edict_t* held = entityField(ent, h == 1 ? f.mainhand_held : f.offhand_held);
    if(held && !held->free)
    {
        return "a carried object";
    }
    if(fieldFloatOr(ent, h == 1 ? f.mainhand_fglocked : f.offhand_fglocked, 0.f) != 0.f)
    {
        return "a force grab locked on";
    }
    edict_t* pulled = entityField(ent, h == 1 ? f.mainhand_fgpulled : f.offhand_fgpulled);
    if(pulled && !pulled->free && fieldFloatOr(pulled, f.fg_state, 0.f) == 1.f && entityField(pulled, f.fg_player) == ent &&
        fieldFloatOr(pulled, f.fg_hand, -1.f) == static_cast<float>(h))
    {
        return "a force grab in flight";
    }
    return nullptr;
}

[[nodiscard]] bool handEmpty(edict_t* ent, const VrMove* move, int h)
{
    return handBusy(ent, move, h) == nullptr;
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
    if(hotspot == HS_GRENADE_POUCH)
    {
        return !hanging && ent->v.ammo_rockets >= 1.f; // a grenade to take (hanging: the next hold)
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
// Grip `g`'s brush model while it can be held (there, solid, the same model), else nullptr.
[[nodiscard]] edict_t* holdEntity(const Grip& g)
{
    if(g.entNum <= 0 || g.entNum >= qcvm->num_edicts)
    {
        return nullptr;
    }
    edict_t* e = EDICT_NUM(g.entNum);
    return !e->free && static_cast<int>(e->v.solid) == SOLID_BSP && static_cast<int>(e->v.modelindex) == g.entModel ? e : nullptr;
}

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
    c.owed = glm::vec3{0.f};
    c.noRoom = false;
    c.slipping = false;
}

// The arm's reach (see the top of the file), units: the shoulder joint to the palm's middle.
[[nodiscard]] float armReach()
{
    const float body = units::bodyScale();
    return (bodycal::armLengthMetres() + (palmReach + reachSlack) * body + std::max(0.f, vr_body_shoulder_reach.value) * body) *
           units::metresToUnits();
}

// The shoulder joint of hand `h`'s arm (world), from the head of `move` (see the top of the file).
[[nodiscard]] glm::vec3 shoulderOf(const VrMove& move, int h)
{
    const float m2u = units::metresToUnits() * units::bodyScale();
    glm::vec3 f, r, u;
    hands::angleVectors(glm::vec3{move.headAngles.x, move.headAngles.y, 0.f}, f, r, u);
    const glm::vec3 neck = move.headPos - (f * eyeToNeckFwd + u * eyeToNeckDown) * m2u;
    const float yaw = glm::radians(move.headAngles.y);
    const glm::vec3 fwd{std::cos(yaw), std::sin(yaw), 0.f}, left{-std::sin(yaw), std::cos(yaw), 0.f};
    const bool rightSide = h == 1; // (hand 1: the main hand, the right controller)
    const glm::vec3 shift = bodycal::shoulderShift(); // back, up, out
    return neck + (-fwd * shift.x + glm::vec3{0.f, 0.f, shift.y - neckToShoulderDown} +
                      left * ((rightSide ? -1.f : 1.f) * (neckToShoulderOut + shift.z))) *
                      m2u;
}

// How deep grip `g`'s ledge's top is from its lip in, `side` units along it from the hold (the ledge map's, up to
// ledges::maxDepth); negative: not known there (past the ledge's end).
[[nodiscard]] float depthAt(const Grip& g, float side)
{
    if(!g.ledge.map || g.ledge.edge < 0 || g.mapGeneration != ledges::generation())
    {
        return -1.f;
    }
    const ledges::Edge& e = g.ledge.map->edges[static_cast<size_t>(g.ledge.edge)];
    const glm::vec3 along{-g.ledge.out.y, g.ledge.out.x, 0.f};
    const float t = g.ledge.t + side * glm::dot(along, e.dir);
    if(t < -1e-3f || t > e.len + 1e-3f)
    {
        return -1.f;
    }
    return g.ledge.map->sampleAt(e, std::clamp(t, 0.f, e.len)).depth;
}

// The hands in sync (vr_climb_slide; see the top of the file): grip `g`'s hold slid along its lip towards where the
// tracked hand `tracked` has it (the hand as far from the hold as when it took hold), as far as the lip goes and no
// further from `shoulder` than `reach` (or than it is already).
void slideHold(Grip& g, const glm::vec3& tracked, const glm::vec3& shoulder, float reach)
{
    if(!g.ledge.map || g.ledge.edge < 0 || g.mapGeneration != ledges::generation())
    {
        return;
    }
    const ledges::Edge& e = g.ledge.map->edges[static_cast<size_t>(g.ledge.edge)];
    const glm::vec3 hold = holdNow(g);
    float s = glm::dot(tracked - g.grabOffset - hold, e.dir);
    s = std::clamp(g.ledge.t + s, 0.f, e.len) - g.ledge.t;
    // |hold + e.dir * s - shoulder| <= max(reach, now): s between the roots of s^2 + 2 b s + c = 0 (c <= 0).
    const glm::vec3 v = hold - shoulder;
    const float b = glm::dot(v, e.dir);
    const float c = glm::dot(v, v) - std::max(reach * reach, glm::dot(v, v));
    const float root = std::sqrt(std::max(0.f, b * b - c));
    s = std::clamp(s, -b - root, -b + root);
    if(std::abs(s) < 1e-4f)
    {
        return;
    }
    const glm::vec3 d = e.dir * s;
    g.hold += d;
    g.ledge.hold += d;
    g.ledge.top += d.z;
    g.ledge.t += s;
}

// The mantle: a spot on top of grip `g`'s ledge where the box fits, over floor, reachable straight up (or up and
// along the edge a little, or up from a little further out) and then over: straight in from the hold, else along the edge (near a ledge's end, where
// the box would stick out past it). 22 to 38 units in from the lip; else (a narrow top: a wall's, a beam's) over the
// top's middle, or further out, the box's middle over the top.
//
// The ledge map gives the top's depth from the lip (the narrow top's spots), and whether a spot's middle is over the
// top: then the box stands on it (the floor and footing below need no traces), and one sweep over from the top of the
// way up (shared by the spots on that side) is all it takes. Elsewhere (a spot past the ledge's end, or in further
// than the top goes, where the box may still stand on its back part) the traces as before.
[[nodiscard]] bool findMantle(edict_t* ent, Grip& g, glm::vec3& mid, glm::vec3& to)
{
    const glm::vec3 origin = vec(ent->v.origin);
    const glm::vec3 hold = holdNow(g);
    const float z = topNow(g) - ent->v.mins[2] + 1.f;
    if(z < origin.z)
    {
        return false;
    }
    const glm::vec3 along{-g.ledge.out.y, g.ledge.out.x, 0.f};
    const glm::vec3 lip = hold + g.ledge.out * holdInset;
    constexpr float sides[] = {0.f, 8.f, -8.f, 16.f, -16.f};
    constexpr int sideCount = static_cast<int>(std::size(sides));
    // Straight up to over the top, `side` along the edge; else from a little further out (a body pressed against the
    // face, under a trim on its lip). The same for every spot in from the lip: swept once a side.
    int upState[sideCount] = {}; // 0: not swept yet, 1: clear (to upMid), -1: blocked
    glm::vec3 upMid[sideCount];
    const auto up = [&](int s) {
        if(upState[s] == 0)
        {
            upState[s] = -1;
            for(const float back : {0.f, 2.f, 4.f})
            {
                const glm::vec3 m = glm::vec3{origin.x, origin.y, z} + along * sides[s] + g.ledge.out * back;
                if(tracePlayer(ent, origin, m).fraction >= 1.f)
                {
                    upState[s] = 1;
                    upMid[s] = m;
                    break;
                }
            }
        }
        return upState[s] > 0;
    };
    // The spot `in` units in from the lip, `sides[s]` along it.
    const auto fits = [&](int s, float in) {
        if(!up(s))
        {
            return false;
        }
        mid = upMid[s];
        const float side = sides[s];
        to = glm::vec3{lip.x, lip.y, z} - g.ledge.out * in + along * side;
        // Over the top (the ledge map): it stands there; the sweep over (below) finds anything in the way or in the
        // spot.
        if(const float depth = depthAt(g, side); !(depth >= 0.f && in <= depth - 0.5f))
        {
            if(tracePlayer(ent, to, to).startsolid)
            {
                return false;
            }
            if(traceBox(to, vec(ent->v.mins), vec(ent->v.maxs), to - glm::vec3{0.f, 0.f, 8.f}, MOVE_NOMONSTERS, ent).fraction >= 1.f)
            {
                return false; // nothing to stand on
            }
            // and not just its very edge (a box touching the far side of a narrow wall's top stood on nothing): the
            // top under a point `minFooting` in from the box's sides, at least (the box traces are the hull's whole
            // size).
            bool footing = false;
            for(int i = -1; i <= 1 && !footing; i++)
            {
                for(int j = -1; j <= 1 && !footing; j++)
                {
                    const glm::vec3 p = to + glm::vec3{i * (ent->v.maxs[0] - minFooting), j * (ent->v.maxs[1] - minFooting),
                                                 ent->v.mins[2] + 0.5f};
                    const trace_t down = traceLine(p, p - glm::vec3{0.f, 0.f, 8.5f}, ent);
                    footing = !down.startsolid && down.fraction < 1.f;
                }
            }
            if(!footing)
            {
                return false;
            }
        }
        return tracePlayer(ent, mid, to).fraction >= 1.f;
    };
    for(int s = 0; s < sideCount; s++)
    {
        for(const float k : {20.f, 28.f, 36.f})
        {
            if(fits(s, k + holdInset))
            {
                return true;
            }
        }
    }
    // Every side swept above: none clear, no spot (the narrow top's either).
    bool anyUp = false;
    for(int s = 0; s < sideCount; s++)
    {
        anyUp = anyUp || upState[s] > 0;
    }
    if(!anyUp)
    {
        return false;
    }
    // A narrow top (at least `minFooting` deep): from over its middle outwards, the box's middle still over it (not
    // standing on its edge over the drop: a rung against a wall is no place to stand).
    const float depth = std::max(0.f, depthAt(g, 0.f));
    if(depth < minFooting)
    {
        return false;
    }
    for(int s = 0; s < sideCount; s++)
    {
        if(upState[s] < 0)
        {
            continue;
        }
        for(float in = std::min(0.5f * depth, 22.f); in >= 0.5f; in -= 2.f)
        {
            if(fits(s, in))
            {
                return true;
            }
        }
    }
    return false;
}

// The player's box from `origin` towards `target`, sliding along what it meets (Quake's clip against up to 4 planes,
// along the crease of two): a move partly into the face under a ledge still makes its part along the face.
[[nodiscard]] glm::vec3 slideBody(edict_t* ent, const glm::vec3& origin, const glm::vec3& target)
{
    const glm::vec3 wanted = target - origin;
    glm::vec3 pos = origin, delta = wanted;
    glm::vec3 planes[4];
    int count = 0;
    for(int bump = 0; bump < 4 && glm::dot(delta, delta) > 1e-8f; bump++)
    {
        const trace_t tr = tracePlayer(ent, pos, pos + delta);
        if(tr.allsolid)
        {
            break;
        }
        pos = vec(tr.endpos);
        if(tr.fraction >= 1.f)
        {
            break;
        }
        const glm::vec3 rest = delta * (1.f - tr.fraction);
        planes[count++] = vec(tr.plane.normal);
        // The rest of the move, off every plane met (along their crease for two); none that turns back.
        delta = glm::vec3{0.f};
        for(int i = 0; i < count; i++)
        {
            glm::vec3 d = rest - planes[i] * std::min(0.f, glm::dot(rest, planes[i]));
            bool clear = true;
            for(int j = 0; j < count; j++)
            {
                clear = clear && (j == i || glm::dot(d, planes[j]) >= -1e-4f);
            }
            if(clear)
            {
                delta = d;
                break;
            }
        }
        if(delta == glm::vec3{0.f} && count >= 2)
        {
            const glm::vec3 crease = glm::cross(planes[count - 2], planes[count - 1]);
            if(const float l = glm::length(crease); l > 1e-4f)
            {
                delta = crease / l * glm::dot(rest, crease / l);
            }
        }
        if(glm::dot(delta, wanted) <= 0.f || count == 4)
        {
            break;
        }
    }
    return pos;
}

// Moves the body towards `target` through the world: the whole way, else the nearest to it of: sliding along what
// blocks it (the face under a ledge: a shimmy pressed against it, pulled partly into it, still moves along it), up/down
// then across, across then up/down (round a ledge's lip).
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
        if(const glm::vec3 slid = slideBody(ent, origin, target);
            glm::distance(slid, target) < glm::distance(best, target) - 1e-4f)
        {
            best = slid;
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

// ---------------------------------------------------------------------------------------------------------------------
// Climbing stamina (vr_climb_stamina; ROUND21.md, "Hands: both work; props through teleporters; climbing stamina").
// A hang spends the pool parries, shoves and blows spend (vr_melee.qc: .vr_stamina_used of vr_parry_stamina_max; the
// rest before it comes back counts from .vr_stamina_time, the last spend): vr_climb_stamina_rate a second from one hand,
// vr_climb_stamina_rate_2h from both (the two together, less: the arms share the weight). Only while the body hangs: a
// hand holding with the feet on something (a low ledge gripped standing) spends nothing, nor do the mantle and standing
// on a ledge. Spending every hanging frame keeps it from coming back while you hang. As it gets low (the gadget's blink:
// the QC's VR_Stamina_LowAt, three seconds of a one-handed hang among the others) the tiring breath plays once; at none
// the hands let go with a gasp and a long buzz in both, or, with vr_climb_stamina_slip, they can't pull you up any more,
// you sink to straight arms, and they slip off that many seconds later. No hang starts with less left than a second's
// hang from one hand.

constexpr float slipSink = 8.f; // units a second an exhausted body sinks, slipping (to straight arms: the reach stops it)

[[nodiscard]] bool staminaOn(edict_t* ent)
{
    const FieldOffsets& f = fields();
    return ent && vr_climb_stamina.value != 0.f && bindings().isVrProgs && f.vr_stamina_used >= 0 && f.vr_stamina_time >= 0;
}

[[nodiscard]] float staminaMax()
{
    return std::max(1.f, vr_parry_stamina_max.value);
}

// 0 .. staminaMax (staminaOn only).
[[nodiscard]] float staminaLeft(edict_t* ent)
{
    return std::clamp(staminaMax() - fieldFloat(ent, fields().vr_stamina_used), 0.f, staminaMax());
}

// Too tired to start a hang: less left than a second's hang from one hand (it would let go at once).
[[nodiscard]] bool tooTired(edict_t* ent)
{
    return staminaOn(ent) && staminaLeft(ent) < std::max(0.001f, vr_climb_stamina_rate.value);
}

// Tiring Warning (vr_parry_stamina_warn): the breath's volume and the buzz's strength, as the parry's.
[[nodiscard]] float staminaWarn()
{
    return std::clamp(vr_parry_stamina_warn.value, 0.f, 1.f);
}

// The pool low, as the gadget last showed it (VR_Melee_Hud's 256: one more of the dearest effort would empty it).
[[nodiscard]] bool staminaLow(edict_t* ent)
{
    return (static_cast<int>(fieldFloatOr(ent, fields().vr_melee_hud, 0.f)) & 256) != 0;
}

void spendStamina(edict_t* ent, float amount)
{
    const FieldOffsets& f = fields();
    float& used = fieldFloat(ent, f.vr_stamina_used);
    used = std::min(staminaMax(), std::max(0.f, used) + std::max(0.f, amount));
    fieldFloat(ent, f.vr_stamina_time) = static_cast<float>(qcvm->time); // (the rest counts from now: none comes back)
    if(f.vr_stamina_said >= 0)
    {
        fieldFloat(ent, f.vr_stamina_said) = 0.f;
    }
}

void staminaSound(edict_t* ent, const char* sample, float volume)
{
    if(volume > 0.f)
    {
        SV_StartSound(ent, 0, sample, static_cast<int>(std::lround(255.f * std::min(volume, 1.f))), 1.f); // (ATTN_NORM)
    }
}

// The body stands on something (the box 2 units down meets a floor, a step, a brush model's top).
[[nodiscard]] bool feetSupported(edict_t* ent)
{
    vec3_t end{ent->v.origin[0], ent->v.origin[1], ent->v.origin[2] - 2.f};
    const trace_t tr = SV_Move(ent->v.origin, ent->v.mins, ent->v.maxs, end, MOVE_NOMONSTERS, ent);
    return !tr.startsolid && tr.fraction < 1.f && tr.plane.normal[2] >= 0.7f;
}

// Out of stamina: every hand lets go (no fling: they give way), a gasp and a long buzz.
void exhaustedLetGo(edict_t* ent, Climber& c, double time)
{
    for(int h = 0; h < 2; h++)
    {
        if(c.grips[h].active)
        {
            server::sendHaptic(ent, h, 0.f, 0.5f, 30.f, staminaWarn());
        }
    }
    staminaSound(ent, "player/gasp1.wav", staminaWarn());
    letGoAll(c);
    c.noGrabUntil = time + regrabDelay;
    setVec(ent->v.velocity, glm::vec3{0.f});
    if(debug())
    {
        Con_Printf("climb: no stamina left, lets go at (%.1f %.1f %.1f)\n", ent->v.origin[0], ent->v.origin[1],
            ent->v.origin[2]);
    }
}

// Each hanging frame, after the pull and the mantle's check: the hang's stamina (see above).
void hangStamina(edict_t* ent, Climber& c, double time)
{
    c.draining = false;
    if(!staminaOn(ent) || !c.hanging() || c.mantling)
    {
        c.slipping = false;
        return;
    }
    const int hands = (c.grips[0].active ? 1 : 0) + (c.grips[1].active ? 1 : 0);
    const bool standing = feetSupported(ent);
    const float before = staminaLeft(ent);
    if(!standing)
    {
        const float rate = std::max(0.f, hands > 1 ? vr_climb_stamina_rate_2h.value : vr_climb_stamina_rate.value);
        spendStamina(ent, rate * static_cast<float>(host_frametime));
        c.draining = rate > 0.f;
        const bool low = staminaLow(ent);
        if(low && !c.wasLow)
        {
            staminaSound(ent, "player/gasp2.wav", 0.8f * staminaWarn()); // tiring (the QC's breath: VR_Stamina_Effort)
            if(debug())
            {
                Con_Printf("climb: stamina low (%.1f left): the tiring breath\n", staminaLeft(ent));
            }
        }
        c.wasLow = low;
        if(vr_climb_debug.value >= 2.f || (debug() && std::floor(before / 10.f) != std::floor(staminaLeft(ent) / 10.f)))
        {
            Con_Printf("climbstamina %.4f %d hand%s: %.2f of %.0f left (-%.3f)%s\n", time, hands, hands > 1 ? "s" : "",
                staminaLeft(ent), staminaMax(), before - staminaLeft(ent), low ? " low" : "");
        }
    }
    if(standing || staminaLeft(ent) > 0.f)
    {
        c.slipping = false;
        return;
    }
    const float slip = std::max(0.f, vr_climb_stamina_slip.value);
    if(slip <= 0.f)
    {
        exhaustedLetGo(ent, c, time);
        return;
    }
    if(!c.slipping)
    {
        c.slipping = true;
        c.slipStart = time;
        for(int h = 0; h < 2; h++)
        {
            if(c.grips[h].active)
            {
                server::sendHaptic(ent, h, 0.f, 0.3f, 40.f, 0.6f * staminaWarn());
            }
        }
        staminaSound(ent, "player/gasp2.wav", 0.8f * staminaWarn());
        if(debug())
        {
            Con_Printf("climb: no stamina left, slipping (%.2f s)\n", slip);
        }
    }
    else if(time - c.slipStart >= slip)
    {
        exhaustedLetGo(ent, c, time);
    }
}

// vr_debug_hands: each hand's state, a line a hand when it changes (2: every frame).
void debugHands(edict_t* ent, Climber& c, const VrMove* move, double time)
{
    if(vr_debug_hands.value <= 0.f)
    {
        return;
    }
    const FieldOffsets& f = fields();
    const auto entName = [](edict_t* e) -> std::string {
        if(!e)
        {
            return "none";
        }
        char buf[96];
        q_snprintf(buf, sizeof(buf), "%d %s%s", NUM_FOR_EDICT(e), PR_GetString(e->v.classname), e->free ? " (freed)" : "");
        return buf;
    };
    for(int h = 0; h < 2; h++)
    {
        edict_t* held = entityField(ent, h == 1 ? f.mainhand_held : f.offhand_held);
        edict_t* otherHeld = entityField(ent, h == 1 ? f.offhand_held : f.mainhand_held);
        edict_t* target = entityField(ent, h == 1 ? f.mainhand_fgtarget : f.offhand_fgtarget);
        edict_t* pulled = entityField(ent, h == 1 ? f.mainhand_fgpulled : f.offhand_fgpulled);
        const int hotspot = move ? move->hotspots[h] : 0;
        const bool atHolster = move && holsterWins(ent, hotspot, c.grips[1 - h].active);
        const char* busy = handBusy(ent, move, h);
        char pulledState[48] = "";
        if(pulled)
        {
            q_snprintf(pulledState, sizeof(pulledState), " (state %g, hand %g)", fieldFloatOr(pulled, f.fg_state, 0.f),
                fieldFloatOr(pulled, f.fg_hand, -1.f));
        }
        const char* climb = c.grips[h].active                      ? "holding"
                            : c.mantling                           ? "mantling"
                            : busy                                 ? busy
                            : atHolster                            ? "a holster wins"
                            : time < c.noGrabUntil                 ? "too soon"
                            : !c.hanging() && tooTired(ent) ? "too tired"
                            : vr_climb.value == 0.f                ? "climbing off"
                                                                   : "free";
        char line[512];
        q_snprintf(line, sizeof(line),
            "grip %d | weapon %g | carry %s%s | force grab: target %s%s, pulled %s%s | flashlight %d | hotspot %d | "
            "climb: %s",
            move && (move->vrBits0 & grabBit[h]) ? 1 : 0,
            h == 1 ? ent->v.weapon : fieldFloatOr(ent, f.weapon2, 0.f), entName(held).c_str(),
            held && held == otherHeld ? " (both hands)" : "", entName(target).c_str(),
            fieldFloatOr(ent, h == 1 ? f.mainhand_fglocked : f.offhand_fglocked, 0.f) != 0.f ? " locked" : "",
            entName(pulled).c_str(), pulledState, move && (move->buttons & busyButton[h]) ? 1 : 0, hotspot, climb);
        if(vr_debug_hands.value >= 2.f || c.handsLine[h] != line)
        {
            c.handsLine[h] = line;
            Con_Printf("hands %.3f %s: %s\n", time, handName(h), line);
        }
    }
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
    c.draining = false; // (VR_ClientClimb's hang says otherwise)
    if(fields().vr_climb_drain >= 0)
    {
        fieldFloat(ent, fields().vr_climb_drain) = 0.f;
    }

    server::rebaseHands(ent);
    const VrMove* move = server::clientMove(ent);
    const bool tracked = move && (move->buttons & protocol::QVR_BUTTON_HANDSTRACKED);
    const bool enabled = vr_climb.value != 0.f && tracked && static_cast<int>(ent->v.movetype) == MOVETYPE_WALK &&
                         ent->v.health > 0.f;

    // Moved by something else (a teleporter, a respawn, setorigin), disabled, dead, noclipping, the
    // hold's brush model gone (freed, not solid, another model): let go, and fall as whatever moved the body has it.
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
        if(g.active && g.entNum > 0 && !holdEntity(g))
        {
            if(debug())
            {
                Con_Printf("climb: the hold's brush model is gone, letting go\n");
            }
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
        const char* busy = handBusy(ent, move, h);
        if(time < c.noGrabUntil || atHolster || busy)
        {
            if(debug())
            {
                Con_Printf("climb: %s hand grips: %s%s\n", handName(h),
                    time < c.noGrabUntil ? "too soon" : atHolster ? "at a holster" : "not empty: ",
                    time < c.noGrabUntil || atHolster ? "" : busy);
            }
            continue;
        }
        if(!hanging && tooTired(ent))
        {
            // Too little stamina left to hang (parries, shoves, blows, the last hang): no new hang until more comes back
            // (a hand joining the other on a hold is let: two hands tire less). A soft buzz says why; the gadget shows it.
            if(!(time - c.tiredAt < 0.5))
            {
                c.tiredAt = time;
                server::sendHaptic(ent, h, 0.f, 0.15f, 30.f, 0.4f * staminaWarn());
            }
            if(debug())
            {
                Con_Printf("climb: %s hand grips: too tired (%.1f stamina left)\n", handName(h), staminaLeft(ent));
            }
            continue;
        }

        const glm::vec3 hand = move->hands[h].pos;
        const double t0 = Sys_DoubleTime();
        LenientStats st;
        const int traces0 = traceCount;
        bool lenient = false, fromTracked = false;
        const std::optional<Ledge> ledge = gripHold(ent, *move, h, lenient, fromTracked, &st);
        if(debug() && vr_climb_debug.value >= 3.f)
        {
            Con_Printf("climb: hold search: %d ledges looked at, %d traces in %.4f ms\n", st.points, traceCount - traces0,
                (Sys_DoubleTime() - t0) * 1000.0);
        }
        const char* how = lenient ? (fromTracked ? ", lenient, from the controller" : ", lenient")
                          : fromTracked ? ", from the controller"
                                        : "";
        if(!ledge)
        {
            if(debug())
            {
                const glm::vec3 own = move->hands[h].tracked;
                Con_Printf("climb: %s hand at (%.1f %.1f %.1f): no hold (the controller at %.1f %.1f %.1f)\n", handName(h), hand.x,
                    hand.y, hand.z, own.x, own.y, own.z);
            }
            continue;
        }

        g.active = true;
        g.owned = true;
        g.serial++;
        g.missed = false;
        g.hold = g.lastHold = ledge->hold;
        g.relAtGrab = g.lastRel = handRel(ent, *move, h);
        g.grabOffset = hand - ledge->hold;
        g.ledge = *ledge;
        g.entNum = ledge->ent ? NUM_FOR_EDICT(ledge->ent) : 0;
        g.entOrigin = ledge->ent ? vec(ledge->ent->v.origin) : glm::vec3{0.f};
        g.entModel = ledge->ent ? static_cast<int>(ledge->ent->v.modelindex) : 0;
        g.ledge.ent = nullptr; // (kept as its number: entNum)
        g.mapGeneration = ledges::generation();
        g.allowed = std::max(armReach(), glm::distance(ledge->hold, shoulderOf(*move, h)));
        if(!hanging)
        {
            c.owed = glm::vec3{0.f};
            c.noRoom = false;
        }
        c.lastOrigin = vec(ent->v.origin);
        server::sendHaptic(ent, h, 0.f, 0.06f, 80.f, 0.6f);
        physsound::grab(ent, ledge->hold, ledge->ent); // a slap and a tap of the hold's material
        if(debug())
        {
            Con_Printf("climb: %s hand holds at %.1f (out %.2f %.2f; hand %.1f %.1f %.1f, hold %.1f %.1f %.1f) from (%.1f %.1f %.1f)%s%s\n",
                handName(h), ledge->top, ledge->out.x, ledge->out.y, hand.x, hand.y, hand.z, ledge->hold.x, ledge->hold.y,
                ledge->hold.z, ent->v.origin[0], ent->v.origin[1], ent->v.origin[2], hanging ? ", both hands" : "", how);
        }
        if(vr_climb_debug.value >= 2.f)
        {
            Con_Printf("climbreach: %s hand's hold %.1f units from its shoulder (reach %.1f: the arm %.3f m)\n", handName(h),
                glm::distance(ledge->hold, shoulderOf(*move, h)), armReach(), bodycal::armLengthMetres());
        }
    }

    if(!wasHanging && c.hanging() && staminaOn(ent))
    {
        c.wasLow = staminaLow(ent); // (the breath as the hang makes it low, not for a pool low already)
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
    debugHands(ent, c, move, time);
}

// SV_Physics_Client, in place of the move: the body hangs from the hands, or mantles.
// 0: not climbing (move as usual); 1: moved; -1: the entity was freed by its think.
// Whether the hanging body is under an overhang: a hold of a hand that pulls still further from its shoulder than it
// may be (the body couldn't be kept within reach), above the box's top, and the box blocked straight up.
[[nodiscard]] bool underOverhang(edict_t* ent, const Climber& c, const glm::vec3 (&shoulder)[2], const bool (&driving)[2])
{
    const glm::vec3 at = vec(ent->v.origin);
    bool beyond = false;
    for(int h = 0; h < 2; h++)
    {
        const Grip& g = c.grips[h];
        const glm::vec3 hold = holdNow(g);
        beyond = beyond || (driving[h] && hold.z > at.z + ent->v.maxs[2] &&
                               glm::distance(hold, at + shoulder[h]) > g.allowed + 0.05f);
    }
    if(!beyond)
    {
        return false;
    }
    const trace_t up = tracePlayer(ent, at, at + glm::vec3{0.f, 0.f, 1.f});
    return up.startsolid || up.fraction < 1.f;
}

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
        // Onto a brush model: the way moves with it (the push carries the body too: VR_ClimbCarried).
        edict_t* onto = c.mantleEnt > 0 && c.mantleEnt < qcvm->num_edicts ? EDICT_NUM(c.mantleEnt) : nullptr;
        if(onto && !onto->free && static_cast<int>(onto->v.solid) == SOLID_BSP)
        {
            c.mantleShift = vec(onto->v.origin) - c.mantleBase;
        }
        else
        {
            onto = nullptr;
        }
        const float up = glm::distance(c.mantleFrom, c.mantleMid);
        const float over = glm::distance(c.mantleMid, c.mantleTo);
        const float t = CLAMP(0.f, static_cast<float>((time - c.mantleStart) / mantleTime), 1.f);
        const float along = t * (up + over);
        const glm::vec3 pos = (along <= up && up > 0.f
                                  ? glm::mix(c.mantleFrom, c.mantleMid, along / up)
                                  : glm::mix(c.mantleMid, c.mantleTo, over > 0.f ? (along - up) / over : 1.f)) +
                              c.mantleShift;
        setVec(ent->v.origin, pos);
        setVec(ent->v.velocity, glm::vec3{0.f});
        ent->v.flags = static_cast<float>(static_cast<int>(ent->v.flags) & ~FL_ONGROUND);
        if(t >= 1.f)
        {
            c.mantling = false;
            c.noGrabUntil = time + regrabDelay;
            ent->v.flags = static_cast<float>(static_cast<int>(ent->v.flags) | FL_ONGROUND);
            if(onto)
            {
                ent->v.groundentity = EDICT_TO_PROG(onto); // standing on it: it carries the body from now on
            }
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
    float weight[2]{0.f, 0.f};
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
            weight[h] = w;
        }
        carry += holdNow(g) - g.lastHold;
        top = std::max(top, topNow(g));
        count++;
    }
    carry /= static_cast<float>(count);
    const glm::vec3 pulled = weights > 1e-6f ? -pull / weights : glm::vec3{0.f};
    const glm::vec3 wanted = carry + pulled;

    // What the body owes (earlier pulls it couldn't make) is made up first by a pull the other way; it never moves the
    // body by itself.
    glm::vec3 move3 = pulled;
    if(const float owed = glm::length(c.owed); owed > 1e-6f)
    {
        const glm::vec3 dir = c.owed / owed;
        if(const float s = glm::dot(move3, dir); s < 0.f)
        {
            const float used = std::min(-s, owed);
            move3 += dir * used;
            c.owed -= dir * used;
        }
    }
    if(c.slipping)
    {
        // No stamina left, slipping: the arms can't pull the body up; it sinks (to straight arms: the reach below).
        move3.z = std::min(move3.z, 0.f) - slipSink * static_cast<float>(host_frametime);
    }
    const glm::vec3 meant = carry + move3; // what the body would do unhindered

    // The limits, each stopping the body only from going further past it: the feet no higher than the highest hold
    // (the mantle does that), the body within 48 units (horizontally) of each hold.
    const glm::vec3 origin = vec(ent->v.origin);
    glm::vec3 target = origin + carry + move3;
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

    // And within an arm's reach of each hold whose hand pulls (see the top of the file): where the move took the body
    // (a move into the floor or the face, blocked, moves it nowhere), moved back towards the holds, the least that
    // does (a few passes for two holds), from where it was.
    glm::vec3 shoulder[2];
    bool driving[2]{false, false};
    for(int h = 0; h < 2; h++)
    {
        shoulder[h] = shoulderOf(*move, h) - origin;
        driving[h] = c.grips[h].active && (weights <= 1e-6f || weight[h] >= passiveShare * weights);
    }
    const glm::vec3 pushed = vec(ent->v.origin);
    const auto within = [&]
    {
        glm::vec3 kept = pushed;
        for(int pass = 0; pass < 4; pass++)
        {
            for(int h = 0; h < 2; h++)
            {
                if(!driving[h])
                {
                    continue;
                }
                const glm::vec3 toHold = holdNow(c.grips[h]) - (kept + shoulder[h]);
                if(const float d = glm::length(toHold); d > c.grips[h].allowed + 0.01f)
                {
                    kept += toHold / d * (d - c.grips[h].allowed);
                }
            }
        }
        return kept;
    };
    if(const glm::vec3 kept = within(); kept != pushed)
    {
        setVec(ent->v.origin, origin);
        moveBody(ent, kept);
    }
    // Under an overhang (see the top of the file): the way back within reach is up into what the box is under, below
    // a hold over it. The arms stretch instead, up to vr_climb_overhang_stretch units past the reach (the box stands 0.6 m
    // over the shoulders and in front of the eyes, a head and a chest about 0.25 and 0.15), so that pushing away moves
    // the body out from under it.
    if(vr_climb_overhang_stretch.value > 0.f && underOverhang(ent, c, shoulder, driving))
    {
        for(int h = 0; h < 2; h++)
        {
            Grip& g = c.grips[h];
            if(driving[h])
            {
                const float d = glm::distance(holdNow(g), pushed + shoulder[h]);
                g.allowed = std::max(g.allowed, std::min(d, armReach() + std::max(0.f, vr_climb_overhang_stretch.value)));
            }
        }
        setVec(ent->v.origin, origin);
        moveBody(ent, within());
        if(vr_climb_debug.value >= 2.f)
        {
            Con_Printf("climbreach: under an overhang, the arms stretch to %.2f / %.2f\n", c.grips[0].allowed,
                c.grips[1].allowed);
        }
    }
    setVec(ent->v.velocity, glm::vec3{0.f});
    ent->v.flags = static_cast<float>(static_cast<int>(ent->v.flags) & ~FL_ONGROUND);
    c.lastOrigin = vec(ent->v.origin);
    const glm::vec3 done = vec(ent->v.origin) - origin;
    // Owed: what of the pull into the face under the holds (towards the ledge) the body couldn't make.
    glm::vec3 into{0.f};
    for(const Grip& g : c.grips)
    {
        into -= g.active ? g.ledge.out : glm::vec3{0.f};
    }
    if(glm::length(into) > 1e-3f)
    {
        into = glm::normalize(into);
        if(const float blocked = glm::dot(meant - done, into); blocked > 0.f)
        {
            c.owed += into * blocked;
        }
    }
    if(const float reach = armReach(), owed = glm::length(c.owed); owed > reach)
    {
        c.owed *= reach / owed;
    }
    float dist[2]{0.f, 0.f};
    for(int h = 0; h < 2; h++)
    {
        Grip& g = c.grips[h];
        if(g.active && vr_climb_slide.value != 0.f)
        {
            slideHold(g, vec(ent->v.origin) + rel[h], vec(ent->v.origin) + shoulder[h], g.allowed);
        }
        g.lastRel = rel[h];
        g.lastHold = holdNow(g);
        if(!g.active)
        {
            continue;
        }
        // Out of reach: a hand that hardly pulled is torn off its hold, any hand the body couldn't be kept near lets go.
        dist[h] = glm::distance(g.lastHold, vec(ent->v.origin) + shoulder[h]);
        if(dist[h] > g.allowed + (driving[h] ? brokenSlack : passiveSlack))
        {
            g.active = false;
            server::sendHaptic(ent, h, 0.f, 0.08f, 60.f, 0.5f);
            if(debug())
            {
                Con_Printf("climb: %s hand out of reach (%.1f, reach %.1f%s), lets go at (%.1f %.1f %.1f)\n", handName(h),
                    dist[h], g.allowed, driving[h] ? "" : ", not pulling", ent->v.origin[0], ent->v.origin[1],
                    ent->v.origin[2]);
            }
            continue;
        }
        g.allowed = std::max(armReach(), std::min(g.allowed, dist[h]));
    }
    SV_CheckWater(ent);
    traceFrame(ent, c, move, count > 1 ? "hang2" : "hang1", wanted, done);
    if(vr_climb_debug.value >= 2.f)
    {
        // "sync": how far each tracked hand has drifted from its drawn hand (on the hold) since it took hold.
        glm::vec3 sync[2]{glm::vec3{0.f}, glm::vec3{0.f}};
        for(int h = 0; h < 2; h++)
        {
            const Grip& g = c.grips[h];
            sync[h] = g.active ? vec(ent->v.origin) + rel[h] - holdNow(g) - g.grabOffset : glm::vec3{0.f};
        }
        Con_Printf("climbreach %.4f off %.2f/%.2f%s main %.2f/%.2f%s owed %.3f %.3f %.3f%s sync %.2f %.2f (%.2f %.2f %.2f / "
                   "%.2f %.2f %.2f)\n",
            qcvm->time, dist[0], c.grips[0].allowed, driving[0] ? "" : " passive", dist[1], c.grips[1].allowed,
            driving[1] ? "" : " passive", c.owed.x, c.owed.y, c.owed.z, c.noRoom ? " noroom" : "", glm::length(sync[0]),
            glm::length(sync[1]), sync[0].x, sync[0].y, sync[0].z, sync[1].x, sync[1].y, sync[1].z);
    }
    if(vr_climb_debug.value >= 3.f)
    {
        const glm::vec3 so = vec(ent->v.origin) + shoulder[0], sm = vec(ent->v.origin) + shoulder[1];
        Con_Printf("climbshoulder off %.1f %.1f %.1f main %.1f %.1f %.1f\n", so.x, so.y, so.z, sm.x, sm.y, sm.z);
    }
    if(!c.hanging())
    {
        // Let go of everything (out of reach): fall.
        letGoAll(c);
        c.noGrabUntil = time + regrabDelay;
        if(debug())
        {
            Con_Printf("climb: falls, flung at (0 0 0)\n");
        }
        return 1;
    }

    // The mantle: pulled down, the head over the ledge, and room on top.
    const float headZ = move->headPos.z;
    bool noRoom = false;
    for(int h = 0; h < 2; h++)
    {
        Grip& g = c.grips[h];
        if(!g.active)
        {
            continue;
        }
        const float pulledDown = g.relAtGrab.z - rel[h].z;
        const float headAbove = headZ + (ent->v.origin[2] - origin.z) - topNow(g);
        if(pulledDown < mantlePull || headAbove < mantleHead || c.slipping)
        {
            continue;
        }
        // No room found here last time, the body and the hold where they were: none yet (searched again after
        // `mantleRetry`, or once either moves; see findMantle).
        const glm::vec3 now = vec(ent->v.origin), holdAt = holdNow(g);
        if(g.missed && time - g.missTime < mantleRetry && glm::distance(now, g.missOrigin) <= mantleRetryMove &&
            holdAt == g.missHold && topNow(g) == g.missTop && vec(ent->v.mins) == g.missMins && vec(ent->v.maxs) == g.missMaxs)
        {
            if(vr_climb_debug.value >= 4.f)
            {
                Con_Printf("climbcost mantle %s: no room (searched %.3f s ago)\n", handName(h), time - g.missTime);
            }
            noRoom = true;
            continue;
        }
        glm::vec3 mid, to;
        const double t0 = Sys_DoubleTime();
        const int traces0 = traceCount;
        const bool found = findMantle(ent, g, mid, to);
        if(vr_climb_debug.value >= 4.f)
        {
            Con_Printf("climbcost mantle %s: %d traces, %.4f ms\n", handName(h), traceCount - traces0,
                (Sys_DoubleTime() - t0) * 1000.0);
        }
        g.missed = !found;
        if(!found)
        {
            g.missTime = time;
            g.missOrigin = now;
            g.missHold = holdAt;
            g.missTop = topNow(g);
            g.missMins = vec(ent->v.mins);
            g.missMaxs = vec(ent->v.maxs);
            noRoom = true;
            continue;
        }
        c.mantling = true;
        c.mantleStart = time;
        c.mantleFrom = vec(ent->v.origin);
        c.mantleMid = mid;
        c.mantleTo = to;
        c.mantleEnt = g.entNum;
        c.mantleBase = c.mantleEnt > 0 ? vec(EDICT_NUM(c.mantleEnt)->v.origin) : glm::vec3{0.f};
        c.mantleShift = glm::vec3{0.f};
        c.grips[0].active = c.grips[1].active = false;
        c.owed = glm::vec3{0.f};
        if(debug())
        {
            Con_Printf("climb: mantle from (%.1f %.1f %.1f) up to %.1f, onto (%.1f %.1f %.1f)\n", c.mantleFrom.x,
                c.mantleFrom.y, c.mantleFrom.z, mid.z, to.x, to.y, to.z);
        }
        break;
    }
    if(!c.mantling)
    {
        // No room on top: a soft buzz as the pull over the top starts (the body stays on the face: the pull into it
        // is owed, so the push over the top doesn't push it back off).
        if(noRoom && !c.noRoom)
        {
            for(int h = 0; h < 2; h++)
            {
                if(c.grips[h].active)
                {
                    server::sendHaptic(ent, h, 0.f, 0.12f, 40.f, 0.25f);
                }
            }
            if(debug())
            {
                Con_Printf("climb: no room to mantle at (%.1f %.1f %.1f)\n", ent->v.origin[0], ent->v.origin[1],
                    ent->v.origin[2]);
            }
        }
        c.noRoom = noRoom;
    }

    hangStamina(ent, c, time);
    if(c.draining && fields().vr_climb_drain >= 0)
    {
        fieldFloat(ent, fields().vr_climb_drain) = 1.f;
    }
    return 1;
}

// SV_PushMove: whether `check` (a player) hangs from the brush model `pusher` or mantles onto it: it rides it then, as
// one standing on it does (carried in the same push, exactly: the holds keep their place on it).
extern "C" int VR_ClimbHangsFrom(edict_t* check, edict_t* pusher)
{
    const Climber* c = climberOf(check);
    if(!c)
    {
        return 0;
    }
    const int num = NUM_FOR_EDICT(pusher);
    return (c->grips[0].active && c->grips[0].entNum == num) || (c->grips[1].active && c->grips[1].entNum == num) ||
           (c->mantling && c->mantleEnt == num);
}

// SV_PushMove, after moving `check` (hanging from `pusher`, VR_ClimbHangsFrom) from `from` by `move`: whether the ride
// stopped short (the body carried into something) blocks the pusher (vr_climb_mover_crush 1: Quake's rules, as anything
// in its way: its blocked function, a door or plat going back, a train's damage). Else (0) the hands on it let go (the
// body stays where the push got it, and falls), or the mantle onto it stops.
extern "C" int VR_ClimbCarryBlocked(edict_t* check, edict_t* pusher, const float* from, const float* move)
{
    Climber* cp = climberOf(check);
    if(!cp)
    {
        return 0;
    }
    Climber& c = *cp;
    const glm::vec3 wanted = vec(from) + vec(move);
    if(glm::distance(vec(check->v.origin), wanted) <= 0.05f)
    {
        return 0;
    }
    if(vr_climb_mover_crush.value != 0.f)
    {
        if(debug())
        {
            Con_Printf("climb: carried into something at (%.1f %.1f %.1f): the mover is blocked (health %.0f)\n", check->v.origin[0],
                check->v.origin[1], check->v.origin[2], check->v.health);
        }
        return 1;
    }
    const int num = NUM_FOR_EDICT(pusher);
    for(int h = 0; h < 2; h++)
    {
        Grip& g = c.grips[h];
        if(g.active && g.entNum == num)
        {
            g.active = false;
            server::sendHaptic(check, h, 0.f, 0.08f, 60.f, 0.5f);
        }
    }
    if(c.mantling && c.mantleEnt == num)
    {
        c.mantling = false;
    }
    if(debug())
    {
        Con_Printf("climb: carried into something at (%.1f %.1f %.1f): lets go\n", check->v.origin[0], check->v.origin[1],
            check->v.origin[2]);
    }
    c.lastOrigin = vec(check->v.origin);
    if(!c.hanging() && !c.mantling)
    {
        letGoAll(c);
        c.noGrabUntil = qcvm->time + regrabDelay;
    }
    return 0;
}

// SV_PushMove, when `pusher` has moved by `move` with what rides it: the holds on it moved with it, and the bodies
// hanging from it with them (VR_ClientClimb's carry is only what the holds moved otherwise: a model moved by the QC).
extern "C" void VR_ClimbCarried(edict_t* pusher, const float* move)
{
    const int num = NUM_FOR_EDICT(pusher);
    const int clients = std::min(svs.maxclients, static_cast<int>(MAX_SCOREBOARD));
    for(int i = 0; i < clients; i++)
    {
        Climber& c = climbers[i];
        bool rides = c.mantling && c.mantleEnt == num;
        for(Grip& g : c.grips)
        {
            if(g.active && g.entNum == num)
            {
                g.lastHold += vec(move);
                rides = true;
            }
        }
        if(rides)
        {
            c.lastOrigin = vec(EDICT_NUM(i + 1)->v.origin);
        }
    }
}

namespace
{

// vr_climb_probe [yaw]: the ledges in front of the player, or towards `yaw` (debugging: where a hand could take hold),
// from 16 to 64 units ahead, from 16 to 128 over the feet.
void probe(edict_t* ent, float yaw)
{
    const glm::vec3 origin = vec(ent->v.origin);
    const float feet = origin.z + ent->v.mins[2];
    const glm::vec3 fwd{std::cos(glm::radians(yaw)), std::sin(glm::radians(yaw)), 0.f};
    const glm::vec3 a = origin + fwd * 16.f, b = origin + fwd * 64.f;
    const glm::vec3 mins{std::min(a.x, b.x) - 24.f, std::min(a.y, b.y) - 24.f, feet + 16.f};
    const glm::vec3 maxs{std::max(a.x, b.x) + 24.f, std::max(a.y, b.y) + 24.f, feet + 128.f};
    int found = 0;
    const auto list = [&](const ledges::Map& m, const glm::vec3& offset, int entNum) {
        std::vector<int> nearby;
        m.nearby(mins - offset, maxs - offset, nearby);
        for(const int i : nearby)
        {
            const ledges::Edge& e = m.edges[static_cast<size_t>(i)];
            const glm::vec3 p = e.a + offset, q = e.point(e.len) + offset;
            const ledges::Sample& s = m.sampleAt(e, 0.5f * e.len);
            Con_Printf("ledge%s: (%.1f %.1f %.1f) to (%.1f %.1f %.1f), %.0f above the feet, out (%.2f %.2f), the drop %.1f out, "
                       "the top %.1f deep (its middle)\n",
                entNum ? va(" on entity %d", entNum) : "", p.x, p.y, p.z, q.x, q.y, q.z, p.z - feet, e.out.x, e.out.y,
                s.dropOut, s.depth);
            found++;
        }
    };
    if(const ledges::Map* w = ledges::of(sv.worldmodel))
    {
        list(*w, glm::vec3{0.f}, 0);
    }
    for(int i = 1; i < qcvm->num_edicts; i++)
    {
        edict_t* e = EDICT_NUM(i);
        const int index = static_cast<int>(e->v.modelindex);
        if(!e->free && static_cast<int>(e->v.solid) == SOLID_BSP && index > 0 && index < MAX_MODELS)
        {
            if(const ledges::Map* m = ledges::of(sv.models[index]))
            {
                list(*m, vec(e->v.origin), i);
            }
        }
    }
    if(!found)
    {
        Con_Printf("no ledge within 64 units ahead\n");
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
    LenientStats st;
    bool lenient = false;
    const int traces0 = traceCount;
    const double t0 = Sys_DoubleTime();
    const std::optional<Ledge> l = findHold(ent, hand, vel, head, leniencyUnits(), lenient, &st);
    const double ms = (Sys_DoubleTime() - t0) * 1000.0;
    if(l)
    {
        Con_Printf("climbtry %.2f %.2f %.2f %s: %s hold %.2f %.2f %.2f top %.1f out %.2f %.2f, %.2f units from the hand (%.1f cm)",
            hand.x, hand.y, hand.z, handName(h), lenient ? "lenient" : "exact", l->hold.x, l->hold.y, l->hold.z, l->top,
            l->out.x, l->out.y, glm::distance(l->hold, hand), glm::distance(l->hold, hand) / units::metresToUnits() * 100.f);
    }
    else
    {
        Con_Printf("climbtry %.2f %.2f %.2f %s: none (leniency %.1f cm)", hand.x, hand.y, hand.z, handName(h),
            vr_climb_leniency.value);
    }
    Con_Printf("; %d points, %d holds seen, turned down: %d low, %d below, %d far, %d behind, %d through a wall, %d covered, "
               "%d hidden; %.4f ms, %d traces\n",
        st.points, st.found, st.low, st.below, st.tooFar, st.behind, st.unseen, st.covered, st.hidden, ms, traceCount - traces0);
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
    glm::vec3 fromOut{1.f, 0.f, 0.f}, toOut{1.f, 0.f, 0.f}; // the holds' ways out (the hand's turn on them)
    double last{-1.0};
    [[nodiscard]] float eased() const
    {
        return along * along * (3.f - 2.f * along);
    }
    [[nodiscard]] glm::vec3 hold() const
    {
        return glm::mix(from, to, eased());
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
    const bool right = hand == 1; // (the main hand, the right controller)
    const glm::vec3 rightward{-out.y, out.x, 0.f}; // facing the ledge (looking along -out), to the right
    return (out * vr_climb_hand_out.value + glm::vec3{0.f, 0.f, vr_climb_hand_up.value} +
               rightward * (right ? vr_climb_hand_side.value : -vr_climb_hand_side.value)) *
           cm;
}

// Quake angles to axes (forward, left, up), and back.
[[nodiscard]] glm::mat3 anglesAxes(const glm::vec3& a)
{
    glm::vec3 f, r, u;
    hands::angleVectors(a, f, r, u);
    return glm::mat3{f, -r, u};
}
[[nodiscard]] glm::vec3 axesAngles(const glm::mat3& m)
{
    return hands::anglesFromVectors(glm::normalize(m[0]), glm::normalize(m[2]));
}

// The drawn hand's turn on a hold whose way out (towards the drop) is `out` (its axes: forward, left, up, as the hand
// model's): facing the ledge, its front (the fist's knuckles) towards it, the palm down on the top, the fingers
// curling over the lip; turned by vr_climb_hand_pitch (the front up), _yaw (the front towards the other hand) and
// _roll (about the front: the thumb's side up), mirrored for the off hand (the left hand's model).
[[nodiscard]] glm::mat3 holdTurn(const glm::vec3& out, bool mirrored)
{
    const float side = mirrored ? -1.f : 1.f;
    const glm::vec3 up{0.f, 0.f, 1.f};
    glm::vec3 front{-out.x, -out.y, 0.f};
    front = glm::length(front) > 1e-4f ? glm::normalize(front) : glm::vec3{1.f, 0.f, 0.f};
    front = glm::angleAxis(glm::radians(vr_climb_hand_yaw.value) * side, up) * front;
    const glm::vec3 left = glm::cross(up, front); // facing the ledge, to the left
    // A right hand's palm faces its model's left (a gun's grip in it): turned a quarter about its front, down; the
    // mirrored left hand's the other way.
    glm::mat3 m{front, -up * side, left * side};
    m = glm::mat3_cast(glm::angleAxis(-glm::radians(vr_climb_hand_pitch.value), left)) * m;
    return m * glm::mat3_cast(glm::angleAxis(glm::radians(vr_climb_hand_roll.value) * side, glm::vec3{1.f, 0.f, 0.f}));
}

} // namespace

void qvr::climb::reset()
{
    for(Climber& c : climbers)
    {
        c = Climber{};
    }
}

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

float qvr::climb::drawnHand(const hands::State& s, int hand, const glm::mat3& handTurn, glm::vec3& pos, glm::vec3& rot,
    glm::vec3& lightShift)
{
    using namespace protocol;
    lightShift = glm::vec3{0.f};
    if(hand < 0 || hand > 1)
    {
        return 0.f;
    }
    Pin& pin = pins[hand];
    const float dt = pin.last >= 0.0 ? static_cast<float>(CLAMP(0.0, realtime - pin.last, 0.1)) : 0.f;
    pin.last = realtime;
    const int bits = cl.stats[STAT_QVR_CLIMB];
    if(bits & (1 << hand))
    {
        const int first = hand ? STAT_QVR_CLIMBMAINX : STAT_QVR_CLIMBOFFX;
        const float yaw = static_cast<float>((bits >> (14 + 8 * hand)) & 255) / yawSteps * glm::two_pi<float>();
        const glm::vec3 out{std::cos(yaw), std::sin(yaw), 0.f};
        pin.offset = handOffset(out, hand);
        const glm::vec3 hold = glm::vec3{static_cast<float>(cl.stats[first]), static_cast<float>(cl.stats[first + 1]),
                                   static_cast<float>(cl.stats[first + 2])} /
                                   statScale +
                               pin.offset;
        const int serial = (bits >> (2 + 6 * hand)) & 63;
        if(pin.weight <= 0.f)
        {
            pin.from = pin.to = hold;
            pin.fromOut = out;
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
            pin.fromOut = glm::normalize(glm::mix(pin.fromOut, pin.toOut, pin.eased()) + glm::vec3{1e-4f, 0.f, 0.f});
            pin.along = 0.f;
            pin.inTime = easeTime(glm::distance(hold, pin.from));
        }
        pin.to = hold; // (a hold on a moving brush model moves)
        pin.toOut = out;
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
        return 0.f;
    }
    const float w = pin.weight * pin.weight * (3.f - 2.f * pin.weight);

    // The hand's turn: the hold's (facing it), blended towards the controller's by vr_climb_hand_turn_blend (1: the
    // controller's, as it was), eased in and out with the hand's place.
    const glm::mat3 drawn = anglesAxes(rot) * handTurn;
    const bool mirrored = hand == 0; // (the off hand: the left hand's model, as vr_view.cpp draws it)
    const glm::quat onFrom = glm::quat_cast(holdTurn(pin.fromOut, mirrored)), onTo = glm::quat_cast(holdTurn(pin.toOut, mirrored));
    const glm::quat qDrawn = glm::normalize(glm::quat_cast(drawn));
    const glm::quat qHold = glm::normalize(glm::slerp(onFrom, onTo, pin.eased()));
    const glm::quat qBlend = glm::slerp(qHold, qDrawn, CLAMP(0.f, vr_climb_hand_turn_blend.value, 1.f));
    rot = axesAngles(glm::mat3_cast(glm::normalize(glm::slerp(qDrawn, qBlend, w))) * glm::transpose(handTurn));

    // The palm's middle on the hold (the palm where the hand, so turned, has it).
    const glm::vec3 palm = s.palmValid[hand] ? hands::redirect(s.palmLocal[hand], rot) : glm::vec3{0.f};
    const glm::vec3 pinned = pin.hold() - palm;
    pos = glm::mix(pos, pinned, w);
    if(vr_climb_debug.value >= 3.f)
    {
        // The drawn arm's shoulder (last frame's pose) against the server's estimate ("climbshoulder").
        avatar::Shoulder sh;
        if(avatar::shoulder(hand, sh))
        {
            Con_Printf("climbarm %s shoulder %.1f %.1f %.1f palm %.1f %.1f %.1f: %.1f units\n", handName(hand), sh.joint.x,
                sh.joint.y, sh.joint.z, pin.hold().x, pin.hold().y, pin.hold().z, glm::distance(sh.joint, pin.hold()));
        }
    }
    lightShift = -w * pin.offset;
    return w;
}
