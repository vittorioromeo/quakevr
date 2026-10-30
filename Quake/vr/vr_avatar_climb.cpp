// vr_avatar_climb.cpp -- the body's feet climbing (vr_body_climb_feet; see vr_avatar.hpp): footholds found
//    by traces, the steps between them, the hanging legs, the logs. Moved out of vr_avatar.cpp; what they share
//    is vr_avatar_internal.hpp.

#include "vr_modelmetadata.hpp"
#include "vr_trace.hpp"
#include "vr_progs.hpp"
#include "vr_avatar.hpp"
#include "vr_avatar_internal.hpp"
#include "vr_agarmor.hpp"
#include "vr_selfcollide.hpp"
#include "vr_engine.hpp"
#include "vr_units.hpp"
#include "vr_backend.hpp"
#include "vr_bodycal.hpp"
#include "vr_bodytrack.hpp"
#include "vr_box3d.hpp"
#include "vr_climb.hpp"
#include "vr_cvars.hpp"
#include "vr_lines.hpp"
#include "vr_motion.hpp"
#include "vr_profile.hpp"
#include "vr_ragdoll.hpp"
#include "vr_seated.hpp"
#include "vr_view.hpp"

#include "Zancle/Container/Array.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Acos.hpp"
#include "Zancle/Math/Asin.hpp"
#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/Copysign.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Exp.hpp"
#include "Zancle/Math/Floor.hpp"
#include "Zancle/Math/Fmod.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Remainder.hpp"
#include "Zancle/Math/Round.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/Math/Sqrt.hpp"
#include "Zancle/String/String.hpp"
#include "vr_zancle.hpp"
#include "vr_rig2.hpp"

#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>

#include <string.h>
#include "vr_geom.hpp"

