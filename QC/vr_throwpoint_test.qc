#ifdef QVR_TESTS
// vr_throwpoint_test.qc -- `vr_embed_test 47` (gibthrow): a thrown sword or dagger steadies towards its point in the air
// (his note 2026-11-15 00:03: "if the throw is good enough there should be some force that stabilizes it toward the point ... not
// instantly turning straight like an arrow"; the engine's pointAlign, vr_box3d_step.cpp, Point-First Settling). In the open
// (vrtesthall, god, notarget). Each throw leaves at VR_TP_SPEED a little upwards ahead of you, its point some degrees off its
// flight and tumbling a little or a lot; its blade's angle to its flight read once it is VR_TP_DIST m out (or as it first
// touches anything). Every set is thrown twice, the settling off (vr_throw_point_align 0) and at its setting:
//   good  12 throws, swords and daggers, the point 10..40 degrees off, tumbling 1..5 rad/s: point first (within
//         vr_embed_sword_angle, what sticks a point) more often with it, and not every one at once (no arrow);
//   bad   6 throws, the point 70..180 degrees off or tumbling 12..20 rad/s: as many point first as without it, the tumble kept;
//   axes  4 axe throws tumbling: the same flight with it as without (only pointed blades).
// Ends `gibthrow: N passed, M failed`.

float VR_TP_SPEED = 10;  // m/s
float VR_TP_DIST = 3;    // m: where the angle is read (a throw at someone a few steps off)
float VR_TP_GOOD = 12;
float VR_TP_BAD = 6;
float VR_TP_AXES = 4;
float vr_tp_i;           // the throw now, over every set: good, bad, axes
float vr_tp_on;          // the pass: 0 the settling off, 1 at its setting
float vr_tp_keep;        // vr_throw_point_align as it was
vector vr_tp_from;       // where the throw now left from
float vr_tp_hit[6];      // point first at the read: [set * 2 + pass] counts
float vr_tp_angsum[6];   // its angles summed (degrees)
float vr_tp_tumble[4];   // the bad set's tumble across the blade at the read summed (rad/s), [pass]; the axes' spin [2 + pass]
float vr_tp_axeang[8];   // the axes: the blade's way at the read (x of the line) per throw and pass, to compare
float vr_tp_early;       // throws that touched something before the read

void() VR_TP_Begin =
{
    VR_Test_BeginFlags("gibthrow", VR_TEST_LOUD);
    vr_eh_home = VR_Player().origin;
    vr_tp_keep = cvar_hget(cvarh_vr_throw_point_align);
    vr_tp_i = 0;
    vr_tp_on = 0;
    vr_tp_early = 0;
    for(float k = 0; k < 6; k++)
    {
        vr_tp_hit[k] = 0;
        vr_tp_angsum[k] = 0;
    }
    for(float k2 = 0; k2 < 4; k2++)
    {
        vr_tp_tumble[k2] = 0;
    }
    localcmd("tune vr_throw_point_align 0\n");
    vr_et_on = 47;
    vr_et_case = 0;
    VR_ET_To(0);
};

// The set of throw `i`: 0 good, 1 bad, 2 axes.
float(float i) VR_TP_Set =
{
    return (i < VR_TP_GOOD) ? 0 : ((i < VR_TP_GOOD + VR_TP_BAD) ? 1 : 2);
};

// Degrees between `a` and `b`.
float(vector a, vector b) VR_TP_Angle =
{
    vector c;
    c_x = a_y * b_z - a_z * b_y;
    c_y = a_z * b_x - a_x * b_z;
    c_z = a_x * b_y - a_y * b_x;
    return atan2(vlen(c), a * b) * (180 / 3.14159265);
};

// Its line, handle to tip (world).
vector(entity w) VR_TP_Line =
{
    return normalize(weaponmuzzle(w) - w.origin);
};

