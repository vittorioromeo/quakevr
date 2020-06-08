/*
==============================================================================

ITEMS

items now set a lot of their own values directly rather than expect touch 
functions to check classnames

==============================================================================
*/

float	ITEM_SUSPENDED = 4;
/*FGD
@baseclass base(Appearflags, Target, Targetname) = Item
[
	message(string) : "Message"
	wait(integer) : "Respawn time" : 0
	count(integer) : "Respawn count" : 0
	spawnflags(flags) = 
	[
		4 : "Suspended" : 0
	]
]
*/

/*
================================
ItemFall
================================
*/
void() ItemFall =
{
	if (!(self.spawnflags & ITEM_SUSPENDED))
		return;
	self.solid = SOLID_TRIGGER;
	self.movetype = MOVETYPE_TOSS;	
	self.velocity = '0 0 1';
	self.spawnflags = not(self.spawnflags, ITEM_SUSPENDED);
}

/*
================================
ItemUse

triggering an item
 - if it regenerates, stop regenerating
 - if it doesn't, force it to respawn
items which have yet to be spawned have a different .use
================================
*/
void() ItemUse =
{
	if (self.wait > 0)
	{
		self.wait = 0;
		self.think = SUB_Remove;	// if nextthink is in the future, don't respawn again
		return;
	}
	self.alpha = 1;
	ItemPiff(self);
	ItemSpawn();
}

/*
================================
ItemPiff
throw some light teledust off an item when it respawns
================================
*/
void(entity i) ItemPiff =
{
	vector org;
	org = (i.absmin + i.absmax) * 0.5;
	org_z = i.absmin_z + 8;
	particle(org, '8 8 0', 8, 16);
	particle(org, '8 -8 0', 8, 16);
	particle(org, '-8 8 0', 8, 16);
	particle(org, '-8 -8 0', 8, 16);
}

/*
================================
ItemSpawn
================================
*/
void() ItemSpawn =
{
	// optimized away .mdl, recycled .wad as only the world uses it
	self.model = self.wad;		// restore original model
	self.solid = SOLID_TRIGGER;	// allow it to be touched again
	setorigin (self, self.origin);
	//ItemFall();

	
	if (self.targetname != string_null && self.use != SUB_regen)
		sound (self, CHAN_VOICE, "misc/r_tele1.wav", 1, ATTN_NORM);
	else
		sound (self, CHAN_VOICE, "items/itembk2.wav", 1, ATTN_NORM);	// play respawn sound
	
	if (self.wait)
		self.use = ItemUse;
}

/*
================================
SUB_regen
make the little effect on respawn but not initial spawn - there's too many cases where
I trigger items behind doors so they look like they were always there, and the piff
gives them away
================================
*/
void() SUB_regen = {ItemSpawn();}

/*
================================
ItemPrespawnFlicker
================================
*/
void() ItemPrespawnFlicker =
{
	if (time >= self.attack_finished)
	{
		self.alpha = 1;
		ItemPiff(self);
		ItemSpawn();
		return;
	}

	// flicker back into place
	//self.alpha = (3.5 - (self.attack_finished - time)) / 8 + (1 - mod(self.attack_finished - time, 0.5)) * 0.2;
	self.alpha = (0.5 - mod(self.attack_finished - time, 0.5));
	self.nextthink = time + 0.05;
}

/*
================================
ItemPrespawn
get ready to respawn by setting up a throbby fadein
================================
*/
void() ItemPrespawn =
{
	self.model = self.wad;		// restore original model
	setorigin(self, self.origin);
	
	self.think = ItemPrespawnFlicker;
	ItemPrespawnFlicker();
}

/*
================================
ItemTouched
================================
*/
void() ItemTouched =
{
	// wait > 0 == item respawns, either from deathmatch rules or entity keyvalue
	if (self.wait > 0)
	{
		//self.model = string_null;
		self.alpha = 0.2;
		self.solid = SOLID_NOT;
		
		self.think = ItemPrespawn;
		self.attack_finished = time + self.wait;
		self.nextthink = self.attack_finished - 2.45;
	}
	// wait -1 == respawn every time the item is triggered
	else if (self.wait < 0 || self.use == SUB_regen)	// maintain old respawn hacks
	{
		self.model = string_null;
		self.solid = SOLID_NOT;
		if (self.use != SUB_regen)
			self.use = ItemUse;
	}
	else
	{
		SUB_RemoveSoon();
	}
	
	if (self.count > 0)		// only respawn 'count' more times
	{
		self.count = self.count - 1;
		if (self.count == 0)
			SUB_RemoveSoon();	// can't remove during c touch/movement code
	}
	
	activator = other;
	SUB_UseTargets();
}



