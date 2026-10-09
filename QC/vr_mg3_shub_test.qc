// Dawn of the Machine's Shub-Niggurath tests, M3-26/27 (vr_mg3_shubtest N, developer 1; Debug > Tests > Dawn of the
// Machine: Shub): docs/vr-port/MG3.md, docs/vr-port/TESTING.md. Prints "mg3shubtest: PASS/FAIL <what>" and a total.
// On boss2 the map's Shub (the player taken to the arena's arrival point, tele1); elsewhere a free one spawned 450
// units ahead (the Debug spawner's: killed, she ends nothing).
//   1 report: her data, the map's Shub (phase, health), its shub zombie spawns, pillars, zombies and children alive
//   2 her phases (god mode): her first wound, 3/4, 1/2, 1/4 (her thrash immune, a seeker at phase 2, her waves fired),
//     her attack's rotation of children in each phase, her autoguns by skill, each child made, working and dead
//     (spammer, swiper, blasters, vortex, seeker; an eye spent after 72 shots), the counts of the level's monsters
//   3 (boss2) her zombies: raised at the map's spawn points (lying, counted, at most 33), up 7 s later, her spheres and
//     their flesh passing each other, killed; the pillars: 20 down a hit, a blast sinks one too, you ridden down on one
//     to the floor, which holds you as it sinks on
//   5 the training dummy as Shub (free) and as her eye: killed as each, nothing ended, a dummy back
//   4 her death (god mode): immune, her children cleared, the lights out, the burst of gibs, then (a map's Shub) the
//     final text and the credits; a free one ends nothing

float mg3st_pass, mg3st_fail;
float mg3st_step;   // the running request's step, 0: none
float mg3st_next;   // when its next step runs
entity mg3st_m;     // her
float mg3st_used;   // uses of the tests' target (her waves)
float mg3st_t, mg3st_k, mg3st_n, mg3st_n2; // monsters total and killed, counts before

void(float ok, string label) MG3_ST_Check =
{
    if(ok) mg3st_pass++; else mg3st_fail++;
    dprint(sprintf("mg3shubtest: %s %s\n", ok ? "PASS" : "FAIL", label));
};

void() MG3_ST_Total =
{
    dprint(sprintf("mg3shubtest: total %g/%g\n", mg3st_pass, mg3st_fail));
    mg3st_step = 0;
};

void() MG3_ST_TargetUse = { mg3st_used = mg3st_used + 1; };

// How many of `xClass` are there (mg3st_n2: of them alive).
float(string xClass) MG3_ST_Count =
{
    float n = 0;
    mg3st_n2 = 0;
    entity e = find(world, classname, xClass);
    while(e)
    {
        n++;
        if(e.health > 0) mg3st_n2++;
        e = find(e, classname, xClass);
    }
    return n;
};

void() MG3_ST_Report =
{
    entity m = find(world, classname, "monster_oldone_new");
    dprint(sprintf("mg3shubtest: report map %s campaign %g skill %g data %g\n", mapname, cvar("vr_campaign"), skill, MG3_Shub_Available()));
    dprint(sprintf("mg3shubtest: shub %g phase %g health %g/%g immune %g free %g at %v\n", m != world, m.mg3_shub_phase,
        m.health, m.max_health, m.mg3_shub_immune, m.mg3_shub_free, m.origin));
    float spawns = MG3_ST_Count("info_szombie_spawn");
    float pillars = MG3_ST_Count("func_breakable");
    float zombies = 0;
    entity e = find(world, classname, "monster_zombie");
    while(e) { if(MG3_SZombie_Is(e)) zombies++; e = find(e, classname, "monster_zombie"); }
    float kids = MG3_ST_Count("oldnew_child");
    float eyes = MG3_ST_Count("oldnew_eye");
    dprint(sprintf("mg3shubtest: szombie spawns %g, pillars %g, shub zombies %g, children %g, seekers %g; monsters %g/%g\n",
        spawns, pillars, zombies, kids, eyes, killed_monsters, total_monsters));
    string sunk = "";
    e = find(world, classname, "func_breakable");
    while(e) { sunk = strcat(sunk, sprintf(" %g", -e.origin_z)); e = find(e, classname, "func_breakable"); }
    if(pillars) dprint(sprintf("mg3shubtest: pillars sunk by:%s\n", sunk));
};

