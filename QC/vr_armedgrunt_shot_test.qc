#ifdef QVR_TESTS
//
//
//
// ----------------------------------------------------------------------------
// vr_armedgrunt_shot_test.qc -- shooting the weapon out of its hand (vr_armedgrunt_shot.qc): `vr_agrunt_test 50` in
// vrtesthall (start 0 -288 25 facing 90: 640 free ahead), the player unseen (notarget) and kept alive. Each case: a fresh
// armed grunt 520 units ahead (5000 health), its weapon or hand shot from `dist` units off it along the player's line to
// it (the pellets, nails and bolts the player's own: FireBulletsImpl, launch_spike, LightningDamage), the result 0.5 s
// after the last shot; `scratch1` trials a case (default 100). Prints `agshot: <case>: <k>/<n> disarmed (<rate>), hits
// on weapons <w>, on hands <h>, rolls <r> a trial, health lost <l>` and checks the defaults' targets (PASS/FAIL), then
// `agshot: done: <p> passed, <f> failed`.
// ----------------------------------------------------------------------------

float vr_agst_case;     // the case
float vr_agst_trial;    // ... its trial
float vr_agst_n;        // ... trials
float vr_agst_step;     // 0 the case's setup, 1 a trial's grunt, 2 its shots, 3 its result
float vr_agst_until;
float vr_agst_shots;    // the trial's shots fired
float vr_agst_d0;       // vr_ags_disarms at the trial's start
float vr_agst_dis;      // the case's disarms
float vr_agst_wrong;    // ... dual: disarms of the wrong hand's weapon
float vr_agst_hw0;      // vr_ags_hits_w at the case's start
float vr_agst_hh0;      // ... vr_ags_hits_h
float vr_agst_r0;       // ... vr_ags_rolls
float vr_agst_loss;     // ... health lost in all
float vr_agst_ab0;      // ... vr_ags_absorbed at its start
float vr_agst_h0;       // the trial's grunt's health before its shots
float vr_agst_passed;
float vr_agst_failed;
entity vr_agst_g;
entity vr_agst_right;   // dual: its right hand's weapon at the trial's start
float vr_agst_rate0;    // shotgun 2 m: its rate, its health lost a trial
float vr_agst_loss0;
float vr_agst_small;    // size 0.5's rate (15 m)
float vr_agst_keep_parts;
float vr_agst_keep_size;
float vr_agst_keep_absorb;
float vr_agst_keep_disarm;

// The case's parameters (VR_AGST_Setup).
string vr_agst_name;
float vr_agst_wid;      // its weapon (two: a sword and a crowbar)
float vr_agst_kind;     // 0 a shotgun shot, 1 a super shotgun's, 2 nails, 3 lightning, 4 one pellet
float vr_agst_count;    // shots
float vr_agst_dist;     // units the shots come from
float vr_agst_aim;      // 0 the weapon, 1 the forearm, 2 the muzzle end
float vr_agst_dual;     // the left hand's weapon shot

void(string s) VR_AGST_Print = { bprint(sprintf("agshot: %s\n", s)); };

void(string what, float ok) VR_AGST_Check =
{
    VR_AGST_Print(sprintf("%s %s", what, ok ? "PASS" : "FAIL"));
    if(ok) { vr_agst_passed = vr_agst_passed + 1; } else { vr_agst_failed = vr_agst_failed + 1; }
};

