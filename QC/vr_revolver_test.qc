#ifdef QVR_TESTS
//
//
//
// ----------------------------------------------------------------------------
// The heavy revolver's test (vr_rv_test 1; Debug > Tests > Heavy Revolver; vr_revolver.qc)
// ----------------------------------------------------------------------------
//
// Headless, fast mode, a definite end: every check prints "rvtest: PASS|FAIL <what>", the run ends on
// "rvtest: N passed, M failed". The steps (a state machine on the game's frames, each waiting on its own condition, with
// a frame count only as a safety net):
//   1 a revolver in the main hand, six live (immersive reloading on it: vr_reload_mode_revolver 3), a target ahead.
//   2 the trigger held 0.7 s: one shot (double action); the hammer cocked (frame 1), then down with the trigger held (2),
//     at rest let go (0); the cylinder turned a chamber: the one fired under the hammer, spent.
//   3 pulls every other frame: the other five, each at least vr_revolver_cooldown after the last (and at most 3 frames
//     more), all on the target; six spent.
//   4 a seventh pull: a click, the cylinder turned all the same.
//   5 B/Y (the mock's): it swings out. 6 B/Y again: its six spent shells drop (casings), none live; empty.
//   7 three shells from the pouch, one at a time (singles, though the pouch tapes pairs), each held at a chamber's place on
//     its back face (the engine's load point): into chambers 3, 5 and 0.
//   8 a fourth by the hands (the off hand's shell at the load point: vr_mock_hand_to off lport): into the next empty one
//     to come under the hammer.
//   9 a spin (900 degrees a second): it slows, stops on a chamber; the chamber under the hammer moved with its turn.
//  10 a flick of the wrist the other way (the mock's roll with vr_mock_turn_velocity 1): it swings home.
//  11 pulls until two shots: each pull a shot or a click as its chambers say, from the one after the hammer's on.
//  12 a flick towards its cylinder's side: it swings out. 13 the muzzle tilted up: its two spent and two live shells drop
//     (two live rounds lying about).
//  14 the flicks' thresholds: the force (vr_revolver_*_flick_speed) and the wrist flick (_angle), both ways.
//  15 its damage: a grunt to one slug at its head (x1.5, its head popped), another to two in its body, an ogre to 4-5 in its
//     body (vr_revolver_heavy_mult); the head pop's chance by range against the shotguns'.
//  16 an armed grunt with a revolver fires it.

float vr_rvt_pass, vr_rvt_fail;
float vr_rvt_step;     // 0 idle
float vr_rvt_frames;   // frames into the step
float vr_rvt_seen;     // shots seen so far
float vr_rvt_last;     // the time of the last one
float vr_rvt_mingap, vr_rvt_maxgap;
float vr_rvt_hits;     // shots that hit the target
float vr_rvt_saw1, vr_rvt_saw2; // the hammer's frames seen while the trigger was held
float vr_rvt_n, vr_rvt_k;
float vr_rvt_at0;      // the chamber under the hammer before a spin
float vr_rvt_turned;   // and the turn since, degrees
float vr_rvt_prevcyl;
float vr_rvt_shots0, vr_rvt_dry0, vr_rvt_live0;
string vr_rvt_want, vr_rvt_got; // the pulls' outcomes ("F" a shot, "c" a click): expected, seen
float vr_rvt_mode, vr_rvt_grip, vr_rvt_tv, vr_rvt_notarget;
entity vr_rvt_player;
entity vr_rvt_target;
entity vr_rvt_g;

void(float ok, string what) VR_RVT_Check =
{
    if(ok) vr_rvt_pass++; else vr_rvt_fail++;
    dprint(sprintf("rvtest: %s %s\n", ok ? "PASS" : "FAIL", what));
};

entity() VR_RVT_Rec = { return VRGetEntWeaponInst(vr_rvt_player, cVR_MainHand); };

void() VR_RVT_End =
{
    VR_NPT_Freeze(FALSE);
    if(vr_rvt_target) { remove(vr_rvt_target); vr_rvt_target = world; }
    cvar_set("vr_reload_mode_revolver", ftos(vr_rvt_mode));
    cvar_set("vr_weapon_grip_mode", ftos(vr_rvt_grip));
    cvar_set("vr_mock_turn_velocity", ftos(vr_rvt_tv));
    cvar_set("vr_decap_pop_roll", "-1");
    VR_NPT_Trigger(0);
    localcmd("vr_mock_button main secondary 0\n");
    if(!vr_rvt_notarget) vr_rvt_player.flags = vr_rvt_player.flags - (vr_rvt_player.flags & FL_NOTARGET);
    dprint(sprintf("rvtest: %g passed, %g failed\n", vr_rvt_pass, vr_rvt_fail));
    vr_rvt_step = 0;
};

void() VR_RVT_Next = { vr_rvt_step++; vr_rvt_frames = 0; };

// A target (a box that takes damage) `xDist` ahead along the main hand's aim, from its muzzle.
void(float xDist) VR_RVT_Target =
{
    entity os = self;
    self = vr_rvt_player;
    makevectors(VRGetWeaponFireRot(cVR_MainHand));
    vector at = VRGetMuzzlePos(cVR_MainHand) + v_forward * xDist;
    self = os;
    entity t = spawn();
    t.classname = "rv_target";
    t.solid = SOLID_BBOX;
    t.movetype = MOVETYPE_NONE;
    t.takedamage = DAMAGE_YES;
    t.health = 100000;
    setsize(t, '-24 -24 -24', '24 24 24');
    setorigin(t, at);
    vr_rvt_target = t;
};

