/* ALL LIGHTS SHOULD BE 0 1 0 IN COLOR ALL OTHER ITEMS SHOULD
BE .8 .3 .4 IN COLOR */

void(entity entPlayer) VRItemPickupHaptic =
{
    if(!entPlayer.ishuman)
    {
        return;
    }

    if(VRIsTouchingHandFake(entPlayer))
    {
        haptic(cVR_OffHand, 0.0, 0.3, 75, 1.0);
        haptic(cVR_MainHand, 0.0, 0.3, 75, 1.0);
        return;
    }

    haptic(VRGetTouchingHand(entPlayer), 0.0, 0.3, 75, 1.0);
}

void() SUB_regen =
{
    self.model = self.mdl;        // restore original model
    self.solid = SOLID_TRIGGER;    // allow it to be touched again
    sound(self, CHAN_VOICE, "items/itembk2.wav", 1, ATTN_NORM);    // play respawn sound
    setorigin(self, self.origin);
};



/*QUAKED noclass (0 0 0) (-8 -8 -8) (8 8 8)
prints a warning message when spawned
*/
void() noclass =
{
    dprint("noclass spawned at");
    dprint(vtos(self.origin));
    dprint("\n");
    remove(self);
};

float(entity ent) IsAmmoBox =
{
    return ent.classname == "item_shells" ||
           ent.classname == "item_spikes" ||
           ent.classname == "item_rockets" ||
           ent.classname == "item_cells";
}

void(entity ent, entity entPlayer) SpawnAmmoBoxWeaponsImpl =
{
    if(random() < 0.65)
    {
        return;
    }

    if(ent.classname == "item_shells")
    {
        if(!TryAmmoBoxDrop(ent, entPlayer, WID_AXE, 0.2))
        if(!TryAmmoBoxDrop(ent, entPlayer, WID_SHOTGUN, 0.5))
        if(!TryAmmoBoxDrop(ent, entPlayer, WID_SUPER_SHOTGUN, 0.7))
        { /* Intentionally empty. */ }
    }
    else if(ent.classname == "item_spikes" || ent.classname == "item_lava_nails")
    {
        if(!TryAmmoBoxDrop(ent, entPlayer, WID_NAILGUN, 0.4))
        if(!TryAmmoBoxDrop(ent, entPlayer, WID_SUPER_NAILGUN, 0.5))
        if(!TryAmmoBoxDrop(ent, entPlayer, WID_AXE, 0.2))
        { /* Intentionally empty. */ }
    }
    else if(ent.classname == "item_rockets" || ent.classname == "item_multi_rockets")
    {
        if(!TryAmmoBoxDrop(ent, entPlayer, WID_GRENADE_LAUNCHER, 0.3))
        if(!TryAmmoBoxDrop(ent, entPlayer, WID_PROXIMITY_GUN, 0.4))
        if(!TryAmmoBoxDrop(ent, entPlayer, WID_ROCKET_LAUNCHER, 0.5))
        if(!TryAmmoBoxDrop(ent, entPlayer, WID_AXE, 0.2))
        { /* Intentionally empty. */ }
    }
    else if(ent.classname == "item_cells" || ent.classname == "item_plasma")
    {
        if(!TryAmmoBoxDrop(ent, entPlayer, WID_LIGHTNING, 0.3))
        if(!TryAmmoBoxDrop(ent, entPlayer, WID_LASER_CANNON, 0.4))
        if(!TryAmmoBoxDrop(ent, entPlayer, WID_MJOLNIR, 0.4))
        if(!TryAmmoBoxDrop(ent, entPlayer, WID_AXE, 0.2))
        { /* Intentionally empty. */ }
    }
}

void(entity ent) SpawnAmmoBoxWeapons =
{
    local entity entPlayer;

    entPlayer = find(world, classname, "player");

    if(entPlayer == world)
    {
        bprint("`SpawnAmmoBoxWeapons`: player entity not found\n");
        return;
    }

    SpawnAmmoBoxWeaponsImpl(ent, entPlayer);
}


/*
============
PlaceItem

plants the object on the floor
============
*/
void() PlaceItem =
{
    local float oldz;

    self.mdl = self.model;        // so it can be restored on respawn
    self.flags = FL_ITEM;        // make extra wide
    self.solid = SOLID_TRIGGER;
    self.movetype = MOVETYPE_TOSS;
    self.velocity = '0 0 0';

    if(self.spawnflags & ITEM_FLOATING) // HONEY
    {
        self.movetype = MOVETYPE_NONE;
        setorigin(self, self.origin);
    }
    else
    {
        self.origin_z = self.origin_z + 6;
        oldz = self.origin_z;
        if(!droptofloor())
        {
            dprint("Bonus item fell out of level!\n");
            dprint(self.classname);
            dprint(" at ");
            dprint(vtos(self.origin));
            dprint("\n");
            remove(self);
            return;
        }
    }

    // HONEY
    if(self.spawnflags & ITEM_SECRET)
    {
        total_secrets = total_secrets + 1;
        if(!self.message)
            self.message = "You found a secret item!";
        if(skill == 0 || world.model == "maps/start.bsp"){
            ambient_secret_think();
        }
    }

    // HONEY
    self.th_die = self.handtouch; // Overriding self.touch and saving it to support items flagged as secret
    self.handtouch = itemTouch;

    // HONEY
    if(self.spawnflags & SPAWNFLAG_TRIGGER_SPAWN)
    {
        item_unspawn();
    }

    SpawnAmmoBoxWeapons(self);
};

