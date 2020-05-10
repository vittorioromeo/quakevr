/*
=============================================================================

	LEVEL CHANGING / INTERMISSION

=============================================================================
*/

void() GotoNextMap =
{
	if (cvar("samelevel"))	// if samelevel is set, stay on same level
		changelevel (mapname);	// name of current level
	else
		changelevel (nextmap);	// global set to next level
}


void() ExitIntermission =
{
	// skip any text in deathmatch
	if (deathmatch)
	{
		GotoNextMap ();
		return;
	}
	
	intermission_exittime = time + 1;
	intermission_running = intermission_running + 1;

	// end of episode drivel
	if (intermission_running == 2)
	{
		WriteByte (MSG_ALL, SVC_CDTRACK);
		WriteByte (MSG_ALL, 2);
		WriteByte (MSG_ALL, 3);
		if (world.model == "maps/e1m7.bsp")
		{
			if (!cvar("registered"))
			{
				WriteByte (MSG_ALL, SVC_FINALE);
				WriteString (MSG_ALL, "As the corpse of the monstrous entity\nChthon sinks back into the lava whence\nit rose, you grip the Rune of Earth\nMagic tightly. Now that you have\nconquered the Dimension of the Doomed,\nrealm of Earth Magic, you are ready to\ncomplete your task in the other three\nhaunted lands of Quake. Or are you? If\nyou don't register Quake, you'll never\nknow what awaits you in the Realm of\nBlack Magic, the Netherworld, and the\nElder World!");
			}
			else
			{
				WriteByte (MSG_ALL, SVC_FINALE);
				WriteString (MSG_ALL, "As the corpse of the monstrous entity\nChthon sinks back into the lava whence\nit rose, you grip the Rune of Earth\nMagic tightly. Now that you have\nconquered the Dimension of the Doomed,\nrealm of Earth Magic, you are ready to\ncomplete your task. A Rune of magic\npower lies at the end of each haunted\nland of Quake. Go forth, seek the\ntotality of the four Runes!");
			}
			return;
		}
		else if (world.model == "maps/e2m6.bsp")
		{
			WriteByte (MSG_ALL, SVC_FINALE);
			WriteString (MSG_ALL, "The Rune of Black Magic throbs evilly in\nyour hand and whispers dark thoughts\ninto your brain. You learn the inmost\nlore of the Hell-Mother; Shub-Niggurath!\nYou now know that she is behind all the\nterrible plotting which has led to so\nmuch death and horror. But she is not\ninviolate! Armed with this Rune, you\nrealize that once all four Runes are\ncombined, the gate to Shub-Niggurath's\nPit will open, and you can face the\nWitch-Goddess herself in her frightful\notherworld cathedral.");
			return;
		}
		else if (world.model == "maps/e3m6.bsp")
		{
			WriteByte (MSG_ALL, SVC_FINALE);
			WriteString (MSG_ALL, "The charred viscera of diabolic horrors\nbubble viscously as you seize the Rune\nof Hell Magic. Its heat scorches your\nhand, and its terrible secrets blight\nyour mind. Gathering the shreds of your\ncourage, you shake the devil's shackles\nfrom your soul, and become ever more\nhard and determined to destroy the\nhideous creatures whose mere existence\nthreatens the souls and psyches of all\nthe population of Earth.");
			return;
		}
		else if (world.model == "maps/e4m7.bsp")
		{
			WriteByte (MSG_ALL, SVC_FINALE);
			WriteString (MSG_ALL, "Despite the awful might of the Elder\nWorld, you have achieved the Rune of\nElder Magic, capstone of all types of\narcane wisdom. Beyond good and evil,\nbeyond life and death, the Rune\npulsates, heavy with import. Patient and\npotent, the Elder Being Shub-Niggurath\nweaves her dire plans to clear off all\nlife from the Earth, and bring her own\nfoul offspring to our world! For all the\ndwellers in these nightmare dimensions\nare her descendants! Once all Runes of\nmagic power are united, the energy\nbehind them will blast open the Gateway\nto Shub-Niggurath, and you can travel\nthere to foil the Hell-Mother's plots\nin person.");
			return;
		}
		GotoNextMap();
	}
	
	if (intermission_running == 3)
	{
		if (!cvar("registered"))
		{	// shareware episode has been completed, go to sell screen
			WriteByte (MSG_ALL, SVC_SELLSCREEN);
			return;
		}
		
		if ( (serverflags & SVFL_ALLEPISODES) == SVFL_ALLEPISODES)
		{
			WriteByte (MSG_ALL, SVC_FINALE);
			WriteString (MSG_ALL, "Now, you have all four Runes. You sense\ntremendous invisible forces moving to\nunseal ancient barriers. Shub-Niggurath\nhad hoped to use the Runes Herself to\nclear off the Earth, but now instead,\nyou will use them to enter her home and\nconfront her as an avatar of avenging\nEarth-life. If you defeat her, you will\nbe remembered forever as the savior of\nthe planet. If she conquers, it will be\nas if you had never been born.");
			return;
		}
		GotoNextMap();
	}
	
	if (intermission_running == 4)
	{
		GotoNextMap();
	}
		
	if (intermission_running >= 5)
	{
		GotoNextMap();
		return;
	}

		
	dprint("fell off the end of exitintermission\n");
	return;
}



