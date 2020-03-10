void() W_SetCurrentAmmo;
/* ALL LIGHTS SHOULD BE 0 1 0 IN COLOR ALL OTHER ITEMS SHOULD
BE .8 .3 .4 IN COLOR */


void() SUB_regen =
{
    self.model = self.mdl;        // restore original model
    self.solid = SOLID_TRIGGER;    // allow it to be touched again
    sound (self, CHAN_VOICE, "items/itembk2.wav", 1, ATTN_NORM);    // play respawn sound
    setorigin (self, self.origin);
};



/*QUAKED noclass (0 0 0) (-8 -8 -8) (8 8 8)
prints a warning message when spawned
*/
void() noclass =
{
    dprint ("noclass spawned at");
    dprint (vtos(self.origin));
    dprint ("\n");
    remove (self);
};



/*
============
PlaceItem

plants the object on the floor
============
*/
void() PlaceItem =
{
    local float    oldz;

    self.mdl = self.model;        // so it can be restored on respawn
    self.flags = FL_ITEM;        // make extra wide
    self.solid = SOLID_TRIGGER;
    self.movetype = MOVETYPE_TOSS;
    self.velocity = '0 0 0';
    self.origin_z = self.origin_z + 6;
    oldz = self.origin_z;
    if (!droptofloor())
    {
        dprint ("Bonus item fell out of level!\n");
        dprint (self.classname);
        dprint (" at ");
        dprint (vtos(self.origin));
        dprint ("\n");
        remove(self);
        return;
    }
};

/*
============
StartItem

Sets the clipping size and plants the object on the floor
============
*/
void() StartItem =
{
    self.nextthink = time + 0.2;    // items start after other solids
    self.think = PlaceItem;
};

/*
=========================================================================

HEALTH BOX

=========================================================================
*/
//
// T_Heal: add health to an entity, limiting health to max_health
// "ignore" will ignore max_health limit
//
float (entity e, float healamount, float ignore) T_Heal =
{
    if (e.health <= 0)
        return 0;
    if ((!ignore) && (e.health >= other.max_health))
        return 0;
    healamount = ceil(healamount);

    e.health = e.health + healamount;
    if ((!ignore) && (e.health >= other.max_health))
        e.health = other.max_health;

    if (e.health > 250)
        e.health = 250;
    return 1;
};

/*QUAKED item_health (.3 .3 1) (0 0 0) (32 32 32) rotten megahealth
Health box. Normally gives 25 points.
Rotten box heals 5-10 points,
megahealth will add 100 health, then
rot you down to your maximum health limit,
one point per second.
*/

float    H_ROTTEN = 1;
float    H_MEGA = 2;
.float    healamount, healtype;
void() health_touch;
void() item_megahealth_rot;

void() item_health =
{
    self.touch = health_touch;

    if (self.spawnflags & H_ROTTEN)
    {
        precache_model("maps/b_bh10.bsp");

        precache_sound("items/r_item1.wav");
        setmodel(self, "maps/b_bh10.bsp");
        self.noise = "items/r_item1.wav";
        self.healamount = 15;
        self.healtype = 0;
    }
    else
    if (self.spawnflags & H_MEGA)
    {
        precache_model("maps/b_bh100.bsp");
        precache_sound("items/r_item2.wav");
        setmodel(self, "maps/b_bh100.bsp");
        self.noise = "items/r_item2.wav";
        self.healamount = 100;
        self.healtype = 2;
    }
    else
    {
        precache_model("maps/b_bh25.bsp");
        precache_sound("items/health1.wav");
        setmodel(self, "maps/b_bh25.bsp");
        self.noise = "items/health1.wav";
        self.healamount = 25;
        self.healtype = 1;
    }
    setsize (self, '0 0 0', '32 32 56');
    StartItem ();
};


void() health_touch =
{
    local    float amount;
    local    string    s;

    if (other.classname != "player")
        return;

    if (self.healtype == 2) // Megahealth?  Ignore max_health...
    {
        if (other.health >= 250)
            return;
        if (!T_Heal(other, self.healamount, 1))
            return;
    }
    else
    {
        if (!T_Heal(other, self.healamount, 0))
            return;
    }

    sprint(other, "You receive ");
    s = ftos(self.healamount);
    sprint(other, s);
    sprint(other, " health\n");

// health touch sound
    sound(other, CHAN_ITEM, self.noise, 1, ATTN_NORM);

    stuffcmd (other, "bf\n");

    self.model = string_null;
    self.solid = SOLID_NOT;

    // Megahealth = rot down the player's super health
    if (self.healtype == 2 && !RuneHasElder(other))
    {
        other.items2 = other.items2 | IT2_SUPERHEALTH;
        self.nextthink = time + 5;
        self.think = item_megahealth_rot;
        self.owner = other;
    }
    else
    {
        if (deathmatch != 2)        // deathmatch 2 is the silly old rules
        {
            if (deathmatch)
                self.nextthink = time + 20;
            self.think = SUB_regen;
        }
    }

    activator = other;
    SUB_UseTargets();                // fire all targets / killtargets
};

void() item_megahealth_rot =
{
    other = self.owner;

    if (other.health > other.max_health)
    {
        other.health = other.health - 1;
        self.nextthink = time + 1;
        return;
    }

// it is possible for a player to die and respawn between rots, so don't
// just blindly subtract the flag off
    other.items2 = other.items2 - (other.items2 & IT2_SUPERHEALTH);

    if (deathmatch == 1)    // deathmatch 2 is silly old rules
    {
        self.nextthink = time + 20;
        self.think = SUB_regen;
    }
};

/*
===============================================================================

ARMOR

===============================================================================
*/

void() armor_touch;

