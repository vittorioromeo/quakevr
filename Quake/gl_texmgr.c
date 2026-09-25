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

//gl_texmgr.c -- fitzquake's texture manager. manages opengl texture images

#include "quakedef.h"
#include "glquake.h"
#include "vr/vr_api_render.h" // QVR

typedef struct {
	GLenum		id;
	int			ratio;
} glformat_t;

static const struct {
	glformat_t	solid, alpha;
} glformats[2] = {
	{{GL_RGB, 1},							{GL_RGBA, 1}},
	{{GL_COMPRESSED_RGBA_BPTC_UNORM, 4},	{GL_COMPRESSED_RGBA_BPTC_UNORM, 4}},
};

cvar_t			r_softemu = {"r_softemu", "0", CVAR_ARCHIVE};
cvar_t			r_softemu_metric = {"r_softemu_metric", "-1", CVAR_ARCHIVE};
cvar_t			r_softemu_lightmap_banding = {"r_softemu_lightmap_banding", "-1", CVAR_ARCHIVE};
cvar_t			r_softemu_mdl_warp = {"r_softemu_mdl_warp", "-1", CVAR_ARCHIVE};
cvar_t			r_softemu_dither_screen = {"r_softemu_dither_screen", "1.0", CVAR_ARCHIVE};
cvar_t			r_softemu_dither_texture = {"r_softemu_dither_texture", "1.0", CVAR_ARCHIVE};

static cvar_t	gl_max_size = {"gl_max_size", "0", CVAR_NONE};
static cvar_t	gl_picmip = {"gl_picmip", "0", CVAR_NONE};
cvar_t			gl_lodbias = {"gl_lodbias", "auto", CVAR_ARCHIVE };
cvar_t			gl_texturemode = {"gl_texturemode", "", CVAR_ARCHIVE};
cvar_t			gl_texture_anisotropy = {"gl_texture_anisotropy", "8", CVAR_ARCHIVE};
cvar_t			gl_compress_textures = {"gl_compress_textures", "0", CVAR_ARCHIVE};
GLint			gl_max_texture_size;

static float	lodbias;
softemu_t		softemu;

#define	MAX_GLTEXTURES	4096
static int numgltextures;
static gltexture_t	*active_gltextures, *free_gltextures;
gltexture_t		*notexture, *nulltexture, *whitetexture, *greytexture, *blacktexture;

// QVR: normal maps (TexMgr_LoadNormalMap), by the index of a texture in the array of them: a texture's normal map,
// and for a normal map what it is made from (and NORMALMAP_HEIGHTS) and the width of its texture in the world (for its depth).
static gltexture_t	*gltextures_base;
static gltexture_t	*normalmap_of[MAX_GLTEXTURES];
static byte			normalmap_kind[MAX_GLTEXTURES];
static unsigned short	normalmap_worldwidth[MAX_GLTEXTURES];
static gltexture_t	*flatnormaltexture;
#define NORMALMAP_MAXSIZE	256 // made ones: the bumps a dynamic light shows need no more (and a 512 texture's would take 1.4 MB)
#define NORMALMAP_TEXELS	2 // made ones: at most 2 texels a unit (a replacement's finer grain is noise as bumps)
#define NORMALMAP_DEPTH		4.f // units deep a texture's shading from black to white is, at vr_normalmap_strength 1
#define NORMALMAP_MEAN		0.35f // the brightness of a texture NORMALMAP_DEPTH is for (relative: dark ones twice as deep at most)
#define HEIGHT_LOW			0.02f // made heights (vr_parallax): the share of a texture's texels at the deepest,
#define HEIGHT_HIGH			0.98f // and below the surface's top
#define HEIGHT_MINRANGE		0.2f // a flatter texture's shading isn't stretched to the whole depth (its grain would)

unsigned int d_8to24table_opaque[256];			//standard palette with alpha 255 for all colors
unsigned int d_8to24table[256];					//standard palette, 255 is transparent
unsigned int d_8to24table_fbright[256];			//fullbright palette, 0-223 are black (for additive blending)
unsigned int d_8to24table_alphabright[256];		//palette with lighting mask in alpha channel (0=fullbright, 255=lit)
unsigned int d_8to24table_fbright_fence[256];	//fullbright palette, for fence textures
unsigned int d_8to24table_nobright[256];		//nobright palette, 224-255 are black (for additive blending)
unsigned int d_8to24table_nobright_fence[256];	//nobright palette, for fence textures
unsigned int d_8to24table_conchars[256];		//conchars palette, 0 and 255 are transparent

uint32_t is_fullbright[256/32];

static void GL_DeleteTexture (gltexture_t *texture);

/*
================================================================================

	COMMANDS

================================================================================
*/

const glmode_t glmodes[NUM_GLMODES] =
{
	{GL_NEAREST, GL_NEAREST,					"GL_NEAREST",					"Classic"},
	{GL_NEAREST, GL_NEAREST_MIPMAP_NEAREST,		"GL_NEAREST_MIPMAP_NEAREST",	"Classic"},
	{GL_NEAREST, GL_NEAREST_MIPMAP_LINEAR,		"GL_NEAREST_MIPMAP_LINEAR",		"Classic"},
	{GL_LINEAR,  GL_LINEAR,						"GL_LINEAR",					"Smooth"},
	{GL_LINEAR,  GL_LINEAR_MIPMAP_NEAREST,		"GL_LINEAR_MIPMAP_NEAREST",		"Smooth"},
	{GL_LINEAR,  GL_LINEAR_MIPMAP_LINEAR,		"GL_LINEAR_MIPMAP_LINEAR",		"Smooth"},
};
static int glmode_idx = 2; /* nearest with linear mips */

static GLuint gl_samplers[NUM_GLMODES * 2]; // x2: nomip + mip

texfilter_t gl_texfilter;
static int gl_texfilter_smooth; // QVR: VR_TextureSmoothing () as applied

#define GLMODE_SMOOTH 5 // QVR: GL_LINEAR_MIPMAP_LINEAR

/*
===============
TexMgr_FilterMode -- QVR: the filter mode of a texture. Replacement textures (textures/*.tga and the like, drawn
several texels to a Quake texel) and normal maps are smooth, linear with trilinear mipmaps (vr_texture_smooth 1),
or every mipmapped texture is (2); Quake's own follow gl_texturemode.
===============
*/
static int TexMgr_FilterMode (gltexture_t *glt)
{
	if (!(glt->flags & TEXPREF_MIPMAP) || gl_texfilter_smooth <= 0)
		return gl_texfilter.mode;
	if (gl_texfilter_smooth >= 2)
		return GLMODE_SMOOTH;
	if (glt->source_format == SRC_RGBA && glt->source_file[0] && !glt->source_offset) // an image file
		return GLMODE_SMOOTH;
	if (gltextures_base && normalmap_kind[glt - gltextures_base] != NORMALMAP_NONE)
		return GLMODE_SMOOTH;
	return gl_texfilter.mode;
}


/*
===============
TexMgr_DeleteSamplers
===============
*/
static void TexMgr_DeleteSamplers (void)
{
	GL_DeleteSamplersFunc (countof(gl_samplers), gl_samplers);
	memset (gl_samplers, 0, sizeof(gl_samplers));
}

/*
===============
TexMgr_CreateSamplers
===============
*/
static void TexMgr_CreateSamplers (void)
{
	int i;

	TexMgr_DeleteSamplers ();
	GL_GenSamplersFunc (countof(gl_samplers), gl_samplers);

	for (i = 0; i < NUM_GLMODES; i++)
	{
		GL_SamplerParameteriFunc (gl_samplers[i*2+0], GL_TEXTURE_MAG_FILTER, glmodes[i].magfilter);
		GL_SamplerParameteriFunc (gl_samplers[i*2+0], GL_TEXTURE_MIN_FILTER, glmodes[i].magfilter);
		GL_SamplerParameterfFunc (gl_samplers[i*2+0], GL_TEXTURE_MAX_ANISOTROPY_EXT, gl_texfilter.anisotropy);
		GL_SamplerParameterfFunc (gl_samplers[i*2+0], GL_TEXTURE_LOD_BIAS, gl_texfilter.lodbias);

		GL_SamplerParameteriFunc (gl_samplers[i*2+1], GL_TEXTURE_MAG_FILTER, glmodes[i].magfilter);
		GL_SamplerParameteriFunc (gl_samplers[i*2+1], GL_TEXTURE_MIN_FILTER, glmodes[i].minfilter);
		GL_SamplerParameterfFunc (gl_samplers[i*2+1], GL_TEXTURE_MAX_ANISOTROPY_EXT, gl_texfilter.anisotropy);
		GL_SamplerParameterfFunc (gl_samplers[i*2+1], GL_TEXTURE_LOD_BIAS, gl_texfilter.lodbias);
	}
}

/*
===============
TexMgr_DescribeTextureModes_f -- report available texturemodes
===============
*/
static void TexMgr_DescribeTextureModes_f (void)
{
	int i;
	for (i = 0; i < NUM_GLMODES; i++)
		Con_SafePrintf ("   %2i: %s\n", i + 1, glmodes[i].name);
	Con_Printf ("%i modes\n", i);
}

