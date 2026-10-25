#ifdef QVR_TESTS
//
//
//
// ----------------------------------------------------------------------------
// The flare gun's tests (vr_flare_test; Debug > Tests > Flare Gun; vr_flaregun.qc)
// ----------------------------------------------------------------------------
//
// Headless, fast mode, a definite end: every check prints "flaretest: PASS|FAIL <what>", the run ends on
// "flaretest: N passed, M failed". vr_flare_test 1, the steps (a state machine on the game's frames, each waiting on its
// own condition; a step's frame limit is only a safety net):
//   1 a loaded flare gun in the main hand (its counter 2, skin 0); the trigger held 0.4 s: one flare (the top barrel's);
//     let go and pulled again: the second (the bottom's); a third pull: a dry click; spent: clip 0, skin 2; a spent one
//     lying about is depleted (no loot beam, no force grab) and drawn with skin 2.
//   2 a grunt ahead, a flare into its chest: struck (vr_dmg_flaregun) and on fire.
//   3 a crate ahead: a flare sets it on fire; 4 an explosive box: a flare sets it off.
//   5 a flare into the nearest wall (vr_flare_stick_time 2 meanwhile): stuck (skin 1, still), out after 2 s.
//   6 a flare fired up (vr_flare_air_time 3, vr_flare_air_fall 20): drifts down from the top of its climb (skin 2),
//     sinking 20 units a second, out after 3 s.
//   7 an armed grunt from the pool with a flare gun first (vr_agrunt_flaregun 1): its two flares (skins 1, 2), then the
//     spent flare gun thrown away (depleted) and its own weapon in its hand.
// vr_flare_test 5: its muzzles ("flaremuzzle: N passed, M failed"). 7: the crates' flare guns, twice from the same plan
// ("flarecrates: ..."). 2-4, 6: test aids (in your hands, the holsters, the flares' scene).

float vr_flt_pass, vr_flt_fail;
float vr_flt_step;      // 0 idle
float vr_flt_frames;    // frames into the step
float vr_flt_t0;        // a step's moment (a flare stuck, drifting)
float vr_flt_z0;        // a drifting flare's height at the first measure
float vr_flt_barrel0, vr_flt_barrel1; // the barrels of the first two shots
float vr_flt_shots0;
float vr_flt_notarget, vr_flt_grip, vr_flt_stick, vr_flt_air, vr_flt_fall, vr_flt_agfg, vr_flt_dual; // the settings before
float vr_flt_mode;      // the test running (1 or 5)
entity vr_flt_player;
entity vr_flt_e;        // a step's target, a flare
vector vr_flt_from0, vr_flt_from1; // muzzles test: the two flares' starts
vector vr_flt_fwd0, vr_flt_up0;    // ... the shot line and the gun's up at the first

void(float ok, string what) VR_FLT_Check =
{
    if(ok) vr_flt_pass++; else vr_flt_fail++;
    dprint(sprintf("%s: %s %s\n", vr_flt_mode == 5 ? "flaremuzzle" : "flaretest", ok ? "PASS" : "FAIL", what));
};

void() VR_FLT_Next = { vr_flt_step++; vr_flt_frames = 0; };

// The main hand's trigger, as the controller's (+attack: the client's move sets button0 next frame).
float vr_flt_trigger;
void(float xOn) VR_FLT_Trigger =
{
    xOn = xOn ? 1 : 0;
    if(xOn != vr_flt_trigger)
    {
        localcmd(xOn ? "+attack\n" : "-attack\n");
        vr_flt_trigger = xOn;
    }
};

// Every flare gone (each step's own).
void() VR_FLT_ClearFlares =
{
    entity f = find(world, classname, "vr_flare");
    while(f)
    {
        entity nx = find(f, classname, "vr_flare");
        sound(f, CHAN_BODY, "misc/null.wav", 1, ATTN_IDLE);
        remove(f);
        f = nx;
    }
};

