#ifdef QVR_TESTS
// ============================================================================
// vr_armedgrunt_expose_test.qc: `vr_agrunt_test 76` (Debug > Tests > Armed Grunts: Aim Behind Cover)
// ============================================================================
// Aimed at what it can hit (vr_agrunt_aim_exposed; vr_armedgrunt.qc VR_AG_Expose). vrtesthall's start: an armed grunt
// 300 units ahead hunting you (on the floor below the start's step), both held in place, its pauses between attacks
// 0.3 s at most (more shots); a box (vr_brush_box, a brush as the map's walls are) 40 units before you: none (you in the
// open), a low wall (to 14 over your origin: your head and shoulders over it), a door frame (your left half behind it,
// its edge 1 unit left of your middle), a crate (40 wide, to 17 over your origin: 9 of your box over it, your eyes too),
// a wall (all of you behind it). Each case for each gun (shotgun, nailgun, the enforcer's rifle, the revolver, the
// rocket launcher) twice, vr_agrunt_aim_exposed 0 (before) and 1, its normal aim error and spread: 16 shots, or 10 s;
// its shots, its hits on you (damage events), the attacks it held (nothing of you its gun's line reached) and the parts
// it aimed at (as it sees you: shoulderL its left, your right). Checked: in the open the same hits (within 20%) and its
// aim on your chest (70% of shots at least); behind the low wall, the door frame and the crate a quarter of its shots
// at least, and no more than 25% fewer than before (the door frame's is near the old: noise); behind the wall no shots.
// The looks' traces per armed grunt per second printed (the cost). Run with sv_random_seed for the same numbers.

float VR_AGX_GUNS = 5;
float VR_AGX_CASES = 5;
float VR_AGX_SHOTS = 16;
float VR_AGX_SECS = 10;
float vr_agx_w;
float vr_agx_case;
float vr_agx_mode;
float vr_agx_t0;
float vr_agx_t1;
float vr_agx_shots0;
float vr_agx_hits0;
float vr_agx_held0;
float vr_agx_tr0;
float vr_agx_keep;
float vr_agx_old_shots; // the case's run before (vr_agrunt_aim_exposed 0)
float vr_agx_old_hits;
float vr_agx_tr_n;      // the new runs' traces, their seconds
float vr_agx_tr_secs;
vector vr_agx_start;
vector vr_agx_fw;
vector vr_agx_side;
vector vr_agx_spot;
float vr_agx_xs0[14];

float(float i) VR_AGX_Wid =
{
    if(i == 0) { return WID_SHOTGUN; }
    if(i == 1) { return WID_NAILGUN; }
    if(i == 2) { return WID_ENFRIFLE; }
    if(i == 3) { return WID_REVOLVER; }
    return WID_ROCKET_LAUNCHER;
};

string(float c) VR_AGX_CaseName =
{
    if(c == 0) { return "in the open"; }
    if(c == 1) { return "low wall"; }
    if(c == 2) { return "door frame"; }
    if(c == 3) { return "crate"; }
    return "behind a wall";
};

string(float i) VR_AGX_PartName =
{
    if(i == 1) { return "chest"; }
    if(i == 2) { return "head"; }
    if(i == 3) { return "belly"; }
    if(i == 4) { return "shoulderL"; }
    if(i == 5) { return "shoulderR"; }
    if(i == 6) { return "headL"; }
    if(i == 7) { return "headR"; }
    if(i == 8) { return "hipL"; }
    if(i == 9) { return "hipR"; }
    if(i == 10) { return "legs"; }
    if(i == 11) { return "floor"; }
    if(i == 12) { return "floorL"; }
    if(i == 13) { return "floorR"; }
    return "none";
};

vector vr_agx_boxc; // the last box's probe point: in its left half (15 to your left), over its top

// The probe's line: down through the last box (a brush there: made, not yet removed).
float() VR_AGX_Blocked =
{
    traceline(vr_agx_boxc, vr_agx_boxc - '0 0 200', TRUE, vr_agt_player);
    return trace_ent.classname == "func_qvr_brush";
};

