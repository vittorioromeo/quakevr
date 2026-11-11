//
// vr_arena_director.qc -- Survival Arena: the budget director's waves (vr_arena_director 1; docs/vr-port/ARENA_DIRECTOR.md).
//
// The engine plans each wave (Quake/vr/vr_arenadirector.cpp, off quakevr/data/arena_director.txt): its theme, its foes
// (a body, a weapon row, armour, a boss mark, the surge it belongs to, its cost) and its cadence. This file turns the plan
// into vr_arena.qc's plan arrays (VR_ARENA_DirPlan, the same ones the classic waves and the presets fill) and paces it:
//   - a spawn GROUP comes in (1 to N of the wave's own, N the theme's), then the next one waits its base gap (the theme's,
//     divided by the difficulty) stretched by the threat still up: gap x (1 + stretch x threat up / the wave's threat),
//     doubled again past `soft` of it. A player who finds and kills each arrival faces them a few at a time; a slow one
//     piles them up;
//   - a SURGE (the held-back budget) is announced ("SURGE!", "AMBUSH!": a banner and a horn) and comes in at once from
//     places spread round the player, behind him first in an ambush;
//   - a boss is announced as ever and comes in alone;
//   - no alive cap as a pacing tool: vr_arena_alive_cap (20) is a safety cap for the frame rate only.
// Kills pay by cost (vr_arena_score_kill x cost / 8; a quick kill, within the data file's quick_time of its arrival, pays
// quick_pay more), and a clear's supplies are the engine's supply budget's (VR_ARENA_DirDrops).
//

// arenadirget's fields (vr_arenadirector.hpp FoeField, WaveField).
float AR_DF_KIND = 0;
float AR_DF_ROW = 1;
float AR_DF_ROW2 = 2;
float AR_DF_ARMOUR = 3;
float AR_DF_PLATES = 4;
float AR_DF_BOSS = 6;
float AR_DF_SURGE = 7;
float AR_DF_COST = 8;
float AR_DW_THEME = 0;
float AR_DW_SPENT = 2;
float AR_DW_SURGES = 3;
float AR_DW_GAP = 4;
float AR_DW_GMIN = 5;
float AR_DW_GMAX = 6;
float AR_DW_STRETCH = 7;
float AR_DW_SOFT = 8;
float AR_DW_SPREAD = 9;
float AR_DW_QTIME = 11;
float AR_DW_QPAY = 12;
float AR_DW_WARN = 13;
float AR_DW_AMBUSH = 14;
float AR_DW_GAPREF = 15;

float ar_dir;          // 1: this run plays the budget director's waves
float ar_dir_threat;   // this wave's planned threat
float ar_dir_last;     // the time this wave's last spawn group came in
float ar_dir_gap;      // ... and its base gap to the next (rolled, by the difficulty)
float ar_dir_warn_at;  // a warned group (a surge, a boss) comes in at this time (0: none warned)
float ar_dir_spread;   // 1: the group being placed is spread round the player (VR_ARENA_DirSector)
float ar_dir_ambush;   // 1: ... from behind him first
float ar_dir_sectors;  // the sixths round him this group used (bits; 1: ahead)
float ar_dir_hp0;      // his health and half his armour as the wave began (the supplies' lean)
float ar_dir_quick;    // this wave's quick kills,
float ar_dir_quick_pts; // ... and what they paid more
float ar_dir_groups;   // this wave's spawn groups so far (the tests, the log)
float ar_dir_surged;   // ... its surges come in
float ar_dir_group_max; // ... the biggest group
float ar_dir_group_cost; // the group being brought in: its threat (its gap's length)
float ar_dir_drop_power; // a clear's power-up (its SupplyCode, 0 none),
float ar_dir_drop_mega;  // ... its megahealths,
float ar_dir_drop_ammo;  // ... its ammo boxes

float() VR_ARENA_DirOn = { return ar_dir && ar_preset_n <= 0; }

