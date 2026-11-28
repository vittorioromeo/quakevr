// QVR: the VR main menu.
// Moved as it was out of menu.c, which includes this file at the same place (one translation unit: the
// same statics, no behaviour change). Not compiled on its own (Quake/vr/iw/README.md; IRONWAIL_DIFF.md).

//=============================================================================
/* MAIN MENU */

int	m_main_cursor = 6; // QVR: MAIN_CAMPAIGNS (below): the playing group's first row (Select Campaign), below the VR rows
int m_main_mods;
qboolean m_main_continue_offered; // QVR: the cursor put on Continue once, at the menu's first opening with a save to continue
qboolean m_main_autosaves; // QVR: an autosave to load (Load Autosave's row shown), as the menu's opening found
// QVR: how the main menu last opened and how its VR Hub row was answered, for M_Main_StartHub's console line (his
// e5start_2026-11-22_15-29-11: the hub's Start landed in Dimension of the Past, then the main menu's VR Hub confirm
// took him back 3.3 s later; not reproduced with the mock: the next time, the voice note's log says what did it).
static struct
{
	double time; // realtime it opened
	int from; // the menu state it opened from (-1: the game)
	int key; // the last key pressed before (key_lastpress)
	int rowkey; // the key that chose VR Hub
	double rowtime;
	float laserx, lasery; // the mouse (the laser) then
} m_main_opened = {0.0, -1, -1, -1, 0.0, 0.f, 0.f};

enum
{
	MAIN_CONTINUE, // QVR: the newest save of a campaign's map (vr_continue.cpp), the first row; shown only when there is one
	MAIN_LOADAUTOSAVE, // QVR: Load Autosave, the rotating autosaves newest first (M_Menu_Autosaves_f); shown when there is one
	MAIN_VRCALIBRATION, // QVR: the first-time setup (vr_setup.hpp)
	MAIN_VRTUTORIAL, // QVR: the tutorial (as the Play page's Tutorial), confirmed
	MAIN_VRHUB, // QVR: the VR hub (vrstart, as the Play page's VR Hub), confirmed
	MAIN_VRSETTINGS, // QVR: the VR Settings (as Options > VR Settings), after the VR places; Back from them to this menu
	MAIN_CAMPAIGNS, // QVR: Select Campaign, the official campaigns' page (was Single Player > Official Campaigns)
	MAIN_SINGLEPLAYER,
	MAIN_MULTIPLAYER,
	MAIN_MAPLIBRARY, // QVR: Download Maps, the map browser (vr_menu_maps.inc)
	MAIN_PLAYCUSTOM, // QVR: Play Custom Map, Ironwail's Levels (the maps installed)
	MAIN_OPTIONS,
	MAIN_VRADVANCED, // QVR: the Advanced VR Options (as the corner's Advanced VR)
	MAIN_MODS, // QVR: shown with vr_menu_main_mods only (Options > Mods otherwise)
	MAIN_QUIT, // QVR: no Help/Ordering row

	MAIN_ITEMS,
};

// QVR: the rows as text, in the lettering of Quake's picture of them (gfx/mainmenu.lmp), cut from id's menu pictures
// (vr_bigfont.cpp), so that VR Calibration looks like the others.
static const char *const m_main_labels[MAIN_ITEMS] =
{
	"Continue", "Load Autosave", "VR Calibration", "VR Tutorial", "VR Hub", "VR Settings", "Select Campaign", "Single Player", "Multiplayer", "Download Maps",
	"Play Custom Map", "Options", "Advanced VR", "Mods", "Quit",
};

const char *M_Main_RowLabel (void) // QVR: menu_vr pos
{
	return m_main_labels[m_main_cursor];
}

// QVR: whether a row is shown: Mods only where Ironwail shows it (no mod with its own menu) and vr_menu_main_mods asks
// for it (Advanced VR > HUD and Menus > Menu; Options > Mods has it).
static qboolean M_Main_Shown (int item)
{
	if (item == MAIN_CONTINUE) // QVR: only with a save to continue (vr_continue.cpp)
		return VR_ContinueAvailable ();
	if (item == MAIN_LOADAUTOSAVE) // QVR: only with an autosave to load
		return m_main_autosaves;
	return item != MAIN_MODS || (m_main_mods && VR_MenuMainShowsMods ());
}

