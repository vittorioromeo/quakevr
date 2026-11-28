// QVR: the autosave's safety waits and co-op autosaves.
// Moved as it was out of host.c, which includes this file at the same place (one translation unit: the
// same statics, no behaviour change). Not compiled on its own (Quake/vr/iw/README.md; IRONWAIL_DIFF.md).

/*
==================
Host_AutosaveUnsafe

QVR: why an autosave due must wait (player feedback: an autosave made in an ambush trapped them, dying again at each
respawn): 0 safe; 1 a living monster after you (vr_autosave_safe_alerted) or near you and in sight
(vr_autosave_safe_radius): the longest wait (vr_autosave_safe_maxdelay) ends it; 2 your health below
vr_autosave_safe_health percent: only healing ends it. *reason: what, for the log.
==================
*/
static int Host_AutosaveUnsafe (const char **reason)
{
	edict_t		*ed;
	int			i, self;
	float		radius, percent, maxhealth;
	qboolean	alerted;
	vec3_t		eye, mid, d;
	trace_t		tr;

	*reason = "";
	if (!Cvar_VariableValue ("vr_autosave_safe"))
		return 0;
	percent = Cvar_VariableValue ("vr_autosave_safe_health");
	maxhealth = sv_player->v.max_health > 0.f ? sv_player->v.max_health : 100.f;
	if (percent > 0.f && sv_player->v.health < maxhealth * percent / 100.f)
	{
		*reason = "your health is low";
		return 2;
	}
	radius = Cvar_VariableValue ("vr_autosave_safe_radius");
	alerted = Cvar_VariableValue ("vr_autosave_safe_alerted") != 0.f;
	self = EDICT_TO_PROG (sv_player);
	VectorAdd (sv_player->v.origin, sv_player->v.view_ofs, eye);
	for (i = 1, ed = NEXT_EDICT (qcvm->edicts); i < qcvm->num_edicts; i++, ed = NEXT_EDICT (ed))
	{
		// a living monster in the world (not a corpse, nor one yet to appear: AD's spawned later)
		if (ed->free || !((int) ed->v.flags & FL_MONSTER) || ed->v.health <= 0.f || ed->v.deadflag != 0.f ||
			ed->v.solid == SOLID_NOT || !ed->v.modelindex)
			continue;
		if (alerted && ed->v.enemy == self)
		{
			*reason = "a monster is after you";
			return 1;
		}
		if (radius <= 0.f)
			continue;
		VectorSubtract (ed->v.origin, sv_player->v.origin, d);
		if (DotProduct (d, d) > radius * radius)
			continue;
		VectorAdd (ed->v.absmin, ed->v.absmax, mid);
		VectorScale (mid, 0.5f, mid);
		tr = SV_Move (eye, vec3_origin, vec3_origin, mid, MOVE_NOMONSTERS, sv_player);
		if (tr.fraction >= 1.f && !tr.allsolid)
		{
			*reason = "a monster is near, in sight";
			return 1;
		}
	}
	return 0;
}

/*
==================
Host_AutosaveWait

QVR: true while an autosave due waits for it to be safe (Host_AutosaveUnsafe), what it waits for logged as it changes
(developer 1); false when it may be made: safe, or the monsters waited for vr_autosave_safe_maxdelay seconds.
==================
*/
static qboolean Host_AutosaveWaitFor (int unsafe, const char *reason);

static qboolean Host_AutosaveWait (void)
{
	const char	*reason;
	int			unsafe = Host_AutosaveUnsafe (&reason);

	return Host_AutosaveWaitFor (unsafe, reason);
}

/*
==================
Host_CheckCoopAutosave

QVR: a coop game's autosave (MP_PLAN.md B4b): only as each level starts (vr_autosave_level_start, sv_autosave), once
every player in the game is in it and alive and none is in danger (Host_AutosaveUnsafe for each: the longest wait
ends it), on a dedicated server too. Its name is autosave/coop~<the single player's name>, a coop save (format 2):
Continue and Load Autosave pass over it, `load autosave/coop~<map>` loads it. The interval's autosaves follow one
player: none in coop.
==================
*/
static void Host_CheckCoopAutosave (void)
{
	int			i, players = 0, unsafe = 0, u;
	const char	*reason = "", *r;
	client_t	*c;
	edict_t		*own = sv_player;
	char		single[MAX_QPATH], autosavename[MAX_QPATH];

	if (sv.autosave.level_started || !sv_autosave.value || deathmatch.value || !coop.value || qcvm->time < 2.5)
		return;
	if (!Cvar_VariableValue ("vr_autosave_level_start") || !VR_AutosaveAllowed (sv.name) || VR_RunIronman () ||
		COM_PrepareRun ())
	{
		sv.autosave.level_started = true;
		return;
	}
	for (i = 0, c = svs.clients; i < svs.maxclients; i++, c++)
	{
		if (!c->active)
			continue;
		if (!c->spawned || !c->edict || c->edict->v.health <= 0.f)
			return; // (a player still coming in, or dead: not yet)
		sv_player = c->edict;
		u = Host_AutosaveUnsafe (&r);
		sv_player = own;
		if (u > unsafe)
		{
			unsafe = u;
			reason = r;
		}
		players++;
	}
	if (!players || Host_AutosaveWaitFor (unsafe, reason))
		return;
	sv.autosave.level_started = true;
	sv.autosave.time = qcvm->time;
	VR_AutosaveName (sv.name, single, sizeof (single));
	q_snprintf (autosavename, sizeof (autosavename), "autosave/coop~%s", single + strlen ("autosave/"));
	Con_DPrintf ("Autosave: coop, %d players\n", players);
	Cbuf_AddText (va ("save \"%s\" 0 rotate\n", autosavename));
}

static qboolean Host_AutosaveWaitFor (int unsafe, const char *reason)
{
	float		maxdelay = Cvar_VariableValue ("vr_autosave_safe_maxdelay");

	if (!unsafe)
	{
		if (sv.autosave.wait_since)
			Con_DPrintf ("Autosave: safe now, after %.1f s\n", qcvm->time - sv.autosave.wait_since);
		sv.autosave.wait_since = 0.;
		sv.autosave.wait_reason = NULL;
		return false;
	}
	if (!sv.autosave.wait_since)
		sv.autosave.wait_since = qcvm->time;
	if (unsafe == 1 && maxdelay > 0.f && qcvm->time - sv.autosave.wait_since >= maxdelay)
	{
		Con_DPrintf ("Autosave: made anyway after %.0f s (%s)\n", qcvm->time - sv.autosave.wait_since, reason);
		sv.autosave.wait_since = 0.;
		sv.autosave.wait_reason = NULL;
		return false;
	}
	if (sv.autosave.wait_reason != reason)
		Con_DPrintf ("Autosave: waiting: %s\n", reason);
	sv.autosave.wait_reason = reason;
	return true;
}

