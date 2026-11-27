#ifdef QVR_TESTS
// Test 231 (`vr_agrunt_test 231`; ROUND26.md, "adposekind"; needs AD's data: `vr_campaign_native ad; map vrtesthall`): AD's
// knights' own pose kind (agpose::adKnight, the Pose Studio's "AD Knight", seeded from the knight's keys) keeps their hands
// on their weapons in every key. The death guard (row 4), then the crossbow knight (row 5), frozen 160 ahead, through
// weapon sets (VR_AGAP_Set: the fists, a sword, an axe, a shotgun, a nail pistol, a rocket launcher, the laser cannon, two
// shotguns, two axes, the chainsaw) and, for each, every key its class has (VR_AGAP_Key: Carry, Aim, Run, the Pickup with a
// weapon; the strikes' three phases: the punches, the swings (two held: the scissor too), the chainsaw's thrust; the
// shove's), each held as the Pose Studio holds it (vr_agk_test_hold / _phase: VR_AGK_Hold, the studio's own path) until
// its drawn hands are still (6 frames moving under 0.02; a key whose hold moves by itself, the run's bob, a punch's
// jiggle: after 1 s as it is), then its arms' numbers counted 4 frames under tag row * 512 + set * 48 + key
// (vr_agrunt_ik_tag) and read back (weapondrawnpose(world, tag, 54)): a check each, both hands within VR_AGAP_TOL units
// of their grips on the weapon (reach short; the take's VR_AGAP_TOL_TAKE) and the off hand of its second grip
// (VR_AGAP_TOL_SECOND); `adkey` lines carry each key's numbers and shortfalls' ways (Misc/quakevr/adpose_retarget.py). Ends on `adpose: test 231 over: N
// passed, M failed`.

float vr_agap_step;
float vr_agap_row;   // 0 the death guard, 1 the crossbow knight (from vr_agap_first)
float vr_agap_first; // the first row (scratch1 n > 0: row n - 1 alone, a comparison: 3 the knight)
float vr_agap_last;
float vr_agap_set;
float vr_agap_key;
float vr_agap_at;
float vr_agap_still;
float vr_agap_meas;  // frames counted under the tag
float vr_agap_keys;  // keys measured
vector vr_agap_lastR;
vector vr_agap_lastL;
float vr_agap_w2;    // VR_AGAP_Set's left hand's weapon (0 none)
float VR_AGAP_SETS = 10;
// The tolerances (units): a hand off its grip on the weapon (the knight on his own keys: 25 of 123 keys past 1, up to
// 4.7 in his strikes; ROUND26.md, "adposekind"), the take's, the off hand off its second grip (the Weapon Holds').
float VR_AGAP_TOL = 2.5;
float VR_AGAP_TOL_TAKE = 4;
float VR_AGAP_TOL_SECOND = 4.5;

float(float s) VR_AGAP_Set =
{
    vr_agap_w2 = 0;
    if(s == 0) { return -1; }
    if(s == 1) { return WID_SWORD; }
    if(s == 2) { return WID_AXE; }
    if(s == 3) { return WID_SHOTGUN; }
    if(s == 4) { return WID_NAILPISTOL; }
    if(s == 5) { return WID_ROCKET_LAUNCHER; }
    if(s == 6) { return WID_LASER_CANNON; }
    if(s == 7) { vr_agap_w2 = WID_SHOTGUN; return WID_SHOTGUN; }
    if(s == 8) { vr_agap_w2 = WID_AXE; return WID_AXE; }
    return WID_CHAINSAW;
};

