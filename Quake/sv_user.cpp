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
// sv_user.c -- server code for moving users

#include "quakedef.hpp"
#include <iostream>

edict_t* sv_player;

extern cvar_t sv_friction;
cvar_t sv_edgefriction = {"edgefriction", "2", CVAR_NONE};
extern cvar_t sv_stopspeed;

static vec3_t forward, right, up;

// world
float* angles;
float* origin;
float* velocity;

qboolean onground;

usercmd_t cmd;

cvar_t sv_idealpitchscale = {"sv_idealpitchscale", "0.8", CVAR_NONE};
cvar_t sv_altnoclip = {"sv_altnoclip", "1", CVAR_ARCHIVE}; // johnfitz

/*
===============
SV_SetIdealPitch
===============
*/
#define MAX_FORWARD 6
void SV_SetIdealPitch()
{
    float angleval;

    float sinval;

    float cosval;
    trace_t tr;
    vec3_t top;

    vec3_t bottom;
    float z[MAX_FORWARD];
    int i;

    int j;
    int step;

    int dir;

    int steps;

    if(!((int)sv_player->v.flags & FL_ONGROUND))
    {
        return;
    }

    angleval = sv_player->v.angles[YAW] * M_PI * 2 / 360;
    sinval = sin(angleval);
    cosval = cos(angleval);

    for(i = 0; i < MAX_FORWARD; i++)
    {
        top[0] = sv_player->v.origin[0] + cosval * (i + 3) * 12;
        top[1] = sv_player->v.origin[1] + sinval * (i + 3) * 12;
        top[2] = sv_player->v.origin[2] + sv_player->v.view_ofs[2];

        bottom[0] = top[0];
        bottom[1] = top[1];
        bottom[2] = top[2] - 160;

        tr = SV_Move(top, vec3_origin, vec3_origin, bottom, 1, sv_player);
        if(tr.allsolid)
        {
            return; // looking at a wall, leave ideal the way is was
        }

        if(tr.fraction == 1)
        {
            return; // near a dropoff
        }

        z[i] = top[2] + tr.fraction * (bottom[2] - top[2]);
    }

    dir = 0;
    steps = 0;
    for(j = 1; j < i; j++)
    {
        step = z[j] - z[j - 1];
        if(step > -ON_EPSILON && step < ON_EPSILON)
        {
            continue;
        }

        if(dir && (step - dir > ON_EPSILON || step - dir < -ON_EPSILON))
        {
            return; // mixed changes
        }

        steps++;
        dir = step;
    }

    if(!dir)
    {
        sv_player->v.idealpitch = 0;
        return;
    }

    if(steps < 2)
    {
        return;
    }
    sv_player->v.idealpitch = -dir * sv_idealpitchscale.value;
}


/*
==================
SV_UserFriction

==================
*/
void SV_UserFriction()
{
    float* vel;
    float speed;

    float newspeed;

    float control;
    vec3_t start;

    vec3_t stop;
    float friction;
    trace_t trace;

    vel = velocity;

    speed = sqrt(vel[0] * vel[0] + vel[1] * vel[1]);
    if(!speed)
    {
        return;
    }

    // if the leading edge is over a dropoff, increase friction
    start[0] = stop[0] = origin[0] + vel[0] / speed * 16;
    start[1] = stop[1] = origin[1] + vel[1] / speed * 16;
    start[2] = origin[2] + sv_player->v.mins[2];
    stop[2] = start[2] - 34;

    trace = SV_Move(start, vec3_origin, vec3_origin, stop, true, sv_player);

    if(trace.fraction == 1.0)
    {
        friction = sv_friction.value * sv_edgefriction.value;
    }
    else
    {
        friction = sv_friction.value;
    }

    // apply friction
    control = speed < sv_stopspeed.value ? sv_stopspeed.value : speed;
    newspeed = speed - host_frametime * control * friction;

    if(newspeed < 0)
    {
        newspeed = 0;
    }
    newspeed /= speed;

    vel[0] = vel[0] * newspeed;
    vel[1] = vel[1] * newspeed;
    vel[2] = vel[2] * newspeed;
}

