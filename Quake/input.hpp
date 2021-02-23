/*
Copyright (C) 1996-2001 Id Software, Inc.
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

#pragma once

#include "protocol.hpp"

// input.h -- external (non-keyboard) input devices

void IN_Init();

void IN_Shutdown();

void IN_Commands();
// oportunity for devices to stick commands on the script buffer

// mouse moved by dx and dy pixels
void IN_MouseMotion(int dx, int dy, int wx, int wy);

void IN_SendKeyEvents();
// used as a callback for Sys_SendKeyEvents() by some drivers

void IN_UpdateInputMode();
// do stuff if input mode (text/non-text) changes matter to the keyboard driver

void IN_Move(usercmd_t* cmd);
// add additional movement on top of the keyboard move cmd

void IN_ClearStates();
// restores all button and position states to defaults

// spike - called whenever mouse focus etc has changed (including console
// toggled). this is optional, but there's still a number of blocking commands,
// like connect doing all the mode, state, etc checks in one place ensures that
// they're consistent, regardless of what else is happening.
void IN_UpdateGrabs();
