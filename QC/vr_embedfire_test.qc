#ifdef QVR_TESTS
// vr_embedfire_test.qc -- embedfire (his VR notes 2026-11-26 20:24-20:27): the Crystal Lance fires while it is stuck in a body
// and its hand holds it (QC/vr_propfire.qc: the one firing path, the prop's muzzle and line). Map vrtesthall, god, notarget,
// Dwell's data for the Lance (vr_ad_test 207). A grunt ahead given 5000 health (the bolt's 150 measured), held still.
// `vr_embed_test 77` (VR, the mock headset: VR_EH_Blow's real stab, the mock trigger):
//   case 0: the trigger pressed before the stab, the Lance charging in the hand; it sticks; the charge goes on (the same charge,
//     none begun again) to full on the stuck Lance; the trigger let go: one shot from the stuck Lance's tip (its muzzle), 6
//     cells, the grunt hit;
//   case 1: stuck first; the trigger held there: charged, let go: the same;
//   case 2: stuck, the hand let go of it; the trigger held a charge's time and let go: nothing fires, no cells;
//   case 3: stuck, the trigger held to half a charge, the Lance pulled out: in the hand with the same charge going on; let go
//     full: its shot from the hand (not the prop), 6 cells.
//   Ends `embedfire: N passed, M failed`.
// `vr_embed_test 78` (flat-screen, -flatscreenmode; V's thrust sticks it): case 0 the fire button held before the thrust, case
//   1 after it: charged and fired from the stuck Lance as above, no pull; case 2 a tap of the fire button: a pull, no shot.
//   Ends `embedfireflat: N passed, M failed`.

float VR_EF_SHOT_WAIT = 0.4; // s after the let-go its shot must have come by (the charge's frame: at once)
float VR_EF_HEALTH = 5000;   // the grunt's health (a bolt does 150)
float VR_EF_PULL_AT = 0.15;  // case 3: the charge pulled out at
float VR_EF_PULL_TIME = 0.45; // s the pull takes (out before the charge is full)
float VR_EF_SETTLE = 0.2;    // s after a shot before the next case (the client's charge look fades out on the Lance first)
float vr_ef_flat;
float vr_ef_s;               // the case's steps after its stab (0: the stab's)
float vr_ef_t0;
float vr_ef_pressed;         // the trigger held by the test
float vr_ef_cells;           // noted as the trigger was let go (or as it stuck)
float vr_ef_health;
float vr_ef_shots;           // dw_variant_shots then
float vr_ef_pshots;          // vr_pfire_shots then
float vr_ef_starts;          // dw_lc_starts as the trigger was pressed
float vr_ef_pulls;
string vr_ef_name;

void(float xS) VR_EF_To =
{
    vr_ef_s = xS;
    vr_ef_t0 = time;
};

void(float xOn) VR_EF_Trigger =
{
    vr_ef_pressed = xOn;
    if(vr_ef_flat)
    {
        localcmd(xOn ? "+attack\n" : "-attack\n");
    }
    else
    {
        localcmd(sprintf("vr_mock_button main trigger %g\n", xOn));
    }
};

void(float xAsk) VR_EF_Begin =
{
    vr_ef_flat = xAsk == 78;
    VR_Test_BeginFlags(vr_ef_flat ? "embedfireflat" : "embedfire", VR_TEST_LOUD);
    vr_pe_saved_kd = cvar_hget(cvarh_vr_knockdown);
    vr_pe_saved_stag = cvar_hget(cvarh_vr_hit_stagger);
    vr_pe_saved_push = cvar_hget(cvarh_vr_melee_push);
    cvar_set("vr_knockdown", "0");
    cvar_set("vr_hit_stagger", "0");
    cvar_set("vr_melee_push", "0");
    if(!vr_ef_flat)
    {
        localcmd("vr_mock_hand_to_absolute 1\n");
    }
    vr_eh_home = VR_Player().origin;
    vr_et_on = xAsk;
    vr_et_case = 0;
    VR_EF_To(0);
    VR_ET_To(0);
};

void() VR_EF_Next =
{
    VR_EF_Trigger(FALSE);
    VR_EF_To(0);
    VR_EH_NextCase();
};