void() armor_touch =
{
    local    float    type, value, bit;

    if (other.health <= 0)
        return;
    if (other.classname != "player")
        return;

    if (self.classname == "item_armor1")
    {
        type = 0.3;
        value = 100;
        bit = IT2_ARMOR1;
    }
    if (self.classname == "item_armor2")
    {
        type = 0.6;
        value = 150;
        bit = IT2_ARMOR2;
    }
    if (self.classname == "item_armorInv")
    {
        type = 0.8;
        value = 200;
        bit = IT2_ARMOR3;
    }
    if (other.armortype*other.armorvalue >= type*value)
        return;

    other.armortype = type;
    other.armorvalue = value;
    other.items2 = other.items2 -
                (other.items2 & (IT2_ARMOR1 | IT2_ARMOR2 | IT2_ARMOR3)) + bit;

    self.solid = SOLID_NOT;
    self.model = string_null;
    if (deathmatch == 1)
        self.nextthink = time + 20;
    self.think = SUB_regen;

    sprint(other, "You got armor\n");
// armor touch sound
    sound(other, CHAN_ITEM, "items/armor1.wav", 1, ATTN_NORM);
    stuffcmd (other, "bf\n");

    activator = other;
    SUB_UseTargets();                // fire all targets / killtargets
};


/*QUAKED item_armor1 (0 .5 .8) (-16 -16 0) (16 16 32)
*/

void() item_armor1 =
{
    self.touch = armor_touch;
    precache_model ("progs/armor.mdl");
    setmodel (self, "progs/armor.mdl");
    self.skin = 0;
    setsize (self, '-16 -16 0', '16 16 56');
    StartItem ();
};

/*QUAKED item_armor2 (0 .5 .8) (-16 -16 0) (16 16 32)
*/

void() item_armor2 =
{
    self.touch = armor_touch;
    precache_model ("progs/armor.mdl");
    setmodel (self, "progs/armor.mdl");
    self.skin = 1;
    setsize (self, '-16 -16 0', '16 16 56');
    StartItem ();
};

/*QUAKED item_armorInv (0 .5 .8) (-16 -16 0) (16 16 32)
*/

void() item_armorInv =
{
    self.touch = armor_touch;
    precache_model ("progs/armor.mdl");
    setmodel (self, "progs/armor.mdl");
    self.skin = 2;
    setsize (self, '-16 -16 0', '16 16 56');
    StartItem ();
};

/*
===============================================================================

WEAPONS

===============================================================================
*/

void() bound_other_ammo =
{
    if (other.ammo_shells1 > 100)
        other.ammo_shells1 = 100;
    if (other.ammo_nails1 > 200)
        other.ammo_nails1 = 200;
    if (other.ammo_rockets1 > 100)
        other.ammo_rockets1 = 100;
    if (other.ammo_cells1 > 100)
        other.ammo_cells1 = 100;

    // PGM addition
    if (other.ammo_lava_nails > 200)
        other.ammo_lava_nails = 200;
    if (other.ammo_multi_rockets > 100)
        other.ammo_multi_rockets = 100;
    if (other.ammo_plasma > 100)
        other.ammo_plasma = 100;
};


float(float w) RankForWeapon =
{
    if (w == IT_PLASMA_GUN)
        return 1;
    if (w == IT_LIGHTNING)
        return 2;
    if (w == IT_MULTI_ROCKET)
        return 3;
    if (w == IT_ROCKET_LAUNCHER)
        return 4;
    if (w == IT_LAVA_SUPER_NAILGUN)
        return 5;
    if (w == IT_SUPER_NAILGUN)
        return 6;
    if (w == IT_MULTI_GRENADE)
        return 7;
    if (w == IT_GRENADE_LAUNCHER)
        return 8;
    if (w == IT_LAVA_NAILGUN)
        return 9;
    if (w == IT_SUPER_SHOTGUN)
        return 10;
    if (w == IT_NAILGUN)
        return 11;
    return 12;
};

/*
=============
Deathmatch_Weapon

Deathmatch weapon change rules for picking up a weapon

.float        ammo_shells1, ammo_nails1, ammo_rockets1, ammo_cells1;
=============
*/
void(float old, float new) Deathmatch_Weapon =
{
    local float or, nr;

// PGM - fix for changing to weapons when grappling.
    if (self.weapon == IT_GRAPPLE && self.button0)
        return;

// change self.weapon if desired
    or = RankForWeapon (self.weapon);
    nr = RankForWeapon (new);
    if ( nr < or )
        self.weapon = new;
};

void(entity comboOwner) EnableComboWeapons =
{
    if (comboOwner.classname != "player")
        return;

    if (!(comboOwner.items & IT_LAVA_NAILGUN))
    {
        if (comboOwner.ammo_lava_nails > 0 && (comboOwner.items & IT_NAILGUN))
        {
            sprint (comboOwner, "Lava Enabled\n");
            comboOwner.items = comboOwner.items | IT_LAVA_NAILGUN;
        }
    }

    if (!(comboOwner.items & IT_LAVA_SUPER_NAILGUN))
    {
        if (comboOwner.ammo_lava_nails > 0 &&
                            (comboOwner.items & IT_SUPER_NAILGUN))
        {
            sprint (comboOwner, "Super Lava Enabled\n");
            comboOwner.items = comboOwner.items | IT_LAVA_SUPER_NAILGUN;
        }
    }

    if (!(comboOwner.items & IT_MULTI_GRENADE))
    {
        if (comboOwner.ammo_multi_rockets > 0 &&
                            (comboOwner.items & IT_GRENADE_LAUNCHER))
        {
            sprint (comboOwner, "Multi Grenades Enabled\n");
            comboOwner.items = comboOwner.items | IT_MULTI_GRENADE;
        }
    }

    if (!(comboOwner.items & IT_MULTI_ROCKET))
    {
        if (comboOwner.ammo_multi_rockets > 0 &&
                            (comboOwner.items & IT_ROCKET_LAUNCHER))
        {
            sprint (comboOwner, "Multi Rockets Enabled\n");
            comboOwner.items = comboOwner.items | IT_MULTI_ROCKET;
        }
    }

    if (!(comboOwner.items & IT_PLASMA_GUN))
    {
        if (comboOwner.ammo_plasma > 0 && (comboOwner.items & IT_LIGHTNING))
        {
            sprint (comboOwner, "Plasma Gun Enabled\n");
            comboOwner.items = comboOwner.items | IT_PLASMA_GUN;
        }
    }
};

