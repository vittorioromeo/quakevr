// Dawn of the Machine monsters' tests, M3-19..23 (vr_mg3_btest N, developer 1; Debug > Tests > Dawn of the Machine
// Bestiary): docs/vr-port/MG3_PLAN.md, docs/vr-port/TESTING.md. Prints "mg3btest: PASS/FAIL <what>" and a total.
//   1 report: the data each monster needs, and how many of each this map has (alive, dead)
//   2 the orb (a few seconds): one spawned ahead, its eye behind, its bursts of spheres at you (god mode), its pain,
//     killed: flung, blown up where it lands, counted once; the dummy type, the spawner, the grapple's mass

float mg3bt_pass, mg3bt_fail;
float mg3bt_step;  // the running request's step, 0: none
float mg3bt_next;  // when its next step runs
entity mg3bt_a;
float mg3bt_n, mg3bt_n2; // counts read in a step before

void(float ok, string label) MG3_BT_Check =
{
    if(ok) mg3bt_pass++; else mg3bt_fail++;
    dprint(sprintf("mg3btest: %s %s\n", ok ? "PASS" : "FAIL", label));
};

void() MG3_BT_Total =
{
    dprint(sprintf("mg3btest: total %g/%g\n", mg3bt_pass, mg3bt_fail));
    mg3bt_step = 0;
};

entity() MG3_BT_Player = { return find(world, classname, "player"); };

// A monster `xChoice` (func_enemy_dispenser's: 40 orb ...) `xDist` units ahead of the player, `xSide` aside, `xUp` up.
entity(entity p, float xChoice, float xDist, float xSide, float xUp) MG3_BT_Spawn =
{
    makevectors('0 1 0' * p.v_angle_y);
    entity d = spawn();
    d.weapon = xChoice;
    setorigin(d, p.origin + v_forward * xDist + v_right * xSide + '0 0 1' * xUp);
    entity oldself = self;
    self = d;
    activator = p;
    func_enemy_dispenser_use();
    entity m = self;
    self = oldself;
    remove(d);
    return m;
};

// How many `xClass` this map has: alive (mg3bt_n2: dead).
float(string xClass) MG3_BT_Count =
{
    float alive = 0;
    mg3bt_n2 = 0;
    entity e = find(world, classname, xClass);
    while(e)
    {
        if(e.health > 0 || e.MG_deferred) alive++; else mg3bt_n2++;
        e = find(e, classname, xClass);
    }
    return alive;
};

void() MG3_BT_Report =
{
    dprint(sprintf("mg3btest: report map %s campaign %g mg3 data: orb %g\n", mapname, cvar("vr_campaign"), MG3_Orb_Available()));
    float n = MG3_BT_Count("monster_orb");
    dprint(sprintf("mg3btest: monster_orb %g alive %g dead; monsters %g/%g\n", n, mg3bt_n2, killed_monsters, total_monsters));
};

// `m` wakes at `p` (as a sight of it would).
void(entity m, entity p) MG3_BT_Wake =
{
    entity oldself = self;
    self = m;
    self.enemy = p;
    FoundTarget();
    self = oldself;
};

