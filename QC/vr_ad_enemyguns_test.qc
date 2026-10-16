// vr_ad_enemyguns's test harness (moved out of vr_ad_enemyguns.qc; ROUND22's QC cleanup: a pure move). Its globals and the
// cvars it reads stay with the feature.

void(entity attacker, float damage) VR_ADGun_TestTargetPain = {};

void() VR_ADGun_TestTargetDie = { self.health = 100000; };

// The target, `xDist` units out along the main hand's aim.
void(entity p, float xDist) VR_ADGun_TestPlace =
{
    entity t = vr_adgun_test_target;
    if(t == world)
    {
        t = spawn();
        t.classname = "vr_adgun_target";
        t.solid = SOLID_SLIDEBOX;
        t.movetype = MOVETYPE_NONE;
        t.takedamage = DAMAGE_AIM;
        t.th_pain = VR_ADGun_TestTargetPain;
        t.th_die = VR_ADGun_TestTargetDie;
        t.flags = 0; // (not a monster: the monsters' hooks would want its AI)
        setmodel(t, "progs/player.mdl");
        setsize(t, '-16 -16 -24', '16 16 32');
        vr_adgun_test_target = t;
    }
    t.health = 100000;
    entity oldSelf = self;
    self = p;
    vector at = VRGetMuzzlePos(cVR_MainHand) + VRGetWeaponFireRotFwd(cVR_MainHand) * xDist;
    self = oldSelf;
    setorigin(t, at - '0 0 4');
};

void() VR_ADGun_TestThink =
{
    entity p = self.owner;
    float wid = WID_ADROCKET + vr_adgun_test_index;
    entity oldSelf = self;
    if(vr_adgun_test_step == 0)
    {
        self = p;
        VR_ADGun_Ensure(wid);
        VR_WeaponUtil_EntAddWeapon(self, wid);
        VRGiveEntWeapon(self, cVR_MainHand, wid, 0, WeaponIdToBaseClipSize(wid));
        W_SetCurrentAmmoFor(cVR_MainHand);
        self = oldSelf;
        vr_adgun_test_clip = VRGetEntWeaponClip(p, cVR_MainHand);
        VR_ADGun_TestPlace(p, (wid == WID_ADPYRO || wid == WID_ADFUMIGATOR) ? 90 : 160);
        vr_adgun_hits = 0;
    }
    if(vr_adgun_test_step <= 5 && (vr_adgun_test_step == 0 || wid == WID_ADPYRO || wid == WID_ADFUMIGATOR))
    {
        self = p;
        VR_EnemyGun_Attack(cVR_MainHand, wid, 0, &self.attack_finished);
        self = oldSelf;
    }
    vr_adgun_test_step = vr_adgun_test_step + 1;
    if(vr_adgun_test_step < 6)
    {
        self.nextthink = time + 0.1;
        return;
    }
    if(vr_adgun_test_step < 20)
    {
        self.nextthink = time + 0.1; // (time for it to strike: 1.4 s)
        return;
    }
    float clip = VRGetEntWeaponClip(p, cVR_MainHand);
    float dealt = 100000 - vr_adgun_test_target.health;
    float ok = (clip < vr_adgun_test_clip && dealt > 0 && VRGetEntWeapon(p, cVR_MainHand) == wid);
    string model = WeaponIdToModel(wid, 0);
    bprint(sprintf("adgun test: %s (%s): rounds %g -> %g, dealt %g (hits %g): %s\n", VR_ADGun_Name(wid), model,
        vr_adgun_test_clip, clip, dealt, vr_adgun_hits, ok ? "PASS" : "FAIL"));
    if(ok) { vr_adgun_test_pass++; } else { vr_adgun_test_fail++; }
    vr_adgun_test_index = vr_adgun_test_index + 1;
    vr_adgun_test_step = 0;
    if(vr_adgun_test_index > WID_ADFUMIGATOR - WID_ADROCKET)
    {
        bprint(sprintf("adgun test: %g passed, %g failed\n", vr_adgun_test_pass, vr_adgun_test_fail));
        cvar_set("vr_weapon_grip_mode", ftos(vr_adgun_test_grip));
        remove(vr_adgun_test_target);
        vr_adgun_test_target = world;
        remove(self);
        return;
    }
    self.nextthink = time + 0.3;
};

