#ifdef QVR_TESTS
//
//
//
// ----------------------------------------------------------------------------
// vr_armedgrunt_preparry_test.qc -- `vr_agrunt_test 95` and `96` (Debug > Tests > Armed Grunts: Preventive Parry,
// Wind-up Parry): an armed humanoid's guard (vr_armedgrunt_guard.qc). In vrtesthall, you unseen, a fresh grunt ahead for
// each case, held still and aware of you (its weapon's think runs: the guard as in play), the times exact (fixed frames).
// The reactive parry off in both (only the guard raises its parry); your hands empty (the fists' category multipliers
// tuned to 1, the others left as they are).
// 95 (vr_agrunt_preparry; the wind-up parry off; chance 1, checks every 0.1 s, delay 0.2, guard 0.8, recovery 0.6):
//   0 a shotgun grunt 36 units off, its shove ready: no guard in 1 s, no roll;
//   1 its shove cooling down: the guard rises 0.2 s after the roll and stays 0.8 s ("preparry" on Show Disarm Volumes);
//     a fist on its barrel in it (strike beat 0): parried, armed; one in the recovery: disarmed;
//   2 ... its recovery lasts 0.6 s (it can't parry), then the guard rises again;
//   3 the melee's punch through its barrel into its chest in the guard: parried (the damage cut); in the recovery:
//     disarmed, the damage not cut;
//   4 176 units off (range 64): no guard; 5 chance 0: rolls every check, never a guard;
//   6 a sword grunt, its shove ready: the guard rises (a melee weapon in its hand);
//   7 random times (delay 0.1-0.4, guard 0.3-0.6, recovery 0.2-0.5): 4 guards, each time within its range, not all equal.
// 96 (vr_agrunt_windup_parry; the preventive off; chance 1, read 0.08 s, delay 0.15, guard 0.5, recovery 0.6, rearm 1;
//   your main hand a closed fist placed each frame against your head and the way to it):
//   0 the hand at rest ahead of your chest: no guard, no roll in 1 s;
//   1 raised overhead: the guard rises 0.23 s later (read + delay), stays 0.5 s ("windup"); lowered, a fist on its
//     barrel in the recovery: disarmed (the bait);
//   2 held overhead 2 s: one guard (a held raise read once); lowered 0.2 s and raised again: a second;
//   3 cocked out to the side: the guard rises; 4 chance 0, held 1 s: one roll, no guard;
//   5 by what you hold (bare rolls, 400 each, chances 0.5, delays 0.1-0.3, the default multipliers): an axe raised is
//     parried more often and sooner than a shotgun (the wind-up's and the preventive's), each delay within its range.
// Each case `agtest: PASS|FAIL guard <test>.<case>: ...`; ends on `agtest: test 95 over` (96).
// ----------------------------------------------------------------------------

float vr_agpp_case;      // the case running
float vr_agpp_until;     // its give-up time (a safety net: FAIL "timed out")
float vr_agpp_t0;        // the case's start (its grunt held)
float vr_agpp_prev;      // its guard's state last frame
float vr_agpp_t1;        // when the last roll was won (state 1), the guard raised, dropped, its recovery over
float vr_agpp_tR;
float vr_agpp_tD;
float vr_agpp_tE;
float vr_agpp_raises;    // guards raised this case
float vr_agpp_rolls0;    // vr_agg_rolls at the case's start
float vr_agpp_sub;       // the case's own step
float vr_agpp_tRaise;    // when your hand was raised (96)
float vr_agpp_pose;      // your hand's pose (96): 0 rest, 1 overhead, 2 side
float vr_agpp_n0;
float vr_agpp_ok;        // the case's checks so far (each printed as it is made)
float vr_agpp_d_lo;      // case 7's least and most delay seen
float vr_agpp_d_hi;
float VR_AGPP_FRAME = 0.0139; // s: one fixed frame (1/72)
float VR_AGPP_KEEPS = 28;   // the tunables (then the cvars, from VR_AGPP_KEEP_CVARS) the tests change
float VR_AGPP_KEEP_CVARS = 24;
float vr_agpp_keep[28];   // ... as they were

