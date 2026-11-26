#ifdef QVR_TESTS
// vr_poleembed_test.qc -- flatembed3/4 (his flat items 7 and 10, 2026-11-25/26), flat-screen play only (-flatscreenmode; map
// vrtesthall, god, notarget, Dwell's data for the Crystal Lance). The engine's probes (vr_embed_probe, vr_view_hands.cpp):
// `vr_embed_test 71` (item 7, "the off-hand should actually hold the shaft"): the spear (case 0) and Dwell's Crystal Lance
//   (case 1, vr_ad_test 207) in the main hand, the other hand on its pole, in four states: held upright, the flipped grip (impulse
//   43), the parry guard (+offhandattack held) and stuck in a grunt by V's thrust (held still), the other hand joining it. In each
//   the drawn other hand, after everything that places it, has its grip channel round the drawn shaft: vr_embed_probe_pdist (cm
//   off the shaft's line) under VR_PE_DIST, _pturn (degrees off its way) under VR_PE_TURN, over VR_PE_FRAMES frames (_pon).
//   Ends `poleembed: N passed, M failed`.
// `vr_embed_test 72` (item 10, "the hand and bladed weapon look very jittery/springy"): the sword stuck in a grunt by V's thrust,
//   the grunt killed (a ragdoll with it in, the hand on it), a second of the ragdoll's fall measured (vr_embed_probe_jraw: its
//   frame-to-frame change of velocity and turn rate in the body; _jdrawn: as drawn; _jlag: cm the drawn blade trailed it; _jon:
//   frames; _hoff: cm the hand on its handle was off its place on the weapon). Case 0 with vr_embed_ragdoll_smooth_time as set
//   (its default 0.04): drawn under VR_PE_CALM of the body's jitter, the lag within vr_embed_ragdoll_smooth_max_pos, the hand on
//   the handle. Case 1 with it 0: drawn as in the body (no lag, the same jitter). Ends `poleembedjit: N passed, M failed`.

float VR_PE_DIST = 1;     // cm the other hand's grip channel may be off the drawn shaft's line
float VR_PE_TURN = 5;     // degrees it may be turned off the shaft's way
float VR_PE_FINGERS = 3;  // the fewest of its four fingers closed on the pole (flatgrip3: a fist beside the shaft closes none)
float VR_PE_FRAMES = 10;  // frames each state must have the hand drawn wholly on the shaft
float VR_PE_CALM = 0.85;  // the most of the body's jitter the smoothed blade may still be drawn with
float VR_PE_HANDLE = 0.5; // cm the hand on the handle may be off its place on the weapon
float VR_PE_MEASURE = 0.5; // s each pole state is measured (36 frames)
float VR_PE_FALL = 1;      // s of the ragdoll's fall measured
float VR_PE_SPEAR_AHEAD = 32; // units ahead the grunt the spear sticks in stands (its stuck shaft in the flat arm's reach)

float vr_pe_saved_kd;
float vr_pe_saved_stag;
float vr_pe_saved_push;
float vr_pe_saved_smooth;
string vr_pe_name;

void() VR_PE_Finish =
{
    localcmd("-offhandattack\n");
    cvar_set("vr_knockdown", ftos(vr_pe_saved_kd));
    cvar_set("vr_hit_stagger", ftos(vr_pe_saved_stag));
    cvar_set("vr_melee_push", ftos(vr_pe_saved_push));
    localcmd("tune_reset vr_embed_ragdoll_smooth_time\n");
    cvar_set("vr_embed_probe", "");
    VR_Test_Verdict();
    vr_et_on = 0;
};

void(float xTest) VR_PE_Begin =
{
    VR_Test_BeginFlags(xTest == 71 ? "poleembed" : "poleembedjit", VR_TEST_LOUD);
    vr_pe_saved_kd = cvar_hget(cvarh_vr_knockdown);
    vr_pe_saved_stag = cvar_hget(cvarh_vr_hit_stagger);
    vr_pe_saved_push = cvar_hget(cvarh_vr_melee_push);
    vr_pe_saved_smooth = cvar_hget(cvarh_vr_embed_ragdoll_smooth_time);
    cvar_set("vr_knockdown", "0");
    cvar_set("vr_hit_stagger", "0");
    cvar_set("vr_melee_push", "0"); // (the blow's push would carry the body off the blade)
    vr_eh_home = VR_Player().origin;
    vr_et_on = xTest;
    vr_et_case = 0;
    VR_ET_To(0);
};