void() VR_ADGun_ShotsTestThink =
{
    entity p = self.owner;
    entity oldSelf = self;
    float ph = vr_adgun_t90_phase;
    if(vr_adgun_test_step == 0)
    {
        self = p;
        float wid = (ph == 0 || ph == 4) ? WID_ADGRENADE : (ph == 1 ? WID_GRENADE_LAUNCHER : (ph == 2 ? WID_ADROCKET : WID_ADPLASMA));
        if(VR_IsADGun(wid)) { VR_ADGun_Ensure(wid); }
        VR_WeaponUtil_EntAddWeapon(self, wid);
        VRGiveEntWeapon(self, cVR_MainHand, wid, 0, WeaponIdToBaseClipSize(wid));
        self.ammo_rockets = max(self.ammo_rockets, 10);
        W_SetCurrentAmmoFor(cVR_MainHand);
        vr_adgun_t90_from = VRGetMuzzlePos(cVR_MainHand);
        vr_owner_touches = 0;
        ad_newmis = world;
        if(ph == 1) { W_FireGrenade(cVR_MainHand); }
        else if(ph == 4)
        {
            // (Not counted: the gun's grenade before 2026-10-15, AD's monster grenade model, launched as ours.)
            entity og = spawn();
            og.owner = self;
            og.movetype = MOVETYPE_BOUNCE;
            og.solid = SOLID_BBOX;
            og.classname = "grenade";
            og.velocity = VRGetWeaponFireRotFwd(cVR_MainHand) * 600 + vrProjectileVelocity(cVR_MainHand, 0.6);
            og.avelocity = '300 300 300';
            og.angles = vectoangles(og.velocity);
            og.touch = GrenadeTouch;
            og.nextthink = time + 2.5;
            og.think = GrenadeExplode;
            setmodel(og, ad_MODEL_PROJ_GRENADE);
            setsize(og, '0 0 0', '0 0 0');
            setorigin(og, VRGetMuzzlePos(cVR_MainHand));
            VR_ShotLaunched(og, cVR_MainHand);
            VR_Grenade_Make(og, GrenadeExplode, 2.5, TRUE);
        }
        else { VR_EnemyGun_Attack(cVR_MainHand, wid, 0, &self.attack_finished); }
        self = oldSelf;
        entity s = world;
        if(ph == 3)
        {
            s = ad_newmis;
        }
        else
        {
            string cn = (ph <= 1 || ph == 4) ? "grenade" : "missile";
            for(entity g = find(world, classname, cn); g != world; g = find(g, classname, cn))
            {
                if(g.owner == p && g.vr_adgun_wid == 0) { s = g; }
            }
        }
        vr_adgun_t90_shot = s;
        if(s == world)
        {
            bprint(sprintf("adgun shots: %g: nothing fired: FAIL\n", ph));
            vr_adgun_test_fail++;
            vr_adgun_test_step = 20;
        }
        else
        {
            s.vr_adgun_wid = -1 - ph; // (marked: this test's)
            vr_adgun_t90_model = s.model;
            float fuse = (s.vr_gren_fuse ? s.vr_gren_fuse : s.nextthink) - time;
            float spd = vlen(s.velocity);
            float funcs = 0;
            if(ph <= 1)
            {
                funcs = ((s.touch == VR_Grenade_Touch ? s.vr_gren_touch : s.touch) == GrenadeTouch &&
                         (s.vr_gren_boom ? s.vr_gren_boom : s.think) == GrenadeExplode);
            }
            else if(ph == 2) { funcs = s.touch == T_MissileTouch; }
            else { funcs = s.touch == VR_ADGun_PlasmaTouch; }
            bprint(sprintf("adgun shots: %g: %s, box %v, move %g, solid %g, ", ph, s.model, s.size, s.movetype, s.solid));
            bprint(sprintf("fuse %.2f, speed %.0f, ours %g\n", fuse, spd, funcs));
            if(ph == 0)
            {
                vr_adgun_t90_gsize = s.size; vr_adgun_t90_gmove = s.movetype; vr_adgun_t90_gsolid = s.solid;
                vr_adgun_t90_gfuse = fuse; vr_adgun_t90_gspeed = spd; vr_adgun_t90_gfuncs = funcs;
            }
            else if(ph == 1)
            {
                float same = vr_adgun_t90_gfuncs && funcs && s.model == "progs/grenade.mdl" && s.size == vr_adgun_t90_gsize &&
                             s.movetype == vr_adgun_t90_gmove && s.solid == vr_adgun_t90_gsolid &&
                             fabs(fuse - vr_adgun_t90_gfuse) < 0.06 && fabs(spd - vr_adgun_t90_gspeed) < 0.05 * spd;
                bprint(sprintf("adgun shots: the grenade soldier's grenade is the grenade launcher's: %s\n",
                    same ? "PASS" : "FAIL"));
                if(same) { vr_adgun_test_pass++; } else { vr_adgun_test_fail++; }
            }
            else if(ph != 4)
            {
                float own = funcs && (ph == 3 || s.model == "progs/missile.mdl");
                bprint(sprintf("adgun shots: %g: its projectile %s: %s\n", ph, ph == 2 ? "our rocket" : "AD's bolt, our hit",
                    own ? "PASS" : "FAIL"));
                if(own) { vr_adgun_test_pass++; } else { vr_adgun_test_fail++; }
            }
        }
    }
    vr_adgun_test_step = vr_adgun_test_step + 1;
    if(vr_adgun_test_step == 7)
    {
        // 0.3 s on: still flying (not caught, not gone off), well clear of the muzzle.
        entity t = vr_adgun_t90_shot;
        float alive = t.vr_adgun_wid == -1 - ph && t.model == vr_adgun_t90_model;
        float d = alive ? vlen(t.origin - vr_adgun_t90_from) : 0;
        float ok = alive && t.carry_player == world && d >= 90;
        bprint(sprintf("adgun shots: %g: 0.3 s on: %s, %.0f from the muzzle, touches of its firer %g: %s\n", ph,
            alive ? (t.carry_player != world ? "held" : "flying") : "gone", d, vr_owner_touches,
            ph == 4 ? "(before, not counted)" : (ok ? "PASS" : "FAIL")));
        if(ph != 4)
        {
            if(ok) { vr_adgun_test_pass++; } else { vr_adgun_test_fail++; }
        }
    }
    if(vr_adgun_test_step < 20)
    {
        self.nextthink = time + 0.05;
        return;
    }
    vr_adgun_test_step = 0;
    vr_adgun_t90_phase = ph + 1;
    if(vr_adgun_t90_phase > 4)
    {
        bprint(sprintf("adgun shots: %g passed, %g failed\n", vr_adgun_test_pass, vr_adgun_test_fail));
        cvar_set("vr_weapon_grip_mode", ftos(vr_adgun_test_grip));
        remove(self);
        return;
    }
    self.nextthink = time + 0.5;
};

