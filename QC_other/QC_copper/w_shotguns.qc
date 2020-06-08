/*
===============

BULLETS

sg:		6 * 4 = 24		/ 48 / 72 / 96 / 120
ssg:	14 * 4 = 56		/ 112 / 168 / 224 / 280

sg:		5 * 5 = 25		/ 50 / 75 / 100 / 125
ssg:	11 * 5 = 55		/ 110 / 165 / 220 / 275
ssg:	12 * 5 = 60		/ 120 / 180 / 240 / 300

===============
*/


/*
================
BulletImpact
Go to the trouble of combining multiple pellets into a single damage call.
================
*/
void(float damage, vector dir) BulletImpact =
{
	vector vel, org;
	
	vel = normalize(dir + v_up*crandom() + v_right*crandom());
	vel = vel + 2*trace_plane_normal;

	org = trace_endpos - dir*4;

	if (trace_ent.takedamage)
	{
		SpawnBlood (org, vel*40, damage);
		AddMultiDamage (trace_ent, damage);
	}
	else gunshot(org);
}

float(entity t) HasBeenShotToDeath =
{
	if (t.multi_dmg < t.health) return FALSE;
	if (t.multi_dmg - ArmorSave(t, t.multi_dmg) >= t.health) return TRUE;
	return FALSE;
}

/*
================
FireBullets

Used by shotgun, super shotgun, and enemy soldier firing
================
*/
void(float shotcount, vector dir, vector spread) FireBullets =
{
	vector src, direction;
	entity ig;
	float s, lim;
	
	if (shotcount <= 0) return;
	
	makevectors(self.v_angle);

	if (trace_debug) dprint3("*** firing ", ftos(shotcount), " bullets *\n\n");
	for (s = 0; s < shotcount; s++)
	{
		src = self.origin + v_forward*10;
		src_z = self.absmin_z + self.size_z * 0.7;
		direction = SpreadVector(dir, spread);
		
		if (trace_debug) dprint("* firing a bullet *\n");
		
		lim = 0;
		// check for trace-through cases
		for (ig = self; ig != world; lim++)	// ig == world if we hit a wall or hit nothing
		{
			traceline2(src, src + direction*2048, ig, 0);
			
			// if we've already pelted a monster enough to kill it, let additional bullets add 
			// up for possible gibs (ala quake), but trace them again through the target for 
			// better crowd control feels (ala doom).
			// this is only noticeable with big hordes of weak guys, but it makes big hordes of
			// weak guys not feel like solid ammo-sponge walls.
			if (trace_ent.takedamage && 
				(trace_ent.flags & FL_MONSTER || trace_ent.classname == "player" ) && 
				// some id shootables are actually trigger_multiples with health and SOLID_BBOX, so this doesn't work (see e1m6):
				//( trace_ent.solid == SOLID_BBOX || trace_ent.solid == SOLID_SLIDEBOX ) &&	
				HasBeenShotToDeath(trace_ent) )
			{
				BulletImpact (2+2*random(), direction);

				// start 0.1 unit forward, or else when shooting an adjacent enemy we'll
				// just hit ourselves again
				src = trace_endpos + direction * 0.1;
				ig = trace_ent;
				if (trace_debug) dprint3("bullet hit ", ig.classname, ", continuing thru\n");
			}
			else
			{
				if (trace_debug) dprint("bullet reached world\n");
				ig = world;
			}
			if (lim > 8)
			{
				dprint("bullet pen limit exceeded\n");
				break;
			}
		}
		
		if (trace_fraction != 1.0)
		{
			BulletImpact (4, direction);
		}
		
		if (trace_debug) dprint("bullet is over\n\n");
	}
}

/*
================
W_FireShotgun
================
*/
void() W_FireShotgun =
{
	local vector dir;
	
	// for ai_nav:
	/*if (nav_w_nodeLink())
	{
		self.attack_finished = time + 0.5;
		return;
	}*/
	
	sound (self, CHAN_WEAPON, "weapons/guncock.wav", 1, ATTN_NORM);	

	self.punchangle_x = -2;
	makevectors(self.v_angle);
	
	self.currentammo = self.ammo_shells = self.ammo_shells - 1;
	dir = aim (self, AUTOAIM_DIST);

	ClearMultiDamage ();
	FireBullets (6, dir, '0.04 0.04 0');
	self.attack_finished = time + 0.5;
	
	ApplyMultiDamage ();
}


/*
================
W_FireSuperShotgun
================
*/
void() W_FireSuperShotgun =
{
	local vector dir;
	
	// for ai_nav:
	/*if (nav_w_nodeLinkSpecial())
	{
		self.attack_finished = time + 0.5;
		return;
	}*/

	if (self.currentammo == 1)
	{
		W_FireShotgun ();
		return;
	}
		
	sound (self ,CHAN_WEAPON, "weapons/shotgn2.wav", 1, ATTN_NORM);	

	self.punchangle_x = -4;
	
	self.currentammo = self.ammo_shells = self.ammo_shells - 2;
	dir = aim (self, AUTOAIM_DIST);

	ClearMultiDamage ();

	FireBullets (14, dir, '0.14 0.08 0');
	self.attack_finished = time + 0.75;
	
	ApplyMultiDamage ();

}
