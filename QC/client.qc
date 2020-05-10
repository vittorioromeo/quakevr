float modelindex_eyes, modelindex_player, modelindex_hammer;

//
//
//
// ----------------------------------------------------------------------------
// Level changing / Intermission
// ----------------------------------------------------------------------------

float intermission_running;
float intermission_exittime;

/*QUAKED info_intermission (1 0.5 0.5) (-16 -16 -16) (16 16 16)
This is the camera point for the intermission.
Use mangle instead of angle, so you can set pitch or roll as well as yaw.  'pitch roll yaw'
*/
void() info_intermission =
{
};

void() SetChangeParms =
{
    if(self.health <= 0)
    {
        SetNewParms();

        // ROGUE
        parm22 = self.steam; // Save the current team of the player

        return;
    }

    VR_ItemUtil_EntDelItem(self, IID_KEY1);
    VR_ItemUtil_EntDelItem(self, IID_KEY2);
    VR_ItemUtil_EntDelItem(self, IID_INVISIBILITY);
    VR_ItemUtil_EntDelItem(self, IID_INVULNERABILITY);
    VR_ItemUtil_EntDelItem(self, IID_SUIT);
    VR_ItemUtil_EntDelItem(self, IID_QUAD);

    // HIPNOTIC
    VR_ItemUtil_EntDelItem(self, IID_WETSUIT);
    VR_ItemUtil_EntDelItem(self, IID_EMPATHY_SHIELDS);

    // ROGUE
    VR_ItemUtil_EntDelItem(self, IID_ANTIGRAV);
    VR_ItemUtil_EntDelItem(self, IID_SHIELD);
    VR_ItemUtil_EntDelItem(self, IID_SUPERHEALTH);

    self.gravity = 1.0;

    // cap super health
    if(self.health > 100)
    {
        self.health = 100;
    }

    if(self.health < 50)
    {
        self.health = 50;
    }

    // ROGUE
    // Don't carry items between levels in CTF
    teamplay = cvar_hget(cvarh_teamplay);
    if(teamplay >= TEAM_CTF)
    {
        SetNewParms();
    }
    else
    {
        parm1 = self.items;
        parm2 = self.health;
        parm3 = self.armorvalue;

        if(self.ammo_shells < 25)
            parm4 = 25;
        else
            parm4 = self.ammo_shells;

        parm5 = self.ammo_nails;
        parm6 = self.ammo_rockets;
        parm7 = self.ammo_cells;
        parm8 = self.weapon;
        parm9 = self.armortype * 100;
        parm10 = self.weapon2;

        // --------------------------------------------------------------------
        // Holster weapon ids:
        parm11 = self.holsterweapon0;
        parm12 = self.holsterweapon1;
        parm13 = self.holsterweapon2;
        parm14 = self.holsterweapon3;
        parm15 = self.holsterweapon4;
        parm16 = self.holsterweapon5;

        // --------------------------------------------------------------------
        // Mission packs:
        // HIPNOTIC
        parm17 = self.hipnotic_items;

        // ROGUE
        parm18 = self.rogue_items;
        parm19 = self.ammo_lava_nails;
        parm20 = self.ammo_multi_rockets;
        parm21 = self.ammo_plasma;
    }

    // ROGUE
    parm22 = self.steam; // Save team setting

    // ------------------------------------------------------------------------
    // Weapon flags:
    parm23 = self.weaponflags;
    parm24 = self.weaponflags2;
    parm25 = self.holsterweaponflags0;
    parm26 = self.holsterweaponflags1;
    parm27 = self.holsterweaponflags2;
    parm28 = self.holsterweaponflags3;
    parm29 = self.holsterweaponflags4;
    parm30 = self.holsterweaponflags5;
};

void() OnSpawnServer =
{
    VR_CVars_InitAllHandles();
}

void() SetNewParms =
{
    // HONEY
    // The secret count doesn't get updated when using the restart command for some reason.
    SUB_DoLater(updateSecrets, 0.2);

    parm1 = IT_SHOTGUN | IT_AXE;
    parm2 = 100;
    parm3 = 0;
    parm4 = 25;
    parm5 = 0;
    parm6 = 0;
    parm7 = 0;
    parm8 = WID_FIST;
    parm9 = 0;
    parm10 = WID_FIST;

    // ------------------------------------------------------------------------
    // Holster weapon ids:
    parm11 = WID_FIST;
    parm12 = WID_FIST;
    parm13 = WID_AXE;
    parm14 = WID_SHOTGUN;
    parm15 = WID_FIST;
    parm16 = WID_FIST;

    // ------------------------------------------------------------------------
    // Mission packs:
    // HIPNOTIC
    parm17 = 0;

    // ROGUE
    if(deathmatch && teamplay >= TEAM_CTF)
    {
        parm18 = ROGUE_IT_GRAPPLE;
    }
    else
    {
        parm18 = 0;
    }

    // ROGUE
    parm19 = 0;
    parm20 = 0;
    parm21 = 0;

    // ROGUE
    parm22 = -1; // CTF: reset current team selection

    // ------------------------------------------------------------------------
    // Weapon flags:
    parm23 = 0;
    parm24 = 0;
    parm25 = 0;
    parm26 = 0;
    parm27 = 0;
    parm28 = 0;
    parm29 = 0;
    parm30 = 0;
};

void() DecodeLevelParms =
{
    if(!deathmatch)
    {
        if(world.model == "maps/start.bsp")
            SetNewParms();        // take away all stuff on starting new episode
        if(world.model == "maps/orig_start.bsp")
            SetNewParms();        // take away all stuff on starting new episode
    }

    // HIPNOTIC
    if(world.model == "maps/start.bsp")
        SetNewParms();      // take away all stuff on starting new episode
    if(world.model == "maps/orig_start.bsp")
        SetNewParms();      // take away all stuff on starting new episode
    if(world.model == "maps/hip1m1.bsp")
        SetNewParms();      // take away all stuff on starting new episode
    if(world.model == "maps/hip2m1.bsp")
        SetNewParms();      // take away all stuff on starting new episode
    if(world.model == "maps/hip3m1.bsp")
        SetNewParms();      // take away all stuff on starting new episode

    // ROGUE
    if(world.model == "maps/r2m1.bsp" && !deathmatch)
        SetNewParms(); // PGM - 01/29 change to have level 8 reset all your stuff.

    self.items = parm1;
    self.health = parm2;
    self.armorvalue = parm3;
    self.ammo_shells = parm4;
    self.ammo_nails = parm5;
    self.ammo_rockets = parm6;
    self.ammo_cells = parm7;
    self.weapon = parm8;
    self.armortype = parm9 * 0.01;
    self.weapon2 = parm10;

    // ------------------------------------------------------------------------
    // Holster weapon ids:
    self.holsterweapon0 = parm11;
    self.holsterweapon1 = parm12;
    self.holsterweapon2 = parm13;
    self.holsterweapon3 = parm14;
    self.holsterweapon4 = parm15;
    self.holsterweapon5 = parm16;
    self.holsterweaponmodel0 = WeaponIdToModel(self.holsterweapon0, parm25);
    self.holsterweaponmodel1 = WeaponIdToModel(self.holsterweapon1, parm26);
    self.holsterweaponmodel2 = WeaponIdToModel(self.holsterweapon2, parm27);
    self.holsterweaponmodel3 = WeaponIdToModel(self.holsterweapon3, parm28);
    self.holsterweaponmodel4 = WeaponIdToModel(self.holsterweapon4, parm29);
    self.holsterweaponmodel5 = WeaponIdToModel(self.holsterweapon5, parm30);

    // ------------------------------------------------------------------------
    // Mission packs:
    // HIPNOTIC
    self.hipnotic_items = parm17;

    // ROGUE
    self.rogue_items = parm18;
    self.ammo_lava_nails = parm19;
    self.ammo_multi_rockets = parm20;
    self.ammo_plasma = parm21;

    // ROGUE
    //ZOID--
    if(TeamColorIsLegal(parm22)) {
        self.steam = parm22;
        TeamSetSkin();
    }
    //--ZOID

    // ------------------------------------------------------------------------
    // Weapon flags:
    self.weaponflags = parm23;
    self.weaponflags2 = parm24;
    self.holsterweaponflags0 = parm25;
    self.holsterweaponflags1 = parm26;
    self.holsterweaponflags2 = parm27;
    self.holsterweaponflags3 = parm28;
    self.holsterweaponflags4 = parm29;
    self.holsterweaponflags5 = parm30;
};

/*
============
FindIntermission

Returns the entity to view from
============
*/
entity() FindIntermission =
{
    local    entity spot;
    local    float cyc;

// look for info_intermission first
    spot = find(world, classname, "info_intermission");
    if(spot)
    {    // pick a random one
        cyc = random() * 4;
        while(cyc > 1)
        {
            spot = find(spot, classname, "info_intermission");
            if(!spot)
                spot = find(spot, classname, "info_intermission");
            cyc = cyc - 1;
        }
        return spot;
    }

// then look for the start position
    spot = find(world, classname, "info_player_start");
    if(spot)
        return spot;

// testinfo_player_start is only found in regioned levels
    spot = find(world, classname, "testplayerstart");
    if(spot)
        return spot;

    objerror ("FindIntermission: no spot");

    return world;
};


