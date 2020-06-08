/*
================================================================

	PROJECTILES

================================================================
*/


/*
============
traceline2

a more robust wrapper for traceline() that can ignore a wider array of entities.
performs one or more staggered traceline()s until the trace_ent isn't something we 
actually want to ignore, so what would be one trace can turn into several.
============
*/
void(vector start, vector end, entity ignore, float traceflags) traceline2 =
{
	entity ig = ignore;
	//entity lastent = world;
	vector src = start;
	vector lastend = start;
	float traces = 0;
	float nomonsters = 0;
	
	if (traceflags & TRACE_NOMONSTERS) nomonsters = TRUE;
	
	float	t_allsolid = 2;
	float	t_startsolid = -1;
	float	t_inopen = -1;
	float	t_inwater = -1;

	if (trace_debug) dprint5("tracing from ", vtos(src), " to ", vtos(end), "\n");

	// check for trace-through cases and repeat
	do {
		if (trace_debug) dprint5("  trace: ", vtos(src), " to ", vtos(end), "\n");
		traceline (src, end, nomonsters, ig);
		
		// preserve the accuracy of the trace result flags
		if (t_startsolid == -1) t_startsolid = trace_startsolid;
		if (!trace_startsolid)
		{		
			t_allsolid = min(t_allsolid, trace_allsolid);
			t_inopen = max(t_inopen, trace_inopen);
			t_inwater = max(t_inwater, trace_inwater);
			
			if ( trace_ent.deadflag == DEAD_DYING || 	// don't let guys in death frames who aren't nonsolid yet act like solid walls to bullets
				trace_ent == ignore || 					// always ignore the original ignore ent on future loops
				trace_ent.movetype == MOVETYPE_NOCLIP || // don't shoot noclipping players
				trace_ent.notrace)						// flag to make things game-functionally nonsolid (monsterclip/etc)
			{
				// if two entities that are both to be ignored are touching, the trace will
				// fail with a distance of zero once 'ig' changes, so nudge forward one unit
				// to make sure the point of origin isn't touching the old ig
				lastend = trace_endpos;
				src = trace_endpos + normalize(end - start);
				ig = trace_ent;
				if (trace_debug) dprint3("    hit ", trace_ent.classname, ", trying again\n");
			}
			else ig = world;
		}
		else
		{
			// if the nudge forward put the trace start in a solid, the last trace should 
			// have been the end. this can happen when a notrace surface and a legit solid 
			// surface are coincident in the path of the trace2, and the notrace one is the 
			// one quake 'sees' first.
			if (trace_debug) dprint("  trace started solid!\n");
			trace_endpos = lastend;
			ig = world;
		}
		traces++;
	} while (ig != world);	// true if we hit a wall or hit nothing
	
	if (trace_debug) dprint3("trace complete after ", ftos(traces), " traces \n");
	
	// unify global trace results so the calling function can pretend it only made one trace
	float lastfrac = vlen(start - trace_endpos) / vlen(start - end);
	entity lastent = trace_ent;
	if (traceflags & (TRACE_NOMONSTERS|TRACE_WATER) == TRACE_WATER)
	{
		// thanks to a structural oddity in SV_ClipToLinks (world.c), a traceline() without nomonsters
		// doesn't properly populate the open/inwater trace flags, so we have to do another nomonsters
		// traceline2 over the path of the last one to catch the case of crossing water contents.
		
		// we can't just check pointcontents at start and end because it doesn't catch when mappers do
		// wacky things with water volumes (ie the E4 entry in the original start.bsp), and it can't be
		// a singular traceline() because a monster might be looking through a notrace bmodel into water.
		//dprint(" tracing again with nomonsters to get trace flags right\n");
		traceline2(start, trace_endpos, ignore, TRACE_NOMONSTERS);
	}
	else
	{
		trace_startsolid = t_startsolid;
		trace_allsolid = t_allsolid;
		trace_inopen = t_inopen;
		trace_inwater = t_inwater;
	}
	trace_fraction = lastfrac;
	trace_ent = lastent;
	
	if (trace_debug) {
		if (trace_ent)
			dprint3("  entity: ", trace_ent.classname, "\n");
		dprint3("  end: ", vtos(trace_endpos), "\n");
		dprint3("  fraction: ", ftos(trace_fraction*100), "%\n");
		dprint3("  startsolid: ", ftos(trace_startsolid), "\n");
		dprint3("  allsolid: ", ftos(trace_allsolid), "\n");
		dprint3("  inopen: ", ftos(trace_inopen), "\n");
		dprint("----------\n");
	//	dprint3("  inwater: ", ftos(trace_inwater), "\n----------\n");
	}
}

