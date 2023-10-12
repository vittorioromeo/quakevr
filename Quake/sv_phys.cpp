/*
Copyright (C) 1996-2001 Id Software, Inc.
Copyright (C) 2002-2009 John Fitzgibbons and others
Copyright (C) 2010-2014 QuakeSpasm developers
Copyright (C) 2020-2021 Vittorio Romeo

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// sv_phys.c

#include "console.hpp"
#include <glm/fwd.hpp>
#include "quakedef.hpp"
#include "server.hpp"
#include "vr.hpp"
#include "vr_cvars.hpp"
#include "world.hpp"
#include "util.hpp"
#include "quakeglm.hpp"
#include "sys.hpp"
#include "qcvm.hpp"

#include <algorithm>
#include <tuple>

/*


pushmove objects do not obey gravity, and do not interact with each other or
trigger fields, but block normal movement and push normal objects when they
move.

onground is set for toss objects when they come to a complete rest.  it is set
for steping or walking objects

doors, plats, etc are SOLID_BSP, and MOVETYPE_PUSH
bonus items are SOLID_TRIGGER touch, and MOVETYPE_TOSS
corpses are SOLID_NOT and MOVETYPE_TOSS
crates are SOLID_BBOX and MOVETYPE_TOSS
walking monsters are SOLID_SLIDEBOX and MOVETYPE_STEP
flying/floating monsters are SOLID_SLIDEBOX and MOVETYPE_FLY

solid_edge items only clip against bsp models.

*/

cvar_t sv_friction = {"sv_friction", "4", CVAR_NOTIFY | CVAR_SERVERINFO};
cvar_t sv_stopspeed = {"sv_stopspeed", "100", CVAR_NONE};
cvar_t sv_gravity = {"sv_gravity", "800", CVAR_NOTIFY | CVAR_SERVERINFO};
cvar_t sv_maxvelocity = {"sv_maxvelocity", "2000", CVAR_NONE};
cvar_t sv_nostep = {"sv_nostep", "0", CVAR_NONE};
cvar_t sv_freezenonclients = {"sv_freezenonclients", "0", CVAR_NONE};
cvar_t sv_gameplayfix_spawnbeforethinks = {
    "sv_gameplayfix_spawnbeforethinks", "0", CVAR_NONE};

cvar_t sv_sound_watersplash = {
    "sv_sound_watersplash", "misc/h2ohit1.wav", CVAR_NONE};
cvar_t sv_sound_land = {"sv_sound_land", "demon/dland2.wav", CVAR_NONE};


#define MOVE_EPSILON 0.01

void SV_Physics_Toss(edict_t* ent);

/*
================
SV_CheckAllEnts
================
*/
void SV_CheckAllEnts()
{
    // see if any solid entities are inside the final position
    edict_t* check = NEXT_EDICT(qcvm->edicts);
    for(int e = 1; e < qcvm->num_edicts; e++, check = NEXT_EDICT(check))
    {
        if(check->free)
        {
            continue;
        }

        if(check->v.movetype == MOVETYPE_PUSH ||
            check->v.movetype == MOVETYPE_NONE ||
            check->v.movetype == MOVETYPE_NOCLIP)
        {
            continue;
        }

        if(SV_TestEntityPosition(check))
        {
            Con_Printf("entity in invalid position\n");
        }
    }
}

/*
================
SV_CheckVelocity
================
*/
void SV_CheckVelocity(edict_t* ent)
{
    //
    // bound velocity
    //
    for(int i = 0; i < 3; i++)
    {
        if(IS_NAN(ent->v.velocity[i]))
        {
            Con_Printf(
                "Got a NaN velocity on %s\n", PR_GetString(ent->v.classname));
            ent->v.velocity[i] = 0;
        }

        if(IS_NAN(ent->v.origin[i]))
        {
            Con_Printf(
                "Got a NaN origin on %s\n", PR_GetString(ent->v.classname));
            ent->v.origin[i] = 0;
        }

        ent->v.velocity[i] = std::clamp(ent->v.velocity[i],
            float(-sv_maxvelocity.value), float(sv_maxvelocity.value));
    }
}

/*
=============
SV_RunThink

Runs thinking code if time.  There is some play in the exact time the think
function will be called, because it is called before any movement is done
in a frame.  Not used for pushmove objects, because they must be exact.
Returns false if the entity removed itself.
=============
*/
template <auto TNextThink, auto TThink, bool TDoLerp>
bool SV_RunThinkImpl(edict_t* ent)
{
    if(!((ent->v).*TThink))
    {
        return !ent->free;
    }

    float thinktime = (ent->v).*TNextThink;
    if(thinktime <= 0 || thinktime > qcvm->time + host_frametime)
    {
        return true;
    }

    if(thinktime < qcvm->time)
    {
        thinktime = qcvm->time; // don't let things stay in the past.
    }
    // it is possible to start that way
    // by a trigger with a local time.

    float oldframe = ent->v.frame; // johnfitz

    (ent->v).*TNextThink = 0;
    pr_global_struct->time = thinktime;
    pr_global_struct->self = EDICT_TO_PROG(ent);
    pr_global_struct->other = EDICT_TO_PROG(qcvm->edicts);
    PR_ExecuteProgram((ent->v).*TThink);

    if(TDoLerp)
    {
        // johnfitz -- PROTOCOL_QUAKEVR
        // capture interval to nextthink here and send it to client for better
        // lerp timing, but only if interval is not 0.1 (which client assumes)
        ent->sendinterval = false;
        if(!ent->free && (ent->v).*TNextThink &&
            (ent->v.movetype == MOVETYPE_STEP || ent->v.frame != oldframe))
        {
            int i = Q_rint(((ent->v).*TNextThink - thinktime) * 255);
            if(i >= 0 && i < 256 && i != 25 && i != 26)
            {
                // 25 and 26 are close enough to 0.1 to not send
                ent->sendinterval = true;
            }
        }
        // johnfitz
    }

    return !ent->free;
}

