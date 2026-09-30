// vr_modelload.cpp -- what Quake VR adds to loading models (gl_model.c calls these): the light's directions beside a
// relit map (.lux, deluxemaps), the parts of their textures the item boxes' faces show (parallax mapping), full-colour
// replacement skins (DarkPlaces' names), and the skins' normal maps with their islands (alias models and MD5 meshes).
// Engine-style code, kept out of gl_model.c (docs/vr-port/IRONWAIL_DIFF.md).

#include "vr_engine.hpp"

#include <cfloat>
#include <cmath>
#include <cstdlib>
#include <cstring>

/*
=================
VR_LoadLux -- the light's directions (deluxemaps): a .lux beside the map's .lit, as ericw-tools' `light -lux`
writes it and DarkPlaces, FTE and QuakeSpasm-Spiked read it: "QLIT", version 1, then 3 bytes (x, y, z mapped from
-1..1 to 0..255) for each byte of the lighting lump, in the same order (every style of a face has its own), the
direction the light comes from at the luxel in the face's texture space (x along the texture's s axis, y against its
t axis, z the face's normal: ericw-tools' light/write.cc). GL_BuildLightmaps makes a texture of them.
=================
*/
extern "C" void VR_LoadLux (qmodel_t *loadmodel, lump_t *l)
{
	char luxfilename[MAX_OSPATH];
	unsigned int path_id;
	int mark;
	byte *data;

	loadmodel->luxdata = NULL;
	if (!l->filelen || loadmodel->bspversion == BSPVERSION_QUAKE64)
		return;
	q_strlcpy (luxfilename, VR_ModelFile (loadmodel->name), sizeof (luxfilename)); // the relit map's, if any
	COM_StripExtension (luxfilename, luxfilename, sizeof (luxfilename));
	q_strlcat (luxfilename, ".lux", sizeof (luxfilename));
	mark = Hunk_LowMark ();
	data = (byte *) COM_LoadHunkFile (luxfilename, &path_id);
	if (!data)
		return;
	// as the .lit: only from the map's own game folder or one searched before it, and for this lightmap
	if (path_id < loadmodel->path_id || com_filesize != 8 + l->filelen * 3 || memcmp (data, "QLIT", 4) ||
		LittleLong (((int *)data)[1]) != 1)
	{
		Hunk_FreeToLowMark (mark);
		Con_DPrintf ("ignored %s (another game folder, version or size)\n", luxfilename);
		return;
	}
	Con_DPrintf2 ("%s loaded\n", luxfilename);
	loadmodel->luxdata = data + 8;
}

