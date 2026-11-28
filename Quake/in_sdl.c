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

gyro related code is based on
https://github.com/yquake2/yquake2/blob/master/src/client/input/sdl.c

*/

#include "quakedef.h"
#include "vr/vr_api.h"
#if defined(SDL_FRAMEWORK) || defined(NO_SDL_CONFIG)
#include <SDL2/SDL.h>
#else
#include "SDL.h"
#endif

extern cvar_t ui_mouse;
extern cvar_t language;

static qboolean windowhasfocus = true;	//just in case sdl fails to tell us... -- seat-shared: the window's
static textmode_t textmode = TEXTMODE_OFF;	// seat-shared: the window's text input (the console and menus': one, phase 9)
static keydevice_t lastactivetype = KD_NONE;	// seat-shared: the menus' button glyphs (one menu, phase 9)

static cvar_t in_debugkeys = {"in_debugkeys", "0", CVAR_NONE};

#ifdef __APPLE__
/* Mouse acceleration needs to be disabled on OS X */
#define MACOS_X_ACCELERATION_HACK
#endif

#ifdef MACOS_X_ACCELERATION_HACK
#include <IOKit/IOTypes.h>
#include <IOKit/hidsystem/IOHIDLib.h>
#include <IOKit/hidsystem/IOHIDParameter.h>
#include <IOKit/hidsystem/event_status_driver.h>
#endif

// the gamepad alt modifier button is pressed
// => K_x_BTN turns into K_x_BTN_ALT
// QVR: splitscreen: a seat's (seatkeys_t altmodifier)

// SDL2 Game Controller cvars
cvar_t	joy_deadzone_look = { "joy_deadzone_look", "0.175", CVAR_ARCHIVE };
cvar_t	joy_deadzone_move = { "joy_deadzone_move", "0.175", CVAR_ARCHIVE };
cvar_t	joy_outer_threshold_look = { "joy_outer_threshold_look", "0.02", CVAR_ARCHIVE };
cvar_t	joy_outer_threshold_move = { "joy_outer_threshold_move", "0.02", CVAR_ARCHIVE };
cvar_t	joy_deadzone_trigger = { "joy_deadzone_trigger", "0.2", CVAR_ARCHIVE };
cvar_t	joy_sensitivity_yaw = { "joy_sensitivity_yaw", "240", CVAR_ARCHIVE };
cvar_t	joy_sensitivity_pitch = { "joy_sensitivity_pitch", "130", CVAR_ARCHIVE };
cvar_t	joy_invert = { "joy_invert", "0", CVAR_ARCHIVE };
cvar_t	joy_exponent = { "joy_exponent", "2", CVAR_ARCHIVE };
cvar_t	joy_exponent_move = { "joy_exponent_move", "2", CVAR_ARCHIVE };
cvar_t	joy_swapmovelook = { "joy_swapmovelook", "0", CVAR_ARCHIVE };
cvar_t	joy_flick = { "joy_flick", "0", CVAR_ARCHIVE };
cvar_t	joy_flick_time = { "joy_flick_time", "0.125", CVAR_ARCHIVE };
cvar_t	joy_flick_recenter = { "joy_flick_recenter", "0.0", CVAR_ARCHIVE };
cvar_t	joy_flick_deadzone = { "joy_flick_deadzone", "0.9", CVAR_ARCHIVE };
cvar_t	joy_flick_noise_thresh = { "joy_flick_noise_thresh", "2.0", CVAR_ARCHIVE };
cvar_t	joy_flick_adjust_speed = { "joy_flick_adjust_speed", "30.0", CVAR_ARCHIVE };
cvar_t	joy_rumble = { "joy_rumble", "0.3", CVAR_ARCHIVE };
cvar_t	joy_device = { "joy_device", "0", CVAR_ARCHIVE };
cvar_t	joy_always_active = { "joy_always_active", "0", CVAR_ARCHIVE };

cvar_t gyro_enable = {"gyro_enable", "1", CVAR_ARCHIVE};
cvar_t gyro_mode = {"gyro_mode", "2", CVAR_ARCHIVE}; // 2 = GYRO_BUTTON_DISABLES (see gyromode_t)
cvar_t gyro_turning_axis = {"gyro_turning_axis", "0", CVAR_ARCHIVE};

cvar_t gyro_yawsensitivity = {"gyro_yawsensitivity", "2.5", CVAR_ARCHIVE};
cvar_t gyro_pitchsensitivity= {"gyro_pitchsensitivity", "2.5", CVAR_ARCHIVE};

cvar_t gyro_calibration_x = {"gyro_calibration_x", "0", CVAR_ARCHIVE};
cvar_t gyro_calibration_y = {"gyro_calibration_y", "0", CVAR_ARCHIVE};
cvar_t gyro_calibration_z = {"gyro_calibration_z", "0", CVAR_ARCHIVE};

cvar_t gyro_noise_thresh = {"gyro_noise_thresh", "1.5", CVAR_ARCHIVE};

typedef struct joyaxis_s
{
	float x;
	float y;
} joyaxis_t;

typedef struct joy_buttonstate_s
{
	qboolean buttondown[SDL_CONTROLLER_BUTTON_MAX];
} joybuttonstate_t;

typedef struct axisstate_s
{
	float axisvalue[SDL_CONTROLLER_AXIS_MAX]; // normalized to +-1
} joyaxisstate_t;

typedef struct joyflick_s
{
	float	yaw;
	float	pitch;
	float	yaw_delta;
	float	prev_lerp_frac;
	float	prev_angle;
	float	prev_scale;
} joyflick_t;

// QVR: splitscreen (docs/vr-port/SPLITSCREEN.md, phase 2): up to four game controllers open at once, each driving a
// seat (Ironwail opened one: the joy_active_* globals, now pad 0's fields). Pad 0 is the controller Ironwail would
// use (joy_device, IN_UseNextGamepad: chosen as before); pads 1-3 are the others plugged in. The keyboard and mouse
// are always seat 0's; controllers join as the seats after it (IN_PadSeat): with one seat in play pad 0 drives seat
// 0, exactly as before, and the other pads are open but drive nothing.
typedef struct joypad_s
{
	SDL_GameController	*controller;	// NULL: no controller here (a mock pad has none either)
	SDL_JoystickID		instanceid;
	int					device;			// its SDL device index, -1: none
	gamepadtype_t		type;
	char				name[256];		// (SDL_GameControllerName returns NULL once unplugged)
	qboolean			has_rumble;
	qboolean			gyro_present;
	joybuttonstate_t	buttonstate;
	joyaxisstate_t		axisstate;
	double				buttontimer[SDL_CONTROLLER_BUTTON_MAX];
	double				emulatedkeytimer[6];
	double				scrolltimer;	// the look stick scrolling the console
	float				gyro_yaw, gyro_pitch, gyro_raw_mag;
	float				gyro_center_frac, gyro_center_amount;
	joyflick_t			flick;
	int					seat;			// the seat it drove at its last poll (-1: none): let go of there when that changes
	qboolean			mock;			// vr_seat_test's pad: its buttons and sticks from IN_MockPad*, no controller
	joybuttonstate_t	mockbuttons;
	joyaxisstate_t		mockaxes;
	qboolean			unseated;		// split11: it left its seat: drives none, its Start or A joins (IN_PadSetUnseated)
	qboolean			joinheld;		// ... its Start or A held at the last look (a press joins once)
	float				cursorfrac[2];	// split11: the look stick's motion of a flat-play cursor not yet a whole count
} joypad_t;

#define MAX_JOYPADS 4
static joypad_t joy_pads[MAX_JOYPADS] = // seat-shared: the controllers open, each driving the seat IN_PadSeat names
{
	{NULL, -1, -1}, {NULL, -1, -1}, {NULL, -1, -1}, {NULL, -1, -1}
};
#define joy_primary (&joy_pads[0])
static void IN_PadKeepAsRead (joypad_t *pad); // QVR: splitscreen (below)

static qboolean	no_mouse = false; // seat-shared: the system's mouse (-nomouse)

static const int buttonremap[] =
{
	K_MOUSE1,	/* left button		*/
	K_MOUSE3,	/* middle button	*/
	K_MOUSE2,	/* right button		*/
	K_MOUSE4,	/* back button		*/
	K_MOUSE5	/* forward button	*/
};

// QVR: the mouse's motion since the last move is seat 0's (seatkeys_t mouse_dx, mouse_dy: was total_dx, total_dy);
// the gyro's readings, centring and the flick stick are each pad's (joypad_t).

// used for gyro calibration
#define GYRO_CALIBRATION_SAMPLES	300
static float gyro_accum[3] = {0.f, 0.f, 0.f};	// seat-shared: the calibration's (pad 0's: the Options menu's gamepad)
static unsigned int updates_countdown = 0;		// seat-shared: the calibration's

// split11: pad k left its seat in the local multiplayer game running now (IN_PadSetUnseated): it drives no seat
static qboolean IN_PadIsUnseated (int k)
{
	return joy_pads[k].unseated && sv.active && svs.maxclients > 1 && !cls.demoplayback;
}

static void IN_ResetFlickState (joypad_t *pad)
{
	memset (&pad->flick, 0, sizeof (pad->flick));
}

/*
================
IN_PadSeat -- QVR (splitscreen)

The seat pad k drives, -1 none. Seat 0 is the keyboard and mouse; a pad that joined seat 1-3 drives it, and the
lowest pad slot no seat has drives seat 0 (the keyboard player's own pad: with one seat, pad 0, as before).
================
*/
int IN_PadSeat (int k)
{
	int s, j;

	if (k < 0 || k >= MAX_JOYPADS || (!joy_pads[k].controller && !joy_pads[k].mock))
		return -1;
	// QVR: splitscreen (phase 6): a pad that joined seat 1-3 drives it (seat_join); the lowest pad slot no seat has
	// drives seat 0 with the keyboard and mouse (one seat: pad 0, as before)
	for (s = 1; s < CL_SeatCount (); s++)
		if (CL_SeatPlays (s) && CL_SeatPad (s) == k)
			return s;
	for (j = 0; j < MAX_JOYPADS; j++)
	{
		if (IN_PadIsUnseated (j)) // split11: a pad that left its seat is no spare of seat 0's (its Start or A joins)
			continue;
		for (s = 1; s < CL_SeatCount () && !(CL_SeatPlays (s) && CL_SeatPad (s) == j); s++)
			;
		if (s == CL_SeatCount ())
			break;
	}
	return k == j ? 0 : -1;
}

void IN_PadSetUnseated (int k, qboolean unseated)
{
	if (k >= 0 && k < MAX_JOYPADS)
	{
		joy_pads[k].unseated = unseated;
		joy_pads[k].joinheld = true; // (the press that left, or joined, is no join: a fresh press is)
	}
}

// the pad driving a seat (NULL: none)
static joypad_t *IN_SeatPad (int seat)
{
	int k;

	for (k = 0; k < MAX_JOYPADS; k++)
		if (IN_PadSeat (k) == seat)
			return &joy_pads[k];
	return NULL;
}

static int IN_PadIndex (const joypad_t *pad)
{
	return (int) (pad - joy_pads);
}

static int SDLCALL IN_FilterMouseEvents (const SDL_Event *event)
{
	switch (event->type)
	{
	case SDL_MOUSEMOTION:
	// case SDL_MOUSEBUTTONDOWN:
	// case SDL_MOUSEBUTTONUP:
		if (key_dest == key_menu)
		{
			VR_MenuPadHide (); // QVR: the gamepad's menu cursor hidden (padflat)
			M_Mousemove (event->motion.x, event->motion.y);
		}
		else if (key_dest == key_console)
			Con_Mousemove (event->motion.x, event->motion.y);
		return 0;
	}

	return 1;
}

static int SDLCALL IN_SDL2_FilterMouseEvents (void *userdata, SDL_Event *event)
{
	return IN_FilterMouseEvents (event);
}

