// QVR: the pushers' candidate grid (sv_fastpushmove).
// Moved as it was out of sv_phys.c, which includes this file at the same place (one translation unit: the
// same statics, no behaviour change). Not compiled on its own (Quake/vr/iw/README.md; IRONWAIL_DIFF.md).

/*
===============================================================================

QVR: THE PUSHERS' CANDIDATE GRID (after vkQuake af387967, d6bf6c4c, 32f74820)

SV_PushMove tests every edict against each moving pusher: on a big map (2000+ edicts, each its own page of fields)
with many trains, doors and plats that is most of the server's time. From the second pusher moving in a tick, a
spatial hash of every edict's abs box (256-unit cells) hands each push only the edicts near its sweep instead.

Exact, by construction: the candidates are a superset of what the walk would act on, visited in edict order, each
tested exactly as the walk tests it (free, movetype, riding, hanging, the box, Box3D's skip):
- the grid holds every edict but MOVETYPE_PUSH ones (and big NONE/NOCLIP brushes: triggers, func_walls) in the cells
  of its abs box as it was at the build, and again wherever SV_LinkEdict puts it later in the tick (the only place
  the box changes: a stale entry costs one test); boxes spanning 4+ cells and edicts allocated this tick (no box
  yet) are always candidates, and so are the clients (hanging from a pusher at arm's length: VR_ClimbHangsFrom);
- the query is the pusher's whole sweep (riders sit on its old box) plus a slack for riders whose ground contact
  outlives a small gap;
- an edict past the one being tested that is linked or allocated during the push (touch functions, blocked) gets
  the rest of the list gathered again, as the walk would still reach it.
sv_fastpushmove 0 walks every edict as Quake does (to compare); 2 uses the grid from the first pusher moving and
checks every push's candidates against the walk (a miss warned of; sv_pushgrid_stats counts them).
===============================================================================
*/
cvar_t sv_fastpushmove = {"sv_fastpushmove", "1", CVAR_NONE}; // QVR: 1 = the pushers' candidate grid, 0 = Quake's walk

#define PUSHGRID_CELL_SHIFT	8		// 256-unit cells
#define PUSHGRID_BUCKETS	4096	// hashed cells (a collision only adds candidates)
#define PUSHGRID_MAX_SPAN	4		// a box spanning this many cells on an axis is always a candidate
#define PUSHGRID_MAX_QUERY	1024	// a sweep over more cells walks instead
#define PUSHGRID_SLACK		8.f		// units round the sweep (riders)

typedef struct
{
	edict_t	*ent;
	int		next;
} pushgrid_entry_t;

typedef struct
{
	int					heads[PUSHGRID_BUCKETS];
	pushgrid_entry_t	*entries;
	int					numentries, maxentries;
	edict_t				**always;	// big boxes, edicts allocated since the build
	int					numalways, maxalways;
	edict_t				**cand;		// SV_PushMove's candidates, in edict order
	int					numcand, maxcand;
	int					movers;		// pushers moved this tick (the grid is built at the second)
	qboolean			active;		// built for this tick
	edict_t				*cursor;	// the edict SV_PushMove is testing (NULL outside it)
	qboolean			regather;	// an edict past the cursor moved or came: gather the rest again
} pushgrid_t;

static pushgrid_t pushgrid;
static int sv_pushgrid_builds, sv_pushgrid_pushes, sv_pushgrid_cands; // sv_pushgrid_stats's counts
static int sv_pushgrid_checked, sv_pushgrid_missed; // sv_fastpushmove 2's: pushes checked, candidates missed

static void *PushGrid_Reserve (void *p, int *max, int need, size_t size)
{
	if (need <= *max)
		return p;
	*max = q_max (need, q_max (*max * 2, 1024));
	p = realloc (p, (size_t)*max * size);
	if (!p)
		Sys_Error ("PushGrid: out of memory (%d)", *max);
	return p;
}

static int PushGrid_Cell (float v)
{
	const float limit = (float)(1 << 23); // NaN and huge origins from broken QC never reach the int conversion
	if (!(v >= -limit))
		v = -limit;
	else if (v > limit)
		v = limit;
	return ((int)floorf (v)) >> PUSHGRID_CELL_SHIFT;
}

static unsigned PushGrid_Bucket (int x, int y, int z)
{
	return ((unsigned)x * 73856093u ^ (unsigned)y * 19349663u ^ (unsigned)z * 83492791u) & (PUSHGRID_BUCKETS - 1);
}

