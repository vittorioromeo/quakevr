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
void() MG3_TestFrame = {
    float request = cvar("vr_mg3_test"); if(!request) return;
    cvar_set("vr_mg3_test", "0");
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
    else dprint(sprintf("mg3test: unknown request %g\n", request));
    self = oldself; other = oldother; activator = oldact;
};
