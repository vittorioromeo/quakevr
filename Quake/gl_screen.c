/*
Copyright (C) 1996-2001 Id Software, Inc.
Copyright (C) 2002-2009 John Fitzgibbons and others
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

// screen.c -- master for refresh, status bar, console, chat, notify, etc

#include "quakedef.h"
#include "steam.h"
#include "vr/vr_api.h"
#include "vr/vr_api_render.h"
#include <time.h>

/*

background clear
rendering
turtle/net/ram icons
sbar
centerprint / slow centerprint
notify lines
intermission / finale overlay
loading plaque
console
menu

required background clears
required update regions


syncronous draw mode or async
One off screen buffer, with updates either copied or xblited
Need to double buffer?


async draw will require the refresh area to be cleared, because it will be
xblited, but sync draw can just ignore it.

sync
draw

CenterPrint ()
SlowPrint ()
Screen_Update ();
Con_Printf ();

net
turn off messages option

the refresh is allways rendered, unless the console is full screen


console is:
	notify lines
	half
	full

*/


// seat-shared: the window's (its size, the loading plaque, the dialog, the pics, the HUD style cvar's)
int			glx, gly, glwidth, glheight;

float		scr_con_current;
float		scr_conlines;		// lines of console to display

//johnfitz -- new cvars
cvar_t		scr_menuscale = {"scr_menuscale", "1", CVAR_ARCHIVE};
cvar_t		scr_menubgalpha = {"scr_menubgalpha", "0.7", CVAR_ARCHIVE};
cvar_t		scr_menubgstyle = {"scr_menubgstyle", "-1", CVAR_ARCHIVE};
cvar_t		scr_centerprintbg = {"scr_centerprintbg", "2", CVAR_ARCHIVE}; // 0 = off; 1 = text box; 2 = menu box; 3 = menu strip
cvar_t		scr_sbarscale = {"scr_sbarscale", "1", CVAR_ARCHIVE};
cvar_t		scr_sbaralpha = {"scr_sbaralpha", "0.75", CVAR_ARCHIVE};
cvar_t		scr_conwidth = {"scr_conwidth", "0", CVAR_ARCHIVE};
cvar_t		scr_conscale = {"scr_conscale", "1", CVAR_ARCHIVE};
cvar_t		scr_crosshairscale = {"scr_crosshairscale", "1", CVAR_ARCHIVE};
cvar_t		scr_infoscale = {"scr_infoscale", "2.0", CVAR_ARCHIVE};
cvar_t		scr_pixelaspect = {"scr_pixelaspect", "1", CVAR_ARCHIVE};
cvar_t		scr_showfps = {"scr_showfps", "0", CVAR_ARCHIVE};
cvar_t		scr_showspeed = {"scr_showspeed", "0", CVAR_ARCHIVE};
cvar_t		scr_showspeed_ofs = {"scr_showspeed_ofs", "0", CVAR_ARCHIVE};
cvar_t		scr_clock = {"scr_clock", "0", CVAR_ARCHIVE};
//johnfitz
cvar_t		scr_usekfont = {"scr_usekfont", "0", CVAR_NONE}; // 2021 re-release

cvar_t		scr_hudstyle = {"hudstyle", "2", CVAR_ARCHIVE};
cvar_t		cl_screenshotname = {"cl_screenshotname", "screenshots/%map%_%date%_%time%", CVAR_ARCHIVE};
cvar_t		scr_demobar_timeout = {"scr_demobar_timeout", "1", CVAR_ARCHIVE};

cvar_t		scr_viewsize = {"viewsize","100", CVAR_ARCHIVE};
// QVR: splitscreen phase 7 (docs/vr-port/SPLITSCREEN.md section 15): the seats' tiles. 0: from the window's aspect (an
// ultrawide, 2:1 or wider: side by side; else 2 seats stacked, 3 one wide on top and two below, 4 a grid); 1 side by
// side, 2 stacked, 3 one wide on top and the others below, 4 a 2x2 grid.
cvar_t		scr_splitlayout = {"scr_splitlayout", "0", CVAR_ARCHIVE};
cvar_t		scr_seatmarkers = {"scr_seatmarkers", "0", CVAR_NONE}; // QVR: each tile marked with its seat's colour (1); 2 also read back after the frame (vr_seat_render_test)
cvar_t		scr_fov = {"fov","90",CVAR_ARCHIVE};	// 10 - 170
cvar_t		scr_fov_adapt = {"fov_adapt","1",CVAR_ARCHIVE};
cvar_t		scr_zoomfov = {"zoom_fov","30",CVAR_ARCHIVE};	// 10 - 170
cvar_t		scr_zoomspeed = {"zoom_speed","8",CVAR_ARCHIVE};
cvar_t		scr_conspeed = {"scr_conspeed","2000",CVAR_ARCHIVE};
cvar_t		scr_centertime = {"scr_centertime","2",CVAR_NONE};
cvar_t		scr_showturtle = {"showturtle","0",CVAR_NONE};
cvar_t		scr_showpause = {"showpause","1",CVAR_NONE};
cvar_t		scr_printspeed = {"scr_printspeed","8",CVAR_NONE};
cvar_t		gl_triplebuffer = {"gl_triplebuffer", "1", CVAR_ARCHIVE};

cvar_t		cl_gun_fovscale = {"cl_gun_fovscale","1",CVAR_ARCHIVE}; // Qrack
cvar_t		cl_gun_x = {"cl_gun_x","0",CVAR_ARCHIVE};
cvar_t		cl_gun_y = {"cl_gun_y","0",CVAR_ARCHIVE};
cvar_t		cl_gun_z = {"cl_gun_z","0",CVAR_ARCHIVE};

extern	char	crosshair_char;
extern	cvar_t	crosshair;
extern	cvar_t	con_notifyfade;
extern	cvar_t	con_notifyfadetime;

extern	edict_t	**bbox_linked;
extern	cvar_t	r_showfields;
extern	cvar_t	r_showfields_align;

// seat-shared: the window's (its size, the loading plaque, the dialog, the pics, the HUD style cvar's)
qboolean	scr_initialized;		// ready to draw

// seat-shared: the window's (its size, the loading plaque, the dialog, the pics, the HUD style cvar's)
qpic_t		*scr_net;
// seat-shared: the window's (its size, the loading plaque, the dialog, the pics, the HUD style cvar's)
qpic_t		*scr_turtle;

int			clearconsole;
int			clearnotify;


// seat-shared: the window's (its size, the loading plaque, the dialog, the pics, the HUD style cvar's)
qboolean	scr_disabled_for_loading;
// seat-shared: the window's (its size, the loading plaque, the dialog, the pics, the HUD style cvar's)
qboolean	scr_drawloading;
// seat-shared: the window's (its size, the loading plaque, the dialog, the pics, the HUD style cvar's)
float		scr_disabled_time;

// seat-shared: the window's (its size, the loading plaque, the dialog, the pics, the HUD style cvar's)
int	scr_tileclear_updates = 0; //johnfitz

// seat-shared: the window's (its size, the loading plaque, the dialog, the pics, the HUD style cvar's)
hudstyle_t	hudstyle;

static void SCR_ScreenShot_f (void);

/*
===============================================================================

CENTER PRINTING

===============================================================================
*/

// QVR: splitscreen phase 3: the centre print (scr_centerstring, its times and sizes) is the seat's HUD (seathud_t,
// client.h); scr_vrect the seat's view (seatview_t's viewrect).

/*
==============
SCR_GetCenterPrintWrapLimit

Returns the maximum number of chars to be printed on a single line.
==============
*/
static int SCR_GetCenterPrintWrapLimit (void)
{
	drawtransform_t transform;
	float left, top, right, bottom;
	float width;

	Draw_GetCanvasTransform (CANVAS_MENU, &transform);
	Draw_GetTransformBounds (&transform, &left, &top, &right, &bottom);
	width = right - left;

	// avoid overlong lines
	width = LERP (320.0f, width, 0.25f);
	width = CLAMP (320.0f, width, 400.0f);

	return ((int) width) >> 3;
}

/*
==============
SCR_CenterPrint

Called for important messages that should stay in the center of the screen
for a few moments
==============
*/
void SCR_CenterPrint (const char *str) //update centerprint data
{
	seathud_t *hud = SCR_SeatHud (); // QVR: splitscreen: the current seat's
	int cols;

	VR_GameCenterPrint (str); // QVR: the wrist gadget's hologram shows the game's

	cols = scr_usekfont.value ? SCR_GetCenterPrintWrapLimit () : 0;
	COM_WordWrap (hud->centerstring, str, sizeof (hud->centerstring), cols);

	if (!hud->centerstring[0])
	{
		hud->center_lines = 0;
		hud->center_maxcols = 0;
		return;
	}

	hud->centertime_off = scr_centertime.value;
	hud->centertime_start = cl.time;
	if (!cl.intermission)
		hud->centertime_off += q_max (0.f, con_notifyfade.value * con_notifyfadetime.value);

// count the number of lines for centering
	hud->center_lines = 1;
	hud->center_maxcols = 0;
	str = hud->centerstring;
	cols = 0;
	while (*str)
	{
		if (*str == '\n')
		{
			hud->center_lines++;
			hud->center_maxcols = q_max (hud->center_maxcols, cols);
			cols = -1; // compensate the following ++
		}
		str++;
		cols++;
	}
	hud->center_maxcols = q_max (hud->center_maxcols, cols);
}

static void SCR_DrawCenterStringBG (seathud_t *hud, int y, float alpha)
{
	const char *str;
	int i, len, lines, x;

	if (cl.intermission || q_min (hud->center_lines, hud->center_maxcols) <= 0 || alpha <= 0.f)
		return;

	// skip leading empty lines (might be there just to reposition the text)
	str = hud->centerstring;
	while (*str == '\n')
	{
		str++;
		y += CHARSIZE;
	}

	// skip trailing empty lines
	len = (int) strlen (str);
	while (len > 0 && str[len - 1] == '\n')
		--len;

	// count remaining lines
	for (i = 0, lines = 1; i < len; i++)
		if (str[i] == '\n')
			lines++;

	// draw the background
	switch ((int)scr_centerprintbg.value)
	{
	case 1:
		len = (hud->center_maxcols + 3) & ~1;
		x = (320 - len * 8) / 2;
		GL_PushCanvasColor (1.f, 1.f, 1.f, alpha * scr_menubgalpha.value);
		M_DrawTextBox (x - 8, y - 12, len, lines + 1);
		GL_PopCanvasColor ();
		break;

	case 2:
		len = hud->center_maxcols + 2;
		x = (320 - len* 8) / 2;
		Draw_PartialFadeScreen (x, x + len * 8, y - 4, y + lines * 8 + 4, alpha);
		break;

	case 3:
		Draw_PartialFadeScreen (glcanvas.left, glcanvas.right, y - 4, y + lines * 8 + 4, alpha);
		break;

	default:
		return;
	}
}

// QVR: the centre print's fade out at the end of its time (con_notifyfade; shared with the visor's popup)
static float SCR_CenterPrintFade (seathud_t *hud)
{
	float fade = q_max (con_notifyfade.value * con_notifyfadetime.value, 0.f);
	return fade ? q_min (hud->centertime_off / fade, 1.f) : 1.f;
}

