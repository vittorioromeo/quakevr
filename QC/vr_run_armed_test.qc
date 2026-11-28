//
//
//
// ----------------------------------------------------------------------------
// The armed-humanoid run mutators' check (docs/vr-port/MUTATORS.md, "Ranged Knights, Wild Weaponry, Armored Foes"):
// the engine's `vr_run_armed_check <rangedknights> <wildweapons> <armoredfoes>` (vr_runsetup.cpp) calls
// VR_Run_ArmedCheck1..4 in the running game. Each line printed (developer 1) "vr_run_armed_check: PASS|FAIL <what>".
// - the rolls over many spawns (seeds that are not the map's: the game's own rolls are not moved): a knight's pool
//   (VR_AKN_Pick) in its weights' shares, the sword the most common; the grunts' (VR_AGR_Pick) and the knights' other
//   guns owned by a player and no rare tier before its map unless Wild Weaponry, some of each with it; the armour roll
//   (VR_AgArmor_Roll, its sequence put back) at the skill's chance, or Armored Foes' raised one;
// - the map's own: every armed knight rolled (none pending once a player is in), holding swords only without Ranged
//   Knights (a map's "weapon" key aside), taking up guns as the choice says; with Armored Foes most armed humanoids
//   armoured.
// ----------------------------------------------------------------------------
#ifdef QVR_TESTS

void(string what, float ok) VR_RAT_Expect = { VR_Test_Check(ok, what); };

// A share `n` of `of` within `want` +- `tol`.
void(string what, float n, float of, float want, float tol) VR_RAT_Share =
{
    float f = n / max(1, of);
    VR_RAT_Expect(sprintf("%s: %g of %g (%.3f, want %.3f +- %.2f)", what, n, of, f, want, tol), fabs(f - want) <= tol);
};

// The `i`th test seed (not a map's: its z is past any kind's).
vector(float i) VR_RAT_Seed =
{
    vector s;
    s_x = VR_AGR_Mod(i * 37 + 11, 30011);
    s_y = VR_AGR_Mod(i * 101 + 5, 30011);
    s_z = 50 + VR_AGR_Mod(i, 7);
    return s;
};

// A code a player owns now (or need not: tier 1, AD's guns).
float(float code) VR_RAT_Owned =
{
    if(!VR_AG_Decode(code) || VR_IsADGun(code) || code == WID_SHOTGUN || code == WID_NAILPISTOL)
    {
        return TRUE;
    }
    return VR_AGR_Owned(vr_ag_dec_wid);
};

// The tier of a weapon code in the replacements' tiers (0: none of them).
float(float code) VR_RAT_TierOf =
{
    float t;
    float i;
    for(t = 1; t <= 3; t++)
    {
        for(i = 0; VR_AGR_TierCode(t, i) >= 0; i++)
        {
            if(VR_AGR_TierCode(t, i) == code)
            {
                return t;
            }
        }
    }
    return 0;
};

// The rare tier is before its map here (Wild Weaponry aside).
float() VR_RAT_RareEarly =
{
    float from = cvar_hget(cvarh_vr_agrunt_repl_rare_map);
    return from > 0 && VR_AGR_MapIndex() < max(1, from);
};