// Her on this map, or a free one spawned ahead; the player at the arena's arrival point (boss2).
entity(entity p) MG3_ST_Shub =
{
    entity m = find(world, classname, "monster_oldone_new");
    if(m)
    {
        entity dest = find(world, targetname, "tele1");
        if(dest)
        {
            setorigin(p, dest.origin);
            p.angles = dest.angles;
            p.fixangle = TRUE;
        }
        return m;
    }
    makevectors('0 1 0' * p.v_angle_y);
    entity d = spawn();
    d.weapon = 60;
    setorigin(d, p.origin + v_forward * 450);
    entity oldself = self;
    self = d;
    activator = p;
    func_enemy_dispenser_use();
    m = self;
    self = oldself;
    remove(d);
    return m;
};

// `e` (its spawn function's self) wakes at `p`.
void(entity e, entity p) MG3_ST_Wake =
{
    entity oldself = self;
    self = e;
    self.enemy = p;
    FoundTarget();
    self = oldself;
};

// Each of Shub's children hurt to death by `p` (an eye left with `spend` shots spends itself instead): how many.
float(entity p, float spend) MG3_ST_KillChildren =
{
    float n = 0;
    entity e = find(world, classname, "oldnew_child");
    while(e)
    {
        entity nx = find(e, classname, "oldnew_child");
        if(e.takedamage && e.health > 0)
        {
            if(spend) { e.count = 72; spend = FALSE; }
            else { T_Damage(e, p, p, 500); n++; }
        }
        e = nx;
    }
    e = find(world, classname, "oldnew_eye");
    while(e)
    {
        entity nx2 = find(e, classname, "oldnew_eye");
        T_Damage(e, p, p, 500);
        n++;
        e = nx2;
    }
    return n;
};

// Her attack `n` times at `phase` (each a child's turn: her volley count reset), from her rotation's start.
void(float phase, float n) MG3_ST_Attacks =
{
    self.mg3_shub_phase = phase;
    self.count = 0;
    while(n > 0)
    {
        self.cnt = 0;
        oldnew_attack();
        n--;
    }
};

float mg3st_c1, mg3st_c2, mg3st_c3, mg3st_c4, mg3st_c5; // children made before (spammers, swipers, vortices, blasters, seekers)
void() MG3_ST_Mark = { mg3st_c1 = mg3_shub_spammers; mg3st_c2 = mg3_shub_swipers; mg3st_c3 = mg3_shub_vortices; mg3st_c4 = mg3_shub_blasters; mg3st_c5 = mg3_shub_eyes; };
float(float a, float b, float c, float d, float e) MG3_ST_Made =
{
    return mg3_shub_spammers - mg3st_c1 == a && mg3_shub_swipers - mg3st_c2 == b && mg3_shub_vortices - mg3st_c3 == c &&
        mg3_shub_blasters - mg3st_c4 == d && mg3_shub_eyes - mg3st_c5 == e;
};
string() MG3_ST_MadeStr =
{
    return sprintf("spammers %g swipers %g vortices %g blasters %g seekers %g", mg3_shub_spammers - mg3st_c1,
        mg3_shub_swipers - mg3st_c2, mg3_shub_vortices - mg3st_c3, mg3_shub_blasters - mg3st_c4, mg3_shub_eyes - mg3st_c5);
};

