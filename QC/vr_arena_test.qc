#ifdef QVR_TESTS
// vr_arena_test.qc -- the Survival Arena's tests (vr_arena.qc). `vr_arena_test N` (Debug > Automation &
// Self-Tests; VR_ARENA_Test_Frame, world.qc's StartFrame), the player kept alive (`god`); each ends on
// - 36 (vrarena, god): how long walkers take between the floor and the dais, by the marked flights (Misc/quakevr/arenapath_test.sh).
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
//   Then the first foes in the field stand upright (no pitch, no roll: they face the player by yaw alone).
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
//   wave (its boss named), then Hard (4 s rest, the same 5 s countdown) into a plain wave; each countdown beeps 6
//   times (a tick a second, a call as the wave comes in).
// - 15 (vrarena): the scattered supplies (vr_arena_supply.qc): the walkable places (room, floor, the high ones), the
//   picks over three seeds (apart, off the player, different per seed, the same for one seed), two live clears' 1 or
//   2 shards and every supply settled on a walkable place, on its floor, clear of the brushes, dry, its line said.
// - 17 (vrarena): the walls are flat: a player's box and an ogre's stop 16 out of each wall at every height, a shot meets
//   the trim itself.
// - 18 (vrarena; run it fresh, after `restart fresh` and after a load): a run starts with no ammunition, fists in hand.
// - 38 (vrarena, and e1m1 for the half off the arena): the arena map check and the banners (vr_arena_banners.qc): a board's
//   text and the pixels of the image drawn from it (the client's copy, the gadget's layout) follow a scripted wave (foes left
//   falls, the score rises, the wave changes, the time runs, a perk's choice, the run's end), off it empties them.
// - 19 (vrarena): the waves still arm an empty player: wave 1 comes by itself and a fallen foe's pack gives ammunition.
// - 39 (vrarena, the headset mock): the arena's messages on the wrist (vr_arena_wrist): the countdown, wave 1, its clear,
//   a perk taken, a fall, each taken by the gadget's hologram and shown there (frames facing the eyes, ink in its image),
//   none in the middle of the view; the clear coming with the gadget out of view buzzes, shows nothing until it is
//   raised, then shows; off (0), the next ones show in view again and the wrist takes none.

// ---------------------------------------------------------------------------
// The test's own state
// ---------------------------------------------------------------------------

float vr_at_case;        // 0 idle; 1 .. 14 running
float vr_at_step;
float vr_at_until;
float vr_at_keep[35];    // the cvars the tests set, put back at the end
float vr_at_skill0;      // the skill the run had (its cvar's value; the tests pin it and put it back)
float vr_at_kills0;
float vr_at_score0;
float vr_at_a[AR_PLAN_MAX];
float vr_at_b[AR_PLAN_MAX];

// The cvars the tests set: VR_AT_Save keeps the run's own and VR_AT_Restore puts them back; VR_AT_SetDefaults sets each
// to its value here (the arena's own defaults, so a check says what the plan says and not what a config said; "": left
// for the tests that set it).
typedef struct
{
    string name, value;
} vr_at_cvar;

vr_at_cvar VR_AT_CVARS[] =
{
    { "vr_arena_seed", "7" },
    { "vr_arena_boss_every", "5" },
    { "vr_arena_boss_health", "1.25" },
    { "vr_arena_medkits_after", "2" },
    { "vr_arena_armor_every", "4" },
    { "vr_arena_score_kill", "100" },
    { "vr_arena_max_alive", "8" },
    { "vr_arena_skill_impact", "1" },
    { "vr_arena_corpse_fade", "14" },
    { "vr_arena_corpse_max", "12" },
    { "vr_arena_item_waves", "2" },
    { "vr_arena_gun_max", "24" },
    { "vr_arena_wave_size", "1" }, // (the ladder's own counts: tests 1 to 5 check them; 6 and 9 the default 1.75)
    { "vr_arena_alive_max", "6" },
    { "vr_arena_alive_grow", "4" },
    { "vr_arena_burst", "3" },
    { "vr_arena_spawn_gap", "1.5" },
    { "vr_arena_foe_armor", "1" },
    { "vr_arena_foe_armor_scale", "1" },
    { "vr_arena_foe_armor_from", "4" },
    { "vr_agarmor_skill1", "" },  // (test 10 sets the skill's own roll to certain, to see it held)
    { "vr_arena", "" },           // (test 11 looks at a run and at the same map with the run off)
    { "vr_enemy_drops", "" },     // (test 11: what a foe may leave behind, and its "never")
    { "vr_ammobox_drops", "" },   // (... the same for what an ammo box may carry)
    { "vr_arena_wave_rest", "5" },
    { "vr_arena_countdown", "5" },
    { "vr_arena_perks", "0" },    // (the perk cards hold the arena until picked: vr_arena_perks_test drives them)
    { "vr_arena_countdown_sound", "1" }, // (test 14 counts the beeps)
    { "vr_arena_scatter", "1" },  // (test 15: the supplies on random places)
    { "vr_arena_shards", "2" },
    { "vr_arena_xray", "1" },     // (test 16: the last foes seen through walls)
    { "vr_arena_xray_left", "2" },
    { "vr_arena_xray_distance", "600" },
    { "vr_arena_banners", "1" },  // (test 38: the banners)
    { "vr_arena_wrist", "1" },    // (test 39: the messages on the wrist)
    { "vr_agrunt_flaregun", "" }  // (test 40: its grunts hold the one weapon they were given)
};
float VR_AT_NCVARS = 35;

void() VR_AT_Save =
{
    float i;
    for(i = 0; i < VR_AT_NCVARS; i++) { vr_at_keep[i] = cvar(VR_AT_CVARS[i].name); }
}

void() VR_AT_Restore =
{
    float i;
    for(i = 0; i < VR_AT_NCVARS; i++) { cvar_set(VR_AT_CVARS[i].name, ftos(vr_at_keep[i])); }
}

// The tests' own difficulty, both halves of it: the cvar (world.qc reads it into the `skill` global every frame)
// and the global itself. 0 Easy, 1 Normal, 2 Hard, 3 Nightmare.
void(float xSkill) VR_AT_SetSkill =
{
    cvar_set("skill", ftos(xSkill));
    skill = xSkill;
}

// The tests' settings (VR_AT_CVARS' values) and the skill.
void() VR_AT_SetDefaults =
{
    float i;
    for(i = 0; i < VR_AT_NCVARS; i++)
    {
        if(VR_AT_CVARS[i].value != "") { cvar_set(VR_AT_CVARS[i].name, VR_AT_CVARS[i].value); }
    }
    VR_AT_SetSkill(1); // (Normal, where the skill's factor is exactly 1: every family's own numbers mean the cvars)
}

void(float ok, string what) VR_AT_Check = { VR_Test_Check(ok, what); }; // (`arena: FAIL <what>`)

