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

#define CMP_NONE 0
#define CMP_LZSS 1

#define TYP_NONE 0
#define TYP_LABEL 1

#define TYP_LUMPY 64 // 64 + grab command number
#define TYP_PALETTE 64
#define TYP_QTEX 65
#define TYP_QPIC 66
#define TYP_SOUND 67
#define TYP_MIPTEX 68

#define WADFILENAME \
    "gfx.wad" // johnfitz -- filename is now hard-coded for honesty

typedef struct
{
    char identification[4]; // should be WAD2 or 2DAW
    int numlumps;
    int infotableofs;
} wadinfo_t;

typedef struct
{
    int filepos;
    int disksize;
    int size; // uncompressed
    char type;
    char compression;
    char pad1, pad2;
    char name[16]; // must be null terminated
} lumpinfo_t;

extern int wad_numlumps;
extern lumpinfo_t* wad_lumps;
extern byte* wad_base;

void W_LoadWadFile(); // johnfitz -- filename is now hard-coded for honesty

// QSS
void* W_GetLumpName(const char* name, lumpinfo_t** out_info);
