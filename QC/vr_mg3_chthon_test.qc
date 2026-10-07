// Dawn of the Machine's Chthon tests, M3-24/25 (vr_mg3_ctest N, developer 1; Debug > Tests > Dawn of the Machine: Chthon):
// docs/vr-port/MG3_PLAN.md, docs/vr-port/TESTING.md. Prints "mg3ctest: PASS/FAIL <what>" and a total.
//   1 report: the data, this map's Chthon (his phase, health, fits), the second arena's points, the boss teleport,
//     lava suits, music
//   2 a Chthon of the Debug spawner's ahead (any map; god mode meanwhile): spawned and woken, immune as he rises, his
//     damage rules, his spheres, each fit and phase (waves, no arena here: he fights on), the spiral, killed: counted
//     once, his rings, his gibs, no ending; a second one killed at once uses his targets five times
//   3 (Dawn of the Machine's boss map) the fight: the player walks into the trigger that wakes him, the music, each fit
//     and phase, wave1's knights, the move to the second arena and the countdown's trigger_boss_teleport back to him,
//     waves 2 and 3, the spiral, killed: the gibs; the comfort fades; 8 s on the ending (the finale text, the credits next)
//   4 the same in a Bloody Nightmare game (serverflags 64 + 128 set): the ending is its new game (map1)

float mg3ct_pass, mg3ct_fail;
float mg3ct_step;   // the running request's step, 0: none
float mg3ct_next;   // when its next step runs
float mg3ct_k;      // the fit being driven (1 first blood .. 6 at 16%)
float mg3ct_waited; // since when the step waits for him
float mg3ct_t, mg3ct_kills, mg3ct_n, mg3ct_n2, mg3ct_sph;
float mg3ct_w1, mg3ct_w2, mg3ct_w3, mg3ct_tele, mg3ct_used; // uses of the test's targets
float mg3ct_map;    // 1: the map's fight (requests 3, 4)
float mg3ct_bn;     // 1: in a Bloody Nightmare game (request 4)
float mg3ct_fades;  // comfort fades before
string mg3ct_nextmap;
entity mg3ct_c;
float MG3_CT_WALK1 = 17, MG3_CT_SHOCKA1 = 80, MG3_CT_DEATH1 = 48, MG3_CT_DEATH9 = 56; // (boss.mdl's frames)
entity(entity p, float xDist) MG3_CT_Spawn;

void(float ok, string label) MG3_CT_Check =
{
    if(ok) mg3ct_pass++; else mg3ct_fail++;
    dprint(sprintf("mg3ctest: %s %s\n", ok ? "PASS" : "FAIL", label));
};

void() MG3_CT_Total =
{
    dprint(sprintf("mg3ctest: total %g/%g\n", mg3ct_pass, mg3ct_fail));
    mg3ct_step = 0;
    entity p = find(world, classname, "player");
    if(p) p.flags = p.flags - (p.flags & FL_GODMODE);
};

entity() MG3_CT_Player = { return find(world, classname, "player"); };

// This map's Chthon of Dawn of the Machine (the map's first, else any).
entity() MG3_CT_Chthon =
{
    entity e = find(world, classname, "monster_boss");
    while(e && !e.mg3_chthon) e = find(e, classname, "monster_boss");
    return e;
};

float(string xClass) MG3_CT_CountClass =
{
    float n = 0;
    entity e = find(world, classname, xClass);
    while(e) { n++; e = find(e, classname, xClass); }
    return n;
};

// A wave's doors (func_door whose targetname begins with `xPrefix`) open or opening: the map's waves stand behind them.
float(string xPrefix) MG3_CT_CountWave =
{
    float n = 0;
    float len = strlen(xPrefix);
    entity e = nextent(world);
    while(e)
    {
        if(e.movetype == MOVETYPE_PUSH && substring(e.targetname, 0, len) == xPrefix && (e.state == STATE_UP || e.state == STATE_TOP))
            n = n + 1;
        e = nextent(e);
    }
    return n;
};

