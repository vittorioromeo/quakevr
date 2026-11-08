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
// as far as vr_gun_wall_max. Hands stay within 50 units of the torso.
// However the weapon is held: by its handle, or carried off it (taken by a hotspot or anywhere on it, or after the
// hand-off: no muzzle, its tip as drawn instead), its points in the frame of the hand carrying it; carried with the other
// hand on it too (drawn between the hands), the other hand is held out with it (ROUND22.md, "Weapons held off their
// handle collide with the level"; they went through walls and floors before).
//
// Weight: what a hand holds follows it through a spring (vr_weight.cpp), after the two-handed aim (weightDirections).
// The old engine's weight (each frame a hand moved and turned only part of the way to the tracked pose, the Speed
// Limit) is gone (round 21, "Spring only").

#include "vr_handpose.hpp"
#include "vr_engine.hpp"
#include "vr_flatplay.hpp"
#include "vr_mem.hpp"
#include "vr_avatar.hpp"
#include "vr_backend.hpp"
#include "vr_cvars.hpp"
#include "vr_modelcollide.hpp"
#include "vr_protocol.hpp"
#include "vr_trace.hpp"
#include "vr_twohand.hpp"
#include "vr_units.hpp"
#include "vr_view.hpp"
#include "vr_weight.hpp"

#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Chrono/Clock.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sqrt.hpp"
#include "Zancle/Vocabulary/Optional.hpp"
#include "vr_zancle.hpp"


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

// A strike let past the level (strikeLength): how long it still lasts after the hand slowed, how far it is let in
// (1 all the way, eased back to 0), and the body it was going for.
struct StrikePass
{
    float left{0.f};
    float level{0.f};
    int target{0};
};
StrikePass strikePasses[HAND_COUNT];
constexpr float strikePassEase = 0.15f; // s from let in to held out whole again

// Sweeps a small box from `from` to `to`. When it hits, `pos` stops along the axes the hit plane
// faces where the box stopped (the other axes are kept).
// Monsters (anything that bleeds, not a brush) don't stop it: their boxes are much bigger than they
// look, and a sword stopped at one jerked the hand back, which the server's melee took for a new
// stroke (no hit), so blows with a weapon's far end didn't register (ROUND15.md (removed 2026-10-06; git history)).
void stopAtWall(glm::vec3& pos, const glm::vec3 from, const glm::vec3 to, bool xWallsOnly = false)
{
    const glm::vec3 box{1.f};
    auto tr = worldtrace::move(from, -box, box, to, (xWallsOnly ? MOVE_NOMONSTERS : MOVE_NORMAL) | MOVE_PORTALS);
    if(tr && tr->fraction < 1.f && tr->ent && tr->ent->v.takedamage != 0.f && static_cast<int>(tr->ent->v.solid) != SOLID_BSP)
    {
        tr = worldtrace::move(from, -box, box, to, MOVE_NOMONSTERS | MOVE_PORTALS);
    }
    if(!tr || tr->fraction >= 1.f)
    {
        return;
    }

    const glm::vec3 n = worldtrace::normal(*tr);
    const glm::vec3 stop = worldtrace::endPos(*tr);
    for(int i = 0; i < 3; i++)
    {
        if(n[i] != 0.f)
        {
            pos[i] = stop[i];
        }
    }
}

// A line from `from` to `to` through the level (and the things lying in it); monsters don't stop it (stopAtWall).
[[nodiscard]] za::Optional<trace_t> lineTrace(const glm::vec3& from, const glm::vec3& to)
{
    const glm::vec3 zero{0.f};
    auto tr = worldtrace::move(from, zero, zero, to, MOVE_NORMAL | MOVE_PORTALS);
    if(tr && tr->fraction < 1.f && tr->ent && tr->ent->v.takedamage != 0.f && static_cast<int>(tr->ent->v.solid) != SOLID_BSP)
    {
        tr = worldtrace::move(from, zero, zero, to, MOVE_NOMONSTERS | MOVE_PORTALS);
    }
    return tr;
}

constexpr float gunMargin = 0.4f;  // units the weapon is kept off a surface
constexpr int gunModelPoints = 48; // of its model's points and a held prop's (modelcollide::weaponShape) at most
constexpr int gunLinePoints = 5;   // without them, points on the line from the hand to the muzzle
constexpr float gunRestart = 48.f; // units: the slide starts where the hand is when it was farther than this