static void SCR_DrawCenterString (seathud_t *hud) //actually do the drawing
{
	char	*start;
	int		l;
	int		j;
	int		x, y;
	int		remaining;
	float	alpha, forcedalpha;

	GL_SetCanvas (CANVAS_MENU); //johnfitz

// the finale prints the characters one at a time
	if (cl.intermission)
	{
		remaining = scr_printspeed.value * (cl.time - hud->centertime_start);
		alpha = 1.f;
	}
	else
	{
		remaining = 9999;
		alpha = SCR_CenterPrintFade (hud);
	}

	if (M_ForcedCenterPrint (&forcedalpha))
		alpha *= forcedalpha * forcedalpha;

	GL_PushCanvasColor (1.f, 1.f, 1.f, alpha);

	hud->erase_center = 0;
	start = hud->centerstring;

	if (hud->center_lines <= 4)
		y = 200*0.35;	//johnfitz -- 320x200 coordinate system
	else
		y = 48;
	if (crosshair.value && scr_viewsize.value < 130)
		y -= 8;

	SCR_DrawCenterStringBG (hud, y, alpha);

	do
	{
	// scan the width of the line
		for (l=0 ; start[l] ; l++)
			if (start[l] == '\n')
				break;
		x = (320 - l*8)/2;	//johnfitz -- 320x200 coordinate system
		for (j=0 ; j<l ; j++, x+=8)
		{
			Draw_Character (x, y, start[j]);	//johnfitz -- stretch overlays
			if (!remaining--)
				return;
		}

		y += 8;
		start += l;

		if (!*start)
			break;
		start++;		// skip the \n
	} while (1);

	GL_PopCanvasColor ();
}

// QVR: a centre print shown `seconds` at least (QC viewmessage without a headset: vr_worldtext.cpp).
void SCR_CenterPrintFor (const char *str, float seconds)
{
	seathud_t *hud = SCR_SeatHud (); // QVR: splitscreen: the current seat's
	SCR_CenterPrint (str);
	if (hud->center_lines > 0)
		hud->centertime_off = q_max (hud->centertime_off, seconds);
}

static void SCR_CheckDrawCenterString (seathud_t *hud)
{
	if (hud->center_lines > hud->erase_lines)
		hud->erase_lines = hud->center_lines;

	hud->centertime_off -= host_frametime;

	if (hud->centertime_off <= 0 && !cl.intermission)
		return;
	if (!M_ForcedCenterPrint (NULL))
	{
		if (Key_SeatDest () != key_game) // QVR: splitscreen phase 9: the seat's own (its seat menu, or seat 1's menus)
			return;
		if (cl.paused) //johnfitz -- don't show centerprint during a pause
			return;
		if (VR_CenterPrintOnWrist ()) // QVR: the wrist gadget's hologram shows it
			return;
		if (VR_CenterPrintOnVisor (hud->centerstring, SCR_CenterPrintFade (hud))) // QVR: flatscreen play's visor's popup shows it
			return;
		if (VR_CenterPrintInView (hud->centerstring, SCR_CenterPrintFade (hud))) // QVR: the headset's panel, in the flat visor popup's style (vr_panel.cpp)
			return;
	}

	SCR_DrawCenterString (hud);
}

// QVR: a centre print is up (in view or in the wrist's hologram): new tips wait until it is gone (vr_tips.cpp).
qboolean SCR_CenterPrintShowing (void)
{
	seathud_t *hud = SCR_SeatHud (); // QVR: splitscreen: the current seat's
	return hud->center_lines > 0 && hud->centertime_off > 0.f && !cl.intermission;
}

void SCR_ClearCenterString (void)
{
	seathud_t *hud = SCR_SeatHud (); // QVR: splitscreen: the current seat's
	hud->centertime_off = 0;
}

//=============================================================================

/*
====================
SCR_ToggleZoom_f
====================
*/
static void SCR_ToggleZoom_f (void)
{
	if (cl.zoomdir)
		cl.zoomdir = -cl.zoomdir;
	else
		cl.zoomdir = cl.zoom > 0.5f ? -1.f : 1.f;
}

/*
====================
SCR_ZoomDown_f
====================
*/
static void SCR_ZoomDown_f (void)
{
	cl.zoomdir = 1.f;
}

/*
====================
SCR_ZoomUp_f
====================
*/
static void SCR_ZoomUp_f (void)
{
	cl.zoomdir = -1.f;
}

/*
====================
SCR_UpdateZoom
====================
*/
void SCR_UpdateZoom (void)
{
	float speed = scr_zoomspeed.value > 0.f ? scr_zoomspeed.value : 1e6;
	float delta = cl.zoomdir * speed * (cl.time - cl.oldtime);
	if (!delta)
		return;
	cl.zoom += delta;
	if (cl.zoom >= 1.f)
	{
		cl.zoom = 1.f;
		cl.zoomdir = 0.f;
	}
	else if (cl.zoom <= 0.f)
	{
		cl.zoom = 0.f;
		cl.zoomdir = 0.f;
	}
	vid.recalc_refdef = 1;
}

/*
====================
AdaptFovx
Adapt a 4:3 horizontal FOV to the current screen size using the "Hor+" scaling:
2.0 * atan(width / height * 3.0 / 4.0 * tan(fov_x / 2.0))
====================
*/
float AdaptFovx (float fov_x, float width, float height)
{
	float	a, x;

	if (fov_x < 1 || fov_x > 179)
		Sys_Error ("Bad fov: %f", fov_x);

	if (!scr_fov_adapt.value)
		return fov_x;
	if ((x = height / width) == 0.75)
		return fov_x;
	a = atan(0.75 / x * tan(fov_x / 360 * M_PI));
	a = a * 360 / M_PI;
	return a;
}

/*
====================
CalcFovy
====================
*/
float CalcFovy (float fov_x, float width, float height)
{
	float	a, x;

	if (fov_x < 1 || fov_x > 179)
		Sys_Error ("Bad fov: %f", fov_x);

	x = width / tan(fov_x / 360 * M_PI);
	a = atan(height / x);
	a = a * 360 / M_PI;
	return a;
}

/*
=================
SCR_CalcRefdef

Must be called whenever vid changes
Internal use only
=================
*/
/*
==================
SCR_SeatRect and the tiles -- QVR

Splitscreen phase 7 (docs/vr-port/SPLITSCREEN.md section 15): in flatscreen play (no headset) each seat in play is drawn
in its tile of the window, laid out by scr_splitlayout (its default from the window's aspect: his answer, 2026-11-21).
While seats are drawn in tiles, each seat's pass makes its tile the window (SCR_SeatPassBegin: glx, gly, glwidth,
glheight, vid's sizes): the view, the refdef, the status bar and the crosshair are laid out in it by the code that lays
them out in the window. One seat: the whole window, the frame drawn as it always was.
==================
*/
static vrect_t scr_window; // seat-shared: the window this frame (GL_BeginRendering's; the tiles' parent while a seat's pass shrinks glwidth)

void SCR_SeatWindow (vrect_t *out)
{
	out->x = 0;
	out->y = 0;
	out->width = scr_window.width > 0 ? scr_window.width : glwidth;
	out->height = scr_window.height > 0 ? scr_window.height : glheight;
}

int SCR_SeatsDrawn (void)
{
	if (VR_IsActive ()) // (a headset's view is seat 0's: a VR host with flat seats is phase 14's)
		return 1;
	return q_max (1, CL_SeatsInPlay ());
}

int SCR_SplitLayout (void)
{
	vrect_t win;
	int n = SCR_SeatsDrawn (), layout = (int) scr_splitlayout.value;
	if (layout >= 1 && layout <= 4)
		return layout;
	SCR_SeatWindow (&win);
	if (win.width >= 2 * win.height) // an ultrawide: side by side, always
		return 1;
	return n <= 2 ? 2 : n == 3 ? 3 : 4;
}

// Tile i of n in a w x h window, layout 1-4 (SCR_SplitLayout's).
static void SCR_LayoutTile (int layout, int n, int i, int w, int h, vrect_t *out)
{
	int cols = 1, rows = 1, col = 0, row = 0;
	if (layout == 3 && n >= 3) // one wide on top, the others side by side below
	{
		if (i == 0)
		{
			out->x = 0;
			out->y = 0;
			out->width = w;
			out->height = h / 2;
			return;
		}
		out->x = (i - 1) * w / (n - 1);
		out->width = i * w / (n - 1) - out->x;
		out->y = h / 2;
		out->height = h - h / 2;
		return;
	}
	if (layout == 1)
	{
		cols = n;
		col = i;
	}
	else if (layout == 4 && n > 2)
	{
		cols = 2;
		rows = 2;
		col = i % 2;
		row = i / 2;
	}
	else if (layout == 4)
	{
		cols = n;
		col = i;
	}
	else // 2, and 3 with two seats: stacked
	{
		rows = n;
		row = i;
	}
	out->x = col * w / cols;
	out->width = (col + 1) * w / cols - out->x;
	out->y = row * h / rows;
	out->height = (row + 1) * h / rows - out->y;
}

void SCR_SeatRect (int seat, vrect_t *out)
{
	vrect_t win;
	int n = SCR_SeatsDrawn (), i = 0, s;
	SCR_SeatWindow (&win);
	*out = win;
	if (n <= 1 || seat < 0 || seat >= MAX_SPLITSEATS || !CL_SeatPlays (seat))
		return;
	for (s = 0; s < seat; s++) // (its place among the seats in play: seat 3 is the second tile when seat 2 is out)
		i += CL_SeatPlays (s) ? 1 : 0;
	SCR_LayoutTile (SCR_SplitLayout (), n, i, win.width, win.height, out);
}

// A seat's pass: its tile made the window, and back.
typedef struct
{
	int		glx, gly, glwidth, glheight;
	int		width, height, guiwidth, guiheight;
} scrwindow_t;

static void SCR_SeatPassBegin (const vrect_t *tile, scrwindow_t *saved)
{
	saved->glx = glx;
	saved->gly = gly;
	saved->glwidth = glwidth;
	saved->glheight = glheight;
	saved->width = vid.width;
	saved->height = vid.height;
	saved->guiwidth = vid.guiwidth;
	saved->guiheight = vid.guiheight;
	glx = saved->glx + tile->x;
	gly = saved->gly + saved->glheight - tile->y - tile->height;
	glwidth = tile->width;
	glheight = tile->height;
	vid.width = q_max (1, saved->width * tile->width / q_max (1, saved->glwidth));
	vid.height = q_max (1, saved->height * tile->height / q_max (1, saved->glheight));
	vid.guiwidth = q_max (1, saved->guiwidth * tile->width / q_max (1, saved->glwidth));
	vid.guiheight = q_max (1, saved->guiheight * tile->height / q_max (1, saved->glheight));
	vid.recalc_refdef = 1;
}

static void SCR_SeatPassEnd (const scrwindow_t *saved)
{
	glx = saved->glx;
	gly = saved->gly;
	glwidth = saved->glwidth;
	glheight = saved->glheight;
	vid.width = saved->width;
	vid.height = saved->height;
	vid.guiwidth = saved->guiwidth;
	vid.guiheight = saved->guiheight;
	vid.recalc_refdef = 1;
}

// The markers' colours: seat 1 red, 2 green, 3 blue, 4 yellow (vr_seat_render_test checks them).
static const float scr_seatcolors[MAX_SPLITSEATS][3] = {{1.f, 0.f, 0.f}, {0.f, 1.f, 0.f}, {0.f, 0.f, 1.f}, {1.f, 1.f, 0.f}};
static const int scr_markerat = 20; // the marker's middle, in the tile's canvas units from its top left (its read-back)

static void SCR_DrawSeatMarker (int seat)
{
	GL_SetCanvas (CANVAS_DEFAULT);
	Draw_FillEx (scr_markerat - 12, scr_markerat - 12, 24, 24, scr_seatcolors[seat], 1.f);
	Draw_String (scr_markerat + 18, scr_markerat - 4, va ("seat %d", seat + 1));
}