void() MG3_CT_W1 = { mg3ct_w1++; };
void() MG3_CT_W2 = { mg3ct_w2++; };
void() MG3_CT_W3 = { mg3ct_w3++; };
void() MG3_CT_Tele = { mg3ct_tele++; };
void() MG3_CT_Used = { mg3ct_used++; };
void(string xName, void() xUse) MG3_CT_Target =
{
    entity t = find(world, targetname, xName);
    if(!t) { t = spawn(); t.classname = "mg3ct_target"; t.targetname = xName; t.use = xUse; }
};

// `p`'s blow of `xDamage` at `c`: plain, or as the lightning gun's bolt from his main hand.
void(entity c, entity p, float xDamage, float xLightning) MG3_CT_Hit =
{
    if(!xLightning) { T_Damage(c, p, p, xDamage); return; }
    float w1 = p.weapon, w2 = p.weapon2;
    p.weapon = p.weapon2 = WID_LIGHTNING;
    VR_SetHitKind(QVR_HITKIND_SHOT, cVR_MainHand, '0 0 0', '0 0 0');
    T_Damage(c, p, p, xDamage);
    VR_ClearHitKind();
    p.weapon = w1; p.weapon2 = w2;
};

// A wave's entities (targetname beginning `xPrefix`): monsters alive, waiting (deferred), dead or gone; the rest.
void(string xPrefix) MG3_CT_WaveReport =
{
    float nAlive = 0, nWaiting = 0, nDead = 0, nRest = 0;
    float len = strlen(xPrefix);
    entity e = nextent(world);
    while(e)
    {
        if(substring(e.targetname, 0, len) == xPrefix)
        {
            if(e.MG_deferred)
            {
                nWaiting = nWaiting + 1;
            }
            else if((e.flags & FL_MONSTER) && e.health > 0)
            {
                nAlive = nAlive + 1;
            }
            else if(e.th_die)
            {
                nDead = nDead + 1;
            }
            else
            {
                nRest = nRest + 1;
            }
        }
        e = nextent(e);
    }
    float dTop = 0, dBottom = 0, dMoving = 0;
    e = nextent(world);
    while(e)
    {
        if(e.movetype == MOVETYPE_PUSH && substring(e.targetname, 0, len) == xPrefix)
        {
            if(e.state == STATE_TOP) dTop = dTop + 1;
            else if(e.state == STATE_BOTTOM) dBottom = dBottom + 1;
            else dMoving = dMoving + 1;
        }
        e = nextent(e);
    }
    dprint(sprintf("mg3ctest: wave %s doors: %g open, %g shut, %g moving\n", xPrefix, dTop, dBottom, dMoving));
    dprint(sprintf("mg3ctest: wave %s: %g alive, %g waiting, %g dead, %g other\n", xPrefix, nAlive, nWaiting, nDead, nRest));
};

void() MG3_CT_Report =
{
    dprint(sprintf("mg3ctest: report map %s campaign %g data %g skill %g serverflags %g\n", mapname, cvar("vr_campaign"),
                   MG3_Chthon_Available(), skill, serverflags));
    entity c = MG3_CT_Chthon();
    if(c)
        dprint(sprintf("mg3ctest: Chthon (%g) spawnflags %g health %g/%g phase %g fits %g immune %g\n", c.mg3_chthon,
                       c.spawnflags, c.health, c.max_health, c.wait, c.frags, c.mg3_chthon_immune));
    dprint(sprintf("mg3ctest: points first %g second %g boss %g; trigger_boss_teleport %g; lava suits %g; trigger_music %g\n",
                   MG3_CT_CountClass("info_boss_teleport_first"), MG3_CT_CountClass("info_boss_teleport_second"),
                   MG3_CT_CountClass("info_boss_teleport_boss"), MG3_CT_CountClass("trigger_boss_teleport"),
                   MG3_CT_CountClass("item_artifact_lavasuit"), MG3_CT_CountClass("trigger_music")));
    if(MG3_CT_Chthon()) { MG3_CT_WaveReport("rein1"); MG3_CT_WaveReport("lavaman3"); MG3_CT_WaveReport("lavaman4"); }
};