/*
=============
weapon_touch
=============
*/
float() W_BestWeapon;

void() weapon_touch =
{
    local    float    hadammo, best, new, old;
    local    entity    stemp;
    local    float    leave;

    if (!(other.flags & FL_CLIENT))
        return;

// if the player was using his best weapon, change up to the new one if better
    stemp = self;
    self = other;
    best = W_BestWeapon();
    self = stemp;

    if (deathmatch == 2 || coop)
        leave = 1;
    else
        leave = 0;

    if (self.classname == "weapon_nailgun")
    {
        if (leave && (other.items & IT_NAILGUN) )
            return;
        hadammo = other.ammo_nails1;
        new = IT_NAILGUN;
        if (leave)
            other.ammo_lava_nails = other.ammo_lava_nails + 20;
        other.ammo_nails1 = other.ammo_nails1 + 30;
    }
    else if (self.classname == "weapon_supernailgun")
    {
        if (leave && (other.items & IT_SUPER_NAILGUN) )
            return;
        hadammo = other.ammo_rockets1;
        new = IT_SUPER_NAILGUN;
        if (leave)
            other.ammo_lava_nails = other.ammo_lava_nails + 20;
        other.ammo_nails1 = other.ammo_nails1 + 30;
    }
    else if (self.classname == "weapon_supershotgun")
    {
        if (leave && (other.items & IT_SUPER_SHOTGUN) )
            return;
        hadammo = other.ammo_rockets1;
        new = IT_SUPER_SHOTGUN;
        other.ammo_shells1 = other.ammo_shells1 + 5;
    }
    else if (self.classname == "weapon_rocketlauncher")
    {
        if (leave && (other.items & IT_ROCKET_LAUNCHER) )
            return;
        hadammo = other.ammo_rockets1;
        new = IT_ROCKET_LAUNCHER;
        if (leave)
            other.ammo_multi_rockets = other.ammo_multi_rockets + 3;
        other.ammo_rockets1 = other.ammo_rockets1 + 5;
    }
    else if (self.classname == "weapon_grenadelauncher")
    {
        if (leave && (other.items & IT_GRENADE_LAUNCHER) )
            return;
        hadammo = other.ammo_rockets1;
        new = IT_GRENADE_LAUNCHER;
        if (leave)
            other.ammo_multi_rockets = other.ammo_multi_rockets + 3;
        other.ammo_rockets1 = other.ammo_rockets1 + 5;
    }
    else if (self.classname == "weapon_lightning")
    {
        if (leave && (other.items & IT_LIGHTNING) )
            return;
        hadammo = other.ammo_rockets1;
        new = IT_LIGHTNING;
        if (leave)
            other.ammo_plasma = other.ammo_plasma + 5;
        other.ammo_cells1 = other.ammo_cells1 + 15;
    }
    else
        objerror ("weapon_touch: unknown classname");

    sprint (other, "You got the ");
    sprint (other, self.netname);
    sprint (other, "\n");
// weapon touch sound
    sound (other, CHAN_ITEM, "weapons/pkup.wav", 1, ATTN_NORM);
    stuffcmd (other, "bf\n");

    bound_other_ammo ();

// change to the weapon
    old = other.items;
    other.items = other.items | new;

    EnableComboWeapons ( other );

    // PGM - fake picking up the plasma gun.
    if (new == IT_LIGHTNING)
    {
        if (other.ammo_plasma > 0)
            new = IT_PLASMA_GUN;
    }
    else if (new == IT_ROCKET_LAUNCHER)
    {
        if (other.ammo_multi_rockets > 0)
            new = IT_MULTI_ROCKET;
    }
    else if (new == IT_GRENADE_LAUNCHER)
    {
        if (other.ammo_multi_rockets > 0)
            new = IT_MULTI_GRENADE;
    }
    else if (new == IT_SUPER_NAILGUN)
    {
        if (other.ammo_lava_nails > 1)
            new = IT_LAVA_SUPER_NAILGUN;
    }
    else if (new == IT_NAILGUN)
    {
        if (other.ammo_lava_nails > 0)
            new = IT_LAVA_NAILGUN;
    }


    stemp = self;
    self = other;

    if (!deathmatch)
        self.weapon = new;
    else
        Deathmatch_Weapon (old, new);

    UpdateAmmoCounts ( self );
    W_SetCurrentAmmo();

    self = stemp;

    if (leave)
        return;

// remove it in single player, or setup for respawning in deathmatch
    self.model = string_null;
    self.solid = SOLID_NOT;
    if (deathmatch == 1)
         self.nextthink = time + 30;
    self.think = SUB_regen;

    activator = other;
    SUB_UseTargets();                // fire all targets / killtargets
};


/*QUAKED weapon_supershotgun (0 .5 .8) (-16 -16 0) (16 16 32)
*/

void() weapon_supershotgun =
{
    precache_model ("progs/g_shot.mdl");
    setmodel (self, "progs/g_shot.mdl");
    self.weapon = IT_SUPER_SHOTGUN;
    self.netname = "Double-barrelled Shotgun";
    self.touch = weapon_touch;
    setsize (self, '-16 -16 0', '16 16 56');
    StartItem ();
};