namespace qvr::avatar
{

using namespace detail;

namespace
{

// Climbing (vr_body_climb_feet; BACKLOG "Climbing leg animation"): while a hand holds on (climb::heldAt), each foot
// finds a foothold below its hip, within reach (findFoothold: a handful of traces, only when it looks for one; through
// the level and, vr_body_climb_feet_props, the physics props' shapes as they lie: footTrace): a
// ledge's, a rung's or the floor's top, else a spot on the face in front of it (at the 16-unit spacing of a flat
// ladder's rungs), else none: it hangs, a little bent, swinging slightly and trailing the body's motion. A foot on a
// hold stays where it is in the world while the body moves, until it is out of reach (the leg stretched, the knee
// pressed into the chest, too far to the side) or, climbing up, left low; it then steps to a new hold on a short arc
// out from the face, the other foot waiting for it (they take turns). Only drawn: nothing collides.

} // namespace

namespace detail
{

Climbing climbing;

} // namespace detail

namespace
{

constexpr float CLIMB_STEP_TIME = 0.35f; // seconds a step to a new hold takes (vr_body_climb_feet_speed 1)
constexpr float CLIMB_STEP_OUT = 0.12f;  // metres the stepping foot swings out from the face past its own length (its toes clear the lips)
constexpr float CLIMB_RUNGS = 16.f;      // units: a face's spots at this spacing (a flat ladder's rungs)

[[nodiscard]] float smooth01(float x)
{
    return x * x * (3.f - 2.f * x);
}

struct FootHit
{
    bool hit{false};   // met a surface (and didn't start in one)
    bool solid{false}; // started in one
    float frac{1.f};
    glm::vec3 pos{0.f};
    glm::vec3 normal{0.f};
};

// A point trace on the listen server (the world and brush models, the player passed through); none without one.
[[nodiscard]] FootHit footTrace(const glm::vec3& a, const glm::vec3& z)
{
    FootHit h;
    if(!progs::localPlayer())
    {
        return h;
    }
    climbing.traces++;
    const progs::ServerVm vm; // (drawing: the client's VM, or none, is in)
    const trace_t tr = worldtrace::serverLine(a, z, progs::localPlayer());
    h.solid = tr.startsolid || tr.allsolid;
    h.hit = !h.solid && tr.fraction < 1.f;
    h.frac = tr.fraction;
    h.pos = {tr.endpos[0], tr.endpos[1], tr.endpos[2]};
    h.normal = {tr.plane.normal[0], tr.plane.normal[1], tr.plane.normal[2]};
    // The physics props' shapes as they lie (a crate climbed: its top and its faces; the trace above passes them by), up
    // to where the level stopped it.
    box3d::PropHit p;
    bool inside = false;
    if(!h.solid && vr_body_climb_feet_props.value != 0.f && box3d::propTrace(a, h.pos, p, inside))
    {
        if(inside)
        {
            h.solid = true;
            h.hit = false;
        }
        else
        {
            h.hit = true;
            h.frac *= p.fraction;
            h.pos = p.point;
            h.normal = p.normal;
        }
    }
    return h;
}

[[nodiscard]] float legLength(int side)
{
    const int thigh = side == 0 ? ThighL : ThighR;
    return boneLength(thigh, thigh + 1) + boneLength(thigh + 1, thigh + 2);
}

} // namespace

namespace detail
{

// The foot's length ahead of the ankle (metres, flat) and the ankle's height over the sole.
[[nodiscard]] float toeLength(int side)
{
    const int foot = side == 0 ? FootL : FootR;
    const glm::vec3 toe = bind().toe[side] - bind().pos[foot];
    return glm::length(glm::vec2{toe.x, toe.y});
}

[[nodiscard]] float ankleHeight(int side)
{
    return bind().pos[side == 0 ? FootL : FootR].z;
}

// The foot of `side`'s side (outward from the body, flat) while climbing.
[[nodiscard]] glm::vec3 climbOutward(int side)
{
    const glm::vec3 across{-climbing.in.y, climbing.in.x, 0.f}; // left of the way in
    return side == 0 ? across : -across;
}

} // namespace detail

namespace
{

// A foothold for the foot of `side`, its hip at `hip`: where the ankle goes (`at`), its kind (1 the face, 2 a top) and
// the way the foot points (`fwd`, into the face). Looks straight in at the preferred height (half the leg below the hip
// climbing up, more hanging) and, missing, towards the holds (a pillar, a pipe); then down just inside what it met
// from higher up (its top) and just in front of it from the preferred height (a ledge or a rung sticking out lower;
// the holds' plane when nothing was met: a free-standing ladder's rungs); the top nearest the preferred height wins,
// else the face. The toes kept out of the face; out of reach or inside something: none.
[[nodiscard]] bool findFoothold(const Body& b, int side, const glm::vec3& hip, glm::vec3& at, int& kind, glm::vec3& fwd)
{
    const Climbing& c = climbing;
    const float u = b.m2w;
    const float leg = legLength(side) * b.legM2w;
    const float reach = CLAMP(0.5f, vr_body_climb_feet_reach.value, 1.2f);
    const bool up = c.rise > 0.08f;
    const float zTop = hip.z - 0.25f * leg;
    const float zPref = hip.z - (up ? 0.5f : 0.6f) * leg * za::min(1.f, reach);
    const float zBot = hip.z - 0.92f * leg * reach;
    const float toe = toeLength(side) * u;
    const float ankle = ankleHeight(side) * u;
    const glm::vec3 col = glm::vec3{hip.x, hip.y, 0.f} + climbOutward(side) * (0.05f * u);
    const glm::vec3 p0{col.x, col.y, zPref};
    const float range = 1.f * u;

    // The face ahead.
    glm::vec3 dir = c.in;
    FootHit face = footTrace(p0, p0 + dir * range);
    if(face.solid)
    {
        return false; // the hip's column in something (over a top): no room for a foot
    }
    if(!face.hit)
    {
        const glm::vec3 toMid{c.holdMid.x - p0.x, c.holdMid.y - p0.y, 0.f};
        const float len = glm::length(toMid);
        if(len > 1.f && glm::dot(toMid / len, c.in) > 0.5f && glm::dot(toMid / len, c.in) < 0.99f)
        {
            dir = toMid / len;
            face = footTrace(p0, p0 + dir * range);
        }
    }
    const float planeDepth = CLAMP(0.f, glm::dot(c.holdMid - p0, dir), range); // the holds' lip
    const float depth = face.hit ? face.frac * range : planeDepth;
    const float maxReach = 0.97f * leg;
    // Within reach, and far enough under the hip for how far out it is (a hold almost level with the hips, far out, has
    // the legs sticking out straight: no support).
    const auto usable = [&](const glm::vec3& ankleAt) {
        const glm::vec3 d = ankleAt - hip;
        return glm::length(d) <= maxReach && -d.z >= 0.45f * glm::length(glm::vec2{d.x, d.y});
    };

    // Tops (the first under the highest a foot goes): just inside what was met, or at the holds' lip if that is nearer (a
    // ladder's or a rung wall's rungs, in front of the wall); then in front of what was met, from the preferred height
    // down (a ledge sticking out lower). Not usable: not one.
    float bestZ = 0.f;
    float bestScore = 1e9f;
    glm::vec3 bestCol{0.f};
    const auto consider = [&](const FootHit& h, const glm::vec3& column) {
        const glm::vec3 ankleAt = glm::vec3{column.x, column.y, h.pos.z + ankle} - dir * (toe * 0.45f);
        if(h.hit && h.normal.z > 0.7f && h.pos.z >= zBot && h.pos.z <= zTop && usable(ankleAt))
        {
            const float score = za::abs(h.pos.z - zPref) * (h.pos.z > zPref ? 0.7f : 1.f); // climbing up: a higher one
            if(score < bestScore)
            {
                bestScore = score;
                bestZ = h.pos.z;
                bestCol = column;
            }
        }
    };
    {
        const glm::vec3 inside = p0 + dir * za::min(depth + 3.f, planeDepth + 2.f);
        consider(footTrace({inside.x, inside.y, zTop}, {inside.x, inside.y, zBot}), inside);
    }
    if(face.hit && depth > 2.f)
    {
        const glm::vec3 front = p0 + dir * (depth - 1.5f);
        consider(footTrace({front.x, front.y, zPref}, {front.x, front.y, zBot}), front);
    }

    // The foot out of the face: traced in from behind the heel to past the toes, just over the sole and just over the
    // ankle (a face at a rung's underside); met, the foot back that much. Starting in something, or then not usable:
    // false.
    const auto place = [&](glm::vec3& p) {
        float pull = 0.f;
        for(const float z : {p.z - ankle + 1.5f, p.z + 1.5f})
        {
            const glm::vec3 a{p.x, p.y, z};
            const FootHit h = footTrace(a - dir * toe, a + dir * (toe + 1.f));
            if(h.solid)
            {
                return false;
            }
            if(h.hit)
            {
                pull = za::max(pull, (1.f - h.frac) * (2.f * toe + 1.f));
            }
        }
        p -= dir * pull;
        return usable(p);
    };

    fwd = dir;
    if(bestScore < 1e8f)
    {
        // The ball of the foot on the top's edge.
        at = glm::vec3{bestCol.x, bestCol.y, bestZ + ankle} - dir * (toe * 0.45f);
        kind = 2;
        return place(at);
    }
    if(face.hit && za::abs(face.normal.z) < 0.6f)
    {
        // At the spacing's height under the preferred (the knee less high), else the preferred, else the spacing's over
        // it.
        const float lo = CLAMP(zBot + ankle, za::floor(zPref / CLIMB_RUNGS) * CLIMB_RUNGS, zTop);
        kind = 1;
        for(const float z : {lo, CLAMP(zBot + ankle, zPref, zTop), CLAMP(zBot + ankle, lo + CLIMB_RUNGS, zTop)})
        {
            at = glm::vec3{face.pos.x, face.pos.y, z} - dir * toe;
            if(place(at))
            {
                return true;
            }
        }
        return false;
    }
    return false;
}

// Whether the player climbs (a hand on a hold): the holds' middle and the way into their face.
[[nodiscard]] bool climbHolds(const Body& b, glm::vec3& mid, glm::vec3& in)
{
    int n = 0;
    glm::vec3 sumAt{0.f}, sumOut{0.f};
    for(int hand = 0; hand < 2; hand++)
    {
        glm::vec3 at, out;
        if(climb::heldAt(hand, at, out))
        {
            sumAt += at;
            sumOut += out;
            n++;
        }
    }
    if(n == 0)
    {
        return false;
    }
    mid = sumAt / static_cast<float>(n);
    in = -safeNormalize(glm::vec3{sumOut.x, sumOut.y, 0.f}, -b.fwd);
    return true;
}

// Where a hanging foot goes: under its hip, the knee a little bent, a little ahead (towards the face), swinging slowly
// and trailing the body's motion.
[[nodiscard]] glm::vec3 hangingFoot(const Body& b, int side, const glm::vec3& hip)
{
    const float u = b.m2w;
    const float leg = legLength(side) * b.legM2w;
    const float swing = za::sin(climbing.sway + (side == 0 ? 0.f : 2.1f));
    return hip - UP * (0.92f * leg) + climbing.in * ((0.1f + 0.04f * swing) * u) + climbOutward(side) * (0.03f * u) -
           climbing.lag;
}

} // namespace

namespace detail
{

void updateClimbing(const Body& b, float dt, bool legs)
{
    QVR_PROFILE("avatar climb feet");
    Climbing& c = climbing;
    c.traces = 0;
    glm::vec3 mid, in;
    const bool on = legs && vr_body_climb_feet.value != 0.f && climbHolds(b, mid, in);
    // Off: the weight waits while a foot steps out from the face off its hold (the toes clear of the lip first).
    bool steppingOut = false;
    for(const ClimbFoot& f : c.feet)
    {
        steppingOut = steppingOut || (!f.held && f.step >= 0.f && f.step < 0.3f) || (!on && f.held);
    }
    c.weight = on ? za::min(1.f, c.weight + dt * 5.f) : steppingOut ? c.weight : za::max(0.f, c.weight - dt * 3.f);
    climbHang = c.weight;
    if(!on && c.weight <= 0.f)
    {
        for(ClimbFoot& f : c.feet)
        {
            f.held = false;
            f.step = -1.f;
        }
        c.hipsValid = false;
        return;
    }
    if(on)
    {
        c.in = in;
        c.holdMid = mid;
    }
    else
    {
        // Let go (a fall, a mantle): the feet step off their holds to hang (out from the face first), easing back to
        // the legs' own pose.
        for(ClimbFoot& f : c.feet)
        {
            if(f.held)
            {
                f.from = f.now;
                f.held = false;
                f.step = 0.f;
            }
            f.retry = 1.f;
        }
    }

    const glm::vec3 hips = (childPos(b, ThighL) + childPos(b, ThighR)) * 0.5f;
    const bool jumped = c.hipsValid && glm::distance(hips, c.lastHips) > legLength(0) * b.m2w; // a teleport
    if(c.hipsValid && dt > 0.f && !jumped)
    {
        const glm::vec3 v = (hips - c.lastHips) / dt;
        c.rise += (v.z / b.m2w - c.rise) * (1.f - za::exp(-4.f * dt));
        const glm::vec3 flat{v.x, v.y, 0.f};
        const glm::vec3 lagTo = flat * 0.08f;
        const float maxLag = 0.2f * b.m2w;
        c.lag += ((glm::length(lagTo) > maxLag ? lagTo * (maxLag / glm::length(lagTo)) : lagTo) - c.lag) *
                 (1.f - za::exp(-5.f * dt));
    }
    if(!c.hipsValid || jumped)
    {
        for(ClimbFoot& f : c.feet)
        {
            f.held = false;
            f.step = -1.f;
            f.retry = 0.f;
        }
        c.rise = 0.f;
        c.lag = glm::vec3{0.f};
    }
    c.lastHips = hips;
    c.hipsValid = true;
    c.sway = za::fmod(c.sway + dt * 1.6f, glm::two_pi<float>());

    // Which feet want a new hold.
    const float reach = CLAMP(0.5f, vr_body_climb_feet_reach.value, 1.2f);
    const float speed = CLAMP(0.3f, vr_body_climb_feet_speed.value, 3.f);
    za::Array<bool, 2> want{false, false};
    za::Array<glm::vec3, 2> hip;
    for(int side = 0; side < 2; side++)
    {
        ClimbFoot& f = c.feet[side];
        hip[side] = childPos(b, side == 0 ? ThighL : ThighR);
        const float leg = legLength(side) * b.legM2w;
        if(f.step >= 0.f)
        {
            f.step = za::min(1.f, f.step + dt * speed / CLIMB_STEP_TIME);
            if(f.step >= 1.f)
            {
                f.step = -1.f;
            }
        }
        else if(f.held)
        {
            const glm::vec3 d = f.at - hip[side];
            const ClimbFoot& other = c.feet[1 - side];
            f.retry -= dt;
            const bool over = glm::length(d) > 0.98f * leg;
            if(over && other.step >= 0.f)
            {
                // Out of reach while the other steps: it lets go (and looks again once the other is done).
                f.from = f.now;
                f.held = false;
                f.step = 0.f;
                f.retry = 0.f;
                continue;
            }
            want[side] = over || (f.retry <= 0.f && (d.z > -0.15f * leg ||
                         za::abs(glm::dot(f.at - hips, climbOutward(0))) > 0.45f * b.m2w ||
                         (c.rise > 0.08f && d.z < -0.8f * leg * reach && other.held && other.step < 0.f)));
        }
        else
        {
            f.retry -= dt;
            want[side] = f.retry <= 0.f;
        }
    }

    // One foot at a time: the one whose hold is worse off (hanging first, then the lower), not the one that just went.
    int seek = -1;
    if(c.feet[0].step < 0.f && c.feet[1].step < 0.f)
    {
        if(want[0] && want[1])
        {
            const bool held0 = c.feet[0].held, held1 = c.feet[1].held;
            seek = held0 != held1 ? (held0 ? 1 : 0)
                 : c.lastStep >= 0 ? 1 - c.lastStep
                                   : (c.feet[0].at.z <= c.feet[1].at.z ? 0 : 1);
        }
        else
        {
            seek = want[0] ? 0 : want[1] ? 1 : -1;
        }
    }
    else
    {
        // The other is stepping: a hanging foot may still look (both feet off their holds at the start).
        for(int side = 0; side < 2; side++)
        {
            if(want[side] && !c.feet[side].held && c.feet[1 - side].step >= 0.f && c.feet[1 - side].step > 0.5f)
            {
                seek = side;
            }
        }
    }

    if(seek >= 0)
    {
        ClimbFoot& f = c.feet[seek];
        const double t0 = vr_debug_climb_feet.value ? Sys_DoubleTime() : 0.0;
        const int traces0 = c.traces;
        glm::vec3 at{0.f}, fwd{0.f};
        int kind = 0;
        const bool found = findFoothold(b, seek, hip[seek], at, kind, fwd);
        const glm::vec3 was = f.held || f.step >= 0.f ? f.now : hangingFoot(b, seek, hip[seek]);
        if(found && f.held && glm::distance(at, f.at) < 3.f)
        {
            f.retry = 0.2f; // the same hold again: it stays (and looks again later)
        }
        else if(found)
        {
            f.from = was;
            f.at = at;
            f.fwd = fwd;
            f.kind = kind;
            f.step = 0.f;
            f.held = true;
            f.serial++;
            c.lastStep = seek;
        }
        else
        {
            // None: it hangs (easing there from a hold).
            f.from = was;
            f.step = f.held ? 0.f : f.step;
            f.held = false;
            f.retry = 0.25f;
        }
        if(vr_debug_climb_feet.value)
        {
            const float u = b.m2w;
            Con_Printf("climbfoot %s %s %.1f %.1f %.1f (hip %.1f %.1f %.1f): %.0f cm under the hip, %.0f cm in; %d traces, "
                       "%.3f ms\n",
                seek == 0 ? "L" : "R", !found ? "none" : kind == 2 ? "top" : "face", at.x, at.y, at.z, hip[seek].x,
                hip[seek].y, hip[seek].z,
                found ? (hip[seek].z - at.z) / u * 100.f : 0.f, found ? glm::dot(at - hip[seek], c.in) / u * 100.f : 0.f,
                c.traces - traces0, (Sys_DoubleTime() - t0) * 1000.0);
        }
    }

    // Where each ankle is aimed.
    for(int side = 0; side < 2; side++)
    {
        ClimbFoot& f = c.feet[side];
        // A step (to a hold, or off one to hang): out from the face first, across, then in onto the hold, so that the
        // toes clear the lip left and the one above.
        const glm::vec3 to = f.held ? f.at : hangingFoot(b, side, hip[side]);
        if(f.step >= 0.f)
        {
            const float s = f.step;
            const float outW = smooth01(za::min(1.f, za::min(s / 0.3f, (1.f - s) / 0.25f)));
            const float e = smooth01(CLAMP(0.f, (s - 0.15f) / 0.65f, 1.f));
            const float out = (f.held ? 1.f : 1.f - e) * outW * (toeLength(side) + CLIMB_STEP_OUT) * b.m2w;
            f.now = glm::mix(f.from, to, e) - f.fwd * out + UP * (za::sin(glm::pi<float>() * s) * 0.03f * b.m2w);
            f.flat = f.held ? 1.f : 1.f - e;
        }
        else
        {
            f.now = to;
            f.flat = f.held ? 1.f : 0.f;
        }
    }
}

// The climbing feet each frame (vr_debug_climb_feet 2): each foot's state (0 hanging, 1 on its hold, 2 stepping), its
// hold's number, where the drawn ankle is and whether the ankle or the toes are inside something.
void logClimbing(const Body& b)
{
    if(vr_debug_climb_feet.value < 2.f || climbing.weight <= 0.f)
    {
        return;
    }
    char line[256];
    int n = q_snprintf(line, sizeof(line), "climbfeet %.3f w %.2f rise %.2f us %.1f", cl.time, climbing.weight,
        climbing.rise, climbing.updateMs * 1000.0);
    for(int side = 0; side < 2; side++)
    {
        const ClimbFoot& f = climbing.feet[side];
        const Bone& bone = b.bones[side == 0 ? FootL : FootR];
        const glm::vec3 tip = bone.pos + bone.rot[0] * (toeLength(side) * b.m2w);
        const bool inside = footTrace(bone.pos, bone.pos).solid || footTrace(tip, tip).solid;
        const int state = !f.held ? 0 : f.step >= 0.f ? 2 : 1;
        n += q_snprintf(line + n, sizeof(line) - static_cast<size_t>(n), " %s %d %d %.2f %.2f %.2f %d", side == 0 ? "L" : "R",
            state, f.serial, bone.pos.x, bone.pos.y, bone.pos.z, inside ? 1 : 0);
    }
    Con_Printf("%s\n", line);
    if(vr_debug_climb_feet.value >= 3.f)
    {
        for(const ClimbFoot& f : climbing.feet)
        {
            if(f.held)
            {
                lines::point(f.at, 1.2f, {0.2f, 1.f, 0.3f, 0.9f});
                lines::line(f.now, f.at, 0.3f, {0.2f, 1.f, 0.3f, 0.6f}, {0.2f, 1.f, 0.3f, 0.9f});
            }
        }
    }
}

} // namespace detail

} // namespace qvr::avatar