// Whether he can have a fit now (boss_final_pain's conditions), risen.
float(entity c) MG3_CT_Ready =
{
    return c.health > 0 && c.attack_finished <= time && c.pain_finished <= time && !c.mg3_chthon_immune &&
           c.think != SUB_Null && c.frame >= MG3_CT_WALK1;
};

// The fits' thresholds (boss_final_pain): just under them.
float(float k) MG3_CT_Fraction =
{
    if(k == 2) return 0.82;
    if(k == 3) return 0.65;
    if(k == 4) return 0.49;
    if(k == 5) return 0.32;
    return 0.15;
};

// Fit `mg3ct_k` driven: he is hurt just past its threshold (the first: a plain blow).
void(entity c, entity p) MG3_CT_DriveFit =
{
    if(mg3ct_k == 1)
    {
        float h = c.health;
        MG3_CT_Hit(c, p, 100, FALSE);
        MG3_CT_Check(h - c.health == 80 * ((MG3_Campaign() && (serverflags & MG3_SF_BN_ACTIVE)) ? 0.8 : 1),
                     sprintf("a plain blow of 100: %g (0.8)", h - c.health));
    }
    else
    {
        c.health = c.max_health * MG3_CT_Fraction(mg3ct_k) + 1;
        MG3_CT_Hit(c, p, 4, TRUE);
    }
    MG3_CT_Check(c.frags == mg3ct_k && c.mg3_chthon_immune && c.frame >= MG3_CT_SHOCKA1,
                 sprintf("fit %g at %g of %g health: immune, his fit (frame %g)", mg3ct_k, c.health, c.max_health, c.frame));
    float h2 = c.health;
    MG3_CT_Hit(c, p, 100, TRUE);
    MG3_CT_Check(c.health == h2, "nothing during it");
};

// The zone a shot from ahead of him (v_forward: his facing), level, at `at` strikes on his model as drawn (-2: missed).
float(entity c, vector at) MG3_CT_Zone =
{
    vector a = at + v_forward * 300, b = at - v_forward * 64;
    float f = hitmodel_segment(c, a, b, 0, HITCLASS_GUNS);
    if(f < 0) return -2;
    return PositionalRegion(c, a + (b - a) * f, b - a);
};

// After fit `mg3ct_k` (its phase now `k`): what the phase brought.
void(entity c, entity p) MG3_CT_AfterFit =
{
    float k = mg3ct_k;
    MG3_CT_Check(c.wait == k, sprintf("phase %g after fit %g", c.wait, k));
    if(k == 1)
    {
        float h = c.health;
        MG3_CT_Hit(c, p, 100, TRUE);
        float bn = (MG3_Campaign() && (serverflags & MG3_SF_BN_ACTIVE)) ? 0.8 : 1;
        MG3_CT_Check(h - c.health == 100 * bn, sprintf("the lightning gun's bolt of 100: %g (whole)", h - c.health));
        entity lm = spawn();
        lm.classname = "monster_lava_man";
        h = c.health;
        T_Damage(c, lm, lm, 100);
        remove(lm);
        MG3_CT_Check(c.health == h, "nothing from a lava man");
        MG3_CT_Check(mg3_chthon_spheres > mg3ct_sph, sprintf("he throws fans of spheres (%g)", mg3_chthon_spheres - mg3ct_sph));
        // His zones (positional damage; on his model as drawn, mapped to walk1 standing): shots from ahead, level.
        makevectors('0 1 0' * c.angles_y);
        PositionalHead(c);
        float zh = MG3_CT_Zone(c, c.origin + v_forward * xHeadForward + '0 0 1' * xHeadUp);
        float zb = MG3_CT_Zone(c, c.origin + '0 0 140');
        MG3_CT_Check(zh == 1 && zb == 0, sprintf("a shot at his head: zone %g; at his chest: %g (1 head, 0 body, 2 limbs)", zh, zb));
    }
    if(k == 2 && !mg3ct_map) MG3_CT_Check(mg3ct_w1 == 1 && mg3ct_w2 == 0, "phase 2: wave1 sent");
    if(k == 3)
    {
        if(!mg3ct_map)
            MG3_CT_Check(mg3ct_tele == 1 && c.think != SUB_Null && !c.mg3_chthon_immune,
                         "phase 3: his tele_target fired; no second arena here: he fights on");
    }
    if(k == 4 && !mg3ct_map) MG3_CT_Check(mg3ct_w2 == 1, "phase 4: wave2 sent");
    if(k == 5)
    {
        if(!mg3ct_map) MG3_CT_Check(mg3ct_w3 == 1, "phase 5: wave3 sent");
        MG3_CT_Check(find(world, classname, "chthon_spiral") != world && mg3_chthon_spirals == 1, "phase 5: the spiral");
    }
};