void() VR_PE_Next =
{
    localcmd("-offhandattack\n");
    VRSetHandEmpty(self, cVR_OffHand);
    VR_EH_NextCase();
};

// The pole probe's numbers for the state `xState` since its reset.
void(string xState) VR_PE_PoleCheck =
{
    float d = cvar_hget(cvarh_vr_embed_probe_pdist);
    float r = cvar_hget(cvarh_vr_embed_probe_pturn);
    float n = cvar_hget(cvarh_vr_embed_probe_pon);
    float f = cvar_hget(cvarh_vr_embed_probe_pmet);
    VR_Test_Check(n >= VR_PE_FRAMES && d >= 0 && d <= VR_PE_DIST && r >= 0 && r <= VR_PE_TURN && f >= VR_PE_FINGERS,
        sprintf("%s, %s: the other hand round the drawn shaft (%.2f cm off its line, %.1f degrees off its way, %g of 4 fingers on it, %g frames)",
            vr_pe_name, xState, d, r, f, n));
    if(cvar_hget(cvarh_vr_embed_probe_shots))
    {
        localcmd("screenshot\n"); // (flatgrip3: vr_embed_probe_shots 1 for a picture of each state, the probe's last frame)
    }
};

// (The grunt ahead held where it stands, as test 64's.)
void() VR_PE_HoldStill =
{
    if(vr_et_m != world && (vr_et_m.flags & FL_MONSTER) && vr_et_m.health > 0 && vr_et_m.takedamage)
    {
        vr_et_m.nextthink = time + 0.5;
        vr_et_m.velocity = '0 0 0';
    }
};

// (`xAhead`: the spear's stuck shaft at 42, test 64's, is out of the flat arm's reach -- both hands drawn short of it, the other
// one as near the shaft as its arm gets, not on it --, so its stuck state stands nearer.)
void(float xAhead) VR_PE_Grunt =
{
    entity p = self;
    makevectors('0 1 0' * p.v_viewangle_y);
    vr_et_m = VR_ET_Grunt(xAhead);
    setorigin(vr_et_m, p.origin + v_forward * xAhead - '0 0 1');
};

void() VR_PE_PoleFrame =
{
    entity p = self;
    float t = time - vr_et_t0;
    float h = cVR_MainHand;
    entity w = VR_EH_Blade();
    if(vr_et_case > 1)
    {
        VR_PE_Finish();
        return;
    }
    if(t > VR_ET_GIVEUP)
    {
        VR_Test_Check(FALSE, sprintf("timed out: case %s state %g", vr_pe_name, vr_et_s));
        VR_PE_Next();
        return;
    }
    VR_PE_HoldStill();
    if(vr_et_s == 0)
    {
        setorigin(p, vr_eh_home - '0 40 0');
        p.velocity = '0 0 0';
        VRSetHandEmpty(p, cVR_OffHand);
        vr_pe_name = vr_et_case == 0 ? "spear" : "Crystal Lance";
        if(vr_et_case == 0)
        {
            VR_EH_Give(WID_SWORD, VR_ADMelee_Flags(VR_SWORDKIND_SPEAR));
        }
        else
        {
            VRSetHandEmpty(p, h);
            localcmd("vr_ad_test 207\n"); // (the Lance in the main hand, as Dwell's own draw sets it up)
        }
        vr_et_m = world;
        VR_ET_To(1);
    }
    else if(vr_et_s == 1 && t > 0.8)
    {
        VR_Test_Check(VR_PoleHeld(h), sprintf("%s held in the main hand (a pole weapon; holds %g)", vr_pe_name, VRGetEntWeapon(p, h)));
        localcmd("vr_embed_probe reset\n");
        VR_ET_To(2);
    }
    else if(vr_et_s == 2 && t > VR_PE_MEASURE)
    {
        VR_PE_PoleCheck("held upright");
        localcmd("impulse 43\n"); // (no other ammo: its grip flips)
        VR_ET_To(3);
    }
    else if(vr_et_s == 3 && t > 0.4)
    {
        VR_Test_Check(VRGetEntWeaponFlags(p, h) & QVR_WPNFLAG_REVGRIP, sprintf("%s: impulse 43 flips its grip", vr_pe_name));
        localcmd("vr_embed_probe reset\n");
        VR_ET_To(4);
    }
    else if(vr_et_s == 4 && t > VR_PE_MEASURE)
    {
        VR_PE_PoleCheck("the flipped grip");
        localcmd("impulse 43\n");
        VR_ET_To(5);
    }
    else if(vr_et_s == 5 && t > 0.4)
    {
        localcmd("+offhandattack\n"); // (the right mouse button held: the parry guard)
        VR_ET_To(6);
    }
    else if(vr_et_s == 6 && t > 0.5)
    {
        localcmd("vr_embed_probe reset\n");
        VR_ET_To(7);
    }
    else if(vr_et_s == 7 && t > VR_PE_MEASURE)
    {
        VR_PE_PoleCheck("the parry guard");
        localcmd("-offhandattack\n");
        VR_PE_Grunt(vr_et_case == 0 ? VR_PE_SPEAR_AHEAD : 42);
        VR_ET_To(8);
    }
    else if(vr_et_s == 8 && t > 0.6)
    {
        localcmd("vr_flat_punch v\n");
        VR_ET_To(9);
    }
    else if(vr_et_s == 9 && (w != world || t > 0.9))
    {
        VR_Test_Check(w != world, sprintf("%s: V's thrust sticks it in the grunt", vr_pe_name));
        if(w == world)
        {
            VR_PE_Next();
            return;
        }
        VR_ET_To(10);
    }
    else if(vr_et_s == 10 && t > 0.3)
    {
        VR_Test_Check(w != world && w.vr_hold_hands == 3, sprintf("%s: stuck, both hands on it (hands %g)", vr_pe_name, w != world ? w.vr_hold_hands : 0));
        localcmd("vr_embed_probe reset\n");
        VR_ET_To(11);
    }
    else if(vr_et_s == 11 && t > VR_PE_MEASURE)
    {
        VR_PE_PoleCheck("stuck in a body");
        VR_PE_Next();
    }
};

