// ----------------------------------------------------------------------------
// vr_armedgrunt_ragfire.qc: armed humanoids firing as ragdolls (vr_agrunt_ragfire, a prototype, off by default;
// docs/vr-port/RAGFIRE.md; his words 2026-11-28, item 17).
//
// One holding a one-handed gun (VR_AG_RagFireGun) that staggers, lies knocked down or is held in a player's hands as a
// ragdoll (.vr_knockdown 1, not getting up, no tug-of-war over its gun) keeps firing it at its target:
// - its gun arm reaches towards the target (ragdollaim: the active ragdoll's arm reach, vr_agrunt_ragfire_aim; the
//   kinds driven alive only: the others fire only when their gun happens to point near);
// - it fires only while its gun, as it is drawn on the ragdoll's hand, points within vr_agrunt_ragfire_cone of the
//   target, its shot turned towards it by at most vr_agrunt_ragfire_steer (its wrist), off by up to
//   vr_agrunt_ragfire_error at random, at most once every vr_agrunt_ragfire_interval (nails: _nail_interval) s;
// - never with its muzzle in the floor or a wall (the line from its grip to its muzzle clear), never into the floor,
//   a wall or a friend before its target (the line from its muzzle clear that far); its own shots can't hit it (bullets
//   leave it out of their traces, nails and lasers are its own);
// - the shot is its gun's own (VR_AG_ShootProp: the clip, the flash, the friend and blast checks), from the muzzle
//   along the line chosen here (vr_ag_rf_dir: no convergence on the target, no drawn-line turn), a replacement's own
//   gun's too (VR_AGR_NativeShot, EnemyFireBullets).
// Called from its gun's think (VR_AG_PropThink) every frame while it is a ragdoll, on the server only.
// ----------------------------------------------------------------------------

float vr_ag_rf_fired;   // every shot fired as a ragdoll (the tests)
float vr_ag_rf_blocked; // ... held back: its muzzle or its line blocked (the tests)
float vr_ag_rf_asked;   // ... times one was due and its gun pointed within the cone (the tests)
float vr_ag_rf_ang;     // the last one's drawn gun's angle off its target, degrees (each frame it may fire; the tests)
float vr_ag_rf_ang_at;  // ... the time it was measured
float vr_ag_rf_shot_ang; // ... as its last shot left (the tests)
float VR_AG_RF_AIM_HOLD = 0.3; // s: each ragdollaim's hold (asked every frame: it eases out this long after the last)
float VR_AG_RF_CORR_RATE = 3;  // 1/s: its aim's turn for its gun's line off its arm's, learnt this fast
float VR_AG_RF_CORR_MOST = 0.6; // ... at most this (a unit vector's length: about 35 degrees)
.vector vr_ag_rf_corr;         // that turn now (VR_AG_RagFire; cleared with each new ragdoll)

// A gun it may fire as a ragdoll: one-handed and ranged, never big, explosive or a stream (his list: the shotgun, the
// nailgun, the nail pistol, the revolver; the grunt's and the enforcer's own guns with them). Not the flare gun (it sets
// you on fire), not Dwell's variants of these (their own shots).
float(entity p) VR_AG_RagFireGun =
{
    if(!p)
    {
        return FALSE;
    }
    float wid = p.weapon;
    entity inst = p.weaponinst;
    if(inst && inst.wi_dwvar && DW_VariantFor(wid) == inst.wi_dwvar)
    {
        return FALSE;
    }
    return wid == WID_SHOTGUN || wid == WID_NAILGUN || wid == WID_NAILPISTOL || wid == WID_REVOLVER ||
           wid == WID_GRUNTGUN || wid == WID_ENFRIFLE;
};

// The time to its next shot as a ragdoll (vr_agrunt_ragfire_interval, a nail gun's _nail_interval; up to half again).
float(entity p) VR_AG_RagFireInterval =
{
    float nails = (p.weapon == WID_NAILGUN || p.weapon == WID_NAILPISTOL);
    float iv = max(0.05, cvar_hget(nails ? cvarh_vr_agrunt_ragfire_nail_interval : cvarh_vr_agrunt_ragfire_interval));
    return iv * (1 + 0.5 * random());
};

