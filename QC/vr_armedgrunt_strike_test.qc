#ifdef QVR_TESTS
//
//
//
// ----------------------------------------------------------------------------
// vr_armedgrunt_strike_test.qc -- `vr_agrunt_test 94` (Debug > Tests > Armed Grunts: Strikes on Its Weapon Disarm): a
// deliberate strike on an armed grunt's weapon or gun hand disarms it unless its parry was raised for it
// (vr_armedgrunt_parry.qc VR_AG_BlowOnWeapon; vr_agrunt_strike_first). In vrtesthall, you unseen, a fresh shotgun grunt
// 60 units ahead for each case, held still, its parry state set (unaware; aware but caught out; aware, raised); your
// hand mocked as the melee tracker has it (its swept points this frame, its real-time swing, the swing the frame before,
// the fist closed), then its weapon's think (VR_AG_WeaponStruck) or the melee's damage (T_Damage_VRMelee) or a throw:
//   fist on its barrel 6 m/s: unaware, caught out: disarmed; caught out with vr_agrunt_strike_first 0: parried (the old
//   order); raised, vr_agrunt_strike_beat 0: parried; 1: disarmed; on its forearm (off its weapon): disarmed;
//   an open hand reaching, a 2 m/s push: neither; a held shotgun's far end bashed through its barrel (your hand 20 units
//   off it): disarmed; a carried 24-unit crate swung into it, your fist 13 units off: disarmed (the fist alone: not met);
//   a gun thrown across its barrel: disarmed; one bounced off its body by its gun hand (5 damage, under the disarming
//   blow): disarmed; the melee's punch on its legs: its full damage, armed; through its barrel into its chest, caught
//   out: disarmed, the blow's full damage; into a raised parry, beat 0: parried (cut); the caught-out fist again in slow
//   motion (vr_timescale 0.3): disarmed.
// Each case `agtest: PASS|FAIL strike <case>: ...`; ends on `agtest: test 94 over: P passed, F failed`.
// ----------------------------------------------------------------------------

float VR_AGST_CASES = 16;
float vr_agst_case;       // the case running
float vr_agst_until;      // its set-up's give-up time (a safety net: FAIL "timed out")
float vr_agst_keep_beat;  // cvars before (put back at the end)
float vr_agst_keep_first;
float vr_agst_keep_parry;
float vr_agst_keep_scale;
float vr_agst_keep_armor; // vr_agarmor_rule (pinned 0: a plate under rule 1 stops case 13's whole blow)
entity vr_agst_crate;

// The case's name.
string(float c) VR_AGST_Name =
{
    if(c == 0) { return "fist on its barrel, unaware"; }
    if(c == 1) { return "fist on its barrel, caught out"; }
    if(c == 2) { return "fist on its barrel, caught out, old order (vr_agrunt_strike_first 0)"; }
    if(c == 3) { return "fist into its raised parry, beat 0"; }
    if(c == 4) { return "fist into its raised parry, beat 1"; }
    if(c == 5) { return "fist on its forearm, caught out"; }
    if(c == 6) { return "open hand reaching for its barrel, caught out"; }
    if(c == 7) { return "a 2 m/s push on its barrel, unaware"; }
    if(c == 8) { return "a held shotgun bashed through its barrel, caught out"; }
    if(c == 9) { return "a carried crate swung into its barrel, caught out"; }
    if(c == 10) { return "a gun thrown across its barrel"; }
    if(c == 11) { return "a gun thrown at its gun hand, bounced off its body"; }
    if(c == 12) { return "the melee's punch on its legs, caught out"; }
    if(c == 13) { return "the melee's punch through its barrel into its chest, caught out"; }
    if(c == 14) { return "the melee's punch through its barrel into its raised parry, beat 0"; }
    return "fist on its barrel, caught out, slow motion (vr_timescale 0.3)";
};

// Its parry: 0 unaware of you, 1 aware but caught out, 2 aware and raised; held still, healthy, not in pain.
void(entity g, float state) VR_AGST_Hold =
{
    g.think = SUB_Null;
    g.nextthink = -1;
    g.health = 1000;
    g.pain_finished = 0;
    g.vr_ag_act = 0;
    g.enemy = state > 0 ? vr_agt_player : world;
    g.vr_agpar_ready = state == 2 ? time - 0.2 : -1;
    g.vr_agpar_w = state == 2 ? 1 : 0;
    g.vr_agpar_seen = time;
    g.vr_agpar_tby = vr_agt_player;
};

