#ifdef QVR_TESTS
// vr_arena_test.qc -- the Survival Arena's tests (vr_arena.qc). `vr_arena_test N` (Debug > Automation &
// Self-Tests; VR_ARENA_Test_Frame, world.qc's StartFrame), the player kept alive (`god`); each ends on
// `arena: test N over: P passed, F failed` (run.sh -QuitOn "arena: test [0-9]+ over").
// - 1 (any open map): the waves' plans: wave 1 unarmed, the ladder's rungs no earlier than their wave, Quake's own
//   monsters no earlier than theirs, the boss cadence and its kind, a plan never thicker than the cap, the score's
//   sums, a wave clear's drops, the knobs at work, the seed's determinism, and a fall counted once through
//   combat.qc's hook.
// - 2 (vrtesthall, vr_arena 1, god): the waves live: wave 1 unarmed in the field, the alive cap held (set to 2),
//   a fall scored the way combat scores it, the medkits a wave clear dropped, wave 2 with its nail pistol.
// - 3 (vrtesthall, vr_arena 1, god): the boss wave: the arena jumped to wave 5 has one boss, an enforcer with a
//   gun, worth a boss's tier, and no left-over foes of the wave it jumped over.
// - 4 (vrarena, or any map with vr_arena 1): the places a wave comes in on: the map's ground and high places are
//   known, each has ground under it to land on, the two kinds do not share a place, and a place already holding a
//   foe of this wave is passed over. On a map with no info_arena_spawn of its own the same checks run on the ring.
// - 5 (vrtesthall with vr_arena 1, vrarena, or any map with vr_arena 1): the difficulty scaling (vr_arena_skill_impact,
//   VR_ARENA_Skill): the factor at each skill and at impact 0 / 1 / 2, and what it does to a wave -- its size, the
//   field's cap, its pacing, the waves Quake's own monsters and the higher rungs join at, the unarmed share of the
//   early waves, a boss wave's bosses and their health, a wave clear's supplies, a kill's score and the perks a boss
//   clear offers. Every test family pins the skill (Normal) so its own numbers mean the cvars' values.

// ---------------------------------------------------------------------------
// The test's own state
// ---------------------------------------------------------------------------

float vr_at_case;        // 0 idle; 1, 2, 3, 4, 5 running
float vr_at_pass;
float vr_at_fail;
float vr_at_step;
float vr_at_until;
float vr_at_keep[8];     // the cvars the tests set, put back at the end
float vr_at_skill0;      // the skill the run had (its cvar's value; the tests pin it and put it back)
float vr_at_kills0;
float vr_at_score0;
float vr_at_a[AR_PLAN_MAX];
float vr_at_b[AR_PLAN_MAX];

string(float i) VR_AT_CvarName =
{
    if(i == 0) { return "vr_arena_seed"; }
    if(i == 1) { return "vr_arena_boss_every"; }
    if(i == 2) { return "vr_arena_boss_health"; }
    if(i == 3) { return "vr_arena_medkits_after"; }
    if(i == 4) { return "vr_arena_armor_every"; }
    if(i == 5) { return "vr_arena_score_kill"; }
    if(i == 6) { return "vr_arena_max_alive"; }
    if(i == 7) { return "vr_arena_skill_impact"; }
    return "";
}
float VR_AT_NCVARS = 8;

void() VR_AT_Save =
{
    float i;
    for(i = 0; i < VR_AT_NCVARS; i++) { vr_at_keep[i] = cvar(VR_AT_CvarName(i)); }
}

void() VR_AT_Restore =
{
    float i;
    for(i = 0; i < VR_AT_NCVARS; i++) { cvar_set(VR_AT_CvarName(i), ftos(vr_at_keep[i])); }
}

// The tests' own difficulty, both halves of it: the cvar (world.qc reads it into the `skill` global every frame)
// and the global itself. 0 Easy, 1 Normal, 2 Hard, 3 Nightmare.
void(float xSkill) VR_AT_SetSkill =
{
    cvar_set("skill", ftos(xSkill));
    skill = xSkill;
}

// The tests' settings: the arena's own defaults, so a check says what the plan says and not what a config said.
void() VR_AT_SetDefaults =
{
    cvar_set("vr_arena_seed", "7");
    cvar_set("vr_arena_boss_every", "5");
    cvar_set("vr_arena_boss_health", "1.25");
    cvar_set("vr_arena_medkits_after", "2");
    cvar_set("vr_arena_armor_every", "4");
    cvar_set("vr_arena_score_kill", "100");
    cvar_set("vr_arena_max_alive", "8");
    cvar_set("vr_arena_skill_impact", "1");
    VR_AT_SetSkill(1); // (Normal, where the skill's factor is exactly 1: every family's own numbers mean the cvars)
}

void(float ok, string what) VR_AT_Check =
{
    if(ok) { vr_at_pass = vr_at_pass + 1; return; }
    vr_at_fail = vr_at_fail + 1;
    dprint(sprintf("arena: fail %s\n", what));
}

void() VR_AT_End =
{
    dprint(sprintf("arena: test %g over: %g passed, %g failed\n", vr_at_case, vr_at_pass, vr_at_fail));
    VR_AT_Restore();
    VR_AT_SetSkill(vr_at_skill0);
    cvar_set("vr_arena_test", "0");
    vr_at_case = 0;
}

// ---------------------------------------------------------------------------
// Test 1: the waves' plans (no live monsters needed)
// ---------------------------------------------------------------------------

float(float xKind) VR_AT_CountKind =
{
    float n = 0;
    float i;
    for(i = 0; i < ar_plan_n; i++) { if(ar_plan_kind[i] == xKind) { n = n + 1; } }
    return n;
}

float(float xKind) VR_AT_IsHumanoid = { return xKind == AR_SP_GRUNT || xKind == AR_SP_ENFORCER; }

// The highest ladder rung an armed humanoid of the wave in the plan arrays holds.
float() VR_AT_HumanoidMaxTier =
{
    float m = 0;
    float i;
    for(i = 0; i < ar_plan_n; i++)
    {
        if(!VR_AT_IsHumanoid(ar_plan_kind[i])) { continue; }
        float t = VR_ARENA_WeaponTier(ar_plan_kind[i], ar_plan_gun[i]);
        if(t > m) { m = t; }
    }
    return m;
}

float() VR_AT_HumanoidCount =
{
    float n = 0;
    float i;
    for(i = 0; i < ar_plan_n; i++) { if(ar_plan_kind[i] == AR_SP_GRUNT) { n = n + 1; } }
    return n;
}

float() VR_AT_BossCount =
{
    float n = 0;
    float i;
    for(i = 0; i < ar_plan_n; i++) { if(ar_plan_boss[i]) { n = n + 1; } }
    return n;
}

float() VR_AT_GunSum =
{
    float s = 0;
    float i;
    for(i = 0; i < ar_plan_n; i++) { s = s + ar_plan_gun[i] * (i + 1); }
    return s;
}