void IN_ShowCursor (void)
{
	VID_SetMouseCursor (MOUSECURSOR_DEFAULT);

	if (!SDL_GetRelativeMouseMode ())
		return;
	if (SDL_SetRelativeMouseMode (SDL_FALSE) != 0)
		Con_Printf ("WARNING: could not disable relative mouse mode (%s).\n", SDL_GetError ());

	if (!ui_mouse.value)
	{
		SDL_Window *window = (SDL_Window *) VID_GetWindow ();
		int width, height;
		SDL_GetWindowSize (window, &width, &height);
		SDL_WarpMouseInWindow (window, width/2, height/2);
	}
}

void IN_HideCursor (void)
{
#ifdef __APPLE__
	{
		// Work around https://github.com/sezero/quakespasm/issues/48
		int width, height;
		SDL_GetWindowSize((SDL_Window*) VID_GetWindow(), &width, &height);
		SDL_WarpMouseInWindow((SDL_Window*) VID_GetWindow(), width / 2, height / 2);
	}
#endif
	if (SDL_SetRelativeMouseMode(SDL_TRUE) != 0)
		Con_Printf("WARNING: could not enable relative mouse mode (%s).\n", SDL_GetError());
}

static void IN_BeginIgnoringMouseEvents(void)
{
	SDL_EventFilter currentFilter = NULL;
	void *currentUserdata = NULL;
	SDL_GetEventFilter(&currentFilter, &currentUserdata);
	if (currentFilter != IN_SDL2_FilterMouseEvents)
		SDL_SetEventFilter(IN_SDL2_FilterMouseEvents, NULL);
}

static void IN_EndIgnoringMouseEvents(void)
{
	SDL_EventFilter currentFilter;
	void *currentUserdata;
	if (SDL_GetEventFilter(&currentFilter, &currentUserdata) == SDL_TRUE)
		SDL_SetEventFilter(NULL, NULL);
}

#ifdef MACOS_X_ACCELERATION_HACK
static cvar_t in_disablemacosxmouseaccel = {"in_disablemacosxmouseaccel", "1", CVAR_ARCHIVE};
static double originalMouseSpeed = -1.0;

static io_connect_t IN_GetIOHandle(void)
{
	io_connect_t iohandle = MACH_PORT_NULL;
	io_service_t iohidsystem = MACH_PORT_NULL;
	mach_port_t masterport;
	kern_return_t status;

	status = IOMasterPort(MACH_PORT_NULL, &masterport);
	if (status != KERN_SUCCESS)
		return 0;

	iohidsystem = IORegistryEntryFromPath(masterport, kIOServicePlane ":/IOResources/IOHIDSystem");
	if (!iohidsystem)
		return 0;

	status = IOServiceOpen(iohidsystem, mach_task_self(), kIOHIDParamConnectType, &iohandle);
	IOObjectRelease(iohidsystem);

	return iohandle;
}

static void IN_DisableOSXMouseAccel (void)
{
	io_connect_t mouseDev = IN_GetIOHandle();
	if (mouseDev != 0)
	{
		if (IOHIDGetAccelerationWithKey(mouseDev, CFSTR(kIOHIDMouseAccelerationType), &originalMouseSpeed) == kIOReturnSuccess)
		{
			if (IOHIDSetAccelerationWithKey(mouseDev, CFSTR(kIOHIDMouseAccelerationType), -1.0) != kIOReturnSuccess)
			{
				Cvar_Set("in_disablemacosxmouseaccel", "0");
				Con_Printf("WARNING: Could not disable mouse acceleration (failed at IOHIDSetAccelerationWithKey).\n");
			}
		}
		else
		{
			Cvar_Set("in_disablemacosxmouseaccel", "0");
			Con_Printf("WARNING: Could not disable mouse acceleration (failed at IOHIDGetAccelerationWithKey).\n");
		}
		IOServiceClose(mouseDev);
	}
	else
	{
		Cvar_Set("in_disablemacosxmouseaccel", "0");
		Con_Printf("WARNING: Could not disable mouse acceleration (failed at IO_GetIOHandle).\n");
	}
}

static void IN_ReenableOSXMouseAccel (void)
{
	io_connect_t mouseDev = IN_GetIOHandle();
	if (mouseDev != 0)
	{
		if (IOHIDSetAccelerationWithKey(mouseDev, CFSTR(kIOHIDMouseAccelerationType), originalMouseSpeed) != kIOReturnSuccess)
			Con_Printf("WARNING: Could not re-enable mouse acceleration (failed at IOHIDSetAccelerationWithKey).\n");
		IOServiceClose(mouseDev);
	}
	else
	{
		Con_Printf("WARNING: Could not re-enable mouse acceleration (failed at IO_GetIOHandle).\n");
	}
	originalMouseSpeed = -1;
}
#endif /* MACOS_X_ACCELERATION_HACK */


void IN_Activate (void)
{
	if (no_mouse)
		return;

#ifdef MACOS_X_ACCELERATION_HACK
	/* Save the status of mouse acceleration */
	if (originalMouseSpeed == -1 && in_disablemacosxmouseaccel.value)
		IN_DisableOSXMouseAccel();
#endif

	IN_HideCursor();
	IN_EndIgnoringMouseEvents();

	Key_SeatOf (0)->mouse_dx = 0; // QVR: the mouse is seat 0's
	Key_SeatOf (0)->mouse_dy = 0;
}

void IN_Deactivate (qboolean free_cursor)
{
	if (no_mouse)
		return;

#ifdef MACOS_X_ACCELERATION_HACK
	if (originalMouseSpeed != -1)
		IN_ReenableOSXMouseAccel();
#endif

	if (free_cursor)
		IN_ShowCursor();
	else
		IN_HideCursor();

	/* discard all mouse events when input is deactivated */
	IN_BeginIgnoringMouseEvents();
}

void IN_DeactivateForConsole (void)
{
	IN_Deactivate(true);
}

void IN_DeactivateForMenu (void)
{
	IN_Deactivate(modestate == MS_WINDOWED || ui_mouse.value);
}

// QVR: a pad closed: the slot free again (its seat's keys are not let go here, as Ironwail's pad was not)
static void IN_ClosePad (joypad_t *pad)
{
	if (pad->controller)
	{
#if SDL_VERSION_ATLEAST (2, 0, 9)
		if (pad->has_rumble)
			SDL_GameControllerRumble (pad->controller, 0, 0, 100);
#if SDL_VERSION_ATLEAST(2, 0, 14)
		if (SDL_GameControllerHasLED (pad->controller))
			SDL_GameControllerSetLED (pad->controller, 0, 0, 0);
#endif // SDL_VERSION_ATLEAST (2, 0, 14)
#endif // SDL_VERSION_ATLEAST (2, 0, 9)
		SDL_GameControllerClose (pad->controller);
	}
	memset (pad, 0, sizeof (*pad));
	pad->instanceid = -1;
	pad->device = -1;
	pad->type = GAMEPAD_NONE;
}

// QVR: the pad holding a controller instance (NULL: none)
static joypad_t *IN_PadForInstance (SDL_JoystickID id)
{
	int k;

	for (k = 0; k < MAX_JOYPADS; k++)
		if (joy_pads[k].controller && joy_pads[k].instanceid == id)
			return &joy_pads[k];
	return NULL;
}

// QVR: a controller opened into a pad (Ironwail's IN_UseController's opening, for any pad)
static qboolean IN_OpenPad (joypad_t *pad, int device_index)
{
	SDL_GameController *gamecontroller;
	const char *controllername;

	gamecontroller = SDL_GameControllerOpen (device_index);
	if (!gamecontroller)
	{
		Con_Warning ("couldn't open gamepad device %d\n", device_index);
		return false;
	}

	controllername = SDL_GameControllerName (gamecontroller);
	if (!controllername)
		controllername = "[Unknown gamepad]";
	if (pad == joy_primary)
		Con_Printf ("Using gamepad: %s\n", controllername);
	else
		Con_Printf ("Gamepad %d (splitscreen): %s\n", IN_PadIndex (pad) + 1, controllername);

	IN_ClosePad (pad);
	pad->controller = gamecontroller;
	pad->instanceid = SDL_JoystickInstanceID (SDL_GameControllerGetJoystick (gamecontroller));
	pad->device = device_index;
	// Save controller name so we can print it when unplugged (SDL_GameControllerName would return NULL)
	q_strlcpy (pad->name, controllername, sizeof (pad->name));

	// Save controller family so we can show more appropriate button names
#if SDL_VERSION_ATLEAST(2, 0, 12)
	switch (SDL_GameControllerGetType (pad->controller))
	{
	default:
	case SDL_CONTROLLER_TYPE_XBOX360:
	case SDL_CONTROLLER_TYPE_XBOXONE:
		pad->type = GAMEPAD_XBOX;
		break;

	case SDL_CONTROLLER_TYPE_PS3:
	case SDL_CONTROLLER_TYPE_PS4:
	case SDL_CONTROLLER_TYPE_PS5:
		pad->type = GAMEPAD_PLAYSTATION;
		break;

	case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_PRO:
#if SDL_VERSION_ATLEAST(2, 24, 0)
	case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_LEFT:
	case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT:
	case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_PAIR:
#endif // SDL_VERSION_ATLEAST(2, 24, 0)
		pad->type = GAMEPAD_NINTENDO;
		break;
	}
#else
	// No controller type info (old SDL headers), assume Xbox-compatible
	pad->type = GAMEPAD_XBOX;
#endif // SDL_VERSION_ATLEAST(2, 0, 12)

#if SDL_VERSION_ATLEAST(2, 0, 14)
	if (SDL_GameControllerHasLED (pad->controller))
	{
		// orange LED, seemed fitting for Quake
		SDL_GameControllerSetLED (pad->controller, 80, 20, 0);
	}
	if (SDL_GameControllerHasSensor (pad->controller, SDL_SENSOR_GYRO)
		&& !SDL_GameControllerSetSensorEnabled (pad->controller, SDL_SENSOR_GYRO, SDL_TRUE))
	{
		pad->gyro_present = true;
#if SDL_VERSION_ATLEAST(2, 0, 16)
		Con_Printf ("Gyro sensor enabled at %g Hz\n", SDL_GameControllerGetSensorDataRate (pad->controller, SDL_SENSOR_GYRO));
#else
		Con_printf ("Gyro sensor enabled.\n")
#endif // SDL_VERSION_ATLEAST(2, 0, 16)
	}
	else
	{
		Con_Printf ("Gyro sensor not found\n");
	}
#endif // SDL_VERSION_ATLEAST(2, 0, 14)

#if SDL_VERSION_ATLEAST (2, 0, 9)
	pad->has_rumble = SDL_GameControllerHasRumble (pad->controller);
#endif // SDL_VERSION_ATLEAST (2, 0, 9)
	return true;
}

// QVR: pads 1-3: every other game controller plugged in, opened into a free pad; the ones unplugged closed (their
// device indices brought up to date). Pad 0 (IN_UseController) is chosen first, as before.
static void IN_RefreshExtraPads (void)
{
	int i, k, count;
	joypad_t *pad;

	if (!SDL_WasInit (SDL_INIT_GAMECONTROLLER))
		return;
	count = SDL_NumJoysticks ();
	for (k = 1; k < MAX_JOYPADS; k++)
	{
		pad = &joy_pads[k];
		if (!pad->controller)
			continue;
		pad->device = -1;
		for (i = 0; i < count; i++)
			if (SDL_JoystickGetDeviceInstanceID (i) == pad->instanceid)
				pad->device = i;
		if (pad->device == -1)
		{
			Con_Printf ("Gamepad removed: %s\n", pad->name);
			IN_ClosePad (pad);
		}
	}
	for (i = 0; i < count; i++)
	{
		if (!SDL_IsGameController (i) || IN_PadForInstance (SDL_JoystickGetDeviceInstanceID (i)))
			continue;
		for (k = 1; k < MAX_JOYPADS && joy_pads[k].controller; k++)
			;
		if (k == MAX_JOYPADS)
			break;
		IN_OpenPad (&joy_pads[k], i);
	}
}

