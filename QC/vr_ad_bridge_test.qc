#ifdef QVR_TESTS
// vr_ad_bridge's test harness (moved out of vr_ad_bridge.qc; ROUND22's QC cleanup: a pure move). Its globals and the
// cvars it reads stay with the feature.

// vr_ad_test (Debug > Tests > Arcane Dimensions): 1 plays the map's first cinematic camera (misc_camera with a
// targetname) on the player; 2 reports the cinematic system's and the player's state (one "adtest:" line).
void(float request) AD_Test =
{
    entity p = find(world, classname, "player");
    if(request >= 10)
    {
        AD_RulesTest(request, p); // (the follow-ups' tests, any map: vr_ad_player.qc)
        return;
    }
    if(request == 1)
    {
        entity cam = find(world, classname, "misc_camera");
        while(cam && cam.targetname == "") cam = find(cam, classname, "misc_camera");
        if(!cam || !p)
        {
            bprint("adtest: no misc_camera with a targetname here\n");
            return;
        }
        entity oself = self, oother = other;
        self = cam;
        other = p;
        ad_activator = p;
        ad_misc_camera_use();
        self = oself;
        other = oother;
        bprint(sprintf("adtest: camera %s started at %v\n", cam.targetname, cam.origin));
        return;
    }
    if(request == 5)
    {
        // 5: a Widowmaker pickup (weapon_upgrade_ssg) 48 units ahead (vr_pickup_test 2 takes it); 6: the upgrades held.
        entity oself5 = self;
        makevectors(p.angles);
        self = spawn();
        self.classname = "weapon_upgrade_ssg";
        setorigin(self, p.origin + v_forward * 48);
        weapon_upgrade_ssg();
        bprint(sprintf("adtest: %s (%s) put at %v\n", self.classname, self.netname, self.origin));
        self = oself5;
        return;
    }
    if(request == 6)
    {
        // (each record's upgrades: 1 the Shadow Axe, 2 the Widowmaker, 4 the Plasma Gun; parm64 packs them so)
        bprint(sprintf("adtest: upgrades: main hand %s (%g), off hand %s (%g), ", WeaponIdToWeaponName(VRGetEntWeapon(p, cVR_MainHand)),
            AD_UpgradeCode(p.weaponinst), WeaponIdToWeaponName(VRGetEntWeapon(p, cVR_OffHand)), AD_UpgradeCode(p.weaponinst2)));
        bprint(sprintf("packed with the holsters' %g; player's own %g\n", AD_PackWeaponUpgrades(p),
            p.ad_moditems & (ad_IT_UPGRADE_AXE | ad_IT_UPGRADE_SSG | ad_IT_UPGRADE_LG)));
        return;
    }
    if(request == 8 || request == 9)
    {
        // 8: the worldspawn keys take_weapons (the shotgun) and give_weapons (the nailgun, the rocket launcher), 9:
        // no_axestart, on the parms of the player's loadout now (SetChangeParms): the slots they leave (not applied).
        entity os8 = self;
        self = p;
        SetChangeParms();
        self = os8;
        bprint(sprintf("adtest: slots before %g %g | %g %g %g ", parm8, parm10, parm11, parm12, parm13));
        bprint(sprintf("%g %g %g\n", parm14, parm15, parm16));
        if(request == 8) AD_WorldWeaponKeysWith(ad_IT_NAILGUN | ad_IT_ROCKET_LAUNCHER, ad_IT_SHOTGUN, FALSE);
        else AD_WorldWeaponKeysWith(0, 0, TRUE);
        bprint(sprintf("adtest: slots after %g %g | %g %g %g ", parm8, parm10, parm11, parm12, parm13));
        bprint(sprintf("%g %g %g\n", parm14, parm15, parm16));
        bprint(sprintf("adtest: owned shotgun %g nailgun %g rocket launcher %g axe %g\n", (parm1 & IT_SHOTGUN) != 0,
            (parm1 & IT_NAILGUN) != 0, (parm1 & IT_ROCKET_LAUNCHER) != 0, (parm1 & IT_AXE) != 0));
        return;
    }
    if(request == 7)
    {
        // 7: an older game's upgrades (the player's own, AD's .moditems: all three) put on the weapons he carries.
        p.ad_moditems = p.ad_moditems | ad_IT_UPGRADE_AXE | ad_IT_UPGRADE_SSG | ad_IT_UPGRADE_LG;
        AD_UpgradesToWeapons(p);
        AD_Test(6);
        return;
    }
    if(request == 3 || request == 4)
    {
        // 3: the Widowmaker's shot (the super shotgun with AD's SSG upgrade, AD's pellets by the Projectile Rules);
        // 4: the Plasma Gun's (the lightning gun with AD's LG upgrade): what they fired, the ammo they took.
        entity oself = self;
        self = p;
        float wid = request == 3 ? WID_SUPER_SHOTGUN : WID_LIGHTNING;
        p.ammo_shells = 50;
        p.ammo_cells = 50;
        VRGiveEntWeapon(p, cVR_MainHand, wid, 0, WeaponIdToBaseClipSize(wid));
        p.weaponinst.wi_adup = AD_UpgradeFor(wid); // (that weapon upgraded)
        W_SetCurrentAmmoFor(cVR_MainHand);
        float shells = p.ammo_shells, cells = p.ammo_cells;
        entity e;
        float before = 0, after = 0;
        for(e = nextent(world); e; e = nextent(e)) if(e.classname == "proj_bullet" || e.ad_classtype == ad_CT_PROJ_PLASMA) before++;
        if(request == 3) W_FireSuperShotgun(cVR_MainHand);
        else AD_FirePlasma(cVR_MainHand);
        for(e = nextent(world); e; e = nextent(e)) if(e.classname == "proj_bullet" || e.ad_classtype == ad_CT_PROJ_PLASMA) after++;
        bprint(sprintf("adtest: %s fired %g projectiles; reserve shells %g -> %g, cells %g -> %g, clip %g\n",
            request == 3 ? "Widowmaker" : "Plasma Gun", after - before, shells, p.ammo_shells, cells, p.ammo_cells,
            VRGetEntWeaponClip(p, cVR_MainHand)));
        bprint(sprintf("adtest: AD's pellets %g, Shadow Axe x%g\n", AD_Pellets(p), AD_AxeScale(cVR_MainHand)));
        self = oself;
        return;
    }
    bprint(sprintf("adtest: cinematic %g active %g player movetype %g solid %g takedamage %g origin %v health %g\n",
        ad_cinematic_running, ad_cam_active, p.movetype, p.solid, p.takedamage, p.origin, p.health));
};