/*
=================
VR_ItemTextureClamp -- the part of each texture the faces of an ammo or health box (maps/b_*.bsp) show, for
parallax mapping (texture_t uvclamp): the rays it walks stop at the face's edges instead of reading what lies past
them (the shells' and nails' sides show the lower three quarters of their textures: the black top quarter, which
the rays wrapped into below the face, sank the box's sides into a black box). In the texture's coordinates (the
vertices'), each face's range moved by whole textures to start in 0..1; none on an axis where a face shows a whole
texture or more (it tiles) or the faces' ranges disagree.
=================
*/
extern "C" void VR_ItemTextureClamp (qmodel_t *loadmodel)
{
	int			i, j, e, axis;
	msurface_t	*s;
	texture_t	*t, *t2;

	if (q_strncasecmp (loadmodel->name, "maps/b_", 7))
		return;
	for (i = 0; i < loadmodel->numtextures; i++)
		if (loadmodel->textures[i])
			memset (loadmodel->textures[i]->uvclamp, 0, sizeof (loadmodel->textures[i]->uvclamp));
	for (i = 0, s = loadmodel->surfaces; i < loadmodel->numsurfaces; i++, s++)
	{
		float lo[2] = {FLT_MAX, FLT_MAX}, hi[2] = {-FLT_MAX, -FLT_MAX};
		t = s->texinfo && s->texinfo->texnum >= 0 && s->texinfo->texnum < loadmodel->numtextures ?
			loadmodel->textures[s->texinfo->texnum] : NULL;
		if (!t || (s->flags & (SURF_DRAWSKY | SURF_DRAWTURB)) || !t->width || !t->height)
			continue;
		for (j = 0; j < s->numedges; j++)
		{
			mvertex_t *v;
			e = loadmodel->surfedges[s->firstedge + j];
			v = e >= 0 ? &loadmodel->vertexes[loadmodel->edges[e].v[0]] : &loadmodel->vertexes[loadmodel->edges[-e].v[1]];
			for (axis = 0; axis < 2; axis++)
			{
				float c = (DotProduct (v->position, s->texinfo->vecs[axis]) + s->texinfo->vecs[axis][3]) /
					(float)(axis ? t->height : t->width);
				lo[axis] = q_min (lo[axis], c);
				hi[axis] = q_max (hi[axis], c);
			}
		}
		for (axis = 0; axis < 2; axis++)
		{
			float *c = t->uvclamp, k;
			if (c[axis] < 0.f) // this axis tiles already
				continue;
			if (hi[axis] - lo[axis] >= 0.999f)
			{
				c[axis] = -1.f;
				c[axis + 2] = -2.f;
				continue;
			}
			k = floorf (lo[axis] + 1e-4f);
			lo[axis] -= k;
			hi[axis] -= k;
			if (c[axis + 2] > c[axis]) // another face's range: both
			{
				lo[axis] = q_min (lo[axis], c[axis]);
				hi[axis] = q_max (hi[axis], c[axis + 2]);
				if (hi[axis] - lo[axis] >= 0.999f)
				{
					c[axis] = -1.f;
					c[axis + 2] = -2.f;
					continue;
				}
			}
			c[axis] = lo[axis];
			c[axis + 2] = hi[axis];
		}
	}
	// the animation's other frames (their faces name the first)
	for (i = 0; i < loadmodel->numtextures; i++)
	{
		t = loadmodel->textures[i];
		if (!t || t->name[0] != '+')
			continue;
		if (t->uvclamp[2] <= t->uvclamp[0] && t->uvclamp[3] <= t->uvclamp[1])
			continue;
		for (t2 = t->anim_next; t2 && t2 != t; t2 = t2->anim_next)
			memcpy (t2->uvclamp, t->uvclamp, sizeof (t->uvclamp));
		for (t2 = t->alternate_anims; t2 && t2 != t; t2 = t2->anim_next)
			if (t2->uvclamp[2] <= t2->uvclamp[0] && t2->uvclamp[3] <= t2->uvclamp[1])
				memcpy (t2->uvclamp, t->uvclamp, sizeof (t->uvclamp));
	}
}

/*
===============
Mod_SkinIslands -- a skin's islands (the texels its triangles cover) for its normal map's heights and bumps
(VR_SetHeightMask), on the hunk: per texel of a w x h skin, how many texels it lies inside (0 outside). `corners`:
each triangle's three corners in texels (x, y), `numtris` of them. A texel within 0.7 of a texel of a triangle is in.
===============
*/
static byte *Mod_SkinIslands (const float *corners, int numtris, int w, int h);

// the same, malloc'd (VR_SetHeightMaskLazy)
static byte *Mod_SkinIslandsMalloc (const float *corners, int numtris, int w, int h)
{
	double	t0 = Sys_DoubleTime ();
	int		mark = Hunk_LowMark ();
	byte	*mask = (byte *) malloc ((size_t) w * h);
	if (!mask)
		Sys_Error ("Mod_SkinIslandsMalloc: out of memory");
	memcpy (mask, Mod_SkinIslands (corners, numtris, w, h), (size_t) w * h);
	Hunk_FreeToLowMark (mark);
	VR_TimeAdd ("        islands", Sys_DoubleTime () - t0); // load timing (vr_startup_times)
	return mask;
}

