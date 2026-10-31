#ifdef QVR_TESTS
//
//
//
// ----------------------------------------------------------------------------
// vr_armedgrunt_blade_test.qc -- `vr_agrunt_test 77` (Debug > Tests > Armed Grunts: Blade on Blade): your blow on a
// sword grunt's blade away from its hand (vr_armedgrunt_parry.qc VR_AG_BladeContact; ROUND23.md, "Blade on blade"). In
// vrtesthall, you unseen, a fresh sword grunt 60 units ahead for each case, held still, its parry state set (unaware;
// aware but caught out; aware, raised); your main hand's blow mocked as the melee tracker has it (vr_armedgrunt_strike_
// test.qc's VR_AGST_Blow: a held sword's line 32 units long, or a fist), then its weapon's think (VR_AG_WeaponStruck) or
// the melee's damage (T_Damage_VRMelee):
//   your sword on its blade's middle at 3 m/s (unaware), 5 m/s (caught out); on its tip at 3 and 5 m/s: a blade contact
//   (sparks, the clang, your hand knocked back), armed, unhurt, no stagger; on its middle at 8 m/s: the same, and it
//   staggers, its sword arm knocked along the blow; a fist on its middle at 5 m/s: a blade contact without sparks;
//   your sword on its hand at 5 m/s, caught out: disarmed (the hand's rules, unchanged); on its middle at 5 m/s into its
//   raised parry (vr_agrunt_strike_beat 1): parried, armed (no roll on the blade); the melee's cut through its blade into
//   its chest: no damage, a blade contact; across its legs: its full damage; a shotgun grunt's barrel struck at 5 m/s:
//   disarmed (blade contacts are for melee weapons); its middle at 4 m/s (under a deliberate strike), it aware and
//   facing you: parried (the parry's own rule).
// Each case `agtest: PASS|FAIL blade <case>: ...`; ends on `agtest: test 77 over: P passed, F failed`.
// ----------------------------------------------------------------------------

float VR_AGBL_CASES = 12;
float vr_agbl_case;      // the case running
float vr_agbl_until;     // its set-up's give-up time (a safety net: FAIL "timed out")
float vr_agbl_keep_beat; // cvars before (put back at the end)
float vr_agbl_keep_first;
float vr_agbl_keep_parry;
float vr_agbl_keep_armor;

string(float c) VR_AGBL_Name =
{
    if(c == 0) { return "sword on its blade's middle, 3 m/s, unaware"; }
    if(c == 1) { return "sword on its blade's middle, 5 m/s, caught out"; }
    if(c == 2) { return "sword on its blade's tip, 3 m/s, unaware"; }
    if(c == 3) { return "sword on its blade's tip, 5 m/s, caught out"; }
    if(c == 4) { return "sword on its blade's middle, 8 m/s, caught out"; }
    if(c == 5) { return "fist on its blade's middle, 5 m/s, caught out"; }
    if(c == 6) { return "sword on its sword hand, 5 m/s, caught out"; }
    if(c == 7) { return "sword on its blade's middle, 5 m/s, into its raised parry, beat 1"; }
    if(c == 8) { return "the melee's cut through its blade into its chest, 5 m/s, caught out"; }
    if(c == 9) { return "the melee's cut across its legs, 5 m/s, caught out"; }
    if(c == 10) { return "sword on a shotgun grunt's barrel, 5 m/s, caught out"; }
    return "sword on its blade's middle, 4 m/s, aware and facing you";
};

void() VR_AGBL_End =
{
    entity p = vr_agt_player;
    localcmd(sprintf("tune vr_agrunt_strike_beat %g\n", vr_agbl_keep_beat)); // (a tunable: set by `tune`)
    cvar_set("vr_agrunt_strike_first", ftos(vr_agbl_keep_first));
    cvar_set("vr_agrunt_parry", ftos(vr_agbl_keep_parry));
    cvar_set("vr_agarmor_rule", ftos(vr_agbl_keep_armor));
    VR_AGPT_Clear();
    p.flags = p.flags - (p.flags & FL_NOTARGET);
    p.vrbits0 = p.vrbits0 - (p.vrbits0 & QVR_VRBITS0_MAINHAND_GRABBING);
    p.weapon = WID_FIST;
    vr_agbl_case = 0;
    cvar_set("vr_agrunt_test", "0");
};