void() VR_FLT_End =
{
    VR_FLT_Trigger(0);
    VR_FLT_ClearFlares();
    VR_NPT_Freeze(FALSE);
    cvar_set("vr_weapon_grip_mode", ftos(vr_flt_grip));
    cvar_set("vr_flare_stick_time", ftos(vr_flt_stick));
    cvar_set("vr_flare_air_time", ftos(vr_flt_air));
    cvar_set("vr_flare_air_fall", ftos(vr_flt_fall));
    cvar_set("vr_agrunt_flaregun", ftos(vr_flt_agfg));
    cvar_set("vr_agrunt_dual_chance", ftos(vr_flt_dual));
    if(!vr_flt_notarget) vr_flt_player.flags = vr_flt_player.flags - (vr_flt_player.flags & FL_NOTARGET);
    dprint(sprintf("%s: %g passed, %g failed\n", vr_flt_mode == 5 ? "flaremuzzle" : "flaretest", vr_flt_pass, vr_flt_fail));
    vr_flt_step = 0;
};

// A point `xDist` ahead of the player on the floor (short of a wall: a real map's room may be small).
vector(float xDist) VR_FLT_Ahead =
{
    entity p = vr_flt_player;
    makevectors('0 1 0' * p.angles_y);
    traceline(p.origin, p.origin + v_forward * xDist, TRUE, p);
    vector at = trace_endpos - v_forward * (trace_fraction < 1 ? 32 : 0);
    traceline(at, at - '0 0 256', TRUE, p);
    return trace_endpos;
};

// A flare of the player's from 40 units before `xAt`, at it.
entity(vector xAt) VR_FLT_FlareAt =
{
    entity os = self;
    self = vr_flt_player;
    vector dir = normalize(xAt - vr_flt_player.origin);
    dir_z = 0;
    dir = normalize(dir);
    entity f = VR_Flare_Launch(xAt - dir * 40, dir, cvar_hget(cvarh_vr_flaregun_speed), '0 0 0');
    self = os;
    return f;
};

// The flare guns in the player's hands or holsters (2, 3, 4: test aids).
void(float req) VR_FLT_Give =
{
    entity p = vr_flt_player;
    entity os = self;
    self = p;
    if(req == 2 || req == 3)
    {
        VRGiveEntWeapon(self, cVR_MainHand, WID_FLAREGUN, 0, VR_FlareGun_Barrels());
        W_SetCurrentAmmoFor(cVR_MainHand);
        if(req == 3)
        {
            VRGiveEntWeapon(self, cVR_OffHand, WID_FLAREGUN, 0, VR_FlareGun_Barrels());
            W_SetCurrentAmmoFor(cVR_OffHand);
        }
        updateAmmoCounters();
        dprint(sprintf("flaretest: a flare gun in %s\n", req == 3 ? "both hands" : "the main hand"));
    }
    else
    {
        p.holsterweapon3 = WID_FLAREGUN;
        p.holsterweaponflags3 = 0;
        p.holsterweaponinst3 = WeaponInst_Carried(WID_FLAREGUN, VR_FlareGun_Barrels(), 0);
        p.holsterweaponmodel3 = WeaponIdToModel(WID_FLAREGUN, 0);
        p.holsterweapon5 = WID_FLAREGUN;
        p.holsterweaponflags5 = 0;
        p.holsterweaponinst5 = WeaponInst_Carried(WID_FLAREGUN, 1, 0); // (one fired: skin 1)
        p.holsterweaponmodel5 = WeaponIdToModel(WID_FLAREGUN, 0);
        dprint("flaretest: a flare gun in the right hip (loaded) and the right upper holster (one fired)\n");
    }
    self = os;
};

// The flares' scene (6): a flare stuck in the wall ahead, one lying on the floor nearer, one drifting down above, a
// loaded flare gun in your hand; their times long (the eyeshots).
void() VR_FLT_Scene =
{
    entity p = vr_flt_player;
    VR_FLT_Give(2);
    cvar_set("vr_flare_stick_time", "600");
    cvar_set("vr_flare_air_time", "600");
    cvar_set("vr_flare_air_fall", "1");
    entity os = self;
    self = p;
    makevectors('0 1 0' * p.angles_y);
    vector eye = p.origin + '0 0 22';
    traceline(eye, eye + v_forward * 600, TRUE, p);
    VR_Flare_Launch(trace_endpos - v_forward * 24, v_forward, 600, '0 0 0');                  // into the wall
    VR_Flare_Launch(p.origin + v_forward * 72 + '0 0 -10', '0 0 -1', 200, '0 0 0');              // onto the floor
    entity a = VR_Flare_Launch(p.origin + v_forward * 160 + v_right * 48 + '0 0 90', '0 0 1', 10, '0 0 0');
    VR_Flare_Drift(a);                                                                           // drifting above
    self = os;
    dprint("flaretest: the flares' scene: one into the wall ahead, one on the floor, one drifting\n");
};