void() VR_TP_Throw =
{
    entity p = self;
    float i = vr_tp_i;
    float set = VR_TP_Set(i);
    makevectors([0, p.v_angle_y, 0]);
    vector go = normalize(v_forward + '0 0 0.15');
    vector side = v_right;
    vector up = normalize( // (square to both)
        [side_y * go_z - side_z * go_y, side_z * go_x - side_x * go_z, side_x * go_y - side_y * go_x]);
    // Deterministic spreads: the point's way off (about an axis turning round the flight), the tumble's axis and rate.
    float u = (i * 0.61803) - floor(i * 0.61803);
    float turnAbout = u * 360;
    vector k = side * cos(turnAbout * 0.01745329) + up * sin(turnAbout * 0.01745329);
    float offDeg;
    float rate;
    if(set == 0)
    {
        offDeg = 10 + 30 * ((i * 0.381966) - floor(i * 0.381966));
        rate = 1 + 4 * ((i * 0.7548) - floor(i * 0.7548));
    }
    else if(set == 1)
    {
        float j = i - VR_TP_GOOD;
        offDeg = (j < 3) ? (70 + 40 * j) : (15 + 5 * j); // (far off; or near, but tumbling hard)
        rate = (j < 3) ? 6 : (12 + 3 * (j - 3));
    }
    else
    {
        offDeg = 0;
        rate = 10 + 2 * (i - VR_TP_GOOD - VR_TP_BAD);
    }
    vr_tp_from = p.origin + '0 0 22' + go * 40;
    float speed = VR_TP_SPEED * VRMetersToUnits();
    if(set < 2)
    {
        vr_et_flags = (i & 1) ? VR_ADMelee_Flags(VR_SWORDKIND_DAGGER) : 0;
        vr_et_w = VR_ET_ThrowSword(WID_SWORD, vr_tp_from, go, VR_AxeStick_Turn(go, k, offDeg), FALSE);
        vr_et_flags = 0;
        vector line = VR_TP_Line(vr_et_w);
        // Tumbling across the blade: about an axis square to its line, between the flight's side and its up.
        vector across = normalize(k - line * (k * line));
        vr_et_w.vr_spin = across * rate;
    }
    else
    {
        vr_et_w = VR_ET_ThrowAxe(vr_tp_from, go, FALSE);
        vr_et_w.vr_spin = side * rate;
    }
    vr_et_w.velocity = go * speed;
};