static byte *Mod_SkinIslands (const float *corners, int numtris, int w, int h)
{
	int		i, j, k, x, y;
	byte	*mask = (byte *) Hunk_Alloc (w * h);

	for (i = 0; i < numtris; i++)
	{
		const float *p = corners + i * 6;
		float	area, xmin = FLT_MAX, xmax = -FLT_MAX, ymin = FLT_MAX, ymax = -FLT_MAX;
		for (j = 0; j < 3; j++)
		{
			xmin = q_min (xmin, p[j*2]); xmax = q_max (xmax, p[j*2]);
			ymin = q_min (ymin, p[j*2+1]); ymax = q_max (ymax, p[j*2+1]);
		}
		area = (p[2] - p[0]) * (p[5] - p[1]) - (p[3] - p[1]) * (p[4] - p[0]);
		for (y = CLAMP (0, (int) floorf (ymin - 1.f), h - 1); y <= CLAMP (0, (int) ceilf (ymax + 1.f), h - 1); y++)
			for (x = CLAMP (0, (int) floorf (xmin - 1.f), w - 1); x <= CLAMP (0, (int) ceilf (xmax + 1.f), w - 1); x++)
			{
				float c[2] = {x + 0.5f, y + 0.5f};
				qboolean inside = true;
				for (j = 0; j < 3 && inside; j++)
				{
					const float *a = p + j * 2, *b = p + ((j + 1) % 3) * 2;
					float ex = b[0] - a[0], ey = b[1] - a[1], len = sqrtf (ex * ex + ey * ey);
					float e = (ex * (c[1] - a[1]) - ey * (c[0] - a[0])) * (area < 0.f ? -1.f : 1.f);
					inside = len <= 0.f || e >= -0.7f * len;
				}
				if (inside)
					mask[y * w + x] = 255;
			}
	}
	// how far inside, in texels (0 outside; the skin's edges are outside)
	for (k = 0; k < 3; k++)
	{
		for (y = 0; y < h; y++)
			for (x = 0; x < w; x++)
			{
				int d = mask[y * w + x], dx, dy;
				if (!d)
					continue;
				for (dy = -1; dy <= 1; dy++)
					for (dx = -1; dx <= 1; dx++)
					{
						int nx = x + dx, ny = y + dy;
						int n = nx < 0 || ny < 0 || nx >= w || ny >= h ? 0 : mask[ny * w + nx];
						d = q_min (d, n + 1);
					}
				mask[y * w + x] = (byte) d;
			}
	}
	return mask;
}

/*
===============
Mod_SkinNormalMapLater, VR_LoadSkinNormalMaps -- a model's skins get their normal maps (vr_normalmaps), with
heights for parallax mapping (vr_parallax_models), once its triangles are loaded: the heights rise to the top at the
edges of the skin's islands (the parts its triangles cover; VR_SetHeightMask), so that the rays stop at a seam
instead of reading another part of the skin (the rest of the skin or another part of the model); the bumps are made
within the islands (TexMgr_SkinToNormals). `name`: the skin's external name (progs/ogre.mdl_0), where an authored
normal map is looked for (VR_LoadNormalMap: its _norm, or its _bump), for Quake's 8-bit skin as for a full-colour
replacement. `data`: the skin's pixels, 8-bit (in the model's file) or a replacement's RGBA (malloc'ed: freed here).
===============
*/
static struct
{
	gltexture_t		*glt;
	byte			*data;
	enum srcformat	format;
	qboolean		owned;
	char			name[MAX_QPATH];
} skinnormalmaps[MAX_SKINS * 4];
static int numskinnormalmaps;

static void Mod_SkinNormalMapLater (gltexture_t *glt, byte *data, enum srcformat format, qboolean owned, const char *name)
{
	if (glt && numskinnormalmaps < (int) countof (skinnormalmaps))
	{
		skinnormalmaps[numskinnormalmaps].glt = glt;
		skinnormalmaps[numskinnormalmaps].data = data;
		skinnormalmaps[numskinnormalmaps].format = format;
		skinnormalmaps[numskinnormalmaps].owned = owned;
		q_strlcpy (skinnormalmaps[numskinnormalmaps].name, name, sizeof (skinnormalmaps[0].name));
		numskinnormalmaps++;
	}
	else if (owned)
		free (data);
}

