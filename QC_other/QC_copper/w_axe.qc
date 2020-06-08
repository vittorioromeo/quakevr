/*
===============

THE AXE

===============
*/

// SOUNDS

void() W_AxeWhiffSound =
{
		sound (self, CHAN_OTHER, "weapons/ax1.wav", 1, ATTN_NORM);
}

void() W_AxeHitSound =
{
	if (random() < 0.5)
		sound (self, CHAN_WEAPON, "weapons/axhit1.wav", 1, ATTN_NORM);
	else
		sound (self, CHAN_WEAPON, "weapons/axhit2.wav", 1, ATTN_NORM);
}


void(float foff) W_AxeCycle =
{
	// lip and touch_time are overloaded on the player for animation event timing
	// lip = 10fps player anim clock (axe animates at 20 so player has to think on 20s)
	// touch_time = timer for the actual weapon attack trace event
	self.weaponframe = min(57, self.weaponframe + foff);
	
	// entire axe swing anim is scheduled to be done by now
	if (self.attack_finished <= time)
	{
		// stop the player anim cycle and restore normal thinking
		W_SetIdleWeaponFrame();
		player_run();
		return;
	}
	
	if (self.touch_time && self.touch_time <= time)
	{
		self.touch_time = 0;
		W_FireAxe();
	}
	
	// time for a regular animation think
	else if (self.lip <= time)
	{
		self.think1();	// advance the player anim on the standard 10fps clock
		self.lip = time + 0.1;
	}
	self.nextthink = time + 0.05;
}

void() W_AxeThink = { self.think = W_AxeThink; W_AxeCycle(1);  } 
void() W_AxeThinkReverse = {  self.think = W_AxeThinkReverse; W_AxeCycle(-1); } 

void() W_AxeSwing =
{
	W_AxeWhiffSound();
		self.weaponframe = 10 * (floor(4.99 * random()));
		self.touch_time = time + 0.1;
		self.attack_finished = time + 0.49;
	self.think1 = player_axe;
	W_AxeThink();
}


/*
================
W_FireAxe
================
*/
void() W_FireAxe =
{
	vector	source, org;
	if (self.health <= 0)
		return;
	
	makevectors (self.v_angle);
	source = self.origin + '0 0 16';
	
	// for ai_nav:
	//if (nav_w_nodeNudge()) return;
	
		
		traceline2(source, source + v_forward * (64), self, 0);
		if (trace_fraction == 1.0) 
			return;
		
		self.show_hostile = time + 1;	// wake monsters up
		
		org = trace_endpos - v_forward*4;
		
		if (trace_ent.takedamage)
		{
			trace_ent.customflags = trace_ent.customflags | CFL_AXEHITME;
			SpawnBlood (org, '0 0 0', 20);
			
			if (trace_ent.type == "zombie")
				zombie_knockdown(trace_ent);
			
				T_Damage (trace_ent, self, self, 24);
			
			if (trace_ent.flags & FL_MONSTER)
			{
				W_AxeHitSound();
			}
			else
			{
				sound (self, CHAN_WEAPON, "zombie/z_hit.wav", 1, ATTN_IDLE);
			}
			
			// do knockback with a quad
			if (self.super_damage_finished > time)
			{
				if ((trace_ent.movetype == MOVETYPE_STEP || trace_ent.movetype == MOVETYPE_WALK) && 
					// don't swat bosses or other non-standard-sized enemies around
					trace_ent.maxs_x <= 32 && trace_ent.health > 0 && trace_ent.type != "boss")
				{
					local vector toss;
					toss = v_forward;
					toss_z = 0;
					toss = normalize(toss) * 400;
					toss_z = 100;
					//bprint(vtos(toss));
					//bprint("\n");
					//toss *=  - vlen(head.maxs - head.mins)
					trace_ent.origin_z = trace_ent.origin_z + 1;
					trace_ent.velocity = toss;
					trace_ent.flags = not(trace_ent.flags, FL_ONGROUND);
				}
			}
		}
		else
		{	// hit wall
			sound (self, CHAN_WEAPON, "player/axhit2.wav", 1, ATTN_NORM);
			gunshot(org);
		}
}

