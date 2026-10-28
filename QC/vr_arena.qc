//
// vr_arena.qc - Survival Arena: the wave director.
//
// The waves and their ladder: the first waves are unarmed grunts, then grunts with a nail pistol and a shotgun,
// and so on up the weapon ladder with the mix drawn afresh every wave (vr_arena_seed's rolls), a boss every
// vr_arena_boss_every waves, Quake's own monsters joining as the waves go, and medkits and armour dropped between
// waves. The player starts at their fists and climbs the ladder on the guns their enemies carried (the game's
// own enemy drops: vr_enemy_drops).
//
// Off by default: none of this runs unless the map is "vrarena" or vr_arena is set. A wave's foes are marked
// with .ar_slot (their wave's number, flipped negative once they fall), so the director never has to guess who
// of the map's monsters is its own.
//
// The knobs (Quake/vr/vr_cvars.inc; read here with plain cvar()):
//   vr_arena              0    the arena runs even in a map that is not "vrarena"
//   vr_arena_wave_delay   6    seconds from a wave clear to the next wave
//   vr_arena_max_alive    8    foes alive at once at most; the rest of the wave trickles in as they free up
//   vr_arena_boss_every   5    a boss wave every this many waves (0: no boss waves)
//   vr_arena_boss_health  1.25 a boss's health times this
//   vr_arena_medkits_after 2   medkits after every wave (0: none)
//   vr_arena_armor_every  4    an armour after every this many waves (0: none)
//   vr_arena_score_kill   100  the score of a kill, times its ladder tier (a boss: four of those)
//   vr_arena_seed         0    the waves' rolls: 0 a different ladder every run, else that seed's ladder
//   vr_arena_test         0    the QC's arena tests (vr_arena_test.qc): 1 the plans, 2 the live waves, 3 a boss wave
//

// ---------------------------------------------------------------------------
// The fields the arena puts on things
// ---------------------------------------------------------------------------

.float kind;    // info_arena_spawn: 1 a ground point, 2 a high one (a wave's ranged and flying foes come in above the floor)
.float ar_slot; // > 0: one of the arena's wave monsters, of the wave that number (its fall flips it to that number's negative)
.float ar_tier; // what a wave monster is worth on the ladder: 1, 2, 3 (a boss: plus AR_BOSS_MARK)
.float ar_drop; // > 0: an arena's own drop between waves: 1 a medkit, 2 an armour

// ---------------------------------------------------------------------------
// The waves' tables
// ---------------------------------------------------------------------------

#define AR_PLAN_MAX 12   // a wave's planned foes at most (vr_arena_max_alive caps how many of them are up at once)
#define AR_RING_MAX 8    // the fallback ring's places around the player, when a map has no info_arena_spawn
#define AR_ASIDE_MAX 5   // a place is tried as the map drew it, then aside this many ways
#define AR_ASIDE_STEP 48 // how far a step off a taken place goes (a monster's own box is 32 wide)
#define AR_PLACE_GAP  48 // a place this close to one of this wave's live foes counts as taken

// The species a wave sends (the plan's kind: its monster's own spawn function).
float AR_SP_GRUNT    = 0; // monster_vr_grunt: a grunt holding one of the player's weapons
float AR_SP_ENFORCER = 1; // monster_vr_enforcer: an enforcer holding one of the player's weapons
float AR_SP_ZOMBIE   = 2; // monster_zombie
float AR_SP_DOG      = 3; // monster_dog
float AR_SP_KNIGHT   = 4; // monster_knight
float AR_SP_OGRE     = 5; // monster_ogre
float AR_SP_FIEND    = 6; // monster_demon1: a fiend
float AR_SP_HKNIGHT  = 7; // monster_hell_knight: a death knight

// The ladder's rungs a wave may draw from, and the waves Quake's own monsters join at.
float AR_W_TIER2   = 10; // the second tier (the super shotgun, the nail guns) from this wave
float AR_W_TIER3   = 18; // the third tier (the launchers, the lightning) from this wave
float AR_W_ZOMBIE  = 2;  // (a wave 3's zombie: the plan's own line; the table's floor)
float AR_W_KNIGHT  = 4;
float AR_W_DOG     = 6;
float AR_W_OGRE    = 8;
float AR_W_FIEND   = 12;
float AR_W_HKNIGHT = 16;

float AR_FIST        = -1;   // the plan's "no gun": an armed humanoid at their fists (monster_vr_grunt's "weapon" -1)
float AR_BOSS_MARK   = 8;    // a boss's ar_tier carries this (its worth: the tier times four)
float AR_TICK        = 0.5;  // the director's beat (s)
float AR_SPAWN_GAP   = 1.5;  // a wave's foes come in this far apart, at least
float AR_RING_R      = 400;  // no info_arena_spawn in the map: foes come in this far off the player
float AR_DROP_R      = 220;  // its medkits and armour land about this far off the player
float AR_STALL       = 300;  // a wave this long over is cleared (its stragglers removed) so the arena keeps going
float AR_WAVE_BONUS  = 250;  // a wave clear's score times the wave
float AR_KILL_BOSS   = 4;    // a boss is worth this many of its tier's kills

// ---------------------------------------------------------------------------
// The run's state (file-scope: QC has no function-local statics)
// ---------------------------------------------------------------------------

entity ar_mgr;             // the arena's director (world: no arena in this map)
entity ar_fallen;          // the last player the arena saw fall (its message's target)

