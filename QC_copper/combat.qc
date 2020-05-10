/*
==============================================================================

COMBAT

==============================================================================
*/

void() killed_monster =
{
	killed_monsters = killed_monsters + 1;
	WriteByte (MSG_ALL, SVC_KILLEDMONSTER);
}

/*
============
Killed
============
*/
void(entity targ, entity attacker) Killed =
{
	local entity oself;

	activator = attacker;	// catches a lot of misc cases like a shootable door firing a trigger_secret
	
	oself = self;
	self = targ;
	
	//bprint4(self.classname, " died with ", ftos(self.health), " hp\n");
	
	if (self.health < -99)
		self.health = -99;		// don't let sbar look bad (or throw gibs with crazy speed)

	if (self.movetype == MOVETYPE_PUSH || // doors, triggers, etc
		(self.movetype == MOVETYPE_NONE && self.classname != "player")) 
		// players are movetype none while hanging out in a void pit waiting to die
	{
		//self.enemy = attacker;	// why did you do this lunaran
		self.th_die ();
		self = oself;
		return;
	}

	// ai_nav
	//if (self.navtarget) remove(self.navtarget);
	// clear floaty bullshit on scrags and fish
	ai_floatmove_start();
	
	self.enemy = attacker;
	self.deadflag = DEAD_DYING;
	self.takedamage = DAMAGE_NO;
	self.touch = SUB_Null;

	// bump the monster counter
	if (self.flags & FL_MONSTER)
	{
		killed_monster();
		monster_death_use();
	}


	if (self.classname != "player" && self.customflags & CFL_PLUNGE)
		remove(self);
	else
		self.th_die ();
	
	self = oself;
}



void(float t) PainFinished =
{
	//if (skill > 2)
	//	self.pain_finished = time + (t * 1.25);
	//else
		self.pain_finished = time + t;
}

/*
==============================================================================

MULTI-DAMAGE

Collects multiple small damages into a single damage. Creates a chain of 
entities and stores damage on each one, then applies it one by one and 
wipes the chain.


==============================================================================
*/

entity	multi_ent;

void() ClearMultiDamage =
{
	multi_ent = world;
}

void() ApplyMultiDamage =
{
	local entity head;
	if (!multi_ent)
		return;
	head = multi_ent;
	while (head) {
		//bprint(ftos(head.multi_dmg));
		//bprint("\n");
		T_Damage (head, self, self, head.multi_dmg);
		head.multi_dmg = 0;
		head = head.multidmg_chain;
	}
}

void(entity hit, float damage) AddMultiDamage =
{
	local entity head;
	
	if (!hit)
		return;

	if (!multi_ent)
	{
		// start the chain
		head = multi_ent = hit;
		head.multidmg_chain = world;
	}
	
	else if (hit != multi_ent)
	{
		// search the chain for the entity we hit
		head = multi_ent;
		while (head) {
			if (head == hit) break;
			head = head.multidmg_chain;
		}
		if (!head) {
			// put him at the beginning of the chain
			hit.multidmg_chain = multi_ent;
			multi_ent = hit;
			head = hit;
		}
	}
	else
		head = multi_ent;
	
	head.multi_dmg = head.multi_dmg + damage;	
}


