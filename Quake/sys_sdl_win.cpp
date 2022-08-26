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

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <mmsystem.h>

#include "host.hpp"
#include "quakedef.hpp"
#include "quakeparms.hpp"
#include "platform.hpp"
#include "common.hpp"
#include "input.hpp"
#include "sys.hpp"
#include "console.hpp"
#include "progs.hpp"
#include "qcvm.hpp"

#include <sys/types.h>
#include <errno.h>
#include <io.h>
#include <direct.h>

#include <string>

#include <SDL2/SDL.h>

#pragma comment(lib, "dbgeng.lib")
#define BOOST_STACKTRACE_USE_WINDBG
#include <boost/stacktrace.hpp>
#include <iostream>

bool isDedicated;
bool Win95, Win95old, WinNT, WinVista;
cvar_t sys_throttle = {"sys_throttle", "0.02", CVAR_ARCHIVE};

static HANDLE hinput, houtput;

static size_t
    sys_handles_max; /* spike -- removed limit, was 32 (johnfitz -- was 10) */
static FILE** sys_handles;
static int findhandle()
{
    size_t i, n;

    for(i = 1; i < sys_handles_max; i++)
    {
        if(!sys_handles[i])
        {
            return i;
        }
    }
    n = sys_handles_max + 10;
    sys_handles = (FILE**)realloc(sys_handles, sizeof(*sys_handles) * n);
    if(!sys_handles)
    {
        Sys_Error("out of handles");
    }
    while(sys_handles_max < n)
    {
        sys_handles[sys_handles_max++] = nullptr;
    }
    return i;
}

long Sys_filelength(FILE* f)
{
    long pos;
    long end;

    pos = ftell(f);
    fseek(f, 0, SEEK_END);
    end = ftell(f);
    fseek(f, pos, SEEK_SET);

    return end;
}

int Sys_FileOpenRead(const char* path, int* hndl)
{
    FILE* f;
    int i;
    int retval;

    i = findhandle();
    f = fopen(path, "rb");

    if(!f)
    {
        *hndl = -1;
        retval = -1;
    }
    else
    {
        sys_handles[i] = f;
        *hndl = i;
        retval = Sys_filelength(f);
    }

    return retval;
}

int Sys_FileOpenWrite(const char* path)
{
    FILE* f;
    int i;

    i = findhandle();
    f = fopen(path, "wb");

    if(!f)
    {
        Sys_Error("Error opening %s: %s", path, strerror(errno));
    }

    sys_handles[i] = f;
    return i;
}

int Sys_FileOpenStdio(FILE* file)
{
    int i;
    i = findhandle();
    sys_handles[i] = file;
    return i;
}

void Sys_FileClose(int handle)
{
    fclose(sys_handles[handle]);
    sys_handles[handle] = nullptr;
}

void Sys_FileSeek(int handle, int position)
{
    fseek(sys_handles[handle], position, SEEK_SET);
}

int Sys_FileRead(int handle, void* dest, int count)
{
    return fread(dest, 1, count, sys_handles[handle]);
}

int Sys_FileWrite(int handle, const void* data, int count)
{
    return fwrite(data, 1, count, sys_handles[handle]);
}

int Sys_FileTime(const char* path)
{
    FILE* f;

    f = fopen(path, "rb");

    if(f)
    {
        fclose(f);
        return 1;
    }

    return -1;
}

static char cwd[1024];

static void Sys_GetBasedir(char* argv0, char* dst, size_t dstsize)
{
    (void)argv0;

    char* tmp;
    size_t rc;

    rc = GetCurrentDirectory(dstsize, dst);
    if(rc == 0 || rc > dstsize)
    {
        Sys_Error("Couldn't determine current directory");
    }

    tmp = dst;
    while(*tmp != 0)
    {
        tmp++;
    }
    while(*tmp == 0 && tmp != dst)
    {
        --tmp;
        if(tmp != dst && (*tmp == '/' || *tmp == '\\'))
        {
            *tmp = 0;
        }
    }
}

typedef enum
{
    dpi_unaware = 0,
    dpi_system_aware = 1,
    dpi_monitor_aware = 2
} dpi_awareness;
typedef BOOL(WINAPI* SetProcessDPIAwareFunc)();
typedef HRESULT(WINAPI* SetProcessDPIAwarenessFunc)(dpi_awareness value);

