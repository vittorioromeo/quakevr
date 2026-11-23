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
// r_world.c: world model rendering

#include "quakedef.h"
#include "vr/vr_tracy.h" // QVR: profiling scopes (compiled out of normal builds)
#include "vr/vr_api.h"
#include "vr/vr_api_render.h"

extern cvar_t gl_fullbrights, r_oldskyleaf, r_showtris; //johnfitz
extern cvar_t gl_zfix; // QuakeSpasm z-fighting fix
extern cvar_t r_oit;

extern gltexture_t *lightmap_texture;
extern gltexture_t *lux_texture; // QVR: r_brush.c

extern GLuint gl_bmodel_vbo;
extern size_t gl_bmodel_vbo_size;
extern GLuint gl_bmodel_ibo;
extern size_t gl_bmodel_ibo_size;
extern GLuint gl_bmodel_indirect_buffer;
extern size_t gl_bmodel_indirect_buffer_size;
extern GLuint gl_bmodel_surf_buffer;
extern GLuint gl_bmodel_marksurf_buffer;
extern GLuint gl_bmodel_marksurf_buffer_size;
extern GLuint gl_bmodel_vertcmd_buffer; // QVR: the bound path's world textures in arrays (r_brush.c)
extern int *gl_bmodel_cmdcap, gl_bmodel_numcmds; // QVR

typedef struct gpumark_frame_s {
	vec4_t		frustum[4];
	vec3_t		vieworg;
	GLuint		oldskyleaf;
	GLuint		framecount;
	GLuint		padding[3];
	vec3_t		vieworg2;	// QVR: single-pass stereo: the other eye's (a surface either eye faces is kept); else vieworg
	GLuint		padding2;
	GLuint		litinfo[4];	// QVR: the ES profile's split by surface (C4v; QVR_LITSPLIT): the spheres, the first lit command, the textures split
	vec4_t		litspheres[QVR_WORLD_LIT_SPHERES];
} gpumark_frame_t;

extern int gl_bmodel_litcmd_first, gl_bmodel_litcmd_count; // QVR: the world's lit commands (r_brush.c; C4v)
// QVR: C4v, the ES profile's split by surface (vr_gl_world_unlit 2): the cull waits for R_PushDlights (R_MarkSurfacesLate)
// to know the lights reaching the world; this view's split (-1 none: one program for the world; else the world's lit
// surfaces are in their own commands, drawn with the lights' loop, the rest without), the brush entities lit or not
static gpumark_frame_t r_markframe;
static qboolean r_markpending;
static int r_world_litsplit = -1;
static qboolean r_world_litbmodels;
// QVR: C4w, which of the world's lit commands can get a surface this view (R_WorldLitTextures): only those are drawn
// (on ES each command is a draw of its own: the empty ones cost 18 draws on e1m2's start); from each leaf's box round
// its surfaces' boxes (R_WorldLitLeavesBuild, at the map's load)
static byte *r_world_littex;		// a byte a lit command: 1 drawn
static int r_world_littexcap;
static float (*r_litleafbox)[6];	// each leaf's (leafs[i + 1]) box round its surfaces' (mins, maxs; empty: mins > maxs)
static int r_litleafcount, r_litleafcap;
static int *r_litsurftex;			// each world surface's lit command (its texture's index; -1: not split)
static int r_litsurfcap;
static qboolean r_world_litskip;	// this view's: the empty lit commands skipped (vr_gl_world_lit_skip)

/*
===============
R_WorldLitLeavesBuild -- QVR

C4w: each world leaf's box round its surfaces' boxes (the boxes the GPU's cull tests the lights' spheres against), for
R_WorldLitTextures; at the map's load, when the ES profile's lit commands are made (r_brush.c).
===============
*/
void R_WorldLitLeavesBuild (void)
{
	int i, j, k;
	int *texidx;
	r_litleafcount = 0;
	if (gl_bmodel_litcmd_first < 0 || !cl.worldmodel)
		return;
	if (cl.worldmodel->numleafs > r_litleafcap)
	{
		free (r_litleafbox);
		r_litleafcap = cl.worldmodel->numleafs;
		r_litleafbox = (float (*)[6]) malloc (sizeof (r_litleafbox[0]) * r_litleafcap);
		if (!r_litleafbox)
			Sys_Error ("R_WorldLitLeavesBuild: out of memory (%d leaves)", r_litleafcap);
	}
	if (cl.worldmodel->numsurfaces > r_litsurfcap)
	{
		free (r_litsurftex);
		r_litsurfcap = cl.worldmodel->numsurfaces;
		r_litsurftex = (int *) malloc (sizeof (r_litsurftex[0]) * r_litsurfcap);
		if (!r_litsurftex)
			Sys_Error ("R_WorldLitLeavesBuild: out of memory (%d surfaces)", r_litsurfcap);
	}
	texidx = (int *) malloc (sizeof (texidx[0]) * q_max (cl.worldmodel->numtextures, 1)); // (as GL_BuildBModelMarkBuffers' texidx)
	if (!texidx)
		Sys_Error ("R_WorldLitLeavesBuild: out of memory (%d textures)", cl.worldmodel->numtextures);
	for (i = 0; i < cl.worldmodel->numtextures; i++)
		texidx[i] = -1;
	for (i = 0; i < cl.worldmodel->texofs[TEXTYPE_COUNT]; i++)
		texidx[cl.worldmodel->usedtextures[i]] = i;
	for (i = 0; i < cl.worldmodel->numsurfaces; i++)
	{
		const int t = cl.worldmodel->surfaces[i].texinfo->texnum;
		const int idx = t >= 0 && t < cl.worldmodel->numtextures ? texidx[t] : -1;
		r_litsurftex[i] = idx >= 0 && idx < gl_bmodel_litcmd_count ? idx : -1;
	}
	free (texidx);
	for (i = 0; i < cl.worldmodel->numleafs; i++)
	{
		const mleaf_t *leaf = &cl.worldmodel->leafs[i + 1];
		float *box = r_litleafbox[i];
		box[0] = box[1] = box[2] = 1e30f;
		box[3] = box[4] = box[5] = -1e30f;
		for (j = 0; j < leaf->nummarksurfaces; j++)
		{
			const msurface_t *s = &cl.worldmodel->surfaces[leaf->firstmarksurface[j]];
			for (k = 0; k < 3; k++)
			{
				box[k] = q_min (box[k], s->mins[k]);
				box[3 + k] = q_max (box[3 + k], s->maxs[k]);
			}
		}
	}
	r_litleafcount = cl.worldmodel->numleafs;
	if (r_world_littexcap < gl_bmodel_litcmd_count)
	{
		free (r_world_littex);
		r_world_littexcap = gl_bmodel_litcmd_count;
		r_world_littex = (byte *) malloc (r_world_littexcap);
		if (!r_world_littex)
			Sys_Error ("R_WorldLitLeavesBuild: out of memory (%d textures)", r_world_littexcap);
	}
}

/*
===============
R_WorldLitTextures -- QVR

C4w: which of the world's lit commands the GPU's cull can send a surface to this view: a texture with a surface whose
box a sphere touches (the cull's own test, the radius a unit wider) in a leaf of the view's PVS. A superset: the cull
also drops back faces and what is out of the frustum. Their texture index as the cull's (R_AddBModelPassCalls' j).
===============
*/
static qboolean R_WorldLitTextures (const byte *vis, const vec4_t *spheres, int count)
{
	int i, j, l, k;
	if (!r_world_littex || !r_litsurftex || r_litleafcount != cl.worldmodel->numleafs || !vis)
		return false;
	memset (r_world_littex, 0, gl_bmodel_litcmd_count);
	for (i = 0; i < r_litleafcount; i++)
	{
		const float *box = r_litleafbox[i];
		const mleaf_t *leaf;
		if (!(vis[i >> 3] & (1 << (i & 7))) || box[0] > box[3])
			continue;
		for (l = 0; l < count; l++)
		{
			const float *s = spheres[l];
			float d2 = 0.f, r = s[3] + 1.f;
			for (k = 0; k < 3; k++)
			{
				float d = q_max (q_max (box[k] - s[k], s[k] - box[3 + k]), 0.f);
				d2 += d * d;
			}
			if (d2 <= r * r)
				break;
		}
		if (l == count)
			continue;
		leaf = &cl.worldmodel->leafs[i + 1];
		for (j = 0; j < leaf->nummarksurfaces; j++)
		{
			const msurface_t *surf = &cl.worldmodel->surfaces[leaf->firstmarksurface[j]];
			const int tex = r_litsurftex[leaf->firstmarksurface[j]];
			if (tex < 0 || r_world_littex[tex])
				continue;
			for (l = 0; l < count; l++)
			{
				const float *s = spheres[l];
				float d2 = 0.f, r = s[3] + 1.f;
				for (k = 0; k < 3; k++)
				{
					float d = q_max (q_max (surf->mins[k] - s[k], s[k] - surf->maxs[k]), 0.f);
					d2 += d * d;
				}
				if (d2 <= r * r)
				{
					r_world_littex[tex] = 1;
					break;
				}
			}
		}
	}
	return true;
}

byte *SV_FatPVS (vec3_t org, qmodel_t *worldmodel);

static byte *r_stereovis; // QVR: single-pass stereo: both eyes' PVS (R_MarkSurfaces)
static size_t r_stereovissize;
static byte *r_lightsvis; // QVR: the view's PVS kept for R_PushDlights' reach test (VR_WorldLightsCheck, C4u)
static size_t r_lightsvissize;

/*
================
R_ViewVis -- QVR

The PVS R_MarkSurfaces chose for the view being drawn (both eyes' under single-pass stereo), a bit per leaf; NULL
before the first view.
================
*/
const byte *R_ViewVis (void)
{
	return r_lightsvis;
}

/*
===============
R_MarkVisSurfaces
===============
*/
static void R_DispatchMarkSurfaces (const byte* vis, const gpumark_frame_t *frame);

static void R_MarkVisSurfaces (byte* vis)
{
	int			i;
	gpumark_frame_t frame;

	memset (&frame, 0, sizeof (frame)); // QVR: (the split's part)
	for (i = 0; i < 4; i++)
	{
		frame.frustum[i][0] = frustum[i].normal[0];
		frame.frustum[i][1] = frustum[i].normal[1];
		frame.frustum[i][2] = frustum[i].normal[2];
		frame.frustum[i][3] = frustum[i].dist;
	}
	frame.vieworg[0] = r_refdef.vieworg[0];
	frame.vieworg[1] = r_refdef.vieworg[1];
	frame.vieworg[2] = r_refdef.vieworg[2];
	VectorCopy (frame.vieworg, frame.vieworg2); // QVR
	if (VR_StereoSinglePhase () == 1)
		VR_StereoSingleEyeOrigin (1, frame.vieworg2);
	frame.padding2 = 0;
	frame.oldskyleaf = r_oldskyleaf.value != 0.f;
	frame.framecount = r_framecount;

	r_world_litsplit = -1; // QVR
	r_world_litskip = false;
	r_world_litbmodels = false;
	if (gl_bmodel_litcmd_first >= 0) // QVR: the ES profile's split by surface: after R_PushDlights (R_MarkSurfacesLate)
	{
		r_markframe = frame;
		r_markpending = true;
		return;
	}
	R_DispatchMarkSurfaces (vis, &frame);
}

/*
===============
R_MarkSurfacesLate -- QVR

C4v: the world's cull under the ES profile, after R_PushDlights (vr_lighting.cpp VR_WorldLightsCheck: the lights
reaching the world): the surfaces a light's sphere touches to their texture's lit command (vr_gl_world_unlit 2).
===============
*/
void R_MarkSurfacesLate (void)
{
	int			bmodels = 0;

	if (!r_markpending)
		return;
	r_markpending = false;
	r_world_litsplit = VR_WorldLitSpheres (r_markframe.litspheres, &bmodels);
	r_world_litbmodels = bmodels != 0;
	r_markframe.litinfo[0] = (GLuint) q_max (r_world_litsplit, 0);
	r_world_litskip = r_world_litsplit >= 0 && VR_WorldLitSkip () && R_WorldLitTextures (r_lightsvis, r_markframe.litspheres, r_world_litsplit); // QVR: C4w
	r_markframe.litinfo[1] = (GLuint) gl_bmodel_litcmd_first;
	r_markframe.litinfo[2] = (GLuint) gl_bmodel_litcmd_count;
	VR_ProfileBegin ("mark surfaces"); // QVR: profile
	R_DispatchMarkSurfaces (r_lightsvis, &r_markframe);
	VR_ProfileEnd ();
}