// Your main hand's blow this frame as the melee tracker has it: its points from `a0`..`a1` (its last pose) to `b0`..`b1`
// (one point: a fist), at `sp` m/s in real time along `dir`, swung the frame before too; `closed` the fist closed.
void(vector a0, vector a1, vector b0, vector b1, float n, vector dir, float sp, float closed, float thing) VR_AGST_Blow =
{
    entity pl = vr_agt_player;
    entity was = self;
    self = pl;
    entity b = VR_Blow(cVR_MainHand);
    self = was;
    b.mh_thing = thing;
    b.mh_time = time;
    b.mh_fast = sp;
    b.mh_fresh = TRUE;
    b.mh_dt = 1 / 72;
    b.mh_lenient = 1;
    b.mh_vfar_last = dir * sp;
    b.mh_vgrip_last = dir * sp;
    b.mh_n = n;
    b.mh_pp[0] = a0;
    b.mh_pp[1] = a1;
    b.mh_cp[0] = b0;
    b.mh_cp[1] = b1;
    pl.handpos = b0;
    // (the hand's world velocity: the game's, in slow motion 1 over its scale faster: vr_hand_time_scale)
    pl.handvel = dir * sp * VRMetersToUnits() / (vr_hand_time_scale > 0 ? vr_hand_time_scale : 1);
    pl.velocity = '0 0 0';
    pl.vrbits0 = pl.vrbits0 - (pl.vrbits0 & QVR_VRBITS0_MAINHAND_GRABBING);
    if(closed)
    {
        pl.vrbits0 = pl.vrbits0 | QVR_VRBITS0_MAINHAND_GRABBING;
    }
    pl.vr_ag_knock_at = 0;
};

// The melee's blow of 20 by your main hand meeting its body at `at` (T_Damage_VRMelee, as test 60's): the damage dealt;
// vr_agpt2_expect what it would be unparried.
float(entity g, vector at, vector dir) VR_AGST_Melee =
{
    entity p = vr_agt_player;
    float mult = VR_Melee_Positional(g, at, dir);
    vr_agpt2_expect = VR_Melee_CapDamage(20 * mult, mult);
    float h0 = g.health;
    entity was = self;
    self = p;
    VR_SetHitKind(QVR_HITKIND_MELEE, cVR_MainHand, at, dir);
    T_Damage_VRMelee(g, p, p, 20);
    VR_ClearHitKind();
    self = was;
    return h0 - g.health;
};

void() VR_AGST_End =
{
    entity p = vr_agt_player;
    localcmd(sprintf("tune vr_agrunt_strike_beat %g\n", vr_agst_keep_beat)); // (a tunable: set by `tune`)
    cvar_set("vr_agrunt_strike_first", ftos(vr_agst_keep_first));
    cvar_set("vr_agrunt_parry", ftos(vr_agst_keep_parry));
    cvar_set("vr_timescale", ftos(vr_agst_keep_scale));
    cvar_set("vr_agarmor_rule", ftos(vr_agst_keep_armor));
    VR_AGPT_Clear();
    p.flags = p.flags - (p.flags & FL_NOTARGET);
    p.vrbits0 = p.vrbits0 - (p.vrbits0 & QVR_VRBITS0_MAINHAND_GRABBING);
    vr_agst_case = 0;
    cvar_set("vr_agrunt_test", "0");
};