// The weapon's points as offsets from the hand: the hand itself first, its muzzle, then its model's (or the line's).
za::Vector<glm::vec3> shape[HAND_COUNT];

struct Plane
{
    glm::vec3 n;
    float c; // the push `p` must have dot(p, n) >= c
};

// The weapon slide's buffers (the client's frame: the main thread).
struct HandposeScratch
{
    za::Vector<glm::vec3> model; // the weapon model's points (makeShape)
    za::Vector<Plane> planes;    // the rays' planes (gunDepenetrate)
    za::Vector<glm::vec4> fist;  // the drawn hand model's vertices (gunWallDepth_f)
    auto members() { return qvr::mem::list(model, planes, fist); }
};
mem::Scratch<HandposeScratch> scratch{"handpose"};

void makeShape(int h, const glm::vec3& rot, const glm::vec3& muzzle)
{
    za::Vector<glm::vec3>& out = shape[h];
    za::Vector<glm::vec3>& model = scratch.model;
    out.clear();
    out.pushBack(glm::vec3{0.f});
    out.pushBack(muzzle);
    if(modelcollide::weaponShape(h, rot, model) && !model.empty())
    {
        out.emplaceBackRange(model.data(), za::min<za::SizeT>(static_cast<za::SizeT>(gunModelPoints), model.size()));
        return;
    }
    for(int k = 1; k < gunLinePoints - 1; k++)
    {
        out.pushBack(muzzle * (static_cast<float>(k) / static_cast<float>(gunLinePoints - 1)));
    }
}

// The rays from the hand at `hand` to the weapon's points (`pts`) that go into a surface: each a plane there (the push
// `p` so far: the part of the ray inside, moved by it, out of that surface by gunMargin) into `planes`. The deepest
// part inside (0: clear).
float gunPlanes(const glm::vec3& hand, const za::Vector<glm::vec3>& pts, const glm::vec3& p, za::Vector<Plane>* planes)
{
    float deepest = 0.f;
    for(za::SizeT k = 1; k < pts.size(); k++)
    {
        const glm::vec3 end = hand + p + pts[k];
        const auto tr = lineTrace(hand + p, end);
        if(!tr || tr->startsolid || tr->allsolid || tr->fraction >= 1.f)
        {
            continue;
        }
        const glm::vec3 n = worldtrace::normal(*tr);
        const float depth = za::max(glm::dot(worldtrace::endPos(*tr) - end, n), 0.f);
        deepest = za::max(deepest, depth);
        if(planes)
        {
            planes->pushBack(Plane{n, depth + gunMargin + glm::dot(p, n)});
        }
    }
    return deepest;
}

