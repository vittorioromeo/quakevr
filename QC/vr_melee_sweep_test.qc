#ifdef QVR_TESTS
// vr_melee_sweep_test 1 (Debug > Tests > Melee Sweep): VR_Melee_Sweep's contacts (its probe: no speeds, nothing armed
// or decided) for scripted sweeps of the main hand's weapon (its line's length, its layout and a hammer's or mace's head
// as held: give it first, `vr_ad_test 116`..`129`), in the air ahead of the player past a target box, and once at the
// floor; each with vr_melee_swept 0 (the points' lines, as before) and 1 (the swept surface). "meleesweep:" lines (hit,
// the striking point, the contact), then PASS or FAIL:
//   A  the blade's middle swept across a box (20 units), its tip past it: both strike; swept, the contact is where the
//      blade crossed the box's middle (old: its outermost point, at its edge).
//   E  a small box (6 units) only the blade's outer third crosses (0.85 of its line): swept strikes it.
//   B  a thin pole (1 unit) halfway between the two striking points farthest apart: swept strikes it.
//   C  a hammer's or mace's head (when it has one) swept by a small box beside its line, within the head: swept strikes it.
//   F  a box the sweep passes 3 units clear of (past the tip, past a head's face): neither strikes.
//   D  the blade laid flat on the floor in one frame (from 4 units over to 3 into it): swept lands at the blade's
//      middle (old: its first point).

float vr_mswt_fail;

// One probe of the sweep of what `b` holds (`xThing`) from the line at `xG0` (grip), `xA0` (way, unit) to `xG1`, `xA1`,
// `xLen` long, the model's +y `xY`: into vr_msw_*.
void(entity b, float xThing, vector xG0, vector xA0, vector xG1, vector xA1, vector xY, float xLen, float xSwept) VR_MSWT_Probe =
{
    float i;
    VR_Melee_Layout(xThing);
    b.mh_thing = xThing;
    b.mh_free = FALSE;
    b.mh_struck = world;
    b.mh_hit_ent = world;
    b.mh_n = vr_mpt_n;
    vr_mline_bend = 0;
    vr_mline_free = FALSE;
    for(i = 0; i < vr_mpt_n; ++i)
    {
        vr_mline_grip = xG0;
        vr_mline_far = xG0 + xA0 * xLen;
        vr_mline_near = xG0 - xA0 * xLen * 0.2;
        b.mh_pp[i] = VR_Melee_At(vr_mpt_f[i]);
        vr_mline_grip = xG1;
        vr_mline_far = xG1 + xA1 * xLen;
        vr_mline_near = xG1 - xA1 * xLen * 0.2;
        b.mh_cp[i] = VR_Melee_At(vr_mpt_f[i]);
    }
    // The grip at both poses: VR_Melee_ArcRib bends a point's line round the grip's way from mh_pgrip to mh_grip (left
    // '0 0 0', the lines were bent round the map's origin: F and D failed wherever the test ran far from it).
    b.mh_pgrip = xG0;
    b.mh_grip = xG1;
    b.mh_far = xG1 + xA1 * xLen;
    b.mh_hd_pg = xG0;
    b.mh_hd_pf = xG0 + xA0 * xLen;
    b.mh_hd_py = xY;
    b.mh_hd_cg = xG1;
    b.mh_hd_cf = xG1 + xA1 * xLen;
    b.mh_hd_cy = xY;
    float was = cvar("vr_melee_swept");
    cvar_set("vr_melee_swept", ftos(xSwept));
    vr_msw_probe = TRUE;
    VR_Melee_Sweep(cVR_MainHand, b);
    vr_msw_probe = FALSE;
    cvar_set("vr_melee_swept", ftos(was));
};

// Probes both ways and prints them; the contact's place along the blade (from the grip, `xG1` along `xA1`) and off the
// target's middle `xC`.
float vr_mswt_hit0;
float vr_mswt_hit1;
vector vr_mswt_at0;
vector vr_mswt_at1;
void(string xName, entity b, entity xT, float xThing, vector xG0, vector xA0, vector xG1, vector xA1, vector xY, float xLen,
     vector xC) VR_MSWT_Case =
{
    float s;
    for(s = 0; s < 2; ++s)
    {
        VR_MSWT_Probe(b, xThing, xG0, xA0, xG1, xA1, xY, xLen, s);
        float hit = (vr_msw_edge == xT);
        vector at = vr_msw_edge_at;
        float pt = vr_msw_edge_i;
        if(xT == world)
        {
            hit = (vr_msw_wall_i >= 0);
            at = vr_msw_wall_at;
            pt = vr_msw_wall_i;
        }
        if(s == 0)
        {
            vr_mswt_hit0 = hit;
            vr_mswt_at0 = at;
        }
        else
        {
            vr_mswt_hit1 = hit;
            vr_mswt_at1 = at;
        }
        bprint(sprintf("meleesweep: %s swept %g (%g lines): %s", xName, s, vr_rib_n, hit ? "hit" : "miss"));
        if(hit)
        {
            bprint(sprintf(" by point %g (%s), %.1f along the blade, %.1f off the target's middle", pt, vr_mpt_name[pt],
                (at - xG1) * xA1, vlen(at - xC)));
        }
        bprint("\n");
    }
};