/*
================================================================

PENETRATION AND PASSTHRU

we "patch" the limited options for solid/nonsolid interaction in the quake physics by
overriding the results of said physics in the progs .touch() functions that are called
immediately after.

to emulate objects being nonsolid to each other when quake didn't want them to be,
we respond to their collisions by setting one as the .owner of the other, which causes
quake to ignore their collisions on subsequent frames, then restoring the physics to 
roughly the state they'd be in if SV_Move had not altered their velocities in response to 
that collision. in the case of linear projectiles, this means restoring their starting
velocity, but for TOSS/BOUNCE projectiles it means tracking their always-changing 
velocities constantly with a frequent .think(). thus all grenades, as well as gibs, 
think at 20hz until they come to rest. not ideal, but not seriously so.

this is further complicated by not being able to alter .velocity from the touch function
itself, because at that point quake is in the physics half of the game frame when such
monkeying is not allowed. thus, we have to make our corrections immediately thereafter
in a .think(), while preserving the previous .think() and .nextthink so as not to break 
the projectile's normal behavior (voreball homing, particles, etc).

on all projectiles, .touch must be some function that calls CheckProjectilePassthru() and 
halts if it returns TRUE (representing "not an actual collision"). the check handles all 
consequences behind the scenes.

on ballistic projectiles, .think and .nextthink are overridden, so they cannot be used
for projectile timeout. set .lifetime_finished to the time when projectile should expire, 
and .th_die to the function to call when it does.

================================================================
*/


/*
============
projectile_passthru_think

think function to make a projectile ignore what it just hit
the new owner has been set by this point
============
*/
void() projectile_passthru_think =
{
	self.flags = not(self.flags, FL_ONGROUND);
	self.velocity = self.oldvelocity;
	self.think = self.think1;
	self.nextthink = self.pain_finished;
	
	// nudge the projectile forward a tiny bit to be inside its new owner
	setorigin(self, self.origin + normalize(self.velocity) * 0.1);
	// if the old owner and the new owner are touching at the point that the
	// projectile crosses over, next frame it'll be coincident with both old
	// and new, the trace in sv_move will wind up being allsolid, and the
	// projectile will travel through worldspawn.
}

/*
============
projectile_toss_think

when a passthru overrides server physics for a TOSS projectile, we must 
restore a recent enough velocity that the projectile appeared not to touch.
grenades change velocity every frame, so they have to save it near-continuously
============
*/
void() projectile_toss_think =
{
	if (time > self.lifetime_finished)
	{
		self.th_die();
		return;
	}
	if (self.velocity == VEC_ORIGIN)	// stopped on the ground
	{
		self.think = self.th_die;
		self.nextthink = self.lifetime_finished;
	//	bprint3("at rest, exploding in ", ftos(self.nextthink), " seconds\n");
		self.th_die = SUB_Null;
		self.lifetime_finished = 0;
		self.solid = SOLID_NOT;
		return;
	}
		
	self.flags = not(self.flags, FL_ONGROUND);
	self.oldvelocity = self.velocity;
	self.nextthink = time + 0.05;	// 10hz is probably enough, but 20hz is surely twice as good
}

/*
============
projectile_valid_passthru

returns TRUE in cases where we pass through and should not collide
============
*/
float() projectile_valid_passthru =
{
	// SOLID_NOT entities touch SOLID_BSP by default without generating touch events.
	// gibs had to be made SOLID_BBOX in order to make selectively passing through 
	// notrace/monsterclip even possible, so they need special handling by our owner-
	// changing hackery to ensure they stay nonsolid in the other cases too
	if (self.type == "gib")
	{
		if (other.movetype == MOVETYPE_PUSH || other.solid == SOLID_BSP)
			return other.notrace;
		return TRUE;
	}
	
	if (other.deadflag == DEAD_DYING || // corpses not yet on the ground
		other.notrace  || 				// specially marked (monsterclip)
		other.movetype == MOVETYPE_NOCLIP || // ignore noclipping players
		other == self.trueowner	)		// still track the original owner - there are cases when monsters 
										// or players firing fat FLYMISSILE projectiles into an adjacent 
										// monsterclip can be hit by them when the owner switches
		return TRUE;
		
	return FALSE;
}

/*
============
projectile_passthru

we can't alter physics stuff to make the projectile physically penetrate during 
touch/physics, so set an immediate think to 'repair' the state of the projectile
============
*/
void() projectile_passthru =
{
	self.owner = other;
	
	// our nextthink was probably to explode or remove, so save that
	self.think1 = self.think;	
	self.pain_finished = self.nextthink;
	
	self.think = projectile_passthru_think;
	self.nextthink = time;
}

/*
============
CheckProjectilePassthru

called within every projectile touch function, returns TRUE if pierced an otherwise solid entity
============
*/
float() CheckProjectilePassthru =
{
	if (!other) return FALSE;	// always collide with world
	if (other == self.owner) return FALSE;	// this actually shouldn't have been called, but: don't touch owner
	
	if (other.classname == "func_void")
	{
		// go away at the bottoms of pits
		SUB_RemoveSoon();
		return TRUE;
	}
	
	// handle overriding the quake physics with our own desired result
	if (projectile_valid_passthru())
	{
		projectile_passthru();
		return TRUE;
	}

	return FALSE;
}