// scr_seatmarkers 2 (vr_seat_render_test), in the seat's pass (its tile the window) once its view, HUD and marker are
// drawn, before the shared 2D: its marker's pixel and a checksum of its middle read back (a test's stall, never else).
static void SCR_SeatProbe (seatview_t *sv)
{
	byte block[32 * 32 * 3];
	unsigned hash = 2166136261u;
	int i;
	glPixelStorei (GL_PACK_ALIGNMENT, 1);
	glReadPixels (glx + scr_markerat * glwidth / q_max (1, vid.guiwidth),
		gly + glheight - 1 - scr_markerat * glheight / q_max (1, vid.guiheight), 1, 1, GL_RGB, GL_UNSIGNED_BYTE, sv->probe);
	glReadPixels (glx + glwidth / 2 - 16, gly + glheight / 2 - 16, 32, 32, GL_RGB, GL_UNSIGNED_BYTE, block);
	glPixelStorei (GL_PACK_ALIGNMENT, 4);
	for (i = 0; i < (int) sizeof (block); i++)
		hash = (hash ^ block[i]) * 16777619u;
	sv->probesum = hash;
}

static void SCR_CalcRefdef (void)
{
	float		size, scale; //johnfitz -- scale
	float		zoom;
	seatview_t	*sv = V_SeatView (); // QVR: splitscreen: the seat's view, drawn in its window area
	vrect_t		area = {0, 0, glwidth, glheight}; // QVR: the window, or the seat's tile while its pass makes it the window
	vrect_t		*rect = &area;

	SCR_SeatRect (CL_SeatIndex (), &sv->rect); // QVR: (its tile in the window)

// force the status bar to redraw
	Sbar_Changed ();

	scr_tileclear_updates = 0; //johnfitz

// bound viewsize
	if (scr_viewsize.value < 30)
		Cvar_SetQuick (&scr_viewsize, "30");
	if (scr_viewsize.value > 130)
		Cvar_SetQuick (&scr_viewsize, "130");

// bound fov
	if (scr_fov.value < 10)
		Cvar_SetQuick (&scr_fov, "10");
	if (scr_fov.value > 170)
		Cvar_SetQuick (&scr_fov, "170");
	if (scr_zoomfov.value < 10)
		Cvar_SetQuick (&scr_zoomfov, "10");
	if (scr_zoomfov.value > 170)
		Cvar_SetQuick (&scr_zoomfov, "170");

	vid.recalc_refdef = 0;

	//johnfitz -- rewrote this section
	size = scr_viewsize.value;
	scale = CLAMP (1.0f, scr_sbarscale.value, (float)rect->width / 320.0f); // QVR: the seat's area (was glwidth)
	scale *= (float) vid.height / vid.guiheight;

	if (size >= 120 || cl.intermission || scr_sbaralpha.value < 1 || hudstyle != HUD_CLASSIC || cl.qcvm.extfuncs.CSQC_DrawHud //johnfitz -- scr_sbaralpha.value
		|| VR_VisorOn ()) // QVR: flatscreen play's visor HUD keeps no status bar's room (vr/vr_visor.cpp)
		sb_lines = 0;
	else if (size >= 110)
		sb_lines = 24 * scale;
	else
		sb_lines = 48 * scale;

	size = q_min(scr_viewsize.value, 100.f) / 100;
	//johnfitz

	//johnfitz -- rewrote this section
	// QVR: in the seat's window area (the whole window with one seat: glwidth, glheight as before)
	r_refdef.vrect.width = q_max(rect->width * size, 96.0f); //no smaller than 96, for icons
	r_refdef.vrect.height = q_min((int)(rect->height * size), rect->height - sb_lines); //make room for sbar
	r_refdef.vrect.x = rect->x + (rect->width - r_refdef.vrect.width)/2;
	r_refdef.vrect.y = rect->y + (rect->height - sb_lines - r_refdef.vrect.height)/2;
	//johnfitz

	zoom = cl.zoom;
	zoom *= zoom * (3.f - 2.f * zoom); // smoothstep
	// QVR: on a flatscreen (vr_flatplay.cpp) the ironsight narrows this (vr_flat_aim_fov, vr_flatsight.cpp); in the
	// headset, or with it off, VR_FlatAdsFov returns the LERP unchanged. in_sdl.c IN_FovScale reads basefov, so the
	// mouse look's speed and the crosshair's distance on the screen keep their measure.
	r_refdef.basefov = VR_FlatAdsFov (LERP (scr_fov.value, scr_zoomfov.value, zoom));
	r_refdef.fov_x = AdaptFovx (r_refdef.basefov, vid.width, vid.height);
	r_refdef.fov_y = CalcFovy (r_refdef.fov_x, r_refdef.vrect.width, r_refdef.vrect.height);

	sv->viewrect = r_refdef.vrect; // QVR: the seat's (scr_vrect)
}


/*
=================
SCR_SizeUp_f

Keybinding command
=================
*/
void SCR_SizeUp_f (void)
{
	Cvar_SetValueQuick (&scr_viewsize, scr_viewsize.value+10);
}


/*
=================
SCR_SizeDown_f

Keybinding command
=================
*/
void SCR_SizeDown_f (void)
{
	Cvar_SetValueQuick (&scr_viewsize, scr_viewsize.value-10);
}

static void SCR_Callback_refdef (cvar_t *var)
{
	vid.recalc_refdef = 1;
}

/*
==================
SCR_Conwidth_f -- johnfitz -- called when scr_conwidth or scr_conscale changes
==================
*/
void SCR_Conwidth_f (cvar_t *var)
{
	vid.recalc_refdef = 1;
	VID_RecalcConsoleSize ();
}

/*
==================
SCR_AutoScale_f

Sets UI scale cvars to an automatic value based on resolution
==================
*/
void SCR_AutoScale_f (void)
{
	float scale = q_min (glwidth / 640.f, glheight / 480.f);
	scale = q_max (1.f, scale);
	Cvar_SetValueQuick (&scr_conscale, scale);
	Cvar_SetValueQuick (&scr_menuscale, scale);
	Cvar_SetValueQuick (&scr_sbarscale, scale);
	Cvar_SetValueQuick (&scr_crosshairscale, scale);
}

/*
==================
SCR_PixelAspect_f
==================
*/
void SCR_PixelAspect_f (cvar_t *cvar)
{
	vid.recalc_refdef = 1;
	VID_RecalcInterfaceSize ();
}

/*
==================
SCR_HUDStyle_f

Updates hudstyle variable and invalidates refdef when scr_hudstyle changes
==================
*/
void SCR_HUDStyle_f (cvar_t *cvar)
{
	int val = (int) cvar->value;
	hudstyle = (hudstyle_t) CLAMP (0, val, (int) HUD_COUNT - 1);
	vid.recalc_refdef = 1;
}

//============================================================================

/*
==================
SCR_LoadPics -- johnfitz
==================
*/
void SCR_LoadPics (void)
{
	scr_net = Draw_PicFromWad ("net");
	scr_turtle = Draw_PicFromWad ("turtle");
}

/*
==================
SCR_Init
==================
*/
void SCR_Init (void)
{
	//johnfitz -- new cvars
	Cvar_RegisterVariable (&scr_menuscale);
	Cvar_RegisterVariable (&scr_menubgalpha);
	Cvar_RegisterVariable (&scr_menubgstyle);
	Cvar_RegisterVariable (&scr_centerprintbg);
	Cvar_RegisterVariable (&scr_sbarscale);
	Cvar_SetCallback (&scr_sbaralpha, SCR_Callback_refdef);
	Cvar_RegisterVariable (&scr_sbaralpha);
	Cvar_SetCallback (&scr_conwidth, &SCR_Conwidth_f);
	Cvar_SetCallback (&scr_conscale, &SCR_Conwidth_f);
	Cvar_RegisterVariable (&scr_conwidth);
	Cvar_RegisterVariable (&scr_conscale);
	Cvar_RegisterVariable (&scr_crosshairscale);
	Cvar_RegisterVariable (&scr_infoscale);
	Cvar_RegisterVariable (&scr_showfps);
	Cvar_RegisterVariable (&scr_showspeed);
	Cvar_RegisterVariable (&scr_showspeed_ofs);
	Cvar_RegisterVariable (&scr_clock);
	Cvar_RegisterVariable (&cl_screenshotname);
	Cvar_RegisterVariable (&scr_demobar_timeout);
	//johnfitz
	Cvar_RegisterVariable (&scr_usekfont); // 2021 re-release
	Cvar_SetCallback (&scr_fov, SCR_Callback_refdef);
	Cvar_SetCallback (&scr_fov_adapt, SCR_Callback_refdef);
	Cvar_SetCallback (&scr_zoomfov, SCR_Callback_refdef);
	Cvar_SetCallback (&scr_viewsize, SCR_Callback_refdef);
	Cvar_SetCallback (&scr_hudstyle, SCR_HUDStyle_f);
	Cvar_RegisterVariable (&scr_hudstyle);
	Cvar_RegisterVariable (&scr_fov);
	Cvar_RegisterVariable (&scr_fov_adapt);
	Cvar_RegisterVariable (&scr_zoomfov);
	Cvar_RegisterVariable (&scr_zoomspeed);
	Cvar_RegisterVariable (&scr_viewsize);
	Cvar_RegisterVariable (&scr_splitlayout); // QVR
	Cvar_RegisterVariable (&scr_seatmarkers); // QVR
	Cvar_RegisterVariable (&scr_conspeed);
	Cvar_RegisterVariable (&scr_showturtle);
	Cvar_RegisterVariable (&scr_showpause);
	Cvar_RegisterVariable (&scr_centertime);
	Cvar_RegisterVariable (&scr_printspeed);
	Cvar_RegisterVariable (&gl_triplebuffer);
	Cvar_RegisterVariable (&cl_gun_fovscale);
	Cvar_RegisterVariable (&cl_gun_x);
	Cvar_RegisterVariable (&cl_gun_y);
	Cvar_RegisterVariable (&cl_gun_z);

	Cmd_AddCommand ("scr_autoscale",SCR_AutoScale_f);

	Cmd_AddCommand ("screenshot",SCR_ScreenShot_f);
	Cmd_AddCommand ("sizeup",SCR_SizeUp_f);
	Cmd_AddCommand ("sizedown",SCR_SizeDown_f);

	Cmd_AddCommand ("togglezoom", SCR_ToggleZoom_f);
	Cmd_AddCommand ("+zoom", SCR_ZoomDown_f);
	Cmd_AddCommand ("-zoom", SCR_ZoomUp_f);

	SCR_LoadPics (); //johnfitz

	scr_initialized = true;
}

//============================================================================

/*
==============
SCR_IsClockVisible
==============
*/
static qboolean SCR_IsClockVisible (void)
{
	return scr_clock.value || Sbar_ShowingScores ();
}

