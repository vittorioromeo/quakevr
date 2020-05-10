/*
==============================================================================

AI

Moving, animating, and decision making

==============================================================================
*/




/*
=============
eyeline
=============
*/
vector(entity targ) eyeline =
{
	vector spot1, spot2;
	spot1 = self.origin + self.view_ofs;
	spot2 = targ.origin + targ.view_ofs;
	
	return (spot2 - spot1);
}

/*
=============
range

returns the range categorization of an entity relative to self
=============
*/
float(entity targ) range =
{
	vector d;
	float r;
	
	if (EntitiesTouching(self, targ))	// always melee someone standing on your head
		return RANGE_MELEE;
	
	d = eyeline(targ);
	d_z *= 2;	// don't melee guys way above or below you
	r = vlen(d);
	if (r < 120)
		return RANGE_MELEE;
	if (r < 500)
		return RANGE_NEAR;
	if (r < 900)
		return RANGE_MID;
	if (r < 1250)
		return RANGE_FAR;
	return RANGE_TOOFAR;
}

/*
=============
visible

returns 1 if the entity is visible to self, even if not infront()

uses enemy's real position, NOT the potentially ring-of-shadows-offset BS position!
=============
*/
float (entity targ) visible =
{
	local vector	spot1, spot2;
	
	spot1 = self.origin + self.view_ofs;
	spot2 = targ.origin + targ.view_ofs;
	traceline2(spot1, spot2, self, TRACE_NOMONSTERS|TRACE_WATER);	// see through other monsters
	
	if (trace_inopen && trace_inwater)
		return FALSE;			// sight line crossed contents

	if (trace_fraction == 1)
		return TRUE;
	return FALSE;
}


/*
=============
infront

returns 1 if the entity is in front (in sight) of self
=============
*/
float(entity targ) infront =
{
	vector vec;
	float dot;
	
	makevectors (self.angles);
	vec = normalize (targ.origin - self.origin);
	dot = vec * v_forward;
	
	if ( dot > 0.3)
	{
		return TRUE;
	}
	return FALSE;
}
/*
float(entity src) infrontof =
{
	local float result;
	local entity temp;
	
	temp = self;
	self = src;
	
	result = infront(temp);
	
	self = temp;
	return result;
}
*/

/*
=============
enemy_vispos

Run every monster's aiming routine against this func instead of self.enemy.origin, so they
can be confused by a ring of shadows and fire in wrong directions
=============
*/
vector() enemy_vispos =
{
	// dogs can still smell the player
	if (self.enemy.invisible_finished < time || self.classname == "monster_dog" )
		return self.enemy.origin;
	
	vector visOrg;
	
	// pick random offset based on time, salted with monster's position for variety
	visOrg = SinCos(time * 121 - self.origin_x - self.origin_y) * (vlen(self.enemy.origin - self.origin) * 0.4 + 64);
	visOrg += self.enemy.origin;
	
	return visOrg;
}

/*
=============
enemy_yaw
=============
*/
float() enemy_yaw =
{
	if (!self.enemy)
		return self.ideal_yaw;
	
	return vectoyaw(enemy_vispos() - self.origin);
}

/*
============
FacingIdeal
============
*/
float() FacingIdeal =
{
	local	float	delta;
	
	delta = anglemod(self.angles_y - self.ideal_yaw);
	if (delta > 45 && delta < 315)
		return FALSE;
	return TRUE;
}


/*
============
LeadTarget

returns a relative position to aim at to lead a shot at a moving target
(not a normalized firing vector, so monsters can modify the result further first)

source: where the projectile will originate from
misvel: speed of projectile
degree: amount to lead target, < 1 will still lag behind target a bit
============
*/
vector(vector source, float misvel, float degree) LeadTarget =
{
	local vector vel, lead_offset, evel;
	
	vel = enemy_vispos() - source;
	// don't lead if you can't see the target
	if (self.enemy.invisible_finished > time)
		return vel;
	
	evel = self.enemy.velocity;
	if( !(self.enemy.flags & FL_ONGROUND) && fabs(evel_z) < 275 )
		evel_z = 0;	// ignore if the player is jumping around
	
	lead_offset = (vlen(vel) / misvel) * evel * degree;
	return vel + lead_offset;
}

