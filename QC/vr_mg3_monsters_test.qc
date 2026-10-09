// Dawn of the Machine monsters' tests (vr_mg3_mtest N, developer 1; Debug > Tests > Dawn of the Machine Monsters):
// docs/vr-port/MG3.md M3-15..18, docs/vr-port/TESTING.md. Prints "mg3mtest: PASS/FAIL <what>" and a total.
//   1 report: the data, the map's Dawn of the Machine monsters by kind, the kills
//   2 the infected (any campaign): a grunt, a knight, an enforcer and a death knight ahead, each killed: burst into a
//     zombie or a fiend (its model, class, health, box), not counted; then each killed again (gibbed): counted once
//   3 the death knight lying as a corpse (MG3's corpse A): not solid, in his last death frame; woken, he rises
//   4 every infected on the map killed once: each bursts (a zombie or a fiend, or gibbed where wedged), no kill counted
//   5 the rocket ogre (Dawn of the Machine's data): spawned ahead (its model, class, head zone), made to shoot: a volley
//     of two rockets (batted as a monster's missile), the player hurt; killed: its corpse
//   6 the demo dog (Dawn of the Machine's data): one shot dead beside a grunt: its grenades spill, it bursts (gibs, its
//     head), counted; 3 s later the grunt is hurt; a second one leaps onto the player: dies, spills, the player hurt; a
//     third beheaded: spills, lies headless (its ragdoll)
//   7 the ranged knight (Dawn of the Machine's data): spawned ahead (its model, class, head zone, no melee), made to
//     cast a fan: six diamonds in flight (MG3's model; batted as a monster's missile), the player hurt; killed: its
//     death frames; another gibbed: the death knight's head

