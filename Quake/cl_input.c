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
// cl.input.c  -- builds an intended movement command to send to the server

// Quake is a trademark of Id Software, Inc., (c) 1996 Id Software, Inc. All
// rights reserved.

#include "quakedef.h"
#include "vr/vr_api.h"

extern cvar_t cl_maxpitch; //johnfitz -- variable pitch clamping
extern cvar_t cl_minpitch; //johnfitz -- variable pitch clamping
extern cvar_t cl_mwheelpitch;

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


// QVR: the buttons (in_forward, in_attack...) and the impulse are the seat's own (seatinput_t, client.h;
// docs/vr-port/SPLITSCREEN.md): every +command and the move below act on CL_SeatInput (), the current seat's.


void KeyDown (kbutton_t *b)
{
	int		k;
	const char	*c;

	c = Cmd_Argv(1);
	if (c[0])
		k = atoi(c);
	else
		k = -1;		// typed manually at the console for continuous down

	if (k == b->down[0] || k == b->down[1])
		return;		// repeating key

	if (!b->down[0])
		b->down[0] = k;
	else if (!b->down[1])
		b->down[1] = k;
	else
	{
		Con_Printf ("Three keys down for a button!\n");
		return;
	}

	if (b->state & 1)
		return;		// still down
	b->state |= 1 + 2;	// down + impulse down
}

void KeyUp (kbutton_t *b)
{
	int		k;
	const char	*c;

	c = Cmd_Argv(1);
	if (c[0])
		k = atoi(c);
	else
	{ // typed manually at the console, assume for unsticking, so clear all
		b->down[0] = b->down[1] = 0;
		b->state = 4;	// impulse up
		return;
	}

	if (b->down[0] == k)
		b->down[0] = 0;
	else if (b->down[1] == k)
		b->down[1] = 0;
	else
		return;		// key up without coresponding down (menu pass through)
	if (b->down[0] || b->down[1])
		return;		// some other key is still holding it down

	if (!(b->state & 1))
		return;		// still up (this should not happen)
	b->state &= ~1;		// now up
	b->state |= 4; 		// impulse up
}

void IN_AccumMWheelPitch (float amt)
{
	int key;
	if (key_dest != key_game || Cmd_Argc() < 2)
		return;
	key = atoi (Cmd_Argv (1));
	if (key == K_MWHEELDOWN || key == K_MWHEELUP)
	{
		cl.wheel_pitch += amt;
		cl.wheel_pitch = CLAMP (-90.f, cl.wheel_pitch, 90.f);
	}
}

void IN_KLookDown (void) {KeyDown(&CL_SeatInput()->klook);}
void IN_KLookUp (void) {KeyUp(&CL_SeatInput()->klook);}
void IN_MLookDown (void) {KeyDown(&CL_SeatInput()->mlook);}
void IN_MLookUp (void) {
	KeyUp(&CL_SeatInput()->mlook);
	if ( !((CL_SeatInput()->mlook.state & 1) || freelook.value) && lookspring.value)
		V_StartPitchDrift();
}
void IN_UpDown(void) {KeyDown(&CL_SeatInput()->up);}
void IN_UpUp(void) {KeyUp(&CL_SeatInput()->up);}
void IN_DownDown(void) {KeyDown(&CL_SeatInput()->down);}
void IN_DownUp(void) {KeyUp(&CL_SeatInput()->down);}
void IN_LeftDown(void) {KeyDown(&CL_SeatInput()->left);}
void IN_LeftUp(void) {KeyUp(&CL_SeatInput()->left);}
void IN_RightDown(void) {KeyDown(&CL_SeatInput()->right);}
void IN_RightUp(void) {KeyUp(&CL_SeatInput()->right);}
void IN_ForwardDown(void) {KeyDown(&CL_SeatInput()->forward);}
void IN_ForwardUp(void) {KeyUp(&CL_SeatInput()->forward);}
void IN_BackDown(void) {KeyDown(&CL_SeatInput()->back);}
void IN_BackUp(void) {KeyUp(&CL_SeatInput()->back);}
void IN_LookupDown(void) {KeyDown(&CL_SeatInput()->lookup);}
void IN_LookupUp(void) {KeyUp(&CL_SeatInput()->lookup); IN_AccumMWheelPitch(-cl_mwheelpitch.value);}
void IN_LookdownDown(void) {KeyDown(&CL_SeatInput()->lookdown);}
void IN_LookdownUp(void) {KeyUp(&CL_SeatInput()->lookdown); IN_AccumMWheelPitch(cl_mwheelpitch.value);}
void IN_MoveleftDown(void) {KeyDown(&CL_SeatInput()->moveleft);}
void IN_MoveleftUp(void) {KeyUp(&CL_SeatInput()->moveleft);}
void IN_MoverightDown(void) {KeyDown(&CL_SeatInput()->moveright);}
void IN_MoverightUp(void) {KeyUp(&CL_SeatInput()->moveright);}