void(float request, entity p) AD_GunTest =
{
    if(request == 81 || request == 82)
    {
        // 81: each AD weapon made ahead (the spawner's 309..319), taken into the main hand and fired once.
        // 82: each laid ahead (left there), then a Widowmaker taken into the main hand (save, load, then 83 and 84).
        float grip = cvar("vr_weapon_grip_mode");
        cvar_set("vr_weapon_grip_mode", "1"); // (a weapon stays in a hand not gripping)
        p.ammo_shells = max(p.ammo_shells, 50);
        p.ammo_cells = max(p.ammo_cells, 50);
        float pass = 0, fail = 0;
        for(float c = 309; c <= 319; c++)
        {
            entity w = VR_ADWeapons_Make(p, c, request == 81 ? 64 : 112 + (c - 309) * 24);
            if(w == world)
            {
                bprint(sprintf("adweapons: %g not made (its scope: vr_ad_upgrades %g, vr_ad_everywhere %g, vr_ad_rules %g): FAIL\n",
                    c, cvar("vr_ad_upgrades"), cvar("vr_ad_everywhere"), cvar("vr_ad_rules")));
                fail++;
                continue;
            }
            if(request == 82)
            {
                continue;
            }
            string what = sprintf("%g %s", c, w.netname);
            VR_ADWeapons_Take(p, w);
            if(VR_ADWeapons_Fire(p, what)) { pass++; } else { fail++; }
            VRGiveEntWeapon(p, cVR_MainHand, WID_FIST, 0, 0);
        }
        if(request == 82)
        {
            entity wm = VR_ADWeapons_Make(p, 310, 400);
            if(wm != world)
            {
                VR_ADWeapons_Take(p, wm);
            }
            VR_ADWeapons_Report(p);
        }
        else
        {
            bprint(sprintf("adweapons: %g passed, %g failed\n", pass, fail));
        }
        cvar_set("vr_weapon_grip_mode", ftos(grip));
        return;
    }
    if(request == 83)
    {
        VR_ADWeapons_Report(p);
        return;
    }
    if(request == 85)
    {
        // A plain super shotgun in the main hand, loaded, 53 shells in reserve (83's comparison, no AD weapon).
        VRGiveEntWeapon(p, cVR_MainHand, WID_SUPER_SHOTGUN, 0, 2);
        p.ammo_shells = 53;
        VR_ADWeapons_Report(p);
        return;
    }
    if(request == 86)
    {
        // What the Debug menu's AD pickup row (vr_test_spawn N with impulse 241) just made: the pickup or weapon with a
        // model nearest where it was made (vr_test_spawn_dist ahead), where it lies from you (ahead, within reach) and its
        // model (adspawn: one line, PASS or FAIL); then removed, for the next row's. A monster the spawner made too.
        makevectors('0 1 0' * p.v_viewangle_y);
        vector at = p.origin + v_forward * cvar("vr_test_spawn_dist");
        entity n = world;
        float best = 4096;
        for(entity e3 = nextent(world); e3; e3 = nextent(e3))
        {
            string k = substring(e3.classname, 0, 5);
            if(e3.modelindex > 0 && (k == "item_" || k == "weapo" || e3.classname == "thrown_weapon" ||
               ((e3.flags & FL_MONSTER) && e3.health > 0)) &&
               vlen(e3.origin - at) < best)
            {
                n = e3;
                best = vlen(e3.origin - at);
            }
        }
        if(n == world)
        {
            bprint(sprintf("adspawn: %g: nothing made: FAIL\n", cvar("vr_test_spawn")));
            return;
        }
        vector mid = (n.absmin + n.absmax) * 0.5;
        vector flat = mid - p.origin;
        float dz = flat_z;
        flat_z = 0;
        float dist = vlen(flat);
        float ahead = dist > 0 ? (flat * v_forward) / dist : 0;
        float near = (dist < max(128, cvar("vr_test_spawn_dist") + 64) && ahead > 0.7 && fabs(dz) < 64);
        bprint(sprintf("adspawn: %g: %s (%s) at %g ahead x%g, height %g: %s\n", cvar("vr_test_spawn"), n.classname, n.model,
            rint(dist), rint(ahead * 100) / 100, rint(dz), near ? "PASS" : "FAIL"));
        if(near)
        {
            remove(n);
        }
        return;
    }
    if(request == 84)
    {
        float grip2 = cvar("vr_weapon_grip_mode");
        cvar_set("vr_weapon_grip_mode", "1");
        bprint(sprintf("adweapons: the main hand fires: %s\n", VR_ADWeapons_Fire(p, "held") ? "PASS" : "FAIL"));
        cvar_set("vr_weapon_grip_mode", ftos(grip2));
        return;
    }
    if(request == 92)
    {
        // 92: the nearest AD pyro, fumigator or plasma soldier (plasma, eliminator, centurion) to you fires once, as its
        // own attack does, at the monster or body nearest it (none: 200 units ahead of it); again for a stream.
        entity sh = world;
        float shd = 2048;
        for(entity e92 = findflags(world, flags, FL_MONSTER); e92; e92 = findflags(e92, flags, FL_MONSTER))
        {
            float k92 = e92.ad_classtype;
            if(e92.health > 0 && vlen(e92.origin - p.origin) < shd && (k92 == ad_CT_MONPYRO || k92 == ad_CT_MONFUMIGATOR ||
                k92 == ad_CT_MONARMYPLASMA || k92 == ad_CT_MONELIMATOR || k92 == ad_CT_MONCENTURION))
            {
                sh = e92;
                shd = vlen(e92.origin - p.origin);
            }
        }
        if(sh == world)
        {
            bprint("adfx: no AD pyro, fumigator or plasma soldier near\n");
            return;
        }
        entity tg = world;
        float tgd = 1024;
        for(entity t92 = findflags(world, flags, FL_MONSTER); t92; t92 = findflags(t92, flags, FL_MONSTER))
        {
            if(t92 != sh && t92.takedamage && vlen(t92.origin - sh.origin) < tgd)
            {
                tg = t92;
                tgd = vlen(t92.origin - sh.origin);
            }
        }
        makevectors(sh.angles);
        vector from92 = sh.origin + '0 0 12' + v_forward * 16;
        vector to92 = tg != world ? (tg.absmin + tg.absmax) * 0.5 : from92 + v_forward * 200;
        vector dir92 = normalize(to92 - from92);
        sh.angles_y = vectoyaw(dir92);
        entity os92 = self;
        self = sh;
        if(sh.ad_classtype == ad_CT_MONPYRO || sh.ad_classtype == ad_CT_MONFUMIGATOR)
        {
            ad_pyroflame_spawn(from92, dir92);
            if(tg != world && tg.health > 0 && (tg.flags & FL_ONGROUND))
            {
                // (A flame is a trigger: it touches what moves into it. A standing target hops, as one walking would.)
                tg.flags = tg.flags - FL_ONGROUND;
                tg.velocity_z = 80;
            }
        }
        else
        {
            ad_launch_plasma(from92, dir92, sh.ad_classtype, ad_SPEED_PLASMA);
            ad_newmis.velocity = dir92 * ad_SPEED_PLASMA;
            ad_newmis.angles = vectoangles(ad_newmis.velocity);
        }
        self = os92;
        dprint(sprintf("adfx: %s fires at %s (health %g, takedamage %g, poisoned %g)\n", sh.classname, tg.classname,
            tg.health, tg.takedamage, tg.vr_adpoison != world));
        return;
    }
    if(request == 90)
    {
        if(!cvar("vr_ad_mounted"))
        {
            bprint("adgun shots: Arcane Dimensions' data is not loaded\n");
            return;
        }
        AD_WeaponsOnDemand();
        entity e90 = spawn();
        e90.classname = "vr_adgun_test";
        e90.owner = p;
        vr_adgun_t90_phase = 0;
        vr_adgun_test_step = 0;
        vr_adgun_test_pass = vr_adgun_test_fail = 0;
        vr_adgun_test_grip = cvar("vr_weapon_grip_mode");
        cvar_set("vr_weapon_grip_mode", "1");
        e90.think = VR_ADGun_ShotsTestThink;
        e90.nextthink = time + 0.1;
        bprint("adgun shots: the launchers' and plasma guns' shots from your main hand\n");
        return;
    }
    if(request == 70)
    {
        if(!cvar("vr_ad_mounted"))
        {
            bprint("adgun test: Arcane Dimensions' data is not loaded (its guns' models are its monsters')\n");
            return;
        }
        AD_WeaponsOnDemand(); // (the sounds AD's soldiers precache where they are: none here)
        entity e = spawn();
        e.classname = "vr_adgun_test";
        e.owner = p;
        vr_adgun_test_index = 0;
        vr_adgun_test_step = 0;
        vr_adgun_test_pass = vr_adgun_test_fail = 0;
        vr_adgun_test_grip = cvar("vr_weapon_grip_mode");
        cvar_set("vr_weapon_grip_mode", "1"); // (as it was again at the end)
        e.think = VR_ADGun_TestThink;
        e.nextthink = time + 0.1;
        bprint("adgun test: each gun fired from your main hand at a target ahead\n");
        return;
    }
    if(request == 71)
    {
        // Each gun dropped from a soldier's place ahead (VR_DropEnemyGun as VR_ADGun_Drop calls it): its model and clip.
        float pass = 0, fail = 0;
        makevectors(p.angles);
        entity s = spawn();
        setorigin(s, p.origin + v_forward * 96);
        s.angles = p.angles;
        entity oldSelf = self;
        for(float ci = 0; ci < 8; ci++)
        {
            // (each armed soldier's drop)
            float wid = VR_ADGun_ForClass(ci == 0 ? ad_CT_MONARMYROCKET : (ci == 1 ? ad_CT_MONARMYGRENADE :
                (ci == 2 ? ad_CT_MONARMYPLASMA : (ci == 3 ? ad_CT_MONDEFENDER : (ci == 4 ? ad_CT_MONELIMATOR :
                (ci == 5 ? ad_CT_MONCENTURION : (ci == 6 ? ad_CT_MONPYRO : ad_CT_MONFUMIGATOR)))))));
            self = s;
            VR_DropEnemyGun(wid);
            self = oldSelf;
            entity w = world;
            for(entity e2 = find(world, classname, "thrown_weapon"); e2; e2 = find(e2, classname, "thrown_weapon"))
            {
                if(e2.weapon == wid && e2.vr_drop_born == time)
                {
                    w = e2;
                }
            }
            float ok = (w != world && w.model == WeaponIdToModel(wid, 0) && w.weaponinst.wi_clip == WeaponIdToBaseClipSize(wid) &&
                        w.modelindex > 0);
            bprint(sprintf("adgun drop: %s (%g): %s, %g rounds: %s\n", VR_IsADGun(wid) ? VR_ADGun_Name(wid) :
                WeaponIdToWeaponName(wid), wid, w != world ? w.model : "none",
                w != world ? w.weaponinst.wi_clip : 0, ok ? "PASS" : "FAIL"));
            if(ok) { pass++; } else { fail++; }
        }
        remove(s);
        bprint(sprintf("adgun drop: %g passed, %g failed\n", pass, fail));
        return;
    }
    if(request >= 73 && request <= 80)
    {
        // The gun `request - 73` (in WID_ADROCKET's order) in your main hand, full (with vr_weapon_grip_mode 1, or the
        // grip held, else it drops at once): a look at it held.
        float gw = WID_ADROCKET + request - 73;
        entity was = self;
        self = p;
        VR_ADGun_Ensure(gw);
        VR_WeaponUtil_EntAddWeapon(self, gw);
        VRGiveEntWeapon(self, cVR_MainHand, gw, 0, WeaponIdToBaseClipSize(gw));
        W_SetCurrentAmmoFor(cVR_MainHand);
        self = was;
        bprint(sprintf("adgun: %s in your main hand\n", VR_ADGun_Name(gw)));
        return;
    }
    if(request == 87)
    {
        // Carried through a level change: the rocket soldier's launcher in the main hand (with vr_weapon_grip_mode 1)
        // and the plasma soldier's rifle in the left shoulder holster; then `changelevel` to a map without those
        // soldiers (or vr_adgun_drops 0) and `vr_ad_test 88`.
        entity was2 = self;
        self = p;
        VR_ADGun_Ensure(WID_ADROCKET);
        VR_WeaponUtil_EntAddWeapon(self, WID_ADROCKET);
        VRGiveEntWeapon(self, cVR_MainHand, WID_ADROCKET, 0, WeaponIdToBaseClipSize(WID_ADROCKET));
        W_SetCurrentAmmoFor(cVR_MainHand);
        self = was2;
        VR_ADGun_Ensure(WID_ADPLASMA);
        p.holsterweapon0 = WID_ADPLASMA;
        p.holsterweaponflags0 = 0;
        p.holsterweaponinst0 = WeaponInst_Carried(WID_ADPLASMA, WeaponIdToBaseClipSize(WID_ADPLASMA), 0);
        p.holsterweaponmodel0 = WeaponIdToModel(WID_ADPLASMA, 0);
        bprint(sprintf("adgun carry: armed %s (%s), holster %s (%s)\n", VR_ADGun_Name(p.weapon), p.weaponmodel,
            VR_ADGun_Name(p.holsterweapon0), p.holsterweaponmodel0));
        return;
    }
    if(request == 88)
    {
        float okHand = p.weapon == WID_ADROCKET && p.weaponmodel == WeaponIdToModel(WID_ADROCKET, 0);
        float okHolster = p.holsterweapon0 == WID_ADPLASMA && p.holsterweaponmodel0 == WeaponIdToModel(WID_ADPLASMA, 0);
        bprint(sprintf("adgun carry: %s: hand %g (%s), holster %g (%s), drops %g: %s\n", mapname, p.weapon,
            p.weaponmodel, p.holsterweapon0, p.holsterweaponmodel0, cvar("vr_adgun_drops"),
            (okHand && okHolster) ? "PASS" : "FAIL"));
        return;
    }
    if(request == 89)
    {
        // A gun model the engine cannot make (as another release of AD's model, or a replacement pack): id's soldier's
        // model named as an AD gun. Quake VR's stand-in's file loads under the name (before: a Host_Error).
        entity g = spawn();
        setmodel(g, sprintf("%s#gun0", "progs/soldier.mdl")); // (made: setmodel precaches it late)
        bprint(sprintf("adgun standin: %s index %g size %v: %s\n", g.model, g.modelindex, g.size,
            (g.modelindex > 0 && vlen(g.size) > 0) ? "PASS" : "FAIL"));
        remove(g);
        return;
    }
    if(request == 72)
    {
        for(float w2 = WID_ADROCKET; w2 <= WID_ADFUMIGATOR; w2++)
        {
            float st = (w2 == WID_ADPYRO || w2 == WID_ADFUMIGATOR);
            if(w2 == WID_ADROCKET || w2 == WID_ADGRENADE)
            {
                bprint(sprintf("%s (%s): Quake VR's %s, ", VR_ADGun_Name(w2), WeaponIdToModel(w2, 0),
                    w2 == WID_ADROCKET ? "rocket" : "grenade"));
            }
            else
            {
                bprint(sprintf("%s (%s): damage %g, splash %g, speed %g, ", VR_ADGun_Name(w2), WeaponIdToModel(w2, 0),
                    VR_ADGun_Value(w2, "damage"), VR_ADGun_Value(w2, "splash"), VR_ADGun_Value(w2, "speed")));
            }
            bprint(sprintf("%s %g, %s %g\n", st ? "fuel" : "ammo", VR_ADGun_ClipSize(w2), st ? "rate" : "refire",
                VR_ADGun_Value(w2, st ? "rate" : "refire")));
        }
        return;
    }
};
