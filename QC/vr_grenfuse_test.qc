#ifdef QVR_TESTS
// vr_grenfuse_test.qc -- one fuse for every grenade, and the countdown on launched ones (vr_grenade.qc VR_Grenade_Fuse,
// VR_Grenade_Light; ROUND25.md, "One grenade fuse; launched grenades tick"). Any map with some room ahead (e1m1's
// start). Ends `grenfuse: N passed, M failed`.
// `vr_grenfuse_test 1`: in turn, a hand grenade armed on the floor ahead (its pin: VR_HandGrenade_Arm), the grenade
//   launcher's grenade fired from the main hand (VR_FireGrenadeMissile), an ogre's grenade (OgreFireGrenade, from a
//   stand-in thrower ahead lobbing it further away), and the launcher's again with vr_grenade_countdown 0: each is
//   lit as it is armed or thrown (the fizz; its ticks timed to its fuse) but the last, each goes off VR_Grenade_Fuse
//   after its arming (one frame's detection late), and the three blow up within a frame of each other with the same
//   ticks (one apart at most). vr_grenade_catch 2 and vr_grenade_countdown 1 for the test (put back after); the player
//   takes no damage meanwhile. Then a launcher's grenade a hand takes out of the air 1.5 s in: its fuse starts over (off
//   VR_Grenade_Fuse after the grab), the restart cue plays once, and its ticks stay one stream (VR_Grenade_Relight).

float vr_gft_on;     // the running test's number
float vr_gft_s;      // its state: 0 set up, 1 a grenade's turn starts, 2 its fuse runs, 3 after its blast
float vr_gft_case;   // the grenade's turn: 0 hand, 1 launcher, 2 ogre, 3 launcher without the countdown
float vr_gft_t0;     // when the state began (a stuck test: FAIL "timed out")
float vr_gft_wait;   // frames left before the state goes on
entity vr_gft_g;     // the grenade
float vr_gft_arm;    // when it was armed (or thrown)
float vr_gft_ticks0; // vr_gren_ticks then
float vr_gft_catch;  // vr_grenade_catch as it was (put back)
float vr_gft_count;  // vr_grenade_countdown as it was
float vr_gft_dmg;    // the player's takedamage as it was
entity vr_gft_shooter; // the ogre's stand-in
entity vr_gft_target;  // and what it lobs at
float vr_gft_dt0, vr_gft_dt1, vr_gft_dt2;       // each case's blast after its arming
float vr_gft_tk0, vr_gft_tk1, vr_gft_tk2;       // and its ticks

float vr_gft_grab;   // case 4: its grab was made
float vr_gft_rearm0; // vr_gren_rearms then

float VR_GFT_GRABAT = 1.5;   // case 4: seconds after the throw the hand takes it
float VR_GFT_GIVEUP = 8;    // seconds a state may last
float VR_GFT_FRAME = 0.0139; // one fixed frame (1/72 s) and a little
float VR_GFT_AFTER = 36;     // frames after a blast before the next grenade (its explosion done)
float VR_GFT_TICKS = 5;      // ticks a countdown plays at least (2.5 s: about a dozen)

void(float s, float frames) VR_GFT_To =
{
    vr_gft_s = s;
    vr_gft_wait = frames;
    vr_gft_t0 = time;
};

void() VR_GFT_End =
{
    cvar_set("vr_grenade_catch", ftos(vr_gft_catch)); // (cvars set at once: a localcmd waits behind the run's script)
    cvar_set("vr_grenade_countdown", ftos(vr_gft_count));
    entity p = VR_Player();
    if(p != world)
    {
        p.takedamage = vr_gft_dmg;
    }
    if(vr_gft_shooter != world)
    {
        remove(vr_gft_shooter);
        vr_gft_shooter = world;
    }
    if(vr_gft_target != world)
    {
        remove(vr_gft_target);
        vr_gft_target = world;
    }
    VR_Test_Verdict();
    vr_gft_on = 0;
    cvar_set("vr_grenfuse_test", "0");
};

// The case's name, for the lines.
string(float c) VR_GFT_Name =
{
    if(c == 0) { return "hand grenade"; }
    if(c == 1) { return "launcher grenade"; }
    if(c == 2) { return "ogre grenade"; }
    if(c == 4) { return "launcher grenade grabbed mid-air"; }
    return "launcher grenade, countdown off";
};