// The case's box (a brush): `lo`..`hi` to your right (side), `top` over your origin, from under the lower floor's (yours
// or the grunt's); 8 thick, `depth` before you.
void(float lo, float hi, float top, float depth) VR_AGX_Box =
{
    vector c = vr_agx_start + vr_agx_fw * depth;
    c_z = min(vr_agx_start_z, vr_agx_spot_z) + vr_agt_player.mins_z - 1;
    vector a = c - vr_agx_fw * 4 + vr_agx_side * lo;
    vector b = c + vr_agx_fw * 4 + vr_agx_side * hi;
    b_z = vr_agx_start_z + top;
    vr_agx_boxc = c - vr_agx_side * 15;
    vr_agx_boxc_z = b_z + 40;
    localcmd(sprintf("vr_brush_box %v %v\n", a, b));
};

// A projectile of `g`'s still flying.
float(entity g) VR_AGX_InFlight =
{
    entity e = nextent(world);
    while(e)
    {
        if(e.owner == g && (e.movetype == MOVETYPE_FLYMISSILE || e.movetype == MOVETYPE_FLY || e.movetype == MOVETYPE_BOUNCE))
        {
            return TRUE;
        }
        e = nextent(e);
    }
    return FALSE;
};

void() VR_AGX_End =
{
    VR_AGPT_Clear();
    localcmd("vr_brush_delete all\n");
    setorigin(vr_agt_player, vr_agx_start);
    cvar_set("vr_agrunt_aim_exposed", ftos(vr_agx_keep));
    VR_AGT_Print(sprintf("expose: the looks' traces %.1f a second per armed grunt (%g in %.1f s)",
        vr_agx_tr_n / max(0.1, vr_agx_tr_secs), vr_agx_tr_n, vr_agx_tr_secs));
    cvar_set("vr_agrunt_test", "0");
};