// The `i`th of them.
string(float i) VR_AGPP_KeepName =
{
    if(i == 0) { return "vr_agrunt_preparry_chance"; }
    if(i == 1) { return "vr_agrunt_preparry_interval"; }
    if(i == 2) { return "vr_agrunt_preparry_delay_min"; }
    if(i == 3) { return "vr_agrunt_preparry_delay_max"; }
    if(i == 4) { return "vr_agrunt_preparry_guard_min"; }
    if(i == 5) { return "vr_agrunt_preparry_guard_max"; }
    if(i == 6) { return "vr_agrunt_preparry_recover_min"; }
    if(i == 7) { return "vr_agrunt_preparry_recover_max"; }
    if(i == 8) { return "vr_agrunt_windup_chance"; }
    if(i == 9) { return "vr_agrunt_windup_read"; }
    if(i == 10) { return "vr_agrunt_windup_delay_min"; }
    if(i == 11) { return "vr_agrunt_windup_delay_max"; }
    if(i == 12) { return "vr_agrunt_windup_guard_min"; }
    if(i == 13) { return "vr_agrunt_windup_guard_max"; }
    if(i == 14) { return "vr_agrunt_windup_recover_min"; }
    if(i == 15) { return "vr_agrunt_windup_recover_max"; }
    if(i == 16) { return "vr_agrunt_windup_rearm"; }
    if(i == 17) { return "vr_agrunt_pcat_fist_chance"; }
    if(i == 18) { return "vr_agrunt_pcat_fist_delay"; }
    if(i == 19) { return "vr_agrunt_pcat_fist_guard"; }
    if(i == 20) { return "vr_agrunt_pcat_fist_range"; }
    if(i == 21) { return "vr_agrunt_pcat_fist_windup"; }
    if(i == 22) { return "vr_agrunt_pcat_fist_pre"; }
    if(i == 23) { return "vr_agrunt_strike_beat"; }
    if(i == 24) { return "vr_agrunt_preparry"; }
    if(i == 25) { return "vr_agrunt_windup_parry"; }
    if(i == 26) { return "vr_agrunt_react_parry"; }
    if(i == 27) { return "vr_agrunt_parry"; }
    return "";
};

// Everything the tests change kept, once (both tests); put back at the end (the tunables by `tune`).
void(float restore) VR_AGPP_KeepAll =
{
    float i;
    for(i = 0; i < VR_AGPP_KEEPS; i++)
    {
        string n = VR_AGPP_KeepName(i);
        if(!restore)
        {
            vr_agpp_keep[i] = cvar(n);
        }
        else
        {
            localcmd(sprintf("%s%s %g\n", i < VR_AGPP_KEEP_CVARS ? "tune " : "", n, vr_agpp_keep[i]));
        }
    }
};


// Your hands empty, open (the main one closed for 96), each frame.
void(entity p, float closed) VR_AGPP_Hands =
{
    VR_AGPT_EmptyHand(p);
    p.weapon2 = WID_FIST;
    p.vrbits0 = p.vrbits0 - (p.vrbits0 & (QVR_VRBITS0_MAINHAND_GRABBING | QVR_VRBITS0_OFFHAND_GRABBING));
    if(closed)
    {
        p.vrbits0 = p.vrbits0 | QVR_VRBITS0_MAINHAND_GRABBING;
    }
};

// Your main hand placed in `pose` (0 at rest ahead of your chest, 1 overhead, 2 cocked out to the side), against your
// head and the way to `g`; the off hand at rest.
void(entity p, entity g, float pose) VR_AGPP_Place =
{
    float mu = VRMetersToUnits();
    vector head = VRGetHeadPos(p);
    vector fw = g.origin - head;
    fw_z = 0;
    fw = normalize(fw);
    vector rt = VR_Cross(fw, '0 0 1');
    vector rest = head + fw * 0.3 * mu - '0 0 1' * 0.5 * mu;
    vector at = rest;
    if(pose == 1) { at = head + '0 0 1' * 0.15 * mu + rt * 0.1 * mu; }
    if(pose == 2) { at = head + rt * 0.55 * mu - '0 0 1' * 0.2 * mu; }
    p.handpos = at;
    p.offhandpos = rest - rt * 0.2 * mu;
    p.handvel = '0 0 0';
    p.offhandvel = '0 0 0';
};

