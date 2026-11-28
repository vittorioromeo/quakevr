// QVR: normal maps as textures and the textures' memory count.
// Moved as it was out of gl_texmgr.c, which includes this file at the same place (one translation unit: the
// same statics, no behaviour change). Not compiled on its own (Quake/vr/iw/README.md; IRONWAIL_DIFF.md).

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

	if (isDedicated || !base || !data || NORMALMAP_TYPE (kind) == NORMALMAP_NONE || format == SRC_LIGHTMAP)
		return NULL;

	q_snprintf (nmname, sizeof (nmname), "%s_vrnorm", name ? name : base->name);
	if (name && (kind & NORMALMAP_FILE) && (glt = TexMgr_ShareNormalMap (base, name, kind)) != NULL)
		return glt;
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
	normalmap_generated[glt - gltextures_base] = (byte) VR_PBRGeneratedFlags (source_file); // resolved once, not per draw

	mark = Hunk_LowMark ();
	texmgr_normalasync = true; // QVR: a skin's made on a job (TexMgr_LoadImage32)
	texmgr_normalposted = false;
	{
		VR_TRACY_SCOPE ("normal map source prepared"); // QVR: Tracy builds only
		if (format == SRC_INDEXED)
			TexMgr_LoadImage8 (glt, data);
		else
			TexMgr_LoadImage32 (glt, (unsigned *)data);
	}
	texmgr_normalasync = false;
	if (texmgr_normalposted) // QVR: uploaded, and base's normal map, at its join (TexMgr_CommitNormalMap)
	{
		texmgr_normalposted = false;
		VR_SkinNormalsSetBase (glt, base);
		Hunk_FreeToLowMark (mark);
		return glt;
	}
	TexMgr_CommitNormalMap (glt, base, NULL);
	Hunk_FreeToLowMark (mark);
	return glt;
}

/*
================
TexMgr_CommitNormalMap -- QVR: a normal map's texels uploaded (`data`, its final size: a skin's made on a job, at its
join, vr/vr_normalmaps.cpp; NULL: uploaded already), labelled, made resident (bindless), and base's normal map
================
*/
void TexMgr_CommitNormalMap (gltexture_t *glt, gltexture_t *base, byte *data)
{
	if (data)
		TexMgr_Upload32 (glt, (unsigned *) data, Sys_DoubleTime ());
	GL_ObjectLabelFunc (GL_TEXTURE, glt->texnum, -1, glt->name);
	if (glt->flags & TEXPREF_BINDLESS && gl_bindless_able)
	{
		glt->bindless_handle = GL_GetTextureHandleARBFunc (glt->texnum);
		GL_MakeTextureHandleResidentARBFunc (glt->bindless_handle);
	}
	if (base)
		normalmap_of[base - gltextures_base] = glt;
}

/*
================
TexMgr_ShareNormalMap -- QVR: an authored file (`name`: progs/hand_rig_00_00_norm) serves every skin of its model that
uses it (the hand's 4, the body's 16, a model's skins) with one texture: `base` gets the one already made from it, if
any (then the file needn't even be read again)
================
*/
gltexture_t *TexMgr_ShareNormalMap (gltexture_t *base, const char *name, int kind)
{
	char		nmname[64];
	gltexture_t	*glt;

	if (!base || !name)
		return NULL;
	q_snprintf (nmname, sizeof (nmname), "%s_vrnorm", name);
	glt = TexMgr_FindTexture (base->owner, nmname);
	if (!glt || (normalmap_kind[glt - gltextures_base] & ~NORMALMAP_FLAT) != (byte) kind)
		return NULL;
	normalmap_of[base - gltextures_base] = glt;
	return glt;
}

/*
================
TexMgr_LoadExtNormalMap -- QVR: an external pack's normal map (`name`, a "vrext/" file: vr/vr_extmaps.cpp) for `base`,
authored, with heights in its alpha, kept beside base's own (TexMgr_NormalMap stays the made one: the pack's is drawn
instead by choice, vr_extmaps_normals). `data` NULL: only one already made from that file for another texture, or NULL.
================
*/
gltexture_t *TexMgr_LoadExtNormalMap (gltexture_t *base, const char *name, int width, int height, byte *data, int worldwidth)
{
	const int kind = NORMALMAP_AUTHORED | NORMALMAP_FILE | NORMALMAP_HEIGHTS | NORMALMAP_EXT;
	gltexture_t *own, *glt;

	if (!base || !gltextures_base)
		return NULL;
	own = normalmap_of[base - gltextures_base];
	glt = data ? TexMgr_LoadNormalMap (base, name, width, height, SRC_RGBA, data, name, 0, kind, worldwidth)
		: TexMgr_ShareNormalMap (base, name, kind);
	normalmap_of[base - gltextures_base] = own;
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
TexMgr_NormalMapAuthored -- QVR: whether a texture's normal map is an authored file's (a *_norm, or a *_bump's
heights): a real shape, drawn at its own strength on models (vr_normalmap_authored)
================
*/
qboolean TexMgr_NormalMapAuthored (gltexture_t *glt)
{
	gltexture_t *nm = glt && gltextures_base ? normalmap_of[glt - gltextures_base] : NULL;
	return nm && (normalmap_kind[nm - gltextures_base] & NORMALMAP_FILE) != 0;
}

/*
================
TexMgr_NormalMapGenerated -- QVR: a normal map's verified generated components (1 normal, 2 height: vr/vr_pbr.cpp),
resolved from its source when it was made; 0 for any other texture
================
*/
int TexMgr_NormalMapGenerated (const gltexture_t *nm)
{
	return nm && gltextures_base ? normalmap_generated[nm - gltextures_base] : 0;
}

/*
================
TexMgr_NormalMapParallax -- QVR: what heights a texture's normal map carries for parallax mapping: 0 none (no normal
map, none kept, or an authored file without alpha: flat), 1 made ones (from the shading; vr_parallax_models on models),
2 an authored file's own alpha (a baked map's relief; vr_parallax_authored)
================
*/
int TexMgr_NormalMapParallax (gltexture_t *glt)
{
	gltexture_t *nm = glt && gltextures_base ? normalmap_of[glt - gltextures_base] : NULL;
	int kind = nm ? normalmap_kind[nm - gltextures_base] : NORMALMAP_NONE;
	if (!(kind & NORMALMAP_HEIGHTS) || (kind & NORMALMAP_FLAT))
		return 0;
	return (kind & NORMALMAP_FILE) && NORMALMAP_TYPE (kind) == NORMALMAP_AUTHORED ? 2 : 1;
}

/*
================
TexMgr_MemStats -- QVR: for vr_memstats: the managed textures, how many of them are normal maps, and their size in
megabytes as imagelist counts it
================
*/
void TexMgr_MemStats (int *count, int *normalmaps, double *megabytes)
{
	gltexture_t	*glt;
	double		bytes = 0;
	int			nm = 0;

	for (glt = active_gltextures; glt; glt = glt->next)
	{
		double s = (double) glt->width * glt->height * (glt->flags & TEXPREF_CUBEMAP ? glt->depth * 6 : glt->depth);
		if (glt->flags & TEXPREF_MIPMAP)
			s = s * 4.0 / 3.0;
		bytes += s * 4 / glt->compression;
		if (gltextures_base && normalmap_kind[glt - gltextures_base] != NORMALMAP_NONE)
			nm++;
	}
	*count = numgltextures;
	*normalmaps = nm;
	*megabytes = bytes / 0x100000;
}