/*
==============
SCR_DrawFPS -- johnfitz
==============
*/
static void SCR_DrawFPS (void)
{
	static double	oldtime = 0;
	static double	lastfps = 0;
	static int	oldframecount = 0;
	double	elapsed_time;
	int	frames;

	if (con_forcedup)
	{
		oldtime = realtime;
		oldframecount = r_framecount;
		lastfps = 0;
		return;
	}

	elapsed_time = realtime - oldtime;
	frames = r_framecount - oldframecount;

	if (elapsed_time < 0 || frames < 0)
	{
		oldtime = realtime;
		oldframecount = r_framecount;
		return;
	}
	// update value every 3/4 second
	if (elapsed_time > 0.75)
	{
		lastfps = frames / elapsed_time;
		oldtime = realtime;
		oldframecount = r_framecount;
	}

	if (scr_showfps.value && scr_viewsize.value < 130 && lastfps)
	{
		char	st[16];
		int	x, y;
		if (scr_showfps.value > 0.f)
			sprintf (st, "%4.0f fps", lastfps);
		else
			sprintf (st, "%.2f ms", 1000.f / lastfps);
		x = 320 - (strlen(st)<<3);
		if (hudstyle != HUD_CLASSIC)
		{
			x = 320 - 16 - (strlen(st)<<3);
			y = 8;
			if (SCR_IsClockVisible ()) y += 8; //make room for clock
			GL_SetCanvas (CANVAS_TOPRIGHT);
		}
		else
		{
			x = 320 - (strlen(st)<<3);
			y = 200 - 8;
			if (SCR_IsClockVisible ()) y -= 8; //make room for clock
			GL_SetCanvas (CANVAS_BOTTOMRIGHT);
		}
		Draw_String (x, y, st);
		scr_tileclear_updates = 0;
	}
}

/*
==============
SCR_DrawNetHud -- QVR: (B13) the net HUD under the clock and the frame rate, its lines right-aligned (vr_net_hud;
vr/vr_nethud.cpp)
==============
*/
static void SCR_DrawNetHud (void)
{
	const char	*line;
	int		i, x, y;

	if (con_forcedup || scr_viewsize.value >= 130)
		return;
	for (i = 0; (line = VR_NetHudLine (i)) != NULL; i++)
	{
		if (hudstyle != HUD_CLASSIC)
		{
			y = 8 + 8 * i;
			if (SCR_IsClockVisible ()) y += 8;
			if (scr_showfps.value) y += 8;
			x = 320 - 16 - ((int) strlen (line) << 3);
			GL_SetCanvas (CANVAS_TOPRIGHT);
		}
		else
		{
			y = 200 - 8 - 8 * i;
			if (SCR_IsClockVisible ()) y -= 8;
			if (scr_showfps.value) y -= 8;
			x = 320 - ((int) strlen (line) << 3);
			GL_SetCanvas (CANVAS_BOTTOMRIGHT);
		}
		Draw_String (x, y, line);
		scr_tileclear_updates = 0;
	}
}

/*
==============
SCR_DrawSpeed
==============
*/
static void SCR_DrawSpeed (void)
{
	if (cl.intermission || CL_InCutscene () || scr_viewsize.value >= 130)
		return;

	const float show_speed_interval_value = 0.05f;
	static float maxspeed = 0, display_speed = -1;
	static double lastrealtime = 0;
	float speed;
	vec3_t vel;

	if (lastrealtime > realtime)
	{
		lastrealtime = 0;
		display_speed = -1;
		maxspeed = 0;
	}

	VectorCopy (cl.velocity, vel);
	vel[2] = 0;
	speed = VectorLength (vel);

	if (speed > maxspeed)
		maxspeed = speed;

	if (scr_showspeed.value)
	{
		if (display_speed >= 0)
		{
			float y;
			char str[12];

			sprintf (str, "%d", (int) display_speed);
			GL_SetCanvas (CANVAS_CROSSHAIR);
			y = CLAMP (glcanvas.top, 4.f + scr_showspeed_ofs.value, glcanvas.bottom - 8.f);
			Draw_String (-(int)strlen(str)*4, y, str);
		}
	}

	if (realtime - lastrealtime >= show_speed_interval_value)
	{
		lastrealtime = realtime;
		display_speed = maxspeed;
		maxspeed = 0;
	}
}

/*
==============
SCR_DrawClock -- johnfitz
==============
*/
static void SCR_DrawClock (void)
{
	char	str[12];

	if (SCR_IsClockVisible () && scr_viewsize.value < 130)
	{
		int minutes, seconds;

		minutes = cl.time / 60;
		seconds = ((int)cl.time)%60;

		sprintf (str,"%i:%i%i", minutes, seconds/10, seconds%10);
	}
	else
		return;

	//draw it
	if (hudstyle == HUD_CLASSIC)
	{
		GL_SetCanvas (CANVAS_BOTTOMRIGHT);
		Draw_String (320 - (strlen(str)<<3), 200 - 8, str);
	}
	else
	{
		GL_SetCanvas (CANVAS_TOPRIGHT);
		Draw_String (320 - 16 - (strlen(str)<<3), 8, str);
	}

	scr_tileclear_updates = 0;
}

/*
==============
SCR_PrintMirrored
==============
*/
static void SCR_PrintMirrored (int x, int y, const char *str)
{
	x += strlen (str) * 8;
	while (*str)
	{
		Draw_CharacterEx (x, y, -8, 8, 0x80 ^ (*str++));
		x -= 8;
	}
}

/*
==============
SCR_DrawDemoControls
==============
*/
static void SCR_DrawDemoControls (void)
{
	static const int	TIMEBAR_CHARS = 38;
	static float		prevspeed = 1.0f;
	static float		prevbasespeed = 1.0f;
	static float		showtime = 1.0f;
	int					i, len, x, y, min, sec;
	float				frac;
	const char			*str;
	char				name[31]; // size chosen to avoid overlap with side text

	if (!cls.demoplayback || scr_demobar_timeout.value < 0.f)
	{
		showtime = 0.f;
		return;
	}

	// Determine for how long the demo playback info should be displayed
	if (cls.demospeed != prevspeed || cls.basedemospeed != prevbasespeed ||			// speed/base speed changed
		fabs (cls.demospeed) > cls.basedemospeed ||									// fast forward/rewind
		!scr_demobar_timeout.value)													// controls always shown
	{
		prevspeed = cls.demospeed;
		prevbasespeed = cls.basedemospeed;
		showtime = scr_demobar_timeout.value > 0.f ? scr_demobar_timeout.value : 1.f;
	}
	else
	{
		showtime -= host_rawframetime;
		if (showtime < 0.f)
		{
			showtime = 0.f;
			return;
		}
	}

	// Approximate the fraction of the demo that's already been played back
	// based on the current file offset and total demo size
	// Note: we need to take into account the starting offset for pak files
	frac = (Sys_ftell (cls.demofile) - cls.demofilestart) / (double)cls.demofilesize;
	frac = CLAMP (0.f, frac, 1.f);

	if (cl.intermission)
	{
		GL_SetCanvas (CANVAS_MENU);
		y = LERP (glcanvas.bottom, glcanvas.top, 0.125f) + 8;
	}
	else
	{
		GL_SetCanvas (CANVAS_SBAR);
		y = glcanvas.bottom - 68;
	}
	x = (glcanvas.left + glcanvas.right) / 2 - TIMEBAR_CHARS / 2 * 8;

	// Draw status box background
	GL_SetCanvasColor (1.f, 1.f, 1.f, scr_sbaralpha.value);
	M_DrawTextBox (x - 8, y - 8, TIMEBAR_CHARS, 1);
	GL_SetCanvasColor (1.f, 1.f, 1.f, 1.f);

	// Print playback status on the left (paused/playing/fast-forward/rewind)
	// Note: character #13 works well as a forward symbol, but Alkaline 1.2 changes it to a disk.
	// If we have a custom conchars texture we switch to a safer alternative, the '>' character.
	if (!cls.demospeed)
		str = "II";
	else if (fabs (cls.demospeed) > 1.f)
		str = custom_conchars ? ">>" : "\xD\xD";
	else 
		str = custom_conchars ? ">" : "\xD";
	if (cls.demospeed >= 0.f)
		M_Print (x, y, str);
	else
		SCR_PrintMirrored (x, y, str);

	// Print base playback speed on the right
	if (!cls.basedemospeed)
		str = "";
	else if (fabs (cls.basedemospeed) >= 1.f)
		str = va ("%gx", fabs (cls.basedemospeed));
	else
		str = va ("1/%gx", 1.f / fabs (cls.basedemospeed));
	M_Print (x + (TIMEBAR_CHARS - strlen (str)) * 8, y, str);

	// Print demo name in the center
	COM_StripExtension (COM_SkipPath (cls.demofilename), name, sizeof (name));
	x = (glcanvas.left + glcanvas.right) / 2;
	M_Print (x - strlen (name) * 8 / 2, y, name);

	// Draw seek bar rail
	x = (glcanvas.left + glcanvas.right) / 2 - TIMEBAR_CHARS / 2 * 8;
	y -= 8;
	Draw_Character (x - 8, y, 128);
	for (i = 0; i < TIMEBAR_CHARS; i++)
		Draw_Character (x + i * 8, y, 129);
	Draw_Character (x + i * 8, y, 130);

	// Draw seek bar cursor
	x += (TIMEBAR_CHARS - 1) * 8 * frac;
	Draw_Character (x, y, 131);

	// Print current time above the cursor
	y -= 11;
	sec = (int) cl.time;
	min = sec / 60;
	sec %= 60;
	str = va ("%i:%02i", min, sec);
	x -= (strchr (str, ':') - str) * 8; // align ':' with cursor
	len = strlen (str);
	// M_DrawTextBox effectively rounds width up to a multiple of 2,
	// so if our length is odd we pad by half a character on each side
	GL_SetCanvasColor (1.f, 1.f, 1.f, scr_sbaralpha.value);
	M_DrawTextBox (x - 8 - (len & 1) * 8 / 2, y - 8, len + (len & 1), 1);
	GL_SetCanvasColor (1.f, 1.f, 1.f, 1.f);
	Draw_String (x, y, str);
}

/*
==============
SCR_DrawDevStats
==============
*/
static void SCR_DrawDevStats (void)
{
	char	str[40];
	int		y = 25-10; //10=number of lines to print
	int		x = 0; //margin

	if (!devstats.value)
		return;

	GL_SetCanvas (CANVAS_BOTTOMLEFT);

	Draw_Fill (x, y*8, 21*8, 10*8, 0, 0.5); //dark rectangle

	sprintf (str, "devstats | Curr  Peak");
	Draw_String (x, (y++)*8-x, str);

	sprintf (str, "---------+-----------");
	Draw_String (x, (y++)*8-x, str);

	sprintf (str, "Edicts   |%5i %5i", dev_stats.edicts, dev_peakstats.edicts);
	Draw_String (x, (y++)*8-x, str);

	sprintf (str, "Packet   |%5i %5i", dev_stats.packetsize, dev_peakstats.packetsize);
	Draw_String (x, (y++)*8-x, str);

	sprintf (str, "Visedicts|%5i %5i", dev_stats.visedicts, dev_peakstats.visedicts);
	Draw_String (x, (y++)*8-x, str);

	sprintf (str, "Efrags   |%5i %5i", dev_stats.efrags, dev_peakstats.efrags);
	Draw_String (x, (y++)*8-x, str);

	sprintf (str, "Dlights  |%5i %5i", dev_stats.dlights, dev_peakstats.dlights);
	Draw_String (x, (y++)*8-x, str);

	sprintf (str, "Beams    |%5i %5i", dev_stats.beams, dev_peakstats.beams);
	Draw_String (x, (y++)*8-x, str);

	sprintf (str, "Tempents |%5i %5i", dev_stats.tempents, dev_peakstats.tempents);
	Draw_String (x, (y++)*8-x, str);

	sprintf (str, "GL upload|%4iK %4iK", dev_stats.gpu_upload/1024, dev_peakstats.gpu_upload/1024);
	Draw_String (x, (y++)*8-x, str);
}

/*
==============
SCR_DrawTurtle
==============
*/
static void SCR_DrawTurtle (void)
{
	static int	count;

	if (!scr_showturtle.value)
		return;

	if (host_frametime < 0.1)
	{
		count = 0;
		return;
	}

	count++;
	if (count < 3)
		return;

	GL_SetCanvas (CANVAS_DEFAULT); //johnfitz

	Draw_Pic (V_SeatView ()->viewrect.x, V_SeatView ()->viewrect.y, scr_turtle); // QVR: the seat's view
}