// Key `i` of class `cls` (armed: a weapon in hand): its pose (vr_agap_pose) and phase (vr_agap_phase); FALSE past the last.
float vr_agap_pose;
float vr_agap_phase;
float(float cls, float armed, float i) VR_AGAP_Key =
{
    vr_agap_phase = 0;
    if(i < 3) { vr_agap_pose = i; return TRUE; } // Carry, Aim, Run
    i = i - 3;
    if(armed)
    {
        if(i == 0) { vr_agap_pose = VR_AGK_PICKUP; return TRUE; }
        i = i - 1;
    }
    // (the strikes its class has, three phases each, then the shove's)
    float first = 0;
    float n = 0;
    if(cls == VR_AGK_FISTS) { first = VR_AGK_JAB; n = 4; }
    else if(cls == VR_AGK_MELEE || cls == VR_AGK_MELEE2) { first = VR_AGK_OVERHEAD; n = 4; }
    else if(cls == VR_AGK_TWOMELEE) { first = VR_AGK_OVERHEAD; n = 5; } // (the scissor, VR_AGK_DUAL, after the four)
    else if(cls == VR_AGK_SAW) { first = VR_AGK_SAWTHRUST; n = 1; }
    float s = floor(i / 3);
    vr_agap_phase = i - s * 3;
    if(s < n)
    {
        vr_agap_pose = (cls == VR_AGK_TWOMELEE && s == 4) ? VR_AGK_DUAL : first + s;
        return TRUE;
    }
    if(s == n)
    {
        vr_agap_pose = VR_AGK_SHOVE;
        return TRUE;
    }
    return FALSE;
};

void() VR_AGAP_End =
{
    if(vr_agt_grunt && vr_agt_grunt.classname != "") { VR_AGT_Remove(vr_agt_grunt); }
    vr_agk_test_hold = 0;
    vr_agk_test_phase = 0;
    cvar_set("vr_agrunt_ik_tag", "-1");
    cvar_set("vr_agrunt_freeze", "0");
    cvar_set("vr_agrunt_test_kind", "0");
    vr_agt_player.flags = vr_agt_player.flags - (vr_agt_player.flags & FL_GODMODE);
    VR_AGT_Print(sprintf("adpose: %g keys held on AD's knights", vr_agap_keys));
    VR_Test_Check(vr_agap_keys > 0, "keys held");
    vr_agap_step = 0;
    cvar_set("vr_agrunt_test", "0");
};

void(string what) VR_AGAP_Fail =
{
    VR_Test_Check(FALSE, what);
};

