// vr_normalmaps.cpp -- the normal maps Quake VR makes for the world's textures and the models' skins (vr_normalmaps;
// gl_texmgr.c's TexMgr_LoadNormalMap makes them textures): from a texture's shading or a skin's colours, with the heights
// parallax mapping walks (vr_parallax), an authored map's heights, and a skin's islands; and coverage-preserving mipmaps
// for alpha-tested textures (vr_alpha_coverage). Engine-style code, kept out of gl_texmgr.c (docs/vr-port/IRONWAIL_DIFF.md);
// the texture manager calls VR_NormalMapMipSize, VR_MakeNormalMap, VR_AlphaCoverage and VR_AlphaCoverageMip.

#include "vr_engine.hpp"

#include "Zancle/String/String.hpp"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define NORMALMAP_MAXSIZE	256 // made ones: the bumps a dynamic light shows need no more (and a 512 texture's would take 1.4 MB)
#define NORMALMAP_TEXELS	2 // made ones: at most 2 texels a unit (a replacement's finer grain is noise as bumps)
#define NORMALMAP_DEPTH		4.f // units deep a texture's shading from black to white is, at vr_normalmap_strength 1
#define NORMALMAP_MEAN		0.35f // the brightness of a texture NORMALMAP_DEPTH is for (relative: dark ones twice as deep at most)
#define HEIGHT_LOW			0.02f // made heights (vr_parallax): the share of a texture's texels at the deepest,
#define HEIGHT_HIGH			0.98f // and below the surface's top
#define HEIGHT_MINRANGE		0.2f // a flatter texture's shading isn't stretched to the whole depth (its grain would)
#define HEIGHT_RIM			2 // made heights of a skin with islands (VR_SetHeightMask): texels over which they rise to the top at an island's edge
static const byte	*heightmask; // VR_SetHeightMask
static int			heightmask_width, heightmask_height;
static const float	*heightmask_corners; // VR_SetHeightMaskLazy: made from these when first needed
static int			heightmask_numtris;
static byte			*(*heightmask_make) (const float *corners, int numtris, int w, int h);
static byte			*heightmask_made; // (malloc'd)
static void TexMgr_EnsureHeightMask (void);


/*
================
TexMgr_ShadingToNormals -- a texture's shading made a normal map (DarkPlaces' r_shadow_bumpscale_basetexture):
its luminance taken for height, bumps from its Sobel gradient; `scale` is how deep a step from black to white is, in
texels, for a texture of average brightness (NORMALMAP_MEAN; darker ones deeper). The texture tiles: the edges wrap
round. Tangent space: x along the texture's s, y up its rows (green up). With `heights`, alpha is the height parallax
mapping walks (TexMgr_ShadingToHeights), `texelsperunit` how fine the texture is in the world; else 255.
================
*/
static void TexMgr_ShadingToHeights (const float *lum, float *h, int width, int height, float texelsperunit);

/*
================
TexMgr_DilateIslands -- a skin's luminance outside its islands (VR_SetHeightMask) replaced by the islands'
own, grown out a texel at a time (DILATE_TEXELS), so that the bumps and heights made from it have no edge at a seam:
the skin's background (often black) or another part of it beside an island would make a ridge along the model's
seams, which the bumps on the model's own light (vr_normalmap_models) would show as bright and dark lines.
================
*/
#define DILATE_TEXELS 4

static void TexMgr_DilateIslands (float *lum, int width, int height)
{
	int		x, y, pass, mark = Hunk_LowMark ();
	byte	*in = (byte *) Hunk_AllocNoFill (width * height), *next = (byte *) Hunk_AllocNoFill (width * height);
	float	*src = (float *) Hunk_AllocNoFill (width * height * sizeof (float));

	for (y = 0; y < height; y++)
		for (x = 0; x < width; x++)
			in[y * width + x] = heightmask[(y * heightmask_height / height) * heightmask_width + x * heightmask_width / width] != 0;
	for (pass = 0; pass < DILATE_TEXELS * q_max (1, width / heightmask_width); pass++)
	{
		memcpy (src, lum, width * height * sizeof (float));
		memcpy (next, in, width * height);
		for (y = 0; y < height; y++)
			for (x = 0; x < width; x++)
			{
				int		dx, dy, count = 0;
				float	sum = 0.f;
				if (in[y * width + x])
					continue;
				for (dy = -1; dy <= 1; dy++)
					for (dx = -1; dx <= 1; dx++)
					{
						int i = ((y + dy + height) % height) * width + (x + dx + width) % width;
						if (in[i])
						{
							sum += src[i];
							count++;
						}
					}
				if (count)
				{
					lum[y * width + x] = sum / count;
					next[y * width + x] = 1;
				}
			}
		memcpy (in, next, width * height);
	}
	Hunk_FreeToLowMark (mark);
}

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
	if (heightmask)
		TexMgr_DilateIslands (lum, width, height);
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
TexMgr_ShadingToHeights -- the heights parallax mapping (vr_parallax) walks, from a texture's luminance `lum`:
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
	// a skin's islands (VR_SetHeightMask): up to the top over HEIGHT_RIM texels at their edges, so that the rays
	// parallax mapping walks stop there instead of reading the skin's other parts past a seam
	if (heightmask)
		for (y = 0; y < height; y++)
			for (x = 0; x < width; x++)
			{
				int mx = x * heightmask_width / width, my = y * heightmask_height / height;
				float d = heightmask[my * heightmask_width + mx] * (width / (float) heightmask_width); // in these texels
				h[y * width + x] = 1.f - (1.f - h[y * width + x]) * CLAMP (0.f, (d - 0.5f) / HEIGHT_RIM, 1.f);
			}
	Hunk_FreeToLowMark (mark);
}

