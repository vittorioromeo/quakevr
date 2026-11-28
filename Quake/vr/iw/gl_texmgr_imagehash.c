// QVR: texture checksums and the readback test (TexMgr_Imagehash_f).
// Moved as it was out of gl_texmgr.c, which includes this file at the same place (one translation unit: the
// same statics, no behaviour change). Not compiled on its own (Quake/vr/iw/README.md; IRONWAIL_DIFF.md).

/*
===============
TexMgr_Imagehash_f -- QVR: a checksum of every loaded texture as the GPU holds it (level 0 read back as RGBA, FNV-1a
64), one line each in <gamedir>/imagehash.txt ("hash width height depth name") and their sum (order-free) in the
console: two runs (e.g. a texture pack's TGA and PNG files) drew the same pixels when the files match
===============
*/
static void TexMgr_Imagehash_f (void)
{
	const char *filter = Cmd_Argc () >= 2 ? Cmd_Argv (1) : NULL;
	char path[MAX_OSPATH];
	FILE *out;
	gltexture_t *glt;
	unsigned long long total = 0;
	int count = 0;
	VR_LoadJobsJoin (); // QVR: the skins' normal maps made on jobs uploaded first (vr/vr_normalmaps.cpp)

	q_snprintf (path, sizeof (path), "%s/imagehash.txt", com_gamedir);
	out = fopen (path, "w");
	if (!out)
	{
		Con_Printf ("imagehash: can't write %s\n", path);
		return;
	}
	for (glt = active_gltextures; glt; glt = glt->next)
	{
		int faces = (glt->flags & TEXPREF_CUBEMAP) ? 6 : 1;
		size_t facebytes = (size_t) glt->width * glt->height * (glt->depth ? glt->depth : 1) * 4;
		unsigned long long h = 14695981039346656037ULL;
		byte *buffer;
		const char *c;
		size_t i;
		int f;

		if (filter && !q_strcasestr (glt->name, filter))
			continue;
		buffer = (byte *) VR_HeapMalloc (facebytes);
		if (!buffer)
			Sys_Error ("TexMgr_Imagehash_f: out of memory (%dx%dx%d)", glt->width, glt->height, glt->depth);
		GL_Bind (GL_TEXTURE0, glt);
		for (f = 0; f < faces; f++)
		{
			VR_GLGetTexImage (faces == 6 ? GL_TEXTURE_CUBE_MAP_POSITIVE_X + f : glt->target, 0, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
			for (i = 0; i < facebytes; i++)
				h = (h ^ buffer[i]) * 1099511628211ULL;
		}
		VR_HeapFree (buffer);
		fprintf (out, "%016llx %d %d %d %s\n", h, glt->width, glt->height, glt->depth, glt->name);
		for (c = glt->name; *c; c++)
			h = (h ^ (byte) *c) * 1099511628211ULL;
		total += h;
		count++;
	}
	fclose (out);
	Con_Printf ("imagehash: %d textures, sum %016llx, to %s\n", count, total, path);
}

/*
===============
TexMgr_ReadbackTest -- QVR: vr_gles_readback_test's part (vr/vr_glsafe.cpp): every loaded texture's level 0 (each
face of a cube) read as RGBA bytes by glGetTexImage and by its OpenGL ES form (VR_GLGetTexImage under -glescompat),
counted the same, different or skipped (integer or compressed: no ES read)
===============
*/
void TexMgr_ReadbackTest (int *passed, int *failed, int *skipped)
{
	gltexture_t *glt;
	VR_LoadJobsJoin ();
	glPixelStorei (GL_PACK_ALIGNMENT, 1);
	for (glt = active_gltextures; glt; glt = glt->next)
	{
		int faces = (glt->flags & TEXPREF_CUBEMAP) ? 6 : 1;
		int f;
		GL_Bind (GL_TEXTURE0, glt);
		for (f = 0; f < faces; f++)
		{
			int same = VR_GLReadTexImageSame (faces == 6 ? GL_TEXTURE_CUBE_MAP_POSITIVE_X + f : glt->target, 0, GL_RGBA, GL_UNSIGNED_BYTE, glt->name);
			*(same > 0 ? passed : same == 0 ? failed : skipped) += 1;
		}
	}
	glPixelStorei (GL_PACK_ALIGNMENT, 4);
}

