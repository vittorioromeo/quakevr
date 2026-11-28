// QVR: the world's lit-surface split by the lights (the lit grid and leaves, C4v).
// Moved as it was out of r_world.c, which includes this file at the same place (one translation unit: the
// same statics, no behaviour change). Not compiled on its own (Quake/vr/iw/README.md; IRONWAIL_DIFF.md).

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
static struct { // QVR: R_WorldLitTextures' last inputs (questperf: the second eye's, still lights' answer reused)
	qboolean	valid;
	int			count;			// its spheres
	int			textures;		// gl_bmodel_litcmd_count then
	vec4_t		spheres[QVR_WORLD_LIT_SPHERES];
	byte		*vis;			// the view's PVS, a bit a leaf
	size_t		viscap;
	byte		*check;			// vr_gl_world_lit_reuse 2: the grid walk's answer, compared with every leaf's
	int			checkcap;
	int			hits, misses, checked, bad;
	double		spheres_n, visleaves, touched, surftests; // the walks' sums (vr_gl_world_lit_stats)
} r_litreuse; // seat-shared: the renderer's lit-world reuse stats
static int r_world_littexcap; // seat-shared: the renderer's lit texture's size
#define LITGRID_DIM		32	// QVR: the lit walk's grid: cells an axis at most
#define LITGRID_SPAN	64	// a leaf whose box covers more cells: tested by every walk (big)
#define LITGRID_CELL	64.f	// the smallest cell's side
static struct { // QVR: R_WorldLitGridWalk's grid of the leaves' boxes (questperf; built by R_WorldLitLeavesBuild)
	qboolean	valid;
	int			dims[3];
	float		origin[3];
	float		inv;			// 1 / a cell's side
	int			*start;			// a cell's first entry in list (cells + 1)
	int			startcap;
	int			*list;			// the leaves (index i: leafs[i + 1]) in each cell
	int			listcap;
	int			*big;			// the leaves over more than LITGRID_SPAN cells
	int			bigcount, bigcap;
	int			*stamp;			// a leaf's last walk (each leaf tested once a walk)
	int			stampcap;
	int			walk;
} r_litgrid; // seat-shared: the renderer's leaf grid (scratch)
static float (*r_litleafbox)[6];	// each leaf's (leafs[i + 1]) box round its surfaces' (mins, maxs; empty: mins > maxs)
static int r_litleafcount, r_litleafcap;
static int *r_litsurftex;			// each world surface's lit command (its texture's index; -1: not split)
static int r_litsurfcap;
static qboolean r_world_litskip;	// this view's: the empty lit commands skipped (vr_gl_world_lit_skip)

/*
===============
R_WorldLitGridCell -- QVR

The grid's cell along axis k holding the coordinate v (outside the grid: below 0 or from dims[k] on).
===============
*/
static int R_WorldLitGridCell (float v, int k)
{
	const float c = floorf ((v - r_litgrid.origin[k]) * r_litgrid.inv);
	return c < -1.f ? -1 : c > (float)LITGRID_DIM ? LITGRID_DIM : (int)c;
}

