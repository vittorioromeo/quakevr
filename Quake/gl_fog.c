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
//gl_fog.c -- global and volumetric fog

#include "quakedef.h"

//==============================================================================
//
//  GLOBAL FOG
//
//==============================================================================

#define DEFAULT_DENSITY 0.0
#define DEFAULT_GRAY 0.3

static float fog_density;
static float fog_red;
static float fog_green;
static float fog_blue;

static float old_density;
static float old_red;
static float old_green;
static float old_blue;

static float fade_time; //duration of fade
static float fade_done; //time when fade will be done

extern float skyfog;

/*
=============
Fog_FadeLeft

QVR: the share of the old fog still showing (0..1), or 0 with no fade in progress. A fog set at once
(svc_fog or "fog" with time 0: Arcane Dimensions sends its fog again and again) leaves fade_time 0 and
fade_done = cl.time, a float rounded from the double clock, so about half the time a few microseconds
past it: fade_done > cl.time until the clock moved on, and (fade_done - cl.time) / 0 an infinite share,
the fog's colour huge and its density NaN in the frames drawn before then: the world drawn black and the
sky dimmed for a frame each time AD sent its fog (round 22, "AD flicker").
=============
*/
static float Fog_FadeLeft (void)
{
	float f;
	if (fade_time <= 0.f || fade_done <= cl.time)
		return 0.f;
	f = (fade_done - cl.time) / fade_time;
	return CLAMP (0.f, f, 1.f);
}

/*
=============
Fog_Update

update internal variables
=============
*/
void Fog_Update (float density, float red, float green, float blue, float time)
{
	//save previous settings for fade
	if (time > 0)
	{
		//check for a fade in progress
		if (Fog_FadeLeft () > 0.f) // QVR: was fade_done > cl.time (a division by a zero fade_time)
		{
			float f;

			f = Fog_FadeLeft ();
			old_density = f * old_density + (1.0 - f) * fog_density;
			old_red = f * old_red + (1.0 - f) * fog_red;
			old_green = f * old_green + (1.0 - f) * fog_green;
			old_blue = f * old_blue + (1.0 - f) * fog_blue;
		}
		else
		{
			old_density = fog_density;
			old_red = fog_red;
			old_green = fog_green;
			old_blue = fog_blue;
		}
	}

	fog_density = density;
	fog_red = red;
	fog_green = green;
	fog_blue = blue;
	fade_time = time;
	fade_done = cl.time + time;}

/*
=============
Fog_ParseServerMessage

handle an SVC_FOG message from server
=============
*/
void Fog_ParseServerMessage (void)
{
	float density, red, green, blue, time;

	density = MSG_ReadByte() / 255.0;
	red = MSG_ReadByte() / 255.0;
	green = MSG_ReadByte() / 255.0;
	blue = MSG_ReadByte() / 255.0;
	time = MSG_ReadShort() / 100.0;
	if (time < 0.0f) time = 0.0f;

	Fog_Update (density, red, green, blue, time);
}

/*
=============
Fog_FogCommand_f

handle the 'fog' console command
=============
*/
void Fog_FogCommand_f (void)
{
	float d, r, g, b, t;

	switch (Cmd_Argc())
	{
	default:
	case 1:
		Con_Printf("usage:\n");
		Con_Printf("   fog <density>\n");
		Con_Printf("   fog <red> <green> <blue>\n");
		Con_Printf("   fog <density> <red> <green> <blue>\n");
		Con_Printf("current values:\n");
		Con_Printf("   \"density\" is \"%g\"\n", fog_density);
		Con_Printf("   \"red\"     is \"%g\"\n", fog_red);
		Con_Printf("   \"green\"   is \"%g\"\n", fog_green);
		Con_Printf("   \"blue\"    is \"%g\"\n", fog_blue);
		return;
	case 2:
		d = Q_atof(Cmd_Argv(1));
		t = 0.0f;
		r = fog_red;
		g = fog_green;
		b = fog_blue;
		break;
	case 3: //TEST
		d = Q_atof(Cmd_Argv(1));
		t = Q_atof(Cmd_Argv(2));
		r = fog_red;
		g = fog_green;
		b = fog_blue;
		break;
	case 4:
		d = fog_density;
		t = 0.0f;
		r = Q_atof(Cmd_Argv(1));
		g = Q_atof(Cmd_Argv(2));
		b = Q_atof(Cmd_Argv(3));
		break;
	case 5:
		d = Q_atof(Cmd_Argv(1));
		r = Q_atof(Cmd_Argv(2));
		g = Q_atof(Cmd_Argv(3));
		b = Q_atof(Cmd_Argv(4));
		t = 0.0f;
		break;
	case 6: //TEST
		d = Q_atof(Cmd_Argv(1));
		r = Q_atof(Cmd_Argv(2));
		g = Q_atof(Cmd_Argv(3));
		b = Q_atof(Cmd_Argv(4));
		t = Q_atof(Cmd_Argv(5));
		break;
	}

	if      (d < 0.0f) d = 0.0f;
	if      (r < 0.0f) r = 0.0f;
	else if (r > 1.0f) r = 1.0f;
	if      (g < 0.0f) g = 0.0f;
	else if (g > 1.0f) g = 1.0f;
	if      (b < 0.0f) b = 0.0f;
	else if (b > 1.0f) b = 1.0f;
	Fog_Update(d, r, g, b, t);
}