string nextmap;
void() GotoNextMap =
{
    if(cvar_hget(cvarh_samelevel))    // if samelevel is set, stay on same level
        changelevel (mapname);
    else
        changelevel (nextmap);
};


void() ExitIntermission =
{
    // skip any text in deathmatch
    if(deathmatch)
    {
        GotoNextMap();
        return;
    }

    intermission_exittime = time + 1;
    intermission_running = intermission_running + 1;

    // run some text if at the end of an episode
    if(intermission_running == 2)
    {
        if(world.model == "maps/e1m7.bsp")
        {
            WriteByte(MSG_ALL, SVC_CDTRACK);
            WriteByte(MSG_ALL, 2);
            WriteByte(MSG_ALL, 3);
            if(!cvar_hget(cvarh_registered))
            {
                WriteByte(MSG_ALL, SVC_FINALE);
                WriteString(MSG_ALL, "As the corpse of the monstrous entity\nChthon sinks back into the lava whence\nit rose, you grip the Rune of Earth\nMagic tightly. Now that you have\nconquered the Dimension of the Doomed,\nrealm of Earth Magic, you are ready to\ncomplete your task in the other three\nhaunted lands of Quake. Or are you? If\nyou don't register Quake, you'll never\nknow what awaits you in the Realm of\nBlack Magic, the Netherworld, and the\nElder World!");
            }
            else
            {
                WriteByte(MSG_ALL, SVC_FINALE);
                WriteString(MSG_ALL, "As the corpse of the monstrous entity\nChthon sinks back into the lava whence\nit rose, you grip the Rune of Earth\nMagic tightly. Now that you have\nconquered the Dimension of the Doomed,\nrealm of Earth Magic, you are ready to\ncomplete your task. A Rune of magic\npower lies at the end of each haunted\nland of Quake. Go forth, seek the\ntotality of the four Runes!");
            }
            return;
        }
        else if(world.model == "maps/e2m6.bsp")
        {
            WriteByte(MSG_ALL, SVC_CDTRACK);
            WriteByte(MSG_ALL, 2);
            WriteByte(MSG_ALL, 3);

            WriteByte(MSG_ALL, SVC_FINALE);
            WriteString(MSG_ALL, "The Rune of Black Magic throbs evilly in\nyour hand and whispers dark thoughts\ninto your brain. You learn the inmost\nlore of the Hell-Mother; Shub-Niggurath!\nYou now know that she is behind all the\nterrible plotting which has led to so\nmuch death and horror. But she is not\ninviolate! Armed with this Rune, you\nrealize that once all four Runes are\ncombined, the gate to Shub-Niggurath's\nPit will open, and you can face the\nWitch-Goddess herself in her frightful\notherworld cathedral.");
            return;
        }
        else if(world.model == "maps/e3m6.bsp")
        {
            WriteByte(MSG_ALL, SVC_CDTRACK);
            WriteByte(MSG_ALL, 2);
            WriteByte(MSG_ALL, 3);

            WriteByte(MSG_ALL, SVC_FINALE);
            WriteString(MSG_ALL, "The charred viscera of diabolic horrors\nbubble viscously as you seize the Rune\nof Hell Magic. Its heat scorches your\nhand, and its terrible secrets blight\nyour mind. Gathering the shreds of your\ncourage, you shake the devil's shackles\nfrom your soul, and become ever more\nhard and determined to destroy the\nhideous creatures whose mere existence\nthreatens the souls and psyches of all\nthe population of Earth.");
            return;
        }
        else if(world.model == "maps/e4m7.bsp")
        {
            WriteByte(MSG_ALL, SVC_CDTRACK);
            WriteByte(MSG_ALL, 2);
            WriteByte(MSG_ALL, 3);

            WriteByte(MSG_ALL, SVC_FINALE);
            WriteString(MSG_ALL, "Despite the awful might of the Elder\nWorld, you have achieved the Rune of\nElder Magic, capstone of all types of\narcane wisdom. Beyond good and evil,\nbeyond life and death, the Rune\npulsates, heavy with import. Patient and\npotent, the Elder Being Shub-Niggurath\nweaves her dire plans to clear off all\nlife from the Earth, and bring her own\nfoul offspring to our world! For all the\ndwellers in these nightmare dimensions\nare her descendants! Once all Runes of\nmagic power are united, the energy\nbehind them will blast open the Gateway\nto Shub-Niggurath, and you can travel\nthere to foil the Hell-Mother's plots\nin person.");
            return;
        }

        // HIPNOTIC
        if(world.model == "maps/hip1m4.bsp")
        {
            WriteByte(MSG_ALL, SVC_CDTRACK);
            WriteByte(MSG_ALL, 6);
            WriteByte(MSG_ALL, 3);
            WriteByte(MSG_ALL, SVC_FINALE);

            WriteString(MSG_ALL, "Deep within the bowels of the\nResearch Facility, you discover the\npassage that the followers of Quake\nhave used to enter our world.\nThe bastards used some type of\ngigantic teleporter to overload\none of our own slipgates!  As long as\nthis portal exists, Earth will never\nbe safe from Quake's cruel minions.");
            return;
        }
        else if(world.model == "maps/hip2m5.bsp")
        {
            WriteByte(MSG_ALL, SVC_CDTRACK);
            WriteByte(MSG_ALL, 6);
            WriteByte(MSG_ALL, 3);

            WriteByte(MSG_ALL, SVC_FINALE);
            WriteString(MSG_ALL, "After destroying the power generator,\nyou pass beyond the gate of Mortum's\nKeep.  A wave of nausea suddenly flows\nover you and you find yourself cast\nout into a liquid void.  You float\nlifelessly, yet aware, in a lavender\nsea of energy.");
            return;
        }
        else if(world.model == "maps/hipend.bsp")
        {
            WriteByte(MSG_ALL, SVC_CDTRACK);
            WriteByte(MSG_ALL, 2);
            WriteByte(MSG_ALL, 3);

            WriteByte(MSG_ALL, SVC_FINALE);
            WriteString(MSG_ALL, "After the last echoes of Armagon's\ndeath yell fade away, you breathe a\nheavy sigh of relief.  With the loss\nof his magic, Armagon's fortress\nbegins to collapse.  The rift he\ncreated to send his grisly troops\nthrough time slowly closes and seals\nitself forever.  In the chaos that\nensues, a wall collapses, revealing\none remaining time portal.  With your\nchances to escape rapidly growing\nslim, you race for the portal,\nmindless of your destination.  In a\nflash of light, you find yourself\nback at Command HQ, safe and sound.");
            return;
        }

        // ROGUE
        else if(world.model == "maps/r1m7.bsp")
        {
            WriteByte (MSG_ALL, SVC_FINALE);
            WriteString(MSG_ALL, "Victory! The Overlord's mangled\nremains are the evidence.  His evil\nWrath army?  Cast out to wander\naimlessly throughout time.\n\nAs the Slipgate fog surrounds you,\nthoughts rage into your consciousness:\nHas Quake's oppressive reign ended?\nIs it Salvation, or Damnation, which\nwaits beyond the Vortex?\n\nAnother thought, not quite your own,\nrazors through the haze.  \"Forgiveness\ncan yet be granted; Our Master remains\nto absolve your sins against his Chosen.\nFall down upon your knees-pray for\nQuake's mercy.\"");
            return;
        }

        GotoNextMap();
    }

    if(intermission_running == 3)
    {
        if(!cvar_hget(cvarh_registered))
        {    // shareware episode has been completed, go to sell screen
            WriteByte(MSG_ALL, SVC_SELLSCREEN);
            return;
        }

        if((serverflags&15) == 15)
        {
            WriteByte(MSG_ALL, SVC_FINALE);
            WriteString(MSG_ALL, "Now, you have all four Runes. You sense\ntremendous invisible forces moving to\nunseal ancient barriers. Shub-Niggurath\nhad hoped to use the Runes Herself to\nclear off the Earth, but now instead,\nyou will use them to enter her home and\nconfront her as an avatar of avenging\nEarth-life. If you defeat her, you will\nbe remembered forever as the savior of\nthe planet. If she conquers, it will be\nas if you had never been born.");
            return;
        }

        // HIPNOTIC
        if(world.model == "maps/hip1m4.bsp")
        {
            WriteByte(MSG_ALL, SVC_FINALE);
            WriteString(MSG_ALL, "If you can find the source of the\nportal's power, you can shut it\ndown--possibly forever!  With only a\nmoment's consideration for your own\nsafety, you re-enter the dark domain,\nknowing Hell would be a better fate\nthan experiencing the reign of Quake.");
            return;
        }
        else if(world.model == "maps/hip2m5.bsp")
        {
            WriteByte(MSG_ALL, SVC_FINALE);
            WriteString(MSG_ALL, "After what seems like an eternity,\nyou feel the presence of a diabolical\nintelligence.  You are held helpless\nfor a moment as your mind is open to\nthat of Armagon--Quake's General and\nmaster of this realm.  Recognizing\nyou as the one who foiled his\nattempt to conquer Earth, a hellish\nhowl fills your mind and blots out\nall consciousness.  When you awake,\nyou find yourself on the shores of\nreality, but in a time and place\nunknown to you.");
            return;
        }
        else if(world.model == "maps/hipend.bsp")
        {
            WriteByte(MSG_ALL, SVC_FINALE);
            WriteString(MSG_ALL, "Congratulations!  You are victorious!\nThe minions of Quake have once again\nfallen before your mighty hand.\nIs this the last you will see of\nQuake's hellions?\n\nOnly time will tell...");
            intermission_exittime = time + 10000000;  // never allow exit
            return;
        }
   }

    GotoNextMap();
};