void IN_SpeedDown(void)
{
	const int was = CL_SeatInput()->speed.state & 1;
	KeyDown(&CL_SeatInput()->speed);
	if (!was && (CL_SeatInput()->speed.state & 1))
		VR_SpeedPressed (); // QVR: vr_sprint_toggle
}
void IN_SpeedUp(void) {KeyUp(&CL_SeatInput()->speed);}
void IN_StrafeDown(void) {KeyDown(&CL_SeatInput()->strafe);}
void IN_StrafeUp(void) {KeyUp(&CL_SeatInput()->strafe);}

void IN_AttackDown(void) {if (VR_HumStudioAttack (1)) return; KeyDown(&CL_SeatInput()->attack);} // QVR: the Texture Studio's strokes while it paints (vr/vr_humstudio.cpp)
void IN_AttackUp(void) {if (VR_HumStudioAttack (0)) return; KeyUp(&CL_SeatInput()->attack);} // QVR

void IN_UseDown (void) {KeyDown(&CL_SeatInput()->use);}
void IN_UseUp (void) {KeyUp(&CL_SeatInput()->use);}
void IN_JumpDown (void) {KeyDown(&CL_SeatInput()->jump);}
void IN_JumpUp (void) {KeyUp(&CL_SeatInput()->jump);}

void IN_Impulse (void) {CL_SeatInput()->impulse=Q_atoi(Cmd_Argv(1));}

/*
===============
CL_KeyState

Returns 0.25 if a key was pressed and released during the frame,
0.5 if it was pressed and held
0 if held then released, and
1.0 if held for the entire time
===============
*/
float CL_KeyState (kbutton_t *key)
{
	float		val;
	qboolean	impulsedown, impulseup, down;

	impulsedown = key->state & 2;
	impulseup = key->state & 4;
	down = key->state & 1;
	val = 0;

	if (impulsedown && !impulseup)
	{
		if (down)
			val = 0.5;	// pressed and held this frame
		else
			val = 0;	//	I_Error ();
	}
	if (impulseup && !impulsedown)
	{
		if (down)
			val = 0;	//	I_Error ();
		else
			val = 0;	// released this frame
	}
	if (!impulsedown && !impulseup)
	{
		if (down)
			val = 1.0;	// held the entire frame
		else
			val = 0;	// up the entire frame
	}
	if (impulsedown && impulseup)
	{
		if (down)
			val = 0.75;	// released and re-pressed this frame
		else
			val = 0.25;	// pressed and released this frame
	}

	key->state &= 1;		// clear impulses

	return val;
}


//==========================================================================

cvar_t	cl_upspeed = {"cl_upspeed","200",CVAR_NONE};
cvar_t	cl_forwardspeed = {"cl_forwardspeed","200", CVAR_ARCHIVE};
cvar_t	cl_backspeed = {"cl_backspeed","200", CVAR_ARCHIVE};
cvar_t	cl_sidespeed = {"cl_sidespeed","350",CVAR_NONE};

cvar_t	cl_movespeedkey = {"cl_movespeedkey","2.0",CVAR_NONE};

cvar_t	cl_yawspeed = {"cl_yawspeed","140",CVAR_NONE};
cvar_t	cl_pitchspeed = {"cl_pitchspeed","150",CVAR_NONE};