// The fits, one after another (steps 10..19), then the kill (20..).
void(entity p) MG3_CT_FightStep =
{
    entity c = mg3ct_c;
    if(mg3ct_step == 10) // waiting for him to be ready for fit mg3ct_k
    {
        if(c.classname != "monster_boss" || c.health <= 0) { MG3_CT_Check(FALSE, "he is gone"); MG3_CT_Total(); return; }
        if(!MG3_CT_Ready(c))
        {
            if(time - mg3ct_waited > 40)
            {
                MG3_CT_Check(FALSE, sprintf("he was never ready for fit %g (think frozen %g, immune %g, frame %g)", mg3ct_k,
                                            c.think == SUB_Null, c.mg3_chthon_immune, c.frame));
                MG3_CT_Total();
                return;
            }
            mg3ct_next = time + 0.1;
            return;
        }
        if(mg3ct_map) // (the map's wave doors before the fit whose phase opens them)
            mg3ct_n = MG3_CT_CountWave(mg3ct_k == 2 ? "rein1" : (mg3ct_k == 4 ? "lavaman3" : "lavaman4"));
        MG3_CT_DriveFit(c, p);
        mg3ct_step = 11; mg3ct_next = time + 1.3; // (a fit: 1 s)
        return;
    }
    if(mg3ct_step == 11)
    {
        MG3_CT_AfterFit(c, p);
        if(mg3ct_map && mg3ct_k == 2) { mg3ct_step = 12; mg3ct_next = time + 8; return; }
        if(mg3ct_map && mg3ct_k == 3) { mg3ct_step = 13; mg3ct_next = time; mg3ct_waited = time; return; }
        if(mg3ct_map && mg3ct_k == 4) { mg3ct_step = 15; mg3ct_next = time + 3; return; }
        if(mg3ct_map && mg3ct_k == 5) { mg3ct_step = 16; mg3ct_next = time + 3; return; }
        mg3ct_step = 18; mg3ct_next = time;
        return;
    }
    if(mg3ct_step == 12) // the map's wave1: rein1's relays open its doors and its knights come (1 to 7 s on)
    {
        mg3ct_n2 = MG3_CT_CountWave("rein1");
        MG3_CT_Check(mg3ct_n2 > mg3ct_n, sprintf("wave1: %g of rein1's doors open (%g before)", mg3ct_n2, mg3ct_n));
        mg3ct_step = 18; mg3ct_next = time;
        return;
    }
    if(mg3ct_step == 13) // the map's phase 3: the players in the second arena, he sunk, frozen, immune
    {
        entity pt = find(world, classname, "info_boss_teleport_first");
        float near = 0;
        while(pt) { if(vlen(p.origin - pt.origin) < 64) near = 1; pt = find(pt, classname, "info_boss_teleport_first"); }
        MG3_CT_Check(near, sprintf("phase 3: the player moved to a first point (%v)", p.origin));
        MG3_CT_Check(c.think == SUB_Null && c.mg3_chthon_immune && c.frame == 0, "he sank and waits, immune");
        MG3_CT_Check(mg3_chthon_fades == mg3ct_fades + 1, "the player's view faded in from black (comfort)");
        mg3ct_step = 14; mg3ct_next = time + 0.5; mg3ct_waited = time;
        return;
    }
    if(mg3ct_step == 14) // the countdown (the arena's trigger touched as the player lands) brings trigger_boss_teleport
    {
        entity bp = find(world, classname, "info_boss_teleport_boss");
        if(c.think == SUB_Null)
        {
            if(time - mg3ct_waited > 30)
            {
                MG3_CT_Check(FALSE, "no trigger_boss_teleport in 30 s (the countdown)");
                MG3_CT_Total();
                return;
            }
            mg3ct_next = time + 0.2;
            return;
        }
        MG3_CT_Check(bp && vlen(c.origin - bp.origin) < 1, sprintf("%.1f s on: he rose at his point (%v)", time - mg3ct_waited, c.origin));
        entity pt2 = find(world, classname, "info_boss_teleport_second");
        float near2 = 0;
        while(pt2) { if(vlen(p.origin - pt2.origin) < 96) near2 = 1; pt2 = find(pt2, classname, "info_boss_teleport_second"); }
        MG3_CT_Check(near2, sprintf("the player moved to a second point (%v)", p.origin));
        MG3_CT_Check(mg3_chthon_fades == mg3ct_fades + 2, "faded in from black again");
        mg3ct_step = 18; mg3ct_next = time;
        return;
    }
    if(mg3ct_step == 15 || mg3ct_step == 16) // the map's waves 2 and 3
    {
        string w = (mg3ct_step == 15) ? "lavaman3" : "lavaman4";
        mg3ct_n2 = MG3_CT_CountWave(w);
        MG3_CT_Check(mg3ct_n2 > mg3ct_n, sprintf("wave%g: %g of %s's doors open (%g before)", mg3ct_step - 13, mg3ct_n2, w, mg3ct_n));
        mg3ct_step = 18; mg3ct_next = time;
        return;
    }
    if(mg3ct_step == 18) // the next fit, or the kill
    {
        if(mg3ct_k < 6) { mg3ct_k++; mg3ct_step = 10; mg3ct_next = time; mg3ct_waited = time; return; }
        mg3ct_t = total_monsters; mg3ct_kills = killed_monsters; mg3ct_used = 0; mg3ct_sph = mg3_chthon_spheres;
        mg3_chthon_gibs = 0;
        c.health = 10;
        MG3_CT_Hit(c, p, 50, TRUE);
        MG3_CT_Check(c.health <= 0 && killed_monsters == mg3ct_kills + 1 && c.frame >= MG3_CT_DEATH1 && c.frame <= MG3_CT_DEATH9,
                     sprintf("killed: counted once (%g/%g), sinking", killed_monsters, total_monsters));
        mg3ct_step = 20; mg3ct_next = time + 1.5;
        return;
    }
    if(mg3ct_step == 20) // sunk (death 1..9 and 10: 1 s), the first ring (1 s)
    {
        MG3_CT_Check(MG3_CT_Chthon() == world, "gone under the lava");
        MG3_CT_Check(mg3_chthon_gibs == 49, sprintf("a shower of %g gibs", mg3_chthon_gibs));
        MG3_CT_Check(mg3_chthon_spheres - mg3ct_sph == 72 && MG3_CT_CountClass("chthon_circle") == 3,
                     sprintf("a ring of %g spheres, two more to come", mg3_chthon_spheres - mg3ct_sph));
        if(!mg3ct_map) // (on the map his rings kill knights too)
            MG3_CT_Check(killed_monsters == mg3ct_kills + 1, "counted once");
        if(!mg3ct_map)
            MG3_CT_Check(mg3ct_used == 1, sprintf("his target used by his death alone (every fit had): %g", mg3ct_used));
        mg3ct_step = 21; mg3ct_next = time + 8;
        return;
    }
    if(mg3ct_step == 21) // 9.5 s after his death
    {
        if(!mg3ct_map)
        {
            MG3_CT_Check(!intermission_running && nextmap == mg3ct_nextmap, "the Debug spawner's: no ending");
            // A second one killed at once: his targets used five times in all (upstream: one for each fit not had).
            mg3ct_used = 0;
            entity c2 = MG3_CT_Spawn(p, 600);
            c2.target = "mg3ct_used";
            mg3ct_c = c2;
            mg3ct_step = 22; mg3ct_next = time + 2.5; // (risen)
            return;
        }
        // The map's: the ending (M3-10's MG3_BossEnding), 8 s after he sank: the finale text, then the credits (start), or
        // in a Bloody Nightmare game its new game (map1, the runes cleared: serverflags 448).
        MG3_CT_Check(mg3_chthon_endings == 1 && intermission_running && MG_intermissiontext == "$mg3_qc_boss_finale",
                     sprintf("the ending: the finale text (intermission %g)", intermission_running));
        if(mg3ct_bn)
            MG3_CT_Check(nextmap == "map1" && serverflags == (MG3_SF_BN_ACTIVE | MG3_SF_BN_DISCOVERED | MG3_SF_BN_NEWGAME),
                         sprintf("Bloody Nightmare: its new game next (%s, serverflags %g)", nextmap, serverflags));
        else
            MG3_CT_Check(nextmap == "start", sprintf("next: %s (the credits)", nextmap));
        MG3_CT_Total();
        return;
    }
    if(mg3ct_step == 22)
    {
        mg3ct_c.health = 10;
        MG3_CT_Hit(mg3ct_c, p, 50, TRUE);
        mg3ct_step = 23; mg3ct_next = time + 1.5;
        return;
    }
    if(mg3ct_step == 23)
    {
        MG3_CT_Check(mg3ct_used == 5, sprintf("killed before any fit: his target used %g times", mg3ct_used));
        MG3_CT_Total();
        return;
    }
};