// A run begins (VR_ARENA_WorldSpawn): the engine's planner started on this run's seed and the rows this game has.
void() VR_ARENA_DirStart =
{
    ar_dir = 0;
    if(cvar_hget(cvarh_vr_arena_director) == 0 || ar_preset_n > 0) { return; }
    float lo = 0;
    float hi = 0;
    float bit = 1;
    float i;
    for(i = 0; i < AR_GUN_N; i++)
    {
        if(i == 17) { bit = 1; }
        if(VR_ARENA_GunOk(i))
        {
            if(i < 17) { lo = lo + bit; } else { hi = hi + bit; }
        }
        bit = bit * 2;
    }
    float seed = cvar_hget(cvarh_vr_arena_seed);
    if(seed <= 0) { seed = 1 + floor(29998 * random()); } // (an unseeded run: its own waves, one base for the run)
    ar_dir = arenadirstart(VR_ARENA_SkillLevel(), seed, lo, hi);
    if(!ar_dir) { dprint("arena: budget director: no data/arena_director.txt: the classic waves\n"); return; }
    dprint(sprintf("arena: budget director on, seed %g\n", seed));
    // (the supplies only the director drops)
    precache_model("maps/b_bh10.bsp");
    precache_model("maps/b_bh100.bsp");
    precache_model("maps/b_shell0.bsp");
    precache_model("progs/quaddama.mdl");
    precache_model("progs/invisibl.mdl");
    precache_model("progs/invulner.mdl");
    precache_sound("items/r_item1.wav");
    precache_sound("items/r_item2.wav");
    precache_sound("items/damage.wav");
    precache_sound("items/damage2.wav");
    precache_sound("items/damage3.wav");
    precache_sound("items/inv1.wav");
    precache_sound("items/inv2.wav");
    precache_sound("items/inv3.wav");
    precache_sound("items/protect.wav");
    precache_sound("items/protect2.wav");
    precache_sound("items/protect3.wav");
    precache_sound("weapons/lock4.wav");
}

// A plan's armour plates: the cuirass and the helmet, then n - 2 of the eight limb plates at random (exactly).
float(float n) VR_ARENA_DirMask =
{
    float mask = 3;
    float need = clamp(n, 2, 10) - 2;
    float i;
    for(i = 2; i < 10 && need > 0; i++)
    {
        if(VR_ARENA_Rand() * (10 - i) < need)
        {
            mask = mask | VR_AgArmor_Bit(i);
            need = need - 1;
        }
    }
    return mask;
}

// The wave's plan from the engine's, in its order (a boss amid the wave's own, each surge after its share of them).
void(float xWave) VR_ARENA_DirPlan =
{
    VR_ARENA_SeedWaves(xWave + 3000); // (a run row's gun and the plates: their own stream)
    ar_plan_n = 0;
    ar_trickle_n = 0;
    float n = arenadirplan(xWave);
    float i;
    for(i = 0; i < n && ar_plan_n < AR_PLAN_MAX; i++)
    {
        float kind = arenadirget(0, i, AR_DF_KIND);
        float row = arenadirget(0, i, AR_DF_ROW);
        float gun = 0;
        if(row >= 0) { gun = VR_ARENA_GunDeal(row); }
        else if(row == -1) { gun = AR_FIST; }
        float j = ar_plan_n;
        VR_ARENA_PlanAdd(kind, gun, arenadirget(0, i, AR_DF_BOSS));
        float row2 = arenadirget(0, i, AR_DF_ROW2);
        float am = VR_ARENA_ArmedRow(kind);
        if(row2 >= 0 && gun > 0 && am >= 0)
        {
            float gun2 = VR_ARENA_GunDeal(row2);
            if(VR_AM_UiPairs(VR_AM_UiWeapon(am, gun), VR_AM_UiWeapon(am, gun2))) { ar_plan_gun2[j] = gun2; }
        }
        float arm = arenadirget(0, i, AR_DF_ARMOUR);
        if(arm > 0 && VR_ARENA_ArmsHumanoid(kind))
        {
            ar_plan_armour[j] = arm;
            ar_plan_amask[j] = VR_ARENA_DirMask(arenadirget(0, i, AR_DF_PLATES));
        }
    }
    ar_dir_threat = max(1, arenadirget(1, 0, AR_DW_SPENT));
}

