// Destructive native shared-trigger acceptance; Debug > Tests. Reload afterward.
float MG_T_pass;
float MG_T_fail;
float MG_T_started;
.entity MG_T_actor;
.float MG_T_bad;
.float MG_T_last;
.float MG_T_interval;
void(float ok, string text) MG_T_Check = {
    if(ok) MG_T_pass++; else MG_T_fail++;
    dprint(sprintf("mgtriggertest: %s %s\n",ok ? "PASS" : "FAIL",text));
};
void() MG_T_Sink = {
    self.cnt++;
    if(activator.classname != "player") self.MG_T_bad++;
    self.MG_T_actor = activator;
    if(self.MG_T_last) self.MG_T_interval = time - self.MG_T_last;
    self.MG_T_last = time;
};
entity(string name) MG_T_NewSink = {
    entity e = spawn(); e.targetname = name; e.use = MG_T_Sink; return e;
};
entity(string name) MG_T_NewRelay = {
    entity e = spawn(); e.target = name; e.use = SUB_UseTargets; return e;
};
void(string name, entity actor) MG_T_Fire = {
    entity oldself = self;
    entity r = MG_T_NewRelay(name); self = r; activator = actor; SUB_UseTargets(); remove(r); self = oldself;
};
void() MG_T_Finish = {
    entity manager = self;
    entity e = find(world,targetname,"mgtt_explosion_sink");
    MG_T_Check(e.cnt == 1 && !e.MG_T_bad,"no-damage delayed explosion fires target once with activating player");
    e = find(world, targetname, "mgtt_repeat_sink");
    MG_T_Check(e.cnt >= 3 && e.cnt <= 6, "repeater fires at authored cadence");
    MG_T_Check(e.MG_T_interval >= 0.074 && e.MG_T_interval <= 0.12, "repeater interval retains wait plus jitter");
    MG_T_Check(!e.MG_T_bad, "repeater retains physical player activator");
    e = find(world, targetname, "mgtt_stopped_sink");
    MG_T_Check(e.cnt == 0, "toggled-off repeater never fires");
    e = find(world, targetname, "mgtt_delayed_sink");
    MG_T_Check(e.cnt == 0, "all canceled delayed activations stay canceled");
    e = find(world, targetname, "mgtt_other_sink");
    MG_T_Check(e.cnt == 1 && !e.MG_T_bad, "unrelated delayed target fires once with actor");
    MG_T_Check(!find(world, targetname, "mgtt_fade_dead"), "fade removes the dead target");
    e = find(world, targetname, "mgtt_fade_alive");
    MG_T_Check(e && e.health == 1 && e.alpha == 1, "fade preserves living target");
    e = find(world, targetname, "mgtt_light_sink");
    MG_T_Check(e.cnt == 11 && !e.MG_T_bad, "lightning flags and delayed double pulses preserve activator");
    entity timed = find(world, targetname, "mgtt_timed");
    MG_T_Check(timed.count == 2, "actual timed-counter timeout restores original count");
    self = timed; activator = manager.MG_T_actor;
    MG_counter_timed_use(); MG_counter_timed_use();
    e = find(world, targetname, "mgtt_timed_sink");
    MG_T_Check(e.cnt == 1 && !e.MG_T_bad, "timed completion fires immediately once after reset");
    e = find(world, targetname, "mgtt_repeat"); self = e; MG_repeater_use();
    dprint(sprintf("mgtriggertest: %g passed, %g failed\n",MG_T_pass,MG_T_fail));
    remove(self);
    remove(manager);
};