// The weapon moved out of what its rays go into: the least move out of all their planes (Gauss-Seidel), tested again
// from there (three rounds at most: a corner, a curved wall).
void gunDepenetrate(glm::vec3& hand, const za::Vector<glm::vec3>& pts)
{
    za::Vector<Plane>& planes = scratch.planes;
    planes.clear();
    glm::vec3 p{0.f};
    for(int round = 0; round < 3; round++)
    {
        const za::SizeT before = planes.size();
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
                    worst = za::max(worst, short_);
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
[[nodiscard]] glm::vec3 gunSlide(glm::vec3 hand, const glm::vec3& to, const za::Vector<glm::vec3>& pts)
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
bool gunOutOfWalls(HandMemory& m, glm::vec3& pos, const za::Vector<glm::vec3>& pts)
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
    const float most = za::max(vr_gun_wall_max.value, 0.f) * units::metresToUnits() / 100.f;
    glm::vec3 off = hand - target;
    if(const float len = glm::length(off); len > most)
    {
        off *= most / len;
    }
    pos = target + off;
    return glm::length(off) > 0.01f;
}

// The tip of the weapon in `hand` as placed last frame, as tracked (less the models' drawn push, as the muzzle is): its
// muzzle; one carried off its handle (by a hotspot, anywhere on it, after the hand-off) has no aim and no muzzle: its tip
// as drawn. False with neither.
[[nodiscard]] bool lastTip(const hands::State& s, int hand, glm::vec3& out)
{
    if(s.muzzleValid[hand])
    {
        out = s.muzzle[hand];
        return true;
    }
    glm::vec3 pos, rot, tip;
    bool mirrored = false;
    if(twohand::carrying(hand) && view::carriedWeaponPose(hand, pos, rot, mirrored, tip))
    {
        out = tip - modelcollide::drawnOffset(hand);
        return true;
    }
    return false;
}

// Strikes let past the level (ROUND22.md, "Swords past the floor"): the share of the weapon in hand
// `h` (at `pos`; its points `shape[h]`) still held out of the walls and the floor now: 1 all of it; less while the hand
// swings fast with the weapon's far part (beyond that share, as it is and a frame ahead) near a monster, a corpse or a
// ragdoll, and for vr_melee_world_pass_time after it slowed, then eased back. A blade chopping at a body lying on the
// floor reached the floor first and was pushed back up, its cut missing; the game's blow is judged where it is held.
float strikeLength(int h, const hands::State& s, const glm::vec3& pos)
{
    StrikePass& p = strikePasses[h];
    const float keep = za::clamp(vr_melee_world_pass_length.value, 0.f, 1.f);
    const float dt = newFrame ? frameDt : 0.f;
    const float speed = glm::length(s.vel[h]);
    int target = 0;
    if(keep < 1.f && speed >= za::max(vr_melee_world_pass_speed.value, 0.1f) && shape[h].size() > 1)
    {
        float reach = 0.f;
        for(const glm::vec3& o : shape[h])
        {
            reach = za::max(reach, glm::length(o));
        }
        const glm::vec3 ahead = s.vel[h] * (units::metresToUnits() * za::max(frameDt, 1.f / 90.f));
        const float grow = za::max(vr_melee_world_pass_reach.value, 0.f);
        glm::vec3 lo{1e9f}, hi{-1e9f};
        for(const glm::vec3& o : shape[h])
        {
            if(glm::length(o) >= reach * keep)
            {
                lo = glm::min(lo, glm::min(pos + o, pos + o + ahead));
                hi = glm::max(hi, glm::max(pos + o, pos + o + ahead));
            }
        }
        target = lo.x <= hi.x ? modelcollide::strikeTarget(h, lo - glm::vec3{grow}, hi + glm::vec3{grow}) : 0;
        if(vr_debug_hand_offset.value == 3.f)
        {
            Con_Printf("strike pass %d: %.2f m/s, far part %.0f %.0f %.0f .. %.0f %.0f %.0f, target %d\n", h,
                static_cast<double>(speed), static_cast<double>(lo.x), static_cast<double>(lo.y), static_cast<double>(lo.z),
                static_cast<double>(hi.x), static_cast<double>(hi.y), static_cast<double>(hi.z), target);
        }
    }
    const bool was = p.level > 0.f;
    if(target)
    {
        p.left = za::max(vr_melee_world_pass_time.value, 0.f);
        p.level = 1.f;
        p.target = target;
    }
    else if(p.left > 0.f)
    {
        p.left = za::max(p.left - dt, 0.f);
    }
    else
    {
        p.level = za::max(p.level - dt / strikePassEase, 0.f);
    }
    if(vr_debug_hand_offset.value != 0.f && was != (p.level > 0.f))
    {
        Con_Printf("strike pass %s: %s at %.2f m/s, entity %d\n", h == HAND_MAIN ? "main" : "off",
            p.level > 0.f ? "on" : "off", static_cast<double>(speed), p.target);
    }
    return 1.f - p.level * (1.f - keep);
}

} // namespace

constexpr float wallSteep = 0.7f;  // gunDepth: a surface whose normal's up is more than this is a floor or a ceiling
constexpr float marginReach = 2.f; // a wall margin's level lines from a point are this many margins long (walls met at up to 60 degrees)