/*
==============
SCR_DrawNet
==============
*/
static void SCR_DrawNet (void)
{
	if (realtime - cl.last_received_message < 0.3)
		return;
	if (cls.demoplayback)
		return;

	GL_SetCanvas (CANVAS_DEFAULT); //johnfitz

	Draw_Pic (V_SeatView ()->viewrect.x+64, V_SeatView ()->viewrect.y, scr_net); // QVR: the seat's view
}

/*
==============
DrawPause
==============
*/
static void SCR_DrawPause (void)
{
	qpic_t	*pic;
	float	alpha;
	qboolean recolor; // QVR

	if (!cl.paused)
		return;

	if (!scr_showpause.value)		// turn off for screenshots
		return;

	if (M_ForcedCenterPrint (&alpha))
	{
		alpha = 1.f - alpha;
		if (alpha <= 0.f)
			return;
	}
	else
		alpha = 1.f;

	GL_SetCanvas (CANVAS_MENU); //johnfitz
	GL_PushCanvasColor (1.f, 1.f, 1.f, alpha);
	recolor = Draw_SetMenuRecolor (true); // QVR: in the menus' red (vr_menu_recolor)

	pic = Draw_CachePic ("gfx/pause.lmp");
	Draw_Pic ( (320 - pic->width)/2, (240 - 48 - pic->height)/2, pic); //johnfitz -- stretched menus

	Draw_SetMenuRecolor (recolor); // QVR
	GL_PopCanvasColor ();

	scr_tileclear_updates = 0; //johnfitz
}

/*
==============
SCR_DrawLoading
==============
*/
static void SCR_DrawLoading (void)
{
	qpic_t	*pic;
	qboolean recolor; // QVR

	if (!scr_drawloading)
		return;

	GL_SetCanvas (CANVAS_MENU); //johnfitz
	recolor = Draw_SetMenuRecolor (true); // QVR: in the menus' red (vr_menu_recolor)

	pic = Draw_CachePic ("gfx/loading.lmp");
	Draw_Pic ( (320 - pic->width)/2, (240 - 48 - pic->height)/2, pic); //johnfitz -- stretched menus
	Draw_SetMenuRecolor (recolor); // QVR

	scr_tileclear_updates = 0; //johnfitz
}

/*
==============
SCR_DrawSaving
==============
*/
static void SCR_DrawSaving (void)
{
	int x, y;

	if (!Host_IsSaving ())
		return;

	GL_SetCanvas (CANVAS_TOPRIGHT);

	x = 320 - 16 - draw_disc->width;
	y = 8;
	if (hudstyle != HUD_CLASSIC && scr_viewsize.value < 130)
	{
		if (SCR_IsClockVisible ()) y += 8;
		if (scr_showfps.value) y += 8;
		if (y != 8)
			y += 8;
	}

	Draw_Pic (x, y, draw_disc);
}

/*
==============
SCR_DrawRelight -- QVR: the in-game relighting's progress, under the saving disc's corner (vr_relight.cpp)
==============
*/
static void SCR_DrawRelight (void)
{
	float fraction = 0.f;
	const char *text = VR_RelightIndicator (&fraction);
	int x, w;

	if (!text)
		return;

	GL_SetCanvas (CANVAS_TOPRIGHT);
	w = 8 * (int) strlen (text);
	x = 320 - 8 - w;
	Draw_String (x, 40, text);
	Draw_Fill (x, 50, w, 2, 4, 1.f);
	Draw_Fill (x, 50, (int) (w * CLAMP (0.f, fraction, 1.f)), 2, 192, 1.f);
}

/*
==============
SCR_DrawTakes -- QVR: flat play's take light and rating prompt, small in the top right corner (vr_takes.cpp)
==============
*/
static void SCR_DrawTakes (void)
{
	const float red[3] = {1.f, 0.1f, 0.1f};
	int light = 0;
	float s = 1.f, x;
	const char *text = VR_TakesIndicator (&light, &s);

	if (!text)
		return;

	GL_SetCanvas (CANVAS_TOPRIGHT); // (the top left holds the notify lines)
	x = 320.f - 4.f - 8.f * s * (float) strlen (text);
	if (light && fmod (realtime, 1.0) < 0.7)
		Draw_FillEx (x - 8.f * s, 4.f, 6.f * s, 6.f * s, red, 1.f);
	Draw_StringEx (x, 4.f, 8.f * s, text);
}

/*
==============
SCR_DrawCrosshair -- johnfitz
==============
*/
static void SCR_DrawCrosshair (void)
{
	if (cl.intermission || CL_InCutscene () || !crosshair.value || scr_viewsize.value >= 130)
		return;
	if (VR_FlatXhairOn ()) // QVR: flatscreen play's visor crosshair instead (vr/vr_flatxhair.cpp)
		return;

	GL_SetCanvas (CANVAS_CROSSHAIR);
	// QVR: the flatscreen ironsight's picture moved on the screen takes the crosshair with it (0 0 outside it).
	float shiftx, shifty;
	VR_FlatCrosshairShift (&shiftx, &shifty);
	const float canvas = vid.guiheight / CLAMP (1.0f, scr_crosshairscale.value, 10.0f);
	// QVR: and the ironsight fades it out over its own travel, back in on release (1 outside it).
	const float alpha = VR_FlatCrosshairAlpha ();
	if (alpha <= 0.f)
		return;
	GL_PushCanvasColor (1.f, 1.f, 1.f, alpha);
	Draw_Character(-4 + (int)(shiftx * canvas), -4 + (int)(shifty * canvas), crosshair_char);
	GL_PopCanvasColor ();
}

typedef struct
{
	float	scale[2];
	float	offset[2];
} canvasmap_t;

/*
==============
SCR_SetupProjToCanvasMap

Computes mapping from NDC coordinates (-1..1) to canvas coordinates
Note: canvas Y increases from top to bottom, GL Y is bottom to top
==============
*/
static void SCR_SetupProjToCanvasMap (canvasmap_t *remap)
{
	float	xmin, ymin, width, height;

	width = r_refdef.vrect.width / (float)glwidth * (glcanvas.right - glcanvas.left);
	height = r_refdef.vrect.height / (float)glheight * (glcanvas.top - glcanvas.bottom);
	xmin = glcanvas.left + (r_refdef.vrect.x / (float)glwidth) * (glcanvas.right - glcanvas.left);
	ymin = glcanvas.bottom - ((glheight - r_refdef.vrect.y - r_refdef.vrect.height) / (float)glheight) * (glcanvas.bottom - glcanvas.top);

	remap->scale[0]  = width * 0.5f;
	remap->offset[0] = width * 0.5f + xmin;
	remap->scale[1]  = height * 0.5f;
	remap->offset[1] = height * 0.5f + ymin;
}

/*
==============
SCR_ProjToCanvas
==============
*/
static void SCR_ProjToCanvas (const vec3_t proj, const canvasmap_t *remap, float *outx, float *outy)
{
	*outx = proj[0] * remap->scale[0] + remap->offset[0];
	*outy = proj[1] * remap->scale[1] + remap->offset[1];
}

/*
==============
SCR_GetEntityBottom
==============
*/
static void SCR_GetEntityBottom (const edict_t *ed, vec3_t pos)
{
	pos[0] = ed->v.origin[0] + (ed->v.mins[0] + ed->v.maxs[0]) * 0.5f;
	pos[1] = ed->v.origin[1] + (ed->v.mins[1] + ed->v.maxs[1]) * 0.5f;
	pos[2] = ed->v.origin[2] + ed->v.mins[2];

	// If it's a point entity, move anchor point down by 8 units to avoid overlapping debug visualization
	if (VectorCompare (ed->v.mins, ed->v.maxs))
		pos[2] -= 8.f;
}

/*
==============
SCR_GetEntityCenter
==============
*/
static void SCR_GetEntityCenter (const edict_t *ed, vec3_t pos)
{
	if (!VectorCompare (ed->v.mins, ed->v.maxs))
	{
		pos[0] = ed->v.origin[0] + (ed->v.mins[0] + ed->v.maxs[0]) * 0.5f;
		pos[1] = ed->v.origin[1] + (ed->v.mins[1] + ed->v.maxs[1]) * 0.5f;
		pos[2] = ed->v.origin[2] + (ed->v.mins[2] + ed->v.maxs[2]) * 0.5f;
	}
	else
	{
		VectorCopy (ed->v.origin, pos);
	}
}

/*
==============
SCR_ClipToFrustum

Clips the position to the view frustum towards a reference point.
==============
*/
static void SCR_ClipToFrustum (vec3_t pos, const vec3_t ref)
{
	int i;

	for (i = 0; i < 4; i++)
	{
		mplane_t *plane = &frustum[i];
		float dist = DotProduct (plane->normal, pos) - plane->dist;
		if (dist < 0.f)
		{
			float ref_dist = DotProduct (plane->normal, ref) - plane->dist;
			if (ref_dist != dist)
				VectorLerp (pos, ref, -dist / (ref_dist - dist), pos);
		}
	}
}

/*
==============
SCR_DrawKeyValueOverlay
==============
*/
static void SCR_DrawKeyValueOverlay (float x, float y, const char *multistr, const vec3_t bgcolor)
{
	int			ofs, numlines, keylen, vallen, maxkey, maxval;
	const char	*key, *val;

	// Compute column widths and total number of lines
	for (ofs = numlines = maxkey = maxval = 0; ofs < (int) VEC_SIZE (multistr); numlines++)
	{
		key = multistr + ofs;
		keylen = (int) strlen (key);
		ofs += keylen + 1;

		val = multistr + ofs;
		vallen = (int) strlen (val);
		ofs += vallen + 1;

		if (keylen == 0)
		{
			// empty key, value is printed centered
			keylen = vallen * CHARSIZE / 2;
			maxkey = q_max (maxkey, keylen);
			maxval = q_max (maxval, keylen);
		}
		else
		{
			// key/value pair, we leave half a character on each side for spacing
			maxkey = q_max (maxkey, keylen * CHARSIZE + CHARSIZE / 2);
			maxval = q_max (maxval, vallen * CHARSIZE + CHARSIZE / 2);
		}
	}

	// Try to keep as much text as possible on screen
	if (y > glcanvas.bottom - numlines * CHARSIZE)
		y = glcanvas.bottom - numlines * CHARSIZE;
	if (y < glcanvas.top)
		y = glcanvas.top;
	if (x > glcanvas.right - maxval)
		x = glcanvas.right - maxval;
	if (x < glcanvas.left + maxkey)
		x = glcanvas.left + maxkey;

	// Draw background box
	Draw_FillEx (x - maxkey, y, maxkey + maxval, numlines * CHARSIZE, bgcolor, 0.75f);

	// Print each line
	for (ofs = 0; ofs < (int) VEC_SIZE (multistr); y += CHARSIZE)
	{
		key = multistr + ofs;
		keylen = (int) strlen (key);
		ofs += keylen + 1;

		val = multistr + ofs;
		vallen = (int) strlen (val);
		ofs += vallen + 1;

		if (keylen == 0)
		{
			// empty key, center-print the value
			Draw_StringEx (x - vallen * CHARSIZE / 2, y, CHARSIZE, val);
		}
		else
		{
			// key/value pair, leave half a character on each side for spacing
			Draw_StringEx (x - CHARSIZE / 2 - keylen * CHARSIZE, y, CHARSIZE, key);
			Draw_StringEx (x + CHARSIZE / 2, y, CHARSIZE, val);
		}
	}
}

