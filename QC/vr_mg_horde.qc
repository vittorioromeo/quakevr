/*  Copyright (C) 1996-2022 id Software LLC

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

    See file, 'COPYING', for details.
*/

// Native MG1 Horde adaptation; official source licensing above applies.
.string MGH_category;
.void() MGH_start;
.entity MGH_pickup_spawn;
.float MGH_hunger_time;
.float MGH_reviving;
.float MGH_powerup_expire;
.float MGH_corpse_expire;
entity MGH_horde_ent;
float MGH_initial_serverflags;
nosave float MGH_restart_queued; // a team wipe queues one arena restart (the press can last several frames)
.float MGH_killtime;
.float MGH_killspree;
float MGH_keys_silver, MGH_keys_gold, MGH_powerup_chance;
float MGH_DEFAULT_POWERUP_CHANCE = 0.025;
float MGH_POWERUP_CHANCE_GAIN = 0.025;
float MGH_HORDE_START_DELAY = 10;
float MGH_HORDE_AMMO_RESPAWN_DELAY = 20;
float MGH_START_OFF = 1;
string MGH_CATEGORY_MONSTER = "monster";
.float MGH_storednextthink;
.void() MGH_storedthink;
void() MGH_Countdown;
void() MGH_Wavecheck;
void() MGH_SpawnAmmo;
.float	wave; // what wave of the horde mode we're on
.float	fodder; // how many fodder squads to spawn
.float	elites; // how many elite squads to spawn
.float	bosses; // how many bosses to spawn
.float 	spawncount; // number of monster spawns
.float army; // Spawn Army? True/fallse
float(entity spawnpoint) MGH_CheckBlockedSpawn;

float MGH_key_spawned; // TRUE/FALSE, determine if key has already been spawned, prevent dupes.

float MGH_SPAWN_RESET_TIME = 5; // amount of time that a spawn point is considered occupied and not valid for use

/*
================
MGH_HordeGetPlayersAlive

added Aug31 2021
Returns a float for the number of living players
================
*/
float() MGH_HordeGetPlayersAlive =
{
	local float playercount = 0;
	local entity e;

	// count entities
	e = find(world, classname, "player");
	while(e)
	{
		if (e.health > 0)
		{
			if (e.flags & FL_CLIENT)
				playercount++;
			else
				dprint("Player is no longer a client!\n");
		}
		e = find(e, classname, "player");
	}
	dprint("there are ");
	dprint(ftos(playercount));
	dprint(" players alive.\n");

	return playercount;
};

/*
================
MGH_HordeGetMonstersAlive

added Oct28 2021
Manually counts all living monsters
================
*/
float() MGH_HordeGetMonstersAlive =
{
	local float monstercount = 0;
	local entity e;

	// count monster entities
	e = find(world, MGH_category, "monster");
	while(e)
	{
		if ((e.health > 0) && (e.classname != "monster_zombie"))
			monstercount++;

		e = find(e, MGH_category, "monster");
	}
	dprint("verification count. There are ");
	dprint(ftos(monstercount));
	dprint(" monsters alive.\n");

	return monstercount;
};
/*
================
MGH_HordeFindTarget

added Aug31 2021
Returns a random living player
================
*/
entity() MGH_HordeFindTarget =
{
	local float playercount = MGH_HordeGetPlayersAlive();

	if (playercount == 0)
		return world;

	local entity e;
	local float rcount; // random value [0,count]
	rcount = random() * playercount;

	playercount = 0; // reset for new count
	e = find(world, classname, "player");
	while(e)
	{
		if (e.health > 0)
			playercount++;

		if ((rcount <= playercount) && !(e.flags & FL_NOTARGET))
			return e;
		else
			e = find(e, classname, "player");
	}
	return world;
};


/*QUAKED info_monster_start (1 0 0) (-80 -80 0) (80 80 128) START_OFF SKIP_BLOCK_CHECK
if targeted, it will toggle between on or off, like lights
*/
float MGH_SKIP_BLOCK_CHECK = 2;
void() MGH_monster_start_use =
{
	if (self.spawnflags & MGH_START_OFF)
		self.spawnflags = self.spawnflags - MGH_START_OFF;
	else
		self.spawnflags = self.spawnflags + MGH_START_OFF;
};

void() info_monster_start =
{
 if(!MG1_Campaign()) { remove(self); return; }
	self.wait = FALSE; // used for checking if this is a valid spawn
	self.use = MGH_monster_start_use;
	setorigin (self, self.origin);
	setsize(self, '-80 -80 0', '80 80 128');
};

void() info_monster_start_ranged = // used for ogres and enforcers
{
 if(!MG1_Campaign()) { remove(self); return; }
	self.wait = FALSE; // used for checking if this is a valid spawn
	self.use = MGH_monster_start_use;
	setorigin (self, self.origin);
	setsize(self, '-44 -44 0', '44 44 128');
};

void() info_monster_start_flying = // used exclusively for scrags (wizards)
{
 if(!MG1_Campaign()) { remove(self); return; }
	self.wait = FALSE; // used for checking if this is a valid spawn
	self.use = MGH_monster_start_use;
	setorigin (self, self.origin);
	setsize(self, '-80 -80 0', '80 80 128');
};

void() info_monster_start_boss = // used for shalraths and shamblers
{
 if(!MG1_Campaign()) { remove(self); return; }
	self.wait = FALSE; // used for checking if this is a valid spawn
	self.use = MGH_monster_start_use;
	setorigin (self, self.origin);
	setsize(self, '-44 -44 0', '44 44 128');
};

// ENTITY
void() info_horde_ammo =
{
 if(!MG1_Campaign()) { remove(self); return; }
	//self.wait = FALSE;
	self.think = MGH_SpawnAmmo;
	self.nextthink = time + MGH_HORDE_START_DELAY + random() * 3;
};

// ENTITY
void() info_horde_item =
{
 if(!MG1_Campaign()) { remove(self); return; }
	self.wait = FALSE;
};

float MGH_KEY_FIRST = 1;
float MGH_KEY_SECOND = 2;
float MGH_KEY_THIRD = 4;
float MGH_KEY_FOURTH_PLUS = 8;

