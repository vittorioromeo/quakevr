#ifdef QVR_TESTS
// vr_revthrow_test.qc -- `vr_embed_test 73` (revthrow, his words 2026-11-26 20:11: "It should be a bit easier for spears to
// stabilize when they're thrown overhead with a reverse grip pose ... even if I throw it with a bit of wrist snap, then it should
// almost immediately stabilize with its point forward"; the engine's pointAlign, vr_box3d_step.cpp: vr_throw_point_rev_pole and
// vr_throw_point_rev_blade). Map vrtesthall, god, notarget. Each throw leaves as a reverse grip's overhead throw does: from ahead
// of your eyes at the wall VR_RT_WALL units off, a little upwards, its point VR_RT_UP degrees above its flight and the wrist
// snap's tumble turning it down across the blade (QVR_WPNFLAG_REVGRIP on it, as DropWeaponInHandScaled leaves it); the spear
// 10..18 m/s at 8..20 rad/s, the knight's sword and the dagger at 14..22 rad/s. Its blade's angle to its flight read
// VR_RT_SOON s after the throw, and the most from then until it first touches anything. Three passes over the same throws:
//   0 reversed at the settings: every spear point first within VR_RT_SOON s, staying so, stuck in the wall by its point;
//   1 reversed, both leniencies 0 (as before revthrow): the spears and the blades less often or less nearly point first;
//   2 upright (no reverse grip flag) at the settings: every throw exactly as pass 1's (an upright throw unchanged).
// Ends `revthrow: N passed, M failed`.

float VR_RT_WALL = 300;  // units from the wall
float VR_RT_SOON = 0.15; // s: point first by then (his "almost immediately")
float VR_RT_UP = 10;     // degrees: its point above its flight as it leaves the hand
float VR_RT_SPEARS = 4;
float VR_RT_THROWS = 10; // the spears, then three swords and three daggers
float VR_RT_PASSES = 3;
float vr_rt_i;           // the throw now
float vr_rt_on;          // the pass
float vr_rt_keepp;       // vr_throw_point_rev_pole as it was
float vr_rt_keepb;       // vr_throw_point_rev_blade as it was
float vr_rt_soon;        // the throw now's angle at VR_RT_SOON (-1 not yet)
float vr_rt_most;        // and its most since
float vr_rt_ang[30];     // [pass * 10 + throw]: its angle at VR_RT_SOON
float vr_rt_max[30];     // ... its most from then on
float vr_rt_stuck[30];   // ... stuck by its point (1), else 0

// The spears' leniency pass `xPass` throws with (pass 1: 0).
float(float xPass) VR_RT_PoleFor =
{
    return (xPass == 1) ? 0 : vr_rt_keepp;
};

// The throw's settings for pass `xPass` (pass 1: both leniencies 0; a tunable takes the console's `tune`, a frame later: the
// throw waits for it, VR_RT_Frame).
void(float xPass) VR_RT_Tune =
{
    localcmd(sprintf("tune vr_throw_point_rev_pole %g\ntune vr_throw_point_rev_blade %g\n", VR_RT_PoleFor(xPass),
        (xPass == 1) ? 0 : vr_rt_keepb));
};

void() VR_RT_Begin =
{
    VR_Test_BeginFlags("revthrow", VR_TEST_LOUD);
    vr_eh_home = VR_Player().origin;
    vr_rt_keepp = cvar_hget(cvarh_vr_throw_point_rev_pole);
    vr_rt_keepb = cvar_hget(cvarh_vr_throw_point_rev_blade);
    vr_rt_i = 0;
    vr_rt_on = 0;
    VR_RT_Tune(0);
    vr_et_on = 73;
    vr_et_case = 0;
    VR_ET_To(0);
};

// Degrees between `a` and `b`.
float(vector a, vector b) VR_RT_Angle =
{
    vector c;
    c_x = a_y * b_z - a_z * b_y;
    c_y = a_z * b_x - a_x * b_z;
    c_z = a_x * b_y - a_y * b_x;
    return atan2(vlen(c), a * b) * (180 / 3.14159265);
};

// Its line, handle to tip (world).
vector(entity w) VR_RT_Line =
{
    return normalize(weaponmuzzle(w) - w.origin);
};