/*
================
TexMgr_SkinToNormals -- a model skin's colours made a normal map (NORMALMAP_SKIN; round 21). Brightness taken
for height (TexMgr_ShadingToNormals) made every speck of an 8-bit skin's dithering a bump and every change of paint a
ramp, and showed little: instead
- edges: the brightness's slope at the finest scale (a 1 2 1 blur) where it is coherent (a line or an edge, by the
  structure tensor), not speckle, and over the skin's own noise level (its median slope);
- forms: the slopes of the brightness blurred over about 1.4, 3.5 and 5.5 texels, so that plate edges, folds and
  muscles read as broad shapes;
- paint: at every scale, a slope where the colour changes more than the brightness (blood on skin, a band of another
  hue) counts less (by the brightness's share of the whole change);
- materials, from the colour: metal (greys and blue-greys) keeps crisp edges and little form, flesh (warm, moderately
  saturated) broad forms and soft edges, blood (saturated red) almost nothing, the rest (cloth, leather) between;
- the tilt is compressed (tanh), so that strong edges make bevels of up to about 45 degrees, not walls.
Within a skin's islands (VR_SetHeightMask) every blur is normalised by the islands' coverage, so that nothing
outside an island (the background, another part) reaches in across a seam. `scale`: the tilt from a slope of 1 in
brightness a texel, then scaled to the skin's own relief (SKIN_P90). With `heights`, alpha is
TexMgr_ShadingToHeights' (as before).
================
*/
#define SKIN_FINE			1.2f	// the edges' weight
#define SKIN_FORMS			3		// the blurred scales for the forms
#define SKIN_COLOR			0.6f	// how much a change of colour counts against one of brightness (paint, not shape)
#define SKIN_BEVEL			1.2f	// the tilt the compression tends to (tanh)
#define SKIN_DEPTH			14.f		// the tilt of a slope of 1 in brightness a texel (at vr_normalmap_strength 1), before SKIN_P90
#define SKIN_P90			0.85f	// a skin's 90th percentile of tilt (before the compression): its relief relative to its own

static float TexMgr_Smoothstep (float a, float b, float x)
{
	float t = CLAMP (0.f, (x - a) / (b - a), 1.f);
	return t * t * (3.f - 2.f * t);
}

// A box blur of radius r across and down (clamped at the edges), in place; `n` channels interleaved (at most 4).
// Running sums: across along each row, down with a sum per column (row by row, in memory order).
static void TexMgr_BoxBlur (float *a, int n, int w, int h, int r, float *tmp)
{
	int		x, y, c, i, stride = w * n, mark = Hunk_LowMark ();
	float	inv = 1.f / (2 * r + 1), sum[4];
	float	*copy = (float *) Hunk_AllocNoFill (w * h * n * sizeof (float)), *sums = (float *) Hunk_AllocNoFill (stride * sizeof (float));

	(void) tmp;
	for (y = 0; y < h; y++)
	{
		float *row = a + y * stride, *out = copy + y * stride;
		for (c = 0; c < n; c++)
			for (sum[c] = 0.f, i = -r; i <= r; i++)
				sum[c] += row[CLAMP (0, i, w - 1) * n + c];
		for (x = 0; x < w; x++)
		{
			const float *add = row + q_min (x + r + 1, w - 1) * n, *sub = row + q_max (x - r, 0) * n;
			for (c = 0; c < n; c++)
			{
				out[x * n + c] = sum[c] * inv;
				sum[c] += add[c] - sub[c];
			}
		}
	}
	memset (sums, 0, stride * sizeof (float));
	for (i = -r; i <= r; i++)
	{
		const float *row = copy + CLAMP (0, i, h - 1) * stride;
		for (x = 0; x < stride; x++)
			sums[x] += row[x];
	}
	for (y = 0; y < h; y++)
	{
		const float *add = copy + q_min (y + r + 1, h - 1) * stride, *sub = copy + q_max (y - r, 0) * stride;
		float *out = a + y * stride;
		for (x = 0; x < stride; x++)
		{
			out[x] = sums[x] * inv;
			sums[x] += add[x] - sub[x];
		}
	}
	Hunk_FreeToLowMark (mark);
}