/*
==============
SV_Accelerate
==============
*/
cvar_t sv_maxspeed = {"sv_maxspeed", "320", CVAR_NOTIFY | CVAR_SERVERINFO};
cvar_t sv_accelerate = {"sv_accelerate", "10", CVAR_NONE};
void SV_Accelerate(float wishspeed, const vec3_t wishdir)
{
    int i;
    float addspeed;

    float accelspeed;

    float currentspeed;

    currentspeed = DotProduct(velocity, wishdir);
    addspeed = wishspeed - currentspeed;
    if(addspeed <= 0)
    {
        return;
    }
    accelspeed = sv_accelerate.value * host_frametime * wishspeed;
    if(accelspeed > addspeed)
    {
        accelspeed = addspeed;
    }

    for(i = 0; i < 3; i++)
    {
        velocity[i] += accelspeed * wishdir[i];
    }
}

void SV_AirAccelerate(float wishspeed, vec3_t wishveloc)
{
    int i;
    float addspeed;

    float wishspd;

    float accelspeed;

    float currentspeed;

    wishspd = VectorNormalize(wishveloc);
    if(wishspd > 30)
    {
        wishspd = 30;
    }
    currentspeed = DotProduct(velocity, wishveloc);
    addspeed = wishspd - currentspeed;
    if(addspeed <= 0)
    {
        return;
    }
    //	accelspeed = sv_accelerate.value * host_frametime;
    accelspeed = sv_accelerate.value * wishspeed * host_frametime;
    if(accelspeed > addspeed)
    {
        accelspeed = addspeed;
    }

    for(i = 0; i < 3; i++)
    {
        velocity[i] += accelspeed * wishveloc[i];
    }
}


void DropPunchAngle()
{
    float len;

    len = VectorNormalize(sv_player->v.punchangle);

    len -= 10 * host_frametime;
    if(len < 0)
    {
        len = 0;
    }
    VectorScale(sv_player->v.punchangle, len, sv_player->v.punchangle);
}

/*
===================
SV_WaterMove

===================
*/
void SV_WaterMove()
{
    int i;
    vec3_t wishvel;
    float speed;

    float newspeed;

    float wishspeed;

    float addspeed;

    float accelspeed;

    //
    // user intentions
    //
    AngleVectors(sv_player->v.v_angle, forward, right, up);

    for(i = 0; i < 3; i++)
    {
        wishvel[i] = forward[i] * cmd.forwardmove + right[i] * cmd.sidemove;
    }

    if(!cmd.forwardmove && !cmd.sidemove && !cmd.upmove)
    {
        wishvel[2] -= 60; // drift towards bottom
    }
    else
    {
        wishvel[2] += cmd.upmove;
    }

    wishspeed = VectorLength(wishvel);
    if(wishspeed > sv_maxspeed.value)
    {
        VectorScale(wishvel, sv_maxspeed.value / wishspeed, wishvel);
        wishspeed = sv_maxspeed.value;
    }
    wishspeed *= 0.7;

    //
    // water friction
    //
    speed = VectorLength(velocity);
    if(speed)
    {
        newspeed = speed - host_frametime * speed * sv_friction.value;
        if(newspeed < 0)
        {
            newspeed = 0;
        }
        VectorScale(velocity, newspeed / speed, velocity);
    }
    else
    {
        newspeed = 0;
    }

    //
    // water acceleration
    //
    if(!wishspeed)
    {
        return;
    }

    addspeed = wishspeed - newspeed;
    if(addspeed <= 0)
    {
        return;
    }

    VectorNormalize(wishvel);
    accelspeed = sv_accelerate.value * wishspeed * host_frametime;
    if(accelspeed > addspeed)
    {
        accelspeed = addspeed;
    }

    for(i = 0; i < 3; i++)
    {
        velocity[i] += accelspeed * wishvel[i];
    }
}

