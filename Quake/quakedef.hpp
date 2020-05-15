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

#include "sys.hpp"
#include "zone.hpp"

#include "protocol.hpp"

#include "cmd.hpp"

#include "progs.hpp"
#include "server.hpp"

#include <SDL2/SDL.h>

#include "console.hpp"
#include "vid.hpp"
#include "screen.hpp"
#include "draw.hpp"
#include "render.hpp"
#include "view.hpp"
#include "q_sound.hpp"
#include "client.hpp"

#include "gl_model.hpp"
#include "world.hpp"

#include "gl_texmgr.hpp" //johnfitz
#include "input.hpp"
#include "keys.hpp"
#include "menu.hpp"


//=============================================================================

// the host system specifies the base of the directory tree, the
// command line parms passed to the program, and the amount of memory
// available for the program to use

extern bool noclip_anglehack;

//
// host
//
extern quakeparms_t* host_parms;

extern cvar_t sys_ticrate;
extern cvar_t sys_throttle;
extern cvar_t sys_nostdout;
extern cvar_t developer;
extern cvar_t max_edicts; // johnfitz

extern bool host_initialized; // true if into command execution
extern double host_frametime;
extern byte* host_colormap;
extern int host_framecount; // incremented every frame, never reset
extern double realtime;     // not bounded in any way, changed at
                            // start of every frame, never reset

typedef struct filelist_item_s
{
    char name[32];
    struct filelist_item_s* next;
} filelist_item_t;

extern filelist_item_t* modlist;
extern filelist_item_t* extralevels;
extern filelist_item_t* demolist;

void Host_ClearMemory();
void Host_ServerFrame();
void Host_InitCommands();
void Host_Init();
void Host_Shutdown();
void Host_Callback_Notify(cvar_t* var); /* callback function for CVAR_NOTIFY */
void Host_Warn(const char* error, ...) FUNC_PRINTF(1, 2);
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

void ExtraMaps_Init();
void Modlist_Init();
void DemoList_Init();

void DemoList_Rebuild();

extern int current_skill; // skill level for currently loaded level (in case
                          //  the user changes the cvar while the level is
                          //  running, this reflects the level actually in use)

extern bool isDedicated;

extern int minimum_memory;
