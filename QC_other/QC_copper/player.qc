/*
==============================================================================

PLAYER

==============================================================================
*/

$cd id1/models/player_4
$origin 0 -6 24
$base base		
$skin skin

// running 0-11
$frame axrun1 axrun2 axrun3 axrun4 axrun5 axrun6
$frame rockrun1 rockrun2 rockrun3 rockrun4 rockrun5 rockrun6

// standing 12-28
$frame stand1 stand2 stand3 stand4 stand5
$frame axstnd1 axstnd2 axstnd3 axstnd4 axstnd5 axstnd6
$frame axstnd7 axstnd8 axstnd9 axstnd10 axstnd11 axstnd12

// pain 13-40
$frame axpain1 axpain2 axpain3 axpain4 axpain5 axpain6
$frame pain1 pain2 pain3 pain4 pain5 pain6

// death 41-102
$frame axdeth1 axdeth2 axdeth3 axdeth4 axdeth5 axdeth6 axdeth7 axdeth8 axdeth9
$frame deatha1 deatha2 deatha3 deatha4 deatha5 deatha6 deatha7 deatha8 deatha9 deatha10 deatha11
$frame deathb1 deathb2 deathb3 deathb4 deathb5 deathb6 deathb7 deathb8 deathb9
$frame deathc1 deathc2 deathc3 deathc4 deathc5 deathc6 deathc7 deathc8 deathc9 deathc10 deathc11 deathc12 deathc13 deathc14 deathc15
$frame deathd1 deathd2 deathd3 deathd4 deathd5 deathd6 deathd7 deathd8 deathd9
$frame deathe1 deathe2 deathe3 deathe4 deathe5 deathe6 deathe7 deathe8 deathe9

// gun attacks 103-118
$frame nailatt1 nailatt2
$frame light1 light2
$frame rockatt1 rockatt2 rockatt3 rockatt4 rockatt5 rockatt6
$frame shotatt1 shotatt2 shotatt3 shotatt4 shotatt5 shotatt6

// ax attax 119-142
$frame axatt1 axatt2 axatt3 axatt4 axatt5 axatt6
$frame axattb1 axattb2 axattb3 axattb4 axattb5 axattb6
$frame axattc1 axattc2 axattc3 axattc4 axattc5 axattc6
$frame axattd1 axattd2 axattd3 axattd4 axattd5 axattd6


/*
==============================================================================
PLAYER
==============================================================================
*/

void()	player_stand1 =[	$axstnd1,	player_stand1	]
{
	W_SetIdleWeaponFrame();
	if (self.velocity_x || self.velocity_y)
	{
		self.walkframe=0;
		player_run();
		return;
	}

	if (self.weapon == IT_AXE )
	{
		if (self.walkframe >= 12)
			self.walkframe = 0;
		self.frame = $axstnd1 + self.walkframe;
	}
	else
	{
		if (self.walkframe >= 5)
			self.walkframe = 0;
		self.frame = $stand1 + self.walkframe;
	}
	self.walkframe = self.walkframe + 1;	
}

void()	player_run =[	$rockrun1,	player_run	]
{
	W_SetIdleWeaponFrame();
	if (!self.velocity_x && !self.velocity_y)
	{
		self.walkframe=0;
		player_stand1();
		return;
	}

	if (self.weapon == IT_AXE )
	{
		if (self.walkframe >= 6)
			self.walkframe = 0;
		self.frame = $axrun1 + self.walkframe;
	}
	else
	{
		if (self.walkframe >= 6)
			self.walkframe = 0;
		self.frame = self.frame + self.walkframe;
	}
	self.walkframe = self.walkframe + 1;
}

void()	player_shot1 =	[$shotatt1, player_shot2	] {self.weaponframe=1;
self.effects = self.effects | EF_MUZZLEFLASH;}
void()	player_shot2 =	[$shotatt2, player_shot3	] {self.weaponframe=2;}
void()	player_shot3 =	[$shotatt3, player_shot4	] {self.weaponframe=3;}
void()	player_shot4 =	[$shotatt4, player_shot5	] {self.weaponframe=4;}
void()	player_shot5 =	[$shotatt5, player_shot6	] {self.weaponframe=5;}
void()	player_shot6 =	[$shotatt6, player_run	] {self.weaponframe=6;}