/*QUAKED weapon_nailgun (0 .5 .8) (-16 -16 0) (16 16 32)
*/

void() weapon_nailgun =
{
    precache_model ("progs/g_nail.mdl");
    setmodel (self, "progs/g_nail.mdl");
    self.weapon = IT_NAILGUN;
    self.netname = "nailgun";
    self.touch = weapon_touch;
    setsize (self, '-16 -16 0', '16 16 56');
    StartItem ();
};

/*QUAKED weapon_supernailgun (0 .5 .8) (-16 -16 0) (16 16 32)
*/

void() weapon_supernailgun =
{
    precache_model ("progs/g_nail2.mdl");
    setmodel (self, "progs/g_nail2.mdl");
    self.weapon = IT_SUPER_NAILGUN;
    self.netname = "Super Nailgun";
    self.touch = weapon_touch;
    setsize (self, '-16 -16 0', '16 16 56');
    StartItem ();
};

/*QUAKED weapon_grenadelauncher (0 .5 .8) (-16 -16 0) (16 16 32)
*/

void() weapon_grenadelauncher =
{
    precache_model ("progs/g_rock.mdl");
    setmodel (self, "progs/g_rock.mdl");
    self.weapon = 3;
    self.netname = "Grenade Launcher";
    self.touch = weapon_touch;
    setsize (self, '-16 -16 0', '16 16 56');
    StartItem ();
};

/*QUAKED weapon_rocketlauncher (0 .5 .8) (-16 -16 0) (16 16 32)
*/

void() weapon_rocketlauncher =
{
    precache_model ("progs/g_rock2.mdl");
    setmodel (self, "progs/g_rock2.mdl");
    self.weapon = 3;
    self.netname = "Rocket Launcher";
    self.touch = weapon_touch;
    setsize (self, '-16 -16 0', '16 16 56');
    StartItem ();
};


/*QUAKED weapon_lightning (0 .5 .8) (-16 -16 0) (16 16 32)
*/

void() weapon_lightning =
{
    precache_model ("progs/g_light.mdl");
    setmodel (self, "progs/g_light.mdl");
    self.weapon = 3;
    self.netname = "Thunderbolt";
    self.touch = weapon_touch;
    setsize (self, '-16 -16 0', '16 16 56');
    StartItem ();
};


/*
===============================================================================

AMMO

===============================================================================
*/

void() ammo_touch =
{
local entity    stemp;
local float        best;

    if (other.classname != "player")
        return;
    if (other.health <= 0)
        return;

// if the player was using his best weapon, change up to the new one if better

    stemp = self;
    self = other;
    best = W_BestWeapon();
    self = stemp;

// by this point, other is the player and self is the ammo pack. - PGM

// shotgun
    if (self.weapon == 1)
    {
        if (other.ammo_shells1 >= 100)
            return;
        other.ammo_shells1 = other.ammo_shells1 + self.aflag;
    }

// spikes
    if (self.weapon == 2)
    {
        if (other.ammo_nails1 >= 200)
            return;
        other.ammo_nails1 = other.ammo_nails1 + self.aflag;
    }

//    rockets
    if (self.weapon == 3)
    {
        if (other.ammo_rockets1 >= 100)
            return;
        other.ammo_rockets1 = other.ammo_rockets1 + self.aflag;
    }

//    cells
    if (self.weapon == 4)
    {
        if (other.ammo_cells1 >= 100)
            return;
        other.ammo_cells1 = other.ammo_cells1 + self.aflag;
    }

//    lava spikes
    if (self.weapon == 5)
    {
        if(other.ammo_lava_nails >= 200)
            return;
        other.ammo_lava_nails = other.ammo_lava_nails + self.aflag;
    }

//    multi grenades
    if (self.weapon == 6)
    {
        if(other.ammo_multi_rockets >= 100)
            return;
        other.ammo_multi_rockets = other.ammo_multi_rockets + self.aflag;
    }

//    plasma
    if (self.weapon == 7)
    {
        if(other.ammo_plasma >= 100)
            return;
        other.ammo_plasma = other.ammo_plasma + self.aflag;
    }

    EnableComboWeapons ( other );
    UpdateAmmoCounts ( other );

    bound_other_ammo ();

    sprint (other, "You got the ");
    sprint (other, self.netname);
    sprint (other, "\n");
// ammo touch sound
    sound (other, CHAN_ITEM, "weapons/lock4.wav", 1, ATTN_NORM);
    stuffcmd (other, "bf\n");

// change to a better weapon if appropriate

    if ( other.weapon == best )
    {
        stemp = self;
        self = other;
        self.weapon = W_BestWeapon();
        W_SetCurrentAmmo ();
        self = stemp;
    }

// if changed current ammo, update it
    stemp = self;
    self = other;
    W_SetCurrentAmmo();
    self = stemp;

// remove it in single player, or setup for respawning in deathmatch
    self.model = string_null;
    self.solid = SOLID_NOT;
    if (deathmatch == 1)
        self.nextthink = time + 30;
    self.think = SUB_regen;

    activator = other;
    SUB_UseTargets();                // fire all targets / killtargets
};




float WEAPON_BIG2 = 1;

/*QUAKED item_shells (0 .5 .8) (0 0 0) (32 32 32) big
Small box is 20, Big box is 40.
*/

void() item_shells =
{
    self.touch = ammo_touch;

    if (self.spawnflags & WEAPON_BIG2)
    {
        precache_model ("maps/b_shell1.bsp");
        setmodel (self, "maps/b_shell1.bsp");
        self.aflag = 40;
    }
    else
    {
        precache_model ("maps/b_shell0.bsp");
        setmodel (self, "maps/b_shell0.bsp");
        self.aflag = 20;
    }
    self.weapon = 1;
    self.netname = "shells";
    setsize (self, '0 0 0', '32 32 56');
    StartItem ();
};

