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
    vector hlo = '0 0 0';
    vector hhi = '0 0 0';
    float hk = VR_ADMelee_Kind(VRGetEntWeaponFlags(p, cVR_MainHand));
    if(thing == VR_MTHING_AXE && hk > 0 && VR_ADMelee_Blunt(hk))
    {
        hlo = admeleehead(hk, 0);
        hhi = admeleehead(hk, 1);
    }
    VR_Melee_Layout(thing);
    float r = VR_MELEE_RADIUS * max(0.25, cvar_hget(cvarh_vr_melee_range_multiplier));
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

// Every server frame (StartFrame): the test asked for (vr_melee_sweep_test).
void() VR_Melee_SweepTestFrame =
{
    if(!cvar("vr_melee_sweep_test"))
    {
        return;
    }
    cvar_set("vr_melee_sweep_test", "0");
    entity p = find(world, classname, "player");
    if(p != world)
    {
        VR_Melee_SweepTest(p);
    }
};
