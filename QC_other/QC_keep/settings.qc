/*======================================================================
 MOD CUSTOM SETTINGS
======================================================================*/
void() toggleautoaim;        // Toggle auto aim
void() togglezaware;        // Toggle Z aware monsters
void() togglefootstep;        // Toggle footstep sound (player/monster)
void() toggleliquiddmg;        // Toggle Monster Liquid damage
void() toggleshotgunproj;    // Toggle Shotgun Projectiles
void() toggleshotguncase;    // Toggle Shotgun Casing
void() toggleupgradeaxe;    // Toggle Axe Upgrade (inventory only)
void() toggleupgradessg;    // Toggle Shotgun Upgrade (inventory only)
void() toggleupgradelg;        // Toggle Lightning Upgrade (inventory only)
void() togglepixels;        // Toggle Texture modes
void() togglecoopartifacts;    // Toggle COOP can drop artifacts
void() togglecoopammoboxes;    // Toggle COOP ammo boxes to respawn

void() display_configflag;    // Display config flag
void() display_playinfo;    // Display Player information
void() display_version;        // Display MOD information
void() display_mapvar;        // Display Map Variables
void() display_customkeys;    // Display Custom Key inventory
void() toggleaipath;        // Toggle AI path extra info
void() CycleParticleDebug;    // Cycle particle debug messages

void(float sigil_bit) givesigil;    // Testing only
void(float key_bit) givekey;        // Testing only

void() CycleWeaponCommand;
void() CycleWeaponReverseCommand;
void() CheatCommand;
void() SharpshooterCheat;
void() NailPiercerCheat;
void() EnvSuitCheat;
void() PentagramCheat;
void() InvisibleCheat;
void() QuadDamageCheat;
void() StartIntermissionCamera;

/*======================================================================
 Show MOD name and version
======================================================================*/
void() display_version =
{
    // Default return conditions (must be player)
    if ( !(self.flags & FL_CLIENT) ) return;
    
    self.impulse = 0;
    sprint(self,MOD_VERSION);
};


/*======================================================================
 Prints which custom keys the player has in their inventory
======================================================================*/
void() display_customkeys =
{
    local string ckey_str1, ckey_str2, ckey_str3, ckey_str4;
    local float ckey_count;
    
    // Default return conditions (must be player)
    if ( !(self.flags & FL_CLIENT) ) return;
    self.impulse = 0;
    
    // Check for any custom keys in player inventory
    if (self.moditems & IT_CKEYALL) {
        // Always reset key count
        ckey_count = 0;
        
        if (self.moditems & IT_CKEY1) {
            ckey_str1 = self.ckeyname1;
            ckey_count = ckey_count + 1;
        }
        if (self.moditems & IT_CKEY2) {
            if (ckey_count == 1) ckey_str2 = self.ckeyname2;
            else ckey_str1 = self.ckeyname2;
            ckey_count = ckey_count + 1;
        }
        if (self.moditems & IT_CKEY3) {
            if (ckey_count == 2) ckey_str3 = self.ckeyname3;
            else if (ckey_count == 1) ckey_str2 = self.ckeyname3;
            else ckey_str1 = self.ckeyname3;
            ckey_count = ckey_count + 1;
        }
        if (self.moditems & IT_CKEY4) {
            if (ckey_count == 3) ckey_str4 = self.ckeyname4;
            else if (ckey_count == 2) ckey_str3 = self.ckeyname4;
            else if (ckey_count == 1) ckey_str2 = self.ckeyname4;
            else ckey_str1 = self.ckeyname4;
            ckey_count = ckey_count + 1;
        }
        
        // if 4 keys are present, then squash the last two key names together
        // The maximum amount of strings that centerprint can handle is 7
        // A bit of shame, because only needed one more string!
        if (ckey_count == 4)
            centerprint7 (self, "-[ \bArcane Key Inventory\b ]-\nYou have the following\n\n", ckey_str1, "\n", ckey_str2, "\n", ckey_str3, ckey_str4);
        else
            // Space out title and keys (does not print key 4)
            centerprint7 (self, "-[ \bArcane Key Inventory\b ]-\nYou have the following\n\n", ckey_str1, "\n", ckey_str2, "\n", ckey_str3, "\n");
    }
    else 
        // Should always say the state of the inventory
        centerprint(self, "-[ \bArcane Key Inventory\b ]-\n\nIs Empty!\n"); 
};

