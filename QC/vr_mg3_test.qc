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
    else dprint(sprintf("mg3test: unknown request %g\n", request));
    self = oldself; other = oldother; activator = oldact;
};
