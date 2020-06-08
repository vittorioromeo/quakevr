/*
==============================================================================

MONSTERS

All things spawning and initializing

==============================================================================
*/

/*FGD
@baseclass base(Angle, Appearflags, Target, Targetname, Deathtype) color(220 0 0) = Monster 
[
	spawnflags(Flags) = 
	[
		1 : "Ambush" : 0
		16 : "Triggered Spawn" : 0
		32 : "Teleflash on Spawn" : 0
		64 : "No Telefrag on Spawn" : 0
		128 : "Spawn Angry" : 0
	]
	movedir(string) : "Auto jump velocity on awaken"
	count(integer) : "Number of times to spawn if triggered (-1 for infinite)"
]
*/

// at map start, anything that is a monster calls this and adds itself to various
// balance debugging totals, to take some of the guesswork out of ammo placement
void() TallyHitpoints =
{
	//dprint5("tally hitpoints for ",self.classname," adding ",ftos(self.health),"\n");
	if (self.classname == "monster_boss" || self.classname == "monster_oldone")
		return;
	
	local float num;
	num = 1;
	if (self.spawnflags & SPAWN_TRIGGERED)
	{
		if (self.count > 1)
			num = self.count;
	}
	
	if (self.type == "zombie")
	{
		if ( !( self.spawnflags & SPAWN_CRUCIFIED ) )
			num_zombies += num;
	}
	else
		monster_health += self.health * num;
	
	if (self.classname == "monster_army")
		ammo_total += 5 * DEBUG_DMG_PER_SHELL;
	//if (self.classname == "monster_enforcer")
	//	ammo_total += 5 * DEBUG_DMG_PER_CELL;
	if (self.classname == "monster_ogre")
		num_rockets += 1;
}

/*
================================================================

SPAWNING

all monster_* entities can be used as triggerable monster teleport spawns,
which is way better than building a big teleport setup in a hidden room
for each one.

setting 'count' on any monster allows the same entity to be triggered more than 
once to spawn monsters repeatedly. 'count' monsters are added to the kill total 
when the map starts.
setting count to a negative number makes the spawner unlimited use, and monsters
are added to the kill total as they're spawned. 

FIXME: their bodies and head gibs WILL stick around forever

================================================================
*/



/*
================
monster_spawnsetup_go

sanity check for trigger-spawned monsters with no existing targeters
fix by iw
================
*/
void() monster_spawnsetup_go =
{
	self.think = SUB_Null;  // just to tidy up after ourselves

	if (self.targetname == string_null)
	{
		dprint5("WARNING: removed triggered-spawn ", self.classname,
			" with no targetname at ", vtos(self.origin), "\n");
		remove(self);
		return;
	}

	if (!SUB_VerifyTriggerable())
	{
		dprint4("WARNING: removed triggered-spawn ", self.classname,
			" with unmatched targetname '", self.targetname);
		dprint3("' at ", vtos(self.origin), "\n");
		remove(self);
		return;
	}

	self.customflags |= CFL_SPAWNER;
	self.use = self.think1;
	self.think1 = SUB_Null;  // just to tidy up after ourselves

	if (!self.count)
		self.count = 1;
	if (self.count < 0)
		self.count = 0;
	addmonster(self.count);
};

/*
================
monster_spawnsetup

Checks if this monster is in some way a spawner of monsters, and returns
TRUE if so, after scheduling appropriate setup. A return of TRUE should
signal the calling spawnfunc that the monster shouldn't continue to be
spawned right now.

'spawnthink' is the function that should be used as the triggering action
if this monster is a spawner.
================
*/
float( void() spawnthink ) monster_spawnsetup =
{
	TallyHitpoints();
	
	if (!(self.spawnflags & SPAWN_TRIGGERED))
		return FALSE;

	// wait for all potential targeters to finish spawning
	self.think1 = spawnthink;
	self.think = monster_spawnsetup_go;
	self.nextthink = time + 0.1;
	
	return TRUE;
}

