//----------------------------------------------------------------------
void() weapon_touch = {
    local float best, new;
    local float leave, moditem, use2, useperms;

    // if the player was using his best weapon
    // change up to the new one if better        
    best = W_BestWeapon(other);

    use2 = FALSE; //bit to set if checking against self.items2
    useperms = FALSE; // diddo for checking against self.perms
    // Deathmatch/coop switch
    if (deathmatch == 2 || coop) leave = TRUE;
    else leave = FALSE;
    
    new = -1;
    //----------------------------------------------------------------------
    if (self.classtype == CT_AXE) {
        new = IT_AXE;
    }
    //----------------------------------------------------------------------
    if (self.classtype == CT_UPGRADE_AXE) {
        if (leave && (other.moditems & IT_UPGRADE_AXE) ) return;
        update_configflag(SVR_UPDAXE, TRUE);
        moditem = TRUE;
        new = IT_UPGRADE_AXE;
    }
    //----------------------------------------------------------------------
    else if (self.classtype == CT_SHOTGUN) {
        if (self.perms & IT_BLASTER) other.ammo_shells = other.ammo_shells + AMMO_SHELLS_WPN;
        if (leave && (other.items & IT_SHOTGUN) ) return;
        new = IT_SHOTGUN;
        if (!(self.perms & IT_BLASTER)) other.ammo_shells = other.ammo_shells + AMMO_SHELLS_WPN;
    }
    //----------------------------------------------------------------------
    else if (self.classtype == CT_BLASTERWEP) {
        if (leave && (other.items & IT_SHOTGUN) ) return;
        new = IT_SHOTGUN;
        other.ammo_shells = other.ammo_shells + AMMO_SHELLS_WPN;
        other.perms = other.perms | IT_BLASTER;
    }
    //----------------------------------------------------------------------
    else if (self.classtype == CT_AXE) {
        if (leave && (other.items & IT_AXE) ) return;
        new = IT_AXE;
    }
    //----------------------------------------------------------------------
    else if (self.classtype == CT_UPGRADE_SSG) {
        if (leave && (other.moditems & IT_UPGRADE_SSG) ) return;
        update_configflag(SVR_UPDSSG, TRUE);
        moditem = TRUE;
        new = IT_UPGRADE_SSG;
        other.ammo_shells = other.ammo_shells + AMMO_SHELLS_WPN;
    }
    //----------------------------------------------------------------------
    else if (self.classtype == CT_SUPER_SHOTGUN) {
        if (leave && (other.items & IT_SUPER_SHOTGUN) ) return;
        new = IT_SUPER_SHOTGUN;
        other.ammo_shells = other.ammo_shells + AMMO_SHELLS_WPN;
    }
    //----------------------------------------------------------------------
    else if (self.classtype == CT_NAILGUN) {
        if (leave && (other.items & IT_NAILGUN) ) return;
        new = IT_NAILGUN;
        other.ammo_nails = other.ammo_nails + AMMO_NAILS_WPN;
    }
    //----------------------------------------------------------------------
    else if (self.classtype == CT_SUPER_NAILGUN) {
        if (leave && (other.items & IT_SUPER_NAILGUN) ) return;
        new = IT_SUPER_NAILGUN;
        other.ammo_nails = other.ammo_nails + AMMO_NAILS_WPN;
    }
    //----------------------------------------------------------------------
    else if (self.classtype == CT_GRENADE_LAUNCHER) {
        if (leave && (other.items & IT_GRENADE_LAUNCHER) ) return;
        new = IT_GRENADE_LAUNCHER;
        other.ammo_rockets = other.ammo_rockets + AMMO_ROCKETS_WPN;
    }
    //----------------------------------------------------------------------
    else if (self.classtype == CT_ROCKET_LAUNCHER) {
        if (leave && (other.items & IT_ROCKET_LAUNCHER) ) return;
        new = IT_ROCKET_LAUNCHER;
        other.ammo_rockets = other.ammo_rockets + AMMO_ROCKETS_WPN;
    }
    //----------------------------------------------------------------------
    else if (self.classtype == CT_MULTI_GRENADE) {
        if (leave && (other.items & IT_MULTI_GRENADE) ) return;
        new = IT_MULTI_GRENADE;
        other.ammo_multi_rockets = other.ammo_multi_rockets + AMMO_MULTI_ROCKETS_WPN;
    }
    //----------------------------------------------------------------------
    else if (self.classtype == CT_MULTI_ROCKET) {
        if (leave && (other.items & IT_MULTI_ROCKET) ) return;
        new = IT_MULTI_ROCKET;
        other.ammo_multi_rockets = other.ammo_multi_rockets + AMMO_MULTI_ROCKETS_WPN;
    }
    //----------------------------------------------------------------------
    else if (self.classtype == CT_CHAINLIGHTGUN) {
        if (leave && (other.items2 & IT2_CHAINLIGHTNING) ) return;
        new = IT2_CHAINLIGHTNING;
        other.ammo_cells = other.ammo_cells + AMMO_CELLS_WPN;
        use2 = TRUE;
    }
    //----------------------------------------------------------------------
    else if (self.classtype == CT_CHAINSAW) {
        if (leave && (other.perms & IT_CHAINSAW) ) return;
        new = IT_AXE;
        other.perms = other.perms | IT_CHAINSAW;
    }
    //----------------------------------------------------------------------
    else if (self.classtype == CT_UPGRADE_LG) {
        if (leave && (other.moditems & IT_UPGRADE_LG) ) return;
        update_configflag(SVR_UPDLG, TRUE);
        moditem = TRUE;
        new = IT_UPGRADE_LG;
        other.ammo_cells = other.ammo_cells + AMMO_CELLS_WPN;
    }
    //----------------------------------------------------------------------
    else if (self.classtype == CT_LIGHTNING) {
        if (leave && (other.items & IT_LIGHTNING) ) return;
        new = IT_LIGHTNING;
        other.ammo_cells = other.ammo_cells + AMMO_CELLS_WPN;
    }
    else if (self.classtype == CT_EXTRA_WEAPON) {
        if (leave && (other.items & IT_EXTRA_WEAPON) ) return;
        other.ammo_bolts = other.ammo_bolts + AMMO_BOLTS_WPN;
        new = IT_EXTRA_WEAPON;
    }
    else if (self.classtype == CT_LAVANG) {
        if (leave && (other.items & IT_LAVA_NAILGUN) ) return;
        new = IT_LAVA_NAILGUN;
        other.ammo_lava_nails = other.ammo_lava_nails + AMMO_LAVA_NAILS_WPN;
    }
    else if (self.classtype == CT_LAVASNG) {
        if (leave && (other.items & IT_LAVA_SUPER_NAILGUN) ) return;
        new = IT_LAVA_SUPER_NAILGUN;
        other.ammo_lava_nails = other.ammo_lava_nails + AMMO_LAVA_NAILS_WPN;
    }
    else if (self.classtype == CT_PLASMAGUN) {
        if (leave && (other.items & IT_PLASMA_GUN) ) return;
        new = IT_PLASMA_GUN;
        other.ammo_plasma = other.ammo_plasma + AMMO_PLASMA_WPN;
    }
    else if (self.classtype == CT_PROXIMITY_GUN) {
        if (leave && (other.items2 & IT2_PROXIMITY_GUN) ) return;
        new = IT2_PROXIMITY_GUN;
        other.ammo_rockets = other.ammo_rockets + AMMO_ROCKETS_WPN;
        use2 = TRUE;
    }
    else if (self.classtype == CT_LASER_CANNON) {
        if (leave && (other.items2 & IT2_LASER_CANNON) ) return;
        new = IT2_LASER_CANNON;
        other.ammo_cells = other.ammo_cells + AMMO_CELLS_WPN;
        use2 = TRUE;
    }
    else if (self.classtype == CT_MJOLNIR) {
        if (leave && (other.items2 & IT2_MJOLNIR) ) return;
        new = IT2_MJOLNIR;
        other.ammo_cells = other.ammo_cells + AMMO_CELLS_WPN;
        use2 = TRUE;
    }
    else if (self.classtype == CT_GRAPPLE) {
        if (leave && (other.items2 & IT2_GRAPPLE) ) return;
        new = IT2_GRAPPLE;
        use2 = TRUE;
    }
    else if (self.classtype == CT_WAND) {
        if (leave && (other.items2 & IT2_CROSS) ) return;
        new = IT2_CROSS;
        use2 = TRUE;
    }
    //----------------------------------------------------------------------
    else return;

    if (self.classtype == CT_UPGRADE_AXE && other.moditems & IT_UPGRADE_AXE)
        sprint (other, "You got another axe to grind\n");
    else if (self.classtype == CT_SUPER_SHOTGUN && other.moditems & IT_UPGRADE_SSG)
        sprint (other, "You got a handful of Shells\n");
    else if (self.classtype == CT_UPGRADE_SSG && other.moditems & IT_UPGRADE_SSG)
        sprint (other, "You got triple barrel boomstick\n");
    else if (self.classtype == CT_LIGHTNING && other.moditems & IT_UPGRADE_LG)
        sprint (other, "You got some AAA batteries\n");
    else if (self.classtype == CT_UPGRADE_LG && other.moditems & IT_UPGRADE_LG)
        sprint (other, "You got another burst of plasma\n");
    else {
        sprint (other, "You got the ");
        sprint (other, self.netname);
        sprint (other, "\n");
    }
    // weapon touch sound
    if (self.classtype == CT_CHAINSAW) {
        sound (other, CHAN_ITEM, "weapons/sawidle.wav", 1, ATTN_NORM);
    } else {
        sound (other, CHAN_ITEM, "weapons/pkup.wav", 1, ATTN_NORM);
    }
    stuffcmd (other, "bf\n");

    // Make sure no ammo is over limits
    bound_other_ammo (other);
    
    // Stop the player switching to weapons already got
    if (!deathmatch) {
        // Check for new shadow axe and SG and skip weapon switch
        if (moditem && new == IT_UPGRADE_AXE && other.moditems & new) new = -1;
        else if (new == IT_SHOTGUN && other.items & new) new = -1;
        // Stop switching to a lower tier weapon (DM Rules)
        // if (DM_RankForWeapon(other.weapon) < DM_RankForWeapon(new)) new = -1;
    }
    
    // Skip any weapon switch if no new weapon
    if (new != -1 || moditem) {
        // change to the weapon and add to inventory
        if (!moditem) {
            if (use2) {
                other.items2 = other.items2 | new;
            } else {
                if (useperms) {
                    other.perms = other.perms | new;
                } else {
                    other.items = other.items | new;
                }
            }
        }
        if (moditem) {
            other.moditems = other.moditems | new;
            if (moditem == IT_UPGRADE_LG) other.items = other.items | IT_LIGHTNING;  // give lightning just in case
            if (moditem == IT_UPGRADE_AXE || self.perms & IT_CHAINSAW) other.items = other.items | IT_AXE; // give axe just in case
            if (moditem == IT_UPGRADE_SSG) other.items = other.items | IT_SUPER_SHOTGUN; // give super shot gun just in case
        }
        
        // Check for best weapon (DM only) and switch to it!
        if (deathmatch) {
            DM_Weapon(new);
        } else {
            if (moditem == IT_UPGRADE_AXE  || self.perms & IT_CHAINSAW) {
                other.weapon = IT_AXE;
            } else {
                if (moditem == IT_UPGRADE_SSG) {
                    other.weapon = IT_SUPER_SHOTGUN;
                } else {
                    if (moditem == IT_UPGRADE_LG) {
                        other.weapon = IT_LIGHTNING;
                    } else {
                        if (new == IT2_CROSS) {
                            other.weapon = IT2_CROSS;
                        } else {
                            other.weapon = new;
                        }
                    }
                }
            }
        }
        //W_SetCurrentAmmo(other);
    }
    W_SetCurrentAmmo(other);

    // If co-op active, exit
    if (leave == TRUE) {
        // Make sure any targets on the weapon are fired for co-op
        // This is basically a run once check using state as a flag
        // Ideally no weapon should use important target triggers!
        // Code idea from Bengt Jardrup
        if (self.state == FALSE) {
            self.state = TRUE;
            activator = other;
            SUB_UseTargets ();
        }
        return;
    }

    // hide item and check for respawn
    item_finished();
    check_item_respawn();
    
    activator = other;
    SUB_UseTargets();
};

