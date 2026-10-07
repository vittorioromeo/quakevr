// Dawn of the Machine monsters' tests (vr_mg3_mtest N, developer 1; Debug > Tests > Dawn of the Machine Monsters):
// docs/vr-port/MG3_PLAN.md M3-15..18, docs/vr-port/TESTING.md. Prints "mg3mtest: PASS/FAIL <what>" and a total.
//   1 report: the data, the map's Dawn of the Machine monsters by kind, the kills
//   2 the infected (any campaign): a grunt, a knight, an enforcer and a death knight ahead, each killed: burst into a
//     zombie or a fiend (its model, class, health, box), not counted; then each killed again (gibbed): counted once
//   3 the death knight lying as a corpse (MG3's corpse A): not solid, in his last death frame; woken, he rises
//   4 every infected on the map killed once: each bursts (a zombie or a fiend, or gibbed where wedged), no kill counted

float mg3mt_pass, mg3mt_fail;
float mg3mt_step;      // the running request's step, 0: none
float mg3mt_next;      // when its next step runs
entity mg3mt_m1, mg3mt_m2, mg3mt_m3, mg3mt_m4;
float mg3mt_killed;    // killed_monsters at a step's start
float mg3mt_total;     // total_monsters at a step's start
float mg3mt_kd;        // whether the infected grunt was knocked down (a ragdoll) before he was killed

void(float ok, string label) MG3_MT_Check =
{
    if(ok) mg3mt_pass++; else mg3mt_fail++;
    dprint(sprintf("mg3mtest: %s %s\n", ok ? "PASS" : "FAIL", label));
};

void() MG3_MT_Total =
{
    dprint(sprintf("mg3mtest: total %g passed, %g failed\n", mg3mt_pass, mg3mt_fail));
    mg3mt_pass = 0;
    mg3mt_fail = 0;
};

entity() MG3_MT_Player = { return find(world, classname, "player"); };

// A monster `xChoice` (func_enemy_dispenser's) `xDist` units ahead of the player, `xSide` aside, facing him.
entity(entity p, float xChoice, float xDist, float xSide) MG3_MT_Spawn =
{
    makevectors('0 1 0' * p.v_angle_y);
    entity d = spawn();
    d.weapon = xChoice;
    setorigin(d, p.origin + v_forward * xDist + v_right * xSide);
    entity oldself = self;
    self = d;
    activator = p;
    func_enemy_dispenser_use();
    entity m = self;
    self = oldself;
    remove(d);
    m.angles_y = p.v_angle_y + 180;
    return m;
};

void() MG3_MT_Report =
{
    float n1 = 0, n2 = 0, n3 = 0, n4 = 0;
    entity e = nextent(world);
    while(e)
    {
        if(e.th_stand == MG3_HKnight_Lie && e.solid == SOLID_NOT) { n4 = n4 + 1; }
        if(e.vr_mg3_infected == MG3_INFECTED_ZOMBIE) { n1 = n1 + 1; }
        else if(e.vr_mg3_infected == MG3_INFECTED_FIEND) { n2 = n2 + 1; }
        if((e.flags & FL_MONSTER) && e.health > 0) { n3 = n3 + 1; }
        e = nextent(e);
    }
    dprint(sprintf("mg3mtest: report map %s campaign %g: infected (to zombie %g, to fiend %g; lying %g), turned %g\n",
        mapname, cvar("vr_campaign"), n1, n2, n4, vr_mg3_infected_turned));
    dprint(sprintf("mg3mtest: monsters alive %g, killed %g of %g\n", n3, killed_monsters, total_monsters));
    dprint(sprintf("mg3mtest: the rising death knight's sound %s\n", MG3_Infected_RiseSound()));
};

// `m` killed by `p` with `xDamage` (a blow's: Killed through T_Damage).
void(entity p, entity m, float xDamage) MG3_MT_Hurt =
{
    T_Damage(m, p, p, xDamage);
};

void(entity m, string xFrom, string xClass, string xModel, float xHealth) MG3_MT_CheckTurned =
{
    MG3_MT_Check(m.classname == xClass && m.model == xModel && m.health == xHealth && !m.vr_mg3_infected &&
        m.solid == SOLID_SLIDEBOX && m.takedamage == DAMAGE_AIM && !m.vr_headless,
        sprintf("%s burst into a %s (%s %s health %g solid %g)", xFrom, xClass, m.classname, m.model, m.health, m.solid));
};