void() VR_PE_JitterFrame =
{
    entity p = self;
    float t = time - vr_et_t0;
    entity w = VR_EH_Blade();
    if(vr_et_case > 1)
    {
        VR_PE_Finish();
        return;
    }
    if(t > VR_ET_GIVEUP)
    {
        VR_Test_Check(FALSE, sprintf("timed out: case %s state %g", vr_pe_name, vr_et_s));
        VR_PE_Next();
        return;
    }
    if(vr_et_s < 3)
    {
        VR_PE_HoldStill();
    }
    if(vr_et_s == 0)
    {
        setorigin(p, vr_eh_home - '0 40 0');
        p.velocity = '0 0 0';
        VRSetHandEmpty(p, cVR_OffHand);
        vr_pe_name = vr_et_case == 0 ? "smoothed" : "smoothing off";
        // (A tunable: set by `tune`, as the menu's slider does; cvar_set from the QC doesn't reach it.)
        localcmd(vr_et_case == 0 ? "tune_reset vr_embed_ragdoll_smooth_time\n" : "tune vr_embed_ragdoll_smooth_time 0\n");
        cvar_set("vr_embed_probe", "on"); // (the hand's place on the sword held, kept before it goes in: embedGripProbe)
        VR_EH_Give(WID_SWORD, 0);
        VR_PE_Grunt(42);
        VR_ET_To(1);
    }
    else if(vr_et_s == 1 && t > 0.4 && vr_et_m.takedamage)
    {
        localcmd("vr_flat_punch v\n");
        VR_ET_To(2);
    }
    else if(vr_et_s == 2 && (w != world || t > 0.9))
    {
        VR_Test_Check(w != world, sprintf("%s: V's thrust sticks the sword in the grunt", vr_pe_name));
        if(w == world)
        {
            VR_PE_Next();
            return;
        }
        vr_et_m.health = 1;
        T_Damage(vr_et_m, p, p, 2); // (a ragdoll with the sword in, the hand on it)
        localcmd("vr_embed_probe reset\n");
        VR_ET_To(3);
    }
    else if(vr_et_s == 3 && (t > VR_PE_FALL || w == world))
    {
        float raw = cvar_hget(cvarh_vr_embed_probe_jraw);
        float drawn = cvar_hget(cvarh_vr_embed_probe_jdrawn);
        float lag = cvar_hget(cvarh_vr_embed_probe_jlag);
        float n = cvar_hget(cvarh_vr_embed_probe_jon);
        float hoff = cvar_hget(cvarh_vr_embed_probe_hoff);
        float most = cvar_hget(cvarh_vr_embed_ragdoll_smooth_max_pos);
        VR_Test_Print(sprintf("%s: %g frames, jitter in the body %.3f, drawn %.3f, the most lag %.2f cm, the hand %.2f cm off the handle\n",
            vr_test_tag, n, raw, drawn, lag, hoff));
        VR_Test_Check(n >= 20, sprintf("%s: the held blade measured in the ragdoll (%g frames, held %g)", vr_pe_name, n, w != world));
        VR_Test_Check(raw > 0, sprintf("%s: the ragdoll moves it (jitter %.3f in the body)", vr_pe_name, raw));
        if(vr_et_case == 0)
        {
            VR_Test_Check(vr_pe_saved_smooth > 0, sprintf("vr_embed_ragdoll_smooth_time on by default (%g)", vr_pe_saved_smooth));
            VR_Test_Check(drawn <= raw * VR_PE_CALM, sprintf("%s: drawn with less jitter than in the body (%.3f of %.3f: %.0f%%, at most %.0f%%)", vr_pe_name,
                drawn, raw, raw > 0 ? drawn / raw * 100 : 0, VR_PE_CALM * 100));
            VR_Test_Check(lag >= 0 && lag <= most + 0.1, sprintf("%s: the drawn blade trails its wound by %.2f cm at most (vr_embed_ragdoll_smooth_max_pos %g)", vr_pe_name, lag, most));
        }
        else
        {
            VR_Test_Check(fabs(drawn - raw) <= raw * 0.02 + 0.001, sprintf("%s: drawn as in the body (jitter %.3f of %.3f)", vr_pe_name, drawn, raw));
            VR_Test_Check(lag >= 0 && lag < 0.05, sprintf("%s: no lag (%.2f cm)", vr_pe_name, lag));
        }
        VR_Test_Check(hoff >= 0 && hoff <= VR_PE_HANDLE, sprintf("%s: the hand on the handle as the blade is drawn (%.2f cm off it)", vr_pe_name, hoff));
        VR_PE_Next();
    }
};