/*
==============
SCR_DrawEdictInfo

Show info for the highlighted entity with r_showfields/r_showbboxes
==============
*/

static char *scr_edictoverlaystrings = NULL;

static void SCR_DrawEdictInfo (void)
{
	char		tinted[1024];
	int			i;
	float		x, y;
	canvasmap_t	proj2canvas;
	vec3_t		crosshair, focus, anchor, proj, bgcolor;
	edict_t		*ed;

	if (VEC_SIZE (bbox_linked) == 0 && VEC_SIZE (r_pointfile) == 0)
		return;

	GL_SetCanvas (CANVAS_INFO);
	SCR_SetupProjToCanvasMap (&proj2canvas);
	VectorMA (r_origin, 8.f, vpn, crosshair);

	// If a pointfile was loaded, print "Leak" at the beginning
	if (VEC_SIZE (r_pointfile) != 0)
	{
		VectorCopy (r_pointfile[0], anchor);
		SCR_ClipToFrustum (anchor, crosshair);
		ProjectVector (anchor, r_matviewproj, proj);
		SCR_ProjToCanvas (proj, &proj2canvas, &x, &y);

		VEC_CLEAR (scr_edictoverlaystrings);
		MultiString_Append (&scr_edictoverlaystrings, "");
		COM_TintString ("Leak", tinted, sizeof (tinted));
		MultiString_Append (&scr_edictoverlaystrings, tinted);

		SCR_DrawKeyValueOverlay (x, y, scr_edictoverlaystrings, rgb_black);
	}

	if (VEC_SIZE (bbox_linked) == 0)
		return;

	PR_SwitchQCVM (&sv.qcvm);

	SCR_GetEntityCenter (bbox_linked[0], focus);
	SCR_ClipToFrustum (focus, crosshair);

	// Show edict numbers and classnames for all highlighted entities.
	// Note: reversed order, so that the focused entity is drawn last,
	// on top of all the others.
	for (i = (int) VEC_SIZE (bbox_linked) - 1; i >= 0; i--)
	{
		ed = bbox_linked[i];

		// Compute anchor point
		if (i == 0)
		{
			// With r_showfields_align 0 the field overlay tracks the focused entity,
			// so we disable the simple one (number + classname) to avoid overlap.
			if (!r_showfields_align.value)
				continue;
			//SCR_GetEntityBottom (ed, anchor);
			SCR_GetEntityCenter (ed, anchor);
			SCR_ClipToFrustum (anchor, crosshair);
		}
		else
		{
			SCR_GetEntityCenter (ed, anchor);
			SCR_ClipToFrustum (anchor, focus);
		}
		ProjectVector (anchor, r_matviewproj, proj);
		SCR_ProjToCanvas (proj, &proj2canvas, &x, &y);

		// Simple overlay: centered edict number and classname (if not empty)
		VEC_CLEAR (scr_edictoverlaystrings);
		MultiString_Append (&scr_edictoverlaystrings, "");
		MultiString_Append (&scr_edictoverlaystrings, va ("edict %d", NUM_FOR_EDICT (ed)));
		if (ed->v.classname)
		{
			MultiString_Append (&scr_edictoverlaystrings, "");
			MultiString_Append (&scr_edictoverlaystrings, PR_GetString (ed->v.classname));
		}

		// Set background color based on link type
		switch (ed->showbboxflags)
		{
		default:
		case SHOWBBOX_LINK_NONE:		VectorSet (bgcolor, 0.f,	0.f,	0.f); break;
		case SHOWBBOX_LINK_INCOMING:	VectorSet (bgcolor, 0.25f,	0.125f,	0.125f); break;
		case SHOWBBOX_LINK_OUTGOING:	VectorSet (bgcolor, 0.125f,	0.125f,	0.25f); break;
		case SHOWBBOX_LINK_BOTH:		VectorSet (bgcolor, 0.25f,	0.125f,	0.25f); break;
		}

		SCR_DrawKeyValueOverlay (x, y, scr_edictoverlaystrings, bgcolor);
	}

	// Show all relevant entity fields for the highlighted entity (first in bbox_linked)
	if (r_showfields.value)
	{
		ed = bbox_linked[0];

		// Compute anchor point
		SCR_GetEntityBottom (ed, anchor);
		ProjectVector (anchor, r_matviewproj, proj);
		SCR_ProjToCanvas (proj, &proj2canvas, &x, &y);

		// r_showfields_align 1 locks the overlay to the bottom-right
		if (r_showfields_align.value)
		{
			x = glcanvas.right;
			y = glcanvas.bottom;
		}

		// Build overlay text: first 2 lines are entity number and classname
		VEC_CLEAR (scr_edictoverlaystrings);
		MultiString_Append (&scr_edictoverlaystrings, "Edict");
		MultiString_Append (&scr_edictoverlaystrings, va ("%d", NUM_FOR_EDICT (ed)));
		COM_TintString ("classname", tinted, sizeof (tinted));
		MultiString_Append (&scr_edictoverlaystrings, tinted);
		MultiString_Append (&scr_edictoverlaystrings, PR_GetString (ed->v.classname));

		// Add all relevant fields, excluding classname (already added to the header)
		for (i = 1; i < qcvm->progs->numfielddefs; i++)
		{
			stringview_t	line;
			const char		*val;
			ddef_t			*d = &qcvm->fielddefs[i];

			if (d->ofs*4 == offsetof (entvars_t, classname) || !ED_IsRelevantField (ed, d))
				continue;

			COM_TintString (PR_GetString (d->s_name), tinted, sizeof (tinted));
			val = ED_FieldValueString (ed, d);

			while (COM_ParseLine (&val, &line))
			{
				MultiString_Append (&scr_edictoverlaystrings, tinted);
				MultiString_AppendN (&scr_edictoverlaystrings, line.data, line.len);
				tinted[0] = ' ';
				tinted[1] = '\0';
			}
		}

		SCR_DrawKeyValueOverlay (x, y, scr_edictoverlaystrings, rgb_black);
	}

	PR_SwitchQCVM (NULL);
}


//=============================================================================


/*
==================
SCR_SetUpToDrawConsole
==================
*/
static void SCR_SetUpToDrawConsole (void)
{
	//johnfitz -- let's hack away the problem of slow console when host_timescale is <0
	extern cvar_t host_timescale;
	float timescale, conspeed;
	//johnfitz

	Con_CheckResize ();

	if (scr_drawloading)
		return;		// never a console with loading plaque

// decide on the height of the console
	con_forcedup = !cl.worldmodel || cls.signon != SIGNONS;

	if (con_forcedup)
	{
		scr_conlines = glheight; //full screen //johnfitz -- glheight instead of vid.height
		scr_con_current = scr_conlines;
	}
	else if (key_dest == key_console || M_WantsConsole (NULL))
		scr_conlines = glheight/2; //half screen //johnfitz -- glheight instead of vid.height
	else
		scr_conlines = 0; //none visible

	timescale = (host_timescale.value > 0) ? host_timescale.value : 1; //johnfitz -- timescale
	conspeed = (scr_conspeed.value > 0 && !cls.timedemo) ? scr_conspeed.value : 1e6f;

	// make sure the console isn't too slow to come down if we're forcing it open to preview a specific option
	if (key_dest == key_menu)
		conspeed = q_max (conspeed, 2000.f);

	if (scr_conlines < scr_con_current)
	{
		// ericw -- (glheight/600.0) factor makes conspeed resolution independent, using 800x600 as a baseline
		scr_con_current -= conspeed*(glheight/600.0)*host_frametime/timescale; //johnfitz -- timescale
		if (scr_conlines > scr_con_current)
			scr_con_current = scr_conlines;
	}
	else if (scr_conlines > scr_con_current)
	{
		// ericw -- (glheight/600.0)
		scr_con_current += conspeed*(glheight/600.0)*host_frametime/timescale; //johnfitz -- timescale
		if (scr_conlines < scr_con_current)
			scr_con_current = scr_conlines;
	}

	if (clearconsole++ < vid.numpages)
		Sbar_Changed ();

	if (!con_forcedup && scr_con_current)
		scr_tileclear_updates = 0; //johnfitz
}

/*
==================
SCR_DrawConsole
==================
*/
static void SCR_DrawConsole (void)
{
	if (scr_con_current)
	{
		if (key_dest != key_menu || (!con_forcedup || M_WantsConsole (NULL)))
			Con_DrawConsole (scr_con_current, true, key_dest == key_console);
		else
			Draw_ConsoleBackground ();
		clearconsole = 0;
	}
	else
	{
		if (key_dest == key_game || key_dest == key_message)
			Con_DrawNotify ();	// only draw notify in game
	}
}


/*
==============================================================================

SCREEN SHOTS

==============================================================================
*/

/*
==================
SCR_GetCleanMapTitle
==================
*/
static void SCR_GetCleanMapTitle (char *buf, size_t maxchars)
{
	char clean[countof (cl.levelname)];
	size_t i, j;

	for (i = j = 0; i + 1 < countof (cl.levelname) && cl.levelname[i]; i++)
	{
		char c = cl.levelname[i] & 0x7f;
		switch (c)
		{
		case '/':
		case '|':
		case ':':
		case '\n':
			c = '-';
			break;
		case '*':
			c = '=';
			break;
		case '<':
			c = '[';
			break;
		case '>':
			c = ']';
			break;
		case '?':
		case '!':
			c = '.';
			break;
		case '"':
			c = '\'';
			break;
		case '\t':
			c = ' ';
			break;
		case '\\':
			if (i + 1 < countof (cl.levelname) && cl.levelname[i] == 'n')
				i++;
			c = '-';
			break;
		default:
			break;
		}
		// remove leading spaces, replace consecutive spaces with a single one
		if (c != ' ' || (j > 0 && clean[j - 1] != c))
			clean[j++] = c;
	}
	clean[j++] = '\0';

	UTF8_FromQuake (buf, maxchars, clean);
}