void(entity p) VR_Melee_SweepTest =
{
    float thing = VR_Melee_Thing(p, cVR_MainHand);
    if(!VR_Melee_Line(p, cVR_MainHand, thing) || thing == VR_MTHING_FIST || thing == VR_MTHING_NONE)
    {
        bprint(sprintf("meleesweep: the main hand holds no weapon to sweep (thing %g, %s): FAIL\n", thing, p.weaponmodel));
        return;
    }
    float len = vlen(vr_mline_far - vr_mline_grip);
    // Its head's box and width as the track keeps them (an AD hammer's or mace's, or the weapon's own: Edit Blade Shape).
    VR_Melee_HeadOf(p, cVR_MainHand, thing);
    vector hlo = vr_mhead_lo;
    vector hhi = vr_mhead_hi;
    float width = vr_mline_shape_y;
    VR_Melee_Layout(thing);
    float r = VR_MELEE_RADIUS * max(0.25, cvar_hget(cvarh_vr_melee_range_multiplier)) * width;
    bprint(sprintf("meleesweep: %s (thing %g), its line %.1f units, %g points, head %v .. %v\n", p.weaponmodel, thing, len,
        vr_mpt_n, hlo, hhi));
    vr_mswt_fail = FALSE;

    // In the air ahead (the player's facing), clear of walls.
    makevectors('0 1 0' * p.v_angle_y);
    vector fwd = v_forward;
    vector side = v_right;
    vector O = p.origin + fwd * (len + 40) + '0 0 24';
    traceline(p.origin + '0 0 24', O + fwd * (len + 40), TRUE, p);
    if(trace_fraction < 1)
    {
        bprint("meleesweep: no room ahead: FAIL\n");
        return;
    }
    entity b = spawn();
    b.mh_hd_lo = hlo;
    b.mh_hd_hi = hhi;
    b.mh_bwidth = width;
    entity t = spawn();
    t.classname = "vr_meleesweep_target";
    t.solid = SOLID_BBOX;
    t.movetype = MOVETYPE_NONE;
    t.takedamage = DAMAGE_AIM;
    t.health = 1000;
    entity was = self;
    self = p;

    // The blade along `side` (across the facing), swept 24 units ahead in one frame; the grip at O - side * len/2.
    vector g0 = O - side * (len * 0.5) - fwd * 12;
    vector g1 = g0 + fwd * 24;
    vector up = '0 0 1';

    // A: a box (20 units) whose middle the blade's middle crosses; the tip 0.45 of the line past its middle.
    float fA = 0.55;
    vector cA = O - side * (len * 0.5) + side * (len * fA);
    setorigin(t, cA);
    setsize(t, '-10 -10 -10', '10 10 10');
    VR_MSWT_Case("A (the blade's middle across a box)", b, t, thing, g0, side, g1, side, up, len, cA);
    float offOld = fabs((vr_mswt_at0 - g1) * side - len * fA);
    float offNew = fabs((vr_mswt_at1 - g1) * side - len * fA);
    if(!vr_mswt_hit1 || offNew > 10 || (vr_mswt_hit0 && offNew > offOld + 0.5))
    {
        bprint("meleesweep: A FAIL\n");
        vr_mswt_fail = TRUE;
    }

    // E: a small box (6 units) only the blade's outer third crosses (0.85 of its line): swept strikes it.
    float fE = 0.85;
    vector cE = O - side * (len * 0.5) + side * (len * fE);
    setorigin(t, cE);
    setsize(t, '-3 -3 -3', '3 3 3');
    VR_MSWT_Case("E (only the outer third across a box)", b, t, thing, g0, side, g1, side, up, len, cE);
    if(!vr_mswt_hit1)
    {
        bprint("meleesweep: E FAIL\n");
        vr_mswt_fail = TRUE;
    }
    setsize(t, '-10 -10 -10', '10 10 10');

    // B: a thin pole halfway between the two striking points farthest apart.
    float i;
    float gapAt = -1;
    float gap = 0;
    for(i = 0; i + 1 < vr_mpt_n; ++i)
    {
        if(VR_Melee_Strikes(vr_mpt_kind[i]) && VR_Melee_Strikes(vr_mpt_kind[i + 1]) && vr_mpt_f[i + 1] - vr_mpt_f[i] > gap)
        {
            gap = vr_mpt_f[i + 1] - vr_mpt_f[i];
            gapAt = (vr_mpt_f[i] + vr_mpt_f[i + 1]) * 0.5;
        }
    }
    if(gapAt > 0)
    {
        vector cB = O - side * (len * 0.5) + side * (len * gapAt);
        setorigin(t, cB);
        setsize(t, '-0.5 -0.5 -20', '0.5 0.5 20');
        bprint(sprintf("meleesweep: B's gap %.1f units (the points' lines %.1f thick)\n", gap * len, r));
        VR_MSWT_Case("B (a thin pole between two points)", b, t, thing, g0, side, g1, side, up, len, cB);
        if(!vr_mswt_hit1)
        {
            bprint("meleesweep: B FAIL\n");
            vr_mswt_fail = TRUE;
        }
    }

    // C: a head (the line up, its face ahead): a small box beside the line within the head's depth, the head swept 24
    // units through it along the facing.
    if(hhi != '0 0 0')
    {
        // The line up (`up`), the model's +y across the facing (`side`): the head's width along `side` x `up`.
        vector hg0 = O - up * (len * (hlo_x + hhi_x) * 0.5) - fwd * 12;
        vector hg1 = hg0 + fwd * 24;
        vector pn = normalize(VR_Melee_Cross(side, up));
        vector hc = O + pn * (len * (hlo_y + hhi_y) * 0.5);
        vector cC = hc + side * (len * hhi_z * 0.7 + 2);
        setorigin(t, cC);
        setsize(t, '-2 -2 -2', '2 2 2');
        bprint(sprintf("meleesweep: C's head %.1f long, %.1f across, %.1f deep; the box %.1f off its line\n",
            len * (hhi_x - hlo_x), len * (hhi_y - hlo_y), len * hhi_z * 2, len * hhi_z * 0.7 + 2));
        VR_MSWT_Case("C (a box beside the head)", b, t, thing, hg0, up, hg1, up, side, len, cC);
        if(!vr_mswt_hit1)
        {
            bprint("meleesweep: C FAIL\n");
            vr_mswt_fail = TRUE;
        }
        // F (head): 3 units clear of its depth.
        vector cF = hc + side * (len * hhi_z + r + 3 + 2);
        setorigin(t, cF);
        VR_MSWT_Case("F (a box clear of the head)", b, t, thing, hg0, up, hg1, up, side, len, cF);
        if(vr_mswt_hit0 || vr_mswt_hit1)
        {
            bprint("meleesweep: F FAIL\n");
            vr_mswt_fail = TRUE;
        }
    }
    else
    {
        // F: a box 3 units clear past the tip.
        vector cT = O - side * (len * 0.5) + side * (len + r + 3 + 2);
        setorigin(t, cT);
        setsize(t, '-2 -2 -2', '2 2 2');
        VR_MSWT_Case("F (a box clear past the tip)", b, t, thing, g0, side, g1, side, up, len, cT);
        if(vr_mswt_hit0 || vr_mswt_hit1)
        {
            bprint("meleesweep: F FAIL\n");
            vr_mswt_fail = TRUE;
        }
    }
    setorigin(t, p.origin + '0 0 -4000');
    setsize(t, '0 0 0', '0 0 0');

    // D: the floor under the line's middle: the blade flat along `side`, from 4 units over it to 3 into it.
    traceline(O, O - '0 0 400', TRUE, p);
    if(trace_fraction < 1 && trace_plane_normal_z > 0.9)
    {
        float fz = trace_endpos_z;
        vector d0 = O - side * (len * 0.5);
        d0_z = fz + 4;
        vector d1 = d0;
        d1_z = fz - 3;
        vector cD = d1 + side * (len * 0.6);
        cD_z = fz;
        VR_MSWT_Case("D (the blade flat onto the floor)", b, world, thing, d0, side, d1, side, up, len, cD);
        // Its striking part: from its first striking point to its tip.
        float s0 = 1;
        for(i = 0; i < vr_mpt_n; ++i)
        {
            if(VR_Melee_Strikes(vr_mpt_kind[i]))
            {
                s0 = min(s0, vr_mpt_f[i]);
            }
        }
        float midD = len * (s0 + 1) * 0.5;
        float alongD = (vr_mswt_at1 - d1) * side;
        bprint(sprintf("meleesweep: D's striking part's middle %.1f along the blade; swept landed %.1f, old %.1f\n", midD,
            alongD, (vr_mswt_at0 - d1) * side));
        if(!vr_mswt_hit1 || fabs(alongD - midD) > len * 0.15 || fabs(vr_mswt_at1_z - fz) > 1)
        {
            bprint("meleesweep: D FAIL\n");
            vr_mswt_fail = TRUE;
        }
    }
    else
    {
        bprint("meleesweep: D: no floor under the test\n");
    }
    self = was;
    remove(t);
    remove(b);
    bprint(vr_mswt_fail ? "meleesweep: FAIL\n" : "meleesweep: PASS\n");
};