// The theme's word ("" with the banner off or the director off).
string(float xWave) VR_ARENA_DirTheme =
{
    if(!VR_ARENA_DirOn() || cvar_hget(cvarh_vr_arena_theme_banner) == 0) { return ""; }
    arenadirplan(xWave);
    return arenadirname(0, 0);
}

// The wave begins (VR_ARENA_WaveStart): its cadence from the start, the first group at once.
void() VR_ARENA_DirWaveStart =
{
    ar_pool = clamp(cvar_hget(cvarh_vr_arena_alive_cap), 1, AR_PLAN_MAX); // (a safety cap, not the pacing)
    ar_burst_left = 0;
    ar_dir_last = time - 1000;
    ar_dir_gap = 0;
    ar_dir_warn_at = 0;
    ar_dir_quick = 0;
    ar_dir_quick_pts = 0;
    ar_dir_groups = 0;
    ar_dir_surged = 0;
    ar_dir_group_max = 0;
    entity p = VR_Player();
    ar_dir_hp0 = p ? max(1, p.health + 0.5 * p.armorvalue) : 100;
}

// The threat this wave has up now (its foes' costs).
float() VR_ARENA_DirThreatUp =
{
    float up = 0;
    entity e = findflags(world, ar_slot, ar_wave);
    while(e)
    {
        if(e.health > 0 && e.ar_slot == ar_wave) { up = up + e.ar_cost; }
        e = findflags(e, ar_slot, ar_wave);
    }
    return up;
}

// The gap's stretch: 1 + stretch x the threat up / the wave's, doubled again past `soft` of it.
float() VR_ARENA_DirStretch =
{
    float share = VR_ARENA_DirThreatUp() / max(1, ar_dir_threat);
    float s = 1 + arenadirget(1, 0, AR_DW_STRETCH) * share;
    if(share > arenadirget(1, 0, AR_DW_SOFT)) { s = s * 2; }
    return s;
}

// A spawn group came in: the next one's base gap (the theme's, by the difficulty, give or take a fifth), longer for a
// big group (times its threat / gap_ref, at least 1): a player who downs it quickly waits for the next, a slow one
// still has it up when the next comes.
void(float xN) VR_ARENA_DirGroupDone =
{
    ar_dir_last = time;
    ar_dir_gap = VR_ARENA_SkillTime(arenadirget(1, 0, AR_DW_GAP), AR_TICK) * (0.8 + 0.4 * VR_ARENA_Rand()) *
                 max(1, ar_dir_group_cost / max(1, arenadirget(1, 0, AR_DW_GAPREF)));
    ar_dir_group_cost = 0;
    ar_dir_sectors = 0;
    ar_dir_groups = ar_dir_groups + 1;
    ar_dir_group_max = max(ar_dir_group_max, xN);
    dprint(sprintf("arena: group %g of %g foes, next gap %.1f s (x%.2f now), threat up %g of %g\n", ar_dir_groups, xN,
                   ar_dir_gap, VR_ARENA_DirStretch(), VR_ARENA_DirThreatUp(), ar_dir_threat));
}

// A place's sixth round the player (0 ahead, 3 behind), and whether a spread group or an ambush may use it now (it is
// marked used when it may): a spread group takes a sixth once, an ambush the three behind him first.
float(vector xAt) VR_ARENA_DirSector =
{
    if(!ar_dir_spread) { return TRUE; }
    entity p = VR_PlayerAlive();
    if(p == world) { return TRUE; }
    float yaw = anglemod(vectoyaw(xAt - p.origin) - p.v_angle_y + 30);
    float s = floor(yaw / 60);
    float bit = 1;
    float k;
    for(k = 0; k < s; k++) { bit = bit * 2; }
    if(ar_dir_sectors & bit) { return FALSE; }
    if(ar_dir_ambush && !(ar_dir_sectors & (4 | 8 | 16)) && (s < 2 || s > 4)) { return FALSE; }
    ar_dir_sectors = ar_dir_sectors | bit;
    return TRUE;
}

