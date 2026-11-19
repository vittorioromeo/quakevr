#ifdef QVR_TESTS
// dw_test.qc -- Dwell support (DWELL.md P2a): vr_ad_test 200, the map's Dwell boss fought to its end (d1end's Awoken,
// d2end's Sculptor): woken as the map would (the Sculptor used, as its fight_phase1 relay does; the Awoken shown its
// enemy), then a killing blow each time it can be hurt, through every phase, until it dies. The Sculptor rests 3 s
// between phases here (its `wait`: 40 in the map), and must go through all its motion's steps (flying, flat and
// shielded, teleporting, resting at its target2). Where the map has a target_bullethell (d2end), first the pattern
// sweep, once he has landed: its styles 1-8 fired one after another next to him (in god mode meanwhile; each when the
// last one's shots, tasks and marks are gone), each to spawn shots, the aimed line (1) to hit him, the edicts to stay
// within 400 of where they were and come back there.
// Prints "dwbosstest: ..." and "dwhelltest: ..." lines and one verdict:
// "dwbosstest: <classname> phases <n> died <0|1> motion <ok> patterns <ok>: PASS|FAIL".

.float dw_test_phases, dw_test_started, dw_test_seen;
.float dw_test_style, dw_test_emstyle, dw_test_god, dw_test_base, dw_test_peak, dw_test_fired, dw_test_shots0;
.float dw_test_hits, dw_test_hellok, dw_test_restok, dw_test_linehits;
.entity dw_test_em;
.vector dw_test_emorg;

float() DW_Test_Edicts =
{
    float n = 0;
    entity e = nextent(world);
    while(e)
    {
        n = n + 1;
        e = nextent(e);
    }
    return n;
};

// The sweep's emitter next to the player where he is now, at his height: the clearest of 8 ways from him (up to 320
// units, things in the way counted).
void(entity em, entity p) DW_HellSweep_Place =
{
    float k, best = -1;
    vector at = p.origin;
    for(k = 0; k < 8; k++)
    {
        traceline(p.origin, p.origin + DW_Yaw(k * 45) * 320, FALSE, p);
        if(trace_fraction > best)
        {
            best = trace_fraction;
            at = trace_endpos - DW_Yaw(k * 45) * 32;
        }
    }
    setorigin(em, at);
    bprint(sprintf("dwhelltest: emitter %g units from the player (at %v)\n", vlen(at - p.origin), p.origin));
};

// The pattern sweep's step (every 0.1 s); FALSE once it is over (or there is none).
float() DW_HellSweep_Think =
{
    entity em = self.dw_test_em;
    entity p = self.owner;
    if(!em || self.dw_test_style > 8) { return FALSE; }
    self.dw_test_peak = max(self.dw_test_peak, DW_Test_Edicts());
    if(find(world, classname, "dw_helltask") || find(world, classname, "dw_hellshot") || find(world, classname, "dw_hellmark"))
    {
        return TRUE;
    }
    if(!self.dw_test_style && !(p.flags & FL_ONGROUND)) { return TRUE; } // (d2end drops him in: on his feet first)
    if(self.dw_test_style > 0)
    {
        float n = dw_hell_shots - self.dw_test_shots0;
        if(self.dw_test_style == 1) { self.dw_test_linehits = dw_hell_hits - self.dw_test_hits; } // (aimed at him)
        bprint(sprintf("dwhelltest: style %g shots %g hits %g (edicts peak +%g)\n", self.dw_test_style, n,
            dw_hell_hits - self.dw_test_hits, self.dw_test_peak - self.dw_test_base));
        if(n > 0) { self.dw_test_fired = self.dw_test_fired + 1; }
    }
    self.dw_test_style = self.dw_test_style + 1;
    if(self.dw_test_style > 8)
    {
        float left = DW_Test_Edicts() - self.dw_test_base;
        float peak = self.dw_test_peak - self.dw_test_base;
        setorigin(em, self.dw_test_emorg);
        em.style = self.dw_test_emstyle;
        p.flags = (p.flags - (p.flags & FL_GODMODE)) | self.dw_test_god;
        float fine = self.dw_test_fired == 8 && self.dw_test_linehits > 0;
        float tidy = left <= 2 && peak < 400;
        self.dw_test_hellok = (fine && tidy);
        string word = "BAD";
        if(self.dw_test_hellok) { word = "ok"; }
        bprint(sprintf("dwhelltest: patterns %g/8 fired, the line's hits %g, hits %g, edicts peak +%g, left %g: %s\n",
            self.dw_test_fired, self.dw_test_linehits, dw_hell_hits, peak, left, word));
        return FALSE;
    }
    self.dw_test_shots0 = dw_hell_shots;
    self.dw_test_hits = dw_hell_hits;
    em.style = self.dw_test_style;
    DW_HellSweep_Place(em, p);
    entity oself = self;
    self = em;
    activator = p;
    em.use();
    self = oself;
    return TRUE;
};