/*
============
IntermissionThink

When the player presses attack or jump, change to the next level
============
*/
void() IntermissionThink =
{
    if(time < intermission_exittime)
        return;

    if(!self.button0 && !self.button1 && !self.button2)
        return;

    ExitIntermission();
};

void() execute_changelevel =
{
    local entity pos;

    // HONEY
    local entity fogsettings;

    intermission_running = 1;

    // enforce a wait time before allowing changelevel
    if(deathmatch)
        intermission_exittime = time + 5;
    else
        intermission_exittime = time + 2;

    WriteByte(MSG_ALL, SVC_CDTRACK);
    WriteByte(MSG_ALL, 3);
    WriteByte(MSG_ALL, 3);

    pos = FindIntermission();

    // HONEY
    if(pos.target != "") {
        fogsettings = find(world, targetname, pos.target);
        if(fogsettings.classname == "trigger_fogcontroller") {
            fogsettings = fogsettings.owner;
        }
    }

    other = find(world, classname, "player");
    while(other != world)
    {
        other.view_ofs = '0 0 0';
        other.angles = other.v_angle = pos.mangle;
        other.fixangle = TRUE;        // turn this way immediately
        other.nextthink = time + 0.5;
        other.takedamage = DAMAGE_NO;
        other.solid = SOLID_NOT;
        other.movetype = MOVETYPE_NONE;
        other.modelindex = 0;
        setorigin(other, pos.origin);

        // HONEY
        if(fogsettings.classname == "trigger_fogcontroller") {
            fog_setOverrideConditional(other, fogsettings.view_ofs, fogsettings.ideal_yaw, 1);
        }

        other = find(other, classname, "player");
    }

    WriteByte(MSG_ALL, SVC_INTERMISSION);
};


void() changelevel_touch =
{
    if(other.classname != "player")
        return;

    local float noexit;
    noexit = cvar_hget(cvarh_noexit);

    if((noexit == 1) || ((noexit == 2) && (mapname != "start")))
    {
        T_Damage(other, self, self, 50000);
        return;
    }

    if(coop || deathmatch)
    {
        bprint(other.netname);
        bprint(" exited the level\n");
    }

    nextmap = self.map;

    SUB_UseTargets();

    if((self.spawnflags & 1) && (deathmatch == 0))
    {    // NO_INTERMISSION
        GotoNextMap();
        return;
    }

    self.touch = SUB_Null;

// we can't move people right now, because touch functions are called
// in the middle of C movement code, so set a think time to do it
    self.think = execute_changelevel;
    self.nextthink = time + 0.1;
};

/*QUAKED trigger_changelevel (0.5 0.5 0.5) ? NO_INTERMISSION
When the player touches this, he gets sent to the map listed in the "map" variable.  Unless the NO_INTERMISSION flag is set, the view will go to the info_intermission spot and display stats.
*/
void() trigger_changelevel =
{
    if(!self.map)
        objerror ("changelevel trigger doesn't have map");

    InitTrigger();
    self.touch = changelevel_touch;
};


/*
=============================================================================

                PLAYER GAME EDGE FUNCTIONS

=============================================================================
*/

void() set_suicide_frame;

// called by ClientKill and DeadThink
void() respawn =
{
    if(coop)
    {
        // make a copy of the dead body for appearances sake
        // Unless the player died from falling in a pit!
        if(!self.enemy) // HONEY
            CopyToBodyQue(self);

        // get the spawn parms as they were at level start
        setspawnparms(self);

        // respawn
        PutClientInServer();
    }
    else if(deathmatch)
    {
        // make a copy of the dead body for appearances sake
        // Unless the player died from falling in a pit!
        if(!self.enemy) // HONEY
            CopyToBodyQue(self);

        // set default spawn parms
        SetNewParms();

        // respawn
        PutClientInServer();

        // set the skin correctly.
        TeamSetSkin();
    }
    else
    {
        // restart the entire server
        localcmd ("restart\n");
    }
};


/*
============
ClientKill

Player entered the suicide command
============
*/
void() ClientKill =
{
    if((intermission_running)&&((coop)||(deathmatch)))  // not allowed during intermission
        return;

    local entity vSphere;

    // ZOID--
    if(self.suicide_count > 3)
    {
        sprint(self, "You have suicided too much already.\n");
        return;
    }

    TeamDropFlagOfPlayer(self);
    self.suicide_count = self.suicide_count + 1;
    // --ZOID

    if(tag_token_owner == self)
    {
        self.health = 0;
        self.solid = SOLID_NOT;
        tag_token_drop();
    }

    if(VR_ItemUtil_EntHasItem(self, IID_V_SPHERE))
    {
        vSphere = find(world, classname, "Vengeance");

        while(vSphere)
        {
            if(vSphere.owner == self)
            {
                remove(vSphere);
            }

            vSphere = find(vSphere, classname, "Vengeance");
        }
    }

    bprint(self.netname);
    bprint(" suicides\n");
    set_suicide_frame();
    self.modelindex = modelindex_player;
    self.frags = self.frags - 2;    // extra penalty
    respawn();
};

float(vector v) CheckSpawnPoint =
{
    return FALSE;
};

/*
============
SelectSpawnPoint

Returns the entity to spawn at
============
*/
entity() SelectSpawnPoint =
{
    local entity spot;
    local entity thing;
    local float  pcount;

    //ZOID--
    local entity startspot;
    local float  t;
    //--ZOID


// testinfo_player_start is only found in regioned levels
    spot = find(world, classname, "testplayerstart");
    if(spot)
        return spot;

// choose a info_player_deathmatch point
    if(coop)
    {
        lastspawn = find(lastspawn, classname, "info_player_coop");
        if(lastspawn == world)
            lastspawn = find(lastspawn, classname, "info_player_start");
        if(lastspawn != world)
            return lastspawn;
    }
    else if(deathmatch)
    {
        startspot = spot = lastspawn;
        t = 0;

        if(!self.ctf_killed && teamplay >= TEAM_CTF)
        {
            if(self.steam == TEAM1)
            {
                startspot = spot = team1_lastspawn;
                t = TEAM1;
            }
            else if(self.steam == TEAM2)
            {
                startspot = spot = team2_lastspawn;
                t = TEAM2;
            }
        }

        while(1)
        {
            if(t == TEAM1)
            {
                spot = find(spot, classname, "info_player_team1");
            }
            else if(t == TEAM2)
            {
                spot = find(spot, classname, "info_player_team2");
            }
            else
            {
                spot = find(spot, classname, "info_player_deathmatch");
            }

            if(spot != world)
            {
                if(spot == startspot)
                {
                    return startspot;
                }

                pcount = 0;
                thing = findradius(spot.origin, 32);

                while(thing)
                {
                    if(thing.classname == "player")
                        pcount = pcount + 1;
                    thing = thing.chain;
                }

                if(pcount == 0)
                {
                    if(t == TEAM1)
                    {
                        team1_lastspawn = spot;
                    }
                    else if(t == TEAM2)
                    {
                        team2_lastspawn = spot;
                    }
                    else
                    {
                        lastspawn = spot;
                    }

                    return spot;
                }
            }
            else
            {
                // pgm fix for ctf in levels w/o ctf support
                t = 0;
            }
        }
    }

    if(serverflags)
    {    // return with a rune to start
        spot = find(world, classname, "info_player_start2");
        if(spot)
            return spot;
    }

    spot = find(world, classname, "info_player_start");

    if(!spot)
    {
        dprint("WARNING: PutClientInServer: no info_player_start on level");

        // HONEY
        spot = spawn();
        setorigin(spot, '0 0 0');
    }

    return spot;
};

