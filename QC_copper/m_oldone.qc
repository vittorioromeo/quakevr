/*
==============================================================================

OLD ONE

==============================================================================
*/
$cd id1/models/old_one
$origin 0 0 24
$base base
$skin skin
$scale 1

$frame old1 old2 old3 old4 old5 old6 old7 old8 old9 
$frame old10 old11 old12 old13 old14 old15 old16 old17 old18 old19 
$frame old20 old21 old22 old23 old24 old25 old26 old27 old28 old29 
$frame old30 old31 old32 old33 old34 old35 old36 old37 old38 old39 
$frame old40 old41 old42 old43 old44 old45 old46 

$frame shake1 shake2 shake3 shake4 shake5 shake6 shake7 shake8
$frame shake9 shake10 shake11 shake12 shake13 shake14
$frame shake15 shake16 shake17 shake18 shake19 shake20

void() old_idle1	=[	$old1,  old_idle2	] {}
void() old_idle2	=[	$old2,  old_idle3	] {}
void() old_idle3	=[	$old3,  old_idle4	] {}
void() old_idle4	=[	$old4,  old_idle5	] {}
void() old_idle5	=[	$old5,  old_idle6	] {}
void() old_idle6	=[	$old6,  old_idle7	] {}
void() old_idle7	=[	$old7,  old_idle8	] {}
void() old_idle8	=[	$old8,  old_idle9	] {}
void() old_idle9	=[	$old9,  old_idle10	] {}
void() old_idle10	=[	$old10, old_idle11	] {}
void() old_idle11	=[	$old11, old_idle12	] {}
void() old_idle12	=[	$old12, old_idle13	] {}
void() old_idle13	=[	$old13, old_idle14	] {}
void() old_idle14	=[	$old14, old_idle15	] {}
void() old_idle15	=[	$old15, old_idle16	] {}
void() old_idle16	=[	$old16, old_idle17	] {}
void() old_idle17	=[	$old17, old_idle18	] {}
void() old_idle18	=[	$old18, old_idle19	] {}
void() old_idle19	=[	$old19, old_idle20	] {}
void() old_idle20	=[	$old20, old_idle21	] {}
void() old_idle21	=[	$old21, old_idle22	] {}
void() old_idle22	=[	$old22, old_idle23	] {}
void() old_idle23	=[	$old23, old_idle24	] {}
void() old_idle24	=[	$old24, old_idle25	] {}
void() old_idle25	=[	$old25, old_idle26	] {}
void() old_idle26	=[	$old26, old_idle27	] {}
void() old_idle27	=[	$old27, old_idle28	] {}
void() old_idle28	=[	$old28, old_idle29	] {}
void() old_idle29	=[	$old29, old_idle30	] {}
void() old_idle30	=[	$old30, old_idle31	] {}
void() old_idle31	=[	$old31, old_idle32	] {}
void() old_idle32	=[	$old32, old_idle33	] {}
void() old_idle33	=[	$old33, old_idle34	] {}
void() old_idle34	=[	$old34, old_idle35	] {}
void() old_idle35	=[	$old35, old_idle36	] {}
void() old_idle36	=[	$old36, old_idle37	] {}
void() old_idle37	=[	$old37, old_idle38	] {}
void() old_idle38	=[	$old38, old_idle39	] {}
void() old_idle39	=[	$old39, old_idle40	] {}
void() old_idle40	=[	$old40, old_idle41	] {}
void() old_idle41	=[	$old41, old_idle42	] {}
void() old_idle42	=[	$old42, old_idle43	] {}
void() old_idle43	=[	$old43, old_idle44	] {}
void() old_idle44	=[	$old44, old_idle45	] {}
void() old_idle45	=[	$old45, old_idle46	] {}
void() old_idle46	=[	$old46, old_idle1	] {}

