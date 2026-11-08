#ifdef QVR_TESTS
// vr_selfshot_test.qc -- shooting yourself (vr_selfshot.qc; ROUND24.md, "Shooting yourself"), with the mock headset
// (any map; God mode off). Ends `selfshot: N passed, M failed`.
// `vr_selfshot_test 1`: aimed: the main hand's shot (FireBullets, one pellet) at the off hand's palm, the off forearm,
//   the thigh: hit there (that part), damage taken, the hand struck buzzes, its wound painted within 1.5 units of the
//   point; a nail at the off forearm; a nail started inside it (a muzzle pressed into it); the head is met (a shot at
//   it mid-fire: the headshot multiplier on the palm's damage, the pain sound), not by a shot ahead from the face; with
//   vr_selfshot 0 nothing; another entity than your own player never met.
// `vr_selfshot_test 2`: ordinary firing: a grid of hand poses (one hand forward, dual side by side, arms crossed, the off
//   hand cupping the gun, pointing down, kicked up, from the hip, turned out and in), each hand firing a shotgun blast,
//   a nail and a lightning bolt's line along its aim: never a self-hit.

float vr_sst_on;    // the running test's number
float vr_sst_s;     // its state
float vr_sst_case;  // the case (aimed: the target; ordinary: the pose)
float vr_sst_t0;    // when the state began (a stuck test: FAIL "timed out")
float vr_sst_wait;  // frames left before the state goes on
float vr_sst_hits0; // vr_ss_hits at the case's start
float vr_sst_hp0;   // health and armour then
float vr_sst_felt0; // vr_ss_felt then
float vr_sst_wounds0; // precise wounds painted then (selfhit_info 1)
entity vr_sst_nail; // the case's nail in flight
float vr_sst_palm;  // the palm shot's damage (case 0): the head shot's reference

float VR_SST_GIVEUP = 6; // seconds a state may last
float VR_SST_POSE_FRAMES = 4; // frames for a mock pose to reach the drawn body (the next frame's input, then its pose)
// The aimed cases' poses: the off hand held out ahead, palm turned in, below the main hand; for the thigh, the main hand
// low and pointing down, the off hand aside.
string VR_SST_AIM_POSE = "vr_mock_hand main 0.25 1.3 -0.3 0 0 0; vr_mock_hand off -0.1 1.0 -0.45 0 -30 0";
string VR_SST_THIGH_POSE = "vr_mock_hand main 0.22 0.9 -0.35 -40 0 0; vr_mock_hand off -0.35 1.1 -0.2 0 0 0";

void(float s, float frames) VR_SST_To =
{
    vr_sst_s = s;
    vr_sst_wait = frames;
    vr_sst_t0 = time;
};

void() VR_SST_End =
{
    if(cvar_hget(cvarh_vr_selfshot_debug) >= 3)
    {
        // (the wounds as they are, looked down at, before the hands go back)
        localcmd("vr_mock_look 55 0; gl_cshiftpercent 0; wait; wait; vr_eyeshot 1; screenshot; vr_mock_look 0 0\n");
    }
    localcmd("vr_mock_hand main; vr_mock_hand off; vr_selfshot 1\n");
    VR_Test_Verdict();
    vr_sst_on = 0;
    cvar_set("vr_selfshot_test", "0");
};

float(entity p) VR_SST_Hp = { return p.health + p.armorvalue; };

void(entity p) VR_SST_Mark =
{
    vr_sst_hits0 = vr_ss_hits;
    vr_sst_hp0 = VR_SST_Hp(p);
    vr_sst_felt0 = vr_ss_felt;
    vr_sst_wounds0 = selfhit_info(1);
};

// The aimed cases' targets: the part and how far along it (0..1) the shot aims.
float(float xCase) VR_SST_Part =
{
    if(xCase == 0) { return QVR_SS_PART_OFF_HAND; }
    if(xCase == 1) { return QVR_SS_PART_OFF_FOREARM; }
    return QVR_SS_PART_LEGS;
};
float(float xCase) VR_SST_Along =
{
    if(xCase == 0) { return 0.6; }
    if(xCase == 1) { return 0.5; }
    return 0.5; // (the legs' capsule from the knees' height up to the hips: the thigh)
};
string(float xCase) VR_SST_Name =
{
    if(xCase == 0) { return "off hand's palm"; }
    if(xCase == 1) { return "off forearm"; }
    if(xCase == 2) { return "thigh"; }
    if(xCase == 3) { return "nail at the off forearm"; }
    return "nail started inside the off forearm";
};