/*
================================
ItemRotateAndSize
jigger the origin and bounds of items with off-center origins so their models
appear in the center of their bboxes using MATHS
================================
*/
void() ItemRotateAndSize =
{
	if (self.angles_y == 0)
		return;
	if (self.mins != '0 0 0')
		return;	// not a silly corner-bbox item

	vector rot, orgofs;
	vector max = self.maxs;
	
	rot = SinCos(self.angles_y - 135);
	rot *= 22.6274;	// length of diagonal radius of 32u box
	orgofs_x = rot_y + 16;
	orgofs_y = rot_x + 16;
	orgofs_z = 0;
	
	setorigin(self, self.origin + orgofs);
	setsize(self, orgofs * -1, max - orgofs);
}

/*
================================
ItemDrop
plants the object on the floor or sometimes just destroys it completely
================================
*/
void() ItemDrop =
{
	float	oldz;
	self.movetype = MOVETYPE_TOSS;	
	self.velocity = '0 0 0';
	self.origin_z = self.origin_z + 6;
	oldz = self.origin_z;
	if (!droptofloor(0, 0))
	{
		dprint4(self.classname," fell out of level at ",vtos(self.origin),"\n");
		remove(self);
	}
}

/*
================================
ItemPlace
work out initial placement based on states
================================
*/
void() ItemPlace =
{
	self.flags |= FL_ITEM;		// make extra wide
	
	if (SUB_VerifyTriggerable())
	{	// hide them until triggered if triggerable
		self.solid = SOLID_NOT;
		self.model = string_null;
		self.use = ItemSpawn;
	}
	else
	{
		self.solid = SOLID_TRIGGER;
		SUB_ChangeModel(self,self.wad);
		if (self.use != SUB_regen)	// map hack back compat
			self.use = ItemUse;
	}
	
	if (self.spawnflags & ITEM_SUSPENDED)
	{
		//dprint("item is suspended\n");
		self.movetype = MOVETYPE_NONE;
	}	
	else
	{
		ItemDrop();
	}
	
	ItemRotateAndSize();	// must do this after item_drop, droptofloor is picky about the weird things we do
}

void() PlaceItem = {ItemPlace();}


/*
================================
StartItem
Sets the clipping size and drops to floor
================================
*/
void() StartItem =
{
	// putting this here means items that never appear in the level will precache
	// anyway, but this isn't a big deal and I'm lazy
	if (!SUB_ShouldSpawn()) return;
	
	if (!self.type)
		self.type = "item";
		
	self.wad = self.model;		// so it can be restored on respawn
	self.model = string_null;
	
	self.nextthink = time + 0.2;	// items start after other solids
	self.think = ItemPlace;
	
	ammo_total +=	self.ammo_shells * DEBUG_DMG_PER_SHELL +
					self.ammo_cells * DEBUG_DMG_PER_CELL +
					self.ammo_nails * DEBUG_DMG_PER_NAIL;
	num_rockets += self.ammo_rockets;
}


/*
===============================================================================

DROPPING

===============================================================================
*/

void(string tname) target_drop_do =
{
	if (tname == string_null)
		return;
		
	entity t;
	t = world;
	
	do	{
		t = find (t, targetname, tname);
		if (!t) break;
		
		SUB_CallAsSelf(ItemFall, t);
	} while (t);
}

void() target_drop_use =
{
	target_drop_do(self.target);
	target_drop_do(self.target2);
	target_drop_do(self.target3);
	target_drop_do(self.target4);
}

/*QUAKED target_drop (0.75 0.25 0) (-16 -16 -16) (16 16 16) ?
Causes all targeted items which are SUSPENDED to drop when triggered.
*/
/*FGD
@PointClass base(Appearflags, Target, Targetname) size(32 32 32) color(192 64 0) = target_drop : 
"Target Drop. Causes all targeted items which are suspended to drop when triggered." []
*/
void() target_drop =
{
	if (!SUB_ShouldSpawn()) return;
	self.use = target_drop_use;
}