/*QUAKED item_spikes (0 .5 .8) (0 0 0) (32 32 32) big
Small box is 25, Big box is 50.
*/

void() item_spikes =
{
    self.touch = ammo_touch;

    if (self.spawnflags & WEAPON_BIG2)
    {
        precache_model ("maps/b_nail1.bsp");
        setmodel (self, "maps/b_nail1.bsp");
        self.aflag = 50;
    }
    else
    {
        precache_model ("maps/b_nail0.bsp");
        setmodel (self, "maps/b_nail0.bsp");
        self.aflag = 25;
    }
    self.weapon = 2;
    self.netname = "nails";
    setsize (self, '0 0 0', '32 32 56');
    StartItem ();
};

/*QUAKED item_lava_spikes (0 .5 .8) (0 0 0) (32 32 32) big
Small box is 25, Big box is 50.
*/

void() item_lava_spikes =
{
    self.touch = ammo_touch;

    if (self.spawnflags & WEAPON_BIG2)
    {
        precache_model ("maps/b_lnail1.bsp");
        setmodel (self, "maps/b_lnail1.bsp");
        self.aflag = 50;
    }
    else
    {
        precache_model ("maps/b_lnail0.bsp");
        setmodel (self, "maps/b_lnail0.bsp");
        self.aflag = 25;
    }
    self.weapon = 5;
    self.netname = "lava nails";
    setsize (self, '0 0 0', '32 32 56');
    StartItem ();
};

/*QUAKED item_rockets (0 .5 .8) (0 0 0) (32 32 32) big
Small box is 5, Big box is 10.
*/

void() item_rockets =
{
    self.touch = ammo_touch;

    if (self.spawnflags & WEAPON_BIG2)
    {
        precache_model ("maps/b_rock1.bsp");
        setmodel (self, "maps/b_rock1.bsp");
        self.aflag = 10;
    }
    else
    {
        precache_model ("maps/b_rock0.bsp");
        setmodel (self, "maps/b_rock0.bsp");
        self.aflag = 5;
    }
    self.weapon = 3;
    self.netname = "rockets";
    setsize (self, '0 0 0', '32 32 56');
    StartItem ();
};

/*QUAKED item_multi_rockets (0 .5 .8) (0 0 0) (32 32 32) big
Small box is 5, Big box is 10.
*/

void() item_multi_rockets =
{
    self.touch = ammo_touch;

    if (self.spawnflags & WEAPON_BIG2)
    {
        precache_model ("maps/b_mrock1.bsp");
        setmodel (self, "maps/b_mrock1.bsp");
        self.aflag = 10;
    }
    else
    {
        precache_model ("maps/b_mrock0.bsp");
        setmodel (self, "maps/b_mrock0.bsp");
        self.aflag = 5;
    }
    self.weapon = 6;
    self.netname = "multi rockets";
    setsize (self, '0 0 0', '32 32 56');
    StartItem ();
};


/*QUAKED item_cells (0 .5 .8) (0 0 0) (32 32 32) big
Small box is 6, Big box is 12.
*/
void() item_cells =
{
    self.touch = ammo_touch;

    if (self.spawnflags & WEAPON_BIG2)
    {
        precache_model ("maps/b_batt1.bsp");
        setmodel (self, "maps/b_batt1.bsp");
        self.aflag = 12;
    }
    else
    {
        precache_model ("maps/b_batt0.bsp");
        setmodel (self, "maps/b_batt0.bsp");
        self.aflag = 6;
    }
    self.weapon = 4;
    self.netname = "cells";
    setsize (self, '0 0 0', '32 32 56');
    StartItem ();
};

/*QUAKED item_plasma (0 .5 .8) (0 0 0) (32 32 32) big
Small box is 6, Big box is 12.
*/
void() item_plasma =
{
    self.touch = ammo_touch;

    if (self.spawnflags & WEAPON_BIG2)
    {
        precache_model ("maps/b_plas1.bsp");
        setmodel (self, "maps/b_plas1.bsp");
        self.aflag = 12;
    }
    else
    {
        precache_model ("maps/b_plas0.bsp");
        setmodel (self, "maps/b_plas0.bsp");
        self.aflag = 6;
    }
    self.weapon = 7;
    self.netname = "plasma";
    setsize (self, '0 0 0', '32 32 56');
    StartItem ();
};


/*QUAKED item_weapon (0 .5 .8) (0 0 0) (32 32 32) shotgun rocket spikes big
DO NOT USE THIS!!!! IT WILL BE REMOVED!
*/

float WEAPON_SHOTGUN = 1;
float WEAPON_ROCKET = 2;
float WEAPON_SPIKES = 4;
float WEAPON_BIG = 8;
void() item_weapon =
{
    self.touch = ammo_touch;

    if (self.spawnflags & WEAPON_SHOTGUN)
    {
        if (self.spawnflags & WEAPON_BIG)
        {
            precache_model ("maps/b_shell1.bsp");
            setmodel (self, "maps/b_shell1.bsp");
            self.aflag = 40;
        }
        else
        {
            precache_model ("maps/b_shell0.bsp");
            setmodel (self, "maps/b_shell0.bsp");
            self.aflag = 20;
        }
        self.weapon = 1;
        self.netname = "shells";
    }

    if (self.spawnflags & WEAPON_SPIKES)
    {
        if (self.spawnflags & WEAPON_BIG)
        {
            precache_model ("maps/b_nail1.bsp");
            setmodel (self, "maps/b_nail1.bsp");
            self.aflag = 40;
        }
        else
        {
            precache_model ("maps/b_nail0.bsp");
            setmodel (self, "maps/b_nail0.bsp");
            self.aflag = 20;
        }
        self.weapon = 2;
        self.netname = "spikes";
    }

    if (self.spawnflags & WEAPON_ROCKET)
    {
        if (self.spawnflags & WEAPON_BIG)
        {
            precache_model ("maps/b_rock1.bsp");
            setmodel (self, "maps/b_rock1.bsp");
            self.aflag = 10;
        }
        else
        {
            precache_model ("maps/b_rock0.bsp");
            setmodel (self, "maps/b_rock0.bsp");
            self.aflag = 5;
        }
        self.weapon = 3;
        self.netname = "rockets";
    }

    setsize (self, '0 0 0', '32 32 56');
    StartItem ();
};