bool SV_RunThink(edict_t* ent)
{
    return //
        SV_RunThinkImpl<&entvars_t::nextthink, &entvars_t::think, true>(ent) &&
        SV_RunThinkImpl<&entvars_t::nextthink2, &entvars_t::think2, false>(ent);
}

/*
==================
SV_Impact

Two entities have touched, so run their touch functions
==================
*/
void SV_Impact(edict_t* e1, edict_t* e2, func_t entvars_t::*impactFunc)
{
    const int old_self = pr_global_struct->self;
    const int old_other = pr_global_struct->other;

    pr_global_struct->time = qcvm->time;
    if(e1->v.*impactFunc && e1->v.solid != SOLID_NOT)
    {
        pr_global_struct->self = EDICT_TO_PROG(e1);
        pr_global_struct->other = EDICT_TO_PROG(e2);
        PR_ExecuteProgram(e1->v.*impactFunc);
    }

    if(e2->v.*impactFunc && e2->v.solid != SOLID_NOT)
    {
        pr_global_struct->self = EDICT_TO_PROG(e2);
        pr_global_struct->other = EDICT_TO_PROG(e1);
        PR_ExecuteProgram(e2->v.*impactFunc);
    }

    pr_global_struct->self = old_self;
    pr_global_struct->other = old_other;
}


/*
==================
ClipVelocity

Slide off of the impacting object
returns the blocked flags (1 = floor, 2 = step / wall)
==================
*/
#define STOP_EPSILON 0.1

int ClipVelocity(
    const qvec3& in, const qvec3& normal, qvec3& out, float overbounce)
{
    int blocked = 0;

    if(normal[2] > 0)
    {
        blocked |= 1; // floor
    }

    if(!normal[2])
    {
        blocked |= 2; // step
    }

    const float backoff = DotProduct(in, normal) * overbounce;

    for(int i = 0; i < 3; i++)
    {
        const float change = normal[i] * backoff;
        out[i] = in[i] - change;

        if(out[i] > -STOP_EPSILON && out[i] < STOP_EPSILON)
        {
            out[i] = 0;
        }
    }

    return blocked;
}

/*
============
SV_FlyMove

The basic solid body movement clip that slides along multiple planes
Returns the clipflags if the velocity was modified (hit something solid)
1 = floor
2 = wall / step
4 = dead stop
If steptrace is not nullptr, the trace of any vertical wall hit will be stored
============
*/
#define MAX_CLIP_PLANES 5
int SV_FlyMove(edict_t* ent, float time, trace_t* steptrace)
{
    constexpr int numbumps = 4;

    const auto primal_velocity = ent->v.velocity;

    qvec3 planes[MAX_CLIP_PLANES];
    qvec3 original_velocity = ent->v.velocity;
    qvec3 new_velocity;

    float time_left = time;

    int blocked = 0;
    int numplanes = 0;

    for(int bumpcount = 0; bumpcount < numbumps; bumpcount++)
    {
        if(!ent->v.velocity[0] && !ent->v.velocity[1] && !ent->v.velocity[2])
        {
            break;
        }

        const auto end = ent->v.origin + time_left * ent->v.velocity;

        trace_t trace =
            SV_Move(ent->v.origin, ent->v.mins, ent->v.maxs, end, false, ent);

        if(trace.allsolid)
        {
            // entity is trapped in another solid
            ent->v.velocity = vec3_zero;
            return 3;
        }

        if(trace.fraction > 0)
        {
            // actually covered some distance
            ent->v.origin = trace.endpos;
            original_velocity = ent->v.velocity;
            numplanes = 0;
        }

        if(trace.fraction == 1)
        {
            break; // moved the entire distance
        }

        if(!trace.ent)
        {
            Sys_Error("SV_FlyMove: !trace.ent");
        }

        if(quake::util::traceHitGround(trace))
        {
            blocked |= 1; // floor
            if(trace.ent->v.solid == SOLID_BSP)
            {
                quake::util::addFlag(ent, FL_ONGROUND);
                ent->v.groundentity = EDICT_TO_PROG(trace.ent);
            }
        }

        if(!trace.plane.normal[2])
        {
            blocked |= 2; // step
            if(steptrace)
            {
                *steptrace = trace; // save for player extrafriction
            }
        }

        //
        // run the impact function
        //
        SV_Impact(ent, trace.ent, &entvars_t::touch);

        if(ent->free)
        {
            break; // removed by the impact function
        }

        time_left -= time_left * trace.fraction;

        // cliped to another plane
        if(numplanes >= MAX_CLIP_PLANES)
        {
            // this shouldn't really happen
            ent->v.velocity = vec3_zero;
            return 3;
        }

        planes[numplanes] = trace.plane.normal;
        numplanes++;

        //
        // modify original_velocity so it parallels all of the clip planes
        //
        int i, j;
        for(i = 0; i < numplanes; i++)
        {
            ClipVelocity(original_velocity, planes[i], new_velocity, 1);
            for(j = 0; j < numplanes; j++)
            {
                if(j != i)
                {
                    if(DotProduct(new_velocity, planes[j]) < 0)
                    {
                        break; // not ok
                    }
                }
            }
            if(j == numplanes)
            {
                break;
            }
        }

        if(i != numplanes)
        {
            // go along this plane
            ent->v.velocity = new_velocity;
        }
        else
        {
            // go along the crease
            if(numplanes != 2)
            {
                //				Con_Printf ("clip velocity, numplanes ==
                //%i\n",numplanes);
                ent->v.velocity = vec3_zero;
                return 7;
            }

            const auto dir = glm::cross(planes[0], planes[1]);
            const auto d = DotProduct(dir, ent->v.velocity);
            ent->v.velocity = dir * d;
        }

        //
        // if original velocity is against the original velocity, stop dead
        // to avoid tiny occilations in sloping corners
        //
        if(DotProduct(ent->v.velocity, primal_velocity) <= 0)
        {
            ent->v.velocity = vec3_zero;
            return blocked;
        }
    }

    return blocked;
}