void() VR_TP_Frame =
{
    entity p = self;
    float t = time - vr_et_t0;
    float total = VR_TP_GOOD + VR_TP_BAD + VR_TP_AXES;
    if(vr_et_s == 0)
    {
        setorigin(p, vr_eh_home);
        p.velocity = '0 0 0';
        VR_ET_To(1);
    }
    else if(vr_et_s == 1 && t > 0.05) // (its tune taken)
    {
        VR_TP_Throw();
        VR_ET_To(2);
    }
    else if(vr_et_s == 2)
    {
        entity w = vr_et_w;
        float out = (vlen(w.origin - vr_tp_from) >= VR_TP_DIST * VRMetersToUnits());
        float touched = ((w.throwhit == QVR_THROWHIT_HIT) || (vlen(w.velocity) < 0.5 * VR_TP_SPEED * VRMetersToUnits()));
        if(out || touched || (t > 1.5))
        {
            float set = VR_TP_Set(vr_tp_i);
            vector line = VR_TP_Line(w);
            float ang = VR_TP_Angle(line, normalize(w.velocity));
            if(!out)
            {
                vr_tp_early = vr_tp_early + 1;
            }
            float slot = set * 2 + vr_tp_on;
            if(ang <= cvar_hget(cvarh_vr_embed_sword_angle))
            {
                vr_tp_hit[slot] = vr_tp_hit[slot] + 1;
            }
            vr_tp_angsum[slot] = vr_tp_angsum[slot] + ang;
            vector sp = w.vr_spin;
            if(set == 1)
            {
                vr_tp_tumble[vr_tp_on] = vr_tp_tumble[vr_tp_on] + vlen(sp - line * (sp * line));
            }
            else if(set == 2)
            {
                vr_tp_tumble[2 + vr_tp_on] = vr_tp_tumble[2 + vr_tp_on] + vlen(sp);
                vr_tp_axeang[(vr_tp_i - VR_TP_GOOD - VR_TP_BAD) * 2 + vr_tp_on] = line_x + line_y * 10 + line_z * 100;
            }
            if(cvar_hget(cvarh_developer))
            {
                dprint(sprintf("gibthrow: throw %g (set %g, pass %g): %.1f degrees off its flight %s\n", vr_tp_i, set, vr_tp_on, ang,
                    out ? "in the air" : "(touched first)"));
            }
            remove(w);
            vr_et_w = world;
            vr_tp_i = vr_tp_i + 1;
            if(vr_tp_i >= total)
            {
                vr_tp_i = 0;
                vr_tp_on = vr_tp_on + 1;
                localcmd(sprintf("tune vr_throw_point_align %g\n", (vr_tp_keep > 0) ? vr_tp_keep : 1));
            }
            VR_ET_To((vr_tp_on >= 2) ? 3 : 0);
        }
    }
    else if(vr_et_s == 3)
    {
        localcmd(sprintf("tune vr_throw_point_align %g\n", vr_tp_keep));
        float a = cvar_hget(cvarh_vr_embed_sword_angle);
        VR_Test_Print(sprintf("%s: good throws point first (within %g degrees) at %g m: %g of %g without, %g with",
            vr_test_tag, a, VR_TP_DIST, vr_tp_hit[0], VR_TP_GOOD, vr_tp_hit[1]));
        VR_Test_Print(sprintf(" (mean %.0f -> %.0f degrees off)\n", vr_tp_angsum[0] / VR_TP_GOOD, vr_tp_angsum[1] / VR_TP_GOOD));
        VR_Test_Print(sprintf("%s: bad throws point first: %g of %g without, %g with (tumble across the blade %.1f -> %.1f rad/s)",
            vr_test_tag, vr_tp_hit[2], VR_TP_BAD, vr_tp_hit[3], vr_tp_tumble[0] / VR_TP_BAD, vr_tp_tumble[1] / VR_TP_BAD));
        VR_Test_Print(sprintf("; axes' spin %.2f -> %.2f rad/s; %g touched first\n", vr_tp_tumble[2] / VR_TP_AXES, vr_tp_tumble[3] / VR_TP_AXES, vr_tp_early));
        VR_Test_Check(vr_tp_early == 0, sprintf("every throw read in the air at %g m (%g touched something first)", VR_TP_DIST, vr_tp_early));
        VR_Test_Check(vr_tp_hit[1] >= vr_tp_hit[0] + 3, sprintf("good throws settle point first more often (%g of %g, %g without)", vr_tp_hit[1], VR_TP_GOOD, vr_tp_hit[0]));
        VR_Test_Check(vr_tp_hit[1] < VR_TP_GOOD, sprintf("not every good throw: no arrow (%g of %g)", vr_tp_hit[1], VR_TP_GOOD));
        VR_Test_Check((vr_tp_hit[3] <= vr_tp_hit[2]) && (vr_tp_tumble[1] >= vr_tp_tumble[0] * 0.85),
            sprintf("bad throws stay a tumble (%g point first, %g without; tumble %.1f of %.1f rad/s)", vr_tp_hit[3], vr_tp_hit[2], vr_tp_tumble[1] / VR_TP_BAD, vr_tp_tumble[0] / VR_TP_BAD));
        float same = TRUE;
        for(float n = 0; n < VR_TP_AXES; n++)
        {
            same = (same && (fabs(vr_tp_axeang[n * 2] - vr_tp_axeang[n * 2 + 1]) < 0.001));
        }
        VR_Test_Check(same && (fabs(vr_tp_tumble[2] - vr_tp_tumble[3]) < 0.001), sprintf("axes fly the same with it (spin %.3f, %.3f rad/s)", vr_tp_tumble[2] / VR_TP_AXES, vr_tp_tumble[3] / VR_TP_AXES));
        VR_Test_Verdict();
        vr_et_on = 0;
    }
};
#endif