// vr_melee_sweep_test 3 (the blade shape editor, ROUND22.md "Blade shape editor"): the main hand's weapon's own shape
// (Weapon Offsets' Blade keys) strikes as it is drawn. "bladeshape:" lines, then PASS or FAIL:
//   curve  (its line bent: a curve or a blade start of its own, or an AD sword's) the blade's points swept 24 units across its flat in one
//          frame (VR_Melee_Sweep, swept): a small box just outside the bow is struck along the curve and missed by the
//          straight line; one just inside it (across the line) struck by the line, missed by the curve.
//   width  a small box past the tip, 1.5 radii from its sweep: missed at width 1; struck at the weapon's Blade Width (when
//          it is 2 or more).
//   near   the near end's share behind the hand (Blade Near End, or the kind's).
// The curve probe: the main hand's line as VR_Melee_Line left it, its points moved by `xD` in one frame; `xBent` FALSE the
// line straight. TRUE if `xT` is struck.
float(entity b, entity xT, float xThing, vector xD, float xBent, float xWidth) VR_MSWT_LineProbe =
{
    float i;
    VR_Melee_Line(self, cVR_MainHand, xThing); // (afresh: the sweep's own may have laid out another)
    float bend = vr_mline_bend;
    if(!xBent)
    {
        vr_mline_bend = 0;
    }
    VR_Melee_Layout(xThing);
    VR_Melee_LayoutFrom(vr_mline_from_x); // (a blade start of its own: its striking points from it)
    b.mh_thing = xThing;
    b.mh_free = FALSE;
    b.mh_struck = world;
    b.mh_hit_ent = world;
    b.mh_n = vr_mpt_n;
    b.mh_bwidth = xWidth;
    for(i = 0; i < vr_mpt_n; ++i)
    {
        b.mh_pp[i] = VR_Melee_At(vr_mpt_f[i]);
        b.mh_cp[i] = b.mh_pp[i] + xD;
    }
    vr_mline_bend = bend;
    b.mh_pgrip = vr_mline_grip;
    b.mh_grip = vr_mline_grip + xD;
    b.mh_far = vr_mline_far + xD;
    b.mh_hd_lo = b.mh_hd_hi = '0 0 0';
    float was = cvar("vr_melee_swept");
    cvar_set("vr_melee_swept", "1");
    vr_msw_probe = TRUE;
    VR_Melee_Sweep(cVR_MainHand, b);
    vr_msw_probe = FALSE;
    cvar_set("vr_melee_swept", ftos(was));
    return vr_msw_edge == xT;
};

void(entity p) VR_Melee_BladeShapeTest =
{
    entity was = self;
    self = p;
    float thing = VR_Melee_Thing(p, cVR_MainHand);
    if(!VR_Melee_Line(p, cVR_MainHand, thing) || thing == VR_MTHING_FIST || thing == VR_MTHING_NONE)
    {
        bprint(sprintf("bladeshape: the main hand holds no weapon (thing %g, %s): FAIL\n", thing, p.weaponmodel));
        self = was;
        return;
    }
    vector grip = vr_mline_grip, far = vr_mline_far;
    float len = vlen(far - grip);
    float rBase = VR_MELEE_RADIUS * max(0.25, cvar_hget(cvarh_vr_melee_range_multiplier));
    bprint(sprintf("bladeshape: %s slot %g shape %v bend %g, line %.1f units, near end %.3f behind\n", p.weaponmodel,
        vr_mline_slot, vr_mline_shape, vr_mline_bend, len, vlen(vr_mline_near - grip) / max(1, len)));
    vector half = VR_Melee_At(0.5);
    bprint(sprintf("bladeshape: at 0.5 world %.2f %.2f %.2f (Edit Blade Shape's report: its curve 0.5)\n", half_x, half_y,
        half_z));
    float fail = FALSE;
    entity b = spawn();
    entity t = spawn();
    t.classname = "vr_meleesweep_target";
    t.solid = SOLID_BBOX;
    t.movetype = MOVETYPE_NONE;
    t.takedamage = DAMAGE_AIM;
    t.health = 1000;
    setsize(t, '-1 -1 -1', '1 1 1');
    // Across the flat (the model's +y: VR_Melee_BendLine's).
    vector across = VR_Melee_ModelY(p, cVR_MainHand, far - grip);
    vector a = normalize(far - grip);
    vector d = across * 24;

    // The curve: the layout's point farthest off the line (the sweep strikes with them, the blade between two of them
    // as the straight line joining them).
    if(vr_mline_bend)
    {
        float most = 0;
        vector mostAt = grip;
        vector mostLine = grip;
        float f;
        float i;
        VR_Melee_Layout(thing);
        VR_Melee_LayoutFrom(vr_mline_from_x);
        for(i = 0; i < vr_mpt_n; ++i)
        {
            f = vr_mpt_f[i];
            if(f <= 0)
            {
                continue;
            }
            vector at = VR_Melee_At(f);
            vector line = grip + (far - grip) * f;
            if(vlen(at - line) > most)
            {
                most = vlen(at - line);
                mostAt = at;
                mostLine = line;
            }
        }
        vector out = normalize(mostAt - mostLine);
        vector outer = mostAt + out * (1 + rBase * 0.5) + d * 0.5;
        vector inner = mostLine - out * (1 + rBase * 0.5) + d * 0.5;
        setorigin(t, outer);
        float co = VR_MSWT_LineProbe(b, t, thing, d, TRUE, 1);
        float so = VR_MSWT_LineProbe(b, t, thing, d, FALSE, 1);
        setorigin(t, inner);
        float ci = VR_MSWT_LineProbe(b, t, thing, d, TRUE, 1);
        float si = VR_MSWT_LineProbe(b, t, thing, d, FALSE, 1);
        float ok = ((most > 2) && co && !so && !ci && si);
        bprint(sprintf("bladeshape: curve %.2f units off the line at most (%.2f along): outside struck by the curve %g, "
            "the line %g; inside %g, %g: %s\n", most, ((mostLine - grip) * a) / max(1, len), co, so, ci, si,
            ok ? "PASS" : "FAIL"));
        fail = (fail || !ok);
    }
    else
    {
        bprint("bladeshape: curve: straight\n");
    }

    // The width: a box past the tip (on the line, where no curve goes), its near face 1.5 radii from the tip's sweep.
    setorigin(t, far + d * 0.5 + a * (rBase * 1.5 + 1));
    float w1 = VR_MSWT_LineProbe(b, t, thing, d, TRUE, 1);
    float ww = VR_MSWT_LineProbe(b, t, thing, d, TRUE, vr_mline_shape_y);
    float wok = (!w1 && (vr_mline_shape_y < 2 || ww));
    bprint(sprintf("bladeshape: width %.2f: a box %.1f units off the blade struck at width 1 %g, at its own %g: %s\n",
        vr_mline_shape_y, rBase * 1.5, w1, ww, wok ? "PASS" : "FAIL"));
    fail = (fail || !wok);
    self = was;
    remove(t);
    remove(b);
    bprint(fail ? "bladeshape: FAIL\n" : "bladeshape: PASS\n");
};

