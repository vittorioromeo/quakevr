float modelindex_eyes, modelindex_player;

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
    MG3_RuneRemoved(); // Dawn of the Machine: a view only with so many runes (vr_mg3_defs.qc)
};

// Magnetic holsters (vr_magholster.qc): the bound weapons lying about home before the level ends; the bindings carried.
void() VR_MagHolster_EncodeParms;
void() VR_MagHolster_DecodeParms;

void(entity p) VR_Embed_LevelEnd; // (vr_embedhold.qc)

void() SetChangeParms =
{
    if(AD_SetChangeParms()) return; // AD support: AD's level state (parm57..72), an exit's inventory reset (vr_ad_bridge.qc)
    VR_Embed_LevelEnd(self); // (a blade held in a body: back in the hand, vr_embedhold.qc)
    MG3_EncodeParms(); // (Dawn of the Machine's upgrades: kept even by the SetNewParms below, vr_mg3_defs.qc)
    VR_MagHolster_EncodeParms(); // (parm73: the bound weapons home first, so the holsters' parms below have them)

    if(self.health <= 0)
    {
        SetNewParms();

        // ROGUE
        parm22 = self.steam; // Save the current team of the player

        return;
    }

    // The authored MG1 hub starts each episode with fresh VR equipment.
    if(MG1_Hub()) { SetNewParms(); return; }

    VR_HandGrenade_LevelEnd(); // an unarmed hand grenade in a hand: its rocket goes on with you (vr_grenade.qc)
    VR_Reload_LevelEnd(); // a round in a hand back into the reserve (vr_reload.qc)

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

    // The carried health: at most the player's capacity, at least half of it (Dopa/Machine Nightmare: 50/25; Dawn of the
    // Machine: its upgraded capacity; otherwise 100/50).
    float carryHealthCap = VR_MaxHealth(self);
    if(self.health > carryHealthCap)
    {
        self.health = carryHealthCap;
    }

    if(self.health < carryHealthCap * 0.5)
    {
        self.health = carryHealthCap * 0.5;
    }

    // ROGUE
    // Don't carry items between levels in CTF
    teamplay = cvar_hget(cvarh_teamplay);
    if(teamplay >= TEAM_CTF)
    {
        // As a dead player's: a new game's parms, the team kept (the weapons' flags, magazines and ids below are the old
        // weapons'; the next weapon id, parm49, is SetNewParms's; bullet time's meter and the plates start afresh).
        SetNewParms();
        parm22 = self.steam;
        return;
    }
    else
    {
        parm1 = self.items;
        parm2 = self.health;
        parm3 = self.armorvalue;

        if(self.ammo_shells < 25)
        {
            parm4 = 25;
        }
        else
        {
            parm4 = self.ammo_shells;
        }

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
    // The weapons' records (vr_weaponinst.qc): their magazines and their ids.
    parm31 = self.weaponinst.wi_clip;
    parm32 = self.weaponinst2.wi_clip;
    parm33 = self.holsterweaponinst0.wi_clip;
    parm34 = self.holsterweaponinst1.wi_clip;
    parm35 = self.holsterweaponinst2.wi_clip;
    parm36 = self.holsterweaponinst3.wi_clip;
    parm37 = self.holsterweaponinst4.wi_clip;
    parm38 = self.holsterweaponinst5.wi_clip;
    parm41 = self.weaponinst.wi_uid;
    parm42 = self.weaponinst2.wi_uid;
    parm43 = self.holsterweaponinst0.wi_uid;
    parm44 = self.holsterweaponinst1.wi_uid;
    parm45 = self.holsterweaponinst2.wi_uid;
    parm46 = self.holsterweaponinst3.wi_uid;
    parm47 = self.holsterweaponinst4.wi_uid;
    parm48 = self.holsterweaponinst5.wi_uid;
    parm49 = qvr_weaponinst_next;
    parm64 = AD_PackWeaponUpgrades(self); // AD support: the carried weapons' upgrades (vr_ad_bridge.qc)
    parm76 = DW_PackWeaponVariants(self); // Dwell support: the carried weapons' variants (dwell/dw_items.qc)
    parm77 = VR_Attach_Pack(self); // the carried weapons' attachments (vr_attach.qc)
    WeaponInst_PackHot(self); // (the blades still glowing: parm65..72)
    parm50 = self.vr_flashlight_state; // (bits 0..18: the engine's lamp; bit 19: a chainsaw's cord pulled)
    if(self.vr_saw_cord_pulled) parm50 = parm50 + VR_PARM50_SAW_CORD_PULLED;
    parm74 = self.vr_bullettime_spent; // (bullet time's meter, kept from level to level: vr_bullettime.cpp)
    parm75 = self.vr_pp_mask; // (the armour plates shown: which ones, kept from level to level: vr_playerplates.qc)
};

void() OnSpawnServerBeforeLoad =
{
    // float fromSaveFile;
    // fromSaveFile = spawnServerFromSaveFile;

    VR_CVars_InitAllHandles();
}

void() OnSpawnServerAfterLoad =
{
    VR_CVars_InitAllHandles();

    // (The map's ammo boxes roll their weapon at its first PutClientInServer, against that player: SpawnAllAmmoBoxWeapons.)
}

void() VR_Crackles_AfterLoad; // (vr_burning.qc)
void() VR_AM_AfterLoad;       // (vr_armedmon.qc)

void() OnLoadGame =
{
    VR_CVars_InitAllHandles();
    vr_ammobox_rolled_map = TRUE; // (the map's boxes rolled before the save; an older save's flag was not kept)
    VR_Skewer_ListRebuild(); // (the skewered gibs' list: an older save had none)
    VR_AM_AfterLoad(); // the armed monsters' row cache: the map's spawn filled it for another kind than the save's
    SUB_DoLater(VR_Crackles_AfterLoad, 0.1); // the torches' and fires' crackles started again (sounds are not saved)
    VR_Tips_Reapply(); // (TRIGGERED tips used before the save: shown again)
    AD_LoadedGame(); // AD support: the loaded game's Evil skill choice, an older save's upgrades (vr_ad_player.qc)
    VR_ADGun_LegacyWorld(); // AD support: an older save's eliminator's plasma guns the centurion's (vr_ad_enemyguns.qc)
    // The load's pause/loading screens often arrive with neither controller gripping. Protect only the weapons
    // restored in the hands, starting in game time (the server stays paused until the client is ready).
    entity p = VR_Player();
    while(p)
    {
        p.vr_weapon_load_until = time + max(0, cvar_hget(cvarh_vr_weapon_load_grace));
        p.vr_weapon_load_main = VRGetEntWeaponInst(p, cVR_MainHand);
        p.vr_weapon_load_off = VRGetEntWeaponInst(p, cVR_OffHand);
        // AD support: the AD weapons in its hands and holsters made and precached again (precached late in the saved
        // game; the hands' model fields name them, which the load's model rebinding doesn't cover: SV_ModelIndex fails)
        VR_ADGun_EnsureCarried(p);
        VR_ADMelee_EnsureCarried(p);
        WeaponInst_OwnedSync(p, FALSE); // the variants his weapons are his (an older save's: owned from them)
        p = VR_NextPlayer(p);
    }
}

float() VR_FlatReload_NewGameCycles; // (vr_flatreload.qc)

void() SetNewParms =
{
    // HONEY
    // The secret count doesn't get updated when using the restart command for
    // some reason.
    if(VR_GameUtil_EnableHoneyExtensions())
    {
        SUB_DoLater(Honey_UpdateSecrets, 0.2);
    }

    if(VR_GameUtil_InTutorialMap())
    {
        parm1 = 0;
    }
    else
    {
        parm1 = IT_SHOTGUN | IT_AXE;
    }

    parm2 = VR_BaseHealth();
    parm3 = 0;
    parm4 = mapname == "vrtutorial" ? 0 : 25; // (the tutorial: no shells until its range)
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

    if(VR_GameUtil_InTutorialMap())
    {
        parm13 = WID_FIST;
        parm14 = WID_FIST;
    }
    else
    {
        parm13 = WID_AXE;
        parm14 = WID_SHOTGUN;
    }

    parm15 = WID_FIST;
    parm16 = WID_FIST;

    // ------------------------------------------------------------------------
    // Mission packs:
    // HIPNOTIC
    parm17 = 0;

    // ROGUE
    if(deathmatch && teamplay >= TEAM_CTF) // (the grapple is Quake VR's own)
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
    parm31 = 0;
    parm32 = 0;
    parm33 = 0;
    parm34 = 0;
    parm35 = 0;
    parm36 = 0;
    parm37 = 0;
    parm38 = 0;
    // Immersive reloading (vr_reload.qc): the shotgun you start with (in the right hip holster) comes loaded from your
    // shells, as a map's pickup does: the holstered gun's own magazine (its record, parm36), drawn from the hip full, in
    // a headset and in flat-screen play with its holsters (vr_flat_holsters, the default); after that it is loaded by
    // hand from the ammo pouch (flat: R at the hip). Not flat-screen play's weapon cycle (its holsters off): 1 and 2 take a
    // gun into the hand's own record and load it from the reserve then (VRCycleEntWeaponInst, VR_FlatReload_CycleIn),
    // so rounds loaded here would stay in a holster never drawn from: a new game's 25 shells were 9 and 8 in the clip.
    // (Until 2026-11-10 that exception took in every flat-screen game, the holsters' too: the gun was drawn empty.)
    if(VR_Reload_ManualFor(parm14) && !VR_FlatReload_NewGameCycles())
    {
        parm36 = min(WeaponIdToBaseClipSize(parm14), parm4);
        parm4 = parm4 - parm36;
    }
    parm41 = 0; // (new weapons: new ids)
    parm42 = 0;
    parm43 = 0;
    parm44 = 0;
    parm45 = 0;
    parm46 = 0;
    parm47 = 0;
    parm48 = 0;
    parm49 = qvr_weaponinst_next;
    parm64 = 0; // (AD support: no upgrades)
    parm77 = 0; // (no attachments)
    parm65 = 0; parm66 = 0; parm67 = 0; parm68 = 0; parm69 = 0; parm70 = 0; parm71 = 0; parm72 = 0; // (cold)
    parm50 = 0;
    parm73 = 0; // (no magnetic holster bindings)
    parm74 = 0; // (bullet time's meter full)
    parm75 = 0; // (no armour plates)
    parm76 = 0; // (Dwell support: no variants)
    VR_Run_NewParms(); // the run's mutators: Starting Weapon, Melee Only's axe (vr_run.qc)
};

void() DecodeLevelParms =
{
    if(MGH_Active() && !self.MGH_reviving) SetNewParms();
    if(MG1_Hub()) SetNewParms();
    if(VR_GameUtil_InTutorialMap())
    {
        SetNewParms();
    }

    if(!deathmatch && !AD_Rules()) // AD support: AD's hub keeps the inventory (AD_DecodeLevelParms)
    {
        if(world.model == "maps/start.bsp")
            SetNewParms();        // take away all stuff on starting new episode
    }

    // HIPNOTIC
    if(world.model == "maps/start.bsp" && !AD_Rules()) // (AD support, as above)
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

    // Changing installations must not resurrect unavailable weapons or powerups from level parms.
    // (each item by whether its files are here: Quake VR's own weapons always are)
    parm17 = VR_Pack_KeepBit(parm17, HIP_IT_WETSUIT, IID_WETSUIT);
    parm17 = VR_Pack_KeepBit(parm17, HIP_IT_EMPATHY_SHIELDS, IID_EMPATHY_SHIELDS);
    parm17 = VR_Pack_KeepBit(parm17, HIP_IT_HORN_OF_CONJURING, IID_HORN_OF_CONJURING);
    parm18 = VR_Pack_KeepBit(parm18, ROGUE_IT_LAVA_NAILGUN, IID_LAVA_NAILGUN);
    parm18 = VR_Pack_KeepBit(parm18, ROGUE_IT_LAVA_SUPER_NAILGUN, IID_LAVA_SUPER_NAILGUN);
    parm18 = VR_Pack_KeepBit(parm18, ROGUE_IT_LAVA_NAILS, IID_LAVA_NAILS);
    parm18 = VR_Pack_KeepBit(parm18, ROGUE_IT_MULTI_ROCKET, IID_MULTI_ROCKET);
    parm18 = VR_Pack_KeepBit(parm18, ROGUE_IT_PLASMA_GUN, IID_PLASMA_GUN);
    parm18 = VR_Pack_KeepBit(parm18, ROGUE_IT_PLASMA_AMMO, IID_PLASMA_AMMO);
    parm18 = VR_Pack_KeepBit(parm18, ROGUE_IT_SHIELD, IID_SHIELD);
    parm18 = VR_Pack_KeepBit(parm18, ROGUE_IT_ANTIGRAV, IID_ANTIGRAV);
    parm18 = VR_Pack_KeepBit(parm18, ROGUE_IT_EARTHQUAKE, IID_EARTHQUAKE);
    parm18 = VR_Pack_KeepBit(parm18, ROGUE_IT_V_SPHERE, IID_V_SPHERE);
    if(!VR_Pack_AmmoAvailable(AID_LAVA_NAILS)) { parm19 = 0; }
    if(!VR_Pack_AmmoAvailable(AID_PLASMA)) { parm21 = 0; }
    if(!VR_Pack_WeaponAvailable(parm8)) { parm8 = WID_FIST; }
    parm23 = VR_Pack_WeaponFlags(parm8, parm23);
    if(!VR_Pack_WeaponAvailable(parm10)) { parm10 = WID_FIST; }
    parm24 = VR_Pack_WeaponFlags(parm10, parm24);
    if(!VR_Pack_WeaponAvailable(parm11)) { parm11 = WID_FIST; }
    parm25 = VR_Pack_WeaponFlags(parm11, parm25);
    if(!VR_Pack_WeaponAvailable(parm12)) { parm12 = WID_FIST; }
    parm26 = VR_Pack_WeaponFlags(parm12, parm26);
    if(!VR_Pack_WeaponAvailable(parm13)) { parm13 = WID_FIST; }
    parm27 = VR_Pack_WeaponFlags(parm13, parm27);
    if(!VR_Pack_WeaponAvailable(parm14)) { parm14 = WID_FIST; }
    parm28 = VR_Pack_WeaponFlags(parm14, parm28);
    if(!VR_Pack_WeaponAvailable(parm15)) { parm15 = WID_FIST; }
    parm29 = VR_Pack_WeaponFlags(parm15, parm29);
    if(!VR_Pack_WeaponAvailable(parm16)) { parm16 = WID_FIST; }
    parm30 = VR_Pack_WeaponFlags(parm16, parm30);

    // Dawn of the Machine: boss2 on skill 3 is Bloody Nightmare, whose levels start with the axe and shotgun only.
    MG3_BloodyNightmareLevel();
    MG3_BloodyNightmareStrip();
    VR_Trailer_StripParms(); // QVR: the trailer scene starts empty-handed (vr_trailer.qc)
    AD_DecodeLevelParms(); // AD support: AD's worldspawn inventory keys and map variables in an AD map (vr_ad_bridge.qc)

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
    // The weapons' records (vr_weaponinst.qc): their magazines, and their ids where free (a coop player respawning
    // with the level's start parms while his old weapon still lies about gets a new one).
    qvr_weaponinst_next = max(qvr_weaponinst_next, parm49);
    self.vr_saw_cord_pulled = parm50 >= VR_PARM50_SAW_CORD_PULLED;
    self.vr_flashlight_state = parm50 - (self.vr_saw_cord_pulled ? VR_PARM50_SAW_CORD_PULLED : 0);
    self.vr_bullettime_spent = parm74;
    self.vr_pp_mask = parm75;
    MG3_DecodeParms(); // (parm51..56: Dawn of the Machine's upgrades and bloody weapons, vr_mg3_defs.qc)
    self.max_health = VR_MaxHealth(self); // (with them: upstream sets it here)
    if(MG3_Campaign() && self.health > self.max_health) { self.health = self.max_health; }
    self.weaponinst = WeaponInst_Carried(self.weapon, parm31, parm41);
    self.weaponinst2 = WeaponInst_Carried(self.weapon2, parm32, parm42);
    self.holsterweaponinst0 = WeaponInst_Carried(self.holsterweapon0, parm33, parm43);
    self.holsterweaponinst1 = WeaponInst_Carried(self.holsterweapon1, parm34, parm44);
    self.holsterweaponinst2 = WeaponInst_Carried(self.holsterweapon2, parm35, parm45);
    self.holsterweaponinst3 = WeaponInst_Carried(self.holsterweapon3, parm36, parm46);
    self.holsterweaponinst4 = WeaponInst_Carried(self.holsterweapon4, parm37, parm47);
    self.holsterweaponinst5 = WeaponInst_Carried(self.holsterweapon5, parm38, parm48);
    AD_UnpackWeaponUpgrades(self, parm64); // AD support: their upgrades (vr_ad_bridge.qc)
    DW_UnpackWeaponVariants(self, parm76); // Dwell support: their variants (dwell/dw_items.qc)
    VR_Attach_Unpack(self, parm77); // their attachments (vr_attach.qc)
    WeaponInst_UnpackHot(self);
    VR_MagHolster_DecodeParms(); // (parm73: the bindings, to the carried weapons' records)
    VR_ADGun_EnsureCarried(self); // AD support: AD soldiers' guns carried here precached (vr_ad_enemyguns.qc)

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
    // (A gun carried by its foregrip is held normally in a new level: the client's pose is gone.)
    // (Nor a grappling hook's tag: its hook stayed in the last level.)
    // (Nor a chainsaw's running engine: it starts again with its cord.)
    // (A grapple's only: those bits are an AD sword's kind, vr_ad_guns_defs.qc.)
    float handOnly = QVR_WPNFLAG_FOREGRIP_CARRIED | QVR_WPNFLAG_SAW_STATE;
    self.weaponflags = VR_Grapple_StripTagOf(self.weapon, parm23 - (parm23 & handOnly));
    self.weaponflags2 = VR_Grapple_StripTagOf(self.weapon2, parm24 - (parm24 & handOnly));
    self.holsterweaponflags0 = VR_Grapple_StripTagOf(self.holsterweapon0, parm25);
    self.holsterweaponflags1 = VR_Grapple_StripTagOf(self.holsterweapon1, parm26);
    self.holsterweaponflags2 = VR_Grapple_StripTagOf(self.holsterweapon2, parm27);
    self.holsterweaponflags3 = VR_Grapple_StripTagOf(self.holsterweapon3, parm28);
    self.holsterweaponflags4 = VR_Grapple_StripTagOf(self.holsterweapon4, parm29);
    self.holsterweaponflags5 = VR_Grapple_StripTagOf(self.holsterweapon5, parm30);
    VR_ADMelee_EnsureCarried(self); // AD support: AD melee weapons carried here precached (vr_ad_melee.qc)
};

/*
============
FindIntermission

Returns the entity to view from
============
*/
entity() FindIntermission =
{
    entity spot;
    float cyc;

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
    {
        changelevel(mapname);
    }
    else
    {
        changelevel(MGH_Active() ? MGH_NextArena() : nextmap);
    }
};


void() ExitIntermission =
{
    if(MGH_Active()) { GotoNextMap(); return; }
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
        if(MG_WorldCampaign() && (strlen(MG_intermissiontext) > 0 || strlen(world.endtext) > 0)) {
            WriteByte(MSG_ALL, SVC_CDTRACK); WriteByte(MSG_ALL, 2); WriteByte(MSG_ALL, 3);
            WriteByte(MSG_ALL, SVC_FINALE);
            WriteString(MSG_ALL, strlen(MG_intermissiontext) > 0 ? MG_intermissiontext : world.endtext);
            MG_intermissiontext = "";
            return;
        }
        if(MG_WorldCampaign() && nextmap == "start" && !coop) { localcmd("disconnect\nmenu_credits\n"); return; }
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
            WriteByte(MSG_ALL, SVC_FINALE);
            WriteString(MSG_ALL, "Victory! The Overlord's mangled\nremains are the evidence.  His evil\nWrath army?  Cast out to wander\naimlessly throughout time.\n\nAs the Slipgate fog surrounds you,\nthoughts rage into your consciousness:\nHas Quake's oppressive reign ended?\nIs it Salvation, or Damnation, which\nwaits beyond the Vortex?\n\nAnother thought, not quite your own,\nrazors through the haze.  \"Forgiveness\ncan yet be granted; Our Master remains\nto absolve your sins against his Chosen.\nFall down upon your knees-pray for\nQuake's mercy.\"");
            return;
        }

        GotoNextMap();
    }

    if(intermission_running == 3)
    {
        if(MG_WorldCampaign() && nextmap == "start" && !coop) { localcmd("disconnect\nmenu_credits\n"); return; }
        if(!cvar_hget(cvarh_registered))
        {    // shareware episode has been completed, go to sell screen
            WriteByte(MSG_ALL, SVC_SELLSCREEN);
            return;
        }

        // (Dawn of the Machine's serverflags are its own runes and Bloody Nightmare bits: no id1 finale.)
        if(!MG3_Campaign() && (serverflags & (MG1_Campaign() ? 31 : 15)) == (MG1_Campaign() ? 31 : 15))
        {
            WriteByte(MSG_ALL, SVC_FINALE);
            WriteString(MSG_ALL, MG1_Campaign() ? "$qc_mg1_endtext_all_runes" : "Now, you have all four Runes. You sense\ntremendous invisible forces moving to\nunseal ancient barriers. Shub-Niggurath\nhad hoped to use the Runes Herself to\nclear off the Earth, but now instead,\nyou will use them to enter her home and\nconfront her as an avatar of avenging\nEarth-life. If you defeat her, you will\nbe remembered forever as the savior of\nthe planet. If she conquers, it will be\nas if you had never been born.");
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

float foundHoneyFogSettings;

void() execute_changelevel =
{
    // HONEY
    foundHoneyFogSettings = FALSE;
    entity fogsettings = NULLENT;

    intermission_running = 1;

    // enforce a wait time before allowing changelevel
    if(deathmatch)
        intermission_exittime = time + 5;
    else
        intermission_exittime = time + 2;

    WriteByte(MSG_ALL, SVC_CDTRACK);
    WriteByte(MSG_ALL, 3);
    WriteByte(MSG_ALL, 3);

    entity pos = FindIntermission();

    // HONEY
    if(pos.target != "") {
        fogsettings = find(world, targetname, pos.target);
        if(fogsettings.classname == "trigger_fogcontroller") {
            foundHoneyFogSettings = TRUE;
            fogsettings = fogsettings.owner;
        }
    }

    other = VR_Player();
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

        if(MG_WorldCampaign()) VR_Fog_PushFrom(other, pos, 0);

        // HONEY
        if(fogsettings != NULLENT && fogsettings.classname == "trigger_fogcontroller") {
            fog_setOverrideConditional(other, fogsettings.view_ofs, fogsettings.ideal_yaw, 1);
        }

        other = VR_NextPlayer(other);
    }

    WriteByte(MSG_ALL, SVC_INTERMISSION);
};


void() changelevel_touch =
{
    if(other.classname != "player")
        return;
    if(!AD_ChangelevelTouch()) return; // AD support: AD's exits off until triggered, its on-exit rules (vr_ad_bridge.qc)
    if(VR_Run_HubAsks(self)) return; // the hub's teleporter: the Run Setup screen first (vr_run.qc)

    float noexit;
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
    // VR: the first start's calibration room leads to the tutorial (on Easy) until it has been started once
    // (vr_tutorial_started: vr_tutorial.qc; the engine's startGameCommands does the same at a start)
    if(mapname == "vrcalibration" && nextmap == "vrstart" && !deathmatch && !coop && !cvar_hget(cvarh_vr_tutorial_started))
    {
        nextmap = "vrtutorial";
        cvar_set("skill", "0");
    }
    // Dawn of the Machine: the new game Bloody Nightmare starts leaves the hub for Shub (boss2), not secret2.
    if(MG3_Campaign() && mapname == "hub" && nextmap == "secret2"
        && (serverflags & MG3_SF_BN_NEWGAME) && (serverflags & MG3_SF_BN_ACTIVE))
    {
        nextmap = "boss2";
    }
    if(MG_WorldCampaign()) MG_intermissiontext = self.endtext;
    // Dimension of the Machine finished: its runes cleared, as upstream (the hub's final gate closed again: a coop game,
    // which keeps serverflags, would otherwise start with it open).
    if(MG1_Campaign() && world.model == "maps/mgend.bsp")
    {
        serverflags = 0;
    }

    SUB_UseTargets();

    if((self.spawnflags & 1) && (deathmatch == 0) && (!MG_WorldCampaign() || strlen(self.endtext) == 0))
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
    AD_ChangelevelSpawn(); // AD support: AD's exit keys in an AD map (vr_ad_bridge.qc)
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
nosave float vr_restart_queued; // VR: respawn's "restart" asked for (a new server starts it at 0 again)
// VR: the death menu (the engine's: menu.c M_CheckDeathMenu, M_Autosaves_Key): set by the engine while it holds the
// respawn; a press while held (the engine opens the menu); its Respawn chosen (PlayerDeathThink respawns)
nosave float vr_death_menu_hold;
nosave float vr_death_menu_asked;
nosave float vr_death_menu_respawn;

void() respawn =
{
    // VR: a map with checkpoints (vrtutorial: info_vr_checkpoint) brings him back at the latest, the level as it was
    if(VR_Checkpoint_Respawn())
    {
        return;
    }
    // VR: the run's Ironman: the level from its start, with what he entered it with (vr_run_ironman.qc)
    if(VR_Run_Respawn())
    {
        return;
    }

    if(coop)
    {
        // make a copy of the dead body for appearances sake
        // Unless the player died from falling in a pit!
        if(!self.enemy && !VR_DeathDoll_Has(self)) // HONEY; VR: his ragdoll body is there already
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
        if(!self.enemy && !VR_DeathDoll_Has(self)) // HONEY; VR: his ragdoll body is there already
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
        // VR: once: PlayerDeathThink asks again every frame the button is held until the restart runs, and "Loading..."
        // puts it off for frames (each restart loaded the last save again: ROUND22.md, "Respawn loaded the map several
        // times"); MG1 Horde's MGH_restart_queued likewise
        if(vr_restart_queued)
            return;
        vr_restart_queued = TRUE;
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
    if(MGH_Active()) { self.health=0; PlayerDie(); return; }
    if((intermission_running)&&((coop)||(deathmatch)))  // not allowed during intermission
        return;

    entity vSphere;

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

/*
============
SelectSpawnPoint

Returns the entity to spawn at
============
*/
// VR (B13, MP_PLAN): a deathmatch start a living player (not `self`) stands within 64 units of: a spawn there would
// telefrag him (spawn_tdeath's box reaches 49 units out on each axis). Stock Quake counted any "player" within 32 -- the
// dead ones too, and not one 33-49 units away, whom the spawn then telefragged: with 16 players on a map's 6 starts the
// bots' instant respawns telefragged each other hundreds of times a second (mp_soak), every death dropping weapons until
// the server ran out of edicts.
float(vector org) VR_DMPlaceBlocked =
{
    entity thing = findradius(org, 64);
    while(thing)
    {
        if(thing.classname == "player" && thing != self && thing.health > 0 && thing.deadflag == DEAD_NO)
            return TRUE;
        thing = thing.chain;
    }
    return FALSE;
};

float(entity spot) VR_DMSpotBlocked =
{
    return VR_DMPlaceBlocked(spot.origin);
};

.float vr_spawn_wait; // VR (B13): a deathmatch respawn waiting for a clear start: the time it gives up waiting (0: none)
float VR_DM_SPAWN_WAIT = 2;

// VR (B13b, MP_PLAN): every deathmatch start taken by a living player, a place beside one: up to 144 units out round it
// (8 ways, its own facing first; 48, 96, 144 units), the player's box clear of the level and of solid things there and
// all the way from the start (the same room, never through a wall), a floor under its feet within 40 units (not a ledge),
// no liquid at its head, middle or feet, and no living player within 64 units (VR_DMPlaceBlocked). With 16 players on
// dm3's few starts the bots standing at them were telefragged by the respawns ~100 times in 10 s (mp_soak); the respawn
// waited up to 2 s, then telefragged as stock Quake did. Its place in vr_dm_beside_at.
vector vr_dm_beside_at;
entity vr_dm_beside; // the spot a respawn beside a start takes (classname "vr_dm_beside": made again on each map)
float(entity spot) VR_DMSpotBeside =
{
    for(float r = 48; r <= 144; r = r + 48)
    {
        for(float a = 0; a < 360; a = a + 45)
        {
            makevectors('0 1 0' * (spot.angles_y + a));
            vector p = spot.origin + v_forward * r;
            tracebox(spot.origin, VEC_HULL_MIN, VEC_HULL_MAX, p, TRUE, self); // (the level: a player on the start is no wall)
            if(trace_fraction < 1 || trace_startsolid || trace_allsolid)
                continue;
            tracebox(p, VEC_HULL_MIN, VEC_HULL_MAX, p, FALSE, self); // (and nothing solid there)
            if(trace_startsolid || trace_allsolid)
                continue;
            traceline(p, p - '0 0 64', TRUE, self);
            if(trace_fraction == 1 || p_z - trace_endpos_z > 24 + 40)
                continue;
            if(pointcontents(p + '0 0 22') != CONTENT_EMPTY || pointcontents(p) != CONTENT_EMPTY ||
               pointcontents(p - '0 0 23') != CONTENT_EMPTY)
                continue;
            if(VR_DMPlaceBlocked(p))
                continue;
            vr_dm_beside_at = p;
            return TRUE;
        }
    }
    return FALSE;
};

// VR (B13b): the spot a respawn beside `spot` takes: vr_dm_beside at vr_dm_beside_at (VR_DMSpotBeside's), facing as `spot`.
entity(entity spot) VR_BesideSpotTake =
{
    if(!vr_dm_beside || vr_dm_beside.classname != "vr_dm_beside")
    {
        vr_dm_beside = spawn();
        vr_dm_beside.classname = "vr_dm_beside";
    }
    vr_dm_beside.origin = vr_dm_beside_at;
    vr_dm_beside.angles = spot.angles;
    return vr_dm_beside;
};

// VR (B13b): the first deathmatch start (from `from` on, round) with a clear place beside it (VR_DMSpotBeside), as the spot
// a respawn takes (vr_dm_beside, facing as the start); world if none.
entity(entity from) VR_DMBesideSpot =
{
    if(from.classname != "info_player_deathmatch")
        from = world;
    entity spot = from;
    for(float k = 0; k < 64; k++)
    {
        spot = find(spot, classname, "info_player_deathmatch");
        if(!spot)
        {
            spot = find(world, classname, "info_player_deathmatch");
            if(!spot)
                return world;
        }
        if(VR_DMSpotBeside(spot))
        {
            dprint(sprintf("dm spawn: every start taken; beside one at %s (%.0f units from it)\n", vtos(vr_dm_beside_at),
                vlen(vr_dm_beside_at - spot.origin)));
            return VR_BesideSpotTake(spot);
        }
        if(spot == from)
            return world;
        if(!from)
            from = spot; // (from the first: once round)
    }
    return world;
};

// VR (B13): whether a deathmatch start is clear now (VR_DMSpotBlocked), or (B13b) a place beside one; CTF's team starts
// are not counted (no wait there).
float() VR_DMSpotClear =
{
    if(teamplay >= TEAM_CTF)
        return TRUE;
    entity spot = find(world, classname, "info_player_deathmatch");
    if(!spot)
        return TRUE;
    while(spot)
    {
        if(!VR_DMSpotBlocked(spot))
            return TRUE;
        spot = find(spot, classname, "info_player_deathmatch");
    }
    return VR_DMBesideSpot(lastspawn) != world;
};

// VR (B25b, MP_PLAN): the coop start after `from` as stock Quake cycled them (the info_player_coop after it, else the
// info_player_start), and VR_CoopSpotRound's step: every coop start then the player start, round, from any of them.
entity(entity from) VR_CoopSpotNext =
{
    entity s = find(from, classname, "info_player_coop");
    if(!s)
        s = find(world, classname, "info_player_start");
    return s;
};

entity(entity from) VR_CoopSpotRound =
{
    entity s = world;
    if(from.classname == "info_player_coop")
        s = find(from, classname, "info_player_coop");
    else if(from.classname != "info_player_start")
        s = find(world, classname, "info_player_coop");
    if(!s && from.classname == "info_player_coop")
        s = find(world, classname, "info_player_start");
    if(!s)
        s = find(world, classname, "info_player_coop");
    if(!s)
        s = find(world, classname, "info_player_start");
    return s;
};

// VR (B25b, MP_PLAN): a coop spawn's start. Stock Quake took the next start in turn, a player standing on it or not, and
// the spawn telefragged him: a player joining a coop game in progress (or back from a crash) killed the one at the next
// start (mp_rejoin: "C was telefragged by B" once another join had moved the turn on). The next start in turn if no
// living player is within 64 units of it (VR_DMPlaceBlocked, deathmatch's rule), else the next clear one round, else a
// clear place beside one (VR_DMSpotBeside), else the next in turn as stock Quake (the telefrag). world: no coop start.
entity() VR_CoopSpawnPoint =
{
    entity first = VR_CoopSpotNext(lastspawn);
    if(!first)
        return world;
    entity s = first;
    for(float k = 0; k < 64; k++)
    {
        if(!VR_DMSpotBlocked(s))
        {
            lastspawn = s;
            return s;
        }
        s = VR_CoopSpotRound(s);
        if(!s || s == first)
            break;
    }
    lastspawn = first;
    s = first;
    for(float j = 0; j < 64; j++)
    {
        if(VR_DMSpotBeside(s))
        {
            dprint(sprintf("coop spawn: every start taken; beside one at %s\n", vtos(vr_dm_beside_at)));
            return VR_BesideSpotTake(s);
        }
        s = VR_CoopSpotRound(s);
        if(!s || s == first)
            break;
    }
    return first;
};

entity() SelectSpawnPoint =
{
    entity spot;
    if(MGH_Active()) { spot=MGH_SelectSpawn(); if(spot) return spot; }
    spot = AD_SelectSpawnPoint(); if(spot) return spot; // AD support: AD's spawn points in an AD map (vr_ad_bridge.qc)
    entity thing;
    float  pcount;

    //ZOID--
    entity startspot;
    float  t;
    //--ZOID


// testinfo_player_start is only found in regioned levels
    spot = find(world, classname, "testplayerstart");
    if(spot)
        return spot;

    // VR: single player back at the latest checkpoint taken (vr_tutorial.qc), if any
    if(!coop && !deathmatch)
    {
        spot = VR_Checkpoint_Latest();
        if(spot)
            return spot;
    }

// choose a info_player_deathmatch point
    if(coop)
    {
        if(MG_WorldCampaign()) {
            entity first = find(lastspawn, classname, "info_player_coop");
            if(!first) first = find(world, classname, "info_player_coop");
            entity c = first;
            if(c) do {
                if(c.state == 73) { lastspawn = c; return c; }
                c = find(c, classname, "info_player_coop");
                if(!c) c = find(world, classname, "info_player_coop");
            } while(c && c != first);
            return find(world, classname, "info_player_start");
        }
        spot = VR_CoopSpawnPoint();
        if(spot != world)
            return spot;
        lastspawn = world;
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

        // VR (mpbugs3): the scan ends after two passes past the last start: a map with no deathmatch start (Dwell's start,
        // any single player map) looped here for ever ("runaway loop error" from Select Campaign in a multiplayer game,
        // his note 2026-11-21), and so did every start blocked with no last spawn yet (startspot world: never met again).
        float wraps = 0;
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
                    // VR (B13b): every start taken: a place beside one, if any (not CTF's team starts)
                    if(t == 0)
                    {
                        entity beside = VR_DMBesideSpot(lastspawn);
                        if(beside)
                            return beside;
                    }
                    return startspot;
                }

                pcount = VR_DMSpotBlocked(spot); // VR (B13): a living player near it (stock: any player within 32)

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
                if(t == 0)
                {
                    wraps = wraps + 1;
                }
                if(wraps >= 2)
                {
                    spot = find(world, classname, "info_player_deathmatch");
                    if(!spot)
                    {
                        // The coop starts and the player start, as coop picks them (the next clear one, else beside
                        // one): not the player start for all, each spawn telefragging the one on it.
                        dprint("SelectSpawnPoint: deathmatch, no info_player_deathmatch: coop's starts\n");
                        spot = VR_CoopSpawnPoint();
                        if(spot)
                            return spot;
                        break; // (the player start below, as single player)
                    }
                    entity besideAll = VR_DMBesideSpot(spot); // every start taken: a place beside one, else the first
                    if(besideAll)
                        return besideAll;
                    lastspawn = spot;
                    return spot;
                }
                // pgm fix for ctf in levels w/o ctf support
                t = 0;
            }
        }
    }

    if(MG1_Campaign()) {
        spot = MG1_ReturnSpawn();
        if(spot) return spot;
    }

    if(serverflags && !MG3_Campaign()) // (Dawn of the Machine has no rune start spots: its serverflags mean other things)
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
void() VR_Toolgun_SpawnInHolster; // (vr_toolgun.qc)

void() PutClientInServer =
{
    entity spot;

    VR_Head_StopFlies(self); // QVR: flies round a gibbed player's head don't follow it back to life
    self.vr_deathdoll_body = world; // QVR: alive again, his ragdoll body only a body (vr_deathdoll.qc); the others see him
    self.vr_spawn_wait = 0; // QVR: (B13) no deathmatch respawn waiting

    self.classname = "player";
    self.health = 100;
    self.takedamage = DAMAGE_AIM;
    self.solid = SOLID_SLIDEBOX;
    self.movetype = MOVETYPE_WALK;
    self.show_hostile = 0;
    self.max_health = VR_MaxHealth(self); // (again in DecodeLevelParms, once the level's upgrades are known)
    self.flags = FL_CLIENT;
    self.air_finished = time + VR_AirSupply();
    self.dmg = 2;           // initial water damage
    self.super_damage_finished = 0;
    self.radsuit_finished = 0;
    self.lavasuit_finished = 0;
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

    VR_ItemUtil_EntClearItems(self);
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

    if(MG_WorldCampaign())
    {
        self.fog_active_fog_trigger = world;
        if(!VR_Fog_PushFrom(self, spot, 0)) VR_Fog_PushFrom(self, world, 0);
    }
    self.origin = self.oldorigin = spot.origin + '0 0 1';    // 1998-07-21 Respawning where player died fix by Robert Field
    self.angles = spot.angles;
    self.fixangle = TRUE;        // turn this way immediately

    self.touch = player_touch;

    // oh, this is a hack!

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


    self.handpos = self.origin;
    self.offhandpos = self.origin;
    // VR: the muzzles and loading ports with them, until the first move places them (left at the world's origin, the
    // engine's line from each hand to its muzzle crossed the map on the first frames, pressing a button on the way:
    // ROUND21.md, "A far button pressed at a map load").
    self.muzzlepos = self.origin;
    self.offmuzzlepos = self.origin;
    self.loadportpos = self.origin;
    self.offloadportpos = self.origin;
    self.magboxx = self.magboxy = self.magboxz = '0 0 0'; // (no magazine, until a move says)
    self.offmagboxx = self.offmagboxy = self.offmagboxz = '0 0 0';
    self.ammopouchx = self.ammopouchy = self.ammopouchz = '0 0 0'; // (no ammo pouch, until a move says)

    self.handvel = '0 0 0';
    self.offhandvel = '0 0 0';

    self.handthrowvel = '0 0 0';
    self.offhandthrowvel = '0 0 0';

    self.handavel = '0 0 0';
    self.offhandavel = '0 0 0';

    self.headvel = '0 0 0';

    VRSetTeleporting(self, FALSE);
    VRSetOffHandGrabbingBit(self, FALSE);
    VRSetOffHandPrevGrabbingBit(self, FALSE);
    VRSetMainHandGrabbingBit(self, FALSE);
    VRSetMainHandPrevGrabbingBit(self, FALSE);
    VRSet2HAiming(self, FALSE);
    VRSetOffHandReloadFlickingBit(self, FALSE);
    VRSetOffHandPrevReloadFlickingBit(self, FALSE);
    VRSetMainHandReloadFlickingBit(self, FALSE);
    VRSetMainHandPrevReloadFlickingBit(self, FALSE);

    VR_Forcegrab_Reset(self);

    spawn_tdeath(self.origin, self);

    if(deathmatch)
    {
        self.vr_readytime = time;
    }
    else
    {
        self.vr_readytime = time + 0.5;
    }
    if(MGH_Active()) { MGH_horde_set_keys(self); MGH_ResetHunger(self); }
    AD_PlayerSpawned(); // AD support: what AD's code reads of a player, in an AD map (vr_ad_bridge.qc)
    AD_UpgradesToWeapons(self); // AD support: an older game's upgrades (the player's) onto his weapons (vr_ad_bridge.qc)
    VR_Toolgun_SpawnInHolster(); // vr_debug_spawn_toolgun (vr_toolgun.qc)
    SpawnAllAmmoBoxWeapons(self);
    CU_PlayerSpawnedAt(spot); // Copper's rules: its spawn point's targets (vr_copper_bridge.qc)
};

// The map's first PutClientInServer (`p`, the first player in coop; not a loaded game's: none): each ammo box's weapon
// rolled against what he carries (DecodeLevelParms has run), once. A box not placed yet rolls at its placement.
void(entity p) SpawnAllAmmoBoxWeapons =
{
    if(vr_ammobox_rolled_map)
    {
        return;
    }
    vr_ammobox_rolled_map = TRUE;
    if(deathmatch || VR_GameUtil_InTutorialMap())
    {
        return;
    }
    for(entity e = nextent(world); e != world; e = nextent(e))
    {
        if(IsAmmoBox(e) && !e.vr_ammobox_rolls && e.think != PlaceItem)
        {
            VR_AmmoBox_Roll(e, p);
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
    if(MG_WorldCampaign()) {
        if(!coop) { remove(self); return; }
        if(!self.targetname || (self.spawnflags & 1)) self.state = 73;
    }
};

/*
===============================================================================

RULES

===============================================================================
*/

/*
go to the next level for deathmatch
*/
void() NextLevel =
{
    entity o;

    if(mapname == "start" && !MG3_Campaign()) // (id1's episode cycle by rune bits; MG3's start has its own exit)
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
    float        timelimit;
    float        fraglimit;

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
    float forward;

    if((self.flags & FL_ONGROUND))
    {
        forward = vlen(self.velocity);
        forward = forward - 20;
        if(forward <= 0)
            self.velocity = '0 0 0';
        else
            self.velocity = forward * normalize(self.velocity);
    }

    // VR: the death menu's Respawn (menu.c M_Autosaves_Key): as a press did
    if(vr_death_menu_respawn)
    {
        vr_death_menu_respawn = FALSE;
        self.button0 = 0;
        self.button1 = 0;
        self.button2 = 0;
        if(MGH_Active()) { MGH_DeathThink(); return; }
        respawn();
        return;
    }

// wait for all buttons released
    if(self.deadflag == DEAD_DEAD)
    {
        if(self.button2 || self.button1 || self.button0)
            return;
        self.deadflag = DEAD_RESPAWNABLE;
        return;
    }

// wait for any button down (VR: B13, or a respawn already asked for, waiting for a clear start: below)
    if(!self.button2 && !self.button1 && !self.button0 && !self.vr_spawn_wait)
        return;

    self.button0 = 0;
    self.button1 = 0;
    self.button2 = 0;
    // VR: single player with the death menu (vr_death_menu): a press opens it (the engine: menu.c M_CheckDeathMenu), and
    // only its choices respawn (author's note 2026-10-22); released and pressed again to ask again
    if(vr_death_menu_hold)
    {
        vr_death_menu_asked = TRUE;
        self.deadflag = DEAD_DEAD;
        return;
    }
    // MG1 Horde (official client.qc): the same release-then-press gate, then a team wipe restarts the arena.
    if(MGH_Active()) { MGH_DeathThink(); return; }
    // VR (B13): in deathmatch, every start with a living player at it: he waits for one to clear (his next press tries
    // again), at most VR_DM_SPAWN_WAIT seconds from his first try; then he spawns as stock Quake did (a telefrag)
    if(deathmatch && !VR_DMSpotClear())
    {
        if(!self.vr_spawn_wait)
            self.vr_spawn_wait = time + VR_DM_SPAWN_WAIT;
        if(time < self.vr_spawn_wait)
            return;
    }
    self.vr_spawn_wait = 0;
    respawn();
};


void() PlayerJump =
{
    if(self.flags & FL_WATERJUMP)
        return;

    if(self.waterlevel >= 2)
    {
        if(self.vr_movejump)
        {
            // (a remote client's moves rise in each, the engine's: MP_PLAN B8e; the sound is ours)
        }
        else if(self.watertype == CONTENT_WATER)
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

    if(self.vr_prone) return; // lying down (the engine's vr_prone: his box is low, he crawls): no jumping, boots or not

    if(AD_JumpBoots()) return; // AD support: AD's Jump Boots' jumps, in the air too (vr_ad_items.qc)

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

// VR: a splash where the player goes in (vr_water_splash; the sound is Quake's, above), as hard as
// the fall or the run into it.
void() VR_PlayerWaterSplash =
{
    local vector top, at, flat;
    local float hard;

    top = self.origin + '0 0 1' * (self.maxs_z + 48);
    at = liquidentry(top, self.origin + '0 0 1' * (self.mins_z - 8));
    if(at == top)
        return;

    flat = self.velocity;
    flat_z = 0;
    hard = self.velocity_z * -0.045 + vlen(flat) * 0.015;
    if(hard < 5)
        hard = 5;
    if(hard > 50)
        hard = 50;
    watersplash(at, normalize(self.velocity), hard, QVR_SPLASH_SILENT);
};

void() WaterMove =
{
//dprint(ftos(self.waterlevel));
    if(self.movetype == MOVETYPE_NOCLIP)
        return;
    if(self.health < 0)
        return;
    if(VR_Liquid_IsWet(self.watertype))
        VR_Status_Soak(self, self.waterlevel); // VR: wet (the status component, vr_status.qc)

    if(self.waterlevel != 3)
    {
        if(self.air_finished < time)
            sound(self, CHAN_VOICE, "player/gasp2.wav", 1, ATTN_NORM);
        else if(self.air_finished < time + VR_AirSupply() * 0.75) // (a quarter of the breath used: id's 3 of 12 s)
            sound(self, CHAN_VOICE, "player/gasp1.wav", 1, ATTN_NORM);
        self.air_finished = time + VR_AirSupply();
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
        if(self.dmgtime < time && self.lavasuit_finished <= time && // (the lava suit: none, MG3)
           !AD_LavaShield(self)) // AD support: nor with AD's Lava Shield (vr_ad_items.qc)
        {
            if(self.radsuit_finished > time)
                self.dmgtime = time + 1;
            else
                self.dmgtime = time + VR_Liquid_BurnEvery(CONTENT_LAVA); // VR: the liquids' one table (vr_liquids.qc)

            T_Damage(self, world, world, VR_Liquid_BurnDamage(CONTENT_LAVA, self.waterlevel));
        }
    }
    else if(self.watertype == CONTENT_SLIME)
    {    // do damage
        if(self.dmgtime < time && self.radsuit_finished < time && self.lavasuit_finished < time) // (the lava suit too)
        {
            self.dmgtime = time + VR_Liquid_BurnEvery(CONTENT_SLIME);
            T_Damage(self, world, world, VR_Liquid_BurnDamage(CONTENT_SLIME, self.waterlevel));
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
        VR_PlayerWaterSplash();

        self.flags = self.flags | FL_INWATER;
        self.dmgtime = 0;
    }

    if(!(self.flags & FL_WATERJUMP) && !self.vr_moveswim) // (a remote client's moves drag in each: MP_PLAN B8e)
        self.velocity = self.velocity - 0.8*self.waterlevel*frametime*self.velocity;
};

// Quake's jump out of the water, along the level `xDir`: solid at the waist within 24 units ahead, open at eye
// level: the player is lifted out (FL_WATERJUMP; SV_WaterJump carries him over the edge). TRUE if he was.
float(vector xDir) WaterJumpTry =
{
    vector start, end;

    start = self.origin;
    start_z = start_z + 8;
    end = start + xDir*24;
    traceline(start, end, TRUE, self);
    if(trace_fraction < 1)
    {    // solid at waist
        start_z = start_z + self.maxs_z - 8;
        end = start + xDir*24;
        self.movedir = trace_plane_normal * -50;
        traceline(start, end, TRUE, self);
        if(trace_fraction == 1)
        {    // open at eye level
            self.flags = self.flags | FL_WATERJUMP;
            self.velocity_z = 225;
            self.flags = self.flags - (self.flags & FL_JUMPRELEASED);
            self.teleport_time = time + 2;    // safety net
            dprint(sprintf("waterjump: out of the water at %v\n", self.origin));
            return TRUE;
        }
    }
    return FALSE;
};

// Climbing out of the water by hand (vr_water_jump 0, vr_climb on): an empty hand gripping on the edge, the
// ground right under it, pulled down (0.3 m/s or faster) lifts the player out as Quake's jump out would, over the
// edge towards that hand, high enough to clear the top under it. A bank no ledge of the ledge map is on (a slope, a
// step: vr_ledges.cpp) climbs so; a ledge's hold takes the climbing's own way (vr_climb.cpp: hang, pull up, mantle).
// The engine's (vr_pmove.cpp waterClimbTry: one function with a remote client's moves, MP_PLAN B8j part 3): the hands
// that may climb (empty: bit 0 the off hand, 1 the main) are QuakeC's.
float() VR_WaterClimb_FreeHands =
{
    return (VR_Hand_Free(self, cVR_OffHand, VR_HANDFREE_BUSY | VR_HANDFREE_LOCKED) ? 1 : 0) |
           (VR_Hand_Free(self, cVR_MainHand, VR_HANDFREE_BUSY | VR_HANDFREE_LOCKED) ? 2 : 0);
};

// ... the engine's climb lifted self out by hand `xHand`: the top under it `xTop` over the feet, rising at `xRise`.
void(float xHand, float xTop, float xRise) VR_WaterClimb_Climbed =
{
    dprint(sprintf("waterjump: climbed out by hand %g, the top %.1f above the feet, rising at %.0f\n", xHand, xTop, xRise));
    VRItemPickupHaptic(self);
};

// Off (vr_water_jump 0), a player climbs out with the hands: only while climbing is on (else there would be no way
// out of a pool whose sides are higher than a stroke lifts him). A flat-screen player has no hands to climb with:
// Quake's own jump out lifts him, whatever the setting (docs/vr-port/FLATPLAY.md, "Swimming"). TRUE: the hands'.
float() VR_WaterJumpByHand =
{
    return self.ishuman && !VR_FlatPlay(self) && !cvar_hget(cvarh_vr_water_jump) && cvar_hget(cvarh_vr_climb);
};

void() CheckWaterJump =
{
// check for a jump-out-of-water
    makevectors(self.angles);
    v_forward_z = 0;
    if(VR_WaterJumpByHand())
    {
        if(!self.vr_movewaterclimb) // (a remote client's moves try it, each: MP_PLAN B8j part 3)
        {
            float grips = (VR_HandGrabUtil_IsHandGrabbing(self, cVR_OffHand) ? 1 : 0) |
                          (VR_HandGrabUtil_IsHandGrabbing(self, cVR_MainHand) ? 2 : 0);
            waterclimb(grips & VR_WaterClimb_FreeHands());
        }
        return;
    }
    if(self.vr_movewaterjump)
    {
        return; // (a remote client's moves try it, each: MP_PLAN B8f)
    }
    WaterJumpTry(v_forward);
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

    VR_Hand_Derive(self); // QVR: the engine's busy kinds into the hand slot, before anything reads it (vr_carry.qc)

    // HONEY
    fog_updateAll();
    AD_PlayerPreThink(); // AD support: AD's ladders in an AD map (vr_ad_bridge.qc)

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
    // (a remote client's moves run Quake's jump out each, his prediction with them: MP_PLAN B8f)
    self.vr_movewaterjump = (self.vr_moveswim && !VR_WaterJumpByHand());
    // (... or the hands' climb out: bit 0, the empty hands in bits 1-2; MP_PLAN B8j part 3)
    self.vr_movewaterclimb = 0;
    if(self.vr_moveswim && VR_WaterJumpByHand() && (self.vr_movetakes & 1))
    {
        self.vr_movewaterclimb = 1 + 2 * VR_WaterClimb_FreeHands();
    }

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

    if(self.vr_movejump && (self.ad_moditems & ad_IT_ARTJUMPBOOTS))
    {
        self.vr_movejump = FALSE; // (AD's boots jump in the air too: QuakeC's, once a frame)
    }
    if(self.vr_movejump)
    {
        // A remote client's walk runs move by move (MP_PLAN B8d): the engine jumps as PlayerJump does, in the move whose
        // key pressed it (vr_pmove.cpp), so his own prediction jumps with it; the key's release is its too. Swimming,
        // the rise is the engine's too: PlayerJump plays its sound.
        if(self.button2 && self.waterlevel >= 2)
            PlayerJump();
    }
    else if(self.button2)
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
    if(self.rogue_items & ROGUE_IT_ANTIGRAV) // (VR_ItemUtil_EntHasItem, directly: every frame)
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

    // The grappling hooks of the guns in the hands and the holsters (vr_grapple.qc)
    VR_Grapple_PlayerFrame();
    VR_NailPistol_PlayerFrame(); // its trigger re-armed, its slide, its smoke (vr_nailpistol.qc)
    VR_FlareGun_PlayerFrame(); // its trigger re-armed, its muzzles' smoke (vr_flaregun.qc)
    VR_Revolver_PlayerFrame(); // its hammer, cylinder and chambers, its loading by hand (vr_revolver.qc)

    // Slid by a grunt's, an enforcer's or a vore's shove (vr_enemyshove.qc)
    VR_EnemyShove_PlayerFrame();
};

/*
================
CheckPowerups

Check for turning off powerups
================
*/
// CheckPowerups' steps for a timed powerup whose end is `xFinished` and whose warning clock is `xTime` (1: not warned yet).
// Its last 3 seconds: the message, a flash and the sound once, then a flash a second.
void(.float xFinished, .float xTime, string xMsg, string xSound) VR_Powerup_Warn =
{
    if(self.xFinished < time + 3)
    {
        if(self.xTime == 1)
        {
            sprint(self, xMsg);
            stuffcmd(self, "bf\n");
            sound(self, CHAN_AUTO, xSound, 1, ATTN_NORM);
            self.xTime = time + 1;
        }

        if(self.xTime < time)
        {
            self.xTime = time + 1;
            stuffcmd(self, "bf\n");
        }
    }
};

// Its end: the item `xItem` gone, both clocks cleared.
void(.float xFinished, .float xTime, float xItem) VR_Powerup_Expire =
{
    if(self.xFinished < time)
    {    // just stopped
        VR_ItemUtil_EntDelItem(self, xItem);
        self.xFinished = 0;
        self.xTime = 0;
    }
};

// The glow of the ones that glow, while they last.
void(.float xFinished) VR_Powerup_Glow =
{
    if(self.xFinished > time)
        self.effects = self.effects | EF_DIMLIGHT;
    else
        self.effects = self.effects - (self.effects & EF_DIMLIGHT);
};

void() CheckPowerups =
{
    if(self.health <= 0)
        return;
    DW_PowerupFrame(); // Dwell's Haste, Berserk, Wetsuit (dw_items.qc)

// invisibility
    if(self.invisible_finished)
    {
// sound and screen flash when items starts to run out
        if(self.invisible_sound < time)
        {
            sound(self, CHAN_AUTO, "items/inv3.wav", 0.5, ATTN_IDLE);
            self.invisible_sound = time + ((random() * 3) + 1);
        }

        VR_Powerup_Warn(invisible_finished, invisible_time, "Ring of Shadows magic is fading\n", "items/inv2.wav");
        VR_Powerup_Expire(invisible_finished, invisible_time, IID_INVISIBILITY);

    // use the eyes
        self.frame = 0;
        self.modelindex = modelindex_eyes;
    }
    else
        self.modelindex = modelindex_player;    // don't use eyes

// invincibility
    if(self.invincible_finished)
    {
        VR_Powerup_Warn(invincible_finished, invincible_time, "Protection is almost burned out\n", "items/protect2.wav");
        VR_Powerup_Expire(invincible_finished, invincible_time, IID_INVULNERABILITY);
        VR_Powerup_Glow(invincible_finished);
    }

// super damage
    if(self.super_damage_finished)
    {
        VR_Powerup_Warn(super_damage_finished, super_time, "Quad Damage is wearing off\n", "items/damage2.wav");
        VR_Powerup_Expire(super_damage_finished, super_time, IID_QUAD);
        VR_Powerup_Glow(super_damage_finished);
    }

// suit
    if(self.radsuit_finished)
    {
        self.air_finished = time + VR_AirSupply();        // don't drown
        VR_Powerup_Warn(radsuit_finished, rad_time, "Air supply in Biosuit expiring\n", "items/suit2.wav");
        VR_Powerup_Expire(radsuit_finished, rad_time, IID_SUIT);
    }

// lava suit (Dawn of the Machine, vr_mg3_items.qc)
    if(self.lavasuit_finished)
    {
        self.air_finished = time + VR_AirSupply();        // don't drown
        VR_Powerup_Warn(lavasuit_finished, lavasuit_time, "$mg3_qc_lavasuit_wearing_out", "items/suit2.wav");
        if(self.lavasuit_finished < time)
        {    // just stopped (the biosuit's item stays while a biosuit lasts)
            if(self.radsuit_finished < time) VR_ItemUtil_EntDelItem(self, IID_SUIT);
            self.lavasuit_time = 0;
            self.lavasuit_finished = 0;
        }
    }

//JIM
// wetsuit
    if(self.wetsuit_finished)
    {
        self.air_finished = time + VR_AirSupply();        // don't drown
        VR_Powerup_Warn(wetsuit_finished, wetsuit_time, "Air supply in Wetsuit is running out\n", "items/suit2.wav");
        VR_Powerup_Expire(wetsuit_finished, wetsuit_time, IID_WETSUIT);
    }

//MED
// empathy shields
    if(self.empathy_finished)
    {
        VR_Powerup_Warn(empathy_finished, empathy_time, "Empathy Shields are running out\n", "items/suit2.wav");
        VR_Powerup_Expire(empathy_finished, empathy_time, IID_EMPATHY_SHIELDS);
        VR_Powerup_Glow(empathy_finished);
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
void(entity pl) VR_PDG_Frame; // (vr_playerdisarm_grab.qc)
void(float xHand, void(float) xFn) VR_PropFire_With; // (vr_propfire.qc)
void() PlayerPostThink =
{
    VR_PP_Frame(self); // QVR: the armour's plates follow what is left of it (vr_playerplates.qc; bots too)
    VR_PD_Frame(self); // QVR: shots and projectiles on your weapons: knocked out? (vr_playerdisarm.qc)
    VR_PDG_Frame(self); // QVR: another player's weapon grabbed and pulled out of his hand (vr_playerdisarm_grab.qc)

    // FRIKBOT
    if(BotPostFrame())
        return;

    MG3_LorePostThink(); // QVR: a trigger_lore's text, gone a while after leaving it (vr_mg3_triggers.qc)

    if(self.view_ofs == '0 0 0')
    {
       earthquake_postthink();
       VR_Chainsaw_Silence(); // QVR: its engine's loop stops
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
    {
        VR_Carry_Dead(); // QVR: what the hands carried drops
        VR_Chainsaw_Silence(); // QVR: its engine's loop stops
        return;
    }

    VR_Cheat_AmmoFrame(); // QVR: Debug > Cheats and Recording's Infinite Ammo (vr_cheat_ammo; vr_cheats.qc)

    // ROGUE
    if(cutscene_running)
    {
        xpackEnding();
    }

    // do weapon stuff

    VR_HandGrabUtil_UpdateState(self);
    VR_Hand_Frame(self); // QVR: the hand slot's check (vr_carry.qc)

    PlayerVRMelee();

    W_WeaponFrameImpl(cVR_MainHand);
    W_WeaponFrameImpl(cVR_OffHand);
    W_Frame();
    VR_Chainsaw_Frame(cVR_MainHand); // QVR: the chainsaws' engines, chains and cords (vr_chainsaw.qc)
    VR_Chainsaw_Frame(cVR_OffHand);
    VR_PropFire_With(cVR_MainHand, VR_EnemyGuns_Frame); // QVR: the grunts' burst rifles' bursts (vr_enemyguns.qc), on a gun
    VR_PropFire_With(cVR_OffHand, VR_EnemyGuns_Frame);  // stuck in a body that the hand drives too (vr_propfire.qc)
    VR_ShotImpacts(); // QVR: projectiles fired from inside what they hit meet it (weapons.qc)

// check to see if player landed and play landing sound
    // QVR: landing on a living monster crushes the part under your feet (vr_ragimpact.qc): on the floor (a ragdoll), or
    // stopped short in the air on a standing one's top (Quake never sets FL_ONGROUND on a monster). Cushioned: no fall
    // damage (vr_ragimpact_crush_cushion). Slower landings than Quake's count too (a jump from the same level, about 270
    // units/s): down to the crush's or the stomp's least speed, the lower (a soft landing's jump_flag zeroed below).
    float crushCushion = FALSE;
    float crushGate = max(100, min(cvar_hget(cvarh_vr_ragimpact_crush_speed), cvar_hget(cvarh_vr_ragimpact_stomp_speed)));
    if((self.jump_flag <= -crushGate) && (self.health > 0) &&
       ((self.flags & FL_ONGROUND) || (self.velocity_z > (self.jump_flag * 0.5))))
    {
        float onFloor = (self.flags & FL_ONGROUND) ? TRUE : FALSE;
        if(VR_RagImpact_Crush(self, -self.jump_flag, onFloor))
        {
            crushCushion = cvar_hget(cvarh_vr_ragimpact_crush_cushion) != 0;
        }
    }
    // QVR: landing on small gibs pops them (vr_smallgibs.qc; from vr_smallgibs_stomp_speed).
    if(self.jump_flag < 0 && (self.flags & FL_ONGROUND) && self.health > 0)
    {
        VR_SmallGib_Stomp(self, -self.jump_flag);
    }
    if(AD_PlayerLanded()) {} // AD support: AD's fall rules in an AD map (vr_ad_bridge.qc)
    else if((self.jump_flag < -300) && (self.flags & FL_ONGROUND) && (self.health > 0))
    {
        if(self.watertype == CONTENT_WATER)
            sound(self, CHAN_BODY, "player/h2ojump.wav", 1, ATTN_NORM);
        else if(self.jump_flag < -650 && !crushCushion)
        {
            T_Damage(self, world, world, 5);
            sound(self, CHAN_VOICE, "player/land2.wav", 1, ATTN_NORM);
            self.deathtype = "falling";
        }
        else
            sound(self, CHAN_VOICE, "player/land.wav", 1, ATTN_NORM);
        if(self.watertype != CONTENT_WATER && time - self.stl_climb_time > 0.5) // QVR: a hard landing's thud is heard (vr_stealth.qc)
            VR_Stealth_Noise(self.origin, min(500, -self.jump_flag * 0.6), self, STL_N_STEP);

        self.jump_flag = 0;
    }
    if((self.flags & FL_ONGROUND) && self.jump_flag < 0) // QVR: a soft landing counted once (the crush's gate, above)
        self.jump_flag = 0;

    if(!(self.flags & FL_ONGROUND))
        self.jump_flag = self.velocity_z;

    CheckPowerups();
    WeaponInst_OwnedSync(self, TRUE); // his weapons as the variants he owns (every path does it: one missed, counted)
    VR_Attach_Frame(self); // an attachment brought to a gun snaps on; one pulled off (vr_attach.qc)
    AD_PlayerPostThink(); // AD support: AD's first frame, spawn targets and debuffs in an AD map (vr_ad_bridge.qc)
    VR_RN_PlayerFrame(); // the ruleset's one megahealth rot a player (Copper's: vr_rules_numbers.qc)
    VR_Grapple_GunFrames(); // QVR: a grappling gun whose hook is out drawn empty (vr_grapple.qc)
    VR_Stealth_PlayerFrame(); // QVR: his running heard, his touch felt, how lit he is (vr_stealth.qc)
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

    // MG1 Horde (official client.qc): a leaving player no longer counts as alive for waves, targets or revival.
    if(MGH_Active()) self.health = 0;

    // What he carried is let go (as a dead player's, PlayerPostThink): no frame of his runs again to do it, and a thing
    // left with its carry_player set hung in the air, taken by nobody (a key so held ended the level for the others).
    VR_Carry_Dead();

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
// ClientObituary's tables (W20). A monster's kill: "<player><obit>" by the killer's classname (the ones whose words depend on
// more than that are in ClientObituary). The classnames are distinct, so the order is free.
string VR_OBIT_MONSTERS[] =
{
    "monster_army", " was shot by a Grunt\n",
    "monster_demon1", " was eviscerated by a Fiend\n",
    "monster_dragon", " was annihilated by the Dragon\n",
    "monster_dragon_dead", " was squashed by the Dragon\n",
    "monster_enforcer", " was blasted by an Enforcer\n",
    "monster_fish", " was fed to the Rotfish\n",
    "monster_hell_knight", " was slain by a Death Knight\n",
    "monster_ranged_knight", " was slain by a Death Knight\n", // (QVR: MG3's)
    "monster_knight", " was slashed by a Knight\n",
    "monster_shambler", " was smashed by a Shambler\n",
    "monster_vomit", " was vomited on by a Vomitus\n",
    "monster_wizard", " was scragged by a Scrag\n",
    "monster_zombie", " joins the Zombies\n",
    // HIPNOTIC
    "monster_gremlin", " was outsmarted by a Gremlin\n",
    "monster_scourge", " was stung by a Centroid\n",
    "monster_armagon", " was outgunned by Armagon\n",
    // ROGUE
    "monster_eel", " was electrified by an Eel\n",
    "monster_wrath", " was disintegrated by a Wrath\n",
    "monster_super_wrath", " was obliterated by an Overlord\n",
    "monster_sword", " was slit open by a Phantom Swordsman\n",
    // Dawn of the Machine
    "monster_super_shambler", " met the blood shambler\n", // ($mg3_qc_obit_supershambler)
    "monster_orb", " was blasted by an Orb\n", // (upstream has none)
    "monster_morph", " was crushed by a Guardian\n",
    "monster_mummy", " was Mummified\n"
};
#define VR_OBIT_MONSTERS_N 48 // (strings: pairs)

// A trap's or a hazard's kill (after the crushers and the falls): "<player><obit>" by the killer's classname.
string VR_OBIT_TRAPS[] =
{
    "trap_shooter", " was spiked\n",
    "trap_spikeshooter", " was spiked\n",
    "fireball", " ate a lavaball\n",
    "trigger_changelevel", " tried to leave\n",
    // ROGUE
    "ltrail_start", " rode the lightning\n",
    "ltrail_relay", " rode the lightning\n",
    "pendulum", " was cleaved in two\n",
    "buzzsaw", " was sliced to pieces\n",
    "plasma", " was turned to plasma\n"
};
#define VR_OBIT_TRAPS_N 18

// A player's kill by another: "<targ><a><attacker><b>" by the killer's weapon; a2, b2 with its secondary ammo ("": the
// same). Its other words (a gib, the water, the laser's two) are ClientObituary's. A weapon not here (Quake VR's and
// the mods' own: the sword, the grunt's gun, the revolver, AD's...): "<targ> was killed by <attacker>'s <weapon name>"
// (split12: it said both names alone, no line end: "playerplayer 2" ran into the next print).
typedef struct
{
    float wid;
    string a, b, a2, b2;
} vr_obit_weapon;

vr_obit_weapon VR_OBIT_WEAPONS[] =
{
    { WID_FIST, " was punched by ", "\n", "", "" },
    { WID_AXE, " was ax-murdered by ", "\n", "", "" },
    { WID_GRAPPLE, " was grappled by ", "\n", "", "" }, // ZOID
    { WID_SHOTGUN, " chewed on ", "'s boomstick\n", "", "" },
    { WID_SUPER_SHOTGUN, " ate 2 loads of ", "'s buckshot\n", "", "" },
    { WID_NAILGUN, " was nailed by ", "\n", " was burned by ", "\n" },
    { WID_SUPER_NAILGUN, " was punctured by ", "\n", " was burned by ", "\n" },
    { WID_GRENADE_LAUNCHER, " eats ", "'s pineapple\n", " was blasted to bits by ", "\n" },
    { WID_ROCKET_LAUNCHER, " rides ", "'s rocket\n", " was blasted to bits by ", "\n" },
    { WID_LIGHTNING, " accepts ", "'s shaft\n", " was fused by ", "\n" },
    { WID_LASER_CANNON, " was toasted by ", "'s laser\n", "", "" }, // MED
    { WID_MJOLNIR, " was slammed by ", "'s hammer\n", "", "" } // MED
};
#define VR_OBIT_WEAPONS_N 12
void(entity targ, entity attacker) ClientObituary =
{
    float rnum;
    string deathstring = "";
    string deathstring2 = "";
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
                float sec = attacker.weaponflags & QVR_WPNFLAG_USE_SECONDARY_AMMO;
                for(float i = 0; i < VR_OBIT_WEAPONS_N; i++)
                {
                    vr_obit_weapon ow = VR_OBIT_WEAPONS[i];
                    if(ow.wid == rnum)
                    {
                        deathstring = (sec && ow.a2 != "") ? ow.a2 : ow.a;
                        deathstring2 = (sec && ow.a2 != "") ? ow.b2 : ow.b;
                        break;
                    }
                }
                if((rnum == WID_GRENADE_LAUNCHER || rnum == WID_ROCKET_LAUNCHER) && targ.health < -40)
                {
                    deathstring = " was gibbed by ";
                    deathstring2 = rnum == WID_GRENADE_LAUNCHER ? "'s grenade\n" : "'s rocket\n";
                }
                else if(rnum == WID_LIGHTNING && !sec && attacker.waterlevel > 1)
                {
                    deathstring2 = "'s discharge\n";
                }
                else if(rnum == WID_LASER_CANNON && random() >= 0.5) //MED
                {
                    deathstring = " was radiated by ";
                }
                if(deathstring == "") // QVR: a weapon not in VR_OBIT_WEAPONS
                {
                    bprint(targ.netname);
                    bprint(" was killed by ");
                    bprint(attacker.netname);
                    string wname = VR_WROW_OK(rnum) ? WeaponNameOf(rnum, attacker.weaponflags) : "";
                    if(wname != "")
                    {
                        bprint("'s ");
                        bprint(wname);
                    }
                    bprint("\n");
                    return;
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
         if(attacker.deathtype != NULLSTR)
            {
            bprint(" ");
            bprint(attacker.deathtype);
            bprint("\n");
            return;
            }

            if(attacker.flags & FL_MONSTER)
            {
                for(float m = 0; m < VR_OBIT_MONSTERS_N; m += 2)
                {
                    if(attacker.classname == VR_OBIT_MONSTERS[m])
                    {
                        bprint(VR_OBIT_MONSTERS[m + 1]);
                    }
                }
                if(attacker.classname == "monster_dog")
                    bprint(attacker.vr_mg3_mon ? " was blown up by a demo dog\n" : " was mauled by a Rottweiler\n"); // (QVR: MG3's demo dog)
                if(attacker.classname == "monster_ogre")
                    bprint(attacker.vr_mg3_mon ? " rode a rocket to hell\n" : " was destroyed by an Ogre\n"); // (QVR: MG3's rocket ogre)
                if(attacker.classname == "monster_oldone" || MG3_Shub_Is(attacker) || MG3_Shub_IsChild(attacker)) // (QVR: MG3's and her eyes)
                    bprint(" became one with Shub-Niggurath\n");
                if(attacker.classname == "monster_shalrath")
                    bprint(attacker.vr_eshove_striking ? " was swatted by a Vore\n" : " was exploded by a Vore\n"); // (QVR: its shove)
                if(attacker.classname == "monster_tarbaby")
                {
                    if(attacker.mg3_slimekin)
                        bprint(" was slimed\n"); // (Dawn of the Machine's slime: $mg3_qc_obit_slime)
                    else
                        bprint(" was slimed by a Spawn\n");
                }
                if(attacker.classname == "monster_lava_man") // ROGUE
                {
                    if(attacker.mg3_lavaman)
                        bprint(" was burned to a crisp\n"); // (Dawn of the Machine's: $mg3_qc_obit_lavaman)
                    else
                        bprint(" fries in Hephaestus' fury\n");
                }

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
            for(float t = 0; t < VR_OBIT_TRAPS_N; t += 2)
            {
                if(attacker.classname == VR_OBIT_TRAPS[t])
                {
                    bprint(VR_OBIT_TRAPS[t + 1]);
                    return;
                }
            }
            if(attacker.classname == "trigger_deathpit") // HONEY
            {
                if(attacker.message != NULLSTR) {
                    bprint(attacker.message);
                } else {
                    bprint(" fell into a deep and dark crevasse");
                }

                bprint("\n");
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
    if(xSound != "" && time >= self.mainhand_melee_attack_finished)
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
    if(xSound != "" && time >= self.offhand_melee_attack_finished)
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

float(float xHand) VR_PoleHeld; // (vr_poleweapon.qc: the Crystal Lance, the spear)
void(float xHand) VR_PoleMelee;
float(float xHand, entity b, entity g, vector xAt, vector xDir, float xSpeed, float xKind) VR_Skewer_Blow; // (vr_skewer.qc)
void(float xHand, entity b) VR_Skewer_SwingHit;
float(float xHand, entity b) VR_Skewer_GoesBy;

// The blow of `xHand` VR_Melee_Strike (or VR_Melee_StrikeMore) found, in mh_c_*: dealt by the weapon in that hand.
void(float xHand) VR_Melee_Deal =
{
    entity b = VR_Blow(xHand);
    if(VR_Skewer_GoesBy(xHand, b))
    {
        return;
    }
    VR_Skewer_SwingHit(xHand, b); // (a gib on the blade where it met it)
    VR_Embed_Blow(xHand, b, b.mh_c_ent, b.mh_c_pos, b.mh_c_dir, b.mh_speed, b.mh_kind); // (it may stay in: vr_embedhold.qc)
    // Each thing struck sounds (a swing through two monsters sounds twice).
    float spent = VRGetMeleeHitSoundPlayed(xHand);    if(spent && b.mh_c_ent != b.mh_hit_ent)
    {
        VRSetMeleeHitSoundPlayed(xHand, FALSE);
    }

    if(b.mh_c_ent.vr_gib)
    {
        // A loose gib or head (vr_melee.qc sweeps them since round 21's melee fixes): a stab skewers it, else it bursts as a
        // held one does.
        // (A gib you threw, struck only to be skewered, VR_Skewer_AirTarget: not burst by a stab that doesn't.)
        if(!VR_Skewer_Blow(xHand, b, b.mh_c_ent, b.mh_c_pos, b.mh_c_dir, b.mh_speed, b.mh_kind) &&
           (VR_Blow_Strikable(b.mh_c_ent) || VR_SmallGib_BlowBursts(b.mh_c_ent)))
        {
            VR_Gib_Blow(xHand, b.mh_c_ent, b.mh_speed, b.mh_c_pos, b.mh_c_dir);
        }
    }
    else if(VR_Melee_CorpseTarget(b.mh_c_ent))
    {
        // A corpse the sweep crossed (one swing, many things): VR_Corpse_StrikeFrame's blow, where the blade met it.
        float blunt = (b.mh_kind == VR_MKIND_POMMEL) || ((VRGetEntWeapon(self, xHand) == WID_SWORD) &&
                      VR_ADMelee_Blunt(VR_ADMelee_Kind(VRGetEntWeaponFlags(self, xHand))));
        VR_Corpse_Struck(xHand, b.mh_c_ent, b.mh_c_pos, b.mh_c_dir, b.mh_speed, blunt);
        VRSetMeleeHitSoundPlayed(xHand, TRUE);
    }
    else
    {
        VR_Hit_Begin(QVR_HITKIND_MELEE, xHand, '0 0 0', '0 0 0');
        vr_melee_blunt = ((b.mh_kind == VR_MKIND_POMMEL) || // its own sound (VR_Melee_HitSound)
                         ((VRGetEntWeapon(self, xHand) == WID_SWORD) && // AD support: a hammer, a mace (vr_ad_guns_defs.qc)
                          VR_ADMelee_Blunt(VR_ADMelee_Kind(VRGetEntWeaponFlags(self, xHand)))));
        vr_melee_slap = b.mh_kind == VR_MKIND_SLAP; // a slap's sound, no blood or gore (W_FistMelee, vr_smallgibs.qc)
        vr_sgib_at = b.mh_c_pos; // where small gibs are torn out (vr_smallgibs.qc)
        vr_melee_blowhand = xHand + 1; // a prop it breaks by the impact rule (T_Damage_VRMelee, VR_Prop_BlowDamage)
        // (A pole weapon first: the spear is a sword's kind, the Crystal Lance a gun's; both strike as a pole.)
        if(VR_PoleHeld(xHand))
            DoXHandMeleeImpl(xHand, "", VR_PoleMelee);
        else if(!DoXHandMelee(xHand, WID_FIST, "", W_FistMelee))
            if(!DoXHandMelee(xHand, WID_AXE, "", W_FireAxe))
                if(!DoXHandMelee(xHand, WID_MJOLNIR, "", HIP_FireMjolnir))
                if(!DoXHandMelee(xHand, WID_SWORD, "", W_SwordMelee))
                if(!DoXHandMelee(xHand, WID_CHAINSAW, "", W_ChainsawMelee))
                if(!DoXHandMelee(xHand, WID_CROWBAR, "", W_CrowbarMelee))
                if(!DoXHandMelee(xHand, WID_SUPERAXE, "", W_SuperAxeMelee)) // vr_mg3_weapons.qc
                    DoXHandMeleeImpl(xHand, "", W_GunMelee);
        vr_melee_blowhand = 0;
        vr_melee_blunt = FALSE;
        vr_melee_slap = FALSE;
        vr_sgib_at = '0 0 0';
        VR_Hit_End();
    }

    if(VRGetMeleeHitSoundPlayed(xHand))
    {
        // It landed.
        VR_Melee_Landed(xHand);
    }
};

void(float xHand, entity b, entity m, vector xAt, vector xDir, float xSpeed, float xKind) VR_Embed_Blow; // (vr_embedhold.qc)
void(float xHand) VR_Embed_BlowsDone;
void(float xHand) VR_Embed_WaitFrame;

// The blows of `xHand` this frame (vr_melee.qc VR_Melee_Strike): the first, and (one swing, many things:
// VR_Melee_Multi) every other thing, and the world, the same sweep crossed.
void(float xHand) VR_Melee_DealAll =
{
    VR_Melee_Deal(xHand);
    float more;
    for(more = 0; (more < VR_MELEE_MULTI_LIST + 1) && VR_Melee_StrikeMore(xHand); ++more)
    {
        VR_Melee_Deal(xHand);
    }
};

void(float xHand, float xInMelee, float* xOutMeleeAttackFinished, float* xOutInMelee,
     float* xMeleeHitSoundPlayed) PlayerVRMeleeImpl =
{
    // The blow this frame (vr_melee.qc): a part of what the hand holds swept into something, fast enough, as the kind
    // of blow its part and motion make; the hands in a bash's push don't strike, and each thing is struck once a motion.
    // One swing, many things (VR_Melee_Multi): every thing it crosses, each once, and no pause after a blow (each thing
    // struck once a motion: a body crossed the frame after the floor is struck).
    // (Hands that move themselves: tracked, or a bot's -- its punch and its swing, MP B11 part 4: VR_HandLinesKept.)
    if(VR_HandLinesKept(self) && (time >= xInMelee || VR_Melee_Multi(VR_Blow(xHand))) && VR_Melee_Strike(xHand))
    {
        VR_Melee_DealAll(xHand);
        VR_Embed_BlowsDone(xHand);
        *xOutMeleeAttackFinished = time + 0.1;
        *xOutInMelee = time + 0.02;
    }
    VR_Embed_WaitFrame(xHand);

    VR_Grenade_CatchCheck(xHand, VR_Blow(xHand)); // an empty hand not striking catches a grenade in flight (vr_grenade.qc)
    VR_Deflect(xHand, VR_Blow(xHand)); // batting swings (less than a blow) bat enemy projectiles back
}

// Headbutts (vr_headbutt): a lunge of the head, roughly towards where it looks, faster than
// vr_headbutt_speed (m/s), hits what is in front of the eyes as the head slows down, for
// vr_headbutt_damage times the lunge's speed over the threshold (at most three times). Forgiving:
// any monster within reach ahead counts (not just one on a thin line), and one the head is already
// in is hit at once. A headbutt waits 0.4 s after the last. A ragdoll lies apart from its box: only its drawn body counts
// (his note vrragtriggers_2026-11-03_02-18-13: bending down to grab a lying grunt, the head at 1.5 m/s and still 31 units off
// its middle, "headbutted" it for 24: 25 to 0, 23 to -1, read as an impact).

// Whether a headbutt from `xHead` along `xDir` reaches ragdoll `e`'s drawn body.
float(entity e, vector xHead, vector xDir) VR_Headbutt_Reaches =
{
    return hitmodel_segment(e, xHead, xHead + xDir * 22, 6, HITCLASS_MELEE) >= 0;
};

// The monster a headbutt from `xHead` along `xDir` hits, or world (a remote client's: where his screen showed it,
// lagcompbegin, MP_PLAN.md B8c).
entity(vector xHead, vector xDir) VR_Headbutt_TargetNow;
entity(vector xHead, vector xDir) VR_Headbutt_Target =
{
    float lc = lagcompbegin(self);
    entity e = VR_Headbutt_TargetNow(xHead, xDir);
    if(lc)
    {
        lagcompend();
    }
    return e;
};

entity(vector xHead, vector xDir) VR_Headbutt_TargetNow =
{
    entity best = world;
    float bestDist = 99999;
    entity e = findradius(xHead + xDir * 14, 30);
    while(e)
    {
        if(e != self && e.takedamage && e.health > 0 && e.solid != SOLID_NOT &&
           (!isragdoll(e) || VR_Headbutt_Reaches(e, xHead, xDir)))
        {
            vector nearest = e.origin + (e.mins + e.maxs) * 0.5;
            vector to = nearest - xHead;
            float d = vlen(to);
            if(d < bestDist && (d < 16 || normalize(to) * xDir > 0.3))
            {
                best = e;
                bestDist = d;
            }
        }
        e = e.chain;
    }
    return best;
};

// The headbutt lands: along the lunge (headbutt_max_vec), as hard as its fastest (`xThreshold` is
// vr_headbutt_speed).
void(float xThreshold) VR_Headbutt_Hit =
{
    vector dir = normalize(self.headbutt_max_vec);
    float dmg = min(3, vlen(self.headbutt_max_vec) / xThreshold) * cvar_hget(cvarh_vr_headbutt_damage);
    vector head = VRGetHeadPos(self);
    entity victim = VR_Headbutt_Target(head, dir);
    vector at;
    if(victim == world)
    {
        traceline(head, head + dir * 22, FALSE, self);
        if(trace_fraction == 1.0)
        {
            return;
        }
        victim = trace_ent;
        at = trace_endpos;
        if(isragdoll(victim) && !VR_Headbutt_Reaches(victim, head, dir))
        {
            return; // (its box, not its body)
        }
    }
    else
    {
        at = head + dir * 12;
    }

    self.in_headbutt = FALSE;
    self.headbutt_start_time = time + 0.4;
    VR_Melee_Event(sprintf("headbutt %s, %.2f m/s", victim.takedamage ? victim.classname : "a wall", vlen(self.headbutt_max_vec)));
    VR_SoundVaried(self, CHAN_OTHER, "fisthit.wav", 1.0, ATTN_NORM);
    VR_Blunt_HitLayer(self, CHAN_AUTO, victim);
    VRGunHaptic(cVR_OffHand, 0.15, 100, 1.0);
    VRGunHaptic(cVR_MainHand, 0.15, 100, 1.0);

    if(victim.takedamage)
    {
        victim.axhitme = 1;
        VR_HitBloodSplash(victim, at, dmg);
        VR_Hit_Begin(QVR_HITKIND_HEADBUTT, -1, at, dir);
        T_Damage_VRMelee(victim, self, self, dmg);
        VR_Hit_End();
    }
    else
    {
        BroadcastGunshotEffect(at); // a wall: dust, and it hurts a little
        PainSound();
    }
};

void() PlayerVRMeleeHeadbutt =
{
    // Flatscreen play: the head is the game's to move (the lean, the crouch, the slide, lying down), never a lunge of his
    // (his note 2026-11-13: a crouched lean looking down "headbutted" the floor, the fist's thud and his own pain cry).
    if(!cvar_hget(cvarh_vr_headbutt) || !self.ishuman || self.health <= 0 || VR_FlatPlay(self))
    {
        self.in_headbutt = FALSE;
        return;
    }

    float threshold = max(0.3, cvar_hget(cvarh_vr_headbutt_speed));
    float speed = vlen(self.headvel);

    // Lunging: fast enough, roughly towards where the head looks (tested last: most frames the
    // head is slow).
    if(speed >= threshold && time >= self.headbutt_start_time &&
       (self.headvel * VRForwardOf(self.v_angle)) / speed > 0.2)
    {
        if(!self.in_headbutt)
        {
            self.in_headbutt = TRUE;
            self.headbutt_max_vec = self.headvel;
        }
        else if(vlen(self.headbutt_max_vec) < speed)
        {
            self.headbutt_max_vec = self.headvel;
        }

        // Already in a monster: hit now.
        vector head = VRGetHeadPos(self);
        entity close = VR_Headbutt_Target(head, normalize(self.headvel));
        if(close != world && vlen(close.origin + (close.mins + close.maxs) * 0.5 - head) < 20)
        {
            VR_Headbutt_Hit(threshold);
        }
        return;
    }

    // The lunge ends when the head slows to 70% of the threshold (hysteresis).
    if(!self.in_headbutt || speed >= threshold * 0.7)
    {
        return;
    }
    self.in_headbutt = FALSE;
    self.headbutt_start_time = time + 0.4;
    VR_Headbutt_Hit(threshold);
}

void() VR_KdTrig_Tackle; // (vr_kdtrig.qc)
void() VR_Clash_Frame; // (vr_ownclash.qc)

void() PlayerVRMelee =
{
    PlayerVRMeleeHeadbutt();
    VR_Heartbeat();
    VR_Melee_Frame(); // both hands' new poses (vr_melee.qc)
    VR_HotBlade_Frame(); // the blades in a flame heat, in water cool (vr_hotblade.qc)
    VR_Clash_Frame(); // your two weapons meeting fast: sparks and a sound by their materials (vr_ownclash.qc)
    VR_Buttons_WeaponFrame(); // a held weapon touching a wall button presses it, all of it (buttons.qc)
    VR_Gib_HeldFrame(); // a gib held in one hand struck by the other (vr_carry.qc)
    VR_Bash();
    VR_KdTrig_Tackle(); // running into a monster in a guard or a shove (the ragdoll triggers' Tackles)

    PlayerVRMeleeImpl(
        cVR_MainHand,
        self.in_melee,
        &self.mainhand_melee_attack_finished,
        &self.in_melee,
        &self.melee_hit_sound_played
    );

    PlayerVRMeleeImpl(
        cVR_OffHand,
        self.offhand_in_melee,
        &self.offhand_melee_attack_finished,
        &self.offhand_in_melee,
        &self.offhand_melee_hit_sound_played
    );
}
