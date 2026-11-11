// vr_flatswap_test.qc -- flat-screen play's swap key (3) with every two-handed hold and with carried things (his words
// 2026-11-11: "All cases where a single weapon is held with two hands should follow those animation steps during a hand
// swap" and "The swap hands key should also seamlessly work when a prop is being held, or any combination of
// props/weapons"; the engine's vr_flatswap.cpp, QC vr_wpnholster.qc VR_FlatSwapHands). -flatscreenmode only.
//
// `vr_flatswap_test 1`: each weapon in the right hand alone, the left empty (the shotgun, the super shotgun, the nailgun,
//   the super nailgun, the grenade and rocket launchers, the thunderbolt, the axe, the chainsaw, the crowbar): the
//   key (vr_mock_key 3) is drawn the two-handed way (release, pass, attach: vr_flat_swap_seen 2), and the weapon ends in
//   the left hand, the right empty.
// `vr_flatswap_test 2`: what each hand holds (right / left): shotgun+brick, brick+shotgun, brick+brick, brick+empty,
//   empty+brick, two shotguns, a brick in both hands: drawn the hands' meeting way (vr_flat_swap_seen 1; a brick in both:
//   0, nothing to change), and each hand ends with what the other held, a brick in its new hand (within 16 units of it).
// `vr_flatswap_test 11..15`: a strip each (screenshots before the press and through the swap; the rocket launcher, the
//   super shotgun, the nailgun, the axe, the shotgun with a brick in the left hand), then the verdict line.
// Each ends on its "flatswap_test: N passed, M failed" line (Misc/quakevr/flatswap_test.sh).
#ifdef QVR_TESTS

float vr_fsw_on;    // the running test's number
float vr_fsw_s;     // its state
float vr_fsw_case;  // which case
float vr_fsw_t0;    // when the state began (a stuck test: FAIL "timed out")
float vr_fsw_wait;  // frames left before the state goes on
float vr_fsw_shot;  // (the strips) screenshots taken
entity vr_fsw_bm;   // the brick the case gave the right hand
entity vr_fsw_bo;   // and the left

float VR_FSW_GIVEUP = 8; // seconds a state may last
float VR_FSW_WEAPON_CASES = 10;
float VR_FSW_THING_CASES = 7;

void(float s, float frames) VR_FSW_To =
{
    vr_fsw_s = s;
    vr_fsw_wait = frames;
    vr_fsw_t0 = time;
};

// Test 1's weapon of case `c`.
float(float c) VR_FSW_Weapon =
{
    if(c == 0) return WID_SHOTGUN;
    if(c == 1) return WID_SUPER_SHOTGUN;
    if(c == 2) return WID_NAILGUN;
    if(c == 3) return WID_SUPER_NAILGUN;
    if(c == 4) return WID_GRENADE_LAUNCHER;
    if(c == 5) return WID_ROCKET_LAUNCHER;
    if(c == 6) return WID_LIGHTNING;
    if(c == 7) return WID_AXE;
    if(c == 8) return WID_CHAINSAW;
    return WID_CROWBAR;
};

// Test 2's case `c` in hand `h`: 0 nothing, 1 a shotgun, 2 a brick, 3 the one brick in both hands.
float(float c, float h) VR_FSW_Holds =
{
    if(c == 0) return h == cVR_MainHand ? 1 : 2;
    if(c == 1) return h == cVR_MainHand ? 2 : 1;
    if(c == 2) return 2;
    if(c == 3) return h == cVR_MainHand ? 2 : 0;
    if(c == 4) return h == cVR_MainHand ? 0 : 2;
    if(c == 5) return 1;
    return 3;
};

string(float x) VR_FSW_HoldName =
{
    return x == 0 ? "empty" : x == 1 ? "shotgun" : x == 2 ? "brick" : "the brick in both";
};

// The strips' case: what the right hand holds (the left empty; 15: the shotgun, a brick in the left).
float(float ask) VR_FSW_StripWeapon =
{
    if(ask == 11) return WID_ROCKET_LAUNCHER;
    if(ask == 12) return WID_SUPER_SHOTGUN;
    if(ask == 13) return WID_NAILGUN;
    if(ask == 14) return WID_AXE;
    return WID_SHOTGUN;
};

entity(entity p, float xHand) VR_FSW_Brick =
{
    entity oldself = self;
    entity b = spawn();
    setorigin(b, VRGetEntHandPos(p, xHand));
    b.model = "progs/vr_brick1.mdl";
    self = b;
    vr_debris_piece();
    self = p;
    VR_Carry_Start(p, xHand, b);
    self = oldself;
    return b;
};

void(entity p, float xHand, float xWid) VR_FSW_Give =
{
    entity oldself = self;
    self = p;
    VRGiveEntWeapon(p, xHand, xWid, 0, xWid == WID_FIST ? 0 : WeaponIdToBaseClipSize(xWid));
    W_SetCurrentAmmoFor(xHand);
    self = oldself;
};