cvar_t	cl_anglespeedkey = {"cl_anglespeedkey","1.5",CVAR_NONE};

cvar_t	cl_alwaysrun = {"cl_alwaysrun","0",CVAR_ARCHIVE}; // QuakeSpasm -- new always run. QVR: walking by default (the author, 2026-10-10: a new install walks; the left stick's press runs, +speed held); configs keep theirs (every archived cvar is saved)

/*
================
CL_InCutscene
================
*/
qboolean CL_InCutscene (void)
{
	return cl.fixangle && !cl.viewent.model;
}

/*
================
CL_AdjustAngles

Moves the local angle positions
================
*/
void CL_AdjustAngles (void)
{
	seatinput_t	*in = CL_SeatInput ();
	float	speed;
	float	up, down;

	if (CL_InCutscene ())
	{
		V_StopPitchDrift ();
		return;
	}

	if (VR_SpeedButton (in->speed.state & 1) ^ (cl_alwaysrun.value != 0.0)) // QVR: VR_SpeedButton (vr_sprint_toggle)
		speed = host_frametime * cl_anglespeedkey.value;
	else
		speed = host_frametime;

	if (!(in->strafe.state & 1))
	{
		cl.viewangles[YAW] -= speed*cl_yawspeed.value*CL_KeyState (&in->right);
		cl.viewangles[YAW] += speed*cl_yawspeed.value*CL_KeyState (&in->left);
		cl.viewangles[YAW] -= floor (cl.viewangles[YAW]/360.0) * 360.0;
	}
	if (in->klook.state & 1)
	{
		V_StopPitchDrift ();
		cl.viewangles[PITCH] -= speed*cl_pitchspeed.value * CL_KeyState (&in->forward);
		cl.viewangles[PITCH] += speed*cl_pitchspeed.value * CL_KeyState (&in->back);
	}

	up = CL_KeyState (&in->lookup);
	down = CL_KeyState(&in->lookdown);

	cl.viewangles[PITCH] -= speed*cl_pitchspeed.value * up;
	cl.viewangles[PITCH] += speed*cl_pitchspeed.value * down;

	if (up || down || cl.wheel_pitch)
		V_StopPitchDrift ();

	if (cl.wheel_pitch)
	{
		float delta = speed*cl_pitchspeed.value;
		if (cl.wheel_pitch > 0.f)
		{
			cl.viewangles[PITCH] += q_min (delta, cl.wheel_pitch);
			cl.wheel_pitch -= delta;
			cl.wheel_pitch = q_max (0.f, cl.wheel_pitch);
		}
		else
		{
			cl.viewangles[PITCH] -= q_min (delta, -cl.wheel_pitch);
			cl.wheel_pitch += delta;
			cl.wheel_pitch = q_min (0.f, cl.wheel_pitch);
		}
	}

	//johnfitz -- variable pitch clamping
	if (cl.viewangles[PITCH] > cl_maxpitch.value)
		cl.viewangles[PITCH] = cl_maxpitch.value;
	if (cl.viewangles[PITCH] < cl_minpitch.value)
		cl.viewangles[PITCH] = cl_minpitch.value;
	//johnfitz

	if (cl.viewangles[ROLL] > 50)
		cl.viewangles[ROLL] = 50;
	if (cl.viewangles[ROLL] < -50)
		cl.viewangles[ROLL] = -50;
}