/*======================================================================
 The MOD can be customizied from three different source:
 quake.rc < worldspawn < impulse commands (top priority)
 
 The serverflags is the ONLY variable that is passed between MAP loads
 and that variable is not available straightaway! Once the worldspawn
 is loaded (first thing to happen) the serverflags is still empty
 and the functions have to cope with it 'out of sync' until later.
 
 after worldspawn is loaded, the combination of the quake.rc (TEMP1)
 is merged with the keys on worldspawn and all spawning functions
 use the configflags until it can be sync'd with serverflags.
 
 The configflags is setup to be temporary so that any previous value
 from a quickload does not interfere with it.
 
======================================================================*/
// Assume that serverflags not setup yet, merge worldspawn with configflag
// Once serverflags available, then sync everything together
void(float regbitflag, float worldvalue) update_worldspawn =
{
    // Is the bit value TRUE?
    if (worldvalue > 0) configflag = configflag | regbitflag;
    // Only take about a bit value from configflag if bit value is present
    else if (worldvalue < 0) {
        if (configflag & regbitflag) configflag = configflag - regbitflag;
    }
};

//----------------------------------------------------------------------
// The worldspawn key values always override the quake.rc values and
// should be merged with the console variable when loading a map
// serverflags is not available before spawn functions
void() sycn_worldspawn =
{
    update_worldspawn(SVR_ITEMOFFSET, self.no_item_offset);
    update_worldspawn(SVR_ITEMROTATE, self.no_item_rotate);
    update_worldspawn(SVR_UPDAXE, self.upgrade_axe);
    update_worldspawn(SVR_UPDSSG, self.upgrade_ssg);
    update_worldspawn(SVR_UPDLG, self.upgrade_lg);
    update_worldspawn(SVR_ZAWARE, self.no_zaware);
    update_worldspawn(SVR_LIQDAM, self.no_liquiddmg);
};

//----------------------------------------------------------------------
// Reset the serverflag to console + worldspawn settings
// This only needs to be done when loading a new map or changelevel
// quickload cannot run this function it will damage inventory
void() sync_serverflags =
{
    local float checkflag;
    
    if (serverflags & SVR_LIVE) {
        dprint("\b[SERVERFLAG]\b MAP LOAD reset/sync\n");
        // Take 1-8 runes, 9-16 worldspawn merge, 17-23 mod/console options
        checkflag = (serverflags & SVR_RUNEFLAG) + (temp1flag & SVR_WORLDFLAG) + (configflag & SVR_HIGHFLAG);
        // Update the serverflags ready for future updates
        serverflags =  configflag = checkflag;    
    }
};

//----------------------------------------------------------------------
// Update configflag and if the serverflags is active, update that as well!
void(float regbitflag, float regbitvalue) update_configflag =
{
    // Is the bit value TRUE?
    if (regbitvalue == TRUE) {
        configflag = configflag | regbitflag;
        if (serverflags & SVR_LIVE) serverflags = serverflags | regbitflag;
    }
    // If the bit value FALSE
    else {
        configflag = configflag - (configflag & regbitflag);
        if (serverflags & SVR_LIVE) {
            serverflags = serverflags - (serverflags & regbitflag);
        }
    }
        
    // Only the top 8 bits of TEMP1 flag can be updated
    // Some options should not be changed easily, causes problems
    if (regbitflag & SVR_HIGHFLAG) {
        // Re-fresh the console variable 
        dprint("\b[TEMP1]\b Changing ("); dprint(ftos(regbitflag));
        dprint(")\n");
        temp1flag = cvar("temp1");
        if (regbitvalue > 0) temp1flag = temp1flag | regbitflag;
        else temp1flag = temp1flag - (temp1flag & regbitflag);
        // Write any changes to the console variable (TEMP1)
        cvar_set("temp1", ftos(temp1flag));
    }        
};

//----------------------------------------------------------------------
float(float regbitflag) query_configflag =
{
    // Return bitflag value so multiple bits can be tested together
    return (configflag & regbitflag);
};

//----------------------------------------------------------------------
float(float regbitflag) query_temp1flag =
{
    // Return bitflag value so multiple bits can be tested together
    return (temp1flag & regbitflag);
};

//----------------------------------------------------------------------
// Once global variables are available (client/server is setup)
// Sync the serverflag with the configflag
//----------------------------------------------------------------------
void() sync_configflag =
{
    local float checkflag;
    
    // At this point the live data is all over the place and needs to be merged
    // The serverflag can contain (rune keys and console options) if quickload
    // and the configflag contains default + worldspawn keys
    if (serverflags & SVR_LIVE) {
        dprint("\b[SERVERFLAG]\b LIVE data detected\n");
        // Take 1-16 runes & spawn options, 17-23 mod/console options
        checkflag = (serverflags & SVR_LOWFLAG) + (configflag & SVR_HIGHFLAG);

        // Make sure all flags are in sync with each other
        configflag = serverflags = checkflag | SVR_LIVE;
    }
    else {
        dprint("\b[SERVERFLAG]\b new game detected\n");
        // Update the serverflags ready for future updates
        serverflags =  configflag | SVR_LIVE;    
    }
};

