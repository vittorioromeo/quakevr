//This file isn't needed for the simple-csqc API.
//However, old versions of FTE, DP, or RMQe don't support the simple-csqc API, so this provides entry points that match the older API so that these older versions can use our new hud too.

static float dp_fuckedsizes;	//true for DP, false for FTE
static float sb_showscores;		//magic name in DP



//to avoid large extensions files, these are the minimum extra things needed for both dp+fte.
void() clearscene = #300;
void(float mask) addentities = #301;
float(float property, ...) setproperty = #303;
void() renderscene = #304;
const float MASK_ENGINE = 1;
const float MASK_VIEWMODEL = 2;
const float VF_MIN = 1;
const float VF_SIZE = 4;
const float VF_DRAWENGINESBAR = 20;
const float VF_DRAWCROSSHAIR = 21;



void(float vwidth, float vheight, float notmenu) CSQC_UpdateView =
{
	vector ssize = [vwidth, vheight];
	clearscene();
	addentities(MASK_ENGINE|MASK_VIEWMODEL);
	setproperty(VF_MIN, '0 0');		//otherwise DP bugs out if viewsize < 100
	setproperty(VF_SIZE, ssize);	//otherwise DP bugs out if viewsize < 100
	setproperty(VF_DRAWENGINESBAR, FALSE);	//we have our own hud. that's the whole point of this.
	setproperty(VF_DRAWCROSSHAIR, TRUE);
	renderscene();

	if (dp_fuckedsizes)
	{
		//Yes, we got told the size to use.
		//but apparently that's not enough.
		//reading these cvars means that we're screwed if either is set to 0 (to mean 'maintain aspect')
		//ideally one of these will ALWAYS be 0, so yeah, expect fucked aspects in DP. Its been like this for YEARS.
		ssize_x = cvar("vid_conwidth");
		ssize_y = cvar("vid_conheight");
	}

	if (!intermission)
		intermission_time = time;	//make a guess at the intermission time...
	deathmatch = stof(serverkey("deathmatch"));
	coop = !deathmatch && maxclients > 1;
	
	CSQC_DrawHud(ssize, sb_showscores);
	CSQC_DrawScores(ssize, sb_showscores);
};

//The following functions are just stubs, because DP INSISTS that they exist.
//Note that they might have been defined elsewhere already, but in case they were not, fteqcc's weak references to the rescue!
__weak float(float evtype, float scanx, float chary, float devid) CSQC_InputEvent =
{
	return FALSE;
};
__weak float(string str) CSQC_ConsoleCommand =
{
	tokenize_console(str);
	return FALSE;
};
__weak void(float apilevel, string enginename, float engineversion) CSQC_Init =
{
}

//we need to wrap this because its the easiest way to see if its a shitty old engine.
//and if its shitty then we can reliably assume that its DP.
__wrap void(float apilevel, string enginename, float engineversion) CSQC_Init =
{
	if (!apilevel && !enginename && !engineversion)
	{
		dp_fuckedsizes = TRUE;	//shitty engine
		dprint("Assuming DP fuckage\n");
	}
	prior(apilevel, enginename, engineversion);
	
	registercommand("+showscores");
	registercommand("+showscores");
	registercommand("+showteamscores");
	registercommand("+showteamscores");
};

//but we need to figure out when +showscores etc are pressed...
__wrap float(string str) CSQC_ConsoleCommand =
{
	if (prior(str))
		return TRUE;
	string c = argv(0);
	if (c == "+showscores")
		sb_showscores |= 1;
	else if (c == "+showteamscores")
		sb_showscores |= 2;
	else if (c == "-showscores")
		sb_showscores &~= 1;
	else if (c == "-showteamscores")
		sb_showscores &~= 2;
	else
		return FALSE;
	return TRUE;
};