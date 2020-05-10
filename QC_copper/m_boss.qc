/*
==============================================================================

BOSS-ONE

==============================================================================
*/
$cd id1/models/boss1
$origin 0 0 -15
$base base
$skin skin
$scale 5

$frame rise1 rise2 rise3 rise4 rise5 rise6 rise7 rise8 rise9 rise10
$frame rise11 rise12 rise13 rise14 rise15 rise16 rise17 

$frame walk1 walk2 walk3 walk4 walk5 walk6 walk7 walk8
$frame walk9 walk10 walk11 walk12 walk13 walk14 walk15
$frame walk16 walk17 walk18 walk19 walk20 walk21 walk22
$frame walk23 walk24 walk25 walk26 walk27 walk28 walk29 walk30 walk31

$frame death1 death2 death3 death4 death5 death6 death7 death8 death9

$frame attack1 attack2 attack3 attack4 attack5 attack6 attack7 attack8
$frame attack9 attack10 attack11 attack12 attack13 attack14 attack15
$frame attack16 attack17 attack18 attack19 attack20 attack21 attack22
$frame attack23

$frame shocka1 shocka2 shocka3 shocka4 shocka5 shocka6 shocka7 shocka8
$frame shocka9 shocka10 

$frame shockb1 shockb2 shockb3 shockb4 shockb5 shockb6

$frame shockc1 shockc2 shockc3 shockc4 shockc5 shockc6 shockc7 shockc8 
$frame shockc9 shockc10

void() boss_face =
{
	
// go for another player if multi player
	if (self.enemy.health <= 0 || random() < 0.02)
	{
		self.enemy = find(self.enemy, classname, "player");
		if (!self.enemy)
			self.enemy = find(self.enemy, classname, "player");
	}
	ai_face();
}

void() boss_rise1	=[	$rise1, boss_rise2 ] {
sound (self, CHAN_WEAPON, "boss1/out1.wav", 1, ATTN_NORM);
}
void() boss_rise2	=[	$rise2, boss_rise3 ] {
sound (self, CHAN_VOICE, "boss1/sight1.wav", 1, ATTN_NORM);
}
void() boss_rise3	=[	$rise3, boss_rise4 ] {}
void() boss_rise4	=[	$rise4, boss_rise5 ] {}
void() boss_rise5	=[	$rise5, boss_rise6 ] {}
void() boss_rise6	=[	$rise6, boss_rise7 ] {}
void() boss_rise7	=[	$rise7, boss_rise8 ] {}
void() boss_rise8	=[	$rise8, boss_rise9 ] {}
void() boss_rise9	=[	$rise9, boss_rise10 ] {}
void() boss_rise10	=[	$rise10, boss_rise11 ] {}
void() boss_rise11	=[	$rise11, boss_rise12 ] {}
void() boss_rise12	=[	$rise12, boss_rise13 ] {}
void() boss_rise13	=[	$rise13, boss_rise14 ] {}
void() boss_rise14	=[	$rise14, boss_rise15 ] {}
void() boss_rise15	=[	$rise15, boss_rise16 ] {}
void() boss_rise16	=[	$rise16, boss_rise17 ] {}
void() boss_rise17	=[	$rise17, boss_missile1 ] {}

