
/*
===============

LIGHTNING

===============
*/

/*
=================
LightningDamage
=================
*/

void(entity from, float damage, float multi) LightningMultiDamage_Do =
{
	//if (trace_ent.classname == "monster_enforcer")
	//	trace_ent.armorvalue = 0;
	particle (trace_endpos, '0 0 100', 225, damage*4);
	trace_ent.customflags |= CFL_ZAPPED;
	if (multi) AddMultiDamage (trace_ent, damage);
	else T_Damage (trace_ent, from, from, damage);
	if (self.classname == "player")
	{
		if (trace_ent.classname == "player")
			trace_ent.velocity_z += 400;
	}
}

void(vector p1, vector p2, entity from, float damage, float multi) LightningMultiDamage =
{
	local entity		e1, e2;
	local vector		f;
	
	f = p2 - p1;
	normalize (f);
	f_x = 0 - f_y;
	f_y = f_x;
	f_z = 0;
	f = f*16;

	e1 = e2 = world;

	traceline2(p1, p2, self, 0);
	if (trace_ent.takedamage)
	{
		LightningMultiDamage_Do(from, damage, multi);
	}
	e1 = trace_ent;

	traceline2(p1 + f, p2 + f, self, 0);
	if (trace_ent != e1 && trace_ent.takedamage)
	{
		LightningMultiDamage_Do(from, damage, multi);
	}
	e2 = trace_ent;

	traceline2(p1 - f, p2 - f, self, 0);
	if (trace_ent != e1 && trace_ent != e2 && trace_ent.takedamage)
	{
		LightningMultiDamage_Do(from, damage, multi);
	}
}

void(vector p1, vector p2, entity from, float damage) LightningDamage =
{
	LightningMultiDamage( p1, p2, from, damage, 0);
}

void(entity source, entity from, float iter) W_ChainLightning =
{
	local entity	head;
	local entity	targ1, targ2;
	local float		len1, len2, dist, kill;
	local vector	org, dorg;
	
	if (iter == 5) {
		// let's crash quake yaay
		T_Damage (source, from, from, 60);
		return;
	}
	kill = 25 + 5*iter;	// increase damage per iteration
		// I was reducing it per iteration, but it wasn't as much
		// fun when your primary target died before everything else did
		// and you had to find another target. this further amps the 
		// usefulness in crowd situations.
	iter = iter+1;
	
	org = source.origin + (source.mins + source.maxs)*0.5;
	head = findradius(source.origin+(source.mins + source.maxs)*0.5, 192);
	
	targ1 = targ2 = source;
	len1 = len2 = A_SHITLOAD;
	
	// find the two closest enemies who haven't been zapped yet
	while (head)
	{
		if (head != from && 
			head.takedamage && 
			!(head.customflags & CFL_ZAPPED) && 
			(head.movetype > 2 && head.movetype < 6))	// walk/step/fly
		{
			traceline2(org, head.origin + (head.mins + head.maxs)*0.5, self, TRACE_NOMONSTERS);
			if (trace_fraction == 1.0)
			{	
				dist = vlen(head.origin - source.origin);
				if (dist <= len1) {
					targ2 = targ1;
					len2 = len1;
					targ1 = head;
					len1 = dist;
				} else if (dist <= len2) {
					targ2 = head;
					len2 = dist;
				} 
			}
		}
		head = head.chain;
	}
	
	if (targ1 != source) {
		targ1.customflags = targ1.customflags | CFL_ZAPPED;
		dorg = targ1.origin + (targ1.mins + targ1.maxs)*0.5;
		
		WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
		WriteByte (MSG_BROADCAST, TE_LIGHTNING2);
		WriteEntity (MSG_BROADCAST, targ1);
		WriteCoord (MSG_BROADCAST, org_x);
		WriteCoord (MSG_BROADCAST, org_y);
		WriteCoord (MSG_BROADCAST, org_z);
		WriteCoord (MSG_BROADCAST, dorg_x);
		WriteCoord (MSG_BROADCAST, dorg_y);
		WriteCoord (MSG_BROADCAST, dorg_z);					
		W_ChainLightning(targ1, from, iter);

		if (targ2 != source) {
			targ2.customflags = targ2.customflags | CFL_ZAPPED;
			dorg = targ2.origin + (targ2.mins + targ2.maxs)*0.5;
			
			WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
			WriteByte (MSG_BROADCAST, TE_LIGHTNING2);
			WriteEntity (MSG_BROADCAST, targ2);
			WriteCoord (MSG_BROADCAST, org_x);
			WriteCoord (MSG_BROADCAST, org_y);
			WriteCoord (MSG_BROADCAST, org_z);
			WriteCoord (MSG_BROADCAST, dorg_x);
			WriteCoord (MSG_BROADCAST, dorg_y);
			WriteCoord (MSG_BROADCAST, dorg_z);	
			W_ChainLightning(targ2, from, iter);		
		}
		else kill += 30 - 5 * iter;	// do more damage if there's no one else to chain to
	}
	else kill += 60 - 5 * iter;	// this makes the LG more like a normal quad LG against single targets
	T_Damage (source, from, from, kill);
}

void(float special) W_FireLightning =
{
	local	vector		org;
	local	float		cells;

	if (self.ammo_cells < 1 && !special)
	{
		W_SelectBestWeapon();
		return;
	}

// explode if under water
	if (self.waterlevel > 1)
	{
		cells = self.ammo_cells;
		self.ammo_cells = 0;
		W_SetCurrentAmmo ();
		T_RadiusDamage (self, self, 35*cells, world);
		return;
	}

	if (self.width < time && !special)
	{
		sound (self, CHAN_WEAPON, "weapons/lhit.wav", 1, ATTN_NORM);
		self.width = time + 0.6;
	}
	self.punchangle_x = -2;

	if (!special) self.currentammo = self.ammo_cells = self.ammo_cells - 1;

	org = self.origin + '0 0 16';
	
		traceline2(org, org + v_forward*600, self, TRACE_NOMONSTERS);
	
		WriteByte (MSG_BROADCAST, SVC_TEMPENTITY);
		WriteByte (MSG_BROADCAST, TE_LIGHTNING2);
		WriteEntity (MSG_BROADCAST, self);
		WriteCoord (MSG_BROADCAST, org_x);
		WriteCoord (MSG_BROADCAST, org_y);
		WriteCoord (MSG_BROADCAST, org_z);
		WriteCoord (MSG_BROADCAST, trace_endpos_x);
		WriteCoord (MSG_BROADCAST, trace_endpos_y);
		WriteCoord (MSG_BROADCAST, trace_endpos_z);
		if (special)
			LightningMultiDamage (self.origin, trace_endpos + v_forward*4, self, 30, 1);
		else
			LightningDamage (self.origin, trace_endpos + v_forward*4, self, 30);
}
