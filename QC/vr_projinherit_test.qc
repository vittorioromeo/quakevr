#ifdef QVR_TESTS
// vr_projinherit_test.qc -- what a projectile keeps of the player's motion (weapons.qc vrProjectileVelocity; ROUND24.md,
// "Projectiles keep less of your motion"). Any map. Ends `projinherit: N passed, M failed`.
// `vr_projinherit_test 1` (the mock headset): strafing at 320 u/s with the hand still, a nail and a rocket fly within
//   0.5 degrees of the aim and a grenade keeps vr_proj_inherit_lobbed of the strafe; vr_proj_inherit_fast 1 gives the
//   rocket all of it; then the main hand swung (vr_mock_swing), a grenade keeps vr_proj_inherit_lobbed_hand of the
//   hand's velocity and a rocket none of it.
// `vr_projinherit_test 2` (-flatscreenmode): the view turned fast (+right), the flat-screen hands swinging round with it
//   (their motion synthesized; the caller holds +right with cl_yawspeed 1440 first): the hand reports no velocity, VR_HandMotionTracked says no, and a grenade fired with the
//   player still keeps nothing of the swing; then, for the record, the hand's velocity with vr_handsource_gate 0.

float vr_pit_on;    // the running test's number
float vr_pit_s;     // its state
float vr_pit_t0;    // when the state began (a stuck test: FAIL "timed out")
float vr_pit_wait;  // frames left before the state goes on
float vr_pit_fast;  // vr_proj_inherit_fast as it was (put back)
float vr_pit_n;     // (2) frames the hand has swung fast
vector vr_pit_rel;  // (2) the main hand's place from the player's origin when last seen
float vr_pit_relt;  // (2) and when

float VR_PIT_GIVEUP = 6;    // seconds a state may last
float VR_PIT_STRAFE = 320;  // u/s: a running strafe
float VR_PIT_AIM_DEG = 0.5; // degrees a fast shot may be off the aim
float VR_PIT_SPREAD = 15;   // u/s: a grenade's own spread (10 right and 10 up, at most) and rounding
float VR_PIT_SWING = 2;     // m/s: a hand swinging fast enough to tell

void(float s, float frames) VR_PIT_To =
{
    vr_pit_s = s;
    vr_pit_wait = frames;
    vr_pit_t0 = time;
};

void() VR_PIT_End =
{
    cvar_set("vr_mock_swing", "0"); // (cvars set at once: a localcmd waits behind the rest of a run's script)
    cvar_set("vr_handsource_gate", "1");
    if(vr_pit_on == 2) { localcmd("-right\n"); }
    cvar_set("vr_proj_inherit_fast", ftos(vr_pit_fast));
    VR_Test_Verdict();
    vr_pit_on = 0;
    cvar_set("vr_projinherit_test", "0");
};

// Ammo enough for any gun in the hand (W_FireSpikes asks for one).
void(entity p) VR_PIT_Arm =
{
    p.ammo_shells = p.ammo_nails = p.ammo_rockets = p.ammo_cells = 100;
    if(VRGetWeapon(cVR_MainHand) != WID_NAILGUN)
    {
        VRGiveEntWeapon(p, cVR_MainHand, WID_NAILGUN, 0, 0);
    }
    VRSetWeaponClip(cVR_MainHand, 10);
};

// A nail fired from the main hand (self: the player): the spike made (world: none).
entity() VR_PIT_Nail =
{
    entity e = find(world, classname, "spike");
    while(e != world)
    {
        entity next = find(e, classname, "spike");
        if(e.owner == self) { remove(e); } // (the test's nail is the one left after)
        e = next;
    }
    W_FireSpikes(cVR_MainHand, 0);
    e = find(world, classname, "spike");
    while(e != world && e.owner != self) { e = find(e, classname, "spike"); }
    return e;
};

// A grenade's velocity beyond its launch (600 u/s along the aim; with no tracked hands, 200 up too): what it kept.
vector(entity g) VR_PIT_Kept =
{
    makevectors(VRGetWeaponFireRot(cVR_MainHand));
    vector base = v_forward * 600;
    if(!VRHandsTracked(self)) { base += v_up * 200; }
    return g.velocity - base;
};

