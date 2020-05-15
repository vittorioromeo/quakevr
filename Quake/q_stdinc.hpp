/*
 * q_stdinc.h - includes the minimum necessary stdc headers,
 *		defines common and / or missing types.
 *
 * NOTE:	for net stuff use net_sys.h,
 *		for byte order use q_endian.h,
 *		for math stuff use mathlib.h,
 *		for locale-insensitive ctype.h functions use q_ctype.h.
 *
 * Copyright (C) 1996-1997  Id Software, Inc.
 * Copyright (C) 2007-2011  O.Sezer <sezero@users.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef __QSTDINC_H
#define __QSTDINC_H

#include <sys/types.h>
#include <stddef.h>
#include <limits.h>

#ifndef _WIN32 /* others we support without sys/param.h? */
#include <sys/param.h>
#endif

#include <stdio.h>

/* NOTES on TYPE SIZES:
   Quake/Hexen II engine relied on 32 bit int type size
   with ILP32 (not LP32) model in mind.  We now support
   LP64 and LLP64, too. We expect:
   sizeof (char)	== 1
   sizeof (short)	== 2
   sizeof (int)		== 4
   sizeof (float)	== 4
   sizeof (long)	== 4 / 8
   sizeof (pointer *)	== 4 / 8
   For this, we need stdint.h (or inttypes.h)
   FIXME: On some platforms, only inttypes.h is available.
   FIXME: Properly replace certain short and int usage
      with int16_t and int32_t.
 */
#if defined(_MSC_VER) && (_MSC_VER < 1600)
/* MS Visual Studio provides stdint.h only starting with
 * version 2010.  Even in VS2010, there is no inttypes.h.. */
#include "msinttypes/stdint.hpp"
#else
#include <stdint.h>
#endif

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <float.h>

/*==========================================================================*/

#define Q_MAXCHAR ((char)0x7f)
#define Q_MAXSHORT ((short)0x7fff)
#define Q_MAXINT ((int)0x7fffffff)
#define Q_MAXLONG ((int)0x7fffffff)

#define Q_MINCHAR ((char)0x80)
#define Q_MINSHORT ((short)0x8000)
#define Q_MININT ((int)0x80000000)
#define Q_MINLONG ((int)0x80000000)

/* Make sure the types really have the right
 * sizes: These macros are from SDL headers.
 */
static_assert(sizeof(char) == 1);
static_assert(sizeof(float) == 4);
static_assert(sizeof(long) >= 4);
static_assert(sizeof(int) == 4);
static_assert(sizeof(short) == 2);

/* make sure enums are the size of ints for structure packing */
typedef enum
{
    THE_DUMMY_VALUE
} THE_DUMMY_ENUM;
static_assert(sizeof(THE_DUMMY_ENUM) == sizeof(int));


/* Provide a substitute for offsetof() if we don't have one.
 * This variant works on most (but not *all*) systems...
 */
#ifndef offsetof
#define offsetof(t, m) ((size_t) & (((t*)0)->m))
#endif


/*==========================================================================*/

typedef unsigned char byte;

#undef true
#undef false

/* some structures have bool members and the x86 asm code expect
 * those members to be 4 bytes long. therefore, bool must be 32
 * bits and it can NOT be binary compatible with the 8 bit C++ bool.  */

/*==========================================================================*/

/* math */
using vec3_t = float[3];
using fixed8_t = int;


/*==========================================================================*/

/* MAX_OSPATH (max length of a filesystem pathname, i.e. PATH_MAX)
 * Note: See GNU Hurd and others' notes about brokenness of this:
 * http://www.gnu.org/software/hurd/community/gsoc/project_ideas/maxpath.html
 * http://insanecoding.blogspot.com/2007/11/pathmax-simply-isnt.html */

#if !defined(PATH_MAX)
/* equivalent values? */
#if defined(MAXPATHLEN)
#define PATH_MAX MAXPATHLEN
#elif defined(_WIN32) && defined(_MAX_PATH)
#define PATH_MAX _MAX_PATH
#elif defined(_WIN32) && defined(MAX_PATH)
#define PATH_MAX MAX_PATH
#else /* fallback */
#define PATH_MAX 1024
#endif
#endif /* PATH_MAX */

#define MAX_OSPATH PATH_MAX

/*==========================================================================*/

/* missing types: */
#if defined(_MSC_VER)
typedef ptrdiff_t ssize_t;
#endif

/*==========================================================================*/

/* function attributes, etc */

#if defined(__GNUC__)
#define FUNC_PRINTF(x, y) __attribute__((__format__(__printf__, x, y)))
#else
#define FUNC_PRINTF(x, y)
#endif

/* argument format attributes for function pointers are supported for gcc >= 3.1
 */
#if defined(__GNUC__) && (__GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ > 0))
#define FUNCP_PRINTF FUNC_PRINTF
#else
#define FUNCP_PRINTF(x, y)
#endif

/*==========================================================================*/

#endif /* __QSTDINC_H */
