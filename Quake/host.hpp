/*
Copyright (C) 1996-2001 Id Software, Inc.
Copyright (C) 2002-2009 John Fitzgibbons and others
Copyright (C) 2007-2008 Kristian Duske
Copyright (C) 2010-2019 QuakeSpasm developers

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

#include <ctime>

struct cvar_t;

void Host_ClearMemory();
void Host_ServerFrame();
void Host_InitCommands();
void Host_Init();
void Host_Shutdown();
void Host_Callback_Notify(cvar_t* var); /* callback function for CVAR_NOTIFY */
void Host_Warn(const char* error, ...) FUNC_PRINTF(1, 2);
bool Host_MakeSavegame(const char* filename, const std::time_t* timestamp,
    const bool printMessage);

[[noreturn]] void Host_Error(const char* error, ...) FUNC_PRINTF(1, 2);
[[noreturn]] void Host_EndGame(const char* message, ...) FUNC_PRINTF(1, 2);

#ifdef __WATCOMC__
#pragma aux Host_Error aborts;
#pragma aux Host_EndGame aborts;
#endif

void Host_Frame(float time);
void Host_Quit_f();
void Host_ClientCommands(const char* fmt, ...) FUNC_PRINTF(1, 2);
void Host_ShutdownServer(bool crash);
void Host_WriteConfiguration();