void(entity p) MG_T_Interactions = {
    entity caller = self;
    float oldItems = p.items;
    entity button = spawn(); self = button;
    button.classname = "func_button"; button.state = STATE_BOTTOM; button.spawnflags = 16; button.speed = 100;
    button.pos1 = button.origin = '0 0 4000'; button.pos2 = button.pos1 + '0 0 1';
    button.noise = "buttons/switch02.wav"; precache_sound(button.noise);
    MG_ButtonInit();
    MG_T_Check(button.items == IT_KEY1, "authored silver key button bit16 selects silver key");
    p.items = 0; other = p; button_handtouch();
    MG_T_Check(button.state == STATE_BOTTOM, "hand cannot bypass authored key button");
    VR_Button_PressBy(p,cVR_MainHand,button);
    MG_T_Check(button.state == STATE_BOTTOM, "held prop/weapon activation cannot bypass key button");
    entity thrown = spawn(); thrown.classname = "thrown_weapon"; thrown.movetype = MOVETYPE_TOSS;
    thrown.carry_letgo = p; thrown.velocity = '1000 0 0';
    float oldThrow = cvar("vr_button_throw"); cvar_set("vr_button_throw","1");
    other = thrown; button_touch_any();
    MG_T_Check(button.state == STATE_BOTTOM, "thrown weapon cannot bypass key button");
    p.items = IT_KEY1; other = p; button_handtouch();
    MG_T_Check(button.state == STATE_UP && !button.items && !(p.items & IT_KEY1), "physical key press consumes the player's key and permanently unlocks ordinary button");
    button.state = STATE_BOTTOM; button.nextthink = -1;
    button.spawnflags = 40; MG_ButtonInit(); p.items = IT_KEY2; other = p; button_handtouch();
    MG_T_Check(button.items == IT_KEY2 && !(p.items & IT_KEY2), "gold key plus always-required flag keeps button locked after spending key");
    button.state = STATE_BOTTOM; button.nextthink = -1; button_handtouch();
    MG_T_Check(button.state == STATE_BOTTOM, "always-required button needs another key for another physical press");
    cvar_set("vr_button_throw",ftos(oldThrow)); p.items = oldItems;
    remove(button); remove(thrown);
    entity door = spawn(); self = door; door.noise2 = "misc/null.wav"; precache_sound(door.noise2);
    door.origin = '0 0 4000'; door.pos1 = door.origin + '100 0 0'; door.speed = 50; door.speed2 = 100; door.cnt = 5;
    door_go_down(); MG_T_Check(fabs(vlen(door.velocity)-100)<0.001,"official door speed2 controls closing velocity");
    door.speed2 = 0; door_go_down(); MG_T_Check(fabs(vlen(door.velocity)-50)<0.001,"official door without speed2 ignores Honey cnt multiplier");
    remove(door);
    entity corpse = spawn(); corpse.flags = FL_MONSTER; corpse.health = 0; corpse.targetname = "mgtt_cleanup_dead";
    entity live = spawn(); live.flags = FL_MONSTER; live.health = 1; live.targetname = "mgtt_cleanup_live";
    entity cleanup = spawn(); self = cleanup; float oldCoop = coop; coop = 1; trigger_cleanup_corpses(); MG_cleanup_corpses_use(); coop = oldCoop;
    MG_T_Check(!find(world,targetname,"mgtt_cleanup_dead") && find(world,targetname,"mgtt_cleanup_live") == live && p.health > 0,"cooperative corpse cleanup removes dead monsters and preserves living monsters/players");
    remove(live);
    entity explosion = spawn(); self = explosion; explosion.spawnflags = 1; explosion.delay = 0.05;
    explosion.target = "mgtt_explosion_sink"; explosion.targetname = "mgtt_explosion";
    MG_T_NewSink(explosion.target); trigger_explosion(); activator = p; MG_trigger_explosion_use();
    MG_T_Check(explosion.takedamage == DAMAGE_NO && explosion.nextthink > time,"official explosion is an activated delayed point effect without Rogue brush behavior");
    self = caller;
};
void() MG_T_Start = {
    entity oldself = self; entity oldother = other; entity oldact = activator;
    entity p = find(world, classname, "player");
    MG_T_pass = MG_T_fail = 0;
    if(!p || !MG_WorldCampaign()) { dprint("mgtriggertest: native campaign with a player required\n"); return; }
    MG_T_Interactions(p);
    entity manager = spawn(); manager.MG_T_actor = p;
    manager.think = MG_T_Finish; manager.nextthink = time + 0.45;
    MG_T_NewSink("mgtt_repeat_sink"); MG_T_NewSink("mgtt_stopped_sink");
    entity r = spawn(); self = r; r.targetname = "mgtt_repeat"; r.target = "mgtt_repeat_sink";
    r.wait = 0.075; r.pausetime = 0.025; trigger_repeater(); activator = p;
    MG_repeater_use(); MG_T_Check(r.nextthink >= time + 0.074 && r.nextthink <= time + 0.101, "repeater schedules first interval with random jitter");
    entity stopped = spawn(); self = stopped; stopped.target = "mgtt_stopped_sink"; stopped.spawnflags = 1;
    trigger_repeater(); MG_repeater_use(); MG_T_Check(!stopped.nextthink && stopped.think == SUB_Null, "start-on repeater toggles fully off");
    MG_T_NewSink("mgtt_delayed_sink"); MG_T_NewSink("mgtt_other_sink");
    entity source = MG_T_NewRelay("mgtt_delayed_sink"); self = source;
    source.targetname = "mgtt_cancel"; source.delay = 0.1; activator = p;
    SUB_UseTargets(); source.delay = 0.2; SUB_UseTargets();
    MG_T_Check(MG_CountTargets(source) == 1, "delayed source retains its independent target");
    entity killer = spawn(); self = killer; killer.killtarget = "mgtt_cancel"; SUB_UseTargets();
    MG_T_Check(!find(world, targetname, "mgtt_cancel"), "killtarget cancels source plus every pending use"); remove(killer);
    source = MG_T_NewRelay("mgtt_other_sink"); self = source; source.delay = 0.1; SUB_UseTargets();
    entity counter = spawn(); self = counter; counter.count = 2; counter.delay = 0.12; counter.spawnflags = 1;
    counter.target = "mgtt_timed_sink"; counter.targetname = "mgtt_timed";
    MG_T_NewSink("mgtt_timed_sink"); trigger_counter_timed(); MG_counter_timed_use();
    MG_T_Check(counter.count == 1 && fabs(counter.nextthink - time - 0.12) < 0.001, "timed counter schedules authored inactivity window");
    entity loop = spawn(); self = loop; loop.count = 2; loop.spawnflags = 3; loop.target = "mgtt_loop_sink";
    entity loopsink = MG_T_NewSink(loop.target); trigger_counter(); MG_counter_use(); MG_counter_use();
    MG_T_Check(loopsink.cnt == 1 && loop.count == 2, "looping counter restores authored count on completion");
    entity change = spawn(); entity changed = spawn(); changed.targetname = "mgtt_rewire"; changed.target = "old";
    self = change; change.target = "mgtt_rewire"; change.killtarget = "new"; trigger_changetarget(); MG_changetarget_use();
    MG_T_Check(changed.target == "new" && find(world,targetname,"mgtt_rewire") == changed, "changetarget rewrites without removing endpoint");
    entity dead = spawn(); dead.health = 0; dead.targetname = "mgtt_fade_dead";
    entity alive = spawn(); alive.health = 1; alive.targetname = "mgtt_fade_alive";
    entity fade = spawn(); self = fade; fade.target = dead.targetname; fade.delay = 0.1; trigger_fade(); MG_spawn_fade_manager();
    fade.target = alive.targetname; MG_spawn_fade_manager();
    entity freeze = spawn(); entity frozenTarget = spawn(); frozenTarget.targetname = "mgtt_freeze"; frozenTarget.nextthink = time + 1; frozenTarget.think = SUB_Null;
    self = freeze; freeze.target = frozenTarget.targetname; trigger_freeze(); MG_SUB_FreezeTargets();
    MG_T_Check(frozenTarget.MG_is_frozen && frozenTarget.MG_storednextthink == time + 1, "freeze saves pending think time");
    MG_SUB_FreezeTargets(); MG_T_Check(!frozenTarget.MG_is_frozen && frozenTarget.nextthink == time + 1, "freeze toggle restores authored pending time");
    entity quake = spawn(); self = quake; quake.spawnflags = 1; quake.wait = 0.2; trigger_screenshake(); MG_screenshake_use();
    vector punch = p.punchangle; vector view = p.v_angle; vector oldOrigin = p.origin;
    MG_T_Shake(); MG_T_Check(p.punchangle == punch && p.v_angle == view && p.origin == oldOrigin, "quake feedback never forces head angles or tracked position");
    entity light_sink = MG_T_NewSink("mgtt_light_sink");
    entity endpoint = spawn(); endpoint.targetname = "mgtt_light_end"; endpoint.target = light_sink.targetname;
    setorigin(endpoint,p.origin + '0 0 48');
    entity endpoint2 = spawn(); endpoint2.targetname = endpoint.targetname; endpoint2.target = light_sink.targetname;
    setorigin(endpoint2,p.origin + '8 0 48');
    entity bolt = spawn(); self = bolt; bolt.target = endpoint.targetname; setorigin(bolt,p.origin + '0 0 48'); trigger_lightning();
    MG_play_lightning(); // two endpoints, each immediate plus delayed (4)
    bolt.spawnflags = 8; MG_play_lightning(); // once each (2)
    bolt.spawnflags = 4; MG_play_lightning(); // no endpoint targets (0)
    bolt.spawnflags = 9; MG_play_lightning(); // one random endpoint once (1)
    bolt.spawnflags = 10; MG_play_lightning(); // backwards trace once each (2)
    bolt.spawnflags = 24; MG_play_lightning(); // silent MG3 once each (2)
    MG_T_Check(light_sink.cnt == 9, "lightning immediate endpoint pulses honor all flags");
    self = oldself; other = oldother; activator = oldact;
};
void() MG_T_MapFinish = {
    entity manager = self;
    entity counter = find(world,targetname,"rune_puz_counter");
    MG_T_Check(!counter, "authored rune puzzle counter completes exactly once");
    MG_T_Check(!find(world,targetname,"rune_puz_1") && !find(world,targetname,"rune_puz_2"), "authored completion removes buttons and pending flashes");
    entity door = find(world,targetname,"rune_puz_unlock");
    while(door && door.classname != "door") door = find(door,targetname,"rune_puz_unlock");
    MG_T_Check(door && door.state == STATE_TOP && vlen(door.origin - door.pos2) < 0.01, "real engine movement reaches rune door authored 120-unit endpoint");
    entity monster = find(world,targetname,"rune_puz_unlock");
    while(monster && monster.classname != "monster_shalrath") monster = find(monster,targetname,"rune_puz_unlock");
    MG_T_Check(monster && !monster.MG_deferred, "authored puzzle completion activates deferred monster after 1.5 seconds");
    MG_T_Check(!find(world,targetname,"spookydoor_open_kill"), "authored quake sequence removes blocking geometry after nine seconds");
    entity rep = find(world,targetname,"spookydoor_lightning_repeater");
    MG_T_Check(!rep, "authored quake sequence stops lightning repeater");
    dprint(sprintf("mgtriggertest: mge5m2 %g passed, %g failed\n",MG_T_pass,MG_T_fail));
    remove(manager);
};
void() MG_T_MapSecond = {
    entity manager = self;
    entity counter = find(world,targetname,"rune_puz_counter");
    MG_T_Check(counter && counter.count == counter.cnt, "authored rune puzzle resets after missed two-second window");
    entity b = find(world,targetname,"rune_puz_1");
    while(b && b.classname != "func_button") b = find(b,targetname,"rune_puz_1");
    self = b; other = manager.MG_T_actor; button_handtouch();
    b = find(world,targetname,"rune_puz_2");
    while(b && b.classname != "func_button") b = find(b,targetname,"rune_puz_2");
    self = b; other = manager.MG_T_actor; button_handtouch();
    self = manager; manager.think = MG_T_MapFinish; manager.nextthink = MG_T_started + 10.2;
};
void() MG_T_MapStart = {
    if(mapname != "mge5m2") { dprint("mgtriggertest: authored test needs mge5m2\n"); return; }
    entity oldself = self; entity oldother = other; entity oldact = activator;
    entity p = find(world,classname,"player"); MG_T_pass = MG_T_fail = 0; MG_T_started = time;
    entity manager = spawn(); manager.MG_T_actor = p; manager.think = MG_T_MapSecond; manager.nextthink = time + 3.2;
    entity teleport = find(world,targetname,"voidtele"); MG_T_Fire("pickup_rune",p);
    MG_T_Check(teleport.target == "voidsave_2", "authored rune pickup rewires safety teleport");
    MG_T_Fire("trigger_1",p); MG_T_Check(teleport.target == "voidsave_3", "authored progression advances safety teleport");
    entity d = find(world,targetname,"spookydoor_open");
    MG_T_Check(d && fabs(vlen(d.pos2-d.pos1)-60)<0.01, "authored movedir is exact door delta, independent of brush size");
    entity b = find(world,targetname,"rune_puz_1");
    while(b && b.classname != "func_button") b = find(b,targetname,"rune_puz_1");
    self = b; other = p; button_handtouch();
    MG_T_Fire("spookydoor",p);
    self = oldself; other = oldother; activator = oldact;
};