void() MG3_ST_PhaseStep =
{
    entity p = find(world, classname, "player");
    entity m = mg3st_m;
    entity oldself = self;
    if(mg3st_step == 1)
    {
        mg3st_pass = mg3st_fail = 0;
        p.flags = p.flags | FL_GODMODE;
        mg3st_t = total_monsters;
        m = mg3st_m = MG3_ST_Shub(p);
        MG3_ST_Check(m.classname == "monster_oldone_new" && m.model == "progs/oldone.mdl" && m.health == 12000 &&
            m.max_health == 12000 && m.mg3_shub_phase == 0 && (m.flags & FL_MONSTER),
            sprintf("Shub: her model, 12000 health, the prelude (%s %s %g, free %g)", m.classname, m.model, m.health, m.mg3_shub_free));
        MG3_ST_Check(VR_Grapple_MonsterMass(m) == 10000, sprintf("10000 kg to the grapple (%g)", VR_Grapple_MonsterMass(m)));
        entity t = find(world, targetname, "mg3st_target");
        if(!t) { t = spawn(); t.targetname = "mg3st_target"; t.use = MG3_ST_TargetUse; }
        m.wave1 = "mg3st_target";
        m.wave2 = "mg3st_target";
        mg3st_used = 0;
        MG3_ST_Wake(m, p);
        mg3st_step = 2; mg3st_next = time + 0.3;
        return;
    }
    if(mg3st_step == 2)
    {
        MG3_ST_Check(m.enemy == p && m.think != oldnew_idle1, "awake: facing you");
        vector o = m.origin;
        VR_Push(m, '1 0 0', 700, 200);
        MG3_ST_Check(m.velocity == '0 0 0' && m.origin == o, "no shove moves her");
        mg3st_n = mg3_shub_eyes;
        T_Damage(m, p, p, 10);
        MG3_ST_Check(m.mg3_shub_phase == 1 && m.mg3_shub_immune && m.health == 11990,
            sprintf("her first wound: phase 1, thrashing, immune (phase %g health %g)", m.mg3_shub_phase, m.health));
        T_Damage(m, p, p, 100);
        MG3_ST_Check(m.health == 11990, sprintf("immune: no damage while she thrashes (%g)", m.health));
        mg3st_step = 3; mg3st_next = time + 1.7;
        return;
    }
    if(mg3st_step >= 3 && mg3st_step <= 5)
    {
        float want = mg3st_step - 1;
        float hp = m.health;
        MG3_ST_Check(!m.mg3_shub_immune, "the thrash over: not immune");
        T_Damage(m, p, p, 3000);
        MG3_ST_Check(m.mg3_shub_phase == want && m.mg3_shub_immune && m.health == hp - 3000,
            sprintf("under %g/4 of her health: phase %g (%g, health %g)", 5 - want, want, m.mg3_shub_phase, m.health));
        if(want == 2)
            MG3_ST_Check(mg3_shub_eyes == mg3st_n + 1 && find(world, classname, "oldnew_eye") != world, "phase 2: a seeker eye");
        MG3_ST_Check(mg3st_used == want - 1, sprintf("her waves fired: %g (wave1 at 2, wave2 at 3 and 4)", mg3st_used));
        mg3st_step = mg3st_step + 1; mg3st_next = time + 1.7;
        return;
    }
    if(mg3st_step == 6)
    {
        // Her attacks: the rotation of each phase's children, then a volley of diamonds, none while immune.
        mg3st_t = total_monsters;
        self = m;
        self.mg3_shub_immune = 0;
        MG3_ST_Mark(); MG3_ST_Attacks(1, 2);
        MG3_ST_Check(MG3_ST_Made(1, 1, 0, 0, 0), sprintf("phase 1: a spammer, a swiper (%s)", MG3_ST_MadeStr()));
        MG3_ST_Mark(); MG3_ST_Attacks(2, 3);
        MG3_ST_Check(MG3_ST_Made(1, 1, 1, 0, 0), sprintf("phase 2: a vortex, a swiper, a spammer (%s)", MG3_ST_MadeStr()));
        MG3_ST_Mark(); MG3_ST_Attacks(3, 4);
        MG3_ST_Check(MG3_ST_Made(0, 3, 0, 1, 0), sprintf("phase 3: blasters, three swipers (%s)", MG3_ST_MadeStr()));
        MG3_ST_Mark(); MG3_ST_Attacks(4, 4);
        MG3_ST_Check(MG3_ST_Made(1, 1, 0, 1, 1), sprintf("phase 4: a spammer, a seeker, a swiper, blasters (%s)", MG3_ST_MadeStr()));
        mg3st_n = MG3_ST_Count("oldnew_chunks");
        self.cnt = 1;
        oldnew_attack();
        MG3_ST_Check(MG3_ST_Count("oldnew_chunks") == mg3st_n + 1 && self.cnt == 2, "a volley of diamonds between");
        MG3_ST_Mark();
        self.mg3_shub_immune = 1;
        self.cnt = 0;
        oldnew_attack();
        MG3_ST_Check(MG3_ST_Made(0, 0, 0, 0, 0) && self.cnt == 0, "none while immune");
        self.mg3_shub_immune = 0;
        // Her autoguns: the c-th of a run up to her phase + 2 and skill + 1, none on skill 0.
        mg3st_n = mg3_shub_autogun;
        float c = 1;
        float expect = 0;
        while(c <= 5)
        {
            oldnew_autogun_check(1, 0.05 * c, c);
            if(skill > 0 && c <= self.mg3_shub_phase + 2 && c <= skill + 1) expect++;
            c++;
        }
        MG3_ST_Check(mg3_shub_autogun - mg3st_n == expect, sprintf("her autoguns at skill %g: %g of 5 (%g)", skill, expect, mg3_shub_autogun - mg3st_n));
        MG3_ST_Check(total_monsters == mg3st_t + 5, sprintf("her eyes counted as they appear: %g (5)", total_monsters - mg3st_t));
        // Those eyes killed at once (they appeared on each other: they would telefrag each other as upstream's do), and a
        // pair of blasters on their own.
        mg3st_k = killed_monsters;
        mg3st_n = MG3_ST_KillChildren(p, FALSE);
        MG3_ST_Check(mg3st_n == 7 && killed_monsters - mg3st_k == 5,
            sprintf("killed: 5 eyes and 2 seekers (%g), the eyes counted dead (%g)", mg3st_n, killed_monsters - mg3st_k));
        self = oldself;
        mg3st_step = 9; mg3st_next = time + 0.5; // (once their telefrag triggers are gone: 0.2 s)
        return;
    }
    if(mg3st_step == 9)
    {
        self = m;
        spawn_blaster();
        self = oldself;
        mg3st_n2 = mg3_shub_blasts;
        mg3st_step = 7; mg3st_next = time + 3;
        return;
    }
    if(mg3st_step == 7)
    {
        float spammers = 0, swipers = 0;
        entity e = find(world, classname, "oldnew_child");
        while(e) { if(e.think == spammer_think) spammers++; if(e.think == oldnew_swipe) swipers++; e = find(e, classname, "oldnew_child"); }
        MG3_ST_Check(spammers == 0 && swipers == 0, sprintf("spammers and swipers done and gone (%g, %g)", spammers, swipers));
        MG3_ST_Check(mg3_shub_spam >= 39, sprintf("the spammers' plasma: 13 each (%g)", mg3_shub_spam));
        MG3_ST_Check(mg3_shub_blasts > mg3st_n2, sprintf("the eyes spiral spheres at you (%g)", mg3_shub_blasts - mg3st_n2));
        mg3st_k = killed_monsters;
        mg3st_n = MG3_ST_KillChildren(p, TRUE);
        mg3st_step = 8; mg3st_next = time + 1.5;
        return;
    }
    if(mg3st_step == 8)
    {
        float left = MG3_ST_Count("oldnew_child") + MG3_ST_Count("oldnew_eye");
        MG3_ST_Check(left == 0, sprintf("every child dead: %g killed, the other eye spent its shots; %g left", mg3st_n, left));
        MG3_ST_Check(killed_monsters - mg3st_k == 2, sprintf("the blasters counted dead: %g (2)", killed_monsters - mg3st_k));
        float inflight = MG3_ST_Count("spam");
        MG3_ST_Check(mg3_shub_spamblasts + mg3_shub_spamhits + inflight == mg3_shub_spam && mg3_shub_spamblasts > 0,
            sprintf("the plasma: %g fired, %g blown up where they landed, %g on something alive, %g still about",
            mg3_shub_spam, mg3_shub_spamblasts, mg3_shub_spamhits, inflight));
        m.mg3_shub_immune = 0;
        p.flags = p.flags - (p.flags & FL_GODMODE);
        MG3_ST_Total();
        return;
    }
};