// QVR: the rows in groups, a gap above each but the first: the VR rows (VR Calibration, VR Tutorial, VR Hub, VR Settings:
// the author's note vrstart_2026-10-09_18-37-10), playing (Select
// Campaign, Single Player, Multiplayer), the maps
// (Download Maps, Play Custom Map), the settings (Options, Advanced VR, Mods), Quit apart (the author's note
// vrfiringrange_2026-10-08_00-02-55).
static qboolean M_Main_GroupStart (int item)
{
	if (item == MAIN_LOADAUTOSAVE) // QVR: with Continue when it is shown, else the first row
		return false;
	if (item == MAIN_VRCALIBRATION) // QVR: Continue and Load Autosave on their own above the VR rows, when shown
		return M_Main_Shown (MAIN_CONTINUE) || M_Main_Shown (MAIN_LOADAUTOSAVE);
	return item == MAIN_CAMPAIGNS || item == MAIN_MAPLIBRARY || item == MAIN_OPTIONS || item == MAIN_QUIT;
}

void M_Menu_Main_f (void)
{
	if (key_dest != key_menu || m_state != m_main) // QVR: when and from where it opened (M_Main_StartHub's line)
	{
		m_main_opened.time = realtime;
		m_main_opened.from = key_dest == key_menu ? (int) m_state : -1;
		m_main_opened.key = key_lastpress;
	}
	IN_DeactivateForMenu();
	key_dest = key_menu;
	m_state = m_main;
	m_entersound = true;

	// When switching to a mod with a custom UI the 'Mods' option
	// is no longer available in the main menu, so we move the cursor
	// to 'Options' to nudge the player toward the secondary location.
	// TODO (maybe): inform the user about the missing option
	// and its alternative location?
	if (!M_Main_Shown (MAIN_MODS) && m_main_cursor == MAIN_MODS) // QVR: or not shown (vr_menu_main_mods)
	{
		m_main_cursor = MAIN_OPTIONS;
		M_Options_SelectMods ();
	}

	// QVR: Continue (vr_continue.cpp): the saves looked at again; the cursor on it the first time it is offered, off it
	// when there is none (the playing group's first row)
	VR_ContinueRefresh ();
	m_main_autosaves = VR_AutosaveAny ();
	if (!M_Main_Shown (MAIN_LOADAUTOSAVE) && m_main_cursor == MAIN_LOADAUTOSAVE)
		m_main_cursor = MAIN_CAMPAIGNS;
	if (M_Main_Shown (MAIN_CONTINUE) && !m_main_continue_offered)
	{
		m_main_continue_offered = true;
		m_main_cursor = MAIN_CONTINUE;
	}
	if (!M_Main_Shown (MAIN_CONTINUE) && m_main_cursor == MAIN_CONTINUE)
		m_main_cursor = MAIN_CAMPAIGNS;
}


// QVR: the main menu's top (menu y: the title's, the flat banner's; the rows start 28 below), as M_Main_Layout last
// placed it: Quake's 4, or in the headset higher, the title, rows and banner centred in the panel (the author's note:
// the VR main menu sat towards the panel's bottom). A flat screen keeps Quake's place (its corner buttons and notices
// stand above it, level with the title's row).
static int m_main_top = 4;