vector(vector source, float misvel) LeadTargetBySkill =
{
	// in normal combat, leading above ~50% makes for worse accuracy, not better, because 
	// the player is always changing speed and direction
	return LeadTarget(source, misvel, min(0.5, skill * 0.25)); 
}



/*
==============================================================================

ATTACK CHECKS

==============================================================================
*/


/*
===========
CheckClearAttack

Ensure the line of fire to the enemy isn't blocked by another creature
============
*/
float() CheckClearAttack =
{
	vector spot1, spot2;
	
	if (self.enemy.invisible_finished > time)	// if we can't see him just blaze away
		return TRUE;

	
	spot1 = self.origin + self.view_ofs;
	spot2 = enemy_vispos() + self.enemy.view_ofs;
	traceline2(spot1, spot2, self, TRACE_WATER);

	if (trace_inopen && trace_inwater)
		return FALSE;	// sight line crossed contents

	if (trace_ent != self.enemy)
	{
		if (trace_ent.classname == "player")
			return TRUE;	// go ahead and hit a player
		
		return FALSE;	// don't have a clear shot
	}
	
	return TRUE;
}

/*
===========
ai_attack_finished

lunaran: nightmare is no longer made harder by all monsters always firing. they looked 
really stupid when they'd automatically become turrets, and it made combat much less 
mobile and dynamic (shamblers were particularly 'cheesable')

monster attack frequency is no longer used as a means to artificially inflate or
scale difficulty. a theoretical player who begins learning Quake on Easy would gain
skills and reflexive instincts which can be guaranteed to carry over to higher skills
as that player improves to higher challenges.
============
*/
void(float normal) ai_attack_finished =
{
	self.attack_finished = time + normal;
}

/*
===========
ai_check_refire

these monsters DO fire more than once on nightmare, but not infinitely over and
over - making it random makes it uncertain and thus not cheesable
============
*/
void (void() th, float minf) ai_check_refire_min =
{
	if (self.enemy.health <= 0 || self.enemy.customflags & CFL_PLUNGE )
		return;
	if (!CheckClearAttack())
		return;
	
	if (self.cnt < minf)
	{
		self.cnt += 1;
		self.think = th;
		return;
	}
	
	// no further refire unless nightmare
	if (skill < 3)
	{
		self.cnt = 0;
		return;
	}
	
	// diminish chances of refire with each extra shot
	self.cnt = max(1, (self.cnt-minf) * 2);
	if ( random() * self.cnt > 0.5 )
	{
		self.cnt = 0;
		return;
	}
	self.cnt += minf;
	self.think = th;
}

void (void() th) ai_check_refire =
{
	ai_check_refire_min(th, 0);
}