void(float ww) VR_RAT_Knights =
{
    float n = 1000; // (each pick reads every gun's eligibility: more, a runaway loop with Wild Weaponry)
    float sword = 0;
    float dual = 0;
    float xbow = 0;
    float xsword = 0;
    float saw = 0;
    float gun = 0;
    float unowned = 0;
    float i;
    for(i = 0; i < n; i++)
    {
        VR_AKN_Pick(VR_AM_KNIGHT, VR_RAT_Seed(i));
        if(vr_agr_tier == 0) { sword = sword + 1; if(vr_agr_code2 == WID_SWORD) { dual = dual + 1; } }
        if(vr_agr_tier == 1) { xbow = xbow + 1; if(vr_agr_code2 == WID_SWORD && vr_agr_gunaxe) { xsword = xsword + 1; } }
        if(vr_agr_tier == 2) { saw = saw + 1; }
        if(vr_agr_tier == 3)
        {
            gun = gun + 1;
            if(!VR_RAT_Owned(vr_agr_code) || (VR_RAT_TierOf(vr_agr_code) == 3 && VR_RAT_RareEarly()))
            {
                unowned = unowned + 1;
            }
        }
    }
    // The weights as VR_AKN_Pick reads them (VR_AKN_GunCount: the other guns there are now).
    float ws = max(0, cvar_hget(cvarh_vr_rangedknights_w_sword));
    float wx = (VR_AG_Decode(WID_ADCROSSBOW) && AD_Rules()) ? max(0, cvar_hget(cvarh_vr_rangedknights_w_crossbow)) : 0;
    float wc = VR_AG_Decode(WID_CHAINSAW) ? max(0, cvar_hget(cvarh_vr_rangedknights_w_chainsaw)) : 0;
    float wg = VR_AKN_GunCount() > 0 ? max(0, cvar_hget(cvarh_vr_rangedknights_w_gun)) : 0;
    float tot = max(0.001, ws + wx + wc + wg);
    dprint(sprintf("vr_run_armed_check: knights' pool over %g: sword %g (two %g), crossbow %g (and sword %g), ", n, sword, dual,
                   xbow, xsword));
    dprint(sprintf("chainsaw %g, other gun %g (%g unowned or early; %g guns open)\n", saw, gun, unowned, VR_AKN_GunCount()));
    VR_RAT_Share("knights: the sword", sword, n, ws / tot, 0.04);
    VR_RAT_Share("knights: the crossbow", xbow, n, wx / tot, 0.03);
    VR_RAT_Share("knights: the chainsaw", saw, n, wc / tot, 0.02);
    VR_RAT_Share("knights: other guns", gun, n, wg / tot, 0.02);
    if(sword >= 50)
    {
        VR_RAT_Share("knights: two swords among the swords", dual, sword, VR_AGR_Unit(cvar_hget(cvarh_vr_rangedknights_dual)), 0.05);
    }
    if(xbow >= 50)
    {
        VR_RAT_Share("knights: crossbow and sword among the crossbows", xsword, xbow,
            VR_AGR_Unit(cvar_hget(cvarh_vr_rangedknights_xbow_sword)), 0.08);
    }
    if(ws >= wx && ws >= wc && ws >= wg) // (the shipped weights; a test may set the sword's to 0)
    {
        VR_RAT_Expect("knights: the sword the most common", sword > xbow && sword > saw && sword > gun);
    }
    if(!ww)
    {
        VR_RAT_Expect(sprintf("knights: other guns owned and in their map (%g not)", unowned), unowned == 0);
    }
    else if(VR_RAT_RareEarly())
    {
        VR_RAT_Expect(sprintf("knights: Wild Weaponry: guns not owned or before their map (%g)", unowned), unowned > 0);
    }
};

void(float ww) VR_RAT_Grunts =
{
    float n = 800; // (a pick reads every tier's eligibility: fewer, or a runaway loop)
    float unowned = 0;
    float early = 0;
    float i;
    for(i = 0; i < n; i++)
    {
        VR_AGR_Pick(VR_AM_GRUNT, VR_RAT_Seed(i));
        if(vr_agr_tier >= 2 && !VR_RAT_Owned(vr_agr_code))
        {
            unowned = unowned + 1;
        }
        if(vr_agr_tier == 3 && VR_RAT_RareEarly())
        {
            early = early + 1;
        }
    }
    dprint(sprintf("vr_run_armed_check: grunts over %g: %g not owned, %g rare before map %g (this map %g)\n", n, unowned, early,
                   cvar_hget(cvarh_vr_agrunt_repl_rare_map), VR_AGR_MapIndex()));
    if(!ww)
    {
        VR_RAT_Expect(sprintf("grunts: every weapon owned, no rare before its map (%g, %g)", unowned, early), unowned == 0 && early == 0);
    }
    else
    {
        VR_RAT_Expect(sprintf("grunts: Wild Weaponry: weapons not owned yet (%g)", unowned), unowned > 0);
        if(VR_RAT_RareEarly())
        {
            VR_RAT_Expect(sprintf("grunts: Wild Weaponry: rare ones before their map (%g)", early), early > 0);
        }
    }
};

void(float af) VR_RAT_Armour =
{
    float sk = max(0, min(3, floor(cvar_hget(cvarh_skill))));
    float c = min(1, max(0, VR_AgArmor_SkillValue(sk, 0)));
    float want = af ? 1 - (1 - c) * (1 - min(1, max(0, cvar_hget(cvarh_vr_armoredfoes_chance)))) : c;
    float seq = vr_ar_seq;
    float n = 2000;
    float worn = 0;
    float i;
    for(i = 0; i < n; i++)
    {
        vector at;
        at_x = i * 37.3;
        at_y = i * 11.1;
        at_z = 0;
        VR_AgArmor_Roll(sk, at);
        if(vr_ar_roll_type > 0)
        {
            worn = worn + 1;
        }
    }
    vr_ar_seq = seq; // (the game's own sequence as it was)
    VR_RAT_Share(sprintf("armour rolls on skill %g", sk), worn, n, want, 0.04);
};