void() MG_T_LightningDamage = {
    entity caller = self; entity oldother = other; entity oldact = activator;
    entity p = find(world,classname,"player"); MG_T_pass = MG_T_fail = 0;
    vector startpos = p.origin + '0 0 16';
    vector raydir = '1 0 0';
    traceline(startpos,startpos+raydir*128,TRUE,p);
    if(trace_fraction != 1) { raydir = '0 1 0'; traceline(startpos,startpos+raydir*128,TRUE,p); }
    if(trace_fraction != 1) { raydir = '-1 0 0'; traceline(startpos,startpos+raydir*128,TRUE,p); }
    if(trace_fraction != 1) { raydir = '0 -1 0'; traceline(startpos,startpos+raydir*128,TRUE,p); }
    if(trace_fraction != 1) { dprint("mgtriggertest: damage test needs a clear 128-unit corridor\n"); return; }
    entity dummy = spawn(); dummy.classname = "mgtt_damage_dummy"; dummy.solid = SOLID_BBOX;
    dummy.health = 100; dummy.takedamage = DAMAGE_YES; dummy.th_pain = SUB_NullPain;
    setsize(dummy,'-4 -4 -4','4 4 4'); setorigin(dummy,startpos+raydir*64);
    entity endpoint = spawn(); endpoint.targetname = "mgtt_damage_end"; setorigin(endpoint,startpos+raydir*128);
    entity bolt = spawn(); self = bolt; bolt.target = endpoint.targetname; setorigin(bolt,startpos);
    trigger_lightning(); bolt.dmg = 10; activator = p; MG_play_lightning();
    MG_T_Check(dummy.health == 90,"lightning applies authored damage through native positional hit code");
    bolt.spawnflags = 2; MG_play_lightning();
    MG_T_Check(dummy.health == 80,"backwards lightning trace applies the same authored damage");
    dummy.wetsuit_finished = time+1; MG_play_lightning();
    MG_T_Check(dummy.health == 80,"lightning respects native wetsuit protection");
    remove(dummy); remove(endpoint); remove(bolt);
    self = caller; other = oldother; activator = oldact;
    dprint(sprintf("mgtriggertest: damage %g passed, %g failed\n",MG_T_pass,MG_T_fail));
};

