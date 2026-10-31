#ifdef QVR_TESTS
//
//
//
// ----------------------------------------------------------------------------
// vr_dirparry_test.qc -- `vr_agrunt_test 78` (Debug > Tests > Armed Grunts: Directional Parry): the directional parry
// (vr_dirparry.qc; ROUND23.md, "Directional parry"). In vrtesthall, you unseen; for each case a fresh sword grunt 36
// units ahead, held, then its swing forced (an overhead or a horizontal one, its blade as drawn), you holding a weapon
// still in your main hand (the hand, its turn and its muzzle set each frame: the weapon's own line from its hand to its
// far end, measured in the hand's frame from the tracked pose). Your weapon's middle is put where its blade was at its
// blow in that kind's first swing (cases 0 and 1: your fists, nothing parries: the reference, the blow lands):
//   2 overhead vs your sword held level across: parried (no damage, sparks within 2 units of both lines);
//   3 overhead vs your sword upright: the blow lands;  4 horizontal vs upright: parried;  5 horizontal vs level across:
//   lands;  6 overhead vs a shotgun's barrel level across: parried;  7 overhead vs the sword level across 40 units to
//   the side (out of reach): lands;  8 vr_dirparry 0, horizontal vs level across: the level-across rule as before (its
//   own verdict at the blow), no directional reading.
// Each `agtest: PASS|FAIL dirparry <case>: ...`; ends on `agtest: test 78 over: P passed, F failed`.
// ----------------------------------------------------------------------------

float VR_DRPT_CASES = 9;
float vr_drpt_case;      // the case running
float vr_drpt_until;     // its give-up time (a safety net: FAIL "timed out")
float vr_drpt_keep_on;   // cvars before (put back at the end)
float vr_drpt_keep_parry;
vector vr_drpt_r0a;      // the reference swings' blade at their blow: overhead
vector vr_drpt_r0b;
vector vr_drpt_r2a;      // ... horizontal
vector vr_drpt_r2b;
vector vr_drpt_lm;       // your weapon's muzzle (its far end) in your hand's frame
vector vr_drpt_lg;       // ... its parry line's near end
vector vr_drpt_lf;       // ... its far end
vector vr_drpt_rot;      // your hand's turn that lays it along the case's axis
vector vr_drpt_center;   // ... its middle's place
vector vr_drpt_axis;     // ... its axis
vector vr_drpt_home;     // where you stand for every case
vector vr_drpt_hand;     // your hand's place as posed
float vr_drpt_posed;     // the pose is held (its turn found)
float vr_drpt_wid;       // the weapon in your main hand
float vr_drpt_refd;      // the reference swing's blade recorded
float vr_drpt_t0;        // the swing's start
float vr_drpt_ev0;       // counters at the swing's start
float vr_drpt_pr0;
float vr_drpt_fx0;

string(float c) VR_DRPT_Name =
{
    if(c == 0) { return "overhead vs your fists (reference)"; }
    if(c == 1) { return "horizontal vs your fists (reference)"; }
    if(c == 2) { return "overhead vs your sword level across"; }
    if(c == 3) { return "overhead vs your sword upright"; }
    if(c == 4) { return "horizontal vs your sword upright"; }
    if(c == 5) { return "horizontal vs your sword level across"; }
    if(c == 6) { return "overhead vs a shotgun's barrel level across"; }
    if(c == 7) { return "overhead vs your sword level across, 40 units to the side"; }
    return "vr_dirparry 0: horizontal vs your sword level across (the old rule)";
};

float(float c) VR_DRPT_Kind = { return (c == 0 || c == 2 || c == 3 || c == 6 || c == 7) ? 0 : 2; };
float(float c) VR_DRPT_Wants = { return (c == 2) || (c == 4) || (c == 6); };

vector(vector d, vector ang) VR_DRPT_Local =
{
    makevectors(ang);
    return [d * v_forward, d * v_right, d * v_up];
};

vector(vector l, vector ang) VR_DRPT_World =
{
    makevectors(ang);
    return v_forward * l_x + v_right * l_y + v_up * l_z;
};

