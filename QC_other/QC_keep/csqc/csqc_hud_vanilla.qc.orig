//	This file implements NQ, QW, and N64-style huds in csqc. Its meant to be complete, while not otherwise being that different.
//	The hud layout+items used are specific to vanilla Quake. For the mission packs, you should use the appropriate alternative huds.
//	
//	The entry points are at the bottom of the file.
//	You can just add this file (and fteextensions.qc or qsextensions.qc) into any mod and then just adapt it as desired.
//
//	Note that it doesn't understand viewsizes all that well, but no worse than eg scr_sbaralpha<1 in QS (its default). (this keeps the api spec simpler.)
//	the 'simple csqc' api means that this file (and api defs) is all you need for a hud, and can be directly embedded into a regular nq progs.dat, if needed.
//
//	I have made attempts to avoid copying the engine code, however I will not be held liable if there's any GPL contagion.
//	There is otherwise no license for the code in this file.


/*autocvars:
	cl_sbar: alternative layouts (sorry for this complication, but mneh).
		0: QuakeWorld style hud
		1: Vanilla style hud
		2: n64-style hud
		3: n64-style with meshes instead of models, where supported.
	cl_hudswap: QW/n64 ammo+weapons are on the
		0: right
		1: left
	scr_sbaralpha: opacity of backgroundy elements
*/