void() weapon_grapple = {
    self.mdl = "progs/drake/g_grpple.mdl";
    precache_model (self.mdl);
    self.weapon = IT2_GRAPPLE;
    self.classtype = CT_GRAPPLE;
    self.netname = "Grapple";
    weapon_start();
};

void() weapon_chainsaw = {
    self.mdl = "progs/drake/g_saw.mdl";
    precache_model (self.mdl);
    self.classtype = CT_CHAINSAW;
    self.netname = "Chainsaw";
    weapon_start();
};

void() weapon_axe = {
    self.mdl = g_axemodel; // set by world.qc, mapper can pick to use AD or original axe
    precache_model (self.mdl);
    self.weapon = IT_AXE;
    self.classtype = CT_AXE;
    self.netname = "Axe";
    weapon_start();
};

void() weapon_mjolnir = {
    self.mdl = MODEL_GWEAP_MJOL;
    precache_model (self.mdl);
    self.weapon = IT2_MJOLNIR;
    self.classtype = CT_MJOLNIR;
    self.netname = "Mjolnir's Hammer";
    if (world.devdata & DEV_AMMO_N_HEALTH) world.deventity.ammo_cells = world.deventity.ammo_cells + AMMO_CELLS_WPN;
    weapon_start();
};

void() weapon_wand = {
    self.mdl = MODEL_GWEAP_WAND;
    precache_model (self.mdl);
    self.weapon = IT2_CROSS;
    self.classtype = CT_WAND;
    self.netname = "Magic Wand";
    weapon_start();
};