// ENTITY
void() info_horde_key =
{
 if(!MG1_Campaign()) { remove(self); return; }
	self.wait = FALSE;
};
//============================================================================
void() MGH_PrecacheMonsters =
{
	// knight
	precache_model ("progs/knight.mdl");
	precache_model ("progs/h_knight.mdl");

	precache_sound ("knight/kdeath.wav");
	precache_sound ("knight/khurt.wav");
	precache_sound ("knight/ksight.wav");
	precache_sound ("knight/sword1.wav");
	precache_sound ("knight/sword2.wav");
	precache_sound ("knight/idle.wav");

	// hellknight
	precache_model ("progs/hknight.mdl");
	precache_model ("progs/k_spike.mdl");
	precache_model ("progs/h_hellkn.mdl");


	precache_sound ("hknight/attack1.wav");
	precache_sound ("hknight/death1.wav");
	precache_sound ("hknight/pain1.wav");
	precache_sound ("hknight/sight1.wav");
	precache_sound ("hknight/hit.wav");		// used by C code, so don't sound2
	precache_sound ("hknight/slash1.wav");
	precache_sound ("hknight/idle.wav");
	precache_sound ("hknight/grunt.wav");

	precache_sound ("knight/sword1.wav");
	precache_sound ("knight/sword2.wav");

	// dog
	precache_model ("progs/h_dog.mdl");
	precache_model ("progs/dog.mdl");

	precache_sound ("dog/dattack1.wav");
	precache_sound ("dog/ddeath.wav");
	precache_sound ("dog/dpain1.wav");
	precache_sound ("dog/dsight.wav");
	precache_sound ("dog/idle.wav");

	// demon
	precache_model ("progs/demon.mdl");
	precache_model ("progs/h_demon.mdl");

	precache_sound ("demon/ddeath.wav");
	precache_sound ("demon/dhit2.wav");
	precache_sound ("demon/djump.wav");
	precache_sound ("demon/dpain1.wav");
	precache_sound ("demon/idle1.wav");
	precache_sound ("demon/sight2.wav");

	// ogre
	precache_model ("progs/ogre.mdl");
	precache_model ("progs/h_ogre.mdl");
	precache_model ("progs/grenade.mdl");

	precache_sound ("ogre/ogdrag.wav");
	precache_sound ("ogre/ogdth.wav");
	precache_sound ("ogre/ogidle.wav");
	precache_sound ("ogre/ogidle2.wav");
	precache_sound ("ogre/ogpain1.wav");
	precache_sound ("ogre/ogsawatk.wav");
	precache_sound ("ogre/ogwake.wav");

	// grunt
	precache_model ("progs/soldier.mdl");
	precache_model ("progs/h_guard.mdl");
	precache_model ("progs/gib1.mdl");
	precache_model ("progs/gib2.mdl");
	precache_model ("progs/gib3.mdl");

	precache_sound ("soldier/death1.wav");
	precache_sound ("soldier/idle.wav");
	precache_sound ("soldier/pain1.wav");
	precache_sound ("soldier/pain2.wav");
	precache_sound ("soldier/sattck1.wav");
	precache_sound ("soldier/sight1.wav");

	precache_sound ("player/udeath.wav");		// gib death

	// enforcer
	precache_model ("progs/enforcer.mdl");
	precache_model ("progs/h_mega.mdl");
	precache_model ("progs/laser.mdl");

	precache_sound ("enforcer/death1.wav");
	precache_sound ("enforcer/enfire.wav");
	precache_sound ("enforcer/enfstop.wav");
	precache_sound ("enforcer/idle1.wav");
	precache_sound ("enforcer/pain1.wav");
	precache_sound ("enforcer/pain2.wav");
	precache_sound ("enforcer/sight1.wav");
	precache_sound ("enforcer/sight2.wav");
	precache_sound ("enforcer/sight3.wav");
	precache_sound ("enforcer/sight4.wav");

	// shambler
	precache_model ("progs/shambler.mdl");
	precache_model ("progs/s_light.mdl");
	precache_model ("progs/h_shams.mdl");
	precache_model ("progs/bolt.mdl");

	precache_sound ("shambler/sattck1.wav");
	precache_sound ("shambler/sboom.wav");
	precache_sound ("shambler/sdeath.wav");
	precache_sound ("shambler/shurt2.wav");
	precache_sound ("shambler/sidle.wav");
	precache_sound ("shambler/ssight.wav");
	precache_sound ("shambler/melee1.wav");
	precache_sound ("shambler/melee2.wav");
	precache_sound ("shambler/smack.wav");

	// shalrath
	precache_model ("progs/shalrath.mdl");
	precache_model ("progs/h_shal.mdl");
	precache_model ("progs/v_spike.mdl");

	precache_sound ("shalrath/attack.wav");
	precache_sound ("shalrath/attack2.wav");
	precache_sound ("shalrath/death.wav");
	precache_sound ("shalrath/idle.wav");
	precache_sound ("shalrath/pain.wav");
	precache_sound ("shalrath/sight.wav");

	// wizard
	precache_model ("progs/wizard.mdl");
	precache_model ("progs/h_wizard.mdl");
	precache_model ("progs/w_spike.mdl");

	precache_sound ("wizard/hit.wav");		// used by c code
	precache_sound ("wizard/wattack.wav");
	precache_sound ("wizard/wdeath.wav");
	precache_sound ("wizard/widle1.wav");
	precache_sound ("wizard/widle2.wav");
	precache_sound ("wizard/wpain.wav");
	precache_sound ("wizard/wsight.wav");

	// zombies
	precache_model ("progs/zombie.mdl");
	precache_model ("progs/h_zombie.mdl");
	precache_model ("progs/zom_gib.mdl");

	precache_sound ("zombie/z_idle.wav");
	precache_sound ("zombie/z_idle1.wav");
	precache_sound ("zombie/z_shot1.wav");
	precache_sound ("zombie/z_gib.wav");
	precache_sound ("zombie/z_pain.wav");
	precache_sound ("zombie/z_pain1.wav");
	precache_sound ("zombie/z_fall.wav");
	precache_sound ("zombie/z_miss.wav");
	precache_sound ("zombie/z_hit.wav");
	precache_sound ("zombie/idle_w2.wav");


	// ammo
	precache_model ("maps/b_shell1.bsp");
	precache_model ("maps/b_shell0.bsp");
	precache_model ("maps/b_nail1.bsp");
	precache_model ("maps/b_nail0.bsp");
	precache_model ("maps/b_rock1.bsp");
	precache_model ("maps/b_rock0.bsp");
	precache_model ("maps/b_batt1.bsp");
	precache_model ("maps/b_batt0.bsp");

	// items
	precache_model("maps/b_bh10.bsp"); // rotten health
	precache_sound("items/r_item1.wav");
	precache_model("maps/b_bh25.bsp"); // regular health
	precache_sound("items/health1.wav");
	precache_model ("progs/armor.mdl"); // regular armor

	// quad damage
	precache_model ("progs/quaddama.mdl");
	precache_sound ("items/damage.wav");
	precache_sound ("items/damage2.wav");
	precache_sound ("items/damage3.wav");

	// pentagram (invulnerability)
	precache_model ("progs/invulner.mdl");
	precache_sound ("items/protect.wav");
	precache_sound ("items/protect2.wav");
	precache_sound ("items/protect3.wav");

	// new horde sounds
	precache_sound("horde/nyom.wav");

	// precache keys
	if (world.worldtype == 0 || world.worldtype == 3)
	{
		precache_sound ("misc/medkey.wav");
		precache_model("progs/w_g_key.mdl"); // gold key
		precache_model("progs/w_s_key.mdl"); // silver key
	}
	if (world.worldtype == 1)
	{
		precache_sound ("misc/runekey.wav");
		precache_model ("progs/m_g_key.mdl");
		precache_model ("progs/m_s_key.mdl");
	}
	if (world.worldtype == 2)
	{
		precache_sound ("misc/basekey.wav");
		precache_model ("progs/b_g_key.mdl");
		precache_model ("progs/b_s_key.mdl");
	}
};
// =================================================================
void() MGH_GibMonster =
{
	T_Damage(self, world, world, 4000);
};
// =================================================================
/*
================
MGH_SpawnMonster
================
*/
entity(string class, vector org, vector temp_angles) MGH_SpawnMonster = {
 entity manager = self, monster = spawn(); self = monster;
 monster.angles = temp_angles; setorigin(monster, org + '0 0 32');
 if(class == "knight") { monster.classname = "monster_knight"; monster_knight(); }
 else if(class == "hellknight") { monster.classname = "monster_hell_knight"; monster_hell_knight(); }
 else if(class == "dog") { monster.classname = "monster_dog"; monster_dog(); }
 else if(class == "demon") { monster.classname = "monster_demon1"; monster_demon1(); }
 else if(class == "ogre") { monster.classname = "monster_ogre"; monster_ogre(); }
 else if(class == "grunt") { monster.classname = "monster_army"; monster_army(); }
 else if(class == "enforcer") { monster.classname = "monster_enforcer"; monster_enforcer(); }
 else if(class == "shambler") { monster.classname = "monster_shambler"; monster_shambler(); }
 else if(class == "shalrath") { monster.classname = "monster_shalrath"; monster_shalrath(); }
 else if(class == "wizard") { monster.classname = "monster_wizard"; monster_wizard(); }
 else if(class == "zombie") { monster.classname = "monster_zombie"; monster_zombie(); }
 else { remove(monster); self = manager; return world; }
 monster.MGH_category = MGH_CATEGORY_MONSTER;
 monster.MGH_start = monster.think;
 monster.think = MGH_MonsterStart; monster.nextthink = time + 0.1;
 monster.owner = manager;
 if(class == "zombie") total_monsters--;
 self = manager; return monster;
};
void() MGH_MonsterStart = {
 self.MGH_start();
 self.enemy = MGH_HordeFindTarget();
 if(self.enemy) { self.think = FoundTarget; self.nextthink = time + 0.1; }
};