/*
===============================================================================

GIVING/TAKING/CHECKING

===============================================================================
*/

float SPAWN_TGTITEMS_TAKE = 1;
float SPAWN_TGTITEMS_OVERRIDE = 2;
float SPAWN_TGTITEMS_FLASH = 4;
float SPAWN_TGTITEMS_TEST = 8;

/*FGD
@baseclass = TrigItems [
	svflags(flags) =
	[
		1 : "E1 complete" : 0
		2 : "E2 complete" : 0
		4 : "E3 complete" : 0
		8 : "E4 complete" : 0
		16 : "Unused" : 0
		32 : "Unused" : 0
		64 : "Unused" : 0
		128 : "Unused" : 0
	]
	items(flags) =
	[
		1 : "Shotgun" : 0
		2 : "Super Shotgun" : 0
		4 : "Nailgun" : 0
		8 : "Perforator" : 0
		16 : "Grenade Laucher" : 0
		32 : "Rocket Launcher" : 0
		64 : "Lightning Gun" : 0
		4096 : "Axe" : 0
		131072 : "Silver Key" : 0
		262144 : "Gold Key" : 0
		524288 : "Ring" : 0
		1048576 : "Pent" : 0
		2097152 : "Biosuit" : 0
		4194304 : "Quad" : 0
	]
	ammo_shells(integer) : "Shells" : 0
	ammo_nails(integer) : "Nails" : 0
	ammo_rockets(integer) : "Rockets" : 0
	ammo_cells(integer) : "Cells" : 0
	health(integer) : "Health" : 0
	armorvalue(integer) : "Armor" : 0
]
*/
void(entity e) target_items_setweapon =
{
	if (self.weapon)
	{
		if (self.weapon & e.items)
			e.weapon = self.weapon;
	}
	if (!(e.weapon & e.items))
		SUB_CallAsSelf(W_SelectBestWeapon, e);
}


void(entity e) target_items_giveinv =
{
	if (self.health)
		T_Heal(e, self.health, TRUE);
		
	if (self.armorvalue)
		armor_give(e, zeroconvert(self.armorvalue), FALSE);

	if (self.ammo_shells) e.ammo_shells += zeroconvert(self.ammo_shells);
	if (self.ammo_nails) e.ammo_nails += zeroconvert(self.ammo_nails);
	if (self.ammo_rockets) e.ammo_rockets += zeroconvert(self.ammo_rockets);
	if (self.ammo_cells) e.ammo_cells += zeroconvert(self.ammo_cells);
	bound_ammo(e);
	
	if (self.svflags)
		serverflags |= zeroconvert(self.svflags);

	if (self.items)
	{
		float it = zeroconvert(self.items);

		if (it & IT_WEAPONS)
			e.items |= it & IT_WEAPONS;
		
		// set artifact timers properly
		if (it & IT_INVISIBILITY)
			powerup_invisibility(e, self.length);
		if (it & IT_INVULNERABILITY)
			powerup_invulnerability(e, self.length);
		if (it & IT_SUIT)
			powerup_envirosuit(e, self.length);
		if (it & IT_QUAD)
			powerup_super_damage(e, self.length);
		
		if (it & IT_KEY1)
			key_give_silver(e);
		if (it & IT_KEY2)
			key_give_gold(e);
			
		target_items_setweapon(e);
	}
}

float(entity e) target_items_testinv =
{
	if (self.health > 0 && self.health > e.health) return FALSE;
	if (self.armorvalue && self.armorvalue > e.armorvalue) return FALSE;

	if (self.svflags)
	{
		float sf = zeroconvert(self.svflags);
		if (sf & serverflags != sf) return FALSE;
	}	


	if (self.ammo_shells && self.ammo_shells > e.ammo_shells) return FALSE;
	if (self.ammo_nails && self.ammo_nails > e.ammo_nails) return FALSE;
	if (self.ammo_rockets && self.ammo_rockets > e.ammo_rockets) return FALSE;
	if (self.ammo_cells && self.ammo_cells > e.ammo_cells) return FALSE;

	if (self.items)
	{
		float it = zeroconvert(self.items);
		if (it & e.items != it) return FALSE;
	}
	
	return TRUE;
}