void SV_WaterJump()
{
    if(sv.time > sv_player->v.teleport_time || !sv_player->v.waterlevel)
    {
        sv_player->v.flags = (int)sv_player->v.flags & ~FL_WATERJUMP;
        sv_player->v.teleport_time = 0;
    }
    sv_player->v.velocity[0] = sv_player->v.movedir[0];
    sv_player->v.velocity[1] = sv_player->v.movedir[1];
}

/*
===================
SV_NoclipMove -- johnfitz

new, alternate noclip. old noclip is still handled in SV_AirMove
===================
*/
void SV_NoclipMove()
{
    AngleVectors(sv_player->v.v_angle, forward, right, up);

    velocity[0] = forward[0] * cmd.forwardmove + right[0] * cmd.sidemove;
    velocity[1] = forward[1] * cmd.forwardmove + right[1] * cmd.sidemove;
    velocity[2] = forward[2] * cmd.forwardmove + right[2] * cmd.sidemove;
    velocity[2] += cmd.upmove * 2; // doubled to match running speed

    if(VectorLength(velocity) > sv_maxspeed.value)
    {
        VectorNormalize(velocity);
        VectorScale(velocity, sv_maxspeed.value, velocity);
    }
}

/*
===================
SV_AirMove
===================
*/
void SV_AirMove()
{
    int i;
    vec3_t wishvel;

    vec3_t wishdir;
    float wishspeed;
    float fmove;

    float smove;

    AngleVectors(sv_player->v.v_viewangle, forward, right, up);

    fmove = cmd.forwardmove;
    smove = cmd.sidemove;

    // hack to not let you back into teleporter
    if(sv.time < sv_player->v.teleport_time && fmove < 0)
    {
        fmove = 0;
    }

    for(i = 0; i < 3; i++)
    {
        wishvel[i] = forward[i] * fmove + right[i] * smove;
    }

    if((int)sv_player->v.movetype != MOVETYPE_WALK)
    {
        wishvel[2] = cmd.upmove;
    }
    else
    {
        wishvel[2] = 0;
    }

    VectorCopy(wishvel, wishdir);
    wishspeed = VectorNormalize(wishdir);
    if(wishspeed > sv_maxspeed.value)
    {
        VectorScale(wishvel, sv_maxspeed.value / wishspeed, wishvel);
        wishspeed = sv_maxspeed.value;
    }

    if(sv_player->v.movetype == MOVETYPE_NOCLIP)
    { // noclip
        VectorCopy(wishvel, velocity);
    }
    else if(onground)
    {
        SV_UserFriction();
        SV_Accelerate(wishspeed, wishdir);
    }
    else
    { // not on ground, so little effect on velocity
        SV_AirAccelerate(wishspeed, wishvel);
    }
}

/*
===================
SV_ClientThink

the move fields specify an intended velocity in pix/sec
the angle fields specify an exact angular motion in degrees
===================
*/
void SV_ClientThink()
{
    vec3_t v_angle;

    if(sv_player->v.movetype == MOVETYPE_NONE)
    {
        return;
    }

    onground = (int)sv_player->v.flags & FL_ONGROUND;

    origin = sv_player->v.origin;
    velocity = sv_player->v.velocity;

    DropPunchAngle();

    //
    // if dead, behave differently
    //
    if(sv_player->v.health <= 0)
    {
        return;
    }

    //
    // angles
    // show 1/3 the pitch angle and all the roll angle
    cmd = host_client->cmd;
    angles = sv_player->v.angles;

    VectorAdd(sv_player->v.v_angle, sv_player->v.punchangle, v_angle);
    angles[ROLL] = V_CalcRoll(sv_player->v.angles, sv_player->v.velocity) * 4;
    if(!sv_player->v.fixangle)
    {
        angles[PITCH] = -v_angle[PITCH] / 3;
        angles[YAW] = v_angle[YAW];
    }

    if((int)sv_player->v.flags & FL_WATERJUMP)
    {
        SV_WaterJump();
        return;
    }
    //
    // walk
    //
    // johnfitz -- alternate noclip
    if(sv_player->v.movetype == MOVETYPE_NOCLIP && sv_altnoclip.value)
    {
        SV_NoclipMove();
    }
    else if(sv_player->v.waterlevel >= 2 &&
            sv_player->v.movetype != MOVETYPE_NOCLIP)
    {
        SV_WaterMove();
    }
    else
    {
        SV_AirMove();
    }
    // johnfitz
}