static void Sys_SetDPIAware()
{
    HMODULE hUser32;
    HMODULE hShcore;
    SetProcessDPIAwarenessFunc setDPIAwareness;
    SetProcessDPIAwareFunc setDPIAware;

    /* Neither SDL 1.2 nor SDL 2.0.3 can handle the OS scaling our window.
      (e.g. https://bugzilla.libsdl.org/show_bug.cgi?id=2713)
      Call SetProcessDpiAwareness/SetProcessDPIAware to opt out of scaling.
    */

    hShcore = LoadLibraryA("Shcore.dll");
    hUser32 = LoadLibraryA("user32.dll");
    setDPIAwareness =
        (SetProcessDPIAwarenessFunc)(hShcore ? GetProcAddress(hShcore,
                                                   "SetProcessDpiAwareness")
                                             : nullptr);
    setDPIAware = (SetProcessDPIAwareFunc)(hUser32 ? GetProcAddress(hUser32,
                                                         "SetProcessDPIAware")
                                                   : nullptr);

    if(setDPIAwareness)
    { /* Windows 8.1+ */
        setDPIAwareness(dpi_monitor_aware);
    }
    else if(setDPIAware)
    { /* Windows Vista-8.0 */
        setDPIAware();
    }

    if(hShcore)
    {
        FreeLibrary(hShcore);
    }
    if(hUser32)
    {
        FreeLibrary(hUser32);
    }
}

static void Sys_SetTimerResolution()
{
    /* Set OS timer resolution to 1ms.
       Works around buffer underruns with directsound and SDL2, but also
       will make Sleep()/SDL_Dleay() accurate to 1ms which should help framerate
       stability.
    */
    timeBeginPeriod(1);
}

// Returns the last Win32 error, in string format. Returns an empty string if
// there is no error.
static std::string GetLastErrorAsString()
{
    // Get the error message, if any.
    DWORD errorMessageID = ::GetLastError();
    if(errorMessageID == 0)
        return std::string(); // No error message has been recorded

    LPSTR messageBuffer = nullptr;
    size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                                     FORMAT_MESSAGE_FROM_SYSTEM |
                                     FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPSTR)&messageBuffer, 0, nullptr);

    std::string message(messageBuffer, size);

    // Free the buffer.
    LocalFree(messageBuffer);

    return message;
}

void Sys_Init()
{
    OSVERSIONINFO vinfo;

    Sys_SetTimerResolution();
    Sys_SetDPIAware();

    memset(cwd, 0, sizeof(cwd));
    Sys_GetBasedir(nullptr, cwd, sizeof(cwd));
    host_parms->basedir = cwd;

    /* userdirs not really necessary for windows guys.
     * can be done if necessary, though... */
    host_parms->userdir =
        host_parms->basedir; /* code elsewhere relies on this ! */

    vinfo.dwOSVersionInfoSize = sizeof(vinfo);

    if(!GetVersionEx(&vinfo))
    {
        Sys_Error("Couldn't get OS info");
    }

    if((vinfo.dwMajorVersion < 4) ||
        (vinfo.dwPlatformId == VER_PLATFORM_WIN32s))
    {
        Sys_Error("QuakeSpasm requires at least Win95 or NT 4.0");
    }

    if(vinfo.dwPlatformId == VER_PLATFORM_WIN32_NT)
    {
        SYSTEM_INFO info;
        WinNT = true;
        if(vinfo.dwMajorVersion >= 6)
        {
            WinVista = true;
        }
        GetSystemInfo(&info);
        host_parms->numcpus = info.dwNumberOfProcessors;
        if(host_parms->numcpus < 1)
        {
            host_parms->numcpus = 1;
        }
    }
    else
    {
        WinNT = false; /* Win9x or WinME */
        host_parms->numcpus = 1;
        if((vinfo.dwMajorVersion == 4) && (vinfo.dwMinorVersion == 0))
        {
            Win95 = true;
            /* Win95-gold or Win95A can't switch bpp automatically */
            if(vinfo.szCSDVersion[1] != 'C' && vinfo.szCSDVersion[1] != 'B')
            {
                Win95old = true;
            }
        }
    }
    Sys_Printf("Detected %d CPUs.\n", host_parms->numcpus);

    if(isDedicated)
    {
        // TODO VR: (P0) deal with allocconsole
        if(false && !AllocConsole())
        {
            isDedicated = false; /* so that we have a graphical error dialog */
            Sys_Error("Couldn't create dedicated server console: '%s'",
                GetLastErrorAsString().data());
        }

        hinput = GetStdHandle(STD_INPUT_HANDLE);
        houtput = GetStdHandle(STD_OUTPUT_HANDLE);
    }
}

void Sys_mkdir(const char* path)
{
    if(CreateDirectory(path, nullptr) != 0)
    {
        return;
    }
    if(GetLastError() != ERROR_ALREADY_EXISTS)
    {
        Sys_Error("Unable to create directory %s", path);
    }
}