float ar_wave;             // the wave in the field (0: none yet)
float ar_score;            // the run's score
float ar_kills;            // the run's kills
float ar_wave_kills;       // this wave's kills
float ar_plan_n;           // this wave's planned foes
float ar_plan_left;        // its foes not in the field yet
float ar_plan_live;        // its foes put in the field (up or down)
float ar_plan_kind[AR_PLAN_MAX];
float ar_plan_gun[AR_PLAN_MAX];
float ar_plan_tier[AR_PLAN_MAX];
float ar_plan_boss[AR_PLAN_MAX];
float ar_next_spawn;       // the time the next planned foe may come in
float ar_wave_at;          // the time this wave began
float ar_alive_peak;       // the most foes the arena had up at once, this run (its wave cap at work)
float ar_drop_health;      // a wave clear's medkits
float ar_drop_armour;      // a wave clear's armours
float ar_spot_next;        // the round robin over the map's spawn points
float ar_item_next;        // the round robin over the map's item points
float ar_used_ground;      // this wave's foes placed on the map's floor places
float ar_used_high;        // this wave's foes placed on the map's high places
float ar_s1; float ar_s2; float ar_s3; // the wave rolls' three states (Levy's triple, as vr_armedrepl.qc's)
float ar_serverflags0;   // the map's serverflags as they were, for a fresh run (as vr_mg_horde.qc keeps its own)
float ar_stripped;         // the players have been put down to their fists already
float ar_down;             // the players are down: a fresh run is on its way

// The director's own beats (vr_arena.qc's own cycle; world.qc only calls WorldSpawn and Frame).
void() VR_ARENA_WaveStart;
void() VR_ARENA_ManagerThink;
void() VR_ARENA_RestartThink;

// The boss wave's perk choice (vr_arena_perks.qc, compiled after this file): every wave clear reports itself to
// it, and so does every kill the director counted. Neither does anything unless the run took a perk / the offer
// knob is on.
void(float xWave) VR_ARK_WaveDone;
void(entity e) VR_ARK_Kill;

// ---------------------------------------------------------------------------
// The knobs, and the small maths they need
// ---------------------------------------------------------------------------

float() VR_ARENA_On =
{
    if(mapname == "vrarena") { return TRUE; }
    return cvar("vr_arena") != 0;
}

float(float xValue, float xLow, float xHigh) VR_ARENA_Clamp =
{
    if(xValue < xLow)  { return xLow; }
    if(xValue > xHigh) { return xHigh; }
    return xValue;
}

float(float x, float xMod) VR_ARENA_Mod =
{
    float r = x - xMod * floor(x / xMod);
    if(r < 0)  { r = r + xMod; }
    if(r >= xMod) { r = r - xMod; }
    return r;
}

float() VR_ARENA_Opt_MaxAlive   { return VR_ARENA_Clamp(cvar("vr_arena_max_alive"), 1, AR_PLAN_MAX); }
float() VR_ARENA_Opt_Delay      { return VR_ARENA_Clamp(cvar("vr_arena_wave_delay"), 1, 60); }
float() VR_ARENA_Opt_BossEvery  { return VR_ARENA_Clamp(cvar("vr_arena_boss_every"), 0, 20); }
float() VR_ARENA_Opt_BossHealth { return VR_ARENA_Clamp(cvar("vr_arena_boss_health"), 1, 8); }
float() VR_ARENA_Opt_Medkits    { return VR_ARENA_Clamp(cvar("vr_arena_medkits_after"), 0, 4); }
float() VR_ARENA_Opt_ArmorEvery { return VR_ARENA_Clamp(cvar("vr_arena_armor_every"), 0, 12); }
float() VR_ARENA_Opt_ScoreKill  { return VR_ARENA_Clamp(cvar("vr_arena_score_kill"), 1, 10000); }

// What a wave monster's ar_tier is worth on the ladder: its mark (AR_BOSS_MARK) is not a rung, so it comes off
// before the tier is scored (a boss is worth four of its rung, not four of the mark).
float(float xTier) VR_ARENA_TierOf =
{
    if(xTier >= AR_BOSS_MARK) { return xTier - AR_BOSS_MARK; }
    return xTier;
}

// The score of one kill: 100 (vr_arena_score_kill) times the ladder tier it held, a boss four of those.
float(float xTier, float xBoss) VR_ARENA_KillScore =
{
    float t = VR_ARENA_Clamp(xTier, 1, 3);
    float per = VR_ARENA_Opt_ScoreKill() * t;
    if(xBoss) { per = per * AR_KILL_BOSS; }
    return per;
}

// A wave clear's bonus: 250 times the wave.
float(float xWave) VR_ARENA_WaveBonus = { return AR_WAVE_BONUS * xWave; }

// A wave clear's medkits, and which armour it adds: the light one until the waves reach AR_W_FIEND's wave, and
// from there the heavier one and the light one by turns (a run's armours keep meaning something both ways).
float(float xWave) VR_ARENA_MedkitCount = { return VR_ARENA_Opt_Medkits(); }
float(float xWave) VR_ARENA_ArmourKind =
{
    float every = VR_ARENA_Opt_ArmorEvery();
    if(every <= 0) { return 0; }
    if(VR_ARENA_Mod(xWave, every) != 0) { return 0; }
    if(xWave < AR_W_FIEND) { return 1; }
    return VR_ARENA_Mod(xWave / every, 2) == 1 ? 2 : 1;
}

// ---------------------------------------------------------------------------
// The wave rolls: a Levy triple reseeded from vr_arena_seed and the wave, so a wave's mix of guns is the
// same every run for a seed, and different every run for vr_arena_seed 0.
// ---------------------------------------------------------------------------

void(float xWave) VR_ARENA_SeedWaves =
{
    float base = VR_ARENA_Mod(cvar("vr_arena_seed"), 30000);
    if(cvar("vr_arena_seed") <= 0) { base = 1 + floor(29998 * random()); } // (an unseeded run: its own ladder)
    base = base + 7919 * xWave;

    ar_s1 = 1 + VR_ARENA_Mod(base, 30268);
    ar_s2 = 1 + VR_ARENA_Mod(3 * base + 17, 30306);
    ar_s3 = 1 + VR_ARENA_Mod(7 * base + 29, 30322);
}

