/*
==============================================================================

SHAL-RATH

==============================================================================
*/

float SHAL_BALL_TURN_RATE	= 225;
float SHAL_BALL_THINK_RATE	= 0.1;//0.075;
float SHAL_BALL_VEL			= 272;


$cd id1/models/shalrath
$origin 0 0 24
$base base
$skin skin
$scale 0.7

$frame attack1 attack2 attack3 attack4 attack5 attack6 attack7 attack8
$frame attack9 attack10 attack11

$frame pain1 pain2 pain3 pain4 pain5 

$frame death1 death2 death3 death4 death5 death6 death7

$frame	walk1 walk2 walk3 walk4 walk5 walk6 walk7 walk8 walk9 walk10
$frame	walk11 walk12

void() shal_stand     =[      $walk1,       shal_stand    ] {ai_stand();}

void() shal_walk1     =[      $walk2,       shal_walk2    ] {
if (random() < 0.2)
	sound (self, CHAN_VOICE, "shalrath/idle.wav", 1, ATTN_IDLE);
ai_walk(6);}
void() shal_walk2     =[      $walk3,       shal_walk3    ] {ai_walk(4);}
void() shal_walk3     =[      $walk4,       shal_walk4    ] {ai_walk(0);}
void() shal_walk4     =[      $walk5,       shal_walk5    ] {ai_walk(0);}
void() shal_walk5     =[      $walk6,       shal_walk6    ] {ai_walk(0);}
void() shal_walk6     =[      $walk7,       shal_walk7    ] {ai_walk(0);}
void() shal_walk7     =[      $walk8,       shal_walk8    ] {ai_walk(5);}
void() shal_walk8     =[      $walk9,       shal_walk9    ] {ai_walk(6);}
void() shal_walk9     =[      $walk10,       shal_walk10    ] {ai_walk(5);}
void() shal_walk10    =[      $walk11,       shal_walk11    ] {ai_walk(0);}
void() shal_walk11    =[      $walk12,       shal_walk12    ] {ai_walk(4);}
void() shal_walk12    =[      $walk1,       shal_walk1    ] {ai_walk(5);}

void() shal_run1     =[      $walk2,       shal_run2    ] {
if (random() < 0.2)
	sound (self, CHAN_VOICE, "shalrath/idle.wav", 1, ATTN_IDLE);
ai_run(6);}
void() shal_run2     =[      $walk3,       shal_run3    ] {ai_run(4);}
void() shal_run3     =[      $walk4,       shal_run4    ] {ai_run(0);}
void() shal_run4     =[      $walk5,       shal_run5    ] {ai_run(0);}
void() shal_run5     =[      $walk6,       shal_run6    ] {ai_run(0);}
void() shal_run6     =[      $walk7,       shal_run7    ] {ai_run(0);}
void() shal_run7     =[      $walk8,       shal_run8    ] {ai_run(5);}
void() shal_run8     =[      $walk9,       shal_run9    ] {ai_run(6);}
void() shal_run9     =[      $walk10,       shal_run10    ] {ai_run(5);}
void() shal_run10    =[      $walk11,       shal_run11    ] {ai_run(0);}
void() shal_run11    =[      $walk12,       shal_run12    ] {ai_run(4);}
void() shal_run12    =[      $walk1,       shal_run1    ] {ai_run(5);}

void() shal_attack1     =[      $attack1,       shal_attack2    ] {
sound (self, CHAN_VOICE, "shalrath/attack.wav", 1, ATTN_NORM);
ai_face();
}
void() shal_attack2     =[      $attack2,       shal_attack3    ] {ai_face();}
void() shal_attack3     =[      $attack3,       shal_attack4    ] {ai_face();}
void() shal_attack4     =[      $attack4,       shal_attack5    ] {ai_face();}
void() shal_attack5     =[      $attack5,       shal_attack6    ] {ai_face();}
void() shal_attack6     =[      $attack6,       shal_attack7    ] {ai_face();}
void() shal_attack7     =[      $attack7,       shal_attack8    ] {ai_face();}
void() shal_attack8     =[      $attack8,       shal_attack9    ] {ai_face();}
void() shal_attack9     =[      $attack9,       shal_attack10   ] {ShalMissile();}
void() shal_attack10    =[      $attack10,      shal_attack11   ] {ai_face();}
void() shal_attack11    =[      $attack11,      shal_run1   ] {}