// QVR: the rows' spacing and the groups' gaps: Quake's 20 and 10, closer where the canvas is too short for them all (a
// flat screen's 200 rows, the headset's panel at Menu Height 1): the gaps a little first, then the rows (to 15), then the
// gaps the rest of the way; then, still too short, the rows to 13 with no gaps, their letters drawn smaller (to fit 15
// apart); the last row's letters, 20 tall, kept inside it. In the headset the whole panel's height is theirs and the
// menu is centred in it (m_main_top).
void M_Main_Layout (int *step, int *gap)
{
	drawtransform_t transform;
	float left, top, right, bottom, sx, sy;
	int rows = 0, groups = 0, i, avail, height;
	qboolean centred = VR_MenuCanvas (&sx, &sy);

	for (i = 0; i < MAIN_ITEMS; i++)
	{
		rows += M_Main_Shown (i);
		groups += i > 0 && M_Main_GroupStart (i);
	}
	Draw_GetCanvasTransform (CANVAS_MENU, &transform);
	Draw_GetTransformBounds (&transform, &left, &top, &right, &bottom);
	if (!centred)
	{
		top = q_max (top, -24.f); // (the title up to y -20 at most: the canvas above Quake's 4 spaces the groups)
		bottom = q_min (bottom, 200.f); // to Quake's 200, not the screen's: the notice box and corner buttons fill below it
	}
	avail = (int)(bottom - 2 - (q_min (top, 0.f) + 4) - 28 - 20);
	*step = CLAMP (15, (avail - groups * (int)(6 * VR_MenuMainGap ())) / (rows - 1), 20); // QVR: Menu Look's gap
	if ((rows - 1) * *step > avail) // more rows than fit 15 apart (a flat screen's 200 rows): closer, to 13, their letters
		*step = q_max (13, avail / (rows - 1)); // smaller (M_Main_Draw), no gaps
	*gap = CLAMP (0, (avail - (rows - 1) * *step) / groups, (int)(10 * VR_MenuMainGap () + 0.5f)); // QVR: vr_menu_main_gap

	// the title's top to the last row's bottom, its middle at the panel's, never lower than Quake's place nor above the
	// panel's top
	height = 28 + (rows - 1) * *step + groups * *gap + 20;
	m_main_top = 4;
	if (!centred)
		m_main_top = q_min (4, (int) ceil (top + 4.f));
	else
		m_main_top = CLAMP ((int) ceil (top + 4.f), Q_rint ((top + bottom - height) * 0.5f), 4);
}

// QVR: each row's right end and bottom (menu x and y) as last drawn: M_ContentRightBelow (the corner's version box and
// update notice keep clear of them: vr_menubrand.cpp). A row not shown: nothing.
static float m_main_row_right[MAIN_ITEMS], m_main_row_bottom[MAIN_ITEMS];

// QVR: a row's top (menu y): 28 below the menu's top (Quake's 32) for the first, a step for each row shown above it, a
// gap for each group.
static int M_Main_RowY (int item, int step, int gap)
{
	int i, y = m_main_top + 28;

	for (i = 0; i < item; i++)
		if (M_Main_Shown (i))
			y += step;
	for (i = 1; i <= item; i++)
		if (M_Main_GroupStart (i))
			y += gap;
	return y;
}

// QVR: Continue's save in two small lines right of its row (as far right as the canvas goes), the row's right end
// moved to theirs (M_ContentRightBelow: the corner's version box keeps clear of them).
static void M_Main_DrawContinue (int row)
{
	drawtransform_t transform;
	float left, top, right, bottom;
	char title[96], detail[96];
	int x = (int) m_main_row_right[MAIN_CONTINUE] + 10, chars;

	VR_ContinueDescribe (title, sizeof (title), detail, sizeof (detail));
	Draw_GetCanvasTransform (CANVAS_MENU, &transform);
	Draw_GetTransformBounds (&transform, &left, &top, &right, &bottom);
	chars = (int) (right - 4 - x) / 8;
	if (chars < 8)
		return;
	if ((int) strlen (title) > chars)
		title[chars] = 0;
	if ((int) strlen (detail) > chars)
		detail[chars] = 0;
	M_PrintWhite (x, row + 1, title);
	M_Print (x, row + 11, detail);
	m_main_row_right[MAIN_CONTINUE] = (float) (x + 8 * q_max ((int) strlen (title), (int) strlen (detail)));
}

