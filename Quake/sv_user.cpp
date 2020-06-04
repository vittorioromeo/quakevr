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
// sv_user.c -- server code for moving users

#include "quakedef.hpp"
#include "vr.hpp"
#include "world.hpp"
#include "util.hpp"
#include "q_stdinc.hpp"
#include "net.hpp"
#include "keys.hpp"
#include "msg.hpp"
#include "sys.hpp"
#include "console.hpp"
#include "vid.hpp"
#include "draw.hpp"
#include "screen.hpp"
#include "view.hpp"
#include "cmd.hpp"

#include <iostream>

edict_t* sv_player{nullptr};

extern cvar_t sv_friction;
cvar_t sv_edgefriction = {"edgefriction", "2", CVAR_NONE};
extern cvar_t sv_stopspeed;

static qvec3 forward, right, up;

// world
qvec3* origin{nullptr};
qvec3* velocity{nullptr};

bool onground;

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
    qvec3 top;

    qvec3 bottom;
    float z[MAX_FORWARD];
    int i;

    int j;
    int step;

    int dir;

    int steps;

    if(!quake::util::hasFlag(sv_player, FL_ONGROUND))
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

        tr = SV_MoveTrace(top, bottom, 1, sv_player);
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
    qvec3* vel;
    float speed;

    float newspeed;

    float control;
    qvec3 start;

    qvec3 stop;
    float friction;
    trace_t trace;

    vel = velocity;

    speed = sqrt((*vel)[0] * (*vel)[0] + (*vel)[1] * (*vel)[1]);
    if(!speed)
    {
        return;
    }

    // if the leading edge is over a dropoff, increase friction
    start[0] = stop[0] = (*origin)[0] + (*vel)[0] / speed * 16;
    start[1] = stop[1] = (*origin)[1] + (*vel)[1] / speed * 16;
    start[2] = (*origin)[2] + sv_player->v.mins[2];
    stop[2] = start[2] - 34;

    trace = SV_MoveTrace(start, stop, true, sv_player);

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
    (*vel) *= newspeed;
}

/*
==============
SV_Accelerate
==============
*/
cvar_t sv_maxspeed = {"sv_maxspeed", "320", CVAR_NOTIFY | CVAR_SERVERINFO};
cvar_t sv_accelerate = {"sv_accelerate", "10", CVAR_NONE};

void SV_Accelerate(float wishspeed, const qvec3& wishdir)
{
    const float currentspeed = DotProduct((*velocity), wishdir);
    const float addspeed = wishspeed - currentspeed;

    if(addspeed <= 0)
    {
        return;
    }

    float accelspeed = sv_accelerate.value * host_frametime * wishspeed;
    if(accelspeed > addspeed)
    {
        accelspeed = addspeed;
    }

    for(int i = 0; i < 3; i++)
    {
        (*velocity)[i] += accelspeed * wishdir[i];
    }
}

void SV_AirAccelerate(float wishspeed, const qvec3& wishveloc)
{
    float wishspd = glm::length(wishveloc);

    if(wishspd > 30)
    {
        wishspd = 30;
    }

    const auto wishvelocdir = safeNormalize(wishveloc);
    const float currentspeed = DotProduct((*velocity), wishvelocdir);
    const float addspeed = wishspd - currentspeed;

    if(addspeed <= 0)
    {
        return;
    }

    float accelspeed = sv_accelerate.value * wishspeed * host_frametime;
    if(accelspeed > addspeed)
    {
        accelspeed = addspeed;
    }

    for(int i = 0; i < 3; i++)
    {
        (*velocity)[i] += accelspeed * wishvelocdir[i];
    }
}


void DropPunchAngle()
{
    float len = glm::length(sv_player->v.punchangle);
    sv_player->v.punchangle = safeNormalize(sv_player->v.punchangle);

    len -= 10 * host_frametime;
    if(len < 0)
    {
        len = 0;
    }

    sv_player->v.punchangle *= len;
}