static void R_DispatchMarkSurfaces (const byte* vis, const gpumark_frame_t *frame)
{
	GLuint		buf;
	GLbyte*		ofs;
	size_t		vissize = (cl.worldmodel->numleafs + 7) >> 3;
	size_t		nummark = gl_bmodel_marksurf_buffer_size / sizeof (bmodel_gpu_marksurf_t);
#ifdef QVR_TRACY
	size_t		i;
#endif

	GL_BeginGroup ("Mark surfaces");

	COMPILE_TIME_ASSERT (vis_alignment_must_be_power_of_2, (VIS_ALIGN & (VIS_ALIGN - 1)) == 0);
	COMPILE_TIME_ASSERT (vis_alignment_must_be_multiple_of_uint, (VIS_ALIGN & 3) == 0);
	vissize = (vissize + VIS_ALIGN_MASK) & ~VIS_ALIGN_MASK; // round up

	GL_BeginGroup ("cs: clear indirect draws"); // QVR: (vr_gl_markers)
	GL_UseProgram (glprogs.clear_indirect);
	if (GL_WorldUnlitForms ()) // QVR: the ES profile's clear (QVR_LITSPLIT; the PC's with vr_gl_pc_forms 4): the whole buffer bound, the world's lit commands cleared too (C4v)
	{
		int litcount = r_world_litsplit >= 0 ? gl_bmodel_litcmd_count : 0;
		GL_BindBufferRange (GL_SHADER_STORAGE_BUFFER, 1, gl_bmodel_indirect_buffer, 0, gl_bmodel_indirect_buffer_size);
		GL_Uniform1iFunc (0, cl.worldmodel->texofs[TEXTYPE_COUNT]);
		GL_Uniform1iFunc (1, q_max (gl_bmodel_litcmd_first, 0));
		GL_Uniform1iFunc (2, litcount);
		GL_DispatchComputeFunc ((q_max (cl.worldmodel->texofs[TEXTYPE_COUNT], litcount) + 63) / 64, 1, 1);
	}
	else
	{
		GL_BindBufferRange (GL_SHADER_STORAGE_BUFFER, 1, gl_bmodel_indirect_buffer, 0, cl.worldmodel->texofs[TEXTYPE_COUNT] * sizeof(bmodel_draw_indirect_t));
		GL_DispatchComputeFunc ((cl.worldmodel->texofs[TEXTYPE_COUNT] + 63) / 64, 1, 1);
	}
	GL_MemoryBarrierFunc (GL_SHADER_STORAGE_BARRIER_BIT);
	GL_EndGroup (); // QVR

	GL_UseProgram (glprogs.cull_mark);
	GL_BindBufferRange (GL_SHADER_STORAGE_BUFFER, 2, gl_bmodel_ibo, 0, gl_bmodel_ibo_size);
	GL_Upload (GL_SHADER_STORAGE_BUFFER, vis, vissize, &buf, &ofs);
#ifdef QVR_TRACY
	for (i = 0; i < (cl.worldmodel->numleafs + 7) >> 3; i++) // QVR: profile (the Tracy plot: the leaves this view's PVS holds)
		vr_profcounts.worldleaves += __builtin_popcount (vis[i]);
#endif
	GL_BindBufferRange (GL_SHADER_STORAGE_BUFFER, 3, buf, (GLintptr)ofs, vissize);
	GL_BindBufferRange (GL_SHADER_STORAGE_BUFFER, 4, gl_bmodel_marksurf_buffer, 0, gl_bmodel_marksurf_buffer_size);
	GL_BindBufferRange (GL_SHADER_STORAGE_BUFFER, 5, gl_bmodel_surf_buffer, 0, cl.worldmodel->numsurfaces * sizeof(bmodel_gpu_surf_t));
	GL_Upload (GL_UNIFORM_BUFFER, frame, sizeof(*frame), &buf, &ofs);
	GL_BindBufferRange (GL_UNIFORM_BUFFER, 1, buf, (GLintptr)ofs, sizeof(*frame));

	GL_BeginGroup ("cs: cull and mark world surfaces"); // QVR: (vr_gl_markers)
	GL_DispatchComputeFunc ((nummark + 63) / 64, 1, 1);
	GL_MemoryBarrierFunc (GL_COMMAND_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT | GL_ELEMENT_ARRAY_BARRIER_BIT);
	GL_EndGroup (); // QVR

	VR_GLProbeBuffer ("world indirect commands", gl_bmodel_indirect_buffer, 0, // QVR: vr_gl_probe (Debug > Startup)
		q_min ((size_t)cl.worldmodel->texofs[TEXTYPE_COUNT], (size_t)64) * sizeof(bmodel_draw_indirect_t));
	VR_GLProbeBuffer ("world indices", gl_bmodel_ibo, 0, q_min (gl_bmodel_ibo_size, (size_t)262144));
	VR_GLProbeBuffer ("world vertices", gl_bmodel_vbo, 0, q_min (gl_bmodel_vbo_size, (size_t)4096));

	GL_EndGroup ();
}

/*
===============
R_MarkSurfaces
===============
*/
static byte *R_ChooseVis (mleaf_t *viewleaf, const vec3_t origin) // QVR: (split out: single-pass stereo's other eye)
{
	int			i;
	qboolean	nearwaterportal;

	// check this leaf for water portals
	// TODO: loop through all water surfs and use distance to leaf cullbox
	nearwaterportal = false;
	for (i=0; i < viewleaf->nummarksurfaces; i++)
		if (cl.worldmodel->surfaces[viewleaf->firstmarksurface[i]].flags & SURF_DRAWTURB)
			nearwaterportal = true;

	// choose vis data
	if (r_novis.value || viewleaf->contents == CONTENTS_SOLID || viewleaf->contents == CONTENTS_SKY)
		return Mod_NoVisPVS (cl.worldmodel);
	else if (nearwaterportal)
	{
		vec3_t pvsorigin;
		VectorCopy (origin, pvsorigin);
		VR_PortalPVSOrigin (pvsorigin); // QVR: through a teleporter, round the destination, not the camera behind it
		return SV_FatPVS (pvsorigin, cl.worldmodel);
	}
	else
		return Mod_LeafPVS (viewleaf, cl.worldmodel);
}

void R_MarkSurfaces (void)
{
	byte		*vis;

	vis = R_ChooseVis (r_viewleaf, r_origin);
	if (VR_StereoSinglePhase () == 1) // QVR: single-pass stereo: what either eye's leaf sees
	{
		vec3_t		other;
		mleaf_t		*otherleaf;
		VR_StereoSingleEyeOrigin (1, other);
		otherleaf = Mod_PointInLeaf (other, cl.worldmodel);
		if (otherleaf != r_viewleaf)
		{
			size_t i, size = (((cl.worldmodel->numleafs + 7) >> 3) + VIS_ALIGN_MASK) & ~VIS_ALIGN_MASK; // (as the vis buffers)
			byte *othervis;
			if (r_stereovissize < size)
			{
				free (r_stereovis);
				r_stereovis = (byte *) malloc (size);
				r_stereovissize = size;
			}
			memcpy (r_stereovis, vis, size);
			othervis = R_ChooseVis (otherleaf, other);
			for (i = 0; i < size; i++)
				r_stereovis[i] |= othervis[i];
			vis = r_stereovis;
		}
	}

	r_visframecount++;

	VR_WaterMarkVis (vis); // QVR: the geometric waves' faces seen (vr/vr_water.cpp)
	R_MarkVisSurfaces (vis);
	R_AddStaticModels (vis);
	// QVR: kept (the vis buffers are reused): a light lights no pixel of this view unless it reaches a leaf in it. Its
	// readers are the ES profile's (VR_WorldLightsCheck: the unlit world; R_MarkSurfacesLate: the split by surface):
	// not copied every view on the PC.
	if (glprogs.world_unlit[0][0] || gl_bmodel_litcmd_first >= 0)
	{
		size_t size = (((cl.worldmodel->numleafs + 7) >> 3) + VIS_ALIGN_MASK) & ~VIS_ALIGN_MASK; // (as the vis buffers)
		if (r_lightsvissize < size)
		{
			free (r_lightsvis);
			r_lightsvis = (byte *) malloc (size);
			if (!r_lightsvis)
				Sys_Error ("R_MarkSurfaces: out of memory (the view's PVS, %d bytes)", (int) size);
			r_lightsvissize = size;
		}
		memcpy (r_lightsvis, vis, size);
	}
}

/*
================
GL_WaterAlphaForEntityTextureType
 
Returns the water alpha to use for the entity and texture type combination.
================
*/
float GL_WaterAlphaForEntityTextureType (entity_t *ent, textype_t type)
{
	float entalpha;
	if (ent == NULL || ent->alpha == ENTALPHA_DEFAULT)
		entalpha = GL_WaterAlphaForTextureType(type);
	else
		entalpha = ENTALPHA_DECODE(ent->alpha);
	if (type == TEXTYPE_TELE)
		entalpha *= VR_TeleportOpacity ();
	return entalpha;
}

typedef struct bmodel_gpu_instance_s {
	float		world[12];	// world matrix (transposed mat4x3)
	float		alpha;
	float		padding[3];
	float		wound[4];	// QVR: a held prop's blood (vr/vr_wounds.cpp: its box mask)
	float		woundbox[4];	// QVR: its box's centre, 1 / its largest side
	float		retro[4];	// QVR: retro textures (vr/vr_retro.h): its set (VR_RetroInstance)
	float		portalclip[4];
	float		prevworld[12];	// QVR: its matrix one frame ago (temporal AA's motion vectors: vr/vr_temporal.cpp)
} bmodel_gpu_instance_t;

typedef struct bmodel_bindless_gpu_call_s {
	GLuint		flags;
	GLfloat		alpha;
	GLuint64	texture;
	GLuint64	fullbright;
	GLuint64	normalmap;	// QVR: vr_normalmaps
	GLuint64	specmap;	// QVR: an external pack's specular map (vr/vr_extmaps.cpp; CF_SPECMAP)
	GLuint64	padding;	// QVR: (uvclamp on 16 bytes, as std430 places it)
	GLfloat		uvclamp[4];	// QVR: texture_t's (parallax mapping)
	GLfloat		detail[4];	// QVR: its detail texture (vr/vr_detail.cpp)
	GLfloat		extmat[4];	// QVR: x/w generated normal/height multipliers; y/z external sheen/hardness
	GLfloat		retro[4];	// QVR: retro textures (VR_RetroCall): the texture's Quake size, its set
} bmodel_bindless_gpu_call_t;

typedef struct bmodel_bound_gpu_call_s {
	GLuint		flags;
	GLfloat		alpha;
	GLint		baseinstance;
	GLint		layer;		// QVR: its textures' layer in their arrays (vr/vr_worldarrays.cpp)
	GLfloat		uvclamp[4];	// QVR: texture_t's (parallax mapping)
	GLfloat		detail[4];	// QVR: its detail texture (vr/vr_detail.cpp)
	GLfloat		extmat[4];	// QVR: x/w generated normal/height multipliers; y/z external sheen/hardness
	GLfloat		retro[4];	// QVR: retro textures (VR_RetroCall)
} bmodel_bound_gpu_call_t;

typedef struct bmodel_gpu_call_remap_s {
	GLuint		src;
	GLuint		inst;
} bmodel_gpu_call_remap_t;

