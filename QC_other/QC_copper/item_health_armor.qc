/*
=========================================================================

HEALTH BOXES

=========================================================================
*/

float	ITEM_HEALTH_ROTTEN = 1;
float	ITEM_HEALTH_MEGA = 2;


void() health_touch =
{
	if (!CheckValidTouch()) return;
	
	if (!T_Heal(other, self.healamount, (self.spawnflags & ITEM_HEALTH_MEGA)))
		return;
	sound(other, CHAN_ITEM, self.noise, 1, ATTN_NORM);

	ItemTouched();
}	


/*
================================
item_health & item_health_go
split up precaches and business code for late game health spawns
================================
*/
void() item_health_go
{
	self.touch = health_touch;
	self.type = "health";

	if (self.spawnflags & ITEM_HEALTH_ROTTEN)
	{
		setmodel(self, "maps/b_bh10.bsp");
		self.noise = "items/r_item1.wav";
		self.healamount = 15;
	}
	else if (self.spawnflags & ITEM_HEALTH_MEGA)
	{
		setmodel(self, "maps/b_bh100.bsp");
		self.noise = "items/r_item2.wav";
		self.healamount = 100;
	}
	else
	{
		setmodel(self, "maps/b_bh25.bsp");
		self.noise = "items/health1.wav";
		self.healamount = 25;
	}
	setsize (self, '0 0 0', '32 32 56');
	
	if (deathmatch == 1)
		self.wait = 20;
		
	StartItem ();
}

// ===============================================================================

/*QUAKED item_health (0 .5 .5) (0 0 0) (32 32 32) SMALL MEGA SUSPENDED 
Health box. Normally gives 25 points.

Flags:
"mega" will add 100 health, then rot you down to your maximum health limit, one point per second
"small" gives 15 points

Keys:
"target/2/3/4/k" - entity to trigger when picked up
"targetname" - will not spawn until triggered
"wait" - will respawn after 'wait' seconds. fires targets every time.
"count" - limit number of times to respawn
*/
/*FGD
@PointClass size(0 0 0, 32 32 56) base(Item) 
	model(
        {{
            spawnflags & 2 ->   ":maps/b_bh100.bsp",
            spawnflags & 1 ->   ":maps/b_bh10.bsp",
                                ":maps/b_bh25.bsp"
        }}
    ) =
    item_health : "Health pack" 
[
	spawnflags(flags) = 
	[
		1 : "Rotten" : 0
		2 : "Megahealth" : 0
	]
]
*/
void() item_health =
{	
	if (!SUB_ShouldSpawn()) return;
	if (self.spawnflags & ITEM_HEALTH_MEGA)
	{
		precache_model("maps/b_bh100.bsp");
		precache_sound("items/r_item2.wav");
	}
	else if (self.spawnflags & ITEM_HEALTH_ROTTEN)
	{
		precache_model("maps/b_bh10.bsp");
		precache_sound("items/r_item1.wav");
	}
	else
	{
		precache_model("maps/b_bh25.bsp");
		precache_sound("items/health1.wav");
	}

	item_health_go();
}





/*
===============================================================================

ARMOR

===============================================================================
*/

/*
============
armor_set_type

coverage proportional to quantity, not derived from last armor grabbed
============
*/
void(entity e) armor_set_type =
{
	if (e.classname != "player")
		return;
		
	e.items = not(e.items, (IT_ARMOR1 | IT_ARMOR2 | IT_ARMOR3));
	if (!e.armorvalue)
		return;
	
	// change HUD icon based on protection strength
	if (e.armorvalue <= 100)
		e.items |= IT_ARMOR1;
	else if (e.armorvalue <= 150)
		e.items |= IT_ARMOR2;
	else
		e.items |= IT_ARMOR3;
	
	e.armorvalue = min(e.armorvalue, 500);

	// armor never worth less than 25%, absorbs a theoretical 100% of damage at 250 armor points
	e.armortype = 0.05 + e.armorvalue / 250;
	e.armortype = clamp(e.armortype, 0.25, 1.0);
	// this yields 25% up to 50 armor, 45% at 100 armor, 65% at 150, and 85% at 200
	// armors thus start out a little stronger than their vanilla counterparts (30/60/80%) before weakening with loss
	// the green armor gets the biggest buff from this ramp but that's okay
}