/*
============
CanDamage

Returns true if the inflictor can directly damage the target.  Used for
explosions and melee attacks.
============
*/
float(entity targ, entity inflictor) CanDamage =
{

	// bmodels need special checking because their origin is 0,0,0
	if (targ.movetype == MOVETYPE_PUSH)
	{
		traceline2(inflictor.origin, 0.5 * (targ.absmin + targ.absmax), self, TRACE_NOMONSTERS);
		if (trace_fraction == 1)
			return TRUE;
		if (trace_ent == targ)
			return TRUE;
		return FALSE;
	}
	
	vector corner;
	
	// center
	traceline2(inflictor.origin, targ.origin, self, TRACE_NOMONSTERS);
	if (trace_fraction == 1)
		return TRUE;
	
	// lunaran: adapted from maddes fix
	
	// do top first, as it's most likely to return a positive
	corner = Vector(0,0,targ.maxs_z);
	traceline2(inflictor.origin, targ.origin + corner, self, TRACE_NOMONSTERS);
	if (trace_fraction == 1)
		return TRUE;
	
	// check proper extents of wider monsters
	corner = targ.size * 0.5 - '1 1 0';
	corner_z = 0;
	traceline2(inflictor.origin, targ.origin + corner, self, TRACE_NOMONSTERS);
	if (trace_fraction == 1)
		return TRUE;
		
	corner_x *= -1;
	traceline2(inflictor.origin, targ.origin + corner, self, TRACE_NOMONSTERS);
	if (trace_fraction == 1)
		return TRUE;
		
	corner_y *= -1;
	traceline2(inflictor.origin, targ.origin + corner, self, TRACE_NOMONSTERS);
	if (trace_fraction == 1)
		return TRUE;
		
	corner_x *= -1;
	traceline2(inflictor.origin, targ.origin + corner, self, TRACE_NOMONSTERS);
	if (trace_fraction == 1)
		return TRUE;

	// bottom
	corner = Vector(0,0,targ.mins_z);
	traceline2(inflictor.origin, targ.origin + corner, self, TRACE_NOMONSTERS);
	if (trace_fraction == 1)
		return TRUE;
	
	return FALSE;
}

/*
============
ArmorSave

save damage based on the target's armor level.
is separate from T_Damage and duplicates a few checks so that func_voids can check
	if they would kill the player before they actually try
============
*/
float ArmorSave(entity targ, float damage)
{
	if (!damage) return 0;
	if (!targ.takedamage)
		return 0;
	if (targ.flags & FL_GODMODE || targ.invincible_finished >= time)
		return 0;

	float save, savefrac;
		
	if (targ.deathtype == "drowning" ||	// armor doesn't protect you from drowning
		targ.deathtype == "noarmor" || // only used by skill trigger health adjustment so far
		targ.armorvalue <= 0)
		return 0;

	savefrac = targ.armortype * damage;

	if (targ.classname == "player")
	{
		/* ========		
		lots of tiny damages (like ai_melee or crushy movers) will always round to armor
		when yellow or red, stripping it away too fast, and always round to health when
		green, defeating the purpose of armor.
		
		instead we randomly apply the last fractional point to either armor or health so 
		that it's statistically balanced (... over geologic timescales)
		======== */
		save = floor(savefrac);
		savefrac -= save;
		if ( random() < savefrac ) save += 1;
	}
	else
	{
		// maintain old round-up behavior for everything else because mappers will use
		// QC hacks to do just about anything
		save = ceil(savefrac);
	}
	save = min(save, targ.armorvalue);
	
	return save;
}

/*
============
T_ReactToDamage

.dmgtime: a monster will not be distracted from its current target 
	until after this time - set when hurt
============
*/
void( entity attacker ) T_ReactToDamage =
{
	// ignore self dmg
	if ( attacker == self ) return;
	// ignore bosses because we can't kill them anyway
	if ( attacker.type == "boss" ) return;	
	

	if (attacker == self.enemy)
	{
		self.frags = 0;	// .frags used on monsters as a 'give up on infighting' timer
		return;
	}

	if (attacker.classname == "player")
	{
		if (self.enemy.classname != "player")
		{
			self.enemy = attacker;
			FoundTarget ();
		}
		self.enemy = attacker;

		self.dmgtime = time + 2;	// players pull guaranteed aggro for 2 seconds before infighting can distract
		return;
	}

	// ========
	// all infighting cases follow:
	
	// don't switch away from a player who's actively attacking
	if (time < self.dmgtime) return;
	
	// ignore doors and nailshooters and so on
	if (!(attacker.flags & FL_MONSTER)) return;
	
	// fish should ignore monsters that aren't in the water with them
	if (self.flags & FL_SWIM && pointcontents(attacker.origin) > CONTENT_WATER) return;
	
	// get mad unless of the same class (except for soldiers)
	// classname check replaced with new "type" check
	// .type defaults to classname anyway, or can be overwritten so monsters can be grouped into mutually not-infighting groups
	if ( ( self.type != attacker.type ) || ( self.classname == "monster_army" ) )
	{
		if (self.enemy.classname == "player")
			self.oldenemy = self.enemy;
		self.enemy = attacker;
		self.dmgtime = time + 1;	// focus on infight for a minumum of 1 second
		FoundTarget();
	}
}

