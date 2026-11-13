// vr_run_test.qc -- phase 2's run mutators' tests (docs/vr-port/MUTATORS.md): Randomized Pickups and Ironman. Each goes
// through map loads (a map, a reload, a save and its load, a changelevel, a restart): its step and tally are cvars
// (vr_run_test_step, vr_run_test_tally, vr_run_test_a/b/c), and a step waits for the next map's player after asking for
// a load (vr_rt_load: a nosave global, cleared by the load's new progs).
// `vr_runpickups_test 1`: Balanced on e1m1 (seed 777): items swapped, each for one of its kind and tier (ammo for an
//   allowed kind, its size kept), nothing the map names; e1m1 again: the same layout, the same unresolved targets and
//   trigger_counter links; saved, Randomized Pickups set off, the save loaded: the run's choice and the layout back;
//   changelevel to e1m2: swapped there; another run's seed on e1m2: other pickups; the first run's save loaded: its
//   seed back, and a changelevel to e1m2 gives the first run's pickups there; Anything Goes with Melee Only and Glass
//   Cannon on e1m1: kinds crossed, no gun
//   where there was none, the map's links and monster count as before, vr_run_check's game checks; a new run without it:
//   nothing swapped. Verdict "runpickups: N passed, M failed".
// `vr_runpickups_test 2`: the map loaded now (with Randomized Pickups on: another game's, AD's), then again with it off:
//   the swaps follow the rules, and the map's targets, trigger_counter links and monsters are the same.
// `vr_ironman_test 1`: Ironman (with Randomized Pickups, Glass Cannon) on e1m1: save and the menu's Save refused; a
//   rocket launcher and 50 rockets given, changelevel to e1m2: entered with them; a grenade launcher given, rockets
//   spent, killed: no death menu, the respawn restarts e1m2 with the rocket launcher, 50 rockets and no grenade
//   launcher, the run's choices kept; a new run without Ironman saves. Verdict "ironman: N passed, M failed".
#ifdef QVR_TESTS

nosave float vr_rt_load;   // a step asked for a map load: the next step waits for it
nosave float vr_rt_frames; // frames the current step has waited (a stuck test: FAIL "timed out")
float VR_RT_GIVEUP = 2160; // frames (30 s of game time)

void(float step, float load) VR_RT_To =
{
    cvar_set("vr_run_test_step", ftos(step));
    vr_rt_load = load;
    vr_rt_frames = 0;
};

void(string tag) VR_RT_Resume =
{
    VR_Test_Begin(tag); // (dprint: a broadcast before a map load never reaches the console)
    float t = cvar_hget(cvarh_vr_run_test_tally);
    vr_test_pass = floor(t / 1000);
    vr_test_fail = t - vr_test_pass * 1000;
};

void() VR_RT_Store =
{
    cvar_set("vr_run_test_tally", ftos(vr_test_pass * 1000 + vr_test_fail));
};

void() VR_RT_End =
{
    VR_Test_Verdict();
    cvar_set("vr_run_test_tally", "0");
    VR_RT_To(0, FALSE);
};

float(float x, float m) VR_RT_Mod = { return x - floor(x / m) * m; };

