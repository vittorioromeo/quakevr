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
#include "vr_protocol.hpp"
#include "vr_weapons.hpp"

#include <algorithm>
#include <cmath>

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
    var = std::clamp(var + frameDt * (on ? speed : -speed), 0.f, 1.f);
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
    glm::vec3 d{std::cos(pitch) * std::cos(yaw), std::cos(pitch) * std::sin(yaw), std::sin(pitch)};
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
    const float t = len2 > 0.f ? std::clamp(glm::dot(p - a, ab) / len2, 0.f, 1.f) : 0.f;
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
    const bool canGrab = client::grabbing(helping) && weaponId(helping) == widFist;

    // The grips (round 18). The grip point below the holding hand (GRIP_FOREGRIP): the blade along the
    // line from the helping hand through the holding hand. Unlike a gun's, it holds when the blade
    // touches a wall or the floor (a swing's end): the hands are kept out of walls anyway, and
    // letting go would turn the sword back mid-swing. The blade towards the tip (GRIP_BLADE, the
    // half-sword grip): the blade along the line from the holding hand (at the hilt) through the
    // helping hand, wherever the holding hand's wrist points it.
    const float foreDist = s.grip2HValid[holding] ? glm::distance(s.pos[helping], s.grip2H[holding]) : 1e9f;
    const float bladeAt = weapons::value(slot, Key::TwoHBladeGrip);
    const float length = s.muzzleValid[holding] ? glm::distance(holdingPos, s.muzzle[holding]) : 0.f;
    float bladeDist = 1e9f;
    if(bladeAt > 0.f && length > 8.f)
    {
        const glm::vec3 toTip = s.muzzle[holding] - holdingPos;
        bladeDist = segmentDistance(s.pos[helping], holdingPos + toTip * std::max(0.3f, bladeAt - 0.3f), holdingPos + toTip * 1.05f);
    }

    bool held = false;
    if(canGrab && shouldAim[holding])
    {
        // Held: the grip holds on.
        if(grip[holding] == GRIP_BLADE)
        {
            const float apart = glm::distance(holdingPos, s.pos[helping]);
            held = apart > gripLength[holding] * bladeKeepMin && apart < gripLength[holding] * bladeKeepMax;
        }
        else
        {
            held = foreDist < foregripKeep;
        }
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
    const bool goodDot = grip[holding] == GRIP_BLADE || vr_2h_angle_threshold.value <= -1.f ||
                         glm::dot(line, blade) > vr_2h_angle_threshold.value;

    shouldAim[holding] = held && goodDot;
    helpingHand[helping] = shouldAim[holding];
    transition(aimTransition[holding], shouldAim[holding], 5.f);
    stockTransition[holding] = 0.f;

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
    const bool fixedMode = holdingWeapon && s.grip2HValid[holding];
    const bool goodDistance = fixedMode ? glm::distance(s.pos[helping], s.grip2H[holding]) < (shouldAim[holding] ? 20.f : 5.5f)
                                        : handDist > 5.f && handDist < 25.f;

    // Muzzles move with the firing animation, hence the margin.
    const bool beforeMuzzle =
        !s.muzzleValid[holding] || handDist <= glm::distance(holdingPos, s.muzzle[holding]) + 7.5f;

    const bool canGrab = client::grabbing(helping) && wpnMode != WPN_2H_FORBIDDEN &&
                         weaponId(helping) == widFist && beforeMuzzle && !handpose::gunColliding(holding);
    const bool goodDot = vr_2h_angle_threshold.value <= -1.f || glm::dot(handDir, origDir) > vr_2h_angle_threshold.value;

    shouldAim[holding] = canGrab && goodDistance && goodDot;
    helpingHand[helping] = shouldAim[holding];
    transition(aimTransition[holding], shouldAim[holding], 5.f);

    const float t = aimTransition[holding];
    if(t <= 0.f)
    {
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

    frameDt = lastTime >= 0.0 ? static_cast<float>(std::clamp(cl.time - lastTime, 0.0, 0.1)) : 0.f;
    lastTime = cl.time;

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

    // The blade's axis, as drawn: through its tip, along its direction.
    const glm::vec3 d = bladeDirection(slot, holding, s.visualRot[holding]);
    const glm::vec3 tip = s.muzzle[holding];
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

    // Slid onto the blade where it holds it, between the hilt and the tip.
    const float hilt = glm::dot(holderPos - tip, d); // negative
    const float along = std::clamp(glm::dot(pos - hands::redirect(o, rot) - tip, d), hilt * 0.75f, hilt * 0.05f);
    pos = tip + d * along + hands::redirect(o, rot);
    return true;
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
    }
    lastTime = -1.0;
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
// just now: the server hands off on the release, a few frames at most after the last help).
[[nodiscard]] const HelpRecord* carriedPose(int hand)
{
    const bool on = carrying(hand);
    if(on && !carryWasOn[hand])
    {
        carryPoseValid[hand] = help[hand].valid && realtime - help[hand].time < 0.5;
        carryPose[hand] = help[hand];
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

void updateHotspots(hands::State& s)
{
    for(int hand = 0; hand < 2; hand++)
    {
        const int other = 1 - hand;
        if(weaponId(hand) == widFist && carrying(other) && handleValid[other] &&
            glm::distance(s.pos[hand], handle[other]) < carriedGripRadius)
        {
            s.hotspot[hand] = body::HS_CARRIED_GRIP;
        }
    }
}

} // namespace qvr::twohand