// ---- vr_embed_test 73 (vrspear; his VR notes 2026-11-26 20:07-20:10), mock VR (no -flatscreenmode), map vrtesthall: the spear
// (cases 1 and 2) and Dwell's Crystal Lance (case 0) in the gripping main hand, aimed at a grunt as test 7's stabs are (VR_EH_Blow,
// its states 0 to 2); then the off hand put on the pole VR_VS_SPOT of the way from the handle to the tip, VR_VS_CM cm over it (the
// hand's point, not its palm: a grip with an offset), gripping: VR's pole grip (weaponanygrip) takes it, and the drawn hand has its
// grip channel round the drawn shaft, fingers on it (the probe of test 71). The hand then moved VR_VS_SLIDE further along the pole
// and back: with vr_pole_grip_fixed 1 (cases 0, 2) the drawn hand stays where it took hold (vr_embed_probe_pslide), with 0 (case 2)
// it slides. (Case 0 with the main hand's analog grip loose, as the mock's +grabmain leaves it: the main hand slides along the pole
// instead, poleShift, and the off hand is drawn on the same point of it; cases 1 and 2 with it closed: the off hand's own place,
// kept or slid. The mock's grip read as analog from then on, the closed case go last.) Then both hands glided into the grunt at
// 6 m/s (the stab): it sticks, both hands on it, and over VR_PE_MEASURE the off hand stays on it (no let-go), round the stuck shaft,
// where it was along the shaft as held (vr_embed_probe_palong).
// Ends `vrspear: N passed, M failed`.
float VR_VS_SLIDE = 0.15; // of the handle-to-tip length the off hand moves along the pole, gripping
float VR_VS_STILL = 1;    // cm the drawn hand may move along the shaft, fixed
float VR_VS_SLID = 3;     // cm it must move along it, sliding
float VR_VS_KEEP = 2;     // cm the hand on the stuck shaft may be off where it was along it as held
float vr_vs_s;            // the off hand's steps (0: VR_EH_Blow's)
float vr_vs_t0;
float vr_vs_along;        // cm along the shaft the drawn hand was, held, before the stab
float vr_vs_spot;
float vr_vs_cm;
float vr_vs_last;         // the run's last case

void(float xS) VR_VS_To =
{
    vr_vs_s = xS;
    vr_vs_t0 = time;
};

