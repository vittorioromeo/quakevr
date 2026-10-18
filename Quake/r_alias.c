/*
Copyright (C) 1996-2001 Id Software, Inc.
Copyright (C) 2002-2009 John Fitzgibbons and others
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

//r_alias.c -- alias model rendering

#include "quakedef.h"
#include "vr/vr_tracy.h" // QVR: profiling scopes (compiled out of normal builds)

extern cvar_t gl_overbright_models, gl_fullbrights, r_lerpmodels, r_lerpmove; //johnfitz
extern cvar_t scr_fov, cl_gun_fovscale, cl_gun_x, cl_gun_y, cl_gun_z;
extern cvar_t r_oit;

//up to 16 color translated skins
gltexture_t *playertextures[MAX_SCOREBOARD]; //johnfitz -- changed to an array of pointers

const float	r_avertexnormals[NUMVERTEXNORMALS][3] = {
#include "anorms.h"
};

typedef enum { ALIAS_STANDARD, ALIAS_SHOWTRIS, ALIAS_SHOWSKEL, ALIAS_DEPTH, } aliasmode_t; // QVR: ALIAS_DEPTH, the shadow maps' casters (depth only: no lighting)

extern vec3_t	lightcolor; //johnfitz -- replaces "float shadelight" for lit support

static float	entalpha; //johnfitz
static qboolean	aliasdepth; // QVR: drawing the shadow maps' casters (R_DrawAliasModelsDepth)
static int		aliasnear; // QVR: the batch's VR_AliasNearEye flags (1 depth clamp, 2 both sides)
// QVR: a shadow map's casters drawn once into all the light's faces they reach (R_DrawAliasModelsDepthLayered): the
// faces of the entity being added (bit f: face f, its viewport f), NULL one face at a time (R_DrawAliasModelsDepth)
static const unsigned char	*aliasfaces;

#ifndef GL_DEPTH_CLAMP
#define GL_DEPTH_CLAMP 0x864F
#endif

//johnfitz -- struct for passing lerp information to drawing functions
typedef struct {
	short pose1;
	short pose2;
	float blend;
	vec3_t origin;
	vec3_t angles;
} lerpdata_t;
//johnfitz

#define MAX_ALIAS_INSTANCES 256

typedef struct aliasinstance_s {
	float		worldmatrix[12];
	vec3_t		lightcolor;
	float		alpha;
	int32_t		pose1;
	int32_t		pose2;
	float		blend;
	int32_t		padding;
	vraliasinstance_t	vr; // QVR: Quake VR's (vr_api_render.h: VR_AliasInstance)
	int32_t		motion[4]; // QVR: temporal AA's vertex slots (VR_TemporalAliasMotion; the shader's Motion)
} aliasinstance_t;

struct ibuf_s {
	int			count;
	entity_t	*ent;

	struct {
		float	matviewproj[16];
		vec3_t	eyepos;
		float	_pad;
		vec4_t	fog;
		float	dither;
		float	_padding[3];
	} global;
	aliasinstance_t inst[MAX_ALIAS_INSTANCES];
} ibuf;

// QVR: the layered shadow casters' (R_DrawAliasModelsDepthLayered): each batched instance's faces, and the shader's
// FaceBuffer (gl_shaders.h): the faces' view-projections, then a draw per instance and face (its index | face << 16)
static unsigned char aliasinstfaces[MAX_ALIAS_INSTANCES];
static struct {
	float		viewproj[6][16];
	uint32_t	draws[MAX_ALIAS_INSTANCES * 6];
} aliasfacebuf;

// QVR: the shadow maps' casters set up once a pass over the lights (R_AliasDepthCacheBegin/End, vr_lighting.cpp's
// VR_RenderShadowMaps): a caster in several lights had its lerp, matrices and zero blend worked out again for each.
// Within a pass nothing they read changes (the entity, the time, the hands), and the first set-up's lerp updates leave
// the entity as the next one finds it: the same numbers. Keyed by the entity (and its model and frame, against a copy
// in the same place); a full neighbourhood just sets it up again. Not the depth draws outside a pass, not cl.viewent
// (its gun offsets follow the camera's axes).
#define ALIASDEPTH_CACHE_SIZE	4096	// a power of two
#define ALIASDEPTH_CACHE_PROBES	16
// vr_shadow_setup_share (R_AliasDepthShare) 1 or 2 also keeps each caster's whole instance (its pose, matrix, zero
// blend, depth-only data, motion slots: the same for every light) and its swing rows (r_lerpmodels 3), copied into the
// other lights' batches; and the first eye's scene takes the pass's lerp and matrices for the same entities instead of
// setting them up again (R_AliasTransformShared: R_AliasTransform is the same for a light and an eye; 2 sets them up
// anyway and compares). 0: only the lerp and the matrix, as before.
typedef struct {
	const entity_t	*ent;
	const qmodel_t	*model;
	int			frame;
	int			gen;
	int			totalverts;
	int			zeroblend;
	lerpdata_t	lerpdata;
	float		model_matrix[16];
	float		bounds_matrix[16];
	int			instok;		// inst and the swing rows kept (vr_shadow_setup_share)
	const float	*boxbones;	// a posed caster's box (R_AliasPosedBox) and the bone matrices it is of (NULL: none)
	vec3_t		boxmins, boxmaxs;
	int			smoothfirst;	// its swing rows: where they are kept (VR_AnimSmoothKeep), how many, where they were in the batch
	int			smoothrows;
	int			smoothat;
	aliasinstance_t	inst;
} aliasdepthentry_t;
static aliasdepthentry_t	aliasdepthcache[ALIASDEPTH_CACHE_SIZE];
static int					aliasdepthgen; // the pass's (0: none)
static int					aliasdepthgens; // the last pass's
static int					aliasdepthhits, aliasdepthmisses; // the last passes' (R_AliasDepthCacheStats)
static int					aliasdepthshare; // vr_shadow_setup_share: 0 off, 1 on, 2 the first eye's checked
static int					aliasdepthkeep; // this pass keeps the casters' swing rows (none of the eyes' kept ones to clear)
static int					aliasdepthlast; // the frame's last pass's gen, for the first eye (R_AliasTransformShared); 0 none
static int					aliaseyemode; // (the eyes' cache's, below)
enum { ADS_TAKEN, ADS_CHECKED, ADS_BAD, ADS_NUMSTATS };
static int					aliasdepthsharestats[ADS_NUMSTATS];

/*
=================
R_AliasDepthCacheBegin / R_AliasDepthCacheEnd -- QVR

A pass over the shadow maps' lights (on: 0 sets each up every time, vr_shadow_layered_check's comparison).
=================
*/
void R_AliasDepthCacheBegin (qboolean on)
{
	if (++aliasdepthgens <= 0)
	{
		memset (aliasdepthcache, 0, sizeof (aliasdepthcache));
		aliasdepthgens = 1;
	}
	aliasdepthgen = on ? aliasdepthgens : 0;
	aliasdepthlast = 0;
	// the swing rows are kept in the store the eyes' cache keeps its own in (cleared as the first eye begins): kept here
	// only outside the eyes' modes (then cleared here: none kept), or in the first eye's (whose clearing came first)
	aliasdepthkeep = aliasdepthgen && aliasdepthshare && (aliaseyemode == 0 || aliaseyemode == 1);
	if (aliasdepthkeep && aliaseyemode == 0)
		VR_AnimSmoothKeepClear ();
}

void R_AliasDepthCacheEnd (void)
{
	aliasdepthlast = aliasdepthshare ? aliasdepthgen : 0;
	aliasdepthgen = 0;
	aliasdepthkeep = 0;
}

void R_AliasDepthShare (int mode)
{
	aliasdepthshare = mode < 0 ? 0 : mode > 2 ? 2 : mode;
}

void R_AliasDepthShareStats (int *taken, int *checked, int *bad)
{
	*taken = aliasdepthsharestats[ADS_TAKEN];
	*checked = aliasdepthsharestats[ADS_CHECKED];
	*bad = aliasdepthsharestats[ADS_BAD];
	memset (aliasdepthsharestats, 0, sizeof (aliasdepthsharestats));
}

void R_AliasDepthCacheStats (int *hits, int *misses)
{
	*hits = aliasdepthhits;
	*misses = aliasdepthmisses;
	aliasdepthhits = aliasdepthmisses = 0;
}

// e's entry this pass (*hit: already set up) or the slot to keep it in; NULL none (no pass, or no room)
static aliasdepthentry_t *R_AliasDepthSlot (const entity_t *e, qboolean *hit)
{
	uint64_t h;
	int i;
	*hit = false;
	if (!aliasdepthgen || e == &cl.viewent)
		return NULL;
	h = ((uint64_t) (uintptr_t) e >> 4) * 0x9E3779B97F4A7C15ull;
	h >>= 64 - 12;
	for (i = 0; i < ALIASDEPTH_CACHE_PROBES; i++)
	{
		aliasdepthentry_t *s = &aliasdepthcache[(h + i) & (ALIASDEPTH_CACHE_SIZE - 1)];
		if (s->gen != aliasdepthgen)
		{
			s->gen = 0; // claimed below once set up
			return s;
		}
		if (s->ent == e)
		{
			if (s->model != e->model || s->frame != e->frame)
				return NULL;
			*hit = true;
			return s;
		}
	}
	return NULL;
}

// QVR: the second eye's set-up of each alias entity (vr_alias_eye_cache; R_AliasEyeCacheBegin, from the eyes' loop in
// vr_stereo.cpp): what of it does not depend on the eye, made in the first eye's scene and kept for the second's, which
// set it all up again from the same entity at the same time. Kept: the lerp and the transform (R_SetupAliasFrame,
// R_SetupEntityTransform, VR_AliasPreTransform/PostTransform), the light at its origin (R_LightPoint's, before the
// dynamic lights: those are culled by each eye's frustum), its vertex count and zero blend, its Quake VR instance data
// (VR_AliasInstance) and its smooth animation's (R_AliasSmoothInstance: its swing rows kept and added to the second
// eye's batch where it lands). Done per eye still: the portals (which side of a gate), the cull, the alpha, the dynamic
// lights and the light's minimums and clamp, the near-eye flags, the batching, and the view entities' instance data (a
// bare forearm takes the body's ambient light as last drawn: the first eye draws the hands before the body, the second
// after; the hands, weapons and body are few). Never: cl.viewent (its gun offsets
// follow the eye's axes), the views through teleporters, the shadow maps, r_showtris/r_showbboxes, the spectator's view,
// the drawn moves' log. Mode 2 (the check): the second eye sets everything up anew and compares (vr_alias_eye_cache_stats).
#define ALIASEYE_CACHE_SIZE		2048	// a power of two
#define ALIASEYE_CACHE_PROBES	16
typedef struct {
	const entity_t	*ent;
	const qmodel_t	*model;
	int			frame;
	int			skinnum;
	int			gen;
	int			lit;		// 0: the transform only (culled in the first eye); 1: all of it
	int			vrok;		// its instance data too (0: a view entity's, made per eye)
	lerpdata_t	lerpdata;
	float		model_matrix[16];
	float		bounds_matrix[16];
	vec3_t		light;		// R_LightPoint's
	int			totalverts;
	int			zeroblend;
	int			smoothfirst;	// its swing rows: where they are kept (VR_AnimSmoothKeep), how many, where they were in the batch
	int			smoothrows;
	int			smoothat;
	vraliasinstance_t	vr;
} aliaseyeentry_t;
static aliaseyeentry_t	aliaseyecache[ALIASEYE_CACHE_SIZE];
static int				aliaseyegen;	// the frame's (0: none)
static int				aliaseyegens;	// the last frame's
// aliaseyemode (declared above, with the shadow casters' cache): 0 off, 1 the first eye (kept), 2 the second (reused), 3
// the second (checked)

// vr_alias_eye_cache_stats: what the second eyes found since the last call; the check's mismatches by part
enum { AEYE_HIT, AEYE_TRANSFORM_ONLY, AEYE_MISS, AEYE_CHECKED, AEYE_BAD_LERP, AEYE_BAD_MATRIX, AEYE_BAD_LIGHT,
	AEYE_BAD_COUNTS, AEYE_BAD_VR, AEYE_BAD_SMOOTH, AEYE_NUMSTATS };
static int				aliaseyestats[AEYE_NUMSTATS];

void R_AliasEyeCacheBegin (int mode)
{
	if (mode == 1)
	{
		if (++aliaseyegens <= 0)
		{
			memset (aliaseyecache, 0, sizeof (aliaseyecache));
			aliaseyegens = 1;
		}
		aliaseyegen = aliaseyegens;
		VR_AnimSmoothKeepClear ();
	}
	aliaseyemode = aliaseyegen ? mode : 0; // (0 between the eyes: the frame's kept)
}

void R_AliasEyeCacheEnd (void)
{
	aliaseyemode = 0;
	aliaseyegen = 0;
	aliasdepthlast = 0; // QVR: (the frame's shadow casters' set-ups: R_AliasTransformShared)
}