float() VR_ARENA_Rand =
{
    ar_s1 = VR_ARENA_Mod(171 * ar_s1, 30269);
    ar_s2 = VR_ARENA_Mod(172 * ar_s2, 30307);
    ar_s3 = VR_ARENA_Mod(170 * ar_s3, 30323);
    float r = ar_s1 / 30269 + ar_s2 / 30307 + ar_s3 / 30323;
    return r - floor(r);
}

// ---------------------------------------------------------------------------
// The wave's plan: who it sends, and the gun an armed humanoid holds
// ---------------------------------------------------------------------------

// What a wave monster is worth on the ladder (a classic monster's own tier, an armed humanoid's gun's tier).
float(float xKind, float xGun) VR_ARENA_WeaponTier =
{
    if(xKind == AR_SP_OGRE || xKind == AR_SP_FIEND)    { return 2; }
    if(xKind == AR_SP_HKNIGHT)                         { return 3; }
    if(xGun == WID_SUPER_SHOTGUN || xGun == WID_NAILGUN || xGun == WID_SUPER_NAILGUN) { return 2; }
    if(xGun == WID_ROCKET_LAUNCHER || xGun == WID_GRENADE_LAUNCHER || xGun == WID_LIGHTNING) { return 3; }
    return 1; // (its fists, the shotgun, the nail pistol, the axe)
}

float(float xTier) VR_ARENA_PickTierGun =
{
    float r = VR_ARENA_Rand();

    if(xTier == 3)
    {
        if(r < 0.34) { return WID_ROCKET_LAUNCHER; }
        if(r < 0.67) { return WID_LIGHTNING; }
        return WID_GRENADE_LAUNCHER;
    }
    if(xTier == 2)
    {
        if(r < 0.40) { return WID_SUPER_SHOTGUN; }
        if(r < 0.75) { return WID_NAILGUN; }
        return WID_SUPER_NAILGUN;
    }
    if(r < 0.50) { return WID_SHOTGUN; }
    if(r < 0.90) { return WID_NAILPISTOL; }
    return WID_AXE;
}

// A wave's humanoid's gun, from the tiers this wave has reached.
float(float xWave) VR_ARENA_PickGun =
{
    float r = VR_ARENA_Rand();
    if(xWave >= AR_W_TIER3 && r < 0.25) { return VR_ARENA_PickTierGun(3); }
    if(xWave >= AR_W_TIER2 && r < 0.60) { return VR_ARENA_PickTierGun(2); }
    return VR_ARENA_PickTierGun(1);
}

// The first waves' humanoids: still some of them unarmed, and only the ladder's bottom rungs.
float(float xWave) VR_ARENA_PickEarlyGun =
{
    float r = VR_ARENA_Rand();
    if(xWave <= 4 && r < 0.20) { return AR_FIST; }
    if(r < 0.60) { return WID_SHOTGUN; }
    if(r < 0.90) { return WID_NAILPISTOL; }
    return WID_AXE;
}

float(float xWave) VR_ARENA_IsBossWave =
{
    float every = VR_ARENA_Opt_BossEvery();
    if(every <= 0) { return FALSE; }
    return VR_ARENA_Mod(xWave, every) == 0;
}

float(float xWave) VR_ARENA_BossKind =
{
    if(xWave < AR_W_TIER2) { return AR_SP_ENFORCER; } // (a boss with a gun of its own before the classic bosses)
    if(xWave < AR_W_HKNIGHT) { return AR_SP_OGRE; }
    return AR_SP_HKNIGHT;
}

// Which of Quake's own monsters this wave sends, cycling the kinds unlocked by now (never one before its wave).
float(float xWave, float i) VR_ARENA_ClassicKind =
{
    float k = VR_ARENA_Mod(i, 4);
    if(k == 0) { return AR_SP_ZOMBIE; }
    if(k == 1) { return xWave >= AR_W_DOG ? AR_SP_DOG : AR_SP_ZOMBIE; }
    if(k == 2)
    {
        if(xWave >= AR_W_OGRE)   { return AR_SP_OGRE; }
        if(xWave >= AR_W_KNIGHT) { return AR_SP_KNIGHT; }
        return AR_SP_ZOMBIE;
    }
    if(xWave >= AR_W_HKNIGHT) { return AR_SP_HKNIGHT; }
    if(xWave >= AR_W_FIEND)   { return AR_SP_FIEND; }
    if(xWave >= AR_W_KNIGHT)  { return AR_SP_KNIGHT; }
    return AR_SP_ZOMBIE;
}

void(float xKind, float xGun, float xBoss) VR_ARENA_PlanAdd =
{
    if(ar_plan_n >= AR_PLAN_MAX) { return; } // (a wave is planned no thicker than this)
    ar_plan_kind[ar_plan_n] = xKind;
    ar_plan_gun[ar_plan_n]   = xGun;
    ar_plan_boss[ar_plan_n]  = xBoss;
    ar_plan_tier[ar_plan_n]  = VR_ARENA_WeaponTier(xKind, xGun);
    ar_plan_n = ar_plan_n + 1;
}

