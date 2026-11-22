// vr_pmove.cpp -- a player's walk, one move at a time (vr_pmove.hpp; MP_PLAN.md, B8). Each piece follows its sv_user.c /
// sv_phys.c original line by line, with the move's dt for host_frametime and the Body's hooks for the edicts.

#include "vr_pmove.hpp"

#include <glm/glm.hpp>

#include "vr_ledges.hpp"

#include "Zancle/Base/IsFinite.hpp"
#include "Zancle/Math/Acos.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/Math/Tan.hpp"

#include <math.h>

namespace qvr::pmove
{
namespace
{

constexpr float stopEpsilon = 0.1f;
constexpr int maxClipPlanes = 5;

// SV_PushEntity: moved by `move` as far as it goes, linked (the server's touches).
trace_t push(Body& b, const float* move)
{
    vec3_t end;
    VectorAdd(b.origin, move, end);
    const trace_t tr = b.trace(b.origin, end);
    VectorCopy(tr.endpos, b.origin);
    b.pushed(tr);
    return tr;
}

// SV_UserFriction.
void friction(Body& b, const Rules& r, double dt)
{
    float* vel = b.velocity;
    const float speed = sqrtf(vel[0] * vel[0] + vel[1] * vel[1]);
    if(!speed)
    {
        return;
    }
    const float fr = b.overDropoff(vel, speed) ? r.friction * r.edgefriction : r.friction;
    const float control = speed < r.stopspeed ? r.stopspeed : speed;
    float newspeed = static_cast<float>(speed - dt * control * fr); // (in double, as sv_user.c's host_frametime)
    if(newspeed < 0.f)
    {
        newspeed = 0.f;
    }
    newspeed /= speed;
    vel[0] = vel[0] * newspeed;
    vel[1] = vel[1] * newspeed;
    vel[2] = vel[2] * newspeed;
}

// SV_Accelerate.
void accelerate(Body& b, float wishspeed, const float* wishdir, const Rules& r, double dt)
{
    const float currentspeed = DotProduct(b.velocity, wishdir);
    const float addspeed = wishspeed - currentspeed;
    if(addspeed <= 0.f)
    {
        return;
    }
    float accelspeed = static_cast<float>(r.accelerate * dt * wishspeed);
    if(accelspeed > addspeed)
    {
        accelspeed = addspeed;
    }
    for(int i = 0; i < 3; i++)
    {
        b.velocity[i] += accelspeed * wishdir[i];
    }
}

// SV_AirAccelerate.
void airAccelerate(Body& b, float wishspeed, float* wishveloc, const Rules& r, double dt)
{
    float wishspd = VectorNormalize(wishveloc);
    if(wishspd > 30.f)
    {
        wishspd = 30.f;
    }
    const float currentspeed = DotProduct(b.velocity, wishveloc);
    const float addspeed = wishspd - currentspeed;
    if(addspeed <= 0.f)
    {
        return;
    }
    float accelspeed = static_cast<float>(r.accelerate * wishspeed * dt);
    if(accelspeed > addspeed)
    {
        accelspeed = addspeed;
    }
    for(int i = 0; i < 3; i++)
    {
        b.velocity[i] += accelspeed * wishveloc[i];
    }
}

// SV_AirMove's slide (VR_SlideMove; vr_slide.cpp's slide::move, one function for both): a headset player's (B14), on
// the move's slide input -- true when it moved him on the ground (no friction, no walk). Its stamina's parts as they
// fall due (in the air too: the same slide), the server's QuakeC paying them (Body::slidePaid).
bool slideMove(Body& b, const Input& in, const Rules& r, bool onground, const float* wishdir, float wishspeed)
{
    slide::Slide& s = b.slide;
    slide::MoveIn m;
    m.enabled = r.slide;
    m.held = in.slide;
    m.waterLevel = b.waterLevel;
    m.onground = onground;
    m.jumpHeld = in.jump || !(b.flags & FL_JUMPRELEASED);
    if(slide::floorNeeded(s, m.enabled, m.held))
    {
        // (slide::floorNormal's: his box 4 units down against the world and the brush models)
        vec3_t end;
        VectorCopy(b.origin, end);
        end[2] -= 4.f;
        const trace_t tr = b.traceWorld(b.origin, end);
        if(tr.fraction < 1.f && !tr.startsolid && tr.plane.normal[2] > 0.5f)
        {
            m.floorFound = true;
            m.normal = glm::vec3{tr.plane.normal[0], tr.plane.normal[1], tr.plane.normal[2]};
        }
    }
    m.originZ = b.origin[2];
    m.dt = static_cast<float>(in.dt);
    m.now = b.slideClock;
    m.staminaLeft = b.staminaLeft;
    const bool moved = slide::move(s, m, r.slideTuning, wishdir, wishspeed, b.velocity);
    slide::payParts(s, b.slideClock, r.slideTuning, [&](float part) { return b.slidePaid(part); });
    return moved;
}

// SV_AirMove for a walking player (the head's yaw steers; the stick's scale and most speed are the rules').
void airMove(Body& b, const Input& in, const Rules& r, bool onground)
{
    vec3_t steer, forward, right, up;
    VectorCopy(in.steer, steer);
    steer[PITCH] = steer[ROLL] = 0.f;
    AngleVectors(steer, forward, right, up);

    const float scale = r.stickScale * (b.waterLevel >= 1 ? r.shallowStick : 1.f); // (VR_WaterStickScale, not swimming)
    float fmove = in.forward * scale;
    const float smove = in.side * scale;
    if(fmove < 0.f && b.holdLeft > 0.f)
    {
        fmove = 0.f;
    }

    vec3_t wishvel, wishdir;
    for(int i = 0; i < 3; i++)
    {
        wishvel[i] = forward[i] * fmove + right[i] * smove;
    }
    wishvel[2] = 0.f;

    VectorCopy(wishvel, wishdir);
    float wishspeed = VectorNormalize(wishdir);
    if(wishspeed > r.maxspeed)
    {
        VectorScale(wishvel, r.maxspeed / wishspeed, wishvel);
        wishspeed = r.maxspeed;
    }
    b.closeQuarters(wishvel);
    VectorCopy(wishvel, wishdir);
    wishspeed = VectorNormalize(wishdir);

    if(slideMove(b, in, r, onground, wishdir, wishspeed))
    {
        // (a slide: its own friction and slope, no walk)
    }
    else if(onground)
    {
        friction(b, r, in.dt);
        accelerate(b, wishspeed, wishdir, r, in.dt);
    }
    else
    {
        airAccelerate(b, wishspeed, wishvel, r, in.dt);
    }
}

// SV_WaterMove for a swimming player (the head's full angles steer), the stick scaled as VR_WaterStickScale swimming,
// then VR_AfterWaterMove's glide off the bottom and its speed's cap (its strokes, the hands', are the server's).
void swimMove(Body& b, const Input& in, const Rules& r, bool onground)
{
    vec3_t steer, forward, right, up;
    VectorCopy(in.steer, steer);
    AngleVectors(steer, forward, right, up);

    const float scale = r.stickScale * (b.waterLevel >= 3 || !onground ? r.swimStick : r.wadeStick);
    const float fmove = in.forward * scale, smove = in.side * scale, umove = in.up * scale;
    vec3_t wishvel;
    for(int i = 0; i < 3; i++)
    {
        wishvel[i] = forward[i] * fmove + right[i] * smove;
    }
    if(!fmove && !smove && !umove)
    {
        wishvel[2] -= 60.f; // (drift towards the bottom)
    }
    else
    {
        wishvel[2] += umove;
    }
    float wishspeed = VectorLength(wishvel);
    if(wishspeed > r.swimMax)
    {
        VectorScale(wishvel, r.swimMax / wishspeed, wishvel);
        wishspeed = r.swimMax;
    }
    wishspeed *= 0.7f;

    // (water friction)
    float* vel = b.velocity;
    const float speed = VectorLength(vel);
    float newspeed = 0.f;
    if(speed)
    {
        newspeed = static_cast<float>(speed - in.dt * speed * r.friction);
        if(newspeed < 0.f)
        {
            newspeed = 0.f;
        }
        VectorScale(vel, newspeed / speed, vel);
    }

    // (water acceleration)
    if(wishspeed)
    {
        const float addspeed = wishspeed - newspeed;
        if(addspeed > 0.f)
        {
            VectorNormalize(wishvel);
            float accelspeed = static_cast<float>(r.accelerate * wishspeed * in.dt);
            if(accelspeed > addspeed)
            {
                accelspeed = addspeed;
            }
            for(int i = 0; i < 3; i++)
            {
                vel[i] += accelspeed * wishvel[i];
            }
        }
    }

    // VR_AfterWaterMove's glide: part of the friction just taken given back off the bottom; then its cap.
    if(r.glide > 0.f && !onground)
    {
        const float lost = static_cast<float>(in.dt) * r.friction; // (VR_AfterWaterMove's float frame time)
        if(lost > 0.f && lost < 0.9f)
        {
            VectorScale(vel, (1.f - lost * (1.f - r.glide)) / (1.f - lost), vel);
        }
    }
    if(r.swimMost >= 0.f)
    {
        const float len = VectorLength(vel);
        if(len > r.swimMost && len > 0.f)
        {
            VectorScale(vel, r.swimMost / len, vel);
        }
    }
}

// SV_CheckVelocity (a player: bounded an axis at a time).
void checkVelocity(Body& b, const Rules& r)
{
    for(int i = 0; i < 3; i++)
    {
        if(IS_NAN(b.velocity[i]))
        {
            b.velocity[i] = 0.f;
        }
        if(IS_NAN(b.origin[i]))
        {
            b.origin[i] = 0.f;
        }
        if(b.velocity[i] > r.maxvelocity)
        {
            b.velocity[i] = r.maxvelocity;
        }
        else if(b.velocity[i] < -r.maxvelocity)
        {
            b.velocity[i] = -r.maxvelocity;
        }
    }
}

// SV_AddGravity, then vr_slope_walk's VR_GroundGravity: on a walkable slope met lately, only its part into the slope.
void gravity(Body& b, const Rules& r, double dt)
{
    vec3_t before;
    VectorCopy(b.velocity, before);
    b.velocity[2] = static_cast<float>(b.velocity[2] - r.gravity * dt); // (in double, as SV_AddGravity's)
    const float* n = b.groundNormal;
    if(!r.slopeWalk || !(b.flags & FL_ONGROUND) || b.groundAge > 0.1f || n[2] <= 0.7f || n[2] >= 0.9999f)
    {
        return;
    }
    vec3_t added;
    VectorSubtract(b.velocity, before, added);
    const float into = DotProduct(added, n);
    for(int i = 0; i < 3; i++)
    {
        b.velocity[i] = before[i] + n[i] * into;
    }
}

// SV_WallFriction (the aim's way into the wall).
void wallFriction(Body& b, const float* view, const trace_t& trace)
{
    vec3_t forward, right, up, into, side;
    AngleVectors(const_cast<float*>(view), forward, right, up);
    float d = DotProduct(trace.plane.normal, forward);
    d += 0.5f;
    if(d >= 0.f)
    {
        return;
    }
    const float i = DotProduct(trace.plane.normal, b.velocity);
    VectorScale(trace.plane.normal, i, into);
    VectorSubtract(b.velocity, into, side);
    b.velocity[0] = side[0] * (1.f + d);
    b.velocity[1] = side[1] * (1.f + d);
}

// SV_TryUnstick.
int tryUnstick(Body& b, const float* oldvel, MoveNotes* notes)
{
    vec3_t oldorg, dir{0.f, 0.f, 0.f};
    VectorCopy(b.origin, oldorg);
    trace_t steptrace;
    for(int i = 0; i < 8; i++)
    {
        switch(i)
        {
            case 0: dir[0] = 2; dir[1] = 0; break;
            case 1: dir[0] = 0; dir[1] = 2; break;
            case 2: dir[0] = -2; dir[1] = 0; break;
            case 3: dir[0] = 0; dir[1] = -2; break;
            case 4: dir[0] = 2; dir[1] = 2; break;
            case 5: dir[0] = -2; dir[1] = 2; break;
            case 6: dir[0] = 2; dir[1] = -2; break;
            default: dir[0] = -2; dir[1] = -2; break;
        }
        push(b, dir);
        if(b.removed)
        {
            return 7;
        }
        b.velocity[0] = oldvel[0];
        b.velocity[1] = oldvel[1];
        b.velocity[2] = 0.f;
        const int clip = flyMove(b, 0.1f, &steptrace, notes);
        if(fabsf(oldorg[1] - b.origin[1]) > 4.f || fabsf(oldorg[0] - b.origin[0]) > 4.f)
        {
            return clip;
        }
        VectorCopy(oldorg, b.origin);
    }
    VectorCopy(vec3_origin, b.velocity);
    return 7;
}

} // namespace

int clipVelocity(const float* in, const float* normal, float* out, float overbounce)
{
    int blocked = 0;
    if(normal[2] > 0.f)
    {
        blocked |= 1;
    }
    if(!normal[2])
    {
        blocked |= 2;
    }
    const float backoff = DotProduct(in, normal) * overbounce;
    for(int i = 0; i < 3; i++)
    {
        const float change = normal[i] * backoff;
        out[i] = in[i] - change;
        if(out[i] > -stopEpsilon && out[i] < stopEpsilon)
        {
            out[i] = 0.f;
        }
    }
    return blocked;
}

// SV_FlyMove.
int flyMove(Body& b, float time, trace_t* steptrace, MoveNotes* notes)
{
    vec3_t planes[maxClipPlanes];
    vec3_t primal_velocity, original_velocity, new_velocity, dir, end;
    int numplanes = 0, blocked = 0;
    VectorCopy(b.velocity, original_velocity);
    VectorCopy(b.velocity, primal_velocity);
    float time_left = time;

    for(int bumpcount = 0; bumpcount < 4; bumpcount++)
    {
        if(!b.velocity[0] && !b.velocity[1] && !b.velocity[2])
        {
            break;
        }
        for(int i = 0; i < 3; i++)
        {
            end[i] = b.origin[i] + time_left * b.velocity[i];
        }
        const trace_t trace = b.trace(b.origin, end);
        if(trace.allsolid)
        {
            VectorCopy(vec3_origin, b.velocity);
            return 3;
        }
        if(trace.fraction > 0.f)
        {
            VectorCopy(trace.endpos, b.origin);
            VectorCopy(b.velocity, original_velocity);
            numplanes = 0;
        }
        if(trace.fraction == 1.f)
        {
            break;
        }

        if(trace.plane.normal[2] > 0.7f)
        {
            blocked |= 1;
            if(b.metBsp(trace) || b.standsOn(trace))
            {
                b.flags |= FL_ONGROUND;
                if(notes && trace.plane.normal[2] < 0.999f)
                {
                    notes->slope = true;
                }
                VectorCopy(trace.plane.normal, b.groundNormal);
                b.groundAge = 0.f;
                b.floorMet(trace);
            }
        }
        else
        {
            if(notes)
            {
                notes->wall = true;
            }
            b.bumped(trace);
        }
        if(!trace.plane.normal[2])
        {
            blocked |= 2;
            if(steptrace)
            {
                *steptrace = trace;
            }
        }

        b.impact(trace);
        if(b.removed)
        {
            break;
        }

        time_left -= time_left * trace.fraction;

        // (the same plane again, not moved since: nudged out of it, as sv_phys.c)
        int i = 0;
        for(; i < numplanes; i++)
        {
            if(DotProduct(trace.plane.normal, planes[i]) > 0.99f)
            {
                break;
            }
        }
        if(i < numplanes)
        {
            VectorAdd(trace.plane.normal, b.velocity, b.velocity);
            continue;
        }

        if(numplanes >= maxClipPlanes)
        {
            VectorCopy(vec3_origin, b.velocity);
            return 3;
        }
        VectorCopy(trace.plane.normal, planes[numplanes]);
        numplanes++;

        int j = 0;
        for(i = 0; i < numplanes; i++)
        {
            clipVelocity(original_velocity, planes[i], new_velocity, 1.f);
            for(j = 0; j < numplanes; j++)
            {
                if(j != i && DotProduct(new_velocity, planes[j]) < 0.f)
                {
                    break;
                }
            }
            if(j == numplanes)
            {
                break;
            }
        }

        if(i != numplanes)
        {
            VectorCopy(new_velocity, b.velocity);
        }
        else
        {
            if(numplanes != 2)
            {
                VectorCopy(vec3_origin, b.velocity);
                return 7;
            }
            CrossProduct(planes[0], planes[1], dir);
            const float d = DotProduct(dir, b.velocity);
            VectorScale(dir, d, b.velocity);
        }

        if(DotProduct(b.velocity, primal_velocity) <= 0.f)
        {
            VectorCopy(vec3_origin, b.velocity);
            return blocked;
        }
    }
    return blocked;
}

// SV_WalkMove.
void walkMove(Body& b, const Input& in, const Rules& r, double dt, MoveNotes* notes)
{
    const int oldonground = b.flags & FL_ONGROUND;
    b.flags &= ~FL_ONGROUND;

    vec3_t oldorg, oldvel, nosteporg, nostepvel, upmove{0.f, 0.f, 0.f}, downmove{0.f, 0.f, 0.f};
    VectorCopy(b.origin, oldorg);
    VectorCopy(b.velocity, oldvel);

    trace_t steptrace{};
    const float time = static_cast<float>(dt); // (SV_FlyMove's float time)
    int clip = flyMove(b, time, &steptrace, notes);
    if(!(clip & 2) || b.removed)
    {
        return;
    }
    if(!oldonground && b.waterLevel == 0 && !slide::stepsUp(b.slide)) // (a slide on the ground climbs it: VR_SlideStepUp)
    {
        return; // (no stairs up while jumping)
    }
    if(r.nostep || (b.flags & FL_WATERJUMP))
    {
        return;
    }

    VectorCopy(b.origin, nosteporg);
    VectorCopy(b.velocity, nostepvel);

    VectorCopy(oldorg, b.origin);
    upmove[2] = r.stepsize;
    downmove[2] = static_cast<float>(-r.stepsize + oldvel[2] * dt);

    push(b, upmove);
    if(b.removed)
    {
        return;
    }

    b.velocity[0] = oldvel[0];
    b.velocity[1] = oldvel[1];
    b.velocity[2] = 0.f;
    clip = flyMove(b, time, &steptrace, notes);
    if(b.removed)
    {
        return;
    }

    if(clip && fabsf(oldorg[1] - b.origin[1]) < 0.03125f && fabsf(oldorg[0] - b.origin[0]) < 0.03125f)
    {
        clip = tryUnstick(b, oldvel, notes);
        if(b.removed)
        {
            return;
        }
    }

    if(clip & 2)
    {
        wallFriction(b, in.view, steptrace);
    }

    const trace_t downtrace = push(b, downmove);
    if(b.removed)
    {
        return;
    }
    if(downtrace.plane.normal[2] > 0.7f)
    {
        // (a player is never SOLID_BSP: Quake's step down sets no FL_ONGROUND; the next move's slide finds the floor)
        VectorCopy(downtrace.plane.normal, b.groundNormal);
        b.groundAge = 0.f;
        b.steppedOnto(downtrace);
        if(notes && downtrace.plane.normal[2] < 0.999f)
        {
            notes->slope = true;
        }
        if(notes && b.origin[2] != nosteporg[2])
        {
            notes->stepped = true;
        }
    }
    else
    {
        VectorCopy(nosteporg, b.origin);
        VectorCopy(nostepvel, b.velocity);
    }
}

void checkWater(Body& b, const Rules& r)
{
    vec3_t p{b.origin[0], b.origin[1], b.origin[2] + r.feetZ};
    b.waterLevel = 0;
    b.waterType = CONTENTS_EMPTY;
    int cont = b.contents(p);
    if(cont > CONTENTS_WATER)
    {
        return;
    }
    b.waterType = cont;
    b.waterLevel = 1;
    p[2] = b.origin[2] + r.waistZ;
    if(b.contents(p) > CONTENTS_WATER)
    {
        return;
    }
    b.waterLevel = 2;
    p[2] = b.origin[2] + r.eyeZ;
    if(b.contents(p) <= CONTENTS_WATER)
    {
        b.waterLevel = 3;
    }
}

// QC WaterJumpTry along his facing (CheckWaterJump's makevectors(self.angles), level: the move's aim, its pitch a third,
// as SV_ClientThink sets the angles): solid at the waist within 24 units ahead, open at the eyes -> lifted out
// (FL_WATERJUMP, 225 up, pushed over the edge by .movedir until out of the water or 2 seconds on).
// QC WaterJumpTry along the level `dir` (x, y): true, lifted out (`movedir` its push over the edge: the caller tells
// the hook).
bool waterJumpAlong(Body& b, const float* dir, const Rules& r, float* movedir)
{
    vec3_t start{b.origin[0], b.origin[1], b.origin[2] + 8.f}, end;
    end[0] = start[0] + dir[0] * 24.f;
    end[1] = start[1] + dir[1] * 24.f;
    end[2] = start[2];
    const trace_t waist = b.line(start, end);
    if(waist.fraction >= 1.f)
    {
        return false;
    }
    start[2] += r.maxsZ - 8.f;
    end[2] = start[2];
    VectorScale(waist.plane.normal, -50.f, movedir);
    b.jumpDir[0] = movedir[0];
    b.jumpDir[1] = movedir[1];
    if(b.line(start, end).fraction < 1.f)
    {
        return false;
    }
    b.flags = (b.flags | FL_WATERJUMP) & ~FL_JUMPRELEASED;
    b.velocity[2] = 225.f;
    b.holdLeft = 2.f; // (.teleport_time: the safety net)
    return true;
}

void waterJumpTry(Body& b, const Input& in, const Rules& r)
{
    const float pitch = DEG2RAD(-in.view[PITCH] / 3.f), yaw = DEG2RAD(in.view[YAW]);
    const float fwd[2] = {cosf(pitch) * cosf(yaw), cosf(pitch) * sinf(yaw)};
    vec3_t movedir;
    if(waterJumpAlong(b, fwd, r, movedir))
    {
        b.waterJumped(movedir);
    }
}

// One hand's try (QC VR_WaterClimb_HandPull, moved here: MP_PLAN B8j part 3).
bool waterClimbHand(Body& b, const Input& in, const Rules& r, int h)
{
    if(!(r.climbHands & (1 << h)) || !(in.grabs & (1 << h)) || !(in.handVel[h][2] <= -waterClimbPull))
    {
        return false;
    }
    float d[2] = {in.hands[h][0], in.hands[h][1]};
    const float len = sqrtf(d[0] * d[0] + d[1] * d[1]);
    if(!(len >= 8.f))
    {
        return false; // a hand by the body
    }
    const vec3_t p{b.origin[0] + in.hands[h][0], b.origin[1] + in.hands[h][1], b.origin[2] + in.hands[h][2]};
    const vec3_t from{p[0], p[1], p[2] + 4.f}, to{p[0], p[1], p[2] - 16.f};
    const trace_t under = b.line(from, to);
    if(under.fraction >= 1.f || under.startsolid || under.plane.normal[2] < 0.7f)
    {
        return false; // no ground under the hand (a top that could be stood on) to push on
    }
    const float top = under.endpos[2];
    d[0] /= len;
    d[1] /= len;
    vec3_t movedir;
    if(!waterJumpAlong(b, d, r, movedir))
    {
        return false;
    }
    // Lifted high enough to clear the top under the hand (Quake's 225 units/s rise reaches about 31 units: a bank's top
    // higher than the waist is not climbed so), and carried over the edge a little faster than Quake's.
    const float rise = top - (b.origin[2] + r.minsZ) + 6.f;
    if(rise > 0.f && r.gravity > 0.f)
    {
        b.velocity[2] = za::max(225.f, sqrtf(2.f * r.gravity * rise));
    }
    VectorScale(movedir, 1.6f, movedir); // (Quake's: away from the edge's face, 50 units/s)
    b.jumpDir[0] = movedir[0];
    b.jumpDir[1] = movedir[1];
    b.waterJumped(movedir);
    b.waterClimbed(h, rise - 6.f, b.velocity[2]);
    return true;
}

bool waterClimbTry(Body& b, const Input& in, const Rules& r)
{
    return waterClimbHand(b, in, r, 1) || waterClimbHand(b, in, r, 0);
}

float grappleReel(float& len, float full, float age, float dist, float shortest, float dt)
{
    const float speed = full * za::min(1.f, 0.2f + 0.8f * age / grappleRamp);
    float wound = za::min(len, dist) - speed * dt;
    wound = za::max(wound, dist - grappleStall);
    len = za::max(za::min(len, za::max(shortest, wound)), 0.f);
    return za::min(speed, sqrtf(2.f * grappleBrake * za::max(0.f, dist - shortest)));
}

void grappleStep(Body& b, Grapple& g, bool reeling, float dt)
{
    vec3_t toA;
    VectorSubtract(g.at, b.origin, toA);
    const float d = VectorLength(toA);
    float speed = 0.f;
    if(reeling)
    {
        // The reel's one rule (QC's d: the whole rope's way, round the corner).
        g.reelAge = g.reelAge < 0.f ? 0.f : g.reelAge;
        speed = grappleReel(g.len, g.speed, g.reelAge, d + g.beyond, g.shortest, dt);
        g.reelAge += dt;
    }
    else
    {
        g.reelAge = -1.f;
        speed = g.payout > 0.f ? -g.payout : 0.f; // (paying out: he goes away no faster than the rope comes off)
    }

    // VR_Grapple_HoldPlayer, on the rope's length less its path beyond the corner.
    const float hold = g.len - g.beyond;
    if(reeling && d >= 1.f)
    {
        const float closing = DotProduct(b.velocity, toA) / d;
        if(closing > speed + 20.f)
        {
            VectorMA(b.velocity, -(closing - speed) / d, toA, b.velocity);
        }
    }
    if(d <= hold || d < 1.f)
    {
        g.taut = false;
        return;
    }
    vec3_t n;
    VectorScale(toA, 1.f / d, n);
    const float along = DotProduct(b.velocity, n);
    if(!g.taut)
    {
        b.grappleTaut(g.hand, -along + za::min(0.f, speed)); // (paying out: how much faster than the rope runs)
    }
    g.taut = true;

    const float excess = d - hold;
    vec3_t move, end;
    VectorScale(n, excess, move);
    VectorAdd(b.origin, move, end);
    const trace_t tr = b.trace(b.origin, end);
    if(!tr.startsolid && !tr.allsolid)
    {
        VectorCopy(tr.endpos, b.origin);
        if(tr.fraction < 1.f)
        {
            // Against something (a ceiling's beam, a wall): the rest slid along it, as a body on a rope swings round.
            vec3_t rest;
            VectorScale(move, 1.f - tr.fraction, rest);
            const float into = DotProduct(rest, tr.plane.normal);
            VectorMA(rest, -into, tr.plane.normal, rest);
            VectorAdd(b.origin, rest, end);
            const trace_t slid = b.trace(b.origin, end);
            if(!slid.startsolid && !slid.allsolid)
            {
                VectorCopy(slid.endpos, b.origin);
            }
        }
    }
    if(along < speed)
    {
        VectorMA(b.velocity, speed - along, n, b.velocity);
    }
    // Lifted: off the floor (Quake's walking would hold it there, with its friction).
    if((speed > 0.f && n[2] > 0.1f) || excess * n[2] > 0.25f)
    {
        b.flags &= ~FL_ONGROUND;
    }
}

bool crawlStickWant(float yaw, float forwardmove, float sidemove, float top, bool onGround, float* want)
{
    const float rad = yaw * 0.01745329251994329576923690768489f; // (glm::radians')
    const float look[2] = {za::cos(rad), za::sin(rad)};
    const float right[2] = {za::sin(rad), -za::cos(rad)};
    const float wish[2] = {look[0] * forwardmove + right[0] * sidemove, look[1] * forwardmove + right[1] * sidemove};
    const float wishLen = sqrtf(wish[0] * wish[0] + wish[1] * wish[1]);
    if(!onGround || !(wishLen > 1.f) || !(top > 0.f))
    {
        return false;
    }
    const float k = za::min(wishLen, top) / wishLen;
    want[0] += wish[0] * k;
    want[1] += wish[1] * k;
    return true;
}

bool crawlStep(float* crawl, const float* want, bool pulling, float friction, float most, float dt, bool onGround,
    float* velocity)
{
    const float wantLen = sqrtf(want[0] * want[0] + want[1] * want[1]);
    if(!pulling)
    {
        const float keep = za::max(0.f, 1.f - za::max(0.f, friction) * dt);
        crawl[0] *= keep;
        crawl[1] *= keep;
    }
    else if(wantLen > 0.f)
    {
        const float way[2] = {want[0] / wantLen, want[1] / wantLen};
        const float along = crawl[0] * way[0] + crawl[1] * way[1];
        if(along < wantLen)
        {
            const float k = (wantLen - along) * za::min(1.f, dt * crawlGrip);
            crawl[0] += way[0] * k;
            crawl[1] += way[1] * k;
        }
    }
    const float len = sqrtf(crawl[0] * crawl[0] + crawl[1] * crawl[1]);
    if(!ZA_ISFINITE(len))
    {
        crawl[0] = crawl[1] = 0.f;
        return false;
    }
    if(len > most)
    {
        crawl[0] *= most / len;
        crawl[1] *= most / len;
    }
    // On the ground: the player's speed along the crawl kept up to it (the floor's friction took some since).
    const float speed = sqrtf(crawl[0] * crawl[0] + crawl[1] * crawl[1]);
    if(!onGround || speed < 0.01f)
    {
        return true;
    }
    const float way[2] = {crawl[0] / speed, crawl[1] / speed};
    const float along = velocity[0] * way[0] + velocity[1] * way[1];
    if(along < speed)
    {
        velocity[0] += way[0] * (speed - along);
        velocity[1] += way[1] * (speed - along);
    }
    return true;
}

// VR_AfterCrawlMove's stick share (after SV_AirMove, walking): the stick, its share of the crawl's top, feeds the crawl
// as a pull does; between pulls it glides out. The hands' pulls are the server's, once a frame (physics::crawlHands).
void crawlMove(Body& b, const Input& in, const Rules& r, bool onground)
{
    const float scale = r.stickScale * (b.waterLevel >= 1 ? r.shallowStick : 1.f); // (the stick as SV_AirMove got it)
    float want[2] = {0.f, 0.f};
    const bool pulling = crawlStickWant(in.steer[YAW], in.forward * scale, in.side * scale, r.crawlTop, onground, want);
    crawlStep(b.crawl, want, pulling, r.crawlFriction, za::max(0.f, r.crawlMost), static_cast<float>(in.dt), onground, b.velocity);
}

namespace
{

[[nodiscard]] glm::vec3 gv(const float* v)
{
    return glm::vec3{v[0], v[1], v[2]};
}

void sv(float* out, const glm::vec3& v)
{
    out[0] = v.x;
    out[1] = v.y;
    out[2] = v.z;
}

[[nodiscard]] trace_t traceBody(Body& b, const glm::vec3& start, const glm::vec3& end)
{
    vec3_t s{start.x, start.y, start.z}, e{end.x, end.y, end.z};
    return b.trace(s, e);
}

// Slides the body from `origin` towards `target` along what it meets (up to 4 planes: off each, along the crease of
// two; never back against the wanted move) -> where it ends.
[[nodiscard]] glm::vec3 slideBody(Body& b, const glm::vec3& origin, const glm::vec3& target)
{
    const glm::vec3 wanted = target - origin;
    glm::vec3 pos = origin, delta = wanted;
    glm::vec3 planes[4];
    int count = 0;
    for(int bump = 0; bump < 4 && glm::dot(delta, delta) > 1e-8f; bump++)
    {
        const trace_t tr = traceBody(b, pos, pos + delta);
        if(tr.allsolid)
        {
            break;
        }
        pos = gv(tr.endpos);
        if(tr.fraction >= 1.f)
        {
            break;
        }
        const glm::vec3 rest = delta * (1.f - tr.fraction);
        planes[count++] = gv(tr.plane.normal);
        // The rest of the move, off every plane met (along their crease for two); none that turns back.
        delta = glm::vec3{0.f};
        for(int i = 0; i < count; i++)
        {
            glm::vec3 d = rest - planes[i] * za::min(0.f, glm::dot(rest, planes[i]));
            bool clear = true;
            for(int j = 0; j < count; j++)
            {
                clear = clear && (j == i || glm::dot(d, planes[j]) >= -1e-4f);
            }
            if(clear)
            {
                delta = d;
                break;
            }
        }
        if(delta == glm::vec3{0.f} && count >= 2)
        {
            const glm::vec3 crease = glm::cross(planes[count - 2], planes[count - 1]);
            if(const float l = glm::length(crease); l > 1e-4f)
            {
                delta = crease / l * glm::dot(rest, crease / l);
            }
        }
        if(glm::dot(delta, wanted) <= 0.f || count == 4)
        {
            break;
        }
    }
    return pos;
}

} // namespace

void moveBody(Body& b, const float* targetIn)
{
    const glm::vec3 origin = gv(b.origin), target = gv(targetIn);
    trace_t tr = traceBody(b, origin, target);
    if(tr.allsolid)
    {
        return;
    }
    glm::vec3 best = gv(tr.endpos);
    if(tr.fraction < 1.f)
    {
        const glm::vec3 vertical{origin.x, origin.y, target.z};
        const glm::vec3 horizontal{target.x, target.y, origin.z};
        for(const glm::vec3& corner : {vertical, horizontal})
        {
            const trace_t a = traceBody(b, origin, corner);
            if(a.allsolid)
            {
                continue;
            }
            const trace_t c = traceBody(b, gv(a.endpos), target);
            const glm::vec3 end = c.allsolid ? gv(a.endpos) : gv(c.endpos);
            if(glm::distance(end, target) < glm::distance(best, target))
            {
                best = end;
            }
        }
        if(const glm::vec3 slid = slideBody(b, origin, target); glm::distance(slid, target) < glm::distance(best, target) - 1e-4f)
        {
            best = slid;
        }
    }
    sv(b.origin, best);
}

void hangStep(Body& b, Hang& h, const float (*rel)[3], const float* carryIn, float dt, HangSums* sums)
{
    // The pull: each holding hand's motion relative to the body since the last step, the other way, weighted by how
    // far it moved; the holds' own motion carries the body.
    const glm::vec3 carry = carryIn ? gv(carryIn) : glm::vec3{0.f};
    glm::vec3 pull{0.f};
    float weights = 0.f, weight[2]{0.f, 0.f}, top = -1e9f;
    int count = 0;
    for(int k = 0; k < 2; k++)
    {
        const HangGrip& g = h.grips[k];
        if(!g.active)
        {
            continue;
        }
        const glm::vec3 moved = gv(rel[k]) - gv(g.lastRel);
        const float w = glm::length(moved);
        if(ZA_ISFINITE(w))
        {
            pull += moved * w;
            weights += w;
            weight[k] = w;
        }
        top = za::max(top, g.top);
        count++;
    }
    const glm::vec3 pulled = weights > 1e-6f ? -pull / weights : glm::vec3{0.f};
    const glm::vec3 wanted = carry + pulled;

    // What the body owes (earlier pulls it couldn't make) is made up first by a pull the other way; it never moves the
    // body by itself.
    glm::vec3 move3 = pulled;
    glm::vec3 owedNow = gv(h.owed);
    if(const float owed = glm::length(owedNow); owed > 1e-6f)
    {
        const glm::vec3 dir = owedNow / owed;
        if(const float s = glm::dot(move3, dir); s < 0.f)
        {
            const float used = za::min(-s, owed);
            move3 += dir * used;
            owedNow -= dir * used;
        }
    }
    sv(h.owed, owedNow);
    if(h.slipping)
    {
        // No stamina left, slipping: the arms can't pull the body up; it sinks (to straight arms: the server's reach).
        move3.z = za::min(move3.z, 0.f) - hangSlipSink * dt;
    }
    const glm::vec3 meant = carry + move3; // what the body would do unhindered

    // The limits, each stopping the body only from going further past it: the feet no higher than the highest hold
    // (the mantle does that), the body within hangMaxReach (horizontally) of each hold.
    const glm::vec3 origin = gv(b.origin);
    glm::vec3 target = origin + carry + move3;
    const float highest = top - h.minsZ + 2.f;
    if(target.z > highest)
    {
        target.z = za::max(highest, za::min(target.z, origin.z));
    }
    for(int pass = 0; pass < 2; pass++)
    {
        for(const HangGrip& g : h.grips)
        {
            if(!g.active)
            {
                continue;
            }
            const glm::vec3 hold = gv(g.hold);
            glm::vec2 fromHold{target.x - hold.x, target.y - hold.y};
            const float reach = za::max(hangMaxReach, glm::length(glm::vec2{origin.x - hold.x, origin.y - hold.y}));
            if(glm::length(fromHold) > reach)
            {
                fromHold *= reach / glm::length(fromHold);
                target.x = hold.x + fromHold.x;
                target.y = hold.y + fromHold.y;
            }
        }
    }
    glm::vec3 step = target - origin;
    const float maxStep = hangMaxSpeed * dt;
    if(glm::length(step) > maxStep)
    {
        step *= maxStep / glm::length(step);
    }
    vec3_t to;
    sv(to, origin + step);
    moveBody(b, to);
    VectorCopy(vec3_origin, b.velocity);
    b.flags &= ~FL_ONGROUND;
    for(int k = 0; k < 2; k++)
    {
        VectorCopy(rel[k], h.grips[k].lastRel);
    }

    if(sums)
    {
        if(sums->steps == 0)
        {
            sv(sums->origin, origin);
        }
        sums->steps++;
        sv(sums->meant, gv(sums->meant) + meant);
        sv(sums->wanted, gv(sums->wanted) + wanted);
        sums->weights += weights;
        sums->weight[0] += weight[0];
        sums->weight[1] += weight[1];
        sums->top = za::max(sums->top, top);
        sums->count = count;
    }
}

namespace
{

[[nodiscard]] trace_t lineBody(Body& b, const glm::vec3& start, const glm::vec3& end)
{
    vec3_t s{start.x, start.y, start.z}, e{end.x, end.y, end.z};
    return b.line(s, e);
}

[[nodiscard]] trace_t worldBody(Body& b, const glm::vec3& start, const glm::vec3& end)
{
    vec3_t s{start.x, start.y, start.z}, e{end.x, end.y, end.z};
    return b.traceWorld(s, e);
}

// The top under a point minFooting in from the box's sides, standing at `at` (not just its very edge: a box touching the
// far side of a narrow wall's top stood on nothing; the box traces are the hull's whole size).
[[nodiscard]] bool footing(Body& b, const Hang& h, const glm::vec3& at)
{
    for(int i = -1; i <= 1; i++)
    {
        for(int j = -1; j <= 1; j++)
        {
            const glm::vec3 p = at + glm::vec3{static_cast<float>(i) * (h.maxsXY[0] - minFooting),
                                         static_cast<float>(j) * (h.maxsXY[1] - minFooting), h.minsZ + 0.5f};
            const trace_t down = lineBody(b, p, p - glm::vec3{0.f, 0.f, 8.5f});
            if(!down.startsolid && down.fraction < 1.f)
            {
                return true;
            }
        }
    }
    return false;
}

// The mantle onto a level top (vr_climb.cpp's notes before findMantle): spots 20, 28 and 36 units in from the lip, at
// mantleSides along it, each reached straight up (or from a little further out: a body pressed against the face, under a
// trim on its lip) and over; the ledge map's depth (`g.depth`) tells a spot over the top (it stands there: the sweep over
// finds anything in the way); a narrow top's middle last.
[[nodiscard]] bool findMantleLevel(Body& b, const Hang& h, const HangGrip& g, glm::vec3& mid, glm::vec3& to)
{
    const glm::vec3 origin = gv(b.origin);
    const float z = g.top - h.minsZ + 1.f;
    if(z < origin.z)
    {
        return false;
    }
    const glm::vec3 out{g.out[0], g.out[1], 0.f};
    const glm::vec3 along{-out.y, out.x, 0.f};
    const glm::vec3 lip = gv(g.hold) + out * ledges::holdInset;
    constexpr int sideCount = static_cast<int>(sizeof(mantleSides) / sizeof(mantleSides[0]));
    int upState[sideCount] = {}; // 0: not swept yet, 1: clear (to upMid), -1: blocked
    glm::vec3 upMid[sideCount];
    const auto up = [&](int s) {
        if(upState[s] == 0)
        {
            upState[s] = -1;
            for(const float back : {0.f, 2.f, 4.f})
            {
                const glm::vec3 m = glm::vec3{origin.x, origin.y, z} + along * mantleSides[s] + out * back;
                if(traceBody(b, origin, m).fraction >= 1.f)
                {
                    upState[s] = 1;
                    upMid[s] = m;
                    break;
                }
            }
        }
        return upState[s] > 0;
    };
    const auto fits = [&](int s, float in) {
        if(!up(s))
        {
            return false;
        }
        mid = upMid[s];
        to = glm::vec3{lip.x, lip.y, z} - out * in + along * mantleSides[s];
        if(const float depth = g.depth[s]; !(depth >= 0.f && in <= depth - 0.5f))
        {
            if(traceBody(b, to, to).startsolid)
            {
                return false;
            }
            if(worldBody(b, to, to - glm::vec3{0.f, 0.f, 8.f}).fraction >= 1.f)
            {
                return false; // nothing to stand on
            }
            if(!footing(b, h, to))
            {
                return false;
            }
        }
        return traceBody(b, mid, to).fraction >= 1.f;
    };
    for(int s = 0; s < sideCount; s++)
    {
        for(const float k : {20.f, 28.f, 36.f})
        {
            if(fits(s, k + ledges::holdInset))
            {
                return true;
            }
        }
    }
    bool anyUp = false;
    for(int s = 0; s < sideCount; s++)
    {
        anyUp = anyUp || upState[s] > 0;
    }
    // A narrow top (at least minFooting deep): from over its middle outwards, the box's middle still over it (not
    // standing on its edge over the drop: a rung against a wall is no place to stand).
    const float depth = za::max(0.f, g.depth[0]);
    if(!anyUp || depth < minFooting)
    {
        return false;
    }
    for(int s = 0; s < sideCount; s++)
    {
        if(upState[s] < 0)
        {
            continue;
        }
        for(float in = za::min(0.5f * depth, 22.f); in >= 0.5f; in -= 2.f)
        {
            if(fits(s, in))
            {
                return true;
            }
        }
    }
    return false;
}

// The mantle onto a sloping top (vr_climb.cpp's notes before findMantle, vr_climb_mantle_slope): for each spot the box
// let down onto the top from as high as the slope could rise under it to as low as it could fall; it must land on
// something no steeper, on more than its edge, with room; the way straight up to the higher of the lip's height and the
// spot's (or from a little further out), then over.
[[nodiscard]] bool findMantleSloped(Body& b, const Hang& h, const HangGrip& g, glm::vec3& mid, glm::vec3& to)
{
    const float slope = h.mantleSlope;
    if(slope <= 0.f)
    {
        return false;
    }
    const glm::vec3 origin = gv(b.origin);
    const float z = g.top - h.minsZ + 1.f;
    if(z < origin.z)
    {
        return false;
    }
    const float minNormalZ = za::cos(glm::radians(slope)) - 1e-3f;
    const float tanSlope = za::tan(glm::radians(slope));
    const glm::vec3 out{g.out[0], g.out[1], 0.f};
    const glm::vec3 along{-out.y, out.x, 0.f};
    const glm::vec3 lip = gv(g.hold) + out * ledges::holdInset;
    for(const float side : mantleSides)
    {
        for(const float k : {20.f, 28.f, 36.f})
        {
            const float in = k + ledges::holdInset;
            const glm::vec3 spot = glm::vec3{lip.x, lip.y, z} - out * in + along * side;
            const float rise = za::min(48.f, (in + h.maxsXY[0]) * tanSlope) + 1.f;
            const glm::vec3 high = spot + glm::vec3{0.f, 0.f, rise};
            if(traceBody(b, high, high).startsolid)
            {
                continue; // in a wall or under a ceiling even there
            }
            const trace_t down = traceBody(b, high, spot - glm::vec3{0.f, 0.f, rise});
            if(down.startsolid || down.allsolid || down.fraction >= 1.f || down.plane.normal[2] < minNormalZ)
            {
                continue; // nothing to stand on, or too steep (a wall's edge, a ramp)
            }
            const glm::vec3 land = gv(down.endpos);
            if(!footing(b, h, land))
            {
                continue;
            }
            const float overZ = za::max(z, land.z);
            to = glm::vec3{land.x, land.y, overZ};
            for(const float back : {0.f, 2.f, 4.f})
            {
                const glm::vec3 m = glm::vec3{origin.x, origin.y, overZ} + along * side + out * back;
                if(traceBody(b, origin, m).fraction >= 1.f && traceBody(b, m, to).fraction >= 1.f)
                {
                    mid = m;
                    if(h.verbose)
                    {
                        Con_Printf("climb: sloped mantle spot %.0f in, %+.0f along: the top %+.1f from the lip's, its slope "
                                   "%.0f deg\n", in, side, land.z - z,
                            glm::degrees(za::acos(za::clamp(down.plane.normal[2], -1.f, 1.f))));
                    }
                    return true;
                }
            }
        }
    }
    return false;
}

} // namespace

bool findMantle(Body& b, const Hang& h, int k, float* mid, float* to)
{
    const HangGrip& g = h.grips[k];
    glm::vec3 m{0.f}, t{0.f};
    if(!findMantleLevel(b, h, g, m, t) && !findMantleSloped(b, h, g, m, t))
    {
        return false;
    }
    sv(mid, m);
    sv(to, t);
    return true;
}

bool hangMantle(Body& b, Hang& h, const float (*rel)[3], float headZ, float dt, Mantle& m)
{
    h.mantleHand = -1;
    bool noRoom = false;
    for(int k = 0; k < 2; k++)
    {
        HangGrip& g = h.grips[k];
        g.missAge += g.missed ? dt : 0.f;
    }
    if(h.mantleTime <= 0.f || h.slipping)
    {
        h.noRoom = false;
        return false;
    }
    const glm::vec3 now = gv(b.origin);
    for(int k = 0; k < 2; k++)
    {
        HangGrip& g = h.grips[k];
        if(!g.active)
        {
            continue;
        }
        // (The head's height over the top: under it, he can lean up and down over the ledge without mantling.)
        const float pulledDown = g.relAtGrabZ - rel[k][2];
        const float headAbove = headZ + now.z - g.top;
        if(pulledDown < mantlePull || headAbove < h.mantleHeight)
        {
            continue;
        }
        // No room found here lately, the body and the hold where they were: none yet.
        if(g.missed && g.missAge < mantleRetry && glm::distance(now, gv(g.missOrigin)) <= mantleRetryMove &&
            VectorCompare(g.hold, g.missHold) && g.top == g.missTop)
        {
            noRoom = true;
            continue;
        }
        vec3_t mid, to;
        g.missed = !findMantle(b, h, k, mid, to);
        if(g.missed)
        {
            g.missAge = 0.f;
            sv(g.missOrigin, now);
            VectorCopy(g.hold, g.missHold);
            g.missTop = g.top;
            noRoom = true;
            continue;
        }
        m = Mantle{};
        m.on = true;
        VectorCopy(b.origin, m.from);
        VectorCopy(mid, m.mid);
        VectorCopy(to, m.to);
        m.time = h.mantleTime;
        m.headAbove = headAbove;
        h.mantleHand = k;
        h.noRoom = false;
        return true;
    }
    h.noRoom = noRoom;
    return false;
}

bool mantleStep(Mantle& m, float dt, float* out)
{
    m.at += dt;
    vec3_t d0, d1;
    VectorSubtract(m.mid, m.from, d0);
    VectorSubtract(m.to, m.mid, d1);
    const float up = VectorLength(d0);
    const float over = VectorLength(d1);
    const float t = m.time > 0.f ? CLAMP(0.f, m.at / m.time, 1.f) : 1.f;
    const float along = t * (up + over);
    const bool first = along <= up && up > 0.f;
    const float* a = first ? m.from : m.mid;
    const float* z = first ? m.mid : m.to;
    const float f = first ? along / up : over > 0.f ? (along - up) / over : 1.f;
    for(int i = 0; i < 3; i++)
    {
        out[i] = a[i] + (z[i] - a[i]) * f; // (glm::mix's)
    }
    return t >= 1.f;
}

namespace
{
void playerMoveOne(Body& b, const Input& in, const Rules& r, MoveNotes* notes)
{
    // (the VR pieces' and QuakeC's frame time: a float, as theirs; sv_user.c's and sv_phys.c's: in.dt, a double)
    const float fdt = static_cast<float>(in.dt);
    b.mantledNow = false;
    if(b.hang.on)
    {
        // A grip let go lets go of its hold (the server's climb at the frame of that move: VR_ClimbPreThink); none
        // left, he falls: this move walks.
        bool any = false;
        for(int k = 0; k < 2; k++)
        {
            b.hang.grips[k].active = b.hang.grips[k].active && (in.grabs & (1 << k)) != 0;
            any = any || b.hang.grips[k].active;
        }
        b.hang.on = any;
    }
    if(b.hang.on)
    {
        // Hanging (vr_climb.cpp's holds): the move's hands pull the body (B8i), then the mantle's test (B8j: begun, the
        // next moves carry him along it); the reach, the owed's settling and the stamina are the server's, once a
        // frame: climb::hangMoved.
        if(notes)
        {
            notes->air = true;
        }
        hangStep(b, b.hang, in.hands, nullptr, fdt, b.hangSums);
        if(hangMantle(b, b.hang, in.hands, in.headZ, fdt, b.mantle))
        {
            b.hang.on = false;
        }
        return;
    }
    if(b.mantle.on)
    {
        // Mantling (vr_climb.cpp's way up and over): carried along it at the move's time, nothing else; at its end he
        // stands (the server's climb then lets go: climb::mantleMoved).
        if(notes)
        {
            notes->air = true;
        }
        const bool done = mantleStep(b.mantle, fdt, b.origin);
        VectorCopy(vec3_origin, b.velocity);
        b.flags &= ~FL_ONGROUND;
        if(done)
        {
            b.mantle.on = false;
            b.mantledNow = true;
            b.flags |= FL_ONGROUND;
        }
        return;
    }
    const bool onground = (b.flags & FL_ONGROUND) != 0;
    if(notes && !onground)
    {
        notes->air = true;
    }
    // SV_ClientThink: the water jump, the swim or the walk, by the level the last move found (a server frame's
    // SV_CheckWater runs after SV_ClientThink and PlayerPreThink: they go by the last frame's; B11b part 2). Its
    // drag, the water jump out and the jump's swim below too; this move's own level, found after them, its gravity and
    // walk (b.waterLevel and b.waterType at a move's start are part of the state: the record carries them).
    b.thinkLevel = b.waterLevel;
    if(b.flags & FL_WATERJUMP)
    {
        // SV_WaterJump: carried over the edge until out of the water or its time is up (gravity off meanwhile).
        if(b.holdLeft < 0.f || !b.waterLevel)
        {
            b.flags &= ~FL_WATERJUMP;
            b.holdLeft = 0.f;
        }
        b.velocity[0] = b.jumpDir[0];
        b.velocity[1] = b.jumpDir[1];
    }
    else if(b.waterLevel >= 2)
    {
        swimMove(b, in, r, onground);
    }
    else
    {
        airMove(b, in, r, onground);
        if(r.crawl)
        {
            crawlMove(b, in, r, onground);
        }
    }
    if(!r.crawl)
    {
        b.crawl[0] = b.crawl[1] = 0.f; // (VR_AfterCrawlMove's reset: not crawling now)
    }
    if(notes && (b.waterLevel >= 2 || (b.flags & FL_WATERJUMP)))
    {
        notes->swim = true;
    }
    // QC WaterMove's drag (PlayerPreThink, after SV_ClientThink's move), in water and not jumping out of it.
    if(r.waterDrag && b.waterLevel && !(b.flags & FL_WATERJUMP))
    {
        const float k = 0.8f * static_cast<float>(b.waterLevel) * fdt;
        for(int i = 0; i < 3; i++)
        {
            b.velocity[i] -= k * b.velocity[i];
        }
    }
    // QC CheckWaterJump (PlayerPreThink, after WaterMove; its .vr_movewaterjump leaves it to the moves), waist deep.
    if(r.waterJumpOut && b.waterLevel == 2 && !(b.flags & FL_WATERJUMP))
    {
        waterJumpTry(b, in, r);
        if(notes && (b.flags & FL_WATERJUMP))
        {
            notes->swim = true;
        }
    }
    // ... or the hands' climb out (its .vr_movewaterclimb: vr_water_jump 0, climbing on; B8j part 3), waist deep (in a
    // water jump too, as QC's CheckWaterJump: a hand still pulling down lifts him again).
    if(r.waterClimb && b.waterLevel == 2 && waterClimbTry(b, in, r) && notes)
    {
        notes->swim = true;
    }
    if(r.jumpSpeed > 0.f)
    {
        // QC PlayerPreThink's jump (PlayerJump; after SV_ClientThink's air move, before the physics, as a server frame
        // runs them), in the move whose key pressed it. Swimming, the key rises (PlayerJump's water branch); a water
        // jump out (QC's CheckWaterJump, once a server frame) is the state's. The rules' cases left to QuakeC (lying
        // down, AD's boots) are jumpSpeed 0.
        if(!in.jump)
        {
            b.flags |= FL_JUMPRELEASED;
        }
        else if(b.flags & FL_WATERJUMP)
        {
        }
        else if(b.waterLevel >= 2)
        {
            b.velocity[2] = b.waterType == CONTENTS_WATER ? 100.f : b.waterType == CONTENTS_SLIME ? 80.f : 50.f;
        }
        else if((b.flags & FL_ONGROUND) && (b.flags & FL_JUMPRELEASED))
        {
            b.flags &= ~(FL_JUMPRELEASED | FL_ONGROUND); // (don't pogo stick; don't stairwalk)
            b.velocity[2] += r.jumpSpeed;
            b.jumped();
            if(notes)
            {
                notes->air = true;
            }
        }
    }
    // QC VR_Grapple_PlayerFrame's rope (PlayerPreThink, after the jump; its .vr_movegrapple leaves the hold and the reel
    // to the moves: B8j part 4), on the move's B/Y.
    if(b.grapple.on)
    {
        grappleStep(b, b.grapple, (in.reels & b.grapple.drivers) != 0, fdt);
        if(notes && !(b.flags & FL_ONGROUND))
        {
            notes->air = true;
        }
    }
    checkVelocity(b, r);
    checkWater(b, r); // (SV_Physics_Client's, before gravity: this move's level, the next move's walk or swim's)
    if(b.waterLevel < 2 && !(b.flags & FL_WATERJUMP))
    {
        gravity(b, r, in.dt);
    }
    walkMove(b, in, r, in.dt, notes);
    if(b.removed)
    {
        return;
    }

    // The room-scale walk (VR_ClientRoomscaleMove): a second, horizontal walk that never leaves the ground.
    const float mx = in.roomscale[0] * r.roomscaleScale, my = in.roomscale[1] * r.roomscaleScale;
    if(r.roomscaleOn && (mx != 0.f || my != 0.f) && !IS_NAN(mx) && !IS_NAN(my))
    {
        vec3_t oldvel;
        VectorCopy(b.velocity, oldvel);
        b.velocity[0] = mx;
        b.velocity[1] = my;
        b.velocity[2] = 0.f;
        const int og = b.flags & FL_ONGROUND;
        walkMove(b, in, r, in.dt, notes);
        b.flags |= og;
        VectorCopy(oldvel, b.velocity);
    }
    b.groundAge += fdt;
    b.holdLeft -= fdt;
}
} // namespace

void playerMove(Body& b, const Input& in, const Rules& r, MoveNotes* notes)
{
    playerMoveOne(b, in, r, notes);
    b.slideClock += in.dt; // (the slide's clock: every move, hanging and mantling too)
}

} // namespace qvr::pmove