// Her death (god mode): immune, her children gone, the lights out, the burst, the ending.
void() MG3_ST_EndStep =
{
    entity p = find(world, classname, "player");
    entity m = mg3st_m;
    if(mg3st_step == 20)
    {
        mg3st_pass = mg3st_fail = 0;
        p.flags = p.flags | FL_GODMODE;
        m = mg3st_m = MG3_ST_Shub(p);
        MG3_ST_Wake(m, p);
        entity oldself = self;
        self = m;
        spawn_blaster();
        spawn_eye();
        MG3_SZombie_Raise();
        MG3_SZombie_Raise();
        self = oldself;
        m.mg3_shub_phase = 4;
        m.mg3_shub_immune = 0;
        m.health = 100;
        mg3st_k = killed_monsters;
        mg3st_n = mg3_shub_gibs;
        T_Damage(m, p, p, 200);
        MG3_ST_Check(m.takedamage == DAMAGE_NO && m.mg3_shub_immune && killed_monsters == mg3st_k + 1,
            sprintf("killed: counted, immune, dying (%g)", killed_monsters - mg3st_k));
        MG3_ST_Check(find(world, classname, "oldnew_zombie_cleaner") != world, "her zombies' cleaner set");
        mg3st_step = 21; mg3st_next = time + 1;
        return;
    }
    if(mg3st_step == 21)
    {
        MG3_ST_Check(MG3_ST_Count("oldnew_child") == 0 && MG3_ST_Count("oldnew_eye") == 0, "her children gone");
        MG3_ST_Check(m.classname == "monster_oldone_new" && intermission_running == 0, "still thrashing");
        mg3st_step = 22; mg3st_next = time + 5;
        return;
    }
    if(mg3st_step == 22)
    {
        float up = 0, i = 0;
        while(i < 100) { vector v = MG3_Shub_SphereDir(i); if(v_z > 0) up++; i++; }
        MG3_ST_Check(m.modelindex == 0 && !m.takedamage, "burst: gone");
        MG3_ST_Check(mg3_shub_gibs - mg3st_n == up, sprintf("her gibs: %g (%g)", mg3_shub_gibs - mg3st_n, up));
        float zs = 0;
        entity z = find(world, classname, "monster_zombie");
        while(z) { if(MG3_SZombie_Is(z)) zs++; z = find(z, classname, "monster_zombie"); }
        MG3_ST_Check(zs == 0, sprintf("her zombies gibbed by her cleaner: %g left", zs));
        if(m.mg3_shub_free)
        {
            MG3_ST_Check(intermission_running == 0, "a free Shub ends nothing");
            p.flags = p.flags - (p.flags & FL_GODMODE);
            MG3_ST_Total();
            return;
        }
        MG3_ST_Check(intermission_running == 1 && nextmap == "start" && MG_intermissiontext == "$map_dopa_endtext_final",
            sprintf("the ending: intermission %g, next map %s, text %s", intermission_running, nextmap, MG_intermissiontext));
        mg3st_step = 23; mg3st_next = time + 2.5;
        return;
    }
    if(mg3st_step == 23 || mg3st_step == 24)
    {
        entity oldself2 = self;
        self = p;
        ExitIntermission();
        self = oldself2;
        if(mg3st_step == 23)
        {
            MG3_ST_Check(intermission_running == 2 && MG_intermissiontext == "", sprintf("a press: the final text (stage %g)", intermission_running));
            mg3st_step = 24; mg3st_next = time + 1.5;
            return;
        }
        MG3_ST_Check(intermission_running == 3, sprintf("another: the credits (stage %g; disconnect, menu_credits queued)", intermission_running));
        MG3_ST_Total();
        return;
    }
};