// `out` (3 channels): `in` blurred over the islands only (a convolution normalised by `wgt`, the islands' coverage):
// three box passes of radius r (a Gaussian of sigma sqrt(r (r + 1))), or with r 0 a 1 2 1 pass (sigma 0.7).
static void TexMgr_BlurIslands (float *out, const float *in, const float *wgt, int w, int h, int r, float *tmp)
{
	int		i, pass, count = w * h, mark = Hunk_LowMark ();
	float	*acc = (float *) Hunk_AllocNoFill (count * 4 * sizeof (float));

	for (i = 0; i < count; i++)
	{
		acc[i*4+0] = in[i*3+0] * wgt[i];
		acc[i*4+1] = in[i*3+1] * wgt[i];
		acc[i*4+2] = in[i*3+2] * wgt[i];
		acc[i*4+3] = wgt[i];
	}
	if (r <= 0)
	{
		float *copy = (float *) Hunk_AllocNoFill (count * 4 * sizeof (float));
		int x, y, c;
		for (pass = 0; pass < 2; pass++)
		{
			memcpy (copy, acc, count * 4 * sizeof (float));
			for (y = 0; y < h; y++)
				for (x = 0; x < w; x++)
				{
					int m = y * w + x;
					int a = pass ? CLAMP (0, y - 1, h - 1) * w + x : y * w + CLAMP (0, x - 1, w - 1);
					int b = pass ? CLAMP (0, y + 1, h - 1) * w + x : y * w + CLAMP (0, x + 1, w - 1);
					for (c = 0; c < 4; c++)
						acc[m * 4 + c] = 0.25f * copy[a * 4 + c] + 0.5f * copy[m * 4 + c] + 0.25f * copy[b * 4 + c];
				}
		}
	}
	else
		for (pass = 0; pass < 3; pass++)
			TexMgr_BoxBlur (acc, 4, w, h, r, tmp);
	for (i = 0; i < count; i++)
	{
		float k = 1.f / q_max (acc[i*4+3], 1e-6f);
		out[i*3+0] = acc[i*4+0] * k;
		out[i*3+1] = acc[i*4+1] * k;
		out[i*3+2] = acc[i*4+2] * k;
	}
	Hunk_FreeToLowMark (mark);
}

// The slopes of the three channels of `img` at x, y (central differences, clamped at the edges), per texel.
static void TexMgr_Slopes (const float *img, int w, int h, int x, int y, float d[6])
{
	const float *l = img + (y * w + q_max (x - 1, 0)) * 3, *r = img + (y * w + q_min (x + 1, w - 1)) * 3;
	const float *u = img + (q_max (y - 1, 0) * w + x) * 3, *b = img + (q_min (y + 1, h - 1) * w + x) * 3;
	int c;
	for (c = 0; c < 3; c++)
	{
		d[c * 2 + 0] = 0.5f * (r[c] - l[c]);
		d[c * 2 + 1] = 0.5f * (b[c] - u[c]);
	}
}

