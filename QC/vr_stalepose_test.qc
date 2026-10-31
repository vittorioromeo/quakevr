#ifdef QVR_TESTS
// ----------------------------------------------------------------------------
// vr_stalepose_test.qc: test 79 (vr_agrunt_test 79), shots and grip spheres from the drawn pose (ROUND23.md, "Armed
// humanoids: shots and grip spheres from the drawn pose"). For every kind (vr_agrunt_test_kind 0..3: grunt, enforcer,
// knight, death knight), 60 ahead of you:
// - Guard: unarmed, frozen in its stand frame and held in its Aim (guard) pose as the Pose Studio holds it (test 90's
//   way), its forearm drawn (weapondrawnpose(g, 0, 37)) far from where its animation frame has it (38). A shot (a
//   hitmodel_segment, the guns' class) from in front across the drawn forearm hits it, near it, in the arm's zone
//   (PositionalRegion 2); one across the frame's forearm alone misses (the frame's arm is drawn nowhere).
// - Swing: a sword, its swing forced at you (test 78's way); at the first frame its drawn grip (VR_AG_DrawnFrame) is 18
//   units or more from the server's weapon place (and that place 10 or more from its drawn forearm line), a blow
//   (VR_AG_Hit, the body's part) at the server's place does not disarm it; one at the drawn grip does.
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

// The guard: its drawn forearm shot, its frame's forearm not.
void(entity g) VR_SPT_Guard =
{
    string wn = sprintf("stalepose %s guard", VR_SPT_Name(vr_spt_k));
    vector f = weapondrawnpose(g, 0, 37);
    vector m = weapondrawnpose(g, 0, 38);
    vector fw = vr_agt_player.origin - g.origin;
    fw_z = 0;
    fw = normalize(fw);
    VR_AGT_Check(sprintf("%s: its forearm drawn %.1f units from its frame's (drawn %v, frame %v)", wn, vlen(f - m), f, m),
        f != '0 0 0' && m != '0 0 0' && vlen(f - m) > 6);
    vector a = f + fw * 40;
    vector b = f - fw * 8;
    float t = hitmodel_segment(g, a, b, 0, 0);
    vector at = a + (b - a) * max(0, t);
    float region = t >= 0 ? PositionalRegion(g, at, b - a) : -1;
    VR_AGT_Check(sprintf("%s: a shot across its drawn forearm hits (t %.2f) %.1f units from it, zone %g (want 2: the arm)",
        wn, t, vlen(at - f), region), t >= 0 && vlen(at - f) < 10 && region == 2);
    a = m + fw * 6;
    b = m - fw * 6;
    t = hitmodel_segment(g, a, b, 0, 0);
    VR_AGT_Check(sprintf("%s: a shot across its frame's forearm alone misses (t %.2f)", wn, t), t == -1);
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
        vr_spt_still = (f != '0 0 0' && vlen(f - vr_spt_last) < 0.02) ? vr_spt_still + 1 : 0;
        vr_spt_last = f;
        if(vr_spt_still < 20)
        {
            if(t > 10)
            {
                VR_AGT_Check(sprintf("stalepose %s guard: timed out (its drawn forearm never still)", VR_SPT_Name(vr_spt_k)),
                    FALSE);
                vr_spt_still = 20; // (the next part)
            }
            else
            {
                return;
            }
        }
        else
        {
            VR_SPT_Guard(g);
        }
        cvar_set("vr_agrunt_freeze", "0");
        vr_agk_test_hold = 0;
        VR_AGT_Remove(g);
        // The swing: a sword.
        vr_agt_grunt = g = VR_AGT_Spawn(WID_SWORD, 60);
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
        g.vr_ag_act_hand = 0;
        self = was;
        vr_spt_done = FALSE;
        vr_spt_gap = 0;
        vr_spt_t0 = time;
        vr_agt_step = 4;
        return;
    }
    if(vr_agt_step == 4)
    {
        string wn = sprintf("stalepose %s swing", VR_SPT_Name(vr_spt_k));
        if(!vr_spt_done && p)
        {
            VR_AG_DrawnFrame(p);
            vector dg = vr_ag_df_o;
            float gap = vlen(p.origin - dg);
            vr_spt_gap = max(vr_spt_gap, gap);
            float line = VR_AG_HandLineDist(g, p, p.origin);
            if(gap >= 18 && line >= 10)
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
        if(!vr_spt_done && t < VR_AGP_Length(g) + 0.05)
        {
            return; // (its swing's whole length: wind-up, strike, recovery)
        }
        if(!vr_spt_done)
        {
            VR_AGT_Check(sprintf("%s: its drawn grip 18 units from the server's weapon place (most %.1f)", wn, vr_spt_gap),
                FALSE);
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