// The trigger frames' counters noted (before the let-go whose shot is measured).
void() VR_EF_Note =
{
    entity p = self;
    vr_ef_cells = p.ammo_cells;
    vr_ef_health = vr_et_m != world ? vr_et_m.health : 0;
    vr_ef_shots = dw_variant_shots;
    vr_ef_pshots = vr_pfire_shots;
};

// The stab that sticks the Lance in the grunt (TRUE: done, stuck or not: VR_EH_Blade). Case 0 presses the trigger before it.
float(float t) VR_EF_Stab =
{
    entity p = self;
    if(!vr_ef_flat)
    {
        if(vr_et_s == 0)
        {
            vr_eh_givecmd = "vr_ad_test 207\n";
            vr_eh_flags = 0;
        }
        if(vr_et_s == 3 && vr_et_case == 0 && !vr_ef_pressed)
        {
            VR_EF_Trigger(TRUE); // (aimed, the glide next: charging in the hand as it goes in)
            vr_ef_starts = dw_lc_starts;
        }
        return VR_EH_Blow(WID_LIGHTNING, 6, 0, FALSE, t);
    }
    // Flat-screen: the Lance given, the grunt put ahead, V's thrust (vr_poleembed_test.qc's way).
    if(vr_et_s == 0)
    {
        setorigin(p, vr_eh_home - '0 40 0');
        p.velocity = '0 0 0';
        VRSetHandEmpty(p, cVR_OffHand);
        VRSetHandEmpty(p, cVR_MainHand);
        localcmd("vr_ad_test 207\n");
        vr_et_m = world;
        VR_ET_To(1);
    }
    else if(vr_et_s == 1 && t > 0.8)
    {
        VR_PE_Grunt(42);
        VR_ET_To(2);
    }
    else if(vr_et_s == 2 && t > 0.6)
    {
        if(vr_et_case == 0)
        {
            VR_EF_Trigger(TRUE);
            vr_ef_starts = dw_lc_starts;
        }
        VR_ET_To(3);
    }
    else if(vr_et_s == 3 && t > 0.15)
    {
        localcmd("vr_flat_punch v\n");
        VR_ET_To(4);
    }
    else if(vr_et_s == 4 && (VR_EH_Blade() != world || t > 0.9))
    {
        return TRUE;
    }
    return FALSE;
};

// The shot measured after the let-go: from the stuck Lance `w` (xProp) or from the hand.
void(entity w, float xProp) VR_EF_ShotCheck =
{
    entity p = self;
    float shots = dw_variant_shots - vr_ef_shots;
    float pshots = vr_pfire_shots - vr_ef_pshots;
    VR_Test_Check(shots == 1 && pshots == (xProp ? 1 : 0), sprintf("%s: one shot fired %s (shots %g, from a prop %g)", vr_ef_name,
        xProp ? "from the stuck Lance" : "from the hand", shots, pshots));
    VR_Test_Check(vr_ef_cells - p.ammo_cells == 6, sprintf("%s: 6 cells spent, once (%g)", vr_ef_name, vr_ef_cells - p.ammo_cells));
    VR_Test_Check(vr_et_m != world && vr_ef_health - vr_et_m.health >= 150, sprintf("%s: the grunt hit by the bolt (%g damage)", vr_ef_name,
        vr_et_m != world ? vr_ef_health - vr_et_m.health : 0));
    VR_Test_Check(dw_lc_starts == vr_ef_starts + 1, sprintf("%s: one charge, carried through (%g begun)", vr_ef_name, dw_lc_starts - vr_ef_starts));
    if(xProp && w != world)
    {
        vector m = weapondrawnpose(w, 0, 6);
        VR_Test_Check(vlen(dw_lance_last_from - m) < 1 && vlen(m - w.origin) > 8, sprintf("%s: the bolt from the stuck Lance's tip (%.2f units off its muzzle, %.1f from its handle)",
            vr_ef_name, vlen(dw_lance_last_from - m), vlen(m - w.origin)));
    }
    if(vr_ef_flat && w != world)
    {
        VR_Test_Check(w.vr_hold_flatpulls == vr_ef_pulls, sprintf("%s: the fire button held and let go is no pull (%g pulls)", vr_ef_name, w.vr_hold_flatpulls - vr_ef_pulls));
    }
};

