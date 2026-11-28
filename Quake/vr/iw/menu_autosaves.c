// QVR: the Load Autosave menu.
// Moved as it was out of menu.c, which includes this file at the same place (one translation unit: the
// same statics, no behaviour change). Not compiled on its own (Quake/vr/iw/README.md; IRONWAIL_DIFF.md).

//=============================================================================
/* LOAD AUTOSAVE MENU */ // QVR

// QVR: Load Autosave (the main menu's row, VR Settings > Saves and Autosaves, menu_autosaves) and the death menu (single
// player, dead for vr_death_menu_delay seconds: M_CheckDeathMenu; player feedback: an autosave in an ambush trapped
// them). The rotating autosaves (vr_continue.cpp), newest first, each in two lines: its map and title; health, armor,
// time into the level and how long ago. The death menu has Respawn (as fire does: the last save loaded, sv_autoload,
// else the level again) and Restart the Level above them; Back from it to the dead player (a press opens it again: only
// its choices respawn).
// (The author, 2026-10-22: it sat low on the VR menu's canvas, its rows crammed: the title now near the canvas's top,
// the rows further apart.)
#define AUTOSAVES_PAD	16	// the title below the canvas's top
#define AUTOSAVES_TITLEH	36	// the title's room (its big letters at AUTOSAVES_TITLESCALE, and a gap below)
#define AUTOSAVES_TITLESCALE	1.25f
#define AUTOSAVES_ROWH	30	// a row's height (two lines; the laser's target)

static struct
{
	qboolean	death;			// the death menu
	qboolean	dismissed;		// closed while dead: not opened again until the player lives
	qboolean	respawning;		// held back for this death: a respawn under way (logged once)
	double		deadsince;		// sv.time the player was first seen dead (0: alive)
	int			count;			// autosaves listed
	int			cursor;			// the row (the death menu's actions first)
	int			first;			// the first row drawn (scrolled)
} autosavesmenu;

// The rows above the autosaves: the death menu's Respawn and Restart, then which games' autosaves are listed (this
// game's or every game's: vr_autosave_list_all; selecting it switches).
static int M_Autosaves_Actions (void)
{
	return autosavesmenu.death ? 3 : 1;
}

static int M_Autosaves_ScopeRow (void)
{
	return M_Autosaves_Actions () - 1;
}

static int M_Autosaves_Rows (void)
{
	return M_Autosaves_Actions () + autosavesmenu.count;
}

// The canvas's top and bottom as shown (the VR panel's canvas reaches far above and below Quake's 320 x 200; a flat
// screen's at least that).
static void M_Autosaves_Bounds (float *top, float *bottom)
{
	drawtransform_t transform;
	float left, right;

	Draw_GetCanvasTransform (CANVAS_MENU, &transform);
	Draw_GetTransformBounds (&transform, &left, top, &right, bottom);
	*top = CLAMP (-400.f, *top, 0.f);
	*bottom = CLAMP (200.f, *bottom, 600.f);
}

// The title's y (near the canvas's top) and the first row's (below it).
static int M_Autosaves_TitleY (void)
{
	float top, bottom;

	M_Autosaves_Bounds (&top, &bottom);
	return (int) top + AUTOSAVES_PAD;
}

static int M_Autosaves_Top (void)
{
	return M_Autosaves_TitleY () + AUTOSAVES_TITLEH;
}

// How many rows the canvas has room for, below the title (and a line for "more below...").
static int M_Autosaves_Visible (void)
{
	float top, bottom;

	M_Autosaves_Bounds (&top, &bottom);
	return q_max (3, (int) (bottom - 16 - M_Autosaves_Top ()) / AUTOSAVES_ROWH);
}

static void M_Autosaves_KeepVisible (void)
{
	int visible = M_Autosaves_Visible ();

	if (autosavesmenu.cursor < autosavesmenu.first)
		autosavesmenu.first = autosavesmenu.cursor;
	if (autosavesmenu.cursor >= autosavesmenu.first + visible)
		autosavesmenu.first = autosavesmenu.cursor - visible + 1;
	autosavesmenu.first = CLAMP (0, autosavesmenu.first, q_max (0, M_Autosaves_Rows () - visible));
}

static void M_Menu_Autosaves_Open (qboolean death)
{
	IN_DeactivateForMenu ();
	key_dest = key_menu;
	m_state = m_autosaves;
	m_entersound = true;
	autosavesmenu.death = death;
	autosavesmenu.count = VR_AutosaveListRefresh ();
	autosavesmenu.cursor = !death && autosavesmenu.count ? M_Autosaves_Actions () : 0; // (the newest autosave; Respawn)
	autosavesmenu.first = 0;
	Con_DPrintf ("%s: %d autosaves (title at y %d, rows from y %d, %d apart)\n", death ? "Death menu" : "Load Autosave",
		autosavesmenu.count, M_Autosaves_TitleY (), M_Autosaves_Top (), AUTOSAVES_ROWH);
}

void M_Menu_Autosaves_f (void)
{
	M_Menu_Autosaves_Open (Cmd_Argc () > 1 && !q_strcasecmp (Cmd_Argv (1), "death"));
}