void() boss_idle1	=[	$walk1, boss_idle2 ]
{
// look for other players
}
void() boss_idle2	=[	$walk2, boss_idle3 ] {boss_face();}
void() boss_idle3	=[	$walk3, boss_idle4 ] {boss_face();}
void() boss_idle4	=[	$walk4, boss_idle5 ] {boss_face();}
void() boss_idle5	=[	$walk5, boss_idle6 ] {boss_face();}
void() boss_idle6	=[	$walk6, boss_idle7 ] {boss_face();}
void() boss_idle7	=[	$walk7, boss_idle8 ] {boss_face();}
void() boss_idle8	=[	$walk8, boss_idle9 ] {boss_face();}
void() boss_idle9	=[	$walk9, boss_idle10 ] {boss_face();}
void() boss_idle10	=[	$walk10, boss_idle11 ] {boss_face();}
void() boss_idle11	=[	$walk11, boss_idle12 ] {boss_face();}
void() boss_idle12	=[	$walk12, boss_idle13 ] {boss_face();}
void() boss_idle13	=[	$walk13, boss_idle14 ] {boss_face();}
void() boss_idle14	=[	$walk14, boss_idle15 ] {boss_face();}
void() boss_idle15	=[	$walk15, boss_idle16 ] {boss_face();}
void() boss_idle16	=[	$walk16, boss_idle17 ] {boss_face();}
void() boss_idle17	=[	$walk17, boss_idle18 ] {boss_face();}
void() boss_idle18	=[	$walk18, boss_idle19 ] {boss_face();}
void() boss_idle19	=[	$walk19, boss_idle20 ] {boss_face();}
void() boss_idle20	=[	$walk20, boss_idle21 ] {boss_face();}
void() boss_idle21	=[	$walk21, boss_idle22 ] {boss_face();}
void() boss_idle22	=[	$walk22, boss_idle23 ] {boss_face();}
void() boss_idle23	=[	$walk23, boss_idle24 ] {boss_face();}
void() boss_idle24	=[	$walk24, boss_idle25 ] {boss_face();}
void() boss_idle25	=[	$walk25, boss_idle26 ] {boss_face();}
void() boss_idle26	=[	$walk26, boss_idle27 ] {boss_face();}
void() boss_idle27	=[	$walk27, boss_idle28 ] {boss_face();}
void() boss_idle28	=[	$walk28, boss_idle29 ] {boss_face();}
void() boss_idle29	=[	$walk29, boss_idle30 ] {boss_face();}
void() boss_idle30	=[	$walk30, boss_idle31 ] {boss_face();}
void() boss_idle31	=[	$walk31, boss_idle1 ] {boss_face();}

void() boss_missile1	=[	$attack1, boss_missile2 ] {boss_face();}
void() boss_missile2	=[	$attack2, boss_missile3 ] {boss_face();}
void() boss_missile3	=[	$attack3, boss_missile4 ] {boss_face();}
void() boss_missile4	=[	$attack4, boss_missile5 ] {boss_face();}
void() boss_missile5	=[	$attack5, boss_missile6 ] {boss_face();}
void() boss_missile6	=[	$attack6, boss_missile7 ] {boss_face();}
void() boss_missile7	=[	$attack7, boss_missile8 ] {boss_face();}
void() boss_missile8	=[	$attack8, boss_missile9 ] {boss_missile('100 -100 200');}
void() boss_missile9	=[	$attack9, boss_missile10 ] {boss_face();}
void() boss_missile10	=[	$attack10, boss_missile11 ] {boss_face();}
void() boss_missile11	=[	$attack11, boss_missile12 ] {boss_face();}
void() boss_missile12	=[	$attack12, boss_missile13 ] {boss_face();}
void() boss_missile13	=[	$attack13, boss_missile14 ] {boss_face();}
void() boss_missile14	=[	$attack14, boss_missile15 ] {boss_face();}
void() boss_missile15	=[	$attack15, boss_missile16 ] {boss_face();}
void() boss_missile16	=[	$attack16, boss_missile17 ] {boss_face();}
void() boss_missile17	=[	$attack17, boss_missile18 ] {boss_face();}
void() boss_missile18	=[	$attack18, boss_missile19 ] {boss_face();}
void() boss_missile19	=[	$attack19, boss_missile20 ] {boss_missile('100 100 200');}
void() boss_missile20	=[	$attack20, boss_missile21 ] {boss_face();}
void() boss_missile21	=[	$attack21, boss_missile22 ] {boss_face();}
void() boss_missile22	=[	$attack22, boss_missile23 ] {boss_face();}
void() boss_missile23	=[	$attack23, boss_missile1 ] {boss_face();}