/*
===================
SV_ReadClientMove
===================
*/
void SV_ReadClientMove(usercmd_t* move)
{
    int i;
    int bits;

    // read ping time
    host_client->ping_times[host_client->num_pings % NUM_PING_TIMES] =
        sv.time - MSG_ReadFloat();
    host_client->num_pings++;

    const auto readAngles = [&](auto& target) {
        // read current angles
        for(int i = 0; i < 3; i++)
        {
            // johnfitz -- 16-bit angles for PROTOCOL_FITZQUAKE
            if(sv.protocol == PROTOCOL_NETQUAKE)
            {
                target[i] = MSG_ReadAngle(sv.protocolflags);
            }
            else
            {
                target[i] = MSG_ReadAngle16(sv.protocolflags);
            }
            // johnfitz
        }
    };

    const auto readVec = [&](auto& target) {
        target[0] = MSG_ReadFloat();
        target[1] = MSG_ReadFloat();
        target[2] = MSG_ReadFloat();
    };

    // aimangles
    readAngles(host_client->edict->v.v_angle);

    // viewangles
    readAngles(host_client->edict->v.v_viewangle);

    // main hand: handpos, handrot, handvel, handvelmag
    // handpos
    readVec(move->handpos);
    VectorCopy(move->handpos, host_client->edict->v.handpos);

    // handrot
    readVec(move->handrot);
    VectorCopy(move->handrot, host_client->edict->v.handrot);

    // handvel
    readVec(move->handvel);
    VectorCopy(move->handvel, host_client->edict->v.handvel);

    // handvelmag
    move->handvelmag = MSG_ReadFloat();
    host_client->edict->v.handvelmag = move->handvelmag;

    // off hand: offhandpos, offhandrot, offhandvel, offhandvelmag
    // offhandpos
    readVec(move->offhandpos);
    VectorCopy(move->offhandpos, host_client->edict->v.offhandpos);

    // offhandrot
    readVec(move->offhandrot);
    VectorCopy(move->offhandrot, host_client->edict->v.offhandrot);

    // offhandvel
    readVec(move->offhandvel);
    VectorCopy(move->offhandvel, host_client->edict->v.offhandvel);

    // offhandvelmag
    move->offhandvelmag = MSG_ReadFloat();
    host_client->edict->v.offhandvelmag = move->offhandvelmag;

    // read movement
    move->forwardmove = MSG_ReadShort();
    move->sidemove = MSG_ReadShort();
    move->upmove = MSG_ReadShort();

    // read buttons
    bits = MSG_ReadByte();
    host_client->edict->v.button0 = bits & 1;
    host_client->edict->v.button2 = (bits & 2) >> 1;

    i = MSG_ReadByte();
    if(i)
    {
        host_client->edict->v.impulse = i;
    }
}