void(entity g, entity p) VR_AG_RagFire =
{
    if(g.vr_ag_prop != p || g.health <= 0 || !isragdoll(g) || g.vr_kd_rise || g.vr_ag_tug_pl || !VR_AG_RagFireGun(p))
    {
        return;
    }
    if(!p.vr_ag_native && p.weaponinst.wi_clip < 1)
    {
        return; // (empty: it reloads once it is up)
    }
    entity t = g.enemy;
    if(!t || t.health <= 0 || !t.takedamage)
    {
        return;
    }
    if(!cvar_hget(cvarh_vr_agrunt_ragfire_held) && VR_Knockdown_Held(g))
    {
        return;
    }
    if(g.vr_ag_rf_since != g.vr_kd_since || !g.vr_ag_rf_next)
    {
        g.vr_ag_rf_since = g.vr_kd_since; // (a new ragdoll: its first shot an interval on)
        g.vr_ag_rf_next = time + VR_AG_RagFireInterval(p);
        g.vr_ag_rf_corr = '0 0 0';
    }
    entity oldself = self;
    self = g;
    vector at = VR_AG_AimPoint(); // (its target's chest, as its standing shots')
    self = oldself;
    VR_AG_DrawnFrame(p); // (its gun as drawn on the ragdoll's hand: vr_ag_df_fw)
    vector fw = normalize(vr_ag_df_fw);
    vector m = weaponmuzzle(p);
    if(m == p.origin)
    {
        m = p.origin + fw * 16;
    }
    vector to = normalize(at - m);
    float reach = cvar_hget(cvarh_vr_agrunt_ragfire_aim);
    float aimed = 0;
    if(reach > 0)
    {
        // Its arm reaches along the way to its target turned by what its gun is still off (its wrist's turn in its hand:
        // the gun is not along the forearm), learnt as it goes: the gun's own line brought onto the target.
        vector corr = g.vr_ag_rf_corr + (to - fw) * min(1, VR_AG_RF_CORR_RATE * frametime);
        if(vlen(corr) > VR_AG_RF_CORR_MOST)
        {
            corr = normalize(corr) * VR_AG_RF_CORR_MOST;
        }
        g.vr_ag_rf_corr = corr;
        vector want = normalize(to + corr);
        // (Its wrist turns the gun onto the target too, given the gun's line: vr_agrunt_ragfire_wrist, ragfire2. Measured
        // with it: the learnt turn above off was worse, the arm then aimed straight at the target and the wrist's 30
        // degrees too few for the gun's own turn in the hand.)
        vector barrel = cvar_hget(cvarh_vr_agrunt_ragfire_wrist) ? fw : '0 0 0';
        aimed = ragdollaim(g, m + want * vlen(at - m), VR_AG_RF_AIM_HOLD, p.origin, min(1, reach), barrel);
    }
    float c = fw * to;
    float ang = atan2(vlen(to - fw * c), c) * 57.29578;
    vr_ag_rf_ang = ang;
    vr_ag_rf_ang_at = time;
    if(time < g.vr_ag_rf_next)
    {
        return;
    }
    if(ang > cvar_hget(cvarh_vr_agrunt_ragfire_cone))
    {
        if(cvar_hget(cvarh_vr_agrunt_debug) >= 2)
        {
            VR_AG_Log(sprintf("ragfire: %s pointing %.0f deg off its target (arm reach %s), grip %v muzzle %v fw %v",
                p.netname, ang, aimed ? "on" : "not driven", p.origin, m, fw));
        }
        g.vr_ag_rf_next = time + 0.25; // (looked at again soon: its arm swinging round)
        return; // (pointing away: its arm still swinging round, or it can't)
    }
    vr_ag_rf_asked = vr_ag_rf_asked + 1;
    // Its muzzle clear of the floor and the walls (its gun lying across them), and its line to its target clear.
    traceline(p.origin, m, TRUE, g);
    float blocked = (trace_fraction < 1 || trace_startsolid);
    float steer = max(0, cvar_hget(cvarh_vr_agrunt_ragfire_steer));
    vector dir = to;
    if(ang > steer)
    {
        vector k;
        k_x = fw_y * to_z - fw_z * to_y; // (fw x to: its turn towards the target)
        k_y = fw_z * to_x - fw_x * to_z;
        k_z = fw_x * to_y - fw_y * to_x;
        dir = vlen(k) > 0.001 ? normalize(VR_AG_Rotate(fw, normalize(k), steer)) : fw;
    }
    if(!blocked)
    {
        traceline(m, m + dir * vlen(at - m), FALSE, g);
        blocked = (trace_fraction < 1 && trace_ent != t);
    }
    if(blocked)
    {
        vr_ag_rf_blocked = vr_ag_rf_blocked + 1;
        g.vr_ag_rf_next = time + 0.2; // (looked at again soon: its arm moves)
        return;
    }
    float err = max(0, cvar_hget(cvarh_vr_agrunt_ragfire_error));
    if(err > 0)
    {
        vector a = VR_AG_Angles(dir);
        a_x = -a_x + crandom() * err; // (a view's pitch for makevectors)
        a_y = a_y + crandom() * err;
        makevectors(a);
        dir = v_forward;
    }
    float before = vr_ag_shots_fired;
    vr_ag_rf_dir = dir;
    self = g;
    VR_AG_ShootProp(p);
    self = oldself;
    vr_ag_rf_dir = '0 0 0';
    g.vr_ag_rf_next = time + VR_AG_RagFireInterval(p);
    if(vr_ag_shots_fired > before)
    {
        g.vr_ag_rf_shots = g.vr_ag_rf_shots + 1;
        vr_ag_rf_fired = vr_ag_rf_fired + 1;
        vr_ag_rf_shot_ang = ang;
        VR_AG_Log(sprintf("ragfire: %s fired as a ragdoll (%s), its gun %.1f deg off %s, turned %.1f", p.netname,
            g.vr_kd_stagger ? "staggering" : VR_Knockdown_Held(g) ? "held" : "down", ang, t.classname, min(ang, steer)));
    }
};