static unsigned char bmodel_portal_counts[MAX_VISEDICTS + 1];
static bmodel_gpu_instance_t		bmodel_instances[2 * MAX_VISEDICTS + 1]; // +1 for worldspawn
static union {
	struct {
		bmodel_bindless_gpu_call_t	params[MAX_BMODEL_DRAWS];
	} bindless;
	struct {
		bmodel_bound_gpu_call_t		params[MAX_BMODEL_DRAWS];
		gltexture_t					*textures[MAX_BMODEL_DRAWS][4];	// QVR: and the normal and specular maps
		int							set[MAX_BMODEL_DRAWS];	// QVR: its textures' set in arrays (vr/vr_worldarrays.cpp; -1 none)
		int							cmd[MAX_BMODEL_DRAWS];	// QVR: its command (gl_bmodel_indirect_buffer's)
		int							numinst[MAX_BMODEL_DRAWS];	// QVR: its instances
	} bound;
} bmodel_calls;
static bmodel_gpu_call_remap_t		bmodel_call_remap[MAX_BMODEL_DRAWS];
static int							num_bmodel_calls;
static GLuint						bmodel_batch_program;
static int							bmodel_relief_filter; // QVR: R_AddBModelCall keeps 0 every call, 1 only those with parallax relief, 2 only those without (R_DrawSolidSplit)
// QVR: C4v, the split by surface (r_world_litsplit): R_AddBModelPassCalls adds 0 the world's commands and its lit ones,
// 1 the unlit part (the world's commands, the brush entities unless lit), 2 the lit part (the world's lit commands, the
// brush entities if lit); a flush's calls from bmodel_seg_first on (-1: none) drawn with bmodel_seg_program, one gather
static int							bmodel_lit_mode;
static int							bmodel_seg_first = -1;
static GLuint						bmodel_seg_program;
// QVR: the bound path's world textures in arrays (vr/vr_worldarrays.cpp): this batch may use them; each command's call
// in the flush's draws of many (CmdCall in the vertex shader) and the flush it was last taken in (one call a command)
static qboolean						bmodel_arrays_on;
static GLuint						*bmodel_cmd_call;
static unsigned					*bmodel_cmd_stamp;
static int							bmodel_cmd_count;
static unsigned					bmodel_flush_stamp;
typedef struct { GLuint cmd, groupfirst, base, group; } bmodel_merge_item_t;
static bmodel_merge_item_t			bmodel_merge_items[MAX_BMODEL_DRAWS];
static int							bmodel_group_key[MAX_BMODEL_DRAWS], bmodel_group_of[MAX_BMODEL_DRAWS];
static int							bmodel_group_first[MAX_BMODEL_DRAWS], bmodel_group_count[MAX_BMODEL_DRAWS];
static GLuint						bmodel_group_base[MAX_BMODEL_DRAWS];
static qboolean						bmodel_call_merged[MAX_BMODEL_DRAWS];

/*
=============
R_InitBModelInstance
=============
*/
static void R_BModelMatrix (entity_t *ent, float mat[16]) // QVR: (R_PaintBrushWounds too)
{
	vec3_t angles;

	angles[0] = -ent->angles[0];
	angles[1] =  ent->angles[1];
	angles[2] =  ent->angles[2];
	R_EntityMatrix (mat, ent->origin, angles, ent == &cl_entities[0] ? ENTSCALE_DEFAULT : ent->scale);
	if (ent != &cl_entities[0]) // QVR
		VR_BrushTransform (ent, mat); // QVR
}

void VR_BModelMatrix (entity_t *ent, float mat[16]) // QVR: (vr/vr_permdecals.cpp: a mark on a door into its own space)
{
	R_BModelMatrix (ent, mat);
}

// QVR: a brush prop (not a map's door or lift) drawn split by a teleporter (vr/vr_portals.cpp): R_SortEntities keeps it
// though its own place is out of view, for its half drawn through the gate may be in view (a box sticking out of the
// gate seen from the destination's side, its middle still at the entrance: its frustum cull dropped both halves).
qboolean R_BModelPortalSplit (entity_t *ent)
{
	float mat[16], mapped[16], source[4], dest[4];
	if (ent == &cl_entities[0] || !ent->model || ent->model->name[0] == '*')
		return false;
	R_BModelMatrix (ent, mat);
	return VR_PortalAlias (ent, mat, mat, mapped, source, dest) != 0;
}

static void R_InitBModelInstance (bmodel_gpu_instance_t *inst, entity_t *ent)
{
	float mat[16];

	inst->portalclip[0] = inst->portalclip[1] = inst->portalclip[2] = 0.f;
	inst->portalclip[3] = 1.f;
	R_BModelMatrix (ent, mat); // QVR: (shared with R_PaintBrushWounds)

	MatrixTranspose4x3 (mat, inst->world);
	VR_TemporalBrushPrev (ent, inst->world, inst->prevworld); // QVR

	inst->alpha =ent->alpha == ENTALPHA_DEFAULT ? -1.f : ENTALPHA_DECODE (ent->alpha);
	memset (&inst->padding, 0, sizeof(inst->padding));
	inst->padding[0] = ent == &cl_entities[0] ? 0.f : VR_EntityGlow (ent); // QVR: the shader's glow
	inst->padding[1] = VR_ParallaxDepth (ent, mat, NULL, 1); // QVR: its parallax depth in units (vr_parallax)
	inst->padding[2] = ent == &cl_entities[0] ? 0.f : VR_BrushAOSelf (ent); // QVR: its own dynamic occlusion group (vr/vr_ao.cpp)
	VR_RetroInstance (ent, inst->retro); // QVR: retro textures (vr/vr_retro.cpp)
	inst->retro[3] = ent == &cl_entities[0] || VR_IsRuntimeBrushModel (ent->model) ? 1.f : 0.f; // QVR: the world and the boxes made in the game (vr/vr_brushes.cpp): their decals (vr/vr_decals.cpp, QVR_WORLD_FS_DECALS)
	if (ent == &cl_entities[0]) // QVR: a held prop's blood (vr/vr_wounds.cpp)
	{
		memset (inst->wound, 0, sizeof (inst->wound));
		memset (inst->woundbox, 0, sizeof (inst->woundbox));
	}
	else
		VR_BrushWound (ent, inst->wound, inst->woundbox);
}

/*
=================
R_BModelCentre -- QVR

Where the middle of brush model `e`'s box is drawn this frame (vr/vr_wounds.cpp's tests).
=================
*/
void R_BModelCentre (entity_t *e, float out[3])
{
	float	mat[16];
	vec3_t	mid;
	int		i;

	R_BModelMatrix (e, mat);
	for (i = 0; i < 3; i++)
		mid[i] = 0.5f * (e->model->mins[i] + e->model->maxs[i]);
	for (i = 0; i < 3; i++)
		out[i] = mat[i] * mid[0] + mat[4 + i] * mid[1] + mat[8 + i] * mid[2] + mat[12 + i];
}

/*
=================
R_PaintBrushWounds -- QVR

A held prop's blood (vr/vr_wounds.cpp): brush model `e`'s box, as it is drawn this frame, drawn into the bound
framebuffer (its box mask's layer; the viewport: its region), each side of the box into its cell (vr_glsl.h's
QVR_BOX_WOUND_PAINT_VS), with `numsplats` splats (five vec4 each: the wound paint's fragment shader). The caller sets
the blending.
=================
*/
qboolean R_PaintBrushWounds (entity_t *e, int numsplats, const float *splats)
{
	float	mat[16], world[12], centre[4], half[4];
	int		i;

	if (!e->model || e->model->type != mod_brush || numsplats <= 0)
		return false;
	R_BModelMatrix (e, mat);
	MatrixTranspose4x3 (mat, world);
	centre[3] = 0.f;
	for (i = 0; i < 3; i++)
	{
		centre[i] = 0.5f * (e->model->mins[i] + e->model->maxs[i]);
		half[i] = 0.5f * (e->model->maxs[i] - e->model->mins[i]);
		centre[3] = q_max (centre[3], 2.f * half[i]);
	}
	half[3] = 0.f;
	if (centre[3] <= 0.f)
		return false;

	GL_UseProgram (glprogs.woundpaintbox);
	GL_SetState (GLS_BLEND_OPAQUE | GLS_NO_ZTEST | GLS_NO_ZWRITE | GLS_CULL_NONE | GLS_ATTRIBS (0));
	GL_Uniform1iFunc (0, numsplats);
	GL_Uniform4fvFunc (1, numsplats * 5, splats);
	GL_Uniform4fvFunc (81, 3, world);
	GL_Uniform4fvFunc (84, 1, centre);
	GL_Uniform4fvFunc (85, 1, half);
	glDrawArrays (GL_TRIANGLES, 0, 36);
	return true;
}

/*
=============
R_ResetBModelCalls
=============
*/
static void R_PrewarmArrayTwins (void);
static void R_ResetBModelCalls (GLuint program)
{
	bmodel_batch_program = program;
	num_bmodel_calls = 0;
	bmodel_seg_first = -1; // QVR
	bmodel_arrays_on = false;
	if (!gl_bindless_able) // QVR: the world's textures in arrays (made here: no call pending)
	{
		int arrays = VR_WorldArraysBegin ();
		bmodel_arrays_on = arrays != 0;
		if (arrays == 2) // just made: their programs' twins compiled now, not in play
			R_PrewarmArrayTwins ();
	}
}

/*
=============
R_MergeBModelCallsArrays, R_DrawBModelCallsArrays -- QVR

The flush's calls whose textures are in arrays (vr/vr_worldarrays.cpp; any call for a program reading no texture: the
depth pre-pass), drawn with the program's array twin: each set's calls of one instance as one draw (a group: their
gathered indices one after another, world_arrays_merge_compute_shader; each vertex's call from its command through
CmdCall), the rest of its calls (instanced, or a command already taken) alone with their layer. The calls' order
changes from texture to texture, never within one (the same as the bound path's within a texture). `bound`: the calls
drawn before on the bound path (the report). The merge is dispatched beside the gather (R_FlushBModelCalls), under
its barrier: no compute pass of its own between the draws (on a tiler each one may end the render pass).
=============
*/
typedef struct { int numgroups, numitems; GLuint idxbuf, grpbuf; size_t idxofs, grpofs; } bmodel_merge_t;
static bmodel_merge_t bmodel_merge; // R_MergeBModelCallsArrays' groups, for R_DrawBModelCallsArrays

static qboolean R_MergeBModelCallsArrays (qboolean depthonly)
{
	int			i, g, k, numgroups = 0, numitems = 0;
	GLuint		capacity = 0;
	GLuint		idxbuf = 0, grpbuf = 0, buf;
	size_t		idxofs = 0, grpofs = 0;
	GLbyte		*ofs;
	int			*groupfirst = bmodel_group_first, *groupcount = bmodel_group_count;
	GLuint		*groupbase = bmodel_group_base;
	qboolean	*merged = bmodel_call_merged;

	if (bmodel_cmd_count != gl_bmodel_numcmds) // the map's commands
	{
		VR_HeapFree (bmodel_cmd_call);
		VR_HeapFree (bmodel_cmd_stamp);
		bmodel_cmd_count = gl_bmodel_numcmds;
		bmodel_cmd_call = (GLuint *) VR_HeapCalloc (q_max (bmodel_cmd_count, 1), sizeof (bmodel_cmd_call[0]));
		bmodel_cmd_stamp = (unsigned *) VR_HeapCalloc (q_max (bmodel_cmd_count, 1), sizeof (bmodel_cmd_stamp[0]));
		if (!bmodel_cmd_call || !bmodel_cmd_stamp)
			Sys_Error ("R_DrawBModelCallsArrays: out of memory (%d commands)", bmodel_cmd_count);
	}
	if (++bmodel_flush_stamp == 0)
	{
		memset (bmodel_cmd_stamp, 0, sizeof (bmodel_cmd_stamp[0]) * q_max (bmodel_cmd_count, 1));
		bmodel_flush_stamp = 1;
	}

	// the groups (a set each, one for a program reading no texture), in the order their first calls came; each call's
	// place: merged (one instance, its command's first call here, a map's command) or alone
	for (i = 0; i < num_bmodel_calls; i++)
	{
		int key = depthonly ? 0 : bmodel_calls.bound.set[i], cmd = bmodel_calls.bound.cmd[i];
		int vcmd = gl_bmodel_litcmd_first >= 0 && cmd >= gl_bmodel_litcmd_first ? cmd - gl_bmodel_litcmd_first : cmd; // QVR: a lit command's vertices are its texture's (C4v)
		bmodel_group_of[i] = -1;
		merged[i] = false;
		if (!depthonly && key < 0)
			continue;
		if (bmodel_seg_first >= 0 && i >= bmodel_seg_first) // QVR: the second program's calls in groups of their own (C4v)
			key |= 0x40000000;
		for (g = 0; g < numgroups; g++)
			if (bmodel_group_key[g] == key)
				break;
		if (g == numgroups)
		{
			bmodel_group_key[numgroups] = key;
			groupcount[numgroups] = 0;
			groupbase[numgroups] = 0;
			numgroups++;
		}
		bmodel_group_of[i] = g;
		if (bmodel_calls.bound.numinst[i] == 1 && cmd >= 0 && cmd < bmodel_cmd_count && gl_bmodel_cmdcap[cmd] > 0 &&
			bmodel_cmd_stamp[cmd] != bmodel_flush_stamp)
		{
			bmodel_cmd_stamp[cmd] = bmodel_flush_stamp;
			bmodel_cmd_call[cmd] = (GLuint) i;
			if (vcmd != cmd && bmodel_cmd_stamp[vcmd] != bmodel_flush_stamp) // QVR: CmdCall reads the texture's command: its call the same as this one (C4v)
				bmodel_cmd_call[vcmd] = (GLuint) i;
			merged[i] = true;
			groupcount[g]++;
			groupbase[g] += (GLuint) gl_bmodel_cmdcap[cmd]; // (its capacity, for now)
		}
	}
	for (g = 0, k = 0; g < numgroups; g++) // each group's first item and indices' start
	{
		GLuint cap = groupbase[g];
		groupfirst[g] = k;
		groupbase[g] = capacity;
		capacity += cap;
		k += groupcount[g];
		groupcount[g] = 0;
	}
	for (i = 0; i < num_bmodel_calls; i++)
	{
		bmodel_merge_item_t *item;
		if (!merged[i])
			continue;
		g = bmodel_group_of[i];
		item = &bmodel_merge_items[groupfirst[g] + groupcount[g]++];
		item->cmd = (GLuint) bmodel_calls.bound.cmd[i];
		item->groupfirst = (GLuint) groupfirst[g];
		item->base = groupbase[g];
		item->group = (GLuint) g;
		numitems++;
	}
	for (g = 0; g < numgroups; g++) // its last item writes its draw
		if (groupcount[g] > 0)
			bmodel_merge_items[groupfirst[g] + groupcount[g] - 1].group |= 0x80000000u;

	if (numitems > 0)
	{
		GL_ReserveDeviceMemory (GL_ELEMENT_ARRAY_BUFFER, sizeof (GLuint) * q_max (capacity, 1u), &idxbuf, &idxofs);
		GL_ReserveDeviceMemory (GL_DRAW_INDIRECT_BUFFER, sizeof (bmodel_draw_indirect_t) * numgroups, &grpbuf, &grpofs);
		GL_UseProgram (GL_WorldArraysMergeProgram ());
		GL_BindBufferRange (GL_SHADER_STORAGE_BUFFER, 5, gl_bmodel_indirect_buffer, 0, gl_bmodel_indirect_buffer_size);
		GL_BindBufferRange (GL_SHADER_STORAGE_BUFFER, 6, grpbuf, grpofs, sizeof (bmodel_draw_indirect_t) * numgroups);
		GL_Upload (GL_SHADER_STORAGE_BUFFER, bmodel_merge_items, sizeof (bmodel_merge_items[0]) * numitems, &buf, &ofs);
		GL_BindBufferRange (GL_SHADER_STORAGE_BUFFER, 7, buf, (GLintptr)ofs, sizeof (bmodel_merge_items[0]) * numitems);
		GL_BindBufferRange (GL_SHADER_STORAGE_BUFFER, 8, gl_bmodel_ibo, 0, gl_bmodel_ibo_size);
		GL_BindBufferRange (GL_SHADER_STORAGE_BUFFER, 9, idxbuf, idxofs, sizeof (GLuint) * q_max (capacity, 1u));
		GL_Uniform1iFunc (0, (GLint) (idxofs / sizeof (GLuint)));
		GL_DispatchComputeFunc (numitems, 1, 1); // (the barrier: the gather's, R_FlushBModelCalls)
	}
	bmodel_merge.numgroups = numgroups;
	bmodel_merge.numitems = numitems;
	bmodel_merge.idxbuf = idxbuf;
	bmodel_merge.idxofs = idxofs;
	bmodel_merge.grpbuf = grpbuf;
	bmodel_merge.grpofs = grpofs;
	return numitems > 0;
}

