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

// common.c -- misc functions used in client and server

#include "common.hpp"
#include "host.hpp"
#include "q_ctype.hpp"
#include "cmd.hpp"
#include "console.hpp"
#include "zone.hpp"
#include "quakeparms.hpp"
#include "crc.hpp"
#include "net.hpp"
#include "mathlib.hpp"
#include "glquake.hpp"
#include "sys.hpp"
#include "zone.hpp"
#include "sizebuf.hpp"
#include "msg.hpp"
#include "vr.hpp"
#include "byteorder.hpp"
#include "vr_cvars.hpp"
#include "sys.hpp"
#include "client.hpp"
#include "draw.hpp"
#include "gl_texmgr.hpp"

#include <cerrno>
#include <string_view>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <winbase.h>
#include <winnt.h>

#else
#include <fnmatch.h>

#ifndef FNM_CASEFOLD
#define FNM_CASEFOLD 0 // not available. I guess we're not on gnu/linux
#endif

#include <dirent.h>
#endif

static char* largv[MAX_NUM_ARGVS + 1];
static char argvdummy[] = " ";

int safemode;

cvar_t registered = {"registered", "1",
    CVAR_ROM}; /* set to correct value in COM_CheckRegistered() */
cvar_t cmdline = {
    "cmdline", "", CVAR_ROM /*|CVAR_SERVERINFO*/}; /* sending cmdline upon
                                                      CCREQ_RULE_INFO is evil */

// QSS
cvar_t allow_download = {"allow_download",
    "1"}; /*set to 0 to block file downloads, both client+server*/

static bool com_modified; // set true if using non-id files

static void COM_Path_f();

// if a packfile directory differs from this, it is assumed to be hacked
#define PAK0_COUNT 339      /* id1/pak0.pak - v1.0x */
#define PAK0_CRC_V100 13900 /* id1/pak0.pak - v1.00 */
#define PAK0_CRC_V101 62751 /* id1/pak0.pak - v1.01 */
#define PAK0_CRC_V106 32981 /* id1/pak0.pak - v1.06 */
#define PAK0_CRC (PAK0_CRC_V106)
#define PAK0_COUNT_V091 308 /* id1/pak0.pak - v0.91/0.92, not supported */
#define PAK0_CRC_V091 28804 /* id1/pak0.pak - v0.91/0.92, not supported */

char com_token[1024];
int com_argc;
char** com_argv;

#define CMDLINE_LENGTH 256 /* johnfitz -- mirrored in cmd.c */
char com_cmdline[CMDLINE_LENGTH];

// TODO VR: (P1) remove rogue/hipnotic special cases
bool standard_quake = true, rogue, hipnotic;

// this graphic needs to be in the pak file to use registered features
static unsigned short pop[] = {0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x6600, 0x0000, 0x0000, 0x0000, 0x6600,
    0x0000, 0x0000, 0x0066, 0x0000, 0x0000, 0x0000, 0x0000, 0x0067, 0x0000,
    0x0000, 0x6665, 0x0000, 0x0000, 0x0000, 0x0000, 0x0065, 0x6600, 0x0063,
    0x6561, 0x0000, 0x0000, 0x0000, 0x0000, 0x0061, 0x6563, 0x0064, 0x6561,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0061, 0x6564, 0x0064, 0x6564, 0x0000,
    0x6469, 0x6969, 0x6400, 0x0064, 0x6564, 0x0063, 0x6568, 0x6200, 0x0064,
    0x6864, 0x0000, 0x6268, 0x6563, 0x0000, 0x6567, 0x6963, 0x0064, 0x6764,
    0x0063, 0x6967, 0x6500, 0x0000, 0x6266, 0x6769, 0x6a68, 0x6768, 0x6a69,
    0x6766, 0x6200, 0x0000, 0x0062, 0x6566, 0x6666, 0x6666, 0x6666, 0x6562,
    0x0000, 0x0000, 0x0000, 0x0062, 0x6364, 0x6664, 0x6362, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0062, 0x6662, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0061, 0x6661, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x6500, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x6400, 0x0000, 0x0000, 0x0000};

/*

All of Quake's data access is through a hierchal file system, but the contents
of the file system can be transparently merged from several sources.

The "base directory" is the path to the directory holding the quake.exe and all
game directories.  The sys_* files pass this to host_init in
quakeparms_t->basedir. This can be overridden with the "-basedir" command line
parm to allow code debugging in a different directory.  The base directory is
only used during filesystem initialization.

The "game directory" is the first tree on the search path and directory that all
generated files (savegames, screenshots, demos, config files) will be saved to.
This can be overridden with the "-game" command line parameter.  The game
directory can never be changed while quake is executing.  This is a precacution
against having a malicious server instruct clients to write files over areas
they shouldn't.

The "cache directory" is only used during development to save network bandwidth,
especially over ISDN / T1 lines.  If there is a cache directory specified, when
a file is found by the normal search path, it will be mirrored into the cache
directory, then opened there.

FIXME:
The file "parms.txt" will be read out of the game directory and appended to the
current command line arguments to allow different games to initialize startup
parms differently.  This could be used to add a "-sspeed 22050" for the high
quality sound edition.  Because they are added at the end, they will not
override an explicit setting on the original command line.

*/

//============================================================================



/*
============================================================================

                    LIBRARY REPLACEMENT FUNCTIONS

============================================================================
*/

int q_strcasecmp(const char* s1, const char* s2)
{
    const char* p1 = s1;
    const char* p2 = s2;
    char c1;
    char c2;

    if(p1 == p2)
    {
        return 0;
    }

    do
    {
        c1 = q_tolower(*p1++);
        c2 = q_tolower(*p2++);
        if(c1 == '\0')
        {
            break;
        }
    } while(c1 == c2);

    return (int)(c1 - c2);
}

int q_strncasecmp(const char* s1, const char* s2, size_t n)
{
    const char* p1 = s1;
    const char* p2 = s2;
    char c1;
    char c2;

    if(p1 == p2 || n == 0)
    {
        return 0;
    }

    do
    {
        c1 = q_tolower(*p1++);
        c2 = q_tolower(*p2++);
        if(c1 == '\0' || c1 != c2)
        {
            break;
        }
    } while(--n > 0);

    return (int)(c1 - c2);
}

// spike -- grabbed this from fte, because its useful to me
char* q_strcasestr(const char* haystack, const char* needle)
{
    int c1;
    int c2;
    int c2f;
    int i;
    c2f = *needle;
    if(c2f >= 'a' && c2f <= 'z')
    {
        c2f -= ('a' - 'A');
    }
    if(!c2f)
    {
        return (char*)haystack;
    }
    while(true)
    {
        c1 = *haystack;
        if(!c1)
        {
            return nullptr;
        }
        if(c1 >= 'a' && c1 <= 'z')
        {
            c1 -= ('a' - 'A');
        }
        if(c1 == c2f)
        {
            for(i = 1;; i++)
            {
                c1 = haystack[i];
                c2 = needle[i];
                if(c1 >= 'a' && c1 <= 'z')
                {
                    c1 -= ('a' - 'A');
                }
                if(c2 >= 'a' && c2 <= 'z')
                {
                    c2 -= ('a' - 'A');
                }
                if(!c2)
                {
                    return (char*)haystack; // end of needle means we found a
                }
                // complete match
                if(!c1)
                {
                    // end of haystack means we can't possibly find needle
                    // in it any more
                    return nullptr;
                }
                if(c1 != c2)
                {
                    // mismatch means no match starting at haystack[0]
                    break;
                }
            }
        }
        haystack++;
    }
    return nullptr; // didn't find it
}

char* q_strlwr(char* str)
{
    char* c;
    c = str;
    while(*c)
    {
        *c = q_tolower(*c);
        c++;
    }
    return str;
}