static qboolean IN_UseController (int device_index)
{
	joypad_t *other;

	if (device_index == joy_primary->device)
		return true;

	if (joy_primary->device != -1)
	{
		// Only show "gamepad removed" message when disabling the gamepad altogether,
		// not when switching to a different device
		if (device_index == -1)
			Con_Printf ("Gamepad removed: %s\n", joy_primary->name);

		IN_ClosePad (joy_primary);
		Cvar_SetValueQuick (&joy_device, -1);
	}

	if (device_index == -1)
		return true;

	if (device_index < 0 || device_index >= SDL_NumJoysticks ())
		return false;

	// QVR: open already as another pad: that pad gives it up (IN_RefreshExtraPads opens the one pad 0 had in its place)
	other = IN_PadForInstance (SDL_JoystickGetDeviceInstanceID (device_index));
	if (other && other != joy_primary)
		IN_ClosePad (other);

	if (!IN_OpenPad (joy_primary, device_index))
		return false;
	Cvar_SetValueQuick (&joy_device, device_index);
	return true;
}

static void IN_SetupJoystick (void)
{
	int	count = SDL_NumJoysticks ();
	int	device_index = CLAMP (-1, (int)joy_device.value, count - 1);
	IN_UseController (device_index);
	IN_RefreshExtraPads (); // QVR: splitscreen: the other controllers
}

static qboolean IN_RemapJoystick (void)
{
	int i, count;

	if (joy_primary->instanceid == -1)
		return false;

	for (i = 0, count = SDL_NumJoysticks (); i < count; i++)
	{
		if (SDL_JoystickGetDeviceInstanceID (i) == joy_primary->instanceid)
		{
			joy_primary->device = i;
			Cvar_SetValueQuick (&joy_device, i);
			return true;
		}
	}

	return false;
}

void IN_StartupJoystick (void)
{
    int i;
    int nummappings;
    char controllerdb[MAX_OSPATH];

    if (COM_CheckParm("-nojoy"))
        return;

#if SDL_VERSION_ATLEAST(2, 0, 12)
	SDL_SetHint (SDL_HINT_JOYSTICK_HIDAPI_GAMECUBE, "1");
#endif
#if SDL_VERSION_ATLEAST(2, 0, 14)
	SDL_SetHint (SDL_HINT_JOYSTICK_HIDAPI_PS5, "1");
#endif
#if SDL_VERSION_ATLEAST(2, 0, 22)
	SDL_SetHint (SDL_HINT_JOYSTICK_HIDAPI, "1");
	SDL_SetHint (SDL_HINT_JOYSTICK_HIDAPI_PS4, "1");
	SDL_SetHint (SDL_HINT_JOYSTICK_HIDAPI_SWITCH, "1");
	SDL_SetHint (SDL_HINT_JOYSTICK_HIDAPI_JOY_CONS, "1");

	// Enable rumble and motion sensors for PS4 and PS5 controllers while uder Bluetooth connectivitiy
	SDL_SetHint (SDL_HINT_JOYSTICK_HIDAPI_PS4_RUMBLE, "1");
	SDL_SetHint (SDL_HINT_JOYSTICK_HIDAPI_PS5_RUMBLE, "1");
#endif
#if SDL_VERSION_ATLEAST(2, 23, 2)
	SDL_SetHint (SDL_HINT_JOYSTICK_HIDAPI_COMBINE_JOY_CONS, "1");
#endif
#if SDL_VERSION_ATLEAST(2, 25, 1)
	SDL_SetHint (SDL_HINT_JOYSTICK_HIDAPI_PS3, "1");
#endif
#if SDL_VERSION_ATLEAST(2, 26, 0)
	SDL_SetHint (SDL_HINT_JOYSTICK_HIDAPI_WII, "1");
#endif
#if SDL_VERSION_ATLEAST(2, 0, 18)
	SDL_SetHint (SDL_HINT_JOYSTICK_RAWINPUT, "1");
	SDL_SetHint (SDL_HINT_JOYSTICK_RAWINPUT_CORRELATE_XINPUT, "1");
#endif

    if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) == -1)
    {
        Con_Warning("could not initialize SDL Game Controller\n");
        return;
    }

    // Load additional SDL2 controller definitions from gamecontrollerdb.txt
    for (i = 0; i < com_numbasedirs; i++)
    {
        q_snprintf(controllerdb, sizeof(controllerdb), "%s/gamecontrollerdb.txt", com_basedirs[i]);
        nummappings = SDL_GameControllerAddMappingsFromFile(controllerdb);
        if (nummappings > 0)
            Con_Printf("%d mappings loaded from gamecontrollerdb.txt\n", nummappings);
    }

    IN_SetupJoystick();
}

void IN_ShutdownJoystick (void)
{
	int k;

	IN_UseController (-1);
	for (k = 1; k < MAX_JOYPADS; k++) // QVR: splitscreen: the other pads
		IN_ClosePad (&joy_pads[k]);
	SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
}

// QVR: the menus' gamepad (its name, type, rumble, gyro; the gyro's calibration) is pad 0's: one menu (SPLITSCREEN.md,
// phase 9)
qboolean IN_HasGamepad (void)
{
	return joy_primary->controller != NULL;
}

const char *IN_GetGamepadName (void)
{
	return joy_primary->controller ? joy_primary->name : NULL;
}

gamepadtype_t IN_GetGamepadType (void)
{
	return joy_primary->type;
}

// QVR: SDL's own type of the gamepad in use (SDL_GameControllerType; -1: none, 0: unknown): the Controller Bindings
// screen's picture (vr/vr_bindsetup.cpp). split20: the current seat's pad (seat 0: the keyboard player's own pad, pad 0
// with one seat); a mock pad (the tests') is an unknown one (0).
int IN_GetGamepadSdlType (void)
{
	joypad_t *pad = IN_SeatPad (CL_SeatIndex ());

	if (!pad)
		return -1;
	if (!pad->controller)
		return pad->mock ? 0 : -1;
#if SDL_VERSION_ATLEAST(2, 0, 12)
	return (int) SDL_GameControllerGetType (pad->controller);
#else
	return 0;
#endif
}

// QVR: nonzero when a gamepad is connected and is the device last used (the bindings reminder panels show its buttons
// then: vr/vr_bindpanel.cpp).
int IN_GamepadLastUsed (void)
{
	return IN_SeatPad (CL_SeatIndex ()) != NULL && IN_GetLastActiveDeviceType () == KD_GAMEPAD; // (split20: the current seat's pad)
}

void IN_UseNextGamepad (int dir, qboolean allow_disable)
{
	int i, j, numiter, numdev;

	numdev = SDL_NumJoysticks ();
	numiter = allow_disable ? numdev : numdev - 1;

	for (i = 0, j = joy_primary->device + dir; i < numiter; i++, j += dir)
	{
		if (j < -1)
			j = numdev - 1;
		else if (j < 0)
			j = allow_disable ? -1 : numdev - 1;
		else if (j >= numdev)
			j = allow_disable ? -1 : 0;

		if ((j == -1 || SDL_IsGameController (j)) && IN_UseController (j))
			break; // QVR: was return (the other pads brought up to date below)
	}
	IN_RefreshExtraPads (); // QVR: splitscreen: the one pad 0 gave up opened as another pad
}

qboolean IN_HasRumble (void)
{
	return joy_primary->has_rumble;
}

// QVR: splitscreen: the gyro and alt modifier buttons are the seat's whose bind runs them (seatkeys_t)
void IN_GyroActionDown (void)
{
	Key_Seat ()->gyrobutton = true;
}

void IN_GyroActionUp (void)
{
	Key_Seat ()->gyrobutton = false;
}

static void IN_JoyAltModifierDown (void)
{
	Key_Seat ()->altmodifier = true;
}

static void IN_JoyAltModifierUp (void)
{
	Key_Seat ()->altmodifier = false;
}

/*
================
IN_Pads_f -- QVR (splitscreen)

in_pads: the controllers open and the seat each drives (Debug menu: Splitscreen pads)
================
*/
static void IN_Pads_f (void)
{
	int k, seat;
	const joypad_t *pad;

	Con_Printf ("%d seat(s) in play; seat 1: keyboard and mouse\n", CL_SeatsInPlay ());
	for (k = 0; k < MAX_JOYPADS; k++)
	{
		pad = &joy_pads[k];
		seat = IN_PadSeat (k);
		if (!pad->controller && !pad->mock)
			Con_Printf ("pad %d: -\n", k + 1);
		else
			Con_Printf ("pad %d: %s (device %d)%s, %s%s\n", k + 1, pad->mock ? "mock pad" : pad->name, pad->device,
				k == 0 ? ", the Options menu's" : "", seat < 0 ? "drives no seat" : "drives seat ",
				seat < 0 ? "" : va ("%d", seat + 1));
	}
}

/*
================
IN_MockPad, IN_MockPadButton, IN_MockPadAxes -- QVR (vr_seat_test)

A pad without a controller, its buttons and sticks set by the test, polled by IN_Commands as a real one is. split12: a
real controller plugged in at k is stood in for while the mock is on (its reads are the mock's: every read takes
mockbuttons / mockaxes when pad->mock), so the tests run the same with a controller attached (the seat tests' pad 0 was
the plugged-in one, its stick at rest).
================
*/
void IN_MockPad (int k, qboolean on)
{
	joypad_t *pad;

	if (k < 0 || k >= MAX_JOYPADS)
		return;
	pad = &joy_pads[k];
	if (pad->controller)
	{
		pad->mock = on;
		memset (&pad->mockbuttons, 0, sizeof (pad->mockbuttons));
		memset (&pad->mockaxes, 0, sizeof (pad->mockaxes));
		return;
	}
	memset (pad, 0, sizeof (*pad));
	pad->instanceid = -1;
	pad->device = -1;
	pad->seat = -1;
	pad->mock = on;
	if (on)
		q_strlcpy (pad->name, "mock pad", sizeof (pad->name));
}

void IN_MockPadButton (int k, int button, qboolean down)
{
	if (k >= 0 && k < MAX_JOYPADS && button >= 0 && button < SDL_CONTROLLER_BUTTON_MAX)
		joy_pads[k].mockbuttons.buttondown[button] = down;
}

void IN_MockPadAxes (int k, float lx, float ly, float rx, float ry)
{
	if (k < 0 || k >= MAX_JOYPADS)
		return;
	joy_pads[k].mockaxes.axisvalue[SDL_CONTROLLER_AXIS_LEFTX] = lx;
	joy_pads[k].mockaxes.axisvalue[SDL_CONTROLLER_AXIS_LEFTY] = ly;
	joy_pads[k].mockaxes.axisvalue[SDL_CONTROLLER_AXIS_RIGHTX] = rx;
	joy_pads[k].mockaxes.axisvalue[SDL_CONTROLLER_AXIS_RIGHTY] = ry;
}

/*
================
Joy_Device_f

Called when joy_device changes
================
*/
static void Joy_Device_f (cvar_t *cvar)
{
	if ((int)cvar->value != joy_primary->device)
		IN_SetupJoystick ();
}

/*
================
Joy_Device_Completion_f

Tab completion for the joy_device cvar
================
*/
static void Joy_Device_Completion_f (cvar_t *cvar, const char *partial)
{
	int i, count;

	for (i = 0, count = SDL_NumJoysticks (); i < count; i++)
		if (SDL_IsGameController (i))
			Con_AddToTabList (va ("%d", i), partial, SDL_GameControllerNameForIndex (i));
}

/*
================
Joy_Flick_f

Called when joy_flick changes
================
*/
static void Joy_Flick_f (cvar_t *cvar)
{
	int k;

	for (k = 0; k < MAX_JOYPADS; k++) // QVR: every pad's
		IN_ResetFlickState (&joy_pads[k]);
}

/*
================
IN_UpdateSDLTextInput

Calls SDL_StartTextInput/StopTextInput as needed
based on textmode and the device type
================
*/
static void IN_UpdateSDLTextInput (void)
{
	// For devices with an on-screen keyboard (e.g. Steam Deck) we only start text input
	// if requested explicitly (TEXTMODE_ON) in order to avoid having the OSK pop up
	// in searchable menus (Options, Maps, or Mods), which use emulated char events instead.
	// For all the other devices we start text input if not disabled explicitly (TEXTMODE_OFF).
	// In other words, TEXTMODE_NOPOPUP is treated as OFF on Steam Deck, ON on desktop.
	qboolean enabled = SDL_HasScreenKeyboardSupport () ? textmode == TEXTMODE_ON : textmode != TEXTMODE_OFF;
	if (enabled)
		SDL_StartTextInput ();
	else
		SDL_StopTextInput ();
}