// The hand's turn laying its weapon's line along `axis` (either way): the best of a 15-degree grid, then 3 degrees round it.
vector(vector axis) VR_DRPT_Solve =
{
    vector dl = normalize(vr_drpt_lf - vr_drpt_lg);
    float best = -1;
    vector ba = '0 0 0';
    vector a = '0 0 0';
    float step = 15;
    for(float pass = 0; pass < 2; pass++)
    {
        vector c = ba;
        float span = pass == 0 ? 0 : 12;
        float lo = pass == 0 ? -90 : -span;
        for(float x = lo; x <= (pass == 0 ? 90 : span); x += step)
        {
            for(float y = (pass == 0 ? 0 : -span); y <= (pass == 0 ? 345 : span); y += step)
            {
                for(float z = (pass == 0 ? 0 : -span); z <= (pass == 0 ? 345 : span); z += step)
                {
                    a = pass == 0 ? [x, y, z] : c + [x, y, z];
                    float d = fabs(normalize(VR_DRPT_World(dl, a)) * axis);
                    if(d > best)
                    {
                        best = d;
                        ba = a;
                    }
                }
            }
        }
        step = 3;
    }
    return ba;
};

// Your main hand held in the case's pose this frame (the tracking's put back each frame by the engine).
void(entity p) VR_DRPT_Apply =
{
    vector g = VR_DRPT_World(vr_drpt_lg, vr_drpt_rot);
    vector f = VR_DRPT_World(vr_drpt_lf, vr_drpt_rot);
    vector m = VR_DRPT_World(vr_drpt_lm, vr_drpt_rot);
    p.handrot = vr_drpt_rot;
    p.handpos = vr_drpt_center - (g + f) * 0.5;
    vr_drpt_hand = p.handpos;
    p.muzzlepos = p.handpos + m;
    p.handvel = '0 0 0';
};

void() VR_DRPT_End =
{
    entity p = vr_agt_player;
    cvar_set("vr_dirparry", ftos(vr_drpt_keep_on));
    cvar_set("vr_agrunt_parry", ftos(vr_drpt_keep_parry));
    localcmd("vr_mock_button main grip 0\n");
    VR_AGPT_Clear();
    VR_AGPT_EmptyHand(p);
    p.flags = p.flags - (p.flags & FL_NOTARGET);
    vr_drpt_case = 0;
    cvar_set("vr_agrunt_test", "0");
};

// The case's outcome after its swing.
void(entity g, float c) VR_DRPT_Judge =
{
    entity p = vr_agt_player;
    float hurt = 1000 - p.health;
    float evals = vr_drp_evals - vr_drpt_ev0;
    float parries = vr_drp_parries - vr_drpt_pr0;
    float fx = vr_drp_fx_n - vr_drpt_fx0;
    float want = VR_DRPT_Wants(c);
    float ok;
    string got = sprintf("hurt %g, directional parries %g (early %g), sparks %g, readings %g", hurt, parries, vr_drp_early,
        fx, evals);
    if(vr_drp_hand >= 0)
    {
        got = sprintf("%s; last: score %.2f, %.0f deg off square, gap %.1f", got, vr_drp_score, vr_drp_off, vr_drp_gap);
        float al = fabs(normalize(vr_drp_pb - vr_drp_pa) * vr_drpt_axis);
        float mid = vlen((vr_drp_pa + vr_drp_pb) * 0.5 - vr_drpt_center);
        got = sprintf("%s; your line along the axis %.3f, its middle %.1f units off (%.1f from you; your hand %.1f off its pose)", got, al, mid,
            vlen(vr_drpt_center - p.origin), vlen(p.handpos - vr_drpt_hand));
        vector tf = p.origin - g.origin;
        tf_z = 0;
        tf = normalize(tf);
        vector tl = VR_Cross('0 0 1', tf); // (its left)
        vector bx = normalize(vr_drp_eb - vr_drp_ea);
        got = sprintf("%s; (towards you, its left, up) its blade [%.2f %.2f %.2f]", got, bx * tf, bx * tl, bx_z);
        got = sprintf("%s, its way [%.2f %.2f %.2f] (%.2f s)", got, vr_drp_sweep * tf, vr_drp_sweep * tl, vr_drp_sweep_z,
            vr_drp_tau);
        vector yl = normalize(vr_drp_pb - vr_drp_pa);
        got = sprintf("%s, your line [%.2f %.2f %.2f]", got, yl * tf, yl * tl, yl_z);
    }
    if(c == 8)
    {
        // (the old rule: no directional reading; its blow parried or not by the level-across test, as before)
        ok = ((evals == 0) && (parries == 0) && (hurt > 0));
        got = sprintf("%s; the level-across rule %s", got, vr_parry_old_hand >= 0 ? "parried (its damage cut)" :
            "did not parry");
    }
    else if(want)
    {
        float dp = VR_AG_SegDist(vr_drp_fx_at, vr_drp_fx_at, vr_drp_pa, vr_drp_pb);
        float de = VR_AG_SegDist(vr_drp_fx_at, vr_drp_fx_at, vr_drp_ea, vr_drp_eb);
        float half = max(0.5, cvar("vr_dirparry_contact")) * 0.5 + 0.01; // (the meeting: the middle of the nearest points)
        ok = ((hurt == 0) && (parries == 1) && (fx == 1) && (vlen(vr_drp_fx_at - vr_drp_at) <= 2) && (dp <= half) && (de <= half));
        got = sprintf("%s; sparks %.1f units off your line, %.1f off its blade", got, dp, de);
    }
    else
    {
        ok = ((hurt > 0) && (parries == 0) && (fx == 0) && ((c < 2) || (evals > 0)));
    }
    VR_AGT_Check(sprintf("dirparry %s: %s; expected %s", VR_DRPT_Name(c), got, want ? "parried, no damage, sparks at "
        "the meeting" : c == 8 ? "the old rule" : "the blow lands"), ok);
};

