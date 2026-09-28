// vr_handpose.cpp -- see vr_handpose.hpp. From the old engine's SetHandPos,
// VR_GetResolvedHandPos, VR_UpdateGunWallCollisions and VR_DoWeaponDirSlerp.
//
// Collisions (when hosting, see vr_trace): a small box is swept from the upper torso to each
// tracked hand and stopped along the plane it hits; then from the hand to its weapon's muzzle,
// pushing the hand back so the barrel does not go through walls. Hands stay within 50 units of
// the torso.
//
// Weight: what a hand holds follows it through a spring (vr_weight.cpp), after the two-handed aim (weightDirections).
// The old engine's weight (each frame a hand moved and turned only part of the way to the tracked pose, the Speed
// Limit) is gone (round 21, "Spring only").

#include "vr_handpose.hpp"
#include "vr_engine.hpp"
#include "vr_avatar.hpp"
#include "vr_backend.hpp"
#include "vr_cvars.hpp"
#include "vr_protocol.hpp"
#include "vr_trace.hpp"
#include "vr_weight.hpp"

#include <algorithm>
#include <cmath>

namespace qvr::handpose
{
namespace
{

struct HandMemory
{
    bool valid{false};
    glm::vec3 lastPos{0.f};     // world position last frame (the muzzle was placed from it)
    bool climbing{false};       // holding a ledge or a rung (vr_climb.cpp),
    glm::vec3 climbStop{0.f};   // and the wall's stop when it took hold, kept
};

HandMemory memory[2];
bool colliding[2]{false, false};
double lastTime = -1.0;
float frameDt = 0.f;
bool newFrame = false; // the hands may be recomputed within a frame: the weight's spring advances once

// Sweeps a small box from `from` to `to`. When it hits, `pos` stops along the axes the hit plane
// faces, `back` short of where the box stopped (the other axes are kept), and it returns true.
// Monsters (anything that bleeds, not a brush) don't stop it: their boxes are much bigger than they
// look, and a sword stopped at one jerked the hand back, which the server's melee took for a new
// stroke (no hit), so blows with a weapon's far end didn't register (docs/vr-port/ROUND15.md).
bool stopAtWall(glm::vec3& pos, const glm::vec3 from, const glm::vec3 to, const glm::vec3& back = glm::vec3{0.f})
{
    const glm::vec3 box{1.f};
    auto tr = worldtrace::move(from, -box, box, to, MOVE_NORMAL);
    if(tr && tr->fraction < 1.f && tr->ent && tr->ent->v.takedamage != 0.f && static_cast<int>(tr->ent->v.solid) != SOLID_BSP)
    {
        tr = worldtrace::move(from, -box, box, to, MOVE_NOMONSTERS);
    }
    if(!tr || tr->fraction >= 1.f)
    {
        return false;
    }

    const glm::vec3 n = worldtrace::normal(*tr);
    const glm::vec3 stop = worldtrace::endPos(*tr) - back;
    for(int i = 0; i < 3; i++)
    {
        if(n[i] != 0.f)
        {
            pos[i] = stop[i];
        }
    }
    return true;
}

} // namespace

void resolvePositions(hands::State& s, float /* turnYaw */)
{
    // On the real clock, every frame: cl.time moves in steps with the server's messages (72 Hz, or
    // 48 and 72 by turns at 144 fps), so a weight stepped on it moved the hands in uneven jerks that
    // the server's melee read as a wrist speeding up and slowing down (up to half again at 144 fps).
    frameDt = lastTime >= 0.0 ? static_cast<float>(std::clamp(realtime - lastTime, 0.0, 0.1)) : 0.f;
    newFrame = realtime != lastTime;
    lastTime = realtime;

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
        glm::vec3 pos = s.pos[h];
        stopAtWall(pos, torso, s.pos[h]);

        // The weapon's muzzle, as placed last frame, must not go through walls either.
        colliding[h] = false;
        if(m.valid && s.muzzleValid[h])
        {
            const glm::vec3 muzzleOffset = s.muzzle[h] - m.lastPos;
            colliding[h] = stopAtWall(pos, pos, pos + muzzleOffset, muzzleOffset);
        }

        // A hand holding a ledge or a rung (vr_climb.cpp) keeps the stop it had when it took hold: the climb pulls by
        // the controller's own motion (a stop that came and went as the body moved by the face was a pull of its own),
        // and the hand is drawn on its hold.
        const bool climbing = (cl.stats[protocol::STAT_QVR_CLIMB] & (1 << h)) != 0;
        if(climbing && !m.climbing)
        {
            m.climbStop = pos - s.pos[h];
        }
        m.climbing = climbing;
        if(climbing)
        {
            pos = s.pos[h] + m.climbStop;
        }

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
    // What each hand holds follows it through its spring (vr_weight.cpp); the muzzle placed from where it has the hand
    // (the walls' test next frame).
    weight::spring(s, turnYaw, frameDt, newFrame);
    for(int h = 0; h < HAND_COUNT; h++)
    {
        memory[h].lastPos = s.pos[h];
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
    weight::reset();
}

} // namespace qvr::handpose