/*
================
IN_Init
================
*/
void IN_Init (void)
{
	textmode = Key_TextEntry();
	IN_UpdateSDLTextInput ();

	if (safemode || COM_CheckParm("-nomouse"))
	{
		no_mouse = true;
		/* discard all mouse events when input is deactivated */
		IN_BeginIgnoringMouseEvents();
	}

#ifdef MACOS_X_ACCELERATION_HACK
	Cvar_RegisterVariable(&in_disablemacosxmouseaccel);
#endif
	Cvar_RegisterVariable(&in_debugkeys);
	Cvar_RegisterVariable(&joy_sensitivity_yaw);
	Cvar_RegisterVariable(&joy_sensitivity_pitch);
	Cvar_RegisterVariable(&joy_deadzone_look);
	Cvar_RegisterVariable(&joy_deadzone_move);
	Cvar_RegisterVariable(&joy_outer_threshold_look);
	Cvar_RegisterVariable(&joy_outer_threshold_move);
	Cvar_RegisterVariable(&joy_deadzone_trigger);
	Cvar_RegisterVariable(&joy_invert);
	Cvar_RegisterVariable(&joy_exponent);
	Cvar_RegisterVariable(&joy_exponent_move);
	Cvar_RegisterVariable(&joy_swapmovelook);
	Cvar_RegisterVariable(&joy_flick);
	Cvar_SetCallback (&joy_flick, Joy_Flick_f);
	Cvar_RegisterVariable(&joy_flick_time);
	Cvar_RegisterVariable(&joy_flick_recenter);
	Cvar_RegisterVariable(&joy_flick_deadzone);
	Cvar_RegisterVariable(&joy_flick_noise_thresh);
	Cvar_RegisterVariable(&joy_flick_adjust_speed);
	Cvar_RegisterVariable(&joy_rumble);
	Cvar_RegisterVariable(&joy_device);
	Cvar_SetCallback(&joy_device, Joy_Device_f);
	Cvar_SetCompletion(&joy_device, Joy_Device_Completion_f);
	Cvar_RegisterVariable(&joy_always_active);

	Cvar_RegisterVariable(&gyro_enable);
	Cvar_RegisterVariable(&gyro_mode);
	Cvar_RegisterVariable(&gyro_turning_axis);

	Cvar_RegisterVariable(&gyro_yawsensitivity);
	Cvar_RegisterVariable(&gyro_pitchsensitivity);

	Cvar_RegisterVariable(&gyro_calibration_x);
	Cvar_RegisterVariable(&gyro_calibration_y);
	Cvar_RegisterVariable(&gyro_calibration_z);
	Cvar_RegisterVariable(&gyro_noise_thresh);

	Cmd_AddCommand ("+gyroaction", IN_GyroActionDown);
	Cmd_AddCommand ("-gyroaction", IN_GyroActionUp);
	Cmd_AddCommand ("+altmodifier", IN_JoyAltModifierDown);
	Cmd_AddCommand ("-altmodifier", IN_JoyAltModifierUp);
	Cmd_AddCommand ("in_pads", IN_Pads_f); // QVR: splitscreen: the controllers open, the seat each drives

	IN_Activate();
	IN_StartupJoystick();
	// QVR: only with the keyboard's focus (else on SDL_WINDOWEVENT_FOCUS_GAINED). The hook is the whole desktop's:
	// each key press in any program waits until this thread services it (it pumps once a frame, not at all through
	// a load), up to Windows' LowLevelHooksTimeout. A window never focused (a test run's, hidden or in the background)
	// never had a focus loss to take it off, and its loads held up every key the desktop's player pressed.
	if (SDL_GetWindowFlags ((SDL_Window *) VID_GetWindow ()) & SDL_WINDOW_INPUT_FOCUS)
		Sys_ActivateKeyFilter(true);
}

void IN_Shutdown (void)
{
	Sys_ActivateKeyFilter(false);
	IN_Deactivate(true);
	IN_ShutdownJoystick();
}

extern cvar_t v_centerspeed;
extern cvar_t cl_maxpitch; /* johnfitz -- variable pitch clamping */
extern cvar_t cl_minpitch; /* johnfitz -- variable pitch clamping */
extern cvar_t scr_fov;

static float IN_FovScale (void)
{
	// QVR: without flatscreen play's speed FOV (vr_flatspeedfov.cpp): a degree of mouse stays a degree of turn at any speed.
	// In its ironsight, times vr_flat_aim_sens and the match distance's correction (vr_flataimsens.cpp; 1 at their defaults).
	const float lookfov = VR_FlatLookFov (r_refdef.basefov);
	return tan (DEG2RAD (lookfov) * 0.5f) / tan (DEG2RAD (scr_fov.value) * 0.5f) * VR_FlatAimSens (lookfov, scr_fov.value);
}

// QVR: splitscreen: the mouse is seat 0's (its motion goes to seat 0's mouse_dx/dy whichever seat is current)
void IN_MouseMotion(int dx, int dy)
{
	seatkeys_t *mouse = Key_SeatOf (0);

	if (!windowhasfocus)
		dx = dy = 0;	//don't change view angles etc while unfocused.
	if (cls.state != ca_connected || cls.signon != SIGNONS || key_dest != key_game || CL_InCutscene ())
	{
		mouse->mouse_dx = 0;
		mouse->mouse_dy = 0;
		return;
	}
	if (VR_FlatToolgunMouse (dx, dy)) // QVR: flatscreen play's toolgun panel cursor, or its adjust key held: not the view's
		return;
	mouse->mouse_dx += dx;
	mouse->mouse_dy += dy;
}

// QVR: the tests' mouse (vr_flatleanmouse_test; vr/vr_flatmove.cpp): counts as if the mouse moved, the window's focus
// aside (the headless window has none). Seat 0's, as the mouse.
void IN_MockMouseMotion (int dx, int dy)
{
	if (cls.state != ca_connected || cls.signon != SIGNONS || key_dest != key_game)
		return;
	Key_SeatOf (0)->mouse_dx += dx;
	Key_SeatOf (0)->mouse_dy += dy;
}


#ifdef __WATCOMC__ /* OW1.9 doesn't have powf() / sqrtf() */
#define powf pow
#define sqrtf sqrt
#endif

/*
================
IN_AxisMagnitude

Returns the vector length of the given joystick axis
================
*/
static vec_t IN_AxisMagnitude(joyaxis_t axis)
{
	vec_t magnitude = sqrtf((axis.x * axis.x) + (axis.y * axis.y));
	return magnitude;
}

/*
================
IN_ApplyEasing

assumes axis values are in [-1, 1] and the vector magnitude has been clamped at 1.
Raises the axis values to the given exponent, keeping signs.
================
*/
static joyaxis_t IN_ApplyEasing(joyaxis_t axis, float exponent)
{
	joyaxis_t result = {0};
	vec_t eased_magnitude;
	vec_t magnitude = IN_AxisMagnitude(axis);
	
	if (magnitude == 0)
		return result;
	
	eased_magnitude = powf(magnitude, exponent);
	
	result.x = axis.x * (eased_magnitude / magnitude);
	result.y = axis.y * (eased_magnitude / magnitude);
	return result;
}

/*
================
IN_ApplyDeadzone

in: raw joystick axis values converted to floats in +-1
out: applies a circular inner deadzone and a circular outer threshold and clamps the magnitude at 1
     (my 360 controller is slightly non-circular and the stick travels further on the diagonals)

deadzone is expected to satisfy 0 < deadzone < 1 - outer_threshold
outer_threshold is expected to satisfy 0 < outer_threshold < 1 - deadzone

from https://github.com/jeremiah-sypult/Quakespasm-Rift
and adapted from http://www.third-helix.com/2013/04/12/doing-thumbstick-dead-zones-right.html
================
*/
static joyaxis_t IN_ApplyDeadzone(joyaxis_t axis, float deadzone, float outer_threshold)
{
	joyaxis_t result = {0};
	vec_t magnitude = IN_AxisMagnitude(axis);
	
	if ( magnitude > deadzone ) {
		// rescale the magnitude so deadzone becomes 0, and 1-outer_threshold becomes 1
		const vec_t new_magnitude = q_min(1.0, (magnitude - deadzone) / (1.0 - deadzone - outer_threshold));
		const vec_t scale = new_magnitude / magnitude;
		result.x = axis.x * scale;
		result.y = axis.y * scale;
	}
	
	return result;
}

/*
================
IN_KeyForControllerButton
================
*/
static int IN_KeyForControllerButton(SDL_GameControllerButton button)
{
	switch (button)
	{
		case SDL_CONTROLLER_BUTTON_A: return K_ABUTTON;
		case SDL_CONTROLLER_BUTTON_B: return K_BBUTTON;
		case SDL_CONTROLLER_BUTTON_X: return K_XBUTTON;
		case SDL_CONTROLLER_BUTTON_Y: return K_YBUTTON;
		// Note: back and start are always mapped to TAB/ESC, the player cannot rebind them
		case SDL_CONTROLLER_BUTTON_BACK: return K_BACK; // QVR: its own key (Tab before), bindable (padflat: the alt layer; vr_bindings.cfg)
		case SDL_CONTROLLER_BUTTON_START: return K_ESCAPE;
		case SDL_CONTROLLER_BUTTON_LEFTSTICK: return K_LTHUMB;
		case SDL_CONTROLLER_BUTTON_RIGHTSTICK: return K_RTHUMB;
		case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: return K_LSHOULDER;
		case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return K_RSHOULDER;
		case SDL_CONTROLLER_BUTTON_DPAD_UP: return K_DPAD_UP;
		case SDL_CONTROLLER_BUTTON_DPAD_DOWN: return K_DPAD_DOWN;
		case SDL_CONTROLLER_BUTTON_DPAD_LEFT: return K_DPAD_LEFT;
		case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return K_DPAD_RIGHT;
		case SDL_CONTROLLER_BUTTON_MISC1: return K_MISC1;
		case SDL_CONTROLLER_BUTTON_PADDLE1: return K_PADDLE1;
		case SDL_CONTROLLER_BUTTON_PADDLE2: return K_PADDLE2;
		case SDL_CONTROLLER_BUTTON_PADDLE3: return K_PADDLE3;
		case SDL_CONTROLLER_BUTTON_PADDLE4: return K_PADDLE4;
		case SDL_CONTROLLER_BUTTON_TOUCHPAD: return K_TOUCHPAD;
		default: return 0;
	}
}

/*
================
IN_JoyKeyEvent

Sends a Key_Event if a unpressed -> pressed or pressed -> unpressed transition occurred,
and generates key repeats if the button is held down.

Adapted from DarkPlaces by lordhavoc
================
*/
// QVR: a gamepad's key event, through the menus' gamepad cursor (padflat; vr/vr_menuui.cpp): A clicks while it shows
static void IN_PadKeyEvent (int key, qboolean down)
{
	key = VR_MenuPadKey (key, down);
	if (key >= 0)
		Key_Event (key, down);
}

static void IN_JoyKeyEvent(qboolean wasdown, qboolean isdown, int key, double *timer)
{
	static const double repeatdelay = 0.5; // time (in seconds) between initial press and first repetition
	static const double repeatrate = 32.0; // ticks per second

	// we can't use `realtime` for key repeats because it is not monotomic
	const double currenttime = Sys_DoubleTime();
	
	if (wasdown)
	{
		if (isdown)
		{
			if (currenttime >= *timer)
			{
				*timer = currenttime + 1.0 / repeatrate;
				lastactivetype = KD_GAMEPAD;
				IN_PadKeyEvent(key, true);
			}
		}
		else
		{
			*timer = 0;
			lastactivetype = KD_GAMEPAD;
			IN_PadKeyEvent(key, false);
		}
	}
	else
	{
		if (isdown)
		{
			*timer = currenttime + repeatdelay;
			lastactivetype = KD_GAMEPAD;
			IN_PadKeyEvent(key, true);
		}
	}
}