void(float num) addmonster =
{
	if (!self.th_checkattack)
		self.th_checkattack = CheckAttack;
	
	if (!num) return;
	
	total_monsters = total_monsters + num;
	WriteByte (MSG_ALL, SVC_UPDATESTAT);
	WriteByte (MSG_ALL, STAT_TOTALMONSTERS);
	WriteLong (MSG_ALL, total_monsters);
}

float(entity me) monster_check_enemy_activator =
{
	if (me.enemy)
		return FALSE;
	if (activator.health <= 0)
		return FALSE;
	// monsters spawning asleep seemed like a neat consequence for the ring of shadows,
	// but it neuters big ambushes a little too hard
	//if (activator.items & IT_INVISIBILITY)
	//	return FALSE;
	if (activator.flags & FL_NOTARGET)
		return FALSE;
	if (activator.movetype == MOVETYPE_NOCLIP)
		return FALSE;
	if (activator.classname != "player")
		return FALSE;
	return TRUE;
}


entity() find_touching =
{
	entity head = findradius(BoundsCenter(self), self.size_z);
	while (head != world)
	{
		if (head.health <= 0 || 
			(head.solid == SOLID_NOT || head.solid == SOLID_TRIGGER) ||
			!BoundsTouching(self.absmin, self.absmax, head.absmin, head.absmax, 2) )
		{
			head = head.chain;
		}
		else return head;
	}
	return world;
}


entity( void() spawnthink ) mon_spawner_use =
{
	if ( (self.spawnflags & SPAWN_NO_TFRAG) )
	{
		if (find_touching())
		{
			return world;
		}
	}

	local entity mon;
	mon = spawn();
	setorigin(mon, self.origin);
	mon.angles = self.angles;
	mon.nextthink = time + 0.01 + random()*0.05;
	mon.spawnflags = self.spawnflags;
	mon.movedir = self.movedir;
	mon.lip = self.lip;
	mon.state = self.state;
	mon.health = self.health;
	mon.think = spawnthink;
	SUB_CopyTargets(mon);

	// set .enemy before monster_start thinks, because 'activator' will be different by then
	if ( (self.spawnflags & SPAWN_INSTAWAKE) && monster_check_enemy_activator(mon) )
	{
		mon.enemy = activator;
	}
	
	if (!(self.spawnflags & SPAWN_NO_TFOG))
	{
		teleport_flash();
	}

	if (self.count > 0)
	{
		self.count = self.count - 1;
		if (self.count == 0)
		{
			SUB_Remove();
		}
	}
	else 
	{
		addmonster(1); // add them one at a time if this is an unlimited spawner
	}
	
	return mon;
}

void(entity m) monster_kill =
{
	T_Damage(m, self, self, m.health + 100);
}

/*
================
monster_use

Using a monster makes it angry at the current activator, because
nobody likes to be used
================
*/
void() monster_use =
{
	if (!monster_check_enemy_activator(self)) return;
	
	// delay reaction so the monster is still heard if it's teleported
	self.enemy = activator;
	self.nextthink = time + 0.1;
	self.think = FoundTarget;

}


/*
================
monster_death_use

When a monster dies, it fires all of its targets with the current
enemy as activator.
================
*/
void() monster_death_use =
{
	// fall to ground
	if (self.flags & FL_FLY)
		self.flags = self.flags - FL_FLY;
	if (self.flags & FL_SWIM)
		self.flags = self.flags - FL_SWIM;

	if (!self.target && !self.target2 && !self.target3 && !self.target4 && !self.killtarget)
		return;

	// try to make player activator even if the monster was killed while infighting
	if (self.enemy.classname != "player" && self.oldenemy.classname == "player")
		activator = self.oldenemy;
	else
		activator = self.enemy;
	
	SUB_UseTargets ();
}


//============================================================================

/*
================
Common monster starts
================
*/

