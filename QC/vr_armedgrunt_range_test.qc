#ifdef QVR_TESTS
// vr_armedgrunt_range_test.qc -- armed humanoids' fighting distance (vr_armedgrunt.qc VR_AG_RangeOf, VR_AG_RangeMove,
// VR_AG_WalkFire; vr_agrunt_range; ROUND22.md, "Armed humanoids' fighting distance"). `vr_agrunt_test 48` (Debug >
// Tests > Armed Grunts: Fighting Distance), vrtesthall from its start, the player kept alive: for each weapon one armed
// humanoid (vr_agrunt_test_kind) 260 units ahead hunting you, 6 s of each case: you standing (a shotgun in hand), you
// walking at it (80 u/s, the shotgun), you charging it with an axe (280 u/s, stopping 48 units from it). Each prints its
// distance to you (least, mean), its time within 64 units (your melee reach), its shots and those fired walking, its
// steps backing off: `agtest: range: <weapon> <case>: ...`. Checked (both vr_agrunt_range settings, 0 the before):
// a gun's holder never within 64 of you standing, its mean distance at least its weapon's least (VR_AG_RangeOf) less
// 20%; the axe's still reaches you (unchanged). `scratch1 N` (1..9): weapon N alone; with `scratch2 M`: weapons N..M.

float VR_AGRG_N = 10;
float vr_agrg_saw_d;   // the chainsaw's first thrust: its distance then (0: none yet),
float vr_agrg_saw_dmg; // ... your damage taken before it,
float vr_agrg_saw_hit; // ... its damage to you within its 1 s
float VR_AGRG_SECS = 6;
float vr_agrg_w;      // the weapon (0..)
float vr_agrg_case;   // 0 you standing, 1 walking at it, 2 charging it with an axe, 3 walking at it (120 u/s) with an axe
float vr_agrg_t0;
float vr_agrg_dmin;
float vr_agrg_dsum;
float vr_agrg_dn;
float vr_agrg_inside;
float vr_agrg_shots;
float vr_agrg_walkshots;
float vr_agrg_lastshots;
float vr_agrg_movedat;
float vr_agrg_backs0;
float vr_agrg_yaw0;
float vr_agrg_keepw;
float vr_agrg_keepw2;
vector vr_agrg_lastpos;
vector vr_agrg_start;

float(float i) VR_AGRG_Wid =
{
    if(i == 0) { return WID_SHOTGUN; }
    if(i == 1) { return WID_SUPER_SHOTGUN; }
    if(i == 2) { return WID_NAILGUN; }
    if(i == 3) { return WID_SUPER_NAILGUN; }
    if(i == 4) { return WID_LIGHTNING; }
    if(i == 5) { return WID_ROCKET_LAUNCHER; }
    if(i == 6) { return WID_GRUNTGUN; }
    if(i == 7) { return WID_ENFRIFLE; }
    if(i == 8) { return WID_AXE; }
    return WID_CHAINSAW;
};

string(float c) VR_AGRG_CaseName =
{
    return c == 0 ? "standing" : c == 1 ? "walking at it" : c == 2 ? "charging, axe" : "walking at it, axe";
};