// The wave's plan, wave 1 to the waves past the table (13 and on: the same ladder, thicker).
void(float xWave) VR_ARENA_Plan =
{
    float boss = VR_ARENA_IsBossWave(xWave);
    float n;
    float i;
    float bk;

    ar_plan_n = 0;
    VR_ARENA_SeedWaves(xWave);

    if(boss)
    {
        // Its big one first, so a thick wave's plan never cuts the boss out.
        bk = VR_ARENA_BossKind(xWave);
        VR_ARENA_PlanAdd(bk, bk == AR_SP_ENFORCER ? VR_ARENA_PickGun(xWave) : 0, 1);
    }

    if(xWave == 1)
    {
        n = 1 + (VR_ARENA_Rand() < 0.5); // (one or two grunts, all of them unarmed)
        for(i = 0; i < n; i++) { VR_ARENA_PlanAdd(AR_SP_GRUNT, AR_FIST, 0); }
        return;
    }
    if(xWave == 2)
    {
        VR_ARENA_PlanAdd(AR_SP_GRUNT, AR_FIST, 0);
        VR_ARENA_PlanAdd(AR_SP_GRUNT, WID_NAILPISTOL, 0);
        return;
    }
    if(xWave == 3)
    {
        n = 3 + (VR_ARENA_Rand() < 0.5);
        for(i = 0; i < n; i++) { VR_ARENA_PlanAdd(AR_SP_GRUNT, VR_ARENA_PickEarlyGun(3), 0); }
        VR_ARENA_PlanAdd(AR_SP_ZOMBIE, 0, 0);
        return;
    }
    if(xWave == 4)
    {
        for(i = 0; i < 4; i++) { VR_ARENA_PlanAdd(AR_SP_GRUNT, VR_ARENA_PickEarlyGun(4), 0); }
        VR_ARENA_PlanAdd(AR_SP_KNIGHT, 0, 0);
        return;
    }
    if(xWave == 5) // (its boss, an enforcer with a gun, was planned above)
    {
        n = 3 + (VR_ARENA_Rand() < 0.5);
        for(i = 0; i < n; i++) { VR_ARENA_PlanAdd(AR_SP_GRUNT, VR_ARENA_PickGun(5), 0); }
        return;
    }
    if(xWave == 6)
    {
        for(i = 0; i < 3; i++) { VR_ARENA_PlanAdd(AR_SP_GRUNT, VR_ARENA_PickGun(6), 0); }
        VR_ARENA_PlanAdd(AR_SP_ENFORCER, VR_ARENA_PickGun(6), 0);
        VR_ARENA_PlanAdd(AR_SP_DOG, 0, 0);
        return;
    }
    if(xWave == 7)
    {
        n = 3 + (VR_ARENA_Rand() < 0.4);
        for(i = 0; i < n; i++) { VR_ARENA_PlanAdd(AR_SP_GRUNT, VR_ARENA_PickGun(7), 0); }
        VR_ARENA_PlanAdd(AR_SP_ENFORCER, VR_ARENA_PickGun(7), 0);
        VR_ARENA_PlanAdd(AR_SP_DOG, 0, 0);
        return;
    }
    if(xWave == 8)
    {
        for(i = 0; i < 3; i++) { VR_ARENA_PlanAdd(AR_SP_GRUNT, VR_ARENA_PickGun(8), 0); }
        VR_ARENA_PlanAdd(AR_SP_ENFORCER, VR_ARENA_PickGun(8), 0);
        VR_ARENA_PlanAdd(AR_SP_KNIGHT, 0, 0);
        VR_ARENA_PlanAdd(AR_SP_OGRE, 0, 0);
        return;
    }
    if(xWave == 9)
    {
        for(i = 0; i < 4; i++) { VR_ARENA_PlanAdd(AR_SP_GRUNT, VR_ARENA_PickGun(9), 0); }
        VR_ARENA_PlanAdd(AR_SP_ZOMBIE, 0, 0);
        VR_ARENA_PlanAdd(AR_SP_OGRE, 0, 0);
        return;
    }
    if(xWave == 10) // (its boss, an ogre, was planned above)
    {
        for(i = 0; i < 2; i++) { VR_ARENA_PlanAdd(AR_SP_ENFORCER, VR_ARENA_PickGun(10), 0); }
        for(i = 0; i < 2; i++) { VR_ARENA_PlanAdd(AR_SP_GRUNT, VR_ARENA_PickGun(10), 0); }
        VR_ARENA_PlanAdd(AR_SP_DOG, 0, 0);
        return;
    }
    if(xWave == 11)
    {
        for(i = 0; i < 4; i++) { VR_ARENA_PlanAdd(AR_SP_GRUNT, VR_ARENA_PickGun(11), 0); }
        VR_ARENA_PlanAdd(AR_SP_ENFORCER, VR_ARENA_PickGun(11), 0);
        for(i = 0; i < 2; i++) { VR_ARENA_PlanAdd(AR_SP_OGRE, 0, 0); }
        return;
    }
    if(xWave == 12)
    {
        for(i = 0; i < 4; i++) { VR_ARENA_PlanAdd(AR_SP_GRUNT, VR_ARENA_PickGun(12), 0); }
        VR_ARENA_PlanAdd(AR_SP_ENFORCER, VR_ARENA_PickGun(12), 0);
        VR_ARENA_PlanAdd(AR_SP_FIEND, 0, 0);
        VR_ARENA_PlanAdd(AR_SP_KNIGHT, 0, 0);
        return;
    }

    // 13 and on: the same ladder, thicker, a boss every vr_arena_boss_every waves.
    n = VR_ARENA_Clamp(4 + floor((xWave - 13) / 4), 1, 7) - (boss ? 1 : 0);
    for(i = 0; i < n; i++) { VR_ARENA_PlanAdd(AR_SP_GRUNT, VR_ARENA_PickGun(xWave), 0); }

    float nenf = VR_ARENA_Clamp(1 + floor((xWave - 13) / 8), 1, 3);
    for(i = 0; i < nenf; i++) { VR_ARENA_PlanAdd(AR_SP_ENFORCER, VR_ARENA_PickGun(xWave), 0); }

    float nclass = VR_ARENA_Clamp(1 + floor((xWave - 13) / 8), 1, 3);
    for(i = 0; i < nclass; i++) { VR_ARENA_PlanAdd(VR_ARENA_ClassicKind(xWave, i), 0, 0); }
}

// ---------------------------------------------------------------------------
// The players, the readout, the map's spots
// ---------------------------------------------------------------------------

entity() VR_ARENA_FindPlayer =
{
    entity p = find(world, classname, "player");
    while(p)
    {
        if(p.health > 0 && !p.deadflag) { return p; }
        p = find(p, classname, "player");
    }
    return world;
}

float() VR_ARENA_AnyPlayerUp = { return VR_ARENA_FindPlayer() != world; }