// A slug of the player's into `g` at its height `xUp` above its origin (-1: its head zone's middle, vr_decap.qc), from 48
// units in front of it.
void(entity g, float xUp) VR_RVT_SlugAt =
{
    entity os = self;
    self = vr_rvt_player;
    vector to = g.origin + '0 0 1' * xUp;
    if(xUp < 0 && VR_Decap_HeadZone(g))
    {
        makevectors('0 1 0' * g.angles_y);
        to = g.origin + v_forward * xHeadForward + VR_HeadSide(v_forward) + '0 0 1' * xHeadUp;
    }
    vector from = to + normalize(vr_rvt_player.origin - g.origin) * 48;
    from_z = to_z;
    VR_Revolver_SlugFrom(from, normalize(to - from), 128);
    self = os;
};

// The point at chamber `c`'s place on the swung-out cylinder's back face in the main hand (as VR_Revolver_ChamberFor
// reads it), 0.4 units back from it.
vector(float c) VR_RVT_ChamberPoint =
{
    entity os = self;
    self = vr_rvt_player;
    VR_Revolver_Frame(cVR_MainHand);
    entity r = VR_RVT_Rec();
    float a = (90 + 60 * c + VR_Revolver_CylAngle(r) + RV_SWING) * 0.0174533;
    vector at = vr_rv_port + (vr_rv_left * cos(a) + vr_rv_up * sin(a)) * (1.45 * RV_SCALE) - vr_rv_axis * 0.4;
    self = os;
    return at;
};

// A grunt (an ogre: `xOgre`) `xDist` ahead of the player turned `xYaw` degrees off its facing (each its own way: the
// last one's body never in the next one's way), facing it, on its feet (Killed skips a MOVETYPE_NONE thing's Quake VR
// dying, its head pop with it); shot once it has started (walkmonster_start_go: it takes damage).
entity(float xDist, float xYaw, float xOgre) VR_RVT_Monster =
{
    entity p = vr_rvt_player;
    makevectors('0 1 0' * (p.angles_y + xYaw));
    traceline(p.origin, p.origin + v_forward * xDist, FALSE, p);
    vector at = trace_endpos - v_forward * (trace_fraction < 1 ? 40 : 0);
    entity os = self;
    self = spawn();
    self.classname = xOgre ? "monster_ogre" : "monster_army";
    setorigin(self, at);
    self.angles_y = vectoyaw(p.origin - at);
    if(xOgre) { monster_ogre(); } else { monster_army(); }
    droptofloor();
    entity m = self;
    self = os;
    return m;
};

// What `xPulls` pulls do from now (the chambers as they are): "F" a shot, "c" a click, until `xShots` shots.
string(entity r, float xShots) VR_RVT_Expect =
{
    string s = "";
    float n = 0;
    for(float k = 1; k <= 12 && n < xShots; k++)
    {
        if(VR_Revolver_Chamber(r, r.wi_rv_at + k) == 1)
        {
            s = strcat(s, "F");
            n++;
        }
        else
        {
            s = strcat(s, "c");
        }
    }
    return s;
};

float() VR_RVT_LiveRounds =
{
    float n = 0;
    entity e = find(world, classname, VR_RELOAD_CLASS);
    while(e)
    {
        if(e.vr_ammo_aid == AID_SHELLS && !e.carry_player) { n++; }
        e = find(e, classname, VR_RELOAD_CLASS);
    }
    return n;
};