// A Chthon of the Debug spawner's `xDist` ahead of `p`, his wave and teleport targets the test's.
entity(entity p, float xDist) MG3_CT_Spawn =
{
    makevectors('0 1 0' * p.v_angle_y);
    entity d = spawn();
    d.weapon = 50;
    setorigin(d, p.origin + v_forward * xDist);
    entity oldself = self;
    self = d;
    activator = p;
    func_enemy_dispenser_use();
    entity m = self;
    self = oldself;
    remove(d);
    m.wave1 = "mg3ct_w1"; m.wave2 = "mg3ct_w2"; m.wave3 = "mg3ct_w3"; m.tele_target = "mg3ct_tele";
    return m;
};

// Request 2: steps 1..3, then the fight (10..).
void(entity p) MG3_CT_AnywhereStep =
{
    entity c = mg3ct_c;
    if(mg3ct_step == 1)
    {
        MG3_CT_Target("mg3ct_w1", MG3_CT_W1); MG3_CT_Target("mg3ct_w2", MG3_CT_W2);
        MG3_CT_Target("mg3ct_w3", MG3_CT_W3); MG3_CT_Target("mg3ct_tele", MG3_CT_Tele);
        MG3_CT_Target("mg3ct_used", MG3_CT_Used);
        mg3ct_w1 = mg3ct_w2 = mg3ct_w3 = mg3ct_tele = mg3ct_used = 0;
        mg3_chthon_spirals = 0;
        mg3ct_nextmap = nextmap;
        mg3ct_t = total_monsters;
        c = MG3_CT_Spawn(p, 600);
        c.target = "mg3ct_used";
        mg3ct_c = c;
        MG3_CT_Check(c.classname == "monster_boss" && c.mg3_chthon == 2 && (c.spawnflags & MG3_CHTHON_KILLABLE),
                     "the spawner's Chthon: killable, not the map's");
        MG3_CT_Check(total_monsters == mg3ct_t + 1, "counted");
        MG3_CT_Check(PositionalHead(c) && xHeadUp == 240 && xHeadForward == 80 && VR_Decap_HeadModel(c) == "",
                     "his head zone (80 forward, 240 up); no head to cut off");
        MG3_CT_Check(VR_Grapple_MonsterMass(c) == 5000, "Chthon's grapple mass");
        MG3_CT_Check(VR_Dummy_Available(40), "training dummy 40");
        mg3ct_step = 2; mg3ct_next = time + 0.5;
        return;
    }
    if(mg3ct_step == 2)
    {
        MG3_CT_Check(c.use == monster_use && c.health == 12000 && c.frame < MG3_CT_WALK1 && c.enemy == p, "woken at you: rising, 12000 health");
        float h = c.health;
        MG3_CT_Hit(c, p, 100, TRUE);
        MG3_CT_Check(c.health == h, "nothing while he rises");
        mg3ct_sph = mg3_chthon_spheres;
        mg3ct_k = 1; mg3ct_step = 10; mg3ct_next = time; mg3ct_waited = time;
        return;
    }
};

