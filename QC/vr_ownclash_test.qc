#ifdef QVR_TESTS
// vr_clash_test (Debug > Tests > Weapon Clash; vr_ownclash.qc): your two weapons' clash rule on lines laid out in both
// hands' blows (their last pose and now, one frame of 1/72 s apart), through VR_Clash_Frame as the game calls it.
// 1: every case in one frame, then the verdict line `clashtest: <p> passed, <f> failed`. developer 1 prints the
// "clash: ..." lines. 2: the same with real hand poses (the mock headset's): two swords in your hands, the main one swung
// down through the off one, fast then slow (`clashtest2: ...`).

float VR_CLASH_TEST_DT = 0.013888889; // (1/72 s)

// Both hands' blows laid out: the main hand's `xMain` (a VR_MTHING_*) crossing over the off hand's `xOff` along y, moving
// down along z by `xSpeed` m/s over one frame through the off one's line (along x, still), meeting halfway. `xRest`:
// touching already at the last pose (a scrape). The meeting point expected: vr_clash_test_at.
vector vr_clash_test_at;
void(float xMain, float xOff, float xSpeed, float xRest) VR_Clash_TestLay =
{
    vector o = self.origin + '0 0 20';
    float r = VR_Clash_Radius(VR_Clash_MatOf(xMain)) + VR_Clash_Radius(VR_Clash_MatOf(xOff));
    float d = xSpeed * VRMetersToUnits() * VR_CLASH_TEST_DT;
    float z0 = xRest ? r * 0.5 : r + d * 0.5;
    entity b = VR_Blow(cVR_MainHand);
    b.mh_thing = xMain;
    b.mh_n = 2;
    b.mh_fresh = TRUE;
    b.mh_dt = VR_CLASH_TEST_DT;
    b.mh_pp[0] = o + '40 -20 0' + '0 0 1' * z0;
    b.mh_pp[1] = o + '40 20 0' + '0 0 1' * z0;
    b.mh_cp[0] = o + '40 -20 0' + '0 0 1' * (z0 - d);
    b.mh_cp[1] = o + '40 20 0' + '0 0 1' * (z0 - d);
    b = VR_Blow(cVR_OffHand);
    b.mh_thing = xOff;
    b.mh_n = 2;
    b.mh_fresh = TRUE;
    b.mh_dt = VR_CLASH_TEST_DT;
    b.mh_pp[0] = o + '20 0 0';
    b.mh_pp[1] = o + '60 0 0';
    b.mh_cp[0] = b.mh_pp[0];
    b.mh_cp[1] = b.mh_pp[1];
    vr_clash_test_at = o + '40 0 0' + '0 0 0.5' * r;
};

// One case: laid out, the frame's rule run (its cooldown cleared unless `xKeepCool`); whether it clashed.
float(float xMain, float xOff, float xSpeed, float xRest, float xKeepCool) VR_Clash_TestCase =
{
    VR_Clash_TestLay(xMain, xOff, xSpeed, xRest);
    if(!xKeepCool)
    {
        self.vr_clash_next = 0;
    }
    float was = vr_clash_n;
    vr_clash_snd = "";
    VR_Clash_Frame();
    return vr_clash_n > was;
};

