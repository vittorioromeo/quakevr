/*
Copyright (C) 1996-2001 Id Software, Inc.
Copyright (C) 2002-2009 John Fitzgibbons and others
Copyright (C) 2010-2014 QuakeSpasm developers
Copyright (C) 2016      Spike

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
#include "q_ctype.h"

#define	STRINGTEMP_BUFFERS		1024
#define	STRINGTEMP_LENGTH		1024
static	char	pr_string_temp[STRINGTEMP_BUFFERS][STRINGTEMP_LENGTH];
static	unsigned int	pr_string_tempindex = 0; // QVR: was a byte: only 256 of the 1024 buffers were ever used

static char *PR_GetTempString (void)
{
	return pr_string_temp[(STRINGTEMP_BUFFERS-1) & ++pr_string_tempindex];
}

int PR_MakeTempString (const char *val)
{
	char *tmp = PR_GetTempString();
	q_strlcpy(tmp, val, STRINGTEMP_LENGTH);
	return PR_SetEngineString(tmp);
}

#define	RETURN_EDICT(e) (((int *)qcvm->globals)[OFS_RETURN] = EDICT_TO_PROG(e))

#define	MSG_BROADCAST	0		// unreliable to all
#define	MSG_ONE		1		// reliable to one (msg_entity)
#define	MSG_ALL		2		// reliable to all
#define	MSG_INIT	3		// write to the init string

/*
===============================================================================

	BUILT-IN FUNCTIONS

===============================================================================
*/

static const char* PF_GetStringArg(int idx, void* userdata)
{
	if (userdata)
		idx += *(int*)userdata;
	if (idx < 0 || idx >= qcvm->argc)
		return "";
	return LOC_GetString(G_STRING(OFS_PARM0 + idx * 3));
}

static char *PF_VarString (int	first)
{
	int		i;
	static char out[1024];
	const char *format;
	size_t s;

	out[0] = 0;
	s = 0;

	if (first >= qcvm->argc)
		return out;

	format = LOC_GetString(G_STRING((OFS_PARM0 + first * 3)));
	if (LOC_HasPlaceholders(format))
	{
		int offset = first + 1;
		s = LOC_Format(format, PF_GetStringArg, &offset, out, sizeof(out));
	}
	else
	{
		for (i = first; i < qcvm->argc; i++)
		{
			s = q_strlcat(out, LOC_GetString(G_STRING(OFS_PARM0+i*3)), sizeof(out));
			if (s >= sizeof(out))
			{
				Con_Warning("PF_VarString: overflow (string truncated)\n");
				return out;
			}
		}
	}
	if (s > 255)
	{
		if (!dev_overflows.varstring || dev_overflows.varstring + CONSOLE_RESPAM_TIME < realtime)
		{
			Con_DWarning("PF_VarString: %i characters exceeds standard limit of 255 (max = %d).\n",
								(int) s, (int)(sizeof(out) - 1));
			dev_overflows.varstring = realtime;
		}
	}
	return out;
}


/*
=================
PF_error

This is a TERMINAL error, which will kill off the entire server.
Dumps self.

error(value)
=================
*/
static void PF_error (void)
{
	char	*s;
	edict_t	*ed;

	s = PF_VarString(0);
	Con_Printf ("======SERVER ERROR in %s:\n%s\n",
			PR_GetString(qcvm->xfunction->s_name), s);
	ed = PROG_TO_EDICT(pr_global_struct->self);
	ED_Print (ed);

	Host_Error ("Program error");
}

/*
=================
PF_objerror

Dumps out self, then an error message.  The program is aborted and self is
removed, but the level can continue.

objerror(value)
=================
*/
static void PF_objerror (void)
{
	char	*s;
	edict_t	*ed;

	s = PF_VarString(0);
	Con_Printf ("======OBJECT ERROR in %s:\n%s\n",
			PR_GetString(qcvm->xfunction->s_name), s);
	ed = PROG_TO_EDICT(pr_global_struct->self);
	ED_Print (ed);
	ED_Free (ed);

	//Host_Error ("Program error"); //johnfitz -- by design, this should not be fatal
}



/*
==============
PF_makevectors

Writes new values for v_forward, v_up, and v_right based on angles
makevectors(vector)
==============
*/
static void PF_makevectors (void)
{
	AngleVectors (G_VECTOR(OFS_PARM0), pr_global_struct->v_forward, pr_global_struct->v_right, pr_global_struct->v_up);
}

/*
=================
PF_setorigin

This is the only valid way to move an object without using the physics
of the world (setting velocity and waiting).  Directly changing origin
will not set internal links correctly, so clipping would be messed up.

This should be called when an object is spawned, and then only if it is
teleported.

setorigin (entity, origin)
=================
*/
static void PF_setorigin (void)
{
	edict_t	*e;
	float	*org;

	e = G_EDICT(OFS_PARM0);
	org = G_VECTOR(OFS_PARM1);
	VectorCopy (org, e->v.origin);
	SV_LinkEdict (e, false);
}


static void SetMinMaxSize (edict_t *e, float *minvec, float *maxvec, qboolean rotate)
{
	float	*angles;
	vec3_t	rmin, rmax;
	float	bounds[2][3];
	float	xvector[2], yvector[2];
	float	a;
	vec3_t	base, transformed;
	int		i, j, k, l;

	for (i = 0; i < 3; i++)
		if (minvec[i] > maxvec[i])
			PR_RunError ("backwards mins/maxs");

	rotate = false;		// FIXME: implement rotation properly again

	if (!rotate)
	{
		VectorCopy (minvec, rmin);
		VectorCopy (maxvec, rmax);
	}
	else
	{
	// find min / max for rotations
		angles = e->v.angles;

		a = angles[1]/180 * M_PI;

		xvector[0] = cos(a);
		xvector[1] = sin(a);
		yvector[0] = -sin(a);
		yvector[1] = cos(a);

		VectorCopy (minvec, bounds[0]);
		VectorCopy (maxvec, bounds[1]);

		rmin[0] = rmin[1] = rmin[2] = FLT_MAX;
		rmax[0] = rmax[1] = rmax[2] = -FLT_MAX;

		for (i = 0; i <= 1; i++)
		{
			base[0] = bounds[i][0];
			for (j = 0; j <= 1; j++)
			{
				base[1] = bounds[j][1];
				for (k = 0; k <= 1; k++)
				{
					base[2] = bounds[k][2];

				// transform the point
					transformed[0] = xvector[0]*base[0] + yvector[0]*base[1];
					transformed[1] = xvector[1]*base[0] + yvector[1]*base[1];
					transformed[2] = base[2];

					for (l = 0; l < 3; l++)
					{
						if (transformed[l] < rmin[l])
							rmin[l] = transformed[l];
						if (transformed[l] > rmax[l])
							rmax[l] = transformed[l];
					}
				}
			}
		}
	}

// set derived values
	VectorCopy (rmin, e->v.mins);
	VectorCopy (rmax, e->v.maxs);
	VectorSubtract (maxvec, minvec, e->v.size);

	SV_LinkEdict (e, false);
}

/*
=================
PF_setsize

the size box is rotated by the current angle

setsize (entity, minvector, maxvector)
=================
*/
static void PF_setsize (void)
{
	edict_t	*e;
	float	*minvec, *maxvec;

	e = G_EDICT(OFS_PARM0);
	minvec = G_VECTOR(OFS_PARM1);
	maxvec = G_VECTOR(OFS_PARM2);
	SetMinMaxSize (e, minvec, maxvec, false);
}


/*
=================
PF_setmodel

setmodel(entity, model)
=================
*/
static void PF_setmodel (void)
{
	int		i;
	const char	*m, **check;
	qmodel_t	*mod;
	edict_t		*e;

	e = G_EDICT(OFS_PARM0);
	m = G_STRING(OFS_PARM1);

// check to see if model was properly precached
	for (i = 0, check = sv.model_precache; *check; i++, check++)
	{
		if (!strcmp(*check, m))
			break;
	}

	if (!*check)
	{
		i = VR_LatePrecacheModel (m); // QVR
		if (i < 0)
			PR_RunError ("no precache: %s", m);
		check = &sv.model_precache[i]; // QVR
	}
	e->v.model = PR_SetEngineString(*check);
	e->v.modelindex = i; //SV_ModelIndex (m);

	mod = sv.models[ (int)e->v.modelindex];  // Mod_ForName (m, true);

	if (mod)
	//johnfitz -- correct physics cullboxes for bmodels
	{
		if (mod->type == mod_brush)
			SetMinMaxSize (e, mod->clipmins, mod->clipmaxs, true);
		else
			SetMinMaxSize (e, mod->mins, mod->maxs, true);
	}
	//johnfitz
	else
		SetMinMaxSize (e, vec3_origin, vec3_origin, true);
}

/*
=================
PF_bprint

broadcast print to everyone on server

bprint(value)
=================
*/
static void PF_bprint (void)
{
	char		*s;

	s = PF_VarString(0);
	SV_BroadcastPrintf ("%s", s);
}

/*
=================
PF_sprint

single print to a specific client

sprint(clientent, value)
=================
*/
static void PF_sprint (void)
{
	char		*s;
	client_t	*client;
	int	entnum;

	entnum = G_EDICTNUM(OFS_PARM0);
	s = PF_VarString(1);

	if (entnum < 1 || entnum > svs.maxclients)
	{
		Con_Printf ("tried to sprint to a non-client\n");
		return;
	}

	client = &svs.clients[entnum-1];

	MSG_WriteChar (&client->message,svc_print);
	MSG_WriteString (&client->message, s );
}


/*
=================
PF_centerprint

single print to a specific client

centerprint(clientent, value)
=================
*/
static void PF_centerprint (void)
{
	char		*s;
	client_t	*client;
	int	entnum;

	entnum = G_EDICTNUM(OFS_PARM0);
	s = PF_VarString(1);

	if (entnum < 1 || entnum > svs.maxclients)
	{
		Con_Printf ("tried to sprint to a non-client\n");
		return;
	}

	client = &svs.clients[entnum-1];

	MSG_WriteChar (&client->message,svc_centerprint);
	MSG_WriteString (&client->message, s);
}


/*
=================
PF_normalize

vector normalize(vector)
=================
*/
static void PF_normalize (void)
{
	float	*value1;
	vec3_t	newvalue;
	double	new_temp;

	value1 = G_VECTOR(OFS_PARM0);

	new_temp = (double)value1[0] * value1[0] + (double)value1[1] * value1[1] + (double)value1[2]*value1[2];
	new_temp = sqrt (new_temp);

	if (new_temp == 0)
		newvalue[0] = newvalue[1] = newvalue[2] = 0;
	else
	{
		new_temp = 1 / new_temp;
		newvalue[0] = value1[0] * new_temp;
		newvalue[1] = value1[1] * new_temp;
		newvalue[2] = value1[2] * new_temp;
	}

	VectorCopy (newvalue, G_VECTOR(OFS_RETURN));
}

/*
=================
PF_vlen

scalar vlen(vector)
=================
*/
static void PF_vlen (void)
{
	float	*value1;
	double	new_temp;

	value1 = G_VECTOR(OFS_PARM0);

	new_temp = (double)value1[0] * value1[0] + (double)value1[1] * value1[1] + (double)value1[2]*value1[2];
	new_temp = sqrt(new_temp);

	G_FLOAT(OFS_RETURN) = new_temp;
}

/*
=================
PF_vectoyaw

float vectoyaw(vector)
=================
*/
static void PF_vectoyaw (void)
{
	float	*value1;
	float	yaw;

	value1 = G_VECTOR(OFS_PARM0);

	if (value1[1] == 0 && value1[0] == 0)
		yaw = 0;
	else
	{
		yaw = (int) (atan2(value1[1], value1[0]) * 180 / M_PI);
		if (yaw < 0)
			yaw += 360;
	}

	G_FLOAT(OFS_RETURN) = yaw;
}


/*
=================
PF_vectoangles

vector vectoangles(vector)
=================
*/
static void PF_vectoangles (void)
{
	float	*value1;
	float	forward;
	float	yaw, pitch;

	value1 = G_VECTOR(OFS_PARM0);

	if (value1[1] == 0 && value1[0] == 0)
	{
		yaw = 0;
		if (value1[2] > 0)
			pitch = 90;
		else
			pitch = 270;
	}
	else
	{
		yaw = (int) (atan2(value1[1], value1[0]) * 180 / M_PI);
		if (yaw < 0)
			yaw += 360;

		forward = sqrt (value1[0]*value1[0] + value1[1]*value1[1]);
		pitch = (int) (atan2(value1[2], forward) * 180 / M_PI);
		if (pitch < 0)
			pitch += 360;
	}

	G_FLOAT(OFS_RETURN+0) = pitch;
	G_FLOAT(OFS_RETURN+1) = yaw;
	G_FLOAT(OFS_RETURN+2) = 0;
}

/*
=================
PF_Random

Returns a number from 0 < num < 1

random()
=================
*/
cvar_t sv_gameplayfix_random = {"sv_gameplayfix_random", "1", CVAR_ARCHIVE};
cvar_t sv_random_seed = {"sv_random_seed", "0", CVAR_NONE}; // QVR: the server's random numbers' seed at each map load (tests: the same AI every run; 0: the clock's; vr_srvrandom.cpp)
float PR_RandomNumber (void) // QVR: random()'s number, for the engine's natives of QuakeC too (vr_propthink.cpp)
{
	float		num;
	int		r = VR_ProbeRandom (); // QVR: the probes' own numbers (the server's stream left as it was)

	if (r < 0)
		r = VR_ServerRandom (); // QVR: the server's own stream, not the C library's rand() the client's effects share
	if (sv_gameplayfix_random.value)
		num = ((r & 0x7fff) + 0.5f) * (1.f / 0x8000);
	else
		num = (r & 0x7fff) / ((float)0x7fff);

	return num;
}

static void PF_random (void)
{
	G_FLOAT(OFS_RETURN) = PR_RandomNumber (); // QVR: the number in PR_RandomNumber
}

/*
=================
PF_particle

particle(origin, color, count)
=================
*/
static void PF_particle (void)
{
	float		*org, *dir;
	float		color;
	float		count;

	org = G_VECTOR(OFS_PARM0);
	dir = G_VECTOR(OFS_PARM1);
	color = G_FLOAT(OFS_PARM2);
	count = G_FLOAT(OFS_PARM3);
	SV_StartParticle (org, dir, color, count);
}


/*
=================
PF_ambientsound

=================
*/
static void PF_ambientsound (void)
{
	const char	*samp, **check;
	float		*pos;
	float		vol, attenuation;
	int		i, soundnum;
	int		large = false; //johnfitz -- PROTOCOL_FITZQUAKE

	pos = G_VECTOR (OFS_PARM0);
	samp = G_STRING(OFS_PARM1);
	vol = G_FLOAT(OFS_PARM2);
	attenuation = G_FLOAT(OFS_PARM3);

// check to see if samp was properly precached
	for (soundnum = 0, check = sv.sound_precache; *check; check++, soundnum++)
	{
		if (!strcmp(*check, samp))
			break;
	}

	if (!*check)
	{
		Con_Printf ("no precache: %s\n", samp);
		return;
	}

	//johnfitz -- PROTOCOL_FITZQUAKE
	if (soundnum > 255)
	{
		if (sv.protocol == PROTOCOL_NETQUAKE)
			return; //don't send any info protocol can't support
		else
			large = true;
	}
	//johnfitz

	SV_ReserveSignonSpace (17);

// add an svc_spawnambient command to the level signon packet

	//johnfitz -- PROTOCOL_FITZQUAKE
	if (large)
		MSG_WriteByte (sv.signon,svc_spawnstaticsound2);
	else
		MSG_WriteByte (sv.signon,svc_spawnstaticsound);
	//johnfitz

	for (i = 0; i < 3; i++)
		MSG_WriteCoord(sv.signon, pos[i], sv.protocolflags);

	//johnfitz -- PROTOCOL_FITZQUAKE
	if (large)
		MSG_WriteShort(sv.signon, soundnum);
	else
		MSG_WriteByte (sv.signon, soundnum);
	//johnfitz

	MSG_WriteByte (sv.signon, vol*255);
	MSG_WriteByte (sv.signon, attenuation*64);

}

/*
=================
PF_sound

Each entity can have eight independant sound sources, like voice,
weapon, feet, etc.

Channel 0 is an auto-allocate channel, the others override anything
already running on that entity/channel pair.

An attenuation of 0 will play full volume everywhere in the level.
Larger attenuations will drop off.

=================
*/
static void PF_sound (void)
{
	const char	*sample;
	int		channel;
	edict_t		*entity;
	int		volume;
	float	attenuation;

	entity = G_EDICT(OFS_PARM0);
	channel = G_FLOAT(OFS_PARM1);
	sample = G_STRING(OFS_PARM2);
	volume = G_FLOAT(OFS_PARM3) * 255;
	attenuation = G_FLOAT(OFS_PARM4);
	VR_DebugImpactSound (NUM_FOR_EDICT (entity), channel, sample, volume, attenuation); // QVR: vr_debug_impacts

	// QVR: a sixth argument, the pitch in percent (DP's and FTE's speedpct: 100 as recorded; 0 the same)
	if (qcvm->argc > 5 && G_FLOAT(OFS_PARM5) > 0.f)
		SV_StartSoundPitch (entity, channel, sample, volume, attenuation, G_FLOAT(OFS_PARM5) * 0.01f);
	else
		SV_StartSound (entity, channel, sample, volume, attenuation);
}

/*
=================
PF_break

break()
=================
*/
static void PF_break (void)
{
	Con_Printf ("break statement\n");
	*(int *)-4 = 0;	// dump to debugger
//	PR_RunError ("break statement");
}