void() boss_shocka1 =[	$shocka1, boss_shocka2 ] {}
void() boss_shocka2 =[	$shocka2, boss_shocka3 ] {}
void() boss_shocka3 =[	$shocka3, boss_shocka4 ] {}
void() boss_shocka4 =[	$shocka4, boss_shocka5 ] {}
void() boss_shocka5 =[	$shocka5, boss_shocka6 ] {}
void() boss_shocka6 =[	$shocka6, boss_shocka7 ] {}
void() boss_shocka7 =[	$shocka7, boss_shocka8 ] {}
void() boss_shocka8 =[	$shocka8, boss_shocka9 ] {}
void() boss_shocka9 =[	$shocka9, boss_shocka10 ] {}
void() boss_shocka10 =[	$shocka10, boss_missile1 ] {}

void() boss_shockb1 =[	$shockb1, boss_shockb2 ] {}
void() boss_shockb2 =[	$shockb2, boss_shockb3 ] {}
void() boss_shockb3 =[	$shockb3, boss_shockb4 ] {}
void() boss_shockb4 =[	$shockb4, boss_shockb5 ] {}
void() boss_shockb5 =[	$shockb5, boss_shockb6 ] {}
void() boss_shockb6 =[	$shockb6, boss_shockb7 ] {}
void() boss_shockb7 =[	$shockb1, boss_shockb8 ] {}
void() boss_shockb8 =[	$shockb2, boss_shockb9 ] {}
void() boss_shockb9 =[	$shockb3, boss_shockb10 ] {}
void() boss_shockb10 =[	$shockb4, boss_missile1 ] {}

void() boss_shockc1 =[	$shockc1, boss_shockc2 ] {}
void() boss_shockc2 =[	$shockc2, boss_shockc3 ] {}
void() boss_shockc3 =[	$shockc3, boss_shockc4 ] {}
void() boss_shockc4 =[	$shockc4, boss_shockc5 ] {}
void() boss_shockc5 =[	$shockc5, boss_shockc6 ] {}
void() boss_shockc6 =[	$shockc6, boss_shockc7 ] {}
void() boss_shockc7 =[	$shockc7, boss_shockc8 ] {}
void() boss_shockc8 =[	$shockc8, boss_shockc9 ] {}
void() boss_shockc9 =[	$shockc9, boss_shockc10 ] {}
void() boss_shockc10 =[	$shockc10, boss_death1 ] {}

void() boss_death1 = [$death1, boss_death2] {
sound (self, CHAN_VOICE, "boss1/death.wav", 1, ATTN_NORM);
}
void() boss_death2 = [$death2, boss_death3] {}
void() boss_death3 = [$death3, boss_death4] {}
void() boss_death4 = [$death4, boss_death5] {}
void() boss_death5 = [$death5, boss_death6] {}
void() boss_death6 = [$death6, boss_death7] {}
void() boss_death7 = [$death7, boss_death8] {}
void() boss_death8 = [$death8, boss_death9] {}
void() boss_death9 = [$death9, boss_death10]
{
	// moved kill count here to break boss_death10 killcount hack, as it was only 
	// used to fix the rotfish killcount bug (iw)
	killed_monsters = killed_monsters + 1;
	WriteByte (MSG_ALL, SVC_KILLEDMONSTER);
	
	sound (self, CHAN_BODY, "boss1/out1.wav", 1, ATTN_NORM);
	WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
	WriteByte (MSG_BROADCAST, TE_LAVASPLASH);
	WriteCoord (MSG_BROADCAST, self.origin_x);
	WriteCoord (MSG_BROADCAST, self.origin_y);
	WriteCoord (MSG_BROADCAST, self.origin_z);
}

void() boss_death10 = [$death9, boss_death10]
{
	activator = self.enemy;
	SUB_UseTargets ();
	remove (self);
}