// ----------------------------------------------------------------------------
// vr_flare_test 7: the crates' flare guns (the engine's plan), listed twice: now, and after the map is restarted (planned
// again: vr_place_keep 0 meanwhile); the two the same. The guns lying near a crate's top, each.
float vr_flc_frames;
float vr_flc_n, vr_flc_far, vr_flc_hash; // VR_FLC_List's: the guns, those not on a crate, their places' checksum

// Prints the crates' flare guns (their places to the unit) and sets vr_flc_*.
void(string xWhen) VR_FLC_List =
{
    vr_flc_n = vr_flc_far = vr_flc_hash = 0;
    entity e = find(world, classname, "thrown_weapon");
    while(e)
    {
        if(e.weapon == WID_FLAREGUN)
        {
            vr_flc_n++;
            vr_flc_hash = (vr_flc_hash * 7 + fabs(rint(e.origin_x)) + fabs(rint(e.origin_y)) * 3 + fabs(rint(e.origin_z)) * 5) % 999983;
            // (on a crate: one within 48 units across, below it)
            float near = FALSE;
            entity c = findradius(e.origin, 64);
            while(c)
            {
                vector d = c.origin - e.origin;
                d_z = 0;
                if(VR_Crate_Is(c) && c.origin_z < e.origin_z && vlen(d) < 48) { near = TRUE; }
                c = c.chain;
            }
            if(!near) { vr_flc_far++; }
            dprint(sprintf("flarecrates: %s: a flare gun at %v (on a crate: %g)\n", xWhen, e.origin, near));
        }
        e = find(e, classname, "thrown_weapon");
    }
    dprint(sprintf("flarecrates: %s: %g flare guns, %g not on a crate, checksum %g\n", xWhen, vr_flc_n, vr_flc_far, vr_flc_hash));
};

void() VR_FLC_Frame =
{
    vr_flc_frames++;
    if(vr_flc_frames != 3) // (the crates and their guns laid: a few frames in)
    {
        return;
    }
    float req = cvar("vr_flare_test");
    if(req == 7)
    {
        VR_FLC_List("first plan");
        cvar_set("temp1", ftos(vr_flc_hash));
        cvar_set("vr_flare_test", "8");
        localcmd("restart\n");
        return;
    }
    float first = cvar("temp1");
    VR_FLC_List("planned again");
    float ok = first == vr_flc_hash && vr_flc_n > 0 && vr_flc_far == 0;
    dprint(sprintf("flarecrates: %s: %s (%g, %g), %g on crates, %g not\n", ok ? "PASS" : "FAIL",
        first == vr_flc_hash ? "the same plan" : "a different plan", first, vr_flc_hash, vr_flc_n, vr_flc_far));
    cvar_set("vr_flare_test", "0");
};