static joyaxis_t IN_GetLookAxis (const joyaxisstate_t *state)
{
	joyaxis_t axis;
	axis.x = state->axisvalue[joy_swapmovelook.value ? SDL_CONTROLLER_AXIS_LEFTX : SDL_CONTROLLER_AXIS_RIGHTX];
	axis.y = state->axisvalue[joy_swapmovelook.value ? SDL_CONTROLLER_AXIS_LEFTY : SDL_CONTROLLER_AXIS_RIGHTY];
	return axis;
}

static joyaxis_t IN_GetMoveAxis (const joyaxisstate_t *state)
{
	joyaxis_t axis;
	axis.x = state->axisvalue[joy_swapmovelook.value ? SDL_CONTROLLER_AXIS_RIGHTX : SDL_CONTROLLER_AXIS_LEFTX];
	axis.y = state->axisvalue[joy_swapmovelook.value ? SDL_CONTROLLER_AXIS_RIGHTY : SDL_CONTROLLER_AXIS_LEFTY];
	return axis;
}

// QVR: splitscreen: a pad counts as in use (its gyro, its rumble): driving seat 0, as Ironwail's pad did (always, or
// after the last device used was a gamepad); driving a controller's seat, always
static qboolean IN_JoyActive (joypad_t *pad)
{
	if (!pad || (!pad->controller && !pad->mock))
		return false;
	return joy_always_active.value || lastactivetype == KD_GAMEPAD || IN_PadSeat (IN_PadIndex (pad)) > 0;
}

/*
================
IN_PadEmit -- QVR (splitscreen: Ironwail's IN_Commands' events, for any pad)

A pad's new buttons and sticks: key events for what changed, in the current seat (the pad's), then kept as its state
================
*/
static void IN_PadEmit (joypad_t *pad, const joybuttonstate_t *newbuttons, const joyaxisstate_t *newaxisstate)
{
	int i;
	const float stickthreshold = 0.9;
	const float triggerthreshold = joy_deadzone_trigger.value;

	// emit key events for controller buttons
	for (i = 0; i < SDL_CONTROLLER_BUTTON_MAX; i++)
	{
		qboolean newstate = newbuttons->buttondown[i];
		qboolean oldstate = pad->buttonstate.buttondown[i];

		pad->buttonstate.buttondown[i] = newstate;

		// NOTE: This can cause a reentrant call of IN_Commands, via SCR_ModalMessage when confirming a new game.
		IN_JoyKeyEvent(oldstate, newstate, IN_KeyForControllerButton((SDL_GameControllerButton)i), &pad->buttontimer[i]);
	}

	// emit emulated arrow keys so the analog sticks can be used in the menu
	// QVR: split21: the menu's gamepad cursor (padflat) is seat 1's full menu's; a seat 2-4 pad's own seat menu takes the
	// emulated arrows (splitscreen phase 9: Key_SeatDest is the pad's seat's)
	if (key_dest == key_menu && (CL_SeatIndex () == 0 || !VR_SeatMenusOn ()) && VR_MenuPadStick (IN_ApplyDeadzone (IN_GetMoveAxis (newaxisstate), 0.f, joy_outer_threshold_move.value).x,
			IN_ApplyDeadzone (IN_GetMoveAxis (newaxisstate), 0.f, joy_outer_threshold_move.value).y, (float) host_rawframetime))
	{
		// QVR: the left stick moved the menu's gamepad cursor (padflat; the d-pad still steps the rows)
		if (IN_AxisMagnitude (IN_GetMoveAxis (newaxisstate)) > joy_deadzone_move.value)
			lastactivetype = KD_GAMEPAD;
	}
	else if (Key_SeatDest () != key_game)
	{
		joyaxis_t old_move = IN_GetMoveAxis (&pad->axisstate);
		joyaxis_t new_move = IN_GetMoveAxis (newaxisstate);
		IN_JoyKeyEvent(old_move.x < -stickthreshold, new_move.x < -stickthreshold, K_LEFTARROW,		&pad->emulatedkeytimer[0]);
		IN_JoyKeyEvent(old_move.x >  stickthreshold, new_move.x >  stickthreshold, K_RIGHTARROW,	&pad->emulatedkeytimer[1]);
		IN_JoyKeyEvent(old_move.y < -stickthreshold, new_move.y < -stickthreshold, K_UPARROW,		&pad->emulatedkeytimer[2]);
		IN_JoyKeyEvent(old_move.y >  stickthreshold, new_move.y >  stickthreshold, K_DOWNARROW,		&pad->emulatedkeytimer[3]);
	}
	else if (lastactivetype != KD_GAMEPAD)
	{
		if (IN_AxisMagnitude (IN_GetLookAxis (newaxisstate)) > joy_deadzone_look.value ||
			IN_AxisMagnitude (IN_GetMoveAxis (newaxisstate)) > joy_deadzone_move.value)
			lastactivetype = KD_GAMEPAD;
	}

	// emit emulated keys for the analog triggers
	IN_JoyKeyEvent(pad->axisstate.axisvalue[SDL_CONTROLLER_AXIS_TRIGGERLEFT] > triggerthreshold,  newaxisstate->axisvalue[SDL_CONTROLLER_AXIS_TRIGGERLEFT] > triggerthreshold, K_LTRIGGER, &pad->emulatedkeytimer[4]);
	IN_JoyKeyEvent(pad->axisstate.axisvalue[SDL_CONTROLLER_AXIS_TRIGGERRIGHT] > triggerthreshold, newaxisstate->axisvalue[SDL_CONTROLLER_AXIS_TRIGGERRIGHT] > triggerthreshold, K_RTRIGGER, &pad->emulatedkeytimer[5]);

	pad->axisstate = *newaxisstate;
}

/*
================
IN_PadPoll -- QVR (splitscreen: the rest of Ironwail's IN_Commands, for any pad)

A pad read (its controller, or a mock pad's set state), its events emitted in the current seat (the pad's), the look
stick scrolling the console, the rumble
================
*/
static void IN_PadPoll (joypad_t *pad)
{
	joybuttonstate_t newbuttons;
	joyaxisstate_t newaxisstate;
	int i;

	for (i = 0; i < SDL_CONTROLLER_BUTTON_MAX; i++)
		newbuttons.buttondown[i] = pad->mock ? pad->mockbuttons.buttondown[i] : SDL_GameControllerGetButton(pad->controller, (SDL_GameControllerButton)i);
	for (i = 0; i < SDL_CONTROLLER_AXIS_MAX; i++)
		newaxisstate.axisvalue[i] = pad->mock ? pad->mockaxes.axisvalue[i] : SDL_GameControllerGetAxis(pad->controller, (SDL_GameControllerAxis)i) / 32768.0f;

	IN_PadEmit (pad, &newbuttons, &newaxisstate);

	// scroll console with look stick
	if (Key_SeatDest () == key_console) // QVR: (seat 1's console only)
	{
		const float scrollthreshold = 0.1f;
		const float maxscrollspeed = 72.f; // lines per second
		const float scrollinterval = 1.f / maxscrollspeed;
		joyaxis_t raw, deadzone, eased;
		float scale;

		raw = IN_GetLookAxis (&pad->axisstate);
		deadzone = IN_ApplyDeadzone (raw, joy_deadzone_look.value, joy_outer_threshold_look.value);
		eased = IN_ApplyEasing (deadzone, joy_exponent.value);
		if (joy_invert.value)
			eased.y = -eased.y;

		scale = fabs (eased.y);
		if (scale > scrollthreshold)
		{
			scale = (scale - scrollthreshold) / (1.f - scrollthreshold);
			pad->scrolltimer -= scale * host_rawframetime;
			if (pad->scrolltimer < 0.0)
			{
				int ticks = (int) ceil (-pad->scrolltimer / scrollinterval);
				pad->scrolltimer += ticks * scrollinterval;
				Con_Scroll (eased.y < 0.0f ? ticks : -ticks);
			}
		}
		else
		{
			pad->scrolltimer = 0.0;
		}
	}

#if SDL_VERSION_ATLEAST (2, 0, 9)
	if (pad->controller && pad->has_rumble && !IN_IsCalibratingGyro () && joy_rumble.value > 0.f && IN_JoyActive (pad))
	{
		float strength = CLAMP (0.f, joy_rumble.value, 1.f) * 0xffff;
		float lofreq = GetClampedFraction (S_GetLoFreqLevel (), 0.067f, 0.45f);
		float hifreq = GetClampedFraction (S_GetHiFreqLevel (), 0.061f, 0.45f);
		hifreq *= hifreq;
		SDL_GameControllerRumble (pad->controller, lofreq * strength, hifreq * strength, 100);
	}
#endif // SDL_VERSION_ATLEAST (2, 0, 9)
}

/*
================
IN_PadsToJoinScreen -- QVR (splitscreen phase 10)

While the join screen is up (M_Splitscreen_OwnsPads) every pad is the screen's: a pad's A joins, its B leaves...
whichever seat it drives (vr/vr_splitjoin.cpp reads them with IN_PadDown, IN_PadAxis). No key events: on the first
frame each pad lets go in its seat; then its state is kept as read, so a button still held when the screen shuts
(Start, which starts the game) is not pressed again in the pad's new seat.
================
*/
static qboolean joy_joinscreen = false; // seat-shared: the join screen had the pads last frame (one screen: seat 1's)

static void IN_PadsToJoinScreen (const joybuttonstate_t *nobuttons, const joyaxisstate_t *noaxes)
{
	joypad_t *pad;
	int k, seat, prev;

	for (k = 0; k < MAX_JOYPADS; k++)
	{
		pad = &joy_pads[k];
		if (!pad->controller && !pad->mock)
			continue;
		if (!joy_joinscreen && pad->seat >= 0) // (let go in its seat: the buttons that opened the screen)
		{
			prev = CL_SeatSet (pad->seat);
			IN_PadEmit (pad, nobuttons, noaxes);
			CL_SeatSet (prev);
		}
		IN_PadKeepAsRead (pad);
	}
	joy_joinscreen = true;
	M_Splitscreen_Pads (); // (it joins and leaves seats)
	for (k = 0; k < MAX_JOYPADS; k++) // (each pad's seat now: no letting go there when the screen shuts)
	{
		pad = &joy_pads[k];
		seat = IN_PadSeat (k);
		if (seat != pad->seat && seat > 0 && !Key_SeatHasBindings (seat))
			Key_SeatCopyBindings (seat, 0);
		pad->seat = seat;
	}
}

// QVR: splitscreen phase 10: pad k's name (NULL: no pad there), a button held, a stick's axis (as last read)
const char *IN_PadName (int k)
{
	if (k < 0 || k >= MAX_JOYPADS || (!joy_pads[k].controller && !joy_pads[k].mock))
		return NULL;
	return joy_pads[k].name;
}

qboolean IN_PadDown (int k, int button)
{
	if (!IN_PadName (k) || button < 0 || button >= SDL_CONTROLLER_BUTTON_MAX)
		return false;
	return joy_pads[k].buttonstate.buttondown[button];
}

float IN_PadAxis (int k, int axis)
{
	if (!IN_PadName (k) || axis < 0 || axis >= SDL_CONTROLLER_AXIS_MAX)
		return 0.f;
	return joy_pads[k].axisstate.axisvalue[axis];
}

/*
================
IN_Commands

Emit key events for game controller buttons, including emulated buttons for analog sticks/triggers

QVR: splitscreen: every pad's, each with its seat current (IN_PadSeat: the device names its seat), so its binds
press that seat's buttons. A pad whose seat changed (seats joined or left, unplugged) lets go there first.
================
*/
/*
================
IN_PadKeepAsRead -- QVR (splitscreen)

A pad no seat polls (the join screen has it, or it drives none): its state kept as read, so what it holds when a seat
polls it again is no press there; nor a key repeat (split11: a held Start's repeat, a press to a seat that never saw
the press, opened the seat's menu as the join screen shut).
================
*/
static void IN_PadKeepAsRead (joypad_t *pad)
{
	int i;
	for (i = 0; i < SDL_CONTROLLER_BUTTON_MAX; i++)
	{
		pad->buttonstate.buttondown[i] = pad->mock ? pad->mockbuttons.buttondown[i] : SDL_GameControllerGetButton (pad->controller, (SDL_GameControllerButton)i);
		if (pad->buttonstate.buttondown[i])
			pad->buttontimer[i] = Sys_DoubleTime () + 1e9; // (no repeat until it is let go and pressed again)
	}
	for (i = 0; i < SDL_CONTROLLER_AXIS_MAX; i++)
		pad->axisstate.axisvalue[i] = pad->mock ? pad->mockaxes.axisvalue[i] : SDL_GameControllerGetAxis (pad->controller, (SDL_GameControllerAxis)i) / 32768.0f;
}