/*
=============
Fog_ParseWorldspawn

called at map load
=============
*/
void Fog_ParseWorldspawn (void)
{
	char key[128], value[4096];
	const char *data;

	//initially no fog
	fog_density = DEFAULT_DENSITY;
	fog_red = DEFAULT_GRAY;
	fog_green = DEFAULT_GRAY;
	fog_blue = DEFAULT_GRAY;

	old_density = DEFAULT_DENSITY;
	old_red = DEFAULT_GRAY;
	old_green = DEFAULT_GRAY;
	old_blue = DEFAULT_GRAY;

	fade_time = 0.0;
	fade_done = 0.0;

	data = COM_Parse(cl.worldmodel->entities);
	if (!data)
		return; // error
	if (com_token[0] != '{')
		return; // error
	while (1)
	{
		data = COM_Parse(data);
		if (!data)
			return; // error
		if (com_token[0] == '}')
			break; // end of worldspawn
		if (com_token[0] == '_')
			q_strlcpy(key, com_token + 1, sizeof(key));
		else
			q_strlcpy(key, com_token, sizeof(key));
		while (key[0] && key[strlen(key)-1] == ' ') // remove trailing spaces
			key[strlen(key)-1] = 0;
		data = COM_ParseEx(data, CPE_ALLOWTRUNC);
		if (!data)
			return; // error
		q_strlcpy(value, com_token, sizeof(value));

		if (!strcmp("fog", key))
		{
			sscanf(value, "%f %f %f %f", &fog_density, &fog_red, &fog_green, &fog_blue);
		}
	}
}

/*
=============
Fog_GetColor

calculates fog color for this frame, taking into account fade times
=============
*/
float *Fog_GetColor (void)
{
	static float c[4];
	float f;
	int i;

	if (Fog_FadeLeft () > 0.f) // QVR: was fade_done > cl.time (a division by a zero fade_time: NaN fog)
	{
		f = Fog_FadeLeft ();
		c[0] = f * old_red + (1.0 - f) * fog_red;
		c[1] = f * old_green + (1.0 - f) * fog_green;
		c[2] = f * old_blue + (1.0 - f) * fog_blue;
		c[3] = 1.0;
	}
	else
	{
		c[0] = fog_red;
		c[1] = fog_green;
		c[2] = fog_blue;
		c[3] = 1.0;
	}

	for (i = 0; i < 3; i++) {
		c[i] = CLAMP (0.f, c[i], 1.f);
	}

	//find closest 24-bit RGB value, so solid-colored sky can match the fog perfectly
	for (i = 0; i < 3; i++) {
		c[i] = (float)(Q_rint(c[i] * 255)) / 255.0f;
	}

	return c;
}

/*
=============
Fog_GetDensity

returns current density of fog
=============
*/
float Fog_GetDensity (void)
{
	float f;

	if (Fog_FadeLeft () > 0.f) // QVR: was fade_done > cl.time (a division by a zero fade_time: NaN fog)
	{
		f = Fog_FadeLeft ();
		return f * old_density + (1.0 - f) * fog_density;
	}
	else
		return fog_density;
}

/*
=============
Fog_SetupFrame

called at the beginning of each frame
=============
*/
void Fog_SetupFrame (void)
{
	const float ExpAdjustment = 1.20112241f; // sqrt(log2(e))
	const float SphericalCorrection = 0.85f; // compensate higher perceived density with spherical fog
	const float DensityScale = ExpAdjustment * SphericalCorrection / 64.0f;
	float density = Fog_GetDensity() * DensityScale;
	memcpy(r_framedata.fogdata, Fog_GetColor(), 3 * sizeof(float));
	memcpy(r_framedata.skyfogdata, r_framedata.fogdata, 3 * sizeof(float));
	r_framedata.fogdata[3] = density * density;
	r_framedata.skyfogdata[3] = density > 0.f ? CLAMP (0.f, VR_AD_SkyFog (skyfog), 1.f) : 0.f; // QVR: AD support: the sky fog AD set (vr/vr_ad.cpp)
	VR_WaterFog (r_framedata.fogdata, r_framedata.skyfogdata); // QVR: an eye's in a liquid (vr/vr_water.cpp)
}