extern "C" void VR_LoadSkinNormalMaps (qmodel_t *loadmodel, void *hdr, const stvert_t *verts, const dtriangle_t *tris)
{
	aliashdr_t *pheader = (aliashdr_t *) hdr;
	double	t0 = Sys_DoubleTime (); // load timing (vr_startup_times)
	int		i, j, n, w = pheader->skinwidth, h = pheader->skinheight, mark;
	byte	*mask = NULL;

	if (!numskinnormalmaps)
		return;
	mark = Hunk_LowMark ();
	if (w > 0 && h > 0)
	{
		// the triangles' corners on the skin (the back's seam vertices half a skin right)
		float *corners = (float *) Hunk_AllocNoFill (q_max (pheader->numtris, 1) * 6 * sizeof (float));
		for (i = 0, n = 0; i < pheader->numtris; i++)
		{
			for (j = 0; j < 3; j++)
				if (tris[i].vertindex[j] < 0 || tris[i].vertindex[j] >= pheader->numverts)
					break;
			if (j < 3)
				continue;
			for (j = 0; j < 3; j++)
			{
				const stvert_t *v = &verts[tris[i].vertindex[j]];
				corners[n * 6 + j * 2 + 0] = (float) v->s + (!tris[i].facesfront && v->onseam ? w / 2 : 0);
				corners[n * 6 + j * 2 + 1] = (float) v->t;
			}
			n++;
		}
		VR_SetHeightMaskLazy (corners, n, w, h, Mod_SkinIslandsMalloc); // made only for a normal map made anew
	}
	else
		VR_SetHeightMask (mask, w, h);
	for (i = 0; i < numskinnormalmaps; i++)
	{
		char shared[MAX_QPATH];
		q_snprintf (shared, sizeof (shared), "%s_0", loadmodel->name); // skin 0's (every skin has the same coordinates)
		VR_LoadNormalMap (skinnormalmaps[i].glt, skinnormalmaps[i].name, strcmp (shared, skinnormalmaps[i].name) ? shared : NULL,
			skinnormalmaps[i].data, skinnormalmaps[i].format, w, NORMALMAP_HEIGHTS | NORMALMAP_SKIN);
		if (skinnormalmaps[i].owned)
			free (skinnormalmaps[i].data);
	}
	VR_SetHeightMask (NULL, 0, 0);
	numskinnormalmaps = 0;
	Hunk_FreeToLowMark (mark);
	VR_TimeAdd ("  their normal maps (authored files or made)", Sys_DoubleTime () - t0);
}

/*
===============
Mod_LoadExternalSkin -- a full-colour replacement for an alias model's skin (DarkPlaces' names, as model packs
ship them: `name` is progs/ogre.mdl_0, a group's frames progs/ogre.mdl_0_1), and its fullbrights (name_glow, or
name_luma); else the 8-bit skin's own fullbrights stay (`*fb` untouched). The pixels, malloc'ed, go to the normal map
made once the triangles are known (Mod_SkinNormalMapLater). NULL if there is none.
===============
*/
static gltexture_t *Mod_LoadExternalSkin (qmodel_t *loadmodel, const char *name, unsigned texflags, gltexture_t **fb)
{
	int				mark = Hunk_LowMark (), fwidth, fheight, i;
	enum srcformat	fmt;
	byte			*data, *copy;
	gltexture_t		*glt, *glow;
	size_t			size;
	char			fbname[MAX_QPATH];

	if (isDedicated)
		return NULL;
	data = Image_LoadImage (name, &fwidth, &fheight, &fmt);
	if (!data || fwidth < 1 || fheight < 1)
	{
		Hunk_FreeToLowMark (mark);
		return NULL;
	}
	size = (size_t) fwidth * fheight * (fmt == SRC_INDEXED ? 1 : 4);
	copy = (byte *) malloc (size);
	if (copy)
		memcpy (copy, data, size);
	glt = TexMgr_LoadImage (loadmodel, name, fwidth, fheight, fmt, data, name, 0,
		(texflags & TEXPREF_ALPHA) | TEXPREF_MIPMAP | (fmt == SRC_INDEXED ? 0 : TEXPREF_NOBRIGHT));
	Hunk_FreeToLowMark (mark);
	for (i = 0, glow = NULL; i < 2 && !glow; i++)
	{
		int gw, gh;
		enum srcformat gfmt;
		q_snprintf (fbname, sizeof (fbname), "%s%s", name, i ? "_luma" : "_glow");
		data = Image_LoadImage (fbname, &gw, &gh, &gfmt);
		if (data)
			glow = TexMgr_LoadImage (loadmodel, fbname, gw, gh, gfmt, data, fbname, 0, TEXPREF_MIPMAP);
		Hunk_FreeToLowMark (mark);
	}
	if (glow)
		*fb = glow;
	if (copy)
		Mod_SkinNormalMapLater (glt, copy, fmt, true, name);
	Con_DPrintf ("%s: external skin %s (%d x %d)%s\n", loadmodel->name, name, fwidth, fheight, glow ? " with fullbrights" : "");
	return glt;
}