/*
================
IN_PadDropIn -- QVR (split11)

A pad that drives no seat (one that left its seat, or a spare) in a running local multiplayer game, flatscreen: its
Start or A joins it as the next free seat at once (drop-in, no menu: VR_SeatDropIn).
================
*/
static void IN_PadDropIn (int k)
{
	joypad_t *pad = &joy_pads[k];
	qboolean held;
	if (!pad->controller && !pad->mock)
		return;
	// (its state kept as read, as the join screen keeps it: the buttons held as it joins are no presses in its seat, so
	// the Start that joins opens no seat menu and the A no jump)
	IN_PadKeepAsRead (pad);
	held = pad->buttonstate.buttondown[SDL_CONTROLLER_BUTTON_START] || pad->buttonstate.buttondown[SDL_CONTROLLER_BUTTON_A];
	if (held && !pad->joinheld && sv.active && svs.maxclients > 1 && !cls.demoplayback && cls.signon == SIGNONS)
		VR_SeatDropIn (k);
	pad->joinheld = held;
}

void IN_Commands (void)
{
	joybuttonstate_t nobuttons;
	joyaxisstate_t noaxes;
	joypad_t *pad;
	int k, seat, prev;

	memset (&nobuttons, 0, sizeof (nobuttons));
	memset (&noaxes, 0, sizeof (noaxes));
	if (M_Splitscreen_OwnsPads ()) // QVR: splitscreen phase 10: the join screen reads every pad itself
	{
		IN_PadsToJoinScreen (&nobuttons, &noaxes);
		return;
	}
	joy_joinscreen = false;
	for (k = 0; k < MAX_JOYPADS; k++)
	{
		pad = &joy_pads[k];
		seat = IN_PadSeat (k);
		if (seat != pad->seat)
		{
			if (pad->seat >= 0)
			{
				prev = CL_SeatSet (pad->seat);
				IN_PadEmit (pad, &nobuttons, &noaxes);
				CL_SeatSet (prev);
			}
			pad->seat = seat;
			if (seat > 0 && !Key_SeatHasBindings (seat)) // a controller's seat with no bindings yet: seat 0's to start
				Key_SeatCopyBindings (seat, 0);
		}
		if (seat < 0)
		{
			IN_PadDropIn (k);
			continue;
		}
		prev = CL_SeatSet (seat);
		IN_PadPoll (pad);
		CL_SeatSet (prev);
	}
}


/*
================
IN_FlickStickEasing
================
*/
static float IN_FlickStickEasing (float frac)
{
	frac = 1.f - frac;
	frac = 1.f - frac * frac;
	return frac;
}

/*
================
IN_JoyMove
================
*/
void IN_JoyMove (usercmd_t *cmd)
{
	float	speed;
	joyaxis_t moveRaw, moveDeadzone, moveEased;
	joyaxis_t lookRaw, lookDeadzone, lookEased;
	extern	cvar_t	sv_maxspeed;
	joypad_t *pad = IN_SeatPad (CL_SeatIndex ()); // QVR: splitscreen: the pad driving the seat moving

	if (!pad)
		return;

	if (cl.paused || Key_SeatDest () != key_game) // QVR: splitscreen phase 9: the moving seat's (its seat menu open: no move)
		return;

	moveRaw = IN_GetMoveAxis (&pad->axisstate);
	lookRaw = IN_GetLookAxis (&pad->axisstate);

	moveDeadzone = IN_ApplyDeadzone(moveRaw, joy_deadzone_move.value, joy_outer_threshold_move.value);
	lookDeadzone = IN_ApplyDeadzone(lookRaw, joy_deadzone_look.value, joy_outer_threshold_look.value);

	moveEased = IN_ApplyEasing(moveDeadzone, joy_exponent_move.value);
	lookEased = IN_ApplyEasing(lookDeadzone, joy_exponent.value);

	if (VR_SpeedButton (CL_SeatInput()->speed.state & 1) ^ (cl_alwaysrun.value != 0.0 || cl_forwardspeed.value >= sv_maxspeed.value)) // QVR: VR_SpeedButton (vr_sprint_toggle)
		// running
		speed = sv_maxspeed.value;
	else if (cl_forwardspeed.value >= sv_maxspeed.value)
		// not running, with always run = vanilla
		speed = q_min(sv_maxspeed.value, cl_forwardspeed.value / cl_movespeedkey.value);
	else
		// not running, with always run = off or quakespasm
		speed = cl_forwardspeed.value;

	cmd->sidemove += speed * moveEased.x;
	cmd->forwardmove -= speed * moveEased.y;

	if (CL_InCutscene ())
		return;

	// QVR: flatscreen play's open weapon wheel or holster menu: the right stick is their cursor (padflat; vr_flatplay.cpp;
	// split21: the pad's seat's wheel and menu, its own cursor's remainder)
	if (VR_FlatStickCursor (lookDeadzone.x, lookDeadzone.y, (float) host_rawframetime))
	{
		pad->cursorfrac[0] = pad->cursorfrac[1] = 0.f;
		IN_ResetFlickState (pad);
		return;
	}
	// QVR: split11: flat play's toolgun panel open: the look stick moves its cursor, as the mouse does (a controller seat
	// has no mouse); the view stays
	{
		const float speed = 900.f * (float) host_rawframetime; // counts a second at full tilt (a window pixel each)
		int dx, dy;
		pad->cursorfrac[0] += lookEased.x * speed;
		pad->cursorfrac[1] += lookEased.y * speed;
		dx = (int) pad->cursorfrac[0];
		dy = (int) pad->cursorfrac[1];
		if (VR_FlatToolgunMouse (dx, dy))
		{
			pad->cursorfrac[0] -= (float) dx;
			pad->cursorfrac[1] -= (float) dy;
			return;
		}
		pad->cursorfrac[0] = pad->cursorfrac[1] = 0.f;
	}

	// handle flick stick if enabled
	if (joy_flick.value && pad->gyro_present && gyro_enable.value)
	{
		float		angle, scale, lerp_frac, delta;
		qboolean	isactive, wasactive;

		// get current stick position in polar coordinates
		angle = NormalizeAngle (RAD2DEG (atan2 (lookRaw.y, lookRaw.x)) + 90.f);
		scale = IN_AxisMagnitude (lookRaw);

		// handle state transitions
		isactive = scale > joy_flick_deadzone.value;
		wasactive = pad->flick.prev_scale > joy_flick_deadzone.value;
		if (isactive != wasactive)
		{
			if (!wasactive) // start new flick
			{
				pad->flick.prev_lerp_frac = 0.f;
				pad->flick.yaw = angle;
				pad->flick.pitch = cl.viewangles[PITCH];
			}
		}
		else if (isactive) // continuous adjustments
		{
			delta = AngleDifference (angle, pad->flick.prev_angle);
			if (joy_flick_noise_thresh.value > 0.f) // filter small movements
			{
				float filter_scale = fabs (delta) / joy_flick_noise_thresh.value;
				if (filter_scale < 1.f)
				{
					filter_scale = LERP (0.05f, 1.f, filter_scale * filter_scale);
					delta *= filter_scale;
					angle = NormalizeAngle (pad->flick.prev_angle + delta);
				}
			}
			pad->flick.yaw_delta += delta;
		}

		// apply yaw adjustment
		if (joy_flick_adjust_speed.value > 0.f)
			delta = pad->flick.yaw_delta * q_min (1.0, host_rawframetime * joy_flick_adjust_speed.value);
		else
			delta = pad->flick.yaw_delta;
		if (fabs (delta) > 0.01)
		{
			cl.viewangles[YAW] -= delta;
			pad->flick.yaw_delta -= delta;
		}

		// advance angle animation
		if (joy_flick_time.value > 0.f)
		{
			lerp_frac = pad->flick.prev_lerp_frac + host_rawframetime / joy_flick_time.value;
			lerp_frac = CLAMP (0.f, lerp_frac, 1.f);
		}
		else
			lerp_frac = 1.f;

		delta = IN_FlickStickEasing (lerp_frac) - IN_FlickStickEasing (pad->flick.prev_lerp_frac);
		cl.viewangles[YAW] -= pad->flick.yaw * delta;
		cl.viewangles[PITCH] -= pad->flick.pitch * delta * CLAMP (0.f, joy_flick_recenter.value, 1.f);

		// update state
		pad->flick.prev_scale = scale;
		pad->flick.prev_angle = angle;
		pad->flick.prev_lerp_frac = lerp_frac;
	}
	else // traditional joystick look
	{
		float right, down;
		// QVR: flatscreen play's ironsight slows the stick as it does the mouse (vr_flataimsens.cpp); exactly 1 otherwise
		// QVR: splitscreen phase 9: a seat 2-4's own sensitivity and invert (its seat menu); seat 1's exactly 1 and none
		const float aim = VR_FlatAimStickScale (VR_FlatLookFov (r_refdef.basefov)) * VR_SeatLookScale ();
		const qboolean invert = (joy_invert.value != 0.f) != (VR_SeatInvertLook () != 0);

		IN_ResetFlickState (pad);

		right = lookEased.x * joy_sensitivity_yaw.value * host_rawframetime * aim;
		down = lookEased.y * joy_sensitivity_pitch.value * (invert ? -1.0 : 1.0) * host_rawframetime * aim;
		if (VR_FlatLookTurn (&right, &down)) // QVR: flatscreen play's view rolled: along the rolled screen (see IN_MouseMove)
		{
			cl.viewangles[YAW] -= right;
			cl.viewangles[PITCH] += down;
		}
		else
		{
			cl.viewangles[YAW] -= lookEased.x * joy_sensitivity_yaw.value * host_rawframetime * aim;
			cl.viewangles[PITCH] += lookEased.y * joy_sensitivity_pitch.value * (invert ? -1.0 : 1.0) * host_rawframetime * aim;
		}

		if (lookEased.x != 0 || lookEased.y != 0)
			V_StopPitchDrift();
	}

	/* johnfitz -- variable pitch clamping */
	if (cl.viewangles[PITCH] > cl_maxpitch.value)
		cl.viewangles[PITCH] = cl_maxpitch.value;
	if (cl.viewangles[PITCH] < cl_minpitch.value)
		cl.viewangles[PITCH] = cl_minpitch.value;
}

static float IN_RecenterEasing (float frac)
{
	return frac * frac;
}