void() VR_AT_End =
{
    VR_Test_VerdictPart(sprintf("test %g over:", vr_at_case));
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

// How many of the plan's (non-boss) humanoids hold a gun of the weapon schedule's row `xRow` (any code of its run).
float(float xRow) VR_AT_RowCount =
{
    float n = 0;
    float j;
    for(j = 0; j < ar_plan_n; j++)
    {
        if(VR_AT_IsHumanoid(ar_plan_kind[j]) && !ar_plan_boss[j] && VR_ARENA_GunRow(ar_plan_gun[j]) == xRow) { n = n + 1; }
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
        i = VR_ARENA_GunRow(ar_plan_gun[j]);
        if(i >= 0 && xWave >= VR_ARENA_GunEligible(i)) { ok = TRUE; }
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
    entity holder = VR_PlayerAlive();
    entity held0 = world;
    if(holder) { held0 = holder.mainhand_held; VR_Hand_Take(holder, cVR_MainHand, foe); foe.carry_player = holder; }
    VR_ARENA_ClearField(5);
    VR_AT_Check(VR_AT_InUse(body), "a stalled wave's clear leaves an older wave's body");
    VR_AT_Check(!VR_AT_InUse(foe), "a stalled wave's clear takes its foe still up");
    if(holder)
    {
        VR_AT_Check(holder.mainhand_held == world, "a stalled wave's foe in a hand is let go of first");
        VR_Hand_Take(holder, cVR_MainHand, held0);
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
    entity p = VR_PlayerAlive();
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
    entity p = VR_PlayerAlive();

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
        vr_at_step = 1;
        return;
    }

    // The foes come in upright, turned toward the player by yaw alone: a pitch taken from the line to the player (a
    // foe on a high place, the player below) stayed on them for good, their guns and swings drawn tilted. Checked once
    // two of the wave's foes are in (the first wave's opening burst).
    float nfoes = 0;
    float ntilted = 0;
    entity f = nextent(world);
    while(f)
    {
        if(f.ar_slot > 0 && f.health > 0 && (f.flags & FL_MONSTER))
        {
            nfoes = nfoes + 1;
            if(fabs(f.angles_x) > 0.01 || fabs(f.angles_z) > 0.01)
            {
                ntilted = ntilted + 1;
                dprint(sprintf("arena: t4 %s tilted: pitch %g roll %g\n", f.classname, f.angles_x, f.angles_z));
            }
        }
        f = nextent(f);
    }
    if(nfoes < min(2, ar_plan_n)) { return; }
    dprint(sprintf("arena: t4 foes in the field %g, tilted %g\n", nfoes, ntilted));
    VR_AT_Check(ntilted == 0, "the wave's foes come in upright (no pitch, no roll)");
    VR_AT_End();
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
        dprint(sprintf("arena: t5 impact %g skill %g factor %g\n", cvar_hget(cvarh_vr_arena_skill_impact), sk, VR_AT5_Factor()));
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
float AR_T6_DROPS = 5;       // a wave clear's drops at most (two medkits, an armour and two shards, at Normal)
float AR_T6_SPAN  = 4;       // waves in each end's window: the growth is the late window's floor less the early one's
                             // (the edicts at a wave's start, its drops left out, swing by +-30 with the gibs and limbs
                             // of the moment: one count against one count failed a run with no leak, nightly 2026-11-01)
float vr_at6_lo0;            // the fewest edicts (less the drops) in the early window (waves AR_T6_FROM..), the late one
float vr_at6_lo1;
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
    entity p = VR_PlayerAlive();
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
    vr_at6_delay = cvar_hget(cvarh_vr_arena_wave_rest);
    cvar_set("vr_arena_wave_rest", "1");
    cvar_set("vr_arena_countdown", "0"); // (the soak: no countdown either; VR_AT_End puts it back)
    cvar_set("vr_arena_wave_size", "1.75"); // (the default's bigger waves: the soak is bounded with them)
    vr_at6_wave = ar_wave;
    vr_at6_e0 = 0;
    vr_at6_lo0 = 100000;
    vr_at6_lo1 = 100000;
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
    // (Between the windows' middles: AR_T6_FROM + (SPAN - 1) / 2 and AR_T6_WAVES - (SPAN - 1) / 2.)
    float span = (AR_T6_WAVES - AR_T6_FROM) - (AR_T6_SPAN - 1);
    float grow = vr_at6_lo1 - vr_at6_lo0;
    dprint(sprintf("arena: t6 waves %g edicts %g -> %g (peak %g) drops %g -> %g\n", waves, vr_at6_e0, e1, vr_at6_peak,
                   vr_at6_d0, d1));
    dprint(sprintf("arena: t6 floor %g -> %g over %g waves\n", vr_at6_lo0, vr_at6_lo1, span));
    dprint(sprintf("arena: t6 bodies %g guns %g longest wave %g s\n", VR_AT6_Bodies(), VR_AT6_CountClass("thrown_weapon"),
                   vr_at6_long));
    VR_AT_Check(ar_wave >= AR_T6_WAVES, "the soak reached its last wave");
    VR_AT_Check(vr_at6_long < AR_STALL, "no wave stalled");
    VR_AT_Check(grow <= AR_T6_LEAK * span, "the map grows no faster than a wave clear's drops");
    // The cleanup's caps (vr_arena.qc): the bodies lying (not fading), the foes' guns, a wave clear's drops.
    VR_AT_Check(VR_AT_LyingBodies() <= cvar_hget(cvarh_vr_arena_corpse_max), "no more bodies lie than vr_arena_corpse_max");
    VR_AT_Check(VR_AT_FoeGuns() <= cvar_hget(cvarh_vr_arena_gun_max), "no more of the foes' guns lie than vr_arena_gun_max");
    VR_AT_Check(d1 <= AR_T6_DROPS * (cvar_hget(cvarh_vr_arena_item_waves) + 1), "a wave clear's drops go vr_arena_item_waves clears on");
    dprint(sprintf("arena: t6 gun scan: %g weapons scanned, %g broken, %g fades mended\n", vr_at6_gunscans, vr_at6_gunbad,
                   vr_dropcap_mended));
    // The fades' safety net (VR_EnemyWeapons_MendFades): a fade stopped midway (its think replaced) on again; one long
    // overdue gone.
    entity pl = VR_PlayerAlive();
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
        float net = n - VR_AT6_Drops();
        if(ar_wave >= AR_T6_FROM && ar_wave < AR_T6_FROM + AR_T6_SPAN && net < vr_at6_lo0) { vr_at6_lo0 = net; }
        if(ar_wave > AR_T6_WAVES - AR_T6_SPAN && net < vr_at6_lo1) { vr_at6_lo1 = net; }
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
    entity p = VR_PlayerAlive();
    if(p == world) { VR_AT_Check(FALSE, "test 7 needs a player"); VR_AT_End(); return; }

    VR_AT7_Guns(p);

    float n0 = VR_AT_LyingBodies();
    cvar_set("vr_arena_corpse_fade", "2");
    cvar_set("vr_arena_corpse_max", ftos(n0 + 3));
    vr_at7_held0 = p.mainhand_held;
    vr_at7_b0 = VR_AT7_Body();
    VR_Hand_Take(p, cVR_MainHand, vr_at7_b0); // (a hand on it)
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
    entity p = VR_PlayerAlive();
    if(vr_at_step == 1)
    {
        if(p) { VR_Hand_Take(p, cVR_MainHand, vr_at7_b0); } // (held: taken again every beat, as a held ragdoll's limb is)
        VR_ARENA_Leftovers();
        if(VR_AT_Live(vr_at7_b1) || VR_AT_Live(vr_at7_b2) || VR_AT_Live(vr_at7_b3) || VR_AT_Live(vr_at7_d1)) { return; }
        VR_AT_Check(VR_AT_Live(vr_at7_b0) && vr_at7_b0.alpha == 0, "bodies: held, it outlives its time, seen whole");
        VR_AT_Check(VR_AT_Live(vr_at7_b5), "bodies: pulled by a force grab, it outlives its time");
        VR_AT_Check(VR_AT_Live(vr_at7_b4), "bodies: still dying, it outlives its time");
        VR_AT_Check(VR_AT_Live(vr_at7_d4), "drops: pulled by a force grab, it outlives its time");
        VR_AT_Check(VR_AT_Live(vr_at7_d2), "drops: one clear old, it lies on");
        if(p) { VR_Hand_Take(p, cVR_MainHand, vr_at7_held0); }
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
    entity p = VR_Player();
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
    else if(xBossWave && c > 0) { want = clamp(c + AR_ARM_GUARD, 0, AR_ARM_CAP); }

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
            entity p = VR_PlayerAlive();
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
        setorigin(g, VR_PlayerAlive().origin + '0 0 4096'); // (out of the way; it is removed at once)
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

    // And off the run the gate is the player's own guns, as ever. (an arena map's name alone keeps its run on, so this
    // half is run on a map where the cvar is the only thing saying "arena".)
    cvar_set("vr_arena", "0");
    if(VR_ARK_Running())
    {
        VR_AT_Check(FALSE, "test 11 needs a map that is not an arena map: its name alone keeps the run on");
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
    entity p = VR_PlayerAlive();
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

    entity p = VR_PlayerAlive();
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
        // (the gun in its hand now, not the one its plan named: by the code it came with while it holds that weapon --
        // an AD upgrade's or a sword kind's row, not the plain weapon's)
        float code = e.vr_ag_wid;
        if(VR_AG_Decode(e.vr_ag_code) && vr_ag_dec_wid == e.vr_ag_wid) { code = e.vr_ag_code; }
        tier = VR_ARENA_WeaponTier(AR_SP_GRUNT, code);
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

#define AR_T12_ROWS 34 // (AR_GUN_N)
float AR_T12_SEEDS = 24;  // seeded runs per skill
float AR_T12_LAST  = 30;  // waves planned per run
float vr_at12_first[AR_T12_ROWS]; // the first wave a row's gun was in a (non-boss) humanoid's hands, over the runs
float vr_at12_cnt[AR_T12_ROWS];   // this plan's (non-boss) humanoids holding a row's gun (VR_AT12_Count)
float vr_at12_ntr[AR_T12_ROWS];   // over the runs: a row's guns on its trickle waves (eligible .. pool - 1) ...
float vr_at12_npool[AR_T12_ROWS]; // ... and in the pool after (up to wave AR_T12_LAST)

float vr_at12_early;     // a row's gun in hands before its wave
float vr_at12_bad;       // a trickle wave whose count is not the table's
float vr_at12_short;     // a trickle wave with fewer humanoids than carriers (not checked: reported)
float vr_at12_trickled;  // the trickle waves checked
float vr_at12_bossDealt; // a boss given a trickle gun
float vr_at12_offrow;    // a humanoid's gun no row of the table has (not its fists)
float vr_at12_k;         // the skill being run (its index in VR_AT12_SkillAt)
float vr_at12_s;         // its next seed

void(float xSkill) VR_AT12_SkillBegin =
{
    VR_AT_SetSkill(xSkill);
    float i;
    for(i = 0; i < AR_T12_ROWS; i++) { vr_at12_first[i] = 0; vr_at12_ntr[i] = 0; vr_at12_npool[i] = 0; }
    vr_at12_early = 0;
    vr_at12_bad = 0;
    vr_at12_short = 0;
    vr_at12_trickled = 0;
    vr_at12_bossDealt = 0;
    vr_at12_offrow = 0;
}

// The plan's (non-boss) humanoids by their guns' rows (vr_at12_cnt), in one pass.
void() VR_AT12_Count =
{
    float i;
    for(i = 0; i < AR_T12_ROWS; i++) { vr_at12_cnt[i] = 0; }
    for(float j = 0; j < ar_plan_n; j++)
    {
        if(!VR_AT_IsHumanoid(ar_plan_kind[j]) || ar_plan_boss[j] || ar_plan_gun[j] == AR_FIST) { continue; }
        i = VR_ARENA_GunRow(ar_plan_gun[j]);
        if(i < 0) { vr_at12_offrow = vr_at12_offrow + 1; continue; }
        vr_at12_cnt[i] = vr_at12_cnt[i] + 1;
    }
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
            VR_AT12_Count();
            float free = 0;
            for(j = 0; j < ar_plan_n; j++)
            {
                if(VR_AT_IsHumanoid(ar_plan_kind[j]) && !ar_plan_boss[j]) { free = free + 1; }
                if(ar_plan_boss[j] && ar_plan_dealt[j]) { vr_at12_bossDealt = vr_at12_bossDealt + 1; }
            }
            float need = 0;
            for(i = 0; i < AR_T12_ROWS; i++) { need = need + VR_ARENA_GunCarriers(i, w); }
            if(need > free && xSkill == 1)
            {
                dprint(sprintf("arena: t12 skill %g seed %g wave %g short: %g carriers, %g humanoids\n", xSkill, s, w, need, free));
            }
            for(i = 0; i < AR_T12_ROWS; i++)
            {
                float n = vr_at12_cnt[i];
                if(n > 0 && (vr_at12_first[i] == 0 || w < vr_at12_first[i])) { vr_at12_first[i] = w; }
                if(w >= VR_ARENA_GunPoolFrom(i)) { vr_at12_npool[i] = vr_at12_npool[i] + n; }
                else if(w >= VR_ARENA_GunEligible(i)) { vr_at12_ntr[i] = vr_at12_ntr[i] + n; }
                if(VR_ARENA_GunFrom(i) <= 0) { continue; }
                if(w < VR_ARENA_GunEligible(i) && n > 0) { vr_at12_early = vr_at12_early + 1; }
                float c = VR_ARENA_GunCarriers(i, w);
                if(c <= 0) { continue; }
                if(need > free) { vr_at12_short = vr_at12_short + 1; continue; }
                vr_at12_trickled = vr_at12_trickled + 1;
                if(n != c)
                {
                    vr_at12_bad = vr_at12_bad + 1;
                    dprint(sprintf("arena: t12 skill %g seed %g wave %g row %g (%s): %g carriers, table %g\n", xSkill, s, w,
                                   i, VR_ARENA_GunName(i), n, c));
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

        // (the counts over the skill's AR_T12_SEEDS runs: on its trickle waves, then in the pool to wave AR_T12_LAST)
        dprint(sprintf("arena: t12 skill %g row %g %s tier %g ok %g ", xSkill, i, VR_ARENA_GunName(i),
                       VR_ARENA_GunTier(i), VR_ARENA_GunOk(i)));
        dprint(sprintf("eligible %g pool from %g first seen %g trickle %g pool %g\n", VR_ARENA_GunEligible(i),
                       VR_ARENA_GunPoolFrom(i), vr_at12_first[i], vr_at12_ntr[i], vr_at12_npool[i]));
        if(!VR_ARENA_GunOk(i))
        {
            VR_AT_Check(vr_at12_first[i] == 0, sprintf("skill %g: %s (not in this game) never comes", xSkill, VR_ARENA_GunName(i)));
        }
        else if(from > 0)
        {
            VR_AT_Check(vr_at12_first[i] == VR_ARENA_GunEligible(i),
                        sprintf("skill %g: %s first at wave %g, the table's %g", xSkill, VR_ARENA_GunName(i),
                                vr_at12_first[i], VR_ARENA_GunEligible(i)));
        }
        else
        {
            VR_AT_Check(vr_at12_first[i] > 0, sprintf("skill %g: %s (a bottom rung) comes", xSkill, VR_ARENA_GunName(i)));
        }
    }
    VR_AT_Check(vr_at12_first[1] == 2, sprintf("skill %g: the nail pistol first at wave 2 (its own plan)", xSkill));
    VR_AT_Check(vr_at12_first[0] == AR_GUN_FIRST, sprintf("skill %g: the shotgun first at wave 3", xSkill));
    VR_AT_Check(vr_at12_early == 0, sprintf("skill %g: no gun before its wave (%g seen)", xSkill, vr_at12_early));
    VR_AT_Check(vr_at12_offrow == 0, sprintf("skill %g: every humanoid's gun is one of the table's (%g not)", xSkill, vr_at12_offrow));
    VR_AT_Check(vr_at12_bad == 0 && vr_at12_trickled > 0, sprintf("skill %g: every trickle wave carries the table's count (%g of %g off)",
                                                  xSkill, vr_at12_bad, vr_at12_trickled));
    VR_AT_Check(vr_at12_bossDealt == 0, sprintf("skill %g: the bosses keep their own picks", xSkill));
    if(xSkill == 1) { VR_AT_Check(vr_at12_short == 0, "Normal: no trickle wave is short of humanoids"); }
    dprint(sprintf("arena: t12 skill %g trickle waves %g, short %g\n", xSkill, vr_at12_trickled, vr_at12_short));
}

float(float k) VR_AT12_SkillAt = { if(k == 0) { return 1; } if(k == 1) { return 0; } return k; }

void() VR_AT12_Run =
{
    // The pool's weights: a newcomer rarer on joining than four waves on, an old gun fading, never to nothing.
    float sng = VR_ARENA_GunRow(WID_SUPER_NAILGUN);
    float pf = VR_ARENA_GunPoolFrom(sng);
    // The rows of the whole table: each its own code(s), in order of the wave it comes at; the tiers rise with it.
    float i;
    float rowsOk = TRUE;
    for(i = 0; i < AR_GUN_N; i++)
    {
        if(VR_ARENA_GunRow(VR_ARENA_GunWid(i)) != i) { rowsOk = FALSE; }
        if(i > 0 && VR_ARENA_GunFrom(i) < VR_ARENA_GunFrom(i - 1)) { rowsOk = FALSE; }
    }
    VR_AT_Check(rowsOk, "each row of the weapon schedule is its own code, the rows in the order of their waves");
    VR_AT_Check(VR_ARENA_WeaponTier(AR_SP_GRUNT, WID_REVOLVER) == 2 && VR_ARENA_WeaponTier(AR_SP_GRUNT, WID_ENFRIFLE) == 2 &&
                VR_ARENA_WeaponTier(AR_SP_GRUNT, WID_ADPYRO) == 3 && VR_ARENA_WeaponTier(AR_SP_GRUNT, WID_CROWBAR) == 1 &&
                VR_ARENA_WeaponTier(AR_SP_GRUNT, VR_AG_CODE_ADMELEE + VR_ADMELEE_DKNIGHT) == 2,
                "the new rows' tiers: the revolver and the rifles the second, the flamethrower the third, the crowbar the first");
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
    entity p = VR_PlayerAlive();
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
float vr_at14_sounds;    // the countdown's beeps before this countdown

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
        vr_at14_sounds = ar_count_sounds - 1; // (the countdown's first tick came with its start)
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
        // (its sounds: a tick each of its 5 seconds and a call as the wave comes in)
        dprint(sprintf("arena: t14 skill %g countdown sounds %g\n", skill, ar_count_sounds - vr_at14_sounds));
        VR_AT_Check(ar_count_sounds - vr_at14_sounds == 6, sprintf("skill %g: the countdown beeped 6 times", skill));
        if(skill >= 2) { VR_AT_End(); return; }
        VR_AT_SetSkill(2);
        vr_at14_from = ar_wave;
        vr_at14_state = 0;
        return;
    }
}

// ---------------------------------------------------------------------------
// Test 15: the scattered supplies (vrarena): the map's walkable places (the player's box fits on each, floor under it,
// some up high), the picks over three seeds (each good, apart, off the player, seed to seed different, the same seed
// the same), then two live wave clears: 1 or 2 shards each, every supply on one of the walkable places, settled on
// its floor clear of the brushes and out of any liquid, and its line said.
// ---------------------------------------------------------------------------

#define AR_T15_PICKS 6
float vr_at15_state;
float vr_at15_from;              // the wave being cleared
float vr_at15_n;                 // wave clears looked at
float vr_at15_high;              // picks that landed up high
vector vr_at15_pick[18];         // three seeds' picks (AR_T15_PICKS each)
vector vr_at15_last;             // the last clear's first supply (two clears land apart)

void(float xSeed, float xRow) VR_AT15_Picks =
{
    cvar_set("vr_arena_seed", ftos(xSeed));
    VR_ARENA_ScatterBegin(4);
    entity p = VR_PlayerAlive();
    float i;
    for(i = 0; i < AR_T15_PICKS; i++)
    {
        vector f = VR_ARENA_ScatterPlace();
        VR_AT_Check(ar_place_ok, sprintf("seed %g: pick %g found a place", xSeed, i));
        VR_AT_Check(!p || vlen(f + '0 0 24' - p.origin) >= AR_SCATTER_NEAR, sprintf("seed %g: pick %g off the player", xSeed, i));
        vr_at15_pick[xRow * AR_T15_PICKS + i] = f;
        float j;
        for(j = 0; j < i; j++)
        {
            VR_AT_Check(vlen(vr_at15_pick[xRow * AR_T15_PICKS + j] - f) >= AR_SCATTER_GAP,
                        sprintf("seed %g: picks %g and %g apart", xSeed, j, i));
        }
    }
    vr_at15_high = vr_at15_high + ar_sc_high;
    dprint(sprintf("arena: t15 seed %g picks %s %s %s ... (%g up high)\n", xSeed, vtos(vr_at15_pick[xRow * AR_T15_PICKS]),
                   vtos(vr_at15_pick[xRow * AR_T15_PICKS + 1]), vtos(vr_at15_pick[xRow * AR_T15_PICKS + 2]), ar_sc_high));
}

// f (a floor point) on one of the walkable places.
float(vector f) VR_AT15_OnPlace =
{
    float k;
    for(k = 0; k < ar_nav_n; k++)
    {
        vector d = (navplace(k) - '0 0 24') - f;
        if(fabs(d_z) < 24 && vlen(d - d_z * '0 0 1') < 40) { return TRUE; }
    }
    return FALSE;
}

void() VR_AT15_Places =
{
    float k;
    float bad = 0;
    float top = 0;
    for(k = 0; k < ar_nav_n; k++)
    {
        vector o = navplace(k);
        tracebox(o, VEC_HULL_MIN, VEC_HULL_MAX, o, TRUE, world);
        float inside = trace_startsolid;
        tracebox(o, VEC_HULL_MIN, VEC_HULL_MAX, o - '0 0 4', TRUE, world); // (his box stands on it: on a stair's edge too)
        float floored = trace_fraction < 1 && !trace_startsolid;
        if(inside || !floored)
        {
            if(bad < 4) { dprint(sprintf("arena: t15 bad place %s: inside %g floor %g\n", vtos(o), inside, floored)); }
            bad = bad + 1;
            continue;
        }
        if(o_z - 24 > top) { top = o_z - 24; }
    }
    dprint(sprintf("arena: t15 %g walkable places (%g high), %g bad, highest floor %g\n", ar_nav_n,
                   ar_nav_hi_n, bad, top));
    VR_AT_Check(ar_nav_n >= 300, sprintf("the arena's walkable places found (%g)", ar_nav_n));
    VR_AT_Check(ar_nav_n < AR_NAV_MAX, "the walk ended before its cap");
    VR_AT_Check(bad == 0, sprintf("every walkable place has the player's room and floor under it (%g not)", bad));
    VR_AT_Check(ar_nav_hi_n >= 20, sprintf("the high places are walked to (%g)", ar_nav_hi_n));
    VR_AT_Check(top >= 250, sprintf("the tower tops and the wall walk are among them (highest %g)", top));
    // (vrarena's 8 covers, 96 square and 48 high at radius 512, 45 deg apart: no step up there, so their tops are no
    // place to land on -- a place over one is a cover's top)
    float covers = 0;
    for(k = 0; k < ar_nav_n; k++)
    {
        vector c = navplace(k);
        float a;
        for(a = 0; a < 8; a++)
        {
            vector m = '0 0 0';
            m_x = 8 * rint(512 * cos(a * 0.785398) / 8); // (45 deg)
            m_y = 8 * rint(512 * sin(a * 0.785398) / 8);
            if(fabs(c_x - m_x) < 48 && fabs(c_y - m_y) < 48 && c_z - 24 > 40 && c_z - 24 < 60)
            {
                if(covers < 4) { dprint(sprintf("arena: t15 a place on a cover's top %s\n", vtos(c))); }
                covers = covers + 1;
            }
        }
    }
    VR_AT_Check(covers == 0, sprintf("no place on top of a 48-high cover (%g)", covers));

    vr_at15_high = 0;
    VR_AT15_Picks(3, 0);
    VR_AT15_Picks(11, 1);
    VR_AT15_Picks(29, 2);
    float same = 0;
    for(k = 0; k < AR_T15_PICKS; k++)
    {
        if(vlen(vr_at15_pick[k] - vr_at15_pick[AR_T15_PICKS + k]) < 1) { same = same + 1; }
        if(vlen(vr_at15_pick[k] - vr_at15_pick[2 * AR_T15_PICKS + k]) < 1) { same = same + 1; }
    }
    VR_AT_Check(same <= 2, sprintf("the picks differ seed to seed (%g of 12 the same)", same));
    VR_AT_Check(vr_at15_high > 0, sprintf("some picks land up high (%g of 18)", vr_at15_high));
    vector first = vr_at15_pick[0];
    vector second = vr_at15_pick[1];
    VR_AT15_Picks(3, 1);
    VR_AT_Check(vlen(vr_at15_pick[AR_T15_PICKS] - first) < 1 && vlen(vr_at15_pick[AR_T15_PICKS + 1] - second) < 1,
                "the same seed picks the same places");
    cvar_set("vr_arena_seed", "7");
}

// A clear's supplies, settled: each on a walkable place, on its floor, clear of the brushes, out of any liquid.
void() VR_AT15_Clear =
{
    float total = 0;
    float shards = 0;
    float kits = 0;
    float bad = 0;
    vector one = '0 0 0';
    entity e = findflags(world, ar_drop, 3);
    while(e)
    {
        if(e.ar_drop_wave == vr_at15_from && e.model != "")
        {
            total = total + 1;
            if(e.classname == "item_armor_shard") { shards = shards + 1; }
            if(e.ar_drop == 1) { kits = kits + 1; }
            vector c = e.origin + 0.5 * (e.mins + e.maxs);
            c_z = e.origin_z + e.mins_z;
            if(total == 1) { one = c; }
            tracebox(e.origin, e.mins, e.maxs, e.origin, TRUE, world);
            float inside = trace_startsolid;
            traceline(c + '0 0 4', c - '0 0 12', TRUE, world);
            float floored = trace_fraction < 1;
            float dry = pointcontents(c + '0 0 8') == CONTENT_EMPTY;
            float placed = VR_AT15_OnPlace(c);
            if(inside || !floored || !dry || !placed)
            {
                bad = bad + 1;
                dprint(sprintf("arena: t15 bad supply %s at %s: inside %g floor %g dry %g on a place %g\n", e.classname,
                               vtos(c), inside, floored, dry, placed));
            }
        }
        e = findflags(e, ar_drop, 3);
    }
    string line = VR_ARENA_SupplyLine();
    dprint(sprintf("arena: t15 wave %g clear: %g supplies (%g medkits, %g shards), %g on random places, %g up high, ",
                   vr_at15_from, total, kits, shards, ar_sc_n, ar_sc_high));
    dprint(sprintf("%g bad; the line: %s\n", bad, line));
    VR_AT_Check(shards >= 1 && shards <= 2 && shards == ar_drop_shards, sprintf("wave %g: 1 or 2 shards (%g)", vr_at15_from, shards));
    VR_AT_Check(kits == ar_drop_health && kits == VR_ARENA_MedkitCount(vr_at15_from), sprintf("wave %g: its medkits", vr_at15_from));
    VR_AT_Check(total == ar_drop_health + ar_drop_armour + ar_drop_shards, sprintf("wave %g: every supply lies there", vr_at15_from));
    VR_AT_Check(ar_sc_n == total, sprintf("wave %g: every supply on a random place", vr_at15_from));
    VR_AT_Check(bad == 0, sprintf("wave %g: every supply on a walkable place, on its floor, clear and dry", vr_at15_from));
    VR_AT_Check(line != "" && (VR_ARENA_Between()), sprintf("wave %g: its supplies' line said", vr_at15_from));
    if(vr_at15_n > 0) { VR_AT_Check(vlen(one - vr_at15_last) > 1, "two clears' supplies land apart"); }
    vr_at15_last = one;
}

void() VR_AT15_Begin =
{
    if(!ar_mgr) { VR_AT_Check(FALSE, "test 15 needs the arena (vrarena)"); VR_AT_End(); return; }
    vr_at15_state = 0;
    vr_at15_n = 0;
    vr_at_until = time + 240;
}

void() VR_AT15_Frame =
{
    if(vr_at15_state == 0) // (the walk over the map: until it is done)
    {
        if(!VR_ARNAV_Ready()) { return; }
        VR_AT15_Places();
        vr_at15_from = 3;
        VR_ARENA_FastForward(vr_at15_from);
        vr_at15_state = 1;
        return;
    }
    if(vr_at15_state == 1) // (the wave cleared by the test, a foe at a time)
    {
        if(ar_wave != vr_at15_from) { return; }
        if(VR_ARENA_Between()) { vr_at15_state = 2; return; }
        if(VR_ARENA_CountAlive() > 0) { VR_AT_KillOne(ar_wave); }
        return;
    }
    if(vr_at15_state == 2) // (its supplies settled on their floors)
    {
        entity e = findflags(world, ar_drop, 3);
        while(e)
        {
            if(e.ar_drop_wave == vr_at15_from && e.think == PlaceItem) { return; }
            e = findflags(e, ar_drop, 3);
        }
        VR_AT15_Clear();
        vr_at15_n = vr_at15_n + 1;
        if(vr_at15_n >= 2) { VR_AT_End(); return; }
        vr_at15_from = vr_at15_from + 1;
        vr_at15_state = 1;
        return;
    }
}

// ---------------------------------------------------------------------------
// Test 16: the wave's last foes seen through walls (vr_arena_xray; vrarena). A wave is brought down a foe at a time:
// with three left (up and to come) none is marked or outlined; with two left every one of them up is marked, and the
// engine puts each through its outline pass (vr_arena_xray_shown, the foes it outlined the last frame, read back)
// while the outline distance is short, none at a distance past the arena's size; turned off, none is marked.
// ---------------------------------------------------------------------------
float vr_at16_state;
float vr_at16_from; // the wave
float vr_at16_at;   // the time of the step's last change (the director's beat and a frame drawn since)

// The wave's foes up are all marked (xMarked) or none is; a fallen foe is never marked.
float(float xMarked) VR_AT16_Marks =
{
    float up = 0;
    float marked = 0;
    entity e = findflags(world, ar_slot, ar_wave);
    while(e)
    {
        if(e.health > 0) { up = up + 1; if(e.vr_xray) { marked = marked + 1; } }
        else if(e.vr_xray) { marked = marked + 100; }
        e = findflags(e, ar_slot, ar_wave);
    }
    if(xMarked) { return marked == up && up > 0; }
    return marked == 0;
}

void() VR_AT16_Begin =
{
    if(!ar_mgr) { VR_AT_Check(FALSE, "test 16 needs the arena (vrarena)"); VR_AT_End(); return; }
    cvar_set("vr_arena_alive_max", "8"); // (the whole wave up at once)
    cvar_set("vr_arena_burst", "8");
    vr_at16_from = 6;
    VR_ARENA_FastForward(vr_at16_from);
    vr_at16_state = 0;
    vr_at16_at = 0;
    vr_at_until = time + 120;
}

void() VR_AT16_Frame =
{
    if(ar_wave != vr_at16_from || VR_ARENA_Between()) { return; }
    float alive = VR_ARENA_CountAlive();
    float left = ar_plan_left + alive;
    if(vr_at16_state == 0) // (down to three, a foe at a time, the director's beat between blows)
    {
        if(time < vr_at16_at + 0.6) { return; }
        if(left > 3) { if(VR_AT_KillOne(ar_wave)) { vr_at16_at = time; } return; }
        VR_AT_Check(left == 3, sprintf("three of the wave left (%g)", left));
        VR_AT_Check(VR_AT16_Marks(FALSE), "with three left no foe is marked");
        VR_AT_Check(cvar_hget(cvarh_vr_arena_xray_shown) == 0, sprintf("with three left none is outlined (%g)",
                                                                   cvar_hget(cvarh_vr_arena_xray_shown)));
        VR_AT_KillOne(ar_wave);
        vr_at16_at = time;
        vr_at16_state = 1;
        return;
    }
    if(time < vr_at16_at + 0.6) { return; } // (a fall wakes the director at once; a frame drawn since)
    if(vr_at16_state == 1) // (two left: marked, and outlined beyond a short distance)
    {
        VR_AT_Check(left == 2, sprintf("two of the wave left (%g)", left));
        VR_AT_Check(VR_AT16_Marks(TRUE), "with two left every foe up is marked");
        cvar_set("vr_arena_xray_distance", "32");
        vr_at16_at = time;
        vr_at16_state = 2;
        return;
    }
    if(vr_at16_state == 2)
    {
        VR_AT_Check(cvar_hget(cvarh_vr_arena_xray_shown) == alive, sprintf("with two left both are outlined beyond 32 (%g of %g)",
                                                                       cvar_hget(cvarh_vr_arena_xray_shown), alive));
        cvar_set("vr_arena_xray_distance", "5000");
        vr_at16_at = time;
        vr_at16_state = 3;
        return;
    }
    if(vr_at16_state == 3)
    {
        VR_AT_Check(cvar_hget(cvarh_vr_arena_xray_shown) == 0, sprintf("none is outlined nearer than 5000 (%g)",
                                                               cvar_hget(cvarh_vr_arena_xray_shown)));
        cvar_set("vr_arena_xray_distance", "32");
        cvar_set("vr_arena_xray", "0");
        vr_at16_at = time;
        vr_at16_state = 4;
        return;
    }
    VR_AT_Check(VR_AT16_Marks(FALSE), "off: no foe is marked");
    VR_AT_Check(cvar_hget(cvarh_vr_arena_xray_shown) == 0, sprintf("off: none is outlined (%g)", cvar_hget(cvarh_vr_arena_xray_shown)));
    VR_AT_End();
}

// ---------------------------------------------------------------------------
// Test 17: the walls are flat (vrarena; the author, 2026-11-01 and 2026-11-02: players and foes walked up onto the
// skirting, and a player got up on the rail and climbed the trim above it). A player's box (hull 1) and an ogre's (hull
// 2) pushed flat into the middle of each of the 8 walls at heights from just off the floor to over the wall walk: each
// stops a full 16 out from the wall at every height (the clip's one face), whatever trim is behind it, so there is
// nothing to stand on. A shot's line still meets the trim itself: the rail, 8 out. One frame; no foes needed. (That no
// hold is left on the trim is vr_climb_try's: ROUND23.md.)
// ---------------------------------------------------------------------------
void(vector c, vector n, vector lo, vector hi, string what) VR_AT17_Wall =
{
    float half = hi_x * (fabs(n_x) + fabs(n_y)); // (the box's reach towards the wall)
    float b;
    float worst = 1000;
    float worstAt = 0;
    for(b = 0; b < 8; b++)
    {
        float bottom = 1;
        if(b == 1) { bottom = 17; }      // over the skirting
        if(b == 2) { bottom = 40; }
        if(b == 3) { bottom = 81; }      // over the wainscot
        if(b == 4) { bottom = 97; }      // over the rail (where the author stood)
        if(b == 5) { bottom = 100; }     // a sconce behind it (and an ogre still under a deck)
        if(b == 6) { bottom = 257; }     // on a deck's level, by an upright
        if(b == 7) { bottom = 288; }     // ... over a lamp's level
        vector at = c + '0 0 1' * (bottom - lo_z);
        tracebox(at + n * 128, lo, hi, at, TRUE, world);
        float gap = (trace_endpos - c) * n - half; // (how far out of the wall its face stopped)
        if(gap < worst) { worst = gap; worstAt = bottom; }
    }
    VR_AT_Check(worst >= 15.9, sprintf("%s: %s stops %g out of the wall (at %g up; want 16)", what, vtos(c), worst,
                                       worstAt));
}

void() VR_AT17_Run =
{
    if(!ar_mgr) { VR_AT_Check(FALSE, "test 17 needs the arena (vrarena)"); return; }
    float i;
    for(i = 0; i < 8; i++)
    {
        vector c = [800, 0, 0]; // (the middle of each wall, on the floor: its inward normal points at the centre)
        if(i == 1) { c = [700, 700, 0]; }
        if(i == 2) { c = [0, 800, 0]; }
        if(i == 3) { c = [-700, 700, 0]; }
        if(i == 4) { c = [-800, 0, 0]; }
        if(i == 5) { c = [-700, -700, 0]; }
        if(i == 6) { c = [0, -800, 0]; }
        if(i == 7) { c = [700, -700, 0]; }
        vector n = normalize('0 0 0' - c);
        VR_AT17_Wall(c, n, VEC_HULL_MIN, VEC_HULL_MAX, "a player");
        VR_AT17_Wall(c, n, VEC_HULL2_MIN, VEC_HULL2_MAX, "an ogre");
        traceline(c + n * 64 + '0 0 90', c + '0 0 90', TRUE, world);
        float seen = (trace_endpos - c) * n;
        VR_AT_Check(fabs(seen - 8) < 0.1, sprintf("a shot meets the rail itself at %s (%g out; want 8)", vtos(c), seen));
    }
}

// ---------------------------------------------------------------------------
// Test 18: a run starts with no ammunition (vrarena; the author, 2026-11-02): fists in both hands, none of any kind.
// Run it fresh, after `restart fresh` and after a load (the same checks): it waits only for the strip.
// ---------------------------------------------------------------------------
void() VR_AT18_Check =
{
    entity p = VR_Player();
    VR_AT_Check(p != world, "there is a player");
    if(!p) { return; }
    float total = p.ammo_shells + p.ammo_nails + p.ammo_rockets + p.ammo_cells + p.ammo_lava_nails +
                  p.ammo_multi_rockets + p.ammo_plasma;
    VR_AT_Check(total == 0, sprintf("no ammunition at the start (shells %g, nails %g, rockets %g, cells %g, others %g)",
                                    p.ammo_shells, p.ammo_nails, p.ammo_rockets, p.ammo_cells, p.ammo_lava_nails +
                                    p.ammo_multi_rockets + p.ammo_plasma));
    VR_AT_Check(p.currentammo == 0, "nothing in the hands' counter");
    VR_AT_Check(VRGetEntWeapon(p, cVR_MainHand) == WID_FIST && VRGetEntWeapon(p, cVR_OffHand) == WID_FIST,
                "fists in both hands");
}

void() VR_AT18_Frame =
{
    if(!ar_stripped) { return; } // (the run's first frame hasn't come yet)
    VR_AT18_Check();
    VR_AT_End();
}

// ---------------------------------------------------------------------------
// Test 19: the waves still arm an empty player (vrarena): the run starts with nothing, wave 1 comes by itself, its foes
// fall, and the first pack one leaves (its monster's own rounds, or an armed foe's spare ones), taken by a hand (its
// handler called), gives the player ammunition. Every foe is felled by the test the frame it stands; the player is in
// god mode for it.
// ---------------------------------------------------------------------------
void() VR_AT19_Begin =
{
    if(!ar_mgr) { VR_AT_Check(FALSE, "test 19 needs the arena (vrarena)"); VR_AT_End(); return; }
    vr_at_until = time + 120;
    vr_at_step = 0;
}

void() VR_AT19_Frame =
{
    entity p = VR_Player();
    entity e;
    if(vr_at_step == 0)
    {
        if(!ar_stripped || !p) { return; }
        p.flags |= FL_GODMODE;
        VR_AT18_Check();
        vr_at_step = 1;
        return;
    }
    if(ar_wave < 1) { return; }
    if(vr_at_step == 1)
    {
        VR_AT_Check(ar_wave == 1, "wave 1 comes by itself");
        vr_at_step = 2;
    }
    e = findflags(world, ar_slot, ar_wave);
    while(e)
    {
        if(e.health > 0) { T_Damage(e, p, p, e.health + 1); }
        e = findflags(e, ar_slot, ar_wave);
    }
    // (a worn pack whose wearer is dead, on the body: wave 1's foes wear them; a pack it didn't wear, on the floor)
    entity pack = find(world, classname, "vr_wornpack");
    while(pack && !(pack.vr_wp_state == VR_WP_DEAD && pack.ammo_shells + pack.ammo_nails + pack.ammo_rockets + pack.ammo_cells > 0))
    {
        pack = find(pack, classname, "vr_wornpack");
    }
    if(!pack)
    {
        pack = nextent(world);
        while(pack && pack.handtouch != BackpackTouch) { pack = nextent(pack); }
    }
    if(!pack) { return; }
    VR_AT_Check(p.ammo_shells + p.ammo_nails + p.ammo_rockets + p.ammo_cells == 0, "still none until a hand takes some");
    entity oself = self;
    entity oother = other;
    self = pack;
    other = p;
    BackpackTouch();
    self = oself;
    other = oother;
    VR_AT_Check(p.ammo_shells + p.ammo_nails + p.ammo_rockets + p.ammo_cells > 0,
                sprintf("wave %g: a fallen foe's pack gives ammunition (shells %g, nails %g, rockets %g, cells %g)", ar_wave,
                        p.ammo_shells, p.ammo_nails, p.ammo_rockets, p.ammo_cells));
    VR_AT_End();
}

// ---------------------------------------------------------------------------
// Test 30: Default as it is: every skill's first 30 plans hashed and one live wave's foes printed under a fixed seed
// ("arenaplan" and "arena: foe" lines, developer 1), to compare two builds: no preset chosen must print the same.
// ---------------------------------------------------------------------------

void() VR_AT30_Begin =
{
    cvar_set("vr_arena_seed", "4242");
    float sk;
    float w;
    float i;
    for(sk = 0; sk < 4; sk++)
    {
        VR_AT_SetSkill(sk);
        for(w = 1; w <= 30; w++)
        {
            VR_ARENA_Plan(w);
            float h = ar_plan_n;
            for(i = 0; i < ar_plan_n; i++)
            {
                h = VR_ARENA_Mod(h * 31 + ar_plan_kind[i], 1000003);
                h = VR_ARENA_Mod(h * 31 + ar_plan_gun[i] + 1, 1000003);
                h = VR_ARENA_Mod(h * 31 + ar_plan_tier[i], 1000003);
                h = VR_ARENA_Mod(h * 31 + ar_plan_boss[i], 1000003);
                h = VR_ARENA_Mod(h * 31 + ar_plan_armour[i], 1000003);
                h = VR_ARENA_Mod(h * 31 + ar_plan_amask[i], 1000003);
            }
            dprint(sprintf("arenaplan skill %g wave %g foes %g hash %g\n", sk, w, ar_plan_n, h));
        }
    }
    VR_AT_SetSkill(1);
    VR_ARENA_FastForward(6);
}

void() VR_AT30_Frame =
{
    if(ar_wave == 6 && ar_plan_n - ar_plan_left >= 3)
    {
        VR_AT_Check(TRUE, "wave 6's first foes in");
        VR_AT_End();
    }
}

// ---------------------------------------------------------------------------
// Tests 31..34: a preset's waves. Their presets: vr_arena_waves_dir arena_waves_test; vr_arena_waves_fixture, then
// vr_arena_play fixture_good (31, 32), fixture_bad (33); 34 in another map with a preset assigned.
// ---------------------------------------------------------------------------

void() VR_AT31_Run =
{
    VR_AT_Check(ar_preset_n == 3 && vr_ar_preset_skipped == 0 && vr_ar_preset_foes == 8,
        sprintf("the preset's waves %g, skipped %g, foes %g", ar_preset_n, vr_ar_preset_skipped, vr_ar_preset_foes));
    VR_ARENA_Plan(1);
    VR_AT_Check(ar_plan_n == 3 && ar_plan_kind[0] == AR_SP_GRUNT && ar_plan_kind[1] == AR_SP_GRUNT && ar_plan_kind[2] == AR_SP_ZOMBIE,
        sprintf("wave 1: two grunts and a zombie (%g foes)", ar_plan_n));
    VR_AT_Check(ar_plan_gun[0] == WID_NAILGUN && ar_plan_armour[0] == 1 && ar_plan_amask[0] == 3 && ar_plan_at[0] == 0 && ar_plan_at[2] == 1,
        sprintf("wave 1: a grunt's nailgun %g, green %g, chest and helmet %g; the zombie's delay %g", ar_plan_gun[0], ar_plan_armour[0], ar_plan_amask[0], ar_plan_at[2]));
    VR_ARENA_Plan(2);
    VR_AT_Check(ar_plan_n == 4 && ar_plan_kind[0] == AR_SP_ENFORCER && ar_plan_gun[0] == AR_FIST && ar_plan_armour[0] == 3 && ar_plan_amask[0] == 1023 &&
        ar_plan_infect[0] == VR_Infect_ByName("shambler"), sprintf("wave 2: the enforcer: fists %g, red %g, full %g, infected %g", ar_plan_gun[0], ar_plan_armour[0], ar_plan_amask[0], ar_plan_infect[0]));
    VR_AT_Check(ar_plan_kind[1] == AR_SP_PGRUNT && ar_plan_kind[2] == AR_SP_PGRUNT && ar_plan_armour[1] == 0, "wave 2: two of id's grunts");
    VR_AT_Check(ar_plan_kind[3] == AR_SP_AKNIGHT && ar_plan_gun[3] == WID_AXE && ar_plan_gun2[3] == WID_AXE && ar_plan_boss[3] == 1 && ar_plan_armour[3] == 2,
        sprintf("wave 2: the armed knight: axes %g %g, boss %g, yellow %g", ar_plan_gun[3], ar_plan_gun2[3], ar_plan_boss[3], ar_plan_armour[3]));
    VR_AT_Check(VR_ARENA_IsBossWave(2) && !VR_ARENA_IsBossWave(1) && !VR_ARENA_IsBossWave(3) && VR_ARENA_BossName(2) == "armed_knight", "the boss waves are the ones with a boss");
    VR_ARENA_Plan(3);
    VR_AT_Check(ar_plan_n == 1 && ar_plan_kind[0] == AR_SP_DOG, "wave 3: a dog");
    VR_ARENA_Plan(7);
    VR_AT_Check(ar_plan_n == 1 && ar_plan_kind[0] == AR_SP_DOG, "past its end: its last wave");
    VR_AT_Check(stof(arenawavesfield(1, -1, 1)) == 3, "wave 2's pool");
}

float at32_plan[7];
float at32_seen[7];
float at32_wave;
float at32_checked;
float at32_waves_done;
float at32_rose;
entity at32_riser;

// The waves' table's first row of a classname (id's grunt and the armed one are one class: their hands tell them apart).
float(string xClass) VR_AT32_ClassRow =
{
    float i;
    for(i = 0; i < VR_WAVE_KINDS_N; i++) { if(VR_WAVE_KINDS[i].classname == xClass) { return i; } }
    return -1;
}

void() VR_AT32_Begin =
{
    vr_at_until = time + 400;
    at32_wave = 0;
    at32_checked = 0;
    at32_waves_done = 0;
    at32_rose = 0;
    at32_riser = world;
    VR_ARENA_FastForward(1);
}

void() VR_AT32_Frame =
{
    float i;
    if(ar_preset_over)
    {
        VR_AT_Check(at32_waves_done == 3 && at32_rose == 1, sprintf("all three waves came, one riser (%g, %g); ARENA CLEARED", at32_waves_done, at32_rose));
        VR_AT_End();
        return;
    }
    if(ar_wave <= 0 || VR_ARENA_Between()) { return; }
    if(ar_wave != at32_wave)
    {
        at32_wave = ar_wave;
        at32_checked = 0;
        for(i = 0; i < 7; i++) { at32_plan[i] = 0; at32_seen[i] = 0; }
        for(i = 0; i < ar_plan_n; i++)
        {
            float armed = VR_ARENA_ArmedRow(ar_plan_kind[i]) >= 0;
            at32_plan[0] += VR_AT32_ClassRow(VR_ARENA_ClassName(ar_plan_kind[i])) + 1;
            at32_plan[1] += armed ? ar_plan_gun[i] + 2 : 0;
            at32_plan[2] += ar_plan_gun2[i] > 0;
            at32_plan[3] += ar_plan_armour[i];
            at32_plan[4] += ar_plan_amask[i];
            at32_plan[5] += ar_plan_infect[i];
            at32_plan[6] += ar_plan_boss[i] > 0;
        }
    }
    entity p = VR_Player();
    entity e = findflags(world, ar_slot, ar_wave);
    while(e)
    {
        entity next = findflags(e, ar_slot, ar_wave);
        if(e.ar_slot == ar_wave && e.health > 0 && e.takedamage)
        {
            if(e == at32_riser)
            {
                at32_riser = world;
                T_Damage(e, p, p, 100000);
                VR_AT_Check(e.ar_slot == -ar_wave, "the riser's fall is the kill");
            }
            else
            {
                at32_seen[0] += VR_AT32_ClassRow(e.classname) + 1;
                at32_seen[1] += e.vr_ag ? e.vr_ag_code + 2 : 0;
                at32_seen[2] += e.vr_ag_prop2 != world;
                at32_seen[3] += e.vr_ar_type;
                at32_seen[4] += e.vr_ar_mask;
                at32_seen[5] += e.vr_mg3_infected;
                at32_seen[6] += e.ar_tier >= AR_BOSS_MARK;
                if(e.classname == "monster_zombie")
                {
                    VR_AT_Check(time >= ar_wave_at + 1, sprintf("a delayed entry waits: in at %g s", time - ar_wave_at));
                }
                float kills = ar_wave_kills;
                float was = e.vr_mg3_infected;
                float tries;
                for(tries = 0; tries < 6 && e.health > 0 && e.vr_mg3_infected == was; tries++) { T_Damage(e, p, p, 100000); }
                if(was)
                {
                    at32_rose = at32_rose + 1;
                    at32_riser = e;
                    VR_AT_Check(e.health > 0 && e.classname == "monster_shambler" && e.ar_slot == ar_wave && ar_wave_kills == kills && !e.vr_ag,
                        sprintf("the infected enforcer rose a shambler, the same foe, no kill yet (%s health %g slot %g kills %g)", e.classname, e.health, e.ar_slot, ar_wave_kills));
                    VR_AT_Check(VR_ARENA_CountAlive() >= 1, "the wave waits for the riser");
                }
                else
                {
                    for(tries = 0; tries < 6 && e.health > 0; tries++) { T_Damage(e, p, p, 100000); }
                }
            }
        }
        e = next;
    }
    if(!at32_checked && ar_plan_left == 0 && VR_ARENA_CountAlive() == 0 && ar_wave_kills == ar_plan_n)
    {
        at32_checked = 1;
        at32_waves_done = at32_waves_done + 1;
        // (0 class, 1 main hand, 2 off hand, 3 armour, 4 pieces, 5 infection, 6 boss)
        for(i = 0; i < 7; i++)
        {
            VR_AT_Check(at32_plan[i] == at32_seen[i], sprintf("wave %g came as written: field %g: %g, written %g", ar_wave, i, at32_seen[i], at32_plan[i]));
        }
        VR_AT_Check(ar_wave_kills == ar_plan_n, sprintf("wave %g: %g kills, %g foes", ar_wave, ar_wave_kills, ar_plan_n));
    }
}

void() VR_AT33_Run =
{
    VR_AT_Check(ar_preset_n == 1 && vr_ar_preset_skipped == 6 && vr_ar_preset_foes == 1,
        sprintf("the bad preset still plays: waves %g, skipped %g, foes %g", ar_preset_n, vr_ar_preset_skipped, vr_ar_preset_foes));
    VR_ARENA_Plan(1);
    VR_AT_Check(ar_plan_n == 1 && ar_plan_kind[0] == AR_SP_DOG, sprintf("only what it can make: a dog (%g foes)", ar_plan_n));
}

void() VR_AT34_Run =
{
    VR_AT_Check(!isarenamap(mapname) && ar_mgr != world, sprintf("%s runs the arena: its name is assigned", mapname));
    VR_AT_Check(ar_preset_n == 1, sprintf("its preset loaded (%g waves)", ar_preset_n));
    VR_ARENA_Plan(1);
    VR_AT_Check(ar_plan_n == 3 && ar_plan_kind[2] == AR_SP_ZOMBIE, "its first wave as written");
}

// ---------------------------------------------------------------------------
// Test 35: generic infection: every outcome from every armed kind, with room, in a slot too narrow for a big body, and
// walled in (no room at all). The director held; each spawned, killed, looked at and removed in turn.
// ---------------------------------------------------------------------------

float at35_kind;
float at35_row;
float at35_room; // 0 open, 1 a slot 40 wide, 2 boxed in
float at35_state;
float at35_at;
float at35_kills;
float at35_drops;
entity at35_m;
entity at35_b1;
entity at35_b2;

entity(vector xLo, vector xHi) VR_AT35_Block =
{
    entity b = spawn();
    b.classname = "vr_at35_block";
    b.solid = SOLID_BBOX;
    b.movetype = MOVETYPE_NONE;
    setorigin(b, '0 0 0');
    setsize(b, xLo, xHi);
    return b;
}

void() VR_AT35_Spawn =
{
    entity p = VR_Player();
    makevectors([0, p.v_angle_y, 0]);
    entity oself = self;
    self = spawn();
    at35_m = self;
    setorigin(self, p.origin + v_forward * 200 + '0 0 8');
    self.angles = [0, p.v_angle_y + 180, 0];
    self.weapon = at35_kind < 2 ? WID_NAILGUN : WID_AXE;
    self.vr_mg3_infected = at35_row;
    VR_Infect_Precache(at35_row);
    if(at35_kind == VR_AM_GRUNT) { monster_vr_grunt(); }
    else if(at35_kind == VR_AM_ENFORCER) { monster_vr_enforcer(); }
    else if(at35_kind == VR_AM_KNIGHT) { monster_vr_knight(); }
    else { monster_vr_hknight(); }
    self = oself;
    at35_state = 1;
}

void() VR_AT35_Next =
{
    if(at35_m) { remove(at35_m); }
    if(at35_b1) { remove(at35_b1); }
    if(at35_b2) { remove(at35_b2); }
    at35_m = world;
    at35_b1 = world;
    at35_b2 = world;
    at35_room = at35_room + 1;
    if(at35_room > 2) { at35_room = 0; at35_row = at35_row + 1; }
    if(at35_row >= VR_INFECT_N) { at35_row = 1; at35_kind = at35_kind + 1; }
    at35_state = 0;
}

void() VR_AT35_Begin =
{
    vr_at_until = time + 600;
    ar_mgr.think = SUB_Null; // (no waves)
    ar_mgr.nextthink = 0;
    at35_kind = 0;
    at35_row = 1;
    at35_room = 0;
    at35_state = 0;
    at35_m = world;
}

void() VR_AT35_Frame =
{
    if(at35_kind >= VR_AM_COUNT)
    {
        VR_AT_End();
        return;
    }
    if(at35_state == 0) { VR_AT35_Spawn(); return; }
    entity m = at35_m;
    string what = sprintf("armed kind %g row %s room %g", at35_kind, VR_INFECT_ROWS[at35_row].name, at35_room);
    if(at35_state == 1)
    {
        if(!m.takedamage) { return; } // (its own start's think)
        VR_AgArmor_Give(m, 1, 1023); // (after its own start's roll for the skill)
        vector o = m.origin;
        if(at35_room == 1)
        {
            at35_b1 = VR_AT35_Block(o + '20 -400 -60', o + '400 400 300');
            at35_b2 = VR_AT35_Block(o + '-400 -400 -60', o + '-20 400 300');
        }
        else if(at35_room == 2)
        {
            at35_b1 = VR_AT35_Block(o + '-140 -140 -60', o + '140 140 300');
        }
        at35_kills = killed_monsters;
        at35_drops = vr_mg3_infected_drops;
        entity p = VR_Player();
        VR_AT_Check(m.vr_ag != 0 && m.vr_ar_type == 1, sprintf("%s: armed and armoured (armed %g, type %g)", what, m.vr_ag, m.vr_ar_type));
        T_Damage(m, p, p, 100000);
        float tries;
        for(tries = 0; tries < 6 && m.vr_mg3_infected; tries++) { T_Damage(m, p, p, 100000); }
        VR_AT_Check(vr_mg3_infected_drops == at35_drops + 1 && m.vr_ag == 0 && m.vr_ag_prop == world, sprintf("%s: his weapon fell, the armed layer gone", what));
        VR_AT_Check(m.vr_ar_type == 0 && m.vr_ar_mask == 0, sprintf("%s: his plates shed", what));
        if(at35_room == 2)
        {
            VR_AT_Check(m.health <= 0 && killed_monsters == at35_kills + 1, sprintf("%s: no room: he doesn't rise, counted (health %g, kills +%g)", what, m.health, killed_monsters - at35_kills));
            at35_m = m.classname == "" ? world : world; // (his body's the game's now: gibbed, it goes on its own)
            VR_AT35_Next();
            return;
        }
        string want = VR_INFECT_ROWS[at35_row].classname;
        if(at35_room == 1 && (want == "monster_ogre" || want == "monster_shambler" || want == "monster_shalrath")) { want = "monster_zombie"; }
        VR_AT_Check(m.classname == want && m.health > 0 && killed_monsters == at35_kills, sprintf("%s: rose as %s (%s, health %g, kills +%g)", what, want, m.classname, m.health, killed_monsters - at35_kills));
        at35_state = 2;
        at35_at = time + 0.2;
        return;
    }
    if(time < at35_at) { return; }
    // (the blow that burst him may knock the new body down, as any blow may: his ragdoll, then he gets up)
    VR_AT_Check(m.classname != "" && m.health > 0 && (m.solid == SOLID_SLIDEBOX || m.vr_knockdown) && m.takedamage && m.nextthink > time - 0.2, sprintf("%s: up and running a moment later (%s health %g solid %g dmg %g frame %g)", what, m.classname, m.health, m.solid, m.takedamage, m.frame));
    VR_AT35_Next();
}

// ---------------------------------------------------------------------------
// Test 36: how long the waves' walkers take between the floor and the dais (vrarena), and where they stall. One foe at a
// time, the player god-moded and standing: on the dais' pad with the foe on the floor ("up"), or on the floor with the
// foe on the dais' top across from him ("down"). The floor's places: 4 off the flights' lines, 4 in the corridors
// between the dais' diagonal faces and the towers. Each line: when its feet reached his level and when it got within a
// blow of him; its stalled time (under 8 units in a half-second sample) and its last stall's place. The sums per kind
// and direction close it. It fails if any foe never reached his level inside the cap, or never reached him. Known red: a
// knight and an ogre going down to slot 7 (him right beside a flight's side: off the flight, a foe can climb back on, as
// the straight line to him crosses its lower steps, and dither there; ROUND23.md).
// ---------------------------------------------------------------------------

float AT36_KINDS = 4;
float AT36_SLOTS = 8;
float AT36_CAP = 25;            // seconds: a foe not there by then is stuck
float AT36_REACH = 120;         // a blow's distance (id's melee range), on his level
float AT36_LEVEL = 20;          // feet this close to his are on his level (the pad's 16 included)
float at36_kind;
float at36_dir;                 // 0 up, 1 down
float at36_slot;
entity at36_m;
float at36_state;               // 0 place the next; 1 its own start; 2 walking
float at36_t0;
float at36_level;               // its time to his level (-1: not yet)
float at36_stall;
float at36_sample;              // next half-second sample's time
vector at36_was;
vector at36_stallat;
float at36_sum_level[8];        // per kind and direction: kind * 2 + dir
float at36_sum_arrive[8];
float at36_sum_stall[8];
float at36_capped[8];
float at36_nolevel[8];

float(float xKind) VR_AT36_Kind =
{
    if(xKind == 0) { return AR_SP_GRUNT; }
    if(xKind == 1) { return AR_SP_KNIGHT; }
    if(xKind == 2) { return AR_SP_OGRE; }
    return AR_SP_DOG;
}

// A floor place: the first 4 slots far off the flights' lines, the others in the diagonal corridors; a quarter turn each.
vector(float xSlot) VR_AT36_Floor =
{
    vector v = xSlot < 4 ? '600 200 0' : '230 230 0';
    float k = xSlot < 4 ? xSlot : xSlot - 4;
    if(k == 1) { v = [0 - v_y, v_x, 0]; }
    else if(k == 2) { v = [0 - v_x, 0 - v_y, 0]; }
    else if(k == 3) { v = [v_y, 0 - v_x, 0]; }
    return v;
}

string(vector xAt) VR_AT36_Where =
{
    float ax = fabs(xAt_x);
    float ay = fabs(xAt_y);
    float along = max(ax, ay);
    float across = min(ax, ay);
    if(xAt_z > 116) { return "top"; }
    if(xAt_z > 32) { return "stair"; }
    if(along < 440 && along > 200 && across < 90) { return "stair foot"; }
    if(ax + ay < 458 || along < 280) { return "dais side"; }
    return "floor";
}

void() VR_AT36_Begin =
{
    if(!ar_mgr) { VR_AT_Check(FALSE, "test 36 needs the arena on (map vrarena)"); VR_AT_End(); return; }
    vr_at_until = time + 2 * AT36_KINDS * AT36_SLOTS * (AT36_CAP + 1) + 60;
    ar_mgr.think = SUB_Null; // (no waves)
    ar_mgr.nextthink = 0;
    at36_kind = 0;
    at36_dir = 0;
    at36_slot = 0;
    at36_state = 0;
    at36_m = world;
    float i;
    for(i = 0; i < 8; i++)
    {
        at36_sum_level[i] = 0;
        at36_sum_arrive[i] = 0;
        at36_sum_stall[i] = 0;
        at36_capped[i] = 0;
        at36_nolevel[i] = 0;
    }
}

void() VR_AT36_Spawn =
{
    entity p = VR_Player();
    vector floorAt = VR_AT36_Floor(at36_slot);
    vector at;
    if(at36_dir == 0)
    {
        setorigin(p, '0 0 137');
        at = floorAt + '0 0 25';
    }
    else
    {
        setorigin(p, floorAt + '0 0 25');
        at = normalize(floorAt) * -130;
        at_z = 121;
    }
    p.velocity = '0 0 0';
    float kind = VR_AT36_Kind(at36_kind);
    entity oself = self;
    self = spawn();
    at36_m = self;
    self.classname = VR_ARENA_ClassName(kind);
    setorigin(self, at);
    self.angles = [0, vectoyaw(p.origin - at), 0];
    if(VR_ARENA_ArmsHumanoid(kind)) { self.weapon = AR_FIST; self.weapon2 = 0; }
    vr_ar_hold_roll = 1;
    vr_agr_hold = vr_agr_hold + 1;
    VR_ARENA_CallSpawn(kind);
    vr_agr_hold = vr_agr_hold - 1;
    vr_ar_hold_roll = 0;
    self = oself;
    at36_state = 1;
}

void(float xArrived) VR_AT36_Close =
{
    entity m = at36_m;
    float idx = at36_kind * 2 + at36_dir;
    float took = xArrived ? time - at36_t0 : AT36_CAP;
    float lev = at36_level >= 0 ? at36_level : AT36_CAP;
    at36_sum_level[idx] = at36_sum_level[idx] + lev;
    at36_sum_arrive[idx] = at36_sum_arrive[idx] + took;
    at36_sum_stall[idx] = at36_sum_stall[idx] + at36_stall;
    if(!xArrived) { at36_capped[idx] = at36_capped[idx] + 1; }
    if(at36_level < 0) { at36_nolevel[idx] = at36_nolevel[idx] + 1; }
    dprint(sprintf("arenapath: %s %s slot %g size %g: level %.1f there %.1f%s", m.classname, at36_dir ? "down" : "up", at36_slot,
                   m.maxs_x, lev, took, xArrived ? "" : " CAPPED"));
    dprint(sprintf(" stall %.1f last at %v (%s) ended %v\n", at36_stall, at36_stallat,
                   at36_stall > 0 ? VR_AT36_Where(at36_stallat) : "-", m.origin));
    remove(m);
    at36_m = world;
    at36_state = 0;
    at36_slot = at36_slot + 1;
    if(at36_slot >= AT36_SLOTS)
    {
        at36_slot = 0;
        at36_dir = at36_dir + 1;
        if(at36_dir >= 2)
        {
            at36_dir = 0;
            at36_kind = at36_kind + 1;
        }
    }
}

void() VR_AT36_Frame =
{
    if(at36_kind >= AT36_KINDS)
    {
        float i;
        float n = AT36_SLOTS;
        for(i = 0; i < 8; i++)
        {
            string name = VR_ARENA_ClassName(VR_AT36_Kind(floor(i / 2)));
            string way = (i & 1) ? "down" : "up";
            dprint(sprintf("arenapath: SUM %s %s: level %.1f there %.1f stall %.1f capped %g of %g\n", name, way,
                           at36_sum_level[i] / n, at36_sum_arrive[i] / n, at36_sum_stall[i] / n, at36_capped[i], n));
            VR_AT_Check(at36_nolevel[i] == 0, sprintf("%s %s: all reached his level inside %g s (%g did not)", name, way, AT36_CAP, at36_nolevel[i]));
            VR_AT_Check(at36_capped[i] == 0, sprintf("%s %s: all got to him inside %g s (%g did not)", name, way, AT36_CAP, at36_capped[i]));
        }
        VR_AT_End();
        return;
    }
    if(at36_state == 0) { VR_AT36_Spawn(); return; }
    entity m = at36_m;
    entity p = VR_Player();
    if(at36_state == 1)
    {
        if(!m.takedamage) { return; } // (its own start's think)
        m.enemy = p;
        m.think = FoundTarget;
        m.nextthink = time + 0.05;
        at36_t0 = time;
        at36_level = -1;
        at36_stall = 0;
        at36_stallat = '0 0 0';
        at36_sample = time + 0.5;
        at36_was = m.origin;
        at36_state = 2;
        return;
    }
    float onLevel = fabs((m.origin_z + m.mins_z) - (p.origin_z + p.mins_z)) <= AT36_LEVEL;
    if(onLevel && at36_level < 0) { at36_level = time - at36_t0; }
    vector d = m.origin - p.origin;
    d_z = 0;
    if(onLevel && vlen(d) < AT36_REACH) { VR_AT36_Close(TRUE); return; }
    if(time - at36_t0 >= AT36_CAP) { VR_AT36_Close(FALSE); return; }
    if(time >= at36_sample)
    {
        vector moved = m.origin - at36_was;
        moved_z = 0;
        if(vlen(moved) < 8)
        {
            at36_stall = at36_stall + 0.5;
            at36_stallat = m.origin;
        }
        at36_was = m.origin;
        at36_sample = time + 0.5;
    }
}

// ---------------------------------------------------------------------------
// Test 41: the corner towers (vrarena; NAVMESH.md 13). One foe at a time, the player god-moded and standing: "up" (him
// on a tower's top, the foe on the floor off the tower's far side, so its way is round the tower to the stair), "down"
// (the foe on the top, him on that floor place) and "across" (the foe on one tower's top, him on the next tower's,
// a quarter turn on: NE to NW and SW to SE go down a stair, over the floor and up the other; NW to SW and SE to NE go
// over a wall walk). Each of the 4 towers, each kind: an armed grunt with a shotgun and an enforcer with a rifle (their
// own range keeping and strafes: the case vr_arena_test 36 missed, its grunts at their fists), a knight, an ogre (the
// widest walker: 20 x 88 on 64-wide stairs), a dog. Done: its feet on his level and within a blow of him (a gun's
// holder: within 400, its fighting distance). It fails if any foe never got there inside the cap.
// ---------------------------------------------------------------------------

float AT41_KINDS = 5;
float AT41_DIRS = 3;
float AT41_TOWERS = 4;
float AT41_CAP = 40;            // seconds: across is a stair down, the floor and a stair up
float AT41_LEVEL = 20;          // feet this close to his are on his level
float at41_kind;
float at41_dir;                 // 0 up, 1 down, 2 across
float at41_tower;
entity at41_m;
float at41_state;               // 0 place the next; 1 its own start; 2 walking
float at41_t0;
float at41_level;
float at41_stall;
float at41_sample;
vector at41_was;
vector at41_stallat;
float at41_sum_level[15];       // per kind and direction: kind * 3 + dir
float at41_sum_arrive[15];
float at41_sum_stall[15];
float at41_capped[15];

float(float xKind) VR_AT41_Kind =
{
    if(xKind == 0) { return AR_SP_GRUNT; }
    if(xKind == 1) { return AR_SP_ENFORCER; }
    if(xKind == 2) { return AR_SP_KNIGHT; }
    if(xKind == 3) { return AR_SP_OGRE; }
    return AR_SP_DOG;
}

// `xAt` turned a quarter turn `xTower` times (NE, NW, SW, SE: the towers and their pinwheel stairs turn so).
vector(vector xAt, float xTower) VR_AT41_Turn =
{
    float k = xTower - 4 * floor(xTower / 4);
    if(k == 1) { return [0 - xAt_y, xAt_x, xAt_z]; }
    if(k == 2) { return [0 - xAt_x, 0 - xAt_y, xAt_z]; }
    if(k == 3) { return [xAt_y, 0 - xAt_x, xAt_z]; }
    return xAt;
}

vector(float xTower) VR_AT41_Top = { return VR_AT41_Turn('402 402 281', xTower); }
vector(float xTower) VR_AT41_Floor = { return VR_AT41_Turn('620 260 25', xTower); } // (the NE tower's: past its east face)

void() VR_AT41_Begin =
{
    if(!ar_mgr) { VR_AT_Check(FALSE, "test 41 needs the arena on (map vrarena)"); VR_AT_End(); return; }
    vr_at_until = time + AT41_KINDS * AT41_DIRS * AT41_TOWERS * (AT41_CAP + 1) + 60;
    ar_mgr.think = SUB_Null; // (no waves)
    ar_mgr.nextthink = 0;
    at41_kind = 0;
    at41_dir = 0;
    at41_tower = 0;
    at41_state = 0;
    at41_m = world;
    float i;
    for(i = 0; i < 15; i++)
    {
        at41_sum_level[i] = 0;
        at41_sum_arrive[i] = 0;
        at41_sum_stall[i] = 0;
        at41_capped[i] = 0;
    }
}

void() VR_AT41_Spawn =
{
    entity p = VR_Player();
    vector at;
    if(at41_dir == 0) { setorigin(p, VR_AT41_Top(at41_tower)); at = VR_AT41_Floor(at41_tower); }
    else if(at41_dir == 1) { setorigin(p, VR_AT41_Floor(at41_tower)); at = VR_AT41_Top(at41_tower); }
    else { setorigin(p, VR_AT41_Top(at41_tower + 1)); at = VR_AT41_Top(at41_tower); }
    p.velocity = '0 0 0';
    float kind = VR_AT41_Kind(at41_kind);
    entity oself = self;
    self = spawn();
    at41_m = self;
    self.classname = VR_ARENA_ClassName(kind);
    setorigin(self, at);
    self.angles = [0, vectoyaw(p.origin - at), 0];
    if(kind == AR_SP_GRUNT) { self.weapon = WID_SHOTGUN; self.weapon2 = 0; }
    else if(kind == AR_SP_ENFORCER) { self.weapon = WID_ENFRIFLE; self.weapon2 = 0; }
    vr_ar_hold_roll = 1;
    vr_agr_hold = vr_agr_hold + 1;
    VR_ARENA_CallSpawn(kind);
    vr_agr_hold = vr_agr_hold - 1;
    vr_ar_hold_roll = 0;
    self = oself;
    at41_state = 1;
}

void(float xArrived) VR_AT41_Close =
{
    entity m = at41_m;
    float idx = at41_kind * 3 + at41_dir;
    float took = xArrived ? time - at41_t0 : AT41_CAP;
    float lev = at41_level >= 0 ? at41_level : AT41_CAP;
    at41_sum_level[idx] = at41_sum_level[idx] + lev;
    at41_sum_arrive[idx] = at41_sum_arrive[idx] + took;
    at41_sum_stall[idx] = at41_sum_stall[idx] + at41_stall;
    if(!xArrived) { at41_capped[idx] = at41_capped[idx] + 1; }
    string way = at41_dir == 0 ? "up" : (at41_dir == 1 ? "down" : "across");
    dprint(sprintf("towerpath: %s %s tower %g size %g: level %.1f there %.1f%s", m.classname, way, at41_tower, m.maxs_x, lev,
                   took, xArrived ? "" : " CAPPED"));
    dprint(sprintf(" stall %.1f last at %v ended %v\n", at41_stall, at41_stallat, m.origin));
    remove(m);
    at41_m = world;
    at41_state = 0;
    at41_tower = at41_tower + 1;
    if(at41_tower >= AT41_TOWERS)
    {
        at41_tower = 0;
        at41_dir = at41_dir + 1;
        if(at41_dir >= AT41_DIRS)
        {
            at41_dir = 0;
            at41_kind = at41_kind + 1;
        }
    }
}

void() VR_AT41_Frame =
{
    if(at41_kind >= AT41_KINDS)
    {
        float i;
        float n = AT41_TOWERS;
        float capped = 0;
        for(i = 0; i < 15; i++)
        {
            string name = VR_ARENA_ClassName(VR_AT41_Kind(floor(i / 3)));
            float dir = i - 3 * floor(i / 3);
            string way = dir == 0 ? "up" : (dir == 1 ? "down" : "across");
            capped = capped + at41_capped[i];
            dprint(sprintf("towerpath: SUM %s %s: level %.1f there %.1f stall %.1f capped %g of %g\n", name, way,
                           at41_sum_level[i] / n, at41_sum_arrive[i] / n, at41_sum_stall[i] / n, at41_capped[i], n));
            VR_AT_Check(at41_capped[i] == 0, sprintf("%s %s: all got to him inside %g s (%g did not)", name, way, AT41_CAP, at41_capped[i]));
        }
        dprint(sprintf("towerpath: capped %g of %g\n", capped, AT41_KINDS * AT41_DIRS * AT41_TOWERS));
        VR_AT_End();
        return;
    }
    if(at41_state == 0) { VR_AT41_Spawn(); return; }
    entity m = at41_m;
    entity p = VR_Player();
    if(at41_state == 1)
    {
        if(!m.takedamage) { return; } // (its own start's think)
        m.enemy = p;
        m.think = FoundTarget;
        m.nextthink = time + 0.05;
        at41_t0 = time;
        at41_level = -1;
        at41_stall = 0;
        at41_stallat = '0 0 0';
        at41_sample = time + 0.5;
        at41_was = m.origin;
        at41_state = 2;
        return;
    }
    float onLevel = fabs((m.origin_z + m.mins_z) - (p.origin_z + p.mins_z)) <= AT41_LEVEL;
    if(onLevel && at41_level < 0) { at41_level = time - at41_t0; }
    vector d = m.origin - p.origin;
    d_z = 0;
    float reach = (m.vr_ag && VR_WP_CarriesRanged(m)) ? 400 : 120;
    if(onLevel && vlen(d) < reach) { VR_AT41_Close(TRUE); return; }
    if(time - at41_t0 >= AT41_CAP) { VR_AT41_Close(FALSE); return; }
    if(time >= at41_sample)
    {
        vector moved = m.origin - at41_was;
        moved_z = 0;
        if(vlen(moved) < 8)
        {
            at41_stall = at41_stall + 0.5;
            at41_stallat = m.origin;
        }
        at41_was = m.origin;
        at41_sample = time + 0.5;
    }
}

// The toolgun's spawn with outcome `xRow` (vr_toolgun_armed_infected_test, vr_toolgun.cpp): it carries it, and rises as it.
float(entity e, float xRow) VR_Infect_TestOne =
{
    if(e.vr_mg3_infected != xRow) { return 0; }
    string want = VR_INFECT_ROWS[xRow].classname;
    e.takedamage = DAMAGE_AIM; // (its own start's think hasn't come: this is its one frame)
    float tries;
    for(tries = 0; tries < 6 && e.vr_mg3_infected; tries++) { T_Damage(e, world, world, 100000); }
    float ok = e.classname == want && e.health > 0;
    if(!ok) { dprint(sprintf("infect test: row %g: %s health %g\n", xRow, e.classname, e.health)); }
    remove(e);
    return ok;
}

// ---------------------------------------------------------------------------
// Test 37: the weapons that appear by ammo boxes (items.qc SpawnAmmoBoxWeaponsImpl): the nail pistol on nail boxes (any
// player), the heavy revolver on shell boxes (only with the super shotgun or a revolver). Any open map off the arena
// (e1m1). Each row rolls AT37_BOXES real boxes through SpawnAmmoBoxWeaponsImpl and counts what lies above them.
// ---------------------------------------------------------------------------

float AT37_BOXES = 6000;
float AT37_BATCH = 500;
float AT37_ROWS = 8;
float AT37_TOL = 0.009;     // absolute: ~3 sigma at the largest expected rate
float at37_row;
float at37_done;            // boxes rolled in this row
float at37_np;              // nail pistols seen in this row
float at37_rv;              // heavy revolvers seen
float at37_save_mode;
float at37_had;             // the player's weapons before (bits: ssg, rev, ng, sng)
entity at37_box;
float at37_exp_np;
float at37_exp_rv;
string at37_name;

void(entity p, float ssg, float rev, float ng, float sng) VR_AT37_Hold =
{
    if(ssg) { VR_WeaponUtil_EntAddWeapon(p, WID_SUPER_SHOTGUN); } else { VR_WeaponUtil_EntDelWeapon(p, WID_SUPER_SHOTGUN); }
    if(rev) { VR_WeaponUtil_EntAddWeapon(p, WID_REVOLVER); } else { VR_WeaponUtil_EntDelWeapon(p, WID_REVOLVER); }
    if(ng)  { VR_WeaponUtil_EntAddWeapon(p, WID_NAILGUN); } else { VR_WeaponUtil_EntDelWeapon(p, WID_NAILGUN); }
    if(sng) { VR_WeaponUtil_EntAddWeapon(p, WID_SUPER_NAILGUN); } else { VR_WeaponUtil_EntDelWeapon(p, WID_SUPER_NAILGUN); }
}

// A row: the box (nails or shells), the guns held, the drop mode, the rates it must show (the expected shares of boxes).
void(entity p) VR_AT37_Row =
{
    float mode = QVR_AMMOBOXDROPS_WHENELIGIBLE;
    string cls = ITEM_NAME_SPIKES;
    at37_exp_np = 0;
    at37_exp_rv = 0;
    if(at37_row == 0) { at37_name = "nail box, no nail weapon"; VR_AT37_Hold(p, 0, 0, 0, 0); at37_exp_np = 0.054; }
    if(at37_row == 1) { at37_name = "nail box, nailgun"; VR_AT37_Hold(p, 0, 0, 1, 0); at37_exp_np = 0.0324; }
    if(at37_row == 2) { at37_name = "nail box, nailgun and super nailgun"; VR_AT37_Hold(p, 0, 0, 1, 1); at37_exp_np = 0.0162; }
    if(at37_row == 3) { at37_name = "shell box, no super shotgun, no revolver"; cls = ITEM_NAME_SHELLS; VR_AT37_Hold(p, 0, 0, 0, 0); }
    if(at37_row == 4) { at37_name = "shell box, super shotgun"; cls = ITEM_NAME_SHELLS; VR_AT37_Hold(p, 1, 0, 0, 0); at37_exp_rv = 0.036; }
    if(at37_row == 5) { at37_name = "shell box, a revolver only"; cls = ITEM_NAME_SHELLS; VR_AT37_Hold(p, 0, 1, 0, 0); at37_exp_rv = 0.036; }
    if(at37_row == 6) { at37_name = "nail box, drops disabled"; mode = QVR_AMMOBOXDROPS_DISABLE; VR_AT37_Hold(p, 1, 1, 0, 0); }
    if(at37_row == 7) { at37_name = "shell box, always, nothing held"; cls = ITEM_NAME_SHELLS; mode = QVR_AMMOBOXDROPS_ALWAYS; VR_AT37_Hold(p, 0, 0, 0, 0); at37_exp_rv = 0.036; }
    cvar_set("vr_ammobox_drops", ftos(mode));
    at37_done = 0;
    at37_np = 0;
    at37_rv = 0;
    at37_box.classname = cls;
}

// ---------------------------------------------------------------------------
// Test 38: the arena map check, and the banners following a scripted wave (vr_arena_banners.qc). On an arena map: the
// map's boards hold the very text expected at each step (read back from the server's list), and the image drawn from that
// text on the client (panelboard_read: it answers once the text has come through the client's list and been drawn) changes
// in the tile that moved and only there. Off the arena (e1m1): the check refuses it, a banner placed there makes no board and
// the map's list of texts stays empty.
// ---------------------------------------------------------------------------

float vr_at38_state;
float vr_at38_at;
float vr_at38_wave;
float vr_at38_left;
float vr_at38_score;
float vr_at38_secs;
float vr_at38_hwave;   // the tiles' checksums at the last step
float vr_at38_hmid;
float vr_at38_htime;
float vr_at38_hscore;
float vr_at38_bar;

// A board's text, each of them holding the same.
string() VR_AT38_Board =
{
    entity e = find(world, classname, "info_arena_banner");
    if(!e) { return "(no board)"; }
    string text = worldtext_hgettext(e.ar_banner_h);
    entity o = find(e, classname, "info_arena_banner");
    while(o)
    {
        if(worldtext_hgettext(o.ar_banner_h) != text) { return "(boards differ)"; }
        o = find(o, classname, "info_arena_banner");
    }
    return text;
}

// The text expected.
string(float xWave, float xBoss, float xShow, float xValue, float xSecs, float xScore) VR_AT38_Text =
{
    return sprintf("{arena:%.0f %.0f %.0f %.0f %.0f %.0f %.0f}", xWave, xBoss, xShow, xValue, ar_plan_n, xSecs, xScore);
}

// The text expected in a wave's fight.
string(float xBoss, float xWave, float xLeft, float xSecs, float xScore) VR_AT38_Fight =
{
    return VR_AT38_Text(xWave, xBoss, AR_BANNER_SHOW_FOES, xLeft, xSecs, xScore);
}

// The image's part (vr_gadget_panel.inc's tiles): its checksum or ink, from what the board says now.
float(float xX, float xY, float xW, float xH, float xWhat) VR_AT38_Read =
{
    return panelboard_read(VR_AT38_Board(), xX, xY, xW, xH, xWhat);
}
float() VR_AT38_Wave  = { return VR_AT38_Read(4, 2, 48, 34, 1); }
float() VR_AT38_Mid   = { return VR_AT38_Read(58, 2, 72, 37, 1); }
float() VR_AT38_Time  = { return VR_AT38_Read(137, 3, 109, 16, 1); }
float() VR_AT38_Score = { return VR_AT38_Read(137, 22, 109, 16, 1); }
float() VR_AT38_Bar   = { return VR_AT38_Read(58, 36, 72, 2, 0); }

// The tiles as they are now, for the next step's compare.
void() VR_AT38_Keep =
{
    vr_at38_hwave = VR_AT38_Wave();
    vr_at38_hmid = VR_AT38_Mid();
    vr_at38_htime = VR_AT38_Time();
    vr_at38_hscore = VR_AT38_Score();
}

void() VR_AT38_Begin =
{
    vr_test_flags = VR_TEST_LOUD | VR_TEST_QUIET; // (broadcast: the verdict is seen with developer 0, for -QuitOn)
    VR_AT_Check(isarenamap("vrarena"), "isarenamap takes vrarena");
    VR_AT_Check(!isarenamap("e1m1"), "isarenamap refuses e1m1");
    VR_AT_Check(!isarenamap(""), "isarenamap refuses no name");

    if(mapname == "e1m1")
    {
        VR_AT_Check(!ar_mgr, "no arena runs in e1m1");
        VR_AT_Check(!VR_ARENA_On(), "VR_ARENA_On refuses e1m1");
        entity oself = self;
        self = spawn();
        self.classname = "info_arena_banner";
        info_arena_banner();
        self = oself;
        VR_AT_Check(ar_banner_n == 0, sprintf("a banner placed off the arena is no board (%g)", ar_banner_n));
        VR_AT_Check(!find(world, classname, "info_arena_banner"), "and it is gone");
        float next = worldtext_hmake();
        VR_AT_Check(next == 0, sprintf("the map's list of texts is empty: nothing to draw (next handle %g)", next));
        VR_AT_End();
        return;
    }

    if(!ar_mgr) { VR_AT_Check(FALSE, "test 38 needs an arena map"); VR_AT_End(); return; }
    VR_AT_Check(isarenamap(mapname), sprintf("%s is an arena map", mapname));
    VR_AT_Check(VR_ARENA_On(), "VR_ARENA_On takes it");
    VR_AT_Check(ar_banner_n >= 1, sprintf("the map places banners (%g)", ar_banner_n));
    cvar_set("vr_arena_alive_max", "8");
    cvar_set("vr_arena_burst", "8");
    vr_at38_state = 0;
    vr_at38_at = time;
    vr_at_until = time + 120;
}

void() VR_AT38_Frame =
{
    string want;
    // Every step's check is of a board whose image has caught up (the client has its text and has drawn it): but off, where
    // there is none.
    if(vr_at38_state != 5 && (ar_bn_on == 0 || VR_AT38_Read(0, 0, 1, 1, 0) < 0)) { return; }

    if(vr_at38_state == 0) // (the board before the run: the wave's still to come, the clock not started)
    {
        if(!VR_ARENA_Between()) { return; }
        float next = VR_ARENA_BannerValue(AR_BANNER_SHOW_NEXT);
        want = VR_AT38_Text(0, 0, AR_BANNER_SHOW_NEXT, next, 0, ar_score);
        VR_AT_Check(VR_AT38_Board() == want, sprintf("before the run: [%s], wanted [%s]", VR_AT38_Board(), want));
        VR_AT_Check(next >= 1, sprintf("the next wave's seconds (%g)", next));
        VR_AT_Check(VR_AT38_Read(0, 0, 250, 1, 0) > 0.95, sprintf("pixels: the frame is lit (%g)", VR_AT38_Read(0, 0, 250, 1, 0)));
        VR_AT_Check(VR_AT38_Read(4, 11, 48, 24, 0) > 0.01, sprintf("pixels: the wave's tile has its dashes (%g)", VR_AT38_Read(4, 11, 48, 24, 0)));
        VR_AT_Check(VR_AT38_Read(58, 11, 72, 24, 0) > 0.05, sprintf("pixels: the middle has its seconds (%g)", VR_AT38_Read(58, 11, 72, 24, 0)));
        VR_AT_Check(VR_AT38_Read(56, 3, 2, 34, 0) == 0, sprintf("pixels: nothing runs over between the tiles (%g)", VR_AT38_Read(56, 3, 2, 34, 0)));
        VR_AT_Check(VR_AT38_Bar() == 0, sprintf("pixels: no foes' bar between waves (%g)", VR_AT38_Bar()));
        VR_AT38_Keep();
        vr_at38_left = next;
        vr_at38_state = 1;
        return;
    }
    if(vr_at38_state == 1) // (the countdown falls)
    {
        if(VR_ARENA_BannerValue(AR_BANNER_SHOW_NEXT) >= vr_at38_left) { return; }
        want = VR_AT38_Text(0, 0, AR_BANNER_SHOW_NEXT, vr_at38_left - 1, 0, ar_score);
        VR_AT_Check(VR_AT38_Board() == want, sprintf("the countdown falls: [%s]", VR_AT38_Board()));
        VR_AT_Check(VR_AT38_Mid() != vr_at38_hmid, "pixels: the middle changes with the seconds");
        VR_AT_Check(VR_AT38_Wave() == vr_at38_hwave, "pixels: the wave's tile doesn't");
        VR_AT_Check(VR_AT38_Score() == vr_at38_hscore, "pixels: nor the score's");
        // The perk's choice: the director held, as a boss clear's cards hold it (the cards themselves: vr_arena_perks_test).
        ark_open = 1;
        ark_open_at = time;
        VR_ARK_Hold();
        vr_at38_state = 8;
        return;
    }
    if(vr_at38_state == 8) // (CHOOSE A PERK, and back)
    {
        if(VR_AT38_Mid() == vr_at38_hmid) { return; } // (its seconds: until the middle has moved on from them)
        VR_AT_Check(VR_ARENA_BannerShow() == AR_BANNER_SHOW_PERK, "the cards up: the board says so");
        VR_AT_Check(VR_AT38_Board() == VR_AT38_Text(0, 0, AR_BANNER_SHOW_PERK, 0, 0, ar_score), sprintf("perk: [%s]", VR_AT38_Board()));
        VR_AT_Check(VR_AT38_Read(58, 11, 72, 24, 0) > 0.05, sprintf("pixels: the middle has its word (%g)", VR_AT38_Read(58, 11, 72, 24, 0)));
        VR_ARK_CloseOffer();
        VR_ARENA_FastForward(6); // (a plain wave, with a few foes)
        vr_at38_wave = 6;
        vr_at38_state = 2;
        return;
    }
    if(ar_wave != vr_at38_wave || VR_ARENA_Between()) { return; }
    if(vr_at38_state == 2) // (the wave is on: it is all up there, the clock started with it)
    {
        if(VR_ARENA_CountAlive() < 1) { return; }
        VR_AT_Check(ar_foes_left == ar_plan_n, sprintf("all its foes are left (%g of %g)", ar_foes_left, ar_plan_n));
        want = VR_AT38_Fight(0, ar_wave, ar_plan_n, VR_ARENA_BannerSeconds(), ar_score);
        VR_AT_Check(VR_AT38_Board() == want, sprintf("wave on: [%s], wanted [%s]", VR_AT38_Board(), want));
        VR_AT_Check(VR_AT38_Wave() != vr_at38_hwave, "pixels: the wave's tile has its number");
        vr_at38_bar = VR_AT38_Bar();
        VR_AT_Check(vr_at38_bar > 0.95, sprintf("pixels: the foes' bar is full (%g)", vr_at38_bar));
        VR_AT38_Keep();
        vr_at38_left = ar_plan_n;
        vr_at38_score = ar_score;
        vr_at38_secs = VR_ARENA_BannerSeconds();
        VR_AT_KillOne(ar_wave);
        vr_at38_at = time;
        vr_at38_state = 3;
        return;
    }
    if(vr_at38_state == 3) // (a foe down: within the director's beat, the board says so)
    {
        if(time < vr_at38_at + 0.2) { return; } // (a fall wakes the director within 0.05 s)
        VR_AT_Check(ar_foes_left == vr_at38_left - 1, sprintf("foes left falls (%g from %g)", ar_foes_left, vr_at38_left));
        VR_AT_Check(ar_score > vr_at38_score, sprintf("the score rises (%g from %g)", ar_score, vr_at38_score));
        want = VR_AT38_Fight(0, ar_wave, vr_at38_left - 1, VR_ARENA_BannerSeconds(), ar_score);
        VR_AT_Check(VR_AT38_Board() == want, sprintf("a foe down: [%s], wanted [%s]", VR_AT38_Board(), want));
        VR_AT_Check(VR_AT38_Mid() != vr_at38_hmid, "pixels: the foes' number changes");
        VR_AT_Check(VR_AT38_Score() != vr_at38_hscore, "pixels: the score's does");
        VR_AT_Check(VR_AT38_Wave() == vr_at38_hwave, "pixels: the wave's tile doesn't");
        VR_AT_Check(VR_AT38_Bar() < vr_at38_bar && VR_AT38_Bar() > 0, sprintf("pixels: the bar falls (%g from %g)", VR_AT38_Bar(), vr_at38_bar));
        VR_AT38_Keep();
        vr_at38_at = time;
        vr_at38_state = 4;
        return;
    }
    if(vr_at38_state == 4) // (the time runs on, two whole seconds)
    {
        if(VR_ARENA_BannerSeconds() < vr_at38_secs + 2) { return; }
        want = VR_AT38_Fight(0, ar_wave, ar_foes_left, VR_ARENA_BannerSeconds(), ar_score);
        VR_AT_Check(VR_AT38_Board() == want, sprintf("time runs: [%s], wanted [%s]", VR_AT38_Board(), want));
        VR_AT_Check(VR_AT38_Time() != vr_at38_htime, "pixels: the time's row changes");
        VR_AT_Check(VR_AT38_Wave() == vr_at38_hwave, "pixels: the wave's tile doesn't");
        VR_AT_Check(VR_AT38_Score() == vr_at38_hscore, "pixels: nor the score's");
        cvar_set("vr_arena_banners", "0");
        vr_at38_state = 5;
        return;
    }
    if(vr_at38_state == 5) // (off: the boards are empty, and the image is let go; on: back at once)
    {
        if(ar_bn_on != 1 || panelboard_read("", 0, 0, 0, 0, 2) != 0) { return; }
        VR_AT_Check(VR_AT38_Board() == "", sprintf("off: the boards are empty: [%s]", VR_AT38_Board()));
        VR_AT_Check(panelboard_read("", 0, 0, 0, 0, 2) == 0, "off: no image kept");
        cvar_set("vr_arena_banners", "1");
        vr_at38_state = 6;
        return;
    }
    if(vr_at38_state == 6)
    {
        want = VR_AT38_Fight(0, ar_wave, ar_foes_left, VR_ARENA_BannerSeconds(), ar_score);
        VR_AT_Check(VR_AT38_Board() == want, sprintf("on again: [%s], wanted [%s]", VR_AT38_Board(), want));
        VR_AT_Check(VR_AT38_Read(4, 11, 48, 24, 0) > 0.05, "pixels: drawn again");
        VR_AT38_Keep();
        vr_at38_wave = ar_wave + 1;
        VR_ARENA_FastForward(vr_at38_wave); // (the wave changes)
        vr_at38_state = 7;
        return;
    }
    if(vr_at38_state == 7)
    {
        if(time < ar_wave_at + 0.1) { return; }
        want = VR_AT38_Fight(VR_ARENA_IsBossWave(ar_wave) ? 1 : 0, vr_at38_wave, ar_plan_n, VR_ARENA_BannerSeconds(), ar_score);
        VR_AT_Check(VR_AT38_Board() == want, sprintf("the wave changes: [%s], wanted [%s]", VR_AT38_Board(), want));
        VR_AT_Check(VR_AT38_Wave() != vr_at38_hwave, "pixels: the wave's tile changes");
        VR_AT38_Keep();
        ar_down = 1; // (the run's end, the board's part: the flag it reads)
        vr_at38_state = 9;
        return;
    }
    if(vr_at38_state == 9) // (YOU FELL: over the wave's and the foes' tiles, in the warning's red)
    {
        if(VR_ARENA_BannerShow() != AR_BANNER_SHOW_FELL || VR_AT38_Mid() == vr_at38_hmid) { return; }
        VR_AT_Check(VR_AT38_Read(4, 11, 126, 24, 3) > 0.15, sprintf("pixels: YOU FELL is red (%g)", VR_AT38_Read(4, 11, 126, 24, 3)));
        VR_AT_Check(VR_AT38_Read(0, 0, 250, 1, 3) > 0.15, sprintf("pixels: and the frame (%g)", VR_AT38_Read(0, 0, 250, 1, 3)));
        VR_AT_Check(ar_run_end > 0, "the clock stopped");
        VR_AT38_Keep();
        ar_down = 0;
        ar_preset_over = 1;
        vr_at38_state = 10;
        return;
    }
    if(vr_at38_state == 10) // (CLEARED)
    {
        if(VR_ARENA_BannerShow() != AR_BANNER_SHOW_CLEAR || VR_AT38_Mid() == vr_at38_hmid) { return; }
        VR_AT_Check(VR_AT38_Read(4, 11, 126, 24, 0) > 0.05, sprintf("pixels: CLEARED is up (%g)", VR_AT38_Read(4, 11, 126, 24, 0)));
        VR_AT_Check(VR_AT38_Read(4, 11, 126, 24, 3) < 0.15, sprintf("pixels: and not red (%g)", VR_AT38_Read(4, 11, 126, 24, 3)));
        ar_preset_over = 0;
        VR_AT_End();
    }
}

void() VR_AT37_Begin =
{
    entity p = VR_PlayerAlive();
    if(!p) { VR_AT_Check(FALSE, "test 37 needs a player"); VR_AT_End(); return; }
    if(VR_ARK_Running()) { VR_AT_Check(FALSE, "test 37 runs off the arena (map e1m1)"); VR_AT_End(); return; }
    vr_test_flags = VR_TEST_LOUD | VR_TEST_QUIET; // (broadcast: the verdict is seen with developer 0, for -QuitOn)
    at37_save_mode = cvar_hget(cvarh_vr_ammobox_drops);
    VR_AT_Check(cvar_hget(cvarh_vr_ammobox_drops_chance_mult) == 1, "the ammo box chance multiplier is at its default 1 (a tunable)");
    at37_had = VR_WeaponUtil_EntHasWeapon(p, WID_SUPER_SHOTGUN) + 2 * VR_WeaponUtil_EntHasWeapon(p, WID_REVOLVER) +
               4 * VR_WeaponUtil_EntHasWeapon(p, WID_NAILGUN) + 8 * VR_WeaponUtil_EntHasWeapon(p, WID_SUPER_NAILGUN);
    at37_box = spawn();
    setorigin(at37_box, p.origin + '0 0 4096');
    at37_row = 0;
    vr_at_until = time + 120;
    VR_AT37_Row(p);
}

void() VR_AT37_Finish =
{
    entity p = VR_PlayerAlive();
    if(p)
    {
        VR_AT37_Hold(p, at37_had & 1, (at37_had & 2) != 0, (at37_had & 4) != 0, (at37_had & 8) != 0);
    }
    remove(at37_box);
    cvar_set("vr_ammobox_drops", ftos(at37_save_mode));
    VR_AT_End();
}

void() VR_AT37_Frame =
{
    entity p = VR_PlayerAlive();
    if(!p) { VR_AT_Check(FALSE, "test 37 lost its player"); VR_AT37_Finish(); return; }

    float i;
    for(i = 0; i < AT37_BATCH; i++) { SpawnAmmoBoxWeaponsImpl(at37_box, p); }
    at37_done = at37_done + AT37_BATCH;

    // What lies far above the player is this test's: counted and taken back.
    entity e = find(world, classname, "thrown_weapon");
    while(e)
    {
        entity nx = find(e, classname, "thrown_weapon");
        if(e.origin_z > p.origin_z + 3000)
        {
            if(e.weapon == WID_NAILPISTOL) { at37_np = at37_np + 1; }
            if(e.weapon == WID_REVOLVER)   { at37_rv = at37_rv + 1; }
            remove(e);
        }
        e = nx;
    }

    if(at37_done < AT37_BOXES) { return; }

    bprint(sprintf("arena37: row %g (%s): %g boxes, %g nail pistols, %g revolvers\n", at37_row, at37_name, at37_done, at37_np, at37_rv));
    float rnp = at37_np / at37_done;
    float rrv = at37_rv / at37_done;
    if(at37_exp_np == 0) { VR_AT_Check(at37_np == 0, sprintf("%s: no nail pistol (%g)", at37_name, at37_np)); }
    else { VR_AT_Check(fabs(rnp - at37_exp_np) <= AT37_TOL, sprintf("%s: nail pistol %g in %g boxes (expected %g)", at37_name, at37_np, at37_done, at37_exp_np * at37_done)); }
    if(at37_exp_rv == 0) { VR_AT_Check(at37_rv == 0, sprintf("%s: no revolver (%g)", at37_name, at37_rv)); }
    else { VR_AT_Check(fabs(rrv - at37_exp_rv) <= AT37_TOL, sprintf("%s: revolver %g in %g boxes (expected %g)", at37_name, at37_rv, at37_done, at37_exp_rv * at37_done)); }

    at37_row = at37_row + 1;
    if(at37_row >= AT37_ROWS) { VR_AT37_Finish(); return; }
    VR_AT37_Row(p);
}

// ---------------------------------------------------------------------------
// Test 39: the arena's messages on the wrist (vrarena, the headset mock; vr_arena_wrist, gadget::arenaMessage)
// ---------------------------------------------------------------------------
// The messages are the director's own (VR_ARENA_Show / ShowAll; ar_said_n counts them), all but the fall's (its function
// called as the director calls it: the test's player can't fall). The engine's side is read through panelboard_read's
// what 4..8 (vr_gadget.hpp): the last one's ink in the hologram's image and its frames shown facing the eyes, the counts
// taken by the wrist, shown in view instead, and the buzzes.

float vr_at39_state;
float vr_at39_n;       // ar_said_n at the step's start
float vr_at39_said0;   // ... and at the test's
float vr_at39_taken0;  // the wrist's count then
float vr_at39_view0;   // the view's
float vr_at39_buzz0;   // the buzzes
float vr_at39_frames;  // frames counted out of view
float vr_at39_ok;      // messages seen on the wrist

float(float xWhat) VR_AT39_Read = { return panelboard_read("", 0, 0, 0, 0, xWhat); }

// The off hand: its gadget raised in front of the eyes (1), or turned away (0).
void(float xLook) VR_AT39_Hand =
{
    if(xLook) { localcmd("vr_mock_hand off -0.05 1.40 -0.35 0 -80 0\n"); }
    else { localcmd("vr_mock_hand off -0.05 1.40 -0.35 0 90 0\n"); }
}

// 1 once the newest message is on the wrist and shown: taken (as many as sent), 3 frames facing the eyes, inked.
float(string xWhat) VR_AT39_Shown =
{
    if(VR_AT39_Read(6) - vr_at39_taken0 < ar_said_n - vr_at39_said0) { return 0; }
    if(VR_AT39_Read(5) < 3 || VR_AT39_Read(4) < 0) { return 0; }
    float ink = VR_AT39_Read(4);
    VR_AT_Check(ink > 0.003, sprintf("%s: its words in the hologram's image (ink %g)", xWhat, ink));
    vr_at39_ok = vr_at39_ok + 1;
    dprint(sprintf("arena: test 39: %s on the wrist (ink %g, %g frames)\n", xWhat, ink, VR_AT39_Read(5)));
    vr_at39_n = ar_said_n;
    return 1;
}

void() VR_AT39_Begin =
{
    vr_test_flags = VR_TEST_LOUD | VR_TEST_QUIET; // (broadcast: the verdict is seen with developer 0, for -QuitOn)
    if(!ar_mgr) { VR_AT_Check(FALSE, "test 39 needs an arena map"); VR_AT_End(); return; }
    cvar_set("vr_arena_wrist", "1");
    cvar_set("vr_arena_perks", "1");
    VR_AT39_Hand(1);
    vr_at39_said0 = vr_at39_n = ar_said_n;
    vr_at39_taken0 = VR_AT39_Read(6);
    vr_at39_view0 = VR_AT39_Read(7);
    vr_at39_buzz0 = VR_AT39_Read(8);
    vr_at39_ok = 0;
    vr_at39_state = 1;
    vr_at_until = time + 120;
}

void() VR_AT39_Frame =
{
    if(vr_at39_state < 10 && VR_AT39_Read(7) != vr_at39_view0)
    {
        VR_AT_Check(FALSE, sprintf("on: an arena message showed in the middle of the view (%g)", VR_AT39_Read(7) - vr_at39_view0));
        VR_AT_End();
        return;
    }
    if(vr_at39_state == 1) // (the countdown to wave 1)
    {
        if(ar_said_n == vr_at39_n || !VR_ARENA_Between() || ar_count <= 0) { return; }
        if(VR_AT39_Shown("the countdown")) { vr_at39_state = 2; }
        return;
    }
    if(vr_at39_state == 2) // (wave 1 comes in)
    {
        if(ar_said_n == vr_at39_n || VR_ARENA_Between() || ar_wave != 1) { return; }
        if(!VR_AT39_Shown("wave 1")) { return; }
        VR_AT39_Hand(0);
        vr_at39_state = 3;
        return;
    }
    if(vr_at39_state == 3) // (wave 1's line gone, the gadget away)
    {
        if(VR_AT39_Read(5) >= 0) { return; }
        vr_at39_state = 4;
        return;
    }
    if(vr_at39_state == 4) // (its foes down: the clear comes out of view)
    {
        if(!VR_ARENA_Between()) { VR_AT_KillOne(1); return; }
        if(ar_said_n == vr_at39_n || VR_AT39_Read(6) - vr_at39_taken0 < ar_said_n - vr_at39_said0) { return; }
        VR_AT_Check(VR_AT39_Read(8) == vr_at39_buzz0 + 1, sprintf("out of view: the clear buzzed once (%g)", VR_AT39_Read(8) - vr_at39_buzz0));
        vr_at39_frames = 0;
        vr_at39_state = 5;
        return;
    }
    if(vr_at39_state == 5) // (half a second out of view: not shown)
    {
        vr_at39_frames = vr_at39_frames + 1;
        if(vr_at39_frames < 36) { return; }
        VR_AT_Check(VR_AT39_Read(5) == 0, sprintf("out of view: the clear not shown (%g frames)", VR_AT39_Read(5)));
        VR_AT39_Hand(1);
        vr_at39_state = 6;
        return;
    }
    if(vr_at39_state == 6) // (raised: the clear shows; then a perk's choice)
    {
        if(!VR_AT39_Shown("wave 1 clear, the wrist raised")) { return; }
        VR_ARK_Offer(5);
        VR_AT_Check(ark_open && ark_offer_n > 0, sprintf("a perk's cards are up (%g)", ark_offer_n));
        VR_AT_Check(VR_ARK_CardPick(0) == 1, "the first card taken");
        vr_at39_state = 7;
        return;
    }
    if(vr_at39_state == 7) // (the perk taken: in place of the clear, no buzz)
    {
        if(!VR_AT39_Shown("a perk taken")) { return; }
        VR_AT_Check(VR_AT39_Read(8) == vr_at39_buzz0 + 1, sprintf("in view: no more buzzes (%g)", VR_AT39_Read(8) - vr_at39_buzz0));
        VR_ARENA_Show(VR_Player(), sprintf("YOU FELL AT WAVE %g\n\nscore %g\n", ar_wave, ar_score), 9); // (the director's own line)
        vr_at39_state = 8;
        return;
    }
    if(vr_at39_state == 8) // (the fall; then off)
    {
        if(!VR_AT39_Shown("a fall")) { return; }
        VR_AT_Check(vr_at39_ok == 5, sprintf("five messages seen on the wrist (%g)", vr_at39_ok));
        cvar_set("vr_arena_wrist", "0");
        vr_at39_taken0 = VR_AT39_Read(6);
        vr_at39_view0 = VR_AT39_Read(7);
        vr_at39_state = 10;
        return;
    }
    if(vr_at39_state == 10) // (off: the next two show in view, the wrist takes none)
    {
        if(ar_said_n < vr_at39_n + 2 || VR_AT39_Read(7) < vr_at39_view0 + 2) { return; }
        VR_AT_Check(VR_AT39_Read(7) - vr_at39_view0 == ar_said_n - vr_at39_n,
                    sprintf("off: every message in the view (%g of %g)", VR_AT39_Read(7) - vr_at39_view0, ar_said_n - vr_at39_n));
        VR_AT_Check(VR_AT39_Read(6) == vr_at39_taken0, sprintf("off: none on the wrist (%g)", VR_AT39_Read(6) - vr_at39_taken0));
        VR_AT_End();
    }
}

// ---------------------------------------------------------------------------
// Test 42 (vrarena): the budget director's gun knights (his ask 2026-11-11: knights and death knights with actual guns,
// a death knight boss with a rocket launcher and red armour). Ahead of the player: a knight with a shotgun, a knight
// with a super nailgun and a death knight boss with a rocket launcher in red armour (as VR_ARENA_SpawnMonster dresses
// one: its plan's gun held, VR_AgArmor_Give, the boss's bulk). The player hunted (god mode): each must hold its gun and
// fire it at him (its .vr_ag_total_shots); a screenshot while they do; each felled must drop its gun (a thrown_weapon of
// that weapon by it). Verdict `arena: test 42 over: N passed, M failed`.
// ---------------------------------------------------------------------------

entity vr_at42_g[3];
vector vr_at42_at[3]; // where each fell
float vr_at42_wid[3];
float vr_at42_shot;   // the screenshot taken

float vr_at42_wait; // when the wait for the arena and the player gives up (0: not waiting yet)

void() VR_AT42_Begin =
{
    entity p = VR_PlayerAlive();
    if(!ar_mgr || !p || time < 2) // (the map's start: its arena and its player a few frames in; the frames retry)
    {
        if(vr_at42_wait <= 0) { vr_at42_wait = time + 30; }
        if(time > vr_at42_wait) { VR_AT_Check(FALSE, "test 42 needs the arena running (vrarena) and a player"); VR_AT_End(); }
        vr_at_step = 0;
        return;
    }
    entity keep = self;
    p.flags = p.flags | FL_GODMODE;
    makevectors([0, p.v_viewangle_y, 0]); // (ahead of his view, the headset's: the screenshot shows them)
    float k;
    for(k = 0; k < 3; k++)
    {
        entity g = spawn();
        self = g;
        g.classname = k < 2 ? "monster_knight" : "monster_hell_knight";
        float wid = WID_SHOTGUN;
        if(k == 1) { wid = WID_SUPER_NAILGUN; }
        if(k == 2) { wid = WID_ROCKET_LAUNCHER; }
        vr_at42_wid[k] = wid;
        setorigin(g, p.origin + v_forward * (k == 2 ? 520 : 380) + v_right * (k == 0 ? -140 : k == 1 ? 140 : 0) + '0 0 24');
        g.angles = [0, p.angles_y + 180, 0];
        g.weapon = wid;
        vr_ar_hold_roll = 1;
        if(k < 2) { monster_vr_knight(); } else { monster_vr_hknight(); }
        vr_ar_hold_roll = 0;
        if(k == 2)
        {
            VR_AgArmor_Give(g, 3, 1023);
            g.health = VR_ARENA_BossHealth(g.health, 1);
        }
        g.enemy = p;
        vr_at42_g[k] = g;
        self = keep;
        VR_AT_Check(g.vr_ag_wid == wid && g.vr_ag_prop != world,
                    sprintf("%s given weapon %g holds it (%g, %s)", k < 2 ? "a knight" : "the death knight boss", wid,
                            g.vr_ag_wid, g.vr_ag_prop ? "a prop in its hand" : "nothing"));
    }
    VR_AT_Check(vr_at42_g[2].vr_ar_type == 3, sprintf("the death knight boss wears red armour (type %g)", vr_at42_g[2].vr_ar_type));
    vr_at42_shot = 0;
    vr_at_step = 1;
    vr_at_until = time + 60;
}

void() VR_AT42_Frame =
{
    if(vr_at_step == 0) { VR_AT42_Begin(); return; } // (still waiting for the arena and the player)
    entity p = VR_PlayerAlive();
    if(!p) { VR_AT_Check(FALSE, "the player died"); VR_AT_End(); return; }
    float k;
    if(vr_at_step == 1) // (each has fired its gun at him)
    {
        float fired = 0;
        for(k = 0; k < 3; k++) { fired = fired + (vr_at42_g[k].vr_ag_total_shots > 0); }
        if(!vr_at42_shot && vr_at42_g[2].vr_ag_total_shots > 0)
        {
            vr_at42_shot = 1;
            localcmd("screenshot\n");
        }
        if(fired < 3 && time < vr_at_until - 20) { return; }
        for(k = 0; k < 3; k++)
        {
            VR_AT_Check(vr_at42_g[k].vr_ag_total_shots > 0 && vr_at42_g[k].vr_ag_wid == vr_at42_wid[k],
                        sprintf("%s fired its weapon %g at the player (%g shots)", k < 2 ? "a knight" : "the death knight boss",
                                vr_at42_wid[k], vr_at42_g[k].vr_ag_total_shots));
        }
        vr_at_step = 2;
        vr_at_until = time + 8;
        return;
    }
    if(vr_at_step == 2) // (each felled drops its gun)
    {
        for(k = 0; k < 3; k++) // (felled, not gibbed, as test 40: its armour soaked up, a blow it shrugged off again)
        {
            entity f = vr_at42_g[k];
            if(f.health > 0 && f.takedamage)
            {
                vr_at42_at[k] = f.origin;
                T_Damage(f, p, p, f.health + f.armorvalue + 20);
            }
        }
        float lying = 0;
        for(k = 0; k < 3; k++)
        {
            entity e = find(world, classname, "thrown_weapon");
            while(e)
            {
                if(e.weapon == vr_at42_wid[k] && vlen(e.origin - vr_at42_at[k]) < 320) { lying = lying + 1; break; }
                e = find(e, classname, "thrown_weapon");
            }
        }
        if(lying < 3 && time < vr_at_until) { return; }
        for(entity w = find(world, classname, "thrown_weapon"); w; w = find(w, classname, "thrown_weapon"))
        {
            dprint(sprintf("arena: t42 lying %s (weapon %g) at %.0f / %.0f / %.0f from the three\n", w.netname, w.weapon,
                           vlen(w.origin - vr_at42_at[0]), vlen(w.origin - vr_at42_at[1]), vlen(w.origin - vr_at42_at[2])));
        }
        VR_AT_Check(lying == 3, sprintf("each felled dropped its gun by it (%g of 3)", lying));
        p.flags = p.flags - (p.flags & FL_GODMODE);
        VR_AT_End();
    }
}

// ---------------------------------------------------------------------------
// Test 40 (vrarena): the weapon schedule's weapons in the player's hands (ROUND24.md, "Survival Arena: every armed
// humanoid's weapon"). For each code of each row this game has (VR_ARENA_GunOk; a run's every code), an armed grunt
// holding it is spawned the way the director spawns one and felled ahead of the player; the weapon it drops is taken
// into the player's empty main hand (its hand touch, as a grab: VR_ClassTest_Take) and must be that weapon (an AD
// sword's kind, the death knight's sword's flag kept); a gun must come loaded and fire, its trigger held (W_Attack a
// frame: the spun-up ones, the nailguns and the Widowmaker, take a few). Verdict
// `arena: test 40 over: N passed, M failed`.
// ---------------------------------------------------------------------------

void(entity w) VR_ClassTest_Take;         // (vr_pickup_test.qc: self, a player, takes `w` into its main hand)

float vr_at40_row;    // the row being tried
float vr_at40_code;   // its code being tried (0: none yet)
float vr_at40_cases;  // the codes tried
float vr_at40_taken;  // ... whose drop the player took and held
entity vr_at40_g;     // its grunt
float vr_at40_at;     // when it was spawned, then when it fell
vector vr_at40_org;   // where
float vr_at40_clip;   // the gun taken: its rounds then

// The next code to try (FALSE: none left): the row's next code this game has, else the next row's first.
float() VR_AT40_Next =
{
    float r = vr_at40_row;
    float c = vr_at40_code + 1;
    while(r < AR_GUN_N)
    {
        VR_ARENA_GunRowSet(r);
        if(c < ar_gr_lo) { c = ar_gr_lo; }
        float hi = ar_gr_hi;
        for(; c <= hi; c++)
        {
            if(VR_AG_Decode(c)) { vr_at40_row = r; vr_at40_code = c; return TRUE; }
        }
        r = r + 1;
        c = 0;
    }
    return FALSE;
}

// The weapons lying about (thrown_weapon props), cleared before each case: the one found after is its grunt's.
void() VR_AT40_ClearDrops =
{
    entity e = find(world, classname, "thrown_weapon");
    while(e)
    {
        entity nx = find(e, classname, "thrown_weapon");
        remove(e);
        e = nx;
    }
}

void() VR_AT40_Begin =
{
    if(!ar_mgr) { VR_AT_Check(FALSE, "test 40 needs the arena running (vrarena)"); VR_AT_End(); return; }
    entity p = VR_PlayerAlive();
    if(!p) { VR_AT_Check(FALSE, "test 40 needs a player"); VR_AT_End(); return; }
    cvar_set("vr_agrunt_flaregun", "0"); // (no flare gun drawn as a second weapon: the drop is the given one)
    p.flags = p.flags | FL_NOTARGET;
    vr_at40_row = 0;
    vr_at40_code = 0;
    vr_at40_cases = 0;
    vr_at40_taken = 0;
    vr_at_step = 0;
    vr_at_until = time + 240;
}

void() VR_AT40_Frame =
{
    entity p = VR_PlayerAlive();
    if(!p) { VR_AT_Check(FALSE, "the player died"); VR_AT_End(); return; }
    entity keep = self;
    string name;

    if(vr_at_step == 0)
    {
        if(!VR_AT40_Next())
        {
            dprint(sprintf("arena: t40 %g codes tried, %g taken and held\n", vr_at40_cases, vr_at40_taken));
            VR_AT_Check(vr_at40_cases > 0 && vr_at40_taken == vr_at40_cases,
                        sprintf("every weapon of the schedule a foe drops is the player's to take (%g of %g)",
                                vr_at40_taken, vr_at40_cases));
            p.flags = p.flags - (p.flags & FL_NOTARGET);
            VR_AT_End();
            return;
        }
        vr_at40_cases = vr_at40_cases + 1;
        VR_AT40_ClearDrops();
        VR_AG_Decode(vr_at40_code);
        float wid = vr_ag_dec_wid;
        makevectors([0, p.angles_y, 0]);
        entity g = spawn();
        self = g;
        g.classname = "monster_army";
        // (a place of its own: five along an arc ahead, so it never comes in on the bodies of the cases before)
        float slot = vr_at40_cases - 5 * floor(vr_at40_cases / 5) - 2;
        setorigin(g, p.origin + v_forward * 128 + v_right * (56 * slot) + '0 0 8');
        g.angles = [0, p.angles_y + 180, 0];
        g.weapon = vr_at40_code;
        vr_ar_hold_roll = 1; // (as VR_ARENA_SpawnMonster spawns a wave's humanoid: its plan's weapon held)
        monster_vr_grunt();
        vr_ar_hold_roll = 0;
        self = keep;
        name = VR_ARENA_GunName(vr_at40_row);
        dprint(sprintf("arena: t40 row %g (%s) code %g: grunt holds %g (%s)\n", vr_at40_row, name, vr_at40_code,
                       g.vr_ag_wid, g.vr_ag_prop ? "a prop in its hand" : "nothing"));
        VR_AT_Check(g.vr_ag_wid == wid && g.vr_ag_prop != world,
                    sprintf("%s (code %g): a grunt given it holds it", name, vr_at40_code));
        vr_at40_g = g;
        vr_at40_at = time;
        vr_at_step = 2;
        return;
    }

    if(vr_at_step == 2) // (felled once it has started: a monster takes no damage before its first think)
    {
        entity f = vr_at40_g;
        if(f.takedamage == DAMAGE_NO && time < vr_at40_at + 2) { return; }
        vr_at40_org = f.origin;
        T_Damage(f, p, p, f.health + 20);
        if(f.health > 0 && time < vr_at40_at + 2) { return; } // (a blow it shrugged off: again next frame)
        VR_AT_Check(f.health <= 0, sprintf("%s (code %g): its grunt felled (health %g)", VR_ARENA_GunName(vr_at40_row),
                                           vr_at40_code, f.health));
        vr_at40_at = time;
        vr_at_step = 1;
        return;
    }

    name = VR_ARENA_GunName(vr_at40_row);
    if(vr_at_step == 3) // (a gun: its trigger held a frame at a time until it fires, as a hand holds it)
    {
        self = p;
        W_Attack();
        self = keep;
        float now = VRGetEntWeaponClip(p, cVR_MainHand);
        if(now >= vr_at40_clip && time < vr_at40_at + 1.5) { return; }
        dprint(sprintf("arena: t40 %s: fired, %g -> %g rounds\n", name, vr_at40_clip, now));
        VR_AT_Check(now < vr_at40_clip, sprintf("%s fires in the player's hand (%g -> %g rounds)", name, vr_at40_clip, now));
        if(now < vr_at40_clip) { vr_at40_taken = vr_at40_taken + 1; }
        VRSetEntWeapon(p, cVR_MainHand, WID_FIST);
        vr_at_step = 0;
        return;
    }

    // Its weapon on the floor near where it fell (it may take a few frames to come down out of its hand).
    VR_AG_Decode(vr_at40_code);
    float want = vr_ag_dec_wid;
    float wantFlags = vr_ag_dec_flags;
    entity w = world;
    for(entity e = find(world, classname, "thrown_weapon"); e; e = find(e, classname, "thrown_weapon"))
    {
        if((e.weapon == want) && (vlen(e.origin - vr_at40_org) < 320)) { w = e; }
    }
    if(!w && time < vr_at40_at + 2) { return; }
    vr_at_step = 0;
    VR_AT_Check(w != world, sprintf("%s (code %g): dropped, it lies on the floor", name, vr_at40_code));
    if(!w) { return; }

    self = p;
    VR_ClassTest_Take(w);
    self = keep;
    float held = VRGetEntWeapon(p, cVR_MainHand);
    float flags = VRGetEntWeaponFlags(p, cVR_MainHand);
    float kindMask = QVR_WPNFLAG_SWORD_KIND | QVR_WPNFLAG_USE_SECONDARY_AMMO;
    float ok = ((held == want) && ((want != WID_SWORD) || ((flags & kindMask) == (wantFlags & kindMask))));
    VR_AT_Check(ok, sprintf("%s (code %g): taken, the main hand holds it (weapon %g flags %g)", name, vr_at40_code, held, flags));
    float cls = VR_AG_Class(want);
    if(ok && (cls != VR_AG_CLASS_MELEE) && (cls != VR_AG_CLASS_FISTS))
    {
        vr_at40_clip = VRGetEntWeaponClip(p, cVR_MainHand);
        dprint(sprintf("arena: t40 %s: taken with %g rounds in it\n", name, vr_at40_clip));
        VR_AT_Check(vr_at40_clip > 0, sprintf("%s: it comes loaded (%g)", name, vr_at40_clip));
        p.attack_finished = 0;
        vr_at40_at = time;
        vr_at_step = 3;
        return;
    }
    if(ok) { vr_at40_taken = vr_at40_taken + 1; }
    VRSetEntWeapon(p, cVR_MainHand, WID_FIST);
}
void() VR_ARENA_Test_Frame =
{
    float c = cvar_hget(cvarh_vr_arena_test);

    if(c == 0)
    {
        if(vr_at_case) { VR_AT_Restore(); VR_AT_SetSkill(vr_at_skill0); vr_at_case = 0; dprint("arena: test stopped\n"); }
        return;
    }

    if(vr_at_case == 0)
    {
        vr_at_case = c;
        VR_Test_BeginFlags("arena", VR_TEST_QUIET);
        vr_at_step = 0;
        vr_at_until = time + 30;
        VR_AT_Save();
        vr_at_skill0 = cvar_hget(cvarh_skill);
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
        if(c == 15) { VR_AT15_Begin(); return; }
        if(c == 16) { VR_AT16_Begin(); return; }
        if(c == 17) { VR_AT17_Run(); VR_AT_End(); return; }
        if(c == 18) { return; } // (its frame: once the strip has been)
        if(c == 19) { VR_AT19_Begin(); return; }
        if(c == 30) { VR_AT30_Begin(); return; }
        if(c == 31) { VR_AT31_Run(); VR_AT_End(); return; }
        if(c == 32) { VR_AT32_Begin(); return; }
        if(c == 33) { VR_AT33_Run(); VR_AT_End(); return; }
        if(c == 34) { VR_AT34_Run(); VR_AT_End(); return; }
        if(c == 35) { VR_AT35_Begin(); return; }
        if(c == 36) { VR_AT36_Begin(); return; }
        if(c == 37) { VR_AT37_Begin(); return; }
        if(c == 38) { VR_AT38_Begin(); return; }
        if(c == 39) { VR_AT39_Begin(); return; }
        if(c == 40) { VR_AT40_Begin(); return; }
        if(c == 41) { VR_AT41_Begin(); return; }
        if(c == 42) { VR_AT42_Begin(); return; }

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
    if(vr_at_case == 15) { VR_AT15_Frame(); return; }
    if(vr_at_case == 16) { VR_AT16_Frame(); return; }
    if(vr_at_case == 18) { VR_AT18_Frame(); return; }
    if(vr_at_case == 19) { VR_AT19_Frame(); return; }
    if(vr_at_case == 30) { VR_AT30_Frame(); return; }
    if(vr_at_case == 32) { VR_AT32_Frame(); return; }
    if(vr_at_case == 35) { VR_AT35_Frame(); return; }
    if(vr_at_case == 36) { VR_AT36_Frame(); return; }
    if(vr_at_case == 37) { VR_AT37_Frame(); return; }
    if(vr_at_case == 38) { VR_AT38_Frame(); return; }
    if(vr_at_case == 39) { VR_AT39_Frame(); return; }
    if(vr_at_case == 40) { VR_AT40_Frame(); return; }
    if(vr_at_case == 41) { VR_AT41_Frame(); return; }
    if(vr_at_case == 42) { VR_AT42_Frame(); return; }
}
#endif
