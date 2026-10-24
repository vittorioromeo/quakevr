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
    if(WID_ADROCKET + vr_adgun_test_index == WID_ADELIMINATOR)
    {
        vr_adgun_test_index = vr_adgun_test_index + 1; // (gone: the centurion's)
    }
    vr_adgun_test_step = 0;
    if(vr_adgun_test_index > WID_ADCROSSBOW - WID_ADROCKET)
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

// vr_ad_test 91: the flamethrower (phase 0) and the poison sprayer (1) fired from the main hand at the monster nearest
// you (its health raised, held ahead of the muzzle; each new flame's or puff's touch on it called, as its flight into it
// would: test 70 aims the streams): what it took, the gore, wounds and blood sent (none: fire and
// poison do not bleed), and what it is left with (burning; poisoned). Phase 2 a shot's damage on it (the counters'
// control: it does bleed).
// vr_ad_test 101: the two streams' flames and puffs flying into the monster nearest you (one awake first; else the
// map's own waiting for its trigger, woken by it), held with its near side vr_test_spawn_dist units (50 if 0) along the
// main hand's aim (level it: vr_mock_hand main 0.25 1.2 -0.4 70 0 0): what it took (`adgun fly: ... dealt N`; PASS: some,
// and burning or poisoned). A big monster held off at a distance, the minotaur's, the reach's test.
entity vr_adgun_t73_mon;
float vr_adgun_t73_phase;
float vr_adgun_t73_gore, vr_adgun_t73_wound, vr_adgun_t73_blood, vr_adgun_t73_health;
float vr_adgun_t73_fly; // vr_ad_test 101: the flames and puffs fly into the monster (no touch called for them)

void(entity p) VR_ADGun_FxTestPlace =
{
    entity m = vr_adgun_t73_mon;
    entity oldSelf = self;
    self = p;
    vector fwd = VRGetWeaponFireRotFwd(cVR_MainHand);
    vector at = VRGetMuzzlePos(cVR_MainHand);
    self = oldSelf;
    if(vr_adgun_t73_fly)
    {
        // Its near side 50 units ahead of the muzzle, the aim through its middle (a big one's too).
        float gap = cvar("vr_test_spawn_dist") > 0 ? cvar("vr_test_spawn_dist") : 50;
        at = at + fwd * (gap + max(m.maxs_x, m.maxs_y));
        setorigin(m, at - (m.mins + m.maxs) * 0.5);
        m.velocity = '0 0 0';
        return;
    }
    at = at + fwd * 70;
    setorigin(m, at - '0 0 8');
    m.velocity = '0 0 0';
};

