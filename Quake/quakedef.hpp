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

#include "cvar.hpp"
#include "quakeparms.hpp"
#include "q_stdinc.hpp"

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
extern cvar_t max_edicts; // johnfitz

extern bool host_initialized; // true if into command execution
extern double host_frametime;
extern byte* host_colormap;
extern int host_framecount; // incremented every frame, never reset
extern double realtime;     // not bounded in any way, changed at
                            // start of every frame, never reset

struct filelist_item_t
{
    char name[32];
    filelist_item_t* next;
};

extern filelist_item_t* modlist;
extern filelist_item_t* extralevels;
extern filelist_item_t* demolist;

void ExtraMaps_Init();
void Modlist_Init();
void DemoList_Init();

void DemoList_Rebuild();

extern int current_skill; // skill level for currently loaded level (in case
                          // the user changes the cvar while the level is
                          // running, this reflects the level actually in use)

extern bool isDedicated;

extern int minimum_memory;
