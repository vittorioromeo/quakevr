/*
Copyright (C) 1996-2001 Id Software, Inc.
Copyright (C) 2002-2009 John Fitzgibbons and others
Copyright (C) 2010-2014 QuakeSpasm developers

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
// chase.c -- chase camera code

#include "quakedef.hpp"

cvar_t chase_back = {"chase_back", "100", CVAR_NONE};
cvar_t chase_up = {"chase_up", "16", CVAR_NONE};
cvar_t chase_right = {"chase_right", "0", CVAR_NONE};
cvar_t chase_active = {"chase_active", "0", CVAR_NONE};

/*
==============
Chase_Init
==============
*/
void Chase_Init()
{
    Cvar_RegisterVariable(&chase_back);
    Cvar_RegisterVariable(&chase_up);
    Cvar_RegisterVariable(&chase_right);
    Cvar_RegisterVariable(&chase_active);
}

/*
==============
TraceLine

TODO: impact on bmodels, monsters
==============
*/
trace_t TraceLine(vec3_t start, vec3_t end, vec3_t impact)
{
    trace_t trace;
    memset(&trace, 0, sizeof(trace));

    SV_RecursiveHullCheck(cl.worldmodel->hulls, 0, 0, 1, start, end, &trace);
    VectorCopy(trace.endpos, impact);

    return trace;
}

trace_t TraceLineToEntity(vec3_t start, vec3_t end, vec3_t impact, edict_t* ent)
{
    trace_t trace;
    memset(&trace, 0, sizeof(trace));

    trace = SV_Move(start, vec3_origin, vec3_origin, end, MOVE_NORMAL, ent);
    VectorCopy(trace.endpos, impact);

    return trace;
}

/*
==============
Chase_UpdateForClient -- johnfitz -- orient client based on camera. called after
input
==============
*/
void Chase_UpdateForClient()
{
    // place camera

    // assign client angles to camera

    // see where camera points

    // adjust client angles to point at the same place
}

/*
==============
Chase_UpdateForDrawing -- johnfitz -- orient camera based on client. called
before drawing

TODO: stay at least 8 units away from all walls in this leaf
==============
*/
void Chase_UpdateForDrawing(refdef_t& refdef, entity_t* viewent)
{
    vec3_t forward, right, up;
    AngleVectors(cl.viewangles, forward, right, up);

    // calc ideal camera location before checking for walls
    vec3_t ideal;
    for(int i = 0; i < 3; i++)
    {
        ideal[i] = viewent->origin[i] - forward[i] * chase_back.value +
                   right[i] * chase_right.value;
    }
    //+ up[i]*chase_up.value;
    ideal[2] = viewent->origin[2] + chase_up.value;

    // make sure camera is not in or behind a wall
    vec3_t temp;
    TraceLine(refdef.vieworg, ideal, temp);
    if(VectorLength(temp) != 0) VectorCopy(temp, ideal);

    // place camera
    VectorCopy(ideal, refdef.vieworg);

    // find the spot the player is looking at
    VectorMA(viewent->origin, 4096, forward, temp);

    vec3_t crosshair;
    TraceLine(viewent->origin, temp, crosshair);

    // calculate camera angles to look at the same spot
    VectorSubtract(crosshair, refdef.vieworg, temp);
    VectorAngles(temp, refdef.viewangles);
    if(refdef.viewangles[PITCH] == 90 || refdef.viewangles[PITCH] == -90)
    {
        refdef.viewangles[YAW] = cl.viewangles[YAW];
    }
}