/*
===========
PutClientInServer

called each time a player is spawned
============
*/
void() PutClientInServer =
{
    local entity spot;

    self.classname = "player";
    self.health = 100;
    self.takedamage = DAMAGE_AIM;
    self.solid = SOLID_SLIDEBOX;
    self.movetype = MOVETYPE_WALK;
    self.show_hostile = 0;
    self.max_health = 100;
    self.flags = FL_CLIENT;
    self.air_finished = time + 12;
    self.dmg = 2;           // initial water damage
    self.super_damage_finished = 0;
    self.radsuit_finished = 0;
    self.invisible_finished = 0;
    self.invincible_finished = 0;
    self.effects = 0;
    self.invincible_time = 0;
    //JIM
    self.wetsuit_finished = 0;
    //MED
    self.empathy_finished = 0;
    //MED
    self.hipnotic_items = 0;
    self.gravity = 1.0;

    self.shield_finished = 0;
    self.antigrav_finished = 0;

    //ZOID--
    self.ctf_lastreturnedflag = -10;
    self.ctf_lastfraggedcarrier = -10;
    self.ctf_flagsince = -10;
    self.ctf_lasthurtcarrier = -10;
    //--ZOID

    // HONEY
    self.enemy = world;
    // A hack: This doesn't work at level load, so if time is < 2 try again a bit later.
    if(time < 2){
        SUB_DoLater(info_stuffcmd_do, 0.1);
    }

    DecodeLevelParms();

    // spawn selection must be after DecodeLevelParms because of team stuff
    spot = SelectSpawnPoint ();

    W_SetCurrentAmmo();

    self.attack_finished = time;
    self.offhand_attack_finished = time;
    self.mainhand_melee_attack_finished = time;
    self.offhand_melee_attack_finished = time;
    self.th_pain = player_pain;
    self.th_die = PlayerDie;

    self.deadflag = DEAD_NO;
    // paustime is set by teleporters to keep the player from moving a while
    self.pausetime = 0;

    self.origin = self.oldorigin = spot.origin + '0 0 1';    // 1998-07-21 Respawning where player died fix by Robert Field
    self.angles = spot.angles;
    self.fixangle = TRUE;        // turn this way immediately

    self.touch = player_touch;

    // oh, this is a hack!

    // VR: This works even if someone does not own SoA because it falls back to
    // 'progs/player.mdl'. (Hardcoded in C++.)
    setmodel(self, "progs/playham.mdl");
    modelindex_hammer = self.modelindex;

    setmodel(self, "progs/eyes.mdl");
    modelindex_eyes = self.modelindex;

    setmodel(self, "progs/player.mdl");
    modelindex_player = self.modelindex;

    setsize(self, '-16 -16 -24', '16 16 26');

    self.view_ofs = '0 0 22';

    self.velocity = '0 0 0';    // 1998-07-21 Player moves after respawn fix by Xian

    player_stand1(cVR_MainHand);
    player_stand1(cVR_OffHand);

    if(deathmatch || coop)
    {
        makevectors(self.angles);
        spawn_tfog(self.origin + v_forward*20);
    }

    // ROGUE
    //Grapple stuff
    self.on_hook = FALSE;
    self.hook_out = FALSE;
    self.on_hook2 = FALSE;
    self.hook_out2 = FALSE;

    self.handpos = self.origin;
    self.offhandpos = self.origin;

    self.handvel = '0 0 0';
    self.offhandvel = '0 0 0';

    self.handthrowvel = '0 0 0';
    self.offhandthrowvel = '0 0 0';

    self.handavel = '0 0 0';
    self.offhandavel = '0 0 0';

    self.headvel = '0 0 0';

    VRSetTeleporting(self, FALSE);
    VRSetOffHandGrabbing(self, FALSE);
    VRSetOffHandPrevGrabbing(self, FALSE);
    VRSetMainHandGrabbing(self, FALSE);
    VRSetMainHandPrevGrabbing(self, FALSE);
    VRSet2HAiming(self, FALSE);

    self.offhand_forcegrabbing = FALSE;
    self.mainhand_forcegrabbing = FALSE;

    spawn_tdeath(self.origin, self);

    readytime = time + 0.5;

    SpawnAllAmmoBoxWeapons();
};

void() SpawnAllAmmoBoxWeapons =
{
    local entity e;
    for(e = nextent(world); e != world; e = nextent(e))
    {
        if(IsAmmoBox(e))
        {
            SpawnAmmoBoxWeaponsImpl(e, self);
        }
    }
}

/*
=============================================================================

                QUAKED FUNCTIONS

=============================================================================
*/


/*QUAKED info_player_start (1 0 0) (-16 -16 -24) (16 16 24)
The normal starting point for a level.
*/
void() info_player_start =
{
};


/*QUAKED info_player_start2 (1 0 0) (-16 -16 -24) (16 16 24)
Only used on start map for the return point from an episode.
*/
void() info_player_start2 =
{
};


/*
saved out by quaked in region mode
*/
void() testplayerstart =
{
};

/*QUAKED info_player_deathmatch (1 0 1) (-16 -16 -24) (16 16 24)
potential spawning position for deathmatch games
*/
void() info_player_deathmatch =
{
    SpawnRunes();
};

/*QUAKED info_player_coop (1 0 1) (-16 -16 -24) (16 16 24)
potential spawning position for coop games
*/
void() info_player_coop =
{
};

/*
===============================================================================

RULES

===============================================================================
*/

/*
go to the next level for deathmatch
*/
// TODO VR: (P2) check here whether to go orig_start? or is this only deathmatch?
void() NextLevel =
{
    local entity o;

    if(mapname == "start")
    {
        if(!cvar_hget(cvarh_registered))
        {
            mapname = "e1m1";
        }
        else if(!(serverflags & 1))
        {
            mapname = "e1m1";
            serverflags = serverflags | 1;
        }
        else if(!(serverflags & 2))
        {
            mapname = "e2m1";
            serverflags = serverflags | 2;
        }
        else if(!(serverflags & 4))
        {
            mapname = "e3m1";
            serverflags = serverflags | 4;
        }
        else if(!(serverflags & 8))
        {
            mapname = "e4m1";
            serverflags = serverflags - 7;
        }

        o = spawn();
        o.map = mapname;
    }
    else
    {
        // find a trigger changelevel
        o = find(world, classname, "trigger_changelevel");

        // go back to start if no trigger_changelevel
        if(!o)
        {
            mapname = "start";
            o = spawn();
            o.map = mapname;
        }
    }

    nextmap = o.map;
    gameover = TRUE;

    if(o.nextthink < time)
    {
        o.think = execute_changelevel;
        o.nextthink = time + 0.1;
    }
};

/*
============
CheckRules

Exit deathmatch games upon conditions
============
*/
void() CheckRules =
{
    local    float        timelimit;
    local    float        fraglimit;

    if(gameover)    // someone else quit the game already
        return;

    timelimit = cvar_hget(cvarh_timelimit) * 60;
    fraglimit = cvar_hget(cvarh_fraglimit);

    if(deathmatch && timelimit && time >= timelimit)    // 1998-07-27 Timelimit/Fraglimit fix by Maddes
    {
        NextLevel();
        return;
    }

    if(deathmatch && fraglimit && self.frags >= fraglimit)    // 1998-07-27 Timelimit/Fraglimit fix by Maddes
    {
        NextLevel();
        return;
    }

    //ZOID--
    // update team scores?
    TeamCheckUpdate();
    //--ZOID
};

//============================================================================

void() PlayerDeathThink =
{
    local float forward;

    if((self.flags & FL_ONGROUND))
    {
        forward = vlen(self.velocity);
        forward = forward - 20;
        if(forward <= 0)
            self.velocity = '0 0 0';
        else
            self.velocity = forward * normalize(self.velocity);
    }

// wait for all buttons released
    if(self.deadflag == DEAD_DEAD)
    {
        if(self.button2 || self.button1 || self.button0)
            return;
        self.deadflag = DEAD_RESPAWNABLE;
        return;
    }

// wait for any button down
    if(!self.button2 && !self.button1 && !self.button0)
        return;

    self.button0 = 0;
    self.button1 = 0;
    self.button2 = 0;
    respawn();
};


void() PlayerJump =
{
    if(self.flags & FL_WATERJUMP)
        return;

    if(self.waterlevel >= 2)
    {
        if(self.watertype == CONTENT_WATER)
            self.velocity_z = 100;
        else if(self.watertype == CONTENT_SLIME)
            self.velocity_z = 80;
        else
            self.velocity_z = 50;

// play swiming sound
        if(self.swim_flag < time)
        {
            self.swim_flag = time + 1;
            if(random() < 0.5)
                sound(self, CHAN_BODY, "misc/water1.wav", 1, ATTN_NORM);
            else
                sound(self, CHAN_BODY, "misc/water2.wav", 1, ATTN_NORM);
        }

        return;
    }

    if(!(self.flags & FL_ONGROUND))
        return;

    if(!(self.flags & FL_JUMPRELEASED))
        return;        // don't pogo stick

    self.flags = self.flags - (self.flags & FL_JUMPRELEASED);

    self.flags = self.flags - FL_ONGROUND;    // don't stairwalk

    self.button2 = 0;
// player jumping sound
    sound(self, CHAN_BODY, "player/plyrjmp8.wav", 1, ATTN_NORM);
    self.velocity_z = self.velocity_z + 270;
};


/*
===========
WaterMove

============
*/
.float    dmgtime;