/*
============
SV_AddGravity

============
*/
float SV_AddGravityImpl(const float ent_gravity)
{
    return (double)ent_gravity * (double)sv_gravity.value * host_frametime;
}

float SV_AddGravityImpl(edict_t* ent)
{
    eval_t* val = GetEdictFieldValue(ent, qcvm->gravityfieldoffset);

    const float ent_gravity =
        (val && static_cast<bool>(val->_float)) ? val->_float : 1.0f;

    return SV_AddGravityImpl(ent_gravity);
}

void SV_AddGravity(edict_t* ent)
{
    ent->v.velocity[2] -= SV_AddGravityImpl(ent);
}

/*
===============================================================================

PUSHMOVE

===============================================================================
*/

static void SV_PushEntityImpact(edict_t* ent, const trace_t& trace)
{
    if(!trace.ent)
    {
        return;
    }

    SV_Impact(ent, trace.ent, &entvars_t::touch);
}

/*
============
SV_PushEntity

Does not change the entities velocity at all
============
*/
trace_t SV_PushEntity(edict_t* ent, const qvec3& push)
{
    const auto start = ent->v.origin - push;
    const auto end = ent->v.origin + push;

    trace_t trace;
    if(ent->v.movetype == MOVETYPE_FLYMISSILE)
    {
        trace =
            SV_Move(start, ent->v.mins, ent->v.maxs, end, MOVE_MISSILE, ent);
    }
    else if(ent->v.solid == SOLID_TRIGGER || ent->v.solid == SOLID_NOT ||
            ent->v.solid == SOLID_NOT_BUT_TOUCHABLE)
    {
        // only clip against bmodels
        trace =
            SV_Move(start, ent->v.mins, ent->v.maxs, end, MOVE_NOMONSTERS, ent);
    }
    else
    {
        trace = SV_Move(start, ent->v.mins, ent->v.maxs, end, MOVE_NORMAL, ent);
    }

    ent->v.origin = trace.endpos;

    SV_LinkEdict(ent, true);
    SV_PushEntityImpact(ent, trace);

    return trace;
}

/*
============
SV_PushMove
============
*/
void SV_PushMove(edict_t* pusher, float movetime)
{
    // When changing this, test the following:
    // * Lift in E1M1
    // * Platform (controlled by button) in E1M1
    // * Crusher in E1M3
    // * Big doors in E1M3
    // * Slow elevator in E2M6
    // * Crusher in HIP3M4

    if(!pusher->v.velocity[0] && !pusher->v.velocity[1] &&
        !pusher->v.velocity[2])
    {
        pusher->v.ltime += movetime;
        return;
    }

    const auto move = pusher->v.velocity * movetime;
    const auto pusherNewMins = pusher->v.absmin + move;
    const auto pusherNewMaxs = pusher->v.absmax + move;

    const auto oldPushorig = pusher->v.origin;

    // move the pusher to it's final position

    pusher->v.origin += move;
    pusher->v.ltime += movetime;
    SV_LinkEdict(pusher, false);

    // johnfitz -- dynamically allocate
    const int mark = Hunk_LowMark(); // johnfitz
    const auto moved_edict = Hunk_Alloc<edict_t*>(qcvm->num_edicts);
    const auto moved_from = Hunk_Alloc<qvec3>(qcvm->num_edicts);
    // johnfitz

    // see if any solid entities are inside the final position
    int num_moved = 0;
    edict_t* check = NEXT_EDICT(qcvm->edicts);
    for(int e = 1; e < qcvm->num_edicts; e++, check = NEXT_EDICT(check))
    {
        if(check->free)
        {
            continue;
        }

        if(check->v.movetype == MOVETYPE_PUSH ||
            check->v.movetype == MOVETYPE_NONE ||
            check->v.movetype == MOVETYPE_NOCLIP)
        {
            continue;
        }

        // if the entity is standing on the pusher, it will definitely be moved
        if(!quake::util::hasFlag(check, FL_ONGROUND) ||
            PROG_TO_EDICT(check->v.groundentity) != pusher)
        {
            if(!quake::util::boxIntersection(check->v.absmin, check->v.absmax,
                   pusherNewMins, pusherNewMaxs))
            {
                continue;
            }

            // see if the ent's bbox is inside the pusher's final position

            const qvec3 minBottom{0.f, 0.f, -1.f};

            const bool checkIntoSolid =
                SV_TestEntityPositionCustomOrigin(check, check->v.origin);

            trace_t traceBuffer;
            qvec3 offsetBuffer;

            const bool checkOnTopOfPusher =
                quake::util::checkGroundCollision(MOVE_NOMONSTERS, check,
                    traceBuffer, offsetBuffer, minBottom, 0.f, 0.f) &&
                traceBuffer.ent == pusher;

            if(!checkIntoSolid && !checkOnTopOfPusher)
            {
                continue;
            }
        }

        // remove the onground flag for non-players
        if(check->v.movetype != MOVETYPE_WALK)
        {
            quake::util::removeFlag(check, FL_ONGROUND);
        }

        const qvec3 entorig = check->v.origin;
        moved_from[num_moved] = check->v.origin;
        moved_edict[num_moved] = check;
        ++num_moved;

        // try moving the contacted entity
        pusher->v.solid = SOLID_NOT;
        SV_PushEntity(check, move);
        pusher->v.solid = SOLID_BSP;

        if(move[2] > 0)
        {
            quake::util::addFlag(check, FL_ONGROUND);
            check->v.groundentity = EDICT_TO_PROG(pusher);
        }

        const auto checkBlock = [&](edict_t* ent, qvec3 adjUpMove)
        {
            if(adjUpMove[2] < 0)
            {
                adjUpMove[2] *= -1.f;
            }

            const trace_t trace =
                SV_Move(ent->v.origin + adjUpMove, ent->v.mins, ent->v.maxs,
                    ent->v.origin + adjUpMove, MOVE_NORMAL, ent);

            return trace.startsolid;
        };

        // if it is still inside the pusher, block
        const bool block = checkBlock(check, move);

        if(block)
        {
            // fail the move
            if(check->v.mins[0] == check->v.maxs[0])
            {
                continue;
            }

            if(check->v.solid == SOLID_NOT || check->v.solid == SOLID_TRIGGER ||
                check->v.solid == SOLID_NOT_BUT_TOUCHABLE)
            {
                continue;
            }

            check->v.origin = entorig;
            SV_LinkEdict(check, true);

            pusher->v.origin = oldPushorig;
            SV_LinkEdict(pusher, false);
            pusher->v.ltime -= movetime;

            // if the pusher has a "blocked" function, call it
            // otherwise, just stay in place until the obstacle is gone
            if(pusher->v.blocked)
            {
                pr_global_struct->self = EDICT_TO_PROG(pusher);
                pr_global_struct->other = EDICT_TO_PROG(check);
                PR_ExecuteProgram(pusher->v.blocked);
            }

            // move back any entities we already moved
            for(int i = 0; i < num_moved; i++)
            {
                moved_edict[i]->v.origin = moved_from[i];
                SV_LinkEdict(moved_edict[i], false);
            }

            Hunk_FreeToLowMark(mark); // johnfitz
            return;
        }
    }

    Hunk_FreeToLowMark(mark); // johnfitz
}