/*
============
T_Damage

The damage is coming from inflictor, but get mad at attacker
This should be the only function that ever reduces health.
============
*/
void(entity targ, entity inflictor, entity attacker, float damage) T_Damage =
{
	vector	dir;
	entity	oldself;
	float	save, take;

	if (!damage || !targ.takedamage) return;
		
	if (targ == attacker)
	{
		// monsters which shouldn't be able to splash damage themselves to death:
		if (attacker.type == "boss") return;
		if (attacker.classname == "monster_ogre" || 
			attacker.classname == "monster_shalrath")
			return;
	}
	
	// used by buttons and triggers to set activator for target firing
	damage_attacker = attacker;

	if (attacker.super_damage_finished > time)
	{
		damage = damage * 4;
		
		// silly hacky fix:
		// knight health was dropped 3hp (75 to 72) so that exactly three shells can kill a knight
		// the side effect of this was that one quad SNG shot (which does 72 dmg) would exactly kill a knight
		// when, in vanilla, one would drop the knight to 3hp and the next would gib him
		// the cleanest way to preserve the 'feel' of this is:
		if (targ.classname == "monster_knight" && attacker.classname == "player" && inflictor.classname == "spike")
			damage -= 3;
	}
	
	
	if (attacker.classname == targ.classname)
	{
		if (attacker.classname != "player" && attacker.classname != "monster_grunt")
			damage = damage * 0.5;
	}

	// .frags used on monsters to count time spent not damaging an enemy, so impossible infights time out
	if (attacker.flags & FL_MONSTER && attacker.enemy == targ)
	{
		attacker.frags = 0;
	}

	// figure momentum add
	if ( (inflictor != world) && (targ.movetype == MOVETYPE_WALK) )
	{
		dir = targ.origin - (inflictor.absmin + inflictor.absmax) * 0.5;
		dir = normalize(dir);
		targ.velocity = targ.velocity + dir*damage*8;
	}

	if (targ.flags & FL_GODMODE)
		return;
	
	// check for artifacts
	
	if (targ.invincible_finished >= time)
	{
		if (targ.invincible_sound < time)
		{
			sound (targ, CHAN_ITEM, "items/protect3.wav", 1, ATTN_NORM);
			targ.invincible_sound = time + 1;
		}
		return;
	}

	// team play damage avoidance
	if ( (teamplay == 1) && 
		(targ.team > 0) && 
		(targ.team == attacker.team) && 
		(targ != attacker) && 
		(attacker.classname == "player") && 
		(inflictor.classname != "door") )	// because squishing a teammate is still possible
		return;

	// ready to do the damage
	save = ArmorSave(targ, damage);
	targ.armorvalue = targ.armorvalue - save;
	if (targ.classname == "player")
		armor_set_type(targ);
	take = damage - save;

	// add to the damage total for clients, which will be sent as a single
	// message at the end of the frame
	// FIXME: remove after combining shotgun blasts?
	if (targ.flags & FL_CLIENT)
	{
		targ.dmg_take = targ.dmg_take + take;
		targ.dmg_save = targ.dmg_save + save;
		targ.dmg_inflictor = inflictor;
	}

	// do the damage
	if (targ.customflags & CFL_UNDYING)
		take = min(take, targ.health - 1);
	
	//bprint3("doing ", ftos(take), " damage\n");
	targ.health = ceil(targ.health - take);	// be an integer dammit
	
	if (targ.health <= 0)
	{
		Killed (targ, attacker);
		return;
	}

	// react to the damage
	oldself = self;
	self = targ;
	if ( (self.flags & FL_MONSTER) && attacker != world)
		T_ReactToDamage( attacker );
	
	if (self.th_pain)
	{
		self.th_pain (attacker, take);
		// nightmare scaling of .pain_finished moved to PainFinished(), called from .th_pain
		// on appropriate monsters only
	}
	self.customflags = not(self.customflags, CFL_AXEHITME | CFL_ZAPPED);	// clear flags
	self = oldself;
}

