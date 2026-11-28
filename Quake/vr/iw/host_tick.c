// QVR: the server's fixed tick and its stats (host_fixedtick).
// Moved as it was out of host.c, which includes this file at the same place (one translation unit: the
// same statics, no behaviour change). Not compiled on its own (Quake/vr/iw/README.md; IRONWAIL_DIFF.md).

/*
================
QVR: the server's tick (host_fixedtick; ROUND21.md, "Server tick rate")
================
*/
#define TICKSTATS_BINS 80	// the ticks' lengths in half milliseconds, up to 40 ms
#define TICKSTATS_PATTERN 48	// the last frames' tick counts

static struct
{
	int		frames, ticks;
	double	frametime;		// the host frames' time (real seconds)
	double	ticktime;		// the ticks' (real seconds, before slow motion)
	double	tickmin, tickmax;
	int		perframe[5];	// frames with 0, 1, 2, 3, 4+ ticks
	int		lengths[TICKSTATS_BINS + 1];
	int		gap, maxgap;	// frames in a row without a tick
	double	sincetick, wallmin, wallmax; // the time between frames that ticked
	char	pattern[TICKSTATS_PATTERN + 1];
	int		box3dsteps;
	double	svtime;			// the server's clock (slow motion's ticks are shorter)
	int		spread;			// frames that ran fewer ticks than were due (host_fixedtick_spread)
} tickstats;

static void Host_TickStatsReset (void)
{
	memset (&tickstats, 0, sizeof (tickstats));
	tickstats.tickmin = tickstats.wallmin = 1e9;
	tickstats.sincetick = -1.0;
	tickstats.box3dsteps = VR_Box3DSteps ();
	tickstats.svtime = sv.active ? sv.qcvm.time : 0.0;
}

static void Host_TickStatsFrame (double frametime, int ticks, double ticklen)
{
	int i;
	if (!tickstats.tickmin)
		Host_TickStatsReset ();
	tickstats.frames++;
	tickstats.frametime += frametime;
	tickstats.perframe[q_min (ticks, 4)]++;
	memmove (tickstats.pattern, tickstats.pattern + 1, TICKSTATS_PATTERN - 1);
	tickstats.pattern[TICKSTATS_PATTERN - 1] = ticks < 10 ? '0' + ticks : '+';
	if (tickstats.sincetick >= 0.0)
		tickstats.sincetick += frametime;
	if (!ticks)
	{
		tickstats.maxgap = q_max (tickstats.maxgap, ++tickstats.gap);
		return;
	}
	tickstats.gap = 0;
	if (tickstats.sincetick >= 0.0)
	{
		tickstats.wallmin = q_min (tickstats.wallmin, tickstats.sincetick);
		tickstats.wallmax = q_max (tickstats.wallmax, tickstats.sincetick);
	}
	tickstats.sincetick = 0.0;
	for (i = 0; i < ticks; i++)
	{
		tickstats.ticks++;
		tickstats.ticktime += ticklen;
		tickstats.tickmin = q_min (tickstats.tickmin, ticklen);
		tickstats.tickmax = q_max (tickstats.tickmax, ticklen);
		tickstats.lengths[CLAMP (0, (int)(ticklen * 2000.0), TICKSTATS_BINS)]++;
	}
}