/*
================
SV_Physics_Pusher

================
*/
void SV_Physics_Pusher(edict_t* ent)
{
    const float oldltime = ent->v.ltime;
    const float thinktime = ent->v.nextthink;

    float movetime;
    if(thinktime < ent->v.ltime + host_frametime)
    {
        movetime = thinktime - ent->v.ltime;
        if(movetime < 0)
        {
            movetime = 0;
        }
    }
    else
    {
        movetime = host_frametime;
    }

    if(movetime)
    {
        SV_PushMove(ent, movetime); // advances ent->v.ltime if not blocked
    }

    if(thinktime > oldltime && thinktime <= ent->v.ltime)
    {
        ent->v.nextthink = 0;
        pr_global_struct->time = qcvm->time;
        pr_global_struct->self = EDICT_TO_PROG(ent);
        pr_global_struct->other = EDICT_TO_PROG(qcvm->edicts);
        PR_ExecuteProgram(ent->v.think);
        if(ent->free)
        {
            return;
        }
    }
}


/*
===============================================================================

CLIENT MOVEMENT

===============================================================================
*/

/*
=============
SV_CheckStuck

This is a big hack to try and fix the rare case of getting stuck in the world
clipping hull.
=============
*/
void SV_CheckStuck(edict_t* ent)
{
    if(!SV_TestEntityPosition(ent))
    {
        ent->v.oldorigin = ent->v.origin;
        return;
    }

    const qvec3 org = ent->v.origin;
    ent->v.origin = ent->v.oldorigin;

    if(!SV_TestEntityPosition(ent))
    {
        Con_DPrintf("Unstuck.\n");
        SV_LinkEdict(ent, true);
        return;
    }

    for(int z = 0; z < 18; z++)
    {
        for(int i = -1; i <= 1; i++)
        {
            for(int j = -1; j <= 1; j++)
            {
                ent->v.origin = org + qvec3{i, j, z};

                if(!SV_TestEntityPosition(ent))
                {
                    Con_DPrintf("Unstuck.\n");
                    SV_LinkEdict(ent, true);
                    return;
                }
            }
        }
    }

    ent->v.origin = org;
    Con_DPrintf("player is stuck.\n");
}


/*
=============
SV_CheckWater
=============
*/
bool SV_CheckWater(edict_t* ent)
{
    const auto prevWaterlevel = ent->v.waterlevel;

    qvec3 point = ent->v.origin;
    point[2] += ent->v.mins[2] + 1;

    ent->v.waterlevel = 0;
    ent->v.watertype = CONTENTS_EMPTY;

    int cont = SV_PointContents(point);
    if(cont <= CONTENTS_WATER)
    {
        ent->v.watertype = cont;
        ent->v.waterlevel = 1;
        point[2] = ent->v.origin[2] + (ent->v.mins[2] + ent->v.maxs[2]) * 0.5f;

        cont = SV_PointContents(point);
        if(cont <= CONTENTS_WATER)
        {
            ent->v.waterlevel = 2;
            point[2] = ent->v.origin[2] + ent->v.view_ofs[2];

            cont = SV_PointContents(point);
            if(cont <= CONTENTS_WATER)
            {
                ent->v.waterlevel = 3;
            }
        }
    }

    if(ent->v.waterlevel != prevWaterlevel)
    {
        ent->v.lastwatertime = qcvm->time;
    }

    return ent->v.waterlevel > 1;
}

/*
============
SV_WallFriction

============
*/
void SV_WallFriction(edict_t* ent, trace_t* trace)
{
    const auto fwd = quake::util::getFwdVecFromPitchYawRoll(ent->v.v_angle);
    qfloat d = DotProduct(trace->plane.normal, fwd);

    d += 0.5_qf;
    if(d >= 0)
    {
        return;
    }

    // cut the tangential velocity
    const auto i = DotProduct(trace->plane.normal, ent->v.velocity);
    const auto into = trace->plane.normal * i;
    const auto side = ent->v.velocity - qvec3(into);

    ent->v.velocity[0] = side[0] * (1 + d);
    ent->v.velocity[1] = side[1] * (1 + d);
}