void() VR_Clash_TestAll =
{
    VR_Test_Begin("clashtest");
    float melee = cvar("vr_clash_melee");
    float ranged = cvar("vr_clash_ranged");
    float need = cvar("vr_clash_speed");
    cvar_set("vr_clash_melee", "1");
    cvar_set("vr_clash_ranged", "1");
    cvar_set("vr_clash_speed", "1.5");
    float sparks0;
    float tinks0;
    float slow0;

    // Two swords swung into each other fast: sparks, the clang and a tink, at the meeting point.
    sparks0 = vr_clash_nsparks;
    tinks0 = vr_clash_ntinks;
    float hit = VR_Clash_TestCase(VR_MTHING_SWORD, VR_MTHING_SWORD, 6, FALSE, FALSE);
    VR_Test_Check(hit, sprintf("swords at 6 m/s clash (%.2f m/s measured)", vr_clash_spd));
    VR_Test_Check(fabs(vr_clash_spd - 6) < 0.6, sprintf("its speed measured %.2f m/s, swung 6", vr_clash_spd));
    VR_Test_Check(vr_clash_nsparks > sparks0 && vr_clash_sparked > 0, sprintf("swords spark (%.0f)", vr_clash_sparked));
    VR_Test_Check(vr_clash_ntinks > tinks0, "swords tink");
    VR_Test_Check(strstrofs(vr_clash_snd, "axhit", 0) >= 0, // (impactsound picks one of a set: its family's name)
        sprintf("swords ring with the blade's clang (%s)", vr_clash_snd));
    VR_Test_Check(vlen(vr_clash_x - vr_clash_test_at) < 2, sprintf("at the meeting point (%.2f units off)",
        vlen(vr_clash_x - vr_clash_test_at)));
    float fastSparks = vr_clash_sparked;
    float fastVol = vr_clash_vol;

    // Again at once: the pair's cooldown.
    VR_Test_Check(!VR_Clash_TestCase(VR_MTHING_SWORD, VR_MTHING_SWORD, 6, FALSE, TRUE), "none again within the cooldown");

    // Slower, over the least speed: fewer sparks, quieter.
    VR_Clash_TestCase(VR_MTHING_SWORD, VR_MTHING_SWORD, 2.5, FALSE, FALSE);
    VR_Test_Check(vr_clash_sparked > 0 && vr_clash_sparked < fastSparks && vr_clash_vol < fastVol,
        sprintf("2.5 m/s: fewer sparks (%.0f, at 6 %.0f), quieter (%.2f, at 6 %.2f)", vr_clash_sparked, fastSparks,
        vr_clash_vol, fastVol));

    // Slow: nothing.
    slow0 = vr_clash_slow;
    VR_Test_Check(!VR_Clash_TestCase(VR_MTHING_SWORD, VR_MTHING_SWORD, 0.8, FALSE, FALSE), "swords at 0.8 m/s: none");
    VR_Test_Check(vr_clash_slow > slow0, "... counted as a meeting too slow");

    // Resting crossed already (touching at the last pose), moved fast: a scrape, no clash.
    VR_Test_Check(!VR_Clash_TestCase(VR_MTHING_SWORD, VR_MTHING_SWORD, 6, TRUE, FALSE), "touching already: none");

    // Two shotguns: a dull knock, no sparks, no tink.
    sparks0 = vr_clash_nsparks;
    tinks0 = vr_clash_ntinks;
    hit = VR_Clash_TestCase(VR_MTHING_GUN, VR_MTHING_GUN, 6, FALSE, FALSE);
    VR_Test_Check(hit, "guns at 6 m/s clash");
    VR_Test_Check(vr_clash_nsparks == sparks0 && vr_clash_sparked == 0, "guns: no sparks");
    VR_Test_Check(vr_clash_ntinks == tinks0, "guns: no tink");
    VR_Test_Check(strstrofs(vr_clash_snd, "metal_l", 0) >= 0,
        sprintf("guns: the dull light metal knock (%s)", vr_clash_snd));

    // A sword on a gun: fewer sparks, the clank.
    VR_Clash_TestCase(VR_MTHING_SWORD, VR_MTHING_GUN, 6, FALSE, FALSE);
    VR_Test_Check(vr_clash_sparked > 0 && vr_clash_sparked < fastSparks, sprintf("sword on gun: fewer sparks (%.0f)",
        vr_clash_sparked));
    VR_Test_Check(strstrofs(vr_clash_snd, "metal_h", 0) >= 0,
        sprintf("sword on gun: the clank (%s)", vr_clash_snd));

    // A club (a torch) on a sword: a wooden thud, no sparks.
    VR_Clash_TestCase(VR_MTHING_CLUB, VR_MTHING_SWORD, 6, FALSE, FALSE);
    VR_Test_Check(vr_clash_sparked == 0 && strstrofs(vr_clash_snd, "wood_m", 0) >= 0,
        sprintf("club on sword: a thud, no sparks (%s)", vr_clash_snd));

    // The toggles: melee off, swords nothing; ranged off, a gun pair nothing (and a sword on a gun).
    cvar_set("vr_clash_melee", "0");
    VR_Test_Check(!VR_Clash_TestCase(VR_MTHING_SWORD, VR_MTHING_SWORD, 6, FALSE, FALSE), "melee off: swords none");
    cvar_set("vr_clash_melee", "1");
    cvar_set("vr_clash_ranged", "0");
    VR_Test_Check(!VR_Clash_TestCase(VR_MTHING_GUN, VR_MTHING_GUN, 6, FALSE, FALSE), "ranged off: guns none");
    VR_Test_Check(!VR_Clash_TestCase(VR_MTHING_SWORD, VR_MTHING_GUN, 6, FALSE, FALSE), "ranged off: sword on gun none");
    cvar_set("vr_clash_ranged", "1");
    // Empty hands never clash.
    VR_Test_Check(!VR_Clash_TestCase(VR_MTHING_FIST, VR_MTHING_FIST, 6, FALSE, FALSE), "fists: none");

    // An enemy's weapon met by your blow takes the same table: your sword on its sword the blade row, your fist no sparks.
    VR_Clash_Row(VR_Clash_MatOf(VR_MTHING_SWORD), VR_Clash_MatOfWid(WID_SWORD));
    VR_Test_Check(vr_clash_rsparks == 1 && vr_clash_rtink, "enemy: sword on its sword sparks and tinks");
    VR_Clash_Row(VR_Clash_MatOf(VR_MTHING_GUN), VR_Clash_MatOfWid(WID_AXE));
    VR_Test_Check(vr_clash_rsparks > 0 && vr_clash_rsparks < 1 && !vr_clash_rtink, "enemy: gun on its axe fewer sparks");
    VR_Clash_Row(VR_Clash_MatOf(VR_MTHING_FIST), VR_Clash_MatOfWid(WID_SWORD));
    VR_Test_Check(vr_clash_rsparks == 0, "enemy: fist on its sword no sparks");

    cvar_set("vr_clash_melee", ftos(melee));
    cvar_set("vr_clash_ranged", ftos(ranged));
    cvar_set("vr_clash_speed", ftos(need));
    self.vr_clash_next = 0;
    VR_Test_Verdict();
};