void R_AliasEyeCacheStats (int *out, int count)
{
	int i;
	for (i = 0; i < count && i < AEYE_NUMSTATS; i++)
		out[i] = aliaseyestats[i];
	memset (aliaseyestats, 0, sizeof (aliaseyestats));
}

// e's entry this frame (*found: the first eye's), the slot to keep it in (the first eye), or NULL (not kept)
static aliaseyeentry_t *R_AliasEyeSlot (entity_t *e, qboolean *found)
{
	uint64_t h;
	int i;
	*found = false;
	if (!aliaseyemode || e == &cl.viewent || VR_PortalDrawing () || VR_DebugDrawnOn ())
		return NULL;
	h = ((uint64_t) (uintptr_t) e >> 4) * 0x9E3779B97F4A7C15ull;
	h >>= 64 - 11;
	for (i = 0; i < ALIASEYE_CACHE_PROBES; i++)
	{
		aliaseyeentry_t *s = &aliaseyecache[(h + i) & (ALIASEYE_CACHE_SIZE - 1)];
		if (s->gen != aliaseyegen)
		{
			if (aliaseyemode != 1)
				break;
			s->gen = 0; // claimed below once set up
			return s;
		}
		if (s->ent == e)
		{
			if (s->model != e->model || s->frame != e->frame || s->skinnum != e->skinnum)
				return NULL;
			*found = true;
			return s;
		}
	}
	if (aliaseyemode != 1)
		aliaseyestats[AEYE_MISS]++;
	return NULL;
}

// The check (mode 3): a part set up anew against the first eye's
static void R_AliasEyeCompare (const void *a, const void *b, size_t size, int stat)
{
	if (memcmp (a, b, size))
		aliaseyestats[stat]++;
}

/*
=================
R_FrameLerpFinish -- QVR: when the frame lerp of an entity whose messages carry a lerpfinish ends: as the message that
changed its pose said, not as the latest says. (Each message sends the time to its next think again, rounded to 1/255 s:
the lerp's end wandered by a few milliseconds from message to message, and the pose went back and forth by as much, in slow
motion several frames' worth: a knocked-down monster's get-up jittered in bullet time.)
=================
*/
float R_FrameLerpFinish (const entity_t *e)
{
	return e->animlerpfinish > e->lerpstart ? e->animlerpfinish : e->lerpfinish;
}

/*
=================
R_AnimHistoryPush -- QVR: r_lerpmodels 3 (vr/vr_animsmooth.cpp): a new lerp starts from e's current pose: the lerp
that ends here (animfrom to currentpose) becomes the history its spline curves in from, if it ran about its time up to
now; one cut short by this one (Quake jumps to its end: never drawn at that speed) or that ended a while ago (the pose
held, the model out of sight) is none: the new lerp starts from rest. Called before lerpstart moves on.
=================
*/
static void R_AnimHistoryPush (entity_t *e)
{
	float since = (float)(cl.time - e->lerpstart);
	float span = e->animlerpfinish > e->lerpstart ? e->animlerpfinish - e->lerpstart : e->lerptime;
	if (e->lerpstart > 0.f && since >= span * 0.5f && since <= span * 1.6f + 0.02f && e->animfrom != e->currentpose)
	{
		e->animpose0 = e->animfrom;
		e->animspan0 = since;
	}
	else
	{
		e->animpose0 = e->currentpose;
		e->animspan0 = 0.f;
	}
	e->animfrom = e->currentpose;
}

/*
=================
R_SetupAliasFrame -- johnfitz -- rewritten to support lerping
=================
*/
void R_SetupAliasFrame (entity_t *e, aliashdr_t *paliashdr, lerpdata_t *lerpdata)
{
	int posenum, numposes;
	int frame = e->frame;

	if ((frame >= paliashdr->numframes) || (frame < 0))
	{
		Con_DPrintf ("R_AliasSetupFrame: no such frame %d for '%s'\n", frame, e->model->name);
		frame = 0;
	}

	posenum = paliashdr->frames[frame].firstpose;
	numposes = paliashdr->frames[frame].numposes;

	if (numposes > 1)
	{
		e->lerptime = paliashdr->frames[frame].interval;
		posenum += (int)(cl.time / e->lerptime) % numposes;
	}
	else
		e->lerptime = 0.1;

	if (e->lerpflags & LERP_RESETANIM) //kill any lerp in progress
	{
		e->lerpstart = 0;
		e->animlerpfinish = 0; // QVR
		e->previouspose = posenum;
		e->currentpose = posenum;
		e->animfrom = e->animpose0 = posenum; // QVR: r_lerpmodels 3's history
		e->animspan0 = 0.f;
		e->lerpflags -= LERP_RESETANIM;
	}
	else if (e->currentpose != posenum) // pose changed, start new lerp
	{
		if (e->lerpflags & LERP_RESETANIM2) //defer lerping one more time
		{
			e->lerpstart = 0;
			e->animlerpfinish = 0; // QVR
			e->previouspose = posenum;
			e->currentpose = posenum;
			e->animfrom = e->animpose0 = posenum; // QVR
			e->animspan0 = 0.f;
			e->lerpflags -= LERP_RESETANIM2;
		}
		else
		{
			R_AnimHistoryPush (e); // QVR: r_lerpmodels 3: the lerp ending here becomes the spline's first span
			e->lerpstart = cl.time;
			e->animlerpfinish = (e->lerpflags & LERP_FINISH) ? e->lerpfinish : 0.f; // QVR: R_FrameLerpFinish
			e->previouspose = e->currentpose;
			e->currentpose = posenum;
		}
	}

	//set up values
	if (r_lerpmodels.value && !(e->model->flags & MOD_NOLERP && r_lerpmodels.value != 2))
	{
		float s = (cls.demoplayback && cls.demospeed < 0.f) ? -1.f : 1.f;
		if (e->lerpflags & LERP_FINISH && numposes == 1)
			lerpdata->blend = CLAMP (0.0f, (float)(cl.time - e->lerpstart) / (R_FrameLerpFinish (e) - e->lerpstart), 1.0f); // QVR
		else
			lerpdata->blend = CLAMP (0.0f, (float)(cl.time - e->lerpstart) / e->lerptime * s, 1.0f);
		if (lerpdata->blend == 1.0f)
			e->previouspose = e->currentpose;
		lerpdata->pose1 = e->previouspose;
		lerpdata->pose2 = e->currentpose;
	}
	else //don't lerp
	{
		lerpdata->blend = 1;
		lerpdata->pose1 = posenum;
		lerpdata->pose2 = posenum;
	}
	VR_AnimSmoothPinned (e, paliashdr, &lerpdata->pose1, &lerpdata->pose2, &lerpdata->blend); // QVR: vr_anim_smooth_pin
	VR_AliasPoseOverride (e, paliashdr, &lerpdata->pose1, &lerpdata->pose2, &lerpdata->blend); // QVR: the AD crossbow's string (vr_adcrossbow.cpp)
}

/*
=================
R_MoveLerpBlend -- QVR: how far along its last move (previousorigin .. currentorigin) a stepping entity is drawn now: over
the time to its next think as the message that moved it said (movelerpfinish), not as the latest message says. (The
latest's lerpfinish is its next frame's: once a stepping monster stood still through frames sent with a lerpfinish, as a
knocked-down monster getting up does after its move to where it stands, each new frame stretched the finished move and
it was drawn back along it and slid forward again: the get-up's jitter.)
=================
*/
float R_MoveLerpBlend (const entity_t *e)
{
	float s = (cls.demoplayback && cls.demospeed < 0.f) ? -1.f : 1.f;
	if (e->movelerpfinish > e->movelerpstart)
		return CLAMP (0.0f, (float)(cl.time - e->movelerpstart) / (e->movelerpfinish - e->movelerpstart), 1.0f);
	return CLAMP (0.0f, (float)(cl.time - e->movelerpstart) / 0.1f * s, 1.0f);
}

/*
=================
R_MoveLerpStart -- QVR: where a stepping entity's new move (begun this frame) starts from: where its last move is drawn
now (vr_monster_lerp_continue 1), not that move's end. A move begun before the last one was drawn to its end (a monster
moved again a frame or two after a step: a dog's 64-unit steps, then a nudge or a turn) snapped it to that end: the
dogs seemed to teleport as they turned. With the last move drawn to its end (or the switch off): its end, as Quake's.
=================
*/
void R_MoveLerpStart (const entity_t *e, vec3_t origin, vec3_t angles)
{
	float blend = 1.f;
	int i;
	if (e->movelerpstart > 0 && VR_MoveLerpContinuous ())
		blend = R_MoveLerpBlend (e);
	for (i = 0; i < 3; i++)
	{
		float d = e->currentangles[i] - e->previousangles[i];
		if (d > 180) d -= 360;
		if (d < -180) d += 360;
		origin[i] = e->previousorigin[i] + (e->currentorigin[i] - e->previousorigin[i]) * blend;
		angles[i] = e->previousangles[i] + d * blend;
	}
}

/*
=================
R_SetupEntityTransform -- johnfitz -- set up transform part of lerpdata
=================
*/
void R_SetupEntityTransform (entity_t *e, lerpdata_t *lerpdata)
{
	float blend;
	vec3_t d;
	int i;

	// if LERP_RESETMOVE, kill any lerps in progress
	if (e->lerpflags & LERP_RESETMOVE)
	{
		e->movelerpstart = 0;
		e->movelerpfinish = 0; // QVR
		VectorCopy (e->origin, e->previousorigin);
		VectorCopy (e->origin, e->currentorigin);
		VectorCopy (e->angles, e->previousangles);
		VectorCopy (e->angles, e->currentangles);
		e->lerpflags -= LERP_RESETMOVE;
	}
	else if (!VectorCompare (e->origin, e->currentorigin) || !VectorCompare (e->angles, e->currentangles)) // origin/angles changed, start new lerp
	{
		if (VR_DebugDrawnOn ()) // QVR: the drawn moves' log: what started this lerp
			VR_DebugDrawnRestart (e, !VectorCompare (e->origin, e->currentorigin));
		vec3_t startorigin, startangles; // QVR: from where it is drawn (R_MoveLerpStart)
		R_MoveLerpStart (e, startorigin, startangles);
		e->movelerpstart = cl.time;
		e->movelerpfinish = (e->lerpflags & LERP_FINISH) ? e->lerpfinish : 0.f; // QVR: this move's (R_MoveLerpBlend)
		VectorCopy (startorigin, e->previousorigin);
		VectorCopy (e->origin,  e->currentorigin);
		VectorCopy (startangles, e->previousangles);
		VectorCopy (e->angles,  e->currentangles);
	}

	//set up values
	if (r_lerpmove.value && e != &cl.viewent && e->lerpflags & LERP_MOVESTEP)
	{
		blend = R_MoveLerpBlend (e); // QVR

		//translation
		VectorSubtract (e->currentorigin, e->previousorigin, d);
		lerpdata->origin[0] = e->previousorigin[0] + d[0] * blend;
		lerpdata->origin[1] = e->previousorigin[1] + d[1] * blend;
		lerpdata->origin[2] = e->previousorigin[2] + d[2] * blend;

		//rotation
		VectorSubtract (e->currentangles, e->previousangles, d);
		for (i = 0; i < 3; i++)
		{
			if (d[i] > 180)  d[i] -= 360;
			if (d[i] < -180) d[i] += 360;
		}
		lerpdata->angles[0] = e->previousangles[0] + d[0] * blend;
		lerpdata->angles[1] = e->previousangles[1] + d[1] * blend;
		lerpdata->angles[2] = e->previousangles[2] + d[2] * blend;
	}
	else //don't lerp
	{
		VectorCopy (e->origin, lerpdata->origin);
		VectorCopy (e->angles, lerpdata->angles);
	}

	// chasecam
	if (chase_active.value && e == &cl_entities[cl.viewentity])
		lerpdata->angles[PITCH] *= 0.3f;

	if (VR_DebugDrawnOn ()) // QVR: a stepping monster's drawn place jumping between frames, logged
		VR_DebugDrawnMove (e, lerpdata->origin);
}

/*
=================
R_SetupAliasLighting -- johnfitz -- broken out from R_DrawAliasModel and rewritten
=================
*/
// QVR: R_SetupAliasLighting in two: the light at its origin (the same for both eyes: R_AliasEyeSlot), then the rest
static void R_AliasLightPoint (entity_t *e)
{
	// if the initial trace is completely black, try again from above
	// this helps with models whose origin is slightly below ground level
	// (e.g. some of the candles in the DOTM start map)
	vec3_t		lightorg;
	VR_AliasLightOrigin (e, lightorg); // QVR: the origin, or a climbing hand's as if not moved for looks
	if (!R_LightPoint (lightorg, 0.f, &e->lightcache))
		R_LightPoint (lightorg, e->model->maxs[2] * 0.5f, &e->lightcache);
}

