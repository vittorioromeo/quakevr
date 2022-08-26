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
// cl.input.c  -- builds an intended movement command to send to the server

// Quake is a trademark of Id Software, Inc., (c) 1996 Id Software, Inc. All
// rights reserved.

#include "quakedef.hpp"
#include "cmd.hpp"
#include "console.hpp"
#include "net.hpp"
#include "mathlib.hpp"
#include "msg.hpp"
#include "input.hpp"
#include "client.hpp"
#include "snd_voip.hpp"

extern cvar_t cl_maxpitch; // johnfitz -- variable pitch clamping
extern cvar_t cl_minpitch; // johnfitz -- variable pitch clamping

/*
===============================================================================

KEY BUTTONS

Continuous button event tracking is complicated by the fact that two different
input sources (say, mouse button 1 and the control key) can both press the
same button, but the button should only be released when both of the
pressing key have been released.

When a key event issues a button command (+forward, +attack, etc), it appends
its key number as a parameter to the command so it can be matched up with
the release.

state bit 0 is the current state of the key
state bit 1 is edge triggered on the up to down transition
state bit 2 is edge triggered on the down to up transition

===============================================================================
*/


kbutton_t in_mlook, in_klook;
kbutton_t in_left, in_right, in_forward, in_back;
kbutton_t in_lookup, in_lookdown, in_moveleft, in_moveright;
kbutton_t in_strafe, in_speed, in_use, in_jump, in_attack, in_offhandattack,
    in_button3, in_button4, in_button5, in_button6, in_button7, in_button8;
kbutton_t in_up, in_down;
kbutton_t in_grableft, in_grabright;
kbutton_t in_reloadleft, in_reloadright;

int in_impulse;


void KeyDown(kbutton_t* b)
{
    int k;
    const char* c;

    c = Cmd_Argv(1);
    if(c[0])
    {
        k = atoi(c);
    }
    else
    {
        k = -1; // typed manually at the console for continuous down
    }

    if(k == b->down[0] || k == b->down[1])
    {
        return; // repeating key
    }

    if(!b->down[0])
    {
        b->down[0] = k;
    }
    else if(!b->down[1])
    {
        b->down[1] = k;
    }
    else
    {
        Con_Printf("Three keys down for a button!\n");
        return;
    }

    if(b->state & 1)
    {
        return; // still down
    }
    b->state |= 1 + 2; // down + impulse down
}

void KeyUp(kbutton_t* b)
{
    int k;
    const char* c;

    c = Cmd_Argv(1);
    if(c[0])
    {
        k = atoi(c);
    }
    else
    {
        // typed manually at the console, assume for unsticking, so clear all
        b->down[0] = b->down[1] = 0;
        b->state = 4; // impulse up
        return;
    }

    if(b->down[0] == k)
    {
        b->down[0] = 0;
    }
    else if(b->down[1] == k)
    {
        b->down[1] = 0;
    }
    else
    {
        return; // key up without coresponding down (menu pass through)
    }
    if(b->down[0] || b->down[1])
    {
        return; // some other key is still holding it down
    }

    if(!(b->state & 1))
    {
        return; // still up (this should not happen)
    }
    b->state &= ~1; // now up
    b->state |= 4;  // impulse up
}

void IN_KLookDown()
{
    KeyDown(&in_klook);
}
void IN_KLookUp()
{
    KeyUp(&in_klook);
}
void IN_MLookDown()
{
    KeyDown(&in_mlook);
}
void IN_MLookUp()
{
    KeyUp(&in_mlook);
    if(!(in_mlook.state & 1) && lookspring.value)
    {
        V_StartPitchDrift();
    }
}
void IN_UpDown()
{
    KeyDown(&in_up);
}
void IN_UpUp()
{
    KeyUp(&in_up);
}
void IN_DownDown()
{
    KeyDown(&in_down);
}
void IN_DownUp()
{
    KeyUp(&in_down);
}
void IN_LeftDown()
{
    KeyDown(&in_left);
}
void IN_LeftUp()
{
    KeyUp(&in_left);
}
void IN_RightDown()
{
    KeyDown(&in_right);
}
void IN_RightUp()
{
    KeyUp(&in_right);
}
void IN_ForwardDown()
{
    KeyDown(&in_forward);
}
void IN_ForwardUp()
{
    KeyUp(&in_forward);
}
void IN_BackDown()
{
    KeyDown(&in_back);
}
void IN_BackUp()
{
    KeyUp(&in_back);
}
void IN_LookupDown()
{
    KeyDown(&in_lookup);
}
void IN_LookupUp()
{
    KeyUp(&in_lookup);
}
void IN_LookdownDown()
{
    KeyDown(&in_lookdown);
}
void IN_LookdownUp()
{
    KeyUp(&in_lookdown);
}
void IN_MoveleftDown()
{
    KeyDown(&in_moveleft);
}
void IN_MoveleftUp()
{
    KeyUp(&in_moveleft);
}
void IN_MoverightDown()
{
    KeyDown(&in_moveright);
}
void IN_MoverightUp()
{
    KeyUp(&in_moveright);
}


