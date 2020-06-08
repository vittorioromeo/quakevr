/*
================
spawn_touchblood

only used by nailguns so now it's here
================
*/
void(float damage) spawn_touchblood =
{
	local vector	vel;

	vel = normalize(self.velocity) * -1;
	vel = normalize(vel + v_up*(random()- 0.5) + v_right*(random()- 0.5));
	
	//vel = vel + 2*trace_plane_normal;
	// nailguns don't do traces so there is no valid trace plane normal
	// (which explains why nailgun blood spurts always seemed to be caught in some strange wind)
	
	SpawnBlood (self.origin + vel, vel * 40, damage);
}




/*
===============

SPIKY BITS

===============
*/


// --------------------------------


entity (vector muzzle, float speed, float spread) W_FireSpike =
{
	vector dir, org;

	org = self.origin + '0 0 16' + v_forward * 11;	
	org += muzzle_x * v_right + muzzle_z * v_up;

	if (spread)
		dir = normalize(v_forward + (2 * v_right * crandom() + v_up * crandom()) * spread) * speed;
	else
		dir = normalize(v_forward) * speed;
	return launch_nail(org, dir, 0);
}

void() W_FireSpikes =
{
	//makevectors (self.v_angle);
	vector v;
	entity spike1, spike2;
	v = vectoangles( aim( self, AUTOAIM_DIST ) );
	v_x *= -1;	// pitch comes back inverted from vectoangles :|
	makevectors( v );

	if (self.ammo_nails < 1)
	{
		W_SelectBestWeapon();
		return;
	}
	else if (self.ammo_nails >= 2 && self.weapon == IT_SUPER_NAILGUN)
	{
		sound (self, CHAN_WEAPON, "weapons/spike2.wav", 1, ATTN_NORM);
		self.currentammo = self.ammo_nails = self.ammo_nails - 2;
		
		if (mod(self.cnt,4) == 0) v = '0 0 1.25';
		else if (mod(self.cnt,4) == 1) v = '1 0 1';
		else if (mod(self.cnt,4) == 2) v = '1.25 0 0';
		else if (mod(self.cnt,4) == 3) v = '1 0 -1';
		spike1 = W_FireSpike(v, 950, 0.03);
		spike2 = W_FireSpike(-v, 1050, 0.03);
		self.attack_finished = time + 0.09;
		if (self.super_damage_finished > time)
		{
			// here's a silly thing:
			// one vanilla SNG nail does 18 dmg, 72 with quad, enough to gib zombies and grunts
			// two copper nails do 9 each, 36 each with quad, for the same total damage, but that
			//   only kills grunts and knocks zombies down because the second nail doesn't hit
			
			// the dumb solution is to make one nail do 0 dmg and the other do 18 in quad mode
			//   so the math works out like the old math and the quad SNG still feels right
			spike2.dmg = 18;
			spike1.dmg = 18;
			spike2.buddy = spike1;
			spike1.buddy = spike2;
			// first one to hit does its damage and then nerfs its 'buddy'
		}
	}
	else
	{
		sound (self, CHAN_WEAPON, "weapons/rocket1i.wav", 1, ATTN_NORM);
		self.currentammo = self.ammo_nails = self.ammo_nails - 1;
		v = VEC_ORIGIN;
		if ( !mod( self.cnt, 2 ) ) v_x = 2.5;
		else v_x = -2.5;	
		W_FireSpike(v, 1996, 0);	// :)
		// default sv_maxvelocity is 2000, any higher leads to weird 
		// angular offsets caused by clamping the velocity
		self.attack_finished = time + 0.09;
	}
	self.punchangle_x = -2;
	
	self.cnt = cycle(self.cnt,8);
}

float() CheckValidSpikeTouch =
{
	if (other == self.owner) {dprint("other==self.owner\n");return 0;}
	if (other.solid == SOLID_TRIGGER) {dprint("other==trig\n");return 0;}
	if (CheckProjectilePassthru()) {dprint("passthru true\n");return 0;}

	if (pointcontents(self.origin) == CONTENT_SKY)
	{
		remove(self);
		return 0;
	}
	
	return 1;
}


void() spike_touch =
{
	if (!CheckValidSpikeTouch()) return;
	
	if (self.dmg)
	{
		if (other.takedamage)
		{
			spawn_touchblood (self.dmg);
			T_Damage (other, self, self.trueowner, self.dmg);
		}
		else
		{
			WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
			
			switch(self.classname)
			{
			case "wizspike":
				WriteByte (MSG_BROADCAST, TE_WIZSPIKE); break;
			case "knightspike":
				WriteByte (MSG_BROADCAST, TE_KNIGHTSPIKE); break;
			case "laser":
				WriteByte(MSG_BROADCAST, TE_GUNSHOT); break;
			default:
				WriteByte (MSG_BROADCAST, TE_SPIKE);
			}
			WriteCoord (MSG_BROADCAST, self.origin_x);
			WriteCoord (MSG_BROADCAST, self.origin_y);
			WriteCoord (MSG_BROADCAST, self.origin_z);
		}
		
		if (self.classname == "laser")
		{
			sound (self, CHAN_WEAPON, "enforcer/enfstop.wav", 1, ATTN_STATIC);
			particle (self.origin, self.velocity*-0.01, 235, 15);
		}
	}
	// nonsense for combining quad SNG spike damage
	if (self.buddy)
		self.buddy.dmg = 0;
	remove(self);
}


