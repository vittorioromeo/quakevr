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

#include <cassert>
#include <string>

#include "host.hpp"
#include "quakedef.hpp"
#include "common.hpp"
#include "quakeparms.hpp"
#include "sys.hpp"
#include "vid.hpp"
#include "client.hpp"
#include "screen.hpp"

#include <SDL2/SDL.h>

#include <stdio.h>

/* need at least SDL_2.0.0 */
#define SDL_MIN_X 2
#define SDL_MIN_Y 0
#define SDL_MIN_Z 0
#define SDL_REQUIREDVERSION (SDL_VERSIONNUM(SDL_MIN_X, SDL_MIN_Y, SDL_MIN_Z))
#define SDL_NEW_VERSION_REJECT (SDL_VERSIONNUM(3, 0, 0))

static void Sys_AtExit()
{
    SDL_Quit();
}

static void Sys_InitSDL()
{
    SDL_version v;
    SDL_version* sdl_version = &v;
    SDL_GetVersion(&v);

    Sys_Printf("Found SDL version %i.%i.%i\n", sdl_version->major,
        sdl_version->minor, sdl_version->patch);
    if(SDL_VERSIONNUM(sdl_version->major, sdl_version->minor,
           sdl_version->patch) < SDL_REQUIREDVERSION)
    { /*reject running under older SDL versions */
        Sys_Error("You need at least v%d.%d.%d of SDL to run this game.",
            SDL_MIN_X, SDL_MIN_Y, SDL_MIN_Z);
    }
    if(SDL_VERSIONNUM(sdl_version->major, sdl_version->minor,
           sdl_version->patch) >= SDL_NEW_VERSION_REJECT)
    { /*reject running under newer (1.3.x) SDL */
        Sys_Error(
            "Your version of SDL library is incompatible with me.\n"
            "You need a library version in the line of %d.%d.%d\n",
            SDL_MIN_X, SDL_MIN_Y, SDL_MIN_Z);
    }

    if(SDL_Init(0) < 0)
    {
        Sys_Error("Couldn't init SDL: %s", SDL_GetError());
    }
    atexit(Sys_AtExit);
}

#define DEFAULT_MEMORY \
    (256 * 1024 * 1024) // ericw -- was 72MB (64-bit) / 64MB (32-bit)

static quakeparms_t parms;

// On OS X we call SDL_main from the launcher, but SDL2 doesn't redefine main
// as SDL_main on OS X anymore, so we do it ourselves.
#if defined(__APPLE__)
#define main SDL_main
#endif

// TODO VR: (P2) what to do with this?
extern std::string vr_working_directory;

int main(int argc, char* argv[])
{
    // TODO VR: (P2) more portable/reliable way of doing this
    assert(argc >= 1);
    vr_working_directory = argv[0];

// TODO VR: (P2) linux hack
#ifdef WIN32
    vr_working_directory =
        vr_working_directory.substr(0, vr_working_directory.find_last_of('\\'));
#else
    vr_working_directory =
        "/run/media/vittorioromeo/D2703413703400B1/OHWorkspace/quakevr/Windows/"
        "VisualStudio/Build-quakespasm-sdl2/x64/Debug";
    // vr_working_directory.substr(0, vr_working_directory.find_last_of("/"));
#endif

    Sys_Printf("Working directory: '%s'\n", vr_working_directory.c_str());

    int t;
    double time;
    double oldtime;
    double newtime;

    host_parms = &parms;
    parms.basedir = ".";

    parms.argc = argc;
    parms.argv = argv;

    parms.errstate = 0;

    COM_InitArgv(parms.argc, parms.argv);

    isDedicated = (COM_CheckParm("-dedicated") != 0);

    Sys_InitSDL();

    Sys_Init();

    parms.memsize = DEFAULT_MEMORY;
    if(COM_CheckParm("-heapsize"))
    {
        t = COM_CheckParm("-heapsize") + 1;
        if(t < com_argc)
        {
            parms.memsize = Q_atoi(com_argv[t]) * 1024;
        }
    }

    parms.membase = malloc(parms.memsize);

    if(!parms.membase)
    {
        Sys_Error("Not enough memory free; check disk space\n");
    }

    Sys_Printf("Quake %1.2f (c) id Software\n", VERSION);
    Sys_Printf("GLQuake %1.2f (c) id Software\n", GLQUAKE_VERSION);
    Sys_Printf("FitzQuake %1.2f (c) John Fitzgibbons\n", FITZQUAKE_VERSION);
    Sys_Printf("FitzQuake SDL port (c) SleepwalkR, Baker\n");
    Sys_Printf("QuakeSpasm " QUAKESPASM_VER_STRING
               " (c) Ozkan Sezer, Eric Wasylishen & others\n");
    Sys_Printf("QuakeSpasm-Spiked (c) Spike\n");
    Sys_Printf("Quake VR " QUAKEVR_VERSION " by Vittorio Romeo & others\n");

    Sys_Printf("Host_Init\n");
    Host_Init();

    oldtime = Sys_DoubleTime();
    if(isDedicated)
    {
        while(true)
        {
            newtime = Sys_DoubleTime();
            time = newtime - oldtime;

            while(time < sys_ticrate.value)
            {
                SDL_Delay(1);
                newtime = Sys_DoubleTime();
                time = newtime - oldtime;
            }

            Host_Frame(time);
            oldtime = newtime;
        }
    }
    else
    {
        while(true)
        {
            /* If we have no input focus at all, sleep a bit */
            if(!VID_HasMouseOrInputFocus() || cl.paused)
            {
                SDL_Delay(16);
            }
            /* If we're minimised, sleep a bit more */
            if(VID_IsMinimized())
            {
                scr_skipupdate = 1;
                SDL_Delay(32);
            }
            else
            {
                scr_skipupdate = 0;
            }
            newtime = Sys_DoubleTime();
            time = newtime - oldtime;

            Host_Frame(time);

            if(time < sys_throttle.value && !cls.timedemo)
            {
                SDL_Delay(1);
            }

            oldtime = newtime;
        }
    }

    return 0;
}