void() VR_AT_Plans =
{
    float i;
    float w;

    // wave 1: one or two grunts, all of them unarmed, no boss
    VR_ARENA_Plan(1);
    VR_AT_Check(ar_plan_n >= 1 && ar_plan_n <= 2, "wave 1 size");
    VR_AT_Check(VR_AT_HumanoidCount() == ar_plan_n, "wave 1 is grunts only");
    for(i = 0; i < ar_plan_n; i++) { VR_AT_Check(ar_plan_gun[i] == AR_FIST, "wave 1 unarmed"); }
    VR_AT_Check(VR_AT_BossCount() == 0, "wave 1 has no boss");

    // wave 2: an unarmed grunt and one with a nail pistol, nothing above the ladder's first rung
    VR_ARENA_Plan(2);
    VR_AT_Check(ar_plan_n == 2, "wave 2 size");
    float fists = 0;
    float nailp = 0;
    for(i = 0; i < ar_plan_n; i++)
    {
        if(ar_plan_gun[i] == AR_FIST) { fists = fists + 1; }
        if(ar_plan_gun[i] == WID_NAILPISTOL) { nailp = nailp + 1; }
    }
    VR_AT_Check(fists == 1, "wave 2 keeps an unarmed grunt");
    VR_AT_Check(nailp == 1, "wave 2 brings a nail pistol");
    VR_AT_Check(VR_AT_HumanoidMaxTier() == 1, "wave 2 stays on the first rung");

    // wave 3: three or four grunts and a zombie; wave 4: four grunts and a knight
    VR_ARENA_Plan(3);
    VR_AT_Check(VR_AT_HumanoidCount() >= 3 && VR_AT_HumanoidCount() <= 4, "wave 3 grunt count");
    VR_AT_Check(VR_AT_CountKind(AR_SP_ZOMBIE) == 1, "wave 3 brings a zombie");
    VR_AT_Check(VR_AT_HumanoidMaxTier() == 1, "wave 3 stays on the first rung");

    VR_ARENA_Plan(4);
    VR_AT_Check(VR_AT_CountKind(AR_SP_KNIGHT) == 1, "wave 4 brings a knight");
    VR_AT_Check(VR_AT_HumanoidCount() == 4, "wave 4 grunt count");
    VR_AT_Check(VR_AT_HumanoidMaxTier() == 1, "wave 4 stays on the first rung");
    VR_AT_Check(VR_AT_CountKind(AR_SP_OGRE) == 0 && VR_AT_CountKind(AR_SP_FIEND) == 0 &&
                VR_AT_CountKind(AR_SP_HKNIGHT) == 0 && VR_AT_CountKind(AR_SP_DOG) == 0, "wave 4 monster mix");

    // boss waves: every vr_arena_boss_every (5), the boss first in the plan
    VR_ARENA_Plan(5);
    VR_AT_Check(VR_AT_BossCount() == 1, "wave 5 has one boss");
    VR_AT_Check(ar_plan_boss[0] && ar_plan_kind[0] == AR_SP_ENFORCER, "wave 5's boss is an enforcer");
    // A plan's tier is the rung its boss holds (the boss's own mark lands on the monster's ar_tier, where test 3
    // reads it); what it is worth: four of that rung.
    VR_AT_Check(ar_plan_tier[0] == 1, "wave 5's boss holds a first rung gun");
    VR_AT_Check(VR_ARENA_KillScore(ar_plan_tier[0], TRUE) == 400, "a first rung boss is scored four of its rung");
    VR_AT_Check(VR_AT_HumanoidMaxTier() <= 1, "wave 5 stays on the first rung");

    VR_ARENA_Plan(10);
    VR_AT_Check(VR_AT_BossCount() == 1 && ar_plan_kind[0] == AR_SP_OGRE, "wave 10's boss is an ogre");
    VR_AT_Check(VR_AT_HumanoidMaxTier() <= 2, "wave 10 reaches the second rung");

    VR_ARENA_Plan(15);
    VR_AT_Check(VR_AT_BossCount() == 1 && ar_plan_kind[0] == AR_SP_OGRE, "wave 15's boss is an ogre");

    VR_ARENA_Plan(20);
    VR_AT_Check(VR_AT_BossCount() == 1 && ar_plan_kind[0] == AR_SP_HKNIGHT, "wave 20's boss is a death knight");
    VR_AT_Check(VR_AT_HumanoidMaxTier() <= 3, "wave 20 reaches the third rung");

    // the ladder's rungs, and Quake's own monsters: none of them before their wave
    for(w = 1; w <= 24; w++)
    {
        VR_ARENA_Plan(w);
        VR_AT_Check(ar_plan_n >= 1 && ar_plan_n <= AR_PLAN_MAX, "a wave is planned 1 to the cap");
        VR_AT_Check(VR_AT_HumanoidCount() <= 7, "a wave's grunts are capped");
        VR_AT_Check(VR_AT_CountKind(AR_SP_ENFORCER) <= 4, "a wave's enforcers are capped");

        if(w < AR_W_TIER2)   { VR_AT_Check(VR_AT_HumanoidMaxTier() <= 1, "no second rung before wave 10"); }
        if(w < AR_W_TIER3)   { VR_AT_Check(VR_AT_HumanoidMaxTier() <= 2, "no third rung before wave 18"); }
        if(w < AR_W_ZOMBIE)  { VR_AT_Check(VR_AT_CountKind(AR_SP_ZOMBIE) == 0, "no zombie before wave 2"); }
        if(w < 4)            { VR_AT_Check(VR_AT_CountKind(AR_SP_KNIGHT) == 0, "no knight before wave 4"); }
        if(w < AR_W_DOG)     { VR_AT_Check(VR_AT_CountKind(AR_SP_DOG) == 0, "no dog before wave 6"); }
        if(w < AR_W_OGRE)    { VR_AT_Check(VR_AT_CountKind(AR_SP_OGRE) == 0, "no ogre before wave 8"); }
        if(w < AR_W_FIEND)   { VR_AT_Check(VR_AT_CountKind(AR_SP_FIEND) == 0, "no fiend before wave 12"); }
        if(w < AR_W_HKNIGHT) { VR_AT_Check(VR_AT_CountKind(AR_SP_HKNIGHT) == 0, "no death knight before wave 16"); }

        float boss = VR_ARENA_IsBossWave(w);
        VR_AT_Check(boss == (VR_ARENA_Mod(w, 5) == 0), "the boss cadence");
        VR_AT_Check(boss ? VR_AT_BossCount() == 1 : VR_AT_BossCount() == 0, "one boss on a boss wave");
    }

    // the third rung does show up once the waves have reached it
    float tier3 = 0;
    for(w = 19; w <= 24; w++)
    {
        VR_ARENA_Plan(w);
        if(VR_AT_HumanoidMaxTier() == 3) { tier3 = 1; }
    }
    VR_AT_Check(tier3 == 1, "the third rung comes once the waves reach it");

    // the score's sums: 100 a kill, times the tier, a boss four of that; 250 a wave clear, times the wave
    VR_AT_Check(VR_ARENA_KillScore(1, 0) == 100, "a first rung kill is 100");
    VR_AT_Check(VR_ARENA_KillScore(2, 0) == 200, "a second rung kill is 200");
    VR_AT_Check(VR_ARENA_KillScore(3, 0) == 300, "a third rung kill is 300");
    VR_AT_Check(VR_ARENA_KillScore(1, 1) == 400, "a boss kill is four of its tier");
    VR_AT_Check(VR_ARENA_KillScore(3, 1) == 1200, "a third rung boss is 1200");
    VR_AT_Check(VR_ARENA_KillScore(0, 0) == 100, "an unset tier counts as the first rung");
    VR_AT_Check(VR_ARENA_WaveBonus(3) == 750, "a wave clear is 250 times the wave");

    // what a wave clear drops
    VR_AT_Check(VR_ARENA_MedkitCount(1) == 2, "two medkits after a wave");
    VR_AT_Check(VR_ARENA_MedkitCount(9) == 2, "a wave clear's medkits do not thin out");
    VR_AT_Check(VR_ARENA_ArmourKind(4) == 1, "an armour after every 4 waves");
    VR_AT_Check(VR_ARENA_ArmourKind(5) == 0, "no armour between");
    VR_AT_Check(VR_ARENA_ArmourKind(12) == 2, "the heavier armour from wave 12");
    VR_AT_Check(VR_ARENA_ArmourKind(16) == 1, "the light armour still comes");

    // what a monster or a gun is worth on the ladder
    VR_AT_Check(VR_ARENA_WeaponTier(AR_SP_GRUNT, AR_FIST) == 1, "fists are the first rung");
    VR_AT_Check(VR_ARENA_WeaponTier(AR_SP_GRUNT, WID_SHOTGUN) == 1, "a shotgun is the first rung");
    VR_AT_Check(VR_ARENA_WeaponTier(AR_SP_GRUNT, WID_NAILPISTOL) == 1, "a nail pistol is the first rung");
    VR_AT_Check(VR_ARENA_WeaponTier(AR_SP_GRUNT, WID_AXE) == 1, "an axe is the first rung");
    VR_AT_Check(VR_ARENA_WeaponTier(AR_SP_GRUNT, WID_SUPER_SHOTGUN) == 2, "a super shotgun is the second rung");
    VR_AT_Check(VR_ARENA_WeaponTier(AR_SP_GRUNT, WID_NAILGUN) == 2, "a nail gun is the second rung");
    VR_AT_Check(VR_ARENA_WeaponTier(AR_SP_GRUNT, WID_LIGHTNING) == 3, "lightning is the third rung");
    VR_AT_Check(VR_ARENA_WeaponTier(AR_SP_GRUNT, WID_ROCKET_LAUNCHER) == 3, "a rocket launcher is the third rung");
    VR_AT_Check(VR_ARENA_WeaponTier(AR_SP_OGRE, 0) == 2, "an ogre is worth the second rung");
    VR_AT_Check(VR_ARENA_WeaponTier(AR_SP_FIEND, 0) == 2, "a fiend is worth the second rung");
    VR_AT_Check(VR_ARENA_WeaponTier(AR_SP_HKNIGHT, 0) == 3, "a death knight is worth the third rung");

    // the knobs: they drive the plans, they are not baked in
    cvar_set("vr_arena_boss_every", "0");
    float anyboss = 0;
    for(w = 1; w <= 20; w++) { VR_ARENA_Plan(w); if(VR_AT_BossCount()) { anyboss = 1; } }
    VR_AT_Check(anyboss == 0, "vr_arena_boss_every 0: no boss waves");
    cvar_set("vr_arena_boss_every", "3");
    VR_ARENA_Plan(3);
    VR_AT_Check(VR_AT_BossCount() == 1, "vr_arena_boss_every 3: wave 3 has a boss");
    VR_ARENA_Plan(4);
    VR_AT_Check(VR_AT_BossCount() == 0, "vr_arena_boss_every 3: wave 4 has none");
    cvar_set("vr_arena_boss_every", "5");

    cvar_set("vr_arena_score_kill", "50");
    VR_AT_Check(VR_ARENA_KillScore(2, 0) == 100, "vr_arena_score_kill 50: a second rung kill is 100");
    cvar_set("vr_arena_score_kill", "100");

    cvar_set("vr_arena_medkits_after", "0");
    VR_AT_Check(VR_ARENA_MedkitCount(3) == 0, "vr_arena_medkits_after 0: no medkits");
    cvar_set("vr_arena_medkits_after", "2");

    cvar_set("vr_arena_armor_every", "0");
    VR_AT_Check(VR_ARENA_ArmourKind(4) == 0, "vr_arena_armor_every 0: no armour");
    cvar_set("vr_arena_armor_every", "4");

    // the seed: the same seed's ladder every run, another seed another ladder
    cvar_set("vr_arena_seed", "7");
    VR_ARENA_Plan(7);  vr_at_a[0] = VR_AT_GunSum(); vr_at_a[3] = ar_plan_n;
    VR_ARENA_Plan(11); vr_at_a[1] = VR_AT_GunSum(); vr_at_a[4] = ar_plan_n;
    VR_ARENA_Plan(19); vr_at_a[2] = VR_AT_GunSum(); vr_at_a[5] = ar_plan_n;
    VR_ARENA_Plan(7);  vr_at_b[0] = VR_AT_GunSum(); vr_at_b[3] = ar_plan_n;
    VR_ARENA_Plan(11); vr_at_b[1] = VR_AT_GunSum(); vr_at_b[4] = ar_plan_n;
    VR_ARENA_Plan(19); vr_at_b[2] = VR_AT_GunSum(); vr_at_b[5] = ar_plan_n;
    VR_AT_Check(vr_at_a[0] == vr_at_b[0] && vr_at_a[1] == vr_at_b[1] && vr_at_a[2] == vr_at_b[2] &&
                vr_at_a[3] == vr_at_b[3] && vr_at_a[4] == vr_at_b[4] && vr_at_a[5] == vr_at_b[5],
                "the same seed draws the same ladder twice");
    cvar_set("vr_arena_seed", "2031");
    VR_ARENA_Plan(7);  vr_at_b[0] = VR_AT_GunSum();
    VR_ARENA_Plan(11); vr_at_b[1] = VR_AT_GunSum();
    VR_ARENA_Plan(19); vr_at_b[2] = VR_AT_GunSum();
    VR_AT_Check(vr_at_b[0] != vr_at_a[0] || vr_at_b[1] != vr_at_a[1] || vr_at_b[2] != vr_at_a[2],
                "another seed draws another ladder");
    cvar_set("vr_arena_seed", "7");

    // the wave in the field is planned, and re-planned the same way
    VR_ARENA_Plan(13);
    vr_at_a[0] = VR_AT_GunSum(); float n13 = ar_plan_n;
    VR_ARENA_Plan(13);
    VR_AT_Check(vr_at_a[0] == VR_AT_GunSum() && n13 == ar_plan_n, "a wave re-planned is the same wave");
}

// combat.qc's hook, on a dummy of the wave: its score, once, and a boss's worth.
// Is `xEnt` still in use: an arena-marked edict findflags still walks to (it passes over freed ones).
float(entity xEnt) VR_AT_InUse =
{
    entity e = findflags(world, ar_slot, xEnt.ar_slot);
    while(e)
    {
        if(e == xEnt) { return TRUE; }
        e = findflags(e, ar_slot, xEnt.ar_slot);
    }
    return FALSE;
}