char* q_strupr(char* str)
{
    char* c;
    c = str;
    while(*c)
    {
        *c = q_toupper(*c);
        c++;
    }
    return str;
}

/* platform dependant (v)snprintf function names: */
#if defined(_WIN32)
#define snprintf_func _snprintf
#define vsnprintf_func _vsnprintf
#else
#define snprintf_func snprintf
#define vsnprintf_func vsnprintf
#endif

int q_vsnprintf(char* str, size_t size, const char* format, va_list args)
{
    int ret;

    ret = vsnprintf_func(str, size, format, args);

    if(ret < 0)
    {
        ret = (int)size;
    }
    if(size == 0)
    { /* no buffer */
        return ret;
    }
    if((size_t)ret >= size)
    {
        str[size - 1] = '\0';
    }

    return ret;
}

int q_snprintf(char* str, size_t size, const char* format, ...)
{
    int ret;
    va_list argptr;

    va_start(argptr, format);
    ret = q_vsnprintf(str, size, format, argptr);
    va_end(argptr);

    return ret;
}

void Q_memset(void* dest, int fill, size_t count)
{
    size_t i;

    if((((size_t)dest | count) & 3) == 0)
    {
        count >>= 2;
        fill = fill | (fill << 8) | (fill << 16) | (fill << 24);
        for(i = 0; i < count; i++)
        {
            ((int*)dest)[i] = fill;
        }
    }
    else
    {
        for(i = 0; i < count; i++)
        {
            ((byte*)dest)[i] = fill;
        }
    }
}

void Q_memcpy(void* dest, const void* src, size_t count)
{
    size_t i;

    if((((size_t)dest | (size_t)src | count) & 3) == 0)
    {
        count >>= 2;
        for(i = 0; i < count; i++)
        {
            ((int*)dest)[i] = ((int*)src)[i];
        }
    }
    else
    {
        for(i = 0; i < count; i++)
        {
            ((byte*)dest)[i] = ((byte*)src)[i];
        }
    }
}

int Q_memcmp(const void* m1, const void* m2, size_t count)
{
    while(count)
    {
        count--;
        if(((byte*)m1)[count] != ((byte*)m2)[count])
        {
            return -1;
        }
    }
    return 0;
}

void Q_strcpy(char* dest, const char* src)
{
    while(*src)
    {
        *dest++ = *src++;
    }
    *dest++ = 0;
}

void Q_strncpy(char* dest, const char* src, int count)
{
    while(*src && count--)
    {
        *dest++ = *src++;
    }
    if(count)
    {
        *dest++ = 0;
    }
}

int Q_strlen(const char* str)
{
    int count;

    count = 0;
    while(str[count])
    {
        count++;
    }

    return count;
}

char* Q_strrchr(const char* s, char c)
{
    int len = Q_strlen(s);
    s += len;
    while(len--)
    {
        if(*--s == c)
        {
            return (char*)s;
        }
    }
    return nullptr;
}

void Q_strcat(char* dest, const char* src)
{
    dest += Q_strlen(dest);
    Q_strcpy(dest, src);
}

int Q_strcmp(const char* s1, const char* s2)
{
    while(true)
    {
        if(*s1 != *s2)
        {
            return -1; // strings not equal
        }
        if(!*s1)
        {
            return 0; // strings are equal
        }
        s1++;
        s2++;
    }

    return -1;
}

int Q_strncmp(const char* s1, const char* s2, int count)
{
    while(true)
    {
        if(!count--)
        {
            return 0;
        }
        if(*s1 != *s2)
        {
            return -1; // strings not equal
        }
        if(!*s1)
        {
            return 0; // strings are equal
        }
        s1++;
        s2++;
    }

    return -1;
}

int Q_atoi(const char* str)
{
    int val;
    int sign;
    int c;

    if(*str == '-')
    {
        sign = -1;
        str++;
    }
    else
    {
        sign = 1;
    }

    val = 0;

    //
    // check for hex
    //
    if(str[0] == '0' && (str[1] == 'x' || str[1] == 'X'))
    {
        str += 2;
        while(true)
        {
            c = *str++;
            if(c >= '0' && c <= '9')
            {
                val = (val << 4) + c - '0';
            }
            else if(c >= 'a' && c <= 'f')
            {
                val = (val << 4) + c - 'a' + 10;
            }
            else if(c >= 'A' && c <= 'F')
            {
                val = (val << 4) + c - 'A' + 10;
            }
            else
            {
                return val * sign;
            }
        }
    }

    //
    // check for character
    //
    if(str[0] == '\'')
    {
        return sign * str[1];
    }

    //
    // assume decimal
    //
    while(true)
    {
        c = *str++;
        if(c < '0' || c > '9')
        {
            return val * sign;
        }
        val = val * 10 + c - '0';
    }

    return 0;
}


float Q_atof(const char* str)
{
    double val;
    int sign;
    int c;
    int decimal;
    int total;

    if(*str == '-')
    {
        sign = -1;
        str++;
    }
    else
    {
        sign = 1;
    }

    val = 0;

    //
    // check for hex
    //
    if(str[0] == '0' && (str[1] == 'x' || str[1] == 'X'))
    {
        str += 2;
        while(true)
        {
            c = *str++;
            if(c >= '0' && c <= '9')
            {
                val = (val * 16) + c - '0';
            }
            else if(c >= 'a' && c <= 'f')
            {
                val = (val * 16) + c - 'a' + 10;
            }
            else if(c >= 'A' && c <= 'F')
            {
                val = (val * 16) + c - 'A' + 10;
            }
            else
            {
                return val * sign;
            }
        }
    }

    //
    // check for character
    //
    if(str[0] == '\'')
    {
        return sign * str[1];
    }

    //
    // assume decimal
    //
    decimal = -1;
    total = 0;
    while(true)
    {
        c = *str++;
        if(c == '.')
        {
            decimal = total;
            continue;
        }
        if(c < '0' || c > '9')
        {
            break;
        }
        val = val * 10 + c - '0';
        total++;
    }

    if(decimal == -1)
    {
        return val * sign;
    }
    while(total > decimal)
    {
        val /= 10;
        total--;
    }

    return val * sign;
}


/*
============
COM_SkipPath
============
*/
const char* COM_SkipPath(const char* pathname)
{
    const char* last;

    last = pathname;
    while(*pathname)
    {
        if(*pathname == '/')
        {
            last = pathname + 1;
        }
        pathname++;
    }
    return last;
}

/*
============
COM_StripExtension
============
*/
void COM_StripExtension(const char* in, char* out, size_t outsize)
{
    int length;

    if(!*in)
    {
        *out = '\0';
        return;
    }
    if(in != out)
    { /* copy when not in-place editing */
        q_strlcpy(out, in, outsize);
    }
    length = (int)strlen(out) - 1;
    while(length > 0 && out[length] != '.')
    {
        --length;
        if(out[length] == '/' || out[length] == '\\')
        {
            return; /* no extension */
        }
    }
    if(length > 0)
    {
        out[length] = '\0';
    }
}

/*
============
COM_FileGetExtension - doesn't return nullptr
============
*/
const char* COM_FileGetExtension(const char* in)
{
    const char* src;
    size_t len;

    len = strlen(in);
    if(len < 2)
    { /* nothing meaningful */
        return "";
    }

    src = in + len - 1;
    while(src != in && src[-1] != '.')
    {
        src--;
    }
    if(src == in || strchr(src, '/') != nullptr || strchr(src, '\\') != nullptr)
    {
        return ""; /* no extension, or parent directory has a dot */
    }

    return src;
}

/*
============
COM_ExtractExtension
============
*/
void COM_ExtractExtension(const char* in, char* out, size_t outsize)
{
    const char* ext = COM_FileGetExtension(in);
    if(!*ext)
    {
        *out = '\0';
    }
    else
    {
        q_strlcpy(out, ext, outsize);
    }
}