void() WaterMove =
{
//dprint(ftos(self.waterlevel));
    if(self.movetype == MOVETYPE_NOCLIP)
        return;
    if(self.health < 0)
        return;

    if(self.waterlevel != 3)
    {
        if(self.air_finished < time)
            sound(self, CHAN_VOICE, "player/gasp2.wav", 1, ATTN_NORM);
        else if(self.air_finished < time + 9)
            sound(self, CHAN_VOICE, "player/gasp1.wav", 1, ATTN_NORM);
        self.air_finished = time + 12;
        self.dmg = 2;
    }
    else if(self.air_finished < time)
    {    // drown!
        if(self.pain_finished < time)
        {
            self.dmg = self.dmg + 2;
            if(self.dmg > 15)
                self.dmg = 10;
            T_Damage(self, world, world, self.dmg);
            self.pain_finished = time + 1;
        }
    }

    if(!self.waterlevel)
    {
        if(self.flags & FL_INWATER)
        {
            // play leave water sound
            sound(self, CHAN_BODY, "misc/outwater.wav", 1, ATTN_NORM);
            self.flags = self.flags - FL_INWATER;
        }
        return;
    }

    if(self.watertype == CONTENT_LAVA)
    {    // do damage
        if(self.dmgtime < time)
        {
            if(self.radsuit_finished > time)
                self.dmgtime = time + 1;
            else
                self.dmgtime = time + 0.2;

            T_Damage(self, world, world, 10*self.waterlevel);
        }
    }
    else if(self.watertype == CONTENT_SLIME)
    {    // do damage
        if(self.dmgtime < time && self.radsuit_finished < time)
        {
            self.dmgtime = time + 1;
            T_Damage(self, world, world, 4*self.waterlevel);
        }
    }

    if(!(self.flags & FL_INWATER))
    {

// player enter water sound

        if(self.watertype == CONTENT_LAVA)
            sound(self, CHAN_BODY, "player/inlava.wav", 1, ATTN_NORM);
        if(self.watertype == CONTENT_WATER)
            sound(self, CHAN_BODY, "player/inh2o.wav", 1, ATTN_NORM);
        if(self.watertype == CONTENT_SLIME)
            sound(self, CHAN_BODY, "player/slimbrn2.wav", 1, ATTN_NORM);

        self.flags = self.flags | FL_INWATER;
        self.dmgtime = 0;
    }

    if(! (self.flags & FL_WATERJUMP))
        self.velocity = self.velocity - 0.8*self.waterlevel*frametime*self.velocity;
};

void() CheckWaterJump =
{
    local vector start, end;

// check for a jump-out-of-water
    makevectors(self.angles);
    start = self.origin;
    start_z = start_z + 8;
    v_forward_z = 0;
    normalize(v_forward);
    end = start + v_forward*24;
    traceline(start, end, TRUE, self);
    if(trace_fraction < 1)
    {    // solid at waist
        start_z = start_z + self.maxs_z - 8;
        end = start + v_forward*24;
        self.movedir = trace_plane_normal * -50;
        traceline(start, end, TRUE, self);
        if(trace_fraction == 1)
        {    // open at eye level
            self.flags = self.flags | FL_WATERJUMP;
            self.velocity_z = 225;
            self.flags = self.flags - (self.flags & FL_JUMPRELEASED);
            self.teleport_time = time + 2;    // safety net
            return;
        }
    }
};

/*
================
PlayerPreThink

Called every frame before physics are run
================
*/
//MED 01/17/97
void(float num_bubbles) DeathBubbles;
void() doWetsuit =
{
    if(self.wetsuit_finished > time)
    {
        if(self.waterlevel==2)
        {
            self.velocity = self.velocity * 1.25;
        }
        if(self.waterlevel==3)
        {
            self.velocity = self.velocity * 1.5;
        }
        if(self.waterlevel >= 2)
        {
            // play scuba sound
            if(self.swim_flag < time)
            {
                self.swim_flag = time + 7;
                sound(self, CHAN_BODY, "misc/wetsuit.wav", 1, ATTN_NORM);
            }
            //MED 01/17/97
            else
            {
                if(fabs(self.swim_flag - time - 6)<0.04)
                {
                    DeathBubbles(1);
                }
                else if(fabs(self.swim_flag - time - 5.5)<0.04)
                {
                    DeathBubbles(1);
                }
                else if(fabs(self.swim_flag - time - 5)<0.04)
                {
                    DeathBubbles(1);
                }
            }
         }
    }
}


void() PlayerPreThink =
{
    // FRIKBOT
    if(BotPreFrame())
        return;

    // HONEY
    fog_updateAll();

    if(intermission_running)
    {
        earthquake_prethink();
        IntermissionThink();    // otherwise a button could be missed between
        return;                    // the think tics
    }

    if(self.view_ofs == '0 0 0')
        return;        // intermission or finale

    // ROGUE
    if(earthquake_active)
    {
        if(self.flags & FL_ONGROUND)
        {
            self.velocity_x = self.velocity_x +
                                (random() * earthquake_intensity * 2) -
                                earthquake_intensity;

            self.velocity_y = self.velocity_y +
                                (random() * earthquake_intensity * 2) -
                                earthquake_intensity;

            self.velocity_z = self.velocity_z +
                                (random() * earthquake_intensity * 2) -
                                earthquake_intensity;
        }
    }

    //JIM
   // Kill player on Edge of Oblivion
    if((self.origin_z < -1300) && (world.model == "maps/hipdm1.bsp") &&
      (self.health > 0))
    {
        self.deathtype = "falling";

        if(self.invincible_finished >= time)
           {
           self.invincible_finished = 0;

           VR_ItemUtil_EntDelItem(self, IID_INVULNERABILITY);

           self.invincible_time = 0;
              self.invincible_finished = 0;
           self.effects = self.effects - (self.effects & EF_DIMLIGHT);
           }
        T_Damage(self, self, world, self.health + 1000);
    }

    //JIM
    earthquake_prethink();

    makevectors(self.v_angle);        // is this still used

    //ZOID--
    // TeamCheckLock performs all necessary teamlock checking, and performs all
    // actions needed.
    TeamCheckLock();
    //--ZOID

    CheckMOTD();
    CheckRules();
    WaterMove();

    //JIM
    //WETSUIT
    doWetsuit();

    if(self.waterlevel == 2)
    {
        CheckWaterJump();
    }

    if(self.deadflag >= DEAD_DEAD)
    {
        PlayerDeathThink();
        return;
    }

    if(self.deadflag == DEAD_DYING)
        return;    // dying, so do nothing

    if(self.button2)
    {
        PlayerJump();
    }
    else
    {
        self.flags = self.flags | FL_JUMPRELEASED;
    }

    // teleporters can force a non-moving pause time
    if(time < self.pausetime)
    {
        self.velocity = '0 0 0';
    }

    // ROGUE
    if(VR_ItemUtil_EntHasItem(self, IID_ANTIGRAV))
    {
        if(time > self.AGping)
        {
            sound(self, CHAN_AUTO, "belt/use.wav", 0.4, ATTN_NORM);

            self.AGping = time + 3;

            if(self.AGping >= (self.antigrav_finished - 3))
            {
                self.AGping = self.antigrav_finished + 3;
            }
        }
    }

    // ROGUE
    RuneApplyElder(self); // regeneration rune

    // Track the grapple
    if(self.hook_out2)
    {
        GrappleService(cVR_OffHand);
    }

    if(self.hook_out)
    {
        GrappleService(cVR_MainHand);
    }
};

