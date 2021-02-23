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

#include "quakedef.hpp"

#include <windows.h>
#include "platform.hpp"

#include "keys.hpp"
#include "vid.hpp"
#include "zone.hpp"
#include "common.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>

static HICON icon;

void PL_SetWindowIcon()
{
    HINSTANCE handle;
    SDL_SysWMinfo wminfo;
    HWND hwnd;

    handle = GetModuleHandle(nullptr);
    icon = LoadIcon(handle, "icon");

    if(!icon)
    {
        return; /* no icon in the exe */
    }

    SDL_VERSION(&wminfo.version);

    if(SDL_GetWindowWMInfo((SDL_Window*)VID_GetWindow(), &wminfo) != SDL_TRUE)
    {
        return; /* wrong SDL version */
    }

    hwnd = wminfo.info.win.window;

#ifdef _WIN64
    SetClassLongPtr(hwnd, GCLP_HICON, (LONG_PTR)icon);
#else
    SetClassLong(hwnd, GCL_HICON, (LONG)icon);
#endif
}

void PL_VID_Shutdown()
{
    DestroyIcon(icon);
}

#define MAX_CLIPBOARDTXT MAXCMDLINE /* 256 */
char* PL_GetClipboardData()
{
    char* data = nullptr;
    char* cliptext;

    if(OpenClipboard(nullptr) != 0)
    {
        HANDLE hClipboardData;

        if((hClipboardData = GetClipboardData(CF_TEXT)) != nullptr)
        {
            cliptext = (char*)GlobalLock(hClipboardData);
            if(cliptext != nullptr)
            {
                size_t size = GlobalSize(hClipboardData) + 1;
                /* this is intended for simple small text copies
                 * such as an ip address, etc:  do chop the size
                 * here, otherwise we may experience Z_Malloc()
                 * failures and all other not-oh-so-fun stuff. */
                size = q_min(MAX_CLIPBOARDTXT, size);
                data = (char*)Z_Malloc(size);
                q_strlcpy(data, cliptext, size);
                GlobalUnlock(hClipboardData);
            }
        }
        CloseClipboard();
    }
    return data;
}

void PL_ErrorDialog(const char* errorMsg)
{
    MessageBox(nullptr, errorMsg, "Quake Error",
        MB_OK | MB_SETFOREGROUND | MB_ICONSTOP);
}