//----------------------------------------------------------------------
void() display_configflag =
{
    if (developer == 0) return;

    dprint("\n\b[CONFIG]\b MOD Settings ("); 
    if (configflag & SVR_LIVE) dprint(ftos(configflag-SVR_LIVE));
    else dprint(ftos(configflag));
    if (configflag & SVR_LIVE) dprint(") L\n");
    else dprint(")\n");
    dprint("--------------------------------------\n");
    // Particle system
    if (query_configflag(SVR_PARTICLES)) dprint("+ Particle System enabled\n");
    else dprint("- Particle System disabled\n");
    if (query_configflag(SVR_SPRPARTON) || world.sprite_particles == TRUE) 
        dprint("* Sprite/Dot particles ONLY\n");
    if (query_configflag(SVR_WEATHER)) dprint("- Weather System disabled\n");
    else dprint("+ Weather System enabled\n");
    // All new MOD weapons
    if (world.upgrade_axe < 0) dprint("W Axe Upg Remove on Worldspawn\n");
    else if (world.upgrade_axe > 0) dprint("W Axe Upg Added on Worldspawn\n");
    else if (query_temp1flag(SVR_UPDAXE)) dprint("* Axe Upgrade in quake.rc\n");
    else if (query_configflag(SVR_UPDAXE)) dprint("+ Axe Upgrade from inventory\n");
    else dprint("- NO Axe upgrade detected\n");

    if (world.upgrade_ssg < 0) dprint("W SSG Upg Remove on Worldspawn\n");
    else if (world.upgrade_ssg > 0) dprint("W SSG Upg Added on Worldspawn\n");
    else if (query_temp1flag(SVR_UPDSSG)) dprint("* SSG Upgrade in quake.rc\n");
    else if (query_configflag(SVR_UPDSSG)) dprint("+ SSG Upgrade from inventory\n");
    else dprint("- NO SSG upgrade detected\n");

    if (world.upgrade_lg < 0) dprint("W LG Upg Remove on Worldspawn\n");
    else if (world.upgrade_lg > 0) dprint("W LG Upg Added on Worldspawn\n");
    else if (query_temp1flag(SVR_UPDLG)) dprint("* LG Upgrade in quake.rc\n");
    else if (query_configflag(SVR_UPDLG)) dprint("+ LG Upgrade from inventory\n");
    else dprint("- NO LG upgrade detected\n");

    // New Shotgun bullet projectile/casing system
    if (world.no_sgprojectile > 0) dprint("W SG Projs Disabled on worldspawn\n");
    else if (query_configflag(SVR_SHOTGPROJ)) dprint("- Shotgun hitscan system\n");
    else dprint("+ Shotgun projectiles system\n");
    if (world.no_sgcasing > 0) dprint("W SG Casing Disabled on worldspawn\n");
    else if (query_configflag(SVR_SHOTGCASE)) dprint("- Shotgun casing disabled\n");
    else dprint("+ Shotgun casing enabled\n");
    if (playerprojsize == 0) dprint("+ Large Projectile Collision\n");
    else dprint("- Small Projectile Collision\n");

    // Items updates; origin points and random rotation
    if (query_configflag(SVR_ITEMOFFSET)) dprint("+ All items use center origin\n");
    else dprint("- All items use corner origin\n");
    if (query_configflag(SVR_ITEMROTATE)) dprint("- All items use entity angle key\n");
    else dprint("+ All items are randomly rotated\n");

    // General MOD settings
    if (autoaim_cvar >= 1) dprint("+ Player weapon autoaim disabled\n");
    else dprint("- Player weapon autoaim enabled\n");
    if (query_configflag(SVR_ZAWARE)) dprint("- Z Aware Monster combat disabled\n");
    else dprint("+ Z Aware Monster combat enabled\n");
    if (query_configflag(SVR_FOOTSTEP)) dprint("- footstep sounds disabled\n");
    else dprint("+ footstep sounds enabled\n");
    if (query_configflag(SVR_LIQDAM)) dprint("- monster liquid damage off\n");
    else dprint("+ monster liquid damage on\n");
    if (query_configflag(SVR_AIPATH)) dprint("- Path Corner Arrows disabled\n");
    else dprint("+ Path Corner Arrows enabled\n");
    
    // Body fade/floor checks
    if (map_bodyfadeaway > 0) dprint("+ Monster dead body fade away\n");
    else dprint("- No fade away for monster bodies\n");
    if (map_bodyflrcheck > 0) dprint("+ Monster dead body check floor\n");
    else dprint("- No floor checks for monster bodies\n");

    // Visual settings - fog / wateralpha / gravity
    if (query_configflag(SVR_NOFOGCMDS)) dprint("- Global fog system disabled\n");
    else if (fog_active && fog_control) {
        dprint("+ Gfog ("); 
        lftos(self, fog_control.fog_currden, 1,3, BUFFER_DPRINT); dprint(")(");
        lftos(self, fog_control.fog_currcol_x, 1,3, BUFFER_DPRINT); dprint(" ");
        lftos(self, fog_control.fog_currcol_y, 1,3, BUFFER_DPRINT); dprint(" ");
        lftos(self, fog_control.fog_currcol_z, 1,3, BUFFER_DPRINT); dprint(")\n");
    }
    else dprint("* Global fog default not defined\n");
    if (liquid_alpha != 1) { 
        dprint("+ Water Alpha has changed ("); 
        lftos(self, liquid_alpha, 0,3, BUFFER_DPRINT); dprint(")\n");
    }
    if (map_gravity != DEF_GRAVITY) { 
        dprint("+ Map gravity has changed ("); 
        dprint(ftos(map_gravity)); dprint(")\n");
    }
    
    // DP compatible visual settings
    if (engine == ENG_DPEXT) {
        dprint("+ DP features ( ");
        // Show all active features
        if (ext_dpfog) dprint("FOG ");
        if (ext_dpsurf) dprint("SURF ");
        if (ext_dprain) dprint("RAIN ");
        if (ext_dpsnow) dprint("SNOW");
        dprint(")\n");
    }

    // Show the coop respawn options
    if (coop > 0) {
        if (coop_weapons == FALSE) dprint("+ COOP weapons respawn TRUE\n");
        else dprint("- COOP weapons respawn FALSE\n");
        if (coop_ammoboxes == FALSE) dprint("- COOP ammo boxes respawn FALSE\n");
        else dprint("+ COOP ammo boxes respawn TRUE\n");
        if (coop_health == FALSE) dprint("- COOP health packs respawn FALSE\n");
        else dprint("+ COOP health packs respawn TRUE\n");
        if (coop_powerups == FALSE) dprint("- COOP powerups respawn FALSE\n");
        else dprint("+ COOP powerups respawn TRUE\n");
    }
    
    dprint("--------------------------------------\n");
};