/*
============
T_DamageMelee
============
*/
void(entity targ, entity attacker, float damage) T_DamageMelee =
{
	if (targ.classname == "player")
	{
		if (targ.invisible_finished > time)
			damage *= 0.5;
	}
	
	// always do at least a damage
	T_Damage(targ, attacker, attacker, max(1, floor(damage)));
}

/*
============
T_RadiusDamage
============
*/

void(entity inflictor, entity attacker, entity tgt, float points, float multi) T_RadiusDamageCheck =
{
	local float pnts;
	pnts = points;
	if ( !CanDamage ( tgt, inflictor ) )
		return;
	
	// shambler takes half damage from all explosions
	if (tgt.classname == "monster_shambler") pnts *= 0.5;
	
	
	if (multi && tgt.movetype != MOVETYPE_WALK )
		AddMultiDamage( tgt, pnts );
	else T_Damage (tgt, inflictor, attacker, pnts);
}


void(entity inflictor, entity attacker, float damage, entity ignore, float multi) T_RadiusMultiDamage =
{
	float 	points, dist;
	entity	head;
	vector	org;
	
	head = findradius(inflictor.origin, damage+40);
	
	while (head)
	{
		if (head != ignore && head.takedamage)
		{
			org = head.origin + (head.mins + head.maxs)*0.5;
			dist = vlen (inflictor.origin - org);
			points = damage - (0.5 * dist);
			if (head == attacker)
				points = points * 0.5;
			
			// don't set off shootable doors/buttons with splash unless the player means it
			if ((head.movetype == MOVETYPE_PUSH || head.th_die == multi_killed) && 
				head.type != "explobox" && 
				dist > damage / 3)
				points = 0;

			points = max(0, points);	//clamp
			if (points > 0)
				T_RadiusDamageCheck(inflictor, attacker, head, points, multi);
		}
		head = head.chain;
	}
}

void(entity inflictor, entity attacker, float damage, entity ignore) T_RadiusDamage =
{
	T_RadiusMultiDamage(inflictor, attacker, damage, ignore, 0);
}



/*
============
T_BeamDamage
... unused
============
*/
void(entity attacker, float damage) T_BeamDamage =
{
	local	float 	points;
	local	entity	head;
	
	head = findradius(attacker.origin, damage+40);
	
	while (head)
	{
		if (head.takedamage)
		{
			points = 0.5*vlen (attacker.origin - head.origin);
			if (points < 0)
				points = 0;
			points = damage - points;
			if (head == attacker)
				points = points * 0.5;
			if (points > 0)
			{
				if (CanDamage (head, attacker))
				{
					T_Damage (head, attacker, attacker, points);
				}
			}
		}
		head = head.chain;
	}
}


/*
============
T_Heal: add health to an entity, limiting health to max_health if it's set
special rules if the player is being healed:
	"overheal" will limit health to the hard ceiling rather than medkit limit
returns TRUE if healing successfully took place
============
*/
float (entity e, float healamt, float overheal) T_Heal =
{
	float hmax, hceil;
	
	if (e.health <= 0)
		return FALSE;
	
	healamt = ceil(healamt);

	if (e.classname == "player")
	{
		hmax = e.max_health;
		hceil = 250;

		if (overheal)
			healamt = min(healamt, hceil - e.health);
		else
			healamt = min(healamt, hmax - e.health);

		healamt = floor(healamt);	// this should never come in as a float in the first place, buuut
		
		if (healamt <= 0)
			return FALSE;

		if (overheal)
			e.healthtime = time + 5;
		else
			e.healthtime = time + 1;
			
		e.health = e.health + healamt;

		// moved here from healthtouch so any source of healing prints messages
		sprint(e, "You are healed by ");
		sprint(e, ftos(healamt));
		sprint(e, "\n");
		stuffcmd (e, "bf\n");
		return TRUE;
	}

	if (e.max_health >= 0)
		healamt = min(healamt, e.max_health - e.health);	
	
	if (healamt <= 0)
		return FALSE;
	
	e.health = e.health + healamt;

	return TRUE;
}