/*
================
CheckPowerups

Check for turning off powerups
================
*/
void() CheckPowerups =
{
    if(self.health <= 0)
        return;

// invisibility
    if(self.invisible_finished)
    {
// sound and screen flash when items starts to run out
        if(self.invisible_sound < time)
        {
            sound(self, CHAN_AUTO, "items/inv3.wav", 0.5, ATTN_IDLE);
            self.invisible_sound = time + ((random() * 3) + 1);
        }


        if(self.invisible_finished < time + 3)
        {
            if(self.invisible_time == 1)
            {
                sprint(self, "Ring of Shadows magic is fading\n");
                stuffcmd(self, "bf\n");
                sound(self, CHAN_AUTO, "items/inv2.wav", 1, ATTN_NORM);
                self.invisible_time = time + 1;
            }

            if(self.invisible_time < time)
            {
                self.invisible_time = time + 1;
                stuffcmd(self, "bf\n");
            }
        }

        if(self.invisible_finished < time)
        {    // just stopped
            VR_ItemUtil_EntDelItem(self, IID_INVISIBILITY);
            self.invisible_finished = 0;
            self.invisible_time = 0;
        }

    // use the eyes
        self.frame = 0;
        self.modelindex = modelindex_eyes;
    }
// TODO VR: (P2) maybe check for mission pack 1 support here
//MED 12/04/96 added mjolnir stuff
    else if(self.weapon == WID_MJOLNIR)
        self.modelindex = modelindex_hammer;   // don't use eyes
    else
        self.modelindex = modelindex_player;    // don't use eyes

// invincibility
    if(self.invincible_finished)
    {
// sound and screen flash when items starts to run out
        if(self.invincible_finished < time + 3)
        {
            if(self.invincible_time == 1)
            {
                sprint(self, "Protection is almost burned out\n");
                stuffcmd(self, "bf\n");
                sound(self, CHAN_AUTO, "items/protect2.wav", 1, ATTN_NORM);
                self.invincible_time = time + 1;
            }

            if(self.invincible_time < time)
            {
                self.invincible_time = time + 1;
                stuffcmd(self, "bf\n");
            }
        }

        if(self.invincible_finished < time)
        {    // just stopped
            VR_ItemUtil_EntDelItem(self, IID_INVULNERABILITY);
            self.invincible_time = 0;
            self.invincible_finished = 0;
        }
        if(self.invincible_finished > time)
            self.effects = self.effects | EF_DIMLIGHT;
        else
            self.effects = self.effects - (self.effects & EF_DIMLIGHT);
    }

// super damage
    if(self.super_damage_finished)
    {

// sound and screen flash when items starts to run out

        if(self.super_damage_finished < time + 3)
        {
            if(self.super_time == 1)
            {
                sprint(self, "Quad Damage is wearing off\n");
                stuffcmd(self, "bf\n");
                sound(self, CHAN_AUTO, "items/damage2.wav", 1, ATTN_NORM);
                self.super_time = time + 1;
            }

            if(self.super_time < time)
            {
                self.super_time = time + 1;
                stuffcmd(self, "bf\n");
            }
        }

        if(self.super_damage_finished < time)
        {    // just stopped
            VR_ItemUtil_EntDelItem(self, IID_QUAD);
            self.super_damage_finished = 0;
            self.super_time = 0;
        }
        if(self.super_damage_finished > time)
            self.effects = self.effects | EF_DIMLIGHT;
        else
            self.effects = self.effects - (self.effects & EF_DIMLIGHT);
    }

// suit
    if(self.radsuit_finished)
    {
        self.air_finished = time + 12;        // don't drown

// sound and screen flash when items starts to run out
        if(self.radsuit_finished < time + 3)
        {
            if(self.rad_time == 1)
            {
                sprint(self, "Air supply in Biosuit expiring\n");
                stuffcmd(self, "bf\n");
                sound(self, CHAN_AUTO, "items/suit2.wav", 1, ATTN_NORM);
                self.rad_time = time + 1;
            }

            if(self.rad_time < time)
            {
                self.rad_time = time + 1;
                stuffcmd(self, "bf\n");
            }
        }

        if(self.radsuit_finished < time)
        {    // just stopped
            VR_ItemUtil_EntDelItem(self, IID_SUIT);
            self.rad_time = 0;
            self.radsuit_finished = 0;
        }
    }

//JIM
// wetsuit
    if(self.wetsuit_finished)
    {
    self.air_finished = time + 12;        // don't drown

    // sound and screen flash when items starts to run out
    if(self.wetsuit_finished < time + 3)
        {
            if(self.wetsuit_time == 1)
            {
                sprint(self, "Air supply in Wetsuit is running out\n");
                stuffcmd(self, "bf\n");
                sound(self, CHAN_AUTO, "items/suit2.wav", 1, ATTN_NORM);
                self.wetsuit_time = time + 1;
            }

            if(self.wetsuit_time < time)
            {
                self.wetsuit_time = time + 1;
                stuffcmd(self, "bf\n");
            }
        }

        if(self.wetsuit_finished < time)
        {    // just stopped
            VR_ItemUtil_EntDelItem(self, IID_WETSUIT);
            self.wetsuit_time = 0;
            self.wetsuit_finished = 0;
        }
    }

//MED
// empathy shields
   if(self.empathy_finished)
    {
      // sound and screen flash when items starts to run out
      if(self.empathy_finished < time + 3)
        {
         if(self.empathy_time == 1)
            {
            sprint(self, "Empathy Shields are running out\n");
                stuffcmd(self, "bf\n");
                sound(self, CHAN_AUTO, "items/suit2.wav", 1, ATTN_NORM);
            self.empathy_time = time + 1;
            }

         if(self.empathy_time < time)
            {
            self.empathy_time = time + 1;
                stuffcmd(self, "bf\n");
            }
        }

      if(self.empathy_finished < time)
        {    // just stopped
         VR_ItemUtil_EntDelItem(self, IID_EMPATHY_SHIELDS);
         self.empathy_time = 0;
         self.empathy_finished = 0;
        }
//MED
      if(self.empathy_finished > time)
            self.effects = self.effects | EF_DIMLIGHT;
        else
            self.effects = self.effects - (self.effects & EF_DIMLIGHT);
   }

    if(self.shield_finished)
    {
        if(self.shield_finished < time + 3)
        {
            if(self.shield_time == 1)
            {
                sprint(self, "Shield failing...\n");
                sound(self, CHAN_AUTO, "shield/fadeout.wav", 1, ATTN_NORM);
                self.shield_time = 0;
            }

            if(self.shield_time < time)
            {
                self.shield_time = time + 1;
                stuffcmd (self, "bf\n");
            }
        }

        if(self.shield_finished < time)
        {
            sprint(self, "Shield Lost.\n");
            self.shield_finished = 0;

            VR_ItemUtil_EntDelItem(self, IID_SHIELD);
        }
    }

    if(self.antigrav_finished)
    {
        if(self.antigrav_finished < time + 3)
        {
            if(self.antigrav_time == 1)
            {
                sprint(self, "Antigrav failing...\n");
                self.antigrav_time = 0;
                sound(self, CHAN_AUTO, "belt/fadeout.wav", 1, ATTN_NORM);
            }

            if(self.antigrav_time < time)
            {
                self.antigrav_time = time + 1;
                stuffcmd (self, "bf\n");
            }
        }

        if(self.antigrav_finished < time)
        {
            sprint(self, "Antigrav Lost.\n");
            self.antigrav_finished = 0;
            VR_ItemUtil_EntDelItem(self, IID_ANTIGRAV);
            self.gravity = 1.0;
        }
    }
};


void() PlayerVRMelee;

/*
================
PlayerPostThink

Called every frame after physics are run
================
*/
void() PlayerPostThink =
{
    // FRIKBOT
    if(BotPostFrame())
        return;

    if(self.view_ofs == '0 0 0')
    {
       earthquake_postthink();
       return;     // intermission or finale
    }

    //JIM
    //WETSUIT
    if(self.wetsuit_finished > time)
    {
        if(self.waterlevel==2)
        {
            self.velocity = self.velocity * 0.8;
        }
        if(self.waterlevel==3)
        {
            self.velocity = self.velocity * 0.66;
        }
    }

    //JIM
    earthquake_postthink();

    if(self.deadflag)
        return;

    // ROGUE
    if(cutscene_running)
    {
        xpackEnding();
    }

    // do weapon stuff

    PlayerVRMelee();

    W_WeaponFrameImpl(cVR_MainHand);
    W_WeaponFrameImpl(cVR_OffHand);
    W_Frame();

// check to see if player landed and play landing sound
    if((self.jump_flag < -300) && (self.flags & FL_ONGROUND) && (self.health > 0))
    {
        if(self.watertype == CONTENT_WATER)
            sound(self, CHAN_BODY, "player/h2ojump.wav", 1, ATTN_NORM);
        else if(self.jump_flag < -650)
        {
            T_Damage(self, world, world, 5);
            sound(self, CHAN_VOICE, "player/land2.wav", 1, ATTN_NORM);
            self.deathtype = "falling";
        }
        else
            sound(self, CHAN_VOICE, "player/land.wav", 1, ATTN_NORM);

        self.jump_flag = 0;
    }

    if(!(self.flags & FL_ONGROUND))
        self.jump_flag = self.velocity_z;

    CheckPowerups();
};


/*
===========
ClientConnect

called when a player connects to a server
============
*/
void() ClientConnect =
{
    // FRIKBOT
    ClientInRankings();

    bprint(self.netname);
    bprint(" entered the game\n");

    //ZOID--
    self.suicide_count = 0;
    self.ctf_killed = 0;

    SetMOTD();

    // If this is our first connection, parm14 is < 0
    // Set lastteam negative.
    if(parm14 < 0 && teamplay > 0)
    {
        if(cvar_hget(cvarh_gamecfg) & GAMECFG_USE_COLOR)
        {
            self.steam = self.team; // accept joining players color if legal
        }
        else
        {
            self.steam = -50; // always reassign
        }

        TeamCheckLock();
    }
    //--ZOID

    // a client connecting during an intermission can cause problems
    if(intermission_running)
        ExitIntermission();
};


/*
===========
ClientDisconnect

called when a player disconnects from a server
============
*/
void() ClientDisconnect =
{
    // FRIKBOT
    ClientDisconnected();

    if(gameover)
        return;
    // if the level end trigger has been activated, just return
    // since they aren't *really* leaving

    // let everyone else know
    bprint(self.netname);
    bprint(" left the game with ");
    bprint(ftos(self.frags));
    bprint(" frags\n");
    sound(self, CHAN_BODY, "player/tornoff2.wav", 1, ATTN_NONE);
    set_suicide_frame();
};