void() MG_T_RetargetGate = {
    entity caller = self; entity oldother = other; entity oldact = activator;
    entity p = find(world,classname,"player");
    entity gate = find(world,classname,"trigger_teleport");
    while(gate && !portal_handles(gate,p)) gate = find(gate,classname,"trigger_teleport");
    if(!gate) { dprint("mgtriggertest: retarget test needs a native visible slipgate\n"); return; }
    entity olddest = find(world,targetname,gate.target);
    entity dest = spawn(); self = dest; dest.targetname = "mgtt_gate_destination";
    dest.origin = olddest.origin + '128 64 -27'; dest.angles = olddest.mangle;
    info_teleport_destination();
    gate.targetname = "mgtt_cached_gate"; gate.spawnflags = gate.spawnflags | 4;
    entity change = spawn(); self = change; change.target = gate.targetname; change.killtarget = dest.targetname;
    trigger_changetarget(); MG_changetarget_use();
    float carries = portal_handles(gate,p);
    dprint(sprintf("mgtriggertest: retarget cached gate carries %g expected destination (%.0f %.0f %.0f)\n",carries,dest.origin_x,dest.origin_y,dest.origin_z));
    remove(change); self = caller; other = oldother; activator = oldact;
    localcmd("vr_portals_info\n");
};
void() MG_TriggerTestFrame = {
    float request = cvar("vr_mg_trigger_test"); if(!request) return;
    cvar_set("vr_mg_trigger_test","0");
    if(!MG_WorldCampaign()) { dprint("mgtriggertest: select a native campaign first\n"); return; }
    if(request == 2) MG_T_MapStart(); else if(request == 3) MG_T_LightningDamage(); else if(request == 4) MG_T_RetargetGate(); else MG_T_Start();
};