/*
===============
TexMgr_SetFilterModes
===============
*/
static void TexMgr_SetFilterModes (gltexture_t *glt)
{
	if (glt->bindless_handle)
	{
		int sampleridx;
		if (glt->flags & (TEXPREF_NEAREST|TEXPREF_LINEAR))
			return;

		GL_MakeTextureHandleNonResidentARBFunc (glt->bindless_handle);
		sampleridx = TexMgr_FilterMode (glt) * 2; // QVR
		if (glt->flags & TEXPREF_MIPMAP)
			sampleridx++;
		glt->bindless_handle = GL_GetTextureSamplerHandleARBFunc (glt->texnum, gl_samplers[sampleridx]);
		GL_MakeTextureHandleResidentARBFunc (glt->bindless_handle);

		return;
	}

	GL_Bind (GL_TEXTURE0, glt);

	if (glt->flags & TEXPREF_NEAREST)
	{
		glTexParameterf(glt->target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameterf(glt->target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	}
	else if (glt->flags & TEXPREF_LINEAR)
	{
		glTexParameterf(glt->target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameterf(glt->target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	}
	else if (glt->flags & TEXPREF_MIPMAP)
	{
		int mode = TexMgr_FilterMode (glt); // QVR
		glTexParameterf(glt->target, GL_TEXTURE_MAG_FILTER, glmodes[mode].magfilter);
		glTexParameterf(glt->target, GL_TEXTURE_MIN_FILTER, glmodes[mode].minfilter);
		glTexParameterf(glt->target, GL_TEXTURE_MAX_ANISOTROPY_EXT, gl_texfilter.anisotropy);
		glTexParameterf(glt->target, GL_TEXTURE_LOD_BIAS, gl_texfilter.lodbias);
	}
	else
	{
		glTexParameterf(glt->target, GL_TEXTURE_MAG_FILTER, glmodes[gl_texfilter.mode].magfilter);
		glTexParameterf(glt->target, GL_TEXTURE_MIN_FILTER, glmodes[gl_texfilter.mode].magfilter);
	}

	if (glt->flags & TEXPREF_CLAMP)
	{
		glTexParameteri(glt->target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(glt->target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}
}

/*
===============
TexMgr_TextureMode_Completion_f -- tab completion for gl_texturemode
===============
*/
static void TexMgr_TextureMode_Completion_f (cvar_t *cvar, const char *partial)
{
	int i;

	for (i = 0; i < NUM_GLMODES; i++)
		Con_AddToTabList (glmodes[i].name, partial, NULL);
}

/*
===============
TexMgr_TextureMode_f -- called when gl_texturemode changes
===============
*/
static void TexMgr_TextureMode_f (cvar_t *var)
{
	int i;

	for (i = 0; i < NUM_GLMODES; i++)
	{
		if (!Q_strcmp (glmodes[i].name, gl_texturemode.string))
		{
			glmode_idx = i;
			return;
		}
	}

	for (i = 0; i < NUM_GLMODES; i++)
	{
		if (!q_strcasecmp (glmodes[i].name, gl_texturemode.string))
		{
			Cvar_SetQuick (&gl_texturemode, glmodes[i].name);
			return;
		}
	}

	i = atoi(gl_texturemode.string);
	if (i >= 1 && i <= NUM_GLMODES)
	{
		Cvar_SetQuick (&gl_texturemode, glmodes[i-1].name);
		return;
	}

	Con_Printf ("\"%s\" is not a valid texturemode\n", gl_texturemode.string);
	Cvar_SetQuick (&gl_texturemode, glmodes[glmode_idx].name);
}

/*
===============
TexMgr_Anisotropy_f -- called when gl_texture_anisotropy changes
===============
*/
void TexMgr_Anisotropy_f (cvar_t *var)
{
	if (!host_initialized)
		return;

	if (gl_texture_anisotropy.value < 1)
	{
		Cvar_SetQuick (&gl_texture_anisotropy, "1");
	}
	else if (gl_texture_anisotropy.value > gl_max_anisotropy)
	{
		Cvar_SetValueQuick (&gl_texture_anisotropy, gl_max_anisotropy);
	}
}

/*
===============
TexMgr_SoftEmu_f -- called when r_softemu changes
===============
*/
static void TexMgr_SoftEmu_f (cvar_t *var)
{
	softemu = (int)r_softemu.value;
	softemu = CLAMP (0, (int)softemu, SOFTEMU_NUMMODES - 1);
}

/*
===============
TexMgr_LodBias_f -- called when gl_lodbias changes
===============
*/
static void TexMgr_LodBias_f (cvar_t *var)
{
	extern cvar_t vid_fsaa, vid_fsaamode;

	lodbias = var->value;

	if (!q_strcasecmp (var->string, "auto"))
		lodbias = Q_log2 (vid_fsaa.value * vid_fsaamode.value) / -2.f;
}

/*
===============
TexMgr_ForceFilterUpdate

Forces an update the next time TexMgr_ApplySettings is called
===============
*/
static void TexMgr_ForceFilterUpdate (void)
{
	gl_texfilter.mode = -1;
	gl_texfilter.anisotropy = -1.f;
}

/*
===============
TexMgr_UsesFilterOverride
===============
*/
qboolean TexMgr_UsesFilterOverride (void)
{
	return softemu >= SOFTEMU_COARSE || r_softemu_lightmap_banding.value > 0.f;
}

/*
===============
TexMgr_ApplySettings -- called at the beginning of each frame
===============
*/
void TexMgr_ApplySettings (void)
{
	texfilter_t prev = gl_texfilter;
	gltexture_t	*glt;

	int			prevsmooth = gl_texfilter_smooth; // QVR

	gl_texfilter.mode		= glmode_idx;
	gl_texfilter.anisotropy	= CLAMP (1.f, gl_texture_anisotropy.value, gl_max_anisotropy);
	gl_texfilter.lodbias	= lodbias;
	gl_texfilter_smooth		= TexMgr_UsesFilterOverride () ? 0 : VR_TextureSmoothing (); // QVR

	// if softemu is either 2 & 3 or r_softemu_lightmap_banding is > 0 we override the filtering mode, unless it's GL_NEAREST
	if (gl_texfilter.mode != 0 && TexMgr_UsesFilterOverride ())
	{
		const float SOFTEMU_ANISOTROPY = 8.f;
		gl_texfilter.mode = 2; // nearest with linear mips
		if (gl_texfilter.anisotropy < SOFTEMU_ANISOTROPY)
			gl_texfilter.anisotropy = q_min (SOFTEMU_ANISOTROPY, gl_max_anisotropy);
	}

	if (gl_texfilter.mode		== prev.mode &&
		gl_texfilter.anisotropy	== prev.anisotropy &&
		gl_texfilter.lodbias	== prev.lodbias &&
		gl_texfilter_smooth		== prevsmooth) // QVR
		return;

	if (gl_bindless_able)
		if (gl_texfilter.anisotropy	!= prev.anisotropy ||
			gl_texfilter.lodbias	!= prev.lodbias)
			TexMgr_CreateSamplers ();

	for (glt = active_gltextures; glt; glt = glt->next)
		TexMgr_SetFilterModes (glt);

	Sbar_Changed (); //sbar graphics need to be redrawn with new filter mode
}

/*
===============
TexMgr_Imagelist_Completion_f -- tab completion for imagelist/imagedump
===============
*/
static void TexMgr_Imagelist_Completion_f (const char *partial)
{
	gltexture_t	*glt;

	for (glt = active_gltextures; glt; glt = glt->next)
		Con_AddToTabList (glt->name, partial, NULL);
}

/*
===============
TexMgr_Imagelist_f -- report loaded textures
===============
*/
static void TexMgr_Imagelist_f (void)
{
	double bytes = 0;
	double texels = 0;
	int count = 0;
	const char *filter = NULL;
	gltexture_t	*glt;

	if (Cmd_Argc () >= 2)
		filter = Cmd_Argv (1);

	for (glt = active_gltextures; glt; glt = glt->next)
	{
		char buf[MAX_QPATH];
		char mip = glt->flags & TEXPREF_MIPMAP ? 'm' : ' ';
		char comp = glt->compression > 1 ? 'c' : ' ';
		unsigned int layers = glt->flags & TEXPREF_CUBEMAP ? glt->depth * 6 : glt->depth;
		unsigned int s = glt->width * glt->height * layers;

		if (filter)
		{
			if (!q_strcasestr (glt->name, filter))
				continue;
			COM_TintSubstring (glt->name, filter, buf, sizeof (buf));
		}
		else
		{
			q_strlcpy (buf, glt->name, sizeof (buf));
		}

		if (layers > 1)
			Con_SafePrintf ("%3i x %4i x %4i %c%c %s\n", layers, glt->width, glt->height, comp, mip, buf);
		else
			Con_SafePrintf ("      %4i x %4i %c%c %s\n", glt->width, glt->height, comp, mip, buf);

		if (glt->flags & TEXPREF_MIPMAP)
			s = (s * 4 + 3) / 3;
		texels += s;
		bytes += s * 4 / glt->compression;
		count++;
	}

	if (filter)
		Con_Printf ("%i/%i textures containing '%s': %.1lf mpixels %1.1lf megabytes\n",
			count, numgltextures, filter, texels * 1e-6, bytes / 0x100000);
	else
		Con_Printf ("%i textures %.1lf mpixels %1.1lf megabytes\n",
			numgltextures, texels * 1e-6, bytes / 0x100000);
}

/*
===============
TexMgr_Imagedump_f -- dump all current textures to TGA files
===============
*/
static void TexMgr_Imagedump_f (void)
{
	char tganame[MAX_OSPATH], tempname[MAX_OSPATH], dirname[MAX_OSPATH];
	const char *reldirname = "imagedump";
	const char *filter = NULL;
	int count = 0;
	gltexture_t	*glt;
	byte *buffer;
	char *c;

	if (Cmd_Argc () >= 2)
		filter = Cmd_Argv (1);

	q_snprintf(dirname, sizeof(dirname), "%s/%s", com_gamedir, reldirname);

	glPixelStorei (GL_PACK_ALIGNMENT, 1);/* for widths that aren't a multiple of 4 */

	//loop through textures
	for (glt = active_gltextures; glt; glt = glt->next)
	{
		int channels = (glt->flags & TEXPREF_HASALPHA) ? 4 : 3;
		int format   = (glt->flags & TEXPREF_HASALPHA) ? GL_RGBA : GL_RGB;

		if (filter && !q_strcasestr (glt->name, filter))
			continue;

		q_strlcpy (tempname, glt->name, sizeof(tempname));
		for (c = tempname; *c; ++c)
			if (*c == ':' || *c == '/' || *c == '*')
				*c = '_';

		GL_Bind (GL_TEXTURE0, glt);
		buffer = (byte *) malloc(glt->width * glt->height * glt->depth * channels);
		if (!buffer)
			Sys_Error ("TexMgr_Imagedump_f: out of memory (%dx%dx%d %d bpp)", glt->width, glt->height, glt->depth, channels * 8);

		if (glt->flags & TEXPREF_CUBEMAP)
		{
			const char *suf[6] = {"ft", "bk", "up", "dn", "rt", "lf"};
			int i;
			for (i = 0; i < 6; i++)
			{
				q_snprintf(tganame, sizeof(tganame), "imagedump/%s%s.tga", tempname, suf[i]);
				glGetTexImage(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, format, GL_UNSIGNED_BYTE, buffer);
				Image_WriteTGA (tganame, buffer, glt->width, glt->height*glt->depth, channels*8, true);
			}
		}
		else
		{
			q_snprintf(tganame, sizeof(tganame), "imagedump/%s.tga", tempname);
			glGetTexImage(glt->target, 0, format, GL_UNSIGNED_BYTE, buffer);
			Image_WriteTGA (tganame, buffer, glt->width, glt->height*glt->depth, channels*8, true);
		}

		free (buffer);
		count++;
	}

	if (filter)
		Con_SafePrintf ("dumped %i textures containing '%s' to ", count, filter);
	else
		Con_SafePrintf ("dumped %i textures to ", count);
	Con_LinkPrintf (va ("%s/", dirname), "%s", reldirname);
	Con_SafePrintf (".\n");
}

/*
===============
TexMgr_FrameUsage -- report texture memory usage for this frame
===============
*/
float TexMgr_FrameUsage (void)
{
	float mb;
	float texels = 0;
	gltexture_t	*glt;

	for (glt = active_gltextures; glt; glt = glt->next)
	{
		if (glt->visframe == r_framecount)
		{
			int faces = glt->flags & TEXPREF_CUBEMAP ? 6 : 1;
			if (glt->flags & TEXPREF_MIPMAP)
				texels += glt->width * glt->height * glt->depth * faces * (4.0f / 3.0f);
			else
				texels += (glt->width * glt->height * glt->depth * faces);
		}
	}

	mb = texels * 4.0f / 0x100000;
	return mb;
}

/*
===============
TexMgr_CanCompress
===============
*/
static qboolean TexMgr_CanCompress (gltexture_t *glt)
{
	return glt->source_format != SRC_LIGHTMAP && (glt->flags & TEXPREF_UNCOMPRESSED) == 0;
}

/*
===============
TexMgr_CompressTextures_f -- called when gl_compress_textures changes
===============
*/
void TexMgr_CompressTextures_f (cvar_t *var)
{
	gltexture_t	*glt;

	Con_SafePrintf ("Using %s textures\n", var->value ? "compressed" : "uncompressed");

	// In an attempt to reduce VRAM fragmentation, instead of unloading and reloading
	// each texture sequentially, we first unload them all, then reload them
	for (glt = active_gltextures; glt; glt = glt->next)
		if (TexMgr_CanCompress (glt))
			GL_DeleteTexture (glt);

	for (glt = active_gltextures; glt; glt = glt->next)
		if (TexMgr_CanCompress (glt))
			TexMgr_ReloadImage (glt, -1, -1);
}

/*
================================================================================

	TEXTURE MANAGER

================================================================================
*/

/*
================
TexMgr_FindTexture
================
*/
gltexture_t *TexMgr_FindTexture (qmodel_t *owner, const char *name)
{
	gltexture_t	*glt;

	if (name)
	{
		for (glt = active_gltextures; glt; glt = glt->next)
		{
			if (glt->owner == owner && !strcmp (glt->name, name))
				return glt;
		}
	}

	return NULL;
}

/*
================
TexMgr_NewTexture
================
*/
gltexture_t *TexMgr_NewTexture (void)
{
	gltexture_t *glt;

	if (numgltextures == MAX_GLTEXTURES)
		Sys_Error("numgltextures == MAX_GLTEXTURES\n");

	glt = free_gltextures;
	free_gltextures = glt->next;
	glt->next = active_gltextures;
	active_gltextures = glt;
	normalmap_of[glt - gltextures_base] = NULL; // QVR
	normalmap_kind[glt - gltextures_base] = NORMALMAP_NONE;

	glGenTextures(1, &glt->texnum);
	numgltextures++;
	return glt;
}

//ericw -- workaround for preventing TexMgr_FreeTexture during TexMgr_ReloadImages
static qboolean in_reload_images;

/*
================
TexMgr_FreeTexture
================
*/
void TexMgr_FreeTexture (gltexture_t *kill)
{
	gltexture_t *glt;

	if (in_reload_images)
		return;

	if (kill == NULL)
	{
		Con_Printf ("TexMgr_FreeTexture: NULL texture\n");
		return;
	}

	// QVR: its normal map is freed with it (they have the same owner)
	normalmap_of[kill - gltextures_base] = NULL;
	normalmap_kind[kill - gltextures_base] = NORMALMAP_NONE;

	if (active_gltextures == kill)
	{
		active_gltextures = kill->next;
		kill->next = free_gltextures;
		free_gltextures = kill;

		GL_DeleteTexture(kill);
		numgltextures--;
		return;
	}

	for (glt = active_gltextures; glt; glt = glt->next)
	{
		if (glt->next == kill)
		{
			glt->next = kill->next;
			kill->next = free_gltextures;
			free_gltextures = kill;

			GL_DeleteTexture(kill);
			numgltextures--;
			return;
		}
	}

	Con_Printf ("TexMgr_FreeTexture: not found\n");
}

/*
================
TexMgr_FreeTextures

compares each bit in "flags" to the one in glt->flags only if that bit is active in "mask"
================
*/
void TexMgr_FreeTextures (unsigned int flags, unsigned int mask)
{
	gltexture_t *glt, *next;

	for (glt = active_gltextures; glt; glt = next)
	{
		next = glt->next;
		if ((glt->flags & mask) == (flags & mask))
			TexMgr_FreeTexture (glt);
	}
}

/*
================
TexMgr_FreeTexturesForOwner
================
*/
void TexMgr_FreeTexturesForOwner (qmodel_t *owner)
{
	gltexture_t *glt, *next;

	for (glt = active_gltextures; glt; glt = next)
	{
		next = glt->next;
		if (glt && glt->owner == owner)
			TexMgr_FreeTexture (glt);
	}
}

/*
================
TexMgr_DeleteTextureObjects
================
*/
void TexMgr_DeleteTextureObjects (void)
{
	gltexture_t *glt;

	for (glt = active_gltextures; glt; glt = glt->next)
	{
		GL_DeleteTexture (glt);
	}
	TexMgr_DeleteSamplers ();
}

/*
================================================================================

	INIT

================================================================================
*/

static void SetColor (uint32_t *dst, byte r, byte g, byte b, byte a)
{
	((byte*)dst)[0] = r;
	((byte*)dst)[1] = g;
	((byte*)dst)[2] = b;
	((byte*)dst)[3] = a;
}

/*
=================
TexMgr_LoadPalette -- johnfitz -- was VID_SetPalette, moved here, renamed, rewritten
=================
*/
void TexMgr_LoadPalette (void)
{
	byte *pal, *src, *colormap;
	int i, j, mark, numfb;
	FILE *f;

	COM_FOpenFile ("gfx/palette.lmp", &f, NULL);
	if (!f)
		Sys_Error ("Couldn't load gfx/palette.lmp");

	mark = Hunk_LowMark ();
	pal = (byte *) Hunk_AllocNoFill (768);
	if (fread (pal, 768, 1, f) != 1)
		Sys_Error ("Failed reading gfx/palette.lmp");
	fclose(f);

	COM_FOpenFile ("gfx/colormap.lmp", &f, NULL);
	if (!f)
		Sys_Error ("Couldn't load gfx/colormap.lmp");
	colormap = (byte *) Hunk_AllocNoFill (256 * 64);
	if (fread (colormap, 256 * 64, 1, f) != 1)
		Sys_Error ("TexMgr_LoadPalette: colormap read error");
	fclose(f);

	//find fullbright colors
	memset (is_fullbright, 0, sizeof (is_fullbright));
	numfb = 0;
	src = pal;
	for (i = 0; i < 256; i++, src += 3)
	{
		if (!src[0] && !src[1] && !src[2])
			continue; // black can't be fullbright

		for (j = 1; j < 64; j++)
			if (colormap[i + j * 256] != colormap[i])
				break;

		if (j == 64) 
		{
			SetBit (is_fullbright, i);
			numfb++;
		}
	}
	if (developer.value)
		Con_SafePrintf ("Colormap has %d fullbright colors\n", numfb);

	//fill color tables
	src = pal;
	for (i = 0; i < 256; i++, src += 3)
	{
		SetColor (&d_8to24table_opaque[i], src[0], src[1], src[2], 255);
		if (GetBit (is_fullbright, i))
		{
			SetColor (&d_8to24table_alphabright[i],	src[0], src[1], src[2], 0);
			SetColor (&d_8to24table_fbright[i],		src[0], src[1], src[2], 255);
			SetColor (&d_8to24table_nobright[i],	0, 0, 0, 255);
		}
		else
		{
			SetColor (&d_8to24table_alphabright[i],	src[0], src[1], src[2], 255);
			SetColor (&d_8to24table_fbright[i],		0, 0, 0, 255);
			SetColor (&d_8to24table_nobright[i],	src[0], src[1], src[2], 255);
		}
	}

	memcpy(d_8to24table, d_8to24table_opaque, 256*4);
	((byte *) &d_8to24table[255]) [3] = 0; //standard palette, 255 is transparent

	//fullbright palette, for fence textures
	memcpy(d_8to24table_fbright_fence, d_8to24table_fbright, 256*4);
	d_8to24table_fbright_fence[255] = 0; // Alpha of zero.

	//nobright palette, for fence textures
	memcpy(d_8to24table_nobright_fence, d_8to24table_nobright, 256*4);
	d_8to24table_nobright_fence[255] = 0; // Alpha of zero.

	//conchars palette, 0 and 255 are transparent
	memcpy(d_8to24table_conchars, d_8to24table, 256*4);
	((byte *) &d_8to24table_conchars[0]) [3] = 0;

	Hunk_FreeToLowMark (mark);
}

/*
================
TexMgr_NewGame
================
*/
void TexMgr_NewGame (void)
{
	TexMgr_FreeTextures (0, TEXPREF_PERSIST); //deletes all textures where TEXPREF_PERSIST is unset
	TexMgr_LoadPalette ();
}


/*
================
TexMgr_Init

must be called before any texture loading
================
*/
void TexMgr_Init (void)
{
	int i;
	static byte notexture_data[16] = {159,91,83,255,0,0,0,255,0,0,0,255,159,91,83,255}; //black and pink checker
	static byte nulltexture_data[16] = {127,191,255,255,0,0,0,255,0,0,0,255,127,191,255,255}; //black and blue checker
	static byte whitetexture_data[16] = {255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255}; //white
	static byte greytexture_data[16] = {127,127,127,255,127,127,127,255,127,127,127,255,127,127,127,255}; //50% grey
	static byte blacktexture_data[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}; //black
	static byte flatnormal_data[16] = {128,128,255,255,128,128,255,255,128,128,255,255,128,128,255,255}; // QVR: a flat normal map
	extern texture_t *r_notexture_mip, *r_notexture_mip2;
	cmd_function_t	*cmd;

	// init texture list
	free_gltextures = (gltexture_t *) Hunk_AllocName (MAX_GLTEXTURES * sizeof(gltexture_t), "gltextures");
	gltextures_base = free_gltextures; // QVR
	active_gltextures = NULL;
	for (i = 0; i < MAX_GLTEXTURES - 1; i++)
		free_gltextures[i].next = &free_gltextures[i+1];
	free_gltextures[i].next = NULL;
	numgltextures = 0;

	// init texture filter
	TexMgr_ForceFilterUpdate ();
	TexMgr_ApplySettings ();

	// palette
	TexMgr_LoadPalette ();

	Cvar_RegisterVariable (&gl_max_size);
	Cvar_RegisterVariable (&gl_picmip);
	gl_texturemode.string = glmodes[glmode_idx].name;
	Cvar_RegisterVariable (&gl_texturemode);
	Cvar_SetCallback (&gl_texturemode, &TexMgr_TextureMode_f);
	Cvar_SetCompletion (&gl_texturemode, &TexMgr_TextureMode_Completion_f);
	Cvar_RegisterVariable (&gl_lodbias);
	Cvar_SetCallback (&gl_lodbias, TexMgr_LodBias_f);
	Cvar_RegisterVariable (&r_softemu);
	Cvar_SetCallback (&r_softemu, TexMgr_SoftEmu_f);
	Cvar_RegisterVariable (&r_softemu_lightmap_banding);
	Cvar_SetCallback (&r_softemu_lightmap_banding, TexMgr_SoftEmu_f);
	Cvar_RegisterVariable (&r_softemu_mdl_warp);
	Cvar_RegisterVariable (&r_softemu_dither_screen);
	Cvar_RegisterVariable (&r_softemu_dither_texture);
	Cmd_AddCommand ("gl_describetexturemodes", &TexMgr_DescribeTextureModes_f);
	cmd = Cmd_AddCommand ("imagelist", &TexMgr_Imagelist_f);
	if (cmd)
		cmd->completion = TexMgr_Imagelist_Completion_f;
	cmd = Cmd_AddCommand ("imagedump", &TexMgr_Imagedump_f);
	if (cmd)
		cmd->completion = TexMgr_Imagelist_Completion_f;

	// poll max size from hardware
	glGetIntegerv (GL_MAX_TEXTURE_SIZE, &gl_max_texture_size);

	// load notexture images
	notexture = TexMgr_LoadImage (NULL, "notexture", 2, 2, SRC_RGBA, notexture_data, "", (src_offset_t)notexture_data, TEXPREF_NEAREST | TEXPREF_PERSIST | TEXPREF_NOPICMIP | TEXPREF_BINDLESS);
	nulltexture = TexMgr_LoadImage (NULL, "nulltexture", 2, 2, SRC_RGBA, nulltexture_data, "", (src_offset_t)nulltexture_data, TEXPREF_NEAREST | TEXPREF_PERSIST | TEXPREF_NOPICMIP | TEXPREF_BINDLESS);
	whitetexture = TexMgr_LoadImage (NULL, "whitetexture", 2, 2, SRC_RGBA, whitetexture_data, "", (src_offset_t)whitetexture_data, TEXPREF_NEAREST | TEXPREF_PERSIST | TEXPREF_NOPICMIP | TEXPREF_BINDLESS);
	greytexture = TexMgr_LoadImage (NULL, "greytexture", 2, 2, SRC_RGBA, greytexture_data, "", (src_offset_t)greytexture_data, TEXPREF_NEAREST | TEXPREF_PERSIST | TEXPREF_NOPICMIP | TEXPREF_BINDLESS);
	blacktexture = TexMgr_LoadImage (NULL, "blacktexture", 2, 2, SRC_RGBA, blacktexture_data, "", (src_offset_t)blacktexture_data, TEXPREF_NEAREST | TEXPREF_PERSIST | TEXPREF_NOPICMIP | TEXPREF_BINDLESS);
	flatnormaltexture = TexMgr_LoadImage (NULL, "flatnormaltexture", 2, 2, SRC_RGBA, flatnormal_data, "", (src_offset_t)flatnormal_data, TEXPREF_NEAREST | TEXPREF_PERSIST | TEXPREF_NOPICMIP | TEXPREF_BINDLESS | TEXPREF_UNCOMPRESSED); // QVR

	//have to assign these here becuase Mod_Init is called before TexMgr_Init
	r_notexture_mip->gltexture = r_notexture_mip2->gltexture = notexture;
}

/*
================================================================================

	IMAGE LOADING

================================================================================
*/

/*
================
TexMgr_Pad -- return smallest power of two greater than or equal to s
================
*/
int TexMgr_Pad (int s)
{
	int i;
	for (i = 1; i < s; i<<=1)
		;
	return i;
}

/*
===============
TexMgr_SafeTextureSize -- return a size with hardware and user prefs in mind
===============
*/
int TexMgr_SafeTextureSize (int s)
{
	int p = (int)gl_max_size.value;
	if (p > 0) {
		p = TexMgr_Pad(p);
		if (p < s) s = p;
	}
	s = CLAMP(1, s, gl_max_texture_size);
	return s;
}

/*
================
TexMgr_PadConditional -- only pad if a texture of that size would be padded. (used for tex coords)
================
*/
int TexMgr_PadConditional (int s)
{
	if (s < TexMgr_SafeTextureSize(s))
		return TexMgr_Pad(s);
	return s;
}

/*
================
TexMgr_MipMapW
================
*/
static unsigned *TexMgr_MipMapW (unsigned *data, int width, int height, int depth)
{
	int	i, size;
	byte	*out, *in;

	if (!data)
		return NULL;

	out = in = (byte *)data;
	size = ((width*height)>>1)*depth;

#ifdef USE_SSE2
	while (size >= 4)
	{
		__m128i v0, v1, v2, v3;

		v0 = _mm_loadu_si128 ((const __m128i *)in);
		v1 = _mm_loadu_si128 ((const __m128i *)in + 1);
		v0 = _mm_shuffle_epi32 (v0, _MM_SHUFFLE (3, 1, 2, 0));
		v1 = _mm_shuffle_epi32 (v1, _MM_SHUFFLE (3, 1, 2, 0));
		v2 = _mm_unpacklo_epi64 (v0, v1);
		v3 = _mm_unpackhi_epi64 (v0, v1);
		v0 = _mm_avg_epu8 (v2, v3);
		_mm_storeu_si128 ((__m128i *)out, v0);

		size -= 4;
		in += 32;
		out += 16;
	}
#endif

	for (i = 0; i < size; i++, out += 4, in += 8)
	{
		out[0] = (in[0] + in[4] + 1)>>1;
		out[1] = (in[1] + in[5] + 1)>>1;
		out[2] = (in[2] + in[6] + 1)>>1;
		out[3] = (in[3] + in[7] + 1)>>1;
	}

	return data;
}

/*
================
TexMgr_MipMapH
================
*/
static unsigned *TexMgr_MipMapH (unsigned *data, int width, int height, int depth)
{
	int	i, j;
	byte	*out, *in;

	if (!data)
		return NULL;

	out = in = (byte *)data;
	height>>=1;
	height*=depth;
	width<<=2;

	for (i = 0; i < height; i++, in += width)
	{
		j = 0;
#ifdef USE_SSE2
		while (j + 16 <= width)
		{
			__m128i v0, v1;

			v0 = _mm_loadu_si128 ((const __m128i *)in);
			v1 = _mm_loadu_si128 ((const __m128i *)(in + width));
			v0 = _mm_avg_epu8 (v0, v1);
			_mm_storeu_si128 ((__m128i *)out, v0);

			j += 16;
			in += 16;
			out += 16;
		}
#endif
		for (; j < width; j += 4, out += 4, in += 4)
		{
			out[0] = (in[0] + in[width+0] + 1)>>1;
			out[1] = (in[1] + in[width+1] + 1)>>1;
			out[2] = (in[2] + in[width+2] + 1)>>1;
			out[3] = (in[3] + in[width+3] + 1)>>1;
		}
	}

	return data;
}

/*
===============
TexMgr_AlphaEdgeFix

eliminate pink edges on sprites, etc.
operates in place on 32bit data
===============
*/
static void TexMgr_AlphaEdgeFix (byte *data, int width, int height)
{
	int	i, j, n = 0, b, c[3] = {0,0,0},
		lastrow, thisrow, nextrow,
		lastpix, thispix, nextpix;
	byte	*dest = data;

	if (!data)
		return;

	for (i = 0; i < height; i++)
	{
		lastrow = width * 4 * ((i == 0) ? height-1 : i-1);
		thisrow = width * 4 * i;
		nextrow = width * 4 * ((i == height-1) ? 0 : i+1);

		for (j = 0; j < width; j++, dest += 4)
		{
			if (dest[3]) //not transparent
				continue;

			lastpix = 4 * ((j == 0) ? width-1 : j-1);
			thispix = 4 * j;
			nextpix = 4 * ((j == width-1) ? 0 : j+1);

			b = lastrow + lastpix; if (data[b+3]) {c[0] += data[b]; c[1] += data[b+1]; c[2] += data[b+2]; n++;}
			b = thisrow + lastpix; if (data[b+3]) {c[0] += data[b]; c[1] += data[b+1]; c[2] += data[b+2]; n++;}
			b = nextrow + lastpix; if (data[b+3]) {c[0] += data[b]; c[1] += data[b+1]; c[2] += data[b+2]; n++;}
			b = lastrow + thispix; if (data[b+3]) {c[0] += data[b]; c[1] += data[b+1]; c[2] += data[b+2]; n++;}
			b = nextrow + thispix; if (data[b+3]) {c[0] += data[b]; c[1] += data[b+1]; c[2] += data[b+2]; n++;}
			b = lastrow + nextpix; if (data[b+3]) {c[0] += data[b]; c[1] += data[b+1]; c[2] += data[b+2]; n++;}
			b = thisrow + nextpix; if (data[b+3]) {c[0] += data[b]; c[1] += data[b+1]; c[2] += data[b+2]; n++;}
			b = nextrow + nextpix; if (data[b+3]) {c[0] += data[b]; c[1] += data[b+1]; c[2] += data[b+2]; n++;}

			//average all non-transparent neighbors
			if (n)
			{
				dest[0] = (byte)(c[0]/n);
				dest[1] = (byte)(c[1]/n);
				dest[2] = (byte)(c[2]/n);

				n = c[0] = c[1] = c[2] = 0;
			}
		}
	}
}

/*
===============
TexMgr_PadEdgeFixW -- special case of AlphaEdgeFix for textures that only need it because they were padded

operates in place on 32bit data, and expects unpadded height and width values
===============
*/
static void TexMgr_PadEdgeFixW (byte *data, int width, int height)
{
	byte *src, *dst;
	int i, padw, padh;

	padw = TexMgr_PadConditional(width);
	padh = TexMgr_PadConditional(height);

	//copy last full column to first empty column, leaving alpha byte at zero
	src = data + (width - 1) * 4;
	for (i = 0; i < padh; i++)
	{
		src[4] = src[0];
		src[5] = src[1];
		src[6] = src[2];
		src += padw * 4;
	}

	//copy first full column to last empty column, leaving alpha byte at zero
	src = data;
	dst = data + (padw - 1) * 4;
	for (i = 0; i < padh; i++)
	{
		dst[0] = src[0];
		dst[1] = src[1];
		dst[2] = src[2];
		src += padw * 4;
		dst += padw * 4;
	}
}

/*
===============
TexMgr_PadEdgeFixH -- special case of AlphaEdgeFix for textures that only need it because they were padded

operates in place on 32bit data, and expects unpadded height and width values
===============
*/
static void TexMgr_PadEdgeFixH (byte *data, int width, int height)
{
	byte *src, *dst;
	int i, padw, padh;

	padw = TexMgr_PadConditional(width);
	padh = TexMgr_PadConditional(height);

	//copy last full row to first empty row, leaving alpha byte at zero
	dst = data + height * padw * 4;
	src = dst - padw * 4;
	for (i = 0; i < padw; i++)
	{
		dst[0] = src[0];
		dst[1] = src[1];
		dst[2] = src[2];
		src += 4;
		dst += 4;
	}

	//copy first full row to last empty row, leaving alpha byte at zero
	dst = data + (padh - 1) * padw * 4;
	src = data;
	for (i = 0; i < padw; i++)
	{
		dst[0] = src[0];
		dst[1] = src[1];
		dst[2] = src[2];
		src += 4;
		dst += 4;
	}
}

/*
================
TexMgr_8to32
================
*/
static unsigned *TexMgr_8to32 (byte *in, int pixels, unsigned int *usepal)
{
	int i;
	unsigned *out, *data;

	out = data = (unsigned *) Hunk_AllocNoFill (pixels*4);

	for (i = 0; i < pixels; i++)
		*out++ = usepal[*in++];

	return data;
}

/*
================
TexMgr_PadImageW -- return image with width padded up to power-of-two dimentions
================
*/
static byte *TexMgr_PadImageW (byte *in, int width, int height, byte padbyte)
{
	int i, j, outwidth;
	byte *out, *data;

	if (width == TexMgr_Pad(width))
		return in;

	outwidth = TexMgr_Pad(width);

	out = data = (byte *) Hunk_AllocNoFill(outwidth*height);

	for (i = 0; i < height; i++)
	{
		for (j = 0; j < width; j++)
			*out++ = *in++;
		for (  ; j < outwidth; j++)
			*out++ = padbyte;
	}

	return data;
}

/*
================
TexMgr_PadImageH -- return image with height padded up to power-of-two dimentions
================
*/
static byte *TexMgr_PadImageH (byte *in, int width, int height, byte padbyte)
{
	int i, srcpix, dstpix;
	byte *data, *out;

	if (height == TexMgr_Pad(height))
		return in;

	srcpix = width * height;
	dstpix = width * TexMgr_Pad(height);

	out = data = (byte *) Hunk_AllocNoFill(dstpix);

	for (i = 0; i < srcpix; i++)
		*out++ = *in++;
	for (     ; i < dstpix; i++)
		*out++ = padbyte;

	return data;
}

/*
================
GL_TexImage -- calls glTexImage2D/3D based on texture type
================
*/
static void GL_TexImage (gltexture_t *glt, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels)
{
	const GLvoid **images = (const GLvoid **)pixels; // for arrays/cubemaps "pixels" is actually an array of pointers
	unsigned int i;

	switch (glt->target)
	{
	case GL_TEXTURE_2D_ARRAY:
		GL_TexImage3DFunc (glt->target, level, internalformat, width, height, glt->depth, 0, format, type, NULL);
		for (i = 0; i < glt->depth; i++)
			GL_TexSubImage3DFunc (glt->target, level, 0, 0, i, width, height, 1, format, type, images ? images[i] : NULL);
		break;

	case GL_TEXTURE_CUBE_MAP:
		for (i = 0; i < 6; i++)
			glTexImage2D (GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, level, internalformat, width, height, 0, format, type, images ? images[i] : NULL);
		break;

	case GL_TEXTURE_2D:
		glTexImage2D (glt->target, level, internalformat, width, height, 0, format, type, pixels);
		break;

	default:
		Sys_Error ("GL_TexImage: unknown target %d for %s", glt->target, glt->name);
		break;
	}
}

/*
================
TexMgr_ShadingToNormals -- QVR: a texture's shading made a normal map (DarkPlaces' r_shadow_bumpscale_basetexture):
its luminance taken for height, bumps from its Sobel gradient; `scale` is how deep a step from black to white is, in
texels, for a texture of average brightness (NORMALMAP_MEAN; darker ones deeper). The texture tiles: the edges wrap
round. Tangent space: x along the texture's s, y up its rows (green up). With `heights`, alpha is the height parallax
mapping walks (TexMgr_ShadingToHeights), `texelsperunit` how fine the texture is in the world; else 255.
================
*/
static void TexMgr_ShadingToHeights (const float *lum, float *h, int width, int height, float texelsperunit);

static void TexMgr_ShadingToNormals (byte *data, int width, int height, float scale, qboolean heights, float texelsperunit)
{
	int		x, y, mark;
	float	*lum, *h = NULL, mean = 0.f;

	if (width < 1 || height < 1)
		return;
	mark = Hunk_LowMark ();
	lum = (float *) Hunk_AllocNoFill (width * height * sizeof (float));
	for (x = 0; x < width * height; x++)
	{
		lum[x] = (data[x*4+0] * 0.299f + data[x*4+1] * 0.587f + data[x*4+2] * 0.114f) * (1.f / 255.f);
		mean += lum[x];
	}
	if (heights)
	{
		h = (float *) Hunk_AllocNoFill (width * height * sizeof (float));
		TexMgr_ShadingToHeights (lum, h, width, height, texelsperunit);
	}
	// Shading relative to the texture's own brightness: Quake's dark textures as bumpy as bright ones.
	mean /= (float)(width * height);
	scale *= CLAMP (0.75f, NORMALMAP_MEAN / q_max (mean, 1e-3f), 2.f);

	for (y = 0; y < height; y++)
	{
		const float *up = lum + ((y + height - 1) % height) * width;
		const float *mid = lum + y * width;
		const float *down = lum + ((y + 1) % height) * width;
		for (x = 0; x < width; x++)
		{
			int		l = (x + width - 1) % width, r = (x + 1) % width;
			float	gx = (up[r] + 2.f * mid[r] + down[r]) - (up[l] + 2.f * mid[l] + down[l]); // 8 x d(height)/ds
			float	gy = (down[l] + 2.f * down[x] + down[r]) - (up[l] + 2.f * up[x] + up[r]); // 8 x d(height)/dt, rows down
			float	n[3], len;
			byte	*out = data + (y * width + x) * 4;
			n[0] = -gx * (scale / 8.f);
			n[1] = gy * (scale / 8.f); // green up: against the rows
			n[2] = 1.f;
			len = 1.f / sqrtf (n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
			out[0] = (byte) CLAMP (0, (int)((n[0] * len * 0.5f + 0.5f) * 255.f + 0.5f), 255);
			out[1] = (byte) CLAMP (0, (int)((n[1] * len * 0.5f + 0.5f) * 255.f + 0.5f), 255);
			out[2] = (byte) CLAMP (0, (int)((n[2] * len * 0.5f + 0.5f) * 255.f + 0.5f), 255);
			out[3] = h ? (byte) CLAMP (0, (int)(h[y * width + x] * 255.f + 0.5f), 255) : 255;
		}
	}
	Hunk_FreeToLowMark (mark);
}

/*
================
TexMgr_ShadingToHeights -- QVR: the heights parallax mapping (vr_parallax) walks, from a texture's luminance `lum`:
darker is deeper, as with the bumps. Blurred a little first (1 2 1 across and down, twice on a texture finer than a
texel a unit: about a unit either side), so that its grain doesn't make spikes; then stretched to the texture's own
range, HEIGHT_LOW of its texels at the bottom and 1 - HEIGHT_HIGH at the top, the surface (1). A flat texture's range
is taken as HEIGHT_MINRANGE at least: shallower. The texture tiles: the edges wrap round.
================
*/
static void TexMgr_ShadingToHeights (const float *lum, float *h, int width, int height, float texelsperunit)
{
	int		x, y, i, pass, passes, mark, count = width * height;
	int		histogram[256], seen;
	float	*tmp, lo, hi, range;

	mark = Hunk_LowMark ();
	tmp = (float *) Hunk_AllocNoFill (count * sizeof (float));
	memcpy (h, lum, count * sizeof (float));
	passes = texelsperunit > 1.5f ? 2 : 1;
	for (pass = 0; pass < passes; pass++)
	{
		for (y = 0; y < height; y++)
			for (x = 0; x < width; x++)
			{
				const float *row = h + y * width;
				tmp[y * width + x] = (row[(x + width - 1) % width] + 2.f * row[x] + row[(x + 1) % width]) * 0.25f;
			}
		for (y = 0; y < height; y++)
		{
			const float *up = tmp + ((y + height - 1) % height) * width;
			const float *mid = tmp + y * width;
			const float *down = tmp + ((y + 1) % height) * width;
			for (x = 0; x < width; x++)
				h[y * width + x] = (up[x] + 2.f * mid[x] + down[x]) * 0.25f;
		}
	}

	memset (histogram, 0, sizeof (histogram));
	for (i = 0; i < count; i++)
		histogram[CLAMP (0, (int)(h[i] * 255.f + 0.5f), 255)]++;
	for (i = 0, seen = 0; i < 255 && seen + histogram[i] <= (int)(count * HEIGHT_LOW); i++)
		seen += histogram[i];
	lo = i / 255.f;
	for (i = 255, seen = 0; i > 0 && seen + histogram[i] <= (int)(count * (1.f - HEIGHT_HIGH)); i--)
		seen += histogram[i];
	hi = i / 255.f;
	range = q_max (hi - lo, HEIGHT_MINRANGE);
	for (i = 0; i < count; i++)
		h[i] = 1.f - CLAMP (0.f, (hi - h[i]) / range, 1.f);
	Hunk_FreeToLowMark (mark);
}

/*
================
TexMgr_LoadImage32 -- handles 32bit source data
================
*/
static void TexMgr_LoadImage32 (gltexture_t *glt, unsigned *data)
{
	int	miplevel, mipwidth, mipheight, picmip;
	glformat_t internalformat;
	qboolean compress;
	int normalmap = normalmap_kind[glt - gltextures_base] & ~NORMALMAP_HEIGHTS; // QVR
	qboolean heights; // QVR

	// HASALPHA detection
	if (glt->source_format == SRC_RGBA && !(glt->flags & TEXPREF_ALPHAPIXELS) && !normalmap) // QVR: normal maps are opaque
	{
		int num_pixels = glt->width * glt->height;
		byte* pixel_data = (byte*)data;

		for (int i = 0; i < num_pixels; i++)
		{
			if (pixel_data[i * 4 + 3] != 255)
			{
				glt->flags |= TEXPREF_ALPHA;
				glt->flags |= TEXPREF_ALPHAPIXELS;
				break;
			}
		}
	}

	// mipmap down
	picmip = (glt->flags & TEXPREF_NOPICMIP) ? 0 : q_max((int)gl_picmip.value, 0);
	mipwidth = TexMgr_SafeTextureSize (glt->width >> picmip);
	mipheight = TexMgr_SafeTextureSize (glt->height >> picmip);
	if (normalmap == NORMALMAP_SHADING) // QVR: made at most NORMALMAP_MAXSIZE, and NORMALMAP_TEXELS a unit
	{
		int most = NORMALMAP_TEXELS * q_max (1, (int)normalmap_worldwidth[glt - gltextures_base]);
		while (mipwidth > most && mipwidth > 1 && mipheight > 1 && !(mipwidth & 1) && !(mipheight & 1))
		{
			mipwidth >>= 1;
			mipheight >>= 1;
		}
		while (mipwidth > NORMALMAP_MAXSIZE && !(mipwidth & 1))
			mipwidth >>= 1;
		while (mipheight > NORMALMAP_MAXSIZE && !(mipheight & 1))
			mipheight >>= 1;
	}
	while ((int) glt->height > mipheight)
	{
		TexMgr_MipMapH (data, glt->width, glt->height, glt->depth);
		glt->height >>= 1;
		if (glt->flags & TEXPREF_ALPHA && glt->target == GL_TEXTURE_2D)
			TexMgr_AlphaEdgeFix ((byte *)data, glt->width, glt->height);
	}
	while ((int) glt->width > mipwidth)
	{
		TexMgr_MipMapW (data, glt->width, glt->height, glt->depth);
		glt->width >>= 1;
		if (glt->flags & TEXPREF_ALPHA && glt->target == GL_TEXTURE_2D)
			TexMgr_AlphaEdgeFix ((byte *)data, glt->width, glt->height);
	}

	// QVR: the world's normal maps (NORMALMAP_HEIGHTS) carry the heights parallax mapping walks (vr_parallax) in alpha:
	// made ones from the shading, authored ones their own alpha (DarkPlaces' convention; 255, flat, if they have none)
	heights = (normalmap_kind[glt - gltextures_base] & NORMALMAP_HEIGHTS) != 0;
	if (normalmap == NORMALMAP_SHADING) // QVR
	{
		float texelsperunit = glt->width / (float) q_max (1, (int)normalmap_worldwidth[glt - gltextures_base]);
		TexMgr_ShadingToNormals ((byte *)data, glt->width, glt->height, NORMALMAP_DEPTH * texelsperunit, heights, texelsperunit);
	}

	// upload
	compress = gl_compress_textures.value && TexMgr_CanCompress (glt);
	internalformat = (glt->flags & TEXPREF_HASALPHA) ? glformats[compress].alpha : glformats[compress].solid;
	if (normalmap) // QVR: x and y only, half the memory (the shaders make z); and the height, for the world's
	{
		internalformat.id = heights ? GL_RGBA8 : GL_RG8;
		internalformat.ratio = heights ? 1 : 2;
	}
	glt->compression = internalformat.ratio;
	GL_Bind (GL_TEXTURE0, glt);
	GL_TexImage (glt, 0, internalformat.id, glt->width, glt->height, GL_RGBA, GL_UNSIGNED_BYTE, data);

	// upload mipmaps
	if (glt->flags & TEXPREF_MIPMAP)
	{
		if (glt->flags & (TEXPREF_CUBEMAP|TEXPREF_ARRAY))
		{
			GL_GenerateMipmapFunc (glt->target);
		}
		else
		{
			mipwidth = glt->width;
			mipheight = glt->height;

			for (miplevel=1; mipwidth > 1 || mipheight > 1; miplevel++)
			{
				if (mipheight > 1)
				{
					TexMgr_MipMapH (data, mipwidth, mipheight, glt->depth);
					mipheight >>= 1;
				}
				if (mipwidth > 1)
				{
					TexMgr_MipMapW (data, mipwidth, mipheight, glt->depth);
					mipwidth >>= 1;
				}
				GL_TexImage (glt, miplevel, internalformat.id, mipwidth, mipheight, GL_RGBA, GL_UNSIGNED_BYTE, data);
			}
		}
	}

	// set filter modes
	TexMgr_SetFilterModes (glt);
}

/*
================
TexMgr_LoadImage8 -- handles 8bit source data, then passes it to LoadImage32
================
*/
static void TexMgr_LoadImage8 (gltexture_t *glt, byte *data)
{
	extern cvar_t gl_fullbrights;
	qboolean padw = false, padh = false;
	byte padbyte;
	unsigned int *usepal;
	int i;

	// HACK HACK HACK -- taken from tomazquake
	if (strstr(glt->name, "shot1sid") &&
	    glt->width == 32 && glt->height == 32 &&
	    CRC_Block(data, 1024) == 65393)
	{
		// This texture in b_shell1.bsp has some of the first 32 pixels painted white.
		// They are invisible in software, but look really ugly in GL. So we just copy
		// 32 pixels from the bottom to make it look nice.
		memcpy (data, data + 32*31, 32);
	}

	// detect false alpha cases
	if (glt->flags & TEXPREF_ALPHA && !(glt->flags & TEXPREF_CONCHARS))
	{
		for (i = 0; i < (int) (glt->width * glt->height * glt->depth); i++)
			if (data[i] == 255) //transparent index
				break;
		if (i == (int) (glt->width * glt->height * glt->depth))
			glt->flags -= TEXPREF_ALPHA;
	}

	// choose palette and padbyte
	if (glt->flags & TEXPREF_ALPHABRIGHT)
	{
		usepal = gl_fullbrights.value ? d_8to24table_alphabright : d_8to24table_opaque;
		padbyte = 0;
	}
	else if (glt->flags & TEXPREF_FULLBRIGHT)
	{
		if (glt->flags & TEXPREF_ALPHA)
			usepal = d_8to24table_fbright_fence;
		else
			usepal = d_8to24table_fbright;
		padbyte = 0;
	}
	else if (glt->flags & TEXPREF_NOBRIGHT && gl_fullbrights.value)
	{
		if (glt->flags & TEXPREF_ALPHA)
			usepal = d_8to24table_nobright_fence;
		else
			usepal = d_8to24table_nobright;
		padbyte = 0;
	}
	else if (glt->flags & TEXPREF_CONCHARS)
	{
		usepal = d_8to24table_conchars;
		padbyte = 0;
	}
	else
	{
		usepal = d_8to24table;
		padbyte = 255;
	}

	// pad each dimention, but only if it's not going to be downsampled later
	if (glt->flags & TEXPREF_PAD)
	{
		if ((int) glt->width < TexMgr_SafeTextureSize(glt->width))
		{
			data = TexMgr_PadImageW (data, glt->width, glt->height, padbyte);
			glt->width = TexMgr_Pad(glt->width);
			padw = true;
		}
		if ((int) glt->height < TexMgr_SafeTextureSize(glt->height))
		{
			data = TexMgr_PadImageH (data, glt->width, glt->height, padbyte);
			glt->height = TexMgr_Pad(glt->height);
			padh = true;
		}
	}

	// convert to 32bit
	data = (byte *)TexMgr_8to32(data, glt->width * glt->height * glt->depth, usepal);

	// fix edges
	if (glt->flags & TEXPREF_ALPHA)
		TexMgr_AlphaEdgeFix (data, glt->width, glt->height);
	else
	{
		if (padw)
			TexMgr_PadEdgeFixW (data, glt->source_width, glt->source_height);
		if (padh)
			TexMgr_PadEdgeFixH (data, glt->source_width, glt->source_height);
	}

	// upload it
	TexMgr_LoadImage32 (glt, (unsigned *)data);
}

/*
================
TexMgr_LoadLightmap -- handles lightmap data
================
*/
static void TexMgr_LoadLightmap (gltexture_t *glt, byte *data)
{
	// upload it
	glt->compression = 1;
	GL_Bind (GL_TEXTURE0, glt);
	GL_TexImage (glt, 0, GL_RGBA8, glt->width, glt->height, gl_lightmap_format, GL_UNSIGNED_BYTE, data);

	// set filter modes
	TexMgr_SetFilterModes (glt);
}

/*
================
TexMgr_LoadImageEx -- the one entry point for loading all textures
================
*/
gltexture_t *TexMgr_LoadImageEx (qmodel_t *owner, const char *name, int width, int height, int depth, enum srcformat format,
			       byte *data, const char *source_file, src_offset_t source_offset, unsigned flags)
{
	unsigned short crc = 0;
	gltexture_t *glt = NULL;
	int mark;

	if (isDedicated)
		return NULL;

	// cubemaps/arrays are only partially implemented, disable unsupported flags
	if (flags & (TEXPREF_ARRAY | TEXPREF_CUBEMAP))
		flags = (flags & ~(TEXPREF_OVERWRITE | TEXPREF_PAD)) | TEXPREF_NOPICMIP;

	// cache check
	if (data && (flags & TEXPREF_OVERWRITE))
	{
		switch (format)
		{
		case SRC_INDEXED:
			crc = CRC_Block(data, width * height * depth);
			break;
		case SRC_LIGHTMAP:
			crc = CRC_Block(data, width * height * depth * lightmap_bytes);
			break;
		case SRC_RGBA:
			crc = CRC_Block(data, width * height * depth * 4);
			break;
		default: /* not reachable but avoids compiler warnings */
			crc = 0;
			break;
		}

		if ((glt = TexMgr_FindTexture (owner, name)) && glt->source_crc == crc)
			return glt;
	}

	if (!glt)
		glt = TexMgr_NewTexture ();

	// copy data
	glt->owner = owner;
	if (flags & TEXPREF_CUBEMAP)
		glt->target = GL_TEXTURE_CUBE_MAP;
	else if (flags & TEXPREF_ARRAY)
		glt->target = GL_TEXTURE_2D_ARRAY;
	else
		glt->target = GL_TEXTURE_2D;
	q_strlcpy (glt->name, name, sizeof(glt->name));
	glt->width = width;
	glt->height = height;
	glt->depth = depth;
	glt->compression = 1;
	glt->flags = flags;
	glt->shirt = -1;
	glt->pants = -1;
	q_strlcpy (glt->source_file, source_file, sizeof(glt->source_file));
	glt->source_offset = source_offset;
	glt->source_format = format;
	glt->source_width = width;
	glt->source_height = height;
	glt->source_crc = crc;

	//upload it
	mark = Hunk_LowMark();

	switch (glt->source_format)
	{
	case SRC_INDEXED:
		TexMgr_LoadImage8 (glt, data);
		break;
	case SRC_LIGHTMAP:
		TexMgr_LoadLightmap (glt, data);
		break;
	case SRC_RGBA:
		TexMgr_LoadImage32 (glt, (unsigned *)data);
		break;
	}

	GL_ObjectLabelFunc (GL_TEXTURE, glt->texnum, -1, glt->name);
	if (flags & TEXPREF_BINDLESS && gl_bindless_able)
	{
		glt->bindless_handle = GL_GetTextureHandleARBFunc (glt->texnum);
		GL_MakeTextureHandleResidentARBFunc (glt->bindless_handle);
	}

	Hunk_FreeToLowMark(mark);

	return glt;
}

/*
================
TexMgr_LoadNormalMap -- QVR: the normal map dynamic lights light `base` with (vr_normalmaps): made from `data`, the
texture's own pixels or a *_bump height map (NORMALMAP_SHADING), or an authored *_norm map (NORMALMAP_AUTHORED).
`worldwidth` is the texture's width in the world (a replacement image has more texels), for the bumps' depth. With
NORMALMAP_HEIGHTS or'ed into `kind` (the world's), its alpha is the height parallax mapping walks (vr_parallax). It is
reloaded from its source like any texture, and freed with its owner.
================
*/
gltexture_t *TexMgr_LoadNormalMap (gltexture_t *base, const char *name, int width, int height, enum srcformat format,
	byte *data, const char *source_file, src_offset_t source_offset, int kind, int worldwidth)
{
	char		nmname[64];
	gltexture_t	*glt;
	int			mark;

	if (isDedicated || !base || !data || (kind & ~NORMALMAP_HEIGHTS) == NORMALMAP_NONE || format == SRC_LIGHTMAP)
		return NULL;

	q_snprintf (nmname, sizeof (nmname), "%s_vrnorm", name ? name : base->name);
	glt = TexMgr_NewTexture ();
	glt->owner = base->owner;
	glt->target = GL_TEXTURE_2D;
	q_strlcpy (glt->name, nmname, sizeof (glt->name));
	glt->width = width;
	glt->height = height;
	glt->depth = 1;
	glt->compression = 1;
	glt->flags = TEXPREF_MIPMAP | (base->flags & (TEXPREF_BINDLESS | TEXPREF_PAD | TEXPREF_PERSIST));
	glt->shirt = -1;
	glt->pants = -1;
	q_strlcpy (glt->source_file, source_file, sizeof (glt->source_file));
	glt->source_offset = source_offset;
	glt->source_format = format;
	glt->source_width = width;
	glt->source_height = height;
	glt->source_crc = 0;
	glt->bindless_handle = 0;
	normalmap_kind[glt - gltextures_base] = (byte) kind;
	normalmap_worldwidth[glt - gltextures_base] = (unsigned short) CLAMP (1, worldwidth, 65535);

	mark = Hunk_LowMark ();
	if (format == SRC_INDEXED)
		TexMgr_LoadImage8 (glt, data);
	else
		TexMgr_LoadImage32 (glt, (unsigned *)data);
	GL_ObjectLabelFunc (GL_TEXTURE, glt->texnum, -1, glt->name);
	if (glt->flags & TEXPREF_BINDLESS && gl_bindless_able)
	{
		glt->bindless_handle = GL_GetTextureHandleARBFunc (glt->texnum);
		GL_MakeTextureHandleResidentARBFunc (glt->bindless_handle);
	}
	Hunk_FreeToLowMark (mark);

	normalmap_of[base - gltextures_base] = glt;
	return glt;
}

/*
================
TexMgr_IndexedSmooth -- QVR: whether Quake's own (8-bit) textures are drawn smooth (vr_texture_smooth 2, or a linear
gl_texturemode). Only then do they get heights for parallax mapping: drawn sharp, its shifts bend their texels.
================
*/
qboolean TexMgr_IndexedSmooth (void)
{
	return VR_TextureSmoothing () >= 2 || glmodes[gl_texfilter.mode].magfilter == GL_LINEAR;
}

/*
================
TexMgr_NormalMap -- QVR: a texture's normal map, or a flat one
================
*/
gltexture_t *TexMgr_NormalMap (gltexture_t *glt)
{
	gltexture_t *nm = glt && gltextures_base ? normalmap_of[glt - gltextures_base] : NULL;
	return nm ? nm : flatnormaltexture;
}

/*
================
TexMgr_LoadImage
================
*/
gltexture_t *TexMgr_LoadImage (qmodel_t *owner, const char *name, int width, int height, enum srcformat format,
			       byte *data, const char *source_file, src_offset_t source_offset, unsigned flags)
{
	return TexMgr_LoadImageEx (owner, name, width, height, 1, format, data, source_file, source_offset, flags);
}


/*
================================================================================

	COLORMAPPING AND TEXTURE RELOADING

================================================================================
*/

/*
================
TexMgr_ReloadImage -- reloads a texture, and colormaps it if needed
================
*/
void TexMgr_ReloadImage (gltexture_t *glt, int shirt, int pants)
{
	byte	translation[256];
	byte	*src, *dst, *data = NULL, *translated;
	int		mark, size, i;
	enum srcformat fmt = glt->source_format;

//
// get source data
//
	mark = Hunk_LowMark ();

	if (glt->source_file[0] && glt->source_offset) {
		//lump inside file
		FILE *f;
		int sz;
		COM_FOpenFile(glt->source_file, &f, NULL);
		if (!f) goto invalid;
		fseek (f, glt->source_offset, SEEK_CUR);
		size = glt->source_width * glt->source_height;
		/* should be SRC_INDEXED, but no harm being paranoid:  */
		if (glt->source_format == SRC_RGBA) {
			size *= 4;
		}
		else if (glt->source_format == SRC_LIGHTMAP) {
			size *= lightmap_bytes;
		}
		data = (byte *) Hunk_AllocNoFill (size);
		sz = (int) fread (data, 1, size, f);
		fclose (f);
		if (sz != size) {
			Hunk_FreeToLowMark(mark);
			Host_Error("Read error for %s", glt->name);
		}
	}
	else if (glt->source_file[0] && !glt->source_offset) {
		data = Image_LoadImage (glt->source_file, (int *)&glt->source_width, (int *)&glt->source_height, &fmt); //simple file
	}
	else if (!glt->source_file[0] && glt->source_offset) {
		data = (byte *) glt->source_offset; //image in memory
	}
	if (!data && shirt > -1 && pants > -1) {
invalid:	Con_Printf ("TexMgr_ReloadImage: invalid source for %s\n", glt->name);
		Hunk_FreeToLowMark(mark);
		return;
	}

	if (fmt != glt->source_format)
		Sys_Error ("Texture %s changed format from %i to %i", glt->name, glt->source_format, fmt);

	glt->width = glt->source_width;
	glt->height = glt->source_height;
//
// apply shirt and pants colors
//
// if shirt and pants are -1,-1, use existing shirt and pants colors
// if existing shirt and pants colors are -1,-1, don't bother colormapping
	if (shirt > -1 && pants > -1)
	{
		if (glt->source_format == SRC_INDEXED)
		{
			glt->shirt = shirt;
			glt->pants = pants;
		}
		else
			Con_DPrintf ("TexMgr_ReloadImage: can't colormap a non SRC_INDEXED texture: %s\n", glt->name);
	}
	if (glt->shirt > -1 && glt->pants > -1)
	{
		//create new translation table
		for (i = 0; i < 256; i++)
			translation[i] = i;

		shirt = glt->shirt * 16;
		if (shirt < 128)
		{
			for (i = 0; i < 16; i++)
				translation[TOP_RANGE+i] = shirt + i;
		}
		else
		{
			for (i = 0; i < 16; i++)
				translation[TOP_RANGE+i] = shirt+15-i;
		}

		pants = glt->pants * 16;
		if (pants < 128)
		{
			for (i = 0; i < 16; i++)
				translation[BOTTOM_RANGE+i] = pants + i;
		}
		else
		{
			for (i = 0; i < 16; i++)
				translation[BOTTOM_RANGE+i] = pants+15-i;
		}

		//translate texture
		size = glt->width * glt->height;
		dst = translated = (byte *) Hunk_AllocNoFill (size);
		src = data;

		for (i = 0; i < size; i++)
			*dst++ = translation[*src++];

		data = translated;
	}
//
// upload it
//
	GL_DeleteTexture (glt);
	glGenTextures (1, &glt->texnum);

	switch (glt->source_format)
	{
	case SRC_INDEXED:
		TexMgr_LoadImage8 (glt, data);
		break;
	case SRC_LIGHTMAP:
		TexMgr_LoadLightmap (glt, data);
		break;
	case SRC_RGBA:
		TexMgr_LoadImage32 (glt, (unsigned *)data);
		break;
	}

	GL_ObjectLabelFunc (GL_TEXTURE, glt->texnum, -1, glt->name);
	if (glt->flags & TEXPREF_BINDLESS && gl_bindless_able)
	{
		glt->bindless_handle = GL_GetTextureHandleARBFunc (glt->texnum);
		GL_MakeTextureHandleResidentARBFunc (glt->bindless_handle);
	}

	Hunk_FreeToLowMark(mark);
}

/*
================
TexMgr_ReloadImages -- reloads all texture images. called only by vid_restart
================
*/
void TexMgr_ReloadImages (void)
{
	gltexture_t *glt;

	TexMgr_CreateSamplers ();

// ericw -- tricky bug: if the hunk is almost full, an allocation in TexMgr_ReloadImage
// triggers cache items to be freed, which calls back into TexMgr to free the
// texture. If this frees 'glt' in the loop below, the active_gltextures
// list gets corrupted.
// A test case is jam3_tronyn.bsp with -heapsize 65536, and do several mode
// switches/fullscreen toggles
// 2015-09-04 -- Cache_Flush workaround was causing issues (http://sourceforge.net/p/quakespasm/bugs/10/)
// switching to a boolean flag.
	in_reload_images = true;

	for (glt = active_gltextures; glt; glt = glt->next)
	{
		glGenTextures(1, &glt->texnum);
		TexMgr_ReloadImage (glt, -1, -1);
	}

	in_reload_images = false;
}

/*
================
TexMgr_ReloadNobrightImages -- reloads all texture that were loaded with the nobright palette.  called when gl_fullbrights changes
================
*/
void TexMgr_ReloadNobrightImages (void)
{
	gltexture_t *glt;

	for (glt = active_gltextures; glt; glt = glt->next)
		if (glt->flags & (TEXPREF_NOBRIGHT|TEXPREF_ALPHABRIGHT))
			TexMgr_ReloadImage(glt, -1, -1);
}

/*
================================================================================

	TEXTURE BINDING / TEXTURE UNIT SWITCHING

================================================================================
*/

static GLuint	currenttexture[4]; // to avoid unnecessary texture sets
static GLenum	currenttexunit = GL_TEXTURE0_ARB;

/*
================
GL_SelectTexture
================
*/
static void GL_SelectTexture (GLenum texunit)
{
	if (texunit == currenttexunit)
		return;

	GL_ActiveTextureFunc(texunit);
	currenttexunit = texunit;
}

/*
================
GL_Bind
================
*/
qboolean GL_Bind (GLenum texunit, gltexture_t *texture)
{
	if (!texture)
		texture = nulltexture;
	if (!GL_BindNative (texunit, texture->target, texture->texnum))
		return false;
	texture->visframe = r_framecount;
	return true;
}

/*
================
GL_BindTextures

Wrapper around glBindTextures with fallback
if ARB_multi_bind is not present
================
*/
void GL_BindTextures (GLuint first, GLsizei count, gltexture_t **textures)
{
	GLuint handles[8];
	GLsizei i;

	if (gl_multi_bind_able && count < countof (handles))
	{
		for (i = 0; i < count; i++)
		{
			gltexture_t *tex = textures[i];
			if (!tex)
				tex = nulltexture;
			tex->visframe = r_framecount;
			handles[i] = tex->texnum;
			if (i + first < countof (currenttexture))
				currenttexture[i + first] = tex->texnum;
		}
		GL_BindTexturesFunc (first, count, handles);
	}
	else
	{
		for (i = 0; i < count; i++)
			GL_Bind (GL_TEXTURE0 + first + i, textures[i]);
	}
}

/*
================
GL_BindNative
================
*/
qboolean GL_BindNative (GLenum texunit, GLenum type, GLuint handle)
{
	GLuint index = texunit - GL_TEXTURE0;
	SDL_assert(texunit >= GL_TEXTURE0);

	if (index < countof (currenttexture))
	{
		if (currenttexture[index] == handle)
			return false;
		currenttexture[index] = handle;
	}

	GL_SelectTexture (texunit);
	glBindTexture (type, handle);

	return true;
}

/*
================
GL_DeleteNativeTexture -- ericw

Wrapper around glDeleteTextures that also clears the given texture number
from our per-TMU cached texture binding table.
================
*/
void GL_DeleteNativeTexture (GLuint texnum)
{
	int i;
	for (i = 0; i < countof(currenttexture); i++)
		if (texnum == currenttexture[i])
			currenttexture[i] = 0;
	glDeleteTextures (1, &texnum);
}

/*
================
GL_DeleteTexture -- ericw
================
*/
static void GL_DeleteTexture (gltexture_t *texture)
{
	if (!texture->texnum)
		return;
	if (texture->bindless_handle)
	{
		GL_MakeTextureHandleNonResidentARBFunc (texture->bindless_handle);
		texture->bindless_handle = 0;
	}
	GL_DeleteNativeTexture (texture->texnum);
	texture->texnum = 0;
}

/*
================
GL_ClearBindings -- ericw
================
*/
void GL_ClearBindings(void)
{
	int i;

	memset (&currenttexture, 0, sizeof (currenttexture));
	if (gl_multi_bind_able)
	{
		GL_BindTexturesFunc (0, countof (currenttexture), NULL);
		GL_BindSamplersFunc (0, countof (currenttexture), NULL);
	}
	else
		for (i = 0; i < countof (currenttexture); i++)
		{
			GL_SelectTexture (GL_TEXTURE0 + i);
			glBindTexture (GL_TEXTURE_2D, 0);
			GL_BindSamplerFunc (i, 0);
		}
}

/*
================================================================================

	PALETTIZATION

================================================================================
*/

GLuint gl_palette_lut;
GLuint gl_palette_buffer[2]; // original + postprocessed

static unsigned int cached_palette[256];
static softemu_metric_t cached_softemu_metric = SOFTEMU_METRIC_INVALID;
static float cached_gamma;
static float cached_contrast;
static vec4_t cached_blendcolor;

/*
================
GLPalette_DeleteResources
================
*/
static void GLPalette_InvalidateRemapped (void)
{
	int i;
	cached_gamma = -1.f;
	cached_contrast = -1.f;
	for (i = 0; i < 4; i++)
		cached_blendcolor[i] = -1.f;
}

/*
================
GLPalette_DeleteResources
================
*/
void GLPalette_DeleteResources (void)
{
	GL_DeleteNativeTexture (gl_palette_lut);
	GL_DeleteBuffer (gl_palette_buffer[1]);
	GL_DeleteBuffer (gl_palette_buffer[0]);
	gl_palette_lut = 0;
	gl_palette_buffer[0] = 0;
	gl_palette_buffer[1] = 0;
}

/*
================
GLPalette_CreateResources
================
*/
void GLPalette_CreateResources (void)
{
	int i;

	glGenTextures (1, &gl_palette_lut);
	GL_BindNative (GL_TEXTURE0, GL_TEXTURE_3D, gl_palette_lut);
	GL_ObjectLabelFunc (GL_TEXTURE, gl_palette_lut, -1, "palette lut");
	GL_TexImage3DFunc (GL_TEXTURE_3D, 0, GL_R8UI, 128, 128, 128, 0, GL_RED_INTEGER, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri (GL_TEXTURE_3D, GL_TEXTURE_BASE_LEVEL, 0);
	glTexParameteri (GL_TEXTURE_3D, GL_TEXTURE_MAX_LEVEL, 0);
	glTexParameteri (GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri (GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	for (i = 0; i < 2; i++)
		gl_palette_buffer[i] =
			GL_CreateBuffer (GL_SHADER_STORAGE_BUFFER, GL_STATIC_DRAW,
				i ? "remapped palette buffer" : "src palette buffer",
				256 * sizeof (GLuint), NULL
			);

	memset (cached_palette, 0, sizeof (cached_palette));
	cached_softemu_metric = SOFTEMU_METRIC_INVALID;
	GLPalette_InvalidateRemapped ();
}

/*
================
GLPalette_UpdateLookupTable
================
*/
void GLPalette_UpdateLookupTable (void)
{
	softemu_metric_t metric;
	int i;

	if (r_softemu_metric.value < 0.f)
	{
		qboolean oklab =
			softemu != SOFTEMU_BANDED ||
			cls.state != ca_connected ||
			cls.signon != SIGNONS ||
			!cl.worldmodel ||
			cl.worldmodel->litfile
		;
		metric = oklab ? SOFTEMU_METRIC_OKLAB : SOFTEMU_METRIC_NAIVE;
	}
	else
	{
		metric = (int)r_softemu_metric.value;
		metric = CLAMP (0, (int)metric, SOFTEMU_METRIC_COUNT - 1);
	}

	SDL_assert ((unsigned)metric < SOFTEMU_METRIC_COUNT);

	if (cached_softemu_metric == metric && !memcmp (cached_palette, d_8to24table, sizeof (cached_palette)))
		return;

	cached_softemu_metric = metric;
	memcpy (cached_palette, d_8to24table, sizeof (cached_palette));
	GLPalette_InvalidateRemapped ();

	GL_UseProgramFunc (glprogs.palette_init[metric]);
	GL_BindImageTextureFunc (0, gl_palette_lut, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_R8UI);
	GL_BindBufferRange (GL_SHADER_STORAGE_BUFFER, 0, gl_palette_buffer[0], 0, 256 * sizeof (GLuint));
	GL_BufferSubDataFunc (GL_SHADER_STORAGE_BUFFER, 0, 256 * sizeof (GLuint), d_8to24table);

	for (i = 0; i < 128; i++)
	{
		GL_Uniform1iFunc (0, i << 16);
		GL_DispatchComputeFunc (1, 128, 1);
	}

	GL_MemoryBarrierFunc (GL_TEXTURE_FETCH_BARRIER_BIT);
}

/*
================
GLPalette_Postprocess

Applies viewblend, gamma, and contrast if needed

Returns index of palette buffer to use:
0 = original palette
1 = postprocessed palette
================
*/
int GLPalette_Postprocess (void)
{
	const float *blend;
	
	if (!softemu)
		return 0;

	blend = (v_blend[3] && gl_polyblend.value) ? v_blend : vec4_origin;

	/* can we use the original palette? */
	if (vid_gamma.value == 1.f &&
		vid_contrast.value == 1.f &&
		blend[3] == 0.f)
		return 0;

	/* no change since last time? */
	if (cached_gamma == vid_gamma.value &&
		cached_contrast == vid_contrast.value &&
		memcmp (cached_blendcolor, blend, 4 * sizeof (float)) == 0)
		return 1;

	cached_gamma = vid_gamma.value;
	cached_contrast = vid_contrast.value;
	memcpy (cached_blendcolor, blend, 4 * sizeof (float));

	GL_BeginGroup ("Postprocess palette");

	GL_UseProgram (glprogs.palette_postprocess);
	GL_BindBufferRange (GL_SHADER_STORAGE_BUFFER, 0, gl_palette_buffer[0], 0, 256 * sizeof (GLuint));
	GL_BindBufferRange (GL_SHADER_STORAGE_BUFFER, 1, gl_palette_buffer[1], 0, 256 * sizeof (GLuint));
	GL_Uniform2fFunc (0, vid_gamma.value, CLAMP (1.0f, vid_contrast.value, 2.0f));
	GL_Uniform4fvFunc (1, 1, blend);
	GL_DispatchComputeFunc (256/64, 1, 1);
	GL_MemoryBarrierFunc (GL_SHADER_STORAGE_BARRIER_BIT);

	GL_EndGroup ();

	return 1;
}