// items. FIXME: you should share these with your ssqc...
/*
float	IT_SHOTGUN				= 1<<0;
float	IT_SUPER_SHOTGUN		= 1<<1;
float	IT_NAILGUN				= 1<<2;
float	IT_SUPER_NAILGUN		= 1<<3;
float	IT_GRENADE_LAUNCHER		= 1<<4;
float	IT_ROCKET_LAUNCHER		= 1<<5;
float	IT_LIGHTNING			= 1<<6;

//float	IT_EXTRA_WEAPON			= 1<<7;

float	IT_SHELLS				= 1<<8;
float	IT_NAILS				= 1<<9;
float	IT_ROCKETS				= 1<<10;
float	IT_CELLS				= 1<<11;
float	IT_AXE					= 1<<12;

float	IT_ARMOR1				= 1<<13;
float	IT_ARMOR2				= 1<<14;
float	IT_ARMOR3				= 1<<15;
//float	IT_SUPERHEALTH			= 1<<16;

float	IT_KEY1					= 1<<17;
float	IT_KEY2					= 1<<18;
float	IT_INVISIBILITY			= 1<<19;
float	IT_INVULNERABILITY		= 1<<20;
float	IT_SUIT					= 1<<21;
float	IT_QUAD					= 1<<22;

float IT2_RUNE1					= 1<<5;
float IT2_RUNE2					= 1<<6;
float IT2_RUNE3					= 1<<7;
float IT2_RUNE4					= 1<<8;
*/
//=======================================SELF.ITEMS1======================================================
float IT_SHOTGUN    		= 1.000;		//Correct for standard HUD & HIP & DOE			W
float IT_SUPER_SHOTGUN   	= 2.000;		//Correct for standard HUD & HIP & DOE			W
float IT_NAILGUN    		= 4.000;		//Correct for standard HUD & HIP & DOE			W
float IT_SUPER_NAILGUN    	= 8.000;		//Correct for standard HUD & HIP & DOE			W
float IT_GRENADE_LAUNCHER    	= 16.000;		//Correct for standard HUD & HIP & DOE			W
float IT_ROCKET_LAUNCHER    	= 32.000;		//Correct for standard HUD & HIP & DOE			W
float IT_LIGHTNING    		= 64.000;		//Correct for standard HUD & HIP & DOE			W
float IT_SHELLS    		= 128.000;		//Correct for DOE HUD					I
float IT_NAILS    		= 256.000;		//Correct for DOE HUD					I
float IT_ROCKETS    		= 512.000;		//Correct for DOE HUD					I
float IT_CELLS    		= 1024.000;		//Correct for DOE HUD					I
float IT_AXE    		= 2048.000;		//Correct for DOE HUD					W
float IT_LAVA_NAILGUN    	= 4096.000;		//Correct for DOE HUD (okay for standard and HIP)	W
float IT_LAVA_SUPER_NAILGUN    	= 8192.000;		//Correct for DOE HUD (okay for standard and HIP)	W
float IT_MULTI_GRENADE    	= 16384.000;		//Correct for DOE HUD (okay for standard and HIP)	W
float IT_MULTI_ROCKET    	= 32768.000;		//Correct for DOE HUD (okay for standard and HIP)	W
float IT_PLASMA_GUN   	 	= 65536.000;		//Correct for DOE HUD					W
float IT_KEY1    		= 131072.000;		//Correct for standard HUD & HIP & DOE			I
float IT_KEY2    		= 262144.000;		//Correct for standard HUD & HIP & DOE			I
float IT_INVISIBILITY    	= 524288.000;		//Correct for standard HUD & HIP & DOE			I
float IT_INVULNERABILITY    	= 1048576.000;		//Correct for standard HUD & HIP & DOE			I
float IT_SUIT    		= 2097152.000;		//Correct for standard HUD & HIP & DOE			I
float IT_QUAD    		= 4194304.000;		//Correct for standard HUD & HIP & DOE			I
float IT_EXTRA_WEAPON  		= 8388608.000;		//Correct for DOE HUD					W
//=======================================SELF.ITEMS2======================================================
float IT2_ARMOR1    		= 1.000;		//Correct for DOE HUD					I
float IT2_ARMOR2    		= 2.000;		//Correct for DOE HUD					I
float IT2_ARMOR3    		= 4.000;		//Correct for DOE HUD					I
float IT2_LAVA_NAILS    	= 8.000;		//Correct for DOE HUD					I
float IT2_PLASMA_AMMO   	= 16.000;		//Correct for DOE HUD					I
float IT2_MULTI_ROCKETS    	= 32.000;		//Correct for DOE HUD					I
float IT2_SHIELD    		= 64.000;		//Correct for DOE HUD					I
float IT2_CHAINLIGHTNING	= 128.000;		//Zerstorer chain lightning				W	(I Antigrav for DOE HUD)
float IT2_SUPERHEALTH    	= 256.000;		//Correct for DOE HUD					I(available for a new weapon)
float IT2_EARTHQUAKE    	= 512.000;		//Correct for DOE HUD					I(available for a new weapon)
float IT2_V_SPHERE    		= 1024.000;		//Correct for DOE HUD					I(available for a new weapon)
float IT2_BOLTS		    	= 2048.000;		//Keep specific						I
float IT2_POISON    		= 4096.000;		//EMTPY SLOT						I
float HIP_IT2_WETSUIT    	= 8192.000;		//Keep specific (modified from HIP)			I
float HIP_IT2_EMPATHY_SHIELDS   = 16384.000;		//Keep specific (modified from HIP)			I
float HIP_IT2_HORN_OF_CONJURING = 32768.000;		//Keep specific (modified from HIP)			I
			   	//= 65536.000;		EMPTY SLOT (skipped...same as IT_PLASMA_GUN)		I
float IT2_PROXIMITY_GUN    	= 131072.000;		//Keep specific.Unique number for self.weapon calls 	W
float IT2_MJOLNIR 		= 262144.000;		//Keep specific	""					W
float IT2_LASER_CANNON    	= 524288.000;		//Keep specific	""					W
float IT2_GRAPPLE		= 1048576.000;		//Keep specific "" W Rogue grapple
float IT2_TRINITY		= 2097152.000;		//Quoth
float IT2_CROSS			= 4194304.000;		//Quoth							(available for a new weapon)
				//= 8388608.000; 	EMPTY SLOT
//========================================================================================================