void(string xText) VR_ARENA_PrintAll =
{
    entity p = find(world, classname, "player");
    while(p) { centerprint(p, xText); p = find(p, classname, "player"); }
}

// The map's spots of a class (info_arena_spawn, info_arena_item), round robin; none: world.
entity(string xClassname, float xIndex) VR_ARENA_MapSpot =
{
    entity e = find(world, classname, xClassname);
    float total = 0;
    while(e) { total = total + 1; e = find(e, classname, xClassname); }
    if(!total) { return world; }

    float want = VR_ARENA_Mod(xIndex, total);
    e = find(world, classname, xClassname);
    while(want > 0) { e = find(e, classname, xClassname); want = want - 1; }
    return e;
}

// How many of the map's spawn places are of a kind ("kind" 1 the floor, 2 the high places).
float(float xWant) VR_ARENA_SpotCount =
{
    float total = 0;
    entity e = find(world, classname, "info_arena_spawn");
    while(e)
    {
        if(e.kind == xWant) { total = total + 1; }
        e = find(e, classname, "info_arena_spawn");
    }
    return total;
}

// The xIndex'th place of a kind: world when the map has no more of it (its places are used up, not walked twice).
entity(float xWant, float xIndex) VR_ARENA_SpawnSpot =
{
    float total = VR_ARENA_SpotCount(xWant);
    if(!total || xIndex >= total) { return world; }

    entity e = find(world, classname, "info_arena_spawn");
    while(e)
    {
        if(e.kind == xWant)
        {
            if(xIndex <= 0) { return e; }
            xIndex = xIndex - 1;
        }
        e = find(e, classname, "info_arena_spawn");
    }
    return world;
}

// A ring around the player, for a map with no info_arena_spawn / info_arena_item of its own.
vector(float i) VR_ARENA_RingDir =
{
    float k = VR_ARENA_Mod(i, AR_RING_MAX);
    if(k == 0) { return '1 0 0'; }
    if(k == 1) { return '-0.5 0.87 0'; }
    if(k == 2) { return '-1 0 0'; }
    if(k == 3) { return '-0.5 -0.87 0'; }
    if(k == 4) { return '0.5 -0.87 0'; }
    if(k == 5) { return '0.5 0.87 0'; }
    if(k == 6) { return '0.7 0.7 0'; }
    return '-0.7 -0.7 0';
}

// A spot used twice in one wave is stepped aside, so two foes never spawn inside one another. The first is the
// place as the map drew it; then the four diagonals a little over a monster's box (its own box is 32 wide) off it.
vector(float i) VR_ARENA_Aside =
{
    float k = VR_ARENA_Mod(i, AR_ASIDE_MAX);
    if(k == 0) { return '0 0 0'; }
    if(k == 1) { return AR_ASIDE_STEP * '1 1 0'; }
    if(k == 2) { return AR_ASIDE_STEP * '-1 1 0'; }
    if(k == 3) { return AR_ASIDE_STEP * '1 -1 0'; }
    return AR_ASIDE_STEP * '-1 -1 0';
}

// A place already holding one of this wave's foes is stepped aside. The step is small on purpose (a monster's own
// box is 32 wide): from a tower top, a wall walk or the octagon's cut corner it stays on the ground it belongs to.
float(vector o) VR_ARENA_PlaceTaken =
{
    entity e = findflags(world, ar_slot, ar_wave);
    while(e)
    {
        if(e.health > 0)
        {
            vector d = e.origin - o;
            d_z = 0; // (apart on the ground it stands on: a foe on the floor below is not in this place)
            if(vlen(d) < AR_PLACE_GAP) { return 1; }
        }
        e = findflags(e, ar_slot, ar_wave);
    }
    return 0;
}

entity(float xIndex) VR_ARENA_ItemSpot { return VR_ARENA_MapSpot("info_arena_item", xIndex); }

// Is there ground under a place, read off the brushes: 600 units down, this wave's own foes not counted. A place
// inside a wall counts as having none (the trace starts inside solid), and a place with nothing under it is a foe
// falling out of the map.
float(vector o) VR_ARENA_HasFloor =
{
    traceline(o + '0 0 4', o + '0 0 -600', TRUE, world);
    return !trace_allsolid && !trace_startsolid && trace_fraction < 1;
}

// Where this wave's foe comes in. Its own kind of place first: the map's high places (kind 2: a tower top, a wall
// walk) for the fiend and the enforcer, its floor places (kind 1) for everything that walks. The places are gone
// through in order from where this wave starts and each one is used at most once, so 12 foes do not arrive in 4
// corners; a place already holding a foe of this wave is passed over rather than shared. A high foe whose places are
// all taken comes in on the floor places instead of being stepped off its ledge, and a map with no places of its own
// gets the ring around the player, stepped aside until it is free and never over a hole or into a wall.
vector(float xHigh) VR_ARENA_SpawnPlace =
{
    float want = xHigh ? 2 : 1;
    float total = VR_ARENA_SpotCount(want);

    float used;
    if(xHigh) { used = ar_used_high; ar_used_high = ar_used_high + 1; }
    else { used = ar_used_ground; ar_used_ground = ar_used_ground + 1; }

    float k;
    for(k = 0; k < total; k++)
    {
        entity e = VR_ARENA_SpawnSpot(want, VR_ARENA_Mod(used + ar_wave + k, total));
        if(!e) { break; } // (out of this kind of place: the ring below)

        vector here = e.origin;
        if(xHigh && e.kind == 2) { here_z = here_z + 96; } // (over a high place: it drops onto the ledge it belongs to)
        if(!VR_ARENA_PlaceTaken(here))
        {
            ar_spot_next = ar_spot_next + 1;
            return here;
        }
    }

    // Every high place is holding one of this wave's foes already: better its ground places than a step off a ledge.
    if(xHigh && VR_ARENA_SpotCount(1) > 0) { return VR_ARENA_SpawnPlace(0); }

    entity p = VR_ARENA_FindPlayer();
    vector base = ar_mgr.origin;
    if(p != world) { base = p.origin; }
    vector ring = base + AR_RING_R * VR_ARENA_RingDir(ar_spot_next);

    float k2;
    vector o = ring;
    float ground = FALSE; // (any place with ground under it: the last resort)
    float free = FALSE;   // (a place with ground under it and no foe of this wave on it)
    for(k2 = 0; k2 < AR_RING_MAX && !free; k2++)
    {
        vector r = (k2 == 0) ? ring : base + AR_RING_R * VR_ARENA_RingDir(ar_spot_next + k2);
        float t;
        for(t = 0; t < AR_ASIDE_MAX; t++)
        {
            vector c = r + VR_ARENA_Aside(t);
            if(!VR_ARENA_HasFloor(c)) { continue; }
            if(!ground) { ground = TRUE; o = c; }
            if(!VR_ARENA_PlaceTaken(c)) { o = c; free = TRUE; break; }
        }
    }

    ar_spot_next = ar_spot_next + 1;
    return o;
}