// Request 3 (the boss map): steps 30..31, then the fight (10..).
void(entity p) MG3_CT_MapStep =
{
    entity c = mg3ct_c;
    if(mg3ct_step == 30)
    {
        c = MG3_CT_Chthon();
        mg3ct_c = c;
        MG3_CT_Check(c && c.mg3_chthon == 1 && (c.spawnflags & MG3_CHTHON_KILLABLE) && c.use == boss_final_awake &&
                     c.wave1 == "rein1" && c.wave2 == "lavaman3" && c.wave3 == "lavaman4" && c.tele_target == "boss_tele1",
                     "the map's Chthon waits, killable, his waves and teleport target");
        MG3_CT_Check(MG3_CT_CountClass("info_boss_teleport_first") == 4 && MG3_CT_CountClass("info_boss_teleport_second") == 4 &&
                     MG3_CT_CountClass("info_boss_teleport_boss") == 1, "4 + 4 + 1 teleport points");
        entity tb = find(world, classname, "trigger_boss_teleport");
        MG3_CT_Check(tb && (tb.spawnflags & 1) && tb.targetname == "tele", "trigger_boss_teleport (BOSS) on the countdown's end");
        MG3_CT_Check(MG3_CT_CountClass("item_artifact_lavasuit") > 0, sprintf("%g lava suits (of the map's 21 by skill)", MG3_CT_CountClass("item_artifact_lavasuit")));
        entity trig = find(world, target, "chthon_count");
        MG3_CT_Check(trig != world, "the trigger that wakes him");
        if(!c || !trig) { MG3_CT_Total(); return; }
        mg3ct_sph = mg3_chthon_spheres;
        mg3_chthon_spirals = 0;
        setorigin(p, (trig.absmin + trig.absmax) * 0.5); // (the player walks into it)
        p.velocity = '0 0 0';
        mg3ct_step = 31; mg3ct_next = time + 0.5;
        return;
    }
    if(mg3ct_step == 31)
    {
        MG3_CT_Check(c.use == monster_use && c.health == 12000 && c.enemy == p, "the trigger woke him at you");
        entity m = find(world, classname, "trigger_music");
        MG3_CT_Check(m && m.sounds == 12, sprintf("the music: track %g", m.sounds));
        mg3ct_k = 1; mg3ct_step = 10; mg3ct_next = time; mg3ct_waited = time;
        return;
    }
};

