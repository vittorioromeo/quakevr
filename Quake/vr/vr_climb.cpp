// vr_climb.cpp -- climbing: holds taken with either hand or both, hand over hand, shimmying and mantling (vr_climb,
// experimental; server side, and the drawn hands on their holds, client side).
//
// Holds. An empty hand can only take hold of a LEDGE of the ledge map (vr_ledges.cpp, made once from the BSP for each
// brush model: the lip of a walkable top with room over it for the hand and a drop of at least ledges::minDrop beyond
// it, starting at most ledges::edgeReach out, steps down on the way passed over: a ledge, a rung, a beam, a trimmed
// lip; not open floor, a wall or a stair). The hand is AT a ledge when it touches it (Touch: vr_climb_touch, the only
// distance there is at Grab Leniency 0): along the lip, between the hold's inset and the touch under its top (sunk into
// it, or into a rung) and the touch over it, and either over the top, in from the lip no further than the top goes and
// the drop is within vr_climb_over_top (that less how far out the drop starts), or in front of the face under the lip, at
// most the touch out, when the lip faces the body (the hand between the body and the ledge). The hand is the drawn
// hand's point, or the controller's own where the level holds the drawn hand out of it (gripHold: a hand reaching up
// under a rung stops with its fingers against the rung's underside).
// Of the ledges the hand is at, it takes (takenBefore) one it is over the top of before one it is in front of,
// the higher top (a rung above before the one below), the lip facing the body more (a rung's front, not its end; the
// near side of a thin wall), the nearer hold. The grip must be pressed at the hold (a press elsewhere, dragged onto
// one, does nothing; but in the air, not holding on, a press at no hold keeps looking while it stays pressed, for
// vr_climb_air_grab_time seconds: a ledge jumped at is in reach only as the hand passes its lip), with no weapon,
// carried object, locked force grab or flashlight in that hand, and not at a
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
// Holds on props (vr_climb_props). The solid physics props (Box3D's, SOLID_BBOX: crates, barrels, explosive boxes; not
// one in a hand, a ragdoll or a corpse) have ledges too, from their own shape as they lie now: each edge of a convex
// piece's walkable top (normal z at least ledges::minTopNormal) whose face beyond falls away (ledges::propLedges; the
// pieces' faces made once for each body: box3d::climbShape). Only the props round the hand are looked at (gather). The
// drop starts where the prop's own side, flaring out under the lip, leaves the column clear; the checks when a hold is
// taken see the props' shapes too (box3d::climbRay: another crate on the top, under the lip). From the ground, a prop's
// ledge must be vr_climb_prop_min_height over the feet (lower, the grip is the QC's: it picks the prop up); hanging or
// in the air, vr_climb_min_height as any. The hold keeps its place on the prop: each frame (followProps) it, its ledge
// and the way out are moved as the prop's body moved (turned too). A prop at least vr_climb_prop_mass kg (or pinned)
// holds like the level: nothing pushes it, and its motion carries the body as a moving brush's does. A lighter one is
// pulled (pullProps): the body's weight shared by the holding hands, at most vr_climb_prop_pull kg a hand, from the hold
// towards the body (a body over the hold presses it down), and doesn't carry the body (a crate pulled towards you
// doesn't drag you after it: no loop). Its hold lets go once it moved vr_climb_prop_slip from where it was taken; any
// prop's lets go once its top turned vr_climb_prop_tilt from how it lay (tipped), its speed changed by
// vr_climb_prop_letgo_speed in a frame (knocked away), or it is gone (broken, taken, not solid). The player's own
// capsule still meets it (a light crate tipping towards you leans on you, it doesn't fall through you); the hand's own
// bodies (the open hand, the fist) are off while it holds a prop (vr_box3d.cpp: they'd shove it). A mantle onto a prop
// follows it as it lies, and ends standing on it (VR_StandsOn's ground).
//
// Holds on ropes (vr_rope_climb; the climbable ropes and chains, vr_climbrope.cpp, docs/vr-port/ROPES.md "Climbing"). A
// grip at a rope (ropeHold: the hand within vr_climb_touch and a palm's breadth of its surface, plus the leniency) takes
// it there, before any ledge: the hold is kept as the rope, how far along it and how far straight from its top, and
// each frame (followRopes) it is where the rope has it now. A climber whose hands are only on one rope rides it
// (reportRopes, climbrope::ride): his weight is a bob swinging round its top, the rope straight from the top through his
// hands, the holds on that line; their motion carries the body as a moving brush's does (holdsCarry), so the body
// swings with it, and a body that couldn't follow (a wall) stops the swing that way (climbrope::riderBlocked). Hand over
// hand: a higher grip, pulled down, lifts the body as on a ledge (the hold slides nowhere; no mantle from a rope).
// Letting go of everything keeps the rope's swing at the hands as well as the hands' fling (fallFlung, up to
// ropeLetGoMax); a jump press while holding one lets go of it and adds vr_rope_jump up (ropeJump). Moving holds: the
// hang runs in the server's frame (holdsStill), as a prop's.
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
// Mantle: once a hand has pulled down by `mantlePull` units since it took hold and the head is vr_climb_mantle_height units
// above its ledge, and the player's box fits on top (with floor under it) and the way there (straight up, then over)
// is clear, the body is carried there in `mantleTime` seconds, and stands. The ledge map knows how deep the top is at
// each place along the lip: a spot whose middle is over the top needs only the sweep over to it (and the sweep up,
// shared by the spots on its side); elsewhere the traces below the box as before. The spot is 22 to 38 units in from the lip;
// on a narrower top (a wall's, a beam's: at least `minFooting` deep), over its middle, or further out, as long as the
// box's middle is over it (it stands on the top, not on its edge over the drop). With no room on top (a ceiling, a top
// too thin), it hangs on, and the holding hands feel a soft buzz as the pull over the top starts; the push over the top
// that follows doesn't push it back off the face (the pull that brought the hands in was owed, above), and at most
// straightens the arms. With vr_climb_mantle_trigger the mantle waits there, high enough, for the last holding hand to
// let go or for a jump (triggeredMantle): he can hang and look over the top first; let go lower, he falls as ever.
//
// Letting go of everything: the player falls, flung by the hands' release (vr_climb_fling, capped). No new hold for
// `regrabDelay` seconds after letting go of everything or mantling; holds lower than vr_climb_min_height above the
// feet are ignored.
//
// The QC never sees the grip of a hand that holds (its grab bits are masked in .vrbits0 from the press until the
// release), so the press picks nothing up; instead .vrbits0 bits 15 (off hand) and 16 (main hand) tell it the hand
// holds, both while it mantles (QVR_VRBITS0_*HAND_CLIMBING: no melee blows, no force grab). The client gets the holds as stats (STAT_QVR_CLIMB*).

#include "vr_climb.hpp"
#include "vr_avatar.hpp"
#include "vr_bodycal.hpp"
#include "vr_box3d.hpp"
#include "vr_climbrope.hpp"
#include "vr_cvars.hpp"
#include "vr_flatplay.hpp"
#include "vr_tunables.hpp"
#include "vr_engine.hpp"
#include "vr_hands.hpp"
#include "vr_hull.hpp"
#include "vr_ledges.hpp"
#include "vr_mem.hpp"
#include "vr_move.hpp"
#include "vr_physsound.hpp"
#include "vr_pmove.hpp"
#include "vr_progs.hpp"
#include "vr_protocol.hpp"
#include "vr_server.hpp"
#include "vr_units.hpp"

#include "Zancle/Algorithm/AnyOf.hpp"
#include "Zancle/Algorithm/Sort.hpp"
#include "Zancle/Algorithm/StableSort.hpp"
#include "Zancle/Base/GetArraySize.hpp"
#include "Zancle/Base/IsFinite.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Base/Strcmp.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Acos.hpp"
#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Floor.hpp"
#include "Zancle/Math/Hypot.hpp"
#include "Zancle/Math/Lround.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/Math/Sqrt.hpp"
#include "Zancle/Math/Tan.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/Vocabulary/Optional.hpp"
#include "vr_zancle.hpp"
#include "vr_geom.hpp"
#include "vr_api.h"


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
constexpr int HS_AMMO_POUCH = 12;

using ledges::holdInset;              // the hold (the palm's middle) is this far behind the lip (vr_ledges.hpp)
using qvr::climb::regrabDelay;      // seconds without new holds after letting go of everything or mantling (vr_climb.hpp)
// (The body follows the hands at pmove::hangMaxSpeed units a second at most, and stays within pmove::hangMaxReach
// units, horizontally, of each hold: vr_pmove.hpp, the hang's steps.)
// (A mantle's pull down, its search's retry and the top it stands on: pmove::mantlePull, mantleRetry, minFooting.)
constexpr float mantleTime = 0.3f;    // seconds the mantle takes
constexpr float flingMax = 200.f;     // units / second the release flings at most,
constexpr float flingMaxUp = 150.f;   // and upwards
constexpr float ropeLetGoMax = 1000.f; // units / second letting go of a rope keeps at most (its swing, the fling, a jump)
constexpr float pumpFullMove = 400.f;  // the move (forwardmove, sidemove) that pumps a rope's swing fully
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

using geom::vec;

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

// Who looks for a hold (findHold; MP B8j part 2): the server's player (`player`: the world, the brush models and props
// round the hand, the ladders; his edict's lines) or a remote client's replay (`body`: the world alone, its own lines:
// the world's hull 0 and the brush models, as MOVE_NOMONSTERS), the body at `origin`, its feet at `feet`.
struct Seeker
{
    edict_t* player{nullptr};
    pmove::Body* body{nullptr};
    glm::vec3 origin{0.f};
    float feet{0.f};

    [[nodiscard]] trace_t line(const glm::vec3& a, const glm::vec3& b) const
    {
        return player ? traceLine(a, b, player) : body->line(&a.x, &b.x);
    }
};

[[nodiscard]] Seeker seekerOf(edict_t* ent)
{
    return Seeker{ent, nullptr, vec(ent->v.origin), ent->v.origin[2] + ent->v.mins[2]};
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
    edict_t* ent{nullptr};      // the brush model it belongs to (nullptr: the world), or the prop
    const ledges::Map* map{nullptr}; // its ledge in the ledge map: the model's map, the ledge,
    int edge{-1};
    float t{0.f};                    // and the hold's place along it
    // A prop's (see "Holds on props"): its ledge and that ledge's sample as it lies now (world space; `map` is the search's
    // and goes with it).
    bool prop{false};
    ledges::Edge propEdge;
    ledges::Sample propSample;
    bool volume{false}; // AD support: a ladder volume's own rung (ladderHold), no solid under it
    bool drawn{false};  // and of those, one on a drawn rung (drawnRung)
    // A ladder volume's (hubladder): how far out from the hold its ladder's near side is (the drawn bar's face, else the
    // volume's), and its middle across its width (the body kept out of it and, flat-screen, centred on it: ladderKeep).
    float ladderFace{0.f};
    glm::vec2 ladderMid{0.f};
    // A climbable rope's (see "Holds on ropes"): `ent` the rope's entity, the hold `ropeS` along it from its top and
    // `ropeD` straight from it.
    bool rope{false};
    float ropeS{0.f}, ropeD{0.f};
};

// How near a ledge the hand's point must be to be AT it (Grab Leniency 0; the leniency adds its distance round that):
// touching it, vr_climb_touch (cm) from its top or from the face under its lip, or in it (sunk into its top, or into a
// rung) no deeper than the hold's inset and that; over the top as far in as the ledge map's ledge goes and at most
// vr_climb_over_top (units) from the drop.
struct Touch
{
    float over{0.f};  // above the top
    float front{0.f}; // in front of the face under the lip (that faces the body)
    float sink{0.f};  // under the top (sunk into the ledge or a rung, in front of its face)
    float in{0.f};    // over the top, how far in from the drop (so the lip less how far out the drop starts)
};

[[nodiscard]] Touch touch()
{
    const float t = za::max(0.f, vr_climb_touch.value) * 0.01f * units::metresToUnits();
    return Touch{t, t, holdInset + t, za::max(0.f, vr_climb_over_top.value)};
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
    p.t = za::clamp(raw, 0.f, e.len);
    const glm::vec3 lip = e.point(p.t);
    const ledges::Sample& s = m.sampleAt(e, p.t);
    const glm::vec2 out{e.out};
    p.facing = glm::dot(out, toBody);
    // In from the lip as far as the top goes and the drop is within reach; out in front of it when it faces the body.
    const Touch k = touch();
    const float in = za::max(0.f, za::min(s.depth, k.in - s.dropOut));
    const float front = p.facing > 0.f ? k.front : 0.f;
    const float across = glm::dot(glm::vec2{hand} - glm::vec2{lip}, out);
    const float across1 = za::clamp(across, -in, front);
    const glm::vec3 overAt = lip + e.out * za::min(across1, 0.f);
    const float surface = e.topAt(overAt); // the top under the hand (the lip's height in front of it)
    const float up = hand.z - surface;
    const float up1 = za::clamp(up, -k.sink, k.over);
    const float da = raw - p.t, ds = across - across1, dz = up - up1;
    p.dist = za::sqrt(da * da + ds * ds + dz * dz);
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
    if(za::abs(a.top - b.top) > 0.01f)
    {
        return a.top > b.top;
    }
    if(za::abs(a.facing - b.facing) > 1e-3f)
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
    bool prop{false}; // a prop's (`ent`; the search's map of the props' ledges as they lie now)

    [[nodiscard]] const ledges::Edge& ledge() const
    {
        return map->edges[static_cast<za::SizeT>(edge)];
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
        if(prop)
        {
            l.prop = true;
            l.propEdge = ledge();
            l.propSample = map->sampleAt(ledge(), place.t);
        }
        return l;
    }
};

[[nodiscard]] glm::vec2 towardsBody(const Seeker& s, const glm::vec3& hand)
{
    const glm::vec2 d{s.origin.x - hand.x, s.origin.y - hand.y};
    return glm::length(d) >= 1.f ? glm::normalize(d) : glm::vec2{0.f};
}

// A lenient candidate (findHold): its score (the lower the nearer), and where the hand would take it.
struct Scored
{
    float score;
    Candidate c;
    glm::vec3 at;
};

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

// A candidate's frame and box (freePoint: each made once per search, not once per candidate checked against it).
struct FrameBox
{
    Frame f{};
    Box b{};
    bool made{false};
};

// The hold search's buffers (the server's frame: the main thread).
struct ClimbScratch
{
    za::Vector<int> nearby;             // the ledges of one model near the hand (gather)
    za::Vector<edict_t*> areaEdicts;    // the entities round the hand (gather)
    za::Vector<Candidate> found;        // the ledges near the hand (findHold)
    za::Vector<edict_t*> movers;        // the brush models round it (findHold)
    za::Vector<const Candidate*> order; // those the hand is at, the first taken first (findHold)
    za::Vector<Scored> scored;          // the lenient ones (findHold)
    za::Vector<FrameBox> frameBoxes;    // the found ones' frames and boxes, by index (findHold, freePoint)
    ledges::Map props;                  // the props' ledges round the hand as they lie now (gather),
    za::Vector<int> propNums;           // and each one's prop
    auto members() { return qvr::mem::list(nearby, areaEdicts, found, movers, order, scored, frameBoxes, props, propNums); }
};
mem::Scratch<ClimbScratch> scratch{"climb"};

// What the search tried and turned down (vr_climb_try).
struct LenientStats
{
    int points{0}, found{0}, low{0}, below{0}, tooFar{0}, behind{0}, unseen{0}, covered{0}, hidden{0};
};