void() MGH_horde_ammo_touch =
{
	local entity temp_player, t;

	// early exit touch if not a player or not alive
	if (other.classname != "player")
		return;
	if (other.health <= 0)
		return;

	// if player is at max ammo for ammo's type, return
	if (((self.weapon == 1) && (other.ammo_shells >= VR_MaxAmmo(other, AID_SHELLS))) || // shotgun
		((self.weapon == 2) && (other.ammo_nails >= VR_MaxAmmo(other, AID_NAILS))) || // nailgun
		((self.weapon == 3) && (other.ammo_rockets >= VR_MaxAmmo(other, AID_ROCKETS))) || // rockets
		((self.weapon == 4) && (other.ammo_cells >= VR_MaxAmmo(other, AID_CELLS)))) // cells
		return;

	sprint(other, "You got ");
	sprint(other, self.netname);
	sprint(other, "\n");

	// ammo touch sound
	sound (other, CHAN_ITEM, "weapons/lock4.wav", 1, ATTN_NORM);
	stuffcmd (other, "bf\n");

	// loop through all players and give ammo
	temp_player = find(world, classname, "player");
	while(temp_player)
	{
		if (temp_player.health > 0) // only give ammo to living players
		{
			if (self.weapon == 1) // shotgun
				temp_player.ammo_shells += self.aflag;
			else if (self.weapon == 2) // spikes
				temp_player.ammo_nails += self.aflag;
			else if (self.weapon == 3) // rockets
				temp_player.ammo_rockets += self.aflag;
			else if (self.weapon == 4) // cells
				temp_player.ammo_cells += self.aflag;

			// temp swap and set bounds
			t = other;
			other = temp_player;
			bound_other_ammo();
			other = t;

			// temp swap again and set current ammo
			t = self;
			self = temp_player;
			W_SetCurrentAmmo();
			self = t;
		}
		temp_player = find(temp_player, classname, "player");
	}

	// remove
	self.model = string_null;
	self.solid = SOLID_NOT;
	// AY dec06 2021
	//self.MGH_pickup_spawn.wait = FALSE; The Old Way
	self.MGH_pickup_spawn.think = MGH_SpawnAmmo;
	self.MGH_pickup_spawn.nextthink = time + MGH_HORDE_AMMO_RESPAWN_DELAY;
	// end AY
	VR_Carry_Forget(self); remove(self);
};

void() MGH_horde_health_touch =
{
	if (other.classname != "player")
		return;

	if (!T_Heal(other, self.healamount, 0))
		return;

	sprint(other, sprintf("You got %g health\n", self.healamount));

	// health touch sound
	sound(other, CHAN_ITEM, self.noise, 1, ATTN_NORM);
	stuffcmd (other, "bf\n");

	self.model = string_null;
	self.solid = SOLID_NOT;
	self.MGH_pickup_spawn.wait = FALSE;
	VR_Carry_Forget(self); remove(self);
};

void() MGH_horde_armor_touch =
{
	local float type = 0, value = 0, bit = 0;

	if (other.health <= 0)
		return;
	if (other.classname != "player")
		return;

	if (self.classname == "item_armor1")
	{
		type = 0.3;
		value = 100;
		bit = IT_ARMOR1;
	}
	if (self.classname == "item_armor2")
	{
		type = 0.6;
		value = 150;
		bit = IT_ARMOR2;
	}
	if (self.classname == "item_armorInv")
	{
		type = 0.8;
		value = 200;
		bit = IT_ARMOR3;
	}
	if (other.armortype*other.armorvalue >= type*value)
		return;

	other.armortype = type;
	other.armorvalue = value;
	VR_ItemUtil_EntDelItem(other, IID_ARMOR1); VR_ItemUtil_EntDelItem(other, IID_ARMOR2); VR_ItemUtil_EntDelItem(other, IID_ARMOR3); VR_ItemUtil_EntAddItem(other, bit == IT_ARMOR1 ? IID_ARMOR1 : bit == IT_ARMOR2 ? IID_ARMOR2 : IID_ARMOR3);

	self.solid = SOLID_NOT;
	self.model = string_null;
	sprint(other, "You got armor\n");
// armor touch sound
	sound(other, CHAN_ITEM, "items/armor1.wav", 1, ATTN_NORM);
	stuffcmd (other, "bf\n");
	self.MGH_pickup_spawn.wait = FALSE;
	VR_Carry_Forget(self); remove(self);
};

void() MGH_horde_print_keys =
{
	dprint(ftos(MGH_keys_silver));
	dprint(" silver keys : ");
	dprint(ftos(MGH_keys_gold));
	dprint(" gold keys\n");
};

void(entity temp_player) MGH_horde_set_keys =
{
 VR_ItemUtil_EntDelItem(temp_player, IID_KEY1); VR_ItemUtil_EntDelItem(temp_player, IID_KEY2);
	// player has just respawned, reset their keys based on global key values
	if (MGH_keys_silver > 0)
		VR_ItemUtil_EntAddItem(temp_player, IID_KEY1);

	if (MGH_keys_gold > 0)
		VR_ItemUtil_EntAddItem(temp_player, IID_KEY2);
};


void(float key_item) MGH_horde_key_give_all =
{
	local entity temp_player;
	temp_player = find(world, classname, "player");

	while (temp_player)
	{
		VR_ItemUtil_EntAddItem(temp_player, key_item == IT_KEY1 ? IID_KEY1 : IID_KEY2);
		temp_player = find(temp_player, classname, "player");
	}

	MGH_horde_print_keys();
};

void(float key_item) MGH_horde_key_remove_all =
{
	local entity temp_player;
	temp_player = find(world, classname, "player");

	while (temp_player)
	{
		VR_ItemUtil_EntDelItem(temp_player, key_item == IT_KEY1 ? IID_KEY1 : IID_KEY2);
		temp_player = find(temp_player, classname, "player");
	}

	MGH_horde_print_keys();
};

void(float key_item)MGH_horde_key_give =
{
	if (key_item & IT_KEY1)
	{
		MGH_keys_silver++;
		if (MGH_keys_silver == 1)
			MGH_horde_key_give_all(IT_KEY1);
	}
	else if (key_item & IT_KEY2)
	{
		MGH_keys_gold++;
		if (MGH_keys_gold == 1)
			MGH_horde_key_give_all(IT_KEY2);
	}
	MGH_horde_print_keys();
}

void(float key_item)MGH_horde_key_spend =
{
	if (key_item & IT_KEY1)
	{
		if(MGH_keys_silver <= 0) return; MGH_keys_silver--;
		if (MGH_keys_silver == 0)
			MGH_horde_key_remove_all(IT_KEY1);
	}
	else if (key_item & IT_KEY2)
	{
		if(MGH_keys_gold <= 0) return; MGH_keys_gold--;
		if (MGH_keys_gold == 0)
			MGH_horde_key_remove_all(IT_KEY2);
	}
	MGH_horde_print_keys();
}

void() MGH_horde_key_touch =
{
	if (other.classname != "player")
		return;
	if (other.health <= 0)
		return;


	sprint(other, "You got ");
	sprint(other, self.netname);
	sprint(other, "\n");

	sound (other, CHAN_ITEM, self.noise, 1, ATTN_NORM);
	stuffcmd (other, "bf\n");

	MGH_horde_key_give(self.items);

	// find the horde manager and trigger the next wave
	if (MGH_key_spawned)
	{
		dprint("key triggers next wave\n");
		MGH_horde_ent.wait = TRUE;
		MGH_horde_ent.think = MGH_Countdown;
		MGH_horde_ent.nextthink = time;
	}
	else
		dprint("key shouldn't trigger next wave!\n");
	/*
	local entity t = find(world, classname, "horde_manager");
	if (t != world)
	{
		t.wait = TRUE;
		t.think = MGH_Countdown;
		t.nextthink = time;
	}
	*/
	VR_Carry_Forget(self); remove(self);
}

