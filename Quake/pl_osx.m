/*
Copyright (C) 1996-2001 Id Software, Inc.
Copyright (C) 2002-2005 John Fitzgibbons and others
Copyright (C) 2007-2008 Kristian Duske
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

#include "quakedef.h"
#include "platform.hpp"

#include <SDL2/SDL.h>

#import <Cocoa/Cocoa.h>

void PL_SetWindowIcon()
{
    /* nothing to do on OS X */
}

void PL_VID_Shutdown()
{
}

#define MAX_CLIPBOARDTXT MAXCMDLINE /* 256 */
char* PL_GetClipboardData()
{
    char* data = nullptr;
    NSPasteboard* pasteboard = [NSPasteboard generalPasteboard];
    NSArray* types = [pasteboard types];

    if([types containsObject:NSStringPboardType])
    {
        NSString* clipboardString =
            [pasteboard stringForType:NSStringPboardType];
        if(clipboardString != nullptr && [clipboardString length] > 0)
        {
            size_t sz = [clipboardString length] + 1;
            sz = q_min(MAX_CLIPBOARDTXT, sz);
            data = (char*)Z_Malloc(sz);
#if(MAC_OS_X_VERSION_MIN_REQUIRED < \
    1040) /* for ppc builds targeting 10.3 and older */
            q_strlcpy(data, [clipboardString cString], sz);
#else
            q_strlcpy(data,
                [clipboardString cStringUsingEncoding:NSASCIIStringEncoding],
                sz);
#endif
        }
    }
    return data;
}

void PL_ErrorDialog(const char* errorMsg)
{
#if(MAC_OS_X_VERSION_MIN_REQUIRED < \
    1040) /* ppc builds targeting 10.3 and older */
    NSString* msg = [NSString stringWithCString:errorMsg];
#else
    NSString* msg = [NSString stringWithCString:errorMsg
                                       encoding:NSASCIIStringEncoding];
#endif
    NSRunCriticalAlertPanel(@"Quake Error", @"%@", @"OK", nil, nil, msg);
}
