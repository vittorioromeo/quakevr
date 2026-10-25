#ifdef QVR_TESTS
//
//
//
// ----------------------------------------------------------------------------
// The nail pistol's test (vr_np_test 1; Debug > Tests > Nail Pistol; vr_nailpistol.qc)
// ----------------------------------------------------------------------------
//
// Headless, fast mode, a definite end: every check prints "nptest: PASS|FAIL <what>", the run ends on
// "nptest: N passed, M failed". The steps (a state machine on the game's frames, each waiting on its own condition):
//   1 a nail pistol in the main hand, a full magazine (immersive reloading on it: vr_reload_mode_npistol 3), a target ahead
//     on its aim; the trigger held 0.55 s: one shot (semi-automatic); its slide back after it, home after.
//   2 pulls every other frame: the rest of the magazine, 12 in all, each at least vr_nailpistol_cooldown after the last;
//     12 hits on the target; a 13th pull clicks dry; its slide locked back, empty.
//   3 the magazine cycle (as the hands do it: B/Y's eject, the other hand at the pouch, the magazine's top at the well the
//     engine reports): out empty, a new one of 12 from your nails, seated; the slide home; a shot from it.
//   4 its damage: a grunt killed by nails at its head (each x1.5), another by nails in its body (x1): how many each.
//   5 armed grunts: one with a nail pistol fires it; one with a pair fires both.

float vr_npt_pass, vr_npt_fail;
float vr_npt_step;      // 0 idle
float vr_npt_frames;    // frames into the step
float vr_npt_last;      // the time of the last shot seen
float vr_npt_seen;      // shots seen so far
float vr_npt_mingap;    // the shortest gap between two shots
float vr_npt_slide1;    // the slide's frame just after the first shot (1 back)
float vr_npt_n;
.float vr_npt_frozen;   // a map's monster held still for the test (its think put off)
float vr_npt_notarget;  // the player's FL_NOTARGET before
float vr_npt_mode;      // vr_reload_mode_npistol before
float vr_npt_grip;      // vr_weapon_grip_mode before (1 meanwhile: the mock's hands keep what they hold)
entity vr_npt_player;
entity vr_npt_g;

void(float ok, string what) VR_NPT_Check =
{
    if(ok) vr_npt_pass++; else vr_npt_fail++;
    dprint(sprintf("nptest: %s %s\n", ok ? "PASS" : "FAIL", what));
};

// The map's own monsters stand still meanwhile (one walking into the grunts' line of fire holds their shots).
void(float xOn) VR_NPT_Freeze =
{
    entity e = nextent(world);
    while(e)
    {
        if(xOn)
        {
            if((e.flags & FL_MONSTER) && e.health > 0 && e.nextthink > 0)
            {
                e.vr_npt_frozen = TRUE;
                e.nextthink = 0;
            }
        }
        else if(e.vr_npt_frozen)
        {
            e.vr_npt_frozen = FALSE;
            e.nextthink = time + 0.1;
        }
        e = nextent(e);
    }
};

void() VR_NPT_End =
{
    VR_NPT_Freeze(FALSE);
    if(vr_np_target) { remove(vr_np_target); vr_np_target = world; }
    cvar_set("vr_reload_mode_npistol", ftos(vr_npt_mode));
    cvar_set("vr_weapon_grip_mode", ftos(vr_npt_grip));
    VR_NPT_Trigger(0);
    if(!vr_npt_notarget) vr_npt_player.flags = vr_npt_player.flags - (vr_npt_player.flags & FL_NOTARGET);
    dprint(sprintf("nptest: %g passed, %g failed\n", vr_npt_pass, vr_npt_fail));
    vr_npt_step = 0;
};

void() VR_NPT_Next = { vr_npt_step++; vr_npt_frames = 0; };

// The main hand's trigger, as the controller's (+attack: the client's move sets button0 next frame).
float vr_npt_trigger;
void(float xOn) VR_NPT_Trigger =
{
    xOn = xOn ? 1 : 0;
    if(xOn != vr_npt_trigger)
    {
        localcmd(xOn ? "+attack\n" : "-attack\n");
        vr_npt_trigger = xOn;
    }
};