// Plan foe i is of the warned group (the boss, or that surge).
float(float i, float xBoss, float xSurge) VR_ARENA_DirSameGroup =
{
    if(xBoss) { return ar_plan_boss[i] > 0; }
    return arenadirget(0, i, AR_DF_SURGE) == xSurge;
}

// The director's beat's spawning (VR_ARENA_ManagerThink, in place of the classic burst and gaps): the foes up after it.
float(float xAlive) VR_ARENA_DirCadence =
{
    if(ar_plan_left <= 0) { return xAlive; }
    float cap = ar_pool;
    float i = ar_plan_n - ar_plan_left;
    float boss = (ar_plan_boss[i] > 0);
    float surge = arenadirget(0, i, AR_DF_SURGE);
    float n = 0;

    if(ar_dir_warn_at > 0)
    {
        if(time < ar_dir_warn_at) { return xAlive; }
        ar_dir_warn_at = 0;
        ar_dir_spread = (boss == 0);
        ar_dir_ambush = ((boss == 0) && (arenadirget(1, 0, AR_DW_AMBUSH) != 0));
        while(ar_plan_left > 0 && xAlive < cap)
        {
            i = ar_plan_n - ar_plan_left;
            if(!VR_ARENA_DirSameGroup(i, boss, surge)) { break; }
            if(!VR_ARENA_SpawnNext()) { break; }
            ar_dir_group_cost = ar_dir_group_cost + arenadirget(0, i, AR_DF_COST);
            xAlive = xAlive + 1;
            n = n + 1;
        }
        ar_dir_spread = 0;
        ar_dir_ambush = 0;
        if((n > 0) && (boss == 0)) { ar_dir_surged = ar_dir_surged + 1; }
        i = ar_plan_n - ar_plan_left;
        if(ar_plan_left > 0 && VR_ARENA_DirSameGroup(i, boss, surge))
        {
            ar_dir_warn_at = time + AR_TICK; // (the rest of it a beat later: the cap, a place not free)
        }
        if(n > 0) { VR_ARENA_DirGroupDone(n); }
        return xAlive;
    }

    if(time < ar_dir_last + ar_dir_gap * VR_ARENA_DirStretch()) { return xAlive; }

    if(boss || (surge > 0))
    {
        float warn = AR_BOSS_WARN;
        if(!boss) { warn = arenadirget(1, 0, AR_DW_WARN); }
        if(boss) { VR_ARENA_ShowAll(sprintf("A BOSS IS COMING\n\n%s\n", VR_ARENA_BossName(ar_wave)), AR_BOSS_WARN); }
        else
        {
            float ambush = (arenadirget(1, 0, AR_DW_AMBUSH) != 0);
            string say = "SURGE!\n\nthey come from all sides\n";
            if(ambush) { say = "AMBUSH!\n\nbehind you\n"; }
            VR_ARENA_ShowAll(say, max(1, warn));
            entity p = VR_Player();
            while(p) { sound_pitch(p, CHAN_AUTO, AR_BEEP_GO, 1, ATTN_NONE, 60); p = VR_NextPlayer(p); }
        }
        ar_dir_warn_at = time + max(AR_TICK, warn);
        return xAlive;
    }

    // A spawn group: 1..N of the wave's own (it stops at a boss, a surge, the cap).
    float gmin = arenadirget(1, 0, AR_DW_GMIN);
    float g = gmin + floor(VR_ARENA_Rand() * (arenadirget(1, 0, AR_DW_GMAX) - gmin + 1));
    ar_dir_spread = ((arenadirget(1, 0, AR_DW_SPREAD) != 0) && (g > 1));
    while(n < g && ar_plan_left > 0 && xAlive < cap)
    {
        i = ar_plan_n - ar_plan_left;
        if(ar_plan_boss[i] || (arenadirget(0, i, AR_DF_SURGE) > 0)) { break; }
        if(!VR_ARENA_SpawnNext()) { break; }
        ar_dir_group_cost = ar_dir_group_cost + arenadirget(0, i, AR_DF_COST);
        xAlive = xAlive + 1;
        n = n + 1;
    }
    ar_dir_spread = 0;
    if(n > 0) { VR_ARENA_DirGroupDone(n); }
    return xAlive;
}