static void TexMgr_SkinToNormals (byte *data, int width, int height, float scale, qboolean heights, float texelsperunit)
{
	static constexpr int	radius[SKIN_FORMS] = {1, 3, 5};			// sigma 1.4, 3.5, 5.5 texels (of the model's skin)
	static constexpr float	formweight[SKIN_FORMS] = {0.8f, 0.9f, 0.7f};
	int		i, x, y, s, mark, count = width * height, hist[256], seen, below;
	float	*col, *blurred, *wgt, *gx, *gy, *jt, *tmp, *lum, *h = NULL, *fine, *forms;
	float	noise, d[6];
	double	start = Sys_DoubleTime (), tformed, theights, tfine;

	if (width < 3 || height < 3)
		return;
	mark = Hunk_LowMark ();
	col = (float *) Hunk_AllocNoFill (count * 3 * sizeof (float));
	blurred = (float *) Hunk_AllocNoFill (count * 3 * sizeof (float));
	jt = (float *) Hunk_AllocNoFill (count * 3 * sizeof (float));
	wgt = (float *) Hunk_AllocNoFill (count * sizeof (float));
	gx = (float *) Hunk_Alloc (count * sizeof (float));
	gy = (float *) Hunk_Alloc (count * sizeof (float));
	fine = (float *) Hunk_AllocNoFill (count * sizeof (float));
	forms = (float *) Hunk_AllocNoFill (count * sizeof (float));
	lum = (float *) Hunk_AllocNoFill (count * sizeof (float));
	tmp = (float *) Hunk_AllocNoFill (q_max (width, height) * sizeof (float));

	// brightness and two colour differences; the islands' coverage; the materials' weights for edges and forms
	for (i = 0; i < count; i++)
	{
		float r = data[i*4+0] * (1.f / 255.f), g = data[i*4+1] * (1.f / 255.f), b = data[i*4+2] * (1.f / 255.f);
		float mx = q_max (r, q_max (g, b)), mn = q_min (r, q_min (g, b)), sat = (mx - mn) / q_max (mx, 1e-3f);
		float grey, metal, blood, flesh, other;
		int	tx = i % width, ty = i / width;

		col[i*3+0] = lum[i] = r * 0.299f + g * 0.587f + b * 0.114f;
		col[i*3+1] = r - g;
		col[i*3+2] = (r + g) * 0.5f - b;
		wgt[i] = !heightmask || heightmask[(ty * heightmask_height / height) * heightmask_width + tx * heightmask_width / width] ? 1.f : 1e-3f;

		grey = r > b + 0.02f ? 1.f - TexMgr_Smoothstep (0.06f, 0.14f, sat) : 1.f - TexMgr_Smoothstep (0.22f, 0.4f, sat);
		metal = grey * TexMgr_Smoothstep (0.03f, 0.12f, mx);
		blood = r > g ? TexMgr_Smoothstep (0.5f, 0.75f, sat) * TexMgr_Smoothstep (0.08f, 0.25f, r - q_max (g, b)) : 0.f;
		flesh = r >= g && g >= b * 0.9f ? (1.f - metal) * (1.f - blood) * TexMgr_Smoothstep (0.12f, 0.25f, sat) *
			(1.f - TexMgr_Smoothstep (0.5f, 0.7f, sat)) * TexMgr_Smoothstep (0.25f, 0.45f, mx) : 0.f;
		other = CLAMP (0.f, 1.f - metal - blood - flesh, 1.f);
		fine[i] = metal * 1.f + flesh * 0.35f + blood * 0.15f + other * 0.7f;
		forms[i] = metal * 0.5f + flesh * 1.f + blood * 0.3f + other * 0.7f;
	}

	// edges: the slopes of the 1 2 1 blur, kept where coherent (the structure tensor, over about 1.4 texels) and
	// over the noise (the median slope within the islands)
	TexMgr_BlurIslands (blurred, col, wgt, width, height, 0, tmp);
	memset (hist, 0, sizeof (hist));
	for (y = 0, seen = 0; y < height; y++)
		for (x = 0; x < width; x++)
		{
			i = y * width + x;
			TexMgr_Slopes (blurred, width, height, x, y, d);
			jt[i*3+0] = d[0] * d[0];
			jt[i*3+1] = d[1] * d[1];
			jt[i*3+2] = d[0] * d[1];
			if (wgt[i] >= 1.f)
			{
				hist[CLAMP (0, (int)(sqrtf (d[0] * d[0] + d[1] * d[1]) * 1024.f), 255)]++;
				seen++;
			}
		}
	for (s = 0; s < 3; s++)
		TexMgr_BoxBlur (jt, 3, width, height, 1, tmp);
	for (i = 0, below = 0; i < 255 && below + hist[i] < seen / 2; i++)
		below += hist[i];
	noise = (i + 0.5f) / 1024.f;
	for (y = 0; y < height; y++)
		for (x = 0; x < width; x++)
		{
			float gl, gc, jxx, jyy, jxy, tr, coh, keep;
			i = y * width + x;
			TexMgr_Slopes (blurred, width, height, x, y, d);
			gl = d[0] * d[0] + d[1] * d[1];
			gc = d[2] * d[2] + d[3] * d[3] + d[4] * d[4] + d[5] * d[5];
			jxx = jt[i*3+0]; jyy = jt[i*3+1]; jxy = jt[i*3+2];
			tr = jxx + jyy;
			coh = tr > 1e-12f ? ((jxx - jyy) * (jxx - jyy) + 4.f * jxy * jxy) / (tr * tr) : 0.f; // ((l1 - l2) / (l1 + l2))^2
			keep = TexMgr_Smoothstep (noise * 0.8f, noise * 2.5f, sqrtf (gl)) * (0.35f + 0.65f * coh) *
				gl / (gl + SKIN_COLOR * gc + 1e-9f) * fine[i] * SKIN_FINE;
			gx[i] += d[0] * keep;
			gy[i] += d[1] * keep;
		}

	tfine = Sys_DoubleTime ();
	// forms: the slopes of broader blurs (a blurred step's slope falls as 1 / sigma: made up for by its square root)
	for (s = 0; s < SKIN_FORMS; s++)
	{
		int r = radius[s] * q_max (1, (int)(texelsperunit + 0.5f)); // an image finer than the skin: as wide on the model
		float k = formweight[s] * sqrtf (sqrtf (r * (r + 1.f)));
		TexMgr_BlurIslands (blurred, col, wgt, width, height, r, tmp);
		for (y = 0; y < height; y++)
			for (x = 0; x < width; x++)
			{
				float gl, gc;
				i = y * width + x;
				TexMgr_Slopes (blurred, width, height, x, y, d);
				gl = d[0] * d[0] + d[1] * d[1];
				gc = d[2] * d[2] + d[3] * d[3] + d[4] * d[4] + d[5] * d[5];
				gl = k * forms[i] * gl / (gl + SKIN_COLOR * gc + 1e-9f);
				gx[i] += d[0] * gl;
				gy[i] += d[1] * gl;
			}
	}

	tformed = Sys_DoubleTime ();
	if (heights)
	{
		if (heightmask)
			TexMgr_DilateIslands (lum, width, height);
		h = (float *) Hunk_AllocNoFill (count * sizeof (float));
		TexMgr_ShadingToHeights (lum, h, width, height, texelsperunit);
	}
	theights = Sys_DoubleTime ();
	// relative to the skin's own relief: its 90th percentile of tilt made SKIN_P90 (within a factor of 3 either way),
	// so that a dark or a soft skin gets as much shape as a contrasty one
	{
		int tilts[256] = {0}, all = 0, p90;
		float k;
		for (i = 0; i < count; i++)
			if (wgt[i] >= 1.f)
			{
				tilts[CLAMP (0, (int)(sqrtf (gx[i] * gx[i] + gy[i] * gy[i]) * scale * 64.f), 255)]++;
				all++;
			}
		for (p90 = 0, below = 0; p90 < 255 && below + tilts[p90] < all * 9 / 10; p90++)
			below += tilts[p90];
		k = CLAMP (1.f / 3.f, SKIN_P90 / ((p90 + 0.5f) / 64.f), 3.f);
		Con_DPrintf ("skin normal map %d x %d: relief 90%% %.2f times %.2f (noise %.4f)\n", width, height, (p90 + 0.5f) / 64.f, k, noise);
		scale *= k;
	}
	for (i = 0; i < count; i++)
	{
		float n[2], t, len;
		byte *out = data + i * 4;
		n[0] = -gx[i] * scale;
		n[1] = gy[i] * scale; // green up: against the rows
		t = sqrtf (n[0] * n[0] + n[1] * n[1]);
		if (t > 1e-6f)
		{
			float c = tanhf (t / SKIN_BEVEL) * SKIN_BEVEL / t;
			n[0] *= c;
			n[1] *= c;
		}
		len = 1.f / sqrtf (n[0] * n[0] + n[1] * n[1] + 1.f);
		out[0] = (byte) CLAMP (0, (int)((n[0] * len * 0.5f + 0.5f) * 255.f + 0.5f), 255);
		out[1] = (byte) CLAMP (0, (int)((n[1] * len * 0.5f + 0.5f) * 255.f + 0.5f), 255);
		out[2] = (byte) CLAMP (0, (int)((len * 0.5f + 0.5f) * 255.f + 0.5f), 255);
		out[3] = h ? (byte) CLAMP (0, (int)(h[i] * 255.f + 0.5f), 255) : 255;
	}
	Hunk_FreeToLowMark (mark);
	Con_DPrintf ("skin normal map %d x %d made in %.1f ms (fine %.1f forms %.1f heights %.1f)" "\n", width, height, (Sys_DoubleTime () - start) * 1000.0, (tfine-start)*1000.0, (tformed-tfine)*1000.0, (theights-tformed)*1000.0);
}