void() VR_RVT_Flicks =
{
    entity h = spawn();
    float dt = 1 / 72;
    float r1 = 0;
    float k;
    // The force and the wrist flick towards the cylinder's side: 400 deg/s for 0.1 s (40 deg) swings it out.
    for(k = 0; k < 8 && !r1; k++) { r1 = VR_Revolver_FlickRate(h, 400, dt); }
    VR_RVT_Check(r1 == 1, sprintf("a flick of 400 deg/s: out after %g frames (%.0f deg)", k, k * 400 * dt));
    // Too weak (300 < 350 deg/s), however far.
    h.rvh_fo = h.rvh_fop = h.rvh_fc = h.rvh_fcp = 0;
    float r2 = 0;
    for(k = 0; k < 20 && !r2; k++) { r2 = VR_Revolver_FlickRate(h, 300, dt); }
    VR_RVT_Check(r2 == 0, sprintf("a roll of 300 deg/s for 20 frames (83 deg): nothing (%g)", r2));
    // Strong but short (900 deg/s for 2 frames: 25 < 30 deg), then still.
    h.rvh_fo = h.rvh_fop = h.rvh_fc = h.rvh_fcp = 0;
    float r3 = VR_Revolver_FlickRate(h, 900, dt) + VR_Revolver_FlickRate(h, 900, dt) + VR_Revolver_FlickRate(h, 0, dt);
    VR_RVT_Check(r3 == 0, sprintf("a jerk of 900 deg/s for 25 deg: nothing (%g)", r3));
    // Home: -350 deg/s for 0.1 s (35 deg) swings it home; -250 (< 300) never.
    h.rvh_fo = h.rvh_fop = h.rvh_fc = h.rvh_fcp = 0;
    float r4 = 0;
    for(k = 0; k < 8 && !r4; k++) { r4 = VR_Revolver_FlickRate(h, -350, dt); }
    h.rvh_fo = h.rvh_fop = h.rvh_fc = h.rvh_fcp = 0;
    float r5 = 0;
    for(k = 0; k < 20 && !r5; k++) { r5 = VR_Revolver_FlickRate(h, -250, dt); }
    VR_RVT_Check(r4 == -1 && r5 == 0, sprintf("the other way: -350 deg/s home (%g), -250 deg/s nothing (%g)", r4, r5));
    // Customized: a lighter force (200) takes 300 deg/s; a longer wrist flick (60 deg) needs more of it.
    float s0 = cvar("vr_revolver_open_flick_speed");
    float a0 = cvar("vr_revolver_open_flick_angle");
    cvar_set("vr_revolver_open_flick_speed", "200");
    cvar_set("vr_revolver_open_flick_angle", "60");
    h.rvh_fo = h.rvh_fop = h.rvh_fc = h.rvh_fcp = 0;
    float r6 = 0;
    for(k = 0; k < 30 && !r6; k++) { r6 = VR_Revolver_FlickRate(h, 300, dt); }
    VR_RVT_Check(r6 == 1 && k >= 14, sprintf("customized (200 deg/s, 60 deg): 300 deg/s out after %g frames (%.0f deg)", k,
        k * 300 * dt));
    cvar_set("vr_revolver_open_flick_speed", ftos(s0));
    cvar_set("vr_revolver_open_flick_angle", ftos(a0));
    // The time window: a slow roll (50 deg/s, 30 deg takes 0.6 s) never opens within 0.25 s, does within 1 s. Also how long
    // today's default flick (30 deg at 350 deg/s) takes.
    float w0 = cvar("vr_revolver_open_flick_time");
    cvar_set("vr_revolver_open_flick_speed", "50");
    cvar_set("vr_revolver_open_flick_angle", "30");
    cvar_set("vr_revolver_open_flick_time", "0.25");
    h.rvh_fo = h.rvh_fop = h.rvh_fot = h.rvh_fc = h.rvh_fcp = h.rvh_fct = 0;
    float r7 = 0;
    for(k = 0; k < 60 && !r7; k++) { r7 = VR_Revolver_FlickRate(h, 50, dt); }
    cvar_set("vr_revolver_open_flick_time", "1");
    h.rvh_fo = h.rvh_fop = h.rvh_fot = h.rvh_fc = h.rvh_fcp = h.rvh_fct = 0;
    float r8 = 0;
    for(k = 0; k < 60 && !r8; k++) { r8 = VR_Revolver_FlickRate(h, 50, dt); }
    VR_RVT_Check(r7 == 0 && r8 == 1, sprintf("a slow 30 deg roll over 0.6 s: window 0.25 s nothing (%g), window 1 s out after %g frames", r7, k));
    cvar_set("vr_revolver_open_flick_speed", ftos(s0));
    cvar_set("vr_revolver_open_flick_angle", ftos(a0));
    cvar_set("vr_revolver_open_flick_time", ftos(w0));
    h.rvh_fo = h.rvh_fop = h.rvh_fot = h.rvh_fc = h.rvh_fcp = h.rvh_fct = 0;
    float r9 = 0;
    for(k = 0; k < 60 && !r9; k++) { r9 = VR_Revolver_FlickRate(h, 350, dt); }
    VR_RVT_Check(r9 == 1, sprintf("default flick (30 deg at 350 deg/s): out after %g frames = %.3f s (window %g s)", k, k * dt, w0));
    remove(h);
};