void(float rk, float af) VR_RAT_Map =
{
    float knights = 0;
    float ranged = 0;
    float takes = 0;
    float pending = 0;
    float armed = 0;
    float armoured = 0;
    entity e = nextent(world);
    while(e)
    {
        if(e.vr_ag && e.health > 0)
        {
            armed = armed + 1;
            if(e.vr_ar_type > 0)
            {
                armoured = armoured + 1;
            }
            if(e.vr_agr_pending)
            {
                pending = pending + 1;
            }
            if(VR_AKN_IsKnight(e.vr_ag_kind))
            {
                knights = knights + 1;
                if((e.vr_ag_prop && e.vr_ag_wid != WID_SWORD) || (e.vr_ag_prop2 && e.vr_ag_wid2 != WID_SWORD))
                {
                    ranged = ranged + 1;
                }
                if(VR_AM_Takes(e, WID_SHOTGUN))
                {
                    takes = takes + 1;
                }
            }
        }
        e = nextent(e);
    }
    dprint(sprintf("vr_run_armed_check: the map: %g armed (%g armoured), %g knights (%g not swords only, %g take guns), %g pending\n",
                   armed, armoured, knights, ranged, takes, pending));
    VR_RAT_Expect("the map: every armed humanoid rolled", pending == 0);
    if(!rk)
    {
        VR_RAT_Expect("the map: knights hold swords only", ranged == 0);
    }
    else if(cvar_hget(cvarh_vr_rangedknights_w_sword) <= 0 && knights > 0)
    {
        VR_RAT_Expect(sprintf("the map: no sword weight: every knight holds a gun or the chainsaw (%g of %g)", ranged, knights),
            ranged == knights);
    }
    float pick = cvar_hget(cvarh_vr_agknight_ranged_pickup);
    VR_RAT_Expect(sprintf("the map: knights take guns up as the run says (%g of %g)", takes, knights),
        takes == ((rk || pick) ? knights : 0));
    if(af && armed >= 6)
    {
        VR_RAT_Expect(sprintf("the map: Armored Foes: most armoured (%g of %g)", armoured, armed), armoured * 2 > armed);
    }
};

// 1: the running game arms its humanoids as the three choices say (each miss printed). Its parts are called one at a
// time by the engine (VR_Run_ArmedCheck1..4): thousands of rolls in one call would be a runaway loop.
float(float rk, float ww, float af) VR_Run_ArmedCheck1 = { VR_Test_Begin("vr_run_armed_check"); VR_RAT_Knights(ww); return vr_test_fail == 0; };
float(float rk, float ww, float af) VR_Run_ArmedCheck2 = { VR_Test_Begin("vr_run_armed_check"); VR_RAT_Grunts(ww); return vr_test_fail == 0; };
float(float rk, float ww, float af) VR_Run_ArmedCheck3 = { VR_Test_Begin("vr_run_armed_check"); VR_RAT_Armour(af); return vr_test_fail == 0; };
float(float rk, float ww, float af) VR_Run_ArmedCheck4 = { VR_Test_Begin("vr_run_armed_check"); VR_RAT_Map(rk, af); return vr_test_fail == 0; };