/*
============
COM_FileBase
take 'somedir/otherdir/filename.ext',
write only 'filename' to the output
============
*/
void COM_FileBase(const char* in, char* out, size_t outsize)
{
    const char* dot;
    const char* slash;
    const char* s;

    s = in;
    slash = in;
    dot = nullptr;
    while(*s)
    {
        if(*s == '/')
        {
            slash = s + 1;
        }
        if(*s == '.')
        {
            dot = s;
        }
        s++;
    }
    if(dot == nullptr)
    {
        dot = s;
    }

    if(dot - slash < 2)
    {
        q_strlcpy(out, "?model?", outsize);
    }
    else
    {
        size_t len = dot - slash;
        if(len >= outsize)
        {
            len = outsize - 1;
        }
        memcpy(out, slash, len);
        out[len] = '\0';
    }
}

/*
==================
COM_DefaultExtension
if path doesn't have a .EXT, append extension
(extension should include the leading ".")
==================
*/
#if 0 /* can be dangerous */
void COM_DefaultExtension (char *path, const char *extension, size_t len)
{
	char	*src;

	if (!*path) return;
	src = path + strlen(path) - 1;

	while (*src != '/' && *src != '\\' && src != path)
	{
		if (*src == '.')
			return; // it has an extension
		src--;
	}

	q_strlcat(path, extension, len);
}
#endif

/*
==================
COM_AddExtension
if path extension doesn't match .EXT, append it
(extension should include the leading ".")
==================
*/
void COM_AddExtension(char* path, const char* extension, size_t len)
{
    if(strcmp(COM_FileGetExtension(path), extension + 1) != 0)
    {
        q_strlcat(path, extension, len);
    }
}

// QSS

/*
spike -- this function simply says whether a filename is acceptable for
downloading (used by both client+server)
*/
bool COM_DownloadNameOkay(const char* filename)
{
    if(!allow_download.value)
    {
        return false;
    }

    // quickly test the prefix to ensure that its in one of the allowed subdirs
    if(strncmp(filename, "sound/", 6) && strncmp(filename, "progs/", 6) &&
        strncmp(filename, "maps/", 5) && strncmp(filename, "models/", 7))
    {
        return false;
    }
    // windows paths are NOT permitted, nor are alternative data streams, nor
    // wildcards, and double quotes are always bad(which allows for spaces)
    if(strchr(filename, '\\') || strchr(filename, ':') ||
        strchr(filename, '*') || strchr(filename, '?') ||
        strchr(filename, '\"'))
    {
        return false;
    }
    // some operating systems interpret this as 'parent directory'
    if(strstr(filename, "//"))
    {
        return false;
    }
    // block unix hidden files, also blocks relative paths.
    if(*filename == '.' || strstr(filename, "/."))
    {
        return false;
    }
    // test the extension to ensure that its in one of the allowed file types
    //(no .dll, .so, .com, .exe, .bat, .vbs, .xls, .doc, etc please)
    // also don't allow config files.
    filename = COM_FileGetExtension(filename);
    if(
        // model formats
        q_strcasecmp(filename, "bsp") && q_strcasecmp(filename, "mdl") &&
        q_strcasecmp(filename, "iqm") && // in case we ever support these later
        q_strcasecmp(filename, "md3") && q_strcasecmp(filename, "spr") &&
        q_strcasecmp(filename, "spr32") &&
        // audio formats
        q_strcasecmp(filename, "wav") && q_strcasecmp(filename, "ogg") &&
        // image formats (if we ever need that)
        q_strcasecmp(filename, "tga") && q_strcasecmp(filename, "png") &&
        // misc stuff
        q_strcasecmp(filename, "lux") && q_strcasecmp(filename, "lit2") &&
        q_strcasecmp(filename, "lit"))
    {
        return false;
    }
    // okay, well, we didn't throw a hissy fit, so whatever dude, go ahead and
    // download
    return true;
}


/*
==============
COM_Parse

Parse a token out of a string
==============
*/
const char* COM_Parse(const char* data)
{
    int c;
    int len;

    len = 0;
    com_token[0] = 0;

    if(!data)
    {
        return nullptr;
    }

// skip whitespace
skipwhite:
    while((c = *data) <= ' ')
    {
        if(c == 0)
        {
            return nullptr; // end of file
        }
        data++;
    }

    // skip // comments
    if(c == '/' && data[1] == '/')
    {
        while(*data && *data != '\n')
        {
            data++;
        }
        goto skipwhite;
    }

    // skip /*..*/ comments
    if(c == '/' && data[1] == '*')
    {
        data += 2;
        while(*data && !(*data == '*' && data[1] == '/'))
        {
            data++;
        }
        if(*data)
        {
            data += 2;
        }
        goto skipwhite;
    }

    // handle quoted strings specially
    if(c == '\"')
    {
        data++;
        while(true)
        {
            if((c = *data) != 0)
            {
                ++data;
            }
            if(c == '\"' || !c)
            {
                com_token[len] = 0;
                return data;
            }
            com_token[len] = c;
            len++;
        }
    }

    // parse single characters
    if(c == '{' || c == '}' || c == '(' || c == ')' || c == '\'' || c == ':')
    {
        com_token[len] = c;
        len++;
        com_token[len] = 0;
        return data + 1;
    }

    // parse a regular word
    do
    {
        com_token[len] = c;
        data++;
        len++;
        c = *data;
        /* commented out the check for ':' so that ip:port works */
        if(c == '{' || c == '}' || c == '(' || c == ')' ||
            c == '\'' /* || c == ':' */)
        {
            break;
        }
    } while(c > 32);

    com_token[len] = 0;
    return data;
}

// QSS
/*
================
COM_CheckParm

Returns the position (1 to argc-1) in the program's argument list
where the given parameter apears, or 0 if not present
================
*/
int COM_CheckParmNext(int last, const char* parm)
{
    for(int i = last + 1; i < com_argc; i++)
    {
        if(!com_argv[i])
        {
            continue; // NEXTSTEP sometimes clears appkit vars.
        }
        if(!Q_strcmp(parm, com_argv[i]))
        {
            return i;
        }
    }

    return 0;
}

int COM_CheckParm(const char* parm)
{
    return COM_CheckParmNext(0, parm);
}
// ---


/*
================
COM_CheckRegistered

Looks for the pop.txt file and verifies it.
Sets the "registered" cvar.
Immediately exits out if an alternate game was attempted to be started without
being registered.
================
*/
static void COM_CheckRegistered()
{
    int h;
    unsigned short check[128];
    int i;

    // TODO VR: (P1) could use something similar to detect mission packs
    COM_OpenFile("gfx/pop.lmp", &h, nullptr);

    if(h == -1)
    {
        Cvar_SetROM("registered", "0");
        Con_Printf("Playing shareware version.\n");
        if(com_modified)
        {
            Sys_Error(
                "You must have the registered version to use modified "
                "games.\n\n"
                "Basedir is: %s\n\n"
                "Check that this has an " GAMENAME
                " subdirectory containing pak0.pak and pak1.pak, "
                "or use the -basedir command-line option to specify another "
                "directory.",
                com_basedir);
        }
        return;
    }

    Sys_FileRead(h, check, sizeof(check));
    COM_CloseFile(h);

    for(i = 0; i < 128; i++)
    {
        if(pop[i] != (unsigned short)BigShort(check[i]))
        {
            Sys_Error("Corrupted data file.");
        }
    }

    for(i = 0; com_cmdline[i]; i++)
    {
        if(com_cmdline[i] != ' ')
        {
            break;
        }
    }

    Cvar_SetROM("cmdline", &com_cmdline[i]);
    Cvar_SetROM("registered", "1");
    Con_Printf("Playing registered version.\n");
}


