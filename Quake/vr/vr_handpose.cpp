// vr_handpose.cpp -- see vr_handpose.hpp. From the old engine's SetHandPos,
// VR_GetResolvedHandPos, VR_UpdateGunWallCollisions and VR_DoWeaponDirSlerp.
//
// Collisions (when hosting, see vr_trace): a small box is swept from the upper torso to each
// tracked hand and stopped along the plane it hits. The weapon then stays out of the walls (round
// 21, "Held weapons and props against the level"): its points (its drawn model's, spread over it,
// the muzzle, and those of a prop the hand holds alone: modelcollide::weaponShape; else the line to
// the muzzle), turned with the hand. Where they are clear it is where the hand is; where not, it
// slides from where it was last frame towards there along what it meets (a table top holds it up, a
// wall holds it off), as the player's box moves (SV_FlyMove's clip), after leaving any surface its
// turn put it into along that surface's normal (the rays from the hand to its points; Gauss-Seidel),
// as far as vr_gun_wall_max. vr_gun_wall_slide 0: the old push-back along the aim (the hand moved
// back so that the muzzle stops at the wall). Hands stay within 50 units of the torso.
//
// Weight: what a hand holds follows it through a spring (vr_weight.cpp), after the two-handed aim (weightDirections).
// The old engine's weight (each frame a hand moved and turned only part of the way to the tracked pose, the Speed
// Limit) is gone (round 21, "Spring only").

#include "vr_handpose.hpp"
#include "vr_engine.hpp"
#include "vr_avatar.hpp"
#include "vr_backend.hpp"
#include "vr_cvars.hpp"
#include "vr_modelcollide.hpp"
#include "vr_protocol.hpp"
#include "vr_trace.hpp"
#include "vr_units.hpp"
#include "vr_weight.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <optional>
#include <vector>

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
    glm::vec3 lastFwd{1.f, 0.f, 0.f}, lastRight{0.f, -1.f, 0.f}, lastUp{0.f, 0.f, 1.f}; // its turn then (the muzzle's)
    bool gunValid{false};       // the weapon was tested last frame, the hand at `gunPos` (the slide starts there),
    glm::vec3 gunPos{0.f};
    bool slid{false};           // and held out of the walls there
};

HandMemory memory[2];
bool colliding[2]{false, false};
glm::vec3 wallPushes[2]{glm::vec3{0.f}, glm::vec3{0.f}}; // how far resolvePositions moved each hand (vr_debug_hand_offset)
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

// A line from `from` to `to` through the level (and the things lying in it); monsters don't stop it (stopAtWall).
[[nodiscard]] std::optional<trace_t> lineTrace(const glm::vec3& from, const glm::vec3& to)
{
    const glm::vec3 zero{0.f};
    auto tr = worldtrace::move(from, zero, zero, to, MOVE_NORMAL);
    if(tr && tr->fraction < 1.f && tr->ent && tr->ent->v.takedamage != 0.f && static_cast<int>(tr->ent->v.solid) != SOLID_BSP)
    {
        tr = worldtrace::move(from, zero, zero, to, MOVE_NOMONSTERS);
    }
    return tr;
}

constexpr float gunMargin = 0.4f;  // units the weapon is kept off a surface
constexpr int gunModelPoints = 48; // of its model's points and a held prop's (modelcollide::weaponShape) at most
constexpr int gunLinePoints = 5;   // without them, points on the line from the hand to the muzzle
constexpr float gunRestart = 48.f; // units: the slide starts where the hand is when it was farther than this

// The weapon's points as offsets from the hand: the hand itself first, its muzzle, then its model's (or the line's).
std::vector<glm::vec3> shape[HAND_COUNT];

void makeShape(int h, const glm::vec3& rot, const glm::vec3& muzzle)
{
    std::vector<glm::vec3>& out = shape[h];
    static std::vector<glm::vec3> model;
    out.clear();
    out.push_back(glm::vec3{0.f});
    out.push_back(muzzle);
    if(modelcollide::weaponShape(h, rot, model) && !model.empty())
    {
        out.insert(out.end(), model.begin(), model.begin() + std::min<std::ptrdiff_t>(gunModelPoints, std::ssize(model)));
        return;
    }
    for(int k = 1; k < gunLinePoints - 1; k++)
    {
        out.push_back(muzzle * (static_cast<float>(k) / static_cast<float>(gunLinePoints - 1)));
    }
}

struct Plane
{
    glm::vec3 n;
    float c; // the push `p` must have dot(p, n) >= c
};