void IN_SpeedDown()
{
    KeyDown(&in_speed);
}
void IN_SpeedUp()
{
    KeyUp(&in_speed);
}
void IN_StrafeDown()
{
    KeyDown(&in_strafe);
}
void IN_StrafeUp()
{
    KeyUp(&in_strafe);
}

void IN_AttackDown()
{
    KeyDown(&in_attack);
}
void IN_AttackUp()
{
    KeyUp(&in_attack);
}

void IN_OffHandAttackDown()
{
    KeyDown(&in_offhandattack);
}
void IN_OffHandAttackUp()
{
    KeyUp(&in_offhandattack);
}

void IN_UseDown()
{
    KeyDown(&in_use);
}
void IN_UseUp()
{
    KeyUp(&in_use);
}
void IN_JumpDown()
{
    KeyDown(&in_jump);
}
void IN_JumpUp()
{
    KeyUp(&in_jump);
}

void IN_Button3Down()
{
    KeyDown(&in_button3);
}
void IN_Button3Up()
{
    KeyUp(&in_button3);
}
void IN_Button4Down()
{
    KeyDown(&in_button4);
}
void IN_Button4Up()
{
    KeyUp(&in_button4);
}
void IN_Button5Down()
{
    KeyDown(&in_button5);
}
void IN_Button5Up()
{
    KeyUp(&in_button5);
}
void IN_Button6Down()
{
    KeyDown(&in_button6);
}
void IN_Button6Up()
{
    KeyUp(&in_button6);
}
void IN_Button7Down()
{
    KeyDown(&in_button7);
}
void IN_Button7Up()
{
    KeyUp(&in_button7);
}
void IN_Button8Down()
{
    KeyDown(&in_button8);
}
void IN_Button8Up()
{
    KeyUp(&in_button8);
}

void IN_Impulse()
{
    in_impulse = Q_atoi(Cmd_Argv(1));
}

/*
===============
CL_KeyState

Returns 0.25 if a key was pressed and released during the frame,
0.5 if it was pressed and held
0 if held then released, and
1.0 if held for the entire time
===============
*/
float CL_KeyState(kbutton_t* key)
{
    bool impulsedown = key->state & 2;
    bool impulseup = key->state & 4;
    bool down = key->state & 1;
    float val = 0;

    if(impulsedown && !impulseup)
    {
        if(down)
        {
            val = 0.5; // pressed and held this frame
        }
        else
        {
            val = 0; //	I_Error ();
        }
    }
    if(impulseup && !impulsedown)
    {
        if(down)
        {
            val = 0; //	I_Error ();
        }
        else
        {
            val = 0; // released this frame
        }
    }
    if(!impulsedown && !impulseup)
    {
        if(down)
        {
            val = 1.0; // held the entire frame
        }
        else
        {
            val = 0; // up the entire frame
        }
    }
    if(impulsedown && impulseup)
    {
        if(down)
        {
            val = 0.75; // released and re-pressed this frame
        }
        else
        {
            val = 0.25; // pressed and released this frame
        }
    }

    key->state &= 1; // clear impulses

    return val;
}


//==========================================================================