// What throw `i` is: 0 the spear, 1 the knight's sword, 2 the dagger.
float(float i) VR_RT_Kind =
{
    if(i < VR_RT_SPEARS)
    {
        return 0;
    }
    return (i < VR_RT_SPEARS + 3) ? 1 : 2;
};

void() VR_RT_Throw =
{
    entity p = self;
    float i = vr_rt_i;
    float kind = VR_RT_Kind(i);
    setorigin(p, vr_eh_home);
    p.velocity = '0 0 0';
    VR_ET_FaceWall(VR_RT_WALL);
    makevectors('0 1 0' * p.v_viewangle_y);
    vector go = normalize(v_forward + '0 0 0.05');
    vector side = v_right;
    vector line = VR_AxeStick_Turn(go, side * -1, VR_RT_UP); // (its point up: about the left)
    float rate;
    float speed;
    if(kind == 0)
    {
        rate = 8 + 4 * i;     // 8, 12, 16, 20 rad/s
        speed = 10 + 2 * i;   // 10 .. 16 m/s (his: 9.5, 12)
    }
    else
    {
        float j = (i - VR_RT_SPEARS) - (kind == 2 ? 3 : 0);
        rate = 14 + 4 * j;    // 14, 18, 22 rad/s
        speed = 12;
    }
    float flags = kind == 0 ? VR_ADMelee_Flags(VR_SWORDKIND_SPEAR) : (kind == 2 ? VR_ADMelee_Flags(VR_SWORDKIND_DAGGER) : 0);
    vr_et_flags = flags | (vr_rt_on == 2 ? 0 : QVR_WPNFLAG_REVGRIP);
    vr_et_w = VR_ET_ThrowSword(WID_SWORD, p.origin + '0 0 22' + go * 96, go, line, FALSE);
    vr_et_flags = 0;
    if(vr_et_w != world)
    {
        vr_et_w.velocity = go * (speed * VRMetersToUnits());
        vr_et_w.vr_spin = side * rate; // (about the right: its point turning down, a wrist snap's)
    }
    vr_rt_soon = -1;
    vr_rt_most = 0;
};

// The throws' sums for `xPass` over `xKind` (-1 the blades both): [point first at VR_RT_SOON and after, angle at it summed].
float vr_rt_sumok;
float vr_rt_sumang;
float vr_rt_sumstuck;
void(float xPass, float xKind) VR_RT_Sum =
{
    vr_rt_sumok = vr_rt_sumang = vr_rt_sumstuck = 0;
    float most = cvar_hget(cvarh_vr_embed_sword_angle);
    for(float i = 0; i < VR_RT_THROWS; i++)
    {
        float k = VR_RT_Kind(i);
        if(k == xKind || (xKind < 0 && k > 0))
        {
            float slot = xPass * 10 + i;
            vr_rt_sumok = vr_rt_sumok + (vr_rt_ang[slot] >= 0 && vr_rt_max[slot] <= most);
            vr_rt_sumang = vr_rt_sumang + (vr_rt_ang[slot] >= 0 ? vr_rt_ang[slot] : 180);
            vr_rt_sumstuck = vr_rt_sumstuck + vr_rt_stuck[slot];
        }
    }
};