// What a grenade should keep: the lobbed shares of the player's and of the hand's velocity.
vector() VR_PIT_LobbedWant =
{
    if(!VRHandsTracked(self)) { return VEC_ORIGIN; }
    vector hv = VR_HandMotionTracked(self, cVR_MainHand) ? VRGetHandVel(cVR_MainHand) : VEC_ORIGIN;
    return self.velocity * cvar_hget(cvarh_vr_proj_inherit_lobbed) +
           hv * (VRMetersToUnits() * cvar_hget(cvarh_vr_proj_inherit_lobbed_hand));
};

// The strafe: a nail, a rocket and a grenade fired at 320 u/s to the aim's right, the hand still.
void(entity p) VR_PIT_Strafe =
{
    VR_PIT_Arm(p);
    makevectors(VRGetWeaponFireRot(cVR_MainHand));
    vector fwd = v_forward;
    vector right = v_right;
    right_z = 0;
    right = normalize(right);
    vector vel0 = p.velocity;
    p.velocity = right * VR_PIT_STRAFE;
    VR_Test_Print(sprintf("projinherit: hand %.2f m/s, tracked %g, motion tracked %g\n", vlen(VRGetHandVel(cVR_MainHand)),
        VRHandsTracked(p), VR_HandMotionTracked(p, cVR_MainHand)));

    entity n = VR_PIT_Nail();
    if(n == world)
    {
        VR_Test_Check(FALSE, "strafe: no nail fired");
    }
    else
    {
        float dn = VR_DegreesBetween(n.velocity, fwd);
        VR_Test_Check(dn <= VR_PIT_AIM_DEG, sprintf("strafe: nail %.2f deg off the aim (speed %.0f)", dn, vlen(n.velocity)));
        remove(n);
    }
    entity r = VR_FireRocketMissile(cVR_MainHand);
    float dr = VR_DegreesBetween(r.velocity, fwd);
    VR_Test_Check(dr <= VR_PIT_AIM_DEG, sprintf("strafe: rocket %.2f deg off the aim", dr));
    remove(r);

    entity g = VR_FireGrenadeMissile(cVR_MainHand);
    vector kept = VR_PIT_Kept(g);
    vector want = VR_PIT_LobbedWant();
    VR_Test_Check(vlen(kept - want) <= VR_PIT_SPREAD && fabs(kept * right - VR_PIT_STRAFE * cvar_hget(cvarh_vr_proj_inherit_lobbed)) <= VR_PIT_SPREAD,
        sprintf("strafe: grenade kept %.0f u/s of the strafe (want %.0f; off by %.1f)", kept * right, want * right,
            vlen(kept - want)));
    remove(g);

    // The cvar is read: all of it for fast shots.
    cvar_set("vr_proj_inherit_fast", "1");
    r = VR_FireRocketMissile(cVR_MainHand);
    float side = (r.velocity - fwd * 1000) * right;
    VR_Test_Check(fabs(side - VR_PIT_STRAFE) <= 1, sprintf("strafe: vr_proj_inherit_fast 1: rocket kept %.1f u/s", side));
    remove(r);
    cvar_set("vr_proj_inherit_fast", ftos(vr_pit_fast));
    p.velocity = vel0;
};

// The swing: the hand's velocity in a grenade, never in a rocket (the player still).
void(entity p) VR_PIT_Flick =
{
    VR_PIT_Arm(p);
    vector vel0 = p.velocity;
    p.velocity = VEC_ORIGIN;
    vector hv = VRGetHandVel(cVR_MainHand);
    vector fwd = VRGetWeaponFireRotFwd(cVR_MainHand);
    entity g = VR_FireGrenadeMissile(cVR_MainHand);
    vector kept = VR_PIT_Kept(g);
    vector want = VR_PIT_LobbedWant();
    VR_Test_Check(VR_HandMotionTracked(p, cVR_MainHand) && vlen(kept - want) <= VR_PIT_SPREAD &&
                  vlen(want) >= VR_PIT_SWING * VRMetersToUnits() * cvar_hget(cvarh_vr_proj_inherit_lobbed_hand) * 0.9,
        sprintf("flick: hand %.2f m/s, grenade kept %.0f u/s (want %.0f; off by %.1f)", vlen(hv), vlen(kept), vlen(want),
            vlen(kept - want)));
    remove(g);
    entity r = VR_FireRocketMissile(cVR_MainHand);
    float dr = VR_DegreesBetween(r.velocity, fwd);
    VR_Test_Check(dr <= VR_PIT_AIM_DEG, sprintf("flick: rocket %.2f deg off the aim", dr));
    remove(r);
    p.velocity = vel0;
};

