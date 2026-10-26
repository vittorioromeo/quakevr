#ifdef QVR_TESTS
// vr_armedgrunt_range_test.qc -- armed humanoids' fighting distance (vr_armedgrunt.qc VR_AG_RangeOf, VR_AG_RangeMove,
// VR_AG_WalkFire; vr_agrunt_range; ROUND22.md, "Armed humanoids' fighting distance"). `vr_agrunt_test 47` (Debug >
// Tests > Armed Grunts: Fighting Distance), vrtesthall from its start, the player kept alive: for each weapon one armed
// humanoid (vr_agrunt_test_kind) 260 units ahead hunting you, 6 s of each case: you standing (a shotgun in hand), you
// walking at it (80 u/s, the shotgun), you charging it with an axe (280 u/s, stopping 48 units from it). Each prints its
// distance to you (least, mean), its time within 64 units (your melee reach), its shots and those fired walking, its
// steps backing off: `agtest: range: <weapon> <case>: ...`. Checked (both vr_agrunt_range settings, 0 the before):
// a gun's holder never within 64 of you standing, its mean distance at least its weapon's least (VR_AG_RangeOf) less
// 20%; the axe's still reaches you (unchanged). `scratch1 N` (1..9): weapon N alone; with `scratch2 M`: weapons N..M.

float VR_AGRG_N = 9;
float VR_AGRG_SECS = 6;
float vr_agrg_w;      // the weapon (0..)
float vr_agrg_case;   // 0 you standing, 1 walking at it, 2 charging it with an axe
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
    return WID_AXE;
};

string(float c) VR_AGRG_CaseName = { return c == 0 ? "standing" : c == 1 ? "walking at it" : "charging, axe"; };

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
        vr_agt_step = 2;
        return;
    }
    // Running: you as the case has you, it measured.
    p.weapon = vr_agrg_case == 2 ? WID_AXE : WID_SHOTGUN;
    p.weapon2 = WID_FIST;
    float alive = g && g.health > 0;
    if(alive)
    {
        vector to = g.origin - p.origin;
        to_z = 0;
        float d = vlen(g.origin - p.origin);
        float sp = vr_agrg_case == 1 ? 80 : vr_agrg_case == 2 ? 280 : 0;
        if((p.flags & FL_ONGROUND) && sp > 0)
        {
            vector v = vlen(to) > 48 ? normalize(to) * sp : '0 0 0';
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
    if(!VR_AG_RangeOf(wid))
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
    else
    {
        VR_AGT_Print(s);
    }
    vr_agrg_case = vr_agrg_case + 1;
    vr_agt_step = 1;
    if(vr_agrg_case > 2)
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
#endif