// vr_melee_sweep_test 4 (the fist's blow line, ROUND22.md "The fist's blow line"): the main hand empty. "fistline:" lines:
// its ends and near end in the hand's frame (forward, right, up from the hand) and its width; then a small box at the
// line's knuckles end, swept across (24 units in one frame, square to the line and to the way the edit moved the end) by
// the line as it is now and by the default one (the hand to 4 units ahead, width 1). PASS: the line now strikes it, and
// the default line strikes it only if it reaches it (its swept radius plus the box's half from it): an edited line far
// enough from the default one strikes what the default misses.
float(entity b, entity xT, vector xGrip, vector xFar, vector xD, float xWidth) VR_MSWT_FistProbe =
{
    VR_Melee_Layout(VR_MTHING_FIST);
    b.mh_thing = VR_MTHING_FIST;
    b.mh_free = FALSE;
    b.mh_struck = world;
    b.mh_hit_ent = world;
    b.mh_n = vr_mpt_n;
    b.mh_bwidth = xWidth;
    b.mh_pp[0] = xGrip;
    b.mh_cp[0] = xGrip + xD;
    b.mh_pp[1] = xFar;
    b.mh_cp[1] = xFar + xD;
    b.mh_pgrip = xGrip;
    b.mh_grip = xGrip + xD;
    b.mh_far = xFar + xD;
    b.mh_hd_lo = b.mh_hd_hi = '0 0 0';
    float was = cvar("vr_melee_swept");
    cvar_set("vr_melee_swept", "1");
    vr_msw_probe = TRUE;
    VR_Melee_Sweep(cVR_MainHand, b);
    vr_msw_probe = FALSE;
    cvar_set("vr_melee_swept", ftos(was));
    return vr_msw_edge == xT;
};

void(entity p) VR_Melee_FistLineTest =
{
    entity was = self;
    self = p;
    float thing = VR_Melee_Thing(p, cVR_MainHand);
    if(thing != VR_MTHING_FIST || !VR_Melee_Line(p, cVR_MainHand, thing))
    {
        bprint(sprintf("fistline: the main hand is not empty (thing %g, %s): FAIL\n", thing, p.weaponmodel));
        self = was;
        return;
    }
    vector h = VRGetEntHandPos(p, cVR_MainHand);
    vector grip = vr_mline_grip, far = vr_mline_far, nearEnd = vr_mline_near;
    float width = vr_mline_shape_y;
    makevectors(VRGetEntHandRot(p, cVR_MainHand));
    vector fwd = v_forward, up = v_up;
    vector lg, lf, ln;
    lg_x = (grip - h) * v_forward; lg_y = (grip - h) * v_right; lg_z = (grip - h) * v_up;
    lf_x = (far - h) * v_forward; lf_y = (far - h) * v_right; lf_z = (far - h) * v_up;
    ln_x = (nearEnd - h) * v_forward; ln_y = (nearEnd - h) * v_right; ln_z = (nearEnd - h) * v_up;
    bprint(sprintf("fistline: from %v to %v near %v width %.3f\n", lg, lf, ln, width));
    vector dGrip = h;
    vector dFar = h + VRForwardOf(VRGetEntHandRot(p, cVR_MainHand)) * 4;
    // Across: square to the line and to the edit's move of the far end (the hand's up when it did not move).
    vector moved = far - dFar;
    vector across = VR_Melee_Cross(fwd, moved);
    if(vlen(across) < 0.01)
    {
        across = up;
    }
    vector d = normalize(across) * 24;
    entity b = spawn();
    entity t = spawn();
    t.classname = "vr_meleesweep_target";
    t.solid = SOLID_BBOX;
    t.movetype = MOVETYPE_NONE;
    t.takedamage = DAMAGE_AIM;
    t.health = 1000;
    setsize(t, '-1 -1 -1', '1 1 1');
    setorigin(t, far + d * 0.5);
    float now = VR_MSWT_FistProbe(b, t, grip, far, d, width);
    float def = VR_MSWT_FistProbe(b, t, dGrip, dFar, d, 1);
    float rBase = VR_MELEE_RADIUS * max(0.25, cvar_hget(cvarh_vr_melee_range_multiplier));
    float apart = VR_SegmentDistance(far, far, dGrip, dFar);
    float reaches = apart < rBase + 1;
    float ok = now && (def == reaches || fabs(apart - rBase - 1) < 0.5);
    bprint(sprintf("fistline: a box at its knuckles end (%.2f units off the default line): struck by it %g, by the "
        "default line %g: %s\n", apart, now, def, ok ? "PASS" : "FAIL"));
    self = was;
    remove(t);
    remove(b);
};