void() shal_pain1       =[      $pain1, shal_pain2      ] {}
void() shal_pain2       =[      $pain2, shal_pain3      ] {}
void() shal_pain3       =[      $pain3, shal_pain4      ] {}
void() shal_pain4       =[      $pain4, shal_pain5      ] {}
void() shal_pain5       =[      $pain5, shal_run1      ] {}

void() shal_death1      =[      $death1,        shal_death2     ] {}
void() shal_death2      =[      $death2,        shal_death3     ] {self.solid = SOLID_NOT;}
void() shal_death3      =[      $death3,        shal_death4     ] {}
void() shal_death4      =[      $death4,        shal_death5     ] {}
void() shal_death5      =[      $death5,        shal_death6     ] {}
void() shal_death6      =[      $death6,        shal_death7     ] {}
void() shal_death7      =[      $death7,        shal_death7    ] {}


void( entity attacker, float damage ) shalrath_pain =
{
	if (self.health <= 0) return;
	if (self.pain_finished > time) return;

	sound (self, CHAN_VOICE, "shalrath/pain.wav", 1, ATTN_NORM);
	shal_pain1();
	
	PainFinished(3);
}

void() shalrath_die =
{
// check for gib
	if (self.health < -90)
	{
		Gib ("progs/h_shal.mdl", self.health);
		return;
	}

	sound (self, CHAN_VOICE, "shalrath/death.wav", 1, ATTN_NORM);
	shal_death1();
}

/*
================
ShalMissile
================
*/

void() ShalMissileExplode =
{
	T_RadiusDamage (self, self.trueowner, 40, world);
	sound (self, CHAN_WEAPON, "weapons/r_exp3.wav", 1, ATTN_NORM);

	BecomeExplosion();
}

void() ShalMissileTouch =
{
	if (CheckProjectilePassthru()) return;
	if (other == self.owner)
		return;		// don't explode on owner

	if (other.type == "zombie")
		T_Damage (other, self, self.trueowner, other.health + 25);
	
	ShalMissileExplode();
}

vector(entity targ, float turnrate) ShalTurnToward =
{
	vector	vel, aimdir, temp, temp2, out;

	temp = targ.origin + '0 0 10';
	aimdir = temp - self.origin;
	aimdir = normalize(aimdir);
	vel = normalize(self.velocity);
	
	temp = CrossProduct(vel, aimdir);
	out = rotateVectorAround(vel, temp, turnrate * SHAL_BALL_THINK_RATE);
	
	// don't turn past the desired direction
	temp2 = CrossProduct(out, aimdir);
	if ((temp * temp2) < 0)
		return aimdir;
	return out;
}

void() ShalHome =
{
	// a voreball with perfect homing causes combat to effectively stop, forcing the player
	// to run around around trying to shake it, often backpedaling through lots of empty map.
	// limiting the voreball's turn rate so they can be dodged and forgotten about makes vores
	// a more flexible enemy and more useful to the mapper
	float dist, rate, dot;
	
	if (self.lifetime_finished < time)
	{
		ShalMissileExplode();
		return;
	}		
	if (self.enemy.health <= 0)
	{
		remove(self);
		return;
	}
	
	self.nextthink = time + SHAL_BALL_THINK_RATE;
	rate = SHAL_BALL_TURN_RATE;
	dot = normalize(self.velocity) * normalize(self.enemy.origin - self.origin);
	
	// 'poor' homing (half turn rate):
	if ( self.owner.health <= 0		// if our mama dies
		//|| !visible(self.enemy)		// if we lost LOS
		// took ^^ this one out, because it made voreballs even more effective at going
		// around corners, which meant that high skill players had a nice new dodge move
		// but average players who tried to fight the vore the old cautious way were punished
		|| self.enemy.invisible_finished > time			// if target is invis
		|| pointcontents(self.origin) < CONTENT_SOLID ) // if voreball is underwater
	{
		// voreballs headed straight at the player when he's hidden should maintain course, to
		// make the dodge possible without it immediately turning to hit him
		if (dot > 0)
		{
			rate *= max(1, 1.5 * (1 - dot));
		}
		self.velocity = SHAL_BALL_VEL * ShalTurnToward(self.enemy, rate * 0.5);
	}
	else // good homing (full turn rate):
	{
		// voreballs headed straight away from the player don't normally seem to be able to pick a direction to 
		// turn and wobble more or less straight away - amp turn rate in these cases to force a 'decision'
		if (dot < 0)
		{
			rate *= min(1.4, 1.4 + fabs(dot));
		}
		// FIXME: figure out how to stop them veering up or down when they don't have to
		self.velocity = SHAL_BALL_VEL * ShalTurnToward(self.enemy, rate);
	}
	
	// go faster at long distance to close the gap, so snipey vores have a better presence
	dist = vlen(self.enemy.origin - self.origin);
	dist = saturate( (dist-120) / 512 );
	self.velocity *= 1 + dist;
	
	self.think = ShalHome;
	self.oldvelocity = self.velocity;
}