/*
============
ClearItemEffects

turn off glow and screen coloring while we're in a camera
============
*/
void(entity who) ClearItemEffects =
{
	who.items = not(who.items, IT_INVISIBILITY | IT_INVULNERABILITY | IT_SUIT | IT_QUAD);
	who.effects = 0;

	// remember how much time was left on the powerup
	if (who.radsuit_finished > time)
		who.radsuit_finished -= time;
	if (who.invincible_finished > time)
		who.invincible_finished -= time;
	if (who.invisible_finished > time)
		who.invisible_finished -= time;
	if (who.super_damage_finished > time)
		who.super_damage_finished -= time;
	/*
	who.rad_time = 0;
	who.invincible_time = 0;
	who.invisible_time = 0;
	who.super_time = 0;
	*/
}


/*
============
RestoreItemEffects
============
*/
void(entity who) RestoreItemEffects =
{
	if ( who.radsuit_finished > 0 )
	{
		who.radsuit_finished += time;
		who.items += IT_SUIT;
	}
	if ( who.invincible_finished > 0 )
	{
		who.invincible_finished += time;
		who.items += IT_INVULNERABILITY;
	}
	if ( who.invisible_finished > 0 )
	{
		who.invisible_finished += time;
		who.items += IT_INVISIBILITY;
	}
	if ( who.super_damage_finished > 0 )
	{
		who.super_damage_finished += time;
		who.items += IT_QUAD;
	}
}



/*
============
FindIntermission

Returns an entity to view from
============
*/
entity(entity start) FindIntermission =
{
	entity spot;
	float cyc;

// look for info_intermission first
	spot = find (start, classname, "info_intermission");
	if (spot)
	{
		// pick a random one
		cyc = random() * 16;
		while (cyc > 1)
		{
			spot = find (spot, classname, "info_intermission");
			if (!spot)
				spot = find (spot, classname, "info_intermission");
			cyc = cyc - 1;
		}
		return spot;
	}
	spot = find (world, classname, "info_intermission");
	if (spot)
		return spot;

// then look for the start position
	spot = find (world, classname, "info_player_start");
	if (spot)
		return spot;
	
// testinfo_player_start is only found in regioned levels
	spot = find (world, classname, "testplayerstart");
	if ( spot && ( spot.spawnflags & 1 ) )
		return spot;
	
	objerror ("FindIntermission: no spot");
	return world;
}

entity(entity start) FindNextIntermission =
{
	if (!start) return world;
	if (start.classname != "info_intermission") return start;
	
	entity spot;
	spot = find(start, classname, "info_intermission");	
	if (!spot)
		spot = find(world, classname, "info_intermission");
	return spot;
}

entity(entity start) FindPreviousIntermission =
{
	if (!start) return world;
	if (start.classname != "info_intermission") return start;
	entity last, spot;
	spot = find(world, classname, "info_intermission");
	
	if (spot == start)
	{
		// there's either only one spot or we're starting with the first one, so try to find the last one
		while(spot)
		{
			last = spot;
			spot = find(spot, classname, "info_intermission");
		}
		return last;
	}
	
	while(spot != start)
	{
		last = spot;
		spot = find(spot, classname, "info_intermission");
	}
	return last;
}

// ================================================================

void(entity client, entity spot) move_player_to_intermission =
{
	ClearItemEffects(client);	// remove powerups to clear glows/tints
	
	client.flags |= FL_NOTARGET;
	client.view_ofs = '0 0 0';
	client.angles = client.v_angle = client.finalangle = spot.mangle;
	client.fixangle = TRUE;		// turn this way immediately
	client.nextthink = time + 0.5;
	client.takedamage = DAMAGE_NO;
	client.solid = SOLID_NOT;
	client.movetype = MOVETYPE_NONE;
	client.goalentity = spot;
	//bprint3("setting origin to ", vtos(spot.origin), "\n");
	
	// dumb hack: origins don't update without a model?
	client.modelindex = modelindex_eyes;
	setorigin (client, spot.origin);
//	client.modelindex = 0;
	
	fog_setFromEnt(client, spot);	// need to get fog from any camera we jump to
}