/*
=====================
SV_TryUnstick

Player has come to a dead stop, possibly due to the problem with limited
float precision at some angle joins in the BSP hull.

Try fixing by pushing one pixel in each direction.

This is a hack, but in the interest of good gameplay...
======================
*/
int SV_TryUnstick(edict_t* ent, qvec3 oldvel)
{
    qvec3 oldorg;
    qvec3 dir;
    int clip;
    trace_t steptrace;

    oldorg = ent->v.origin;
    dir = vec3_zero;

    for(int i = 0; i < 8; i++)
    {
        // try pushing a little in an axial direction
        switch(i)
        {
            case 0:
                dir[0] = 2;
                dir[1] = 0;
                break;
            case 1:
                dir[0] = 0;
                dir[1] = 2;
                break;
            case 2:
                dir[0] = -2;
                dir[1] = 0;
                break;
            case 3:
                dir[0] = 0;
                dir[1] = -2;
                break;
            case 4:
                dir[0] = 2;
                dir[1] = 2;
                break;
            case 5:
                dir[0] = -2;
                dir[1] = 2;
                break;
            case 6:
                dir[0] = 2;
                dir[1] = -2;
                break;
            case 7:
                dir[0] = -2;
                dir[1] = -2;
                break;
        }

        SV_PushEntity(ent, dir);

        // retry the original move
        ent->v.velocity[0] = oldvel[0];
        ent->v.velocity[1] = oldvel[1];
        ent->v.velocity[2] = 0;
        clip = SV_FlyMove(ent, 0.1, &steptrace);

        if(fabs((double)oldorg[1] - (double)ent->v.origin[1]) > 4 ||
            fabs((double)oldorg[0] - (double)ent->v.origin[0]) > 4)
        {
            // Con_DPrintf ("unstuck!\n");
            return clip;
        }

        // go back to the original pos and try again
        ent->v.origin = oldorg;
    }

    ent->v.velocity = vec3_zero;
    return 7; // still not moving
}

/*
=====================
SV_WalkMove

Only used by players
======================
*/
void SV_WalkMove(edict_t* ent, const bool resetOnGround)
{
    //
    // do a regular slide move unless it looks like you ran into a step
    //
    const int oldonground = quake::util::hasFlag(ent, FL_ONGROUND);

    if(resetOnGround)
    {
        quake::util::removeFlag(ent, FL_ONGROUND);
    }

    const qvec3 oldorg = ent->v.origin;
    const qvec3 oldvel = ent->v.velocity;

    trace_t steptrace;
    int clip = SV_FlyMove(ent, host_frametime, &steptrace);

    if(!(clip & 2))
    {
        return; // move didn't block on a step
    }

    if(!oldonground && ent->v.waterlevel == 0)
    {
        return; // don't stair up while jumping
    }

    if(ent->v.movetype != MOVETYPE_WALK)
    {
        return; // gibbed by a trigger
    }

    if(sv_nostep.value)
    {
        return;
    }

    if(quake::util::hasFlag(sv_player, FL_WATERJUMP))
    {
        return;
    }

    const qvec3 nosteporg = ent->v.origin;
    const qvec3 nostepvel = ent->v.velocity;

    //
    // try moving up and forward to go up a step
    //
    ent->v.origin = oldorg; // back to start pos

    const float stepsize = vr_player_stepsize.value;
    const qvec3 upmove{0.f, 0.f, stepsize};
    const qvec3 downmove{0.f, 0.f, -stepsize + oldvel[2] * host_frametime};

    // move up
    SV_PushEntity(ent, upmove); // FIXME: don't link?

    // move forward
    ent->v.velocity[0] = oldvel[0];
    ent->v.velocity[1] = oldvel[1];
    ent->v.velocity[2] = 0;
    clip = SV_FlyMove(ent, host_frametime, &steptrace);

    // check for stuckness, possibly due to the limited precision of floats
    // in the clipping hulls
    if(clip)
    {
        if(fabs((double)oldorg[1] - (double)ent->v.origin[1]) < 0.03125 &&
            fabs((double)oldorg[0] - (double)ent->v.origin[0]) < 0.03125)
        {
            // stepping up didn't make any progress
            clip = SV_TryUnstick(ent, oldvel);
        }
    }

    // extra friction based on view angle
    if(clip & 2)
    {
        SV_WallFriction(ent, &steptrace);
    }

    // move down
    const trace_t downtrace =
        SV_PushEntity(ent, downmove); // FIXME: don't link?

    if(quake::util::traceHitGround(downtrace))
    {
        if(ent->v.solid == SOLID_BSP)
        {
            quake::util::addFlag(ent, FL_ONGROUND);
            ent->v.groundentity = EDICT_TO_PROG(downtrace.ent);
        }
    }
    else
    {
        // if the push down didn't end up on good ground, use the move
        // without the step up.  This happens near wall / slope
        // combinations, and can cause the player to hop up higher on a
        // slope too steep to climb
        ent->v.origin = nosteporg;
        ent->v.velocity = nostepvel;
    }
}