/*
void() ShalHome =
{
	local vector	dir, vtemp;
	
	if (self.attack_finished < time)
	{
		ShalMissileExplode();
		return;
	}		
	
	vtemp = self.enemy.origin + '0 0 10';
	
	if (self.enemy.health < 1)
	{
		remove(self);
		return;
	}
	dir = normalize(vtemp - self.origin);
	if (skill >= 3)
		self.velocity = dir * 350;
	else
		self.velocity = dir * 250;
	self.nextthink = time + 0.2;
	self.think = ShalHome;	
}
*/

void() ShalMissile =
{
	local	entity 	missile;
/*	local	vector	dir;
	local	float	dist, flytime;

	dir = normalize((self.enemy.origin + '0 0 10') - self.origin);
	dist = vlen (self.enemy.origin - self.origin);
	flytime = dist * 0.002;
	if (flytime < 0.1)
		flytime = 0.1;
*/
	self.effects = self.effects | EF_MUZZLEFLASH;
	sound (self, CHAN_WEAPON, "shalrath/attack2.wav", 1, ATTN_NORM);
	makevectors(self.angles);
	
	missile = launch_projectile(self.origin + '0 0 10' + v_forward * 16, normalize((enemy_vispos() + '0 0 10') - self.origin) * 300);

	SUB_ChangeModel (missile, "progs/v_spike.mdl");

	missile.avelocity = '300 300 300';
	missile.nextthink = time + 0.1;
	missile.think = ShalHome;
	missile.enemy = self.enemy;
	missile.touch = ShalMissileTouch;
	missile.lifetime_finished = time + 30;	 // blow up after a while
}

//=================================================================
void() monster_shalrath_spawn =
{
	self.classname = "monster_shalrath";
		
	self.solid = SOLID_SLIDEBOX;
	self.movetype = MOVETYPE_STEP;
	
	setmodel (self, "progs/shalrath.mdl");
	setsize (self, VEC_HULL2_MIN, VEC_HULL2_MAX);
	self.health = 400;

	self.th_stand = shal_stand;
	self.th_walk = shal_walk1;
	self.th_run = shal_run1;
	self.th_die = shalrath_die;
	self.th_pain = shalrath_pain;
	self.th_missile = shal_attack1;
	self.th_checkattack = CheckAttack;

	if (self.deathtype == string_null)
		self.deathtype = "was exploded by a Vore";
		
	self.think = walkmonster_start;
	self.nextthink = time + 0.1 + random ()*0.1;	
}

void() monster_shalrath_spawner = {mon_spawner_use(monster_shalrath_spawn);}


/*QUAKED monster_shalrath (1 0 0) (-32 -32 -24) (32 32 40) Ambush ? ? ? Triggered NoTfog NoTelefrag Instawake
Vore (Shalrath), 400 health points.

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
@PointClass base(Monster) size(-32 -32 -24, 32 32 64) model({ "path": ":progs/shalrath.mdl" }) = monster_shalrath : "Vore" []
*/
void() monster_shalrath =
{
	if (!SUB_ShouldSpawn()) return;
	if (deathmatch || cvar("nomonsters") != 0)
	{
		remove(self);
		return;
	}
	precache_model2 ("progs/shalrath.mdl");
	precache_model2 ("progs/h_shal.mdl");
	precache_model2 ("progs/v_spike.mdl");
	
	precache_sound2 ("shalrath/attack.wav");
	precache_sound2 ("shalrath/attack2.wav");
	precache_sound2 ("shalrath/death.wav");
	precache_sound2 ("shalrath/idle.wav");
	precache_sound2 ("shalrath/pain.wav");
	precache_sound2 ("shalrath/sight.wav");
	
	self.health = 400;
	setsize (self, VEC_HULL2_MIN, VEC_HULL2_MAX);
	if ( monster_spawnsetup( monster_shalrath_spawner ) ) return;
	
	addmonster(1);
	monster_shalrath_spawn();
}