/*======================================================================
 Prints origin and mangle to console screen
 Useful for creating good locations for intermission camera's
======================================================================*/
void() display_playinfo =
{
    // Default return conditions (must be player)
    if ( !(self.flags & FL_CLIENT) ) return;
    
    self.impulse = 0;
    sprint(self,"\b[INFO]\b Org ("); sprint(self,vtos(self.origin));
    sprint(self,") Ang ("); sprint(self,vtos(self.v_angle));
    sprint(self,")\n");
};

/*======================================================================
 Give the player various sigils (serverflag)
 Give the player silver/gold keys
======================================================================*/
void(float sigil_bit) givesigil =
{
    if (developer == 0) {
        sprint(self,"\b[IMPULSE]\b Only works in developer mode!\n");
        return;
    }

    dprint("\b[SIGIL]\b Server updated ("); 
    dprint(ftos(sigil_bit)); dprint(")\n");
    if (sigil_bit & SVR_RUNE_KEY1) update_configflag(SVR_RUNE_KEY1,TRUE);
    if (sigil_bit & SVR_RUNE_KEY2) update_configflag(SVR_RUNE_KEY2,TRUE);
    if (sigil_bit & SVR_RUNE_KEY3) update_configflag(SVR_RUNE_KEY3,TRUE);
    if (sigil_bit & SVR_RUNE_KEY4) update_configflag(SVR_RUNE_KEY4,TRUE);
};

//----------------------------------------------------------------------
void(float key_bit) givekey =
{
    if (developer == 0) {
        sprint(self,"\b[IMPULSE]\b Only works in developer mode!\n");
        return;
    }

    dprint("\b[KEYS]\b Inventory updated ("); 
    dprint(ftos(key_bit)); dprint(")\n");
    if (key_bit & IT_KEY1) self.items = self.items | IT_KEY1;
    if (key_bit & IT_KEY2) self.items = self.items | IT_KEY2;
};

/*======================================================================
 TOGGLE auto aim feature (default OFF)
======================================================================*/
//----------------------------------------------------------------------
void() toggleautoaim =
{
    // Default return conditions (must be player)
    if ( !(self.flags & FL_CLIENT) ) return;
    
    self.impulse = 0;
    
    // Toggle the state of the console variable
    if (autoaim_cvar >= 1) autoaim_cvar = 0.93;
    else autoaim_cvar = 1;
    cvar_set("sv_aim", ftos(autoaim_cvar));
    
    sprint(self, "\b[Impulse]\b Auto aim state ("); 
    if (autoaim_cvar < 1) sprint(self, "ON)\n");
    else sprint(self, "OFF)\n");
};

//----------------------------------------------------------------------
void() toggleprojsize = 
{
    // Default return conditions (must be player)
    if ( !(self.flags & FL_CLIENT) ) return;
    
    self.impulse = 0;
    
    // Toggle the state of the global variable
    if (playerprojsize == 1) playerprojsize = 0;
    else playerprojsize = 1;
    
    sprint(self, "\b[Impulse]\b Player proj size ("); 
    if (playerprojsize == 1) sprint(self, "SMALL)\n");
    else sprint(self, "LARGE)\n");
};