// Each frame (world.qc StartFrame).
void() MG3_ChthonTestFrame =
{
    if(mg3ct_step && time >= mg3ct_next)
    {
        entity os = self, oo = other, oa = activator;
        entity p = MG3_CT_Player();
        if(!p) mg3ct_step = 0;
        else if(mg3ct_step < 10) MG3_CT_AnywhereStep(p);
        else if(mg3ct_step < 30) MG3_CT_FightStep(p);
        else MG3_CT_MapStep(p);
        self = os; other = oo; activator = oa;
    }
    float request = cvar("vr_mg3_ctest");
    if(!request) return;
    cvar_set("vr_mg3_ctest", "0");
    entity pl = MG3_CT_Player();
    if(!pl) return;
    entity oldself = self, oldother = other, oldact = activator;
    if(request == 1) MG3_CT_Report();
    else if(request == 2 || request == 3 || request == 4)
    {
        if(!MG3_Chthon_Available()) dprint("mg3ctest: the Dawn of the Machine data is not there (no Chthon)\n");
        else
        {
            mg3ct_pass = mg3ct_fail = 0;
            mg3ct_map = request >= 3;
            mg3ct_bn = request == 4;
            if(mg3ct_bn) serverflags = serverflags | MG3_SF_BN_ACTIVE | MG3_SF_BN_DISCOVERED;
            mg3ct_fades = mg3_chthon_fades;
            pl.flags = pl.flags | FL_GODMODE;
            mg3ct_step = mg3ct_map ? 30 : 1;
            mg3ct_next = time;
        }
    }
    self = oldself; other = oldother; activator = oldact;
};