/*
===============
R_WorldLitGridBuild -- QVR

questperf: the leaves' boxes (r_litleafbox) put in a grid of at most LITGRID_DIM cells an axis over the world's
leaves, each leaf in every cell its box covers (one over more than LITGRID_SPAN cells kept apart as big).
===============
*/
static void R_WorldLitGridBuild (void)
{
	float mins[3] = {1e30f, 1e30f, 1e30f}, maxs[3] = {-1e30f, -1e30f, -1e30f}, side;
	int i, k, x, y, z, cells, entries = 0, lo[3], hi[3];
	int *cursor;
	r_litgrid.valid = false;
	r_litgrid.bigcount = 0;
	for (i = 0; i < r_litleafcount; i++)
	{
		const float *box = r_litleafbox[i];
		if (box[0] > box[3])
			continue;
		for (k = 0; k < 3; k++)
		{
			mins[k] = q_min (mins[k], box[k]);
			maxs[k] = q_max (maxs[k], box[3 + k]);
		}
	}
	if (mins[0] > maxs[0])
		return;
	side = q_max (q_max (maxs[0] - mins[0], maxs[1] - mins[1]), maxs[2] - mins[2]) / LITGRID_DIM;
	side = q_max (side, LITGRID_CELL);
	r_litgrid.inv = 1.f / side;
	for (k = 0; k < 3; k++)
	{
		r_litgrid.origin[k] = mins[k];
		r_litgrid.dims[k] = CLAMP (1, (int)ceilf ((maxs[k] - mins[k]) * r_litgrid.inv), LITGRID_DIM);
	}
	cells = r_litgrid.dims[0] * r_litgrid.dims[1] * r_litgrid.dims[2];
	if (cells + 1 > r_litgrid.startcap)
	{
		free (r_litgrid.start);
		r_litgrid.startcap = cells + 1;
		r_litgrid.start = (int *) malloc (sizeof (int) * r_litgrid.startcap);
		if (!r_litgrid.start)
			Sys_Error ("R_WorldLitGridBuild: out of memory (%d cells)", cells);
	}
	if (r_litleafcount > r_litgrid.bigcap)
	{
		free (r_litgrid.big);
		free (r_litgrid.stamp);
		r_litgrid.bigcap = r_litgrid.stampcap = r_litleafcount;
		r_litgrid.big = (int *) malloc (sizeof (int) * r_litgrid.bigcap);
		r_litgrid.stamp = (int *) malloc (sizeof (int) * r_litgrid.stampcap);
		if (!r_litgrid.big || !r_litgrid.stamp)
			Sys_Error ("R_WorldLitGridBuild: out of memory (%d leaves)", r_litleafcount);
	}
	memset (r_litgrid.stamp, 0, sizeof (int) * r_litleafcount);
	r_litgrid.walk = 0;
	memset (r_litgrid.start, 0, sizeof (int) * (cells + 1));
	for (i = 0; i < r_litleafcount; i++) // (the cells' counts, at start[c + 1])
	{
		const float *box = r_litleafbox[i];
		if (box[0] > box[3])
			continue;
		for (k = 0; k < 3; k++)
		{
			lo[k] = CLAMP (0, R_WorldLitGridCell (box[k], k), r_litgrid.dims[k] - 1);
			hi[k] = CLAMP (0, R_WorldLitGridCell (box[3 + k], k), r_litgrid.dims[k] - 1);
		}
		if ((hi[0] - lo[0] + 1) * (hi[1] - lo[1] + 1) * (hi[2] - lo[2] + 1) > LITGRID_SPAN)
		{
			r_litgrid.big[r_litgrid.bigcount++] = i;
			continue;
		}
		for (z = lo[2]; z <= hi[2]; z++)
			for (y = lo[1]; y <= hi[1]; y++)
				for (x = lo[0]; x <= hi[0]; x++)
				{
					r_litgrid.start[(z * r_litgrid.dims[1] + y) * r_litgrid.dims[0] + x + 1]++;
					entries++;
				}
	}
	for (i = 0; i < cells; i++)
		r_litgrid.start[i + 1] += r_litgrid.start[i];
	if (entries > r_litgrid.listcap)
	{
		free (r_litgrid.list);
		r_litgrid.listcap = entries;
		r_litgrid.list = (int *) malloc (sizeof (int) * r_litgrid.listcap);
		if (!r_litgrid.list)
			Sys_Error ("R_WorldLitGridBuild: out of memory (%d entries)", entries);
	}
	cursor = (int *) malloc (sizeof (int) * cells);
	if (!cursor)
		Sys_Error ("R_WorldLitGridBuild: out of memory (%d cells)", cells);
	memcpy (cursor, r_litgrid.start, sizeof (int) * cells);
	for (i = 0; i < r_litleafcount; i++) // (the same cells as counted)
	{
		const float *box = r_litleafbox[i];
		if (box[0] > box[3])
			continue;
		for (k = 0; k < 3; k++)
		{
			lo[k] = CLAMP (0, R_WorldLitGridCell (box[k], k), r_litgrid.dims[k] - 1);
			hi[k] = CLAMP (0, R_WorldLitGridCell (box[3 + k], k), r_litgrid.dims[k] - 1);
		}
		if ((hi[0] - lo[0] + 1) * (hi[1] - lo[1] + 1) * (hi[2] - lo[2] + 1) > LITGRID_SPAN)
			continue;
		for (z = lo[2]; z <= hi[2]; z++)
			for (y = lo[1]; y <= hi[1]; y++)
				for (x = lo[0]; x <= hi[0]; x++)
					r_litgrid.list[cursor[(z * r_litgrid.dims[1] + y) * r_litgrid.dims[0] + x]++] = i;
	}
	free (cursor);
	r_litgrid.valid = true;
}

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
	r_litreuse.valid = false; // QVR: (questperf: a new map's leaves and textures)
	r_litgrid.valid = false;
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
	R_WorldLitGridBuild (); // QVR: (questperf)
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
R_WorldLitLeaf -- QVR