// ----------------------------------------------------------------------------
// vr_flare_test 5: its muzzles. The main hand still (the mock's pose), its spread none (a flare has none): the first
// flare leaves the weapon's muzzle (the top barrel's exit), the second 2.3 model units under it (the bottom barrel's, at
// the drawn Scale) along the gun's up, both along the shot line.
void() VR_FLM_Frame =
{
    entity p = vr_flt_player;
    float s = vr_flt_step;
    if(s == 1 && vr_flt_frames == 1)
    {
        localcmd("vr_mock_hand main 0.15 1.2 -0.45 70 0 0\n");
    }
    else if(s == 1 && vr_flt_frames >= 12 && time >= readytime)
    {
        VRGiveEntWeapon(p, cVR_MainHand, WID_FLAREGUN, 0, VR_FlareGun_Barrels());
        W_SetCurrentAmmoFor(cVR_MainHand);
        VRSetAttackFinished(cVR_MainHand, 0);
        vr_fg_shots = 0;
        VR_FLT_Next();
    }
    else if(s == 2)
    {
        VR_FLT_Trigger((vr_flt_frames >= 2 && vr_flt_frames <= 6) || (vr_flt_frames >= 40 && vr_flt_frames <= 44));
        if(vr_fg_shots == 1 && vr_flt_barrel0 < 0)
        {
            vr_flt_barrel0 = vr_fg_lastbarrel;
            vr_flt_from0 = vr_fg_lastfrom;
            makevectors(VRGetWeaponFireRot(cVR_MainHand));
            vr_flt_fwd0 = v_forward;
            vr_flt_up0 = v_up;
            vector m = VRGetMuzzlePos(cVR_MainHand);
            VR_FLT_Check(vr_flt_barrel0 == 0 && vlen(vr_flt_from0 - m) < 0.05,
                sprintf("the first flare from the top barrel's muzzle: barrel %g, %.3f units from the muzzle", vr_flt_barrel0,
                    vlen(vr_flt_from0 - m)));
            float off = VR_DegreesBetween(vr_fg_lastflare.velocity, vr_flt_fwd0);
            VR_FLT_Check(off < 0.5, sprintf("the first flare along the shot line: %.3f degrees off", off));
        }
        if(vr_fg_shots == 2 && vr_flt_barrel1 < 0)
        {
            vr_flt_barrel1 = vr_fg_lastbarrel;
            vr_flt_from1 = vr_fg_lastfrom;
            vector d = vr_flt_from0 - vr_flt_from1;
            float want = VR_FLAREGUN_BARREL_GAP * cvar("vr_wofs_scale_38");
            float along = d * vr_flt_up0;
            float across = vlen(d - vr_flt_up0 * along);
            VR_FLT_Check(vr_flt_barrel1 == 1 && fabs(along - want) < 0.05 && across < 0.05,
                sprintf("the second from the bottom barrel's: barrel %g, %.3f units under the first along the gun's up "
                    "(want %.3f), %.3f off it", vr_flt_barrel1, along, want, across));
            float off1 = VR_DegreesBetween(vr_fg_lastflare.velocity, VRGetWeaponFireRotFwd(cVR_MainHand));
            VR_FLT_Check(off1 < 0.5, sprintf("the second along the shot line: %.3f degrees off", off1));
        }
        if(vr_flt_frames == 60)
        {
            VR_FLT_Check(vr_fg_shots == 2, sprintf("two flares fired (%g)", vr_fg_shots));
            VR_FLT_End();
        }
    }
};