//wad image names in array form, for easier access.
static string number[10] = {
	"gfx/num_0",
	"gfx/num_1",
	"gfx/num_2",
	"gfx/num_3",
	"gfx/num_4",
	"gfx/num_5",
	"gfx/num_6",
	"gfx/num_7",
	"gfx/num_8",
	"gfx/num_9"
};
static string anumber[10] = {
	"gfx/anum_0",
	"gfx/anum_1",
	"gfx/anum_2",
	"gfx/anum_3",
	"gfx/anum_4",
	"gfx/anum_5",
	"gfx/anum_6",
	"gfx/anum_7",
	"gfx/anum_8",
	"gfx/anum_9"
};
static string weaponnames[] = {
	"shotgun",
	"sshotgun",
	"nailgun",
	"snailgun",
	"rlaunch",
	"srlaunch",
	"lightng",
#define WEAPONICONS 7
#define LIGHTNING_WIDTH 48
};
static string facetab[] = {
	"gfx/face5", "gfx/face4", "gfx/face3", "gfx/face2", "gfx/face1",
	"gfx/face_p5", "gfx/face_p4", "gfx/face_p3", "gfx/face_p2", "gfx/face_p1",
	"gfx/face_inv2", "gfx/face_quad", "gfx/face_invis", "gfx/face_invul2"
};



//copies of stats, for easier access
static float sitems;		//copy of .items
static float sitems2;		//copy of .items2 (or serverflags)
static float sweapon;		//copy of .weapon
static float shealth;		//copy of .health

//Note: these are not tracked for splitscreen, which limits them to one player. We're lazy and just don't update them.
static float flashtime[WEAPONICONS];	//tracks the time that various items were obtained, so we can flash stuff accordingly.
static float painfinishtime;//tracks when we took damage, for flinching.


//registers the various pics so they're ready as needed.
static void() Hud_Init
{
	float i;
	precache_pic("gfx/sbar", TRUE);
	precache_pic("gfx/ibar", TRUE);
	for (i = 0; i < 10; i++)
	{
		precache_pic(number[i], TRUE);
		precache_pic(anumber[i], TRUE);
	}

	precache_pic("gfx/sb_armor1", TRUE);
	precache_pic("gfx/sb_armor2", TRUE);
	precache_pic("gfx/sb_armor3", TRUE);
	precache_pic("gfx/disc", TRUE);

	precache_pic("gfx/sb_shells", TRUE);
	precache_pic("gfx/sb_nails", TRUE);
	precache_pic("gfx/sb_rocket", TRUE);
	precache_pic("gfx/sb_cells", TRUE);

	precache_pic("gfx/sb_key1", TRUE);
	precache_pic("gfx/sb_key2", TRUE);
	precache_pic("gfx/sb_invis", TRUE);
	precache_pic("gfx/sb_invuln", TRUE);
	precache_pic("gfx/sb_suit", TRUE);
	precache_pic("gfx/sb_quad", TRUE);
	
	precache_pic("gfx/sb_sigil1", TRUE);
	precache_pic("gfx/sb_sigil2", TRUE);
	precache_pic("gfx/sb_sigil3", TRUE);
	precache_pic("gfx/sb_sigil4", TRUE);

	for (i = 0; i < weaponnames.length; i++)
	{
		precache_pic(strcat("gfx/inv_", weaponnames[i]), TRUE);
		precache_pic(strcat("gfx/inv2_", weaponnames[i]), TRUE);
		precache_pic(strcat("gfx/inva1_", weaponnames[i]), TRUE);
		precache_pic(strcat("gfx/inva2_", weaponnames[i]), TRUE);
		precache_pic(strcat("gfx/inva3_", weaponnames[i]), TRUE);
		precache_pic(strcat("gfx/inva4_", weaponnames[i]), TRUE);
		precache_pic(strcat("gfx/inva5_", weaponnames[i]), TRUE);
	}
	for (i = 0; i < facetab.length; i++)
		precache_pic(facetab[i], TRUE);
};




static void(vector pos, float value, float threshhold) Hud_DrawLargeValue =
{
	float c;
	float len;
	string s;
	if (value < 0)
		value = 0;	//hrm
	if (value>999)
		value = 999;

	s = ftos(floor(value));
	len = strlen(s);

	pos_x += 24 * (3-len);
	

	if (value <= threshhold)
	{	//use alternate (red) numbers

		while(len>0)
		{
			len--;
			c = str2chr(s, len);

			drawpic(pos+len * '24 0 0', anumber[c-'0'], '24 24 0', '1 1 1', 1, 0);
		}
	}
	else
	{	//use normal numbers

		while(len>0)
		{
			len--;
			c = str2chr(s, len);

			drawpic(pos+len * '24 0 0', number[c-'0'], '24 24 0', '1 1 1', 1, 0);
		}
	}
};