void() VR_AT_HookCheck =
{
    entity save = self;
    entity mgr0 = ar_mgr;
    float wave0 = ar_wave;
    float score0 = ar_score;
    float kills0 = ar_kills;
    entity dummy;

    if(!ar_mgr) { ar_mgr = spawn(); } // (the hook's own gate: the arena runs)

    ar_wave = 3;
    ar_score = 0;
    ar_kills = 0;

    dummy = spawn();
    self = dummy;
    dummy.classname = "monster_army";
    dummy.ar_slot = 3;
    dummy.ar_tier = 2;
    VR_ARENA_MonsterDie();
    VR_AT_Check(ar_score == 200, "a second rung fall is scored 200");
    VR_AT_Check(ar_kills == 1, "a fall is counted once");
    VR_AT_Check(dummy.ar_slot == -3, "the fall's monster stops being counted");
    VR_AT_Check(VR_ARENA_CountAlive() == 0, "a fallen monster is not alive");
    VR_ARENA_MonsterDie();
    VR_AT_Check(ar_score == 200 && ar_kills == 1, "the same fall is not scored twice");

    dummy.ar_slot = 3;
    dummy.ar_tier = 2 + AR_BOSS_MARK;
    VR_ARENA_MonsterDie();
    VR_AT_Check(ar_score == 1000, "a boss fall is scored four of its tier");

    // A stalled wave's clear takes exactly its own foes still up: not an older wave's body (ar_slot -1 shares a bit
    // with 5), and a foe in a player's hand is let go of before it goes.
    entity body = spawn();
    body.classname = "monster_army";
    body.ar_slot = -1;
    entity foe = spawn();
    foe.classname = "monster_army";
    foe.ar_slot = 5;
    foe.health = 30;
    entity holder = VR_ARENA_FindPlayer();
    entity held0 = world;
    if(holder) { held0 = holder.mainhand_held; holder.mainhand_held = foe; foe.carry_player = holder; }
    VR_ARENA_ClearField(5);
    VR_AT_Check(VR_AT_InUse(body), "a stalled wave's clear leaves an older wave's body");
    VR_AT_Check(!VR_AT_InUse(foe), "a stalled wave's clear takes its foe still up");
    if(holder)
    {
        VR_AT_Check(holder.mainhand_held == world, "a stalled wave's foe in a hand is let go of first");
        holder.mainhand_held = held0;
    }
    if(VR_AT_InUse(foe)) { remove(foe); }
    if(VR_AT_InUse(body)) { remove(body); }

    remove(dummy);
    self = save;
    if(!mgr0) { remove(ar_mgr); }
    ar_mgr = mgr0;
    ar_wave = wave0;
    ar_score = score0;
    ar_kills = kills0;
}

// ---------------------------------------------------------------------------
// Test 2: the waves, live
// ---------------------------------------------------------------------------

void() VR_AT2_Begin =
{
    if(!ar_mgr) { VR_AT_Check(FALSE, "test 2 needs the arena on (vr_arena 1)"); VR_AT_End(); return; }
    cvar_set("vr_arena_max_alive", "2"); // (the field's cap, small enough for the waves to have to respect it)
    vr_at_until = time + 60;
    dprint("arena: test 2 waves\n");
}

// One of a wave's foes, brought down by the test's own blow (its path out of combat.qc, so the score's whole route
// is walked). Its kind's start has to be through first (monsters.qc: that is where its takedamage comes on), so a
// foe still on its way to the floor is left for the next frame. 1 when a blow landed.
float(float xWave) VR_AT_KillOne =
{
    entity p = VR_ARENA_FindPlayer();
    if(p == world) { return 0; }

    entity e = findflags(world, ar_slot, xWave);
    while(e)
    {
        entity next = findflags(e, ar_slot, xWave);
        if(e.health > 0 && e.takedamage)
        {
            T_Damage(e, p, p, e.health + 100);
            return 1;
        }
        e = next;
    }
    return 0;
}

void() VR_AT2_Frame =
{
    entity e;
    entity p = VR_ARENA_FindPlayer();

    if(!p) { VR_AT_Check(FALSE, "test 2 needs a player in the map"); VR_AT_End(); return; }

    if(vr_at_step == 0) // (wave 1: its foes, all of them unarmed grunts)
    {
        if(ar_wave < 1 || ar_plan_live < ar_plan_n) { return; }

        float armed = 0;
        float fists = 0;
        float up = 0;
        e = findflags(world, ar_slot, 1);
        while(e)
        {
            if(e.health > 0)
            {
                up = up + 1;
                if(e.vr_ag) { armed = armed + 1; }
                if(e.vr_ag_code == AR_FIST) { fists = fists + 1; }
            }
            e = findflags(e, ar_slot, 1);
        }
        VR_AT_Check(ar_plan_n >= 1 && ar_plan_n <= 2, "wave 1 puts 1 to 2 foes in the field");
        VR_AT_Check(up == ar_plan_n, "wave 1's plan is in the field");
        VR_AT_Check(armed == up, "wave 1's foes are armed humanoids");
        VR_AT_Check(fists == up, "wave 1's foes are at their fists");
        VR_AT_Check(ar_alive_peak <= 2, "the field's cap is held");

        vr_at_kills0 = ar_kills;
        vr_at_score0 = ar_score;
        vr_at_step = 1;
        vr_at_until = time + 60;
        return;
    }

    if(vr_at_step == 1) // (one fall: the score's path through combat.qc)
    {
        if(!VR_AT_KillOne(1)) { return; }
        vr_at_step = 2;
        vr_at_until = time + 60;
        return;
    }

    if(vr_at_step == 2) // (that fall was scored the way the arena says)
    {
        entity d = findflags(world, ar_slot, 1);
        while(d) // (what the blow left behind, so a miss here says where it went)
        {
            dprint(sprintf("arena: t2 foe health %g slot %g tier %g takedamage %g class %s\n",
                           d.health, d.ar_slot, d.ar_tier, d.takedamage, d.classname));
            d = findflags(d, ar_slot, 1);
        }
        dprint(sprintf("arena: t2 kills %g score %g wave %g alive %g\n", ar_kills, ar_score, ar_wave, VR_ARENA_CountAlive()));

        VR_AT_Check(ar_kills - vr_at_kills0 == 1, "combat.qc's hook counted the fall");
        VR_AT_Check(ar_score - vr_at_score0 == 100, "an unarmed fall is worth 100");
        vr_at_step = 3;
        return;
    }

    if(vr_at_step == 3) // (the rest of the wave goes: its clear drops the medkits)
    {
        VR_AT_KillOne(1);
        if(ar_wave >= 2)
        {
            float kits = 0;
            entity it = findflags(world, ar_drop, 1);
            while(it) { kits = kits + 1; it = findflags(it, ar_drop, 1); }
            VR_AT_Check(ar_drop_health == VR_ARENA_MedkitCount(1), "the wave clear made its medkits");
            VR_AT_Check(kits >= 1, "the medkits are in the map");
            VR_AT_Check(ar_score >= 100 + 250, "the wave clear's bonus was paid");
            vr_at_step = 4;
            vr_at_until = time + 60;
        }
        return;
    }

    if(vr_at_step == 4) // (wave 2: the ladder's next rung, a nail pistol)
    {
        if(ar_plan_live < ar_plan_n) { return; }
        float got = 0;
        e = findflags(world, ar_slot, 2);
        while(e)
        {
            if(e.health > 0 && e.vr_ag_code == WID_NAILPISTOL) { got = 1; }
            e = findflags(e, ar_slot, 2);
        }
        VR_AT_Check(ar_wave == 2, "the arena went on to wave 2");
        VR_AT_Check(got == 1, "wave 2 brings a nail pistol");
        VR_AT_Check(ar_alive_peak <= 2, "the field's cap held across the waves");
        VR_AT_End();
        return;
    }
}

// ---------------------------------------------------------------------------
// Test 3: a boss wave, live
// ---------------------------------------------------------------------------

void() VR_AT3_Begin =
{
    if(!ar_mgr) { VR_AT_Check(FALSE, "test 3 needs the arena on (vr_arena 1)"); VR_AT_End(); return; }
    VR_ARENA_FastForward(5);
    vr_at_until = time + 40;
    dprint("arena: test 3 boss\n");
}

void() VR_AT3_Frame =
{
    if(vr_at_step == 0)
    {
        if(ar_wave < 5) { return; }

        VR_AT_Check(ar_wave == 5, "the arena jumped to its boss wave");
        VR_AT_Check(ar_plan_boss[0] == 1 && ar_plan_kind[0] == AR_SP_ENFORCER, "wave 5's plan leads with its boss");
        VR_AT_Check(VR_AT_BossCount() == 1, "a boss wave has one boss");

        entity boss = world;
        entity e = findflags(world, ar_slot, 5);
        while(e)
        {
            if(e.health > 0 && e.ar_tier >= AR_BOSS_MARK) { boss = e; }
            e = findflags(e, ar_slot, 5);
        }
        if(!boss) { return; } // (its own spawn comes a beat after the wave begins)

        VR_AT_Check(boss.vr_ag == 1, "the boss is an armed humanoid");
        VR_AT_Check(boss.ar_tier == 1 + AR_BOSS_MARK, "the boss is worth a boss's tier");
        VR_AT_Check(boss.vr_ag_code > 0, "the boss holds a gun");
        VR_AT_Check(ar_alive_peak <= VR_ARENA_FieldCap(), "the boss wave respects the field's cap");
        VR_AT_End();
        return;
    }
}

// ---------------------------------------------------------------------------
// Test 4: the places a wave comes in on
// ---------------------------------------------------------------------------

// How far down from a place the first solid is, read off the brushes (a big number when nothing is under it): a foe
// put on a place has to land on something.
float(vector p) VR_AT_FloorUnder =
{
    traceline(p + '0 0 4', p + '0 0 -600', TRUE, world); // (TRUE: its own foes are not what is being looked for)
    if(trace_allsolid || trace_startsolid || trace_fraction >= 1) { return 9999; }
    return p_z - trace_endpos_z;
}

void() VR_AT4_Begin =
{
    if(!ar_mgr) { VR_AT_Check(FALSE, "test 4 needs the arena on (vr_arena 1)"); VR_AT_End(); return; }
    vr_at_step = 0;
    vr_at_until = time + 60;
    dprint("arena: t4 places\n");
}