// Case 2: the mock headset's hands, two swords held up ahead, leaning apart, side by side; the off hand still, the main
// hand swung across into it (vr_mock_hand each frame). Phases: 1 the swords given and the hands placed
// (VR_CLASH_E2E_SETTLE frames), 2 the slow swing (VR_CLASH_E2E_SLOW m a frame), 3 the main hand drawn back slowly the
// way it came (moving apart: no clash), 4 the fast swing (VR_CLASH_E2E_STEP), then the verdict. (Slow first: the fast
// swing can leave the swords out of the hands, holstered or dropped.)
float VR_CLASH_E2E_SETTLE = 20;   // frames for the hands to settle where they are put
float VR_CLASH_E2E_STEP = 0.05;   // m a frame (3.6 m/s)
float VR_CLASH_E2E_SLOW = 0.008;  // m a frame (0.58 m/s)
float VR_CLASH_E2E_DROP = 0.45;   // m: the main hand's way across
float vr_clash_e2e;   // the phase (0: not running)
float vr_clash_e2e_f; // its frames so far
float vr_clash_e2e_n; // vr_clash_n as it began
float vr_clash_e2e_s; // vr_clash_nsparks as it began
float vr_clash_e2e_slow; // vr_clash_slow as it began

void(float xAcross) VR_Clash_E2EPlace =
{
    localcmd(sprintf("vr_mock_hand main %f 1.2 -0.42 0 40 0\n", 0.18 - xAcross));
    localcmd("vr_mock_hand off -0.18 1.2 -0.42 0 -40 0\n");
};

// The two blades' least distance now (units).
float() VR_Clash_E2EGap =
{
    entity b0 = VR_Blow(cVR_MainHand);
    entity b1 = VR_Blow(cVR_OffHand);
    return VR_AG_SegDist(b0.mh_cp[0], b0.mh_cp[b0.mh_n - 1], b1.mh_cp[0], b1.mh_cp[b1.mh_n - 1]);
};