namespace
{

// What the line from `from` to a weapon's point `end` meets first: its normal, and how far that surface is past the
// point along it (`depth`), `margin` more for a wall (not a floor or a ceiling). With a margin and the line clear, a wall
// short of it in front of the point counts too: a level line on from the point, the way the line goes (marginReach
// margins long, less as the line points down or up), meets it (depth: the margin less the wall's way off). Level, so
// a gun's end low over the floor by a wall doesn't meet the floor first and the wall the next degree (a jump).
struct MarginHit
{
    glm::vec3 n{0.f};
    float depth{0.f};
    bool wall{false};
};
[[nodiscard]] za::Optional<MarginHit> marginTrace(const glm::vec3& from, const glm::vec3& end, float margin)
{
    if(const auto tr = lineTrace(from, end); tr && !tr->startsolid && !tr->allsolid && tr->fraction < 1.f)
    {
        MarginHit hit;
        hit.n = worldtrace::normal(*tr);
        hit.wall = za::abs(hit.n.z) <= wallSteep;
        hit.depth = za::max(glm::dot(worldtrace::endPos(*tr) - end, hit.n), 0.f) + (hit.wall ? margin : 0.f);
        return za::Optional<MarginHit>{hit};
    }
    const float len = glm::distance(from, end);
    if(margin <= 0.f || len <= 0.001f)
    {
        return za::nullOpt;
    }
    const glm::vec3 level{end.x - from.x, end.y - from.y, 0.f};
    if(glm::length(level) <= 0.001f)
    {
        return za::nullOpt;
    }
    const auto tr = lineTrace(end, end + level * (marginReach * margin / len));
    if(!tr || tr->startsolid || tr->allsolid || tr->fraction >= 1.f)
    {
        return za::nullOpt;
    }
    MarginHit hit;
    hit.n = worldtrace::normal(*tr);
    hit.wall = za::abs(hit.n.z) <= wallSteep;
    hit.depth = glm::dot(worldtrace::endPos(*tr) - end, hit.n) + margin;
    if(!hit.wall || hit.depth <= 0.f)
    {
        return za::nullOpt;
    }
    return za::Optional<MarginHit>{hit};
}

} // namespace

float gunBack(int hand, const glm::vec3& from, const glm::vec3& pos, const glm::vec3& rot, const glm::vec3& muzzle,
    const glm::vec3& way, float most, float margin)
{
    constexpr float leastOut = 0.1f; // a surface the way runs along: going back does not take the weapon off it
    makeShape(hand, rot, muzzle);
    margin = za::max(margin, 0.f);
    float back = 0.f;
    for(int round = 0; round < 3 && back < most; round++)
    {
        float more = 0.f;
        for(const glm::vec3& o : shape[hand])
        {
            const auto hit = marginTrace(from, pos + way * back + o, margin);
            if(!hit)
            {
                continue;
            }
            if(const float out = glm::dot(way, hit->n); out > leastOut)
            {
                more = za::max(more, (hit->depth + gunMargin) / out);
            }
        }
        if(more <= 0.f)
        {
            break;
        }
        back += more;
    }
    return za::min(back, most);
}

float gunDepth(int hand, const glm::vec3& from, const glm::vec3& pos, const glm::vec3& rot, const glm::vec3& muzzle,
    float margin, float handMargin, glm::vec3* normal)
{
    makeShape(hand, rot, muzzle);
    float deepest = 0.f;
    for(za::SizeT i = 0; i < shape[hand].size(); i++)
    {
        // (Walls only: a floor or a ceiling, steeper than wallSteep, is not counted -- looking down, the gun's end in the
        // floor stays hidden there as before, and the gun doesn't tilt up at the ground. The hand's own point is first.)
        const auto hit = marginTrace(from, pos + shape[hand][i], za::max(i == 0 ? handMargin : margin, 0.f));
        if(hit && hit->wall && hit->depth > deepest)
        {
            deepest = hit->depth;
            if(normal)
            {
                *normal = hit->n;
            }
        }
    }
    return deepest;
}

float gunFloorDepth(int hand, const glm::vec3& from, const glm::vec3& pos, const glm::vec3& rot, const glm::vec3& muzzle,
    float margin)
{
    makeShape(hand, rot, muzzle);
    const glm::vec3 drop{0.f, 0.f, za::max(margin, 0.f)};
    float deepest = 0.f;
    for(const glm::vec3& o : shape[hand])
    {
        const glm::vec3 end = pos + o - drop; // (the point `margin` lower: a floor that near counts as under it)
        const auto tr = lineTrace(from, end);
        if(!tr || tr->startsolid || tr->allsolid || tr->fraction >= 1.f)
        {
            continue;
        }
        const glm::vec3 n = worldtrace::normal(*tr);
        if(n.z > wallSteep)
        {
            deepest = za::max(deepest, glm::dot(worldtrace::endPos(*tr) - end, n));
        }
    }
    return deepest;
}

