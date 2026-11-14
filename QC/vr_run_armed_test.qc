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
#endif
