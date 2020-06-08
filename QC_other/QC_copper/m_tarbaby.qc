/*
==============================================================================

BLOB

==============================================================================
*/

$cd id1/models/tarbaby
$origin 0 0 24
$base base		

$skin skin

$frame walk1 walk2 walk3 walk4  walk5 walk6 walk7 walk8 walk9 walk10
$frame walk11 walk12 walk13 walk14 walk15 walk16 walk17 walk18 walk19
$frame walk20 walk21 walk22 walk23 walk24 walk25

$frame run1 run2 run3 run4 run5 run6  run7 run8 run9 run10 run11 run12 run13
$frame run14 run15 run16 run17 run18 run19 run20 run21 run22 run23
$frame run24 run25

$frame jump1 jump2 jump3 jump4 jump5 jump6

$frame fly1 fly2 fly3 fly4

$frame exp

void() tbaby_idle =
{
	if (random() < 0.4) 
		sound (self, CHAN_BODY, "blob/idle.wav", 1, ATTN_IDLE);
}

void() tbaby_stand1	=[	$walk1,		tbaby_stand1] {ai_stand();}

void() tbaby_hang1	=[	$walk1,		tbaby_hang1	] {ai_stand();}

void() tbaby_walk1	=[	$walk1,		tbaby_walk2	] {ai_turn();}
void() tbaby_walk2	=[	$walk2,		tbaby_walk3	] {ai_turn();}
void() tbaby_walk3	=[	$walk3,		tbaby_walk4	] {ai_turn();}
void() tbaby_walk4	=[	$walk4,		tbaby_walk5	] {ai_turn();}
void() tbaby_walk5	=[	$walk5,		tbaby_walk6	] {ai_turn();}
void() tbaby_walk6	=[	$walk6,		tbaby_walk7	] {ai_walk(1);}
void() tbaby_walk7	=[	$walk7,		tbaby_walk8	] {ai_walk(1);}
void() tbaby_walk8	=[	$walk8,		tbaby_walk9	] {ai_walk(1.5);}
void() tbaby_walk9	=[	$walk9,		tbaby_walk10] {ai_walk(2.5);}
void() tbaby_walk10	=[	$walk10,	tbaby_walk11] {ai_walk(2);}
void() tbaby_walk11	=[	$walk11,	tbaby_walk12] {ai_walk(4);}
void() tbaby_walk12	=[	$walk12,	tbaby_walk13] {ai_walk(6);}
void() tbaby_walk13	=[	$walk13,	tbaby_walk14] {ai_walk(7);}
void() tbaby_walk14	=[	$walk14,	tbaby_walk15] {ai_walk(6);tbaby_idle();}
void() tbaby_walk15	=[	$walk15,	tbaby_walk16] {ai_walk(5);}
void() tbaby_walk16	=[	$walk16,	tbaby_walk17] {ai_turn();}
void() tbaby_walk17	=[	$walk17,	tbaby_walk18] {ai_turn();}
void() tbaby_walk18	=[	$walk18,	tbaby_walk19] {ai_turn();}
void() tbaby_walk19	=[	$walk19,	tbaby_walk20] {ai_turn();}
void() tbaby_walk20	=[	$walk20,	tbaby_walk21] {ai_turn();}
void() tbaby_walk21	=[	$walk21,	tbaby_walk22] {ai_turn();}
void() tbaby_walk22	=[	$walk22,	tbaby_walk23] {ai_turn();}
void() tbaby_walk23	=[	$walk23,	tbaby_walk24] {ai_walk(3);}
void() tbaby_walk24	=[	$walk24,	tbaby_walk25] {ai_walk(1);}
void() tbaby_walk25	=[	$walk25,	tbaby_walk1	] {ai_walk(1);}

void() tbaby_run1	=[	$run1,		tbaby_run2	] {
	ai_face();
	Tar_Forget();
	self.cnt = 0;
}
void() tbaby_run2	=[	$run2,		tbaby_run3	] {ai_face();}
void() tbaby_run3	=[	$run3,		tbaby_run4	] {ai_face();}
void() tbaby_run4	=[	$run4,		tbaby_run5	] {ai_face();}
void() tbaby_run5	=[	$run5,		tbaby_run6	] {ai_face();}
void() tbaby_run6	=[	$run6,		tbaby_run7	] {ai_face();}
void() tbaby_run7	=[	$run7,		tbaby_run8	] {ai_run(1);}
void() tbaby_run8	=[	$run8,		tbaby_run9	] {ai_run(1);}
void() tbaby_run9	=[	$run9,		tbaby_run10	] {ai_run(1);}
void() tbaby_run10	=[	$run10,		tbaby_run11	] {ai_run(2);}
void() tbaby_run11	=[	$run11,		tbaby_run12	] {ai_run(2);}
void() tbaby_run12	=[	$run12,		tbaby_run13	] {ai_run(3);}
void() tbaby_run13	=[	$run13,		tbaby_run14	] {ai_run(3);}
void() tbaby_run14	=[	$run14,		tbaby_run15	] {ai_run(1);}
void() tbaby_run15	=[	$run15,		tbaby_run16	] {ai_face();tbaby_idle();}
void() tbaby_run16	=[	$run16,		tbaby_run17	] {ai_face();}
void() tbaby_run17	=[	$run17,		tbaby_run18	] {ai_run(2);}
void() tbaby_run18	=[	$run18,		tbaby_run19	] {ai_run(4);}
void() tbaby_run19	=[	$run19,		tbaby_run20	] {ai_run(5);}
void() tbaby_run20	=[	$run20,		tbaby_run21	] {ai_run(1);}
void() tbaby_run21	=[	$run21,		tbaby_run22	] {ai_run(4);}
void() tbaby_run22	=[	$run22,		tbaby_run23	] {ai_run(2);}
void() tbaby_run23	=[	$run23,		tbaby_run24	] {ai_run(2);}
void() tbaby_run24	=[	$run24,		tbaby_run25	] {ai_run(1);}
void() tbaby_run25	=[	$run25,		tbaby_run1	] {ai_run(1);}