/*
================
COM_InitArgv
================
*/
void COM_InitArgv(int argc, char** argv)
{
    int i;
    int j;
    int n;

    // reconstitute the command line for the cmdline externally visible cvar
    n = 0;

    for(j = 0; (j < MAX_NUM_ARGVS) && (j < argc); j++)
    {
        i = 0;

        while((n < (CMDLINE_LENGTH - 1)) && argv[j][i])
        {
            com_cmdline[n++] = argv[j][i++];
        }

        if(n < (CMDLINE_LENGTH - 1))
        {
            com_cmdline[n++] = ' ';
        }
        else
        {
            break;
        }
    }

    if(n > 0 && com_cmdline[n - 1] == ' ')
    {
        com_cmdline[n - 1] = 0; // johnfitz -- kill the trailing space
    }

    Con_Printf("Command line: %s\n", com_cmdline);

    for(com_argc = 0; (com_argc < MAX_NUM_ARGVS) && (com_argc < argc);
        com_argc++)
    {
        largv[com_argc] = argv[com_argc];
        if(!Q_strcmp("-safe", argv[com_argc]))
        {
            safemode = 1;
        }
    }

    largv[com_argc] = argvdummy;
    com_argv = largv;

    if(COM_CheckParm("-rogue"))
    {
        rogue = true;
        standard_quake = false;
    }

    if(COM_CheckParm("-hipnotic") ||
        COM_CheckParm("-quoth")) // johnfitz -- "-quoth" support
    {
        hipnotic = true;
        standard_quake = false;
    }
}

/*
================
Test_f -- johnfitz
================
*/
#ifdef _DEBUG
static void FitzTest_f()
{
}
#endif

entity_state_t nullentitystate;
static void COM_SetupNullState()
{
    // the null state has some specific default values
    //	nullentitystate.drawflags = /*SCALE_ORIGIN_ORIGIN*/96;
    nullentitystate.colormod[0] = 32;
    nullentitystate.colormod[1] = 32;
    nullentitystate.colormod[2] = 32;
    //	nullentitystate.glowmod[0] = 32;
    //	nullentitystate.glowmod[1] = 32;
    //	nullentitystate.glowmod[2] = 32;
    nullentitystate.alpha =
        0; // fte has 255 by default, with 0 for invisible. fitz uses 1 for
           // invisible, 0 default, and 255=full alpha

    nullentitystate.scale = 16;


    //	nullentitystate.solidsize = 0;//ES_SOLID_BSP;
}

/*
================
COM_Init
================
*/
void COM_Init()
{
    ByteOrder_Init();

#ifdef _DEBUG
    Cmd_AddCommand("fitztest", FitzTest_f); // johnfitz
#endif

    COM_SetupNullState();
}


/*
============
va

does a varargs printf into a temp buffer. cycles between
4 different static buffers. the number of buffers cycled
is defined in VA_NUM_BUFFS.
FIXME: make this buffer size safe someday
============
*/
#define VA_NUM_BUFFS 4
#define VA_BUFFERLEN 1024

static char* get_va_buffer()
{
    static char va_buffers[VA_NUM_BUFFS][VA_BUFFERLEN];
    static int buffer_idx = 0;
    buffer_idx = (buffer_idx + 1) & (VA_NUM_BUFFS - 1);
    return va_buffers[buffer_idx];
}

char* va(const char* format, ...)
{
    va_list argptr;
    char* va_buf;

    va_buf = get_va_buffer();
    va_start(argptr, format);
    q_vsnprintf(va_buf, VA_BUFFERLEN, format, argptr);
    va_end(argptr);

    return va_buf;
}

/*
=============================================================================

QUAKE FILESYSTEM

=============================================================================
*/

int com_filesize;


//
// on-disk pakfile
//
typedef struct
{
    char name[56];
    int filepos, filelen;
} dpackfile_t;

typedef struct
{
    char id[4];
    int dirofs;
    int dirlen;
} dpackheader_t;

#define MAX_FILES_IN_PACK 2048

// QSS
char com_gamenames[1024]; // eg: "hipnotic;quoth;warp", no id1, no private stuff

char com_gamedir[MAX_OSPATH];
char com_basedir[MAX_OSPATH];
int file_from_pak; // ZOID: global indicating that file came from a pak

searchpath_t* com_searchpaths;
searchpath_t* com_base_searchpaths;

/*
============
COM_Path_f
============
*/
static void COM_Path_f()
{
    searchpath_t* s;

    Con_Printf("Current search path:\n");
    for(s = com_searchpaths; s; s = s->next)
    {
        if(s->pack)
        {
            Con_Printf("%s (%i files)\n", s->pack->filename, s->pack->numfiles);
        }
        else
        {
            Con_Printf("%s\n", s->filename);
        }
    }
}

/*
============
COM_WriteFile

The filename will be prefixed by the current game directory
============
*/
void COM_WriteFile(const char* filename, const void* data, int len)
{
    int handle;
    char name[MAX_OSPATH];

    Sys_mkdir(com_gamedir); // johnfitz -- if we've switched to a nonexistant
                            // gamedir, create it now so we don't crash

    q_snprintf(name, sizeof(name), "%s/%s", com_gamedir, filename);

    handle = Sys_FileOpenWrite(name);
    if(handle == -1)
    {
        Sys_Printf("COM_WriteFile: failed on %s\n", name);
        return;
    }

    Sys_Printf("COM_WriteFile: %s\n", name);
    Sys_FileWrite(handle, data, len);
    Sys_FileClose(handle);
}

/*
============
COM_CreatePath
============
*/
void COM_CreatePath(char* path)
{
    char* ofs;

    for(ofs = path + 1; *ofs; ofs++)
    {
        if(*ofs == '/')
        {
            // create the directory
            *ofs = 0;
            Sys_mkdir(path);
            *ofs = '/';
        }
    }
}

/*
================
COM_filelength
================
*/
long COM_filelength(FILE* f)
{
    long pos;
    long end;

    pos = ftell(f);
    fseek(f, 0, SEEK_END);
    end = ftell(f);
    fseek(f, pos, SEEK_SET);

    return end;
}