// The open floor `xDist` units ahead of `p` (short of a wall), at its feet's height.
vector(entity p, float xDist) VR_GFT_Ahead =
{
    makevectors('0 1 0' * p.v_viewangle_y);
    vector from = p.origin + '0 0 8';
    traceline(from, from + v_forward * xDist, TRUE, p);
    vector at = trace_endpos - v_forward * 16;
    traceline(at, at - '0 0 256', TRUE, p);
    return trace_endpos + '0 0 4';
};

// The case's grenade, armed or thrown now (self: the player). World: none made.
entity(entity p) VR_GFT_Launch =
{
    entity g = world;
    if(vr_gft_case == 0)
    {
        g = VR_HandGrenade_Make(p, WID_GRENADE_LAUNCHER, AID_ROCKETS, VR_GFT_Ahead(p, 160), '0 0 0');
        g.velocity = '0 0 0';
        VR_HandGrenade_Arm(g, cVR_MainHand, "the test's pin");
        return g;
    }
    if(vr_gft_case == 1 || vr_gft_case == 3 || vr_gft_case == 4)
    {
        return VR_FireGrenadeMissile(cVR_MainHand);
    }
    // The ogre's: a stand-in thrower a third of the way to the wall ahead, lobbing at a mark by the wall (away from
    // you), as vr_impulse_test.qc's monster_test_shooter.
    vector far = VR_GFT_Ahead(p, 640);
    vector near = p.origin + (far - p.origin) * 0.35;
    near_z = far_z;
    vr_gft_target = spawn();
    vr_gft_target.classname = "info_test_target";
    vr_gft_target.health = 100;
    setorigin(vr_gft_target, far);
    vr_gft_shooter = spawn();
    vr_gft_shooter.classname = "monster_test_shooter";
    vr_gft_shooter.flags = FL_MONSTER;
    vr_gft_shooter.enemy = vr_gft_target;
    setorigin(vr_gft_shooter, near + '0 0 24');
    vr_gft_shooter.angles = vectoangles(far - near);
    entity oldSelf = self;
    self = vr_gft_shooter;
    OgreFireGrenade();
    self = oldSelf;
    for(entity e = nextent(world); e; e = nextent(e))
    {
        if(e.owner == vr_gft_shooter && e.vr_grenade)
        {
            g = e;
        }
    }
    return g;
};

// Whether the case's grenade has gone off (its tick no longer runs: Quake's explosion took over, or it is gone).
float(entity g) VR_GFT_Gone =
{
    return !g.vr_grenade || g.think != VR_Grenade_Think;
};