/*
=================
PF_traceline

Used for use tracing and shot targeting
Traces are blocked by bbox and exact bsp entityes, and also slide box entities
if the tryents flag is set.

traceline (vector1, vector2, tryents)
=================
*/
static void PF_traceline (void)
{
	float	*v1, *v2;
	trace_t	trace;
	int	nomonsters;
	edict_t	*ent;

	v1 = G_VECTOR(OFS_PARM0);
	v2 = G_VECTOR(OFS_PARM1);
	nomonsters = G_FLOAT(OFS_PARM2);
	ent = G_EDICT(OFS_PARM3);

	/* FIXME FIXME FIXME: Why do we hit this with certain progs.dat ?? */
	if (developer.value) {
	  if (IS_NAN(v1[0]) || IS_NAN(v1[1]) || IS_NAN(v1[2]) ||
	      IS_NAN(v2[0]) || IS_NAN(v2[1]) || IS_NAN(v2[2])) {
	    Con_Warning ("NAN in traceline:\nv1(%f %f %f) v2(%f %f %f)\nentity %d\n",
		      v1[0], v1[1], v1[2], v2[0], v2[1], v2[2], NUM_FOR_EDICT(ent));
	  }
	}

	if (IS_NAN(v1[0]) || IS_NAN(v1[1]) || IS_NAN(v1[2]))
		v1[0] = v1[1] = v1[2] = 0;
	if (IS_NAN(v2[0]) || IS_NAN(v2[1]) || IS_NAN(v2[2]))
		v2[0] = v2[1] = v2[2] = 0;

	VR_PortalTraceBegin (); // QVR: (the last portal trace's crossings forgotten)
	trace = SV_Move (v1, vec3_origin, vec3_origin, v2, nomonsters & ~MOVE_PORTALS, ent);
	if (nomonsters & MOVE_PORTALS) // QVR: on through the teleporters it crosses (vr/vr_portals.cpp)
		VR_PortalTrace (v1, v2, nomonsters & ~MOVE_PORTALS, ent, &trace);

	pr_global_struct->trace_allsolid = trace.allsolid;
	pr_global_struct->trace_startsolid = trace.startsolid;
	pr_global_struct->trace_fraction = trace.fraction;
	pr_global_struct->trace_inwater = trace.inwater;
	pr_global_struct->trace_inopen = trace.inopen;
	VectorCopy (trace.endpos, pr_global_struct->trace_endpos);
	VectorCopy (trace.plane.normal, pr_global_struct->trace_plane_normal);
	pr_global_struct->trace_plane_dist =  trace.plane.dist;
	if (trace.ent)
		pr_global_struct->trace_ent = EDICT_TO_PROG(trace.ent);
	else
		pr_global_struct->trace_ent = EDICT_TO_PROG(qcvm->edicts);
}

/*
=================
PF_checkpos

Returns true if the given entity can move to the given position from it's
current position by walking or rolling.
FIXME: make work...
scalar checkpos (entity, vector)
=================
*/
#if 0
static void PF_checkpos (void)
{
}
#endif

//============================================================================

static byte	*checkpvs;	//ericw -- changed to malloc
static int	checkpvs_capacity;

static int PF_newcheckclient (int check)
{
	int		i;
	byte	*pvs;
	edict_t	*ent;
	mleaf_t	*leaf;
	vec3_t	org;
	int	pvsbytes;

// cycle to the next one

	if (check < 1)
		check = 1;
	if (check > svs.maxclients)
		check = svs.maxclients;

	if (check == svs.maxclients)
		i = 1;
	else
		i = check + 1;

	for ( ;  ; i++)
	{
		if (i == svs.maxclients+1)
			i = 1;

		ent = EDICT_NUM(i);

		if (i == check)
			break;	// didn't find anything else

		if (ent->free)
			continue;
		if (ent->v.health <= 0)
			continue;
		if ((int)ent->v.flags & FL_NOTARGET)
			continue;

	// anything that is a client, or has a client as an enemy
		break;
	}

// get the PVS for the entity
	VectorAdd (ent->v.origin, ent->v.view_ofs, org);
	leaf = Mod_PointInLeaf (org, sv.worldmodel);
	pvs = Mod_LeafPVS (leaf, sv.worldmodel);
	
	pvsbytes = (sv.worldmodel->numleafs+7)>>3;
	if (checkpvs == NULL || pvsbytes > checkpvs_capacity)
	{
		checkpvs_capacity = pvsbytes;
		checkpvs = (byte *) VR_HeapRealloc (checkpvs, checkpvs_capacity);
		if (!checkpvs)
			Sys_Error ("PF_newcheckclient: realloc() failed on %d bytes", checkpvs_capacity);
	}
	memcpy (checkpvs, pvs, pvsbytes);

	return i;
}

/*
=================
PF_checkclient

Returns a client (or object that has a client enemy) that would be a
valid target.

If there are more than one valid options, they are cycled each frame

If (self.origin + self.viewofs) is not in the PVS of the current target,
it is not returned at all.

name checkclient ()
=================
*/
#define	MAX_CHECK	16
static int c_invis, c_notvis;
static void PF_checkclient (void)
{
	edict_t	*ent, *self;
	mleaf_t	*leaf;
	int		l;
	vec3_t	view;

// find a new check if on a new frame
	if (qcvm->time - sv.lastchecktime >= 0.1)
	{
		sv.lastcheck = PF_newcheckclient (sv.lastcheck);
		sv.lastchecktime = qcvm->time;
	}

// return check if it might be visible
	ent = EDICT_NUM(sv.lastcheck);
	if (ent->free || ent->v.health <= 0)
	{
		RETURN_EDICT(qcvm->edicts);
		return;
	}

// if current entity can't possibly see the check entity, return 0
	self = PROG_TO_EDICT(pr_global_struct->self);
	VectorAdd (self->v.origin, self->v.view_ofs, view);
	leaf = Mod_PointInLeaf (view, sv.worldmodel);
	l = (leaf - sv.worldmodel->leafs) - 1;
	if ( (l < 0) || !(checkpvs[l>>3] & (1 << (l & 7))) )
	{
		c_notvis++;
		RETURN_EDICT(qcvm->edicts);
		return;
	}

// might be able to see it
	c_invis++;
	RETURN_EDICT(ent);
}

//============================================================================


/*
=================
PF_stuffcmd

Sends text over to the client's execution buffer

stuffcmd (clientent, value)
=================
*/
static void PF_stuffcmd (void)
{
	int		entnum;
	const char	*str;
	client_t	*old;

	entnum = G_EDICTNUM(OFS_PARM0);
	if (entnum < 1 || entnum > svs.maxclients)
		PR_RunError ("Parm 0 not a client");
	str = G_STRING(OFS_PARM1);

	old = host_client;
	host_client = &svs.clients[entnum-1];
	Host_ClientCommands ("%s", str);
	host_client = old;
}

/*
=================
PF_localcmd

Sends text over to the client's execution buffer

localcmd (string)
=================
*/
static void PF_localcmd (void)
{
	const char	*str;

	str = G_STRING(OFS_PARM0);
	Cbuf_AddText (str);
}

/*
=================
PF_cvar

float cvar (string)
=================
*/
static void PF_cvar (void)
{
	const char	*str;

	str = G_STRING(OFS_PARM0);

	G_FLOAT(OFS_RETURN) = Cvar_VariableValue (str);
}

/*
=================
PF_cvar_set

float cvar (string)
=================
*/
static void PF_cvar_set (void)
{
	const char	*var, *val;

	var = G_STRING(OFS_PARM0);
	val = G_STRING(OFS_PARM1);

	Cvar_Set (var, val);
}

/*
=================
PF_findradius

Returns a chain of entities that have origins within a spherical area

findradius (origin, radius)
=================
*/
#define FINDRADIUS_AHEAD 8 // QVR: edicts prefetched ahead of the walk

// ---- PROTOTYPE (agent/perfdesign, scratch, not for shipping): findradius variants timed and compared ----
#include <intrin.h>
void SV_AreaEdictsUnordered (const float *mins, const float *maxs, edict_t **list, int *listcount, int listspace);
cvar_t vr_frp = {"vr_frp", "0", CVAR_NONE}; // 0 vanilla; 1 all variants timed, compared (vanilla's chain); 2 solid-first; 3 grid
#define FRP_V 5	// 0 vanilla, 1 vanilla again (warm), 2 solid-first, 3 area grid + sort, 4 SoA snapshot scan
#define FRP_CALLERS 96
typedef struct { dfunction_t *f; const char *name; double calls, rad, hits, cand; unsigned long long t[FRP_V]; double diff[FRP_V]; } frp_caller_t;
static frp_caller_t frp_callers[FRP_CALLERS];
static int frp_ncallers, frp_griddiffs;
static edict_t **frp_buf[FRP_V]; static int frp_cnt[FRP_V]; static int frp_space;
static edict_t **frp_area; static int frp_areaspace;
static float *frp_cx, *frp_cy, *frp_cz; static unsigned char *frp_alive; static int frp_snapn, frp_snapspace;
static double frp_snaptime = -1, frp_frames, frp_calls, frp_edicts, frp_free, frp_notsolid, frp_samecentre, frp_samecall;
static unsigned long long frp_snapcycles, frp_c0; static double frp_t0;
#define FRP_SEEN 1024
static float frp_seen[FRP_SEEN][4]; static int frp_nseen;

static void FRP_Grow (void)
{
	int i, n = qcvm->num_edicts + 16;
	if (n <= frp_space)
		return;
	frp_space = n * 2;
	for (i = 0; i < FRP_V; i++)
		frp_buf[i] = (edict_t **) realloc (frp_buf[i], frp_space * sizeof (edict_t *));
	frp_area = (edict_t **) realloc (frp_area, frp_space * sizeof (edict_t *));
	frp_areaspace = frp_space;
}

static qboolean FRP_In (const edict_t *ent, const float *org, float rad)
{
	float d, lensq;
	d = org[0] - (ent->v.origin[0] + (ent->v.mins[0] + ent->v.maxs[0]) * 0.5); lensq = d * d; if (lensq > rad) return false;
	d = org[1] - (ent->v.origin[1] + (ent->v.mins[1] + ent->v.maxs[1]) * 0.5); lensq += d * d; if (lensq > rad) return false;
	d = org[2] - (ent->v.origin[2] + (ent->v.mins[2] + ent->v.maxs[2]) * 0.5); lensq += d * d; if (lensq > rad) return false;
	return true;
}

static void FRP_Vanilla (const float *org, float rad, edict_t **out, int *n)
{
	int i; edict_t *ent = NEXT_EDICT(qcvm->edicts);
	*n = 0;
	for (i = 1; i < qcvm->num_edicts; i++, ent = NEXT_EDICT(ent))
	{
		if (i + FINDRADIUS_AHEAD < qcvm->num_edicts)
		{
			const byte *ahead = (const byte *)ent + FINDRADIUS_AHEAD * qcvm->edict_size;
			__builtin_prefetch (ahead);
			__builtin_prefetch (ahead + offsetof (edict_t, v.solid));
			__builtin_prefetch (ahead + offsetof (edict_t, v.origin));
			__builtin_prefetch (ahead + offsetof (edict_t, v.mins));
			__builtin_prefetch (ahead + offsetof (edict_t, v.maxs) + sizeof (ent->v.maxs) - 1);
		}
		if (ent->free || ent->v.solid == SOLID_NOT || !FRP_In (ent, org, rad))
			continue;
		out[(*n)++] = ent;
	}
}

// ED_Free sets .solid 0: a free edict is SOLID_NOT, so .solid first skips the header's line
static void FRP_SolidFirst (const float *org, float rad, edict_t **out, int *n)
{
	int i; edict_t *ent = NEXT_EDICT(qcvm->edicts);
	*n = 0;
	for (i = 1; i < qcvm->num_edicts; i++, ent = NEXT_EDICT(ent))
	{
		if (i + FINDRADIUS_AHEAD < qcvm->num_edicts)
		{
			const byte *ahead = (const byte *)ent + FINDRADIUS_AHEAD * qcvm->edict_size;
			__builtin_prefetch (ahead + offsetof (edict_t, v.solid));
			__builtin_prefetch (ahead + offsetof (edict_t, v.mins));
		}
		if (ent->v.solid == SOLID_NOT || ent->free || !FRP_In (ent, org, rad))
			continue;
		out[(*n)++] = ent;
	}
}

static int FRP_PtrCmp (const void *a, const void *b)
{
	const edict_t *x = *(edict_t * const *)a, *y = *(edict_t * const *)b;
	return x < y ? -1 : x > y;
}

static void FRP_Grid (const float *org, float rad, edict_t **out, int *n, int *cand)
{
	float r = sqrtf (rad), lo[3], hi[3];
	int i, c = 0;
	*n = 0;
	for (i = 0; i < 3; i++) { lo[i] = org[i] - r; hi[i] = org[i] + r; }
	SV_AreaEdictsUnordered (lo, hi, frp_area, &c, frp_areaspace);
	*cand = c;
	for (i = 0; i < c; i++)
	{
		edict_t *ent = frp_area[i];
		if (ent->free || ent->v.solid == SOLID_NOT || !FRP_In (ent, org, rad))
			continue;
		out[(*n)++] = ent;
	}
	if (*n > 1)
		qsort (out, *n, sizeof (edict_t *), FRP_PtrCmp);
}

static void FRP_Snapshot (void)
{
	int i, n = qcvm->num_edicts; edict_t *ent;
	unsigned long long c = __rdtsc ();
	if (n > frp_snapspace)
	{
		frp_snapspace = n * 2;
		frp_cx = (float *) realloc (frp_cx, frp_snapspace * sizeof (float));
		frp_cy = (float *) realloc (frp_cy, frp_snapspace * sizeof (float));
		frp_cz = (float *) realloc (frp_cz, frp_snapspace * sizeof (float));
		frp_alive = (unsigned char *) realloc (frp_alive, frp_snapspace);
	}
	ent = qcvm->edicts;
	for (i = 0; i < n; i++, ent = NEXT_EDICT(ent))
	{
		frp_alive[i] = i > 0 && !ent->free && ent->v.solid != SOLID_NOT;
		frp_cx[i] = ent->v.origin[0] + (ent->v.mins[0] + ent->v.maxs[0]) * 0.5f;
		frp_cy[i] = ent->v.origin[1] + (ent->v.mins[1] + ent->v.maxs[1]) * 0.5f;
		frp_cz[i] = ent->v.origin[2] + (ent->v.mins[2] + ent->v.maxs[2]) * 0.5f;
		frp_free += ent->free;
		frp_notsolid += !ent->free && ent->v.solid == SOLID_NOT;
	}
	frp_snapn = n;
	frp_snapcycles += __rdtsc () - c;
}

static void FRP_SoA (const float *org, float rad, edict_t **out, int *n)
{
	int i; const float ox = org[0], oy = org[1], oz = org[2];
	*n = 0;
	for (i = 1; i < frp_snapn; i++)
	{
		const float dx = ox - frp_cx[i], dy = oy - frp_cy[i], dz = oz - frp_cz[i];
		if (frp_alive[i] && dx * dx + dy * dy + dz * dz <= rad)
			out[(*n)++] = EDICT_NUM(i);
	}
}

static void FRP_Run (const float *org, float rad)
{
	int v, i, cand = 0, mode = (int) vr_frp.value;
	unsigned long long c;
	frp_caller_t *cs = NULL;
	edict_t *chain = (edict_t *)qcvm->edicts, **res;
	FRP_Grow ();
	if (!frp_c0) { frp_c0 = __rdtsc (); frp_t0 = Sys_DoubleTime (); }
	if (qcvm->time != frp_snaptime)
	{
		frp_snaptime = qcvm->time; frp_frames++; frp_nseen = 0;
		frp_edicts += qcvm->num_edicts;
		if (mode == 1) FRP_Snapshot ();
	}
	frp_calls++;
	for (i = 0; i < frp_nseen; i++)
		if (frp_seen[i][0] == org[0] && frp_seen[i][1] == org[1] && frp_seen[i][2] == org[2])
		{
			frp_samecentre++;
			if (frp_seen[i][3] == rad) frp_samecall++;
			break;
		}
	if (frp_nseen < FRP_SEEN) { frp_seen[frp_nseen][0] = org[0]; frp_seen[frp_nseen][1] = org[1]; frp_seen[frp_nseen][2] = org[2]; frp_seen[frp_nseen][3] = rad; frp_nseen++; }
	for (i = 0; i < frp_ncallers; i++)
		if (frp_callers[i].f == qcvm->xfunction) { cs = &frp_callers[i]; break; }
	if (!cs && frp_ncallers < FRP_CALLERS) { cs = &frp_callers[frp_ncallers++]; memset (cs, 0, sizeof (*cs)); cs->f = qcvm->xfunction; cs->name = qcvm->xfunction ? PR_GetString (qcvm->xfunction->s_name) : "?"; }
	rad = fabsf (rad);
	rad *= rad;
	if (mode == 1)
	{
		c = __rdtsc (); FRP_Vanilla (org, rad, frp_buf[0], &frp_cnt[0]); if (cs) cs->t[0] += __rdtsc () - c;
		c = __rdtsc (); FRP_Vanilla (org, rad, frp_buf[1], &frp_cnt[1]); if (cs) cs->t[1] += __rdtsc () - c;
		c = __rdtsc (); FRP_SolidFirst (org, rad, frp_buf[2], &frp_cnt[2]); if (cs) cs->t[2] += __rdtsc () - c;
		c = __rdtsc (); FRP_Grid (org, rad, frp_buf[3], &frp_cnt[3], &cand); if (cs) cs->t[3] += __rdtsc () - c;
		c = __rdtsc (); FRP_SoA (org, rad, frp_buf[4], &frp_cnt[4]); if (cs) cs->t[4] += __rdtsc () - c;
		for (v = 1; v < FRP_V && cs; v++)
			if (frp_cnt[v] != frp_cnt[0] || memcmp (frp_buf[v], frp_buf[0], frp_cnt[0] * sizeof (edict_t *)))
			{
				cs->diff[v]++;
				if (v == 3 && frp_griddiffs < 40)
				{
					int a, b;
					for (a = 0; a < frp_cnt[0]; a++)
					{
						edict_t *e = frp_buf[0][a];
						for (b = 0; b < frp_cnt[3] && frp_buf[3][b] != e; b++) {}
						if (b == frp_cnt[3] && frp_griddiffs++ < 40)
							Con_Printf ("frpdiff: %s missed #%d %s solid %g movetype %g linked %d origin %.1f %.1f %.1f abs centre %.1f %.1f %.1f\n",
								cs->name, NUM_FOR_EDICT(e), PR_GetString (e->v.classname), e->v.solid, e->v.movetype, e->area.prev != NULL,
								e->v.origin[0], e->v.origin[1], e->v.origin[2], (e->v.absmin[0] + e->v.absmax[0]) * 0.5,
								(e->v.absmin[1] + e->v.absmax[1]) * 0.5, (e->v.absmin[2] + e->v.absmax[2]) * 0.5);
					}
				}
			}
		res = frp_buf[0]; v = 0;
	}
	else
	{
		v = mode == 3 ? 3 : mode == 2 ? 2 : 0;
		c = __rdtsc ();
		if (v == 3) FRP_Grid (org, rad, frp_buf[3], &frp_cnt[3], &cand);
		else if (v == 2) FRP_SolidFirst (org, rad, frp_buf[2], &frp_cnt[2]);
		else FRP_Vanilla (org, rad, frp_buf[0], &frp_cnt[0]);
		if (cs) cs->t[v] += __rdtsc () - c;
		res = frp_buf[v];
	}
	if (cs) { cs->calls++; cs->rad += sqrtf (rad); cs->hits += frp_cnt[v]; cs->cand += cand; }
	for (i = 0; i < frp_cnt[v]; i++)
	{
		res[i]->v.chain = EDICT_TO_PROG(chain);
		chain = res[i];
	}
	RETURN_EDICT(chain);
}