static void(float type, vector pos, float drawback) Hud_DrawAmmoCount =
{
	string s;
	float value;
	float len;
	float c;
	value = getstatf(STAT_SHELLS+type);
	if (value < 0)
		value = 0;	//hrm
	if (value>999)
		value = 999;
		
	if (drawback)
	{
		drawsubpic(pos, '42 11', "gfx/ibar", [(3.0/320)+type*(48.0/320), 0], [42.0/320, 11.0/24], '1 1 1', autocvar(scr_sbaralpha, 0.75), 0);
		pos_x += 8;
	}

	s = ftos(floor(value));
	len = strlen(s);

	pos_x += 8 * (3-len);
	while(len>0)
	{
		len--;
		c = str2chr(s, len);
		drawcharacter(pos+len * '8 0 0', (18-'0') + c, '8 8 0', '1 1 1', 1, 0);
	}
};

static void(float num, vector pos) Hud_DrawWeapon =
{
	string prefix;
	float flash = (time - flashtime[num])*10;
	if (flash >= 10)
	{
		if (sweapon == (1<<num))
			prefix = "gfx/inv2_";
		else
			prefix = "gfx/inv_";
	}
	else
	{
		static string tab[] = {"gfx/inva1_","gfx/inva2_","gfx/inva3_","gfx/inva4_","gfx/inva5_"};
		if (flash >= 5)
			flash -= 5;
		prefix = tab[flash];
	}

	drawpic(pos, strcat(prefix, weaponnames[num]), '24 16 0', '1 1 1', 1, 0);
};
static void(float num, float lflashtime, float iscurrent, vector pos, float drawwidth, float imgwidth) Hud_DrawWeaponEx =
{
	string prefix;
	float flash = (time - lflashtime)*10;
	if (flash >= 10)
	{
		if (iscurrent)
			prefix = "gfx/inv2_";
		else
			prefix = "gfx/inv_";
	}
	else
	{
		static string tab[] = {"gfx/inva1_","gfx/inva2_","gfx/inva3_","gfx/inva4_","gfx/inva5_"};
		if (flash >= 5)
			flash -= 5;
		prefix = tab[flash];
	}
	drawsubpic(pos, [drawwidth, 16], strcat(prefix, weaponnames[num]), '0 0 0', [drawwidth/imgwidth, 1], '1 1 1', 1, 0);
};

static string(float f, float chars, string lead) FormatFloat
{
	string s = ftos(f);
	if (f < 10 && chars >= 3)
		s = strcat(lead, lead, s);
	else if (f < 10 && chars == 2)
		s = strcat(lead, s);
	else if (f < 100 && chars >= 3)
		s = strcat(lead, s);
	return s;
};

static void(vector pos) Hud_CoopScores_SBar =
{
	string s;
	float nsecs;
	float nmins;
	vector sbar = pos;
	drawpic(sbar, "gfx/scorebar", '320 24 0', '1 1 1', autocvar(scr_sbaralpha, 0.75), 0);

	s = strcat("Monsters:", FormatFloat(getstatf(STAT_KILLEDMONSTERS), 3, " "), "/", FormatFloat(getstatf(STAT_TOTALMONSTERS), 3, " "));
	drawstring(sbar + '8 4', s, '8 8 0', '1 1 1', 1, 0);
	s = strcat("Secrets :", FormatFloat(getstatf(STAT_FOUNDSECRETS), 3, " "), "/", FormatFloat(getstatf(STAT_TOTALSECRETS), 3, " "));
	drawstring(sbar + '8 12', s, '8 8 0', '1 1 1', 1, 0);

	nmins = floor(time/60);
	nsecs = floor(time - nmins*60);
	s = strcat("Time :", FormatFloat(nmins, 3, " "), ":", FormatFloat(nsecs, 2, "0"));
	drawstring(sbar + '184 4', s, '8 8 0', '1 1 1', 1, 0);

	drawstring(sbar + '232 12' - strlen(world.message)*'4 0', world.message, '8 8 0', '1 1 1', 1, 0);
};

