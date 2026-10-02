// vr_twohand.cpp -- see vr_twohand.hpp. Ported from the old engine's VR_Do2HAiming.
//
// A weapon is aimed two-handed when the other hand is empty, gripping, between the holding
// hand and the muzzle (plus a margin), roughly along the weapon (vr_2h_angle_threshold, a
// cosine), and either at the weapon's foregrip ("fixed" display mode, most guns: within 5.5
// units to take hold, 20 to keep it) or 5 to 25 units from the holding hand. The aim then points from the holding hand to the helping
// hand (offset by the weapon's 2H offsets), blended in over 0.2 s. With vr_2h_mode 2 ("virtual
// stock"), a holding hand close to the shoulder (vr_virtual_stock_thresh) aims from the
// shoulder instead, mixed by vr_2h_virtual_stock_factor.
//
// Swords (weapon TwoHMode 3) are held the other way round: the helping hand closes below the
// holding hand, on the grip towards the pommel (the "fixed" display mode's grip point), and the
// blade then lies along the line from the helping hand through the holding hand, the holding hand
// leading. The weapon's TwoHPitch/TwoHYaw say which way its blade points in the model (degrees up
// from the model's forward, and to its left); the holding hand is turned (the least turn) until
// the blade, drawn as the view draws it, lies along that line. No virtual stock.
//
// Weapons may have more than one two-handed grip (round 18). A sword has two: the grip below the
// holding hand (above), and its blade towards the tip (weapon key TwoHBladeGrip, a share of the way
// from the hand to the tip): the "half-sword" grip, the blade held across the body with the helping
// hand on it, like a staff. The helping hand takes the nearest grip in reach (the blade: within 6
// units of its outer part); with the blade grip the blade lies along the line from the holding hand
// (at the hilt, leading) through the helping hand, and the helping hand is drawn on the blade
// (bladeGripHand). The server sees two-handed aiming as with the other grip: a level blade held so
// parries and bashes (QC VR_Parry_Blocks) and swings as a two-handed sword.
//
// Hand-off (vr_2h_handoff; the server's side is QC VRTryHandOff): when the holding hand lets go of a
// weapon held two-handed, the helping hand keeps it. A sword simply changes hands. A gun hangs from
// its foregrip (QVR_WPNFLAG_FOREGRIP_CARRIED in the carrying hand's weapon flags): drawn as the hand
// that let go held it, at that moment, moving rigidly with the carrying hand, which stays drawn on the
// foregrip. For this the view records, every frame a hand helps, where the holding hand and the drawn
// helping hand are relative to the helping hand's tracked pose. The empty hand closing on the carried
// gun's handle (HS_CARRIED_GRIP) takes it back.

#include "vr_twohand.hpp"
#include "vr_engine.hpp"
#include "vr_backend.hpp"
#include "vr_body.hpp"
#include "vr_carry2h.hpp"
#include "vr_client.hpp"
#include "vr_cvars.hpp"
#include "vr_handpose.hpp"
#include "vr_held.hpp"
#include "vr_protocol.hpp"
#include "vr_units.hpp"
#include "vr_weapons.hpp"

#include "Zancle/Math/Acos.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sin.hpp"