void() MG3_BT_OrbStep =
{
    entity p = MG3_BT_Player();
    entity oldself = self;
    if(mg3bt_step == 1)
    {
        mg3bt_pass = mg3bt_fail = 0;
        p.flags = p.flags | FL_GODMODE;
        float total = total_monsters;
        mg3bt_a = MG3_BT_Spawn(p, 40, 160, 0, 48);
        MG3_BT_Check(mg3bt_a.classname == "monster_orb" && mg3bt_a.model == "owned/mg3/progs/teleporter_eye_blink.mdl",
            sprintf("an orb ahead (%s, %s)", mg3bt_a.classname, mg3bt_a.model));
        MG3_BT_Check(mg3bt_a.health == 300 && total_monsters == total + 1, sprintf("300 health (%g), counted (%g)", mg3bt_a.health, total_monsters - total));
        MG3_BT_Check(VR_Grapple_MonsterMass(mg3bt_a) == 120, "the grapple weighs it 120 kg");
        MG3_BT_Check(VR_Dummy_Available(30), "the training dummy can stand as one (type 30)");
        mg3bt_step = 2; mg3bt_next = time + 1;
        return;
    }
    if(mg3bt_step == 2)
    {
        MG3_BT_Check((mg3bt_a.flags & FL_FLY) && (mg3bt_a.flags & FL_MONSTER) && mg3bt_a.takedamage, "it flies, a monster, hurt by shots");
        // Its eye at its back: facing away, it still sees you; a scrag facing away does not.
        self = mg3bt_a;
        self.angles_y = vectoyaw(self.origin - p.origin);
        float behind = infront(p);
        self.angles_y = vectoyaw(p.origin - self.origin);
        float ahead = infront(p);
        self.angles_y = 90 + vectoyaw(p.origin - self.origin);
        float side = infront(p);
        self = oldself;
        MG3_BT_Check(behind && ahead && !side, sprintf("its eyes: ahead %g, behind %g, aside %g", ahead, behind, side));
        mg3bt_n = mg3_orb_spheres;
        mg3bt_n2 = p.health;
        MG3_BT_Wake(mg3bt_a, p);
        mg3bt_step = 3; mg3bt_next = time + 6;
        return;
    }
    if(mg3bt_step == 3)
    {
        float fired = mg3_orb_spheres - mg3bt_n;
        dprint(sprintf("mg3btest: in 6 s: %g spheres, enemy %s, state %g, frame %g\n", fired, mg3bt_a.enemy.classname, mg3bt_a.attack_state, mg3bt_a.frame));
        MG3_BT_Check(fired >= 4 && mg3bt_a.enemy == p, sprintf("awake, it bursts spheres at you (%g in 6 s)", fired));
        // A hard hit: it flinches (eyes shut), throws lightning.
        self = mg3bt_a;
        self.pain_finished = 0;
        Orb_Pain(p, 250);
        self = oldself;
        MG3_BT_Check(mg3bt_a.pain_finished > time && mg3bt_a.think == orb_pain2, "a hard hit: its pain (lightning about it)");
        mg3bt_step = 4; mg3bt_next = time + 1.2;
        return;
    }
    if(mg3bt_step == 4)
    {
        mg3bt_n = killed_monsters;
        mg3bt_n2 = mg3_orb_blowups;
        T_Damage(mg3bt_a, p, p, 1000);
        MG3_BT_Check(mg3bt_a.health <= 0 && killed_monsters == mg3bt_n + 1, "killed, counted once");
        MG3_BT_Check(!(mg3bt_a.flags & FL_FLY) && vlen(mg3bt_a.velocity) > 100 && mg3bt_a.size == '0 0 0',
            sprintf("flung (%v), falling", mg3bt_a.velocity));
        mg3bt_step = 5; mg3bt_next = time + 4;
        return;
    }
    if(mg3bt_step == 5)
    {
        MG3_BT_Check(mg3_orb_blowups == mg3bt_n2 + 1 && killed_monsters == mg3bt_n + 1,
            sprintf("it blew up where it landed (%g), still counted once", mg3_orb_blowups - mg3bt_n2));
        p.flags = p.flags - (p.flags & FL_GODMODE);
        MG3_BT_Total();
        return;
    }
};

// Each frame (world.qc StartFrame).
void() MG3_BestiaryTestFrame =
{
    if(mg3bt_step && time >= mg3bt_next)
    {
        entity os = self, oo = other, oa = activator;
        if(mg3bt_step < 10) MG3_BT_OrbStep();
        self = os; other = oo; activator = oa;
    }
    float request = cvar("vr_mg3_btest");
    if(!request) return;
    cvar_set("vr_mg3_btest", "0");
    if(!MG3_BT_Player()) return;
    entity oldself = self, oldother = other, oldact = activator;
    if(request == 1) MG3_BT_Report();
    else if(request == 2)
    {
        if(!MG3_Orb_Available()) dprint("mg3btest: the Dawn of the Machine data is not there (no orb)\n");
        else { mg3bt_step = 1; mg3bt_next = time; }
    }
    self = oldself; other = oldother; activator = oldact;
};