// A target (a box that takes damage) `xDist` ahead along the main hand's aim, from its muzzle.
void(float xDist) VR_NPT_Target =
{
    entity os = self;
    self = vr_npt_player;
    makevectors(VRGetWeaponFireRot(cVR_MainHand));
    vector at = VRGetMuzzlePos(cVR_MainHand) + v_forward * xDist;
    self = os;
    entity t = spawn();
    t.classname = "np_target";
    t.solid = SOLID_BBOX;
    t.movetype = MOVETYPE_NONE;
    t.takedamage = DAMAGE_YES;
    t.health = 100000;
    setsize(t, '-24 -24 -24', '24 24 24');
    setorigin(t, at);
    vr_np_target = t;
    vr_np_hits = 0;
};

// A grunt (unarmed: its fists) `xDist` ahead of the player, facing it.
entity(float xDist, float xWeapon, float xWeapon2) VR_NPT_Grunt =
{
    entity p = vr_npt_player;
    makevectors('0 1 0' * p.angles_y);
    // (stopping short of a wall or another monster, which would hold its fire: a real map's room may be smaller than the vrtesthall's)
    traceline(p.origin, p.origin + v_forward * xDist, FALSE, p);
    vector at = trace_endpos - v_forward * (trace_fraction < 1 ? 24 : 0);
    entity os = self;
    self = spawn();
    setorigin(self, at);
    self.angles_y = vectoyaw(p.origin - at);
    self.weapon = xWeapon;
    self.weapon2 = xWeapon2;
    droptofloor(); // (a ledge in a real map: it lands first, not after the nails)
    VR_AM_Spawn(0);
    self.movetype = MOVETYPE_NONE; // (it stands where the nails are aimed)
    entity g = self;
    self = os;
    return g;
};

// A nail of the player's into `g` at its height `xUp` above its origin, from 24 units in front of it (a step or ledge in a real map would stop a longer flight).
void(entity g, float xUp) VR_NPT_NailAt =
{
    entity os = self;
    self = vr_npt_player;
    vector to = g.origin + '0 0 1' * xUp;
    vector from = to + normalize(vr_npt_player.origin - g.origin) * 24;
    from_z = to_z;
    vector dir = normalize(to - from);
    entity n = launch_spike(from, dir);
    n.touch = VR_NailPistol_SpikeTouch;
    n.velocity = dir * cvar_hget(cvarh_vr_nailpistol_speed);
    self = os;
};

float(entity g) VR_NPT_Spikes =
{
    float c = 0;
    entity e = find(world, classname, "spike");
    while(e)
    {
        if(e.owner == g && e.touch == VR_NailPistol_SpikeTouch) { c++; }
        e = find(e, classname, "spike");
    }
    return c;
};

