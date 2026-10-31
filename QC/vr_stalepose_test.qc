#ifdef QVR_TESTS
// ----------------------------------------------------------------------------
// vr_stalepose_test.qc: test 79 (vr_agrunt_test 79), shots and grip spheres from the drawn pose (ROUND23.md, "Armed
// humanoids: shots and grip spheres from the drawn pose"). For every kind (vr_agrunt_test_kind 0..3: grunt, enforcer,
// knight, death knight), 60 ahead of you:
// - Guard: unarmed, frozen in its stand frame and held in its Aim (guard) pose as the Pose Studio holds it (test 90's
//   way), its forearm drawn (weapondrawnpose(g, 0, 37)) far from where its animation frame has it (38). A shot (a
//   hitmodel_segment, the guns' class) from in front across the drawn forearm hits it, near it, in the arm's zone
//   (PositionalRegion 2); one across the frame's forearm alone misses (the frame's arm is drawn nowhere), when the two
//   are VR_SPT_ARM_APART apart (the grunt's guard holds its forearm nearer its frame's: then in its swing).
// - Swing: a sword, its swings forced at you (test 78's way; each kind in turn, overhead first, until the gaps below
//   are met: at most four); at the first frame its drawn grip (VR_AG_DrawnFrame) is
//   VR_SPT_GRIP_APART or more from the server's weapon place (and that place 10 or more from its drawn forearm line), a
//   blow (VR_AG_Hit, the body's part) at the server's place does not disarm it; one at the drawn grip does.
// Ends on `agtest: test 79 over: ...` (run.sh -QuitOn on it); each wait is the state reached (or its give-up, a FAIL).
// ----------------------------------------------------------------------------

float vr_spt_k;        // the kind under test (0..3)
float vr_spt_t0;       // the state's start (its give-up's clock)
float vr_spt_still;    // frames the drawn forearm moved under 0.02
vector vr_spt_last;    // ... its last place
float vr_spt_done;     // the swing's blows dealt
float vr_spt_gap;      // the swing's largest drawn-to-server grip gap
float vr_spt_disarm;   // vr_agrunt_disarm as it was
float vr_spt_kind;     // vr_agrunt_test_kind as it was
float vr_spt_armdone;  // the frame's forearm shot (a miss) tested, in its guard or its swing
float vr_spt_swing;    // its swing's kind (vr_ag_act_kind: 0..3, each in turn until the gaps are met)
float VR_SPT_GRIP_APART = 12; // its drawn grip this far from the server's weapon place: the blows' test (VR_AG_DISARM_HAND 10)

string(float k) VR_SPT_Name =
{
    VR_AM_Row(k);
    return vr_am_name;
};

// A blow by you on `g` at `at` (its body: part 0) as T_DamageDeal hands it to VR_AG_Hit: whether its weapon is kept.
float(entity g, vector at) VR_SPT_Blow =
{
    entity pl = vr_agt_player;
    entity p = g.vr_ag_prop;
    VR_SetHitKind(QVR_HITKIND_MELEE, cVR_MainHand, '0 0 0', '0 0 0');
    hit_blast = 0;
    VR_Hit_SetZone(g, 0, at);
    VR_AG_Hit(g, pl, pl, 1000);
    VR_ClearHitKind();
    return g.vr_ag_prop == p && p != world;
};

float VR_SPT_ARM_APART = 9; // its drawn forearm this far from its frame's: a shot across the frame's alone is a miss test