R_WorldLitTextures' test of the world's leaf i (leafs[i + 1]): in the view's PVS and its box touched by a sphere, the
lit commands of its surfaces a sphere touches marked.
===============
*/
static void R_WorldLitLeaf (int i, const byte *vis, const vec4_t *spheres, int count)
{
	const float *box = r_litleafbox[i];
	const mleaf_t *leaf;
	int j, l, k;
	if (!(vis[i >> 3] & (1 << (i & 7))) || box[0] > box[3])
		return;
	r_litreuse.visleaves++;
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
		return;
	r_litreuse.touched++;
	leaf = &cl.worldmodel->leafs[i + 1];
	for (j = 0; j < leaf->nummarksurfaces; j++)
	{
		const msurface_t *surf = &cl.worldmodel->surfaces[leaf->firstmarksurface[j]];
		const int tex = r_litsurftex[leaf->firstmarksurface[j]];
		if (tex < 0 || r_world_littex[tex])
			continue;
		r_litreuse.surftests++;
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

/*
===============
R_WorldLitGridWalk -- QVR

questperf: R_WorldLitLeaf for the leaves in the grid's cells round each sphere (and the big leaves), each once: the
leaves whose box a sphere touches are all among them (each sits in every cell its box covers; the cells round a sphere
taken a unit wider than the test), so the answer is the walk over every leaf's. The hub: 8 spheres, ~30,000 leaves
visible, ~1,200 touched.
===============
*/
static void R_WorldLitGridWalk (const byte *vis, const vec4_t *spheres, int count)
{
	int b, l, k, x, y, z, lo[3], hi[3];
	if (++r_litgrid.walk == INT_MAX)
	{
		memset (r_litgrid.stamp, 0, sizeof (r_litgrid.stamp[0]) * r_litleafcount);
		r_litgrid.walk = 1;
	}
	for (b = 0; b < r_litgrid.bigcount; b++)
		R_WorldLitLeaf (r_litgrid.big[b], vis, spheres, count);
	for (l = 0; l < count; l++)
	{
		const float *s = spheres[l];
		const float r = s[3] + 2.f;
		for (k = 0; k < 3; k++)
		{
			lo[k] = R_WorldLitGridCell (s[k] - r, k);
			hi[k] = R_WorldLitGridCell (s[k] + r, k);
			if (hi[k] < 0 || lo[k] >= r_litgrid.dims[k])
				break;
			lo[k] = q_max (lo[k], 0);
			hi[k] = q_min (hi[k], r_litgrid.dims[k] - 1);
		}
		if (k < 3)
			continue;
		for (z = lo[2]; z <= hi[2]; z++)
			for (y = lo[1]; y <= hi[1]; y++)
				for (x = lo[0]; x <= hi[0]; x++)
				{
					const int c = (z * r_litgrid.dims[1] + y) * r_litgrid.dims[0] + x;
					int n;
					for (n = r_litgrid.start[c]; n < r_litgrid.start[c + 1]; n++)
					{
						const int i = r_litgrid.list[n];
						if (r_litgrid.stamp[i] == r_litgrid.walk)
							continue;
						r_litgrid.stamp[i] = r_litgrid.walk;
						R_WorldLitLeaf (i, vis, spheres, count);
					}
				}
	}
}

/*
===============
R_WorldLitTextures -- QVR

C4w: which of the world's lit commands the GPU's cull can send a surface to this view: a texture with a surface whose
box a sphere touches (the cull's own test, the radius a unit wider) in a leaf of the view's PVS. A superset: the cull
also drops back faces and what is out of the frustum. Their texture index as the cull's (R_AddBModelPassCalls' j).
questperf (vr_gl_world_lit_reuse): 1 the last answer given again while the PVS and the spheres are the same (the
second eye; lights that stay put), else the grid's walk (R_WorldLitGridWalk); 0 every leaf walked for each view, as
C4w; 2 the grid's walk and every leaf's both, compared (vr_gl_world_lit_stats: "differed" stays 0).
===============
*/
static qboolean R_WorldLitTextures (const byte *vis, const vec4_t *spheres, int count)
{
	int i;
	const size_t vissize = (size_t)((r_litleafcount + 7) >> 3);
	const int mode = VR_WorldLitReuse ();
	if (!r_world_littex || !r_litsurftex || r_litleafcount != cl.worldmodel->numleafs || !vis)
		return false;
	count = q_min (q_max (count, 0), QVR_WORLD_LIT_SPHERES);
	if (mode == 1 && r_litreuse.valid && count == r_litreuse.count && r_litreuse.textures == gl_bmodel_litcmd_count &&
		!memcmp (spheres, r_litreuse.spheres, sizeof (spheres[0]) * count) && !memcmp (vis, r_litreuse.vis, vissize))
	{
		r_litreuse.hits++;
		return true;
	}
	r_litreuse.misses++;
	r_litreuse.spheres_n += count;
	memset (r_world_littex, 0, gl_bmodel_litcmd_count);
	if (mode && r_litgrid.valid)
		R_WorldLitGridWalk (vis, spheres, count);
	else
		for (i = 0; i < r_litleafcount; i++)
			R_WorldLitLeaf (i, vis, spheres, count);
	if (mode == 2 && r_litgrid.valid)
	{
		if (r_litreuse.checkcap < gl_bmodel_litcmd_count)
		{
			free (r_litreuse.check);
			r_litreuse.checkcap = gl_bmodel_litcmd_count;
			r_litreuse.check = (byte *) malloc (r_litreuse.checkcap);
			if (!r_litreuse.check)
				Sys_Error ("R_WorldLitTextures: out of memory (%d textures)", r_litreuse.checkcap);
		}
		memcpy (r_litreuse.check, r_world_littex, gl_bmodel_litcmd_count);
		memset (r_world_littex, 0, gl_bmodel_litcmd_count);
		for (i = 0; i < r_litleafcount; i++)
			R_WorldLitLeaf (i, vis, spheres, count);
		r_litreuse.checked++;
		if (memcmp (r_litreuse.check, r_world_littex, gl_bmodel_litcmd_count))
			r_litreuse.bad++;
	}
	if (vissize > r_litreuse.viscap)
	{
		free (r_litreuse.vis);
		r_litreuse.viscap = vissize;
		r_litreuse.vis = (byte *) malloc (r_litreuse.viscap);
		if (!r_litreuse.vis)
			Sys_Error ("R_WorldLitTextures: out of memory (%d leaves)", r_litleafcount);
	}
	memcpy (r_litreuse.vis, vis, vissize);
	memcpy (r_litreuse.spheres, spheres, sizeof (spheres[0]) * count);
	r_litreuse.count = count;
	r_litreuse.textures = gl_bmodel_litcmd_count;
	r_litreuse.valid = true;
	return true;
}

/*
===============
R_WorldLitReuseStats_f -- QVR

vr_gl_world_lit_stats: how often R_WorldLitTextures reused its last answer (vr_gl_world_lit_reuse), and with 2 how
often working it out again gave another one (always 0: the reuse is exact). "reset" zeroes the counts.
===============
*/
void R_WorldLitReuseStats_f (void)
{
	if (Cmd_Argc () > 1 && !q_strcasecmp (Cmd_Argv (1), "reset"))
	{
		r_litreuse.hits = r_litreuse.misses = r_litreuse.checked = r_litreuse.bad = 0;
		r_litreuse.spheres_n = r_litreuse.visleaves = r_litreuse.touched = r_litreuse.surftests = 0.0;
		return;
	}
	Con_Printf ("vr_gl_world_lit_stats: %d reused, %d worked out, %d checked, %d differed (vr_gl_world_lit_reuse %d; "
		"grid %dx%dx%d, %d big leaves)\n", r_litreuse.hits, r_litreuse.misses, r_litreuse.checked, r_litreuse.bad,
		VR_WorldLitReuse (), r_litgrid.dims[0], r_litgrid.dims[1], r_litgrid.dims[2], r_litgrid.bigcount);
	if (r_litreuse.misses)
	{
		const double n = r_litreuse.misses;
		Con_Printf ("  a walk: %.1f spheres, %.0f of %d leaves visible, %.0f touched, %.0f surfaces tested\n",
			r_litreuse.spheres_n / n, r_litreuse.visleaves / n, r_litleafcount, r_litreuse.touched / n, r_litreuse.surftests / n);
	}
}

