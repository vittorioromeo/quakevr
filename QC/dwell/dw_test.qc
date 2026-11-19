#ifdef QVR_TESTS
// dw_test.qc -- Dwell support (DWELL.md P2a): vr_ad_test 200, the map's Dwell boss fought to its end (d1end's Awoken,
// d2end's Sculptor): woken as the map would (the Sculptor used, as its fight_phase1 relay does; the Awoken shown its
// enemy), then a killing blow each time it can be hurt, through every phase, until it dies. The Sculptor rests 3 s
// between phases here (its `wait`: 40 in the map). Prints "dwbosstest: ..." lines and one verdict:
// "dwbosstest: <classname> phases <n> died <0|1>: PASS|FAIL".

.float dw_test_phases, dw_test_started;

void() DW_BossTest_Think =
{
    entity b = self.enemy;
    entity p = self.owner;
    self.nextthink = time + 0.1;
    float over = !b || b.classname != self.netname || b.health <= 0; // (gone, or another entity in its slot)
    if(over || time > self.dw_test_started + 600)
    {
        float died = over;
        if(b) { bprint(sprintf("dwbosstest: (%s health %g state %g)\n", b.classname, b.health, b.state)); }
        bprint(sprintf("dwbosstest: %s phases %g died %g: %s\n", self.netname, self.dw_test_phases, died,
            (died && self.dw_test_phases >= 3) ? "PASS" : "FAIL"));
        remove(self);
        return;
    }
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
    t.think = DW_BossTest_Think;
    t.nextthink = time + 0.5;
};
#endif