void() MG3_MT_Step =
{
    entity p = MG3_MT_Player();
    if(!p) { mg3mt_step = 0; return; }
    if(mg3mt_step == 1)
    {
        mg3mt_total = total_monsters;
        mg3mt_m1 = MG3_MT_Spawn(p, 30, 96, 0); // (in a line ahead: e1m1's start corridor, MG3's map rooms)
        mg3mt_m2 = MG3_MT_Spawn(p, 31, 176, 0);
        mg3mt_m3 = MG3_MT_Spawn(p, 32, 256, 0);
        mg3mt_m4 = MG3_MT_Spawn(p, 33, 336, 0);
        MG3_MT_Check(total_monsters == mg3mt_total + 4, sprintf("four infected counted among the monsters (%g -> %g)",
            mg3mt_total, total_monsters));
        MG3_MT_Check(mg3mt_m1.classname == "monster_army" && mg3mt_m1.vr_mg3_infected == MG3_INFECTED_ZOMBIE &&
            mg3mt_m2.classname == "monster_knight" && mg3mt_m3.classname == "monster_enforcer" &&
            mg3mt_m4.classname == "monster_hell_knight" && mg3mt_m4.vr_mg3_infected == MG3_INFECTED_FIEND,
            "they are a grunt, a knight, an enforcer and a death knight, infected");
        mg3mt_step = 5; mg3mt_next = time + 0.7; // (they start: walkmonster_start_go)
        return;
    }
    if(mg3mt_step == 5)
    {
        mg3mt_kd = VR_Knockdown_Start(mg3mt_m1); // (vr_ragdoll 1: the grunt down as a ragdoll, killed down there)
        dprint(sprintf("mg3mtest: the infected grunt knocked down %g\n", mg3mt_kd));
        mg3mt_step = 2; mg3mt_next = time + 1;
        return;
    }
    if(mg3mt_step == 2)
    {
        mg3mt_killed = killed_monsters;
        float turned = vr_mg3_infected_turned;
        MG3_MT_Hurt(p, mg3mt_m1, mg3mt_m1.health + 5);
        // The knight killed by a beheading cut (armed as a sword's swing at his head arms it): he bursts, his zombie
        // keeps its head (VR_Decap_After, after the blow, finds nothing armed).
        makevectors('0 1 0' * p.v_angle_y);
        float armed = VR_Decap_Arm(mg3mt_m2, p, mg3mt_m2.origin + '0 0 20', v_right * 400, QVR_DECAP_SWING);
        MG3_MT_Hurt(p, mg3mt_m2, mg3mt_m2.health + 5);
        VR_Decap_After(mg3mt_m2);
        MG3_MT_Check(armed && mg3mt_m2.health > 0 && !mg3mt_m2.vr_headless && mg3mt_m2.classname == "monster_zombie",
            sprintf("a beheading cut bursts the knight; his zombie keeps its head (armed %g health %g headless %g)", armed,
            mg3mt_m2.health, mg3mt_m2.vr_headless));
        MG3_MT_Check(mg3mt_m1.vr_knockdown == 0 && mg3mt_m1.solid == SOLID_SLIDEBOX && mg3mt_m1.movetype == MOVETYPE_STEP,
            sprintf("the grunt killed down (knocked down %g) gets up as a zombie (knockdown %g solid %g)", mg3mt_kd,
            mg3mt_m1.vr_knockdown, mg3mt_m1.solid));
        MG3_MT_Hurt(p, mg3mt_m3, mg3mt_m3.health + 5);
        MG3_MT_Hurt(p, mg3mt_m4, mg3mt_m4.health + 5);
        MG3_MT_Check(vr_mg3_infected_turned == turned + 4, sprintf("four turned (%g)", vr_mg3_infected_turned - turned));
        MG3_MT_Check(killed_monsters == mg3mt_killed, sprintf("no kill counted for the bursts (%g -> %g)", mg3mt_killed,
            killed_monsters));
        MG3_MT_CheckTurned(mg3mt_m1, "the grunt", "monster_zombie", "progs/zombie.mdl", 60);
        MG3_MT_CheckTurned(mg3mt_m2, "the knight", "monster_zombie", "progs/zombie.mdl", 60);
        MG3_MT_CheckTurned(mg3mt_m3, "the enforcer", "monster_demon1", "progs/demon.mdl", 300);
        MG3_MT_CheckTurned(mg3mt_m4, "the death knight", "monster_demon1", "progs/demon.mdl", 300);
        MG3_MT_Check(mg3mt_m3.maxs_x == 16 && mg3mt_m1.maxs_x == 12, sprintf("their boxes the fiend's and the zombie's (%v %v)",
            mg3mt_m3.maxs, mg3mt_m1.maxs));
        mg3mt_step = 3; mg3mt_next = time + 1.5;
        return;
    }
    if(mg3mt_step == 3)
    {
        MG3_MT_Check(mg3mt_m1.health > 0 && mg3mt_m3.health > 0 && mg3mt_m1.th_die == zombie_die && mg3mt_m3.th_die == demon_die,
            sprintf("alive a second later, a zombie's and a fiend's death (%g %g)", mg3mt_m1.health, mg3mt_m3.health));
        // Killed again, gibbed: counted now, once each.
        MG3_MT_Hurt(p, mg3mt_m1, 200);
        MG3_MT_Hurt(p, mg3mt_m2, 200);
        MG3_MT_Hurt(p, mg3mt_m3, 500);
        MG3_MT_Hurt(p, mg3mt_m4, 500);
        MG3_MT_Check(killed_monsters == mg3mt_killed + 4, sprintf("killed again: four kills counted (%g -> %g)",
            mg3mt_killed, killed_monsters));
        MG3_MT_Check(mg3mt_total + 4 == total_monsters, sprintf("the monster total unchanged by the bursts (%g)", total_monsters));
        mg3mt_step = 0;
        MG3_MT_Total();
        return;
    }
    if(mg3mt_step == 10)
    {
        cvar_set("vr_test_spawn_flags", ftos(MG3_CORPSE_A));
        mg3mt_m1 = MG3_MT_Spawn(p, 33, 128, 0);
        cvar_set("vr_test_spawn_flags", "0");
        mg3mt_step = 11; mg3mt_next = time + 1.5;
        return;
    }
    if(mg3mt_step == 11)
    {
        MG3_MT_Check(mg3mt_m1.solid == SOLID_NOT && mg3mt_m1.frame == MG3_HKNIGHT_DEATH1 + 11 && mg3mt_m1.health > 0,
            sprintf("the death knight lies as a corpse (solid %g frame %g health %g)", mg3mt_m1.solid, mg3mt_m1.frame,
            mg3mt_m1.health));
        entity os = self;
        self = mg3mt_m1;
        activator = p;
        monster_use();
        self = os;
        mg3mt_step = 12; mg3mt_next = time + 2.5;
        return;
    }
    if(mg3mt_step == 12)
    {
        MG3_MT_Check(mg3mt_m1.solid == SOLID_SLIDEBOX && mg3mt_m1.th_pain == hknight_pain && mg3mt_m1.th_run == hknight_run1 &&
            (mg3mt_m1.frame < MG3_HKNIGHT_DEATH1 || mg3mt_m1.frame > MG3_HKNIGHT_DEATHB1 + 8),
            sprintf("woken, he rose and fights (solid %g frame %g)", mg3mt_m1.solid, mg3mt_m1.frame));
        MG3_MT_Hurt(p, mg3mt_m1, mg3mt_m1.health + 5);
        MG3_MT_Check(mg3mt_m1.classname == "monster_demon1", "killed, he bursts into a fiend");
        mg3mt_step = 0;
        MG3_MT_Total();
        return;
    }
    mg3mt_step = 0;
};