namespace qvr::twohand
{
namespace
{

using weapons::Key;

enum Wpn2HMode : int
{
    WPN_2H_DEFAULT = 0,
    WPN_2H_NO_VIRTUAL_STOCK = 1,
    WPN_2H_FORBIDDEN = 2,
    WPN_2H_SWORD = 3, // the helping hand below the holding hand, the blade along the hands' line
};

enum Vr2HMode : int
{
    VR_2H_DISABLED = 0,
    VR_2H_BASIC = 1,
    VR_2H_VIRTUAL_STOCK = 2,
};

constexpr int widFist = 0; // QC WID_FIST
constexpr int wpnFlagForegripCarried = 2; // QC QVR_WPNFLAG_FOREGRIP_CARRIED

float aimTransition[2]{0.f, 0.f};   // per holding hand, 0..1
float stockTransition[2]{0.f, 0.f}; // per holding hand, 0..1
bool shouldAim[2]{false, false};    // per holding hand
bool helpingHand[2]{false, false};
bool cupHeld[2]{false, false};        // per holding hand: the other hand holds it by a cup hotspot (flickAllowed)

// A weapon's two-handed grips (round 18): where on it the helping hand may take hold. The nearest
// within reach takes it; once held, that grip holds on until the hand lets go or moves off it.
enum GripKind : int
{
    GRIP_FOREGRIP = 0, // the "fixed" display mode's grip point: a gun's foregrip, a sword's grip below the hand
    GRIP_BLADE = 1,    // a sword's blade towards its tip (weapon key TwoHBladeGrip): the half-sword grip
};
int grip[2]{GRIP_FOREGRIP, GRIP_FOREGRIP}; // per holding hand: the grip held (or last held, while letting go)
float gripLength[2]{0.f, 0.f};             // per holding hand: the hand-to-tip length when the blade was taken

constexpr float foregripTake = 5.5f;  // units from the grip point to take hold
constexpr float foregripKeep = 20.f;  // and to keep it
constexpr float bladeTake = 6.f;      // units from the blade's outer part (TwoHBladeGrip -0.3 .. the tip)
constexpr float bladeKeepMin = 0.25f; // the hands kept this share of the blade's length apart,
constexpr float bladeKeepMax = 1.35f; // at most this

double lastTime = -1.0;

// Sticky grips (vr_2h_sticky*): this frame's multiplier of a held grip's let-go distance and angle, and the swing's
// share of vr_2h_sticky_fast (0..1: the faster hand's speed; eased down over vr_2h_sticky_fast_hold once it slows).
float stickiness = 1.f;
float fastShare = 0.f;
double debugPrintAt = -1.0; // vr_debug_2h_grip 2: the next line

// A pose relative to a hand's tracked pose: a position in its frame, and axes (forward and up).
struct RelPose
{
    glm::vec3 pos{0.f};
    glm::vec3 fwd{1.f, 0.f, 0.f};
    glm::vec3 up{0.f, 0.f, 1.f};
};

// Per helping hand: the last frame it helped, the holding hand's pose (what the weapon is drawn at)
// and the drawn helping hand's, relative to it.
struct HelpRecord
{
    bool valid = false;
    double time = -1.0;
    RelPose holder;
    bool holderMirrored = false;
    RelPose drawnHand;
    bool free = false; // held anywhere (a free grip, or anywhere off the floor): a retake keeps it there (startRetake)
};
HelpRecord help[2];

// Per carrying hand: the record it carries by (taken when the carry began), and the handle.
bool carryWasOn[2]{false, false};
bool carryPoseValid[2]{false, false};
HelpRecord carryPose[2];
bool handleValid[2]{false, false};
glm::vec3 handle[2]{glm::vec3{0.f}, glm::vec3{0.f}};

// Per empty hand: the weapon lying about it is near a hotspot of (vr_weapon_grab_hotspots), and how it would carry it by
// that hotspot (a help record, as if the other hand had held it and let go).
struct GroundSpot
{
    int entity = 0; // 0: none
    int index = -1;
    HelpRecord record;
    int recordIndex = -1; // the hotspot the record is for (kept after the hand takes it)
};
GroundSpot groundSpots[2];

// Weapons held anywhere (vr_weapon_grab_anywhere): per holding (or carrying) hand, the other hand holding its weapon
// anywhere on it (a free grip).
struct FreeGrip
{
    bool on = false;
    int mode = FREE_FOREGRIP;
    int slot = -1;           // the weapon's (held by its handle; -1 carried)
    float t = 0.f;           // blended in (the aim's turn)
    glm::vec3 point{0.f};    // where the other hand took it, in the holding hand's aim frame then (its tracked point)
    bool handPending = true; // the drawn hand's place on it: taken from the first frame drawn
    RelPose hand;            // the other hand as tracked then, in the frame the weapon is drawn from
    carry2h::Hold hold;      // rigid, or carried: both hands' grips (the object: the pose the weapon is drawn from)
    RelPose carrier;         // carried: the carrying hand as drawn then, in that frame
    glm::quat turn{1.f, 0.f, 0.f, 0.f}; // the aim's turn it gave last (in the holding hand's frame): eased out once let go
    double lastOn = -1.0;    // the last frame it held (helpKind: a moment ago)
    bool counts = true;      // two hands on it for the server (aiming): not a weapon with Two-Handed: Not Allowed
};
FreeGrip freeGrips[2];
bool freeCandidate[2]{false, false}; // the view's (setFreeCandidate), at candidateTime
double candidateTime[2]{-1.0, -1.0};
bool grabWas[2]{false, false};
double grabStart[2]{-1.0, -1.0}; // when each hand last started gripping
double carryLast[2]{-1.0, -1.0}; // the last frame each hand carried a weapon
double retakeFrom[2]{-1.0, -1.0}; // the carry (its carryLast) a retake was last tried for (startRetake: once)

// Per holding hand: what it held last frame (its weapon id, -1 none yet; carried off its handle or not). Another weapon
// (taken, drawn, picked up, handed over or off, dropped, thrown, holstered) or another way of holding it starts the
// hand's two-handed state afresh (NOTES.md vrfiringrange_2026-10-01_22-40-50: a sword's blade grip, kept, made the
// next gun's foregrip a blade grip: the helping hand slid along the whole gun, turned as on a blade).
int heldWas[2]{-1, -1};
bool carriedWas[2]{false, false};

[[nodiscard]] RelPose relativeTo(const glm::vec3& basePos, const glm::vec3& baseRot, const glm::vec3& pos,
    const glm::vec3& rot)
{
    glm::vec3 bf, br, bu, f, r, u;
    hands::angleVectors(baseRot, bf, br, bu);
    hands::angleVectors(rot, f, r, u);
    const auto local = [&](const glm::vec3& v) { return glm::vec3{glm::dot(v, bf), glm::dot(v, br), glm::dot(v, bu)}; };
    return RelPose{local(pos - basePos), local(f), local(u)};
}

void fromRelative(const glm::vec3& basePos, const glm::vec3& baseRot, const RelPose& rel, glm::vec3& pos, glm::vec3& rot)
{
    pos = basePos + hands::redirect(rel.pos, baseRot);
    rot = hands::anglesFromVectors(hands::redirect(rel.fwd, baseRot), hands::redirect(rel.up, baseRot));
}
float frameDt = 0.f; // advances once per client frame, however often the hands are recomputed

// A turn as the hands' angles (hands::angleVectors: the columns forward, left, up), and back; as carry2h's frames.
[[nodiscard]] glm::mat3 basisOf(const glm::vec3& a)
{
    glm::vec3 f, r, u;
    hands::angleVectors(a, f, r, u);
    return glm::mat3{f, -r, u};
}
[[nodiscard]] glm::vec3 anglesOfQuat(const glm::quat& q)
{
    const glm::mat3 b = glm::mat3_cast(q);
    return hands::anglesFromVectors(glm::normalize(b[0]), glm::normalize(b[2]));
}
[[nodiscard]] carry2h::Frame frameOf(const glm::vec3& pos, const glm::vec3& rot)
{
    return {pos, glm::normalize(glm::quat_cast(basisOf(rot)))};
}

[[nodiscard]] int weaponId(int hand)
{
    return cl.stats[hand == HAND_MAIN ? protocol::STAT_QVR_WEAPON : protocol::STAT_QVR_WEAPON2];
}

void transition(float& var, bool on, float speed)
{
    var = za::clamp(var + frameDt * (on ? speed : -speed), 0.f, 1.f);
}

// A held grip's stickiness: the global one (vr_2h_sticky*, this frame's) times its hotspot's own (Weapon Offsets:
// Stickiness; 0 in an old config: 1).
[[nodiscard]] float stickinessOf(float own)
{
    return stickiness * (own > 0.f ? own : 1.f);
}

// The two-handed aim's angle threshold (vr_2h_angle_threshold, a cosine) for a grip already held with stickiness
// `stick`: its angle times it (at 180 degrees or more: any).
[[nodiscard]] float heldDot(float threshold, float stick)
{
    if(threshold <= -1.f)
    {
        return -2.f;
    }
    const float angle = za::acos(za::min(threshold, 1.f)) * stick;
    return angle >= glm::pi<float>() ? -2.f : za::cos(angle);
}

// vr_debug_2h_grip: why the helping hand let go of the weapon in `holding` (the check that failed, and its numbers).
void reportLetGo(int holding, const char* why, float value, float limit, float stick)
{
    if(vr_debug_2h_grip.value)
    {
        Con_Printf("2h grip: %s hand let go (%s: %.2f, limit %.2f; stickiness %.2f, swing %.2f)\n",
            holding == HAND_MAIN ? "off" : "main", why, value, limit, stick, fastShare);
    }
}

[[nodiscard]] glm::vec3 safeNormalize(const glm::vec3& v)
{
    const float len = glm::length(v);
    return len > 0.f ? v / len : v;
}

// The world direction of a sword's blade held at `rot` in `hand`, as the view draws the weapon
// (vr_view.cpp setupWeapon: the hand's angles plus the weapon's, the pitch negated for alias
// models, mirrored in the off hand).
[[nodiscard]] glm::vec3 bladeDirection(int slot, int hand, const glm::vec3& rot)
{
    const bool mirrored = hand == HAND_OFF;
    glm::vec3 o = weapons::vec(slot, Key::Pitch, Key::Yaw, Key::Roll);
    o.x += vr_gunmodelpitch.value;
    if(mirrored)
    {
        o.y = -o.y;
        o.z = -o.z;
    }

    const float pitch = glm::radians(weapons::value(slot, Key::TwoHPitch));
    const float yaw = glm::radians(weapons::value(slot, Key::TwoHYaw));
    glm::vec3 d{za::cos(pitch) * za::cos(yaw), za::cos(pitch) * za::sin(yaw), za::sin(pitch)};
    if(mirrored)
    {
        d.y = -d.y;
    }

    float m[16];
    vec3_t origin{0.f, 0.f, 0.f};
    vec3_t angles{-rot.x + o.x, rot.y + o.y, rot.z + o.z};
    R_EntityMatrix(m, origin, angles, ENTSCALE_DEFAULT);
    return safeNormalize(glm::vec3{m[0] * d.x + m[4] * d.y + m[8] * d.z, m[1] * d.x + m[5] * d.y + m[9] * d.z,
        m[2] * d.x + m[6] * d.y + m[10] * d.z});
}

// `rot` turned by the least rotation taking `from` to `to` (unit vectors).
[[nodiscard]] glm::vec3 turnAngles(const glm::vec3& rot, const glm::vec3& from, const glm::vec3& to)
{
    const glm::vec3 axis = glm::cross(from, to);
    const float s = glm::length(axis);
    const float c = glm::dot(from, to);
    if(s < 1e-6f)
    {
        return rot; // aligned (or exactly opposite: never asked for)
    }
    const glm::vec3 k = axis / s;
    const auto turn = [&](const glm::vec3& v) {
        return v * c + glm::cross(k, v) * s + k * glm::dot(k, v) * (1.f - c); // Rodrigues
    };

    glm::vec3 fwd, right, up;
    hands::angleVectors(rot, fwd, right, up);
    return hands::anglesFromVectors(turn(fwd), turn(up));
}

[[nodiscard]] float segmentDistance(const glm::vec3& p, const glm::vec3& a, const glm::vec3& b)
{
    const glm::vec3 ab = b - a;
    const float len2 = glm::dot(ab, ab);
    const float t = len2 > 0.f ? za::clamp(glm::dot(p - a, ab) / len2, 0.f, 1.f) : 0.f;
    return glm::distance(p, a + ab * t);
}

void applySword(hands::State& s, const glm::vec3 (&originalRots)[2], int holding, int helping, int slot)
{
    const glm::vec3 holdingPos = s.pos[holding];
    glm::vec3 helpingPos = s.pos[helping];
    glm::vec3 off = weapons::vec(slot, Key::TwoHOffsetX, Key::TwoHOffsetY, Key::TwoHOffsetZ);
    if(holding == HAND_OFF)
    {
        off.y = -off.y;
    }
    helpingPos += hands::redirect(off, originalRots[holding]);

    const glm::vec3 blade = bladeDirection(slot, holding, originalRots[holding]);
    // (An empty hand: no weapon, and not carrying a box either: held::handEmpty.)
    const bool canGrab = client::grabbing(helping) && held::handEmpty(helping);
    const bool wasHeld = shouldAim[holding];

    // The grips (round 18). The grip point below the holding hand (GRIP_FOREGRIP): the blade along the
    // line from the helping hand through the holding hand. Unlike a gun's, it holds when the blade
    // touches a wall or the floor (a swing's end): the hands are kept out of walls anyway, and
    // letting go would turn the sword back mid-swing. The blade towards the tip (GRIP_BLADE, the
    // half-sword grip): the blade along the line from the holding hand (at the hilt) through the
    // helping hand, wherever the holding hand's wrist points it.
    const float foreDist =
        s.grip2HValid[holding] ? glm::distance(s.grip2HPalm[holding] ? hands::palmPoint(s, helping) : s.pos[helping], s.grip2H[holding]) - s.grip2HBias[holding] : 1e9f;
    // The blade's hotspot (round 21: its grip's middle, a share of the way from the hand to the tip; its bias).
    float bladeAt = 0.f, bladeFrom = 0.f, bladeTo = 0.f, bladeBias = 0.f, bladeSticky = 0.f;
    for(int i = 0; i < weapons::maxHotspots; i++)
    {
        if(const weapons::Hotspot h = weapons::hotspot(slot, i); h.type == weapons::HotspotType::Blade)
        {
            bladeAt = h.pos.x;
            bladeFrom = weapons::bladeFrom(h);
            bladeTo = weapons::bladeTo(h);
            bladeBias = h.bias;
            bladeSticky = h.sticky;
            break;
        }
    }
    const float length = s.muzzleValid[holding] ? glm::distance(holdingPos, s.muzzle[holding]) : 0.f;
    float bladeDist = 1e9f;
    if(bladeAt > 0.f && length > 8.f)
    {
        const glm::vec3 toTip = s.muzzle[holding] - holdingPos;
        bladeDist =
            segmentDistance(s.pos[helping], holdingPos + toTip * bladeFrom, holdingPos + toTip * bladeTo) - bladeBias;
    }

    bool held = false;
    if(canGrab && shouldAim[holding])
    {
        // Held: the grip holds on (as far as its Stickiness and vr_2h_sticky* let it).
        if(grip[holding] == GRIP_BLADE)
        {
            const float apart = glm::distance(holdingPos, s.pos[helping]);
            const float stick = stickinessOf(bladeSticky);
            const float most = gripLength[holding] * bladeKeepMax * stick;
            held = apart > gripLength[holding] * bladeKeepMin / stick && apart < most;
            if(!held)
            {
                reportLetGo(holding, "the blade's hands apart", apart, most, stick);
            }
        }
        else
        {
            const float keep = foregripKeep * stickinessOf(s.grip2HSticky[holding]);
            held = foreDist < keep;
            if(!held)
            {
                reportLetGo(holding, "distance", foreDist, keep, stickinessOf(s.grip2HSticky[holding]));
            }
        }
    }
    else if(wasHeld)
    {
        reportLetGo(holding, "the grip let go", 0.f, 0.f, stickiness);
    }
    else if(canGrab)
    {
        // Taking hold: the nearest grip within reach.
        if(bladeDist < bladeTake && bladeDist < foreDist)
        {
            grip[holding] = GRIP_BLADE;
            gripLength[holding] = length;
            held = true;
        }
        else if(foreDist < foregripTake)
        {
            grip[holding] = GRIP_FOREGRIP;
            held = true;
        }
    }

    const glm::vec3 line = grip[holding] == GRIP_BLADE ? safeNormalize(s.pos[helping] - holdingPos) // hilt to tip
                                                       : safeNormalize(holdingPos - helpingPos);    // pommel to blade
    // The blade grip is on the blade already: the wrist may point it anywhere.
    const float stick = stickinessOf(grip[holding] == GRIP_BLADE ? bladeSticky : s.grip2HSticky[holding]);
    const float dotNeed = wasHeld ? heldDot(vr_2h_angle_threshold.value, stick) : vr_2h_angle_threshold.value;
    const bool goodDot = grip[holding] == GRIP_BLADE || dotNeed <= -1.f || glm::dot(line, blade) > dotNeed;
    if(wasHeld && held && !goodDot)
    {
        reportLetGo(holding, "angle (cosine)", glm::dot(line, blade), dotNeed, stick);
    }

    shouldAim[holding] = held && goodDot;
    helpingHand[helping] = shouldAim[holding];
    transition(aimTransition[holding], shouldAim[holding], 5.f);
    stockTransition[holding] = 0.f;
    if(vr_debug_2h_grip.value && !wasHeld && shouldAim[holding])
    {
        Con_Printf("2h grip: %s hand took the %s (%.1f units off it)\n", holding == HAND_MAIN ? "off" : "main",
            grip[holding] == GRIP_BLADE ? "blade" : "grip below the hand",
            grip[holding] == GRIP_BLADE ? bladeDist : foreDist);
    }
    if(vr_debug_2h_grip.value >= 2 && shouldAim[holding] && grip[holding] == GRIP_BLADE && length > 0.f &&
        vr_gametime >= debugPrintAt)
    {
        // Where along the blade the helping hand holds it: a share of the way from the holding hand to the tip.
        const glm::vec3 toTip = s.muzzle[holding] - holdingPos;
        Con_Printf("2h grip: on the blade at %.2f of the way to the tip\n",
            glm::dot(s.pos[helping] - holdingPos, toTip) / glm::dot(toTip, toTip));
        debugPrintAt = vr_gametime + 0.25;
    }

    const float t = aimTransition[holding];
    if(t <= 0.f)
    {
        return;
    }

    // The weapon's angles add to the hand's as Euler angles, so the blade's turn with the hand is
    // not quite rigid: a few passes settle it.
    const glm::vec3 target = safeNormalize(glm::mix(blade, line, t));
    glm::vec3 rot = originalRots[holding];
    for(int pass = 0; pass < 3; pass++)
    {
        rot = turnAngles(rot, bladeDirection(slot, holding, rot), target);
    }
    s.rot[holding] = rot;
}

// ---------------------------------------------------------------------------------------------------------------------
// Weapons held anywhere (vr_weapon_grab_anywhere; ROUND21.md, "Weapons held anywhere").

// The free grip's mode for the weapon of `slot`: its own Other Hand Anywhere, else vr_weapon_anygrip_mode. A weapon the
// other hand may not aim (Two-Handed: Not Allowed) is only supported.
[[nodiscard]] bool twoHandsForbidden(int slot)
{
    return slot >= 0 && static_cast<int>(weapons::value(slot, Key::TwoHMode)) == WPN_2H_FORBIDDEN;
}

[[nodiscard]] int freeModeFor(int slot)
{
    if(slot < 0 || static_cast<int>(weapons::value(slot, Key::TwoHMode)) == WPN_2H_FORBIDDEN)
    {
        return FREE_SUPPORT;
    }
    const float own = weapons::value(slot, Key::AnyGripMode);
    const int mode = static_cast<int>(own >= 0.f ? own : vr_weapon_anygrip_mode.value);
    return mode < FREE_FOREGRIP || mode > FREE_RIGID ? FREE_FOREGRIP : mode;
}

[[nodiscard]] const char* freeModeName(int mode)
{
    return mode == FREE_SUPPORT ? "support" : mode == FREE_RIGID ? "rigid" : "foregrip";
}

// How far (units) a free grip's hand may be off its place on the weapon before it lets go, as a prop held in both hands
// (vr_carry_two_hands_drift + _detach), times the grips' stickiness.
[[nodiscard]] float freeKeep()
{
    return (za::max(vr_carry_two_hands_drift.value, 0.f) + za::max(vr_carry_two_hands_detach.value, 0.f)) * 0.01f *
           units::metresToUnits() * stickinessOf(1.f);
}

// The view says the hand is on the other hand's weapon, this frame or the last few.
[[nodiscard]] bool candidate(int hand)
{
    return freeCandidate[hand] && vr_gametime - candidateTime[hand] < 0.25;
}

// A grip that may take hold anywhere: just started (not slid onto the weapon gripping), or the hand carried the weapon a
// moment ago (the other hand took its handle: this one goes on holding it where it is).
[[nodiscard]] bool freshGrab(int hand)
{
    return vr_gametime - grabStart[hand] < 0.3 || (!carrying(hand) && vr_gametime - carryLast[hand] < 0.5);
}

[[nodiscard]] carry2h::Frame handsFrame(const hands::State& s, const glm::vec3 (&rots)[2], int h)
{
    return frameOf(s.pos[h], rots[h]);
}

void endFree(int holding, const char* why, float value, float limit)
{
    FreeGrip& g = freeGrips[holding];
    if(g.on && vr_debug_2h_grip.value)
    {
        Con_Printf("2h grip: %s hand let go of the weapon held anywhere (%s: %.1f, limit %.1f)\n",
            holding == HAND_MAIN ? "off" : "main", why, value, limit);
    }
    g.on = false;
}

// The hand `holding` holds another weapon, or holds it another way (heldWas): nothing of the last one's grips carries
// over (the grip kind, the blade's length, the aim's and the stock's transitions, a free grip and its eased-out turn).
// Not the helping hand's own history (help, carryPose, carryLast: a retake and a hand-off read it as the weapon moves).
void resetHolding(int holding, int was, int now)
{
    if(vr_debug_2h_grip.value && (aimTransition[holding] > 0.f || grip[holding] != GRIP_FOREGRIP || freeGrips[holding].on ||
                                     freeGrips[holding].t > 0.f))
    {
        Con_Printf("2h grip: %s hand's grips reset (weapon %d -> %d)\n", holding == HAND_MAIN ? "main" : "off", was, now);
    }
    endFree(holding, "another weapon", 0.f, 0.f);
    freeGrips[holding] = FreeGrip{};
    aimTransition[holding] = stockTransition[holding] = 0.f;
    shouldAim[holding] = false;
    grip[holding] = GRIP_FOREGRIP;
    gripLength[holding] = 0.f;
}

// The other hand (`helping`) holding the weapon in `holding` (by its handle) anywhere: held on (false once it lets go),
// the aim turned as its mode says.
bool updateFree(hands::State& s, const glm::vec3 (&originalRots)[2], int holding, int helping, int slot)
{
    FreeGrip& g = freeGrips[holding];
    if(slot != g.slot)
    {
        endFree(holding, "another weapon", 0.f, 0.f);
        return false;
    }
    const glm::vec3 holdingPos = s.pos[holding];
    const glm::vec3& rot0 = originalRots[holding];

    // The aim it gives, and how far the hand is off its place on the weapon so aimed.
    glm::vec3 aimRot = rot0;
    float off = 0.f, limit = freeKeep();
    if(g.mode == FREE_RIGID)
    {
        const carry2h::Frame both[2]{handsFrame(s, originalRots, HAND_OFF), handsFrame(s, originalRots, HAND_MAIN)};
        const carry2h::Frame object = carry2h::solve(g.hold, both);
        aimRot = anglesOfQuat(object.rot);
        off = glm::distance(s.pos[helping], carry2h::onGrip(g.hold, object, helping).pos);
    }
    else if(g.mode == FREE_FOREGRIP)
    {
        // The least turn putting where it took hold on the line from the holding hand through it: a foregrip there.
        const glm::vec3 have = hands::redirect(g.point, rot0);
        const glm::vec3 want = s.pos[helping] - holdingPos;
        if(glm::length(want) > 2.f)
        {
            aimRot = turnAngles(rot0, safeNormalize(have), safeNormalize(want));
        }
        off = glm::distance(s.pos[helping], holdingPos + hands::redirect(g.point, aimRot));
        limit = foregripKeep * stickinessOf(1.f);
    }
    else
    {
        off = glm::distance(s.pos[helping], holdingPos + hands::redirect(g.point, rot0));
    }

    if(!client::grabbing(helping) || !held::handEmpty(helping))
    {
        endFree(holding, "the grip let go", 0.f, 0.f);
        return false;
    }
    if(off > limit)
    {
        endFree(holding, "distance", off, limit);
        return false;
    }

    g.lastOn = vr_gametime;
    helpingHand[helping] = true;
    shouldAim[holding] = false;
    transition(aimTransition[holding], false, 5.f);
    stockTransition[holding] = 0.f;
    transition(g.t, true, 5.f);
    if(g.mode != FREE_SUPPORT && g.t > 0.f)
    {
        const glm::quat from = glm::quat_cast(basisOf(rot0)), to = glm::quat_cast(basisOf(aimRot));
        g.turn = glm::normalize(glm::inverse(from) * to);
        s.rot[holding] = anglesOfQuat(glm::slerp(from, to, g.t));
    }
    return true;
}

// The other hand (`helping`) taking hold of the weapon in `holding` (by its handle) anywhere: an empty hand starting to
// grip on it (the view's candidate: away from its handle and hotspots), not taking a hotspot.
bool startFree(const hands::State& s, const glm::vec3 (&originalRots)[2], int holding, int helping, int slot)
{
    if(!vr_weapon_grab_anywhere.value || slot < 0 || shouldAim[holding] || aimTransition[holding] > 0.f ||
        !client::grabbing(helping) || !held::handEmpty(helping) || carrying(helping) || !candidate(helping) ||
        !freshGrab(helping))
    {
        return false;
    }
    FreeGrip& g = freeGrips[holding];
    g = FreeGrip{};
    g.on = true;
    g.slot = slot;
    g.mode = freeModeFor(slot);
    g.counts = !twoHandsForbidden(slot);
    g.point = relativeTo(s.pos[holding], originalRots[holding], s.pos[helping], originalRots[helping]).pos;
    if(g.mode == FREE_FOREGRIP && glm::length(g.point) < 3.f)
    {
        g.mode = FREE_SUPPORT; // (at the handle: nothing to aim by)
    }
    const carry2h::Frame both[2]{handsFrame(s, originalRots, HAND_OFF), handsFrame(s, originalRots, HAND_MAIN)};
    g.hold = carry2h::record(frameOf(s.pos[holding], originalRots[holding]), both);
    g.lastOn = vr_gametime;
    if(vr_debug_2h_grip.value)
    {
        Con_Printf("2h grip: %s hand took the weapon anywhere (%s), %.1f units from its handle (%.1f %.1f %.1f)\n",
            holding == HAND_MAIN ? "off" : "main", freeModeName(g.mode), glm::length(g.point), g.point.x, g.point.y,
            g.point.z);
    }
    return true;
}

// The other hand (`helping`) carried this weapon anywhere a moment ago and `holding` has just taken its handle (QC
// VRTakeCarriedWeapon): it goes on holding it where it held it (NOTES.md vrfiringrange_2026-10-01_17-13-41), even though
// the weapon jumps to the taking hand's pose (taken up to 6 units off the handle, turned as that hand is), as long as
// its place on the weapon is still within the hotspots' keep (20 units, times the stickiness) of the hand: as a
// foregrip, after the aim's turn (updateFree's own check: only the hands' distance apart counts); supported or rigid,
// as the weapon is drawn now. Tried once a carry. Drawn on its place on the weapon as it carried it.
bool startRetake(const hands::State& s, const glm::vec3 (&originalRots)[2], int holding, int helping, int slot)
{
    if(!vr_weapon_grab_anywhere.value || slot < 0 || shouldAim[holding] || aimTransition[holding] > 0.f ||
        !client::grabbing(helping) || !held::handEmpty(helping) || carrying(helping) || !carryPoseValid[helping] ||
        !carryPose[helping].free || vr_gametime - carryLast[helping] >= 0.5 || retakeFrom[helping] == carryLast[helping])
    {
        return false;
    }
    retakeFrom[helping] = carryLast[helping];
    const HelpRecord& r = carryPose[helping];

    // Its tracked place in the frame the weapon was drawn from as it carried it (the hand's pose then and now are the
    // same: the record is relative to it), and its drawn pose there.
    glm::vec3 holderPos, holderRot, drawnPos, drawnRot;
    fromRelative(s.pos[helping], s.rot[helping], r.holder, holderPos, holderRot);
    fromRelative(s.pos[helping], s.rot[helping], r.drawnHand, drawnPos, drawnRot);
    glm::vec3 place = relativeTo(holderPos, holderRot, s.pos[helping], s.rot[helping]).pos;
    RelPose hand = relativeTo(holderPos, holderRot, drawnPos, drawnRot);
    // Drawn now from the holding hand's pose (held as that hand holds weapons): mirrored the other way, its place too.
    const bool mirrorChange = r.holderMirrored != (holding == HAND_OFF);
    if(mirrorChange)
    {
        place.y = -place.y;
    }

    const int mode = freeModeFor(slot);
    const glm::vec3 placeNow = s.pos[holding] + hands::redirect(place, originalRots[holding]);
    const float keep = foregripKeep * stickinessOf(1.f);
    if(mode != FREE_FOREGRIP && glm::distance(s.pos[helping], placeNow) > keep)
    {
        if(vr_debug_2h_grip.value)
        {
            Con_Printf("2h grip: %s hand let go as the other hand took the handle (%.1f units off its place, limit %.1f)\n",
                helping == HAND_MAIN ? "main" : "off", glm::distance(s.pos[helping], placeNow), keep);
        }
        return false;
    }

    FreeGrip& g = freeGrips[holding];
    g = FreeGrip{};
    g.on = true;
    g.slot = slot;
    g.mode = mode;
    g.counts = !twoHandsForbidden(slot);
    // As a foregrip, its place on the weapon (the aim turns it onto the hand); otherwise where the hand is now (what lets
    // go of it is how far it then moves).
    g.point = mode == FREE_FOREGRIP ? place
                                    : relativeTo(s.pos[holding], originalRots[holding], s.pos[helping], originalRots[helping]).pos;
    if(g.mode == FREE_FOREGRIP && glm::length(g.point) < 3.f)
    {
        g.mode = FREE_SUPPORT;
    }
    g.handPending = mirrorChange; // (mirrored the other way: drawn where it is)
    g.hand = hand;
    const carry2h::Frame both[2]{handsFrame(s, originalRots, HAND_OFF), handsFrame(s, originalRots, HAND_MAIN)};
    g.hold = carry2h::record(frameOf(s.pos[holding], originalRots[holding]), both);
    g.lastOn = vr_gametime;
    if(vr_debug_2h_grip.value)
    {
        Con_Printf("2h grip: %s hand goes on holding the weapon anywhere (%s) as the other hand took its handle, %.1f units "
                   "from it, %.1f units off its place\n",
            holding == HAND_MAIN ? "off" : "main", freeModeName(g.mode), glm::length(place),
            glm::distance(s.pos[helping], placeNow));
    }
    return true;
}

// The other hand on a weapon `holding` carries off its handle (defined after carriedPose).
void applyCarried(hands::State& s, int holding, int helping);

void applyHotspots(hands::State& s, const glm::vec3 (&originalRots)[2], int holding, int helping, int mode)
{
    const int slot = weapons::heldSlot(holding);
    const bool holdingWeapon = slot >= 0 && weaponId(holding) != widFist;

    if(holdingWeapon && static_cast<int>(weapons::value(slot, Key::TwoHMode)) == WPN_2H_SWORD)
    {
        applySword(s, originalRots, holding, helping, slot);
        return;
    }

    // Not a sword: no blade grip (bladeGrip would draw the helping hand along the whole weapon).
    grip[holding] = GRIP_FOREGRIP;
    const glm::vec3 holdingPos = s.pos[holding];

    // The helping hand's grip point, in the holding hand's frame (mirrored for the off hand).
    glm::vec3 helpingPos = s.pos[helping];
    if(holdingWeapon)
    {
        glm::vec3 off = weapons::vec(slot, Key::TwoHOffsetX, Key::TwoHOffsetY, Key::TwoHOffsetZ);
        if(holding == HAND_OFF)
        {
            off.y = -off.y;
        }
        helpingPos += hands::redirect(off, originalRots[holding]);
    }

    const glm::vec3 handDiff = helpingPos - holdingPos;
    const glm::vec3 handDir = safeNormalize(handDiff);

    glm::vec3 shoulderOffsets{vr_shoulder_offset_x.value, vr_shoulder_offset_y.value, vr_shoulder_offset_z.value};
    if(holding == HAND_OFF)
    {
        shoulderOffsets.y = -shoulderOffsets.y;
    }
    const glm::vec3 shoulder = body::chestAnchor(s, shoulderOffsets);
    const glm::vec3 averageDir =
        safeNormalize(glm::mix(handDiff, helpingPos - shoulder, vr_2h_virtual_stock_factor.value));

    glm::vec3 origDir, right, up;
    hands::angleVectors(originalRots[holding], origDir, right, up);

    const int wpnMode = holdingWeapon ? static_cast<int>(weapons::value(slot, Key::TwoHMode)) : WPN_2H_FORBIDDEN;

    const bool useStock = glm::distance(shoulder, holdingPos) < vr_virtual_stock_thresh.value &&
                          mode == VR_2H_VIRTUAL_STOCK && wpnMode != WPN_2H_NO_VIRTUAL_STOCK;
    transition(stockTransition[holding], useStock, 5.f);

    const float handDist = glm::distance(holdingPos, helpingPos);

    // "Fixed" display mode (most guns): the hand must come to the weapon's foregrip, and may
    // then move a little further before letting go. Otherwise anywhere 5-25 units away.
    // Held, it may go further (its hotspot's Stickiness, and vr_2h_sticky*: more while swinging).
    const bool wasHeld = shouldAim[holding];
    const bool fixedMode = holdingWeapon && s.grip2HValid[holding];
    const float gripDist = fixedMode ? glm::distance(s.grip2HPalm[holding] ? hands::palmPoint(s, helping) : s.pos[helping], s.grip2H[holding]) -
                                           s.grip2HBias[holding]
                                     : 0.f;
    const float stick = wasHeld ? stickinessOf(fixedMode ? s.grip2HSticky[holding] : 1.f) : 1.f;
    const float keep = wasHeld ? foregripKeep * stick : foregripTake;
    const bool goodDistance = fixedMode ? gripDist < keep : handDist > 5.f && handDist < 25.f;

    // Muzzles move with the firing animation, hence the margin.
    const float muzzleDist =
        s.muzzleValid[holding] ? glm::distance(holdingPos, s.muzzle[holding]) + 7.5f * stick : 0.f;
    const bool beforeMuzzle = !s.muzzleValid[holding] || handDist <= muzzleDist;

    const bool canGrab = client::grabbing(helping) && wpnMode != WPN_2H_FORBIDDEN &&
                         held::handEmpty(helping) && beforeMuzzle && !handpose::gunColliding(holding);
    // A cup (a two-handed pistol grip) is held wherever the hands point: it doesn't aim.
    const bool cup = fixedMode && s.grip2HCup[holding];
    const float dotNeed = wasHeld ? heldDot(vr_2h_angle_threshold.value, stick) : vr_2h_angle_threshold.value;
    const bool goodDot = cup || dotNeed <= -1.f || glm::dot(handDir, origDir) > dotNeed;

    shouldAim[holding] = canGrab && goodDistance && goodDot;
    cupHeld[holding] = shouldAim[holding] && fixedMode && s.grip2HPalm[holding]; // (a cup hotspot itself: grip2HPalm)
    if(vr_debug_2h_grip.value && fixedMode && !wasHeld && shouldAim[holding])
    {
        Con_Printf("2h grip: %s hand took it (%.1f units off its grip)\n", holding == HAND_MAIN ? "off" : "main", gripDist);
    }
    if(vr_debug_2h_grip.value >= 2 && fixedMode && vr_gametime >= debugPrintAt)
    {
        // Where its grip is from the helping hand (units, world), to place a test's hand on it.
        const glm::vec3 d = s.grip2H[holding] - (s.grip2HPalm[holding] ? hands::palmPoint(s, helping) : s.pos[helping]);
        Con_Printf("2h grip: %s hand %.1f units off the grip (%.1f %.1f %.1f), held %d, keep %.1f\n",
            holding == HAND_MAIN ? "off" : "main", gripDist, d.x, d.y, d.z, shouldAim[holding] ? 1 : 0, keep);
        debugPrintAt = vr_gametime + 0.25;
    }
    if(wasHeld && !shouldAim[holding])
    {
        if(!client::grabbing(helping) || !held::handEmpty(helping))
        {
            reportLetGo(holding, "the grip let go", 0.f, 0.f, stick);
        }
        else if(handpose::gunColliding(holding))
        {
            reportLetGo(holding, "the weapon against a wall", 0.f, 0.f, stick);
        }
        else if(!beforeMuzzle)
        {
            reportLetGo(holding, "past the muzzle", handDist, muzzleDist, stick);
        }
        else if(!goodDistance)
        {
            reportLetGo(holding, "distance", fixedMode ? gripDist : handDist, fixedMode ? keep : 25.f, stick);
        }
        else if(!goodDot)
        {
            reportLetGo(holding, "angle (cosine)", glm::dot(handDir, origDir), dotNeed, stick);
        }
        else
        {
            reportLetGo(holding, "no two-handed use", 0.f, 0.f, stick);
        }
    }
    helpingHand[helping] = shouldAim[holding];
    transition(aimTransition[holding], shouldAim[holding], 5.f);

    const float t = aimTransition[holding];
    if(t <= 0.f || cup)
    {
        stockTransition[holding] = 0.f;
        return;
    }

    const glm::vec3 stockDir = glm::mix(handDir, averageDir, stockTransition[holding]);
    const glm::vec3 dir = safeNormalize(glm::mix(origDir, stockDir, t));

    glm::vec3 angles = hands::anglesFromVectors(dir, up);

    glm::vec3 offsets = weapons::vec(slot, Key::TwoHPitch, Key::TwoHYaw, Key::TwoHRoll);
    if(holding == HAND_OFF) // mirrored
    {
        offsets.y = -offsets.y;
        offsets.z = -offsets.z;
    }
    s.rot[holding] = angles + offsets * t;
}

void applyHand(hands::State& s, const glm::vec3 (&originalRots)[2], int holding, int helping, int mode)
{
    cupHeld[holding] = false;
    // A weapon carried off its handle is not aimed, with one hand or two (the other hand may hold it too: applyCarried).
    if(carrying(holding))
    {
        shouldAim[holding] = false;
        aimTransition[holding] = stockTransition[holding] = 0.f;
        applyCarried(s, holding, helping);
        return;
    }

    const int slot = weapons::heldSlot(holding);
    const bool holdingWeapon = slot >= 0 && weaponId(holding) != widFist;
    FreeGrip& g = freeGrips[holding];
    if(g.on && (!holdingWeapon || g.slot < 0))
    {
        endFree(holding, g.slot < 0 ? "it changed hands" : "the weapon left the hand", 0.f, 0.f);
    }
    // Held anywhere by the other hand: that grip alone (it never takes a hotspot while it holds).
    if(g.on && updateFree(s, originalRots, holding, helping, slot))
    {
        return;
    }
    applyHotspots(s, originalRots, holding, helping, mode);
    // Let go of: its turn of the aim eased out (as a hotspot's), unless a hotspot aims it now.
    if(!g.on && g.t > 0.f)
    {
        transition(g.t, false, 5.f);
        if(holdingWeapon && g.slot == slot && g.mode != FREE_SUPPORT && aimTransition[holding] <= 0.f)
        {
            const glm::quat from = glm::quat_cast(basisOf(originalRots[holding]));
            s.rot[holding] = anglesOfQuat(from * glm::slerp(glm::quat{1.f, 0.f, 0.f, 0.f}, g.turn, g.t));
        }
    }
    // The hotspots first; else, anywhere on it (where the other hand carried it, if this hand just took its handle).
    if(holdingWeapon && (startRetake(s, originalRots, holding, helping, slot) || startFree(s, originalRots, holding, helping, slot)))
    {
        updateFree(s, originalRots, holding, helping, slot);
    }
}

} // namespace

void apply(hands::State& s)
{
    const int mode = static_cast<int>(vr_2h_mode.value);
    if(mode == VR_2H_DISABLED)
    {
        reset();
        return;
    }

    frameDt = lastTime >= 0.0 ? static_cast<float>(za::clamp(cl.time - lastTime, 0.0, 0.1)) : 0.f;
    lastTime = cl.time;

    // Sticky grips: the faster hand's speed (last frame's, m/s) makes a held grip stickier while it swings.
    const float speed = za::max(glm::length(s.vel[HAND_OFF]), glm::length(s.vel[HAND_MAIN]));
    const float from = za::max(vr_2h_sticky_fast_from.value, 0.f);
    const float full = za::max(vr_2h_sticky_fast_full.value, from + 0.01f);
    const float now = za::clamp((speed - from) / (full - from), 0.f, 1.f);
    fastShare = za::max(now, fastShare - frameDt / za::max(vr_2h_sticky_fast_hold.value, 0.01f));
    stickiness = za::max(vr_2h_sticky.value, 0.1f) * (1.f + za::max(vr_2h_sticky_fast.value, 0.f) * fastShare);

    const glm::vec3 originalRots[2]{s.rot[HAND_OFF], s.rot[HAND_MAIN]};
    helpingHand[HAND_OFF] = helpingHand[HAND_MAIN] = false;
    for(int h = 0; h < 2; h++)
    {
        const bool grab = client::grabbing(h);
        if(grab && !grabWas[h])
        {
            grabStart[h] = vr_gametime;
        }
        grabWas[h] = grab;
        const int id = weaponId(h);
        const bool carried = carrying(h);
        if(id != heldWas[h] || carried != carriedWas[h])
        {
            if(heldWas[h] >= 0)
            {
                resetHolding(h, heldWas[h], id);
            }
            heldWas[h] = id;
            carriedWas[h] = carried;
        }
        if(carried)
        {
            carryLast[h] = vr_gametime;
        }
    }
    applyHand(s, originalRots, HAND_MAIN, HAND_OFF, mode);
    applyHand(s, originalRots, HAND_OFF, HAND_MAIN, mode);
}

bool aiming()
{
    // (Every free grip, whatever its mode, a carried weapon's too: two hands on it, for the spread, melee and the parry;
    // not a weapon with Two-Handed: Not Allowed. Only the hotspots aim by their Two-Handed Aim offsets.)
    const auto freeAims = [](int h) {
        const FreeGrip& g = freeGrips[h];
        return g.on && g.counts && g.t >= 0.5f;
    };
    return aimTransition[HAND_OFF] >= 0.5f || aimTransition[HAND_MAIN] >= 0.5f || freeAims(HAND_OFF) || freeAims(HAND_MAIN);
}

float transition(int hand)
{
    return aimTransition[hand];
}

bool helping(int hand)
{
    return helpingHand[hand];
}

bool flickAllowed(int holding)
{
    // One-handed always; with the other hand on it, only by a cup (a two-handed pistol grip, round the holding hand's
    // grip): not on its barrel, a foregrip or anywhere on it (NOTES.md vrfiringrange_2026-10-02_01-08-19), nor carried
    // off its handle.
    if(carrying(holding) || freeGrips[holding].on)
    {
        return false;
    }
    return !helpingHand[1 - holding] || cupHeld[holding];
}

bool bladeGrip(int hand)
{
    return aimTransition[hand] > 0.f && grip[hand] == GRIP_BLADE;
}

bool bladeGripHand(const hands::State& s, int hand, const glm::vec3& holderPos, const glm::vec3& holderRot, glm::vec3& pos,
    glm::vec3& rot)
{
    const int holding = 1 - hand;
    const int slot = weapons::heldSlot(holding);
    if(!bladeGrip(holding) || !s.muzzleValid[holding] || slot < 0)
    {
        return false;
    }
    bladeGripOn(slot, holding, s.muzzle[holding], s.visualRot[holding], holderPos, holderRot, pos, rot);
    return true;
}

void bladeGripOn(int slot, int holding, const glm::vec3& tip, const glm::vec3& holderVisualRot, const glm::vec3& holderPos,
    const glm::vec3& holderRot, glm::vec3& pos, glm::vec3& rot)
{
    // The blade's axis, as drawn: through its tip, along its direction.
    const glm::vec3 d = bladeDirection(slot, holding, holderVisualRot);
    const auto onAxis = [&](const glm::vec3& p) { return tip + d * glm::dot(p - tip, d); };
    const auto local = [](const glm::vec3& v, const glm::vec3& angles) {
        glm::vec3 f, r, u;
        hands::angleVectors(angles, f, r, u);
        return glm::vec3{glm::dot(v, f), glm::dot(v, r), glm::dot(v, u)};
    };

    // How the holding hand holds it, in its own frame: the hand's origin off the blade's axis, and the
    // blade's direction. The helping hand is the other hand, drawn mirrored: the same grip, mirrored.
    glm::vec3 o = local(holderPos - onAxis(holderPos), holderRot);
    glm::vec3 g = local(d, holderRot);
    o.y = -o.y;
    g.y = -g.y;

    // The helping hand as tracked, turned (the least turn) until its grip lies along the blade, either
    // way round (the thumb towards the tip or towards the hilt).
    const glm::vec3 gw = safeNormalize(hands::redirect(g, rot));
    rot = turnAngles(rot, gw, glm::dot(gw, d) >= 0.f ? d : -d);

    // Slid onto the blade where it holds it, between the hilt and the tip (or where its Blade hotspot ends: the crowbar's
    // short of its hook).
    float end = 0.95f;
    for(int i = 0; i < weapons::maxHotspots; i++)
    {
        if(const weapons::Hotspot h = weapons::hotspot(slot, i); h.type == weapons::HotspotType::Blade)
        {
            end = za::min(end, weapons::bladeTo(h));
            break;
        }
    }
    const float hilt = glm::dot(holderPos - tip, d); // negative
    const float along =
        za::clamp(glm::dot(pos - hands::redirect(o, rot) - tip, d), hilt * 0.75f, hilt * (1.f - za::max(0.26f, end)));
    pos = tip + d * along + hands::redirect(o, rot);
}

void reset()
{
    for(int h = 0; h < 2; h++)
    {
        aimTransition[h] = stockTransition[h] = 0.f;
        shouldAim[h] = helpingHand[h] = false;
        grip[h] = GRIP_FOREGRIP;
        help[h] = HelpRecord{};
        carryWasOn[h] = carryPoseValid[h] = handleValid[h] = false;
        groundSpots[h] = GroundSpot{};
        freeGrips[h] = FreeGrip{};
        freeCandidate[h] = grabWas[h] = false;
        candidateTime[h] = grabStart[h] = carryLast[h] = retakeFrom[h] = -1.0;
        gripLength[h] = 0.f;
        heldWas[h] = -1;
        carriedWas[h] = false;
    }
    lastTime = -1.0;
    fastShare = 0.f;
    stickiness = 1.f;
}

bool carrying(int hand)
{
    const int flags = cl.stats[hand == HAND_MAIN ? protocol::STAT_QVR_WEAPONFLAGS : protocol::STAT_QVR_WEAPONFLAGS2];
    return weaponId(hand) != widFist && (flags & wpnFlagForegripCarried) != 0;
}

void recordHelp(const hands::State& s, int hand, const glm::vec3& drawnPos, const glm::vec3& drawnRot,
    const glm::vec3& holderPos, const glm::vec3& holderRot, bool holderMirrored)
{
    const int holder = 1 - hand;
    HelpRecord& r = help[hand];
    const bool going = r.valid && vr_gametime - r.time < 0.1; // helping since the last frames
    r.time = vr_gametime;
    if(going && !client::grabbing(holder))
    {
        return; // the holding hand let go: the pose at the release (it moves away before the server hands off)
    }
    r.valid = true;
    r.holder = relativeTo(s.pos[hand], s.rot[hand], holderPos, holderRot);
    r.holderMirrored = holderMirrored;
    r.drawnHand = relativeTo(s.pos[hand], s.rot[hand], drawnPos, drawnRot);
    r.free = freeHelping(hand);
}

namespace
{

// The pose a carrying hand carries by: taken when its carry begins, from its last help (if that was
// just now: the server hands off on the release, a few frames at most after the last help), or from the hotspot it was
// near on a weapon lying about (taken by it: vr_weapon_grab_hotspots), whichever is the later.
[[nodiscard]] const HelpRecord* carriedPose(int hand)
{
    const bool on = carrying(hand);
    if(on && !carryWasOn[hand])
    {
        const HelpRecord& ground = groundSpots[hand].record;
        const HelpRecord& from = ground.valid && (!help[hand].valid || ground.time > help[hand].time) ? ground : help[hand];
        carryPoseValid[hand] = from.valid && vr_gametime - from.time < 0.5;
        carryPose[hand] = from;
        if(vr_debug_2h_grip.value && &from == &ground)
        {
            Con_Printf(groundSpots[hand].recordIndex == anywhereSpot ? "2h grip: %s hand carries a weapon anywhere on it (%d), taken off the floor (%s)\n"
                                                                     : "2h grip: %s hand carries a weapon by its hotspot %d, taken off the floor (%s)\n",
                hand == HAND_MAIN ? "main" : "off", groundSpots[hand].recordIndex, carryPoseValid[hand] ? "posed" : "too late");
        }
    }
    carryWasOn[hand] = on;
    if(!on)
    {
        handleValid[hand] = false;
    }
    return on && carryPoseValid[hand] ? &carryPose[hand] : nullptr;
}

// The pose a weapon carried by `hand` is drawn from while the other hand holds it too: where both hands hold it.
[[nodiscard]] carry2h::Frame carriedInBoth(const hands::State& s, int hand)
{
    const carry2h::Frame both[2]{frameOf(s.pos[HAND_OFF], s.rot[HAND_OFF]), frameOf(s.pos[HAND_MAIN], s.rot[HAND_MAIN])};
    return carry2h::solve(freeGrips[hand].hold, both);
}

void applyCarried(hands::State& s, int holding, int helping)
{
    FreeGrip& g = freeGrips[holding];
    const HelpRecord* r = carriedPose(holding);
    if(!r)
    {
        endFree(holding, "nothing carried", 0.f, 0.f);
        return;
    }
    if(g.on && g.slot < 0)
    {
        const carry2h::Frame object = carriedInBoth(s, holding);
        const float off = glm::distance(s.pos[helping], carry2h::onGrip(g.hold, object, helping).pos);
        const bool gripping = client::grabbing(helping) && held::handEmpty(helping);
        if(gripping && off <= freeKeep())
        {
            g.lastOn = vr_gametime;
            helpingHand[helping] = true;
            transition(g.t, true, 5.f);
            return;
        }
        // Let go: the carrying hand goes on carrying it where it is now, as it is drawn on it.
        const glm::vec3 objectRot = anglesOfQuat(object.rot);
        glm::vec3 drawnPos, drawnRot;
        fromRelative(object.pos, objectRot, g.carrier, drawnPos, drawnRot);
        carryPose[holding].holder = relativeTo(s.pos[holding], s.rot[holding], object.pos, objectRot);
        carryPose[holding].drawnHand = relativeTo(s.pos[holding], s.rot[holding], drawnPos, drawnRot);
        carryPose[holding].free = true;
        endFree(holding, gripping ? "distance" : "the grip let go", off, freeKeep());
        return;
    }
    g.on = false;
    // The other hand taking hold of it anywhere but its handle (which takes it back: HS_CARRIED_GRIP).
    if(!vr_weapon_grab_anywhere.value || !client::grabbing(helping) || !held::handEmpty(helping) || carrying(helping) ||
        !candidate(helping) || !freshGrab(helping))
    {
        return;
    }
    glm::vec3 holderPos, holderRot, drawnPos, drawnRot;
    fromRelative(s.pos[holding], s.rot[holding], r->holder, holderPos, holderRot);
    fromRelative(s.pos[holding], s.rot[holding], r->drawnHand, drawnPos, drawnRot);
    g = FreeGrip{};
    g.on = true;
    g.mode = FREE_RIGID;
    g.slot = -1;
    g.counts = !twoHandsForbidden(weapons::heldSlot(holding));
    const carry2h::Frame both[2]{frameOf(s.pos[HAND_OFF], s.rot[HAND_OFF]), frameOf(s.pos[HAND_MAIN], s.rot[HAND_MAIN])};
    g.hold = carry2h::record(frameOf(holderPos, holderRot), both);
    g.carrier = relativeTo(holderPos, holderRot, drawnPos, drawnRot);
    g.lastOn = vr_gametime;
    helpingHand[helping] = true;
    if(vr_debug_2h_grip.value)
    {
        Con_Printf("2h grip: %s hand took the weapon the other hand carries (both hands hold it)\n",
            helping == HAND_MAIN ? "main" : "off");
    }
}

} // namespace

bool carriedWeapon(const hands::State& s, int hand, HeldAs& out)
{
    const HelpRecord* r = carriedPose(hand);
    if(!r)
    {
        return false;
    }
    if(freeGrips[hand].on && freeGrips[hand].slot < 0)
    {
        const carry2h::Frame object = carriedInBoth(s, hand);
        out.pos = object.pos;
        out.rot = anglesOfQuat(object.rot);
    }
    else
    {
        fromRelative(s.pos[hand], s.rot[hand], r->holder, out.pos, out.rot);
    }
    out.mirrored = r->holderMirrored;
    return true;
}

bool carryingHand(const hands::State& s, int hand, glm::vec3& pos, glm::vec3& rot)
{
    const HelpRecord* r = carriedPose(hand);
    if(!r)
    {
        return false;
    }
    if(freeGrips[hand].on && freeGrips[hand].slot < 0)
    {
        const carry2h::Frame object = carriedInBoth(s, hand);
        fromRelative(object.pos, anglesOfQuat(object.rot), freeGrips[hand].carrier, pos, rot);
    }
    else
    {
        fromRelative(s.pos[hand], s.rot[hand], r->drawnHand, pos, rot);
    }
    return true;
}

void setFreeCandidate(int hand, bool on)
{
    freeCandidate[hand] = on;
    if(on)
    {
        candidateTime[hand] = vr_gametime;
    }
}

bool freeHelping(int hand)
{
    return helpingHand[hand] && freeGrips[1 - hand].on;
}

int freeMode(int holding)
{
    return freeGrips[holding].on ? freeGrips[holding].mode : -1;
}

bool freeHand(int hand, const glm::vec3& trackedPos, const glm::vec3& trackedRot, const glm::vec3& holderPos,
    const glm::vec3& holderRot, glm::vec3& pos, glm::vec3& rot)
{
    FreeGrip& g = freeGrips[1 - hand];
    if(!freeHelping(hand))
    {
        return false;
    }
    if(g.handPending)
    {
        g.hand = relativeTo(holderPos, holderRot, trackedPos, trackedRot);
        g.handPending = false;
    }
    fromRelative(holderPos, holderRot, g.hand, pos, rot);
    return true;
}

int helpKind(int hand)
{
    const FreeGrip& g = freeGrips[1 - hand];
    if(helpingHand[hand])
    {
        return g.on ? 2 : 1;
    }
    return vr_gametime - g.lastOn < 0.2 ? 2 : 0;
}

bool freeGripPoint(const hands::State& s, int hand, glm::vec3& out, float& t)
{
    const FreeGrip& g = freeGrips[hand];
    if(!g.on)
    {
        return false;
    }
    t = g.t;
    if(g.slot >= 0)
    {
        out = s.pos[hand] + hands::redirect(g.point, s.rot[hand]); // (its place, as the weapon is aimed)
    }
    else
    {
        out = carry2h::onGrip(g.hold, carriedInBoth(s, hand), 1 - hand).pos;
    }
    return true;
}

float support(int hand)
{
    const FreeGrip& g = freeGrips[hand];
    return za::max(aimTransition[hand], g.on ? g.t : 0.f);
}

void setCarriedHandle(int hand, const glm::vec3& pos)
{
    handleValid[hand] = true;
    handle[hand] = pos;
}

void recordGroundSpot(int hand, int entity, int index, const glm::vec3& trackedPos, const glm::vec3& trackedRot,
    const glm::vec3& holderPos, const glm::vec3& holderRot, bool holderMirrored, const glm::vec3& drawnPos,
    const glm::vec3& drawnRot)
{
    GroundSpot& g = groundSpots[hand];
    g.entity = entity;
    g.index = index;
    g.recordIndex = index;
    g.record.valid = true;
    g.record.time = vr_gametime;
    g.record.holder = relativeTo(trackedPos, trackedRot, holderPos, holderRot);
    g.record.holderMirrored = holderMirrored;
    g.record.drawnHand = relativeTo(trackedPos, trackedRot, drawnPos, drawnRot);
    g.record.free = index == anywhereSpot;
}

bool carriedHandle(int hand, glm::vec3& out)
{
    if(!carrying(hand) || !handleValid[hand])
    {
        return false;
    }
    out = handle[hand];
    return true;
}

void clearGroundSpot(int hand)
{
    // (The record stays: the carry that begins as the server takes the weapon, a frame or two on, still takes it.)
    groundSpots[hand].entity = 0;
    groundSpots[hand].index = -1;
}

int groundSpot(int hand, int entity)
{
    const GroundSpot& g = groundSpots[hand];
    // (Seen this frame or the last few: the server runs before the view in a frame.)
    return entity > 0 && g.entity == entity && g.index >= 0 && vr_gametime - g.record.time < 0.25 ? g.index + 1 : 0;
}

void updateHotspots(hands::State& s)
{
    for(int hand = 0; hand < 2; hand++)
    {
        const int other = 1 - hand;
        if(held::handEmpty(hand) && carrying(other) && handleValid[other] &&
            glm::distance(s.pos[hand], handle[other]) < carriedGripRadius)
        {
            s.hotspot[hand] = body::HS_CARRIED_GRIP;
        }
    }
}

} // namespace qvr::twohand