static int FRP_CallerCmp (const void *a, const void *b)
{
	const frp_caller_t *x = (const frp_caller_t *)a, *y = (const frp_caller_t *)b;
	return x->t[0] + x->t[2] + x->t[3] < y->t[0] + y->t[2] + y->t[3] ? 1 : -1;
}

static void FRP_Stats_f (void)
{
	int i, v;
	double ns = frp_c0 ? (Sys_DoubleTime () - frp_t0) * 1e9 / (double)(__rdtsc () - frp_c0) : 0, tot[FRP_V] = {0}, diffs[FRP_V] = {0};
	Con_Printf ("frp: mode %d, %.0f frames, %.0f calls (%.1f a frame), edicts %.0f avg (free %.0f, SOLID_NOT %.0f a frame), edict_size %d\n",
		(int) vr_frp.value, frp_frames, frp_calls, frp_calls / (frp_frames > 0 ? frp_frames : 1), frp_edicts / (frp_frames > 0 ? frp_frames : 1),
		frp_free / (frp_frames > 0 ? frp_frames : 1), frp_notsolid / (frp_frames > 0 ? frp_frames : 1), sv.qcvm.edict_size);
	Con_Printf ("frp: offsets free %d solid %d origin %d mins %d maxs %d absmin %d chain %d\n", (int) offsetof (edict_t, free),
		(int) offsetof (edict_t, v.solid), (int) offsetof (edict_t, v.origin), (int) offsetof (edict_t, v.mins), (int) offsetof (edict_t, v.maxs),
		(int) offsetof (edict_t, v.absmin), (int) offsetof (edict_t, v.chain));
	Con_Printf ("frp: same centre again in a frame %.0f (same centre and radius %.0f); snapshot build %.2f us a frame\n", frp_samecentre,
		frp_samecall, frp_frames > 0 ? frp_snapcycles * ns / 1000.0 / frp_frames : 0);
	qsort (frp_callers, frp_ncallers, sizeof (frp_caller_t), FRP_CallerCmp);
	Con_Printf ("frp: caller calls/frame rad hits cand | us a call: vanilla, again, solidfirst, grid, soa | diffs again sf grid soa\n");
	for (i = 0; i < frp_ncallers; i++)
	{
		const frp_caller_t *c = &frp_callers[i];
		for (v = 0; v < FRP_V; v++) { tot[v] += c->t[v]; diffs[v] += c->diff[v]; }
		if (i < 14)
			Con_Printf ("frp: %-28s %6.2f %5.0f %5.1f %5.1f | %6.2f %6.2f %6.2f %6.2f %6.2f | %.0f %.0f %.0f %.0f\n",
				c->name, c->calls / (frp_frames > 0 ? frp_frames : 1), c->rad / c->calls, c->hits / c->calls,
				c->cand / c->calls, c->t[0] * ns / 1000 / c->calls, c->t[1] * ns / 1000 / c->calls, c->t[2] * ns / 1000 / c->calls,
				c->t[3] * ns / 1000 / c->calls, c->t[4] * ns / 1000 / c->calls, c->diff[1], c->diff[2], c->diff[3], c->diff[4]);
	}
	Con_Printf ("frp: total ms a frame: vanilla %.3f again %.3f solidfirst %.3f grid %.3f soa %.3f (+snapshot) | diffs %.0f %.0f %.0f %.0f\n",
		tot[0] * ns / 1e6 / frp_frames, tot[1] * ns / 1e6 / frp_frames, tot[2] * ns / 1e6 / frp_frames, tot[3] * ns / 1e6 / frp_frames,
		tot[4] * ns / 1e6 / frp_frames, diffs[1], diffs[2], diffs[3], diffs[4]);
	Con_Printf ("frp: done\n");
	frp_ncallers = 0; frp_frames = frp_calls = frp_edicts = frp_free = frp_notsolid = frp_samecentre = frp_samecall = 0; frp_snapcycles = 0;
}

void FRP_Init (void)
{
	Cvar_RegisterVariable (&vr_frp);
	Cmd_AddCommand ("vr_frp_stats", FRP_Stats_f);
}
// ---- end PROTOTYPE ----

static void PF_findradius (void)
{
	edict_t	*ent, *chain;
	float	rad;
	float	*org;
	int		i;

	if (vr_frp.value && qcvm == &sv.qcvm) // PROTOTYPE
	{
		FRP_Run (G_VECTOR(OFS_PARM0), G_FLOAT(OFS_PARM1));
		return;
	}
	chain = (edict_t *)qcvm->edicts;

	org = G_VECTOR(OFS_PARM0);
	rad = G_FLOAT(OFS_PARM1);
	rad *= rad;

	ent = NEXT_EDICT(qcvm->edicts);
	for (i = 1; i < qcvm->num_edicts; i++, ent = NEXT_EDICT(ent))
	{
		float d, lensq;
		// QVR: the edicts are kilobytes apart, beyond the hardware prefetchers' reach (a page): the lines this walk reads
		// asked for FINDRADIUS_AHEAD edicts ahead (an armed humanoid's three findradius a frame cost 12 us each on a
		// big AD map; ROUND24.md, "Tracy session 11-07"). It reads the same fields: the same chain.
		if (i + FINDRADIUS_AHEAD < qcvm->num_edicts)
		{
			const byte *ahead = (const byte *)ent + FINDRADIUS_AHEAD * qcvm->edict_size;
			__builtin_prefetch (ahead);
			__builtin_prefetch (ahead + offsetof (edict_t, v.solid));
			__builtin_prefetch (ahead + offsetof (edict_t, v.origin));
			__builtin_prefetch (ahead + offsetof (edict_t, v.mins));
			__builtin_prefetch (ahead + offsetof (edict_t, v.maxs) + sizeof (ent->v.maxs) - 1);
		}
		if (ent->free)
			continue;
		if (ent->v.solid == SOLID_NOT)
			continue;

		d = org[0] - (ent->v.origin[0] + (ent->v.mins[0] + ent->v.maxs[0]) * 0.5);
		lensq = d * d;
		if (lensq > rad)
			continue;
		d = org[1] - (ent->v.origin[1] + (ent->v.mins[1] + ent->v.maxs[1]) * 0.5);
		lensq += d * d;
		if (lensq > rad)
			continue;
		d = org[2] - (ent->v.origin[2] + (ent->v.mins[2] + ent->v.maxs[2]) * 0.5);
		lensq += d * d;
		if (lensq > rad)
			continue;

		ent->v.chain = EDICT_TO_PROG(chain);
		chain = ent;
	}

	RETURN_EDICT(chain);
}

/*
=========
PF_dprint
=========
*/
static void PF_dprint (void)
{
	Con_DPrintf ("%s",PF_VarString(0));
}

static void PF_ftos (void)
{
	float	v;
	char	*s;

	v = G_FLOAT(OFS_PARM0);
	s = PR_GetTempString();
	if (v == (int)v)
		sprintf (s, "%d",(int)v);
	else
		sprintf (s, "%5.1f",v);
	G_INT(OFS_RETURN) = PR_SetEngineString(s);
}

static void PF_fabs (void)
{
	float	v;
	v = G_FLOAT(OFS_PARM0);
	G_FLOAT(OFS_RETURN) = fabs(v);
}

static void PF_vtos (void)
{
	char	*s;

	s = PR_GetTempString();
	sprintf (s, "'%5.1f %5.1f %5.1f'", G_VECTOR(OFS_PARM0)[0], G_VECTOR(OFS_PARM0)[1], G_VECTOR(OFS_PARM0)[2]);
	G_INT(OFS_RETURN) = PR_SetEngineString(s);
}

static void PF_Spawn (void)
{
	edict_t	*ed;

	ed = ED_Alloc();

	RETURN_EDICT(ed);
}

static void PF_Remove (void)
{
	edict_t	*ed;

	ed = G_EDICT(OFS_PARM0);
	ED_Free (ed);
}


// entity (entity start, .string field, string match) find = #5;
static void PF_Find (void)
{
	int		e;
	int		f;
	const char	*s, *t;
	edict_t	*ed;

	e = G_EDICTNUM(OFS_PARM0);
	f = G_INT(OFS_PARM1);
	s = G_STRING(OFS_PARM2);
	if (!s)
		PR_RunError ("PF_Find: bad search string");

	if (qcvm == &sv.qcvm) // QVR: .classname through the edict index (vr_edictindex.cpp), the same edict as the walk
	{
		const int found = VR_EdictIndex_Find (e, f, s);
		if (found >= 0)
		{
			RETURN_EDICT(EDICT_NUM(found));
			return;
		}
	}

	for (e++ ; e < qcvm->num_edicts ; e++)
	{
		ed = EDICT_NUM(e);
		if (ed->free)
			continue;
		t = E_STRING(ed,f);
		if (!t)
			continue;
		if (!strcmp(t,s))
		{
			RETURN_EDICT(ed);
			return;
		}
	}

	RETURN_EDICT(qcvm->edicts);
}

static void PR_CheckEmptyString (const char *s)
{
	if (s[0] <= ' ')
		PR_RunError ("Bad string");
}

static void PF_precache_file (void)
{	// precache_file is only used to copy files with qcc, it does nothing
	G_INT(OFS_RETURN) = G_INT(OFS_PARM0);
}

static void PF_precache_sound (void)
{
	const char	*s;
	int		i;

	if (sv.state != ss_loading && !VR_AllowLatePrecache ()) // QVR
		PR_RunError ("PF_Precache_*: Precache can only be done in spawn functions");

	s = G_STRING(OFS_PARM0);
	G_INT(OFS_RETURN) = G_INT(OFS_PARM0);
	PR_CheckEmptyString (s);

	for (i = 0; i < MAX_SOUNDS; i++)
	{
		if (!sv.sound_precache[i])
		{
			sv.sound_precache[i] = s;
			return;
		}
		if (!strcmp(sv.sound_precache[i], s))
			return;
	}
	PR_RunError ("PF_precache_sound: overflow");
}

static void PF_precache_model (void)
{
	const char	*s;
	int		i;

	if (sv.state != ss_loading && !VR_AllowLatePrecache ()) // QVR
		PR_RunError ("PF_Precache_*: Precache can only be done in spawn functions");

	s = G_STRING(OFS_PARM0);
	G_INT(OFS_RETURN) = G_INT(OFS_PARM0);
	PR_CheckEmptyString (s);

	for (i = 0; i < MAX_MODELS; i++)
	{
		if (!sv.model_precache[i])
		{
			double t0 = Sys_DoubleTime (); // QVR: a late one's load (the frame's hitch), said with developer 1
			sv.model_precache[i] = s;
			sv.models[i] = Mod_ForName (s, true);
			if (sv.state == ss_active)
				Con_DPrintf ("late precache: %s, %.1f ms\n", s, (Sys_DoubleTime () - t0) * 1000.0);
			return;
		}
		if (!strcmp(sv.model_precache[i], s))
			return;
	}
	PR_RunError ("PF_precache_model: overflow");
}


static void PF_coredump (void)
{
	ED_PrintEdicts ();
}

static void PF_traceon (void)
{
	qcvm->trace = true;
}

static void PF_traceoff (void)
{
	qcvm->trace = false;
}

static void PF_eprint (void)
{
	ED_PrintNum (G_EDICTNUM(OFS_PARM0));
}

/*
===============
PF_walkmove

float(float yaw, float dist) walkmove
===============
*/
static void PF_walkmove (void)
{
	edict_t	*ent;
	float	yaw, dist;
	vec3_t	move;
	dfunction_t	*oldf;
	int	oldself;

	ent = PROG_TO_EDICT(pr_global_struct->self);
	yaw = G_FLOAT(OFS_PARM0);
	dist = G_FLOAT(OFS_PARM1);

	if ( !( (int)ent->v.flags & (FL_ONGROUND|FL_FLY|FL_SWIM) ) )
	{
		G_FLOAT(OFS_RETURN) = 0;
		return;
	}

	yaw = yaw * M_PI * 2 / 360;

	move[0] = cos(yaw) * dist;
	move[1] = sin(yaw) * dist;
	move[2] = 0;

// save program state, because SV_movestep may call other progs
	oldf = qcvm->xfunction;
	oldself = pr_global_struct->self;

	G_FLOAT(OFS_RETURN) = SV_movestep(ent, move, true);


// restore program state
	qcvm->xfunction = oldf;
	pr_global_struct->self = oldself;
}

/*
===============
PF_droptofloor

void() droptofloor
===============
*/
static void PF_droptofloor (void)
{
	edict_t		*ent;
	vec3_t		end;
	trace_t		trace;

	if (VR_DropToFloor ()) // QVR
		return;

	ent = PROG_TO_EDICT(pr_global_struct->self);

	VectorCopy (ent->v.origin, end);
	end[2] -= 256;

	trace = SV_Move (ent->v.origin, ent->v.mins, ent->v.maxs, end, false, ent);

	if (trace.fraction == 1 || trace.allsolid)
		G_FLOAT(OFS_RETURN) = 0;
	else
	{
		VectorCopy (trace.endpos, ent->v.origin);
		SV_LinkEdict (ent, false);
		ent->v.flags = (int)ent->v.flags | FL_ONGROUND;
		ent->v.groundentity = EDICT_TO_PROG(trace.ent);
		G_FLOAT(OFS_RETURN) = 1;
	}
}

/*
===============
PF_lightstyle

void(float style, string value) lightstyle
===============
*/
static void PF_lightstyle (void)
{
	int		style;
	const char	*val;
	client_t	*client;
	int	j;

	style = G_FLOAT(OFS_PARM0);
	val = G_STRING(OFS_PARM1);

// bounds check to avoid clobbering sv struct
	if (style < 0 || style >= MAX_LIGHTSTYLES)
	{
		Con_DWarning("PF_lightstyle: invalid style %d\n", style);
		return;
	}

// change the string in sv
	sv.lightstyles[style] = val;

// send message to all clients on this server
	if (sv.state != ss_active)
		return;

	for (j = 0, client = svs.clients; j < svs.maxclients; j++, client++)
	{
		if (client->active || client->spawned)
		{
			MSG_WriteChar (&client->message, svc_lightstyle);
			MSG_WriteChar (&client->message, style);
			MSG_WriteString (&client->message, val);
		}
	}
}

static void PF_rint (void)
{
	float	f;
	f = G_FLOAT(OFS_PARM0);
	if (f > 0)
		G_FLOAT(OFS_RETURN) = (int)(f + 0.5);
	else
		G_FLOAT(OFS_RETURN) = (int)(f - 0.5);
}

static void PF_floor (void)
{
	G_FLOAT(OFS_RETURN) = floor(G_FLOAT(OFS_PARM0));
}

static void PF_ceil (void)
{
	G_FLOAT(OFS_RETURN) = ceil(G_FLOAT(OFS_PARM0));
}


/*
=============
PF_checkbottom
=============
*/
static void PF_checkbottom (void)
{
	edict_t	*ent;

	ent = G_EDICT(OFS_PARM0);

	G_FLOAT(OFS_RETURN) = SV_CheckBottom (ent);
}

/*
=============
PF_pointcontents
=============
*/
static void PF_pointcontents (void)
{
	float	*v;

	v = G_VECTOR(OFS_PARM0);

	G_FLOAT(OFS_RETURN) = SV_PointContents (v);
}

/*
=============
PF_nextent

entity nextent(entity)
=============
*/
static void PF_nextent (void)
{
	int		i;
	edict_t	*ent;

	i = G_EDICTNUM(OFS_PARM0);
	while (1)
	{
		i++;
		if (i == qcvm->num_edicts)
		{
			RETURN_EDICT(qcvm->edicts);
			return;
		}
		ent = EDICT_NUM(i);
		if (!ent->free)
		{
			RETURN_EDICT(ent);
			return;
		}
	}
}

/*
=============
PF_aim

Pick a vector for the player to shoot along
vector aim(entity, missilespeed)
=============
*/
cvar_t	sv_aim = {"sv_aim", "1", CVAR_NONE}; // ericw -- turn autoaim off by default. was 0.93
static void PF_aim (void)
{
	edict_t	*ent, *check, *bestent;
	vec3_t	start, dir, end, bestdir;
	int		i, j;
	trace_t	tr;
	float	dist, bestdist;
	float	speed;

	ent = G_EDICT(OFS_PARM0);
	speed = G_FLOAT(OFS_PARM1);
	(void) speed; /* variable set but not used */

	VectorCopy (ent->v.origin, start);
	start[2] += 20;

// try sending a trace straight
	VectorCopy (pr_global_struct->v_forward, dir);
	VectorMA (start, 2048, dir, end);
	tr = SV_Move (start, vec3_origin, vec3_origin, end, false, ent);
	if (tr.ent && tr.ent->v.takedamage == DAMAGE_AIM
		&& (!teamplay.value || ent->v.team <= 0 || ent->v.team != tr.ent->v.team) )
	{
		VectorCopy (pr_global_struct->v_forward, G_VECTOR(OFS_RETURN));
		return;
	}

// try all possible entities
	VectorCopy (dir, bestdir);
	bestdist = sv_aim.value;
	bestent = NULL;

	check = NEXT_EDICT(qcvm->edicts);
	for (i = 1; i < qcvm->num_edicts; i++, check = NEXT_EDICT(check) )
	{
		if (check->v.takedamage != DAMAGE_AIM)
			continue;
		if (check == ent)
			continue;
		if (teamplay.value && ent->v.team > 0 && ent->v.team == check->v.team)
			continue;	// don't aim at teammate
		for (j = 0; j < 3; j++)
			end[j] = check->v.origin[j] + 0.5 * (check->v.mins[j] + check->v.maxs[j]);
		VectorSubtract (end, start, dir);
		VectorNormalize (dir);
		dist = DotProduct (dir, pr_global_struct->v_forward);
		if (dist < bestdist)
			continue;	// to far to turn
		tr = SV_Move (start, vec3_origin, vec3_origin, end, false, ent);
		if (tr.ent == check)
		{	// can shoot at this one
			bestdist = dist;
			bestent = check;
		}
	}

	if (bestent)
	{
		VectorSubtract (bestent->v.origin, ent->v.origin, dir);
		dist = DotProduct (dir, pr_global_struct->v_forward);
		VectorScale (pr_global_struct->v_forward, dist, end);
		end[2] = dir[2];
		VectorNormalize (end);
		VectorCopy (end, G_VECTOR(OFS_RETURN));
	}
	else
	{
		VectorCopy (bestdir, G_VECTOR(OFS_RETURN));
	}
}