/*
=============
Fog_EnableGFog

called before drawing stuff that should be fogged
=============
*/
void Fog_EnableGFog (void)
{
	Fog_SetupFrame();
}

/*
=============
Fog_DisableGFog

called after drawing stuff that should be fogged
=============
*/
void Fog_DisableGFog (void)
{
	memset(r_framedata.fogdata, 0, sizeof(r_framedata.fogdata));
}

//==============================================================================
//
//  VOLUMETRIC FOG
//
//==============================================================================

cvar_t r_vfog = {"r_vfog", "1", CVAR_NONE};

void Fog_DrawVFog (void){}
void Fog_MarkModels (void){}

//==============================================================================
//
//  INIT
//
//==============================================================================

/*
=============
Fog_NewMap

called whenever a map is loaded
=============
*/
void Fog_NewMap (void)
{
	Fog_ParseWorldspawn (); //for global fog
	Fog_MarkModels (); //for volumetric fog
}

/*
=============
Fog_Init

called when quake initializes
=============
*/
/*
=============
Fog_Check_f

QVR: vr_fog_check: the fog set at once (time 0) at a thousand client clock values, and a fade with the
clock a little before its start: the fog drawn must be the one set (finite, never NaN) and a fade's share
stay within 0..1. Prints "fogcheck: N passed, M failed (K clocks rounded up)" (round 22, "AD flicker").
The fog and the clock are put back.
=============
*/
static void Fog_Check_f (void)
{
	const float sd = fog_density, sr = fog_red, sg = fog_green, sb = fog_blue;
	const float od = old_density, or_ = old_red, og = old_green, ob = old_blue, ft = fade_time, fd = fade_done;
	const double st = cl.time;
	int i, passed = 0, failed = 0, roundedUp = 0;
	float d, *c;

	for (i = 0; i < 1000; i++)
	{
		cl.time = 61.0 + i * 0.0137;
		Fog_Update (0.04f, 0.37f, 0.36f, 0.43f, 0.f);
		roundedUp += fade_done > cl.time;
		d = Fog_GetDensity ();
		c = Fog_GetColor ();
		if (d == 0.04f && fabsf (c[0] - 0.37f) < 0.01f && fabsf (c[2] - 0.43f) < 0.01f)
			passed++;
		else if (failed++ < 5)
			Con_Printf ("fogcheck: FAIL at cl.time %.6f: density %g colour %g %g %g\n", cl.time, d, c[0], c[1], c[2]);
	}
	// a fade over 1 s from that fog to a thicker one, the clock then 0.1 s before the fade began
	cl.time = 70.0;
	Fog_Update (0.04f, 0.37f, 0.36f, 0.43f, 0.f);
	cl.time = 71.0;
	Fog_Update (0.2f, 0.37f, 0.36f, 0.43f, 1.f);
	cl.time = 70.9;
	d = Fog_GetDensity ();
	if (d >= 0.04f - 1e-6f && d <= 0.04f + 1e-6f)
		passed++;
	else
	{
		failed++;
		Con_Printf ("fogcheck: FAIL the clock before a fade: density %g (want 0.04)\n", d);
	}
	cl.time = 71.5;
	d = Fog_GetDensity ();
	if (d > 0.11f && d < 0.13f)
		passed++;
	else
	{
		failed++;
		Con_Printf ("fogcheck: FAIL half way through a fade: density %g (want 0.12)\n", d);
	}

	fog_density = sd; fog_red = sr; fog_green = sg; fog_blue = sb;
	old_density = od; old_red = or_; old_green = og; old_blue = ob; fade_time = ft; fade_done = fd;
	cl.time = st;
	Con_Printf ("fogcheck: %d passed, %d failed (%d of 1000 clocks rounded up past the fog's time)\n", passed, failed, roundedUp);
}

void Fog_Init (void)
{
	Cmd_AddCommand ("fog",Fog_FogCommand_f);
	Cmd_AddCommand ("vr_fog_check", Fog_Check_f); // QVR

	//Cvar_RegisterVariable (&r_vfog);

	//set up global fog
	fog_density = DEFAULT_DENSITY;
	fog_red = DEFAULT_GRAY;
	fog_green = DEFAULT_GRAY;
	fog_blue = DEFAULT_GRAY;
}
