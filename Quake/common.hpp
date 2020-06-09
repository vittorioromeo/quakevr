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
#include "quakedef_macros.hpp"
#include "q_stdinc.hpp"
#include "link.hpp"

// comndef.h  -- general definitions

#if defined(_WIN32)
#ifdef _MSC_VER
#pragma warning(disable : 4244)
/* 'argument'	: conversion from 'type1' to 'type2',
          possible loss of data */
#pragma warning(disable : 4305)
/* 'identifier'	: truncation from 'type1' to 'type2' */
/*  in our case, truncation from 'double' to 'float' */
#pragma warning(disable : 4267)
/* 'var'	: conversion from 'size_t' to 'type',
          possible loss of data (/Wp64 warning) */
#endif /* _MSC_VER */
#endif /* _WIN32 */

#undef min
#undef max
#define q_min(a, b) (((a) < (b)) ? (a) : (b))
#define q_max(a, b) (((a) > (b)) ? (a) : (b))
#define CLAMP(_minval, x, _maxval) \
    ((x) < (_minval) ? (_minval) : (x) > (_maxval) ? (_maxval) : (x))


//============================================================================

void Q_memset(void* dest, int fill, size_t count);
void Q_memcpy(void* dest, const void* src, size_t count);
int Q_memcmp(const void* m1, const void* m2, size_t count);
void Q_strcpy(char* dest, const char* src);
void Q_strncpy(char* dest, const char* src, int count);
int Q_strlen(const char* str);
char* Q_strrchr(const char* s, char c);
void Q_strcat(char* dest, const char* src);
int Q_strcmp(const char* s1, const char* s2);
int Q_strncmp(const char* s1, const char* s2, int count);
int Q_atoi(const char* str);
float Q_atof(const char* str);


#include "strl_fn.hpp"

/* locale-insensitive strcasecmp replacement functions: */
extern int q_strcasecmp(const char* s1, const char* s2);
extern int q_strncasecmp(const char* s1, const char* s2, size_t n);

/* locale-insensitive case-insensitive alternative to strstr */
extern char* q_strcasestr(const char* haystack, const char* needle);

/* locale-insensitive strlwr/upr replacement functions: */
extern char* q_strlwr(char* str);
extern char* q_strupr(char* str);

/* snprintf, vsnprintf : always use our versions. */
extern int q_snprintf(char* str, size_t size, const char* format, ...)
    FUNC_PRINTF(3, 4);
extern int q_vsnprintf(char* str, size_t size, const char* format, va_list args)
    FUNC_PRINTF(3, 0);

//============================================================================

extern char com_token[1024];
extern bool com_eof;

const char* COM_Parse(const char* data);


extern int com_argc;
extern char** com_argv;

extern int safemode;
/* safe mode: in true, the engine will behave as if one
   of these arguments were actually on the command line:
   -nosound, -nocdaudio, -nomidi, -stdvid, -dibonly,
   -nomouse, -nojoy, -nolan
 */

int COM_CheckParm(const char* parm);
int COM_CheckParmNext(int last, const char* parm); // QSS

void COM_Init();
void COM_InitArgv(int argc, char** argv);
void COM_InitFilesystem();

const char* COM_SkipPath(const char* pathname);
void COM_StripExtension(const char* in, char* out, size_t outsize);
void COM_FileBase(const char* in, char* out, size_t outsize);
void COM_AddExtension(char* path, const char* extension, size_t len);
bool COM_DownloadNameOkay(const char* filename); // QSS

#if 0 /* COM_DefaultExtension can be dangerous */
void COM_DefaultExtension (char *path, const char *extension, size_t len);
#endif
const char* COM_FileGetExtension(const char* in); /* doesn't return nullptr */
void COM_ExtractExtension(const char* in, char* out, size_t outsize);
void COM_CreatePath(char* path);

char* va(const char* format, ...) FUNC_PRINTF(1, 2);
// does a varargs printf into a temp buffer


//============================================================================

// QUAKEFS
typedef struct
{
    char name[MAX_QPATH];
    int filepos, filelen;
} packfile_t;

typedef struct pack_s
{
    char filename[MAX_OSPATH];
    int handle;
    int numfiles;
    packfile_t* files;
} pack_t;

typedef struct searchpath_s
{
    unsigned int path_id; // identifier assigned to the game directory
                          // Note that <install_dir>/game1 and
                          // <userdir>/game1 have the same id.
    char filename[MAX_OSPATH];
    pack_t* pack; // only one of filename / pack will be used
    struct searchpath_s* next;
} searchpath_t;

extern searchpath_t* com_searchpaths;
extern searchpath_t* com_base_searchpaths;

extern int com_filesize;
struct cache_user_s;

extern char com_basedir[MAX_OSPATH];
extern char com_gamedir[MAX_OSPATH];
extern int file_from_pak; // global indicating that file came from a pak

// QSS
const char* COM_GetGameNames(bool full);
bool COM_GameDirMatches(const char* tdirs);

void COM_WriteFile(const char* filename, const void* data, int len);
int COM_OpenFile(const char* filename, int* handle, unsigned int* path_id);
int COM_FOpenFile(const char* filename, FILE** file, unsigned int* path_id);
bool COM_FileExists(const char* filename, unsigned int* path_id);
void COM_CloseFile(int h);

// these procedures open a file using COM_FindFile and loads it into a proper
// buffer. the buffer is allocated with a total size of com_filesize + 1. the
// procedures differ by their buffer allocation method.
byte* COM_LoadStackFile(
    const char* path, void* buffer, int bufsize, unsigned int* path_id);
// uses the specified stack stack buffer with the specified size
// of bufsize. if bufsize is too short, uses temp hunk. the bufsize
// must include the +1
byte* COM_LoadTempFile(const char* path, unsigned int* path_id);
// allocates the buffer on the temp hunk.
byte* COM_LoadHunkFile(const char* path, unsigned int* path_id);
// allocates the buffer on the hunk.
byte* COM_LoadZoneFile(const char* path, unsigned int* path_id);
// allocates the buffer on the zone.
void COM_LoadCacheFile(
    const char* path, struct cache_user_s* cu, unsigned int* path_id);
// uses cache mem for allocating the buffer.
byte* COM_LoadMallocFile(const char* path, unsigned int* path_id);
// allocates the buffer on the system mem (malloc).

// Opens the given path directly, ignoring search paths.
// Returns nullptr on failure, or else a '\0'-terminated malloc'ed buffer.
// Loads in "t" mode so CRLF to LF translation is performed on Windows.
byte* COM_LoadMallocFile_TextMode_OSPath(const char* path, long* len_out);

// Attempts to parse an timestamp, followed by a newline.
// Returns advanced buffer position.
// Doesn't signal parsing failure, but this is not needed for savegame loading.
const char* COM_ParseTimestampNewline(const char* buffer);

// Attempts to parse an int, followed by a newline.
// Returns advanced buffer position.
// Doesn't signal parsing failure, but this is not needed for savegame loading.
const char* COM_ParseIntNewline(const char* buffer, int* value);

// Attempts to parse a float followed by a newline.
// Returns advanced buffer position.
const char* COM_ParseFloatNewline(const char* buffer, float* value);

// Parse a string of non-whitespace into com_token, then tries to consume a
// newline. Returns advanced buffer position.
const char* COM_ParseStringNewline(const char* buffer);

extern struct cvar_t registered;
extern bool standard_quake, rogue, hipnotic;
/* if true, run in fitzquake mode disabling custom quakespasm hacks */