static void R_DrawBModelCallsArrays (GLuint twin, GLuint twin2, qboolean depthonly, GLuint cmdbuf, size_t cmdofs, int bound) // QVR: twin2: the second program's (C4v; 0 none)
{
	int			i, g, alone = 0, many = 0;
	int			numgroups = bmodel_merge.numgroups;
	GLuint		buf;
	GLbyte		*ofs;
	int			*groupcount = bmodel_group_count;
	qboolean	*merged = bmodel_call_merged;

	GL_UseProgram (twin);
	GL_BindBufferRange (GL_SHADER_STORAGE_BUFFER, 5, gl_bmodel_vertcmd_buffer, 0, gl_bmodel_vbo_size / sizeof (glvert_t) * sizeof (GLuint));
	GL_Upload (GL_SHADER_STORAGE_BUFFER, bmodel_cmd_call, sizeof (bmodel_cmd_call[0]) * q_max (bmodel_cmd_count, 1), &buf, &ofs);
	GL_BindBufferRange (GL_SHADER_STORAGE_BUFFER, 6, buf, (GLintptr)ofs, sizeof (bmodel_cmd_call[0]) * q_max (bmodel_cmd_count, 1));
	for (g = 0; g < numgroups; g++)
	{
		if (twin2) // QVR: the group's program (C4v)
			GL_UseProgram ((bmodel_group_key[g] & 0x40000000) ? twin2 : twin);
		if (!depthonly)
			VR_WorldArraysBind (bmodel_group_key[g] & ~0x40000000);
		if (groupcount[g] > 0)
		{
			GL_Uniform1iFunc (0, -1);
			GL_BindBuffer (GL_ELEMENT_ARRAY_BUFFER, bmodel_merge.idxbuf);
			GL_BindBuffer (GL_DRAW_INDIRECT_BUFFER, bmodel_merge.grpbuf);
			VR_GLProbeState ("a world arrays group"); // QVR: vr_gl_probe (Debug > Startup)
			GL_DrawElementsIndirectFunc (GL_TRIANGLES, GL_UNSIGNED_INT, (const byte *)(bmodel_merge.grpofs + g * sizeof (bmodel_draw_indirect_t)));
			many++;
		}
		GL_BindBuffer (GL_ELEMENT_ARRAY_BUFFER, gl_bmodel_ibo);
		GL_BindBuffer (GL_DRAW_INDIRECT_BUFFER, cmdbuf);
		for (i = 0; i < num_bmodel_calls; i++)
		{
			if (bmodel_group_of[i] != g || merged[i])
				continue;
			GL_Uniform1iFunc (0, i);
			GL_DrawElementsIndirectFunc (GL_TRIANGLES, GL_UNSIGNED_INT, (const byte *)(cmdofs + i * sizeof (bmodel_draw_indirect_t)));
			alone++;
		}
	}
	VR_WorldArraysCount (many, bmodel_merge.numitems, alone, bound);
}


/*
=============
R_FlushBModelCalls
=============
*/
static void R_FlushBModelCalls (void)
{
	GLuint	cmdbuf, buf;
	GLbyte	*ofs;
	size_t	dstcmdofs;
	GLuint	twin = 0; // QVR: the program's array twin (the world's textures in arrays: vr/vr_worldarrays.cpp)
	GLuint	twin2 = 0; // QVR: the second program's (C4v)
	int		seg; // QVR: the second program's first call (num_bmodel_calls: none)
	qboolean depthonly = false, merging = false;

	if (!num_bmodel_calls)
		return;
	seg = bmodel_seg_first >= 0 && bmodel_seg_first < num_bmodel_calls ? bmodel_seg_first : num_bmodel_calls;
	if (!gl_bindless_able && bmodel_arrays_on && GL_WorldArraysMergeProgram ()) // QVR
	{
		twin = GL_WorldArraysProgram (bmodel_batch_program, &depthonly);
		if (twin && seg < num_bmodel_calls)
		{
			qboolean depthonly2 = false;
			twin2 = GL_WorldArraysProgram (bmodel_seg_program, &depthonly2);
			if (!twin2 || depthonly2 != depthonly) // (no twin: every call bound)
				twin = twin2 = 0;
		}
	}

	GL_ReserveDeviceMemory (GL_DRAW_INDIRECT_BUFFER, sizeof (bmodel_draw_indirect_t) * num_bmodel_calls, &cmdbuf, &dstcmdofs);

	GL_BeginGroup ("cs: gather indirect draws"); // QVR: (vr_gl_markers)
	if (twin) // QVR: (its own bindings; the gather's are made after)
		merging = R_MergeBModelCallsArrays (depthonly);
	GL_UseProgram (glprogs.gather_indirect);
	GL_BindBufferRange (GL_SHADER_STORAGE_BUFFER, 5, gl_bmodel_indirect_buffer, 0, gl_bmodel_indirect_buffer_size);
	GL_BindBufferRange (GL_SHADER_STORAGE_BUFFER, 6, cmdbuf, dstcmdofs, sizeof (bmodel_draw_indirect_t) * num_bmodel_calls);
	GL_Upload (GL_SHADER_STORAGE_BUFFER, bmodel_call_remap, sizeof (bmodel_call_remap[0]) * num_bmodel_calls, &buf, &ofs);
	GL_BindBufferRange  (GL_SHADER_STORAGE_BUFFER, 7, buf, (GLintptr)ofs, sizeof (bmodel_call_remap[0]) * num_bmodel_calls);
	GL_DispatchComputeFunc ((num_bmodel_calls + 63) / 64, 1, 1);
	GL_MemoryBarrierFunc (GL_COMMAND_BARRIER_BIT | (merging ? GL_ELEMENT_ARRAY_BARRIER_BIT : 0)); // QVR: (the merge's indices)
	GL_EndGroup (); // QVR

	GL_UseProgram (bmodel_batch_program);
	VR_RetroBind (QVR_RETRO_LUT_UNIT_WORLD); // QVR: the palette's table for retro textures (vr/vr_retro.cpp)
	GL_BindBuffer (GL_ELEMENT_ARRAY_BUFFER, gl_bmodel_ibo);
	GL_BindBuffer (GL_ARRAY_BUFFER, gl_bmodel_vbo);
	GL_BindBuffer (GL_DRAW_INDIRECT_BUFFER, cmdbuf);
	GL_VertexAttribPointerFunc (0, 3, GL_FLOAT, GL_FALSE, sizeof (glvert_t), (void *) offsetof (glvert_t, pos));
	GL_VertexAttribPointerFunc (1, 4, GL_FLOAT, GL_FALSE, sizeof (glvert_t), (void *) offsetof (glvert_t, st));
	GL_VertexAttribPointerFunc (2, 1, GL_FLOAT, GL_FALSE, sizeof (glvert_t), (void *) offsetof (glvert_t, lmofs));
	GL_VertexAttribIPointerFunc (3, 4, GL_UNSIGNED_BYTE, sizeof (glvert_t), (void *) offsetof (glvert_t, styles));
	GL_VertexAttribPointerFunc (4, 1, GL_FLOAT, GL_FALSE, sizeof (glvert_t), (void *) offsetof (glvert_t, swellpin));
	GL_VertexAttribPointerFunc (5, 3, GL_FLOAT, GL_FALSE, sizeof (glvert_t), (void *) offsetof (glvert_t, centre));
	GL_VertexAttribPointerFunc (6, 4, GL_FLOAT, GL_FALSE, sizeof (glvert_t), (void *) offsetof (glvert_t, grime)); // QVR: the permanent decals' layer (enabled while it is read: R_DrawBrushModels_Real)

	if (gl_bindless_able)
	{
		GL_Upload (GL_SHADER_STORAGE_BUFFER, bmodel_calls.bindless.params, sizeof (bmodel_calls.bindless.params[0]) * num_bmodel_calls, &buf, &ofs);
		GL_BindBufferRange (GL_SHADER_STORAGE_BUFFER, 1, buf, (GLintptr)ofs, sizeof (bmodel_calls.bindless.params[0]) * num_bmodel_calls);
		if (seg > 0)
			GL_MultiDrawElementsIndirectFunc (GL_TRIANGLES, GL_UNSIGNED_INT, (const void *)dstcmdofs, seg, sizeof (bmodel_draw_indirect_t));
		if (seg < num_bmodel_calls) // QVR: the second program's (C4v)
		{
			GL_UseProgram (bmodel_seg_program);
			GL_MultiDrawElementsIndirectFunc (GL_TRIANGLES, GL_UNSIGNED_INT, (const void *)(dstcmdofs + seg * sizeof (bmodel_draw_indirect_t)), num_bmodel_calls - seg, sizeof (bmodel_draw_indirect_t));
		}
	}
	else
	{
		int i, bound = 0;
		qboolean second = false; // QVR: the second program bound (C4v)

		GL_Upload (GL_SHADER_STORAGE_BUFFER, &bmodel_calls.bound.params, sizeof (bmodel_calls.bound.params[0]) * num_bmodel_calls, &buf, &ofs);
		GL_BindBufferRange (GL_SHADER_STORAGE_BUFFER, 1, buf, (GLintptr)ofs, sizeof (bmodel_calls.bound.params[0]) * num_bmodel_calls);

		for (i = 0; i < num_bmodel_calls; i++)
		{
			if (twin && (depthonly || bmodel_calls.bound.set[i] >= 0)) // QVR: drawn from the arrays (R_DrawBModelCallsArrays)
				continue;
			if (i >= seg && !second) // QVR
			{
				GL_UseProgram (bmodel_seg_program);
				second = true;
			}
			bound++;
			GL_Uniform1iFunc (0, i);
			GL_BindTextures (0, 2, bmodel_calls.bound.textures[i]);
			GL_Bind (GL_TEXTURE3, bmodel_calls.bound.textures[i][2]); // QVR: the normal map
			GL_Bind (GL_TEXTURE11, bmodel_calls.bound.textures[i][3]); // QVR: the specular map (SpecTex)
			VR_GLProbeState ("a world call"); // QVR: vr_gl_probe (Debug > Startup)
			GL_DrawElementsIndirectFunc (GL_TRIANGLES, GL_UNSIGNED_INT, (const byte *)(dstcmdofs + i * sizeof (bmodel_draw_indirect_t)));
		}
		if (twin) // QVR
			R_DrawBModelCallsArrays (twin, twin2, depthonly, cmdbuf, dstcmdofs, bound);
	}

	num_bmodel_calls = 0;
	if (bmodel_seg_first >= 0) // QVR: the second program's calls go on (an overflow's flush) with it alone (C4v)
	{
		bmodel_batch_program = bmodel_seg_program;
		bmodel_seg_first = -1;
	}
}