// Case `c`'s parameters and settings (FALSE: none left).
float(float c) VR_AGST_Setup =
{
    vr_agst_wid = WID_SHOTGUN;
    vr_agst_kind = 0;
    vr_agst_count = 1;
    vr_agst_dist = 64;
    vr_agst_aim = 0;
    vr_agst_dual = FALSE;
    cvar_set("vr_agrunt_shotdisarm_parts", ftos(vr_agst_keep_parts));
    cvar_set("vr_agrunt_shotdisarm_size", ftos(vr_agst_keep_size));
    cvar_set("vr_agrunt_shotdisarm_absorb", ftos(vr_agst_keep_absorb));
    if(c == 0) { vr_agst_name = "shotgun 2 m on the gun"; return TRUE; }
    if(c == 1) { vr_agst_name = "shotgun 5 m on the gun"; vr_agst_dist = 160; return TRUE; }
    if(c == 2) { vr_agst_name = "shotgun 15 m on the gun"; vr_agst_dist = 480; return TRUE; }
    if(c == 3) { vr_agst_name = "super shotgun 2 m on the gun"; vr_agst_kind = 1; return TRUE; }
    if(c == 4) { vr_agst_name = "3 nails in 0.4 s on the gun, 5 m"; vr_agst_kind = 2; vr_agst_count = 3; vr_agst_dist = 160; return TRUE; }
    if(c == 5) { vr_agst_name = "1 nail on the gun, 10 m"; vr_agst_kind = 2; vr_agst_dist = 320; return TRUE; }
    if(c == 6) { vr_agst_name = "1 pellet on the gun, 10 m"; vr_agst_kind = 4; vr_agst_dist = 320; return TRUE; }
    if(c == 7) { vr_agst_name = "lightning 0.2 s on the gun, 5 m"; vr_agst_kind = 3; vr_agst_count = 2; vr_agst_dist = 160; return TRUE; }
    if(c == 8) { vr_agst_name = "shotgun 2 m on the forearm"; vr_agst_aim = 1; return TRUE; }
    if(c == 9)
    {
        vr_agst_name = "hand only: shotgun 2 m on the muzzle end";
        vr_agst_aim = 2;
        cvar_set("vr_agrunt_shotdisarm_parts", "1");
        return TRUE;
    }
    if(c == 10)
    {
        vr_agst_name = "hand only: shotgun 2 m on the forearm";
        vr_agst_aim = 1;
        cvar_set("vr_agrunt_shotdisarm_parts", "1");
        return TRUE;
    }
    if(c == 11)
    {
        vr_agst_name = "weapon only: shotgun 2 m on the forearm";
        vr_agst_aim = 1;
        cvar_set("vr_agrunt_shotdisarm_parts", "0");
        return TRUE;
    }
    if(c == 12)
    {
        vr_agst_name = "size 0.5: shotgun 15 m on the gun";
        vr_agst_dist = 480;
        cvar_set("vr_agrunt_shotdisarm_size", "0.5");
        return TRUE;
    }
    if(c == 13)
    {
        vr_agst_name = "size 2: shotgun 15 m on the gun";
        vr_agst_dist = 480;
        cvar_set("vr_agrunt_shotdisarm_size", "2");
        return TRUE;
    }
    if(c == 14)
    {
        vr_agst_name = "absorb: shotgun 2 m on the gun";
        cvar_set("vr_agrunt_shotdisarm_absorb", "1");
        return TRUE;
    }
    if(c == 15)
    {
        vr_agst_name = "dual: shotgun 2 m on the left hand's crowbar";
        vr_agst_wid = WID_SWORD;
        vr_agst_dual = TRUE;
        return TRUE;
    }
    return FALSE;
};

// The weapon shot at: its left hand's (dual), else its first.
entity(entity g) VR_AGST_Target =
{
    if(vr_agst_dual)
    {
        return g.vr_ag_prop2.vr_ag_left ? g.vr_ag_prop2 : (g.vr_ag_prop.vr_ag_left ? g.vr_ag_prop : world);
    }
    return g.vr_ag_prop;
};

vector(entity g, entity p) VR_AGST_AimAt =
{
    if(vr_agst_aim == 1)
    {
        VR_AGS_HandLine(g, p);
        return vr_ags_ha + (vr_ags_hb - vr_ags_ha) * 0.7;
    }
    VR_AG_WeaponLine(p);
    if(vr_agst_aim == 2)
    {
        return vr_ag_wb - normalize(vr_ag_wb - p.origin) * 2;
    }
    return p.origin + (vr_ag_wb - p.origin) * 0.5;
};