/*
========================================
Player Axe Thinks

this was done to decouple the player's in-world animations from the weaponframe cycle,
so that the weapon code can take over the think cycle directly and drive (normally
invisible) player animation as a secondary consideration, rather than vice versa
========================================
*/


// simplified little machine for cycling through an axe attack anim, then the
// axe run to artificially extend the anim up to 1.2 seconds
void()	player_axecyc =
{
	self.frame += 1;
	if (mod(max(0,self.frame - $axatt1), 6) == 0)	// conveniently all the same length
	{
		self.frame = $axrun1;
	}
}


void()	player_axe1 =	[$axatt1, player_axe2	] {W_AxeWhiffSound();}
void()	player_axe2 =	[$axatt2, player_axe3	] {self.weaponframe += 1;}
void()	player_axe3 =	[$axatt3, player_axe4	] {self.weaponframe += 1; W_FireAxe();}
void()	player_axe4 =	[$axatt4, player_axe5	] {self.weaponframe += 1;}
void()	player_axe5 =	[$axatt5, player_run	] {self.weaponframe += 1;}
void()	player_axe6 =	[$axatt6, player_run	] {self.weaponframe += 1;}

void()	player_axeb1 =	[$axattb1, player_axeb2	] {W_AxeWhiffSound();}
void()	player_axeb2 =	[$axattb2, player_axeb3	] {self.weaponframe += 1;}
void()	player_axeb3 =	[$axattb3, player_axeb4	] {self.weaponframe += 1; W_FireAxe();}
void()	player_axeb4 =	[$axattb4, player_axeb5	] {self.weaponframe += 1;}
void()	player_axeb5 =	[$axattb5, player_run	] {self.weaponframe += 1;}
void()	player_axeb6 =	[$axattb6, player_run	] {self.weaponframe += 1;}

void()	player_axec1 =	[$axattc1, player_axec2	] {W_AxeWhiffSound();}
void()	player_axec2 =	[$axattc2, player_axec3	] {self.weaponframe += 1;}
void()	player_axec3 =	[$axattc3, player_axec4	] {self.weaponframe += 1; W_FireAxe();}
void()	player_axec4 =	[$axattc4, player_axec5	] {self.weaponframe += 1;}
void()	player_axec5 =	[$axattc5, player_run	] {self.weaponframe += 1;}
void()	player_axec6 =	[$axattc6, player_run	] {self.weaponframe += 1;}

void()	player_axed1 =	[$axattd1, player_axed2	] {W_AxeWhiffSound();}
void()	player_axed2 =	[$axattd2, player_axed3	] {self.weaponframe += 1;}
void()	player_axed3 =	[$axattd3, player_axed4	] {self.weaponframe += 1; W_FireAxe();}
void()	player_axed4 =	[$axattd4, player_axed5	] {self.weaponframe += 1;}
void()	player_axed5 =	[$axattd5, player_run	] {self.weaponframe += 1;}
void()	player_axed6 =	[$axattd6, player_run	] {self.weaponframe += 1;}

void()	player_axee1 =	[$axattc1, player_axee2] {}
void()	player_axee2 =	[$axattb1, player_axee3] {self.weaponframe += 1;}
void()	player_axee3 =	[$axattb2, player_axee4] {
	self.weaponframe += 1;
	W_AxeWhiffSound();
}
void()	player_axee4 =	[$axattb3, player_axee5] {self.weaponframe += 1;}
void()	player_axee5 =	[$axattb4, player_axee6] {self.weaponframe += 1; W_FireAxe();}
void()	player_axee6 =	[$axattb5, player_axee7] {self.weaponframe += 1;}
void()	player_axee7 =	[$axattb6, player_run] {self.weaponframe += 1;}
void()	player_axee8 =	[$axattb6, player_run] {self.weaponframe += 1;}

