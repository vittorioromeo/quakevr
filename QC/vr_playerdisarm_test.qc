#ifdef QVR_TESTS
// vr_playerdisarm_test.qc -- the player disarmed (vr_playerdisarm.qc; ROUND25.md, "The player disarmed"), with the mock
// headset. `vr_pd_test 1`: the shotgun held out in the main hand (a fist in the other), vr_player_disarm_chance 100
// (certain): a grunt's blast across the gun knocks it out: it drops, settles on the floor, a world weapon with its clip;
// a blast across the body: no disarm; an enforcer's laser across the gun: knocked out; the option off: never.
// Ends with `pdtest: N passed, M failed`.

float vr_pdt_on;     // the running test's number
float vr_pdt_s;      // its state
float vr_pdt_t0;     // when the state began (a stuck test: FAIL "timed out")
float vr_pdt_wait;   // frames left before the state goes on
float vr_pdt_n;      // frames the state has run
float vr_pdt_dis0;   // vr_pd_disarms at the case's start
float vr_pdt_hits0;  // vr_pd_hits then
float vr_pdt_chance; // vr_player_disarm_chance before the test
entity vr_pdt_shooter; // the case's shooter (a stand-in grunt or enforcer)
entity vr_pdt_drop;  // the knocked-out weapon
entity vr_pdt_laser; // the laser case's bolt

float VR_PDT_GIVEUP = 6;      // seconds a state may last
float VR_PDT_POSE_FRAMES = 6; // frames for a mock pose and a weapon given to be in place
float VR_PDT_CLIP = 5;        // the shotgun's clip (kept by the dropped weapon)
string VR_PDT_POSE = "vr_mock_hand main 0.2 1.2 -0.4 70 0 0; vr_mock_hand off -0.25 1.0 -0.2 70 0 0";

void(float s, float frames) VR_PDT_To =
{
    vr_pdt_s = s;
    vr_pdt_wait = frames;
    vr_pdt_t0 = time;
    vr_pdt_n = 0;
};

void() VR_PDT_End =
{
    cvar_set("vr_player_disarm", "1");
    cvar_set("vr_player_disarm_chance", ftos(vr_pdt_chance));
    if(vr_pdt_shooter)
    {
        remove(vr_pdt_shooter);
        vr_pdt_shooter = world;
    }
    localcmd("vr_mock_hand main; vr_mock_hand off\n");
    VR_Test_Verdict();
    vr_pdt_on = 0;
    cvar_set("vr_pd_test", "0");
};

void(entity p) VR_PDT_Arm =
{
    VRGiveEntWeapon(p, cVR_MainHand, WID_SHOTGUN, 0, VR_PDT_CLIP);
    VRGiveEntWeapon(p, cVR_OffHand, WID_FIST, 0, 0);
    W_SetCurrentAmmoFor(cVR_MainHand);
    VR_Scene_HoldGiven(p, cVR_MainHand); // (held with the mock's grip unpressed)
    p.health = 100;
    vr_pdt_dis0 = vr_pd_disarms;
    vr_pdt_hits0 = vr_pd_hits;
};

// A stand-in shooter (never thinks: the test fires for it).
entity(string cls) VR_PDT_Shooter =
{
    if(vr_pdt_shooter)
    {
        remove(vr_pdt_shooter);
    }
    vr_pdt_shooter = spawn();
    vr_pdt_shooter.classname = cls;
    vr_pdt_shooter.flags = FL_MONSTER;
    vr_pdt_shooter.takedamage = DAMAGE_AIM;
    vr_pdt_shooter.health = 100;
    return vr_pdt_shooter;
};