/*
===============
VR_ExternalSkin -- skin i (frame j of a group; -1 a single skin) of the alias model being loaded, just loaded
from its 8-bit pixels: replaced by an external full-colour one if there is one (Mod_LoadExternalSkin; the 8-bit
skin's fullbrights kept unless it brings its own), and its normal map queued (Mod_SkinNormalMapLater) by that name.
===============
*/
extern "C" void VR_ExternalSkin (qmodel_t *loadmodel, void *hdr, gltexture_t **skin, gltexture_t **fb, unsigned texflags,
	byte *texels, int size, src_offset_t offset, int i, int j)
{
	aliashdr_t	*pheader = (aliashdr_t *) hdr;
	char		name[MAX_QPATH];
	gltexture_t	*ext, *extfb = NULL;
	char		fbname[MAX_QPATH];

	if (j < 0) // progs/ogre.mdl_0 (a derived model's are its source's: VR_ModelSkinName)
		q_snprintf (name, sizeof (name), "%s_%i", VR_ModelSkinName (loadmodel->name), i);
	else // a group's frame: progs/ogre.mdl_0_1
		q_snprintf (name, sizeof (name), "%s_%i_%i", VR_ModelSkinName (loadmodel->name), i, j);
	ext = Mod_LoadExternalSkin (loadmodel, name, texflags, &extfb);
	if (!ext)
	{
		Mod_SkinNormalMapLater (*skin, texels, SRC_INDEXED, false, name);
		return;
	}
	if (!extfb && !*fb && Mod_CheckFullbrights (texels, size)) // an ALPHABRIGHT skin: its fullbrights as their own texture
	{
		if (j < 0)
			q_snprintf (fbname, sizeof (fbname), "%s:frame%i_glow", loadmodel->name, i);
		else
			q_snprintf (fbname, sizeof (fbname), "%s:frame%i_%i_glow", loadmodel->name, i, j);
		extfb = TexMgr_LoadImage (loadmodel, fbname, pheader->skinwidth, pheader->skinheight, SRC_INDEXED, texels,
			loadmodel->name, offset, (texflags & ~TEXPREF_ALPHA) | TEXPREF_FULLBRIGHT);
	}
	*skin = ext;
	if (extfb)
		*fb = extfb;
}


/*
===============
VR_SkinsBegin -- an alias model's skins start loading (Mod_LoadAllSkins): none queued for normal maps
===============
*/
extern "C" void VR_SkinsBegin (void)
{
	numskinnormalmaps = 0;
}

/*
===============
Mod_MD5SkinNormalMap -- an MD5 mesh's skin (`name`: progs/hand_rig_00_00; the jointed hands, the body) gets its
normal map as an alias model's does (Mod_LoadSkinNormalMaps): an authored name_norm or name_bump (or `shared`'s, the
first skin's: progs/hand_rig_00_00), else one made from its colours within its islands (from the mesh's texture
coordinates).
===============
*/
static byte			*md5islands; // the islands of the surface whose skins load now (Mod_LoadMD5Skins): the same for
static const aliashdr_t	*md5islands_surf; // all its skins of a size (the body's 16, 1024 x 1024: 5 ms each)
static int				md5islands_w, md5islands_h;

static void Mod_MD5SkinNormalMap (aliashdr_t *surf, gltexture_t *glt, const char *name, const char *shared, byte *data,
	enum srcformat fmt, int w, int h)
{
	int				i, j, mark;
	const iqmvert_t	*verts = (const iqmvert_t *)((byte *) surf + surf->vertexes);
	const unsigned short *indexes = (const unsigned short *)((byte *) surf + surf->indexes);
	float			*corners;
	char			first[MAX_QPATH];