void() VR_AT4_Frame =
{
    if(vr_at_step == 0)
    {
        // Its first wave has to be out: the places are chosen as the waves are built, and a place is only known to
        // be held while that wave is in the field.
        if(ar_wave < 1) { return; }

        float ground = VR_ARENA_SpotCount(1);
        float high = VR_ARENA_SpotCount(2);
        dprint(sprintf("arena: t4 places: ground %g high %g\n", ground, high));

        vector g = VR_ARENA_SpawnPlace(0);
        vector h = VR_ARENA_SpawnPlace(1);
        float fg = VR_AT_FloorUnder(g);
        float fh = VR_AT_FloorUnder(h);
        dprint(sprintf("arena: t4 ground place %g %g %g, %g down\n", g_x, g_y, g_z, fg));
        dprint(sprintf("arena: t4 high place %g %g %g, %g down\n", h_x, h_y, h_z, fh));

        VR_AT_Check(fg < 300, "a foe put on a ground place has ground under it");
        VR_AT_Check(fh < 300, "a foe put on a high place has ground to drop onto");
        VR_AT_Check(vlen(g - h) > 1, "the two kinds of place are not the same place");

        if(high > 0)
        {
            VR_AT_Check(ground > 0, "a map with high places has ground places too");
            VR_AT_Check(h_z > g_z, "the high places are above the ground ones");
        }

        // A place already holding a foe of this wave is passed over, not shared.
        entity dummy = spawn();
        dummy.classname = "arena_test_place";
        dummy.origin = g;
        dummy.health = 1; // (a place only counts as held while something live is standing on it)
        dummy.ar_slot = ar_wave;

        vector next = VR_ARENA_SpawnPlace(0);
        dprint(sprintf("arena: t4 past a taken place: %g %g %g\n", next_x, next_y, next_z));
        VR_AT_Check(vlen(next - g) > 1, "a place already holding a foe is passed over");
        VR_AT_Check(VR_AT_FloorUnder(next) < 600, "the place it fell back to has ground under it");

        remove(dummy);
        VR_AT_End();
        return;
    }
}

// ---------------------------------------------------------------------------
// Test 5: the difficulty scaling (vr_arena_skill_impact, VR_ARENA_Skill)
// ---------------------------------------------------------------------------

// The steps this test measures the factor against -- the ones VR_ARENA_Skill's table promises (Normal 1).
float VR_AT5_EASY      = 0.75;
float VR_AT5_HARD      = 1.25;
float VR_AT5_NIGHTMARE = 1.5;

float() VR_AT5_Factor = { return VR_ARENA_Skill(VR_AT5_EASY, VR_AT5_HARD, VR_AT5_NIGHTMARE); }

// The factor at every skill for one impact: xEasy / xHard / xNightmare what it must come to at Easy, Hard and
// Nightmare (Normal is always exactly 1 -- as the knobs say).
void(float xImpact, float xEasy, float xHard, float xNight, string xWhat) VR_AT5_FactorAt =
{
    cvar_set("vr_arena_skill_impact", ftos(xImpact));

    VR_AT_SetSkill(0); VR_AT_Check(VR_AT5_Factor() == xEasy,  sprintf("%s at Easy", xWhat));
    VR_AT_SetSkill(1); VR_AT_Check(VR_AT5_Factor() == 1,      sprintf("%s at Normal", xWhat));
    VR_AT_SetSkill(2); VR_AT_Check(VR_AT5_Factor() == xHard,  sprintf("%s at Hard", xWhat));
    VR_AT_SetSkill(3); VR_AT_Check(VR_AT5_Factor() == xNight, sprintf("%s at Nightmare", xWhat));

    float sk;
    for(sk = 0; sk <= 3; sk++)
    {
        VR_AT_SetSkill(sk);
        dprint(sprintf("arena: t5 impact %g skill %g factor %g\n", cvar("vr_arena_skill_impact"), sk, VR_AT5_Factor()));
    }
}

// A wave's plan at this skill: the same seed and the same rolls as always -- only its counts move.
float(float xWave) VR_AT5_PlanSize = { VR_ARENA_Plan(xWave); return ar_plan_n; }

// xWave with what it must plan at Easy, Normal, Hard and Nightmare (vr_arena_seed 7).
void(float xWave, float xEasy, float xNormal, float xHard, float xNight, string xWhat) VR_AT5_PlanSizes =
{
    float sk;
    for(sk = 0; sk <= 3; sk++)
    {
        float want = sk == 0 ? xEasy : sk == 1 ? xNormal : sk == 2 ? xHard : xNight;
        VR_AT_SetSkill(sk);
        float got = VR_AT5_PlanSize(xWave);
        VR_AT_Check(got == want, sprintf("%s at skill %g plans %g foes (want %g)", xWhat, sk, got, want));
        dprint(sprintf("arena: t5 %s skill %g -> %g foes\n", xWhat, sk, got));
    }
    VR_AT_SetSkill(1);
}

// vr_arena_max_alive as the field reads it at each skill: the knob times the factor, whole foes, never past the
// plan's cap and never below one.
void(float xKnob, float xEasy, float xNormal, float xHard, float xNight) VR_AT5_Caps =
{
    float sk;
    cvar_set("vr_arena_max_alive", ftos(xKnob));
    for(sk = 0; sk <= 3; sk++)
    {
        float want = sk == 0 ? xEasy : sk == 1 ? xNormal : sk == 2 ? xHard : xNight;
        VR_AT_SetSkill(sk);
        float got = VR_ARENA_FieldCap();
        VR_AT_Check(got == want && got >= 1 && got <= AR_PLAN_MAX,
                    sprintf("vr_arena_max_alive %g at skill %g is the field's cap %g (got %g)", xKnob, sk, want, got));
        dprint(sprintf("arena: t5 cap of knob %g at skill %g -> %g up at once\n", xKnob, sk, got));
    }
    cvar_set("vr_arena_max_alive", "8");
    VR_AT_SetSkill(1);
}

// A gate of the ladder (a rung, a species, the early waves' unarmed window) at each skill: whole waves earlier at
// Hard and Nightmare, later at Easy, never earlier than wave 2. The numbers below are the table's gate plus
// +3 / 0 / -2 / -4.
void(float xGate, float xEasy, float xNormal, float xHard, float xNight, string xWhat) VR_AT5_Gate =
{
    float sk;
    for(sk = 0; sk <= 3; sk++)
    {
        float want = sk == 0 ? xEasy : sk == 1 ? xNormal : sk == 2 ? xHard : xNight;
        VR_AT_SetSkill(sk);
        float got = VR_ARENA_WaveAdj(xGate);
        VR_AT_Check(got == want && got >= 2, sprintf("%s arrives at wave %g at skill %g (got %g)", xWhat, want, sk, got));
        dprint(sprintf("arena: t5 %s at skill %g -> wave %g\n", xWhat, sk, got));
    }
    VR_AT_SetSkill(1);
}

// How many of a wave's planned humanoids come unarmed (ar_gun AR_FIST).
float() VR_AT5_Unarmed =
{
    float n = 0;
    float i;
    for(i = 0; i < ar_plan_n; i++)
    {
        if(VR_AT_IsHumanoid(ar_plan_kind[i]) && ar_plan_gun[i] == AR_FIST) { n = n + 1; }
    }
    return n;
}

// The first waves' unarmed share and its window of waves: wider, and further into the early waves, the softer the
// difficulty. (The share itself is not a binary-exact number, so it is checked as an order and a bound.)
void() VR_AT5_Early =
{
    VR_AT_SetSkill(0); float e = VR_ARENA_EarlyUnarmed(); float ew = VR_ARENA_WaveAdj(4);
    VR_AT_SetSkill(1); float n = VR_ARENA_EarlyUnarmed(); float nw = VR_ARENA_WaveAdj(4);
    VR_AT_SetSkill(2); float h = VR_ARENA_EarlyUnarmed();
    VR_AT_SetSkill(3); float m = VR_ARENA_EarlyUnarmed(); float mw = VR_ARENA_WaveAdj(4);

    VR_AT_Check(e > n && n > h && h > m, "more of the first waves' humanoids come unarmed at Easy, fewer at Nightmare");
    VR_AT_Check(m > 0 && e <= 0.5, "the unarmed share stays a share");
    VR_AT_Check(ew == 7 && nw == 4 && mw == 2, "and its window reaches wave 7 at Easy, wave 4 at Normal, wave 2 at Nightmare");

    // The plan obeys the window: at Nightmare the early waves send nobody unarmed.
    VR_ARENA_Plan(3); VR_AT_Check(VR_AT5_Unarmed() == 0, "no unarmed grunt at wave 3 at Nightmare");
    VR_ARENA_Plan(4); VR_AT_Check(VR_AT5_Unarmed() == 0, "no unarmed grunt at wave 4 at Nightmare");

    // vr_arena_skill_impact 0: the first waves are today's, whatever the skill.
    cvar_set("vr_arena_skill_impact", "0");
    VR_AT_SetSkill(3);
    VR_AT_Check(VR_ARENA_EarlyUnarmed() == 0.20 && VR_ARENA_WaveAdj(4) == 4, "vr_arena_skill_impact 0 leaves the early waves as the knobs set them");
    cvar_set("vr_arena_skill_impact", "1");
    dprint(sprintf("arena: t5 unarmed share %g %g %g %g (Easy Normal Hard Nightmare), window wave %g / %g / %g\n", e, n, h, m, ew, nw, mw));
    VR_AT_SetSkill(1);
}