/*
===========
COM_FindFile

Finds the file in the search path.
Sets com_filesize and one of handle or file
If neither of file or handle is set, this
can be used for detecting a file's presence.
===========
*/
static int COM_FindFile(
    const char* filename, int* handle, FILE** file, unsigned int* path_id)
{
    if(file && handle)
    {
        Sys_Error("COM_FindFile: both handle and file set");
    }

    file_from_pak = 0;

    //
    // search through the path, one element at a time
    //
    for(searchpath_t* search = com_searchpaths; search; search = search->next)
    {
        if(search->pack) /* look through all the pak file elements */
        {
            pack_t* pak = search->pack;
            for(int i = 0; i < pak->numfiles; i++)
            {
                if(strcmp(pak->files[i].name, filename) != 0)
                {
                    continue;
                }

                // VR: This hack allows multiple "start.bsp" maps to coexist.
                // The user can decide which one is loaded by setting a CVar.
                const auto extractedPakName = VR_ExtractPakName(*pak);
                if(std::strcmp(filename, "maps/start.bsp") == 0 &&
                    extractedPakName != VR_GetActiveStartPakName() &&
                    extractedPakName != "pak0")
                {
                    continue;
                }

                // found it!
                com_filesize = pak->files[i].filelen;
                file_from_pak = 1;

                if(path_id)
                {
                    *path_id = search->path_id;
                }

                if(handle)
                {
                    if(pak->files[i].deflatedsize)
                    {
                        FILE* f;
                        f = fopen(pak->filename, "rb");
                        if(f)
                        {
                            fseek(f, pak->files[i].filepos, SEEK_SET);
                            f = FSZIP_Deflate(f, pak->files[i].deflatedsize,
                                pak->files[i].filelen);
                            *handle = Sys_FileOpenStdio(f);
                        }
                        else
                        { // error!
                            com_filesize = -1;
                            *handle = -1;
                        }
                    }
                    else
                    {
                        *handle = pak->handle;
                        Sys_FileSeek(pak->handle, pak->files[i].filepos);
                    }
                    return com_filesize;
                }
                else if(file)
                { /* open a new file on the pakfile */

                    *file = fopen(pak->filename, "rb");

                    if(*file)
                    {
                        fseek(*file, pak->files[i].filepos, SEEK_SET);
                        if(pak->files[i].deflatedsize)
                            *file =
                                FSZIP_Deflate(*file, pak->files[i].deflatedsize,
                                    pak->files[i].filelen);
                    }

                    return com_filesize;
                }
                else /* for COM_FileExists() */
                {
                    return com_filesize;
                }
            }
        }
        else /* check a file in the directory tree */
        {
            char netpath[MAX_OSPATH];
            q_snprintf(
                netpath, sizeof(netpath), "%s/%s", search->filename, filename);

            const int findtime = Sys_FileTime(netpath);
            if(findtime == -1)
            {
                continue;
            }

            if(path_id)
            {
                *path_id = search->path_id;
            }

            if(handle)
            {
                int i;
                com_filesize = Sys_FileOpenRead(netpath, &i);
                *handle = i;
                return com_filesize;
            }
            else if(file)
            {
                *file = fopen(netpath, "rb");
                com_filesize = (*file == nullptr) ? -1 : COM_filelength(*file);
                return com_filesize;
            }
            else
            {
                return 0; /* dummy valid value for COM_FileExists() */
            }
        }
    }

    const char* ext = COM_FileGetExtension(filename);
    if(strcmp(ext, "pcx") != 0 && strcmp(ext, "tga") != 0 &&
        strcmp(ext, "png") != 0 && strcmp(ext, "jpg") != 0 &&
        strcmp(ext, "jpeg") != 0 && strcmp(ext, "lit") != 0 &&
        strcmp(ext, "ent") != 0)
    {
        Con_DPrintf2("FindFile: can't find %s\n", filename);
    }
    else
    {
        Con_DPrintf3("FindFile: can't find %s\n", filename);
    }
    // Log pcx, tga, lit, ent misses only if (developer.value >= 2)

    if(handle)
    {
        *handle = -1;
    }

    if(file)
    {
        *file = nullptr;
    }

    com_filesize = -1;
    return com_filesize;
}


/*
===========
COM_FileExists

Returns whether the file is found in the quake filesystem.
===========
*/
bool COM_FileExists(const char* filename, unsigned int* path_id)
{
    int ret = COM_FindFile(filename, nullptr, nullptr, path_id);
    return (ret == -1) ? false : true;
}

/*
===========
COM_OpenFile

filename never has a leading slash, but may contain directory walks
returns a handle and a length
it may actually be inside a pak file
===========
*/
int COM_OpenFile(const char* filename, int* handle, unsigned int* path_id)
{
    return COM_FindFile(filename, handle, nullptr, path_id);
}

/*
===========
COM_FOpenFile

If the requested file is inside a packfile, a new FILE * will be opened
into the file.
===========
*/
int COM_FOpenFile(const char* filename, FILE** file, unsigned int* path_id)
{
    return COM_FindFile(filename, nullptr, file, path_id);
}

/*
============
COM_CloseFile

If it is a pak file handle, don't really close it
============
*/
void COM_CloseFile(int h)
{
    searchpath_t* s;

    for(s = com_searchpaths; s; s = s->next)
    {
        if(s->pack && s->pack->handle == h)
        {
            return;
        }
    }

    Sys_FileClose(h);
}


/*
============
COM_LoadFile

Filename are reletive to the quake directory.
Allways appends a 0 byte.
============
*/
#define LOADFILE_ZONE 0
#define LOADFILE_HUNK 1
#define LOADFILE_TEMPHUNK 2
#define LOADFILE_CACHE 3
#define LOADFILE_STACK 4
#define LOADFILE_MALLOC 5

static byte* loadbuf;
static cache_user_t* loadcache;
static int loadsize;

byte* COM_LoadFile(const char* path, int usehunk, unsigned int* path_id)
{
    int h;
    byte* buf;
    char base[32];
    int len;

    buf = nullptr; // quiet compiler warning

    // look for it in the filesystem or pack files
    len = COM_OpenFile(path, &h, path_id);
    if(h == -1)
    {
        return nullptr;
    }

    // extract the filename base name for hunk tag
    COM_FileBase(path, base, sizeof(base));

    switch(usehunk)
    {
        case LOADFILE_HUNK: buf = (byte*)Hunk_AllocName(len + 1, base); break;
        case LOADFILE_TEMPHUNK: buf = (byte*)Hunk_TempAlloc(len + 1); break;
        case LOADFILE_ZONE: buf = (byte*)Z_Malloc(len + 1); break;
        case LOADFILE_CACHE:
            buf = (byte*)Cache_Alloc(loadcache, len + 1, base);
            break;
        case LOADFILE_STACK:
            if(len < loadsize)
            {
                buf = loadbuf;
            }
            else
            {
                buf = (byte*)Hunk_TempAlloc(len + 1);
            }
            break;
        case LOADFILE_MALLOC: buf = (byte*)malloc(len + 1); break;
        default: Sys_Error("COM_LoadFile: bad usehunk");
    }

    if(!buf)
    {
        Sys_Error("COM_LoadFile: not enough space for %s", path);
    }

    ((byte*)buf)[len] = 0;

    Sys_FileRead(h, buf, len);
    COM_CloseFile(h);

    return buf;
}

byte* COM_LoadHunkFile(const char* path, unsigned int* path_id)
{
    return COM_LoadFile(path, LOADFILE_HUNK, path_id);
}

byte* COM_LoadZoneFile(const char* path, unsigned int* path_id)
{
    return COM_LoadFile(path, LOADFILE_ZONE, path_id);
}

byte* COM_LoadTempFile(const char* path, unsigned int* path_id)
{
    return COM_LoadFile(path, LOADFILE_TEMPHUNK, path_id);
}

void COM_LoadCacheFile(
    const char* path, struct cache_user_s* cu, unsigned int* path_id)
{
    loadcache = cu;
    COM_LoadFile(path, LOADFILE_CACHE, path_id);
}

// uses temp hunk if larger than bufsize
byte* COM_LoadStackFile(
    const char* path, void* buffer, int bufsize, unsigned int* path_id)
{
    byte* buf;

    loadbuf = (byte*)buffer;
    loadsize = bufsize;
    buf = COM_LoadFile(path, LOADFILE_STACK, path_id);

    return buf;
}

// returns malloc'd memory
byte* COM_LoadMallocFile(const char* path, unsigned int* path_id)
{
    return COM_LoadFile(path, LOADFILE_MALLOC, path_id);
}

byte* COM_LoadMallocFile_TextMode_OSPath(const char* path, long* len_out)
{
    FILE* f;
    byte* data;
    long len;
    long actuallen;

    // ericw -- this is used by Host_Loadgame_f. Translate CRLF to LF on load
    // games, othewise multiline messages have a garbage character at the end of
    // each line.
    // TODO: could handle in a way that allows loading CRLF savegames on
    // mac/linux without the junk characters appearing.
    f = fopen(path, "rt");
    if(f == nullptr)
    {
        return nullptr;
    }

    len = COM_filelength(f);
    if(len < 0)
    {
        return nullptr;
    }

    data = (byte*)malloc(len + 1);
    if(data == nullptr)
    {
        return nullptr;
    }

    // (actuallen < len) if CRLF to LF translation was performed
    actuallen = fread(data, 1, len, f);
    if(ferror(f))
    {
        free(data);
        return nullptr;
    }
    data[actuallen] = '\0';

    if(len_out != nullptr)
    {
        *len_out = actuallen;
    }
    return data;
}