void() boss_missile_touch =
{
	sound(self, CHAN_VOICE, "misc/null.wav", 0.5, ATTN_STATIC);
	sound(self, CHAN_WEAPON, "misc/null.wav", 0.5, ATTN_STATIC);
	sound(self, CHAN_BODY, "misc/null.wav", 0.5, ATTN_STATIC);
	T_MissileTouch();
}

void(float chan) boss_lavahiss =
{
	float r = random();
	if (r < 0.25)
		sound(self, chan + 1, "weapons/lavabal1.wav", 1, ATTN_STATIC);
	else if (r < 0.5)
		sound(self, chan + 1, "weapons/lavabal2.wav", 1, ATTN_STATIC);
	else if (r < 0.75)
		sound(self, chan + 1, "weapons/lavabal3.wav", 1, ATTN_STATIC);
	else
		sound(self, chan + 1, "weapons/lavabal4.wav", 1, ATTN_STATIC);
}

void() boss_missile_think =
{
	self.state = cycle(self.state, 3);
	boss_lavahiss(self.state);
	self.nextthink = time + 0.1;
}

void(vector p) boss_missile =
{
	vector	org, vec, dir;
	entity	missile;
	float 	hard, spd, lead;

	org = self.origin + p_x*v_forward + p_y*v_right + p_z * '0 0 1';
	
	// accelerate the projectile danger with each phase
	hard = (1 - (self.health+self.armorvalue)/self.max_health);
	spd = 300 + 200 * hard;
	
	// lead the player on hard mode
	lead = skill * 0.333;
	lead = lead * lead + 0.05 * hard;
	if (self.enemy.teleport_time > time)	// don't instantly snipe the player the moment he teleports in
		dir = normalize(enemy_vispos() - org);
	else
		dir = LeadTarget(org, spd, min(lead, 0.8));
	
	vec = normalize (dir);

	missile = launch_lavamissile (org, vec*spd);
	missile.avelocity = '200 100 300';
	missile.dmg = 100;
	missile.think = boss_missile_think;
	missile.nextthink = time;
	missile.touch = boss_missile_touch;
	sound (self, CHAN_WEAPON, "boss1/throw.wav", 1, ATTN_NORM);

	// check for dead enemy
	if (self.enemy.health <= 0)
		boss_idle1 ();
}


void() boss_awake =
{
	self.type = "boss";	// so nobody gets mad at us
	self.classname = "monster_boss";	// map hack back compat
	
	self.solid = SOLID_SLIDEBOX;
	self.movetype = MOVETYPE_STEP;
	self.takedamage = DAMAGE_NO;
	
	setmodel (self, "progs/boss.mdl");
	setsize (self, '-128 -128 -24', '128 128 256');
	
	if (skill == 0)
		self.health = 1;
	else
		self.health = 3;
	self.max_health = self.health + self.armorvalue;	// for hacked takedamage cthons

	self.enemy = activator;

	WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
	WriteByte (MSG_BROADCAST, TE_LAVASPLASH);
	WriteCoord (MSG_BROADCAST, self.origin_x);
	WriteCoord (MSG_BROADCAST, self.origin_y);
	WriteCoord (MSG_BROADCAST, self.origin_z);

	self.yaw_speed = 20;
	boss_rise1 ();
}


/*QUAKED monster_boss (1 0 0) (-128 -128 -24) (128 128 256)
*/
/*FGD
@PointClass base(Angle, Appearflags, Target, Targetname, Deathtype) size(-128 -128 -24, 128 128 256) color(0 0 220) model({ "path": ":progs/boss.mdl" }) = monster_boss : "Chthon" []
*/
void() monster_boss =
{
	if (!SUB_ShouldSpawn()) return;
	if (deathmatch || cvar("nomonsters") != 0)
	{
		remove(self);
		return;
	}
	precache_model ("progs/boss.mdl");
	precache_model ("progs/lavaball.mdl");

	precache_sound ("weapons/rocket1i.wav");
	precache_sound ("weapons/lavabal1.wav");
	precache_sound ("weapons/lavabal2.wav");
	precache_sound ("weapons/lavabal3.wav");
	precache_sound ("weapons/lavabal4.wav");
	precache_sound ("boss1/out1.wav");
	precache_sound ("boss1/sight1.wav");
	precache_sound ("misc/power.wav");
	precache_sound ("boss1/throw.wav");
	precache_sound ("boss1/pain.wav");
	precache_sound ("boss1/death.wav");
	
	if (self.deathtype == string_null)
		self.deathtype = "was owned by Chthon";

	total_monsters = total_monsters + 1;

	self.type = "boss";	// so nobody gets mad at us
	self.use = boss_awake;
}