// One shot of the case's at its target (as the player's).
void(entity g, entity p) VR_AGST_Fire =
{
    entity pl = vr_agt_player;
    vector at = VR_AGST_AimAt(g, p);
    vector eye = pl.origin + pl.view_ofs;
    vector dir = normalize(at - eye);
    vector from = at - dir * vr_agst_dist;
    entity oldself = self;
    self = pl;
    if(vr_agst_kind == 2)
    {
        launch_spike(from, dir);
    }
    else if(vr_agst_kind == 3)
    {
        VR_SetHitKind(QVR_HITKIND_SHOT, cVR_MainHand, '0 0 0', dir);
        LightningDamage(from, at + dir * 64, pl, cvar_hget(cvarh_vr_dmg_lightning));
        VR_ClearHitKind();
    }
    else
    {
        float n = vr_agst_kind == 1 ? 14 : (vr_agst_kind == 4 ? 1 : 6);
        vector spread = vr_agst_kind == 1 ? '0.035 0.025 0' : '0.01 0.01 0';
        vr_pellet_damage = cvar_hget(vr_agst_kind == 1 ? cvarh_vr_dmg_super_shotgun : cvarh_vr_dmg_shotgun);
        VR_SetHitKind(QVR_HITKIND_SHOT, cVR_MainHand, '0 0 0', dir);
        vr_tracer_hand = cVR_MainHand;
        vr_tracer_from = from;
        FireBulletsImpl(n, dir, spread, from, 2048, FALSE);
        VR_ClearHitKind();
        vr_pellet_damage = 0;
    }
    self = oldself;
};

// The case's results printed and checked.
void() VR_AGST_Report =
{
    float n = max(1, vr_agst_n);
    float rate = vr_agst_dis / n;
    float loss = vr_agst_loss / n;
    float ab = (vr_ags_absorbed - vr_agst_ab0) / n;
    // (two prints: sprintf takes 7 values at most)
    VR_AGST_Print(sprintf("%s: %g/%g disarmed (%.2f), hits on weapons %.2f, on hands %.2f", vr_agst_name, vr_agst_dis,
        vr_agst_n, rate, (vr_ags_hits_w - vr_agst_hw0) / n, (vr_ags_hits_h - vr_agst_hh0) / n));
    VR_AGST_Print(sprintf("    a trial: rolls %.2f, shots stopped %.2f, health lost %.1f", (vr_ags_rolls - vr_agst_r0) / n, ab,
        loss));
    float c = vr_agst_case;
    if(c == 0)
    {
        vr_agst_rate0 = rate;
        vr_agst_loss0 = loss;
        VR_AGST_Check("shotgun 2 m: 0.5..0.8, none stopped:", rate >= 0.5 && rate <= 0.8 && ab == 0);
    }
    if(c == 4) { VR_AGST_Check("3 nails: 0.25..0.5:", rate >= 0.25 && rate <= 0.5); }
    if(c == 7) { VR_AGST_Check("lightning 0.2 s: 0.2..0.6:", rate >= 0.2 && rate <= 0.6); }
    if(c == 5 || c == 6) { VR_AGST_Check("a single far shot: 0.05 at most:", rate <= 0.05); }
    if(c == 9) { VR_AGST_Check("hand only, the muzzle end: 0.05 at most:", rate <= 0.05); }
    if(c == 10) { VR_AGST_Check("hand only, the forearm: over 0.2:", rate > 0.2); }
    if(c == 12) { vr_agst_small = rate; }
    if(c == 13) { VR_AGST_Check(sprintf("size 2 over size 0.5 (%.2f):", vr_agst_small), rate > vr_agst_small); }
    if(c == 14)
    {
        VR_AGST_Check(sprintf("absorb: pellets stopped, no more health lost than passing through (%.1f):", vr_agst_loss0),
            ab >= 3 && loss < vr_agst_loss0);
    }
    if(c == 15) { VR_AGST_Check(sprintf("dual: the left's alone (%g wrong), over 0.3:", vr_agst_wrong), vr_agst_wrong == 0 && rate > 0.3); }
};