// The case's strike, its weapon's line laid out (vr_ag_wa..wb); its result checked.
void(entity g, float c) VR_AGST_Strike =
{
    entity p = vr_agt_player;
    entity w = g.vr_ag_prop;
    VR_AG_WeaponLine(w);
    vector wa = vr_ag_wa;
    vector wb = vr_ag_wb;
    vector along = normalize(wb - wa);
    vector side = normalize(VR_Cross(along, '0 0 1'));
    vector up = normalize(VR_Cross(side, along));
    vector bar = wa + (wb - wa) * 0.75; // (on its barrel, away from its hand)
    float step = 6 * VRMetersToUnits() / 72; // (a 6 m/s blow's way in a frame: 3.3 units)
    float n0 = vr_agpar_n;
    float d0 = vr_ag_strike_dis;
    float fb = vr_ag_feedbacks;
    float sp = 6;
    float state = 1;
    if(c == 0 || c == 7 || c == 10 || c == 11) { state = 0; }
    if(c == 3 || c == 4 || c == 14) { state = 2; }
    VR_AGST_Hold(g, state);
    cvar_set("vr_agrunt_strike_first", c == 2 ? "0" : "1");
    float expectDis = TRUE;
    float expectParry = FALSE;
    float dmg = -1;
    string extra = "";
    float okExtra = TRUE;
    vr_ag_strike_where = 0;
    p.weapon = WID_FIST;
    if((c <= 7) || (c == 15))
    {
        vector at = bar;
        if(c == 5)
        {
            // (its forearm, towards its elbow from its grip)
            VR_AGS_HandLine(g, w);
            at = vr_ags_hb;
            extra = sprintf(", %.1f units off its weapon's line", VR_AG_SegDist(at, at, wa, wb));
        }
        if(c == 7) { sp = 2; step = 2 * VRMetersToUnits() / 72; }
        VR_AGST_Blow(at + side * step * 0.5, '0 0 0', at - side * step * 0.5, '0 0 0', 1, side * -1, sp, c != 6, VR_MTHING_FIST);
        expectDis = (c != 2 && c != 3 && c != 6 && c != 7);
        expectParry = (c == 2 || c == 3);
        VR_AG_WeaponStruck(g, w);
    }
    else if(c == 8)
    {
        // A held shotgun: your hand 22 units to its side, the gun's line across to 2 units past its barrel, swung down.
        p.weapon = WID_SHOTGUN;
        vector g0 = bar + side * 22;
        vector g1 = bar - side * 2;
        VR_AGST_Blow(g0 + up * (step + 1.5), g1 + up * (step + 1.5), g0 + up * 1.5, g1 + up * 1.5, 2, up * -1, sp, TRUE, VR_MTHING_GUN);
        VR_AG_WeaponStruck(g, w);
        p.weapon = WID_FIST;
    }
    else if(c == 9)
    {
        // A carried crate (24 units) on your fist, the fist 13 units above its barrel, swung across: the crate meets it.
        vector f = bar + up * 13;
        VR_AGST_Blow(f + side * step * 0.5, '0 0 0', f - side * step * 0.5, '0 0 0', 1, side * -1, sp, TRUE, VR_MTHING_FIST);
        float alone = VR_AG_StrikeMeets(g, w, p, cVR_MainHand);
        vr_agst_crate = spawn();
        vr_agst_crate.classname = "vr_agst_crate";
        vr_agst_crate.flags = FL_FORCEGRABBABLE;
        setsize(vr_agst_crate, '-12 -12 -12', '12 12 12');
        setorigin(vr_agst_crate, bar - side * step * 0.5 + up * 1);
        VR_Hand_Take(p, cVR_MainHand, vr_agst_crate);
        extra = sprintf(", the fist alone met it: %s", alone >= 0 ? "yes" : "no");
        VR_AG_WeaponStruck(g, w);
        VR_Hand_Release(p, cVR_MainHand, "test");
        remove(vr_agst_crate);
        vr_agst_crate = world;
        expectDis = alone < 0;
    }
    else if(c == 10)
    {
        // A nailgun thrown at 14 m/s across its barrel, 4 units short of it now.
        entity t = VR_AGPT_Weapon(WID_NAILGUN, bar + side * 4, FALSE);
        setorigin(t, bar + side * 4);
        t.velocity = side * -14 * VRMetersToUnits();
        t.movetype = MOVETYPE_FLY;
        VR_AG_WeaponStruck(g, w);
        extra = sprintf(", %.1f kg", VR_Thrown_Mass(t));
    }
    else if(c == 11)
    {
        // A nailgun thrown at its gun hand, touching its body by its forearm and bounced back at 14 m/s: 5 damage.
        VR_AGS_HandLine(g, w);
        vector toBody = (g.absmin + g.absmax) * 0.5 - vr_ags_ha;
        toBody_z = 0;
        vector c11 = vr_ags_ha + normalize(vr_ags_hb - vr_ags_ha) * 6 - normalize(toBody) * 6;
        entity t2 = VR_AGPT_Weapon(WID_NAILGUN, c11, FALSE);
        setorigin(t2, c11);
        t2.velocity = normalize(toBody) * -14 * VRMetersToUnits();
        t2.movetype = MOVETYPE_FLY;
        t2.owner = p;
        float h0 = g.health;
        entity was = self;
        self = t2;
        PositionalDamage(g, t2.origin, normalize(toBody));
        T_Damage(g, t2, p, 5);
        self = was;
        extra = sprintf(", %g damage, %.1f kg", h0 - g.health, VR_Thrown_Mass(t2));
    }
    else
    {
        // The melee's punch: on its legs (across), or across its barrel meeting its chest.
        vector a;
        vector b;
        vector bodyAt;
        if(c == 12)
        {
            a = VR_AGPT2_At(g, [6, -14, -16]);
            b = VR_AGPT2_At(g, [6, 14, -16]);
            bodyAt = VR_AGPT2_At(g, [2, 0, -16]);
            expectDis = FALSE;
        }
        else
        {
            a = bar + side * step * 0.5;
            b = bar - side * step * 0.5;
            bodyAt = VR_AGPT2_At(g, VR_AGPar_Chest(g));
            expectDis = c == 13;
            expectParry = c == 14;
        }
        VR_AGST_Blow(a, '0 0 0', b, '0 0 0', 1, normalize(b - a), sp, TRUE, VR_MTHING_FIST);
        dmg = VR_AGST_Melee(g, bodyAt, normalize(b - a));
        float kept = 20 * cvar_hget(cvarh_vr_agrunt_parry_damage);
        // (Parried: the parry's share; else not cut: on its legs the melee's own damage, through its weapon into its chest
        // more than a parry's (its chest may be armoured: vr_agarmor; the test pins vr_agarmor_rule 0).)
        extra = sprintf(", %g damage (unparried %g, parried %g)", dmg, vr_agpt2_expect, kept);
        if(expectParry)
        {
            okExtra = fabs(dmg - kept) < 0.01;
        }
        else if(c == 12)
        {
            okExtra = (fabs(dmg - vr_agpt2_expect) < 0.01 && dmg > 0);
        }
        else
        {
            okExtra = dmg > kept + 0.01;
        }
    }
    float disarmed = !g.vr_ag_prop;
    float parried = vr_agpar_n - n0;
    float okDis = disarmed == expectDis;
    float okParry = (parried > 0) == expectParry;
    if(c == 5)
    {
        okExtra = vr_ag_strike_where == 2; // (its forearm, not its weapon, met)
    }
    float ok = okDis && okParry && okExtra;
    extra = sprintf("%s [%g%g%g]", extra, okDis, okParry, okExtra);
    string want = expectParry ? "parried" : "neither";
    if(expectDis)
    {
        want = "disarmed";
    }
    string got = sprintf("%s, parried %g, strikes disarmed %g, %g clatter", disarmed ? "disarmed" : "armed", parried,
        vr_ag_strike_dis - d0, vr_ag_feedbacks - fb);
    VR_AGT_Check(sprintf("strike %s: %s%s; expected %s (%s)", VR_AGST_Name(c), got, extra, want, vr_ag_strike_why), ok);
};