void () weapon_crossbow = {
    self.mdl = "progs/g_crossbow.mdl";
    precache_model(self.mdl);
    self.weapon = IT_EXTRA_WEAPON;
    self.classtype = CT_EXTRA_WEAPON; 
    self.netname = "Crossbow";
    if (world.devdata & DEV_AMMO_N_HEALTH) world.deventity.ammo_bolts = world.deventity.ammo_bolts + AMMO_BOLTS_WPN;
    weapon_start();
};

void() weapon_lava_nailgun = {
    self.mdl = MODEL_GWEAP_LNG;
    precache_model (self.mdl);
    self.weapon = IT_LAVA_NAILGUN;
    self.classtype = CT_LAVANG;
    self.netname = "Lava Nailgun";
    if (world.devdata & DEV_AMMO_N_HEALTH) world.deventity.ammo_lava_nails = world.deventity.ammo_lava_nails + AMMO_LAVA_NAILS_WPN;
    weapon_start();
};

void() weapon_lava_super_nailgun = {
    self.mdl = MODEL_GWEAP_LSNG;
    precache_model (self.mdl);
    self.weapon = IT_LAVA_SUPER_NAILGUN;
    self.classtype = CT_LAVASNG;
    self.netname = "Lava Super Nailgun";
    if (world.devdata & DEV_AMMO_N_HEALTH) world.deventity.ammo_lava_nails = world.deventity.ammo_lava_nails + AMMO_LAVA_NAILS_WPN;
    weapon_start();
};