/*
===============================================================================

KEYS

===============================================================================
*/

void() key_touch =
{
local entity    stemp;
local float        best;

    if (other.classname != "player")
        return;
    if (other.health <= 0)
        return;
    if (other.items & self.items)
        return;

    sprint (other, "You got the ");
    sprint (other, self.netname);
    sprint (other,"\n");

    sound (other, CHAN_ITEM, self.noise, 1, ATTN_NORM);
    stuffcmd (other, "bf\n");
    other.items = other.items | self.items;

    if (!coop)
    {
        self.solid = SOLID_NOT;
        self.model = string_null;
    }

    activator = other;
    SUB_UseTargets();                // fire all targets / killtargets
};


void() key_setsounds =
{
    if (world.worldtype == 0)
    {
        precache_sound ("misc/medkey.wav");
        self.noise = "misc/medkey.wav";
    }
    if (world.worldtype == 1)
    {
        precache_sound ("misc/runekey.wav");
        self.noise = "misc/runekey.wav";
    }
    if (world.worldtype == 2)
    {
        precache_sound2 ("misc/basekey.wav");
        self.noise = "misc/basekey.wav";
    }
};

/*QUAKED item_key1 (0 .5 .8) (-16 -16 -24) (16 16 32)
SILVER key
In order for keys to work
you MUST set your maps
worldtype to one of the
following:
0: medieval
1: metal
2: base
*/

void() item_key1 =
{
    if (world.worldtype == 0)
    {
        precache_model ("progs/w_s_key.mdl");
        setmodel (self, "progs/w_s_key.mdl");
        self.netname = "silver key";
    }
    else if (world.worldtype == 1)
    {
        precache_model ("progs/m_s_key.mdl");
        setmodel (self, "progs/m_s_key.mdl");
        self.netname = "silver runekey";
    }
    else if (world.worldtype == 2)
    {
        precache_model2 ("progs/b_s_key.mdl");
        setmodel (self, "progs/b_s_key.mdl");
        self.netname = "silver keycard";
    }
    key_setsounds();
    self.touch = key_touch;
    self.items = IT_KEY1;
    setsize (self, '-16 -16 -24', '16 16 32');
    StartItem ();
};

/*QUAKED item_key2 (0 .5 .8) (-16 -16 -24) (16 16 32)
GOLD key
In order for keys to work
you MUST set your maps
worldtype to one of the
following:
0: medieval
1: metal
2: base
*/

void() item_key2 =
{
    if (world.worldtype == 0)
    {
        precache_model ("progs/w_g_key.mdl");
        setmodel (self, "progs/w_g_key.mdl");
        self.netname = "gold key";
    }
    if (world.worldtype == 1)
    {
        precache_model ("progs/m_g_key.mdl");
        setmodel (self, "progs/m_g_key.mdl");
        self.netname = "gold runekey";
    }
    if (world.worldtype == 2)
    {
        precache_model2 ("progs/b_g_key.mdl");
        setmodel (self, "progs/b_g_key.mdl");
        self.netname = "gold keycard";
    }
    key_setsounds();
    self.touch = key_touch;
    self.items = IT_KEY2;
    setsize (self, '-16 -16 -24', '16 16 32');
    StartItem ();
};



/*
===============================================================================

END OF LEVEL RUNES

===============================================================================
*/

void() sigil_touch =
{
local entity    stemp;
local float        best;

    if (other.classname != "player")
        return;
    if (other.health <= 0)
        return;

    centerprint (other, "You got the rune!");

    sound (other, CHAN_ITEM, self.noise, 1, ATTN_NORM);
    stuffcmd (other, "bf\n");
    self.solid = SOLID_NOT;
    self.model = string_null;
    serverflags = serverflags | (self.spawnflags & 15);
    self.classname = "";        // so rune doors won't find it

    activator = other;
    SUB_UseTargets();                // fire all targets / killtargets
};


/*QUAKED item_sigil (0 .5 .8) (-16 -16 -24) (16 16 32) E1 E2 E3 E4
End of level sigil, pick up to end episode and return to jrstart.
*/

void() item_sigil =
{
    if (!self.spawnflags)
        objerror ("no spawnflags");

    precache_sound ("misc/runekey.wav");
    self.noise = "misc/runekey.wav";

    if (self.spawnflags & 1)
    {
        precache_model ("progs/end1.mdl");
        setmodel (self, "progs/end1.mdl");
    }
    if (self.spawnflags & 2)
    {
        precache_model2 ("progs/end2.mdl");
        setmodel (self, "progs/end2.mdl");
    }
    if (self.spawnflags & 4)
    {
        precache_model2 ("progs/end3.mdl");
        setmodel (self, "progs/end3.mdl");
    }
    if (self.spawnflags & 8)
    {
        precache_model2 ("progs/end4.mdl");
        setmodel (self, "progs/end4.mdl");
    }

    self.touch = sigil_touch;
    setsize (self, '-16 -16 -24', '16 16 32');
    StartItem ();
};

/*
===============================================================================

POWERUPS

===============================================================================
*/

void() powerup_touch;
void() random_regen;