// QVR: Menu Look: each main menu row's growth while selected (0 to 1: vr_menu_big_hl 2), eased over a tenth of a second,
// and when it last stepped.
static float m_main_grow[MAIN_ITEMS];
static double m_main_grow_time;

static void M_Main_StepGrow (void)
{
	int i;
	float step = (float) CLAMP (0.0, (realtime - m_main_grow_time) / 0.1, 1.0);

	m_main_grow_time = realtime;
	for (i = 0; i < MAIN_ITEMS; i++)
	{
		float target = i == m_main_cursor ? 1.f : 0.f;
		m_main_grow[i] = m_main_grow[i] < target ? q_min (m_main_grow[i] + step, target) : q_max (m_main_grow[i] - step, target);
	}
}

// QVR: how far right of Quake's columns (left: negative) the rows, their cursor and a flat screen's banner are drawn,
// as M_Main_Draw last drew them (M_Main_Shift).
static int m_main_shift;

// QVR: the main menu centred (the author's note: its rows sat right of the title, the menu looked misaligned): the rows
// from their cursor's column (Quake's 54) to the widest one's right end centred under the title (x 160), never right of
// Quake's place; no further left than x 0 (the VR menu style's corner buttons stand left of it: M_ContentLeft) nor than
// a flat screen's canvas leaves room for the banner moved with them.
static int M_Main_Shift (qboolean text, float scale)
{
	drawtransform_t transform;
	float left, top, right, bottom, bannerLeft, bannerRight, widest = 0.f, width;
	qpic_t *p = text ? NULL : Draw_CachePic ("gfx/mainmenu.lmp");
	int i, least = -54;

	for (i = 0; i < MAIN_ITEMS; i++)
	{
		if (!M_Main_Shown (i))
			continue;
		width = text ? 1.f + VR_BigFont_WidthScaled (scale, m_main_labels[i])
					 : (float) q_max (p->width, 16 * (int) strlen (m_main_labels[i]) + 2);
		widest = q_max (widest, width);
	}
	Draw_GetCanvasTransform (CANVAS_MENU, &transform);
	Draw_GetTransformBounds (&transform, &left, &top, &right, &bottom);
	if (VR_MenuBannerExtent (16, &bannerLeft, &bannerRight))
		least = q_max (least, (int) ceil (left + 2.f - bannerLeft));
	return CLAMP (least, Q_rint (160.f - (54.f + 72.f + widest) * 0.5f), 0);
}