// A nail from `xFrom` going `xDir`, as the main hand's (VR_ShotLaunched marks it).
entity(entity p, vector xFrom, vector xDir) VR_SST_Nail =
{
    entity oldSelf = self;
    self = p;
    entity n = launch_spike(xFrom, xDir);
    VR_ShotLaunched(n, cVR_MainHand);
    setorigin(n, xFrom); // (where it starts: VR_ShotPlace moves it back only for something solid along the gun)
    self = oldSelf;
    return n;
};

void(entity p, vector xDir) VR_SST_Shoot =
{
    entity oldSelf = self;
    self = p;
    FireBullets(cVR_MainHand, 1, xDir, '0 0 0');
    self = oldSelf;
};

// Test 1's states: 0 the body drawn; 1 a case fired; 2 its hit checked, its wound awaited; 3 the head, the option off,
// another entity.
void(entity p) VR_SST_Aimed =
{
    if(vr_sst_s == 0)
    {
        p.health = 100;
        p.armorvalue = 0;
        if(selfhit_info(4) < 7)
        {
            return; // (the body and both forearms posed: the next frames)
        }
        VR_Test_Check(TRUE, sprintf("the drawn body: %g parts", selfhit_info(4)));
        vr_sst_case = 0;
        VR_SST_To(1, 0);
        return;
    }
    if(vr_sst_s == 1)
    {
        p.health = 100;
        VR_SST_Mark(p);
        vector muzzle = VRGetMuzzlePos(cVR_MainHand);
        if(vr_sst_case <= 2)
        {
            vector target = selfhit_point(VR_SST_Part(vr_sst_case), VR_SST_Along(vr_sst_case));
            VR_SST_Shoot(p, normalize(target - muzzle));
            VR_SST_To(2, 0);
        }
        else if(vr_sst_case == 3)
        {
            vr_sst_nail = VR_SST_Nail(p, muzzle, normalize(selfhit_point(QVR_SS_PART_OFF_FOREARM, 0.5) - muzzle));
            VR_SST_To(2, 0);
        }
        else
        {
            vector mid = selfhit_point(QVR_SS_PART_OFF_FOREARM, 0.5);
            vr_sst_nail = VR_SST_Nail(p, mid, '0 0 1'); // (going up out of it)
            VR_SST_To(2, 0);
        }
        return;
    }
    if(vr_sst_s == 2)
    {
        float part = vr_sst_case <= 2 ? VR_SST_Part(vr_sst_case) : QVR_SS_PART_OFF_FOREARM;
        if(vr_ss_hits == vr_sst_hits0)
        {
            return; // (a nail: its flight's next frames)
        }
        if(selfhit_info(1) == vr_sst_wounds0)
        {
            return; // (its wound: painted by the client the next frame)
        }
        string n = VR_SST_Name(vr_sst_case);
        VR_Test_Check(vr_ss_part == part, sprintf("%s: struck part %g (aimed at %g)", n, vr_ss_part, part));
        VR_Test_Check(VR_SST_Hp(p) < vr_sst_hp0, sprintf("%s: damage taken %g", n, vr_sst_hp0 - VR_SST_Hp(p)));
        if(vr_sst_case == 0)
        {
            vr_sst_palm = vr_sst_hp0 - VR_SST_Hp(p); // (the head's reference)
        }
        if(part != QVR_SS_PART_LEGS)
        {
            VR_Test_Check(vr_ss_felt > vr_sst_felt0 && vr_ss_felt_hand == cVR_OffHand,
                sprintf("%s: the off hand buzzed (%g, hand %g)", n, vr_ss_felt - vr_sst_felt0, vr_ss_felt_hand));
        }
        VR_Test_Check(selfhit_info(2) >= 0 && selfhit_info(2) < 1.5 && selfhit_info(3) == part,
            sprintf("%s: wound %.2f units from the hit, on part %g", n, selfhit_info(2), selfhit_info(3)));
        vr_sst_case++;
        if(vr_sst_case == 2 || vr_sst_case == 3)
        {
            localcmd(strcat(vr_sst_case == 2 ? VR_SST_THIGH_POSE : VR_SST_AIM_POSE, "\n"));
        }
        VR_SST_To(vr_sst_case <= 4 ? 1 : 3, VR_SST_POSE_FRAMES);
        return;
    }
    if(vr_sst_s == 3)
    {
        // The head (his note 2026-11-08: shooting yourself in the head): a line through it, side to side, meets it; a
        // shot at it from the hand, mid-fire, takes the palm's damage times the headshot multiplier and cries out.
        vector m = VRGetMuzzlePos(cVR_MainHand);
        vector head = selfhit_point(QVR_SS_PART_HEAD, 0.5);
        makevectors(p.v_angle);
        float fh = selfhit_segment(p, head + v_right * 24, head - v_right * 24, 0, 0);
        VR_Test_Check(fh >= 0 && selfhit_info(0) == QVR_SS_PART_HEAD,
            sprintf("a line through your own head: met (%g, part %g)", fh, selfhit_info(0)));
        p.health = 100;
        p.weaponframe = 2; // (mid-fire: player_pain skips its frames)
        p.pain_finished = 0;
        VR_SST_Mark(p);
        VR_SST_Shoot(p, normalize(head - m));
        float headDmg = vr_sst_hp0 - VR_SST_Hp(p);
        // (a pellet's 4, times Shooting Yourself's damage and the headshot multiplier; the palm's has the limbs')
        float want = 4 * cvar_hget(cvarh_vr_selfshot_damage) *
            (cvar_hget(cvarh_vr_positional_damage) ? cvar_hget(cvarh_vr_headshot_mult) : 1);
        VR_Test_Check(vr_ss_hits > vr_sst_hits0 && vr_ss_part == QVR_SS_PART_HEAD,
            sprintf("a shot at your head: struck part %g", vr_ss_part));
        VR_Test_Check(fabs(headDmg - want) <= 1 && headDmg > vr_sst_palm,
            sprintf("your head: damage %g (a pellet's times the headshot multiplier: %g; the palm's %g)", headDmg, want,
                vr_sst_palm));
        VR_Test_Check(p.pain_finished > time, sprintf("your head, mid-fire: the pain sound played (%s)", p.noise));
        p.weaponframe = 0;
        p.health = 100;
        // A gun held at the eyes aiming ahead (its line from in front of the face): not met.
        float fa = selfhit_segment(p, head + v_forward * 8, head + v_forward * 600, 0, 0);
        VR_Test_Check(fa < 0, sprintf("a shot ahead from in front of your face: not met (%g)", fa));
        // The option off: nothing.
        cvar_set("vr_selfshot", "0");
        VR_SST_Mark(p);
        VR_SST_Shoot(p, normalize(selfhit_point(QVR_SS_PART_OFF_HAND, 0.6) - m));
        VR_Test_Check(vr_ss_hits == vr_sst_hits0 && VR_SST_Hp(p) == vr_sst_hp0, "vr_selfshot 0: the palm shot does nothing");
        cvar_set("vr_selfshot", "1");
        // Another entity (a co-op guest, a bot: the engine draws no body for it) is never met.
        entity guest = spawn();
        guest.flags = FL_CLIENT;
        float f = selfhit_segment(guest, m, selfhit_point(QVR_SS_PART_OFF_HAND, 0.6) + (selfhit_point(QVR_SS_PART_OFF_HAND, 0.6) - m), 0, 0);
        remove(guest);
        VR_Test_Check(f < 0, sprintf("another player's segment through your hand: not met (%g)", f));
        VR_SST_End();
    }
};