// The case's tunables and cvars (a console line run before its grunt is held).
string(float t, float c) VR_AGPP_Setup =
{
    string s = "tune vr_agrunt_pcat_fist_chance 1; tune vr_agrunt_pcat_fist_delay 1; tune vr_agrunt_pcat_fist_guard 1; "
               "tune vr_agrunt_pcat_fist_range 1; tune vr_agrunt_pcat_fist_windup 1; tune vr_agrunt_pcat_fist_pre 1; "
               "tune vr_agrunt_strike_beat 0; vr_agrunt_parry 1; vr_agrunt_react_parry 0\n";
    if(t == 95)
    {
        s = strcat(s, "vr_agrunt_preparry 1; vr_agrunt_windup_parry 0; tune vr_agrunt_preparry_interval 0.1\n");
        s = strcat(s, sprintf("tune vr_agrunt_preparry_chance %g\n", c == 5 ? 0 : 1));
        if(c == 7)
        {
            s = strcat(s, "tune vr_agrunt_preparry_delay_min 0.1; tune vr_agrunt_preparry_delay_max 0.4; "
                          "tune vr_agrunt_preparry_guard_min 0.3; tune vr_agrunt_preparry_guard_max 0.6; "
                          "tune vr_agrunt_preparry_recover_min 0.2; tune vr_agrunt_preparry_recover_max 0.5\n");
        }
        else
        {
            s = strcat(s, "tune vr_agrunt_preparry_delay_min 0.2; tune vr_agrunt_preparry_delay_max 0.2; "
                          "tune vr_agrunt_preparry_guard_min 0.8; tune vr_agrunt_preparry_guard_max 0.8; "
                          "tune vr_agrunt_preparry_recover_min 0.6; tune vr_agrunt_preparry_recover_max 0.6\n");
        }
        return s;
    }
    s = strcat(s, "vr_agrunt_preparry 0; vr_agrunt_windup_parry 1; tune vr_agrunt_windup_read 0.08; tune vr_agrunt_windup_rearm 1\n");
    if(c == 5)
    {
        return strcat(s, "tune vr_agrunt_windup_chance 0.5; tune vr_agrunt_preparry_chance 0.5; "
                         "tune vr_agrunt_windup_delay_min 0.1; tune vr_agrunt_windup_delay_max 0.3; "
                         "tune vr_agrunt_preparry_delay_min 0.1; tune vr_agrunt_preparry_delay_max 0.3\n");
    }
    s = strcat(s, sprintf("tune vr_agrunt_windup_chance %g\n", c == 4 ? 0 : 1));
    return strcat(s, "tune vr_agrunt_windup_delay_min 0.15; tune vr_agrunt_windup_delay_max 0.15; "
                     "tune vr_agrunt_windup_guard_min 0.5; tune vr_agrunt_windup_guard_max 0.5; "
                     "tune vr_agrunt_windup_recover_min 0.6; tune vr_agrunt_windup_recover_max 0.6\n");
};

float(float t) VR_AGPP_Cases = { return t == 95 ? 8 : 6; };

string(float t, float c) VR_AGPP_Name =
{
    if(t == 95)
    {
        if(c == 0) { return "95.0 shove ready: no guard"; }
        if(c == 1) { return "95.1 shove cooling: guard 0.2 s after the roll, 0.8 s; strike in it parried, in the recovery disarms"; }
        if(c == 2) { return "95.2 recovery 0.6 s, then the guard again"; }
        if(c == 3) { return "95.3 punch through its barrel: in the guard parried, in the recovery disarmed in full"; }
        if(c == 4) { return "95.4 176 units off: no guard"; }
        if(c == 5) { return "95.5 chance 0: never"; }
        if(c == 6) { return "95.6 a sword grunt, shove ready: guards"; }
        return "95.7 random times within their ranges";
    }
    if(c == 0) { return "96.0 hand at rest: no guard"; }
    if(c == 1) { return "96.1 raised overhead: guard after read + delay, 0.5 s; bait, strike in the recovery disarms"; }
    if(c == 2) { return "96.2 held overhead: one guard; lowered and raised again: a second"; }
    if(c == 3) { return "96.3 cocked to the side: guards"; }
    if(c == 4) { return "96.4 chance 0: one roll, no guard"; }
    return "96.5 an axe raised: parried more often and sooner than a shotgun";
};