void M_Main_Draw (void)
{
	int		i, step, gap, x; // QVR: i, step, gap, x
	qpic_t	*p;
	qboolean text; // QVR: the rows as text (VR_BigFont_Draw)
	float scale; // QVR: their letters' size

	// QVR: the rows as text in the picture's letters when the pictures give every one (a mod's own pictures may not):
	// else the picture's rows, the others in the mods' fallback row's letters.
	text = true;
	for (i = 0; i < MAIN_ITEMS; i++)
		if (M_Main_Shown (i) && !VR_BigFont_CanDraw (m_main_labels[i]))
			text = false;

	M_Main_Layout (&step, &gap);
	scale = step < 15 ? step / 15.f : 1.f; // (rows closer than 15: smaller letters)
	m_main_shift = M_Main_Shift (text, scale);
	x = 72 + m_main_shift; // QVR: the rows' left (Quake's 72), centred

	M_DrawPlaque (16 + m_main_shift, m_main_top); // QVR: the Quake VR banner, moved with the rows
	p = Draw_CachePic ("gfx/ttl_main.lmp");
	M_DrawPic ( (320-p->width)/2, m_main_top, p); // QVR: m_main_top (centred in the headset)

	for (i = 0; i < MAIN_ITEMS; i++)
	{
		m_main_row_right[i] = -1e9f;
		m_main_row_bottom[i] = M_Main_Shown (i) ? (float)(M_Main_RowY (i, step, gap) + 20) : -1e9f;
	}
	M_Main_StepGrow ();
	if (M_Main_Shown (m_main_cursor)) // QVR: Menu Look: a soft bar behind the selected row (vr_menu_big_hl)
	{
		int row = M_Main_RowY (m_main_cursor, step, gap);
		float w = text ? 1.f + VR_BigFont_WidthScaled (scale * VR_MenuBigGrow (), m_main_labels[m_main_cursor])
					   : (float) q_max (Draw_CachePic ("gfx/mainmenu.lmp")->width, 16 * (int) strlen (m_main_labels[m_main_cursor]) + 2);
		VR_MenuDrawBigHighlight ((float)(x - 22), x + w + 6.f, row + 10.f * scale, 11.f * scale);
	}
	if (text)
	{
		for (i = 0; i < MAIN_ITEMS; i++) // QVR: the rows as text (the selected one grown: vr_menu_big_hl 2)
			if (M_Main_Shown (i))
			{
				float s = scale * (1.f + (VR_MenuBigGrow () - 1.f) * m_main_grow[i]);
				int row = M_Main_RowY (i, step, gap) - (int) floor (10.f * (s - scale) + 0.5f);
				m_main_row_right[i] = x + 1.f + VR_BigFont_DrawScaled (x + 1, row, s, m_main_labels[i]);
			}
	}
	else
	{ // QVR: the picture's rows (its Help row left out), the others in the mods' row's letters
		p = Draw_CachePic ("gfx/mainmenu.lmp");
		for (i = 0; i < MAIN_ITEMS; i++)
		{
			int row = M_Main_RowY (i, step, gap);
			if (!M_Main_Shown (i))
				continue;
			m_main_row_right[i] = (float)(x + q_max (p->width, 16 * (int) strlen (m_main_labels[i]) + 2));
			switch (i)
			{
			case MAIN_SINGLEPLAYER: M_DrawSubpic (x, row, p, 0, 0, p->width, 20); break;
			case MAIN_MULTIPLAYER: M_DrawSubpic (x, row, p, 0, 20, p->width, 20); break;
			case MAIN_OPTIONS: M_DrawSubpic (x, row, p, 0, 40, p->width, 20); break;
			case MAIN_QUIT: M_DrawSubpic (x, row, p, 0, 80, p->width, p->height - 80); break;
			case MAIN_MODS:
				if (m_main_mods > 0)
					M_DrawTransPic (x, row, Draw_CachePic ("gfx/menumods.lmp"));
				else
					M_PrintEx (x + 2, row + 1, 16, "MODS");
				break;
			case MAIN_CONTINUE: M_PrintEx (x + 2, row + 1, 16, "CONTINUE"); break;
			case MAIN_LOADAUTOSAVE: M_PrintEx (x + 2, row + 1, 16, "LOAD AUTOSAVE"); break;
			case MAIN_VRCALIBRATION: M_PrintEx (x + 2, row + 1, 16, "VR CALIBRATION"); break;
			case MAIN_VRTUTORIAL: M_PrintEx (x + 2, row + 1, 16, "VR TUTORIAL"); break;
			case MAIN_VRHUB: M_PrintEx (x + 2, row + 1, 16, "VR HUB"); break;
			case MAIN_CAMPAIGNS: M_PrintEx (x + 2, row + 1, 16, "SELECT CAMPAIGN"); break;
			case MAIN_MAPLIBRARY: M_PrintEx (x + 2, row + 1, 16, "DOWNLOAD MAPS"); break;
			case MAIN_PLAYCUSTOM: M_PrintEx (x + 2, row + 1, 16, "PLAY CUSTOM MAP"); break;
			case MAIN_VRSETTINGS: M_PrintEx (x + 2, row + 1, 16, "VR SETTINGS"); break;
			case MAIN_VRADVANCED: M_PrintEx (x + 2, row + 1, 16, "ADVANCED VR"); break;
			}
		}
	}

	// QVR: Continue's save beside it, small: its map, its campaign and when (vr_continue.cpp)
	if (M_Main_Shown (MAIN_CONTINUE))
		M_Main_DrawContinue (M_Main_RowY (MAIN_CONTINUE, step, gap));

	M_DrawQuakeCursor (x - 18, M_Main_RowY (m_main_cursor, step, gap));
}

