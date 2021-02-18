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

#pragma once

#include "q_stdinc.hpp"

//
// console
//
extern int con_totallines;
extern int con_backscroll;
extern bool con_forcedup; // because no entities to refresh
extern bool con_initialized;
extern byte* con_chars;

extern char con_lastcenterstring[]; // johnfitz

void Con_DrawCharacter(int cx, int line, int num);

void Con_CheckResize();
void Con_Init();
void Con_DrawConsole(int lines, bool drawinput);
void Con_Printf(const char* fmt, ...) FUNC_PRINTF(1, 2);
void Con_DWarning(const char* fmt, ...) FUNC_PRINTF(1, 2); // ericw
void Con_Warning(const char* fmt, ...) FUNC_PRINTF(1, 2);  // johnfitz
void Con_DPrintf(const char* fmt, ...) FUNC_PRINTF(1, 2);
void Con_DPrintf2(const char* fmt, ...) FUNC_PRINTF(1, 2); // johnfitz
void Con_DPrintf3(const char* fmt, ...) FUNC_PRINTF(1, 2);
void Con_SafePrintf(const char* fmt, ...) FUNC_PRINTF(1, 2);
void Con_DrawNotify();
void Con_ClearNotify();
void Con_ToggleConsole_f();

// QSS
bool Con_IsRedirected(); // returns true if its redirected. this generally means
                         // that things are a little more verbose.

void Con_Redirect(void (*flush)(const char* text));

void Con_NotifyBox(const char* text); // during startup for sound / cd warnings

void Con_Show();
void Con_Hide();

const char* Con_Quakebar(int len);
void Con_TabComplete();
void Con_LogCenterPrint(const char* str);

//
// debuglog
//
struct quakeparms_t;
void LOG_Init(quakeparms_t* parms);
void LOG_Close();
void Con_DebugLog(const char* msg);