/*
============
StartItem

Sets the clipping size and plants the object on the floor
============
*/
void() StartItem =
{
    // TODO VR: (P2) doesn't seem to ever be called?

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
    if(e.health <= 0)
        return 0;

    if((!ignore) && (e.health >= other.max_health))
        return 0;

    healamount = ceil(healamount);

    e.health = e.health + healamount;

    if((!ignore) && (e.health >= other.max_health))
        e.health = other.max_health;

    if(e.health > 250)
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

void() item_health =
{
    self.handtouch = health_touch;

    if(self.spawnflags & H_ROTTEN)
    {
        precache_model("maps/b_bh10.bsp");

        precache_sound("items/r_item1.wav");
        setmodel(self, "maps/b_bh10.bsp");
        self.noise = "items/r_item1.wav";
        self.healamount = 15;
        self.healtype = 0;
        setsize(self, '0 0 0', '32 32 16');
    }
    else if(self.spawnflags & H_MEGA)
    {
        precache_model("maps/b_bh100.bsp");
        precache_sound("items/r_item2.wav");
        setmodel(self, "maps/b_bh100.bsp");
        self.noise = "items/r_item2.wav";
        self.healamount = 100;
        self.healtype = 2;
        setsize(self, '0 0 0', '32 32 32');
    }
    else
    {
        precache_model("maps/b_bh25.bsp");
        precache_sound("items/health1.wav");
        setmodel(self, "maps/b_bh25.bsp");
        self.noise = "items/health1.wav";
        self.healamount = 25;
        self.healtype = 1;
        setsize(self, '0 0 0', '32 32 16');
    }

    if(deathmatch == 1) {
        self.wait = 20;
    }

    StartItem();
};


void() health_touch =
{
    local    string    s;

    if(self.healtype == 2) // Megahealth?  Ignore max_health...
    {
        if(other.health >= 250)
            return;
        if(!T_Heal(other, self.healamount, 1))
            return;
    }
    else
    {
        if(!T_Heal(other, self.healamount, 0))
            return;
    }

    VRItemPickupHaptic(other);

    sprint(other, "You receive ");
    s = ftos(self.healamount);
    sprint(other, s);
    sprint(other, " health\n");

// health touch sound
    sound(other, CHAN_ITEM, self.noise, 1, ATTN_NORM);

    stuffcmd(other, "bf\n");

    self.model = string_null;
    self.solid = SOLID_NOT;

    // Megahealth = rot down the player's super health
    if(self.healtype == 2 && !RuneHasElder(other))
    {
        VR_ItemUtil_EntAddItem(other, IID_SUPERHEALTH);
        self.nextthink = time + 5;
        self.think = item_megahealth_rot;
        self.owner = other;
    }
};

void() item_megahealth_rot =
{
    other = self.owner;

    if(other.health > other.max_health)
    {
        other.health = other.health - 1;
        self.nextthink = time + 1;
        return;
    }

    // it is possible for a player to die and respawn between rots, so don't
    // just blindly subtract the flag off
    VR_ItemUtil_EntDelItem(other, IID_SUPERHEALTH);
};

/*
===============================================================================

ARMOR

===============================================================================
*/

void() armor_touch =
{
    local float type, value, itemId;

    if(self.classname == "item_armor1")
    {
        type = 0.3;
        value = 100;
        itemId = IID_ARMOR1;
    }
    else if(self.classname == "item_armor2")
    {
        type = 0.6;
        value = 150;
        itemId = IID_ARMOR2;
    }
    else if(self.classname == "item_armorInv")
    {
        type = 0.8;
        value = 200;
        itemId = IID_ARMOR3;
    }

    if(other.armortype*other.armorvalue >= type*value)
    {
        return;
    }

    other.armortype = type;
    other.armorvalue = value;

    VR_ItemUtil_EntDelItem(other, IID_ARMOR1);
    VR_ItemUtil_EntDelItem(other, IID_ARMOR2);
    VR_ItemUtil_EntDelItem(other, IID_ARMOR3);

    VR_ItemUtil_EntAddItem(other, itemId);

    self.solid = SOLID_NOT;
    self.model = string_null;

    VRItemPickupHaptic(other);

    sprint(other, "You got armor\n");
// armor touch sound
    sound(other, CHAN_ITEM, "items/armor1.wav", 1, ATTN_NORM);
    stuffcmd(other, "bf\n");
};


/*QUAKED item_armor1 (0 .5 .8) (-16 -16 0) (16 16 32)
*/

void() item_armor1 =
{
    self.handtouch = armor_touch;
    precache_model("progs/armor.mdl");
    setmodel(self, "progs/armor.mdl");
    self.skin = 0;
    setsize(self, '-10 -10 0', '10 10 28');
    if(deathmatch == 1) { self.wait = 20; }
    StartItem();
};

/*QUAKED item_armor2 (0 .5 .8) (-16 -16 0) (16 16 32)
*/

void() item_armor2 =
{
    self.handtouch = armor_touch;
    precache_model("progs/armor.mdl");
    setmodel(self, "progs/armor.mdl");
    self.skin = 1;
    setsize(self, '-10 -10 0', '10 10 28');
    if(deathmatch == 1) { self.wait = 20; }
    StartItem();
};

/*QUAKED item_armorInv (0 .5 .8) (-16 -16 0) (16 16 32)
*/

void() item_armorInv =
{
    self.handtouch = armor_touch;
    precache_model("progs/armor.mdl");
    setmodel(self, "progs/armor.mdl");
    self.skin = 2;
    setsize(self, '-10 -10 0', '10 10 28');
    if(deathmatch == 1) { self.wait = 20; }
    StartItem();
};

void() item_armorinv =
{
    item_armorInv();
}

/*
===============================================================================

WEAPONS

===============================================================================
*/

void() bound_other_ammo =
{
    if(other.ammo_shells > 100)
        other.ammo_shells = 100;
    if(other.ammo_nails > 200)
        other.ammo_nails = 200;
    if(other.ammo_rockets > 100)
        other.ammo_rockets = 100;
    if(other.ammo_cells > 100)
        other.ammo_cells = 100;

    // ROGUE
    // PGM addition
    if(other.ammo_lava_nails > 200)
        other.ammo_lava_nails = 200;
    if(other.ammo_multi_rockets > 100)
        other.ammo_multi_rockets = 100;
    if(other.ammo_plasma > 100)
        other.ammo_plasma = 100;
};


//MED 01/06/97 added hipnotic weapons into rankings
float(float w) RankForWeapon =
{
    if(w == WID_LIGHTNING)
        return 1;
    if(w == WID_ROCKET_LAUNCHER)
        return 2;
    if(w == WID_LASER_CANNON)
        return 3;
    if(w == WID_SUPER_NAILGUN)
        return 4;
    if(w == WID_PROXIMITY_GUN)
        return 5;
    if(w == WID_GRENADE_LAUNCHER)
        return 6;
    if(w == WID_SUPER_SHOTGUN)
        return 7;
    if(w == WID_NAILGUN)
        return 8;
    if(w == WID_MJOLNIR)
        return 9;
   return 10;
};

/*
=============
weapon_touch
=============
*/
void() weapon_touch =
{
    local float  hadammo;
    local entity stemp;
    local float  leave;

    // Do not pick up the weapon if the touching hand is not grabbing.
    if(cvar_hget(cvarh_vr_body_interactions) == 0 &&
        cvar_hget(cvarh_vr_enabled) == 1 &&
        other.ishuman)
    {
        if(!VRIsEntHandGrabbing(other, VRGetTouchingHand(other)) ||
            VRGetEntWeapon(other, VRGetTouchingHand(other)) != WID_FIST)
        {
            return;
        }
    }

    if(deathmatch == 2 || coop)
        leave = 1;
    else
        leave = 0;

    local float weaponId;
    weaponId = WeaponClassNameToWeaponId(self.classname);

    if(weaponId == WID_FIST)
    {
        objerror ("weapon_touch: unknown classname");
    }

    if(leave && VR_WeaponUtil_EntHasWeapon(other, weaponId))
    {
        return;
    }

    local float ammoId;
    ammoId = WeaponIdToAmmoId(weaponId, 0 /* weaponflags */);

    hadammo = VR_AmmoUtil_GetAmmoCountByAmmoId(other, ammoId);

    VR_AmmoUtil_SetAmmoCountByAmmoId(other, ammoId,
        hadammo + WeaponIdToInitialAmmoCount(weaponId));

    VRItemPickupHaptic(other);

    sprint(other, "You got the ");
    sprint(other, self.netname);
    sprint(other, "\n");

    // weapon touch sound
    sound(other, CHAN_ITEM, "weapons/pkup.wav", 1, ATTN_NORM);
    stuffcmd(other, "bf\n");

    bound_other_ammo();

    VR_WeaponUtil_EntAddWeapon(other, weaponId);

    stemp = self;
    self = other;

    local float touchinghandIsBody;
    touchinghandIsBody = VRIsTouchingHandFake(other);

    if(touchinghandIsBody)
    {
        if(other.ishuman)
        {
            if(VRIsHandEmpty(other, cVR_MainHand))
            {
                VRSetWeapon(cVR_MainHand, weaponId);
                W_SetCurrentAmmoFor(cVR_MainHand);
            }
            else if(VRIsHandEmpty(other, cVR_OffHand))
            {
                VRSetWeapon(cVR_OffHand, weaponId);
                W_SetCurrentAmmoFor(cVR_OffHand);
            }
        }
        else
        {
            DropWeaponInHand(other, cVR_MainHand,
                VRGetEntWeapon(other, cVR_MainHand),
                VRGetEntWeaponFlags(other, cVR_MainHand));
        }
    }
    else
    {
        local float touchinghand;
        touchinghand = VRGetTouchingHand(other);

        VRSetWeapon(touchinghand, weaponId);
        W_SetCurrentAmmoFor(touchinghand);
    }


    self = stemp;

    if(leave)
        return;

// remove it in single player, or setup for respawning in deathmatch
    self.model = string_null;
    self.solid = SOLID_NOT;
};


/*QUAKED weapon_supershotgun (0 .5 .8) (-16 -16 0) (16 16 32)
*/

void() weapon_supershotgun =
{
    precache_model("progs/g_shot.mdl");
    setmodel(self, "progs/g_shot.mdl");
    self.weapon = WID_SUPER_SHOTGUN;
    self.netname = "Double-barrelled Shotgun";
    self.handtouch = weapon_touch;
    setsize(self, '-8 -8 16', '8 8 38');
    if(deathmatch == 1) { self.wait = 30; }
    StartItem();
};

/*QUAKED weapon_nailgun (0 .5 .8) (-16 -16 0) (16 16 32)
*/

void() weapon_nailgun =
{
    precache_model("progs/g_nail.mdl");
    setmodel(self, "progs/g_nail.mdl");
    self.weapon = WID_NAILGUN;
    self.netname = "nailgun";
    self.handtouch = weapon_touch;
    setsize(self, '-8 -8 16', '8 8 38');
    if(deathmatch == 1) { self.wait = 30; }
    StartItem();
};

/*QUAKED weapon_supernailgun (0 .5 .8) (-16 -16 0) (16 16 32)
*/

void() weapon_supernailgun =
{
    precache_model("progs/g_nail2.mdl");
    setmodel(self, "progs/g_nail2.mdl");
    self.weapon = WID_SUPER_NAILGUN;
    self.netname = "Super Nailgun";
    self.handtouch = weapon_touch;
    setsize(self, '-8 -8 16', '8 8 38');
    if(deathmatch == 1) { self.wait = 30; }
    StartItem();
};

/*QUAKED weapon_grenadelauncher (0 .5 .8) (-16 -16 0) (16 16 32)
*/

void() weapon_grenadelauncher =
{
    precache_model("progs/g_rock.mdl");
    setmodel(self, "progs/g_rock.mdl");
    self.weapon = WID_GRENADE_LAUNCHER;
    self.netname = "Grenade Launcher";
    self.handtouch = weapon_touch;
    setsize(self, '-8 -8 16', '8 8 38');
    if(deathmatch == 1) { self.wait = 30; }
    StartItem();
};

/*QUAKED weapon_rocketlauncher (0 .5 .8) (-16 -16 0) (16 16 32)
*/

void() weapon_rocketlauncher =
{
    precache_model("progs/g_rock2.mdl");
    setmodel(self, "progs/g_rock2.mdl");
    self.weapon = WID_ROCKET_LAUNCHER;
    self.netname = "Rocket Launcher";
    self.handtouch = weapon_touch;
    setsize(self, '-8 -8 16', '8 8 38');
    if(deathmatch == 1) { self.wait = 30; }
    StartItem();
};


/*QUAKED weapon_lightning (0 .5 .8) (-16 -16 0) (16 16 32)
*/

void() weapon_lightning =
{
    precache_model("progs/g_light.mdl");
    setmodel(self, "progs/g_light.mdl");
    self.weapon = WID_LIGHTNING;
    self.netname = "Thunderbolt";
    self.handtouch = weapon_touch;
    setsize(self, '-8 -8 16', '8 8 38');
    if(deathmatch == 1) { self.wait = 30; }
    StartItem();
};


/*
===============================================================================

AMMO

===============================================================================
*/

void() ammo_touch =
{
    local entity stemp;

    // shotgun
    if(self.weapon == 1)
    {
        if(other.ammo_shells >= 100) { return; }
        other.ammo_shells = other.ammo_shells + self.aflag;
    }
    // spikes
    else if(self.weapon == 2)
    {
        if(other.ammo_nails >= 200) { return; }
        other.ammo_nails = other.ammo_nails + self.aflag;
    }
    // rockets
    else if(self.weapon == 3)
    {
        if(other.ammo_rockets >= 100) { return; }
        other.ammo_rockets = other.ammo_rockets + self.aflag;
    }
    // cells
    else if(self.weapon == 4)
    {
        if(other.ammo_cells >= 100) { return; }
        other.ammo_cells = other.ammo_cells + self.aflag;
    }
    // lava spikes
    else if(self.weapon == 5)
    {
        if(other.ammo_lava_nails >= 200) { return; }
        other.ammo_lava_nails = other.ammo_lava_nails + self.aflag;
    }
    // multi grenades
    else if(self.weapon == 6)
    {
        if(other.ammo_multi_rockets >= 100) { return; }
        other.ammo_multi_rockets = other.ammo_multi_rockets + self.aflag;
    }
    // plasma
    else if(self.weapon == 7)
    {
        if(other.ammo_plasma >= 100) { return; }
        other.ammo_plasma = other.ammo_plasma + self.aflag;
    }

    bound_other_ammo();

    VRItemPickupHaptic(other);

    sprint(other, "You got the ");
    sprint(other, self.netname);
    sprint(other, "\n");

    // ammo touch sound
    sound(other, CHAN_ITEM, "weapons/lock4.wav", 1, ATTN_NORM);
    stuffcmd(other, "bf\n");

    // if changed current ammo, update it
    stemp = self;
    self = other;
    W_SetCurrentAmmo();
    self = stemp;

    // remove it in single player, or setup for respawning in deathmatch
    self.model = string_null;
    self.solid = SOLID_NOT;
};




float WEAPON_BIG2 = 1;

/*QUAKED item_shells (0 .5 .8) (0 0 0) (32 32 32) big
Small box is 20, Big box is 40.
*/

void() item_shells =
{
    self.handtouch = ammo_touch;

    if(self.spawnflags & WEAPON_BIG2)
    {
        precache_model("maps/b_shell1.bsp");
        setmodel(self, "maps/b_shell1.bsp");
        self.aflag = 40;
    }
    else
    {
        precache_model("maps/b_shell0.bsp");
        setmodel(self, "maps/b_shell0.bsp");
        self.aflag = 20;
    }

    self.weapon = 1;
    self.netname = "shells";
    self.classname = "item_shells";

    setsize(self, '0 0 0', '24 24 24');
    if(deathmatch == 1) { self.wait = 30; }
    StartItem();
};

/*QUAKED item_spikes (0 .5 .8) (0 0 0) (32 32 32) big
Small box is 25, Big box is 50.
*/

void() item_spikes =
{
    self.handtouch = ammo_touch;

    if(self.spawnflags & WEAPON_BIG2)
    {
        precache_model("maps/b_nail1.bsp");
        setmodel(self, "maps/b_nail1.bsp");
        self.aflag = 50;
    }
    else
    {
        precache_model("maps/b_nail0.bsp");
        setmodel(self, "maps/b_nail0.bsp");
        self.aflag = 25;
    }

    self.weapon = 2;
    self.netname = "nails";
    self.classname = "item_spikes";

    setsize(self, '0 0 0', '24 24 24');
    if(deathmatch == 1) { self.wait = 30; }
    StartItem();
};

/*QUAKED item_lava_spikes (0 .5 .8) (0 0 0) (32 32 32) big
Small box is 25, Big box is 50.
*/

void() item_lava_spikes =
{
    self.handtouch = ammo_touch;

    if(self.spawnflags & WEAPON_BIG2)
    {
        precache_model ("maps/b_lnail1.bsp");
        setmodel(self, "maps/b_lnail1.bsp");
        self.aflag = 50;
    }
    else
    {
        precache_model ("maps/b_lnail0.bsp");
        setmodel(self, "maps/b_lnail0.bsp");
        self.aflag = 25;
    }

    self.weapon = 5;
    self.netname = "lava nails";
    self.classname = "item_lava_nails";

    if(deathmatch == 1) { self.wait = 30; }
    setsize(self, '0 0 0', '32 32 56');
    StartItem ();
};


/*QUAKED item_rockets (0 .5 .8) (0 0 0) (32 32 32) big
Small box is 5, Big box is 10.
*/

void() item_rockets =
{
    self.handtouch = ammo_touch;

    if(self.spawnflags & WEAPON_BIG2)
    {
        precache_model("maps/b_rock1.bsp");
        setmodel(self, "maps/b_rock1.bsp");
        self.aflag = 10;
    }
    else
    {
        precache_model("maps/b_rock0.bsp");
        setmodel(self, "maps/b_rock0.bsp");
        self.aflag = 5;
    }

    self.weapon = 3;
    self.netname = "rockets";
    self.classname = "item_rockets";

    if(deathmatch == 1) { self.wait = 30; }
    setsize(self, '0 0 0', '16 16 32');
    StartItem();
};

/*QUAKED item_multi_rockets (0 .5 .8) (0 0 0) (32 32 32) big
Small box is 5, Big box is 10.
*/

void() item_multi_rockets =
{
    self.handtouch = ammo_touch;

    if(self.spawnflags & WEAPON_BIG2)
    {
        precache_model ("maps/b_mrock1.bsp");
        setmodel(self, "maps/b_mrock1.bsp");
        self.aflag = 10;
    }
    else
    {
        precache_model ("maps/b_mrock0.bsp");
        setmodel(self, "maps/b_mrock0.bsp");
        self.aflag = 5;
    }

    self.weapon = 6;
    self.netname = "multi rockets";
    self.classname = "item_multi_rockets";

    if(deathmatch == 1) { self.wait = 30; }
    setsize(self, '0 0 0', '32 32 56');
    StartItem ();
};

/*QUAKED item_cells (0 .5 .8) (0 0 0) (32 32 32) big
Small box is 6, Big box is 12.
*/

void() item_cells =
{
    self.handtouch = ammo_touch;

    if(self.spawnflags & WEAPON_BIG2)
    {
        precache_model("maps/b_batt1.bsp");
        setmodel(self, "maps/b_batt1.bsp");
        self.aflag = 12;
    }
    else
    {
        precache_model("maps/b_batt0.bsp");
        setmodel(self, "maps/b_batt0.bsp");
        self.aflag = 6;
    }

    self.weapon = 4;
    self.netname = "cells";
    self.classname = "item_cells";

    if(deathmatch == 1) { self.wait = 30; }
    setsize(self, '0 0 0', '24 24 24');
    StartItem();
};

/*QUAKED item_plasma (0 .5 .8) (0 0 0) (32 32 32) big
Small box is 6, Big box is 12.
*/
void() item_plasma =
{
    self.handtouch = ammo_touch;

    if(self.spawnflags & WEAPON_BIG2)
    {
        precache_model ("maps/b_plas1.bsp");
        setmodel(self, "maps/b_plas1.bsp");
        self.aflag = 12;
    }
    else
    {
        precache_model ("maps/b_plas0.bsp");
        setmodel(self, "maps/b_plas0.bsp");
        self.aflag = 6;
    }

    self.weapon = 7;
    self.netname = "plasma";
    self.classname = "item_plasma";

    if(deathmatch == 1) { self.wait = 30; }
    setsize(self, '0 0 0', '32 32 56');
    StartItem ();
};

/*QUAKED item_weapon (0 .5 .8) (0 0 0) (32 32 32) shotgun rocket spikes big
DO NOT USE THIS!!!! IT WILL BE REMOVED!
*/

/*
float WEAPON_SHOTGUN = 1;
float WEAPON_ROCKET = 2;
float WEAPON_SPIKES = 4;
float WEAPON_BIG = 8;
void() item_weapon =
{
    self.handtouch = ammo_touch;

    if(self.spawnflags & WEAPON_SHOTGUN)
    {
        if(self.spawnflags & WEAPON_BIG)
        {
            precache_model("maps/b_shell1.bsp");
            setmodel(self, "maps/b_shell1.bsp");
            self.aflag = 40;
        }
        else
        {
            precache_model("maps/b_shell0.bsp");
            setmodel(self, "maps/b_shell0.bsp");
            self.aflag = 20;
        }
        self.weapon = 1;
        self.netname = "shells";
    }

    if(self.spawnflags & WEAPON_SPIKES)
    {
        if(self.spawnflags & WEAPON_BIG)
        {
            precache_model("maps/b_nail1.bsp");
            setmodel(self, "maps/b_nail1.bsp");
            self.aflag = 40;
        }
        else
        {
            precache_model("maps/b_nail0.bsp");
            setmodel(self, "maps/b_nail0.bsp");
            self.aflag = 20;
        }
        self.weapon = 2;
        self.netname = "spikes";
    }

    if(self.spawnflags & WEAPON_ROCKET)
    {
        if(self.spawnflags & WEAPON_BIG)
        {
            precache_model("maps/b_rock1.bsp");
            setmodel(self, "maps/b_rock1.bsp");
            self.aflag = 10;
        }
        else
        {
            precache_model("maps/b_rock0.bsp");
            setmodel(self, "maps/b_rock0.bsp");
            self.aflag = 5;
        }
        self.weapon = 3;
        self.netname = "rockets";
    }

    setsize(self, '0 0 0', '24 24 24');
    StartItem();
};
*/

/*
===============================================================================

KEYS

===============================================================================
*/

void() key_touch =
{
    if(VR_ItemUtil_EntHasItem(other, self.vr_itemId))
        return;

    VRItemPickupHaptic(other);

    sprint(other, "You got the ");
    sprint(other, self.netname);
    sprint(other,"\n");

    sound(other, CHAN_ITEM, self.noise, 1, ATTN_NORM);
    stuffcmd(other, "bf\n");

    VR_ItemUtil_EntAddItem(other, self.vr_itemId);

    if(!coop)
    {
        self.solid = SOLID_NOT;
        self.model = string_null;
    }
};


void() key_setsounds =
{
    if(world.worldtype == 0)
    {
        precache_sound("misc/medkey.wav");
        self.noise = "misc/medkey.wav";
    }
    else if(world.worldtype == 1)
    {
        precache_sound("misc/runekey.wav");
        self.noise = "misc/runekey.wav";
    }
    else if(world.worldtype == 2)
    {
        precache_sound2("misc/basekey.wav");
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
    if(world.worldtype == 0)
    {
        precache_model("progs/w_s_key.mdl");
        setmodel(self, "progs/w_s_key.mdl");
        self.netname = "silver key";
    }
    else if(world.worldtype == 1)
    {
        precache_model("progs/m_s_key.mdl");
        setmodel(self, "progs/m_s_key.mdl");
        self.netname = "silver runekey";
    }
    else if(world.worldtype == 2)
    {
        precache_model2("progs/b_s_key.mdl");
        setmodel(self, "progs/b_s_key.mdl");
        self.netname = "silver keycard";
    }

    key_setsounds();
    self.handtouch = key_touch;
    self.vr_itemId = IID_KEY1;

    setsize(self, '-16 -16 -24', '16 16 32');

    StartItem();
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
    if(world.worldtype == 0)
    {
        precache_model("progs/w_g_key.mdl");
        setmodel(self, "progs/w_g_key.mdl");
        self.netname = "gold key";
    }
    else if(world.worldtype == 1)
    {
        precache_model("progs/m_g_key.mdl");
        setmodel(self, "progs/m_g_key.mdl");
        self.netname = "gold runekey";
    }
    else if(world.worldtype == 2)
    {
        precache_model2("progs/b_g_key.mdl");
        setmodel(self, "progs/b_g_key.mdl");
        self.netname = "gold keycard";
    }

    key_setsounds();
    self.handtouch = key_touch;
    self.vr_itemId = IID_KEY2;

    setsize(self, '-16 -16 -24', '16 16 32');

    // TODO VR: (P2) these cannot be made smaller otherwise they get OOB in
    // some maps. Need to fix up origin but haven't figured out how.
    // setsize(self, '-8 -8 -24', '8 8 32');
    // self.origin = self.origin + '0 -16 0';

    StartItem();
};



/*
===============================================================================

END OF LEVEL RUNES

===============================================================================
*/

void() sigil_touch =
{
    centerprint(other, "You got the rune!");

    sound(other, CHAN_ITEM, self.noise, 1, ATTN_NORM);
    stuffcmd(other, "bf\n");
    self.solid = SOLID_NOT;
    self.model = string_null;
    serverflags = serverflags | (self.spawnflags & 15);
    self.classname = "";        // so rune doors won't find it
};


/*QUAKED item_sigil (0 .5 .8) (-16 -16 -24) (16 16 32) E1 E2 E3 E4
End of level sigil, pick up to end episode and return to jrstart.
*/

void() item_sigil =
{
    if(!self.spawnflags)
        objerror ("no spawnflags");

    precache_sound("misc/runekey.wav");
    self.noise = "misc/runekey.wav";

    if(self.spawnflags & 1)
    {
        precache_model("progs/end1.mdl");
        setmodel(self, "progs/end1.mdl");
    }

    if(self.spawnflags & 2)
    {
        precache_model2("progs/end2.mdl");
        setmodel(self, "progs/end2.mdl");
    }

    if(self.spawnflags & 4)
    {
        precache_model2("progs/end3.mdl");
        setmodel(self, "progs/end3.mdl");
    }

    if(self.spawnflags & 8)
    {
        precache_model2("progs/end4.mdl");
        setmodel(self, "progs/end4.mdl");
    }

    self.handtouch = sigil_touch;

    setsize(self, '-16 -16 -24', '16 16 32');
    StartItem();
};

/*
===============================================================================

POWERUPS

===============================================================================
*/

void() powerup_touch =
{
    VRItemPickupHaptic(other);

    sprint(other, "You got the ");
    sprint(other, self.netname);
    sprint(other,"\n");

    if(deathmatch)
    {
        self.mdl = self.model;

        if(self.classname == "item_random_powerup")
        {
            self.nextthink = time + 30;
            self.think = random_regen;
        }
        else
        {
            if((self.classname == "item_artifact_invulnerability") ||
                (self.classname == "item_artifact_invisibility"))
                self.nextthink = time + 60*5;
            else
                self.nextthink = time + 60;

            self.think = SUB_regen;
        }
    }

    sound(other, CHAN_VOICE, self.noise, 1, ATTN_NORM);
    stuffcmd(other, "bf\n");
    self.solid = SOLID_NOT;
    VR_ItemUtil_EntAddItem(other, self.vr_itemId);
    self.model = string_null;

// do the apropriate action
    if(self.classname == "item_artifact_envirosuit")
    {
        other.rad_time = 1;
        other.radsuit_finished = time + 30;
    }

    if(self.classname == "item_artifact_invulnerability")
    {
        other.invincible_time = 1;
        other.invincible_finished = time + 30;
    }

    if(self.classname == "item_artifact_invisibility")
    {
        other.invisible_time = 1;
        other.invisible_finished = time + 30;
    }

    if(self.classname == "item_artifact_super_damage")
    {
        other.super_time = 1;
        other.super_damage_finished = time + 30;
    }
};



/*QUAKED item_artifact_invulnerability (0 .5 .8) (-16 -16 -24) (16 16 32)
Player is invulnerable for 30 seconds
*/
void() item_artifact_invulnerability =
{
    self.handtouch = powerup_touch;

    precache_model("progs/invulner.mdl");
    precache_sound("items/protect.wav");
    precache_sound("items/protect2.wav");
    precache_sound("items/protect3.wav");
    self.noise = "items/protect.wav";
    setmodel(self, "progs/invulner.mdl");
    self.classname = "item_artifact_invulnerability";
    self.netname = "Pentagram of Protection";
    self.vr_itemId = IID_INVULNERABILITY;
    setsize(self, '-16 -16 -24', '16 16 32');
    if(deathmatch == 1) { self.wait = 60*5; }
    StartItem();
};

/*QUAKED item_artifact_envirosuit (0 .5 .8) (-16 -16 -24) (16 16 32)
Player takes no damage from water or slime for 30 seconds
*/

void() item_artifact_envirosuit =
{
    self.handtouch = powerup_touch;

    precache_model("progs/suit.mdl");
    precache_sound("items/suit.wav");
    precache_sound("items/suit2.wav");
    self.noise = "items/suit.wav";
    setmodel(self, "progs/suit.mdl");
    self.classname = "item_artifact_envirosuit";
    self.netname = "Biosuit";
    self.vr_itemId = IID_SUIT;
    // TODO VR: (P2) changing 16 to 8 makes biosuit disappear from E1M1, why?
    setsize(self, '-16 -16 -24', '16 16 32');
    // setsize(self, '-8 -8 -24', '8 8 32');
    // setorigin(self, self.origin + '-8 -8 0');
    if(deathmatch == 1) { self.wait = 60; }
    StartItem();
};


/*QUAKED item_artifact_invisibility (0 .5 .8) (-16 -16 -24) (16 16 32)
Player is invisible for 30 seconds
*/
void() item_artifact_invisibility =
{
    self.handtouch = powerup_touch;

    precache_model("progs/invisibl.mdl");
    precache_sound("items/inv1.wav");
    precache_sound("items/inv2.wav");
    precache_sound("items/inv3.wav");
    self.noise = "items/inv1.wav";
    setmodel(self, "progs/invisibl.mdl");
    self.classname = "item_artifact_invisibility";
    self.netname = "Ring of Shadows";
    self.vr_itemId = IID_INVISIBILITY;
    setsize(self, '-16 -16 -24', '16 16 32');
    if(deathmatch == 1) { self.wait = 60*5; }
    StartItem();
};


/*QUAKED item_artifact_super_damage (0 .5 .8) (-16 -16 -24) (16 16 32)
The next attack from the player will do 4x damage
*/
void() item_artifact_super_damage =
{
    self.handtouch = powerup_touch;

    precache_model("progs/quaddama.mdl");
    precache_sound("items/damage.wav");
    precache_sound("items/damage2.wav");
    precache_sound("items/damage3.wav");
    self.noise = "items/damage.wav";
    setmodel(self, "progs/quaddama.mdl");
    self.classname = "item_artifact_super_damage";
    self.netname = "Quad Damage";
    self.vr_itemId = IID_QUAD;
    setsize(self, '-16 -16 -24', '16 16 32');
    if(deathmatch == 1) { self.wait = 60; }
    StartItem();
};



/*
===============================================================================

PLAYER BACKPACKS

===============================================================================
*/

void() BackpackTouch =
{
    local string s;
    local float old;
    local entity stemp;
    local float acount;

    if(other.classname != "player")
    {
        return;
    }

    if(other.health <= 0)
    {
        return;
    }

    //ZOID--
    // Don't let the owner pick up his own backpack for a second.
    if((other == self.owner) && ((self.nextthink - time) > 119))
        return;
    //--ZOID

    VRItemPickupHaptic(other);

    acount = 0;
    sprint(other, "You get ");

    other.ammo_shells = other.ammo_shells + self.ammo_shells;
    other.ammo_nails = other.ammo_nails + self.ammo_nails;
    other.ammo_rockets = other.ammo_rockets + self.ammo_rockets;
    other.ammo_cells = other.ammo_cells + self.ammo_cells;

    other.ammo_lava_nails = other.ammo_lava_nails + self.ammo_lava_nails;
    other.ammo_multi_rockets = other.ammo_multi_rockets + self.ammo_multi_rockets;
    other.ammo_plasma = other.ammo_plasma + self.ammo_plasma;

    bound_other_ammo();

    if(self.ammo_shells)
    {
        if(acount)
            sprint(other, ", ");
        acount = 1;
        s = ftos(self.ammo_shells);
        sprint(other, s);
        sprint(other, " shells");
    }

    if(self.ammo_nails)
    {
        if(acount)
            sprint(other, ", ");
        acount = 1;
        s = ftos(self.ammo_nails);
        sprint(other, s);
        sprint(other, " nails");
    }

    if(self.ammo_rockets)
    {
        if(acount)
            sprint(other, ", ");
        acount = 1;
        s = ftos(self.ammo_rockets);
        sprint(other, s);
        sprint(other, " rockets");
    }

    if(self.ammo_cells)
    {
        if(acount)
            sprint(other, ", ");
        acount = 1;
        s = ftos(self.ammo_cells);
        sprint(other, s);
        sprint(other, " cells");
    }

    if(self.ammo_lava_nails)
    {
        if(acount)
            sprint(other, ", ");
        acount = 1;
        s = ftos(self.ammo_lava_nails);
        sprint(other, s);
        sprint(other, " lava nails");
    }

    if(self.ammo_multi_rockets)
    {
        if(acount)
            sprint(other, ", ");
        acount = 1;
        s = ftos(self.ammo_multi_rockets);
        sprint(other, s);
        sprint(other, " multi rockets");
    }

    if(self.ammo_plasma)
    {
        if(acount)
            sprint(other, ", ");
        acount = 1;
        s = ftos(self.ammo_plasma);
        sprint(other, s);
        sprint(other, " plasma balls");
    }

    sprint(other, "\n");

    // backpack touch sound
    sound(other, CHAN_ITEM, "weapons/lock4.wav", 1, ATTN_NORM);
    stuffcmd(other, "bf\n");

    // remove the backpack, change self to the player
    remove(self);
    self = other;

    W_SetCurrentAmmo();
};

/*
===============
DropBackpack
===============
*/
void() DropBackpack =
{
    local entity    item;

    if(!(self.ammo_shells + self.ammo_nails + self.ammo_rockets + self.ammo_cells
    + self.ammo_lava_nails + self.ammo_multi_rockets + self.ammo_plasma))
        return;    // nothing in it

    item = spawn();
    item.origin = self.origin - '0 0 24';

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
    setmodel(item, "progs/backpack.mdl");
    setsize(item, '-8 -8 0', '8 8 24');
    item.handtouch = BackpackTouch;
    // TODO VR: (P2) backpack spins because of MF_ROTATE/EF_ROTATE flag in mdl file

    item.nextthink = time + 120;    // remove after 2 minutes
    item.think = SUB_Remove;
};