/*
================
TexMgr_AuthoredHeights -- an authored normal map's heights (its alpha: 255 the surface, lower deeper; DarkPlaces'
convention and bake_normals.py's) as parallax mapping walks them (vr_parallax_authored): none if its alpha is all
255 (a map without alpha, or flat: marked NORMALMAP_FLAT, so no rays are walked on it); else, on a skin with islands
(VR_SetHeightMask), up to the top over HEIGHT_RIM texels at their edges as the made heights are, so that the rays
stop at a seam instead of reading the skin's other parts past it
================
*/
static void TexMgr_AuthoredHeights (gltexture_t *glt, byte *data, int *kind)
{
	int x, y, i, n = glt->width * glt->height;

	*kind &= ~NORMALMAP_FLAT;
	for (i = 0; i < n && data[i * 4 + 3] == 255; i++)
		;
	if (i == n)
	{
		*kind |= NORMALMAP_FLAT;
		return;
	}
	TexMgr_EnsureHeightMask (); // (only now: most authored maps are flat, VR_SetHeightMaskLazy)
	if (!heightmask)
		return;
	for (y = 0; y < (int) glt->height; y++)
		for (x = 0; x < (int) glt->width; x++)
		{
			int mx = x * heightmask_width / glt->width, my = y * heightmask_height / glt->height;
			float d = heightmask[my * heightmask_width + mx] * (glt->width / (float) heightmask_width); // in these texels
			byte *a = &data[(y * glt->width + x) * 4 + 3];
			*a = (byte) (255 - (int) ((255 - *a) * CLAMP (0.f, (d - 0.5f) / HEIGHT_RIM, 1.f) + 0.5f));
		}
}

