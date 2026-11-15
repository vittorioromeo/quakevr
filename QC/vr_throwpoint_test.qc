#ifdef QVR_TESTS
// vr_throwpoint_test.qc -- `vr_embed_test 47` (gibthrow, throwfix): a thrown sword or dagger steadies towards its point in the
// air (his notes 2026-11-15 00:03: "if the throw is good enough there should be some force that stabilizes it toward the point";
// 03:21: "it always seems to immediately start spinning a lot"; the engine's pointAlign, vr_box3d_step.cpp, Point-First
// Settling). In the open (vrtesthall, god, notarget). Each throw leaves a little upwards ahead of you; its blade's angle to its
// flight read once it is VR_TP_DIST m out and again at VR_TP_DIST2 (point first at both; or as it first touches anything). Every set is thrown three times: the settling off
// (vr_throw_point_align 0), at its settings, and at extreme settings (VR_TP_Extreme):
//   good  12 throws, swords and daggers, the point 10..40 degrees off, tumbling 1..5 rad/s: point first more often with it;
//   vr    12 throws as a hand releases them in VR (his take vrtesthall_2026-11-15_03-21-03 replayed: the sword left the hand
//         136 degrees off its flight, its point up and back, turning over at 12.5 rad/s across the blade): the point 100..150
//         degrees off, up and back, turning towards its flight at 8..16 rad/s, a little roll, 10..15 m/s: most point first;
//   spun  6 throws spun on purpose, tumbling 19..24 rad/s (past Point-First Settling: Tumble): as many point first as
//         without, the tumble kept; at the extreme settings (the tumble limit 20 turns/s) steadied too;
//   axes  4 axe throws tumbling: the same flight in every pass (only pointed blades);
//   any   16 throws let go any way as a hand does (throwfix2, his note 2026-11-15 17:58: the dagger "much harder to throw point
//         first ... still tumbles"): the point 60..170 degrees off, mostly turning towards its flight, the dagger tumbling
//         14..20 rad/s at 12..20 m/s (a light blade: Max Spin's 20), the sword as fast as its weight lets (12.5 rad/s at most,
//         10..12 m/s); swords and daggers alternate: the dagger (weapons.txt throw_point) as often point first as the sword.
// Then the thrown blades' blows (vr_throwdmg_test.qc). Ends `gibthrow: N passed, M failed`.
void() VR_TD_Start;
void() VR_TD_Frame;

float VR_TP_SPEED = 10;  // m/s (the good, spun and axe throws)
float VR_TP_DIST = 3;    // m: where the angle is read (a throw at someone a few steps off) ...
float VR_TP_DIST2 = 5;   // ... and again further on: point first at both (steadied, not passing through it as it turns over)
float vr_tp_ang1;        // the throw now's angle at VR_TP_DIST (-1 short of it)
float VR_TP_GOOD = 12;
float VR_TP_VR = 12;
float VR_TP_SPUN = 6;
float VR_TP_AXES = 4;
float VR_TP_ANY = 16;
float VR_TP_PASSES = 3;
float vr_tp_anyb[6];     // the any set's point first: [blade * 3 + pass] (blade 0 the sword, 1 the dagger)
float vr_tp_spunsw[3];   // the spun set's swords point first, [pass] (the dagger, throw_point, steadies even a spin)
float vr_tp_tumblesw[3]; // and their tumble summed
float vr_tp_i;           // the throw now, over every set: good, vr, spun, axes
float vr_tp_on;          // the pass: 0 the settling off, 1 at its settings, 2 extreme
float vr_tp_keep;        // vr_throw_point_align as it was
float vr_tp_keeps;       // vr_throw_point_spin as it was
float vr_tp_keept;       // vr_throw_point_time as it was
vector vr_tp_from;       // where the throw now left from
float vr_tp_hit[15];     // point first at the read: [set * 3 + pass] counts
float vr_tp_angsum[15];  // its angles summed (degrees)
float vr_tp_tumble[6];   // the spun set's tumble across the blade at the read summed (rad/s), [pass]; the axes' spin [3 + pass]
float vr_tp_axeang[12];  // the axes: the blade's way at the read (x of the line) per throw and pass, to compare
float vr_tp_early;       // throws that touched something before the read
float vr_tp_yaw;         // the way thrown: the most open of eight from where you stand