void() move_player_to_next_intermission =
{
	entity	spot;
	spot = FindNextIntermission(self.goalentity);
	move_player_to_intermission(self,spot);
}

void() move_player_to_previous_intermission =
{
	entity	spot;
	spot = FindPreviousIntermission(self.goalentity);
	move_player_to_intermission(self,spot);
}

void() move_players_to_intermission =
{
	entity spot;
	spot = FindIntermission(world);

	other = find (world, classname, "player");

	while (other != world)
	{
		move_player_to_intermission(other,spot);		
		other.lifetime_finished = time + 6;
		other = find (other, classname, "player");		
	}
}

void() execute_changelevel =
{

	//bprint("WHY ARE WE IN EXECUTE CHANGELEVEL\n");
	
	intermission_running = 1;
	
// enforce a wait time before allowing changelevel
	if (deathmatch)
		intermission_exittime = time + 5;
	else
		intermission_exittime = time + 2;

	WriteByte (MSG_ALL, SVC_CDTRACK);
	WriteByte (MSG_ALL, 3);
	WriteByte (MSG_ALL, 3);

	move_players_to_intermission();

	WriteByte (MSG_ALL, SVC_INTERMISSION);
}

void() changelevel_do =
{
	if (coop || deathmatch)
	{
		bprint (other.netname);
		bprint (" exited the level\n");
	}
	
		nextmap = self.map;
	
	if ( (self.spawnflags & 1) && (deathmatch == 0) )
	{	// NO_INTERMISSION
		GotoNextMap();
		return;
	}

	// we can't move people right now, because touch functions are called
	// in the middle of C movement code, so set a think time to do it
	self.think = execute_changelevel;
	self.nextthink = time + 0.1;
}


void() changelevel_touch =
{
	if (!CheckValidTouch()) return;
	if (self.customflags & CFL_LOCKED) return;

	if (deathmatch)
	{
		if ((cvar("noexit") == 1) || ((cvar("noexit") == 2) && (mapname != "start")))
		{
			T_Damage (other, self, self, 50000);
			return;
		}
	}

	self.touch = SUB_Null;
	
	activator = other;
	SUB_UseTargets ();
	
	changelevel_do();
}

/*QUAKED trigger_changelevel (.5 0 .5) ? no_intermission ? TRIGGER_FIRST
When the player touches this, he gets sent to the map listed in the "map" variable. The view will go to the info_intermission spot and display stats.

Flags:
"no_intermission" bypasses intermission screen
"trigger_first" won't be touchable until triggered once by something else
   
Keys:
"map" set to name of next map (e.g. e1m1) if no map is set, the current map will restart
*/
/*FGD
@SolidClass base(Trigger) = trigger_changelevel : "Trigger: Change level"
[
	map(string) : "Next map"
	spawnflags(flags) =
	[
		1: "No intermission" : 0
	]
]
*/
void() trigger_changelevel =
{
	if (!SUB_ShouldSpawn()) return;
	if (!self.map)
		objerror ("trigger_changelevel doesn't have map");
	
	InitTrigger ();
	self.touch = changelevel_touch;
}

/*QUAKED target_changelevel (1 1 0.5) (-16 -16 -16) (16 16 16) NO_INTERMISSION
When the player triggers this by any means, he gets sent to the map listed in the "map" variable.  
Spawnflags:
NO_INTERMISSION: the view will not go to the info_intermission spot and display stats
*/
/*FGD
@PointClass base(Trigger) = trigger_changelevel : "Target: Change level"
[
	map(string) : "Next map"
	spawnflags(flags) =
	[
		1: "No intermission" : 0
	]
]
*/
void() target_changelevel =
{
	if (!SUB_ShouldSpawn()) return;
	if (!self.map)
		objerror ("target_changelevel doesn't have map");
	
	self.use = changelevel_do;
}


/*QUAKED info_intermission (.75 .25 .25) (-8 -8 -8) (8 8 8)
This is the camera point for the intermission. Use "mangle" instead of angle, so you can set pitch or roll as well as yaw.
Quake does a random pick if more than one exists.
If no info_intermission entity is set, Quake uses the player start.

Keys:
"mangle" set pitch yaw roll, positive pitch is down
"fog_color"/"fog_density" set the fog for the view from this camera

*/
/*FGD
@PointClass base(Fog, Appearflags) = info_intermission : "Intermission camera" 
[
	mangle(string) : "Camera angle (Pitch Yaw Roll, positive pitch down)"
]
*/
void() info_intermission =
{
	if (!SUB_ShouldSpawn()) return;
}


