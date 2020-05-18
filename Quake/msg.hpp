/*
Copyright (C) 1996-2001 Id Software, Inc.
Copyright (C) 2002-2009 John Fitzgibbons and others
Copyright (C) 2010-2014 QuakeSpasm developers
Copyright (C) 2020-2020 Vittorio Romeo

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

#include "quakeglm_qvec3.hpp"

struct sizebuf_t;

extern int msg_readcount;
extern bool msg_badread; // set if a read goes beyond end of message

void MSG_WriteChar(sizebuf_t* sb, int c);
void MSG_WriteUnsignedChar(sizebuf_t* sb, unsigned char c);
void MSG_WriteByte(sizebuf_t* sb, int c);
void MSG_WriteShort(sizebuf_t* sb, int c);
void MSG_WriteLong(sizebuf_t* sb, int c);
void MSG_WriteFloat(sizebuf_t* sb, float f);
void MSG_WriteString(sizebuf_t* sb, const char* s);
void MSG_WriteCoord(sizebuf_t* sb, float f, unsigned int flags);
void MSG_WriteAngle(sizebuf_t* sb, float f, unsigned int flags);
void MSG_WriteAngle16(sizebuf_t* sb, float f, unsigned int flags);
void MSG_WriteVec3(sizebuf_t* sb, const qvec3& v, unsigned int flags);

void MSG_BeginReading();

[[nodiscard]] int MSG_ReadChar();
[[nodiscard]] unsigned char MSG_ReadUnsignedChar();
[[nodiscard]] int MSG_ReadByte();
[[nodiscard]] int MSG_ReadShort();
[[nodiscard]] int MSG_ReadLong();
[[nodiscard]] float MSG_ReadFloat();
[[nodiscard]] const char* MSG_ReadString();
[[nodiscard]] float MSG_ReadCoord(unsigned int flags);
[[nodiscard]] float MSG_ReadAngle(unsigned int flags);
[[nodiscard]] float MSG_ReadAngle16(unsigned int flags);
[[nodiscard]] qvec3 MSG_ReadVec3(unsigned int flags);