// Where a wave clear's medkit or armour lands: the map's info_arena_item, or the ring off the player.
vector(float i) VR_ARENA_DropPlace =
{
    entity e = VR_ARENA_ItemSpot(ar_item_next);
    vector o;

    if(e)
    {
        o = e.origin;
    }
    else
    {
        entity p = VR_ARENA_FindPlayer();
        vector base = ar_mgr.origin;
        if(p != world) { base = p.origin; }
        o = base + AR_DROP_R * VR_ARENA_RingDir(i);
    }

    ar_item_next = ar_item_next + 1;
    return o;
}

// ---------------------------------------------------------------------------
// A wave's monster: its place, its gun, its kind's own spawn
// ---------------------------------------------------------------------------

string(float xKind) VR_ARENA_ClassName =
{
    if(xKind == AR_SP_ZOMBIE)   { return "monster_zombie"; }
    if(xKind == AR_SP_DOG)      { return "monster_dog"; }
    if(xKind == AR_SP_KNIGHT)   { return "monster_knight"; }
    if(xKind == AR_SP_OGRE)     { return "monster_ogre"; }
    if(xKind == AR_SP_FIEND)    { return "monster_demon1"; }
    if(xKind == AR_SP_HKNIGHT)  { return "monster_hell_knight"; }
    if(xKind == AR_SP_ENFORCER) { return "monster_enforcer"; } // (its armed version puts its own classname on: VR_AM_Spawn)
    return "monster_army";
}

void(float xKind) VR_ARENA_CallSpawn =
{
    if(xKind == AR_SP_GRUNT)    { monster_vr_grunt(); return; }    // (vr_armedgrunt.qc: a grunt holding one of the player's weapons)
    if(xKind == AR_SP_ENFORCER) { monster_vr_enforcer(); return; }
    if(xKind == AR_SP_ZOMBIE)   { monster_zombie(); return; }
    if(xKind == AR_SP_DOG)      { monster_dog(); return; }
    if(xKind == AR_SP_KNIGHT)   { monster_knight(); return; }
    if(xKind == AR_SP_OGRE)     { monster_ogre(); return; }
    if(xKind == AR_SP_FIEND)    { monster_demon1(); return; }
    monster_hell_knight();
}

float(float xKind) VR_ARENA_ArmsHumanoid = { return xKind == AR_SP_GRUNT || xKind == AR_SP_ENFORCER; }

// The wave's monster at its place: an armed humanoid holding its plan's gun (or its fists), or one of Quake's
// own monsters. Its kind's spawn function does the rest (its health, its sounds, its pains, its death).
// world when it could not be put in the field at all (its kind's spawn held it back): the spawner tries again.
entity(float xKind, float xGun, float xTier, float xBoss) VR_ARENA_SpawnMonster =
{
    entity manager = self;
    entity m = spawn();
    vector o = VR_ARENA_SpawnPlace(xKind == AR_SP_FIEND || xKind == AR_SP_ENFORCER);
    self = m;

    m.classname = VR_ARENA_ClassName(xKind);
    m.ar_slot = ar_wave;
    m.ar_tier = xTier + (xBoss ? AR_BOSS_MARK : 0);
    m.angles = '0 0 0';
    if(VR_ARENA_ArmsHumanoid(xKind))
    {
        m.weapon = xGun; // (monster_vr_grunt's "weapon": a WID_*, -1 its fists; its own spawn clears it)
        m.weapon2 = 0;
        m.weaponflags = 0;
        m.weaponflags2 = 0;
    }
    entity p = VR_ARENA_FindPlayer();
    if(p != world) { m.angles = vectoangles(p.origin - o); }
    setorigin(m, o + '0 0 32');

    VR_ARENA_CallSpawn(xKind); // (the monster's own spawn, on self)

    if(!self.modelindex && !self.MG_deferred)
    {
        remove(self);
        self = manager;
        return world;
    }

    if(xBoss)
    {
        // A boss is a thicker one of its kind (what else a boss does is worker W3's boss pass).
        self.health = floor(self.health * VR_ARENA_Opt_BossHealth()) + 1;
        dprint(sprintf("arena: boss kind %g health %g tier %g\n", xKind, self.health, xTier));
    }

    dprint(sprintf("arena: foe kind %g gun %g tier %g at %g %g %g\n", xKind, xGun, xTier, o_x, o_y, o_z));

    // Its kind's own start runs as it always does (monsters.qc's walkmonster_start_go: its takedamage, its full
    // health, its AI): a wave monster must not be started another way, or it never becomes something you can hit.
    // VR_ARENA_HuntPlayer gives it the player once that start is through.

    self = manager;
    return m;
}

// ---------------------------------------------------------------------------
// A wave clear's drops: items.qc's medkit and armour makers, made grabbable as they are
// ---------------------------------------------------------------------------