void()	player_axe =
{
	float r;
	r = random();
	if (r < 0.25)
		self.frame = $axatt1;
		//player_axe1();
	else if (r<0.5)
		self.frame = $axattb1;
		//player_axeb1();
	else if (r<0.75)
		self.frame = $axattc1;
		//player_axec1();
	else
		self.frame = $axattd1;
		//player_axed1();
		
	self.think1 = player_axecyc;
}

//============================================================================

void() player_nail =
{
	if (!self.button0)
	{
		player_run();
		return;
	}
		
	self.effects |= EF_MUZZLEFLASH;

	if (self.frame != $nailatt1) self.frame = $nailatt1;
	else self.frame += 1;
	self.think = player_nail;
	self.nextthink = time + 0.125;	// TODO: oh fuck it, it works, you're never touching this again
	
	self.weaponframe = self.cnt + 1;
}

//============================================================================

void() player_light1   =[$light1, player_light2  ] 
{
	self.effects = self.effects | EF_MUZZLEFLASH;

	if (!self.button0)
		{player_run ();return;}
	self.weaponframe = self.weaponframe + 1;
	if (self.weaponframe == 5)
		self.weaponframe = 1;
	SuperDamageSound();
	W_FireLightning(0);
	self.attack_finished = time + 0.2;
}
void() player_light2   =[$light2, player_light1  ]
{
	self.effects = self.effects | EF_MUZZLEFLASH;

	if (!self.button0)
		{player_run ();return;}
	self.weaponframe = self.weaponframe + 1;
	if (self.weaponframe == 5)
		self.weaponframe = 1;
	SuperDamageSound();
	W_FireLightning(0);
	self.attack_finished = time + 0.2;
}

//============================================================================


void() player_rocket1   =[$rockatt1, player_rocket2  ] {self.weaponframe=1;
self.effects = self.effects | EF_MUZZLEFLASH;}
void() player_rocket2   =[$rockatt2, player_rocket3  ] {self.weaponframe=2;}
void() player_rocket3   =[$rockatt3, player_rocket4  ] {self.weaponframe=3;}
void() player_rocket4   =[$rockatt4, player_rocket5  ] {self.weaponframe=4;}
void() player_rocket5   =[$rockatt5, player_rocket6  ] {self.weaponframe=5;}
void() player_rocket6   =[$rockatt6, player_run  ] {self.weaponframe=6;}


void() PainSound = {
	float		rs;

	if (self.health < 0)
		return;

	if (damage_attacker.classname == "teledeath")
	{
		sound (self, CHAN_VOICE, "player/teledth1.wav", 1, ATTN_NONE);
		return;
	}

	// water pain sounds
	if (self.watertype == CONTENT_WATER && self.waterlevel >= 3 && (self.items & IT_SUIT == 0))
	{
		DeathBubbles(1);
		if (random() > 0.5)
			sound (self, CHAN_VOICE, "player/drown1.wav", 1, ATTN_NORM);
		else
			sound (self, CHAN_VOICE, "player/drown2.wav", 1, ATTN_NORM);
		return;
	}

	// slime/lava pain sounds
	if (( (self.watertype == CONTENT_SLIME || self.watertype == CONTENT_LAVA) && damage_attacker == world) || 
		damage_attacker.classname == "fireball")
	{
		if (random() > 0.5)
			sound (self, CHAN_VOICE, "player/lburn1.wav", 1, ATTN_NORM);
		else
			sound (self, CHAN_VOICE, "player/lburn2.wav", 1, ATTN_NORM);
		return;
	}

	// don't make multiple pain sounds right after each other
	if (self.pain_finished > time)
	{
		self.customflags = not(self.customflags, CFL_AXEHITME );
		return;
	}
	self.pain_finished = time + 0.5;


// ax pain sound
	if (self.customflags & CFL_AXEHITME)
	{
		self.customflags = not(self.customflags, CFL_AXEHITME );
		sound (self, CHAN_VOICE, "player/axhit1.wav", 1, ATTN_NORM);
		return;
	}
	

	rs = rint((random() * 5) + 1);

	self.noise = string_null;
	if (rs == 1)
		self.noise = "player/pain1.wav";
	else if (rs == 2)
		self.noise = "player/pain2.wav";
	else if (rs == 3)
		self.noise = "player/pain3.wav";
	else if (rs == 4)
		self.noise = "player/pain4.wav";
	else if (rs == 5)
		self.noise = "player/pain5.wav";
	else
		self.noise = "player/pain6.wav";

	sound (self, CHAN_VOICE, self.noise, 1, ATTN_NORM);
	return;
}