// The rays from the hand at `hand` to the weapon's points (`pts`) that go into a surface: each a plane there (the push
// `p` so far: the part of the ray inside, moved by it, out of that surface by gunMargin) into `planes`. The deepest
// part inside (0: clear).
float gunPlanes(const glm::vec3& hand, const std::vector<glm::vec3>& pts, const glm::vec3& p, std::vector<Plane>* planes)
{
    float deepest = 0.f;
    for(std::size_t k = 1; k < pts.size(); k++)
    {
        const glm::vec3 end = hand + p + pts[k];
        const auto tr = lineTrace(hand + p, end);
        if(!tr || tr->startsolid || tr->allsolid || tr->fraction >= 1.f)
        {
            continue;
        }
        const glm::vec3 n = worldtrace::normal(*tr);
        const float depth = std::max(glm::dot(worldtrace::endPos(*tr) - end, n), 0.f);
        deepest = std::max(deepest, depth);
        if(planes)
        {
            planes->push_back(Plane{n, depth + gunMargin + glm::dot(p, n)});
        }
    }
    return deepest;
}

// The weapon moved out of what its rays go into: the least move out of all their planes (Gauss-Seidel), tested again
// from there (three rounds at most: a corner, a curved wall).
void gunDepenetrate(glm::vec3& hand, const std::vector<glm::vec3>& pts)
{
    static std::vector<Plane> planes;
    planes.clear();
    glm::vec3 p{0.f};
    for(int round = 0; round < 3; round++)
    {
        const std::size_t before = planes.size();
        gunPlanes(hand, pts, p, &planes);
        if(planes.size() == before)
        {
            break;
        }
        for(int sweep = 0; sweep < 16; sweep++)
        {
            float worst = 0.f;
            for(const Plane& plane : planes)
            {
                if(const float short_ = plane.c - glm::dot(p, plane.n); short_ > 0.f)
                {
                    p += plane.n * short_;
                    worst = std::max(worst, short_);
                }
            }
            if(worst < 0.01f)
            {
                break;
            }
        }
    }
    hand += p;
}

// The weapon slid from `hand` towards `to`: its points swept along the move, stopped at the first surface any meets,
// and the rest of the move along the surfaces met so far (along the crease of two), four times at most.
[[nodiscard]] glm::vec3 gunSlide(glm::vec3 hand, const glm::vec3& to, const std::vector<glm::vec3>& pts)
{
    glm::vec3 move = to - hand;
    glm::vec3 planes[3];
    int count = 0;
    for(int bump = 0; bump < 4 && glm::dot(move, move) > 1e-6f; bump++)
    {
        float first = 1.f;
        glm::vec3 n{0.f};
        for(const glm::vec3& o : pts)
        {
            const glm::vec3 a = hand + o;
            const auto tr = lineTrace(a, a + move);
            if(!tr || tr->startsolid || tr->allsolid || tr->fraction >= first)
            {
                continue;
            }
            first = tr->fraction;
            n = worldtrace::normal(*tr);
        }
        hand += move * first;
        if(first >= 1.f || count == 3)
        {
            break;
        }
        hand += n * gunMargin;
        move *= 1.f - first;
        planes[count++] = n;

        // The rest of the move off each surface met that it goes into; still into one: along the crease with the last.
        for(int i = 0; i < count; i++)
        {
            if(const float into = glm::dot(move, planes[i]); into < 0.f)
            {
                move -= planes[i] * into;
            }
        }
        for(int i = 0; i < count; i++)
        {
            if(glm::dot(move, planes[i]) < -1e-4f)
            {
                const glm::vec3 crease = glm::cross(planes[i], n);
                const float len2 = glm::dot(crease, crease);
                move = len2 > 1e-6f ? crease * (glm::dot(move, crease) / len2) : glm::vec3{0.f};
                break;
            }
        }
    }
    return hand;
}

