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
#endif