/*
================
CL_BaseMove

Send the intended movement message to the server
================
*/
void CL_BaseMove (usercmd_t *cmd)
{
	seatinput_t	*in = CL_SeatInput ();

	if (cls.signon != SIGNONS)
		return;

	Q_memset (cmd, 0, sizeof(*cmd));

	if (in->strafe.state & 1)
	{
		cmd->sidemove += cl_sidespeed.value * CL_KeyState (&in->right);
		cmd->sidemove -= cl_sidespeed.value * CL_KeyState (&in->left);
	}

	cmd->sidemove += cl_sidespeed.value * CL_KeyState (&in->moveright);
	cmd->sidemove -= cl_sidespeed.value * CL_KeyState (&in->moveleft);

	cmd->upmove += cl_upspeed.value * CL_KeyState (&in->up);
	cmd->upmove -= cl_upspeed.value * q_max (CL_KeyState (&in->down), VR_FlatCrouchSwim ()); // QVR: flat play's crouch keys swim down too

	if (! (in->klook.state & 1) )
	{
		cmd->forwardmove += cl_forwardspeed.value * CL_KeyState (&in->forward);
		cmd->forwardmove -= cl_backspeed.value * CL_KeyState (&in->back);
	}

//
// adjust for speed key
//
	if (VR_SpeedButton (in->speed.state & 1) ^ (cl_alwaysrun.value != 0.0)) // QVR: VR_SpeedButton (vr_sprint_toggle)
	{
		cmd->forwardmove *= cl_movespeedkey.value;
		cmd->sidemove *= cl_movespeedkey.value;
		cmd->upmove *= cl_movespeedkey.value;
	}
	VR_FlatMoveScale (&cmd->forwardmove, &cmd->sidemove); // QVR: leaning (flatscreen play), slower (vr_flat_lean_speed)
}


/*
==============
CL_SendMove
==============
*/
void CL_SendMove (const usercmd_t *cmd)
{
	seatinput_t	*in = CL_SeatInput ();
	int		i;
	int		bits;
	sizebuf_t	buf;
	byte	data[1024]; // QVR: was 128

	buf.maxsize = sizeof (data); // QVR
	buf.cursize = 0;
	buf.data = data;

	if (cmd) 
	{
		cl.cmd = *cmd;

	//
	// send the movement message
	//
		MSG_WriteByte (&buf, clc_move);

		MSG_WriteFloat (&buf, cl.mtime[0]);	// so server can get ping times

		for (i=0 ; i<3 ; i++)
			//johnfitz -- 16-bit angles for PROTOCOL_FITZQUAKE
			if (cl.protocol == PROTOCOL_NETQUAKE)
				MSG_WriteAngle (&buf, cl.viewangles[i], cl.protocolflags);
			else
				MSG_WriteAngle16 (&buf, cl.viewangles[i], cl.protocolflags);
			//johnfitz

		MSG_WriteShort (&buf, cmd->forwardmove);
		MSG_WriteShort (&buf, cmd->sidemove);
		MSG_WriteShort (&buf, cmd->upmove);

	//
	// send button bits
	//
		bits = 0;

		if ( VR_MoveMainFire ((in->attack.state & 3) != 0) ) // QVR: flatscreen play routes the mouse's buttons by what the hands hold
			bits |= 1;
		in->attack.state &= ~2;

		if (VR_MoveJump (in->jump.state)) // QVR: flatscreen play, lying down: the jump press stands him up instead
			bits |= 2;
		in->jump.state &= ~2;

		if (VR_FlatFlycamOwnsInput ()) // QVR: the flatscreen fly camera has the keys (Space flies it up): no fire, no jump, no impulse reaches him
		{
			bits = 0;
			in->impulse = 0;
		}

		MSG_WriteByte (&buf, bits);

		MSG_WriteByte (&buf, in->impulse);

		// QVR: the VR block; a remote client's whole move instead as quantized deltas with repeats (vr/vr_netmove.cpp)
		VR_WriteMoveExtras (&buf, cmd->forwardmove, cmd->sidemove, cmd->upmove, bits, in->impulse);
		in->impulse = 0;
	}

	VR_WriteVoice (&buf); // QVR: voice chat's frames (vr/vr_voicechat.cpp, B10v)

//
// deliver the message
//
	if (cls.demoplayback)
		return;

//
// allways dump the first two message, because it may contain leftover inputs
// from the last level
//
	if (++cl.movemessages <= 2)
		return;

	if (NET_SendUnreliableMessage (cls.netcon, &buf) == -1)
	{
		Con_Printf ("CL_SendMove: lost server connection\n");
		CL_Disconnect ();
	}
}