// QVR: the main menu's rows' left and right ends as last drawn (menu x: from their cursor's column), for menu_vr pos.
void M_Main_RowsExtent (float *left, float *right)
{
	int i;

	*left = 54.f + m_main_shift;
	*right = -1e9f;
	for (i = 0; i < MAIN_ITEMS; i++)
		if (M_Main_Shown (i) && i != MAIN_CONTINUE)
			*right = q_max (*right, m_main_row_right[i]);
}

// QVR: the main menu's top (the title's) and its last row's bottom as last drawn, and the canvas's top and bottom (menu
// y), for menu_vr pos.
void M_Main_RowsSpan (float *top, float *bottom, float *canvasTop, float *canvasBottom)
{
	drawtransform_t transform;
	float left, right;
	int i;

	*top = (float) m_main_top;
	*bottom = -1e9f;
	for (i = 0; i < MAIN_ITEMS; i++)
		*bottom = q_max (*bottom, m_main_row_bottom[i]);
	Draw_GetCanvasTransform (CANVAS_MENU, &transform);
	Draw_GetTransformBounds (&transform, &left, canvasTop, &right, canvasBottom);
}

// QVR: the cursor to the next row shown up (-1) or down (1), round the ends.
static void M_Main_MoveCursor (int dir)
{
	int i;

	for (i = 0; i < MAIN_ITEMS; i++)
	{
		m_main_cursor = (m_main_cursor + dir + MAIN_ITEMS) % MAIN_ITEMS;
		if (M_Main_Shown (m_main_cursor))
			break;
	}
}

// QVR: VR Calibration's confirmed: the menu closed, the calibration room (vr_setup.hpp).
static void M_Main_StartCalibration (void)
{
	IN_Activate ();
	key_dest = key_game;
	m_state = m_none;
	Cbuf_InsertText ("vr_setup\n"); // before anything queued (a test script's next commands)
}

// QVR: VR Tutorial's and VR Hub's confirmed: the menu closed, the map started (its command queued: vr_menu.cpp).
static void M_Main_StartTutorial (void)
{
	IN_Activate ();
	key_dest = key_game;
	m_state = m_none;
	VR_StartTutorial ();
}

// QVR: Continue's (confirmed when a game was running): the menu closed, the save loaded (its campaign first).
static void M_Main_Continue (void)
{
	IN_Activate ();
	key_dest = key_game;
	m_state = m_none;
	VR_ContinueStart ();
}

static void M_Main_StartHub (void)
{
	IN_Activate ();
	key_dest = key_game;
	m_state = m_none;
	Con_Printf ("[skipnotify]VR Hub: the main menu opened %.1f s ago (from %s, after key %s); VR Hub chosen by %s %.1f s ago "
		"(the mouse at %.0f %.0f), confirmed by %s\n", realtime - m_main_opened.time,
		m_main_opened.from < 0 ? "the game" : va ("menu %d", m_main_opened.from), Key_KeynumToString (m_main_opened.key),
		Key_KeynumToString (m_main_opened.rowkey), realtime - m_main_opened.rowtime, m_main_opened.laserx,
		m_main_opened.lasery, Key_KeynumToString (key_lastpress));
	VR_StartHub ();
}