float gunNearest(int hand, const glm::vec3& eye, const glm::vec3& pos, const glm::vec3& rot, const glm::vec3& muzzle,
    const hands::State* s, float* torsoDepth)
{
    makeShape(hand, rot, muzzle);
    float nearest = 1e9f, deepest = 0.f;
    for(za::SizeT i = 0; i < shape[hand].size(); i++)
    {
        const glm::vec3 p = pos + shape[hand][i];
        nearest = za::min(nearest, glm::distance(eye, p));
        // (Each point and the line from the one before: the barrel across the face, the body through the chest.)
        if(i > 0)
        {
            const glm::vec3 q = pos + shape[hand][i - 1];
            const glm::vec3 d = p - q;
            const float len2 = glm::dot(d, d);
            const float t = len2 > 1e-6f ? za::clamp(glm::dot(eye - q, d) / len2, 0.f, 1.f) : 0.f;
            nearest = za::min(nearest, glm::distance(eye, q + d * t));
            if(s && torsoDepth)
            {
                deepest = za::max(deepest, -flatplay::torsoGap(*s, q, p));
            }
        }
    }
    if(torsoDepth)
    {
        *torsoDepth = deepest;
    }
    return nearest;
}

float pointWallDepth(const glm::vec3& from, const glm::vec3& point, float margin)
{
    const auto hit = marginTrace(from, point, za::max(margin, 0.f));
    return hit && hit->wall ? za::max(hit->depth, 0.f) : 0.f;
}

