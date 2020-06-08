void() main =
{
	dprint ("main function\n");
	
// these are just commands to the prog compiler to copy these files

	precache_file ("progs.dat");
	precache_file ("gfx.wad");
	precache_file ("quake.rc");
	precache_file ("default.cfg");

	precache_file ("end1.bin");
	precache_file2 ("end2.bin");

	precache_file ("demo1.dem");
	precache_file ("demo2.dem");
	precache_file ("demo3.dem");

//
// these are all of the lumps from the cached.ls files
//
	precache_file ("gfx/palette.lmp");
	precache_file ("gfx/colormap.lmp");

	precache_file2 ("gfx/pop.lmp");

	precache_file ("gfx/complete.lmp");
	precache_file ("gfx/inter.lmp");

	precache_file ("gfx/ranking.lmp");
	precache_file ("gfx/vidmodes.lmp");
	precache_file ("gfx/finale.lmp");
	precache_file ("gfx/conback.lmp");
	precache_file ("gfx/qplaque.lmp");

	precache_file ("gfx/menudot1.lmp");
	precache_file ("gfx/menudot2.lmp");
	precache_file ("gfx/menudot3.lmp");
	precache_file ("gfx/menudot4.lmp");
	precache_file ("gfx/menudot5.lmp");
	precache_file ("gfx/menudot6.lmp");

	precache_file ("gfx/menuplyr.lmp");
	precache_file ("gfx/bigbox.lmp");
	precache_file ("gfx/dim_modm.lmp");
	precache_file ("gfx/dim_drct.lmp");
	precache_file ("gfx/dim_ipx.lmp");
	precache_file ("gfx/dim_tcp.lmp");
	precache_file ("gfx/dim_mult.lmp");
	precache_file ("gfx/mainmenu.lmp");
	
	precache_file ("gfx/box_tl.lmp");
	precache_file ("gfx/box_tm.lmp");
	precache_file ("gfx/box_tr.lmp");
	
	precache_file ("gfx/box_ml.lmp");
	precache_file ("gfx/box_mm.lmp");
	precache_file ("gfx/box_mm2.lmp");
	precache_file ("gfx/box_mr.lmp");
	
	precache_file ("gfx/box_bl.lmp");
	precache_file ("gfx/box_bm.lmp");
	precache_file ("gfx/box_br.lmp");
	
	precache_file ("gfx/sp_menu.lmp");
	precache_file ("gfx/ttl_sgl.lmp");
	precache_file ("gfx/ttl_main.lmp");
	precache_file ("gfx/ttl_cstm.lmp");
	
	precache_file ("gfx/mp_menu.lmp");
	
	precache_file ("gfx/netmen1.lmp");
	precache_file ("gfx/netmen2.lmp");
	precache_file ("gfx/netmen3.lmp");
	precache_file ("gfx/netmen4.lmp");
	precache_file ("gfx/netmen5.lmp");
	
	precache_file ("gfx/sell.lmp");
	
	precache_file ("gfx/help0.lmp");
	precache_file ("gfx/help1.lmp");
	precache_file ("gfx/help2.lmp");
	precache_file ("gfx/help3.lmp");
	precache_file ("gfx/help4.lmp");
	precache_file ("gfx/help5.lmp");

	precache_file ("gfx/pause.lmp");
	precache_file ("gfx/loading.lmp");

	precache_file ("gfx/p_option.lmp");
	precache_file ("gfx/p_load.lmp");
	precache_file ("gfx/p_save.lmp");
	precache_file ("gfx/p_multi.lmp");

// sounds loaded by C code
	precache_sound ("misc/menu1.wav");
	precache_sound ("misc/menu2.wav");
	precache_sound ("misc/menu3.wav");

	precache_sound ("ambience/water1.wav");
	precache_sound ("ambience/wind2.wav");

// shareware
	precache_file ("maps/start.bsp");

	precache_file ("maps/e1m1.bsp");
	precache_file ("maps/e1m2.bsp");
	precache_file ("maps/e1m3.bsp");
	precache_file ("maps/e1m4.bsp");
	precache_file ("maps/e1m5.bsp");
	precache_file ("maps/e1m6.bsp");
	precache_file ("maps/e1m7.bsp");
	precache_file ("maps/e1m8.bsp");

// registered
	precache_file2 ("gfx/pop.lmp");

	precache_file2 ("maps/e2m1.bsp");
	precache_file2 ("maps/e2m2.bsp");
	precache_file2 ("maps/e2m3.bsp");
	precache_file2 ("maps/e2m4.bsp");
	precache_file2 ("maps/e2m5.bsp");
	precache_file2 ("maps/e2m6.bsp");
	precache_file2 ("maps/e2m7.bsp");

	precache_file2 ("maps/e3m1.bsp");
	precache_file2 ("maps/e3m2.bsp");
	precache_file2 ("maps/e3m3.bsp");
	precache_file2 ("maps/e3m4.bsp");
	precache_file2 ("maps/e3m5.bsp");
	precache_file2 ("maps/e3m6.bsp");
	precache_file2 ("maps/e3m7.bsp");

	precache_file2 ("maps/e4m1.bsp");
	precache_file2 ("maps/e4m2.bsp");
	precache_file2 ("maps/e4m3.bsp");
	precache_file2 ("maps/e4m4.bsp");
	precache_file2 ("maps/e4m5.bsp");
	precache_file2 ("maps/e4m6.bsp");
	precache_file2 ("maps/e4m7.bsp");
	precache_file2 ("maps/e4m8.bsp");

	precache_file2 ("maps/end.bsp");

	precache_file2 ("maps/dm1.bsp");
	precache_file2 ("maps/dm2.bsp");
	precache_file2 ("maps/dm3.bsp");
	precache_file2 ("maps/dm4.bsp");
	precache_file2 ("maps/dm5.bsp");
	precache_file2 ("maps/dm6.bsp");
}