static void PushGrid_AddAlways (edict_t *ent)
{
	pushgrid.always = (edict_t **)PushGrid_Reserve (pushgrid.always, &pushgrid.maxalways, pushgrid.numalways + 1, sizeof (edict_t *));
	pushgrid.always[pushgrid.numalways++] = ent;
}

// returns true when the edict went in (a candidate from now on)
static qboolean PushGrid_Insert (edict_t *ent)
{
	int lo[3], hi[3], i, x, y, z;

	if (ent->free || ent->v.movetype == MOVETYPE_PUSH)
		return false;
	for (i = 0; i < 3; i++)
	{
		lo[i] = PushGrid_Cell (ent->v.absmin[i]);
		hi[i] = PushGrid_Cell (ent->v.absmax[i]);
	}
	if (hi[0] - lo[0] >= PUSHGRID_MAX_SPAN || hi[1] - lo[1] >= PUSHGRID_MAX_SPAN || hi[2] - lo[2] >= PUSHGRID_MAX_SPAN || hi[0] < lo[0] || hi[1] < lo[1] || hi[2] < lo[2])
	{
		if (ent->v.movetype == MOVETYPE_NONE || ent->v.movetype == MOVETYPE_NOCLIP)
			return false; // a trigger's or a func_wall's brush: never pushed
		PushGrid_AddAlways (ent);
		return true;
	}
	pushgrid.entries = (pushgrid_entry_t *)PushGrid_Reserve (pushgrid.entries, &pushgrid.maxentries,
		pushgrid.numentries + (hi[0] - lo[0] + 1) * (hi[1] - lo[1] + 1) * (hi[2] - lo[2] + 1), sizeof (pushgrid_entry_t));
	for (x = lo[0]; x <= hi[0]; x++)
		for (y = lo[1]; y <= hi[1]; y++)
			for (z = lo[2]; z <= hi[2]; z++)
			{
				int *head = &pushgrid.heads[PushGrid_Bucket (x, y, z)];
				pushgrid.entries[pushgrid.numentries].ent = ent;
				pushgrid.entries[pushgrid.numentries].next = *head;
				*head = pushgrid.numentries++;
			}
	return true;
}

static void PushGrid_Build (void)
{
	int e;
	edict_t *ent;

	VR_TRACY_SCOPE ("SV push grid build");
	memset (pushgrid.heads, 0xff, sizeof (pushgrid.heads));
	pushgrid.numentries = pushgrid.numalways = 0;
	for (e = 1, ent = NEXT_EDICT (qcvm->edicts); e < qcvm->num_edicts; e++, ent = NEXT_EDICT (ent))
		PushGrid_Insert (ent);
	pushgrid.active = true;
	sv_pushgrid_builds++;
}

// SV_LinkEdict, the abs box set: the edict's new cells
void SV_PushGridLinked (edict_t *ent)
{
	if (!pushgrid.active || qcvm != &sv.qcvm)
		return;
	if (PushGrid_Insert (ent) && pushgrid.cursor && ent > pushgrid.cursor)
		pushgrid.regather = true;
}

// ED_Alloc: a new edict (no box yet) is a candidate for the rest of the tick
void SV_PushGridAllocated (edict_t *ent)
{
	if (!pushgrid.active || qcvm != &sv.qcvm)
		return;
	PushGrid_AddAlways (ent);
	if (pushgrid.cursor && ent > pushgrid.cursor)
		pushgrid.regather = true;
}

static void PushGrid_AddCand (edict_t *ent, edict_t *after)
{
	if (ent <= after)
		return;
	pushgrid.cand = (edict_t **)PushGrid_Reserve (pushgrid.cand, &pushgrid.maxcand, pushgrid.numcand + 1, sizeof (edict_t *));
	pushgrid.cand[pushgrid.numcand++] = ent;
}

static int PushGrid_CompareEdicts (const void *a, const void *b)
{
	uintptr_t pa = (uintptr_t)*(edict_t *const *)a, pb = (uintptr_t)*(edict_t *const *)b;
	return (pa > pb) - (pa < pb);
}