void(entity p) VR_GFT_Run =
{
    float fuse = VR_Grenade_Fuse();
    string n = VR_GFT_Name(vr_gft_case);
    if(vr_gft_s == 0)
    {
        p.ammo_rockets = max(p.ammo_rockets, 10);
        VR_GFT_To(1, 2);
        return;
    }
    if(vr_gft_s == 1)
    {
        cvar_set("vr_grenade_countdown", vr_gft_case == 3 ? "0" : "1");
        vr_gft_ticks0 = vr_gren_ticks;
        vr_gft_arm = time;
        vr_gft_grab = FALSE;
        vr_gft_g = VR_GFT_Launch(p);
        if(vr_gft_g == world)
        {
            VR_Test_Check(FALSE, sprintf("%s: none made", n));
            VR_GFT_End();
            return;
        }
        if(vr_gft_case == 3)
        {
            VR_Test_Check(!vr_gft_g.vr_gren_lit, sprintf("%s: not lit as it is thrown", n));
        }
        else
        {
            VR_Test_Check(vr_gft_g.vr_gren_lit, sprintf("%s: lit as it is %s (the fizz, the countdown)", n,
                vr_gft_case == 0 ? "armed" : "thrown"));
            VR_Test_Check(fabs(vr_gft_g.vr_gren_full - fuse) < 0.001, sprintf("%s: its ticks timed to its %.2f s fuse (%.2f s)",
                n, fuse, vr_gft_g.vr_gren_full));
        }
        VR_GFT_To(2, 0);
        return;
    }
    if(vr_gft_s == 2)
    {
        if(vr_gft_case == 4 && !vr_gft_grab && time - vr_gft_arm >= VR_GFT_GRABAT && !VR_GFT_Gone(vr_gft_g))
        {
            // A hand takes it out of the air (a force grab's catch: VR_Grenade_Caught at its next tick).
            vr_gft_grab = TRUE;
            vr_gft_arm = time; // (the fuse is timed from the grab now)
            vr_gft_ticks0 = vr_gren_ticks;
            vr_gft_rearm0 = vr_gren_rearms;
            vr_gft_g.carry_player = p; // (the grenade's tick sees the hand: no grip in the mock, so no real carry)
            return;
        }
        if(vr_gft_case == 4 && vr_gft_grab && vr_gft_g.carry_player != world && time - vr_gft_arm > 3 * VR_GFT_FRAME)
        {
            vr_gft_g.carry_player = world; // (the catch seen: it flies on, as if thrown again)
        }
        if(!VR_GFT_Gone(vr_gft_g))
        {
            return;
        }
        float dt = time - vr_gft_arm;
        float ticks = vr_gren_ticks - vr_gft_ticks0;
        if(vr_gft_case == 4)
        {
            VR_Test_Check(vr_gft_grab, sprintf("%s: grabbed at %.1f s", n, VR_GFT_GRABAT));
            VR_Test_Check(vr_gren_rearms - vr_gft_rearm0 == 1, sprintf("%s: the restart cue played once (%g)", n,
                vr_gren_rearms - vr_gft_rearm0));
            VR_Test_Check(ticks <= vr_gft_tk1 + 1, sprintf("%s: one tick stream, not two (%g ticks vs %g in a plain fuse)", n,
                ticks, vr_gft_tk1));
        }
        VR_Test_Check(dt >= fuse - 0.001 && dt <= fuse + 2 * VR_GFT_FRAME, sprintf("%s: went off %.3f s after it was %s (the fuse %.2f s)",
            n, dt, vr_gft_case == 0 ? "armed" : "thrown", fuse));
        if(vr_gft_case == 3)
        {
            VR_Test_Check(ticks == 0, sprintf("%s: no ticks (%g)", n, ticks));
        }
        else
        {
            VR_Test_Check(ticks >= VR_GFT_TICKS, sprintf("%s: its countdown ticked %g times", n, ticks));
        }
        if(vr_gft_case == 0) { vr_gft_dt0 = dt; vr_gft_tk0 = ticks; }
        else if(vr_gft_case == 1) { vr_gft_dt1 = dt; vr_gft_tk1 = ticks; }
        else if(vr_gft_case == 2) { vr_gft_dt2 = dt; vr_gft_tk2 = ticks; }
        VR_GFT_To(3, VR_GFT_AFTER);
        return;
    }
    // (3) its blast done: the next grenade, or the comparison.
    if(vr_gft_case < 4)
    {
        vr_gft_case++;
        VR_GFT_To(1, 0);
        return;
    }
    float lo = min(vr_gft_dt0, min(vr_gft_dt1, vr_gft_dt2));
    float hi = max(vr_gft_dt0, max(vr_gft_dt1, vr_gft_dt2));
    VR_Test_Check(hi - lo <= VR_GFT_FRAME, sprintf("hand, launcher and ogre grenades go off within a frame of each other (%.3f, %.3f, %.3f s)",
        vr_gft_dt0, vr_gft_dt1, vr_gft_dt2));
    lo = min(vr_gft_tk0, min(vr_gft_tk1, vr_gft_tk2));
    hi = max(vr_gft_tk0, max(vr_gft_tk1, vr_gft_tk2));
    VR_Test_Check(hi - lo <= 1, sprintf("and tick alike (%g, %g, %g ticks)", vr_gft_tk0, vr_gft_tk1, vr_gft_tk2));
    VR_GFT_End();
};

void() VR_GrenFuseTest_Frame =
{
    float ask = cvar_hget(cvarh_vr_grenfuse_test);
    if(!ask)
    {
        return;
    }
    entity p = VR_Player();
    if(p == world)
    {
        return;
    }
    if(vr_gft_on != ask)
    {
        VR_Test_Begin("grenfuse");
        vr_gft_on = ask;
        vr_gft_case = 0;
        vr_gft_catch = cvar_hget(cvarh_vr_grenade_catch);
        vr_gft_count = cvar_hget(cvarh_vr_grenade_countdown);
        vr_gft_dmg = p.takedamage;
        p.takedamage = DAMAGE_NO;
        cvar_set("vr_grenade_catch", "2");
        VR_GFT_To(0, 2);
    }
    if(vr_gft_wait > 0)
    {
        vr_gft_wait = vr_gft_wait - 1;
        return;
    }
    if(time - vr_gft_t0 > VR_GFT_GIVEUP)
    {
        VR_Test_Check(FALSE, sprintf("timed out: case %g, state %g", vr_gft_case, vr_gft_s));
        VR_GFT_End();
        return;
    }
    entity oldSelf = self;
    self = p; // (the hands' getters and the guns read self)
    if(ask == 1) { VR_GFT_Run(p); }
    else { VR_GFT_End(); }
    self = oldSelf;
};

#endif