void() VR_ADGun_FxTestThink =
{
    entity p = self.owner;
    entity m = vr_adgun_t73_mon;
    entity oldSelf = self;
    float ph = vr_adgun_t73_phase;
    float wid = ph == 0 ? WID_ADPYRO : WID_ADFUMIGATOR;
    if(m == world || m.health <= 0)
    {
        bprint("adgun fx: the monster is gone: FAIL\n");
        vr_adgun_test_fail++;
        remove(self);
        return;
    }
    if(vr_adgun_test_step == 0)
    {
        m.health = 2000;
        vr_adgun_t73_health = m.health;
        vr_adgun_t73_gore = vr_gore_sent;
        vr_adgun_t73_wound = vr_wound_sent;
        vr_adgun_t73_blood = vr_blood_sent;
        if(ph < 2)
        {
            self = p;
            VR_ADGun_Ensure(wid);
            VR_WeaponUtil_EntAddWeapon(self, wid);
            VRGiveEntWeapon(self, cVR_MainHand, wid, 0, WeaponIdToBaseClipSize(wid));
            W_SetCurrentAmmoFor(cVR_MainHand);
            self = oldSelf;
        }
    }
    VR_ADGun_FxTestPlace(p);
    if(vr_adgun_test_step <= 5)
    {
        if(ph < 2)
        {
            self = p;
            VR_EnemyGun_Attack(cVR_MainHand, wid, 0, &self.attack_finished);
            // Its new flame (or puff) meets the monster: its touch, as its flight's into it would be.
            entity fl = world;
            for(entity f2 = find(world, classname, "vr_adflame"); f2; f2 = find(f2, classname, "vr_adflame"))
            {
                if(f2.owner == p && f2.cnt == 0 && f2.vr_adgun_wid == wid)
                {
                    fl = f2;
                }
            }
            if(fl != world && !vr_adgun_t73_fly)
            {
                setorigin(fl, (m.absmin + m.absmax) * 0.5);
                self = fl;
                other = m;
                VR_ADGun_FlameTouch();
            }
            self = oldSelf;
        }
        else if(vr_adgun_test_step == 0)
        {
            vector mid = (m.absmin + m.absmax) * 0.5;
            VR_SetHitKind(QVR_HITKIND_SHOT, cVR_MainHand, mid, '1 0 0');
            T_Damage(m, p, p, 10);
            VR_ClearHitKind();
        }
    }
    vr_adgun_test_step = vr_adgun_test_step + 1;
    if(vr_adgun_test_step < 20)
    {
        self.nextthink = time + 0.1;
        return;
    }
    float gore = vr_gore_sent - vr_adgun_t73_gore, wound = vr_wound_sent - vr_adgun_t73_wound;
    float blood = vr_blood_sent - vr_adgun_t73_blood, dealt = vr_adgun_t73_health - m.health;
    float burning = (m.burn_fire != world), poisoned = (m.vr_adpoison != world);
    float ok = (dealt > 0);
    if(vr_adgun_t73_fly)
    {
        // (101: what flew into it hurt it; it burns or is poisoned)
        ok = (ok && (ph == 0 ? burning : poisoned));
        bprint(sprintf("adgun fly: %s on %s: dealt %g, burning %g, poisoned %g: %s\n", VR_ADGun_Name(wid), m.classname, dealt,
            burning, poisoned, ok ? "PASS" : "FAIL"));
        if(ok) { vr_adgun_test_pass++; } else { vr_adgun_test_fail++; }
        vr_adgun_test_step = 0;
        vr_adgun_t73_phase = ph + 1;
        if(vr_adgun_t73_phase > 1)
        {
            bprint(sprintf("adgun fly: %g passed, %g failed\n", vr_adgun_test_pass, vr_adgun_test_fail));
            cvar_set("vr_weapon_grip_mode", ftos(vr_adgun_test_grip));
            remove(self);
            return;
        }
        self.nextthink = time + 0.3;
        return;
    }
    if(ph < 2)
    {
        ok = (ok && gore == 0 && wound == 0 && blood == 0 && (ph == 0 ? burning : poisoned));
    }
    else
    {
        ok = (ok && gore > 0 && wound > 0);
    }
    bprint(sprintf("adgun fx: %s on %s: dealt %g, gore %g, wounds %g, blood %g, ", ph < 2 ? VR_ADGun_Name(wid) : "a shot (control)",
        m.classname, dealt, gore, wound, blood));
    bprint(sprintf("burning %g, poisoned %g: %s\n", burning, poisoned, ok ? "PASS" : "FAIL"));
    if(ok) { vr_adgun_test_pass++; } else { vr_adgun_test_fail++; }
    vr_adgun_test_step = 0;
    vr_adgun_t73_phase = ph + 1;
    if(vr_adgun_t73_phase > 2)
    {
        bprint(sprintf("adgun fx: %g passed, %g failed\n", vr_adgun_test_pass, vr_adgun_test_fail));
        cvar_set("vr_weapon_grip_mode", ftos(vr_adgun_test_grip));
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

// vr_ad_test 93 (ROUND22 "AD crossbow"): the crossbow knight's crossbow, end to end. A crossbow knight near you (the
// toolgun's or `vr_test_spawn 202; impulse 241`) is killed (T_Damage, its own death: it drops its crossbow), the drop
// taken into your main hand and fired at a target ahead (the bolt's damage), holstered at the left hip, drawn into the
// off hand and fired again (its bolts its own: one less each). One "adxb:" line a step, then PASS or FAIL.
float vr_adxb_step;
float vr_adxb_ok;
float vr_adxb_grip;
float vr_adxb_clip; // the rounds in it before the step's shot

void(entity p, float xHand) VR_ADXbow_Target =
{
    VR_ADGun_TestPlace(p, 64);
    entity was = self;
    self = p;
    // (Short of the knight's corpse, 150 ahead; near enough that, the mock's hand aiming steeply down, the floor is not met
    // first by a bolt leaving from its groove, 10 behind the muzzle.)
    vector at = VRGetMuzzlePos(xHand) + VRGetWeaponFireRotFwd(xHand) * 40;
    self = was;
    setorigin(vr_adgun_test_target, at - '0 0 4');
    vr_adgun_test_target.health = 100000;
    vr_adgun_hits = 0;
};

void(string xWhat, float xOk) VR_ADXbow_Report =
{
    bprint(sprintf("adxb: %s: %s\n", xWhat, xOk ? "PASS" : "FAIL"));
    vr_adxb_ok = (vr_adxb_ok && xOk);
};

// The hold of what `xHand` holds (vr_ad_test 94: both hands'): its muzzle from the hand along the gun's aim's axes
// (ahead along the shot, right, up).
void(entity p, float xHand) VR_ADXbow_Hold =
{
    entity was = self;
    self = p;
    vector toMuzzle = VRGetMuzzlePos(xHand) - VRGetHandPos(xHand);
    makevectors(VRGetWeaponFireRot(xHand));
    self = was;
    bprint(sprintf("adxb: hold (hand %g, weapon %g): muzzle %.1f ahead of the hand along the shot, %.1f right, %.1f up\n",
        xHand, VRGetEntWeapon(p, xHand), toMuzzle * v_forward, toMuzzle * v_right, toMuzzle * v_up));
};

void() VR_ADXbow_Think =
{
    entity p = self.owner;
    entity was = self;
    float hit;
    if(vr_adxb_step == 0)
    {
        entity k = world;
        for(entity m = nextent(world); m; m = nextent(m))
        {
            if(m.ad_classtype == ad_CT_MONDCROSS && m.health > 0 && (!k || vlen(m.origin - p.origin) < vlen(k.origin - p.origin)))
            {
                k = m;
            }
        }
        VR_ADXbow_Report("a living crossbow knight near you", k != world);
        if(!k)
        {
            remove(self);
            return;
        }
        T_Damage(k, p, p, k.health + 5);
        VR_ADXbow_Report(sprintf("killed (health %g)", k.health), k.health <= 0);
        vr_adxb_step = 1;
        self.nextthink = time + 1.5; // (the drop lands)
        return;
    }
    if(vr_adxb_step == 1)
    {
        entity w = world;
        for(entity t = find(world, classname, "thrown_weapon"); t; t = find(t, classname, "thrown_weapon"))
        {
            if(t.weapon == WID_ADCROSSBOW)
            {
                w = t;
            }
        }
        VR_ADXbow_Report(sprintf("its crossbow lying (%s, %g bolts)", w ? w.model : "none", w ? w.weaponinst.wi_clip : 0),
            w != world && w.modelindex > 0 && w.weaponinst.wi_clip == VR_ADGun_ClipSize(WID_ADCROSSBOW));
        if(!w)
        {
            remove(self);
            return;
        }
        VR_ADWeapons_Take(p, w);
        VR_ADXbow_Report(sprintf("taken into the main hand (weapon %g, model %s)", VRGetEntWeapon(p, cVR_MainHand),
            p.weaponmodel), VRGetEntWeapon(p, cVR_MainHand) == WID_ADCROSSBOW);
        vr_adxb_step = 5; // (fired in a second: the client draws its bolt in the groove first, .launchofs)
        self.nextthink = time + 1;
        return;
    }
    if(vr_adxb_step == 5)
    {
        VR_ADXbow_Target(p, cVR_MainHand);
        vr_adxb_clip = VRGetEntWeaponClip(p, cVR_MainHand);
        self = p;
        self.attack_finished = 0;
        entity before = ad_newmis;
        W_AttackImpl(cVR_MainHand, WID_ADCROSSBOW, self.weaponflags, 0, &self.weapon, &self.attack_finished); // (the trigger)
        // Its bolt (his notes 2026-10-19): 1.5 times as fast as the knights' (vr_adgun_crossbow_speed; AD's 700 on normal
        // skill), along the shot, leaving from the groove where the client draws it (.launchofs, in the shot's frame).
        entity bolt = ad_newmis;
        vector fwd = VRGetWeaponFireRotFwd(cVR_MainHand);
        vector groove = VRGetMuzzlePos(cVR_MainHand);
        if(cvar("vr_adgun_crossbow_anim") && self.launchofs != '0 0 0')
        {
            makevectors(VRGetWeaponFireRot(cVR_MainHand));
            groove = groove + v_forward * self.launchofs_x + v_right * self.launchofs_y + v_up * self.launchofs_z;
        }
        self = was;
        float bspeed = (bolt != before && bolt.vr_adgun_wid == WID_ADCROSSBOW) ? vlen(bolt.velocity) : 0;
        float bwant = cvar("vr_adgun_crossbow_speed");
        VR_ADXbow_Report(sprintf("its bolt at %.0f u/s (setting %g; the knights' %g on this skill, 700 on normal), along the "
            "shot %.4f", bspeed, bwant, ad_SPEED_DCROSS + ad_skill * ad_SPEED_BOLTSKILL, (bspeed > 0) ? (normalize(bolt.velocity) * fwd) : 0),
            bspeed > 0 && fabs(bspeed - bwant) < bwant * 0.05 + 10 && bwant >= 1.5 * 700 - 1 &&
            normalize(bolt.velocity) * fwd > 0.999);
        VR_ADXbow_Report(sprintf("its bolt leaves from the drawn bolt's place (%.2f off; launch offset %v)",
            (bspeed > 0) ? vlen(bolt.origin - groove) : -1, p.launchofs), bspeed > 0 && vlen(bolt.origin - groove) < 0.5 &&
            (!cvar("vr_adgun_crossbow_anim") || p.launchofs != '0 0 0'));
        vr_adxb_step = 2;
        self.nextthink = time + 1;
        return;
    }
    if(vr_adxb_step == 2)
    {
        hit = 100000 - vr_adgun_test_target.health;
        VR_ADXbow_Report(sprintf("main hand fired: bolts %g -> %g, the target took %g", vr_adxb_clip,
            VRGetEntWeaponClip(p, cVR_MainHand), hit), hit > 0 && VRGetEntWeaponClip(p, cVR_MainHand) == vr_adxb_clip - 1);
        vr_adxb_clip = VRGetEntWeaponClip(p, cVR_MainHand);
        self = p;
        // (the left hip emptied first: a start's axe there)
        setHolsterWeapon(QVR_HS_LEFT_HIP_HOLSTER, WID_FIST);
        setHolsterWeaponModel(QVR_HS_LEFT_HIP_HOLSTER, WeaponIdToModel(WID_FIST, 0));
        setHolsterWeaponFlags(QVR_HS_LEFT_HIP_HOLSTER, 0);
        VRSetEntHolsterWeaponInst(p, QVR_HS_LEFT_HIP_HOLSTER, world);
        float freeHip = (getHolsterWeapon(QVR_HS_LEFT_HIP_HOLSTER) == WID_FIST);
        if(freeHip)
        {
            VRHolsterHandWeapon(p, cVR_MainHand, QVR_HS_LEFT_HIP_HOLSTER);
        }
        float inHolster = getHolsterWeapon(QVR_HS_LEFT_HIP_HOLSTER);
        entity hinst = VRGetEntHolsterWeaponInst(p, QVR_HS_LEFT_HIP_HOLSTER);
        self = was;
        VR_ADXbow_Report(sprintf("holstered at the left hip (holster %g, model %s, %g bolts; hand %g)", inHolster,
            p.holsterweaponmodel2, hinst ? hinst.wi_clip : -1, VRGetEntWeapon(p, cVR_MainHand)),
            freeHip && inHolster == WID_ADCROSSBOW && p.holsterweaponmodel2 == WeaponIdToModel(WID_ADCROSSBOW, 0) &&
            hinst.wi_clip == vr_adxb_clip && VRGetEntWeapon(p, cVR_MainHand) == WID_FIST);
        self = p;
        VRSetHandEmpty(p, cVR_OffHand);
        VRDrawHolsterWeapon(p, cVR_OffHand, QVR_HS_LEFT_HIP_HOLSTER);
        self = was;
        VR_ADXbow_Report(sprintf("drawn into the off hand (weapon %g, %g bolts)", VRGetEntWeapon(p, cVR_OffHand),
            VRGetEntWeaponClip(p, cVR_OffHand)), VRGetEntWeapon(p, cVR_OffHand) == WID_ADCROSSBOW &&
            VRGetEntWeaponClip(p, cVR_OffHand) == vr_adxb_clip);
        VR_ADXbow_Target(p, cVR_OffHand);
        self = p;
        VR_EnemyGun_Attack(cVR_OffHand, WID_ADCROSSBOW, 0, &self.attack_finished);
        self = was;
        vr_adxb_step = 3;
        self.nextthink = time + 1;
        return;
    }
    hit = 100000 - vr_adgun_test_target.health;
    VR_ADXbow_Report(sprintf("off hand fired: bolts %g -> %g, the target took %g", vr_adxb_clip,
        VRGetEntWeaponClip(p, cVR_OffHand), hit), hit > 0 && VRGetEntWeaponClip(p, cVR_OffHand) == vr_adxb_clip - 1);
    VR_ADXbow_Hold(p, cVR_OffHand);
    bprint(sprintf("adxb: crossbow test %s\n", vr_adxb_ok ? "PASS" : "FAIL"));
    cvar_set("vr_weapon_grip_mode", ftos(vr_adxb_grip));
    remove(vr_adgun_test_target);
    vr_adgun_test_target = world;
    remove(self);
};

// vr_ad_test 95 (ROUND22 "Freddie's super nailgun"): a Freddie near you (the toolgun's or `vr_test_spawn 234; impulse
// 241`) is killed and his gun arm (the left forearm) cut off: a super nailgun of ours lies where it was, loaded as a
// dropped enemy gun (a full clip), and is taken into your main hand; his body then gibbed: no second one. vr_ad_test 102
// (ROUND22 "Monster weapon drops"): a Freddie near you gibbed whole (his arm not cut): one super nailgun lies, loaded,
// taken into your main hand. "adfred:" lines, then PASS or FAIL.
float vr_adfred_step;
float vr_adfred_ok;
float vr_adfred_gib; // vr_ad_test 102: gibbed whole
float vr_adfred_drops;

void(string xWhat, float xOk) VR_ADFred_Report =
{
    bprint(sprintf("adfred: %s: %s\n", xWhat, xOk ? "PASS" : "FAIL"));
    vr_adfred_ok = (vr_adfred_ok && xOk);
};

void() VR_ADFred_Think =
{
    entity p = self.owner;
    if(vr_adfred_step == 0)
    {
        entity k = world;
        for(entity m = nextent(world); m; m = nextent(m))
        {
            if(m.ad_classtype == ad_CT_MONFREDDIE && m.health > 0 && (!k || vlen(m.origin - p.origin) < vlen(k.origin - p.origin)))
            {
                k = m;
            }
        }
        VR_ADFred_Report("a living Freddie near you", k != world);
        if(!k)
        {
            remove(self);
            return;
        }
        self.enemy = k;
        if(vr_adfred_gib)
        {
            vr_adfred_drops = vr_adgun_freddie_drops;
            k.health = 1; // (gibbed whole: far past his gib health)
            T_Damage(k, p, p, 1000);
            VR_ADFred_Report(sprintf("gibbed whole (health %g, gibbed %g)", k.health, k.ad_gibbed), k.ad_gibbed);
            VR_ADFred_Report(sprintf("a super nailgun dropped (%g)", vr_adgun_freddie_drops - vr_adfred_drops),
                vr_adgun_freddie_drops == vr_adfred_drops + 1);
            vr_adfred_step = 2;
            self.nextthink = time + 1.5; // (the drop lands)
            return;
        }
        k.health = 1; // (killed, not gibbed: a body to cut)
        T_Damage(k, p, p, 5);
        VR_ADFred_Report(sprintf("killed (health %g)", k.health), k.health <= 0);
        vr_adfred_step = 1;
        self.nextthink = time + 0.3;
        return;
    }
    if(vr_adfred_step == 1)
    {
        vr_adfred_drops = vr_adgun_freddie_drops;
        vr_limb_last = world;
        float cut = VR_Limb_Cut(self.enemy, p, '0 -200 100', QVR_DECAP_SWING, VR_AD_FREDDIE_GUNBONE, FALSE);
        VR_ADFred_Report(sprintf("his gun arm (bone %g) cut off: %s", VR_AD_FREDDIE_GUNBONE,
            vr_limb_last ? vr_limb_last.model : "no piece"), cut);
        VR_ADFred_Report(sprintf("a super nailgun dropped (%g)", vr_adgun_freddie_drops - vr_adfred_drops),
            vr_adgun_freddie_drops == vr_adfred_drops + 1);
        vr_adfred_step = 2;
        self.nextthink = time + 1.5; // (the drop lands)
        return;
    }
    entity w = world;
    for(entity t = find(world, classname, "thrown_weapon"); t; t = find(t, classname, "thrown_weapon"))
    {
        if(t.weapon == WID_SUPER_NAILGUN)
        {
            w = t;
        }
    }
    VR_ADFred_Report(sprintf("it lies (%s, %g in it, %.0f from his body)", w ? w.model : "none", w ? w.weaponinst.wi_clip : 0,
        w ? vlen(w.origin - self.enemy.origin) : -1),
        w != world && w.modelindex > 0 && w.weaponinst.wi_clip == WeaponIdToBaseClipSize(WID_SUPER_NAILGUN));
    if(w)
    {
        VR_ADWeapons_Take(p, w);
        VR_ADFred_Report(sprintf("taken into the main hand (weapon %g)", VRGetEntWeapon(p, cVR_MainHand)),
            VRGetEntWeapon(p, cVR_MainHand) == WID_SUPER_NAILGUN);
    }
    if(!vr_adfred_gib && self.enemy.ad_classtype == ad_CT_MONFREDDIE && !self.enemy.ad_gibbed)
    {
        // His body, the arm cut off, gibbed (AD's gibbing, as a blast's on a body: VR_AD_Gibbed): no second gun.
        vr_adfred_drops = vr_adgun_freddie_drops;
        entity os = self;
        self = self.enemy;
        ad_monster_ThrowGib();
        self = os;
        VR_ADFred_Report(sprintf("his body gibbed after the cut: no second super nailgun (%g)",
            vr_adgun_freddie_drops - vr_adfred_drops), self.enemy.ad_gibbed && vr_adgun_freddie_drops == vr_adfred_drops);
    }
    bprint(sprintf("adfred: Freddie's super nailgun test %s\n", vr_adfred_ok ? "PASS" : "FAIL"));
    remove(self);
};

// The fuelled guns' tanks lying about (vr_ad_enemyguns.qc "Fuel tanks"; ROUND22.md "Fuel tanks lying about"): 150 the
// count (tanks, fuelled guns lying about, pairs, tanks without their gun, guns without a tank) and each pair's distance;
// 151 the pyro's flamethrower in the main hand, 152 the fumigator's sprayer in the off hand (vr_weapon_grip_mode 1);
// 153 the main hand's gun dropped, 154 the off hand's; 155 the nearest fuelled gun lying about taken into the empty
// main hand; 156 the sprayer in holster 0 (the left shoulder's), the flamethrower in holster 2 (the left hip's); 157 the
// nearest fuelled gun lying about removed; 158 a sprayer made lying 64 ahead (the toolgun's spawner); 159 the nearest
// kicked hard (its rope drags its tank: vr_adgun_hose_info says whether they stay within it); 160 its tank carried in
// the empty main hand.
entity(entity p) AD_FuelTank_Nearest =
{
    entity best = world;
    for(entity t = find(world, classname, "thrown_weapon"); t; t = find(t, classname, "thrown_weapon"))
    {
        if(VR_FuelTank_Fuelled(t.weapon) && (!best || vlen(t.origin - p.origin) < vlen(best.origin - p.origin)))
        {
            best = t;
        }
    }
    return best;
};

void(float request, entity p) AD_FuelTankTest =
{
    entity was = self;
    if(request == 150)
    {
        float tanks = 0, guns = 0, paired = 0, orphans = 0, bare = 0;
        for(entity e = nextent(world); e; e = nextent(e))
        {
            if(e.classname == "vr_fueltank")
            {
                tanks++;
                entity g = VR_FuelTank_Gun(e);
                if(g)
                {
                    paired++;
                    bprint(sprintf("fueltank: gun %s at %v, tank %s at %v, %.2f m apart\n", g.netname, g.origin, e.model,
                        e.origin, vlen(e.origin - g.origin) / VRMetersToUnits()));
                }
                else
                {
                    orphans++;
                }
            }
            else if(e.classname == "thrown_weapon" && VR_FuelTank_Fuelled(e.weapon))
            {
                guns++;
                bare += !VR_FuelTank_Of(e);
            }
        }
        bprint(sprintf("fueltank: %g tanks, %g fuelled guns lying about, %g paired, %g orphan tanks, %g guns without a tank; "
            "%g made\n", tanks, guns, paired, orphans, bare, vr_fueltank_made));
        bprint(sprintf("fueltank: hands %g %g, holsters %g %g\n", p.weapon, p.weapon2, p.holsterweapon0, p.holsterweapon2));
        return;
    }
    if(request == 151 || request == 152)
    {
        float gw = request == 151 ? WID_ADPYRO : WID_ADFUMIGATOR;
        float hand = request == 151 ? cVR_MainHand : cVR_OffHand;
        self = p;
        VR_ADGun_Ensure(gw);
        VR_WeaponUtil_EntAddWeapon(self, gw);
        VRGiveEntWeapon(self, hand, gw, 0, WeaponIdToBaseClipSize(gw));
        W_SetCurrentAmmoFor(hand);
        self = was;
        bprint(sprintf("fueltank: %s in the %s hand\n", VR_ADGun_Name(gw), request == 151 ? "main" : "off"));
        return;
    }
    if(request == 153 || request == 154)
    {
        float dh = request == 153 ? cVR_MainHand : cVR_OffHand;
        float dw = VRGetEntWeapon(p, dh);
        self = p;
        DropWeaponInHand(p, dh, dw, VRGetEntWeaponFlags(p, dh));
        self = was;
        bprint(sprintf("fueltank: dropped %s from the %s hand\n", WeaponIdToWeaponName(dw), request == 153 ? "main" : "off"));
        return;
    }
    if(request == 155 || request == 157)
    {
        entity n = AD_FuelTank_Nearest(p);
        if(!n)
        {
            bprint("fueltank: no fuelled gun lying about\n");
            return;
        }
        bprint(sprintf("fueltank: %s at %v (its tank %s) %s\n", n.netname, n.origin, VR_FuelTank_Of(n) ? "there" : "none",
            request == 155 ? "taken" : "removed"));
        if(request == 155)
        {
            VR_ADWeapons_Take(p, n);
        }
        else
        {
            remove(n);
        }
        return;
    }
    if(request == 156)
    {
        VR_ADGun_Ensure(WID_ADFUMIGATOR);
        VR_ADGun_Ensure(WID_ADPYRO);
        p.holsterweapon0 = WID_ADFUMIGATOR;
        p.holsterweaponflags0 = 0;
        p.holsterweaponinst0 = WeaponInst_Carried(WID_ADFUMIGATOR, WeaponIdToBaseClipSize(WID_ADFUMIGATOR), 0);
        p.holsterweaponmodel0 = WeaponIdToModel(WID_ADFUMIGATOR, 0);
        p.holsterweapon2 = WID_ADPYRO;
        p.holsterweaponflags2 = 0;
        p.holsterweaponinst2 = WeaponInst_Carried(WID_ADPYRO, WeaponIdToBaseClipSize(WID_ADPYRO), 0);
        p.holsterweaponmodel2 = WeaponIdToModel(WID_ADPYRO, 0);
        bprint("fueltank: the sprayer in holster 0, the flamethrower in holster 2\n");
        return;
    }
    if(request == 160)
    {
        // The tank of the nearest fuelled gun lying about taken into the empty main hand, as a grip closing on it does
        // (carried: its gun hangs from it by the rope).
        entity ct = VR_FuelTank_Of(AD_FuelTank_Nearest(p));
        if(ct)
        {
            VR_Carry_Start(p, cVR_MainHand, ct);
        }
        bprint(sprintf("fueltank: tank %s, carried by %s\n", ct ? "there" : "none", ct.carry_player.classname));
        return;
    }
    if(request == 159)
    {
        // The nearest fuelled gun lying about kicked hard up and sideways: its rope drags its tank after it.
        entity k = AD_FuelTank_Nearest(p);
        if(k)
        {
            makevectors(p.angles_y * '0 1 0');
            k.velocity = v_right * 700 + '0 0 250';
            bprint(sprintf("fueltank: %s kicked from %v\n", k.netname, k.origin));
        }
        return;
    }
    if(request == 158)
    {
        entity m = VR_ADWeapons_Make(p, 319, 64);
        bprint(sprintf("fueltank: made %s, its tank %s\n", m ? m.netname : "nothing", VR_FuelTank_Of(m) ? "there" : "none"));
        return;
    }
    // Bursting (ROUND22.md "Fuel tanks burst"): 161 the nearest fuelled gun's tank shot by you for 10 (its health and
    // whether it burst); 162 the same gun's fuel set to `temp1` of its full (0 empty; set first) and its tank
    // shot for 1000; 163 the nearest fuelled gun's state: its flags' burst bit, spent, fuel, tank, force grab.
    if(request == 161 || request == 162)
    {
        entity bg = AD_FuelTank_Nearest(p);
        entity bt = VR_FuelTank_Of(bg);
        if(!bt)
        {
            bprint("fueltank: no tank to shoot\n");
            return;
        }
        if(request == 162 && bg.weaponinst)
        {
            bg.weaponinst.wi_clip = floor(WeaponIdToBaseClipSize(bg.weapon) * clamp(cvar("temp1"), 0, 1));
        }
        float bursts = vr_fueltank_bursts;
        T_Damage(bt, p, p, request == 161 ? 10 : 1000);
        bprint(sprintf("fueltank: shot %s's tank: health %g, burst %g, reached %g, fuel %g\n", bg.netname, bt.health,
            vr_fueltank_bursts - bursts, vr_fueltank_burst_hit, bg.weaponinst.wi_clip));
        return;
    }
    if(request == 164)
    {
        // A flamethrower whose tank burst in the main hand (spent: no fuel, no back tank, no hose).
        self = p;
        VR_ADGun_Ensure(WID_ADPYRO);
        VR_WeaponUtil_EntAddWeapon(self, WID_ADPYRO);
        VRGiveEntWeapon(self, cVR_MainHand, WID_ADPYRO, QVR_WPNFLAG_TANK_BURST, 0);
        W_SetCurrentAmmoFor(cVR_MainHand);
        self = was;
        bprint(sprintf("fueltank: a burst flamethrower in the main hand (flags %g)\n", p.weaponflags));
        return;
    }
    if(request == 163)
    {
        entity sg = AD_FuelTank_Nearest(p);
        if(!sg)
        {
            bprint(sprintf("fueltank: no fuelled gun lying about; hands %g (flags %g, clip %g) %g (flags %g, clip %g)\n",
                p.weapon, p.weaponflags, VRGetEntWeaponClip(p, cVR_MainHand), p.weapon2, p.weaponflags2,
                VRGetEntWeaponClip(p, cVR_OffHand)));
            return;
        }
        bprint(sprintf("fueltank: %s at %v: burst %g, depleted %g, fuel %g, ", sg.netname, sg.origin,
            (sg.weaponflags & QVR_WPNFLAG_TANK_BURST) ? 1 : 0, sg.vr_depleted, sg.weaponinst.wi_clip));
        bprint(sprintf("tank %s, force grab skips %g, bursts %g\n", VR_FuelTank_Of(sg) ? "there" : "none",
            VR_EnemyGun_DepletedSkipped(sg), vr_fueltank_bursts));
        bprint(sprintf("fueltank: hands %g (flags %g, clip %g) %g (flags %g, clip %g)\n", p.weapon, p.weaponflags,
            VRGetEntWeaponClip(p, cVR_MainHand), p.weapon2, p.weaponflags2, VRGetEntWeaponClip(p, cVR_OffHand)));
        return;
    }
};

// The burst's field and the armed monsters' tanks (ROUND22.md "Fuel tanks: the burst's field, the pickup, armed monsters'
// tanks"): 175 the fields now (each one's kind, time left, reach, its loop's volume step) and the sounds' state (loops
// and whooshes playing: 0 when they are all gone), the test grunt's health and state; 176 a grunt made just outside the
// nearest field (its edge and 48 more, to your right of it); 177 that grunt into the field's middle (it walks in);
// 178 an armed grunt with a flamethrower (temp1 1: a poison sprayer) 160 ahead, its back to you; 179 the nearest worn
// tank: its place on its wearer's back, its health, whether a shot from your eyes meets it; 180 that tank shot by you
// for 10 (three burst it: its wearer's state after; whom it is after: you, the hit alerting it); 182 an armed monster
// firing now (else the nearest wearer) killed by you: whether it was firing, its loop stopped. 175 also counts the 3D
// flames, the fields' effects sent and the monsters' streams ended (2026-10-24, ROUND22.md "Tank fields' look").
entity vr_tankfield_testmon; // 176's grunt
entity(entity p) VR_FuelTank_TestWorn;
entity(entity p, float xWid, float xDist) VR_FuelTank_TestWearer;

entity(entity p) AD_TankField_Nearest =
{
    entity best = world;
    for(entity f = find(world, classname, "vr_tankfield"); f; f = find(f, classname, "vr_tankfield"))
    {
        if(f.vr_tf_end > time && (!best || vlen(f.origin - p.origin) < vlen(best.origin - p.origin)))
        {
            best = f;
        }
    }
    return best;
};

void(entity e) AD_TankField_PrintMon =
{
    if(!e)
    {
        return;
    }
    float burning = (e.burn_fire != world && e.burn_fire.enemy == e);
    float poisoned = (e.vr_adpoison != world && e.vr_adpoison.enemy == e);
    bprint(sprintf("tankfield: %s at %v: health %g, burning %g (%.1f s more), ", e.classname, e.origin, e.health, burning,
        burning ? e.burn_fire.burn_end - time : 0));
    bprint(sprintf("poisoned %g (%.1f/s, %.1f s more)\n", poisoned, poisoned ? e.vr_adpoison.dmg : 0,
        poisoned ? e.vr_adpoison.attack_finished - time : 0));
};

void(float request, entity p) AD_TankFieldTest =
{
    if(request == 175)
    {
        float n = 0;
        for(entity f = find(world, classname, "vr_tankfield"); f; f = find(f, classname, "vr_tankfield"))
        {
            n++;
            // (sprintf takes 7 values at most: two prints)
            bprint(sprintf("tankfield: %s at %v: %.1f s left of %.1f, reach %.0f, ", f.vr_tf_kind == 1 ? "fire" : "poison",
                f.origin, f.vr_tf_end - time, f.vr_tf_end - f.vr_tf_start, f.vr_tf_radius));
            bprint(sprintf("strength %.2f, loop step %g, whoosh %s\n", VR_TankField_Strength(f), f.vr_tf_level,
                f.vr_tf_hush ? "on" : "off"));
        }
        bprint(sprintf("tankfield: time %.2f: %g fields (%g made), %g reached, loops playing %g, whooshes playing %g, ", time,
            n, vr_tankfield_made, vr_tankfield_hits, vr_tankfield_loops, vr_tankfield_whoosh));
        float flames = 0;
        for(entity fm = find(world, classname, "vr_tankflame"); fm; fm = find(fm, classname, "vr_tankflame"))
        {
            flames++;
        }
        bprint(sprintf("bursts %g, worn tanks made %g, 3D flames made %g (%g now), ", vr_fueltank_bursts, vr_fueltank_worn_made,
            vr_tankflame_made, flames));
        bprint(sprintf("monsters' streams ended %g (on death or a drop %g); ", vr_ag_stream_ends, vr_ag_stream_ends_gone));
        bprint(sprintf("fields' effects sent: fire %g, smoke %g, cloud %g\n", vr_tankfield_fx_fire, vr_tankfield_fx_smoke,
            vr_tankfield_fx_cloud));
        AD_TankField_PrintMon(vr_tankfield_testmon);
        return;
    }
    if(request == 176 || request == 177)
    {
        entity nf = AD_TankField_Nearest(p);
        if(!nf)
        {
            bprint("tankfield: no field\n");
            return;
        }
        makevectors('0 1 0' * p.angles_y);
        if(request == 176)
        {
            entity was = self;
            self = spawn();
            setorigin(self, nf.origin + v_right * (nf.vr_tf_radius + 48) + '0 0 25');
            self.angles_y = p.angles_y + 90;
            self.classname = "monster_army";
            monster_army();
            vr_tankfield_testmon = self;
            self = was;
        }
        else if(vr_tankfield_testmon)
        {
            setorigin(vr_tankfield_testmon, nf.origin + '0 0 25');
        }
        AD_TankField_PrintMon(vr_tankfield_testmon);
        return;
    }
    if(request == 181)
    {
        // A hit on the nearest tank (lying or worn): metal, never blood (VR_HitBlood and the shotgun's pellets' check).
        entity ht = world;
        for(entity e = nextent(world); e; e = nextent(e))
        {
            if(VR_FuelTank_Is(e) && e.modelindex && (!ht || vlen(e.origin - p.origin) < vlen(ht.origin - p.origin)))
            {
                ht = e;
            }
        }
        if(!ht)
        {
            bprint("tankfield: no tank to hit\n");
            return;
        }
        float blood = vr_blood_sent;
        VR_HitBlood(ht, ht.origin, '0 0 -100', 10);
        VR_HitBloodSplash(ht, ht.origin, 10);
        bprint(sprintf("tankfield: hits on %s: metal %g, blood particles sent %g\n", ht.classname, VR_Hit_Metal(ht),
            vr_blood_sent - blood));
        return;
    }
    if(request == 178)
    {
        entity w = VR_FuelTank_TestWearer(p, cvar("temp1") == 1 ? WID_ADFUMIGATOR : WID_ADPYRO, 160);
        bprint(sprintf("tankfield: %s armed with %s at %v\n", w.classname, w.vr_ag_prop.netname, w.origin));
        return;
    }
    entity t = VR_FuelTank_TestWorn(p);
    if(!t)
    {
        bprint("tankfield: no worn tank\n");
        return;
    }
    entity m = t.owner;
    if(request == 179)
    {
        makevectors('0 1 0' * m.angles_y);
        vector d = t.origin - m.origin;
        vector eye = p.origin + p.view_ofs;
        traceline(eye, t.origin, MOVE_HITGIBS | MOVE_HITMODEL, p); // (as a bullet: weapons.qc FireBullets)
        bprint(sprintf("tankfield: worn tank %s on %s: %.1f forward, %.1f right, %.1f up of its origin, ", t.model,
            m.classname, d * v_forward, d * v_right, d * v_up));
        bprint(sprintf("angles %v, health %g; a shot from your eyes meets %s\n", t.angles, t.health,
            trace_ent == t ? "the tank" : trace_ent.classname));
        return;
    }
    if(request == 180)
    {
        float bursts = vr_fueltank_bursts;
        float hp = m.health;
        T_Damage(t, p, p, 10);
        bprint(sprintf("tankfield: shot %s's worn tank: health %g, burst %g, reached %g; its wearer: health %g -> %g, "
            "holds %s\n", m.classname, t.health, vr_fueltank_bursts - bursts, vr_fueltank_burst_hit, hp, m.health,
            m.vr_ag_prop ? m.vr_ag_prop.netname : "nothing"));
        bprint(sprintf("tankfield: its wearer after %s, wearers alerted by their tank %g\n", m.enemy ? m.enemy.classname : "nothing",
            vr_fueltank_worn_alerts));
        AD_TankField_PrintMon(m);
        return;
    }
    if(request == 182)
    {
        // Its wearer killed by you, firing or not (one firing now first, anywhere): its flamethrower's loop stopped
        // (VR_ADGun_MonsterStreamEnd).
        for(entity fg = nextent(world); fg; fg = nextent(fg))
        {
            if(fg.vr_adgun_stream > 0 && (fg.flags & FL_MONSTER) && fg.health > 0)
            {
                m = fg;
            }
        }
        float firing = m.vr_adgun_stream > 0;
        float ends = vr_ag_stream_ends;
        T_Damage(m, p, p, 1000);
        bprint(sprintf("tankfield: killed %s (health %g), firing %g: its stream ended %g, still firing %g; ", m.classname,
            m.health, firing, vr_ag_stream_ends - ends, m.vr_adgun_stream > 0));
        float on = 0;
        for(entity g = nextent(world); g; g = nextent(g))
        {
            on += (g.vr_adgun_stream > 0 && (g.flags & FL_MONSTER));
        }
        bprint(sprintf("monsters firing now %g, streams ended %g (on death or a drop %g)\n", on, vr_ag_stream_ends,
            vr_ag_stream_ends_gone));
        return;
    }
};

void(float request, entity p) AD_GunTest =
{
    if(request >= 150 && request <= 164)
    {
        AD_FuelTankTest(request, p);
        return;
    }
    if(request >= 175 && request <= 182)
    {
        AD_TankFieldTest(request, p);
        return;
    }
    if(request >= 96 && request <= 100)
    {
        // AD's upgraded weapons' VR models (vr_adweapons.cpp; ROUND22.md "AD's weapons as VR models"): 96 the three
        // upgrades' pickups made ahead in a row, as the toolgun makes them (309 the Shadow Axe, 310 the Widowmaker, 311
        // the Plasma Gun); 97, 98, 99 the Shadow Axe, the Widowmaker or the Plasma Gun taken into the main hand, the other
        // two in the hip holsters (left, right); 100 the main hand's weapon dropped ahead. Each prints the models drawn.
        if(request == 96)
        {
            for(float u = 309; u <= 311; u++)
            {
                entity pk = VR_ADWeapons_Make(p, u, 64 + (u - 309) * 28);
                bprint(sprintf("adweapons-vr: %g %s model %s\n", u, pk != world ? pk.netname : "(not made)", pk != world ? pk.model : ""));
            }
            return;
        }
        if(request == 100)
        {
            float dw = VRGetEntWeapon(p, cVR_MainHand);
            entity os99 = self;
            self = p;
            DropWeaponInHand(p, cVR_MainHand, dw, VRGetEntWeaponFlags(p, cVR_MainHand));
            self = os99;
            for(entity t = find(world, classname, "thrown_weapon"); t; t = find(t, classname, "thrown_weapon"))
            {
                if(t.weapon == dw && t.vr_drop_born == time)
                {
                    bprint(sprintf("adweapons-vr: dropped %s model %s\n", WeaponIdToWeaponName(t.weapon), t.model));
                }
            }
            return;
        }
        // (given as a cheat gives them: each its own record, upgraded; the hand's model swapped by AD_SyncWeaponModels)
        float wids[3];
        wids[0] = WID_AXE; wids[1] = WID_SUPER_SHOTGUN; wids[2] = WID_LIGHTNING;
        float held = request - 97;
        float slot = 2;
        for(float k = 0; k < 3; k++)
        {
            float wk = wids[k];
            entity inst = WeaponInst_New(WeaponIdToBaseClipSize(wk));
            inst.wi_adup = AD_UpgradeFor(wk);
            if(k == held)
            {
                VRGiveEntWeapon(p, cVR_MainHand, wk, 0, WeaponIdToBaseClipSize(wk));
                p.weaponinst.wi_adup = AD_UpgradeFor(wk);
                p.weaponmodel = WeaponIdToModel(wk, 0);
                continue;
            }
            if(slot == 2) { p.holsterweapon2 = wk; p.holsterweaponflags2 = 0; p.holsterweaponinst2 = inst; p.holsterweaponmodel2 = WeaponIdToModel(wk, 0); }
            else          { p.holsterweapon3 = wk; p.holsterweaponflags3 = 0; p.holsterweaponinst3 = inst; p.holsterweaponmodel3 = WeaponIdToModel(wk, 0); }
            slot = 3;
        }
        AD_SyncWeaponModels(p);
        bprint(sprintf("adweapons-vr: main %s (%g) %s; hips %s %s\n", WeaponIdToWeaponName(p.weapon), AD_UpgradeCode(p.weaponinst),
            p.weaponmodel, p.holsterweaponmodel2, p.holsterweaponmodel3));
        return;
    }
    if(request == 95 || request == 102)
    {
        if(!cvar("vr_ad_mounted"))
        {
            bprint("adfred: Arcane Dimensions' data is not loaded\n");
            return;
        }
        entity fr = spawn();
        fr.classname = "vr_adfred_test";
        fr.owner = p;
        vr_adfred_step = 0;
        vr_adfred_ok = TRUE;
        vr_adfred_gib = request == 102;
        fr.think = VR_ADFred_Think;
        fr.nextthink = time + 0.1;
        return;
    }
    if(request == 94)
    {
        VR_ADXbow_Hold(p, cVR_MainHand);
        VR_ADXbow_Hold(p, cVR_OffHand);
        return;
    }
    if(request == 93)
    {
        if(!cvar("vr_ad_mounted"))
        {
            bprint("adxb: Arcane Dimensions' data is not loaded\n");
            return;
        }
        AD_WeaponsOnDemand();
        entity xb = spawn();
        xb.classname = "vr_adxb_test";
        xb.owner = p;
        vr_adxb_step = 0;
        vr_adxb_ok = TRUE;
        vr_adxb_grip = cvar("vr_weapon_grip_mode");
        cvar_set("vr_weapon_grip_mode", "1"); // (the gun stays in a hand not gripping; as it was again at the end)
        xb.think = VR_ADXbow_Think;
        xb.nextthink = time + 0.1;
        return;
    }
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
        // An older save's eliminator's plasma gun lying about (weapon 24, its old model): the centurion's on load.
        {
            entity old = spawn();
            old.classname = "thrown_weapon";
            old.weapon = WID_ADELIMINATOR;
            setorigin(old, p.origin + v_forward * 64);
            precache_model("progs/v_gruntgun.mdl#adv4"); // (the old name: the engine makes the centurion's)
            setmodel(old, "progs/v_gruntgun.mdl#adv4");
            setsize(old, '-4 -4 -4', '4 4 4');
            float changed = VR_ADGun_LegacyWorld();
            float okOld = old.weapon == WID_ADCENTURION && old.model == WeaponIdToModel(WID_ADCENTURION, 0) && old.size == '8 8 8' &&
                          VR_ADGun_ForClass(ad_CT_MONELIMATOR) == WID_ADCENTURION;
            bprint(sprintf("adgun drop: an older save's eliminator's gun: weapon %g, %s, %g changed: %s\n", old.weapon, old.model,
                changed, okOld ? "PASS" : "FAIL"));
            if(okOld) { pass++; } else { fail++; }
            remove(old);
        }
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
    if(request == 91 || request == 101)
    {
        vr_adgun_t73_fly = (request == 101); // (101: the flames and puffs fly into it: a big monster's too)
        if(!cvar("vr_ad_mounted"))
        {
            bprint("adgun fx: Arcane Dimensions' data is not loaded\n");
            return;
        }
        AD_WeaponsOnDemand();
        entity m73 = world;
        float d73 = vr_adgun_t73_fly ? 3072 : 1024;
        for(entity e73 = findflags(world, flags, FL_MONSTER); e73; e73 = findflags(e73, flags, FL_MONSTER))
        {
            // (101: one awake first, by 2048 units: one waiting for its trigger only if none is)
            float w73 = (vr_adgun_t73_fly && (e73.solid == SOLID_NOT || !e73.takedamage)) ? 2048 : 0;
            if(e73.health > 0 && !VR_IsDummy(e73) && vlen(e73.origin - p.origin) + w73 < d73)
            {
                m73 = e73;
                d73 = vlen(e73.origin - p.origin) + w73;
            }
        }
        if(m73 == world)
        {
            bprint("adgun fx: no monster near (vr_test_spawn 0; impulse 241)\n");
            return;
        }
        entity t73 = spawn();
        t73.classname = "vr_adgun_test";
        t73.owner = p;
        vr_adgun_t73_mon = m73;
        vr_adgun_t73_phase = 0;
        vr_adgun_test_step = 0;
        vr_adgun_test_pass = vr_adgun_test_fail = 0;
        vr_adgun_test_grip = cvar("vr_weapon_grip_mode");
        cvar_set("vr_weapon_grip_mode", "1");
        t73.think = VR_ADGun_FxTestThink;
        t73.nextthink = time + 0.1;
        if(vr_adgun_t73_fly && (m73.solid == SOLID_NOT || !m73.takedamage) && m73.use)
        {
            // (101: one waiting for its trigger, as the map's own are, woken by its trigger as the map would: in a while)
            entity os101 = self;
            activator = p;
            self = m73;
            m73.use();
            self = os101;
            t73.nextthink = time + 3;
            bprint(sprintf("adgun fly: %s woken by its trigger (%s)\n", m73.classname, m73.targetname));
        }
        bprint(sprintf("adgun fx: the flamethrower and the poison sprayer at %s\n", m73.classname));
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