// The map's swapped items: their rules (Balanced's: `balanced`), the layout's hash, kinds crossed, guns made.
float vr_rt_swapped;
float vr_rt_broken;
float vr_rt_crossed;
float vr_rt_guns;
float vr_rt_named;
float vr_rt_hash;
void(float balanced) VR_RT_Layout =
{
    vr_rt_swapped = 0;
    vr_rt_broken = 0;
    vr_rt_crossed = 0;
    vr_rt_guns = 0;
    vr_rt_named = 0;
    vr_rt_hash = 0;
    for(entity e = nextent(world); e; e = nextent(e))
    {
        if(e.vr_rpk_row < 0)
        {
            if(e.vr_rpk_from) vr_rt_named++;
        } // (a kept item is the map's own: never a swapped-in one)
        if(!(e.vr_rpk_from)) continue;
        vr_rt_swapped++;
        float r0 = e.vr_rpk_from - 1;
        float r = VR_RPK_RowOf(e);
        if(r < 0) { vr_rt_broken++; continue; }
        VR_RPK_Load(r0);
        float cat0 = vr_rpk_cat;
        float tier0 = vr_rpk_tier;
        float gun0 = FALSE;
        if(vr_rpk_cat == RPK_WEAPON) gun0 = VR_RunMeleeOnly_Refuses(vr_rpk_wid);
        VR_RPK_Load(r);
        if(vr_rpk_cat != cat0) vr_rt_crossed++;
        if(vr_rpk_cat == RPK_WEAPON && !gun0)
        {
            if(VR_RunMeleeOnly_Refuses(vr_rpk_wid)) vr_rt_guns++;
        }
        if(balanced)
        {
            float ok = TRUE;
            if(vr_rpk_cat != cat0) ok = FALSE;
            if(vr_rpk_cat == RPK_ARMOUR)
            {
                if(fabs(vr_rpk_tier - tier0) > 1) ok = FALSE;
            }
            else if(vr_rpk_tier != tier0) ok = FALSE;
            if(vr_rpk_cat == RPK_AMMO)
            {
                if(!(e.vr_rpk_mask & VR_RPK_AmmoBit(vr_rpk_aid))) ok = FALSE;
            }
            if(!ok)
            {
                vr_rt_broken++;
                VR_Test_Print(sprintf("%s: %s at %v replaced row %g (tier %g, ammo mask %g)\n", vr_test_tag, e.classname,
                    e.origin, r0, tier0, e.vr_rpk_mask));
            }
        }
        vr_rt_hash = VR_RT_Mod(vr_rt_hash + (r + 1) * (1 + VR_RT_Mod(fabs(rint(e.origin_x)), 997) +
                                                       3 * VR_RT_Mod(fabs(rint(e.origin_y)), 991)), 65521);
    }
};

// The map's links: targets and killtargets naming nothing, plus 1000 a link to a trigger_counter.
float() VR_RT_Links =
{
    float n = 0;
    for(entity e = nextent(world); e; e = nextent(e))
    {
        if(e.target != "")
        {
            entity t = find(world, targetname, e.target);
            if(!t) n++;
            else if(t.classname == "trigger_counter") n += 1000;
        }
        if(e.killtarget != "" && !find(world, targetname, e.killtarget)) n++;
    }
    return n;
};

// The layout's checks, `what` naming the map's load.
void(float balanced, string what) VR_RT_CheckLayout =
{
    VR_RT_Layout(balanced);
    VR_Test_Check(vr_rt_swapped > 0 && vr_rpk_swapped == vr_rt_swapped, sprintf("%s: %g of %g items swapped, %g of them "
        "in place (%g kept for the map)", what, vr_rpk_swapped, vr_rpk_seen, vr_rt_swapped, vr_rpk_kept));
    VR_Test_Check(!vr_rt_broken, sprintf("%s: every swap %s (%g broken)", what,
        balanced ? "of its kind and tier, ammo of a held or nearby gun" : "a kind of the table", vr_rt_broken));
    VR_Test_Check(!vr_rt_named, sprintf("%s: the items the map names or counts kept (%g)", what, vr_rpk_kept));
};