// Its drawn forearm shot (in state `what`); its frame's forearm too when the two are VR_SPT_ARM_APART apart (else
// noted): TRUE when that miss was tested.
float(entity g, string what) VR_SPT_Arm =
{
    string wn = sprintf("stalepose %s %s", VR_SPT_Name(vr_spt_k), what);
    vector f = weapondrawnpose(g, 0, 37);
    vector m = weapondrawnpose(g, 0, 38);
    vector fw = vr_agt_player.origin - g.origin;
    fw_z = 0;
    fw = normalize(fw);
    VR_AGT_Check(sprintf("%s: its forearm drawn (%v) and in its frame (%v), %.1f apart", wn, f, m, vlen(f - m)),
        f != '0 0 0' && m != '0 0 0');
    vector a = f + fw * 40;
    vector b = f - fw * 8;
    float t = hitmodel_segment(g, a, b, 0, 0);
    vector at = a + (b - a) * max(0, t);
    float region = t >= 0 ? PositionalRegion(g, at, b - a) : -1;
    // (The zone: the same place on its standing pose; the guard's forearm hit there lies out at its side, the arm's zone.
    // A swing's hit may be on the forearm's inner side, which the standing pose holds within the body's zone.)
    float zoned = what != "guard" || region == 2;
    VR_AGT_Check(sprintf("%s: a shot across its drawn forearm hits (t %.2f) %.1f units from it, zone %g%s", wn, t,
        vlen(at - f), region, what == "guard" ? " (want 2: the arm)" : ""), t >= 0 && vlen(at - f) < 10 && zoned);
    if(vlen(f - m) < VR_SPT_ARM_APART)
    {
        VR_AGT_Print(sprintf("%s: its frame's forearm %.1f from the drawn one (under %g): no miss tested here", wn,
            vlen(f - m), VR_SPT_ARM_APART));
        return FALSE;
    }
    a = m + fw * 6;
    b = m - fw * 6;
    t = hitmodel_segment(g, a, b, 0, 0);
    VR_AGT_Check(sprintf("%s: a shot across its frame's forearm alone misses (t %.2f)", wn, t), t == -1);
    return TRUE;
};