//=======================
/*QUAKED worldspawn (0 0 0) ?

Keys:
"light" default light level
"message" sets the title of the map
"sounds" CD track to play
"wad" which graphics wads to use. semicolon delineated, no spaces
"worldtype" style of keys to spawn
    0 = medieval
    1 = metal
    2 = base
"gravity" how fast your dreams come crashing down

"_sunlight" "#"
"_sun_mangle" "# # 0" yaw pitch roll. pitch is 90 to -90 degrees up to down.
"_sunlight_color" "# # #"

"_dirt" use dirtmapping
"_dirtdepth" length of dirt rays, default 128.
"_dirtscale" Scale factor used in dirt calculations, default 1 (higher is darker)
"_dirtgain" Exponent used in dirt calculation, default 1 (lower is darker)
*/
/*FGD
@SolidClass = worldspawn : "World entity"
[
	message(string) : "Map name"
	worldtype(choices) : "Key Style" : 0 =
	[
		0 : "Medieval"
		1 : "Metal (runic)"
		2 : "Base"
	]
	sounds(integer) : "CD track to play" : 0
	light(integer) : "Ambient light"
	gravity(integer) : "Gravity"
	_sunlight(integer) : "Sunlight"
	_sun_mangle(string) : "Sun mangle (Yaw pitch roll)"
	_sunlight_color(string) : "Sun color"
	_dirt(integer) : "use dirtmapping" : 0
	_dirtdepth(integer) : "length of dirt rays" : 128
	_dirtscale(string) : "Scale factor used in dirt calculations, higher is darker" : "1.0"
	_dirtgain(string) : "Exponent used in dirt calculation, lower is darker" : "1.0"
]
*/
//=======================
void() worldspawn =
{
	lastspawn = world;
	startingserverflags = serverflags;
	
	InitBodyQueue ();
	InitLightStyles();	// moved to lights.qc

	if (!self.gravity)
	{
		if (self.model == "maps/e1m8.bsp")
		{
			self.gravity = 100;
		}
		else
		{
			self.gravity = 800;
		}
	}
	cvar_set("sv_gravity", ftos(self.gravity));
	self.light_lev = 0;	// shut up FTEQCC warning
	
// the area based ambient sounds MUST be the first precache_sounds

// player precaches	
	W_Precache ();			// get weapon precaches

	precache_sound ("misc/null.wav");

	// sounds used from C physics code
	precache_sound ("demon/dland2.wav");		// landing thud
	precache_sound ("misc/h2ohit1.wav");		// landing splash

	// player sounds
	precache_sound ("items/itembk2.wav");		// item respawn sound
	precache_sound ("player/plyrjmp8.wav");		// player jump
	precache_sound ("player/land.wav");			// player landing
	precache_sound ("player/land2.wav");		// player hurt landing
	precache_sound ("player/drown1.wav");		// drowning pain
	precache_sound ("player/drown2.wav");		// drowning pain
	precache_sound ("player/gasp1.wav");		// gasping for air
	precache_sound ("player/gasp2.wav");		// taking breath
	precache_sound ("player/h2odeath.wav");		// drowning death

	precache_sound ("misc/talk.wav");			// talk
	precache_sound ("player/teledth1.wav");		// telefrag
	precache_sound ("misc/r_tele1.wav");		// teleport sounds
	precache_sound ("misc/r_tele2.wav");
	precache_sound ("misc/r_tele3.wav");
	precache_sound ("misc/r_tele4.wav");
	precache_sound ("misc/r_tele5.wav");
	
// iw -- fix powerup cheat sounds
// sounds used by cheats
	precache_sound ("items/suit2.wav");			// suit (finish)
	precache_sound ("items/protect2.wav");		// pent (finish)
	precache_sound ("items/protect3.wav");		// pent (protect)
	precache_sound ("items/inv2.wav");			// ring (finish)
	precache_sound ("items/inv3.wav");			// ring (idle)
	precache_sound ("items/damage2.wav");		// quad (finish)
	precache_sound ("items/damage3.wav");		// quad (attack)
// iw -- END

	// items
	precache_sound ("weapons/lock4.wav");		// ammo pick up
	precache_sound ("weapons/pkup.wav");		// weapon up
	precache_sound ("items/armor1.wav");		// armor up

	precache_sound3("items/pack.wav");	// backpacks can play either of these
	precache_sound3("items/patch.wav");

	// player gib sounds
	precache_sound ("player/gib.wav");			// player gib sound
	precache_sound ("player/udeath.wav");		// player gib sound
	precache_sound ("player/tornoff2.wav");		// gib sound

// player pain sounds

	precache_sound ("player/pain1.wav");
	precache_sound ("player/pain2.wav");
	precache_sound ("player/pain3.wav");
	precache_sound ("player/pain4.wav");
	precache_sound ("player/pain5.wav");
	precache_sound ("player/pain6.wav");

// player death sounds
	precache_sound ("player/death1.wav");
	precache_sound ("player/death2.wav");
	precache_sound ("player/death3.wav");
	precache_sound ("player/death4.wav");
	precache_sound ("player/death5.wav");

// liquids
	precache_sound ("player/h2ojump.wav");		// player jumping into water
	precache_sound ("player/slimbrn2.wav");		// player enter slime
	precache_sound ("player/inh2o.wav");		// player enter water
	precache_sound ("player/inlava.wav");		// player enter lava
	precache_sound ("misc/outwater.wav");		// leaving water sound
	precache_sound ("player/lburn1.wav");		// lava burn
	precache_sound ("player/lburn2.wav");		// lava burn
	precache_sound ("misc/water1.wav");			// swimming
	precache_sound ("misc/water2.wav");			// swimming

	precache_model ("progs/player.mdl");
	precache_model ("progs/eyes.mdl");
	precache_model ("progs/h_player.mdl");
	precache_model ("progs/gib1.mdl");
	precache_model ("progs/gib2.mdl");
	precache_model ("progs/gib3.mdl");

	precache_model ("progs/s_bubble.spr");	// drowning bubbles
	precache_model ("progs/s_explod.spr");	// sprite explosion

	precache_model ("progs/bolt.mdl");		// for lightning gun
	precache_model ("progs/bolt2.mdl");		// for lightning gun
	precache_model ("progs/bolt3.mdl");		// for boss shock
	precache_model ("progs/lavaball.mdl");	// for testing
	
	precache_model ("progs/missile.mdl");
	precache_model ("progs/grenade.mdl");
	precache_model ("progs/spike.mdl");
	//precache_model ("progs/s_spike.mdl");

	precache_model ("progs/backpack.mdl");

	precache_model ("progs/zom_gib.mdl");
	
}