// Shub zombies and pillars (boss2; god mode).
float(float alive) MG3_ST_SZombies =
{
    float n = 0;
    entity e = find(world, classname, "monster_zombie");
    while(e) { if(e.mg3_szombie && (!alive || e.health > 0)) n++; e = find(e, classname, "monster_zombie"); }
    return n;
};
entity mg3st_z, mg3st_b;
float mg3st_z0, mg3st_floor;
void() MG3_ST_ZombieStep =
{
    entity p = find(world, classname, "player");
    entity m = mg3st_m;
    entity oldself = self;
    if(mg3st_step == 50)
    {
        mg3st_pass = mg3st_fail = 0;
        p.flags = p.flags | FL_GODMODE;
        m = mg3st_m = MG3_ST_Shub(p);
        float spawns = 0, rested = 0;
        entity e = find(world, classname, "info_szombie_spawn");
        while(e) { spawns++; if(e.wait == -1) rested++; e = find(e, classname, "info_szombie_spawn"); }
        MG3_ST_Check(spawns == 36 && rested == 36, sprintf("boss2's 36 shub zombie spawns, unused (%g, %g)", spawns, rested));
        float pillars = 0, good = 0;
        e = find(world, classname, "func_breakable");
        while(e)
        {
            pillars++;
            if(e.solid == SOLID_BSP && e.movetype == MOVETYPE_PUSH && e.health == 10000 && e.takedamage) good++;
            e = find(e, classname, "func_breakable");
        }
        MG3_ST_Check(pillars == 8 && good == 8, sprintf("its 8 pillars, solid pushers of 10000 (%g, %g)", pillars, good));
        // One raised: lying at a spawn point, counted, after you.
        mg3st_t = total_monsters;
        self = m;
        MG3_SZombie_Raise();
        self = oldself;
        entity z = find(world, classname, "monster_zombie");
        while(z && !z.mg3_szombie) z = find(z, classname, "monster_zombie");
        mg3st_z = z;
        e = find(world, classname, "info_szombie_spawn");
        float at = FALSE;
        while(e) { if(e.origin == z.origin && e.wait > time) at = TRUE; e = find(e, classname, "info_szombie_spawn"); }
        MG3_ST_Check(z && at && z.solid == SOLID_NOT && z.frame == MG3_ZOMBIE_PAINE12 + 1 && z.inpain == 2 && z.health == 60 &&
            z.enemy == p && total_monsters == mg3st_t + 1 && z.model == "progs/zombie.mdl",
            sprintf("a shub zombie raised: lying at a spawn point (resting now), 60, counted, after you (%v)", z.origin));
        MG3_ST_Check(VR_Decap_HeadModel(z) == "progs/h_zombie.mdl", "a zombie's head to lose");
        // More: never past 33 alive.
        float i = 0;
        self = m;
        while(i < 40) { MG3_SZombie_Raise(); i++; }
        self = oldself;
        mg3st_n = MG3_ST_SZombies(TRUE);
        MG3_ST_Check(mg3st_n == 33 && total_monsters == mg3st_t + 33, sprintf("at most 33 alive (%g; counted %g)", mg3st_n, total_monsters - mg3st_t));
        // Her sphere passes it; its flesh passes her.
        entity s1 = spawn();
        s1.owner = m;
        s1.classname = "spike";
        s1.nextthink = time + 5;
        self = s1; other = z;
        sphere_mis_touch();
        self = oldself;
        MG3_ST_Check(z.health == 60 && s1.nextthink == -1, "her sphere passes it (gone, no harm)"); // (freed: nextthink -1)
        self = z;
        z.enemy = p;
        makevectors(z.angles);
        ZombieFireGrenade('-10 -22 30');
        self = oldself;
        entity g = find(world, classname, "zgrenade");
        while(g && g.owner != z) g = find(g, classname, "zgrenade");
        float hp = m.health;
        self = g; other = m;
        g.touch();
        self = oldself;
        MG3_ST_Check(g && m.health == hp && g.nextthink == -1, sprintf("its flesh passes her (%g)", m.health));
        mg3st_step = 51; mg3st_next = time + 8;
        return;
    }
    if(mg3st_step == 51)
    {
        float up = 0;
        entity e2 = find(world, classname, "monster_zombie");
        while(e2) { if(e2.mg3_szombie && e2.health > 0 && e2.solid == SOLID_SLIDEBOX) up++; e2 = find(e2, classname, "monster_zombie"); }
        MG3_ST_Check(up >= 1 && mg3st_z.solid == SOLID_SLIDEBOX, sprintf("up 7 s later: %g of %g standing (the rest wait for room)", up, MG3_ST_SZombies(TRUE)));
        // A swiper's beam does a shub zombie 100.
        mg3st_k = killed_monsters;
        T_Damage(mg3st_z, m, m, 100);
        MG3_ST_Check(mg3st_z.health <= 0 && killed_monsters == mg3st_k + 1, "a 100 kills one, counted");
        // Every one killed: counted.
        float n = 0;
        e2 = find(world, classname, "monster_zombie");
        while(e2) { entity nx = find(e2, classname, "monster_zombie"); if(MG3_SZombie_Is(e2)) { T_Damage(e2, p, p, 200); n++; } e2 = nx; }
        MG3_ST_Check(MG3_ST_SZombies(TRUE) == 0 && killed_monsters == mg3st_k + 1 + n, sprintf("all %g more killed, counted", n));
        // A pillar hit: 20 down in a second, its health back.
        mg3st_b = find(world, classname, "func_breakable");
        while(mg3st_b && mg3st_b.model != "*52") mg3st_b = find(mg3st_b, classname, "func_breakable");
        mg3st_z0 = mg3st_b.origin_z;
        T_Damage(mg3st_b, p, p, 10);
        MG3_ST_Check(mg3st_b.health == 10000 && mg3st_b.velocity_z == -20, "a pillar hit: sinking, its health back");
        mg3st_step = 52; mg3st_next = time + 1.5;
        return;
    }
    if(mg3st_step == 52)
    {
        float d = mg3st_z0 - mg3st_b.origin_z;
        MG3_ST_Check(fabs(d - 20) < 1 && mg3st_b.velocity == '0 0 0', sprintf("20 down, stopped (%g)", d));
        // A blast beside it (her spammer's): it sinks too.
        entity bl = spawn();
        setorigin(bl, (mg3st_b.absmin + mg3st_b.absmax) * 0.5 + '0 -100 0');
        bl.origin_z = 32;
        T_RadiusDamage(bl, bl, 100, world);
        remove(bl);
        MG3_ST_Check(mg3st_b.velocity_z == -20, "a blast beside it sinks it");
        // You on it, taken down with it to the floor and no further.
        setorigin(p, '2840 -496 0' + '0 0 1' * (mg3st_b.absmax_z + 26));
        p.velocity = '0 0 0';
        traceline('2700 -496 64', '2700 -496 -64', TRUE, p); // (the floor beside it)
        mg3st_floor = trace_endpos_z;
        mg3st_n = 0;
        mg3st_step = 53; mg3st_next = time + 0.5;
        return;
    }
    if(mg3st_step == 53)
    {
        // Hit each second until its top is at the floor (and once more).
        T_Damage(mg3st_b, p, p, 1);
        mg3st_n++;
        if(mg3st_b.absmax_z > mg3st_floor + 2 && mg3st_n < 40) { mg3st_next = time + 0.9; return; }
        mg3st_step = 54; mg3st_next = time + 3;
        return;
    }
    if(mg3st_step == 54)
    {
        float feet = p.origin_z + p.mins_z;
        float onground = (p.flags & FL_ONGROUND) != 0;
        MG3_ST_Check(mg3st_b.absmax_z < mg3st_floor + 2 && feet >= mg3st_floor - 1 && pointcontents(p.origin) != CONTENT_SOLID &&
            onground, sprintf("you rode it down %g hits, the floor took you as it sank on: its top %g, the floor %g, your feet %g, on the ground %g",
            mg3st_n, mg3st_b.absmax_z, mg3st_floor, feet, onground));
        p.flags = p.flags - (p.flags & FL_GODMODE);
        MG3_ST_Total();
        return;
    }
};