void(float xAsk) VR_VS_Begin =
{
    VR_Test_BeginFlags("vrspear", VR_TEST_LOUD);
    vr_pe_saved_kd = cvar_hget(cvarh_vr_knockdown);
    vr_pe_saved_stag = cvar_hget(cvarh_vr_hit_stagger);
    vr_pe_saved_push = cvar_hget(cvarh_vr_melee_push);
    cvar_set("vr_knockdown", "0");
    cvar_set("vr_hit_stagger", "0");
    cvar_set("vr_melee_push", "0");
    localcmd("vr_mock_hand_to_absolute 1\n");
    vr_eh_home = VR_Player().origin;
    vr_et_on = 73;
    vr_et_case = xAsk == 74 ? 2 : 0; // (74: the fixed spear alone, a run of its own: a third case's off hand in one run never took the pole)
    vr_vs_last = xAsk == 74 ? 2 : 1;
    VR_VS_To(0);
    VR_ET_To(0);
};

void() VR_VS_Next =
{
    localcmd("-grableft\nvr_mock_button off grip 0\nvr_mock_hand_glide 0\n");
    VRSetHandEmpty(self, cVR_OffHand);
    VR_VS_To(0);
    VR_EH_NextCase();
};

void() VR_VS_Frame =
{
    entity p = self;
    float t = time - vr_et_t0;
    float u = time - vr_vs_t0;
    float off = cVR_OffHand;
    entity w = VR_EH_Blade();
    float fixed = vr_et_case != 1; // (the sliding case second: the third case's off hand never took the pole, whichever it was)
    if(vr_et_case > vr_vs_last)
    {
        localcmd("vr_pole_grip_fixed 1\nvr_mock_hand_to_absolute 0\n");
        VR_PE_Finish();
        return;
    }
    if((vr_vs_s == 0 && t > VR_ET_GIVEUP) || (vr_vs_s > 0 && u > VR_ET_GIVEUP))
    {
        VR_Test_Check(FALSE, sprintf("timed out: case %g state %g/%g", vr_et_case, vr_et_s, vr_vs_s));
        VR_VS_Next();
        return;
    }
    VR_PE_HoldStill();
    if(vr_vs_s == 0)
    {
        if(vr_et_s == 0)
        {
            vr_pe_name = vr_et_case == 0 ? "Crystal Lance" : (fixed ? "spear" : "spear, sliding");
            localcmd(sprintf("vr_pole_grip_fixed %g\nvr_embed_probe reset\n", fixed));
            VRSetHandEmpty(p, off);
            vr_eh_flags = vr_et_case == 0 ? 0 : VR_ADMelee_Flags(VR_SWORDKIND_SPEAR);
            vr_eh_givecmd = vr_et_case == 0 ? "vr_ad_test 207\n" : "";
            vr_vs_spot = vr_et_case == 0 ? 0.6 : 0.55;
            vr_vs_cm = vr_et_case == 0 ? 1 : 2; // (the hand's point that far over the shaft: its fist touches it, as grabbing anywhere needs)
        }
        if(vr_et_s == 3)
        {
            VR_VS_To(1); // (aimed, the hand back at the stab's start: the off hand's turn)
            return;
        }
        VR_EH_Blow(vr_et_case == 0 ? WID_LIGHTNING : WID_SWORD, 6, 0, FALSE, t);
        return;
    }
    if(vr_vs_s == 1 && u > 0.3)
    {
        if(!VR_PoleHeld(cVR_MainHand) || vr_vs_cm == (vr_et_case == 0 ? 1 : 2))
        {
            VR_Test_Check(VR_PoleHeld(cVR_MainHand), sprintf("%s held in the main hand (a pole weapon; holds %g)", vr_pe_name, VRGetEntWeapon(p, cVR_MainHand)));
        }
        localcmd(sprintf("vr_mock_hand_to off held %g %g\n", vr_vs_spot, vr_vs_cm));
        VR_VS_To(2);
    }
    else if(vr_vs_s == 2 && u > 0.1)
    {
        localcmd(sprintf("vr_mock_hand_to off held %g %g\n", vr_vs_spot, vr_vs_cm)); // (again: from where it got to)
        VR_VS_To(3);
    }
    else if(vr_vs_s == 3 && u > 0.1)
    {
        localcmd("+grableft\nvr_mock_button off grip 1\n");
        if(vr_et_case > 0)
        {
            localcmd("vr_mock_fingers main 0 1\n"); // (the main hand's grip closed: it holds, it doesn't slide)
        }
        VR_VS_To(4);
    }
    else if(vr_vs_s == 4 && weaponanygrip(p, off) == 0 && u > 0.6 && vr_vs_cm > -2)
    {
        // (The fist not touching the shaft from there -- grabbing anywhere needs it on the surface, and the mock hand's turn is
        // whatever the last case left --: let go, a centimetre lower, again.)
        vr_vs_cm = vr_vs_cm - 1;
        localcmd("-grableft\nvr_mock_button off grip 0\n");
        VR_VS_To(1);
    }
    else if(vr_vs_s == 4 && ((weaponanygrip(p, off) != 0 && u > 0.3) || u > 1.5))
    {
        VR_Test_Check(weaponanygrip(p, off) != 0, sprintf("%s: the off hand %g of the way up, %g cm over the shaft, takes the pole", vr_pe_name, vr_vs_spot, vr_vs_cm));
        if(weaponanygrip(p, off) == 0)
        {
            VR_VS_Next();
            return;
        }
        localcmd("vr_embed_probe reset\n");
        VR_VS_To(5);
    }
    else if(vr_vs_s == 5 && u > VR_PE_MEASURE)
    {
        VR_PE_PoleCheck(sprintf("held (the hand put %g cm over the shaft)", vr_vs_cm));
        localcmd(sprintf("vr_embed_probe reset\nvr_mock_hand_to off held %g %g\n", vr_vs_spot + VR_VS_SLIDE, vr_vs_cm));
        VR_VS_To(6);
    }
    else if(vr_vs_s == 6 && u > 0.4)
    {
        float slid = cvar_hget(cvarh_vr_embed_probe_pslide);
        if(fixed)
        {
            VR_Test_Check(slid >= 0 && slid <= VR_VS_STILL && weaponanygrip(p, off) != 0,
                sprintf("%s: the hand moved along the pole, the drawn hand stays where it took hold (%.2f cm along it; holds %g)", vr_pe_name, slid, weaponanygrip(p, off)));
        }
        else
        {
            VR_Test_Check(slid >= VR_VS_SLID && weaponanygrip(p, off) != 0,
                sprintf("vr_pole_grip_fixed 0: the drawn hand slides along the pole with it (%.2f cm; holds %g)", slid, weaponanygrip(p, off)));
        }
        localcmd(sprintf("vr_mock_hand_to off held %g %g\n", vr_vs_spot, vr_vs_cm)); // (back where it took hold)
        VR_VS_To(7);
    }
    else if(vr_vs_s == 7 && u > 0.4)
    {
        localcmd("vr_embed_probe reset\n");
        VR_VS_To(8);
    }
    else if(vr_vs_s == 8 && u > 0.2)
    {
        vr_vs_along = cvar_hget(cvarh_vr_embed_probe_palong);
        VR_EH_Blow(vr_et_case == 0 ? WID_LIGHTNING : WID_SWORD, 6, 0, FALSE, t); // (its state 3: the main hand's stab glide)
        localcmd(sprintf("vr_mock_hand_to off by %v\n", vr_eh_end - vr_eh_start)); // (the off hand with it, as glided)
        VR_VS_To(9);
    }
    else if(vr_vs_s == 9 && (w != world || u > vr_eh_for + 0.4))
    {
        VR_Test_Check(w != world, sprintf("%s: the stab with both hands sticks it in the grunt", vr_pe_name));
        if(w == world)
        {
            VR_VS_Next();
            return;
        }
        localcmd("vr_embed_probe reset\n");
        VR_VS_To(10);
    }
    else if(vr_vs_s == 10 && u > 0.1)
    {
        VR_Test_Check(w.vr_hold_hands == 3, sprintf("%s: stuck, both hands on it (hands %g)", vr_pe_name, w.vr_hold_hands));
        localcmd("vr_embed_probe reset\n");
        VR_VS_To(11);
    }
    else if(vr_vs_s == 11 && u > VR_PE_MEASURE)
    {
        float at = cvar_hget(cvarh_vr_embed_probe_palong);
        VR_Test_Check(w != world && w.vr_hold_hands == 3, sprintf("%s: the off hand still on it %.1f s on (hands %g)", vr_pe_name, VR_PE_MEASURE + 0.1, w != world ? w.vr_hold_hands : 0));
        VR_PE_PoleCheck("stuck in the grunt");
        VR_Test_Check(fabs(at - vr_vs_along) <= VR_VS_KEEP && cvar_hget(cvarh_vr_embed_probe_pslide) <= VR_VS_STILL,
            sprintf("%s: the off hand stays where it held the shaft (%.2f cm along it, held %.2f; it moved %.2f cm along it)", vr_pe_name, at, vr_vs_along,
                cvar_hget(cvarh_vr_embed_probe_pslide)));
        VR_VS_Next();
    }
};
#endif