// The most open way from `p` (the yaw of the eight whose trace goes furthest at the throws' height).
float(entity p) VR_TP_OpenYaw =
{
    float best = p.v_angle_y;
    float bestFrac = -1;
    for(float y = 0; y < 360; y = y + 45)
    {
        makevectors([0, y, 0]);
        traceline(p.origin + '0 0 64', p.origin + '0 0 64' + v_forward * 1000, TRUE, p);
        if(trace_fraction > bestFrac + 0.01)
        {
            bestFrac = trace_fraction;
            best = y;
        }
    }
    return best;
};

// The pass's settings: 0 the settling off, 1 as they were, 2 extreme (5x strength, any tumble, 1 s).
void(float xPass) VR_TP_Tune =
{
    float align = (xPass == 0) ? 0 : ((xPass == 2) ? 5 : vr_tp_keep);
    localcmd(sprintf("tune vr_throw_point_align %g\ntune vr_throw_point_spin %g\ntune vr_throw_point_time %g\n", align,
        (xPass == 2) ? 20 : vr_tp_keeps, (xPass == 2) ? 1 : vr_tp_keept));
};

void() VR_TP_Begin =
{
    VR_Test_BeginFlags("gibthrow", VR_TEST_LOUD);
    vr_eh_home = VR_Player().origin;
    vr_tp_keep = cvar_hget(cvarh_vr_throw_point_align);
    vr_tp_keeps = cvar_hget(cvarh_vr_throw_point_spin);
    vr_tp_keept = cvar_hget(cvarh_vr_throw_point_time);
    if(vr_tp_keep <= 0)
    {
        vr_tp_keep = 1;
    }
    vr_tp_i = 0;
    vr_tp_on = 0;
    vr_tp_early = 0;
    for(float k = 0; k < 15; k++)
    {
        vr_tp_hit[k] = 0;
        vr_tp_angsum[k] = 0;
    }
    for(float k2 = 0; k2 < 6; k2++)
    {
        vr_tp_tumble[k2] = 0;
        vr_tp_anyb[k2] = 0;
    }
    for(float k3 = 0; k3 < 3; k3++)
    {
        vr_tp_spunsw[k3] = 0;
        vr_tp_tumblesw[k3] = 0;
    }
    VR_TP_Tune(0);
    vr_tp_yaw = VR_TP_OpenYaw(VR_Player());
    vr_et_on = 47;
    vr_et_case = 0;
    VR_ET_To(0);
};

// The set of throw `i`: 0 good, 1 vr, 2 spun, 3 axes, 4 any.
float(float i) VR_TP_Set =
{
    if(i < VR_TP_GOOD)
    {
        return 0;
    }
    if(i < VR_TP_GOOD + VR_TP_VR)
    {
        return 1;
    }
    if(i >= VR_TP_GOOD + VR_TP_VR + VR_TP_SPUN + VR_TP_AXES)
    {
        return 4;
    }
    return (i < VR_TP_GOOD + VR_TP_VR + VR_TP_SPUN) ? 2 : 3;
};

