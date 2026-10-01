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
#include "vr_client.hpp"
#include "vr_cvars.hpp"
#include "vr_handpose.hpp"
#include "vr_held.hpp"
#include "vr_protocol.hpp"
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
constexpr float carriedGripRadius = 6.f; // units from the carried gun's handle the other hand takes it

float aimTransition[2]{0.f, 0.f};   // per holding hand, 0..1
float stockTransition[2]{0.f, 0.f}; // per holding hand, 0..1
bool shouldAim[2]{false, false};    // per holding hand
bool helpingHand[2]{false, false};

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
        realtime >= debugPrintAt)
    {
        // Where along the blade the helping hand holds it: a share of the way from the holding hand to the tip.
        const glm::vec3 toTip = s.muzzle[holding] - holdingPos;
        Con_Printf("2h grip: on the blade at %.2f of the way to the tip\n",
            glm::dot(s.pos[helping] - holdingPos, toTip) / glm::dot(toTip, toTip));
        debugPrintAt = realtime + 0.25;
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

void applyHand(hands::State& s, const glm::vec3 (&originalRots)[2], int holding, int helping, int mode)
{
    // A gun carried by its foregrip is not aimed, with one hand or two.
    if(carrying(holding))
    {
        shouldAim[holding] = false;
        aimTransition[holding] = stockTransition[holding] = 0.f;
        return;
    }

    const int slot = weapons::heldSlot(holding);
    const bool holdingWeapon = slot >= 0 && weaponId(holding) != widFist;

    if(holdingWeapon && static_cast<int>(weapons::value(slot, Key::TwoHMode)) == WPN_2H_SWORD)
    {
        applySword(s, originalRots, holding, helping, slot);
        return;
    }

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
    if(vr_debug_2h_grip.value && fixedMode && !wasHeld && shouldAim[holding])
    {
        Con_Printf("2h grip: %s hand took it (%.1f units off its grip)\n", holding == HAND_MAIN ? "off" : "main", gripDist);
    }
    if(vr_debug_2h_grip.value >= 2 && fixedMode && realtime >= debugPrintAt)
    {
        // Where its grip is from the helping hand (units, world), to place a test's hand on it.
        const glm::vec3 d = s.grip2H[holding] - (s.grip2HPalm[holding] ? hands::palmPoint(s, helping) : s.pos[helping]);
        Con_Printf("2h grip: %s hand %.1f units off the grip (%.1f %.1f %.1f), held %d, keep %.1f\n",
            holding == HAND_MAIN ? "off" : "main", gripDist, d.x, d.y, d.z, shouldAim[holding] ? 1 : 0, keep);
        debugPrintAt = realtime + 0.25;
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
    applyHand(s, originalRots, HAND_MAIN, HAND_OFF, mode);
    applyHand(s, originalRots, HAND_OFF, HAND_MAIN, mode);
}

bool aiming()
{
    return aimTransition[HAND_OFF] >= 0.5f || aimTransition[HAND_MAIN] >= 0.5f;
}

float transition(int hand)
{
    return aimTransition[hand];
}

bool helping(int hand)
{
    return helpingHand[hand];
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

void recordHelp(const hands::State& s, int hand, const glm::vec3& drawnPos, const glm::vec3& drawnRot)
{
    const int holder = 1 - hand;
    HelpRecord& r = help[hand];
    const bool going = r.valid && realtime - r.time < 0.1; // helping since the last frames
    r.time = realtime;
    if(going && !client::grabbing(holder))
    {
        return; // the holding hand let go: the pose at the release (it moves away before the server hands off)
    }
    r.valid = true;
    r.holder = relativeTo(s.pos[hand], s.rot[hand], s.pos[holder], s.visualRot[holder]);
    r.holderMirrored = holder == HAND_OFF;
    r.drawnHand = relativeTo(s.pos[hand], s.rot[hand], drawnPos, drawnRot);
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
        carryPoseValid[hand] = from.valid && realtime - from.time < 0.5;
        carryPose[hand] = from;
        if(vr_debug_2h_grip.value && &from == &ground)
        {
            Con_Printf("2h grip: %s hand carries a weapon by its hotspot %d, taken off the floor (%s)\n",
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

} // namespace

bool carriedWeapon(const hands::State& s, int hand, HeldAs& out)
{
    const HelpRecord* r = carriedPose(hand);
    if(!r)
    {
        return false;
    }
    fromRelative(s.pos[hand], s.rot[hand], r->holder, out.pos, out.rot);
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
    fromRelative(s.pos[hand], s.rot[hand], r->drawnHand, pos, rot);
    return true;
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
    g.record.time = realtime;
    g.record.holder = relativeTo(trackedPos, trackedRot, holderPos, holderRot);
    g.record.holderMirrored = holderMirrored;
    g.record.drawnHand = relativeTo(trackedPos, trackedRot, drawnPos, drawnRot);
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
    return entity > 0 && g.entity == entity && g.index >= 0 && realtime - g.record.time < 0.25 ? g.index + 1 : 0;
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