// Across the main hand's gun from its right (or `xLow`: across the body, at its middle, under the gun), 200 units off.
vector vr_pdt_from;
vector vr_pdt_dir;
void(entity p, float xLow) VR_PDT_Aim =
{
    VR_Parry_WeaponLine(p, cVR_MainHand);
    vector mid = vr_parry_butt + (vr_parry_end - vr_parry_butt) * 0.75; // (out ahead of your box: the laser's touch)
    vector fwd = vr_parry_end - vr_parry_butt;
    fwd_z = 0;
    fwd = normalize(fwd);
    vector side;
    side_x = fwd_y;
    side_y = -fwd_x;
    side_z = 0;
    vector at = xLow ? p.origin : mid;
    traceline(at, at + side * 200, TRUE, p); // (never inside a wall)
    vr_pdt_from = trace_endpos - side * 8;
    vr_pdt_dir = normalize(at - vr_pdt_from);
};

void(entity p) VR_PDT_Blast =
{
    entity oldSelf = self;
    self = VR_PDT_Shooter("monster_army");
    setorigin(self, vr_pdt_from);
    FireBulletsImpl(6, vr_pdt_dir, '0 0 0', vr_pdt_from, 2048, FALSE);
    self = oldSelf;
};

void(entity p) VR_PDT_Run =
{
    vr_pdt_n = vr_pdt_n + 1;
    if(vr_pdt_s == 0)
    {
        VR_PDT_Arm(p);
        VR_PDT_To(1, VR_PDT_POSE_FRAMES);
        return;
    }
    if(vr_pdt_s == 1)
    {
        // A grunt's blast across the gun.
        VR_Test_Check(VR_Parry_WeaponLine(p, cVR_MainHand), "the shotgun held out in the main hand: its line");
        VR_PDT_Aim(p, FALSE);
        VR_PDT_Blast(p);
        VR_Test_Check(vr_pd_hits > vr_pdt_hits0, sprintf("a blast across the gun: hits counted on it (%g)", vr_pd_hits - vr_pdt_hits0));
        VR_PDT_To(2, 2); // (the roll: the next PlayerPostThink)
        return;
    }
    if(vr_pdt_s == 2)
    {
        VR_Test_Check(vr_pd_disarms == vr_pdt_dis0 + 1 && vr_pd_last_hand == cVR_MainHand,
            sprintf("a blast across the gun: knocked out of the main hand (%g)", vr_pd_disarms - vr_pdt_dis0));
        VR_Test_Check(VRIsHandWeaponless(p, cVR_MainHand), "the main hand empty");
        vr_pdt_drop = vr_wpn_dropped;
        VR_Test_Check(vr_pdt_drop != world && vr_pdt_drop.classname == "thrown_weapon" && vr_pdt_drop.weapon == WID_SHOTGUN &&
            vr_pdt_drop.handtouch == wpnthrow_handtouch,
            sprintf("dropped as a world weapon you can take: %s, weapon %g", vr_pdt_drop.classname, vr_pdt_drop.weapon));
        VR_Test_Check(vr_pdt_drop.weaponinst.wi_clip == VR_PDT_CLIP,
            sprintf("its clip kept: %g (was %g)", vr_pdt_drop.weaponinst.wi_clip, VR_PDT_CLIP));
        VR_PDT_To(3, 0);
        return;
    }
    if(vr_pdt_s == 3)
    {
        // Settled: still for 10 frames (a frame's cap: the give-up).
        if(vr_pdt_drop == world || vlen(vr_pdt_drop.velocity) > 1)
        {
            vr_pdt_n = 0;
            return;
        }
        if(vr_pdt_n < 10)
        {
            return;
        }
        traceline(vr_pdt_drop.origin, vr_pdt_drop.origin - '0 0 32', TRUE, vr_pdt_drop);
        VR_Test_Check(trace_fraction < 1 && vr_pdt_drop.origin_z - trace_endpos_z < 16 && vr_pdt_drop.origin_z < p.origin_z,
            sprintf("it lies on the floor: %.1f units over it (you at %.1f, it at %.1f)", vr_pdt_drop.origin_z - trace_endpos_z,
            p.origin_z, vr_pdt_drop.origin_z));
        remove(vr_pdt_drop);
        vr_pdt_drop = world;
        VR_PDT_Arm(p);
        VR_PDT_To(4, VR_PDT_POSE_FRAMES);
        return;
    }
    if(vr_pdt_s == 4)
    {
        // A blast across the body, under the gun.
        VR_PDT_Aim(p, TRUE);
        VR_PDT_Blast(p);
        VR_PDT_To(5, 2);
        return;
    }
    if(vr_pdt_s == 5)
    {
        VR_Test_Check(vr_pd_disarms == vr_pdt_dis0 && vr_pd_hits == vr_pdt_hits0 && !VRIsHandWeaponless(p, cVR_MainHand),
            sprintf("a blast across the body: no hit on the gun (%g), not disarmed (%g)", vr_pd_hits - vr_pdt_hits0,
            vr_pd_disarms - vr_pdt_dis0));
        VR_PDT_Arm(p);
        // An enforcer's laser across the gun.
        VR_PDT_Aim(p, FALSE);
        entity oldSelf = self;
        self = VR_PDT_Shooter("monster_enforcer");
        setorigin(self, vr_pdt_from);
        vr_pdt_laser = LaunchLaser(vr_pdt_from, vr_pdt_dir);
        vr_pdt_laser.velocity = vr_pdt_dir * 600; // (straight at it: no aim cone)
        self = oldSelf;
        VR_PDT_To(6, 0);
        return;
    }
    if(vr_pdt_s == 6)
    {
        if(vr_pd_disarms == vr_pdt_dis0 && vr_pdt_laser.solid == SOLID_BBOX && vr_pdt_laser.touch == Laser_Touch)
        {
            return; // (on its way: 200 units at 600 a second)
        }
        VR_Test_Check(vr_pd_disarms == vr_pdt_dis0 + 1 && VRIsHandWeaponless(p, cVR_MainHand),
            sprintf("an enforcer's laser across the gun: knocked out (%g)", vr_pd_disarms - vr_pdt_dis0));
        if(vr_wpn_dropped != world)
        {
            remove(vr_wpn_dropped);
            vr_wpn_dropped = world;
        }
        VR_PDT_Arm(p);
        cvar_set("vr_player_disarm", "0");
        VR_PDT_To(7, VR_PDT_POSE_FRAMES);
        return;
    }
    if(vr_pdt_s == 7)
    {
        VR_PDT_Aim(p, FALSE);
        VR_PDT_Blast(p);
        VR_PDT_To(8, 2);
        return;
    }
    if(vr_pdt_s == 8)
    {
        VR_Test_Check(vr_pd_disarms == vr_pdt_dis0 && vr_pd_hits == vr_pdt_hits0 && !VRIsHandWeaponless(p, cVR_MainHand),
            sprintf("vr_player_disarm 0: a blast across the gun does nothing (%g hits, %g disarms)", vr_pd_hits - vr_pdt_hits0,
            vr_pd_disarms - vr_pdt_dis0));
        VR_PDT_End();
    }
};

