// vr_hotblade's test harness (moved out of vr_hotblade.qc; ROUND22's QC cleanup: a pure move). Its globals and the
// cvars it reads stay with the feature.

// The main hand's blade's far point (its last striking point, as tracked), '0 0 0' none.
vector() VR_HotBlade_TestTip =
{
    entity b = VR_Blow(cVR_MainHand);
    if(b.mh_n <= 0 || self.weapon == WID_FIST)
    {
        return '0 0 0';
    }
    return b.mh_cp[b.mh_n - 1];
};

// The player moved (noclip: the test's) so that the main hand's blade's far point is 2 units past `xTo` (its tip in
// it, the head round it).
void(vector xTo, string xWhat) VR_HotBlade_TestPut =
{
    vector tip = VR_HotBlade_TestTip();
    if(tip == '0 0 0')
    {
        dprint("hotblade: test: no blade in the main hand\n");
        return;
    }
    vector d = xTo - tip;
    setorigin(self, self.origin + d);
    self.velocity = '0 0 0';
    dprint(sprintf("hotblade: test: blade put in %s at %v (moved %.0f)\n", xWhat, xTo, vlen(d)));
};

// The nearest of the map's flames to the main hand's blade (its fire spots, a torch on its wall), '0 0 0' none.
vector() VR_HotBlade_TestFlame =
{
    vector tip = VR_HotBlade_TestTip();
    vector best = '0 0 0';
    float bestD = 100000;
    float i;
    for(i = 0; i < vr_fire_spots; i++)
    {
        float kind = vr_fire_spot_large[i]; // (one array read a statement: the getters clobber each other)
        vector o = vr_fire_spot[i];
        if(kind == 3)
        {
            continue; // (a candle: nails only)
        }
        o = o + (kind == 1 ? '0 0 18' : '0 0 8');
        if(vlen(o - tip) < bestD)
        {
            bestD = vlen(o - tip);
            best = o;
        }
    }
    entity e;
    for(e = find(world, classname, "light_torch_small_walltorch"); e; e = find(e, classname, "light_torch_small_walltorch"))
    {
        if(e.wt_state == 0 && vlen(e.origin + '0 0 10' - tip) < bestD)
        {
            bestD = vlen(e.origin + '0 0 10' - tip);
            best = e.origin + '0 0 10';
        }
    }
    return best;
};

// The nearest point in contents `xContents` to the player within 384 units across and 256 down (16 apart), '0 0 0' none.
vector(float xContents) VR_HotBlade_TestFind =
{
    vector best = '0 0 0';
    float bestD = 100000;
    float x;
    float y;
    float z;
    for(z = 64; z >= -256; z = z - 16)
    {
        for(y = -384; y <= 384; y = y + 16)
        {
            for(x = -384; x <= 384; x = x + 16)
            {
                vector p = self.origin + [x, y, z];
                if(pointcontents(p) == xContents && pointcontents(p + '0 0 8') == xContents && vlen(p - self.origin) < bestD)
                {
                    bestD = vlen(p - self.origin);
                    best = p;
                }
            }
        }
    }
    return best;
};