/*
==============
PF_changeyaw

This was a major timewaster in progs, so it was converted to C
==============
*/
void PF_changeyaw (void)
{
	edict_t		*ent;
	float		ideal, current, move, speed;

	ent = PROG_TO_EDICT(pr_global_struct->self);
	current = anglemod( ent->v.angles[1] );
	ideal = ent->v.ideal_yaw;
	speed = ent->v.yaw_speed;

	if (current == ideal)
		return;
	move = ideal - current;
	if (ideal > current)
	{
		if (move >= 180)
			move = move - 360;
	}
	else
	{
		if (move <= -180)
			move = move + 360;
	}
	if (move > 0)
	{
		if (move > speed)
			move = speed;
	}
	else
	{
		if (move < -speed)
			move = -speed;
	}

	ent->v.angles[1] = anglemod (current + move);
}

/*
===============================================================================

MESSAGE WRITING

===============================================================================
*/

static sizebuf_t *WriteDest (int len) // QVR: len, the most bytes the write takes (MSG_BROADCAST's room)
{
	int		entnum;
	int		dest;
	edict_t	*ent;

	dest = G_FLOAT(OFS_PARM0);
	switch (dest)
	{
	case MSG_BROADCAST:
		return VR_BroadcastDest (len); // QVR: sv.datagram, or nowhere when full (was an overflow's Host_Error)

	case MSG_ONE:
		ent = PROG_TO_EDICT(pr_global_struct->msg_entity);
		entnum = NUM_FOR_EDICT(ent);
		if (entnum < 1 || entnum > svs.maxclients)
			PR_RunError ("WriteDest: not a client");
		return &svs.clients[entnum-1].message;

	case MSG_ALL:
		return &sv.reliable_datagram;

	case MSG_INIT:
		return sv.signon;

	default:
		PR_RunError ("WriteDest: bad destination");
		break;
	}

	return NULL;
}

static void PF_WriteByte (void)
{
	sizebuf_t *dest = WriteDest (1);
	MSG_WriteByte (dest, G_FLOAT(OFS_PARM1));
	VR_BroadcastWritten (dest); // QVR
}

static void PF_WriteChar (void)
{
	sizebuf_t *dest = WriteDest (1);
	MSG_WriteChar (dest, G_FLOAT(OFS_PARM1));
	VR_BroadcastWritten (dest); // QVR
}

static void PF_WriteShort (void)
{
	sizebuf_t *dest = WriteDest (2);
	MSG_WriteShort (dest, G_FLOAT(OFS_PARM1));
	VR_BroadcastWritten (dest); // QVR
}

static void PF_WriteLong (void)
{
	sizebuf_t *dest = WriteDest (4);
	MSG_WriteLong (dest, G_FLOAT(OFS_PARM1));
	VR_BroadcastWritten (dest); // QVR
}

static void PF_WriteAngle (void)
{
	sizebuf_t *dest = WriteDest (4);
	MSG_WriteAngle (dest, G_FLOAT(OFS_PARM1), sv.protocolflags);
	VR_BroadcastWritten (dest); // QVR
}

static void PF_WriteCoord (void)
{
	sizebuf_t *dest = WriteDest (4);
	MSG_WriteCoord (dest, G_FLOAT(OFS_PARM1), sv.protocolflags);
	VR_BroadcastWritten (dest); // QVR
}

static void PF_WriteString (void)
{
	const char *str = LOC_GetString(G_STRING(OFS_PARM1));
	sizebuf_t *dest = WriteDest ((int) strlen (str) + 1);
	MSG_WriteString (dest, str);
	VR_BroadcastWritten (dest); // QVR
}

static void PF_WriteEntity (void)
{
	sizebuf_t *dest = WriteDest (2);
	MSG_WriteShort (dest, G_EDICTNUM(OFS_PARM1));
	VR_BroadcastWritten (dest); // QVR
}

//=============================================================================

static void PF_makestatic (void)
{
	edict_t	*ent;
	int		i;
	int	bits = 0; //johnfitz -- PROTOCOL_FITZQUAKE

	ent = G_EDICT(OFS_PARM0);

	//johnfitz -- don't send invisible static entities
	if (ent->alpha == ENTALPHA_ZERO) {
		ED_Free (ent);
		return;
	}
	//johnfitz

	//johnfitz -- PROTOCOL_FITZQUAKE
	if (sv.protocol == PROTOCOL_NETQUAKE)
	{
		if (SV_ModelIndex(PR_GetString(ent->v.model)) & 0xFF00 || (int)(ent->v.frame) & 0xFF00)
		{
			ED_Free (ent);
			return; //can't display the correct model & frame, so don't show it at all
		}
	}
	else
	{
		if (SV_ModelIndex(PR_GetString(ent->v.model)) & 0xFF00)
			bits |= B_LARGEMODEL;
		if ((int)(ent->v.frame) & 0xFF00)
			bits |= B_LARGEFRAME;
		if (ent->alpha != ENTALPHA_DEFAULT)
			bits |= B_ALPHA;

		if (sv.protocol == PROTOCOL_RMQ)
		{
			eval_t* val;
			val = GetEdictFieldValueByName(ent, "scale");
			if (val)
				ent->scale = ENTSCALE_ENCODE(val->_float);
			else
				ent->scale = ENTSCALE_DEFAULT;

			if (ent->scale != ENTSCALE_DEFAULT)
				bits |= B_SCALE;
		}
	}

	SV_ReserveSignonSpace (34);

	if (bits)
	{
		MSG_WriteByte (sv.signon, svc_spawnstatic2);
		MSG_WriteByte (sv.signon, bits);
	}
	else
		MSG_WriteByte (sv.signon, svc_spawnstatic);

	if (bits & B_LARGEMODEL)
		MSG_WriteShort (sv.signon, SV_ModelIndex(PR_GetString(ent->v.model)));
	else
		MSG_WriteByte (sv.signon, SV_ModelIndex(PR_GetString(ent->v.model)));

	if (bits & B_LARGEFRAME)
		MSG_WriteShort (sv.signon, ent->v.frame);
	else
		MSG_WriteByte (sv.signon, ent->v.frame);
	//johnfitz

	MSG_WriteByte (sv.signon, ent->v.colormap);
	MSG_WriteByte (sv.signon, ent->v.skin);
	for (i = 0; i < 3; i++)
	{
		MSG_WriteCoord(sv.signon, ent->v.origin[i], sv.protocolflags);
		MSG_WriteAngle(sv.signon, ent->v.angles[i], sv.protocolflags);
	}

	//johnfitz -- PROTOCOL_FITZQUAKE
	if (bits & B_ALPHA)
		MSG_WriteByte (sv.signon, ent->alpha);
	//johnfitz

	if (bits & B_SCALE)
		MSG_WriteByte (sv.signon, ent->scale);

	VR_OnMakeStatic (ent); // QVR: where it stands (the rocks and bricks keep clear of torches and flames)

// throw the entity away now
	ED_Free (ent);
}

//=============================================================================

/*
==============
PF_setspawnparms
==============
*/
static void PF_setspawnparms (void)
{
	edict_t	*ent;
	int		i;
	client_t	*client;

	ent = G_EDICT(OFS_PARM0);
	i = NUM_FOR_EDICT(ent);
	if (i < 1 || i > svs.maxclients)
		PR_RunError ("Entity is not a client");

	// copy spawn parms out of the client_t
	client = svs.clients + (i-1);

	for (i = 0; i < NUM_SPAWN_PARMS; i++)
		(&pr_global_struct->parm1)[i] = client->spawn_parms[i];
	VR_RestoreSpawnParms (client - svs.clients); // QVR
}

/*
==============
PF_changelevel
==============
*/
static void PF_changelevel (void)
{
	const char	*s;

// make sure we don't issue two changelevels
	if (svs.changelevel_issued)
		return;
	svs.changelevel_issued = true;

	s = G_STRING(OFS_PARM0);
	Cbuf_AddText (va("changelevel %s\n",s));
}

/*
==============
2021 re-release
==============
*/
static void PF_finalefinished (void)
{
	G_FLOAT(OFS_RETURN) = 0;
}
static void PF_CheckPlayerEXFlags (void)
{
	G_FLOAT(OFS_RETURN) = 0;
}
static void PF_walkpathtogoal (void)
{
	G_FLOAT(OFS_RETURN) = 0; /* PATH_ERROR */
}
static void PF_localsound (void)
{
	const char	*sample;
	int		entnum;

	entnum = G_EDICTNUM(OFS_PARM0);
	sample = G_STRING(OFS_PARM1);
	if (entnum < 1 || entnum > svs.maxclients) {
		Con_Printf ("tried to localsound to a non-client\n");
		return;
	}
	SV_LocalSound (&svs.clients[entnum-1], sample);
}

void PF_Fixme (void)
{
	PR_RunError ("unimplemented builtin");
}

/*
===============================================================================

EXTENSION BUILT-INS

===============================================================================
*/

cvar_t pr_checkextension = {"pr_checkextension", "1", CVAR_NONE};	//spike - enables qc extensions. if 0 then they're ALL BLOCKED! MWAHAHAHA! *cough* *splutter*

static void PF_checkextension(void)
{
	const char *extname = G_STRING(OFS_PARM0);
	int i = PR_FindExtensionByName (extname);
	if (i)
		SetBit (qcvm->checked_ext, i);

	// Note: we expose FTE_QC_CHECKCOMMAND so that AD considers the engine
	// FTE-like instead of DP-like, in order to avoid a bug in the DP codepath
	// in older AD versions (e.g. 1.42, used in jam8)
	if (i == FTE_QC_CHECKCOMMAND)
	{
		G_FLOAT(OFS_RETURN) = true;
		SetBit (qcvm->advertised_ext, i);
	}
	else
		G_FLOAT(OFS_RETURN) = false;
}

#define ishex(c) ((c>='0' && c<= '9') || (c>='a' && c<='f') || (c>='A' && c<='F'))
static int dehex(char c)
{
	if (c >= '0' && c <= '9')
		return c-'0';
	if (c >= 'A' && c <= 'F')
		return c-('A'-10);
	if (c >= 'a' && c <= 'f')
		return c-('a'-10);
	return 0;
}
//returns the next char...
struct markup_s
{
	const unsigned char *txt;
	vec4_t tint;	//predefined colour that applies to the entire string
	vec4_t colour;	//colour for the specific glyph in question
	unsigned char mask;
};
void PR_Markup_Begin(struct markup_s *mu, const char *text, float *rgb, float alpha)
{
	if (*text == '\1' || *text == '\2')
	{
		mu->mask = 128;
		text++;
	}
	else
		mu->mask = 0;
	mu->txt = (const unsigned char *)text;
	VectorCopy(rgb, mu->tint);
	mu->tint[3] = alpha;
	VectorCopy(rgb, mu->colour);
	mu->colour[3] = alpha;
}
int PR_Markup_Parse(struct markup_s *mu)
{
	static const vec4_t q3rgb[10] = {
		{0.00,0.00,0.00, 1.0},
		{1.00,0.33,0.33, 1.0},
		{0.00,1.00,0.33, 1.0},
		{1.00,1.00,0.33, 1.0},
		{0.33,0.33,1.00, 1.0},
		{0.33,1.00,1.00, 1.0},
		{1.00,0.33,1.00, 1.0},
		{1.00,1.00,1.00, 1.0},
		{1.00,1.00,1.00, 0.5},
		{0.50,0.50,0.50, 1.0}
	};
	unsigned int c;
	const float *f;
	while ((c = *mu->txt))
	{
		if (c == '^' && false/*pr_checkextension.value*/)
		{	//parse markup like FTE/DP might.
			switch(mu->txt[1])
			{
			case '^':	//doubled up char for escaping.
				mu->txt++;
				break;
			case '0':	//black
			case '1':	//red
			case '2':	//green
			case '3':	//yellow
			case '4':	//blue
			case '5':	//cyan
			case '6':	//magenta
			case '7':	//white
			case '8':	//white+half-alpha
			case '9':	//grey
				f = q3rgb[mu->txt[1]-'0'];
				mu->colour[0] = mu->tint[0] * f[0];
				mu->colour[1] = mu->tint[1] * f[1];
				mu->colour[2] = mu->tint[2] * f[2];
				mu->colour[3] = mu->tint[3] * f[3];
				mu->txt+=2;
				continue;
			case 'h':	//toggle half-alpha
				if (mu->colour[3] != mu->tint[3] * 0.5)
					mu->colour[3] = mu->tint[3] * 0.5;
				else
					mu->colour[3] = mu->tint[3];
				mu->txt+=2;
				continue;
			case 'd':	//reset to defaults (fixme: should reset ^m without resetting \1)
				mu->colour[0] = mu->tint[0];
				mu->colour[1] = mu->tint[1];
				mu->colour[2] = mu->tint[2];
				mu->colour[3] = mu->tint[3];
				mu->mask = 0;
				mu->txt+=2;
				break;
			case 'b':	//blink
			case 's':	//modstack push
			case 'r':	//modstack restore
				mu->txt+=2;
				continue;
			case 'x':	//RGB 12-bit colour
				if (ishex(mu->txt[2]) && ishex(mu->txt[3]) && ishex(mu->txt[4]))
				{
					mu->colour[0] = mu->tint[0] * dehex(mu->txt[2])/15.0;
					mu->colour[1] = mu->tint[1] * dehex(mu->txt[3])/15.0;
					mu->colour[2] = mu->tint[2] * dehex(mu->txt[4])/15.0;
					mu->txt+=5;
					continue;
				}
				break;	//malformed
			case '[':	//start fte's ^[text\key\value\key\value^] links
			case ']':	//end link
				break;	//fixme... skip the keys, recolour properly, etc
		//				txt+=2;
		//				continue;
			case '&':
				if ((ishex(mu->txt[2])||mu->txt[2]=='-') && (ishex(mu->txt[3])||mu->txt[3]=='-'))
				{	//ignore fte's fore/back ansi colours
					mu->txt += 4;
					continue;
				}
				break;	//malformed
			case 'a':	//alternate charset (read: masked)...
			case 'm':	//toggle masking.
				mu->txt+=2;
				mu->mask ^= 128;
				continue;
			case 'U':	//ucs-2 unicode codepoint
				if (ishex(mu->txt[2]) && ishex(mu->txt[3]) && ishex(mu->txt[4]) && ishex(mu->txt[5]))
				{
					c = (dehex(mu->txt[2])<<12) | (dehex(mu->txt[3])<<8) | (dehex(mu->txt[4])<<4) | dehex(mu->txt[5]);
					mu->txt += 6;

					if (c >= 0xe000 && c <= 0xe0ff)
						c &= 0xff;	//private-use 0xE0XX maps to quake's chars
					else if (c >= 0x20 && c <= 0x7f)
						c &= 0x7f;	//ascii is okay too.
					else
						c = '?'; //otherwise its some unicode char that we don't know how to handle.
					return c;
				}
				break; //malformed
			case '{':	//full unicode codepoint, for chars up to 0x10ffff
				mu->txt += 2;
				c = 0;	//no idea
				while(*mu->txt)
				{
					if (*mu->txt == '}')
					{
						mu->txt++;
						break;
					}
					if (!ishex(*mu->txt))
						break;
					c<<=4;
					c |= dehex(*mu->txt++);
				}

				if (c >= 0xe000 && c <= 0xe0ff)
					c &= 0xff;	//private-use 0xE0XX maps to quake's chars
				else if (c >= 0x20 && c <= 0x7f)
					c &= 0x7f;	//ascii is okay too.
				//it would be nice to include a table to de-accent latin scripts, as well as translate cyrilic somehow, but not really necessary.
				else
					c = '?'; //otherwise its some unicode char that we don't know how to handle.
				return c;
			}
		}

		//regular char
		mu->txt++;
		return c|mu->mask;
	}
	return 0;
}


static void PF_cl_getstat_int(void)
{
	int stnum = G_FLOAT(OFS_PARM0);
	if (stnum < 0 || stnum >= countof(cl.stats))
		G_INT(OFS_RETURN) = 0;
	else
		G_INT(OFS_RETURN) = cl.stats[stnum];
}
static void PF_cl_getstat_float(void)
{
	int stnum = G_FLOAT(OFS_PARM0);
	if (stnum < 0 || stnum >= countof(cl.stats))
		G_FLOAT(OFS_RETURN) = 0;
	else if (qcvm->argc > 1)
	{
		int firstbit = G_FLOAT(OFS_PARM1);
		int bitcount = G_FLOAT(OFS_PARM2);
		G_FLOAT(OFS_RETURN) = (cl.stats[stnum]>>firstbit) & ((1<<bitcount)-1);
	}
	else
		G_FLOAT(OFS_RETURN) = cl.statsf[stnum];
}
static void PF_cl_getstat_string(void)
{
	int stnum = G_FLOAT(OFS_PARM0);
	if (stnum < 0 || stnum >= countof(cl.statss) || !cl.statss[stnum])
		G_INT(OFS_RETURN) = 0;
	else
	{
		char *result = PR_GetTempString();
		q_strlcpy(result, cl.statss[stnum], STRINGTEMP_LENGTH);
		G_INT(OFS_RETURN) = PR_SetEngineString(result);
	}
}