// ----------------------------------------------------------------------------
// vr_flare_test 1: the self-test's steps.
void() VR_FLT_Frame =
{
    entity p = vr_flt_player;
    float s = vr_flt_step;
    if(s == 1 && time < readytime)
    {
        vr_flt_frames = 0; // (the player not ready yet after spawning: its guns don't fire)
    }
    else if(s == 1 && vr_flt_frames == 1)
    {
        localcmd("vr_mock_hand main 0.15 1.2 -0.45 70 0 0\n"); // (the main hand out ahead, aiming level)
    }
    else if(s == 1 && vr_flt_frames >= 12)
    {
        p.flags = p.flags | FL_GODMODE;
        VRGiveEntWeapon(p, cVR_MainHand, WID_FLAREGUN, 0, VR_FlareGun_Barrels());
        W_SetCurrentAmmoFor(cVR_MainHand);
        updateAmmoCounters();
        VRSetAttackFinished(cVR_MainHand, 0);
        vr_fg_shots = vr_fg_dry = 0;
        VR_FLT_Check(VRGetWeaponClip(cVR_MainHand) == 2 && p.ammocounter == 2 && VR_FlareGun_Skin(2) == 0,
            sprintf("loaded: clip %g, its counter %g, skin %g", VRGetWeaponClip(cVR_MainHand), p.ammocounter,
                VR_FlareGun_Skin(VRGetWeaponClip(cVR_MainHand))));
        VR_FLT_Next();
    }
    else if(s == 2)
    {
        // Held 30 frames (0.4 s, over the cooldown): one; let go, pulled again: the second; again: a dry click.
        VR_FLT_Trigger((vr_flt_frames >= 2 && vr_flt_frames <= 32) || (vr_flt_frames >= 40 && vr_flt_frames <= 44) ||
            (vr_flt_frames >= 80 && vr_flt_frames <= 84));
        if(vr_fg_shots == 1 && vr_flt_barrel0 < 0) { vr_flt_barrel0 = vr_fg_lastbarrel; }
        if(vr_fg_shots == 2 && vr_flt_barrel1 < 0) { vr_flt_barrel1 = vr_fg_lastbarrel; }
        if(vr_flt_frames == 34)
        {
            VR_FLT_Check(vr_fg_shots == 1 && vr_flt_barrel0 == 0 && VR_FlareGun_Skin(VRGetWeaponClip(cVR_MainHand)) == 1,
                sprintf("the trigger held 0.4 s: %g flare (want 1), from barrel %g (the top: 0), skin %g (want 1)", vr_fg_shots,
                    vr_flt_barrel0, VR_FlareGun_Skin(VRGetWeaponClip(cVR_MainHand))));
        }
        if(vr_flt_frames == 110)
        {
            VR_FLT_Check(vr_fg_shots == 2 && vr_flt_barrel1 == 1 && vr_fg_dry == 1,
                sprintf("pulled again: %g flares (want 2), the second from barrel %g (the bottom: 1); a third pull: %g dry "
                    "clicks (want 1)", vr_fg_shots, vr_flt_barrel1, vr_fg_dry));
            VR_FLT_Check(VRGetWeaponClip(cVR_MainHand) == 0 && p.ammocounter == 0 &&
                VR_FlareGun_Skin(VRGetWeaponClip(cVR_MainHand)) == 2, sprintf("spent: clip %g, counter %g, skin %g (want 2)",
                    VRGetWeaponClip(cVR_MainHand), p.ammocounter, VR_FlareGun_Skin(VRGetWeaponClip(cVR_MainHand))));
            entity w = VR_FlareGun_Spawn(world, p.origin + '0 0 48', '0 0 0', '0 0 0', 0);
            VR_FLT_Check(w && w.vr_depleted && w.skin == 2 && VR_EnemyGun_DepletedSkipped(w),
                sprintf("a spent one lying about: depleted %g (no loot beam, no force grab: %g), skin %g", w.vr_depleted,
                    VR_EnemyGun_DepletedSkipped(w), w.skin));
            entity w1 = VR_FlareGun_Spawn(world, p.origin + '0 0 48', '0 0 0', '0 0 0', 1);
            VR_FLT_Check(w1 && !w1.vr_depleted && w1.skin == 1, sprintf("one with a flare left: depleted %g, skin %g",
                w1.vr_depleted, w1.skin));
            if(w) { remove(w); }
            if(w1) { remove(w1); }
            VR_FLT_ClearFlares();
            vr_flt_e = VR_NPT_Grunt(140, -1, 0); // (a grunt, its fists: standing still)
            vr_flt_t0 = 0;
            vr_fg_hits = vr_fg_ignited = 0;
            VR_FLT_Next();
        }
    }
    else if(s == 3)
    {
        // A flare into the grunt's chest.
        entity g = vr_flt_e;
        if(vr_flt_t0 == 0 && g.takedamage) // (its own start done: it takes damage)
        {
            VR_FLT_FlareAt(g.origin + '0 0 8');
            dprint(sprintf("flaretest: a flare at the grunt at %v (you at %v), %g frames in\n", g.origin, p.origin, vr_flt_frames));
            vr_flt_t0 = time;
        }
        if(vr_fg_hits >= 1 || vr_flt_frames == 90)
        {
            VR_FLT_Check(vr_fg_hits == 1 && g.burn_fire != world && g.health < 30 && vr_fg_ignited == 1,
                sprintf("a grunt struck (%g hits): health %g of 30, on fire %g", vr_fg_hits, g.health, g.burn_fire != world));
            if(g.burn_fire) { remove(g.burn_fire); g.burn_fire = world; }
            VR_AGT_Remove(g);
            VR_FLT_ClearFlares();
            vector at3 = VR_FLT_Ahead(140);
            entity os3 = self;
            self = spawn();
            setorigin(self, at3 + '0 0 40');
            vr_crate();
            vr_flt_e = self;
            self = os3;
            vr_fg_hits = vr_fg_ignited = 0;
            VR_FLT_Next();
        }
    }
    else if(s == 4)
    {
        // A crate: on fire.
        entity c = vr_flt_e;
        if(vr_flt_frames == 3)
        {
            VR_FLT_FlareAt(c.origin + (c.mins + c.maxs) * 0.5);
        }
        if(vr_fg_hits >= 1 || vr_flt_frames == 72)
        {
            VR_FLT_Check(vr_fg_hits == 1 && c.burn_fire != world, sprintf("a crate struck (%g hits): on fire %g", vr_fg_hits,
                c.burn_fire != world));
            if(c.burn_fire) { remove(c.burn_fire); c.burn_fire = world; }
            remove(c);
            VR_FLT_ClearFlares();
            vector at4 = VR_FLT_Ahead(160);
            entity os4 = self;
            self = spawn();
            setorigin(self, at4 + '0 0 8');
            misc_explobox();
            vr_flt_e = self;
            self = os4;
            vr_fg_hits = vr_fg_booms = 0;
            VR_FLT_Next();
        }
    }
    else if(s == 5)
    {
        // An explosive box: set off.
        entity b = vr_flt_e;
        if(vr_flt_frames == 3)
        {
            VR_FLT_FlareAt(b.origin + (b.mins + b.maxs) * 0.5);
        }
        if(vr_fg_hits >= 1 || vr_flt_frames == 72)
        {
            VR_FLT_Check(vr_fg_hits == 1 && vr_fg_booms == 1 && (b.classname == "explo_box" || b.health <= 0),
                sprintf("an explosive box struck (%g hits): set off %g (%s, health %g)", vr_fg_hits, vr_fg_booms, b.classname,
                    b.health));
            VR_FLT_ClearFlares();
            cvar_set("vr_flare_stick_time", "2");
            // The nearest wall of four ways round, at the eyes' height.
            float best = 100000;
            vector to = '0 0 0';
            vector dir4 = '0 0 0';
            float k;
            for(k = 0; k < 4; k++)
            {
                makevectors('0 1 0' * (p.angles_y + k * 90));
                traceline(p.origin + '0 0 20', p.origin + '0 0 20' + v_forward * 2000, TRUE, p);
                if(trace_fraction < 1 && trace_fraction * 2000 < best && trace_fraction * 2000 > 48)
                {
                    best = trace_fraction * 2000;
                    to = trace_endpos;
                    dir4 = v_forward;
                }
            }
            entity os5 = self;
            self = p;
            vr_flt_e = VR_Flare_Launch(to - dir4 * 40, dir4, cvar_hget(cvarh_vr_flaregun_speed), '0 0 0');
            self = os5;
            vr_flt_t0 = 0;
            dprint(sprintf("flaretest: a flare at the wall %.0f units away\n", best));
            VR_FLT_Next();
        }
    }
    else if(s == 6)
    {
        // Stuck: still, its light a stuck one's, out after vr_flare_stick_time (2 s).
        entity f = vr_flt_e;
        if(vr_flt_t0 == 0 && f && f.classname == "vr_flare" && f.vr_flare_state == VR_FLARE_STUCK)
        {
            vr_flt_t0 = time;
            VR_FLT_Check(f.skin == 1 && f.movetype == MOVETYPE_NONE && f.velocity == '0 0 0',
                sprintf("stuck in the wall: skin %g, still %g", f.skin, f.movetype == MOVETYPE_NONE));
        }
        if(vr_flt_t0 > 0 && vr_fg_out != f && time - vr_flt_t0 > 1.0 && time - vr_flt_t0 < 1.02)
        {
            VR_FLT_Check(f.alpha < 1 && f.alpha > 0.5, sprintf("burning out over its last 1.5 s: alpha %.2f at 1.0 s", f.alpha));
        }
        if(vr_flt_t0 > 0 && (vr_fg_out == f || f.vr_flare_state != VR_FLARE_STUCK))
        {
            float lasted = vr_fg_out_at - vr_flt_t0;
            VR_FLT_Check(fabs(lasted - 2) < 0.08, sprintf("out after %.2f s stuck (vr_flare_stick_time 2)", lasted));
            // Up into the air: from the player's place, steeply, slowly (its top under any ceiling: 100 units).
            traceline(p.origin, p.origin + '0 0 400', TRUE, p);
            float room = vlen(trace_endpos - p.origin);
            float sp = sqrt(2 * cvar_hget(cvarh_vr_flaregun_gravity) * cvar("sv_gravity") * min(100, room * 0.4));
            cvar_set("vr_flare_air_time", "3");
            cvar_set("vr_flare_air_fall", "20");
            entity os6 = self;
            self = p;
            makevectors('-89 0 0' + '0 1 0' * p.angles_y);
            vr_flt_e = VR_Flare_Launch(p.origin + '0 0 16', v_forward, sp / v_forward_z, '0 0 0');
            self = os6;
            vr_flt_t0 = 0;
            dprint(sprintf("flaretest: a flare up at %.0f u/s (headroom %.0f)\n", sp / v_forward_z, room));
            VR_FLT_Next();
        }
        else if(vr_flt_frames == 72 * 6)
        {
            VR_FLT_Check(FALSE, "the wall's flare: never stuck or never out");
            VR_FLT_End();
            return;
        }
    }
    else if(s == 7)
    {
        // Drifting down from the top of its climb: skin 2, sinking at vr_flare_air_fall, out after vr_flare_air_time.
        entity a = vr_flt_e;
        if(vr_flt_t0 == 0 && a.classname == "vr_flare" && a.vr_flare_state == VR_FLARE_AIR)
        {
            vr_flt_t0 = time;
            vr_flt_z0 = a.origin_z;
            VR_FLT_Check(a.skin == 2 && a.movetype == MOVETYPE_FLY, sprintf("at the top of its climb, drifting: skin %g", a.skin));
        }
        if(vr_flt_t0 > 0 && vr_fg_out != a && time - vr_flt_t0 >= 1 && time - vr_flt_t0 < 1.012)
        {
            float sank = vr_flt_z0 - a.origin_z;
            VR_FLT_Check(fabs(sank - 20) < 2, sprintf("sinking: %.1f units in its first second (want 20)", sank));
        }
        if(vr_flt_t0 > 0 && (vr_fg_out == a || a.vr_flare_state != VR_FLARE_AIR))
        {
            float lasted7 = vr_fg_out_at - vr_flt_t0;
            VR_FLT_Check(fabs(lasted7 - 3) < 0.08 && vr_fg_out == a,
                sprintf("out after %.2f s drifting (vr_flare_air_time 3; landed first: %g)", lasted7, vr_fg_out != a));
            VR_FLT_ClearFlares();
            // An armed grunt from the pool (vr_agrunt_flaregun 1: a flare gun first, unless its fists), and one with a
            // shotgun given its flare gun as the pool's would be (its weapon known: the checks the same every run;
            // temp1 another weapon id).
            cvar_set("vr_agrunt_flaregun", "1");
            cvar_set("vr_agrunt_dual_chance", "0");
            entity pooled = VR_NPT_Grunt(200, 0, 0);
            VR_FLT_Check(pooled.vr_ag_prop ? (pooled.vr_ag_prop.weapon == WID_FLAREGUN && pooled.vr_ag_main_wid > 0) :
                pooled.vr_ag_main_wid == 0, sprintf("one from the pool: %s first, its %s stowed",
                pooled.vr_ag_prop ? pooled.vr_ag_prop.netname : "fists", WeaponIdToWeaponName(pooled.vr_ag_main_wid)));
            VR_AGT_Remove(pooled);
            vr_flt_e = VR_NPT_Grunt(300, cvar("temp1") > 0 ? cvar("temp1") : WID_SHOTGUN, 0);
            VR_AG_MaybeFlareGun(vr_flt_e, 0);
            VR_FLT_Next();
        }
        else if(vr_flt_frames == 72 * 8)
        {
            VR_FLT_Check(FALSE, sprintf("the air flare: never drifted or never out (state %g)", a.vr_flare_state));
            VR_FLT_End();
            return;
        }
    }
    else if(s == 8 && vr_flt_frames == 2)
    {
        // Its flare gun's two flares (each an attack's shot), its skins; then thrown away, its own weapon taken.
        entity ag = vr_flt_e;
        float main = ag.vr_ag_main_wid;
        VR_FLT_Check(ag.vr_ag_prop && ag.vr_ag_prop.weapon == WID_FLAREGUN && main > 0 && ag.vr_ag_prop.skin == 0,
            sprintf("an armed grunt with a flare gun first (%s), its %s stowed", ag.vr_ag_prop ? ag.vr_ag_prop.netname : "none",
                main > 0 ? WeaponIdToWeaponName(main) : "nothing"));
        entity os8 = self;
        self = ag;
        ag.enemy = p;
        float n0 = 0, n1 = 0, n2 = 0, sk1 = -1, sk2 = -1;
        entity fl = find(world, classname, "vr_flare");
        while(fl) { if(fl.owner == ag) n0++; fl = find(fl, classname, "vr_flare"); }
        if(ag.vr_ag_prop) { VR_AG_ShootProp(ag.vr_ag_prop); sk1 = ag.vr_ag_prop.skin; }
        fl = find(world, classname, "vr_flare");
        while(fl) { if(fl.owner == ag) n1++; fl = find(fl, classname, "vr_flare"); }
        if(ag.vr_ag_prop) { VR_AG_ShootProp(ag.vr_ag_prop); sk2 = ag.vr_ag_prop.skin; }
        fl = find(world, classname, "vr_flare");
        while(fl) { if(fl.owner == ag) n2++; fl = find(fl, classname, "vr_flare"); }
        float draws = vr_ag_flare_draws;
        VR_AG_AfterAttack();
        self = os8;
        VR_FLT_Check(n1 - n0 == 1 && n2 - n1 == 1 && sk1 == 1 && sk2 == 2,
            sprintf("its two flares: %g then %g fired, skins %g then %g", n1 - n0, n2 - n1, sk1, sk2));
        entity thrown = world;
        entity tw = find(world, classname, "thrown_weapon");
        while(tw) { if(tw.weapon == WID_FLAREGUN && vlen(tw.origin - ag.origin) < 128) thrown = tw; tw = find(tw, classname, "thrown_weapon"); }
        VR_FLT_Check(thrown != world && thrown.vr_depleted && thrown.skin == 2,
            sprintf("the spent flare gun thrown away: %g (depleted %g, skin %g)", thrown != world, thrown.vr_depleted, thrown.skin));
        VR_FLT_Check(ag.vr_ag_prop && ag.vr_ag_prop.weapon == main && ag.vr_ag_main_wid == 0 && vr_ag_flare_draws == draws + 1,
            sprintf("then its own: %s in its hand", ag.vr_ag_prop ? ag.vr_ag_prop.netname : "none"));
        if(thrown) { remove(thrown); }
        VR_AGT_Remove(ag);
        VR_FLT_End();
        return;
    }
    if(vr_flt_frames > 72 * 12) // (a safety net only: a stuck step)
    {
        VR_FLT_Check(FALSE, sprintf("step %g timed out", vr_flt_step));
        VR_FLT_End();
    }
};