const char* COM_ParseTimestampNewline(const char* buffer)
{
    int tmp;
    int consumed = 0;

    const int rc = sscanf(buffer, "%d-%d-%d %d:%d:%d\n%n", &tmp, &tmp, &tmp,
        &tmp, &tmp, &tmp, &consumed);

    if(rc != 6)
    {
        return nullptr;
    }

    return buffer + consumed;
}

const char* COM_ParseIntNewline(const char* buffer, int* value)
{
    int consumed = 0;
    sscanf(buffer, "%i\n%n", value, &consumed);
    return buffer + consumed;
}

const char* COM_ParseFloatNewline(const char* buffer, float* value)
{
    int consumed = 0;
    sscanf(buffer, "%f\n%n", value, &consumed);
    return buffer + consumed;
}

const char* COM_ParseStringNewline(const char* buffer)
{
    int consumed = 0;
    com_token[0] = '\0';
    sscanf(buffer, "%1023s\n%n", com_token, &consumed);
    return buffer + consumed;
}

/*
=================
COM_LoadPackFile -- johnfitz -- modified based on topaz's tutorial

Takes an explicit (not game tree related) path to a pak file.

Loads the header and directory, adding the files at the beginning
of the list so they override previous pack files.
=================
*/
static pack_t* COM_LoadPackFile(const char* packfile)
{
    dpackheader_t header;
    int i;
    packfile_t* newfiles;
    int numpackfiles;
    pack_t* pack;
    int packhandle;
    dpackfile_t info[MAX_FILES_IN_PACK];
    unsigned short crc;

    if(Sys_FileOpenRead(packfile, &packhandle) == -1)
    {
        return nullptr;
    }

    Sys_FileRead(packhandle, (void*)&header, sizeof(header));
    if(header.id[0] != 'P' || header.id[1] != 'A' || header.id[2] != 'C' ||
        header.id[3] != 'K')
    {
        Sys_Error("%s is not a packfile", packfile);
    }

    header.dirofs = LittleLong(header.dirofs);
    header.dirlen = LittleLong(header.dirlen);

    numpackfiles = header.dirlen / sizeof(dpackfile_t);

    if(header.dirlen < 0 || header.dirofs < 0)
    {
        Sys_Error("Invalid packfile %s (dirlen: %i, dirofs: %i)", packfile,
            header.dirlen, header.dirofs);
    }
    if(!numpackfiles)
    {
        Sys_Printf("WARNING: %s has no files, ignored\n", packfile);
        Sys_FileClose(packhandle);
        return nullptr;
    }
    if(numpackfiles > MAX_FILES_IN_PACK)
    {
        Sys_Error("%s has %i files", packfile, numpackfiles);
    }

    if(numpackfiles != PAK0_COUNT)
    {
        com_modified = true; // not the original file
    }

    newfiles = (packfile_t*)Z_Malloc(numpackfiles * sizeof(packfile_t));

    Sys_FileSeek(packhandle, header.dirofs);
    Sys_FileRead(packhandle, (void*)info, header.dirlen);

    // crc the directory to check for modifications
    CRC_Init(&crc);
    for(i = 0; i < header.dirlen; i++)
    {
        CRC_ProcessByte(&crc, ((byte*)info)[i]);
    }
    if(crc != PAK0_CRC_V106 && crc != PAK0_CRC_V101 && crc != PAK0_CRC_V100)
    {
        com_modified = true;
    }

    // parse the directory
    for(i = 0; i < numpackfiles; i++)
    {
        q_strlcpy(newfiles[i].name, info[i].name, sizeof(newfiles[i].name));
        newfiles[i].filepos = LittleLong(info[i].filepos);
        newfiles[i].filelen = LittleLong(info[i].filelen);
    }

    pack = (pack_t*)Z_Malloc(sizeof(pack_t));
    q_strlcpy(pack->filename, packfile, sizeof(pack->filename));
    pack->handle = packhandle;
    pack->numfiles = numpackfiles;
    pack->files = newfiles;

    // Sys_Printf ("Added packfile %s (%i files)\n", packfile, numpackfiles);
    return pack;
}

// QSS
#ifdef _WIN32
static time_t Sys_FileTimeToTime(FILETIME ft)
{
    ULARGE_INTEGER ull;
    ull.u.LowPart = ft.dwLowDateTime;
    ull.u.HighPart = ft.dwHighDateTime;
    return ull.QuadPart / 10000000u - 11644473600u;
}
#endif

void COM_ListSystemFiles(void* ctx, const char* gamedir, const char* ext,
    bool (*cb)(void* ctx, const char* fname))
{
#ifdef _WIN32
    WIN32_FIND_DATA fdat;
    HANDLE fhnd;
    char filestring[MAX_OSPATH];
    q_snprintf(filestring, sizeof(filestring), "%s/*.%s", gamedir, ext);
    fhnd = FindFirstFile(filestring, &fdat);
    if(fhnd == INVALID_HANDLE_VALUE)
    {
        return;
    }
    do
    {
        cb(ctx, fdat.cFileName);
    } while(FindNextFile(fhnd, &fdat));
    FindClose(fhnd);
#else
    DIR* dir_p;
    struct dirent* dir_t;
    dir_p = opendir(gamedir);
    if(dir_p == NULL) return;
    while((dir_t = readdir(dir_p)) != NULL)
    {
        if(q_strcasecmp(COM_FileGetExtension(dir_t->d_name), ext) != 0)
            continue;
        cb(ctx, dir_t->d_name);
    }
    closedir(dir_p);
#endif
}

void COM_ListFiles(void* ctx, const char* gamedir, const char* pattern,
    bool (*cb)(void* ctx, const char* fname, time_t mtime, size_t fsize))
{
    char prefixdir[MAX_OSPATH];
    const char* sl;
    sl = strrchr(pattern, '/');
    if(sl)
    {
        sl++;
        if(sl - pattern >= MAX_OSPATH)
        {
            return;
        }
        memcpy(prefixdir, pattern, sl - pattern);
        prefixdir[sl - pattern] = 0;
        pattern = sl;
    }
    else
    {
        *prefixdir = 0;
    }

#ifdef _WIN32
    {
        char filestring[MAX_OSPATH];
        WIN32_FIND_DATA fdat;
        HANDLE fhnd;
        q_snprintf(filestring, sizeof(filestring), "%s/%s%s", gamedir,
            prefixdir, pattern);
        fhnd = FindFirstFile(filestring, &fdat);
        if(fhnd == INVALID_HANDLE_VALUE)
        {
            return;
        }
        do
        {
            q_snprintf(filestring, sizeof(filestring), "%s%s", prefixdir,
                fdat.cFileName);
            cb(ctx, filestring, Sys_FileTimeToTime(fdat.ftLastWriteTime),
                fdat.nFileSizeLow);
        } while(FindNextFile(fhnd, &fdat));
        FindClose(fhnd);
    }
#else
    {
        char filestring[MAX_OSPATH];
        DIR* dir_p;
        struct dirent* dir_t;

        q_snprintf(filestring, sizeof(filestring), "%s/%s%s", gamedir,
            prefixdir, pattern);
        dir_p = opendir(filestring);
        if(dir_p == NULL) return;
        while((dir_t = readdir(dir_p)) != NULL)
        {
            if(!fnmatch(pattern, dir_t->d_name,
                   FNM_NOESCAPE | FNM_PATHNAME | FNM_CASEFOLD))
            {
                q_snprintf(filestring, sizeof(filestring), "%s%s", prefixdir,
                    dir_t->d_name);
                cb(ctx, filestring, 0, 0);
            }
        }
        closedir(dir_p);
    }
#endif
}