void PF_cl_playerkey_internal(int player, const char *key, qboolean retfloat)
{
	char buf[1024];
	const char *ret = buf;
	extern int	fragsort[MAX_SCOREBOARD];
	extern int	scoreboardlines;
	extern int	Sbar_ColorForMap (int m);
	if (player < 0 && player >= -scoreboardlines)
		player = fragsort[-1-player];
	if (player < 0 || player >= MAX_SCOREBOARD)
		ret = NULL;
	else if (!strcmp(key, "viewentity"))
		q_snprintf(buf, sizeof(buf), "%i", player+1);	//hack for DP compat. always returned even when the slot is empty (so long as its valid).
	else if (!*cl.scores[player].name)
		ret = NULL;
	else if (!strcmp(key, "name"))
		ret = cl.scores[player].name;
	else if (!strcmp(key, "frags"))
		q_snprintf(buf, sizeof(buf), "%i", cl.scores[player].frags);
	else if (!strcmp(key, "ping"))
		ret = NULL;	//unknown
	else if (!strcmp(key, "pl"))
		ret = NULL;	//unknown
	else if (!strcmp(key, "entertime"))
		q_snprintf(buf, sizeof(buf), "%g", cl.scores[player].entertime);
	else if (!strcmp(key, "topcolor_rgb"))
	{
		byte *pal = (byte*)&d_8to24table[(cl.scores[player].colors & 0xf0) + 8];
		q_snprintf(buf, sizeof(buf), "%g %g %g", pal[0]/255.0, pal[1]/255.0, pal[2]/255.0);
	}
	else if (!strcmp(key, "bottomcolor_rgb"))
	{
		byte *pal = (byte*)&d_8to24table[((cl.scores[player].colors & 0x0f) << 4) + 8];
		q_snprintf(buf, sizeof(buf), "%g %g %g", pal[0]/255.0, pal[1]/255.0, pal[2]/255.0);
	}
	else if (!strcmp(key, "topcolor"))
		q_snprintf(buf, sizeof(buf), "%i", (cl.scores[player].colors & 0xf0) >> 4);
	else if (!strcmp(key, "bottomcolor"))
		q_snprintf(buf, sizeof(buf), "%i", cl.scores[player].colors & 0x0f);
	else if (!strcmp(key, "team"))	//quakeworld uses team infokeys to decide teams (instead of colours). but NQ never did, so that's fun. Lets allow mods to use either so that they can favour QW and let the engine hide differences .
		q_snprintf(buf, sizeof(buf), "%i", (cl.scores[player].colors & 0x0f)+1);
	else
		ret = NULL;

	if (retfloat)
		G_FLOAT(OFS_RETURN) = ret?atof(ret):0;
	else
		G_INT(OFS_RETURN) = ret?PR_MakeTempString(ret):0;
}
static void PF_cl_playerkey_s(void)
{
	int playernum = G_FLOAT(OFS_PARM0);
	const char *keyname = G_STRING(OFS_PARM1);
	PF_cl_playerkey_internal(playernum, keyname, false);
}
static void PF_cl_playerkey_f(void)
{
	int playernum = G_FLOAT(OFS_PARM0);
	const char *keyname = G_STRING(OFS_PARM1);
	PF_cl_playerkey_internal(playernum, keyname, true);
}


static struct
{
	char name[MAX_QPATH];
	unsigned int flags;
	qpic_t *pic;
} *qcpics;
static size_t numqcpics;
static size_t maxqcpics;
void PR_ReloadPics(qboolean purge)
{
	numqcpics = 0;

	VR_HeapFree(qcpics);
	qcpics = NULL;
	maxqcpics = 0;
}
#define PICFLAG_AUTO		0	//value used when no flags known
#define PICFLAG_WAD			(1u<<0)	//name matches that of a wad lump
//#define PICFLAG_TEMP		(1u<<1)
#define PICFLAG_WRAP		(1u<<2)	//make sure npot stuff doesn't break wrapping.
#define PICFLAG_MIPMAP		(1u<<3)	//disable use of scrap...
//#define PICFLAG_DOWNLOAD	(1u<<8)	//request to download it from the gameserver if its not stored locally.
#define PICFLAG_BLOCK		(1u<<9)	//wait until the texture is fully loaded.
#define PICFLAG_NOLOAD		(1u<<31)
static qpic_t *DrawQC_CachePic(const char *picname, unsigned int flags)
{	//okay, so this is silly. we've ended up with 3 different cache levels. qcpics, pics, and images.
	size_t i;
	unsigned int texflags;
	for (i = 0; i < numqcpics; i++)
	{	//binary search? something more sane?
		if (!strcmp(picname, qcpics[i].name))
		{
			if (qcpics[i].pic)
				return qcpics[i].pic;
			break;
		}
	}

	if (strlen(picname) >= MAX_QPATH)
		return NULL;	//too long. get lost.

	if (flags & PICFLAG_NOLOAD)
		return NULL;	//its a query, not actually needed.

	if (i+1 > maxqcpics)
	{
		maxqcpics = i + 32;
		qcpics = VR_HeapRealloc(qcpics, maxqcpics * sizeof(*qcpics));
	}

	strcpy(qcpics[i].name, picname);
	qcpics[i].flags = flags;
	qcpics[i].pic = NULL;

	texflags = TEXPREF_ALPHA | TEXPREF_PAD | TEXPREF_NOPICMIP | TEXPREF_CLAMP | TEXPREF_UNCOMPRESSED;
	if (flags & PICFLAG_WRAP)
		texflags &= ~(TEXPREF_PAD | TEXPREF_CLAMP);	//don't allow padding if its going to need to wrap (even if we don't enable clamp-to-edge normally). I just hope we have npot support.
	if (flags & PICFLAG_MIPMAP)
		texflags |= TEXPREF_MIPMAP;

	//try to load it from a wad if applicable.
	//the extra gfx/ crap is because DP insists on it for wad images. and its a nightmare to get things working in all engines if we don't accept that quirk too.
	if (flags & PICFLAG_WAD)
		qcpics[i].pic = Draw_PicFromWad2 (picname + (strncmp(picname, "gfx/", 4)?0:4), texflags);
	else if (!strncmp(picname, "gfx/", 4) && !strchr(picname+4, '.'))
		qcpics[i].pic = Draw_PicFromWad2(picname+4, texflags);

	//okay, not a wad pic, try and load a lmp/tga/etc
	if (!qcpics[i].pic)
		qcpics[i].pic = Draw_TryCachePic(picname, texflags);

	if (i == numqcpics)
		numqcpics++;

	return qcpics[i].pic;
}
static void DrawQC_CharacterQuad (float x, float y, int num, float w, float h)
{
	Draw_CharacterEx (x, y, w, h, num);
}
static void PF_cl_drawcharacter(void)
{
	float *pos	= G_VECTOR(OFS_PARM0);
	int charcode= (int)G_FLOAT (OFS_PARM1) & 0xff;
	float *size	= G_VECTOR(OFS_PARM2);
	float *rgb	= G_VECTOR(OFS_PARM3);
	float alpha	= G_FLOAT (OFS_PARM4);
//	int flags	= G_FLOAT (OFS_PARM5);

	if (charcode == 32)
		return; //don't waste time on spaces

	GL_SetCanvasColor (rgb[0], rgb[1], rgb[2], alpha);
	DrawQC_CharacterQuad (pos[0], pos[1], charcode, size[0], size[1]);
	GL_SetCanvasColor (1.f, 1.f, 1.f, 1.f);
}

static void PF_cl_drawrawstring(void)
{
	float *pos	= G_VECTOR(OFS_PARM0);
	const char *text = G_STRING (OFS_PARM1);
	float *size	= G_VECTOR(OFS_PARM2);
	float *rgb	= G_VECTOR(OFS_PARM3);
	float alpha	= G_FLOAT (OFS_PARM4);
//	int flags	= G_FLOAT (OFS_PARM5);

	float x = pos[0];
	int c;

	if (!*text)
		return; //don't waste time on spaces

	GL_SetCanvasColor (rgb[0], rgb[1], rgb[2], alpha);
	while ((c = *text++))
	{
		DrawQC_CharacterQuad (x, pos[1], c, size[0], size[1]);
		x += size[0];
	}
	GL_SetCanvasColor (1.f, 1.f, 1.f, 1.f);
}
static void PF_cl_drawstring(void)
{
	float *pos	= G_VECTOR(OFS_PARM0);
	const char *text = G_STRING (OFS_PARM1);
	float *size	= G_VECTOR(OFS_PARM2);
	float *rgb	= G_VECTOR(OFS_PARM3);
	float alpha	= G_FLOAT (OFS_PARM4);
//	int flags	= G_FLOAT (OFS_PARM5);

	float x = pos[0];
	struct markup_s mu;
	int c;

	if (!*text)
		return; //don't waste time on spaces

	PR_Markup_Begin(&mu, text, rgb, alpha);

	while ((c = PR_Markup_Parse(&mu)))
	{
		GL_SetCanvasColor (mu.colour[0], mu.colour[1], mu.colour[2], mu.colour[3]);
		DrawQC_CharacterQuad (x, pos[1], c, size[0], size[1]);
		x += size[0];
	}
	GL_SetCanvasColor (1.f, 1.f, 1.f, 1.f);
}
static void PF_cl_stringwidth(void)
{
	static const float defaultfontsize[] = {8,8};
	const char *text = G_STRING (OFS_PARM0);
	qboolean usecolours = G_FLOAT(OFS_PARM1);
	const float *fontsize = (qcvm->argc>2)?G_VECTOR (OFS_PARM2):defaultfontsize;
	struct markup_s mu;
	int r = 0;

	if (!usecolours)
		r = strlen(text);
	else
	{
		PR_Markup_Begin(&mu, text, vec3_origin, 1);
		while (PR_Markup_Parse(&mu))
		{
			r += 1;
		}
	}

	//primitive and lame, but hey.
	G_FLOAT(OFS_RETURN) = fontsize[0] * r;
}


static void PF_cl_drawsetclip(void)
{
	float x = G_FLOAT(OFS_PARM0);
	float y = G_FLOAT(OFS_PARM1);
	float w = G_FLOAT(OFS_PARM2);
	float h = G_FLOAT(OFS_PARM3);
	Draw_SetClipRect (x, y, w, h);
}
static void PF_cl_drawresetclip(void)
{
	Draw_ResetClipping ();
}

static void PF_cl_precachepic(void)
{
	const char *name	= G_STRING(OFS_PARM0);
	unsigned int flags = G_FLOAT(OFS_PARM1);

	G_INT(OFS_RETURN) = G_INT(OFS_PARM0);	//return input string, for convienience

	if (!DrawQC_CachePic(name, flags) && (flags & PICFLAG_BLOCK))
		G_INT(OFS_RETURN) = 0;	//return input string, for convienience
}
static void PF_cl_iscachedpic(void)
{
	const char *name	= G_STRING(OFS_PARM0);
	if (DrawQC_CachePic(name, PICFLAG_NOLOAD))
		G_FLOAT(OFS_RETURN) = true;
	else
		G_FLOAT(OFS_RETURN) = false;
}

static void PF_cl_drawpic(void)
{
	float *pos	= G_VECTOR(OFS_PARM0);
	qpic_t *pic	= DrawQC_CachePic(G_STRING(OFS_PARM1), PICFLAG_AUTO);
	float *size	= G_VECTOR(OFS_PARM2);
	float *rgb	= G_VECTOR(OFS_PARM3);
	float alpha	= G_FLOAT (OFS_PARM4);
//	int flags	= G_FLOAT (OFS_PARM5);

	if (pic)
		Draw_SubPic (pos[0], pos[1], size[0], size[1], pic, 0, 0, 1, 1, rgb, alpha);
}

static void PF_cl_getimagesize(void)
{
	qpic_t *pic	= DrawQC_CachePic(G_STRING(OFS_PARM0), PICFLAG_AUTO);
	if (pic)
		G_VECTORSET(OFS_RETURN, pic->width, pic->height, 0);
	else
		G_VECTORSET(OFS_RETURN, 0, 0, 0);
}

static void PF_cl_drawsubpic(void)
{
	float *pos	= G_VECTOR(OFS_PARM0);
	float *size	= G_VECTOR(OFS_PARM1);
	qpic_t *pic	= DrawQC_CachePic(G_STRING(OFS_PARM2), PICFLAG_AUTO);
	float *srcpos	= G_VECTOR(OFS_PARM3);
	float *srcsize	= G_VECTOR(OFS_PARM4);
	float *rgb	= G_VECTOR(OFS_PARM5);
	float alpha	= G_FLOAT (OFS_PARM6);
//	int flags	= G_FLOAT (OFS_PARM7);

	if (pic)
		Draw_SubPic (pos[0], pos[1], size[0], size[1], pic, srcpos[0], srcpos[1], srcsize[0], srcsize[1], rgb, alpha);
}

static void PF_cl_drawfill(void)
{
	float *pos	= G_VECTOR(OFS_PARM0);
	float *size	= G_VECTOR(OFS_PARM1);
	float *rgb	= G_VECTOR(OFS_PARM2);
	float alpha	= G_FLOAT (OFS_PARM3);
//	int flags	= G_FLOAT (OFS_PARM4);

	Draw_FillEx (pos[0], pos[1], size[0], size[1], rgb, alpha);
}

//maths stuff
static void PF_Sin(void)
{
	G_FLOAT(OFS_RETURN) = sin(G_FLOAT(OFS_PARM0));
}
static void PF_asin(void)
{
	G_FLOAT(OFS_RETURN) = asin(G_FLOAT(OFS_PARM0));
}
static void PF_Cos(void)
{
	G_FLOAT(OFS_RETURN) = cos(G_FLOAT(OFS_PARM0));
}
static void PF_acos(void)
{
	G_FLOAT(OFS_RETURN) = acos(G_FLOAT(OFS_PARM0));
}
static void PF_tan(void)
{
	G_FLOAT(OFS_RETURN) = tan(G_FLOAT(OFS_PARM0));
}
static void PF_atan(void)
{
	G_FLOAT(OFS_RETURN) = atan(G_FLOAT(OFS_PARM0));
}
static void PF_atan2(void)
{
	G_FLOAT(OFS_RETURN) = atan2(G_FLOAT(OFS_PARM0), G_FLOAT(OFS_PARM1));
}
static void PF_Sqrt(void)
{
	G_FLOAT(OFS_RETURN) = sqrt(G_FLOAT(OFS_PARM0));
}
static void PF_pow(void)
{
	G_FLOAT(OFS_RETURN) = pow(G_FLOAT(OFS_PARM0), G_FLOAT(OFS_PARM1));
}
static void PF_mod(void)
{
	float a = G_FLOAT(OFS_PARM0);
	float n = G_FLOAT(OFS_PARM1);

	if (n == 0)
	{
		Con_DWarning("PF_mod: mod by zero\n");
		G_FLOAT(OFS_RETURN) = 0;
	}
	else
	{
		//because QC is inherantly floaty, lets use floats.
		G_FLOAT(OFS_RETURN) = a - (n * (int)(a/n));
	}
}
static void PF_min(void)
{
	float r = G_FLOAT(OFS_PARM0);
	int i;
	for (i = 1; i < qcvm->argc; i++)
	{
		if (r > G_FLOAT(OFS_PARM0 + i*3))
			r = G_FLOAT(OFS_PARM0 + i*3);
	}
	G_FLOAT(OFS_RETURN) = r;
}
static void PF_max(void)
{
	float r = G_FLOAT(OFS_PARM0);
	int i;
	for (i = 1; i < qcvm->argc; i++)
	{
		if (r < G_FLOAT(OFS_PARM0 + i*3))
			r = G_FLOAT(OFS_PARM0 + i*3);
	}
	G_FLOAT(OFS_RETURN) = r;
}
static void PF_bound(void)
{
	float minval = G_FLOAT(OFS_PARM0);
	float curval = G_FLOAT(OFS_PARM1);
	float maxval = G_FLOAT(OFS_PARM2);
	if (curval > maxval)
		curval = maxval;
	if (curval < minval)
		curval = minval;
	G_FLOAT(OFS_RETURN) = curval;
}
static void PF_vectorvectors(void)
{
	VectorCopy(G_VECTOR(OFS_PARM0), pr_global_struct->v_forward);
	VectorNormalize(pr_global_struct->v_forward);
	if (!pr_global_struct->v_forward[0] && !pr_global_struct->v_forward[1])
	{
		if (pr_global_struct->v_forward[2])
			pr_global_struct->v_right[1] = -1;
		else
			pr_global_struct->v_right[1] = 0;
		pr_global_struct->v_right[0] = pr_global_struct->v_right[2] = 0;
	}
	else
	{
		pr_global_struct->v_right[0] = pr_global_struct->v_forward[1];
		pr_global_struct->v_right[1] = -pr_global_struct->v_forward[0];
		pr_global_struct->v_right[2] = 0;
		VectorNormalize(pr_global_struct->v_right);
	}
	CrossProduct(pr_global_struct->v_right, pr_global_struct->v_forward, pr_global_struct->v_up);
}

//string stuff
static void PF_strlen(void)
{	//FIXME: doesn't try to handle utf-8
	const char *s = G_STRING(OFS_PARM0);
	G_FLOAT(OFS_RETURN) = strlen(s);
}
// QVR: float(string str, string sub, optional float startpos) strstrofs = #221 (FTE_STRINGS, DP's semantics): the byte
// offset of `sub` in `str` at or after `startpos`, -1 when it isn't there or `startpos` is outside the string; an empty
// `sub` matches at `startpos`. It was missing: a `strstrofs = #0` declaration (AD's) bound to nothing and returned 0.
static void PF_strstrofs(void)
{
	const char *str = G_STRING(OFS_PARM0);
	const char *sub = G_STRING(OFS_PARM1);
	const int first = (qcvm->argc > 2) ? (int)G_FLOAT(OFS_PARM2) : 0;
	const char *at;
	if (first < 0 || first > (int)strlen(str))
	{
		G_FLOAT(OFS_RETURN) = -1;
		return;
	}
	at = strstr(str + first, sub);
	G_FLOAT(OFS_RETURN) = at ? (float)(at - str) : -1;
}
static void PF_strcat(void)
{
	int		i;
	char *out = PR_GetTempString();
	size_t s;

	out[0] = 0;
	s = 0;
	for (i = 0; i < qcvm->argc; i++)
	{
		s = q_strlcat(out, G_STRING((OFS_PARM0+i*3)), STRINGTEMP_LENGTH);
		if (s >= STRINGTEMP_LENGTH)
		{
			Con_Warning("PF_strcat: overflow (string truncated)\n");
			break;
		}
	}

	G_INT(OFS_RETURN) = PR_SetEngineString(out);
}
static void PF_substring(void)
{
	int start, length, slen;
	const char *s;
	char *string;

	s = G_STRING(OFS_PARM0);
	start = G_FLOAT(OFS_PARM1);
	length = G_FLOAT(OFS_PARM2);

	slen = strlen(s);	//utf-8 should use chars, not bytes.

	if (start < 0)
		start = slen+start;
	if (length < 0)
		length = slen-start+(length+1);
	if (start < 0)
	{
	//	length += start;
		start = 0;
	}

	if (start >= slen || length<=0)
	{
		G_INT(OFS_RETURN) = PR_SetEngineString("");
		return;
	}

	slen -= start;
	if (length > slen)
		length = slen;
	//utf-8 should switch to bytes now.
	s += start;

	if (length >= STRINGTEMP_LENGTH)
	{
		length = STRINGTEMP_LENGTH-1;
		Con_Warning("PF_substring: truncation\n");
	}

	string = PR_GetTempString();
	memcpy(string, s, length);
	string[length] = '\0';
	G_INT(OFS_RETURN) = PR_SetEngineString(string);
}