float(entity e) target_items_takeinv =
{
	if (!target_items_testinv(e)) return FALSE;
	
	if (self.health > 0)
	{
		e.health = max(0, e.health - self.health);
		if (e.health <= 0)
			Killed(e, self);
	}
		
	if (self.armorvalue)
	{
		e.armorvalue = max(0, e.armorvalue - self.armorvalue);
		armor_set_type(e);
	}

	if (self.svflags)
	{
		float sf = zeroconvert(self.svflags);
		serverflags = not(serverflags, sf);
	}
	

	if (self.ammo_shells)
	{
		e.ammo_shells = max(0, e.ammo_shells - zeroconvert(self.ammo_shells));
	}
	if (self.ammo_nails)
	{
		e.ammo_nails = max(0, e.ammo_nails - zeroconvert(self.ammo_nails));
	}
	if (self.ammo_rockets)
	{
		e.ammo_rockets = max(0, e.ammo_rockets - zeroconvert(self.ammo_rockets));
	}
	if (self.ammo_cells)
	{
		e.ammo_cells = max(0, e.ammo_cells - zeroconvert(self.ammo_cells));
	}

	if (self.items)
	{
		float it = zeroconvert(self.items);

		if (it & IT_WEAPONS)
			e.items = not(e.items, it & IT_WEAPONS);
		
		// set artifact timers properly
		if (it & IT_INVISIBILITY)
		{
			e.items = not(e.items, it & IT_INVISIBILITY);
			e.invisible_finished = 0;
			e.invisible_time = 0;
		}

		if (it & IT_INVULNERABILITY)
		{
			e.items = not(e.items, it & IT_INVULNERABILITY);
			e.invincible_finished = 0;
			e.invincible_time = 0;
		}

		if (it & IT_SUIT)
		{
			e.items = not(e.items, it & IT_SUIT);
			e.radsuit_finished = 0;
			e.rad_time = 0;
		}

		if (it & IT_QUAD)
		{
			e.items = not(e.items, it & IT_QUAD);
			e.super_damage_finished = 0;
			e.super_time = 0;
		}
		
		if (!(e.items & (IT_INVISIBILITY | IT_INVULNERABILITY | IT_QUAD)))
			e.effects = not(e.effects, EF_DIMLIGHT);
			
		if (it & IT_KEY1)
			key_take_silver(e);
		if (it & IT_KEY2)
			key_take_gold(e);
			
		target_items_setweapon(e);
	}
	
	return TRUE;
}


void(entity e) target_items_setinv =
{
	if (self.health > 0)
		e.health = self.health;
		
	if (self.armorvalue)
	{
		e.armorvalue = zeroconvert(self.armorvalue);
		armor_set_type(e);
	}

	if (self.svflags)
	{
		serverflags = zeroconvert(self.svflags);		
	}
	

	if (self.ammo_shells) e.ammo_shells = zeroconvert(self.ammo_shells);
	if (self.ammo_nails) e.ammo_nails = zeroconvert(self.ammo_nails);
	if (self.ammo_rockets) e.ammo_rockets = zeroconvert(self.ammo_rockets);
	if (self.ammo_cells) e.ammo_cells = zeroconvert(self.ammo_cells);
	bound_ammo(e);
	
	if (self.items)
	{
		float it = zeroconvert(self.items);

		if (it & IT_WEAPONS)
			e.items = not(e.items, IT_WEAPONS) + (it & IT_WEAPONS);
			
		// set artifact timers properly
		if (it & IT_INVISIBILITY)
			powerup_invisibility(e, self.length);
		else
		{
			e.items = not(e.items, IT_INVISIBILITY);
			e.invisible_finished = 0;
			e.invisible_time = 0;
		}

		if (it & IT_INVULNERABILITY)
			powerup_invulnerability(e, self.length);
		else
		{
			e.items = not(e.items, IT_INVULNERABILITY);
			e.invincible_finished = 0;
			e.invincible_time = 0;
		}

		if (it & IT_SUIT)
			powerup_envirosuit(e, self.length);
		else
		{
			e.items = not(e.items, IT_SUIT);
			e.radsuit_finished = 0;
			e.rad_time = 0;
		}

		if (it & IT_QUAD)
			powerup_super_damage(e, self.length);
		else
		{
			e.items = not(e.items, IT_QUAD);
			e.super_damage_finished = 0;
			e.super_time = 0;
		}
		
		if (!(e.items & (IT_INVISIBILITY | IT_INVULNERABILITY | IT_QUAD)))
			e.effects = not(e.effects, EF_DIMLIGHT);
		
		e.items = not(e.items, IT_KEY1 | IT_KEY2);
		e.worldtype = 0;
		if (it & IT_KEY1)
			key_give_silver(e);
		if (it & IT_KEY2)
			key_give_gold(e);

		target_items_setweapon(e);
	}
}