// The hand holding a weapon (`pos`, itself out of the walls) moved so that the weapon is out of them too, as far as
// vr_gun_wall_max (further, the hand goes in by the rest, and the weapon rests where it was held until the hand comes
// back or reaches it: through a thin table top it would slip under it); true if it moved it. It slides from where it
// was last frame; held out of the walls there, even to a place that is clear (not through a thin wall to get there).
bool gunOutOfWalls(HandMemory& m, glm::vec3& pos, const std::vector<glm::vec3>& pts)
{
    const glm::vec3 target = pos;
    const bool fromLast = m.gunValid && glm::distance(m.gunPos, target) < gunRestart;
    glm::vec3 hand = fromLast ? m.gunPos : target;
    m.gunValid = true;
    m.gunPos = target;
    if(!(m.slid && fromLast) && gunPlanes(target, pts, glm::vec3{0.f}, nullptr) <= 0.f)
    {
        m.slid = false; // clear where the hand is: there
        return false;
    }
    gunDepenetrate(hand, pts); // turned into a surface since last frame: out along it
    hand = gunSlide(hand, target, pts);
    gunDepenetrate(hand, pts); // an edge between the points
    if(glm::distance(hand, target) < 0.01f)
    {
        m.slid = false; // got there: free
        return false;
    }
    m.slid = true;
    m.gunPos = hand;
    const float most = std::max(vr_gun_wall_max.value, 0.f) * units::metresToUnits() / 100.f;
    glm::vec3 off = hand - target;
    if(const float len = glm::length(off); len > most)
    {
        off *= most / len;
    }
    pos = target + off;
    return glm::length(off) > 0.01f;
}

// vr_debug_gun_wall: the hand where it is tracked and drawn, the weapon's depth in what it meets, and its lowest point
// over the surface under it (below it: into it), and the test's time.
void debugPrint(int h, const glm::vec3& tracked, const glm::vec3& drawn, const std::vector<glm::vec3>& pts, double us)
{
    const glm::vec3 d = drawn - tracked;
    float lowest = 1e9f;
    for(const glm::vec3& o : pts)
    {
        const glm::vec3 at = drawn + o;
        if(const auto tr = lineTrace(at + glm::vec3{0.f, 0.f, 32.f}, at - glm::vec3{0.f, 0.f, 64.f}); tr && tr->fraction < 1.f)
        {
            lowest = std::min(lowest, at.z - tr->endpos[2]);
        }
    }
    Con_Printf("gunwall: %s hand moved %.2f (up %.2f, across %.2f) units; depth %.2f; lowest point %.2f over the surface "
               "below; %d points, %.0f us (hand %.1f %.1f %.1f)\n",
        h == HAND_MAIN ? "main" : "off", glm::length(d), d.z, std::sqrt(d.x * d.x + d.y * d.y),
        gunPlanes(drawn, pts, glm::vec3{0.f}, nullptr), lowest, static_cast<int>(pts.size()), us, drawn.x, drawn.y, drawn.z);
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

        // The weapon (and what the hand holds) must not go through walls either: from where it was placed last frame.
        colliding[h] = false;
        if(m.valid && s.muzzleValid[h])
        {
            const glm::vec3 last = s.muzzle[h] - m.lastPos;
            if(vr_gun_wall_slide.value)
            {
                // Where the muzzle was from the hand last frame, turned as the hand is now.
                const glm::vec3 local{glm::dot(last, m.lastFwd), glm::dot(last, m.lastRight), glm::dot(last, m.lastUp)};
                const glm::vec3 muzzleOffset = hands::redirect(local, s.rot[h]);
                const glm::vec3 tracked = pos;
                const auto t0 = std::chrono::steady_clock::now();
                makeShape(h, s.rot[h], muzzleOffset);
                colliding[h] = gunOutOfWalls(m, pos, shape[h]);
                if(vr_debug_gun_wall.value >= 2.f || (vr_debug_gun_wall.value && m.slid))
                {
                    const std::chrono::duration<double, std::micro> us = std::chrono::steady_clock::now() - t0;
                    debugPrint(h, tracked, pos, shape[h], us.count());
                }
            }
            else
            {
                m.gunValid = m.slid = false;
                const glm::vec3 tracked = pos;
                colliding[h] = stopAtWall(pos, pos, pos + last, last);
                if(vr_debug_gun_wall.value >= 2.f || (vr_debug_gun_wall.value && colliding[h]))
                {
                    makeShape(h, s.rot[h], last);
                    debugPrint(h, tracked, pos, shape[h], 0.0);
                }
            }
        }
        else
        {
            m.gunValid = m.slid = false;
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

        wallPushes[h] = pos - s.pos[h];
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
        hands::angleVectors(s.rot[h], memory[h].lastFwd, memory[h].lastRight, memory[h].lastUp);
    }
}

bool gunColliding(int hand)
{
    return colliding[hand];
}

glm::vec3 wallPush(int hand)
{
    return hand == 0 || hand == 1 ? wallPushes[hand] : glm::vec3{0.f};
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