static bool COM_AddPackage(searchpath_t* basepath, const char* pakfile)
{
    searchpath_t* search;
    pack_t* pak;
    const char* ext = COM_FileGetExtension(pakfile);

    // don't add the same pak twice.
    for(search = com_searchpaths; search; search = search->next)
    {
        if(search->pack)
        {
            if(!q_strcasecmp(pakfile, search->pack->filename))
            {
                return true;
            }
        }
    }

    if(!q_strcasecmp(ext, "pak"))
    {
        pak = COM_LoadPackFile(pakfile);
    }
    else if(!q_strcasecmp(ext, "pk3") || !q_strcasecmp(ext, "pk4") ||
            !q_strcasecmp(ext, "zip") || !q_strcasecmp(ext, "apk"))
    {
        pak = FSZIP_LoadArchive(pakfile);
        if(pak)
        {
            com_modified =
                true; // would always be true, so we don't bother with crcs.
        }
    }
    else
    {
        pak = nullptr;
    }

    if(!pak)
    {
        return false;
    }

    search = (searchpath_t*)Z_Malloc(sizeof(searchpath_t));
    search->path_id = basepath->path_id;
    search->pack = pak;
    search->next = com_searchpaths;
    com_searchpaths = search;

    return true;
}

static bool COM_AddEnumeratedPackage(void* ctx, const char* pakfile)
{
    searchpath_t* basepath = (searchpath_t*)ctx;
    char fullpakfile[MAX_OSPATH];
    q_snprintf(
        fullpakfile, sizeof(fullpakfile), "%s/%s", basepath->filename, pakfile);
    return COM_AddPackage(basepath, fullpakfile);
}

const char* COM_GetGameNames(bool full)
{
    if(full)
    {
        if(*com_gamenames)
        {
            return va("%s;%s", GAMENAME, com_gamenames);
        }
        else
        {
            return GAMENAME;
        }
    }
    return com_gamenames;
    //	return COM_SkipPath(com_gamedir);
}
// if either contain id1 then that gets ignored
bool COM_GameDirMatches(const char* tdirs)
{
    int gnl = strlen(GAMENAME);
    const char* odirs = COM_GetGameNames(false);

    // ignore any core paths.
    if(!strncmp(tdirs, GAMENAME, gnl) && (tdirs[gnl] == ';' || !tdirs[gnl]))
    {
        tdirs += gnl;
        if(*tdirs == ';')
        {
            tdirs++;
        }
    }
    if(!strncmp(odirs, GAMENAME, gnl) && (odirs[gnl] == ';' || !odirs[gnl]))
    {
        odirs += gnl;
        if(*odirs == ';')
        {
            odirs++;
        }
    }
    // skip any qw in there from quakeworld (remote servers should really be
    // skipping this, unless its maybe the only one in the path).
    if(!strncmp(tdirs, "qw;", 3) || !strcmp(tdirs, "qw"))
    {
        tdirs += 2;
        if(*tdirs == ';')
        {
            tdirs++;
        }
    }
    if(!strncmp(odirs, "qw;", 3) ||
        !strcmp(odirs, "qw")) // need to cope with ourselves setting it that way
                              // too, just in case.
    {
        odirs += 2;
        if(*odirs == ';')
        {
            odirs++;
        }
    }

    // okay, now check it properly
    if(!strcmp(odirs, tdirs))
    {
        return true;
    }
    return false;
}
// ---

/*
=================
COM_AddGameDirectory -- johnfitz -- modified based on topaz's tutorial
=================
*/
static void COM_AddGameDirectory(const char* base, const char* dir)
{
    bool been_here = false;

    q_strlcpy(com_gamedir, va("%s/%s", base, dir), sizeof(com_gamedir));

    // assign a path_id to this game directory
    const unsigned int path_id =
        com_searchpaths ? com_searchpaths->path_id << 1 : 1U;

_add_path:
    // add the directory to the search path
    searchpath_t* search = (searchpath_t*)Z_Malloc(sizeof(searchpath_t));
    search->path_id = path_id;
    q_strlcpy(search->filename, com_gamedir, sizeof(search->filename));
    search->next = com_searchpaths;
    com_searchpaths = search;

    // VR: This was changed to support non-contiguous `.pak` files.
    for(int i = 0; i < 99; i++)
    {
        char pakfile[MAX_OSPATH];
        q_snprintf(pakfile, sizeof(pakfile), "%s/pak%i.pak", com_gamedir, i);

        pack_t* pak = COM_LoadPackFile(pakfile);
        pack_t* qspak;

        if(i != 0 || path_id != 1)
        {
            qspak = nullptr;
        }
        else
        {
            bool old = com_modified;
            if(been_here)
            {
                base = host_parms->userdir;
            }
            q_snprintf(pakfile, sizeof(pakfile), "%s/quakespasm.pak", base);
            qspak = COM_LoadPackFile(pakfile);
            com_modified = old;
        }

        if(pak)
        {
            search = (searchpath_t*)Z_Malloc(sizeof(searchpath_t));
            search->path_id = path_id;
            search->pack = pak;
            search->next = com_searchpaths;
            com_searchpaths = search;

            VR_OnLoadedPak(*pak);
        }

        if(qspak)
        {
            search = (searchpath_t*)Z_Malloc(sizeof(searchpath_t));
            search->path_id = path_id;
            search->pack = qspak;
            search->next = com_searchpaths;
            com_searchpaths = search;
        }

        if(!pak)
        {
            // Con_Printf(
            //     "Could not add pakfile to search paths: '%s'\n", pakfile);
        }
    }

    if(!been_here && host_parms->userdir != host_parms->basedir)
    {
        been_here = true;
        q_strlcpy(com_gamedir, va("%s/%s", host_parms->userdir, dir),
            sizeof(com_gamedir));
        Sys_mkdir(com_gamedir);
        goto _add_path;
    }
}