void()	player_pain1 =	[	$pain1,	player_pain2	] {PainSound();self.weaponframe=0;}
void()	player_pain2 =	[	$pain2,	player_pain3	] {}
void()	player_pain3 =	[	$pain3,	player_pain4	] {}
void()	player_pain4 =	[	$pain4,	player_pain5	] {}
void()	player_pain5 =	[	$pain5,	player_pain6	] {}
void()	player_pain6 =	[	$pain6,	player_run	] {}

void()	player_axpain1 =	[	$axpain1,	player_axpain2	] {PainSound();self.weaponframe=0;}
void()	player_axpain2 =	[	$axpain2,	player_axpain3	] {}
void()	player_axpain3 =	[	$axpain3,	player_axpain4	] {}
void()	player_axpain4 =	[	$axpain4,	player_axpain5	] {}
void()	player_axpain5 =	[	$axpain5,	player_axpain6	] {}
void()	player_axpain6 =	[	$axpain6,	player_run	] {}

void(entity attacker, float damage) player_pain =
{
	if (self.weaponframe)
		return;

	if (self.invisible_finished > time)
		return;		// eyes don't have pain frames

	if (self.weapon == IT_AXE )
		player_axpain1 ();
	else
		player_pain1 ();
}

void() DeathBubblesSpawn =
{
	entity	bubble;
	if (self.owner.waterlevel < 3)
		return;
	bubble = bubble_spawn(self.owner.origin + '0 0 24');

	self.nextthink = time + 0.1;
	self.think = DeathBubblesSpawn;
	self.air_finished += 1;
	if (self.air_finished >= self.bubble_count)
		remove(self);
}

void(float num_bubbles) DeathBubbles =
{
local entity	bubble_spawner;
	
	bubble_spawner = spawn();
	setorigin (bubble_spawner, self.origin);
	bubble_spawner.movetype = MOVETYPE_NONE;
	bubble_spawner.solid = SOLID_NOT;
	bubble_spawner.nextthink = time + 0.1;
	bubble_spawner.think = DeathBubblesSpawn;
	bubble_spawner.air_finished = 0;
	bubble_spawner.owner = self;
	bubble_spawner.bubble_count = num_bubbles;
	return;
}


void() DeathSound =
{
local float		rs;

	// water death sounds
	if (self.waterlevel >= 3 && (self.items & IT_SUIT == 0))
	{
		if (self.watertype == CONTENT_WATER || self.watertype == CONTENT_SLIME)
		{
			DeathBubbles(20);
			sound (self, CHAN_VOICE, "player/h2odeath.wav", 1, ATTN_NONE);
			return;
		}
	}
	
	rs = rint ((random() * 4) + 1);
	if (rs == 1)
		self.noise = "player/death1.wav";
	if (rs == 2)
		self.noise = "player/death2.wav";
	if (rs == 3)
		self.noise = "player/death3.wav";
	if (rs == 4)
		self.noise = "player/death4.wav";
	if (rs == 5)
		self.noise = "player/death5.wav";

	sound (self, CHAN_VOICE, self.noise, 1, ATTN_NONE);
	return;
}


void() PlayerDead =
{
	self.nextthink = -1;
// allow respawn after a certain time
	self.deadflag = DEAD_DEAD;
}