void() VR_EF_Frame =
{
    entity p = self;
    float t = time - vr_et_t0;
    float u = time - vr_ef_t0;
    float h = cVR_MainHand;
    entity w = VR_EH_Blade();
    float full = max(0.05, cvar_hget(cvarh_vr_dw_lance_charge));
    if(vr_et_case > (vr_ef_flat ? 2 : 3))
    {
        VR_EF_Trigger(FALSE);
        if(!vr_ef_flat)
        {
            localcmd("vr_mock_hand_to_absolute 0\n");
        }
        VR_PE_Finish();
        return;
    }
    if((vr_ef_s == 0 && t > VR_ET_GIVEUP) || (vr_ef_s > 0 && u > VR_ET_GIVEUP))
    {
        VR_Test_Check(FALSE, sprintf("timed out: case %g state %g/%g", vr_et_case, vr_et_s, vr_ef_s));
        VR_EF_Next();
        return;
    }
    VR_PE_HoldStill();
    if(vr_et_m != world && vr_et_m.health > 0 && vr_et_m.health < VR_EF_HEALTH - 1000)
    {
        vr_et_m.health = VR_EF_HEALTH; // (the stab's and the bolts' damage measured, never killed)
    }
    if(vr_ef_s == 0)
    {
        if(vr_et_s == 0)
        {
            vr_ef_name = vr_et_case == 0 ? "charged in the hand, let go stuck" : (vr_et_case == 1 ? "charged and let go stuck" :
                (vr_et_case == 3 ? "pulled out charging" : (vr_ef_flat ? "a tap of the fire button" : "the hand off it")));
            p.ammo_cells = 100;
            W_SetCurrentAmmoFor(cVR_MainHand);
            VRSetAttackFinished(cVR_MainHand, 0);
            VRSetAttackFinished(cVR_OffHand, 0);
            vr_ef_pressed = FALSE;
        }
        if(!VR_EF_Stab(t))
        {
            return;
        }
        VR_Test_Check(w != world, sprintf("%s: the Lance sticks in the grunt", vr_ef_name));
        if(w == world)
        {
            VR_EF_Next();
            return;
        }
        VR_Test_Check(VR_PropFire_Held(p, h) == w, sprintf("%s: its hand drives it (embed_fire %g, hands %g)", vr_ef_name, weaponvalue(w.model, "embed_fire"), w.vr_hold_hands));
        vr_ef_pulls = w.vr_hold_flatpulls;
        if(vr_et_case == 0)
        {
            VR_Test_Check(p.dw_lc1 > 0 && dw_lc_starts == vr_ef_starts + 1, sprintf("%s: stuck charging (%.2f), the hand's charge (%g begun)", vr_ef_name,
                p.dw_lc1, dw_lc_starts - vr_ef_starts));
            VR_EF_To(12);
        }
        else if(vr_et_case == 1 || (vr_ef_flat && vr_et_case == 2))
        {
            VR_EF_To(11);
        }
        else if(vr_et_case == 2)
        {
            localcmd("-grabmain\nvr_mock_button main grip 0\n");
            VR_EF_To(20);
        }
        else
        {
            VR_EF_To(40);
        }
        return;
    }
    // Cases 0 and 1 (and the flat tap's press): charged on the stuck Lance, let go, its shot.
    if(vr_ef_s == 11 && u > 0.2)
    {
        VR_EF_Trigger(TRUE);
        vr_ef_starts = dw_lc_starts;
        VR_EF_To((vr_ef_flat && vr_et_case == 2) ? 30 : 12);
    }
    else if(vr_ef_s == 12 && (p.dw_lc1 >= 1 || u > full + 1))
    {
        VR_Test_Check(p.dw_lc1 >= 1 && w != world && VR_PropFire_Held(p, h) == w, sprintf("%s: charged full on the stuck Lance (%.2f), still held", vr_ef_name, p.dw_lc1));
        VR_EF_Note();
        VR_EF_Trigger(FALSE);
        VR_EF_To(13);
    }
    else if(vr_ef_s == 13 && (dw_variant_shots != vr_ef_shots || u > VR_EF_SHOT_WAIT))
    {
        VR_EF_ShotCheck(w, TRUE);
        VR_EF_To(14);
    }
    else if((vr_ef_s == 14 || vr_ef_s == 45) && u > VR_EF_SETTLE)
    {
        VR_EF_Next();
    }
    // Case 2 (VR): the hand off it, the trigger does nothing.
    else if(vr_ef_s == 20 && (w == world || u > 1))
    {
        entity was = find(world, classname, "thrown_weapon");
        VR_Test_Check(w == world && was != world && was.vr_hold_hands == 0 && VR_AxeStick_IsStuck(was), sprintf("%s: let go, it stays stuck (hands %g)",
            vr_ef_name, was != world ? was.vr_hold_hands : -1));
        VR_EF_Note();
        vr_ef_starts = dw_lc_starts;
        VR_EF_Trigger(TRUE);
        VR_EF_To(21);
    }
    else if(vr_ef_s == 21 && u > full + 0.3)
    {
        VR_EF_Trigger(FALSE);
        VR_EF_To(22);
    }
    else if(vr_ef_s == 22 && u > VR_EF_SHOT_WAIT)
    {
        VR_Test_Check(dw_variant_shots == vr_ef_shots && vr_pfire_shots == vr_ef_pshots && p.ammo_cells == vr_ef_cells && dw_lc_starts == vr_ef_starts,
            sprintf("%s: the trigger held and let go fires nothing (shots %g, charges %g, cells %g)", vr_ef_name, dw_variant_shots - vr_ef_shots,
                dw_lc_starts - vr_ef_starts, vr_ef_cells - p.ammo_cells));
        VR_EF_Next();
    }
    // Case 2 (flat): a tap pulls, no shot.
    else if(vr_ef_s == 30 && u > 0.1)
    {
        VR_EF_Note();
        VR_EF_Trigger(FALSE);
        VR_EF_To(31);
    }
    else if(vr_ef_s == 31 && u > 0.15)
    {
        float pulls = w != world ? w.vr_hold_flatpulls - vr_ef_pulls : -1;
        VR_Test_Check(pulls == 1 && dw_variant_shots == vr_ef_shots && p.ammo_cells == vr_ef_cells, sprintf("%s: a pull, no shot (pulls %g, shots %g, cells %g)",
            vr_ef_name, pulls, dw_variant_shots - vr_ef_shots, vr_ef_cells - p.ammo_cells));
        VR_EF_Next();
    }
    // Case 3 (VR): pulled out charging, the charge goes on in the hand, fired from it.
    else if(vr_ef_s == 40)
    {
        VR_EF_Trigger(TRUE);
        vr_ef_starts = dw_lc_starts;
        VR_EF_To(41);
    }
    else if(vr_ef_s == 41 && (p.dw_lc1 >= VR_EF_PULL_AT || u > full + 1))
    {
        VR_Test_Check(p.dw_lc1 >= VR_EF_PULL_AT && p.dw_lc1 < 1 && w != world, sprintf("%s: charging on the stuck Lance (%.2f)", vr_ef_name, p.dw_lc1));
        if(w != world)
        {
            // (Pulled out along it, past how deep it is, at a hand's pace: vr_embed_test 7's way.)
            VR_EH_HandTo(VRGetHandPos(h) - w.vr_stick_dir * (max(0, w.vr_hold_err) + w.vr_stick_depth + w.vr_stick_slide + 12), VR_EF_PULL_TIME);
        }
        VR_EF_To(42);
    }
    else if(vr_ef_s == 42 && ((w == world && VRGetEntWeapon(p, h) == WID_LIGHTNING) || u > VR_EF_PULL_TIME + 1))
    {
        VR_Test_Check(w == world && DW_LanceInHand(h) && p.dw_lc1 > 0 && dw_lc_starts == vr_ef_starts + 1,
            sprintf("%s: out in the hand, the charge going on (%.2f, %g begun)", vr_ef_name, p.dw_lc1, dw_lc_starts - vr_ef_starts));
        VR_EF_To(43);
    }
    else if(vr_ef_s == 43 && (p.dw_lc1 >= 1 || u > full + 1))
    {
        VR_EF_Note();
        VR_EF_Trigger(FALSE);
        VR_EF_To(44);
    }
    else if(vr_ef_s == 44 && (dw_variant_shots != vr_ef_shots || u > VR_EF_SHOT_WAIT))
    {
        VR_EF_ShotCheck(world, FALSE);
        VR_EF_To(45);
    }
};
#endif