static void R_AliasLightFinish (entity_t *e)
{
	vec3_t		dist;
	float		add;
	int			i;
	qboolean	parity = VR_ModelLightParity (); // QVR: on a par with the world: the shader shades 0.6 .. 1.4 by the normal

	//add dlights
	for (i=0; i<r_framedata.numlights && !VR_ModelDlightsPerPixel (); i++) // QVR: or the shader does, per pixel
	{
		gpulight_t *l = &r_lightbuffer.lights[i];
		if (l->shadow[3] != 0.f) // QVR: a map light's shadow entry, not a light
			continue;
		VectorSubtract (e->origin, l->pos, dist);
		add = DotProduct (dist, dist);
		if (l->radius * l->radius > add)
			VectorMA (lightcolor, (l->radius - sqrtf (add)) * VR_SpotCone (l, e->origin), l->color, lightcolor); // QVR: a spot light's cone
	}

	VR_AliasLightCurve (lightcolor); // QVR: the world's lightmap contrast

	// minimum light value on gun (24)
	if (e == &cl.viewent || VR_IsViewEntity (e)) // QVR
	{
		add = 3.0f * VR_ViewModelMinLight () - (lightcolor[0] + lightcolor[1] + lightcolor[2]); // QVR: vr_viewmodel_minlight
		if (add > 0.0f)
		{
			add *= 1.0f / 3.0f;
			lightcolor[0] += add;
			lightcolor[1] += add;
			lightcolor[2] += add;
		}
	}

	// minimum light value on players (8)
	if (e > cl_entities && e <= cl_entities + cl.maxclients)
	{
		add = 24.0f - (lightcolor[0] + lightcolor[1] + lightcolor[2]);
		if (add > 0.0f)
		{
			add *= 1.0f / 3.0f;
			lightcolor[0] += add;
			lightcolor[1] += add;
			lightcolor[2] += add;
		}
	}

	// clamp lighting so it doesn't overbright as much (96)
	if (gl_overbright_models.value && !parity) // QVR: on a par with the world, as bright as it
	{
		add = lightcolor[0] + lightcolor[1] + lightcolor[2];
		if (add > 288.0f)
			VectorScale(lightcolor, 288.0f / add, lightcolor);
	}
	//hack up the brightness when fullbrights but no overbrights (256)
	else if (e->model->flags & MOD_FBRIGHTHACK && gl_fullbrights.value)
	{
		lightcolor[0] = 256.0f;
		lightcolor[1] = 256.0f;
		lightcolor[2] = 256.0f;
	}

	if (parity) // QVR: 128 is full light, as on the world (the shader doubles overbright models)
		VectorScale (lightcolor, gl_overbright_models.value ? 1.0f / 256.0f : 1.0f / 128.0f, lightcolor);
	else
	VectorScale (lightcolor, 1.0f / 200.0f, lightcolor);
	VR_AliasLightModifier (e, lightcolor); // QVR
}

void R_SetupAliasLighting (entity_t	*e)
{
	R_AliasLightPoint (e);
	R_AliasLightFinish (e);
}