cvar_t cl_upspeed = {"cl_upspeed", "200", CVAR_NONE};
cvar_t cl_forwardspeed = {"cl_forwardspeed", "200", CVAR_ARCHIVE};
cvar_t cl_backspeed = {"cl_backspeed", "200", CVAR_ARCHIVE};
cvar_t cl_sidespeed = {"cl_sidespeed", "350", CVAR_NONE};

cvar_t cl_movespeedkey = {"cl_movespeedkey", "0.5", CVAR_ARCHIVE};

cvar_t cl_yawspeed = {"cl_yawspeed", "140", CVAR_NONE};
cvar_t cl_pitchspeed = {"cl_pitchspeed", "150", CVAR_NONE};

cvar_t cl_anglespeedkey = {"cl_anglespeedkey", "1.5", CVAR_NONE};

cvar_t cl_alwaysrun = {
    "cl_alwaysrun", "0", CVAR_ARCHIVE}; // QuakeSpasm -- new always run

/*
================
CL_AdjustAngles

Moves the local angle positions
================
*/
void CL_AdjustAngles()
{
    float speed;

    if((in_speed.state & 1) ^ (cl_alwaysrun.value == 0.0))
    {
        speed = host_frametime * cl_anglespeedkey.value;
    }
    else
    {
        speed = host_frametime;
    }

    if(!(in_strafe.state & 1))
    {
        cl.aimangles[YAW] -= speed * cl_yawspeed.value * CL_KeyState(&in_right);
        cl.aimangles[YAW] += speed * cl_yawspeed.value * CL_KeyState(&in_left);
        cl.aimangles[YAW] = anglemod(cl.aimangles[YAW]);
    }
    if(in_klook.state & 1)
    {
        V_StopPitchDrift();
        cl.aimangles[PITCH] -=
            speed * cl_pitchspeed.value * CL_KeyState(&in_forward);
        cl.aimangles[PITCH] +=
            speed * cl_pitchspeed.value * CL_KeyState(&in_back);
    }

    float up = CL_KeyState(&in_lookup);
    float down = CL_KeyState(&in_lookdown);

    cl.aimangles[PITCH] -= speed * cl_pitchspeed.value * up;
    cl.aimangles[PITCH] += speed * cl_pitchspeed.value * down;

    if(up || down)
    {
        V_StopPitchDrift();
    }

    // johnfitz -- variable pitch clamping
    if(cl.aimangles[PITCH] > cl_maxpitch.value)
    {
        cl.aimangles[PITCH] = cl_maxpitch.value;
    }
    if(cl.aimangles[PITCH] < cl_minpitch.value)
    {
        cl.aimangles[PITCH] = cl_minpitch.value;
    }
    // johnfitz

    if(cl.aimangles[ROLL] > 50)
    {
        cl.aimangles[ROLL] = 50;
    }
    if(cl.aimangles[ROLL] < -50)
    {
        cl.aimangles[ROLL] = -50;
    }
}

/*
================
CL_BaseMove

Send the intended movement message to the server
================
*/
void CL_BaseMove(usercmd_t* cmd)
{
    if(cls.signon != SIGNONS)
    {
        return;
    }

    Q_memset(cmd, 0, sizeof(*cmd));

    if(in_strafe.state & 1)
    {
        cmd->sidemove += cl_sidespeed.value * CL_KeyState(&in_right);
        cmd->sidemove -= cl_sidespeed.value * CL_KeyState(&in_left);
    }

    cmd->sidemove += cl_sidespeed.value * CL_KeyState(&in_moveright);
    cmd->sidemove -= cl_sidespeed.value * CL_KeyState(&in_moveleft);

    cmd->upmove += cl_upspeed.value * CL_KeyState(&in_up);
    cmd->upmove -= cl_upspeed.value * CL_KeyState(&in_down);

    if(!(in_klook.state & 1))
    {
        cmd->forwardmove += cl_forwardspeed.value * CL_KeyState(&in_forward);
        cmd->forwardmove -= cl_backspeed.value * CL_KeyState(&in_back);
    }

    //
    // adjust for speed key
    //
    if((in_speed.state & 1) ^ (cl_alwaysrun.value == 0.0))
    {
        cmd->forwardmove *= cl_movespeedkey.value;
        cmd->sidemove *= cl_movespeedkey.value;
        cmd->upmove *= cl_movespeedkey.value;
    }
}