/*
=============
R_BModelCallsSegment -- QVR

C4v: the calls added from here on drawn with `program` in the same flush (one gather: on a tiler a compute pass
between draws may end the render pass).
=============
*/
static void R_BModelCallsSegment (GLuint program)
{
	if (program == bmodel_batch_program && bmodel_seg_first < 0)
		return;
	bmodel_seg_first = num_bmodel_calls;
	bmodel_seg_program = program;
}

/*
=============
R_BModelTextures -- QVR

A call's maps for texture `t` (NULL: white, showtris): the texture (NULL: none, the lightmap alone), its fullbright
(NULL: none), its normal map (or an external pack's, vr/vr_extmaps.cpp), its specular map (NULL: none); the external
maps' numbers in `extmat`; returns their call flags. R_BModelCallTextures: the four the bound path binds.
=============
*/
static unsigned R_BModelTextures (texture_t *t, gltexture_t **tx, gltexture_t **fb, gltexture_t **nm, gltexture_t **spec, float extmat[4])
{
	unsigned extflags;
	if (t)
	{
		*tx = t->gltexture;
		*fb = t->fullbright;
		if (r_lightmap_cheatsafe)
			*tx = *fb = NULL;
		if (!gl_fullbrights.value && t->type != TEXTYPE_SKY)
			*fb = NULL;
	}
	else
	{
		*tx = *fb = whitetexture;
	}
	*nm = TexMgr_NormalMap (*tx); // QVR: or an external pack's, its specular map, its glow hidden (vr/vr_extmaps.cpp)
	extflags = VR_ExtMapsCall (*tx && t ? t : NULL, nm, spec, fb, extmat);
	VR_PBRStrength (*nm, &extmat[0], &extmat[3]); // QVR: independent component provenance after authored pack selection
	return extflags;
}

void R_BModelCallTextures (texture_t *t, gltexture_t *out[4])
{
	gltexture_t *tx, *fb, *nm, *spec;
	float extmat[4];
	R_BModelTextures (t, &tx, &fb, &nm, &spec, extmat);
	out[0] = tx ? tx : greytexture;
	out[1] = fb ? fb : blacktexture;
	out[2] = nm;
	out[3] = spec ? spec : whitetexture;
}


/*
=============
R_AddBModelCall
=============
*/
static void R_AddBModelCall (int index, int first_instance, int num_instances, texture_t *t, qboolean zfix, entity_t *ent) // QVR: ent, the batch's first (retro textures)
{
	static const float noclamp[4] = {0.f, 0.f, 0.f, 0.f}; // QVR
	GLuint		flags;
	float		alpha;
	gltexture_t	*tx, *fb;
	gltexture_t	*nm, *spec; // QVR
	float		extmat[4]; // QVR
	unsigned	extflags; // QVR

	SDL_assert (num_instances > 0);
	if (num_instances <= 0) // QVR: an empty batch draws nothing (its remap's instance would underflow)
		return;

	if (num_bmodel_calls == MAX_BMODEL_DRAWS)
		R_FlushBModelCalls ();

	if (!gl_zfix.value || map_checks.value)
		zfix = 0;

	extflags = R_BModelTextures (t, &tx, &fb, &nm, &spec, extmat); // QVR: (shared with the arrays' making)
	if (bmodel_relief_filter && (extmat[3] > 0.f) != (bmodel_relief_filter == 1)) // QVR: the depth write's split (R_DrawSolidSplit): extmat[3] the relief's height (the shader's in_pdepth: 0 none)
		return;
	flags =zfix | ((fb != NULL) << 1) | ((r_fullbright_cheatsafe != false) << 2) | extflags;
	if (t && TEXTYPE_ISLIQUID (t->type)) // QVR: the liquid's kind for its look (gl_shaders.h LiquidKind)
		flags |= (t->type - TEXTYPE_FIRSTLIQUID + 1) << 3;
	alpha = t ? GL_WaterAlphaForTextureType (t->type) : 1.f;

	if (gl_bindless_able)
	{
		bmodel_bindless_gpu_call_t *call = &bmodel_calls.bindless.params[num_bmodel_calls];
		call->flags = flags;
		call->alpha = alpha;
		call->texture = tx ? tx->bindless_handle : greytexture->bindless_handle;
		call->fullbright = fb ? fb->bindless_handle : blacktexture->bindless_handle;
		call->normalmap = nm->bindless_handle; // QVR
		call->specmap = (spec ? spec : whitetexture)->bindless_handle; // QVR
		call->padding = 0;
		memcpy (call->uvclamp, t ? t->uvclamp : noclamp, sizeof (call->uvclamp)); // QVR
		VR_DetailCall (tx ? t : NULL, call->detail); // QVR
		memcpy (call->extmat, extmat, sizeof (call->extmat)); // QVR
		VR_RetroCall (ent, tx ? t : NULL, call->retro); // QVR
	}
	else
	{
		bmodel_bound_gpu_call_t *call = &bmodel_calls.bound.params[num_bmodel_calls];
		gltexture_t **textures = bmodel_calls.bound.textures[num_bmodel_calls];
		call->flags = flags;
		call->alpha = alpha;
		call->baseinstance = first_instance;
		textures[0] = tx ? tx : greytexture;
		textures[1] = fb ? fb : blacktexture;
		textures[2] = nm; // QVR
		textures[3] = spec ? spec : whitetexture; // QVR
		call->layer = 0; // QVR: the world's textures in arrays (vr/vr_worldarrays.cpp)
		bmodel_calls.bound.set[num_bmodel_calls] = bmodel_arrays_on && tx && t ? VR_WorldArraysLookup (t, textures, &call->layer) : -1;
		bmodel_calls.bound.cmd[num_bmodel_calls] = index;
		bmodel_calls.bound.numinst[num_bmodel_calls] = num_instances;
		memcpy (call->uvclamp, t ? t->uvclamp : noclamp, sizeof (call->uvclamp)); // QVR
		VR_DetailCall (tx ? t : NULL, call->detail); // QVR
		memcpy (call->extmat, extmat, sizeof (call->extmat)); // QVR
		VR_RetroCall (ent, tx ? t : NULL, call->retro); // QVR
	}

	SDL_assert (num_instances <= MAX_BMODEL_INSTANCES);
	bmodel_call_remap[num_bmodel_calls].src = index;
	bmodel_call_remap[num_bmodel_calls].inst = first_instance * MAX_BMODEL_INSTANCES + (num_instances - 1);

	++num_bmodel_calls;
}

/*
=============
R_WorldFragments_f

QVR: vr_world_fragments [passes] (a debug tool: Debug > Profiling and Memory): over the next passes (16) of the opaque
world and brush models, the fragments their depth pre-pass and their shading pass run the fragment shader on
(pipeline statistics), and the shading pass's kept ones (samples passed): shaded over kept is the shading wasted on
hidden pixels (whether the depth-writing programs still reject them early: vr_parallax_depth_write). Each
pass's counts are read at once (a stall: for measuring counts, not times).
=============
*/
#define QVR_GL_FRAGMENT_SHADER_INVOCATIONS 0x82F4 // GL 4.6 / ARB_pipeline_statistics_query
typedef struct
{
	int		left, passes;
	GLuint	queries[2];		// fragment shader invocations, samples passed
	double	prepass, shaded, kept;
	int		splits;			// the passes drawn split (R_DrawSolidSplit)
} worldfragments_t;
static worldfragments_t worldfragments;

void R_WorldFragments_f (void)
{
	if (VR_GLESCompat ())
	{
		Con_Printf ("vr_world_fragments: not under -glescompat (OpenGL ES has no pipeline statistics or sample counts)\n");
		return;
	}
	if (worldfragments.queries[0])
		GL_DeleteQueriesFunc (2, worldfragments.queries);
	memset (&worldfragments, 0, sizeof (worldfragments));
	GL_GenQueriesFunc (2, worldfragments.queries);
	worldfragments.left = Cmd_Argc () > 1 ? q_max (1, atoi (Cmd_Argv (1))) : 16;
	Con_Printf ("vr_world_fragments: counting the next %d opaque world passes\n", worldfragments.left);
}

static void R_WorldFragmentsBegin (qboolean shading)
{
	if (worldfragments.left <= 0)
		return;
	GL_BeginQueryFunc (QVR_GL_FRAGMENT_SHADER_INVOCATIONS, worldfragments.queries[0]);
	if (shading)
		GL_BeginQueryFunc (GL_SAMPLES_PASSED, worldfragments.queries[1]);
}

static void R_WorldFragmentsEnd (qboolean shading)
{
	GLuint64 invocations = 0, samples = 0;
	if (worldfragments.left <= 0)
		return;
	GL_EndQueryFunc (QVR_GL_FRAGMENT_SHADER_INVOCATIONS);
	GL_GetQueryObjectui64vFunc (worldfragments.queries[0], GL_QUERY_RESULT, &invocations);
	if (!shading)
	{
		worldfragments.prepass += (double)invocations;
		return;
	}
	GL_EndQueryFunc (GL_SAMPLES_PASSED);
	GL_GetQueryObjectui64vFunc (worldfragments.queries[1], GL_QUERY_RESULT, &samples);
	worldfragments.shaded += (double)invocations;
	worldfragments.kept += (double)samples;
	worldfragments.passes++;
	if (--worldfragments.left > 0)
		return;
	Con_Printf ("vr_world_fragments: %d passes (pdo %d, split %d at %.0f): a pass's pre-pass shaded %.0f, "
		"shading shaded %.0f, kept %.0f: shaded/kept %.3f\n", worldfragments.passes, r_framedata.parallax2[3] > 0.f ? 1 : 0,
		worldfragments.splits, r_framedata.parallax3[1],
		worldfragments.prepass / worldfragments.passes,
		worldfragments.shaded / worldfragments.passes, worldfragments.kept / worldfragments.passes,
		worldfragments.shaded / q_max (worldfragments.kept, 1.0));
	GL_DeleteQueriesFunc (2, worldfragments.queries);
	worldfragments.queries[0] = worldfragments.queries[1] = 0;
}

/*
=============
R_ChooseBModelProgram
=============
*/
static int R_WorldDither (void)
{
	extern cvar_t r_softemu_lightmap_banding;

	switch (softemu)
	{
	case SOFTEMU_BANDED:
		return r_softemu_lightmap_banding.value != 0.f ? 2 : 1;

	case SOFTEMU_COARSE:
		return r_softemu_lightmap_banding.value > 0.f ? 2 : 1;

	default:
		return r_softemu_lightmap_banding.value > 0.f ? 2 : 0;
	}
}

/*
=============
R_PrewarmDecalPrograms

QVR: the world's programs with the decals' retro textures' code, made in the map's load (vr/vr_decals.cpp prepare) when
the decals have a retro set: made by the first mark's frame instead, they were the first punch's hitch (8 ms on the main
thread; ROUND22.md, "Gore hitches"). Once a session (kept until a video restart). Only the opaque world's (`pdo`: the
parallax's depth written, as R_DrawBrushModels_Real picks it): the translucent brush models' (non-pdo, OIT) are made
when first drawn with marks about, as before (1.6-1.7 s each here, uncached by the driver: too long for every load).
=============
*/
void R_PrewarmDecalPrograms (qboolean pdo)
{
	GL_WorldDecalRetroProgram (0, R_WorldDither (), pdo);
	if (pdo) // its plain twin too: the depth write's split draws the far and flat surfaces with it (R_DrawSolidSplit)
		GL_WorldDecalRetroProgram (0, R_WorldDither (), false);
	if (bmodel_arrays_on) // QVR: and their array twins (vr/vr_worldarrays.cpp)
		R_PrewarmArrayTwins ();
}

