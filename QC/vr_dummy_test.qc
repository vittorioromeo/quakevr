// vr_dummy's test harness (moved out of vr_dummy.qc; ROUND22's QC cleanup: a pure move). Its globals and the
// cvars it reads stay with the feature.

// The zone a shot from 64 units ahead of it, level, strikes at height `xUp` over its origin (-1: none).
float(entity d, float xUp) VR_Dummy_TestZone =
{
    makevectors('0 1 0' * d.angles_y);
    vector at = d.origin + '0 0 1' * xUp;
    if(PositionalHead(d) && xUp > d.maxs_z * 0.5)
    {
        at = d.origin + v_forward * xHeadForward + VR_HeadSide(v_forward) + '0 0 1' * xHeadUp; // (its head sphere's middle)
    }
    vector from = at + v_forward * 64;
    traceline(from, at - v_forward * 64, FALSE, self); // (self: you, between)
    if(trace_ent != d)
    {
        return -2; // (missed it)
    }
    return PositionalRegion(d, trace_endpos, at - from);
};

void(float xTest) VR_Dummy_Test =
{
    entity d = VR_Dummy_Nearest(self);
    if(d == world)
    {
        bprint("dummytest: no training dummy\n");
        return;
    }
    VR_Dummy_Health(d);
    if(xTest == 1)
    {
        bprint(sprintf("dummytest: %s (%s) model %s box %v .. %v\n", VR_Dummy_Name(d), d.classname, d.model, d.mins,
            d.maxs));
        bprint(sprintf("dummytest: own health %g, health %g/%g\n", d.vr_dummy_own_health, d.vr_dummy_hp, d.vr_dummy_hpmax));
        bprint(sprintf("dummytest: zones head %g body %g legs %g (1 head, 0 body, 2 limbs, 3 legs, -1 none, -2 missed)\n",
            VR_Dummy_TestZone(d, d.maxs_z), VR_Dummy_TestZone(d, d.maxs_z * 0.3), VR_Dummy_TestZone(d, d.mins_z * 0.5)));
        return;
    }
    if(xTest == 2 || xTest == 3)
    {
        float hurt = xTest == 2 ? 10 : max(1, d.vr_dummy_hp);
        T_Damage(d, self, self, hurt);
        bprint(sprintf("dummytest: hit for %g: health %g/%g, %s\n", hurt, d.vr_dummy_hp, d.vr_dummy_hpmax,
            VR_IsDummy(d) ? "standing" : sprintf("killed (%s, health %g)", d.classname, d.health)));
        return;
    }
    if(xTest == 5)
    {
        // A two-hand shove from you (vr_dummy_anchored: it stays put).
        vector dir = normalize(d.origin - self.origin);
        dir_z = 0;
        float down = VR_Knockdown_Try(d, dir, QVR_HITKIND_SHOVE, -1, 1);
        VR_Shove_Start(d, dir, 400);
        bprint(sprintf("dummytest: shoved (anchored %g): knocked down %g, sliding %g\n", cvar_hget(cvarh_vr_dummy_anchored),
            down || d.vr_knockdown, d.vr_shove != world));
        return;
    }
    bprint(sprintf("dummytest: health %g/%g (%.1f s since its last hit)\n", d.vr_dummy_hp, d.vr_dummy_hpmax,
        time - d.vr_dummy_hit_time));
    bprint(sprintf("dummytest: recovery down %g, dummy think %g, frame %g, home gap %.2f, movement %g\n",
        d.vr_knockdown, d.think == vr_dummy_think, d.frame, vlen(d.origin - d.vr_dummy_home), d.movetype));
};

// Every server frame (StartFrame): a test asked for (vr_dummy_test).
void() VR_Dummy_TestFrame =
{
    float ask = cvar("vr_dummy_test");
    if(!ask)
    {
        return;
    }
    cvar_set("vr_dummy_test", "0");
    entity p = find(world, classname, "player");
    if(p == world)
    {
        return;
    }
    entity oself = self;
    self = p;
    VR_Dummy_Test(ask);
    self = oself;
};