void() weapon_proximity_gun = {
    self.mdl = MODEL_GWEAP_PROX;
    precache_model (self.mdl);
    self.weapon = IT2_PROXIMITY_GUN;
    self.classtype = CT_PROXIMITY_GUN;
    self.netname = "Proximity Grenade Launcher";
    if (world.devdata & DEV_AMMO_N_HEALTH) world.deventity.ammo_rockets = world.deventity.ammo_rockets + AMMO_ROCKETS_WPN;
    weapon_start();
};

void () weapon_laser_gun = {
    self.mdl = MODEL_GWEAP_LASER;
    precache_model (self.mdl);
    self.weapon = IT2_LASER_CANNON;
    self.classtype = CT_LASER_CANNON;
    self.netname = "Laser Cannon";
    if (world.devdata & DEV_AMMO_N_HEALTH) world.deventity.ammo_cells = world.deventity.ammo_cells + AMMO_CELLS_WPN;
    weapon_start();
};

void () weapon_multi_grenade = {
    self.mdl = "progs/g_rock.mdl";
    precache_model (self.mdl);
    self.skin = 2;
    self.weapon = IT_MULTI_GRENADE;
    self.classtype = CT_MULTI_GRENADE;
    self.netname = "Multi-Grenade Launcher";
    if (world.devdata & DEV_AMMO_N_HEALTH) world.deventity.ammo_multi_rockets = world.deventity.ammo_multi_rockets + AMMO_MULTI_ROCKETS_WPN;
    weapon_start();
};

void () weapon_multi_rocket = {
    self.mdl = "progs/g_rock2.mdl";
    self.skin = 1;
    precache_model (self.mdl);
    self.weapon = IT_MULTI_ROCKET;
    self.classtype = CT_MULTI_ROCKET;
    self.netname = "Multi-Rocket Launcher";
    if (world.devdata & DEV_AMMO_N_HEALTH) world.deventity.ammo_multi_rockets = world.deventity.ammo_multi_rockets + AMMO_MULTI_ROCKETS_WPN;
    weapon_start();
};

void () weapon_plasmalightning = {
    self.mdl = "progs/g_light.mdl";
    self.skin = 1;
    precache_model (self.mdl);
    self.weapon = IT_PLASMA_GUN;
    self.classtype = CT_PLASMAGUN;
    self.netname = "Plasma Thunderbolt";
    if (world.devdata & DEV_AMMO_N_HEALTH) world.deventity.ammo_plasma = world.deventity.ammo_plasma + AMMO_PLASMA_WPN;
    weapon_start();
};

void () weapon_chain_lightning = {
    self.mdl = "progs/drake/g_light2.mdl";
    precache_model (self.mdl);
    self.weapon = IT2_CHAINLIGHTNING;
    self.classtype = CT_CHAINLIGHTGUN;
    self.netname = "Chain Lightning Gun";
    if (world.devdata & DEV_AMMO_N_HEALTH) world.deventity.ammo_cells = world.deventity.ammo_cells + AMMO_CELLS_WPN;
    weapon_start();
};