static void Hud_PicOrModel(vector topleft, string pic, string showmodel, float showskin, float speed)
{
	vector sz = '24 24';
#ifdef DP_QC_RENDER_SCENE	//requires fteextensions.qc instead of qsextensions.qc
	if (autocvar(cl_sbar, 1) >= 3)
	if (checkextension("DP_QC_RENDER_SCENE"))
	{
		//reset view props and wipe entity lists
		clearscene();

		//set up the various view details, set up the renderer to be a little larger than the intended size... it avoids harsh cuts.
		setviewprop(VF_MIN, topleft - sz);
		setviewprop(VF_SIZE, sz * 3);
		setviewprop(VF_DRAWENGINESBAR, FALSE);
		setviewprop(VF_DRAWCROSSHAIR, FALSE);
		setviewprop(VF_DRAWWORLD, FALSE);
		setviewprop(VF_AFOV, 30);	//a small fov reduces perspective distortions without making it look like its isometric.
		setviewprop(VF_ANGLES, '15 0 0');	//angle downwards slightly
		makevectors('15 0 0');
		setviewprop(VF_ORIGIN, v_forward*-256);	//and move the camera away so that we can position the model at the center without needing to worry about the tilt.

		//spawn a (reusable) entity for our scene
		static entity hudpicent;
		if (!hudpicent)
			hudpicent = spawn();
		precache_model(showmodel);
		setmodel(hudpicent, showmodel);
		hudpicent.angles_y = cltime*speed;
		hudpicent.skin = showskin;
		makevectors(hudpicent.angles);
		//make sure its centered
		vector org = (hudpicent.maxs+hudpicent.mins)*0.5;
		setorigin(hudpicent, - v_forward*org_x + v_right*org_y - v_up*org_z);
		//and now copy it into the scene
		addentity(hudpicent);

		//and ask the engine to actually draw that single-entity scene. Think of this as a drawpic call.
		renderscene();

		return;
	}
#endif
	drawpic(topleft, pic, '24 24 0', '1 1 1', 1, 0);
};

static void Hud_DrawFace(vector pos)
{
	string face;
	if (sitems & IT_INVISIBILITY)
	{	//invis can go first, because its the most obvious image difference.
		if (sitems & IT_INVULNERABILITY)
			face = facetab[10];	//special double-powerup image!
		else
			face = facetab[12];
	}
	else if (sitems & IT_QUAD)
		face = facetab[11];
	else if (sitems & IT_INVULNERABILITY)
		face = facetab[13];
	else
	{
		float frame = shealth/20;
		if (frame < 0)
			frame = 0;
		else if (frame > 4)
			frame = 4;
		if (painfinishtime > time)
			frame += 5;	//pain.
		face = facetab[frame];
	}
	
	if (shealth > 100)
		Hud_PicOrModel(pos, face, "maps/b_bh100.bsp", 0, -90);
	else if (shealth > 50)
		Hud_PicOrModel(pos, face, "maps/b_bh25.bsp", 0, -90);
	else
		Hud_PicOrModel(pos, face, "maps/b_bh10.bsp", 0, -90);
};