void() monster_start =
{
	// moved from mon_spawner_use because it was too early to catch the monster's mins and maxs
	spawn_tdeath(self.origin, self);

	// wait a few frames to make sure doors have spawned before doing droptofloor
	self.nextthink = 1.15;
	// why 1.15 is a magic number:
	// - quake starts 'time' at 1.0 seconds.
	// - doors and movers and stuff reposition themselves at 1.1 seconds.
	// - items drop themselves into place at 1.2 seconds, after movers, so that they fall on them 
	//    in their final positions.
	// - we want monsters to do this too, but before items do, because they were always before
	//    items originally and altering the drop order too much could cause items that didn't
	//    fall out of the world before to do so erroneously in Copper.
	// - we don't use 'time + 0.15' because this could be running on a late spawn. a constant
	//    1.15 will always be when we want at map start but in the past upon late spawn.

	if (self.type == string_null)
		self.type = self.classname;
	
}

void() monster_start_go =
{
	local entity pathgoal;
	self.takedamage = DAMAGE_AIM;

	self.ideal_yaw = self.angles * '0 1 0';
	self.use = monster_use;
	self.flags = self.flags | FL_MONSTER;
	
	// check if the mapper placed them touching a killer liquid and grant them immunity to it
	vector wtt;
	wtt = WaterTest();
	if (wtt_x)
	{
		if (wtt_y == CONTENT_LAVA || wtt_y == CONTENT_SLIME)
		{
			self.customflags |= CFL_LIQUID_IMMUNE;
		}
	}
	// ai_nav
	//self.radius = (self.maxs_x - self.mins_x) * 0.7;

	if (self.enemy)
	{
		self.th_stand ();
		self.think = FoundTarget;
	}
	else if (self.target)
	{
		pathgoal = find(world, targetname, self.target);
		if (!pathgoal)
		{
			dprint (self.classname);
			dprint (" at ");
			dprint (vtos(self.origin));
			dprint (" can't find target\n");
		}
		
		if (pathgoal.classname == "path_corner")
		{
			self.goalentity = pathgoal;
			self.ideal_yaw = vectoyaw(self.goalentity.origin - self.origin);
			self.th_walk ();
		}
		else
		{
			self.pausetime = time + A_SHITLOAD;
			self.th_stand ();
		}
	}
	else
	{
		self.pausetime = time + A_SHITLOAD;
		self.th_stand ();
	}

	// spread think times so they don't all happen at same time
	// add tiny minimum in case random() ever returns exactly 0 (rare but possible)
	self.nextthink = time + random()*0.1 + 0.01;

}

/*
================
Walkmonster starts
================
*/

void() walkmonster_start_go =
{
	if (self.movedir == '0 0 0' && !(self.spawnflags & SPAWN_TRIGGERED) )	// always put telespawned monsters where the mapper wants
	{
		self.origin_z = self.origin_z + 1;	// raise off floor a bit
		droptofloor(0,0);
	}
	
	if (!walkmove(0,0))
	{
		dprint ("walkmonster in wall at: ");
		dprint (vtos(self.origin));
		dprint ("\n");
	}
	
	if (!self.yaw_speed)
		self.yaw_speed = 20;
	self.view_ofs = '0 0 25';
	
	
	monster_start_go();
}


void() walkmonster_start =
{
	monster_start();
	self.think = walkmonster_start_go;
}

/*
================
Flymonster starts
================
*/

void() flymonster_start_go =
{
	if (!self.yaw_speed)
		self.yaw_speed = 10;
	self.view_ofs = '0 0 25';

	if (!walkmove(0,0))
	{
		dprint ("flymonster in wall at: ");
		dprint (vtos(self.origin));
		dprint ("\n");
	}

	
	monster_start_go();
}

void() flymonster_start =
{
	monster_start();
	// iw -- move these here so fish don't fall for a frame and a half
	self.flags = self.flags | FL_FLY;
	self.movetype = MOVETYPE_FLY;

	self.think = flymonster_start_go;
}

/*
================
Swimmonster starts
================
*/

void() swimmonster_start_go =
{
	if (!self.yaw_speed)
		self.yaw_speed = 20;//10
	self.view_ofs = '0 0 10';
	
	// FIXME: why don't fish do a walkmove test?
	monster_start_go();
	// yes besides the fact that they don't have feet dickhead
}

void() swimmonster_start =
{
	monster_start();
	// iw -- move these here so fish don't fall for a frame and a half
	self.flags = self.flags | FL_SWIM;
	self.movetype = MOVETYPE_FLY;

	self.think = swimmonster_start_go;
}


// -