// The engine's `vr_run_elites_check <allelites>` (vr_runsetup.cpp; All Elites): the map's armed humanoids every one an
// elite as the run says (none without it: the script keeps vr_elite_spawn 0), its other monsters never; then the hook's
// rules on a living armed humanoid: the arena's wave spawn (its held roll, a preset's held replacements too) forced as
// the run says, the tools' never (vr_agr_hold). 1: as expected (each check printed, developer 1).
float(float ae, float b, float c) VR_Run_ElitesCheck =
{
    VR_Test_Begin("vr_run_elites_check");
    float armed = 0, elites = 0, others = 0, otherElites = 0, pending = 0;
    entity probe = world;
    entity e;
    for(e = nextent(world); e; e = nextent(e))
    {
        if(!(e.flags & FL_MONSTER) || e.health <= 0)
        {
            continue;
        }
        if(e.vr_ag)
        {
            armed = armed + 1;
            elites = elites + ((e.vr_attrs & VR_ATTR_ELITE) != 0);
            pending = pending + (e.vr_agr_pending != 0);
            probe = probe ? probe : e;
        }
        else
        {
            others = others + 1;
            otherElites = otherElites + ((e.vr_attrs & VR_ATTR_ELITE) != 0);
        }
    }
    VR_Test_Check(armed > 0 && pending == 0, sprintf("the map: %g armed humanoids, %g still to roll", armed, pending));
    VR_Test_Check(elites == (ae ? armed : 0), sprintf("the map: %g of %g armed humanoids elite (all elites %g)", elites, armed, ae));
    VR_Test_Check(otherElites == 0, sprintf("the map: %g of its %g other monsters elite", otherElites, others));
    if(probe)
    {
        float hold0 = vr_agr_hold, roll0 = vr_ar_hold_roll;
        vr_agr_hold = 0;
        vr_ar_hold_roll = 1;
        float arena = VR_Run_ForcesElite(probe);
        vr_agr_hold = 1;
        float preset = VR_Run_ForcesElite(probe);
        vr_ar_hold_roll = 0;
        float tools = VR_Run_ForcesElite(probe);
        vr_agr_hold = 0;
        float map = VR_Run_ForcesElite(probe);
        vr_agr_hold = hold0;
        vr_ar_hold_roll = roll0;
        VR_Test_Check(arena == (ae != 0) && preset == (ae != 0) && map == (ae != 0) && !tools,
            sprintf("the hook: an arena wave's %g, a preset wave's %g, the map's %g, the tools' %g (all elites %g)", arena, preset,
                    map, tools, ae));
    }
    return vr_test_fail == 0;
};

// vr_gameplay_classic_check (the engine's, vr_gameplaypresets.cpp), the Classic gameplay preset in the running game: no
// gun has a reloading mode (they fire from the reserve), no grunt burst rifle, no armed humanoid, the map's grunts the
// original ones.
float(float a, float b, float c) VR_Classic_Check =
{
    VR_Test_Begin("vr_gameplay_classic_check");
    float modes = 0;
    float wid;
    for(wid = 1; wid <= 40; wid = wid + 1)
    {
        modes = modes + VR_Reload_ModeOf(wid);
    }
    VR_Test_Check(modes == 0, sprintf("no gun has a reloading mode (sum %g)", modes));
    VR_Test_Check(VR_Reload_ModeOf(WID_SHOTGUN) == 0 && VR_Reload_ModeOf(WID_ROCKET_LAUNCHER) == 0 &&
                  VR_Reload_ModeOf(WID_REVOLVER) == 0, "the shotgun, rocket launcher and revolver fire from the reserve");
    // The player's own reloading as played now: a flat-screen player's (vr_flat_reload, its own path) or a headset's.
    entity pl = VR_Player();
    VR_Test_Check(pl != world && !VRIsWeaponReloadingEnabled(pl),
                  sprintf("the player reloads nothing (%s play)", (pl != world && VR_FlatPlay(pl)) ? "flat-screen" : "VR"));
    VR_Test_Check(cvar_hget(cvarh_vr_grunt_burst) == 0, "no burst rifle grunts");
    float grunts = 0;
    float armed = 0;
    entity e = nextent(world);
    while(e)
    {
        if(e.health > 0)
        {
            if(e.vr_ag)
            {
                armed = armed + 1;
            }
            if(e.classname == "monster_army")
            {
                grunts = grunts + 1;
            }
        }
        e = nextent(e);
    }
    dprint(sprintf("vr_gameplay_classic_check: the map: %g grunts, %g armed humanoids\n", grunts, armed));
    VR_Test_Check(armed == 0, "no armed humanoid in the map");
    VR_Test_Check(grunts > 0, "the map's grunts are the original monster_army");
    return vr_test_fail == 0;
};