void() MGH_SpawnAmmo =
{
	local float r, is_big;
	local entity item;
	local vector pos;
	pos = self.origin;
	item = spawn();

	// roll a d4, a 1 means big ammo.
	is_big = (random() * 4) <= 1;

	if (is_big)
		pos = pos - '16 16 0';
	else
		pos = pos - '12 12 0';

	// NEW: roll a d20, 1-8 is shells, 9-14 is spikes, 14-17 is rockets, 18-20 is cells
	// OLD: roll a d12, 1-5 is shells, 6-10 is spikes, 11 is rockets, 12 is cells
	/*
		ITEM		New %		Old %
		shells		40%			41.66%
		spikes		30%			33.33%
		rockets		20%			8.33 %
		cells		10%			8.33 %
	*/
	r = random() * 20;
	if (r <= 7) // shells
	{
		item.classname = "item_shells";
		if (is_big)
		{
			setmodel (item, "maps/b_shell1.bsp");
			item.aflag = 40;
		}
		else
		{
			setmodel (item, "maps/b_shell0.bsp");
			item.aflag = 20;
		}
		item.weapon = 1;
		item.netname = "$qc_shells";
	}
	else if (r <= 14) // spikes
	{
		item.classname = "item_spikes";
		if (is_big)
		{
			setmodel (item, "maps/b_nail1.bsp");
			item.aflag = 50;
		}
		else
		{
			setmodel (item, "maps/b_nail0.bsp");
			item.aflag = 25;
		}
		item.weapon = 2;
		item.netname = "$qc_nails";
	}
	else if (r <= 17) // rockets
	{
		item.classname = "item_rockets";
		if (is_big)
		{
			setmodel (item, "maps/b_rock1.bsp");
			item.aflag = 10;
		}
		else
		{
			setmodel (item, "maps/b_rock0.bsp");
			item.aflag = 5;
		}
		item.weapon = 3;
		item.netname = "$qc_rockets";
	}
	else // cells
	{
		item.classname = "item_cells";
		if (is_big)
		{
			setmodel (item, "maps/b_batt1.bsp");
			item.aflag = 12;
		}
		else
		{
			setmodel (item, "maps/b_batt0.bsp");
			item.aflag = 6;
		}
		item.weapon = 4;
		item.netname = "$qc_cells";
	}
	setorigin (item, pos + '0 0 1');
	setsize (item, '0 0 0', '32 32 56');
	item.MGH_pickup_spawn = self;
	self.wait = TRUE;
	item.movetype = MOVETYPE_TOSS;
	item.solid = SOLID_TRIGGER;
	item.flags = FL_ITEM;
	item.touch = MGH_horde_ammo_touch; MGH_PlacePickup(item, 1);

	MGH_SilentFog(pos + '0 0 8'); // added aug30

	//item.think = PlaceItem;
	//item.nextthink = time + 0.1;
}
void() MGH_SpawnItem =
{
	local float r;
	local entity item;
	local vector pos;
	item = spawn();

	pos = self.origin; // set to origin, then offset as needed

	r = random() * 6;
	if (r < 3) // rotten health
	{
		item.classname = "item_health";
		item.touch = MGH_horde_health_touch;
		setmodel (item, "maps/b_bh10.bsp");
		item.noise = "items/r_item1.wav";
		item.healamount = 15;
		item.healtype = 0;
		setsize (item, '0 0 0', '32 32 56');
		//pos = pos - '16 16 0';
	}
	if (r < 5) // regular health
	{
		item.classname = "item_health";
		item.touch = MGH_horde_health_touch;
		setmodel (item, "maps/b_bh25.bsp");
		item.noise = "items/health1.wav";
		item.healamount = 25;
		item.healtype = 1;
		setsize (item, '0 0 0', '32 32 56');
		pos = pos - '16 16 0';
	}
	else // armor
	{
		item.classname = "item_armor1";
		item.touch = MGH_horde_armor_touch;
		item.armortype = 0.3;
		item.armorvalue = 100;
		setmodel (item, "progs/armor.mdl");
		item.skin = 0;
		setsize (item, '-16 -16 0', '16 16 56');
	}

	setorigin (item, pos + '0 0 1');
	item.MGH_pickup_spawn = self;
	self.wait = TRUE;
	item.movetype = MOVETYPE_TOSS;
	item.solid = SOLID_TRIGGER;
	item.flags = FL_ITEM;
	//item.think = PlaceItem;
	//item.nextthink = time + 0.1;
	MGH_SilentFog(pos + '0 0 8'); MGH_PlacePickup(item, item.classname == "item_health" ? 2 : 3);
};

/* Spawn Key
A key should spawn after every boss wave.

*/
void(float is_gold) MGH_SpawnKey =
{
	local entity item;
	item = spawn();

	if (is_gold)
	{
		item.items = IT_KEY2;

		// set model and sound based on worldtype
		if (world.worldtype == 1)
		{
			item.netname = "$qc_gold_runekey";
			item.noise = "misc/runekey.wav";
			setmodel (item, "progs/m_g_key.mdl");
			MGH_CenterprintAll("$qc_horde_gold_runekey_appears");
		}
		else if (world.worldtype == 2)
		{
			item.netname = "$qc_gold_keycard";
			item.noise = "misc/basekey.wav";
			setmodel (item, "progs/b_g_key.mdl");
			MGH_CenterprintAll("$qc_horde_gold_keycard_appears");
		}
		else // assume medieval
		{
			item.netname = "$qc_gold_key";
			item.noise = "misc/medkey.wav";
			setmodel (item, "progs/w_g_key.mdl");
			MGH_CenterprintAll("$qc_horde_gold_key_appears");
		}
	}
	else
	{
		item.items = IT_KEY1;

		// set model and sound based on worldtype
		if (world.worldtype == 1)
		{
			item.netname = "$qc_silver_runekey";
			item.noise = "misc/runekey.wav";
			setmodel (item, "progs/m_s_key.mdl");
			MGH_CenterprintAll("$qc_horde_silver_runekey_appears");
		}
		else if (world.worldtype == 2)
		{
			item.netname = "$qc_silver_keycard";
			item.noise = "misc/basekey.wav";
			setmodel (item, "progs/b_s_key.mdl");
			MGH_CenterprintAll("$qc_horde_silver_keycard_appears");
		}
		else // assume medieval
		{
			item.netname = "$qc_silver_key";
			item.noise = "misc/medkey.wav";
			setmodel (item, "progs/w_s_key.mdl");
			MGH_CenterprintAll("$qc_horde_silver_key_appears");
		}

	}
	//item.noise = "misc/medkey.wav";
	setsize (item, '-16 -16 -25', '16 16 32');
	item.touch = MGH_horde_key_touch; item.vr_itemId=is_gold ? IID_KEY2 : IID_KEY1;
	//item.think = PlaceItem;
	//item.nextthink = time + 0.2;
	item.target = "horde_manager"; // when used, retrigger the horde manager
	item.flags = FL_ITEM;		// make extra wide
	//item.flags = item.flags & FL_NOBOTS; // AY 11 Nov 2021 so bots can't get keys
	item.solid = SOLID_TRIGGER;
	item.movetype = MOVETYPE_TOSS;
	setorigin (item, self.origin + '0 0 32');
	item.velocity = '0 0 255';

	// AY Feb22, 2022, rune keys shouldn't have light effects on them
	if (world.worldtype != 1)
		item.effects = EF_BRIGHTLIGHT;

	//dprint("     spawned key\n");
	spawn_tfog(item.origin);
	MGH_key_spawned = 1; MGH_PlacePickup(item, 4);
};

void() MGH_SpawnGoldKey =
{
	MGH_SpawnKey(TRUE);
};

void() MGH_SpawnSilverKey =
{
	MGH_SpawnKey(FALSE);
};

// Function get a key given the current wave
// Wave 3 should return a key with spawnflag "KEY_FIRST"
// Wave 6 should return a key with spawnflag "KEY_SECOND"
// Wave 9 should return a key with spawnflag "KEY_THIRD"
// Wave 9+ should return a key with spawnflag "KEY_FOURTH_PLUS"
// If a key with the correct spawnflag isn't found, return any key
void() MGH_GetKey =
{
	local entity t, l;
	t = find(world, classname, "info_horde_key");
	l = t; // save ref for later

	// check if any keys exist
	if (t == world)
	{
		dprint("ERROR: No info_horde_key!\n");
		// continue as if player got the key
		self.think = MGH_Countdown;
		self.nextthink = time + 4;
		return;
	}

	while(t != world)
	{
		if (self.wave <= 3) // looking for MGH_KEY_FIRST
		{
			if (t.spawnflags & MGH_KEY_FIRST)
			{
				t.think = MGH_SpawnSilverKey;
				t.nextthink = time;
				return;
			}
		}
		else if (self.wave <= 6) // looking for MGH_KEY_SECOND
		{
			if (t.spawnflags & MGH_KEY_SECOND)
			{
				t.think = MGH_SpawnSilverKey;
				t.nextthink = time;
				return;
			}
		}
		else if (self.wave <= 9) // looking for MGH_KEY_THIRD
		{
			if (t.spawnflags & MGH_KEY_THIRD)
			{
				t.think = MGH_SpawnGoldKey;
				t.nextthink = time;
				return;
			}
		}
		else // looking for MGH_KEY_FOURTH_PLUS
		{
			// check next key
			if (t.spawnflags & MGH_KEY_FOURTH_PLUS)
			{
				t.think = MGH_SpawnSilverKey;
				t.nextthink = time;
				return;
			}
		}
		t = find(t, classname, "info_horde_key");
	}

	// didn't find key with matching spawnflag, return any key
	dprint("didn't find key with matching spawnflag. Return last: ");
	dprint(l.classname);

	if (self.wave == 9)
		l.think = MGH_SpawnGoldKey;
	else
		l.think = MGH_SpawnSilverKey;
	l.nextthink = time;
};


