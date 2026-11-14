// vr_pmove.cpp -- a player's walk, one move at a time (vr_pmove.hpp; MP_PLAN.md, B8). Each piece follows its sv_user.c /
// sv_phys.c original line by line, with the move's dt for host_frametime and the Body's hooks for the edicts.

#include "vr_pmove.hpp"

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
void friction(Body& b, const Rules& r, float dt)
{
    float* vel = b.velocity;
    const float speed = sqrtf(vel[0] * vel[0] + vel[1] * vel[1]);
    if(!speed)
    {
        return;
    }
    const float fr = b.overDropoff(vel, speed) ? r.friction * r.edgefriction : r.friction;
    const float control = speed < r.stopspeed ? r.stopspeed : speed;
    float newspeed = speed - dt * control * fr;
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
void accelerate(Body& b, float wishspeed, const float* wishdir, const Rules& r, float dt)
{
    const float currentspeed = DotProduct(b.velocity, wishdir);
    const float addspeed = wishspeed - currentspeed;
    if(addspeed <= 0.f)
    {
        return;
    }
    float accelspeed = r.accelerate * dt * wishspeed;
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
void airAccelerate(Body& b, float wishspeed, float* wishveloc, const Rules& r, float dt)
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
    float accelspeed = r.accelerate * wishspeed * dt;
    if(accelspeed > addspeed)
    {
        accelspeed = addspeed;
    }
    for(int i = 0; i < 3; i++)
    {
        b.velocity[i] += accelspeed * wishveloc[i];
    }
}

// SV_AirMove for a walking player (the head's yaw steers; the stick's scale and most speed are the rules').
void airMove(Body& b, const Input& in, const Rules& r, bool onground)
{
    vec3_t steer, forward, right, up;
    VectorCopy(in.steer, steer);
    steer[PITCH] = steer[ROLL] = 0.f;
    AngleVectors(steer, forward, right, up);

    float fmove = in.forward * r.stickScale;
    const float smove = in.side * r.stickScale;
    if(fmove < 0.f && b.backHeld())
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

    if(onground)
    {
        friction(b, r, in.dt);
        accelerate(b, wishspeed, wishdir, r, in.dt);
    }
    else
    {
        airAccelerate(b, wishspeed, wishvel, r, in.dt);
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
void gravity(Body& b, const Rules& r, float dt)
{
    vec3_t before;
    VectorCopy(b.velocity, before);
    b.velocity[2] -= r.gravity * dt;
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
void walkMove(Body& b, const Input& in, const Rules& r, float dt, MoveNotes* notes)
{
    const int oldonground = b.flags & FL_ONGROUND;
    b.flags &= ~FL_ONGROUND;

    vec3_t oldorg, oldvel, nosteporg, nostepvel, upmove{0.f, 0.f, 0.f}, downmove{0.f, 0.f, 0.f};
    VectorCopy(b.origin, oldorg);
    VectorCopy(b.velocity, oldvel);

    trace_t steptrace{};
    int clip = flyMove(b, dt, &steptrace, notes);
    if(!(clip & 2) || b.removed)
    {
        return;
    }
    if(!oldonground && !r.wading)
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
    downmove[2] = -r.stepsize + oldvel[2] * dt;

    push(b, upmove);
    if(b.removed)
    {
        return;
    }

    b.velocity[0] = oldvel[0];
    b.velocity[1] = oldvel[1];
    b.velocity[2] = 0.f;
    clip = flyMove(b, dt, &steptrace, notes);
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

void playerMove(Body& b, const Input& in, const Rules& r, MoveNotes* notes)
{
    const bool onground = (b.flags & FL_ONGROUND) != 0;
    if(notes && !onground)
    {
        notes->air = true;
    }
    airMove(b, in, r, onground);
    checkVelocity(b, r);
    if(r.gravityOn)
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
    b.groundAge += in.dt;
}

} // namespace qvr::pmove