// The bestiary and a boss's own kind follow the gates: a species never before its wave, and its wave is the
// difficulty's.
void() VR_AT5_Bestiary =
{
    VR_AT_SetSkill(1);
    VR_AT_Check(VR_ARENA_ClassicKind(5, 1) == AR_SP_ZOMBIE && VR_ARENA_ClassicKind(6, 1) == AR_SP_DOG, "a dog from wave 6 at Normal");
    VR_AT_Check(VR_ARENA_ClassicKind(3, 2) == AR_SP_ZOMBIE && VR_ARENA_ClassicKind(4, 2) == AR_SP_KNIGHT, "a knight from wave 4 at Normal");
    VR_AT_Check(VR_ARENA_ClassicKind(7, 2) == AR_SP_KNIGHT && VR_ARENA_ClassicKind(8, 2) == AR_SP_OGRE, "an ogre from wave 8 at Normal");
    VR_AT_Check(VR_ARENA_ClassicKind(11, 3) == AR_SP_KNIGHT && VR_ARENA_ClassicKind(12, 3) == AR_SP_FIEND, "a fiend from wave 12 at Normal");
    VR_AT_Check(VR_ARENA_ClassicKind(15, 3) == AR_SP_FIEND && VR_ARENA_ClassicKind(16, 3) == AR_SP_HKNIGHT, "a death knight from wave 16 at Normal");
    VR_AT_Check(VR_ARENA_BossKind(9) == AR_SP_ENFORCER && VR_ARENA_BossKind(10) == AR_SP_OGRE, "an ogre boss from wave 10 at Normal");
    VR_AT_Check(VR_ARENA_BossKind(15) == AR_SP_OGRE && VR_ARENA_BossKind(16) == AR_SP_HKNIGHT, "a death knight boss from wave 16 at Normal");

    VR_AT_SetSkill(3);
    VR_AT_Check(VR_ARENA_ClassicKind(1, 1) == AR_SP_ZOMBIE && VR_ARENA_ClassicKind(2, 1) == AR_SP_DOG, "a dog from wave 2 at Nightmare");
    VR_AT_Check(VR_ARENA_ClassicKind(3, 2) == AR_SP_KNIGHT && VR_ARENA_ClassicKind(4, 2) == AR_SP_OGRE, "a knight from wave 2 and an ogre from wave 4 at Nightmare");
    VR_AT_Check(VR_ARENA_ClassicKind(7, 3) == AR_SP_KNIGHT && VR_ARENA_ClassicKind(8, 3) == AR_SP_FIEND, "a fiend from wave 8 at Nightmare");
    VR_AT_Check(VR_ARENA_ClassicKind(11, 3) == AR_SP_FIEND && VR_ARENA_ClassicKind(12, 3) == AR_SP_HKNIGHT, "a death knight from wave 12 at Nightmare");
    VR_AT_Check(VR_ARENA_BossKind(5) == AR_SP_ENFORCER && VR_ARENA_BossKind(6) == AR_SP_OGRE, "an ogre boss from wave 6 at Nightmare");
    VR_AT_Check(VR_ARENA_BossKind(11) == AR_SP_OGRE && VR_ARENA_BossKind(12) == AR_SP_HKNIGHT, "a death knight boss from wave 12 at Nightmare");

    VR_AT_SetSkill(0);
    VR_AT_Check(VR_ARENA_ClassicKind(8, 1) == AR_SP_ZOMBIE && VR_ARENA_ClassicKind(9, 1) == AR_SP_DOG, "a dog not before wave 9 at Easy");
    VR_AT_Check(VR_ARENA_ClassicKind(3, 2) == AR_SP_ZOMBIE && VR_ARENA_ClassicKind(7, 2) == AR_SP_KNIGHT, "a knight from wave 7 at Easy");
    VR_AT_Check(VR_ARENA_ClassicKind(10, 2) == AR_SP_KNIGHT && VR_ARENA_ClassicKind(11, 2) == AR_SP_OGRE, "an ogre not before wave 11 at Easy");
    VR_AT_Check(VR_ARENA_ClassicKind(18, 3) == AR_SP_FIEND && VR_ARENA_ClassicKind(19, 3) == AR_SP_HKNIGHT, "a death knight not before wave 19 at Easy");
    VR_AT_Check(VR_ARENA_BossKind(12) == AR_SP_ENFORCER && VR_ARENA_BossKind(13) == AR_SP_OGRE, "an ogre boss not before wave 13 at Easy");
    dprint("arena: t5 boss kinds: an ogre boss from wave 13 / 10 / 6 and a death knight boss from wave 19 / 16 / 12 (Easy Normal Nightmare)\n");
    VR_AT_SetSkill(1);
}

// The live plans obey the gates whatever the skill: no rung and no species before its wave, and a rung that its
// wave has unlocked does turn up (VR_ARENA_PickGun picks the gun, so its first appearance is the gate's wave or
// a wave or two after it).
void(float xSkill) VR_AT5_ObeyGates =
{
    float w;
    float seen2 = 0;
    float seen3 = 0;
    float first2 = 0;
    float g2;
    float g3;
    float gd;
    float go;
    float gf;
    float gh;

    // (This test's whole point is the order: pin the difficulty first, then read what its gates are.)
    VR_AT_SetSkill(xSkill);
    g2 = VR_ARENA_WaveAdj(AR_W_TIER2);
    g3 = VR_ARENA_WaveAdj(AR_W_TIER3);
    gd = VR_ARENA_WaveAdj(AR_W_DOG);
    go = VR_ARENA_WaveAdj(AR_W_OGRE);
    gf = VR_ARENA_WaveAdj(AR_W_FIEND);
    gh = VR_ARENA_WaveAdj(AR_W_HKNIGHT);
    for(w = 1; w <= 34; w++)
    {
        VR_ARENA_Plan(w);
        if(w < g2) { VR_AT_Check(VR_AT_HumanoidMaxTier() <= 1, sprintf("no second rung before wave %g at skill %g", g2, xSkill)); }
        if(w < g3) { VR_AT_Check(VR_AT_HumanoidMaxTier() <= 2, sprintf("no third rung before wave %g at skill %g", g3, xSkill)); }
        if(w < gd) { VR_AT_Check(VR_AT_CountKind(AR_SP_DOG) == 0, sprintf("no dog before wave %g at skill %g", gd, xSkill)); }
        if(w < go) { VR_AT_Check(VR_AT_CountKind(AR_SP_OGRE) == 0, sprintf("no ogre before wave %g at skill %g", go, xSkill)); }
        if(w < gf) { VR_AT_Check(VR_AT_CountKind(AR_SP_FIEND) == 0, sprintf("no fiend before wave %g at skill %g", gf, xSkill)); }
        if(w < gh) { VR_AT_Check(VR_AT_CountKind(AR_SP_HKNIGHT) == 0, sprintf("no death knight before wave %g at skill %g", gh, xSkill)); }
        if(w >= g2 && VR_AT_HumanoidMaxTier() >= 2)
        {
            seen2 = 1;
            if(!first2) { first2 = w; }
        }
        if(w >= g3 && VR_AT_HumanoidMaxTier() >= 3) { seen3 = 1; }
    }
    VR_AT_Check(seen2 && first2 >= g2 && first2 <= g2 + 6, sprintf("the second rung arrives at wave %g (its gate %g) at skill %g", first2, g2, xSkill));
    VR_AT_Check(seen3, sprintf("the third rung arrives too at skill %g", xSkill));
    dprint(sprintf("arena: t5 gates at skill %g: second rung %g, third rung %g, dog %g, ogre %g, fiend %g, death knight %g\n", xSkill, g2, g3, gd, go, gf, gh));
    VR_AT_SetSkill(1);
}

// A boss wave's plan at this skill: how many of its foes are marked big ones (vr_arena_seed 7).
float(float xWave) VR_AT5_PlanBosses =
{
    float n = 0;
    float i;
    VR_ARENA_Plan(xWave);
    for(i = 0; i < ar_plan_n; i++) { if(ar_plan_boss[i]) { n = n + 1; } }
    return n;
}

// xWave at this skill must carry xWant marked big ones, and a plan the cap still holds.
void(float xWave, float xSkill, float xWant, string xWhat) VR_AT5_BossPlan =
{
    VR_AT_SetSkill(xSkill);
    float got = VR_AT5_PlanBosses(xWave);
    float sz  = VR_AT5_PlanSize(xWave);
    VR_AT_Check(got == xWant, sprintf("%s: wave %g at skill %g plans %g bosses (want %g)", xWhat, xWave, xSkill, got, xWant));
    VR_AT_Check(sz >= 1 && sz <= AR_PLAN_MAX, sprintf("%s: its plan of %g foes stays under the cap", xWhat, sz));
    dprint(sprintf("arena: t5 %s: wave %g at skill %g -> %g foes, %g of them bosses\n", xWhat, xWave, xSkill, sz, got));
    VR_AT_SetSkill(1);
}