static void M_Autosaves_Close (void)
{
	IN_Activate ();
	key_dest = key_game;
	m_state = m_none;
}

// A line cut to the canvas's right edge from x.
static void M_Autosaves_Print (int x, int y, qboolean white, const char *text)
{
	drawtransform_t transform;
	float left, top, right, bottom;
	char line[128];
	int chars;

	Draw_GetCanvasTransform (CANVAS_MENU, &transform);
	Draw_GetTransformBounds (&transform, &left, &top, &right, &bottom);
	chars = CLAMP (8, (int) (right - 4 - x) / 8, (int) sizeof (line) - 1);
	q_strlcpy (line, text, sizeof (line));
	if ((int) strlen (line) > chars)
		line[chars] = 0;
	if (white)
		M_PrintWhite (x, y, line);
	else
		M_Print (x, y, line);
}

void M_Autosaves_Draw (void)
{
	const char *title = autosavesmenu.death ? "You Died" : "Load Autosave";
	char line1[96], line2[96], scope[64];
	int i, row, y, visible = M_Autosaves_Visible (), titley = M_Autosaves_TitleY (), rowstop = M_Autosaves_Top ();

	if (VR_BigFont_CanDraw (title))
		VR_BigFont_DrawScaled ((int) (160.f - VR_BigFont_WidthScaled (AUTOSAVES_TITLESCALE, title) * 0.5f), titley,
			AUTOSAVES_TITLESCALE, title);
	else // (the big letters lack the capital Y and I: twice the menu's letters)
		M_PrintWhiteEx (160 - 8 * (int) strlen (title), titley + 4, 16, title);

	M_Autosaves_KeepVisible ();
	for (i = 0; i < visible; i++)
	{
		row = autosavesmenu.first + i;
		if (row >= M_Autosaves_Rows ())
			break;
		y = rowstop + i * AUTOSAVES_ROWH + 5; // (two lines 9 apart in the row's middle, a gap above and below)
		if (row == M_Autosaves_ScopeRow ())
		{
			VR_AutosaveListScope (scope, sizeof (scope));
			q_snprintf (line1, sizeof (line1), "Showing: %s", scope);
			if (Cvar_VariableValue ("vr_autosave_list_all"))
				q_strlcpy (line2, "Select: only the game you are playing", sizeof (line2));
			else if (VR_AutosaveListHidden ())
				q_snprintf (line2, sizeof (line2), "Select: all games (%d more)", VR_AutosaveListHidden ());
			else
				q_strlcpy (line2, "Select: all games", sizeof (line2));
			M_Autosaves_Print (24, y + 1, true, line1);
			M_Autosaves_Print (24, y + 10, false, line2);
		}
		else if (row < M_Autosaves_Actions ())
		{
			if (row == 0)
				M_Autosaves_Print (24, y + 6, true, sv.lastsave[0] && sv_autoload.value >= 2.f
					? "Respawn: load your last save" : "Respawn: start the level again");
			else
				M_Autosaves_Print (24, y + 6, true, "Restart the level");
		}
		else
		{
			VR_AutosaveDescribe (row - M_Autosaves_Actions (), line1, sizeof (line1), line2, sizeof (line2));
			M_Autosaves_Print (24, y + 1, true, line1);
			M_Autosaves_Print (24, y + 10, false, line2);
		}
		if (row == autosavesmenu.cursor)
			M_DrawArrowCursor (12, y + 6);
	}
	y = rowstop + visible * AUTOSAVES_ROWH + 4;
	if (!autosavesmenu.count)
		M_Autosaves_Print (24, rowstop + M_Autosaves_Actions () * AUTOSAVES_ROWH + 11, false, VR_AutosaveListHidden () ? "No autosaves of this game yet." : "No autosaves yet.");
	else if (autosavesmenu.first + visible < M_Autosaves_Rows ())
		M_Print (24, y, "more below...");
}

void M_Autosaves_Key (int key)
{
	int rows = M_Autosaves_Rows ();

	switch (key)
	{
	case K_ESCAPE:
	case K_BBUTTON:
	case K_MOUSE4:
	case K_MOUSE2:
		if (autosavesmenu.death) // back to the dead player: a press opens it again (M_CheckDeathMenu)
		{
			autosavesmenu.dismissed = true;
			M_Autosaves_Close ();
		}
		else
			M_Menu_Main_f ();
		break;

	case K_ENTER:
	case K_KP_ENTER:
	case K_ABUTTON:
	case K_MOUSE1:
		if (autosavesmenu.cursor >= rows)
			break;
		M_ThrottledSound ("misc/menu2.wav");
		if (autosavesmenu.cursor == M_Autosaves_ScopeRow ()) // this game's autosaves or every game's: the list again
		{
			autosavesmenu.count = VR_AutosaveListToggleAll ();
			break;
		}
		M_Autosaves_Close ();
		// (Respawn: QC's respawn, as a press did: a checkpoint's, else the restart; a progs without it, the restart)
		if (autosavesmenu.cursor == 0 && autosavesmenu.death && autosavesmenu.deadsince && PR_SetServerGlobalFloat ("vr_death_menu_respawn", 1.f))
			Con_DPrintf ("Death menu: Respawn\n");
		else if (autosavesmenu.cursor < M_Autosaves_Actions ())
			Cbuf_InsertText (autosavesmenu.cursor == 0 ? "restart\n" : "restart fresh\n"); // (next, before any command queued)
		else
			VR_AutosaveLoad (autosavesmenu.cursor - M_Autosaves_Actions ());
		break;

	case K_UPARROW:
	case K_LEFTARROW:
	case K_MWHEELUP:
		if (rows <= 0)
			break;
		M_ThrottledSound ("misc/menu1.wav");
		autosavesmenu.cursor = (autosavesmenu.cursor - 1 + rows) % rows;
		break;

	case K_DOWNARROW:
	case K_RIGHTARROW:
	case K_MWHEELDOWN:
		if (rows <= 0)
			break;
		M_ThrottledSound ("misc/menu1.wav");
		autosavesmenu.cursor = (autosavesmenu.cursor + 1) % rows;
		break;
	}
}