//===========================================================================

void() lightning_fire =
{
	vector	p1, p2;
	entity	le1, le2;
	
	le1 = self.buddy;
	le2 = le1.buddy;
	
	if (time >= self.attack_finished)
	{	// done here, put the terminals back up
		activator = self.enemy;
		SUB_CallAsSelf(door_go_down, le1);
		SUB_CallAsSelf(door_go_down, le2);
		return;
	}
	
	p1 = (le1.mins + le1.maxs) * 0.5;
	p1_z = le1.absmin_z - 8;
	
	p2 = (le2.mins + le2.maxs) * 0.5;
	p2_z = le2.absmin_z - 8;
	
	// compensate for length of bolt
	p2 = p2 - normalize(p2-p1)*100;

	self.nextthink = time + 0.1;
	self.think = lightning_fire;

	WriteByte (MSG_ALL, SVC_TEMPENTITY);
	WriteByte (MSG_ALL, TE_LIGHTNING3);
	WriteEntity (MSG_ALL, world);
	WriteCoord (MSG_ALL, p1_x);
	WriteCoord (MSG_ALL, p1_y);
	WriteCoord (MSG_ALL, p1_z);
	WriteCoord (MSG_ALL, p2_x);
	WriteCoord (MSG_ALL, p2_y);
	WriteCoord (MSG_ALL, p2_z);
}

void() lightning_use =
{
	entity	le1, le2;
	
	if (self.attack_finished >= time + 1)
		return;

	le1 = find( world, target, "lightning");
	le2 = find( le1, target, "lightning");
	if (!le1 || !le2)
	{
		dprint ("missing lightning targets\n");
		return;
	}
	
	self.buddy = le1;
	le1.buddy = le2;
	
	if ((le1.state != le2.state) || 
		(le1.state != STATE_TOP && le1.state != STATE_BOTTOM) ||
		(le2.state != STATE_TOP && le2.state != STATE_BOTTOM) )
	{
//		dprint ("not aligned\n");
		return;
	}

// don't let the electrodes go back up until the bolt is done
	le1.nextthink = -1;
	le2.nextthink = -1;
	self.attack_finished = time + 1;

	self.enemy = activator;
	sound (self, CHAN_VOICE, "misc/power.wav", 1, ATTN_NORM);
	lightning_fire ();		

	if (le1.state == STATE_TOP && le2.state == STATE_TOP)
		SUB_UseTargets();
		
	// advance the boss pain if down
	self = find (world, classname, "monster_boss");
	if (!self)
		return;
	if (le1.state == STATE_TOP && self.health > 0)
	{
		sound (self, CHAN_VOICE, "boss1/pain.wav", 1, ATTN_NORM);
		self.health = self.health - 1;
		if (self.health >= 2)
			boss_shocka1();
		else if (self.health == 1)
			boss_shockb1();
		else if (self.health == 0)
			boss_shockc1();
	}
	
}


/*QUAKED event_lightning (0 1 1) (-16 -16 -16) (16 16 16)
Just for boss level. Fires targets on a successful zap.
*/
/*FGD
@PointClass base(Appearflags, Target, Targetname) =
	event_lightning : "Chthon lightning. Fires targets on a successful zap." []
*/
void() event_lightning =
{
	if (!SUB_ShouldSpawn()) return;
	precache_sound ("misc/power.wav");			//lightning for boss

	self.use = lightning_use;
}