// The case's hands: weapons, then the bricks.
void(entity p, float m, float o) VR_FSW_SetHands =
{
    VR_FSW_Give(p, cVR_MainHand, m == 1 ? WID_SHOTGUN : WID_FIST);
    VR_FSW_Give(p, cVR_OffHand, o == 1 ? WID_SHOTGUN : WID_FIST);
    p.ammo_shells = 50;
    vr_fsw_bm = world;
    vr_fsw_bo = world;
    if(m == 2 || m == 3)
    {
        vr_fsw_bm = VR_FSW_Brick(p, cVR_MainHand);
    }
    if(o == 2)
    {
        vr_fsw_bo = VR_FSW_Brick(p, cVR_OffHand);
    }
    if(m == 3)
    {
        // (The second hand on it, as VR_Carry_TrySecondHand takes it.)
        VR_Hand_Take(p, cVR_OffHand, vr_fsw_bm);
        vr_fsw_bm.carry_2h = TRUE;
        carry2h(vr_fsw_bm, p, TRUE);
    }
};

void(entity p) VR_FSW_DropAll =
{
    entity b;
    float i;
    for(i = 0; i < 2; i++)
    {
        b = i == 0 ? vr_fsw_bm : vr_fsw_bo;
        if(b == world)
        {
            continue;
        }
        if(VR_Carry_Held(p, cVR_MainHand) == b)
        {
            VR_Hand_Release(p, cVR_MainHand, "test");
        }
        if(VR_Carry_Held(p, cVR_OffHand) == b)
        {
            VR_Hand_Release(p, cVR_OffHand, "test");
        }
        b.carry_2h = FALSE;
        VR_Carry_Loose(b);
        remove(b);
    }
    vr_fsw_bm = world;
    vr_fsw_bo = world;
    VR_FSW_Give(p, cVR_MainHand, WID_FIST);
    VR_FSW_Give(p, cVR_OffHand, WID_FIST);
};

void() VR_FSW_Press =
{
    cvar_set("vr_flat_swap_seen", "-1");
    localcmd("vr_mock_key 3 down\nwait\nwait\nvr_mock_key 3 up\n");
};

void() VR_FSW_End =
{
    VR_Test_Verdict();
    vr_fsw_on = 0;
    cvar_set("vr_flatswap_test", "0");
};

// ---- 1: every weapon held alone in the right hand.
void(entity p) VR_FSW_Weapons =
{
    float w = VR_FSW_Weapon(vr_fsw_case);
    if(vr_fsw_s == 0)
    {
        if(vr_fsw_case >= VR_FSW_WEAPON_CASES)
        {
            VR_FSW_End();
            return;
        }
        VR_FSW_Give(p, cVR_MainHand, w);
        VR_FSW_Give(p, cVR_OffHand, WID_FIST);
        VR_FSW_To(1, 72); // (a second: drawn, the free hand on its grip or beside it)
        return;
    }
    if(vr_fsw_s == 1)
    {
        VR_FSW_Press();
        VR_FSW_To(2, 6);
        return;
    }
    if(vr_fsw_s == 2)
    {
        float seen = cvar_hget(cvarh_vr_flat_swap_seen);
        VR_Test_Check(seen == 2, sprintf("weapon %g (%s) alone in the right hand: the two-handed swap (release, pass, attach; seen %g)",
            w, WeaponNameOf(w, 0), seen));
        VR_FSW_To(3, 60); // (0.1 + 0.1 + 0.1 s and the answer: well inside 60 frames)
        return;
    }
    if(vr_fsw_s == 3)
    {
        float om = VRGetEntWeapon(p, cVR_MainHand);
        float oo = VRGetEntWeapon(p, cVR_OffHand);
        VR_Test_Check(oo == w && om == WID_FIST, sprintf("weapon %g: now in the left hand (left %g, right %g)", w, oo, om));
        VR_FSW_Give(p, cVR_OffHand, WID_FIST);
        vr_fsw_case = vr_fsw_case + 1;
        VR_FSW_To(0, 4);
        return;
    }
};