// The training dummy as Shub (50; free: her death ends nothing) and as her eye (51), 300 units ahead: what it is,
// killed as its enemy (vr_dummy_test 1 and 3: "dummytest:" lines).
entity mg3st_d;
void() MG3_ST_DummyStep =
{
    entity p = find(world, classname, "player");
    if(mg3st_step == 30 || mg3st_step == 33)
    {
        if(mg3st_step == 30) { mg3st_pass = mg3st_fail = 0; }
        cvar_set("vr_dummy_type", mg3st_step == 30 ? "50" : "51");
        makevectors('0 1 0' * p.v_angle_y);
        entity oldself = self;
        self = spawn();
        self.classname = "vr_dummy";
        setorigin(self, p.origin + v_forward * 300 + '0 0 16');
        self.angles_y = p.v_angle_y + 180;
        vr_dummy();
        mg3st_d = self;
        self = oldself;
        mg3st_step = mg3st_step + 1; mg3st_next = time + 1;
        return;
    }
    if(mg3st_step == 31 || mg3st_step == 34)
    {
        string want = mg3st_step == 31 ? "progs/oldone.mdl" : "owned/mg3/progs/teleporter_eye.mdl";
        MG3_ST_Check(mg3st_d.model == want && (mg3st_step != 31 || mg3st_d.mg3_shub_free),
            sprintf("the dummy as %s (%s; free %g)", VR_Dummy_Name(mg3st_d), mg3st_d.model, mg3st_d.mg3_shub_free));
        cvar_set("vr_dummy_test", "1");
        mg3st_step = mg3st_step + 1; mg3st_next = time + 0.5;
        if(mg3st_step == 32) { cvar_set("vr_dummy_test", "1"); }
        return;
    }
    if(mg3st_step == 32 || mg3st_step == 35)
    {
        T_Damage(mg3st_d, p, p, 100000);
        mg3st_step = mg3st_step + 10; mg3st_next = time + 7;
        return;
    }
    if(mg3st_step == 42 || mg3st_step == 45)
    {
        MG3_ST_Check(intermission_running == 0, "killed as its enemy, it ends nothing");
        float stands = 0;
        entity e = nextent(world);
        while(e) { if(VR_IsDummy(e) && !e.vr_dummy_dead) stands++; e = nextent(e); }
        MG3_ST_Check(stands == 1, sprintf("a dummy stands again (%g)", stands));
        if(mg3st_step == 42)
        {
            e = nextent(world);
            while(e) { if(VR_IsDummy(e)) remove(e); e = nextent(e); }
            mg3st_step = 33; mg3st_next = time + 0.5;
            return;
        }
        cvar_set("vr_dummy_type", "0");
        MG3_ST_Total();
        return;
    }
};