// Its guard's transitions this frame (seen a frame late: its weapon thinks after this; both ends alike).
void(entity g) VR_AGPP_Track =
{
    float s = g.vr_agg_state;
    if(s == vr_agpp_prev)
    {
        return;
    }
    if(s == 1) { vr_agpp_t1 = time; }
    if(s == 2) { vr_agpp_tR = time; vr_agpp_raises = vr_agpp_raises + 1; }
    if(s == 3) { vr_agpp_tD = time; }
    if(vr_agpp_prev == 3) { vr_agpp_tE = time; } // (its recovery over: idle, or a roll won at once)
    vr_agpp_prev = s;
};

// `x` within lo .. hi, a frame and a half's leeway.
float(float x, float lo, float hi) VR_AGPP_In = { return x >= lo - VR_AGPP_FRAME * 1.5 && x <= hi + VR_AGPP_FRAME * 1.5; };

// One check of the case's (kept: all must pass).
void(float ok, string what) VR_AGPP_Note =
{
    vr_agpp_ok = (vr_agpp_ok && ok);
    VR_AGT_Print(sprintf("  guard %g.%g: %s%s", cvar_hget(cvarh_vr_agrunt_test), vr_agpp_case, (ok ? "" : "[x] "), what));
};

// A closed fist on its barrel now (as test 94's): VR_AG_WeaponStruck run on it.
void(entity g) VR_AGPP_Strike =
{
    entity w = g.vr_ag_prop;
    VR_AG_WeaponLine(w);
    vector along = normalize(vr_ag_wb - vr_ag_wa);
    vector side = normalize(VR_Cross(along, '0 0 1'));
    vector bar = vr_ag_wa + (vr_ag_wb - vr_ag_wa) * 0.75;
    float step = 6 * VRMetersToUnits() / 72;
    VR_AGST_Blow(bar + side * step * 0.5, '0 0 0', bar - side * step * 0.5, '0 0 0', 1, side * -1, 6, TRUE, VR_MTHING_FIST);
    VR_AG_WeaponStruck(g, w);
};

// The melee's punch through its barrel into its chest (test 94's case 13): its damage.
float(entity g) VR_AGPP_Punch =
{
    entity w = g.vr_ag_prop;
    VR_AG_WeaponLine(w);
    vector along = normalize(vr_ag_wb - vr_ag_wa);
    vector side = normalize(VR_Cross(along, '0 0 1'));
    vector bar = vr_ag_wa + (vr_ag_wb - vr_ag_wa) * 0.75;
    float step = 6 * VRMetersToUnits() / 72;
    vector a = bar + side * step * 0.5;
    vector b = bar - side * step * 0.5;
    VR_AGST_Blow(a, '0 0 0', b, '0 0 0', 1, normalize(b - a), 6, TRUE, VR_MTHING_FIST);
    return VR_AGST_Melee(g, VR_AGPT2_At(g, VR_AGPar_Chest(g)), normalize(b - a));
};

// 96.5's bare rolls: `wid` in your main hand, `n` rolls of kind `kind`: the share won (vr_agpp_d_lo/hi: the delays'
// range; returns the share, the mean delay in vr_agg_last_guard's stead: vr_agpp_tE).
float(entity g, float wid, float kind, float n) VR_AGPP_Rolls =
{
    entity p = vr_agt_player;
    p.weapon = wid;
    p.weaponinst = world;
    float won = 0;
    float sum = 0;
    vr_agpp_d_lo = 100;
    vr_agpp_d_hi = 0;
    float i;
    for(i = 0; i < n; i++)
    {
        g.vr_agg_state = 0;
        if(VR_AGG_Roll(g, p, cVR_MainHand, kind))
        {
            won = won + 1;
            sum = sum + vr_agg_last_delay;
            vr_agpp_d_lo = min(vr_agpp_d_lo, vr_agg_last_delay);
            vr_agpp_d_hi = max(vr_agpp_d_hi, vr_agg_last_delay);
        }
    }
    g.vr_agg_state = 0;
    p.weapon = WID_FIST;
    vr_agpp_tE = won > 0 ? sum / won : 0;
    return won / n;
};

void(entity g, float c) VR_AGPP_Case95;
void(entity g, float c) VR_AGPP_Case96;