// The edicts past "after" near the box (edict order, each once) into pushgrid.cand; false: walk instead.
static qboolean PushGrid_Gather (const float *mins, const float *maxs, edict_t *after)
{
	int lo[3], hi[3], i, x, y, z, k, unique;

	for (i = 0; i < 3; i++)
	{
		lo[i] = PushGrid_Cell (mins[i] - PUSHGRID_SLACK);
		hi[i] = PushGrid_Cell (maxs[i] + PUSHGRID_SLACK);
		if (hi[i] < lo[i] || hi[i] - lo[i] >= PUSHGRID_MAX_QUERY)
			return false;
	}
	if ((hi[0] - lo[0] + 1) * (hi[1] - lo[1] + 1) * (hi[2] - lo[2] + 1) > PUSHGRID_MAX_QUERY)
		return false;

	pushgrid.numcand = 0;
	for (x = lo[0]; x <= hi[0]; x++)
		for (y = lo[1]; y <= hi[1]; y++)
			for (z = lo[2]; z <= hi[2]; z++)
				for (k = pushgrid.heads[PushGrid_Bucket (x, y, z)]; k >= 0; k = pushgrid.entries[k].next)
					PushGrid_AddCand (pushgrid.entries[k].ent, after);
	for (k = 0; k < pushgrid.numalways; k++)
		PushGrid_AddCand (pushgrid.always[k], after);
	for (k = 1; k <= svs.maxclients && k < qcvm->num_edicts; k++)
		PushGrid_AddCand (EDICT_NUM (k), after);

	if (pushgrid.numcand > 1)
		qsort (pushgrid.cand, pushgrid.numcand, sizeof (edict_t *), PushGrid_CompareEdicts);
	for (k = 0, unique = 0; k < pushgrid.numcand; k++)
		if (!unique || pushgrid.cand[unique - 1] != pushgrid.cand[k])
			pushgrid.cand[unique++] = pushgrid.cand[k];
	pushgrid.numcand = unique;
	return true;
}

// sv_fastpushmove 2: every edict the walk would test past its box check (riding, hanging, overlapping the final box)
// looked up in the candidates; a missing one warned of (a bug in the grid's bookkeeping)
static void PushGrid_Verify (edict_t *pusher, const float *mins, const float *maxs)
{
	int e;
	edict_t *check;

	sv_pushgrid_checked++;
	for (e = 1, check = NEXT_EDICT (qcvm->edicts); e < qcvm->num_edicts; e++, check = NEXT_EDICT (check))
	{
		qboolean act;
		if (check->free || check->v.movetype == MOVETYPE_PUSH || check->v.movetype == MOVETYPE_NONE || check->v.movetype == MOVETYPE_NOCLIP)
			continue;
		act = (((int)check->v.flags & FL_ONGROUND) && PROG_TO_EDICT (check->v.groundentity) == pusher) ||
			(e <= svs.maxclients && VR_ClimbHangsFrom (check, pusher)) ||
			!(check->v.absmin[0] >= maxs[0] || check->v.absmin[1] >= maxs[1] || check->v.absmin[2] >= maxs[2] ||
			  check->v.absmax[0] <= mins[0] || check->v.absmax[1] <= mins[1] || check->v.absmax[2] <= mins[2]);
		if (act && !bsearch (&check, pushgrid.cand, pushgrid.numcand, sizeof (edict_t *), PushGrid_CompareEdicts))
		{
			sv_pushgrid_missed++;
			Con_Warning ("sv_fastpushmove: %s #%d missed by %s #%d's candidates\n", PR_GetString (check->v.classname), e,
				PR_GetString (pusher->v.classname), NUM_FOR_EDICT (pusher));
		}
	}
}

// sv_pushgrid_stats [reset]: the grid's counts since the last reset
void SV_PushGridStats_f (void)
{
	Con_Printf ("pushgrid: %d grid builds, %d pushes from the grid (%.1f candidates each), %d checked, %d missed\n",
		sv_pushgrid_builds, sv_pushgrid_pushes, sv_pushgrid_pushes ? (double)sv_pushgrid_cands / sv_pushgrid_pushes : 0.0,
		sv_pushgrid_checked, sv_pushgrid_missed);
	if (Cmd_Argc () >= 2 && !q_strcasecmp (Cmd_Argv (1), "reset"))
		sv_pushgrid_builds = sv_pushgrid_pushes = sv_pushgrid_cands = sv_pushgrid_checked = sv_pushgrid_missed = 0;
}

// SV_Physics: the grid is per tick
static void PushGrid_EndTick (void)
{
	pushgrid.active = false;
	pushgrid.movers = 0;
	pushgrid.cursor = NULL;
	pushgrid.regather = false;
}

