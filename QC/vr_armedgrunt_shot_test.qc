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
    VR_Test_Count(ok);
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
    cvar_set("vr_agrunt_shotdisarm_block", "0"); // (the cases as measured: on into the body; case 14 stopped)
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
        cvar_set("vr_agrunt_shotdisarm_block", "1");
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
    float proj = cvar("vr_agrunt_shotdisarm_proj"); // (each nail its own roll, 0.2 a nail: 3 of them 0.49)
    if(c == 4) { VR_AGST_Check(proj ? "3 nails, each its roll: 0.3..0.7:" : "3 nails: 0.25..0.5:", proj ? (rate >= 0.3 && rate <= 0.7) : (rate >= 0.25 && rate <= 0.5)); }
    if(c == 7) { VR_AGST_Check("lightning 0.2 s: 0.2..0.6:", rate >= 0.2 && rate <= 0.6); }
    if(c == 5 && proj) { VR_AGST_Check("a single far nail, its own roll: 0.1..0.35:", rate >= 0.1 && rate <= 0.35); }
    else if(c == 5 || c == 6) { VR_AGST_Check("a single far shot: 0.05 at most:", rate <= 0.05); }
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
            vr_agst_keep_absorb = cvar("vr_agrunt_shotdisarm_block");
            vr_agst_keep_disarm = cvar("vr_agrunt_fall_drop");
            cvar_set("vr_agrunt_fall_drop", "0"); // (only its shots: not a knockdown's drop)
            VR_Test_Nest("agshot", VR_TEST_LOUD);
        }
        if(!VR_AGST_Setup(vr_agst_case))
        {
            cvar_set("vr_agrunt_fall_drop", ftos(vr_agst_keep_disarm));
        cvar_set("vr_agrunt_shotdisarm_block", ftos(vr_agst_keep_absorb));
            VR_Test_VerdictPart("done:");
            VR_Test_Unnest();
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
            vr_agst_until = time + 0.1 + (vr_agst_kind == 2 ? vr_agst_dist / VR_NailSpeed() : 0); // (a nail's flight; a roll's frame)
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

// ----------------------------------------------------------------------------
// Test 93: projectiles' own rolls (vr_agrunt_shotdisarm_proj). A shotgun grunt 520 units ahead; nails, super nails, the
// nail pistol's nails, lava nails and rockets flown at its gun, its forearm, and 3 and 6 units off the gun's surface
// (below it, away from its arm): each projectile rolls once. Each case's mean chance must be the formula's (exact: the
// roll's chance is deterministic) and its rate near it; the pistol's above 0, 6 units off never.
// ----------------------------------------------------------------------------

float vr_agpj_case;    // the case (kind * 4 + where)
float vr_agpj_shot;    // ... its shots so far
float vr_agpj_n;       // ... shots it takes
float vr_agpj_step;    // 0 the case's set-up, 1 a grunt, 2 a shot, 3 its result
float vr_agpj_until;
float vr_agpj_rolls0;  // vr_ags_rolls at the shot
float vr_agpj_dis0;    // vr_ags_disarms at the shot
float vr_agpj_rolled;  // the case's rolls,
float vr_agpj_dis;     // ... disarms,
float vr_agpj_csum;    // ... chances summed
float vr_agpj_keep_proj;
float vr_agpj_keep_drop;
float vr_agpj_keep_block;
float vr_agpj_keep_parts;
entity vr_agpj_g;

string(float k) VR_AGPJ_KindName =
{
    return k == 0 ? "nail" : k == 1 ? "super nail" : k == 2 ? "pistol nail" : k == 3 ? "lava nail" : "rocket";
};

string(float w) VR_AGPJ_WhereName = { return w == 0 ? "on the gun" : w == 1 ? "on the forearm" : w == 2 ? "3 off" : "6 off"; };

// The chance the formula gives (vr_agrunt_shotdisarm_proj_*): `dmg` damage, falloff `f`, part weight 1.
float(float dmg, float f) VR_AGPJ_Expect =
{
    float c = cvar("vr_agrunt_shotdisarm_proj_chance") * dmg / max(0.01, cvar("vr_agrunt_shotdisarm_proj_ref"));
    return min(cvar("vr_agrunt_shotdisarm_proj_max"), c) * f;
};

float(float k) VR_AGPJ_Damage =
{
    if(k == 1) { return VR_WpnDmg(vr_agt_player, cvarh_vr_dmg_super_nail, 18); }
    if(k == 2) { return VR_WpnDmg(vr_agt_player, cvarh_vr_dmg_nailpistol, 9); }
    if(k == 4) { return 110 * VR_WpnDmgScale(vr_agt_player, cvarh_vr_dmg_rocket, 100); }
    return VR_WpnDmg(vr_agt_player, cvarh_vr_dmg_nail, 9);
};

// Where the shot goes (`w`); it comes from the player's side, 256 units back along the way (a rocket 64).
vector(entity g, entity p, float w) VR_AGPJ_AimAt =
{
    if(w == 1)
    {
        VR_AGS_HandLine(g, p);
        return vr_ags_ha + (vr_ags_hb - vr_ags_ha) * 0.7;
    }
    VR_AG_WeaponLine(p);
    // (the muzzle's half: away from its forearm; a rocket's by the muzzle, out of its body's box: it bursts there)
    vector at = vr_ag_wa + (vr_ag_wb - vr_ag_wa) * (vr_agpj_case >= 16 ? 0.95 : 0.75);
    if(w >= 2)
    {
        vector eye = vr_agt_player.origin + vr_agt_player.view_ofs;
        vector dir = normalize(at - eye);
        vector off = normalize(VR_Cross(dir, vr_ag_wb - vr_ag_wa));
        if(off_z > 0) { off = off * -1; }
        at = at + off * (VR_AGS_W_RADIUS + (w == 2 ? 3 : 6));
    }
    return at;
};

void(entity g, entity p, float k, float w) VR_AGPJ_Fire =
{
    entity pl = vr_agt_player;
    vector at = VR_AGPJ_AimAt(g, p, w);
    vector dir = normalize(at - (pl.origin + pl.view_ofs));
    vector from = at - dir * (k == 4 ? 64 : 256); // (a rocket's slow flight: from near, its target not moved meanwhile)
    entity oldself = self;
    self = pl;
    entity e;
    if(k == 4)
    {
        e = spawn();
        e.owner = pl;
        e.movetype = MOVETYPE_FLYMISSILE;
        e.solid = SOLID_BBOX;
        e.classname = "missile";
        e.touch = T_MissileTouch;
        e.angles = vectoangles(dir);
        setmodel(e, "progs/missile.mdl");
        setsize(e, '0 0 0', '0 0 0');
        setorigin(e, from);
        e.velocity = dir * 1000;
        e.nextthink = time + 5;
        e.think = SUB_Remove;
    }
    else
    {
        e = launch_spike(from, dir);
        if(k == 1) { e.touch = superspike_touch; }
        if(k == 2) { e.touch = VR_NailPistol_SpikeTouch; }
        if(k == 3) { e.touch = lavaspike_touch; }
    }
    self = oldself;
};

// The case's results printed and checked.
void() VR_AGPJ_Report =
{
    float k = floor(vr_agpj_case / 4);
    float w = vr_agpj_case - k * 4;
    float n = max(1, vr_agpj_n);
    float f = w == 2 ? max(0, 1 - 3 / max(0.01, cvar("vr_agrunt_shotdisarm_proj_near"))) : (w == 3 ? 0 : 1);
    f = w == 3 ? max(0, 1 - 6 / max(0.01, cvar("vr_agrunt_shotdisarm_proj_near"))) : f;
    float want = VR_AGPJ_Expect(VR_AGPJ_Damage(k), f);
    float mean = vr_agpj_csum / max(1, vr_agpj_rolled);
    float rate = vr_agpj_dis / n;
    string what = sprintf("%s %s: %g/%g disarmed (%.2f), rolls %g, chance %.3f", VR_AGPJ_KindName(k),
        VR_AGPJ_WhereName(w), vr_agpj_dis, n, rate, vr_agpj_rolled, mean);
    // (every shot but one rolls; their chance the formula's (near misses: within 0.03, the gun swaying); the rate within
    // 3 standard deviations of it, at least 0.12)
    float sd = sqrt(want * (1 - want) / n);
    float ok = ((vr_agpj_rolled >= n - 1) && (fabs(mean - want) <= (w == 2 ? 0.03 : 0.01)) && (fabs(rate - want) <= max(0.12, 3 * sd)));
    if(w == 3) { ok = ((vr_agpj_dis == 0) && (vr_agpj_rolled == 0)); }
    if(k == 2 && w == 0) { ok = (ok && (rate > 0)); }
    VR_AGST_Check(sprintf("%s, want %.3f:", what, want), ok);
};

void() VR_AGS_ProjTestFrame =
{
    entity pl = vr_agt_player;
    entity g = vr_agpj_g;
    pl.flags = pl.flags | FL_NOTARGET;
    if(vr_agpj_step == 0)
    {
        if(vr_agpj_case == 0)
        {
            vr_agpj_keep_proj = cvar("vr_agrunt_shotdisarm_proj");
            vr_agpj_keep_drop = cvar("vr_agrunt_fall_drop");
            vr_agpj_keep_block = cvar("vr_agrunt_shotdisarm_block");
            vr_agpj_keep_parts = cvar("vr_agrunt_shotdisarm_parts");
            cvar_set("vr_agrunt_shotdisarm_proj", "1");
            cvar_set("vr_agrunt_fall_drop", "0");
            cvar_set("vr_agrunt_shotdisarm_block", "0"); // (each nail on into the body: its roll only)
            cvar_set("vr_agrunt_shotdisarm_parts", "2");
            VR_Test_Nest("agshot", VR_TEST_LOUD);
        }
        if(vr_agpj_case >= 20)
        {
            cvar_set("vr_agrunt_shotdisarm_proj", ftos(vr_agpj_keep_proj));
            cvar_set("vr_agrunt_fall_drop", ftos(vr_agpj_keep_drop));
            cvar_set("vr_agrunt_shotdisarm_block", ftos(vr_agpj_keep_block));
            cvar_set("vr_agrunt_shotdisarm_parts", ftos(vr_agpj_keep_parts));
            VR_Test_VerdictPart("projectiles done:");
            VR_Test_Unnest();
            pl.flags = pl.flags - (pl.flags & FL_NOTARGET);
            VR_AGPT_Clear();
            vr_agpj_case = 0;
            cvar_set("vr_agrunt_test", "0");
            return;
        }
        float w0 = vr_agpj_case - floor(vr_agpj_case / 4) * 4;
        vr_agpj_n = cvar("scratch1") > 0 ? cvar("scratch1") : (w0 == 0 ? 40 : 12);
        vr_agpj_shot = 0;
        vr_agpj_rolled = 0;
        vr_agpj_dis = 0;
        vr_agpj_csum = 0;
        vr_agpj_step = 1;
        return;
    }
    float k = floor(vr_agpj_case / 4);
    float w = vr_agpj_case - k * 4;
    if(vr_agpj_step == 1)
    {
        VR_AGPT_Clear();
        vr_agpj_g = VR_AGT_Spawn(WID_SHOTGUN, 520);
        vr_agpj_g.health = 5000;
        vr_agpj_step = 2;
        vr_agpj_until = time + 0.2;
        return;
    }
    if(time < vr_agpj_until)
    {
        return;
    }
    if(vr_agpj_step == 2)
    {
        if(!g.vr_ag_prop)
        {
            vr_agpj_step = 1; // (gone: a fresh one)
            return;
        }
        vr_agpj_rolls0 = vr_ags_rolls;
        vr_agpj_dis0 = vr_ags_disarms;
        vr_ags_last_chance = 0;
        VR_AGPJ_Fire(g, g.vr_ag_prop, k, w);
        vr_agpj_step = 3;
        vr_agpj_until = time + 0.2; // (its flight: a nail's 256 units, a rocket's 64 at 1000 u/s)
        return;
    }
    // The shot's result: a roll, its chance, a disarm.
    if(vr_ags_rolls > vr_agpj_rolls0)
    {
        vr_agpj_rolled = vr_agpj_rolled + 1;
        vr_agpj_csum = vr_agpj_csum + vr_ags_last_chance;
    }
    float won = vr_ags_disarms > vr_agpj_dis0;
    vr_agpj_dis = vr_agpj_dis + won;
    vr_agpj_shot = vr_agpj_shot + 1;
    vr_agpj_step = (won || k == 4 || g.health < 4000 || !g.vr_ag_prop) ? 1 : 2; // (a fresh grunt: disarmed, blasted)
    if(vr_agpj_shot >= vr_agpj_n)
    {
        VR_AGPJ_Report();
        vr_agpj_case = vr_agpj_case + 1;
        vr_agpj_step = 0;
    }
};
#endif