/*
================
VR_SetHeightMask -- a skin's islands, for the heights of the normal maps made next (until set to NULL):
per texel of a width x height skin, how many texels it lies inside the triangles (0 outside, HEIGHT_RIM + 1 or more
well inside); the heights rise to the top at their edges (TexMgr_ShadingToHeights). A normal map reloaded later
(vid_restart) is made without it.
================
*/
extern "C" void VR_SetHeightMask (const byte *mask, int width, int height)
{
	heightmask = mask && width > 0 && height > 0 ? mask : NULL;
	heightmask_width = width;
	heightmask_height = height;
	heightmask_corners = NULL;
	free (heightmask_made);
	heightmask_made = NULL;
}

/*
================
VR_SetHeightMaskLazy -- as VR_SetHeightMask, the islands made by `make` (malloc'd) from the triangles'
`corners` on the skin (`numtris` x 3 x (x, y) texels) only when a normal map is made (not read from the cache, nor
shared: 0.8 ms a model, 60 ms of the first load). The corners must live until VR_SetHeightMask (NULL, 0, 0).
================
*/
extern "C" void VR_SetHeightMaskLazy (const float *corners, int numtris, int width, int height,
	byte *(*make) (const float *corners, int numtris, int w, int h))
{
	VR_SetHeightMask (NULL, 0, 0);
	if (!corners || !make || width <= 0 || height <= 0)
		return;
	heightmask_corners = corners;
	heightmask_numtris = numtris;
	heightmask_make = make;
	heightmask_width = width;
	heightmask_height = height;
}

static void TexMgr_EnsureHeightMask (void)
{
	if (heightmask_corners && !heightmask_made)
	{
		heightmask_made = heightmask_make (heightmask_corners, heightmask_numtris, heightmask_width, heightmask_height);
		heightmask = heightmask_made;
	}
}

/*
================
TexMgr_NormalCacheBuild, TexMgr_SkinNormalsKey -- the normal maps made from skins kept on disk (vr_texcache.cpp).
The build: this file's compile time (the maker's code and constants all compile here: a changed maker never reads an
old map). The key: a hash (FNV-1a, 64 bits) of all that TexMgr_SkinToNormals reads: the texels, their size, the
heights or not, the texels per unit, and the islands' mask.
================
*/
static za::String TexMgr_MakeNormalCacheBuild (void)
{
	const char *stamp = __DATE__ " " __TIME__;
	unsigned long long h = 14695981039346656037ULL;
	char build[24];
	for (; *stamp; stamp++)
		h = (h ^ (byte) *stamp) * 1099511628211ULL;
	q_snprintf (build, sizeof (build), "%016llx", h);
	return build;
}
static const za::String normalcache_build = TexMgr_MakeNormalCacheBuild (); // (made before main, read-only)

static const char *TexMgr_NormalCacheBuild (void)
{
	return normalcache_build.cStr ();
}

static unsigned long long TexMgr_Fnv (unsigned long long h, const void *p, size_t n)
{
	const byte *b = (const byte *) p;
	size_t i;
	for (i = 0; i < n; i++)
		h = (h ^ b[i]) * 1099511628211ULL;
	return h;
}

static unsigned long long TexMgr_SkinNormalsKey (const byte *data, int width, int height, qboolean heights, float texelsperunit)
{
	unsigned long long h = 14695981039346656037ULL;
	const qboolean masked = heightmask || heightmask_corners;
	int params[6] = {width, height, heights ? 1 : 0, masked ? heightmask_width : 0, masked ? heightmask_height : 0,
		heightmask_corners ? heightmask_numtris : -1};
	h = TexMgr_Fnv (h, params, sizeof (params));
	h = TexMgr_Fnv (h, &texelsperunit, sizeof (texelsperunit));
	h = TexMgr_Fnv (h, data, (size_t) width * height * 4);
	if (heightmask_corners) // the islands are made from these (VR_SetHeightMaskLazy)
		h = TexMgr_Fnv (h, heightmask_corners, (size_t) heightmask_numtris * 6 * sizeof (float));
	else if (heightmask)
		h = TexMgr_Fnv (h, heightmask, (size_t) heightmask_width * heightmask_height);
	return h;
}