//============================================================================

float(entity e) tar_visible =
{
	return (visible(e) && e.invisible_finished < time);
}

void() Tar_Forget =
{
	if (!self.rad_time)	// time to be RAD
	{
		// we just woke up, don't forget right away, because if we were awoken
		// by another monster waking we'll just lay here and burp (ikspq2)
		self.rad_time = time + 60;
	}
	
	if (time < self.rad_time)
		return;
		
	if (!tar_visible(self.enemy))
	{
		if (tar_visible(self.oldenemy))
		{
			self.enemy = self.oldenemy;
		}
		else
		{
			self.enemy = world;
		}
		self.oldenemy = world;
	}
	if (!self.enemy)
	{
		self.rad_time = 0;	// reset the clock
		ai_stand();
	}
}


void()	Tar_JumpTouchGround =
{
	if (!checkbottom(self))
	{
		if (self.flags & FL_ONGROUND)
		{	// jump randomly to not get hung up
			self.think = tbaby_jump1;
			self.nextthink = time + 0.1;
		}
		return;	// not on ground yet
	}

	self.touch = SUB_Null;
	if (self.cnt > 4 && (!tar_visible(self.enemy)))
	{
		self.think = tbaby_run1;
		self.cnt = 0;
		// if the player listens to sound cues and ambushes the spawn just as it 
		// stops, he has a full second to attack it
		ai_attack_finished(1);	
	}
	else
	{
		self.think = tbaby_jump1;
	}
	self.nextthink = time + 0.1;
}

void()	Tar_JumpTouch =
{
	local	float	ldmg;

	if (other.takedamage &&
		other.classname != self.classname)
	{
		if ( vlen(self.velocity) > 400 )
		{
			if (other.classname != "player" || time > self.touch_time)	// fix for landing on the player's head
			{
				if (other.classname == "player")
				{
					self.touch_time = time + 0.2;
					self.cnt = max(0, self.cnt - 1);
				}
				
				self.rad_time = time + 60;
				ldmg = 10 + 10*random();
				T_Damage (other, self, self, ldmg);	
				sound (self, CHAN_WEAPON, "blob/hit1.wav", 1, ATTN_NORM);
			}
		}
	}
	else
		sound (self, CHAN_WEAPON, "blob/land1.wav", 1, ATTN_NORM);
	
	Tar_JumpTouchGround();
}

void()	tbaby_fly1		=[	$fly1,	tbaby_fly2	] {}
void()	tbaby_fly2		=[	$fly2,	tbaby_fly3	] {}
void()	tbaby_fly3		=[	$fly3,	tbaby_fly4	] {}
void()	tbaby_fly4		=[	$fly4,	tbaby_fly1	] 
{
	self.delay = self.delay + 1;
	if (self.delay == 4)
	{
		//dprint ("spawn hop\n");
		tbaby_jump1 ();
	}
}

void()	tbaby_jump1		=[	$jump1,	tbaby_jump2		] {ai_face();}
void()	tbaby_jump2		=[	$jump2,	tbaby_jump3		] {ai_face();}
void()	tbaby_jump3		=[	$jump3,	tbaby_jump4		] {ai_face();}
void()	tbaby_jump4		=[	$jump4,	tbaby_jump5		] {ai_face();}
void()	tbaby_jump5		=[	$jump5,	tbaby_jump6		]
{	
	self.movetype = MOVETYPE_BOUNCE;
	self.touch = Tar_JumpTouch;
	makevectors (self.angles);
	self.origin_z = self.origin_z + 1;
	self.velocity = v_forward * 600 + '0 0 200';
	self.velocity_z = self.velocity_z + random()*150;
	if (self.flags & FL_ONGROUND)
		self.flags = self.flags - FL_ONGROUND;
		
	if (!tar_visible(self.enemy))
		self.cnt = self.cnt + 1;
		
	self.delay = 0;
	self.rad_time = time + 60;
}
void()	tbaby_jump6	=[	$jump6,tbaby_fly1	] {}