// vr_melee_sweep_test 6 (one swing, many things: ROUND22.md "One swing hits everything its edge passes through"): the
// main hand's weapon (a sword: the scene sword_multi gives one) swung in scripted frames at what the scene placed, every
// blow landing (vr_msw_decide: VR_Melee_Decide's speed tests skipped, a cut at strength 1.5) and dealt as in play
// (VR_Melee_Sweep, VR_Melee_DealAll: the weapon's damage, gore, sparks), each case with vr_melee_multi 0 (as before:
// the frame after a blow skipped, as PlayerVRMeleeImpl's 0.02 s pause at 72 Hz) and then 1. "meleemulti:" lines, then
// PASS or FAIL (multi 1):
//   floor  a chop at the nearest corpse's limb (an arm's lower part) lying on the floor, the blade's tip lower: frame 1
//          its tip into the floor beside the limb, frame 2 the blade through the limb: the floor and the corpse struck,
//          the limb cut off (vr_limbs).
//   two    a level swing across the two live monsters nearest each other, the blade pointing past them, 12 units a
//          frame: both struck.
//   wall   a swing along the wall beside the live monster nearest a wall (another than the two), the tip going into
//          the wall first: the wall and the monster struck.
// The live monsters' health is raised to 5000 first (each case strikes them twice); the corpse is struck by multi 1 only
// (off first: it must stay whole).
float vr_mmt_skip; // (multi 0: the frames skipped after a blow)
float vr_mmt_probeonly; // (VR_MMT_Frame: the poses set, the sweep probed, nothing dealt)
vector vr_mmt_g;   // the last pose's grip, way
vector vr_mmt_a;

// The main hand's blow `b` swept from the last pose to the grip `xG`, the blade's way `xA` (`xLen` long), moving at
// `xVel` (world, m/s): the blows it lands, dealt.
void(entity b, vector xG, vector xA, float xLen, vector xVel) VR_MMT_Frame =
{
    float h = cVR_MainHand;
    VR_Melee_LayoutOf(b);
    b.mh_n = vr_mpt_n;
    vr_mline_bend = 0;
    vr_mline_free = FALSE;
    float i;
    for(i = 0; i < vr_mpt_n; ++i)
    {
        vr_mline_grip = vr_mmt_g;
        vr_mline_far = vr_mmt_g + vr_mmt_a * xLen;
        vr_mline_near = vr_mmt_g - vr_mmt_a * xLen * 0.2;
        b.mh_pp[i] = VR_Melee_At(vr_mpt_f[i]);
        vr_mline_grip = xG;
        vr_mline_far = xG + xA * xLen;
        vr_mline_near = xG - xA * xLen * 0.2;
        b.mh_cp[i] = VR_Melee_At(vr_mpt_f[i]);
    }
    b.mh_pgrip = vr_mmt_g;
    b.mh_grip = xG;
    b.mh_far = xG + xA * xLen;
    b.mh_near = xG - xA * xLen * 0.2;
    b.mh_hd_pg = vr_mmt_g;
    b.mh_hd_pf = vr_mmt_g + vr_mmt_a * xLen;
    b.mh_hd_py = '0 0 1';
    b.mh_hd_cg = xG;
    b.mh_hd_cf = xG + xA * xLen;
    b.mh_hd_cy = '0 0 1';
    vector v = VR_Blow_ToPlaySpace(xVel);
    b.mh_vgrip = v;
    b.mh_vfar = v;
    b.mh_vnear = v;
    b.mh_vgrip_last = v;
    b.mh_vfar_last = v;
    b.mh_vnear_last = v;
    b.mh_dt = 1 / 72;
    vr_mmt_g = xG;
    vr_mmt_a = xA;
    if(vr_mmt_probeonly)
    {
        vr_msw_probe = TRUE; // (only what the sweep finds: vr_msw_*)
        VR_Melee_Sweep(h, b);
        vr_msw_probe = FALSE;
        return;
    }
    if(vr_mmt_skip > 0)
    {
        vr_mmt_skip = vr_mmt_skip - 1;
        return;
    }
    if(!VRGetMeleeHitSoundPlayed(h))
    {
        VR_Melee_HitClear(b);
    }
    b.mh_c_ent = world;
    b.mh_c_pos = '0 0 0';
    if(cvar("developer"))
    {
        vr_msw_probe = TRUE; // (what the sweep finds, armed or not)
        VR_Melee_Sweep(h, b);
        vr_msw_probe = FALSE;
        bprint(sprintf("meleemulti:   a frame (%g lines): the edge in %s (point %g), the wall by point %g;",
            vr_rib_n, vr_msw_edge.classname, vr_msw_edge_i, vr_msw_wall_i));
    }
    float struck = VR_Melee_StrikeAfter(h, b, VR_Melee_Sweep(h, b));
    if(cvar("developer"))
    {
        bprint(sprintf(" struck: %s (weapon %g, tracked %g)\n", struck ? b.mh_c_ent.classname : "nothing", self.weapon,
            VRHandsTracked(self)));
    }
    if(struck)
    {
        entity c = b.mh_c_ent;
        float hp = c.health;
        VR_Melee_DealAll(h);
        if(cvar("developer"))
        {
            bprint(sprintf("meleemulti:   dealt: landed %g, %g things, world %g; its health %g -> %g\n",
                VRGetMeleeHitSoundPlayed(h), b.mh_hit_n, b.mh_hit_world, hp, c.health));
        }
        if(!VR_Melee_Multi(b))
        {
            vr_mmt_skip = 1;
        }
    }
};

// A new swing (nothing struck in this motion), its first pose.
void(entity b, vector xG, vector xA) VR_MMT_Start =
{
    VRSetMeleeHitSoundPlayed(cVR_MainHand, FALSE);
    b.mh_hit_ent = world;
    b.mh_struck = world;
    VR_Melee_HitClear(b);
    b.mh_wall_time = -1;
    b.mh_wall_blow = -10;
    b.mh_wall_on = FALSE;
    b.mh_pend_ent = world;
    vr_mmt_skip = 0;
    vr_mmt_g = xG;
    vr_mmt_a = xA;
};