void() GibPlayer =
{

	self.deadflag = DEAD_DEAD;

	if (damage_attacker.classname == "trigger_void" || damage_attacker.classname == "func_void" )
	{
		sound (self, CHAN_VOICE, "player/splat1.wav", 1, ATTN_NONE);
		stuffcmd( self, "\nv_cshift 16 0 0 255\n");
		return;
	}
	
	if (damage_attacker.classname == "teledeath")
	{
		sound (self, CHAN_VOICE, "player/teledth1.wav", 1, ATTN_NONE);
		return;
	}

	if (damage_attacker.classname == "teledeath2")
	{
		sound (self, CHAN_VOICE, "player/teledth1.wav", 1, ATTN_NONE);
		return;
	}
	
	if (random() < 0.5)
		sound (self, CHAN_VOICE, "player/gib.wav", 1, ATTN_NONE);
	else
		sound (self, CHAN_VOICE, "player/udeath.wav", 1, ATTN_NONE);
		
	GibSpray ("progs/h_player.mdl", self.health);	
}

void() PlayerDie =
{
	local	float	i;
	
	self.items = self.items - (self.items & IT_INVISIBILITY);
	self.invisible_finished = 0;	// don't die as eyes
	self.invincible_finished = 0;
	self.super_damage_finished = 0;
	self.radsuit_finished = 0;
	self.modelindex = modelindex_player;	// don't use eyes

	self.items = not(self.items, IT_INVISIBILITY | IT_INVULNERABILITY | IT_SUIT | IT_QUAD);
	self.effects = 0;

	ClientObituary();	// moved from Killed() so it doesn't get called when non-players die
	
	if (deathmatch || coop)
		DropBackpack();
	
	self.weaponmodel = string_null;
	self.view_ofs = '0 0 0';
//	self.deadflag = DEAD_DYING;	// moved to Killed() so I can use this flag on monsters
	self.solid = SOLID_NOT;
	self.flags = self.flags - (self.flags & FL_ONGROUND);
	self.movetype = MOVETYPE_TOSS;
	setsize(self, '-16 -16 -16', '16 16 8');
	if (self.velocity_z < 10)
		self.velocity_z = self.velocity_z + random()*300;

	if (self.health < -40)
	{
		GibPlayer ();
		return;
	}

	DeathSound();
	
	self.angles_x = 0;
	self.angles_z = 0;
	
	if (self.weapon == IT_AXE )
	{
		player_die_ax1 ();
		return;
	}
	
	i = 1 + floor(random()*6);
	
	if (i == 1)
		player_diea1();
	else if (i == 2)
		player_dieb1();
	else if (i == 3)
		player_diec1();
	else if (i == 4)
		player_died1();
	else
		player_diee1();

}

void() set_suicide_frame =
{	// used by kill command and disconnect command
	if (self.model != "progs/player.mdl")
		return;	// already gibbed
	self.frame = $deatha11;
	self.solid = SOLID_NOT;
	self.movetype = MOVETYPE_TOSS;
	self.deadflag = DEAD_DEAD;
	self.nextthink = -1;
}


void()	player_diea1	=	[	$deatha1,	player_diea2	] {}
void()	player_diea2	=	[	$deatha2,	player_diea3	] {}
void()	player_diea3	=	[	$deatha3,	player_diea4	] {}
void()	player_diea4	=	[	$deatha4,	player_diea5	] {}
void()	player_diea5	=	[	$deatha5,	player_diea6	] {}
void()	player_diea6	=	[	$deatha6,	player_diea7	] {}
void()	player_diea7	=	[	$deatha7,	player_diea8	] {}
void()	player_diea8	=	[	$deatha8,	player_diea9	] {}
void()	player_diea9	=	[	$deatha9,	player_diea10	] {}
void()	player_diea10	=	[	$deatha10,	player_diea11	] {}
void()	player_diea11	=	[	$deatha11,	player_diea11 ] {PlayerDead();}