/*our zoned strings implementation is somewhat specific to quakespasm, so good luck porting*/
static void PF_strzone(void)
{
	char *buf;
	size_t len = 0;
	const char *s[8];
	size_t l[8];
	int i;
	size_t id;

	for (i = 0; i < qcvm->argc; i++)
	{
		s[i] = G_STRING(OFS_PARM0+i*3);
		l[i] = strlen(s[i]);
		len += l[i];
	}
	len++; /*for the null*/

	buf = Z_Malloc(len);
	G_INT(OFS_RETURN) = PR_SetEngineString(buf);
	id = -1-G_INT(OFS_RETURN);
	if (id >= qcvm->knownzonesize)
	{
		qcvm->knownzonesize = (id+32)&~7;
		qcvm->knownzone = Z_Realloc(qcvm->knownzone, (qcvm->knownzonesize+7)>>3);
	}
	qcvm->knownzone[id>>3] |= 1u<<(id&7);

	for (i = 0; i < qcvm->argc; i++)
	{
		memcpy(buf, s[i], l[i]);
		buf += l[i];
	}
	*buf = '\0';
}
static void PF_strunzone(void)
{
	size_t id;
	const char *foo = G_STRING(OFS_PARM0);

	if (!G_INT(OFS_PARM0))
		return;	//don't bug out if they gave a null string
	id = -1-G_INT(OFS_PARM0);
	if (id < qcvm->knownzonesize && (qcvm->knownzone[id>>3] & (1u<<(id&7))))
	{
		qcvm->knownzone[id>>3] &= ~(1u<<(id&7));
		PR_ClearEngineString(G_INT(OFS_PARM0));
		Z_Free((void*)foo);
	}
	else
		Con_Warning("PF_strunzone: string wasn't strzoned\n");
}

static qboolean qc_isascii(unsigned int u)
{	
	if (u < 256)	//should be just \n and 32-127, but we don't actually support any actual unicode and we don't really want to make things worse.
		return true;
	return false;
}
static void PF_str2chr(void)
{
	const char *instr = G_STRING(OFS_PARM0);
	int ofs = (qcvm->argc>1)?G_FLOAT(OFS_PARM1):0;

	if (ofs < 0)
		ofs = strlen(instr)+ofs;

	if (ofs && (ofs < 0 || ofs > (int)strlen(instr)))
		G_FLOAT(OFS_RETURN) = '\0';
	else
		G_FLOAT(OFS_RETURN) = (unsigned char)instr[ofs];
}
static void PF_chr2str(void)
{
	char *ret = PR_GetTempString(), *out;
	int i;
	for (i = 0, out=ret; out-ret < STRINGTEMP_LENGTH-6 && i < qcvm->argc; i++)
	{
		unsigned int u = G_FLOAT(OFS_PARM0 + i*3);
		if (u >= 0xe000 && u < 0xe100)
			*out++ = (unsigned char)u;	//quake chars.
		else if (qc_isascii(u))
			*out++ = u;
		else
			*out++ = '?';	//no unicode support
	}
	*out = 0;
	G_INT(OFS_RETURN) = PR_SetEngineString(ret);
}

//part of PF_strconv
static int chrconv_number(int i, int base, int conv)
{
	i -= base;
	switch (conv)
	{
	default:
	case 5:
	case 6:
	case 0:
		break;
	case 1:
		base = '0';
		break;
	case 2:
		base = '0'+128;
		break;
	case 3:
		base = '0'-30;
		break;
	case 4:
		base = '0'+128-30;
		break;
	}
	return i + base;
}
//part of PF_strconv
static int chrconv_punct(int i, int base, int conv)
{
	i -= base;
	switch (conv)
	{
	default:
	case 0:
		break;
	case 1:
		base = 0;
		break;
	case 2:
		base = 128;
		break;
	}
	return i + base;
}
//part of PF_strconv
static int chrchar_alpha(int i, int basec, int baset, int convc, int convt, int charnum)
{
	//convert case and colour seperatly...

	i -= baset + basec;
	switch (convt)
	{
	default:
	case 0:
		break;
	case 1:
		baset = 0;
		break;
	case 2:
		baset = 128;
		break;

	case 5:
	case 6:
		baset = 128*((charnum&1) == (convt-5));
		break;
	}

	switch (convc)
	{
	default:
	case 0:
		break;
	case 1:
		basec = 'a';
		break;
	case 2:
		basec = 'A';
		break;
	}
	return i + basec + baset;
}
//FTE_STRINGS
//bulk convert a string. change case or colouring.
static void PF_strconv (void)
{
	int ccase = G_FLOAT(OFS_PARM0);		//0 same, 1 lower, 2 upper
	int redalpha = G_FLOAT(OFS_PARM1);	//0 same, 1 white, 2 red,  5 alternate, 6 alternate-alternate
	int rednum = G_FLOAT(OFS_PARM2);	//0 same, 1 white, 2 red, 3 redspecial, 4 whitespecial, 5 alternate, 6 alternate-alternate
	const unsigned char *string = (const unsigned char*)PF_VarString(3);
	int len = strlen((const char*)string);
	int i;
	unsigned char *resbuf = (unsigned char*)PR_GetTempString();
	unsigned char *result = resbuf;

	//UTF-8-FIXME: cope with utf+^U etc

	if (len >= STRINGTEMP_LENGTH)
		len = STRINGTEMP_LENGTH-1;

	for (i = 0; i < len; i++, string++, result++)	//should this be done backwards?
	{
		if (*string >= '0' && *string <= '9')	//normal numbers...
			*result = chrconv_number(*string, '0', rednum);
		else if (*string >= '0'+128 && *string <= '9'+128)
			*result = chrconv_number(*string, '0'+128, rednum);
		else if (*string >= '0'+128-30 && *string <= '9'+128-30)
			*result = chrconv_number(*string, '0'+128-30, rednum);
		else if (*string >= '0'-30 && *string <= '9'-30)
			*result = chrconv_number(*string, '0'-30, rednum);

		else if (*string >= 'a' && *string <= 'z')	//normal numbers...
			*result = chrchar_alpha(*string, 'a', 0, ccase, redalpha, i);
		else if (*string >= 'A' && *string <= 'Z')	//normal numbers...
			*result = chrchar_alpha(*string, 'A', 0, ccase, redalpha, i);
		else if (*string >= 'a'+128 && *string <= 'z'+128)	//normal numbers...
			*result = chrchar_alpha(*string, 'a', 128, ccase, redalpha, i);
		else if (*string >= 'A'+128 && *string <= 'Z'+128)	//normal numbers...
			*result = chrchar_alpha(*string, 'A', 128, ccase, redalpha, i);

		else if ((*string & 127) < 16 || !redalpha)	//special chars..
			*result = *string;
		else if (*string < 128)
			*result = chrconv_punct(*string, 0, redalpha);
		else
			*result = chrconv_punct(*string, 128, redalpha);
	}
	*result = '\0';

	G_INT(OFS_RETURN) = PR_SetEngineString((char*)resbuf);
}

static void PF_sprintf_internal (const char *s, int firstarg, char *outbuf, int outbuflen)
{
	const char *s0;
	char *o = outbuf, *end = outbuf + outbuflen, *err;
	int width, precision, thisarg, flags;
	char formatbuf[16];
	char *f;
	int argpos = firstarg;
	int isfloat;
	static int dummyivec[3] = {0, 0, 0};
	static float dummyvec[3] = {0, 0, 0};

#define PRINTF_ALTERNATE 1
#define PRINTF_ZEROPAD 2
#define PRINTF_LEFT 4
#define PRINTF_SPACEPOSITIVE 8
#define PRINTF_SIGNPOSITIVE 16

	formatbuf[0] = '%';

#define GETARG_FLOAT(a) (((a)>=firstarg && (a)<qcvm->argc) ? (G_FLOAT(OFS_PARM0 + 3 * (a))) : 0)
#define GETARG_VECTOR(a) (((a)>=firstarg && (a)<qcvm->argc) ? (G_VECTOR(OFS_PARM0 + 3 * (a))) : dummyvec)
#define GETARG_INT(a) (((a)>=firstarg && (a)<qcvm->argc) ? (G_INT(OFS_PARM0 + 3 * (a))) : 0)
#define GETARG_INTVECTOR(a) (((a)>=firstarg && (a)<qcvm->argc) ? ((int*) G_VECTOR(OFS_PARM0 + 3 * (a))) : dummyivec)
#define GETARG_STRING(a) (((a)>=firstarg && (a)<qcvm->argc) ? (G_STRING(OFS_PARM0 + 3 * (a))) : "")

	for(;;)
	{
		s0 = s;
		switch(*s)
		{
			case 0:
				goto finished;
			case '%':
				++s;

				if(*s == '%')
					goto verbatim;

				// complete directive format:
				// %3$*1$.*2$ld
				
				width = -1;
				precision = -1;
				thisarg = -1;
				flags = 0;
				isfloat = -1;

				// is number following?
				if(*s >= '0' && *s <= '9')
				{
					width = strtol(s, &err, 10);
					if(!err)
					{
						Con_Warning("PF_sprintf: bad format string: %s\n", s0);
						goto finished;
					}
					if(*err == '$')
					{
						thisarg = width + (firstarg-1);
						width = -1;
						s = err + 1;
					}
					else
					{
						if(*s == '0')
						{
							flags |= PRINTF_ZEROPAD;
							if(width == 0)
								width = -1; // it was just a flag
						}
						s = err;
					}
				}

				if(width < 0)
				{
					for(;;)
					{
						switch(*s)
						{
							case '#': flags |= PRINTF_ALTERNATE; break;
							case '0': flags |= PRINTF_ZEROPAD; break;
							case '-': flags |= PRINTF_LEFT; break;
							case ' ': flags |= PRINTF_SPACEPOSITIVE; break;
							case '+': flags |= PRINTF_SIGNPOSITIVE; break;
							default:
								goto noflags;
						}
						++s;
					}
noflags:
					if(*s == '*')
					{
						++s;
						if(*s >= '0' && *s <= '9')
						{
							width = strtol(s, &err, 10);
							if(!err || *err != '$')
							{
								Con_Warning("PF_sprintf: invalid format string: %s\n", s0);
								goto finished;
							}
							s = err + 1;
						}
						else
							width = argpos++;
						width = GETARG_FLOAT(width);
						if(width < 0)
						{
							flags |= PRINTF_LEFT;
							width = -width;
						}
					}
					else if(*s >= '0' && *s <= '9')
					{
						width = strtol(s, &err, 10);
						if(!err)
						{
							Con_Warning("PF_sprintf: invalid format string: %s\n", s0);
							goto finished;
						}
						s = err;
						if(width < 0)
						{
							flags |= PRINTF_LEFT;
							width = -width;
						}
					}
					// otherwise width stays -1
				}

				if(*s == '.')
				{
					++s;
					if(*s == '*')
					{
						++s;
						if(*s >= '0' && *s <= '9')
						{
							precision = strtol(s, &err, 10);
							if(!err || *err != '$')
							{
								Con_Warning("PF_sprintf: invalid format string: %s\n", s0);
								goto finished;
							}
							s = err + 1;
						}
						else
							precision = argpos++;
						precision = GETARG_FLOAT(precision);
					}
					else if(*s >= '0' && *s <= '9')
					{
						precision = strtol(s, &err, 10);
						if(!err)
						{
							Con_Warning("PF_sprintf: invalid format string: %s\n", s0);
							goto finished;
						}
						s = err;
					}
					else
					{
						Con_Warning("PF_sprintf: invalid format string: %s\n", s0);
						goto finished;
					}
				}

				for(;;)
				{
					switch(*s)
					{
						case 'h': isfloat = 1; break;
						case 'l': isfloat = 0; break;
						case 'L': isfloat = 0; break;
						case 'j': break;
						case 'z': break;
						case 't': break;
						default:
							goto nolength;
					}
					++s;
				}
nolength:

				// now s points to the final directive char and is no longer changed
				if (*s == 'p' || *s == 'P')
				{
					//%p is slightly different from %x.
					//always 8-bytes wide with 0 padding, always ints.
					flags |= PRINTF_ZEROPAD;
					if (width < 0) width = 8;
					if (isfloat < 0) isfloat = 0;
				}
				else if (*s == 'i')
				{
					//%i defaults to ints, not floats.
					if(isfloat < 0) isfloat = 0;
				}

				//assume floats, not ints.
				if(isfloat < 0)
					isfloat = 1;

				if(thisarg < 0)
					thisarg = argpos++;

				if(o < end - 1)
				{
					f = &formatbuf[1];
					if(*s != 's' && *s != 'c')
						if(flags & PRINTF_ALTERNATE) *f++ = '#';
					if(flags & PRINTF_ZEROPAD) *f++ = '0';
					if(flags & PRINTF_LEFT) *f++ = '-';
					if(flags & PRINTF_SPACEPOSITIVE) *f++ = ' ';
					if(flags & PRINTF_SIGNPOSITIVE) *f++ = '+';
					*f++ = '*';
					if(precision >= 0)
					{
						*f++ = '.';
						*f++ = '*';
					}
					if (*s == 'p')
						*f++ = 'x';
					else if (*s == 'P')
						*f++ = 'X';
					else if (*s == 'S')
						*f++ = 's';
					else
						*f++ = *s;
					*f++ = 0;

					if(width < 0) // not set
						width = 0;

					switch(*s)
					{
						case 'd': case 'i':
							if(precision < 0) // not set
								q_snprintf(o, end - o, formatbuf, width, (isfloat ? (int) GETARG_FLOAT(thisarg) : (int) GETARG_INT(thisarg)));
							else
								q_snprintf(o, end - o, formatbuf, width, precision, (isfloat ? (int) GETARG_FLOAT(thisarg) : (int) GETARG_INT(thisarg)));
							o += strlen(o);
							break;
						case 'o': case 'u': case 'x': case 'X': case 'p': case 'P':
							if(precision < 0) // not set
								q_snprintf(o, end - o, formatbuf, width, (isfloat ? (unsigned int) GETARG_FLOAT(thisarg) : (unsigned int) GETARG_INT(thisarg)));
							else
								q_snprintf(o, end - o, formatbuf, width, precision, (isfloat ? (unsigned int) GETARG_FLOAT(thisarg) : (unsigned int) GETARG_INT(thisarg)));
							o += strlen(o);
							break;
						case 'e': case 'E': case 'f': case 'F': case 'g': case 'G':
							if(precision < 0) // not set
								q_snprintf(o, end - o, formatbuf, width, (isfloat ? (double) GETARG_FLOAT(thisarg) : (double) GETARG_INT(thisarg)));
							else
								q_snprintf(o, end - o, formatbuf, width, precision, (isfloat ? (double) GETARG_FLOAT(thisarg) : (double) GETARG_INT(thisarg)));
							o += strlen(o);
							break;
						case 'v': case 'V':
							f[-2] += 'g' - 'v';
							if(precision < 0) // not set
								q_snprintf(o, end - o, va("%s %s %s", /* NESTED SPRINTF IS NESTED */ formatbuf, formatbuf, formatbuf),
									width, (isfloat ? (double) GETARG_VECTOR(thisarg)[0] : (double) GETARG_INTVECTOR(thisarg)[0]),
									width, (isfloat ? (double) GETARG_VECTOR(thisarg)[1] : (double) GETARG_INTVECTOR(thisarg)[1]),
									width, (isfloat ? (double) GETARG_VECTOR(thisarg)[2] : (double) GETARG_INTVECTOR(thisarg)[2])
								);
							else
								q_snprintf(o, end - o, va("%s %s %s", /* NESTED SPRINTF IS NESTED */ formatbuf, formatbuf, formatbuf),
									width, precision, (isfloat ? (double) GETARG_VECTOR(thisarg)[0] : (double) GETARG_INTVECTOR(thisarg)[0]),
									width, precision, (isfloat ? (double) GETARG_VECTOR(thisarg)[1] : (double) GETARG_INTVECTOR(thisarg)[1]),
									width, precision, (isfloat ? (double) GETARG_VECTOR(thisarg)[2] : (double) GETARG_INTVECTOR(thisarg)[2])
								);
							o += strlen(o);
							break;
						case 'c':
							//UTF-8-FIXME: figure it out yourself
//							if(flags & PRINTF_ALTERNATE)
							{
								if(precision < 0) // not set
									q_snprintf(o, end - o, formatbuf, width, (isfloat ? (unsigned int) GETARG_FLOAT(thisarg) : (unsigned int) GETARG_INT(thisarg)));
								else
									q_snprintf(o, end - o, formatbuf, width, precision, (isfloat ? (unsigned int) GETARG_FLOAT(thisarg) : (unsigned int) GETARG_INT(thisarg)));
								o += strlen(o);
							}
/*							else
							{
								unsigned int c = (isfloat ? (unsigned int) GETARG_FLOAT(thisarg) : (unsigned int) GETARG_INT(thisarg));
								char charbuf16[16];
								const char *buf = u8_encodech(c, NULL, charbuf16);
								if(!buf)
									buf = "";
								if(precision < 0) // not set
									precision = end - o - 1;
								o += u8_strpad(o, end - o, buf, (flags & PRINTF_LEFT) != 0, width, precision);
							}
*/							break;
						case 'S':
							{	//tokenizable string
								const char *quotedarg = GETARG_STRING(thisarg);

								//try and escape it... hopefully it won't get truncated by precision limits...
								char quotedbuf[65536];
								size_t l;
								l = strlen(quotedarg);
								if (strchr(quotedarg, '\"') || strchr(quotedarg, '\n') || strchr(quotedarg, '\r') || l+3 >= sizeof(quotedbuf))
								{	//our escapes suck...
									Con_Warning("PF_sprintf: unable to safely escape arg: %s\n", s0);
									quotedarg="";
								}
								quotedbuf[0] = '\"';
								memcpy(quotedbuf+1, quotedarg, l);
								quotedbuf[1+l] = '\"';
								quotedbuf[1+l+1] = 0;
								quotedarg = quotedbuf;

								//UTF-8-FIXME: figure it out yourself
//								if(flags & PRINTF_ALTERNATE)
								{
									if(precision < 0) // not set
										q_snprintf(o, end - o, formatbuf, width, quotedarg);
									else
										q_snprintf(o, end - o, formatbuf, width, precision, quotedarg);
									o += strlen(o);
								}
/*								else
								{
									if(precision < 0) // not set
										precision = end - o - 1;
									o += u8_strpad(o, end - o, quotedarg, (flags & PRINTF_LEFT) != 0, width, precision);
								}
*/							}
							break;
						case 's':
							//UTF-8-FIXME: figure it out yourself
//							if(flags & PRINTF_ALTERNATE)
							{
								if(precision < 0) // not set
									q_snprintf(o, end - o, formatbuf, width, GETARG_STRING(thisarg));
								else
									q_snprintf(o, end - o, formatbuf, width, precision, GETARG_STRING(thisarg));
								o += strlen(o);
							}
/*							else
							{
								if(precision < 0) // not set
									precision = end - o - 1;
								o += u8_strpad(o, end - o, GETARG_STRING(thisarg), (flags & PRINTF_LEFT) != 0, width, precision);
							}
*/							break;
						default:
							Con_Warning("PF_sprintf: invalid format string: %s\n", s0);
							goto finished;
					}
				}
				++s;
				break;
			default:
verbatim:
				if(o < end - 1)
					*o++ = *s;
				s++;
				break;
		}
	}
finished:
	*o = 0;
}