// A boss wave at this difficulty: its gap, how many marked big ones it brings, and how big they are.
void() VR_AT5_Bosses =
{
    float sk;
    float wantEvery;
    float wantTwice;

    // The gap (vr_arena_boss_every 5): a wave longer at Easy only. The big ones: two at Nightmare, one elsewhere.
    for(sk = 0; sk <= 3; sk++)
    {
        VR_AT_SetSkill(sk);
        wantEvery = sk < 1 ? 6 : 5;
        wantTwice = sk >= 3 ? 1 : 0;
        VR_AT_Check(VR_ARENA_BossEvery() == wantEvery, sprintf("a boss every %g waves at skill %g", wantEvery, sk));
        VR_AT_Check(VR_ARENA_BossTwice() == wantTwice, sprintf("%g big ones on a boss wave at skill %g", 1 + wantTwice, sk));
        dprint(sprintf("arena: t5 boss gap at skill %g -> every %g waves, %g big ones\n", sk, VR_ARENA_BossEvery(), 1 + VR_ARENA_BossTwice()));
    }

    // The cadence itself: at Easy the boss wave moves a wave later, everywhere else it is the knob's own.
    VR_AT_SetSkill(0);
    VR_AT_Check(!VR_ARENA_IsBossWave(5),  "no boss wave on wave 5 at Easy");
    VR_AT_Check(VR_ARENA_IsBossWave(6),   "the boss wave comes on wave 6 at Easy");
    VR_AT_Check(VR_ARENA_IsBossWave(12),  "and on wave 12 (a boss every 6 there)");
    VR_AT_SetSkill(1);
    VR_AT_Check(VR_ARENA_IsBossWave(5) && !VR_ARENA_IsBossWave(6), "at Normal wave 5 as the knob says");
    VR_AT_SetSkill(3);
    VR_AT_Check(VR_ARENA_IsBossWave(5),   "and at Nightmare");

    // vr_arena_skill_impact 0: the gap and the one boss are the knobs' own at every skill. vr_arena_boss_every 0:
    // no boss waves at any skill.
    cvar_set("vr_arena_skill_impact", "0");
    for(sk = 0; sk <= 3; sk++)
    {
        VR_AT_SetSkill(sk);
        VR_AT_Check(VR_ARENA_BossEvery() == 5,  "at impact 0 the gap is vr_arena_boss_every at every skill");
        VR_AT_Check(!VR_ARENA_BossTwice(),      "at impact 0 a boss wave brings the one it always had");
    }
    cvar_set("vr_arena_skill_impact", "1");
    cvar_set("vr_arena_boss_every", "0");
    for(sk = 0; sk <= 3; sk++)
    {
        VR_AT_SetSkill(sk);
        VR_AT_Check(VR_ARENA_BossEvery() == 0 && !VR_ARENA_IsBossWave(5) && !VR_ARENA_IsBossWave(6),
                    "vr_arena_boss_every 0 means no boss waves at any skill");
    }
    cvar_set("vr_arena_boss_every", "5");
    VR_AT_SetSkill(1);

    // Which waves carry them, and how many.
    VR_AT5_BossPlan(5,  0, 0, "Easy holds its boss wave back");
    VR_AT5_BossPlan(6,  0, 1, "Easy's boss wave, a wave later");
    VR_AT5_BossPlan(12, 0, 1, "Easy's next one");
    VR_AT5_BossPlan(5,  1, 1, "Normal's boss wave");
    VR_AT5_BossPlan(10, 1, 1, "and the next, still the one it had");
    VR_AT5_BossPlan(5,  2, 1, "Hard's");
    VR_AT5_BossPlan(5,  3, 2, "Nightmare brings two at once");
    VR_AT5_BossPlan(10, 3, 2, "two on every boss wave there");
    VR_AT5_BossPlan(15, 3, 2, "two even where its boss is a death knight");

    // A Nightmare boss wave's two are the same kind, the second flagged as the half one (ar_plan_boss 2).
    VR_AT_SetSkill(3);
    VR_ARENA_Plan(10);
    VR_AT_Check(ar_plan_boss[0] == 1 && ar_plan_boss[1] == 2, "its second big one is the flagged half one");
    VR_AT_Check(ar_plan_kind[0] == AR_SP_OGRE && ar_plan_kind[1] == AR_SP_OGRE, "the same ogre twice");
    VR_ARENA_Plan(15);
    VR_AT_Check(ar_plan_kind[0] == AR_SP_HKNIGHT && ar_plan_kind[1] == AR_SP_HKNIGHT, "the same death knight twice where its boss is one");
    dprint(sprintf("arena: t5 a Nightmare boss wave plans kind %g (boss %g) and %g (boss %g)\n",
                   ar_plan_kind[0], ar_plan_boss[0], ar_plan_kind[1], ar_plan_boss[1]));
    VR_AT_SetSkill(1);

    // A boss's bulk: its kind's own health (an enforcer's is 80) times vr_arena_boss_health (1.25) times the
    // difficulty's factor, and half that for the second one.
    float want1;
    float want2;
    for(sk = 0; sk <= 3; sk++)
    {
        VR_AT_SetSkill(sk);
        want1 = sk < 1 ? 76 : sk < 2 ? 101 : sk < 3 ? 126 : 151;
        want2 = sk < 1 ? 38 : sk < 2 ? 51 : sk < 3 ? 63 : 76;
        VR_AT_Check(VR_ARENA_BossHealth(80, 1) == want1, sprintf("a boss's bulk at skill %g is %g", sk, want1));
        VR_AT_Check(VR_ARENA_BossHealth(80, 2) == want2, sprintf("and its second half that (%g) at skill %g", want2, sk));
        dprint(sprintf("arena: t5 boss bulk at skill %g -> %g and %g\n", sk, VR_ARENA_BossHealth(80, 1), VR_ARENA_BossHealth(80, 2)));
    }

    cvar_set("vr_arena_skill_impact", "0");
    for(sk = 0; sk <= 3; sk++)
    {
        VR_AT_SetSkill(sk);
        VR_AT_Check(VR_ARENA_BossHealth(80, 1) == 101 && VR_ARENA_BossHealth(80, 2) == 51,
                    "at impact 0 a boss is vr_arena_boss_health's own bulk at every skill");
    }
    cvar_set("vr_arena_skill_impact", "1");

    VR_AT_SetSkill(3);
    cvar_set("vr_arena_boss_health", "2");
    VR_AT_Check(VR_ARENA_BossHealth(80, 1) == 241, "vr_arena_boss_health 2 times Nightmare's 1.5");
    VR_AT_Check(VR_ARENA_BossHealth(80, 2) == 121, "and its second one half that");
    cvar_set("vr_arena_boss_health", "1.25");

    // A factor of 0 (impact 4 at Easy) never leaves a boss with nothing to run on.
    cvar_set("vr_arena_skill_impact", "4");
    VR_AT_SetSkill(0);
    VR_AT_Check(VR_ARENA_BossHealth(80, 1) == 1 && VR_ARENA_BossHealth(80, 2) == 1, "a boss is never left with no bulk");
    cvar_set("vr_arena_skill_impact", "1");
    VR_AT_SetSkill(1);
}

// The armour's gap as this difficulty counts it: the distance between the waves its armour actually lands on.
float() VR_AT5_ArmourGap =
{
    float first = 0;
    float w;
    for(w = 1; w <= 24; w++)
    {
        if(!VR_ARENA_ArmourKind(w)) { continue; }
        if(!first) { first = w; continue; }
        return w - first;
    }
    return 0;
}

// What one boss clear's offer looks like at this difficulty: how many pedestals, and whether any perk is on it twice.
float(float xWave) VR_AT5_OfferDupes =
{
    float dup = 0;
    float i;
    float j;
    VR_ARK_Choose(xWave);
    for(i = 0; i < ark_offer_n; i++)
    {
        for(j = i + 1; j < ark_offer_n; j++) { if(ark_offer_bit[i] == ark_offer_bit[j]) { dup = dup + 1; } }
    }
    return dup;
}

// A wave clear's generosity, a kill's worth and a boss clear's offers, at each difficulty -- the knobs left at
// their defaults (vr_arena_medkits_after 2, vr_arena_armor_every 4, vr_arena_score_kill 100, vr_arena_perk_count 3),
// so every number below is what the difficulty makes of them.
void() VR_AT5_Supplies =
{
    float sk;
    float want;

    // Medkits divided by the factor: an Easy clear drops three, a Nightmare one one. An armour's gap is an
    // interval, so it times the factor: an armour every 3 waves at Easy, every 6 at Nightmare. A kill's score, a
    // wave clear's bonus and a boss clear's offers all times it too (risk pays).
    for(sk = 0; sk <= 3; sk++)
    {
        VR_AT_SetSkill(sk);
        want = sk < 1 ? 3 : sk < 2 ? 2 : sk < 3 ? 2 : 1;
        VR_AT_Check(VR_ARENA_MedkitCount(3) == want, sprintf("a wave clear drops %g medkits at skill %g", want, sk));
        VR_AT_Check(VR_ARENA_MedkitCount(9) == want, sprintf("its medkits do not thin out with the wave (skill %g)", sk));
        want = sk < 1 ? 3 : sk < 2 ? 4 : sk < 3 ? 5 : 6;
        VR_AT_Check(VR_AT5_ArmourGap() == want, sprintf("an armour every %g waves at skill %g", want, sk));
        want = sk < 1 ? 75 : sk < 2 ? 100 : sk < 3 ? 125 : 150;
        VR_AT_Check(VR_ARENA_KillScore(1, 0) == want, sprintf("a first rung kill is worth %g at skill %g", want, sk));
        want = sk < 1 ? 900 : sk < 2 ? 1200 : sk < 3 ? 1500 : 1800;
        VR_AT_Check(VR_ARENA_KillScore(3, 1) == want, sprintf("a third rung boss is worth %g at skill %g", want, sk));
        want = sk < 1 ? 563 : sk < 2 ? 750 : sk < 3 ? 938 : 1125;
        VR_AT_Check(VR_ARENA_WaveBonus(3) == want, sprintf("a wave clear's bonus is %g at skill %g", want, sk));
        want = sk < 1 ? 2 : sk < 2 ? 3 : sk < 3 ? 4 : 5;
        VR_AT_Check(VR_ARK_Opt_Count() == want, sprintf("a boss clear offers %g pedestals at skill %g", want, sk));
        dprint(sprintf("arena: t5 at skill %g: %g medkits, an armour every %g waves, a kill %g, a clear %g, %g pedestals\n",
                       sk, VR_ARENA_MedkitCount(3), VR_AT5_ArmourGap(), VR_ARENA_KillScore(1, 0),
                       VR_ARENA_WaveBonus(3), VR_ARK_Opt_Count()));
    }

    // Where the armour actually lands at the two ends (the heavier kind still from the table's wave 12).
    VR_AT_SetSkill(0);
    VR_AT_Check(VR_ARENA_ArmourKind(3) == 1 && VR_ARENA_ArmourKind(4) == 0, "at Easy its armour comes on wave 3, not 4");
    VR_AT_SetSkill(3);
    VR_AT_Check(VR_ARENA_ArmourKind(4) == 0 && VR_ARENA_ArmourKind(6) == 1, "at Nightmare it waits until wave 6");
    VR_AT_Check(VR_ARENA_ArmourKind(12) != 0 && VR_ARENA_ArmourKind(18) == 2, "and the heavier one still from wave 12 on");
    VR_AT_SetSkill(1);

    // Never past the knobs' own clamps, never below one where the knob asks for any, and 0 means none at any skill.
    cvar_set("vr_arena_medkits_after", "4");
    VR_AT_SetSkill(0);
    VR_AT_Check(VR_ARENA_MedkitCount(3) == AR_KIT_MAX, "an Easy clear never drops more than vr_arena_medkits_after's 4");
    VR_AT_SetSkill(3);
    VR_AT_Check(VR_ARENA_MedkitCount(3) == 3, "a Nightmare clear thins even vr_arena_medkits_after 4 (to 3)");
    cvar_set("vr_arena_medkits_after", "1");
    for(sk = 0; sk <= 3; sk++)
    {
        VR_AT_SetSkill(sk);
        VR_AT_Check(VR_ARENA_MedkitCount(3) >= 1, "a wave clear still gives at least one medkit");
    }
    cvar_set("vr_arena_medkits_after", "0");
    for(sk = 0; sk <= 3; sk++)
    {
        VR_AT_SetSkill(sk);
        VR_AT_Check(VR_ARENA_MedkitCount(3) == 0, "vr_arena_medkits_after 0: no medkits at any skill");
    }
    cvar_set("vr_arena_armor_every", "12");
    VR_AT_SetSkill(3);
    VR_AT_Check(VR_AT5_ArmourGap() == AR_ARMOR_MAX, "a Nightmare gap never stretches past vr_arena_armor_every's 12");
    cvar_set("vr_arena_armor_every", "0");
    for(sk = 0; sk <= 3; sk++)
    {
        VR_AT_SetSkill(sk);
        VR_AT_Check(VR_AT5_ArmourGap() == 0, "vr_arena_armor_every 0: no armour at any skill");
    }

    // vr_arena_score_kill 50 at the two ends, and the perk pool's own cap.
    cvar_set("vr_arena_score_kill", "50");
    VR_AT_SetSkill(0);
    VR_AT_Check(VR_ARENA_KillScore(1, 0) == 38, "vr_arena_score_kill 50 at Easy is 38");
    VR_AT_SetSkill(3);
    VR_AT_Check(VR_ARENA_KillScore(1, 0) == 75, "and at Nightmare 75");
    cvar_set("vr_arena_score_kill", "1");
    VR_AT_SetSkill(0);
    VR_AT_Check(VR_ARENA_KillScore(1, 0) >= 1, "a kill is never worth nothing");
    cvar_set("vr_arena_score_kill", "100");
    cvar_set("vr_arena_perk_count", "5");
    VR_AT_SetSkill(3);
    VR_AT_Check(VR_ARK_Opt_Count() == 7, "a Nightmare boss clear never offers more than the pool's 7");
    VR_AT_Check(VR_AT5_OfferDupes(5) == 0, "and its offer is a different perk on each pedestal");
    VR_AT_SetSkill(0);
    VR_AT_Check(VR_ARK_Opt_Count() == 4, "an Easy one offers four of them");
    VR_AT_Check(VR_AT5_OfferDupes(10) == 0, "and nothing is offered twice there either");
    cvar_set("vr_arena_perk_count", "3");

    // vr_arena_skill_impact 0: the generosity is the knobs' own value at every skill -- the arena as it was.
    VR_AT_SetDefaults(); // (the knobs back to their defaults first: this block reads vr_arena_medkits_after 2 ...)
    cvar_set("vr_arena_skill_impact", "0");
    for(sk = 0; sk <= 3; sk++)
    {
        VR_AT_SetSkill(sk);
        VR_AT_Check(VR_ARENA_MedkitCount(3) == 2,    "at impact 0 a wave clear drops vr_arena_medkits_after's 2");
        VR_AT_Check(VR_AT5_ArmourGap() == 4,         "at impact 0 an armour comes every vr_arena_armor_every's 4");
        VR_AT_Check(VR_ARENA_KillScore(1, 0) == 100, "at impact 0 a kill is vr_arena_score_kill's 100");
        VR_AT_Check(VR_ARENA_WaveBonus(3) == 750,    "at impact 0 a clear is 250 times the wave");
        VR_AT_Check(VR_ARK_Opt_Count() == 3,         "at impact 0 a boss clear offers vr_arena_perk_count's 3");
    }
    VR_AT_SetDefaults(); // (the knobs back to their defaults, the skill back to Normal)
}