void() powerup_touch =
{
local entity    stemp;
local float        best;

    if (other.classname != "player")
        return;
    if (other.health <= 0)
        return;

    sprint (other, "You got the ");
    sprint (other, self.netname);
    sprint (other,"\n");

    if (deathmatch)
    {
        self.mdl = self.model;

        if (self.classname == "item_random_powerup")
        {
            self.nextthink = time + 30;
            self.think = random_regen;
        }
        else
        {
            if ((self.classname == "item_artifact_invulnerability") ||
                (self.classname == "item_artifact_invisibility"))
                self.nextthink = time + 60*5;
            else
                self.nextthink = time + 60;

            self.think = SUB_regen;
        }
    }

    sound (other, CHAN_VOICE, self.noise, 1, ATTN_NORM);
    stuffcmd (other, "bf\n");
    self.solid = SOLID_NOT;
    other.items = other.items | self.items;
    self.model = string_null;

// do the apropriate action
//    if (self.classname == "item_artifact_envirosuit")
    if (self.netname == "Biosuit")
    {
        other.rad_time = 1;
        other.radsuit_finished = time + 30;
    }

//    if (self.classname == "item_artifact_invulnerability")
    if (self.netname == "Pentagram of Protection")
    {
        other.invincible_time = 1;
        other.invincible_finished = time + 30;
    }

//    if (self.classname == "item_artifact_invisibility")
    if (self.netname == "Ring of Shadows")
    {
        other.invisible_time = 1;
        other.invisible_finished = time + 30;
    }

//    if (self.classname == "item_artifact_super_damage")
    if (self.netname == "Quad Damage")
    {
        other.super_time = 1;
        other.super_damage_finished = time + 30;
    }

    activator = other;
    SUB_UseTargets();                // fire all targets / killtargets
};



/*QUAKED item_artifact_invulnerability (0 .5 .8) (-16 -16 -24) (16 16 32)
Player is invulnerable for 30 seconds
*/
void() item_artifact_invulnerability =
{
    self.touch = powerup_touch;

    precache_model ("progs/invulner.mdl");
    precache_sound ("items/protect.wav");
    precache_sound ("items/protect2.wav");
    precache_sound ("items/protect3.wav");
    self.noise = "items/protect.wav";
    setmodel (self, "progs/invulner.mdl");
    self.netname = "Pentagram of Protection";
    self.items = IT_INVULNERABILITY;
    setsize (self, '-16 -16 -24', '16 16 32');
    StartItem ();
};

/*QUAKED item_artifact_envirosuit (0 .5 .8) (-16 -16 -24) (16 16 32)
Player takes no damage from water or slime for 30 seconds
*/
void() item_artifact_envirosuit =
{
    self.touch = powerup_touch;

    precache_model ("progs/suit.mdl");
    precache_sound ("items/suit.wav");
    precache_sound ("items/suit2.wav");
    self.noise = "items/suit.wav";
    setmodel (self, "progs/suit.mdl");
    self.netname = "Biosuit";
    self.items = IT_SUIT;
    setsize (self, '-16 -16 -24', '16 16 32');
    StartItem ();
};


/*QUAKED item_artifact_invisibility (0 .5 .8) (-16 -16 -24) (16 16 32)
Player is invisible for 30 seconds
*/
void() item_artifact_invisibility =
{
    self.touch = powerup_touch;

    precache_model ("progs/invisibl.mdl");
    precache_sound ("items/inv1.wav");
    precache_sound ("items/inv2.wav");
    precache_sound ("items/inv3.wav");
    self.noise = "items/inv1.wav";
    setmodel (self, "progs/invisibl.mdl");
    self.netname = "Ring of Shadows";
    self.items = IT_INVISIBILITY;
    setsize (self, '-16 -16 -24', '16 16 32');
    StartItem ();
};


/*QUAKED item_artifact_super_damage (0 .5 .8) (-16 -16 -24) (16 16 32)
The next attack from the player will do 4x damage
*/
void() item_artifact_super_damage =
{
    self.touch = powerup_touch;

    precache_model ("progs/quaddama.mdl");
    precache_sound ("items/damage.wav");
    precache_sound ("items/damage2.wav");
    precache_sound ("items/damage3.wav");
    self.noise = "items/damage.wav";
    setmodel (self, "progs/quaddama.mdl");
    self.netname = "Quad Damage";
    self.items = IT_QUAD;
    setsize (self, '-16 -16 -24', '16 16 32');
    StartItem ();
};


/*
===============================================================================

PLAYER BACKPACKS

===============================================================================
*/