static void Hud_DrawSBar(vector pos, vector virtsize)
{
	if (autocvar(cl_sbar, 1) >= 2)
	{	//n64-style
		pos = [0, virtsize_y-24];
		Hud_DrawFace(pos+[3*24,0]);
		Hud_DrawLargeValue(pos, shealth, 25);

		if (sitems & (IT_INVULNERABILITY|IT_ARMOR1|IT_ARMOR2|IT_ARMOR3))
		{
			pos_y -= 24;
			if (sitems & IT_INVULNERABILITY)
				Hud_DrawLargeValue(pos, 999, 25);
			else
				Hud_DrawLargeValue(pos, getstatf(STAT_ARMOR), 25);
			pos+=[3*24,0];
			if (sitems & IT_INVULNERABILITY)
				Hud_PicOrModel(pos, "gfx/disc", "progs/invulner.mdl", 0, 70);
			else if (sitems & IT_ARMOR3)
				Hud_PicOrModel(pos, "gfx/sb_armor3", "progs/armor.mdl", 2, 70);
			else if (sitems & IT_ARMOR2)
				Hud_PicOrModel(pos, "gfx/sb_armor2", "progs/armor.mdl", 1, 70);
			else if (sitems & IT_ARMOR1)
				Hud_PicOrModel(pos, "gfx/sb_armor1", "progs/armor.mdl", 0, 70);
		}

		pos = virtsize - [24,24];
		if (sitems & IT_SHELLS)
			Hud_PicOrModel(pos, "gfx/sb_shells", "maps/b_shell1.bsp", 0, 80);
		else if (sitems & IT_NAILS)
			Hud_PicOrModel(pos, "gfx/sb_nails", "maps/b_nail1.bsp", 0, 80);
		else if (sitems & IT_ROCKETS)
			Hud_PicOrModel(pos, "gfx/sb_rocket", "maps/b_rock1.bsp", 0, 80);
		else if (sitems & IT_CELLS)
			Hud_PicOrModel(pos, "gfx/sb_cells", "maps/b_batt1.bsp", 0, 80);
		Hud_DrawLargeValue(pos-[24*3], getstatf(STAT_AMMO), 10);
		return;
	}
	
	if (autocvar(cl_sbar, 1))
		drawpic(pos, "gfx/sbar", '320 24 0', '1 1 1', autocvar(scr_sbaralpha, 0.75), 0);

	if (sitems & IT_INVULNERABILITY)
	{
		drawpic(pos, "gfx/disc", '24 24 0', '1 1 1', 1, 0);
		Hud_DrawLargeValue(pos+'24 0 0', 999, 25);
	}
	else
	{
		Hud_DrawLargeValue(pos+'24 0 0', getstatf(STAT_ARMOR), 25);
		if (sitems & IT_ARMOR3)
			drawpic(pos, "gfx/sb_armor3", '24 24 0', '1 1 1', 1, 0);
		else if (sitems & IT_ARMOR2)
			drawpic(pos, "gfx/sb_armor2", '24 24 0', '1 1 1', 1, 0);
		else if (sitems & IT_ARMOR1)
			drawpic(pos, "gfx/sb_armor1", '24 24 0', '1 1 1', 1, 0);
	}


	Hud_DrawFace(pos+'112 0 0');
	Hud_DrawLargeValue(pos+'136 0 0', shealth, 25);

	if (sitems & IT_SHELLS)
		drawpic(pos+'224 0 0', "gfx/sb_shells", '24 24 0', '1 1 1', 1, 0);
	else if (sitems & IT_NAILS)
		drawpic(pos+'224 0 0', "gfx/sb_nails", '24 24 0', '1 1 1', 1, 0);
	else if (sitems & IT_ROCKETS)
		drawpic(pos+'224 0 0', "gfx/sb_rocket", '24 24 0', '1 1 1', 1, 0);
	else if (sitems & IT_CELLS)
		drawpic(pos+'224 0 0', "gfx/sb_cells", '24 24 0', '1 1 1', 1, 0);
	Hud_DrawLargeValue(pos+'248 0 0', getstatf(STAT_AMMO), 10);
};

