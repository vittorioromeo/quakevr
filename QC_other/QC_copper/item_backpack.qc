/*
===============================================================================

BACKPACKS

===============================================================================
*/

/*
================================
print_quant
pretty-print a quantity to a client's buffer
================================
*/
void(float amt, string noun, entity to) print_quant =
{
	string s;
	
	if (amt == 1)
	{
		sprint (to, "1 ");
		sprint (to, noun);
		return;
	}
	
	s = ftos(amt);
	sprint (to, s);
	sprint (to, " ");
	sprint (to, noun);
	sprint (to, "s");
}

/*
================================
BackpackTouch
================================
*/
void() BackpackTouch =
{
	if (!CheckValidTouch()) return;

	float acount;
	
	other.ammo_shells = other.ammo_shells + self.ammo_shells;
	other.ammo_nails = other.ammo_nails + self.ammo_nails;
	other.ammo_rockets = other.ammo_rockets + self.ammo_rockets;
	other.ammo_cells = other.ammo_cells + self.ammo_cells;
	bound_ammo(other);
	armor_give(other, self.armorvalue, FALSE);	
	
	sprint (other, "You got ");

	if (self.items && (other.items & self.items) == 0)
	{
		entity oself;
		acount = TRUE;
		sprint (other, "the ");
		sprint (other, self.netname);
	
		oself = self;
		self = other;
		// change to the weapon
		W_BetterWeapon(oself.items);
		self = oself;
	}

	if (self.armorvalue)
	{
		if (acount) sprint(other, ", ");
		acount = 1;
		sprint(other, ftos(self.armorvalue));
		sprint(other, " armor");
	}
	if (self.ammo_shells)
	{
		if (acount) sprint(other, ", ");
		acount = 2;
		print_quant(self.ammo_shells, "shell", other);
	}
	if (self.ammo_nails)
	{
		if (acount) sprint(other, ", ");
		acount = 2;
		print_quant(self.ammo_nails, "nail", other);
	}
	if (self.ammo_rockets)
	{
		if (acount) sprint(other, ", ");
		acount = 2;
		print_quant(self.ammo_rockets, "rocket", other);
	}
	if (self.ammo_cells)
	{
		if (acount) sprint(other, ", ");
		acount = 2;
		print_quant(self.ammo_cells, "cell", other);
	}
	
	sprint (other, "\n");
	
	// do this after because t_heal prints its own message
	if (self.healamount)
		T_Heal(other, self.healamount, 0);

	// regular monster backpacks don't make the AWESOME NEW backpack sound
	if (self.classname == "item_backpack")
	{
		if (acount == 2)
			sound(other, CHAN_ITEM, "items/pack.wav", 1, ATTN_NORM);
		else	// only got armor, play armor sound instead
			sound(other, CHAN_ITEM, "items/patch.wav", 1, ATTN_NORM);
	}
	else
		sound (other, CHAN_ITEM, "weapons/lock4.wav", 1, ATTN_NORM);
		
	stuffcmd (other, "bf\n");

	SUB_CallAsSelf(W_SetCurrentAmmo, other);
	ItemTouched();
}

/*
================================
BackpackAutoTouch

backpacks can always be picked up, so mapper-placed ammo backpacks should 
give the player something else if they're maxxed out on any ammo types,
since they're little exploration rewards for hiding behind crates and etc
================================
*/
void() BackpackAutoTouch =
{
	if (!CheckValidTouch()) return;

	if (self.spawnflags & 1)
		sprint (other, "You found a big backpack!\n");
	else
		sprint (other, "You found a backpack\n");
		
	if (other.ammo_shells + self.ammo_shells > MAX_AMMO_SHELLS)
	{
		self.armorvalue += other.ammo_shells + self.ammo_shells - MAX_AMMO_SHELLS;
		self.ammo_shells = MAX_AMMO_SHELLS - other.ammo_shells;
	}
	if (other.ammo_nails + self.ammo_nails > MAX_AMMO_NAILS)
	{
		self.armorvalue += ceil((other.ammo_nails + self.ammo_nails - MAX_AMMO_NAILS) / 2);
		self.ammo_nails = MAX_AMMO_NAILS - other.ammo_nails;
	}
	if (other.ammo_rockets + self.ammo_rockets > MAX_AMMO_ROCKETS)
	{
		self.armorvalue += (other.ammo_rockets + self.ammo_rockets - MAX_AMMO_ROCKETS) * 3;
		self.ammo_rockets = MAX_AMMO_ROCKETS - other.ammo_rockets;
	}
	if (other.ammo_cells + self.ammo_cells > MAX_AMMO_CELLS)
	{
		self.armorvalue += (other.ammo_cells + self.ammo_cells - MAX_AMMO_CELLS) * 3;
		self.ammo_cells = MAX_AMMO_CELLS - other.ammo_cells;
	}
	
	self.armorvalue = floor(self.armorvalue / 2);
	BackpackTouch();
}