	if (!glt || !data || w < 1 || h < 1 || !VR_NormalMaps ())
		return;
	q_strlcpy (first, shared, sizeof (first)); // (a va () buffer)
	mark = Hunk_LowMark ();
	if (!md5islands || md5islands_surf != surf || md5islands_w != w || md5islands_h != h)
	{
		free (md5islands);
		corners = (float *) Hunk_AllocNoFill (q_max (surf->numtris, 1) * 6 * sizeof (float));
		for (i = 0; i < surf->numtris; i++)
			for (j = 0; j < 3; j++)
			{
				const iqmvert_t *v = &verts[indexes[i * 3 + j]];
				corners[i * 6 + j * 2 + 0] = v->st[0] * w;
				corners[i * 6 + j * 2 + 1] = v->st[1] * h;
			}
		md5islands = (byte *) malloc ((size_t) w * h);
		if (!md5islands)
			Sys_Error ("Mod_MD5SkinNormalMap: out of memory");
		memcpy (md5islands, Mod_SkinIslands (corners, surf->numtris, w, h), (size_t) w * h);
		md5islands_surf = surf;
		md5islands_w = w;
		md5islands_h = h;
	}
	VR_SetHeightMask (md5islands, w, h);
	VR_LoadNormalMap (glt, name, strcmp (name, first) ? first : NULL, data, fmt, w, NORMALMAP_HEIGHTS | NORMALMAP_SKIN);
	VR_SetHeightMask (NULL, 0, 0);
	Hunk_FreeToLowMark (mark);
}

/*
===============
Mod_MD5SharedNormalMap -- the skin whose authored normal map (progs/<shader>_NN_00_norm or _bump) skin `skin` uses
if it has none of its own: the nearest one before it that has one (the body's: skin 0's for the clothes, skin 4's for
all the armours' 12), else skin 0's name (its made map then).
===============
*/
static void Mod_MD5SharedNormalMap (const char *shader, int skin, char *out, size_t size)
{
	static const char *const suffixes[] = {"_norm.png", "_norm.tga", "_bump.png", "_bump.tga"};
	char	path[MAX_QPATH];
	int		k, i;

	for (k = skin; k >= 0; k--)
		for (i = 0; i < (int) countof (suffixes); i++)
		{
			q_snprintf (path, sizeof (path), "progs/%s_%02d_00%s", shader, k, suffixes[i]);
			if (COM_FileExists (path, NULL))
			{
				q_snprintf (out, size, "progs/%s_%02d_00", shader, k);
				return;
			}
		}
	q_snprintf (out, size, "progs/%s_00_00", shader);
}


/*
===============
VR_MD5SkinNormalMap -- skin `skin`, frame `frame` of an MD5 mesh's surface (Mod_LoadMD5Skins: progs/<shader>_NN_FF,
`data` its pixels) gets its normal map: by the skin's own name (the texture's may have become its _glow or _luma's)
===============
*/
extern "C" void VR_MD5SkinNormalMap (void *surf, gltexture_t *glt, const char *shader, int skin, int frame, byte *data,
	enum srcformat fmt, int w, int h)
{
	char skinname[MAX_QPATH], shared[MAX_QPATH];
	q_snprintf (skinname, sizeof (skinname), "progs/%s_%02u_%02u", shader, (unsigned) skin, (unsigned) frame);
	Mod_MD5SharedNormalMap (shader, skin, shared, sizeof (shared));
	double t0 = Sys_DoubleTime (); // load timing (vr_startup_times)
	Mod_MD5SkinNormalMap ((aliashdr_t *) surf, glt, skinname, shared, data, fmt, w, h);
	VR_TimeAdd ("        md5: their normal maps", Sys_DoubleTime () - t0);
}

/*
===============
VR_MD5SkinsReset -- the islands of the last surface's skins freed (Mod_LoadMD5Skins, before and after a surface's)
===============
*/
extern "C" void VR_MD5SkinsReset (void)
{
	free (md5islands);
	md5islands = NULL;
	md5islands_surf = NULL;
}