/*
=============
R_PrewarmArrayTwins -- QVR

The array twins (vr/vr_worldarrays.cpp, GL_WorldArraysProgram) of the world programs this view's settings draw with
(its dither; opaque and OIT, solid and fence, the unlit ones (the ES profile's vr_gl_world_unlit), the parallax depth
write's, the pre-passes, the decals' retro ones made so far), compiled when the arrays are made at the map's load: a twin made in play stalls a frame (a world shader:
over a second on this PC).
=============
*/
static void R_PrewarmArrayTwins (void)
{
	const int dither = R_WorldDither ();
	qboolean depthonly;
	int oit, alphatest;
	// (under ES the OIT ones only while OIT is on: they are made when first asked for, GL_ShadersLazyStart)
	const int oits = GL_ShadersLazyStart (1, 0, 0) || R_GetEffectiveAlphaMode () == ALPHAMODE_OIT ? 2 : 1;
	for (oit = 0; oit < oits; oit++)
	{
		for (alphatest = 0; alphatest < 2; alphatest++)
			GL_WorldArraysProgram (GL_WorldProgram (oit, dither, alphatest), &depthonly);
		if (glprogs.world_decal_retro[oit][dither])
			GL_WorldArraysProgram (glprogs.world_decal_retro[oit][dither], &depthonly);
	}
	for (alphatest = 0; alphatest < 2; alphatest++)
		if (glprogs.world_unlit[dither][alphatest]) // (the split's batch program: R_AddBModelPassCallsSplit)
			GL_WorldArraysProgram (glprogs.world_unlit[dither][alphatest], &depthonly);
	GL_WorldArraysProgram (glprogs.world_pdo[dither], &depthonly);
	if (glprogs.world_pdo_decal_retro[dither])
		GL_WorldArraysProgram (glprogs.world_pdo_decal_retro[dither], &depthonly);
	GL_WorldArraysProgram (glprogs.world_depth, &depthonly);
	GL_WorldArraysProgram (glprogs.world_depth_pdo, &depthonly);
	GL_WorldArraysMergeProgram ();
}

/*
=============
R_PrewarmTranslucentDecalPrograms

QVR: the translucent brush models' program with the decals' retro code (OIT: R_ChooseBModelProgram's), made in the
load's last frame when the map has a translucent brush model (vr/vr_decals.cpp prepareTranslucent): made in play it
froze the game 2.6-3.1 s (AD's maps; the opaque world's are R_PrewarmDecalPrograms'). Sorted blending's is the
opaque world's plain one, made there.
=============
*/
void R_PrewarmTranslucentDecalPrograms (void)
{
	if (R_GetEffectiveAlphaMode () == ALPHAMODE_OIT)
		GL_WorldDecalRetroProgram (1, R_WorldDither (), false);
	if (bmodel_arrays_on) // QVR: and its array twin (arrays made later: R_PrewarmArrayTwins finds it then)
		R_PrewarmArrayTwins ();
}

/*
=============
R_PrewarmSelectedPrograms -- QVR

At a map's load (R_NewMap): the ES profile's variants the current settings draw with (the world's, the liquids', the
models' and their forms, the particles'), which it makes when first asked for (GL_ShadersLazyStart): compiled in the
load, not at the first frames' draws. Nothing on the PC (made at the start).
=============
*/
void R_PrewarmSelectedPrograms (void)
{
	const int dither = R_WorldDither ();
	const int oits = R_GetEffectiveAlphaMode () == ALPHAMODE_OIT ? 2 : 1;
	const int coarse = softemu == SOFTEMU_COARSE;
	const int amode = R_AliasShaderMode ();
	int oit, mode, alphatest, poseverttype, form;
	if (GL_ShadersLazyStart (1, 1, 1))
		return;
	for (oit = 0; oit < oits; oit++)
	{
		for (mode = 0; mode < 3; mode++)
			GL_WorldProgram (oit, dither, mode);
		GL_WorldProgram (oit, q_max (0, (int)softemu - 1), WORLDSHADER_WATER);
		GL_WaterProgram (oit, coarse);
		GL_ParticlesProgram (oit, coarse);
		for (alphatest = 0; alphatest < 2; alphatest++)
			for (poseverttype = 0; poseverttype < 3; poseverttype++)
				GL_AliasProgram (oit, amode, alphatest, poseverttype);
	}
	for (form = 0; form < ALIASFORM_COUNT; form++)
		for (poseverttype = 0; poseverttype < 3; poseverttype++)
			GL_AliasFormProgram (form, amode, poseverttype);
}

static GLuint R_ChooseBModelProgram (qboolean oit, qboolean alphatest, qboolean pdo)
{
	const int dither = R_WorldDither ();
	if (!alphatest && r_framedata.decalclock[3] > 0.5f) // QVR: the decals with retro textures (VR_DecalsFrame: their set)
		return GL_WorldDecalRetroProgram (oit, dither, pdo && !oit);
	if (pdo && !oit && !alphatest) // QVR: the opaque world writing its parallax hits' depth (vr_parallax_depth_write)
		return glprogs.world_pdo[dither];
	if (!oit && glprogs.world_unlit[dither][alphatest] && VR_WorldUnlit ()) // QVR: no light reaches the world: no lights' loop (C4t, ES only)
		return glprogs.world_unlit[dither][alphatest];
	return GL_WorldProgram (oit, dither, alphatest); // QVR: made when first asked for under ES
}

typedef enum {
	BP_SOLID,
	BP_ALPHATEST,
	BP_SKYLAYERS,
	BP_SKYCUBEMAP,
	BP_SKYSTENCIL,
	BP_SHOWTRIS,
} brushpass_t;

static void R_AddBModelPassCalls (entity_t **ents, int count, textype_t texbegin, textype_t texend, brushpass_t pass); // QVR

/*
=============
R_AddBModelPassCallsSplit -- QVR

C4v (QUEST_PLAN.md 6zb): the pass's calls, after R_ResetBModelCalls (program). Split by surface (r_world_litsplit,
the ES profile's vr_gl_world_unlit 2) with `program` the world's plain one: the world's unlit surfaces (and the brush
entities unless a light touches one) with its form without the lights' loop, the lit ones with `program`, in one
flush. Else every call with `program` (the lit commands too: the cull filled them).
=============
*/
static void R_AddBModelPassCallsSplit (entity_t **ents, int count, textype_t texbegin, textype_t texend, brushpass_t pass, GLuint program)
{
	int dither, alphatest = pass == BP_ALPHATEST;
	if (r_world_litsplit >= 0 && (pass == BP_SOLID || pass == BP_ALPHATEST))
	{
		dither = R_WorldDither ();
		if (program == glprogs.world[0][dither][alphatest] && glprogs.world_unlit[dither][alphatest] && num_bmodel_calls == 0)
		{
			bmodel_batch_program = glprogs.world_unlit[dither][alphatest];
			bmodel_lit_mode = 1;
			R_AddBModelPassCalls (ents, count, texbegin, texend, pass);
			R_BModelCallsSegment (program);
			bmodel_lit_mode = 2;
			R_AddBModelPassCalls (ents, count, texbegin, texend, pass);
			bmodel_lit_mode = 0;
			return;
		}
	}
	R_AddBModelPassCalls (ents, count, texbegin, texend, pass);
}

/*
=============
R_DrawSolidSplit

QVR: the opaque world's draws with the parallax depth write (vr_parallax_depth_write) split three ways, `reliefprog`
writing gl_FragDepth, `flatprog` not. This NVIDIA driver runs every fragment of a program that writes gl_FragDepth
(no early depth test, whatever its layout), so the pre-pass's depth saved it nothing and the costly shading ran on
every hidden pixel too (2.5-3.2x the pixels kept in ad_tfuma; ROUND22.md, "Parallax depth write's cost"). Here only
the textures with relief (extmat[3] > 0), nearer than Parallax3.y in view depth (GL_CLIP_DISTANCE2, the world vertex
shader), go to `reliefprog`; the same textures further away, and the textures without relief, to `flatprog`, whose
fragments the driver still rejects early. The picture is the same: further than the parallax's reach and without
relief, `reliefprog` writes the surface's own depth too. Parallax3.y 0 (no split): all to `reliefprog`, as before.
=============
*/
static void R_DrawSolidSplit (entity_t **ents, int count, textype_t texbegin, textype_t texend, GLuint reliefprog, GLuint flatprog)
{
	if (r_framedata.parallax3[1] <= 0.f || reliefprog == flatprog)
	{
		R_ResetBModelCalls (reliefprog);
		R_AddBModelPassCallsSplit (ents, count, texbegin, texend, BP_SOLID, reliefprog); // QVR: (C4v)
		R_FlushBModelCalls ();
		return;
	}
	if (worldfragments.left > 0) // (vr_world_fragments)
		worldfragments.splits++;
	glEnable (GL_CLIP_DISTANCE2);
	bmodel_relief_filter = 1;
	R_ResetBModelCalls (reliefprog); // the relief, near
	R_AddBModelPassCalls (ents, count, texbegin, texend, BP_SOLID);
	R_FlushBModelCalls ();
	R_ResetBModelCalls (flatprog); // the relief, far
	R_AddBModelPassCalls (ents, count, texbegin, texend, BP_SOLID);
	R_FlushBModelCalls ();
	glDisable (GL_CLIP_DISTANCE2);
	bmodel_relief_filter = 2;
	R_ResetBModelCalls (flatprog); // no relief
	R_AddBModelPassCalls (ents, count, texbegin, texend, BP_SOLID);
	R_FlushBModelCalls ();
	bmodel_relief_filter = 0;
}