//============================================================================
/*
MGH_SpawnSquad2

Yoder FEB02 2022
*/
void(string name, vector org, vector temp_angles) MGH_SpawnSquad2 =
{
	if (name == "3 grunts")
	{
		if (skill > 0)
		{
			MGH_SpawnMonster("grunt", org + '0 -40 0', temp_angles);
			MGH_SpawnMonster("grunt", org + '40 40 0', temp_angles);
			MGH_SpawnMonster("grunt", org + '-40 40 0', temp_angles);
		}
		else
		{
			MGH_SpawnMonster("grunt", org + '-40 0 0', temp_angles);
			MGH_SpawnMonster("grunt", org + '40 0 0', temp_angles);
		}
	}
	else if (name == "2 grunts, 1 dog")
	{
		MGH_SpawnMonster("dog", org + '44 0 0', temp_angles);
		MGH_SpawnMonster("grunt", org + '-40 -40 0', temp_angles);
		MGH_SpawnMonster("grunt", org + '-40 40 0', temp_angles);
	}
	else if (name == "2 dogs")
	{
		if (skill > 0)
		{
			MGH_SpawnMonster("dog", org + '-0 -44 0', temp_angles);
			MGH_SpawnMonster("dog", org + '0 44 0', temp_angles);
		}
		else
			MGH_SpawnMonster("dog", org + '-0 -44 0', temp_angles);
	}
	else if (name == "1 enforcer")
	{
		MGH_SpawnMonster("enforcer", org + '0 0 0', temp_angles);
	}
	else if (name == "2 enforcers")
	{
		if (skill > 0)
		{
			MGH_SpawnMonster("enforcer", org + '40 0 0', temp_angles);
			MGH_SpawnMonster("enforcer", org + '-40 0 0', temp_angles);
		}
		else
			MGH_SpawnMonster("enforcer", org + '0 0 0', temp_angles);
	}
	else if (name == "1 ogre")
	{
		MGH_SpawnMonster("ogre", org + '0 0 0', temp_angles);
	}
	else if (name == "2 knights")
	{
		if (skill > 0)
		{
			MGH_SpawnMonster("knight", org + '40 0 0', temp_angles);
			MGH_SpawnMonster("knight", org + '-40 0 0', temp_angles);
		}
		else
		{
			MGH_SpawnMonster("knight", org + '40 0 0', temp_angles);
		}

	}
	else if (name == "2 zombies")
	{
		MGH_SpawnMonster("zombie", org + '40 0 0', temp_angles);
		MGH_SpawnMonster("zombie", org + '-40 0 0', temp_angles);
	}
	else if (name == "1 wizard")
		MGH_SpawnMonster("wizard", org + '0 0 0', temp_angles);
	else if (name == "2 hellknights")
	{
		if (skill > 0)
		{
			MGH_SpawnMonster("hellknight", org + '0 40 0', temp_angles);
			MGH_SpawnMonster("hellknight", org + '0 -40 0', temp_angles);
		}
		else
			MGH_SpawnMonster("hellknight", org + '0 40 0', temp_angles);
	}
	else if (name == "2 knights, 1 hellknight")
	{
		MGH_SpawnMonster("hellknight", org + '40 0 0', temp_angles);
		MGH_SpawnMonster("knight", org + '-40 40 0', temp_angles);
		MGH_SpawnMonster("knight", org + '-40 -40 0', temp_angles);
	}
	else if (name == "3 wizards")
	{
		if (skill > 0)
		{
			MGH_SpawnMonster("wizard", org + '40 40 40', temp_angles);
			MGH_SpawnMonster("wizard", org + '-40 40 40', temp_angles);
			MGH_SpawnMonster("wizard", org + '-40 -40 40', temp_angles);
		}
		else
		{
			MGH_SpawnMonster("wizard", org + '40 40 40', temp_angles);
			MGH_SpawnMonster("wizard", org + '-40 40 40', temp_angles);
		}
	}
	else if (name == "shambler")
		MGH_SpawnMonster("shambler", org, temp_angles);
	else if (name == "double demon")
	{
		if (skill >= 3 && random() > 0.8)
		{
			// double shambler
			MGH_SpawnMonster("shambler", org + '40 40 0', temp_angles);
			MGH_SpawnMonster("shambler", org + '-40 -40 0', temp_angles);
		}
		else if (skill >= 1)
		{
			MGH_SpawnMonster("demon", org + '40 40 0', temp_angles);
			MGH_SpawnMonster("demon", org + '-40 -40 0', temp_angles);
		}
		else
			MGH_SpawnMonster("demon", org, temp_angles);
	}
	else if (name == "shalrath")
		MGH_SpawnMonster("shalrath", org, temp_angles);
	else
		dprint("ERROR: unrecognized squad name!\n");

	spawn_tfog(org);
};

/*
MGH_HordeFindSpawnpoint

Yoder February 2nd 2022
*/
float MGH_HORDE_SQUAD_TYPE_NORMAL = 0;
float MGH_HORDE_SQUAD_TYPE_RANGED = 1;
float MGH_HORDE_SQUAD_TYPE_FLYING = 2;
float MGH_HORDE_SQUAD_TYPE_BOSS = 3;

float MGH_HORDE_SQUAD_CAT_ERROR = -1;
float MGH_HORDE_SQUAD_CAT_FODDER = 0;
float MGH_HORDE_SQUAD_CAT_ELITE = 1;
float MGH_HORDE_SQUAD_CAT_BOSS = 2;

entity(float squad_type)MGH_HordeFindSpawnpoint =
{
	local float spawnpoint, temp_spawncount, loopcount, randomspawn;
	local float temp_spawncount_valid; // how many of the spawn points are valid
	local entity t;
	spawnpoint = 0;
	loopcount = 0;
	string squad_class;

	if (squad_type == MGH_HORDE_SQUAD_TYPE_BOSS)
		squad_class = "info_monster_start_boss";
	else if (squad_type == MGH_HORDE_SQUAD_TYPE_FLYING)
		squad_class = "info_monster_start_flying";
	else if (squad_type == MGH_HORDE_SQUAD_TYPE_RANGED)
		squad_class = "info_monster_start_ranged";
	else
		squad_class = "info_monster_start";

	// STEP 1: count all spawns of spawn type
	temp_spawncount = 0;
	temp_spawncount_valid = 0;

	// count the spawnpoints for squad_class
	t = find(world, classname, squad_class);
	while (t)
	{
		temp_spawncount++;
		if ((time > t.wait) && (!(t.spawnflags & MGH_START_OFF)) &&
			(!MGH_CheckBlockedSpawn(t)))
			temp_spawncount_valid++;
		t = find(t, classname, squad_class);
	}

	// verify against 0 valid spawns
	if (temp_spawncount_valid <= 0)
	{
		if (squad_type == MGH_HORDE_SQUAD_TYPE_NORMAL)
		{
			dprint("HordeFindSpawnPoint: FOUND 0 Valid spawns\n");
			return world; // already on fallback option, oops
		}
		else
		{
			squad_type = MGH_HORDE_SQUAD_TYPE_NORMAL;
			squad_class = "info_monster_start";
			t = world;
			loopcount = 1;
		}
	}

	// count again, but for Normal spawn points?
	if (loopcount)
	{
		temp_spawncount = 0;
		temp_spawncount_valid = 0;

		t = find(world, classname, squad_class);
		while (t)
		{
			temp_spawncount++;
			if ((time > t.wait) && (!(t.spawnflags & MGH_START_OFF)) &&
				(!MGH_CheckBlockedSpawn(t)))
				temp_spawncount_valid++;
			t = find(t, classname, squad_class);
		}
	}

	// verify against 0 valid spawns
	if (temp_spawncount_valid <= 0)
	{
		dprint("HordeFindSpawnPoint: FOUND 0 Valid spawns after recheck\n");
		return world; // already on fallback option, oops
	}

	// pick random spawncount
	loopcount = 0;
	randomspawn = temp_spawncount * random();
	dprint("randomspawn: ");dprint(ftos(randomspawn));
	dprint(" | whole count: ");dprint(ftos(temp_spawncount));
	dprint(" | valid count: ");dprint(ftos(temp_spawncount_valid));dprint("\n");
	t = find (world, classname, squad_class);
	while(t)
	{
		spawnpoint++;
		if (spawnpoint >= randomspawn)
		{
			if (!(t.spawnflags & MGH_START_OFF) && (time > t.wait) && (!(MGH_CheckBlockedSpawn(t))))
			{
				dprint("picked spawnpoint: ");dprint(ftos(spawnpoint));dprint("\n");
				return t;
			}
		}
		t = find(t, classname, squad_class);

		if (!t)
		{
			if (loopcount)
				dprint("something has gone horribly wrong!\n");
			else
			{
				loopcount = 1;

				t = find(t, classname, squad_class);
				spawnpoint = 0;
				randomspawn = 0; // allow any valid spawn
			}
		}
	}
	return world;
};
/*
MGH_SpawnWave2

Instead of finding a spawn point and then spawning a squad,
decide a squad, and then find a spawn point.
Yoder February 2nd 2022

*/

