/*
Copyright (C) 1996-2001 Id Software, Inc.
Copyright (C) 2002-2009 John Fitzgibbons and others
Copyright (C) 2010-2014 QuakeSpasm developers
Copyright (C) 2020-2020 Vittorio Romeo

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

#include "fwd.hpp"
#include "quakedef.hpp"
#include "vr.hpp"
#include "world.hpp"
#include "util.hpp"
#include "quakeglm.hpp"

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


#define MOVE_EPSILON 0.01

void SV_Physics_Toss(edict_t* ent);

/*
================
SV_CheckAllEnts
================
*/
void SV_CheckAllEnts()
{
    int e;
    edict_t* check;

    // see if any solid entities are inside the final position
    check = NEXT_EDICT(sv.edicts);
    for(e = 1; e < sv.num_edicts; e++, check = NEXT_EDICT(check))
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
    int i;

    //
    // bound velocity
    //
    for(i = 0; i < 3; i++)
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
        if(ent->v.velocity[i] > sv_maxvelocity.value)
        {
            ent->v.velocity[i] = sv_maxvelocity.value;
        }
        else if(ent->v.velocity[i] < -sv_maxvelocity.value)
        {
            ent->v.velocity[i] = -sv_maxvelocity.value;
        }
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
    float thinktime = (ent->v).*TNextThink;
    if(thinktime <= 0 || thinktime > sv.time + host_frametime)
    {
        return true;
    }

    if(thinktime < sv.time)
    {
        thinktime = sv.time; // don't let things stay in the past.
    }
    // it is possible to start that way
    // by a trigger with a local time.

    float oldframe = ent->v.frame; // johnfitz

    (ent->v).*TNextThink = 0;
    pr_global_struct->time = thinktime;
    pr_global_struct->self = EDICT_TO_PROG(ent);
    pr_global_struct->other = EDICT_TO_PROG(sv.edicts);
    PR_ExecuteProgram((ent->v).*TThink);

    if(TDoLerp)
    {
        // johnfitz -- PROTOCOL_FITZQUAKE
        // capture interval to nextthink here and send it to client for better
        // lerp timing, but only if interval is not 0.1 (which client assumes)
        ent->sendinterval = false;
        if(!ent->free && (ent->v).*TNextThink &&
            (ent->v.movetype == MOVETYPE_STEP || ent->v.frame != oldframe))
        {
            int i = Q_rint(((ent->v).*TNextThink - thinktime) * 255);
            if(i >= 0 && i < 256 && i != 25 && i != 26)
            { // 25 and 26 are close enough to 0.1 to not send
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

    pr_global_struct->time = sv.time;
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

int ClipVelocity(const glm::vec3& in, const glm::vec3& normal, glm::vec3& out,
    float overbounce)
{
    float backoff;
    float change;

    int blocked;

    blocked = 0;
    if(normal[2] > 0)
    {
        blocked |= 1; // floor
    }
    if(!normal[2])
    {
        blocked |= 2; // step
    }

    backoff = DotProduct(in, normal) * overbounce;

    for(int i = 0; i < 3; i++)
    {
        change = normal[i] * backoff;
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

    glm::vec3 planes[MAX_CLIP_PLANES];
    glm::vec3 primal_velocity = ent->v.velocity;
    glm::vec3 original_velocity = ent->v.velocity;
    glm::vec3 new_velocity;

    float time_left = time;

    int blocked = 0;
    int numplanes = 0;

    for(int bumpcount = 0; bumpcount < numbumps; bumpcount++)
    {
        if(!ent->v.velocity[0] && !ent->v.velocity[1] && !ent->v.velocity[2])
        {
            break;
        }

        glm::vec3 end;
        for(int i = 0; i < 3; i++)
        {
            end[i] = ent->v.origin[i] + time_left * ent->v.velocity[i];
        }

        trace_t trace =
            SV_Move(ent->v.origin, ent->v.mins, ent->v.maxs, end, false, ent);

        if(trace.allsolid)
        { // entity is trapped in another solid
            ent->v.velocity = vec3_zero;
            return 3;
        }

        if(trace.fraction > 0)
        { // actually covered some distance
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

        if(trace.plane.normal[2] > 0.7)
        {
            blocked |= 1; // floor
            if(trace.ent->v.solid == SOLID_BSP)
            {
                ent->v.flags = (int)ent->v.flags | FL_ONGROUND;
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

        // --------------------------------------------------------------------
        // VR: Simulate touching with right hand if body interactions are off.
        if(!vr_enabled.value || vr_body_interactions.value == 1)
        {
            VR_SetFakeHandtouchParams(ent, trace.ent);
            SV_Impact(ent, trace.ent, &entvars_t::handtouch);
        }
        // --------------------------------------------------------------------

        if(ent->free)
        {
            break; // removed by the impact function
        }

        time_left -= time_left * trace.fraction;

        // cliped to another plane
        if(numplanes >= MAX_CLIP_PLANES)
        { // this shouldn't really happen
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
        { // go along this plane
            ent->v.velocity = new_velocity;
        }
        else
        { // go along the crease
            if(numplanes != 2)
            {
                //				Con_Printf ("clip velocity, numplanes ==
                //%i\n",numplanes);
                ent->v.velocity = vec3_zero;
                return 7;
            }

            const auto dir = glm::cross(planes[0], planes[1]);
            const float d = DotProduct(dir, ent->v.velocity);
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
void SV_AddGravity(edict_t* ent)
{
    float ent_gravity;
    eval_t* val;

    val = GetEdictFieldValue(ent, "gravity");
    if(val && val->_float)
    {
        ent_gravity = val->_float;
    }
    else
    {
        ent_gravity = 1.0;
    }

    ent->v.velocity[2] -=
        (double)ent_gravity * (double)sv_gravity.value * host_frametime;
}


/*
===============================================================================

PUSHMOVE

===============================================================================
*/

/*
============
SV_PushEntity

Does not change the entities velocity at all
============
*/
trace_t SV_PushEntity(edict_t* ent, const glm::vec3& push)
{
    trace_t trace;

    const glm::vec3 end = ent->v.origin + push;

    if(ent->v.movetype == MOVETYPE_FLYMISSILE)
    {
        trace = SV_Move(
            ent->v.origin, ent->v.mins, ent->v.maxs, end, MOVE_MISSILE, ent);
    }
    else if(ent->v.solid == SOLID_TRIGGER || ent->v.solid == SOLID_NOT)
    {
        // only clip against bmodels
        trace = SV_Move(
            ent->v.origin, ent->v.mins, ent->v.maxs, end, MOVE_NOMONSTERS, ent);
    }
    else
    {
        trace = SV_Move(
            ent->v.origin, ent->v.mins, ent->v.maxs, end, MOVE_NORMAL, ent);
    }

    ent->v.origin = trace.endpos;
    SV_LinkEdict(ent, true);

    if(trace.ent)
    {
        SV_Impact(ent, trace.ent, &entvars_t::touch);

        if(!vr_enabled.value || vr_body_interactions.value == 1)
        {
            VR_SetFakeHandtouchParams(ent, trace.ent);
            SV_Impact(ent, trace.ent, &entvars_t::handtouch);
        }
    }

    return trace;
}


/*
============
SV_PushMove
============
*/
void SV_PushMove(edict_t* const pusher, const float movetime)
{
    if(!pusher->v.velocity[0] && !pusher->v.velocity[1] &&
        !pusher->v.velocity[2])
    {
        pusher->v.ltime += movetime;
        return;
    }

    const glm::vec3 move = pusher->v.velocity * movetime;
    const glm::vec3 pusherNewMins = pusher->v.absmin + move;
    const glm::vec3 pusherNewMaxs = pusher->v.absmax + move;

    const glm::vec3 oldPushorig = pusher->v.origin;

    // move the pusher to it's final position

    pusher->v.origin += move;
    pusher->v.ltime += movetime;
    SV_LinkEdict(pusher, false);

    // johnfitz -- dynamically allocate
    const int mark = Hunk_LowMark(); // johnfitz
    const auto moved_edict = Hunk_Alloc<edict_t*>(sv.num_edicts);
    const auto moved_from = Hunk_Alloc<glm::vec3>(sv.num_edicts);
    // johnfitz

    // see if any solid entities are inside the final position
    int num_moved = 0;
    edict_t* check = NEXT_EDICT(sv.edicts);
    for(int e = 1; e < sv.num_edicts; e++, check = NEXT_EDICT(check))
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
        if(!(((int)check->v.flags & FL_ONGROUND) &&
               PROG_TO_EDICT(check->v.groundentity) == pusher))
        {
            if(!quake::util::boxIntersection(check->v.absmin + move,
                   check->v.absmax + move, pusher->v.absmin, pusher->v.absmax))
            {
                continue;
            }

            /*
            const auto fmove = move * 1.1f;

            const auto traceline = [&](const glm::vec3& v1,
                                       const glm::vec3& v2) {
                return SV_Move(
                    v1, vec3_zero, vec3_zero, v2, MOVE_NORMAL, check);
            };

            const auto amn = check->v.absmin;
            const auto amx = check->v.absmax;

            const glm::vec3 bottomSW{amn.x, amn.y, amn.z};
            const glm::vec3 bottomNW{amx.x, amn.y, amn.z};
            const glm::vec3 bottomSE{amn.x, amx.y, amn.z};
            const glm::vec3 bottomNE{amx.x, amx.y, amn.z};
            const glm::vec3 topSW{amn.x, amn.y, amx.z};
            const glm::vec3 topNW{amx.x, amn.y, amx.z};
            const glm::vec3 topSE{amn.x, amx.y, amx.z};
            const glm::vec3 topNE{amx.x, amx.y, amx.z};

            const glm::vec3 bottomFace[4]{
                bottomSW, bottomNW, bottomSE, bottomNE};

            const glm::vec3 topFace[4]{topSW, topNW, topSE, topNE};

            const glm::vec3 frontFace[4]{bottomSW, bottomSE, topSW, topSE};

            const glm::vec3 frontFace[4]{bottomSW, bottomSE, topSW, topSE};

            const bool collidingDown =traceline(
            */



            const trace_t t0 = SV_Move(check->v.origin, vec3_zero, vec3_zero,
                check->v.origin + (check->v.mins * 1.02f), MOVE_NORMAL, check);

            const trace_t t1 = SV_Move(check->v.origin, vec3_zero, vec3_zero,
                check->v.origin + (check->v.maxs * 1.02f), MOVE_NORMAL, check);

            const bool t0coll = t0.fraction < 1.f && t0.ent == pusher;
            const bool t1coll = t1.fraction < 1.f && t1.ent == pusher;

            const bool anyCollision = t0coll || t1coll;

            if(!anyCollision)
            {
                static int j = 0;
                Con_Printf("dio diavolo %d\n", j++);

                continue;
            }

            if(t1coll && t0.fraction == 1.f)
            {
                check->v.origin = t1.endpos - check->v.maxs * 1.05f + move;
                // check->v.flags = (int)check->v.flags & ~FL_ONGROUND;
            }

            // TODO VR: must check all point of the face
            // see if the ent's bbox is inside the pusher's final position
            // if(!SV_TestEntityPositionCustom(
            //       check, check->v.origin + check->v.mins + move * 1.1f) &&
            //    !SV_TestEntityPositionCustom(
            //        check, check->v.origin + check->v.maxs + move * 1.1f))
            //{
            //    static int j = 0;
            //    Con_Printf("dio diavolo %d\n", j++);
            //
            //    continue;
            //}
        }

        // remove the onground flag for non-players
        if(check->v.movetype != MOVETYPE_WALK)
        {
            check->v.flags = (int)check->v.flags & ~FL_ONGROUND;
        }

        const glm::vec3 entorig = check->v.origin;
        moved_from[num_moved] = check->v.origin;
        moved_edict[num_moved] = check;
        ++num_moved;

        // try moving the contacted entity
        pusher->v.solid = SOLID_NOT;
        static int i = 0;
        Con_Printf("do move %d\n", ++i);
        SV_PushEntity(check, move);
        pusher->v.solid = SOLID_BSP;

        const auto& aMins = check->v.origin + check->v.mins;
        const auto& aMaxs = check->v.origin + check->v.maxs;
        const auto& bMins = pusherNewMins;
        const auto& bMaxs = pusherNewMaxs;

        const trace_t t0 = SV_Move(check->v.origin, vec3_zero, vec3_zero,
            check->v.origin + (check->v.mins * 0.95f), MOVE_NORMAL, check);

        const trace_t t1 = SV_Move(check->v.origin, vec3_zero, vec3_zero,
            check->v.origin + (check->v.maxs * 0.95f), MOVE_NORMAL, check);

        // if(SV_TestEntityPositionCustom(check, check->v.origin +
        // check->v.mins))

        if( //
            (t0.fraction < 1.f && t0.ent == pusher) ||
            (t1.fraction < 1.f && t1.ent == pusher) //
        )
        {
            // fail the move
            if(check->v.mins[0] == check->v.maxs[0])
            {
                continue;
            }

            if(check->v.solid == SOLID_NOT || check->v.solid == SOLID_TRIGGER ||
                check->v.solid == SOLID_NOT_BUT_TOUCHABLE)
            {
                // TODO VR: handtouch bug?? ammo is solid trigger
                // corpse
                check->v.mins[0] = check->v.mins[1] = 0;
                check->v.maxs = check->v.mins;
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

        // if(quake::util::boxIntersection(aMins, aMaxs, bMins, bMaxs))
        if(false && SV_TestEntityPositionCustom(
                        check, check->v.origin + check->v.mins + move * 1.1f))
        {
            const float xDiffs[2]{bMaxs[0] - aMins[0], bMins[0] - aMaxs[0]};
            const float yDiffs[2]{bMaxs[1] - aMins[1], bMins[1] - aMaxs[1]};
            const float zDiffs[2]{bMaxs[2] - aMins[2], bMins[2] - aMaxs[2]};

            const auto smallerAbs = [](const float a, const float b) {
                const auto absA = std::abs(a);
                const auto absB = std::abs(b);

                return absA < absB ? a : b;
            };

            const float minXDiff = smallerAbs(xDiffs[0], xDiffs[1]);
            const float minYDiff = smallerAbs(yDiffs[0], yDiffs[1]);
            const float minZDiff = smallerAbs(zDiffs[0], zDiffs[1]);

            // const glm::vec3 resolution{0.f, 0.f, minZDiff};
            // const auto unstuckPush = resolution * 1.001f * 0.f;
            // check->v.velocity.z = 0;
            // Con_Printf("%2.f\n", check->v.velocity.z);

            const glm::vec3 resolution{minXDiff, minYDiff, minZDiff};
            const glm::vec3 resolutionDir = glm::normalize(-move);
            const auto unstuckPush = resolution * resolutionDir * 1.001f;


            const auto oldCheckOrig = check->v.origin;
            pusher->v.solid = SOLID_NOT;
            const auto t = SV_PushEntity(check, unstuckPush);
            pusher->v.solid = SOLID_BSP;
            const auto actualPush = t.endpos - oldCheckOrig;
            const auto checkNewMins = check->v.origin + check->v.mins;
            const auto checkNewMaxs = check->v.origin + check->v.maxs;

            if(unstuckPush[2] > 0)
            {
                check->v.flags = (int)check->v.flags | FL_ONGROUND;
                check->v.groundentity = EDICT_TO_PROG(pusher);
            }

            quake::util::debugPrintSeparated("\n", oldCheckOrig, unstuckPush,
                check->v.origin, checkNewMins, checkNewMaxs, pusherNewMins,
                pusherNewMaxs, minXDiff, minYDiff, minZDiff, resolution,
                actualPush, xDiffs[0], xDiffs[1], yDiffs[0], yDiffs[1],
                zDiffs[0], zDiffs[1]);
            quake::util::debugPrint("\n\n");

            // if it is still inside the pusher, block
            // this can change `check`'s origin, this is why we save `entorig`
            // TODO VR:
            // if(SV_TestEntityPosition(check))

            // check->v.origin[2] = pusherNewMaxs[2] + check->v.maxs[2];
            // check->v.velocity[2] = move[2];
            // Hunk_FreeToLowMark(mark); // johnfitz
            // return;

            if(quake::util::boxIntersection(
                   checkNewMins, checkNewMaxs, pusherNewMins, pusherNewMaxs))
            {
                // fail the move
                if(check->v.mins[0] == check->v.maxs[0])
                {
                    continue;
                }

                if(check->v.solid == SOLID_NOT ||
                    check->v.solid == SOLID_TRIGGER ||
                    check->v.solid == SOLID_NOT_BUT_TOUCHABLE)
                {
                    // TODO VR: handtouch bug?? ammo is solid trigger
                    // corpse
                    check->v.mins[0] = check->v.mins[1] = 0;
                    check->v.maxs = check->v.mins;
                    continue;
                }

                quake::util::debugPrintSeparated("\n", oldCheckOrig,
                    unstuckPush, check->v.origin, checkNewMins, checkNewMaxs,
                    pusherNewMins, pusherNewMaxs, minXDiff, minYDiff, minZDiff,
                    resolution, xDiffs[0], xDiffs[1], yDiffs[0], yDiffs[1],
                    zDiffs[0], zDiffs[1]);
                quake::util::debugPrint("\n\n");

                Con_Printf("entity pushed by %.2f %.2f %.2f still blocked\n",
                    unstuckPush[0], unstuckPush[1], unstuckPush[2]);
                Con_Printf("actual move %.2f %.2f %.2f\n", actualPush[0],
                    actualPush[1], actualPush[2]);
                Con_Printf("frac: %2.f; entnull: %s; pnorm: %.2f %.2f %.2f\n\n",
                    t.fraction, t.ent == nullptr ? "yes" : "no",
                    t.plane.normal[0], t.plane.normal[1], t.plane.normal[2]);

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
    float movetime;
    const float oldltime = ent->v.ltime;
    const float thinktime = ent->v.nextthink;

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
        pr_global_struct->time = sv.time;
        pr_global_struct->self = EDICT_TO_PROG(ent);
        pr_global_struct->other = EDICT_TO_PROG(sv.edicts);
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
    int i;

    int j;
    int z;
    glm::vec3 org;

    if(!SV_TestEntityPosition(ent))
    {
        ent->v.oldorigin = ent->v.origin;
        return;
    }

    org = ent->v.origin;
    ent->v.origin = ent->v.oldorigin;
    if(!SV_TestEntityPosition(ent))
    {
        Con_DPrintf("Unstuck.\n");
        SV_LinkEdict(ent, true);
        return;
    }

    for(z = 0; z < 18; z++)
    {
        for(i = -1; i <= 1; i++)
        {
            for(j = -1; j <= 1; j++)
            {
                ent->v.origin[0] = org[0] + i;
                ent->v.origin[1] = org[1] + j;
                ent->v.origin[2] = org[2] + z;
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
    glm::vec3 point;
    int cont;

    point[0] = ent->v.origin[0];
    point[1] = ent->v.origin[1];
    point[2] = ent->v.origin[2] + ent->v.mins[2] + 1;

    ent->v.waterlevel = 0;
    ent->v.watertype = CONTENTS_EMPTY;
    cont = SV_PointContents(point);
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

    return ent->v.waterlevel > 1;
}

/*
============
SV_WallFriction

============
*/
void SV_WallFriction(edict_t* ent, trace_t* trace)
{
    const auto [forward, right, up] =
        quake::util::getAngledVectors(ent->v.v_angle);

    float d = DotProduct(trace->plane.normal, forward);

    d += 0.5;
    if(d >= 0)
    {
        return;
    }

    // cut the tangential velocity
    float i = DotProduct(trace->plane.normal, ent->v.velocity);
    const glm::vec3 into = trace->plane.normal * i;
    const glm::vec3 side = ent->v.velocity - into;

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
int SV_TryUnstick(edict_t* ent, glm::vec3 oldvel)
{
    int i;
    glm::vec3 oldorg;
    glm::vec3 dir;
    int clip;
    trace_t steptrace;

    oldorg = ent->v.origin;
    dir = vec3_zero;

    for(i = 0; i < 8; i++)
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
#define STEPSIZE 18
void SV_WalkMove(edict_t* ent)
{
    //
    // do a regular slide move unless it looks like you ran into a step
    //
    const int oldonground = (int)ent->v.flags & FL_ONGROUND;
    ent->v.flags = (int)ent->v.flags & ~FL_ONGROUND;

    glm::vec3 oldorg;
    oldorg = ent->v.origin;

    glm::vec3 oldvel;
    oldvel = ent->v.velocity;

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

    if((int)sv_player->v.flags & FL_WATERJUMP)
    {
        return;
    }

    glm::vec3 nosteporg;
    nosteporg = ent->v.origin;

    glm::vec3 nostepvel;
    nostepvel = ent->v.velocity;

    //
    // try moving up and forward to go up a step
    //
    ent->v.origin = oldorg; // back to start pos

    glm::vec3 upmove;
    upmove = vec3_zero;
    upmove[2] = STEPSIZE;

    glm::vec3 downmove;
    downmove = vec3_zero;
    downmove[2] = -STEPSIZE + oldvel[2] * host_frametime;

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
        { // stepping up didn't make any progress
            clip = SV_TryUnstick(ent, oldvel);
        }
    }

    // extra friction based on view angle
    if(clip & 2)
    {
        SV_WallFriction(ent, &steptrace);
    }

    // move down
    trace_t downtrace = SV_PushEntity(ent, downmove); // FIXME: don't link?

    if(downtrace.plane.normal[2] > 0.7)
    {
        if(ent->v.solid == SOLID_BSP)
        {
            ent->v.flags = (int)ent->v.flags | FL_ONGROUND;
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
    // TODO VR: cleanup, too much unnecessary tracing and work

    // Utility constants
    const glm::vec3 handMins{-2.f, -2.f, -2.f};
    const glm::vec3 handMaxs{2.f, 2.f, 2.f};

    // Figure out tracing boundaries
    // (Largest possible volume containing the hands and the player)
    const auto [origin, mins, maxs] = [&] {
        const auto& playerOrigin = ent->v.origin;
        const auto& playerMins = ent->v.mins;
        const auto& playerMaxs = ent->v.maxs;
        const auto playerAbsMin = playerOrigin + playerMins;
        const auto playerAbsMax = playerOrigin + playerMaxs;

        const auto& mainHandOrigin = ent->v.handpos;
        const auto mainHandAbsMin = mainHandOrigin + handMins;
        const auto mainHandAbsMax = mainHandOrigin + handMaxs;

        const auto& offHandOrigin = ent->v.offhandpos;
        const auto offHandAbsMin = offHandOrigin + handMins;
        const auto offHandAbsMax = offHandOrigin + handMaxs;

        const glm::vec3 minBound{
            std::min({playerAbsMin.x, mainHandAbsMin.x, offHandAbsMin.x}),
            std::min({playerAbsMin.y, mainHandAbsMin.y, offHandAbsMin.y}),
            std::min({playerAbsMin.z, mainHandAbsMin.z, offHandAbsMin.z})};

        const glm::vec3 maxBound{
            std::max({playerAbsMax.x, mainHandAbsMax.x, offHandAbsMax.x}),
            std::max({playerAbsMax.y, mainHandAbsMax.y, offHandAbsMax.y}),
            std::max({playerAbsMax.z, mainHandAbsMax.z, offHandAbsMax.z})};

        const auto halfSize = (maxBound - minBound) / 2.f;
        const auto origin = minBound + halfSize;
        return std::tuple{origin, -halfSize, +halfSize};
    }();

    const auto traceCheck = [&](const trace_t& trace) {
        if(!trace.ent)
        {
            return;
        }

        const auto handCollisionCheck = [&](const int hand,
                                            const glm::vec3& handPos) {
            const glm::vec3 aMin = trace.ent->v.origin + trace.ent->v.mins;
            const glm::vec3 aMax = trace.ent->v.origin + trace.ent->v.maxs;
            const glm::vec3 bMin = handPos + handMins;
            const glm::vec3 bMax = handPos + handMaxs;

            if(quake::util::boxIntersection(aMin, aMax, bMin, bMax))
            {
                VR_SetHandtouchParams(hand, ent, trace.ent);
                SV_Impact(ent, trace.ent, &entvars_t::handtouch);
            }
        };

        handCollisionCheck(0, ent->v.offhandpos);
        handCollisionCheck(1, ent->v.handpos);
    };

    const auto endHandPos = [&](const glm::vec3& handPos,
                                const glm::vec3& handRot) {
        const auto [fwd, right, up] = quake::util::getAngledVectors(handRot);
        return handPos + fwd * 1.f;
    };

    const auto mainHandEnd = endHandPos(ent->v.handpos, ent->v.handrot);
    const auto offHandEnd = endHandPos(ent->v.offhandpos, ent->v.offhandrot);

    traceCheck(SV_Move(ent->v.origin, ent->v.mins, ent->v.maxs, mainHandEnd,
        MOVE_NORMAL, ent));
    traceCheck(SV_Move(origin, mins, maxs, mainHandEnd, MOVE_NORMAL, ent));
    traceCheck(SV_Move(
        ent->v.origin, ent->v.mins, ent->v.maxs, offHandEnd, MOVE_NORMAL, ent));
    traceCheck(SV_Move(origin, mins, maxs, offHandEnd, MOVE_NORMAL, ent));

    const auto traceForHand = [&](const glm::vec3& handPos,
                                  const glm::vec3& handRot) {
        const auto [fwd, right, up] = quake::util::getAngledVectors(handRot);
        const auto end = handPos + fwd * 1.f;

        return SV_Move(handPos, handMins, handMaxs, end, MOVE_NORMAL, ent);
    };

    traceCheck(traceForHand(ent->v.handpos, ent->v.handrot));
    traceCheck(traceForHand(ent->v.offhandpos, ent->v.offhandrot));
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
    pr_global_struct->time = sv.time;
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

    //
    // decide which move function to call
    //
    if(ent->v.teleporting == 1)
    {
        if(!SV_RunThink(ent))
        {
            return;
        }

        ent->v.teleport_time = sv.time + 0.3;
        ent->v.teleport_target[2] += 12;
        ent->v.origin = ent->v.teleport_target;
        ent->v.oldorigin = ent->v.teleport_target;
    }
    else
    {
        switch((int)ent->v.movetype)
        {
            case MOVETYPE_NONE:
                if(!SV_RunThink(ent))
                {
                    return;
                }
                break;

            case MOVETYPE_WALK:
            {
                if(!SV_RunThink(ent))
                {
                    return;
                }

                if(!SV_CheckWater(ent) && !((int)ent->v.flags & FL_WATERJUMP))
                {
                    SV_AddGravity(ent);
                }
                SV_CheckStuck(ent);
                SV_WalkMove(ent);


                break;
            }

            case MOVETYPE_TOSS:
            case MOVETYPE_BOUNCE: SV_Physics_Toss(ent); break;

            case MOVETYPE_FLY:
                if(!SV_RunThink(ent))
                {
                    return;
                }
                SV_FlyMove(ent, host_frametime, nullptr);
                break;

            case MOVETYPE_NOCLIP:
                if(!SV_RunThink(ent))
                {
                    return;
                }
                ent->v.origin +=
                    static_cast<float>(host_frametime) * ent->v.velocity;
                break;

            default:
                Sys_Error(
                    "SV_Physics_client: bad movetype %i", (int)ent->v.movetype);
        }

        if(num == cl.viewentity && vr_enabled.value)
        {
            const auto restoreVel = ent->v.velocity;
            extern glm::vec3 vr_room_scale_move;

            const auto newVelocity =
                vr_room_scale_move * static_cast<float>(1.0f / host_frametime);
            ent->v.velocity = newVelocity;

            switch((int)ent->v.movetype)
            {
                case MOVETYPE_NONE: break;

                case MOVETYPE_WALK:
                    ent->v.velocity[2] = -1.0f;
                    SV_CheckStuck(ent);
                    SV_WalkMove(ent);

                    break;

                case MOVETYPE_TOSS:
                case MOVETYPE_BOUNCE: break;

                case MOVETYPE_FLY:
                    SV_FlyMove(ent, host_frametime, nullptr);
                    break;

                case MOVETYPE_NOCLIP:
                    ent->v.origin +=
                        static_cast<float>(host_frametime) * ent->v.velocity;
                    break;

                default:
                    Sys_Error("SV_Physics_client: bad movetype %i",
                        (int)ent->v.movetype);
            }

            ent->v.velocity = restoreVel;
        }
    }

    //
    // call standard player post-think
    //
    SV_LinkEdict(ent, true);

    pr_global_struct->time = sv.time;
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
    int cont;

    cont = SV_PointContents(ent->v.origin);

    if(!ent->v.watertype)
    { // just spawned here
        ent->v.watertype = cont;
        ent->v.waterlevel = 1;
        return;
    }

    if(cont <= CONTENTS_WATER)
    {
        if(ent->v.watertype == CONTENTS_EMPTY)
        { // just crossed into water
            SV_StartSound(ent, 0, "misc/h2ohit1.wav", 255, 1);
        }
        ent->v.watertype = cont;
        ent->v.waterlevel = 1;
    }
    else
    {
        if(ent->v.watertype != CONTENTS_EMPTY)
        { // just crossed into water
            SV_StartSound(ent, 0, "misc/h2ohit1.wav", 255, 1);
        }
        ent->v.watertype = CONTENTS_EMPTY;
        ent->v.waterlevel = cont;
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

    // if onground, return without moving
    if(((int)ent->v.flags & FL_ONGROUND))
    {
        return;
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
    glm::vec3 move = ent->v.velocity * static_cast<float>(host_frametime);
    const trace_t trace = SV_PushEntity(ent, move);

    if(trace.fraction == 1)
    {
        return;
    }

    if(ent->free)
    {
        return;
    }

    float backoff;
    if(ent->v.movetype == MOVETYPE_BOUNCE)
    {
        backoff = 1.5;
    }
    else
    {
        backoff = 1;
    }

    ClipVelocity(ent->v.velocity, trace.plane.normal, ent->v.velocity, backoff);

    // stop if on ground
    if(trace.plane.normal[2] > 0.7)
    {
        if(ent->v.velocity[2] < 60 || ent->v.movetype != MOVETYPE_BOUNCE)
        {
            ent->v.flags = (int)ent->v.flags | FL_ONGROUND;
            ent->v.groundentity = EDICT_TO_PROG(trace.ent);
            ent->v.velocity = vec3_zero;
            ent->v.avelocity = vec3_zero;
        }
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
    if(!((int)ent->v.flags & (FL_ONGROUND | FL_FLY | FL_SWIM)))
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

        if((int)ent->v.flags & FL_ONGROUND) // just hit ground
        {
            if(hitsound)
            {
                SV_StartSound(ent, 0, "demon/dland2.wav", 255, 1);
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
    pr_global_struct->self = EDICT_TO_PROG(sv.edicts);
    pr_global_struct->other = EDICT_TO_PROG(sv.edicts);
    pr_global_struct->time = sv.time;
    PR_ExecuteProgram(pr_global_struct->StartFrame);

    // SV_CheckAllEnts ();

    //
    // treat each object in turn
    //
    ent = sv.edicts;

    if(sv_freezenonclients.value)
    {
        entity_cap =
            svs.maxclients + 1; // Only run physics on clients and the world
    }
    else
    {
        entity_cap = sv.num_edicts;
    }

    // for (i=0 ; i<sv.num_edicts ; i++, ent = NEXT_EDICT(ent))
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
        sv.time += host_frametime;
    }
}