/*
=============
R_DrawBrushModels_Real
=============
*/
static void R_DrawBrushModels_Real (entity_t **ents, int count, brushpass_t pass, qboolean translucent)
{
	int i; // QVR: (j and baseinst: R_AddBModelPassCalls)
	int totalinst;
	unsigned state;
	GLuint program;
	GLuint flatprog = 0; // QVR: the opaque pass's plain program beside the depth-writing one (R_DrawSolidSplit)
	GLuint buf;
	GLbyte *ofs;
	textype_t texbegin, texend;
	qboolean oit;
	qboolean a2c; // QVR

	if (!count)
		return;

	if (count > countof(bmodel_instances))
	{
		Con_DWarning ("bmodel instance overflow: %d > %d\n", count, (int)countof(bmodel_instances));
		count = countof(bmodel_instances);
	}

	VR_TracyBegin ("brush program and instance fill", 0); // QVR: (profiling builds)
	oit = translucent && R_GetEffectiveAlphaMode () == ALPHAMODE_OIT;
	if (oit) // QVR: the OIT targets cleared at their first draw
		R_PrepareOITDraw (3);
	switch (pass)
	{
	default:
	case BP_SOLID:
		texbegin = 0;
		texend = TEXTYPE_CUTOUT;
		program = R_ChooseBModelProgram (oit, false, !translucent && r_framedata.parallax2[3] > 0.f); // QVR: pdo
		flatprog = !translucent && r_framedata.parallax3[1] > 0.f ? R_ChooseBModelProgram (oit, false, false) : program; // QVR: the depth write's split (R_DrawSolidSplit)
		break;
	case BP_ALPHATEST:
		texbegin = TEXTYPE_CUTOUT;
		texend = TEXTYPE_CUTOUT + 1;
		program = R_ChooseBModelProgram (oit, true, false);
		break;
	case BP_SKYLAYERS:
		texbegin = TEXTYPE_SKY;
		texend = TEXTYPE_SKY + 1;
		program = glprogs.skylayers[softemu == SOFTEMU_COARSE];
		break;
	case BP_SKYCUBEMAP:
		texbegin = TEXTYPE_SKY;
		texend = TEXTYPE_SKY + 1;
		program = glprogs.skycubemap[Sky_IsAnimated ()][softemu == SOFTEMU_COARSE];
		break;
	case BP_SKYSTENCIL:
		texbegin = TEXTYPE_SKY;
		texend = TEXTYPE_SKY + 1;
		program = glprogs.skystencil;
		break;
	case BP_SHOWTRIS:
		texbegin = 0;
		texend = TEXTYPE_COUNT;
		program = GL_WorldProgram (0, 0, 0);
		break;
	}

	// fill instance data
	for (i = 0, totalinst = 0; i < count; i++)
	{
		bmodel_portal_counts[i] = 1;
		if (ents[i]->model->texofs[texend] - ents[i]->model->texofs[texbegin] > 0)
		{
			bmodel_gpu_instance_t* inst = &bmodel_instances[totalinst++];
			float mat[16], mapped[16], source[4], dest[4];
			R_InitBModelInstance (inst, ents[i]);
			R_BModelMatrix (ents[i], mat);
			if (ents[i] != &cl_entities[0] && ents[i]->model->name[0] != '*' &&
				VR_PortalAlias (ents[i], mat, mat, mapped, source, dest))
			{
				bmodel_gpu_instance_t* copy = &bmodel_instances[totalinst++];
				*copy = *inst;
				MatrixTranspose4x3 (mapped, copy->world);
				memcpy (copy->prevworld, copy->world, sizeof (copy->world)); // QVR: (the gate's other side: no motion of its own)
				memcpy(inst->portalclip, source, sizeof(source));
				memcpy(copy->portalclip, dest, sizeof(dest));
				bmodel_portal_counts[i] = 2;
			}
		}
	}


	VR_TracyEnd (); // QVR
	if (!totalinst)
		return;

	VR_TracyBegin ("brush state and instances upload", 0); // QVR: (profiling builds; a first use's program link shows here)
	if (pass <= BP_ALPHATEST || pass == BP_SHOWTRIS)
		glEnable (GL_CLIP_DISTANCE1);
	// setup state
	state = GLS_CULL_BACK | GLS_ATTRIBS(pass == BP_SOLID && VR_PermDecalsOn () ? 7 : 4); // QVR: 7: the permanent decals' layer (attribute 6; vr/vr_permdecals.cpp)
	if (!translucent)
		state |= GLS_BLEND_OPAQUE;
	else
		state |= GLS_BLEND_ALPHA_OIT | GLS_NO_ZWRITE;
	
	R_ResetBModelCalls (program);
	GL_SetState (state);
	a2c = pass == BP_ALPHATEST && !translucent && VR_AlphaToCoverage (); // QVR: fences' edges by alpha to coverage (vr_alpha_coverage, MSAA)
	if (a2c)
	{
		glEnable (GL_SAMPLE_ALPHA_TO_COVERAGE);
		glEnable (GL_SAMPLE_ALPHA_TO_ONE);
	}
	if (pass <= BP_ALPHATEST)
	{ // QVR: and the deluxemaps
		GL_Bind (GL_TEXTURE2, r_fullbright_cheatsafe ? greytexture : lightmap_texture);
		GL_Bind (GL_TEXTURE9, lux_texture); // QVR: the light's directions (deluxemaps: LuxTex; read only with ShadowFlags 128)
		if (VR_DlightmapTexture ()) // QVR: the dynamic lights in a texture (vr_gl_world_dlmap: DlightTex, read by glprogs.world_unlit)
			GL_BindNative (GL_TEXTURE15, GL_TEXTURE_2D, VR_DlightmapTexture ());
		else
			GL_Bind (GL_TEXTURE15, blacktexture);
		GL_BindNative (GL_TEXTURE13, GL_TEXTURE_2D_ARRAY, VR_WoundTexture ()); // QVR: a held prop's blood (WoundMasks: BoxWounds)
		VR_BindDecals (); // QVR: the decals on the world (vr/vr_decals.cpp: buffers 3 and 4, the atlas on unit 14)
	}
	else if (pass == BP_SKYCUBEMAP)
		GL_Bind (GL_TEXTURE2, skybox->cubemap);

	GL_Upload (GL_SHADER_STORAGE_BUFFER, bmodel_instances, sizeof(bmodel_instances[0]) * totalinst, &buf, &ofs);
	GL_BindBufferRange (GL_SHADER_STORAGE_BUFFER, 2, buf, (GLintptr)ofs, sizeof(bmodel_instances[0]) * totalinst);
	VR_GLProbeUpload ("world instances", buf, (size_t)ofs, bmodel_instances, sizeof(bmodel_instances[0]) * totalinst); // QVR: vr_gl_probe
	VR_TracyEnd (); // QVR

	// QVR: the opaque world and brush models' depth first (the same draws, depth only), so that their costly shading
	// (parallax, bumps, the lights' shadows) runs once a pixel, for the nearest surface only, not for the ones drawn
	// over later. The vertex shader's gl_Position is invariant: the same depths, which the shading pass then passes.
	// Off (vr_world_prepass 0): a tiler's own early depth test (Adreno's LRZ, made at binning from every draw) may
	// already reject the hidden surfaces, the pass then only doubling the world's vertices and binning (questperf).
	if (pass == BP_SOLID && !translucent && VR_WorldPrepass ())
	{
		VR_GPUZoneBegin ("depth prepass"); // QVR: a GL debug group (vr_gl_markers); profiling builds: a GPU zone with vr_tracy_gpu_detail
		GLuint depthprog = r_framedata.parallax2[3] > 0.f ? glprogs.world_depth_pdo : glprogs.world_depth; // QVR: with pixel depth offset, as far as the parallax hits can be (vr_parallax_depth_write)
		glColorMask (GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
		R_WorldFragmentsBegin (false); // QVR: (vr_world_fragments)
		R_DrawSolidSplit (ents, count, texbegin, texend, depthprog, flatprog != program ? glprogs.world_depth : depthprog); // QVR: split as the shading is
		R_WorldFragmentsEnd (false); // QVR
		glColorMask (GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		R_ResetBModelCalls (program);
		VR_GPUZoneEnd ();
	}

	// QVR: the shading (profiling builds: a GPU zone with vr_tracy_gpu_detail): the opaque pass's holds the parallax,
	// the carved decals' march, the bumps, the PBR and the lights' shadows, all in the one world shader
	VR_GPUZoneBegin (pass == BP_SOLID ? (translucent ? "shading: translucent" : "shading: opaque (parallax, carved decals)") :
		pass == BP_ALPHATEST ? "shading: fences" : pass == BP_SHOWTRIS ? "shading: showtris" : "shading: sky");
	if (pass == BP_SOLID && !translucent)
		R_WorldFragmentsBegin (true); // QVR: (vr_world_fragments)
	if (pass == BP_SOLID && !translucent) // QVR: the depth write's split (R_DrawSolidSplit)
	{
		VR_TemporalMotionWrites (1); // QVR: moving brush models' motion vectors (vr/vr_temporal.cpp)
		R_DrawSolidSplit (ents, count, texbegin, texend, program, flatprog);
	}
	else
	{
		R_AddBModelPassCallsSplit (ents, count, texbegin, texend, pass, program); // QVR: (C4v)
		VR_TemporalMotionWrites (pass <= BP_ALPHATEST && !translucent); // QVR: moving brush models' motion vectors (vr/vr_temporal.cpp)
		R_FlushBModelCalls ();
	}
	VR_TemporalMotionWrites (0); // QVR
	if (pass == BP_SOLID && !translucent)
		R_WorldFragmentsEnd (true); // QVR
	VR_GPUZoneEnd (); // QVR
	glDisable (GL_CLIP_DISTANCE1);
	if (a2c) // QVR
	{
		glDisable (GL_SAMPLE_ALPHA_TO_COVERAGE);
		glDisable (GL_SAMPLE_ALPHA_TO_ONE);
	}
}

/*
=============
R_AddBModelPassCalls

The draw calls of a pass over the entities (instances filled in bmodel_instances in this order).
=============
*/
static void R_AddBModelPassCalls (entity_t **ents, int count, textype_t texbegin, textype_t texend, brushpass_t pass)
{
	int i, j, baseinst;

	// generate drawcalls
	for (i = 0, baseinst = 0; i < count; /**/)
	{
		int numinst;
		entity_t *e = ents[i++];
		qmodel_t *model = e->model;
		qboolean isworld = (e == &cl_entities[0]);
		qboolean isstatic = PTR_IN_RANGE (e, cl_static_entities, cl_static_entities + MAX_STATIC_ENTITIES);
		qboolean zfix = !isworld && !isstatic;
		int frame = isworld ? 0 : e->frame;
		int numtex = model->texofs[texend] - model->texofs[texbegin];

		if (!numtex)
			continue;

		for (numinst = bmodel_portal_counts[i-1]; i < count && ents[i]->model == model && !ents[i]->frame == !frame && numinst + bmodel_portal_counts[i] <= MAX_BMODEL_INSTANCES; i++) // QVR: one frame a batch (a pressed button's alternate textures: docs/vr-port/ROUND21.md)
			numinst += (ents[i]->model->texofs[texend] - ents[i]->model->texofs[texbegin]) > 0 ? bmodel_portal_counts[i] : 0;

		for (j = model->texofs[texbegin]; j < model->texofs[texend]; j++)
		{
			texture_t *t = model->textures[model->usedtextures[j]];
			if (isworld && r_world_litsplit >= 0 && j < gl_bmodel_litcmd_count) // QVR: split by surface (C4v): its lit command too, or alone
			{
				if (bmodel_lit_mode != 2)
					R_AddBModelCall (model->firstcmd + j, baseinst, numinst, pass != BP_SHOWTRIS ? R_TextureAnimation (t, frame) : 0, zfix, e);
				if (bmodel_lit_mode != 1 && (!r_world_litskip || r_world_littex[j])) // (C4w: a lit command no sphere can fill: not drawn)
					R_AddBModelCall (gl_bmodel_litcmd_first + j, baseinst, numinst, pass != BP_SHOWTRIS ? R_TextureAnimation (t, frame) : 0, zfix, e);
				continue;
			}
			if (bmodel_lit_mode && (isworld ? bmodel_lit_mode == 2 : (bmodel_lit_mode == 2) != r_world_litbmodels)) // QVR: not in this part
				continue;
			R_AddBModelCall (model->firstcmd + j, baseinst, numinst, pass != BP_SHOWTRIS ? R_TextureAnimation (t, frame) : 0, zfix, e);
		}

		baseinst += numinst;
	}
} // QVR: R_AddBModelPassCalls

/*
=============
R_FlushLiquidMeshCalls

QVR: the world's liquids from the geometric waves' mesh (vr/vr_water.cpp, vr_water_geo_waves): its own vertex and
index buffers, a pin per vertex (attribute 4) for the vertex shaders' swells. The calls are Ironwail's (R_AddBModelCall),
their commands the ranges of the faces seen this view (VR_WaterMarkVis), not the GPU's culling's.
=============
*/
static bmodel_draw_indirect_t liquid_mesh_cmds[MAX_BMODEL_DRAWS];

static void R_FlushLiquidMeshCalls (void)
{
	GLuint	buf;
	GLbyte	*ofs;

	if (!num_bmodel_calls)
		return;

	GL_Upload (GL_DRAW_INDIRECT_BUFFER, liquid_mesh_cmds, sizeof (liquid_mesh_cmds[0]) * num_bmodel_calls, &buf, &ofs);
	GL_UseProgram (bmodel_batch_program);
	VR_WaterMeshBind ();
	GL_BindBuffer (GL_DRAW_INDIRECT_BUFFER, buf);
	if (gl_bindless_able)
	{
		GLuint pbuf;
		GLbyte *pofs;
		GL_Upload (GL_SHADER_STORAGE_BUFFER, bmodel_calls.bindless.params, sizeof (bmodel_calls.bindless.params[0]) * num_bmodel_calls, &pbuf, &pofs);
		GL_BindBufferRange (GL_SHADER_STORAGE_BUFFER, 1, pbuf, (GLintptr)pofs, sizeof (bmodel_calls.bindless.params[0]) * num_bmodel_calls);
		GL_MultiDrawElementsIndirectFunc (GL_TRIANGLES, GL_UNSIGNED_INT, (const void *)ofs, num_bmodel_calls, sizeof (bmodel_draw_indirect_t));
	}
	else
	{
		int i;
		GLuint pbuf;
		GLbyte *pofs;
		GL_Upload (GL_SHADER_STORAGE_BUFFER, &bmodel_calls.bound.params, sizeof (bmodel_calls.bound.params[0]) * num_bmodel_calls, &pbuf, &pofs);
		GL_BindBufferRange (GL_SHADER_STORAGE_BUFFER, 1, pbuf, (GLintptr)pofs, sizeof (bmodel_calls.bound.params[0]) * num_bmodel_calls);
		for (i = 0; i < num_bmodel_calls; i++)
		{
			GL_Uniform1iFunc (0, i);
			GL_BindTextures (0, 2, bmodel_calls.bound.textures[i]);
			GL_Bind (GL_TEXTURE3, bmodel_calls.bound.textures[i][2]);
			GL_Bind (GL_TEXTURE11, bmodel_calls.bound.textures[i][3]); // QVR: the specular map
			GL_DrawElementsIndirectFunc (GL_TRIANGLES, GL_UNSIGNED_INT, (const byte *)ofs + i * sizeof (bmodel_draw_indirect_t));
		}
	}

	num_bmodel_calls = 0;
}

/*
=============
R_DrawLiquidMesh

QVR: the world's liquids of this pass (translucent or not) from the geometric waves' mesh; baseinst: the world's instance.
=============
*/
static void R_DrawLiquidMesh (int baseinst, qboolean translucent, unsigned state)
{
	qmodel_t *model = cl.worldmodel;
	int j, k, n;

	GL_SetState ((state & ~GLS_MASK_ATTRIBS) | GLS_ATTRIBS(5));
	for (j = model->texofs[TEXTYPE_FIRSTLIQUID]; j < model->texofs[TEXTYPE_LASTLIQUID+1]; j++)
	{
		texture_t *t = model->textures[model->usedtextures[j]];
		const unsigned *ranges;
		if ((GL_WaterAlphaForEntityTextureType (&cl_entities[0], t->type) < 1.f) != translucent)
			continue;
		n = VR_WaterMeshRanges (model->usedtextures[j], &ranges);
		for (k = 0; k < n; k++)
		{
			bmodel_draw_indirect_t *cmd;
			if (num_bmodel_calls == MAX_BMODEL_DRAWS)
				R_FlushLiquidMeshCalls ();
			cmd = &liquid_mesh_cmds[num_bmodel_calls];
			cmd->firstIndex = ranges[k * 2];
			cmd->count = ranges[k * 2 + 1];
			cmd->instanceCount = 1;
			cmd->baseVertex = 0;
			// QVR: ES's field is reservedMustBeZero; the instance comes from the call there (no bindless)
			cmd->baseInstance = VR_GLESForm (VR_GLES_FORM_GLSL) ? 0 : baseinst;
			R_AddBModelCall (0, baseinst, 1, R_TextureAnimation (t, 0), false, &cl_entities[0]);
		}
	}
	R_FlushLiquidMeshCalls ();
	GL_SetState (state);
}

/*
=============
R_EntHasWater
=============
*/
static qboolean R_EntHasWater (entity_t *ent, qboolean translucent)
{
	int i;
	for (i = TEXTYPE_FIRSTLIQUID; i < TEXTYPE_LASTLIQUID+1; i++)
	{
		int numtex = ent->model->texofs[i+1] - ent->model->texofs[i];
		if (i == TEXTYPE_TELE && VR_PortalHideTeleport ()) // QVR: only the entrance overlays stars (views within views: theirs too)
			continue;
		if (numtex && (GL_WaterAlphaForEntityTextureType (ent, (textype_t)i) < 1.f) == translucent)
			return true;
	}
	return false;
}

/*
=============
R_DrawBrushModels_Water
=============
*/
void R_DrawBrushModels_Water (entity_t **ents, int count, qboolean translucent)
{
	int i, j;
	int totalinst, baseinst;
	unsigned state;
	GLuint buf, program;
	GLbyte *ofs;
	qboolean oit;
	GLuint scenedepth; // QVR
	int meshinst = -1; // QVR: the world's instance, its liquids drawn from the geometric waves' mesh (vr/vr_water.cpp)

	if (count > countof(bmodel_instances))
	{
		Con_DWarning ("bmodel instance overflow: %d > %d\n", count, (int)countof(bmodel_instances));
		count = countof(bmodel_instances);
	}

	// fill instance data
	for (i = 0, totalinst = 0; i < count; i++)
		if (R_EntHasWater (ents[i], translucent))
			R_InitBModelInstance (&bmodel_instances[totalinst++], ents[i]);

	if (!totalinst)
		return;

	GL_BeginGroup (translucent ? "Water (translucent)" : "Water (opaque)");

	// QVR: how far the opaque scene is, for the refraction not to read what is in front of the water and for the
	// shoreline foam (vr/vr_water.cpp; a small pass of its own: before this draw's state is set)
	scenedepth = VR_WaterSceneDepth (translucent);

	// setup state
	state = GLS_CULL_BACK | GLS_ATTRIBS(6); // QVR: swell pin and teleporter surface centre
	if (translucent)
		state |= GLS_BLEND_ALPHA_OIT | GLS_NO_ZWRITE;
	else
		state |= GLS_BLEND_OPAQUE;

	oit = translucent && R_GetEffectiveAlphaMode () == ALPHAMODE_OIT;
	if (cl.worldmodel->haslitwater && r_litwater.value)
		program = GL_WorldProgram (oit, q_max(0, (int)softemu - 1), WORLDSHADER_WATER);
	else
		program = GL_WaterProgram (oit, softemu == SOFTEMU_COARSE);

	R_ResetBModelCalls (program);
	GL_SetState (state);
	GL_Bind (GL_TEXTURE2, r_fullbright_cheatsafe ? greytexture : lightmap_texture);
	if (translucent) // QVR: refraction (vr/vr_water.cpp): the opaque scene (with MSAA resolved first)
		VR_BindOpaqueScene ();
	else
		GL_BindNative (GL_TEXTURE6, GL_TEXTURE_2D, 0);
	GL_BindNative (GL_TEXTURE8, GL_TEXTURE_2D, scenedepth); // QVR
	GL_BindNative (GL_TEXTURE16, GL_TEXTURE_CUBE_MAP, VR_WaterCubeTexture ()); // QVR: the room reflected (LiquidCube; vr/vr_envmap.cpp)
	GL_BindNative (GL_TEXTURE17, GL_TEXTURE_2D_ARRAY, VR_PortalTexture ()); // QVR: where a teleporter leads (PortalScene; vr/vr_portals.cpp)
	if (oit) // QVR: the OIT targets cleared at their first draw, after the passes above
		R_PrepareOITDraw (4);

	GL_Upload (GL_SHADER_STORAGE_BUFFER, bmodel_instances, sizeof(bmodel_instances[0]) * totalinst, &buf, &ofs);
	GL_BindBufferRange (GL_SHADER_STORAGE_BUFFER, 2, buf, (GLintptr)ofs, sizeof(bmodel_instances[0]) * count);

	// generate drawcalls
	for (i = 0, baseinst = 0; i < count; /**/)
	{
		int numinst;
		entity_t *e = ents[i++];
		qmodel_t *model = e->model;
		qboolean isworld = (e == &cl_entities[0]);
		int frame = isworld ? 0 : e->frame;

		if (!R_EntHasWater (e, translucent))
			continue;

		// QVR: one instance an entity here (no teleporter copies: bmodel_portal_counts is R_DrawBrushModels_Real's, stale
		// here; reading it made zero-instance calls on warden's lit water), one frame a batch
		for (numinst = 1; i < count && ents[i]->model == model && !ents[i]->frame == !frame && numinst < MAX_BMODEL_INSTANCES; i++)
			numinst += R_EntHasWater (ents[i], translucent);

		if (isworld && VR_WaterMeshActive ()) // QVR
		{
			meshinst = baseinst; // pools use the wave mesh; teleport surfaces always use their BSP vertices
		}

		for (j = model->texofs[TEXTYPE_FIRSTLIQUID]; j < model->texofs[TEXTYPE_LASTLIQUID+1]; j++)
		{
			texture_t *t = model->textures[model->usedtextures[j]];
			if ((isworld && VR_WaterMeshActive () && t->type != TEXTYPE_TELE) ||
				(t->type == TEXTYPE_TELE && VR_PortalHideTeleport ()))
				continue;
			if ((GL_WaterAlphaForEntityTextureType (e, t->type) < 1.f) != translucent)
				continue;
			R_AddBModelCall (model->firstcmd + j, baseinst, numinst, R_TextureAnimation (t, frame), !isworld, e);
		}

		baseinst += numinst;
	}

	VR_TemporalMotionWrites (!translucent); // QVR: opaque liquids flagged in the motion target (vr/vr_temporal.cpp)
	R_FlushBModelCalls ();

	if (meshinst >= 0) // QVR
		R_DrawLiquidMesh (meshinst, translucent, state);
	VR_TemporalMotionWrites (0); // QVR

	GL_EndGroup ();
}

/*
=============
R_GetBModelAlphaPasses
=============
*/
static uint32_t R_GetBModelAlphaPasses (const entity_t *ent)
{
	const qmodel_t *mod = ent->model;
	uint32_t mask = 0;

	if (mod->texofs[TEXTYPE_CUTOUT] != mod->texofs[TEXTYPE_DEFAULT])
		mask |= (1 << BP_SOLID);
	if (mod->texofs[TEXTYPE_SKY] != mod->texofs[TEXTYPE_CUTOUT])
		mask |= (1 << BP_ALPHATEST);

	return mask;
}

/*
=============
R_CanMergeBModelAlphaPasses
=============
*/
static qboolean R_CanMergeBModelAlphaPasses (uint32_t mask_a, uint32_t mask_b)
{
	COMPILE_TIME_ASSERT (check_bit_0, BP_SOLID == 0);
	COMPILE_TIME_ASSERT (check_bit_1, BP_ALPHATEST == 1);

	enum
	{
		#define ALLOW_MERGE(a, b) (1 << ((a)|((b)<<2)))

		MERGE_LUT =
			ALLOW_MERGE (0, 0) |
			ALLOW_MERGE (0, 1) |
			ALLOW_MERGE (0, 2) |
			ALLOW_MERGE (0, 3) |

			ALLOW_MERGE (1, 0) |
			ALLOW_MERGE (1, 1) |
			ALLOW_MERGE (1, 2) |
			ALLOW_MERGE (1, 3) |

			ALLOW_MERGE (2, 0) |
			ALLOW_MERGE (2, 2) |

			ALLOW_MERGE (3, 0) |
			ALLOW_MERGE (3, 2)
		,

		#undef ALLOW_MERGE
	};

	return (MERGE_LUT & (1 << (mask_a | (mask_b << 2)))) != 0;
}

/*
=============
R_DrawBrushModels
=============
*/
void R_DrawBrushModels (entity_t **ents, int count)
{
	qboolean translucent;
	if (!count)
		return;
	translucent = (ents[0] != &cl_entities[0]) && !ENTALPHA_OPAQUE (ents[0]->alpha);
	if (!translucent || R_GetEffectiveAlphaMode () == ALPHAMODE_OIT)
	{
		R_DrawBrushModels_Real (ents, count, BP_SOLID, translucent);
		R_DrawBrushModels_Real (ents, count, BP_ALPHATEST, translucent);
	}
	else
	{
		int i, j;
		for (i = 0; i < count; /**/)
		{
			uint32_t mask = R_GetBModelAlphaPasses (ents[i]);
			if (!mask)
			{
				i++;
				continue;
			}
			for (j = i + 1; j < count; j++)
			{
				uint32_t nextmask = R_GetBModelAlphaPasses (ents[j]);
				if (!R_CanMergeBModelAlphaPasses (mask, nextmask))
					break;
				mask |= nextmask;
			}
			if (mask & (1 << BP_SOLID))
				R_DrawBrushModels_Real (ents + i, j - i, BP_SOLID, true);
			if (mask & (1 << BP_ALPHATEST))
				R_DrawBrushModels_Real (ents + i, j - i, BP_ALPHATEST, true);
			i = j;
		}
	}
}

/*
=============
R_DrawBrushModels_SkyLayers
=============
*/
void R_DrawBrushModels_SkyLayers (entity_t **ents, int count)
{
	R_DrawBrushModels_Real (ents, count, BP_SKYLAYERS, false);
}

/*
=============
R_DrawBrushModels_SkyCubemap
=============
*/
void R_DrawBrushModels_SkyCubemap (entity_t **ents, int count)
{
	R_DrawBrushModels_Real (ents, count, BP_SKYCUBEMAP, false);
}

/*
=============
R_DrawBrushModels_SkyStencil
=============
*/
void R_DrawBrushModels_SkyStencil (entity_t **ents, int count)
{
	R_DrawBrushModels_Real (ents, count, BP_SKYSTENCIL, false);
}

/*
=============
R_DrawBrushModels_ShowTris
=============
*/
void R_DrawBrushModels_ShowTris (entity_t **ents, int count)
{
	R_DrawBrushModels_Real (ents, count, BP_SHOWTRIS, false);
}