void() target_items_use =
{
	entity act = activator;
	if (act.classname != "player") return;
	
	if (self.spawnflags & SPAWN_TGTITEMS_OVERRIDE)
	{
		target_items_setinv(act);
	}
	else if (self.spawnflags & SPAWN_TGTITEMS_TEST)
	{
		// TAKE reverses the polarity of TEST, so that it only tests if you don't
		// have all the specified inventory
		if (target_items_testinv(act) != !!(self.spawnflags & SPAWN_TGTITEMS_TAKE))
		{
			activator = act;
			SUB_UseTargets();
		}
		else
			SUB_PrintMessage();
		return;
	}
	else if (self.spawnflags & SPAWN_TGTITEMS_TAKE)
	{
		if (target_items_takeinv(act))
		{
			if (self.spawnflags & SPAWN_TGTITEMS_FLASH)
				stuffcmd(act, "bf\n");
			activator = act;
			SUB_UseTargets();
		}
		else
			SUB_PrintMessage();
		return;
	}
	else
	{
		target_items_giveinv(act);
	}
	if (self.spawnflags & SPAWN_TGTITEMS_FLASH)
		stuffcmd(act, "bf\n");
	activator = act;
}

/*QUAKED target_items (0.75 0.25 0) (-8 -8 -8) (8 8 8) TAKE OVERRIDE FLASH TEST
Gives weapons, ammo, items, health, armor, and/or runes to the activating player when triggered. 

Keyvalues:
"ammo_shells" number of shells player should receive - same for _cells, _rockets, and _nails
"health" health (will overheal beyond 100)
"armorvalue" armor (protection color scales automatically)
"weapon" item bitflag (not impulse number!) of the weapon the player should swap to (if he has it)
"items" bitmask of what items the player should get (see below for reference)
"svflags" bitmask of serverflags to alter (1/2/4/8 are the episode runes, 16+ are invisible)
"length" length of powerup time in seconds if any are given (default 30)

Spawnflags:
TAKE: subtract specified inventory amounts instead of adding
	- subtracting enough health WILL kill the player
	- subtracting all weapons WILL break the game
	- will fire its targets if all items are successfully taken
	- will print "message" if fewer than all items are successfully taken
OVERRIDE: force the player's inventory to all specified amounts (mainly useful triggered from spawnpoints), specify -1 for any keyvalue to force a 0 (-1 does not work for items or health)
FLASH: flash the recipient's screen when triggered, defaults to being stealthy
TEST: behaves like TAKE, but doesn't take anything 
	- fires targets if activator has the matching inventory
	- prints "message" if not
	- TEST with TAKE will reverse the test (fire if player doesn't have)
	- beware of how this interacts with triggers: it will only test the first client to touch a trigger in coop!

Item Bitmask Reference:
	SG	= 1		SSG	= 2
	NG	= 4		SNG	= 8
	GL	= 16	RL	= 32
	LG	= 64	Axe	= 4096
	
	Silver	= 131072
	Gold	= 262144
	Ring	= 524288
	Pent	= 1048576
	Biosuit	= 2097152
	Quad	= 4194304
*/
/*FGD
@PointClass base(Appearflags, Target, Targetname, TrigItems, Deathtype) size(32 32 32) color(192 64 0) = target_items : 
"Target: Give. Gives weapons, ammo, items, health, armor, and/or runes to the activating player when triggered.

Usage:
TAKE: subtract specified inventory amounts instead of adding
	- subtracting enough health WILL kill the player
	- subtracting all weapons WILL break the game
	- will fire its targets if all items are successfully taken
	- will print 'message' if fewer than all items are successfully taken
OVERRIDE: force the player's inventory to all specified amounts (mainly useful triggered from spawnpoints), specify -1 for any keyvalue to force a 0 (-1 does not work for items or health)
TEST: behaves like TAKE, but doesn't take anything 
	- fires targets if activator has the matching inventory
	- prints 'message' if not
	- TEST with TAKE will reverse the test (fire if player doesn't have)
	- beware of how this interacts with triggers: it will only test the first client to touch a trigger in coop!" 
[
	length(integer) : "Override powerup duration" : 0
	message(string) : "Message to print if Take was unsuccessful"
	spawnflags(flags) =
	[
		1 : "Take instead" : 0
		2 : "Override instead" : 0
		4 : "Flash screen" : 0
		8 : "Test only" : 0
	]
	weapon(choices) : "Force Select Weapon" = 
	[
		0 : "Don't"
		1 : "Shotgun"
		2 : "Super Shotgun"
		4 : "Nailgun"
		8 : "Perforator"
		16 : "Grenade Laucher"
		32 : "Rocket Launcher"
		64 : "Lightning Gun"
		4096 : "Axe"
	]
]
*/
void() target_items =
{
	if (!SUB_ShouldSpawn()) return;
	self.use = target_items_use;
	
	if (self.length <= 0)
		self.length = 30;
	if (self.deathtype == string_null)
		self.deathtype = "had his life removed";
	
	if (self.items > 0)
	{
		// clear the item flags that are only for HUD switches so we can be sloppy with .items later
		self.items = not(self.items, IT_SHELLS | IT_NAILS | IT_ROCKETS | IT_CELLS | 
									 IT_ARMOR1 | IT_ARMOR2 | IT_ARMOR3 );

		if (self.items & IT_INVISIBILITY)
			precache_invisibility();
		if (self.items & IT_INVULNERABILITY)
			precache_invulnerability();
		if (self.items & IT_SUIT)
			precache_envirosuit();
		if (self.items & IT_QUAD)
			precache_super_damage();
	}
}
void() target_give = {target_items();}