/*
=================
R_FlushAliasInstances
=================
*/
void R_FlushAliasInstances (qboolean showtris)
{
	VR_TRACY_SCOPE ("alias flush"); // QVR: the batch's upload and draw (profiling builds)
	extern cvar_t r_softemu_mdl_warp;
	qmodel_t* model;
	aliashdr_t* mainhdr, *hdr;
	qboolean	alphatest, translucent, oit;
	qboolean	a2c; // QVR
	qboolean	layered; // QVR: into all their faces at once (aliasfaces)
	qboolean	depthonly, notex; // QVR: a caster without a fragment shader; and so no textures bound for it
	int			drawcount; // QVR: instances drawn (layered: an instance once per face)
	int			i, f; // QVR
	int			totalverts;
	int			poseverttype;
	int			skinnum, anim, mode;
	unsigned	state, opaque_state, transparent_state;
	GLuint		buf;
	GLbyte* ofs;
	size_t		ibuf_size;
	GLuint		buffers[2];
	GLintptr	offsets[2];
	GLsizeiptr	sizes[2];
	gltexture_t* textures[4];	// QVR: and the normal map
	gltexture_t* glovepaint[2];	// QVR: a glove's painted leather (vr_glove_paint)
	const float	*vrbones;	// QVR: bone matrices of an IK-posed body
	int			numvrbones;	// QVR
	GLuint		vrbonebuf;	// QVR
	GLbyte		*vrboneofs;	// QVR

	if (!ibuf.count)
		return;

	model = ibuf.ent->model;
	mainhdr = (aliashdr_t*)Mod_Extradata (model);

	// QVR: layered shadow casters: a draw per instance and face it reaches (the faces in aliasinstfaces)
	layered = aliasdepth && aliasfaces && glprogs.alias_depth_layered[mainhdr->poseverttype];
	drawcount = ibuf.count;
	if (layered)
	{
		drawcount = 0;
		for (i = 0; i < ibuf.count; i++)
			for (f = 0; f < 6; f++)
				if (aliasinstfaces[i] & (1 << f))
					aliasfacebuf.draws[drawcount++] = (uint32_t)i | ((uint32_t)f << 16);
		if (!drawcount)
		{
			ibuf.count = 0;
			return;
		}
	}
	anim = (int)(cl.time * 10) & 3;

	GL_BeginGroup (model->name);
	poseverttype = mainhdr->poseverttype;

	alphatest = model->flags & MF_HOLEY ? 1 : 0;
	translucent = !ENTALPHA_OPAQUE (ibuf.ent->alpha);
	oit = translucent && R_GetEffectiveAlphaMode () == ALPHAMODE_OIT;
	switch (softemu)
	{
	case SOFTEMU_BANDED:
		mode = r_softemu_mdl_warp.value != 0.f ? ALIASSHADER_NOPERSP : ALIASSHADER_STANDARD;
		break;
	case SOFTEMU_COARSE:
		mode = r_softemu_mdl_warp.value > 0.f ? ALIASSHADER_NOPERSP : ALIASSHADER_DITHER;
		break;
	default:
		mode = r_softemu_mdl_warp.value > 0.f ? ALIASSHADER_NOPERSP : ALIASSHADER_STANDARD;
		break;
	}
	depthonly = aliasdepth && !alphatest && !translucent; // QVR: a shadow map's opaque casters: no fragment shader (holey skins keep theirs)
	if (depthonly)
		GL_UseProgram (layered ? glprogs.alias_depth_layered[poseverttype] : glprogs.alias_depth[poseverttype]);
	else
		GL_UseProgram (glprogs.alias[oit][mode][alphatest][poseverttype]);
	notex = depthonly && aliasdepthshare; // QVR: none of the fragment shader's textures bound (vr_shadow_setup_share)
	if (layered) // QVR: the faces' view-projections and the draws (holey skins are drawn a face at a time: vr_lighting.cpp)
	{
		GLuint		facebuf;
		GLbyte		*faceofs;
		GLsizeiptr	facesize = (GLsizeiptr)(sizeof (aliasfacebuf.viewproj) + sizeof (aliasfacebuf.draws[0]) * drawcount);
		GL_Upload (GL_SHADER_STORAGE_BUFFER, &aliasfacebuf, facesize, &facebuf, &faceofs);
		GL_BindBufferRange (GL_SHADER_STORAGE_BUFFER, 7, facebuf, (GLintptr)faceofs, facesize);
	}

	glEnable (GL_CLIP_DISTANCE1); // QVR: each instance keeps its half of the teleporter
	VR_AliasShadowClip (); // QVR: clipped virtual-light shadow, or a disabled plane

	if (poseverttype == PV_IQM)
		state = GLS_CULL_BACK | GLS_ATTRIBS (5);
	else
		state = GLS_CULL_BACK | GLS_ATTRIBS (1);
	if (VR_AliasMirrored (ibuf.ent)) // QVR: mirroring flips the winding
		state = (state & ~GLS_MASK_CULL) | GLS_CULL_FRONT;
	if (aliasnear & 2) // QVR: an eye inside a held item: its inside blocks the view (vr_nearclip_held 2)
		state = (state & ~GLS_MASK_CULL) | GLS_CULL_NONE;
	if (aliasnear & 1) // QVR: a held item nearer than the near plane is drawn at it, not cut (vr_nearclip_held)
		glEnable (GL_DEPTH_CLAMP);

	opaque_state = (state | GLS_BLEND_OPAQUE) & ~(GLS_BLEND_ALPHA_OIT | GLS_NO_ZWRITE);
	transparent_state = (state | GLS_BLEND_ALPHA) & ~(GLS_BLEND_OPAQUE | GLS_CULL_BACK);

	if (translucent)
	{
		GL_SetState ((state | GLS_BLEND_ALPHA_OIT | GLS_NO_ZWRITE) & ~GLS_CULL_BACK);
	}

	memcpy (ibuf.global.matviewproj, r_matviewproj, sizeof (r_matviewproj));
	memcpy (ibuf.global.eyepos, r_refdef.vieworg, sizeof (r_refdef.vieworg));
	memcpy (ibuf.global.fog, r_framedata.fogdata, 3 * sizeof (float));
	ibuf.global.fog[3] =
		gl_overbright_models.value ?
		-fabs (r_framedata.fogdata[3]) :
		fabs (r_framedata.fogdata[3])
		;
	ibuf.global.dither = r_framedata.screendither;

	ibuf_size = sizeof (ibuf.global) + sizeof (ibuf.inst[0]) * ibuf.count;
	GL_Upload (GL_SHADER_STORAGE_BUFFER, &ibuf.global, ibuf_size, &buf, &ofs);
	vr_profcounts.aliasdrawn += drawcount; // QVR: profile (a model drawn into a view: layered, once per face)

	numvrbones = poseverttype == PV_IQM ? (aliasdepth ? VR_AliasShadowBonePoses (ibuf.ent, &vrbones) : VR_AliasBonePoses (ibuf.ent, &vrbones)) : 0; // QVR: the shadow maps' with your head
	if (numvrbones) // QVR
		GL_Upload (GL_SHADER_STORAGE_BUFFER, vrbones, sizeof (bonepose_t) * numvrbones, &vrbonebuf, &vrboneofs);

	for (hdr = mainhdr, totalverts = 0; hdr; hdr = Mod_NextSurface (hdr))
		totalverts += hdr->numverts_vbo;

	buffers[0] = buf;
	offsets[0] = (GLintptr)ofs;
	sizes[0] = ibuf_size;
	switch (poseverttype)
	{
	case PV_IQM:
		if (numvrbones) // QVR: the entity's own bone matrices instead of the model's poses
		{
			buffers[1] = vrbonebuf; offsets[1] = (GLintptr)vrboneofs; sizes[1] = sizeof (bonepose_t) * numvrbones; // QVR
			break; // QVR
		} // QVR
		buffers[1] = model->meshvbo; offsets[1] = mainhdr->vboposeofs; sizes[1] = sizeof (bonepose_t) * mainhdr->numbones * mainhdr->numposes;
		break;
	case PV_MD3:
		buffers[1] = model->meshvbo; offsets[1] = mainhdr->vbovertofs; sizes[1] = sizeof (md3pose_t) * totalverts * mainhdr->numposes;
		break;
	case PV_QUAKE1:
		buffers[1] = model->meshvbo; offsets[1] = mainhdr->vbovertofs; sizes[1] = sizeof (meshxyz_t) * totalverts * mainhdr->numposes;
		break;
	default:
		return;
	}

	GL_BindBuffer (GL_ARRAY_BUFFER, model->meshvbo);
	GL_BindBuffer (GL_ELEMENT_ARRAY_BUFFER, model->meshindexesvbo);
	GL_BindBuffersRange (GL_SHADER_STORAGE_BUFFER, 1, 2, buffers, offsets, sizes);
	if (poseverttype == PV_QUAKE1) // QVR: the dequantised offsets (PoseFrac: vr/vr_mdldequant.cpp)
		GL_BindBufferRange (GL_SHADER_STORAGE_BUFFER, 11, model->meshvbo, mainhdr->vbofracofs, sizeof (unsigned) * totalverts * mainhdr->numposes);
	VR_AnimSmoothBind (ibuf.ent, 0); // QVR: r_lerpmodels 3's swings (the model's rig: its rest pose, the instances' bones)
	VR_DentBind (); // QVR: the flesh dents (Dents: vr/vr_dents.cpp)
	if (!notex) // QVR: (the fragment shader's only)
	{
		GL_BindNative (GL_TEXTURE14, GL_TEXTURE_CUBE_MAP, VR_EnvCubeTexture ()); // QVR: the reflections' cube map (EnvCube)
		GL_BindNative (GL_TEXTURE13, GL_TEXTURE_2D_ARRAY, VR_WoundTexture ()); // QVR: the wound masks (WoundMasks)
		GL_BindNative (GL_TEXTURE15, GL_TEXTURE_2D_ARRAY, VR_WoundFineTexture ()); // QVR: your own finer ones (WoundMasksFine)
		GL_BindNative (GL_TEXTURE10, GL_TEXTURE_2D_ARRAY, VR_WoundBloodTexture ()); // QVR: the blood on them not yours (WoundBloodFine)
	}

	if (poseverttype == PV_IQM)
	{
		GL_VertexAttribPointerFunc (0, 3, GL_FLOAT, GL_FALSE, sizeof (iqmvert_t), (void*)(mainhdr->vbovertofs + offsetof (iqmvert_t, xyz)));
		GL_VertexAttribPointerFunc (1, 4, GL_BYTE, GL_TRUE, sizeof (iqmvert_t), (void*)(mainhdr->vbovertofs + offsetof (iqmvert_t, norm)));
		GL_VertexAttribPointerFunc (2, 2, GL_FLOAT, GL_FALSE, sizeof (iqmvert_t), (void*)(mainhdr->vbovertofs + offsetof (iqmvert_t, st)));
		GL_VertexAttribPointerFunc (3, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof (iqmvert_t), (void*)(mainhdr->vbovertofs + offsetof (iqmvert_t, weight)));
		GL_VertexAttribIPointerFunc (4, 4, GL_UNSIGNED_BYTE, sizeof (iqmvert_t), (void*)(mainhdr->vbovertofs + offsetof (iqmvert_t, idx)));
	}
	else // PV_QUAKE1 || PV_MD3
	{
		GL_VertexAttribPointerFunc (0, 2, GL_FLOAT, GL_FALSE, sizeof (meshst_t), (void*)mainhdr->vbostofs);
	}

	if (!translucent)
		GL_SetState (opaque_state);
	VR_TemporalMotionWrites (!translucent && !aliasdepth); // QVR: the opaque ones write their motion vectors (vr/vr_temporal.cpp)
	if (!translucent && !aliasdepth && !alphatest && !showtris && !gl_multiview_draw && VR_AliasDepthPrepass (ibuf.ent))
	{ // QVR: your body's depth first (vr_body_prepass): its opaque skins with no fragment shader, the same instances and
	  // the same invariant gl_Position, so that the shading below runs once a pixel (it passes the depth equal, no writes)
		VR_GPUZoneBegin ("body depth prepass");
		GL_UseProgram (glprogs.alias_depth[poseverttype]);
		VR_AliasShadowClip (); // (its own program's plane, the same as the shading's)
		glColorMask (GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
		for (hdr = VR_AliasSurfaceFirst (ibuf.ent, mainhdr); hdr; hdr = VR_AliasSurfaceNext (ibuf.ent, mainhdr, hdr)) // QVR: a glove's shell first
		{
			skinnum = ibuf.ent->skinnum;
			if ((skinnum >= hdr->numskins) || (skinnum < 0)) skinnum = 0;
			if (!hdr->gltextures[skinnum][anim] || (hdr->gltextures[skinnum][anim]->flags & TEXPREF_ALPHAPIXELS))
				continue;
			GL_DrawElementsInstancedFunc (GL_TRIANGLES, hdr->numindexes, GL_UNSIGNED_SHORT, (void*)hdr->eboofs, drawcount);
		}
		glColorMask (GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		GL_UseProgram (glprogs.alias[oit][mode][alphatest][poseverttype]);
		GL_SetState (opaque_state | GLS_NO_ZWRITE);
		VR_GPUZoneEnd ();
	}
	if (!notex)
		VR_RetroBind (QVR_RETRO_LUT_UNIT_ALIAS); // QVR: the palette's table for retro textures (vr/vr_retro.cpp)
	a2c = alphatest && !translucent && VR_AlphaToCoverage (); // QVR: holey skins' edges by alpha to coverage (vr_alpha_coverage, MSAA)
	if (a2c)
	{
		glEnable (GL_SAMPLE_ALPHA_TO_COVERAGE);
		glEnable (GL_SAMPLE_ALPHA_TO_ONE);
	}

	for (hdr = VR_AliasSurfaceFirst (ibuf.ent, mainhdr); hdr; hdr = VR_AliasSurfaceNext (ibuf.ent, mainhdr, hdr)) // QVR: a glove's shell first
	{
		skinnum = ibuf.ent->skinnum;
		if ((skinnum >= hdr->numskins) || (skinnum < 0)) skinnum = 0;
		textures[0] = hdr->gltextures[skinnum][anim];
		if (!textures[0]) continue;

		if (!translucent && (textures[0]->flags & TEXPREF_ALPHAPIXELS))
		{
			continue;
		}
		if (notex) // QVR: (no fragment shader to read them)
		{
			GL_DrawElementsInstancedFunc (GL_TRIANGLES, hdr->numindexes, GL_UNSIGNED_SHORT, (void*)hdr->eboofs, drawcount);
			rs_aliaspasses += hdr->numtris * drawcount;
			continue;
		}
		textures[1] = hdr->fbtextures[skinnum][anim];
		if (hdr == mainhdr && ibuf.ent->colormap != vid.colormap && !gl_nocolors.value)
			if (CL_IsPlayerEnt (ibuf.ent)) textures[0] = playertextures[ibuf.ent - cl_entities - 1];
		if (!gl_fullbrights.value) textures[1] = blacktexture;
		if (r_lightmap_cheatsafe) { textures[0] = greytexture; textures[1] = blacktexture; }
		if (!textures[1]) textures[1] = blacktexture;
		if (showtris) { textures[0] = blacktexture; textures[1] = whitetexture; }
		textures[2] = TexMgr_NormalMap (showtris || r_lightmap_cheatsafe ? NULL : hdr->gltextures[skinnum][anim]); // QVR

		textures[3] = showtris || r_lightmap_cheatsafe ? blacktexture : VR_AliasSkinMask (ibuf.ent);
		GL_BindTextures (0, 3, textures); // QVR: normal map
		GL_BindTextures (6, 1, &textures[3]); // QVR: skin mask, separate from retro LUT on unit 3
		VR_AliasGlovePaint (ibuf.ent, glovepaint); // QVR: a glove's painted leather (GloveTex, GloveHeightTex)
		GL_BindTextures (7, 2, glovepaint);
		GL_DrawElementsInstancedFunc (GL_TRIANGLES, hdr->numindexes, GL_UNSIGNED_SHORT, (void*)hdr->eboofs, drawcount); // QVR: drawcount
		rs_aliaspasses += hdr->numtris * drawcount;
	}

	if (a2c) // QVR
	{
		glDisable (GL_SAMPLE_ALPHA_TO_COVERAGE);
		glDisable (GL_SAMPLE_ALPHA_TO_ONE);
	}

	if (!translucent)
	{
		GL_SetState (transparent_state);

		for (hdr = VR_AliasSurfaceFirst (ibuf.ent, mainhdr); hdr; hdr = VR_AliasSurfaceNext (ibuf.ent, mainhdr, hdr)) // QVR: a glove's shell first
		{
			skinnum = ibuf.ent->skinnum;
			if ((skinnum >= hdr->numskins) || (skinnum < 0)) skinnum = 0;
			textures[0] = hdr->gltextures[skinnum][anim];
			if (!textures[0]) continue;

			if (!(textures[0]->flags & TEXPREF_ALPHAPIXELS))
			{
				continue;
			}
			VR_TemporalMotionWrites (0); // QVR: (not blended into the motion target)
			if (notex) // QVR: (no fragment shader to read them)
			{
				GL_DrawElementsInstancedFunc (GL_TRIANGLES, hdr->numindexes, GL_UNSIGNED_SHORT, (void*)hdr->eboofs, drawcount);
				rs_aliaspasses += hdr->numtris * drawcount;
				continue;
			}

			textures[1] = hdr->fbtextures[skinnum][anim];
			if (hdr == mainhdr && ibuf.ent->colormap != vid.colormap && !gl_nocolors.value)
				if (CL_IsPlayerEnt (ibuf.ent)) textures[0] = playertextures[ibuf.ent - cl_entities - 1];
			if (!gl_fullbrights.value) textures[1] = blacktexture;
			if (r_lightmap_cheatsafe) { textures[0] = greytexture; textures[1] = blacktexture; }
			if (!textures[1]) textures[1] = blacktexture;
			if (showtris) { textures[0] = blacktexture; textures[1] = whitetexture; }
			textures[2] = TexMgr_NormalMap (showtris || r_lightmap_cheatsafe ? NULL : hdr->gltextures[skinnum][anim]); // QVR

			textures[3] = showtris || r_lightmap_cheatsafe ? blacktexture : VR_AliasSkinMask (ibuf.ent);
			GL_BindTextures (0, 3, textures); // QVR: normal map
			GL_BindTextures (6, 1, &textures[3]); // QVR: skin mask, separate from retro LUT on unit 3
			VR_AliasGlovePaint (ibuf.ent, glovepaint); // QVR: a glove's painted leather (GloveTex, GloveHeightTex)
			GL_BindTextures (7, 2, glovepaint);
			GL_DrawElementsInstancedFunc (GL_TRIANGLES, hdr->numindexes, GL_UNSIGNED_SHORT, (void*)hdr->eboofs, drawcount); // QVR: drawcount
			rs_aliaspasses += hdr->numtris * drawcount;
		}

	}

	if (aliasnear & 1) // QVR
		glDisable (GL_DEPTH_CLAMP);

	glDisable (GL_CLIP_DISTANCE1);
	ibuf.count = 0;
	GL_EndGroup ();
}

/*
=================
R_Alias_CanAddToBatch
=================
*/
static qboolean R_Alias_CanAddToBatch (const entity_t *e)
{
	// empty batch
	if (!ibuf.count)
		return true;

	// full batch
	if (ibuf.count == countof (ibuf.inst))
		return false;

	// different models/skins
	if (ibuf.ent->model != e->model || ibuf.ent->skinnum != e->skinnum)
		return false;

	// players have custom colors
	if (!gl_nocolors.value && CL_IsPlayerEnt (ibuf.ent))
		return false;

	if (VR_AliasMirrored (ibuf.ent) != VR_AliasMirrored (e)) // QVR
		return false;
	if (VR_AliasBonePoses (ibuf.ent, NULL) || VR_AliasBonePoses (e, NULL)) // QVR: bone matrices are per entity
		return false;

	return true;
}

static void R_ExtractPosePosition (const bonepose_t *pose, vec3_t out)
{
	out[0] = pose->mat[3];
	out[1] = pose->mat[7];
	out[2] = pose->mat[11];
}

static void R_GetBonePosition (const bonepose_t *root, const bonepose_t *bindpose, const bonepose_t *animpose, vec3_t out)
{
	vec3_t base, anim;
	R_ExtractPosePosition (bindpose, base);
	Matrix3x4_RM_Transform3 (animpose->mat, base, anim);
	Matrix3x4_RM_Transform3 (root->mat, anim, out);
}

static void R_GetLerpedBonePosition (const bonepose_t *root, const bonepose_t *bindpose, const bonepose_t *frame1, const bonepose_t *frame2, float t, vec3_t out)
{
	vec3_t pos1, pos2;
	R_GetBonePosition (root, bindpose, frame1, pos1);
	R_GetBonePosition (root, bindpose, frame2, pos2);
	VectorLerp (pos1, pos2, t, out); 
}

static void R_DrawSkeleton (const aliashdr_t *paliashdr, const float model_matrix[16], const lerpdata_t *lerpdata)
{
	bonepose_t root;
	const bonepose_t *bindpose;
	const bonepose_t *animdata;
	const bonepose_t *frame1, *frame2;
	const boneinfo_t *boneinfo;
	vec3_t *positions;
	int i, mark;

	if (paliashdr->poseverttype != PV_IQM)
		return;

	mark = Hunk_LowMark ();
	positions = (vec3_t *) Hunk_AllocNoFill (sizeof (*positions) * paliashdr->numbones);

	bindpose = (const bonepose_t *) ((const byte *) paliashdr + paliashdr->bindpose);
	boneinfo = (const boneinfo_t *) ((const byte *) paliashdr + paliashdr->boneinfo);
	animdata = (const bonepose_t *) ((const byte *) paliashdr + paliashdr->boneposedata);
	frame1 = animdata + paliashdr->numbones * lerpdata->pose1;
	frame2 = animdata + paliashdr->numbones * lerpdata->pose2;

	root.mat[0] = model_matrix[0]; root.mat[1] = model_matrix[4]; root.mat[2] = model_matrix[8]; root.mat[3] = model_matrix[12];
	root.mat[4] = model_matrix[1]; root.mat[5] = model_matrix[5]; root.mat[6] = model_matrix[9]; root.mat[7] = model_matrix[13];
	root.mat[8] = model_matrix[2]; root.mat[9] = model_matrix[6]; root.mat[10] = model_matrix[10]; root.mat[11] = model_matrix[14];

	for (i = 0; i < paliashdr->numbones; i++)
		R_GetLerpedBonePosition (&root, bindpose + i, frame1 + i, frame2 + i, lerpdata->blend, positions[i]);

	for (i = 0; i < paliashdr->numbones; i++)
	{
		int parent = boneinfo[i].parent;
		if (parent < 0)
			continue;
		// skip lines starting from root (can get too busy for models with multiple parts, e.g. ogre)
		if (boneinfo[parent].parent < 0)
			continue;
		R_EmitLine (positions[parent], positions[i], 0xFFFF00FFu);
	}

	Hunk_FreeToLowMark (mark);
}

/*
=================
R_AliasSmoothInstance -- QVR

r_lerpmodels 3 (vr/vr_animsmooth.cpp): the instance drawn along a spline through four poses (its InstanceData's Smooth
and SmoothW), its rig's limbs swung round their joints; off (zeros) at a pose itself, whatever the setting: there it is
drawn as r_lerpmodels 1 draws it. `batchindex`: its place in its batch (0 starts one), `channel` 0 the batches, 1 the
wound paint (VR_AnimSmoothBind).
=================
*/
static void R_AliasSmoothInstance (const entity_t *e, const aliashdr_t *hdr, const lerpdata_t *lerpdata, int totalverts, aliasinstance_t *inst, int batchindex, int channel)
{
	int		smooth[4];
	float	weights[4];
	memset (inst->vr.smooth, 0, sizeof (inst->vr.smooth));
	memset (inst->vr.smoothw, 0, sizeof (inst->vr.smoothw));
	if (!VR_AnimSmooth (e, hdr, lerpdata->pose1, lerpdata->pose2, lerpdata->blend, inst->padding, batchindex, channel, smooth, weights))
		return;
	inst->vr.smooth[0] = smooth[0] * totalverts;
	inst->vr.smooth[1] = smooth[1] * totalverts;
	inst->vr.smooth[2] = smooth[2];
	inst->vr.smooth[3] = smooth[3]; // (1 on; 2 + the first row of its hit reaction's kicked bones: vr/vr_hitreact.cpp)
	memcpy (inst->vr.smoothw, weights, sizeof (weights));
}

/*
=================
R_DrawAliasModel_Real
=================
*/
/*
=================
R_AliasTransform -- QVR: (from R_DrawAliasModel_Real) the lerp and the model's matrices: model_matrix with its
vertices' scale, bounds_matrix without
=================
*/
static void R_AliasTransform (entity_t *e, aliashdr_t *paliashdr, lerpdata_t *lerpdata, float model_matrix[16], float bounds_matrix[16])
{
	float fovscale = 1.0f;

	VR_TRACY_DETAIL (vr_tracy_render_zones, "alias frame", R_SetupAliasFrame (e, paliashdr, lerpdata)); // QVR: (vr_tracy_render_detail)
	R_SetupEntityTransform (e, lerpdata);

	if (lerpdata->pose1 == lerpdata->pose2)
		lerpdata->blend = 0.f;

	//
	// viewmodel adjustments (position, fov distortion correction)
	//
	if (e == &cl.viewent)
	{
		if (r_refdef.basefov > 90.f && cl_gun_fovscale.value)
		{
			fovscale = tan (r_refdef.basefov * (0.5f * M_PI / 180.f));
			fovscale = 1.f + (fovscale - 1.f) * cl_gun_fovscale.value;
		}

		VectorMA (lerpdata->origin, cl_gun_x.value * paliashdr->scale[0] * fovscale,	vright,	lerpdata->origin);
		VectorMA (lerpdata->origin, cl_gun_y.value * paliashdr->scale[1] * fovscale,	vup,	lerpdata->origin);
		VectorMA (lerpdata->origin, cl_gun_z.value * paliashdr->scale[2],			vpn,	lerpdata->origin);
	}

	//
	// transform it
	//
	R_EntityMatrix (model_matrix, lerpdata->origin, lerpdata->angles, e->scale);
	VR_AliasPreTransform (e, model_matrix); // QVR
	memcpy (bounds_matrix, model_matrix, sizeof (float[16]));
	ApplyTranslation (model_matrix, paliashdr->scale_origin[0], paliashdr->scale_origin[1] * fovscale, paliashdr->scale_origin[2] * fovscale);
	ApplyScale (model_matrix, paliashdr->scale[0], paliashdr->scale[1] * fovscale, paliashdr->scale[2] * fovscale);
	VR_AliasPostTransform (e, model_matrix); // QVR
}

/*
=================
R_AliasTransformShared -- QVR

R_AliasTransform in the first eye's scene (vr_shadow_setup_share): the lerp and matrices this frame's shadow pass set up
for the same entity (its model and frame unchanged) instead of working them out again; 2 works them out and compares
(vr_alias_eye_cache_stats prints how many were taken, checked and differed). Never cl.viewent (as the casters' cache),
nor while the drawn moves are logged (their order kept).
=================
*/
static const aliasdepthentry_t *R_AliasDepthFind (const entity_t *e)
{
	uint64_t h;
	int i;
	if (!aliasdepthlast || aliaseyemode != 1 || e == &cl.viewent || VR_DebugDrawnOn ())
		return NULL;
	h = ((uint64_t) (uintptr_t) e >> 4) * 0x9E3779B97F4A7C15ull;
	h >>= 64 - 12;
	for (i = 0; i < ALIASDEPTH_CACHE_PROBES; i++)
	{
		const aliasdepthentry_t *s = &aliasdepthcache[(h + i) & (ALIASDEPTH_CACHE_SIZE - 1)];
		if (s->gen != aliasdepthlast) // (as R_AliasDepthSlot: the first free slot ends the search)
			return NULL;
		if (s->ent == e)
			return s->model == e->model && s->frame == e->frame ? s : NULL;
	}
	return NULL;
}

static void R_AliasTransformShared (entity_t *e, aliashdr_t *paliashdr, lerpdata_t *lerpdata, float model_matrix[16], float bounds_matrix[16])
{
	const aliasdepthentry_t *s = R_AliasDepthFind (e);
	if (s && aliasdepthshare == 1)
	{
		*lerpdata = s->lerpdata;
		memcpy (model_matrix, s->model_matrix, sizeof (s->model_matrix));
		memcpy (bounds_matrix, s->bounds_matrix, sizeof (s->bounds_matrix));
		aliasdepthsharestats[ADS_TAKEN]++;
		return;
	}
	R_AliasTransform (e, paliashdr, lerpdata, model_matrix, bounds_matrix);
	if (s)
	{
		aliasdepthsharestats[ADS_CHECKED]++;
		if ((memcmp (lerpdata, &s->lerpdata, sizeof (*lerpdata)) || memcmp (model_matrix, s->model_matrix, sizeof (s->model_matrix)) ||
			memcmp (bounds_matrix, s->bounds_matrix, sizeof (s->bounds_matrix))) && aliasdepthsharestats[ADS_BAD]++ < 8)
			Con_Printf ("vr_shadow_setup_share check: %s's set-up differs from the shadow pass's\n", e->model->name);
	}
}

/*
=================
R_AliasPosedBox -- QVR

The IK-posed skeletal models (VR_AliasBonePoses: your body, its mirror image, the jointed hands, the ragdolls) were
never culled: their bones place them, past the model's bounds (a ragdoll's model says +-64). Their box: each bone's
box round the vertices it moves (in the bind pose, kept by model: R_AliasBoneBoxes) through the bone's matrix, all
together, then through the model's. A vertex is a blend of its bones' places with weights that sum to one, each of
them inside its bone's box so moved: inside their union. The cull (R_AliasCullPosed) is the eyes' (R_CullBox) and the
layered shadow faces' (R_AliasFacesOf); vr_alias_cull_posed 0 culls none, as before.
=================
*/
#define ALIASBONEBOX_CACHE		16
#define ALIASBONEBOX_SAMPLES	16	// vertices a surface for the signature
typedef struct {
	const aliashdr_t	*hdr;	// NULL: free
	int			numbones;
	int			numverts;
	unsigned	sig;		// a sample of the vertices (a ragdoll's model made again where another one was)
	int			frame;		// host_framecount when the signature last matched (checked once a frame)
	float		*box;		// numbones x 6: mins, maxs (mins above maxs: no vertex on it)
} aliasbonebox_t;
static aliasbonebox_t	aliasboneboxes[ALIASBONEBOX_CACHE];
static int				aliasboneboxnext;

static unsigned R_AliasBoneBoxSig (const aliashdr_t *hdr, int *numverts)
{
	unsigned h = 2166136261u;
	*numverts = 0;
	for (; hdr; hdr = Mod_NextSurface ((aliashdr_t *) hdr))
	{
		const iqmvert_t *verts = (const iqmvert_t *) ((const byte *) hdr + hdr->vertexes);
		int step = hdr->numverts_vbo / ALIASBONEBOX_SAMPLES + 1, v;
		size_t i;
		for (v = 0; v < hdr->numverts_vbo; v += step)
		{
			const byte *p = (const byte *) &verts[v];
			for (i = 0; i < sizeof (iqmvert_t); i++)
				h = (h ^ p[i]) * 16777619u;
		}
		*numverts += hdr->numverts_vbo;
	}
	return h;
}

static const float *R_AliasBoneBoxes (const aliashdr_t *mainhdr)
{
	const aliashdr_t *hdr;
	aliasbonebox_t *c;
	float *box;
	int numverts, i, v, k;
	unsigned sig;
	for (i = 0; i < ALIASBONEBOX_CACHE; i++)
	{
		c = &aliasboneboxes[i];
		if (c->hdr == mainhdr && c->numbones == mainhdr->numbones && c->frame == host_framecount)
			return c->box;
	}
	sig = R_AliasBoneBoxSig (mainhdr, &numverts);
	for (i = 0; i < ALIASBONEBOX_CACHE; i++)
	{
		c = &aliasboneboxes[i];
		if (c->hdr == mainhdr && c->numbones == mainhdr->numbones && c->numverts == numverts && c->sig == sig)
		{
			c->frame = host_framecount;
			return c->box;
		}
	}
	if (mainhdr->numbones <= 0)
		return NULL;
	c = &aliasboneboxes[aliasboneboxnext];
	aliasboneboxnext = (aliasboneboxnext + 1) % ALIASBONEBOX_CACHE;
	c->hdr = NULL;
	box = (float *) realloc (c->box, sizeof (float) * 6 * mainhdr->numbones);
	if (!box)
		return NULL;
	c->box = box;
	for (i = 0; i < mainhdr->numbones; i++)
	{
		box[6 * i + 0] = box[6 * i + 1] = box[6 * i + 2] = 1e30f;
		box[6 * i + 3] = box[6 * i + 4] = box[6 * i + 5] = -1e30f;
	}
	for (hdr = mainhdr; hdr; hdr = Mod_NextSurface ((aliashdr_t *) hdr))
	{
		const iqmvert_t *verts = (const iqmvert_t *) ((const byte *) hdr + hdr->vertexes);
		for (v = 0; v < hdr->numverts_vbo; v++)
			for (k = 0; k < 4; k++)
			{
				float *b;
				if (!verts[v].weight[k])
					continue;
				if (verts[v].idx[k] >= mainhdr->numbones)
					return NULL; // (left free)
				b = box + 6 * verts[v].idx[k];
				for (i = 0; i < 3; i++)
				{
					b[i] = q_min (b[i], verts[v].xyz[i]);
					b[3 + i] = q_max (b[3 + i], verts[v].xyz[i]);
				}
			}
	}
	c->hdr = mainhdr;
	c->numbones = mainhdr->numbones;
	c->numverts = numverts;
	c->sig = sig;
	c->frame = host_framecount;
	return box;
}

// the box lo..hi through a 4x4 matrix (columns: m[0..3] the first)
static void R_AliasBoxThrough4 (const float m[16], const vec3_t lo, const vec3_t hi, vec3_t mins, vec3_t maxs)
{
	int i;
	vec3_t c, ext;
	for (i = 0; i < 3; i++)
	{
		c[i] = (lo[i] + hi[i]) * 0.5f;
		ext[i] = (hi[i] - lo[i]) * 0.5f;
	}
	for (i = 0; i < 3; i++)
	{
		float wc = m[i] * c[0] + m[4 + i] * c[1] + m[8 + i] * c[2] + m[12 + i];
		float we = fabs (m[i]) * ext[0] + fabs (m[4 + i]) * ext[1] + fabs (m[8 + i]) * ext[2];
		mins[i] = wc - we;
		maxs[i] = wc + we;
	}
}

// e's world box as posed (model_matrix: its vertices' to the world); false: none (not posed, or not known)
// (cached: the shadow pass's entry for e, set up by an earlier light: its box taken while of the same bone matrices)
static qboolean R_AliasPosedBox (entity_t *e, aliashdr_t *hdr, const float model_matrix[16], vec3_t mins, vec3_t maxs,
	aliasdepthentry_t *cached, qboolean cachehit)
{
	const float *bones = NULL, *box;
	vec3_t lo = {1e30f, 1e30f, 1e30f}, hi = {-1e30f, -1e30f, -1e30f};
	int numbones, b, i;
	if (cached)
		cached->boxbones = cachehit ? cached->boxbones : NULL;
	if (hdr->poseverttype != PV_IQM)
		return false;
	numbones = aliasdepth ? VR_AliasShadowBonePoses (e, &bones) : VR_AliasBonePoses (e, &bones);
	if (cached && cachehit && bones && cached->boxbones == bones)
	{
		VectorCopy (cached->boxmins, mins);
		VectorCopy (cached->boxmaxs, maxs);
		return true;
	}
	if (!bones || numbones < hdr->numbones || !(box = R_AliasBoneBoxes (hdr)))
		return false;
	for (b = 0; b < hdr->numbones; b++)
	{
		const float *bb = box + 6 * b, *m = bones + 12 * b; // (3x4, by rows)
		vec3_t c, ext;
		if (bb[0] > bb[3])
			continue;
		for (i = 0; i < 3; i++)
		{
			c[i] = (bb[i] + bb[3 + i]) * 0.5f;
			ext[i] = (bb[3 + i] - bb[i]) * 0.5f;
		}
		for (i = 0; i < 3; i++)
		{
			const float *r = m + 4 * i;
			float pc = r[0] * c[0] + r[1] * c[1] + r[2] * c[2] + r[3];
			float pe = fabs (r[0]) * ext[0] + fabs (r[1]) * ext[1] + fabs (r[2]) * ext[2];
			lo[i] = q_min (lo[i], pc - pe);
			hi[i] = q_max (hi[i], pc + pe);
		}
	}
	if (lo[0] > hi[0])
		return false;
	R_AliasBoxThrough4 (model_matrix, lo, hi, mins, maxs);
	for (i = 0; i < 3; i++) // (the weights' bytes need not sum to exactly one)
	{
		mins[i] -= 1.f;
		maxs[i] += 1.f;
	}
	if (cached)
	{
		cached->boxbones = bones;
		VectorCopy (mins, cached->boxmins);
		VectorCopy (maxs, cached->boxmaxs);
	}
	return true;
}

// which of `faces` (the layered shadow faces, aliasfacebuf.viewproj) the box reaches: outside none of a face's four sides
static unsigned char R_AliasFacesOf (unsigned char faces, const vec3_t mins, const vec3_t maxs)
{
	unsigned char out = 0;
	int f, k;
	for (f = 0; f < 6; f++)
	{
		const float *m = aliasfacebuf.viewproj[f];
		int outside[4] = {0, 0, 0, 0};
		if (!(faces & (1u << f)))
			continue;
		for (k = 0; k < 8; k++)
		{
			float p[3], x, y, w;
			p[0] = (k & 1) ? maxs[0] : mins[0];
			p[1] = (k & 2) ? maxs[1] : mins[1];
			p[2] = (k & 4) ? maxs[2] : mins[2];
			x = m[0] * p[0] + m[4] * p[1] + m[8] * p[2] + m[12];
			y = m[1] * p[0] + m[5] * p[1] + m[9] * p[2] + m[13];
			w = m[3] * p[0] + m[7] * p[1] + m[11] * p[2] + m[15];
			outside[0] += x < -w;
			outside[1] += x > w;
			outside[2] += y < -w;
			outside[3] += y > w;
		}
		if (outside[0] < 8 && outside[1] < 8 && outside[2] < 8 && outside[3] < 8)
			out |= (unsigned char) (1u << f);
	}
	return out;
}

// vr_alias_cull_posed 2: e culled (from the eye's frustum, or from the layered faces `faces`) skinned again as the shader
// does it, its vertices through model_matrix: whether any lies where its box said none does (on the inner side of every
// side of the frustum or of a culled face: the box was wholly outside one of them, so all its vertices should be)
static qboolean R_AliasPosedWrong (entity_t *e, aliashdr_t *mainhdr, const float model_matrix[16], unsigned char faces)
{
	const float *bones = NULL;
	const aliashdr_t *hdr;
	unsigned char inside[7][4];
	int numbones = aliasdepth ? VR_AliasShadowBonePoses (e, &bones) : VR_AliasBonePoses (e, &bones);
	int f, v, k, i;
	if (!bones || numbones < mainhdr->numbones)
		return false;
	memset (inside, 0, sizeof (inside));
	for (hdr = mainhdr; hdr; hdr = Mod_NextSurface ((aliashdr_t *) hdr))
	{
		const iqmvert_t *verts = (const iqmvert_t *) ((const byte *) hdr + hdr->vertexes);
		for (v = 0; v < hdr->numverts_vbo; v++)
		{
			float m[12], pos[3], world[3];
			memset (m, 0, sizeof (m));
			for (k = 0; k < 4; k++)
				if (verts[v].weight[k] && verts[v].idx[k] < numbones)
					for (i = 0; i < 12; i++)
						m[i] += bones[12 * verts[v].idx[k] + i] * (verts[v].weight[k] / 255.f);
			for (i = 0; i < 3; i++)
				pos[i] = m[4 * i] * verts[v].xyz[0] + m[4 * i + 1] * verts[v].xyz[1] + m[4 * i + 2] * verts[v].xyz[2] + m[4 * i + 3];
			for (i = 0; i < 3; i++)
				world[i] = model_matrix[i] * pos[0] + model_matrix[4 + i] * pos[1] + model_matrix[8 + i] * pos[2] + model_matrix[12 + i];
			if (!aliasfaces)
			{
				for (i = 0; i < 4; i++)
					inside[6][i] |= DotProduct (frustum[i].normal, world) >= frustum[i].dist;
				continue;
			}
			for (f = 0; f < 6; f++)
			{
				const float *c = aliasfacebuf.viewproj[f];
				float x, y, w;
				if (!(faces & (1u << f)))
					continue;
				x = c[0] * world[0] + c[4] * world[1] + c[8] * world[2] + c[12];
				y = c[1] * world[0] + c[5] * world[1] + c[9] * world[2] + c[13];
				w = c[3] * world[0] + c[7] * world[1] + c[11] * world[2] + c[15];
				inside[f][0] |= x >= -w;
				inside[f][1] |= x <= w;
				inside[f][2] |= y >= -w;
				inside[f][3] |= y <= w;
			}
		}
	}
	for (f = 0; f < 7; f++)
		if (inside[f][0] && inside[f][1] && inside[f][2] && inside[f][3])
			return true;
	return false;
}

// vr_alias_cull_stats: the alias draws' culls since the last call, by model (R_AliasCullStats)
#define ALIASCULL_MODELS 48
typedef struct {
	const qmodel_t	*model;
	int		drawn[2];		// eyes, shadow faces: tested (a face each, layered)
	int		culled[2];		// ... culled
	int		posed;			// posed draws (eyes and faces)
	int		repeats;		// the same entity again in one eye's view (R_RenderView)
	int		wrong;			// vr_alias_cull_posed 2: culled with a vertex inside (none expected)
	double	tris[2];		// triangles submitted (eyes, shadow faces)
} aliascullstat_t;
static aliascullstat_t	aliascullstats[ALIASCULL_MODELS];
static int				aliascullframe0, aliascullview0;
static struct { const entity_t *ent; int frame; } aliascullseen[256];

static aliascullstat_t *R_AliasCullStat (const qmodel_t *model)
{
	int i;
	for (i = 0; i < ALIASCULL_MODELS; i++)
	{
		if (aliascullstats[i].model == model)
			return &aliascullstats[i];
		if (!aliascullstats[i].model)
		{
			aliascullstats[i].model = model;
			return &aliascullstats[i];
		}
	}
	return NULL;
}

static int R_AliasFaceCount (unsigned faces)
{
	int n = 0;
	for (; faces; faces &= faces - 1u)
		n++;
	return n;
}

static void R_AliasCullCount (const entity_t *e, aliashdr_t *hdr, int depth, int tested, int culled, qboolean posed, qboolean wrong)
{
	aliascullstat_t *s = R_AliasCullStat (e->model);
	if (!s)
		return;
	for (; hdr && tested > culled; hdr = Mod_NextSurface (hdr))
		s->tris[depth] += (double) hdr->numtris * (tested - culled);
	s->wrong += wrong;
	s->drawn[depth] += tested;
	s->culled[depth] += culled;
	s->posed += posed ? tested : 0;
	if (!depth && tested > culled)
	{
		int slot = (int) (((uintptr_t) e >> 4) & 255);
		if (aliascullseen[slot].ent == e && aliascullseen[slot].frame == r_framecount)
			s->repeats++;
		aliascullseen[slot].ent = e;
		aliascullseen[slot].frame = r_framecount;
	}
}

void R_AliasCullStats (void)
{
	int i, frames = q_max (host_framecount - aliascullframe0, 1), views = r_framecount - aliascullview0;
	int sum[6] = {0, 0, 0, 0, 0, 0};
	double tris[2] = {0, 0};
	Con_Printf ("vr_alias_cull_stats over %d frames, %d views (vr_alias_cull_posed %d), a frame: eyes drawn/culled, shadow faces drawn/culled, posed, repeats; wrongly culled in all\n",
		frames, views, VR_AliasCullPosedOn (NULL, 0));
	for (i = 0; i < ALIASCULL_MODELS && aliascullstats[i].model; i++)
	{
		const aliascullstat_t *s = &aliascullstats[i];
		Con_Printf ("  %-28s %6.1f %6.1f  %6.1f %6.1f  %5.1f %4.1f\n", s->model->name, s->drawn[0] / (float) frames,
			s->culled[0] / (float) frames, s->drawn[1] / (float) frames, s->culled[1] / (float) frames, s->posed / (float) frames,
			s->repeats / (float) frames);
		sum[0] += s->drawn[0]; sum[1] += s->culled[0]; sum[2] += s->drawn[1]; sum[3] += s->culled[1]; sum[4] += s->repeats; sum[5] += s->wrong; tris[0] += s->tris[0]; tris[1] += s->tris[1];
	}
	Con_Printf ("  %-28s %6.1f %6.1f  %6.1f %6.1f        %4.1f; wrong %d\n", "all", sum[0] / (float) frames, sum[1] / (float) frames,
		sum[2] / (float) frames, sum[3] / (float) frames, sum[4] / (float) frames, sum[5]);
	Con_Printf ("  triangles a frame: eyes %.0f, shadow faces %.0f\n", tris[0] / frames, tris[1] / frames);
	memset (aliascullstats, 0, sizeof (aliascullstats));
	aliascullframe0 = host_framecount;
	aliascullview0 = r_framecount;
}

static void R_DrawAliasModel_Real (entity_t *e, aliasmode_t mode)
{
	aliashdr_t	*paliashdr, *hdr;
	lerpdata_t	lerpdata;
	float		model_matrix[16], bounds_matrix[16], mapped_matrix[16];
	float		source_clip[4], destination_clip[4];
	int		portal_split;
	aliasinstance_t	*instance;
	int			totalverts;
	int			nearflags; // QVR
	aliasdepthentry_t	*cached; // QVR: set up already in this pass over the shadow maps' lights (R_AliasDepthSlot)
	qboolean	cachehit;
	aliaseyeentry_t	*eye; // QVR: its set-up in the first eye's scene (R_AliasEyeSlot)
	qboolean	eyefound, eyekeep, eyeuse, eyecheck;
	int			smoothbefore;
	qboolean	posed; // QVR: IK-posed (VR_AliasBonePoses)
	unsigned char	facemask; // QVR: layered: the faces it is drawn into

	//
	// setup pose/lerp data -- do it first so we don't miss updates due to culling
	//
	paliashdr = (aliashdr_t *)Mod_Extradata (e->model);

	eye = mode == ALIAS_STANDARD && !aliasfaces ? R_AliasEyeSlot (e, &eyefound) : NULL;
	eyekeep = eye && !eyefound && aliaseyemode == 1;
	eyeuse = eye && eyefound && aliaseyemode == 2;
	eyecheck = eye && eyefound && aliaseyemode == 3;
	if (eyeuse)
	{
		cached = NULL;
		cachehit = false;
		lerpdata = eye->lerpdata;
		memcpy (model_matrix, eye->model_matrix, sizeof (model_matrix));
		memcpy (bounds_matrix, eye->bounds_matrix, sizeof (bounds_matrix));
		aliaseyestats[eye->lit ? AEYE_HIT : AEYE_TRANSFORM_ONLY]++;
		goto transformed;
	}

	cached = mode == ALIAS_DEPTH ? R_AliasDepthSlot (e, &cachehit) : NULL;
	if (cached && cachehit)
	{
		lerpdata = cached->lerpdata;
		memcpy (model_matrix, cached->model_matrix, sizeof (model_matrix));
		memcpy (bounds_matrix, cached->bounds_matrix, sizeof (bounds_matrix));
		portal_split = 0;
		aliasdepthhits++;
		goto cull;
	}

	VR_TRACY_DETAIL (vr_tracy_render_zones, "alias transform", mode == ALIAS_STANDARD ? R_AliasTransformShared (e, paliashdr, &lerpdata, model_matrix, bounds_matrix) : R_AliasTransform (e, paliashdr, &lerpdata, model_matrix, bounds_matrix)); // QVR: (vr_tracy_render_detail; the first eye: the shadow pass's)
	if (eyekeep) // QVR: kept for the second eye
	{
		eye->ent = e;
		eye->model = e->model;
		eye->frame = e->frame;
		eye->skinnum = e->skinnum;
		eye->lit = 0;
		eye->lerpdata = lerpdata;
		memcpy (eye->model_matrix, model_matrix, sizeof (model_matrix));
		memcpy (eye->bounds_matrix, bounds_matrix, sizeof (bounds_matrix));
		eye->gen = aliaseyegen;
	}
	else if (eyecheck)
	{
		aliaseyestats[AEYE_CHECKED]++;
		R_AliasEyeCompare (&lerpdata, &eye->lerpdata, sizeof (lerpdata), AEYE_BAD_LERP);
		R_AliasEyeCompare (model_matrix, eye->model_matrix, sizeof (model_matrix), AEYE_BAD_MATRIX);
		R_AliasEyeCompare (bounds_matrix, eye->bounds_matrix, sizeof (bounds_matrix), AEYE_BAD_MATRIX);
	}
transformed:
	portal_split = mode != ALIAS_DEPTH && VR_PortalAlias (e, bounds_matrix, model_matrix, mapped_matrix, source_clip, destination_clip);
	if (mode == ALIAS_DEPTH)
		aliasdepthmisses++;
cull:
	posed = VR_AliasBonePoses (e, NULL) != 0; // QVR
	facemask = aliasfaces ? *aliasfaces : 0;
	if (cached && !cachehit)
		cached->boxbones = NULL; // (a new entry: no box yet)
	if (mode == ALIAS_STANDARD || mode == ALIAS_DEPTH) // QVR: the posed by their bones' box (R_AliasPosedBox)
	{
		int tested = aliasfaces ? R_AliasFaceCount (facemask) : 1, culled = 0, cullmode = 0;
		qboolean wrong = false;
		vec3_t pmins, pmaxs;
		if (portal_split)
			;
		else if (!posed)
			culled = !aliasfaces && R_CullModelForEntity (e); // (layered: the caller culled it by face)
		else if ((cullmode = VR_AliasCullPosedOn (e, mode == ALIAS_DEPTH)) && R_AliasPosedBox (e, paliashdr, model_matrix, pmins, pmaxs, cached, cachehit))
		{
			unsigned char before = facemask;
			if (aliasfaces)
			{
				facemask = R_AliasFacesOf (facemask, pmins, pmaxs);
				culled = tested - R_AliasFaceCount (facemask);
			}
			else
				culled = R_CullBox (pmins, pmaxs);
			if (cullmode == 2 && culled)
				wrong = R_AliasPosedWrong (e, paliashdr, model_matrix, (unsigned char) (before & ~facemask));
		}
		R_AliasCullCount (e, paliashdr, mode == ALIAS_DEPTH, tested, culled, posed, wrong);
		if (culled == tested)
			return;
	}
	else if (!portal_split && !posed && !aliasfaces && R_CullModelForEntity(e))
		return;

	//
	// set up for alpha blending
	//
	if (r_lightmap_cheatsafe) //no alpha in drawflat or lightmap mode
		entalpha = 1;
	else
		entalpha = ENTALPHA_DECODE(e->alpha);

	if (entalpha == 0)
		return;

	if (mode == ALIAS_SHOWSKEL && !VR_AliasBonePoses (e, NULL)) // QVR: see vr_body_debug instead
	{
		R_DrawSkeleton (paliashdr, model_matrix, &lerpdata);
		return;
	}

	//
	// set up lighting
	//
	rs_aliaspolys += paliashdr->numtris;
	vr_profcounts.aliastris += paliashdr->numtris; // QVR: profile (the Tracy plot: rs_aliaspolys is cleared only under r_speeds)
	if (mode == ALIAS_DEPTH) // QVR: a shadow map's caster: its depth only (none of the light it gets)
		lightcolor[0] = lightcolor[1] = lightcolor[2] = 0.f;
	else if (eyeuse && eye->lit) // QVR: the light at its origin as the first eye found it, the rest this eye's
	{
		VectorCopy (eye->light, lightcolor);
		VR_TRACY_DETAIL (vr_tracy_render_zones, "alias light finish", R_AliasLightFinish (e)); // QVR: (vr_tracy_render_detail)
	}
	else
	{
		VR_TRACY_DETAIL (vr_tracy_render_zones, "alias light point", R_AliasLightPoint (e));
		if (eyekeep)
			VectorCopy (lightcolor, eye->light);
		else if (eyecheck && eye->lit)
			R_AliasEyeCompare (lightcolor, eye->light, sizeof (vec3_t), AEYE_BAD_LIGHT);
		VR_TRACY_DETAIL (vr_tracy_render_zones, "alias light finish", R_AliasLightFinish (e));
	}

	//
	// draw it
	//

	if (r_fullbright_cheatsafe || mode == ALIAS_SHOWTRIS)
		lightcolor[0] = lightcolor[1] = lightcolor[2] = 0.5f;

	if (mode == ALIAS_SHOWTRIS)
		entalpha = 1.f;

	nearflags = mode == ALIAS_STANDARD ? VR_AliasNearEye (e, model_matrix, paliashdr) : 0; // QVR
	if (!R_Alias_CanAddToBatch (e) || (portal_split && ibuf.count + 2 > countof(ibuf.inst)) || (ibuf.count && aliasnear != nearflags)) // QVR: near the eyes: its own batch
		R_FlushAliasInstances (mode == ALIAS_SHOWTRIS);

	if (!ibuf.count)
	{
		ibuf.ent = e;
		aliasnear = nearflags; // QVR
	}

	aliasinstfaces[ibuf.count] = facemask; // QVR: layered shadow casters: its faces (a posed one's: those its box reaches)
	instance = &ibuf.inst[ibuf.count++];

	if (cached && cachehit && cached->instok) // QVR: the pass's first set-up of it (vr_shadow_setup_share), its swing rows added to this batch
	{
		int at, shift;
		*instance = cached->inst;
		at = VR_AnimSmoothReplay (0, cached->smoothfirst, cached->smoothrows, ibuf.count == 1);
		shift = at - cached->smoothat;
		if (instance->vr.smooth[2] > 0)
			instance->vr.smooth[2] += shift;
		else if (instance->vr.smooth[2] < 0)
			instance->vr.smooth[2] -= shift;
		if (instance->vr.smooth[3] >= 2)
			instance->vr.smooth[3] += shift;
		return; // (a caster: no gates, no pose check)
	}

	MatrixTranspose4x3 (model_matrix, instance->worldmatrix);

	instance->lightcolor[0] = lightcolor[0];
	instance->lightcolor[1] = lightcolor[1];
	instance->lightcolor[2] = lightcolor[2];
	instance->alpha = entalpha;
	instance->pose1 = lerpdata.pose1;
	instance->pose2 = lerpdata.pose2;
	instance->blend = lerpdata.blend;

	if (cached && cachehit)
		totalverts = cached->totalverts;
	else if (eyeuse && eye->lit)
		totalverts = eye->totalverts;
	else
		for (hdr = paliashdr, totalverts = 0; hdr; hdr = Mod_NextSurface (hdr))
			totalverts += hdr->numverts_vbo;

	if (paliashdr->poseverttype == PV_QUAKE1 || paliashdr->poseverttype == PV_MD3)
	{
		instance->pose1 *= totalverts;
		instance->pose2 *= totalverts;
	}
	else if (paliashdr->poseverttype == PV_IQM)
	{
		instance->pose1 *= paliashdr->numbones;
		instance->pose2 *= paliashdr->numbones;
	}

	if (eyeuse && eye->lit && eye->vrok) // QVR: the rest of the first eye's set-up, its swing rows added to this batch
	{
		int at, shift;
		instance->padding = eye->zeroblend;
		instance->vr = eye->vr;
		at = VR_AnimSmoothReplay (0, eye->smoothfirst, eye->smoothrows, ibuf.count == 1);
		shift = at - eye->smoothat;
		if (instance->vr.smooth[2] > 0)
			instance->vr.smooth[2] += shift;
		else if (instance->vr.smooth[2] < 0)
			instance->vr.smooth[2] -= shift;
		if (instance->vr.smooth[3] >= 2)
			instance->vr.smooth[3] += shift;
		goto instanced;
	}
	if (cached && cachehit) // QVR
		instance->padding = cached->zeroblend;
	else
		VR_TRACY_DETAIL (vr_tracy_render_zones, "alias zero blend", instance->padding = VR_AliasZeroBlend (e, paliashdr, totalverts)); // QVR: (vr_tracy_render_detail)
	if (cached && !cachehit) // QVR: kept for the pass's other lights
	{
		cached->ent = e;
		cached->model = e->model;
		cached->frame = e->frame;
		cached->lerpdata = lerpdata;
		cached->totalverts = totalverts;
		cached->zeroblend = instance->padding;
		memcpy (cached->model_matrix, model_matrix, sizeof (model_matrix));
		memcpy (cached->bounds_matrix, bounds_matrix, sizeof (bounds_matrix));
		cached->instok = 0; // (below, once its instance is made)
		if (!posed)
			cached->boxbones = NULL; // (a posed one's: kept at the cull)
		cached->gen = aliasdepthgen;
	}
	VR_TRACY_DETAIL (vr_tracy_render_zones, "alias vr instance", VR_AliasInstance (e, model_matrix, paliashdr, mode == ALIAS_DEPTH ? 2 : mode == ALIAS_STANDARD, &instance->vr)); // QVR
	smoothbefore = ibuf.count == 1 ? 0 : VR_AnimSmoothRows (0); // (a batch's first instance starts its rows)
	VR_TRACY_DETAIL (vr_tracy_render_zones, "alias smooth", R_AliasSmoothInstance (e, paliashdr, &lerpdata, totalverts, instance, ibuf.count - 1, 0)); // QVR: r_lerpmodels 3
	if (cached && !cachehit && aliasdepthkeep) // QVR: its swing rows kept for the pass's other lights (vr_shadow_setup_share)
	{
		cached->smoothrows = VR_AnimSmoothRows (0) - smoothbefore;
		cached->smoothfirst = VR_AnimSmoothKeep (0, smoothbefore, cached->smoothrows);
		cached->smoothat = smoothbefore;
		if (cached->smoothfirst >= 0)
		{
			VR_TemporalAliasMotion (e, totalverts, 0, instance->motion); // (a caster's: none; done here for the copy)
			cached->inst = *instance;
			cached->instok = 1;
		}
	}
	if (eyekeep || (eyecheck && eye->lit && eye->vrok)) // QVR: kept for the second eye, or compared with what was
	{
		int rows = VR_AnimSmoothRows (0) - smoothbefore;
		if (eyekeep)
		{
			eye->smoothfirst = VR_AnimSmoothKeep (0, smoothbefore, rows);
			eye->smoothrows = rows;
			eye->smoothat = smoothbefore;
			eye->totalverts = totalverts;
			eye->zeroblend = instance->padding;
			eye->vr = instance->vr;
			eye->lit = eye->smoothfirst >= 0;
			eye->vrok = !VR_IsViewEntity (e);
		}
		else
		{
			vraliasinstance_t fresh = instance->vr, kept = eye->vr;
			fresh.smooth[2] = fresh.smooth[2] > 0 ? fresh.smooth[2] - smoothbefore : fresh.smooth[2] < 0 ? fresh.smooth[2] + smoothbefore : 0;
			fresh.smooth[3] = fresh.smooth[3] >= 2 ? fresh.smooth[3] - smoothbefore : fresh.smooth[3];
			kept.smooth[2] = kept.smooth[2] > 0 ? kept.smooth[2] - eye->smoothat : kept.smooth[2] < 0 ? kept.smooth[2] + eye->smoothat : 0;
			kept.smooth[3] = kept.smooth[3] >= 2 ? kept.smooth[3] - eye->smoothat : kept.smooth[3];
			if (memcmp (&fresh, &kept, sizeof (fresh)))
			{
				const float *f = (const float *) &fresh, *k = (const float *) &kept;
				int n;
				for (n = 0; n < (int) (sizeof (fresh) / sizeof (float)) && !memcmp (f + n, k + n, sizeof (float)); n++)
					;
				if (aliaseyestats[AEYE_BAD_VR]++ < 8) // (the instance data's floats: vr_api_render.h vraliasinstance_t)
					Con_Printf ("vr_alias_eye_cache check: %s's instance data differs from float %d (%g, first eye %g)\n",
						e->model->name, n, f[n], k[n]);
			}
			if (totalverts != eye->totalverts || instance->padding != eye->zeroblend)
				aliaseyestats[AEYE_BAD_COUNTS]++;
			if (rows != eye->smoothrows || !VR_AnimSmoothRowsEqual (0, smoothbefore, eye->smoothfirst, rows))
				aliaseyestats[AEYE_BAD_SMOOTH]++;
		}
	}
instanced:
	VR_TRACY_DETAIL (vr_tracy_render_zones, "alias motion", VR_TemporalAliasMotion (e, totalverts, mode == ALIAS_STANDARD, instance->motion)); // QVR: its previous frame's positions
	if (mode == ALIAS_STANDARD)
		VR_DebugAliasPose (e, paliashdr, lerpdata.pose1, lerpdata.pose2, lerpdata.blend, model_matrix); // QVR: vr_debug_pose_check
	if (portal_split)
	{
		aliasinstance_t* copy = &ibuf.inst[ibuf.count++];
		*copy = *instance; // exact current pose, bone buffer, skin and wounds
		copy->motion[0] = copy->motion[1] = -1; // QVR: (the gate's other side: no slots of its own)
		memcpy (instance->vr.portalclip, source_clip, sizeof(source_clip));
		memcpy (copy->vr.portalclip, destination_clip, sizeof(destination_clip));
		MatrixTranspose4x3 (mapped_matrix, copy->worldmatrix);
	}
}

/*
=================
R_PaintAliasWounds -- QVR

Wounds painted on models (vr/vr_wounds.cpp): `e` as it is drawn this frame (its pose, its lerp, its place; a posed
skeleton's bones), drawn into the bound framebuffer (its wound mask's layer; the viewport: its region) laid out by its
skin's coordinates, with `numsplats` splats (five vec4 each: gl_shaders.h's wound_paint_fragment_shader). The caller
sets the blending (the most of what is there and what is painted). Its first surface only (the one its mask is for).
`side` 0 or 1: only that side of a mask kept per side (your body's: its left and middle, its right), -1 all of it.
Its triangles, then their edges as lines: a texel a triangle's edge crosses without covering its middle is painted too
(the skin is read there at the edges of its islands).
=================
*/
qboolean R_PaintAliasWounds (entity_t *e, int numsplats, const float *splats, int side)
{
	aliashdr_t	*hdr, *surf;
	lerpdata_t	lerpdata;
	float		model_matrix[16];
	int			totalverts, numbones;
	const float	*bones;
	GLuint		buf, bonebuf;
	GLbyte		*ofs, *boneofs;
	GLuint		buffers[2];
	GLintptr	offsets[2];
	GLsizeiptr	sizes[2];
	struct
	{
		float	matviewproj[16];
		vec3_t	eyepos;
		float	_pad;
		vec4_t	fog;
		float	dither;
		float	_padding[3];
		aliasinstance_t inst;
	} data;

	if (!e->model || e->model->type != mod_alias || numsplats <= 0)
		return false;
	hdr = (aliashdr_t *) Mod_Extradata (e->model);
	if (!hdr || hdr->poseverttype > PV_MD3)
		return false;

	R_SetupAliasFrame (e, hdr, &lerpdata);
	R_SetupEntityTransform (e, &lerpdata);
	if (lerpdata.pose1 == lerpdata.pose2)
		lerpdata.blend = 0.f;
	R_EntityMatrix (model_matrix, lerpdata.origin, lerpdata.angles, e->scale);
	VR_AliasPreTransform (e, model_matrix);
	ApplyTranslation (model_matrix, hdr->scale_origin[0], hdr->scale_origin[1], hdr->scale_origin[2]);
	ApplyScale (model_matrix, hdr->scale[0], hdr->scale[1], hdr->scale[2]);
	VR_AliasPostTransform (e, model_matrix);

	for (surf = hdr, totalverts = 0; surf; surf = Mod_NextSurface (surf))
		totalverts += surf->numverts_vbo;

	memset (&data, 0, sizeof (data));
	data.matviewproj[0] = data.matviewproj[5] = data.matviewproj[10] = data.matviewproj[15] = 1.f; // unused: the skin's layout
	MatrixTranspose4x3 (model_matrix, data.inst.worldmatrix);
	data.inst.alpha = 1.f;
	data.inst.pose1 = lerpdata.pose1;
	data.inst.pose2 = lerpdata.pose2;
	data.inst.blend = lerpdata.blend;
	if (hdr->poseverttype == PV_IQM)
	{
		data.inst.pose1 *= hdr->numbones;
		data.inst.pose2 *= hdr->numbones;
	}
	else
	{
		data.inst.pose1 *= totalverts;
		data.inst.pose2 *= totalverts;
	}
	data.inst.padding = VR_AliasZeroBlend (e, hdr, totalverts);
	data.inst.motion[0] = data.inst.motion[1] = -1; // QVR: (no temporal AA slots)
	VR_AliasWoundPaintSide (e, side, data.inst.vr.woundside);
	R_AliasSmoothInstance (e, hdr, &lerpdata, totalverts, &data.inst, 0, 1); // QVR: r_lerpmodels 3, as it is drawn

	GL_UseProgram (glprogs.woundpaint[hdr->poseverttype]);
	GL_SetState (GLS_BLEND_OPAQUE | GLS_NO_ZTEST | GLS_NO_ZWRITE | GLS_CULL_NONE | GLS_ATTRIBS (hdr->poseverttype == PV_IQM ? 5 : 1));
	GL_Upload (GL_SHADER_STORAGE_BUFFER, &data, sizeof (data), &buf, &ofs);

	buffers[0] = buf;
	offsets[0] = (GLintptr) ofs;
	sizes[0] = sizeof (data);
	numbones = hdr->poseverttype == PV_IQM ? VR_AliasBonePoses (e, &bones) : 0;
	if (numbones)
	{
		GL_Upload (GL_SHADER_STORAGE_BUFFER, bones, sizeof (bonepose_t) * numbones, &bonebuf, &boneofs);
		buffers[1] = bonebuf; offsets[1] = (GLintptr) boneofs; sizes[1] = sizeof (bonepose_t) * numbones;
	}
	else if (hdr->poseverttype == PV_IQM)
	{
		buffers[1] = e->model->meshvbo; offsets[1] = hdr->vboposeofs; sizes[1] = sizeof (bonepose_t) * hdr->numbones * hdr->numposes;
	}
	else if (hdr->poseverttype == PV_MD3)
	{
		buffers[1] = e->model->meshvbo; offsets[1] = hdr->vbovertofs; sizes[1] = sizeof (md3pose_t) * totalverts * hdr->numposes;
	}
	else
	{
		buffers[1] = e->model->meshvbo; offsets[1] = hdr->vbovertofs; sizes[1] = sizeof (meshxyz_t) * totalverts * hdr->numposes;
	}

	GL_BindBuffer (GL_ARRAY_BUFFER, e->model->meshvbo);
	GL_BindBuffer (GL_ELEMENT_ARRAY_BUFFER, e->model->meshindexesvbo);
	GL_BindBuffersRange (GL_SHADER_STORAGE_BUFFER, 1, 2, buffers, offsets, sizes);
	if (hdr->poseverttype == PV_QUAKE1) // QVR: the dequantised offsets (PoseFrac: vr/vr_mdldequant.cpp)
		GL_BindBufferRange (GL_SHADER_STORAGE_BUFFER, 11, e->model->meshvbo, hdr->vbofracofs, sizeof (unsigned) * totalverts * hdr->numposes);
	VR_AnimSmoothBind (e, 1); // QVR: r_lerpmodels 3's swings (its rig: the rest pose, the bones)
	VR_DentBind (); // QVR: the flesh dents (Dents: vr/vr_dents.cpp)
	if (hdr->poseverttype == PV_IQM)
	{
		GL_VertexAttribPointerFunc (0, 3, GL_FLOAT, GL_FALSE, sizeof (iqmvert_t), (void*)(hdr->vbovertofs + offsetof (iqmvert_t, xyz)));
		GL_VertexAttribPointerFunc (1, 4, GL_BYTE, GL_TRUE, sizeof (iqmvert_t), (void*)(hdr->vbovertofs + offsetof (iqmvert_t, norm)));
		GL_VertexAttribPointerFunc (2, 2, GL_FLOAT, GL_FALSE, sizeof (iqmvert_t), (void*)(hdr->vbovertofs + offsetof (iqmvert_t, st)));
		GL_VertexAttribPointerFunc (3, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof (iqmvert_t), (void*)(hdr->vbovertofs + offsetof (iqmvert_t, weight)));
		GL_VertexAttribIPointerFunc (4, 4, GL_UNSIGNED_BYTE, sizeof (iqmvert_t), (void*)(hdr->vbovertofs + offsetof (iqmvert_t, idx)));
	}
	else
	{
		GL_VertexAttribPointerFunc (0, 2, GL_FLOAT, GL_FALSE, sizeof (meshst_t), (void*)hdr->vbostofs);
	}

	GL_Uniform1iFunc (0, numsplats);
	GL_Uniform4fvFunc (1, numsplats * 5, splats);

	GL_DrawElementsInstancedFunc (GL_TRIANGLES, hdr->numindexes, GL_UNSIGNED_SHORT, (void*)hdr->eboofs, 1);
	glPolygonMode (GL_FRONT_AND_BACK, GL_LINE);
	GL_DrawElementsInstancedFunc (GL_TRIANGLES, hdr->numindexes, GL_UNSIGNED_SHORT, (void*)hdr->eboofs, 1);
	glPolygonMode (GL_FRONT_AND_BACK, GL_FILL);
	return true;
}

/*
=================
R_DrawAliasModels
=================
*/
void R_DrawAliasModels (entity_t **ents, int count)
{
	int i;
	for (i = 0; i < count; i++)
		VR_TRACY_DETAIL (vr_tracy_render_zones, "alias model", R_DrawAliasModel_Real (ents[i], ALIAS_STANDARD)); // QVR: (vr_tracy_render_detail)
	R_FlushAliasInstances (false);
	VR_TemporalMotionWrites (0); // QVR: on through the models' batches (switching the draw buffers a batch cost GPU time)
}

/*
=================
R_DrawAliasModelsDepth -- QVR

The shadow maps' casters (vr/vr_lighting.cpp): their depth only, so none of the per-instance lighting (the light
under them, its direction, the ambient cube, glows, rim and reflections) is worked out for each face they are drawn in.
=================
*/
void R_DrawAliasModelsDepth (entity_t **ents, int count)
{
	int i;
	aliasdepth = true;
	for (i = 0; i < count; i++)
		VR_TRACY_DETAIL (vr_tracy_render_zones, "alias shadow model", R_DrawAliasModel_Real (ents[i], ALIAS_DEPTH)); // QVR: (vr_tracy_render_detail)
	R_FlushAliasInstances (false);
	aliasdepth = false;
}

/*
=================
R_DrawAliasModelsDepthLayered -- QVR

The shadow maps' casters drawn once into all of a light's faces they reach (vr/vr_lighting.cpp, vr_shadow_layered):
faces[i] the faces of ents[i] (bit f: face f, drawn into viewport f through faceviewproj[f], 16 floats each; the
caller has culled them by face), their lerp, transforms and bones set up once instead of once per face. Needs
glprogs.alias_depth_layered (gl_viewport_layer_able) and opaque skins (not MF_HOLEY: those keep their fragment shader).
=================
*/
void R_DrawAliasModelsDepthLayered (entity_t **ents, const unsigned char *faces, int count, const float *faceviewproj)
{
	int i;
	memcpy (aliasfacebuf.viewproj, faceviewproj, sizeof (aliasfacebuf.viewproj));
	aliasdepth = true;
	for (i = 0; i < count; i++)
	{
		aliasfaces = &faces[i];
		VR_TRACY_DETAIL (vr_tracy_render_zones, "alias shadow model", R_DrawAliasModel_Real (ents[i], ALIAS_DEPTH)); // QVR: (vr_tracy_render_detail)
	}
	R_FlushAliasInstances (false);
	aliasfaces = NULL;
	aliasdepth = false;
}

/*
=================
R_DrawAliasModels_ShowTris
=================
*/
void R_DrawAliasModels_ShowTris (entity_t **ents, int count)
{
	int i;
	for (i = 0; i < count; i++)
		R_DrawAliasModel_Real (ents[i], ALIAS_SHOWTRIS);
	R_FlushAliasInstances (true);
}

/*
=================
R_DrawAliasModels_ShowSkel
=================
*/
void R_DrawAliasModels_ShowSkel (entity_t **ents, int count)
{
	int i;
	for (i = 0; i < count; i++)
		R_DrawAliasModel_Real (ents[i], ALIAS_SHOWSKEL);
}