#define ALPHATEST_CUTOFF 170 // the shaders' alpha test (0.666): 8-bit alphas from 170 pass

/*
================
TexMgr_AlphaCoverageMip -- coverage-preserving mipmaps (Castano; vr_alpha_coverage): a mip level of an
alpha-tested texture (`in`, `count` texels) copied into `out` with its alpha scaled so that the share of its texels
that pass the alpha test is the top level's, `coverage`. The box filter averages thin bars and wires into alphas
under the cutoff: fences and grates thin out and vanish with distance, and crawl as they come and go. The scale is
the cutoff over the alpha that as many texels reach (the middle of the range of alphas that give that count).
================
*/
extern "C" void VR_AlphaCoverageMip (const byte *in, byte *out, int count, float coverage)
{
	int		hist[256], i, t, above = 0, target = (int)(coverage * count + 0.5f), err = INT_MAX, hi = ALPHATEST_CUTOFF, lo = ALPHATEST_CUTOFF, hiabove = -1;

	memset (hist, 0, sizeof (hist));
	for (i = 0; i < count; i++)
		hist[in[i * 4 + 3]]++;
	for (t = 255; t >= 1; t--) // above: the texels whose alpha is t or more
	{
		above += hist[t];
		if (abs (above - target) < err)
		{
			err = abs (above - target);
			hi = lo = t;
			hiabove = above;
		}
		else if (above == hiabove)
			lo = t;
	}
	t = (hi + lo + 1) / 2;
	for (i = 0; i < count; i++)
	{
		out[i * 4 + 0] = in[i * 4 + 0];
		out[i * 4 + 1] = in[i * 4 + 1];
		out[i * 4 + 2] = in[i * 4 + 2];
		out[i * 4 + 3] = (byte) q_min (255, (in[i * 4 + 3] * ALPHATEST_CUTOFF + t / 2) / t);
	}
}

/*
================
VR_NormalMapMipSize -- a normal map made from a texture's shading (NORMALMAP_SHADING): at most NORMALMAP_MAXSIZE, and
NORMALMAP_TEXELS a unit of the world (`worldwidth`: the texture's width there)
================
*/
extern "C" void VR_NormalMapMipSize (int worldwidth, int *mipwidth, int *mipheight)
{
	int most = NORMALMAP_TEXELS * q_max (1, worldwidth);
	while (*mipwidth > most && *mipwidth > 1 && *mipheight > 1 && !(*mipwidth & 1) && !(*mipheight & 1))
	{
		*mipwidth >>= 1;
		*mipheight >>= 1;
	}
	while (*mipwidth > NORMALMAP_MAXSIZE && !(*mipwidth & 1))
		*mipwidth >>= 1;
	while (*mipheight > NORMALMAP_MAXSIZE && !(*mipheight & 1))
		*mipheight >>= 1;
}

/*
================
VR_MakeNormalMap -- a normal map's texels (`data`, RGBA, glt's size once mipmapped down) made: the world's normal maps
(NORMALMAP_HEIGHTS) carry the heights parallax mapping walks (vr_parallax) in alpha: made ones from the shading,
authored ones their own alpha (DarkPlaces' convention; 255, flat, if they have none: NORMALMAP_FLAT set in the kind
returned). A skin's (NORMALMAP_SKIN) are kept on disk (vr_texcache.cpp: 3-4 ms a skin; the world's textures' take a
few hundredths of that).
================
*/
extern "C" int VR_MakeNormalMap (gltexture_t *glt, byte *data, int kind, int worldwidth)
{
	const int normalmap = NORMALMAP_TYPE (kind);
	const qboolean heights = (kind & NORMALMAP_HEIGHTS) != 0;
	if (heights && normalmap == NORMALMAP_AUTHORED) // an authored map's heights (its alpha, as baked)
		TexMgr_AuthoredHeights (glt, data, &kind);
	if (normalmap == NORMALMAP_SHADING)
	{
		float texelsperunit = glt->width / (float) q_max (1, worldwidth);
		double t0 = Sys_DoubleTime (); // load timing (vr_startup_times)
		if (kind & NORMALMAP_SKIN) // a skin's colours (not a *_bump's heights)
		{
			int cache = VR_NormalCacheMode (), n = glt->width * glt->height * 4, hit = 0, cmark = Hunk_LowMark ();
			unsigned long long key = cache ? TexMgr_SkinNormalsKey (data, glt->width, glt->height, heights, texelsperunit) : 0;
			byte *cached = cache ? (byte *) Hunk_AllocNoFill (n) : NULL;
			hit = cache && VR_NormalCacheLoad (TexMgr_NormalCacheBuild (), key, cached, glt->width, glt->height);
			if (hit && cache == 1)
				memcpy (data, cached, n);
			else
			{
				TexMgr_EnsureHeightMask ();
				TexMgr_SkinToNormals (data, glt->width, glt->height, SKIN_DEPTH * texelsperunit, heights, texelsperunit);
				if (hit) // vr_normalmap_cache 2: made anyway, and compared
					VR_NormalCacheChecked (memcmp (data, cached, n) == 0, glt->name);
				else if (cache)
					VR_NormalCacheStore (TexMgr_NormalCacheBuild (), key, data, glt->width, glt->height);
			}
			Hunk_FreeToLowMark (cmark);
			VR_TimeAdd (hit && cache == 1 ? "normal maps of skins read from the cache" : "normal maps made from skins", Sys_DoubleTime () - t0);
		}
		else
		{
			TexMgr_EnsureHeightMask ();
			TexMgr_ShadingToNormals (data, glt->width, glt->height, NORMALMAP_DEPTH * texelsperunit, heights, texelsperunit);
			VR_TimeAdd ("normal maps made from textures", Sys_DoubleTime () - t0);
		}
	}
	return kind;
}