// ==========================================================

void() trigger_inventory_touch =
{
	// for multiple coop clients touching this at once
	if (time != self.dmgtime)
		if (time < self.attack_finished)
			return;
	
	if (self.wait > 0)
	{
		self.attack_finished = time + self.wait;
	}
	else
	{
		self.attack_finished = time + 1;
		self.think = SUB_Remove;
		self.nextthink = time + 0.1;
	}
	
	// the trigger has to test every client touching this frame until one passes, then stop testing
	// but set attack_finished to the next wait interval either way
	if (target_items_testinv(other) != !!(self.spawnflags & 2))
	{
		multi_trigger();
		self.dmgtime = 0;	// stop checking clients
		return;
	}
	SUB_PrintMessage();
	self.dmgtime = time;
}

/*QUAKED trigger_inventory (.5 .0 .5) ? ? INVERSE TRIGGER_FIRST
Variable sized trigger. Triggers only if the player's current inventory or stats match what's specified by trigger_inventory - this follows the same rules as target_items in TEST mode. Will test all touching clients in a given frame until one passes the test.

Flags:
"inverse" reverse polarity of the test (fire only if player doesn't have specified inventory)
"trigger_first" won't be touchable until triggered once by something else

Keys:
"message" message to display if inventory test fails (ie triggers do NOT fire)
"sounds"
    1 = secret
    2 = beep beep
	3 = large switch
	4 = it is a mysteryyy
	-1 = silent
"noise"	choose your own .wav
"target"  entity to trigger (required)
"targetname"  entity name

Inventory stuff:
"ammo_shells" number of shells player should have - same for _cells, _rockets, and _nails
"health" health minimum
"armorvalue" armor minumum
"items" bitmask of what items the player should have (see below for reference)
"svflags" bitmask of serverflags that must be set (1/2/4/8 are the episode runes, 16+ are invisible)

Item Bitmask Reference:
	SG	= 1		SSG	= 2
	NG	= 4		SNG	= 8
	GL	= 16	RL	= 32
	LG	= 64	Axe	= 4096
	
	Silver	= 131072
	Gold	= 262144
	Ring	= 524288
	Pent	= 1048576
	Biosuit	= 2097152
	Quad	= 4194304

*/
/*FGD
@SolidClass base(Trigger, TrigItems) = trigger_inventory : "Trigger: Activate if inventory matches" [
	spawnflags(Flags) = [
		2 : "Activate if !match" : 0
		4 : "Trigger first" : 0
	]
]
*/
void() trigger_inventory =
{
	if(!SUB_ShouldSpawn()) return;
	InitTrigger();
	if (!self.wait) self.wait = 0.2;
	self.touch = trigger_inventory_touch;
}