void() old_thrash1	=[ $shake1,  old_thrash2	] {lightstyle(0, "m");
	sound (self, CHAN_VOICE, "boss2/death.wav", 1, ATTN_NONE);
	screenshake_oneshot(0.2, 0.8, 0.8, 1.5);
	//self.use = SUB_Null;
}
void() old_thrash2	=[ $shake2,  old_thrash3	] {lightstyle(0, "k");}
void() old_thrash3	=[ $shake3,  old_thrash4	] {lightstyle(0, "j");}
void() old_thrash4	=[ $shake4,  old_thrash5	] {lightstyle(0, "h");}
void() old_thrash5	=[ $shake5,  old_thrash6	] {lightstyle(0, "g");}
void() old_thrash6	=[ $shake6,  old_thrash7	] {lightstyle(0, "e");}
void() old_thrash7	=[ $shake7,  old_thrash8	] {lightstyle(0, "c");}
void() old_thrash8	=[ $shake8,  old_thrash9	] {lightstyle(0, "a");}
void() old_thrash9	=[ $shake9,  old_thrash10	] {lightstyle(0, "b");}
void() old_thrash10	=[ $shake10, old_thrash11	] {lightstyle(0, "e");}
void() old_thrash11	=[ $shake11, old_thrash12	] {lightstyle(0, "f");}
void() old_thrash12	=[ $shake12, old_thrash13	] {lightstyle(0, "g");}
void() old_thrash13	=[ $shake13, old_thrash14	] {lightstyle(0, "i");}
void() old_thrash14	=[ $shake14, old_thrash15	] {lightstyle(0, "j");}
void() old_thrash15	=[ $shake15, old_thrash16	] {lightstyle(0, "h");}
void() old_thrash16	=[	$shake16, old_thrash17	] {lightstyle(0, "e");}
void() old_thrash17	=[	$shake17, old_thrash18	] {lightstyle(0, "g");}
void() old_thrash18	=[	$shake18, old_thrash19	] {lightstyle(0, "j");
	// the death gurgle is the same sound looped, so stop it by overriding that channel with nothing
	sound (self, CHAN_VOICE, "misc/null.wav", 1, ATTN_NORM);
	
	if (world.model == "maps/end.bsp" && self.health <= 0)
	{
		if (self.strength)
		{
			self.strength -= 1;
			self.think = old_thrash1;
		}
		return;
	}
	
	if (self.strength)
	{
		lightstyle(0, "m");
		self.think = old_idle1;
		//self.use = shub_use;
		return;
	}
}
void() old_thrash19	=[	$shake19, old_thrash20	] {lightstyle(0, "k");}
void() old_thrash20	=[	$shake20, shub_finale_4	] {lightstyle(0, "m");}

//============================================================================

// make fake versions of all players as standins, and move the real
// players to the intermission spot
void() shub_set_viewpoints =
{
	local entity	pos, pl;

	intermission_exittime = time + A_SHITLOAD;	// no exit yet
	intermission_running = 1;

	// find the intermission spot
	pos = find (world, classname, "info_intermission");
	if (!pos)
		pos = find (world, classname, "info_player_start");
	if (!pos)
		error ("no info_intermission");
	self.buddy = pos;
	pl = find (world, classname, "misc_teleporttrain");
	if (pl) remove (pl);

	WriteByte (MSG_ALL, SVC_FINALE);
	WriteString (MSG_ALL, string_null);

	move_players_to_intermission();
}

void() shub_dudecycle =
{
	self.frame += 1;
	if (skill > 2)
	{
		if (self.frame > 28) self.frame = 17;
	}
	else
	{
		if (self.frame > 16) self.frame = 12; 
	}
	self.nextthink = time + 0.1;
}

void() shub_finale_1 =
{
	local entity	timer;

	shub_set_viewpoints();
	
	// wait for 1 second
	timer = spawn();
	timer.nextthink = time + 1;
	timer.think = shub_finale_2;
	timer.enemy = self;
}

void() shub_finale_2 =
{
	local vector	o;

	// start a teleport splash inside shub

	o = self.enemy.origin - '0 100 0';
	WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
	WriteByte (MSG_BROADCAST, TE_TELEPORT);
	WriteCoord (MSG_BROADCAST, o_x);
	WriteCoord (MSG_BROADCAST, o_y);
	WriteCoord (MSG_BROADCAST, o_z);

	sound (self.enemy, CHAN_VOICE, "misc/r_tele1.wav", 1, ATTN_NORM);
	
	self.nextthink = time + 2;
	self.think = shub_finale_3;
}

void() shub_finale_3 =
{
	// start shub thrashing wildly
	self.enemy.strength = 2;
	self.enemy.think = old_thrash1;
}

void() shub_finale_4 =
{
	vector	org;
	entity	n;
	float	x, y, z;
	float	r;

	sound (n, CHAN_VOICE, "boss2/pop2.wav", 1, ATTN_NONE);

	// throw tons of meat chunks	
	org = self.origin;
	
	for (z = 16; z <= 144; z += 96)
	{
		for (x = -64; x <= 64; x += 32)
		{
			for (y = -64; y <= 64; y += 32)
			{
				self.origin_x = org_x + x;
				self.origin_y = org_y + y;
				self.origin_z = org_z + z;

				r = random();
				if (r < 0.3)				
					ThrowGib ("progs/gib1.mdl", -300);
				else if (r < 0.6)
					ThrowGib ("progs/gib2.mdl", -300);
				else
					ThrowGib ("progs/gib3.mdl", -300);
			}
		}
	}
	
	lightstyle(0, "m");
	self.origin = org;
	killed_monster();
	activator = self.enemy;
	monster_death_use();
	
	if (world.model == "maps/end.bsp")
	{
		// do this if we triggered shub to death
		if (self.health >= 0)
			shub_set_viewpoints();
			
		// put a player model down
		n = spawn();
		setmodel (n, "progs/player.mdl");
		//org = self.origin;
		org = (org + self.buddy.origin) * 0.5;
		org_z = self.origin_z;
		setorigin (n, org);
		if (skill > 2)	// axe on nightmare because you are very badass
		{
			n.angles = '0 235 0';
			n.frame = 17 + ceil(random() * 10);
		}
		else
		{
			n.angles = '0 295 0';
			n.frame = 12 + ceil(random() * 2);
		}
		n.think = shub_dudecycle;
		n.nextthink = time + 0.1;

		// kill everything else
		shub_gibnearby();
		Cheat_Genocide();
		
		// allow the player to click back to a reset start map
		nextmap = self.map;
		intermission_exittime = time + 3;
		intermission_running = 2;
		serverflags = 0;	// clear all the runes and stuff
		
		// start the end text
		WriteByte (MSG_ALL, SVC_FINALE);
		WriteString (MSG_ALL, "Congratulations, and well done! You have\nbeaten the hideous Shub-Niggurath and\nher hundreds of ugly changelings and\nmonsters. You have proven that your\nskill and your cunning are greater than\nall the powers of Quake. You are the\nmaster now.\n\nId Software is currently out of the\noffice, but if you leave a message,\nthey will be happy to salute you in\na couple of months.");

	// switch cd track
		WriteByte (MSG_ALL, SVC_CDTRACK);
		WriteByte (MSG_ALL, 2);
		WriteByte (MSG_ALL, 2);
	}
	
	remove (self);
}