/*======================================================================
 TOGGLE Z aware monsters (default ON)
======================================================================*/
void() togglezaware =
{    
    // Default return conditions (must be player)
    if ( !(self.flags & FL_CLIENT) ) return;
    
    self.impulse = 0;
    
    // Toggle the state of the console variable using the server flag
    if (query_configflag(SVR_ZAWARE)) update_configflag(SVR_ZAWARE, FALSE);
    else update_configflag(SVR_ZAWARE, TRUE);

    sprint(self, "\b[Impulse]\b Z Aware Monsters ("); 
    if (query_configflag(SVR_ZAWARE)) sprint(self, "Disabled)\n");
    else sprint(self, "Enabled)\n");
};

/*======================================================================
 TOGGLE footsteps (default ON)
======================================================================*/
void() togglefootstep =
{
    // Default return conditions (must be player)
    if ( !(self.flags & FL_CLIENT) ) return;
    
    self.impulse = 0;
    
    // Toggle the state of the console variable using the server flag
    if (query_configflag(SVR_FOOTSTEP)) update_configflag(SVR_FOOTSTEP, FALSE);
    else update_configflag(SVR_FOOTSTEP, TRUE);
    
    sprint(self, "\b[Impulse]\b footsteps ("); 
    if (query_configflag(SVR_FOOTSTEP)) sprint(self, "OFF)\n");
    else sprint(self, "ON)\n");
};

/*======================================================================
 TOGGLE Liquid Damage (default ON)
======================================================================*/
void() toggleliquiddmg =
{
    // Default return conditions (must be player)
    if ( !(self.flags & FL_CLIENT) ) return;
    
    self.impulse = 0;
    
    // Toggle the state of the console variable using the server flag
    if (query_configflag(SVR_LIQDAM)) update_configflag(SVR_LIQDAM, FALSE);
    else update_configflag(SVR_LIQDAM, TRUE);
    
    sprint(self, "\b[Impulse]\b Monster Liquid Damage ("); 
    if (query_configflag(SVR_LIQDAM)) sprint(self, "OFF)\n");
    else sprint(self, "ON)\n");
};

/*======================================================================
 TOGGLE AI path extra info (default ON)
======================================================================*/
void() toggleaipath =
{
    // Default return conditions (must be player)
    if ( !(self.flags & FL_CLIENT) ) return;
    
    self.impulse = 0;
    
    // Toggle the state of the console variable using the server flag
    if (query_configflag(SVR_AIPATH)) update_configflag(SVR_AIPATH, FALSE);
    else update_configflag(SVR_AIPATH, TRUE);
    
    sprint(self, "\b[Impulse]\b Path Corner Arrows ("); 
    if (query_configflag(SVR_AIPATH)) sprint(self, "OFF)\n");
    else sprint(self, "ON)\n");
    sprint(self, "Reload / Quickload map to see changes!\n");
};

/*======================================================================
 TOGGLE SG Projectiles (default ON)
======================================================================*/
void() toggleshotgunproj =
{
    // Default return conditions (must be player)
    if ( !(self.flags & FL_CLIENT) ) return;
    
    self.impulse = 0;
    // Toggle the state of the console variable using the server flag
    if (query_configflag(SVR_SHOTGPROJ)) update_configflag(SVR_SHOTGPROJ, FALSE);
    else update_configflag(SVR_SHOTGPROJ, TRUE);
    
    sprint(self, "\b[Impulse]\b Shotgun Projectiles ("); 
    if (query_configflag(SVR_SHOTGPROJ)) sprint(self, "OFF)\n");
    else sprint(self, "ON)\n");
};

/*======================================================================
 TOGGLE SG Casings (default ON)
======================================================================*/
void() toggleshotguncase =
{
    // Default return conditions (must be player)
    if ( !(self.flags & FL_CLIENT) ) return;
    
    self.impulse = 0;
    // Toggle the state of the console variable using the server flag
    if (query_configflag(SVR_SHOTGCASE)) update_configflag(SVR_SHOTGCASE, FALSE);
    else update_configflag(SVR_SHOTGCASE, TRUE);
    
    sprint(self, "\b[Impulse]\b Shotgun Casing ("); 
    if (query_configflag(SVR_SHOTGCASE)) sprint(self, "OFF)\n");
    else sprint(self, "ON)\n");
};



/*======================================================================
 TOGGLE texture pixel mode (linear = blurry, nearest = crispy)
======================================================================*/
void() togglepixels =
{    
    // Default return conditions (must be player)
    if ( !(self.flags & FL_CLIENT) ) return;
    
    self.impulse = 0;
    sprint(self, "\b[SETTINGS]\b texture filtering ("); 
    if (pixelmode == TRUE) {
        pixelmode = FALSE;
        stuffcmd(self, "\ngl_texturemode GL_LINEAR_MIPMAP_LINEAR\n");
        sprint(self, "blurry)\n");
    }
    else {
        pixelmode = TRUE;
        stuffcmd(self, "\ngl_texturemode GL_NEAREST_MIPMAP_LINEAR\n");
        sprint(self, "crispy)\n");
    }
};


