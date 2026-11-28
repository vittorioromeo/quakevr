// QVR: the base directories found beside the exe and the game folder's root (COM_AddExeBaseDir...).
// Moved as it was out of common.c, which includes this file at the same place (one translation unit: the
// same statics, no behaviour change). Not compiled on its own (Quake/vr/iw/README.md; IRONWAIL_DIFF.md).

/*
=================
COM_AddExeBaseDir

QVR: started without -basedir (QuakeVR.bat, ironwail.exe itself, a launcher's or Steam's entry for it: not the
installer's shortcuts, which pass "-basedir <Quake> -basedir <Quake VR>"), Quake is found as Ironwail finds it (the
current folder, Steam, GOG, Epic) and the exe's own folder, an install's, was no base dir: the quakevr searched was
another one (VR_GameDirectoryRoot: the last base dir that has one), a leftover in the Quake folder (a source build's link
or copy, an old zip install: its progs.dat ran with this engine) or in Ironwail's Saved Games folder (no progs.dat: id's
ran, Quake VR's gameplay off, guns only props). The exe's folder, when its quakevr has the game code, is added as the
last base dir: its quakevr is the one searched, and its config the one saved.
=================
*/
qboolean COM_SamePath (const char *a, const char *b) // QVR: also Cmd_Exec_f, vr_input.cpp
{
	for (;; ++a, ++b)
	{
		while (Sys_IsPathSep (a[0]) && (Sys_IsPathSep (a[1]) || !a[1]))
			++a; // (repeated and trailing separators)
		while (Sys_IsPathSep (b[0]) && (Sys_IsPathSep (b[1]) || !b[1]))
			++b;
		if (!*a || !*b)
			return !*a && !*b;
		if (Sys_IsPathSep (*a) ? !Sys_IsPathSep (*b) : q_tolower (*a) != q_tolower (*b))
			return false;
	}
}

static void COM_AddExeBaseDir (void)
{
	int i;
	const char *dir = host_parms->exedir;
	if (COM_CheckParm ("-basedir") || !dir || !*dir)
		return;
	if (Sys_FileType (va ("%s/quakevr/progs.dat", dir)) != FS_ENT_FILE)
		return;
	for (i = 0; i < com_numbasedirs; i++)
		if (COM_SamePath (com_basedirs[i], dir))
			return;
	COM_AddBaseDir (dir);
	Con_Printf ("VR: no -basedir: the exe's own folder added as the last base dir (its quakevr): %s\n", dir);
}

/*
=================
COM_AddVRGameArg

QVR: Quake VR's own game folder (quakevr: its game code, its data folder's txt files, models, sounds) is mandatory. A start without
"-game quakevr" (ironwail.exe double-clicked, a player's own shortcut, a launcher's entry: id1 alone, every weapon's,
prop's and monster's value 0, "tinyworld" in ROUND26) gets it, where the shipped launches put it: after the campaign
folders given (-game hipnotic/rogue/ad/mg1...: Quake VR's campaigns, mounted by quakevr: "-game ad -game quakevr" as
the Quest's), before the first other mod's ("-game quakevr -game <mod>": the mod over Quake VR, QuakeVR.bat's). No base
dir with Quake VR's game data (a broken install): none added, and the player told (VR_NoteGameDataMissing).
=================
*/
static qboolean COM_HasVRGameData (const char *base)
{
	char path[MAX_OSPATH];
	if (Sys_FileType (va ("%s/quakevr/progs.dat", base)) == FS_ENT_FILE)
		return true;
	q_snprintf (path, sizeof (path), "%s/quakevr/pak0.pak", base);
	return Sys_MatchFileCase (path); // (or PAK0.PAK)
}

// Where "quakevr" goes in a list of game folders (the -game arguments in order; a game command's, the Mods menu's,
// COM_WithVRGame): after the campaign folders leading it (id1, hipnotic, rogue, ad...), before the first other mod's.
// -1: quakevr is in it already.
static int COM_VRGameInsertAt (const char **games, int count)
{
	int i, at = -1;
	for (i = 0; i < count; i++)
	{
		if (!q_strcasecmp (games[i], "quakevr"))
			return -1;
		if (at < 0 && !VR_IsCampaignDirectory (games[i]))
			at = i;
	}
	return at < 0 ? count : at;
}