/*
===================
SV_WaterMove

===================
*/
void SV_WaterMove()
{
    int i;
    qvec3 wishvel;
    float speed;

    float newspeed;

    float wishspeed;

    float addspeed;

    float accelspeed;

    //
    // user intentions
    //

    // TODO VR: (P1) this should probably change depending on the chosen
    // locomotion style,
    std::tie(forward, right, up) =
        quake::util::getAngledVectors(sv_player->v.v_viewangle);

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

    wishspeed = glm::length(wishvel);
    if(wishspeed > sv_maxspeed.value)
    {
        wishvel *= sv_maxspeed.value / wishspeed;
        wishspeed = sv_maxspeed.value;
    }
    wishspeed *= 0.7;

    //
    // water friction
    //
    speed = glm::length(*velocity);
    if(speed)
    {
        newspeed = speed - host_frametime * speed * sv_friction.value;
        if(newspeed < 0)
        {
            newspeed = 0;
        }

        *velocity *= newspeed / speed;
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

    wishvel = safeNormalize(wishvel);
    accelspeed = sv_accelerate.value * wishspeed * host_frametime;
    if(accelspeed > addspeed)
    {
        accelspeed = addspeed;
    }

    for(i = 0; i < 3; i++)
    {
        (*velocity)[i] += accelspeed * wishvel[i];
    }
}

void SV_WaterJump()
{
    if(sv.time > sv_player->v.teleport_time || !sv_player->v.waterlevel)
    {
        quake::util::removeFlag(sv_player, FL_WATERJUMP);
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
    // TODO VR: (P1) this should probably change depending on the chosen
    // locomotion style
    std::tie(forward, right, up) =
        quake::util::getAngledVectors(sv_player->v.v_angle);

    (*velocity)[0] = forward[0] * cmd.forwardmove + right[0] * cmd.sidemove;
    (*velocity)[1] = forward[1] * cmd.forwardmove + right[1] * cmd.sidemove;
    (*velocity)[2] = forward[2] * cmd.forwardmove + right[2] * cmd.sidemove;
    (*velocity)[2] += cmd.upmove * 2; // doubled to match running speed

    if(glm::length(*velocity) > sv_maxspeed.value)
    {
        *velocity = safeNormalize(*velocity);
        *velocity *= sv_maxspeed.value;
    }
}

/*
===================
SV_AirMove
===================
*/
void SV_AirMove()
{
    // TODO VR: (P1) this should probably change depending on the chosen
    // locomotion style
    std::tie(forward, right, up) =
        quake::util::getAngledVectors(sv_player->v.v_viewangle);

    qfloat fmove = cmd.forwardmove;
    const qfloat smove = cmd.sidemove;

    // hack to not let you back into teleporter
    if(sv.time < sv_player->v.teleport_time && fmove < 0)
    {
        fmove = 0;
    }

    qvec3 wishvel = forward * fmove + right * smove;

    if((int)sv_player->v.movetype != MOVETYPE_WALK)
    {
        wishvel[2] = cmd.upmove;
    }
    else
    {
        wishvel[2] = 0;
    }

    float wishspeed = glm::length(wishvel);
    const auto wishdir = safeNormalize(wishvel);
    if(wishspeed > sv_maxspeed.value)
    {
        wishvel *= sv_maxspeed.value / wishspeed;
        wishspeed = sv_maxspeed.value;
    }

    if(sv_player->v.movetype == MOVETYPE_NOCLIP)
    {
        // noclip
        *velocity = wishvel;
    }
    else if(onground)
    {
        SV_UserFriction();
        SV_Accelerate(wishspeed, wishdir);
    }
    else
    {
        // not on ground, so little effect on velocity
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
    if(sv_player->v.movetype == MOVETYPE_NONE)
    {
        return;
    }

    onground = quake::util::hasFlag(sv_player, FL_ONGROUND);

    origin = &sv_player->v.origin;
    velocity = &sv_player->v.velocity;

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

    qvec3 v_angle;
    v_angle = sv_player->v.v_angle + sv_player->v.punchangle;
    sv_player->v.angles[ROLL] =
        V_CalcRoll(sv_player->v.angles, sv_player->v.velocity) * 4;
    if(!sv_player->v.fixangle)
    {
        sv_player->v.angles[PITCH] = -v_angle[PITCH] / 3;
        sv_player->v.angles[YAW] = v_angle[YAW];
    }

    if(quake::util::hasFlag(sv_player, FL_WATERJUMP))
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

    const auto readVec = [&] { return MSG_ReadVec3(sv.protocolflags); };

    // aimangles
    readAngles(host_client->edict->v.v_angle);

    // viewangles
    readAngles(host_client->edict->v.v_viewangle);

    // vr yaw
    host_client->edict->v.vryaw = move->vryaw = MSG_ReadFloat();

    // ------------------------------------------------------------------------
    // main hand values:
    host_client->edict->v.handpos = move->handpos = readVec();
    host_client->edict->v.handrot = move->handrot = readVec();
    host_client->edict->v.handvel = move->handvel = readVec();
    host_client->edict->v.handthrowvel = move->handthrowvel = readVec();
    host_client->edict->v.handvelmag = move->handvelmag = MSG_ReadFloat();
    host_client->edict->v.handavel = move->handavel = readVec();
    // ------------------------------------------------------------------------

    // ------------------------------------------------------------------------
    // off hand values:
    host_client->edict->v.offhandpos = move->offhandpos = readVec();
    host_client->edict->v.offhandrot = move->offhandrot = readVec();
    host_client->edict->v.offhandvel = move->offhandvel = readVec();
    host_client->edict->v.offhandthrowvel = move->offhandthrowvel = readVec();
    host_client->edict->v.offhandvelmag = move->offhandvelmag = MSG_ReadFloat();
    host_client->edict->v.offhandavel = move->offhandavel = readVec();
    // ------------------------------------------------------------------------

    // headvel
    host_client->edict->v.headvel = move->headvel = readVec();

    // muzzlepos
    host_client->edict->v.muzzlepos = move->muzzlepos = readVec();

    // offmuzzlepos
    host_client->edict->v.offmuzzlepos = move->offmuzzlepos = readVec();

    // vrbits
    host_client->edict->v.vrbits0 = move->vrbits0 = MSG_ReadUnsignedChar();

    // movement
    move->forwardmove = MSG_ReadShort();
    move->sidemove = MSG_ReadShort();
    move->upmove = MSG_ReadShort();

    // teleportation
    host_client->edict->v.teleport_target = move->teleport_target = readVec();

    // hands
    host_client->edict->v.offhand_hotspot = move->offhand_hotspot =
        MSG_ReadByte();
    host_client->edict->v.mainhand_hotspot = move->mainhand_hotspot =
        MSG_ReadByte();

    // roomscalemove
    host_client->edict->v.roomscalemove = move->roomscalemove = readVec();

    // read buttons
    bits = MSG_ReadByte();
    host_client->edict->v.button0 = bits & 1;
    host_client->edict->v.button2 = (bits & 2) >> 1;
    host_client->edict->v.button3 = (bits & 4) >> 2;

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
bool SV_ReadClientMessage()
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
            Sys_Printf(
                "SV_ReadClientMessage: NET_GetMessage failed. rc= %d\n", ret);
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