// The size of set `xSet`.
float(float xSet) VR_TP_Count =
{
    if(xSet == 4)
    {
        return VR_TP_ANY;
    }
    return (xSet == 0) ? VR_TP_GOOD : ((xSet == 1) ? VR_TP_VR : ((xSet == 2) ? VR_TP_SPUN : VR_TP_AXES));
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

// The fractional part of `x` (the sets' deterministic spreads).
float(float x) VR_TP_Frac =
{
    return x - floor(x);
};

void() VR_TP_Throw =
{
    entity p = self;
    float i = vr_tp_i;
    float set = VR_TP_Set(i);
    makevectors([0, vr_tp_yaw, 0]);
    vector go = normalize(v_forward + '0 0 0.15');
    vector side = v_right;
    vector up = normalize( // (square to both)
        [side_y * go_z - side_z * go_y, side_z * go_x - side_x * go_z, side_x * go_y - side_y * go_x]);
    // Deterministic spreads: the point's way off (about an axis turning round the flight), the tumble's axis and rate.
    float u = VR_TP_Frac(i * 0.61803);
    float turnAbout = u * 360;
    vector k = side * cos(turnAbout * 0.01745329) + up * sin(turnAbout * 0.01745329);
    float offDeg = 0;
    float rate = 0;
    float speed = VR_TP_SPEED;
    if(set == 0)
    {
        offDeg = 10 + 30 * VR_TP_Frac(i * 0.381966);
        rate = 1 + 4 * VR_TP_Frac(i * 0.7548);
    }
    else if(set == 1)
    {
        // As the hand lets go overhand: the point up and back (about the side, a little askew), turning down towards the flight.
        float j = i - VR_TP_GOOD;
        k = normalize(side + up * (0.3 * (VR_TP_Frac(j * 0.61803) - 0.5)));
        offDeg = 100 + 50 * VR_TP_Frac(j * 0.381966);
        rate = 8 + 8 * VR_TP_Frac(j * 0.7548);
        speed = 10 + 5 * VR_TP_Frac(j * 0.5698);
    }
    else if(set == 2)
    {
        float j2 = i - VR_TP_GOOD - VR_TP_VR;
        offDeg = 360 * VR_TP_Frac(j2 * 0.381966);
        rate = 19 + j2;
    }
    else if(set == 4)
    {
        // Any way a hand lets go: the point 60..170 degrees off about a tilted side, mostly turning towards the flight (a
        // quarter away); the dagger as fast and spun as a light blade leaves (Max Spin), the sword as its weight lets.
        float j4 = i - VR_TP_GOOD - VR_TP_VR - VR_TP_SPUN - VR_TP_AXES;
        k = normalize(side + up * (0.6 * (VR_TP_Frac(j4 * 0.61803) - 0.5)));
        offDeg = 60 + 110 * VR_TP_Frac(floor(j4 / 2) * 0.381966);
        rate = 14 + 6 * VR_TP_Frac(floor(j4 / 2) * 0.7548);
        speed = 12 + 8 * VR_TP_Frac(floor(j4 / 2) * 0.5698);
        if(!(i & 1))
        {
            rate = min(rate, 12.5);
            speed = 10 + 2 * VR_TP_Frac(floor(j4 / 2) * 0.5698);
        }
    }
    else
    {
        rate = 10 + 2 * (i - VR_TP_GOOD - VR_TP_VR - VR_TP_SPUN);
    }
    vr_tp_from = p.origin + '0 0 64' + go * 40;
    vr_tp_ang1 = -1;
    if(set != 3)
    {
        vr_et_flags = (i & 1) ? VR_ADMelee_Flags(VR_SWORDKIND_DAGGER) : 0;
        vr_et_w = VR_ET_ThrowSword(WID_SWORD, vr_tp_from, go, VR_AxeStick_Turn(go, k, offDeg), FALSE);
        vr_et_flags = 0;
        vector line = VR_TP_Line(vr_et_w);
        // Tumbling across the blade: about an axis square to its line (the vr set's: turning its point back down to the flight).
        vector across = normalize(k - line * (k * line));
        float towards = (set == 1) || (set == 4 && VR_TP_Frac(floor((i - VR_TP_GOOD - VR_TP_VR - VR_TP_SPUN - VR_TP_AXES) / 2) * 0.29) < 0.75);
        vr_et_w.vr_spin = across * (towards ? -rate : rate);
        if(set == 1 || set == 4)
        {
            vr_et_w.vr_spin = vr_et_w.vr_spin + line * (VR_TP_Frac(i * 0.29) * 2 - 1); // (a little roll)
        }
    }
    else
    {
        vr_et_w = VR_ET_ThrowAxe(vr_tp_from, go, FALSE);
        vr_et_w.vr_spin = side * rate;
    }
    vr_et_w.velocity = go * (speed * VRMetersToUnits());
};

void() VR_TP_Frame =
{
    entity p = self;
    float t = time - vr_et_t0;
    float total = VR_TP_GOOD + VR_TP_VR + VR_TP_SPUN + VR_TP_AXES + VR_TP_ANY;
    if(vr_et_s >= 10)
    {
        VR_TD_Frame(); // (then the thrown blades' blows: vr_throwdmg_test.qc)
        return;
    }
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
        float far = vlen(w.origin - vr_tp_from);
        float out = (far >= VR_TP_DIST2 * VRMetersToUnits());
        float touched = ((w.throwhit == QVR_THROWHIT_HIT) || (vlen(w.velocity) < 0.5 * VR_TP_SPEED * VRMetersToUnits()));
        if(vr_tp_ang1 < 0 && far >= VR_TP_DIST * VRMetersToUnits())
        {
            vr_tp_ang1 = VR_TP_Angle(VR_TP_Line(w), normalize(w.velocity));
        }
        if(out || touched || (t > 2))
        {
            float set = VR_TP_Set(vr_tp_i);
            vector line = VR_TP_Line(w);
            float ang = VR_TP_Angle(line, normalize(w.velocity));
            if(!out || vr_tp_ang1 < 0)
            {
                vr_tp_early = vr_tp_early + 1;
            }
            float slot = set * 3 + vr_tp_on;
            if(cvar_hget(cvarh_developer))
            {
                dprint(sprintf("gibthrow: throw %g (set %g, pass %g): %.1f degrees off its flight at 3 m, %.1f at 5 m", vr_tp_i, set,
                    vr_tp_on, vr_tp_ang1, ang));
                dprint(sprintf(" %s (%.0f u out, %.0f u up, %.2f s)\n", out ? "in the air" : "(touched first)", far, w.origin_z - vr_tp_from_z, t));
            }
            ang = max(ang, vr_tp_ang1); // (point first at both)
            if(ang <= cvar_hget(cvarh_vr_embed_sword_angle))
            {
                vr_tp_hit[slot] = vr_tp_hit[slot] + 1;
                if(set == 4)
                {
                    vr_tp_anyb[(vr_tp_i & 1) * 3 + vr_tp_on] = vr_tp_anyb[(vr_tp_i & 1) * 3 + vr_tp_on] + 1;
                }
                if(set == 2 && !(vr_tp_i & 1))
                {
                    vr_tp_spunsw[vr_tp_on] = vr_tp_spunsw[vr_tp_on] + 1;
                }
            }
            vr_tp_angsum[slot] = vr_tp_angsum[slot] + ang;
            vector sp = w.vr_spin;
            if(set == 2)
            {
                vr_tp_tumble[vr_tp_on] = vr_tp_tumble[vr_tp_on] + vlen(sp - line * (sp * line));
                if(!(vr_tp_i & 1))
                {
                    vr_tp_tumblesw[vr_tp_on] = vr_tp_tumblesw[vr_tp_on] + vlen(sp - line * (sp * line));
                }
            }
            else if(set == 3)
            {
                vr_tp_tumble[3 + vr_tp_on] = vr_tp_tumble[3 + vr_tp_on] + vlen(sp);
                vr_tp_axeang[(vr_tp_i - VR_TP_GOOD - VR_TP_VR - VR_TP_SPUN) * 3 + vr_tp_on] = line_x + line_y * 10 + line_z * 100;
            }
            remove(w);
            vr_et_w = world;
            vr_tp_i = vr_tp_i + 1;
            if(vr_tp_i >= total)
            {
                vr_tp_i = 0;
                vr_tp_on = vr_tp_on + 1;
                if(vr_tp_on < VR_TP_PASSES)
                {
                    VR_TP_Tune(vr_tp_on);
                }
            }
            VR_ET_To((vr_tp_on >= VR_TP_PASSES) ? 3 : 0);
        }
    }
    else if(vr_et_s == 3)
    {
        VR_TP_Tune(1);
        float a = cvar_hget(cvarh_vr_embed_sword_angle);
        for(float s = 0; s < 3; s++)
        {
            float n = VR_TP_Count(s);
            VR_Test_Print(sprintf("%s: %s throws point first (within %g degrees at 3 and 5 m): off %g, on %g, extreme %g of %g", vr_test_tag,
                (s == 0) ? "good" : ((s == 1) ? "vr" : "spun"), a, vr_tp_hit[s * 3], vr_tp_hit[s * 3 + 1], vr_tp_hit[s * 3 + 2], n));
            VR_Test_Print(sprintf(" (mean %.0f, %.0f, %.0f degrees off)\n", vr_tp_angsum[s * 3] / n, vr_tp_angsum[s * 3 + 1] / n,
                vr_tp_angsum[s * 3 + 2] / n));
        }
        VR_Test_Print(sprintf("%s: spun throws' tumble across the blade %.1f, %.1f, %.1f rad/s", vr_test_tag, vr_tp_tumble[0] / VR_TP_SPUN,
            vr_tp_tumble[1] / VR_TP_SPUN, vr_tp_tumble[2] / VR_TP_SPUN));
        VR_Test_Print(sprintf("; axes' spin %.2f, %.2f, %.2f rad/s; %g touched first\n", vr_tp_tumble[3] / VR_TP_AXES,
            vr_tp_tumble[4] / VR_TP_AXES, vr_tp_tumble[5] / VR_TP_AXES, vr_tp_early));
        VR_Test_Check(vr_tp_early == 0, sprintf("every throw read in the air at %g m (%g touched something first)", VR_TP_DIST2, vr_tp_early));
        VR_Test_Check(vr_tp_hit[1] >= vr_tp_hit[0] + 3, sprintf("good throws settle point first more often (%g of %g, %g without)", vr_tp_hit[1], VR_TP_GOOD, vr_tp_hit[0]));
        VR_Test_Check(vr_tp_hit[4] >= 9 && vr_tp_hit[4] >= vr_tp_hit[3] + 4, sprintf("VR-style overhand throws: most point first (%g of %g, %g without)", vr_tp_hit[4], VR_TP_VR, vr_tp_hit[3]));
        // (Swords: the dagger's leniency, weapons.txt throw_point, steadies even a spin given on purpose: its point first.)
        float spunSw = VR_TP_SPUN / 2;
        VR_Test_Check((vr_tp_spunsw[1] <= vr_tp_spunsw[0] + 1) && (vr_tp_tumblesw[1] >= vr_tp_tumblesw[0] * 0.85),
            sprintf("spun sword throws stay a tumble (%g point first, %g without; tumble %.1f of %.1f rad/s)", vr_tp_spunsw[1], vr_tp_spunsw[0],
            vr_tp_tumblesw[1] / spunSw, vr_tp_tumblesw[0] / spunSw));
        VR_Test_Check((vr_tp_hit[8] >= 4) && (vr_tp_hit[5] >= 11), sprintf("the extreme settings steady even spun throws (%g of %g) and the VR ones (%g of %g)", vr_tp_hit[8], VR_TP_SPUN, vr_tp_hit[5], VR_TP_VR));
        float same = TRUE;
        for(float m = 0; m < VR_TP_AXES; m++)
        {
            same = (same && (fabs(vr_tp_axeang[m * 3] - vr_tp_axeang[m * 3 + 1]) < 0.001) && (fabs(vr_tp_axeang[m * 3] - vr_tp_axeang[m * 3 + 2]) < 0.001));
        }
        VR_Test_Check(same && (fabs(vr_tp_tumble[3] - vr_tp_tumble[4]) < 0.001) && (fabs(vr_tp_tumble[3] - vr_tp_tumble[5]) < 0.001),
            sprintf("axes fly the same in every pass (spin %.3f, %.3f, %.3f rad/s)", vr_tp_tumble[3] / VR_TP_AXES, vr_tp_tumble[4] / VR_TP_AXES, vr_tp_tumble[5] / VR_TP_AXES));
        float half = VR_TP_ANY / 2;
        VR_Test_Print(sprintf("%s: any way throws point first: swords off %g, on %g, extreme %g of %g;", vr_test_tag, vr_tp_anyb[0],
            vr_tp_anyb[1], vr_tp_anyb[2], half));
        VR_Test_Print(sprintf(" daggers off %g, on %g, extreme %g of %g (mean %.0f degrees off, on)\n", vr_tp_anyb[3], vr_tp_anyb[4],
            vr_tp_anyb[5], half, vr_tp_angsum[13] / VR_TP_ANY));
        VR_Test_Check(vr_tp_anyb[4] >= half - 1 && vr_tp_anyb[4] >= vr_tp_anyb[1],
            sprintf("daggers let go any way settle point first, at least as often as swords (%g of %g; swords %g)", vr_tp_anyb[4], half, vr_tp_anyb[1]));
        VR_TD_Start(); // (the thrown blades' blows: vr_throwdmg_test.qc; it ends the test)
    }
};
#endif