/*
==================
SCR_ExpandVariables

Returns true if format contains any variables
==================
*/
static qboolean SCR_ExpandVariables (const char *fmt, char *dst, size_t maxchars)
{
	time_t		now;
	struct tm	*lt;
	char		var[256];
	size_t		i, j, k, numvars;

	if (!maxchars)
		return false;
	--maxchars;

	time (&now);
	lt = localtime (&now);

	for (i = j = numvars = 0; j < maxchars && fmt[i]; /**/)
	{
		if (fmt[i] != '%')
		{
			dst[j++] = fmt[i];
			i++;
			continue;
		}

		i++;
		if (fmt[i] == '%')
		{
			dst[j++] = '%';
			i++;
			continue;
		}

		// find closing %
		for (k = 0; fmt[i + k]; k++)
			if (fmt[i + k] == '%')
				break;

		#define IS_VAR(s)	(k == strlen (s) && q_strncasecmp (fmt + i, s, k) == 0)

		if (IS_VAR ("map"))
		{
			if (cls.state == ca_connected && cls.signon == SIGNONS)
				q_strlcpy (var, cl.mapname, sizeof (var));
			else if (key_dest == key_menu)
				q_strlcpy (var, "menu", sizeof (var));
			else
				q_strlcpy (var, "console", sizeof (var));
		}
		else if (IS_VAR ("maptitle"))
		{
			if (cls.state == ca_connected && cls.signon == SIGNONS)
				SCR_GetCleanMapTitle (var, sizeof (var));
			else if (key_dest == key_menu)
				q_strlcpy (var, "menu", sizeof (var));
			else
				q_strlcpy (var, "console", sizeof (var));
		}
		else if (IS_VAR ("date"))
			q_snprintf (var, sizeof (var), "%04d-%02d-%02d", lt->tm_year + 1900, lt->tm_mon + 1, lt->tm_mday);
		else if (IS_VAR ("time"))
			q_snprintf (var, sizeof (var), "%02d-%02d-%02d", lt->tm_hour, lt->tm_min, lt->tm_sec);
		else if (IS_VAR ("year"))
			q_snprintf (var, sizeof (var), "%04d", lt->tm_year + 1900);
		else if (IS_VAR ("month"))
			q_snprintf (var, sizeof (var), "%02d", lt->tm_mon + 1);
		else if (IS_VAR ("day"))
			q_snprintf (var, sizeof (var), "%02d", lt->tm_mday);
		else if (IS_VAR ("hour"))
			q_snprintf (var, sizeof (var), "%02d", lt->tm_hour);
		else if (IS_VAR ("min"))
			q_snprintf (var, sizeof (var), "%02d", lt->tm_min);
		else if (IS_VAR ("sec"))
			q_snprintf (var, sizeof (var), "%02d", lt->tm_sec);
		else // unknown variable, write name and percentage signs
			q_snprintf (var, sizeof (var), "%%%.*s%%", (int)k, fmt + i);

		#undef IS_VAR

		// advance format cursor
		i += k;
		if (fmt[i]) // skip closing %
			i++;

		// append variable value
		k = strlen (var);
		if (k > maxchars - j)
			k = maxchars - j;
		memcpy (dst + j, var, k);
		j += k;
		numvars++;
	}

	dst[j++] = '\0';

	return numvars != 0;
}

static void SCR_ScreenShot_Usage (void)
{
	Con_Printf ("usage: screenshot <format> <quality>\n");
	Con_Printf ("   format must be \"png\" or \"tga\" or \"jpg\"\n");
	Con_Printf ("   quality must be 1-100\n");
}

/*
==================
SCR_ScreenShot_f
==================
*/
static void SCR_ScreenShot_f (void)
{
	byte	*buffer;
	char	ext[4];
	int		i, quality;

	Q_strncpy (ext, "png", sizeof(ext));

	if (Cmd_Argc () >= 2)
	{
		const char	*requested_ext = Cmd_Argv (1);

		if (!q_strcasecmp ("png", requested_ext)
		    || !q_strcasecmp ("tga", requested_ext)
		    || !q_strcasecmp ("jpg", requested_ext))
			Q_strncpy (ext, requested_ext, sizeof(ext));
		else
		{
			SCR_ScreenShot_Usage ();
			return;
		}
	}

// read quality as the 3rd param (only used for JPG)
	quality = 90;
	if (Cmd_Argc () >= 3)
		quality = Q_atoi (Cmd_Argv(2));
	if (quality < 1 || quality > 100)
	{
		SCR_ScreenShot_Usage ();
		return;
	}

//get data
	if (!(buffer = (byte *) VR_HeapMalloc(glwidth*glheight*3)))
	{
		Con_Printf ("SCR_ScreenShot_f: Couldn't allocate memory\n");
		return;
	}

	if (scr_viewsize.value >= 130)
	{
		qboolean oldskip = scr_skipupdate;
		Con_ClearNotify ();
		SCR_ClearCenterString ();
		scr_skipupdate = 1; // don't swap buffers at end of frame
		SCR_UpdateScreen ();
		scr_skipupdate = oldskip;
	}

	glPixelStorei (GL_PACK_ALIGNMENT, 1);/* for widths that aren't a multiple of 4 */
	glReadPixels (glx, gly, glwidth, glheight, GL_RGB, GL_UNSIGNED_BYTE, buffer);

// now write the file
	if (!Steam_SaveScreenshot (buffer, glwidth, glheight))
	{
		char		basename[MAX_OSPATH];
		char		imagename[MAX_OSPATH];
		char		checkname[MAX_OSPATH];
		qboolean	ok, has_vars;

	// find a file name to save it to
		has_vars = SCR_ExpandVariables (cl_screenshotname.string, basename, sizeof (basename));
		if (!basename[0])
			q_strlcpy (basename, SCREENSHOT_PREFIX, sizeof (basename));

		if (!has_vars)
			goto append_index;

		q_snprintf (imagename, sizeof (imagename), "%s.%s", basename, ext);
		q_snprintf (checkname, sizeof (checkname), "%s/%s", com_gamedir, imagename);
		if (Sys_FileType (checkname) != FS_ENT_NONE || VR_ScreenshotPending (imagename)) // base name already used, try appending an index (QVR: or being written)
		{
		append_index:
			// append underscore if basename ends with a digit
			i = (int) strlen (basename);
			if (i && i + 1 < (int) countof (basename) && (unsigned int)(basename[i - 1] - '0') < 10u)
			{
				basename[i] = '_';
				basename[i + 1] = '\0';
			}

			for (i = has_vars; i < 10000; i++)
			{
				q_snprintf (imagename, sizeof (imagename), "%s%04i.%s", basename, i, ext);
				q_snprintf (checkname, sizeof (checkname), "%s/%s", com_gamedir, imagename);
				if (Sys_FileType (checkname) == FS_ENT_NONE && !VR_ScreenshotPending (imagename)) // QVR: nor being written
					break;	// file doesn't exist
			}
			if (i == 10000)
			{
				Con_Printf ("SCR_ScreenShot_f: Couldn't find an unused filename\n");
				VR_HeapFree (buffer);
				return;
			}
		}

		// QVR: a PNG is encoded and saved on the game's thread pool (most of a screenshot's time: a hitch in the
		// headset, a voice note's): the job owns the buffer; it is saved under another name, renamed when complete
		if (!q_strncasecmp (ext, "png", sizeof(ext)) && VR_ScreenshotWrite (imagename, buffer, glwidth, glheight))
		{
			UTF8_ToQuake (basename, sizeof (basename), imagename);
			Con_SafePrintf ("Wrote ");
			Con_LinkPrintf (va("%s/%s", com_gamedir, imagename), "%s", basename);
			Con_SafePrintf ("\n");
			return;
		}
		if (!q_strncasecmp (ext, "png", sizeof(ext)))
			ok = Image_WritePNG (imagename, buffer, glwidth, glheight, 24, false);
		else if (!q_strncasecmp (ext, "tga", sizeof(ext)))
			ok = Image_WriteTGA (imagename, buffer, glwidth, glheight, 24, false);
		else if (!q_strncasecmp (ext, "jpg", sizeof(ext)))
			ok = Image_WriteJPG (imagename, buffer, glwidth, glheight, 24, quality, false);
		else
			ok = false;

		UTF8_ToQuake (basename, sizeof (basename), imagename);
		if (ok)
		{
			Con_SafePrintf ("Wrote ");
			Con_LinkPrintf (va("%s/%s", com_gamedir, imagename), "%s", basename);
			Con_SafePrintf ("\n");
		}
		else
			Con_Printf ("SCR_ScreenShot_f: Couldn't create %s\n", basename);
	}

	VR_HeapFree (buffer);
}


//=============================================================================


/*
===============
SCR_BeginLoadingPlaque

================
*/
void SCR_BeginLoadingPlaque (void)
{
	int i; // QVR

	S_StopAllSounds (true);

	if (cls.state != ca_connected)
		return;
	if (cls.signon != SIGNONS)
		return;

// redraw with no console and the loading plaque
	if (key_dest != key_console)
	{
		Con_ClearNotify ();
		// don't reset console when advancing to a new demo while previewing a console option
		if (!M_WantsConsole (NULL))
			scr_con_current = 0;
		scr_drawloading = true;
	}

	for (i = 0; i < 4; i++) // QVR: splitscreen: every seat's centre print
		SCR_SeatHudOf (i)->centertime_off = 0;
	Sbar_Changed ();
	SCR_UpdateScreen ();
	scr_drawloading = false;

	scr_disabled_for_loading = true;
	scr_disabled_time = realtime;
}

/*
===============
SCR_EndLoadingPlaque

================
*/
void SCR_EndLoadingPlaque (void)
{
	scr_disabled_for_loading = false;
	Con_ClearNotify ();
}

//=============================================================================

// seat-shared: the window's (its size, the loading plaque, the dialog, the pics, the HUD style cvar's)
const char	*scr_notifystring;
// seat-shared: the window's (its size, the loading plaque, the dialog, the pics, the HUD style cvar's)
qboolean	scr_drawdialog;

static void SCR_DrawNotifyString (void)
{
	const char	*start;
	int		l;
	int		j;
	int		x, y;

	GL_SetCanvas (CANVAS_MENU); //johnfitz

	start = scr_notifystring;

	y = 200 * 0.35; //johnfitz -- stretched overlays

	do
	{
	// scan the width of the line
		for (l=0 ; l<40 ; l++)
			if (start[l] == '\n' || !start[l])
				break;
		x = (320 - l*8)/2; //johnfitz -- stretched overlays
		for (j=0 ; j<l ; j++, x+=8)
			Draw_Character (x, y, start[j]);

		y += 8;

		while (*start && *start != '\n')
			start++;

		if (!*start)
			break;
		start++;		// skip the \n
	} while (1);
}

/*
==================
SCR_ModalMessage

Displays a text string in the center of the screen and waits for a Y or N
keypress.
==================
*/
int SCR_ModalMessage (const char *text, float timeout) //johnfitz -- timeout
{
	double time1, time2; //johnfitz -- timeout
	int lastkey, lastchar;
	qboolean vrframes = false; // QVR: a headset's frames drawn meanwhile

	if (cls.state == ca_dedicated)
		return true;

	scr_notifystring = text;

// draw a fresh screen
	scr_drawdialog = true;
	SCR_UpdateScreen ();
	scr_drawdialog = false;

	S_ClearBuffer ();		// so dma doesn't loop current sound

	time1 = Sys_DoubleTime () + timeout; //johnfitz -- timeout
	time2 = 0.0f; //johnfitz -- timeout

	Key_BeginInputGrab ();
	do
	{
		Sys_SendKeyEvents ();
		if (!VR_ModalMessageFrame ()) // QVR: a headset's frames go on, showing the dialog
			Sys_Sleep (16);
		else
			vrframes = true;
		Key_GetGrabbedInput (&lastkey, &lastchar);
		// QVR: Sys_Sleep (16) above, unless a headset's frame was drawn
		if (timeout) time2 = Sys_DoubleTime (); //johnfitz -- zero timeout means wait forever.
	} while (lastchar != 'y' && lastchar != 'Y' &&
		 lastchar != 'n' && lastchar != 'N' &&
		 lastkey != K_ESCAPE &&
		 lastkey != K_ABUTTON &&
		 lastkey != K_BBUTTON &&
		 lastkey != K_MOUSE2 &&
		 lastkey != K_MOUSE4 &&
		 time2 <= time1);
	Key_EndInputGrab ();
	if (vrframes) // QVR: the host frame goes on as a frame of its own, not the dialog's last (VR_ModalMessageFrame)
		host_framecount++;

//	SCR_UpdateScreen (); //johnfitz -- commented out

	//johnfitz -- timeout
	if (time2 > time1)
		return false;
	//johnfitz

	return (lastchar == 'y' || lastchar == 'Y' || lastkey == K_ABUTTON);
}


//=============================================================================

