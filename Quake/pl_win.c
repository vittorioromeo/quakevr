/*
Copyright (C) 1996-2001 Id Software, Inc.
Copyright (C) 2002-2005 John Fitzgibbons and others
Copyright (C) 2007-2008 Kristian Duske
Copyright (C) 2010-2014 QuakeSpasm developers

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
#include <windows.h>
#if defined(SDL_FRAMEWORK) || defined(NO_SDL_CONFIG)
#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>
#else
#include "SDL.h"
#include "SDL_syswm.h"
#endif

// QVR: the exe's "icon" resource (Windows/QuakeVR.ico), loaded at the system's large and small icon
// sizes so the title bar, taskbar and alt-tab pick the matching frame instead of a rescaled 32x32
static HICON icon;
static HICON icon_small;

void PL_SetWindowIcon (void)
{
	HINSTANCE handle;
	SDL_SysWMinfo wminfo;
	HWND hwnd;

	handle = GetModuleHandle(NULL);
	if (!icon)
		icon = (HICON) LoadImage(handle, "icon", IMAGE_ICON, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), 0);
	if (!icon_small)
		icon_small = (HICON) LoadImage(handle, "icon", IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);

	if (!icon)
		return;	/* no icon in the exe */

	SDL_VERSION(&wminfo.version);

	if (SDL_GetWindowWMInfo((SDL_Window*) VID_GetWindow(), &wminfo) != SDL_TRUE)
		return;	/* wrong SDL version */

	hwnd = wminfo.info.win.window;
#ifdef _WIN64
	SetClassLongPtr(hwnd, GCLP_HICON, (LONG_PTR) icon);
	if (icon_small)
		SetClassLongPtr(hwnd, GCLP_HICONSM, (LONG_PTR) icon_small);
#else
	SetClassLong(hwnd, GCL_HICON, (LONG) icon);
	if (icon_small)
		SetClassLong(hwnd, GCL_HICONSM, (LONG) icon_small);
#endif
	SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM) icon);
	SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM) (icon_small ? icon_small : icon));
}

void PL_VID_Shutdown (void)
{
	DestroyIcon(icon);
	icon = NULL;
	if (icon_small)
		DestroyIcon(icon_small);
	icon_small = NULL;
}

#define MAX_CLIPBOARDTXT	MAXCMDLINE	/* 256 */
char *PL_GetClipboardData (void)
{
	char *data = NULL;
	char *cliptext;

	if (OpenClipboard(NULL) != 0)
	{
		HANDLE hClipboardData;

		if ((hClipboardData = GetClipboardData(CF_TEXT)) != NULL)
		{
			cliptext = (char *) GlobalLock(hClipboardData);
			if (cliptext != NULL)
			{
				size_t size = GlobalSize(hClipboardData) + 1;
			/* this is intended for simple small text copies
			 * such as an ip address, etc:  do chop the size
			 * here, otherwise we may experience Z_Malloc()
			 * failures and all other not-oh-so-fun stuff. */
				size = q_min((size_t)(MAX_CLIPBOARDTXT), size);
				data = (char *) Z_Malloc((int)size);
				q_strlcpy (data, cliptext, size);
				GlobalUnlock (hClipboardData);
			}
		}
		CloseClipboard ();
	}
	return data;
}

static wchar_t error_buffer[4096];
static char error_text[4096];

void PL_ErrorDialog(const char *errorMsg)
{
	wchar_t *msg;
	if (VR_ErrorDialogSuppressed (errorMsg)) // QVR: automated test runs quit on an error instead of waiting on a dialog
		return;
	if (VR_LastCrashReport ()[0]) // QVR: where its report is (Sys_ReportError, VR_ErrorReport)
	{
		const char *report = VR_LastCrashReport ();
		size_t len = strlen (report);
		q_snprintf (error_text, sizeof (error_text),
			"%s\n\nA crash report was saved:\n%s\n%.*s.dmp\n\nPlease attach both files to your bug report, with what you were doing.",
			errorMsg, report, (int)(len > 4 ? len - 4 : len), report);
		errorMsg = error_text;
	}
	if (!MultiByteToWideChar (CP_UTF8, 0, errorMsg, -1, error_buffer, countof (error_buffer)))
		msg = L"An unknown error occurred";
	else
		msg = error_buffer;
	MessageBoxW (NULL, msg, L"Quake VR: Unleashed - Error", MB_OK | MB_SETFOREGROUND | MB_ICONSTOP);
}

