$cd id1/models/fish
$origin 0 0 24
$base base		
$skin skin

$frame attack1 attack2 attack3 attack4 attack5 attack6 
$frame attack7 attack8 attack9 attack10 attack11 attack12 attack13 
$frame attack14 attack15 attack16 attack17 attack18 

$frame death1 death2 death3 death4 death5 death6 death7 
$frame death8 death9 death10 death11 death12 death13 death14 death15 
$frame death16 death17 death18 death19 death20 death21 

$frame swim1 swim2 swim3 swim4 swim5 swim6 swim7 swim8 
$frame swim9 swim10 swim11 swim12 swim13 swim14 swim15 swim16 swim17 
$frame swim18 

$frame pain1 pain2 pain3 pain4 pain5 pain6 pain7 pain8 
$frame pain9 

void() f_stand1 =[	$swim1, f_stand2 ] {ai_stand();}
void() f_stand2 =[	$swim2, f_stand3 ] {ai_stand();}
void() f_stand3 =[	$swim3, f_stand4 ] {ai_stand();}
void() f_stand4 =[	$swim4, f_stand5 ] {ai_stand();}
void() f_stand5 =[	$swim5, f_stand6 ] {ai_stand();}
void() f_stand6 =[	$swim6, f_stand7 ] {ai_stand();}
void() f_stand7 =[	$swim7, f_stand8 ] {ai_stand();}
void() f_stand8 =[	$swim8, f_stand9 ] {ai_stand();}
void() f_stand9 =[	$swim9, f_stand10 ] {ai_stand();}
void() f_stand10 =[	$swim10, f_stand11 ] {ai_stand();}
void() f_stand11 =[	$swim11, f_stand12 ] {ai_stand();}
void() f_stand12 =[	$swim12, f_stand13 ] {ai_stand();}
void() f_stand13 =[	$swim13, f_stand14 ] {ai_stand();}
void() f_stand14 =[	$swim14, f_stand15 ] {ai_stand();}
void() f_stand15 =[	$swim15, f_stand16 ] {ai_stand();}
void() f_stand16 =[	$swim16, f_stand17 ] {ai_stand();}
void() f_stand17 =[	$swim17, f_stand18 ] {ai_stand();}
void() f_stand18 =[	$swim18, f_stand1 ] {ai_stand();}

void() f_walk1 =[	$swim1, f_walk2 ] {ai_walk(5);}
void() f_walk2 =[	$swim2, f_walk3 ] {ai_walk(6);}
void() f_walk3 =[	$swim3, f_walk4 ] {ai_walk(7);}
void() f_walk4 =[	$swim4, f_walk5 ] {ai_walk(8);}
void() f_walk5 =[	$swim5, f_walk6 ] {ai_walk(9);}
void() f_walk6 =[	$swim6, f_walk7 ] {ai_walk(10);}
void() f_walk7 =[	$swim7, f_walk8 ] {ai_walk(9);}
void() f_walk8 =[	$swim8, f_walk9 ] {ai_walk(8);}
void() f_walk9 =[	$swim9, f_walk10 ] {ai_walk(7);}
void() f_walk10 =[	$swim10, f_walk11 ] {ai_walk(6);}
void() f_walk11 =[	$swim11, f_walk12 ] {ai_walk(5);}
void() f_walk12 =[	$swim12, f_walk13 ] {ai_walk(6);}
void() f_walk13 =[	$swim13, f_walk14 ] {ai_walk(7);}
void() f_walk14 =[	$swim14, f_walk15 ] {ai_walk(8);}
void() f_walk15 =[	$swim15, f_walk16 ] {ai_walk(9);}
void() f_walk16 =[	$swim16, f_walk17 ] {ai_walk(8);}
void() f_walk17 =[	$swim17, f_walk18 ] {ai_walk(7);}
void() f_walk18 =[	$swim18, f_walk1 ] {ai_walk(6);}

void() f_run1 =[	$swim1, f_run2 ] {ai_run(12);
	if (random() < 0.5)
		sound (self, CHAN_VOICE, "fish/idle.wav", 1, ATTN_NORM);
}
void() f_run2 =[	$swim3, f_run3 ] {ai_run(14);}
void() f_run3 =[	$swim5, f_run4 ] {ai_run(16);}
void() f_run4 =[	$swim7, f_run5 ] {ai_run(14);}
void() f_run5 =[	$swim9, f_run6 ] {ai_run(12);}
void() f_run6 =[	$swim11, f_run7 ] {ai_run(14);}
void() f_run7 =[	$swim13, f_run8 ] {ai_run(16);}
void() f_run8 =[	$swim15, f_run9 ] {ai_run(14);}
void() f_run9 =[	$swim17, f_run1 ] {ai_run(12);}