void(float t) VR_AGPP_Frame =
{
    entity p = vr_agt_player;
    entity g = vr_agt_grunt;
    if(vr_agt_step == 0)
    {
        if(vr_agpp_case == 0)
        {
            VR_AGPP_KeepAll(FALSE);
            p.flags = p.flags | FL_NOTARGET;
        }
        if(vr_agpp_case >= VR_AGPP_Cases(t))
        {
            VR_AGPP_KeepAll(TRUE);
            VR_AGPT_Clear();
            p.flags = p.flags - (p.flags & FL_NOTARGET);
            p.vrbits0 = p.vrbits0 - (p.vrbits0 & QVR_VRBITS0_MAINHAND_GRABBING);
            vr_agpp_case = 0;
            cvar_set("vr_agrunt_test", "0");
            return;
        }
        VR_AGPT_Clear();
        VR_AGPP_Hands(p, t == 96);
        localcmd(VR_AGPP_Setup(t, vr_agpp_case));
        float wid = WID_SHOTGUN;
        if(t == 95 && vr_agpp_case == 6) { wid = WID_SWORD; }
        vr_agt_grunt = VR_AGT_Spawn(wid, (t == 95 && vr_agpp_case == 4) ? 200 : 60);
        vr_agpp_until = time + 5;
        vr_agpp_ok = TRUE;
        vr_agt_step = 1;
        return;
    }
    if(vr_agt_step == 1)
    {
        // Its start done (it takes damage), its weapon placed, the case's tunables in (the fists' chance a last one).
        VR_AGPP_Hands(p, t == 96);
        if(t == 96) { VR_AGPP_Place(p, g, 0); }
        float tuned = cvar_hget(cvarh_vr_agrunt_pcat_fist_pre) == 1 && cvar_hget(cvarh_vr_agrunt_react_parry) == 0 &&
                      (t == 95 ? cvar_hget(cvarh_vr_agrunt_preparry_interval) == 0.1 : cvar_hget(cvarh_vr_agrunt_windup_rearm) == 1);
        if(!(g.takedamage && g.vr_ag_prop && g.vr_ag_prop.vr_ag_was_n >= 1 && tuned))
        {
            if(time > vr_agpp_until)
            {
                VR_AGT_Check(sprintf("guard %s: timed out (takedamage %g, weapon %g, tuned %g)", VR_AGPP_Name(t, vr_agpp_case),
                    g.takedamage, g.vr_ag_prop != world, tuned), FALSE);
                vr_agt_step = 3;
            }
            return;
        }
        VR_AGST_Hold(g, 1); // (aware of you, its parry down)
        g.vr_agg_state = 0;
        g.vr_agg_next = 0;
        g.vr_agg_wup0 = 0;
        g.vr_agg_wup1 = 0;
        g.vr_agg_wread0 = 0;
        g.vr_agg_wread1 = 0;
        g.vr_eshove_next = (t == 95 && (vr_agpp_case == 0 || vr_agpp_case == 6)) ? 0 : (time + 1000);
        vr_agpp_t0 = time;
        vr_agpp_prev = 0;
        vr_agpp_t1 = 0;
        vr_agpp_tR = 0;
        vr_agpp_tD = 0;
        vr_agpp_tE = 0;
        vr_agpp_raises = 0;
        vr_agpp_rolls0 = vr_agg_rolls;
        vr_agpp_sub = 0;
        vr_agpp_tRaise = 0;
        vr_agpp_pose = 0;
        vr_agpp_n0 = vr_agpar_n;
        vr_agpp_d_lo = 100;
        vr_agpp_d_hi = 0;
        vr_agpp_until = time + 10;
        vr_agt_step = 2;
        return;
    }
    if(vr_agt_step == 2)
    {
        VR_AGPP_Hands(p, t == 96);
        if(g.vr_ag_prop)
        {
            g.health = 1000;
            g.pain_finished = 0;
        }
        VR_AGPP_Track(g);
        if(time > vr_agpp_until)
        {
            VR_AGPP_Note(FALSE, sprintf("timed out at its step %g (state %g)", vr_agpp_sub, g.vr_agg_state));
            vr_agt_step = 3;
        }
        else if(t == 95)
        {
            VR_AGPP_Case95(g, vr_agpp_case);
        }
        else
        {
            VR_AGPP_Case96(g, vr_agpp_case);
        }
        if(vr_agt_step == 3)
        {
            VR_AGT_Check(sprintf("guard %s", VR_AGPP_Name(t, vr_agpp_case)), vr_agpp_ok);
        }
        return;
    }
    // (step 3) the next case
    VR_AGT_Remove(g);
    vr_agt_grunt = world;
    vr_agpp_case = vr_agpp_case + 1;
    vr_agt_step = 0;
};