float mg3mt_pass, mg3mt_fail;
float mg3mt_step;      // the running request's step, 0: none
float mg3mt_next;      // when its next step runs
entity mg3mt_m1, mg3mt_m2, mg3mt_m3, mg3mt_m4;
float mg3mt_killed;    // killed_monsters at a step's start
float mg3mt_total;     // total_monsters at a step's start
float mg3mt_kd;        // whether the infected grunt was knocked down (a ragdoll) before he was killed
float mg3mt_h;         // the player's health at a step's start
float mg3mt_n;         // a count at a step's start (rockets fired)

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
    float n1 = 0, n2 = 0, n3 = 0, n4 = 0, n5 = 0, n6 = 0, n7 = 0;
    entity e = nextent(world);
    while(e)
    {
        if(e.th_stand == MG3_HKnight_Lie && e.solid == SOLID_NOT) { n4 = n4 + 1; }
        if(e.vr_mg3_mon == MG3_MON_ROCKET_OGRE && e.health > 0) { n5 = n5 + 1; }
        if(e.vr_mg3_mon == MG3_MON_DEMODOG && e.health > 0) { n6 = n6 + 1; }
        if(e.vr_mg3_mon == MG3_MON_RANGED_KNIGHT && e.health > 0) { n7 = n7 + 1; }
        if(e.vr_mg3_infected == MG3_INFECTED_ZOMBIE) { n1 = n1 + 1; }
        else if(e.vr_mg3_infected == MG3_INFECTED_FIEND) { n2 = n2 + 1; }
        if((e.flags & FL_MONSTER) && e.health > 0) { n3 = n3 + 1; }
        e = nextent(e);
    }
    dprint(sprintf("mg3mtest: report map %s campaign %g: infected (to zombie %g, to fiend %g; lying %g), turned %g\n",
        mapname, cvar("vr_campaign"), n1, n2, n4, vr_mg3_infected_turned));
    dprint(sprintf("mg3mtest: monsters alive %g, killed %g of %g\n", n3, killed_monsters, total_monsters));
    dprint(sprintf("mg3mtest: the rising death knight's sound %s\n", MG3_Infected_RiseSound()));
    dprint(sprintf("mg3mtest: rocket ogres %g, demo dogs %g, ranged knights %g (alive)\n", n5, n6, n7));
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
        MG3_MT_Check(mg3mt_m3.maxs_x == 16 && (mg3mt_m1.maxs_x == 12 || mg3mt_m1.maxs_x == 16), sprintf("their boxes the "
            "fiend's and the zombie's, or (crowded) the old ones (%v %v)",
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
    if(mg3mt_step == 20)
    {
        if(!MG3_RocketOgre_Available())
        {
            dprint("mg3mtest: the Dawn of the Machine data is not there (no rocket ogre)\n");
            mg3mt_step = 0;
            return;
        }
        mg3mt_m1 = MG3_MT_Spawn(p, 34, 256, 0);
        MG3_MT_Check(mg3mt_m1.classname == "monster_ogre" && mg3mt_m1.vr_mg3_mon == MG3_MON_ROCKET_OGRE &&
            mg3mt_m1.model == MG3_OGRE_ROCKET_MDL && mg3mt_m1.health == 200, sprintf("a rocket ogre (%s %s health %g)",
            mg3mt_m1.classname, mg3mt_m1.model, mg3mt_m1.health));
        MG3_MT_Check(PositionalHead(mg3mt_m1) && xHeadUp == 33 && xHeadForward == 10, sprintf("its own head zone (%g up, %g "
            "forward, %g wide)", xHeadUp, xHeadForward, xHeadRadius));
        mg3mt_step = 21; mg3mt_next = time + 1;
        return;
    }
    if(mg3mt_step == 21)
    {
        p.health = 500;
        mg3mt_h = p.health;
        mg3mt_n = vr_mg3_rockets;
        mg3mt_m1.enemy = p;
        entity os2 = self;
        self = mg3mt_m1;
        ai_face();
        ogre_nail1();
        self = os2;
        mg3mt_step = 22; mg3mt_next = time + 0.45;
        return;
    }
    if(mg3mt_step == 22)
    {
        entity r = find(world, classname, "ogre_missile");
        MG3_MT_Check(r != world && VR_Deflect_IsProjectile(r) && r.owner == mg3mt_m1, sprintf("its rocket in flight, a "
            "monster's missile to bat (%g)", r != world));
        mg3mt_step = 23; mg3mt_next = time + 1.5;
        return;
    }
    if(mg3mt_step == 23)
    {
        float fired = vr_mg3_rockets - mg3mt_n; // (one volley made, more of its own may follow: it has an enemy now)
        MG3_MT_Check(fired >= 2 && fired == floor(fired / 2) * 2, sprintf("volleys of two rockets (%g)", fired));
        MG3_MT_Check(p.health < mg3mt_h, sprintf("the player hurt (%g -> %g)", mg3mt_h, p.health));
        MG3_MT_Hurt(p, mg3mt_m1, mg3mt_m1.health + 5);
        mg3mt_step = 24; mg3mt_next = time + 2;
        return;
    }
    if(mg3mt_step == 24)
    {
        MG3_MT_Check(mg3mt_m1.health <= 0 && mg3mt_m1.model == MG3_OGRE_ROCKET_MDL && mg3mt_m1.frame >= 112 &&
            mg3mt_m1.frame <= 135, sprintf("killed: a corpse in its death frames (frame %g)", mg3mt_m1.frame));
        float saws = 0;
        entity w = find(world, classname, "thrown_weapon");
        while(w) { if(w.weapon == WID_CHAINSAW) { saws = saws + 1; } w = find(w, classname, "thrown_weapon"); }
        MG3_MT_Check(saws == 1, sprintf("its chainsaw dropped (%g)", saws));
        mg3mt_step = 0;
        MG3_MT_Total();
        return;
    }
    if(mg3mt_step == 30)
    {
        if(!MG3_DemoDog_Available())
        {
            dprint("mg3mtest: the Dawn of the Machine data is not there (no demo dog)\n");
            mg3mt_step = 0;
            return;
        }
        mg3mt_m1 = MG3_MT_Spawn(p, 35, 192, 0);
        mg3mt_m2 = MG3_MT_Spawn(p, 0, 192, 40); // (a grunt beside it)
        mg3mt_m2.health = 300; // (survives a grenade or two: hurt, not gone)
        MG3_MT_Check(mg3mt_m1.classname == "monster_dog" && mg3mt_m1.vr_mg3_mon == MG3_MON_DEMODOG &&
            mg3mt_m1.model == MG3_DEMODOG_MDL && mg3mt_m1.health == 25, sprintf("a demo dog (%s %s health %g)",
            mg3mt_m1.classname, mg3mt_m1.model, mg3mt_m1.health));
        mg3mt_step = 31; mg3mt_next = time + 0.7;
        return;
    }
    if(mg3mt_step == 31)
    {
        mg3mt_n = vr_mg3_demo_grenades;
        mg3mt_killed = killed_monsters;
        mg3mt_h = mg3mt_m2.health;
        mg3mt_total = vr_mg3_demo_blasts;
        vr_mg3_demo_watch = mg3mt_m2;
        vr_mg3_demo_nearest = 100000;
        MG3_MT_Hurt(p, mg3mt_m1, 30); // (a shotgun blast's worth: not a gib for a rottweiler)
        MG3_MT_Check(vr_mg3_demo_grenades - mg3mt_n >= 3, sprintf("shot dead: its grenades spill (%g)",
            vr_mg3_demo_grenades - mg3mt_n));
        MG3_MT_Check(mg3mt_m1.model == "progs/h_dog.mdl" && killed_monsters == mg3mt_killed + 1, sprintf("it burst (%s), "
            "counted (%g -> %g)", mg3mt_m1.model, mg3mt_killed, killed_monsters));
        mg3mt_step = 32; mg3mt_next = time + 3.2;
        return;
    }
    if(mg3mt_step == 32)
    {
        MG3_MT_Check(vr_mg3_demo_blasts - mg3mt_total == vr_mg3_demo_grenades - mg3mt_n, sprintf("its grenades went off (%g)",
            vr_mg3_demo_blasts - mg3mt_total));
        // (Tossed about at random: the grunt is hurt when one goes off within its reach, 60 + 40 units.)
        MG3_MT_Check(mg3mt_m2.health < mg3mt_h || vr_mg3_demo_nearest > 100, sprintf("the grunt beside it hurt by the "
            "nearest (%g units off): %g -> %g", vr_mg3_demo_nearest, mg3mt_h, mg3mt_m2.health));
        vr_mg3_demo_watch = world;
        if(mg3mt_m2.health > 0) { MG3_MT_Hurt(p, mg3mt_m2, mg3mt_m2.health + 50); }
        // The second: it leaps onto the player.
        p.health = 500;
        mg3mt_h = p.health;
        mg3mt_n = vr_mg3_demo_grenades;
        mg3mt_m1 = MG3_MT_Spawn(p, 35, 128, 0);
        mg3mt_step = 33; mg3mt_next = time + 0.7;
        return;
    }
    if(mg3mt_step == 33)
    {
        // Its leap's landing on the player, as the engine calls it (Dog_JumpTouch): flying at him at 400 u/s. (A real
        // leap may fall short or turn into a bite: the AI's choice, not what is tested.)
        entity os3 = self, oo3 = other;
        self = mg3mt_m1;
        self.enemy = p;
        self.velocity = normalize(p.origin - self.origin) * 400;
        self.flags = self.flags - (self.flags & FL_ONGROUND);
        other = p;
        Dog_JumpTouch();
        self = os3; other = oo3;
        mg3mt_step = 34; mg3mt_next = time + 0.5;
        return;
    }
    if(mg3mt_step == 34)
    {
        MG3_MT_Check(mg3mt_m1.health <= 0 && vr_mg3_demo_grenades - mg3mt_n >= 3 && p.health < mg3mt_h,
            sprintf("its leap onto the player killed it: health %g, grenades %g, the player %g -> %g", mg3mt_m1.health,
            vr_mg3_demo_grenades - mg3mt_n, mg3mt_h, p.health));
        mg3mt_h = p.health;
        vr_mg3_demo_watch = p;
        vr_mg3_demo_nearest = 100000;
        mg3mt_step = 37; mg3mt_next = time + 3;
        return;
    }
    if(mg3mt_step == 37)
    {
        // (Reported, not checked: tossed at random, the nearest may still be behind a step or the player's own cover.)
        dprint(sprintf("mg3mtest: its grenades at the player's feet: the nearest %g units off, the player %g -> %g\n",
            vr_mg3_demo_nearest, mg3mt_h, p.health));
        vr_mg3_demo_watch = world;
        mg3mt_step = 35; mg3mt_next = time;
        return;
    }
    if(mg3mt_step == 35)
    {
        // The third: beheaded (a sword's cut at its head kills it).
        mg3mt_m1 = MG3_MT_Spawn(p, 35, 96, 0);
        mg3mt_step = 36; mg3mt_next = time + 0.7;
        return;
    }
    if(mg3mt_step == 36)
    {
        mg3mt_n = vr_mg3_demo_grenades;
        makevectors('0 1 0' * p.v_angle_y);
        PositionalHead(mg3mt_m1);
        makevectors('0 1 0' * mg3mt_m1.angles_y);
        vector head = mg3mt_m1.origin + v_forward * xHeadForward + '0 0 1' * xHeadUp;
        makevectors('0 1 0' * p.v_angle_y);
        float armed = VR_Decap_Arm(mg3mt_m1, p, head, v_right * 400, QVR_DECAP_SWING);
        MG3_MT_Hurt(p, mg3mt_m1, 30);
        VR_Decap_After(mg3mt_m1);
        MG3_MT_Check(armed && mg3mt_m1.vr_headless && mg3mt_m1.model == MG3_DEMODOG_MDL && vr_mg3_demo_grenades - mg3mt_n >= 3,
            sprintf("beheaded: grenades %g, it lies headless (%s headless %g)", vr_mg3_demo_grenades - mg3mt_n,
            mg3mt_m1.model, mg3mt_m1.vr_headless));
        mg3mt_step = 0;
        MG3_MT_Total();
        return;
    }
    if(mg3mt_step == 40)
    {
        if(!MG3_RKnight_Available())
        {
            dprint("mg3mtest: the Dawn of the Machine data is not there (no ranged knight)\n");
            mg3mt_step = 0;
            return;
        }
        mg3mt_m1 = MG3_MT_Spawn(p, 36, 256, 0);
        MG3_MT_Check(mg3mt_m1.classname == "monster_ranged_knight" && mg3mt_m1.vr_mg3_mon == MG3_MON_RANGED_KNIGHT &&
            mg3mt_m1.model == MG3_RKNIGHT_MDL && mg3mt_m1.health == 250 && !mg3mt_m1.th_melee, sprintf("a ranged knight "
            "(%s %s health %g, no melee)", mg3mt_m1.classname, mg3mt_m1.model, mg3mt_m1.health));
        MG3_MT_Check(PositionalHead(mg3mt_m1) && xHeadUp == 33 && xHeadForward == 7 && VR_Decap_HeadModel(mg3mt_m1) ==
            "progs/h_hellkn.mdl", sprintf("its own head zone (%g up, %g forward, %g wide), the death knight's head",
            xHeadUp, xHeadForward, xHeadRadius));
        mg3mt_step = 41; mg3mt_next = time + 1;
        return;
    }
    if(mg3mt_step == 41)
    {
        p.health = 500;
        mg3mt_h = p.health;
        mg3mt_n = vr_mg3_diamonds;
        mg3mt_m1.enemy = p;
        entity os4 = self;
        self = mg3mt_m1;
        ai_face();
        rknight_magicc1();
        self = os4;
        mg3mt_step = 42; mg3mt_next = time + 0.8;
        return;
    }
    if(mg3mt_step == 42)
    {
        entity d = find(world, classname, "knightspike");
        MG3_MT_Check(d != world && d.model == MG3_DIAMOND_MDL && d.owner == mg3mt_m1 && VR_Deflect_IsProjectile(d),
            sprintf("a diamond in flight, a monster's missile to bat (%s)", d.model));
        mg3mt_step = 43; mg3mt_next = time + 1.5;
        return;
    }
    if(mg3mt_step == 43)
    {
        float fan = vr_mg3_diamonds - mg3mt_n;
        MG3_MT_Check(fan >= 6, sprintf("a fan of six diamonds (%g so far)", fan));
        MG3_MT_Check(p.health < mg3mt_h, sprintf("the player hurt (%g -> %g)", mg3mt_h, p.health));
        MG3_MT_Hurt(p, mg3mt_m1, mg3mt_m1.health + 5);
        mg3mt_m2 = MG3_MT_Spawn(p, 36, 160, 48);
        mg3mt_step = 44; mg3mt_next = time + 2;
        return;
    }
    if(mg3mt_step == 44)
    {
        MG3_MT_Check(mg3mt_m1.health <= 0 && mg3mt_m1.model == MG3_RKNIGHT_MDL && mg3mt_m1.frame >= 42 && mg3mt_m1.frame <= 62,
            sprintf("killed: a corpse in its death frames (frame %g)", mg3mt_m1.frame));
        MG3_MT_Hurt(p, mg3mt_m2, mg3mt_m2.health + 100);
        MG3_MT_Check(mg3mt_m2.model == "progs/h_hellkn.mdl", sprintf("another gibbed: the death knight's head (%s)",
            mg3mt_m2.model));
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
    else if(request == 5) { mg3mt_step = 20; mg3mt_next = time; }
    else if(request == 6) { mg3mt_step = 30; mg3mt_next = time; }
    else if(request == 7) { mg3mt_step = 40; mg3mt_next = time; }
    self = oldself; other = oldother; activator = oldact;
};