static void Hud_DrawIBar(vector pos, vector br)
{
	if (!autocvar(cl_sbar, 1) || autocvar(cl_sbar, 1) >= 2)
	{	//qw puts its ammo+weapons on the side. mimic that.
		vector pos2, pos3;
		const float hheight = (WEAPONICONS*16)+(11*4);
		pos2 = [0, br_y-48, 0];
		if (pos2_y < hheight)
			pos2_y = hheight;
		if (autocvar(cl_hudswap, 0))
			pos3 = pos2 + [0,-44];	//hudswap puts it on the left
		else
		{	//otherwise align to the right edge
			pos2_x += br_x-42;
			pos3 = pos2 + [42-24,-44];
		}
		if (autocvar(cl_sbar, 1) >= 2)
		{
			pos3_y = 0;	//n64-style hud puts them at the top, apparently.
			pos_x = br_x-320;	//align any keys+powerups as though the hud was aligned to the right.
			if (pos2_y < hheight)
				pos2_y = hheight;
		}
		else pos3_y -= WEAPONICONS*16;
		Hud_DrawAmmoCount(0, pos2 + '0 -44', TRUE);
		Hud_DrawAmmoCount(1, pos2 + '0 -33', TRUE);
		Hud_DrawAmmoCount(2, pos2 + '0 -22', TRUE);
		Hud_DrawAmmoCount(3, pos2 + '0 -11', TRUE);
		if (sitems & IT_SHOTGUN)			Hud_DrawWeapon(0, pos3+[0,16*0]);
		if (sitems & IT_SUPER_SHOTGUN)		Hud_DrawWeapon(1, pos3+[0,16*1]);
		if (sitems & IT_NAILGUN)			Hud_DrawWeapon(2, pos3+[0,16*2]);
		if (sitems & IT_SUPER_NAILGUN)		Hud_DrawWeapon(3, pos3+[0,16*3]);
		if (sitems & IT_GRENADE_LAUNCHER)	Hud_DrawWeapon(4, pos3+[0,16*4]);
		if (sitems & IT_ROCKET_LAUNCHER)	Hud_DrawWeapon(5, pos3+[0,16*5]);
		if (sitems & IT_LIGHTNING)			Hud_DrawWeaponEx(6, flashtime[6], sweapon==IT_LIGHTNING, pos3+[0,16*6], 24, LIGHTNING_WIDTH);
	}
	else
	{	//standard nq sbar
		drawpic(pos, "gfx/ibar", '320 24 0', '1 1 1', autocvar(scr_sbaralpha, 0.75), 0);

		if (sitems & IT_SHOTGUN)			Hud_DrawWeapon(0, pos+[24*0,8]);
		if (sitems & IT_SUPER_SHOTGUN)		Hud_DrawWeapon(1, pos+[24*1,8]);
		if (sitems & IT_NAILGUN)			Hud_DrawWeapon(2, pos+[24*2,8]);
		if (sitems & IT_SUPER_NAILGUN)		Hud_DrawWeapon(3, pos+[24*3,8]);
		 if (sitems & IT_GRENADE_LAUNCHER)	Hud_DrawWeapon(4, pos+[24*4,8]);
		if (sitems & IT_ROCKET_LAUNCHER)	Hud_DrawWeapon(5, pos+[24*5,8]);
		if (sitems & IT_LIGHTNING)			Hud_DrawWeaponEx(6, flashtime[6], sweapon==IT_LIGHTNING, pos+[24*6,8], LIGHTNING_WIDTH, LIGHTNING_WIDTH);

		Hud_DrawAmmoCount(0, pos + '10 0 0', FALSE);
		Hud_DrawAmmoCount(1, pos + '58 0 0', FALSE);
		Hud_DrawAmmoCount(2, pos + '106 0 0', FALSE);
		Hud_DrawAmmoCount(3, pos + '154 0 0', FALSE);
	}

	if (sitems & IT_KEY1)
		drawpic(pos+'192 8 0', "gfx/sb_key1", '16 16 0', '1 1 1', 1, 0);
	if (sitems & IT_KEY2)
		drawpic(pos+'208 8 0', "gfx/sb_key2", '16 16 0', '1 1 1', 1, 0);
	if (sitems & IT_INVISIBILITY)
		drawpic(pos+'224 8 0', "gfx/sb_invis", '16 16 0', '1 1 1', 1, 0);
	if (sitems & IT_INVULNERABILITY)
		drawpic(pos+'240 8 0', "gfx/sb_invuln", '16 16 0', '1 1 1', 1, 0);
	if (sitems & IT_SUIT)
		drawpic(pos+'256 8 0', "gfx/sb_suit", '16 16 0', '1 1 1', 1, 0);
	if (sitems & IT_QUAD)
		drawpic(pos+'272 8 0', "gfx/sb_quad", '16 16 0', '1 1 1', 1, 0);

	if (sitems2 & IT2_RUNE1)
		drawpic(pos+'288 8 0', "gfx/sb_sigil1", '8 16 0', '1 1 1', 1, 0);
	if (sitems2 & IT2_RUNE2)
		drawpic(pos+'296 8 0', "gfx/sb_sigil2", '8 16 0', '1 1 1', 1, 0);
	if (sitems2 & IT2_RUNE3)
		drawpic(pos+'304 8 0', "gfx/sb_sigil3", '8 16 0', '1 1 1', 1, 0);
	if (sitems2 & IT2_RUNE4)
		drawpic(pos+'312 8 0', "gfx/sb_sigil4", '8 16 0', '1 1 1', 1, 0);

	if (deathmatch)
	{	//should probably show team scores, but this mimics vanilla
		pos_x += 194;
		for (float i = -1; i >= -4; i--)
		{
			string frags = getplayerkeyvalue(i, "frags");
			if not (frags)
				break;
			vector top = stov(getplayerkeyvalue(i, "topcolor_rgb"));
			vector bot = stov(getplayerkeyvalue(i, "bottomcolor_rgb"));
			drawfill(pos+[0,1], [28,3], top, 0.75, 0);
			drawfill(pos+[0,4], [28,4], bot, 0.75, 0);
			drawstring(pos+[2,0], strpad(-3, frags), [8,8], [1,1,1],1,0);
			if (player_localentnum == stof(getplayerkeyvalue(i, "viewentity")))
			{
				drawcharacter(pos+[-4,0], 0xe010, [8,8], [1,1,1],1, 0);
				drawcharacter(pos+[24,0], 0xe011, [8,8], [1,1,1],1, 0);
			}
			pos_x += 8*4;
		}
	}
};