// Every infected on the map killed once (where they stand: asleep, lying or not): each bursts, none counted.
void() MG3_MT_KillInfected =
{
    entity p = MG3_MT_Player();
    float killed = killed_monsters, n = 0, turned = vr_mg3_infected_turned;
    entity e = nextent(world);
    while(e)
    {
        entity nx = nextent(e);
        if(e.vr_mg3_infected && e.health > 0 && e.takedamage)
        {
            n = n + 1;
            MG3_MT_Hurt(p, e, e.health + 5);
        }
        e = nx;
    }
    MG3_MT_Check(vr_mg3_infected_turned - turned == n && killed_monsters == killed, sprintf("the map's %g infected killed: "
        "%g turned, kills %g -> %g", n, vr_mg3_infected_turned - turned, killed, killed_monsters));
    MG3_MT_Total();
};

void() MG3_MonstersTestFrame =
{
    if(mg3mt_step && time >= mg3mt_next)
    {
        entity os = self, oo = other, oa = activator;
        MG3_MT_Step();
        self = os; other = oo; activator = oa;
    }
    float request = cvar("vr_mg3_mtest");
    if(!request) return;
    cvar_set("vr_mg3_mtest", "0");
    if(!MG3_MT_Player()) return;
    entity oldself = self, oldother = other, oldact = activator;
    if(request == 1) MG3_MT_Report();
    else if(request == 2) { mg3mt_step = 1; mg3mt_next = time; }
    else if(request == 3) { mg3mt_step = 10; mg3mt_next = time; }
    else if(request == 4) MG3_MT_KillInfected();
    self = oldself; other = oldother; activator = oldact;
};