void() VR_AGX_Frame =
{
    entity p = vr_agt_player;
    entity g = vr_agt_grunt;
    float i;
    if(vr_agt_step == 0)
    {
        vr_agx_start = p.origin;
        makevectors(p.angles);
        vr_agx_fw = v_forward;
        vr_agx_fw_z = 0;
        vr_agx_fw = normalize(vr_agx_fw);
        vr_agx_side_x = vr_agx_fw_y;
        vr_agx_side_y = -vr_agx_fw_x;
        vr_agx_side_z = 0;
        vr_agx_keep = cvar("vr_agrunt_aim_exposed");
        vr_agx_w = 0;
        vr_agx_case = 0;
        vr_agx_mode = 0;
        vr_agx_tr_n = 0;
        vr_agx_tr_secs = 0;
        VR_AGT_Print(sprintf("expose: yaw %.1f, your box %v..%v, eyes %.1f, headset %.1f over your origin, kind %g", p.angles_y,
            p.mins, p.maxs, p.view_ofs_z, p.headpos != '0 0 0' ? p.headpos_z - p.origin_z : p.view_ofs_z, VR_AGT_Kind()));
        vr_agt_step = 1;
    }
    setorigin(p, vr_agx_start); // (you held in place: its shots' knock-back)
    p.velocity = '0 0 0';
    if(g)
    {
        setorigin(g, vr_agx_spot); // (it too: its fighting distance's steps)
        g.velocity = '0 0 0';
        if(vr_agt_step == 4)
        {
            g.attack_finished = min(g.attack_finished, time + 0.3); // (its pauses between attacks shorter: more shots)
        }
    }
    if(vr_agt_step == 1)
    {
        VR_AGPT_Clear();
        vr_agt_grunt = world;
        localcmd("vr_brush_delete all\n");
        vr_agx_t0 = time;
        vr_agt_step = 2;
        return;
    }
    if(vr_agt_step == 2)
    {
        // (the last case's box gone: the grunt now, then this case's box)
        if(VR_AGX_Blocked())
        {
            if(time - vr_agx_t0 > 2)
            {
                VR_AGT_Check("expose: the last box removed (timed out)", FALSE);
                VR_AGX_End();
            }
            return;
        }
        cvar_set("vr_agrunt_aim_exposed", ftos(vr_agx_mode));
        vr_agt_grunt = VR_AGT_Spawn(VR_AGX_Wid(vr_agx_w), 300);
        g = vr_agt_grunt;
        traceline(g.origin, g.origin - '0 0 512', TRUE, g);
        vr_agx_spot = trace_endpos - '0 0 1' * g.mins_z + '0 0 0.25'; // (held on the floor under it)
        VR_AGT_Hunt(g);
        g.attack_finished = time + 0.5; // (no attack before its box stands)
        if(vr_agx_case == 1) { VR_AGX_Box(-64, 64, 14, 40); }
        else if(vr_agx_case == 2) { VR_AGX_Box(-64, -1, 80, 40); }
        else if(vr_agx_case == 3) { VR_AGX_Box(-20, 20, 17, 40); }
        else if(vr_agx_case == 4) { VR_AGX_Box(-64, 64, 80, 40); }
        vr_agx_t0 = time;
        vr_agt_step = 3;
        return;
    }
    if(vr_agt_step == 3)
    {
        if(vr_agx_case > 0 && !VR_AGX_Blocked())
        {
            if(time - vr_agx_t0 > 2)
            {
                VR_AGT_Check("expose: the case's box made (timed out)", FALSE);
                VR_AGX_End();
            }
            return;
        }
        vr_agx_shots0 = vr_ag_shots_fired;
        vr_agx_hits0 = vr_agr_pl_hits;
        vr_agx_held0 = vr_ag_xheld;
        vr_agx_tr0 = vr_ag_xtraces;
        for(i = 0; i < 14; i++)
        {
            vr_agx_xs0[i] = vr_ag_xshots[i];
        }
        vr_agx_t0 = time;
        vr_agt_step = 4;
        return;
    }
    if(vr_agt_step == 4)
    {
        // (its shots, or the case's seconds: behind the wall it never fires)
        if(g && g.health > 0 && vr_ag_shots_fired - vr_agx_shots0 < VR_AGX_SHOTS && time - vr_agx_t0 < VR_AGX_SECS)
        {
            return;
        }
        vr_agx_t1 = time;
        vr_agt_step = 5;
    }
    if(vr_agt_step == 5)
    {
        // (its last nails or rocket landed)
        if(g && VR_AGX_InFlight(g) && ((time - vr_agx_t1) < 2))
        {
            return;
        }
        float shots = vr_ag_shots_fired - vr_agx_shots0;
        float hits = vr_agr_pl_hits - vr_agx_hits0;
        float held = vr_ag_xheld - vr_agx_held0;
        string parts = "";
        float chest = 0;
        float aimed = 0;
        for(i = 0; i < 14; i++)
        {
            float n = vr_ag_xshots[i] - vr_agx_xs0[i];
            if(n > 0)
            {
                parts = strcat(parts, sprintf(" %s %g", VR_AGX_PartName(i), n));
                aimed = aimed + n;
                if(i == 1) { chest = n; }
            }
        }
        string what = sprintf("expose: %s, %s, %s: ", VR_AGT_Name(VR_AGX_Wid(vr_agx_w)), VR_AGX_CaseName(vr_agx_case),
            vr_agx_mode ? "new" : "old");
        what = strcat(what, sprintf("shots %g, hits %g (%.0f%%), held %g, aimed at:%s", shots, hits,
            100 * hits / max(1, shots), held, parts));
        if(vr_agx_mode == 0)
        {
            VR_AGT_Print(what);
            vr_agx_old_shots = shots;
            vr_agx_old_hits = hits;
            vr_agx_mode = 1;
        }
        else
        {
            vr_agx_tr_n = vr_agx_tr_n + vr_ag_xtraces - vr_agx_tr0;
            vr_agx_tr_secs = vr_agx_tr_secs + time - vr_agx_t0;
            float rate = hits / max(1, shots);
            float oldRate = vr_agx_old_hits / max(1, vr_agx_old_shots);
            float ok;
            if(vr_agx_case == 0)
            {
                ok = (shots > 0 && rate >= oldRate - 0.2 && chest >= 0.7 * aimed);
            }
            else if(vr_agx_case == 4)
            {
                ok = shots == 0;
            }
            else
            {
                // (the door frame's: its chest's edge showing, the old aim already near; 9 shots' noise)
                ok = (shots > 0 && rate >= oldRate - 0.25 && rate >= 0.25);
            }
            VR_AGT_Check(what, ok);
            vr_agx_mode = 0;
            vr_agx_case = vr_agx_case + 1;
            if(vr_agx_case >= VR_AGX_CASES)
            {
                vr_agx_case = 0;
                vr_agx_w = vr_agx_w + 1;
            }
        }
        if(vr_agx_w >= VR_AGX_GUNS || (cvar("scratch1") > 0 && vr_agx_w >= cvar("scratch1")))
        {
            VR_AGX_End();
            return;
        }
        vr_agt_step = 1;
    }
};
#endif