void() VR_Clash_E2EFrame =
{
    vr_clash_e2e_f = vr_clash_e2e_f + 1;
    // (Swords the player has no item for are put away for its own weapons after a while: kept in its hands.)
    float h;
    for(h = 0; h < 2; h++)
    {
        if(VRGetEntWeapon(self, h) != WID_SWORD)
        {
            VRGiveEntWeapon(self, h, WID_SWORD, 0, 0);
        }
    }
    if(vr_clash_e2e == 1 || vr_clash_e2e == 3)
    {
        float back = vr_clash_e2e == 3 ? max(0, VR_CLASH_E2E_DROP - vr_clash_e2e_f * VR_CLASH_E2E_SLOW) : 0;
        VR_Clash_E2EPlace(back);
        if(back > 0 || vr_clash_e2e_f < VR_CLASH_E2E_SETTLE)
        {
            return;
        }
        VR_Test_Check(VR_Blow(cVR_MainHand).mh_thing == VR_MTHING_SWORD && VR_Blow(cVR_OffHand).mh_thing == VR_MTHING_SWORD,
            "two swords held");
        VR_Test_Check(VR_Clash_E2EGap() > 4, sprintf("the blades apart before the swing (%.1f units)", VR_Clash_E2EGap()));
        vr_clash_e2e = vr_clash_e2e + 1;
        vr_clash_e2e_f = 0;
        vr_clash_e2e_n = vr_clash_n;
        vr_clash_e2e_s = vr_clash_nsparks;
        vr_clash_e2e_slow = vr_clash_slow;
        return;
    }
    float step = vr_clash_e2e == 4 ? VR_CLASH_E2E_STEP : VR_CLASH_E2E_SLOW;
    float across = min(VR_CLASH_E2E_DROP, vr_clash_e2e_f * step);
    VR_Clash_E2EPlace(across);
    if(across < VR_CLASH_E2E_DROP || vr_clash_e2e_f < VR_CLASH_E2E_DROP / step + 4)
    {
        return; // (and a few frames more for the last pose to arrive)
    }
    if(vr_clash_e2e == 2)
    {
        VR_Test_Check(vr_clash_n == vr_clash_e2e_n, sprintf("the slow swing (%.2f m/s) made none (%g)",
            VR_CLASH_E2E_SLOW * 72, vr_clash_n - vr_clash_e2e_n));
        VR_Test_Check(vr_clash_slow > vr_clash_e2e_slow, sprintf("... its meeting counted as too slow (the blades %.1f "
            "units apart now)", VR_Clash_E2EGap()));
        vr_clash_e2e = 3;
        vr_clash_e2e_f = 0;
        return;
    }
    VR_Test_Check(vr_clash_n == vr_clash_e2e_n + 1, sprintf("the fast swing (%.1f m/s) clashed once (%g)",
        VR_CLASH_E2E_STEP * 72, vr_clash_n - vr_clash_e2e_n));
    VR_Test_Check(vr_clash_nsparks > vr_clash_e2e_s && vr_clash_ntinks > 0, sprintf("... with sparks (%.0f) and a tink, at "
        "%v, %.2f m/s", vr_clash_sparked, vr_clash_x, vr_clash_spd));
    vr_clash_e2e = 0;
    VR_Test_Verdict();
};

void() VR_Clash_TestFrame =
{
    if(vr_clash_e2e)
    {
        entity was = self;
        self = VR_Player();
        if(self != world)
        {
            VR_Clash_E2EFrame();
        }
        self = was;
        return;
    }
    float ask = cvar("vr_clash_test");
    if(!ask)
    {
        return;
    }
    entity oldSelf = self;
    self = VR_Player();
    if(self == world || self.health <= 0)
    {
        self = oldSelf;
        return; // (asked before the map: run once the player is in it)
    }
    cvar_set("vr_clash_test", "0");
    if(ask == 1)
    {
        VR_Clash_TestAll();
    }
    if(ask == 2)
    {
        VR_Test_Begin("clashtest2");
        VRGiveEntWeapon(self, cVR_MainHand, WID_SWORD, 0, 0);
        VRGiveEntWeapon(self, cVR_OffHand, WID_SWORD, 0, 0);
        self.vr_clash_next = 0;
        vr_clash_e2e = 1;
        vr_clash_e2e_f = 0;
    }
    self = oldSelf;
};
#endif
