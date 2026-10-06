// Destructive source-backed MG1 progression acceptance. Debug > Tests; reload after use.
float MG1_H_pass, MG1_H_fail;
.entity MG1_H_player;
.vector MG1_H_origin;
void(float ok, string text) MG1_H_Check = {
    if(ok) MG1_H_pass++; else MG1_H_fail++;
    dprint(sprintf("mghubtest: %s %s\n",ok ? "PASS" : "FAIL",text));
};
void() MG1_H_Sink = { self.cnt++; };
void() MG1_H_Report = {
    entity p = find(world, classname, "player");
    float count = 0, ghosts = 0, active = 0;
    entity e = find(world, classname, "misc_rune_indicator");
    while(e) { count++; if(e.alpha == 0.2) ghosts++; else active++; e = find(e, classname, "misc_rune_indicator"); }
    dprint(sprintf("mghubtest: report map %s flags %g indicators %g active %g ghosts %g finalgate %g\n",mapname,serverflags,count,active,ghosts,!!find(world,classname,"hub_trigger_changelevel")));
    dprint(sprintf("mghubtest: player health %g max %g origin %v\n",p.health,p.max_health,p.origin));
    dprint(sprintf("mghubtest: hands shells %g weapons %g/%g clips %g/%g ids %g/%g\n",p.ammo_shells,p.weapon,p.weapon2,p.weaponinst.wi_clip,p.weaponinst2.wi_clip,p.weaponinst.wi_uid,p.weaponinst2.wi_uid));
    dprint(sprintf("mghubtest: holster0 weapon %g clip %g id %g\n",p.holsterweapon0,p.holsterweaponinst0.wi_clip,p.holsterweaponinst0.wi_uid));
    dprint(sprintf("mghubtest: holster1 weapon %g clip %g id %g\n",p.holsterweapon1,p.holsterweaponinst1.wi_clip,p.holsterweaponinst1.wi_uid));
    dprint(sprintf("mghubtest: holster2 weapon %g clip %g id %g\n",p.holsterweapon2,p.holsterweaponinst2.wi_clip,p.holsterweaponinst2.wi_uid));
    dprint(sprintf("mghubtest: holster3 weapon %g clip %g id %g\n",p.holsterweapon3,p.holsterweaponinst3.wi_clip,p.holsterweaponinst3.wi_uid));
    dprint(sprintf("mghubtest: holster4 weapon %g clip %g id %g\n",p.holsterweapon4,p.holsterweaponinst4.wi_clip,p.holsterweaponinst4.wi_uid));
    dprint(sprintf("mghubtest: holster5 weapon %g clip %g id %g\n",p.holsterweapon5,p.holsterweaponinst5.wi_clip,p.holsterweaponinst5.wi_uid));
};
void() MG1_H_Unit = {
    if(mapname != "hub") { dprint("mghubtest: unit needs Machine hub\n"); return; }
    entity caller = self, p = find(world,classname,"player");
    float flags = serverflags;
    MG1_H_pass = MG1_H_fail = 0;
    float i = 1, bit = 1;
    entity sink = spawn(); sink.targetname = "mghub_test_sink"; sink.use = MG1_H_Sink;
    while(i <= 5) {
        entity rune = spawn(); self = rune; rune.classname = "item_sigil"; rune.spawnflags = bit; rune.target = sink.targetname;
        MG1_item_sigil(); other = p; serverflags = 0;
        rune.MG1_rune_locked = TRUE; MG1_sigil_touch();
        MG1_H_Check(serverflags == 0 && rune.model != "","locked rune refuses collection");
        rune.MG1_rune_locked = FALSE; itemTouch();
        MG1_H_Check((serverflags & 31) == bit && MG1_LastRune() == bit,"rune records owned bit plus last pickup");
        itemTouch(); MG1_H_Check(sink.cnt == i,"rune target fires exactly once");
        entity spot = MG1_ReturnSpawn();
        MG1_H_Check(spot.netname == sprintf("start_%g", i) && MG1_LastRune() == 0 && (serverflags & 31) == bit,"return chooses authored spot and clears only last-pickup bits");
        remove(rune); bit *= 2; i++;
    }
    serverflags = flags; remove(sink); self = caller;
    dprint(sprintf("mghubtest: %g passed, %g failed\n",MG1_H_pass,MG1_H_fail));
};
void() MG1_H_PuzzleDone = {
    entity manager = self, p = self.MG1_H_player;
    entity door = find(world,targetname,"rune_shell"); float opened = 0;
    while(door) {
        if(door.classname == "door" && door.use == door_use) {
            if(vlen(door.origin - door.MG1_H_origin - door.dest2) < 0.1 && door.speed == 500) opened++;
        }
        door = find(door,targetname,"rune_shell");
    }
    MG1_H_Check(opened == 8,"all eight authored shell panels move by dest2 at speed500");
    entity rune = find(world,classname,"item_sigil");
    MG1_H_Check(!rune.MG1_rune_locked,"authored shell button unlocks physical rune pickup");
    self = rune; other = p; MG1_sigil_touch();
    MG1_H_Check((serverflags & 2) && MG1_LastRune() == 2 && rune.model == "","unlocked physical rune awards second episode and runs targets");
    dprint(sprintf("mghubtest: puzzle %g passed, %g failed\n",MG1_H_pass,MG1_H_fail));
    remove(manager);
};
void() MG1_H_PuzzleOpen = {
    entity manager = self, p = self.MG1_H_player;
    MG1_H_Check(!find(world,classname,"mge2m2_electrode_target"),"all four matched electrode endpoints removed after physical presses");
    entity counter = find(world,targetname,"electrode_counter");
    MG1_H_Check(!counter || counter.count == 0,"authored delayed relays complete four-input counter");
    MG1_H_Check(!find(world,targetname,"electrode_fx_repeater"),"counter cancels authored repeating electrode lightning");
    // This driver skips player traversal/combat. Clear living authored enemies from the egg path
    // through ordinary damage before checking exact panel displacement; gameplay collision stays intact.
    entity enemy = nextent(world);
    while(enemy) {
        entity next = nextent(enemy);
        if((enemy.flags & FL_MONSTER) && enemy.health > 0) { self = enemy; T_Damage(enemy,p,p,100000); }
        enemy = next;
    }
    entity door = find(world,targetname,"rune_shell"); float panels = 0;
    while(door) { if(door.classname == "door" && door.use == door_use) { door.MG1_H_origin = door.origin; panels++; } door = find(door,targetname,"rune_shell"); }
    MG1_H_Check(panels == 8,"eight authored rune egg panels retained");
    entity button = find(world,classname,"func_button");
    while(button && button.target != "open_rune_shell") button = find(button,classname,"func_button");
    self = button; other = p; button.handtouch();
    self = manager; self.think = MG1_H_PuzzleDone; self.nextthink = time + 2.5;
};
void() MG1_H_Puzzle = {
    if(mapname != "mge2m2") { dprint("mghubtest: puzzle needs mge2m2\n"); return; }
    entity caller = self, p = find(world,classname,"player");
    MG1_H_pass = MG1_H_fail = 0;
    entity rune = find(world,classname,"item_sigil");
    MG1_H_Check(rune.MG1_rune_locked,"rune starts locked after authored fixer delay");
    float flags = serverflags; self = rune; other = p;
    rune.handtouch(); MG1_H_Check(!rune.carry_player && serverflags == flags,"locked physical object cannot be grabbed or collected");
    MG1_sigil_touch(); MG1_H_Check(serverflags == flags && rune.model != "","locked carry callback cannot bypass puzzle");
    entity b = find(world,classname,"mge2m2_electrode_button"); float buttons = 0;
    while(b) {
        self = b; other = p; buttons++;
        if(buttons == 2) b.vr_wpntouch(); else if(buttons == 3) VR_Button_PressBy(p,cVR_OffHand,b); else b.handtouch();
        MG1_H_Check(b.MG1_electrode_pressed && b.state == STATE_UP,"physical electrode button invokes native once-only effect");
        b = find(b,classname,"mge2m2_electrode_button");
    }
    MG1_H_Check(buttons == 4,"all four authored electrodes retained");
    b = find(world,classname,"func_button");
    while(b && b.target != "rune_shell_plinth_unlock") b = find(b,classname,"func_button");
    self = b; other = p; b.handtouch();
    entity manager = spawn(); manager.MG1_H_player = p; manager.think = MG1_H_PuzzleOpen; manager.nextthink = time + 17;
    self = caller;
};
void() MG1_H_After = { localcmd(sprintf("exec mghub_after%g.cfg\n", cvar("vr_mg_hub_stage"))); };
void(float request) MG1_H_Route = {
    entity caller = self, p = find(world,classname,"player"); other = activator = p;
    if(request == 4) {
        entity rune = find(world,classname,"item_sigil");
        if(rune) { self = rune; MG1_sigil_touch(); dprint(sprintf("mghubtest: actual rune %g map %s flags %g locked %g\n",rune.spawnflags,mapname,serverflags,rune.MG1_rune_locked)); }
    } else if(request == 20) {
        MG_ProgressTest(8);
        p.holsterweapon0 = WID_SHOTGUN; p.holsterweaponflags0 = 0; p.holsterweaponinst0 = WeaponInst_Carried(WID_SHOTGUN, 1, 0);
        p.holsterweapon1 = WID_SHOTGUN; p.holsterweaponflags1 = 0; p.holsterweaponinst1 = WeaponInst_Carried(WID_SHOTGUN, 2, 0);
        p.holsterweapon2 = WID_SHOTGUN; p.holsterweaponflags2 = 0; p.holsterweaponinst2 = WeaponInst_Carried(WID_SHOTGUN, 3, 0);
        p.holsterweapon3 = WID_SHOTGUN; p.holsterweaponflags3 = 0; p.holsterweaponinst3 = WeaponInst_Carried(WID_SHOTGUN, 4, 0);
        p.holsterweapon4 = WID_SHOTGUN; p.holsterweaponflags4 = 0; p.holsterweaponinst4 = WeaponInst_Carried(WID_SHOTGUN, 5, 0);
        p.holsterweapon5 = WID_SHOTGUN; p.holsterweaponflags5 = 0; p.holsterweaponinst5 = WeaponInst_Carried(WID_SHOTGUN, 6, 0);
        dprint("mghubtest: seeded all six independent holster clips/ids; hold both grips for hand carry\n");
    } else if(request == 5) {
        entity exit = find(world,classname,"trigger_changelevel");
        while(exit && exit.map != "hub") exit = find(exit,classname,"trigger_changelevel");
        if(exit) { self = exit; changelevel_touch(); dprint(sprintf("mghubtest: actual return exit next %s\n",nextmap)); }
    } else if(request == 6) {
        entity gate = find(world,classname,"hub_trigger_changelevel");
        if(gate) { self = gate; changelevel_touch(); dprint(sprintf("mghubtest: actual final gate next %s\n",nextmap)); MG1_H_After(); }
    } else if(request == 16 || request == 17 || request == 18) {
        string dest = request == 16 ? "mge1m3" : request == 17 ? "mge1m2" : "start";
        entity gate = find(world,classname,"trigger_changelevel");
        while(gate && gate.map != dest) gate = find(gate,classname,"trigger_changelevel");
        if(gate) { self = gate; changelevel_touch(); dprint(sprintf("mghubtest: actual route exit next %s\n",nextmap)); }
    } else if(request == 7) {
        // Pure driver: next-map commands are appended AFTER the actual QC changelevel.
        self = p;
        if(intermission_running) {
            float hadtext = strlen(MG_intermissiontext) + strlen(world.endtext);
            ExitIntermission();
            if(hadtext && intermission_running == 2) {
                ExitIntermission();
                if(intermission_running == 3 && (serverflags & 31) == 31 && nextmap != "start") ExitIntermission();
            }
        }
        MG1_H_After();
    } else if(request >= 11 && request <= 15) {
        string dest = request == 11 ? "mge1m1" : request == 12 ? "mge2m1" : request == 13 ? "mge3m1" : request == 14 ? "mge4m1" : "mge5m1";
        entity gate = find(world,classname,"trigger_changelevel");
        while(gate && gate.map != dest) gate = find(gate,classname,"trigger_changelevel");
        if(gate) { self = gate; changelevel_touch(); dprint(sprintf("mghubtest: actual episode gate next %s\n",nextmap)); MG1_H_After(); }
        else dprint("mghubtest: missing or sealed episode gate\n");
    }
    self = caller;
};
void() MG1_H_TestFrame = {
    float request = cvar("vr_mg_hub_test"); if(!request) return;
    cvar_set("vr_mg_hub_test","0");
    if(!MG1_Campaign()) { dprint("mghubtest: Machine developer campaign required\n"); return; }
    entity oldself = self, oldother = other, oldact = activator;
    if(request == 1) MG1_H_Unit(); else if(request == 2) MG1_H_Puzzle(); else if(request == 3) MG1_H_Report(); else MG1_H_Route(request);
    self = oldself; other = oldother; activator = oldact;
};