/*
==============
CL_SendMove
==============
*/
void CL_SendMove(const usercmd_t* cmd)
{
    constexpr size_t bufsize = 1024;

    int bits;
    sizebuf_t buf;
    byte data[bufsize];

    buf.maxsize = sizeof(data);
    buf.cursize = 0;
    buf.data = data;

    for(int i = 0; i < cl.ackframes_count; i++)
    {
        MSG_WriteByte(&buf, clcdp_ackframe);
        MSG_WriteLong(&buf, cl.ackframes[i]);
    }
    cl.ackframes_count = 0;

    if(cmd)
    {
        int dump = buf.cursize;
        cl.cmd = *cmd;

        //
        // send the movement message
        //
        MSG_WriteByte(&buf, clc_move);

        MSG_WriteFloat(&buf, cl.mtime[0]); // so server can get ping times

        const auto writeAngles = [&](const auto& angles)
        {
            for(int i = 0; i < 3; i++)
            {
                MSG_WriteAngle16(&buf, angles[i], cl.protocolflags);
            }
        };

        const auto writeVec = [&](const auto& vec)
        { MSG_WriteVec3(&buf, vec, cl.protocolflags); };

        // aimangles
        writeAngles(cl.aimangles);

        // viewangles
        writeAngles(cl.viewangles);

        // vr yaw:
        MSG_WriteFloat(&buf, cmd->vryaw);

        // main hand values:
        writeVec(cmd->handpos);
        writeVec(cmd->handrot);
        writeVec(cmd->handvel);
        writeVec(cmd->handthrowvel);
        MSG_WriteFloat(&buf, cmd->handvelmag);
        writeVec(cmd->handavel);

        // off hand values:
        writeVec(cmd->offhandpos);
        writeVec(cmd->offhandrot);
        writeVec(cmd->offhandvel);
        writeVec(cmd->offhandthrowvel);
        MSG_WriteFloat(&buf, cmd->offhandvelmag);
        writeVec(cmd->offhandavel);

        // headvel
        writeVec(cmd->headvel);

        // muzzlepos
        writeVec(cmd->muzzlepos);

        // offmuzzlepos
        writeVec(cmd->offmuzzlepos);

        // vrbits0
        MSG_WriteUnsignedShort(&buf, cmd->vrbits0);

        // movement
        MSG_WriteShort(&buf, cmd->forwardmove);
        MSG_WriteShort(&buf, cmd->sidemove);
        MSG_WriteShort(&buf, cmd->upmove);

        // teleportation
        writeVec(cmd->teleport_target);

        // hands
        MSG_WriteByte(&buf, cmd->offhand_hotspot);
        MSG_WriteByte(&buf, cmd->mainhand_hotspot);

        // roomscalemove
        writeVec(cmd->roomscalemove);

        //
        // send button bits
        //
        bits = 0;

        if(in_attack.state & 3)
        {
            bits |= 1;
        }
        in_attack.state &= ~2;

        if(in_jump.state & 3)
        {
            bits |= 2;
        }
        in_jump.state &= ~2;

        if(in_offhandattack.state & 3)
        {
            bits |= 4;
        }
        in_offhandattack.state &= ~2;

        MSG_WriteByte(&buf, bits);

        MSG_WriteByte(&buf, in_impulse);
        in_impulse = 0;

        //
        // deliver the message
        //
        if(cls.demoplayback)
        {
            return;
        }

        //
        // allways dump the first two message, because it may contain leftover
        // inputs from the last level
        //
        if(++cl.movemessages <= 2)
        {
            buf.cursize = dump;
        }
        else
        {
            S_Voip_Transmit(clcfte_voicechat, &buf); /*Spike: Add voice
            data*/
        }
    }
    else
    {
        S_Voip_Transmit(clcfte_voicechat,
            nullptr); /*Spike: Add voice data (with cl_voip_test anyway)*/
    }

    // fixme: nops if we're still connecting, or something.

    //
    // deliver the message
    //
    if(cls.demoplayback || !buf.cursize)
    {
        return;
    }

    if(NET_SendUnreliableMessage(cls.netcon, &buf) == -1)
    {
        Con_Printf("CL_SendMove: lost server connection\n");
        CL_Disconnect();
    }
}