void() VR_RT_Pickups =
{
    float step = cvar_hget(cvarh_vr_run_test_step);
    entity p = VR_Player();
    VR_RT_Resume("runpickups");
    if(step == 1)
    {
        cvar_set("vr_run_seed", "777");
        localcmd("vr_run pickups 1\nvr_run meleeonly 0\nvr_run ironman 0\nvr_run glasscannon 0\nmap e1m1\n");
        VR_RT_To(2, TRUE);
    }
    else if(step == 2)
    {
        VR_Test_Check(mapname == "e1m1" && cvar_hget(cvarh_vr_run_pickups) == 1, "e1m1, Balanced");
        VR_RT_CheckLayout(TRUE, "e1m1 balanced");
        cvar_set("vr_run_test_a", ftos(vr_rt_hash));
        cvar_set("vr_run_test_b", ftos(VR_RT_Links()));
        cvar_set("vr_run_test_c", ftos(total_monsters));
        localcmd("map e1m1\n");
        VR_RT_To(3, TRUE);
    }
    else if(step == 3)
    {
        VR_RT_Layout(TRUE);
        VR_Test_Check(vr_rt_hash == cvar_hget(cvarh_vr_run_test_a) && vr_rt_swapped > 0,
            sprintf("e1m1 again, the run's seed: the same pickups (hash %g, was %g)", vr_rt_hash, cvar_hget(cvarh_vr_run_test_a)));
        VR_Test_Check(VR_RT_Links() == cvar_hget(cvarh_vr_run_test_b), sprintf("the map's links as before (%g)", VR_RT_Links()));
        localcmd("save mp_rpk_test\n");
        VR_RT_To(4, FALSE);
    }
    else if(step == 4)
    {
        localcmd("vr_run pickups 0\nload mp_rpk_test\n");
        VR_RT_To(5, TRUE);
    }
    else if(step == 5)
    {
        VR_RT_Layout(TRUE);
        VR_Test_Check(cvar_hget(cvarh_vr_run_pickups) == 1, "the save loaded: the run's Randomized Pickups back (it was set off)");
        VR_Test_Check(vr_rt_hash == cvar_hget(cvarh_vr_run_test_a), sprintf("the save loaded: its pickups (hash %g)", vr_rt_hash));
        localcmd("changelevel e1m2\n");
        VR_RT_To(6, TRUE);
    }
    else if(step == 6)
    {
        VR_Test_Check(mapname == "e1m2" && cvar_hget(cvarh_vr_run_pickups) == 1, "changelevel to e1m2: still Balanced");
        VR_RT_CheckLayout(TRUE, "e1m2 balanced");
        // Another run (its own seed) on e1m2, then the first run's save loaded and a changelevel: the saved run's seed
        // back (the save's `// qvr_run` line), so e1m2's pickups are the first run's again.
        cvar_set("vr_run_test_a", ftos(vr_rt_hash));
        localcmd("vr_run_seed 4242\nmap e1m2\n");
        VR_RT_To(10, TRUE);
    }
    else if(step == 10)
    {
        VR_RT_Layout(TRUE);
        VR_Test_Check(vr_rt_hash != cvar_hget(cvarh_vr_run_test_a) && vr_rt_swapped > 0,
            sprintf("another run's seed: other pickups on e1m2 (hash %g, the first run's %g)", vr_rt_hash,
                cvar_hget(cvarh_vr_run_test_a)));
        localcmd("load mp_rpk_test\n");
        VR_RT_To(11, TRUE);
    }
    else if(step == 11)
    {
        VR_Test_Check(cvar_hget(cvarh_vr_run_seed) == 777,
            sprintf("the first run's save loaded: its seed back (%g, the other run's 4242)", cvar_hget(cvarh_vr_run_seed)));
        localcmd("changelevel e1m2\n");
        VR_RT_To(12, TRUE);
    }
    else if(step == 12)
    {
        VR_RT_Layout(TRUE);
        VR_Test_Check(mapname == "e1m2" && vr_rt_hash == cvar_hget(cvarh_vr_run_test_a),
            sprintf("changelevel to e1m2 after the load: the saved run's pickups (hash %g, was %g)", vr_rt_hash,
                cvar_hget(cvarh_vr_run_test_a)));
        localcmd("vr_run pickups 2\nvr_run meleeonly 1\nvr_run glasscannon 1\nvr_run startgun 0\nvr_run startmelee 0\nmap e1m1\n");
        VR_RT_To(7, TRUE);
    }
    else if(step == 7)
    {
        VR_RT_CheckLayout(FALSE, "e1m1 anything goes, melee only");
        VR_Test_Check(vr_rt_crossed > 0, sprintf("anything goes: %g items of another kind", vr_rt_crossed));
        VR_Test_Check(!vr_rt_guns, sprintf("melee only: no gun where there was none (%g)", vr_rt_guns));
        VR_Test_Check(VR_RT_Links() == cvar_hget(cvarh_vr_run_test_b) && total_monsters == cvar_hget(cvarh_vr_run_test_c),
            sprintf("the map's links (%g) and monsters (%g) as before", VR_RT_Links(), total_monsters));
        VR_Test_Check(VR_Run_TestCheck(RUN_SG_NOTHING * 10 + RUN_SM_AXE, 1, 1), "with Glass Cannon and Melee Only (the axe start): vr_run_check's game checks");
        localcmd("vr_runpick_pickups 0\nvr_runpick_meleeonly 0\nvr_runpick_glasscannon 0\nvr_run start\nmap e1m1\n");
        VR_RT_To(8, TRUE);
    }
    else if(step == 8)
    {
        VR_RT_Layout(FALSE);
        VR_Test_Check(cvar_hget(cvarh_vr_run_pickups) == 0 && !vr_rt_swapped && !vr_rpk_seen,
            sprintf("a new run without it: nothing swapped (%g)", vr_rt_swapped));
        VR_Test_Check(VR_RT_Links() == cvar_hget(cvarh_vr_run_test_b) && total_monsters == cvar_hget(cvarh_vr_run_test_c),
            "the map's links and monsters as with it on");
        // The run's one seed: two new runs with the same choices differ (pickups, the map's content seed: crates and
        // barrels, enemy swaps); a changelevel, a revisit and a saved game's load keep a run's. vr_map_seed -1: the
        // content seed in a test session too (else 0 there); with sv_random_seed set every run would get its seed (the
        // second run's check says so).
        localcmd("vr_map_seed -1\nvr_runpick_pickups 1\nvr_run start\nmap e1m1\n");
        VR_RT_To(13, TRUE);
    }
    else if(step == 13 || step == 14)
    {
        VR_RT_Layout(TRUE);
        VR_Test_Check(vr_map_content_seed > 0 && cvar_hget(cvarh_vr_run_seed) > 0, sprintf("run %g: its seed %g, the map's "
            "content seed %g", step - 12, cvar_hget(cvarh_vr_run_seed), vr_map_content_seed));
        if(step == 14)
        {
            VR_Test_Check(cvar_hget(cvarh_vr_run_seed) != cvar_hget(cvarh_vr_run_test_b) &&
                vr_map_content_seed != cvar_hget(cvarh_vr_run_test_c) && vr_rt_hash != cvar_hget(cvarh_vr_run_test_a),
                sprintf("a second new run, the same choices: another seed (%g, was %g), content seed (%g, was %g), "
                    "pickups (hash %g, was %g)", cvar_hget(cvarh_vr_run_seed), cvar_hget(cvarh_vr_run_test_b),
                    vr_map_content_seed, cvar_hget(cvarh_vr_run_test_c), vr_rt_hash, cvar_hget(cvarh_vr_run_test_a)));
        }
        cvar_set("vr_run_test_a", ftos(vr_rt_hash));
        cvar_set("vr_run_test_b", ftos(cvar_hget(cvarh_vr_run_seed)));
        cvar_set("vr_run_test_c", ftos(vr_map_content_seed));
        if(step == 13)
        {
            localcmd("vr_run start\nmap e1m1\n");
            VR_RT_To(14, TRUE);
        }
        else
        {
            localcmd("save mp_rpk_seed\nchangelevel e1m2\n");
            VR_RT_To(15, TRUE);
        }
    }
    else if(step == 15)
    {
        VR_Test_Check(mapname == "e1m2" && cvar_hget(cvarh_vr_run_seed) == cvar_hget(cvarh_vr_run_test_b),
            sprintf("changelevel to e1m2: the run's seed kept (%g)", cvar_hget(cvarh_vr_run_seed)));
        localcmd("changelevel e1m1\n");
        VR_RT_To(16, TRUE);
    }
    else if(step == 16 || step == 18)
    {
        VR_RT_Layout(TRUE);
        string what = step == 16 ? "back on e1m1 in the run" : "the run's save loaded after another run";
        VR_Test_Check(cvar_hget(cvarh_vr_run_seed) == cvar_hget(cvarh_vr_run_test_b) &&
            vr_map_content_seed == cvar_hget(cvarh_vr_run_test_c) && vr_rt_hash == cvar_hget(cvarh_vr_run_test_a),
            sprintf("%s: its seed (%g), content seed (%g), pickups (hash %g, was %g)", what, cvar_hget(cvarh_vr_run_seed),
                vr_map_content_seed, vr_rt_hash, cvar_hget(cvarh_vr_run_test_a)));
        if(step == 16)
        {
            localcmd("vr_run start\nmap e1m1\n");
            VR_RT_To(17, TRUE);
        }
        else
        {
            localcmd("vr_map_seed 0\nvr_runpick_pickups 0\n");
            VR_RT_End();
        }
    }
    else if(step == 17)
    {
        VR_Test_Check(cvar_hget(cvarh_vr_run_seed) != cvar_hget(cvarh_vr_run_test_b),
            sprintf("a third new run: another seed (%g)", cvar_hget(cvarh_vr_run_seed)));
        localcmd("load mp_rpk_seed\n");
        VR_RT_To(18, TRUE);
    }
    else if(step == 50)
    {
        float mode = cvar_hget(cvarh_vr_run_pickups);
        VR_Test_Check(mode > 0, sprintf("%s: Randomized Pickups on (%g)", mapname, mode));
        VR_RT_CheckLayout(mode == 1, sprintf("%s %s", mapname, mode == 1 ? "balanced" : "anything goes"));
        cvar_set("vr_run_test_a", ftos(mode));
        cvar_set("vr_run_test_b", ftos(VR_RT_Links()));
        cvar_set("vr_run_test_c", ftos(total_monsters));
        localcmd(sprintf("vr_run pickups 0\nmap %s\n", mapname));
        VR_RT_To(51, TRUE);
    }
    else if(step == 51)
    {
        VR_Test_Check(!vr_rpk_seen, sprintf("%s without it: nothing swapped", mapname));
        VR_Test_Check(VR_RT_Links() == cvar_hget(cvarh_vr_run_test_b) && total_monsters == cvar_hget(cvarh_vr_run_test_c),
            sprintf("%s: its links (%g, with it %g) and monsters (%g, with it %g) as with it on", mapname, VR_RT_Links(),
                cvar_hget(cvarh_vr_run_test_b), total_monsters, cvar_hget(cvarh_vr_run_test_c)));
        localcmd(sprintf("vr_run pickups %g\n", cvar_hget(cvarh_vr_run_test_a)));
        VR_RT_End();
    }
    VR_RT_Store();
};

