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
			e->lerpflags -= LERP_RESETANIM2;
		}
		else
		{
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
		e->movelerpstart = cl.time;
		e->movelerpfinish = (e->lerpflags & LERP_FINISH) ? e->lerpfinish : 0.f; // QVR: this move's (R_MoveLerpBlend)
		VectorCopy (e->currentorigin, e->previousorigin);
		VectorCopy (e->origin,  e->currentorigin);
		VectorCopy (e->currentangles, e->previousangles);
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
}

/*
=================
R_SetupAliasLighting -- johnfitz -- broken out from R_DrawAliasModel and rewritten
=================
*/
void R_SetupAliasLighting (entity_t	*e)
{
	vec3_t		dist;
	float		add;
	int			i;
	qboolean	parity = VR_ModelLightParity (); // QVR: on a par with the world: the shader shades 0.6 .. 1.4 by the normal

	// if the initial trace is completely black, try again from above
	// this helps with models whose origin is slightly below ground level
	// (e.g. some of the candles in the DOTM start map)
	vec3_t		lightorg;
	VR_AliasLightOrigin (e, lightorg); // QVR: the origin, or a climbing hand's as if not moved for looks
	if (!R_LightPoint (lightorg, 0.f, &e->lightcache))
		R_LightPoint (lightorg, e->model->maxs[2] * 0.5f, &e->lightcache);

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

/*
=================
R_FlushAliasInstances
=================
*/
void R_FlushAliasInstances (qboolean showtris)
{
	extern cvar_t r_softemu_mdl_warp;
	qmodel_t* model;
	aliashdr_t* mainhdr, *hdr;
	qboolean	alphatest, translucent, oit;
	qboolean	a2c; // QVR
	qboolean	layered; // QVR: into all their faces at once (aliasfaces)
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
	gltexture_t* textures[3];	// QVR: and the normal map
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
	GL_UseProgram (glprogs.alias[oit][mode][alphatest][poseverttype]);
	if (aliasdepth && !alphatest && !translucent) // QVR: a shadow map's opaque casters: no fragment shader (holey skins keep theirs)
		GL_UseProgram (layered ? glprogs.alias_depth_layered[poseverttype] : glprogs.alias_depth[poseverttype]);
	if (layered) // QVR: the faces' view-projections and the draws (holey skins are drawn a face at a time: vr_lighting.cpp)
	{
		GLuint		facebuf;
		GLbyte		*faceofs;
		GLsizeiptr	facesize = (GLsizeiptr)(sizeof (aliasfacebuf.viewproj) + sizeof (aliasfacebuf.draws[0]) * drawcount);
		GL_Upload (GL_SHADER_STORAGE_BUFFER, &aliasfacebuf, facesize, &facebuf, &faceofs);
		GL_BindBufferRange (GL_SHADER_STORAGE_BUFFER, 7, facebuf, (GLintptr)faceofs, facesize);
	}

	glEnable (GL_CLIP_DISTANCE1); // QVR: each instance keeps its half of the slipgate
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
	GL_BindNative (GL_TEXTURE14, GL_TEXTURE_CUBE_MAP, VR_EnvCubeTexture ()); // QVR: the reflections' cube map (EnvCube)
	GL_BindNative (GL_TEXTURE13, GL_TEXTURE_2D_ARRAY, VR_WoundTexture ()); // QVR: the wound masks (WoundMasks)
	GL_BindNative (GL_TEXTURE15, GL_TEXTURE_2D_ARRAY, VR_WoundFineTexture ()); // QVR: your own finer ones (WoundMasksFine)
	GL_BindNative (GL_TEXTURE10, GL_TEXTURE_2D_ARRAY, VR_WoundBloodTexture ()); // QVR: the blood on them not yours (WoundBloodFine)

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
	VR_RetroBind (QVR_RETRO_LUT_UNIT_ALIAS); // QVR: the palette's table for retro textures (vr/vr_retro.cpp)
	a2c = alphatest && !translucent && VR_AlphaToCoverage (); // QVR: holey skins' edges by alpha to coverage (vr_alpha_coverage, MSAA)
	if (a2c)
	{
		glEnable (GL_SAMPLE_ALPHA_TO_COVERAGE);
		glEnable (GL_SAMPLE_ALPHA_TO_ONE);
	}

	for (hdr = mainhdr; hdr; hdr = Mod_NextSurface (hdr))
	{
		skinnum = ibuf.ent->skinnum;
		if ((skinnum >= hdr->numskins) || (skinnum < 0)) skinnum = 0;
		textures[0] = hdr->gltextures[skinnum][anim];
		if (!textures[0]) continue;

		if (!translucent && (textures[0]->flags & TEXPREF_ALPHAPIXELS))
		{
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

		GL_BindTextures (0, 3, textures); // QVR: and the normal map
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

		for (hdr = mainhdr; hdr; hdr = Mod_NextSurface (hdr))
		{
			skinnum = ibuf.ent->skinnum;
			if ((skinnum >= hdr->numskins) || (skinnum < 0)) skinnum = 0;
			textures[0] = hdr->gltextures[skinnum][anim];
			if (!textures[0]) continue;

			if (!(textures[0]->flags & TEXPREF_ALPHAPIXELS))
			{ 
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

			GL_BindTextures (0, 3, textures); // QVR: and the normal map
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
R_DrawAliasModel_Real
=================
*/
static void R_DrawAliasModel_Real (entity_t *e, aliasmode_t mode)
{
	aliashdr_t	*paliashdr, *hdr;
	lerpdata_t	lerpdata;
	float		fovscale = 1.0f;
	float		model_matrix[16], bounds_matrix[16], mapped_matrix[16];
	float		source_clip[4], destination_clip[4];
	int		portal_split;
	aliasinstance_t	*instance;
	int			totalverts;
	int			nearflags; // QVR

	//
	// setup pose/lerp data -- do it first so we don't miss updates due to culling
	//
	paliashdr = (aliashdr_t *)Mod_Extradata (e->model);

	R_SetupAliasFrame (e, paliashdr, &lerpdata);
	R_SetupEntityTransform (e, &lerpdata);

	if (lerpdata.pose1 == lerpdata.pose2)
		lerpdata.blend = 0.f;

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

		VectorMA (lerpdata.origin, cl_gun_x.value * paliashdr->scale[0] * fovscale,	vright,	lerpdata.origin);
		VectorMA (lerpdata.origin, cl_gun_y.value * paliashdr->scale[1] * fovscale,	vup,	lerpdata.origin);
		VectorMA (lerpdata.origin, cl_gun_z.value * paliashdr->scale[2],			vpn,	lerpdata.origin);
	}

	//
	// cull it
	//


	//
	// transform it
	//
	R_EntityMatrix (model_matrix, lerpdata.origin, lerpdata.angles, e->scale);
	VR_AliasPreTransform (e, model_matrix); // QVR
	memcpy (bounds_matrix, model_matrix, sizeof (bounds_matrix));
	ApplyTranslation (model_matrix, paliashdr->scale_origin[0], paliashdr->scale_origin[1] * fovscale, paliashdr->scale_origin[2] * fovscale);
	ApplyScale (model_matrix, paliashdr->scale[0], paliashdr->scale[1] * fovscale, paliashdr->scale[2] * fovscale);
	VR_AliasPostTransform (e, model_matrix); // QVR
	portal_split = mode != ALIAS_DEPTH && VR_PortalAlias (e, bounds_matrix, model_matrix, mapped_matrix, source_clip, destination_clip);
	if (!portal_split && !VR_AliasBonePoses (e, NULL) && !aliasfaces && R_CullModelForEntity(e)) // QVR: layered: the caller culled it by face
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
	if (mode == ALIAS_DEPTH) // QVR: a shadow map's caster: its depth only (none of the light it gets)
		lightcolor[0] = lightcolor[1] = lightcolor[2] = 0.f;
	else
		R_SetupAliasLighting (e);

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

	aliasinstfaces[ibuf.count] = aliasfaces ? *aliasfaces : 0; // QVR: layered shadow casters: its faces
	instance = &ibuf.inst[ibuf.count++];

	MatrixTranspose4x3 (model_matrix, instance->worldmatrix);

	instance->lightcolor[0] = lightcolor[0];
	instance->lightcolor[1] = lightcolor[1];
	instance->lightcolor[2] = lightcolor[2];
	instance->alpha = entalpha;
	instance->pose1 = lerpdata.pose1;
	instance->pose2 = lerpdata.pose2;
	instance->blend = lerpdata.blend;

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

	instance->padding = VR_AliasZeroBlend (e, paliashdr, totalverts); // QVR
	VR_AliasInstance (e, model_matrix, paliashdr, mode == ALIAS_DEPTH ? 2 : mode == ALIAS_STANDARD, &instance->vr); // QVR
	if (portal_split)
	{
		aliasinstance_t* copy = &ibuf.inst[ibuf.count++];
		*copy = *instance; // exact current pose, bone buffer, skin and wounds
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
	VR_AliasWoundPaintSide (e, side, data.inst.vr.woundside);

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
		R_DrawAliasModel_Real (ents[i], ALIAS_STANDARD);
	R_FlushAliasInstances (false);
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
		R_DrawAliasModel_Real (ents[i], ALIAS_DEPTH);
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
		R_DrawAliasModel_Real (ents[i], ALIAS_DEPTH);
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