void() VR_SPT_Frame =
{
    entity pl = vr_agt_player;
    entity g = vr_agt_grunt;
    float t = time - vr_spt_t0;
    pl.flags = pl.flags | FL_GODMODE | FL_NOTARGET;
    if(vr_agt_step == 0)
    {
        vr_spt_disarm = cvar("vr_agrunt_disarm");
        vr_spt_kind = cvar("vr_agrunt_test_kind");
        cvar_set("vr_agrunt_disarm", "1");
        vr_spt_k = 0;
        vr_agt_step = 1;
        return;
    }
    if(vr_agt_step == 1)
    {
        // The guard: unarmed, frozen, held in Aim.
        cvar_set("vr_agrunt_test_kind", ftos(vr_spt_k));
        cvar_set("vr_agrunt_freeze", "1");
        vr_agk_test_hold = VR_AGK_AIM + 1;
        vr_agt_grunt = g = VR_AGT_Spawn(-1, 60);
        vr_spt_still = 0;
        vr_spt_last = '0 0 0';
        vr_spt_t0 = time;
        vr_agt_step = 2;
        return;
    }
    if(vr_agt_step == 2)
    {
        vector f = weapondrawnpose(g, 0, 37);
        vr_spt_still = (f != '0 0 0' && vlen(f - vr_spt_last) < 0.25) ? vr_spt_still + 1 : 0; // (its guard's breath)
        vr_spt_last = f;
        vr_spt_armdone = FALSE;
        if(vr_spt_still < 10)
        {
            if(t > 10)
            {
                VR_AGT_Check(sprintf("stalepose %s guard: timed out (its drawn forearm never still)", VR_SPT_Name(vr_spt_k)),
                    FALSE);
                vr_spt_still = 10; // (the next part)
            }
            else
            {
                return;
            }
        }
        else
        {
            vr_spt_armdone = VR_SPT_Arm(g, "guard");
        }
        cvar_set("vr_agrunt_freeze", "0");
        vr_agk_test_hold = 0;
        VR_AGT_Remove(g);
        // The swing: a sword.
        vr_agt_grunt = g = VR_AGT_Spawn(WID_SWORD, 60);
        vr_spt_done = FALSE;
        vr_spt_gap = 0;
        vr_spt_swing = 0;
        vr_spt_t0 = time;
        vr_agt_step = 3;
        return;
    }
    entity p = g.vr_ag_prop;
    if(vr_agt_step == 3)
    {
        // Its weapon placed and drawn: held, then its swing forced at you.
        if(!(g.takedamage && p && p.vr_ag_was_n >= 1 && weapondrawnpose(p, 0, 31) != '0 0 0'))
        {
            if(t > 10)
            {
                VR_AGT_Check(sprintf("stalepose %s swing: timed out (its sword never drawn)", VR_SPT_Name(vr_spt_k)), FALSE);
                vr_agt_step = 5;
            }
            return;
        }
        VR_AGST_Hold(g, 0);
        entity was = self;
        self = g;
        g.enemy = pl;
        VR_AGP_Begin(VR_AGP_SWING);
        g.vr_ag_act_kind = vr_spt_swing; // (each kind in turn until the test's gaps are met: not its random pick)
        g.vr_ag_act_hand = 0;
        self = was;
        vr_spt_t0 = time;
        vr_agt_step = 4;
        return;
    }
    if(vr_agt_step == 4)
    {
        string wn = sprintf("stalepose %s swing", VR_SPT_Name(vr_spt_k));
        if(!vr_spt_armdone && vlen(weapondrawnpose(g, 0, 37) - weapondrawnpose(g, 0, 38)) >= VR_SPT_ARM_APART)
        {
            vr_spt_armdone = VR_SPT_Arm(g, "swing"); // (its guard's arm too near its frame's: the swing's)
        }
        if(!vr_spt_done && p)
        {
            VR_AG_DrawnFrame(p);
            vector dg = vr_ag_df_o;
            float gap = vlen(p.origin - dg);
            vr_spt_gap = max(vr_spt_gap, gap);
            float line = VR_AG_HandLineDist(g, p, p.origin);
            if(gap >= VR_SPT_GRIP_APART && line >= 10)
            {
                vr_spt_done = TRUE;
                VR_AGT_Print(sprintf("%s: %.2f s in, its drawn grip %v, the server's weapon %v (%.1f apart, %.1f from its "
                    "drawn forearm line)", wn, t, dg, p.origin, gap, line));
                VR_AGT_Check(sprintf("%s: a blow at the server's weapon place (%.1f from the drawn grip) keeps its weapon", wn,
                    gap), VR_SPT_Blow(g, p.origin));
                VR_AG_DrawnFrame(p);
                VR_AGT_Check(sprintf("%s: a blow at its drawn grip disarms it", wn), !VR_SPT_Blow(g, vr_ag_df_o));
            }
        }
        if((!vr_spt_done || !vr_spt_armdone) && t < VR_AGP_Length(g) + 0.05)
        {
            return; // (its swing's whole length: wind-up, strike, recovery)
        }
        if((!vr_spt_done || !vr_spt_armdone) && vr_spt_swing < 3 && g.vr_ag_prop)
        {
            vr_spt_swing = vr_spt_swing + 1; // (its next kind of swing: overhead, its other, horizontal, its other)
            vr_spt_t0 = time;
            vr_agt_step = 3;
            return;
        }
        if(!vr_spt_done)
        {
            VR_AGT_Check(sprintf("%s: its drawn grip %g units from the server's weapon place (most %.1f in %g swings)", wn,
                VR_SPT_GRIP_APART, vr_spt_gap, vr_spt_swing + 1), FALSE);
        }
        if(!vr_spt_armdone)
        {
            VR_AGT_Check(sprintf("%s: its drawn forearm %g units from its frame's in its guard or swing", wn,
                VR_SPT_ARM_APART), FALSE);
        }
        vr_agt_step = 5;
        return;
    }
    if(vr_agt_step == 5)
    {
        VR_AGT_Remove(g);
        vr_agt_grunt = world;
        vr_spt_k = vr_spt_k + 1;
        vr_agt_step = vr_spt_k < VR_AM_COUNT ? 1 : 6;
        return;
    }
    cvar_set("vr_agrunt_disarm", ftos(vr_spt_disarm));
    cvar_set("vr_agrunt_test_kind", ftos(vr_spt_kind));
    pl.flags = pl.flags - (pl.flags & (FL_GODMODE | FL_NOTARGET));
    vr_agt_step = 0;
    cvar_set("vr_agrunt_test", "0");
};
#endif