static const char errortxt1[] = "\nERROR-OUT BEGIN\n\n";
static const char errortxt2[] = "\nQUAKE ERROR: ";

void Sys_Error(const char* error, ...)
{
    std::cout << "Stacktrace:\n"
              << boost::stacktrace::stacktrace() << std::endl;

    va_list argptr;
    char text[1024];
    DWORD dummy;

    host_parms->errstate++;

    va_start(argptr, error);
    q_vsnprintf(text, sizeof(text), error, argptr);
    va_end(argptr);

    PR_SwitchQCVM(nullptr);
    Con_Redirect(nullptr);

    if(isDedicated)
    {
        WriteFile(houtput, errortxt1, strlen(errortxt1), &dummy, nullptr);
    }

    /* SDL will put these into its own stderr log,
       so print to stderr even in graphical mode. */
    fputs(errortxt1, stderr);
    Host_Shutdown();
    fputs(errortxt2, stderr);
    fputs(text, stderr);
    fputs("\n\n", stderr);

    if(!isDedicated)
    {
        PL_ErrorDialog(text);
    }
    else
    {
        WriteFile(houtput, errortxt2, strlen(errortxt2), &dummy, nullptr);
        WriteFile(houtput, text, strlen(text), &dummy, nullptr);
        WriteFile(houtput, "\r\n", 2, &dummy, nullptr);
        SDL_Delay(3000); /* show the console 3 more seconds */
    }

    exit(1);
}

void Sys_Printf(const char* fmt, ...)
{
    va_list argptr;
    char text[1024];
    DWORD dummy;

    va_start(argptr, fmt);
    q_vsnprintf(text, sizeof(text), fmt, argptr);
    va_end(argptr);

    if(isDedicated)
    {
        if(*text == 1 || *text == 2)
        { // mostly for Con_[D]Warning
            SetConsoleTextAttribute(houtput, FOREGROUND_RED);
            WriteFile(houtput, text + 1, strlen(text + 1), &dummy, nullptr);
            SetConsoleTextAttribute(
                houtput, FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED);
        }
        else
        {
            WriteFile(houtput, text, strlen(text), &dummy, nullptr);
        }
    }
    else
    {
        /* SDL will put these into its own stdout log,
           so print to stdout even in graphical mode. */
        fputs(text, stdout);
    }
}

void Sys_Quit()
{
    Host_Shutdown();

    if(isDedicated)
    {
        FreeConsole();
    }

    exit(0);
}

double Sys_DoubleTime()
{
    return SDL_GetPerformanceCounter() /
           (long double)SDL_GetPerformanceFrequency();
}

const char* Sys_ConsoleInput()
{
    static char con_text[256];
    static int textlen;
    INPUT_RECORD recs[1024];
    int ch;
    DWORD dummy;
    DWORD numread;
    DWORD numevents;

    while(true)
    {
        if(GetNumberOfConsoleInputEvents(hinput, &numevents) == 0)
        {
            Sys_Error("Error getting # of console events");
        }

        if(!numevents)
        {
            break;
        }

        if(ReadConsoleInput(hinput, recs, 1, &numread) == 0)
        {
            Sys_Error("Error reading console input");
        }

        if(numread != 1)
        {
            Sys_Error("Couldn't read console input");
        }

        if(recs[0].EventType == KEY_EVENT)
        {
            if(recs[0].Event.KeyEvent.bKeyDown == FALSE)
            {
                ch = recs[0].Event.KeyEvent.uChar.AsciiChar;

                switch(ch)
                {
                    case '\r':
                        WriteFile(houtput, "\r\n", 2, &dummy, nullptr);

                        if(textlen != 0)
                        {
                            con_text[textlen] = 0;
                            textlen = 0;
                            return con_text;
                        }

                        break;

                    case '\b':
                        WriteFile(houtput, "\b \b", 3, &dummy, nullptr);
                        if(textlen != 0)
                        {
                            textlen--;
                        }

                        break;

                    default:
                        if(ch >= ' ')
                        {
                            WriteFile(houtput, &ch, 1, &dummy, nullptr);
                            con_text[textlen] = ch;
                            textlen = (textlen + 1) & 0xff;
                        }

                        break;
                }
            }
        }
    }

    return nullptr;
}

void Sys_Sleep(unsigned long msecs)
{
    /*	Sleep (msecs);*/
    SDL_Delay(msecs);
}

void Sys_SendKeyEvents()
{
    IN_Commands(); // ericw -- allow joysticks to add keys so they can be used
                   // to confirm SCR_ModalMessage
    IN_SendKeyEvents();
}