void() VR_RT_Ironman =
{
    float step = cvar_hget(cvarh_vr_run_test_step);
    entity p = VR_Player();
    VR_RT_Resume("ironman");
    float refused = cvar_hget(cvarh_vr_ironman_refused);
    if(step == 20)
    {
        cvar_set("vr_run_test_a", ftos(refused));
        localcmd("vr_run ironman 1\nvr_run pickups 1\nvr_run glasscannon 1\nvr_run startgun 0\nvr_run startmelee 0\nvr_run meleeonly 0\n"
                 "map e1m1\n");
        VR_RT_To(21, TRUE);
    }
    else if(step == 21)
    {
        VR_Test_Check(cvar_hget(cvarh_vr_run_ironman) == 1 && mapname == "e1m1", "e1m1, Ironman on");
        p.items = p.items | IT_ROCKET_LAUNCHER;
        p.ammo_rockets = 50;
        localcmd("save mp_iron_test\n");
        VR_RT_To(22, FALSE);
    }
    else if(step == 22)
    {
        VR_Test_Check(refused == cvar_hget(cvarh_vr_run_test_a) + 1, sprintf("save refused (%g)", refused));
        localcmd("menu_save\n");
        VR_RT_To(23, FALSE);
    }
    else if(step == 23)
    {
        VR_Test_Check(refused == cvar_hget(cvarh_vr_run_test_a) + 2, "the menu's Save refused");
        localcmd("changelevel e1m2\n");
        VR_RT_To(24, TRUE);
    }
    else if(step == 24)
    {
        VR_Test_Check(mapname == "e1m2" && (p.items & IT_ROCKET_LAUNCHER) && p.ammo_rockets == 50,
            sprintf("e1m2 entered with e1m1's rocket launcher and 50 rockets (%g)", p.ammo_rockets));
        cvar_set("vr_run_test_b", ftos(p.health));
        VR_RT_Layout(TRUE);
        cvar_set("vr_run_test_c", ftos(vr_rt_hash));
        p.items = p.items | IT_GRENADE_LAUNCHER;
        p.ammo_rockets = 7;
        T_Damage(p, world, world, 5000);
        VR_Test_Check(p.health <= 0, "killed");
        VR_RT_To(25, FALSE);
    }
    else if(step == 25)
    {
        if(p.deadflag < DEAD_DEAD) return; // (its death's frames: the respawn waits for them as Quake's does)
        VR_Test_Check(!vr_death_menu_hold, "dead: no death menu holds the respawn (no saves to load)");
        entity oldself = self;
        self = p;
        respawn();
        self = oldself;
        VR_RT_To(26, TRUE);
    }
    else if(step == 26)
    {
        VR_Test_Check(mapname == "e1m2" && cvar_hget(cvarh_vr_ironman_refused) == cvar_hget(cvarh_vr_run_test_a) + 2,
            "the respawn restarted e1m2 (not a save's load)");
        VR_Test_Check((p.items & IT_ROCKET_LAUNCHER) && !(p.items & IT_GRENADE_LAUNCHER) && p.ammo_rockets == 50 &&
            p.health == cvar_hget(cvarh_vr_run_test_b),
            sprintf("e1m2's start loadout: the rocket launcher, 50 rockets (%g), no grenade launcher, health %g", p.ammo_rockets,
                p.health));
        VR_Test_Check(cvar_hget(cvarh_vr_run_ironman) == 1 && cvar_hget(cvarh_vr_run_pickups) == 1 && vr_rpk_swapped > 0,
            "the run's choices kept (Ironman, Randomized Pickups: swapped again)");
        VR_RT_Layout(TRUE);
        VR_Test_Check(vr_rt_hash == cvar_hget(cvarh_vr_run_test_c), sprintf("the restarted e1m2's pickups as before the "
            "death (the run's seed: hash %g)", vr_rt_hash));
        VR_Test_Check(VR_Run_TestCheck(0, 1, 0), "with Glass Cannon: vr_run_check's game checks");
        localcmd("vr_runpick_ironman 0\nvr_runpick_pickups 0\nvr_runpick_glasscannon 0\nvr_run start\nsave mp_iron_test\n");
        VR_RT_To(27, FALSE);
    }
    else if(step == 27)
    {
        VR_Test_Check(!cvar_hget(cvarh_vr_run_ironman) && refused == cvar_hget(cvarh_vr_run_test_a) + 2,
            "a new run without Ironman: its save not refused");
        VR_RT_End();
    }
    VR_RT_Store();
};