// One of AD's monsters waiting for its trigger (AD's delayed spawn): its use brings it in.
float(entity e) AD_TestWaiting =
{
    return AD_Run(e) && !(e.flags & FL_MONSTER) && (e.use == ad_monster_spawn || e.use == ad_monster_spawndelay);
};

// Not test code (the release build keeps these three: Show Melee Arcs and Show Aim Cones use them).
#endif
// One of AD's monsters with a melee attack (Show Melee Arcs: fight.qc VR_MeleeArc_ShowFrame).
float(entity e) AD_HasMelee =
{
    return AD_Run(e) && e.ad_th_melee;
};

// One of AD's monsters with a ranged attack (Show Aim Cones: vr_aimcone.qc VR_AimCone_ShowFrame).
float(entity e) AD_HasMissile =
{
    return AD_Run(e) && e.ad_th_missile;
};

// One of AD's bosses (the aim cone leaves their shots alone: vr_aimcone.qc VR_AimCone_Kind).
float(entity e) AD_IsBoss =
{
    return AD_Run(e) && e.ad_bossflag;
};
#ifdef QVR_TESTS

// Wakes one of AD's monsters (self) to hunt `p`, by AD's own AI. TRUE: it was one.
float(entity p) AD_TestWake =
{
    if(!AD_Run(self) || !self.ad_th_run)
    {
        return FALSE;
    }
    self.enemy = p;
    ad_FoundTarget();
    return TRUE;
};
#endif