/*======================================================================
 AMMO
======================================================================*/
void() ammo_touch = {
    //local float best;

    // if the player was using his best weapon, 
    // change up to the new one if better        
    //best = W_BestWeapon(other);

    //----------------------------------------------------------------------
    if (self.classtype == CT_AMMOSHELLS) {
        if (other.ammo_shells >= AMMO_MAXSHELLS) return;
        other.ammo_shells = other.ammo_shells + self.aflag;
    }
    //----------------------------------------------------------------------
    else if (self.classtype == CT_AMMONAILS) {
        if (other.ammo_nails >= AMMO_MAXNAILS) return;
        other.ammo_nails = other.ammo_nails + self.aflag;
    }
    //----------------------------------------------------------------------
    else if (self.classtype == CT_AMMOROCKETS) {
        if (other.ammo_rockets >= AMMO_MAXROCKETS) return;
        other.ammo_rockets = other.ammo_rockets + self.aflag;
    }
    //----------------------------------------------------------------------
    else if (self.classtype == CT_AMMOCELLS) {
        if (other.ammo_cells >= AMMO_MAXCELLS) return;
        other.ammo_cells = other.ammo_cells + self.aflag;
    }
    //----------------------------------------------------------------------
    else if (self.classtype == CT_AMMOPLASMA) {
        if (other.ammo_plasma >= AMMO_MAXPLASMA) return;
        other.ammo_plasma = other.ammo_plasma + self.aflag;
    }
    //----------------------------------------------------------------------
    else if (self.classtype == CT_AMMOMULTIROCKETS) {
        if (other.ammo_multi_rockets >= AMMO_MAXMULTIROCKETS) return;
        other.ammo_multi_rockets = other.ammo_multi_rockets + self.aflag;
    }
    //----------------------------------------------------------------------
    else if (self.classtype == CT_AMMOLAVANAILS) {
        if (other.ammo_lava_nails >= AMMO_MAXLAVANAILS) return;
        other.ammo_lava_nails = other.ammo_lava_nails + self.aflag;
    }
    else if (self.classtype == CT_AMMOPOISON) {
        if (other.ammo_poison >= AMMO_MAXPOISON) return;
        other.ammo_poison = other.ammo_poison + self.aflag;
    }
    else if (self.classtype == CT_AMMOBOLTS) {
        if (other.ammo_bolts >= AMMO_MAXBOLTS) return;
        other.ammo_bolts = other.ammo_bolts + self.aflag;
    }

    // Check/cap ammo limits
    bound_other_ammo (other);
    
    sprint (other, "You got the ");
    sprint (other, self.netname);
    sprint (other, "\n");
    sound (other, CHAN_ITEM, "weapons/lock4.wav", 1, ATTN_NORM);
    stuffcmd (other, "bf\n");

    // Commented out.  Don't auto switch weapons, people hate it. -Qmaster
    // change to a better weapon if appropriate
    //if ( other.weapon == best ) {
    //    other.weapon = W_BestWeapon(other);
    //    W_SetCurrentAmmo (other);
    //}

    // if changed current ammo, update it
    W_SetCurrentAmmo(other);

    // hide item and check for respawn
    item_finished();
    check_item_respawn();
    
    activator = other;
    SUB_UseTargets();
};

void() item_lava_spikes ={
    if (self.spawnflags & A_LARGE) {
        self.mdl = "maps/b_lnail1.bsp";
        if (self.aflag < 1) self.aflag = AMMO_NAILS_LGR;
    }
    else {
        self.mdl = "maps/b_lnail0.bsp";
        if (self.aflag < 1) self.aflag = AMMO_NAILS_SML;
    }
    self.ammo_lava_nails = self.aflag;
    
    precache_model (self.mdl);
    self.classtype = CT_AMMOLAVANAILS;
    self.netname = "box of lava nails";
    self.respawn_style = PARTICLE_BURST_WHITE + PARTICLE_BURST_RING;
    self.respawn_ofs = '0 0 24';
    self.respawn_effect = TRUE;
    item_ammo_setup();
};

void() item_multi_rockets = {
    if (self.spawnflags & A_LARGE) {
        self.mdl = "maps/b_mrock1.bsp";
        if (self.aflag < 1) self.aflag = AMMO_ROCKETS_LGR;
    }
    else {
        self.mdl = "maps/b_mrock0.bsp";
        if (self.aflag < 1) self.aflag = AMMO_ROCKETS_SML;
    }
    self.ammo_multi_rockets = self.aflag;
    
    precache_model (self.mdl);
    self.classtype = CT_AMMOMULTIROCKETS;
    self.netname = "box of multi rockets";
    if (self.spawnflags & A_LID) self.spawnflags = self.spawnflags - A_LID;
    self.frame_override = TRUE;
    self.respawn_style = PARTICLE_BURST_WHITE + PARTICLE_BURST_RING;
    self.respawn_ofs = '0 0 24';
    self.respawn_effect = TRUE;
    item_ammo_setup();
};

/*======================================================================
/*QUAKED item_plasma (0 .5 .8) (0 0 0) (32 32 32) BIG

/*QUAKED item_plasma (0 .5 .8) (-16 -16 0) (16 16 32) BIG x x x RESPAWN FLOAT STARTOFF NOEFFECTS NOT_EASY NOT_NORMAL NOT_HARD NOT_DM
{ model(":progs/ad171/ammo_cells0.mdl"); }
6 or 12 Cells
-------- KEYS --------
targetname    : toggle state (use trigger ent for exact state)
frame_box     : special animation frame number for box of ammo (1-7)
skin_override : Override world type 1=Base Green, 2=Medieval Wood
aflag         : Ammo quantity override
upgrade_ssg  : = 1 will only spawn if shotgun upgrade active on server
upgrade_axe  : = 1 will only spawn if axe upgrade active on server
upgrade_lg   : = 1 will only spawn if lightning gun upgrade active on server
message       : centerprint message when item is picked up
respawn_time  : time to wait before respawning (=-1 instant)
respawn_count : Total amount of times to respawn (counts down to zero)
respawn_trig : = 1 Wait for trigger before respawning
-------- SPAWNFLAGS --------
BIG      : Double Ammo and different model
RESPAWN  : Can respawn after being picked up
FLOAT    : No drop to floor test
STARTOFF : Starts off and waits for trigger
NOEFFECTS : No particle or effects active
-------- NOTES --------
6 or 12 Cells for the Thunderbolt (LG)

======================================================================*/
void() item_plasma = {
    if (self.spawnflags & A_LARGE) {
        self.mdl = "maps/b_plas1.bsp";
        if (self.aflag < 1) self.aflag = AMMO_CELLS_LGR;
    }
    else {
        self.mdl = "maps/b_plas0.bsp";
        if (self.aflag < 1) self.aflag = AMMO_CELLS_SML;
    }
    self.ammo_plasma = self.aflag;

    precache_model (self.mdl);
    self.classtype = CT_AMMOPLASMA;
    self.netname = "box of cells";
    if (self.spawnflags & A_LID) self.spawnflags = self.spawnflags - A_LID;
    self.frame_override = TRUE;
    self.respawn_style = PARTICLE_BURST_BLUE + PARTICLE_BURST_RING;
    self.respawn_ofs = '0 0 24';
    self.respawn_effect = TRUE;
    item_ammo_setup();
};

