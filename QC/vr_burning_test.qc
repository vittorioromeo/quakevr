// vr_burning's test harness (moved out of vr_burning.qc; ROUND22's QC cleanup: a pure move). Its globals and the
// cvars it reads stay with the feature.

// Test 10: the pieces lying about.
void() VR_Burn_TestPieces =
{
    float n = 0;
    float charred = 0;
    float burning = 0;
    float near = 0; // pieces with another piece or a crate within Pieces Catch Within (their boxes)
    float gap = max(0, cvar_hget(cvarh_vr_burn_piece_gap));
    entity e = find(world, classname, "vr_crate_piece");
    while(e)
    {
        if(VR_CratePiece_Is(e))
        {
            n = n + 1;
            charred = charred + (e.vr_charred ? 1 : 0);
            burning = burning + (e.burn_fire != world ? 1 : 0);
            entity o = findradius((e.absmin + e.absmax) * 0.5, 128);
            while(o)
            {
                if(o != e && VR_Burn_Wood(o) && o.absmin_x <= e.absmax_x + gap && o.absmax_x >= e.absmin_x - gap &&
                   o.absmin_y <= e.absmax_y + gap && o.absmax_y >= e.absmin_y - gap &&
                   o.absmin_z <= e.absmax_z + gap && o.absmax_z >= e.absmin_z - gap)
                {
                    near = near + 1;
                    break;
                }
                o = o.chain;
            }
        }
        e = find(e, classname, "vr_crate_piece");
    }
    dprint(sprintf("burning: test 10: %g pieces, %g charred, %g burning, %g with wood within %g\n", n, charred, burning,
                   near, gap));
};

// Tests 8, 9: the crate nearest `p` smashed by its blow (8), or burnt through now (9).
void(entity p, float xAsk) VR_Burn_TestCrate =
{
    entity best = world;
    float bestd = 100000;
    entity e = findradius(p.origin, 1024);
    while(e)
    {
        if(VR_Crate_Is(e) && e.takedamage && vlen(e.origin - p.origin) < bestd)
        {
            best = e;
            bestd = vlen(e.origin - p.origin);
        }
        e = e.chain;
    }
    if(best == world)
    {
        dprint("burning: test: no crate near\n");
        return;
    }
    if(xAsk == 8)
    {
        dprint(sprintf("burning: test 8: %s smashed\n", best.classname));
        T_Damage(best, p, p, best.health + 1000);
        return;
    }
    if(best.burn_fire == world)
    {
        vector mid = (best.absmin + best.absmax) * 0.5;
        VR_Burn_Ignite(best, p, VR_Burn_OnBody(best, mid + normalize(p.origin - mid) * 64), mid - p.origin, VR_BURN_BLOW);
    }
    if(best.burn_fire != world)
    {
        best.burn_fire.burn_end = time; // (burnt through at its next think: VR_Burn_Think)
        dprint(sprintf("burning: test 9: %s burns through now\n", best.classname));
    }
};