// Test 2's poses: main hand x y z pitch yaw, off hand the same (vr_mock_hand: metres, +x right, +y up, -z forward;
// pitch up, yaw left; the gun aims 70 degrees under the controller's pitch: 70 is level).
string(float xCase) VR_SST_Pose =
{
    if(xCase == 0) { return "vr_mock_hand main 0.2 1.2 -0.4 70 0 0; vr_mock_hand off -0.25 1.0 -0.2 70 0 0"; } // one hand forward
    if(xCase == 1) { return "vr_mock_hand main 0.15 1.25 -0.45 70 0 0; vr_mock_hand off -0.15 1.25 -0.45 70 0 0"; } // dual, side by side
    if(xCase == 2) { return "vr_mock_hand main -0.1 1.22 -0.42 70 0 0; vr_mock_hand off 0.1 1.15 -0.38 70 0 0"; } // dual, arms crossed
    if(xCase == 3) { return "vr_mock_hand main 0.05 1.2 -0.4 70 0 0; vr_mock_hand off 0.03 1.16 -0.37 70 0 0"; } // the off hand cupping the grip
    if(xCase == 4) { return "vr_mock_hand main 0.2 1.1 -0.4 25 0 0; vr_mock_hand off -0.2 1.1 -0.4 25 0 0"; } // pointing down ahead
    if(xCase == 5) { return "vr_mock_hand main 0.2 1.25 -0.4 105 0 0; vr_mock_hand off -0.2 1.25 -0.4 105 0 0"; } // kicked up (recoil)
    if(xCase == 6) { return "vr_mock_hand main 0.22 0.95 -0.25 70 0 0; vr_mock_hand off -0.22 0.95 -0.25 70 0 0"; } // from the hip
    if(xCase == 7) { return "vr_mock_hand main 0.35 1.2 -0.25 70 -60 0; vr_mock_hand off -0.35 1.2 -0.25 70 60 0"; } // turned out
    if(xCase == 8) { return "vr_mock_hand main 0.2 1.25 -0.45 70 25 0; vr_mock_hand off -0.2 1.0 -0.25 50 -25 0"; } // turned in, the other low
    if(xCase == 9) { return "vr_mock_hand main 0.2 1.1 -0.35 0 0 0; vr_mock_hand off -0.25 1.0 -0.15 0 0 0"; } // at the floor ahead of the feet
    return "";
};
float VR_SST_POSES = 10;