// xWhat: 1 a medkit, 2 the light armour, 3 the heavier one.
entity(float xWhat, vector xAt) VR_ARENA_SpawnDrop =
{
    entity manager = self;
    entity it = spawn();
    self = it;

    if(xWhat == 1)
    {
        it.classname = "item_health";
        it.spawnflags = 0; // (items.qc's plain medkit: 25 health, its own grabbable placement)
        setorigin(it, xAt + '0 0 8');
        item_health();
    }
    else
    {
        if(xWhat == 3) { it.classname = "item_armor2"; }
        else { it.classname = "item_armor1"; }
        setorigin(it, xAt + '0 0 8');
        if(xWhat == 3) { item_armor2(); } else { item_armor1(); }
        self.armortype  = xWhat == 3 ? 0.6 : 0.3; // (its touch's own values: the map's keys, for a made item)
        self.armorvalue = xWhat == 3 ? 150 : 100;
    }

    if(!self.modelindex)
    {
        remove(self);
        self = manager;
        return world;
    }

    it.ar_drop = xWhat == 1 ? 1 : 2;
    self = manager;
    return it;
}

void(float xWave) VR_ARENA_Drops =
{
    float n = VR_ARENA_MedkitCount(xWave);
    float armour = VR_ARENA_ArmourKind(xWave);
    float i;

    ar_drop_health = 0;
    ar_drop_armour = 0;

    for(i = 0; i < n; i++)
    {
        if(VR_ARENA_SpawnDrop(1, VR_ARENA_DropPlace(i))) { ar_drop_health = ar_drop_health + 1; }
    }
    if(armour)
    {
        if(VR_ARENA_SpawnDrop(armour + 1, VR_ARENA_DropPlace(n + 1))) { ar_drop_armour = 1; }
    }
}

// ---------------------------------------------------------------------------
// The field: who of the wave is up, who fell, and what that is worth
// ---------------------------------------------------------------------------

float() VR_ARENA_CountAlive =
{
    if(ar_wave <= 0) { return 0; }

    float n = 0;
    entity e = findflags(world, ar_slot, ar_wave);
    while(e)
    {
        if(e.health > 0) { n = n + 1; }
        e = findflags(e, ar_slot, ar_wave);
    }
    return n;
}

// A wave foe whose kind's own start is through (monsters.qc: its takedamage is on) and who has not spotted anyone
// yet: it is given the player, so a wave comes at you instead of standing where it landed. Its own run takes over
// from there (ai.qc's FoundTarget puts it on its th_run).
void() VR_ARENA_HuntPlayer =
{
    if(ar_wave <= 0) { return; }

    entity p = VR_ARENA_FindPlayer();
    if(p == world) { return; }

    entity e = findflags(world, ar_slot, ar_wave);
    while(e)
    {
        if(e.health > 0 && e.takedamage && e.enemy == world && e.th_run)
        {
            e.enemy = p;
            e.think = FoundTarget;
            e.nextthink = time + 0.1 + 0.1 * VR_ARENA_Rand();
        }
        e = findflags(e, ar_slot, ar_wave);
    }
}

// combat.qc (after MGH_Kill): this monster fell. Its score, once (its ar_slot flips negative).
void() VR_ARENA_MonsterDie =
{
    if(!ar_mgr) { return; }
    if(ar_wave <= 0 || self.ar_slot != ar_wave) { return; }

    self.ar_slot = -ar_wave;
    ar_kills = ar_kills + 1;
    ar_wave_kills = ar_wave_kills + 1;
    ar_score = ar_score + VR_ARENA_KillScore(VR_ARENA_TierOf(self.ar_tier), self.ar_tier >= AR_BOSS_MARK);
}

void(float xWave) VR_ARENA_ClearField = // (a stalled wave, and the tests' jump: its monsters leave)
{
    if(xWave <= 0) { return; }
    entity e = findflags(world, ar_slot, xWave);
    while(e)
    {
        entity next = findflags(e, ar_slot, xWave);
        remove(e);
        e = next;
    }
}

// ---------------------------------------------------------------------------
// The director's cycle
// ---------------------------------------------------------------------------

void() VR_ARENA_WaveStart =
{
    ar_wave = ar_wave + 1;
    VR_ARENA_Plan(ar_wave);

    ar_plan_left = ar_plan_n;
    ar_plan_live = 0;
    ar_wave_kills = 0;
    ar_next_spawn = 0;
    ar_wave_at = time;
    ar_used_ground = 0;
    ar_used_high = 0;
    ar_spot_next = ar_spot_next + AR_ASIDE_MAX; // (a wave begins at another place on the map's spots)

    VR_ARENA_PrintAll(sprintf("ARENA - Wave %g: %g foes (score %g)\n", ar_wave, ar_plan_n, ar_score));
    if(VR_ARENA_IsBossWave(ar_wave)) { VR_ARENA_PrintAll("A BOSS is with this wave\n"); }
    dprint(sprintf("arena: wave %g score %g alive %g\n", ar_wave, ar_score, ar_plan_n));

    self.think = VR_ARENA_ManagerThink;
    self.nextthink = time + 0.2;
}

void() VR_ARENA_WaveClear =
{
    float w = ar_wave;

    ar_score = ar_score + VR_ARENA_WaveBonus(w);
    VR_ARENA_Drops(w);

    VR_ARENA_PrintAll(sprintf("ARENA - Wave %g clear: %g foes down (score %g)\n", w, ar_wave_kills, ar_score));
    dprint(sprintf("arena: wave %g clear kills %g medkits %g armour %g score %g\n", w, ar_wave_kills,
                   ar_drop_health, ar_drop_armour, ar_score));

    VR_ARK_WaveDone(w); // QVR: the boss wave's clear offers the player his choice of perks (vr_arena_perks.qc)

    self.think = VR_ARENA_WaveStart;
    self.nextthink = time + VR_ARENA_Opt_Delay();
}