// The ledges the hand at `hand` is within `radius` of being at (see the top of the file), in the world and the brush
// models round it (`movers`: those, SOLID_BSP, their boxes near).
void gather(const Seeker& s, const glm::vec3& hand, float radius, za::Vector<Candidate>& out, za::Vector<edict_t*>& movers,
    LenientStats& st)
{
    edict_t* const player = s.player;
    const glm::vec2 toBody = towardsBody(s, hand);
    const Touch k = touch();
    const glm::vec3 reach{radius + za::max(k.in, k.sink) + 1.f}; // the furthest a hand at a ledge is from its lip
    const glm::vec3 lo = hand - reach, hi = hand + reach;
    za::Vector<int>& nearby = scratch.nearby;
    const auto from = [&](const ledges::Map& m, edict_t* ent, const glm::vec3& offset) {
        nearby.clear();
        m.nearby(lo - offset, hi - offset, nearby);
        for(const int i : nearby)
        {
            st.points++;
            const Place p = placeOf(m, m.edges[static_cast<za::SizeT>(i)], hand - offset, toBody);
            if(p.dist <= radius + 1e-3f)
            {
                out.pushBack(Candidate{&m, i, ent, offset, p});
            }
        }
    };
    if(const ledges::Map* world = ledges::world())
    {
        from(*world, nullptr, glm::vec3{0.f});
    }
    if(!player)
    {
        return; // (a replay's: the world alone)
    }
    za::Vector<edict_t*>& list = scratch.areaEdicts;
    list.resize(static_cast<za::SizeT>(za::max(1, qcvm->num_edicts)));
    int count = 0;
    vec3_t mins{lo.x, lo.y, lo.z}, maxs{hi.x, hi.y, hi.z};
    SV_AreaEdicts(mins, maxs, list.data(), &count, static_cast<int>(list.size()));
    ledges::Map& props = scratch.props;
    props.edges.clear();
    props.samples.clear();
    scratch.propNums.clear();
    for(int i = 0; i < count; i++)
    {
        edict_t* e = list[static_cast<za::SizeT>(i)];
        const int index = static_cast<int>(e->v.modelindex);
        // A solid prop (see "Holds on props"): its ledges as it lies now, near the hand.
        if(vr_climb_props.value != 0.f && e != player && !e->free && static_cast<int>(e->v.solid) == SOLID_BBOX)
        {
            box3d::ClimbBody body;
            if(const box3d::ClimbShape* shape = box3d::climbShape(NUM_FOR_EDICT(e), body))
            {
                const za::SizeT first = props.edges.size();
                ledges::propLedges(NUM_FOR_EDICT(e), *shape, body, lo, hi, props, scratch.propNums);
                for(za::SizeT k = first; k < props.edges.size(); k++)
                {
                    st.points++;
                    const Place p = placeOf(props, props.edges[k], hand, toBody);
                    if(p.dist <= radius + 1e-3f)
                    {
                        Candidate c{&props, static_cast<int>(k), e, glm::vec3{0.f}, p};
                        c.prop = true;
                        out.pushBack(c);
                    }
                }
            }
            continue;
        }
        if(e == player || e->free || static_cast<int>(e->v.solid) != SOLID_BSP || index <= 0 || index >= MAX_MODELS)
        {
            continue;
        }
        movers.pushBack(e);
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
[[nodiscard]] Check check(const Seeker& s, const Candidate& c, const glm::vec3& at, bool lenient, const glm::vec3& hand,
    const glm::vec3& headPos, const za::Vector<edict_t*>& movers)
{
    const ledges::Edge& e = c.ledge();
    const glm::vec3 up{0.f, 0.f, 1.f};
    const glm::vec3 hold = c.place.hold + c.offset;
    const float top = c.place.top + c.offset.z;
    // (A prop's ledge: the props' shapes too, its own among them: another prop on it, under the lip, in the way.)
    const auto clear = [&](const glm::vec3& a, const glm::vec3& b) {
        const trace_t t = s.line(a, b);
        return t.fraction >= 1.f && !t.startsolid && !(c.prop && box3d::climbRay(a, b));
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
        if(!clear(glm::vec3{at.x, at.y, za::max(at.z + touch().sink, surface + 0.5f)}, glm::vec3{at.x, at.y, surface + 0.25f}))
        {
            return Check::unreached;
        }
    }
    else
    {
        const glm::vec3 lip = e.point(c.place.t) + c.offset;
        const glm::vec3 over{at.x, at.y, za::max(at.z, lip.z + 1.f)};
        const glm::vec3 in = glm::vec3{lip.x, lip.y, over.z} - e.out * 1.f;
        if((over.z > at.z && !clear(at, over)) || !clear(over, in))
        {
            return Check::unreached;
        }
    }
    if(lenient)
    {
        const bool handInWall = s.line(hand, hand).startsolid;
        const glm::vec3 lip = glm::vec3{hold.x, hold.y, top + 1.f} + e.out * (ledges::holdInset + 1.f);
        if(!clear(handInWall ? headPos : hand, lip))
        {
            return Check::unseen;
        }
    }
    return Check::ok;
}

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
    const float in = za::max(0.f, za::min(s.depth, k.in - s.dropOut));
    return Box{{0.f, -in, -k.sink}, {e.len, c.place.facing > 0.f ? k.front : 0.f, k.over}};
}

// Where, of candidate `c`'s place, the hand at `hand` would take `c`'s ledge (no other ledge taken before it there,
// takenBefore), nearest the hand: the place less the others' places that come first, as boxes in `c`'s frame (those
// square or parallel to it: rungs above and below, a rung's front and its ends, a thin wall's two sides; any other is
// only checked at the point found). False: nowhere.
[[nodiscard]] bool freePoint(const Candidate& c, const za::Vector<Candidate>& all, za::Vector<FrameBox>& frameBoxes,
    const glm::vec3& hand, const glm::vec2& toBody, glm::vec3& point)
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
    za::Vector<Blocker> blockers;
    const bool flat = za::abs(e.dir.z) < 1e-3f;
    if(flat)
    {
        for(const Candidate& k : all)
        {
            const ledges::Edge& ke = k.ledge();
            const float cosine = za::abs(glm::dot(ke.dir, e.dir));
            if(&k == &c || za::abs(ke.dir.z) >= 1e-3f || (cosine > 1e-3f && cosine < 1.f - 1e-3f))
            {
                continue;
            }
            FrameBox& fb = frameBoxes[static_cast<za::SizeT>(&k - &all[0])];
            if(!fb.made)
            {
                fb = FrameBox{frameOf(k), boxOf(k), true};
            }
            const Frame& kf = fb.f;
            const Box& kb = fb.b;
            Box b{glm::vec3{1e9f}, glm::vec3{-1e9f}};
            for(int i = 0; i < 8; i++)
            {
                const glm::vec3 corner{(i & 1) ? kb.hi.x : kb.lo.x, (i & 2) ? kb.hi.y : kb.lo.y, (i & 4) ? kb.hi.z : kb.lo.z};
                const glm::vec3 q = f.to(kf.from(corner));
                b.lo = glm::min(b.lo, q);
                b.hi = glm::max(b.hi, q);
            }
            blockers.pushBack(Blocker{b, &k});
        }
    }
    const glm::vec3 h = f.to(hand);
    float best = 1e30f;
    bool found = false;
    // The nearest point of `box` not in a blocker that comes first there: split round the first such blocker met.
    // (itself as an argument, not `this auto`: the Android NDK's clang 18 cannot compile a deducing-this lambda's captures)
    const auto search = [&](const auto& search, const Box& box, int depth) -> void {
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
                below.hi[axis] = za::min(box.hi[axis], b.box.lo[axis] - 2e-3f);
                above.lo[axis] = za::max(box.lo[axis], b.box.hi[axis] + 2e-3f);
                if(below.lo[axis] <= below.hi[axis])
                {
                    search(search, below, depth + 1);
                }
                if(above.lo[axis] <= above.hi[axis])
                {
                    search(search, above, depth + 1);
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
        search(search, boxOf(c), 0);
    }
    else
    {
        // (a sloping lip: its place's point nearest the hand)
        const glm::vec3 world = c.place.nearest + c.offset;
        found = !za::anyOf(all.begin(), all.end(), [&](const Candidate& k) { return &k != &c && takenFirst(k, world); });
        point = world;
    }
    return found;
}

// The hold a grip of the hand at `hand` takes (see the top of the file): one the hand is at, else (leniency, `radius`
// units) the nearest it is within `radius` of being at. `vel`: the hand's velocity (m/s); `headPos`: the head.
// `lenient`: whether the hold is a lenient one.
// AD support: trigger volumes gate the setting; only real rung geometry supplies handholds.
[[nodiscard]] bool ladderDisabled(const glm::vec3& hand)
{
    const int ofs = progs::fields().vr_climb_ladder;
    if(ofs < 0 || vr_climb_ladders.value != 0.f) { return false; }
    const float margin = touch().front;
    for(int i = 1; i < qcvm->num_edicts; ++i)
    {
        edict_t* e = EDICT_NUM(i);
        if(e->free || progs::fieldFloat(e, ofs) <= 0.f || static_cast<int>(e->v.solid) != SOLID_TRIGGER) { continue; }
        bool inside = true;
        for(int k = 0; k < 3; ++k)
        {
            inside = inside && hand[k] >= e->v.absmin[k] - margin && hand[k] <= e->v.absmax[k] + margin;
        }
        if(inside) { return true; }
    }
    return false;
}

// AD support: how far above and under the hand, and how deep into the ladder from it, a ladder's rungs count as within
// reach (rungNear), in units.
constexpr float ladderRungReach = 24.f;
constexpr float ladderRungDepth = 32.f;

// AD support: whether a ladder has a real rung within reach of the hand: a walkable solid top, at least `lowest` high,
// in the columns from `at` (the hand's probe) up to ladderRungDepth further into the ladder (along -`out`), from
// ladderRungReach above the hand to as far under it. Short segments, so a column that starts in the wall still finds the
// tops under it.
[[nodiscard]] bool rungNear(edict_t* player, const glm::vec3& at, const glm::vec3& out, float lowest)
{
    constexpr float step = 8.f;
    const glm::vec3 up{0.f, 0.f, 1.f};
    for(float depth = 0.f; depth <= ladderRungDepth; depth += step)
    {
        const glm::vec3 column = at - out * depth;
        for(float dz = ladderRungReach; dz > -ladderRungReach; dz -= step)
        {
            const trace_t tr = traceLine(column + up * (dz + 0.5f), column + up * (dz - step), player);
            if(tr.startsolid || tr.allsolid || tr.fraction >= 1.f || tr.plane.normal[2] < ledges::minTopNormal)
            {
                continue;
            }
            if(tr.endpos[2] >= lowest)
            {
                return true;
            }
        }
    }
    return false;
}

// AD support: closely spaced rungs can fail the general ledge's 32-unit drop rule. A ladder may
// relax that drop only: its hold must still be on an actual walkable solid top within hand-touch distance. A ladder with
// no real rung within reach of the hand (rungNear: a flat ladder-textured wall, or rungs farther apart than a reach) is
// held in its volume (or within the touch distance of it, plus `slack` across: findHold's leniency), facing the body,
// so no ladder is left unclimbable: on its drawn rungs (drawnRung: a non-solid model's bars over it), else on its own
// rungs, ladderRungStep apart down from the volume's top (a Quoth or AD
// ladder is a trigger_ladder brush over a ladder drawn on a wall, his request 2026-11-21), so a hand takes the nearest as
// a brush ladder's rung and the flat ladder (findRungs) finds two one over the other. `lowest`: the lowest a real rung
// may be (the feet plus Lowest Ledge).
constexpr float ladderRungStep = 16.f; // units: a ladder volume's own rungs, down from its top
constexpr float ladderInWall = 12.f;   // units: how far into the wall behind a ladder volume a hand still holds it

// A drawn ladder's rungs: the up-facing faces of a non-solid brush model over a ladder volume (a func_illusionary or a
// static one: kellbase1's pillar ladders, his note 2026-11-22 16:06, "the position of the hands and the ledges doesn't
// match the actual rungs"), within drawnRungMargin behind or before it (across its thinner side: not its rails' caps
// beside it) and inside its height. The top nearest `z` and that rung's footprint (its faces at that top), or nothing:
// no rung drawn there (the volume's own rungs then): none within drawnRungReach of `z`, or a single top only (no ladder
// drawn: ad_e1m1's has one face at its top, which took every hand on it, and the flat ladder found one rung).
constexpr float drawnRungMargin = 12.f; // units: how far before or behind a ladder volume its drawn rungs are looked for
constexpr float drawnRungReach = 40.f;  // units: the farthest from `z` a drawn rung is still its (the flat ladder's ladderGap)

constexpr float drawnBarDepth = 12.f;   // units: a drawn rung's faces wholly this far under its top are the bar's (its depth)

struct DrawnRung
{
    float top{0.f};
    glm::vec2 lo{0.f}, hi{0.f};       // its ridge (its top faces' vertices at its top): the bar's line
    glm::vec2 barLo{0.f}, barHi{0.f}; // the whole bar's (its faces within drawnBarDepth under its top): its depth across
};

// A brush model's surface's vertices' bounds (its mins, maxs are padded).
void surfBounds(const qmodel_t* m, const msurface_t& surf, float (&fmin)[3], float (&fmax)[3])
{
    for(int axis = 0; axis < 3; ++axis)
    {
        fmin[axis] = 1e30f;
        fmax[axis] = -1e30f;
    }
    for(int i = 0; i < surf.numedges; ++i)
    {
        const int se = m->surfedges[surf.firstedge + i];
        const float* p = m->vertexes[se >= 0 ? m->edges[se].v[0] : m->edges[-se].v[1]].position;
        for(int axis = 0; axis < 3; ++axis)
        {
            fmin[axis] = za::min(fmin[axis], p[axis]);
            fmax[axis] = za::max(fmax[axis], p[axis]);
        }
    }
}

[[nodiscard]] za::Optional<DrawnRung> drawnRung(const edict_t* vol, float z)
{
    if(!sv.worldmodel)
    {
        return za::nullOpt;
    }
    const int thin = vol->v.maxs[0] - vol->v.mins[0] <= vol->v.maxs[1] - vol->v.mins[1] ? 0 : 1;
    glm::vec3 lo, hi;
    for(int axis = 0; axis < 3; ++axis)
    {
        const float grow = axis == thin ? drawnRungMargin : axis == 2 ? 1.f : 0.f;
        lo[axis] = vol->v.origin[axis] + vol->v.mins[axis] - grow;
        hi[axis] = vol->v.origin[axis] + vol->v.maxs[axis] + grow;
    }
    za::Optional<DrawnRung> best;
    int bestModel = -1;
    float topLow = 1e30f, topHigh = -1e30f; // (all its tops: a ladder has two or more)
    const auto overlaps = [&](const float* mn, const float* mx) {
        return mn[0] <= hi.x && mx[0] >= lo.x && mn[1] <= hi.y && mx[1] >= lo.y && mn[2] <= hi.z && mx[2] >= lo.z;
    };
    for(int k = 2; k < MAX_MODELS && k <= sv.worldmodel->numsubmodels; ++k) // (sv.models[k]: the submodel *(k - 1))
    {
        const qmodel_t* m = sv.models[k];
        if(!m || m->type != mod_brush || !overlaps(m->mins, m->maxs))
        {
            continue;
        }
        bool drawnOnly = true; // (no solid or trigger entity of it: a mover's or a volume's faces are not rungs)
        for(int i = 1; i < qcvm->num_edicts && drawnOnly; ++i)
        {
            const edict_t* e = EDICT_NUM(i);
            drawnOnly = e->free || static_cast<int>(e->v.modelindex) != k || static_cast<int>(e->v.solid) == SOLID_NOT;
        }
        if(!drawnOnly)
        {
            continue;
        }
        for(int s = 0; s < m->nummodelsurfaces; ++s)
        {
            const msurface_t& surf = m->surfaces[m->firstmodelsurface + s];
            const float nz = (surf.flags & SURF_PLANEBACK) ? -surf.plane->normal[2] : surf.plane->normal[2];
            if(nz < ledges::minTopNormal || !overlaps(surf.mins, surf.maxs))
            {
                continue;
            }
            float fmin[3], fmax[3];
            surfBounds(m, surf, fmin, fmax);
            if(!overlaps(fmin, fmax))
            {
                continue;
            }
            const float top = fmax[2];
            topLow = za::min(topLow, top);
            topHigh = za::max(topHigh, top);
            glm::vec2 rlo{1e30f}, rhi{-1e30f}; // (its ridge: its vertices at its top, a bar's line)
            for(int i = 0; i < surf.numedges; ++i)
            {
                const int se = m->surfedges[surf.firstedge + i];
                const float* p = m->vertexes[se >= 0 ? m->edges[se].v[0] : m->edges[-se].v[1]].position;
                if(p[2] >= top - 0.05f)
                {
                    rlo = glm::min(rlo, glm::vec2{p[0], p[1]});
                    rhi = glm::max(rhi, glm::vec2{p[0], p[1]});
                }
            }
            if(best && za::abs(top - best->top) < 0.5f)
            {
                best->lo = glm::min(best->lo, rlo); // (another face of that rung)
                best->hi = glm::max(best->hi, rhi);
            }
            else if(!best || za::abs(top - z) < za::abs(best->top - z))
            {
                best = za::Optional<DrawnRung>{DrawnRung{top, rlo, rhi}};
                bestModel = k;
            }
        }
    }
    if(!best || topHigh - topLow < 4.f || za::abs(best->top - z) > drawnRungReach)
    {
        return za::nullOpt;
    }
    // The whole bar: its model's faces wholly within drawnBarDepth under its top, along its ridge (not the rails beside
    // it, which run past that depth, nor the next bar down).
    best->barLo = best->lo;
    best->barHi = best->hi;
    const qmodel_t* m = sv.models[bestModel];
    const int along = best->hi.x - best->lo.x <= best->hi.y - best->lo.y ? 1 : 0;
    for(int s = 0; s < m->nummodelsurfaces; ++s)
    {
        float fmin[3], fmax[3];
        surfBounds(m, m->surfaces[m->firstmodelsurface + s], fmin, fmax);
        if(fmin[2] >= best->top - drawnBarDepth && fmax[2] <= best->top + 0.05f && fmin[along] <= best->hi[along] &&
            fmax[along] >= best->lo[along] && overlaps(fmin, fmax))
        {
            best->barLo = glm::min(best->barLo, glm::vec2{fmin[0], fmin[1]});
            best->barHi = glm::max(best->barHi, glm::vec2{fmax[0], fmax[1]});
        }
    }
    return best;
}

[[nodiscard]] za::Optional<Ledge> ladderHold(edict_t* player, const glm::vec3& hand, const glm::vec3& eye, float lowest, float slack = 0.f)
{
    const int ofs = progs::fields().vr_climb_ladder;
    if(ofs < 0 || vr_climb_ladders.value == 0.f) { return za::nullOpt; }
    const Touch k = touch();
    const glm::vec2 toBody = towardsBody(seekerOf(player), hand);
    const glm::vec3 out{toBody.x, toBody.y, 0.f}, up{0.f, 0.f, 1.f};
    // (A hand pushed into the wall a thin trigger_ladder brush stands in front of holds it too.)
    glm::vec3 probe = hand;
    const float across = slack + (SV_PointContents(&probe[0]) == CONTENTS_SOLID ? ladderInWall : 0.f);
    for(int i = 1; i < qcvm->num_edicts; ++i)
    {
        edict_t* e = EDICT_NUM(i);
        if(e->free || progs::fieldFloat(e, ofs) <= 0.f || static_cast<int>(e->v.solid) != SOLID_TRIGGER) { continue; }
        bool inside = true;
        for(int axis = 0; axis < 3; ++axis)
        {
            const float margin = k.front + (axis < 2 ? across : 0.f);
            inside = inside && hand[axis] >= e->v.absmin[axis] - margin && hand[axis] <= e->v.absmax[axis] + margin;
        }
        if(!inside) { continue; }
        // Probe just inside the rung, not the trigger's broad box or a guessed rung spacing.
        const glm::vec3 at = hand - out * (holdInset + k.front);
        const trace_t tr = traceLine(at + up * (k.over + 0.125f), at - up * k.sink, player);
        const bool onTop = !tr.startsolid && !tr.allsolid && tr.fraction < 1.f && tr.plane.normal[2] >= ledges::minTopNormal;
        const glm::vec3 top = vec(tr.endpos);
        const bool reached = onTop && [&] {
            const trace_t reach = traceLine(hand + up * (k.over + 0.125f), top + up * 0.125f, player);
            return !reach.startsolid && reach.fraction >= 1.f;
        }();
        if(!reached)
        {
            if(rungNear(player, at, out, lowest))
            {
                continue; // a real rung within reach: the hand must take that
            }
            // The volume's own rung nearest the hand, where the hand is across the volume (clamped into it).
            const float lo = e->v.origin[2] + e->v.mins[2], hi = e->v.origin[2] + e->v.maxs[2]; // (not the link's padded box)
            const float down = za::clamp(za::round((hi - hand.z) / ladderRungStep), 0.f, za::floor((hi - lo) / ladderRungStep));
            Ledge l;
            const za::Optional<DrawnRung> drawn = drawnRung(e, hand.z); // (its drawn rungs: a headset's and the flat ladder's)
            l.top = drawn ? drawn->top : hi - down * ladderRungStep;
            // (Facing the eye: a headset's head is with the body; the flat ladder's mount looks from over the drop.)
            const glm::vec2 toEye{eye.x - hand.x, eye.y - hand.y};
            l.out = glm::length(toEye) > 1.f ? glm::vec3{glm::normalize(toEye), 0.f} : out;
            l.volume = true;
            l.drawn = drawn.hasValue();
            const glm::vec3 org = vec(e->v.origin);
            l.hold = glm::vec3{za::clamp(hand.x, org.x + e->v.mins[0], org.x + e->v.maxs[0]),
                za::clamp(hand.y, org.y + e->v.mins[1], org.y + e->v.maxs[1]), l.top + ledges::holdLift};
            // (The ladder's width: the volume's, then the drawn bar's; its near side: the drawn bar's face, a volume
            // alone has none drawn.)
            const glm::vec2 vlo{org.x + e->v.mins[0], org.y + e->v.mins[1]}, vhi{org.x + e->v.maxs[0], org.y + e->v.maxs[1]};
            l.ladderMid = 0.5f * (vlo + vhi);
            if(drawn)
            {
                // On the drawn rung itself: along it where the hand is; across it on its near side, as a ledge's hold is
                // behind its lip (hubladder, his note 2026-11-23: the hands were through the bars, the hold on their
                // middle), the way out square to the bar (the hand faces it).
                const int thin = drawn->hi.x - drawn->lo.x <= drawn->hi.y - drawn->lo.y ? 0 : 1; // (across the bar)
                const float side = l.out[thin] >= 0.f ? 1.f : -1.f;
                l.out = glm::vec3{0.f};
                l.out[thin] = side;
                const float depth = drawn->barHi[thin] - drawn->barLo[thin];
                const float face = side > 0.f ? drawn->barHi[thin] : drawn->barLo[thin];
                for(int axis = 0; axis < 2; ++axis)
                {
                    l.hold[axis] = axis == thin ? face - side * za::min(holdInset, 0.5f * depth)
                                                : za::clamp(hand[axis], drawn->lo[axis], drawn->hi[axis]);
                }
                l.ladderFace = za::min(holdInset, 0.5f * depth);
                l.ladderMid[1 - thin] = 0.5f * (drawn->lo[1 - thin] + drawn->hi[1 - thin]);
            }
            return za::Optional<Ledge>{l};
        }
        Ledge l;
        l.top = top.z;
        l.out = out;
        l.hold = top + up * ledges::holdLift;
        l.ent = tr.ent && tr.ent != EDICT_NUM(0) ? tr.ent : nullptr;
        return za::Optional<Ledge>{l};
    }
    return za::nullOpt;
}

[[nodiscard]] za::Optional<Ledge> findHold(const Seeker& s, const glm::vec3& hand, const glm::vec3& vel,
    const glm::vec3& headPos, float radius, float propLow, bool& lenient, LenientStats* stats = nullptr, float lower = 0.f)
{
    // (`lower`: holds that much under Lowest Ledge too: the flat ladder's top, at the feet, mounted from over it)
    if(s.player && ladderDisabled(hand)) { return za::nullOpt; } // AD support (the server's)
    LenientStats st;
    lenient = false;
    za::Vector<Candidate>& found = scratch.found;
    za::Vector<edict_t*>& movers = scratch.movers;
    found.clear();
    movers.clear();
    gather(s, hand, radius, found, movers, st);
    const float feet = s.feet;
    const float lowest = feet + vr_climb_min_height.value - lower;
    const auto finish = [&](const Candidate* c) -> za::Optional<Ledge> {
        if(stats)
        {
            *stats = st;
        }
        return c ? za::Optional<Ledge>{c->toLedge()} : za::nullOpt;
    };

    // At a hold: the one taken first (takenBefore) of those it is at, that passes the checks.
    za::Vector<const Candidate*>& order = scratch.order;
    order.clear();
    for(const Candidate& c : found)
    {
        if(c.place.dist <= 1e-3f)
        {
            order.pushBack(&c);
        }
    }
    // (Stable: equal places in their order, as std::sort left a few. insertionSort, not stableSort: takenBefore compares
    // within tolerances, not a strict weak order, and the two may disagree on such a one.)
    za::insertionSort(order.begin(), order.end(), [](const Candidate* a, const Candidate* b) { return takenBefore(a->place, b->place); });
    for(const Candidate* c : order)
    {
        if(c->place.top + c->offset.z < (c->prop ? propLow : lowest))
        {
            st.low++;
            continue; // too low for a hold (Lowest Ledge)
        }
        if(check(s, *c, hand, false, hand, headPos, movers) == Check::ok)
        {
            return finish(c);
        }
    }
    if(s.player)
    {
        if(const za::Optional<Ledge> rung = ladderHold(s.player, hand, headPos, lowest)) { return rung; } // AD support: actual solid only
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
    const glm::vec2 toBody = towardsBody(s, hand);
    // A lenient hold is at most the leniency plus this from the hand: the furthest the hold is from a hand at the ledge
    // (in over the top as far as it reaches, or in front of its face, and under the lip as far as touching reaches).
    const Touch k = touch();
    const float lenientReach = za::hypot(za::max(k.in, holdInset + k.front), ledges::holdLift + k.sink) + 1e-3f;
    za::Vector<Scored>& scored = scratch.scored;
    scored.clear();
    za::Vector<FrameBox>& frameBoxes = scratch.frameBoxes;
    frameBoxes.clear();
    frameBoxes.resize(found.size());
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
        if(!freePoint(c, found, frameBoxes, hand, toBody, at) || glm::distance(at, hand) > radius + 1e-3f)
        {
            st.hidden++;
            continue;
        }
        Candidate taken = c;
        taken.place = placeOf(*c.map, c.ledge(), at - c.offset, toBody);
        const Ledge l = taken.toLedge();
        if(l.top < (c.prop ? propLow : lowest))
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
        scored.pushBack(Scored{score, taken, at});
    }
    za::stableSort(scored.begin(), scored.end(), [](const Scored& a, const Scored& b) { return a.score < b.score; });
    for(const Scored& sc : scored)
    {
        switch(check(s, sc.c, sc.at, true, hand, headPos, movers))
        {
        case Check::ok: lenient = true; return finish(&sc.c);
        case Check::covered: st.covered++; break;
        case Check::unreached: st.covered++; break;
        case Check::unseen: st.unseen++; break;
        }
    }
    if(s.player)
    {
        // AD support: a ladder volume within the leniency across (the flat ladder's probes, in the wall behind a thin
        // trigger_ladder brush; a hand just off one).
        if(const za::Optional<Ledge> rung = ladderHold(s.player, hand, headPos, lowest, radius))
        {
            lenient = true;
            return rung;
        }
    }
    return finish(nullptr);
}

// The lowest a prop's ledge may be (see "Holds on props"): from the ground vr_climb_prop_min_height over the feet (lower,
// the grip is the QC's: it carries the prop); hanging or off the ground vr_climb_min_height, as any ledge.
[[nodiscard]] float propLowest(const edict_t* ent, bool hanging)
{
    const float feet = ent->v.origin[2] + ent->v.mins[2];
    const bool grounded = !hanging && (static_cast<int>(ent->v.flags) & FL_ONGROUND);
    return feet + (grounded ? za::max(vr_climb_min_height.value, vr_climb_prop_min_height.value) : vr_climb_min_height.value);
}

// Mid-air: a walking player neither on the ground nor holding on (after a jump, falling; not swimming, not noclipping)
// reaches vr_climb_leniency_air cm further (a ledge grabbed mid-jump: NOTES.md vrclimb_2026-10-01_00-32-39).
[[nodiscard]] bool midAir(const edict_t* ent, bool hanging)
{
    return !hanging && static_cast<int>(ent->v.movetype) == MOVETYPE_WALK && !(static_cast<int>(ent->v.flags) & FL_ONGROUND) &&
           ent->v.waterlevel < 2.f;
}

[[nodiscard]] float leniencyUnits(bool airborne)
{
    const float cm = za::max(0.f, vr_climb_leniency.value) + (airborne ? za::max(0.f, vr_climb_leniency_air.value) : 0.f);
    return cm * 0.01f * units::metresToUnits();
}

// The hold a grip of hand `h` takes (findHold): from the drawn hand, or from the controller's own point where the level
// holds the drawn hand out of it (vr_handpose.cpp: a hand reaching up under a rung stops with its fingers against the
// rung's underside, its point a hand's height below it). A hold either point is at comes first (the drawn hand's
// before the controller's), then the drawn hand's lenient one, then the controller's. `tracked`: found from the
// controller's point.
// (`radius` the leniency's, `propLow` propLowest's: the take's rules, climb::TakeRules.)
[[nodiscard]] za::Optional<Ledge> gripHold(const Seeker& s, const VrMove& move, int h, float radius, float propLow, bool& lenient,
    bool& tracked, LenientStats* st)
{
    constexpr float maxHeldOut = 64.f; // units: a controller's point further from the drawn hand is not believed
    const VrHandMove& m = move.hands[h];
    tracked = false;
    za::Optional<Ledge> drawn = findHold(s, m.pos, m.throwVel, move.headPos, radius, propLow, lenient, st);
    const float heldOut = glm::distance(m.tracked, m.pos);
    if((drawn && !lenient) || heldOut < 0.25f || heldOut > maxHeldOut)
    {
        return drawn;
    }
    bool trackedLenient = false;
    LenientStats st2;
    za::Optional<Ledge> own = findHold(s, m.tracked, m.throwVel, move.headPos, radius, propLow, trackedLenient, &st2);
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
    int mapForgets{-1};        // ledges::forgets then (changed: a runtime brush's map dropped, maybe this one)
    za::U32 mapSerial{0};      // and the map's Map::serial (ledges::alive)
    // The mantle's search (findMantle), kept for the hold: the last search that found no room (not searched again, the
    // body hanging still, until pmove::mantleRetry; the hang's steps' clock: pmove::hangMantle).
    bool missed{false};
    float missAge{0.f};
    glm::vec3 missOrigin{0.f}, missHold{0.f};
    float missTop{0.f};
    // A prop's hold (see "Holds on props"): moved as the prop lies each frame (followProps). `hold`, `ledge` (its top, its
    // way out) and the ledge (propMap) are where they are now.
    bool prop{false};
    bool light{false};          // lighter than vr_climb_prop_mass: the pull goes into it, it doesn't carry the body
    ledges::Map propMap;        // its ledge (and its sample), world space now
    glm::vec3 propOrigin{0.f};  // the prop's body when the hold last followed it
    glm::mat3 propTurn{1.f};
    glm::vec3 takenNormal{0.f}; // its top's normal when taken (vr_climb_prop_tilt)
    glm::vec3 takenHold{0.f};   // where it was taken (vr_climb_prop_slip)
    glm::vec3 holdVel{0.f};     // its velocity last frame (vr_climb_prop_letgo_speed)
    // A rope's hold (see "Holds on ropes"): `entNum` the rope, its place on it (followRopes: `hold` where it is now).
    bool rope{false};
    float ropeS{0.f}, ropeD{0.f};
    glm::vec3 grabVel{0.f};     // the body's velocity as it took hold (the rope's first swing)
};

// The climber's phase (design review 8). Which hands hold is each grip's `active`: hanging and slipping hold with one
// hand or both, free and mantling with neither. Only transition() changes it; the frame functions switch on it.
enum class Phase : za::U8
{
    Free,     // not climbing: the body moves as usual
    Hanging,  // from one hand or both: VR_ClientClimb moves the body
    Slipping, // hanging with no stamina left (vr_climb_stamina_slip): the body sinks, no mantle, the hands slip off
    Mantling, // over the top of a ledge, along the mantle's way
};

// Why the phase changes (transition's entry and exit work).
enum class Why : za::U8
{
    Took,       // a hand took hold (free -> hanging)
    Released,   // the hands let go (-> free, flung by the release)
    OutOfReach, // the body couldn't be kept within reach (-> free)
    Forced,     // moved by something else, disabled, the hold gone, knocked off (-> free)
    NoMove,     // no move from the client (-> free, no regrab delay)
    Exhausted,  // no stamina left (-> free)
    Slips,      // no stamina left, vr_climb_stamina_slip > 0 (hanging -> slipping)
    Recovers,   // stamina back, or the feet on something (slipping -> hanging)
    Mantles,    // pulled over the top with room on it (hanging -> mantling)
    Mantled,    // the mantle's end (-> free)
    CarriedOff, // a mover carried the body into something (-> free)
};

struct Climber
{
    Grip grips[2];
    Phase phase{Phase::Free};
    float mantleAt{0.f}; // the mantle's time run (its frames', or a remote client's moves': MP B8h)
    glm::vec3 mantleFrom{0.f}, mantleMid{0.f}, mantleTo{0.f};
    int mantleEnt{0};           // the brush model mantled onto (0: the world): the way follows it,
    glm::vec3 mantleBase{0.f};  // from where it was at the start
    bool mantleProp{false};     // (a prop: the way follows it as it lies, from its body at the start)
    glm::mat3 mantleTurn{1.f};
    glm::vec3 mantleShift{0.f}; // (how far it has moved since)
    double noGrabUntil{0.0};
    double lastTime{-1.0};
    climb::TakeMemory take; // the grips' presses: the last move's, a press in the air at no hold (MP B8j part 2)
    glm::vec3 lastOrigin{0.f}; // where the climb put the body (moved otherwise: let go)
    glm::vec3 owed{0.f};       // the pull the body couldn't make (made up first by a pull the other way)
    bool noRoom{false};        // pulling over the top of a ledge with no room to mantle (felt once)
    bool mantleJumpWas{false}; // the jump key at the last frame (vr_climb_mantle_trigger: a new press mantles)
    bool ropeJumpWas{false};   // and for a jump off a rope (ropeJump)
    bool ropeJumped{false};    // this frame's let-go is a jump off a rope (fallFlung: vr_rope_jump up)
    // Climbing stamina (see "Climbing stamina" below).
    bool draining{false};      // this frame's hang spent stamina
    double slipStart{0.0};
    bool wasLow{false};        // the pool was low (the gadget's blink) at the last hanging frame: the breath once as it gets low
    double tiredAt{-1.0};      // a grip refused for want of stamina: its buzz (at most every 0.5 s)
    za::String handsLine[2];  // vr_debug_hands: the lines last printed
    // Flatscreen play's mantle (flatMantle): the jump it follows.
    double flatJumpAt{-1.0};  // when the jump left the ground (-1: none since landing, or the jump key let go)
    double gruntAt{-1.0};     // the flat mantle's grunt, put off until the jump's own has ended (-1: none waiting)
    bool flatGrounded{false}; // on the ground at the last frame
    int flatMantles{0};       // mantles it started (vr_flat_mantle_test)
    // And its hands on the ledge while it mantles (flatMantleHands): each empty hand's place on the lip, drawn there
    // as a grip's hold is (calcStats, drawnHand); no grip (the mantle has let go of them all).
    struct FlatHold
    {
        bool on{false};
        glm::vec3 hold{0.f};
        glm::vec3 out{0.f};
        int entNum{0};             // the ledge's brush model (0: the world), and where it was: the hold moves with it
        glm::vec3 entOrigin{0.f};
    };
    FlatHold flatHolds[2];
    // Flatscreen play's ledge ladder (flatLadder): hanging from its rungs, both grips on them, moved by the keys.
    bool flatLadder{false};
    bool flatShow[2]{false, false}; // each hand drawn on its rung (an empty hand; one holding a weapon keeps it)
    bool flatJumpWas{false};         // the jump key at the last ladder frame (a new press pushes off)
    glm::vec3 flatLadderOut{0.f};    // the ladder's way out (towards him)
    double flatLadderMiss{-1.0};     // when the last search for a ladder ahead found none (searched again 0.1 s later)
    double flatLadderTry{-1.0};      // when its top's mantle was last looked for
    int flatLadders{0};              // ladder climbs it started (vr_flatladder_test)
    bool flatMount{false};           // the mantle's way runs backwards: off the ladder's top onto its rungs (flatLadderMount)
    Ledge flatMountRungs[2];         // and the rungs it takes there (the higher, the lower)
    // Flatscreen play's rope (flatRope): both grips on a climbable rope, the body moved by the keys (flatRopeFrame).
    bool flatRope{false};
    int flatRopes{0};                // rope climbs it started (vr_rope_flat_test)
    int flatRopeLeft{0};             // the rope it last let go of, and when (not taken again for flatRopeRetake)
    double flatRopeLeftAt{-1.0};
    // (ropes6) A headset's pulls up a rope (ropePullGrunt): each hand's rope grip last seen (its serial), the newest
    // grip's hand and the body's height over its hold as it took it, whether its pull has grunted, when the next grunt
    // may come, and the grunts so far (the tests').
    int pullSeen[2]{-1, -1};
    int pullHand{-1};
    float pullFrom{0.f};
    bool pullGrunted{true};
    double pullGruntNext{0.0};
    int pullGrunts{0};
    // Hanging or slipping: holding on with one hand or both.
    bool hanging() const
    {
        return phase == Phase::Hanging || phase == Phase::Slipping;
    }
    // Any hand holds (the grips: within a frame they change before the phase follows them).
    bool anyHeld() const
    {
        return grips[0].active || grips[1].active;
    }
};

void transition(edict_t* ent, Climber& c, Phase to, Why why, const VrMove* move = nullptr, const bool* held = nullptr);

// A fixed array (as the swimmers'): a climber is held by reference across SV_RunThink, which a growing vector could
// move.
Climber climbers[MAX_SCOREBOARD];

[[nodiscard]] Climber* climberOf(edict_t* ent)
{
    const int client = NUM_FOR_EDICT(ent) - 1;
    if(client < 0 || client >= za::min(svs.maxclients, static_cast<int>(MAX_SCOREBOARD)))
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
    const edict_t* held = entityField(ent, progs::handFields(h == 1).held);
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
    if(hotspot == HS_AMMO_POUCH)
    {
        return !hanging; // a round to take or put back (hanging: the next hold)
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

// Grip `g`'s brush model while it can be held (there, solid, the same model), else nullptr. (A prop's: there; followProps
// lets go of one gone.)
[[nodiscard]] edict_t* holdEntity(const Grip& g)
{
    if(g.entNum <= 0 || g.entNum >= qcvm->num_edicts)
    {
        return nullptr;
    }
    edict_t* e = EDICT_NUM(g.entNum);
    if(g.prop)
    {
        return e->free ? nullptr : e;
    }
    if(g.rope)
    {
        return !e->free && climbrope::isRopeClass(PR_GetString(e->v.classname)) ? e : nullptr;
    }
    return !e->free && static_cast<int>(e->v.solid) == SOLID_BSP && static_cast<int>(e->v.modelindex) == g.entModel ? e : nullptr;
}

// Where grip `g`'s hold is now: it moves with its brush model (a prop's has followed it: followProps).
[[nodiscard]] glm::vec3 holdNow(const Grip& g)
{
    if(g.prop || g.rope || g.entNum <= 0 || g.entNum >= qcvm->num_edicts)
    {
        return g.hold;
    }
    edict_t* e = EDICT_NUM(g.entNum);
    return g.hold + (vec(e->v.origin) - g.entOrigin);
}

// Grip `g`'s ledge's map (a prop's: its own), nullptr if it is gone (a new map).
[[nodiscard]] const ledges::Map* gripMap(const Grip& g)
{
    if(g.prop)
    {
        return g.propMap.edges.empty() ? nullptr : &g.propMap;
    }
    if(!g.ledge.map || g.ledge.edge < 0 || g.mapGeneration != ledges::generation())
    {
        return nullptr;
    }
    // (a runtime brush changed since: only a hold on its map lost it; the world's and the others' are kept)
    return g.mapForgets == ledges::forgets() || ledges::alive(g.ledge.map, g.mapSerial) ? g.ledge.map : nullptr;
}

// The top of grip `g`'s ledge now.
[[nodiscard]] float topNow(const Grip& g)
{
    return g.ledge.top + (holdNow(g).z - g.hold.z);
}

void letGoAll(Climber& c)
{
    c.grips[0].active = c.grips[1].active = false;
    c.owed = glm::vec3{0.f};
    c.noRoom = false;
}

// The arm's reach (see the top of the file), units: the shoulder joint to the palm's middle.
[[nodiscard]] float armReach()
{
    const float body = units::bodyScale();
    return (bodycal::armLengthMetres() + (palmReach + reachSlack) * body + za::max(0.f, vr_body_shoulder_reach.value) * body) *
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
    const glm::vec3 fwd{za::cos(yaw), za::sin(yaw), 0.f}, left{-za::sin(yaw), za::cos(yaw), 0.f};
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
    const ledges::Map* m = gripMap(g);
    if(!m)
    {
        return -1.f;
    }
    const ledges::Edge& e = m->edges[static_cast<za::SizeT>(g.ledge.edge)];
    const glm::vec3 along{-g.ledge.out.y, g.ledge.out.x, 0.f};
    const float t = g.ledge.t + side * glm::dot(along, e.dir);
    if(t < -1e-3f || t > e.len + 1e-3f)
    {
        return -1.f;
    }
    return m->sampleAt(e, za::clamp(t, 0.f, e.len)).depth;
}

// The hands in sync (vr_climb_slide; see the top of the file): grip `g`'s hold slid along its lip towards where the
// tracked hand `tracked` has it (the hand as far from the hold as when it took hold), as far as the lip goes and no
// further from `shoulder` than `reach` (or than it is already).
void slideHold(Grip& g, const glm::vec3& tracked, const glm::vec3& shoulder, float reach)
{
    const ledges::Map* m = gripMap(g);
    if(!m)
    {
        return;
    }
    const ledges::Edge& e = m->edges[static_cast<za::SizeT>(g.ledge.edge)];
    const glm::vec3 hold = holdNow(g);
    float s = glm::dot(tracked - g.grabOffset - hold, e.dir);
    s = za::clamp(g.ledge.t + s, 0.f, e.len) - g.ledge.t;
    // |hold + e.dir * s - shoulder| <= max(reach, now): s between the roots of s^2 + 2 b s + c = 0 (c <= 0).
    const glm::vec3 v = hold - shoulder;
    const float b = glm::dot(v, e.dir);
    const float c = glm::dot(v, v) - za::max(reach * reach, glm::dot(v, v));
    const float root = za::sqrt(za::max(0.f, b * b - c));
    s = za::clamp(s, -b - root, -b + root);
    if(za::abs(s) < 1e-4f)
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
// (vr_pmove.cpp's findMantle, on the hang's state: the steps' test and the flat mantle's search run the same; the
// ledge map's depths at pmove::mantleSides come in the hang's state, hangGripOf.)

// The player's edict as the hang's steps see it (pmove::hangStep, pmove::moveBody: single player's frame and a remote
// client's moves run the same code, MP B8i): his box against the world and entities (SV_Move's MOVE_NORMAL).
struct ClimbBody final : pmove::Body
{
    edict_t* ent;

    explicit ClimbBody(edict_t* e) : ent{e}
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
    int contents(const float* p) override { return SV_PointContents(const_cast<float*>(p)); }
    trace_t trace(const float* start, const float* end) override
    {
        return tracePlayer(ent, glm::vec3{start[0], start[1], start[2]}, glm::vec3{end[0], end[1], end[2]});
    }
    bool metBsp(const trace_t& tr) override { return tr.ent && static_cast<int>(tr.ent->v.solid) == SOLID_BSP; }
    trace_t line(const float* start, const float* end) override
    {
        return traceLine(glm::vec3{start[0], start[1], start[2]}, glm::vec3{end[0], end[1], end[2]}, ent);
    }
    trace_t traceWorld(const float* start, const float* end) override
    {
        return traceBox(glm::vec3{start[0], start[1], start[2]}, vec(ent->v.mins), vec(ent->v.maxs),
            glm::vec3{end[0], end[1], end[2]}, MOVE_NOMONSTERS, ent);
    }
    bool overDropoff(const float*, float) override { return false; }
};

// Grip `g` as the hang's steps see it (pmove::HangGrip): the hold now, the hand's last place, the ledge's way out and
// its depths along it, the pull down's start, the mantle's last search that found no room.
void hangGripOf(const Grip& g, pmove::HangGrip& hg)
{
    hg.active = g.active;
    hg.light = g.light;
    setVec(hg.hold, holdNow(g));
    setVec(hg.lastRel, g.lastRel);
    hg.top = topNow(g);
    hg.out[0] = g.ledge.out.x;
    hg.out[1] = g.ledge.out.y;
    hg.relAtGrabZ = g.rope ? -1e6f : g.relAtGrab.z; // (a rope's hold: no mantle from it, it has no top)
    for(int s = 0; s < 5; s++)
    {
        hg.depth[s] = depthAt(g, pmove::mantleSides[s]);
    }
    hg.missed = g.missed;
    hg.missAge = g.missAge;
    setVec(hg.missOrigin, g.missOrigin);
    setVec(hg.missHold, g.missHold);
    hg.missTop = g.missTop;
}

// What starts a mantle once grip and head are high enough (vr_climb_mantle_trigger): 0 that alone (pmove::hangMantle,
// each step of the hang); otherwise these bits (triggeredMantle, before the grips let go).
constexpr int mantleOnLetGo = 1; // the last holding hand lets go of its grip
constexpr int mantleOnJump = 2;  // a jump press

[[nodiscard]] int mantleTrigger()
{
    return za::clamp(static_cast<int>(vr_climb_mantle_trigger.value), 0, mantleOnLetGo | mantleOnJump);
}

// The body's box and the mantle's rules, for the hang's steps.
void hangBox(const edict_t* ent, pmove::Hang& h)
{
    h.minsZ = ent->v.mins[2];
    h.maxsXY[0] = ent->v.maxs[0];
    h.maxsXY[1] = ent->v.maxs[1];
    // (vr_climb_mantle_trigger: the mantle waits for the release or the jump, triggeredMantle; no test in the steps)
    h.mantleTime = mantleTrigger() == 0 ? mantleTime : 0.f;
    h.mantleHeight = za::clamp(vr_climb_mantle_height.value, 0.f, 40.f);
    h.mantleSlope = za::clamp(vr_climb_mantle_slope.value, 0.f, 60.f);
    h.verbose = debug();
}

// The mantle's way from grip `g` (pmove::findMantle: the steps' own search), the body where the edict is.
[[nodiscard]] bool findMantle(edict_t* ent, const Grip& g, glm::vec3& mid, glm::vec3& to)
{
    pmove::Hang h;
    hangGripOf(g, h.grips[0]);
    hangBox(ent, h);
    ClimbBody b{ent};
    vec3_t m, t;
    if(!pmove::findMantle(b, h, 0, m, t))
    {
        return false;
    }
    mid = vec(m);
    to = vec(t);
    return true;
}

// Moves the body towards `target` through the world: the whole way, else the nearest to it of: sliding along what
// blocks it (the face under a ledge: a shimmy pressed against it, pulled partly into it, still moves along it), up/down
// then across, across then up/down (round a ledge's lip). (pmove::moveBody: the hang's steps' own.)
void moveBody(edict_t* ent, const glm::vec3& target)
{
    ClimbBody b{ent};
    const vec3_t to{target.x, target.y, target.z};
    pmove::moveBody(b, to);
    VectorCopy(b.origin, ent->v.origin);
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

// (An exhausted body sinks, slipping, pmove::hangSlipSink units a second: to straight arms, the reach stops it.)

[[nodiscard]] bool staminaOn(edict_t* ent)
{
    const FieldOffsets& f = fields();
    return ent && vr_climb_stamina.value != 0.f && bindings().isVrProgs && f.vr_stamina_used >= 0 && f.vr_stamina_time >= 0;
}

// (The pool's size and what is left of it, 0 .. its size: the QC's, progs::staminaMax and staminaLeft.)

// Too tired to start a hang: less left than a second's hang from one hand (it would let go at once).
[[nodiscard]] bool tooTired(edict_t* ent)
{
    return staminaOn(ent) && progs::staminaLeft(ent) < za::max(0.001f, vr_climb_stamina_rate.value);
}

// Tiring Warning (vr_parry_stamina_warn): the breath's volume and the buzz's strength, as the parry's.
[[nodiscard]] float staminaWarn()
{
    return za::clamp(vr_parry_stamina_warn.value, 0.f, 1.f);
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
    used = za::min(progs::staminaMax(), za::max(0.f, used) + za::max(0.f, amount));
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
        SV_StartSound(ent, 0, sample, static_cast<int>(za::lround(255.f * za::min(volume, 1.f))), 1.f); // (ATTN_NORM)
    }
}

// A small grunt as a mantle starts (NOTES.md vrclimb_2026-10-01_16-27-48), at vr_climb_mantle_grunt's volume: which one
// is vr_climb_mantle_grunt_sound's (NOTES.md vrfiringrange_2026-10-01_22-43-20: not the jump's by default, so the two
// are told apart). Id's recordings (world.qc precaches them) or two made from them as they load, played slower (deeper;
// vr_audio.cpp derivedSounds). Precached with each map (precache); one not precached stays quiet. The judo throw's grunt
// (vr_foegrab_throw_grunt_sound) is one of the same list (climb::grunt).
constexpr const char* mantleGrunts[] = { // (the menus' gruntChoices: the same order)
    "player/plyrjmp8.wav",         // 0 the jump's grunt
    "player/land2.wav",            // 1 a hard landing's "oof" (the default)
    "vr/derived/plyrjmp8_low.wav", // 2 the jump's, deeper and longer
    "vr/derived/land2_low.wav",    // 3 a hard landing's, deeper
    "player/pain2.wav",            // 4 a short low pain grunt
};

void mantleGrunt(edict_t* ent)
{
    if(climb::grunt(ent, vr_climb_mantle_grunt_sound.value, vr_climb_mantle_grunt.value) && debug())
    {
        Con_Printf("climb: mantle grunt %s %.2f\n", climb::gruntSample(vr_climb_mantle_grunt_sound.value),
            za::clamp(vr_climb_mantle_grunt.value, 0.f, 1.f));
    }
}

// vr_climb_mantle_grunt_test: the mantle's grunt as set, heard here (the menu's Hear Mantle Grunt).
void gruntTest_f()
{
    S_LocalSound(climb::gruntSample(vr_climb_mantle_grunt_sound.value));
}

// The body stands on something (the box 2 units down meets a floor, a step, a brush model's top).
[[nodiscard]] bool feetSupported(edict_t* ent)
{
    vec3_t end{ent->v.origin[0], ent->v.origin[1], ent->v.origin[2] - 2.f};
    const trace_t tr = SV_Move(ent->v.origin, ent->v.mins, ent->v.maxs, end, MOVE_NOMONSTERS, ent);
    return !tr.startsolid && tr.fraction < 1.f && tr.plane.normal[2] >= 0.7f;
}

// Out of stamina: every hand lets go (no fling: they give way), a gasp and a long buzz.
// Each hanging frame, after the pull and the mantle's check: the hang's stamina (see above).
void hangStamina(edict_t* ent, Climber& c, double time)
{
    c.draining = false;
    if(!staminaOn(ent) || !c.hanging())
    {
        if(c.phase == Phase::Slipping)
        {
            transition(ent, c, Phase::Hanging, Why::Recovers);
        }
        return;
    }
    const int hands = (c.grips[0].active ? 1 : 0) + (c.grips[1].active ? 1 : 0);
    const bool standing = feetSupported(ent);
    const float before = progs::staminaLeft(ent);
    if(!standing)
    {
        const float rate = za::max(0.f, hands > 1 ? vr_climb_stamina_rate_2h.value : vr_climb_stamina_rate.value);
        spendStamina(ent, rate * static_cast<float>(host_frametime));
        c.draining = rate > 0.f;
        const bool low = staminaLow(ent);
        if(low && !c.wasLow)
        {
            staminaSound(ent, "player/gasp2.wav", 0.8f * staminaWarn()); // tiring (the QC's breath: VR_Stamina_Effort)
            if(debug())
            {
                Con_Printf("climb: stamina low (%.1f left): the tiring breath\n", progs::staminaLeft(ent));
            }
        }
        c.wasLow = low;
        if(vr_climb_debug.value >= 2.f || (debug() && za::floor(before / 10.f) != za::floor(progs::staminaLeft(ent) / 10.f)))
        {
            Con_Printf("climbstamina %.4f %d hand%s: %.2f of %.0f left (-%.3f)%s\n", time, hands, hands > 1 ? "s" : "",
                progs::staminaLeft(ent), progs::staminaMax(), before - progs::staminaLeft(ent), low ? " low" : "");
        }
    }
    if(standing || progs::staminaLeft(ent) > 0.f)
    {
        if(c.phase == Phase::Slipping)
        {
            transition(ent, c, Phase::Hanging, Why::Recovers);
        }
        return;
    }
    const float slip = za::max(0.f, vr_climb_stamina_slip.value);
    if(slip <= 0.f)
    {
        transition(ent, c, Phase::Free, Why::Exhausted);
        return;
    }
    if(c.phase != Phase::Slipping)
    {
        transition(ent, c, Phase::Slipping, Why::Slips);
    }
    else if(time - c.slipStart >= slip)
    {
        transition(ent, c, Phase::Free, Why::Exhausted);
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
    const auto entName = [](edict_t* e) -> za::String {
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
        edict_t* held = entityField(ent, progs::handFields(h == 1).held);
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
                            : c.phase == Phase::Mantling           ? "mantling"
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
            h == 1 ? ent->v.weapon : fieldFloatOr(ent, f.weapon2, 0.f), entName(held).cStr(),
            held && held == otherHeld ? " (both hands)" : "", entName(target).cStr(),
            fieldFloatOr(ent, h == 1 ? f.mainhand_fglocked : f.offhand_fglocked, 0.f) != 0.f ? " locked" : "",
            entName(pulled).cStr(), pulledState, move && (move->buttons & busyButton[h]) ? 1 : 0, hotspot, climb);
        if(vr_debug_hands.value >= 2.f || c.handsLine[h] != line)
        {
            c.handsLine[h] = line;
            Con_Printf("hands %.3f %s: %s\n", time, handName(h), line);
        }
    }
}

// Holds on props (see "Holds on props" at the top of the file), each frame before the grips: each hand's hold moved as
// its prop lies now (its place on the prop kept), or let go: the prop gone (broken, taken into a hand, no longer solid),
// tipped further than vr_climb_prop_tilt from how it lay when taken, knocked away (the hold's speed changed by
// vr_climb_prop_letgo_speed in a frame), or a light one's hold moved vr_climb_prop_slip from where it was taken.
void followProps(edict_t* ent, Climber& c)
{
    for(int h = 0; h < 2; h++)
    {
        Grip& g = c.grips[h];
        if(!g.active || !g.prop)
        {
            continue;
        }
        box3d::ClimbBody body;
        const char* why = nullptr;
        if(vr_climb_props.value == 0.f || g.entNum >= qcvm->num_edicts || g.propMap.edges.empty() ||
            !box3d::climbBody(g.entNum, body) || static_cast<int>(EDICT_NUM(g.entNum)->v.modelindex) != g.entModel)
        {
            why = "the prop is gone";
        }
        else
        {
            const glm::mat3 turn = body.turn * glm::transpose(g.propTurn);
            const auto moved = [&](const glm::vec3& p) { return body.origin + turn * (p - g.propOrigin); };
            ledges::Edge& e = g.propMap.edges[0];
            const glm::vec3 hold = moved(g.hold);
            const glm::vec3 normal = glm::normalize(turn * e.normal);
            const glm::vec3 vel = body.pointVelocity(hold);
            const float tilt = glm::degrees(za::acos(za::clamp(glm::dot(normal, g.takenNormal), -1.f, 1.f)));
            if(tilt > vr_climb_prop_tilt.value)
            {
                why = va("tipped %.0f degrees", tilt);
            }
            else if(glm::length(vel - g.holdVel) > vr_climb_prop_letgo_speed.value)
            {
                why = va("knocked away (%.0f units/s)", glm::length(vel - g.holdVel));
            }
            else if(g.light && glm::distance(hold, g.takenHold) > vr_climb_prop_slip.value)
            {
                why = va("pulled away (%.1f units)", glm::distance(hold, g.takenHold));
            }
            // The ledge as the prop lies now: its way out kept level and square to the lip.
            const glm::vec3 oldOut = turn * e.out;
            e.a = moved(e.a);
            e.dir = glm::normalize(turn * e.dir);
            e.normal = normal;
            if(glm::vec3 o{e.dir.y, -e.dir.x, 0.f}; glm::length(o) > 1e-3f)
            {
                o = glm::normalize(o);
                e.out = glm::dot(o, oldOut) < 0.f ? -o : o;
            }
            const glm::vec3 b = e.point(e.len);
            e.mins = glm::min(e.a, b);
            e.maxs = glm::max(e.a, b);
            g.ledge.top += hold.z - g.hold.z;
            g.hold = hold;
            g.ledge.hold = moved(g.ledge.hold);
            g.ledge.out = e.out;
            g.propOrigin = body.origin;
            g.propTurn = body.turn;
            g.holdVel = vel;
        }
        if(why)
        {
            g.active = false;
            server::sendHaptic(ent, h, 0.f, 0.08f, 60.f, 0.5f);
            if(debug())
            {
                Con_Printf("climb: %s hand lets go of prop %d: %s, at (%.1f %.1f %.1f)\n", handName(h), g.entNum, why,
                    ent->v.origin[0], ent->v.origin[1], ent->v.origin[2]);
            }
        }
    }
}

// The rope holds each frame (see "Holds on ropes"): where each is now on its rope (its rider's line, or its points), the
// way out from it towards the body; a rope gone lets go.
void followRopes(edict_t* ent, Climber& c)
{
    const int client = NUM_FOR_EDICT(ent) - 1;
    for(int h = 0; h < 2; h++)
    {
        Grip& g = c.grips[h];
        if(!g.active || !g.rope)
        {
            continue;
        }
        glm::vec3 at;
        if(!holdEntity(g) || !climbrope::holdAt(g.entNum, client, g.ropeS, g.ropeD, at))
        {
            g.active = false;
            if(debug())
            {
                Con_Printf("climb: %s hand lets go of rope %d: it is gone\n", handName(h), g.entNum);
            }
            continue;
        }
        g.hold = at;
        g.ledge.hold = at;
        g.ledge.top = at.z;
        if(const glm::vec3 o{ent->v.origin[0] - at.x, ent->v.origin[1] - at.y, 0.f}; glm::length(o) > 1.f)
        {
            g.ledge.out = glm::normalize(o);
        }
    }
}

struct FlatRopeKeys
{
    float climb{0.f};    // -1..1: up +, a share of vr_flat_ladder_speed
    float fwdPump{0.f};  // 0..1: how much of forward / back pumps the swing
    float sidePump{0.f}; // 0..1: how much of the strafe keys pumps it
};
[[nodiscard]] FlatRopeKeys flatRopeKeys(edict_t* ent, const VrMove* move, int rope); // (below: flat play's rope keys)

// Told to the ropes each frame (climbrope::ride): the rope this climber's hands hold, his highest and lowest hand on it
// (a rope's rider: his weight on it, its swing his). A hand on a ledge too: the ledge holds him, no ride.
void reportRopes(edict_t* ent, const Climber& c)
{
    const Grip* hi = nullptr;
    const Grip* lo = nullptr;
    bool other = false;
    for(const Grip& g : c.grips)
    {
        if(!g.active)
        {
            continue;
        }
        if(!g.rope || (hi && g.entNum != hi->entNum))
        {
            other = true;
            continue;
        }
        hi = !hi || g.ropeS < hi->ropeS ? &g : hi;
        lo = !lo || g.ropeS > lo->ropeS ? &g : lo;
    }
    const int client = NUM_FOR_EDICT(ent) - 1;
    if(hi && !other && c.hanging() &&
        climbrope::ride(client, hi->entNum, hi->ropeS, hi->ropeD, lo->ropeS, lo->ropeD,
            glm::length(hi->grabVel) >= glm::length(lo->grabVel) ? hi->grabVel : lo->grabVel))
    {
        // Pumping the swing (P3; the stick or WASD, as the walk's: its way from the view's yaw): vr_rope_pump units/s/s
        // its way, full at the walk's full speed (pumpFullMove), up to vr_rope_swing_cap (climbrope's layRider).
        const usercmd_t& cmd = svs.clients[client].cmd;
        const float yaw = glm::radians(ent->v.v_angle[1]);
        const glm::vec3 fwd{za::cos(yaw), za::sin(yaw), 0.f}, right{za::sin(yaw), -za::cos(yaw), 0.f};
        glm::vec3 push = fwd * cmd.forwardmove + right * cmd.sidemove;
        if(c.flatRope && climbrope::monkey(hi->entNum))
        {
            push = glm::vec3{0.f}; // (flat play: the keys go along it: flatMonkeyFrame)
        }
        else if(c.flatRope)
        {
            // (Flat play: by the view's pitch the forward key climbs it or pumps, the strafe keys pump: flatRopeKeys.)
            const FlatRopeKeys k = flatRopeKeys(ent, server::clientMove(ent), hi->entNum);
            push = fwd * (cmd.forwardmove * k.fwdPump) + right * (cmd.sidemove * k.sidePump);
        }
        const float len = glm::length(push);
        push = len > 1.f ? push / len * (za::min(len / pumpFullMove, 1.f) * za::max(vr_rope_pump.value, 0.f)) : glm::vec3{0.f};
        climbrope::setPump(client, push);
    }
}

// Jumping off a rope (a jump press while a hand holds one, vr_rope_jump): every hand lets go -- the fall keeps the rope's
// swing at the hands and the hands' fling (fallFlung) -- and the jump adds vr_rope_jump up.
[[nodiscard]] bool ropeJump(edict_t* ent, Climber& c)
{
    const bool jump = ent->v.button2 != 0.f;
    const bool press = jump && !c.ropeJumpWas;
    c.ropeJumpWas = jump;
    const bool onRope = (c.grips[0].active && c.grips[0].rope) || (c.grips[1].active && c.grips[1].rope);
    if(!press || !onRope || !c.hanging() || c.flatLadder)
    {
        return false;
    }
    c.grips[0].active = c.grips[1].active = false;
    if(debug())
    {
        Con_Printf("climb: jumps off the rope\n");
    }
    return true;
}

// The holding hands' pull on light props (see "Holds on props"), each frame they hang: the body's weight
// (vr_box3d_player_mass, kg) shared by the holding hands, along the arm (from the hold towards the body; a body over the
// hold presses it down), at most vr_climb_prop_pull kg a hand, through this frame's step (Box3D's gravity: what the
// prop's own weight is too).
void pullProps(edict_t* ent, const Climber& c)
{
    const int hands = (c.grips[0].active ? 1 : 0) + (c.grips[1].active ? 1 : 0);
    if(hands == 0)
    {
        return;
    }
    const float weight = za::max(0.f, vr_box3d_player_mass.value) / static_cast<float>(hands); // kg
    const float force = za::min(weight, za::max(0.f, vr_climb_prop_pull.value));
    if(!(force > 0.f))
    {
        return;
    }
    const glm::vec3 body = vec(ent->v.origin);
    for(const Grip& g : c.grips)
    {
        if(!g.active || !g.prop || !g.light)
        {
            continue;
        }
        const glm::vec3 hold = holdNow(g);
        const glm::vec3 d = body - hold;
        const glm::vec3 dir = d.z >= 0.f || glm::length(d) < 1e-3f ? glm::vec3{0.f, 0.f, -1.f} : glm::normalize(d);
        box3d::climbPull(g.entNum, hold, dir, force);
        if(vr_climb_debug.value >= 2.f)
        {
            Con_Printf("climbpull %.4f prop %d at %.1f %.1f %.1f: %.0f kg (%.2f %.2f %.2f)\n", qcvm->time, g.entNum, hold.x, hold.y,
                hold.z, force, dir.x, dir.y, dir.z);
        }
    }
}

// climbPreThink's phases.
// The QC's knock-off, read and cleared.
[[nodiscard]] bool takeKnockOff(edict_t* ent)
{
    // Knocked off the holds by a blast or a shove since the last frame (the QC's .vr_climb_knockoff: combat.qc
    // VR_Climb_KnockOff), read and cleared every frame, held or not.
    bool knockedOff = false;
    if(fields().vr_climb_knockoff >= 0)
    {
        float& knock = fieldFloat(ent, fields().vr_climb_knockoff);
        knockedOff = knock != 0.f;
        knock = 0.f;
    }
    return knockedOff;
}

// Let go of everything when moved by something else, disabled, the hold gone or knocked off.
void letGoIfForced(edict_t* ent, Climber& c, bool enabled, bool knockedOff)
{
    // Moved by something else (a teleporter, a respawn, setorigin), disabled, dead, noclipping, the
    // hold's brush model gone (freed, not solid, another model): let go, and fall as whatever moved the body has it.
    bool forced = !enabled;
    if(c.phase != Phase::Free && !(glm::distance(vec(ent->v.origin), c.lastOrigin) <= 2.f)) // (a NaN: let go too)
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
    // Knocked off (a blast of vr_climb_blast_letgo or more, an enemy's shove): let go, no fling, the velocity the blast or
    // the shove gives the body kept (letting go here, before the frame's grips, is no release: see below).
    if(knockedOff && c.phase != Phase::Free)
    {
        if(debug())
        {
            Con_Printf("climb: knocked off at (%.1f %.1f %.1f), velocity (%.0f %.0f %.0f)\n", ent->v.origin[0],
                ent->v.origin[1], ent->v.origin[2], ent->v.velocity[0], ent->v.velocity[1], ent->v.velocity[2]);
        }
        forced = true;
    }
    if(forced && c.phase != Phase::Free)
    {
        transition(ent, c, Phase::Free, Why::Forced);
    }
}

// A hand takes hold of the hold found (gripHand): the grip's state, a prop's ledge as it lies, the buzz and the slap.
// A grip's hold on the ledge found (takeHold; the flatscreen mantle's ledge, flatMantle): its place, its brush model or
// prop, its map. Returns the prop's mass (0: not a prop).
float holdLedge(Grip& g, const Ledge& found)
{
    const Ledge* const ledge = &found;
    g.missed = false;
    g.hold = g.lastHold = ledge->hold;
    g.ledge = *ledge;
    g.entNum = ledge->ent ? NUM_FOR_EDICT(ledge->ent) : 0;
    g.entOrigin = ledge->ent ? vec(ledge->ent->v.origin) : glm::vec3{0.f};
    g.entModel = ledge->ent ? static_cast<int>(ledge->ent->v.modelindex) : 0;
    g.ledge.ent = nullptr; // (kept as its number: entNum)
    g.mapGeneration = ledges::generation();
    g.mapForgets = ledges::forgets();
    g.mapSerial = ledge->map ? ledge->map->serial : 0u;
    g.prop = ledge->prop;
    g.light = false; // (set below for a prop's; a stale one would stop a later mover carrying the body)
    g.propMap.edges.clear();
    g.propMap.samples.clear();
    g.rope = ledge->rope;
    g.ropeS = ledge->ropeS;
    g.ropeD = ledge->ropeD;
    if(g.rope)
    {
        g.entModel = 0;
        g.mapSerial = 0u;
    }
    float propMass = 0.f;
    if(g.prop)
    {
        // A prop's: its ledge kept as it lies, followed from its body now (followProps).
        box3d::ClimbBody body;
        (void)box3d::climbBody(g.entNum, body);
        ledges::Edge e = ledge->propEdge;
        e.firstSample = 0;
        g.propMap.edges.pushBack(e);
        g.propMap.samples.pushBack(ledge->propSample);
        g.ledge.map = nullptr;
        g.ledge.edge = 0;
        g.propOrigin = body.origin;
        g.propTurn = body.turn;
        g.light = !body.fixed && body.mass < vr_climb_prop_mass.value;
        g.takenNormal = e.normal;
        g.takenHold = ledge->hold;
        g.holdVel = body.pointVelocity(ledge->hold);
        propMass = body.mass;
    }
    return propMass;
}

// Grip `g` taking hold of `ledge` with hand `h` of `move`, the body at `origin` (the hand's place relative to it, the
// ledge's): the server's grips and a remote client's replay (MP B8j part 2) the same -> the prop's mass (holdLedge).
float gripTaken(Grip& g, const Ledge& ledge, const VrMove& move, int h, const glm::vec3& origin)
{
    g.active = true;
    g.relAtGrab = g.lastRel = move.hands[h].pos - origin;
    g.grabOffset = move.hands[h].pos - ledge.hold;
    return holdLedge(g, ledge);
}

// Holds on ropes (vr_rope_climb; vr_climbrope.cpp, docs/vr-port/ROPES.md "Climbing"): a grip with the hand (the drawn
// hand's point, else the controller's) within vr_climb_touch and a palm's breadth (4 cm) of a rope's surface, plus the
// grab leniency, takes it there, at least `lowest` high (vr_climb_min_height over the feet). Its way out: from the rope
// towards the body, level.
[[nodiscard]] za::Optional<Ledge> ropeHold(const Seeker& s, const VrMove& move, int h, float radius, float lowest)
{
    if(vr_rope_climb.value == 0.f)
    {
        return za::nullOpt;
    }
    const float reach = (za::max(0.f, vr_climb_touch.value) + 4.f) * 0.01f * units::metresToUnits() + radius;
    climbrope::Grab grab;
    if(!climbrope::findGrab(move.hands[h].pos, reach, grab) && !climbrope::findGrab(move.hands[h].tracked, reach, grab))
    {
        return za::nullOpt;
    }
    if(grab.at.z < lowest)
    {
        return za::nullOpt;
    }
    Ledge l;
    l.rope = true;
    l.ent = EDICT_NUM(grab.ent);
    l.ropeS = grab.s;
    l.ropeD = grab.d;
    l.hold = grab.at;
    l.top = grab.at.z;
    const glm::vec3 toBody{s.origin.x - grab.at.x, s.origin.y - grab.at.y, 0.f};
    const glm::vec3 fromHand{move.hands[h].pos.x - grab.at.x, move.hands[h].pos.y - grab.at.y, 0.f};
    l.out = glm::length(toBody) > 1e-3f ? glm::normalize(toBody)
            : glm::length(fromHand) > 1e-3f ? glm::normalize(fromHand)
                                            : glm::vec3{1.f, 0.f, 0.f};
    return za::Optional<Ledge>{l};
}

// (`retryAge`: a press in the air at no hold this long before; negative: none.)
void takeHold(edict_t* ent, Climber& c, const VrMove* move, int h, const za::Optional<Ledge>& ledge, bool hanging,
    const char* how, float retryAge)
{
    Grip& g = c.grips[h];
    const glm::vec3 hand = move->hands[h].pos;
    g.owned = true;
    g.serial++;
    const float propMass = gripTaken(g, *ledge, *move, h, vec(ent->v.origin));
    g.allowed = za::max(armReach(), glm::distance(ledge->hold, shoulderOf(*move, h)));
    if(!hanging)
    {
        c.owed = glm::vec3{0.f};
        c.noRoom = false;
    }
    c.lastOrigin = vec(ent->v.origin);
    g.grabVel = vec(ent->v.velocity); // (a rope's: the swing he brings to it)
    // (ropes7) A climbing pole's grip: a metal one's short hard ring, a wooden one's duller knock; any other hold's as before.
    const physsound::RopeMaterial pole = ledge->rope && ledge->ent ? climbrope::material(NUM_FOR_EDICT(ledge->ent)) : physsound::RopeMaterial::Rope;
    if(pole == physsound::RopeMaterial::MetalPole)
    {
        server::sendHaptic(ent, h, 0.f, 0.04f, 160.f, 0.7f);
    }
    else if(pole == physsound::RopeMaterial::WoodPole)
    {
        server::sendHaptic(ent, h, 0.f, 0.07f, 60.f, 0.65f);
    }
    else
    {
        server::sendHaptic(ent, h, 0.f, 0.06f, 80.f, 0.6f);
    }
    if(!ledge->rope)
    {
        physsound::grab(ent, ledge->hold, ledge->ent); // a slap and a tap of the hold's material
    }
    else if(ledge->ent)
    {
        climbrope::grabbed(ent, NUM_FOR_EDICT(ledge->ent), ledge->hold); // (a chain's metal grip, a rope's soft thud)
    }
    if(debug())
    {
        // (", light" on a hold not on a prop: a light prop's flag left on the hand, which would stop a model the
        // QC moves carrying the body; prop_climb_test.sh's lift case. Early in the line: the log wraps it.)
        Con_Printf("climb: %s hand holds at %.1f%s (out %.2f %.2f; hand %.1f %.1f %.1f, hold %.1f %.1f %.1f) from (%.1f %.1f %.1f)%s%s%s%s\n",
            handName(h), ledge->top, !g.prop && g.light ? ", light" : "", ledge->out.x, ledge->out.y, hand.x, hand.y, hand.z, ledge->hold.x, ledge->hold.y,
            ledge->hold.z, ent->v.origin[0], ent->v.origin[1], ent->v.origin[2], hanging ? ", both hands" : "", how,
            retryAge >= 0.f ? va(", pressed %.0f ms before, in the air", static_cast<double>(retryAge) * 1000.0) : "",
            g.prop ? va(", prop %d (%s, %.0f kg)", g.entNum, g.light ? "light" : "heavy", propMass) : "");
    }
    if(vr_climb_debug.value >= 2.f)
    {
        Con_Printf("climbreach: %s hand's hold %.1f units from its shoulder (reach %.1f: the arm %.3f m)\n", handName(h),
            glm::distance(ledge->hold, shoulderOf(*move, h)), armReach(), bodycal::armLengthMetres());
    }
}

// A hold's take (MP B8j part 2): one hand's pressed grip looking for a hold, for one move (or single player's frame) --
// what came of it.
enum class Take : za::U8
{
    none,    // not looking (not pressed, holding, mantling, climbing off ...)
    refused, // too soon, at a holster, the hand not empty
    tired,   // too little stamina for a new hang
    missed,  // no hold where the hand is
    found,   // `ledge`
};
struct TakeTry
{
    Take what{Take::none};
    bool edge{false};      // the press began this move
    bool retry{false};     // a press in the air at no hold, still looking ...
    float retryAge{-1.f};  // ... since this long
    bool atHolster{false}; // (refused: why)
    bool lenient{false}, tracked{false}; // (found: how)
    LenientStats st;
    za::Optional<Ledge> ledge;
};

// The take of hand `h` of `move` (its places the body's at s.origin) over `dt`, on the rules `r` and the press memory
// `mem`; `active` its grip holds, `otherActive` the other's, `mantling`, `air` (a walking player off the ground, not
// swimming), `grounded` (on the ground). Single player's frame, a remote client's moves on the server and in his
// replay (`s` the world alone there): one function.
//
// A grip pressed in the air at no hold keeps looking while it stays pressed and the player is in the air (not holding
// on with the other hand), for vr_climb_air_grab_time seconds: a ledge jumped at is in the hand's reach for a frame or
// three as it passes the lip on the way up and again on the way down (the hand's point in front of the face: the line
// from the chest holds it off the top); a press anywhere in the jump takes it as it comes.
[[nodiscard]] TakeTry takeTry(climb::TakeMemory& mem, const climb::TakeRules& r, const Seeker& s, const VrMove& move, int h,
    float dt, bool active, bool otherActive, bool mantling, bool air, bool grounded)
{
    TakeTry t;
    const bool pressed = (move.vrBits0 & grabBit[h]) != 0;
    t.edge = pressed && !mem.pressedLast[h];
    mem.pressedLast[h] = pressed;
    const bool inAir = air && !otherActive; // (midAir's)
    if(mem.missAge[h] >= 0.f)
    {
        mem.missAge[h] += dt;
    }
    if(!pressed || t.edge || active || !inAir || mem.missAge[h] > r.airGrabTime)
    {
        mem.missAge[h] = -1.f;
    }
    t.retry = mem.missAge[h] >= 0.f;
    t.retryAge = mem.missAge[h];
    const bool tracked = (move.buttons & protocol::QVR_BUTTON_HANDSTRACKED) != 0;
    if(!r.enabled || !tracked || !(t.edge || t.retry) || active || mantling)
    {
        return t;
    }
    const bool hanging = otherActive;
    const int hotspot = move.hotspots[h];
    t.atHolster = hotspot >= 32 || ((r.holster[hanging ? 1 : 0] >> hotspot) & 1u) != 0u;
    if(r.noGrabLeft > 0.f || t.atHolster || r.busy[h] || (move.buttons & busyButton[h]))
    {
        t.what = Take::refused;
        return t;
    }
    if(!hanging && r.tired)
    {
        mem.missAge[h] = -1.f; // (refused: no buzz again while the grip stays pressed)
        t.what = Take::tired;
        return t;
    }
    const float radius = inAir ? r.leniencyAir : r.leniency;
    if(s.player && (t.ledge = ropeHold(s, move, h, radius, s.feet + r.minHeight)))
    {
        t.what = Take::found; // (a rope in the hand comes before a ledge; the server's own: the replay has no ropes)
        return t;
    }
    const float propLow = s.feet + (grounded && !hanging ? za::max(r.minHeight, r.propMinHeight) : r.minHeight); // (propLowest's)
    t.ledge = gripHold(s, move, h, radius, propLow, t.lenient, t.tracked, &t.st);
    if(!t.ledge)
    {
        if(t.edge)
        {
            mem.missAge[h] = 0.f; // (in the air: it keeps looking)
        }
        t.what = Take::missed;
        return t;
    }
    t.what = Take::found;
    return t;
}

// The take's rules for `ent` now (the server's, once a frame: single player's frame, a remote client's moves and the
// state his replay runs on).
[[nodiscard]] climb::TakeRules takeRules(edict_t* ent, const Climber& c, double time)
{
    climb::TakeRules r;
    r.enabled = vr_climb.value != 0.f && static_cast<int>(ent->v.movetype) == MOVETYPE_WALK && ent->v.health > 0.f;
    r.tired = tooTired(ent);
    for(int h = 0; h < 2; h++)
    {
        r.busy[h] = handBusy(ent, nullptr, h) != nullptr;
    }
    for(int k = 0; k < 2; k++)
    {
        for(int hotspot = 0; hotspot < 32; hotspot++)
        {
            r.holster[k] |= holsterWins(ent, hotspot, k == 1) ? 1u << hotspot : 0u;
        }
    }
    r.noGrabLeft = za::max(0.f, static_cast<float>(c.noGrabUntil - time)); // (0: none, an idle state unchanged)
    r.airGrabTime = vr_climb_air_grab_time.value;
    r.leniency = leniencyUnits(false);
    r.leniencyAir = leniencyUnits(true);
    r.minHeight = vr_climb_min_height.value;
    r.propMinHeight = vr_climb_prop_min_height.value;
    pmove::Hang box;
    hangBox(ent, box);
    r.minsZ = box.minsZ;
    r.maxsX = box.maxsXY[0];
    r.maxsY = box.maxsXY[1];
    r.mantleTime = box.mantleTime;
    r.mantleHeight = box.mantleHeight;
    r.mantleSlope = box.mantleSlope;
    return r;
}

// What the server does with a take (single player's frame, a remote client's move): the prints and buzzes of a refusal,
// the hold taken -> true: taken.
bool takeDone(edict_t* ent, Climber& c, const VrMove& move, int h, const TakeTry& t, double time, int traces0, double t0)
{
    const bool hanging = c.grips[1 - h].active;
    switch(t.what)
    {
    case Take::none:
        return false;
    case Take::refused:
        if(debug() && t.edge)
        {
            const char* busy = handBusy(ent, &move, h);
            Con_Printf("climb: %s hand grips: %s%s\n", handName(h),
                time < c.noGrabUntil ? "too soon" : t.atHolster ? "at a holster" : "not empty: ",
                time < c.noGrabUntil || t.atHolster || !busy ? "" : busy);
        }
        return false;
    case Take::tired:
        // Too little stamina left to hang (parries, shoves, blows, the last hang): no new hang until more comes back
        // (a hand joining the other on a hold is let: two hands tire less). A soft buzz says why; the gadget shows it.
        if(!(time - c.tiredAt < 0.5))
        {
            c.tiredAt = time;
            server::sendHaptic(ent, h, 0.f, 0.15f, 30.f, 0.4f * staminaWarn());
        }
        if(debug())
        {
            Con_Printf("climb: %s hand grips: too tired (%.1f stamina left)\n", handName(h), progs::staminaLeft(ent));
        }
        return false;
    case Take::missed:
    case Take::found:
        break;
    }
    if(debug() && vr_climb_debug.value >= 3.f && (t.edge || t.ledge))
    {
        Con_Printf("climb: hold search: %d ledges looked at, %d traces in %.4f ms\n", t.st.points, traceCount - traces0,
            (Sys_DoubleTime() - t0) * 1000.0);
    }
    const glm::vec3 hand = move.hands[h].pos;
    if(!t.ledge)
    {
        if(debug() && t.edge)
        {
            const glm::vec3 own = move.hands[h].tracked;
            Con_Printf("climb: %s hand at (%.1f %.1f %.1f): no hold (the controller at %.1f %.1f %.1f)\n", handName(h), hand.x,
                hand.y, hand.z, own.x, own.y, own.z);
        }
        return false;
    }
    const char* how = t.lenient ? (t.tracked ? ", lenient, from the controller" : ", lenient")
                      : t.tracked ? ", from the controller"
                                  : "";
    takeHold(ent, c, &move, h, t.ledge, hanging, how, t.retry ? t.retryAge : -1.f);
    return true;
}

// A hand's grip this frame: letting go; then (unless a remote client's moves take, `inMoves`: climb::moveTake) refused
// or taking hold, the frame as one move.
void gripHand(edict_t* ent, Climber& c, const VrMove* move, bool enabled, double time, int h, bool inMoves)
{
    Grip& g = c.grips[h];
    const bool pressed = move && (move->vrBits0 & grabBit[h]);
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

    if(inMoves)
    {
        return;
    }
    if(!move)
    {
        c.take.pressedLast[h] = false;
        c.take.missAge[h] = -1.f;
        return;
    }
    climb::TakeRules r = takeRules(ent, c, time);
    r.enabled = r.enabled && enabled;
    const double t0 = Sys_DoubleTime();
    const int traces0 = traceCount;
    const bool air = static_cast<int>(ent->v.movetype) == MOVETYPE_WALK && !(static_cast<int>(ent->v.flags) & FL_ONGROUND) &&
                     ent->v.waterlevel < 2.f;
    const TakeTry t = takeTry(c.take, r, seekerOf(ent), *move, h, static_cast<float>(host_frametime), g.active,
        c.grips[1 - h].active, c.phase == Phase::Mantling, air, (static_cast<int>(ent->v.flags) & FL_ONGROUND) != 0);
    (void)takeDone(ent, c, *move, h, t, time, traces0, t0);
}

// A move's hands as the take sees them (MP B8j part 2): its places relative to the body (the integers sent) put at the
// body's `origin`, as the server and the replay each have it.
[[nodiscard]] VrMove takeMove(const pmove::Input& in, const float* origin)
{
    VrMove m;
    const glm::vec3 o{origin[0], origin[1], origin[2]};
    m.origin = o;
    for(int h = 0; h < 2; h++)
    {
        m.hands[h].pos = o + glm::vec3{in.hands[h][0], in.hands[h][1], in.hands[h][2]};
        m.hands[h].tracked = o + glm::vec3{in.tracked[h][0], in.tracked[h][1], in.tracked[h][2]};
        m.hands[h].throwVel = glm::vec3{in.throwVel[h][0], in.throwVel[h][1], in.throwVel[h][2]};
        m.hotspots[h] = static_cast<za::U8>(in.hotspots[h]);
        m.vrBits0 = static_cast<za::U16>(m.vrBits0 | ((in.grabs & (1 << h)) ? grabBit[h] : 0));
    }
    m.headPos = o + glm::vec3{in.head[0], in.head[1], in.head[2]};
    m.headAngles = glm::vec3{in.steer[0], in.steer[1], in.steer[2]};
    m.buttons = static_cast<za::U8>(in.buttons);
    return m;
}

// (ropes10) A jump off a pole (a held hand on one): the push away from it, level (vr_pole_jump_push units/s): from its
// axis at the hold out to the body; under a level pole (no way out from it) the view's way across it.
[[nodiscard]] bool poleJumpPush(edict_t* ent, const Climber& c, const bool* held, glm::vec3& push)
{
    const int client = NUM_FOR_EDICT(ent) - 1;
    for(int h = 0; h < 2; h++)
    {
        const Grip& g = c.grips[h];
        if(!held[h] || !g.rope || !climbrope::pole(g.entNum))
        {
            continue;
        }
        glm::vec3 axis{0.f, 0.f, 1.f};
        climbrope::Grab g0, g1;
        if(climbrope::grabAlong(g.entNum, client, g.ropeS - 8.f, g0) && climbrope::grabAlong(g.entNum, client, g.ropeS + 8.f, g1) &&
            glm::length(g1.at - g0.at) > 1e-3f)
        {
            axis = glm::normalize(g1.at - g0.at);
        }
        // (Square out from its axis to the body, level: off a slanted pole's upper side that is down its slope, clear of it.)
        const glm::vec3 to = vec(ent->v.origin) - holdNow(g);
        glm::vec3 away = to - axis * glm::dot(to, axis);
        away.z = 0.f;
        if(glm::length(away) < 4.f)
        {
            glm::vec3 level{axis.x, axis.y, 0.f};
            level = glm::length(level) > 0.1f ? glm::normalize(level) : glm::vec3{0.f};
            const float yaw = glm::radians(ent->v.v_angle[YAW]);
            const glm::vec3 fwd{za::cos(yaw), za::sin(yaw), 0.f};
            const glm::vec3 across = fwd - level * glm::dot(fwd, level);
            away = glm::length(across) > 0.3f ? across : fwd;
        }
        push = glm::normalize(away) * za::max(0.f, vr_pole_jump_push.value);
        return true;
    }
    return false;
}

// Let go of everything: the fall, flung by the hands' release.
void fallFlung(edict_t* ent, Climber& c, const VrMove* move, const bool* held, double time)
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
        fling *= units::metresToUnits() * za::max(0.f, vr_climb_fling.value);
        fling.z = za::min(fling.z, flingMaxUp);
        if(glm::length(fling) > flingMax)
        {
            fling *= flingMax / glm::length(fling);
        }
    }
    // Off a rope (see "Holds on ropes"): its swing at the hands kept, a jump off it adds vr_rope_jump up.
    glm::vec3 ropeVel{0.f};
    int ropeHands = 0;
    for(int h = 0; h < 2 && held; h++)
    {
        const Grip& g = c.grips[h];
        if(held[h] && g.rope)
        {
            ropeVel += climbrope::holdVelocity(g.entNum, NUM_FOR_EDICT(ent) - 1, g.ropeS, g.ropeD);
            ropeHands++;
        }
    }
    if(ropeHands > 0)
    {
        fling += ropeVel / static_cast<float>(ropeHands);
        glm::vec3 push{0.f};
        if(c.ropeJumped && poleJumpPush(ent, c, held, push))
        {
            // (ropes10, his note 2026-11-23: off a pole he barely moved) A pole jump: a normal jump up, pushed off it.
            fling.z += za::max(0.f, vr_pole_jump.value);
            fling += push;
        }
        else
        {
            fling.z += c.ropeJumped ? za::max(0.f, vr_rope_jump.value) : 0.f;
        }
        if(glm::length(fling) > ropeLetGoMax)
        {
            fling *= ropeLetGoMax / glm::length(fling);
        }
    }
    setVec(ent->v.velocity, fling);
    if(debug())
    {
        Con_Printf("climb: falls, flung at (%.0f %.0f %.0f)\n", fling.x, fling.y, fling.z);
    }
}

// The one place the phase changes, with its entry and exit work (the regrab delay, the fall, haptics, sounds). `move`
// and `held` (the hands holding at the frame's start) only for Why::Released: the fling.
void transition(edict_t* ent, Climber& c, Phase to, Why why, const VrMove* move, const bool* held)
{
    const double time = qcvm->time;
    const Phase from = c.phase;
    c.phase = to;
    switch(to)
    {
    case Phase::Free:
    {
        if(why == Why::Exhausted)
        {
            for(int h = 0; h < 2; h++)
            {
                if(c.grips[h].active)
                {
                    server::sendHaptic(ent, h, 0.f, 0.5f, 30.f, staminaWarn());
                }
            }
            staminaSound(ent, "player/gasp1.wav", staminaWarn());
        }
        const bool flatRopeWas = c.flatRope;
        if(flatRopeWas)
        {
            c.flatRopeLeft = c.grips[1].rope ? c.grips[1].entNum : c.grips[0].entNum;
            c.flatRopeLeftAt = time;
        }
        letGoAll(c);
        c.flatLadder = false;
        c.flatRope = false;
        c.flatMount = false;
        if(why != Why::NoMove)
        {
            c.noGrabUntil = time + regrabDelay;
        }
        if(why == Why::Released)
        {
            // (the fall, flung by the hands' release; flat play's rope: its swing only, the drawn hands fling nothing)
            fallFlung(ent, c, flatRopeWas ? nullptr : move, held, time);
        }
        else if(why == Why::Exhausted)
        {
            setVec(ent->v.velocity, glm::vec3{0.f});
            if(debug())
            {
                Con_Printf("climb: no stamina left, lets go at (%.1f %.1f %.1f)\n", ent->v.origin[0], ent->v.origin[1],
                    ent->v.origin[2]);
            }
        }
        else if(why == Why::OutOfReach && debug())
        {
            Con_Printf("climb: falls, flung at (0 0 0)\n");
        }
        return;
    }
    case Phase::Hanging:
        if(from == Phase::Free && staminaOn(ent))
        {
            c.wasLow = staminaLow(ent); // (the breath as the hang makes it low, not for a pool low already)
        }
        // Getting on a ladder volume: the run that brought him there stops (hubladder, his note 2026-11-23: it carried
        // him sideways along the ladder while he climbed).
        if(from == Phase::Free && ((c.grips[0].active && c.grips[0].ledge.volume) || (c.grips[1].active && c.grips[1].ledge.volume)))
        {
            ent->v.velocity[0] = ent->v.velocity[1] = 0.f;
        }
        return;
    case Phase::Slipping:
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
            Con_Printf("climb: no stamina left, slipping (%.2f s)\n", za::max(0.f, vr_climb_stamina_slip.value));
        }
        return;
    case Phase::Mantling:
        c.grips[0].active = c.grips[1].active = false;
        c.flatLadder = false;
        c.flatRope = false;
        c.flatHolds[0].on = c.flatHolds[1].on = false; // (a flat mantle puts its own after: flatMantle)
        c.owed = glm::vec3{0.f};
        if(c.gruntAt < 0.0 && !c.flatMount) // (lowering onto a ladder: no effort)
        {
            mantleGrunt(ent); // (a flat mantle's waits for the jump's grunt to end: climbPreThink plays it at gruntAt)
        }
        return;
    }
}

// The QC: the holding hands' grips hidden; which hands hold.
void hideHeldGrips(edict_t* ent, const Climber& c)
{
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
            // (Mantling: both, its hands on the ledge though their grips have gone: no force grab, no blows.)
            bits = g.active || c.phase == Phase::Mantling ? bits | climbingBit[h] : bits & ~climbingBit[h];
        }
        fieldFloat(ent, vrbits) = static_cast<float>(bits);
    }
}

} // namespace