float numclientseats;
static void Hud_Draw(vector virtsize, float scoreboard)
{
	vector pos;
	float oitems = sitems;
	float oitems2 = sitems2;
	float ohealth = shealth;

	pos_x = (virtsize_x-320)/2;
	pos_y = virtsize_y;

	sitems = getstatbits(STAT_ITEMS, 0, 23);	//note that we could use getstati to read the items stat, but it'd degrade when then cast to a float, and QSS+DP don't support ints, so we have to use two floats. 
	sitems2 = getstatbits(STAT_ITEMS, 23, 9);
	sweapon = getstatf(STAT_ACTIVEWEAPON);
	shealth = getstatf(STAT_HEALTH);
	//note that ssqc can provide custom stats, eg: clientstat(STAT_USER+n, EV_FLOAT, items3); can then be read by us with float items3 = getstatf(STAT_USER+n);
	
	if (shealth <= 0)
		scoreboard = TRUE;

	if (numclientseats <= 1)
	{	//note that we only track one player's stats. with splitscreen we need to stop looking for changes otherwise we'd see them all the time.
		if (ohealth && sitems != oitems)
		{
			for (float f = 0; f < 7; f++)
			{
				if ((sitems & (1<<f)) && !(oitems & (1<<f)))
					flashtime[f] = time;
			}
		}
	}

	if (autocvar(viewsize, 100) < 120)
	{
		if (scoreboard)
			Hud_CoopScores_SBar(pos - '0 24 0');
		else
			Hud_DrawSBar(pos - '0 24 0', virtsize);

		if (autocvar(viewsize, 100) < 110)
			Hud_DrawIBar(pos - '0 48 0', virtsize);
	}
};

//lame entry point, just so that we know when damage was taken
float(float save, float take, vector dir) CSQC_Parse_Damage =
{
	if (numclientseats <= 1)
		painfinishtime = time + 0.2;
	return FALSE;	//let the engine handle any red tints etc
};

//Hud entry point!
//showscores is set if +scores is held (check health yourself).
void(vector virtsize, float showscores) CSQC_DrawHud =
{
	Hud_Draw(virtsize, showscores);
};

void(float apilevel, string enginename, float engineversion) CSQC_Init =
{
	Hud_Init();
};