//----------------------------------------------------------------------
void() togglecoopartifacts =
{    
    // Default return conditions (must be player)
    if ( !(self.flags & FL_CLIENT) ) return;
    if (coop == FALSE) {
        sprint(self,"\b[IMPULSE]\b Only works when COOP active!\n");
        return;
    }
    
    self.impulse = 0;
    sprint(self, "\b[SETTINGS]\b COOP Drop Artifacts ("); 
    if (coop_artifacts == TRUE) {
        coop_artifacts = FALSE;
        sprint(self, "OFF)\n");
    }
    else {
        coop_artifacts = TRUE;
        sprint(self, "ON)\n");
    }
};

//----------------------------------------------------------------------
void() togglecoopweapons =
{    
    // Default return conditions (must be player)
    if ( !(self.flags & FL_CLIENT) ) return;
    if (coop == FALSE) {
        sprint(self,"\b[IMPULSE]\b Only works when COOP active!\n");
        return;
    }
    
    self.impulse = 0;
    sprint(self, "\b[SETTINGS]\b COOP Weapons Respawn ("); 
    if (coop_weapons == TRUE) {
        coop_weapons = FALSE;
        sprint(self, "ON)\n");
    }
    else {
        coop_weapons = TRUE;
        sprint(self, "OFF)\n");
    }
};

//----------------------------------------------------------------------
void() togglecoopammoboxes =
{    
    // Default return conditions (must be player)
    if ( !(self.flags & FL_CLIENT) ) return;
    if (coop == FALSE) {
        sprint(self,"\b[IMPULSE]\b Only works when COOP active!\n");
        return;
    }
    
    self.impulse = 0;
    sprint(self, "\b[SETTINGS]\b COOP Ammo Boxes Respawn ("); 
    if (coop_ammoboxes == TRUE) {
        coop_ammoboxes = FALSE;
        sprint(self, "OFF)\n");
    }
    else {
        coop_ammoboxes = TRUE;
        sprint(self, "ON)\n");
    }
};

//----------------------------------------------------------------------
void() togglecoophealth =
{    
    // Default return conditions (must be player)
    if ( !(self.flags & FL_CLIENT) ) return;
    if (coop == FALSE) {
        sprint(self,"\b[IMPULSE]\b Only works when COOP active!\n");
        return;
    }
    
    self.impulse = 0;
    sprint(self, "\b[SETTINGS]\b COOP Health Packs Respawn ("); 
    if (coop_health == TRUE) {
        coop_health = FALSE;
        sprint(self, "OFF)\n");
    }
    else {
        coop_health = TRUE;
        sprint(self, "ON)\n");
    }
};

//----------------------------------------------------------------------
void() togglecooppowerups =
{    
    // Default return conditions (must be player)
    if ( !(self.flags & FL_CLIENT) ) return;
    if (coop == FALSE) {
        sprint(self,"\b[IMPULSE]\b Only works when COOP active!\n");
        return;
    }
    
    self.impulse = 0;
    sprint(self, "\b[SETTINGS]\b COOP Powerups Respawn ("); 
    if (coop_powerups == TRUE) {
        coop_powerups = FALSE;
        sprint(self, "OFF)\n");
    }
    else {
        coop_powerups = TRUE;
        sprint(self, "ON)\n");
    }
};

//----------------------------------------------------------------------
void() update_weather =
{
    // Check for advanced engines
    if (ext_active) {
        // FTE/QSS only supports one console command for weather
        // Engine does not remember setting and resets it
        if (checkextension("FTE_SV_POINTPARTICLES"))
            if (query_configflag(SVR_WEATHER))
                stuffcmd(self, "\nr_part_rain 0\n");
            else stuffcmd(self, "\nr_part_rain 1\n");
        else {
            // DP engine remembers rain/snow weather settings
            // DP has separate commands for weather types
            if (query_configflag(SVR_WEATHER)) {
                // Weather disabled
                stuffcmd(self, "\ncl_particles_rain 0\n");
                stuffcmd(self, "\ncl_particles_snow 0\n");
            }
            else {
                // Weather enabled
                stuffcmd(self, "\ncl_particles_rain 1\n");
                stuffcmd(self, "\ncl_particles_snow 1\n");
            }
        }
    }
};