void(float t) VR_AGS_TestFrame =
{
    entity pl = vr_agt_player;
    entity g = vr_agst_g;
    pl.flags = pl.flags | FL_NOTARGET;
    if(vr_agst_step == 0)
    {
        if(vr_agst_case == 0)
        {
            vr_agst_keep_parts = cvar("vr_agrunt_shotdisarm_parts");
            vr_agst_keep_size = cvar("vr_agrunt_shotdisarm_size");
            vr_agst_keep_absorb = cvar("vr_agrunt_shotdisarm_absorb");
            vr_agst_keep_disarm = cvar("vr_agrunt_fall_drop");
            cvar_set("vr_agrunt_fall_drop", "0"); // (only its shots: not a knockdown's drop)
            vr_agst_passed = 0;
            vr_agst_failed = 0;
        }
        if(!VR_AGST_Setup(vr_agst_case))
        {
            cvar_set("vr_agrunt_fall_drop", ftos(vr_agst_keep_disarm));
            VR_AGST_Print(sprintf("done: %g passed, %g failed", vr_agst_passed, vr_agst_failed));
            pl.flags = pl.flags - (pl.flags & FL_NOTARGET);
            vr_agst_case = 0;
            cvar_set("vr_agrunt_test", "0");
            if(cvar("scratch2") == 1)
            {
                localcmd("toggleconsole; quit\n"); // (a headless run's end, however long it took)
            }
            return;
        }
        vr_agst_n = cvar("scratch1") > 0 ? cvar("scratch1") : (vr_agst_case <= 8 ? 100 : 40); // (the defaults' rates: 100)
        vr_agst_trial = 0;
        vr_agst_dis = 0;
        vr_agst_wrong = 0;
        vr_agst_loss = 0;
        vr_agst_hw0 = vr_ags_hits_w;
        vr_agst_hh0 = vr_ags_hits_h;
        vr_agst_r0 = vr_ags_rolls;
        vr_agst_ab0 = vr_ags_absorbed;
        vr_agst_step = 1;
        return;
    }
    if(vr_agst_step == 1)
    {
        VR_AGPT_Clear();
        vr_agt_weapon2 = vr_agst_dual ? WID_CROWBAR : 0;
        vr_agst_g = VR_AGT_Spawn(vr_agst_wid, 520);
        vr_agt_weapon2 = 0;
        vr_agst_g.health = 5000;
        vr_agst_step = 2;
        vr_agst_shots = 0;
        vr_agst_until = time + 0.2;
        return;
    }
    if(time < vr_agst_until)
    {
        return;
    }
    if(vr_agst_step == 2)
    {
        entity p = VR_AGST_Target(g);
        if(vr_agst_shots == 0)
        {
            vr_agst_d0 = vr_ags_disarms;
            vr_agst_h0 = g.health; // (its spawn may have set its own since)
            vr_agst_right = vr_agst_dual ? (g.vr_ag_prop == p ? g.vr_ag_prop2 : g.vr_ag_prop) : world;
        }
        if(p && vr_ags_disarms == vr_agst_d0)
        {
            VR_AGST_Fire(g, p);
        }
        vr_agst_shots = vr_agst_shots + 1;
        vr_agst_until = time + (vr_agst_kind == 3 ? 0.1 : 0.2);
        if(vr_agst_shots >= vr_agst_count)
        {
            vr_agst_step = 3;
            vr_agst_until = time + 0.1 + (vr_agst_kind == 2 ? vr_agst_dist / 1750 : 0); // (a nail's flight; a roll's frame)
        }
        return;
    }
    // The trial's result
    if(vr_ags_disarms > vr_agst_d0)
    {
        vr_agst_dis = vr_agst_dis + 1;
        if(vr_agst_dual && (!vr_agst_right || (g.vr_ag_prop != vr_agst_right && g.vr_ag_prop2 != vr_agst_right)))
        {
            vr_agst_wrong = vr_agst_wrong + 1;
        }
    }
    vr_agst_loss = vr_agst_loss + (vr_agst_h0 - g.health);
    vr_agst_trial = vr_agst_trial + 1;
    vr_agst_step = 1;
    if(vr_agst_trial >= vr_agst_n)
    {
        VR_AGST_Report();
        VR_AGPT_Clear();
        vr_agst_case = vr_agst_case + 1;
        vr_agst_step = 0;
    }
};
#endif