void() VR_NailPistol_TestFrame =
{
    float req = cvar("vr_np_test");
    if(req)
    {
        cvar_set("vr_np_test", "0");
        vr_npt_player = find(world, classname, "player");
        if(!vr_npt_player) { dprint("nptest: no player\n"); return; }
        if(req == 2 || req == 3)
        {
            // A nail pistol, loaded, in the main hand (3: in both): Debug > Tests > Nail Pistol, the eyeshots.
            entity os2 = self;
            self = vr_npt_player;
            VR_WeaponUtil_EntAddWeapon(self, WID_NAILPISTOL);
            VRGiveEntWeapon(self, cVR_MainHand, WID_NAILPISTOL, 0, VR_NailPistol_MagSize());
            W_SetCurrentAmmoFor(cVR_MainHand);
            if(req == 3)
            {
                VRGiveEntWeapon(self, cVR_OffHand, WID_NAILPISTOL, 0, VR_NailPistol_MagSize());
                W_SetCurrentAmmoFor(cVR_OffHand);
            }
            self.ammo_nails = max(self.ammo_nails, 48);
            dprint(sprintf("nptest: a nail pistol in %s\n", req == 3 ? "both hands" : "the main hand"));
            self = os2;
            return;
        }
        if(req == 4)
        {
            // A loaded nail pistol in the right hip and the right upper holster (whatever was there goes): Debug > Tests >
            // Nail Pistol, the holstered eyeshots.
            entity p4 = vr_npt_player;
            VR_WeaponUtil_EntAddWeapon(p4, WID_NAILPISTOL);
            p4.holsterweapon3 = WID_NAILPISTOL;
            p4.holsterweaponflags3 = 0;
            p4.holsterweaponinst3 = WeaponInst_Carried(WID_NAILPISTOL, VR_NailPistol_MagSize(), 0);
            p4.holsterweaponmodel3 = WeaponIdToModel(WID_NAILPISTOL, 0);
            p4.holsterweapon5 = WID_NAILPISTOL;
            p4.holsterweaponflags5 = 0;
            p4.holsterweaponinst5 = WeaponInst_Carried(WID_NAILPISTOL, VR_NailPistol_MagSize(), 0);
            p4.holsterweaponmodel5 = WeaponIdToModel(WID_NAILPISTOL, 0);
            dprint("nptest: a nail pistol in the right hip and the right upper holster\n");
            return;
        }
        vr_npt_pass = vr_npt_fail = 0;
        vr_npt_mode = cvar("vr_reload_mode_npistol");
        vr_npt_grip = cvar("vr_weapon_grip_mode");
        cvar_set("vr_weapon_grip_mode", "1");
        // (a real map's monsters must not wake, hunt or shoot meanwhile: the checks need the spawned grunts still)
        vr_npt_notarget = (vr_npt_player.flags & FL_NOTARGET) != 0;
        vr_npt_player.flags = vr_npt_player.flags | FL_NOTARGET;
        VR_NPT_Freeze(TRUE);
        vr_npt_step = 1;
        vr_npt_frames = 0;
    }
    if(!vr_npt_step) return;
    entity p = vr_npt_player;
    entity os = self;
    self = p;
    vr_npt_frames++;
    if(vr_np_shots > vr_npt_seen)
    {
        if(vr_npt_seen > 0) { vr_npt_mingap = min(vr_npt_mingap, time - vr_npt_last); }
        vr_npt_seen = vr_np_shots;
        vr_npt_last = time;
    }
    if(vr_npt_frames > 72 * 20) // (a safety net only: a stuck step)
    {
        VR_NPT_Check(FALSE, sprintf("step %g timed out", vr_npt_step));
        self = os;
        VR_NPT_End();
        return;
    }
    float s = vr_npt_step;
    if(s == 1 && time < readytime)
    {
        vr_npt_frames = 0; // (the player not ready yet after spawning: its guns don't fire)
    }
    else if(s == 1 && vr_npt_frames == 1)
    {
        localcmd("vr_mock_hand main 0.15 1.2 -0.45 70 0 0\n"); // (the main hand out ahead, aiming level: autopump_test.sh's)
    }
    else if(s == 1 && vr_npt_frames < 12)
    {
        // (the hand's pose taken)
    }
    else if(s == 1)
    {
        cvar_set("vr_reload_mode_npistol", "3");
        p.flags = p.flags | FL_GODMODE;
        p.ammo_nails = 100;
        VR_WeaponUtil_EntAddWeapon(p, WID_NAILPISTOL);
        VRGiveEntWeapon(p, cVR_MainHand, WID_NAILPISTOL, 0, VR_NailPistol_MagSize());
        W_SetCurrentAmmoFor(cVR_MainHand);
        VRSetAttackFinished(cVR_MainHand, 0);
        VR_NPT_Trigger(0);
        vr_np_shots = vr_np_dry = 0;
        vr_npt_seen = 0;
        vr_npt_mingap = 1000;
        vr_npt_slide1 = -1;
        VR_NPT_Target(80);
        dprint(sprintf("nptest: in hand: %s, clip %g, magazine in %g, immersive %g, nails %g\n",
            WeaponIdToWeaponName(VRGetWeapon(cVR_MainHand)), VRGetWeaponClip(cVR_MainHand), VR_Reload_MagIn(p, cVR_MainHand),
            VR_Reload_ImmersiveFor(WID_NAILPISTOL), p.ammo_nails));
        VR_NPT_Next();
    }
    else if(s == 2)
    {
        // The trigger held 40 frames (0.55 s, over four cooldowns).
        VR_NPT_Trigger(vr_npt_frames <= 40);
        if(vr_np_shots == 1 && vr_npt_slide1 < 0 && time > vr_npt_last)
        {
            vr_npt_slide1 = VRGetWeaponFrame(cVR_MainHand);
        }
        if(vr_npt_frames == 50)
        {
            VR_NPT_Check(vr_np_shots == 1, sprintf("semi-automatic: the trigger held 0.55 s fired %g (want 1)", vr_np_shots));
            VR_NPT_Check(vr_npt_slide1 == 1 && VRGetWeaponFrame(cVR_MainHand) == 0,
                sprintf("the slide back after the shot (frame %g) and home after (frame %g)", vr_npt_slide1,
                    VRGetWeaponFrame(cVR_MainHand)));
            VR_NPT_Next();
        }
    }
    else if(s == 3)
    {
        // Pulls every other frame until the magazine's 12 are out.
        VR_NPT_Trigger((((vr_npt_frames & 1) != 0) && (vr_np_shots < VR_NailPistol_MagSize())));
        if(vr_np_shots >= VR_NailPistol_MagSize() && !p.button0 && !vr_npt_trigger)
        {
            VR_NPT_Check(vr_np_shots == 12 && vr_np_dry == 0, sprintf("the magazine: %g shots (want 12), %g dry", vr_np_shots,
                vr_np_dry));
            VR_NPT_Check(vr_npt_mingap >= cvar_hget(cvarh_vr_nailpistol_cooldown) - 0.001,
                sprintf("the cooldown: the shortest gap %.3f s (vr_nailpistol_cooldown %g), pulls every %.3f s",
                    vr_npt_mingap, cvar_hget(cvarh_vr_nailpistol_cooldown), 2 * frametime));
            VR_NPT_Next();
        }
    }
    else if(s == 4)
    {
        // The 13th pull, a little after the cooldown.
        VR_NPT_Trigger(((vr_npt_frames >= 12) && (vr_npt_frames <= 14)));
        if(vr_npt_frames == 30)
        {
            VR_NPT_Check(vr_np_shots == 12 && vr_np_dry == 1, sprintf("the 13th pull: %g shots, %g dry clicks (want 12, 1)",
                vr_np_shots, vr_np_dry));
            VR_NPT_Check(vr_np_hits == 12, sprintf("hits on the target: %g (want 12)", vr_np_hits));
            VR_NPT_Check(VRGetWeaponClip(cVR_MainHand) == 0 && VRGetWeaponFrame(cVR_MainHand) == 1,
                sprintf("empty: clip %g, its slide locked back (frame %g)", VRGetWeaponClip(cVR_MainHand),
                    VRGetWeaponFrame(cVR_MainHand)));
            VR_NPT_Next();
        }
    }
    else if(s == 5)
    {
        // The magazine cycle: out (B/Y), a new one from the pouch into the off hand, its top to the engine's well.
        entity m = VR_Reload_Eject(p, cVR_MainHand, '0 0 0', -1, "nptest");
        VR_NPT_Check(m != world && m.vr_ammo_count == 0 && m.model == "progs/vr_mag_npistol.mdl" && !VR_Reload_MagIn(p,
            cVR_MainHand), sprintf("ejected: an empty magazine (%s, %g) out, none in", m.model, m.vr_ammo_count));
        if(m) { remove(m); }
        if(VRGetEntWeapon(p, cVR_OffHand) != WID_FIST) { VRGiveEntWeapon(p, cVR_OffHand, WID_FIST, 0, 0); }
        float before = p.ammo_nails;
        VR_Reload_Take(cVR_OffHand);
        entity r = VR_Carry_Held(p, cVR_OffHand);
        VR_NPT_Check(r != world && r.vr_ammo_wid == WID_NAILPISTOL && r.vr_ammo_count == 12 && p.ammo_nails == before - 12,
            sprintf("from the pouch: a magazine of %g (want 12), nails %g -> %g", r.vr_ammo_count, before, p.ammo_nails));
        float d = vlen(p.loadportpos - VRGetEntHandPos(p, cVR_MainHand));
        VR_NPT_Check(p.loadportpos != '0 0 0' && d < 16, sprintf("the engine's well on the grip: %.1f units from the hand", d));
        if(r) { VR_Reload_MagAtWell(cVR_OffHand, r); }
        VR_NPT_Check(VR_Reload_MagIn(p, cVR_MainHand) && VRGetWeaponClip(cVR_MainHand) == 12 &&
            VR_Carry_Held(p, cVR_OffHand) == world, sprintf("slapped in: clip %g, the hand empty", VRGetWeaponClip(cVR_MainHand)));
        VR_NPT_Next();
    }
    else if(s == 6)
    {
        VR_NPT_Trigger(vr_npt_frames == 3);
        if(vr_npt_frames == 2)
        {
            VR_NPT_Check(VRGetWeaponFrame(cVR_MainHand) == 0, sprintf("the slide home, loaded (frame %g)",
                VRGetWeaponFrame(cVR_MainHand)));
        }
        if(vr_npt_frames == 20)
        {
            VR_NPT_Check(vr_np_shots == 13 && VRGetWeaponClip(cVR_MainHand) == 11,
                sprintf("a shot from the new magazine: %g shots, clip %g", vr_np_shots, VRGetWeaponClip(cVR_MainHand)));
            remove(vr_np_target);
            vr_np_target = world;
            // (that last nail is still flying down the aim: in a long corridor it would reach the grunts and add to their damage)
            entity sp = find(world, classname, "spike");
            while(sp)
            {
                entity nx = find(sp, classname, "spike");
                if(sp.owner == p) remove(sp);
                sp = nx;
            }
            vr_npt_g = VR_NPT_Grunt(256, -1, 0);
            vr_npt_n = 0;
            VR_NPT_Next();
        }
    }
    else if(s == 7 || s == 8)
    {
        // Nails every 6 frames: at its head (step 7), then another grunt's body (8).
        entity g = vr_npt_g;
        float alive = (g.health > 0 && g.deadflag == DEAD_NO);
        if(vr_npt_frames == 1)
        {
            dprint(sprintf("nptest: %s, health %g\n", g.classname, g.health));
        }
        if(alive && vr_npt_n < 8 && (vr_npt_frames % 6) == 2)
        {
            vr_np_hits = 0;
            VR_NPT_NailAt(g, s == 7 ? 24 : 6);
            vr_npt_n++;
        }
        if(!alive || (vr_npt_n >= 8 && (vr_npt_frames % 6) == 5))
        {
            if(s == 7)
            {
                VR_NPT_Check(!alive && vr_npt_n == 2 && vr_np_lastfactor > 1.4,
                    sprintf("headshots: the grunt died to %g (want 2), the last x%.2f", vr_npt_n, vr_np_lastfactor));
                vr_npt_g = VR_NPT_Grunt(360, -1, 0);
            }
            else
            {
                VR_NPT_Check(!alive && (vr_npt_n == 3 || vr_npt_n == 4) && fabs(vr_np_lastfactor - 1) < 0.01,
                    sprintf("body shots: the grunt died to %g (want 3-4), the last x%.2f", vr_npt_n, vr_np_lastfactor));
                vr_npt_g = VR_NPT_Grunt(400, WID_NAILPISTOL, 0);
            }
            vr_npt_n = 0;
            VR_NPT_Next();
        }
    }
    else if(s == 9 || s == 10)
    {
        // An armed grunt with a nail pistol (9), with a pair (10): each gun fired as its attack fires it.
        entity a = vr_npt_g;
        if(vr_npt_frames == 2)
        {
            self = a;
            a.enemy = p;
            float n0 = VR_NPT_Spikes(a);
            if(a.vr_ag_prop) { a.vr_ag_prop.weaponinst.wi_clip = 12; VR_AG_ShootProp(a.vr_ag_prop); }
            float n1 = VR_NPT_Spikes(a);
            if(a.vr_ag_prop2) { a.vr_ag_prop2.weaponinst.wi_clip = 12; VR_AG_ShootProp(a.vr_ag_prop2); }
            float n2 = VR_NPT_Spikes(a);
            self = p;
            if(s == 9)
            {
                VR_NPT_Check(a.vr_ag_prop && a.vr_ag_prop.weapon == WID_NAILPISTOL && n1 - n0 == 1,
                    sprintf("an armed grunt's nail pistol (%s): %g nails fired", a.vr_ag_prop ? a.vr_ag_prop.netname : "none",
                        n1 - n0));
                VR_AGT_Remove(a);
                vr_npt_g = VR_NPT_Grunt(400, WID_NAILPISTOL, WID_NAILPISTOL);
                VR_NPT_Next();
            }
            else
            {
                VR_NPT_Check(a.vr_ag_prop && a.vr_ag_prop2 && a.vr_ag_prop2.weapon == WID_NAILPISTOL && n1 - n0 == 1 &&
                    n2 - n1 == 1, sprintf("a pair of nail pistols: held %g, %g nails right, %g left", a.vr_ag_prop2 != world,
                        n1 - n0, n2 - n1));
                VR_AGT_RemovePair(a);
                self = os;
                VR_NPT_End();
                return;
            }
        }
    }
    self = os;
};
#endif
