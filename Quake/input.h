/*
Copyright (C) 1996-2001 Id Software, Inc.
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

#ifndef QUAKE_INPUT_H
#define QUAKE_INPUT_H

// input.h -- external (non-keyboard) input devices

void IN_Init (void);

void IN_Shutdown (void);

void IN_Commands (void);
// oportunity for devices to stick commands on the script buffer

// mouse moved by dx and dy pixels
void IN_MouseMotion(int dx, int dy);
void IN_MockMouseMotion (int dx, int dy); // QVR: the tests' mouse (focus aside; vr_flatleanmouse_test)

// QVR: splitscreen (docs/vr-port/SPLITSCREEN.md, phase 2): up to four game controllers open, pad k driving the seat
// IN_PadSeat names (-1: none); vr_seat_test's mock pads (no controller, polled by IN_Commands as one)
int IN_PadSeat (int k);
void IN_MockPad (int k, qboolean on);
void IN_MockPadButton (int k, int button, qboolean down);	// button: SDL_GameControllerButton
void IN_MockPadAxes (int k, float lx, float ly, float rx, float ry);
const char *IN_PadName (int k);				// phase 10's join screen: pad k's name (NULL: no pad there)
qboolean IN_PadDown (int k, int button);	// its button held (SDL_GameControllerButton), as last read
float IN_PadAxis (int k, int axis);			// its axis (SDL_GameControllerAxis), -1..1, as last read
// split11: a pad that left its seat drives no seat (not seat 1's as its spare pad) while a local multiplayer game runs;
// its Start or A joins it again (VR_SeatDropIn). Set by a seat's leaving, cleared by its joining.
void IN_PadSetUnseated (int k, qboolean unseated);
void IN_JoyMove (usercmd_t *cmd);
void IN_MouseMove (usercmd_t *cmd);
void IN_MockKeyboardKey (int key, qboolean down); // a key event as the keyboard's (an SDL event, IN_SendKeyEvents)

//
// controller gyro
//
typedef enum gyromode_t
{
	GYRO_BUTTON_IGNORED,
	GYRO_BUTTON_ENABLES,
	GYRO_BUTTON_DISABLES,
	GYRO_BUTTON_INVERTS_DIR,

	GYRO_MODE_COUNT,
} gyromode_t;

qboolean IN_HasGyro (void);
float IN_GetRawGyroMagnitude (void);
void IN_StartGyroCalibration (void);
qboolean IN_IsCalibratingGyro (void);
float IN_GetGyroCalibrationProgress (void);

float IN_GetRawLookMagnitude (void);
float IN_GetRawMoveMagnitude (void);
float IN_GetRawTriggerMagnitude (void);

typedef enum gamepadtype_t
{
	GAMEPAD_NONE,
	GAMEPAD_XBOX,
	GAMEPAD_PLAYSTATION,
	GAMEPAD_NINTENDO,
} gamepadtype_t;

qboolean IN_HasRumble (void);
qboolean IN_HasGamepad (void);
const char *IN_GetGamepadName (void);
gamepadtype_t IN_GetGamepadType (void);
void IN_UseNextGamepad (int dir, qboolean allow_disable);

void IN_SendKeyEvents (void);
// used as a callback for Sys_SendKeyEvents() by some drivers

void IN_UpdateInputMode (void);
// do stuff if input mode (text/non-text) changes matter to the keyboard driver

qboolean IN_EmulatedCharEvents (void);

#ifndef __cplusplus // QVR: C++ (the VR module) has no incomplete enums, and does not need this
enum keydevice_t IN_GetLastActiveDeviceType (void);
#endif // QVR

void IN_Move (usercmd_t *cmd);
// add additional movement on top of the keyboard move cmd

void IN_ClearStates (void);
// restores all button and position states to defaults

// called when the app becomes active
void IN_Activate (void);

// called when the app becomes inactive
void IN_Deactivate (qboolean free_cursor);
void IN_DeactivateForConsole (void);
void IN_DeactivateForMenu (void);

#endif