void() MGH_SpawnWave2 =
{
	local entity t, oself; // t is the temp spawn, oself is for temp storage of self
	local float squad_type = 0;
	local string squad_name = ""; // specific squad name
	local float r; // random roll
	local float squad_cat = 0; //

	//STEP 1: determine what kind of monster is getting spawned
	if (self.army)
	{

		if (self.fodder > 0)
		{
			r = random() * 4;
			if (r < 1)
				squad_name = "3 grunts";
			else if (r < 2)
				squad_name = "2 grunts, 1 dog";
			else if (r < 3.5)
				squad_name = "2 dogs";
			else
			{
				squad_name = "1 enforcer";
				squad_type = MGH_HORDE_SQUAD_TYPE_RANGED;
			}

			squad_cat = MGH_HORDE_SQUAD_CAT_FODDER;
		}
		else if (self.elites > 0)
		{
			squad_type = MGH_HORDE_SQUAD_TYPE_RANGED;
			r = random() * 2;
			if (r < 1.5)
				squad_name = "2 enforcers";
			else
				squad_name = "1 ogre";

			squad_cat = MGH_HORDE_SQUAD_CAT_ELITE;
		}
		else if (self.bosses > 0)
		{
			dprint("ERROR: should not spawn boss squad in army wave\n");
			self.bosses = 0;
			squad_cat = MGH_HORDE_SQUAD_CAT_ERROR;
		}
	}
	else // NON-ARMY
	{
		if (self.fodder > 0)
		{
			r = random() * 4;
			if (r < 2)
				squad_name = "2 knights";
			else if (r < 3)
				squad_name = "2 zombies";
			else
			{
				squad_name = "1 wizard";
				squad_type = MGH_HORDE_SQUAD_TYPE_FLYING;
			}

			squad_cat = MGH_HORDE_SQUAD_CAT_FODDER;
		}
		else if (self.elites > 0)
		{
			r = random() * 4;
			if (r < 1)
				squad_name = "2 hellknights";
			else if (r < 2)
				squad_name = "2 knights, 1 hellknight";
			else if (r < 3)
			{
				squad_name = "1 ogre";
				squad_type = MGH_HORDE_SQUAD_TYPE_RANGED;
			}
			else
			{
				squad_name = "3 wizards";
				squad_type = MGH_HORDE_SQUAD_TYPE_FLYING;
			}

			squad_cat = MGH_HORDE_SQUAD_CAT_ELITE;
		}
		else if (self.bosses > 0)
		{
			squad_type = MGH_HORDE_SQUAD_TYPE_BOSS;
			r = random() * 3;
			if (r < 1)
				squad_name = "shambler";
			else if (r < 2.5)
			{
				squad_name = "double demon";
				squad_type = MGH_HORDE_SQUAD_TYPE_NORMAL;
			}
			else
				squad_name = "shalrath";

			squad_cat = MGH_HORDE_SQUAD_CAT_BOSS;
		}
	}


	//STEP 2: find a spawn point for the squad
	t = MGH_HordeFindSpawnpoint(squad_type);
	dprint("spawnpoint found was: ");dprint(t.classname);dprint("\n");
	if (t && (squad_cat != MGH_HORDE_SQUAD_CAT_ERROR))
	{
		t.wait = time + MGH_SPAWN_RESET_TIME; // block spawnpoint from reuse for a duration

		// use targets
		oself = self;
		self = t;
		SUB_UseTargets();
		self = oself;

		// set angles, just in case
		if (!t.angles)
			t.angles = '0 0 0';

		MGH_SpawnSquad2(squad_name, t.origin, t.angles);
		WriteByte(MSG_ALL, SVC_UPDATESTAT);
		WriteByte(MSG_ALL, 12); // 12 = STAT_TOTALMONSTERS
		WriteLong(MSG_ALL, total_monsters);

		if (squad_cat == MGH_HORDE_SQUAD_CAT_FODDER)
			self.fodder--;
		else if (squad_cat == MGH_HORDE_SQUAD_CAT_ELITE)
			self.elites--;
		else if (squad_cat == MGH_HORDE_SQUAD_CAT_BOSS)
			self.bosses--;

		if ((self.fodder + self.elites + self.bosses) <= 0) // max spawns hit
		{
			dprint("wave spawn completed!\n");
			self.wait = FALSE;
			self.think = MGH_Wavecheck;
			self.nextthink = time + 30;
		}
		else
		{
			// normal spawnwave wait time
			self.think = MGH_SpawnWave2;
			self.nextthink = time + 1 + random() + 1;
		}
	}
	else
	{
		dprint("no valid spawns, wait a moment\n");
		self.think = MGH_SpawnWave2;
		self.nextthink = time + 1;
	}
};

/* MGH_SpawnWavePrep
called once at the countdown starts
determines the number of squads by type
also spawns ammo and items
*/
float() MGH_SpawnWavePrep =
{
	local entity p;
	local float playercount, playerscalar;
	// Reset Key Spawn
	MGH_key_spawned = FALSE;

	// Respawn dead players!
	if (MGH_HordeGetPlayersAlive() < 1)
	{
		dprint("WARNING: Last player died at round end, don't start new round!\n");
		return 0;
	}

	// Items
	p = find (world, classname, "info_horde_item");
	while (p)
	{
		if (!p.wait)
		{
			p.think = MGH_SpawnItem;
			p.nextthink = time + random() * 2;
		}
		p = find (p, classname, "info_horde_item");
	}

	self.wave++;
	dprint("wave ");
	dprint(ftos(self.wave));
	dprint("\n");

	// See spreadsheet for notes on squad scaling

	// AY FEB07 scaling start wave based on difficulty
	local float temp_wave;
	if (skill >= 3)
		temp_wave = self.wave + 6;
	else if (skill >= 2)
		temp_wave = self.wave + 3;
	else
		temp_wave = self.wave;

	// Determine if Army wave
	if ((temp_wave + 2) % 3 == 0)
		self.army = TRUE;
	else
		self.army = FALSE;

	// player scalar
	playercount = MGH_HordeGetPlayersAlive();
	if (playercount >= 4)
		playerscalar = 2;
	else if (playercount >= 3)
		playerscalar = 1.5;
	else if (playercount >= 2)
		playerscalar = 1.25;
	else
		playerscalar = 1;

	// Boss Count
	if (temp_wave % 3 == 0)
	{
		self.bosses = floor((temp_wave+1)/4);
	}
	else if ((skill > 1) && (!self.army) && (temp_wave > 9))
	{
		//AY feb07, some extra spice
		self.bosses = floor((temp_wave+1)/8);
	}

	// Elite Count
	self.elites = ceil((temp_wave - 1)/3) - floor(self.bosses/2);
	self.elites = floor(self.elites * playerscalar);

	// Fodder Count
	self.fodder = (temp_wave + 2) - (self.bosses * 2 + self.elites);
	self.fodder = floor(self.fodder * playerscalar);

	// Trigger map entities
	local entity stemp;
	p = find (world, targetname, self.target);
	while(p)
	{
		stemp = self;
		self = p;
		if (self.use != SUB_Null)
		{
			if (self.use)
			self.use ();
		}
		self = stemp;
		p = find (p, targetname, self.target);
	}

	self.wait = TRUE;
	return 1;
}