void() item_bolts = {
    if (self.spawnflags & A_LARGE) {
        self.mdl = "progs/ammo_bolts1.mdl";
        if (self.aflag < 1) self.aflag = AMMO_BOLTS_LGR;
    } else {
        self.mdl = "progs/ammo_bolts0.mdl";
        if (self.aflag < 1) self.aflag = AMMO_BOLTS_SML;
    }
    self.ammo_bolts = self.aflag;

    precache_model (self.mdl);
    self.classtype = CT_AMMOBOLTS;
    self.netname = "box of crossbow bolts";
    self.skin = 0;
    self.skin_override = 0;
    self.respawn_style = PARTICLE_BURST_WHITE + PARTICLE_BURST_RING;
    self.respawn_ofs = '0 0 24';
    self.respawn_effect = TRUE;
    item_ammo_setup();
};

void() item_poison = {
    if (self.spawnflags & A_LARGE) {
        self.mdl = "progs/ammo_poison1.mdl";
        if (self.aflag < 1) self.aflag = AMMO_POISON_LGR;
    } else {
        self.mdl = "progs/ammo_poison0.mdl";
        if (self.aflag < 1) self.aflag = AMMO_POISON_SML;
    }
    self.ammo_poison = self.aflag;

    precache_model (self.mdl);
    self.classtype = CT_AMMOPOISON;
    self.netname = "box of poison";
    self.skin = 0;
    self.skin_override = 0;
    self.respawn_style = PARTICLE_BURST_WHITE + PARTICLE_BURST_RING;
    self.respawn_ofs = '0 0 24';
    self.respawn_effect = TRUE;
    item_ammo_setup();
};

/*======================================================================
 ARTIFACTS (suit, pent, invis, quad)
======================================================================*/
void() artifact_touch = {
    entity pl;
    //----------------------------------------------------------------------
    // Existing artifacts
    //----------------------------------------------------------------------
    if (self.classtype == CT_ARTSUIT) {
        other.rad_time = TRUE;
        other.radsuit_finished = time + self.cnt;
        ResetDebuffBurning(other);
        ResetDebuffPoisoned(other);
    }
    else if (self.classtype == CT_ARTPENT) {
        other.invincible_time = TRUE;
        other.invincible_finished = time + self.cnt;
        ResetDebuffSystem(other);
    }
    else if (self.classtype == CT_ARTINVS) {
        other.invisible_time = TRUE;
        other.invisible_finished = time + self.cnt;
    }
    else if (self.classtype == CT_ARTQUAD) {
        other.super_time = TRUE;
        other.super_damage_finished = time + self.cnt;
    }
    else if (self.classtype == CT_ARTSHARP) {
        other.sharpshoot_time = TRUE;
        other.sharpshoot_finished = time + self.cnt;
    }
    else if (self.classtype == CT_ARTPIERCE) {
        other.nailpiercer_time = TRUE;
        other.nailpiercer_finished = time + self.cnt;
    }
    else if (self.classtype == CT_ARTPSHIELD) {
        other.shield_time = TRUE;
        other.shield_finished = time + self.cnt;
    }
    else if (self.classtype == CT_ARTGRAVBELT) {
        other.antigrav_time = TRUE;
        other.antigrav_finished = time + self.cnt;
        other.gravity = 0.25; // This is where the magic happens! See client_power.qc for how it goes back to 1.
    }
    else if (self.classtype == CT_ARTWETSUIT) {
        other.wetsuit_time = TRUE;
        other.wetsuit_finished = time + self.cnt;
    }
    else if (self.classtype == CT_ARTEMPATHY) {
        other.empathy_time = TRUE;
        other.empathy_finished = time + self.cnt;
    }
    else if (self.classtype == CT_ARTTHERMSUIT) {
        other.therm_time = TRUE;
        other.therm_finished = time + self.cnt;
        ResetDebuffBurning(other);
    }
    else if (self.classtype == CT_ARTTRINITY) {
        other.trinity_time = 1;
        other.trinity_start = 1;
        other.trinity_finished = time + self.cnt;
    }
    else if (self.classtype == CT_ARTCROSS) {
        other.cross_time = 1;
        other.cross_start = 1;
        other.cross_finished = time + self.cnt;
    }
    else if (self.classtype == CT_ARTBERSERK) {
        other.perms = other.perms | POW_VAMPIRE;
        if (other.health < 100) other.health = 100; // full heal
    }
    else if (self.classtype == CT_ARTEQUALIZER) {
        other.perms = other.perms | POW_DOUBLE;
        other.ammo_nails = AMMO_MAXNAILS; // max out nails ammo
        if (other.ammo_lava_nails < (AMMO_MAXLAVANAILS * 0.5)) other.ammo_lava_nails = AMMO_MAXLAVANAILS * 0.5; // fill lava nails up to half

        // if changed current ammo, update it
        W_SetCurrentAmmo(other);
    }
    else if (self.classtype == CT_ARTLEVITATION) {
        other.wing_time = 1;
        other.wing_finished = time + 45;
    }
    else if (self.classtype == CT_ARTOPTIMIZER) {
        other.perms = other.perms | POW_SHOTTYMAN;
        other.ammo_shells = AMMO_MAXSHELLS; // max out nails ammo
    }
    else if (self.classtype == CT_SIGILBANE) {
        // Give all players 250 health
        pl = find (world, classname, "player");
        while (pl) {
            pl.health = HEAL_MEGAMAX;
            pl = find (pl, classname, "player");
        }
    }
    else return;

    //----------------------------------------------------------------------
    // Pickup sound and initial screen flash
    //----------------------------------------------------------------------
    sprint (other, "You got the ");
    sprint (other, self.netname);
    sprint (other,"\n");
    sound (other, CHAN_VOICE, self.noise, 1, ATTN_NORM);
    stuffcmd (other, "bf\n");

    // Update player item flags (regular + mod)
    other.items = other.items | self.items;
    other.items2 = other.items2 | self.items2;
    other.moditems = other.moditems | self.moditems;

    // hide item and check for respawn
    item_finished();
    check_item_respawn();
    
    activator = other;
    SUB_UseTargets();
};

