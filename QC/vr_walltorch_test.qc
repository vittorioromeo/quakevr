// vr_walltorch's test harness (moved out of vr_walltorch.qc; ROUND22's QC cleanup: a pure move). Its globals and the
// cvars it reads stay with the feature.

void() VR_WallTorch_TestShot =
{
    if(vr_wt_test_until && time >= vr_wt_test_until)
    {
        dprint(sprintf("walltorch result: kind %g detached=%g held=%g blood=%g missile=%g\n", vr_wt_test_kind,
            vr_wt_test_target.wt_state != 0, vr_wt_test_target.carry_player == self,
            vr_blood_sent - vr_wt_test_blood, vr_wt_test_missile != world && vr_wt_test_missile.model != string_null));
        vr_wt_test_until = 0;
    }
    float kind = cvar("vr_test_walltorch_shot");
    if(kind < 0)
    {
        return;
    }
    cvar_set("vr_test_walltorch_shot", "-1");
    if(self.health <= 0)
    {
        return;
    }
    entity t = world;
    float best = VR_WALLTORCH_TEST_REACH;
    float all = 0; // (how many this map has at all: so that no wall torch within reach says the map has none)
    for(entity e = nextent(world); e != world; e = nextent(e))
    {
        if(e.classname != "light_torch_small_walltorch" || e.wt_state != 0)
        {
            continue;
        }
        all = all + 1;
        float d = vlen(e.origin - self.origin);
        if(d < best)
        {
            best = d;
            t = e;
        }
    }
    if(t == world)
    {
        dprint(sprintf("walltorch test: no wall torch on a wall within reach (%g in this map)\n", all));
        return;
    }
    vector mid = 0.5 * (t.absmin + t.absmax);
    dprint(sprintf("walltorch test: kind %g at the wall torch at %v\n", kind, t.origin));
    vr_wt_test_target = t;
    vr_wt_test_missile = world;
    vr_wt_test_kind = kind;
    vr_wt_test_blood = vr_blood_sent;
    vr_wt_test_until = time + 1;
    if(kind == 23)
    {
        VR_SetHitKind(QVR_HITKIND_MELEE, cVR_MainHand, mid, normalize(mid - self.origin));
        T_Damage(t, self, self, 20);
        VR_HitBloodSplash(t, mid, 20);
        VR_HitBlood(t, mid, '0 0 0', 20);
        VR_ClearHitKind();
        return;
    }
    if(kind == 24)
    {
        VR_WallTorch_Grip(t, self, cVR_MainHand, "test immediate grip");
        dprint(sprintf("walltorch grip result: detached=%g held=%g\n", t.wt_state != 0, t.carry_player == self));
        return;
    }
    if(kind >= VR_WALLTORCH_TEST_BOLT)
    {
        if(kind == VR_WALLTORCH_TEST_BOLT)
        {
            dprint("walltorch test: a lightning bolt at it\n");
            LightningDamage(self.origin + self.view_ofs, mid, self, cvar_hget(cvarh_vr_dmg_lightning));
            return;
        }
        entity blast = spawn();
        setorigin(blast, t.origin);
        dprint("walltorch test: a blast at it\n");
        T_RadiusDamage(blast, self, VR_WALLTORCH_TEST_BLAST_RADIUS, world);
        remove(blast);
        return;
    }
    if(kind >= VR_WALLTORCH_TEST_THROW)
    {
        entity p = world;
        best = VR_WALLTORCH_TEST_PROPS;
        for(entity e = nextent(world); e != world; e = nextent(e))
        {
            if(!e.vr_rigid || e.carry_player != world || e == t ||
               !(e.touch == forcegrabbable_touch || e.vr_impact))
            {
                continue;
            }
            float d = vlen(e.origin - self.origin);
            if(d < best)
            {
                best = d;
                p = e;
            }
        }
        if(p == world)
        {
            dprint("walltorch test: no loose prop to throw\n");
            return;
        }
        vector to = normalize(mid - (self.origin + self.view_ofs)); // (the line from your eyes to it)
        vector dir; // (along the wall past it, not at it from where you stand: you are not in the way of the throw)
        dir_x = -to_y;
        dir_y = to_x;
        dir_z = 0;
        float speed = cvar("vr_test_fling_speed") * VRMetersToUnits();
        p.owner = world;
        p.carry_player = world;
        p.carry_letgo = world;
        p.throwhit = QVR_THROWHIT_NEVER_HIT; // (a throw, not a prop resting where it fell)
        p.vr_fling_hit = 0;
        p.fg_state = 0;
        p.flags = p.flags - (p.flags & FL_ONGROUND);
        setorigin(p, mid - dir * (0.5 * max(t.size_x, t.size_y) + 40) - 0.5 * (p.mins + p.maxs));
        p.velocity = dir * speed;
        dprint(sprintf("walltorch test: %s sent at it along %v at %g m/s\n", p.classname, dir,
            cvar("vr_test_fling_speed")));
        return;
    }
    vr_wt_test_missile = VR_Test_Fire(kind, mid);
    if(kind == 2 && vr_wt_test_missile != world)
    {
        // A close contact fixture: a long ballistic shot at the distant torch can fall short.
        vector dir = normalize(mid - (self.origin + self.view_ofs));
        setorigin(vr_wt_test_missile, mid - dir * 40);
        vr_wt_test_missile.velocity = dir * 800;
    }
};