void() MGH_Countdown4 =
{
	if (self.wave % 3 == 0)
		MGH_CenterprintAll("$qc_horde_boss_wave");
	else
		MGH_CenterprintAll("$qc_horde_fight");
	MGH_SpawnWave2();
	sound(self, CHAN_VOICE, "misc/talk.wav", 1, ATTN_NONE);
};
void() MGH_Countdown3 =
{
	MGH_CenterprintAll("1");
	self.think = MGH_Countdown4;
	self.nextthink = time + 1;
	sound(self, CHAN_VOICE, "misc/talk.wav", 1, ATTN_NONE);
};
void() MGH_Countdown2 =
{
	MGH_CenterprintAll("2");
	self.think = MGH_Countdown3;
	self.nextthink = time + 1;
	sound(self, CHAN_VOICE, "misc/talk.wav", 1, ATTN_NONE);
};
void() MGH_Countdown =
{
	if (!MGH_SpawnWavePrep())
		return;

	MGH_CenterprintAll("3");
	self.think = MGH_Countdown2;
	self.nextthink = time + 1;

	sound(self, CHAN_VOICE, "misc/talk.wav", 1, ATTN_NONE);
};

void() MGH_RespawnAllPlayers =
{
	local entity p, oself;
	p = find (world, classname, "player");
	while (p)
	{
		if (p.deadflag > 0)
		{
			oself = self;
			self = p;
			MGH_Revive();
			self = oself;
			//p.think = horde_respawn_teammate;
			//p.nextthink = time; // next frame
		}
		p = find (p, classname, "player");
	}
};

/*
When each enemy spawned by the horde manager dies, they trigger self.owner.use, which is wavecheck.
MGH_Wavecheck sees if all spawned monsters are dead, so it can determine if it needs to start a new wave.
*/
void() MGH_Wavecheck =
{
	if (self.wait) // still in the process of spawning monsters, don't wavecheck
		return;

	self.think = MGH_Wavecheck;
	self.nextthink = time + 10;

	dprint("\n===============\n");
	dprint("checking kills: ");
	dprint(ftos(killed_monsters));
	dprint(" of ");
	dprint(ftos(total_monsters));
	dprint("\n");

	// debug checking last monster position
	if (killed_monsters + 3 >= total_monsters)
	{
		local entity t;
		t = find(world, MGH_category, MGH_CATEGORY_MONSTER);
		while(t)
		{
			if (t.health > 0)
			{
				dprint("Position of monster: ");
				dprint(t.classname);
				dprint(" : ");
				dprint(vtos(t.origin));
				dprint("\n");
			}
			else
				t.MGH_category = string_null;
			t = find(t, MGH_category, MGH_CATEGORY_MONSTER);
		}
	}

	// Early exit for kill count
	if ((self.wave % 3 == 0) || (self.wave < 3)) // Be exact?
	{
		dprint("check monsters killed as boss wave\n");
		if (MGH_HordeGetMonstersAlive() > 0) // testing the new way
			return;
		//if (killed_monsters < total_monsters)
		//	return;
	}
	else
	{
		dprint("check monsters killed as any other wave\n");
		if (MGH_HordeGetMonstersAlive() > 5)
			return;
		//if (killed_monsters < (total_monsters -5))
		//	return;
	}

	// Made it this far, means wave completed successfully
	dprint("wavecheck looks good! Respawning players\n");
	self.wait = 1; // Revival telefrags cannot re-enter this completed wave.
	MGH_RespawnAllPlayers();
	self.think = MGH_Countdown;

	if (self.wave % 3 == 0) // it was a boss wave
	{
		if(!MGH_key_spawned) // key was already spawned, don't spawn another yet!
			MGH_GetKey();

		self.nextthink = time + 20; // in case the players are slow to find the key
	}
	else
	{
		self.nextthink = time;
	}
	dprint("wavecheck now completed.\n");
};

// setup horde rules
void() MGH_SetHorde =
{
	dprint("TEST: It is September 23, 2021!\n"); // yoder sanity test
	entity m = nextent(world); while(m) { if((m.flags & FL_MONSTER) && m.health > 0 && !m.MG_deferred) m.MGH_category=MGH_CATEGORY_MONSTER; m=nextent(m); }
 self.think = MGH_Countdown;
	self.nextthink = time + 1;
}

// define entity
/*QUAKED horde_manager (1 0 0) (-8 -8 -8) (8 8 8)
Machine Horde manager with source waves, rewards, and spawn selection.
*/
void() horde_manager =
{
	if(!MG1_Campaign() || deathmatch) { remove(self); return; }

	MGH_PrecacheMonsters();

	MGH_horde_ent = self; MGH_initial_serverflags = serverflags; // set global reference to the horde manager

	if (!self.target)
	self.target = "horde_event"; // default name

	self.targetname = "horde_manager";
	self.think = MGH_SetHorde;
	self.nextthink = time + MGH_HORDE_START_DELAY;
	self.use = MGH_Wavecheck;

	self.wait = 1; // waiting before next wave?
	self.delay = 9; // max number of  squads to spawn in a wave
	self.wave = 0;

	//if (!horde)
	//	horde = 1; // in case CVAR isn't already set

};

void() MGH_PowerupFade =
{
	if (self.alpha > 0)
	{
		self.alpha = self.alpha - 0.25 * frametime;
		self.nextthink = time; // think next frame
	}
	else
	{
		VR_Carry_Forget(self); remove(self);
	}
}

void() MGH_horde_powerup_think =
{
	if (self.velocity_z < 0)
	{
		dprint("WARNING: Powerup fell out of world. Remove\n");
		VR_Carry_Forget(self); remove(self);
	}
	else
	{
		self.alpha = 1;
		self.think = MGH_PowerupFade;
		self.nextthink = time;
	}
};

void() MGH_horde_spawn_powerup =
{
	if (!MGH_powerup_chance)
		MGH_powerup_chance = MGH_DEFAULT_POWERUP_CHANCE;

	if (random() < MGH_powerup_chance) // "if(1)" to guarantee powerup drop
	{
		dprint("powerup chance was: ");
		dprint(ftos(MGH_powerup_chance));
		dprint("\n");

		MGH_powerup_chance = MGH_DEFAULT_POWERUP_CHANCE;

		local entity powerup = spawn();
		setorigin (powerup, self.origin + '0 0 0');

		//setsize (powerup, '-16 -16 -24', '16 16 32');
		powerup.flags = FL_ITEM;
		powerup.solid = SOLID_TRIGGER;
		powerup.movetype = MOVETYPE_BOUNCE;
		powerup.velocity = '0 0 300';

		//powerup.velocity_x = crandom() * 64;
		//powerup.velocity_y = crandom() * 64;
		powerup.touch = MGH_powerup_touch; powerup.handtouch=MGH_powerup_touch; powerup.carry_use=MGH_powerup_touch; powerup.MGH_powerup_expire=time+14;
		powerup.think = SUB_Null;
		powerup.nextthink = 0;

		if (random() < 0.25) // spawn pentagram (invulnerability)
		{
			powerup.noise = "items/protect.wav";
			setmodel(powerup, "progs/invulner.mdl");
			powerup.netname = "$qc_pentagram_of_protection";
			powerup.items = IT_INVULNERABILITY; powerup.vr_itemId = IID_INVULNERABILITY;
			powerup.classname = "item_artifact_invulnerability";
		}
		else // spawn quad damage
		{
			powerup.noise = "items/damage.wav";
			setmodel(powerup, "progs/quaddama.mdl");
			powerup.netname = "$qc_quad_damage";
			powerup.items = IT_QUAD; powerup.vr_itemId = IID_QUAD;
			powerup.classname = "item_artifact_super_damage";
		}
		//powerup.effects = EF_DIMLIGHT;

		setsize (powerup, '-12 -12 -12', '12 12 12'); entity caller=self; self=powerup; VR_PickupObj_Mark(QVR_PICKUPOBJ_HOLSTER); self=caller;
	}
	else
		MGH_powerup_chance = MGH_powerup_chance + MGH_POWERUP_CHANCE_GAIN;
};