void() fish_melee =
{
	float ldmg;

	if (!self.enemy || self.enemy.health <= 0)
		return;		// removed before stroke
	
	if (vlen(self.enemy.origin - self.origin) > 60)
	{
		self.think = f_run1;
		ai_attack_finished(1);
		return;
	}
		
	sound (self, CHAN_VOICE, "fish/bite.wav", 1, ATTN_NORM);
	ldmg = 6 + random() * 6;
	T_DamageMelee (self.enemy, self, ldmg);
}

/*
void() fish_checkdash =
{
	if (vlen(self.origin - self.enemy.origin) < 60)
	{
		if(infront(self.enemy))
			f_attack3();
		else
			f_run1();
		return;
	}
	if (!ai_fishmovecheck() || self.velocity != self.oldvelocity)
	{
		f_run1();
		return;
	}
}

void() f_dash =
{
	ai_face();
	ai_face();
	if (!infront(self.enemy))
	{
		f_run1();
		return;
	}
	self.velocity = normalize(enemy_vispos() - self.origin) * 300;
	self.oldvelocity = self.velocity;
	f_dash1();
}

void() f_dash1 =[	$swim1, f_dash2 ] {
	//ai_rush(20);
	if (random() < 0.1)
		sound (self, CHAN_VOICE, "fish/idle.wav", 1, ATTN_NORM);
}
void() f_dash2 =[	$swim3, f_dash3 ] {fish_checkdash();}
void() f_dash3 =[	$swim6, f_dash4 ] {fish_checkdash();}
void() f_dash4 =[	$swim10, f_dash5 ] {fish_checkdash();}
void() f_dash5 =[	$swim12, f_dash6 ] {fish_checkdash();}
void() f_dash6 =[	$swim14, f_dash7 ] {fish_checkdash();}
void() f_dash7 =[	$swim16, f_dash1 ] {fish_checkdash();}
*/
void() f_attack =
{
	float r;
	r = random();
	if (r<0.33)
		f_attack1();
	else if (r<0.66)
		f_attack7();
	else
		f_attack13();
}

void() f_attack1 = [ $attack1, f_attack2 ] {ai_charge(10);}
void() f_attack2 = [ $attack2, f_attack3 ] {ai_charge(10);}
void() f_attack3 = [ $attack3, f_attack4 ] {fish_melee();}
void() f_attack4 = [ $attack4, f_attack5 ] {ai_charge(10);}
void() f_attack5 = [ $attack5, f_attack6 ] {ai_charge(10);}
void() f_attack6 = [ $attack6, f_attack7 ] {ai_charge(10);}
void() f_attack7 = [ $attack7, f_attack8 ] {ai_charge(10);}
void() f_attack8 = [ $attack8, f_attack9 ] {ai_charge(10);}
void() f_attack9 = [ $attack9, f_attack10] {fish_melee();}
void() f_attack10 =[ $attack10, f_attack11] {ai_charge(10);}
void() f_attack11 =[ $attack11, f_attack12] {ai_charge(10);}
void() f_attack12 =[ $attack12, f_attack13] {ai_charge(10);}
void() f_attack13 =[ $attack13, f_attack14] {ai_charge(10);}
void() f_attack14 =[ $attack14, f_attack15] {ai_charge(10);}
void() f_attack15 =[ $attack15, f_attack16] {fish_melee();}
void() f_attack16 =[ $attack16, f_attack17] {ai_charge(10);}
void() f_attack17 =[ $attack17, f_attack18] {ai_charge(10);}
void() f_attack18 =[ $attack18, f_attack1 ] {ai_charge(10);}

void() f_death1 =[	$death1, f_death2 ] {}
void() f_death2 =[	$death2, f_death3 ] {}
void() f_death3 =[	$death3, f_death4 ] {}
void() f_death4 =[	$death4, f_death5 ] {}
void() f_death5 =[	$death5, f_death6 ] {}
void() f_death6 =[	$death6, f_death7 ] {}
void() f_death7 =[	$death7, f_death8 ] {}
void() f_death8 =[	$death8, f_death9 ] {}
void() f_death9 =[	$death9, f_death10 ] {}
void() f_death10 =[	$death10, f_death11 ] {}
void() f_death11 =[	$death11, f_death12 ] {}
void() f_death12 =[	$death12, f_death13 ] {}
void() f_death13 =[	$death13, f_death14 ] {}
void() f_death14 =[	$death14, f_death15 ] {}
void() f_death15 =[	$death15, f_death16 ] {}
void() f_death16 =[	$death16, f_death17 ] {}
void() f_death17 =[	$death17, f_death18 ] {}
void() f_death18 =[	$death18, f_death19 ] {}
void() f_death19 =[	$death19, f_death20 ] {}
void() f_death20 =[	$death20, f_death21 ] {}
void() f_death21 =[	$death21, f_death21 ] {}