void() VR_Revolver_TestFrame =
{
    float req = cvar("vr_rv_test");
    if(req)
    {
        cvar_set("vr_rv_test", "0");
        vr_rvt_player = find(world, classname, "player");
        if(!vr_rvt_player) { dprint("rvtest: no player\n"); return; }
        vr_npt_player = vr_rvt_player; // (the nail pistol's test's helpers: its grunts, its trigger)
        entity os0 = self;
        self = vr_rvt_player;
        if(req >= 2 && req <= 4)
        {
            // Debug > Tests > Heavy Revolver (the eyeshots): 2 a loaded one in the main hand, 3 in both, 4 one in the main
            // hand swung out with live and spent shells (immersive reloading on it).
            VR_WeaponUtil_EntAddWeapon(self, WID_REVOLVER);
            VRGiveEntWeapon(self, cVR_MainHand, WID_REVOLVER, 0, RV_CHAMBERS);
            W_SetCurrentAmmoFor(cVR_MainHand);
            if(req == 3)
            {
                VRGiveEntWeapon(self, cVR_OffHand, WID_REVOLVER, 0, RV_CHAMBERS);
                W_SetCurrentAmmoFor(cVR_OffHand);
            }
            self.ammo_shells = max(self.ammo_shells, 36);
            if(req == 4)
            {
                cvar_set("vr_reload_mode_revolver", "3");
                entity r4 = VRGetEntWeaponInst(self, cVR_MainHand);
                r4.wi_rv = 1 + 4 * 2 + 16 * 1 + 64 * 2 + 256 * 0 + 1024 * 1; // (live, spent, live, spent, empty, live)
                VRSetEntWeaponClip(self, cVR_MainHand, 3);
                r4.wi_rv_open = 1;
            }
            dprint(sprintf("rvtest: a revolver in %s\n", req == 3 ? "both hands" : (req == 4 ? "the main hand, swung out" :
                "the main hand")));
            self = os0;
            return;
        }
        if(req == 5)
        {
            // An armed grunt with a revolver, 200 units ahead (Debug > Tests > Heavy Revolver).
            self = os0;
            vr_rvt_g = VR_NPT_Grunt(200, WID_REVOLVER, 0);
            dprint(sprintf("rvtest: an armed grunt with %s\n", vr_rvt_g.vr_ag_prop ? vr_rvt_g.vr_ag_prop.netname : "nothing"));
            return;
        }
        self = os0;
        vr_rvt_pass = vr_rvt_fail = 0;
        vr_rvt_mode = cvar("vr_reload_mode_revolver");
        vr_rvt_grip = cvar("vr_weapon_grip_mode");
        vr_rvt_tv = cvar("vr_mock_turn_velocity");
        cvar_set("vr_weapon_grip_mode", "1");
        cvar_set("vr_mock_turn_velocity", "1");
        vr_rvt_notarget = (vr_rvt_player.flags & FL_NOTARGET) != 0;
        vr_rvt_player.flags = vr_rvt_player.flags | FL_NOTARGET;
        VR_NPT_Freeze(TRUE);
        vr_rvt_step = 1;
        vr_rvt_frames = 0;
    }
    if(!vr_rvt_step) return;
    entity p = vr_rvt_player;
    entity os = self;
    self = p;
    vr_rvt_frames++;
    entity rec = VR_RVT_Rec();
    if(vr_rv_shots > vr_rvt_seen)
    {
        if(vr_rvt_last > 0)
        {
            vr_rvt_mingap = min(vr_rvt_mingap, time - vr_rvt_last);
            vr_rvt_maxgap = max(vr_rvt_maxgap, time - vr_rvt_last);
        }
        vr_rvt_seen = vr_rv_shots;
        vr_rvt_last = time;
        if(vr_rvt_target && vr_rv_lasthit == vr_rvt_target) { vr_rvt_hits++; }
    }
    if(vr_rvt_frames > 72 * 20) // (a safety net only: a stuck step)
    {
        VR_RVT_Check(FALSE, sprintf("step %g timed out", vr_rvt_step));
        self = os;
        VR_RVT_End();
        return;
    }
    float s = vr_rvt_step;
    if(s == 1 && time < readytime)
    {
        vr_rvt_frames = 0; // (the player not ready yet after spawning: its guns don't fire)
    }
    else if(s == 1 && vr_rvt_frames == 1)
    {
        localcmd("vr_mock_hand main 0.15 1.2 -0.45 70 0 0\n"); // (the main hand out ahead, aiming level)
    }
    else if(s == 1 && vr_rvt_frames < 12)
    {
        // (the hand's pose taken)
    }
    else if(s == 1 && vr_rvt_frames == 12)
    {
        cvar_set("vr_reload_mode_revolver", "3");
        p.flags = p.flags | FL_GODMODE;
        p.ammo_shells = 100;
        VR_WeaponUtil_EntAddWeapon(p, WID_REVOLVER);
        VRGiveEntWeapon(p, cVR_MainHand, WID_REVOLVER, 0, RV_CHAMBERS);
        W_SetCurrentAmmoFor(cVR_MainHand);
        if(VRGetEntWeapon(p, cVR_OffHand) != WID_FIST) { VRGiveEntWeapon(p, cVR_OffHand, WID_FIST, 0, 0); }
        VRSetAttackFinished(cVR_MainHand, 0);
        VR_NPT_Trigger(0);
        vr_rv_shots = vr_rv_dry = vr_rv_ej_spent = vr_rv_ej_live = vr_rv_loaded = vr_rv_opened = vr_rv_closed = 0;
        vr_rvt_seen = 0;
        vr_rvt_hits = 0;
        vr_rvt_mingap = 1000;
        vr_rvt_maxgap = 0;
        VR_RVT_Target(80);
    }
    else if(s == 1 && vr_rvt_frames == 14)
    {
        VR_RVT_Check(VRGetWeapon(cVR_MainHand) == WID_REVOLVER && rec && VR_Revolver_Desc(rec) == "LLLLLL" &&
            VRGetWeaponClip(cVR_MainHand) == 6 && VR_Reload_ManualInHand(p, cVR_MainHand) && (p.vr_rv_hud & 131072),
            sprintf("in hand: %s, chambers %s, clip %g, loaded by hand %g, drawn %g", WeaponIdToWeaponName(VRGetWeapon(cVR_MainHand)),
                rec ? VR_Revolver_Desc(rec) : "?", VRGetWeaponClip(cVR_MainHand), VR_Reload_ManualInHand(p, cVR_MainHand),
                p.vr_rv_hud));
        vr_rvt_saw1 = vr_rvt_saw2 = 0;
        VR_RVT_Next();
    }
    else if(s == 2)
    {
        // The trigger held 50 frames (0.69 s, over two cooldowns).
        VR_NPT_Trigger(vr_rvt_frames <= 50);
        if(vr_rvt_frames <= 50 && VRGetWeaponFrame(cVR_MainHand) == 1) { vr_rvt_saw1 = TRUE; }
        if(vr_rvt_frames <= 50 && VRGetWeaponFrame(cVR_MainHand) == 2) { vr_rvt_saw2 = TRUE; }
        if(vr_rvt_frames == 60)
        {
            VR_RVT_Check(vr_rv_shots == 1, sprintf("double action: the trigger held 0.69 s fired %g (want 1)", vr_rv_shots));
            VR_RVT_Check(vr_rvt_saw1 && vr_rvt_saw2 && VRGetWeaponFrame(cVR_MainHand) == 0,
                sprintf("the hammer: cocked (frame 1 seen %g), down with the trigger held (2 seen %g), at rest let go (%g)",
                    vr_rvt_saw1, vr_rvt_saw2, VRGetWeaponFrame(cVR_MainHand)));
            vr_rvt_last = 0; // (the gaps: step 3's alone)
            vr_rvt_mingap = 1000;
            vr_rvt_maxgap = 0;
            VR_RVT_Check(rec.wi_rv_at == 1 && VR_Revolver_Desc(rec) == "SLLLLL" && VRGetWeaponClip(cVR_MainHand) == 5,
                sprintf("the cylinder turned a chamber: chamber %g under the hammer, chambers %s, clip %g", rec.wi_rv_at,
                    VR_Revolver_Desc(rec), VRGetWeaponClip(cVR_MainHand)));
            VR_RVT_Next();
        }
    }
    else if(s == 3)
    {
        // Pulls every other frame until its six are out.
        VR_NPT_Trigger(((vr_rvt_frames & 1) != 0) && vr_rv_shots < 6);
        if(vr_rv_shots >= 6 && !p.button0 && !vr_npt_trigger)
        {
            float cool = cvar_hget(cvarh_vr_revolver_cooldown);
            VR_RVT_Check(vr_rv_shots == 6 && vr_rv_dry == 0, sprintf("six shots (%g), %g clicks", vr_rv_shots, vr_rv_dry));
            VR_RVT_Check(vr_rvt_mingap >= cool - 0.001 && vr_rvt_maxgap <= cool + 3 * frametime + 0.001,
                sprintf("its rate: gaps %.3f .. %.3f s (vr_revolver_cooldown %g), pulls every %.3f s", vr_rvt_mingap,
                    vr_rvt_maxgap, cool, 2 * frametime));
            VR_RVT_Check(vr_rvt_hits == 6, sprintf("on the target: %g of 6 (from the muzzle, along the aim)", vr_rvt_hits));
            VR_RVT_Check(VR_Revolver_Desc(rec) == "SSSSSS" && VRGetWeaponClip(cVR_MainHand) == 0,
                sprintf("spent: %s, clip %g", VR_Revolver_Desc(rec), VRGetWeaponClip(cVR_MainHand)));
            vr_rvt_at0 = rec.wi_rv_at;
            VR_RVT_Next();
        }
    }
    else if(s == 4)
    {
        VR_NPT_Trigger(vr_rvt_frames >= 26 && vr_rvt_frames <= 28);
        if(vr_rvt_frames == 40)
        {
            VR_RVT_Check(vr_rv_shots == 6 && vr_rv_dry == 1 && rec.wi_rv_at == VR_RV_Wrap(vr_rvt_at0 + 1),
                sprintf("a seventh pull: a click (%g), no shot (%g), the cylinder turned (chamber %g, was %g)", vr_rv_dry,
                    vr_rv_shots, rec.wi_rv_at, vr_rvt_at0));
            remove(vr_rvt_target);
            vr_rvt_target = world;
            VR_RVT_Next();
        }
    }
    else if(s == 5)
    {
        if(vr_rvt_frames == 1) { localcmd("vr_mock_button main secondary 1\n"); }
        if(vr_rvt_frames == 4) { localcmd("vr_mock_button main secondary 0\n"); }
        if(rec.wi_rv_open || vr_rvt_frames == 30)
        {
            VR_RVT_Check(rec.wi_rv_open && vr_rv_opened == 1 && vr_rv_lasthow == "the eject button",
                sprintf("B/Y: swung out (%g) by %s", rec.wi_rv_open, vr_rv_lasthow));
            VR_RVT_Next();
        }
    }
    else if(s == 6)
    {
        if(vr_rvt_frames == 30) { localcmd("vr_mock_button main secondary 1\n"); }
        if(vr_rvt_frames == 33) { localcmd("vr_mock_button main secondary 0\n"); }
        if(vr_rv_ej_spent + vr_rv_ej_live > 0 || vr_rvt_frames == 60)
        {
            VR_RVT_Check(vr_rv_ej_spent == 6 && vr_rv_ej_live == 0 && VR_Revolver_Desc(rec) == "------" &&
                VRGetWeaponClip(cVR_MainHand) == 0 && (p.vr_rv_hud & 8192),
                sprintf("B/Y again: %g spent and %g live shells dropped (want 6, 0), chambers %s, the rod pushed %g",
                    vr_rv_ej_spent, vr_rv_ej_live, VR_Revolver_Desc(rec), (p.vr_rv_hud & 8192) != 0));
            VR_RVT_Next();
        }
    }
    else if(s == 7 && vr_rvt_frames == 2)
    {
        // Three shells from the pouch, each held at a chamber's place: 3, 5, 0.
        float before = p.ammo_shells;
        float okAll = TRUE;
        string got = "";
        for(float c = 0; c < 3; c++)
        {
            float want = c == 0 ? 3 : (c == 1 ? 5 : 0);
            VR_Reload_Take(cVR_OffHand);
            entity sh = VR_Carry_Held(p, cVR_OffHand);
            if(!sh || sh.vr_ammo_count != 1)
            {
                okAll = FALSE;
                got = strcat(got, sprintf(" [took %g]", sh ? sh.vr_ammo_count : -1));
            }
            if(sh)
            {
                setorigin(sh, VR_RVT_ChamberPoint(want));
                VR_Revolver_LoadInto(cVR_MainHand, sh, TRUE);
            }
            got = strcat(got, sprintf(" %g", vr_rv_lastch));
            if(vr_rv_lastch != want) { okAll = FALSE; }
        }
        VR_RVT_Check(okAll && vr_rv_loaded == 3 && VRGetWeaponClip(cVR_MainHand) == 3 && p.ammo_shells == before - 3 &&
            VR_Carry_Held(p, cVR_OffHand) == world,
            sprintf("three single shells by chamber (want 3 5 0): into%s; clip %g, shells %g -> %g, chambers %s", got,
                VRGetWeaponClip(cVR_MainHand), before, p.ammo_shells, VR_Revolver_Desc(rec)));
        VR_RVT_Next();
    }
    else if(s == 8)
    {
        if(vr_rvt_frames == 1) { localcmd("vr_mock_button off grip 1\n"); } // (holding it)
        if(vr_rvt_frames == 6)
        {
            VR_Reload_Take(cVR_OffHand);
            vr_rvt_n = -1;
            for(vr_rvt_k = 1; vr_rvt_k <= 6 && vr_rvt_n < 0; vr_rvt_k++)
            {
                if(VR_Revolver_Chamber(rec, rec.wi_rv_at + vr_rvt_k) == 0) { vr_rvt_n = VR_RV_Wrap(rec.wi_rv_at + vr_rvt_k); }
            }
        }
        if(vr_rvt_frames == 8) { localcmd("vr_mock_hand_to off lport\n"); }
        if(vr_rvt_frames == 14 || vr_rvt_frames == 7)
        {
            entity hr = VR_Carry_Held(p, cVR_OffHand);
            dprint(sprintf("rvtest: frame %g: the off hand holds %s (%g), %.1f units from the load point, grabbing %g\n",
                vr_rvt_frames, hr ? hr.classname : "nothing", hr ? hr.vr_ammo_count : 0,
                hr ? vlen(VR_Reload_RoundRef(hr) - p.loadportpos) : -1, VR_HandGrabUtil_IsHandGrabbing(p, cVR_OffHand)));
        }
        if((vr_rv_loaded >= 4 && vr_rvt_frames > 8) || vr_rvt_frames == 70)
        {
            VR_RVT_Check(vr_rv_loaded == 4 && vr_rv_lastch == vr_rvt_n && VRGetWeaponClip(cVR_MainHand) == 4,
                sprintf("by the hands (the off hand's shell at the load point): into chamber %g (want %g, the next to come), clip %g",
                    vr_rv_lastch, vr_rvt_n, VRGetWeaponClip(cVR_MainHand)));
            localcmd("vr_mock_button off grip 0; vr_mock_hand off -0.2 1.0 -0.3 70 0 0\n");
            VR_RVT_Next();
        }
    }
    else if(s == 9)
    {
        if(vr_rvt_frames == 2)
        {
            vr_rvt_at0 = rec.wi_rv_at;
            vr_rvt_turned = 0;
            vr_rvt_k = VR_Revolver_CylAngle(rec); // (a swipe may have turned it already: from where it is)
            vr_rvt_prevcyl = VR_Revolver_CylAngle(rec);
            rec.wi_rv_spin = 900;
        }
        else if(vr_rvt_frames > 2)
        {
            float cyl = VR_Revolver_CylAngle(rec);
            vr_rvt_turned = vr_rvt_turned + VR_RV_AngleNorm(cyl - vr_rvt_prevcyl);
            vr_rvt_prevcyl = cyl;
            if((rec.wi_rv_spin == 0 && rec.wi_rv_ofs == 0) || vr_rvt_frames == 400)
            {
                float steps = rint((vr_rvt_turned - (vr_rvt_k + 60 * vr_rvt_at0)) / 60);
                VR_RVT_Check(rec.wi_rv_spin == 0 && rec.wi_rv_ofs == 0 && vr_rvt_turned > 300 &&
                    fabs(VR_RV_AngleNorm(VR_Revolver_CylAngle(rec) - (vr_rvt_k + vr_rvt_turned))) < 0.01 &&
                    rec.wi_rv_at == VR_RV_Wrap(vr_rvt_at0 - steps),
                    sprintf("spun: turned %.1f deg over %.2f s, stopped on a chamber; under the hammer %g (was %g), chambers %s",
                        vr_rvt_turned, (vr_rvt_frames - 2) * frametime, rec.wi_rv_at, vr_rvt_at0, VR_Revolver_Desc(rec)));
                VR_RVT_Next();
            }
        }
    }
    else if(s == 10)
    {
        if(vr_rvt_frames == 2) { vr_rv_rollmax = 0; }
        if(vr_rvt_frames == 26) { localcmd("vr_mock_hand_turn main 0 0 -150\n"); }
        if(!rec.wi_rv_open || vr_rvt_frames == 60)
        {
            VR_RVT_Check(!rec.wi_rv_open && vr_rv_closed == 1 && substring(vr_rv_lasthow, 0, 7) == "a flick",
                sprintf("a flick away from its cylinder's side (the mock's roll -150 in a frame: the controller pitched 70 off the aim, about a third of it the aim's roll): home (%g) by %s; the wrist's roll %.0f deg/s",
                    !rec.wi_rv_open, vr_rv_lasthow, vr_rv_rollmax));
            vr_rvt_want = VR_RVT_Expect(rec, 2);
            vr_rvt_got = "";
            vr_rvt_shots0 = vr_rv_shots;
            vr_rvt_dry0 = vr_rv_dry;
            vr_rvt_n = 0;
            VR_RVT_Next();
        }
    }
    else if(s == 11)
    {
        // A pull every 26 frames until two shots.
        float ph = vr_rvt_frames % 26;
        VR_NPT_Trigger(ph >= 1 && ph <= 2 && vr_rv_shots - vr_rvt_shots0 < 2);
        if(ph == 12 && vr_rvt_n < 12)
        {
            if(vr_rv_shots - vr_rvt_shots0 + vr_rv_dry - vr_rvt_dry0 > strlen(vr_rvt_got))
            {
                vr_rvt_got = strcat(vr_rvt_got, vr_rv_shots - vr_rvt_shots0 > vr_rvt_n ? "F" : "c");
                vr_rvt_n = vr_rv_shots - vr_rvt_shots0;
            }
        }
        if(vr_rv_shots - vr_rvt_shots0 >= 2 && ph == 20)
        {
            VR_RVT_Check(vr_rvt_got == vr_rvt_want, sprintf("chamber tracking: the pulls %s (want %s, from its chambers after the spin)",
                vr_rvt_got, vr_rvt_want));
            vr_rvt_live0 = VR_RVT_LiveRounds();
            VR_RVT_Next();
        }
    }
    else if(s == 12)
    {
        if(vr_rvt_frames == 2) { vr_rv_rollmax = 0; }
        if(vr_rvt_frames == 26) { localcmd("vr_mock_hand_turn main 0 0 150\n"); }
        if(rec.wi_rv_open || vr_rvt_frames == 60)
        {
            VR_RVT_Check(rec.wi_rv_open && substring(vr_rv_lasthow, 0, 7) == "a flick",
                sprintf("a flick towards its cylinder's side (the mock's roll +150 in a frame): out (%g) by %s; the wrist's roll %.0f deg/s",
                    rec.wi_rv_open, vr_rv_lasthow, vr_rv_rollmax));
            vr_rvt_shots0 = vr_rv_ej_spent;
            vr_rvt_dry0 = vr_rv_ej_live;
            VR_RVT_Next();
        }
    }
    else if(s == 13)
    {
        if(vr_rvt_frames == 26) { localcmd("vr_mock_hand main 0.15 1.3 -0.3 130 0 0\n"); } // (the muzzle 60 degrees up)
        if(vr_rv_ej_spent + vr_rv_ej_live > vr_rvt_shots0 + vr_rvt_dry0 || vr_rvt_frames == 80)
        {
            makevectors(VRGetEntHandRot(p, cVR_MainHand));
            float sp = vr_rv_ej_spent - vr_rvt_shots0;
            float lv = vr_rv_ej_live - vr_rvt_dry0;
            VR_RVT_Check(sp == 2 && lv == 2 && VR_Revolver_Desc(rec) == "------" && VR_RVT_LiveRounds() == vr_rvt_live0 + 2,
                sprintf("the muzzle tilted up (aim z %.2f): %g spent and %g live shells dropped (want 2, 2), %g live rounds lying (+%g), chambers %s",
                    v_forward_z, sp, lv, VR_RVT_LiveRounds(), VR_RVT_LiveRounds() - vr_rvt_live0, VR_Revolver_Desc(rec)));
            localcmd("vr_mock_hand main 0.15 1.2 -0.45 70 0 0\n");
            VR_RVT_Next();
        }
    }
    else if(s == 14 && vr_rvt_frames == 2)
    {
        vr_rvt_saw1 = 0;
        VR_RVT_Flicks();
        vr_rvt_g = VR_RVT_Monster(256, 0, FALSE);
        vr_rvt_n = 0;
        VR_RVT_Next();
    }
    else if(s == 15 || s == 16 || s == 17)
    {
        // Slugs every 6 frames: a grunt's head (15), another grunt's body (16), an ogre's body (17).
        entity g = vr_rvt_g;
        float alive = (g.health > 0 && g.deadflag == DEAD_NO && !g.vr_decap_died); // (a body's health is its corpse's)
        if(vr_rvt_frames == 1)
        {
            dprint(sprintf("rvtest: %s, health %g\n", g.classname, g.health));
            vr_rvt_k = vr_decap_pops;
        }
        if(alive && g.takedamage && vr_rvt_n < 8 && (vr_rvt_frames % 6) == 2)
        {
            g.enemy = p; // (aware of you: no sneak attack's x vr_stealth_sneak, vr_stealth.qc)
            g.th_pain = SUB_Null; // (it stands as it is: no flinch bowing its head into a body shot's way)
            g.nextthink = 0;
            VR_RVT_SlugAt(g, s == 15 ? -1 : (s == 16 ? 6 : 16)); // (the ogre's chest)
            vr_rvt_n++;
            dprint(sprintf("rvtest: slug %g into %s: %.1f dealt (part %g), its health %g, dead %g\n", vr_rvt_n, g.classname,
                vr_rv_lastdmg, vr_rv_lastpart, g.health, g.deadflag));
        }
        if(!alive && !vr_rvt_saw1)
        {
            vr_rvt_saw1 = vr_rvt_frames; // (dead: its head's pop shown as its ragdoll is made, a few frames on)
        }
        if((!alive && vr_rvt_frames >= vr_rvt_saw1 + 30) || (vr_rvt_n >= 8 && (vr_rvt_frames % 6) == 5))
        {
            vr_rvt_saw1 = 0;
            if(s == 15)
            {
                VR_RVT_Check(!alive && vr_rvt_n == 1 && vr_rv_lastpart == 1 && (vr_decap_pops > vr_rvt_k || g.vr_headless),
                    sprintf("a grunt's head: dead to %g slug (want 1), %.1f dealt (part %g), its head popped %g (headless %g)",
                        vr_rvt_n, vr_rv_lastdmg, vr_rv_lastpart, vr_decap_pops - vr_rvt_k, g.vr_headless));
                vr_rvt_g = VR_RVT_Monster(256, 35, FALSE);
            }
            else if(s == 16)
            {
                VR_RVT_Check(!alive && vr_rvt_n == 2 && vr_rv_lastpart == 0,
                    sprintf("a grunt's body: dead to %g slugs (want 2), the last %.1f", vr_rvt_n, vr_rv_lastdmg));
                vr_rvt_g = VR_RVT_Monster(300, -35, TRUE); // (a big body: vr_revolver_heavy_mult)
            }
            else
            {
                VR_RVT_Check(!alive && (vr_rvt_n == 4 || vr_rvt_n == 5) && vr_rv_lastpart != 1,
                    sprintf("an ogre's body (%g full health): dead to %g slugs (want 4-5), the last %.1f (vr_revolver_heavy_mult %g)",
                        g.max_health, vr_rvt_n, vr_rv_lastdmg, cvar_hget(cvarh_vr_revolver_heavy_mult)));
                // The head pop's chance by range (player lengths 2 .. 12): the revolver's against the shotguns'.
                float L = VR_DECAP_PLAYER_LENGTH;
                float c4 = VR_Decap_PopChance(QVR_DECAP_REVOLVER, 4 * L, 1);
                float c8 = VR_Decap_PopChance(QVR_DECAP_REVOLVER, 8 * L, 1);
                float c11 = VR_Decap_PopChance(QVR_DECAP_REVOLVER, 11 * L, 1);
                float sg8 = VR_Decap_PopChance(QVR_DECAP_SHOTGUN, 8 * L, 1);
                float ssg8 = VR_Decap_PopChance(QVR_DECAP_SSG, 8 * L, 1);
                VR_RVT_Check(c4 >= 1 && c8 >= 0.85 && c11 >= 0.4 && c8 > ssg8 && c8 > sg8,
                    sprintf("its head pop chance: %.2f at 4 player lengths, %.2f at 8, %.2f at 11 (the shotgun's at 8 %.2f, the super shotgun's %.2f)",
                        c4, c8, c11, sg8, ssg8));
                vr_rvt_g = VR_NPT_Grunt(300, WID_REVOLVER, 0);
            }
            vr_rvt_n = 0;
            VR_RVT_Next();
        }
    }
    else if(s == 18)
    {
        // An armed grunt with a revolver: its gun fired as its attack fires it.
        entity a = vr_rvt_g;
        if(vr_rvt_frames == 2)
        {
            self = a;
            a.enemy = p;
            float t0 = a.vr_ag_prop ? a.vr_ag_prop.vr_ag_total_shots : 0;
            if(a.vr_ag_prop) { a.vr_ag_prop.weaponinst.wi_clip = 6; VR_AG_ShootProp(a.vr_ag_prop); }
            self = p;
            VR_RVT_Check(a.vr_ag_prop && a.vr_ag_prop.weapon == WID_REVOLVER && a.vr_ag_prop.vr_ag_total_shots == t0 + 1 &&
                a.vr_ag_prop.weaponinst.wi_clip == 5, sprintf("an armed grunt's revolver (%s): %g shots, %g left",
                    a.vr_ag_prop ? a.vr_ag_prop.netname : "none", a.vr_ag_prop ? a.vr_ag_prop.vr_ag_total_shots - t0 : 0,
                    a.vr_ag_prop ? a.vr_ag_prop.weaponinst.wi_clip : -1));
            VR_AGT_Remove(a);
            self = os;
            VR_RVT_End();
            return;
        }
    }
    self = os;
};
#endif
