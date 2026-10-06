// Dawn of the Machine acceptance aids (Debug > Tests > Dawn of the Machine Tests, `vr_mg3_test`). Developer campaign
// only (`vr_campaign_native mg3`); results print with developer 1 as "mg3test:" lines.
float MG3_test_pass, MG3_test_fail;
void(float ok, string label) MG3_Check = {
    if(ok) MG3_test_pass++; else MG3_test_fail++;
    dprint(sprintf("mg3test: %s %s\n", ok ? "PASS" : "FAIL", label));
};
void() MG3_T_Report = {
    entity p = find(world, classname, "player");
    dprint(sprintf("mg3test: report map %s worldtype %g skill %g serverflags %g\n", mapname, world.worldtype, skill, serverflags));
    dprint(sprintf("mg3test: masks live %g %g %g %g %g bloody %g", MG3_upgrade_health, MG3_upgrade_shells, MG3_upgrade_nails,
                   MG3_upgrade_rockets, MG3_upgrade_cells, MG3_bloody));
    dprint(sprintf(" start %g %g %g %g %g bloody %g\n", parm51, parm52, parm53, parm54, parm55, parm56));
    if(!p) return;
    dprint(sprintf("mg3test: player health %g max %g armor %g items %g\n", p.health, p.max_health, p.armorvalue, p.items));
    dprint(sprintf("mg3test: ammo shells %g nails %g rockets %g cells %g\n", p.ammo_shells, p.ammo_nails, p.ammo_rockets, p.ammo_cells));
    dprint(sprintf("mg3test: hands %g/%g holsters %g %g %g", p.weapon, p.weapon2, p.holsterweapon0, p.holsterweapon1, p.holsterweapon2));
    dprint(sprintf(" %g %g %g\n", p.holsterweapon3, p.holsterweapon4, p.holsterweapon5));
};
// Capacities (any campaign; destructive: overfills every ammunition, then heals from 1 health, ordinary and mega).
void() MG3_T_Caps = {
    entity p = find(world, classname, "player"); if(!p) return;
    float mh = VR_MaxHealth(p), mega = VR_MegaHealthCap(p);
    float s = VR_MaxAmmo(p, AID_SHELLS), n = VR_MaxAmmo(p, AID_NAILS), r = VR_MaxAmmo(p, AID_ROCKETS), c = VR_MaxAmmo(p, AID_CELLS);
    dprint(sprintf("mg3test: caps campaign %g skill %g dm %g health %g max_health %g mega %g", cvar("vr_campaign"), skill, deathmatch, mh, p.max_health, mega));
    dprint(sprintf(" shells %g nails %g rockets %g cells %g", s, n, r, c));
    dprint(sprintf(" lava %g multi %g plasma %g\n", VR_MaxAmmo(p, AID_LAVA_NAILS), VR_MaxAmmo(p, AID_MULTI_ROCKETS), VR_MaxAmmo(p, AID_PLASMA)));
    MG3_test_pass = MG3_test_fail = 0;
    MG3_Check(p.max_health == mh, "the player's max_health is its capacity");
    p.ammo_shells = p.ammo_nails = p.ammo_rockets = p.ammo_cells = 1000;
    p.ammo_lava_nails = p.ammo_multi_rockets = p.ammo_plasma = 1000;
    other = p; bound_other_ammo();
    dprint(sprintf("mg3test: bounded shells %g nails %g rockets %g cells %g", p.ammo_shells, p.ammo_nails, p.ammo_rockets, p.ammo_cells));
    dprint(sprintf(" lava %g multi %g plasma %g\n", p.ammo_lava_nails, p.ammo_multi_rockets, p.ammo_plasma));
    MG3_Check(p.ammo_shells == s && p.ammo_nails == n && p.ammo_rockets == r && p.ammo_cells == c, "bound_other_ammo caps at the capacities");
    p.health = 1; other = p; T_Heal(p, 1000, FALSE);
    float healed = p.health;
    T_Heal(p, 1000, TRUE);
    dprint(sprintf("mg3test: healed %g mega %g\n", healed, p.health));
    MG3_Check(healed == mh, "ordinary healing stops at the capacity");
    MG3_Check(p.health == mega, "megahealth stops at the mega cap");
    dprint(sprintf("mg3test: caps %g passed, %g failed\n", MG3_test_pass, MG3_test_fail));
};
float(string c) MG3_T_IsUpgrade = {
    return c == "item_upgrade_health" || c == "item_upgrade_shells" || c == "item_upgrade_nails" || c == "item_upgrade_rockets"
        || c == "item_upgrade_cells";
};
// Takes every upgrade lying on this map by the real pickup (carry_use: what a holster calls; itemTouch otherwise) and
// checks the masks, capacities and refills against the map's bit.
void() MG3_T_Collect = {
    entity p = find(world, classname, "player"); if(!p) return;
    float flag = MG3_UpgradeFlag();
    float h0 = VR_MaxHealth(p), s0 = VR_MaxAmmo(p, AID_SHELLS), n0 = VR_MaxAmmo(p, AID_NAILS), r0 = VR_MaxAmmo(p, AID_ROCKETS), c0 = VR_MaxAmmo(p, AID_CELLS);
    float hadH = MG3_upgrade_health & flag, hadS = MG3_upgrade_shells & flag, hadN = MG3_upgrade_nails & flag;
    float hadR = MG3_upgrade_rockets & flag, hadC = MG3_upgrade_cells & flag;
    float found = 0, taken = 0, faded = 0, kinds = 0;
    p.health = 1; p.ammo_shells = p.ammo_nails = p.ammo_rockets = p.ammo_cells = 0;
    entity e = nextent(world);
    while(e) {
        entity nx = nextent(e);
        if(MG3_T_IsUpgrade(e.classname) && e.model != string_null) {
            found++;
            if(e.alpha == 0.6) faded++;
            float bit = 1, k = e.MG3_upgrade_kind; // (1 health, 2 shells ... 16 cells)
            while(k > 0) { bit *= 2; k--; }
            kinds = kinds | bit;
            self = e; other = p;
            if(self.carry_use) self.carry_use(); else self.handtouch();
            if(e.model == string_null) taken++;
        }
        e = nx;
    }
    dprint(sprintf("mg3test: upgrades on %s flag %g: %g found, %g taken, %g faded (kinds %g)\n", mapname, flag, found, taken, faded, kinds));
    MG3_T_Report();
    dprint(sprintf("mg3test: caps health %g shells %g nails %g rockets %g", VR_MaxHealth(p), VR_MaxAmmo(p, AID_SHELLS), VR_MaxAmmo(p, AID_NAILS), VR_MaxAmmo(p, AID_ROCKETS)));
    dprint(sprintf(" cells %g (before %g %g %g %g %g)\n", VR_MaxAmmo(p, AID_CELLS), h0, s0, n0, r0, c0));
    MG3_test_pass = MG3_test_fail = 0;
    MG3_Check(found > 0 && taken == found, "every upgrade is taken by its pickup");
    if(kinds & 1) {
        MG3_Check(MG3_upgrade_health & flag, "the health mask has the map's bit");
        MG3_Check(VR_MaxHealth(p) == h0 + (hadH ? 0 : 10) && p.max_health == VR_MaxHealth(p), "health capacity +10 once");
        MG3_Check(hadH ? p.health == 1 : p.health == p.max_health, "health filled to the new capacity (a known upgrade: none)");
    }
    if(kinds & 2) MG3_Check(VR_MaxAmmo(p, AID_SHELLS) == s0 + (hadS ? 0 : 10) && p.ammo_shells == (hadS ? 0 : VR_MaxAmmo(p, AID_SHELLS)), "shells +10 once, filled");
    if(kinds & 4) MG3_Check(VR_MaxAmmo(p, AID_NAILS) == n0 + (hadN ? 0 : 10) && p.ammo_nails == (hadN ? 0 : VR_MaxAmmo(p, AID_NAILS)), "nails +10 once, filled");
    if(kinds & 8) MG3_Check(VR_MaxAmmo(p, AID_ROCKETS) == r0 + (hadR ? 0 : 10) && p.ammo_rockets == (hadR ? 0 : VR_MaxAmmo(p, AID_ROCKETS)), "rockets +10 once, filled");
    if(kinds & 16) MG3_Check(VR_MaxAmmo(p, AID_CELLS) == c0 + (hadC ? 0 : 10) && p.ammo_cells == (hadC ? 0 : VR_MaxAmmo(p, AID_CELLS)), "cells +10 once, filled");
    dprint(sprintf("mg3test: upgrades %g passed, %g failed\n", MG3_test_pass, MG3_test_fail));
};
void() MG3_TestFrame = {
    float request = cvar("vr_mg3_test"); if(!request) return;
    cvar_set("vr_mg3_test", "0");
    if(request == 4) { entity os = self, oo = other; MG3_T_Caps(); self = os; other = oo; return; }
    if(!MG3_Campaign()) { dprint("mg3test: Dawn of the Machine developer campaign required (vr_campaign_native mg3)\n"); return; }
    entity oldself = self, oldother = other, oldact = activator;
    if(request == 1) MG3_T_Report();
    else if(request == 2) { // seed every slot with a distinct mask (map1, map3, map2b, secret6 bits...)
        MG3_SetUpgradeMask(MG3_UPGRADE_HEALTH, 5); MG3_SetUpgradeMask(AID_SHELLS, 2); MG3_SetUpgradeMask(AID_NAILS, 8192);
        MG3_SetUpgradeMask(AID_ROCKETS, 16384); MG3_SetUpgradeMask(AID_CELLS, 1); MG3_SetBloodyBits(MG3_BLOODY_SG | MG3_BLOODY_SSG);
        dprint("mg3test: seeded masks 5 2 8192 16384 1 bloody 3\n"); MG3_T_Report();
    }
    else if(request == 3) { // one more health bit this level (a death's restart takes it back: the level-start masks)
        MG3_SetUpgradeMask(MG3_UPGRADE_HEALTH, MG3_UpgradeMask(MG3_UPGRADE_HEALTH) | 64);
        dprint("mg3test: added health bit 64\n"); MG3_T_Report();
    }
    else if(request == 5) MG3_T_Collect();
    else dprint(sprintf("mg3test: unknown request %g\n", request));
    self = oldself; other = oldother; activator = oldact;
};