void()	player_dieb1	=	[	$deathb1,	player_dieb2	] {}
void()	player_dieb2	=	[	$deathb2,	player_dieb3	] {}
void()	player_dieb3	=	[	$deathb3,	player_dieb4	] {}
void()	player_dieb4	=	[	$deathb4,	player_dieb5	] {}
void()	player_dieb5	=	[	$deathb5,	player_dieb6	] {}
void()	player_dieb6	=	[	$deathb6,	player_dieb7	] {}
void()	player_dieb7	=	[	$deathb7,	player_dieb8	] {}
void()	player_dieb8	=	[	$deathb8,	player_dieb9	] {}
void()	player_dieb9	=	[	$deathb9,	player_dieb9	] {PlayerDead();}

void()	player_diec1	=	[	$deathc1,	player_diec2	] {}
void()	player_diec2	=	[	$deathc2,	player_diec3	] {}
void()	player_diec3	=	[	$deathc3,	player_diec4	] {}
void()	player_diec4	=	[	$deathc4,	player_diec5	] {}
void()	player_diec5	=	[	$deathc5,	player_diec6	] {}
void()	player_diec6	=	[	$deathc6,	player_diec7	] {}
void()	player_diec7	=	[	$deathc7,	player_diec8	] {}
void()	player_diec8	=	[	$deathc8,	player_diec9	] {}
void()	player_diec9	=	[	$deathc9,	player_diec10	] {}
void()	player_diec10	=	[	$deathc10,	player_diec11	] {}
void()	player_diec11	=	[	$deathc11,	player_diec12	] {}
void()	player_diec12	=	[	$deathc12,	player_diec13	] {}
void()	player_diec13	=	[	$deathc13,	player_diec14	] {}
void()	player_diec14	=	[	$deathc14,	player_diec15	] {}
void()	player_diec15	=	[	$deathc15,	player_diec15 ] {PlayerDead();}

void()	player_died1	=	[	$deathd1,	player_died2	] {}
void()	player_died2	=	[	$deathd2,	player_died3	] {}
void()	player_died3	=	[	$deathd3,	player_died4	] {}
void()	player_died4	=	[	$deathd4,	player_died5	] {}
void()	player_died5	=	[	$deathd5,	player_died6	] {}
void()	player_died6	=	[	$deathd6,	player_died7	] {}
void()	player_died7	=	[	$deathd7,	player_died8	] {}
void()	player_died8	=	[	$deathd8,	player_died9	] {}
void()	player_died9	=	[	$deathd9,	player_died9	] {PlayerDead();}

void()	player_diee1	=	[	$deathe1,	player_diee2	] {}
void()	player_diee2	=	[	$deathe2,	player_diee3	] {}
void()	player_diee3	=	[	$deathe3,	player_diee4	] {}
void()	player_diee4	=	[	$deathe4,	player_diee5	] {}
void()	player_diee5	=	[	$deathe5,	player_diee6	] {}
void()	player_diee6	=	[	$deathe6,	player_diee7	] {}
void()	player_diee7	=	[	$deathe7,	player_diee8	] {}
void()	player_diee8	=	[	$deathe8,	player_diee9	] {}
void()	player_diee9	=	[	$deathe9,	player_diee9	] {PlayerDead();}

void()	player_die_ax1	=	[	$axdeth1,	player_die_ax2	] {}
void()	player_die_ax2	=	[	$axdeth2,	player_die_ax3	] {}
void()	player_die_ax3	=	[	$axdeth3,	player_die_ax4	] {}
void()	player_die_ax4	=	[	$axdeth4,	player_die_ax5	] {}
void()	player_die_ax5	=	[	$axdeth5,	player_die_ax6	] {}
void()	player_die_ax6	=	[	$axdeth6,	player_die_ax7	] {}
void()	player_die_ax7	=	[	$axdeth7,	player_die_ax8	] {}
void()	player_die_ax8	=	[	$axdeth8,	player_die_ax9	] {}
void()	player_die_ax9	=	[	$axdeth9,	player_die_ax9	] {PlayerDead();}