/*
================
VR_AlphaCoverage -- the share of an alpha-tested texture's texels (`count`, RGBA) that pass the alpha test
================
*/
extern "C" float VR_AlphaCoverage (const byte *data, int count)
{
	int i, pass = 0;
	for (i = 0; i < count; i++)
		pass += data[i * 4 + 3] >= ALPHATEST_CUTOFF;
	return pass / (float) count;
}

/*
=================
VR_LoadNormalMap -- the normal map dynamic lights light a world texture or a skin with (vr_normalmaps): an
authored <image>_norm, or a <image>_bump height map, beside a replacement image; else one made from the texture's
own shading (`data`, as loaded). `shared`: another image whose authored maps serve if `image` has none (a model's
skin 0: all its skins lie on the same texture coordinates). `worldwidth` is the texture's width in the world. `flags`: NORMALMAP_HEIGHTS keeps
the heights parallax mapping walks in its alpha (an authored _norm's own alpha, 255 if it has none); NORMALMAP_SKIN
makes it from a model skin's colours (TexMgr_SkinToNormals). Authored ones are marked NORMALMAP_FILE.
=================
*/
extern "C" void VR_LoadNormalMap (gltexture_t *glt, const char *image, const char *shared, byte *data, enum srcformat format,
	int worldwidth, int flags)
{
	int heights = flags & NORMALMAP_HEIGHTS, n;
	static constexpr struct { const char *suffix; int kind; } authored[] = {
		{"_norm", NORMALMAP_AUTHORED},
		{"_bump", NORMALMAP_SHADING},
	};
	char			filename[MAX_OSPATH];
	int				i, mark, fwidth, fheight;
	enum srcformat	fmt;
	byte			*img;

	if (!glt || !data || !VR_NormalMaps () || TexMgr_NormalMap (glt) != TexMgr_NormalMap (NULL))
		return;
	for (n = 0; n < 2; n++)
	for (i = 0; (n ? shared : image) && i < (int) countof (authored); i++)
	{
		double t0 = Sys_DoubleTime ();
		mark = Hunk_LowMark ();
		q_snprintf (filename, sizeof (filename), "%s%s", n ? shared : image, authored[i].suffix);
		if (TexMgr_ShareNormalMap (glt, filename, authored[i].kind | NORMALMAP_FILE | heights)) // made for another skin
		{
			Hunk_FreeToLowMark (mark);
			return;
		}
		img = Image_LoadImage (filename, &fwidth, &fheight, &fmt);
		if (img)
		{
			TexMgr_LoadNormalMap (glt, filename, fwidth, fheight, fmt, img, filename, 0, authored[i].kind | NORMALMAP_FILE | heights, worldwidth);
			Con_DPrintf ("normal map %s (%d x %d, %.1f ms)" "\n", filename, fwidth, fheight, (Sys_DoubleTime () - t0) * 1000.0); // what it cost
		}
		Hunk_FreeToLowMark (mark);
		if (img)
			return;
	}
	TexMgr_LoadNormalMap (glt, NULL, glt->source_width, glt->source_height, format, data, glt->source_file,
		glt->source_offset, NORMALMAP_SHADING | (flags & (NORMALMAP_HEIGHTS | NORMALMAP_SKIN)), worldwidth);
}


/*
=================
VR_NormalMapSource -- a world texture's RGBA image kept whole for its normal map (on the hunk: the upload mipmaps it in
place), when normal maps are made; else the image itself
=================
*/
extern "C" byte *VR_NormalMapSource (byte *data, enum srcformat fmt, int width, int height)
{
	byte *pristine;
	if (!VR_NormalMaps () || fmt != SRC_RGBA)
		return data;
	pristine = (byte *) Hunk_AllocNoFill (width * height * 4);
	memcpy (pristine, data, width * height * 4);
	return pristine;
}