//==============================================================================
// johnfitz -- dynamic gamedir stuff -- modified by QuakeSpasm team.
//==============================================================================
void ExtraMaps_NewGame();
static void COM_Game_f()
{
    if(Cmd_Argc() > 1)
    {
        const char* p = Cmd_Argv(1);
        const char* p2 = Cmd_Argv(2);
        searchpath_t* search;

        if(!*p || !strcmp(p, ".") || strstr(p, "..") || strstr(p, "/") ||
            strstr(p, "\\") || strstr(p, ":"))
        {
            Con_Printf(
                "gamedir should be a single directory name, not a path\n");
            return;
        }

        if(*p2)
        {
            if(strcmp(p2, "-hipnotic") && strcmp(p2, "-rogue") &&
                strcmp(p2, "-quoth"))
            {
                Con_Printf("invalid mission pack argument to \"game\"\n");
                return;
            }
            if(!q_strcasecmp(p, GAMENAME))
            {
                Con_Printf("no mission pack arguments to %s game\n", GAMENAME);
                return;
            }
        }

        if(!q_strcasecmp(p, COM_SkipPath(com_gamedir))) // no change
        {
            if(com_searchpaths->path_id > 1)
            {
                // current game not id1
                if(*p2 && com_searchpaths->path_id == 2)
                {
                    // rely on QuakeSpasm extension treating '-game missionpack'
                    // as '-missionpack', otherwise would be a mess
                    if(!q_strcasecmp(p, &p2[1]))
                    {
                        goto _same;
                    }
                    Con_Printf("reloading game \"%s\" with \"%s\" support\n", p,
                        &p2[1]);
                }
                else if(!*p2 && com_searchpaths->path_id > 2)
                {
                    Con_Printf(
                        "reloading game \"%s\" without mission pack support\n",
                        p);
                }
                else
                {
                    goto _same;
                }
            }
            else
            {
            _same:
                Con_Printf(
                    "\"game\" is already \"%s\"\n", COM_SkipPath(com_gamedir));
                return;
            }
        }

        com_modified = true;

        // Kill the server
        CL_Disconnect();
        Host_ShutdownServer(true);

        VR_InitGame();

        // Write config file
        Host_WriteConfiguration();

        // Kill the extra game if it is loaded
        while(com_searchpaths != com_base_searchpaths)
        {
            if(com_searchpaths->pack)
            {
                Sys_FileClose(com_searchpaths->pack->handle);
                Z_Free(com_searchpaths->pack->files);
                Z_Free(com_searchpaths->pack);
            }
            search = com_searchpaths->next;
            Z_Free(com_searchpaths);
            com_searchpaths = search;
        }
        hipnotic = false;
        rogue = false;
        standard_quake = true;

        if(q_strcasecmp(p, GAMENAME)) // game is not id1
        {
            if(*p2)
            {
                COM_AddGameDirectory(com_basedir, &p2[1]);
                standard_quake = false;
                if(!strcmp(p2, "-hipnotic") || !strcmp(p2, "-quoth"))
                {
                    hipnotic = true;
                }
                else if(!strcmp(p2, "-rogue"))
                {
                    rogue = true;
                }
                if(q_strcasecmp(p, &p2[1]))
                {
                    // don't load twice
                    COM_AddGameDirectory(com_basedir, p);
                }
            }
            else
            {
                COM_AddGameDirectory(com_basedir, p);
                // QuakeSpasm extension: treat '-game missionpack' as
                // '-missionpack'
                if(!q_strcasecmp(p, "hipnotic") || !q_strcasecmp(p, "quoth"))
                {
                    hipnotic = true;
                    standard_quake = false;
                }
                else if(!q_strcasecmp(p, "rogue"))
                {
                    rogue = true;
                    standard_quake = false;
                }
            }
        }
        else // just update com_gamedir
        {
            q_snprintf(com_gamedir, sizeof(com_gamedir), "%s/%s",
                (host_parms->userdir != host_parms->basedir)
                    ? host_parms->userdir
                    : com_basedir,
                GAMENAME);
        }

        // clear out and reload appropriate data
        Cache_Flush();
        Mod_ResetAll();

        if(!isDedicated)
        {
            TexMgr_NewGame();
            Draw_NewGame();
            R_NewGame();
        }

        ExtraMaps_NewGame();
        DemoList_Rebuild();

        Con_Printf("\"game\" changed to \"%s\"\n", COM_SkipPath(com_gamedir));

        VID_Lock();

        // VR: This is what reads 'config.cfg'.
        Cbuf_AddText("exec quake.rc\n");
        Cbuf_Execute();

        Cbuf_AddText("vid_unlock\n");

        if(vr_enabled.value)
        {
            Cbuf_AddText("map start\n");
            VR_ModAllModels();
        }
    }
    else
    {
        // Diplay the current gamedir
        Con_Printf("\"game\" is \"%s\"\n", COM_SkipPath(com_gamedir));
    }
}

/*
=================
COM_InitFilesystem
=================
*/
void COM_InitFilesystem() // johnfitz -- modified based on topaz's tutorial
{
    int i;
    int j;

    Cvar_RegisterVariable(&registered);
    Cvar_RegisterVariable(&cmdline);
    Cmd_AddCommand("path", COM_Path_f);
    Cmd_AddCommand("game", COM_Game_f); // johnfitz

    i = COM_CheckParm("-basedir");
    if(i && i < com_argc - 1)
    {
        q_strlcpy(com_basedir, com_argv[i + 1], sizeof(com_basedir));
    }
    else
    {
        q_strlcpy(com_basedir, host_parms->basedir, sizeof(com_basedir));
    }

    j = strlen(com_basedir);
    if(j < 1)
    {
        Sys_Error("Bad argument to -basedir");
    }
    if((com_basedir[j - 1] == '\\') || (com_basedir[j - 1] == '/'))
    {
        com_basedir[j - 1] = 0;
    }

    // VR: This starts loading all the `.pak` files.
    // start up with GAMENAME by default (id1)
    COM_AddGameDirectory(com_basedir, GAMENAME);

    /* this is the end of our base searchpath:
     * any set gamedirs, such as those from -game command line
     * arguments or by the 'game' console command will be freed
     * up to here upon a new game command. */
    com_base_searchpaths = com_searchpaths;

    // add mission pack requests (only one should be specified)
    if(COM_CheckParm("-rogue"))
    {
        COM_AddGameDirectory(com_basedir, "rogue");
    }
    if(COM_CheckParm("-hipnotic"))
    {
        COM_AddGameDirectory(com_basedir, "hipnotic");
    }
    if(COM_CheckParm("-quoth"))
    {
        COM_AddGameDirectory(com_basedir, "quoth");
    }


    i = COM_CheckParm("-game");
    if(i && i < com_argc - 1)
    {
        const char* p = com_argv[i + 1];
        if(!*p || !strcmp(p, ".") || strstr(p, "..") || strstr(p, "/") ||
            strstr(p, "\\") || strstr(p, ":"))
        {
            Sys_Error(
                "gamedir should be a single directory name, not a path\n");
        }
        com_modified = true;
        // don't load mission packs twice
        if(COM_CheckParm("-rogue") && !q_strcasecmp(p, "rogue"))
        {
            p = nullptr;
        }
        if(COM_CheckParm("-hipnotic") && !q_strcasecmp(p, "hipnotic"))
        {
            p = nullptr;
        }
        if(COM_CheckParm("-quoth") && !q_strcasecmp(p, "quoth"))
        {
            p = nullptr;
        }
        if(p != nullptr)
        {
            COM_AddGameDirectory(com_basedir, p);
            // QuakeSpasm extension: treat '-game missionpack' as '-missionpack'
            if(!q_strcasecmp(p, "rogue"))
            {
                rogue = true;
                standard_quake = false;
            }
            if(!q_strcasecmp(p, "hipnotic") || !q_strcasecmp(p, "quoth"))
            {
                hipnotic = true;
                standard_quake = false;
            }
        }
    }

    COM_CheckRegistered();
}

// for compat with dpp7 protocols, and mods that cba to precache things.
void COM_Effectinfo_Enumerate(int (*cb)(const char* pname))
{
    int i;
    const char *f, *e;
    char* buf;
    static const char* dpnames[] = {"TE_GUNSHOT", "TE_GUNSHOTQUAD", "TE_SPIKE",
        "TE_SPIKEQUAD", "TE_SUPERSPIKE", "TE_SUPERSPIKEQUAD", "TE_WIZSPIKE",
        "TE_KNIGHTSPIKE", "TE_EXPLOSION", "TE_EXPLOSIONQUAD", "TE_TAREXPLOSION",
        "TE_TELEPORT", "TE_LAVASPLASH", "TE_SMALLFLASH", "TE_FLAMEJET",
        "EF_FLAME", "TE_BLOOD", "TE_SPARK", "TE_PLASMABURN", "TE_TEI_G3",
        "TE_TEI_SMOKE", "TE_TEI_BIGEXPLOSION", "TE_TEI_PLASMAHIT",
        "EF_STARDUST", "TR_ROCKET", "TR_GRENADE", "TR_BLOOD", "TR_WIZSPIKE",
        "TR_SLIGHTBLOOD", "TR_KNIGHTSPIKE", "TR_VORESPIKE", "TR_NEHAHRASMOKE",
        "TR_NEXUIZPLASMA", "TR_GLOWTRAIL", "SVC_PARTICLE", nullptr};

    buf = (char*)COM_LoadMallocFile("effectinfo.txt", nullptr);
    if(!buf)
    {
        return;
    }

    for(i = 0; dpnames[i]; i++)
    {
        cb(dpnames[i]);
    }

    for(f = buf; f; f = e)
    {
        e = COM_Parse(f);
        if(!strcmp(com_token, "effect"))
        {
            e = COM_Parse(e);
            cb(com_token);
        }
        while(e && *e && *e != '\n')
        {
            e++;
        }
    }
    free(buf);
}