//=============================================================================

void(entity attacker, float damage) tbaby_pain =
{
	self.rad_time = time + 60;
	
	// shooting a tbaby while invisible triggers one spastic leap
	if (attacker.invisible_finished > time && self.pain_finished < time)
	{
		self.pain_finished = time + 1;
		self.cnt = 4;
		tbaby_jump1();
		return;
	}
	
	self.cnt = 0;	
}

void()	tbaby_die1	=[	$exp,		tbaby_die2	] {
self.takedamage = DAMAGE_NO;
}
void()	tbaby_die2	=[	$exp,		tbaby_run1	] 
{
	T_RadiusDamage (self, self, 120, world);

	sound (self, CHAN_VOICE, "blob/death1.wav", 1, ATTN_NORM);
	self.origin = self.origin - 8*normalize(self.velocity);

	WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
	WriteByte (MSG_BROADCAST, TE_TAREXPLOSION);
	WriteCoord (MSG_BROADCAST, self.origin_x);
	WriteCoord (MSG_BROADCAST, self.origin_y);
	WriteCoord (MSG_BROADCAST, self.origin_z);
	
	BecomeBlast ();
}

//=============================================================================

float() tbaby_check_attack =
{
	float chance;
	
	// spawns don't launch an attack if the player is invisible, or else
	// they'd time out after five leaps and then just start again
	if (self.enemy.invisible_finished > time) return FALSE;
	
	if (time < self.attack_finished) return FALSE;
	if (enemy_range >= RANGE_FAR) return FALSE;
	if (!CheckClearAttack()) return FALSE;
		
	if (enemy_range == RANGE_MELEE)
	{
		chance = 0.9;
		self.attack_finished = 0;
	}
	else if (enemy_range == RANGE_NEAR)
		chance = 0.4;
	else if (enemy_range == RANGE_MID)
		chance = 0.1;
	else
		chance = 0;

	if (random() < chance)
	{
		self.th_missile ();
		ai_attack_finished (2*random());
		return TRUE;
	}

	return FALSE;
}

void() monster_tarbaby_spawn =
{
	self.classname = "monster_tarbaby";
	
	self.solid = SOLID_SLIDEBOX;
	self.movetype = MOVETYPE_STEP;

	setmodel (self, "progs/tarbaby.mdl");

	setsize (self, '-16 -16 -24', '16 16 40');
	self.health = 80;
	self.yaw_speed = 30;

	self.customflags |= CFL_MELEEONLY;

	self.th_stand = tbaby_stand1;
	self.th_walk = tbaby_walk1;
	self.th_run = tbaby_run1;
	self.th_missile = tbaby_jump1;
	self.th_melee = tbaby_jump1;
	self.th_die = tbaby_die1;
	self.th_checkattack = tbaby_check_attack;
	self.th_pain = tbaby_pain;

	if (self.deathtype == string_null)
		self.deathtype = "was slimed by a Spawn";

	walkmonster_start ();
}

void() monster_tarbaby_spawner = {mon_spawner_use(monster_tarbaby_spawn);}

/*QUAKED monster_tarbaby (1 0 0) (-16 -16 -24) (16 16 24) Ambush ? ? ? Triggered NoTfog NoTelefrag Instawake
Spawn (Tarbaby), 80 health points.

Flags:
"ambush" only wake up on seeing the player, not another monster getting angry

"Triggered"	will not spawn until triggered - triggering again will wake him up. Set 'count' to make this a multi-use spawner.
"NoTfog" 	supress teleport glitter when spawned with 'triggered'
"NoTelefrag" will silently fail to spawn if doing so would telefrag an existing monster. the mapper is responsible for triggering it again.
"Instawake" spawn angry at activator

Keys:
"target" entity to trigger when killed
"targetname" entity name
"movedir" set to a velocity to make the monster jump on awakening
*/
/*FGD
@PointClass base(Monster) size(-16 -16 -24, 16 16 24) model({ "path": ":progs/tarbaby.mdl" }) = monster_tarbaby : "Spawn" []
*/
void() monster_tarbaby =
{
	if (!SUB_ShouldSpawn()) return;
	if (deathmatch || cvar("nomonsters") != 0)
	{
		remove(self);
		return;
	}
	precache_model2 ("progs/tarbaby.mdl");

	precache_sound2 ("blob/death1.wav");
	precache_sound2 ("blob/hit1.wav");
	precache_sound2 ("blob/land1.wav");
	precache_sound2 ("blob/sight1.wav");
	precache_sound2 ("blob/idle.wav");

	setsize (self, '-16 -16 -24', '16 16 40');
	self.health = 80;
	if ( monster_spawnsetup( monster_tarbaby_spawner ) ) return;
	
	addmonster(1);
	monster_tarbaby_spawn();
}