/*
===========
CheckAttack

The player is in view, so decide to move or launch an attack
Returns FALSE if movement should continue
============
*/
float() CheckAttack =
{
	local float		chance;

	if (!CheckClearAttack()) return FALSE;

	if (enemy_range == RANGE_MELEE || 
		(self.enemy.invisible_finished > time && random() < 0.1) )	// wave arms randomly now and then if target is invisible
	{
		if (self.th_melee)
		{
			self.th_melee ();
			return TRUE;
		}
	}
	
	if (!self.th_missile) return FALSE;
	if (time < self.attack_finished) return FALSE;	// after melee check
	if (enemy_range >= RANGE_FAR) return FALSE;
		
	if (enemy_range == RANGE_MELEE)
	{
		chance = 0.9;
		self.attack_finished = 0;
	}
	else if (enemy_range == RANGE_NEAR)
	{
		if (self.th_melee)
			chance = 0.2;
		else
			chance = 0.4;
	}
	else if (enemy_range == RANGE_MID)
	{
		if (self.th_melee)
			chance = 0.05;
		else
			chance = 0.1;
	}
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


/*
==============================================================================

WAKING UP

==============================================================================
*/
void() HuntTarget =
{
	self.goalentity = self.enemy;
	self.ideal_yaw = enemy_yaw();
	
	// if zombie is lying, finish the pain frames before starting
	if (self.spawnflags & SPAWN_LYING && self.type == "zombie")
	{
		
		if (self.classname == "monster_zombie")
		{
			self.think = zombie_getup;
			self.nextthink = time;// + 0.01;
			ai_attack_finished (3);	// wait a while before first attack
		}
		self.spawnflags = not(self.spawnflags, SPAWN_LYING);	// don't do getup again if an infight starts
		return;
	}
	
	self.think = self.th_run;
	self.nextthink = time + 0.1;
	ai_attack_finished (1);	// wait a while before first attack
}

void() SightSound =
{
	if (self.classname == "monster_ogre" )	
		sound (self, CHAN_VOICE, "ogre/ogwake.wav", 1, ATTN_NORM);
	else if (self.classname == "monster_knight" || self.classname == "monster_axegrunt")
		sound (self, CHAN_VOICE, "knight/ksight.wav", 1, ATTN_NORM);
	else if (self.classname == "monster_shambler")
		sound (self, CHAN_VOICE, "shambler/ssight.wav", 1, ATTN_NORM);
	else if (self.classname == "monster_demon1")
		sound (self, CHAN_VOICE, "demon/sight2.wav", 1, ATTN_NORM);
	else if (self.classname == "monster_wizard")
		sound (self, CHAN_VOICE, "wizard/wsight.wav", 1, ATTN_NORM);
	else if (self.classname == "monster_zombie")
		sound (self, CHAN_VOICE, "zombie/z_idle.wav", 1, ATTN_NORM);
	else if (self.classname == "monster_dog")
		sound (self, CHAN_VOICE, "dog/dsight.wav", 1, ATTN_NORM);
	else if (self.classname == "monster_hell_knight" )
		sound (self, CHAN_VOICE, "hknight/sight1.wav", 1, ATTN_NORM);
	else if (self.classname == "monster_tarbaby")
		sound (self, CHAN_VOICE, "blob/sight1.wav", 1, ATTN_NORM);
	else if (self.classname == "monster_enforcer")
	{
		local float	rsnd;
		rsnd = random();
		if (rsnd < 0.25)
			sound (self, CHAN_VOICE, "enforcer/sight1.wav", 1, ATTN_NORM);
		else if (rsnd < 0.5)
			sound (self, CHAN_VOICE, "enforcer/sight2.wav", 1, ATTN_NORM);
		else if (rsnd < 0.75)
			sound (self, CHAN_VOICE, "enforcer/sight3.wav", 1, ATTN_NORM);
		else
			sound (self, CHAN_VOICE, "enforcer/sight4.wav", 1, ATTN_NORM);
	}
	else if (self.classname == "monster_army")
		sound (self, CHAN_VOICE, "soldier/sight1.wav", 1, ATTN_NORM);
	else if (self.classname == "monster_shalrath")
		sound (self, CHAN_VOICE, "shalrath/sight.wav", 1, ATTN_NORM);
}

void() FoundTarget =
{
	if (self.enemy.health <= 0 || self.enemy.customflags & CFL_PLUNGE)
		return;
	
	if (self.enemy.classname == "player")
	{	// let other monsters see this monster for a while
		sight_entity = self;
		sight_entity_time = time;
	}
	
	self.show_hostile = time + 1;		// wake up other monsters
	self.dmgtime = time + 1;	// aggro timer
	self.search_time = time + 5;	// aggro timer
	
	SightSound ();
	
	// for awakening with velocity
	if (self.movetype == MOVETYPE_STEP && self.movedir != '0 0 0')
	{
		self.velocity = self.movedir;
		self.flags = not(self.flags, FL_ONGROUND);
		self.movedir = '0 0 0';	// don't leap again if an infight starts
	}
	
	HuntTarget ();
}

/*
===========
FindTarget

Self is currently not attacking anything, so try to find a target

Returns TRUE if an enemy was sighted

When a player fires a missile, the point of impact becomes a fakeplayer so
that monsters that see the impact will respond as if they had seen the
player.
============
*/
float() FindTarget =
{
	local entity	client;
	local float		r;

	//dprint("in findtarget\n");
	// (spawnflags & 3) unhacked by lunaran since I actually want to use spawnflag 2 for something
	// apparently all the mega-enforcers in lunsp1 were ambush because of this ... oops
	if ( sight_entity_time >= time - 0.1 && ( !(self.spawnflags & SPAWN_AMBUSH ) ) )
	{
		client = sight_entity;
		if (client.enemy == self.enemy)
			return FALSE;
	}
	else
	{
		// to avoid spending too much time, only check a single client (or fakeclient)
		// this means multiplayer games have slightly slower noticing monsters
		client = checkclient();
		if (!client)
			return FALSE;	// no check entities in PVS
	}

	if (client.flags & FL_NOTARGET || client.movetype == MOVETYPE_NOCLIP)
		return FALSE;
	if (client.health <= 0 || client.customflags & CFL_PLUNGE)
		return FALSE;
	
	if (client == self.enemy)
		return FALSE;
	
	r = range (client);
	if (r == RANGE_TOOFAR)
		return FALSE;
	if (!visible (client))
		return FALSE;
	
	// tarbabies have all-around vision when idle because they are amorphous blobs
	if (r == RANGE_NEAR)
	{
		if (client.show_hostile < time && (!infront(client) && self.classname != "monster_tarbaby"))
			return FALSE;
	}
	else if (r == RANGE_MID || r == RANGE_FAR)
	{
		if ( /* client.show_hostile < time || */ (!infront(client) && self.classname != "monster_tarbaby"))
			return FALSE;
	}
	
	// the only two things that alert monsters when invisible besides damage:
	// - firing a weapon
	// - bumping into a dog
	if (client.items & IT_INVISIBILITY)
	{
		if (self.classname == "monster_dog" && client.classname == "player")
		{
			if (r != RANGE_MELEE)
				return FALSE;
		} 
		else if (client.show_hostile < time)
		{
			return FALSE;
		}
	}

	// got one
	self.enemy = client;
	if (self.enemy.classname != "player")
	{
		self.enemy = self.enemy.enemy;	// angered by a monster angering, use its enemy
		if (self.enemy.classname != "player")
		{
			self.enemy = world;
			return FALSE;
		}
	}
	
	FoundTarget ();

	return TRUE;
}


void(entity m) ai_freeze_monster =
{
	if (m == world)
		error("tried to freeze the world");
	if (m.nextthink > time + 0.11 || m.nextthink < time)
		return;	// don't mess with what isn't active anyway
	
	m.nextthink = time + A_SHITLOAD;
	m.button0 = m.movetype;
	m.button1 = m.solid;
	m.button2 = m.takedamage;
	m.oldorigin = m.velocity;
	
	m.movetype = MOVETYPE_NONE;
	m.solid = SOLID_NOT;
	m.takedamage = DAMAGE_NO;
	m.velocity = VEC_ORIGIN;
}



void(entity m) ai_unfreeze_monster =
{
	if (m == world)
		error("tried to unfreeze the world");
	if (!m.button0 && !m.button1 && !m.button2)
		return;	// never frozen
	
	m.nextthink = time + random() * 0.1 + 0.01;
	m.movetype = m.button0;
	m.solid = m.button1;
	m.takedamage = m.button2;
	m.velocity = m.oldorigin;

	m.button0 = m.button1 = m.button2 = 0;
}


/*
==============================================================================
PATH PATROL CODE

The angle of the pathcorner effects standing and bowing direction, but has 
no effect on movement, which always heads to the next target.
==============================================================================
*/

void() path_corner_touch =
{
	if (other.goalentity != self) return;
	if (other.enemy) return;		// fighting, not following a path
	//bprint("valid pathcorner touch\n");
	
	if (self.target != string_null)
	{
		other.goalentity = findunlocked(world, targetname, self.target);
	}
	else
	{
		other.goalentity = world;
	}
	if (!other.goalentity)
	{
		//bprint("dude stopped without a goalentity\n");
		other.pausetime = time + A_SHITLOAD;
		other.think = other.th_stand;
	//	other.th_stand ();
		other.ideal_yaw = self.angles_y;
		return;
	}

	if (other.classname == "monster_ogre")
		sound (other, CHAN_VOICE, "ogre/ogdrag.wav", 1, ATTN_IDLE);// play chainsaw drag sound

	if (self.wait)
	{
		other.pausetime = time + self.wait;
		other.think = other.th_stand;
	//	other.th_stand ();
		other.ideal_yaw = self.angles_y;
		return;
	}
	other.ideal_yaw = vectoyaw(other.goalentity.origin - other.origin);
}

/*QUAKED path_corner (0.5 0.3 0) (-8 -8 -8) (8 8 8)
Monsters will continue walking towards the next target corner.

Keys:
"targetname" must be present.  The name of this pathcorner.
"target" the next spot to move to.  If not present, stop here for good.
"wait" The number of seconds to spend standing here
"angle" direction to face while standing here
*/
/*FGD
@PointClass base(Appearflags, Targetname, Target, Angle) size(16 16 16) color(0 255 255) = 
	path_corner : 
"Waypoint for platforms and monsters. Will stop here for good if no path_corner is targeted by this entity.
Monsters who wait here will turn to face 'angle'."
[
	wait(integer) : "Wait (seconds)" : 0
]
*/
void() path_corner =
{
	if (!SUB_ShouldSpawn()) return;
	
	if (self.targetname == string_null)
	{
		dprint3("path_corner with no targetname at ", vtos(self.origin), "\n");
		remove(self);
		return;
	}
	
	if (!self.wait)
	{
		if (self.pausetime)
			self.wait = self.pausetime;
		else if (!self.target)
			self.wait = -1;
	}
	
	self.solid = SOLID_TRIGGER;
	self.touch = path_corner_touch;
	setsize (self, '-8 -8 -8', '8 8 8');
}


/*
=============
ai_check_liquid

take damage from slime and lava
=============
*/
void() ai_check_liquid =
{
	// there are hacked takedamage cthons out there, and they're all in the lava
	if (self.type == "boss" || self.classname == "monster_rotfish")
		return;
		
	vector wtt;
	wtt = WaterTest();
	float wl, wt;
	wl = wtt_x;
	wt = wtt_y;
	
	// never do liquid damage for some mild wading, since lots of ep3 has lava
	// the monsters can step into and out of
	if (wl < 3)
		return;
	
	// monsters that spawn in slime/lava were clearly intended to be there by the
	// mapper, so give them special dispensation to not quietly die before the 
	// player ever finds them. also, revoke their liquid rights once they leave, so 
	// they just die instead of staying lost if they fall back in
	if (!wl)
	{
		self.customflags = not(self.customflags, CFL_LIQUID_IMMUNE);
		return;
	}
	if (self.customflags & CFL_LIQUID_IMMUNE)
		return;
		
	if (self.type == "zombie")
	{
		// zombies immune to slime (or else they just keep falling and getting up)
		if (wt == CONTENT_LAVA)	// but always gib if touching lava
			T_Damage(self, world, world, self.health + 10);
	}
	else
	{
		if (wt == CONTENT_LAVA)
			T_Damage(self, world, world, wl * 10);
		if (wt == CONTENT_SLIME && self.classname != "monster_wizard")
			// scrags will happily fly down into slime, and they do spit green shit anyway
			T_Damage(self, world, world, wl * 5);
	}
}

/*
==============================================================================

ANIMATION

==============================================================================
*/


/*
=============
ai_face

Stay facing the enemy
=============
*/
void() ai_face =
{
	ai_check_liquid();
	if (self.flags & (FL_FLY|FL_SWIM))
		self.velocity = '0 0 0';
	
	self.ideal_yaw = enemy_yaw();
	ChangeYaw ();
}


/*
=============
ai_floatmove

wrap movement code in this hot bullshit for making scrags do sick drifts
=============
*/
void() ai_floatmove_start =
{
	if (self.flags & (FL_FLY|FL_SWIM) == 0)
		return;
	
	self.velocity = '0 0 0';
	self.movetype = MOVETYPE_STEP;
	self.oldorigin = self.origin;
	self.flags = not(self.flags, FL_ONGROUND);
}

float(vector mask) ai_floatmove_ceiltrace =
{
	vector start, end;
	start = self.origin + vec_scale(self.maxs, mask);
	end = self.origin + vec_scale(VEC_HULL_MAX, mask);
	
	traceline(start, end, FALSE, self);
	if (trace_fraction < 1.0 || trace_startsolid || trace_allsolid)
		return FALSE;
	return TRUE;
}

float() ai_floatmove_ceilcheck =
{
	if (!ai_floatmove_ceiltrace('0 0 1')) return FALSE;
	if (!ai_floatmove_ceiltrace('1 1 1')) return FALSE;
	if (!ai_floatmove_ceiltrace('-1 1 1')) return FALSE;
	if (!ai_floatmove_ceiltrace('1 -1 1')) return FALSE;
	if (!ai_floatmove_ceiltrace('-1 -1 1')) return FALSE;
	return TRUE;
}

float() ai_fishmovecheck =
{
	float opc;
	opc = pointcontents(self.oldorigin);
	if ( pointcontents(self.origin) != opc || 
		pointcontents(self.origin + '0 0 16') != opc ||
		!ai_floatmove_ceilcheck())	// goddamn fish noclipping through ceilings
		return FALSE;
	return TRUE;
}

void(float dist) ai_floatmove_finish =
{
	vector move;
	float interv, spd, frac;
	
	if (self.flags & (FL_FLY|FL_SWIM) == 0)
		return;
		
	self.movetype = MOVETYPE_FLY;
	move = self.origin - self.oldorigin;
	
	// fish have short bounding boxes, which in quake physics land means they can noclip through ceilings
	if (pointcontents(self.origin) == CONTENT_SOLID)
	{
		setorigin(self, self.oldorigin);
		//dprint("detected move into solid\n");
		self.velocity = '0 0 0';
		return;
	}

	if (vlen(move) > dist + 16)
	{
		// probably touched a teleporter, so don't try to velocity interp
		self.velocity = '0 0 0';
		return;
	}
	
	interv = max(0.01, self.nextthink - time);
	
	// flying/swimming monsters don't raise/lower for regular path walking normally, so
	// fudge the z velocity manually
	if (self.goalentity.classname == "path_corner")
	{
		if (fabs(self.goalentity.origin_z - self.origin_z) > 16)
		{
			spd = vlen(move);
			frac = spd / vlen(self.goalentity.origin - self.origin);
			move_z = frac * (self.goalentity.origin_z - self.origin_z);
		}
	}
	// FL_SWIM just invalidates any movement out of water, so simulate velocity 
	// being clipped off at the surface instead
	// TODO: support water volumes that end in other directions than up
	if (self.flags & FL_SWIM)
	{
		if (!ai_fishmovecheck())
			move_z = -1;
	}
	setorigin(self, self.oldorigin);
	self.velocity = move / interv;
}

/*
=============
movetogoalplus/walkmoveplus
=============
*/
void(float dist) movetogoalplus =
{
	ai_floatmove_start();
	movetogoal(dist);
	ai_floatmove_finish(dist);
}

float(float yaw, float dist) walkmoveplus =
{
	float result;
	vector oldorg;
	ai_floatmove_start();
	oldorg = self.origin;
	result = walkmove(yaw, dist);
	if (oldorg == self.origin || !result)
	{
		ai_floatmove_finish(0);
		return FALSE;
	}
	ai_floatmove_finish(dist);
	return TRUE;
}

/*
=============
walkmove_jitter

try walkmoves on either side of ideal_yaw in case we're blocked by something glancing
=============
*/
float(float yaw, float dist) walkmove_jitter =
{
	float yd;
	if (walkmoveplus(yaw, dist)) return TRUE;
	
	if (random() < 0.5) yd = -15;
	else yd = 15;
	
	if (walkmoveplus(yaw + yd, dist)) return TRUE;
	if (walkmoveplus(yaw - yd, dist)) return TRUE;
	return FALSE;
}

/*
=============
ai_rush

The monster is in a charging melee attack, always turning to face the player
rather than navigating around obstacles. this sometimes leads to getting hung
up and stuck on anything in the way, so add a safety check for our origin not
changing. 

we don't do this in normal ai_charge or wonky things happen with the charges 
as part of normal melee-range attacks involving forward steps
=============
*/
void(float d) ai_rush =
{
	ai_face();
	
	if ( !walkmove_jitter(self.ideal_yaw, d) )
	{
		if ( !walkmove_jitter(self.ideal_yaw, d * 0.5) )
		{
			if (!walkmove_jitter(self.ideal_yaw, 1))
			{
				if (self.oldorigin == self.origin && d > 0)
				{
					// haven't moved in 3 frames, verify charge
					if (range(self.enemy) != RANGE_MELEE || !visible(self.enemy))
					{
						// we aren't in the enemy's face, probably hung up on something
						//bprint("canceling charge\n");
						self.think = self.th_run;
						ai_attack_finished(0.4);	// take a half second to get around the obstacle
						return;
					}
					// it's important to use RANGE_MELEE as the test or else monsters will rapidly
					// oscillate in and out of melee when fights get crowded and they can't move
				}
			}
		}
	}
	
	self.oldorigin = self.origin;
}

/*
=============
ai_charge

The monster is in a melee attack, so get as close as possible to .enemy
=============
*/
float(float d) ai_charge =
{
	ai_face();
	
	// try to get closer
	// don't movetogoal in ai_charge or the yaw will spaz as the monster tries to 'pathfind' in c
	if ( !walkmove_jitter(self.ideal_yaw, d) )
	{
		if ( !walkmove_jitter(self.ideal_yaw, d * 0.5) )
		{
			if (walkmove_jitter(self.ideal_yaw, 1))
			{
				return TRUE;
			}
		}
		else
		{
			return TRUE;
		}
	}
	else
	{
		return TRUE;
	}
	return FALSE;
}

void() ai_charge_side =
{
	local	vector	dtemp;
	local	float	heading;
	
	
// aim to the left of the enemy for a flyby

	self.ideal_yaw = enemy_yaw();
	ChangeYaw ();
	

	makevectors (self.angles);
	dtemp = self.enemy.origin - 30*v_right;
	heading = vectoyaw(dtemp - self.origin);
	
	walkmove_jitter(heading, 20);
}


/*
=============
ai_melee
=============
*/
void() ai_melee =
{
	local vector	delta;
	local float 	ldmg;

	if (!self.enemy)
		return;		// removed before stroke
		
	delta = self.enemy.origin - self.origin;

	if (vlensqr(delta) > 60*60)
		return;
		
	if (self.enemy.type == "zombie")
		ldmg = 15;	// trigger zombie pain so ai_melee'ing monsters don't just slash away endlessly
	else
		ldmg = 3 + random() * 2; //(random() + random() + random()) * 3;
		
	T_DamageMelee (self.enemy, self, ldmg);
}


void() ai_melee_side =
{
	local vector	delta;
	local float 	ldmg;

	if (!self.enemy)
		return;		// removed before stroke
		
	ai_charge_side();
	
	delta = self.enemy.origin - self.origin;

	if (vlensqr(delta) > 60*60)
		return;
	if (!CanDamage (self.enemy, self))
		return;
	ldmg = (random() + random() + random()) * 3;
	T_DamageMelee (self.enemy, self, ldmg);
}

//============================================================================

/*
=============
ai_forward/ai_back

move without invoking any decisionmaking
=============
*/
void(float dist) ai_forward =
{
	walkmove_jitter (self.angles_y, dist);
}

void(float dist) ai_back =
{
	walkmove_jitter ( (self.angles_y+180), dist);
}


/*
=============
ai_pain

stagger back a bit
=============
*/
void(float dist) ai_pain =
{
	ai_back (dist);
}

/*
=============
ai_painforward

stagger back a bit, but forward
=============
*/
void(float dist) ai_painforward =
{
	//walkmove (self.ideal_yaw, dist);
	ai_forward (dist);
}

/*
=============
ai_walk

The monster is walking its beat
=============
*/
void(float dist) ai_walk =
{
	//movedist = dist;
	ai_check_liquid();

	// check for noticing a player
	if (FindTarget ())
		return;
		
	movetogoalplus(dist);	// wraps movetogoal
}


/*
=============
ai_stand

The monster is staying in one place for a while
=============
*/
void() ai_stand =
{
	ai_check_liquid();
	
	if (self.flags & (FL_FLY|FL_SWIM))
		self.velocity = '0 0 0';
	
	if (FindTarget ())
		return;
	
	if (time > self.pausetime && self.goalentity)
	{
		self.th_walk ();
		return;
	}
	
	ChangeYaw();
}

/*
=============
ai_turn

don't move, but turn towards ideal_yaw
=============
*/
void() ai_turn =
{
	
	if (FindTarget ())
		return;
	
	ChangeYaw ();
}


//============================================================================

/*
=============
ai_run_melee

Turn and close until within an angle to launch a melee attack
=============
*/
void() ai_run_melee =
{
	self.ideal_yaw = enemy_yaw();
	ChangeYaw ();

	if (FacingIdeal())
	{
		self.th_melee ();
		self.attack_state = AS_STRAIGHT;
	}
}


/*
=============
ai_run_missile

Turn in place until within an angle to launch a missile attack
=============
*/
void() ai_run_missile =
{
	self.ideal_yaw = enemy_yaw();
	ChangeYaw ();
	if (FacingIdeal())
	{
		self.th_missile ();
		self.attack_state = AS_STRAIGHT;
	}
}


/*
=============
ai_run_slide

Strafe sideways, but stay at approximately the same range
FIXME: check CONTENT_SOLID and reverse movement if any corners go out of the map
to fix scrags strafing into walls
is it strafing only or does flying ai_forward do it too?
=============
*/
void(float dist) ai_run_slide =
{
	local float	ofs;
	
	self.ideal_yaw = enemy_yaw();
	ChangeYaw ();
	if (self.customflags & CFL_LEFTY)
		ofs = 90;
	else
		ofs = -90;
	
	if (walkmoveplus (self.ideal_yaw + ofs, dist))
		return;
	
	self.customflags = xor(self.customflags, CFL_LEFTY);
	//self.lefty = 1 - self.lefty;
	
	walkmoveplus (self.ideal_yaw - ofs, dist);
}


/*
=============
ai_run

The monster has an enemy it is trying to kill
=============
*/
void(float dist) ai_run =
{
	local entity targ;
	
	ai_check_liquid();
	
	if (self.health <= 0) return;	// died in slime/lava already
	
	targ = self.enemy;
	
	// if infighting and we haven't damaged our enemy in a while, give up
	if (self.enemy.classname != "player" && self.oldenemy.classname == "player")
	{
		self.frags += 1;
		// 15 seconds for monsters with ranged attacks, 10 for melee monsters
		if (((self.customflags & CFL_MELEEONLY) && self.frags > 100) || self.frags > 150)
		{
			self.frags = 0;
			SightSound();
			self.enemy = self.oldenemy;
		}
	}
	
	// see if the enemy is dead
	// also consider a downed zombie to be dead, instead of waving a chainsaw over its body forever
	if (targ.health <= 0 || targ.customflags & CFL_KNOCKEDDOWN)
	{
		self.enemy = world;
		// FIXME: look all around for other targets
		if (self.oldenemy.health > 0)
		{
				self.enemy = targ = self.oldenemy;
			
			HuntTarget ();
		}
		else
		{
			if (targ.classname == "player")
			{
				// go mill around the dead enemy
				targ = spawn();
				targ.solid = SOLID_TRIGGER;
				setorigin(targ, self.goalentity.origin);
				setsize (targ, '-64 -64 -512', '64 64 1024');
				targ.angles = self.angles;
				targ.touch = path_corner_touch;

				self.goalentity = targ;
				self.th_walk ();
			}
			else
			{
				self.goalentity = world;
				self.th_stand ();
			}
			return;
		}
	}

	self.show_hostile = time + 1;		// wake up other monsters

	// check knowledge of enemy
	enemy_vis = visible(targ);
	enemy_range = range(targ);
	
	if (enemy_vis)
		self.search_time = time + 5;

	// look for other coop players
	if (coop && self.search_time < time)
	{
		if (FindTarget ())
			return;
	}

	if (enemy_vis && !(self.enemy.customflags & CFL_PLUNGE))
	{
		if (self.th_checkattack())
		{
			if (self.attack_state == AS_MISSILE)
				ai_run_missile ();	
			else if (self.attack_state == AS_MELEE)
				ai_run_melee ();
			else if (self.attack_state == AS_SLIDING)
				ai_run_slide (dist);
			return;
		}
	}
	
	if (self.attack_state == AS_SLIDING)
	{
		ai_run_slide (dist);
		return;
	}
	
	// head straight in
	movetogoalplus(dist);	// wraps movetogoal()
}

/*
=============
ai_nop
=============
*/
void() ai_nop =
{
	if (self.flags & (FL_FLY | FL_SWIM))
		self.velocity = '0 0 0';
}