// What the swing struck: "meleemulti:" line; TRUE if `xA` (and `xB`, world: none) and the world (`xWall`) were.
float(string xName, entity b, entity xA, entity xB, float xWall) VR_MMT_Report =
{
    float ha = (xA == world) || VR_Melee_HitHas(b, xA);
    float hb = (xB == world) || VR_Melee_HitHas(b, xB);
    bprint(sprintf("meleemulti: %s multi %g: %g things struck, world %g;", xName, cvar("vr_melee_multi"), b.mh_hit_n,
        b.mh_hit_world));
    bprint(sprintf(" %s at %v %g", xA.classname, xA.origin, VR_Melee_HitHas(b, xA)));
    bprint(sprintf(", %s at %v %g\n", xB.classname, xB.origin, VR_Melee_HitHas(b, xB)));
    return ha && hb && (!xWall || b.mh_hit_world);
};

// The floor case: a chop at `m`'s limb.
float(entity p, entity b, entity m, float xLen) VR_MMT_Floor =
{
    float bone = VR_Limb_TestBone(m);
    if(bone < 0)
    {
        bprint(sprintf("meleemulti: floor: %s at %v has no limb to cut (ragdoll %g)\n", m.classname, m.origin, isragdoll(m)));
        return FALSE;
    }
    vector L = limbplace(m, bone, 3);
    traceline(L + '0 0 4', L - '0 0 64', TRUE, m);
    float zf = trace_endpos_z;
    // The tip comes down into the floor beside the limb (away from the body), the blade's grip end high over it; then
    // the blade turns down about its tip, its middle through the limb (4 units under its surface point).
    // Its way from the limb: of 16 round it, the first whose frame 1 meets the floor, ahead of the player (the floor
    // by the body is no blow: VR_MELEE_FLOOR_AHEAD), and not the body (its other limbs lie about).
    float half = xLen * 0.5;
    float dz2 = L_z - 4 - (zf - 2);
    float d = sqrt(max(1, half * half - dz2 * dz2));
    makevectors('0 1 0' * p.v_angle_y);
    vector fwd = v_forward;
    vector away = '0 0 0';
    vector T = '0 0 0';
    vector m1 = '0 0 0';
    vector a1 = '0 0 0';
    float k;
    vr_mmt_probeonly = TRUE;
    for(k = 0; k < 16; ++k)
    {
        makevectors('0 1 0' * (p.v_angle_y + k * 22.5));
        away = v_forward;
        T = L + away * d;
        T_z = zf - 2;
        m1 = T + (away * -0.5 + '0 0 0.866') * half; // (frame 1: the blade at 60 degrees)
        a1 = normalize(T - m1);
        if((T - p.origin) * fwd < 24)
        {
            continue;
        }
        VR_MMT_Start(b, T - a1 * xLen + '0 0 12', a1);
        VR_MMT_Frame(b, T - a1 * xLen, a1, xLen, '0 0 -6');
        if(vr_msw_wall_i >= 0 && vr_msw_edge == world)
        {
            break;
        }
    }
    vr_mmt_probeonly = FALSE;
    if(k >= 16)
    {
        bprint("meleemulti: floor: no way round the limb whose tip meets only the floor\n");
        return FALSE;
    }
    vector m2 = L - '0 0 4';
    vector a2 = normalize(T - m2);
    float cut0 = ragdolllimbs(m, 2);
    bprint(sprintf("meleemulti: floor: %s at %v (ragdoll %g) limb %g at %v, the floor %.1f below it; the tip into the "
        "floor %.1f from it\n", m.classname, m.origin, isragdoll(m), bone, L, L_z - zf, d));
    VR_MMT_Start(b, T - a1 * xLen + '0 0 12', a1);
    VR_MMT_Frame(b, T - a1 * xLen, a1, xLen, '0 0 -6');
    float world1 = b.mh_hit_world;
    float hit1 = VR_Melee_HitHas(b, m);
    // (Before: VR_Corpse_StrikeFrame's line from the hand to the tip, at frame 2's pose.)
    vr_mline_grip = T - a2 * xLen;
    vr_mline_far = T;
    vr_mline_near = vr_mline_grip - a2 * xLen * 0.2;
    vr_mline_free = FALSE;
    entity old = VR_Corpse_StrikeAlong(p, b.mh_thing, normalize(m2 - m1));
    bprint(sprintf("meleemulti: floor: the corpse line (as before) at frame 2 finds %s\n", old == world ? "nothing" : old.classname));
    VR_MMT_Frame(b, T - a2 * xLen, a2, xLen, normalize(m2 - m1) * 6);
    float cut = ragdolllimbs(m, 2) != cut0;
    bprint(sprintf("meleemulti: floor: frame 1: world %g, corpse %g; then limbs cut %g\n", world1, hit1, cut));
    return VR_MMT_Report("floor", b, m, world, TRUE) && world1 && !hit1 && cut;
};

// The two case: a level swing across `xA` and `xB` (across the line between them), the blade pointing past both.
float(entity p, entity b, entity xA, entity xB, float xLen) VR_MMT_Two =
{
    vector ca = xA.origin + '0 0 10';
    vector cb = xB.origin + '0 0 10';
    vector way = normalize(cb - ca); // (the swing's)
    way_z = 0;
    way = normalize(way);
    vector ax = normalize(VR_Melee_Cross(way, '0 0 1')); // the blade level, across the swing
    vector g = ca - way * 40 - ax * (xLen * 0.6);
    VR_MMT_Start(b, g, ax);
    float k;
    for(k = 1; k <= 12; ++k)
    {
        VR_MMT_Frame(b, g + way * (12 * k), ax, xLen, way * 6);
    }
    return VR_MMT_Report("two", b, xA, xB, FALSE);
};

// The wall case: `xW` within reach of a wall facing `xN` at distance `xD` from its middle.
float(entity p, entity b, entity xW, vector xN, float xD, float xLen) VR_MMT_Wall =
{
    vector c = xW.origin + '0 0 10';
    vector ax = xN * -1;
    vector way = normalize(VR_Melee_Cross(xN, '0 0 1')); // (along the wall)
    vector g0 = c + ax * (xD - 4 - xLen) - way * 46;
    VR_MMT_Start(b, g0, ax);
    // Into the wall (6 units, the tip 2 past it) and along it, 8 units a frame, across the monster.
    vector g = g0 + ax * 6 + way * 6;
    VR_MMT_Frame(b, g, ax, xLen, (ax + way) * 4);
    float world1 = b.mh_hit_world;
    float k;
    for(k = 1; k <= 11; ++k)
    {
        VR_MMT_Frame(b, g + way * (8 * k), ax, xLen, way * 6);
    }
    bprint(sprintf("meleemulti: wall: the wall %.1f from its middle; frame 1 world %g\n", xD, world1));
    return VR_MMT_Report("wall", b, xW, world, TRUE) && world1;
};