// VR_TestsFrame: `vr_runpickups_test`, `vr_ironman_test` start their test; the step runs once its map's player is in.
void() VR_RunTest_Frame =
{
    float ask = cvar_hget(cvarh_vr_runpickups_test);
    float iron = cvar_hget(cvarh_vr_ironman_test);
    if(ask || iron)
    {
        cvar_set("vr_runpickups_test", "0");
        cvar_set("vr_ironman_test", "0");
        cvar_set("vr_run_test_tally", "0");
        VR_RT_To(iron ? 20 : (ask == 2 ? 50 : 1), FALSE);
        return;
    }
    float step = cvar_hget(cvarh_vr_run_test_step);
    if(!step)
    {
        return;
    }
    entity p = VR_Player();
    vr_rt_frames++;
    if(vr_rt_frames > VR_RT_GIVEUP)
    {
        VR_RT_Resume((step >= 20 && step < 50) ? "ironman" : "runpickups");
        VR_Test_Check(FALSE, sprintf("step %g timed out", step));
        VR_RT_End();
        return;
    }
    if(vr_rt_load || !p || (p.health <= 0 && step != 25))
    {
        return; // (the map asked for not loaded yet, or its player not in)
    }
    if(step != 25) dprint(sprintf("runtest: step %g on %s\n", step, mapname));
    if(step >= 20 && step < 50) VR_RT_Ironman();
    else VR_RT_Pickups();
};
#endif
