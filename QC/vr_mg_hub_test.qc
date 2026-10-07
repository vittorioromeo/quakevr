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
    dprint(sprintf("mghubtest: player health %g max %g origin %v skill %g intermission %g armor %g items %g\n",p.health,p.max_health,p.origin,cvar("skill"),intermission_running,p.armorvalue,p.items));
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
// Route sweep: the next real exit. In the hub, the final gate once it exists, else the lowest episode's gate whose
// rune is still missing (a sealed episode's gate is killtargeted); elsewhere the first exit, or with alt one going
// to another map (mge1m1's secret mge1m3).
entity(float alt) MG1_H_NextExit = {
    entity e = world, first = world;
    float i = 1, bit = 1;
    if(mapname == "hub") {
        e = find(world, classname, "hub_trigger_changelevel");
        while(!e && i <= 5) {
            if(!(serverflags & bit)) {
                e = find(world, classname, "trigger_changelevel");
                while(e && e.map != sprintf("mge%gm1", i)) e = find(e, classname, "trigger_changelevel");
            }
            bit *= 2; i++;
        }
        return e;
    }
    first = find(world, classname, "trigger_changelevel");
    if(!alt) return first;
    e = first;
    while(e && e.map == first.map) e = find(e, classname, "trigger_changelevel");
    return e;
};
// Route sweep driver: with vr_mg_route_stage set, each new map's (or loaded save's) first frame with the player in
// it runs the private script mg1route<stage>.cfg once (the scripts raise the stage themselves). A changelevel from
// QC is appended to the console buffer, so a script's step that leaves the map ends the script.
nosave float MG1_H_autoran;
void() MG1_H_RouteAutorun = {
    float stage = cvar("vr_mg_route_stage");
    if(!stage || MG1_H_autoran || time < 1 || !find(world, classname, "player")) return;
    MG1_H_autoran = TRUE;
    localcmd(sprintf("exec mg1route%g.cfg\n", stage));
};
// The presser: real jump presses (the client's button, as a player's) whenever an intermission may be left or the
// player is dead. The credits (the campaign's last exit) disconnect: it then runs mg1route_end.cfg after them.
.float MG1_H_inter;
void() MG1_H_PresserThink = {
    entity p = find(world, classname, "player");
    self.nextthink = time;
    if(intermission_running != self.MG1_H_inter) {
        dprint(sprintf("mghubtest: intermission %g next %s map %s\n", intermission_running, nextmap, mapname));
        if(intermission_running >= 2 && nextmap == "start" && MG1_Campaign() && !coop && !strlen(MG_intermissiontext))
            localcmd("exec mg1route_end.cfg\n");
        self.MG1_H_inter = intermission_running;
    }
    if(time < self.ltime) return;
    if((intermission_running && time >= intermission_exittime) || (p && p.deadflag >= DEAD_DEAD)) {
        dprint(sprintf("mghubtest: press (intermission %g dead %g)\n", intermission_running, p.deadflag));
        localcmd("+jump\nwait\n-jump\n");
        self.ltime = time + 1.5;
    }
};
void() MG1_H_Presser = {
    if(find(world, classname, "mghub_presser")) return;
    entity e = spawn(); e.classname = "mghub_presser"; e.think = MG1_H_PresserThink; e.nextthink = time;
};
float MG1_H_spotfound;
float(float i) MG1_H_GridFrac = { return i == 0 ? 0.5 : i == 1 ? 0.25 : i == 2 ? 0.75 : i == 3 ? 0 : 1; };
// Where the player's box overlaps a trigger's volume and fits out of the walls (a trigger's brush may cut into them,
// or be thinner than the player): a grid across the overlapping positions, its middle first; its centre when none fits.
vector(entity t, entity p) MG1_H_SpotIn = {
    vector lo = t.absmin - p.maxs + '2 2 2', hi = t.absmax - p.mins - '2 2 2', at;
    MG1_H_spotfound = FALSE;
    float fx = 0, fy, fz;
    while(fx <= 4) {
        fy = 0;
        while(fy <= 4) {
            fz = 0;
            while(fz <= 4) {
                at_x = lo_x + (hi_x - lo_x) * MG1_H_GridFrac(fx);
                at_y = lo_y + (hi_y - lo_y) * MG1_H_GridFrac(fy);
                at_z = lo_z + (hi_z - lo_z) * MG1_H_GridFrac(fz);
                tracebox(at, p.mins, p.maxs, at, FALSE, p);
                if(!trace_startsolid && !trace_allsolid) { MG1_H_spotfound = TRUE; return at; }
                fz++;
            }
            fy++;
        }
        fx++;
    }
    return (t.absmin + t.absmax) * 0.5 - (p.mins + p.maxs) * 0.5;
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
    } else if(request == 30 || request == 32) {
        // Route sweep: put the player inside the exit's volume; the engine's own trigger touch on the next
        // physics frame runs the real changelevel (its classname, player and coop rules included), and the
        // presser goes through its intermission with real jump presses.
        entity walk = MG1_H_NextExit(request == 32);
        if(walk) {
            setorigin(p, MG1_H_SpotIn(walk, p));
            // An exit behind a shut door or bars: noclip there (its frame still touches the triggers it overlaps).
            if(!MG1_H_spotfound) p.movetype = MOVETYPE_NOCLIP;
            dprint(sprintf("mghubtest: walk spot %v fits %g exit box %v %v player %v %v\n",p.origin,MG1_H_spotfound,walk.absmin,walk.absmax,p.mins,p.maxs));
            p.velocity = '0 0 0';
            dprint(sprintf("mghubtest: walk %s %s -> %s skill %g flags %g\n",mapname,walk.classname,walk.map,cvar("skill"),serverflags));
            MG1_H_Presser();
        } else dprint(sprintf("mghubtest: walk %s no open exit\n",mapname));
    } else if(request == 35) {
        MG1_H_Presser();
    } else if(request == 36) {
        // A real death (god mode off, damage past the health): the presser's release-then-press respawns, which in
        // single player restarts the map through the engine's autoload of the last save (sv_autoload).
        p.flags = p.flags - (p.flags & FL_GODMODE);
        T_Damage(p, world, world, p.health + 1000);
        dprint(sprintf("mghubtest: died health %g deadflag %g\n", p.health, p.deadflag));
        MG1_H_Presser();
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
    MG1_H_RouteAutorun();
    float request = cvar("vr_mg_hub_test"); if(!request) return;
    cvar_set("vr_mg_hub_test","0");
    if(!MG1_Campaign()) { dprint("mghubtest: Machine developer campaign required\n"); return; }
    entity oldself = self, oldother = other, oldact = activator;
    if(request == 1) MG1_H_Unit(); else if(request == 2) MG1_H_Puzzle(); else if(request == 3) MG1_H_Report(); else MG1_H_Route(request);
    self = oldself; other = oldother; activator = oldact;
};