/*
===========
ClientObituary

called when a player dies
============
*/
void(entity targ, entity attacker) ClientObituary =
{
    local float rnum;
    local string deathstring, deathstring2;
    rnum = random();

    if(targ.classname == "player")
    {
        // ZOID--
        TeamResetCarrier(targ);
        // --ZOID

        if(attacker.classname == "teledeath")
        {
            bprint(targ.netname);
            bprint(" was telefragged by ");
            bprint(attacker.owner.netname);
            bprint("\n");

            attacker.owner.frags = attacker.owner.frags + 1;
            return;
        }

        if(attacker.classname == "teledeath2")
        {
            bprint("Satan's power deflects ");
            bprint(targ.netname);
            bprint("'s telefrag\n");

            targ.frags = targ.frags - 1;
            return;
        }

        if(attacker.classname == "player")
        {
            if(targ == attacker)
            {
                // killed self
                attacker.frags = attacker.frags - 1;
                bprint(targ.netname);

                if(targ.weapon == WID_LIGHTNING && targ.waterlevel > 1)
                {
                    bprint(" discharges into the water.\n");
                    return;
                }

                if(targ.weapon == WID_GRENADE_LAUNCHER)
                {
                    bprint(" tries to put the pin back in\n");
                }
                // ROGUE
                //ZOID--
                //gibbed for changing teams
                else if(teamplay && (targ.team != targ.steam))
                {
                    if(cvar_hget(cvarh_gamecfg) & GAMECFG_ALLOW_CHG)
                    {
                        bprint(" changed teams\n");
                    }
                    else
                    {
                        bprint(" tried to change teams\n");
                    }
                }
                //--ZOID
//JIM
                else if(rnum > 0.4)
                    bprint(" becomes bored with life\n");
                else
                    bprint(" checks if his weapon is loaded\n");
                return;
            }
            else if((teamplay == 2) && (targ.team > 0)&&(targ.team == attacker.team))
            {
                if(rnum < 0.25)
                    deathstring = " mows down a teammate\n";
                else if(rnum < 0.50)
                    deathstring = " checks his glasses\n";
                else if(rnum < 0.75)
                    deathstring = " gets a frag for the other team\n";
                else
                    deathstring = " loses another friend\n";
                bprint(attacker.netname);
                bprint(deathstring);
                attacker.frags = attacker.frags - 1;
                return;
            }
            else
            {
                // ROGUE
                if(teamplay == TEAM_DMATCH_TAG)
                {
                    dmatch_score (targ, attacker);
                }
                else
                {
                    attacker.frags = attacker.frags + 1;
                    //ZOID--
                    TeamAssists(targ, attacker);
                    //--ZOID
                }

//MED 01/19/97
            if(empathyused == 1)
               {
               bprint(targ.netname);
               if(random()<0.5)
                  bprint(" shares ");
               else
                  bprint(" feels ");
               bprint(attacker.netname);
               bprint("'s pain\n");
               return;
               }

//MED 11/18/96
            if(targ.dmg_inflictor.classname == "proximity_grenade")
               {
               bprint(targ.netname);
               if(random()<0.5)
                  bprint(" got too friendly with ");
               else
                  bprint(" did the rhumba with ");
               bprint(attacker.netname);
               bprint("'s bomb\n");
               return;
               }
                rnum = attacker.weapon;
                if(rnum == WID_FIST)
                {
                    deathstring = " was punched by ";
                    deathstring2 = "\n";
                }
                else if(rnum == WID_AXE)
                {
                    deathstring = " was ax-murdered by ";
                    deathstring2 = "\n";
                }
                //ZOID--
                else if(rnum == WID_GRAPPLE)
                {
                    deathstring = " was grappled by ";
                    deathstring2 = "\n";
                }
                //--ZOID
                else if(rnum == WID_SHOTGUN)
                {
                    deathstring = " chewed on ";
                    deathstring2 = "'s boomstick\n";
                }
                else if(rnum == WID_SUPER_SHOTGUN)
                {
                    deathstring = " ate 2 loads of ";
                    deathstring2 = "'s buckshot\n";
                }
                else if(rnum == WID_NAILGUN)
                {
                    if(attacker.weaponflags & QVR_WPNFLAG_USE_SECONDARY_AMMO)
                    {
                        deathstring = " was burned by ";
                        deathstring2 = "\n";
                    }
                    else
                    {
                        deathstring = " was nailed by ";
                        deathstring2 = "\n";
                    }
                }
                else if(rnum == WID_SUPER_NAILGUN)
                {
                    if(attacker.weaponflags & QVR_WPNFLAG_USE_SECONDARY_AMMO)
                    {
                        deathstring = " was burned by ";
                        deathstring2 = "\n";
                    }
                    else
                    {
                        deathstring = " was punctured by ";
                        deathstring2 = "\n";
                    }
                }
                else if(rnum == WID_GRENADE_LAUNCHER)
                {
                    if(attacker.weaponflags & QVR_WPNFLAG_USE_SECONDARY_AMMO)
                    {
                        deathstring = " was blasted to bits by ";
                        deathstring2 = "\n";
                    }
                    else
                    {
                        deathstring = " eats ";
                        deathstring2 = "'s pineapple\n";
                    }

                    if(targ.health < -40)
                    {
                        deathstring = " was gibbed by ";
                        deathstring2 = "'s grenade\n";
                    }

                }
                else if(rnum == WID_ROCKET_LAUNCHER)
                {
                    if(attacker.weaponflags & QVR_WPNFLAG_USE_SECONDARY_AMMO)
                    {
                        deathstring = " was blasted to bits by ";
                        deathstring2 = "\n";
                    }
                    else
                    {
                        deathstring = " rides ";
                        deathstring2 = "'s rocket\n";
                    }

                    if(targ.health < -40)
                    {
                        deathstring = " was gibbed by ";
                        deathstring2 = "'s rocket\n" ;
                    }
                }
                else if(rnum == WID_LIGHTNING)
                {
                    if(attacker.weaponflags & QVR_WPNFLAG_USE_SECONDARY_AMMO)
                    {
                        deathstring = " was fused by ";
                        deathstring2 = "\n";
                    }
                    else
                    {
                        deathstring = " accepts ";
                        if(attacker.waterlevel > 1)
                            deathstring2 = "'s discharge\n";
                        else
                            deathstring2 = "'s shaft\n";
                    }
                }
    //MED
            else if(rnum == WID_LASER_CANNON)
                {
               if(random()<0.5)
                  {
                  deathstring = " was toasted by ";
                  }
               else
                  {
                  deathstring = " was radiated by ";
                  }
               deathstring2 = "'s laser\n";
                }
//MED
            if(rnum == WID_MJOLNIR)
                {
               deathstring = " was slammed by ";
               deathstring2 = "'s hammer\n";
                }
                bprint(targ.netname);
                bprint(deathstring);
                bprint(attacker.netname);
                bprint(deathstring2);
            }
            return;
        }
        else
        {
            targ.frags = targ.frags - 1;        // killed self
            rnum = targ.watertype;

            bprint(targ.netname);

//JIM
         if(attacker.deathtype)
            {
            bprint(" ");
            bprint(attacker.deathtype);
            bprint("\n");
            return;
            }

            // TODO VR: (P2) elses...

            if(attacker.flags & FL_MONSTER)
            {
                if(attacker.classname == "monster_army")
                    bprint(" was shot by a Grunt\n");
                if(attacker.classname == "monster_demon1")
                    bprint(" was eviscerated by a Fiend\n");
                if(attacker.classname == "monster_dog")
                    bprint(" was mauled by a Rottweiler\n");
                if(attacker.classname == "monster_dragon")
                    bprint(" was annihilated by the Dragon\n");
                if(attacker.classname == "monster_dragon_dead")
                    bprint(" was squashed by the Dragon\n");
                if(attacker.classname == "monster_enforcer")
                    bprint(" was blasted by an Enforcer\n");
                if(attacker.classname == "monster_fish")
                    bprint(" was fed to the Rotfish\n");
                if(attacker.classname == "monster_hell_knight")
                    bprint(" was slain by a Death Knight\n");
                if(attacker.classname == "monster_knight")
                    bprint(" was slashed by a Knight\n");
                if(attacker.classname == "monster_ogre")
                    bprint(" was destroyed by an Ogre\n");
                if(attacker.classname == "monster_oldone")
                    bprint(" became one with Shub-Niggurath\n");
                if(attacker.classname == "monster_shalrath")
                    bprint(" was exploded by a Vore\n");
                if(attacker.classname == "monster_shambler")
                    bprint(" was smashed by a Shambler\n");
                if(attacker.classname == "monster_tarbaby")
                    bprint(" was slimed by a Spawn\n");
                if(attacker.classname == "monster_vomit")
                    bprint(" was vomited on by a Vomitus\n");
                if(attacker.classname == "monster_wizard")
                    bprint(" was scragged by a Scrag\n");
                if(attacker.classname == "monster_zombie")
                    bprint(" joins the Zombies\n");

                // HIPNOTIC
                if(attacker.classname == "monster_gremlin")
                    bprint(" was outsmarted by a Gremlin\n");
                if(attacker.classname == "monster_scourge")
                    bprint(" was stung by a Centroid\n");
                if(attacker.classname == "monster_armagon")
                    bprint(" was outgunned by Armagon\n");

                // ROGUE
                if(attacker.classname == "monster_eel")
                    bprint(" was electrified by an Eel\n");
                if(attacker.classname == "monster_wrath")
                    bprint(" was disintegrated by a Wrath\n");
                if(attacker.classname == "monster_super_wrath")
                    bprint(" was obliterated by an Overlord\n");
                if(attacker.classname == "monster_sword")
                    bprint(" was slit open by a Phantom Swordsman\n");
                if(attacker.classname == "monster_lava_man")
                    bprint(" fries in Hephaestus' fury\n");
                if(attacker.classname == "monster_morph")
                    bprint(" was crushed by a Guardian\n");
                if(attacker.classname == "monster_mummy")
                    bprint(" was Mummified\n");

                return;
            }

            // tricks and traps
            if(attacker.classname == "explo_box")
            {
                bprint(" blew up\n");
                return;
            }
            if(attacker.solid == SOLID_BSP && attacker != world)
            {
                bprint(" was squished\n");
                return;
            }
            if(targ.deathtype == "falling")
            {
                targ.deathtype = "";
                bprint(" fell to his death\n");
                return;
            }
            if(attacker.classname == "trap_shooter" || attacker.classname == "trap_spikeshooter")
            {
                bprint(" was spiked\n");
                return;
            }
            if(attacker.classname == "fireball")
            {
                bprint(" ate a lavaball\n");
                return;
            }
            if(attacker.classname == "trigger_changelevel")
            {
                bprint(" tried to leave\n");
                return;
            }
            if(attacker.classname == "trigger_deathpit") // HONEY
            {
                if(attacker.message) {
                    bprint(attacker.message);
                } else {
                    bprint(" fell into a deep and dark crevasse");
                }

                bprint("\n");
                return;
            }

            // ROGUE
            if(attacker.classname == "ltrail_start" ||
                attacker.classname == "ltrail_relay")
            {
                bprint(" rode the lightning\n");
                return;
            }
            if(attacker.classname == "pendulum")
            {
                bprint(" was cleaved in two\n");
                return;
            }
            if(attacker.classname == "buzzsaw")
            {
                bprint(" was sliced to pieces\n");
                return;
            }
            if(attacker.classname == "plasma")
            {
                bprint(" was turned to plasma\n");
                return;
            }
            if(attacker.classname == "Vengeance")
            {
                // vengeance sphere doesn't count as a negative frag
                targ.frags = targ.frags + 1;
                bprint(" was purged by the Vengeance Sphere\n");
                return;
            }
            if(attacker.classname == "power_shield")
            {
                // shield attacks should count for frags too..
                // shield death doesn't count as a negative frag
                targ.frags = targ.frags + 1;
                attacker.owner.frags = attacker.owner.frags + 1;
                TeamAssists(targ, attacker.owner);            //PGM

                deathstring = " was smashed by ";
                deathstring2 = "\n";
                bprint(deathstring);
                bprint(attacker.owner.netname);
                bprint(deathstring2);
                return;
            }

            // in-water deaths
            rnum = targ.watertype;
            if(rnum == -3)
            {
                if(random() < 0.5)
                    bprint(" sleeps with the fishes\n");
                else
                    bprint(" sucks it down\n");
                return;
            }
            else if(rnum == -4)
            {
                if(random() < 0.5)
                    bprint(" gulped a load of slime\n");
                else
                    bprint(" can't exist on slime alone\n");
                return;
            }
            else if(rnum == -5)
            {
                if(targ.health < -15)
                {
                    bprint(" burst into flames\n");
                    return;
                }
                if(random() < 0.5)
                    bprint(" turned into hot slag\n");
                else
                    bprint(" visits the Volcano God\n");
                return;
            }

            // fell to their death?
            if(targ.deathtype == "falling")
            {
                targ.deathtype = "";
                bprint(" fell to his death\n");
                return;
            }

            // hell if I know; he's just dead!!!
            bprint(" died\n");
        }
    }
};