// A fallen foe's worth: vr_arena_score_kill x its cost / 8 (a shotgun grunt 100) by the difficulty's factor; a quick
// kill (within quick_time of its arrival) quick_pay more.
float(entity xFoe) VR_ARENA_DirWorth =
{
    float worth = VR_ARENA_SkillWorth(VR_ARENA_Opt_ScoreKill() * xFoe.ar_cost / 8);
    if(cvar_hget(cvarh_vr_arena_quick_kill) != 0 && time - xFoe.ar_born <= arenadirget(1, 0, AR_DW_QTIME))
    {
        float more = VR_ARENA_SkillWhole(worth * arenadirget(1, 0, AR_DW_QPAY));
        ar_dir_quick = ar_dir_quick + 1;
        ar_dir_quick_pts = ar_dir_quick_pts + more;
        worth = worth + more;
        dprint(sprintf("arena: quick kill %.1f s after it came in: +%g\n", time - xFoe.ar_born, more));
    }
    return worth;
}

// The wave clear's line about its quick kills ("" none).
string() VR_ARENA_DirClearLine =
{
    if(!VR_ARENA_DirOn() || ar_dir_quick <= 0) { return ""; }
    return sprintf("%g quick kills: +%g\n", ar_dir_quick, ar_dir_quick_pts);
}

// A clear's supplies, the engine's supply budget's: what it bought, scattered as the classic ones are.
float(float xCode) VR_ARENA_DirDropWhat =
{
    if(xCode == 1) { return 5; }  // a small medkit
    if(xCode == 2) { return 1; }  // a medkit
    if(xCode == 3) { return 6; }  // a megahealth
    if(xCode == 4) { return 4; }  // a shard
    if(xCode == 5) { return 2; }  // green
    if(xCode == 6) { return 3; }  // yellow
    if(xCode == 7) { return 7; }  // red
    if(xCode == 8) { return 8; }  // ammo
    if(xCode == 9) { return 9; }  // quad
    if(xCode == 10) { return 10; } // ring
    return 11;                    // pent
}

void(float xWave) VR_ARENA_DirDrops =
{
    entity p = VR_Player();
    float hp = p ? clamp(p.health / 100, 0, 1) : 0.5;
    float dom = p ? clamp((p.health + 0.5 * p.armorvalue) / max(1, ar_dir_hp0), 0, 1) : 0.5;
    float n = arenadirsupply(xWave, ar_dir_threat, hp, dom);
    float i;
    ar_drop_health = 0;
    ar_drop_armour = 0;
    ar_drop_armour_kind = 0;
    ar_drop_shards = 0;
    ar_dir_drop_power = 0;
    ar_dir_drop_mega = 0;
    ar_dir_drop_ammo = 0;
    VR_ARENA_ScatterBegin(xWave);
    for(i = 0; i < n; i++)
    {
        float code = arenadirget(2, i, 0);
        float what = VR_ARENA_DirDropWhat(code);
        vector at = VR_ARENA_DropPlace(i);
        if(!ar_place_ok) { dprint(sprintf("arena: no free place for supply %g\n", code)); continue; }
        if(!VR_ARENA_SpawnDrop(what, at)) { continue; }
        if(what == 1 || what == 5) { ar_drop_health = ar_drop_health + 1; }
        else if(what == 4) { ar_drop_shards = ar_drop_shards + 1; }
        else if(what == 2 || what == 3 || what == 7) { ar_drop_armour = 1; ar_drop_armour_kind = what == 7 ? 4 : what; }
        else if(what == 6) { ar_dir_drop_mega = ar_dir_drop_mega + 1; }
        else if(what == 8) { ar_dir_drop_ammo = ar_dir_drop_ammo + 1; }
        else { ar_dir_drop_power = what; }
    }
    dprint(sprintf("arena: supplies wave %g (hp %.2f dom %.2f): %g medkits, armour %g, %g shards, ", xWave, hp, dom,
                   ar_drop_health, ar_drop_armour_kind, ar_drop_shards));
    dprint(sprintf("%g mega, %g ammo, power %g\n", ar_dir_drop_mega, ar_dir_drop_ammo, ar_dir_drop_power));
}