void IN_GyroMove(usercmd_t *cmd)
{
	float scale, duration, lerp_frac;
	joypad_t *pad = IN_SeatPad (CL_SeatIndex ()); // QVR: splitscreen: the pad driving the seat moving
	if (!gyro_enable.value)
		return;

	if (!IN_JoyActive (pad))
		return;

	if (cl.paused || Key_SeatDest () != key_game) // QVR: splitscreen phase 9: the moving seat's (its seat menu open: no move)
		return;

	if (CL_InCutscene ())
		return;

	scale = (180.f / M_PI) * host_rawframetime * IN_FovScale ();
	switch ((int)gyro_mode.value)
	{
	case GYRO_BUTTON_DISABLES:
		if (Key_Seat ()->gyrobutton)
			return;
		break;
	case GYRO_BUTTON_ENABLES:
		if (!Key_Seat ()->gyrobutton)
			return;
		break;
	case GYRO_BUTTON_INVERTS_DIR:
		if (Key_Seat ()->gyrobutton)
			scale = -scale;
		break;
	default:
		break;
	}

	// apply gyro
	{
		float right = -(scale * pad->gyro_yaw * gyro_yawsensitivity.value);
		float down = -(scale * pad->gyro_pitch * gyro_pitchsensitivity.value);
		if (VR_FlatLookTurn (&right, &down)) // QVR: flatscreen play's view rolled: along the rolled screen (see IN_MouseMove)
		{
			cl.viewangles[YAW] -= right;
			cl.viewangles[PITCH] += down;
		}
		else
		{
			cl.viewangles[YAW] += scale * pad->gyro_yaw * gyro_yawsensitivity.value;
			cl.viewangles[PITCH] -= scale * pad->gyro_pitch * gyro_pitchsensitivity.value;
		}
	}

	// Default pitch drift code tries to move towards the ideal pitch
	// every frame, which means it is always fighting the player's input.
	// When gyro is active we disable the default behavior, and instead
	// we perform "additive" camera centering by storing the delta
	// between the ideal pitch and the current pitch when centerview is used,
	// and injecting that delta over some amount of time.

	// stop standard pitch drifting
	V_StopPitchDrift ();

	// store angle delta and reset centering anim if centerview was used this frame
	if (cl.lastcenterstart == cl.time)
	{
		pad->gyro_center_frac = 0.f;
		pad->gyro_center_amount = cl.idealpitch - cl.viewangles[PITCH];
	}

	// try to roughly mimic the look of the default pitch drift code
	if (pad->gyro_center_amount != 0.f && v_centerspeed.value > 0.f)
	{
		duration = fabs (pad->gyro_center_amount / v_centerspeed.value);
		lerp_frac = pad->gyro_center_frac + host_rawframetime / duration;
		lerp_frac = CLAMP (0.f, lerp_frac, 1.f);
	}
	else
		lerp_frac = 1.f;
	scale = IN_RecenterEasing (lerp_frac) - IN_RecenterEasing (pad->gyro_center_frac);
	pad->gyro_center_frac = lerp_frac;
	cl.viewangles[PITCH] += pad->gyro_center_amount * scale;

	/* johnfitz -- variable pitch clamping */
	if (cl.viewangles[PITCH] > cl_maxpitch.value)
		cl.viewangles[PITCH] = cl_maxpitch.value;
	if (cl.viewangles[PITCH] < cl_minpitch.value)
		cl.viewangles[PITCH] = cl_minpitch.value;
}

void IN_MouseMove(usercmd_t *cmd)
{
	float		dmx, dmy;
	float		sens;
	qboolean	mlook = (CL_SeatInput()->mlook.state & 1) || freelook.value;

	sens = sensitivity.value * IN_FovScale ();

	// QVR: splitscreen: the current seat's mouse motion (seat 0's: the mouse's; the others have none)
	dmx = Key_Seat ()->mouse_dx * sens;
	dmy = Key_Seat ()->mouse_dy * sens;

	Key_Seat ()->mouse_dx = 0;
	Key_Seat ()->mouse_dy = 0;

	if (mlook && !(CL_SeatInput()->strafe.state & 1) && !lookstrafe.value)
	{
		// QVR: flatscreen play's view rolled (a lean, a strafe, a hit): the mouse turns it along the rolled screen, straight
		// up the tilted screen for straight up (vr_flat_lean_mouse; vr/vr_flatmove.cpp). Not rolled: Quake's own, below.
		float right = m_yaw.value * dmx, down = m_pitch.value * dmy;
		if (VR_FlatLookTurn (&right, &down))
		{
			cl.viewangles[YAW] -= right;
			if (dmx || dmy)
				V_StopPitchDrift ();
			cl.viewangles[PITCH] += down;
			if (cl.viewangles[PITCH] > cl_maxpitch.value)
				cl.viewangles[PITCH] = cl_maxpitch.value;
			if (cl.viewangles[PITCH] < cl_minpitch.value)
				cl.viewangles[PITCH] = cl_minpitch.value;
			return;
		}
	}

	if ((CL_SeatInput()->strafe.state & 1) || (lookstrafe.value && mlook))
		cmd->sidemove += m_side.value * dmx;
	else
		cl.viewangles[YAW] -= m_yaw.value * dmx;

	if (mlook)
	{
		if (dmx || dmy)
			V_StopPitchDrift ();
	}

	if (mlook && !(CL_SeatInput()->strafe.state & 1))
	{
		cl.viewangles[PITCH] += m_pitch.value * dmy;
		/* johnfitz -- variable pitch clamping */
		if (cl.viewangles[PITCH] > cl_maxpitch.value)
			cl.viewangles[PITCH] = cl_maxpitch.value;
		if (cl.viewangles[PITCH] < cl_minpitch.value)
			cl.viewangles[PITCH] = cl_minpitch.value;
	}
	else
	{
		if ((CL_SeatInput()->strafe.state & 1) && noclip_anglehack)
			cmd->upmove -= m_forward.value * dmy;
		else
			cmd->forwardmove -= m_forward.value * dmy;
	}
}

void IN_Move(usercmd_t *cmd)
{
	IN_JoyMove(cmd);
	IN_GyroMove(cmd);
	IN_MouseMove(cmd);
}

void IN_ClearStates (void)
{
}

void IN_UpdateInputMode (void)
{
	textmode_t want_textmode = Key_TextEntry();
	if (textmode != want_textmode)
	{
		textmode = want_textmode;
		IN_UpdateSDLTextInput ();
	}
}

qboolean IN_EmulatedCharEvents (void)
{
	return textmode == TEXTMODE_NOPOPUP && !SDL_IsTextInputActive ();
}

keydevice_t IN_GetLastActiveDeviceType (void)
{
	return lastactivetype;
}

static inline int IN_SDL2_ScancodeToQuakeKey(SDL_Scancode scancode)
{
	switch (scancode)
	{
	case SDL_SCANCODE_TAB: return K_TAB;
	case SDL_SCANCODE_RETURN: return K_ENTER;
	case SDL_SCANCODE_RETURN2: return K_ENTER;
	case SDL_SCANCODE_ESCAPE: return K_ESCAPE;
	case SDL_SCANCODE_SPACE: return K_SPACE;

	case SDL_SCANCODE_A: return 'a';
	case SDL_SCANCODE_B: return 'b';
	case SDL_SCANCODE_C: return 'c';
	case SDL_SCANCODE_D: return 'd';
	case SDL_SCANCODE_E: return 'e';
	case SDL_SCANCODE_F: return 'f';
	case SDL_SCANCODE_G: return 'g';
	case SDL_SCANCODE_H: return 'h';
	case SDL_SCANCODE_I: return 'i';
	case SDL_SCANCODE_J: return 'j';
	case SDL_SCANCODE_K: return 'k';
	case SDL_SCANCODE_L: return 'l';
	case SDL_SCANCODE_M: return 'm';
	case SDL_SCANCODE_N: return 'n';
	case SDL_SCANCODE_O: return 'o';
	case SDL_SCANCODE_P: return 'p';
	case SDL_SCANCODE_Q: return 'q';
	case SDL_SCANCODE_R: return 'r';
	case SDL_SCANCODE_S: return 's';
	case SDL_SCANCODE_T: return 't';
	case SDL_SCANCODE_U: return 'u';
	case SDL_SCANCODE_V: return 'v';
	case SDL_SCANCODE_W: return 'w';
	case SDL_SCANCODE_X: return 'x';
	case SDL_SCANCODE_Y: return 'y';
	case SDL_SCANCODE_Z: return 'z';

	case SDL_SCANCODE_1: return '1';
	case SDL_SCANCODE_2: return '2';
	case SDL_SCANCODE_3: return '3';
	case SDL_SCANCODE_4: return '4';
	case SDL_SCANCODE_5: return '5';
	case SDL_SCANCODE_6: return '6';
	case SDL_SCANCODE_7: return '7';
	case SDL_SCANCODE_8: return '8';
	case SDL_SCANCODE_9: return '9';
	case SDL_SCANCODE_0: return '0';

	case SDL_SCANCODE_MINUS: return '-';
	case SDL_SCANCODE_EQUALS: return '=';
	case SDL_SCANCODE_LEFTBRACKET: return '[';
	case SDL_SCANCODE_RIGHTBRACKET: return ']';
	case SDL_SCANCODE_BACKSLASH: return '\\';
	case SDL_SCANCODE_NONUSHASH: return '#';
	case SDL_SCANCODE_SEMICOLON: return ';';
	case SDL_SCANCODE_APOSTROPHE: return '\'';
	case SDL_SCANCODE_GRAVE: return '`';
	case SDL_SCANCODE_COMMA: return ',';
	case SDL_SCANCODE_PERIOD: return '.';
	case SDL_SCANCODE_SLASH: return '/';
	case SDL_SCANCODE_NONUSBACKSLASH: return '\\';

	case SDL_SCANCODE_BACKSPACE: return K_BACKSPACE;
	case SDL_SCANCODE_UP: return K_UPARROW;
	case SDL_SCANCODE_DOWN: return K_DOWNARROW;
	case SDL_SCANCODE_LEFT: return K_LEFTARROW;
	case SDL_SCANCODE_RIGHT: return K_RIGHTARROW;

	case SDL_SCANCODE_LALT: return K_ALT;
	case SDL_SCANCODE_RALT: return K_ALT;
	case SDL_SCANCODE_LCTRL: return K_CTRL;
	case SDL_SCANCODE_RCTRL: return K_CTRL;
	case SDL_SCANCODE_LSHIFT: return K_SHIFT;
	case SDL_SCANCODE_RSHIFT: return K_SHIFT;

	case SDL_SCANCODE_F1: return K_F1;
	case SDL_SCANCODE_F2: return K_F2;
	case SDL_SCANCODE_F3: return K_F3;
	case SDL_SCANCODE_F4: return K_F4;
	case SDL_SCANCODE_F5: return K_F5;
	case SDL_SCANCODE_F6: return K_F6;
	case SDL_SCANCODE_F7: return K_F7;
	case SDL_SCANCODE_F8: return K_F8;
	case SDL_SCANCODE_F9: return K_F9;
	case SDL_SCANCODE_F10: return K_F10;
	case SDL_SCANCODE_F11: return K_F11;
	case SDL_SCANCODE_F12: return K_F12;
	case SDL_SCANCODE_INSERT: return K_INS;
	case SDL_SCANCODE_DELETE: return K_DEL;
	case SDL_SCANCODE_PAGEDOWN: return K_PGDN;
	case SDL_SCANCODE_PAGEUP: return K_PGUP;
	case SDL_SCANCODE_HOME: return K_HOME;
	case SDL_SCANCODE_END: return K_END;

	case SDL_SCANCODE_NUMLOCKCLEAR: return K_KP_NUMLOCK;
	case SDL_SCANCODE_KP_DIVIDE: return K_KP_SLASH;
	case SDL_SCANCODE_KP_MULTIPLY: return K_KP_STAR;
	case SDL_SCANCODE_KP_MINUS: return K_KP_MINUS;
	case SDL_SCANCODE_KP_7: return K_KP_HOME;
	case SDL_SCANCODE_KP_8: return K_KP_UPARROW;
	case SDL_SCANCODE_KP_9: return K_KP_PGUP;
	case SDL_SCANCODE_KP_PLUS: return K_KP_PLUS;
	case SDL_SCANCODE_KP_4: return K_KP_LEFTARROW;
	case SDL_SCANCODE_KP_5: return K_KP_5;
	case SDL_SCANCODE_KP_6: return K_KP_RIGHTARROW;
	case SDL_SCANCODE_KP_1: return K_KP_END;
	case SDL_SCANCODE_KP_2: return K_KP_DOWNARROW;
	case SDL_SCANCODE_KP_3: return K_KP_PGDN;
	case SDL_SCANCODE_KP_ENTER: return K_KP_ENTER;
	case SDL_SCANCODE_KP_0: return K_KP_INS;
	case SDL_SCANCODE_KP_PERIOD: return K_KP_DEL;

	case SDL_SCANCODE_LGUI: return K_COMMAND;
	case SDL_SCANCODE_RGUI: return K_COMMAND;

	case SDL_SCANCODE_CAPSLOCK: return K_CAPSLOCK;
	case SDL_SCANCODE_SCROLLLOCK: return K_SCROLLLOCK;

	case SDL_SCANCODE_PRINTSCREEN: return K_PRINTSCREEN;

	case SDL_SCANCODE_PAUSE: return K_PAUSE;

	default: return 0;
	}
}