/*
================
SV_Handtouch

Trigger hand-touching actions (e.g. pick up an item, press a button)
================
*/
void SV_Handtouch(edict_t* ent)
{
    // TODO VR: (P2) cleanup, too much unnecessary tracing and work

    // Utility constants
    const qvec3 handOffsets{2.5f, 2.5f, 2.5f};

    // Figure out tracing boundaries
    // (Largest possible volume containing the hands and the player)
    const auto [origin, mins, maxs] = [&]
    {
        const auto& playerOrigin = ent->v.origin;
        const auto& playerMins = ent->v.mins;
        const auto& playerMaxs = ent->v.maxs;
        const auto playerAbsMin = playerOrigin + playerMins;
        const auto playerAbsMax = playerOrigin + playerMaxs;

        const auto& mainHandOrigin = ent->v.handpos;
        const auto mainHandAbsMin = mainHandOrigin - handOffsets;
        const auto mainHandAbsMax = mainHandOrigin + handOffsets;

        const auto& offHandOrigin = ent->v.offhandpos;
        const auto offHandAbsMin = offHandOrigin - handOffsets;
        const auto offHandAbsMax = offHandOrigin + handOffsets;

        const qvec3 minBound{
            std::min({playerAbsMin.x, mainHandAbsMin.x, offHandAbsMin.x}),
            std::min({playerAbsMin.y, mainHandAbsMin.y, offHandAbsMin.y}),
            std::min({playerAbsMin.z, mainHandAbsMin.z, offHandAbsMin.z})};

        const qvec3 maxBound{
            std::max({playerAbsMax.x, mainHandAbsMax.x, offHandAbsMax.x}),
            std::max({playerAbsMax.y, mainHandAbsMax.y, offHandAbsMax.y}),
            std::max({playerAbsMax.z, mainHandAbsMax.z, offHandAbsMax.z})};

        const auto halfSize = (maxBound - minBound) / 2._qf;
        const auto origin = minBound + halfSize;
        return std::tuple{origin, -halfSize, +halfSize};
    }();

    const auto traceCheck = [&](const trace_t& trace)
    {
        if(!trace.ent)
        {
            return;
        }

        const auto handCollisionCheck =
            [&](const int hand, const qvec3& handPos)
        {
            const float bonus =
                (quake::util::hasFlag(trace.ent, FL_EASYHANDTOUCH))
                    ? VR_GetEasyHandTouchBonus()
                    : 0.f;

            const qvec3 bonusVec{bonus, bonus, bonus};

            const auto aMin =
                trace.ent->v.origin + trace.ent->v.mins - bonusVec;
            const auto aMax =
                trace.ent->v.origin + trace.ent->v.maxs + bonusVec;
            const auto bMin = handPos - handOffsets;
            const auto bMax = handPos + handOffsets;

            if(quake::util::boxIntersection(aMin, aMax, bMin, bMax))
            {
                VR_SetHandtouchParams(hand, ent, trace.ent);
                SV_Impact(ent, trace.ent, &entvars_t::handtouch);
            }
        };

        handCollisionCheck(cVR_OffHand, ent->v.offhandpos);
        handCollisionCheck(cVR_MainHand, ent->v.handpos);
    };

    const auto endHandPos = [&](const qvec3& handPos, const qvec3& handRot)
    {
        const auto fwd = quake::util::getFwdVecFromPitchYawRoll(handRot);
        return handPos + fwd * 1._qf;
    };

    const auto mainHandEnd = endHandPos(ent->v.handpos, ent->v.handrot);
    const auto offHandEnd = endHandPos(ent->v.offhandpos, ent->v.offhandrot);

    traceCheck(SV_Move(ent->v.origin, ent->v.mins, ent->v.maxs, mainHandEnd,
        MOVE_NORMAL, ent));
    traceCheck(SV_Move(origin, mins, maxs, mainHandEnd, MOVE_NORMAL, ent));
    traceCheck(SV_Move(
        ent->v.origin, ent->v.mins, ent->v.maxs, offHandEnd, MOVE_NORMAL, ent));
    traceCheck(SV_Move(origin, mins, maxs, offHandEnd, MOVE_NORMAL, ent));

    const auto traceForHand = [&](const qvec3& handPos, const qvec3& handRot)
    {
        const auto fwd = quake::util::getFwdVecFromPitchYawRoll(handRot);
        const auto end = handPos + fwd * 1._qf;

        return SV_Move(
            handPos, -handOffsets, handOffsets, end, MOVE_NORMAL, ent);
    };

    traceCheck(traceForHand(ent->v.handpos, ent->v.handrot));
    traceCheck(traceForHand(ent->v.offhandpos, ent->v.offhandrot));
}

void SV_VRWpntouch(edict_t* ent)
{
    // TODO VR: (P2) code repetition with vr.cpp setHandPos

    const auto doHand = [&](const HandIdx handIndex)
    {
        const auto& playerOrigin = ent->v.origin;

        const auto worldHandPos = VR_GetWorldHandPos(handIndex, playerOrigin);
        const auto adjPlayerOrigin = VR_GetAdjustedPlayerOrigin(playerOrigin);

        const auto resolvedHandPos =
            VR_GetResolvedHandPos(ent, worldHandPos, adjPlayerOrigin);

        VrGunWallCollision collisionData;
        VR_UpdateGunWallCollisions(
            ent, handIndex, collisionData, resolvedHandPos);

        if(collisionData._ent != nullptr && collisionData._ent->v.vr_wpntouch)
        {
            VR_SetHandtouchParams(handIndex, ent, collisionData._ent);
            SV_Impact(ent, collisionData._ent, &entvars_t::vr_wpntouch);
        }
    };

    doHand(cVR_MainHand);
    doHand(cVR_OffHand);
}



