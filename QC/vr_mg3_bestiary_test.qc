// Dawn of the Machine monsters' tests, M3-19..23 (vr_mg3_btest N, developer 1; Debug > Tests > Dawn of the Machine
// Bestiary): docs/vr-port/MG3_PLAN.md, docs/vr-port/TESTING.md. Prints "mg3btest: PASS/FAIL <what>" and a total.
//   1 report: the data each monster needs, and how many of each this map has (alive, dead)
//   2 the orb (a few seconds): one spawned ahead, its eye behind, its bursts of spheres at you (god mode), its pain,
//     killed: flung, blown up where it lands, counted once; the dummy type, the spawner, the grapple's mass
//   3 a ghost laid to rest by a touch and one by a blow, a sacrifice struck down (its target fired), a sacrifice counter,
//     a slime's two generations (blobs, children counted as they start, every one killed: the kills match the count)
//   5 a lava man of MG3's (it flies, its first hit staggers it, 0.8 but lightning and laser, none from Chthon, throws, a use
//     kills it) and Rogue's beside it, unchanged
//   6 (Dawn of the Machine's map2b) its lava men: waiting, risen by their trigger, killed by a use
//   7 the super shambler: its registrations, blasts at half, plasma sprays (smash 11, claw 9), its fast cast's 3 bolts,
//     killed: a ragdoll on its own rig
//   8, 9 (Dawn of the Machine's map8) its super shambler hurt past its health relays; after a save and a load, killed
//  10 Quake's monsters in Dawn of the Machine (M3-23): backpacks, skill 3's skins and pain rest, the shambler's lightning,
//     the vore's balls, Bloody Nightmare's extra lasers, spikes and grenades, hanging and close-throwing zombies (by
//     the campaign, skill and serverflags it runs in: e1m1 expects none of it)
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
    float lava = 0, lavamg3 = 0, lavastep = 0;
    e = find(world, classname, "monster_lava_man");
    while(e) { lava++; if(e.mg3_lavaman) lavamg3++; if(e.movetype == MOVETYPE_STEP) lavastep++; e = find(e, classname, "monster_lava_man"); }
    dprint(sprintf("mg3btest: monster_super_shambler %g alive\n", MG3_BT_Count("monster_super_shambler")));
    dprint(sprintf("mg3btest: monster_lava_man %g (MG3's %g, walking %g)\n", lava, lavamg3, lavastep));
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
        mg3bt_step = 5; mg3bt_next = time + 8;
        return;
    }
    if(mg3bt_step == 5)
    {
        MG3_BT_Check(mg3_orb_blowups == mg3bt_n2 + 1 && killed_monsters == mg3bt_n + 1,
            sprintf("it blew up where it landed (%g), still counted once", mg3_orb_blowups - mg3bt_n2));
        if(mg3_orb_blowups == mg3bt_n2)
            dprint(sprintf("mg3btest: the orb's body: model %s at %v velocity %v flags %g touch %g\n", mg3bt_a.model, mg3bt_a.origin,
                mg3bt_a.velocity, mg3bt_a.flags, mg3bt_a.touch == orb_death_touch));
        if(mg3_orb_blowups == mg3bt_n2)
            dprint(sprintf("mg3btest: its think orb_death4 %g, next %g (now %g), frame %g, solid %g, movetype %g\n", mg3bt_a.think == orb_death4,
                mg3bt_a.nextthink, time, mg3bt_a.frame, mg3bt_a.solid, mg3bt_a.movetype));
        if(mg3_orb_blowups == mg3bt_n2)
        {
            traceline(mg3bt_a.origin, mg3bt_a.origin - '0 0 8', FALSE, mg3bt_a);
            dprint(sprintf("mg3btest: below it: %s (solid %g, health %g, model %s) at %g\n", trace_ent.classname, trace_ent.solid,
                trace_ent.health, trace_ent.model, trace_fraction));
        }
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
        MG3_BT_Check(mg3bt_b.alpha == (cvar("vr_mg3_ghost_alpha") >= 1 ? 0 : max(0.05, cvar("vr_mg3_ghost_alpha"))),
            sprintf("drawn see-through: alpha %.2f (vr_mg3_ghost_alpha)", mg3bt_b.alpha));
        float gibs0 = vr_sgib_made, cuts0 = vr_limb_cuts + vr_limb_pops;
        T_Damage(mg3bt_b, p, p, 200);
        MG3_BT_Check(mg3bt_b.health <= 0, "a blow: it dies");
        MG3_BT_Check(vr_sgib_made == gibs0 && vr_limb_cuts + vr_limb_pops == cuts0 && !isragdoll(mg3bt_b) &&
            mg3bt_b.model == "progs/player.mdl",
            sprintf("a heavy blow (200): no gibs (%g), limbs (%g) or ragdoll; it plays its death", vr_sgib_made - gibs0,
            vr_limb_cuts + vr_limb_pops - cuts0));
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
        mg3bt_c1 = mg3_slime_stuck + mg3_slime_hits;
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
        float lost = mg3_slime_stuck + mg3_slime_hits - mg3bt_c1; // (where they land: walls and you vary with the spot)
        MG3_BT_Check(kids + lost == blobs && total_monsters == mg3bt_t + 1 + kids,
            sprintf("each blob a child or a blast (%g children, %g blasts), each child counted once", kids, lost));
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

// A ball of `m`'s in flight (its throw seen).
float(entity m) MG3_BT_LavaBalls =
{
    float n = 0;
    entity b = find(world, classname, "lavaman_ball");
    while(b) { if(b.owner == m) n++; b = find(b, classname, "lavaman_ball"); }
    return n;
};

void() MG3_BT_LavaStep =
{
    entity p = MG3_BT_Player();
    entity oldself = self, oldact = activator;
    if(mg3bt_step == 30)
    {
        mg3bt_pass = mg3bt_fail = 0;
        p.flags = p.flags | FL_GODMODE;
        mg3bt_t = total_monsters;
        mg3bt_a = MG3_BT_Spawn(p, 44, 260, -60, 0);
        MG3_BT_Check(mg3bt_a.classname == "monster_lava_man" && mg3bt_a.mg3_lavaman && mg3bt_a.model == "owned/mg3/progs/lavaman.mdl",
            sprintf("an MG3 lava man ahead (%s, %s)", mg3bt_a.classname, mg3bt_a.model));
        MG3_BT_Check(mg3bt_a.movetype == MOVETYPE_FLY && mg3bt_a.health == 1250 + 250 * skill && total_monsters == mg3bt_t + 1,
            sprintf("it flies (stays at the lava's surface), %g health, counted", mg3bt_a.health));
        MG3_BT_Check(mg3bt_a.use == lavaman_force_death && PositionalHead(mg3bt_a) && xHeadUp == 70, "a use kills it; its head zone");
        // Rogue's beside it, unchanged (when Dissolution of Eternity is here).
        mg3bt_b = world;
        if(VR_Pack_Rogue())
        {
            self = spawn();
            makevectors('0 1 0' * p.v_angle_y);
            setorigin(self, p.origin + v_forward * 260 + v_right * 80);
            self.classname = "monster_lava_man";
            monster_lava_man();
            mg3bt_b = self;
            self = oldself;
            MG3_BT_Check(!mg3bt_b.mg3_lavaman && mg3bt_b.movetype == MOVETYPE_STEP && mg3bt_b.model == "progs/lavaman.mdl" &&
                mg3bt_b.use != lavaman_force_death && !PositionalHead(mg3bt_b), "Rogue's lava man beside it: its own (walks, its model, no head zone)");
        }
        else
            dprint("mg3btest: no Dissolution of Eternity here: Rogue's lava man not checked\n");
        mg3bt_step = 31; mg3bt_next = time + 2.5; // (risen)
        return;
    }
    if(mg3bt_step == 31)
    {
        float h = mg3bt_a.health;
        T_Damage(mg3bt_a, p, p, 100);
        float d1 = h - mg3bt_a.health;
        MG3_BT_Check(d1 == 80, sprintf("a blow: 0.8 of it (%g of 100)", d1));
        MG3_BT_Check(mg3bt_a.count == 1 && mg3bt_a.frame == 80, sprintf("its first hit staggers it (frame %g)", mg3bt_a.frame));
        entity bolt = spawn();
        bolt.classname = "hiplaser";
        h = mg3bt_a.health;
        T_Damage(mg3bt_a, bolt, p, 100);
        remove(bolt);
        float d2 = h - mg3bt_a.health;
        float w1 = p.weapon, w2 = p.weapon2;
        p.weapon = p.weapon2 = WID_LIGHTNING;
        VR_SetHitKind(QVR_HITKIND_SHOT, cVR_MainHand, '0 0 0', '0 0 0');
        h = mg3bt_a.health;
        T_Damage(mg3bt_a, p, p, 100);
        float d3 = h - mg3bt_a.health;
        VR_ClearHitKind();
        p.weapon = w1; p.weapon2 = w2;
        MG3_BT_Check(d2 == 100 && d3 == 100, sprintf("the laser cannon's bolt %g, the lightning gun's %g: whole", d2, d3));
        entity chthon = spawn();
        chthon.classname = "monster_boss";
        h = mg3bt_a.health;
        T_Damage(mg3bt_a, chthon, chthon, 100);
        remove(chthon);
        MG3_BT_Check(mg3bt_a.health == h, "nothing from Chthon");
        if(mg3bt_b)
        {
            h = mg3bt_b.health;
            T_Damage(mg3bt_b, p, p, 100);
            MG3_BT_Check(h - mg3bt_b.health == 100, sprintf("Rogue's takes the whole blow (%g)", h - mg3bt_b.health));
        }
        MG3_BT_Wake(mg3bt_a, p);
        self = mg3bt_a;
        self.th_missile(); // (its throws, whatever stands between: e1m1's start has walls)
        self = oldself;
        mg3bt_n = 0;
        mg3bt_n2 = mg3bt_a.origin_z;
        mg3bt_step = 32; mg3bt_next = time + 0.5;
        return;
    }
    if(mg3bt_step >= 32 && mg3bt_step < 40)
    {
        mg3bt_n = max(mg3bt_n, MG3_BT_LavaBalls(mg3bt_a));
        if(mg3bt_step < 39) { mg3bt_step++; mg3bt_next = time + 0.7; return; }
        traceline(mg3bt_a.origin + '0 0 64', p.origin, FALSE, mg3bt_a);
        dprint(sprintf("mg3btest: lava man frame %g enemy %s attack_finished %g (now %g) sight %s; origin %v\n", mg3bt_a.frame,
            mg3bt_a.enemy.classname, mg3bt_a.attack_finished, time, trace_ent.classname, mg3bt_a.origin));
        MG3_BT_Check(mg3bt_n >= 1, sprintf("awake, it throws lava balls (%g in flight at once)", mg3bt_n));
        MG3_BT_Check(mg3bt_a.origin_z - mg3bt_n2 > -1, sprintf("it never fell (%g)", mg3bt_a.origin_z - mg3bt_n2));
        mg3bt_k = killed_monsters;
        activator = p;
        self = mg3bt_a;
        self.use();
        self = oldself; activator = oldact;
        MG3_BT_Check(mg3bt_a.health <= 0 && killed_monsters == mg3bt_k + 1, "used again: it dies, counted");
        if(mg3bt_b) T_Damage(mg3bt_b, p, p, 5000);
        mg3bt_step = 40; mg3bt_next = time + 2;
        return;
    }
    if(mg3bt_step == 40)
    {
        MG3_BT_Check(mg3bt_a.model == "" && killed_monsters == mg3bt_k + 1 + (mg3bt_b != world), "it sank and is gone, counted once");
        p.flags = p.flags - (p.flags & FL_GODMODE);
        MG3_BT_Total();
        return;
    }
};

// Dawn of the Machine's map2b: its two lava men wait for their trigger, rise when it fires, die when used again.
void() MG3_BT_MapLavaStep =
{
    entity p = MG3_BT_Player();
    entity oldself = self, oldact = activator;
    entity e;
    float n = 0, waiting = 0, risen = 0, dead = 0;
    if(mg3bt_step == 50)
    {
        mg3bt_pass = mg3bt_fail = 0;
        p.flags = p.flags | FL_GODMODE;
        e = find(world, classname, "monster_lava_man");
        while(e)
        {
            n++;
            if(e.mg3_lavaman && e.use == lavaman_awake && !e.takedamage && e.targetname != "") waiting++;
            e = find(e, classname, "monster_lava_man");
        }
        MG3_BT_Check(n >= 1 && waiting == n, sprintf("%g lava men of MG3's, %g waiting for their trigger", n, waiting));
        mg3bt_t = total_monsters;
        mg3bt_k = killed_monsters;
        e = find(world, classname, "monster_lava_man");
        while(e)
        {
            entity nxt = find(e, classname, "monster_lava_man");
            activator = p; self = e; self.use(); self = oldself;
            e = nxt;
        }
        activator = oldact;
        mg3bt_step = 51; mg3bt_next = time + 2.5;
        return;
    }
    if(mg3bt_step == 51)
    {
        e = find(world, classname, "monster_lava_man");
        while(e)
        {
            n++;
            if(e.takedamage && e.movetype == MOVETYPE_FLY && e.model == "owned/mg3/progs/lavaman.mdl" && e.use == lavaman_force_death && e.enemy == p) risen++;
            e = find(e, classname, "monster_lava_man");
        }
        MG3_BT_Check(n >= 1 && risen == n, sprintf("its trigger fired: %g of %g risen, flying, after you", risen, n));
        e = find(world, classname, "monster_lava_man");
        while(e)
        {
            activator = p; self = e; self.use(); self = oldself;
            e = find(e, classname, "monster_lava_man");
        }
        activator = oldact;
        MG3_BT_Check(killed_monsters == mg3bt_k + n && total_monsters == mg3bt_t, sprintf("used again: all dead, counted (%g)", killed_monsters - mg3bt_k));
        mg3bt_n = n;
        mg3bt_step = 52; mg3bt_next = time + 2;
        return;
    }
    if(mg3bt_step == 52)
    {
        MG3_BT_Check(!find(world, classname, "monster_lava_man") && killed_monsters == mg3bt_k + mg3bt_n, "sunk, gone, counted once");
        p.flags = p.flags - (p.flags & FL_GODMODE);
        MG3_BT_Total();
        return;
    }
};

// The super shambler: its registrations, its plasma, its lightning, blasts at half, killed: its ragdoll.
void() MG3_BT_SuperShamblerStep =
{
    entity p = MG3_BT_Player();
    entity oldself = self;
    if(mg3bt_step == 60)
    {
        mg3bt_pass = mg3bt_fail = 0;
        p.flags = p.flags | FL_GODMODE;
        mg3bt_t = total_monsters;
        mg3bt_a = MG3_BT_Spawn(p, 45, 200, 0, 0);
        MG3_BT_Check(mg3bt_a.classname == "monster_super_shambler" && mg3bt_a.model == "owned/mg3/progs/shambler_blood.mdl" &&
            mg3bt_a.health == 2000 && total_monsters == mg3bt_t + 1, sprintf("a super shambler ahead (%s, %g health), counted", mg3bt_a.model, mg3bt_a.health));
        MG3_BT_Check(PositionalHead(mg3bt_a) && xHeadUp == 53 && VR_Decap_HeadModel(mg3bt_a) == "progs/h_shams.mdl", "its head zone and head gib");
        MG3_BT_Check(VR_Parry_MeleeMonster(mg3bt_a) && VR_Grapple_MonsterMass(mg3bt_a) == 800 && VR_Dummy_Available(33),
            "its blows parried, 800 kg to the grapple, the dummy's enemy 33");
        mg3bt_step = 61; mg3bt_next = time + 1; // (started: hurt by shots)
        return;
    }
    if(mg3bt_step == 61)
    {
        MG3_BT_Check(mg3bt_a.takedamage && canragdoll(mg3bt_a) && mg3bt_a.vr_kd_chance_h == cvarh_vr_knockdown_chance_shambler,
            "started; a rig, knocked down as a shambler");
        entity boom = spawn();
        setorigin(boom, mg3bt_a.origin);
        float h = mg3bt_a.health;
        T_RadiusDamage(boom, p, 100, world);
        remove(boom);
        float lost = h - mg3bt_a.health;
        MG3_BT_Check(lost > 0 && lost <= 50, sprintf("a blast at it: half (%g of 100 at most)", lost));
        mg3bt_a.enemy = p;
        mg3bt_n = mg3_supsham_plasma;
        self = mg3bt_a; supsham_smash1(); self = oldself;
        mg3bt_step = 62; mg3bt_next = time + 1.5;
        return;
    }
    if(mg3bt_step == 62)
    {
        MG3_BT_Check(mg3_supsham_plasma - mg3bt_n == 11, sprintf("its smash sprays 11 plasma balls (%g)", mg3_supsham_plasma - mg3bt_n));
        mg3bt_n = mg3_supsham_plasma;
        self = mg3bt_a; supsham_swingl1(); self = oldself;
        mg3bt_step = 63; mg3bt_next = time + 1.2;
        return;
    }
    if(mg3bt_step == 63)
    {
        MG3_BT_Check(mg3_supsham_plasma - mg3bt_n == 9, sprintf("a claw sprays 9 (%g)", mg3_supsham_plasma - mg3bt_n));
        mg3bt_n = mg3_supsham_bolts;
        self = mg3bt_a; supsham_magic_b1(); self = oldself;
        mg3bt_step = 64; mg3bt_next = time + 1.6;
        return;
    }
    if(mg3bt_step == 64)
    {
        MG3_BT_Check(mg3_supsham_bolts - mg3bt_n == 3 && !mg3bt_a.supsham_child, sprintf("its fast cast: 3 bolts (%g), its glow gone", mg3_supsham_bolts - mg3bt_n));
        mg3bt_k = killed_monsters;
        mg3bt_a.health = 10;
        T_Damage(mg3bt_a, p, p, 20); // (dies whole: not below -60)
        MG3_BT_Check(mg3bt_a.health <= 0 && killed_monsters == mg3bt_k + 1, "killed, counted");
        mg3bt_step = 65; mg3bt_next = time + 2;
        return;
    }
    if(mg3bt_step == 65)
    {
        MG3_BT_Check(isragdoll(mg3bt_a) && ragdolllimbs(mg3bt_a, 0) > 0, sprintf("he lies as a ragdoll (vr_ragdoll %g), his limb bits %g", cvar("vr_ragdoll"), ragdolllimbs(mg3bt_a, 0)));
        p.flags = p.flags - (p.flags & FL_GODMODE);
        MG3_BT_Total();
        return;
    }
};

// Dawn of the Machine's map8: its super shambler's health relays fire as it is hurt (8); after a save and a load (9) it
// fights on with its health, then dies (its target fired, counted once).
void(float xPart) MG3_BT_MapSuperShambler =
{
    entity p = MG3_BT_Player();
    mg3bt_pass = mg3bt_fail = 0;
    entity s = find(world, classname, "monster_super_shambler");
    while(s && s.health <= 0) s = find(s, classname, "monster_super_shambler");
    if(!s) { dprint("mg3btest: no living monster_super_shambler here (Dawn of the Machine's map8 has one)\n"); return; }
    if(xPart == 1)
    {
        float relays = 0;
        entity r = find(world, targetname, s.health_target);
        while(r) { if(r.classname == "trigger_health_relay") relays++; r = find(r, targetname, s.health_target); }
        MG3_BT_Check(s.model == "owned/mg3/progs/shambler_blood.mdl" && s.health == 2000 && s.max_health == 2000 && relays >= 1,
            sprintf("map8's super shambler (%g health, health_target %s: %g relays)", s.health, s.health_target, relays));
        s.enemy = p;
        T_Damage(s, p, p, 1100);
        T_Damage(s, p, p, 1); // (its relays read its health before a blow, as upstream: the next blow fires them)
        float left = 0;
        r = find(world, targetname, s.health_target);
        while(r) { if(r.classname == "trigger_health_relay") left++; r = find(r, targetname, s.health_target); }
        MG3_BT_Check(s.health == 899 && left < relays, sprintf("hurt to %g: %g of its %g relays fired", s.health, relays - left, relays));
    }
    else
    {
        MG3_BT_Check(s.health == 899 && s.takedamage && s.model == "owned/mg3/progs/shambler_blood.mdl" && s.th_die == supsham_die,
            sprintf("after the load: %g health, its model and death", s.health));
        mg3bt_k = killed_monsters;
        T_Damage(s, p, p, 950);
        MG3_BT_Check(s.health <= 0 && killed_monsters == mg3bt_k + 1, "killed, counted once");
    }
    MG3_BT_Total();
};

// How many backpacks lie about.
float() MG3_BT_Backpacks =
{
    float n = 0;
    entity e = find(world, model, "progs/backpack.mdl");
    while(e) { n++; e = find(e, model, "progs/backpack.mdl"); }
    return n;
};

// `m`'s pain function called `n` times with `xDamage` (its rest cleared each time): how often it flinched (its think
// changed).
float(entity m, float n, float xDamage) MG3_BT_Flinches =
{
    entity oldself = self;
    float flinched = 0;
    self = m;
    while(n > 0)
    {
        self.pain_finished = 0;
        self.think = SUB_Null;
        self.th_pain(MG3_BT_Player(), xDamage);
        if(self.think != SUB_Null) flinched++;
        n--;
    }
    self.think = self.th_run;
    self = oldself;
    return flinched;
};

// Quake's monsters in Dawn of the Machine (vr_mg3_bn.qc): expectations by campaign and Bloody Nightmare (the request
// says which it runs in: an MG3 map with skill 3 and serverflags 64 for all of them; e1m1 for none).
void() MG3_BT_StockStep =
{
    entity p = MG3_BT_Player();
    entity oldself = self;
    float mg3 = MG3_Campaign();
    float bn = MG3_BloodyNightmare();
    float sk3 = skill > 2;
    if(mg3bt_step == 70)
    {
        mg3bt_pass = mg3bt_fail = 0;
        p.flags = p.flags | FL_GODMODE;
        dprint(sprintf("mg3btest: stock monsters: campaign 5 %g, skill %g, Bloody Nightmare %g\n", mg3, skill, bn));
        // Backpacks.
        float packs = MG3_BT_Backpacks();
        entity g = MG3_BT_Spawn(p, 0, 120, -60, 0);
        g.ammo_shells = 5; // (as its death code gives it)
        self = g; DropBackpack(); self = oldself;
        MG3_BT_Check(MG3_BT_Backpacks() == packs + (!mg3), sprintf("a grunt's backpack: %g (campaign 5: none)", MG3_BT_Backpacks() - packs));
        // Skill 3's harder skin (a 1-damage blow, 20 times).
        float f1 = MG3_BT_Flinches(g, 20, 1);
        entity en = MG3_BT_Spawn(p, 8, 120, 60, 0);
        float f2 = MG3_BT_Flinches(en, 20, 1);
        MG3_BT_Check((mg3 && sk3) ? (f1 <= 2 && f2 <= 2) : (f1 >= 15 && f2 >= 15), sprintf("light blows: the grunt flinched %g of 20, the enforcer %g", f1, f2));
        remove(g); remove(en);
        // The nightmare pain rest spares zombies (T_Damage; a fiend for comparison).
        entity z = MG3_BT_Spawn(p, 2, 120, 0, 0);
        entity d = MG3_BT_Spawn(p, 9, 160, 0, 0);
        mg3bt_a = z; mg3bt_b = d;
        mg3bt_step = 71; mg3bt_next = time + 1;
        return;
    }
    if(mg3bt_step == 71)
    {
        mg3bt_a.pain_finished = 0; mg3bt_b.pain_finished = 0;
        T_Damage(mg3bt_a, p, p, 30);
        T_Damage(mg3bt_b, p, p, 100);
        float zr = mg3bt_a.pain_finished - time, dr = mg3bt_b.pain_finished - time;
        MG3_BT_Check(sk3 ? (dr > 4.9 && (mg3 ? (zr < 4.9) : (zr > 4.9))) : TRUE,
            sprintf("the nightmare pain rest: the fiend's %g s, the zombie's %g s", dr, zr));
        remove(mg3bt_a); remove(mg3bt_b);
        // The shambler's lightning after its blows (skill 3).
        entity s = MG3_BT_Spawn(p, 3, 200, 0, 0);
        self = s;
        self.count = 0;
        float calls = 0, cast = 0;
        while(calls < 6 && !cast) { calls++; cast = MG3_ShamblerCasts(); }
        self = oldself;
        MG3_BT_Check((mg3 && sk3) ? (cast && calls <= (bn ? 4 : 5) && calls >= (bn ? 3 : 4)) : (!cast),
            sprintf("the shambler's lightning after %g blows (cast %g)", calls, cast));
        remove(s);
        // A vore's ball.
        mg3bt_a = MG3_BT_Spawn(p, 10, 300, 0, 0);
        mg3bt_a.enemy = p;
        self = mg3bt_a; ShalMissile(); self = oldself;
        // An enforcer's shot, a death knight's spikes, a scrag's, an ogre's volley.
        mg3bt_n = mg3_bn_extra;
        entity e2 = MG3_BT_Spawn(p, 8, 200, -80, 0);
        e2.enemy = p;
        self = e2; enforcer_fire(); self = oldself;
        MG3_BT_Check(mg3_bn_extra - mg3bt_n == bn, sprintf("the enforcer's shot: %g more lasers", mg3_bn_extra - mg3bt_n));
        remove(e2);
        entity h = MG3_BT_Spawn(p, 6, 200, 80, 0);
        h.enemy = p;
        self = h; hknight_shot(0); self = oldself;
        entity k = find(world, classname, "knightspike");
        entity newest = world;
        while(k) { if(k.owner == h) newest = k; k = find(k, classname, "knightspike"); }
        MG3_BT_Check((newest != world) && fabs(vlen(newest.velocity) - (bn ? 500 : 300)) < 1, sprintf("the death knight's spike: %g a second", vlen(newest.velocity)));
        remove(h);
        mg3bt_b = MG3_BT_Spawn(p, 4, 200, 0, 64);
        mg3bt_b.enemy = p;
        self = mg3bt_b; Wiz_StartFast(); Wiz_StartFast(); Wiz_StartFast(); self = oldself;
        mg3bt_n = mg3_bn_extra;
        mg3bt_step = 72; mg3bt_next = time + 0.6;
        return;
    }
    if(mg3bt_step == 72)
    {
        entity ball = find(world, classname, "shal_missile");
        if(mg3)
        {
            float steered = FALSE;
            if(ball)
            {
                self = ball; ShalHome(); self = oldself;
                float want = 250;
                if(skill == 3) want = 285;
                float late = ball.nextthink - time;
                float speed = vlen(ball.velocity);
                if(fabs(late - 0.1) < 0.01 && fabs(speed - want) < 1) steered = TRUE;
            }
            MG3_BT_Check(steered, sprintf("the vore's ball steers every 0.1 s at %g (%s)", vlen(ball.velocity), ball.classname));
        }
        remove(mg3bt_a);
        mg3bt_step = 73; mg3bt_next = time + 1.2;
        return;
    }
    if(mg3bt_step == 73)
    {
        float wiz = mg3_bn_extra - mg3bt_n;
        MG3_BT_Check(bn ? (wiz >= 1) : (wiz == 0), sprintf("the scrag's six spikes brought %g more", wiz));
        remove(mg3bt_b);
        mg3bt_a = MG3_BT_Spawn(p, 1, 250, 0, 0); // (an ogre: started, then its volleys)
        mg3bt_step = 74; mg3bt_next = time + 1;
        return;
    }
    if(mg3bt_step == 74)
    {
        // The ogre: volleys until a burst starts (Bloody Nightmare: 70% of them).
        mg3bt_a.enemy = p;
        mg3bt_n = mg3_bn_extra;
        float tries = 0;
        self = mg3bt_a;
        while(tries < 10 && !self.mg3_burst) { tries++; OgreFireGrenade(); MG3_OgreBurst(); }
        self = oldself;
        mg3bt_step = 75; mg3bt_next = time + 1;
        return;
    }
    if(mg3bt_step == 75)
    {
        MG3_BT_Check(mg3_bn_extra - mg3bt_n == (bn ? 2 : 0), sprintf("the ogre's burst: %g more grenades", mg3_bn_extra - mg3bt_n));
        remove(mg3bt_a);
        // Zombies: hanging, and throwing only up close.
        self = spawn();
        setorigin(self, p.origin + '96 0 32');
        self.classname = "monster_zombie";
        self.spawnflags = MG3_ZOMBIE_HANGING;
        monster_zombie();
        mg3bt_a = self;
        self = spawn();
        setorigin(self, p.origin + '96 64 0');
        self.classname = "monster_zombie";
        self.spawnflags = MG3_ZOMBIE_CLOSE_THROW;
        monster_zombie();
        mg3bt_b = self;
        self = oldself;
        MG3_BT_Check(mg3 ? (mg3bt_a.movetype == MOVETYPE_NONE && mg3bt_a.think == mg3_zombie_hang1) : (mg3bt_a.think != mg3_zombie_hang1),
            "a zombie with spawnflag 8388608 hangs (campaign 5)");
        MG3_BT_Check(mg3 ? (mg3bt_b.th_melee == zombie_missile && mg3bt_b.th_missile == SUB_Null) : (mg3bt_b.th_missile == zombie_missile),
            "spawnflag 128: its flesh thrown only up close (campaign 5)");
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
        else if(mg3bt_step < 20) MG3_BT_GhostSlimeStep();
        else if(mg3bt_step >= 30 && mg3bt_step < 50) MG3_BT_LavaStep();
        else if(mg3bt_step >= 50 && mg3bt_step < 60) MG3_BT_MapLavaStep();
        else if(mg3bt_step >= 60 && mg3bt_step < 70) MG3_BT_SuperShamblerStep();
        else if(mg3bt_step >= 70 && mg3bt_step < 80) MG3_BT_StockStep();
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
    else if(request == 6) { mg3bt_step = 50; mg3bt_next = time; }
    else if(request == 8) MG3_BT_MapSuperShambler(1);
    else if(request == 10) { mg3bt_step = 70; mg3bt_next = time; }
    else if(request == 11) { serverflags = serverflags | MG3_SF_BN_ACTIVE; dprint("mg3btest: Bloody Nightmare on (serverflags 64)\n"); }
    else if(request == 9) MG3_BT_MapSuperShambler(2);
    else if(request == 7) { if(MG3_SuperShambler_Available()) { mg3bt_step = 60; mg3bt_next = time; } }
    else if(request == 5)
    {
        if(!MG3_LavaMan_Available()) dprint("mg3btest: the Dawn of the Machine data is not there (no lava man)\n");
        else { mg3bt_step = 30; mg3bt_next = time; }
    }
    self = oldself; other = oldother; activator = oldact;
};