/*
============
armor_give
============
*/
float(entity e, float amt, float merge) armor_give =
{
/*
	if (e.armorvalue <= 0)
	{
		// default to green armor if there isn't some already
		e.armortype = 0.3;
		e.items = not(e.items, (IT_ARMOR1 | IT_ARMOR2 | IT_ARMOR3)) + IT_ARMOR1;
	}*/
	if (merge)
	{
		// dumb magic formula for merging armor quake2 style
		/*
		float l, s;
		
		if (e.armorvalue < amt)
		{
			l = amt;
			s = e.armorvalue;
		}
		else
		{
			s = amt;
			l = e.armorvalue;
		}
		
		amt = l + max(0,(8 * s - l)) / 10;	
		amt = floor(5 * ceil(amt / 5));	// round up to multiple of 5 so new armor still feels nice and crisp
		*/
		if (e.armorvalue >= amt)
			return FALSE;
		e.armorvalue = amt;
	}
	else
		e.armorvalue += amt;
	
	armor_set_type(e);
	return TRUE;
}


/*
============
armor_touch
============
*/
void() armor_touch =
{
	//float	getamt, atype, bit;
	if (!CheckValidTouch()) return;
	
/*
	float	value;
	// amounts are reduced, but armor always stacks with existing armor
	if (self.classname == "item_armor1")
	{
		//atype = 0.3;
		value = 100;
		//bit = IT_ARMOR1;
		//value = 80;
	}
	else if (self.classname == "item_armor2")
	{
		//atype = 0.6;
		value = 150;
		//bit = IT_ARMOR2;
		//value = 120;
	}
	else //if (self.classname == "item_armorInv")
	{
		//atype = 0.8;
		value = 200;
		//bit = IT_ARMOR3;
		//value = 160;
	}
	*/
	//other.armortype = atype;	// old way
	//other.items = not(other.items, (IT_ARMOR1 | IT_ARMOR2 | IT_ARMOR3)) + bit;
	if (!armor_give(other, self.strength, TRUE))
		return;

	sound(other, CHAN_ITEM, "items/armor1.wav", 1, ATTN_NORM);
	sprint(other, "You got armor\n");

	stuffcmd (other, "bf\n");
	ItemTouched();
}

// ===============================================================================
/*
============
StartArmor
============
*/
void(float skn, float amt) StartArmor =
{
	if (!SUB_ShouldSpawn()) return;

	self.skin = skn;
	self.strength = amt;
	
	self.touch = armor_touch;
	self.type = "armor";
	precache_model ("progs/armor.mdl");
	setmodel (self, "progs/armor.mdl");
	
	if (deathmatch == 1)
		self.wait = 20;

	setsize (self, '-16 -16 0', '16 16 56');
	StartItem ();
}



/*QUAKED item_armor1 (0 .5 .5) (-16 -16 0) (16 16 32) ? ? SUSPENDED
Green armor, gives 100 points.

Keys:
"target/2/3/4/k" - entity to trigger when picked up
"targetname" - will not spawn until triggered
"wait" - will respawn after 'wait' seconds. fires targets every time.
"count" - limit number of times to respawn
*/
/*FGD
@PointClass size(-16 -16 0, 16 16 56) base(Item) model({ "path": ":progs/armor.mdl" }) =
	item_armor1 : "Green armor (100%)" []
*/
void() item_armor1 = { StartArmor(0,100); }

/*QUAKED item_armor2 (0 .5 .5) (-16 -16 0) (16 16 32) ? ? SUSPENDED
Yellow armor, gives 150 points.

Keys:
"target/2/3/4/k" - entity to trigger when picked up
"targetname" - will not spawn until triggered
"wait" - will respawn after 'wait' seconds. fires targets every time.
"count" - limit number of times to respawn
*/
/*FGD
@PointClass size(-16 -16 0, 16 16 56) base(Item) model({ "path": ":progs/armor.mdl", "skin": 1 }) =
	item_armor2 : "Yellow armor (150%)" []
*/
void() item_armor2 = { StartArmor(1,150); }

/*QUAKED item_armorInv (0 .5 .5) (-16 -16 0) (16 16 32) ? ? SUSPENDED
Red armor, gives 200 armor points.

Keys:
"target/2/3/4/k" - entity to trigger when picked up
"targetname" - will not spawn until triggered
"wait" - will respawn after 'wait' seconds. fires targets every time.
"count" - limit number of times to respawn
*/
/*FGD
@PointClass size(-16 -16 0, 16 16 56) base(Item) model({ "path": ":progs/armor.mdl", "skin": 2 }) =
	item_armorInv : "Red armor (200%)" []
*/
void() item_armorInv = { StartArmor(2,200); }