// The case's blow, its result checked.
void(entity g, float c) VR_AGBL_Strike =
{
    entity p = vr_agt_player;
    entity w = g.vr_ag_prop;
    VR_AG_WeaponLine(w);
    vector wa = vr_ag_wa;
    vector wb = vr_ag_wb;
    vector along = normalize(wb - wa);
    vector side = normalize(VR_Cross(along, '0 0 1'));
    vector up = normalize(VR_Cross(side, along));
    float sp = 5;
    if(c == 0 || c == 2) { sp = 3; }
    if(c == 4) { sp = 8; }
    if(c == 11) { sp = 4; }
    float step = sp * VRMetersToUnits() / 72; // (the blow's way in a frame)
    float state = 1;
    if(c == 0 || c == 2) { state = 0; }
    if(c == 7) { state = 2; }
    VR_AGST_Hold(g, state);
    cvar_set("vr_agrunt_strike_first", "1");
    vector at = wa + (wb - wa) * 0.55; // (its blade's middle)
    if(c == 2 || c == 3) { at = wa + (wb - wa) * 0.92; } // (near its tip)
    if(c == 10) { at = wa + (wb - wa) * 0.75; } // (a shotgun's barrel)
    if(c == 6)
    {
        VR_AGS_HandLine(g, w);
        at = vr_ags_ha + normalize(vr_ags_hb - vr_ags_ha) * 2; // (its hand)
    }
    float n0 = vr_agpar_n;
    float b0 = vr_agbc_n;
    float s0 = vr_agbc_sparks;
    float k0 = vr_agbc_kicks;
    float h0 = g.health;
    float dmg = -1;
    vector go = side * -1;
    vector a0 = at + side * step * 0.5;
    vector b0p = at - side * step * 0.5;
    p.weapon = c == 5 ? WID_FIST : WID_SWORD;
    if(c == 5)
    {
        VR_AGST_Blow(a0, '0 0 0', b0p, '0 0 0', 1, go, sp, TRUE, VR_MTHING_FIST);
    }
    else
    {
        // (your sword's line across its blade, 32 units, crossing it at `at`)
        VR_AGST_Blow(a0 - up * 16, a0 + up * 16, b0p - up * 16, b0p + up * 16, 2, go, sp, TRUE, VR_MTHING_SWORD);
    }
    if(c == 8 || c == 9)
    {
        vector bodyAt = VR_AGPT2_At(g, VR_AGPar_Chest(g));
        if(c == 9)
        {
            vector la = VR_AGPT2_At(g, [6, -14, -16]);
            vector lb = VR_AGPT2_At(g, [6, 14, -16]);
            bodyAt = VR_AGPT2_At(g, [2, 0, -16]);
            go = normalize(lb - la);
            VR_AGST_Blow(la, '0 0 0', lb, '0 0 0', 1, go, sp, TRUE, VR_MTHING_SWORD);
        }
        dmg = VR_AGST_Melee(g, bodyAt, go);
    }
    else
    {
        VR_AG_WeaponStruck(g, w);
    }
    float disarmed = !g.vr_ag_prop;
    float parried = vr_agpar_n - n0;
    float blade = vr_agbc_n - b0;
    float sparks = vr_agbc_sparks - s0;
    float kicks = vr_agbc_kicks - k0;
    float staggered = g.vr_parry_stagger_until > time;
    float hurt = h0 - g.health;
    // The expected outcome.
    float wantDis = c == 6 || c == 10;
    float wantParry = c == 7 || c == 11;
    float wantBlade = (!wantDis && !wantParry && (c != 9));
    float wantStagger = c == 4;
    float ok = ((disarmed == wantDis) && ((parried > 0) == wantParry) && ((blade > 0) == wantBlade) &&
                (staggered == wantStagger) && ((kicks > 0) == wantStagger));
    if(wantBlade)
    {
        ok = (ok && (hurt < 0.01) && (blade == 1) && ((sparks > 0) == (c != 5)));
    }
    if(c == 9)
    {
        ok = (ok && (fabs(dmg - vr_agpt2_expect) < 0.01) && (dmg > 0));
    }
    string want = wantDis ? "disarmed" : wantParry ? "parried" : wantBlade ? "a blade contact" : "its full damage";
    if(wantStagger)
    {
        want = strcat(want, ", staggered, its arm knocked");
    }
    string got = sprintf("%s, parried %g, blade contacts %g (sparks %g), staggered %g, arm kicks %g, hurt %g", disarmed ?
        "disarmed" : "armed", parried, blade, sparks, staggered, kicks, hurt);
    if(dmg >= 0)
    {
        got = sprintf("%s, dealt %g of %g", got, dmg, vr_agpt2_expect);
    }
    VR_AGT_Check(sprintf("blade %s: %s; expected %s (%s%s)", VR_AGBL_Name(c), got, want, vr_ag_strike_why, vr_agbc_why), ok);
};

void() VR_AGBL_Frame =
{
    entity p = vr_agt_player;
    entity g = vr_agt_grunt;
    if(vr_agt_step == 0)
    {
        if(vr_agbl_case == 0)
        {
            vr_agbl_keep_beat = cvar("vr_agrunt_strike_beat");
            vr_agbl_keep_first = cvar("vr_agrunt_strike_first");
            vr_agbl_keep_parry = cvar("vr_agrunt_parry");
            vr_agbl_keep_armor = cvar("vr_agarmor_rule");
            cvar_set("vr_agrunt_parry", "1");
            cvar_set("vr_agarmor_rule", "0"); // (the armour rule's plates stay out of the damage cases: test 94's)
            p.flags = p.flags | FL_NOTARGET;
        }
        if(vr_agbl_case >= VR_AGBL_CASES)
        {
            VR_AGBL_End();
            return;
        }
        VR_AGPT_Clear();
        VR_AGPT_EmptyHand(p);
        localcmd(sprintf("tune vr_agrunt_strike_beat %g\n", vr_agbl_case == 7 ? 1 : vr_agbl_keep_beat));
        vr_agt_grunt = VR_AGT_Spawn(vr_agbl_case == 10 ? WID_SHOTGUN : WID_SWORD, 60);
        vr_agbl_until = time + 5;
        vr_agt_step = 1;
        return;
    }
    if(vr_agt_step == 1)
    {
        // Its start done (walkmonster_start_go: it takes damage), its weapon placed and drawn.
        if(!(g.takedamage && g.vr_ag_prop && g.vr_ag_prop.vr_ag_was_n >= 1 && weapondrawnpose(g.vr_ag_prop, 0, 31) != '0 0 0'))
        {
            if(time > vr_agbl_until)
            {
                VR_AGT_Check(sprintf("blade %s: timed out (takedamage %g, weapon %g)", VR_AGBL_Name(vr_agbl_case),
                    g.takedamage, g.vr_ag_prop != world), FALSE);
                vr_agt_step = 2;
            }
            return;
        }
        VR_AGBL_Strike(g, vr_agbl_case);
        vr_agt_step = 2;
        return;
    }
    // (step 2) the next case
    VR_AGT_Remove(g);
    vr_agt_grunt = world;
    vr_agbl_case = vr_agbl_case + 1;
    vr_agt_step = 0;
};
#endif