/*
================
SV_Physics_Client

Player character actions
================
*/
void SV_Physics_Client(edict_t* ent, int num)
{
    if(!svs.clients[num - 1].active)
    {
        return; // unconnected slot
    }

    //
    // call standard client pre-think
    //
    pr_global_struct->time = qcvm->time;
    pr_global_struct->self = EDICT_TO_PROG(ent);
    PR_ExecuteProgram(pr_global_struct->PlayerPreThink);

    //
    // do a move
    //
    SV_CheckVelocity(ent);

    //
    // VR hands
    //
    SV_Handtouch(ent);
    SV_VRWpntouch(ent);

    //
    // decide which move function to call
    //
    if(quake::util::hasFlag(ent->v.vrbits0, QVR_VRBITS0_TELEPORTING))
    {
        if(!SV_RunThink(ent))
        {
            return;
        }

        ent->v.teleport_time = qcvm->time + 0.3;
        ent->v.origin = ent->v.teleport_target;
        ent->v.oldorigin = ent->v.teleport_target;
    }
    else
    {
        switch((int)ent->v.movetype)
        {
            case MOVETYPE_NONE:
            {
                if(!SV_RunThink(ent))
                {
                    return;
                }

                break;
            }

            case MOVETYPE_WALK:
            {
                if(!SV_RunThink(ent))
                {
                    return;
                }

                if(!SV_CheckWater(ent) &&
                    !(quake::util::hasFlag(ent, FL_WATERJUMP)))
                {
                    SV_AddGravity(ent);
                }

                SV_CheckStuck(ent);
                SV_WalkMove(ent, true /* reset onground */);

                break;
            }

            case MOVETYPE_TOSS: [[fallthrough]];
            case MOVETYPE_BOUNCE:
            {
                SV_Physics_Toss(ent);
                break;
            }

            case MOVETYPE_FLY:
            {
                if(!SV_RunThink(ent))
                {
                    return;
                }

                SV_FlyMove(ent, host_frametime, nullptr);
                break;
            }

            case MOVETYPE_NOCLIP:
            {
                if(!SV_RunThink(ent))
                {
                    return;
                }

                ent->v.origin +=
                    static_cast<float>(host_frametime) * ent->v.velocity;
                break;
            }

            default:
            {
                Sys_Error(
                    "SV_Physics_client: bad movetype %i", (int)ent->v.movetype);
            }
        }

        // --------------------------------------------------------------------
        // VR: Room scale movement for entities.
        {
            const auto restoreVel = ent->v.velocity;

            ent->v.velocity[0] = ent->v.roomscalemove[0];
            ent->v.velocity[1] = ent->v.roomscalemove[1];
            ent->v.velocity[2] = 0.f;

            switch((int)ent->v.movetype)
            {
                case MOVETYPE_WALK:
                {
                    SV_CheckStuck(ent);
                    SV_WalkMove(ent, false /* reset onground */);

                    break;
                }

                case MOVETYPE_NONE: [[fallthrough]];
                case MOVETYPE_TOSS: [[fallthrough]];
                case MOVETYPE_BOUNCE: break;

                case MOVETYPE_FLY:
                {
                    SV_FlyMove(ent, host_frametime, nullptr);

                    break;
                }

                case MOVETYPE_NOCLIP:
                {
                    ent->v.origin +=
                        static_cast<float>(host_frametime) * ent->v.velocity;

                    break;
                }

                default:
                {
                    Sys_Error("SV_Physics_client: bad movetype %i",
                        (int)ent->v.movetype);
                }
            }

            ent->v.velocity = restoreVel;
        }
        // --------------------------------------------------------------------
    }

    //
    // call standard player post-think
    //
    SV_LinkEdict(ent, true);

    pr_global_struct->time = qcvm->time;
    pr_global_struct->self = EDICT_TO_PROG(ent);

    PR_ExecuteProgram(pr_global_struct->PlayerPostThink);
}

//============================================================================

/*
=============
SV_Physics_None

Non moving objects can only think
=============
*/
void SV_Physics_None(edict_t* ent)
{
    // regular thinking
    SV_RunThink(ent);
}

/*
=============
SV_Physics_Noclip

A moving object that doesn't obey physics
=============
*/
void SV_Physics_Noclip(edict_t* ent)
{
    // regular thinking
    if(!SV_RunThink(ent))
    {
        return;
    }

    ent->v.angles += static_cast<float>(host_frametime) * ent->v.avelocity;
    ent->v.origin += static_cast<float>(host_frametime) * ent->v.velocity;

    SV_LinkEdict(ent, false);
}

/*
==============================================================================

TOSS / BOUNCE

==============================================================================
*/

/*
=============
SV_CheckWaterTransition

=============
*/
void SV_CheckWaterTransition(edict_t* ent)
{
    const int cont = SV_PointContents(ent->v.origin);
    const auto prevWaterlevel = ent->v.waterlevel;

    if(!ent->v.watertype)
    {
        // just spawned here
        ent->v.watertype = cont;
        ent->v.waterlevel = 1;
        return;
    }

    const float watertimeDiff = qcvm->time - ent->v.lastwatertime;

    if(cont <= CONTENTS_WATER)
    {
        if(ent->v.watertype == CONTENTS_EMPTY && watertimeDiff > 0.2f)
        {
            // just crossed into water
            SV_StartSound(ent, nullptr, 0, sv_sound_watersplash.string, 255, 1);
        }

        ent->v.watertype = cont;
        ent->v.waterlevel = 1;

        if(ent->v.waterlevel != prevWaterlevel)
        {
            ent->v.lastwatertime = qcvm->time;
        }
    }
    else
    {
        if(ent->v.watertype != CONTENTS_EMPTY && watertimeDiff > 0.2f)
        {
            // just crossed into water
            SV_StartSound(ent, nullptr, 0, sv_sound_watersplash.string, 255, 1);
        }

        ent->v.watertype = CONTENTS_EMPTY;
        ent->v.waterlevel = cont;

        if(ent->v.waterlevel != prevWaterlevel)
        {
            ent->v.lastwatertime = qcvm->time;
        }
    }
}