void() VR_AGRG_Frame =
{
    entity p = vr_agt_player;
    entity g = vr_agt_grunt;
    float one = rint(cvar("scratch1"));
    float last = rint(cvar("scratch2")); // (scratch1 N, scratch2 M: weapons N..M)
    if(one < 1)
    {
        last = VR_AGRG_N;
    }
    else if(last < one)
    {
        last = one;
    }
    last = min(last, VR_AGRG_N);
    if(vr_agt_step == 0)
    {
        vr_agrg_start = p.origin;
        vr_agrg_yaw0 = p.angles_y;
        vr_agrg_keepw = p.weapon;
        vr_agrg_keepw2 = p.weapon2;
        vr_agrg_w = (one >= 1 && one <= VR_AGRG_N) ? one - 1 : 0;
        vr_agrg_case = 0;
        VR_AGT_Print(sprintf("range: vr_agrunt_range %g, scale %g, walkfire %g, kind %g", cvar("vr_agrunt_range"),
            cvar("vr_agrunt_range_scale"), cvar("vr_agrunt_range_walkfire"), VR_AGT_Kind()));
        vr_agt_step = 1;
    }
    if(vr_agt_step == 1)
    {
        // The case's start: you back at the start, it 260 ahead hunting you.
        VR_AGPT_Clear();
        setorigin(p, vr_agrg_start);
        p.velocity = '0 0 0';
        p.angles_y = vr_agrg_yaw0;
        p.v_angle_y = vr_agrg_yaw0;
        p.fixangle = TRUE;
        vr_agt_grunt = VR_AGT_Spawn(VR_AGRG_Wid(vr_agrg_w), 260);
        g = vr_agt_grunt;
        VR_AGT_Hunt(g);
        vr_agrg_t0 = time;
        vr_agrg_dmin = 99999;
        vr_agrg_dsum = 0;
        vr_agrg_dn = 0;
        vr_agrg_inside = 0;
        vr_agrg_shots = 0;
        vr_agrg_walkshots = 0;
        vr_agrg_lastshots = g.vr_ag_total_shots;
        vr_agrg_lastpos = g.origin;
        vr_agrg_movedat = -1;
        vr_agrg_backs0 = vr_ag_range_backs;
        vr_agrg_saw_d = 0;
        vr_agrg_saw_hit = 0;
        vr_agt_step = 2;
        return;
    }
    // Running: you as the case has you, it measured.
    p.weapon = vr_agrg_case >= 2 ? WID_AXE : WID_SHOTGUN;
    if(g && g.vr_ag_act == VR_AGP_SAW && !vr_agrg_saw_d)
    {
        vr_agrg_saw_d = vlen(g.origin - p.origin); // (its first thrust begins)
        vr_agrg_saw_dmg = vr_ag_player_dmg;
        vr_agrg_saw_hit = -1;
    }
    if(vr_agrg_saw_hit < 0 && vr_ag_player_dmg > vr_agrg_saw_dmg)
    {
        vr_agrg_saw_hit = 1; // (it reached you)
    }
    p.weapon2 = WID_FIST;
    float alive = g && g.health > 0;
    if(alive)
    {
        vector to = g.origin - p.origin;
        to_z = 0;
        float d = vlen(g.origin - p.origin);
        float sp = vr_agrg_case == 1 ? 80 : vr_agrg_case == 2 ? 280 : vr_agrg_case == 3 ? 120 : 0;
        if((p.flags & FL_ONGROUND) && sp > 0)
        {
            // (kept within 300 of the start: off the hall's walls and their buttons, a new game or the calibration)
            vector v = (vlen(to) > 48 && vlen(p.origin + normalize(to) * 8 - vr_agrg_start) < 300) ? normalize(to) * sp : '0 0 0';
            p.velocity_x = v_x;
            p.velocity_y = v_y;
        }
        if(sp > 0)
        {
            p.angles_y = vectoyaw(to);
            p.v_angle_y = p.angles_y;
            p.fixangle = TRUE;
        }
        vr_agrg_dmin = min(vr_agrg_dmin, d);
        vr_agrg_dsum = vr_agrg_dsum + d;
        vr_agrg_dn = vr_agrg_dn + 1;
        if(d < 64)
        {
            vr_agrg_inside = vr_agrg_inside + frametime;
        }
        if(vlen(g.origin - vr_agrg_lastpos) > 0.2)
        {
            vr_agrg_movedat = time;
        }
        vr_agrg_lastpos = g.origin;
        float ds = g.vr_ag_total_shots - vr_agrg_lastshots;
        if(ds > 0)
        {
            vr_agrg_shots = vr_agrg_shots + ds;
            if(time - vr_agrg_movedat < 0.01)
            {
                vr_agrg_walkshots = vr_agrg_walkshots + ds; // (it moved in the frame it fired)
            }
        }
        vr_agrg_lastshots = g.vr_ag_total_shots;
    }
    if(alive && time - vr_agrg_t0 < VR_AGRG_SECS)
    {
        return;
    }
    float wid = VR_AGRG_Wid(vr_agrg_w);
    float mean = vr_agrg_dsum / max(1, vr_agrg_dn);
    // (sprintf: 8 arguments at most)
    string s = sprintf("range: %s: dist min %.0f mean %.0f, inside 64 %.2f s, shots %g (%g walking), backing steps %g",
        strcat(VR_AGT_Name(wid), " ", VR_AGRG_CaseName(vr_agrg_case)), vr_agrg_dmin, mean, vr_agrg_inside,
        vr_agrg_shots, vr_agrg_walkshots, vr_ag_range_backs - vr_agrg_backs0);
    s = strcat(s, alive ? ", alive, holding " : ", killed, holding ", (g && g.vr_ag_prop) ? g.vr_ag_prop.netname : "nothing", ":");
    if(wid == WID_CHAINSAW && vr_agrg_case == 0)
    {
        // (its first thrust: begun within the saw's reach, and it reaches you)
        s = strcat(s, sprintf(" first thrust at %.0f (reach %g), %s:", vr_agrg_saw_d, cvar("vr_agrunt_saw_start"),
            vr_agrg_saw_hit > 0 ? "it cut you" : "missed"));
        VR_AGT_Check(s, vr_agrg_saw_d > 0 && vr_agrg_saw_d <= cvar("vr_agrunt_saw_start") + 4 && vr_agrg_saw_hit > 0);
    }
    else if(!VR_AG_RangeOf(wid))
    {
        if(vr_agrg_case == 0)
        {
            VR_AGT_Check(s, vr_agrg_dmin < 64); // (a melee weapon: it closes in, as before)
        }
        else
        {
            VR_AGT_Print(s);
        }
    }
    else if(vr_agrg_case == 0)
    {
        VR_AM_Row(VR_AGT_Kind());
        VR_AGT_Check(s, vr_agrg_inside == 0 && mean >= 0.8 * vr_ag_rmin * vr_am_range);
    }
    else if(vr_agrg_case == 3)
    {
        // (you walking at it with an axe: it backs off clearly, still firing now and then)
        VR_AGT_Check(s, vr_agrg_inside == 0 && (vr_agrg_shots > 0 || VR_AG_Class(wid) == VR_AG_CLASS_EXPLOSIVE));
    }
    else
    {
        VR_AGT_Print(s);
    }
    vr_agrg_case = vr_agrg_case + 1;
    vr_agt_step = 1;
    if(vr_agrg_case > 3)
    {
        vr_agrg_case = 0;
        vr_agrg_w = vr_agrg_w + 1;
        if(vr_agrg_w >= last)
        {
            VR_AGPT_Clear();
            p.velocity = '0 0 0';
            setorigin(p, vr_agrg_start);
            p.weapon = vr_agrg_keepw;
            p.weapon2 = vr_agrg_keepw2;
            cvar_set("vr_agrunt_test", "0");
        }
    }
};