// vr_classic_drop_check (the engine's, vr_gameplaypresets.cpp): what a grunt leaves as it dies, and the burst rifle in
// your hand (his report 2026-11-15). The plain grunt nearest the player is gibbed (its whole death at once: army_die's
// gun, SoldierDrop's backpack): its backpack falls; its burst rifle falls to be taken at vr_enemy_held_drops 1, as
// scenery nobody can take at 0 (Classic). Then a burst rifle in the main hand with vr_grunt_burst 0 (the grunts' own
// fire: id's shotgun): a pull begins a 3-round burst from a clip of rounds, not a shotgun's shell. Synchronous: one call;
// its lines dprint's (developer 1: at once, a script may quit in the same frame); returns fails * 1000 + passes, the
// engine prints the verdict line "classicdrop: N passed, M failed".
float(float a, float b, float c) VR_Classic_DropCheck =
{
    VR_Test_Begin("classicdrop");
    entity pl = VR_Player();
    float held = cvar_hget(cvarh_vr_enemy_held_drops);
    entity g = world;
    entity e = find(world, classname, "monster_army");
    while(e)
    {
        if(pl != world && e.health > 0 && !e.vr_ag && (g == world || vlen(e.origin - pl.origin) < vlen(g.origin - pl.origin)))
        {
            g = e;
        }
        e = find(e, classname, "monster_army");
    }
    VR_Test_Check(pl != world && g != world, "a live plain grunt in the map");
    if(pl == world || g == world)
    {
        return vr_test_fail * 1000 + vr_test_pass;
    }
    float scenery = vr_held_scenery_n;
    vector at = g.origin;
    T_Damage(g, pl, pl, 5000); // (gibbed: its gun and its backpack at once)
    float takeable = 0;
    float lying = 0;
    float packs = 0;
    for(e = nextent(world); e; e = nextent(e))
    {
        if(vlen(e.origin - at) > 160)
        {
            continue;
        }
        if(e.classname == "thrown_weapon" && e.weapon == WID_GRUNTGUN)
        {
            if(e.vr_drop_fade > 0 && e.handtouch == SUB_Null && !(e.flags & FL_FORCEGRABBABLE)) { lying = lying + 1; }
            else if(e.vr_drop_fade == 0) { takeable = takeable + 1; }
        }
        if(e.model == "progs/backpack.mdl")
        {
            packs = packs + 1;
        }
    }
    dprint(sprintf("classicdrop: vr_enemy_held_drops %g: grunt gibbed at %v: %g takeable gun(s), %g scenery, %g backpack(s)\n",
        held, at, takeable, lying, packs));
    VR_Test_Check(g.health <= 0, "the grunt died");
    VR_Test_Check(packs >= 1, "its backpack fell (Quake's ammo)");
    if(held)
    {
        VR_Test_Check(takeable == 1 && lying == 0 && vr_held_scenery_n == scenery, "its burst rifle fell, to be taken");
    }
    else
    {
        VR_Test_Check(takeable == 0, "no weapon to take (Classic: enemies drop no weapons)");
        VR_Test_Check(lying == 1 && vr_held_scenery_n == scenery + 1, "its gun fell as scenery (its death frames have none)");
    }

    // The burst rifle in your hand fires bursts whatever the grunts' own fire.
    float burst = cvar_hget(cvarh_vr_grunt_burst);
    cvar_set("vr_grunt_burst", "0");
    entity oldSelf = self;
    self = pl;
    VRGiveEntWeapon(self, cVR_MainHand, WID_GRUNTGUN, 0, WeaponIdToBaseClipSize(WID_GRUNTGUN));
    W_SetCurrentAmmoFor(cVR_MainHand);
    float clip = VRGetEntWeaponClip(pl, cVR_MainHand);
    self.attack_finished = 0;
    VR_EnemyGun_Attack(cVR_MainHand, WID_GRUNTGUN, 0, &self.attack_finished);
    float left = VR_GetBurstLeft(cVR_MainHand);
    float after = VRGetEntWeaponClip(pl, cVR_MainHand);
    VR_SetBurst(cVR_MainHand, 0, 0);
    self = oldSelf;
    cvar_set("vr_grunt_burst", ftos(burst));
    VR_Test_Check(clip >= VR_BURST_ROUNDS && clip == VR_GruntGun_ClipSize() && clip == rint(clip / VR_BURST_ROUNDS) * VR_BURST_ROUNDS,
        sprintf("vr_grunt_burst 0: a burst rifle's clip is rounds, a multiple of 3 (%g)", clip));
    VR_Test_Check(left == VR_BURST_ROUNDS - 1 && after == clip - 1,
        sprintf("vr_grunt_burst 0: a pull begins a burst (%g more to come, %g of %g left), not a shotgun's shot", left, after, clip));
    VR_Test_Check(VR_EnemyGun_Name(WID_GRUNTGUN) == "Burst Rifle", "it is named the burst rifle");
    return vr_test_fail * 1000 + vr_test_pass;
};
#endif
