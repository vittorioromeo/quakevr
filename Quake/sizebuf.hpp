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

struct sizebuf_t
{
    bool allowoverflow; // if false, do a Sys_Error
    bool overflowed;    // set to true if the buffer size failed
    byte* data;
    int maxsize;
    int cursize;
};

void SZ_Alloc(sizebuf_t* buf, int startsize);
void SZ_Free(sizebuf_t* buf);
void SZ_Clear(sizebuf_t* buf);
void* SZ_GetSpace(sizebuf_t* buf, int length);
void SZ_Write(sizebuf_t* buf, const void* data, int length);
void SZ_Print(sizebuf_t* buf, const char* data); // strcats onto the sizebuf
