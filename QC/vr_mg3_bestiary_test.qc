// Dawn of the Machine monsters' tests, M3-19..23 (vr_mg3_btest N, developer 1; Debug > Tests > Dawn of the Machine
// Bestiary): docs/vr-port/MG3_PLAN.md, docs/vr-port/TESTING.md. Prints "mg3btest: PASS/FAIL <what>" and a total.
//   1 report: the data each monster needs, and how many of each this map has (alive, dead)
//   2 the orb (a few seconds): one spawned ahead, its eye behind, its bursts of spheres at you (god mode), its pain,
//     killed: flung, blown up where it lands, counted once; the dummy type, the spawner, the grapple's mass
//   3 a ghost laid to rest by a touch and one by a blow, a sacrifice struck down (its target fired), a sacrifice counter,
//     a slime's two generations (blobs, children counted as they start, every one killed: the kills match the count)
//   4 (Dawn of the Machine's map8) each of the map's victims used: the counter (8) counts down, its target fired once

float mg3bt_pass, mg3bt_fail;
float mg3bt_step;  // the running request's step, 0: none
float mg3bt_next;  // when its next step runs
entity mg3bt_a, mg3bt_b;
float mg3bt_n, mg3bt_n2; // counts read in a step before
float mg3bt_t, mg3bt_k;   // total and killed monsters before
float mg3bt_used;         // uses of the tests' target (MG3_BT_TargetUse)
float mg3bt_c0, mg3bt_c1; // slime children and stuck ones before
entity mg3bt_ctr;

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

// The tests' target (targetname mg3bt_target): counts its uses.
void() MG3_BT_TargetUse = { mg3bt_used = mg3bt_used + 1; };
entity() MG3_BT_Target =
{
    entity t = find(world, targetname, "mg3bt_target");
    if(!t) { t = spawn(); t.targetname = "mg3bt_target"; t.use = MG3_BT_TargetUse; }
    return t;
};

// Every living spawn T_Damaged to death by `p`: how many.
float(entity p) MG3_BT_KillSpawns =
{
    float n = 0;
    entity e = find(world, classname, "monster_tarbaby");
    while(e)
    {
        if(e.health > 0 && e.takedamage) { T_Damage(e, p, p, 500); n++; }
        e = find(e, classname, "monster_tarbaby");
    }
    return n;
};