/*
==================
SCR_TileClear
johnfitz -- modified to use glwidth/glheight instead of vid.width/vid.height
	    also fixed the dimentions of right and top panels
	    also added scr_tileclear_updates
==================
*/
static void SCR_TileClear (void)
{
	//ericw -- added check for glsl gamma. TODO: remove this ugly optimization?
	if (scr_tileclear_updates >= vid.numpages && !gl_clear.value && vid_gamma.value == 1)
		return;
	scr_tileclear_updates++;

	if (r_refdef.vrect.x > 0)
	{
		// left
		Draw_TileClear (0,
						0,
						r_refdef.vrect.x,
						glheight - sb_lines);
		// right
		Draw_TileClear (r_refdef.vrect.x + r_refdef.vrect.width,
						0,
						glwidth - r_refdef.vrect.x - r_refdef.vrect.width,
						glheight - sb_lines);
	}

	if (r_refdef.vrect.y > 0)
	{
		// top
		Draw_TileClear (r_refdef.vrect.x,
						0,
						r_refdef.vrect.width,
						r_refdef.vrect.y);
		// bottom
		Draw_TileClear (r_refdef.vrect.x,
						r_refdef.vrect.y + r_refdef.vrect.height,
						r_refdef.vrect.width,
						glheight - r_refdef.vrect.y - r_refdef.vrect.height - sb_lines);
	}
}

/*
==================
SCR_DrawSbar

QVR: the HUD (Sbar_Draw). With VR it is never in the headset, whatever hudstyle is (the wrist
gadget is the HUD there): it goes to the window only, under the 2D layer (SCR_DrawWindowHud,
from VR_End2D). With vr_hud_mode 0 the canvas gets a classic status bar, for a hand.
==================
*/
static void SCR_DrawSbar (void)
{
	int where = VR_SbarInCanvas ();
	if (where == 2)
	{
		hudstyle_t style = hudstyle;
		hudstyle = HUD_CLASSIC;
		Sbar_Draw ();
		hudstyle = style;
	}
	else if (where == 1)
		Sbar_Draw ();
}

static void SCR_DrawWindowHud (void)
{
	GL_Set2D ();
	Sbar_Draw ();
	Draw_Flush ();
}

/*
==================
SCR_DrawSeats -- QVR

Splitscreen phase 7: each seat in play drawn in its tile, as that seat (its client world, view, hands, HUD): its view
(the flat render, the refdef laid out in the tile), then its HUD (the crosshair, the hit marks, its centre print, its
status bar, its intermission's). The shared 2D (console, menus, notices) is drawn after, over the whole window. A seat
joining (not signed on yet) shows its tile dark with a line.
==================
*/
static void SCR_DrawSeatHud (seathud_t *hud)
{
	if (cl.intermission == 1 && Key_SeatDest () == key_game) // (phase 9: the seat's own menu or the game)
		Sbar_IntermissionOverlay ();
	else if (cl.intermission == 2 && Key_SeatDest () == key_game)
	{
		Sbar_FinaleOverlay ();
		SCR_CheckDrawCenterString (hud);
	}
	else
	{
		VR_FlatHitDraw ();
		SCR_DrawCrosshair ();
		SCR_CheckDrawCenterString (hud);
		SCR_DrawSbar ();
		VR_FlatSeatPanels (); // the seat's toolgun panel and holster menu, in its tile (one seat: VR_End2D's)
	}
}

static void SCR_DrawSeats (void)
{
	int was = CL_SeatIndex (), s;
	for (s = 0; s < MAX_SPLITSEATS; s++)
	{
		vrect_t tile;
		scrwindow_t saved;
		seatview_t *sv;
		if (!CL_SeatPlays (s))
			continue;
		SCR_SeatRect (s, &tile);
		CL_SeatSet (s);
		sv = V_SeatView ();
		SCR_SeatPassBegin (&tile, &saved);
		SCR_CalcRefdef ();
		r_refdef.scale = CLAMP (1, (int)r_scale.value, vid.maxscale);
		if (cls.state == ca_connected && cls.signon == SIGNONS && cl.worldmodel)
		{
			if (s != was)
				V_UpdateBlend (); // (the caller's seat's is done)
			VR_VisorSeatPanels (); // its visor's panels (drawn over its view; rendered at its last HUD's)
			VR_WindowViewBegin ();
			VR_FlatRenderView ();
			VR_WindowViewEnd ();
			VectorCopy (r_refdef.vieworg, sv->drawnorg);
			VectorCopy (vright, sv->drawnright); // (its sound listener's)
			GL_Set2D ();
			SCR_DrawSeatHud (SCR_SeatHud ());
			VR_SeatMenuDraw (); // (phase 9: a seat 2-4's own menu in its tile, or the pause's note)
		}
		else
		{
			const char *line = va ("seat %d: joining", s + 1);
			GL_Set2D ();
			GL_SetCanvas (CANVAS_MENU);
			Draw_String (160 - 4 * (int) strlen (line), 96, line);
		}
		if (scr_seatmarkers.value)
			SCR_DrawSeatMarker (s);
		Draw_Flush ();
		if (scr_seatmarkers.value >= 2)
			SCR_SeatProbe (sv);
		sv->drawnrect = tile;
		sv->drawnframes++;
		SCR_SeatPassEnd (&saved);
	}
	CL_SeatSet (was);
}


/*
==================
SCR_UpdateScreen

This is called every frame, and can also be called explicitly to flush
text to the screen.

WARNING: be very careful calling this from elsewhere, because the refresh
needs almost the entire 256k of stack space!
==================
*/
void SCR_UpdateScreen (void)
{
	seathud_t *hud = SCR_SeatHud (); // QVR: splitscreen: the seat's HUD, passed to its draws
	qboolean split; // QVR: splitscreen: the seats in tiles (SCR_DrawSeats), each its view and HUD
	vid.numpages = (gl_triplebuffer.value) ? 3 : 2;

	if (scr_disabled_for_loading)
	{
		if (realtime - scr_disabled_time > 60)
		{
			scr_disabled_for_loading = false;
			Con_Printf ("load failed.\n");
		}
		else
			return;
	}

	if (!scr_initialized || !con_initialized)
		return;				// not initialized yet

	VR_LoadJobsJoin (); // QVR: the load's work on the job threads committed before anything is drawn (vr/vr_normalmaps.cpp)

	if (VR_SkipScreen ()) // QVR: a test run's unpaced frames, not drawn (vr_mock_fast 2): the view set up alone
	{
		// The console forced down as SCR_SetUpToDrawConsole decides it (not called here): else it keeps the last drawn
		// frame's value, and a run that drew no frame in a map (a take replayed first after start-up) set up no view.
		if (!scr_drawloading)
			con_forcedup = !cl.worldmodel || cls.signon != SIGNONS;
		VR_HeadlessView ();
		return;
	}


	GL_BeginRendering (&glx, &gly, &glwidth, &glheight);
	scr_window.width = glwidth; // QVR: splitscreen: the tiles' window (a seat's pass shrinks glwidth to its tile)
	scr_window.height = glheight;
	split = SCR_SeatsDrawn () > 1; // QVR

	//
	// determine size of refresh window
	//
	if (vid.recalc_refdef)
		SCR_CalcRefdef ();
	r_refdef.scale = CLAMP (1, (int)r_scale.value, vid.maxscale);

//
// do 3D refresh drawing, and then update the screen
//
	SCR_SetUpToDrawConsole ();

	V_UpdateBlend (); //johnfitz -- V_UpdatePalette cleaned up and renamed

	VR_ProfileBeginGPU ("3D"); // QVR: profile
	if (split) // QVR: splitscreen: each seat's view and HUD in its tile (flatscreen play)
		SCR_DrawSeats ();
	else if (!VR_RenderView ()) // QVR: renders both eyes and mirrors one
	{
		VR_WindowViewBegin (); // QVR: as the eyes' first (the shadow pass's set-ups shared)
		VR_FlatRenderView (); // QVR: V_RenderView, with flat-screen play's TAA (vr/vr_temporal.cpp)
		VR_WindowViewEnd (); // QVR
	}
	if (!split) // QVR: splitscreen: the one seat's drawn view, the whole window (SCR_DrawSeats records each tile's)
	{
		seatview_t *sv = V_SeatView ();
		SCR_SeatWindow (&sv->drawnrect);
		VectorCopy (r_refdef.vieworg, sv->drawnorg);
		sv->drawnframes++;
	}
	VR_ProfileEnd (); // QVR

	GL_BeginGroup ("2D");
	VR_ProfileBeginGPU ("2D"); // QVR: profile

	VR_Begin2D (); // QVR: to a canvas shown in the headset
	GL_Set2D ();

	//FIXME: only call this when needed
	if (!split) // QVR: (the tiles are the seats')
		SCR_TileClear ();

	if (scr_drawdialog) //new game confirm
	{
		if (con_forcedup)
			Draw_ConsoleBackground ();
		else
			SCR_DrawSbar (); // QVR
		Draw_FadeScreen (1.f);
		SCR_DrawNotifyString ();
	}
	else if (scr_drawloading || VR_LoadingPlaque ()) //loading (QVR: or a level change waiting for the headset's notice, no world drawn)
	{
		qboolean waited = !scr_drawloading; // QVR
		scr_drawloading = true; // QVR: (SCR_DrawLoading draws only then)
		SCR_DrawLoading ();
		if (waited) // QVR
			scr_drawloading = false;
		SCR_DrawSbar (); // QVR
		M_Draw ();
	}
	else if (!split && cl.intermission == 1 && key_dest == key_game) //end of level (QVR: a seat's in its tile with splitscreen)
	{
		Sbar_IntermissionOverlay ();
		SCR_DrawDemoControls ();
	}
	else if (!split && cl.intermission == 2 && key_dest == key_game) //end of episode
	{
		Sbar_FinaleOverlay ();
		SCR_CheckDrawCenterString (hud);
		SCR_DrawDemoControls ();
	}
	else
	{
		if (!split) // QVR: (splitscreen: each seat's in its tile, SCR_DrawSeatHud)
		{
			VR_FlatHitDraw (); // QVR: flatscreen play's marks of where hits came from (vr_flathit.cpp)
			SCR_DrawCrosshair (); //johnfitz
		}
		SCR_DrawNet ();
		SCR_DrawTurtle ();
		SCR_DrawPause ();
		if (!split) // QVR
		{
			SCR_CheckDrawCenterString (hud);
			SCR_DrawSbar (); // QVR
		}
		SCR_DrawDevStats (); //johnfitz
		SCR_DrawClock (); //johnfitz
		SCR_DrawDemoControls ();
		SCR_DrawSpeed ();
		SCR_DrawEdictInfo ();
		SCR_DrawConsole ();
		M_Draw ();
		SCR_DrawFPS (); //johnfitz
		SCR_DrawNetHud (); // QVR
		SCR_DrawSaving ();
		SCR_DrawRelight (); // QVR
		SCR_DrawTakes (); // QVR
		VR_HumStudioDraw2D (); // QVR: the Texture Studio's skin view while it paints (vr/vr_humstudio.cpp)
	}

	Draw_ResetClipping (); // QVR: (and flushed) a clip rectangle left on (a mod's QC without drawresetcliparea) would cut the canvas's draw into the window and the next frame's clears
	VR_End2D (SCR_DrawWindowHud); // QVR
	VR_ProfileEnd (); // QVR

	GL_EndGroup ();

	if (VR_DebugGLState ()) // QVR: the texture bind cache and framebuffer bindings checked against GL, the clip rectangle off
	{
		GL_CheckBindCache ();
		GL_CheckBoundFramebuffers ("frame end");
		if (glIsEnabled (GL_SCISSOR_TEST))
			Con_Printf ("glstate: clip rectangle left on at the frame's end\n");
	}

	VR_ProfileBegin ("swap"); // QVR: profile
	GL_EndRendering ();
	VR_ProfileEnd (); // QVR
}