/*
============
CL_InitInput
============
*/
void CL_InitInput()
{
    Cmd_AddCommand("+moveup", IN_UpDown);
    Cmd_AddCommand("-moveup", IN_UpUp);
    Cmd_AddCommand("+movedown", IN_DownDown);
    Cmd_AddCommand("-movedown", IN_DownUp);
    Cmd_AddCommand("+left", IN_LeftDown);
    Cmd_AddCommand("-left", IN_LeftUp);
    Cmd_AddCommand("+right", IN_RightDown);
    Cmd_AddCommand("-right", IN_RightUp);
    Cmd_AddCommand("+forward", IN_ForwardDown);
    Cmd_AddCommand("-forward", IN_ForwardUp);
    Cmd_AddCommand("+back", IN_BackDown);
    Cmd_AddCommand("-back", IN_BackUp);
    Cmd_AddCommand("+lookup", IN_LookupDown);
    Cmd_AddCommand("-lookup", IN_LookupUp);
    Cmd_AddCommand("+lookdown", IN_LookdownDown);
    Cmd_AddCommand("-lookdown", IN_LookdownUp);
    Cmd_AddCommand("+strafe", IN_StrafeDown);
    Cmd_AddCommand("-strafe", IN_StrafeUp);
    Cmd_AddCommand("+moveleft", IN_MoveleftDown);
    Cmd_AddCommand("-moveleft", IN_MoveleftUp);
    Cmd_AddCommand("+moveright", IN_MoverightDown);
    Cmd_AddCommand("-moveright", IN_MoverightUp);
    Cmd_AddCommand("+speed", IN_SpeedDown);
    Cmd_AddCommand("-speed", IN_SpeedUp);
    Cmd_AddCommand("+attack", IN_AttackDown);
    Cmd_AddCommand("-attack", IN_AttackUp);
    Cmd_AddCommand("+offhandattack", IN_OffHandAttackDown);
    Cmd_AddCommand("-offhandattack", IN_OffHandAttackUp);
    Cmd_AddCommand("+use", IN_UseDown);
    Cmd_AddCommand("-use", IN_UseUp);
    Cmd_AddCommand("+button3", IN_Button3Down);
    Cmd_AddCommand("-button3", IN_Button3Up);
    Cmd_AddCommand("+button4", IN_Button4Down);
    Cmd_AddCommand("-button4", IN_Button4Up);
    Cmd_AddCommand("+button5", IN_Button5Down);
    Cmd_AddCommand("-button5", IN_Button5Up);
    Cmd_AddCommand("+button6", IN_Button6Down);
    Cmd_AddCommand("-button6", IN_Button6Up);
    Cmd_AddCommand("+button7", IN_Button7Down);
    Cmd_AddCommand("-button7", IN_Button7Up);
    Cmd_AddCommand("+button8", IN_Button8Down);
    Cmd_AddCommand("-button8", IN_Button8Up);
    Cmd_AddCommand("+jump", IN_JumpDown);
    Cmd_AddCommand("-jump", IN_JumpUp);
    Cmd_AddCommand("impulse", IN_Impulse);
    Cmd_AddCommand("+klook", IN_KLookDown);
    Cmd_AddCommand("-klook", IN_KLookUp);
    Cmd_AddCommand("+mlook", IN_MLookDown);
    Cmd_AddCommand("-mlook", IN_MLookUp);
    Cmd_AddCommand("+grableft", [] { KeyDown(&in_grableft); });
    Cmd_AddCommand("-grableft", [] { KeyUp(&in_grableft); });
    Cmd_AddCommand("+grabright", [] { KeyDown(&in_grabright); });
    Cmd_AddCommand("-grabright", [] { KeyUp(&in_grabright); });
    Cmd_AddCommand("+reloadleft", [] { KeyDown(&in_reloadleft); });
    Cmd_AddCommand("-reloadleft", [] { KeyUp(&in_reloadleft); });
    Cmd_AddCommand("+reloadright", [] { KeyDown(&in_reloadright); });
    Cmd_AddCommand("-reloadright", [] { KeyUp(&in_reloadright); });
}
