#pragma once

// vr_foegrab_internal.hpp -- what vr_foegrab.cpp (holding enemies: see vr_foegrab.hpp) shares with its
// diagnostics, vr_foegrab_diag.cpp. Internal: included by those two alone. What was the file's anonymous
// namespace and both use is namespace foegrab::detail: its types and variables here (each variable one
// object: inline), its functions declared here and defined in the file that owns them.

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
#include "vr_mgroups.hpp"
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

#include "Zancle/Chrono/Clock.hpp"
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
#include "Zancle/Math/Pow.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/Math/Sqrt.hpp"
#include "vr_zancle.hpp"

#include <glm/gtc/constants.hpp>

using namespace qvr;
using namespace qvr::progs;

namespace foegrab::detail
{
constexpr float headSurface = 3.5f;   // units: a head's surface lies up to this beyond its sphere (QC VR_HEAD_MARGIN)
constexpr float headJaw = 0.35f;      // the head zone's sphere counts down to this share of its radius under its middle (HeadZone)
constexpr float headWide = 0.9f;      // and no further across its axis than this share of its radius (shoulder pads beside it)
constexpr float neckWide = 0.75f;     // under it, its neck: no further across than this share of the head's radius
constexpr float palmSlack = 2.f;      // units: a ragdoll's zone grown for the palm (its middle, off the skin; ragdollNeckCheck)

// A sample of the hands on a head (neckStep): when, the line between the palms (play space, metres), the head's
// reference frame then (world from its body: a standing one's yaw, a ragdoll's chest), the wrists' roll so far (rad).
struct NeckSample
{
    double t{0.0};
    glm::vec3 line{0.f};
    glm::mat3 frame{1.f};
    float roll{0.f};
};
constexpr int neckSamples = 128;        // a second at neckSampleStep

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
    int dragWhere{-1};       // where this hand alone holds it (oneHandDrags' DragPart; -1 not yet), printed as it changes
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
    // The leg pull's stroke (legCheck): the hands coming towards you at half the pull's speed or more since `legFrom`
    // (their tracked palms' middle then, world).
    bool legStroke{false};
    glm::vec3 legFrom{0.f};
    int legMask{0}; // the hands of that grip (1 the off, 2 the main, 3 both: legCheck; another: its stroke begun again)
    bool legLogged{false}; // the grip's parts printed (vr_foegrab_debug: once a two-hand hold)
    // Its feet held, it can't turn (legCheck): its yaw as the leg grip began (legYawEnt: whose; 0: none).
    int legYawEnt{0};
    float legYaw{0.f};
    // The enemy held on as a ragdoll after a throw or a lift (handOver, carryStep): its entity (0: none), since when; a
    // lift's (`lifted`), when it breaks free (breakAt).
    int carried{0};
    double carriedSince{0.0};
    bool lifted{false};
    bool flailing{false}; // its lift's flail begun (box3d::ragdollFlail): ended when let go of, even killed in the hands
    bool snapped{false};
    double breakAt{-1.0};
    // A leg pull's hold (legCheck's handOver): when it was handed over (-1: none). Soft while it falls (Pull Hold
    // Strength), it firms up to Pull Drag Strength once it lies (legDragStep), to be dragged by the legs.
    double legDragFrom{-1.0};
    bool legDragFull{false}; // (firmed up all the way: printed once)
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

inline Holder holders[MAX_SCOREBOARD];

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
inline WalkTest walkTest;

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
inline Pin pins[2];
[[nodiscard]] glm::vec3 vec(const float* v);
[[nodiscard]] glm::vec3 forwardFlat(edict_t* player);
[[nodiscard]] const char* handName(int h);
[[nodiscard]] float cm();
[[nodiscard]] bool debug(int level = 1);
[[nodiscard]] glm::vec3 trackedPalmOf(edict_t* ent, const VrMove& move, int h);
// Whether action `p` takes `m` (its monster group's switch: vr_mgroups.hpp, Combat > Monster Groups); the training dummy
// always.
[[nodiscard]] bool groupTakes(edict_t* m, mgroups::Param p);
[[nodiscard]] const char* throwResult(float r);
float throwDown(edict_t* player, edict_t* m, const glm::vec3& dir, float twist, bool trace);
[[nodiscard]] int carryingHands(int c, edict_t* m);

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
[[nodiscard]] const char* zoneName(Zone z);

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
[[nodiscard]] bool atHead(Zone z);
[[nodiscard]] HeadZone headZoneOf(edict_t* m);
[[nodiscard]] HeadZone ragdollZoneOf(edict_t* m, glm::vec3& joint);
[[nodiscard]] glm::vec3 restOf(edict_t* m, const glm::vec3& p);
[[nodiscard]] glm::vec3 spotOf(const Hold& g);

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
inline NeckDiag neckDiag;
[[nodiscard]] float neckWindow();
void neckDiagMeasured(int c, edict_t* m, bool ragdoll, const HeadZone& z, const glm::vec3* point, const Zone* where,
    const NeckTwist& t);
void neckDiagEnd(bool snapped);
void neckSnap(int c, edict_t* m, const NeckTwist& t, bool ragdoll);

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
TwoHands twoHandsOf(edict_t* player, const VrMove& move, edict_t* m, bool log);
[[nodiscard]] bool liftable(edict_t* m);
[[nodiscard]] float liftSpeedNeed();
[[nodiscard]] float liftReach();

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
    // The leg grabs' (legCheck; vr_foegrab_leggrab_debug): the part each hand holds (or the palm is at), the grip's
    // weight (0: not a leg grip), its mass's factor, the pull towards you, how far the stroke came (cm), the needs.
    const char* legBone[2]{"", ""};
    int legPart[2]{0, 0};
    float legWeight{0.f};
    float legMass{0.f};
    float legFactor{1.f};
    float legToward{0.f};
    float legDrawn{0.f};
    float legPullNeed{0.f};
    float legLiftNeed{0.f};
    glm::vec3 legYou{0.f}; // level, from it to you
    const char* legNow{""};
    float legPullBest{0.f};
    float legUpBest{0.f};
    bool legTried{false};
    double legAt{-1.0};
    char legLast[48]{};
};
inline GrabDiag grabDiag;
[[nodiscard]] bool legDiagOn();
void liftNote(int c, const char* why);
void throwNote(int c, const char* why);
void grabDiagMeasured(int c, const TwoHands& t, const glm::vec3& way);
void liftTry(int c, edict_t* m, bool ok, const char* last, float up, float off);
void throwTry(int c, edict_t* m, bool ok, const char* last, float twist, float speed, const char* wayName);

// The leg grabs (vr_foegrab_leggrab; ROUND22.md, "Leg grabs"): where on a humanoid's legs a hand holds.
enum class LegPart : int
{
    None = 0,  // not its legs
    Thigh = 1, // above its knee
    Lower = 2, // below it: its shin, its foot
};
[[nodiscard]] const char* legPartName(int p);
[[nodiscard]] LegPart legPartAt(edict_t* m, const glm::vec3& at, const char** bone);
[[nodiscard]] float legWeight(LegPart p);
// Whether hand `h` of player `c` drags the standing enemy it holds (serverFrame's follow and pull): always with both his
// hands on it; alone, by vr_foegrab_drag_onehand and the part it holds (`where`, if given: its name).
[[nodiscard]] bool oneHandDrags(int c, int h, edict_t* m, const char** where = nullptr);
[[nodiscard]] float legMassFactor(float mass);
const char* throwWayName(const glm::vec3& dir, const glm::vec3& middle, const glm::vec3& you, const char** sub);
bool moveFlat(edict_t* m, const glm::vec3& to);
void walkTestStep(double dt);
void neckDiagFrame();
void grabDiagFrame();
} // namespace foegrab::detail

using namespace ::foegrab::detail;