/*
================
Host_TickStats_f -- QVR: the server's ticks since the last call (or `host_tickstats reset`): ticks a second, their
lengths, the frames' tick counts, Box3D's steps
================
*/
static void Host_TickStats_f (void)
{
	int i;
	if (!tickstats.tickmin)
		Host_TickStatsReset ();
	if (Cmd_Argc () < 2 || q_strcasecmp (Cmd_Argv (1), "reset"))
	{
		const double secs = q_max (tickstats.frametime, 1e-9);
		Con_Printf ("ticks: %s, interval %.6f ms (%.3f Hz), tolerance %.2f ms\n",
			!host_netinterval ? "every frame (host_maxfps 72 or less)" : host_fixedtick.value ? "fixed (host_fixedtick 1)" : "old (host_fixedtick 0)",
			host_netinterval * 1000.0, host_netinterval ? 1.0 / host_netinterval : 0.0, host_fixedtick_tolerance.value * 1000.f);
		Con_Printf ("ticks: %d frames in %.3f s (%.2f fps), %d ticks: %.2f ticks/s, %.3f per frame\n",
			tickstats.frames, tickstats.frametime, tickstats.frames / secs, tickstats.ticks, tickstats.ticks / secs,
			tickstats.frames ? (double)tickstats.ticks / tickstats.frames : 0.0);
		if (tickstats.ticks)
			Con_Printf ("ticks: length mean %.3f ms, min %.3f, max %.3f; game time %.3f s of %.3f s\n",
				tickstats.ticktime * 1000.0 / tickstats.ticks, tickstats.tickmin * 1000.0, tickstats.tickmax * 1000.0,
				tickstats.ticktime, tickstats.frametime);
		Con_Printf ("ticks: frames with 0/1/2/3/4+ ticks: %d %d %d %d %d; most frames in a row without one %d; between ticking frames %.2f-%.2f ms\n",
			tickstats.perframe[0], tickstats.perframe[1], tickstats.perframe[2], tickstats.perframe[3], tickstats.perframe[4],
			tickstats.maxgap, tickstats.wallmin < 1e8 ? tickstats.wallmin * 1000.0 : 0.0, tickstats.wallmax * 1000.0);
		Con_Printf ("ticks: frames whose catch-up was spread over the next (host_fixedtick_spread %g): %d\n",
			host_fixedtick_spread.value, tickstats.spread);
		Con_Printf ("ticks: lengths (ms: count):");
		for (i = 0; i <= TICKSTATS_BINS; i++)
			if (tickstats.lengths[i])
				Con_Printf (" %s%.1f: %d", i == TICKSTATS_BINS ? ">=" : "", i * 0.5, tickstats.lengths[i]);
		Con_Printf ("\nticks: the last frames' ticks: %s\n", tickstats.pattern);
		if (sv.active)
			Con_Printf ("ticks: the server's clock went %.3f s (x%.3f the frames')\n", sv.qcvm.time - tickstats.svtime,
				(sv.qcvm.time - tickstats.svtime) / secs);
		Con_Printf ("ticks: Box3D steps %d (%.2f a tick)\n", VR_Box3DSteps () - tickstats.box3dsteps,
			tickstats.ticks ? (double)(VR_Box3DSteps () - tickstats.box3dsteps) / tickstats.ticks : 0.0);
	}
	Host_TickStatsReset ();
}

/*
================
Host_FixedTicks -- QVR: host_fixedtick's ticks for this frame: how many (*len seconds each), the rest of *accum carried
================
*/
static int Host_FixedTicks (double *accum, double *len, double frametime)
{
	const double interval = host_netinterval;
	// QVR: (B24) a dedicated server has no headset to keep pace with: its ticks are all exactly host_netinterval (a
	// lift's step the same every tick, as the clients' replays of their moves have it: MP_PLAN B24)
	const double tol = cls.state == ca_dedicated ? 0.0 : CLAMP (0.0, (double)host_fixedtick_tolerance.value, interval * 0.25);
	const int maxticks = CLAMP (1, (int)host_fixedtick_max.value, 64);
	const double pace = CLAMP (0.0, frametime, 0.2) / interval;
	double rest;
	int n = (int)floor ((*accum + tol) / interval);
	host_tickpace = host_tickpace > 0.0 ? host_tickpace + (pace - host_tickpace) * 0.1 : pace; // (about the last 10 frames)
	if (n <= 0)
		return 0;
	if (n > maxticks) // far behind (a hitch): the rest is dropped, the game slows rather than stalling on catching up
	{
		n = maxticks;
		*accum = n * interval;
	}
	if (host_fixedtick_spread.value)
	{
		const int most = (int)floor (host_tickpace * 1.5) + 1;
		if (n > most) // behind: this frame's share, the rest carried (whole ticks: the time each is given unchanged)
		{
			n = most;
			*len = interval;
			*accum -= n * interval;
			tickstats.spread++;
			return n;
		}
	}
	rest = *accum - n * interval;
	if (fabs (rest) <= tol)
	{
		// Near a whole number of ticks: they take all of it (a few percent longer or shorter), none skipped or doubled
		// against a headset a little off 72 Hz (or 144: a tick every other frame).
		*len = *accum / n;
		*accum = 0.0;
	}
	else
	{
		*len = interval;
		*accum = rest;
	}
	return n;
}

