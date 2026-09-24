// vr_twohand.cpp -- see vr_twohand.hpp. Ported from the old engine's VR_Do2HAiming.
//
// A weapon is aimed two-handed when the other hand is empty, gripping, between the holding
// hand and the muzzle (plus a margin), 5 to 25 units away, and roughly along the weapon
// (vr_2h_angle_threshold, a cosine). The aim then points from the holding hand to the helping
// hand (offset by the weapon's 2H offsets), blended in over 0.2 s. With vr_2h_mode 2 ("virtual
// stock"), a holding hand close to the shoulder (vr_virtual_stock_thresh) aims from the
// shoulder instead, mixed by vr_2h_virtual_stock_factor.

#include "vr_twohand.hpp"
#include "vr_backend.hpp"
#include "vr_client.hpp"
#include "vr_cvars.hpp"
#include "vr_handpose.hpp"
#include "vr_protocol.hpp"
#include "vr_weapons.hpp"

#include <algorithm>

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

void applyHand(hands::State& s, const glm::vec3 (&originalRots)[2], int holding, int helping, int mode)
{
    const int slot = weapons::heldSlot(holding);
    const bool holdingWeapon = slot >= 0 && weaponId(holding) != widFist;

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
    const glm::vec3 shoulder = hands::bodyAnchor(s, shoulderOffsets);
    const glm::vec3 averageDir =
        safeNormalize(glm::mix(handDiff, helpingPos - shoulder, vr_2h_virtual_stock_factor.value));

    glm::vec3 origDir, right, up;
    hands::angleVectors(originalRots[holding], origDir, right, up);

    const int wpnMode = holdingWeapon ? static_cast<int>(weapons::value(slot, Key::TwoHMode)) : WPN_2H_FORBIDDEN;

    const bool useStock = glm::distance(shoulder, holdingPos) < vr_virtual_stock_thresh.value &&
                          mode == VR_2H_VIRTUAL_STOCK && wpnMode != WPN_2H_NO_VIRTUAL_STOCK;
    transition(stockTransition[holding], useStock, 5.f);

    const float handDist = glm::distance(holdingPos, helpingPos);
    const bool goodDistance = handDist > 5.f && handDist < 25.f;

    // Muzzles move with the firing animation, hence the margin.
    const bool beforeMuzzle =
        !s.muzzleValid[holding] || handDist <= glm::distance(holdingPos, s.muzzle[holding]) + 7.5f;

    const bool canGrab = client::grabbing(helping) && wpnMode != WPN_2H_FORBIDDEN &&
                         weaponId(helping) == widFist && beforeMuzzle && !handpose::gunColliding(holding);
    const bool goodDot = glm::dot(handDir, origDir) > vr_2h_angle_threshold.value || vr_2h_disable_angle_threshold.value;

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