/*
============
CL_ReleaseMoveButtons

QVR: a menu opened over a running game (live preview; VR_AdjustMove): the movement buttons let go -- forward, back,
the strafes, turning, up and down (swim, crouch), jump, run -- held by a key or typed at the console alike, so the
player stops and nothing resumes on the menu's close until pressed again (a key still held: its later key up finds
no down, harmless).
============
*/
static const size_t move_buttons[] = { // offsets into seatinput_t: the current seat's buttons
	offsetof (seatinput_t, forward), offsetof (seatinput_t, back), offsetof (seatinput_t, moveleft),
	offsetof (seatinput_t, moveright), offsetof (seatinput_t, left), offsetof (seatinput_t, right),
	offsetof (seatinput_t, lookup), offsetof (seatinput_t, lookdown), offsetof (seatinput_t, up),
	offsetof (seatinput_t, down), offsetof (seatinput_t, jump), offsetof (seatinput_t, speed),
	offsetof (seatinput_t, strafe),
};

static kbutton_t *CL_MoveButton (size_t i)
{
	return (kbutton_t *)((byte *)CL_SeatInput () + move_buttons[i]);
}

void CL_ReleaseMoveButtons (void)
{
	size_t i;
	for (i = 0; i < countof (move_buttons); i++)
	{
		kbutton_t *b = CL_MoveButton (i);
		b->down[0] = b->down[1] = 0;
		b->state = 0;
	}
}

int CL_MoveButtonsHeld (void)
{
	size_t i;
	int held = 0;
	for (i = 0; i < countof (move_buttons); i++)
		held += (CL_MoveButton (i)->state & 1) != 0;
	return held;
}

/*
============
CL_InitInput
============
*/
void CL_InitInput (void)
{
	Cmd_AddCommand ("+moveup",IN_UpDown);
	Cmd_AddCommand ("-moveup",IN_UpUp);
	Cmd_AddCommand ("+movedown",IN_DownDown);
	Cmd_AddCommand ("-movedown",IN_DownUp);
	Cmd_AddCommand ("+left",IN_LeftDown);
	Cmd_AddCommand ("-left",IN_LeftUp);
	Cmd_AddCommand ("+right",IN_RightDown);
	Cmd_AddCommand ("-right",IN_RightUp);
	Cmd_AddCommand ("+forward",IN_ForwardDown);
	Cmd_AddCommand ("-forward",IN_ForwardUp);
	Cmd_AddCommand ("+back",IN_BackDown);
	Cmd_AddCommand ("-back",IN_BackUp);
	Cmd_AddCommand ("+lookup", IN_LookupDown);
	Cmd_AddCommand ("-lookup", IN_LookupUp);
	Cmd_AddCommand ("+lookdown", IN_LookdownDown);
	Cmd_AddCommand ("-lookdown", IN_LookdownUp);
	Cmd_AddCommand ("+strafe", IN_StrafeDown);
	Cmd_AddCommand ("-strafe", IN_StrafeUp);
	Cmd_AddCommand ("+moveleft", IN_MoveleftDown);
	Cmd_AddCommand ("-moveleft", IN_MoveleftUp);
	Cmd_AddCommand ("+moveright", IN_MoverightDown);
	Cmd_AddCommand ("-moveright", IN_MoverightUp);
	Cmd_AddCommand ("+speed", IN_SpeedDown);
	Cmd_AddCommand ("-speed", IN_SpeedUp);
	Cmd_AddCommand ("+attack", IN_AttackDown);
	Cmd_AddCommand ("-attack", IN_AttackUp);
	Cmd_AddCommand ("+use", IN_UseDown);
	Cmd_AddCommand ("-use", IN_UseUp);
	Cmd_AddCommand ("+jump", IN_JumpDown);
	Cmd_AddCommand ("-jump", IN_JumpUp);
	Cmd_AddCommand ("impulse", IN_Impulse);
	Cmd_AddCommand ("+klook", IN_KLookDown);
	Cmd_AddCommand ("-klook", IN_KLookUp);
	Cmd_AddCommand ("+mlook", IN_MLookDown);
	Cmd_AddCommand ("-mlook", IN_MLookUp);

}