static void PF_sprintf(void)
{
	char *outbuf = PR_GetTempString();
	PF_sprintf_internal(G_STRING(OFS_PARM0), 1, outbuf, STRINGTEMP_LENGTH);
	G_INT(OFS_RETURN) = PR_SetEngineString(outbuf);
}

//string tokenizing (gah)
#define MAXQCTOKENS 64
static struct {
	char *token;
	unsigned int start;
	unsigned int end;
} qctoken[MAXQCTOKENS];
static unsigned int qctoken_count;

static void PF_ArgC(void)
{
	G_FLOAT(OFS_RETURN) = qctoken_count;
}

static int tokenizeqc(const char *str, qboolean dpfuckage)
{
	//FIXME: if dpfuckage, then we should handle punctuation specially, as well as /*.
	const char *start = str;
	while(qctoken_count > 0)
	{
		qctoken_count--;
		VR_HeapFree(qctoken[qctoken_count].token);
	}
	qctoken_count = 0;
	while (qctoken_count < MAXQCTOKENS)
	{
		/*skip whitespace here so the token's start is accurate*/
		while (*str && *(const unsigned char*)str <= ' ')
			str++;

		if (!*str)
			break;

		qctoken[qctoken_count].start = str - start;
//		if (dpfuckage)
//			str = COM_ParseDPFuckage(str);
//		else
			str = COM_Parse(str);
		if (!str)
			break;

		qctoken[qctoken_count].token = strdup(com_token);

		qctoken[qctoken_count].end = str - start;
		qctoken_count++;
	}
	return qctoken_count;
}

/*KRIMZON_SV_PARSECLIENTCOMMAND added these two - note that for compatibility with DP, this tokenize builtin is veeery vauge and doesn't match the console*/
static void PF_Tokenize(void)
{
	G_FLOAT(OFS_RETURN) = tokenizeqc(G_STRING(OFS_PARM0), true);
}

static void PF_tokenize_console(void)
{
	G_FLOAT(OFS_RETURN) = tokenizeqc(G_STRING(OFS_PARM0), false);
}

static void PF_ArgV(void)
{
	int idx = G_FLOAT(OFS_PARM0);

	/*negative indexes are relative to the end*/
	if (idx < 0)
		idx += qctoken_count;

	if ((unsigned int)idx >= qctoken_count)
		G_INT(OFS_RETURN) = 0;
	else
	{
		char *ret = PR_GetTempString();
		q_strlcpy(ret, qctoken[idx].token, STRINGTEMP_LENGTH);
		G_INT(OFS_RETURN) = PR_SetEngineString(ret);
	}
}

//conversions (mostly string)
static void PF_stof(void)
{
	G_FLOAT(OFS_RETURN) = atof(G_STRING(OFS_PARM0));
}
static void PF_stov(void)
{
	const char *s = G_STRING(OFS_PARM0);
	s = COM_Parse(s);
	G_VECTOR(OFS_RETURN)[0] = atof(com_token);
	s = COM_Parse(s);
	G_VECTOR(OFS_RETURN)[1] = atof(com_token);
	s = COM_Parse(s);
	G_VECTOR(OFS_RETURN)[2] = atof(com_token);
}
static void PF_etos(void)
{	//yes, this is lame
	char *result = PR_GetTempString();
	q_snprintf(result, STRINGTEMP_LENGTH, "entity %i", G_EDICTNUM(OFS_PARM0));
	G_INT(OFS_RETURN) = PR_SetEngineString(result);
}
static void PF_ftoi(void)
{
	G_INT(OFS_RETURN) = G_FLOAT(OFS_PARM0);
}
static void PF_itof(void)
{
	G_FLOAT(OFS_RETURN) = G_INT(OFS_PARM0);
}

static void PF_cl_registercommand(void)
{
	const char *cmdname = G_STRING(OFS_PARM0);
	Cmd_AddCommand(cmdname, NULL);
}

static struct svcustomstat_s *PR_CustomStat(int idx, int type)
{
	size_t i;
	if (idx < 0 || idx >= MAX_CL_STATS)
		return NULL;
	switch(type)
	{
	case ev_ext_integer:
	case ev_float:
	case ev_vector:
	case ev_entity:
		break;
	default:
		return NULL;
	}

	for (i = 0; i < sv.numcustomstats; i++)
	{
		if (sv.customstats[i].idx == idx && (sv.customstats[i].type==ev_string) == (type==ev_string))
			break;
	}
	if (i == sv.numcustomstats)
		sv.numcustomstats++;
	sv.customstats[i].idx = idx;
	sv.customstats[i].type = type;
	sv.customstats[i].fld = 0;
	sv.customstats[i].ptr = NULL;
	return &sv.customstats[i];
}
static void PF_clientstat(void)
{
	int idx = G_FLOAT(OFS_PARM0);
	int type = G_FLOAT(OFS_PARM1);
	int fldofs = G_INT(OFS_PARM2);
	struct svcustomstat_s *stat = PR_CustomStat(idx, type);
	if (!stat)
		return;
	stat->fld = fldofs;
}

/*
===============================================================================

QVR: AD support: the DP/FTE builtins Arcane Dimensions' QuakeC imports by name (`= #0`, QC/ad/ad_builtins.qc) that
the engine had none of (PR_WarnUnboundBuiltins named them; each returned 0). AD reaches them only on its extension
path (ad_world.qc: VR_AD_Extensions, the bridge's FALSE today, and checkextension denies every extension AD asks
for), so in Quake VR they change nothing yet; they follow DP's/FTE's semantics for any progs that calls them.
`addstat` is DP's name for clientstat (#232, bound by name only: the table's numbers are unique).

===============================================================================
*/

// float(float number, float quantity) bitshift = #218 (EXT_BITSHIFT, FTE's semantics): `number` truncated to an
// int, shifted left by `quantity` (right, arithmetic, when negative). A shift of 32 or more gives 0 (-1 when a
// negative number shifts right) instead of C's undefined behaviour.
static void PF_bitshift (void)
{
	const int number = (int)G_FLOAT(OFS_PARM0);
	const int quantity = (int)G_FLOAT(OFS_PARM1);
	int result;
	if (quantity >= 32)
		result = 0;
	else if (quantity >= 0)
		result = (int)((unsigned int)number << quantity);
	else if (quantity > -32)
		result = number >> -quantity;
	else
		result = number < 0 ? -1 : 0;
	G_FLOAT(OFS_RETURN) = result;
}

// float(string s1, string s2, float len, optional float s1ofs, optional float s2ofs) strncasecmp = #230
// (FTE_STRINGS): compares at most `len` characters ignoring ASCII case, from the optional offsets (clamped to the
// strings); -1, 0 or 1. AD: its sky test (ad_subs_soc.qc ad_check_skycontent) matches "SKY" texture names.
static void PF_strncasecmp (void)
{
	const char *s1 = G_STRING(OFS_PARM0);
	const char *s2 = G_STRING(OFS_PARM1);
	const int len = (int)G_FLOAT(OFS_PARM2);
	int r;
	if (qcvm->argc > 3)
	{
		const int ofs = (int)G_FLOAT(OFS_PARM3);
		const int n = (int)strlen(s1);
		s1 += ofs < 0 ? 0 : ofs > n ? n : ofs;
	}
	if (qcvm->argc > 4)
	{
		const int ofs = (int)G_FLOAT(OFS_PARM4);
		const int n = (int)strlen(s2);
		s2 += ofs < 0 ? 0 : ofs > n ? n : ofs;
	}
	r = len > 0 ? q_strncasecmp(s1, s2, (size_t)len) : 0;
	G_FLOAT(OFS_RETURN) = r < 0 ? -1 : r > 0 ? 1 : 0;
}

// The brush model an entity's surface queries look at (DP's getmodel): NULL when it has none.
static qmodel_t *PF_SurfaceModel (edict_t *ed)
{
	const int index = (int)ed->v.modelindex;
	qmodel_t *mod;
	if (ed->free || index <= 0 || index >= MAX_MODELS)
		return NULL;
	mod = sv.models[index];
	return (mod && mod->type == mod_brush && mod->surfaces) ? mod : NULL;
}

// A brush model surface's vertex `i` (its edge loop, model->surfedges).
static const float *PF_SurfaceVertex (const qmodel_t *mod, const msurface_t *surf, int i)
{
	const int lindex = mod->surfedges[surf->firstedge + i];
	const medge_t *edge = &mod->edges[lindex >= 0 ? lindex : -lindex];
	return mod->vertexes[lindex >= 0 ? edge->v[0] : edge->v[1]].position;
}

// The squared distance from p to the surface's polygon (convex, on its plane): to the projection when it lies
// inside every edge, else to the nearest edge segment.
static float PF_SurfaceDistance2 (const qmodel_t *mod, const msurface_t *surf, const vec3_t p)
{
	const float *n = surf->plane->normal;
	const float planedist = DotProduct(p, n) - surf->plane->dist;
	vec3_t q, e, d, c;
	float best = 1e30f;
	qboolean inside = true;
	int i, sign = 0;
	VectorMA(p, -planedist, n, q);
	for (i = 0; i < surf->numedges; i++)
	{
		const float *a = PF_SurfaceVertex(mod, surf, i);
		const float *b = PF_SurfaceVertex(mod, surf, (i + 1) % surf->numedges);
		float t, len2, dist2, side;
		VectorSubtract(b, a, e);
		VectorSubtract(q, a, d);
		CrossProduct(e, d, c);
		side = DotProduct(c, n);
		if (side > 0.001f || side < -0.001f)
		{
			const int s = side > 0 ? 1 : -1;
			if (sign && s != sign)
				inside = false;
			sign = s;
		}
		VectorSubtract(p, a, d); // nearest point of the edge segment to p
		len2 = DotProduct(e, e);
		t = len2 > 0 ? DotProduct(d, e) / len2 : 0;
		t = t < 0 ? 0 : t > 1 ? 1 : t;
		VectorMA(a, t, e, c);
		VectorSubtract(p, c, d);
		dist2 = DotProduct(d, d);
		if (dist2 < best)
			best = dist2;
	}
	return inside ? planedist * planedist : best;
}

// float(entity e, vector p) getsurfacenearpoint = #438 (DP_QC_GETSURFACE, DP's semantics): the index of the
// surface of e's brush model nearest to p (p made relative to e's origin; no rotation, as DP), -1 when e has no
// brush model. AD: its sky test (ad_check_skycontent) on world.
static void PF_getsurfacenearpoint (void)
{
	edict_t *ed = G_EDICT(OFS_PARM0);
	const float *point = G_VECTOR(OFS_PARM1);
	qmodel_t *mod = PF_SurfaceModel(ed);
	vec3_t p, clipped;
	float bestdist = 1e30f;
	int i, j, best = -1;
	G_FLOAT(OFS_RETURN) = -1;
	if (!mod)
		return;
	VectorSubtract(point, ed->v.origin, p);
	for (i = 0; i < mod->nummodelsurfaces; i++)
	{
		const msurface_t *surf = &mod->surfaces[mod->firstmodelsurface + i];
		float dist;
		for (j = 0; j < 3; j++) // the surface's box first: skip it when even that is farther
			clipped[j] = (p[j] < surf->mins[j] ? surf->mins[j] : p[j] > surf->maxs[j] ? surf->maxs[j] : p[j]) - p[j];
		if (DotProduct(clipped, clipped) >= bestdist)
			continue;
		dist = PF_SurfaceDistance2(mod, surf, p);
		if (dist < bestdist)
		{
			bestdist = dist;
			best = i;
		}
	}
	G_FLOAT(OFS_RETURN) = best;
}

// string(entity e, float s) getsurfacetexture = #437 (DP_QC_GETSURFACE): the texture name of surface s of e's brush
// model (as getsurfacenearpoint numbers them), "" when there is none.
static void PF_getsurfacetexture (void)
{
	qmodel_t *mod = PF_SurfaceModel(G_EDICT(OFS_PARM0));
	const int s = (int)G_FLOAT(OFS_PARM1);
	const msurface_t *surf;
	char *out;
	G_INT(OFS_RETURN) = 0;
	if (!mod || s < 0 || s >= mod->nummodelsurfaces)
		return;
	surf = &mod->surfaces[mod->firstmodelsurface + s];
	if (!surf->texinfo || surf->texinfo->texnum < 0 || surf->texinfo->texnum >= mod->numtextures
		|| !mod->textures[surf->texinfo->texnum])
		return;
	out = PR_GetTempString();
	q_strlcpy(out, mod->textures[surf->texinfo->texnum]->name, STRINGTEMP_LENGTH);
	G_INT(OFS_RETURN) = PR_SetEngineString(out);
}

// The named effects particleeffectnum knows: DP's built-in names for the classic temp entities (its fallback when
// effectinfo.txt has no such effect), drawn as those temp entities' particles without their sounds (DP's
// pointparticles plays none). Quake VR has no effectinfo.txt: any other name (AD's TE_PLASMA, TR_BLOOD, DPP_ITS...)
// is effect 0, which pointparticles ignores. An impact's count is its classic particle count per pointparticles
// count; count 255 is the client's particle explosion (R_ParseParticleEffect).
typedef struct
{
	const char *name;
	int tempentity;		// a svc_temp_entity type, or -1: a svc_particle burst
	int color, count;
} pf_particleeffect_t;
static const pf_particleeffect_t pf_particleeffects[] =
{
	{"TE_EXPLOSION",	-1,				0,		255},
	{"TE_GUNSHOT",		-1,				0,		20},
	{"TE_SPIKE",		-1,				0,		10},
	{"TE_SUPERSPIKE",	-1,				0,		20},
	{"TE_WIZSPIKE",		-1,				20,		30},
	{"TE_KNIGHTSPIKE",	-1,				226,	20},
	{"TE_LAVASPLASH",	TE_LAVASPLASH,	0,		0},
	{"TE_TELEPORT",		TE_TELEPORT,	0,		0},
};

// float(string effectname) particleeffectnum = #335 (DP_SV_POINTPARTICLES): the effect's number, 0 when unknown.
static void PF_particleeffectnum (void)
{
	const char *name = G_STRING(OFS_PARM0);
	int i;
	G_FLOAT(OFS_RETURN) = 0;
	for (i = 0; i < (int)Q_COUNTOF(pf_particleeffects); i++)
	{
		if (!q_strcasecmp(name, pf_particleeffects[i].name))
		{
			G_FLOAT(OFS_RETURN) = i + 1;
			return;
		}
	}
}

// Room for one more message in this frame's datagram (SV_StartParticle's margin; a coord is up to 4 bytes).
static qboolean PF_DatagramRoom (int bytes)
{
	return sv.datagram.cursize + bytes <= MAX_DATAGRAM;
}

// void(float effectnum, vector origin, optional vector dir, optional float count) pointparticles = #337
// (DP_SV_POINTPARTICLES): effect `effectnum` (particleeffectnum's) at origin, sent to every client. dir (default
// '0 0 0') is an impact's particle velocity; count (default 1, 0 or less: nothing) multiplies an impact's particles.
static void PF_pointparticles (void)
{
	const int effect = (int)G_FLOAT(OFS_PARM0);
	const float *org = G_VECTOR(OFS_PARM1);
	const float count = (qcvm->argc > 3) ? G_FLOAT(OFS_PARM3) : 1;
	vec3_t pos, dir = {0, 0, 0};
	const pf_particleeffect_t *fx;
	if (effect < 1 || effect > (int)Q_COUNTOF(pf_particleeffects) || count <= 0)
		return;
	if (qcvm->argc > 2)
		VectorCopy(G_VECTOR(OFS_PARM2), dir);
	VectorCopy(org, pos);
	fx = &pf_particleeffects[effect - 1];
	if (fx->tempentity < 0)
	{
		int n = fx->count;
		if (n != 255)
			n = CLAMP(1, (int)(n * count), 254); // (255 is the explosion)
		SV_StartParticle(pos, dir, fx->color, n);
		return;
	}
	if (!PF_DatagramRoom(2 + 3 * 4))
		return;
	MSG_WriteByte(&sv.datagram, svc_temp_entity);
	MSG_WriteByte(&sv.datagram, fx->tempentity);
	MSG_WriteCoord(&sv.datagram, pos[0], sv.protocolflags);
	MSG_WriteCoord(&sv.datagram, pos[1], sv.protocolflags);
	MSG_WriteCoord(&sv.datagram, pos[2], sv.protocolflags);
	VR_BroadcastMessageEnd(); // (a boundary, as SV_StartParticle's: vr/vr_server.cpp)
}

