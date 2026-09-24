// vr_handpose.cpp -- see vr_handpose.hpp. From the old engine's SetHandPos,
// VR_GetResolvedHandPos, VR_UpdateGunWallCollisions and VR_DoWeaponDirSlerp.
//
// Collisions (when hosting, see vr_trace): a small box is swept from the upper torso to each
// tracked hand and stopped along the plane it hits; then from the hand to its weapon's muzzle,
// pushing the hand back so the barrel does not go through walls. Hands stay within 50 units of
// the torso.
//
// Weight (vr_wpn_pos_weight, vr_wpn_dir_weight): each frame a hand moves (turns) only part of
// the way to the tracked pose, by a factor from the weapon's weight (lighter is quicker; a
// two-handed grip helps). The old engine corrected the lag for the player's movement and
// turning; here the smoothing happens in the body's frame (relative to the player and turned
// with the play space), which amounts to the same.

#include "vr_handpose.hpp"
#include "vr_avatar.hpp"
#include "vr_backend.hpp"
#include "vr_cvars.hpp"
#include "vr_trace.hpp"
#include "vr_twohand.hpp"
#include "vr_weapons.hpp"

#include <algorithm>
#include <cmath>

namespace qvr::handpose
{
namespace
{

using weapons::Key;

struct HandMemory
{
    bool valid{false};
    glm::vec3 local{0.f};       // position in the body's frame
    glm::vec3 localAngles{0.f}; // angles with the play-space yaw taken out
    glm::vec3 lastPos{0.f};     // world position last frame (the muzzle was placed from it)
    bool anglesValid{false};
};

HandMemory memory[2];
bool colliding[2]{false, false};
double lastTime = -1.0;
float frameDt = 0.f;
bool newFrame = false; // the hands may be recomputed within a frame: weight advances once

[[nodiscard]] glm::vec3 rotateZ(const glm::vec3& v, float degrees)
{
    const float r = glm::radians(degrees);
    const float c = std::cos(r);
    const float s = std::sin(r);
    return {v.x * c - v.y * s, v.x * s + v.y * c, v.z};
}

// 0..1 per 1/100 s: how quickly a hand follows, from the weapon's weight (old engine's
// VR_GetWeaponWeightFactorImpl).
[[nodiscard]] float followFactor(int hand, float offset, float mult, float twoHOffset, float twoHMult, Key twoHKey)
{
    const int slot = weapons::heldSlot(hand);
    if(slot < 0)
    {
        return 1.f;
    }

    const float initial = 1.f - weapons::value(slot, Key::Weight);
    const float withOffset = initial + offset;
    const float single = withOffset * mult;
    const float twoHanded = (withOffset + twoHOffset) * twoHMult * weapons::value(slot, twoHKey);
    return std::clamp(glm::mix(single, twoHanded, twohand::transition(hand)), 0.f, 1.f);
}

// The blend towards the tracked pose this frame.
[[nodiscard]] float blend(float factor, float perWeaponMult)
{
    return std::clamp(factor * perWeaponMult * frameDt * 100.f, 0.f, 1.f);
}

[[nodiscard]] glm::vec3 resolveCollision(const glm::vec3& from, const glm::vec3& to)
{
    const glm::vec3 box{1.f};
    const auto tr = worldtrace::move(from, -box, box, to, MOVE_NORMAL);
    if(!tr || tr->fraction >= 1.f)
    {
        return to;
    }

    // Stop along the axes the hit plane faces, keep the others.
    glm::vec3 res = to;
    const glm::vec3 n = worldtrace::normal(*tr);
    const glm::vec3 end = worldtrace::endPos(*tr);
    for(int i = 0; i < 3; i++)
    {
        if(n[i] != 0.f)
        {
            res[i] = end[i];
        }
    }
    return res;
}

} // namespace

void resolvePositions(hands::State& s, float turnYaw)
{
    frameDt = lastTime >= 0.0 ? static_cast<float>(std::clamp(cl.time - lastTime, 0.0, 0.1)) : 0.f;
    newFrame = cl.time != lastTime;
    lastTime = cl.time;

    // The upper torso: the body's chest (kept within the player's box), or 40 units above where
    // the hands are measured from.
    glm::vec3 torso = s.playerOrigin;
    torso.z += vr_floor_offset.value + vr_gun_z_offset.value + 40.f;
    if(vr_body_anchors.value && s.valid)
    {
        const glm::vec3 chest = avatar::torso(s).chest.pos;
        glm::vec2 offset{chest.x - s.playerOrigin.x, chest.y - s.playerOrigin.y};
        if(const float len = glm::length(offset); len > 12.f)
        {
            offset *= 12.f / len;
        }
        torso = {s.playerOrigin.x + offset.x, s.playerOrigin.y + offset.y, chest.z};
    }

    for(int h = 0; h < HAND_COUNT; h++)
    {
        HandMemory& m = memory[h];
        glm::vec3 pos = resolveCollision(torso, s.pos[h]);

        // The weapon's muzzle, as placed last frame, must not go through walls either.
        colliding[h] = false;
        if(m.valid && s.muzzleValid[h])
        {
            const glm::vec3 muzzleOffset = s.muzzle[h] - m.lastPos;
            const glm::vec3 box{1.f};
            if(const auto tr = worldtrace::move(pos, -box, box, pos + muzzleOffset, MOVE_NORMAL);
                tr && tr->fraction < 1.f)
            {
                colliding[h] = true;
                const glm::vec3 pushed = worldtrace::endPos(*tr) - muzzleOffset;
                const glm::vec3 n = worldtrace::normal(*tr);
                for(int i = 0; i < 3; i++)
                {
                    if(n[i] != 0.f)
                    {
                        pos[i] = pushed[i];
                    }
                }
            }
        }

        // Weight, in the body's frame.
        const glm::vec3 local = rotateZ(pos - s.playerOrigin, -turnYaw);
        if(vr_wpn_pos_weight.value && m.valid && newFrame)
        {
            const float factor = followFactor(h, vr_wpn_pos_weight_offset.value, vr_wpn_pos_weight_mult.value,
                vr_wpn_pos_weight_2h_help_offset.value, vr_wpn_pos_weight_2h_help_mult.value, Key::Weight2HPosMult);
            const int slot = weapons::heldSlot(h);
            m.local = glm::mix(m.local, local, blend(factor, slot >= 0 ? weapons::value(slot, Key::WeightPosMult) : 1.f));
        }
        else if(!m.valid || !vr_wpn_pos_weight.value)
        {
            m.local = local;
        }
        pos = s.playerOrigin + rotateZ(m.local, turnYaw);

        // Not too far from the body.
        constexpr float maxReach = 50.f;
        if(glm::distance(pos, torso) > maxReach)
        {
            pos = torso + glm::normalize(pos - torso) * maxReach;
        }

        s.pos[h] = pos;
        m.lastPos = pos;
        m.valid = true;
    }
}

void weightDirections(hands::State& s, float turnYaw)
{
    for(int h = 0; h < HAND_COUNT; h++)
    {
        HandMemory& m = memory[h];
        glm::vec3 target = s.rot[h];
        target.y -= turnYaw;

        if(!vr_wpn_dir_weight.value || !m.anglesValid)
        {
            m.localAngles = target;
            m.anglesValid = true;
            continue;
        }
        if(!newFrame)
        {
            glm::vec3 angles = m.localAngles;
            angles.y += turnYaw;
            s.rot[h] = angles;
            continue;
        }

        const float factor = followFactor(h, vr_wpn_dir_weight_offset.value, vr_wpn_dir_weight_mult.value,
            vr_wpn_dir_weight_2h_help_offset.value, vr_wpn_dir_weight_2h_help_mult.value, Key::Weight2HDirMult);
        const int slot = weapons::heldSlot(h);
        const float t = blend(factor, slot >= 0 ? weapons::value(slot, Key::WeightDirMult) : 1.f);

        // Slerp the forward and up directions (angles do not interpolate well).
        glm::vec3 oldFwd, oldRight, oldUp, newFwd, newRight, newUp;
        hands::angleVectors(m.localAngles, oldFwd, oldRight, oldUp);
        hands::angleVectors(target, newFwd, newRight, newUp);
        const glm::quat from = glm::quatLookAt(oldFwd, oldUp);
        const glm::quat to = glm::quatLookAt(newFwd, newUp);
        const glm::quat q = glm::slerp(from, to, t);

        const glm::vec3 fwd = q * glm::vec3{0.f, 0.f, -1.f};
        const glm::vec3 up = q * glm::vec3{0.f, 1.f, 0.f};
        m.localAngles = hands::anglesFromVectors(fwd, up);

        glm::vec3 angles = m.localAngles;
        angles.y += turnYaw;
        s.rot[h] = angles;
    }
}

bool gunColliding(int hand)
{
    return colliding[hand];
}

void reset()
{
    for(HandMemory& m : memory)
    {
        m = HandMemory{};
    }
    colliding[0] = colliding[1] = false;
    lastTime = -1.0;
}

} // namespace qvr::handpose