void M_Main_Key (int key)
{
	switch (key)
	{
	case K_ESCAPE:
	case K_BBUTTON:
	case K_MOUSE4:
	case K_MOUSE2:
		IN_Activate();
		key_dest = key_game;
		m_state = m_none;
		break;

	case K_DOWNARROW:
		M_ThrottledSound ("misc/menu1.wav");
		M_Main_MoveCursor (1); // QVR: past a row not shown
		break;

	case K_UPARROW:
		M_ThrottledSound ("misc/menu1.wav");
		M_Main_MoveCursor (-1);
		break;

	case K_ENTER:
	case K_KP_ENTER:
	case K_ABUTTON:
	case K_MOUSE1:
		m_entersound = true;

		switch (m_main_cursor)
		{
		case MAIN_CONTINUE: // QVR: the newest save (vr_continue.cpp); confirmed when a campaign's game is running
			if (sv.active && VR_AutosaveAllowed (sv.name)) // (a campaign's map: not the hub, the tutorial...)
			{
				char title[96], detail[96];
				VR_ContinueDescribe (title, sizeof (title), detail, sizeof (detail));
				M_Confirm (va ("Continue from your last save?\n\nThe game in progress ends: you go to\n%s\n%s.", title, detail),
					"Continue", "Cancel", 0.f, M_Main_Continue);
			}
			else
				M_Main_Continue ();
			break;

		case MAIN_LOADAUTOSAVE: // QVR: the rotating autosaves, newest first
			M_Menu_Autosaves_f ();
			break;

		case MAIN_VRCALIBRATION: // QVR: the calibration room (vr_setup)
			M_Confirm (sv.active
				? "Start VR Calibration?\n\nThe game in progress ends: you go\nto the calibration room, and the\ncalibration starts by itself."
				: "Start VR Calibration?\n\nYou go to the calibration room, and\nthe calibration starts by itself:\nyour height, then your body.",
				"Start", "Cancel", 0.f, M_Main_StartCalibration);
			break;

		case MAIN_VRTUTORIAL: // QVR: the tutorial, as the Play page's Tutorial (VR_StartTutorial)
			M_Confirm (sv.active
				? "Start the VR Tutorial?\n\nThe game in progress ends: you go\nto the tutorial's first lesson."
				: "Start the VR Tutorial?\n\nLessons on playing Quake in VR,\none after another.",
				"Start", "Cancel", 0.f, M_Main_StartTutorial);
			break;

		case MAIN_VRHUB: // QVR: the hub, as the Play page's VR Hub (VR_StartHub)
			m_main_opened.rowkey = key;
			m_main_opened.rowtime = realtime;
			m_main_opened.laserx = m_mousex;
			m_main_opened.lasery = m_mousey;
			M_Confirm (sv.active
				? "Go to the VR Hub?\n\nThe game in progress ends: you go\nto the hub, where Quake VR starts."
				: "Go to the VR Hub?\n\nWhere Quake VR starts.",
				"Go", "Cancel", 0.f, M_Main_StartHub);
			break;

		case MAIN_SINGLEPLAYER:
			M_Menu_SinglePlayer_f ();
			break;

		case MAIN_CAMPAIGNS: // QVR: the official campaigns' page; Back from it to this menu
			VR_OpenCampaignSelector ();
			break;

		case MAIN_MULTIPLAYER:
			M_Menu_MultiPlayer_f ();
			break;

		case MAIN_MAPLIBRARY: // QVR
			VR_OpenMapLibrary ();
			break;

		case MAIN_PLAYCUSTOM: // QVR: Ironwail's Levels; Back from them to this menu
			M_Menu_LevelsFrom ();
			break;

		case MAIN_OPTIONS:
			M_Menu_Options_f ();
			break;

		case MAIN_VRSETTINGS: // QVR
			VR_Menu_OpenFromMain (0);
			break;

		case MAIN_VRADVANCED: // QVR
			VR_Menu_OpenFromMain (1);
			break;

		case MAIN_MODS:
			M_Menu_Mods_f ();
			break;

		case MAIN_QUIT:
			M_Menu_Quit_f ();
			break;
		}
	}
}

// QVR: the row under the mouse (the laser's spot in the headset): each row from its top down a step; in a gap between
// groups, above the rows or below them, the row selected stays.
void M_Main_Mousemove (float cx, float cy)
{
	int prev = m_main_cursor;
	int i, y, step, gap;

	M_Main_Layout (&step, &gap);
	for (i = 0; i < MAIN_ITEMS; i++)
	{
		if (!M_Main_Shown (i))
			continue;
		y = M_Main_RowY (i, step, gap);
		if (cy >= y && cy < y + step)
			m_main_cursor = i;
	}
	if (m_main_cursor != prev)
		M_MouseSound ("misc/menu1.wav");
}