void() VR_AGST_Frame =
{
    entity p = vr_agt_player;
    entity g = vr_agt_grunt;
    if(vr_agt_step == 0)
    {
        if(vr_agst_case == 0)
        {
            vr_agst_keep_beat = cvar("vr_agrunt_strike_beat");
            vr_agst_keep_first = cvar("vr_agrunt_strike_first");
            vr_agst_keep_parry = cvar("vr_agrunt_parry");
            vr_agst_keep_scale = cvar("vr_timescale");
            vr_agst_keep_armor = cvar("vr_agarmor_rule");
            cvar_set("vr_agrunt_parry", "1");
            // (The player's armour rule: a plate saves its share, the rest gets through (case 13's damage past a parry's);
            // rule 1, shipped 2026-10-30, stops the whole hit while its pool lasts.)
            cvar_set("vr_agarmor_rule", "0");
            p.flags = p.flags | FL_NOTARGET;
        }
        if(vr_agst_case >= VR_AGST_CASES)
        {
            VR_AGST_End();
            return;
        }
        VR_AGPT_Clear();
        VR_AGPT_EmptyHand(p);
        cvar_set("vr_timescale", vr_agst_case == 15 ? "0.3" : "1");
        // (Into a raised parry: beat 0, then 1; a tunable, set by `tune` (run before the strike: the grunt's start).)
        float beat = vr_agst_keep_beat;
        if(vr_agst_case == 4)
        {
            beat = 1;
        }
        else if(vr_agst_case == 3 || vr_agst_case == 14)
        {
            beat = 0;
        }
        localcmd(sprintf("tune vr_agrunt_strike_beat %g\n", beat));
        vr_agt_grunt = VR_AGT_Spawn(WID_SHOTGUN, 60);
        vr_agst_until = time + 5;
        vr_agt_step = 1;
        return;
    }
    if(vr_agt_step == 1)
    {
        // Its start done (walkmonster_start_go: it takes damage), its weapon placed, the time scale reached.
        float scaled = vr_agst_case == 15 ? vr_hand_time_scale > 0 && vr_hand_time_scale <= 0.31 : vr_hand_time_scale >= 0.99 || vr_hand_time_scale <= 0;
        if(!(g.takedamage && g.vr_ag_prop && g.vr_ag_prop.vr_ag_was_n >= 1 && scaled))
        {
            if(time > vr_agst_until)
            {
                VR_AGT_Check(sprintf("strike %s: timed out (takedamage %g, weapon %g, hand time scale %g)", VR_AGST_Name(vr_agst_case),
                    g.takedamage, g.vr_ag_prop != world, vr_hand_time_scale), FALSE);
                vr_agt_step = 2;
            }
            return;
        }
        VR_AGST_Strike(g, vr_agst_case);
        vr_agt_step = 2;
        return;
    }
    // (step 2) the next case
    VR_AGT_Remove(g);
    vr_agt_grunt = world;
    vr_agst_case = vr_agst_case + 1;
    vr_agt_step = 0;
};
#endif
