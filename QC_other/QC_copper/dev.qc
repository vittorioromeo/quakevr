/*
==============================================================================

DEV COMMANDS

==============================================================================
*/

void() dev_test =
{
	local vector org;
	makevectors(self.v_angle);
	org = self.origin + self.view_ofs;
	traceline2(org, org + v_forward * 8192, self, 0);

	if (trace_fraction == 1.0 || !trace_ent)
		return;

	entity head = findalltargets(trace_ent);
	while(head)
	{
		bprint4(head.classname, " @ ", vtos(head.origin), "\n");
		head =  head.chain;
	}
	bprint("-----\n");
}

void() dev_tracetest =
{
	local vector org;
	makevectors(self.v_angle);
	org = self.origin + self.view_ofs;
	
	traceline (org, org + v_forward * 2048, FALSE, self);
	
	bprint3("startpos: ", vtos(org), "\n");
	bprint3("endpos: ", vtos(trace_endpos), "\n");
	bprint3("water: ", ftos(trace_inwater), "\n");
	bprint3("open: ", ftos(trace_inopen), "\n");
	bprint3("allsolid: ", ftos(trace_allsolid), "\n");
	
	traceline2 (org, org + v_forward * 2048, self, 0);
	
	bprint3("startpos: ", vtos(org), "\n");
	bprint3("endpos: ", vtos(trace_endpos), "\n");
	bprint3("water: ", ftos(trace_inwater), "\n");
	bprint3("open: ", ftos(trace_inopen), "\n");
	bprint3("allsolid: ", ftos(trace_allsolid), "\n");
}

void() dev_hitpoints =
{
	float zdif;
	
	bprint("----------\nRockets: ");
	bprint(ftos(num_rockets));
	bprint(" Zombies: ");
	bprint(ftos(num_zombies));
	
	zdif = num_rockets - num_zombies;
	zdif = max(0,num_rockets);
	ammo_total += zdif * DEBUG_DMG_PER_ROCKET;
	
	bprint("\nMonster HP: ");
	bprint(ftos(monster_health));
	bprint("\nAmmo DP: ");
	bprint(ftos(ammo_total));
	bprint("\nRATIO: ");
	bprint(ftos(ammo_total / monster_health));
	bprint("\n");
}

void() dev_warp =
{
	if(deathmatch || coop)
		return;
	local vector org;
	makevectors(self.v_angle);
	org = self.origin + self.view_ofs;
	traceline (org, org + v_forward * 8192, FALSE, self);

	if (trace_fraction < 1.0)
	{
		org = trace_endpos;
		org += trace_plane_normal * 40;
	}
	setorigin(self,org);
}

void() dev_serverflags =
{
	bprint3("SERVER FLAGS:\n========\n",ftos(serverflags),"\n");
	if (serverflags & SVFL_EPISODE1)	bprint("  episode 1 completed\n");
	if (serverflags & SVFL_EPISODE2)	bprint("  episode 2 completed\n");
	if (serverflags & SVFL_EPISODE3)	bprint("  episode 3 completed\n");
	if (serverflags & SVFL_EPISODE4)	bprint("  episode 4 completed\n");
	if (serverflags & SVFL_ALLEPISODES)	bprint("  all of quake completed\n");
}


// this is the most fun cheat code in the history of anything:
void() dev_invoke =
{
	if(deathmatch || coop)
		return;
	local vector org;
	makevectors(self.v_angle);
	org = self.origin + self.view_ofs;
	traceline2(org, org + v_forward * 8192, self, 0);

	if (trace_fraction < 1.0 && trace_ent)
	{
		activator = self;
		
		if (trace_ent.flags & FL_MONSTER)
			monster_kill(trace_ent);
		else if (trace_ent.classname == "func_door")
			door_force(trace_ent);
		else if (trace_ent.classname == "func_door_secret")
			secret_door_force(trace_ent);
		else if (trace_ent.classname == "func_button")
			button_force(trace_ent);
	}
}

void() dev_reveal =
{
//	bprint("dev\n");
	local vector org;
	makevectors(self.v_angle);
	org = self.origin + self.view_ofs;
	traceline (org, org + v_forward * 8192, FALSE, self);

	if (trace_fraction < 1.0 && trace_ent)
	{
	//	trace_ent.developer = 1;
		//bprint("hit entity:\n");
		bprint3("time: ", ftos(time), "\n");
		eprint(trace_ent);
		//bprint("hit ent's enemy:\n");
		//eprint(trace_ent.enemy);
		if (trace_ent.flags & FL_ONGROUND) bprint("  entity is ONGROUND\n");
		if (trace_ent.flags & FL_PARTIALGROUND) bprint("  entity is PARTIAL_GROUND\n");
		if (checkbottom(trace_ent)) bprint("  checkbottom is true\n");
		else bprint("  checkbottom is FALSE\n");
	}
}

void() dev_trigger =
{
	if(deathmatch || coop)
		return;
	local entity t, act;
	
//	bprint("dev\n");
	act = activator;
	t = world;
	do	{
		t = SUB_UseNextTarget( t, self.netname );
		activator = act;
		if (!t) break;
	} while ( 1 );
}

void() dev_enemychain =
{
	local vector org;
	makevectors(self.v_angle);
	org = self.origin + self.view_ofs;
	traceline (org, org + v_forward * 8192, FALSE, self);

	entity e;
	float i;
	i = 0;
	if (trace_fraction < 1.0 && trace_ent)
	{
		e = trace_ent;
		do
		{
			bprint5(ftos(i),": ",e.classname," at ~ ",vtos((e.absmin + e.absmax)* 0.5));
			bprint("\n");
			e = e.enemy;
			i += 1;
		}
		while (e != trace_ent);
	}
}


// for when i light a map at 1am and then play it again the next afternoon
void() dev_i_cant_see =
{
	if (icantsee)
		lightstyle(0, "m");
	else
		lightstyle(0, "z");
	icantsee = !icantsee;
	cleanUpClientShit = 2;
}


void() dev_whatami =
{
	eprint(self);
}

