// vr_posestudio.cpp -- the Pose Studio: see vr_posestudio.hpp.

#include "vr_posestudio.hpp"
#include "vr_aggrip.hpp"
#include "vr_aglegs.hpp"
#include "vr_agpose.hpp"
#include "vr_backend.hpp"
#include "vr_bladeedit.hpp"
#include "vr_cvars.hpp"
#include "vr_files.hpp"
#include "vr_hands.hpp"
#include "vr_holstersetup.hpp"
#include "vr_lines.hpp"
#include "vr_main.hpp"
#include "vr_menu.hpp"
#include "vr_menuui.hpp"
#include "vr_text3d.hpp"
#include "vr_units.hpp"
#include "vr_view.hpp"
#include "vr_weapons.hpp"

#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Acos.hpp"
#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/Sqrt.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Fabs.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/String/String.hpp"
#include "vr_zancle.hpp"

#include <glm/gtc/quaternion.hpp>

#include <string.h>
#include <time.h>

namespace qvr::posestudio
{
namespace
{

using rigpose::Rigid;

enum PartId : int
{
    PWeaponR,
    PWeaponL,
    PHandR,
    PHandL,
    PElbowR,
    PElbowL,
    PChest,
    // The legs (the Pickup's kneel, vr_aglegs: shown in the Pickup pose only; their key agpose::legCode)
    PPelvis,
    PFootPlant,
    PKneeKneel,
    PFootKneel,
    PKneePlant,
    PCount
};

// (the first six as stage 1 numbered them: the mock's posebutton indices)
enum ButtonId : int
{
    BtnScope,
    BtnUndo,
    BtnReset,
    BtnFrozen,
    BtnCancel,
    BtnDone,
    BtnPosePrev,
    BtnPlay,
    BtnPoseNext,
    BtnKeyPrev,
    BtnResetPose,
    BtnKeyNext,
    BtnWeaponMode,
    BtnCopy,
    BtnGrabSet,
    BtnCount
};

// Each button's place on the panel: column (-1, 0, 1), row (0 the top)
struct Place
{
    int col, row;
};
constexpr Place buttonPlace[BtnCount] = {
    {0, 2},  // Scope
    {0, 3},  // Undo
    {1, 2},  // Reset Grips
    {1, 3},  // Frozen
    {0, 4},  // Cancel
    {1, 4},  // Done
    {-1, 0}, // < Pose
    {0, 0},  // Play
    {1, 0},  // Pose >
    {-1, 1}, // < Key
    {0, 1},  // Reset Pose
    {1, 1},  // Key >
    {-1, 2}, // Weapon: Pose / Grip
    {-1, 3}, // Copy to the other kind
    {-1, 4}, // Grab: Everything / Weapons / Limbs
};
constexpr int PANEL_ROWS = 5;

constexpr int grabStartPart = 99;      // partPoint's: the controller where the grab began
constexpr float PANEL_DISTANCE = 0.55f;// metres from the eyes...
constexpr float PANEL_SIDE = 45.f;     // ... this many degrees to the off hand's side of the view (the subject ahead stays clear)
constexpr float PANEL_DROP = 0.33f;    // metres below the eyes
constexpr float PANEL_FOLLOW = 1.0f;   // metres: the head this far from where the panel was placed brings it along
constexpr float BUTTON_WIDTH = 0.17f;  // metres
constexpr float BUTTON_HEIGHT = 0.055f;
constexpr float BUTTON_GAP = 0.015f;
constexpr float BUTTON_DEPTH = 0.08f;  // a hand this near the panel's plane (metres) is at a button
constexpr float HAND_REACH = 0.07f;    // metres: a controller this near a hand's grip point is at it
constexpr float WEAPON_REACH = 0.05f;  // metres: ... this near a weapon's box (or in it) is at it
constexpr float WEAPON_BIAS = 0.03f;   // metres: a hand near enough beats the weapon round it
constexpr float CHEST_BIAS = 0.045f;   // ... and the chest's box (the hands, elbows and weapons before it)
constexpr float HAND_BOX = 2.f;        // units (the monster's): half a hand's box
constexpr float ELBOW_BOX = 1.4f;      // ... an elbow's
constexpr float LEG_BOX = 1.6f;        // ... a foot's (round its ankle), a knee's
constexpr float PELVIS_TURN_MOST = 60.f; // degrees: the pelvis key's turn, each axis
constexpr float WEAPON_PAD = 1.2f;     // units: a weapon's box round its points
constexpr float TICK_UNITS = 1.f;      // a haptic tick each unit (or TICK_DEG) a held part moves
constexpr float TICK_DEG = 5.f;
constexpr int UNDO_MAX = 64;

struct Grab
{
    int part{-1};
    int hand{-1};     // the player's hand holding it
    int ent{0};       // the monster's (locked while held)
    int kind{0};
    int slotKey{-1};  // the entry: aggrip::allWeapons or the weapon's slot
    int gripHand{0};  // aggrip::Main / Off / Second
    Rigid rel;        // the part's world frame in the controller's frame at the grab
    Rigid handHeld;   // a weapon's drag: its hand's grip frame held there (model space)
    Rigid weaponHeld; // a hand's drag on the second grip: the right weapon's frame (model space)
    Rigid hotspot;    // ... the second grip's place on it (secondAt less its offset)
    Rigid own, glob;  // the entry's weapon's own and global offsets at the grab (the total is glob * own)
    Rigid total0;     // the total at the grab
    Rigid total;      // ... now
    Rigid weaponNow;  // a weapon's drag: its frame now (model space; adjust draws it there)
    glm::vec3 startPos{0.f};
    glm::vec3 tickPos{0.f};
    glm::quat tickRot{1.f, 0.f, 0.f, 0.f};
    // A pose key's drag (vr_agpose.hpp): the key, as it was and now; the part's frame and the controller's at the grab
    // (model space), the arm's shoulder and wrist, the torso's turn and the elbow's pole as sent, the waist.
    bool pose{false};
    int code{-1};
    agpose::Key key0, keyNow;
    Rigid part0, ctrl0;
    glm::vec3 shoulder{0.f}, wrist{0.f}, waist{0.f};
    glm::vec3 torso0{0.f}, pole0{0.f};
    bool legMirror{false}; // a leg part's: the kneel mirrored (a left hand's take: the key's y, yaw and roll flipped)
};

// The kind and weapon class QC tells of each armed humanoid (qcHold), the last few.
struct ClassNote
{
    int ent{0};
    int kind{0};
    int cls{-1};
};
constexpr int CLASS_NOTES = 32;

struct Session
{
    bool active{false};
    int world{0};
    int returnPage{-1};
    float freezeWas{0.f};
    za::String entry;      // vr_agrunt_grips on entering (Cancel)
    za::String entryPoses; // vr_agrunt_poses
    za::Vector<za::String> undo, undoPoses; // (snapshots of both, together)
    // the pose shown and edited (stage 2): the pose (agpose::Pose), a strike's phase, Play's count; a weapon's drag
    // moves its pose's place (else, Weapon: Grip, turns it in its hand: stage 1)
    int pose{agpose::Carry};
    int phase{0};
    int playCount{0};
    bool weaponGrip{false};
    int grabSet{0}; // what a grip takes: 0 everything, 1 the weapons only, 2 the limbs only (hands, elbows, chest, legs)
    ClassNote classes[CLASS_NOTES];
    int nextClass{0};
    // the monsters noted: the nearest this frame (pending, filled while the relink runs) and the last complete
    int noteFrame{-1};
    float pendingDist{1e30f};
    Note pending;
    bool havePending{false};
    Note subject;
    bool haveSubject{false};
    int subjectFrame{-1};
    Note locked; // the held one's, while a part is held
    int lockedFrame{-1};
    Grab grab;
    int hover[HAND_COUNT]{-1, -1};
    int hoverButton[HAND_COUNT]{-1, -1};
    bool eaten[HAND_COUNT][6]{};
    glm::vec3 panelAt{0.f}, panelRight{0.f, -1.f, 0.f}, panelUp{0.f, 0.f, 1.f}, panelNormal{-1.f, 0.f, 0.f};
    glm::vec3 panelHead{0.f}; // the head where it was placed
    float panelYaw{0.f};
    za::String feedback;
    double feedbackUntil{0.0};
};
Session ses;

// --- frames --------------------------------------------------------------------------------------------------------

[[nodiscard]] Rigid compose(const Rigid& a, const Rigid& b)
{
    return Rigid{a.rot * b.rot, a.rot * b.pos + a.pos};
}

[[nodiscard]] Rigid inverse(const Rigid& a)
{
    Rigid r;
    r.rot = glm::inverse(a.rot);
    r.pos = -(r.rot * a.pos);
    return r;
}

[[nodiscard]] glm::quat yawQuat(float yawDeg)
{
    return glm::angleAxis(yawDeg * (3.14159265f / 180.f), glm::vec3{0.f, 0.f, 1.f});
}

[[nodiscard]] glm::vec3 toWorld(const Space& s, const glm::vec3& local)
{
    return s.origin + yawQuat(s.yawDeg) * (local * s.scale);
}

[[nodiscard]] Rigid toWorld(const Space& s, const Rigid& r)
{
    return Rigid{glm::normalize(yawQuat(s.yawDeg) * r.rot), toWorld(s, r.pos)};
}

[[nodiscard]] Rigid toModel(const Space& s, const Rigid& w)
{
    const glm::quat inv = glm::inverse(yawQuat(s.yawDeg));
    return Rigid{glm::normalize(inv * w.rot), inv * (w.pos - s.origin) / za::max(s.scale, 0.01f)};
}

// Where a controller takes parts: the hand before anything holds it out (the monster's body pushes the drawn hand off
// it, the controller goes on in), and its world frame (x forward, y left, z up).
[[nodiscard]] glm::vec3 grabber(const hands::State& s, int hand)
{
    return s.unresolvedPos[hand];
}

[[nodiscard]] Rigid controller(const hands::State& s, int hand)
{
    glm::vec3 f, r, u;
    hands::angleVectors(s.unresolvedRot[hand], f, r, u);
    return Rigid{glm::normalize(glm::quat_cast(glm::mat3{f, -r, u})), grabber(s, hand)};
}

[[nodiscard]] float angleDeg(const glm::quat& a, const glm::quat& b)
{
    const float d = za::fabs(glm::dot(glm::normalize(a), glm::normalize(b)));
    return 2.f * za::acos(za::min(1.f, d)) * (180.f / 3.14159265f);
}

// --- what is shown -------------------------------------------------------------------------------------------------

// The note the parts are drawn from: the held monster's while a part is held, else the nearest's.
[[nodiscard]] const Note* current()
{
    if(ses.grab.part >= 0)
    {
        return ses.lockedFrame >= host_framecount - 2 ? &ses.locked : nullptr;
    }
    return ses.haveSubject && ses.subjectFrame >= host_framecount - 2 ? &ses.subject : nullptr;
}

[[nodiscard]] bool heldR(const Note& n)
{
    return n.prop > 0 && n.placed.on[0];
}

[[nodiscard]] bool heldL(const Note& n)
{
    return n.propL > 0 && n.placed.on[1];
}

// A hand on no weapon: its own target (a fist, a free hand: its pose's key).
[[nodiscard]] bool freeHand(const Note& n, int side)
{
    return n.t.goal[side] == agarms::Goal::Point;
}

[[nodiscard]] bool isLeg(int p)
{
    return p >= PPelvis && p < PCount;
}

// The subject's legs as solved now (model space), in the Pickup pose with its kneel up (vr_aglegs); else false.
[[nodiscard]] bool legsOf(const Note& n, aglegs::Placed& out)
{
    return ses.pose == agpose::Pickup && aglegs::enabled() && aglegs::placed(n.ent, out) && out.on > 0.5f;
}

// A leg part's frame (model space): the pelvis's pivot (turned with it), a foot's ankle, a knee (unturned).
[[nodiscard]] Rigid legFrame(const aglegs::Placed& L, int p)
{
    const int kneel = L.kneel == 0 ? 0 : 1, plant = 1 - kneel;
    switch(p)
    {
        case PPelvis: return Rigid{glm::normalize(L.pelvis.rot), L.pelvis.pos};
        case PFootPlant: return Rigid{glm::quat{1.f, 0.f, 0.f, 0.f}, L.ankle[plant]};
        case PFootKneel: return Rigid{glm::quat{1.f, 0.f, 0.f, 0.f}, L.ankle[kneel]};
        case PKneeKneel: return Rigid{glm::quat{1.f, 0.f, 0.f, 0.f}, L.knee[kneel]};
        default: return Rigid{glm::quat{1.f, 0.f, 0.f, 0.f}, L.knee[plant]};
    }
}

// The leg key's part a leg part's drag moves (the pelvis: its place, its turn the next).
[[nodiscard]] int legKeyPart(int p)
{
    switch(p)
    {
        case PPelvis: return agpose::LegPelvis;
        case PFootPlant: return agpose::LegPlantFoot;
        case PKneeKneel: return agpose::LegKneelKnee;
        case PFootKneel: return agpose::LegKneelFoot;
        default: return agpose::LegPlantKnee;
    }
}

// Whether part `p` can be taken.
[[nodiscard]] bool shown(const Note& n, int p)
{
    switch(p)
    {
        case PWeaponR: return heldR(n);
        case PWeaponL: return heldL(n);
        case PHandR: return heldR(n) || freeHand(n, 0);
        case PHandL: return heldL(n) || (heldR(n) && n.placed.held2) || freeHand(n, 1);
        case PElbowR:
        case PElbowL:
        case PChest: return true;
        default:
        {
            aglegs::Placed L;
            return isLeg(p) && legsOf(n, L);
        }
    }
}

[[nodiscard]] int sideOf(int p)
{
    return p == PWeaponL || p == PHandL || p == PElbowL ? 1 : 0;
}

// Whether a drag of part `p` edits its pose's key (else a grip: a hand on its weapon, a weapon with Weapon: Grip).
[[nodiscard]] bool editsPose(const Note& n, int p)
{
    switch(p)
    {
        case PWeaponR:
        case PWeaponL: return !ses.weaponGrip;
        case PHandR:
        case PHandL: return freeHand(n, sideOf(p));
        default: return true;
    }
}

// Whether part `p` is in the Grab set (the panel's Grab: Everything / Weapons / Limbs; a stick's click cycles it).
[[nodiscard]] bool inGrabSet(int p)
{
    const bool weapon = p == PWeaponR || p == PWeaponL;
    return ses.grabSet == 0 || (ses.grabSet == 1) == weapon;
}

// The kind and weapon class QC last told of `ent` (-1: none yet).
[[nodiscard]] int classOf(int ent)
{
    for(const ClassNote& c : ses.classes)
    {
        if(c.ent == ent && c.cls >= 0)
        {
            return c.cls;
        }
    }
    return -1;
}

// The poses the subject steps through; `ses.pose` one of them (else the first).
int subjectPoses(const Note* n, int* out)
{
    const int cls = n ? classOf(n->ent) : -1;
    if(cls < 0)
    {
        out[0] = agpose::Carry;
        return 1;
    }
    return agpose::posesOf(cls, out, agpose::NumPoses);
}

[[nodiscard]] int poseIndex(const Note* n, int* count)
{
    int poses[agpose::NumPoses];
    const int c = subjectPoses(n, poses);
    *count = c;
    for(int i = 0; i < c; i++)
    {
        if(poses[i] == ses.pose)
        {
            return i;
        }
    }
    return -1;
}

[[nodiscard]] int keyCode(const Note& n)
{
    const int cls = classOf(n.ent);
    return cls < 0 ? -1 : agpose::code(n.kind, cls, ses.pose, agpose::isStrike(ses.pose) ? ses.phase : 0);
}

// The torso's turn (yaw, pitch, roll; degrees) as a rotation (hitreact's bodyTurn) and back.
[[nodiscard]] glm::quat torsoQuat(const glm::vec3& t)
{
    constexpr float d2r = 3.14159265f / 180.f;
    return glm::normalize(glm::angleAxis(t.x * d2r, glm::vec3{0.f, 0.f, 1.f}) * glm::angleAxis(t.y * d2r, glm::vec3{0.f, 1.f, 0.f}) *
                          glm::angleAxis(t.z * d2r, glm::vec3{1.f, 0.f, 0.f}));
}

[[nodiscard]] glm::vec3 torsoOf(const glm::quat& q)
{
    const aggrip::Offset o = aggrip::fromRigid(Rigid{q, glm::vec3{0.f}});
    return glm::vec3{o.angles.y, o.angles.x, o.angles.z};
}

// A rotation vector (its axis, its length in degrees) and back.
[[nodiscard]] glm::quat rotQuat(const glm::vec3& r)
{
    const float a = glm::length(r);
    return a < 1e-5f ? glm::quat{1.f, 0.f, 0.f, 0.f} : glm::angleAxis(a * (3.14159265f / 180.f), r / a);
}

[[nodiscard]] glm::vec3 rotVec(glm::quat q)
{
    q = glm::normalize(q);
    if(q.w < 0.f)
    {
        q = -q;
    }
    const float a = 2.f * za::acos(za::min(1.f, q.w));
    const float s = za::sqrt(za::max(0.f, 1.f - q.w * q.w));
    return s < 1e-5f ? glm::vec3{0.f} : glm::vec3{q.x, q.y, q.z} / s * (a * (180.f / 3.14159265f));
}

[[nodiscard]] float wrap180(float d)
{
    while(d > 180.f)
    {
        d -= 360.f;
    }
    while(d < -180.f)
    {
        d += 360.f;
    }
    return d;
}

// The chest's box: round its shoulders' middle, turned with it (model space)
[[nodiscard]] Rigid chestFrame(const Note& n)
{
    return Rigid{glm::normalize(n.placed.chest.rot), (n.placed.shoulder[0] + n.placed.shoulder[1]) * 0.5f};
}
constexpr glm::vec3 CHEST_LO{-3.5f, -5.5f, -11.f};
constexpr glm::vec3 CHEST_HI{3.5f, 5.5f, 1.5f};
// The pelvis's box: round its pivot (between the hips), turned with it
constexpr glm::vec3 PELVIS_LO{-3.f, -5.f, -3.f};
constexpr glm::vec3 PELVIS_HI{3.f, 5.f, 2.5f};

// A part's box that the hand's distance is taken from (else a point's: the hands, elbows, feet, knees).
[[nodiscard]] bool boxPart(int p, glm::vec3& lo, glm::vec3& hi)
{
    if(p == PChest)
    {
        lo = CHEST_LO;
        hi = CHEST_HI;
        return true;
    }
    if(p == PPelvis)
    {
        lo = PELVIS_LO;
        hi = PELVIS_HI;
        return true;
    }
    return false;
}

// The point the torso turns about (model space): as sent, else QC's VR_AGP_WAIST fitted to the kind.
[[nodiscard]] glm::vec3 waistOf(const Note& n)
{
    if(n.bodyFlag)
    {
        return n.waist;
    }
    return n.kind == 1 ? glm::vec3{-7.7f, 1.2f, 12.3f} : glm::vec3{-4.7f, 0.9f, 4.8f};
}

// The grip entry part `p` edits (aggrip's hand).
[[nodiscard]] int gripHandOf(const Note& n, int p)
{
    if(p == PWeaponL || (p == PHandL && heldL(n)))
    {
        return aggrip::Off;
    }
    return p == PHandL ? aggrip::Second : aggrip::Main;
}

[[nodiscard]] int slotOf(const Note& n, int gripHand)
{
    return gripHand == aggrip::Off ? n.slotL : n.slot;
}

// The part's world frame: a weapon's (its entity's as placed), a hand's grip frame.
[[nodiscard]] Rigid partFrame(const Note& n, int p)
{
    switch(p)
    {
        case PWeaponR: return toWorld(n.space, n.placed.weapon[0]);
        case PWeaponL: return toWorld(n.space, n.placed.weapon[1]);
        case PHandR: return toWorld(n.space, n.placed.hand[0]);
        case PHandL: return toWorld(n.space, n.placed.hand[1]);
        case PElbowR:
        case PElbowL: return toWorld(n.space, Rigid{glm::normalize(n.placed.chest.rot), n.placed.elbow[sideOf(p)]});
        case PChest: return toWorld(n.space, chestFrame(n));
        default:
        {
            aglegs::Placed L;
            return legsOf(n, L) ? toWorld(n.space, legFrame(L, p)) : toWorld(n.space, chestFrame(n));
        }
    }
}

// A weapon's box in its own frame (units of the model; round its butt, muzzle and second grip), else a small one.
void weaponBox(const Note& n, int p, glm::vec3& lo, glm::vec3& hi)
{
    const int num = p == PWeaponR ? n.prop : n.propL;
    lo = glm::vec3{-4.f, -1.f, -1.f};
    hi = glm::vec3{12.f, 1.f, 1.f};
    glm::vec3 muzzle, butt, second;
    int secondType = 0;
    if(num > 0 && num < cl.num_entities && cl_entities[num].model &&
        view::weaponLocalGeometry(cl_entities[num].model, muzzle, butt, second, secondType))
    {
        lo = glm::min(glm::min(muzzle, butt), glm::vec3{0.f});
        hi = glm::max(glm::max(muzzle, butt), glm::vec3{0.f});
        if(secondType == 1)
        {
            lo = glm::min(lo, second);
            hi = glm::max(hi, second);
        }
    }
    lo -= glm::vec3{WEAPON_PAD};
    hi += glm::vec3{WEAPON_PAD};
}

// How far world point `q` is from part `p` (metres; a weapon's: from its box, biased so a hand near enough wins).
[[nodiscard]] float partDistance(const Note& n, int p, const glm::vec3& q)
{
    const float u2m = 1.f / units::metresToUnits();
    const Rigid f = partFrame(n, p);
    glm::vec3 lo, hi;
    const bool weapon = p == PWeaponR || p == PWeaponL;
    if(weapon || boxPart(p, lo, hi))
    {
        if(weapon)
        {
            weaponBox(n, p, lo, hi);
        }
        const glm::vec3 local = glm::inverse(f.rot) * (q - f.pos) / za::max(n.space.scale, 0.01f);
        const glm::vec3 out = glm::max(glm::max(lo - local, local - hi), glm::vec3{0.f});
        return glm::length(out) * n.space.scale * u2m + (weapon ? WEAPON_BIAS : CHEST_BIAS);
    }
    return glm::length(q - f.pos) * u2m;
}

[[nodiscard]] float partReach(int p)
{
    return p == PWeaponR || p == PWeaponL ? WEAPON_REACH + WEAPON_BIAS
           : p == PChest || p == PPelvis  ? WEAPON_REACH + CHEST_BIAS
                                          : HAND_REACH;
}

// The point a mock hand goes to for part `p`: a box's middle (a weapon's, the chest's, the pelvis's), a hand's grip
// point, an elbow, a foot's ankle, a knee.
[[nodiscard]] glm::vec3 partGrabPoint(const Note& n, int p)
{
    const Rigid f = partFrame(n, p);
    glm::vec3 lo, hi;
    const bool weapon = p == PWeaponR || p == PWeaponL;
    if(weapon || boxPart(p, lo, hi))
    {
        if(weapon)
        {
            weaponBox(n, p, lo, hi);
        }
        return f.pos + f.rot * ((lo + hi) * 0.5f * n.space.scale);
    }
    return f.pos;
}

[[nodiscard]] const char* partName(const Note& n, int p)
{
    switch(p)
    {
        case PWeaponR: return ses.weaponGrip ? "Weapon (right): its grip" : "Weapon (right): its place";
        case PWeaponL: return ses.weaponGrip ? "Weapon (left): its grip" : "Weapon (left): its place";
        case PHandR: return freeHand(n, 0) ? "Right fist" : "Right hand: its grip";
        case PHandL: return freeHand(n, 1) ? (n.prop > 0 ? "Left hand (free)" : "Left fist") : heldL(n) ? "Left hand: its grip"
                                                                                                    : "Left hand (second grip)";
        case PElbowR: return "Right elbow";
        case PElbowL: return "Left elbow";
        case PChest: return "Chest: the body's turn";
        case PPelvis: return "Pelvis: its height, place and turn";
        case PFootPlant: return "Planted foot";
        case PKneeKneel: return "Kneeling knee: its way";
        case PFootKneel: return "Kneeling foot";
        default: return "Planted knee: its way";
    }
}

[[nodiscard]] const char* slotLabel(int slot)
{
    const cvar_t* id = slot >= 0 && slot < weapons::numSlots ? weapons::cvar(slot, weapons::Key::ID) : nullptr;
    return id && id->string && id->string[0] ? COM_SkipPath(id->string) : "?";
}

// --- edits ---------------------------------------------------------------------------------------------------------

void say(const za::String& text)
{
    ses.feedback = text;
    ses.feedbackUntil = realtime + 3.0;
    Con_Printf("Pose Studio: %s\n", text.cStr());
}

void haptic(int hand, float seconds, float amplitude)
{
    if(Backend* be = backend(); be && !vr_disablehaptics.value && hand >= 0 && hand < HAND_COUNT)
    {
        be->haptic(hand, seconds, 160.f, amplitude);
    }
}

void pushUndo()
{
    if(static_cast<int>(ses.undo.size()) >= UNDO_MAX)
    {
        for(int i = 0; i + 1 < static_cast<int>(ses.undo.size()); i++)
        {
            ses.undo[static_cast<za::SizeT>(i)] = ses.undo[static_cast<za::SizeT>(i + 1)];
            ses.undoPoses[static_cast<za::SizeT>(i)] = ses.undoPoses[static_cast<za::SizeT>(i + 1)];
        }
        ses.undo.popBack();
        ses.undoPoses.popBack();
    }
    ses.undo.pushBack(za::String{vr_agrunt_grips.string});
    ses.undoPoses.pushBack(za::String{vr_agrunt_poses.string});
}

// The last snapshot dropped if nothing changed since.
void dropUnchangedUndo()
{
    if(!ses.undo.empty() && ses.undo.back() == za::String{vr_agrunt_grips.string} &&
        ses.undoPoses.back() == za::String{vr_agrunt_poses.string})
    {
        ses.undo.popBack();
        ses.undoPoses.popBack();
    }
}

[[nodiscard]] bool scopeAll()
{
    return vr_agrunt_grip_scope.value == 0.f;
}

void grab(int hand, int p)
{
    const Note* np = current();
    if(!np || !shown(*np, p))
    {
        return;
    }
    const Note& n = *np;
    const hands::State& s = hands::current();
    Grab g;
    g.part = p;
    g.hand = hand;
    g.ent = n.ent;
    g.kind = n.kind;
    if(editsPose(n, p))
    {
        // A pose key: the pose held as picked on the panel (frozen), its key moved from where it is (a leg's: the
        // Pickup's legs' own key, mirrored for a left hand's take)
        g.code = keyCode(n);
        if(isLeg(p))
        {
            aglegs::Placed L;
            g.code = legsOf(n, L) ? agpose::legCode(n.kind, agpose::Pickup) : -1;
            g.legMirror = L.kneel == 0;
        }
        if(g.code < 0 || vr_agrunt_freeze.value == 0.f)
        {
            say(g.code < 0 ? "its pose isn't known yet: wait a moment" : "a pose is edited held: Frozen On first");
            return;
        }
        const Rigid c = controller(s, hand);
        const int side = sideOf(p);
        g.pose = true;
        g.key0 = agpose::value(g.code);
        g.keyNow = g.key0;
        g.rel = compose(inverse(c), partFrame(n, p));
        g.part0 = toModel(n.space, partFrame(n, p));
        g.ctrl0 = toModel(n.space, c);
        g.weaponNow = p == PWeaponL ? n.placed.weapon[1] : n.placed.weapon[0];
        g.shoulder = n.placed.shoulder[side];
        g.wrist = n.placed.hand[side].pos;
        g.waist = waistOf(n);
        g.torso0 = n.torso;
        g.pole0 = n.elbowPole[side];
        g.startPos = grabber(s, hand);
        g.tickPos = grabber(s, hand);
        g.tickRot = c.rot;
        pushUndo();
        ses.grab = g;
        ses.locked = n;
        ses.lockedFrame = host_framecount;
        haptic(hand, 0.03f, 0.6f);
        return;
    }
    g.gripHand = gripHandOf(n, p);
    const int slot = slotOf(n, g.gripHand);
    if(slot < 0)
    {
        say("its weapon has no Weapon Offsets slot: nothing to set");
        return;
    }
    g.slotKey = scopeAll() ? aggrip::allWeapons : slot;
    g.glob = aggrip::toRigid(aggrip::global(n.kind, g.gripHand));
    g.own = aggrip::toRigid(aggrip::offset(n.kind, slot, g.gripHand));
    g.total0 = compose(g.glob, g.own);
    g.total = g.total0;
    const Rigid c = controller(s, hand);
    g.rel = compose(inverse(c), partFrame(n, p));
    g.handHeld = n.placed.hand[p == PWeaponL ? 1 : 0];
    g.weaponHeld = n.placed.weapon[0];
    g.hotspot = compose(n.t.secondAt, inverse(g.total0)); // (Second: secondAt = its point on the weapon * the offset)
    g.weaponNow = p == PWeaponL ? n.placed.weapon[1] : n.placed.weapon[0];
    g.startPos = grabber(s, hand);
    g.tickPos = grabber(s, hand);
    g.tickRot = c.rot;
    pushUndo();
    ses.grab = g;
    ses.locked = n;
    ses.lockedFrame = host_framecount;
    haptic(hand, 0.03f, 0.6f);
}

// A pose key's drag: the key the part's place now asks for (model space: its body's), previewed (QC lays it on next
// frame; a weapon drawn at the controller meanwhile).
void poseDrag(Grab& g, const Note& n, const Rigid& c, const Rigid& partNow)
{
    agpose::Key k = g.key0;
    const int side = sideOf(g.part);
    const int pp = side ? agpose::LPos : agpose::RPos;
    switch(g.part)
    {
        case PWeaponR:
        case PWeaponL:
        {
            // (its place and turn moved as the controller moved it: the key's on top of the pose's)
            g.weaponNow = partNow;
            k.v[pp] = g.key0.v[pp] + (partNow.pos - g.part0.pos);
            const glm::quat dR = glm::normalize(partNow.rot * glm::inverse(g.part0.rot));
            k.v[pp + 1] = rotVec(dR * rotQuat(g.key0.v[pp + 1]));
            break;
        }
        case PHandR:
        case PHandL: k.v[pp] = g.key0.v[pp] + (partNow.pos - g.part0.pos); break;
        case PElbowR:
        case PElbowL:
        {
            // (its pole: the way from its shoulder-to-wrist line to where the elbow is held, in its chest's space; the
            // arm's IK takes it as its elbow's way, its wrist's comfort and its limits still weighed)
            const int pe = side ? agpose::ElbowL : agpose::ElbowR;
            const glm::vec3 d = glm::normalize(g.wrist - g.shoulder);
            glm::vec3 e = partNow.pos - g.shoulder;
            e -= d * glm::dot(e, d);
            if(glm::length(e) > 0.5f)
            {
                const glm::vec3 want = glm::inverse(torsoQuat(g.torso0)) * glm::normalize(e);
                k.v[pe] = want - (g.pole0 - g.key0.v[pe]); // (the sent pole: the pose's own plus the key)
            }
            break;
        }
        case PChest:
        {
            // The chest: turned about its waist as the hand carries the box round it, and twisted as the controller
            // twists (yaw); the turn's change added to the key.
            const glm::vec3 a = g.ctrl0.pos - g.waist, b = toModel(n.space, c).pos - g.waist;
            glm::quat arc{1.f, 0.f, 0.f, 0.f};
            if(glm::length(a) > 2.f && glm::length(b) > 2.f)
            {
                const glm::vec3 an = glm::normalize(a), bn = glm::normalize(b), ax = glm::cross(an, bn);
                if(glm::length(ax) > 1e-5f)
                {
                    arc = glm::angleAxis(za::acos(za::clamp(glm::dot(an, bn), -1.f, 1.f)), glm::normalize(ax));
                }
            }
            const glm::vec3 f = glm::normalize(toModel(n.space, c).rot * glm::inverse(g.ctrl0.rot)) * glm::vec3{1.f, 0.f, 0.f};
            const float twist = glm::length(glm::vec2{f.x, f.y}) > 0.2f ? za::atan2(f.y, f.x) : 0.f;
            const glm::quat R = glm::angleAxis(twist, glm::vec3{0.f, 0.f, 1.f}) * arc;
            const glm::vec3 now = torsoOf(R * torsoQuat(g.torso0));
            glm::vec3 t = g.key0.v[agpose::Torso];
            for(int i = 0; i < 3; i++)
            {
                const float most = i == 0 ? 70.f : 50.f;
                t[i] = za::clamp(t[i] + wrap180(now[i] - g.torso0[i]), -most, most);
            }
            k.v[agpose::Torso] = t;
            break;
        }
        default: break; // (the legs: below)
    }
    if(isLeg(g.part))
    {
        // A leg part: its key moved as the controller moved it (model space = the key's; the kneel's own at its lowest,
        // where the studio holds it): the pelvis's place and turn, a foot's ankle, a knee's pole (the knee goes as far
        // towards it as its leg allows).
        k = g.key0;
        glm::vec3 d = partNow.pos - g.part0.pos;
        if(g.legMirror)
        {
            d.y = -d.y;
        }
        const int lp = legKeyPart(g.part);
        k.v[lp] = g.key0.v[lp] + d;
        if(g.part == PPelvis)
        {
            const glm::vec3 t0 = torsoOf(g.part0.rot), t1 = torsoOf(partNow.rot);
            glm::vec3 dt{wrap180(t1.x - t0.x), wrap180(t1.y - t0.y), wrap180(t1.z - t0.z)};
            if(g.legMirror)
            {
                dt.x = -dt.x;
                dt.z = -dt.z;
            }
            for(int i = 0; i < 3; i++)
            {
                k.v[agpose::LegPelvisTurn][i] =
                    za::clamp(g.key0.v[agpose::LegPelvisTurn][i] + dt[i], -PELVIS_TURN_MOST, PELVIS_TURN_MOST);
            }
        }
    }
    g.keyNow = k;
    agpose::setPreview(g.code, k);
}

// The held part to the controller now: the entry's offset previewed (the drawing follows next frame).
void dragFrame(const hands::State& s)
{
    Grab& g = ses.grab;
    const Note* np = current();
    if(g.part < 0 || !np)
    {
        return;
    }
    const Note& n = *np;
    const Rigid c = controller(s, g.hand);
    const Rigid partNow = toModel(n.space, compose(c, g.rel));
    if(g.pose)
    {
        poseDrag(g, n, c, partNow);
    }
    else switch(g.part)
    {
        case PWeaponR:
        case PWeaponL:
            // (the weapon turned in the hand: the hand held where it was; adjust() draws the weapon here meanwhile)
            g.weaponNow = partNow;
            g.total = compose(inverse(g.handHeld), partNow);
            break;
        case PHandR: g.total = compose(inverse(partNow), n.t.weapon[0]); break;
        default:
            if(g.gripHand == aggrip::Off)
            {
                g.total = compose(inverse(partNow), n.t.weapon[1]);
            }
            else
            {
                // (the second grip's: the hand's frame there on the right weapon, less the grip's point)
                g.total = compose(inverse(g.hotspot), compose(inverse(g.weaponHeld), partNow));
            }
            break;
    }
    if(!g.pose)
    {
        const Rigid entry = g.slotKey == aggrip::allWeapons ? compose(g.total, inverse(g.own)) : compose(inverse(g.glob), g.total);
        aggrip::setPreview(g.kind, g.slotKey, g.gripHand, aggrip::fromRigid(entry));
    }
    if(glm::length(grabber(s, g.hand) - g.tickPos) >= TICK_UNITS || angleDeg(c.rot, g.tickRot) >= TICK_DEG)
    {
        g.tickPos = grabber(s, g.hand);
        g.tickRot = c.rot;
        haptic(g.hand, 0.01f, 0.2f);
    }
}

void release()
{
    Grab& g = ses.grab;
    if(g.part < 0)
    {
        return;
    }
    if(g.pose)
    {
        agpose::clearPreview();
        agpose::set(g.code, g.keyNow);
        const glm::vec3 d = g.keyNow.v[agpose::RPos] - g.key0.v[agpose::RPos] + g.keyNow.v[agpose::LPos] - g.key0.v[agpose::LPos];
        const float turn = glm::length(g.keyNow.v[agpose::RTurn] - g.key0.v[agpose::RTurn]) +
                           glm::length(g.keyNow.v[agpose::LTurn] - g.key0.v[agpose::LTurn]);
        const int cls = (g.code / 1000) % 10, pose = (g.code / 10) % 100;
        const char* what = va("%s, %s%s%s", agpose::poseName(cls, pose), agpose::isStrike(pose) ? agpose::phaseName(g.code % 10) : "",
            agpose::isStrike(pose) ? ", " : "", g.kind == 0 ? "grunts" : "enforcers");
        za::String text{what};
        if(isLeg(g.part))
        {
            const int lp = legKeyPart(g.part);
            const glm::vec3 v = g.keyNow.v[lp], m = g.keyNow.v[lp] - g.key0.v[lp];
            const glm::vec3 t = g.keyNow.v[agpose::LegPelvisTurn];
            say(g.part == PPelvis
                    ? va("pelvis: moved %.2f units, its key %.2f %.2f %.2f, turned %.1f %.1f %.1f (the pickup's legs, %s)",
                          static_cast<double>(glm::length(m)), static_cast<double>(v.x), static_cast<double>(v.y),
                          static_cast<double>(v.z), static_cast<double>(t.x), static_cast<double>(t.y), static_cast<double>(t.z),
                          g.kind == 0 ? "grunts" : "enforcers")
                    : va("%s: moved %.2f units, its key %.2f %.2f %.2f (the pickup's legs, %s)", partName(ses.locked, g.part),
                          static_cast<double>(glm::length(m)), static_cast<double>(v.x), static_cast<double>(v.y),
                          static_cast<double>(v.z), g.kind == 0 ? "grunts" : "enforcers"));
        }
        else if(g.part == PChest)
        {
            const glm::vec3 t = g.keyNow.v[agpose::Torso];
            say(va("chest turned: yaw %.1f pitch %.1f roll %.1f (%s)", static_cast<double>(t.x), static_cast<double>(t.y),
                static_cast<double>(t.z), text.cStr()));
        }
        else if(g.part == PElbowR || g.part == PElbowL)
        {
            const glm::vec3 e = g.keyNow.v[g.part == PElbowR ? agpose::ElbowR : agpose::ElbowL];
            say(va("elbow's pole %.2f %.2f %.2f (%s)", static_cast<double>(e.x), static_cast<double>(e.y), static_cast<double>(e.z),
                text.cStr()));
        }
        else
        {
            say(va("%s: moved %.2f units, turned %.1f deg (%s)", g.part == PWeaponR || g.part == PWeaponL ? "weapon" : "hand",
                static_cast<double>(glm::length(d)), static_cast<double>(turn), text.cStr()));
        }
        haptic(g.hand, 0.02f, 0.3f);
        g = Grab{};
        dropUnchangedUndo();
        return;
    }
    const Rigid entry = g.slotKey == aggrip::allWeapons ? compose(g.total, inverse(g.own)) : compose(inverse(g.glob), g.total);
    aggrip::clearPreview();
    aggrip::set(g.kind, g.slotKey, g.gripHand, aggrip::fromRigid(entry));
    const glm::vec3 d = g.total.pos - g.total0.pos;
    say(va("%s: moved %.2f units, turned %.1f deg in the hand (%s, %s)", g.part == PWeaponR || g.part == PWeaponL ? "weapon" : "hand",
        static_cast<double>(glm::length(d)), static_cast<double>(angleDeg(g.total.rot, g.total0.rot)),
        g.slotKey == aggrip::allWeapons ? "every weapon" : slotLabel(g.slotKey),
        g.gripHand == aggrip::Main ? "main hand" : g.gripHand == aggrip::Off ? "off hand" : "second grip"));
    haptic(g.hand, 0.02f, 0.3f);
    g = Grab{};
    dropUnchangedUndo(); // (nothing changed)
}

void undo()
{
    if(ses.grab.part >= 0)
    {
        return; // (let go first)
    }
    if(ses.undo.empty())
    {
        say("nothing to undo");
        return;
    }
    Cvar_SetQuick(&vr_agrunt_grips, ses.undo.back().cStr());
    Cvar_SetQuick(&vr_agrunt_poses, ses.undoPoses.back().cStr());
    ses.undo.popBack();
    ses.undoPoses.popBack();
    say("undone");
}

// The subject's pose's keys back to the shipped ones (every phase of a strike).
void resetPose()
{
    const Note* n = current();
    const int cls = n ? classOf(n->ent) : -1;
    if(!n || cls < 0)
    {
        say("no armed humanoid near: walk up to one");
        return;
    }
    pushUndo();
    agpose::resetPose(n->kind, cls, ses.pose);
    if(ses.pose == agpose::Pickup)
    {
        agpose::resetLegs(n->kind); // (its legs' key too)
    }
    dropUnchangedUndo();
    say(va("%s: %s back to the shipped pose (%s)", n->kind == 0 ? "grunts" : "enforcers", agpose::poseName(cls, ses.pose),
        agpose::className(cls)));
}

// The subject's kind's pose keys and grips laid on the other kind (scaled by their fits).
void copyKind()
{
    const Note* n = current();
    if(!n)
    {
        say("no armed humanoid near: walk up to one");
        return;
    }
    const int to = n->kind == 0 ? 1 : 0;
    pushUndo();
    const int keys = agpose::copyKind(n->kind, to);
    const int grips = aggrip::copyKind(n->kind, to);
    dropUnchangedUndo();
    say(va("%s' edits copied to the %s: %d pose keys, %d grips (Undo puts theirs back)", n->kind == 0 ? "the grunts" : "the enforcers",
        to == 0 ? "grunts" : "enforcers", keys, grips));
}

// < Pose >, < Key >: the pose the subject is held in.
void stepPose(int by)
{
    const Note* n = current();
    int count = 0;
    const int i = poseIndex(n, &count);
    int poses[agpose::NumPoses];
    subjectPoses(n, poses);
    ses.pose = poses[((i < 0 ? 0 : i + by) % count + count) % count];
    if(vr_agrunt_freeze.value == 0.f)
    {
        Cvar_SetValueQuick(&vr_agrunt_freeze, 1.f); // (a pose is shown held)
    }
    say(va("pose: %s%s", agpose::poseName(n ? classOf(n->ent) : 0, ses.pose),
        ses.pose == agpose::Pickup && aglegs::enabled() ? " (its legs too: pelvis, feet, knees)" : ""));
}

void stepKey(int by)
{
    if(!agpose::isStrike(ses.pose))
    {
        say("a stance has one key: pick a strike to step through its wind-up, blow and follow-through");
        return;
    }
    ses.phase = ((ses.phase + by) % agpose::numPhases + agpose::numPhases) % agpose::numPhases;
    say(va("key: %s", agpose::phaseName(ses.phase)));
}

void cycleGrabSet()
{
    ses.grabSet = (ses.grabSet + 1) % 3;
    say(ses.grabSet == 1 ? "grab: the weapons only" : ses.grabSet == 2 ? "grab: the limbs only (hands, elbows, chest, legs)"
                                                            : "grab: everything (a hand inside a weapon's box first)");
}

void play()
{
    if(!agpose::isStrike(ses.pose))
    {
        say("Play: pick a strike first (< Pose >)");
        return;
    }
    ses.playCount++;
    say("playing the strike in place (no blow dealt)");
}

void resetGrips()
{
    const Note* n = current();
    if(!n)
    {
        say("no armed humanoid near: walk up to one");
        return;
    }
    pushUndo();
    if(scopeAll())
    {
        aggrip::resetEntries(n->kind, aggrip::allWeapons);
        say(va("%s: every weapon's grips back to the defaults", n->kind == 0 ? "grunts" : "enforcers"));
    }
    else
    {
        for(const int slot : {n->slot, n->slotL})
        {
            if(slot >= 0)
            {
                aggrip::resetEntries(n->kind, slot);
            }
        }
        say(va("%s: %s's own grips back to the defaults", n->kind == 0 ? "grunts" : "enforcers", slotLabel(n->slot)));
    }
}

void dumpDone()
{
    za::String text;
    aggrip::dumpText(text);
    Con_Printf("%s", text.cStr());
    const time_t now = time(nullptr);
    const za::String dir = za::String{com_gamedir} + "/proposed_defaults";
    files::createDirectories(dir.cStr());
    za::String path = dir + "/agrunt_poses_" + files::timestamp(now) + ".txt";
    for(int i = 2; files::exists(path.cStr()); i++)
    {
        path = va("%s/agrunt_poses_%s_%d.txt", dir.cStr(), files::timestamp(now).cStr(), i);
    }
    za::String poses;
    agpose::dumpText(poses);
    Con_Printf("%s", poses.cStr());
    za::String file = "// Quake VR: the Pose Studio's grips (Done), rows for Quake/vr/vr_aggrip.cpp's defaults (the Every Weapon\n"
                      "// ones for globalDefaults, the others for defaults); kept meanwhile in vr_agrunt_grips.\n";
    file += text;
    file += "\n// The pose keys, kept meanwhile in vr_agrunt_poses:\n";
    file += poses;
    if(files::writeText(path.cStr(), file.toStringView()))
    {
        Con_Printf("Pose Studio: written to %s\n", path.cStr());
    }
}

void press(int b)
{
    switch(b)
    {
        case BtnScope:
            Cvar_SetValueQuick(&vr_agrunt_grip_scope, scopeAll() ? 1.f : 0.f);
            say(scopeAll() ? "edits move the grip for every weapon of this kind" : "edits nudge this weapon's own grip only");
            break;
        case BtnUndo: undo(); break;
        case BtnReset: resetGrips(); break;
        case BtnFrozen:
            Cvar_SetValueQuick(&vr_agrunt_freeze, vr_agrunt_freeze.value != 0.f ? 0.f : 1.f);
            say(vr_agrunt_freeze.value != 0.f ? "frozen: they hold still" : "unfrozen: watch them move (they may attack)");
            break;
        case BtnCancel: finish(false); break;
        case BtnDone: finish(true); break;
        case BtnPosePrev: stepPose(-1); break;
        case BtnPoseNext: stepPose(1); break;
        case BtnKeyPrev: stepKey(-1); break;
        case BtnKeyNext: stepKey(1); break;
        case BtnPlay: play(); break;
        case BtnResetPose: resetPose(); break;
        case BtnWeaponMode:
            ses.weaponGrip = !ses.weaponGrip;
            say(ses.weaponGrip ? "a weapon's drag turns it in its hand (its grip)" : "a weapon's drag moves it in this pose (the hand follows)");
            break;
        case BtnCopy: copyKind(); break;
        case BtnGrabSet: cycleGrabSet(); break;
        default: break;
    }
}

[[nodiscard]] const char* buttonLabel(int b)
{
    const Note* n = current();
    switch(b)
    {
        case BtnScope: return scopeAll() ? "Every Weapon" : "This Weapon";
        case BtnUndo: return "Undo";
        case BtnReset: return "Reset Grips";
        case BtnFrozen: return vr_agrunt_freeze.value != 0.f ? "Frozen: On" : "Frozen: Off";
        case BtnCancel: return "Cancel";
        case BtnDone: return "Done";
        case BtnPosePrev: return "< Pose";
        case BtnPoseNext: return "Pose >";
        case BtnKeyPrev: return "< Key";
        case BtnKeyNext: return "Key >";
        case BtnPlay: return "Play";
        case BtnResetPose: return "Reset Pose";
        case BtnWeaponMode: return ses.weaponGrip ? "Weapon: Grip" : "Weapon: Pose";
        case BtnGrabSet: return ses.grabSet == 1 ? "Grab: Weapons" : ses.grabSet == 2 ? "Grab: Limbs" : "Grab: Everything";
        default: return n && n->kind == 1 ? "Copy > Grunt" : "Copy > Enforcer";
    }
}

// Whether button `b` does anything now (else drawn dim).
[[nodiscard]] bool buttonLive(int b)
{
    switch(b)
    {
        case BtnKeyPrev:
        case BtnKeyNext:
        case BtnPlay: return agpose::isStrike(ses.pose);
        case BtnCopy:
        case BtnResetPose: return current() != nullptr;
        default: return true;
    }
}

// --- the panel -----------------------------------------------------------------------------------------------------

[[nodiscard]] glm::vec3 buttonAt(int b)
{
    const float m2u = units::metresToUnits();
    const float col = static_cast<float>(buttonPlace[b].col), row = static_cast<float>(buttonPlace[b].row);
    return ses.panelAt + ses.panelRight * (col * (BUTTON_WIDTH + BUTTON_GAP) * m2u) +
           ses.panelUp * ((static_cast<float>(PANEL_ROWS - 1) * 0.5f - row) * (BUTTON_HEIGHT + BUTTON_GAP) * m2u);
}

[[nodiscard]] int buttonAtHand(const glm::vec3& p)
{
    const float m2u = units::metresToUnits();
    for(int b = 0; b < BtnCount; b++)
    {
        const glm::vec3 d = p - buttonAt(b);
        if(za::fabs(glm::dot(d, ses.panelRight)) <= (BUTTON_WIDTH * 0.5f + 0.01f) * m2u &&
            za::fabs(glm::dot(d, ses.panelUp)) <= (BUTTON_HEIGHT * 0.5f + 0.01f) * m2u &&
            za::fabs(glm::dot(d, ses.panelNormal)) <= BUTTON_DEPTH * m2u)
        {
            return b;
        }
    }
    return -1;
}

void placePanel(const hands::State& s)
{
    const float m2u = units::metresToUnits();
    const float yaw = s.headAngles.y + PANEL_SIDE;
    glm::vec3 fwd, right, up;
    hands::angleVectors({0.f, yaw, 0.f}, fwd, right, up);
    ses.panelAt = s.head + fwd * (PANEL_DISTANCE * m2u) - up * (PANEL_DROP * m2u);
    ses.panelRight = right;
    ses.panelUp = up;
    ses.panelNormal = -fwd;
    ses.panelYaw = yaw;
    ses.panelHead = s.head;
}

void drawPanel()
{
    const float m2u = units::metresToUnits();
    const glm::vec3 angles{0.f, ses.panelYaw, 0.f};
    for(int b = 0; b < BtnCount; b++)
    {
        const glm::vec3 at = buttonAt(b);
        const bool hovered = ses.hoverButton[HAND_OFF] == b || ses.hoverButton[HAND_MAIN] == b;
        const bool live = buttonLive(b);
        const glm::vec4 color = hovered ? glm::vec4{1.f, 0.85f, 0.15f, 1.f}
                                : live  ? (b >= BtnPosePrev && b <= BtnKeyNext ? glm::vec4{0.45f, 1.f, 0.6f, 0.85f} : glm::vec4{0.5f, 0.7f, 1.f, 0.8f})
                                        : glm::vec4{0.4f, 0.4f, 0.45f, 0.45f};
        const glm::vec3 r = ses.panelRight * (BUTTON_WIDTH * 0.5f * m2u), u = ses.panelUp * (BUTTON_HEIGHT * 0.5f * m2u);
        const glm::vec3 back = at - ses.panelNormal * 0.15f;
        const glm::vec4 face = hovered ? glm::vec4{0.35f, 0.3f, 0.05f, 0.85f} : glm::vec4{0.04f, 0.06f, 0.1f, 0.8f};
        lines::triangle(back - r - u, back + r - u, back + r + u, face);
        lines::triangle(back - r - u, back + r + u, back - r + u, face);
        const glm::vec3 c[4] = {at - r - u, at + r - u, at + r + u, at - r + u};
        for(int i = 0; i < 4; i++)
        {
            (hovered ? lines::glow : lines::line)(c[i], c[(i + 1) % 4], hovered ? 0.25f : 0.12f, color, color);
        }
        const char* label = buttonLabel(b);
        const float chars = za::max(12.f, static_cast<float>(strlen(label))); // ("Every Weapon")
        const float scale = za::min(BUTTON_WIDTH * 0.85f * m2u / (8.f * chars), BUTTON_HEIGHT * 0.55f * m2u / 8.f);
        text3d::queue(label, at, angles, text3d::Align::Centre, scale);
    }
    za::String text = va("%c%c%c%c %c%c%c%c%c%c", 'P' | 0x80, 'O' | 0x80, 'S' | 0x80, 'E' | 0x80, 'S' | 0x80, 'T' | 0x80,
        'U' | 0x80, 'D' | 0x80, 'I' | 0x80, 'O' | 0x80);
    const Note* n = current();
    if(n)
    {
        const int cls = classOf(n->ent);
        int count = 0;
        const int i = poseIndex(n, &count);
        text += va("\n%s with %s%s%s", n->kind == 0 ? "Grunt" : "Enforcer", n->slot >= 0 ? slotLabel(n->slot) : "its fists",
            n->slotL >= 0 ? " + " : "", n->slotL >= 0 ? slotLabel(n->slotL) : "");
        text += va("\n\x10%s\x11 pose %d of %d", agpose::poseName(cls < 0 ? 0 : cls, ses.pose), i + 1, count);
        if(agpose::isStrike(ses.pose))
        {
            // (its keys, the one shown bright: wind-up, blow, follow-through)
            text += "\nkey:";
            for(int ph = 0; ph < agpose::numPhases; ph++)
            {
                char name[32];
                q_strlcpy(name, agpose::phaseName(ph), sizeof(name));
                for(char* c = name; ph == ses.phase && *c; c++)
                {
                    *c = static_cast<char>(*c | 0x80);
                }
                text += va(" %s", name);
            }
        }
        if(vr_agrunt_freeze.value == 0.f)
        {
            text += "\nunfrozen: live (pose edits need Frozen: On)";
        }
        text += ses.weaponGrip ? (scopeAll() ? va("\nweapon: its grip, every weapon a %s holds", n->kind == 0 ? "grunt" : "enforcer")
                                             : va("\nweapon: its grip, %s's own only", slotLabel(n->slot)))
                               : "\nweapon: where it is in this pose";
    }
    else
    {
        text += "\nwalk up to an armed grunt or enforcer";
    }
    text += "\ngrip a box, move and turn it, let go:";
    text += "\n  weapon, fist, elbow, chest: this pose";
    text += "\n  a hand on a weapon: how it holds it";
    text += va("\nstick click: grab %s", ses.grabSet == 1 ? "weapons only" : ses.grabSet == 2 ? "limbs only" : "everything");
    text += "\nbutton: touch it, pull the trigger";
    text += "\nB/Y: undo     menu button: done";
    int lineCount = 1;
    for(const char* c = text.cStr(); *c; c++)
    {
        lineCount += *c == '\n' ? 1 : 0;
    }
    if(realtime < ses.feedbackUntil)
    {
        text += "\n" + ses.feedback;
        lineCount++;
    }
    constexpr float HINT_SCALE = 0.05f;
    const float half = 8.f * HINT_SCALE * (static_cast<float>(lineCount) * 0.5f + 0.4f);
    const float top = (static_cast<float>(PANEL_ROWS - 1) * 0.5f * (BUTTON_HEIGHT + BUTTON_GAP) + BUTTON_HEIGHT * 0.5f + 0.02f) * m2u;
    text3d::queueOverlay(text, ses.panelAt + ses.panelUp * (top + half), angles, HINT_SCALE);
}

// --- the parts -----------------------------------------------------------------------------------------------------

void box(const Rigid& f, const glm::vec3& lo, const glm::vec3& hi, float scale, float width, const glm::vec4& col)
{
    glm::vec3 c[8];
    for(int i = 0; i < 8; i++)
    {
        const glm::vec3 l{(i & 1) ? hi.x : lo.x, (i & 2) ? hi.y : lo.y, (i & 4) ? hi.z : lo.z};
        c[i] = f.pos + f.rot * (l * scale);
    }
    for(int i = 0; i < 8; i++)
    {
        for(int bit = 1; bit < 8; bit <<= 1)
        {
            if(!(i & bit))
            {
                lines::line(c[i], c[i | bit], width, col, col);
            }
        }
    }
}

[[nodiscard]] glm::vec4 partColour(int p)
{
    switch(p)
    {
        case PWeaponR:
        case PWeaponL: return glm::vec4{0.3f, 0.85f, 1.f, 0.75f};   // cyan
        case PElbowR:
        case PElbowL: return glm::vec4{0.85f, 0.45f, 1.f, 0.8f};    // violet
        case PChest: return glm::vec4{0.55f, 0.9f, 0.45f, 0.55f};   // green, faint
        case PPelvis: return glm::vec4{0.95f, 0.8f, 0.45f, 0.6f};   // tan, faint
        case PFootPlant:
        case PFootKneel: return glm::vec4{1.f, 0.4f, 0.6f, 0.8f};   // pink: the feet
        case PKneeKneel:
        case PKneePlant: return glm::vec4{0.6f, 0.5f, 1.f, 0.8f};   // blue-violet: the knees
        default: return glm::vec4{1.f, 0.55f, 0.15f, 0.8f};          // orange: the hands
    }
}

void drawParts(const Note& n)
{
    const hands::State& s = hands::current();
    for(int p = 0; p < PCount; p++)
    {
        if(!shown(n, p))
        {
            continue;
        }
        const bool held = ses.grab.part == p;
        const bool hovered = !held && (ses.hover[HAND_OFF] == p || ses.hover[HAND_MAIN] == p);
        const bool takeable = inGrabSet(p);
        const glm::vec4 col = held      ? glm::vec4{0.2f, 1.f, 0.35f, 1.f}
                              : hovered ? glm::vec4{1.f, 0.9f, 0.2f, 1.f}
                              : takeable ? partColour(p)
                                         : glm::vec4{0.55f, 0.55f, 0.6f, 0.25f}; // (not taken now: Grab: Weapons / Limbs)
        const float width = held || hovered ? 0.3f : takeable ? 0.2f : 0.1f;
        const Rigid f = partFrame(n, p);
        if(p == PWeaponR || p == PWeaponL)
        {
            glm::vec3 lo, hi;
            weaponBox(n, p, lo, hi);
            box(f, lo, hi, n.space.scale, width, col);
        }
        else if(p == PChest)
        {
            box(f, CHEST_LO, CHEST_HI, n.space.scale, width, col);
        }
        else if(p == PPelvis)
        {
            box(f, PELVIS_LO, PELVIS_HI, n.space.scale, width, col);
        }
        else if(isLeg(p))
        {
            box(f, glm::vec3{-LEG_BOX}, glm::vec3{LEG_BOX}, n.space.scale, width, col);
        }
        else if(p == PElbowR || p == PElbowL)
        {
            box(f, glm::vec3{-ELBOW_BOX}, glm::vec3{ELBOW_BOX}, n.space.scale, width, col);
        }
        else
        {
            box(f, glm::vec3{-HAND_BOX}, glm::vec3{HAND_BOX}, n.space.scale, width, col);
            // (its grip's axes: forward red, up blue: the way the weapon lies in it)
            lines::line(f.pos, f.pos + f.rot * glm::vec3{3.f * n.space.scale, 0.f, 0.f}, 0.15f, glm::vec4{1.f, 0.3f, 0.3f, 0.8f},
                glm::vec4{1.f, 0.3f, 0.3f, 0.8f});
            lines::line(f.pos, f.pos + f.rot * glm::vec3{0.f, 0.f, 2.f * n.space.scale}, 0.15f, glm::vec4{0.3f, 0.5f, 1.f, 0.8f},
                glm::vec4{0.3f, 0.5f, 1.f, 0.8f});
        }
        if(held || hovered)
        {
            const glm::vec3 at = partGrabPoint(n, p) + glm::vec3{0.f, 0.f, 2.5f * n.space.scale};
            text3d::queueOverlay(partName(n, p), at, glm::vec3{0.f, s.headAngles.y, 0.f}, 0.05f);
        }
    }
    // The arms: shoulder, elbow, hand (dim lines)
    const glm::vec4 dim{0.6f, 0.6f, 0.65f, 0.35f};
    for(int side = 0; side < 2; side++)
    {
        const glm::vec3 sh = toWorld(n.space, n.placed.shoulder[side]), el = toWorld(n.space, n.placed.elbow[side]);
        lines::line(sh, el, 0.15f, dim, dim);
        lines::line(el, toWorld(n.space, n.placed.hand[side].pos), 0.15f, dim, dim);
    }
    // The legs (the Pickup's kneel): hip, knee, ankle, toe (dim lines), each knee's pole (a dot, a faint line to it)
    if(aglegs::Placed L; legsOf(n, L))
    {
        const glm::vec4 pole{0.6f, 0.5f, 1.f, 0.4f};
        for(int side = 0; side < 2; side++)
        {
            const glm::vec3 h = toWorld(n.space, L.hip[side]), k = toWorld(n.space, L.knee[side]);
            const glm::vec3 a = toWorld(n.space, L.ankle[side]), t = toWorld(n.space, L.toe[side]);
            lines::line(h, k, 0.15f, dim, dim);
            lines::line(k, a, 0.15f, dim, dim);
            lines::line(a, t, 0.15f, dim, dim);
            const glm::vec3 pw = toWorld(n.space, L.pole[side]);
            lines::line(k, pw, 0.05f, pole, pole);
            lines::point(pw, 0.4f, pole);
        }
    }
    // Each controller near a part: a thin line to it.
    for(int h = 0; h < HAND_COUNT; h++)
    {
        if(ses.hover[h] >= 0 && ses.grab.part < 0 && shown(n, ses.hover[h]))
        {
            const glm::vec4 c{1.f, 0.9f, 0.2f, 1.f};
            lines::line(grabber(s, h), partGrabPoint(n, ses.hover[h]), 0.05f, c, c);
            lines::point(grabber(s, h), 0.5f, c);
        }
        else if(ses.grab.hand != h)
        {
            lines::point(grabber(s, h), 0.3f, glm::vec4{1.f, 1.f, 1.f, 0.7f}); // (where it takes: the drawn hand may be held out)
        }
    }
}

void hoverFrame(const hands::State& s)
{
    const Note* n = current();
    for(int h = 0; h < HAND_COUNT; h++)
    {
        int best = -1;
        float bestD = 1e30f;
        // (the parts in the Grab set; within reach of more than one: a hand or an elbow before a weapon, a weapon before
        // the chest (a hand inside its weapon's box is taken), then the nearest)
        int bestTier = 3;
        for(int p = 0; p < PCount && n; p++)
        {
            if(!shown(*n, p) || !inGrabSet(p))
            {
                continue;
            }
            const float d = partDistance(*n, p, grabber(s, h));
            const int tier = p == PChest || p == PPelvis ? 2 : p == PWeaponR || p == PWeaponL ? 1 : 0;
            if(d <= partReach(p) && (tier < bestTier || (tier == bestTier && d < bestD)))
            {
                best = p;
                bestD = d;
                bestTier = tier;
            }
        }
        if(ses.grab.part >= 0)
        {
            best = ses.grab.hand == h ? ses.grab.part : -1;
        }
        if(best != ses.hover[h] && best >= 0)
        {
            haptic(h, 0.015f, 0.25f);
        }
        ses.hover[h] = best;
        const int b = buttonAtHand(s.pos[h]);
        if(b != ses.hoverButton[h] && b >= 0)
        {
            haptic(h, 0.015f, 0.25f);
        }
        ses.hoverButton[h] = b;
    }
}

// --- commands ------------------------------------------------------------------------------------------------------

void start_f()
{
    start(-1);
}

void done_f()
{
    finish(true);
}

void cancel_f()
{
    finish(false);
}

void undo_f()
{
    undo();
}

// vr_posestudio_press <index>: presses panel button `index` (as a hand's trigger there).
void press_f()
{
    if(ses.active && Cmd_Argc() > 1)
    {
        press(Q_atoi(Cmd_Argv(1)));
    }
}

// vr_posestudio_report: whether it runs, the subject, each part's place (world) and the grips it holds now.
void report_f()
{
    const Note* n = current();
    Con_Printf("posestudio active %d subject %d grab %d hover %d %d undo %d scope %s grabset %d\n", ses.active ? 1 : 0, n ? n->ent : 0,
        ses.grab.part, ses.hover[HAND_OFF], ses.hover[HAND_MAIN], static_cast<int>(ses.undo.size()), scopeAll() ? "all" : "own",
        ses.grabSet);
    if(!n)
    {
        return;
    }
    Con_Printf("posestudio subject kind %d slot %d slotL %d pose \"%s\" held2 %d\n", n->kind, n->slot, n->slotL, n->pose,
        n->placed.held2 ? 1 : 0);
    {
        // (the pose held, its key, the torso and poles as sent, the elbows and the chest as solved)
        const int cls = classOf(n->ent);
        const int code = keyCode(*n);
        const agpose::Key k = agpose::value(code);
        Con_Printf("posestudio pose %d \"%s\" phase %d class %d code %d torso %.1f %.1f %.1f\n", ses.pose,
            agpose::poseName(cls < 0 ? 0 : cls, ses.pose), ses.phase, cls, code, static_cast<double>(n->torso.x),
            static_cast<double>(n->torso.y), static_cast<double>(n->torso.z));
        for(int i = 0; i < agpose::NumParts; i++)
        {
            Con_Printf("posestudio key %d %.2f %.2f %.2f\n", i, static_cast<double>(k.v[i].x), static_cast<double>(k.v[i].y),
                static_cast<double>(k.v[i].z));
        }
        for(int side = 0; side < 2; side++)
        {
            const glm::vec3 e = n->placed.elbow[side], w = n->placed.hand[side].pos, t = n->t.weapon[side].pos;
            Con_Printf("posestudio arm %d elbow %.2f %.2f %.2f hand %.2f %.2f %.2f target %.2f %.2f %.2f pole %.2f %.2f %.2f\n", side,
                static_cast<double>(e.x), static_cast<double>(e.y), static_cast<double>(e.z), static_cast<double>(w.x),
                static_cast<double>(w.y), static_cast<double>(w.z), static_cast<double>(t.x), static_cast<double>(t.y),
                static_cast<double>(t.z), static_cast<double>(n->elbowPole[side].x), static_cast<double>(n->elbowPole[side].y),
                static_cast<double>(n->elbowPole[side].z));
        }
        if(aglegs::Placed L; legsOf(*n, L))
        {
            // (the Pickup's legs as solved, model space, and their key)
            const glm::vec3 pt = torsoOf(L.pelvis.rot);
            Con_Printf("posestudio legs on %.2f w %.2f kneel %d pelvis %.2f %.2f %.2f turn %.1f %.1f %.1f\n", static_cast<double>(L.on),
                static_cast<double>(L.w), L.kneel, static_cast<double>(L.pelvis.pos.x), static_cast<double>(L.pelvis.pos.y),
                static_cast<double>(L.pelvis.pos.z), static_cast<double>(pt.x), static_cast<double>(pt.y), static_cast<double>(pt.z));
            for(int side = 0; side < 2; side++)
            {
                Con_Printf("posestudio leg %d knee %.2f %.2f %.2f ankle %.2f %.2f %.2f pole %.2f %.2f %.2f\n", side,
                    static_cast<double>(L.knee[side].x), static_cast<double>(L.knee[side].y), static_cast<double>(L.knee[side].z),
                    static_cast<double>(L.ankle[side].x), static_cast<double>(L.ankle[side].y), static_cast<double>(L.ankle[side].z),
                    static_cast<double>(L.pole[side].x), static_cast<double>(L.pole[side].y), static_cast<double>(L.pole[side].z));
            }
            const agpose::Key lk = agpose::value(agpose::legCode(n->kind, agpose::Pickup));
            for(int i = 0; i <= agpose::LegPlantKnee; i++)
            {
                Con_Printf("posestudio legkey %d %.2f %.2f %.2f\n", i, static_cast<double>(lk.v[i].x), static_cast<double>(lk.v[i].y),
                    static_cast<double>(lk.v[i].z));
            }
        }
        const glm::vec3 wf = n->t.weapon[0].rot * glm::vec3{1.f, 0.f, 0.f};
        Con_Printf("posestudio weapon0 forward %.3f %.3f %.3f\n", static_cast<double>(wf.x), static_cast<double>(wf.y),
            static_cast<double>(wf.z));
    }
    for(int p = 0; p < PCount; p++)
    {
        if(!shown(*n, p))
        {
            continue;
        }
        const Rigid f = partFrame(*n, p);
        const glm::vec3 gp = partGrabPoint(*n, p);
        Con_Printf("posestudio part %d %s at %.2f %.2f %.2f grab %.2f %.2f %.2f\n", p, partName(*n, p), static_cast<double>(f.pos.x),
            static_cast<double>(f.pos.y), static_cast<double>(f.pos.z), static_cast<double>(gp.x), static_cast<double>(gp.y),
            static_cast<double>(gp.z));
    }
    // (each hand's total grip now: the weapon's frame in the hand's grip frame, as drawn; and as the offsets have it)
    for(const int gh : {static_cast<int>(aggrip::Main), static_cast<int>(aggrip::Off), static_cast<int>(aggrip::Second)})
    {
        const int slot = slotOf(*n, gh);
        if(slot < 0 || (gh == aggrip::Off && !heldL(*n)) || (gh == aggrip::Second && !n->placed.held2))
        {
            continue;
        }
        const Rigid T = compose(aggrip::toRigid(aggrip::global(n->kind, gh)), aggrip::toRigid(aggrip::offset(n->kind, slot, gh)));
        const aggrip::Offset o = aggrip::fromRigid(T);
        Con_Printf("posestudio grip %d total %.2f %.2f %.2f ang %.1f %.1f %.1f\n", gh, static_cast<double>(o.pos.x),
            static_cast<double>(o.pos.y), static_cast<double>(o.pos.z), static_cast<double>(o.angles.x),
            static_cast<double>(o.angles.y), static_cast<double>(o.angles.z));
        if(gh != aggrip::Second)
        {
            const int side = gh == aggrip::Main ? 0 : 1;
            const Rigid drawn = compose(inverse(n->placed.hand[side]), n->placed.weapon[side]);
            Con_Printf("posestudio drawn %d weapon-in-hand %.2f %.2f %.2f, turned %.1f deg from the offsets'\n", gh,
                static_cast<double>(drawn.pos.x), static_cast<double>(drawn.pos.y), static_cast<double>(drawn.pos.z),
                static_cast<double>(angleDeg(drawn.rot, T.rot)));
        }
    }
}

} // namespace

void init()
{
    Cmd_AddCommand("vr_posestudio", start_f);
    Cmd_AddCommand("vr_posestudio_done", done_f);
    Cmd_AddCommand("vr_posestudio_cancel", cancel_f);
    Cmd_AddCommand("vr_posestudio_undo", undo_f);
    Cmd_AddCommand("vr_posestudio_press", press_f);
    Cmd_AddCommand("vr_posestudio_report", report_f);
    agpose::registerCommands();
}

glm::vec3 qcHold(int ent, int kind, int cls)
{
    // (its class noted: the subject's poses listed from it)
    ClassNote* slot = nullptr;
    for(ClassNote& c : ses.classes)
    {
        slot = c.ent == ent ? &c : slot;
    }
    if(!slot)
    {
        slot = &ses.classes[ses.nextClass];
        ses.nextClass = (ses.nextClass + 1) % CLASS_NOTES;
    }
    *slot = ClassNote{ent, kind, cls};
    if(!active())
    {
        return glm::vec3{-1.f, 0.f, 0.f};
    }
    const Note* n = current();
    if(!n || n->ent != ent)
    {
        return glm::vec3{-1.f, 0.f, 0.f};
    }
    int poses[agpose::NumPoses];
    const int count = agpose::posesOf(cls, poses, agpose::NumPoses);
    bool ok = false;
    for(int i = 0; i < count; i++)
    {
        ok = ok || poses[i] == ses.pose;
    }
    if(!ok)
    {
        ses.pose = agpose::Carry; // (another class walked up to: its first pose)
    }
    return glm::vec3{static_cast<float>(ses.pose), static_cast<float>(agpose::isStrike(ses.pose) ? ses.phase : 0),
        static_cast<float>(ses.playCount)};
}

bool active()
{
    return ses.active && ses.world == worldGeneration();
}

bool start(int returnPage)
{
    const auto refuse = [](const char* why) {
        Con_Printf("Pose Studio: %s\n", why);
        return false;
    };
    if(ses.active)
    {
        return false;
    }
    if(!vrActive())
    {
        return refuse("it needs the headset (or vr_backend mock)");
    }
    if(cls.state != ca_connected || !cl.worldmodel || !hands::current().valid)
    {
        return refuse("start a map first");
    }
    if(!agarms::enabled())
    {
        return refuse("turn on Posed Arms first (vr_agarms 1)");
    }
    if(bladeedit::active())
    {
        bladeedit::finish(true);
    }
    if(posing::active())
    {
        posing::stop(false);
    }
    if(holstersetup::active())
    {
        holstersetup::finish(true);
    }
    if(key_dest == key_menu)
    {
        menuui::backToGame(HAND_MAIN);
    }
    Cbuf_AddText("-attack\n-offhandattack\n");
    ses = Session{};
    ses.active = true;
    ses.world = worldGeneration();
    ses.returnPage = returnPage;
    ses.entry = za::String{vr_agrunt_grips.string};
    ses.entryPoses = za::String{vr_agrunt_poses.string};
    ses.freezeWas = vr_agrunt_freeze.value;
    Cvar_SetValueQuick(&vr_agrunt_freeze, 1.f);
    placePanel(hands::current());
    say("walk up to an armed grunt or enforcer; grip a box to move it");
    S_LocalSound("misc/menu3.wav");
    return true;
}

void finish(bool keep)
{
    if(!ses.active)
    {
        return;
    }
    if(ses.grab.part >= 0)
    {
        release();
    }
    aggrip::clearPreview();
    agpose::clearPreview();
    if(!keep)
    {
        Cvar_SetQuick(&vr_agrunt_grips, ses.entry.cStr());
        Cvar_SetQuick(&vr_agrunt_poses, ses.entryPoses.cStr());
    }
    const bool changed = ses.entry != za::String{vr_agrunt_grips.string} || ses.entryPoses != za::String{vr_agrunt_poses.string};
    const int page = ses.returnPage;
    ses.active = false;
    ses.undo.clear();
    ses.undoPoses.clear();
    Cvar_SetValueQuick(&vr_agrunt_freeze, ses.freezeWas);
    Con_Printf("Pose Studio: %s\n", keep ? (changed ? "done" : "done (nothing changed)") : "cancelled, the grips and poses put back");
    if(keep && changed)
    {
        dumpDone();
    }
    S_LocalSound(keep ? "misc/menu2.wav" : "misc/menu1.wav");
    if(page >= 0)
    {
        menu::reopen(page);
    }
}

bool button(int hand, posing::Button b, bool down)
{
    const int i = static_cast<int>(b);
    if(hand < 0 || hand >= HAND_COUNT || i < 0 || i >= 6)
    {
        return false;
    }
    if(!down)
    {
        const bool mine = ses.eaten[hand][i];
        ses.eaten[hand][i] = false;
        if(mine && b == posing::Button::Grip && ses.grab.hand == hand)
        {
            release();
        }
        return mine;
    }
    if(!active() || key_dest != key_game)
    {
        return false;
    }
    // (the walk's buttons pass: A/X)
    if(b == posing::Button::Primary)
    {
        return false;
    }
    ses.eaten[hand][i] = true;
    switch(b)
    {
        case posing::Button::StickClick:
            haptic(hand, 0.03f, 0.5f);
            cycleGrabSet(); // (either stick's click: Grab: Everything / Weapons / Limbs)
            break;
        case posing::Button::Grip:
            if(ses.grab.part < 0 && ses.hover[hand] >= 0)
            {
                grab(hand, ses.hover[hand]);
            }
            break;
        case posing::Button::Trigger:
            if(ses.hoverButton[hand] >= 0)
            {
                haptic(hand, 0.03f, 0.5f);
                press(ses.hoverButton[hand]);
            }
            break;
        case posing::Button::Secondary: undo(); break;
        case posing::Button::Menu: finish(true); break;
        default: break;
    }
    return true;
}

void frame()
{
    if(!ses.active)
    {
        return;
    }
    // Leaving with the map, the game, the headset, the menu opened (the changes kept).
    if(ses.world != worldGeneration() || cls.state != ca_connected || !vrActive() || key_dest == key_menu)
    {
        ses.returnPage = -1;
        finish(true);
        return;
    }
    const hands::State& s = hands::current();
    if(!s.valid)
    {
        return;
    }
    // (the nearest noted, complete: the relink's last pass)
    if(ses.havePending && ses.noteFrame >= host_framecount - 1)
    {
        ses.subject = ses.pending;
        ses.haveSubject = true;
        ses.subjectFrame = ses.noteFrame;
    }
    const float m2u = units::metresToUnits();
    if(glm::length(s.head - ses.panelHead) > PANEL_FOLLOW * m2u && ses.hoverButton[HAND_OFF] < 0 && ses.hoverButton[HAND_MAIN] < 0)
    {
        placePanel(s); // (walking off: it comes along)
    }
    dragFrame(s);
    hoverFrame(s);
    if(const Note* n = current())
    {
        drawParts(*n);
    }
    drawPanel();
}

void adjust(int ent, agarms::Targets& t)
{
    const Grab& g = ses.grab;
    if(!active() || g.part < 0 || g.ent != ent)
    {
        return;
    }
    if(g.part == PWeaponR || g.part == PWeaponL)
    {
        t.weapon[g.part == PWeaponR ? 0 : 1] = g.weaponNow; // (the weapon where the controller holds it)
    }
}

void note(const Note& n)
{
    if(!active())
    {
        return;
    }
    if(n.ent == ses.grab.ent && ses.grab.part >= 0)
    {
        ses.locked = n;
        ses.lockedFrame = host_framecount;
    }
    if(ses.noteFrame != host_framecount)
    {
        ses.noteFrame = host_framecount;
        ses.pendingDist = 1e30f;
        ses.havePending = false;
    }
    const hands::State& s = hands::current();
    const float d = glm::length(n.space.origin - s.head);
    if(d <= vr_posestudio_reach.value * units::metresToUnits() && d < ses.pendingDist)
    {
        ses.pendingDist = d;
        ses.pending = n;
        ses.havePending = true;
    }
}

bool partPoint(int hand, int index, const glm::vec3& offset, glm::vec3& out)
{
    if(!active() || hand < 0 || hand >= HAND_COUNT)
    {
        return false;
    }
    // (the mock moves the drawn hand by the way to the point: its push out of the body added, the controller lands on it)
    const hands::State& s = hands::current();
    const glm::vec3 push = s.pos[hand] - grabber(s, hand);
    if(index == grabStartPart)
    {
        if(ses.grab.part < 0)
        {
            return false;
        }
        out = ses.grab.startPos + offset + push;
        return true;
    }
    const Note* n = current();
    if(!n || index < 0 || index >= PCount || !shown(*n, index))
    {
        return false;
    }
    out = partGrabPoint(*n, index) + offset + push;
    return true;
}

bool buttonPoint(int index, glm::vec3& out)
{
    if(!active() || index < 0 || index >= BtnCount)
    {
        return false;
    }
    out = buttonAt(index);
    return true;
}

} // namespace qvr::posestudio
