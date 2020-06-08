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

#include <cstdio>

/* The following FS_*() stdio replacements are necessary if one is
 * to perform non-sequential reads on files reopened on pak files
 * because we need the bookkeeping about file start/end positions.
 * Allocating and filling in the fshandle_t structure is the users'
 * responsibility when the file is initially opened. */

typedef struct _fshandle_t
{
    FILE* file;
    bool pak;    /* is the file read from a pak */
    long start;  /* file or data start position */
    long length; /* file or data size */
    long pos;    /* current position relative to start */
} fshandle_t;

size_t FS_fread(void* ptr, size_t size, size_t nmemb, fshandle_t* fh);
int FS_fseek(fshandle_t* fh, long offset, int whence);
long FS_ftell(fshandle_t* fh);
void FS_rewind(fshandle_t* fh);
int FS_feof(fshandle_t* fh);
int FS_ferror(fshandle_t* fh);
int FS_fclose(fshandle_t* fh);
int FS_fgetc(fshandle_t* fh);
char* FS_fgets(char* s, int size, fshandle_t* fh);
long FS_filelength(fshandle_t* fh);