void M_Autosaves_Mousemove (float cx, float cy)
{
	int prev = autosavesmenu.cursor;
	int row = (int) floor ((cy - M_Autosaves_Top ()) / AUTOSAVES_ROWH);

	if (row < 0 || row >= M_Autosaves_Visible () || autosavesmenu.first + row >= M_Autosaves_Rows ())
		return;
	autosavesmenu.cursor = autosavesmenu.first + row;
	if (autosavesmenu.cursor != prev)
		M_MouseSound ("misc/menu1.wav");
}

// QVR: the death menu opened when the player of a single-player campaign game has been dead vr_death_menu_delay seconds
// (not the VR maps, not after Back closed it for this death); closed when they live again another way.
void M_CheckDeathMenu (void)
{
	edict_t *player;
	qboolean asked;

	// (On every map, Quake VR's own too: they had none, and a press respawned there: author's notes 2026-10-20, 2026-10-22,
	// on vrfiringrange and vrragtriggers. A map with checkpoints respawns at its latest: QC's respawn.)
	if (!sv.active || svs.maxclients != 1 || !svs.clients || !svs.clients[0].edict || cls.signon != SIGNONS)
		return;
	player = svs.clients[0].edict;
	if (player->v.health > 0.f || cl.intermission)
	{
		// (alive again another way: its death menu closed; one opened alive, Debug's, stays)
		if (autosavesmenu.deadsince && m_state == m_autosaves && autosavesmenu.death && key_dest == key_menu)
			M_Autosaves_Close ();
		if (autosavesmenu.deadsince)
			PR_SetServerGlobalFloat ("vr_death_menu_hold", 0.f);
		autosavesmenu.deadsince = 0.;
		autosavesmenu.dismissed = false;
		autosavesmenu.respawning = false;
		return;
	}
	if (!autosavesmenu.deadsince)
	{
		autosavesmenu.deadsince = q_max (sv.qcvm.time, 0.001);
		Con_DPrintf ("Death menu: the player died (key_dest %d, menu %d)\n", (int) key_dest, (int) m_state);
	}
	// The menu holds the respawn: a press only opens it (QC's PlayerDeathThink: vr_death_menu_asked), and only its
	// choices respawn (author's note 2026-10-22). Off, or a load under way: a press respawns as in Quake.
	// (Ironman: none either: no saves to load, and a press restarts the level: QC's respawn, vr_run_ironman.qc.)
	if (!Cvar_VariableValue ("vr_death_menu") || sv.autoloading || VR_RunIronman ())
	{
		PR_SetServerGlobalFloat ("vr_death_menu_hold", 0.f);
		return;
	}
	PR_SetServerGlobalFloat ("vr_death_menu_hold", 1.f);
	asked = PR_ServerGlobalFloat ("vr_death_menu_asked") != 0.f;
	if (asked)
	{
		PR_SetServerGlobalFloat ("vr_death_menu_asked", 0.f);
		autosavesmenu.dismissed = false; // (closed with Back: a press opens it again)
		Con_DPrintf ("Death menu: a press asked for it\n");
	}
	if (autosavesmenu.dismissed)
		return;
	if (!asked && sv.qcvm.time - autosavesmenu.deadsince < Cvar_VariableValue ("vr_death_menu_delay"))
		return;
	// (In the game, whatever m_state says: a map or a load started from a menu leaves key_dest key_game and m_state the
	// menu it came from, and the death menu never opened in play, only in tests started by a script: author's note
	// 2026-10-20. Not once a press asked for a respawn, QC's respawn: its restart, or the load it makes, under way.)
	if (key_dest != key_game)
		return;
	if (PR_ServerGlobalFloat ("vr_restart_queued") || VR_LoadingWaiting ())
	{
		if (!autosavesmenu.respawning)
			Con_DPrintf ("Death menu: not opened: a respawn is under way\n");
		autosavesmenu.respawning = true;
		return;
	}
	Con_DPrintf ("Death menu: opening (menu %d before)\n", (int) m_state);
	M_Menu_Autosaves_Open (true);
	VR_MenuPanelInFront ();
}