// SV_Physics_Client, before PlayerPreThink: takes hold and lets go (per the grips of the latest move), and hides the
// holding hands' grips from the QC.
static void climbPreThink(edict_t* ent);

namespace
{
void flatMantle(edict_t* ent, Climber& c, const VrMove* move, double time); // (below)
void triggeredMantle(edict_t* ent, Climber& c, const VrMove* move, bool enabled);
void flatLadder(edict_t* ent, Climber& c, const VrMove* move, double time);
void flatRope(edict_t* ent, Climber& c, const VrMove* move, double time);
}

extern "C" void VR_ClimbPreThink(edict_t* ent)
{
    VR_ProfileBegin("climb");
    climbPreThink(ent);
    VR_ProfileEnd();
}

static void climbPreThink(edict_t* ent)
{
    const bool knockedOff = takeKnockOff(ent);

    Climber* cp = climberOf(ent);
    if(!cp)
    {
        return;
    }
    Climber& c = *cp;
    const double time = qcvm->time;
    if(c.lastTime < 0.0 || za::abs(time - c.lastTime) > 1.0) // a new map, a loaded game
    {
        c = Climber{};
    }
    c.lastTime = time;
    if(c.gruntAt >= 0.0 && time >= c.gruntAt)
    {
        c.gruntAt = -1.0;
        mantleGrunt(ent); // the flat mantle's, put off past the jump's grunt (flatMantle)
    }
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

    letGoIfForced(ent, c, enabled, knockedOff);

    const bool held[2] = {c.grips[0].active, c.grips[1].active};
    const bool inMoves = climb::takesInMoves(ent); // (a remote client's moves take, one by one: climb::moveTake)
    followProps(ent, c);
    followRopes(ent, c);
    c.ropeJumped = ropeJump(ent, c); // (a jump press on a rope: every hand lets go, the fall below keeps its swing)
    triggeredMantle(ent, c, move, enabled); // (vr_climb_mantle_trigger: before the grips let go)
    for(int h = 0; h < 2; h++)
    {
        if((c.flatLadder || c.flatRope) && c.hanging())
        {
            // (a flat ladder's holds are its own: flatLadderFrame; a flat rope's, flatRopeFrame)
            c.take.pressedLast[h] = move && (move->vrBits0 & grabBit[h]);
            continue;
        }
        gripHand(ent, c, move, enabled, time, h, inMoves);
    }

    // The phase the grips make now: a hand took hold, or the last let go (the fall, flung by the hands' release).
    switch(c.phase)
    {
    case Phase::Free:
        if(c.anyHeld())
        {
            transition(ent, c, Phase::Hanging, Why::Took);
        }
        break;
    case Phase::Hanging:
    case Phase::Slipping:
        if(!c.anyHeld())
        {
            transition(ent, c, Phase::Free, Why::Released, move, held);
        }
        break;
    case Phase::Mantling: // (no hold is taken while mantling)
        break;
    }
    if(enabled && c.phase == Phase::Free)
    {
        traceFrame(ent, c, move, "free", glm::vec3{0.f}, glm::vec3{0.f});
        flatMantle(ent, c, move, time); // (flatscreen play only: vr_flat_mantle)
        if(c.phase == Phase::Free)
        {
            flatLadder(ent, c, move, time); // (and vr_flat_ladder)
        }
        if(c.phase == Phase::Free)
        {
            flatRope(ent, c, move, time); // (and vr_flat_rope)
        }
    }
    else
    {
        c.flatJumpAt = -1.0;
        c.flatGrounded = false;
    }

    reportRopes(ent, c); // (the rope held: his weight on it, its swing his)
    c.ropeJumped = false;

    // The QC: the holding hands' grips hidden; which hands hold.
    hideHeldGrips(ent, c);
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

namespace
{
// VR_ClientClimb's hang, a phase a function over this.
struct Hang
{
    edict_t* ent;
    Climber& c;
    const VrMove* move;
    double time;
    glm::vec3 rel[2]; // the hands relative to the body, before the body moves
    float weight[2]{0.f, 0.f};
    float weights = 0.f;
    float top = -1e9f;
    int count = 0;
    glm::vec3 wanted{0.f};
    glm::vec3 meant{0.f};
    glm::vec3 origin{0.f};
    glm::vec3 shoulder[2];
    bool driving[2]{false, false};
    glm::vec3 done{0.f};
    bool noRoom{false}; // the steps' mantle test pulled for one, no room (pmove::Hang::noRoom)
};

void ladderGrab(edict_t* ent, Climber& c, const VrMove& move, const Ledge& higher, const Ledge& lower); // (below)

// The mantle's way now, for pmove::mantleStep (one step for single player's frame and a remote client's moves, MP B8h).
// Onto a brush model: the way moves with it (the push carries the body too: VR_ClimbCarried). Onto a prop: as it lies
// now (its spot on it kept). `onto`: what it goes onto (null: the world, or it is gone).
pmove::Mantle mantleWay(Climber& c, edict_t*& onto)
{
    onto = c.mantleEnt > 0 && c.mantleEnt < qcvm->num_edicts ? EDICT_NUM(c.mantleEnt) : nullptr;
    box3d::ClimbBody body;
    if(onto && c.mantleProp && box3d::climbBody(c.mantleEnt, body))
    {
        c.mantleShift = body.origin + body.turn * glm::transpose(c.mantleTurn) * (c.mantleTo - c.mantleBase) - c.mantleTo;
    }
    else if(onto && !c.mantleProp && !onto->free && static_cast<int>(onto->v.solid) == SOLID_BSP)
    {
        c.mantleShift = vec(onto->v.origin) - c.mantleBase;
    }
    else
    {
        onto = nullptr;
    }
    pmove::Mantle m;
    m.on = true;
    setVec(m.from, c.mantleFrom + c.mantleShift);
    setVec(m.mid, c.mantleMid + c.mantleShift);
    setVec(m.to, c.mantleTo + c.mantleShift);
    m.at = c.mantleAt;
    m.time = mantleTime;
    return m;
}

// The mantle after a step (its place set): its time kept; at its end (`done`) he stands, on what it went onto.
void mantleEnd(edict_t* ent, Climber& c, const pmove::Mantle& m, bool done, edict_t* onto)
{
    c.mantleAt = m.at;
    const glm::vec3 pos = vec(ent->v.origin);
    if(done && c.flatMount)
    {
        // Lowered off a ladder's top onto it (flatLadderMount): its rungs taken there.
        c.flatMount = false;
        c.lastOrigin = pos;
        if(const VrMove* move = server::clientMove(ent))
        {
            ladderGrab(ent, c, *move, c.flatMountRungs[0], c.flatMountRungs[1]);
        }
        else
        {
            transition(ent, c, Phase::Free, Why::NoMove);
        }
        SV_CheckWater(ent);
        return;
    }
    if(done)
    {
        transition(ent, c, Phase::Free, Why::Mantled);
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
}

// A mantle's frame (single player, a listen host's own player: once a frame; a remote client's moves run it one by one
// instead, vr_predict.cpp).
void mantleStep(edict_t* ent, Climber& c)
{
    edict_t* onto = nullptr;
    pmove::Mantle m = mantleWay(c, onto);
    const bool done = pmove::mantleStep(m, static_cast<float>(host_frametime), ent->v.origin);
    setVec(ent->v.velocity, glm::vec3{0.f});
    ent->v.flags = static_cast<float>(static_cast<int>(ent->v.flags) & ~FL_ONGROUND);
    mantleEnd(ent, c, m, done, onto);
}

// The hang's state for pmove::hangStep: the holds now, each hand's last place relative to the body, the owed, slipping.
pmove::Hang hangState(const edict_t* ent, const Climber& c)
{
    pmove::Hang h;
    h.on = c.phase == Phase::Hanging || c.phase == Phase::Slipping;
    h.slipping = c.phase == Phase::Slipping;
    for(int k = 0; k < 2; k++)
    {
        hangGripOf(c.grips[k], h.grips[k]);
    }
    setVec(h.owed, c.owed);
    hangBox(ent, h);
    return h;
}

// The holds' own motion since the last frame (a moving brush model carries the body; a light prop's hold doesn't: it is
// pulled towards it), averaged over the holds that carry.
[[nodiscard]] glm::vec3 holdsCarry(const Climber& c)
{
    glm::vec3 carry{0.f};
    int carriers = 0; // (one hand on a moving heavy prop, one on a light crate: the heavy prop's whole carry, not half;
                      // a pusher's is SV_PushMove's)
    for(const Grip& g : c.grips)
    {
        if(g.active && !g.light)
        {
            carry += holdNow(g) - g.lastHold;
            carriers++;
        }
    }
    return carriers > 0 ? carry / static_cast<float>(carriers) : carry;
}

// After the steps (pmove::hangStep): the owed and the hands' last places back into the climber, the steps' sums into
// `hang` for the frame's part (the reach, the owed settled, the mantle's test).
void hangStepped(Climber& c, const pmove::Hang& h, const pmove::HangSums& s, Hang& hang)
{
    c.owed = vec(h.owed);
    for(int k = 0; k < 2; k++)
    {
        Grip& g = c.grips[k];
        const pmove::HangGrip& hg = h.grips[k];
        g.lastRel = vec(hg.lastRel);
        g.missed = hg.missed;
        g.missAge = hg.missAge;
        g.missOrigin = vec(hg.missOrigin);
        g.missHold = vec(hg.missHold);
        g.missTop = hg.missTop;
        hang.weight[k] = s.weight[k];
    }
    hang.origin = vec(s.origin);
    hang.meant = vec(s.meant);
    hang.wanted = vec(s.wanted);
    hang.weights = s.weights;
    hang.top = s.top;
    hang.count = s.count;
}

void beginMantle(edict_t* ent, Climber& c, int k, const pmove::Mantle& m); // (below)
void noRoomBuzz(edict_t* ent, Climber& c, bool noRoom);                     // (below)

// The pull, the carry, what is owed, the limits, the move, then the mantle's test: one step of the frame's time
// (pmove::hangStep and pmove::hangMantle, a remote client's moves' own: MP B8i, B8j) -- true: the mantle began.
bool hangSteps(Hang& hang)
{
    pmove::Hang h = hangState(hang.ent, hang.c);
    const glm::vec3 carry = holdsCarry(hang.c);
    const vec3_t carried{carry.x, carry.y, carry.z};
    const float rel[2][3] = {{hang.rel[0].x, hang.rel[0].y, hang.rel[0].z}, {hang.rel[1].x, hang.rel[1].y, hang.rel[1].z}};
    const float headZ = hang.move->headPos.z - hang.ent->v.origin[2]; // (the head over the body before the step)
    const float dt = static_cast<float>(host_frametime);
    pmove::HangSums sums;
    ClimbBody b{hang.ent};
    pmove::hangStep(b, h, rel, carried, dt, &sums);
    pmove::Mantle m;
    const bool mantles = pmove::hangMantle(b, h, rel, headZ, dt, m);
    b.store();
    hangStepped(hang.c, h, sums, hang);
    if(mantles)
    {
        beginMantle(hang.ent, hang.c, h.mantleHand, m);
    }
    else
    {
        hang.noRoom = h.noRoom;
    }
    return mantles;
}

// Kept within an arm's reach of the pulling holds; under an overhang, the arms stretch.
void keepWithinReach(Hang& hang)
{
    edict_t* const ent = hang.ent;
    Climber& c = hang.c;
    const VrMove* const move = hang.move;
    const glm::vec3& origin = hang.origin;
    const float weights = hang.weights;
    const float (&weight)[2] = hang.weight;
    glm::vec3 (&shoulder)[2] = hang.shoulder;
    bool (&driving)[2] = hang.driving;
    // And within an arm's reach of each hold whose hand pulls (see the top of the file): where the move took the body
    // (a move into the floor or the face, blocked, moves it nowhere), moved back towards the holds, the least that
    // does (a few passes for two holds), from where it was.
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
                g.allowed = za::max(g.allowed, za::min(d, armReach() + za::max(0.f, vr_climb_overhang_stretch.value)));
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
}

// Settled: what of the pull into the face the body couldn't make is owed.
void settleOwed(Hang& hang)
{
    edict_t* const ent = hang.ent;
    Climber& c = hang.c;
    const glm::vec3& origin = hang.origin;
    const glm::vec3& meant = hang.meant;
    glm::vec3& done = hang.done;
    setVec(ent->v.velocity, glm::vec3{0.f});
    ent->v.flags = static_cast<float>(static_cast<int>(ent->v.flags) & ~FL_ONGROUND);
    c.lastOrigin = vec(ent->v.origin);
    done = vec(ent->v.origin) - origin;
    // Owed: what of the pull into the face under the holds (towards the ledge) the body couldn't make.
    glm::vec3 into{0.f};
    for(const Grip& g : c.grips)
    {
        into -= g.active && !g.rope ? g.ledge.out : glm::vec3{0.f}; // (a rope has no face to pull into)
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
}

// Out of reach: hands torn off their holds; the props pulled, the water, the trace, the prints.
void reachCheck(Hang& hang)
{
    edict_t* const ent = hang.ent;
    Climber& c = hang.c;
    const VrMove* const move = hang.move;
    const glm::vec3 (&rel)[2] = hang.rel;
    const glm::vec3 (&shoulder)[2] = hang.shoulder;
    const bool (&driving)[2] = hang.driving;
    const int count = hang.count;
    const glm::vec3& wanted = hang.wanted;
    const glm::vec3& done = hang.done;
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
        g.allowed = za::max(armReach(), za::min(g.allowed, dist[h]));
    }
    pullProps(ent, c);
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
}

// The mantle begins (the hang's steps' test, the flat mantle): its way from `from` up to `mid` and over to `to`,
// following the hold's brush model or prop.
void startMantle(edict_t* ent, Climber& c, const Grip& g, const glm::vec3& from, const glm::vec3& mid,
    const glm::vec3& to, float headAbove)
{
    c.mantleAt = 0.f;
    c.mantleFrom = from;
    c.mantleMid = mid;
    c.mantleTo = to;
    c.mantleEnt = g.entNum;
    c.mantleBase = c.mantleEnt > 0 ? vec(EDICT_NUM(c.mantleEnt)->v.origin) : glm::vec3{0.f};
    c.mantleShift = glm::vec3{0.f};
    c.mantleProp = g.prop;
    if(g.prop)
    {
        c.mantleBase = g.propOrigin;
        c.mantleTurn = g.propTurn;
    }
    transition(ent, c, Phase::Mantling, Why::Mantles);
    if(debug())
    {
        Con_Printf("climb: mantle from (%.1f %.1f %.1f) up to %.1f, onto (%.1f %.1f %.1f), the head %.1f over the "
                   "top (Mantle Height %.0f)\n",
            c.mantleFrom.x, c.mantleFrom.y, c.mantleFrom.z, mid.z, to.x, to.y, to.z, headAbove,
            vr_climb_mantle_height.value);
    }
}

// The mantle begun by the hang's steps' test (pmove::hangMantle) from hand `k`'s hold: its way as the test found it.
void beginMantle(edict_t* ent, Climber& c, int k, const pmove::Mantle& m)
{
    const Grip& g = c.grips[k & 1];
    startMantle(ent, c, g, vec(m.from), vec(m.mid), vec(m.to), m.headAbove);
}

// A grip high enough to mantle from (pmove::hangMantle's test, for triggeredMantle): its hand (`rel`, relative to the
// body) pulled down by `mantlePull` since it took hold, the head `headAbove` its top by at least vr_climb_mantle_height,
// and not slipping. (Under the height, he can lean up and down over the ledge without mantling.)
[[nodiscard]] bool highEnough(const Climber& c, const Grip& g, const glm::vec3& rel, float headAbove)
{
    return g.active && c.phase != Phase::Slipping && g.relAtGrab.z - rel.z >= pmove::mantlePull &&
           headAbove >= za::clamp(vr_climb_mantle_height.value, 0.f, 40.f);
}

// The mantle from hand `h`'s grip (high enough; triggeredMantle): true when it started; `noRoom` set when there's no
// room on top. The search's record of no room is pmove::hangMantle's (the grip's missed, missAge, ...).
bool mantleFrom(edict_t* ent, Climber& c, int h, float headAbove, bool& noRoom)
{
    Grip& g = c.grips[h];
    // No room found here lately, the body and the hold where they were: none yet (searched again after
    // pmove::mantleRetry, or once either moves).
    const glm::vec3 now = vec(ent->v.origin), holdAt = holdNow(g);
    if(g.missed && g.missAge < pmove::mantleRetry && glm::distance(now, g.missOrigin) <= pmove::mantleRetryMove &&
        holdAt == g.missHold && topNow(g) == g.missTop)
    {
        if(vr_climb_debug.value >= 4.f)
        {
            Con_Printf("climbcost mantle %s: no room (searched %.3f s ago)\n", handName(h), g.missAge);
        }
        noRoom = true;
        return false;
    }
    glm::vec3 mid, to;
    const bool found = findMantle(ent, g, mid, to);
    g.missed = !found;
    if(!found)
    {
        g.missAge = 0.f;
        g.missOrigin = now;
        g.missHold = holdAt;
        g.missTop = topNow(g);
        noRoom = true;
        return false;
    }
    startMantle(ent, c, g, now, mid, to, headAbove);
    return true;
}

// No room on top: a soft buzz in the holding hands.
void feelNoRoom(edict_t* ent, const Climber& c)
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

// No room on top: a soft buzz as the pull over the top starts (the body stays on the face: the pull into it is owed, so
// the push over the top doesn't push it back off).
void noRoomBuzz(edict_t* ent, Climber& c, bool noRoom)
{
    if(noRoom && !c.noRoom)
    {
        feelNoRoom(ent, c);
    }
    c.noRoom = noRoom;
}

// The flat mantle's hands (his notes 2026-11-08): each empty hand on the ledge as the mantle starts, as a headset's hands
// hold it -- both empty, both; one empty (a free hand on the other's gun's hotspot is empty: it lets go of the hotspot
// for the mantle and goes back after, drawn so: vr_view_hands.cpp), that one; both holding something, none. Each at its
// own side of where the ledge was found, vr_flat_mantle_hand_spread apart along the lip, its hand's place on it as a
// grip's (holdLedge: on the edge's line); a new hold to the drawn hand (its serial), eased on from where it was drawn.
void flatMantleHands(edict_t* ent, Climber& c, const VrMove* move, const Grip& g)
{
    const glm::vec3 right{-g.ledge.out.y, g.ledge.out.x, 0.f}; // (facing the ledge, away from `out`: his right)
    const float half = 0.5f * za::clamp(vr_flat_mantle_hand_spread.value, 0.f, 32.f);
    for(int h = 0; h < 2; h++)
    {
        Climber::FlatHold& f = c.flatHolds[h];
        f.on = handEmpty(ent, move, h);
        if(!f.on)
        {
            continue;
        }
        f.hold = g.hold + right * (h == 1 ? half : -half);
        f.out = g.ledge.out;
        f.entNum = g.prop ? 0 : g.entNum;
        f.entOrigin = g.entOrigin;
        c.grips[h].serial++;
    }
    if(debug())
    {
        Con_Printf("climb: flat mantle's hands on the ledge: off %d, main %d\n", c.flatHolds[0].on ? 1 : 0, c.flatHolds[1].on ? 1 : 0);
    }
}

// The mantle on a trigger (vr_climb_mantle_trigger), from climbPreThink before the grips let go: high enough over a
// ledge (pmove::hangMantle's test, the body and hands as the last frame left them), the last holding hand letting go of its
// grip, or a jump pressed, pulls the body up onto it. Lower, or with no room on top, a release lets go as ever (the
// fall); a jump with no room buzzes and it hangs on. Flatscreen play's ladder keeps its own top (flatLadderFrame).
void triggeredMantle(edict_t* ent, Climber& c, const VrMove* move, bool enabled)
{
    const bool jump = ent->v.button2 != 0.f;
    const bool jumpPress = jump && !c.mantleJumpWas; // (a jump held as the hand took hold waits for the next press)
    c.mantleJumpWas = jump;
    const int trigger = mantleTrigger();
    if(trigger == 0 || !enabled || !move || !c.hanging() || c.flatLadder || c.flatRope)
    {
        return;
    }
    bool lastLetsGo = true; // no holding hand keeps its grip pressed
    for(int h = 0; h < 2; h++)
    {
        lastLetsGo = lastLetsGo && !(c.grips[h].active && (move->vrBits0 & grabBit[h]));
    }
    const bool onLetGo = (trigger & mantleOnLetGo) && lastLetsGo;
    const bool onJump = (trigger & mantleOnJump) && jumpPress;
    if(!onLetGo && !onJump)
    {
        return;
    }
    bool noRoom = false;
    for(int h = 0; h < 2; h++)
    {
        const float headAbove = move->headPos.z - topNow(c.grips[h]);
        if(highEnough(c, c.grips[h], handRel(ent, *move, h), headAbove) && mantleFrom(ent, c, h, headAbove, noRoom))
        {
            if(onJump)
            {
                // (the jump key held onto the top doesn't jump there: QC PlayerJump waits for its release)
                ent->v.flags = static_cast<float>(static_cast<int>(ent->v.flags) & ~FL_JUMPRELEASED);
            }
            if(debug())
            {
                Con_Printf("climb: mantle on %s (Mantle When %d)\n", onLetGo ? "letting go" : "a jump", trigger);
            }
            return;
        }
    }
    if(noRoom && !onLetGo)
    {
        feelNoRoom(ent, c);
    }
}

// Flatscreen play's mantle (vr_flat_mantle; the author's note 2026-11-08). No hands to hang from: after a jump, the jump
// key still held and the view looking up by vr_flat_mantle_look .. vr_flat_mantle_look_max degrees, within
// vr_flat_mantle_time seconds of leaving the ground, a ledge ahead is taken as a hand's grip takes one (findHold, its
// checks: room over the hold, reached, in sight) and the headset's mantle follows (findMantle: the same search for room
// on top; startMantle, mantleStep: the same way up and over, its time and grunt). The ledge: its lip at most
// vr_flat_mantle_reach units ahead of the box's front, its top at most vr_flat_mantle_height over the eyes and at least
// vr_climb_min_height over the feet, facing him; the highest such (a stair of ledges: its top step in reach). Free
// only (not holding on), vr_climb on, standing in no water.
void flatMantle(edict_t* ent, Climber& c, const VrMove* move, double time)
{
    const bool ground = (static_cast<int>(ent->v.flags) & FL_ONGROUND) != 0;
    const bool jumpHeld = ent->v.button2 != 0.f;
    const bool wasGround = c.flatGrounded;
    c.flatGrounded = ground;
    if(ground || !jumpHeld || !move || vr_flat_mantle.value == 0.f || !hull::isFlat(ent) || ent->v.waterlevel >= 2.f)
    {
        c.flatJumpAt = -1.0;
        return;
    }
    constexpr float jumpedUp = 60.f; // u/s up off the ground: a jump, not a step off a ledge (vr_flatmove.cpp's)
    if(wasGround && ent->v.velocity[2] > jumpedUp)
    {
        c.flatJumpAt = time;
    }
    const float up = -move->headAngles.x; // (Quake's pitch: up is negative)
    const float lookMin = za::clamp(vr_flat_mantle_look.value, -90.f, 90.f);
    const float lookMax = za::clamp(vr_flat_mantle_look_max.value, lookMin, 90.f);
    if(c.flatJumpAt < 0.0 || time - c.flatJumpAt > za::max(0.f, vr_flat_mantle_time.value) || time < c.noGrabUntil ||
       up < lookMin || up > lookMax)
    {
        return;
    }
    const glm::vec3 origin = vec(ent->v.origin);
    const glm::vec3 eye = move->headPos;
    const float yaw = glm::radians(move->headAngles.y);
    const glm::vec3 fwd{za::cos(yaw), za::sin(yaw), 0.f};
    const float reach = za::clamp(vr_flat_mantle_reach.value, 2.f, 64.f);
    const float feet = origin.z + ent->v.mins[2];
    const float highest = eye.z + za::clamp(vr_flat_mantle_height.value, -32.f, 64.f);
    const float lowest = feet + vr_climb_min_height.value;
    // A column of points ahead, from the highest top down: each finds the nearest hold round it (the leniency's search,
    // its radius half the reach and a little: the column covers the reach ahead of the box's front).
    const float radius = reach * 0.5f + 4.f;
    const glm::vec3 column = origin + fwd * (ent->v.maxs[0] + reach * 0.5f);
    const float propLow = propLowest(ent, false);
    za::Optional<Ledge> best;
    for(float z = highest; z > lowest - radius; z -= radius)
    {
        bool lenient = false;
        const za::Optional<Ledge> l = findHold(seekerOf(ent), glm::vec3{column.x, column.y, z}, glm::vec3{0.f}, eye, radius, propLow, lenient);
        if(l && l->top <= highest && l->top >= lowest && glm::dot(l->out, fwd) < -0.5f && (!best || l->top > best->top))
        {
            best = l;
        }
    }
    if(vr_climb_debug.value >= 3.f)
    {
        Con_Printf("climb: flat mantle search %.2f s after the jump, looking up %.0f, the feet at %.1f, the eyes at %.1f: %s\n",
            time - c.flatJumpAt, up, feet, eye.z, best ? va("the ledge at %.1f", best->top) : "no ledge");
    }
    if(!best)
    {
        return;
    }
    Grip g;
    (void)holdLedge(g, *best);
    glm::vec3 mid, to;
    if(!findMantle(ent, g, mid, to))
    {
        if(vr_climb_debug.value >= 2.f)
        {
            Con_Printf("climb: flat mantle: no room on the ledge at %.1f\n", best->top);
        }
        return;
    }
    if(debug())
    {
        Con_Printf("climb: flat mantle onto the ledge at %.1f (%.1f over the feet, %.1f over the eyes; looking up %.0f, %.2f s "
                   "after the jump)\n",
            best->top, best->top - feet, best->top - eye.z, up, time - c.flatJumpAt);
    }
    c.lastOrigin = origin;
    // The grunt: the jump's (QC PlayerJump, CHAN_BODY) is still playing this soon after it, and the two together sounded
    // as the jump's alone (the author's note 2026-11-08): put off until vr_flat_mantle_grunt_delay after the jump.
    const double gruntAt = c.flatJumpAt + za::clamp(vr_flat_mantle_grunt_delay.value, 0.f, 1.f);
    c.gruntAt = gruntAt > time ? gruntAt : -1.0;
    c.flatJumpAt = -1.0;
    c.flatMantles++;
    startMantle(ent, c, g, vec(ent->v.origin), mid, to, eye.z - best->top);
    flatMantleHands(ent, c, move, g);
}

// Flatscreen play's ledge ladder (vr_flat_ladder; his request 2026-11-16: "ladders" made of individual ledges, like the
// tutorial's, climbable on the desktop screen). A headset's hands take its rungs one over the other; here walking into
// it does: the hands take the rungs as a grip takes a hold (findHold, holdLedge), the hang is the headset's
// (Phase::Hanging: its stamina, the holds carried by a moving brush model, the QC's climbing bits, the drawn hands and
// the climbing feet), only the body is moved by the keys instead of by the hands' pull, and the top's mantle is the
// headset's (findMantle, startMantle, at vr_climb_mantle_height; the flat mantle's hands on the lip). A ladder: two
// ledges facing him, one over the other (at most ladderGap apart in height, ladderStack along the look), in reach ahead
// of his box's front (vr_flat_ladder_reach). A plain wall, a single ledge, a stair's steps (each one further on): none.
constexpr float ladderGap = 40.f;   // units: the most two rungs may be apart in height
constexpr float ladderStack = 10.f; // and along the look (one over the other)
constexpr int maxRungs = 16;
constexpr float ladderInBox = 8.f;  // units: rungs looked for this far inside the box's front too
constexpr float ladderVolumeGap = 24.f; // units: how far past a floor's edge a ladder volume's rungs may stand (flatLadderMount)

struct Rungs
{
    Ledge at[maxRungs]; // lowest first
    int count{0};
};

// The ledges facing along `fwd` in the column vr_flat_ladder_reach ahead of the box's front (`behind`: the column behind
// its back instead, the ladder's top under it), tops from `low` to `high`, seen from `eye` (findHold's head).
// `drawnBelow`: a ladder's drawn rungs (drawnRung, a headset's hands' too: one source for both; his decision 2026-11-22)
// may stand farther apart than the window over the feet (kellbase1's bars 24 and 32 apart, the window about 31): when
// the window saw one, drawn rungs that far under `low` count too (the lower hand under Lowest Ledge). Other holds never
// do, so a ladder with no drawn rungs finds what it did.
void findRungs(edict_t* ent, const glm::vec3& eye, const glm::vec3& fwd, float low, float high, Rungs& out, bool behind = false,
    float drawnBelow = 0.f)
{
    out.count = 0;
    // (From ladderInBox inside the box's front, as a ladder's thin rungs needn't stop the box: the tutorial's don't.)
    const float reach = za::clamp(vr_flat_ladder_reach.value, 4.f, 48.f);
    const float radius = (reach + ladderInBox) * 0.5f + 4.f;
    const float ahead = ent->v.maxs[0] + (reach - ladderInBox) * 0.5f;
    const glm::vec3 column = vec(ent->v.origin) + fwd * (behind ? -ahead : ahead);
    const float propLow = propLowest(ent, true);
    const float feetLowest = ent->v.origin[2] + ent->v.mins[2] + vr_climb_min_height.value;
    constexpr float step = 6.f; // (finer than any ladder's rungs: each is the nearest hold to one of the points)
    bool drawnSeen = false;
    // (The window's probes, then, only once it saw a drawn rung, those under it down to drawnBelow: drawn rungs alone.)
    for(int pass = 0; pass < 2; pass++)
    {
        const bool under = pass == 1;
        if(under && (!drawnSeen || drawnBelow <= 0.f))
        {
            break;
        }
        const float from = under ? low - drawnBelow : low, to = under ? low - 0.5f * step : high + radius;
        const float bottom = under ? low - drawnBelow : low;
        const float lower = za::max(0.f, feetLowest - bottom);
        for(float z = from; z <= to && out.count < maxRungs; z += step)
        {
            bool lenient = false;
            const za::Optional<Ledge> l = findHold(seekerOf(ent), glm::vec3{column.x, column.y, z}, glm::vec3{0.f}, eye, radius, propLow,
                lenient, nullptr, lower);
            if(vr_climb_debug.value >= 4.f)
            {
                Con_Printf("climb: flat ladder probe (%.1f %.1f %.1f) r %.1f [%.1f..%.1f]: %s\n", column.x, column.y, z, radius, bottom,
                    high, l ? va("top %.1f out %.2f %.2f hold %.1f %.1f %.1f%s", l->top, l->out.x, l->out.y, l->hold.x, l->hold.y,
                                  l->hold.z, l->drawn ? " drawn" : "")
                            : "none");
            }
            drawnSeen = drawnSeen || (l && l->drawn);
            if(!l || l->top < bottom || l->top > high || glm::dot(l->out, fwd) > -0.5f || (under && !l->drawn))
            {
                continue;
            }
            bool known = false;
            for(int i = 0; i < out.count; i++)
            {
                known = known || za::abs(out.at[i].top - l->top) < 2.f;
            }
            if(known)
            {
                continue;
            }
            int i = out.count++;
            for(; i > 0 && out.at[i - 1].top > l->top; i--)
            {
                out.at[i] = out.at[i - 1];
            }
            out.at[i] = *l;
        }
    }
}

// Which way the keys climb a ladder: forward up (looking down past vr_flat_ladder_look_down: down), back down; 0 none.
[[nodiscard]] int ladderDir(edict_t* ent, const VrMove& move)
{
    const int client = NUM_FOR_EDICT(ent) - 1;
    if(client < 0 || client >= svs.maxclients)
    {
        return 0;
    }
    const float forward = svs.clients[client].cmd.forwardmove;
    if(forward < -1.f)
    {
        return -1;
    }
    if(forward > 1.f)
    {
        return move.headAngles.x > za::clamp(vr_flat_ladder_look_down.value, 0.f, 90.f) ? -1 : 1; // (pitch: down is +)
    }
    return 0;
}

// The shoulders' middle now (the move's, carried with the body since the client placed it).
[[nodiscard]] glm::vec3 ladderShoulder(edict_t* ent, const VrMove& move)
{
    return 0.5f * (shoulderOf(move, 0) + shoulderOf(move, 1)) - move.origin + vec(ent->v.origin);
}

// Hand `h` takes rung `l`: a grip's hold on it, at its own side of the rung (vr_flat_mantle_hand_spread apart, as the
// flat mantle's hands), drawn there if it is empty.
void ladderTake(edict_t* ent, Climber& c, const VrMove& move, int h, const Ledge& l)
{
    Grip& g = c.grips[h];
    g.active = true;
    g.owned = false;
    g.serial++;
    (void)holdLedge(g, l);
    const glm::vec3 right{-l.out.y, l.out.x, 0.f}; // (facing the rung, away from `out`: his right)
    const float half = 0.5f * za::clamp(vr_flat_mantle_hand_spread.value, 0.f, 32.f);
    g.hold = g.lastHold = l.hold + right * (h == 1 ? half : -half);
    g.allowed = armReach();
    g.relAtGrab = g.lastRel = handRel(ent, move, h);
    g.grabOffset = glm::vec3{0.f};
    c.flatShow[h] = handEmpty(ent, &move, h);
    if(c.flatShow[h])
    {
        physsound::grab(ent, g.hold, l.ent); // (the slap a headset's grip makes)
    }
    if(debug())
    {
        Con_Printf("climb: flat ladder: %s hand takes the rung at %.1f (feet at %.1f)\n", handName(h), l.top,
            ent->v.origin[2] + ent->v.mins[2]);
    }
}

bool flatLadderMount(edict_t* ent, Climber& c, const VrMove& move, const glm::vec3& fwd, double time); // (below)

// Free: walking into a ladder takes hold of it (forward; in the air, back too; on its top, back lowers onto it).
void flatLadder(edict_t* ent, Climber& c, const VrMove* move, double time)
{
    if(!move || vr_flat_ladder.value == 0.f || !hull::isFlat(ent) || ent->v.waterlevel >= 2.f || time < c.noGrabUntil)
    {
        return;
    }
    const bool ground = (static_cast<int>(ent->v.flags) & FL_ONGROUND) != 0;
    const int dir = ladderDir(ent, *move);
    if(dir == 0 || (c.flatLadderMiss >= 0.0 && time >= c.flatLadderMiss && time - c.flatLadderMiss < 0.1))
    {
        return;
    }
    const glm::vec3 origin = vec(ent->v.origin);
    const float yaw = glm::radians(move->headAngles.y);
    const glm::vec3 fwd{za::cos(yaw), za::sin(yaw), 0.f};
    if(ground && dir < 0)
    {
        // Backing off a ladder's top (facing away from its drop): lowered onto it. The back key only: forward looking down
        // (ladderDir's -1 too) walks on, away from the drop. (ladderDir checked the client.)
        if(svs.clients[NUM_FOR_EDICT(ent) - 1].cmd.forwardmove >= -1.f)
        {
            return;
        }
        if(!flatLadderMount(ent, c, *move, fwd, time))
        {
            c.flatLadderMiss = time;
        }
        return;
    }
    const glm::vec3 shoulder = ladderShoulder(ent, *move);
    Rungs r;
    findRungs(ent, move->headPos, fwd, origin.z + ent->v.mins[2] + vr_climb_min_height.value, shoulder.z + armReach(), r, false, ladderGap);
    // A ladder whose rungs stop the box (steps cut in its wall, AD's ad_test3) can't be come nearer: its rungs are reached
    // from the shoulders leant in to the box's front. (Thin rungs, the tutorial's, never stop it: taken from where he is.)
    glm::vec3 reachFrom = shoulder;
    if(traceBox(origin, vec(ent->v.mins), vec(ent->v.maxs), origin + fwd * 2.f, MOVE_NOMONSTERS, ent).fraction < 1.f)
    {
        reachFrom += fwd * za::max(0.f, glm::dot(origin + fwd * ent->v.maxs[0] - shoulder, fwd));
    }
    int top = -1; // the higher of the highest two rungs one over the other in reach
    for(int i = r.count - 1; i > 0 && top < 0; i--)
    {
        const Ledge& a = r.at[i - 1];
        const Ledge& b = r.at[i];
        const float gap = b.top - a.top;
        if(vr_climb_debug.value >= 3.f)
        {
            Con_Printf("climb: flat ladder pair %.1f/%.1f: gap %.1f, %.1f along the look, %.1f from the shoulders (reach %.1f)\n", a.top,
                b.top, gap, glm::dot(b.hold - a.hold, fwd), glm::distance(b.hold, reachFrom), armReach() + 4.f);
        }
        if(gap > 4.f && gap <= ladderGap && za::abs(glm::dot(b.hold - a.hold, fwd)) <= ladderStack &&
            glm::distance(b.hold, reachFrom) <= armReach() + 4.f)
        {
            top = i;
        }
    }
    if(vr_climb_debug.value >= 3.f)
    {
        Con_Printf("climb: flat ladder search: %d ledges ahead, %s\n", r.count, top >= 0 ? va("rungs at %.1f and %.1f", r.at[top - 1].top, r.at[top].top) : "no ladder");
    }
    if(top < 0)
    {
        c.flatLadderMiss = ground ? time : -1.0; // (in the air, each frame: a ladder is passed in a few)
        return;
    }
    ladderGrab(ent, c, *move, r.at[top], r.at[top - 1]);
}

// Takes the ladder: the main hand on the higher rung (the next goes over it), the off hand on the lower; the hang.
void ladderGrab(edict_t* ent, Climber& c, const VrMove& move, const Ledge& higher, const Ledge& lower)
{
    ladderTake(ent, c, move, 1, higher);
    ladderTake(ent, c, move, 0, lower);
    c.flatLadder = true;
    c.flatLadderOut = higher.out;
    c.flatJumpWas = ent->v.button2 != 0.f; // (a jump held onto it pushes off only when pressed again)
    c.flatLadderTry = -1.0;
    c.flatLadders++;
    c.owed = glm::vec3{0.f};
    c.noRoom = false;
    c.lastOrigin = vec(ent->v.origin);
    setVec(ent->v.velocity, glm::vec3{0.f});
    transition(ent, c, Phase::Hanging, Why::Took);
    if(debug())
    {
        Con_Printf("climb: flat ladder: takes hold at (%.1f %.1f %.1f), rungs at %.1f and %.1f\n", ent->v.origin[0],
            ent->v.origin[1], ent->v.origin[2], lower.top, higher.top);
    }
}

// Standing on a ladder's top, his back to its drop (facing away from it), back held at its lip: lowered onto it, the
// mantle's way backwards (over the lip, then down; mantleStep, its time; the hands on the lip as the flat mantle's),
// to hang with the feet just under where the rung below the top is in reach (vr_climb_min_height), its rungs taken
// there (ladderGrab). Whether it began.
bool flatLadderMount(edict_t* ent, Climber& c, const VrMove& move, const glm::vec3& fwd, double time)
{
    const glm::vec3 origin = vec(ent->v.origin);
    const float feet = origin.z + ent->v.mins[2];
    Rungs r;
    // (Seen from over the drop, where the head will be: from where it stands the top's corner hides the rungs.)
    findRungs(ent, move.headPos - fwd * (2.f * ent->v.maxs[0] + 2.f), fwd, feet - ladderGap - 8.f, feet + 8.f, r, true, ladderGap);
    if(r.count < 2)
    {
        return false;
    }
    Ledge lip = r.at[r.count - 1];
    Ledge below = r.at[r.count - 2];
    const float gap = lip.top - below.top;
    // AD support: a ladder volume's rungs (a Quoth or AD ladder standing free at a floor's edge, kellbase1's up to its
    // catwalk) may stand up to ladderVolumeGap beyond the lip, the lip itself the floor's edge.
    const bool volume = below.volume;
    const glm::vec3 line = volume ? below.hold : lip.hold;
    // (A volume's top rung may be one under the floor's edge, the edge itself the real ledge facing another way: a drawn
    // rung up to ladderGap under it, as its bars stand.)
    if(za::abs(lip.top - feet) > (lip.drawn ? ladderGap : lip.volume ? ladderRungStep + 8.f : 8.f) || gap <= 4.f || gap > ladderGap ||
        (!volume && za::abs(glm::dot(lip.hold - below.hold, fwd)) > ladderStack) ||
        glm::dot(origin - line, fwd) > ent->v.maxs[0] + 8.f + (volume ? ladderVolumeGap : 0.f))
    {
        return false;
    }
    // Hanging: the box's front just off the lip's line, the feet so the rung under it is just in reach. A volume's
    // ladder: off its rungs on this side, else (a floor's edge over it, a clip) on the far side, facing back to the top,
    // the box ladderInBox into its rungs as flatLadderFrame keeps it.
    const float hangZ = below.top - vr_climb_min_height.value - ent->v.mins[2] - 1.f;
    const auto tryHang = [&](const glm::vec3& to, glm::vec3& mid) {
        mid = glm::vec3{to.x, to.y, origin.z};
        const trace_t over = tracePlayer(ent, origin, mid);
        const trace_t down = tracePlayer(ent, mid, to);
        return !over.startsolid && over.fraction >= 1.f && !down.allsolid && down.fraction >= 1.f;
    };
    // (A volume's: out of the ladder (ladderKeep's), else as near it as there is room, down to ladderInBox into its bar.)
    const float off = below.drawn ? ent->v.maxs[0] + below.ladderFace + 1.f
                      : volume    ? ent->v.maxs[0] - ladderInBox - 0.5f
                                  : ent->v.maxs[0] + 1.f;
    const float offLeast = volume ? ent->v.maxs[0] - 2.f * ladderInBox - 0.5f : off;
    glm::vec3 to, mid;
    bool ok = false;
    for(float o = off; !ok; o -= 4.f)
    {
        o = za::max(o, offLeast);
        to = glm::vec3{line.x + lip.out.x * o, line.y + lip.out.y * o, hangZ};
        ok = tryHang(to, mid);
        if(o <= offLeast)
        {
            break;
        }
    }
    if(!ok && volume)
    {
        const float back = ent->v.maxs[0] - ladderInBox - 0.5f;
        to = glm::vec3{line.x - lip.out.x * back, line.y - lip.out.y * back, hangZ};
        ok = tryHang(to, mid);
        if(ok)
        {
            lip.out = -lip.out;
            below.out = -below.out;
            lip.hold = glm::vec3{below.hold.x, below.hold.y, lip.hold.z}; // (the top hand on the ladder's own top rung)
        }
    }
    if(!ok)
    {
        if(vr_climb_debug.value >= 2.f)
        {
            Con_Printf("climb: flat ladder: no way down onto it from its top at %.1f\n", lip.top);
        }
        return false;
    }
    Grip g;
    (void)holdLedge(g, lip);
    c.flatMount = true;
    c.flatMountRungs[0] = lip;
    c.flatMountRungs[1] = below;
    c.lastOrigin = origin;
    startMantle(ent, c, g, origin, mid, to, 0.f);
    flatMantleHands(ent, c, &move, g);
    if(debug())
    {
        Con_Printf("climb: flat ladder: lowered onto it from its top at %.1f, to (%.1f %.1f %.1f), the rung's hold (%.1f %.1f)\n",
            lip.top, to.x, to.y, to.z, below.hold.x, below.hold.y);
    }
    return true;
}

// A ladder volume held (a Quoth or AD trigger_ladder: Ledge::volume; hubladder, his note 2026-11-23): the body kept out
// of the ladder, its box's front at the ladder's near side (Ledge::ladderFace), eased out at ladderPushSpeed (got on
// inside it: a free-standing ladder's bars are not solid, so the view and the hands were in them), and flat-screen
// (`centre`) eased to the middle of its width at ladderCentreSpeed. `target`: the body's place this frame, moved.
// Whether a ladder volume is held.
constexpr float ladderPushSpeed = 160.f;  // units/s
constexpr float ladderCentreSpeed = 64.f; // units/s
bool ladderKeep(const edict_t* ent, const Climber& c, glm::vec3& target, float dt, bool centre)
{
    const Grip* held = nullptr;
    for(const Grip& g : c.grips)
    {
        held = held ? held : (g.active && g.ledge.volume ? &g : nullptr);
    }
    if(!held)
    {
        return false;
    }
    const glm::vec2 out{held->ledge.out.x, held->ledge.out.y};
    const glm::vec3 hold = holdNow(*held);
    const float ahead = glm::dot(glm::vec2{target.x - hold.x, target.y - hold.y}, out);
    // (A volume with no rungs drawn: nothing seen to keep out of; as near as the flat ladder's other rungs, ladderInBox.)
    if(!held->ledge.drawn && !centre)
    {
        return false; // (a headset's body: kept out of drawn rungs only)
    }
    const float least = held->ledge.drawn ? ent->v.maxs[0] + held->ledge.ladderFace + 1.f : ent->v.maxs[0] - ladderInBox;
    if(ahead < least)
    {
        const glm::vec2 push = out * za::min(least - ahead, ladderPushSpeed * dt);
        target += glm::vec3{push.x, push.y, 0.f};
        if(vr_climb_debug.value >= 3.f)
        {
            Con_Printf("climb: ladder keep: %.1f in front of the hold (least %.1f), pushed out to (%.1f %.1f)\n", ahead, least,
                target.x, target.y);
        }
    }
    if(centre)
    {
        const glm::vec2 right{-out.y, out.x};
        const float off = glm::dot(glm::vec2{target.x, target.y} - held->ledge.ladderMid, right);
        const glm::vec2 step = right * za::clamp(off, -ladderCentreSpeed * dt, ladderCentreSpeed * dt);
        target -= glm::vec3{step.x, step.y, 0.f};
    }
    return true;
}

// A ladder's frame (hangFrame's, for a flat ladder): the jump pushes off; the keys move the body up or down (the feet
// no higher than the top hold, the shoulders no lower than an arm under the lowest); the hands go hand over hand; the
// ground ends it going down, the top's mantle going up; the stamina as any hang's.
int flatLadderFrame(edict_t* ent, Climber& c, const VrMove& move, double time)
{
    if(vr_flat_ladder.value == 0.f || !hull::isFlat(ent))
    {
        transition(ent, c, Phase::Free, Why::Forced);
        return 0;
    }
    const glm::vec3 out = c.flatLadderOut;
    const bool jump = ent->v.button2 != 0.f;
    const bool pushOff = jump && !c.flatJumpWas;
    c.flatJumpWas = jump;
    if(pushOff)
    {
        transition(ent, c, Phase::Free, Why::Forced);
        const float push = za::clamp(vr_flat_ladder_push.value, 0.f, 600.f);
        setVec(ent->v.velocity, out * push + glm::vec3{0.f, 0.f, 0.5f * push});
        if(debug())
        {
            Con_Printf("climb: flat ladder: pushes off at (%.1f %.1f %.1f)\n", ent->v.origin[0], ent->v.origin[1], ent->v.origin[2]);
        }
        return 0; // (the usual move from here: it falls away)
    }
    const int dir = c.phase == Phase::Slipping ? -1 : ladderDir(ent, move);
    const float dt = static_cast<float>(host_frametime);
    const glm::vec3 origin = vec(ent->v.origin);
    glm::vec3 carry{0.f};
    int held = 0;
    float top = -1e9f, low = 1e9f;
    for(const Grip& g : c.grips)
    {
        if(g.active)
        {
            carry += holdNow(g) - g.lastHold;
            held++;
            top = za::max(top, topNow(g));
            low = za::min(low, topNow(g));
        }
    }
    if(held > 0)
    {
        carry /= static_cast<float>(held);
    }
    const float shoulderUp = ladderShoulder(ent, move).z - origin.z;
    const float speed = c.phase == Phase::Slipping ? pmove::hangSlipSink : za::clamp(vr_flat_ladder_speed.value, 20.f, 400.f);
    glm::vec3 target = origin + carry + glm::vec3{0.f, 0.f, static_cast<float>(dir) * speed * dt} - out * (40.f * dt);
    if(held > 0 && !ladderKeep(ent, c, target, dt, true)) // (a ladder volume: out of it, centred on it)
    {
        // (Drawn in to the rungs, the box's front no further than ladderInBox past them: a ladder standing free, a Quoth
        // or AD trigger_ladder over a non-solid ladder, has no wall to stop it.)
        glm::vec3 holds{0.f};
        for(const Grip& g : c.grips)
        {
            holds += g.active ? holdNow(g) : glm::vec3{0.f};
        }
        holds /= static_cast<float>(held);
        const float ahead = (target.x - holds.x) * out.x + (target.y - holds.y) * out.y;
        const float least = ent->v.maxs[0] - ladderInBox;
        if(ahead < least)
        {
            target += glm::vec3{out.x, out.y, 0.f} * (least - ahead);
        }
    }
    const float highest = top - ent->v.mins[2] + 2.f;
    const float lowest = low - armReach() - shoulderUp;
    target.z = za::clamp(target.z, za::min(lowest, origin.z + carry.z), za::max(highest, origin.z + carry.z));
    moveBody(ent, target);
    const glm::vec3 now = vec(ent->v.origin);
    setVec(ent->v.velocity, glm::vec3{0.f});
    ent->v.flags = static_cast<float>(static_cast<int>(ent->v.flags) & ~FL_ONGROUND);
    c.lastOrigin = now;
    SV_CheckWater(ent);
    for(int h = 0; h < 2; h++)
    {
        Grip& g = c.grips[h];
        g.lastHold = holdNow(g);
        g.lastRel = handRel(ent, move, h);
        c.flatShow[h] = g.active && handEmpty(ent, &move, h);
    }
    if(dir < 0)
    {
        // Down at the ground: off the ladder, standing.
        const trace_t below = tracePlayer(ent, now, now - glm::vec3{0.f, 0.f, 2.f});
        if(!below.startsolid && below.fraction < 1.f && below.plane.normal[2] >= 0.7f)
        {
            transition(ent, c, Phase::Free, Why::Forced);
            ent->v.flags = static_cast<float>(static_cast<int>(ent->v.flags) | FL_ONGROUND);
            if(debug())
            {
                Con_Printf("climb: flat ladder: down at the ground at (%.1f %.1f %.1f) (%s), lets go\n", now.x, now.y, now.z,
                    below.ent ? PR_GetString(below.ent->v.classname) : "?");
            }
            return 1;
        }
    }
    // Hand over hand: going up, the lower hand (once under the shoulders) to the rung over the other's; going down, the
    // higher (once an arm's half over them) to the rung under the other's.
    const glm::vec3 shoulder = ladderShoulder(ent, move);
    const int lo = !c.grips[0].active || (c.grips[1].active && topNow(c.grips[1]) < topNow(c.grips[0])) ? 1 : 0;
    const int hi = 1 - lo;
    bool atTop = false;
    if(dir > 0 && topNow(c.grips[lo]) < shoulder.z)
    {
        Rungs r;
        findRungs(ent, move.headPos, -out, topNow(c.grips[hi]) + 4.f, shoulder.z + armReach(), r);
        if(r.count > 0)
        {
            ladderTake(ent, c, move, lo, r.at[0]);
        }
        else
        {
            atTop = true;
        }
    }
    else if(dir < 0 && topNow(c.grips[hi]) > shoulder.z + 0.5f * armReach())
    {
        Rungs r;
        findRungs(ent, move.headPos, -out, now.z + ent->v.mins[2] + vr_climb_min_height.value, topNow(c.grips[lo]) - 4.f, r, false, ladderGap);
        if(r.count > 0)
        {
            ladderTake(ent, c, move, hi, r.at[r.count - 1]);
        }
    }
    if(dir < 0 && now.z <= lowest + 0.5f)
    {
        // At the bottom of its rungs over a drop (none under the lowest hold in reach): let go, and fall.
        transition(ent, c, Phase::Free, Why::Forced);
        if(debug())
        {
            Con_Printf("climb: flat ladder: no rung under, lets go at (%.1f %.1f %.1f)\n", now.x, now.y, now.z);
        }
        return 1;
    }
    // The top: the headset's mantle, the head vr_climb_mantle_height over the top hold, with room on it.
    const glm::vec3 eye = move.headPos - move.origin + now;
    const int topHand = topNow(c.grips[1]) >= topNow(c.grips[0]) || !c.grips[0].active ? 1 : 0;
    const float headAbove = eye.z - topNow(c.grips[topHand]);
    if(dir > 0 && (atTop || now.z >= highest - 0.5f) && headAbove >= za::clamp(vr_climb_mantle_height.value, 0.f, 40.f) &&
        !(c.flatLadderTry >= 0.0 && time >= c.flatLadderTry && time - c.flatLadderTry < pmove::mantleRetry))
    {
        c.flatLadderTry = time;
        Grip g = c.grips[topHand];
        glm::vec3 mid, to;
        if(findMantle(ent, g, mid, to))
        {
            startMantle(ent, c, g, vec(ent->v.origin), mid, to, headAbove);
            flatMantleHands(ent, c, &move, g);
            return 1;
        }
        if(vr_climb_debug.value >= 2.f)
        {
            Con_Printf("climb: flat ladder: no room over the top at %.1f\n", topNow(g));
        }
    }
    hangStamina(ent, c, time);
    if(c.draining && fields().vr_climb_drain >= 0)
    {
        fieldFloat(ent, fields().vr_climb_drain) = 1.f;
    }
    return 1;
}

// Flatscreen play's ropes (vr_flat_rope; the climbable ropes and chains, vr_climbrope.cpp, docs/vr-port/ROPES.md "Climbing
// (flat-screen)"). A headset's hands take a rope hand over hand; here walking forward into one, or meeting one in the
// air (a jump, a fall, a swing off another rope), takes it with both hands, the hang the headset's rope hang (its
// rider: his weight swings it, the holds on its line, the QC's climbing bits, the drawn hands, the feet round it, the
// stamina), only the body is moved by the keys as on the flat ladder (flatLadderFrame): looking up or down past
// vr_flat_rope_climb_pitch, forward climbs up (back down; the move up / down keys too) at vr_flat_ladder_speed, the hands
// going hand over hand along it; looking nearer level forward and back pump its swing, the strafe keys always do
// (reportRopes; flatRopeKeys, vr_flat_rope_keys); jump lets go keeping the swing (ropeJump). The body hangs with the rope flatRopeGap ahead of its
// middle, the view's way (turning goes round the rope).
constexpr float flatRopeGap = 10.f;    // units: the rope this far ahead of the box's middle (the chest's front)
constexpr float flatRopeStep = 12.f;   // units along the rope: a hand's new hold past the other's
constexpr float flatRopeTurn = 120.f;  // units/s the body goes round the rope (turning) or in under it at most

// Whether the rope let go of (flatRopeLeft) is still not taken again: for vr_flat_rope_retake seconds (off one, the next
// one is taken). (ropes11, his note 2026-11-23: he wants to grab the same pole again after jumping off; ropes10 kept it
// until he stood on the ground, for a slow slide down a pole that was its boxes holding him, vr_pole_fall_test)
[[nodiscard]] bool flatRopeRetake(const Climber& c, int rope, double time)
{
    return rope == c.flatRopeLeft && time - c.flatRopeLeftAt < za::clamp(vr_flat_rope_retake.value, 0.f, 2.f);
}

constexpr float flatRopeBlend = 5.f; // (ropes9) degrees either side of vr_flat_rope_climb_pitch: the pump eases into the climb

// How the keys work rope entity `rope` (vr_flat_rope_keys): `climb` -1..1 (up +, a share of vr_flat_ladder_speed), and
// how much of forward / back (`fwdPump`) and of the strafe keys (`sidePump`) pump its swing (reportRopes).
// 0 (ropes9, his note 2026-11-23): looking up or down past vr_flat_rope_climb_pitch forward climbs up, back down; nearer
//   level forward / back pump the swing along the view (eased over flatRopeBlend each way); the strafe keys always pump;
//   a pole (rigid) is only climbed. 1 (before ropes8a): looking up past vr_flat_rope_look forward climbs up, looking
//   down down (back the other way), nearer level the move keys pump. 2 (ropes8a): forward up, back down, whatever the
//   pitch; the strafe keys pump. The move up / down keys climb in every mode.
FlatRopeKeys flatRopeKeys(edict_t* ent, const VrMove* move, int rope)
{
    FlatRopeKeys k;
    const int client = NUM_FOR_EDICT(ent) - 1;
    if(client < 0 || client >= svs.maxclients)
    {
        return k;
    }
    const usercmd_t& cmd = svs.clients[client].cmd;
    const int mode = static_cast<int>(vr_flat_rope_keys.value);
    if(za::abs(cmd.upmove) > 1.f)
    {
        k.climb = cmd.upmove > 0.f ? 1.f : -1.f;
        k.sidePump = mode == 1 ? 0.f : 1.f;
        return k;
    }
    const float f = cmd.forwardmove > 1.f ? 1.f : cmd.forwardmove < -1.f ? -1.f : 0.f;
    const float pitch = move ? move->headAngles.x : 0.f; // (pitch: down is +)
    if(mode == 1)
    {
        if(!move || f == 0.f || za::abs(pitch) < za::clamp(vr_flat_rope_look.value, 5.f, 80.f))
        {
            k.fwdPump = k.sidePump = 1.f;
            return k;
        }
        k.climb = pitch < 0.f ? f : -f;
        return k;
    }
    k.sidePump = 1.f;
    if(mode >= 2 || climbrope::pole(rope))
    {
        k.climb = f;
        return k;
    }
    const float at = za::clamp(vr_flat_rope_climb_pitch.value, flatRopeBlend, 90.f - flatRopeBlend);
    const float t = move ? za::clamp((za::abs(pitch) - (at - flatRopeBlend)) / (2.f * flatRopeBlend), 0.f, 1.f) : 0.f;
    const float w = t * t * (3.f - 2.f * t);
    k.climb = f * w;
    k.fwdPump = 1.f - w;
    return k;
}

// Hand `h` takes rope hold `grab`: a grip on it as a headset's (ropeHold's ledge), drawn there if the hand is empty.
void ropeTake(edict_t* ent, Climber& c, const VrMove& move, int h, const climbrope::Grab& grab)
{
    Grip& g = c.grips[h];
    const bool was = g.active;
    g.active = true;
    g.owned = false;
    g.serial++;
    Ledge l;
    l.rope = true;
    l.ent = EDICT_NUM(grab.ent);
    l.ropeS = grab.s;
    l.ropeD = grab.d;
    l.hold = grab.at;
    l.top = grab.at.z;
    const glm::vec3 toBody{ent->v.origin[0] - grab.at.x, ent->v.origin[1] - grab.at.y, 0.f};
    l.out = glm::length(toBody) > 1e-3f ? glm::normalize(toBody) : glm::vec3{1.f, 0.f, 0.f};
    (void)holdLedge(g, l);
    g.allowed = armReach();
    g.relAtGrab = g.lastRel = handRel(ent, move, h);
    g.grabOffset = glm::vec3{0.f};
    g.grabVel = was ? g.grabVel : vec(ent->v.velocity); // (the swing he brings to it: the rider's first)
    c.flatShow[h] = handEmpty(ent, &move, h);
    if(c.flatShow[h])
    {
        climbrope::grabbed(ent, grab.ent, g.hold); // (a chain's metal grip, a rope's soft thud)
    }
    if(debug())
    {
        Con_Printf("climb: flat rope: %s hand takes rope %d %.1f along (at %.1f, feet at %.1f)\n", handName(h), grab.ent, grab.s,
            grab.at.z, ent->v.origin[2] + ent->v.mins[2]);
    }
}

[[nodiscard]] bool flatMonkeyTake(edict_t* ent, Climber& c, const VrMove& move, double time, const glm::vec3& origin,
    const glm::vec3& shoulder, const glm::vec3& fwd, float reach); // (ropes7, below)

// Free: walking forward into a rope (on the ground, not looking down past vr_flat_rope_look), or meeting one in the air,
// takes it: the main hand half an arm over the shoulders, the off hand flatRopeStep under it.
void flatRope(edict_t* ent, Climber& c, const VrMove* move, double time)
{
    if(!move || vr_flat_rope.value == 0.f || vr_rope_climb.value == 0.f || !hull::isFlat(ent) || ent->v.waterlevel >= 2.f ||
        time < c.noGrabUntil)
    {
        return;
    }
    const int client = NUM_FOR_EDICT(ent) - 1;
    if(client < 0 || client >= svs.maxclients)
    {
        return;
    }
    const bool ground = (static_cast<int>(ent->v.flags) & FL_ONGROUND) != 0;
    const usercmd_t& cmd = svs.clients[client].cmd;
    if(ground && (cmd.forwardmove <= 1.f || move->headAngles.x > za::clamp(vr_flat_rope_look.value, 5.f, 80.f)))
    {
        return;
    }
    const glm::vec3 origin = vec(ent->v.origin);
    const glm::vec3 shoulder = ladderShoulder(ent, *move);
    const float yaw = glm::radians(move->headAngles.y);
    const glm::vec3 fwd{za::cos(yaw), za::sin(yaw), 0.f};
    const float reach = ent->v.maxs[0] + za::clamp(vr_flat_ladder_reach.value, 4.f, 48.f);
    if(!ground && flatMonkeyTake(ent, c, *move, time, origin, shoulder, fwd, reach))
    {
        return; // (ropes7: a monkey traverse's rope overhead, met in the air)
    }
    climbrope::Grab grab;
    if(!climbrope::findGrab(glm::vec3{origin.x, origin.y, shoulder.z}, reach, grab) || climbrope::monkey(grab.ent) ||
        flatRopeRetake(c, grab.ent, time) ||
        grab.at.z < origin.z + ent->v.mins[2] + vr_climb_min_height.value || grab.at.z > shoulder.z + armReach())
    {
        return;
    }
    const glm::vec3 to{grab.at.x - origin.x, grab.at.y - origin.y, 0.f};
    if(ground && glm::length(to) > 1.f && glm::dot(glm::normalize(to), fwd) < 0.5f)
    {
        return; // (walking: the rope ahead)
    }
    // (Along the rope from the point met: up to half an arm over the shoulders, hanging about straight.)
    climbrope::Grab hi, lo;
    const float up = za::max(0.f, shoulder.z + 0.5f * armReach() - grab.at.z);
    if(!climbrope::grabAlong(grab.ent, client, grab.s - up, hi) || !climbrope::grabAlong(grab.ent, client, hi.s + flatRopeStep, lo))
    {
        return;
    }
    ropeTake(ent, c, *move, 1, hi);
    ropeTake(ent, c, *move, 0, lo);
    c.flatRope = true;
    c.flatRopes++;
    c.owed = glm::vec3{0.f};
    c.noRoom = false;
    c.lastOrigin = origin;
    transition(ent, c, Phase::Hanging, Why::Took);
    if(debug())
    {
        Con_Printf("climb: flat rope: takes rope %d at (%.1f %.1f %.1f), %s\n", grab.ent, origin.x, origin.y, origin.z,
            ground ? "walked into" : "in the air");
    }
}

// (ropes7) Flat play's monkey traverse (a rope strung level or slanted past vr_rope_monkey_angle, climbrope::monkey):
// met in the air overhead (a jump up to it), taken with both hands flatRopeStep apart along it, the main hand the view's
// way. The body hangs under the hands, the shoulders flatMonkeyHang of an arm under them; the move keys (their part
// along the rope) go along it at vr_flat_ladder_speed, hand over hand (the trailing hand, once the body is half a step
// past the hands' middle, flatRopeStep past the leading one; at its end the body stops); the move-down key lets go (a
// drop), jump jumps off (ropeJump). No pump: the keys go along it.
constexpr float flatMonkeyHang = 0.8f;
// (ropes10, his flat note 2026-11-23: facing across a level pole, moving sideways crossed the forearms in an X over the
// head) Facing across the rope (the view's way within this cosine of square to it, about 37 degrees off along), the hands
// keep their sides: the right hand on his right of the left. Going along it they shuffle (the leading hand, on the way's
// side, a step on; then the trailing one up to a step behind it), never hand over hand past each other; hands found
// crossed (taken facing along it, then turned) change holds.
constexpr float flatMonkeyAcrossCos = 0.8f;

[[nodiscard]] bool flatMonkeyAcross(const glm::vec3& fwd, glm::vec3 along)
{
    along.z = 0.f;
    return glm::length(along) > 1e-3f && za::abs(glm::dot(glm::normalize(along), fwd)) < flatMonkeyAcrossCos;
}

bool flatMonkeyTake(edict_t* ent, Climber& c, const VrMove& move, double time, const glm::vec3& origin, const glm::vec3& shoulder,
    const glm::vec3& fwd, float reach)
{
    const int client = NUM_FOR_EDICT(ent) - 1;
    climbrope::Grab grab;
    if(!climbrope::findGrab(glm::vec3{origin.x, origin.y, shoulder.z + 0.5f * armReach()}, reach, grab) || !climbrope::monkey(grab.ent) ||
        flatRopeRetake(c, grab.ent, time) || grab.at.z < shoulder.z ||
        grab.at.z > shoulder.z + armReach())
    {
        return false;
    }
    climbrope::Grab a, b;
    if(!climbrope::grabAlong(grab.ent, client, grab.s - 0.5f * flatRopeStep, a) ||
        !climbrope::grabAlong(grab.ent, client, a.s + flatRopeStep, b))
    {
        return false;
    }
    // (ropes10) Facing across it, the right hand takes the hold on his right (flatMonkeyAcross: the hands keep their sides).
    const glm::vec3 right{fwd.y, -fwd.x, 0.f};
    const bool across = flatMonkeyAcross(fwd, b.at - a.at);
    const bool bAhead = across ? glm::dot(b.at - a.at, right) >= 0.f : glm::dot(b.at - a.at, fwd) >= 0.f;
    ropeTake(ent, c, move, 1, bAhead ? b : a);
    ropeTake(ent, c, move, 0, bAhead ? a : b);
    c.flatRope = true;
    c.flatRopes++;
    c.owed = glm::vec3{0.f};
    c.noRoom = false;
    c.lastOrigin = origin;
    transition(ent, c, Phase::Hanging, Why::Took);
    if(debug())
    {
        Con_Printf("climb: flat rope: takes rope %d overhead at (%.1f %.1f %.1f), a monkey traverse\n", grab.ent, origin.x,
            origin.y, origin.z);
    }
    return true;
}

int flatMonkeyFrame(edict_t* ent, Climber& c, const VrMove& move, double time, int hi, int lo, int held)
{
    const int client = NUM_FOR_EDICT(ent) - 1;
    const usercmd_t& cmd = svs.clients[client].cmd;
    const bool held2[2] = {c.grips[0].active, c.grips[1].active};
    if(cmd.upmove < -1.f)
    {
        // The move-down key: let go, a drop.
        c.grips[0].active = c.grips[1].active = false;
        transition(ent, c, Phase::Free, Why::Released, nullptr, held2);
        if(debug())
        {
            Con_Printf("climb: flat rope: drops from the monkey traverse at (%.1f %.1f %.1f)\n", ent->v.origin[0],
                ent->v.origin[1], ent->v.origin[2]);
        }
        return 1;
    }
    const int ropeEnt = c.grips[hi].entNum;
    const float dt = static_cast<float>(host_frametime);
    const glm::vec3 origin = vec(ent->v.origin);
    const float yaw = glm::radians(move.headAngles.y);
    const glm::vec3 fwd{za::cos(yaw), za::sin(yaw), 0.f}, right{za::sin(yaw), -za::cos(yaw), 0.f};
    const glm::vec3 a = holdNow(c.grips[hi]), b = holdNow(c.grips[lo]);
    // The rope's way here, level, towards its bottom (along it: further from its top).
    glm::vec3 along = b - a;
    climbrope::Grab g0, g1;
    if((held < 2 || glm::length(glm::vec2{along.x, along.y}) < 1.f) && climbrope::grabAlong(ropeEnt, client, c.grips[hi].ropeS - 8.f, g0) &&
        climbrope::grabAlong(ropeEnt, client, c.grips[hi].ropeS + 8.f, g1))
    {
        along = g1.at - g0.at;
    }
    along.z = 0.f;
    along = glm::length(along) > 1e-3f ? glm::normalize(along) : fwd;
    const glm::vec3 wish = fwd * cmd.forwardmove + right * cmd.sidemove;
    const float w = glm::dot(wish, along);
    const int dir = glm::length(wish) > 1.f && za::abs(w) > 0.3f * glm::length(wish) ? (w > 0.f ? 1 : -1) : 0;
    const glm::vec3 mid = held == 2 ? (a + b) * 0.5f : a;
    const float speed = za::clamp(vr_flat_ladder_speed.value, 20.f, 400.f);
    const float shoulderUp = ladderShoulder(ent, move).z - origin.z;
    // Along: by the keys, within a step of the hands' middle; across: in under the rope at flatRopeTurn at most.
    glm::vec3 off = origin + along * (static_cast<float>(dir) * speed * dt) - mid;
    off.z = 0.f;
    const float u = za::clamp(glm::dot(off, along), -flatRopeStep, flatRopeStep);
    glm::vec3 perp = off - along * glm::dot(off, along);
    if(const float p = glm::length(perp); p > 1e-3f)
    {
        perp -= perp * za::min(1.f, flatRopeTurn * dt / p);
    }
    glm::vec3 target = mid + along * u + perp;
    const float hangZ = mid.z - shoulderUp - flatMonkeyHang * armReach();
    target.z = origin.z + za::clamp(hangZ - origin.z, -flatRopeTurn * dt, flatRopeTurn * dt); // (into the hang, eased)
    moveBody(ent, target);
    const glm::vec3 now = vec(ent->v.origin);
    climbrope::riderBlocked(client, glm::vec3{target.x - now.x, target.y - now.y, 0.f});
    setVec(ent->v.velocity, glm::vec3{0.f});
    ent->v.flags = static_cast<float>(static_cast<int>(ent->v.flags) & ~FL_ONGROUND);
    c.lastOrigin = now;
    SV_CheckWater(ent);
    for(int h = 0; h < 2; h++)
    {
        Grip& g = c.grips[h];
        g.lastHold = holdNow(g);
        g.lastRel = handRel(ent, move, h);
        c.flatShow[h] = g.active && handEmpty(ent, &move, h);
    }
    // Hand over hand: the trailing hand once the body is half a step past the hands' middle, a step past the leading one.
    climbrope::Grab next;
    const float past = glm::dot(now - mid, along) * static_cast<float>(dir);
    const bool across = held == 2 && flatMonkeyAcross(fwd, along);
    // (ropes10) Facing across it: `toRight` the rope's way (further along it) to his right (+1) or left (-1).
    const float toRight = glm::dot(along, right) >= 0.f ? 1.f : -1.f;
    climbrope::Grab ga, gb;
    if(across && (c.grips[1].ropeS - c.grips[0].ropeS) * toRight < -1.f && climbrope::grabAlong(ropeEnt, client, c.grips[0].ropeS, ga) &&
        climbrope::grabAlong(ropeEnt, client, c.grips[1].ropeS, gb))
    {
        ropeTake(ent, c, move, 0, gb); // (crossed: each takes the other's hold)
        ropeTake(ent, c, move, 1, ga);
    }
    else if(across && dir != 0 && past > 0.5f * flatRopeStep)
    {
        const int lead = static_cast<float>(dir) * toRight > 0.f ? 1 : 0, trail = 1 - lead;
        const float gap = (c.grips[lead].ropeS - c.grips[trail].ropeS) * static_cast<float>(dir);
        const float to = gap <= 1.01f * flatRopeStep ? za::max(c.grips[lead].ropeS * static_cast<float>(dir), c.grips[trail].ropeS * static_cast<float>(dir)) + flatRopeStep
                                                     : c.grips[lead].ropeS * static_cast<float>(dir) - flatRopeStep;
        const int who = gap <= 1.01f * flatRopeStep ? lead : trail;
        if(climbrope::grabAlong(ropeEnt, client, to * static_cast<float>(dir), next) && za::abs(next.s - c.grips[who].ropeS) > 1.f)
        {
            ropeTake(ent, c, move, who, next);
        }
    }
    else if(held == 2 && dir != 0 && past > 0.5f * flatRopeStep)
    {
        const int lead = dir > 0 ? lo : hi, trail = dir > 0 ? hi : lo;
        const float leadS = c.grips[lead].ropeS;
        if(climbrope::grabAlong(ropeEnt, client, leadS + static_cast<float>(dir) * flatRopeStep, next) && za::abs(next.s - leadS) > 1.f)
        {
            ropeTake(ent, c, move, trail, next);
        }
    }
    else if(held == 1 && climbrope::grabAlong(ropeEnt, client, c.grips[hi].ropeS + (dir < 0 ? -flatRopeStep : flatRopeStep), next))
    {
        ropeTake(ent, c, move, 1 - hi, next); // (one hand on it: the other takes it a step on)
    }
    hangStamina(ent, c, time);
    if(c.draining && fields().vr_climb_drain >= 0)
    {
        fieldFloat(ent, fields().vr_climb_drain) = 1.f;
    }
    return 1;
}

// A flat rope's frame (hangFrame's): the body under the hands, the rope ahead of it (going round it as he turns), carried
// by the holds' motion (the swing), moved up or down by the keys (the shoulders no higher than the top hand, no lower
// than an arm under the lowest); hand over hand along it; the ground ends it going down, as the rope's end does (let go,
// the swing kept); the stamina as any hang's.
int flatRopeFrame(edict_t* ent, Climber& c, const VrMove& move, double time)
{
    if(vr_flat_rope.value == 0.f || vr_rope_climb.value == 0.f || !hull::isFlat(ent))
    {
        transition(ent, c, Phase::Free, Why::Forced);
        return 0;
    }
    const int client = NUM_FOR_EDICT(ent) - 1;
    int held = 0, hi = -1, lo = -1;
    glm::vec3 carry{0.f};
    for(int h = 0; h < 2; h++)
    {
        const Grip& g = c.grips[h];
        if(g.active && g.rope)
        {
            carry += holdNow(g) - g.lastHold;
            held++;
            hi = hi < 0 || g.ropeS < c.grips[hi].ropeS ? h : hi;
            lo = lo < 0 || g.ropeS >= c.grips[lo].ropeS ? h : lo;
        }
    }
    if(held == 0)
    {
        transition(ent, c, Phase::Free, Why::Forced);
        return 0;
    }
    carry /= static_cast<float>(held);
    if(climbrope::monkey(c.grips[hi].entNum))
    {
        return flatMonkeyFrame(ent, c, move, time, hi, lo, held);
    }
    const float climbKeys = c.phase == Phase::Slipping ? -1.f : flatRopeKeys(ent, &move, c.grips[hi].entNum).climb;
    const int dir = climbKeys > 0.01f ? 1 : climbKeys < -0.01f ? -1 : 0;
    const float dt = static_cast<float>(host_frametime);
    const glm::vec3 origin = vec(ent->v.origin);
    const float shoulderUp = ladderShoulder(ent, move).z - origin.z;
    const float speed = c.phase == Phase::Slipping ? pmove::hangSlipSink : za::clamp(vr_flat_ladder_speed.value, 20.f, 400.f);
    const float yaw = glm::radians(move.headAngles.y);
    const glm::vec3 fwd{za::cos(yaw), za::sin(yaw), 0.f};
    const glm::vec3 low = holdNow(c.grips[lo]), high = holdNow(c.grips[hi]);
    glm::vec3 target = origin + carry + glm::vec3{0.f, 0.f, (dir != 0 ? climbKeys : 0.f) * speed * dt};
    // (Level: to its place behind the rope, at most flatRopeTurn a second: turning goes round it.)
    const glm::vec3 place{low.x - fwd.x * flatRopeGap, low.y - fwd.y * flatRopeGap, target.z};
    if(const glm::vec3 d = place - target; glm::length(d) > 1e-3f)
    {
        target += d * za::min(1.f, flatRopeTurn * dt / glm::length(d));
    }
    const float highest = high.z - shoulderUp + 2.f;
    const float lowest = low.z - armReach() - shoulderUp;
    target.z = za::clamp(target.z, za::min(lowest, origin.z + carry.z), za::max(highest, origin.z + carry.z));
    moveBody(ent, target);
    const glm::vec3 now = vec(ent->v.origin);
    // (The body couldn't go where the swing took it, level: a wall; the swing stopped that way, as the headset's hang.)
    climbrope::riderBlocked(client, glm::vec3{target.x - now.x, target.y - now.y, 0.f});
    setVec(ent->v.velocity, glm::vec3{0.f});
    ent->v.flags = static_cast<float>(static_cast<int>(ent->v.flags) & ~FL_ONGROUND);
    c.lastOrigin = now;
    SV_CheckWater(ent);
    for(int h = 0; h < 2; h++)
    {
        Grip& g = c.grips[h];
        g.lastHold = holdNow(g);
        g.lastRel = handRel(ent, move, h);
        c.flatShow[h] = g.active && handEmpty(ent, &move, h);
    }
    const bool held2[2] = {c.grips[0].active, c.grips[1].active};
    if(dir < 0)
    {
        // Down at the ground: off the rope, standing.
        const trace_t below = tracePlayer(ent, now, now - glm::vec3{0.f, 0.f, 2.f});
        if(!below.startsolid && below.fraction < 1.f && below.plane.normal[2] >= 0.7f)
        {
            transition(ent, c, Phase::Free, Why::Forced);
            ent->v.flags = static_cast<float>(static_cast<int>(ent->v.flags) | FL_ONGROUND);
            if(debug())
            {
                Con_Printf("climb: flat rope: down at the ground at (%.1f %.1f %.1f), lets go\n", now.x, now.y, now.z);
            }
            return 1;
        }
    }
    // Hand over hand: going up, the lower hand (once under the shoulders) flatRopeStep over the other's; going down, the
    // higher (once half an arm over them) flatRopeStep under the other's. (Not past the rope's top or its free end.)
    const glm::vec3 shoulder = ladderShoulder(ent, move);
    const int ropeEnt = c.grips[hi].entNum;
    climbrope::Grab next;
    if(held == 2 && dir > 0 && holdNow(c.grips[lo]).z < shoulder.z &&
        climbrope::grabAlong(ropeEnt, client, c.grips[hi].ropeS - flatRopeStep, next) && next.s < c.grips[hi].ropeS - 1.f)
    {
        ropeTake(ent, c, move, lo, next);
    }
    else if(held == 2 && dir < 0 && holdNow(c.grips[hi]).z > shoulder.z + 0.5f * armReach() &&
            climbrope::grabAlong(ropeEnt, client, c.grips[lo].ropeS + flatRopeStep, next) && next.s > c.grips[lo].ropeS + 1.f)
    {
        ropeTake(ent, c, move, hi, next);
    }
    else if(held == 1 && climbrope::grabAlong(ropeEnt, client, c.grips[hi].ropeS + flatRopeStep, next))
    {
        ropeTake(ent, c, move, 1 - hi, next); // (one hand on it: the other takes it under)
    }
    if(dir < 0 && now.z <= lowest + 0.5f)
    {
        // At the rope's end over a drop (no more of it under the lowest hand): let go, the swing kept.
        c.grips[0].active = c.grips[1].active = false;
        transition(ent, c, Phase::Free, Why::Released, nullptr, held2);
        if(debug())
        {
            Con_Printf("climb: flat rope: at its end, lets go at (%.1f %.1f %.1f)\n", now.x, now.y, now.z);
        }
        return 1;
    }
    hangStamina(ent, c, time);
    if(c.draining && fields().vr_climb_drain >= 0)
    {
        fieldFloat(ent, fields().vr_climb_drain) = 1.f;
    }
    return 1;
}
} // namespace

namespace
{
constexpr float pullGruntRise = 0.15f; // m: the body risen this far over a new rope grip's hold since taking it: a pull
constexpr double pullGruntGap = 0.6;   // seconds between two pull grunts at least

// (ropes6, NOTES.md vrropes_2026-11-22_15-43-56) A short exertion grunt as a headset's pull lifts him up a rope: once a
// grip, when the body has risen pullGruntRise over that grip's hold since the hand took it (pulled down on it; a mere
// grab, or the rope's swing, which carries hold and body alike, lifts nothing), at vr_rope_grunt's volume (one of the
// mantle's grunts: vr_rope_grunt_sound), at most every pullGruntGap. Flat-screen play's keys climb steadily, with no
// pulls: none there.
void ropePullGrunt(edict_t* ent, Climber& c)
{
    for(int h = 0; h < 2; h++)
    {
        const Grip& g = c.grips[h];
        const int serial = g.active && g.rope ? g.serial : -1;
        if(serial >= 0 && serial != c.pullSeen[h])
        {
            c.pullHand = h; // (a new grip: its pull to come)
            c.pullFrom = ent->v.origin[2] - g.hold.z;
            c.pullGrunted = c.flatRope;
        }
        c.pullSeen[h] = serial;
    }
    if(c.pullHand < 0 || c.pullGrunted || c.pullSeen[c.pullHand] < 0)
    {
        return;
    }
    const float rise = ent->v.origin[2] - c.grips[c.pullHand].hold.z - c.pullFrom;
    if(rise < pullGruntRise * units::metresToUnits())
    {
        return;
    }
    c.pullGrunted = true;
    if(qcvm->time >= c.pullGruntNext && climb::grunt(ent, vr_rope_grunt_sound.value, vr_rope_grunt.value))
    {
        c.pullGruntNext = qcvm->time + pullGruntGap;
        c.pullGrunts++;
        if(debug())
        {
            Con_Printf("climb: rope pull grunt (%s hand, up %.1f) %s %.2f\n", handName(c.pullHand), rise,
                climb::gruntSample(vr_rope_grunt_sound.value), za::clamp(vr_rope_grunt.value, 0.f, 1.f));
        }
    }
}

// A hanging (or slipping) frame: the body moved by the holding hands, the mantle's test (the steps'); out of reach, the
// fall; the stamina. The hang's once-a-frame part after its steps (single player's one, a remote client's moves': MP
// B8i), unless they began the mantle: kept within reach, the owed settled, the reach checked, the no-room buzz, the
// stamina.
void hangFramePart(Hang& hang)
{
    edict_t* const ent = hang.ent;
    Climber& c = hang.c;
    keepWithinReach(hang);
    settleOwed(hang);
    reachCheck(hang);
    // A rope's rider whose body couldn't go where the swing (and the pull) took it, level (a wall): his swing stopped
    // that way, the rope pushed back (see "Holds on ropes").
    climbrope::riderBlocked(NUM_FOR_EDICT(ent) - 1, glm::vec3{hang.meant.x - hang.done.x, hang.meant.y - hang.done.y, 0.f});
    ropePullGrunt(ent, c);
    if(!c.hanging())
    {
        // Let go of everything (out of reach): fall.
        transition(ent, c, Phase::Free, Why::OutOfReach);
        return;
    }

    noRoomBuzz(ent, c, hang.noRoom);

    // (A ladder volume's: the body eased out of the ladder, ladderKeep.)
    const glm::vec3 at = vec(ent->v.origin);
    glm::vec3 kept = at;
    if(ladderKeep(ent, c, kept, static_cast<float>(host_frametime), false) && kept != at)
    {
        moveBody(ent, kept);
    }

    hangStamina(ent, c, hang.time);
    if(c.draining && fields().vr_climb_drain >= 0)
    {
        fieldFloat(ent, fields().vr_climb_drain) = 1.f;
    }
}

// Every hold on the world: still between frames (a hold on a brush model or prop moves with it: the frame's hang its
// own, not a remote client's moves').
[[nodiscard]] bool holdsStill(const Climber& c)
{
    for(const Grip& g : c.grips)
    {
        if(g.active && (g.prop || g.rope || g.entNum > 0))
        {
            return false;
        }
    }
    return true;
}

int hangFrame(edict_t* ent, Climber& c, double time)
{
    server::rebaseHands(ent);
    const VrMove* move = server::clientMove(ent);
    if(!move)
    {
        transition(ent, c, Phase::Free, Why::NoMove);
        return 0;
    }
    if(c.flatLadder)
    {
        return flatLadderFrame(ent, c, *move, time); // (flatscreen play's ladder: the keys move the body)
    }
    if(c.flatRope)
    {
        return flatRopeFrame(ent, c, *move, time); // (and its rope)
    }

    // The pull: what the holding hands moved relative to the body since the last frame, the other way, each weighted
    // by how far it moved; and the holds' own motion (a moving brush model carries the body).
    Hang hang{ent, c, move, time, {handRel(ent, *move, 0), handRel(ent, *move, 1)}};
    if(!hangSteps(hang))
    {
        hangFramePart(hang);
    }
    return 1;
}
} // namespace

extern "C" int VR_ClientClimb(edict_t* ent)
{
    Climber* cp = climberOf(ent);
    if(!cp || cp->phase == Phase::Free)
    {
        return 0;
    }
    Climber& c = *cp;
    if((c.phase == Phase::Mantling || (c.hanging() && c.phase != Phase::Free && holdsStill(c))) && VR_PredictWalks(ent))
    {
        return 0; // (a remote client's moves run the mantle and the hang, one by one: the walk's think, then
                  // VR_PredictPhysics; MP B8h, B8i)
    }
    if(!SV_RunThink(ent))
    {
        return -1;
    }
    const double time = qcvm->time;
    switch(c.phase)
    {
    case Phase::Free:
        break;
    case Phase::Hanging:
    case Phase::Slipping:
        return hangFrame(ent, c, time);
    case Phase::Mantling:
        mantleStep(ent, c);
        return 1;
    }
    return 0;
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
           (c->phase == Phase::Mantling && c->mantleEnt == num);
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
    // (Off it: no hand holds any more, or the mantle was onto it.)
    const bool off = c.phase == Phase::Mantling ? c.mantleEnt == num : !c.anyHeld();
    if(debug())
    {
        Con_Printf("climb: carried into something at (%.1f %.1f %.1f): lets go\n", check->v.origin[0], check->v.origin[1],
            check->v.origin[2]);
    }
    c.lastOrigin = vec(check->v.origin);
    if(off)
    {
        transition(check, c, Phase::Free, Why::CarriedOff);
    }
    return 0;
}

// SV_PushMove, when `pusher` has moved by `move` with what rides it: the holds on it moved with it, and the bodies
// hanging from it with them (VR_ClientClimb's carry is only what the holds moved otherwise: a model moved by the QC).
extern "C" void VR_ClimbCarried(edict_t* pusher, const float* move)
{
    const int num = NUM_FOR_EDICT(pusher);
    const int clients = za::min(svs.maxclients, static_cast<int>(MAX_SCOREBOARD));
    for(int i = 0; i < clients; i++)
    {
        Climber& c = climbers[i];
        bool rides = c.phase == Phase::Mantling && c.mantleEnt == num;
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
    const glm::vec3 fwd{za::cos(glm::radians(yaw)), za::sin(glm::radians(yaw)), 0.f};
    const glm::vec3 a = origin + fwd * 16.f, b = origin + fwd * 64.f;
    const glm::vec3 mins{za::min(a.x, b.x) - 24.f, za::min(a.y, b.y) - 24.f, feet + 16.f};
    const glm::vec3 maxs{za::max(a.x, b.x) + 24.f, za::max(a.y, b.y) + 24.f, feet + 128.f};
    int found = 0;
    const auto list = [&](const ledges::Map& m, const glm::vec3& offset, int entNum) {
        za::Vector<int> nearby;
        m.nearby(mins - offset, maxs - offset, nearby);
        for(const int i : nearby)
        {
            const ledges::Edge& e = m.edges[static_cast<za::SizeT>(i)];
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
        // A prop's, as it lies now (see "Holds on props").
        box3d::ClimbBody body;
        const box3d::ClimbShape* shape = vr_climb_props.value != 0.f && !e->free ? box3d::climbShape(i, body) : nullptr;
        if(shape)
        {
            ledges::Map props;
            za::Vector<int> nums;
            ledges::propLedges(i, *shape, body, mins, maxs, props, nums);
            props.finish();
            list(props, glm::vec3{0.f}, i);
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
    if(Cmd_Argc() < 4 || !progs::localPlayer())
    {
        Con_Printf("vr_climb_try <x> <y> <z> [off|main] [<vx> <vy> <vz>]\n");
        return;
    }
    progs::ServerVm serverVm;
    edict_t* ent = progs::localPlayer();
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
    const Climber* climber = climberOf(ent);
    const bool airborne = midAir(ent, climber && climber->hanging());
    const za::Optional<Ledge> l = findHold(seekerOf(ent), hand, vel, head, leniencyUnits(airborne),
        propLowest(ent, climber && climber->hanging()), lenient, &st);
    const double ms = (Sys_DoubleTime() - t0) * 1000.0;
    if(l)
    {
        Con_Printf("climbtry %.2f %.2f %.2f %s: %s hold %.2f %.2f %.2f top %.1f out %.2f %.2f, %.2f units from the hand (%.1f cm)",
            hand.x, hand.y, hand.z, handName(h), lenient ? "lenient" : "exact", l->hold.x, l->hold.y, l->hold.z, l->top,
            l->out.x, l->out.y, glm::distance(l->hold, hand), glm::distance(l->hold, hand) / units::metresToUnits() * 100.f);
        if(airborne)
        {
            Con_Printf(" (mid-air +%.1f cm)", vr_climb_leniency_air.value);
        }
    }
    else
    {
        Con_Printf("climbtry %.2f %.2f %.2f %s: none (leniency %.1f cm%s)", hand.x, hand.y, hand.z, handName(h),
            vr_climb_leniency.value, airborne ? va(", mid-air +%.1f", vr_climb_leniency_air.value) : "");
    }
    Con_Printf("; %d points, %d holds seen, turned down: %d low, %d below, %d far, %d behind, %d through a wall, %d covered, "
               "%d hidden; %.4f ms, %d traces\n",
        st.points, st.found, st.low, st.below, st.tooFar, st.behind, st.unseen, st.covered, st.hidden, ms, traceCount - traces0);
}

void probe_f()
{
    if(!progs::localPlayer())
    {
        return;
    }
    progs::ServerVm serverVm;
    edict_t* ent = progs::localPlayer();
    probe(ent, Cmd_Argc() > 1 ? Q_atof(Cmd_Argv(1)) : ent->v.angles[1]);
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
    glm::vec3 fromUp{0.f}, toUp{0.f}; // on a rope: its way up at the hold, as drawn (0: a ledge's hold; ropeTurn)
    glm::vec3 axis{0.f};   // (ropes8a) on a rope: its axis's point, less the palm's hold (to), as drawn
    float thickness{0.f};  // and its thickness (0: a ledge's hold)
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
constexpr int mantleStatBit = 1 << 30; // STAT_QVR_CLIMB: mantling now (bits 0-29 the holds')
constexpr float ropeHoldNear = 12.f;   // units: a hold this near a drawn rope is on it (a frame's swing between them and more: ropeHold)
constexpr float ropeHandOut = 0.025f;  // m: a hand on a rope, its palm's middle from the rope towards the climber (ropeHandOffset)
constexpr float ropeHandSurface = 0.4f; // of its thickness: at least this far (a thick chain's surface, a little in: ropePalm)

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

// (ropes5) The drawn hand's turn gripping a rope whose way up at the hold is `up` (as drawn), the hold's way out (towards
// the climber) `out`: the rope through the fist as a gun's grip is held (the thumb's side up the rope, the palm against
// it), the front (the knuckles) away from the body; the same for either hand (the left hand's model mirrors it).
[[nodiscard]] glm::mat3 ropeTurn(const glm::vec3& out, const glm::vec3& up)
{
    glm::vec3 front = -(out - up * glm::dot(out, up));
    front = glm::length(front) > 1e-4f ? glm::normalize(front) : glm::vec3{1.f, 0.f, 0.f};
    const glm::mat3 m{front, glm::cross(up, front), up};
    // (ropes10, his flat note 2026-11-23: the knuckles exactly level) Pitched up by vr_rope_hand_pitch: turned about the
    // level line square to the front, the front towards the world's up (a monkey traverse's knuckles, already up, stay).
    const glm::vec3 axis = glm::cross(front, glm::vec3{0.f, 0.f, 1.f});
    const float lean = glm::length(axis);
    const float pitch = glm::radians(za::clamp(vr_rope_hand_pitch.value, 0.f, 45.f)) * za::min(1.f, lean / 0.5f);
    return pitch > 1e-4f ? glm::mat3_cast(glm::angleAxis(pitch, axis / lean)) * m : m;
}

// (ropes8b, his flat note 2026-11-23) Under a monkey traverse's rope (the palm out from it downwards, `out`): which way
// along it `up` (ropeTurn's thumb side) points for `hand`, so that its palm faces forward and in: an overhand grip
// (the palm away from the face, the back of the hand towards him, the knuckles up over the rope) facing across the
// rope; facing along it each palm towards the other side (the rope across the palm either way, never the back of the
// hand on it).
[[nodiscard]] glm::vec3 monkeyRopeUp(int hand, const glm::vec3& out, const glm::vec3& up)
{
    const float yaw = glm::radians(cl.viewangles[YAW]);
    const glm::vec3 fwd{za::cos(yaw), za::sin(yaw), 0.f}, right{za::sin(yaw), -za::cos(yaw), 0.f};
    const glm::vec3 want = fwd + (hand == 1 ? -right : right);
    const glm::mat3 m = ropeTurn(out, up);
    const glm::vec3 palm = hand == 1 ? m[1] : -m[1]; // (the off hand's model mirrored: drawnBack)
    return glm::dot(palm, want) < 0.f ? -up : up;
}

// The drawn hand's turn on a hold: gripping a rope (`up` set: ropeTurn) where vr_rope_hand_grip has it (1 flat-screen
// play only, 2 VR too: the default since ropes6), else as on a ledge (holdTurn).
[[nodiscard]] glm::mat3 turnOn(const glm::vec3& out, const glm::vec3& up, bool mirrored)
{
    const int grip = static_cast<int>(vr_rope_hand_grip.value);
    const bool rope = glm::length(up) > 0.5f && (grip >= 2 || (grip == 1 && flatplay::active()));
    return rope ? ropeTurn(out, up) : holdTurn(out, mirrored);
}

} // namespace

bool qvr::climb::mantlePath(edict_t* ent, pmove::Mantle& m)
{
    Climber* c = climberOf(ent);
    if(!c || c->phase != Phase::Mantling)
    {
        return false;
    }
    edict_t* onto = nullptr;
    m = mantleWay(*c, onto);
    return true;
}

void qvr::climb::mantleMoved(edict_t* ent, const pmove::Mantle& m, bool done)
{
    Climber* c = climberOf(ent);
    if(!c || c->phase != Phase::Mantling)
    {
        return;
    }
    edict_t* onto = nullptr;
    mantleWay(*c, onto); // (what it goes onto: the same frame's)
    mantleEnd(ent, *c, m, done, onto);
}

bool qvr::climb::hangPath(edict_t* ent, pmove::Hang& h)
{
    Climber* c = climberOf(ent);
    if(!c || (c->phase != Phase::Hanging && c->phase != Phase::Slipping) || !c->hanging() || !holdsStill(*c))
    {
        return false;
    }
    h = hangState(ent, *c);
    return true;
}

void qvr::climb::hangMoved(edict_t* ent, const pmove::Hang& h, const pmove::HangSums& s, const pmove::Mantle& m)
{
    Climber* cp = climberOf(ent);
    if(!cp || (cp->phase != Phase::Hanging && cp->phase != Phase::Slipping))
    {
        return;
    }
    Climber& c = *cp;
    const VrMove* move = server::clientMove(ent);
    if(!move)
    {
        transition(ent, c, Phase::Free, Why::NoMove);
        return;
    }
    // (the hands as the newest move ran them: each grip's last place relative to the body)
    Hang hang{ent, c, move, qcvm->time, {vec(h.grips[0].lastRel), vec(h.grips[1].lastRel)}};
    hangStepped(c, h, s, hang);
    if(h.mantleHand >= 0)
    {
        // A move's test began the mantle (B8j): the moves after it carried him along it (`m`: its time run, ended or not).
        beginMantle(ent, c, h.mantleHand, m);
        c.mantleAt = m.at;
        if(!m.on)
        {
            mantleEnd(ent, c, m, true, nullptr);
        }
        return;
    }
    if(s.steps == 0)
    {
        hang.origin = vec(ent->v.origin); // (no moves this frame: nothing moved)
    }
    hang.noRoom = h.noRoom;
    hangFramePart(hang);
}

int qvr::climb::ropeHands(edict_t* ent)
{
    const Climber* c = climberOf(ent);
    if(!c || !c->hanging())
    {
        return 0;
    }
    return (c->grips[0].active && c->grips[0].rope ? 1 : 0) + (c->grips[1].active && c->grips[1].rope ? 1 : 0);
}

int qvr::climb::ropePullGrunts(edict_t* ent)
{
    const Climber* c = climberOf(ent);
    return c ? c->pullGrunts : 0;
}

bool qvr::climb::takesInMoves(edict_t* ent)
{
    const Climber* c = climberOf(ent);
    return c && VR_PredictWalks(ent) && !(c->hanging() && !holdsStill(*c));
}

bool qvr::climb::takeState(edict_t* ent, TakeRules& r, TakeMemory& m)
{
    const Climber* c = climberOf(ent);
    if(!c)
    {
        return false;
    }
    r = takeRules(ent, *c, qcvm->time);
    m = c->take;
    return true;
}

int qvr::climb::moveTake(edict_t* ent, pmove::Body& b, TakeRules& r, const pmove::Input& in)
{
    Climber* cp = climberOf(ent);
    if(!cp)
    {
        return 0;
    }
    Climber& c = *cp;
    r.noGrabLeft -= static_cast<float>(in.dt); // (the float frame time, as before B11b part 2)
    const VrMove move = takeMove(in, b.origin);
    const Seeker s{ent, nullptr, vec(b.origin), b.origin[2] + ent->v.mins[2]};
    const bool air = !(b.flags & FL_ONGROUND) && b.waterLevel < 2;
    // (a hang the moves let go of whose climber has not yet: its frame's let-go next frame, then the regrab delay)
    const bool blocked = b.mantle.on || c.phase == Phase::Mantling || (c.phase != Phase::Free && !b.hang.on);
    int took = 0;
    for(int h = 0; h < 2; h++)
    {
        const bool active = b.hang.on && b.hang.grips[h].active;
        const bool otherActive = b.hang.on && b.hang.grips[1 - h].active;
        const double t0 = Sys_DoubleTime();
        const int traces0 = traceCount;
        const TakeTry t = takeTry(c.take, r, s, move, h, static_cast<float>(in.dt), active, otherActive, blocked, air, (b.flags & FL_ONGROUND) != 0);
        if(t.what == Take::found && c.grips[1 - h].active != otherActive)
        {
            continue; // (the other hand let go in a move, its climber's grip not yet: a rare re-press, the next frame's)
        }
        if(!takeDone(ent, c, move, h, t, qcvm->time, traces0, t0))
        {
            continue;
        }
        if(c.phase == Phase::Free)
        {
            transition(ent, c, Phase::Hanging, Why::Took);
        }
        if(holdsStill(c))
        {
            if(!b.hang.on)
            {
                b.hang = hangState(ent, c); // (the next moves hang from it)
            }
            else
            {
                hangGripOf(c.grips[h], b.hang.grips[h]); // (the other hand joins the hang the moves run)
            }
            took = 1;
        }
        else
        {
            return 2;
        }
    }
    return took;
}

void qvr::climb::replayTake(pmove::Body& b, TakeMemory& m, TakeRules& r, const pmove::Input& in)
{
    r.noGrabLeft -= static_cast<float>(in.dt); // (the float frame time, as before B11b part 2)
    const VrMove move = takeMove(in, b.origin);
    const Seeker s{nullptr, &b, vec(b.origin), b.origin[2] + r.minsZ};
    const bool air = !(b.flags & FL_ONGROUND) && b.waterLevel < 2;
    for(int h = 0; h < 2; h++)
    {
        const bool active = b.hang.on && b.hang.grips[h].active;
        const bool otherActive = b.hang.on && b.hang.grips[1 - h].active;
        const TakeTry t = takeTry(m, r, s, move, h, static_cast<float>(in.dt), active, otherActive, b.mantle.on, air, (b.flags & FL_ONGROUND) != 0);
        if(t.what != Take::found)
        {
            continue;
        }
        // The server's takeHold and hangState, on the world alone (moveTake: the same numbers).
        Grip g;
        (void)gripTaken(g, *t.ledge, move, h, vec(b.origin));
        if(!b.hang.on)
        {
            b.hang = pmove::Hang{};
            b.hang.on = true;
            b.hang.minsZ = r.minsZ;
            b.hang.maxsXY[0] = r.maxsX;
            b.hang.maxsXY[1] = r.maxsY;
            b.hang.mantleTime = r.mantleTime;
            b.hang.mantleHeight = za::clamp(r.mantleHeight, 0.f, 40.f);
            b.hang.mantleSlope = za::clamp(r.mantleSlope, 0.f, 60.f);
        }
        hangGripOf(g, b.hang.grips[h]);
    }
}

void qvr::climb::reset()
{
    for(Climber& c : climbers)
    {
        c = Climber{};
    }
}

namespace
{
// Forgotten with the server's world (vr_mem.hpp's resets): the climbers' holds.
qvr::mem::Reset serverWorldReset{
    "climb", qvr::mem::ResetEvent::ServerWorld, qvr::mem::ResetPhase::State, &qvr::climb::reset};
} // namespace

void qvr::climb::init()
{
    Cmd_AddCommand("vr_climb_probe", probe_f);
    Cmd_AddCommand("vr_climb_try", try_f);
    Cmd_AddCommand("vr_climb_mantle_grunt_test", gruntTest_f);
}

void qvr::climb::precache()
{
    for(const char* grunt : mantleGrunts)
    {
        (void)physsound::precacheOne(grunt);
    }
}

const char* qvr::climb::gruntSample(float choice)
{
    const int n = static_cast<int>(za::getArraySize(mantleGrunts));
    return mantleGrunts[za::clamp(static_cast<int>(choice), 0, n - 1)];
}

bool qvr::climb::grunt(edict_t* ent, float choice, float volume)
{
    const char* sample = gruntSample(choice);
    volume = za::clamp(volume, 0.f, 1.f);
    if(volume <= 0.f)
    {
        return false;
    }
    for(int i = 1; i < MAX_SOUNDS && sv.sound_precache[i]; i++)
    {
        if(!ZA_STRCMP(sv.sound_precache[i], sample))
        {
            SV_StartSound(ent, 2, sample, static_cast<int>(za::lround(255.f * volume)), 1.f); // (CHAN_VOICE, ATTN_NORM)
            return true;
        }
    }
    return false;
}

namespace
{

// Where climber `c`'s hand `h` holds now and the way out from its face: a grip's hold, or a flat mantle's hand on the
// ledge while the mantle runs (flatMantleHands). False: it holds nothing.
bool holdOf(const Climber* c, int h, glm::vec3& hold, glm::vec3& out)
{
    const Climber::FlatHold* flat = c && !c->grips[h].active && c->phase == Phase::Mantling && c->flatHolds[h].on
                                        ? &c->flatHolds[h]
                                        : nullptr;
    // (A flat ladder's hand holding a weapon keeps it: not drawn on its rung, flatLadderFrame; a flat rope's too.)
    if(!c || (!(c->grips[h].active && ((!c->flatLadder && !c->flatRope) || c->flatShow[h])) && !flat))
    {
        return false;
    }
    out = flat ? flat->out : c->grips[h].ledge.out;
    if(flat)
    {
        const bool moves = flat->entNum > 0 && flat->entNum < qcvm->num_edicts;
        hold = flat->hold + (moves ? vec(EDICT_NUM(flat->entNum)->v.origin) - flat->entOrigin : glm::vec3{0.f});
    }
    else
    {
        hold = holdNow(c->grips[h]);
    }
    return true;
}

} // namespace

bool qvr::climb::serverHold(edict_t* ent, int hand, glm::vec3& at, glm::vec3& out)
{
    return hand >= 0 && hand < 2 && holdOf(climberOf(ent), hand, at, out);
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
        glm::vec3 out{0.f};
        if(holdOf(c, h, hold, out))
        {
            const int yaw = static_cast<int>(za::lround(za::atan2(out.y, out.x) / glm::two_pi<float>() * yawSteps)) & 255;
            bits |= (1 << h) | ((c->grips[h].serial & 63) << (2 + 6 * h)) | (yaw << (14 + 8 * h));
        }
        for(int i = 0; i < 3; i++)
        {
            statsi[first + i] = static_cast<int>(za::lround(hold[i] * statScale));
        }
    }
    if(c && c->phase == Phase::Mantling)
    {
        bits |= mantleStatBit; // (the client's mantling(): flat play's mantle FOV, vr_flatspeedfov.cpp)
    }
    statsi[STAT_QVR_CLIMB] = bits;
}

bool qvr::climb::mantling()
{
    return (cl.stats[protocol::STAT_QVR_CLIMB] & mantleStatBit) != 0;
}

bool qvr::climb::ropeHold(const glm::vec3& hold)
{
    return climbrope::clientRopeDistance(hold) <= ropeHoldNear;
}

glm::vec3 qvr::climb::ropeHandOffset(const glm::vec3& out)
{
    return out * (ropeHandOut * units::metresToUnits());
}

bool qvr::climb::ropePalm(const glm::vec3& hold, const glm::vec3& out, glm::vec3& palm, glm::vec3* up, glm::vec3* axis,
    float* thick, glm::vec3* usedOut)
{
    glm::vec3 at{0.f};
    glm::vec3 ropeUp{0.f, 0.f, 1.f};
    float thickness = 0.f;
    if(!climbrope::clientRopeNearest(hold, ropeHoldNear, at, ropeUp, thickness))
    {
        return false;
    }
    glm::vec3 o = out - ropeUp * glm::dot(out, ropeUp); // (square to the rope where it is held)
    // (ropes8b, his flat note 2026-11-23: on a level rope the hands were beside it, turned as on an upright one) Under
    // a monkey traverse's rope (or level pole) the body hangs below it: the hand is out from it downwards (square to
    // it), the palm under the rope, the knuckles up over it (turnOn).
    if(climbrope::clientRopeMonkey(hold, ropeHoldNear))
    {
        const glm::vec3 down{0.f, 0.f, -1.f};
        o = down - ropeUp * glm::dot(down, ropeUp);
    }
    o = glm::length(o) > 1e-4f ? glm::normalize(o) : out;
    palm = at + o * za::max(ropeHandOut * units::metresToUnits(), ropeHandSurface * thickness);
    if(usedOut)
    {
        *usedOut = o;
    }
    if(up)
    {
        *up = ropeUp;
    }
    if(axis)
    {
        *axis = at;
    }
    if(thick)
    {
        *thick = thickness;
    }
    return true;
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
    const float dt = pin.last >= 0.0 ? static_cast<float>(CLAMP(0.0, vr_gametime - pin.last, 0.1)) : 0.f;
    pin.last = vr_gametime;
    const int bits = cl.stats[STAT_QVR_CLIMB];
    if(bits & (1 << hand))
    {
        const int first = hand ? STAT_QVR_CLIMBMAINX : STAT_QVR_CLIMBOFFX;
        const float yaw = static_cast<float>((bits >> (14 + 8 * hand)) & 255) / yawSteps * glm::two_pi<float>();
        const glm::vec3 out{za::cos(yaw), za::sin(yaw), 0.f};
        const glm::vec3 at = glm::vec3{static_cast<float>(cl.stats[first]), static_cast<float>(cl.stats[first + 1]),
                                 static_cast<float>(cl.stats[first + 2])} /
                             statScale;
        // (ropes4: a rope's, on it; ropes5: on the rope as drawn this frame, not the server's hold a tick old: no
        // step a server frame in the hand, no gap where the drawn rope is between messages or bent off the line)
        glm::vec3 hold{0.f};
        glm::vec3 ropeUp{0.f};
        glm::vec3 axis{0.f};
        float thick = 0.f;
        glm::vec3 palmOut = out;
        if(ropePalm(at, out, hold, &ropeUp, &axis, &thick, &palmOut))
        {
            if(palmOut.z < -0.5f)
            {
                ropeUp = monkeyRopeUp(hand, palmOut, ropeUp); // (ropes8b: under a monkey traverse's rope)
            }
            pin.offset = hold - at;
            pin.axis = axis - hold;
            pin.thickness = thick;
        }
        else
        {
            pin.thickness = 0.f;
            pin.offset = handOffset(out, hand);
            hold = at + pin.offset;
        }
        const int serial = (bits >> (2 + 6 * hand)) & 63;
        if(pin.weight <= 0.f)
        {
            pin.from = pin.to = hold;
            pin.fromOut = palmOut;
            pin.fromUp = ropeUp;
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
            pin.fromUp = pin.toUp;
            pin.along = 0.f;
            pin.inTime = easeTime(glm::distance(hold, pin.from));
        }
        pin.to = hold; // (a hold on a moving brush model moves)
        pin.toOut = palmOut;
        pin.toUp = ropeUp;
        pin.serial = serial;
        pin.along = za::min(1.f, pin.along + dt / pin.inTime);
        pin.weight = za::min(1.f, pin.weight + dt / pin.inTime);
    }
    else
    {
        pin.weight = za::max(0.f, pin.weight - dt / pinOutTime);
        pin.along = za::min(1.f, pin.along + dt / pinInTime);
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
    const glm::quat onFrom = glm::quat_cast(turnOn(pin.fromOut, pin.fromUp, mirrored)),
                    onTo = glm::quat_cast(turnOn(pin.toOut, pin.toUp, mirrored));
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

bool qvr::climb::handFree(edict_t* ent, int hand, bool holsters)
{
    if(hand < 0 || hand > 1)
    {
        return false;
    }
    const VrMove* move = server::clientMove(ent);
    return move && handEmpty(ent, move, hand) && !(holsters && holsterWins(ent, move->hotspots[hand], false));
}

int qvr::climb::heldProp(edict_t* ent, int hand)
{
    const Climber* c = ent ? climberOf(ent) : nullptr;
    if(!c || hand < 0 || hand > 1)
    {
        return 0;
    }
    const Grip& g = c->grips[hand];
    return g.active && g.prop ? g.entNum : 0;
}

bool qvr::climb::ropeLine(int hand, glm::vec3& at, glm::vec3& up, float& thickness, float& weight)
{
    if(hand < 0 || hand > 1 || pins[hand].weight <= 0.f || pins[hand].thickness <= 0.f || glm::length(pins[hand].toUp) < 0.5f)
    {
        return false;
    }
    const Pin& pin = pins[hand];
    at = pin.hold() + pin.axis;
    up = glm::normalize(pin.toUp);
    thickness = pin.thickness;
    weight = pin.weight * pin.weight * (3.f - 2.f * pin.weight);
    return true;
}

float qvr::climb::drawnShare(int hand)
{
    if(hand < 0 || hand > 1 || pins[hand].weight <= 0.f)
    {
        return 0.f;
    }
    const float w = pins[hand].weight;
    return w * w * (3.f - 2.f * w);
}

bool qvr::climb::drawnPalm(int hand, glm::vec3& palm)
{
    if(hand < 0 || hand > 1 || pins[hand].weight <= 0.f)
    {
        return false;
    }
    palm = pins[hand].hold();
    return true;
}

bool qvr::climb::drawnBack(int hand, glm::vec3& back, glm::vec3* front)
{
    if(hand < 0 || hand > 1 || pins[hand].weight <= 0.f)
    {
        return false;
    }
    // (The hand model's left is a right hand's palm side; the off hand's mirrored model has it the other way.)
    const glm::mat3 m = turnOn(pins[hand].toOut, pins[hand].toUp, hand == 0);
    back = hand == 0 ? m[1] : -m[1];
    if(front)
    {
        *front = m[0];
    }
    return true;
}

int qvr::climb::flatMantles(edict_t* ent, bool& mantling)
{
    const Climber* c = climberOf(ent);
    mantling = c && c->phase == Phase::Mantling;
    return c ? c->flatMantles : 0;
}

int qvr::climb::flatRopes(edict_t* ent, bool& climbing)
{
    const Climber* c = climberOf(ent);
    climbing = c && c->flatRope && c->hanging();
    return c ? c->flatRopes : 0;
}

int qvr::climb::flatLadders(edict_t* ent, bool& climbing)
{
    const Climber* c = climberOf(ent);
    climbing = c && c->flatLadder && c->hanging();
    return c ? c->flatLadders : 0;
}

bool qvr::climb::holding(int hand)
{
    return hand >= 0 && hand <= 1 && (cl.stats[protocol::STAT_QVR_CLIMB] & (1 << hand)) != 0;
}

bool qvr::climb::heldAt(int hand, glm::vec3& at, glm::vec3& out)
{
    using namespace protocol;
    if(!holding(hand))
    {
        return false;
    }
    const int bits = cl.stats[STAT_QVR_CLIMB];
    const int first = hand ? STAT_QVR_CLIMBMAINX : STAT_QVR_CLIMBOFFX;
    const float yaw = static_cast<float>((bits >> (14 + 8 * hand)) & 255) / yawSteps * glm::two_pi<float>();
    out = glm::vec3{za::cos(yaw), za::sin(yaw), 0.f};
    at = glm::vec3{static_cast<float>(cl.stats[first]), static_cast<float>(cl.stats[first + 1]),
             static_cast<float>(cl.stats[first + 2])} /
         statScale;
    return true;
}
