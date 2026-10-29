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
// - 6 (vrarena): the long run, 32 waves: no wave stalls, the edicts stay bounded, the cleanup's caps hold.
// - 7 (vrarena): the cleanup: bodies fade by time and past their cap, never one held, pulled or dying; a wave clear's
//   drops go two clears on; the foes' guns capped, a player's own never counted, Spoils doubling the cap.
// - 8 (e1m1, no arena): off the arena every body stays.
// - 9 (vrarena): the wave flow (alive pool, burst, gaps).
// - 10 (vrtesthall with vr_arena 1, or vrarena): the foes' armour: its table at each skill (first wave, chance, best
//   type, plates), the roll over many spawns per (wave, skill) against the table, the bosses' and the guard's, no
//   armour before the first wave or with it off, the plan's guns untouched by it; then live: wave 5's foes wear what
//   was planned and the skill's own roll is held (vr_agarmor_skill1 set to certain), the armoured boss's kill pays
//   more, and a grunt spawned outside the director still gets the skill's roll.
// - 11 (vrtesthall with vr_arena 1): the loot the waves leave behind: with the player owning none of them, the waves'
//   tier 2 and 3 guns are eligible and a foe's launcher and an ammo box's gun fall; the pack's own rules and
//   vr_enemy_drops never hold; out of a run the gate is ownership again; and wave 20's armed humanoids hold their
//   plan's strong guns in their hands with the player owning none of them. (vrarena's name alone keeps its run on, so
//   its off-run half needs a map where the cvar is the only thing saying "arena".)
// - 12 (vrtesthall with vr_arena 1, or vrarena): the weapon schedule over 24 seeded runs of 30 waves at each skill:
//   each gun first in hands at its table wave (moved by the difficulty), never before, its trickle waves carrying
//   exactly 1 then 2 of it, the bosses keeping their own picks, the mission packs' guns only with their data, the
//   pool's weights (a newcomer rarer at first, an old gun fading but never gone).
// - 13 (vrarena): no overlaps: a thing on a place takes it, a drop's place with a thing on it is stepped round; then
//   waves 8, 12, 16 live with the player standing on a spawn place and medkits lying on every item point, every foe
//   and drop scanned as it comes in (none may overlap anything) and every medkit finding room.
// - 14 (vrarena, or vr_arena 1): the rest and the countdown before a wave, timed: 5 s + 5 s at Normal into a boss
//   wave (its boss named), then Hard (4 s rest, the same 5 s countdown) into a plain wave.

// ---------------------------------------------------------------------------
// The test's own state
// ---------------------------------------------------------------------------

float vr_at_case;        // 0 idle; 1 .. 14 running
float vr_at_pass;
float vr_at_fail;
float vr_at_step;
float vr_at_until;
float vr_at_keep[27];    // the cvars the tests set, put back at the end
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
    if(i == 8) { return "vr_arena_corpse_fade"; }
    if(i == 9) { return "vr_arena_corpse_max"; }
    if(i == 10) { return "vr_arena_item_waves"; }
    if(i == 11) { return "vr_arena_gun_max"; }
    if(i == 12) { return "vr_arena_wave_size"; }
    if(i == 13) { return "vr_arena_alive_max"; }
    if(i == 14) { return "vr_arena_alive_grow"; }
    if(i == 15) { return "vr_arena_burst"; }
    if(i == 16) { return "vr_arena_spawn_gap"; }
    if(i == 17) { return "vr_arena_foe_armor"; }
    if(i == 18) { return "vr_arena_foe_armor_scale"; }
    if(i == 19) { return "vr_arena_foe_armor_from"; }
    if(i == 20) { return "vr_agarmor_skill1"; } // (test 10 sets the skill's own roll to certain, to see it held)
    if(i == 21) { return "vr_arena"; }         // (test 11 looks at a run and at the same map with the run off)
    if(i == 22) { return "vr_enemy_drops"; }   // (test 11: what a foe may leave behind, and its "never")
    if(i == 23) { return "vr_ammobox_drops"; } // (... the same for what an ammo box may carry)
    if(i == 24) { return "vr_arena_wave_rest"; }
    if(i == 25) { return "vr_arena_countdown"; }
    if(i == 26) { return "vr_arena_perks"; }     // (off: a boss clear's cards would hold the director for a pick)
    return "";
}
float VR_AT_NCVARS = 27;

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
    cvar_set("vr_arena_corpse_fade", "14");
    cvar_set("vr_arena_corpse_max", "12");
    cvar_set("vr_arena_item_waves", "2");
    cvar_set("vr_arena_gun_max", "24");
    cvar_set("vr_arena_wave_size", "1"); // (the ladder's own counts: tests 1 to 5 check them; 6 and 9 the default 1.75)
    cvar_set("vr_arena_alive_max", "6");
    cvar_set("vr_arena_alive_grow", "4");
    cvar_set("vr_arena_burst", "3");
    cvar_set("vr_arena_spawn_gap", "1.5");
    cvar_set("vr_arena_foe_armor", "1");
    cvar_set("vr_arena_foe_armor_scale", "1");
    cvar_set("vr_arena_foe_armor_from", "4");
    cvar_set("vr_arena_wave_rest", "5");
    cvar_set("vr_arena_countdown", "5");
    cvar_set("vr_arena_perks", "0"); // (the perk cards hold the arena until picked: vr_arena_perks_test drives them)
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

// How many of the plan's (non-boss) humanoids hold a gun.
float(float xWid) VR_AT_GunCount =
{
    float n = 0;
    float j;
    for(j = 0; j < ar_plan_n; j++)
    {
        if(VR_AT_IsHumanoid(ar_plan_kind[j]) && !ar_plan_boss[j] && ar_plan_gun[j] == xWid) { n = n + 1; }
    }
    return n;
}