static void COM_AddVRGameArg (void)
{
	const char *games[MAX_NUM_ARGVS];
	int argat[MAX_NUM_ARGVS];
	int i, n = 0, at, found;
	char lookedin[1024];
	for (i = 1; i < com_argc - 1; i++)
	{
		if (q_strcasecmp (com_argv[i], "-game"))
			continue;
		argat[n] = i;
		games[n++] = com_argv[++i];
	}
	at = COM_VRGameInsertAt (games, n);
	if (at < 0)
		return; // (Setup's shortcut, QuakeVR.bat, Steam's, the Quest's, the tests')
	at = at < n ? argat[at] : com_argc;
	*lookedin = 0;
	for (i = 0; i < com_numbasedirs; i++)
	{
		if (COM_HasVRGameData (com_basedirs[i]))
			break;
		if (*lookedin)
			q_strlcat (lookedin, "; ", sizeof (lookedin));
		q_strlcat (lookedin, com_basedirs[i], sizeof (lookedin));
	}
	if (i == com_numbasedirs)
	{
		// The folder named to the player: where Quake VR should be: the last -basedir given (Setup's shortcut's
		// second, its install folder), else the exe's (a zip's, Setup's), not a base dir (it had no quakevr).
		const char *where = host_parms->exedir && *host_parms->exedir ? host_parms->exedir : com_basedirs[0];
		for (i = 1; i < com_argc - 1; i++)
			if (!q_strcasecmp (com_argv[i], "-basedir"))
				where = com_argv[i + 1];
		Con_Warning ("VR: Quake VR's game data wasn't found: no quakevr folder with its game code in %s%s%s. Reinstall "
			"with QuakeVR-Setup or start from its shortcut (a zip install: QuakeVR.bat, or ironwail.exe in the folder "
			"that has quakevr).\n", lookedin, COM_CheckParm ("-basedir") ? "" : ", nor in the exe's folder ",
			COM_CheckParm ("-basedir") ? "" : where);
		VR_NoteGameDataMissing (where);
		return;
	}
	if (com_argc + 2 > MAX_NUM_ARGVS)
	{
		Con_Warning ("VR: the command line is too long to add \"-game quakevr\": Quake VR's game folder not mounted\n");
		return;
	}
	found = i;
	for (i = com_argc; i >= at; i--) // (argvdummy, at com_argv[com_argc], moved too)
		com_argv[i + 2] = com_argv[i];
	com_argv[at] = "-game";
	com_argv[at + 1] = "quakevr";
	com_argc += 2;
	Con_Printf ("VR: no \"-game quakevr\" on the command line: Quake VR's game folder added%s (%s/quakevr)\n",
		at + 2 < com_argc ? ", under the mod given" : "", com_basedirs[found]);
}

// A game switch's folders ("id1;<mod>": the game command's, the Mods menu's, a save's) with quakevr where
// COM_AddVRGameArg puts it ("id1;quakevr;<mod>", campaign folders under it): the mod over Quake VR, not id1's game
// code alone. False: unchanged (quakevr in it, or no base dir has Quake VR's game data: a broken install).
static qboolean COM_WithVRGame (const char *paths, char *out, size_t size)
{
	const char *games[64];
	char copy[1024];
	char *p;
	int i, n = 0, at;
	for (i = 0; i < com_numbasedirs && !COM_HasVRGameData (com_basedirs[i]); i++)
		;
	if (i == com_numbasedirs)
		return false;
	q_strlcpy (copy, paths, sizeof (copy));
	for (p = copy; p && *p && n < (int) countof (games); )
	{
		games[n++] = p;
		p = strchr (p, ';');
		if (p)
			*p++ = 0;
	}
	at = COM_VRGameInsertAt (games, n);
	if (at < 0)
		return false;
	*out = 0;
	for (i = 0; i <= n; i++)
	{
		const char *g = i == at ? "quakevr" : games[i < at ? i : i - 1];
		if (*out)
			q_strlcat (out, ";", size);
		q_strlcat (out, g, size);
	}
	return true;
}