void () item_powerup_shield = {
    self.mdl = "progs/rogue/shield.mdl";
    precache_model (self.mdl);
    self.noise = "shield/pickup.wav";
    self.noise1 = "shield/hit.wav";
    self.noise2 = "shield/fadeout.wav";
    precache_sound (self.noise);
    precache_sound (self.noise1);
    precache_sound (self.noise2);
    precache_model ("progs/rogue/shield.mdl");
    self.netname = "Power Shield";
    self.items = IT_QUAD;
    self.items2 = IT2_SHIELD;
    self.classtype = CT_ARTPSHIELD;
    self.part_active = PARTICLE_STYLE_PENT;
    self.respawn_style = PARTICLE_BURST_RED + PARTICLE_BURST_CENTER;
    self.respawn_ofs = '0 0 16';
    artifact_setup();
};

void () item_powerup_belt = {
    self.mdl = "progs/rogue/beltup.mdl";
    precache_model (self.mdl);
    self.noise = "belt/pickup.wav";
    self.noise1 = "belt/use.wav";
    self.noise2 = "belt/fadeout.wav";
    precache_sound (self.noise);
    precache_sound (self.noise1);
    precache_sound (self.noise2);
    self.netname = "Anti-Grav Belt";
    self.items = IT_QUAD;
    self.classtype = CT_ARTGRAVBELT;
    self.part_active = PARTICLE_STYLE_QUAD;
    self.respawn_style = PARTICLE_BURST_BLUE + PARTICLE_BURST_CENTER;
    self.respawn_ofs = '0 0 16';
    self.cnt = 45;
    artifact_setup();
};

void () item_artifact_empathy_shields = {
    self.mdl = "progs/hipnotic/empathy.mdl";
    precache_model (self.mdl);
    self.noise = "hipitems/empathy.wav";
    self.noise1 = "hipitems/empathy2.wav";
    self.noise2 = "items/suit2.wav";
    precache_sound (self.noise);
    precache_sound (self.noise1);
    precache_sound (self.noise2);
    self.netname = "Empathy Shield";
    self.items2 = HIP_IT2_EMPATHY_SHIELDS;
    self.classtype = CT_ARTEMPATHY;
    self.part_active = PARTICLE_STYLE_SRING;
    self.respawn_style = PARTICLE_BURST_YELLOW + PARTICLE_BURST_CENTER;
    self.respawn_ofs = '0 0 6';
    artifact_setup();
};

void () item_artifact_lavasuit = {
    self.mdl = "progs/suit2.mdl";
    precache_model (self.mdl);
    self.noise = "items/suit.wav";
    self.noise1 = "items/suit2.wav";
    precache_sound (self.noise);
    precache_sound (self.noise1);
    self.netname = "Thermal Protection Suit";
    self.skin = 1;
    self.classtype = CT_ARTTHERMSUIT;
    self.part_active = PARTICLE_STYLE_PENT;
    self.respawn_style = PARTICLE_BURST_RED + PARTICLE_BURST_CENTER;
    self.respawn_ofs = '0 0 6';
    artifact_setup();
};