void() f_pain1 =[	$pain1, f_pain2 ] {ai_nop();}
void() f_pain2 =[	$pain2, f_pain3 ] {ai_pain(6);}
void() f_pain3 =[	$pain3, f_pain4 ] {ai_pain(6);}
void() f_pain4 =[	$pain4, f_pain5 ] {ai_pain(6);}
void() f_pain5 =[	$pain5, f_pain6 ] {ai_pain(6);}
void() f_pain6 =[	$pain6, f_pain7 ] {ai_pain(6);}
void() f_pain7 =[	$pain7, f_pain8 ] {ai_pain(6);}
void() f_pain8 =[	$pain8, f_pain9 ] {ai_pain(6);}
void() f_pain9 =[	$pain9, f_run1 ] {ai_pain(6);}

void(entity attacker, float damage)	fish_pain =
{
	if (time < self.pain_finished) return;
	f_pain1 ();
	PainFinished(1);
}

void() fish_gib =
{
	entity meat;
	vector mvel;
	
	mvel_x = crandom() * 128;
	mvel_y = crandom() * 128;
	mvel_z = random() * 256;

	meat = toss_projectile(self.origin, mvel);
	//meat.solid = SOLID_NOT;
	meat.type = "gib";
	meat.avelocity = '300 100 200';
	meat.lifetime_finished = time + 1;
	meat.classname = "gib";
	
	SUB_ChangeModel(meat, "progs/zom_gib.mdl");
}

void() fish_die =
{
	self.solid = SOLID_NOT;
	
	// just spray a bunch of blood and bubbles
	if (self.health < -30)
	{
		vector org, bl;
		float x,y,z;
		org = self.origin - '0 0 8';
		for (x=-1;x<2;x+=2) for (y=-1;y<2;y+=2) for (z=-1;z<2;z+=2)
		{
			bl = Vector(8 * x * random(), 8 * y * random(), 8 * z * random());
			particle(org + bl, bl, 73, 20);
			particle(org - bl, '0 0 1', 73, 20);
			// removing bubbles because there's no air in fish
			//if (random() < 0.5)
			//	bubble_spawn(org - bl);
			// also duh
		}
		sound (self, CHAN_VOICE, "player/tornoff2.wav", 1, ATTN_NORM);
		for (x=0;x<3;x++)
			fish_gib();
		SUB_RemoveSoon();
		return;
	}
	
	sound (self, CHAN_VOICE, "fish/death.wav", 1, ATTN_NORM);
	f_death1();
}

void() monster_fish_spawn =
{
	self.classname = "monster_fish";
		
	self.solid = SOLID_SLIDEBOX;
	self.movetype = MOVETYPE_STEP;

	setmodel (self, "progs/fish.mdl");

	setsize (self, '-16 -16 -24', '16 16 8');
	//setsize (self, VEC_HULL_MIN, VEC_HULL_MAX);
	self.health = 35;
	self.customflags |= CFL_MELEEONLY;
	
	self.th_stand = f_stand1;
	self.th_walk = f_walk1;
	self.th_run = f_run1;
	self.th_die = fish_die;
	self.th_pain = fish_pain;
	self.th_melee = f_attack;
	//self.th_missile = f_dash;
	self.th_checkattack = CheckAttack;

	if (self.deathtype == string_null)
		self.deathtype = "was fed to the Rotfish";

	swimmonster_start ();
}

void() monster_fish_spawner = {mon_spawner_use(monster_fish_spawn);}


/*QUAKED monster_fish (1 0 0) (-16 -16 -24) (16 16 24) Ambush ? ? ? Triggered NoTfog NoTelefrag Instawake
Rotfish, 35 health points.

Flags:
"ambush" only wake up on seeing the player, not another monster getting angry
"Triggered"	will not spawn until triggered - triggering again will wake him up. Set 'count' to make this a multi-use spawner.
"NoTfog" 	supress teleport glitter when spawned with 'triggered'
"NoTelefrag" will silently fail to spawn if doing so would telefrag an existing monster. the mapper is responsible for triggering it again.
"Instawake" spawn angry at activator

Keys:
"target" entity to trigger when killed
"targetname" entity name
*/
/*FGD
@PointClass base(Monster) size(-16 -16 -24, 16 16 24) model({ "path": ":progs/fish.mdl" }) = monster_fish : "Rotfish" []
*/
void() monster_fish =
{
	if (!SUB_ShouldSpawn()) return;
	if (deathmatch || cvar("nomonsters") != 0)
	{
		remove(self);
		return;
	}
	precache_model2 ("progs/fish.mdl");
	precache_model ("progs/zom_gib.mdl");
	precache_sound2 ("fish/death.wav");
	precache_sound2 ("fish/bite.wav");
	precache_sound2 ("fish/idle.wav");

	self.health = 35;
	setsize (self, '-16 -16 -24', '16 16 8');
	//setsize (self, VEC_HULL_MIN, VEC_HULL_MAX);
	if ( monster_spawnsetup( monster_fish_spawner ) ) return;
	addmonster(1);
	monster_fish_spawn();
}