void(entity p) VR_Melee_MultiTest =
{
    float thing = VR_Melee_Thing(p, cVR_MainHand);
    if(!VR_Melee_Line(p, cVR_MainHand, thing) || thing == VR_MTHING_FIST || thing == VR_MTHING_NONE)
    {
        bprint(sprintf("meleemulti: the main hand holds no weapon (thing %g, %s): FAIL\n", thing, p.weaponmodel));
        return;
    }
    float len = vlen(vr_mline_far - vr_mline_grip);
    entity was = self;
    self = p; // (VR_Blow: self's)
    entity b = VR_Blow(cVR_MainHand);
    b.mh_thing = thing; // (the mock's hands may be untracked: VR_Melee_Track left none)
    b.mh_free = FALSE;
    // The scene's things: the corpse nearest, the two live monsters nearest each other, the live one nearest a wall.
    entity corpse = world;
    entity e;
    float best = 100000;
    for(e = nextent(world); e; e = nextent(e))
    {
        if((e.flags & FL_MONSTER) && e.health <= 0 && !e.vr_gib && vlen(e.origin - p.origin) < best)
        {
            best = vlen(e.origin - p.origin);
            corpse = e;
        }
    }
    entity ta = world;
    entity tb = world;
    best = 100000;
    entity f;
    for(e = nextent(world); e; e = nextent(e))
    {
        if(!(e.flags & FL_MONSTER) || e.health <= 0)
        {
            continue;
        }
        for(f = nextent(e); f; f = nextent(f))
        {
            if((f.flags & FL_MONSTER) && f.health > 0 && vlen(f.origin - e.origin) < best)
            {
                best = vlen(f.origin - e.origin);
                ta = e;
                tb = f;
            }
        }
    }
    entity tw = world;
    vector wn = '0 0 0';
    float wd = 100000;
    for(e = nextent(world); e; e = nextent(e))
    {
        if(!(e.flags & FL_MONSTER) || e.health <= 0 || e == ta || e == tb)
        {
            continue;
        }
        float k;
        for(k = 0; k < 4; ++k)
        {
            vector d = '1 0 0';
            if(k == 1) d = '-1 0 0';
            if(k == 2) d = '0 1 0';
            if(k == 3) d = '0 -1 0';
            traceline(e.origin + '0 0 10', e.origin + '0 0 10' + d * 48, TRUE, e);
            if(trace_fraction < 1 && 48 * trace_fraction < wd)
            {
                wd = 48 * trace_fraction;
                wn = trace_plane_normal;
                tw = e;
            }
        }
    }
    bprint(sprintf("meleemulti: %s (thing %g), its line %.1f units; corpse at %v,", p.weaponmodel, thing, len, corpse.origin));
    bprint(sprintf(" two at %v and %v (%.0f apart), by the wall at %v\n", ta.origin, tb.origin, best, tw.origin));
    if(ta != world) { ta.health = 5000; }
    if(tb != world) { tb.health = 5000; }
    if(tw != world) { tw.health = 5000; }
    float multi = cvar("vr_melee_multi");
    float pass = TRUE;
    float on;
    for(on = 0; on < 2; ++on)
    {
        cvar_set("vr_melee_multi", ftos(on));
        vr_msw_decide = 1.5;
        float ok;
        if((corpse != world) && on)
        {
            ok = VR_MMT_Floor(p, b, corpse, len);
            bprint(sprintf("meleemulti: floor %s\n", ok ? "PASS" : "FAIL"));
            pass = (pass && ok);
        }
        if((ta != world) && (tb != world))
        {
            ok = VR_MMT_Two(p, b, ta, tb, len);
            if(on)
            {
                bprint(sprintf("meleemulti: two %s\n", ok ? "PASS" : "FAIL"));
                pass = (pass && ok);
            }
        }
        if(tw != world)
        {
            ok = VR_MMT_Wall(p, b, tw, wn, wd, len);
            if(on)
            {
                bprint(sprintf("meleemulti: wall %s\n", ok ? "PASS" : "FAIL"));
                pass = (pass && ok);
            }
        }
        vr_msw_decide = 0;
    }
    if((corpse == world) || (ta == world) || (tw == world))
    {
        pass = FALSE;
        bprint("meleemulti: the scene lacks a corpse, two live monsters or one by a wall (vr_scenario sword_multi)\n");
    }
    cvar_set("vr_melee_multi", ftos(multi));
    VRSetMeleeHitSoundPlayed(cVR_MainHand, FALSE);
    VR_Melee_HitClear(b);
    b.mh_hit_ent = world;
    self = was;
    bprint(sprintf("meleemulti: %s\n", pass ? "PASS" : "FAIL"));
};

// vr_melee_sweep_test 5 (the blade's line start, ROUND22.md "The blade's line start"): the main hand's weapon's line with
// its own ends (Blade Org, Blade Tip) against the default line (the hand to the muzzle). "bladeline:" lines: the line's
// start and tip off the hand and the muzzle (the hand's frame), each layout point's distance from the line start .. tip
// (0: on the new line); then a small box at the striking point the ends moved most, swept across (24 units in one frame,
// square to the line and to that move) by the line as it is and by the default one. PASS: every point on the new line,
// the new line strikes the box, and the default line strikes it only if it reaches it (as test 4's).
float(entity b, entity xT, float xThing, vector xGrip, vector xFar, vector xD) VR_MSWT_EndsProbe =
{
    vector g = vr_mline_grip, f = vr_mline_far;
    float bend = vr_mline_bend;
    vr_mline_grip = xGrip;
    vr_mline_far = xFar;
    vr_mline_bend = 0;
    VR_Melee_Layout(xThing);
    VR_Melee_LayoutFrom(vr_mline_from_x);
    b.mh_thing = xThing;
    b.mh_free = FALSE;
    b.mh_struck = world;
    b.mh_hit_ent = world;
    b.mh_n = vr_mpt_n;
    b.mh_bwidth = vr_mline_shape_y;
    float i;
    for(i = 0; i < vr_mpt_n; ++i)
    {
        b.mh_pp[i] = VR_Melee_At(vr_mpt_f[i]);
        b.mh_cp[i] = b.mh_pp[i] + xD;
    }
    b.mh_pgrip = xGrip;
    b.mh_grip = xGrip + xD;
    b.mh_far = xFar + xD;
    b.mh_hd_lo = b.mh_hd_hi = '0 0 0';
    vr_mline_grip = g;
    vr_mline_far = f;
    vr_mline_bend = bend;
    float was = cvar("vr_melee_swept");
    cvar_set("vr_melee_swept", "1");
    vr_msw_probe = TRUE;
    VR_Melee_Sweep(cVR_MainHand, b);
    vr_msw_probe = FALSE;
    cvar_set("vr_melee_swept", ftos(was));
    return vr_msw_edge == xT;
};