// Test 1's states: 0 the strafe, then the swing begun; 1 waiting for a fast hand; 2 done.
void(entity p) VR_PIT_Mock =
{
    if(vr_pit_s == 0)
    {
        VR_PIT_Strafe(p);
        cvar_set("vr_mock_swing", "0.35");
        VR_PIT_To(1, 2);
    }
    else if(vr_pit_s == 1)
    {
        if(vlen(VRGetHandVel(cVR_MainHand)) >= VR_PIT_SWING)
        {
            VR_PIT_Flick(p);
            VR_PIT_End();
        }
    }
};

// Test 2's states: 0 waiting for the hand to swing fast with the turn (the run's script, or the Debug menu's row, holds
// +right: a command, run only after the rest of a script); 2 the gate off, a few frames.
void(entity p) VR_PIT_Flat =
{
    vector rel = VRGetHandPos(cVR_MainHand) - p.origin;
    float swing = vlen(rel - vr_pit_rel) / max(0.001, time - vr_pit_relt) / VRMetersToUnits(); // m/s, from its places
    vr_pit_rel = rel;
    vr_pit_relt = time;
    if(vr_pit_s <= 1)
    {
        vr_pit_n = swing >= VR_PIT_SWING ? vr_pit_n + 1 : 0;
        if(vr_pit_n < 3)
        {
            return;
        }
        VR_PIT_Arm(p);
        vector vel0 = p.velocity;
        p.velocity = VEC_ORIGIN;
        vector hv = VRGetHandVel(cVR_MainHand);
        VR_Test_Check(!VR_HandMotionTracked(p, cVR_MainHand) && vlen(hv) < 0.01,
            sprintf("flat turn: the hand swings %.2f m/s, reports %.2f m/s, motion tracked %g (hands tracked %g)", swing,
                vlen(hv), VR_HandMotionTracked(p, cVR_MainHand), VRHandsTracked(p)));
        entity g = VR_FireGrenadeMissile(cVR_MainHand);
        vector kept = VR_PIT_Kept(g);
        VR_Test_Check(vlen(kept) <= VR_PIT_SPREAD, sprintf("flat turn: grenade kept %.1f u/s", vlen(kept)));
        remove(g);
        p.velocity = vel0;
        cvar_set("vr_handsource_gate", "0");
        VR_PIT_To(2, 4);
    }
    else if(vr_pit_s == 2)
    {
        // For the record: what the engine would send with the gate off (the gate is what keeps it out).
        VR_Test_Print(sprintf("projinherit: flat turn, vr_handsource_gate 0: the hand swings %.2f m/s, reports %.2f m/s, "
            "motion tracked %g\n", swing, vlen(VRGetHandVel(cVR_MainHand)), VR_HandMotionTracked(p, cVR_MainHand)));
        VR_PIT_End();
    }
};

void() VR_ProjInheritTest_Frame =
{
    float ask = cvar_hget(cvarh_vr_projinherit_test);
    if(!ask)
    {
        return;
    }
    entity p = VR_Player();
    if(p == world)
    {
        return;
    }
    if(vr_pit_on != ask)
    {
        VR_Test_Begin("projinherit");
        vr_pit_on = ask;
        vr_pit_fast = cvar_hget(cvarh_vr_proj_inherit_fast);
        vr_pit_rel = VRGetHandPos(cVR_MainHand) - p.origin;
        vr_pit_relt = time;
        VR_PIT_To(0, 2);
    }
    if(vr_pit_wait > 0)
    {
        vr_pit_wait = vr_pit_wait - 1;
        return;
    }
    if(time - vr_pit_t0 > VR_PIT_GIVEUP)
    {
        VR_Test_Check(FALSE, sprintf("timed out: test %g, state %g", ask, vr_pit_s));
        VR_PIT_End();
        return;
    }
    entity oldSelf = self;
    self = p; // (the hands' getters and the guns read self)
    if(ask == 1) { VR_PIT_Mock(p); }
    else if(ask == 2) { VR_PIT_Flat(p); }
    else { VR_PIT_End(); }
    self = oldSelf;
};

#endif