void(string xSound, void(float xHand) xWpnFn) DoMainHandMeleeImpl =
{
    if(time >= self.mainhand_melee_attack_finished)
    {
        sound(self, CHAN_WEAPON, xSound, 1, ATTN_NORM);
    }

    xWpnFn(cVR_MainHand);
}

float(float xWpnId, string xSound, void(float xHand) xWpnFn) DoMainHandMelee =
{
    if(self.weapon == xWpnId)
    {
        DoMainHandMeleeImpl(xSound, xWpnFn);
        return TRUE;
    }

    return FALSE;
}

void(string xSound, void(float xHand) xWpnFn) DoOffHandMeleeImpl =
{
    if(time >= self.offhand_melee_attack_finished)
    {
        sound(self, CHAN_WEAPON2, xSound, 1, ATTN_NORM);
    }

    xWpnFn(cVR_OffHand);
}

float(float xWpnId, string xSound, void(float xHand) xWpnFn) DoOffHandMelee =
{
    if(self.weapon2 == xWpnId)
    {
        DoOffHandMeleeImpl(xSound, xWpnFn);
        return TRUE;
    }

    return FALSE;
}

void(float xHand, string xSound, void(float xHand) xWpnFn) DoXHandMeleeImpl =
{
    if(xHand == cVR_MainHand) { DoMainHandMeleeImpl(xSound, xWpnFn); return; }
    if(xHand == cVR_OffHand)  { DoOffHandMeleeImpl(xSound, xWpnFn); return; }

    bprint("'DoXHandMeleeImpl': Invalid hand '");
    bprint(ftos(xHand));
    bprint("'");
}

float(float xHand, float xWpnId, string xSound, void(float xHand) xWpnFn) DoXHandMelee =
{
    if(xHand == cVR_MainHand) { return DoMainHandMelee(xWpnId, xSound, xWpnFn); }
    if(xHand == cVR_OffHand)  { return DoOffHandMelee(xWpnId, xSound, xWpnFn); }

    bprint("'DoXHandMelee': Invalid hand '");
    bprint(ftos(xHand));
    bprint("'");

    // Bogus, should never happen.
    return FALSE;
}

void(float xHand, float xInMelee, float xHandVelMag,
     float* xOutMeleeAttackFinished, float* xOutInMelee, float* xMeleeHitSoundPlayed)
     PlayerVRMeleeImpl =
{
    local float vr_melee_threshold;
    vr_melee_threshold = cvar_hget(cvarh_vr_melee_threshold);

    if(cvar_hget(cvarh_vr_enabled) == 1 && time >= xInMelee && xHandVelMag > vr_melee_threshold)
    {
        if(!DoXHandMelee(xHand, WID_FIST, "knight/sword1.wav", W_FistMelee))
            if(!DoXHandMelee(xHand, WID_AXE, "weapons/ax1.wav", W_FireAxe))
                if(!DoXHandMelee(xHand, WID_MJOLNIR, "weapons/ax1.wav", HIP_FireMjolnir))
                    DoXHandMeleeImpl(xHand, "knight/sword1.wav", W_GunMelee);

        *xOutMeleeAttackFinished = time + 0.1;
        *xOutInMelee = time + 0.02;
    }

    if(xHandVelMag <= vr_melee_threshold)
    {
        *xMeleeHitSoundPlayed = FALSE;
    }
}

void() PlayerVRMeleeHeadbutt =
{
    local float headvelLen;
    headvelLen = vlen(self.headvel);

    if(headvelLen >= cvar_hget(cvarh_vr_headbutt_velocity_threshold))
    {
        if(!self.in_headbutt)
        {
            self.headbutt_start_time = time;
            self.in_headbutt = TRUE;
            self.headbutt_max_vec = self.headvel;
            sound(self, CHAN_OTHER, "knight/sword1.wav", 1, ATTN_NORM);
        }
        else if(vlen(self.headbutt_max_vec) < headvelLen)
        {
            self.headbutt_max_vec = self.headvel;
        }
    }
    else if(self.in_headbutt)
    {
        self.in_headbutt = FALSE;

        local float xdmg;
        xdmg = (
            vlen(self.headbutt_max_vec) / cvar_hget(cvarh_vr_headbutt_velocity_threshold)
            ) * cvar_hget(cvarh_vr_headbutt_damage_mult);

        local vector dir;
        dir = normalize(self.headbutt_max_vec);

        local vector adjOrigin;
        adjOrigin = self.origin + '0 0 25';

        traceline(adjOrigin, adjOrigin + dir * VRMeleeRange(26), FALSE, self);

        if(trace_fraction != 1.0)
        {
            sound(self, CHAN_OTHER, "fisthit.wav", 1.0, ATTN_NORM);

            if(trace_ent.takedamage)
            {
                VRGunHaptic(cVR_OffHand, 0.15, 100, 1.0);
                VRGunHaptic(cVR_MainHand, 0.15, 100, 1.0);

                trace_ent.axhitme = 1;
                SpawnBloodSplash(trace_endpos, xdmg);
                T_Damage_VRMelee(trace_ent, self, self, xdmg);
            }
            else
            {
                // hit wall
                VRGunHaptic(cVR_OffHand, 0.15, 100, 1.0);
                VRGunHaptic(cVR_MainHand, 0.15, 100, 1.0);

                PainSound();
                SpawnBloodSplash(trace_endpos, xdmg);

                BroadcastGunshotEffect(trace_endpos);
            }
        }
    }
}

void() PlayerVRMelee =
{
    PlayerVRMeleeHeadbutt();

    PlayerVRMeleeImpl(
        cVR_MainHand,
        self.in_melee,
        self.handvelmag,
        &self.mainhand_melee_attack_finished,
        &self.in_melee,
        &self.melee_hit_sound_played
    );

    PlayerVRMeleeImpl(
        cVR_OffHand,
        self.offhand_in_melee,
        self.offhandvelmag,
        &self.offhand_melee_attack_finished,
        &self.offhand_in_melee,
        &self.offhand_melee_hit_sound_played
    );
}