void(entity p) VR_Melee_LineEndsTest =
{
    entity was = self;
    self = p;
    float thing = VR_Melee_Thing(p, cVR_MainHand);
    if(!VR_Melee_Line(p, cVR_MainHand, thing) || thing == VR_MTHING_FIST || thing == VR_MTHING_NONE || vr_mline_free)
    {
        bprint(sprintf("bladeline: the main hand holds no weapon by its handle (thing %g, %s): FAIL\n", thing,
            p.weaponmodel));
        self = was;
        return;
    }
    vector h = VRGetEntHandPos(p, cVR_MainHand);
    vector m = p.muzzlepos;
    vector grip = vr_mline_grip, far = vr_mline_far;
    makevectors(VRGetEntHandRot(p, cVR_MainHand));
    vector fwd = v_forward, rt = v_right, up = v_up;
    vector lg, lt;
    lg_x = (grip - h) * fwd; lg_y = (grip - h) * rt; lg_z = (grip - h) * up;
    lt_x = (far - m) * fwd; lt_y = (far - m) * rt; lt_z = (far - m) * up;
    bprint(sprintf("bladeline: %s slot %g: line start %v off the hand, tip %v off the muzzle, line %.2f units (default "
        "%.2f)\n", p.weaponmodel, vr_mline_slot, lg, lt, vlen(far - grip), vlen(m - h)));
    // Every layout point on the new line (its curve and blade start's bend aside: a straight weapon's).
    VR_Melee_Layout(thing);
    VR_Melee_LayoutFrom(vr_mline_from_x);
    float i;
    float worst = 0;
    float best = -1;
    float bestF = 0;
    float bestMove = 0;
    for(i = 0; i < vr_mpt_n; ++i)
    {
        vector at = VR_Melee_At(vr_mpt_f[i]);
        float off = VR_SegmentDistance(at, at, vr_mpt_f[i] < 0 ? vr_mline_near : grip, far);
        worst = max(worst, off);
        vector was0 = h + (m - h) * vr_mpt_f[i];
        bprint(sprintf("bladeline: pt %g f %.2f off the new line %.3f, moved %.2f from the default line's\n", i,
            vr_mpt_f[i], off, vlen(at - was0)));
        if(VR_Melee_Strikes(vr_mpt_kind[i]) && vr_mpt_f[i] > 0 && vlen(at - was0) > bestMove)
        {
            bestMove = vlen(at - was0);
            best = i;
            bestF = vr_mpt_f[i];
        }
    }
    float onLine = worst < 0.05 || vr_mline_bend;
    if(best < 0)
    {
        bprint(sprintf("bladeline: no striking point moved (the default ends; points on the line %g): %s\n", onLine,
            onLine ? "PASS" : "FAIL"));
        self = was;
        return;
    }
    float rBase = VR_MELEE_RADIUS * max(0.25, cvar_hget(cvarh_vr_melee_range_multiplier)) * vr_mline_shape_y;
    vector pt = VR_Melee_At(bestF);
    vector moved = pt - (h + (m - h) * bestF);
    // (The box one swept radius off the new line, on the side away from the default one: within the new line's reach,
    // out of the default's once the ends moved the point more than the box's half.)
    vector ax = normalize(far - grip);
    vector out = moved - ax * (moved * ax);
    if(vlen(out) > 0.01)
    {
        pt = pt + normalize(out) * rBase;
    }
    vector across = VR_Melee_Cross(ax, moved);
    if(vlen(across) < 0.01)
    {
        across = up;
    }
    vector d = normalize(across) * 24;
    entity b = spawn();
    entity t = spawn();
    t.classname = "vr_meleesweep_target";
    t.solid = SOLID_BBOX;
    t.movetype = MOVETYPE_NONE;
    t.takedamage = DAMAGE_AIM;
    t.health = 1000;
    setsize(t, '-1 -1 -1', '1 1 1');
    setorigin(t, pt + d * 0.5);
    float now = VR_MSWT_EndsProbe(b, t, thing, grip, far, d);
    float def = VR_MSWT_EndsProbe(b, t, thing, h, m, d);
    float apart = VR_SegmentDistance(pt, pt, h, m);
    float reaches = apart < rBase + 1;
    float ok = onLine && now && (def == reaches || fabs(apart - rBase - 1) < 0.5);
    bprint(sprintf("bladeline: points on the new line %g (worst %.3f); a box by point %g (%.2f along)\n", onLine, worst,
        best, bestF));
    bprint(sprintf("bladeline: the box %.2f units off the default line: struck by the new %g, the default %g: %s\n",
        apart, now, def, ok ? "PASS" : "FAIL"));
    self = was;
    remove(t);
    remove(b);
};

// Every server frame (StartFrame): the test asked for (vr_melee_sweep_test: 1 the sweep, 2 the blade's dump, 3 its shape,
// 4 the fist's line, 5 the line's own ends, 6 one swing, many things).
void() VR_Melee_SweepTestFrame =
{
    float mode = cvar("vr_melee_sweep_test");
    if(!mode)
    {
        return;
    }
    cvar_set("vr_melee_sweep_test", "0");
    entity p = find(world, classname, "player");
    if(p == world)
    {
        return;
    }
    if(mode == 2)
    {
        VR_BladeDump(p);
    }
    else if(mode == 3)
    {
        VR_Melee_BladeShapeTest(p);
    }
    else if(mode == 4)
    {
        VR_Melee_FistLineTest(p);
    }
    else if(mode == 5)
    {
        VR_Melee_LineEndsTest(p);
    }
    else if(mode == 6)
    {
        VR_Melee_MultiTest(p);
    }
    else
    {
        VR_Melee_SweepTest(p);
    }
};
#endif