// The boss wave live, the way test 3 reaches one: pinned to Nightmare, where a boss wave brings two of them.
void() VR_AT5_BossWave =
{
    if(!ar_mgr)
    {
        VR_AT_Check(FALSE, "test 5's live boss wave needs the arena on (vr_arena 1)");
        VR_AT_End();
        return;
    }
    VR_AT_SetSkill(3);
    VR_AT_Check(VR_ARENA_IsBossWave(5), "wave 5 is a boss wave at Nightmare");
    VR_ARENA_FastForward(5);
    vr_at_step = 1;
    vr_at_until = time + 90;
    dprint("arena: t5 boss wave (Nightmare)\n");
}

void() VR_AT5_Frame =
{
    entity boss1 = world;
    entity boss2 = world;
    entity e;
    float n = 0;
    float hi;
    float lo;

    if(vr_at_step != 1) { return; }
    if(ar_wave < 5) { return; } // (its jump has not landed yet)

    e = findflags(world, ar_slot, 5);
    while(e)
    {
        if(e.health > 0 && e.ar_tier >= AR_BOSS_MARK)
        {
            n = n + 1;
            if(boss1 == world) { boss1 = e; }
            else if(boss2 == world) { boss2 = e; }
        }
        e = findflags(e, ar_slot, 5);
    }
    if(n < 2) { return; } // (its second big one comes a beat after the first)

    VR_AT_Check(n == 2, sprintf("a Nightmare boss wave has its two big ones in the field (%g)", n));
    VR_AT_Check(boss1.vr_ag == 1 && boss2.vr_ag == 1, "both of them armed humanoids");
    VR_AT_Check(boss1.ar_tier == 1 + AR_BOSS_MARK && boss2.ar_tier == 1 + AR_BOSS_MARK, "both worth a boss's tier");
    VR_AT_Check(boss1.classname == boss2.classname, "the same kind of big one");

    // An enforcer's own health is 80: vr_arena_boss_health 1.25 times Nightmare's 1.5, and half that for the second.
    hi = max(boss1.health, boss2.health);
    lo = min(boss1.health, boss2.health);
    VR_AT_Check(hi == 151 && lo == 76, sprintf("their bulk %g and %g (want 151 and 76)", hi, lo));
    dprint(sprintf("arena: t5 live boss wave %g: bulk %g and %g, tiers %g and %g, %s\n",
                   ar_wave, hi, lo, boss1.ar_tier, boss2.ar_tier, boss1.classname));

    vr_at_step = 0;
    VR_AT_End();
}

// What one test family needs is exactness, not tolerance: every step below is a binary-exact number.
void() VR_AT5_Scaling =
{
    float w;

    // Wave 1 (all unarmed) and the waves the plan writes by hand: the counts the factor gives.
    VR_AT_SetSkill(0); float w1easy = VR_AT5_PlanSize(1);
    VR_AT_SetSkill(1); float w1norm = VR_AT5_PlanSize(1);
    VR_AT_SetSkill(3); float w1night = VR_AT5_PlanSize(1);
    VR_AT_Check(w1easy == 1, "wave 1 at Easy is a single unarmed grunt");
    VR_AT_Check(w1norm >= 1 && w1norm <= 2, "wave 1 at Normal is one or two grunts");
    VR_AT_Check(w1night >= w1norm, "wave 1 at Nightmare is not thinner than at Normal");
    dprint(sprintf("arena: t5 wave 1 -> %g %g %g foes (Easy Normal Nightmare)\n", w1easy, w1norm, w1night));

    // Wave 6: three grunts, an enforcer, a dog. Its groups of one are its own floor, so only the grunts move. At
    // Easy this is where its first boss wave lands (a gap of 6, not 5), so it carries its big one on top of them.
    VR_AT5_PlanSizes(6, 5, 5, 5, 6, "wave 6");
    // Wave 11: four grunts, an enforcer, two ogres -- Hard is where a group of four grows.
    VR_AT5_PlanSizes(11, 5, 7, 8, 10, "wave 11");
    // Wave 14 (past the table): four grunts, an enforcer, a classic.
    VR_AT5_PlanSizes(14, 5, 6, 7, 8, "wave 14");
    // Wave 15: a boss wave at Normal, Hard and Nightmare (at Easy its boss waves are 6 and 12) -- its boss or
    // bosses take a place, at every skill.
    VR_AT5_PlanSizes(15, 5, 6, 7, 8, "wave 15 (a boss wave)");

    // The field's cap: vr_arena_max_alive by the factor, whole foes, never past the plan's cap.
    VR_AT5_Caps(8, 6, 8, 10, 12);
    VR_AT5_Caps(12, 9, 12, 12, 12); // (12 at Hard and Nightmare too: the plan's cap holds)

    // No wave plans past the cap, whatever the skill, and a wave never plans nobody.
    for(w = 1; w <= 40; w++)
    {
        VR_AT_SetSkill(3);
        VR_ARENA_Plan(w);
        VR_AT_Check(ar_plan_n >= 1 && ar_plan_n <= AR_PLAN_MAX, sprintf("wave %g at Nightmare plans within the cap", w));
        VR_AT_SetSkill(1);
        VR_ARENA_Plan(w);
        VR_AT_Check(ar_plan_n >= 1 && ar_plan_n <= AR_PLAN_MAX, sprintf("wave %g at Normal plans within the cap", w));
    }

    // A knob set out of range is still clamped to what works before the skill's factor moves it.
    cvar_set("vr_arena_max_alive", "0");
    VR_AT_SetSkill(3);
    VR_AT_Check(VR_ARENA_FieldCap() == 1, "vr_arena_max_alive 0 plays by its clamp (one foe up at once)");
    cvar_set("vr_arena_max_alive", "99");
    VR_AT_Check(VR_ARENA_FieldCap() == AR_PLAN_MAX, "vr_arena_max_alive 99 plays by its clamp (the plan's cap)");
    cvar_set("vr_arena_max_alive", "8");
    VR_AT_SetSkill(1);

    // A factor of 0 (vr_arena_skill_impact 4 at Easy) still leaves a wave somebody to send and the field one foe.
    cvar_set("vr_arena_skill_impact", "4");
    VR_AT_SetSkill(0);
    VR_AT_Check(VR_ARENA_FieldCap() == 1, "a factor of 0 leaves the field's cap at one");
    for(w = 1; w <= 20; w++)
    {
        VR_ARENA_Plan(w);
        VR_AT_Check(ar_plan_n >= 1, sprintf("wave %g still plans a foe at a factor of 0", w));
    }
    cvar_set("vr_arena_skill_impact", "1");
    VR_AT_SetSkill(1);

    // The arena's own beats: vr_arena_wave_delay and the gap its foes come in at, shorter the harder the skill.
    VR_AT_SetSkill(0); VR_AT_Check(VR_ARENA_SkillTime(6, 1) == 8 && VR_ARENA_SkillTime(AR_SPAWN_GAP, AR_TICK) == 2,
                                   "at Easy the arena waits longer between waves and between its foes");
    VR_AT_SetSkill(1); VR_AT_Check(VR_ARENA_SkillTime(6, 1) == 6 && VR_ARENA_SkillTime(AR_SPAWN_GAP, AR_TICK) == 1.5,
                                   "at Normal the beats are the knobs themselves");
    VR_AT_SetSkill(2); VR_AT_Check(VR_ARENA_SkillTime(6, 1) == 4.8 && VR_ARENA_SkillTime(AR_SPAWN_GAP, AR_TICK) == 1.2,
                                   "at Hard they are shorter");
    VR_AT_SetSkill(3); VR_AT_Check(VR_ARENA_SkillTime(6, 1) == 4 && VR_ARENA_SkillTime(AR_SPAWN_GAP, AR_TICK) == 1,
                                   "at Nightmare shortest");
    VR_AT_Check(VR_ARENA_SkillTime(AR_SPAWN_GAP, AR_TICK) >= AR_TICK, "a wave's foes never come in faster than its beat");
    cvar_set("vr_arena_skill_impact", "4");
    VR_AT_SetSkill(0);
    VR_AT_Check(VR_ARENA_SkillTime(6, 1) == 6, "a factor of 0 leaves the beats as the knobs set them");
    cvar_set("vr_arena_skill_impact", "1");
    dprint("arena: t5 beats: wave delay 8 6 4.8 4 and spawn gap 2 1.5 1.2 1 (Easy Normal Hard Nightmare)\n");
    VR_AT_SetSkill(1);
}