void() projectile_touch_null = { CheckProjectilePassthru(); }




/*
================================================================

	FIRE ZE MISSILES

if you're looking for 'newmis' and 'launch_spike' this is where they went

================================================================
*/

/*
========
launch_projectile

base linear projectile, does not set model, touch, or damage
speed expected to derive from the length of 'dir'
========
*/
entity(vector org, vector dir) launch_projectile =
{
	entity prj;
	
	prj = spawn();
	prj.owner = prj.trueowner = self;
	prj.movetype = MOVETYPE_FLYMISSILE;
	prj.solid = SOLID_BBOX;

	prj.angles = vectoangles(dir);
	
	prj.classname = "spike";
	prj.touch = projectile_touch_null;
	prj.th_die = SUB_Remove;
	prj.think = SUB_Remove;
	prj.nextthink = time + 6;

	setsize (prj, VEC_ORIGIN, VEC_ORIGIN);
	setorigin (prj, org);

	prj.velocity = prj.oldvelocity = dir;
	
	return prj;
}

/*
========
toss_projectile

base ballistic projectile, does not set model, touch, or damage
speed expected to derive from the length of 'dir'
========
*/
entity(vector org, vector dir) toss_projectile =
{
	entity g;
	g = launch_projectile(org, dir);
	g.movetype = MOVETYPE_BOUNCE;
	g.classname = "grenade";
	
	g.think = projectile_toss_think;
	g.nextthink = time;
	g.touch = projectile_touch_null;
	g.th_die = SUB_Remove;
	g.lifetime_finished = time + 6;
	
	return g;
}


//================================================================


/*
========
launch_nail

basic hurty projectile, used by trap_spikeshooter and player nailguns
inherited from by lots of other hurty projectiles
========
*/
entity(vector org, vector dir, float superness) launch_nail =
{
	entity spike;
	
	spike = launch_projectile(org, dir);
	spike.touch = spike_touch;
	
	SUB_ChangeModel (spike, "progs/spike.mdl");	
	
	if(superness)
	{
		spike.classname = "superspike";
		spike.dmg = 18;
		spike.skin = 1;
	}
	else
	{
		spike.dmg = 9;
		spike.skin = 0;
	}
	
	return spike;
}

/*
========
launch_knightspike
hellknight spreadfire
========
*/
entity(vector org, vector vel) launch_knightspike =
{
	entity spike;
	
	spike = launch_projectile(org, vel);	// FLYMISSILE
	spike.touch = spike_touch;
	spike.classname = "knightspike";
	SUB_ChangeModel (spike, "progs/k_spike.mdl");
	
	spike.dmg = 8;
	
	return spike;
}


/*
========
launch_wizspike
scrag spit, used by scrags and nouronihar
========
*/
entity(vector org, vector vel) launch_wizspike =
{
	entity spike;
	
	spike = launch_projectile(org, vel);	// FLYMISSILE
	spike.touch = spike_touch;
	spike.dmg = 9;
	spike.classname = "wizspike";
	SUB_ChangeModel (spike, "progs/w_spike.mdl");
	
	return spike;	
}

/*
========
launch_laser
used by trapshooter and enforcers
========
*/
entity(vector org, vector vel) launch_laser =
{
	entity spike;
	
	spike = launch_projectile(org, vel);
	spike.touch = spike_touch;
	spike.effects = EF_DIMLIGHT;
	spike.movetype = MOVETYPE_FLY;	// enforcer lasers aren't FLYMISSILE in id106

	SUB_ChangeModel (spike, "progs/laser.mdl");

	spike.dmg = 12;
	spike.classname = "laser";
	
	return spike;
}


/*
========
launch_rocket
basic missile, used by player and ogre rocket fire
========
*/
entity(vector org, vector vel) launch_rocket =
{
	entity missile;
	
	missile = launch_projectile(org, vel);	// FLYMISSILE
	missile.classname = "missile";
	missile.dmg = 100;
	missile.touch = T_MissileTouch;
	
	SUB_ChangeModel (missile, "progs/missile.mdl");
	
	return missile;
}

/*
========
launch_lavamissile
exploding cthon lavaball
========
*/
entity(vector org, vector vel) launch_lavamissile =
{
	entity missile;
	
	missile = launch_rocket(org, vel);	// FLYMISSILE
	missile.avelocity = '200 200 200';
	SUB_ChangeModel (missile, "progs/lavaball.mdl");
	
	return missile;
}

/*
========
launch_grenade
basic grenade, used by player and ogre
========
*/
entity(vector org, vector vel) launch_grenade =
{
	entity grenade;
	
	grenade = toss_projectile(org, vel);
	grenade.lifetime_finished = time + 2.5;
	grenade.avelocity = '300 300 300';
	SUB_ChangeModel (grenade, "progs/grenade.mdl");
	
	return grenade;
}