void() DW_BossTest_Think =
{
    entity b = self.enemy;
    entity p = self.owner;
    self.nextthink = time + 0.1;
    if(DW_HellSweep_Think()) { return; }
    float over = !b || b.classname != self.netname || b.health <= 0; // (gone, or another entity in its slot)
    if(over || time > self.dw_test_started + 600)
    {
        float died = over;
        // the Sculptor: flying (4), flat and shielded (2), teleporting (3), resting (5) at its target2
        float motion = self.netname != "monster_d2boss" || ((self.dw_test_seen & 60) == 60 && self.dw_test_restok);
        float hell = !self.dw_test_em || self.dw_test_hellok;
        if(b) { bprint(sprintf("dwbosstest: (%s health %g state %g)\n", b.classname, b.health, b.state)); }
        bprint(sprintf("dwbosstest: states seen %g, rested at target2 %g\n", self.dw_test_seen, self.dw_test_restok));
        bprint(sprintf("dwbosstest: %s phases %g died %g motion %g patterns %g: %s\n", self.netname, self.dw_test_phases,
            died, motion, hell, (died && self.dw_test_phases >= 3 && motion && hell) ? "PASS" : "FAIL"));
        remove(self);
        return;
    }
    if(b.classname == "monster_d2boss" && b.state < 14)
    {
        float bit = 1;
        float k;
        for(k = 0; k < b.state; k++) { bit = bit * 2; }
        self.dw_test_seen = self.dw_test_seen | bit;
        entity dest = find(world, targetname, b.cu_target2);
        if(b.state == 5 && dest && vlen(b.origin - dest.origin) < 1) { self.dw_test_restok = TRUE; }    }
    entity oself = self;
    self = b;
    activator = p;
    if(self.classname == "monster_d2boss" && self.state == 0)
    {
        bprint("dwbosstest: the Sculptor used\n");
        self.use();
    }
    else if(self.classname == "monster_d1boss" && !self.takedamage && self.frame >= 65 && self.frame <= 87)
    {
        self.enemy = p; // (shown its enemy: it stops channelling)
        self.th_run();
    }
    if(self.takedamage && self.health > 0)
    {
        float before = self.state & 15;
        T_Damage(self, p, p, self.health + 1);
        if(self.health > 0)
        {
            oself.dw_test_phases = oself.dw_test_phases + 1;
            bprint(sprintf("dwbosstest: phase %g over (state %g, was %g)\n", oself.dw_test_phases, self.state & 15, before));
        }
    }
    self = oself;
};

void(entity p) DW_BossTest =
{
    entity b = find(world, classname, "monster_d1boss");
    if(!b) { b = find(world, classname, "monster_d2boss"); }
    if(!b || !p)
    {
        bprint("dwbosstest: no boss (or no player) here: FAIL\n");
        return;
    }
    bprint(sprintf("dwbosstest: %s health %g fight\n", b.classname, b.health));
    if(b.classname == "monster_d2boss") { b.wait = 3; } // (after the map's killboss, 2 s after a phase)
    entity t = spawn();
    t.classname = "dw_bosstest";
    t.netname = b.classname;
    t.enemy = b;
    t.owner = p;
    t.dw_test_started = time;
    entity em = find(world, classname, "target_bullethell");
    if(em) // (the pattern sweep first: its emitter next to the player, at his height)
    {
        t.dw_test_em = em;
        t.dw_test_emorg = em.origin;
        t.dw_test_emstyle = em.style;
        t.dw_test_god = p.flags & FL_GODMODE;
        p.flags = p.flags | FL_GODMODE;
        t.dw_test_base = DW_Test_Edicts();
        t.dw_test_peak = t.dw_test_base;
    }
    t.think = DW_BossTest_Think;
    t.nextthink = time + 0.5;
};
#endif