// ---- 2: carried things with weapons and each other.
void(entity p) VR_FSW_Things =
{
    float m = VR_FSW_Holds(vr_fsw_case, cVR_MainHand);
    float o = VR_FSW_Holds(vr_fsw_case, cVR_OffHand);
    string what = sprintf("%s right, %s left", VR_FSW_HoldName(m), VR_FSW_HoldName(o));
    if(vr_fsw_s == 0)
    {
        if(vr_fsw_case >= VR_FSW_THING_CASES)
        {
            VR_FSW_End();
            return;
        }
        VR_FSW_SetHands(p, m, o);
        VR_FSW_To(1, 36); // (half a second: the hands' stats, the bricks drawn in them)
        return;
    }
    if(vr_fsw_s == 1)
    {
        VR_FSW_Press();
        VR_FSW_To(2, 6);
        return;
    }
    if(vr_fsw_s == 2)
    {
        float seen = cvar_hget(cvarh_vr_flat_swap_seen);
        float want = m == 3 ? 0 : 1;
        VR_Test_Check(seen == want, sprintf("%s: %s (seen %g)", what, want ? "the hands meet" : "nothing to change, nothing drawn", seen));
        VR_FSW_To(3, 60);
        return;
    }
    if(vr_fsw_s == 3)
    {
        float wm = VRGetEntWeapon(p, cVR_MainHand) == WID_SHOTGUN;
        float wo = VRGetEntWeapon(p, cVR_OffHand) == WID_SHOTGUN;
        entity tm = VR_Carry_Held(p, cVR_MainHand);
        entity to = VR_Carry_Held(p, cVR_OffHand);
        if(m == 3)
        {
            VR_Test_Check(tm == vr_fsw_bm && to == vr_fsw_bm, sprintf("%s: still in both hands", what));
        }
        else
        {
            // Each hand now holds what the other held.
            entity wantM = o == 2 ? vr_fsw_bo : world;
            entity wantO = m == 2 ? vr_fsw_bm : world;
            VR_Test_Check(wm == (o == 1) && wo == (m == 1), sprintf("%s: the shotguns changed hands (right %g, left %g)", what, wm, wo));
            VR_Test_Check(tm == wantM && to == wantO, sprintf("%s: the bricks changed hands (right %s, left %s)", what,
                tm.classname, to.classname));
            float h;
            for(h = 0; h < 2; h++)
            {
                float hand = h == 0 ? cVR_OffHand : cVR_MainHand;
                entity b = VR_Carry_Held(p, hand);
                if(b == world)
                {
                    continue;
                }
                float d = vlen(b.origin - VRGetEntHandPos(p, hand));
                VR_Test_Check(d < 16, sprintf("%s: the brick in the %s hand, %.1f units off it", what, hand == cVR_MainHand ? "right" : "left", d));
            }
        }
        VR_FSW_DropAll(p);
        vr_fsw_case = vr_fsw_case + 1;
        VR_FSW_To(0, 4);
        return;
    }
};

// ---- 11..15: a strip: a picture before the press, then every 5 frames through the swap (0.3 s and the way back), then
// one at rest.
void(entity p, float ask) VR_FSW_Strip =
{
    if(vr_fsw_s == 0)
    {
        VR_FSW_Give(p, cVR_MainHand, VR_FSW_StripWeapon(ask));
        VR_FSW_Give(p, cVR_OffHand, WID_FIST);
        vr_fsw_bm = world;
        vr_fsw_bo = world;
        if(ask == 15)
        {
            vr_fsw_bo = VR_FSW_Brick(p, cVR_OffHand);
        }
        vr_fsw_shot = 0;
        VR_FSW_To(1, 72);
        return;
    }
    if(vr_fsw_s == 1)
    {
        localcmd("screenshot\n");
        VR_FSW_Press();
        VR_FSW_To(2, 3);
        return;
    }
    if(vr_fsw_s == 2)
    {
        localcmd("screenshot\n");
        vr_fsw_shot = vr_fsw_shot + 1;
        VR_FSW_To(vr_fsw_shot >= 6 ? 3 : 2, vr_fsw_shot >= 6 ? 36 : 5);
        return;
    }
    if(vr_fsw_s == 3)
    {
        localcmd("screenshot\n");
        VR_Test_Check(VRGetEntWeapon(p, cVR_OffHand) == VR_FSW_StripWeapon(ask), "the weapon in the left hand at the strip's end");
        if(ask == 15)
        {
            VR_Test_Check(VR_Carry_Held(p, cVR_MainHand) == vr_fsw_bo, "the brick in the right hand at the strip's end");
        }
        VR_FSW_To(4, 4);
        return;
    }
    if(vr_fsw_s == 4)
    {
        VR_FSW_End();
        return;
    }
};

void() VR_FlatSwapTest_Frame =
{
    float ask = cvar_hget(cvarh_vr_flatswap_test);
    if(!ask)
    {
        return;
    }
    entity p = VR_Player();
    if(p == world)
    {
        return;
    }
    if(vr_fsw_on != ask)
    {
        VR_Test_Begin("flatswap_test");
        vr_fsw_on = ask;
        vr_fsw_case = 0;
        VR_FSW_To(0, 0);
        if(!VR_FlatPlay(p))
        {
            bprint("flatswap_test: flat play off: run with -flatscreenmode\n");
        }
    }
    if(vr_fsw_wait > 0)
    {
        vr_fsw_wait = vr_fsw_wait - 1;
        return;
    }
    if(time - vr_fsw_t0 > VR_FSW_GIVEUP && vr_fsw_s != 0)
    {
        VR_Test_Check(FALSE, sprintf("timed out: test %g, case %g, state %g", ask, vr_fsw_case, vr_fsw_s));
        VR_FSW_End();
        return;
    }
    if(ask == 1) { VR_FSW_Weapons(p); }
    if(ask == 2) { VR_FSW_Things(p); }
    if(ask >= 11 && ask <= 15) { VR_FSW_Strip(p, ask); }
};
#endif