//----------------------------------------------------------------------
void() toggleweather =
{
    // Default return conditions (must be player)
    if ( !(self.flags & FL_CLIENT) ) return;
    // Enhanced engines enabled?
    if (!ext_active) {
        sprint(self,"\b[Impulse]\b Engine does not support weather.\n");
        return;
    }
    
    self.impulse = 0;

    // Toggle the state of the console variable using the server flag
    if (query_configflag(SVR_WEATHER)) update_configflag(SVR_WEATHER, FALSE);
    else update_configflag(SVR_WEATHER, TRUE);
    
    sprint(self, "\b[Impulse]\b Particle weather system ("); 
    if (query_configflag(SVR_WEATHER)) sprint(self, "OFF)\n");
    else sprint(self, "ON)\n");
    update_weather();
};

//----------------------------------------------------------------------
void() togglebodyremoval =
{
    // Default return conditions (must be player)
    if ( !(self.flags & FL_CLIENT) ) return;

    self.impulse = 0;

    // Toggle body removal/fade system timer
    // default = time + 10 + random() * 5
    if (map_bodyfadeaway > 0) map_bodyfadeaway = 0;
    else map_bodyfadeaway = 5;
    
    sprint(self, "\b[Impulse]\b Monster body removal ("); 
    if (map_bodyfadeaway > 0) sprint(self, "ON)\n");
    else sprint(self, "OFF)\n");
};


/*======================================================================
 Require DEVELOPER mode to be active
======================================================================*/
void() toggleupgradeaxe =
{
    // Default return conditions (must be player and developer)
    if ( !(self.flags & FL_CLIENT) ) return;
    if (developer == 0) {
        sprint(self,"\b[IMPULSE]\b Only works in developer mode!\n");
        return;
    }
    
    self.impulse = 0;
    if (self.moditems & IT_UPGRADE_AXE) {
        dprint("\b[Impulse]\b Shadow Axe Removed\n"); 
        self.moditems = self.moditems - IT_UPGRADE_AXE;
    }
    else {
        dprint("\b[Impulse]\b Shadow Axe Added\n"); 
        self.moditems = self.moditems | IT_UPGRADE_AXE;
    }
        
    if (self.weapon == IT_AXE) W_SetCurrentAmmo(self);
};

//----------------------------------------------------------------------
void() toggleupgradessg =
{
    // Default return conditions (must be player and developer)
    if ( !(self.flags & FL_CLIENT) ) return;
    if (developer == 0) {
        sprint(self,"\b[IMPULSE]\b Only works in developer mode!\n");
        return;
    }
    
    self.impulse = 0;
    if (self.moditems & IT_UPGRADE_SSG) {
        dprint("\b[Impulse]\b Widowmaker Shotgun Removed\n"); 
        self.moditems = self.moditems - IT_UPGRADE_SSG;
    }
    else {
        dprint("\b[Impulse]\b Widowmaker Shotgun Added\n"); 
        self.moditems = self.moditems | IT_UPGRADE_SSG;
    }
        
    // make sure player VIEW model is up to date
    if (self.weapon == IT_SUPER_SHOTGUN) W_SetCurrentAmmo(self);
};

//----------------------------------------------------------------------
void() toggleupgradelg =
{
    // Default return conditions (must be player and developer)
    if ( !(self.flags & FL_CLIENT) ) return;
    if (developer == 0) {
        sprint(self,"\b[IMPULSE]\b Only works in developer mode!\n");
        return;
    }
    
    self.impulse = 0;
    if (self.moditems & IT_UPGRADE_LG) {
        dprint("\b[Impulse]\b Plasma Gun Removed\n"); 
        self.moditems = self.moditems - IT_UPGRADE_LG;
    }
    else {
        dprint("\b[Impulse]\b Plasma Gun Added\n"); 
        self.moditems = self.moditems | IT_UPGRADE_LG;
    }
        
    // make sure player VIEW model is up to date
    if (self.weapon == IT_LIGHTNING) W_SetCurrentAmmo(self);
};

//----------------------------------------------------------------------
void() toggledevhelpers =
{
    // Default return conditions (must be player)
    if ( !(self.flags & FL_CLIENT) ) return;
    if (developer == 0) {
        sprint(self,"\b[IMPULSE]\b Only works in developer mode!\n");
        return;
    }

    self.impulse = 0;

    // Toggle the state of the console variable using the server flag
    if (query_configflag(SVR_DEVHELPER)) update_configflag(SVR_DEVHELPER, FALSE);
    else update_configflag(SVR_DEVHELPER, TRUE);
    
    sprint(self, "\b[Impulse]\b Dev Helpers are ("); 
    if (query_configflag(SVR_DEVHELPER)) sprint(self, "OFF)\n");
    else sprint(self, "ON)\n");
};

//----------------------------------------------------------------------
void() CycleParticleDebug =
{
    // Default return conditions (must be player)
    if ( !(self.flags & FL_CLIENT) ) return;
    if (developer == 0) {
        sprint(self,"\b[IMPULSE]\b Only works in developer mode!\n");
        return;
    }
    
    self.impulse = 0;
    
    part_debug = part_debug + 1;
    if (part_debug > PARTICLE_DEBUGMAX) part_debug = 0;

    sprint(self, "\b[Impulse]\b Particle debug level (");
    sprint(self, ftos(part_debug));
    sprint(self, ")\n");
};