void() VR_PlayerDisarmTest_Frame =
{
    float ask = cvar_hget(cvarh_vr_pd_test);
    if(!ask)
    {
        return;
    }
    entity p = VR_Player();
    if(p == world)
    {
        return;
    }
    if(vr_pdt_on != ask)
    {
        VR_Test_BeginFlags("pdtest", VR_TEST_LOUD);
        vr_pdt_on = ask;
        vr_pdt_chance = cvar_hget(cvarh_vr_player_disarm_chance);
        cvar_set("vr_player_disarm", "1");
        cvar_set("vr_player_disarm_chance", "100"); // (certain)
        localcmd(sprintf("%s\n", VR_PDT_POSE));
        VR_PDT_To(0, VR_PDT_POSE_FRAMES);
    }
    if(vr_pdt_wait > 0)
    {
        vr_pdt_wait = vr_pdt_wait - 1;
        return;
    }
    if(time - vr_pdt_t0 > VR_PDT_GIVEUP)
    {
        VR_Test_Check(FALSE, sprintf("timed out: state %g", vr_pdt_s));
        VR_PDT_End();
        return;
    }
    entity oldSelf = self;
    self = p; // (the hands' getters read self)
    if(ask == 1) { VR_PDT_Run(p); }
    else { VR_PDT_End(); }
    self = oldSelf;
};

#endif