void() VR_DRPT_Frame =
{
    entity p = vr_agt_player;
    entity g = vr_agt_grunt;
    float c = vr_drpt_case;
    if(vr_agt_step == 0)
    {
        if(c == 0)
        {
            vr_drpt_keep_on = cvar("vr_dirparry");
            vr_drpt_keep_parry = cvar("vr_agrunt_parry");
            cvar_set("vr_agrunt_parry", "0"); // (its own guard and parries out of it: its swing as forced)
            localcmd("vr_mock_button main grip 1\n"); // (your weapon held: not let go of)
            p.flags = p.flags | FL_NOTARGET;
            vr_drpt_home = p.origin;
        }
        setorigin(p, vr_drpt_home); // (where the reference swings found you: a blow's knockback undone)
        p.velocity = '0 0 0';
        if(c >= VR_DRPT_CASES)
        {
            VR_DRPT_End();
            return;
        }
        cvar_set("vr_dirparry", c == 8 ? "0" : "1");
        VR_AGPT_Clear();
        VR_AGPT_EmptyHand(p);
        vr_drpt_wid = c < 2 ? WID_FIST : c == 6 ? WID_SHOTGUN : WID_SWORD;
        VR_ItemUtil_EntSetItem(p, IID_SWORD, TRUE); // (owned: kept in your hand)
        VR_ItemUtil_EntSetItem(p, IID_SHOTGUN, TRUE);
        p.weapon = vr_drpt_wid;
        vr_drpt_posed = FALSE;
        vr_drpt_until = time + 5;
        vr_agt_step = 1;
        return;
    }
    if(vr_agt_step == 1)
    {
        // Your weapon's line (VR_DRP_PlayerSeg: VR_Melee_Line's, its Edit Blade Shape ends) measured in your hand's
        // frame, its pose for the case found and held from now on; the grunt spawned. (The headless mock places no
        // weapon's muzzle: its muzzle is put along the hand's forward, a sword's 24 units, a shotgun's 20.)
        if(c >= 2)
        {
            vector h = p.handpos;
            vector r = p.handrot;
            vr_drpt_lm = vr_drpt_wid == WID_SHOTGUN ? '20 0 0' : '24 0 0';
            p.muzzlepos = h + VR_DRPT_World(vr_drpt_lm, r);
            if(p.weapon != vr_drpt_wid || !VR_DRP_PlayerSeg(p, cVR_MainHand) || vlen(vr_drp_pb - vr_drp_pa) < 10)
            {
                if(time > vr_drpt_until)
                {
                    VR_AGT_Check(sprintf("dirparry %s: timed out (weapon %g, want %g, its line %.1f units)",
                        VR_DRPT_Name(c), p.weapon, vr_drpt_wid, vlen(vr_drp_pb - vr_drp_pa)), FALSE);
                    vr_agt_step = 4;
                }
                return;
            }
            vr_drpt_lg = VR_DRPT_Local(vr_drp_pa - h, r);
            vr_drpt_lf = VR_DRPT_Local(vr_drp_pb - h, r);
            float k = VR_DRPT_Kind(c);
            vector ra = k == 0 ? vr_drpt_r0a : vr_drpt_r2a;
            vector rb = k == 0 ? vr_drpt_r0b : vr_drpt_r2b;
            makevectors(p.angles); // (the grunt spawns ahead of you: VR_AGT_Spawn)
            vector toG = v_forward;
            toG_z = 0;
            vector side = normalize(VR_Cross(normalize(toG), '0 0 1'));
            float upright = (c == 3) || (c == 4);
            vr_drpt_axis = upright ? '0 0 1' : side;
            vr_drpt_center = ra + (rb - ra) * 0.6 + (c == 7 ? side * 40 : '0 0 0');
            vr_drpt_rot = VR_DRPT_Solve(vr_drpt_axis);
            dprint(sprintf("dirparry test: weapon %g, its line in the hand %v .. %v, the turn %v, its middle at %v\n",
                p.weapon, vr_drpt_lg, vr_drpt_lf, vr_drpt_rot, vr_drpt_center));
            vr_drpt_posed = TRUE;
            VR_DRPT_Apply(p);
        }
        vr_agt_grunt = VR_AGT_Spawn(WID_SWORD, 60);
        vr_agt_step = 2;
        return;
    }
    if(vr_drpt_posed)
    {
        VR_DRPT_Apply(p);
    }
    if(vr_agt_step == 2)
    {
        // Its start done, its weapon placed and drawn: held, then its swing forced at you.
        if(!(g.takedamage && g.vr_ag_prop && g.vr_ag_prop.vr_ag_was_n >= 1 && weapondrawnpose(g.vr_ag_prop, 0, 31) != '0 0 0'))
        {
            if(time > vr_drpt_until)
            {
                VR_AGT_Check(sprintf("dirparry %s: timed out (takedamage %g, weapon %g)", VR_DRPT_Name(c), g.takedamage,
                    g.vr_ag_prop != world), FALSE);
                vr_agt_step = 4;
            }
            return;
        }
        VR_AGST_Hold(g, 0);
        p.health = 1000;
        p.armorvalue = 0;
        vr_drpt_ev0 = vr_drp_evals;
        vr_drpt_pr0 = vr_drp_parries;
        vr_drpt_fx0 = vr_drp_fx_n;
        vr_drp_hand = -1;
        vr_parry_old_hand = -1;
        entity was = self;
        self = g;
        g.enemy = p;
        VR_AGP_Begin(VR_AGP_SWING);
        g.vr_ag_act_kind = VR_DRPT_Kind(c);
        g.vr_ag_act_hand = 0;
        self = was;
        vr_drpt_t0 = time;
        vr_drpt_refd = FALSE;
        vr_agt_step = 3;
        return;
    }
    if(vr_agt_step == 3)
    {
        // The reference swings: its blade as drawn three quarters into its strike (coming onto you, past its blow).
        VR_AGP_Times(g);
        if(c < 2 && !vr_drpt_refd && time - vr_drpt_t0 >= vr_agp_tw + vr_agp_ts * 0.75)
        {
            vr_drpt_refd = TRUE;
            VR_AG_WeaponLine(g.vr_ag_prop);
            if(c == 0)
            {
                vr_drpt_r0a = vr_ag_wa;
                vr_drpt_r0b = vr_ag_wb;
            }
            else
            {
                vr_drpt_r2a = vr_ag_wa;
                vr_drpt_r2b = vr_ag_wb;
            }
        }
        // Its swing's whole length (wind-up, strike, recovery) gone by: judged.
        if(time - vr_drpt_t0 < VR_AGP_Length(g) + 0.05)
        {
            return;
        }
        VR_DRPT_Judge(g, c);
        vr_agt_step = 4;
        return;
    }
    // (step 4) the next case
    if(g)
    {
        VR_AGT_Remove(g);
    }
    vr_agt_grunt = world;
    vr_drpt_posed = FALSE;
    vr_drpt_case = vr_drpt_case + 1;
    vr_agt_step = 0;
};
#endif