/*======================================================================
 IMPULSE 9
 Gives the player certain weapons and ammo (CHEAT COMMAND)
======================================================================*/
void() CheatCommand =
{
    if (intermission_running) return;        // intermission or finale
    if (deathmatch || coop) return;

    dprint("\b[Impulse 9]\b Weapon Cheat Command\n");
    dprint("- Use Impulse 245 to get silver/gold keys!\n");
    self.impulse = 0;
    self.ammo_rockets = 100;
    self.ammo_nails = 150;
    self.ammo_shells = 150;
    self.ammo_cells = 150;
    self.ammo_lava_nails = 200;
    self.ammo_multi_rockets = 100;
    self.ammo_plasma = 100;
    self.ammo_bolts = 100;
    self.ammo_poison = 100;    

    self.moditems = self.moditems | IT_UPGRADE_AXE | IT_UPGRADE_SSG | IT_UPGRADE_LG;
    self.items = self.items | IT_AXE | IT_SHOTGUN | IT_SUPER_SHOTGUN | IT_NAILGUN |
                IT_SUPER_NAILGUN | IT_GRENADE_LAUNCHER | IT_ROCKET_LAUNCHER | IT_LIGHTNING | IT_LAVA_NAILGUN | IT_LAVA_SUPER_NAILGUN | IT_MULTI_GRENADE | IT_MULTI_ROCKET | IT_PLASMA_GUN | IT_EXTRA_WEAPON;
    self.items2 = self.items2 | IT2_ARMOR3 | IT2_PROXIMITY_GUN | IT2_MJOLNIR | IT2_LASER_CANNON;
    self.weapon = IT2_LASER_CANNON;
    self.armorvalue = 150;
    W_SetCurrentAmmo (self);
};

/*======================================================================
 IMPULSE 250
======================================================================*/
void() SharpshooterCheat =
{
    if (intermission_running) return;        // intermission or finale
    if (deathmatch || coop) return;

    dprint("\b[Impulse 250]\b Sharp Shooter Cheat\n");
    self.impulse = 0;
    self.sharpshoot_time = 1;
    self.sharpshoot_finished = time + POWERUP_TIMER;
    self.items = self.items | IT_QUAD;
    self.moditems = self.moditems | IT_ARTSHARP;
};

/*======================================================================
 IMPULSE 251
======================================================================*/
void() NailPiercerCheat =
{
    if (intermission_running) return;        // intermission or finale
    if (deathmatch || coop) return;

    dprint("\b[Impulse 251]\b Nail Piercer Cheat\n");
    self.impulse = 0;
    self.nailpiercer_time = 1;
    self.nailpiercer_finished = time + POWERUP_TIMER;
    self.items = self.items | IT_QUAD;
    self.moditems = self.moditems | IT_ARTPIERCE;
};

/*======================================================================
 IMPULSE 252
======================================================================*/
void() EnvSuitCheat =
{
    if (intermission_running) return;        // intermission or finale
    if (deathmatch || coop) return;

    dprint("\b[Impulse 252]\b Environmental Suit Cheat\n");
    self.impulse = 0;
    self.rad_time = 1;
    self.radsuit_finished = time + POWERUP_TIMER;
    self.items = self.items | IT_SUIT;
};

/*======================================================================
 IMPULSE 253
======================================================================*/
void() PentagramCheat =
{
    if (intermission_running) return;        // intermission or finale
    if (deathmatch || coop) return;

    dprint("\b[Impulse 253]\b Pentagram of Protection Cheat\n");
    self.impulse = 0;
    self.invincible_time = 1;
    self.invincible_finished = time + POWERUP_TIMER;
    self.items = self.items | IT_INVULNERABILITY;
};

/*======================================================================
 IMPULSE 254
======================================================================*/
void() InvisibleCheat =
{
    if (intermission_running) return;        // intermission or finale
    if (deathmatch || coop) return;

    dprint("\b[Impulse 254]\b Ring of Shadows Cheat\n");
    self.impulse = 0;
    self.invisible_time = 1;
    self.invisible_finished = time + POWERUP_TIMER;
    self.items = self.items | IT_INVISIBILITY;
};

/*======================================================================
 IMPULSE 255
======================================================================*/
void() QuadDamageCheat =
{
    if (intermission_running) return;        // intermission or finale
    if (deathmatch || coop) return;

    dprint("\b[Impulse 255]\b Quad Damage Cheat\n");
    self.impulse = 0;
    self.super_time = 1;
    self.super_damage_finished = time + POWERUP_TIMER;
    self.items = self.items | IT_QUAD;
};