// Each frame (world.qc StartFrame).
void() MG3_ShubTestFrame =
{
    if(mg3st_step && time >= mg3st_next)
    {
        entity os = self, oo = other, oa = activator;
        if(mg3st_step < 20) MG3_ST_PhaseStep();
        else if(mg3st_step < 30) MG3_ST_EndStep();
        else if(mg3st_step < 50) MG3_ST_DummyStep();
        else if(mg3st_step < 60) MG3_ST_ZombieStep();
        self = os; other = oo; activator = oa;
    }
    float request = cvar("vr_mg3_shubtest");
    if(!request) return;
    cvar_set("vr_mg3_shubtest", "0");
    if(!find(world, classname, "player")) return;
    entity oldself = self, oldother = other, oldact = activator;
    if(request == 1) MG3_ST_Report();
    else if(!MG3_Shub_Available()) dprint("mg3shubtest: the Dawn of the Machine data is not there (no Shub)\n");
    else if(request == 2) { mg3st_step = 1; mg3st_next = time; }
    else if(request == 4) { mg3st_step = 20; mg3st_next = time; }
    else if(request == 3)
    {
        if(mapname != "boss2") dprint("mg3shubtest: Dawn of the Machine's boss2 needed\n");
        else { mg3st_step = 50; mg3st_next = time; }
    }
    else if(request == 5) { mg3st_step = 30; mg3st_next = time; }
    else dprint(sprintf("mg3shubtest: unknown request %g\n", request));
    self = oldself; other = oldother; activator = oldact;
};