// `vr_agrunt_test 49` (Debug > Tests > Armed Grunts: Aim): their shots' miss to the side of you, still, by distance. For
// each gun a grunt 600 units ahead (as far as vrtesthall's start allows) comes at you (vr_agrunt_range 0, no aim error:
// vr_agrunt_aim_error 0), firing; each shot's line measured where it passes you (VR_AG_SetHand: vr_ag_miss_*): to the
// right of you (+) or left, binned by 100 units: as aimed from its grip (the old way) and as fired. Checked: every bin's
// mean as fired within 1.5 units (the old way printed).
float VR_AGAIM_N = 5;
float vr_agaim_w;
float vr_agaim_t0;
float vr_agaim_n0;
float vr_agaim_keep_err;
float vr_agaim_keep_range;
float vr_agaim_raw[10];
float vr_agaim_lat[10];
float vr_agaim_cnt[10];

float(float i) VR_AGAIM_Wid =
{
    if(i == 0) { return WID_SHOTGUN; }
    if(i == 1) { return WID_NAILGUN; }
    if(i == 2) { return WID_SUPER_NAILGUN; }
    if(i == 3) { return WID_GRUNTGUN; }
    return WID_ENFRIFLE;
};

void() VR_AGAIM_Frame =
{
    entity p = vr_agt_player;
    entity g = vr_agt_grunt;
    float i;
    if(vr_agt_step == 0)
    {
        vr_agrg_start = p.origin;
        vr_agrg_yaw0 = p.angles_y;
        vr_agaim_keep_err = cvar("vr_agrunt_aim_error");
        vr_agaim_keep_range = cvar("vr_agrunt_range");
        cvar_set("vr_agrunt_aim_error", "0");
        cvar_set("vr_agrunt_range", "0");
        vr_agaim_w = 0;
        VR_AGT_Print(sprintf("aim: vr_agrunt_aim_converge %g, kind %g", cvar("vr_agrunt_aim_converge"), VR_AGT_Kind()));
        vr_agt_step = 1;
    }
    if(vr_agt_step == 1)
    {
        VR_AGPT_Clear();
        setorigin(p, vr_agrg_start);
        p.velocity = '0 0 0';
        vr_agt_grunt = VR_AGT_Spawn(VR_AGAIM_Wid(vr_agaim_w), 620);
        VR_AGT_Hunt(vr_agt_grunt);
        for(i = 0; i < 10; i++)
        {
            vr_agaim_raw[i] = 0;
            vr_agaim_lat[i] = 0;
            vr_agaim_cnt[i] = 0;
        }
        vr_agaim_t0 = time;
        vr_agaim_n0 = vr_ag_miss_n;
        vr_agt_step = 2;
        return;
    }
    p.velocity_x = 0;
    p.velocity_y = 0;
    if(vr_ag_miss_n != vr_agaim_n0)
    {
        vr_agaim_n0 = vr_ag_miss_n; // (the last shot this frame: one a frame at most, a pellet volley one shot)
        float b = min(9, floor(vr_ag_miss_d / 100));
        vr_agaim_raw[b] = vr_agaim_raw[b] + vr_ag_miss_raw;
        vr_agaim_lat[b] = vr_agaim_lat[b] + vr_ag_miss_lat;
        vr_agaim_cnt[b] = vr_agaim_cnt[b] + 1;
    }
    float d = g ? vlen(g.origin - p.origin) : 0;
    if(g && g.health > 0 && g.vr_ag_prop && d > 90 && time - vr_agaim_t0 < 10)
    {
        return;
    }
    string s = strcat("aim: ", VR_AGT_Name(VR_AGAIM_Wid(vr_agaim_w)), " (bin: shots, right of you aimed from its grip / as fired):");
    float ok = TRUE;
    float any = 0;
    for(i = 0; i < 10; i++)
    {
        if(vr_agaim_cnt[i] > 0)
        {
            float n = vr_agaim_cnt[i];
            s = strcat(s, sprintf(" %g-%g: %g, %+.1f / %+.1f;", i * 100, i * 100 + 100, n, vr_agaim_raw[i] / n,
                vr_agaim_lat[i] / n));
            if(fabs(vr_agaim_lat[i] / n) > 1.5)
            {
                ok = FALSE;
            }
            any = any + n;
        }
    }
    VR_AGT_Check(s, ok && any > 0);
    vr_agaim_w = vr_agaim_w + 1;
    vr_agt_step = 1;
    if(vr_agaim_w >= VR_AGAIM_N)
    {
        VR_AGPT_Clear();
        setorigin(p, vr_agrg_start);
        cvar_set("vr_agrunt_aim_error", ftos(vr_agaim_keep_err));
        cvar_set("vr_agrunt_range", ftos(vr_agaim_keep_range));
        cvar_set("vr_agrunt_test", "0");
    }
};
#endif