// How many of the plan's (non-boss) humanoids hold a gun the weapon schedule has not made eligible by wave xWave
// (vr_arena.qc's table: a row's VR_ARENA_GunEligible; the nail pistol at wave 2 is that wave's own plan).
float(float xWave) VR_AT_OffSchedule =
{
    float bad = 0;
    float j;
    float i;
    for(j = 0; j < ar_plan_n; j++)
    {
        if(!VR_AT_IsHumanoid(ar_plan_kind[j]) || ar_plan_boss[j] || ar_plan_gun[j] == AR_FIST) { continue; }
        if(xWave == 2 && ar_plan_gun[j] == WID_NAILPISTOL) { continue; }
        float ok = FALSE;
        for(i = 0; i < AR_GUN_N; i++)
        {
            if(ar_plan_gun[j] == VR_ARENA_GunWid(i) && xWave >= VR_ARENA_GunEligible(i)) { ok = TRUE; }
        }
        if(!ok) { bad = bad + 1; }
    }
    return bad;
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
    // (the weapon schedule: the super shotgun's trickle starts here, on one of them; the rest the bottom rungs)
    VR_AT_Check(VR_AT_GunCount(WID_SUPER_SHOTGUN) == 1 && VR_AT_HumanoidMaxTier() <= 2 && VR_AT_OffSchedule(3) == 0,
                "wave 3 brings one super shotgun, the rest the bottom rungs");

    VR_ARENA_Plan(4);
    VR_AT_Check(VR_AT_CountKind(AR_SP_KNIGHT) == 1, "wave 4 brings a knight");
    VR_AT_Check(VR_AT_HumanoidCount() == 4, "wave 4 grunt count");
    VR_AT_Check(VR_AT_GunCount(WID_SUPER_SHOTGUN) == 1 && VR_AT_GunCount(WID_NAILGUN) == 0 && VR_AT_OffSchedule(4) == 0,
                "wave 4 brings one super shotgun again, no nailgun yet");
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
    VR_AT_Check(VR_AT_GunCount(WID_SUPER_SHOTGUN) == 2 && VR_AT_GunCount(WID_NAILGUN) == 1 && VR_AT_HumanoidMaxTier() <= 2,
                "wave 5: two super shotguns and the first nailgun, nothing past the second rung");

    VR_ARENA_Plan(10);
    VR_AT_Check(VR_AT_BossCount() == 1 && ar_plan_kind[0] == AR_SP_OGRE, "wave 10's boss is an ogre");
    VR_AT_Check(VR_AT_GunCount(WID_GRENADE_LAUNCHER) == 1 && VR_AT_GunCount(WID_ROCKET_LAUNCHER) == 0,
                "wave 10 brings the first grenade launcher, no rockets yet");

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

        VR_AT_Check(VR_AT_OffSchedule(w) == 0, sprintf("wave %g: no gun before the schedule's wave for it", w));
        if(w < AR_GUN_FIRST) { VR_AT_Check(VR_AT_HumanoidMaxTier() <= 1, "nothing past the bottom rungs before wave 3"); }
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

        entity boss = world;
        entity e = findflags(world, ar_slot, 5);
        while(e)
        {
            if(e.health > 0 && e.ar_tier >= AR_BOSS_MARK) { boss = e; }
            e = findflags(e, ar_slot, 5);
        }
        if(!boss) { return; } // (it comes in halfway through its wave, after the opening burst and its warning)

        VR_AT_Check(ar_wave == 5, "the arena jumped to its boss wave");
        float ib = 0;
        while(ib < ar_plan_n && !ar_plan_boss[ib]) { ib = ib + 1; }
        VR_AT_Check(ib < ar_plan_n && ar_plan_kind[ib] == AR_SP_ENFORCER, "wave 5's plan has its boss, an enforcer");
        VR_AT_Check(ib >= min(VR_ARENA_Opt_Burst(), ar_plan_n - 1), "wave 5's boss comes in after the opening burst");
        VR_AT_Check(ar_plan_live > ib, "the boss came in after the foes planned ahead of it");
        VR_AT_Check(VR_AT_BossCount() == 1, "a boss wave has one boss");

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
        VR_AT_Check(got == want && got >= 1 && got <= AR_FIELD_MAX,
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
        // (the weapon schedule's gates move with the difficulty: its rows' VR_ARENA_GunEligible; test 12 the rest)
        VR_AT_Check(VR_AT_OffSchedule(w) == 0, sprintf("wave %g: no gun before the schedule's wave for it at skill %g", w, xSkill));
        g2 = VR_ARENA_GunEligible(3); // (the second rung's first gun, the super shotgun)
        g3 = VR_ARENA_GunEligible(6); // (the third rung's first gun, the grenade launcher)
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

// What one boss clear's offer looks like at this difficulty: how many cards, and whether any perk is on it twice.
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
        VR_AT_Check(VR_ARK_Opt_Count() == want, sprintf("a boss clear offers %g cards at skill %g", want, sk));
        dprint(sprintf("arena: t5 at skill %g: %g medkits, an armour every %g waves, a kill %g, a clear %g, %g cards\n",
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
    VR_AT_Check(VR_ARK_Opt_Count() == ARK_POOL_N, "a Nightmare boss clear never offers more than the pool's size");
    VR_AT_Check(VR_AT5_OfferDupes(5) == 0, "and its offer is a different perk on each card");
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
    VR_AT5_Caps(12, 9, 12, 15, 16); // (16 at Nightmare: AR_FIELD_MAX holds)

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
    VR_AT_Check(VR_ARENA_FieldCap() == AR_FIELD_MAX, "vr_arena_max_alive 99 plays by its clamp (AR_FIELD_MAX)");
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

    // The arena's own beats: vr_arena_wave_rest and the gap its foes come in at, shorter the harder the skill.
    VR_AT_SetSkill(0); VR_AT_Check(VR_ARENA_SkillTime(6, 1) == 8 && VR_ARENA_SkillTime(VR_ARENA_Opt_Gap(), AR_TICK) == 2,
                                   "at Easy the arena waits longer between waves and between its foes");
    VR_AT_SetSkill(1); VR_AT_Check(VR_ARENA_SkillTime(6, 1) == 6 && VR_ARENA_SkillTime(VR_ARENA_Opt_Gap(), AR_TICK) == 1.5,
                                   "at Normal the beats are the knobs themselves");
    VR_AT_SetSkill(2); VR_AT_Check(VR_ARENA_SkillTime(6, 1) == 4.8 && VR_ARENA_SkillTime(VR_ARENA_Opt_Gap(), AR_TICK) == 1.2,
                                   "at Hard they are shorter");
    VR_AT_SetSkill(3); VR_AT_Check(VR_ARENA_SkillTime(6, 1) == 4 && VR_ARENA_SkillTime(VR_ARENA_Opt_Gap(), AR_TICK) == 1,
                                   "at Nightmare shortest");
    VR_AT_Check(VR_ARENA_SkillTime(VR_ARENA_Opt_Gap(), AR_TICK) >= AR_TICK, "a wave's foes never come in faster than its beat");
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
// The cleanup's counts (tests 6 and 7)
// ---------------------------------------------------------------------------

// Bodies lying on (not fading yet).
float() VR_AT_LyingBodies =
{
    float n = 0;
    entity e = findflags(world, ar_expiring, 1);
    while(e) { n = n + VR_ARENA_BodyLies(e); e = findflags(e, ar_expiring, 1); }
    return n;
}

// The foes' guns lying about that the arena's cap counts.
float() VR_AT_FoeGuns =
{
    float n = 0;
    entity e = find(world, classname, "thrown_weapon");
    while(e) { n = n + VR_EnemyWeapons_CountedIn(e, TRUE); e = find(e, classname, "thrown_weapon"); }
    return n;
}

// Whether `xEnt` is still in use (nextent passes over freed edicts).
float(entity xEnt) VR_AT_Live =
{
    if(xEnt == world) { return FALSE; }
    entity e = nextent(world);
    while(e) { if(e == xEnt) { return TRUE; } e = nextent(e); }
    return FALSE;
}

// ---------------------------------------------------------------------------
// Test 6: the long run (soak). The live director driven through AR_T6_WAVES waves, every foe felled by the test's
// own blow once its kind's start is through, 1 s between waves; the edicts counted as it goes. It checks that no
// wave stalled, that every wave was planned and cleared in turn, and that once the map has settled (AR_T6_FROM: the
// things that go by themselves -- gibs, limbs, backpacks -- at their steady count) what stays grows no faster than a
// wave clear's own unpicked drops (the player never walks over them here), and the cleanup's caps hold.
// ---------------------------------------------------------------------------

float AR_T6_WAVES = 32;      // waves the soak runs through
float AR_T6_FROM  = 16;      // the wave its edicts are counted from (the map settled)
float AR_T6_LEAK  = 1;       // edicts a wave may leave behind on top of its own drops, on average
float AR_T6_DROPS = 3;       // a wave clear's drops at most (two medkits, an armour, at Normal)
float vr_at6_delay;          // vr_arena_wave_rest as the run had it
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

// The broken-gun scan (ROUND23.md, "Arena: a glitched shotgun"): every frame of the soak, each weapon lying about or
// held by a foe that is drawn but can't be what it looks like: a gun lying (thrown_weapon) with no rigid body once it
// has had a frame to get one (stuck as it was left), or not to be taken (no hand touch, no force grab) without fading,
// or a fade stalled (still there a good while after its fade began); a foe's held weapon (vr_ag_weapon) whose holder no
// longer holds it (left where it was drawn last). Each is counted once (.vr_at_gunbad), its first few printed.
.float vr_at_gunbad;         // 1: counted by the scan; 2..: frames seen without a body (a frame's grace)
float vr_at6_gunbad;         // broken guns the soak saw
float vr_at6_gunscans;       // weapons scanned (summed over frames)

void(entity e, string xWhy) VR_AT_GunBad =
{
    if(e.vr_at_gunbad == 1) { return; }
    e.vr_at_gunbad = 1;
    vr_at6_gunbad = vr_at6_gunbad + 1;
    if(vr_at6_gunbad <= 8)
    {
        dprint(sprintf("arena: t6 broken gun #%g: %s %s at %v: %s;", vr_at6_gunbad, e.classname, e.netname, e.origin,
                       xWhy));
        dprint(sprintf(" owner %s (health %g), alpha %g, fade %g, flags %g, solid %g, movetype %g;", e.owner.classname,
                       e.owner.health, e.alpha, e.vr_drop_fade, e.flags, e.solid, e.movetype));
        dprint(sprintf(" nextthink in %g, born %g, now %g\n", e.nextthink - time, e.vr_drop_born, time));
    }
};

void() VR_AT_GunScan =
{
    entity e;
    for(e = find(world, classname, "thrown_weapon"); e; e = find(e, classname, "thrown_weapon"))
    {
        if(e.model == "") { continue; }
        vr_at6_gunscans = vr_at6_gunscans + 1;
        if(e.vr_drop_fade)
        {
            if(time > e.vr_drop_fade + VR_DROPCAP_FADE + 0.5) { VR_AT_GunBad(e, "its fade stalled"); }
            continue;
        }
        // (VR_Forcegrab_IsEligible: a thrown_weapon not SOLID_NOT; the hand's: its own handtouch)
        if(e.carry_player == world && e.fg_state == 0 && (e.solid == SOLID_NOT || e.handtouch != wpnthrow_handtouch))
        {
            VR_AT_GunBad(e, "not to be taken (no hand touch or force grab), not fading");
        }
        if(e.vr_rigid && e.carry_player == world && !physicsdamp(e, '0 0 0', 1, 1, 0, '0 0 0'))
        {
            // (a frame's grace: its body is made at the next physics step)
            if(e.vr_at_gunbad == 0) { e.vr_at_gunbad = 2; }
            else if(e.vr_at_gunbad >= 2) { e.vr_at_gunbad = e.vr_at_gunbad + 1; }
            if(e.vr_at_gunbad >= 4) { VR_AT_GunBad(e, "no rigid body"); }
        }
        else if(e.vr_at_gunbad >= 2)
        {
            e.vr_at_gunbad = 0;
        }
    }
    for(e = find(world, classname, "vr_ag_weapon"); e; e = find(e, classname, "vr_ag_weapon"))
    {
        vr_at6_gunscans = vr_at6_gunscans + 1;
        entity g = e.owner;
        if(e.model != "" && (g == world || (g.vr_ag_prop != e && g.vr_ag_prop2 != e)))
        {
            VR_AT_GunBad(e, "a foe's held weapon its holder no longer holds");
        }
    }
};

// The soak's blows, varied so the guns go every way they go in play: a gib (as before), a plain death (its body lying,
// the corpse cleanup's fade on it), and a disarm first (its gun knocked out of its hand; it is spared AR_T6_SPARE s to
// go for a gun lying, the cap's fades and its takes meeting), felled after.
float AR_T6_SPARE = 3;
.float vr_at6_spare;         // a disarmed foe spared until then
float vr_at6_blows;

float(float xWave) VR_AT6_KillOne =
{
    entity p = VR_ARENA_FindPlayer();
    if(p == world) { return 0; }
    entity e = findflags(world, ar_slot, xWave);
    while(e)
    {
        entity next = findflags(e, ar_slot, xWave);
        if(e.health > 0 && e.takedamage && e.ar_slot == xWave && time >= e.vr_at6_spare)
        {
            vr_at6_blows = vr_at6_blows + 1;
            float mode = VR_ARENA_Mod(vr_at6_blows, 3);
            if(mode == 2 && e.vr_ag_prop && !e.vr_at6_spare)
            {
                makevectors(e.angles);
                VR_AG_Disarm(e, v_right * crandom() - v_forward, 150, "the soak's blow");
                e.vr_at6_spare = time + AR_T6_SPARE;
                return 1;
            }
            T_Damage(e, p, p, e.health + (mode == 0 ? 100 : 1));
            return 1;
        }
        e = next;
    }
    return 0;
}

void() VR_AT6_Begin =
{
    vr_at6_gunbad = 0;
    vr_at6_blows = 0;
    vr_at6_gunscans = 0;
    vr_at6_delay = cvar("vr_arena_wave_rest");
    cvar_set("vr_arena_wave_rest", "1");
    cvar_set("vr_arena_countdown", "0"); // (the soak: no countdown either; VR_AT_End puts it back)
    cvar_set("vr_arena_wave_size", "1.75"); // (the default's bigger waves: the soak is bounded with them)
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
    // The cleanup's caps (vr_arena.qc): the bodies lying (not fading), the foes' guns, a wave clear's drops.
    VR_AT_Check(VR_AT_LyingBodies() <= cvar("vr_arena_corpse_max"), "no more bodies lie than vr_arena_corpse_max");
    VR_AT_Check(VR_AT_FoeGuns() <= cvar("vr_arena_gun_max"), "no more of the foes' guns lie than vr_arena_gun_max");
    VR_AT_Check(d1 <= AR_T6_DROPS * (cvar("vr_arena_item_waves") + 1), "a wave clear's drops go vr_arena_item_waves clears on");
    dprint(sprintf("arena: t6 gun scan: %g weapons scanned, %g broken, %g fades mended\n", vr_at6_gunscans, vr_at6_gunbad,
                   vr_dropcap_mended));
    // The fades' safety net (VR_EnemyWeapons_MendFades): a fade stopped midway (its think replaced) on again; one long
    // overdue gone.
    entity pl = VR_ARENA_FindPlayer();
    entity w = CreateThrownWeapon(pl, WID_SHOTGUN, 0, world, pl.origin + '64 0 16', '0 0 0', '0 0 0', '0 0 0', FALSE, FALSE, 0);
    VR_EnemyWeapons_Fade(w);
    w.think = SUB_Null;
    VR_EnemyWeapons_MendFades();
    float on = w.think == VR_EnemyWeapons_FadeThink;
    w.think = SUB_Null;
    w.vr_drop_fade = time - 5;
    VR_EnemyWeapons_MendFades();
    VR_AT_Check(on && !VR_AT_Live(w), "a stopped gun fade is taken up again, a long overdue one removed");
    VR_AT_Check(vr_at6_gunscans > 0 && vr_at6_gunbad == 0, "no broken guns (no body, not to be taken, a stalled fade, an orphaned held one)");
    cvar_set("vr_arena_wave_rest", ftos(vr_at6_delay));
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
        if(ar_wave == AR_T6_FROM) { vr_at6_e0 = n; vr_at6_d0 = VR_AT6_Drops(); vr_at6_w0 = ar_wave; }
        if(VR_ARENA_Mod(ar_wave, 4) == 0)
        {
            dprint(sprintf("arena: t6 wave %g edicts %g drops %g bodies %g guns %g score %g\n", ar_wave, n,
                           VR_AT6_Drops(), VR_AT6_Bodies(), VR_AT6_CountClass("thrown_weapon"), ar_score));
        }
        if(ar_wave >= AR_T6_WAVES) { VR_AT6_Finish(); return; }
    }

    if(ar_wave >= 1 && time - ar_wave_at > vr_at6_long) { vr_at6_long = time - ar_wave_at; }
    VR_AT_GunScan();
    VR_AT6_KillOne(ar_wave);
}

// ---------------------------------------------------------------------------
// Test 7: the cleanup (vr_arena.qc, "The cleanup"; vr_enemyguns.qc's cap in the arena). Stand-in bodies and drops
// handed to the cleanup's own hooks: past vr_arena_corpse_max the oldest body not held fades; held (a hand on it,
// re-taken every beat as a held ragdoll's limb would be, or a force grab's pull) or still dying, a body outlives its
// time and goes once let go of; a wave clear's drops go vr_arena_item_waves clears on, one picked up at once, one
// pulled not; the foes' guns are capped at vr_arena_gun_max (one pulled spared, a player's own never counted) and
// the Spoils perk doubles that.
// ---------------------------------------------------------------------------

entity vr_at7_b0; entity vr_at7_b1; entity vr_at7_b2; entity vr_at7_b3; entity vr_at7_b4; entity vr_at7_b5;
entity vr_at7_d1; entity vr_at7_d2; entity vr_at7_d4;
entity vr_at7_held0;         // the player's main hand as it was
float vr_at7_let;            // when the held ones were let go of

entity() VR_AT7_Body =
{
    entity b = spawn();
    b.classname = "vr_at7_body";
    b.health = -10;
    b.ar_slot = -1;
    VR_ARENA_BodyDown(b);
    return b;
}

entity(float xWave, string xModel) VR_AT7_Drop =
{
    entity d = spawn();
    d.classname = "vr_at7_drop";
    d.ar_drop = 1;
    d.ar_drop_wave = xWave;
    d.model = xModel;
    return d;
}

entity(entity xBy) VR_AT7_Gun =
{
    entity g = CreateThrownWeapon(xBy, WID_SHOTGUN, 0, WeaponInst_New(WeaponIdToBaseClipSize(WID_SHOTGUN)),
        xBy.origin + '0 0 24', '0 0 0', '0 0 0', '0 0 0', FALSE, FALSE, 0);
    return g;
}

// A test gun put away (one fading goes by itself).
void(entity g) VR_AT7_PutAway =
{
    if(g != world && !g.vr_drop_fade) { VR_Carry_Forget(g); remove(g); }
}

// The guns' half, all in one frame (nothing falls meanwhile: none of them is in flight).
void(entity p) VR_AT7_Guns =
{
    float g0 = VR_AT_FoeGuns();
    float taken0 = ar_mgr.ark_taken;
    ar_mgr.ark_taken = 0;
    cvar_set("vr_arena_gun_max", ftos(g0 + 2));

    entity m = spawn();
    m.classname = "vr_at7_foe";
    m.flags = FL_MONSTER;
    setorigin(m, p.origin);

    entity g1 = VR_AT7_Gun(m);
    entity g2 = VR_AT7_Gun(m);
    VR_AT_Check(g1 != world && g2 != world, "guns: a foe's drop made");
    if(g1 == world || g2 == world) { remove(m); ar_mgr.ark_taken = taken0; return; }
    VR_AT_Check(VR_EnemyWeapons_CountedIn(g1, TRUE), "guns: a foe's drop is counted");
    g1.fg_state = 1; // (pulled by a force grab: spared)
    entity g3 = VR_AT7_Gun(m);
    VR_AT_Check(VR_AT_FoeGuns() == g0 + 2, "guns: past vr_arena_gun_max the oldest fades");
    VR_AT_Check(!g1.vr_drop_fade && g2.vr_drop_fade, "guns: one pulled is spared, the next oldest fades");

    entity gp = VR_AT7_Gun(p);
    VR_AT_Check(!VR_EnemyWeapons_CountedIn(gp, TRUE), "guns: one the player dropped is never counted");
    VR_AT_Check(VR_AT_FoeGuns() == g0 + 2 && !g3.vr_drop_fade, "guns: and takes nothing's place");

    ar_mgr.ark_taken = ARK_SPOIL;
    entity g4 = VR_AT7_Gun(m);
    entity g5 = VR_AT7_Gun(m);
    VR_AT_Check(VR_AT_FoeGuns() == g0 + 4 && !g3.vr_drop_fade, "guns: Spoils keeps twice vr_arena_gun_max");
    ar_mgr.ark_taken = 0;
    entity g6 = VR_AT7_Gun(m);
    VR_AT_Check(VR_AT_FoeGuns() == g0 + 2 && !g1.vr_drop_fade, "guns: Spoils gone, the cap is vr_arena_gun_max again");

    g1.fg_state = 0;
    VR_AT7_PutAway(g1); VR_AT7_PutAway(g2); VR_AT7_PutAway(g3); VR_AT7_PutAway(g4); VR_AT7_PutAway(g5); VR_AT7_PutAway(g6);
    VR_AT7_PutAway(gp);
    remove(m);
    ar_mgr.ark_taken = taken0;
}

void() VR_AT7_Begin =
{
    if(!ar_mgr) { VR_AT_Check(FALSE, "test 7 needs the arena (vrarena, or vr_arena 1)"); VR_AT_End(); return; }
    entity p = VR_ARENA_FindPlayer();
    if(p == world) { VR_AT_Check(FALSE, "test 7 needs a player"); VR_AT_End(); return; }

    VR_AT7_Guns(p);

    float n0 = VR_AT_LyingBodies();
    cvar_set("vr_arena_corpse_fade", "2");
    cvar_set("vr_arena_corpse_max", ftos(n0 + 3));
    vr_at7_held0 = p.mainhand_held;
    vr_at7_b0 = VR_AT7_Body();
    p.mainhand_held = vr_at7_b0; // (a hand on it)
    vr_at7_b1 = VR_AT7_Body();
    vr_at7_b2 = VR_AT7_Body();
    vr_at7_b3 = VR_AT7_Body();
    VR_AT_Check(VR_AT_LyingBodies() == n0 + 3, "bodies: past vr_arena_corpse_max the oldest fades");
    VR_AT_Check(VR_ARENA_BodyLies(vr_at7_b0) && !VR_ARENA_BodyLies(vr_at7_b1),
                "bodies: one held is kept, the next oldest fades");
    VR_AT_Check(vr_at7_b2.ar_expire >= time + 1.5 - 0.01 && vr_at7_b2.ar_expire <= time + 2.5 + 0.01,
                "bodies: one's time is vr_arena_corpse_fade give or take a quarter");
    cvar_set("vr_arena_corpse_max", "0");
    vr_at7_b4 = VR_AT7_Body();
    vr_at7_b4.vr_corpse = 1; // (still dying)
    vr_at7_b5 = VR_AT7_Body();
    vr_at7_b5.fg_state = 1;  // (pulled by a force grab)

    cvar_set("vr_arena_item_waves", "2");
    vr_at7_d1 = VR_AT7_Drop(1, "progs/w_medkit.mdl");
    vr_at7_d2 = VR_AT7_Drop(2, "progs/w_medkit.mdl");
    entity d3 = VR_AT7_Drop(2, "");
    vr_at7_d4 = VR_AT7_Drop(1, "progs/w_medkit.mdl");
    vr_at7_d4.fg_state = 1;
    VR_ARENA_DropsExpire(3);
    VR_AT_Check(!VR_AT_Live(d3), "drops: one picked up goes at the next clear");
    VR_AT_Check(vr_at7_d1.ar_expiring && !vr_at7_d2.ar_expiring, "drops: two clears old they fade, one clear old not");

    vr_at_step = 1;
    vr_at_until = time + 20;
    dprint("arena: test 7 cleanup\n");
}

void() VR_AT7_Frame =
{
    entity p = VR_ARENA_FindPlayer();
    if(vr_at_step == 1)
    {
        if(p) { p.mainhand_held = vr_at7_b0; } // (held: taken again every beat, as a held ragdoll's limb is)
        VR_ARENA_Leftovers();
        if(VR_AT_Live(vr_at7_b1) || VR_AT_Live(vr_at7_b2) || VR_AT_Live(vr_at7_b3) || VR_AT_Live(vr_at7_d1)) { return; }
        VR_AT_Check(VR_AT_Live(vr_at7_b0) && vr_at7_b0.alpha == 0, "bodies: held, it outlives its time, seen whole");
        VR_AT_Check(VR_AT_Live(vr_at7_b5), "bodies: pulled by a force grab, it outlives its time");
        VR_AT_Check(VR_AT_Live(vr_at7_b4), "bodies: still dying, it outlives its time");
        VR_AT_Check(VR_AT_Live(vr_at7_d4), "drops: pulled by a force grab, it outlives its time");
        VR_AT_Check(VR_AT_Live(vr_at7_d2), "drops: one clear old, it lies on");
        if(p) { p.mainhand_held = vr_at7_held0; }
        vr_at7_b5.fg_state = 0;
        vr_at7_d4.fg_state = 0;
        vr_at7_b4.vr_corpse = 2;
        vr_at7_let = time;
        vr_at_step = 2;
        return;
    }
    if(VR_AT_Live(vr_at7_b0) || VR_AT_Live(vr_at7_b5) || VR_AT_Live(vr_at7_b4) || VR_AT_Live(vr_at7_d4)) { return; }
    VR_AT_Check(time - vr_at7_let <= AR_HELD_GRACE + 0.1, "let go of, they fade out and go");
    remove(vr_at7_d2);
    VR_AT_End();
}

// ---------------------------------------------------------------------------
// Test 8 (a map with no arena: e1m1): the cleanup is the arena's alone. Every monster felled whole (a blow just past
// its health: no gibbing), then AR_T8_WAIT of game time, longer than any arena body lies (vr_arena_corpse_fade's
// most): every body is still there.
// ---------------------------------------------------------------------------

float AR_T8_WAIT = 20; // s: past vr_arena_corpse_fade (14) and its jitter (3)
float vr_at8_n0;
float vr_at8_t0;

float() VR_AT8_Bodies =
{
    float n = 0;
    entity e = nextent(world);
    while(e)
    {
        if(substring(e.classname, 0, 8) == "monster_" && e.health <= 0) { n = n + 1; }
        e = nextent(e);
    }
    return n;
}

void() VR_AT8_Begin =
{
    if(ar_mgr) { VR_AT_Check(FALSE, "test 8 needs a map with no arena (e1m1)"); VR_AT_End(); return; }
    entity p = find(world, classname, "player");
    if(p == world) { VR_AT_Check(FALSE, "test 8 needs a player"); VR_AT_End(); return; }
    float n = 0;
    entity e = findflags(world, flags, FL_MONSTER);
    while(e)
    {
        entity next = findflags(e, flags, FL_MONSTER);
        if(e.health > 0 && e.takedamage) { T_Damage(e, p, p, e.health + 1); n = n + 1; }
        e = next;
    }
    VR_AT_Check(n > 0, "the map's monsters felled");
    vr_at8_n0 = -1;
    vr_at8_t0 = time;
    vr_at_until = time + AR_T8_WAIT + 10;
    dprint(sprintf("arena: test 8 felled %g monsters\n", n));
}

void() VR_AT8_Frame =
{
    if(vr_at8_n0 < 0) { vr_at8_n0 = VR_AT8_Bodies(); return; } // (a frame on: their deaths through)
    if(time < vr_at8_t0 + AR_T8_WAIT) { return; }
    float n1 = VR_AT8_Bodies();
    float marked = 0;
    entity e = nextent(world);
    while(e)
    {
        if(e.ar_expiring || e.ar_fell || e.ar_expire) { marked = marked + 1; }
        e = nextent(e);
    }
    // (The count of bodies is the map's own business: its other systems -- the ragdolls' cap, a corpse's gibbing --
    // may take some. What this checks is that the arena's cleanup never touched one.)
    dprint(sprintf("arena: t8 bodies %g -> %g after %g s, marked by the arena's cleanup %g\n", vr_at8_n0, n1,
                   time - vr_at8_t0, marked));
    VR_AT_Check(vr_at8_n0 > 0, "the felled monsters left bodies");
    VR_AT_Check(marked == 0, "off the arena the cleanup marks nothing");
    VR_AT_End();
}

// ---------------------------------------------------------------------------
// world.qc's test beat (StartFrame, under QVR_TESTS)
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Test 9: the wave flow (vrarena) -- the alive pool, the opening burst, the gaps, a wave's total, its end
// ---------------------------------------------------------------------------
// The live director at the default wave size (1.75) through waves 1, 5 (a boss wave), 10 and 20 at Normal. The test
// fells one foe every AR_T9_KILL s (slower than the trickle, so the pool fills up). Every wave has to end with its
// whole plan put in the field and felled, never more of it up at once than its pool, and every trickle gap within
// vr_arena_spawn_gap's range (at Normal 1.5 to 3 s). It prints the waves' table (total, pool) first.

float AR_T9_KILL  = 8;    // s between the test's kills (slower than the trickle: the pool fills)
float AR_T9_EPS   = 0.05; // s a gap may run past its range (a frame or so)
float AR_T9_WAVES = 4;    // waves watched (VR_AT9_Wave)
float vr_at9_target;      // the wave being watched
float vr_at9_peak;        // the most of it up at once, as the test counted it
float vr_at9_kill;        // the time of the test's next kill
float vr_at9_n;           // waves checked so far

float(float i) VR_AT9_Wave =
{
    if(i == 0) { return 1; }
    if(i == 1) { return 5; }
    if(i == 2) { return 10; }
    return 20;
}

// The next wave watched: the arena jumps to it (VR_ARENA_FastForward clears the field and plans it afresh).
void() VR_AT9_Next =
{
    vr_at9_target = VR_AT9_Wave(vr_at9_n);
    VR_ARENA_FastForward(vr_at9_target);
    vr_at9_peak = 0;
    vr_at9_kill = time + AR_T9_KILL;
    vr_at_until = time + 200;
}

void() VR_AT9_Begin =
{
    if(!ar_mgr) { VR_AT_Check(FALSE, "test 9 needs the arena (vrarena, or vr_arena 1)"); VR_AT_End(); return; }

    // The table at Normal, off the plans alone: the default size's waves are bigger than the ladder's own counts.
    float i;
    for(i = 0; i < AR_T9_WAVES; i++)
    {
        float w = VR_AT9_Wave(i);
        cvar_set("vr_arena_wave_size", "1");
        VR_ARENA_Plan(w);
        float n1 = ar_plan_n;
        cvar_set("vr_arena_wave_size", "1.75");
        VR_ARENA_Plan(w);
        dprint(sprintf("arena: t9 table wave %g total %g (size 1: %g) pool %g\n", w, ar_plan_n, n1, VR_ARENA_Pool(w)));
        VR_AT_Check(ar_plan_n > n1, sprintf("wave %g is bigger at wave size 1.75 than at 1", w));
    }

    // The pool: 6 at wave 1, one more every 4 waves, never past the field's cap (8 at Normal); the difficulty's share.
    VR_AT_Check(VR_ARENA_Pool(1) == 6 && VR_ARENA_Pool(5) == 7 && VR_ARENA_Pool(9) == 8 && VR_ARENA_Pool(20) == 8,
                "the pool grows 6, 7, 8 and stops at the field's cap");
    cvar_set("vr_arena_alive_grow", "0");
    VR_AT_Check(VR_ARENA_Pool(20) == 6, "vr_arena_alive_grow 0: the pool never grows");
    cvar_set("vr_arena_alive_grow", "4");
    VR_AT_SetSkill(0);
    VR_AT_Check(VR_ARENA_Pool(1) == 4, "the pool at Easy is the difficulty's share of it (6 is 4)");
    VR_AT_SetSkill(3);
    VR_AT_Check(VR_ARENA_Pool(1) == 9, "the pool at Nightmare is the difficulty's share of it (6 is 9)");
    VR_ARENA_Plan(40);
    dprint(sprintf("arena: t9 wave 40 at Nightmare plans %g\n", ar_plan_n));
    VR_AT_Check(ar_plan_n < AR_PLAN_MAX, "a Nightmare wave 40 is planned whole, under the plan's cap");
    VR_AT_SetSkill(1);

    vr_at9_n = 0;
    VR_AT9_Next();
    dprint("arena: test 9 the wave flow\n");
}

void() VR_AT9_Frame =
{
    if(ar_wave != vr_at9_target) { return; } // (its jump: the wave has not begun yet)

    float up = VR_ARENA_CountAlive();
    if(up > vr_at9_peak) { vr_at9_peak = up; }

    if(VR_ARENA_Between()) // (the wave is over: its clear ran, the next one waits for its rest)
    {
        float w = ar_wave;
        float lo = VR_ARENA_SkillTime(VR_ARENA_Opt_Gap(), AR_TICK);
        dprint(sprintf("arena: t9 wave %g total %g spawned %g felled %g pool %g peak %g gaps %g\n", w, ar_plan_n,
                       ar_plan_live, ar_wave_kills, ar_pool, ar_wave_peak, ar_gap_n));
        dprint(sprintf("arena: t9 wave %g gap range %g..%g s (allowed %g..%g), took %g s\n", w, ar_gap_lo, ar_gap_hi,
                       lo, 2 * lo, time - ar_wave_at));
        VR_AT_Check(ar_plan_left == 0 && ar_plan_live == ar_plan_n, sprintf("wave %g put its whole plan in the field", w));
        VR_AT_Check(ar_wave_kills == ar_plan_n, sprintf("wave %g ended with every one of its foes felled", w));
        VR_AT_Check(ar_wave_peak <= ar_pool && vr_at9_peak <= ar_pool, sprintf("wave %g never had more up than its pool", w));
        VR_AT_Check(ar_wave_peak >= min(ar_pool, ar_plan_n) - 1, sprintf("wave %g filled its pool (or nearly)", w));
        VR_AT_Check(time - ar_wave_at < AR_STALL, sprintf("wave %g ended, not stalled", w));
        if(ar_plan_n > VR_ARENA_Opt_Burst())
        {
            VR_AT_Check(ar_gap_n >= 1, sprintf("wave %g trickled in after its burst", w));
        }
        if(ar_gap_n > 0)
        {
            VR_AT_Check(ar_gap_lo >= lo - AR_T9_EPS && ar_gap_hi <= 2 * lo + AR_T9_EPS,
                        sprintf("wave %g's gaps are within vr_arena_spawn_gap's range", w));
        }

        vr_at9_n = vr_at9_n + 1;
        if(vr_at9_n >= AR_T9_WAVES) { VR_AT_End(); return; }
        VR_AT9_Next();
        return;
    }

    if(time >= vr_at9_kill && up > 0)
    {
        VR_AT_KillOne(ar_wave);
        vr_at9_kill = time + AR_T9_KILL;
    }
}

// ---------------------------------------------------------------------------
// Test 10: the foes' armour (vr_arena.qc's VR_ARENA_Armour*; ROUND23.md's table)
// ---------------------------------------------------------------------------

float AR_T10_N   = 400;  // rolls per (wave, skill) cell
float AR_T10_TOL = 0.03; // the share armoured within this of the table's chance (a low-discrepancy roll: 0.01 off at most here)
float vr_at10_seq;       // vr_agarmor.qc's skill sequence when the live wave was asked for
float vr_at10_score;

float(float xMask) VR_AT10_Plates =
{
    float n = 0;
    float i;
    for(i = 0; i < 10; i++) { if(xMask & VR_AgArmor_Bit(i)) { n = n + 1; } }
    return n;
}

// One (wave, skill) cell: AR_T10_N rolls off the armour's own stream, against the table.
void(float xWave, float xBoss, float xBossWave) VR_AT10_Cell =
{
    float c = VR_ARENA_ArmourChance(xWave);
    float best = VR_ARENA_ArmourBest(xWave);
    float pcs = VR_ARENA_ArmourPieces(xWave);
    float want = c;
    float wtype = 0;
    float wpcs = pcs;
    if(xBoss) { want = c > 0 ? 1 : 0; wtype = min(3, best + 1); wpcs = min(10, pcs + AR_ARM_BOSS_PCS); }
    else if(xBossWave && c > 0) { want = VR_ARENA_Clamp(c + AR_ARM_GUARD, 0, AR_ARM_CAP); }

    float n = 0;
    float okType = TRUE;
    float okMask = TRUE;
    float seenBest = FALSE;
    float k;
    VR_ARENA_ArmourSeed(xWave);
    for(k = 0; k < AR_T10_N; k++)
    {
        VR_ARENA_ArmourRoll(xWave, xBoss, xBossWave);
        if(ar_arm_type <= 0) { if(ar_arm_mask) { okMask = FALSE; } continue; }
        n = n + 1;
        if(xBoss && ar_arm_type != wtype) { okType = FALSE; }
        if(!xBoss && (ar_arm_type < 1 || ar_arm_type > best)) { okType = FALSE; }
        if(ar_arm_type == best) { seenBest = TRUE; }
        if((ar_arm_mask & 3) != 3 || VR_AT10_Plates(ar_arm_mask) != wpcs) { okMask = FALSE; }
    }
    float share = n / AR_T10_N;
    string where = sprintf("skill %g wave %g%s%s", VR_ARENA_SkillLevel(), xWave, xBoss ? " boss" : "", xBossWave ? " (boss wave)" : "");
    dprint(sprintf("arena: t10 %s: chance %g got %g, best %g, plates %g\n", where, want, share, xBoss ? wtype : best, wpcs));
    VR_AT_Check(fabs(share - want) <= AR_T10_TOL, sprintf("%s: %g armoured, the table says %g", where, share, want));
    VR_AT_Check(okType, sprintf("%s: every type within the table's (best %g)", where, best));
    VR_AT_Check(okMask, sprintf("%s: every armour the cuirass, the helmet and %g plates in all", where, wpcs));
    if(n > 0 && !xBoss && best > 1) { VR_AT_Check(seenBest, sprintf("%s: the best type turns up", where)); }
}

void() VR_AT10_Tables =
{
    float sk;
    float w;
    float i;
    float bare;

    // The table at each skill (impact 1): the first wave, the best type's gates, the plates, the chance.
    VR_AT_SetSkill(0);
    VR_AT_Check(VR_ARENA_ArmourFrom() == 7, "Easy: armour from wave 7");
    VR_AT_Check(VR_ARENA_ArmourBest(14) == 1 && VR_ARENA_ArmourBest(15) == 2 && VR_ARENA_ArmourBest(25) == 3, "Easy: yellow from 15, red from 25");
    VR_AT_SetSkill(1);
    VR_AT_Check(VR_ARENA_ArmourFrom() == 4, "Normal: armour from wave 4");
    VR_AT_Check(VR_ARENA_ArmourChance(3) == 0 && fabs(VR_ARENA_ArmourChance(4) - 0.10) < 0.001 &&
                fabs(VR_ARENA_ArmourChance(12) - 0.34) < 0.001 && fabs(VR_ARENA_ArmourChance(20) - 0.58) < 0.001 &&
                VR_ARENA_ArmourChance(40) == AR_ARM_CAP, "Normal: 0 at 3, 10% at 4, 34% at 12, 58% at 20, the cap by 40");
    VR_AT_Check(VR_ARENA_ArmourBest(11) == 1 && VR_ARENA_ArmourBest(12) == 2 && VR_ARENA_ArmourBest(21) == 2 && VR_ARENA_ArmourBest(22) == 3, "Normal: yellow from 12, red from 22");
    VR_AT_Check(VR_ARENA_ArmourPieces(4) == 2 && VR_ARENA_ArmourPieces(7) == 3 && VR_ARENA_ArmourPieces(10) == 4 &&
                VR_ARENA_ArmourPieces(28) == 10 && VR_ARENA_ArmourPieces(40) == 10, "Normal: plates 2 at 4, 3 at 7, 4 at 10, all ten from 28");
    VR_AT_SetSkill(2);
    VR_AT_Check(VR_ARENA_ArmourFrom() == 2 && fabs(VR_ARENA_ArmourChance(2) - 0.125) < 0.001, "Hard: from wave 2 at 12.5%");
    VR_AT_Check(VR_ARENA_ArmourBest(10) == 2 && VR_ARENA_ArmourBest(20) == 3 && VR_ARENA_ArmourBest(9) == 1, "Hard: yellow from 10, red from 20");
    VR_AT_SetSkill(3);
    VR_AT_Check(VR_ARENA_ArmourFrom() == 2 && fabs(VR_ARENA_ArmourChance(2) - 0.15) < 0.001, "Nightmare: from wave 2 at 15%");
    VR_AT_Check(VR_ARENA_ArmourBest(8) == 2 && VR_ARENA_ArmourBest(18) == 3 && VR_ARENA_ArmourBest(7) == 1, "Nightmare: yellow from 8, red from 18");
    VR_AT_Check(VR_ARENA_ArmourPieces(4) == 3 && VR_ARENA_ArmourPieces(18) == 10, "Nightmare: 3 plates at 4, all ten at 18");

    // The rolls over many spawns, per (wave, skill), against the table; the bosses and a boss wave's guard.
    for(sk = 0; sk <= 3; sk++)
    {
        VR_AT_SetSkill(sk);
        float from = VR_ARENA_ArmourFrom();
        VR_AT10_Cell(from - 1, FALSE, FALSE); // (none before the first wave)
        VR_AT10_Cell(from, FALSE, FALSE);
        VR_AT10_Cell(12, FALSE, FALSE);
        VR_AT10_Cell(24, FALSE, FALSE);
        VR_AT10_Cell(36, FALSE, FALSE);
        VR_AT10_Cell(from - 1, TRUE, TRUE);  // (a boss before the first wave: bare)
        VR_AT10_Cell(max(from, 5), TRUE, TRUE);
        VR_AT10_Cell(24, TRUE, TRUE);
        VR_AT10_Cell(15, FALSE, TRUE);
    }

    // The plans: no armour before the first wave (at each skill), only on grunts and enforcers, a humanoid boss
    // always armoured a type above its wave's, and the plan's guns the same with the armour on or off.
    for(sk = 0; sk <= 3; sk++)
    {
        VR_AT_SetSkill(sk);
        bare = TRUE;
        float onlyHum = TRUE;
        float bossOk = TRUE;
        float bosses = 0;
        for(w = 1; w <= 30; w++)
        {
            VR_ARENA_Plan(w);
            for(i = 0; i < ar_plan_n; i++)
            {
                if(ar_plan_armour[i] > 0 && w < VR_ARENA_ArmourFrom()) { bare = FALSE; }
                if(ar_plan_armour[i] > 0 && !VR_AT_IsHumanoid(ar_plan_kind[i])) { onlyHum = FALSE; }
                if(ar_plan_boss[i] > 0 && VR_AT_IsHumanoid(ar_plan_kind[i]) && w >= VR_ARENA_ArmourFrom())
                {
                    bosses = bosses + 1;
                    if(ar_plan_armour[i] != min(3, VR_ARENA_ArmourBest(w) + 1)) { bossOk = FALSE; }
                }
            }
        }
        VR_AT_Check(bare, sprintf("skill %g: no plan armours a foe before wave %g", sk, VR_ARENA_ArmourFrom()));
        VR_AT_Check(onlyHum, sprintf("skill %g: only grunts and enforcers are armoured", sk));
        VR_AT_Check(bossOk, sprintf("skill %g: each of its %g humanoid bosses a type above its wave's", sk, bosses));
    }
    VR_AT_SetSkill(1);
    VR_ARENA_Plan(5);
    float boss5 = -1;
    for(i = 0; i < ar_plan_n; i++) { if(ar_plan_boss[i] > 0) { boss5 = i; } }
    VR_AT_Check(boss5 >= 0 && ar_plan_armour[boss5] == 2 && VR_AT10_Plates(ar_plan_amask[boss5]) == 6,
                "Normal: wave 5's enforcer boss wears yellow, 6 plates");

    cvar_set("vr_arena_foe_armor", "0");
    VR_ARENA_Plan(20);
    float gunsOff = VR_AT_GunSum();
    bare = TRUE;
    for(i = 0; i < ar_plan_n; i++) { if(ar_plan_armour[i] > 0) { bare = FALSE; } }
    VR_AT_Check(bare && VR_ARENA_ArmourChance(30) == 0, "vr_arena_foe_armor 0: nobody armoured");
    cvar_set("vr_arena_foe_armor", "1");
    VR_ARENA_Plan(20);
    VR_AT_Check(VR_AT_GunSum() == gunsOff, "the armour never moves the plan's guns (its own stream)");
    cvar_set("vr_arena_foe_armor_scale", "0");
    VR_AT_Check(VR_ARENA_ArmourChance(30) == 0, "an armour chance of 0: nobody armoured");
    cvar_set("vr_arena_foe_armor_scale", "2");
    VR_AT_Check(fabs(VR_ARENA_ArmourChance(4) - 0.2) < 0.001, "an armour chance of 2: 20% at its first wave");
    cvar_set("vr_arena_foe_armor_scale", "1");
    cvar_set("vr_arena_foe_armor_from", "8");
    VR_AT_Check(VR_ARENA_ArmourChance(7) == 0 && VR_ARENA_ArmourChance(8) > 0, "armour from wave 8: none at 7");
    cvar_set("vr_arena_foe_armor_from", "4");

    // The score: an armoured kill a quarter more per type.
    VR_AT_Check(VR_ARENA_ArmourWorth(100, 1) == 125 && VR_ARENA_ArmourWorth(100, 2) == 150 &&
                VR_ARENA_ArmourWorth(400, 3) == 700 && VR_ARENA_ArmourWorth(100, 0) == 100, "an armoured kill: 1.25 / 1.5 / 1.75");
}

void() VR_AT10_Begin =
{
    if(!ar_mgr) { VR_AT_Check(FALSE, "test 10 needs the arena (vrarena, or vr_arena 1)"); VR_AT_End(); return; }
    VR_AT10_Tables();

    // Live: wave 5 at Normal, with the skill's own roll set to certain -- the arena's roll must replace it.
    VR_AT_SetSkill(1);
    cvar_set("vr_agarmor_skill1", "1");
    vr_at10_seq = vr_ar_seq;
    VR_ARENA_FastForward(5);
    vr_at_until = time + 60;
    dprint("arena: test 10 live wave 5\n");
}

void() VR_AT10_Frame =
{
    entity e;
    entity boss = world;
    float i;
    if(vr_at_step == 0)
    {
        if(ar_wave < 5 || ar_plan_left > 0) { return; } // (the whole wave in, its boss too)
        float planned = 0;
        float worn = 0;
        float match = TRUE;
        for(i = 0; i < ar_plan_n; i++) { if(ar_plan_armour[i] > 0) { planned = planned + 1; } }
        e = findflags(world, ar_slot, ar_wave);
        while(e)
        {
            if(e.ar_slot == ar_wave && e.health > 0)
            {
                if(e.vr_ar_type > 0) { worn = worn + 1; }
                if(e.vr_ar_type != e.ar_armour) { match = FALSE; }
                if(e.ar_tier >= AR_BOSS_MARK) { boss = e; }
            }
            e = findflags(e, ar_slot, ar_wave);
        }
        dprint(sprintf("arena: t10 live wave 5: %g of %g planned armoured, %g wearing it, skill seq moved %g\n",
                       planned, ar_plan_n, worn, vr_ar_seq - vr_at10_seq));
        VR_AT_Check(worn == planned, sprintf("wave 5's foes wear the plan's armour (%g, %g planned)", worn, planned));
        VR_AT_Check(match, "every foe's armour is the one the arena gave it");
        VR_AT_Check(vr_ar_seq == vr_at10_seq, "the skill's own roll was held for every wave foe (no double roll)");
        VR_AT_Check(worn < ar_plan_n, "with the skill's roll certain, the wave still has bare foes (the arena's roll rules)");
        VR_AT_Check(boss != world && boss.vr_ar_type == 2 && VR_AT10_Plates(boss.vr_ar_mask) == 6, "the boss enforcer in yellow, 6 plates");
        if(boss != world)
        {
            entity p = VR_ARENA_FindPlayer();
            vr_at10_score = ar_score;
            T_Damage(boss, p, p, boss.health + 1000);
            float want = VR_ARENA_ArmourWorth(VR_ARENA_KillScore(VR_ARENA_TierOf(boss.ar_tier), 1), 2);
            VR_AT_Check(ar_score - vr_at10_score == want, sprintf("the armoured boss's kill pays %g (got %g)", want, ar_score - vr_at10_score));
        }

        // Outside the director: a grunt spawned the ordinary way still gets the skill's roll (certain here).
        entity keep = self;
        float seq = vr_ar_seq;
        entity g = spawn();
        self = g;
        g.classname = "monster_army";
        setorigin(g, VR_ARENA_FindPlayer().origin + '0 0 4096'); // (out of the way; it is removed at once)
        monster_vr_grunt();
        self = keep;
        VR_AT_Check(vr_ar_seq == seq + 1 && g.vr_ar_type > 0 && g.ar_armour == 0, "a grunt outside the waves rolls the skill's armour as before");
        dprint(sprintf("arena: t10 outside grunt: type %g pieces %g\n", g.vr_ar_type, g.vr_ar_mask));
        remove(g);
        VR_AT_End();
    }
}

// ---------------------------------------------------------------------------
// Test 11: the loot the waves leave behind (a foe's gun is not gated on what the player carries)
// ---------------------------------------------------------------------------
//
// VR_WeaponUtil_EntIsEligibleForWeapon (vr_weaponutil.qc) asks whether a player owns a weapon -- the axe and the
// shotgun always count. That gates what a dying monster may drop (TryEnemyDrop), what an ammo box may carry
// (TryAmmoBoxDrop), what a crate fills with (vr_crates.qc) and, through VR_AGR_Owned (vr_armedrepl.qc), which tiers a
// replacement gun may come from. A Survival Arena run is the exception this test is about: an arena map has no
// authored weapon pickups, and a run begins with the player stripped to the fists (VR_ARENA_StripPlayers) -- with
// ownership as the gate, every gun the waves carry would stay out of the player's reach for the whole run, because
// nothing on such a map can ever make them own one. In a run the rule is the pack's instead: any gun this build has
// is eligible (a soldier's launcher this pack has not, MG3's super axe without MG3's data, are out either way), and
// the drop settings' "never" is not overridden. The waves' own guns were never gated on the player -- the ladder picks
// them (VR_ARENA_PickGun) and an armed humanoid holds what its plan gave it (VR_AM_Spawn, VR_AG_Usable) -- so the live
// half watches wave 20 hold that, which is the same wave's foes and the same guns the first half drops.

float VR_AT11_N = 6; // the guns a wave's tier 2 and 3 humanoids carry: none of them owned by the player

float(float i) VR_AT11_Wid =
{
    if(i == 0) { return WID_SUPER_SHOTGUN; }
    if(i == 1) { return WID_NAILGUN; }
    if(i == 2) { return WID_SUPER_NAILGUN; }
    if(i == 3) { return WID_GRENADE_LAUNCHER; }
    if(i == 4) { return WID_ROCKET_LAUNCHER; }
    return WID_LIGHTNING;
}

string(float wid) VR_AT11_Name =
{
    if(wid == WID_SUPER_SHOTGUN)    { return "the super shotgun"; }
    if(wid == WID_NAILGUN)          { return "the nail gun"; }
    if(wid == WID_SUPER_NAILGUN)    { return "the super nail gun"; }
    if(wid == WID_GRENADE_LAUNCHER) { return "the grenade launcher"; }
    if(wid == WID_ROCKET_LAUNCHER)  { return "the rocket launcher"; }
    return "the lightning gun";
}

// How many of `wid` lie about -- a drop is a thrown_weapon prop (CreateWeaponDrop, weapons.qc) -- ...
float(float wid) VR_AT11_Lying =
{
    float n = 0;
    for(entity e = find(world, classname, "thrown_weapon"); e; e = find(e, classname, "thrown_weapon"))
    {
        if(e.weapon == wid) { n = n + 1; }
    }
    return n;
}

// ... and the test's own taken back again (the next find is held before the removal: a removed entity is not a
// handle to search from).
void(float wid) VR_AT11_Clear =
{
    entity e = find(world, classname, "thrown_weapon");
    while(e)
    {
        entity nx = find(e, classname, "thrown_weapon");
        if(e.weapon == wid) { remove(e); }
        e = nx;
    }
}

// A stand-in for the ogre whose death the drop helpers are asked about: they read only its origin, and it is put out
// of the way (4096 over the player) and removed in the same frame, so nothing lies about when the live wave arrives.
entity(vector at) VR_AT11_Foe =
{
    entity f = spawn();
    f.classname = "vr_at11_foe";
    setorigin(f, at);
    return f;
}

void(entity p) VR_AT11_Eligible =
{
    float i;
    for(i = 0; i < VR_AT11_N; i++)
    {
        float wid = VR_AT11_Wid(i);
        VR_WeaponUtil_EntDelWeapon(p, wid);
        VR_AT_Check(!VR_WeaponUtil_EntHasWeapon(p, wid),
                    sprintf("the run leaves the player with no %s (its map has no pickup that could change that)", VR_AT11_Name(wid)));
    }
    for(i = 0; i < VR_AT11_N; i++)
    {
        float wid = VR_AT11_Wid(i);
        VR_AT_Check(VR_WeaponUtil_EntIsEligibleForWeapon(p, wid),
                    sprintf("in the run %s is eligible with the player holding none of it", VR_AT11_Name(wid)));
    }
    // The pack still rules what a run may hold out: a gun this build does not have stays out of its waves' loot too.
    VR_AT_Check(VR_WeaponUtil_EntIsEligibleForWeapon(p, WID_ADROCKET) == VR_Pack_WeaponAvailable(WID_ADROCKET),
                "in the run a soldier's launcher this pack has not is out of its loot as ever");
    VR_AT_Check(VR_WeaponUtil_EntIsEligibleForWeapon(p, WID_SUPERAXE) == VR_Pack_WeaponAvailable(WID_SUPERAXE),
                "in the run MG3's super axe follows the pack, not the run");
}

void(entity p) VR_AT11_Drops =
{
    entity foe = VR_AT11_Foe(p.origin + '0 0 4096');
    float n;

    n = VR_AT11_Lying(WID_ROCKET_LAUNCHER);
    VR_AT_Check(TryEnemyDrop(foe, p, WID_ROCKET_LAUNCHER, 1) && VR_AT11_Lying(WID_ROCKET_LAUNCHER) == n + 1,
                "in the run a foe's rocket launcher falls: the ogres the waves send from their wave on carry them");
    VR_AT11_Clear(WID_ROCKET_LAUNCHER);

    n = VR_AT11_Lying(WID_LIGHTNING);
    VR_AT_Check(TryAmmoBoxDrop(foe, p, WID_LIGHTNING, 1) && VR_AT11_Lying(WID_LIGHTNING) == n + 1,
                "in the run an ammo box's lightning gun falls");
    VR_AT11_Clear(WID_LIGHTNING);

    // The settings are not overridden: "never" is never, in a run as out of one.
    cvar_set("vr_enemy_drops", ftos(QVR_ENEMYDROPS_DISABLE));
    n = VR_AT11_Lying(WID_ROCKET_LAUNCHER);
    VR_AT_Check(!TryEnemyDrop(foe, p, WID_ROCKET_LAUNCHER, 1) && VR_AT11_Lying(WID_ROCKET_LAUNCHER) == n,
                "vr_enemy_drops never: not in a run either");
    cvar_set("vr_enemy_drops", ftos(QVR_ENEMYDROPS_WHENELIGIBLE));

    // And off the run the gate is the player's own guns, as ever. (vrarena's name alone keeps its run on, so this
    // half is run on a map where the cvar is the only thing saying "arena".)
    cvar_set("vr_arena", "0");
    if(VR_ARK_Running())
    {
        VR_AT_Check(FALSE, "test 11 needs a map that is not vrarena: its name alone keeps the run on");
    }
    else
    {
        VR_AT_Check(!VR_WeaponUtil_EntIsEligibleForWeapon(p, WID_ROCKET_LAUNCHER),
                    "out of the run the rocket launcher is owned-or-nothing again");
        n = VR_AT11_Lying(WID_ROCKET_LAUNCHER);
        VR_AT_Check(!TryEnemyDrop(foe, p, WID_ROCKET_LAUNCHER, 1) && VR_AT11_Lying(WID_ROCKET_LAUNCHER) == n,
                    "out of the run a foe keeps its launcher to itself until a player has one");
    }
    cvar_set("vr_arena", "1");

    // A wave's own humanoid, spawned the way the director spawns one, holds its plan's strong gun although nobody
    // owns it -- the run's loot and its foes were never two different rules.
    entity keep = self;
    entity g = spawn();
    self = g;
    g.classname = "monster_army";
    setorigin(g, p.origin + '0 0 4096');
    g.weapon = WID_ROCKET_LAUNCHER;
    vr_ar_hold_roll = 1; // (as VR_ARENA_SpawnMonster holds a wave foe's armour roll, and its gun, to itself)
    monster_vr_grunt();
    vr_ar_hold_roll = 0;
    self = keep;
    dprint(sprintf("arena: t11 grunt: kind %g, wid %g, in hand %g, reserve %g\n",
                   g.vr_ag_kind, g.vr_ag_wid, g.vr_ag_prop != world, g.vr_ag_reserve));
    VR_AT_Check(g.vr_ag_wid == WID_ROCKET_LAUNCHER && g.vr_ag_prop != world,
                "an armed grunt holds the rocket launcher it was given, although no player owns one");
    remove(g);
}

// The ladder's strong humanoids, with the player owning none of their guns: waves 18 on.
void() VR_AT11_Plans =
{
    float hums = 0;
    float t2 = 0;
    float t3 = 0;
    float w;
    for(w = 18; w <= 30; w++)
    {
        VR_ARENA_Plan(w);
        for(float i = 0; i < ar_plan_n; i++)
        {
            if(!VR_AT_IsHumanoid(ar_plan_kind[i])) { continue; }
            hums = hums + 1;
            float t = VR_ARENA_WeaponTier(ar_plan_kind[i], ar_plan_gun[i]);
            if(t == 2) { t2 = t2 + 1; }
            if(t == 3) { t3 = t3 + 1; }
        }
    }
    dprint(sprintf("arena: t11 plans 18..30: %g humanoids, %g at tier 2, %g at tier 3\n", hums, t2, t3));
    VR_AT_Check(t2 + t3 > 0 && t3 > 0,
                sprintf("the waves from 18 on bring %g tier-2 and %g tier-3 humanoids, whatever the player holds", t2, t3));
}

void() VR_AT11_Begin =
{
    if(!ar_mgr) { VR_AT_Check(FALSE, "test 11 needs the arena running (vrtesthall with vr_arena 1)"); VR_AT_End(); return; }
    entity p = VR_ARENA_FindPlayer();
    if(!p) { VR_AT_Check(FALSE, "test 11 needs a player"); VR_AT_End(); return; }

    // The normal setting for both: what falls is filtered by eligibility, which is the gate this test is about.
    cvar_set("vr_enemy_drops", ftos(QVR_ENEMYDROPS_WHENELIGIBLE));
    cvar_set("vr_ammobox_drops", ftos(QVR_AMMOBOXDROPS_WHENELIGIBLE));

    VR_AT11_Eligible(p);
    VR_AT11_Drops(p);
    VR_AT11_Plans();

    // Live: wave 20 (tier 2 from 10, tier 3 from 18), the player still owning none of it.
    VR_ARENA_FastForward(20);
    vr_at_until = time + 90;
    dprint("arena: test 11 live wave 20\n");
}

void() VR_AT11_Frame =
{
    if(ar_wave < 20 || ar_plan_left > 0) { return; } // (the whole wave in, as test 10 waits for it)

    entity p = VR_ARENA_FindPlayer();
    float planned = 0;
    float worn = 0;
    float hums = 0;
    float held = 0;
    float owns = 0;
    float tier;

    for(float i = 0; i < ar_plan_n; i++)
    {
        if(VR_AT_IsHumanoid(ar_plan_kind[i]) && VR_ARENA_WeaponTier(ar_plan_kind[i], ar_plan_gun[i]) >= 2) { planned = planned + 1; }
    }

    for(entity e = findflags(world, ar_slot, ar_wave); e; e = findflags(e, ar_slot, ar_wave))
    {
        if(e.ar_slot != ar_wave || e.health <= 0) { continue; }
        if(!e.vr_ag) { continue; } // (a grunt or an enforcer: what the waves send in armed)
        hums = hums + 1;
        if(e.vr_ag_prop != world) { held = held + 1; }
        tier = VR_ARENA_WeaponTier(AR_SP_GRUNT, e.vr_ag_wid); // (the gun in its hand now, not the one its plan named)
        if(tier >= 2) { worn = worn + 1; }
    }

    for(float i = 0; i < VR_AT11_N; i++) { if(VR_WeaponUtil_EntHasWeapon(p, VR_AT11_Wid(i))) { owns = owns + 1; } }

    dprint(sprintf("arena: t11 live wave %g: %g armed humanoids (%g holding a gun), %g of %g strong guns worn, player owns %g of %g\n",
                   ar_wave, hums, held, worn, planned, owns, VR_AT11_N));

    VR_AT_Check(owns == 0, sprintf("wave %g arrives with the player owning none of its humanoids' guns", ar_wave));
    VR_AT_Check(planned > 0, sprintf("wave %g's plan has strong guns for its humanoids (%g of them)", ar_wave, planned));
    VR_AT_Check(worn == planned, sprintf("its humanoids hold its %g tier-2 and tier-3 guns (%g hold one)", planned, worn));
    VR_AT_Check(held >= worn, "and those guns are props in their hands, not a number in a field");

    VR_AT_End();
}

// ---------------------------------------------------------------------------
// Test 12: the weapon schedule (vr_arena.qc's table above VR_ARENA_PickGun) over seeded runs, plans only
// ---------------------------------------------------------------------------

#define AR_T12_ROWS 11 // (AR_GUN_N)
float AR_T12_SEEDS = 24;  // seeded runs per skill
float AR_T12_LAST  = 30;  // waves planned per run
float vr_at12_first[AR_T12_ROWS]; // the first wave a row's gun was in a (non-boss) humanoid's hands, over the runs

float vr_at12_early;     // a row's gun in hands before its wave
float vr_at12_bad;       // a trickle wave whose count is not the table's
float vr_at12_short;     // a trickle wave with fewer humanoids than carriers (not checked: reported)
float vr_at12_trickled;  // the trickle waves checked
float vr_at12_bossDealt; // a boss given a trickle gun
float vr_at12_k;         // the skill being run (its index in VR_AT12_SkillAt)
float vr_at12_s;         // its next seed

void(float xSkill) VR_AT12_SkillBegin =
{
    VR_AT_SetSkill(xSkill);
    float i;
    for(i = 0; i < AR_T12_ROWS; i++) { vr_at12_first[i] = 0; }
    vr_at12_early = 0;
    vr_at12_bad = 0;
    vr_at12_short = 0;
    vr_at12_trickled = 0;
    vr_at12_bossDealt = 0;
}

// One seeded run's 30 plans (one a frame: the whole sweep at once would run past the QC's instruction limit).
void(float xSkill, float s) VR_AT12_Seed =
{
    float i;
    float w;
    float j;
    {
        cvar_set("vr_arena_seed", ftos(s));
        for(w = 1; w <= AR_T12_LAST; w++)
        {
            VR_ARENA_Plan(w);
            float free = 0;
            for(j = 0; j < ar_plan_n; j++)
            {
                if(VR_AT_IsHumanoid(ar_plan_kind[j]) && !ar_plan_boss[j]) { free = free + 1; }
                if(ar_plan_boss[j] && ar_plan_dealt[j]) { vr_at12_bossDealt = vr_at12_bossDealt + 1; }
            }
            float need = 0;
            for(i = 0; i < AR_T12_ROWS; i++) { need = need + VR_ARENA_GunCarriers(i, w); }
            for(i = 0; i < AR_T12_ROWS; i++)
            {
                float n = VR_AT_GunCount(VR_ARENA_GunWid(i));
                if(n > 0 && (vr_at12_first[i] == 0 || w < vr_at12_first[i])) { vr_at12_first[i] = w; }
                if(VR_ARENA_GunFrom(i) <= 0) { continue; }
                if(w < VR_ARENA_GunEligible(i) && n > 0) { vr_at12_early = vr_at12_early + 1; }
                float c = VR_ARENA_GunCarriers(i, w);
                if(c <= 0) { continue; }
                if(need > free) { vr_at12_short = vr_at12_short + 1; continue; }
                vr_at12_trickled = vr_at12_trickled + 1;
                if(n != c)
                {
                    vr_at12_bad = vr_at12_bad + 1;
                    dprint(sprintf("arena: t12 skill %g seed %g wave %g gun %g: %g carriers, table %g\n", xSkill, s, w,
                                   VR_ARENA_GunWid(i), n, c));
                }
            }
        }
    }
}

void(float xSkill) VR_AT12_Finish =
{
    float i;
    for(i = 0; i < AR_T12_ROWS; i++)
    {
        float from = VR_ARENA_GunFrom(i);

        dprint(sprintf("arena: t12 skill %g row %g gun %g eligible %g pool from %g first seen %g\n", xSkill, i,
                       VR_ARENA_GunWid(i), from > 0 ? VR_ARENA_GunEligible(i) : AR_GUN_FIRST,
                       VR_ARENA_GunPoolFrom(i), vr_at12_first[i]));
        if(!VR_ARENA_GunOk(i))
        {
            VR_AT_Check(vr_at12_first[i] == 0, sprintf("skill %g: gun %g (not in this game) never comes", xSkill, VR_ARENA_GunWid(i)));
        }
        else if(from > 0)
        {
            VR_AT_Check(vr_at12_first[i] == VR_ARENA_GunEligible(i),
                        sprintf("skill %g: gun %g first at wave %g, the table's %g", xSkill, VR_ARENA_GunWid(i),
                                vr_at12_first[i], VR_ARENA_GunEligible(i)));
        }
    }
    VR_AT_Check(vr_at12_first[1] == 2, sprintf("skill %g: the nail pistol first at wave 2 (its own plan)", xSkill));
    VR_AT_Check(vr_at12_first[0] == AR_GUN_FIRST, sprintf("skill %g: the shotgun first at wave 3", xSkill));
    VR_AT_Check(vr_at12_early == 0, sprintf("skill %g: no gun before its wave (%g seen)", xSkill, vr_at12_early));
    VR_AT_Check(vr_at12_bad == 0 && vr_at12_trickled > 0, sprintf("skill %g: every trickle wave carries the table's count (%g of %g off)",
                                                  xSkill, vr_at12_bad, vr_at12_trickled));
    VR_AT_Check(vr_at12_bossDealt == 0, sprintf("skill %g: the bosses keep their own picks", xSkill));
    if(xSkill == 1) { VR_AT_Check(vr_at12_short == 0, "Normal: no trickle wave is vr_at12_short of humanoids"); }
    dprint(sprintf("arena: t12 skill %g trickle waves %g, vr_at12_short %g\n", xSkill, vr_at12_trickled, vr_at12_short));
}

float(float k) VR_AT12_SkillAt = { if(k == 0) { return 1; } if(k == 1) { return 0; } return k; }

void() VR_AT12_Run =
{
    // The pool's weights: a newcomer rarer on joining than four waves on, an old gun fading, never to nothing.
    float sng = 5;
    float pf = VR_ARENA_GunPoolFrom(sng);
    VR_AT_Check(VR_ARENA_GunWeight(sng, pf) > 0 && VR_ARENA_GunWeight(sng, pf) < VR_ARENA_GunWeight(sng, pf + 4),
                "a gun joining the pool starts rarer than it gets");
    VR_AT_Check(VR_ARENA_GunWeight(0, 25) < VR_ARENA_GunWeight(0, 5) && VR_ARENA_GunWeight(0, 60) > 0,
                "the shotgun fades from the pool as the waves go, never to nothing");
    VR_AT_Check(VR_ARENA_GunWeight(sng, pf - 1) == 0 && VR_ARENA_GunCarriers(sng, pf - 1) == 2 &&
                VR_ARENA_GunCarriers(sng, pf) == 0, "a gun on its trickle is not in the pool, then is only the pool");

    vr_at12_k = 0;
    vr_at12_s = 1;
    VR_AT12_SkillBegin(VR_AT12_SkillAt(0));
    vr_at_until = time + 120;
}

void() VR_AT12_Frame =
{
    float sk = VR_AT12_SkillAt(vr_at12_k);
    VR_AT12_Seed(sk, vr_at12_s);
    vr_at12_s = vr_at12_s + 1;
    if(vr_at12_s <= AR_T12_SEEDS) { return; }
    VR_AT12_Finish(sk);
    vr_at12_k = vr_at12_k + 1;
    vr_at12_s = 1;
    if(vr_at12_k >= 4) { VR_AT_SetSkill(1); VR_AT_End(); return; }
    VR_AT12_SkillBegin(VR_AT12_SkillAt(vr_at12_k));
}

// ---------------------------------------------------------------------------
// Test 13 (vrarena): spawns never overlap -- the box checks, then live waves with the player on a spawn place and
// medkits on the item points, every spawn scanned (VR_ARENA_ScanSpawn's count must stay 0)
// ---------------------------------------------------------------------------

float AR_T13_KILL = 1;   // s between the test's kills (the field churns: places free up and fill again)
float vr_at13_n;         // waves done
float vr_at13_target;
float vr_at13_kill;
float vr_at13_planned;   // foes the waves planned, all told

float(float i) VR_AT13_Wave = { if(i == 0) { return 8; } if(i == 1) { return 12; } return 16; }

entity(vector xAt) VR_AT13_Blocker =
{
    entity b = spawn();
    b.classname = "vr_at13_blocker";
    setmodel(b, "progs/player.mdl");
    b.solid = SOLID_TRIGGER;
    setsize(b, '-16 -16 0', '16 16 56');
    setorigin(b, xAt);
    return b;
}

void() VR_AT13_Next =
{
    vr_at13_target = VR_AT13_Wave(vr_at13_n);
    VR_ARENA_FastForward(vr_at13_target);
    vr_at13_kill = time + AR_T13_KILL;
    vr_at_until = time + 240;
}

void() VR_AT13_Begin =
{
    if(!ar_mgr) { VR_AT_Check(FALSE, "test 13 needs the arena (vrarena)"); VR_AT_End(); return; }
    entity g = VR_ARENA_SpawnSpot(1, 0);
    entity it = VR_ARENA_ItemSpot(0);
    entity p = VR_ARENA_FindPlayer();
    if(!g || !it || !p) { VR_AT_Check(FALSE, "test 13 needs vrarena's places and a player"); VR_AT_End(); return; }

    // The box checks: a thing on a place takes it; a drop's place with a thing on it is stepped round.
    entity b = VR_AT13_Blocker(g.origin + '0 0 8');
    VR_AT_Check(VR_ARENA_PlaceTaken(g.origin), "a thing standing on a spawn place takes it");
    remove(b);
    VR_AT_Check(!VR_ARENA_PlaceTaken(g.origin), "the same place empty is free");
    b = VR_AT13_Blocker(it.origin + '0 0 8');
    vector f = VR_ARENA_FreeNear(it.origin);
    VR_AT_Check(ar_place_ok && f != it.origin, "a drop's place with a thing on it is stepped round");
    VR_AT_Check(VR_ARENA_BoxBlocker(f + '-16 -16 8', f + '32 32 64', world) == world, "the step's box is clear");
    remove(b);

    // The live half: the player stands on a spawn place, medkits lie on every item point.
    setorigin(p, g.origin + '0 0 24');
    p.velocity = '0 0 0';
    float i;
    float total = 0;
    entity s = find(world, classname, "info_arena_item");
    while(s) { total = total + 1; s = find(s, classname, "info_arena_item"); }
    for(i = 0; i < total; i++) { VR_ARENA_SpawnDrop(1, VR_ARENA_ItemSpot(i).origin); }
    ar_overlaps = 0;
    ar_spawn_scans = 0;
    vr_at13_planned = 0;
    vr_at13_n = 0;
    VR_AT13_Next();
    dprint(sprintf("arena: t13 player on %s, %g medkits laid on the item points\n", vtos(g.origin), total));
}

void() VR_AT13_Frame =
{
    if(ar_wave != vr_at13_target) { return; }
    if(VR_ARENA_Between()) // (its clear ran: its drops are down)
    {
        vr_at13_planned = vr_at13_planned + ar_plan_n;
        dprint(sprintf("arena: t13 wave %g: %g foes in, %g felled, %g medkits %g armour dropped, scans %g overlaps %g\n",
                       ar_wave, ar_plan_live, ar_wave_kills, ar_drop_health, ar_drop_armour, ar_spawn_scans, ar_overlaps));
        VR_AT_Check(ar_plan_live == ar_plan_n, sprintf("wave %g put its whole plan in the field", ar_wave));
        VR_AT_Check(ar_drop_health == VR_ARENA_MedkitCount(ar_wave),
                    sprintf("wave %g's medkits all found room beside the ones lying there", ar_wave));
        vr_at13_n = vr_at13_n + 1;
        if(vr_at13_n >= 3)
        {
            VR_AT_Check(ar_spawn_scans >= vr_at13_planned, sprintf("every spawn was scanned (%g)", ar_spawn_scans));
            VR_AT_Check(ar_overlaps == 0, sprintf("no spawn overlapped anything (%g did)", ar_overlaps));
            VR_AT_End();
            return;
        }
        VR_AT13_Next();
        return;
    }
    if(time >= vr_at13_kill && VR_ARENA_CountAlive() > 0)
    {
        VR_AT_KillOne(ar_wave);
        vr_at13_kill = time + AR_T13_KILL;
    }
}

// ---------------------------------------------------------------------------
// Test 14: the rest and the countdown before a wave, timed (vrarena or vr_arena 1): Normal 5 s + 5 s into a boss
// wave, then Hard (the rest shorter, the countdown not) into a plain one
// ---------------------------------------------------------------------------

float AR_T14_EPS = 0.05; // s (the director's beats land on the 1/72 s frame)
float vr_at14_state;
float vr_at14_from;      // the wave being cleared
float vr_at14_count_at;

void() VR_AT14_Begin =
{
    if(!ar_mgr) { VR_AT_Check(FALSE, "test 14 needs the arena"); VR_AT_End(); return; }
    VR_AT_Check(VR_ARENA_BossName(5) == "an enforcer" && VR_ARENA_BossName(6) == "", "a boss wave's countdown names its boss");
    vr_at14_from = 4;
    vr_at14_state = 0;
    VR_ARENA_FastForward(vr_at14_from);
    vr_at_until = time + 200;
}

void() VR_AT14_Frame =
{
    if(vr_at14_state == 0) // (the wave cleared by the test, a foe at a time)
    {
        if(ar_wave != vr_at14_from || VR_ARENA_Between()) { return; }
        if(ar_plan_left == 0 && VR_ARENA_CountAlive() > 0) { VR_AT_KillOne(ar_wave); }
        else if(VR_ARENA_CountAlive() > 0) { VR_AT_KillOne(ar_wave); }
        if(ar_plan_left == 0 && VR_ARENA_CountAlive() == 0) { vr_at14_state = 1; }
        return;
    }
    if(vr_at14_state == 1) // (the rest: until the countdown begins)
    {
        if(ar_count_at <= 0) { return; }
        vr_at14_count_at = ar_count_at;
        float rest = ar_count_at - ar_rest_at;
        dprint(sprintf("arena: t14 skill %g wave %g clear, rest %g s (table %g)\n", skill, vr_at14_from, rest, VR_ARENA_RestTime()));
        VR_AT_Check(fabs(rest - VR_ARENA_RestTime()) < AR_T14_EPS, sprintf("skill %g: the rest is %g s", skill, VR_ARENA_RestTime()));
        vr_at14_state = 2;
        return;
    }
    if(vr_at14_state == 2) // (the countdown: until the next wave begins)
    {
        if(ar_wave != vr_at14_from + 1) { return; }
        float cd = ar_wave_at - vr_at14_count_at;
        dprint(sprintf("arena: t14 skill %g wave %g countdown %g s, %g lines\n", skill, ar_wave, cd, ar_count_said));
        VR_AT_Check(fabs(cd - 5) < AR_T14_EPS, sprintf("skill %g: the countdown is 5 s", skill));
        VR_AT_Check(ar_count_said == 5, sprintf("skill %g: the countdown said 5 lines", skill));
        if(skill >= 2) { VR_AT_End(); return; }
        VR_AT_SetSkill(2);
        vr_at14_from = ar_wave;
        vr_at14_state = 0;
        return;
    }
}

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
        if(c == 7) { VR_AT7_Begin(); return; }
        if(c == 8) { VR_AT8_Begin(); return; }
        if(c == 9) { VR_AT9_Begin(); return; }
        if(c == 10) { VR_AT10_Begin(); return; }
        if(c == 11) { VR_AT11_Begin(); return; }
        if(c == 12) { VR_AT12_Run(); return; }
        if(c == 13) { VR_AT13_Begin(); return; }
        if(c == 14) { VR_AT14_Begin(); return; }

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
    if(vr_at_case == 7) { VR_AT7_Frame(); return; }
    if(vr_at_case == 8) { VR_AT8_Frame(); return; }
    if(vr_at_case == 9) { VR_AT9_Frame(); return; }
    if(vr_at_case == 10) { VR_AT10_Frame(); return; }
    if(vr_at_case == 11) { VR_AT11_Frame(); return; }
    if(vr_at_case == 12) { VR_AT12_Frame(); return; }
    if(vr_at_case == 13) { VR_AT13_Frame(); return; }
    if(vr_at_case == 14) { VR_AT14_Frame(); return; }
}
#endif