void resolvePositions(hands::State& s, float /* turnYaw */)
{
    // On the real clock, every frame: cl.time moves in steps with the server's messages (72 Hz, or
    // 48 and 72 by turns at 144 fps), so a weight stepped on it moved the hands in uneven jerks that
    // the server's melee read as a wrist speeding up and slowing down (up to half again at 144 fps).
    frameDt = lastTime >= 0.0 ? static_cast<float>(za::clamp(vr_gametime - lastTime, 0.0, 0.1)) : 0.f;
    newFrame = vr_gametime != lastTime;
    lastTime = vr_gametime;

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

    glm::vec3 gunPush[HAND_COUNT]{glm::vec3{0.f}, glm::vec3{0.f}};
    for(int h = 0; h < HAND_COUNT; h++)
    {
        HandMemory& m = memory[h];
        glm::vec3 pos = s.pos[h];
        // (A hand on a blade in a body: its place says how the blade slides. A body dying is no monster to the stop above for the moment
        // its box is still solid, and a stop that comes and goes, a hand inside that box, read as a tug that pulled it out.)
        const bool onBlade = cl.stats[h == HAND_MAIN ? protocol::STAT_QVR_EMBEDMAIN : protocol::STAT_QVR_EMBEDOFF] > 0;
        stopAtWall(pos, torso, s.pos[h], onBlade);

        // The weapon (and what the hand holds) must not go through walls either: from where it was placed last frame.
        // However it is held: by its handle, or carried off it (by a hotspot, anywhere on it, after the hand-off), its
        // shape is its drawn model's in the frame of the hand holding or carrying it.
        colliding[h] = false;
        glm::vec3 tip{0.f};
        if(m.valid && lastTip(s, h, tip))
        {
            const glm::vec3 before = pos;
            const glm::vec3 last = tip - m.lastPos;
            // Where the muzzle was from the hand last frame, turned as the hand is now.
            const glm::vec3 local{glm::dot(last, m.lastFwd), glm::dot(last, m.lastRight), glm::dot(last, m.lastUp)};
            const glm::vec3 muzzleOffset = hands::redirect(local, s.rot[h]);
            makeShape(h, s.rot[h], muzzleOffset);
            if(const float length = strikeLength(h, s, pos); length < 1.f)
            {
                for(glm::vec3& o : shape[h])
                {
                    o *= length; // (the hand's own point, the first, stays at the hand)
                }
            }
            colliding[h] = gunOutOfWalls(m, pos, shape[h]);
            gunPush[h] = pos - before;
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

    // A weapon carried off its handle with the other hand on it too is placed between the hands (vr_twohand.cpp's
    // carriedInBoth): the other hand held out with the carrying one, so that the weapon moves as a whole by the push.
    for(int h = 0; h < HAND_COUNT; h++)
    {
        const int other = 1 - h;
        if(glm::dot(gunPush[h], gunPush[h]) > 0.f && twohand::carrying(h) && twohand::freeHelping(other))
        {
            s.pos[other] += gunPush[h];
            wallPushes[other] += gunPush[h];
            memory[other].lastPos = s.pos[other];
        }
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

constexpr float offMost = 32.f; // gunWallDepth_f: how far off a wall it looks (units)

namespace
{

// How far in front of a wall (not a floor or a ceiling) a point is, on the line from the eyes on past it (negative: in it;
// at most offMost).
[[nodiscard]] float wallOff(const glm::vec3& p)
{
    const glm::vec3 eyes = hands::current().head;
    const float len = glm::distance(eyes, p);
    const glm::vec3 to = len > 0.001f ? p + (p - eyes) * (offMost / len) : p;
    const auto tr = lineTrace(eyes, to);
    if(!tr || tr->startsolid || tr->allsolid || tr->fraction >= 1.f || za::abs(worldtrace::normal(*tr).z) > wallSteep)
    {
        return offMost;
    }
    return za::min(-glm::dot(worldtrace::endPos(*tr) - p, worldtrace::normal(*tr)), offMost);
}

// vr_gun_wall_depth's second line for hand `h`: its drawn hand model's vertex nearest a wall (off; negative: in it; -1
// none drawn) and the weapon's (`gunOff`, cm; -1 none); and the weapon's point nearest the eyes (`eye`, cm; -1 none).
void fistLine(int h, float gunOff, float eye)
{
    const float cm = 100.f / units::metresToUnits();
    float fistOff = -1.f;
    if(view::drawnHandSurface(h, scratch.fist, nullptr) && !scratch.fist.empty())
    {
        fistOff = offMost;
        for(const glm::vec4& p : scratch.fist)
        {
            fistOff = za::min(fistOff, wallOff(glm::vec3{p}));
        }
        fistOff *= cm;
    }
    Con_Printf("gunwalloff: %s fist off %.1fcm gun off %.1fcm eye %.1fcm\n", h == HAND_MAIN ? "main" : "off", fistOff, gunOff,
        eye);
}

} // namespace

void gunWallDepth_f()
{
    za::Vector<glm::vec3>& pts = scratch.model;
    const float cm = 100.f / units::metresToUnits();
    for(int h = 0; h < HAND_COUNT; h++)
    {
        const char* name = h == HAND_MAIN ? "main" : "off";
        glm::vec3 from{0.f};
        if(!modelcollide::drawnWeaponPoints(h, pts, from))
        {
            Con_Printf("gunwall: %s hand, no weapon drawn\n", name);
            fistLine(h, -1.f, -1.f);
            continue;
        }
        const char* how = twohand::carrying(h) ? (twohand::freeHelping(1 - h) ? "carried2h" : "carried")
                        : twohand::helping(1 - h) ? (twohand::freeHelping(1 - h) ? "held+anywhere" : "held+hotspot")
                                                  : "held";
        // The drawn weapon's points inside the level: the line from the eyes to each goes into a surface, this deep past it
        // (from the eyes, not the hand: an empty hand may be drawn a little into a wall, and the lines from it start there).
        const glm::vec3 eyes = hands::current().head;
        const auto depthOf = [&](const glm::vec3& p) {
            const auto tr = lineTrace(eyes, p);
            if(!tr || tr->startsolid || tr->allsolid || tr->fraction >= 1.f)
            {
                return 0.f;
            }
            return za::max(glm::dot(worldtrace::endPos(*tr) - p, worldtrace::normal(*tr)), 0.f);
        };
        float deepest = 0.f;
        int inside = 0;
        glm::vec3 reach{0.f};
        for(const glm::vec3& p : pts)
        {
            if(glm::distance(p, from) > glm::length(reach))
            {
                reach = p - from;
            }
            if(const float depth = depthOf(p); depth > 0.01f)
            {
                inside++;
                deepest = za::max(deepest, depth);
            }
        }
        float gunOff = offMost, eye = 1e9f;
        for(const glm::vec3& p : pts)
        {
            gunOff = za::min(gunOff, wallOff(p));
            eye = za::min(eye, glm::distance(p, eyes));
        }
        // (Short: one console line. in: points in the level of all; out: the hand held out by the walls; hand in: the hand
        // itself; reach: its farthest point from the hand.)
        Con_Printf("gunwall: %s %s in %d/%d deep %.1fcm out %.1fcm hand in %.1fcm at %.0f %.0f %.0f reach %.0f %.0f %.0f\n",
            name, how, inside, static_cast<int>(pts.size()), deepest * cm, glm::length(wallPushes[h]) * cm, depthOf(from) * cm,
            from.x, from.y, from.z, reach.x, reach.y, reach.z);
        fistLine(h, gunOff * cm, eye * cm);
    }
    pts.clear();
    scratch.fist.clear();
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