void() MG3_BT_Report =
{
    dprint(sprintf("mg3btest: report map %s campaign %g mg3 data: orb %g\n", mapname, cvar("vr_campaign"), MG3_Orb_Available()));
    float n = MG3_BT_Count("monster_orb");
    dprint(sprintf("mg3btest: monster_orb %g alive %g dead; monsters %g/%g\n", n, mg3bt_n2, killed_monsters, total_monsters));
    n = MG3_BT_Count("monster_ghost");
    float sac = MG3_BT_Count("misc_sacrifice");
    entity ctr = find(world, classname, "trigger_sacrifice_counter");
    float slimes = 0;
    entity e = find(world, classname, "monster_tarbaby");
    while(e) { if(e.mg3_slime > 0) slimes++; e = find(e, classname, "monster_tarbaby"); }
    dprint(sprintf("mg3btest: monster_ghost %g; misc_sacrifice %g; counter %s count %g; slimes %g\n", n, sac, ctr.targetname, ctr.count, slimes));
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

void() MG3_BT_GhostSlimeStep =
{
    entity p = MG3_BT_Player();
    entity oldself = self, oldother = other, oldact = activator;
    if(mg3bt_step == 10)
    {
        mg3bt_pass = mg3bt_fail = 0;
        p.flags = p.flags | FL_GODMODE;
        float total = total_monsters;
        mg3bt_a = MG3_BT_Spawn(p, 42, 96, 0, 0);
        MG3_BT_Check(mg3bt_a.classname == "monster_ghost" && mg3bt_a.model == "progs/player.mdl" && mg3bt_a.health == 10,
            sprintf("a ghost ahead (%s, %s, %g)", mg3bt_a.classname, mg3bt_a.model, mg3bt_a.health));
        MG3_BT_Check(!(mg3bt_a.flags & FL_MONSTER) && total_monsters == total && mg3bt_a.movetype == MOVETYPE_FLY,
            "not a monster, not counted, it flies");
        MG3_BT_Check(mg3bt_a.handtouch == mg3_ghost_touch, "a hand's touch lays it to rest too");
        mg3bt_n = mg3_ghosts_gone;
        self = mg3bt_a; other = p;
        mg3_ghost_touch();
        self = oldself; other = oldother;
        MG3_BT_Check(mg3bt_a.health <= 0 && mg3bt_a.takedamage == DAMAGE_NO, "your touch: it dies");
        mg3bt_b = MG3_BT_Spawn(p, 42, 96, 48, 0);
        T_Damage(mg3bt_b, p, p, 20);
        MG3_BT_Check(mg3bt_b.health <= 0, "a blow: it dies");
        mg3bt_step = 11; mg3bt_next = time + 2.5;
        return;
    }
    if(mg3bt_step == 11)
    {
        MG3_BT_Check(mg3_ghosts_gone == mg3bt_n + 2 && mg3bt_a.model == "" && mg3bt_b.model == "",
            sprintf("both faded away after their deaths (%g)", mg3_ghosts_gone - mg3bt_n));
        // A sacrifice struck down: gibbed, its target fired.
        mg3bt_a = MG3_BT_Spawn(p, 43, 96, 0, 48);
        MG3_BT_Target();
        mg3bt_a.target = "mg3bt_target";
        mg3bt_used = 0;
        mg3bt_n = mg3_sacrifices;
        MG3_BT_Check(mg3bt_a.classname == "misc_sacrifice" && mg3bt_a.takedamage == DAMAGE_AIM && mg3bt_a.health == 100,
            sprintf("a sacrifice ahead (%s, %s)", mg3bt_a.classname, mg3bt_a.model));
        activator = p;
        T_Damage(mg3bt_a, p, p, 150);
        MG3_BT_Check(mg3_sacrifices == mg3bt_n + 1 && mg3bt_used == 1 && mg3bt_a.model == "", "struck down: gibbed, its target fired");
        // A counter of 3: two uses say how many are left, the third fires its target once.
        self = spawn();
        self.classname = "trigger_sacrifice_counter";
        self.count = 3;
        self.target = "mg3bt_target";
        trigger_sacrifice_counter();
        mg3bt_ctr = self;
        self = oldself;
        mg3bt_used = 0;
        activator = p;
        self = mg3bt_ctr; self.use(); self.use();
        float after2 = self.count;
        float used2 = mg3bt_used;
        activator = p; self.use();
        self = oldself;
        MG3_BT_Check(after2 == 1 && used2 == 0 && mg3bt_used == 1 && mg3bt_ctr.count == 0,
            sprintf("a counter of 3: %g left after two, fired on the third (%g)", after2, mg3bt_used));
        self = mg3bt_ctr; activator = p; self.use(); self = oldself;
        MG3_BT_Check(mg3bt_used == 1, "and never again");
        activator = oldact;
        // The slime.
        mg3bt_t = total_monsters;
        mg3bt_k = killed_monsters;
        mg3bt_n = mg3_slime_blobs;
        mg3bt_c0 = mg3_slime_children;
        mg3bt_c1 = mg3_slime_stuck;
        mg3bt_a = MG3_BT_Spawn(p, 41, 220, 0, 0);
        MG3_BT_Check(mg3bt_a.classname == "monster_tarbaby" && mg3bt_a.mg3_slime == 2 && mg3bt_a.skin == 0 && mg3bt_a.health == 80 &&
            mg3bt_a.th_pain == SUB_NullPain && total_monsters == mg3bt_t + 1, "a slime ahead: a plain spawn, two generations to come, counted");
        mg3bt_step = 12; mg3bt_next = time + 1;
        return;
    }
    if(mg3bt_step == 12)
    {
        mg3bt_a.angles_y = vectoyaw(mg3bt_a.origin - p.origin); // (its blobs thrown away from you, mostly)
        T_Damage(mg3bt_a, p, p, 500);
        mg3bt_step = 13; mg3bt_next = time + 4;
        return;
    }
    if(mg3bt_step == 13)
    {
        float blobs = mg3_slime_blobs - mg3bt_n;
        float kids = mg3_slime_children - mg3bt_c0;
        dprint(sprintf("mg3btest: slime: %g blobs, %g children started, %g blew up in a wall; monsters %g (+%g), killed %g\n",
            blobs, kids, mg3_slime_stuck - mg3bt_c1, total_monsters, total_monsters - mg3bt_t, killed_monsters - mg3bt_k));
        MG3_BT_Check(blobs >= 2 && blobs <= 3, sprintf("it blew up throwing %g blobs", blobs));
        MG3_BT_Check(kids >= 1 && total_monsters == mg3bt_t + 1 + kids, sprintf("children started (%g), each counted once", kids));
        mg3bt_n2 = kids;
        mg3bt_n = mg3_slime_blobs;
        mg3bt_c1 = mg3_slime_stuck + mg3_slime_hits;
        float n = MG3_BT_KillSpawns(p);
        dprint(sprintf("mg3btest: the first generation killed: %g\n", n));
        mg3bt_step = 14; mg3bt_next = time + 4;
        return;
    }
    if(mg3bt_step == 14)
    {
        float kids2 = mg3_slime_children - mg3bt_c0 - mg3bt_n2;
        float blobs2 = mg3_slime_blobs - mg3bt_n;
        float lost2 = mg3_slime_stuck + mg3_slime_hits - mg3bt_c1;
        dprint(sprintf("mg3btest: second generation: %g blobs: %g started, %g blew up (a wall, something alive)\n", blobs2, kids2, lost2));
        MG3_BT_Check(blobs2 >= 2 * mg3bt_n2 && blobs2 <= 3 * mg3bt_n2 && kids2 + lost2 == blobs2,
            sprintf("the children split again: %g blobs from %g, each a spawn or a blast", blobs2, mg3bt_n2));
        MG3_BT_KillSpawns(p);
        mg3bt_n2 = mg3_slime_children;
        mg3bt_step = 15; mg3bt_next = time + 4;
        return;
    }
    if(mg3bt_step == 15)
    {
        MG3_BT_Check(mg3_slime_children == mg3bt_n2, "the grandchildren do not split");
        MG3_BT_KillSpawns(p);
        mg3bt_step = 16; mg3bt_next = time + 1;
        return;
    }
    if(mg3bt_step == 16)
    {
        float alive = 0;
        entity e = find(world, classname, "monster_tarbaby");
        while(e) { if(e.health > 0) alive++; e = find(e, classname, "monster_tarbaby"); }
        MG3_BT_Check(!alive && killed_monsters - mg3bt_k == total_monsters - mg3bt_t,
            sprintf("all dead: %g killed of %g counted", killed_monsters - mg3bt_k, total_monsters - mg3bt_t));
        p.flags = p.flags - (p.flags & FL_GODMODE);
        MG3_BT_Total();
        return;
    }
};

// Map8's victims, each used as its lever would: the counter counts down from 8 and fires once.
void() MG3_BT_MapSacrifices =
{
    entity p = MG3_BT_Player();
    entity oldself = self;
    mg3bt_pass = mg3bt_fail = 0;
    mg3bt_ctr = find(world, classname, "trigger_sacrifice_counter");
    if(!mg3bt_ctr) { dprint("mg3btest: no trigger_sacrifice_counter here (Dawn of the Machine's map8 has one)\n"); return; }
    MG3_BT_Check(mg3bt_ctr.count == 8 && mg3bt_ctr.use == sacrifice_counter_use, sprintf("the counter %s needs %g", mg3bt_ctr.targetname, mg3bt_ctr.count));
    mg3bt_n = mg3_sacrifices;
    float used = 0;
    entity s = find(world, classname, "misc_sacrifice");
    while(s)
    {
        entity nxt = find(s, classname, "misc_sacrifice");
        if(substring(s.target, 0, 10) == "sacrifice_")
        {
            activator = p;
            self = s;
            self.use();
            self = oldself;
            used++;
        }
        s = nxt;
    }
    dprint(sprintf("mg3btest: %g victims used, %g gibbed; the counter at %g, next think %g (now %g)\n", used,
        mg3_sacrifices - mg3bt_n, mg3bt_ctr.count, mg3bt_ctr.nextthink, time));
    MG3_BT_Check(used == 8 && mg3_sacrifices == mg3bt_n + 8, "the 8 victims gibbed");
    MG3_BT_Check(mg3bt_ctr.count == 0 && mg3bt_ctr.nextthink > time, "the counter reached 0 and fired its target");
    MG3_BT_Total();
};

// Each frame (world.qc StartFrame).
void() MG3_BestiaryTestFrame =
{
    if(mg3bt_step && time >= mg3bt_next)
    {
        entity os = self, oo = other, oa = activator;
        if(mg3bt_step < 10) MG3_BT_OrbStep();
        else if(mg3bt_step < 20) MG3_BT_GhostSlimeStep();
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
    else if(request == 3) { mg3bt_step = 10; mg3bt_next = time; }
    else if(request == 4) MG3_BT_MapSacrifices();
    self = oldself; other = oldother; activator = oldact;
};
