// vr_melee_sweep_test 2 (the blade shape editor's check, ROUND22.md "Blade shape editor"): the main hand's melee blade as
// the melee lays it out now, in the hand's frame (units from the hand along its forward, right, up: the same in any game
// for the same mock hand), one "bladedump:" line a point: the line's near end, grip and far end, each layout point
// (VR_Melee_At: on a curve if it has one), the head's box the track keeps (VR_Blow: shares of the line) and the swept
// points' radius. Uses only what the melee had before the editor's keys (so the same file run on the old code gives the
// lines to compare: defaults unchanged).

vector(entity p, vector xAt) VR_BladeDump_Local =
{
    makevectors(VRGetEntHandRot(p, cVR_MainHand));
    // (The hand: the line's start unless it has one of its own; carried off its handle, the grip on its line.)
    vector d = xAt - (vr_mline_free ? vr_mline_grip : VRGetEntHandPos(p, cVR_MainHand));
    vector r;
    r_x = d * v_forward;
    r_y = d * v_right;
    r_z = d * v_up;
    return r;
};

void(entity p) VR_BladeDump =
{
    entity was = self;
    self = p;
    float thing = VR_Melee_Thing(p, cVR_MainHand);
    if(!VR_Melee_Line(p, cVR_MainHand, thing))
    {
        bprint(sprintf("bladedump: %s: no line (thing %g)\n", p.weaponmodel, thing));
        self = was;
        return;
    }
    vector nearEnd = vr_mline_near, farEnd = vr_mline_far;
    VR_Melee_Layout(thing);
    if(!vr_mline_free)
    {
        VR_Melee_LayoutFrom(vr_mline_from_x); // (a blade start of its own, Blade From; nothing with the defaults)
    }
    bprint(sprintf("bladedump: %s thing %g bend %g len %.4f near %v far %v\n", p.weaponmodel, thing, vr_mline_bend != 0,
        vlen(farEnd - vr_mline_grip), VR_BladeDump_Local(p, nearEnd), VR_BladeDump_Local(p, farEnd)));
    float i;
    for(i = 0; i < vr_mpt_n; ++i)
    {
        bprint(sprintf("bladedump: %s pt %g f %.2f %v\n", p.weaponmodel, i, vr_mpt_f[i],
            VR_BladeDump_Local(p, VR_Melee_At(vr_mpt_f[i]))));
    }
    entity b = VR_Blow(cVR_MainHand);
    float r = VR_MELEE_RADIUS * max(0.25, cvar_hget(cvarh_vr_melee_range_multiplier));
    bprint(sprintf("bladedump: %s head %v .. %v radius %.3f\n", p.weaponmodel, b.mh_hd_lo, b.mh_hd_hi, r));
    self = was;
};