void() VR_AGAP_Frame =
{
    entity pl = vr_agt_player;
    pl.flags = pl.flags | FL_GODMODE | FL_NOTARGET;
    if(vr_agap_step == 0)
    {
        vr_agap_row = 0;
        vr_agap_first = cvar_hget(cvarh_scratch1) > 0 ? cvar_hget(cvarh_scratch1) - 1 : VR_AM_DGUARD;
        vr_agap_last = cvar_hget(cvarh_scratch1) > 0 ? vr_agap_first : VR_AM_DCROSSBOW;
        vr_agap_set = 0;
        VR_Test_BeginFlags("adpose", VR_TEST_LOUD | VR_TEST_QUIET);
        vr_agap_keys = 0;
        if(vr_agap_last >= VR_AM_DGUARD && !cvar_hget(cvarh_vr_ad_mounted))
        {
            VR_AGAP_Fail("needs AD's data (vr_campaign_native ad first)");
            VR_AGAP_End();
            return;
        }
        VR_AGT_ClearWeapons();
        cvar_set("vr_agrunt_freeze", "1");
        localcmd("vr_agrunt_ik_err\n"); // (counted afresh)
        vr_agap_step = 1;
        return;
    }
    if(vr_agap_step == 1)
    {
        // A set's subject.
        if(vr_agt_grunt && vr_agt_grunt.classname != "") { VR_AGT_Remove(vr_agt_grunt); }
        cvar_set("vr_agrunt_test_kind", ftos(vr_agap_first + vr_agap_row));
        float w1 = VR_AGAP_Set(vr_agap_set);
        vr_agt_weapon2 = vr_agap_w2;
        vr_agt_grunt = VR_AGT_Spawn(w1, 160);
        vr_agt_weapon2 = 0;
        vr_agap_key = 0;
        vr_agap_step = 2;
        vr_agap_at = time;
        vr_agap_still = 0;
        vr_agap_meas = -1;
        return;
    }
    entity g = vr_agt_grunt;
    if(!g || g.health <= 0)
    {
        VR_AGAP_Fail(sprintf("row %g set %g: its subject gone", vr_agap_first + vr_agap_row, vr_agap_set));
        VR_AGAP_End();
        return;
    }
    float cls = VR_AGK_Class(g, g.vr_ag_prop);
    if(!VR_AGAP_Key(cls, g.vr_ag_prop != world, vr_agap_key))
    {
        // The set's last key: the next set, the next row, or the end.
        vr_agap_set = vr_agap_set + 1;
        if(vr_agap_set >= VR_AGAP_SETS)
        {
            vr_agap_set = 0;
            vr_agap_row = vr_agap_row + 1;
        }
        if(vr_agap_first + vr_agap_row > vr_agap_last)
        {
            VR_AGAP_End();
            return;
        }
        vr_agap_step = 1;
        return;
    }
    vr_agk_test_hold = vr_agap_pose + 1;
    vr_agk_test_phase = vr_agap_phase;
    float tag = vr_agap_row * 512 + vr_agap_set * 48 + vr_agap_key;
    if(vr_agap_meas >= 0)
    {
        vr_agap_meas = vr_agap_meas + 1;
        if(vr_agap_meas >= 4)
        {
            cvar_set("vr_agrunt_ik_tag", "-1");
            vector m = weapondrawnpose(world, tag, 54);
            // (the take: AD's stoop frame, not a key, holds the hand a little over the weapon; two weapons: the left one
            // isn't taken up, the knight's own the same, 20-24 off)
            float take = vr_agap_pose == VR_AGK_PICKUP;
            float tol = take ? VR_AGAP_TOL_TAKE : VR_AGAP_TOL;
            float ok = m_x >= 0 && m_x <= tol && (m_y <= tol || (take && vr_agap_w2)) && m_z <= VR_AGAP_TOL_SECOND;
            string what = sprintf("tag %g: row %g %s class %g pose %g phase %g", tag, vr_agap_first + vr_agap_row,
                VR_AGT_Name(VR_AGAP_Set(vr_agap_set)), cls, vr_agap_pose, vr_agap_phase);
            VR_Test_Check(ok, sprintf("%s: reach short right %.2f left %.2f, off its second grip %.2f", what, m_x, m_y, m_z));
            VR_AGT_Print(sprintf("adkey %g %g %g %g %v %v %v", tag, vr_agap_first + vr_agap_row, cls,
                vr_agap_pose * 10 + vr_agap_phase, m, weapondrawnpose(world, tag, 55), weapondrawnpose(world, tag, 56)));
            vr_agap_keys = vr_agap_keys + 1;
            vr_agap_key = vr_agap_key + 1;
            vr_agap_meas = -1;
            vr_agap_still = 0;
            vr_agap_at = time;
        }
        return;
    }
    vector r = weapondrawnpose(g, 0, 29);
    vector l = weapondrawnpose(g, 1, 29);
    float drawn = vlen(r) > 0.001 && vlen(l) > 0.001;
    vr_agap_still = (drawn && vlen(r - vr_agap_lastR) < 0.02 && vlen(l - vr_agap_lastL) < 0.02) ? vr_agap_still + 1 : 0;
    vr_agap_lastR = r;
    vr_agap_lastL = l;
    if(vr_agap_still >= 6 || (drawn && time - vr_agap_at > 1))
    {
        cvar_set("vr_agrunt_ik_tag", ftos(tag));
        vr_agap_meas = 0;
    }
    else if(time - vr_agap_at > 3)
    {
        VR_AGAP_Fail(sprintf("tag %g (row %g set %g class %g pose %g phase %g): timed out (its hands never drawn)", tag,
            vr_agap_first + vr_agap_row, vr_agap_set, cls, vr_agap_pose, vr_agap_phase));
        vr_agap_key = vr_agap_key + 1;
        vr_agap_still = 0;
        vr_agap_at = time;
    }
};
#endif
