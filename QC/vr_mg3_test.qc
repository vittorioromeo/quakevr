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
// ----------------------------------------------------------------------------
// M3-05 map triggers (any campaign). Request 6: this map's silent teleports (each from its middle: moved by its
// height, keeping x/y, landing in the open) and door relays (each target toggled by its state and flags), then
// spawned trigger_always / multitouch / heal / music / quad / doorgroup / explosion repeater / killmonster, with the
// timed checks half a second later. Request 7: uses every trigger_explosion_repeater on the map (secret2's 60) and
// counts their blasts 20 seconds later.
.float MG3_T_want;
float MG3_T_blasts0, MG3_T_reps, MG3_T_repcount;
entity MG3_T_player;
float MG3_T_health0;
entity(string tname) MG3_T_Sink = { return MG_T_NewSink(tname); };
float(string tname) MG3_T_SinkCount = { entity e = find(world, targetname, tname); return e ? e.cnt : -1; };
entity(string cls) MG3_T_Spawn = {
    entity e = spawn(); e.classname = cls;
    setorigin(e, MG3_T_player.origin + '0 0 -4096'); // far below the map: nothing touches it by itself
    return e;
};
void(entity e) MG3_T_Touch = { entity os = self, oo = other; self = e; other = MG3_T_player; activator = MG3_T_player; e.touch(); self = os; other = oo; };
void(entity e) MG3_T_Use = { entity os = self; self = e; activator = MG3_T_player; e.use(); self = os; };
void() MG3_T_TriggersLate = {
    entity p = MG3_T_player;
    MG3_Check(MG3_T_SinkCount("mg3tt_always") == 1, "trigger_always fired its target once after load");
    MG3_Check(MG3_T_SinkCount("mg3tt_multi") == 2, "multitouch: first touch, then emptied");
    MG3_Check(MG3_T_SinkCount("mg3tt_multi16") == 1, "multitouch IGNORE_FIRST_TOUCH: emptied only");
    MG3_Check(MG3_T_SinkCount("mg3tt_multi64") == 0, "multitouch REQUIRE_ONGROUND: an airborne player does nothing");
    MG3_Check(!find(world, classname, "mg3tt_repeater") && MG3_explosion_blasts - MG3_T_blasts0 == 3, "explosion repeater: count 3 blasts, then gone");
    MG3_Check(p.super_damage_finished <= time, "the quad of trigger_quad ends after leaving it");
    p.flags = p.flags - (p.flags & FL_GODMODE); p.health = MG3_T_health0;
    dprint(sprintf("mg3test: triggers %g passed, %g failed\n", MG3_test_pass, MG3_test_fail));
    remove(self);
};
void() MG3_T_Triggers = {
    entity p = find(world, classname, "player"); if(!p) return;
    MG3_T_player = p; MG3_test_pass = MG3_test_fail = 0;
    p.flags = p.flags | FL_GODMODE; MG3_T_health0 = p.health;
    vector keep = p.origin;
    // Silent teleports.
    float n = 0, moved = 0, open = 0;
    entity e = find(world, classname, "trigger_teleport_silent");
    while(e) {
        vector c = (e.absmin + e.absmax) * 0.5;
        setorigin(p, c);
        MG3_T_Touch(e);
        float want = c_z + e.height; if(e.height < 0) want = want + 1;
        n++;
        if(p.origin_z == want && p.origin_x == c_x && p.origin_y == c_y) moved++;
        if(pointcontents(p.origin) != CONTENT_SOLID) open++;
        else dprint(sprintf("mg3test: silent teleport at %v lands in solid at %v\n", c, p.origin));
        e = find(e, classname, "trigger_teleport_silent");
    }
    setorigin(p, keep);
    dprint(sprintf("mg3test: %g silent teleports, %g moved by their height, %g land in the open\n", n, moved, open));
    if(n) MG3_Check(moved == n && open == n, "every silent teleport moves the player by its height into the open");
    // Door relays: each target toggled by its state unless the flags say leave it.
    float rn = 0, rok = 0;
    e = find(world, classname, "trigger_door_relay");
    while(e) {
        entity t = find(world, targetname, e.target);
        while(t) {
            t.MG3_T_want = 10 + t.state; // unchanged
            if(t.classname == "func_door" || t.classname == "func_button") {
                if((t.state == STATE_TOP || t.state == STATE_UP) && !(e.spawnflags & 1)) t.MG3_T_want = 1;
                if((t.state == STATE_BOTTOM || t.state == STATE_DOWN) && !(e.spawnflags & 2)) t.MG3_T_want = 2;
            }
            t = find(t, targetname, e.target);
        }
        MG3_T_Use(e);
        float good = TRUE;
        t = find(world, targetname, e.target);
        while(t) {
            if(t.MG3_T_want == 1 && t.state != STATE_DOWN && t.state != STATE_BOTTOM) good = FALSE;
            if(t.MG3_T_want == 2 && t.state != STATE_UP && t.state != STATE_TOP) good = FALSE;
            if(t.MG3_T_want >= 10 && t.state != t.MG3_T_want - 10) good = FALSE;
            if(!good) dprint(sprintf("mg3test: door relay %s: %s state %g (want %g)\n", e.target, t.classname, t.state, t.MG3_T_want));
            t = find(t, targetname, e.target);
        }
        rn++; if(good) rok++;
        e = find(e, classname, "trigger_door_relay");
    }
    dprint(sprintf("mg3test: %g door relays, %g toggled their targets\n", rn, rok));
    if(rn) MG3_Check(rok == rn, "every door relay toggles its doors and buttons by state");
    // Spawned triggers.
    entity os = self;
    MG3_T_Sink("mg3tt_always");
    e = MG3_T_Spawn("mg3tt"); e.target = "mg3tt_always"; self = e; trigger_always(); self = os;
    MG3_T_Sink("mg3tt_multi"); MG3_T_Sink("mg3tt_multi16"); MG3_T_Sink("mg3tt_multi64");
    e = MG3_T_Spawn("mg3tt"); e.target = "mg3tt_multi"; self = e; trigger_multitouch(); self = os;
    MG3_T_Touch(e); MG3_T_Touch(e);
    MG3_Check(MG3_T_SinkCount("mg3tt_multi") == 1, "multitouch fires once on the first touch");
    e = MG3_T_Spawn("mg3tt"); e.target = "mg3tt_multi16"; e.spawnflags = 16; self = e; trigger_multitouch(); self = os;
    MG3_T_Touch(e);
    float pf = p.flags; p.flags = p.flags - (p.flags & FL_ONGROUND);
    e = MG3_T_Spawn("mg3tt"); e.target = "mg3tt_multi64"; e.spawnflags = 64 | 16; self = e; trigger_multitouch(); self = os;
    MG3_T_Touch(e); p.flags = pf;
    e = MG3_T_Spawn("mg3tt"); e.dmg = 5; self = e; trigger_heal(); self = os;
    p.health = 10; MG3_T_Touch(e); float h1 = p.health; MG3_T_Touch(e);
    MG3_Check(h1 == 15 && p.health == 15, "trigger_heal heals dmg once per wait");
    e = MG3_T_Spawn("trigger_music"); e.targetname = "mg3tt_music"; e.style = (world.sounds == 7) ? 8 : 7; self = e; trigger_music(); self = os;
    MG3_T_Use(e);
    MG3_Check(e.sounds == e.style && e.style != world.sounds, "trigger_music switches to its style");
    e = MG3_T_Spawn("mg3tt"); self = e; trigger_quad(); self = os;
    MG3_T_Touch(e);
    MG3_Check(VR_ItemUtil_EntHasItem(p, IID_QUAD) && p.super_damage_finished > time, "trigger_quad gives quad inside");
    MG3_T_Sink("mg3tt_group");
    entity d1 = MG3_T_Spawn("mg3tt_door"), d2 = MG3_T_Spawn("mg3tt_door");
    d1.category = d2.category = "mg3tt_cat"; d1.state = STATE_TOP; d2.state = STATE_BOTTOM;
    d1.goal_state = STATE_TOP; d2.goal_state = STATE_TOP;
    e = MG3_T_Spawn("mg3tt"); e.category = "mg3tt_cat"; e.target = "mg3tt_group"; self = e; trigger_doorgroup_relay(); self = os;
    MG3_T_Use(e); float g1 = MG3_T_SinkCount("mg3tt_group");
    d2.state = STATE_UP; MG3_T_Use(e);
    MG3_Check(g1 == 0 && MG3_T_SinkCount("mg3tt_group") == 1, "doorgroup relay fires only with every door at its goal");
    MG3_T_blasts0 = MG3_explosion_blasts;
    e = MG3_T_Spawn("mg3tt_repeater"); e.spawnflags = 4; e.count = 3; e.wait = 0.1; self = e; trigger_explosion_repeater(); self = os;
    MG3_T_Use(e);
    MG3_Check(MG3_explosion_blasts - MG3_T_blasts0 == 1, "explosion repeater blasts at once without a delay");
    entity m = nextent(world);
    while(m && !((m.flags & FL_MONSTER) && m.health > 0)) m = nextent(m);
    if(m) {
        string mt = m.targetname; m.targetname = "mg3tt_victim";
        e = MG3_T_Spawn("mg3tt"); e.targetname = "mg3tt_kill"; e.target = "mg3tt_victim"; self = e; trigger_relay_killmonster(); self = os;
        MG3_T_Use(e);
        MG3_Check(m.health <= 0, "trigger_relay_killmonster kills its target");
        m.targetname = mt;
    }
    e = spawn(); e.think = MG3_T_TriggersLate; e.nextthink = time + 0.5;
};
void() MG3_T_RepeatersLate = {
    float left = 0, fired = 0;
    entity e = find(world, classname, "trigger_explosion_repeater");
    while(e) { left++; if(e.MG3_explosions > 0) fired++; e = find(e, classname, "trigger_explosion_repeater"); }
    float done = MG3_T_reps - left;
    dprint(sprintf("mg3test: repeaters %g used, %g done and gone, %g still going (%g of them fired), blasts %g (counts sum %g)\n",
        MG3_T_reps, done, left, fired, MG3_explosion_blasts - MG3_T_blasts0, MG3_T_repcount));
    MG3_test_pass = MG3_test_fail = 0;
    MG3_Check(MG3_T_reps > 0 && done + fired == MG3_T_reps, "every repeater fired");
    MG3_Check(MG3_T_repcount == 0 || MG3_explosion_blasts - MG3_T_blasts0 >= MG3_T_repcount, "counted repeaters blasted their counts");
    MG3_T_player.flags = MG3_T_player.flags - (MG3_T_player.flags & FL_GODMODE);
    dprint(sprintf("mg3test: repeaters %g passed, %g failed\n", MG3_test_pass, MG3_test_fail));
    remove(self);
};
void() MG3_T_Repeaters = {
    entity p = find(world, classname, "player"); if(!p) return;
    MG3_T_player = p; p.flags = p.flags | FL_GODMODE;
    MG3_T_blasts0 = MG3_explosion_blasts; MG3_T_reps = MG3_T_repcount = 0;
    entity e = find(world, classname, "trigger_explosion_repeater");
    while(e) {
        entity nx = find(e, classname, "trigger_explosion_repeater");
        MG3_T_reps++; if(e.spawnflags & 4) MG3_T_repcount = MG3_T_repcount + e.count;
        MG3_T_Use(e);
        e = nx;
    }
    dprint(sprintf("mg3test: used %g explosion repeaters\n", MG3_T_reps));
    e = spawn(); e.think = MG3_T_RepeatersLate; e.nextthink = time + 20;
};
void() MG3_TestFrame = {
    float request = cvar("vr_mg3_test"); if(!request) return;
    cvar_set("vr_mg3_test", "0");
    if(request == 4 || request == 6 || request == 7) { // any campaign
        entity os = self, oo = other, oa = activator;
        if(request == 4) MG3_T_Caps(); else if(request == 6) MG3_T_Triggers(); else MG3_T_Repeaters();
        self = os; other = oo; activator = oa; return;
    }
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