void() BackpackTouch =
{
    local string    s;
    local    float    best, old, new;
    local        entity    stemp;
    local    float    acount;

    if (other.classname != "player")
        return;
    if (other.health <= 0)
        return;

//ZOID--
    // Don't let the owner pick up his own backpack for a second.
    if ((other == self.owner) && ((self.nextthink - time) > 119))
        return;
//--ZOID

    acount = 0;
    sprint (other, "You get ");

    if (self.items)
        if ((other.items & self.items) == 0)
        {
            acount = 1;
            sprint (other, "the ");
            sprint (other, self.netname);
        }

// if the player was using his best weapon, change up to the new one if better
    stemp = self;
    self = other;
    best = W_BestWeapon();
    self = stemp;

// change weapons
    other.ammo_shells1 = other.ammo_shells1 + self.ammo_shells1;
    other.ammo_nails1 = other.ammo_nails1 + self.ammo_nails1;
    other.ammo_rockets1 = other.ammo_rockets1 + self.ammo_rockets1;
    other.ammo_cells1 = other.ammo_cells1 + self.ammo_cells1;

    other.ammo_lava_nails = other.ammo_lava_nails + self.ammo_lava_nails;
    other.ammo_multi_rockets = other.ammo_multi_rockets +
                                    self.ammo_multi_rockets;
    other.ammo_plasma = other.ammo_plasma + self.ammo_plasma;

    new = self.items;
    if (!new)
        new = other.weapon;
    old = other.items;
    other.items = other.items | new;

    bound_other_ammo ();

    if (self.ammo_shells1)
    {
        if (acount)
            sprint(other, ", ");
        acount = 1;
        s = ftos(self.ammo_shells1);
        sprint (other, s);
        sprint (other, " shells");
    }
    if (self.ammo_nails1)
    {
        if (acount)
            sprint(other, ", ");
        acount = 1;
        s = ftos(self.ammo_nails1);
        sprint (other, s);
        sprint (other, " nails");
    }
    if (self.ammo_rockets1)
    {
        if (acount)
            sprint(other, ", ");
        acount = 1;
        s = ftos(self.ammo_rockets1);
        sprint (other, s);
        sprint (other, " rockets");
    }
    if (self.ammo_cells1)
    {
        if (acount)
            sprint(other, ", ");
        acount = 1;
        s = ftos(self.ammo_cells1);
        sprint (other, s);
        sprint (other, " cells");
    }
    if (self.ammo_lava_nails)
    {
        if (acount)
            sprint(other, ", ");
        acount = 1;
        s = ftos(self.ammo_lava_nails);
        sprint (other, s);
        sprint (other, " lava nails");
    }
    if (self.ammo_multi_rockets)
    {
        if (acount)
            sprint(other, ", ");
        acount = 1;
        s = ftos(self.ammo_multi_rockets);
        sprint (other, s);
        sprint (other, " multi rockets");
    }
    if (self.ammo_plasma)
    {
        if (acount)
            sprint(other, ", ");
        acount = 1;
        s = ftos(self.ammo_plasma);
        sprint (other, s);
        sprint (other, " plasma balls");
    }

    sprint (other, "\n");
// backpack touch sound
    sound (other, CHAN_ITEM, "weapons/lock4.wav", 1, ATTN_NORM);
    stuffcmd (other, "bf\n");

// remove the backpack, change self to the player
    remove(self);
    self = other;

// fixme -- move this to after weapon choice if problematic.
    EnableComboWeapons ( self );

// change to the weapon
    if (!deathmatch)
        self.weapon = new;
    else
        Deathmatch_Weapon (old, new);

//    EnableComboWeapons ( self );
    UpdateAmmoCounts (self);

    W_SetCurrentAmmo ();
};

/*
===============
DropBackpack
===============
*/
void() DropBackpack =
{
    local entity    item;

    // this is to compensate for the new way we're hacking the ammo
    // counts. If they're monsters, just copy shells->shells1 etc.
    if (self.flags & FL_MONSTER)
    {
        self.ammo_shells1 = self.ammo_shells;
        self.ammo_nails1 = self.ammo_nails;
        self.ammo_rockets1 = self.ammo_rockets;
        self.ammo_cells1 = self.ammo_cells;
    }

    if (!(self.ammo_shells1 + self.ammo_nails1 + self.ammo_rockets1 +
            self.ammo_cells1 + self.ammo_lava_nails + self.ammo_multi_rockets))
        return;    // nothing in it

    item = spawn();
    item.origin = self.origin - '0 0 24';

    item.items = self.weapon;
    if (item.items == IT_AXE)
        item.netname = "Axe";
    else if (item.items == IT_SHOTGUN)
        item.netname = "Shotgun";
    else if (item.items == IT_SUPER_SHOTGUN)
        item.netname = "Double-barrelled Shotgun";
    else if (item.items == IT_NAILGUN)
        item.netname = "Nailgun";
    else if (item.items == IT_SUPER_NAILGUN)
        item.netname = "Super Nailgun";
    else if (item.items == IT_GRENADE_LAUNCHER)
        item.netname = "Grenade Launcher";
    else if (item.items == IT_ROCKET_LAUNCHER)
        item.netname = "Rocket Launcher";
    else if (item.items == IT_LIGHTNING)
        item.netname = "Thunderbolt";
    else if (item.items == IT_LAVA_NAILGUN)
    {
        item.netname = "Lava Nail Gun";
        item.items = IT_NAILGUN;
    }
    else if (item.items == IT_LAVA_SUPER_NAILGUN)
    {
        item.netname = "Super Lava Nail Gun";
        item.items = IT_SUPER_NAILGUN;
    }
    else if (item.items == IT_MULTI_GRENADE)
    {
        item.netname = "Multi Grenade Launcher";
        item.items = IT_GRENADE_LAUNCHER;
    }
    else if (item.items == IT_MULTI_ROCKET)
    {
        item.netname = "Multi Rocket Launcher";
        item.items = IT_ROCKET_LAUNCHER;
    }
    else if (item.items == IT_PLASMA_GUN)
    {
        item.netname = "Plasma Gun";
        item.items = IT_LIGHTNING;
    }
    else
        item.netname = "";

    item.ammo_shells1 = self.ammo_shells1;
    item.ammo_nails1 = self.ammo_nails1;
    item.ammo_rockets1 = self.ammo_rockets1;
    item.ammo_cells1 = self.ammo_cells1;
    item.ammo_lava_nails = self.ammo_lava_nails;
    item.ammo_multi_rockets = self.ammo_multi_rockets;
    item.ammo_plasma = self.ammo_plasma;

    item.velocity_z = 300;
    item.velocity_x = -100 + (random() * 200);
    item.velocity_y = -100 + (random() * 200);

    item.flags = FL_ITEM;
    item.solid = SOLID_TRIGGER;
    item.movetype = MOVETYPE_TOSS;
    setmodel (item, "progs/backpack.mdl");
    setsize (item, '-16 -16 0', '16 16 56');
    item.touch = BackpackTouch;

    item.nextthink = time + 120;    // remove after 2 minutes
    item.think = SUB_Remove;
};