void() VR_FlareGun_TestFrame =
{
    float req = cvar("vr_flare_test");
    if(req == 7 || req == 8)
    {
        VR_FLC_Frame();
        return;
    }
    vr_flc_frames = 0;
    if(req)
    {
        cvar_set("vr_flare_test", "0");
        vr_flt_player = find(world, classname, "player");
        vr_npt_player = vr_flt_player; // (the nail pistol test's helpers: VR_NPT_Grunt)
        if(!vr_flt_player) { dprint("flaretest: no player\n"); return; }
        if(req >= 2 && req <= 4)
        {
            VR_FLT_Give(req);
            return;
        }
        if(req == 6 || req == 9)
        {
            if(req == 9)
            {
                lightstyle(0, "b"); // (9: the map's lights all but out: the flares light a dark room)
            }
            VR_FLT_Scene();
            return;
        }
        vr_flt_pass = vr_flt_fail = 0;
        vr_flt_mode = req;
        vr_flt_grip = cvar("vr_weapon_grip_mode");
        vr_flt_stick = cvar("vr_flare_stick_time");
        vr_flt_air = cvar("vr_flare_air_time");
        vr_flt_fall = cvar("vr_flare_air_fall");
        vr_flt_agfg = cvar("vr_agrunt_flaregun");
        vr_flt_dual = cvar("vr_agrunt_dual_chance");
        cvar_set("vr_weapon_grip_mode", "1");
        vr_flt_notarget = (vr_flt_player.flags & FL_NOTARGET) != 0;
        vr_flt_player.flags = vr_flt_player.flags | FL_NOTARGET;
        VR_NPT_Freeze(TRUE);
        vr_flt_barrel0 = vr_flt_barrel1 = -1;
        vr_flt_step = 1;
        vr_flt_frames = 0;
    }
    if(!vr_flt_step) return;
    entity os = self;
    self = vr_flt_player;
    vr_flt_frames++;
    if(vr_flt_mode == 5) { VR_FLM_Frame(); } else { VR_FLT_Frame(); }
    self = os;
};
#endif