void() VR_HotBlade_TestFrame =
{
    float ask = cvar("vr_hotblade_test");
    if(!ask)
    {
        return;
    }
    cvar_set("vr_hotblade_test", "0");
    entity oldSelf = self;
    self = find(world, classname, "player");
    if(self == world)
    {
        self = oldSelf;
        return;
    }
    float h;
    entity r;
    if(ask == 1 || ask == 2 || ask == 4 || ask == 5)
    {
        for(h = 0; h < 2; ++h)
        {
            r = VRGetEntWeaponInst(self, h);
            if(r == world || !VR_HotBlade_Heatable(VRGetEntWeapon(self, h)))
            {
                continue;
            }
            if(ask == 1)
            {
                VR_HotBlade_Heat(r);
            }
            else if(ask == 2)
            {
                VR_HotBlade_Cool(r);
            }
            else if(ask == 4)
            {
                r.wi_hot_end = time + max(0.2, cvar_hget(cvarh_vr_hotblade_fade)) * 0.5;
            }
            else
            {
                r.wi_hot_end = 0;
                r.wi_heat = 0.5;
                r.wi_heat_at = time + 600; // (held there: the test's screenshot)
            }
            dprint(sprintf("hotblade: test %g: hand %g %s\n", ask, h, VR_HotBlade_Desc(r, VRGetEntWeapon(self, h))));
        }
    }
    else if(ask == 3)
    {
        dprint(sprintf("hotblade: report: main %s; off %s; hud %g %g\n", VR_HotBlade_Desc(self.weaponinst, self.weapon),
            VR_HotBlade_Desc(self.weaponinst2, self.weapon2), self.vr_hot_hud, self.vr_hot_hud2));
        dprint(sprintf("hotblade: report: holsters' glow %g %g %g %g %g %g\n", VR_HotBlade_Glow(self.holsterweaponinst0),
            VR_HotBlade_Glow(self.holsterweaponinst1), VR_HotBlade_Glow(self.holsterweaponinst2),
            VR_HotBlade_Glow(self.holsterweaponinst3), VR_HotBlade_Glow(self.holsterweaponinst4),
            VR_HotBlade_Glow(self.holsterweaponinst5)));
    }
    else if(ask == 6)
    {
        // The nearest monster or corpse struck by the main hand's weapon as a landed blow of strength 1 (its damage as
        // VRMeleeDmg prices a weapon of base 20, the heat's extra too: T_Damage_VRMelee).
        entity m = findradius(self.origin, 400);
        entity near = world;
        float nearD = 100000;
        for(; m; m = m.chain)
        {
            if((m.flags & FL_MONSTER) && m.takedamage && vlen(m.origin - self.origin) < nearD)
            {
                nearD = vlen(m.origin - self.origin);
                near = m;
            }
        }
        if(near != world)
        {
            float dmg = (20 + VR_HotBlade_Extra(self, cVR_MainHand));
            vector mid = (near.absmin + near.absmax) * 0.5;
            dprint(sprintf("hotblade: test: strike %s (health %g, corpse %g) for %g\n", near.classname, near.health,
                near.vr_corpse, dmg));
            VR_SetHitKind(QVR_HITKIND_MELEE, cVR_MainHand, mid, normalize(mid - self.origin));
            T_Damage_VRMelee(near, self, self, dmg);
            VR_ClearHitKind();
        }
    }
    else if(ask == 7)
    {
        vector f = VR_HotBlade_TestFlame();
        if(f != '0 0 0')
        {
            VR_HotBlade_TestPut(f, "the nearest map flame");
        }
        else
        {
            dprint(sprintf("hotblade: test: no flame (%g fire spots)\n", vr_fire_spots));
        }
    }
    else if(ask == 8 || ask == 11)
    {
        vector l = VR_HotBlade_TestFind(ask == 8 ? CONTENT_LAVA : CONTENT_SLIME);
        if(l == '0 0 0' && ask == 11)
        {
            l = VR_HotBlade_TestFind(CONTENT_WATER);
        }
        if(l != '0 0 0')
        {
            VR_HotBlade_TestPut(l, ask == 8 ? "lava" : "water");
        }
        else
        {
            dprint("hotblade: test: none near\n");
        }
    }
    else if(ask == 9)
    {
        entity t = findradius(self.origin, 1000);
        entity best = world;
        float bestD = 100000;
        for(; t; t = t.chain)
        {
            if(t.classname == "light_torch_small_walltorch" && t.wt_state == 0 && vlen(t.origin - self.origin) < bestD)
            {
                bestD = vlen(t.origin - self.origin);
                best = t;
            }
        }
        if(best == world)
        {
            for(t = find(world, classname, "light_torch_small_walltorch"); t; t = find(t, classname, "light_torch_small_walltorch"))
            {
                if(t.wt_state == 0 && vlen(t.origin - self.origin) < bestD)
                {
                    bestD = vlen(t.origin - self.origin);
                    best = t;
                }
            }
        }
        if(best != world)
        {
            VR_WallTorch_Grip(best, self, cVR_OffHand, "hotblade test");
        }
    }
    else if(ask == 10)
    {
        entity c = VR_Carry_Held(self, cVR_OffHand);
        vector tip = VR_HotBlade_TestTip();
        if(c != world && VR_WallTorch_IsTorch(c) && c.wt_state == 1 && c.wt_fire > 0 && tip != '0 0 0')
        {
            // The off hand (the mock headset's: vr_mock_hand_to) moved so that its torch's flame is round the blade's tip
            // (moving the player would move both hands).
            vector d = tip - (VR_WallTorch_Head(c) + '0 0 1' * (6 * c.wt_fire));
            localcmd(sprintf("vr_mock_hand_to off by %g %g %g\n", d_x, d_y, d_z));
            dprint(sprintf("hotblade: test: the off hand's torch's flame brought to the blade (moved %.0f: by %g %g %g)\n",
                vlen(d), d_x, d_y, d_z));
        }
        else
        {
            dprint("hotblade: test: no lit torch in the off hand\n");
        }
    }
    else if(ask == 12)
    {
        // The weapons lying about (or flying): their heat, glow and what the engine is told.
        float n = 0;
        entity w;
        for(w = find(world, classname, "thrown_weapon"); w; w = find(w, classname, "thrown_weapon"))
        {
            if(VR_HotBlade_Heatable(w.weapon))
            {
                n = n + 1;
                dprint(sprintf("hotblade: lying: %s; sent %.2f speed %.0f\n", VR_HotBlade_Desc(w.weaponinst, w.weapon),
                    w.vr_hot_glow, vlen(w.velocity)));
            }
        }
        dprint(sprintf("hotblade: lying: %g blades\n", n));
    }
    else if(ask == 13 || ask == 14)
    {
        // 13: the main hand's weapon thrown straight at the nearest monster's middle, at 1.5 times the throw's full-damage
        // speed, hardly falling; 14: let go of where the hand is (dropped, no throw).
        float wid = self.weapon;
        if(wid == WID_FIST)
        {
            dprint("hotblade: test: nothing in the main hand\n");
        }
        else
        {
            vector from = VRGetHandPos(cVR_MainHand);
            vector vel = '0 0 0';
            float m2u = VRMetersToUnits();
            if(ask == 13)
            {
                entity mon = findradius(self.origin, 600);
                entity aim = world;
                float aimD = 100000;
                for(; mon; mon = mon.chain)
                {
                    if((mon.flags & FL_MONSTER) && mon.takedamage && vlen(mon.origin - self.origin) < aimD)
                    {
                        aimD = vlen(mon.origin - self.origin);
                        aim = mon;
                    }
                }
                if(aim != world)
                {
                    // (From 24 units short of it: nothing else on the way, the same hit every run.)
                    vector mid = (aim.absmin + aim.absmax) * 0.5;
                    vel = normalize(mid - from) * (1.5 * 700 * m2u / 120);
                    from = mid - normalize(vel) * (24 + vlen(aim.size) * 0.25);
                    traceline(from, mid, FALSE, self);
                    dprint(sprintf("hotblade: test: aimed at %s from %v (%.0f away), on the way: %s at %.2f\n", aim.classname,
                        from, vlen(mid - from), trace_ent.classname, trace_fraction));
                }
            }
            entity thrown = CreateThrownWeapon(self, wid, self.weaponflags, self.weaponinst, from, VRGetHandRot(cVR_MainHand),
                vel, '0 0 0', FALSE, ask == 13, WeaponIdToThrowDamage(wid));
            if(thrown != world)
            {
                thrown.vr_wpn_from = self;
                thrown.throwdmgspeed = 700 * m2u / 120;
                thrown.gravity = ask == 13 ? 0.01 : 1;
                thrown.vr_rigid = 1; // (as a real throw: DropWeaponInHandScaled)
                dprint(sprintf("hotblade: test: weapon %g %s at %.0f u/s (%s)\n", wid, ask == 13 ? "thrown" : "dropped",
                    vlen(vel), VR_HotBlade_Desc(thrown.weaponinst, wid)));
            }
            VRSetHandEmpty(self, cVR_MainHand);
            W_SetCurrentAmmoFor(cVR_MainHand);
        }
    }
    self = oldSelf;
};