// void(vector org, float color, float colorlength) te_explosion2 = #427 (DP_TE_EXPLOSION2): Quake's TE_EXPLOSION2
// (Hipnotic's colour-mapped explosion: particles of palette colours color .. color+colorlength-1, a light and the
// explosion sound). AD: the mammoth boss's explosions (ad_mon_bossmammoth.qc).
static void PF_te_explosion2 (void)
{
	const float *org = G_VECTOR(OFS_PARM0);
	if (!PF_DatagramRoom(2 + 3 * 4 + 2))
		return;
	MSG_WriteByte(&sv.datagram, svc_temp_entity);
	MSG_WriteByte(&sv.datagram, TE_EXPLOSION2);
	MSG_WriteCoord(&sv.datagram, org[0], sv.protocolflags);
	MSG_WriteCoord(&sv.datagram, org[1], sv.protocolflags);
	MSG_WriteCoord(&sv.datagram, org[2], sv.protocolflags);
	MSG_WriteByte(&sv.datagram, CLAMP(0, (int)G_FLOAT(OFS_PARM1), 255));
	MSG_WriteByte(&sv.datagram, CLAMP(1, (int)G_FLOAT(OFS_PARM2), 255)); // (the client takes colorMod % colorLength)
	VR_BroadcastMessageEnd();
}

// void(vector mins, vector maxs, vector vel, float count, float color) te_particlerain = #409 /
// te_particlesnow = #410 (DP_TE_PARTICLERAIN / DP_TE_PARTICLESNOW): a documented no-op. DP sends its own temp
// entities (64, 65) that this engine's client and protocol don't have (an unknown type is a client Host_Error), and
// svc_particle can't carry rain's speed (about 120 units/s at most). AD's weather (ad_part_generate.qc) needs both
// extensions, which checkextension denies, so it never calls them.
static void PF_te_particleweather (void)
{
}

//server/client stuff
static void PF_checkcommand(void)
{
	const char *name = G_STRING(OFS_PARM0);
	if (Cmd_Exists(name))
		G_FLOAT(OFS_RETURN) = 1;
	else if (Cmd_AliasExists(name))
		G_FLOAT(OFS_RETURN) = 2;
	else if (Cvar_FindVar(name))
		G_FLOAT(OFS_RETURN) = 3;
	else
		G_FLOAT(OFS_RETURN) = 0;
}
static void PF_clientcommand(void)
{
	edict_t	*ed				= G_EDICT(OFS_PARM0);
	const char *str			= G_STRING(OFS_PARM1);
	unsigned int i			= NUM_FOR_EDICT(ed)-1;
	if (i < (unsigned int)svs.maxclients && svs.clients[i].active)
	{
		client_t *ohc = host_client;
		host_client = &svs.clients[i];
		Cmd_ExecuteString (str, src_client);
		host_client = ohc;
	}
	else
		Con_Printf("PF_clientcommand: not a client\n");
}


#define PF_BOTH(x)	x,x
#define PF_CSQC(x)	NULL,x
#define PF_SSQC(x)	x,NULL

builtindef_t pr_builtindefs[] =
{
	{"makevectors",				PF_SSQC(PF_makevectors),		1},		// void(entity e) makevectors		= #1
	{"setorigin",				PF_SSQC(PF_setorigin),			2},		// void(entity e, vector o) setorigin	= #2
	{"setmodel",				PF_SSQC(PF_setmodel),			3},		// void(entity e, string m) setmodel	= #3
	{"setsize",					PF_SSQC(PF_setsize),			4},		// void(entity e, vector min, vector max) setsize	= #4
	{"break",					PF_BOTH(PF_break),				6},		// void() break				= #6
	{"random",					PF_BOTH(PF_random),				7},		// float() random			= #7
	{"sound",					PF_SSQC(PF_sound),				8},		// void(entity e, float chan, string samp) sound	= #8
	{"normalize",				PF_BOTH(PF_normalize),			9},		// vector(vector v) normalize		= #9
	{"error",					PF_SSQC(PF_error),				10},	// void(string e) error			= #10
	{"objerror",				PF_SSQC(PF_objerror),			11},	// void(string e) objerror		= #11
	{"vlen",					PF_BOTH(PF_vlen),				12},	// float(vector v) vlen			= #12
	{"vectoyaw",				PF_BOTH(PF_vectoyaw),			13},	// float(vector v) vectoyaw		= #13
	{"spawn",					PF_SSQC(PF_Spawn),				14},	// entity() spawn			= #14
	{"remove",					PF_SSQC(PF_Remove),				15},	// void(entity e) remove		= #15
	{"traceline",				PF_SSQC(PF_traceline),			16},	// float(vector v1, vector v2, float tryents) traceline	= #16
	{"checkclient",				PF_SSQC(PF_checkclient),		17},	// entity() clientlist			= #17
	{"find",					PF_SSQC(PF_Find),				18},	// entity(entity start, .string fld, string match) find	= #18
	{"precache_sound",			PF_SSQC(PF_precache_sound),		19},	// void(string s) precache_sound	= #19
	{"precache_model",			PF_SSQC(PF_precache_model),		20},	// void(string s) precache_model	= #20
	{"stuffcmd",				PF_SSQC(PF_stuffcmd),			21},	// void(entity client, string s)stuffcmd	= #21
	{"findradius",				PF_SSQC(PF_findradius),			22},	// entity(vector org, float rad) findradius	= #22
	{"bprint",					PF_SSQC(PF_bprint),				23},	// void(string s) bprint		= #23
	{"sprint",					PF_SSQC(PF_sprint),				24},	// void(entity client, string s) sprint	= #24
	{"dprint",					PF_BOTH(PF_dprint),				25},	// void(string s) dprint		= #25
	{"ftos",					PF_BOTH(PF_ftos),				26},	// void(string s) ftos			= #26
	{"vtos",					PF_BOTH(PF_vtos),				27},	// void(string s) vtos			= #27
	{"coredump",				PF_SSQC(PF_coredump),			28},
	{"traceon",					PF_BOTH(PF_traceon),			29},
	{"traceoff",				PF_BOTH(PF_traceoff),			30},
	{"eprint",					PF_SSQC(PF_eprint),				31},	// void(entity e) debug print an entire entity
	{"walkmove",				PF_SSQC(PF_walkmove),			32},	// float(float yaw, float dist) walkmove
	{"droptofloor",				PF_SSQC(PF_droptofloor),		34},
	{"lightstyle",				PF_SSQC(PF_lightstyle),			35},
	{"rint",					PF_BOTH(PF_rint),				36},
	{"floor",					PF_BOTH(PF_floor),				37},
	{"ceil",					PF_BOTH(PF_ceil),				38},
	{"checkbottom",				PF_SSQC(PF_checkbottom),		40},
	{"pointcontents",			PF_SSQC(PF_pointcontents),		41},
	{"fabs",					PF_BOTH(PF_fabs),				43},
	{"aim",						PF_SSQC(PF_aim),				44},
	{"cvar",					PF_BOTH(PF_cvar),				45},
	{"localcmd",				PF_BOTH(PF_localcmd),			46},
	{"nextent",					PF_SSQC(PF_nextent),			47},
	{"particle",				PF_SSQC(PF_particle),			48},
	{"ChangeYaw",				PF_SSQC(PF_changeyaw),			49},
	{"vectoangles",				PF_BOTH(PF_vectoangles),		51},

	{"WriteByte",				PF_SSQC(PF_WriteByte),			52},
	{"WriteChar",				PF_SSQC(PF_WriteChar),			53},
	{"WriteShort",				PF_SSQC(PF_WriteShort),			54},
	{"WriteLong",				PF_SSQC(PF_WriteLong),			55},
	{"WriteCoord",				PF_SSQC(PF_WriteCoord),			56},
	{"WriteAngle",				PF_SSQC(PF_WriteAngle),			57},
	{"WriteString",				PF_SSQC(PF_WriteString),		58},
	{"WriteEntity",				PF_SSQC(PF_WriteEntity),		59},

	{"sin",						PF_BOTH(PF_Sin),				60,		DP_QC_SINCOSSQRTPOW},	// float(float angle)
	{"cos",						PF_BOTH(PF_Cos),				61,		DP_QC_SINCOSSQRTPOW},	// float(float angle)
	{"sqrt",					PF_BOTH(PF_Sqrt),				62,		DP_QC_SINCOSSQRTPOW},	// float(float value)

	{"etos",					PF_BOTH(PF_etos),				65,		DP_QC_ETOS},			// string(entity ent)

	{"movetogoal",				PF_SSQC(SV_MoveToGoal),			67},
	{"precache_file",			PF_SSQC(PF_precache_file),		68},
	{"makestatic",				PF_SSQC(PF_makestatic),			69},

	{"changelevel",				PF_SSQC(PF_changelevel),		70},

	{"cvar_set",				PF_BOTH(PF_cvar_set),			72},
	{"centerprint",				PF_SSQC(PF_centerprint),		73},

	{"ambientsound",			PF_SSQC(PF_ambientsound),		74},

	{"precache_model2",			PF_SSQC(PF_precache_model),		75},
	{"precache_sound2",			PF_SSQC(PF_precache_sound),		76},	// precache_sound2 is different only for qcc
	{"precache_file2",			PF_SSQC(PF_precache_file),		77},

	{"setspawnparms",			PF_SSQC(PF_setspawnparms),		78},

	// 2021 re-release
	{"finaleFinished",			PF_SSQC(PF_finalefinished),		79},	// float() finaleFinished = #79
	{"localsound",				PF_SSQC(PF_localsound),			80},	// void localsound (entity client, string sample) = #80

	{"stof",					PF_BOTH(PF_stof),				81,		FRIK_FILE},			// float(string)

	// 2021 re-release update 3
	{"ex_centerprint",			PF_SSQC(PF_centerprint)},				// void(entity client, string s, ...)
	{"ex_bprint",				PF_SSQC(PF_bprint)},					// void(string s, ...)
	{"ex_sprint",				PF_SSQC(PF_sprint)},					// void(entity client, string s, ...)
	{"ex_finalefinished",		PF_SSQC(PF_finalefinished)},			// float()
	{"ex_CheckPlayerEXFlags",	PF_SSQC(PF_CheckPlayerEXFlags)},		// float(entity playerEnt)
	{"ex_walkpathtogoal",		PF_SSQC(PF_walkpathtogoal)},			// float(float movedist, vector goal)
	{"ex_localsound",			PF_SSQC(PF_localsound)},				// void(entity client, string sample)

	{"min",						PF_BOTH(PF_min),				94,		DP_QC_MINMAXBOUND},	// float(float a, float b, ...)
	{"max",						PF_BOTH(PF_max),				95,		DP_QC_MINMAXBOUND},	// float(float a, float b, ...)
	{"bound",					PF_BOTH(PF_bound),				96,		DP_QC_MINMAXBOUND},	// float(float minimum, float val, float maximum)

	{"pow",						PF_BOTH(PF_pow),				97,		DP_QC_SINCOSSQRTPOW},	// float(float value, float exp)

	{"checkextension",			PF_BOTH(PF_checkextension),		99},	// float(string extname)

	{"strlen",					PF_BOTH(PF_strlen),				114,	FRIK_FILE},	// float(string s)
	{"strcat",					PF_BOTH(PF_strcat),				115,	FRIK_FILE},	// string(string s1, optional string s2, optional string s3, optional string s4, optional string s5, optional string s6, optional string s7, optional string s8)
	{"substring",				PF_BOTH(PF_substring),			116,	FRIK_FILE},	// string(string s, float start, float length)
	{"stov",					PF_BOTH(PF_stov),				117,	FRIK_FILE},	// vector(string s)
	{"strzone",					PF_BOTH(PF_strzone),			118,	FRIK_FILE},	// string(string s, ...)
	{"strunzone",				PF_BOTH(PF_strunzone),			119,	FRIK_FILE},	// void(string s)

	{"strstrofs",				PF_BOTH(PF_strstrofs),			221,	FTE_STRINGS},	// float(string str, string sub, optional float startpos)
	{"str2chr",					PF_BOTH(PF_str2chr),			222,	FTE_STRINGS},	// float(string str, float index)
	{"chr2str",					PF_BOTH(PF_chr2str),			223,	FTE_STRINGS},	// string(float chr, ...)
	{"strconv",					PF_BOTH(PF_strconv),			224,	FTE_STRINGS},	// string(float ccase, float redalpha, float redchars, string str, ...)

	{"clientstat",				PF_SSQC(PF_clientstat),			232},	// void(float num, float type, .__variant fld)
	{"addstat",					PF_SSQC(PF_clientstat)},				// QVR: AD support: DP's name for clientstat

	// QVR: AD support: the DP/FTE builtins AD imports by name (PF_bitshift and the ones after it)
	{"bitshift",				PF_BOTH(PF_bitshift),			218},	// float(float number, float quantity)
	{"strncasecmp",				PF_BOTH(PF_strncasecmp),		230},	// float(string s1, string s2, float len, optional float s1ofs, optional float s2ofs)
	{"particleeffectnum",		PF_SSQC(PF_particleeffectnum),	335},	// float(string effectname)
	{"pointparticles",			PF_SSQC(PF_pointparticles),		337},	// void(float effectnum, vector origin, optional vector dir, optional float count)
	{"te_particlerain",			PF_SSQC(PF_te_particleweather),	409},	// void(vector mins, vector maxs, vector vel, float count, float color): a no-op
	{"te_particlesnow",			PF_SSQC(PF_te_particleweather),	410},	// void(vector mins, vector maxs, vector vel, float count, float color): a no-op
	{"te_explosion2",			PF_SSQC(PF_te_explosion2),		427},	// void(vector org, float color, float colorlength)
	{"getsurfacetexture",		PF_SSQC(PF_getsurfacetexture),	437},	// string(entity e, float s)
	{"getsurfacenearpoint",		PF_SSQC(PF_getsurfacenearpoint),	438},	// float(entity e, vector p)

	{"mod",						PF_BOTH(PF_mod),				245},	// float(float a, float n)

	{"ftoi",					PF_BOTH(PF_ftoi)},						// int(float)
	{"itof",					PF_BOTH(PF_itof)},						// float(int)

	{"checkcommand",			PF_BOTH(PF_checkcommand),		294,	FTE_QC_CHECKCOMMAND},	// float(string name)

	{"iscachedpic",				PF_CSQC(PF_cl_iscachedpic),		316},	// float(string name)
	{"precache_pic",			PF_CSQC(PF_cl_precachepic),		317},	// string(string name, optional float flags)
	{"drawgetimagesize",		PF_CSQC(PF_cl_getimagesize),	318},	// #define draw_getimagesize drawgetimagesize\nvector(string picname)
	{"drawcharacter",			PF_CSQC(PF_cl_drawcharacter),	320},	// float(vector position, float character, vector size, vector rgb, float alpha, optional float drawflag)
	{"drawrawstring",			PF_CSQC(PF_cl_drawrawstring),	321},	// float(vector position, string text, vector size, vector rgb, float alpha, optional float drawflag)
	{"drawpic",					PF_CSQC(PF_cl_drawpic),			322},	// float(vector position, string pic, vector size, vector rgb, float alpha, optional float drawflag)
	{"drawfill",				PF_CSQC(PF_cl_drawfill),		323},	// float(vector position, vector size, vector rgb, float alpha, optional float drawflag)
	{"drawsetcliparea",			PF_CSQC(PF_cl_drawsetclip),		324},	// void(float x, float y, float width, float height)
	{"drawresetcliparea",		PF_CSQC(PF_cl_drawresetclip),	325},	// void(void)
	{"drawstring",				PF_CSQC(PF_cl_drawstring),		326},	// float(vector position, string text, vector size, vector rgb, float alpha, float drawflag)
	{"stringwidth",				PF_CSQC(PF_cl_stringwidth),		327},	// float(string text, float usecolours, vector fontsize='8 8')
	{"drawsubpic",				PF_CSQC(PF_cl_drawsubpic),		328},	// void(vector pos, vector sz, string pic, vector srcpos, vector srcsz, vector rgb, float alpha, optional float drawflag)

	{"getstati",				PF_CSQC(PF_cl_getstat_int),		330},	// #define getstati_punf(stnum) (float)(__variant)getstati(stnum)\nint(float stnum)
	{"getstatf",				PF_CSQC(PF_cl_getstat_float),	331},	// #define getstatbits getstatf\nfloat(float stnum, optional float firstbit, optional float bitcount)
	{"getstats",				PF_CSQC(PF_cl_getstat_string),	332},	// string(float stnum)

	{"getplayerkeyvalue",		PF_CSQC(PF_cl_playerkey_s),		348},	// string(float playernum, string keyname)
	{"getplayerkeyfloat",		PF_CSQC(PF_cl_playerkey_f)},			// float(float playernum, string keyname, optional float assumevalue)

	{"registercommand",			PF_CSQC(PF_cl_registercommand),	352},	// void(string cmdname)

	{"vectorvectors",			PF_BOTH(PF_vectorvectors),		432,	DP_QC_VECTORVECTORS},	// void(vector dir)

	{"clientcommand",			PF_SSQC(PF_clientcommand),		440,	KRIMZON_SV_PARSECLIENTCOMMAND},	// void(entity e, string s)
	{"tokenize",				PF_BOTH(PF_Tokenize),			441,	KRIMZON_SV_PARSECLIENTCOMMAND},	// float(string s)
	{"argv",					PF_BOTH(PF_ArgV),				442,	KRIMZON_SV_PARSECLIENTCOMMAND},	// string(float n)
	{"argc",					PF_BOTH(PF_ArgC)},						// float()

	{"asin",					PF_BOTH(PF_asin),				471,	DP_QC_ASINACOSATANATAN2TAN},	// float(float s)
	{"acos",					PF_BOTH(PF_acos),				472,	DP_QC_ASINACOSATANATAN2TAN},	// float(float c)
	{"atan",					PF_BOTH(PF_atan),				473,	DP_QC_ASINACOSATANATAN2TAN},	// float(float t)
	{"atan2",					PF_BOTH(PF_atan2),				474,	DP_QC_ASINACOSATANATAN2TAN},	// float(float c, float s)
	{"tan",						PF_BOTH(PF_tan),				475,	DP_QC_ASINACOSATANATAN2TAN},	// float(float a)

	{"tokenize_console",		PF_BOTH(PF_tokenize_console),	514,	DP_QC_TOKENIZE_CONSOLE},		// float(string str)

	{"sprintf",					PF_BOTH(PF_sprintf),			627,	DP_QC_SPRINTF},					// string(string fmt, ...)
};
int pr_numbuiltindefs = Q_COUNTOF(pr_builtindefs);

COMPILE_TIME_ASSERT (builtin_buffer_size, countof (pr_builtindefs) + 1 <= MAX_BUILTINS);