void() shub_axefinale_1 =
{
	shub_set_viewpoints();
	shub_finale_4();
}

//============================================================================

// clean up environs for the dramatic final shot - gib monsters in the way so the 
// body pile isn't too deep, and just get rid of backpacks
void() shub_gibnearby =
{
	entity head;
	
	head = findradius(self.origin, 272);	
	while (head)
	{
		if (head.flags & FL_MONSTER)
		{
			T_Damage(head, self, self, head.health + 100);
		}
		head = head.chain;
	}
	
	head = findradiusplus(self.origin, 272, classname, "item_backpack");
	while (head)
	{
		remove(head);
		head = head.chain;
	}	
}

void() shub_die =
{
	self.deadflag = DEAD_DYING;
	if (world.model == "maps/end.bsp")
		shub_finale_1();
	else
		shub_finale_4();
}

// rather than a telefrag, shub can be killed by triggering her a
// specifiable number of times, to allow for more possible encounters
void() shub_use =
{
	self.strength -= 1;
	self.enemy = activator;
	old_thrash1();
}

void (entity attacker, float damage) shub_pain =
{
	if (self.deadflag) return;

	// a separate 'pain counter' is reduced by
	// any damage over 10000
	if (damage >= 10000)
	{
		shub_use();
	}
	if (self.health > 0)
		self.health = 49999;	// stay dead
}

//============================================================================


/*QUAKED monster_oldone (1 0 0) (-128 -128 -24) (128 128 256)
shubby

explodes in gibs on death. can be killed by traditional telefrag or in several new ways:
- being triggered "strength" times
- being damaged for more than 10000 hp "strength" times
both of the above cause one thrashy roary anim that flickers the lights and causes a screenshake.

if the map is end.bsp, the finale cutscene will trigger, otherwise shub will just die like any other monster.

Keyvalues:
"strength" number of hits to take before exploding, defaults to 3
*/
/*FGD
@PointClass base(Appearflags, Target, Targetname, Angle) size(-128 -128 -24, 128 128 256) model({ "path": ":progs/oldone.mdl" }) = monster_oldone : 
"Shubby
can be killed by traditional telefrag or in several new ways:
- being triggered 'strength' times
- being damaged for more than 10000 hp 'strength' times
both of the above cause one thrashy roary anim that flickers the lights and causes a screenshake.
if the map is end.bsp, the finale cutscene will trigger, otherwise shub will just die like any other monster."
[
	strength(integer) : "Health, in hits" : 3
]
*/
void() monster_oldone =
{
	if (!SUB_ShouldSpawn()) return;
	if (deathmatch || cvar("nomonsters") != 0)
	{
		remove(self);
		return;
	}

	precache_model2 ("progs/oldone.mdl");

	precache_sound2 ("boss2/death.wav");
	precache_sound2 ("boss2/idle.wav");
	precache_sound2 ("boss2/sight.wav");
	precache_sound2 ("boss2/pop2.wav");
	precache_sound ("misc/null.wav");

	self.solid = SOLID_SLIDEBOX;
	self.movetype = MOVETYPE_STEP;
	self.type = "boss";
	
	setmodel (self, "progs/oldone.mdl");
	setsize (self, '-128 -128 -24', '128 128 256');

	if (!self.strength)
		self.strength = 3;
	if (!self.map)
		self.map = "start";
	if (self.deathtype == string_null)
		self.deathtype = "became one with Shub-Niggurath";

	self.health = 49999;		// kill by telefrag
	self.think = old_idle1;
	self.nextthink = time + 0.1;	
	self.takedamage = DAMAGE_YES;
	self.th_pain = shub_pain;
	self.th_die = shub_die;
	self.use = shub_use;
	
	total_monsters = total_monsters + 1;
}

