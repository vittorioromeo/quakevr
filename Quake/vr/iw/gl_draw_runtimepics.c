// QVR: run-time pics that last across maps (Draw_ReplacePic...).
// Moved as it was out of gl_draw.c, which includes this file at the same place (one translation unit: the
// same statics, no behaviour change). Not compiled on its own (Quake/vr/iw/README.md; IRONWAIL_DIFF.md).

/*
================
Draw_ReplacePic -- QVR: a pic of data made at run time that lasts across maps and games (vr_bigfont.cpp's
letters). `pic` is Draw_PicBytes () bytes the caller keeps, zeroed before the first call; `data` (8-bit, 255
transparent) stays valid while the pic is used, since reloads read it. The texture of a pic made before in the same
bytes is freed. Filtered as the menu's pictures are (gl_texturemode).
================
*/
size_t Draw_PicBytes (void)
{
	return sizeof (qpic_t) - 4 + sizeof (glpic_t);
}

void Draw_ReplacePic (qpic_t *pic, const char *name, int width, int height, byte *data)
{
	int flags = TEXPREF_ALPHA | TEXPREF_PERSIST | TEXPREF_NOPICMIP | TEXPREF_PAD | TEXPREF_CLAMP | TEXPREF_UNCOMPRESSED;
	glpic_t gl;

	memcpy (&gl, pic->data, sizeof (glpic_t));
	if (gl.gltexture)
		TexMgr_FreeTexture (gl.gltexture);

	pic->width = width;
	pic->height = height;
	gl.gltexture = TexMgr_LoadImage (NULL, name, width, height, SRC_INDEXED, data, "", (src_offset_t)data, flags);
	gl.sl = 0;
	gl.sh = (float)width/(float)TexMgr_PadConditional(width);
	gl.tl = 0;
	gl.th = (float)height/(float)TexMgr_PadConditional(height);
	memcpy (pic->data, &gl, sizeof (glpic_t));
}

/*
================
Draw_ReplacePicRGBA -- QVR: as Draw_ReplacePic, of `data`'s RGBA texels (texwidth x texheight, kept by the caller
while the pic is used), drawn width x height: vr_fontstyle.cpp's upscaled pictures. Smooth: linear, mipmapped;
else nearest.
================
*/
void Draw_ReplacePicRGBA (qpic_t *pic, const char *name, int width, int height, int texwidth, int texheight,
	const unsigned *data, qboolean smooth)
{
	int flags = TEXPREF_ALPHA | TEXPREF_PERSIST | TEXPREF_NOPICMIP | TEXPREF_CLAMP | TEXPREF_UNCOMPRESSED |
		(smooth ? TEXPREF_LINEAR | TEXPREF_MIPMAP : TEXPREF_NEAREST);
	glpic_t gl;

	memcpy (&gl, pic->data, sizeof (glpic_t));
	if (gl.gltexture)
		TexMgr_FreeTexture (gl.gltexture);

	pic->width = width;
	pic->height = height;
	gl.gltexture = TexMgr_LoadImage (NULL, name, texwidth, texheight, SRC_RGBA, (byte *) data, "", (src_offset_t) data, flags);
	gl.sl = 0;
	gl.sh = 1;
	gl.tl = 0;
	gl.th = 1;
	memcpy (pic->data, &gl, sizeof (glpic_t));
}

/*
================
Draw_LoadImagePic -- QVR: a pic (Draw_PicBytes () bytes the caller keeps) from a 32-bit image file (`path`
without its extension: png, tga or jpg, Image_LoadImage), kept across maps and games, mipmapped and drawn smooth
(an image file's filter, TexMgr_FilterMode), made again from the file on a video restart. Its width and height
are the image's; its texture coordinates span the whole image. `bounds` (if not NULL): the rectangle of its texels
that are not fully transparent (left, top, right, bottom; right and bottom past the last). False if there is no
such image.
================
*/
qboolean Draw_LoadImagePic (qpic_t *pic, const char *path, int *bounds)
{
	const int flags = TEXPREF_MIPMAP | TEXPREF_ALPHA | TEXPREF_PERSIST | TEXPREF_NOPICMIP | TEXPREF_CLAMP;
	int mark = Hunk_LowMark ();
	int width, height;
	enum srcformat fmt;
	byte *data = Image_LoadImage (path, &width, &height, &fmt);
	glpic_t gl;

	if (!data)
	{
		Hunk_FreeToLowMark (mark);
		return false;
	}
	if (bounds)
	{
		int x, y;
		bounds[0] = width;
		bounds[1] = height;
		bounds[2] = bounds[3] = 0;
		for (y = 0; y < height; y++)
			for (x = 0; x < width; x++)
				if (fmt != SRC_RGBA || data[(y * width + x) * 4 + 3])
				{
					bounds[0] = q_min (bounds[0], x);
					bounds[1] = q_min (bounds[1], y);
					bounds[2] = q_max (bounds[2], x + 1);
					bounds[3] = q_max (bounds[3], y + 1);
				}
	}
	pic->width = width;
	pic->height = height;
	gl.gltexture = TexMgr_LoadImage (NULL, path, width, height, fmt, data, path, 0, flags);
	gl.sl = 0;
	gl.sh = 1;
	gl.tl = 0;
	gl.th = 1;
	memcpy (pic->data, &gl, sizeof (glpic_t));
	Hunk_FreeToLowMark (mark);
	return true;
}