// One hand's ordinary fire: a blast of six pellets, a nail, the lightning's line, along its aim.
void(entity p, float xHand) VR_SST_FireHand =
{
    entity oldSelf = self;
    self = p;
    vector fwd = VRGetWeaponFireRotFwd(xHand);
    vector muzzle = VRGetMuzzlePos(xHand);
    FireBullets(xHand, 6, fwd, '0.04 0.04 0');
    entity n = launch_spike(muzzle, fwd);
    VR_ShotLaunched(n, xHand);
    vr_sst_nail = n;
    VR_SelfShot_Segment(p, xHand, muzzle, muzzle + fwd * 600);
    self = oldSelf;
};

// Test 2's states: 0 a pose set; 1 its fire (both hands); 2 the nails gone by.
void(entity p) VR_SST_Ordinary =
{
    if(vr_sst_s == 0)
    {
        p.health = 100;
        if(vr_sst_case == 0)
        {
            vr_sst_hits0 = vr_ss_hits;
        }
        localcmd(strcat(VR_SST_Pose(vr_sst_case), "\n"));
        VR_SST_To(1, VR_SST_POSE_FRAMES);
        return;
    }
    if(vr_sst_s == 1)
    {
        p.health = 100;
        if(selfhit_info(4) < 7)
        {
            return;
        }
        float before = vr_ss_hits;
        VR_SST_FireHand(p, cVR_MainHand);
        entity mainNail = vr_sst_nail;
        VR_SST_FireHand(p, cVR_OffHand);
        vr_sst_nail.enemy = mainNail; // (both awaited)
        if(vr_ss_hits != before)
        {
            VR_Test_Check(FALSE, sprintf("pose %g: a hitscan self-hit (part %g at %v)", vr_sst_case, vr_ss_part, vr_ss_at));
        }
        VR_SST_To(2, 0);
        return;
    }
    if(vr_sst_s == 2)
    {
        // The nails flown 96 units off or gone (a hit, a wall).
        entity a = vr_sst_nail, b = vr_sst_nail.enemy;
        float aOn = a.classname == "spike" && vlen(a.origin - p.origin) < 96;
        float bOn = b.classname == "spike" && vlen(b.origin - p.origin) < 96;
        if(aOn || bOn)
        {
            return;
        }
        vr_sst_case++;
        if(vr_sst_case < VR_SST_POSES)
        {
            VR_SST_To(0, 0);
            return;
        }
        VR_Test_Check(vr_ss_hits == vr_sst_hits0,
            sprintf("ordinary firing, %g poses, both hands, blasts, nails and bolts: %g self-hits", VR_SST_POSES,
                vr_ss_hits - vr_sst_hits0));
        VR_SST_End();
    }
};

void() VR_SelfShotTest_Frame =
{
    float ask = cvar_hget(cvarh_vr_selfshot_test);
    if(!ask)
    {
        return;
    }
    entity p = VR_Player();
    if(p == world)
    {
        return;
    }
    if(vr_sst_on != ask)
    {
        VR_Test_Begin("selfshot");
        vr_sst_on = ask;
        vr_sst_case = 0;
        localcmd(sprintf("vr_selfshot 1; %s\n", VR_SST_AIM_POSE)); // (the ordinary poses set their own)
        VR_SST_To(0, VR_SST_POSE_FRAMES);
    }
    if(vr_sst_wait > 0)
    {
        vr_sst_wait = vr_sst_wait - 1;
        return;
    }
    if(time - vr_sst_t0 > VR_SST_GIVEUP)
    {
        VR_Test_Check(FALSE, sprintf("timed out: test %g, case %g, state %g", ask, vr_sst_case, vr_sst_s));
        VR_SST_End();
        return;
    }
    entity oldSelf = self;
    self = p; // (the hands' getters read self)
    if(ask == 1) { VR_SST_Aimed(p); }
    else if(ask == 2) { VR_SST_Ordinary(p); }
    else { VR_SST_End(); }
    self = oldSelf;
};

#endif