/*
=============
SV_Physics_Toss

Toss, bounce, and fly movement.  When onground, do nothing.
=============
*/
void SV_Physics_Toss(edict_t* ent)
{
    // regular thinking
    if(!SV_RunThink(ent))
    {
        return;
    }

    // update "on ground" status, stop/bounce if on ground
    {
        qvec3 vel = ent->v.velocity;

        if(ent->v.movetype != MOVETYPE_FLY &&
            ent->v.movetype != MOVETYPE_FLYMISSILE)
        {
            vel[2] -= SV_AddGravityImpl(ent);
        }

        const qvec3 move = vel * static_cast<float>(host_frametime);

        trace_t traceBuffer;
        qvec3 offsetBuffer;

        if(!quake::util::checkGroundCollision(MOVE_NOMONSTERS, ent, traceBuffer,
               offsetBuffer, move, 0._qf, 0._qf))
        {
            if(quake::util::hasFlag(ent, FL_ONGROUND))
            {
                // remove on ground, if entity is not on ground anymore
                quake::util::removeFlag(ent, FL_ONGROUND);
            }
        }
        else
        {
            const float backoff =
                ent->v.movetype == MOVETYPE_BOUNCE ? 1.5_qf : 1._qf;

            ClipVelocity(ent->v.velocity, traceBuffer.plane.normal,
                ent->v.velocity, backoff);

            if(ent->v.velocity[2] < 60 || ent->v.movetype != MOVETYPE_BOUNCE)
            {
                if(!quake::util::hasFlag(ent, FL_ONGROUND))
                {
                    quake::util::addFlag(ent, FL_ONGROUND);

                    ent->v.groundentity = EDICT_TO_PROG(traceBuffer.ent);
                    ent->v.velocity = ent->v.avelocity = vec3_zero;
                    ent->v.origin = qvec3(traceBuffer.endpos) - ent->v.mins[2] -
                                    qvec3(offsetBuffer);

                    SV_LinkEdict(ent, true);
                    SV_PushEntityImpact(ent, traceBuffer);
                }

                return;
            }
        }
    }

    SV_CheckVelocity(ent);

    // add gravity
    if(ent->v.movetype != MOVETYPE_FLY &&
        ent->v.movetype != MOVETYPE_FLYMISSILE)
    {
        SV_AddGravity(ent);
    }

    // move angles
    ent->v.angles += static_cast<float>(host_frametime) * ent->v.avelocity;

    // move origin
    const qvec3 move = ent->v.velocity * static_cast<float>(host_frametime);

    const trace_t trace = SV_PushEntity(ent, move);
    if(quake::util::hitSomething(trace) && !ent->free)
    {
        const float backoff = ent->v.movetype == MOVETYPE_BOUNCE ? 1.5f : 1.f;
        ClipVelocity(
            ent->v.velocity, trace.plane.normal, ent->v.velocity, backoff);
    }

    // check for in water
    SV_CheckWaterTransition(ent);
}

/*
===============================================================================

STEPPING MOVEMENT

===============================================================================
*/

/*
=============
SV_Physics_Step

Monsters freefall when they don't have a ground entity, otherwise
all movement is done with discrete steps.

This is also used for objects that have become still on the ground, but
will fall if the floor is pulled out from under them.
=============
*/
void SV_Physics_Step(edict_t* ent)
{
    bool hitsound;

    // freefall if not onground
    if(!quake::util::hasAnyFlag(ent, FL_ONGROUND, FL_FLY, FL_SWIM))
    {
        if(ent->v.velocity[2] < sv_gravity.value * -0.1)
        {
            hitsound = true;
        }
        else
        {
            hitsound = false;
        }

        SV_AddGravity(ent);
        SV_CheckVelocity(ent);
        SV_FlyMove(ent, host_frametime, nullptr);
        SV_LinkEdict(ent, true);

        if(quake::util::hasFlag(ent, FL_ONGROUND)) // just hit ground
        {
            if(hitsound)
            {
                SV_StartSound(ent, nullptr, 0, sv_sound_land.string, 255, 1);
            }
        }
    }

    // regular thinking
    SV_RunThink(ent);

    SV_CheckWaterTransition(ent);
}


//============================================================================

/*
================
SV_Physics

================
*/
void SV_Physics()
{
    int i;
    int entity_cap; // For sv_freezenonclients
    edict_t* ent;

    // let the progs know that a new frame has started
    pr_global_struct->self = EDICT_TO_PROG(qcvm->edicts);
    pr_global_struct->other = EDICT_TO_PROG(qcvm->edicts);
    pr_global_struct->time = qcvm->time;
    PR_ExecuteProgram(pr_global_struct->StartFrame);

    // SV_CheckAllEnts ();

    //
    // treat each object in turn
    //
    ent = qcvm->edicts;

    if(sv_freezenonclients.value)
    {
        entity_cap =
            svs.maxclients + 1; // Only run physics on clients and the world
    }
    else
    {
        entity_cap = qcvm->num_edicts;
    }

    // for (i=0 ; i<qcvm->num_edicts ; i++, ent = NEXT_EDICT(ent))
    for(i = 0; i < entity_cap; i++, ent = NEXT_EDICT(ent))
    {
        if(ent->free)
        {
            continue;
        }

        if(pr_global_struct->force_retouch)
        {
            SV_LinkEdict(ent, true); // force retouch even for stationary
        }

        if(i > 0 && i <= svs.maxclients)
        {
            SV_Physics_Client(ent, i);
        }
        else if(ent->v.movetype == MOVETYPE_PUSH)
        {
            SV_Physics_Pusher(ent);
        }
        else if(ent->v.movetype == MOVETYPE_NONE)
        {
            SV_Physics_None(ent);
        }
        else if(ent->v.movetype == MOVETYPE_NOCLIP)
        {
            SV_Physics_Noclip(ent);
        }
        else if(ent->v.movetype == MOVETYPE_STEP)
        {
            SV_Physics_Step(ent);
        }
        else if(ent->v.movetype == MOVETYPE_TOSS ||
                ent->v.movetype == MOVETYPE_BOUNCE ||
                ent->v.movetype == MOVETYPE_FLY ||
                ent->v.movetype == MOVETYPE_FLYMISSILE)
        {
            SV_Physics_Toss(ent);
        }
        else
        {
            Sys_Error("SV_Physics: bad movetype %i", (int)ent->v.movetype);
        }
    }

    if(pr_global_struct->force_retouch)
    {
        pr_global_struct->force_retouch--;
    }

    if(!sv_freezenonclients.value)
    {
        qcvm->time += host_frametime;
    }
}