/*
===================
SV_ReadClientMessage

Returns false if the client should be killed
===================
*/
qboolean SV_ReadClientMessage()
{
    int ret;
    int ccmd;
    const char* s;

    do
    {
    nextmsg:
        ret = NET_GetMessage(host_client->netconnection);
        if(ret == -1)
        {
            Sys_Printf("SV_ReadClientMessage: NET_GetMessage failed\n");
            return false;
        }
        if(!ret)
        {
            return true;
        }

        MSG_BeginReading();

        while(true)
        {
            if(!host_client->active)
            {
                return false; // a command caused an error
            }

            if(msg_badread)
            {
                Sys_Printf("SV_ReadClientMessage: badread\n");
                return false;
            }

            ccmd = MSG_ReadChar();

            switch(ccmd)
            {
                case -1: goto nextmsg; // end of message

                default:
                    Sys_Printf("SV_ReadClientMessage: unknown command char\n");
                    return false;

                case clc_nop:
                    //				Sys_Printf ("clc_nop\n");
                    break;

                case clc_stringcmd:
                    s = MSG_ReadString();
                    ret = 0;
                    if(q_strncasecmp(s, "status", 6) == 0)
                    {
                        ret = 1;
                    }
                    else if(q_strncasecmp(s, "god", 3) == 0)
                    {
                        ret = 1;
                    }
                    else if(q_strncasecmp(s, "notarget", 8) == 0)
                    {
                        ret = 1;
                    }
                    else if(q_strncasecmp(s, "fly", 3) == 0)
                    {
                        ret = 1;
                    }
                    else if(q_strncasecmp(s, "name", 4) == 0)
                    {
                        ret = 1;
                    }
                    else if(q_strncasecmp(s, "noclip", 6) == 0)
                    {
                        ret = 1;
                    }
                    else if(q_strncasecmp(s, "setpos", 6) == 0)
                    {
                        ret = 1;
                    }
                    else if(q_strncasecmp(s, "say", 3) == 0)
                    {
                        ret = 1;
                    }
                    else if(q_strncasecmp(s, "say_team", 8) == 0)
                    {
                        ret = 1;
                    }
                    else if(q_strncasecmp(s, "tell", 4) == 0)
                    {
                        ret = 1;
                    }
                    else if(q_strncasecmp(s, "color", 5) == 0)
                    {
                        ret = 1;
                    }
                    else if(q_strncasecmp(s, "kill", 4) == 0)
                    {
                        ret = 1;
                    }
                    else if(q_strncasecmp(s, "pause", 5) == 0)
                    {
                        ret = 1;
                    }
                    else if(q_strncasecmp(s, "spawn", 5) == 0)
                    {
                        ret = 1;
                    }
                    else if(q_strncasecmp(s, "begin", 5) == 0)
                    {
                        ret = 1;
                    }
                    else if(q_strncasecmp(s, "prespawn", 8) == 0)
                    {
                        ret = 1;
                    }
                    else if(q_strncasecmp(s, "kick", 4) == 0)
                    {
                        ret = 1;
                    }
                    else if(q_strncasecmp(s, "ping", 4) == 0)
                    {
                        ret = 1;
                    }
                    else if(q_strncasecmp(s, "give", 4) == 0)
                    {
                        ret = 1;
                    }
                    else if(q_strncasecmp(s, "ban", 3) == 0)
                    {
                        ret = 1;
                    }

                    if(ret == 1)
                    {
                        Cmd_ExecuteString(s, src_client);
                    }
                    else
                    {
                        Con_DPrintf("%s tried to %s\n", host_client->name, s);
                    }
                    break;

                case clc_disconnect:
                    //				Sys_Printf ("SV_ReadClientMessage: client
                    // disconnected\n");
                    return false;

                case clc_move: SV_ReadClientMove(&host_client->cmd); break;
            }
        }
    } while(ret == 1);

    return true;
}


/*
==================
SV_RunClients
==================
*/
void SV_RunClients()
{
    int i;

    for(i = 0, host_client = svs.clients; i < svs.maxclients;
        i++, host_client++)
    {
        if(!host_client->active)
        {
            continue;
        }

        sv_player = host_client->edict;

        if(!SV_ReadClientMessage())
        {
            SV_DropClient(false); // client misbehaved...
            continue;
        }

        if(!host_client->spawned)
        {
            // clear client movement until a new packet is received
            memset(&host_client->cmd, 0, sizeof(host_client->cmd));
            continue;
        }

        // always pause in single player if in console or menus
        if(!sv.paused && (svs.maxclients > 1 || key_dest == key_game))
        {
            SV_ClientThink();
        }
    }
}