/*QUAKED item_backpack (0 .5 .5) (-16 -16 0) (16 16 40) BIG ? SUSPENDED -
A forgotten explorer's backpack. Grants a paltry sum of shells and nails (about half a small pack of each). Grants armor instead of any ammo the player already has maxed.

Flags:
"big" doubles shells and nails, adds 2 rockets and 4 cells, appears red
    
Keys:
"target/2/3/4/k" - entity to trigger when picked up
"targetname" - will not spawn until triggered
"wait" - will respawn after 'wait' seconds. fires targets every time.
"count" - limit number of times to respawn
*/
/*FGD
@PointClass base(Item) size(-16 -16 0, 16 16 40) color(0 128 128) 
	model({ "path": ":progs/backpack.mdl" }) 
	= item_backpack : 
"A forgotten explorer's backpack. Grants a paltry sum of shells and nails (about half a small pack of each). Grants armor instead of any ammo the player already has maxed."
[
	spawnflags(flags) =	[
		1 : "Big (2x, red, rockets/cells)" : 0
	]
]
*/
void() item_backpack =
{
	if (!SUB_ShouldSpawn()) return;
	precache_model("progs/backpack.mdl");
	precache_sound3("items/pack.wav");	// backpacks can play either of these
	precache_sound3("items/patch.wav");

	self.flags = FL_ITEM;
	self.solid = SOLID_TRIGGER;
	self.movetype = MOVETYPE_TOSS;
	setmodel (self, "progs/backpack.mdl");
	setsize (self, '-16 -16 0', '16 16 56');
	self.skin = 1;
	
	self.ammo_shells = floor(3 * random() + 6);
	self.ammo_nails = floor(5 * random() + 12);
	if (self.spawnflags & 1)
	{
		self.ammo_shells *= 2;
		self.ammo_nails *= 2;
		
		self.ammo_rockets = 2;
		self.ammo_cells = 4;
		self.skin = 2;
	}
	
	self.touch = BackpackAutoTouch;
	StartItem ();
}




/*
===============
DropBackpack
===============
*/
void() DropBackpack =
{
	entity item;

	if (!(self.ammo_shells + self.ammo_nails + self.ammo_rockets + self.ammo_cells))
		return;	// nothing in it

	item = spawn();
	item.origin = self.origin - '0 0 24';
	
	item.items = self.weapon;
	if (item.items == IT_AXE)
		item.netname = "Axe";
	else if (item.items == IT_SHOTGUN)
		item.netname = "Shotgun";
	else if (item.items == IT_SUPER_SHOTGUN)
		item.netname = "Double-Barrelled Shotgun";
	else if (item.items == IT_NAILGUN)
		item.netname = "Nailgun";
	else if (item.items == IT_SUPER_NAILGUN)
		item.netname = "Perforator";
	else if (item.items == IT_GRENADE_LAUNCHER)
		item.netname = "Grenade Launcher";
	else if (item.items == IT_ROCKET_LAUNCHER)
		item.netname = "Rocket Launcher";
	else if (item.items == IT_LIGHTNING)
		item.netname = "Thunderbolt";
	else
		item.netname = string_null;

	item.ammo_shells = self.ammo_shells;
	item.ammo_nails = self.ammo_nails;
	item.ammo_rockets = self.ammo_rockets;
	item.ammo_cells = self.ammo_cells;

	item.velocity_z = 300;
	item.velocity_x = -100 + (random() * 200);
	item.velocity_y = -100 + (random() * 200);
	
	item.flags = FL_ITEM;
	item.solid = SOLID_TRIGGER;
	item.movetype = MOVETYPE_TOSS;
	setmodel (item, "progs/backpack.mdl");
	setsize (item, '-16 -16 0', '16 16 56');
	item.touch = BackpackTouch;
	
	item.nextthink = time + 120;	// remove after 2 minutes
	item.think = SUB_Remove;
}