static void IN_DebugTextEvent(SDL_Event *event)
{
	Con_Printf ("SDL_TEXTINPUT '%s' time: %g\n", event->text.text, Sys_DoubleTime());
}

static void IN_DebugKeyEvent(SDL_Event *event)
{
	const char *eventtype = (event->key.state == SDL_PRESSED) ? "SDL_KEYDOWN" : "SDL_KEYUP";
	Con_Printf ("%s scancode: '%s' keycode: '%s' time: %g\n",
		eventtype,
		SDL_GetScancodeName(event->key.keysym.scancode),
		SDL_GetKeyName(event->key.keysym.sym),
		Sys_DoubleTime());
}

void IN_StartGyroCalibration (void)
{
#if SDL_VERSION_ATLEAST (2, 0, 9)
	// Disable rumble temporarily
	if (joy_primary->has_rumble)
		SDL_GameControllerRumble (joy_primary->controller, 0, 0, 100);
#endif // SDL_VERSION_ATLEAST (2, 0, 9)

	gyro_accum[0] = 0.0;
	gyro_accum[1] = 0.0;
	gyro_accum[2] = 0.0;

	updates_countdown = GYRO_CALIBRATION_SAMPLES;

	// Note: we modify updates_countdown first, before printing the message to the console,
	// because Con_Printf triggers a redraw, which in turn calls M_Calibration_Draw,
	// which would see that updates_countown is 0 and consider that calibration must be done.
	// Alternatively, we could use Con_SafePrintf (which doesn't refresh the screen).

	Con_Printf ("Calibrating, please wait...\n");
}

static qboolean IN_UpdateGyroCalibration (const float newsample[3])
{
	if (!updates_countdown)
		return false;

	gyro_accum[0] += newsample[0];
	gyro_accum[1] += newsample[1];
	gyro_accum[2] += newsample[2];

	updates_countdown--;
	if (!updates_countdown)
	{
		const float inverseSamples = 1.f / GYRO_CALIBRATION_SAMPLES;
		Cvar_SetValue("gyro_calibration_x", gyro_accum[0] * inverseSamples);
		Cvar_SetValue("gyro_calibration_y", gyro_accum[1] * inverseSamples);
		Cvar_SetValue("gyro_calibration_z", gyro_accum[2] * inverseSamples);

		Con_Printf("Calibration results:\n X=%f Y=%f Z=%f\n",
			gyro_calibration_x.value,
			gyro_calibration_y.value,
			gyro_calibration_z.value);

		Con_Printf("Calibration finished\n");

		return false;
	}

	return true;
}

qboolean IN_HasGyro (void)
{
	return joy_primary->gyro_present;
}

float IN_GetRawGyroMagnitude (void)
{
	if (!joy_primary->gyro_present)
		return 0.f;
	return joy_primary->gyro_raw_mag;
}

float IN_GetRawLookMagnitude (void)
{
	joyaxis_t axis;
	if (!joy_primary->controller)
		return 0.f;

	axis.x = joy_primary->axisstate.axisvalue[joy_swapmovelook.value ? SDL_CONTROLLER_AXIS_LEFTX : SDL_CONTROLLER_AXIS_RIGHTX];
	axis.y = joy_primary->axisstate.axisvalue[joy_swapmovelook.value ? SDL_CONTROLLER_AXIS_LEFTY : SDL_CONTROLLER_AXIS_RIGHTY];

	return IN_AxisMagnitude (axis);
}

float IN_GetRawMoveMagnitude (void)
{
	joyaxis_t axis;
	if (!joy_primary->controller)
		return 0.f;

	axis.x = joy_primary->axisstate.axisvalue[joy_swapmovelook.value ? SDL_CONTROLLER_AXIS_RIGHTX : SDL_CONTROLLER_AXIS_LEFTX];
	axis.y = joy_primary->axisstate.axisvalue[joy_swapmovelook.value ? SDL_CONTROLLER_AXIS_RIGHTY : SDL_CONTROLLER_AXIS_LEFTY];

	return IN_AxisMagnitude (axis);
}

float IN_GetRawTriggerMagnitude (void)
{
	float left, right;
	if (!joy_primary->controller)
		return 0.f;

	left = fabs (joy_primary->axisstate.axisvalue[SDL_CONTROLLER_AXIS_TRIGGERLEFT]);
	right = fabs (joy_primary->axisstate.axisvalue[SDL_CONTROLLER_AXIS_TRIGGERRIGHT]);

	return q_max (left, right);

}

qboolean IN_IsCalibratingGyro (void)
{
	return updates_countdown != 0;
}

float IN_GetGyroCalibrationProgress (void)
{
	return (GYRO_CALIBRATION_SAMPLES - updates_countdown) / (float) GYRO_CALIBRATION_SAMPLES;
}

static float IN_FilterGyroSample (float prev, float cur)
{
	float thresh = DEG2RAD (gyro_noise_thresh.value);
	float d = fabs (cur - prev);
	if (d < thresh)
	{
		d /= thresh;
		cur = LERP (prev, cur, 0.01f + 0.99f * d * d);
	}
	return cur;
}

// QVR: vr_seat_test's keyboard: a real SDL key event for a Quake key (its scancode), handled as the keyboard's
void IN_MockKeyboardKey (int key, qboolean down)
{
	SDL_Event event;
	int sc;

	for (sc = 0; sc < SDL_NUM_SCANCODES && IN_SDL2_ScancodeToQuakeKey ((SDL_Scancode) sc) != key; sc++)
		;
	if (sc == SDL_NUM_SCANCODES)
		return;
	memset (&event, 0, sizeof (event));
	event.type = down ? SDL_KEYDOWN : SDL_KEYUP;
	event.key.state = down ? SDL_PRESSED : SDL_RELEASED;
	event.key.keysym.scancode = (SDL_Scancode) sc;
	SDL_PushEvent (&event);
	IN_SendKeyEvents ();
}

void IN_SendKeyEvents (void)
{
	SDL_Event event;
	int key;
	qboolean down;
	// QVR: splitscreen: the events here are the keyboard's and the mouse's (the pads are polled: IN_Commands), always
	// seat 0's: its keys, its binds (the device names its seat)
	int seat = CL_SeatSet (0);

	// QVR: in the headset the sounds play on without the window's focus (the player is in the headset, not at the
	// desktop): never blocked then, nor left blocked from a flat screen's lost focus as VR starts
	if (VR_IsActive () && !windowhasfocus)
		S_UnblockSound ();

	while (SDL_PollEvent(&event))
	{
		if (VR_ImGuiEvent (&event, (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) ? IN_SDL2_ScancodeToQuakeKey (event.key.keysym.scancode) : -1)) // QVR: a flat-screen tool's ImGui took it (the Rig Studio: vr/vr_imgui.cpp); the key, so a key bound to a tool's toggle still reaches its binding
			continue;
		switch (event.type)
		{
		case SDL_WINDOWEVENT:
			if (event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED)
			{
				Sys_ActivateKeyFilter(true);
				windowhasfocus = true;
				S_UnblockSound();
			}
			else if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST)
			{
				windowhasfocus = false;
				if (!VR_IsActive ()) // QVR: see above
					S_BlockSound();
				Sys_ActivateKeyFilter(false);
			}
			else if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
			{
				vid.width = event.window.data1;
				vid.height = event.window.data2;
				vid.resized = true;
			}
			break;
		case SDL_TEXTINPUT:
			lastactivetype = KD_KEYBOARD;

			if (in_debugkeys.value)
				IN_DebugTextEvent(&event);

		// SDL2: We use SDL_TEXTINPUT for typing in the console / chat.
		// SDL2 uses the local keyboard layout and handles modifiers
		// (shift for uppercase, etc.) for us.
			{
				unsigned char *ch;
				for (ch = (unsigned char *)event.text.text; *ch; ch++)
					if ((*ch & ~0x7F) == 0)
						Char_Event (*ch);
			}
			break;
		case SDL_KEYDOWN:
		case SDL_KEYUP:
			down = (event.key.state == SDL_PRESSED);

			lastactivetype = KD_KEYBOARD;

			if (in_debugkeys.value)
				IN_DebugKeyEvent(&event);

		// SDL2: we interpret the keyboard as the US layout, so keybindings
		// are based on key position, not the label on the key cap.
			key = IN_SDL2_ScancodeToQuakeKey(event.key.keysym.scancode);

		// also pass along the underlying keycode using the proper current layout for Y/N prompts.
			Key_EventWithKeycode (key, down, event.key.keysym.sym);
			break;

		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:
			if (event.button.button < 1 ||
			    event.button.button > Q_COUNTOF(buttonremap))
			{
				Con_Printf ("Ignored event for mouse button %d\n",
							event.button.button);
				break;
			}
			lastactivetype = KD_MOUSE;
			if (key_dest == key_menu)
				M_Mousemove (event.button.x, event.button.y);
			else if (key_dest == key_console)
				Con_Mousemove (event.button.x, event.button.y);
			Key_Event(buttonremap[event.button.button - 1], event.button.state == SDL_PRESSED);
			break;

		case SDL_MOUSEWHEEL:
			lastactivetype = KD_MOUSE;
			if (event.wheel.y > 0)
			{
				Key_Event(K_MWHEELUP, true);
				Key_Event(K_MWHEELUP, false);
			}
			else if (event.wheel.y < 0)
			{
				Key_Event(K_MWHEELDOWN, true);
				Key_Event(K_MWHEELDOWN, false);
			}
			break;

		case SDL_MOUSEMOTION:
			IN_MouseMotion(event.motion.xrel, event.motion.yrel);
			break;

#if SDL_VERSION_ATLEAST(2, 0, 14)
		case SDL_CONTROLLERSENSORUPDATE:
			if (event.csensor.sensor == SDL_SENSOR_GYRO && IN_PadForInstance (event.csensor.which)) // QVR: any pad's
			{
				joypad_t *pad = IN_PadForInstance (event.csensor.which);
				float prev_yaw = pad->gyro_yaw;
				float prev_pitch = pad->gyro_pitch;

				if (pad == joy_primary && IN_UpdateGyroCalibration (event.csensor.data)) // QVR: the calibration: pad 0's
					break;

				if (!gyro_turning_axis.value)
					pad->gyro_yaw = event.csensor.data[1] - gyro_calibration_y.value; // yaw
				else
					pad->gyro_yaw = -(event.csensor.data[2] - gyro_calibration_z.value); // roll
				pad->gyro_pitch = event.csensor.data[0] - gyro_calibration_x.value;

				// Save unfiltered magnitude to display in the UI
				pad->gyro_raw_mag = RAD2DEG (sqrt (pad->gyro_yaw*pad->gyro_yaw + pad->gyro_pitch*pad->gyro_pitch));

				pad->gyro_yaw = IN_FilterGyroSample (prev_yaw, pad->gyro_yaw);
				pad->gyro_pitch = IN_FilterGyroSample (prev_pitch, pad->gyro_pitch);
			}
			break;

#endif // SDL_VERSION_ATLEAST(2, 0, 14)

		case SDL_CONTROLLERDEVICEADDED:
			if (!IN_RemapJoystick ())
				IN_UseController (event.jdevice.which);
			IN_RefreshExtraPads (); // QVR: splitscreen: or opened as another pad
			break;
		case SDL_CONTROLLERDEVICEREMOVED:
		case SDL_CONTROLLERDEVICEREMAPPED:
			if (!IN_RemapJoystick ())
				IN_SetupJoystick ();
			IN_RefreshExtraPads (); // QVR: splitscreen: the other pads' gone ones closed
			break;
#if SDL_VERSION_ATLEAST (2, 0, 14)
		case SDL_LOCALECHANGED:
			if (!q_strcasecmp (language.string, "auto"))
				language.callback (&language);
			break;
#endif

		case SDL_QUIT:
			CL_Disconnect ();
			Sys_Quit ();
			break;

		default:
			break;
		}
	}
	CL_SeatSet (seat); // QVR
}