void() VR_AT5_Begin =
{
    cvar_set("vr_arena_skill_impact", "1");
    VR_AT5_FactorAt(1, VR_AT5_EASY, VR_AT5_HARD, VR_AT5_NIGHTMARE, "impact 1");   // the table's own steps
    VR_AT5_FactorAt(0, 1, 1, 1, "impact 0");                                      // the skill changes nothing
    VR_AT5_FactorAt(2, 0.5, 1.5, 2, "impact 2");                                  // the deviation from 1 doubled

    // Normal always means the knobs themselves, whatever the impact and whatever the steps.
    cvar_set("vr_arena_skill_impact", "10");
    VR_AT_SetSkill(1);
    VR_AT_Check(VR_ARENA_Skill(0, 5, 9) == 1, "Normal is always as the knobs say");

    // A step of 0 at a soft skill does not carry past 0 (a knob never turns negative).
    VR_AT_SetSkill(0);
    VR_AT_Check(VR_ARENA_Skill(0, 2, 3) == 0, "the factor is never below 0");

    // And the order: Easy softer, Hard and Nightmare harder.
    cvar_set("vr_arena_skill_impact", "1");
    VR_AT_SetSkill(0); float soft = VR_AT5_Factor();
    VR_AT_SetSkill(2); float hard = VR_AT5_Factor();
    VR_AT_SetSkill(3); float worst = VR_AT5_Factor();
    VR_AT_Check(soft < 1 && hard > 1 && worst > hard, "Easy softer, Hard and Nightmare harder");

    VR_AT5_Scaling();  // (what the factor does to a wave: its counts, the field's cap, the arena's beats)

    // The gates, moved by whole waves rather than by the factor: when a rung, a species, and the thickening arrive.
    VR_AT5_Gate(AR_W_TIER2, 13, 10, 8, 6, "the ladder's second rung");
    VR_AT5_Gate(AR_W_TIER3, 21, 18, 16, 14, "the ladder's third rung");
    VR_AT5_Gate(AR_W_KNIGHT, 7, 4, 2, 2, "the knights");
    VR_AT5_Gate(AR_W_DOG, 9, 6, 4, 2, "the dogs");
    VR_AT5_Gate(AR_W_OGRE, 11, 8, 6, 4, "the ogres");
    VR_AT5_Gate(AR_W_FIEND, 15, 12, 10, 8, "the fiends");
    VR_AT5_Gate(AR_W_HKNIGHT, 19, 16, 14, 12, "the death knights");
    VR_AT5_Gate(4, 7, 4, 2, 2, "the unarmed window of the early waves");

    // vr_arena_skill_impact moves the gates too: 0 leaves every one of them where the table has it, 2 doubles the
    // step, 4 at its most extreme holds the bestiary's own floor (wave 2).
    cvar_set("vr_arena_skill_impact", "0");
    VR_AT5_Gate(AR_W_FIEND, 12, 12, 12, 12, "the fiends at impact 0");
    cvar_set("vr_arena_skill_impact", "2");
    VR_AT5_Gate(AR_W_HKNIGHT, 22, 16, 12, 8, "the death knights at impact 2");
    cvar_set("vr_arena_skill_impact", "4");
    VR_AT5_Gate(AR_W_HKNIGHT, 28, 16, 8, 2, "the death knights at impact 4 (wave 2 is the floor)");
    cvar_set("vr_arena_skill_impact", "1");

    VR_AT5_Early();      // (the first waves' unarmed share and its window)
    VR_AT5_Bestiary();   // (which species, and which boss, from which wave)
    VR_AT5_ObeyGates(1); // (the live plans obey the gates at Normal ...)
    VR_AT5_ObeyGates(3); // (... and at Nightmare, where everything arrives early)
    VR_AT5_Bosses();     // (a boss wave's gap, how many big ones it brings, and how big they are)
    VR_AT5_Supplies();   // (a wave clear's medkits and armour, a kill's and a clear's score, a boss clear's offers)

    // Last, so nothing re-plans under it: a boss wave live, the way test 3 fast-forwards to one.
    VR_AT5_BossWave();
}

// ---------------------------------------------------------------------------
// Test 6: the long run (soak). The live director driven through AR_T6_WAVES waves, every foe felled by the test's
// own blow once its kind's start is through, 1 s between waves; the edicts counted as it goes. It checks that no
// wave stalled, that every wave was planned and cleared in turn, and that what stays in the map grows no faster than
// a wave clear's own unpicked drops (the player never walks over them here).
// ---------------------------------------------------------------------------

float AR_T6_WAVES = 32;      // waves the soak runs through
float AR_T6_LEAK  = 1;       // edicts a wave may leave behind on top of its own drops, on average
float vr_at6_delay;          // vr_arena_wave_delay as the run had it
float vr_at6_wave;           // the wave the soak last saw
float vr_at6_e0;             // the edicts at its first counted wave, and its drops then
float vr_at6_d0;
float vr_at6_w0;
float vr_at6_peak;           // the most edicts seen
float vr_at6_long;           // the longest a wave took (s)

float() VR_AT6_Edicts =
{
    float n = 0;
    entity e = nextent(world);
    while(e) { n = n + 1; e = nextent(e); }
    return n;
}

float(string xClass) VR_AT6_CountClass =
{
    float n = 0;
    entity e = find(world, classname, xClass);
    while(e) { n = n + 1; e = find(e, classname, xClass); }
    return n;
}

float() VR_AT6_Drops =
{
    float n = 0;
    entity e = nextent(world);
    while(e)
    {
        if(e.ar_drop > 0 && e.model != "") { n = n + 1; } // (a drop still lying there: not picked up)
        e = nextent(e);
    }
    return n;
}

float() VR_AT6_Bodies =
{
    float n = 0;
    entity e = nextent(world);
    while(e)
    {
        if(e.ar_slot < 0) { n = n + 1; }
        e = nextent(e);
    }
    return n;
}

void() VR_AT6_Begin =
{
    vr_at6_delay = cvar("vr_arena_wave_delay");
    cvar_set("vr_arena_wave_delay", "1");
    vr_at6_wave = ar_wave;
    vr_at6_e0 = 0;
    vr_at6_peak = 0;
    vr_at6_long = 0;
    vr_at_until = time + 400;
    dprint(sprintf("arena: test 6 soak to wave %g\n", AR_T6_WAVES));
}

void() VR_AT6_Finish =
{
    float e1 = VR_AT6_Edicts();
    float d1 = VR_AT6_Drops();
    float waves = ar_wave - vr_at6_w0;
    float grow = (e1 - vr_at6_e0) - (d1 - vr_at6_d0);
    dprint(sprintf("arena: t6 waves %g edicts %g -> %g (peak %g) drops %g -> %g\n", waves, vr_at6_e0, e1, vr_at6_peak,
                   vr_at6_d0, d1));
    dprint(sprintf("arena: t6 bodies %g guns %g longest wave %g s\n", VR_AT6_Bodies(), VR_AT6_CountClass("thrown_weapon"),
                   vr_at6_long));
    VR_AT_Check(ar_wave >= AR_T6_WAVES, "the soak reached its last wave");
    VR_AT_Check(vr_at6_long < AR_STALL, "no wave stalled");
    VR_AT_Check(grow <= AR_T6_LEAK * waves, "the map grows no faster than a wave clear's drops");
    cvar_set("vr_arena_wave_delay", ftos(vr_at6_delay));
    VR_AT_End();
}

void() VR_AT6_Frame =
{
    if(!ar_mgr) { VR_AT_Check(FALSE, "test 6 needs the arena (vrarena, or vr_arena 1)"); VR_AT6_Finish(); return; }

    if(ar_wave != vr_at6_wave) // (a new wave: count what the last one left)
    {
        vr_at6_wave = ar_wave;
        vr_at_until = time + 400;
        float n = VR_AT6_Edicts();
        if(n > vr_at6_peak) { vr_at6_peak = n; }
        if(ar_wave == 2) { vr_at6_e0 = n; vr_at6_d0 = VR_AT6_Drops(); vr_at6_w0 = ar_wave; }
        if(VR_ARENA_Mod(ar_wave, 5) == 0)
        {
            dprint(sprintf("arena: t6 wave %g edicts %g drops %g bodies %g guns %g score %g\n", ar_wave, n,
                           VR_AT6_Drops(), VR_AT6_Bodies(), VR_AT6_CountClass("thrown_weapon"), ar_score));
        }
        if(ar_wave >= AR_T6_WAVES) { VR_AT6_Finish(); return; }
    }

    if(ar_wave >= 1 && time - ar_wave_at > vr_at6_long) { vr_at6_long = time - ar_wave_at; }
    VR_AT_KillOne(ar_wave);
}

// ---------------------------------------------------------------------------
// world.qc's test beat (StartFrame, under QVR_TESTS)
// ---------------------------------------------------------------------------

void() VR_ARENA_Test_Frame =
{
    float c = cvar("vr_arena_test");

    if(c == 0)
    {
        if(vr_at_case) { VR_AT_Restore(); VR_AT_SetSkill(vr_at_skill0); vr_at_case = 0; dprint("arena: test stopped\n"); }
        return;
    }

    if(vr_at_case == 0)
    {
        vr_at_case = c;
        vr_at_pass = 0;
        vr_at_fail = 0;
        vr_at_step = 0;
        vr_at_until = time + 30;
        VR_AT_Save();
        vr_at_skill0 = cvar("skill");
        VR_AT_SetDefaults();
        dprint(sprintf("arena: test %g begin\n", c));

        if(c == 1)
        {
            VR_AT_Plans();
            VR_AT_HookCheck();
            VR_AT_End();
            return;
        }
        if(c == 2) { VR_AT2_Begin(); return; }
        if(c == 3) { VR_AT3_Begin(); return; }
        if(c == 4) { VR_AT4_Begin(); return; }
        if(c == 5) { VR_AT5_Begin(); return; }
        if(c == 6) { VR_AT6_Begin(); return; }

        VR_AT_Check(FALSE, "no such arena test");
        VR_AT_End();
        return;
    }

    if(vr_at_case != c) { return; } // (a different case asked for while one runs: the running one finishes)

    if(time > vr_at_until)
    {
        VR_AT_Check(FALSE, "the test timed out");
        VR_AT_End();
        return;
    }

    if(vr_at_case == 2) { VR_AT2_Frame(); return; }
    if(vr_at_case == 3) { VR_AT3_Frame(); return; }
    if(vr_at_case == 4) { VR_AT4_Frame(); return; }
    if(vr_at_case == 5) { VR_AT5_Frame(); return; }
    if(vr_at_case == 6) { VR_AT6_Frame(); return; }
}
#endif
