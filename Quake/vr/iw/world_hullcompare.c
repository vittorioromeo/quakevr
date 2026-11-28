// QVR: sv_hulltrace_compare: the hull traces checked against each other.
// Moved as it was out of world.c, which includes this file at the same place (one translation unit: the
// same statics, no behaviour change). Not compiled on its own (Quake/vr/iw/README.md; IRONWAIL_DIFF.md).

/*
==================
QVR: sv_hulltrace_compare [traces] [seed]: the same seeded random moves through the map's point and player hulls
(starts out of solid, up to 512 units long) traced by Quake's and by FTE's: how many differ (a hit against a miss,
the end more than 1/8 unit apart, all-solid), how many of each end inside solid (a move that would stick), and each
one's time. Debug > Profiling and Memory, "Compare Hull Traces".
==================
*/
static float SV_HullCompareRand (unsigned *s)
{
	*s = *s * 1664525u + 1013904223u;
	return (float)((*s >> 8) & 0xffffff) / (float)0x1000000;
}

static void SV_HullTraceCompare_f (void)
{
	int			n = Cmd_Argc () >= 2 ? Q_atoi (Cmd_Argv (1)) : 100000;
	unsigned	seed = Cmd_Argc () >= 3 ? (unsigned)Q_atoi (Cmd_Argv (2)) : 7u;
	int			h, k, i;
	qmodel_t	*w = sv.active ? sv.worldmodel : NULL;

	if (!w || n <= 0)
	{
		Con_Printf ("sv_hulltrace_compare: no map\n");
		return;
	}
	for (h = 0; h < 2; h++)
	{
		hull_t	*hull = &w->hulls[h];
		int		made = 0, hitdiff = 0, enddiff = 0, soliddiff = 0, stuck[2] = {0, 0};
		double	time[2] = {0, 0};
		vec3_t	*from = (vec3_t *)malloc ((size_t)n * sizeof (vec3_t)), *to = (vec3_t *)malloc ((size_t)n * sizeof (vec3_t));
		trace_t	*res = (trace_t *)malloc ((size_t)n * 2 * sizeof (trace_t));
		unsigned s = seed;
		int		tries = 0;

		if (!from || !to || !res)
			Sys_Error ("sv_hulltrace_compare: out of memory");
		while (made < n && tries++ < n * 50)
		{
			vec3_t p;
			for (i = 0; i < 3; i++)
				p[i] = w->mins[i] + (w->maxs[i] - w->mins[i]) * SV_HullCompareRand (&s);
			if (SV_HullPointContents (hull, hull->firstclipnode, p) == CONTENTS_SOLID)
				continue;
			VectorCopy (p, from[made]);
			for (i = 0; i < 3; i++)
				to[made][i] = p[i] + (SV_HullCompareRand (&s) * 2.f - 1.f) * 512.f;
			made++;
		}
		for (k = 0; k < 2; k++)
		{
			double t0 = Sys_DoubleTime ();
			for (i = 0; i < made; i++)
			{
				trace_t *t = &res[i * 2 + k];
				memset (t, 0, sizeof (*t));
				t->fraction = 1;
				t->allsolid = true;
				VectorCopy (to[i], t->endpos);
				if (k == 0)
					SV_QuakeHullCheck (hull, hull->firstclipnode, 0, 1, from[i], to[i], t);
				else
					SV_FteHullCheck (hull, hull->firstclipnode, from[i], to[i], t);
			}
			time[k] = Sys_DoubleTime () - t0;
		}
		for (i = 0; i < made; i++)
		{
			trace_t *a = &res[i * 2], *b = &res[i * 2 + 1];
			vec3_t d;
			if ((a->fraction < 1) != (b->fraction < 1))
				hitdiff++;
			VectorSubtract (a->endpos, b->endpos, d);
			if (DotProduct (d, d) > 0.125f * 0.125f)
				enddiff++;
			if (a->allsolid != b->allsolid || a->startsolid != b->startsolid)
				soliddiff++;
			for (k = 0; k < 2; k++)
				if (!res[i * 2 + k].allsolid && SV_HullPointContents (hull, hull->firstclipnode, res[i * 2 + k].endpos) == CONTENTS_SOLID)
					stuck[k]++;
		}
		Con_Printf ("hulltrace: hull %d, %d traces: %d hit/miss differ, %d ends > 1/8 apart, %d solid flags differ; "
			"ends in solid: quake %d, fte %d; time: quake %.1f ms, fte %.1f ms\n",
			h, made, hitdiff, enddiff, soliddiff, stuck[0], stuck[1], time[0] * 1000.0, time[1] * 1000.0);
		free (from);
		free (to);
		free (res);
	}
}

void SV_HullTraceInit (void)
{
	Cvar_RegisterVariable (&sv_fte_hulltrace);
	Cmd_AddCommand ("sv_hulltrace_compare", SV_HullTraceCompare_f);
}