void() StartFrame =
{
	teamplay = cvar("teamplay");
	skill = cvar("skill");

	//if (framecount != cvar("registered"))
	
	if (cleanUpClientShit)
		cleanUpClientShit -= 1;
	else if (!gamestarted && framecount > 2)
	{
		if (framecount != 3)
			cleanUpClientShit += 2;
		
		gamestarted = TRUE;
	}
		
	framecount++;
	//cvar_set("registered",ftos(framecount));
	
	/*
	'activator' should be a parameter of *_use(), but it's a global. so it goes.

	this can lead to lots of hard-to-repro bugs in cases where 'activator' isn't set
	properly, since it just happens to be set to the player anyway *most* of the time.

	for this reason I enforce a global ban on activator ever being 'world' (via a
	gameplay-ending error() in SUB_UseTargets) and nulling it out every single frame.
	stale or incorrectly set activators are thus smoked out immediately, and not six
	months from now the first time a monster touches a door field at a critical but
	impossible to reproduce moment.
	*/
	activator = world;
}

/*
==============================================================================

BODY QUE[UE ~ ed.]

==============================================================================
*/

void() bodyqueue =
{	// just here so spawn functions don't complain after the world creates bodyqueues
}

void() InitBodyQueue =
{
	bodyqueue_head = spawn();
	bodyqueue_head.classname = "bodyqueue";
	bodyqueue_head.owner = spawn();
	bodyqueue_head.owner.classname = "bodyqueue";
	bodyqueue_head.owner.owner = spawn();
	bodyqueue_head.owner.owner.classname = "bodyqueue";
	bodyqueue_head.owner.owner.owner = spawn();
	bodyqueue_head.owner.owner.owner.classname = "bodyqueue";
	bodyqueue_head.owner.owner.owner.owner = bodyqueue_head;
}


// make a body queue entry for the given ent so the ent can be
// respawned elsewhere
void(entity ent) CopyToBodyQueue =
{
	bodyqueue_head.angles = ent.angles;
	bodyqueue_head.model = ent.model;
	bodyqueue_head.modelindex = ent.modelindex;
	bodyqueue_head.frame = ent.frame;
	bodyqueue_head.colormap = ent.colormap;
	bodyqueue_head.movetype = ent.movetype;
	bodyqueue_head.velocity = ent.velocity;
	bodyqueue_head.flags = 0;
	setorigin (bodyqueue_head, ent.origin);
	setsize (bodyqueue_head, ent.mins, ent.maxs);
	bodyqueue_head = bodyqueue_head.owner;
}