void() VR_Burn_TestFrame =
{
    float ask = cvar("vr_burn_test");
    if(!ask)
    {
        return;
    }
    cvar_set("vr_burn_test", "0");
    entity p = find(world, classname, "player");
    if(p == world)
    {
        return;
    }
    if(ask == 5)
    {
        if(!vr_have_lava) { dprint("burning: lava nail test requires the lava spike model (Rogue)\n"); return; }
        // The main hand's nailgun or super nailgun loaded with lava nails (no reload needed).
        float wid = VRGetEntWeapon(p, cVR_MainHand);
        if(wid != WID_NAILGUN && wid != WID_SUPER_NAILGUN)
        {
            dprint("burning: test 5: no nailgun in the main hand\n");
            return;
        }
        p.ammo_lava_nails = max(p.ammo_lava_nails, 100);
        VRSetEntWeaponFlags(p, cVR_MainHand, VRGetEntWeaponFlags(p, cVR_MainHand) | QVR_WPNFLAG_USE_SECONDARY_AMMO);
        VRSetEntWeaponClip(p, cVR_MainHand, WeaponIdToBaseClipSize(wid));
        entity oself = self;
        self = p;
        W_SetCurrentAmmoFor(cVR_MainHand);
        self = oself;
        dprint("burning: test 5: lava nails loaded in the main hand\n");
        return;
    }
    if(ask == 6 || ask == 7)
    {
        entity bt = world;
        float btd = 100000;
        entity tt = find(world, classname, "light_torch_small_walltorch");
        while(tt)
        {
            if(tt.modelindex && (tt.wt_state == 0 || tt.wt_fire > 0) && vlen(VR_WallTorch_Head(tt) - p.origin) < btd)
            {
                bt = tt;
                btd = vlen(VR_WallTorch_Head(tt) - p.origin);
            }
            tt = find(tt, classname, "light_torch_small_walltorch");
        }
        if(bt == world)
        {
            dprint("burning: test 6: no lit torch\n");
            return;
        }
        vector flame = VR_WallTorch_Head(bt) + '0 0 8';
        if(ask == 7)
        {
            dprint(sprintf("burning: test 7: the nearest lit torch's flame at %v (%s)\n", flame,
                           bt.carry_player != world ? "held" : (bt.wt_state == 0 ? "on its wall" : "lying")));
            return;
        }
        vector way = normalize(flame - (p.origin + '0 0 16'));
        entity os = self;
        self = p;
        launch_spike(flame - way * 48, way);
        self = os;
        dprint(sprintf("burning: test 6: a nail at the torch's flame at %v\n", flame));
        return;
    }
    if(ask == 12)
    {
        // A nail through the nearest of the map's flames (a fire spot: a flame, a brazier, a static wall torch, a
        // candle), level from 64 units before it on the player's side, through its volume's middle.
        float bi = -1;
        float bd = 100000;
        float i;
        for(i = 0; i < vr_fire_spots; i++)
        {
            vector so = vr_fire_spot[i];
            if(vlen(so - p.origin) < bd)
            {
                bd = vlen(so - p.origin);
                bi = i;
            }
        }
        if(bi < 0)
        {
            dprint("burning: test 12: no map flame\n");
            return;
        }
        vector fo = vr_fire_spot[bi];
        float fk = vr_fire_spot_large[bi];
        VR_Burn_FireVolume(fk);
        vector fm = fo + (vr_fv_lo + vr_fv_hi) * 0.5;
        vector fway = fm - p.origin;
        fway_z = 0;
        fway = normalize(fway);
        // From as far back as is clear (up to 64), so that it is not stopped before the flame.
        traceline(fm, fm - fway * 64, TRUE, p);
        float back = max(8, vlen(trace_endpos - fm) - 4);
        entity fs = self;
        self = p;
        launch_spike(fm - fway * back, fway);
        self = fs;
        dprint(sprintf("burning: test 12: a nail at the map's flame (kind %g) at %v, %.0f away, from %.0f before it\n",
                       fk, fm, bd, back));
        return;
    }
    if(ask == 8 || ask == 9)
    {
        VR_Burn_TestCrate(p, ask);
        return;
    }
    if(ask == 10)
    {
        VR_Burn_TestPieces();
        return;
    }
    if(ask == 11)
    {
        if(!vr_have_lava) { dprint("burning: lava nail test requires the lava spike model (Rogue)\n"); return; }
        // A lava nail of yours at the nearest piece, from 40 units above it and towards you.
        entity np = world;
        float npd = 100000;
        entity q = find(world, classname, "vr_crate_piece");
        while(q)
        {
            if(VR_CratePiece_Is(q) && vlen(q.origin - p.origin) < npd)
            {
                np = q;
                npd = vlen(q.origin - p.origin);
            }
            q = find(q, classname, "vr_crate_piece");
        }
        if(np == world)
        {
            dprint("burning: test 11: no piece near\n");
            return;
        }
        vector pm = (np.absmin + np.absmax) * 0.5;
        vector from = pm + '0 0 40' + normalize(p.origin - pm) * 20;
        entity os11 = self;
        self = p;
        entity nl = launch_spike(from, normalize(pm - from));
        self = os11;
        nl.classname = "lava_spike";
        nl.touch = lavaspike_touch;
        setmodel(nl, "progs/lspike.mdl");
        setsize(nl, VEC_ORIGIN, VEC_ORIGIN);
        dprint(sprintf("burning: test 11: a lava nail at the piece at %v (charred %g)\n", pm, np.vr_charred));
        return;
    }
    entity best = world;
    float bestd = 100000;
    entity e = findradius(p.origin, 1024);
    while(e)
    {
        float cand = (e.flags & FL_MONSTER) || VR_Burn_Wood(e) || VR_Burn_Metal(e);
        vector emid = (e.absmin + e.absmax) * 0.5; // (an explosive box's origin is its corner)
        if(e != p && cand && e.modelindex && vlen(emid - p.origin) < bestd)
        {
            best = e;
            bestd = vlen(emid - p.origin);
        }
        e = e.chain;
    }
    if(best == world)
    {
        dprint("burning: test: no monster or corpse near\n");
        return;
    }
    if(ask == 4)
    {
        entity f = best.burn_fire;
        string how = "not burning";
        if(f != world)
        {
            how = sprintf("%g flames, %.1f s left", f.burn_count, f.burn_end - time);
        }
        dprint(sprintf("burning: %s at %v, health %g, corpse %g, skin %g, charred %g: %s\n", best.classname, best.origin,
                       best.health, best.vr_corpse, best.skin, best.vr_charred, how));
        return;
    }
    if(!VR_Burn_Can(best))
    {
        dprint(sprintf("burning: test %g: %s can't burn (health %g)\n", ask,
                       VR_Burn_Metal(best) ? "an explosive box (metal)" : best.classname, best.health));
        return;
    }
    vector mid = (best.absmin + best.absmax) * 0.5;
    vector at = mid + normalize(p.origin - mid) * 64;
    at_z = mid_z + (best.health > 0 ? (best.absmax_z - mid_z) * 0.3 : 0);
    at = VR_Burn_OnBody(best, at);
    dprint(sprintf("burning: test %g on %s (health %g, corpse %g)\n", ask, best.classname, best.health, best.vr_corpse));
    VR_Burn_Ignite(best, p, at, mid - p.origin, ask == 2 ? VR_BURN_NAIL : (ask == 3 ? VR_BURN_TOUCH : VR_BURN_BLOW));
};