// Test 95's cases, each frame (vr_agt_step 3: done).
void(entity g, float c) VR_AGPP_Case95 =
{
    float el = time - vr_agpp_t0;
    float rolls = vr_agg_rolls - vr_agpp_rolls0;
    if(c == 0 || c == 4 || c == 5)
    {
        if(el < 1)
        {
            return;
        }
        VR_AGPP_Note(vr_agpp_raises == 0 && vr_agpp_t1 == 0, sprintf("guards %g", vr_agpp_raises));
        VR_AGPP_Note(c == 5 ? rolls >= 8 : rolls == 0, sprintf("rolls %g in 1 s", rolls));
        vr_agt_step = 3;
        return;
    }
    if(c == 6)
    {
        if(vr_agpp_raises > 0)
        {
            VR_AGPP_Note(TRUE, sprintf("raised %.2f s in", vr_agpp_tR - vr_agpp_t0));
            vr_agt_step = 3;
        }
        else if(el > 1)
        {
            VR_AGPP_Note(FALSE, "no guard in 1 s");
            vr_agt_step = 3;
        }
        return;
    }
    if(c == 7)
    {
        // 4 guards: each delay, guard, recovery within its range.
        if(vr_agpp_sub < 4)
        {
            if(g.vr_agg_state == 2 && vr_agpp_raises == vr_agpp_sub + 1 && vr_agpp_tR == time)
            {
                float d = vr_agpp_tR - vr_agpp_t1;
                vr_agpp_d_lo = min(vr_agpp_d_lo, d);
                vr_agpp_d_hi = max(vr_agpp_d_hi, d);
                VR_AGPP_Note(VR_AGPP_In(d, 0.1, 0.4), sprintf("delay %.3f", d));
            }
            if(g.vr_agg_state == 3 && vr_agpp_tD == time)
            {
                float gd = vr_agpp_tD - vr_agpp_tR;
                VR_AGPP_Note(VR_AGPP_In(gd, 0.3, 0.6), sprintf("guard %.3f", gd));
            }
            if(g.vr_agg_state <= 1 && vr_agpp_tE == time && vr_agpp_tD > 0)
            {
                float rd = vr_agpp_tE - vr_agpp_tD;
                VR_AGPP_Note(VR_AGPP_In(rd, 0.2, 0.5), sprintf("recovery %.3f", rd));
                vr_agpp_sub = vr_agpp_sub + 1;
            }
            return;
        }
        VR_AGPP_Note(vr_agpp_d_hi - vr_agpp_d_lo > VR_AGPP_FRAME * 1.5, sprintf("delays %.3f-%.3f (not all one)",
            vr_agpp_d_lo, vr_agpp_d_hi));
        vr_agt_step = 3;
        return;
    }
    // 1, 2, 3: the guard's times, then the case's blows.
    if(vr_agpp_sub == 0)
    {
        if(g.vr_agg_state == 2 && g.vr_agpar_w >= 0.9)
        {
            float dl = vr_agpp_tR - vr_agpp_t1;
            VR_AGPP_Note(VR_AGPP_In(dl, 0.2, 0.2), sprintf("raised %.3f s after the roll", dl));
            if(c == 1)
            {
                string lab = VR_AGG_Label(g);
                VR_AGPP_Note(substring(lab, 0, 8) == "preparry", sprintf("label '%s'", lab));
                float n0 = vr_agpar_n;
                VR_AGPP_Strike(g);
                VR_AGPP_Note(vr_agpar_n > n0 && g.vr_ag_prop != world, sprintf("a strike in it: parried %g, %s (%s)",
                    vr_agpar_n - n0, g.vr_ag_prop ? "armed" : "disarmed", vr_ag_strike_why));
            }
            if(c == 3)
            {
                float dmg = VR_AGPP_Punch(g);
                float kept = 20 * cvar_hget(cvarh_vr_agrunt_parry_damage);
                VR_AGPP_Note(fabs(dmg - kept) < 0.01 && g.vr_ag_prop != world, sprintf("a punch in it: %g damage (parried %g), %s",
                    dmg, kept, g.vr_ag_prop ? "armed" : "disarmed"));
            }
            vr_agpp_sub = 1;
        }
        return;
    }
    if(vr_agpp_sub == 1)
    {
        if(g.vr_agg_state == 3 && vr_agpp_tD > 0)
        {
            float gt = vr_agpp_tD - vr_agpp_tR;
            VR_AGPP_Note(VR_AGPP_In(gt, 0.8, 0.8), sprintf("guard %.3f s", gt));
            if(c == 1)
            {
                VR_AGPP_Strike(g);
                VR_AGPP_Note(!g.vr_ag_prop, sprintf("a strike in the recovery: %s (%s)", g.vr_ag_prop ? "armed" : "disarmed",
                    vr_ag_strike_why));
                vr_agt_step = 3;
            }
            else if(c == 3)
            {
                float dmg2 = VR_AGPP_Punch(g);
                float kept2 = 20 * cvar_hget(cvarh_vr_agrunt_parry_damage);
                VR_AGPP_Note(dmg2 > kept2 + 0.01 && !g.vr_ag_prop, sprintf("a punch in the recovery: %g damage, %s",
                    dmg2, g.vr_ag_prop ? "armed" : "disarmed"));
                vr_agt_step = 3;
            }
            else
            {
                VR_AGPP_Note(!VR_AGPar_Able(g, vr_agt_player), sprintf("can't parry: %s", vr_agpar_why));
                vr_agpp_sub = 2;
            }
        }
        return;
    }
    // (2) its recovery, then the guard again
    if(vr_agpp_raises >= 2)
    {
        float rt = vr_agpp_tE - vr_agpp_tD;
        VR_AGPP_Note(VR_AGPP_In(rt, 0.6, 0.6), sprintf("recovery %.3f s", rt));
        float again = vr_agpp_tR - vr_agpp_tE;
        VR_AGPP_Note(again <= 0.1 + 0.2 + VR_AGPP_FRAME * 3, sprintf("raised again %.3f s after", again));
        vr_agt_step = 3;
    }
};