void() VR_ARENA_ManagerThink =
{
    float alive = VR_ARENA_CountAlive();
    if(alive > ar_alive_peak) { ar_alive_peak = alive; }

    // The players, all of them down: their score stays, a fresh wave 1 comes (as the Machine Games' horde does).
    if(!VR_ARENA_AnyPlayerUp())
    {
        ar_down = 1;
        ar_fallen = find(world, classname, "player");
        if(ar_fallen) { centerprint(ar_fallen, sprintf("ARENA - you fell at wave %g (score %g)\n", ar_wave, ar_score)); }
        dprint(sprintf("arena: fell wave %g score %g\n", ar_wave, ar_score));
        self.think = VR_ARENA_RestartThink;
        self.nextthink = time + 10;
        return;
    }

    // Its foes trickle in as the field frees up: vr_arena_max_alive is the most up at once.
    if(ar_plan_left > 0 && alive < VR_ARENA_Opt_MaxAlive() && time >= ar_next_spawn)
    {
        float i = ar_plan_n - ar_plan_left;
        entity m = VR_ARENA_SpawnMonster(ar_plan_kind[i], ar_plan_gun[i], ar_plan_tier[i], ar_plan_boss[i]);
        if(m)
        {
            ar_plan_left = ar_plan_left - 1;
            ar_plan_live = ar_plan_live + 1;
        }
        ar_next_spawn = time + AR_SPAWN_GAP + VR_ARENA_Rand(); // (one per beat at most, then this apart)
    }

    if(ar_plan_left == 0 && VR_ARENA_CountAlive() == 0)
    {
        VR_ARENA_WaveClear();
        return;
    }

    VR_ARENA_HuntPlayer(); // (a wave foe that has come to and has not spotted anyone: it comes at you)

    if(time - ar_wave_at > AR_STALL) // (a foe that could not be reached: the wave lets go and the arena goes on)
    {
        dprint(sprintf("arena: wave %g stalled alive %g\n", ar_wave, VR_ARENA_CountAlive()));
        VR_ARENA_ClearField(ar_wave);
        VR_ARENA_WaveClear();
        return;
    }

    self.nextthink = time + AR_TICK;
}

void() VR_ARENA_RestartThink =
{
    // A fresh run: the map again as it was, wave 1, the players at their fists again.
    ar_down = 0;
    serverflags = ar_serverflags0;
    localcmd("restart fresh\n");
}

// The players put down to their fists, once a run begins (vr_trailer.qc's own strip, every rung of the ladder).
void() VR_ARENA_StripPlayers =
{
    entity p = find(world, classname, "player");
    while(p)
    {
        float wid;
        for(wid = WID_FIST + 1; wid <= WID_LAST; wid++)
        {
            VR_Trailer_TakeWeapon(p, wid);
        }
        p = find(p, classname, "player");
    }
}

// ---------------------------------------------------------------------------
// world.qc's two hooks
// ---------------------------------------------------------------------------

void() VR_ARENA_WorldSpawn =
{
    if(!VR_ARENA_On()) { return; }
    if(deathmatch) { return; } // (the arena is one player's run up the ladder)

    ar_mgr = spawn();
    ar_mgr.classname = "vr_arena_manager";
    ar_mgr.origin = world.origin;
    ar_serverflags0 = serverflags;

    ar_wave = 0;
    ar_score = 0;
    ar_kills = 0;
    ar_wave_kills = 0;
    ar_plan_n = 0;
    ar_plan_left = 0;
    ar_plan_live = 0;
    ar_alive_peak = 0;
    ar_drop_health = 0;
    ar_drop_armour = 0;
    ar_spot_next = 0;
    ar_item_next = 0;
    ar_used_ground = 0;
    ar_used_high = 0;
    ar_stripped = 0;
    ar_down = 0;

    MGH_PrecacheMonsters(); // (vr_mg_horde.qc: the models and sounds of the monsters the waves bring)

    ar_mgr.think = VR_ARENA_WaveStart;
    ar_mgr.nextthink = time + 1; // (after the map's own spawns, so its info_arena_spawn points are known)

    dprint(sprintf("arena: %s on (max alive %g, boss every %g, seed %g)\n", mapname, VR_ARENA_Opt_MaxAlive(),
                   VR_ARENA_Opt_BossEvery(), cvar("vr_arena_seed")));
}

void() VR_ARENA_Frame =
{
    if(!ar_mgr) { return; }
    if(!ar_stripped)
    {
        entity p = find(world, classname, "player");
        if(!p) { return; } // (its clients are not in yet)
        ar_stripped = 1;
        VR_ARENA_StripPlayers();
        VR_ARENA_PrintAll("ARENA - unarmed, wave 1 coming\n");
    }
}

// ---------------------------------------------------------------------------
// The tests' jump: the arena runs the wave after this one next (vr_arena_test.qc)
// ---------------------------------------------------------------------------

void(float xWave) VR_ARENA_FastForward =
{
    if(!ar_mgr) { return; }
    VR_ARENA_ClearField(ar_wave);
    ar_plan_left = 0;
    ar_wave = xWave - 1;
    ar_mgr.think = VR_ARENA_WaveStart;
    ar_mgr.nextthink = time + 0.5;
}

// ---------------------------------------------------------------------------
// The map's spots (vr_arena_gen.py's / the arena map's entities)
// ---------------------------------------------------------------------------

/*QUAKED info_arena_spawn (0 .5 .8) (-16 -16 -16) (16 16 16)
A wave's foe comes in here. "kind": 1 (or unset) a ground point, 2 a high one: a wave's ranged and flying foes
come in above the floor there. The waves use these round robin; a point used twice in a wave is stepped aside.
With none in the map, its foes come in a ring 400 units off the player instead.
*/
void() info_arena_spawn =
{
    if(!self.kind) { self.kind = 1; }
}

/*QUAKED info_arena_item (0 .8 .5) (-16 -16 0) (16 16 32)
Where a wave clear's medkit or armour lands, round robin over the map's item points.
With none in the map, the drops land in a ring 220 units off the player instead.
*/
void() info_arena_item =
{
}