// remote wavecheck
// this is to trigger a wavecheck from outside of horde mode
void() MGH_remote_wavecheck =
{
	if(!MGH_horde_ent || (intermission_running))
	{
		dprint("no wavecheck.");
		if (!MGH_horde_ent)
			dprint(" no horde ent found.");
		if (intermission_running)
			dprint(" intermission running.");
		dprint("\n\n");
		return;
	}
	dprint("remote wavecheck from: ");
	dprint(self.classname);
	dprint("\n");
	local entity stemp = self; // the entity that is triggering the remote wavecheck
	self = MGH_horde_ent;
	self.use();

	self = stemp; // set self back to whatever it was
};

/* MGH_CheckBlockedSpawn
AY Feb24, 2022
Checks the horde spawn against all living players
Returns true/false if spawn is blocked.
*/
float(entity spawnpoint) MGH_CheckBlockedSpawn =
{
	local float blocked = FALSE;
	local entity p;
	local vector p_mins, p_maxs, s_mins, s_maxs;

	if (spawnpoint.spawnflags & MGH_SKIP_BLOCK_CHECK)
		return FALSE;

	p = find(world, classname, "player");
	while(p && !blocked)
	{
		if ((p.health > 0) && (p.deadflag <= 0))
		{
			p_mins = p.origin + p.mins;
			p_maxs = p.origin + p.maxs;
			s_mins = spawnpoint.origin + spawnpoint.mins;
			s_maxs = spawnpoint.origin + spawnpoint.maxs;

			if (((p_maxs_x > s_mins_x) && (p_mins_x < s_maxs_x)) &&
				((p_maxs_y > s_mins_y) && (p_mins_y < s_maxs_y)) &&
				((p_maxs_z > s_mins_z) && (p_mins_z < s_maxs_z)))
			{
				dprint("player blocking spawn\n");
				blocked = TRUE;
			}
		}
		p = find(p, classname, "player");
	}
	return blocked;
};
float() MGH_Active = { return MG1_Campaign() && MGH_horde_ent != world; };
void(string text) MGH_CenterprintAll = {
 entity p = find(world,classname,"player"); while(p) { centerprint(p,text); p=find(p,classname,"player"); }
};
void(vector org) MGH_SilentFog = {
 WriteByte(MSG_BROADCAST,SVC_TEMPENTITY); WriteByte(MSG_BROADCAST,TE_TELEPORT);
 WriteCoord(MSG_BROADCAST,org_x); WriteCoord(MSG_BROADCAST,org_y); WriteCoord(MSG_BROADCAST,org_z);
};
void(entity item, float kind) MGH_PlacePickup = {
 entity caller=self; self=item;
 item.handtouch=item.touch;
 if(kind == 1 || kind == 2) {
  if(kind == 1 ? cvar_hget(cvarh_vr_forcegrabbable_ammo_boxes) : cvar_hget(cvarh_vr_forcegrabbable_health_boxes)) MakeItemGrabbable();
  else VRScalePickup();
 } else {
  item.carry_use = item.handtouch;
  VR_PickupObj_Mark(kind == 3 ? QVR_PICKUPOBJ_ARMOUR : QVR_PICKUPOBJ_HOLSTER);
 }
 self=caller;
};
void() MGH_Revive = {
 // Preserve the existing instance records; the normal level serializer allocates replacements.
 entity mainrecord=self.weaponinst, offrecord=self.weaponinst2;
 entity record0=self.holsterweaponinst0;
 entity record1=self.holsterweaponinst1;
 entity record2=self.holsterweaponinst2;
 entity record3=self.holsterweaponinst3;
 entity record4=self.holsterweaponinst4;
 entity record5=self.holsterweaponinst5;
 self.health=self.max_health; SetChangeParms(); self.MGH_reviving=TRUE; PutClientInServer(); self.MGH_reviving=FALSE;
 self.weaponinst=mainrecord; self.weaponinst2=offrecord;
 self.holsterweaponinst0=record0;
 self.holsterweaponinst1=record1;
 self.holsterweaponinst2=record2;
 self.holsterweaponinst3=record3;
 self.holsterweaponinst4=record4;
 self.holsterweaponinst5=record5;
 // The engine reclaims the serializer's unreferenced temporary records at frame end.
 self.health=self.max_health; MGH_horde_set_keys(self); MGH_ResetHunger(self);
};
void() MGH_DeathThink = {
 // Called by PlayerDeathThink after its release-then-press gate. In coop, living teammates mean revival at the
 // wave boundary; otherwise (solo, or the whole team down) the source restarts the arena.
 if(coop && MGH_HordeGetPlayersAlive() > 0) return;
 // Entire team wiped: same authored arena, initial runes, fresh wave manager.
 if(MGH_restart_queued) return;
 MGH_restart_queued = TRUE;
 serverflags = MGH_initial_serverflags;
 // "restart fresh": the arena from its start as the official game does, never the last save (the engine's
 // sv_autoload loads it for a dead solo player; other modes keep that). Engines without the argument ignore it.
 localcmd("restart fresh\n");
};
void(entity p) MGH_ResetHunger = { if(MGH_Active() && (serverflags & 2)) p.MGH_hunger_time=time+10; };
void() MGH_RuneHunger = {
 if(!MGH_Active() || !(self.spawnflags & 2)) return;
 entity p=find(world,classname,"player"); while(p) { p.MGH_hunger_time=time+10; p=find(p,classname,"player"); }
};
void(entity attacker) MGH_Kill = {
 if(!MGH_Active()) return;
 if(self.classname == "player") {
  self.MGH_killspree=0;
  if(attacker.classname == "player" && attacker!=self) { attacker.MGH_killspree=0; attacker.MGH_killtime=0; attacker.frags-=2; }
  return;
 }
 if(!(self.flags & FL_MONSTER)) return;
 if(attacker.classname == "player") {
  attacker.frags++;
  if(time > attacker.MGH_killtime) attacker.MGH_killspree=0;
  attacker.MGH_killspree++; attacker.MGH_killtime=time+2;
  if(attacker.MGH_killspree >= 2) sprint(attacker,sprintf("Horde kill streak: %g\n",attacker.MGH_killspree));
 }
 self.MGH_corpse_expire=time+11+random()*5;
 MGH_horde_spawn_powerup();
 MGH_remote_wavecheck();
};

void() MGH_PowerupFrame = {
 if(!MGH_Active()) return;
 entity e=nextent(world); while(e) {
  entity next=nextent(e);
  if(e.MGH_corpse_expire > 0 && e.health <= 0) {
   if(time >= e.MGH_corpse_expire) { VR_Carry_Forget(e); VR_Head_StopFlies(e); remove(e); e=next; continue; }
   else if(time > e.MGH_corpse_expire-1) e.alpha=e.MGH_corpse_expire-time;
  }
  if(e.MGH_powerup_expire > 0) {
   if(time >= e.MGH_powerup_expire) { VR_Carry_Forget(e); remove(e); }
   else if(time > e.MGH_powerup_expire-4) e.alpha=(e.MGH_powerup_expire-time)*0.25;
  }
  e=next;
 }
};

entity() MGH_SelectSpawn = {
 if(!MGH_Active()) return world;
 entity spot=find(world,classname,"info_player_coop");
 while(spot) {
  entity thing=findradius(spot.origin,64); float blocked=FALSE;
  while(thing) { if(thing.classname=="player" && thing.health>0 && thing!=self) blocked=TRUE; thing=thing.chain; }
  if(!blocked) return spot;
  spot=find(spot,classname,"info_player_coop");
 }
 return world;
};

void() MGH_powerup_touch = {
 if(other.classname != "player" || other.health <= 0 || self.model == string_null) return;
 powerup_touch(); VR_Carry_Forget(self); remove(self);
};

void() MGH_MarkMonster = { if((self.flags & FL_MONSTER) && self.health > 0) self.MGH_category=MGH_CATEGORY_MONSTER; };

string() MGH_NextArena = {
 // Original source rotates four arenas; newer owned packs extend that sequence to seven.
 float current=0, i=1;
 while(i<=7) { if(mapname==sprintf("horde%g",i)) current=i; i++; }
 i=current+1; float tries=0;
 while(tries<7) { if(i>7) i=1; string name=sprintf("horde%g",i); if(fileexists(sprintf("maps/%s.bsp",name))) return name; i++; tries++; }
 return mapname;
};