// Test 96's cases, each frame.
void(entity g, float c) VR_AGPP_Case96 =
{
    entity p = vr_agt_player;
    float el = time - vr_agpp_t0;
    float rolls = vr_agg_rolls - vr_agpp_rolls0;
    if(c == 5)
    {
        // Bare rolls: an axe against a shotgun, the wind-up's and the preventive's.
        float sk = VR_AGPar_SkillReact();
        float mw = VR_AGPP_Rolls(g, WID_AXE, 2, 400);
        float md = vr_agpp_tE;
        float lo = vr_agpp_d_lo;
        float hi = vr_agpp_d_hi;
        float mr = 0.1 * VR_AGG_Mult(0, 1) * sk;
        float mx = 0.3 * VR_AGG_Mult(0, 1) * sk;
        VR_AGPP_Note(lo >= mr - 0.0001 && hi <= mx + 0.0001, sprintf("axe delays %.3f-%.3f in %.3f-%.3f", lo, hi, mr, mx));
        float gw = VR_AGPP_Rolls(g, WID_SHOTGUN, 2, 400);
        float gdl = vr_agpp_tE;
        mr = 0.1 * VR_AGG_Mult(1, 1) * sk;
        mx = 0.3 * VR_AGG_Mult(1, 1) * sk;
        VR_AGPP_Note(vr_agpp_d_lo >= mr - 0.0001 && vr_agpp_d_hi <= mx + 0.0001, sprintf("shotgun delays %.3f-%.3f in %.3f-%.3f",
            vr_agpp_d_lo, vr_agpp_d_hi, mr, mx));
        VR_AGPP_Note(mw > gw + 0.1, sprintf("wind-up parried: axe %.2f, shotgun %.2f", mw, gw));
        VR_AGPP_Note(md < gdl, sprintf("mean delay: axe %.3f s, shotgun %.3f s", md, gdl));
        float pm = VR_AGPP_Rolls(g, WID_AXE, 1, 400);
        float pg = VR_AGPP_Rolls(g, WID_SHOTGUN, 1, 400);
        VR_AGPP_Note(pm > pg + 0.1, sprintf("preventive: axe %.2f, shotgun %.2f", pm, pg));
        vr_agt_step = 3;
        return;
    }
    float pose = 0;
    if(c == 1 || c == 2 || c == 4) { pose = 1; }
    if(c == 3) { pose = 2; }
    if(c == 1 && vr_agpp_sub >= 1) { pose = 0; } // (lowered once its guard dropped)
    if(c == 2 && vr_agpp_sub == 1) { pose = 0; } // (lowered for 0.2 s)
    if(pose != vr_agpp_pose && pose > 0)
    {
        vr_agpp_tRaise = time;
    }
    vr_agpp_pose = pose;
    VR_AGPP_Place(p, g, pose);
    if(c == 0 || c == 4)
    {
        if(el < 1)
        {
            return;
        }
        VR_AGPP_Note(vr_agpp_raises == 0, sprintf("guards %g", vr_agpp_raises));
        VR_AGPP_Note(c == 4 ? rolls == 1 : rolls == 0, sprintf("rolls %g in 1 s", rolls));
        vr_agt_step = 3;
        return;
    }
    if(c == 3)
    {
        if(vr_agpp_raises > 0)
        {
            VR_AGPP_Note(TRUE, sprintf("raised %.3f s after the raise", vr_agpp_tR - vr_agpp_tRaise));
            vr_agt_step = 3;
        }
        else if(el > 1)
        {
            VR_AGPP_Note(FALSE, sprintf("no guard in 1 s (rolls %g)", rolls));
            vr_agt_step = 3;
        }
        return;
    }
    if(c == 1)
    {
        if(vr_agpp_sub == 0 && g.vr_agg_state == 2 && vr_agpp_tR == time)
        {
            float dl = vr_agpp_tR - vr_agpp_tRaise;
            VR_AGPP_Note(VR_AGPP_In(dl, 0.08 + 0.15, 0.08 + 0.15 + VR_AGPP_FRAME), sprintf("raised %.3f s after the raise", dl));
            string lab = VR_AGG_Label(g);
            VR_AGPP_Note(substring(lab, 0, 6) == "windup", sprintf("label '%s'", lab));
        }
        if(vr_agpp_sub == 0 && g.vr_agg_state == 3 && vr_agpp_tD > 0)
        {
            float gt = vr_agpp_tD - vr_agpp_tR;
            VR_AGPP_Note(VR_AGPP_In(gt, 0.5, 0.5), sprintf("guard %.3f s", gt));
            string lab2 = VR_AGG_Label(g);
            VR_AGPP_Note(substring(lab2, 0, 10) == "recovering", sprintf("label '%s'", lab2));
            vr_agpp_sub = 1; // (lowered from the next frame)
            vr_agpp_tRaise = time;
            return;
        }
        if(vr_agpp_sub == 1 && time - vr_agpp_tRaise >= 0.1)
        {
            VR_AGPP_Strike(g);
            VR_AGPP_Note(!g.vr_ag_prop, sprintf("a strike in the recovery: %s (%s)", g.vr_ag_prop ? "armed" : "disarmed",
                vr_ag_strike_why));
            vr_agt_step = 3;
        }
        return;
    }
    // (2) held: one guard in 2 s; lowered 0.2 s, raised: a second
    if(vr_agpp_sub == 0 && el >= 2)
    {
        VR_AGPP_Note(vr_agpp_raises == 1 && rolls == 1, sprintf("held 2 s: guards %g, rolls %g", vr_agpp_raises, rolls));
        vr_agpp_sub = 1;
        vr_agpp_tD = time;
        return;
    }
    if(vr_agpp_sub == 1 && time - vr_agpp_tD >= 0.2)
    {
        vr_agpp_sub = 2;
        vr_agpp_tD = time;
        return;
    }
    if(vr_agpp_sub == 2)
    {
        if(vr_agpp_raises >= 2)
        {
            VR_AGPP_Note(TRUE, sprintf("raised again: %.3f s after", time - vr_agpp_tD));
            vr_agt_step = 3;
        }
        else if(time - vr_agpp_tD > 1)
        {
            VR_AGPP_Note(FALSE, "not raised again in 1 s");
            vr_agt_step = 3;
        }
    }
};
#endif