void() VR_RT_Report =
{
    VR_RT_Sum(0, 0);
    float spOk = vr_rt_sumok, spAng = vr_rt_sumang, spStuck = vr_rt_sumstuck;
    VR_RT_Sum(1, 0);
    float spOk1 = vr_rt_sumok, spAng1 = vr_rt_sumang;
    VR_Test_Print(sprintf("revthrow: spears reversed: %g point first (mean %.1f deg at %.0f ms); leniency 0: %g (%.1f deg)\n", spOk,
        spAng / VR_RT_SPEARS, VR_RT_SOON * 1000, spOk1, spAng1 / VR_RT_SPEARS));
    VR_Test_Check(spOk == VR_RT_SPEARS, sprintf("reversed, every spear point first within %.0f ms and staying so (%g of %g; mean %.1f deg at %.0f ms)",
        VR_RT_SOON * 1000, spOk, VR_RT_SPEARS, spAng / VR_RT_SPEARS, VR_RT_SOON * 1000));
    VR_Test_Check(spStuck == VR_RT_SPEARS, sprintf("... every one stuck in the wall by its point (%g of %g)", spStuck, VR_RT_SPEARS));
    VR_Test_Check(spAng < spAng1, sprintf("... nearer point first than with Reverse Grip Settling: Spears 0 (mean %.1f deg against %.1f; %g point first there)",
        spAng / VR_RT_SPEARS, spAng1 / VR_RT_SPEARS, spOk1));
    for(float b = 1; b <= 2; b++)
    {
        VR_RT_Sum(0, b);
        float ok = vr_rt_sumok, ang = vr_rt_sumang;
        VR_RT_Sum(1, b);
        string what = b == 1 ? "the knight's sword" : "the dagger";
        VR_Test_Check(ok >= vr_rt_sumok && ang < vr_rt_sumang, sprintf("reversed, %s steadier with Reverse Grip Settling: Blades (%g of 3 point first, mean %.1f deg; at 0: %g, %.1f)",
            what, ok, ang / 3, vr_rt_sumok, vr_rt_sumang / 3));
    }
    float same = 0;
    float worst = 0;
    for(float i = 0; i < VR_RT_THROWS; i++)
    {
        float d = fabs(vr_rt_ang[20 + i] - vr_rt_ang[10 + i]);
        worst = max(worst, d);
        same = same + (d <= 0.5);
    }
    VR_Test_Check(same == VR_RT_THROWS, sprintf("upright, every throw as a reversed one with both leniencies 0 (%g of %g within 0.5 deg; worst %.2f): upright throws unchanged",
        same, VR_RT_THROWS, worst));
};

void() VR_RT_Frame =
{
    float t = time - vr_et_t0;
    if(vr_et_s == 0)
    {
        if(t > 2)
        {
            VR_Test_Check(FALSE, "timed out: the pass's tune never took");
            VR_Test_Verdict();
            vr_et_on = 0;
            return;
        }
        if(fabs(cvar_hget(cvarh_vr_throw_point_rev_pole) - VR_RT_PoleFor(vr_rt_on)) > 0.001)
        {
            return; // (the pass's tune not taken yet)
        }
        VR_RT_Throw();
        if(vr_et_w == world)
        {
            VR_Test_Check(FALSE, "the throw made no weapon (the pack not running?)");
            VR_Test_Verdict();
            vr_et_on = 0;
            return;
        }
        VR_ET_To(1);
        return;
    }
    if(vr_et_s != 1)
    {
        return;
    }
    entity w = vr_et_w;
    float stuck = w.vr_stick_kind != 0;
    float touched = stuck || w.throwhit == QVR_THROWHIT_HIT || vlen(w.velocity) < 3 * VRMetersToUnits();
    if(!touched && t >= VR_RT_SOON)
    {
        float ang = VR_RT_Angle(VR_RT_Line(w), normalize(w.velocity));
        if(vr_rt_soon < 0)
        {
            vr_rt_soon = ang;
        }
        vr_rt_most = max(vr_rt_most, ang);
    }
    if(touched || t > 1.5)
    {
        float slot = vr_rt_on * 10 + vr_rt_i;
        vr_rt_ang[slot] = vr_rt_soon;
        vr_rt_max[slot] = vr_rt_soon < 0 ? 180 : vr_rt_most;
        vr_rt_stuck[slot] = 0;
        if(stuck && w.vr_stick_embed == AXESTICK_POINT)
        {
            vr_rt_stuck[slot] = 1;
        }
        if(cvar_hget(cvarh_developer))
        {
            string how = stuck ? (w.vr_stick_embed == AXESTICK_POINT ? "stuck by its point" : "stuck") : (t > 1.5 ? "timed out" : "bounced");
            dprint(sprintf("revthrow: pass %g throw %g (kind %g): %.1f deg at 150 ms, at most %.1f after; %s (%.2f s)\n", vr_rt_on, vr_rt_i,
                VR_RT_Kind(vr_rt_i), vr_rt_soon, vr_rt_most, how, t));
        }
        VR_ET_Clean();
        vr_rt_i = vr_rt_i + 1;
        if(vr_rt_i >= VR_RT_THROWS)
        {
            vr_rt_i = 0;
            vr_rt_on = vr_rt_on + 1;
            VR_RT_Tune(vr_rt_on);
        }
        if(vr_rt_on >= VR_RT_PASSES)
        {
            VR_RT_Tune(0);
            setorigin(self, vr_eh_home);
            VR_RT_Report();
            VR_Test_Verdict();
            vr_et_on = 0;
            return;
        }
        VR_ET_To(0);
    }
};
#endif