void() item_artifact_trinity = {
    self.mdl = "progs/quoth2pt2full/trinity.mdl";
    precache_model (self.mdl);
    self.noise = "items2/trintake.wav";
    self.noise1 = "items2/trinuse.wav";
    self.noise2 = "items2/trinend.wav";
    precache_sound (self.noise);
    precache_sound (self.noise1);
    precache_sound (self.noise2);
    self.netname = "Trinity";
    self.items = IT_QUAD;
    self.items2 = IT2_TRINITY;
    self.classtype = CT_ARTTRINITY;
    self.part_active = PARTICLE_STYLE_SRING;
    self.respawn_style = PARTICLE_BURST_YELLOW + PARTICLE_BURST_CENTER;
    self.respawn_ofs = '0 0 6';
    artifact_setup();
};

void() item_artifact_cross = {
    self.mdl = "progs/quoth2pt2full/cross.mdl";
    precache_model (self.mdl);
    self.noise = "items2/crostake.wav";
    self.noise2 = "items2/crosend.wav";
    precache_sound (self.noise);
    precache_sound (self.noise2);
    self.netname = "Cross of Deflection";
    self.items = IT_QUAD;
    self.classtype = CT_ARTCROSS;
    self.part_active = PARTICLE_STYLE_QUAD;
    self.respawn_style = PARTICLE_BURST_BLUE + PARTICLE_BURST_CENTER;
    self.respawn_ofs = '0 0 16';
    artifact_setup();
};

void() item_artifact_random = {
    local float rnd;

    rnd = random() * 100;
    if (rnd < 5)
        item_artifact_envirosuit();
    else if (rnd < 10)
        item_artifact_invisibility();
    else if (rnd < 15)
        item_artifact_invulnerability();
    else if (rnd < 20)
        item_artifact_cross();
    else if (rnd < 25)
        item_artifact_empathy_shields();
    else if (rnd < 30)
        item_artifact_nail_piercer();
    else if (rnd < 35)
        item_artifact_sharp_shooter();
    else if (rnd < 40)
        item_artifact_super_damage();
    else if (rnd < 45)
        item_artifact_trinity();
    else if (rnd < 50)
        item_artifact_wetsuit();
    else
        item_backpack();
};

void () horn_touch = {
    if ( (other.classname != "player") ) return ;

    self.solid = SOLID_NOT;
    self.model = string_null;
    sprint (other,"You got the Horn of Conjuring\n");
    sound (other,CHAN_VOICE,self.noise,TRUE,ATTN_NONE);
    stuffcmd (other,"bf\n");
    activator = other;
    other.horn_active = TRUE;
    SUB_UseTargets ();
    other.horn_active = FALSE;
};

void () item_artifact_horn = {
    if (deathmatch) { remove(self); return; }

    self.touch2 = horn_touch;
    self.mdl = "progs/hipnotic/horn.mdl";
    precache_model ("progs/hipnotic/horn.mdl");
    precache_sound ("hipitems/horn.wav");
    setmodel (self,self.mdl);
    self.noise = "hipitems/horn.wav";
    self.classtype = CT_ARTHORN;
    setsize (self,VEC_HULL_MIN,VEC_HULL_MAX);
    item_start ();
};

// Can't touch dis!
void () item_taunt = {
   if ( (self.model) ) {
      precache_model (self.model);
      setmodel(self,self.model);
      self.solid = SOLID_NOT;
      self.movetype = MOVETYPE_NONE;
   } else {
      objerror("No model for item_taunt");
   }
};

// HACK alert
// meh, including this here to be nice, from Drake.  This is an item_taunt and it should have a push trigger and a killtarget to remove this and the trigger after jumping
void () item_pushup = {
    self.model = "progs/drake/wings2.mdl";
    item_taunt();
}


/*
===============================================================================
item_circuitboard from Rubicon 2 by ijed, MFX, & Hrimfaxi
===============================================================================
*/

void () circuitboard_touch = {
    if (other.classname != "player")
        return;

    if (other.health <= 0)
        return;

    stuffcmd (other, "bf\n");
    centerprint (other, "You got the 17-centimeter Fromitz board!");
    sound (other, CHAN_ITEM, self.noise, 1, ATTN_NORM);
    activator = other;
    SUB_UseTargets ();
    remove (self);
};

/*QUAKED item_circuitboard (0 .5 .8) (-16 -16 -24) (16 16 32)
collectible circuit board thing
*/

void () item_circuitboard = {
    self.mdl = "progs/fromitz.mdl";
    precache_model ("progs/fromitz.mdl");
    setmodel (self, "progs/fromitz.mdl");
    precache_sound ("items/protect.wav");
    self.noise = "items/protect.wav";
    self.touch2 = circuitboard_touch;
    setsize (self, '-16 -16 -24', VEC_HULL_MAX);
    self.classtype = CT_CIRCUIT;
    item_start();
};