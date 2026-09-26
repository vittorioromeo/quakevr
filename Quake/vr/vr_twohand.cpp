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

float aimTransition[2]{0.f, 0.f};   // per holding hand, 0..1
float stockTransition[2]{0.f, 0.f}; // per holding hand, 0..1
bool shouldAim[2]{false, false};    // per holding hand
bool helpingHand[2]{false, false};

double lastTime = -1.0;
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

    const glm::vec3 line = safeNormalize(holdingPos - helpingPos); // pommel to blade
    const glm::vec3 blade = bladeDirection(slot, holding, originalRots[holding]);

    // Take hold at the grip point below the holding hand, keep it a little further off. Unlike a
    // gun's, the grip holds when the blade touches a wall or the floor (a swing's end): the hands are
    // kept out of walls anyway, and letting go would turn the sword back mid-swing.
    const bool goodDistance =
        s.grip2HValid[holding] && glm::distance(s.pos[helping], s.grip2H[holding]) < (shouldAim[holding] ? 20.f : 5.5f);
    const bool canGrab = client::grabbing(helping) && weaponId(helping) == widFist;
    const bool goodDot = vr_2h_angle_threshold.value <= -1.f || glm::dot(line, blade) > vr_2h_angle_threshold.value;

    shouldAim[holding] = canGrab && goodDistance && goodDot;
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

void reset()
{
    for(int h = 0; h < 2; h++)
    {
        aimTransition[h] = stockTransition[h] = 0.f;
        shouldAim[h] = helpingHand[h] = false;
    }
    lastTime = -1.0;
}

} // namespace qvr::twohand
