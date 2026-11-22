// vr_flatswap_test.qc -- flat-screen play's swap key (3) with every two-handed hold and with carried things (his words
// 2026-11-11: "All cases where a single weapon is held with two hands should follow those animation steps during a hand
// swap" and "The swap hands key should also seamlessly work when a prop is being held, or any combination of
// props/weapons"; the engine's vr_flatswap.cpp, QC vr_wpnholster.qc VR_FlatSwapHands). -flatscreenmode only.
//
// `vr_flatswap_test 1`: each weapon in the right hand alone, the left empty (the shotgun, the super shotgun, the nailgun,
//   the super nailgun, the grenade and rocket launchers, the thunderbolt, the axe, the chainsaw, the crowbar, AD's Plasma Gun): the
//   key (vr_mock_key 3) is drawn the two-handed way (release, pass, attach: vr_flat_swap_seen 2), and the weapon ends in
//   the left hand, the right empty.
// `vr_flatswap_test 2`: what each hand holds (right / left): shotgun+brick, brick+shotgun, brick+brick, brick+empty,
//   empty+brick, two shotguns, a brick in both hands: drawn the hands' meeting way (vr_flat_swap_seen 1; a brick in both:
//   0, nothing to change), and each hand ends with what the other held, a brick in its new hand (within 16 units of it).
// `vr_flatswap_test 3`: each melee weapon alone in the right hand (the axe, Mjolnir, the knight's sword, the crowbar, the
//   super axe, the chainsaw; with AD's data AD's six: the fury's, death knight's, zombie knight's and death guard's
//   swords, the hammer, the mace), the two-handed stance off then on (vr_flat_melee_2h): swapped to the left and back,
//   the engine watching each swap frame by frame (vr_flatswap.cpp probe, vr_flat_swap_vis_faults): the weapon drawn and
//   on the screen, both hands drawn, no hand dipping out of sight and back, nothing not a number. Until 2026-11-12 the
//   hands sank under the view in the attach (the axe, Mjolnir, the chainsaw: 2 to 3 frames past dipDepth each swap).
// `vr_flatswap_test 11..17`: a strip each (screenshots before the press and through the swap; the rocket launcher, the
//   super shotgun, the nailgun, the axe, the shotgun with a brick in the left hand, the grenade launcher, AD's plasma rifle), then the verdict line.
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
float VR_FSW_WEAPON_CASES = 11;
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
    if(c == 9) return WID_CROWBAR;
    // AD's Plasma Gun, given as the test aid gives it (VRGiveEntWeapon; without AD's data too, as vr_test_give_ad): a
    // gun already in a hand keeps going hand to hand (VRPassEntWeapon; it vanished in the swap until 2026-11-12).
    return WID_ADPLASMA;
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

// The strips' case: what the right hand holds (the left empty; 15: the shotgun, a brick in the left; 16 the grenade
// launcher, 17 AD's plasma rifle).
float(float ask) VR_FSW_StripWeapon =
{
    if(ask == 11) return WID_ROCKET_LAUNCHER;
    if(ask == 12) return WID_SUPER_SHOTGUN;
    if(ask == 13) return WID_NAILGUN;
    if(ask == 14) return WID_AXE;
    if(ask == 16) return WID_GRENADE_LAUNCHER;
    if(ask == 17) return WID_ADPLASMA;
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

// ---- 3: every melee weapon seen through the swap, both ways, the two-handed stance on and off.
void(entity p, float xHand, float xFlags) VR_ADMelee_Hand; // (vr_ad_melee.qc)

float VR_FSW_MELEE_CASES = 12; // (6 to 11: AD's, with AD's data only)
float vr_fsw_stance; // the case's vr_flat_melee_2h

// Test 3's weapon of case `c` (an AD kind's: WID_SWORD).
float(float c) VR_FSW_MeleeWeapon =
{
    if(c == 0) return WID_AXE;
    if(c == 1) return WID_MJOLNIR;
    if(c == 2) return WID_SWORD;
    if(c == 3) return WID_CROWBAR;
    if(c == 4) return WID_SUPERAXE;
    if(c == 5) return WID_CHAINSAW;
    return WID_SWORD;
};

// Test 3's AD kind of case `c` (0: none).
float(float c) VR_FSW_MeleeKind =
{
    if(c == 6) return VR_ADMELEE_FURY;
    if(c == 7) return VR_ADMELEE_DKNIGHT;
    if(c == 8) return VR_ADMELEE_ZKNIGHT;
    if(c == 9) return VR_ADMELEE_DGUARD;
    if(c == 10) return VR_ADMELEE_HAMMER;
    if(c == 11) return VR_ADMELEE_MACE;
    return 0;
};

// The key pressed, the engine's watch of the hands and the weapon started afresh (vr_flat_swap_vis_faults -1 until it
// ends: watchTail frames after the swap).
void() VR_FSW_PressWatched =
{
    cvar_set("vr_flat_swap_vis_faults", "-1");
    VR_FSW_Press();
};

// The watch's verdict, once it has ended (FALSE while it runs).
float(float w, string way) VR_FSW_Watched =
{
    float faults = cvar_hget(cvarh_vr_flat_swap_vis_faults);
    if(faults < 0)
    {
        return FALSE;
    }
    VR_Test_Check(faults == 0, sprintf("weapon %g (%s), stance %g, %s: the weapon and both hands seen every frame (%g faulty frames)",
        w, WeaponNameOf(w, 0), vr_fsw_stance, way, faults));
    if(faults != 0)
    {
        localcmd("vr_flatswap_vis_report\n");
    }
    return TRUE;
};

void(entity p, float first) VR_FSW_Melee =
{
    vr_fsw_case = max(vr_fsw_case, first);
    float w = VR_FSW_MeleeWeapon(vr_fsw_case);
    float kind = VR_FSW_MeleeKind(vr_fsw_case);
    if(vr_fsw_s == 0)
    {
        if(vr_fsw_case >= VR_FSW_MELEE_CASES || (kind && !cvar_hget(cvarh_vr_ad_mounted)))
        {
            if(kind)
            {
                bprint("flatswap_test: AD's data not mounted: AD's melee weapons not tried\n");
            }
            cvar_set("vr_flat_melee_2h", "1");
            VR_FSW_End();
            return;
        }
        cvar_set("vr_flat_melee_2h", ftos(vr_fsw_stance));
        VR_FSW_Give(p, cVR_OffHand, WID_FIST);
        if(kind)
        {
            VR_ADMelee_Ensure(kind);
            VR_ADMelee_Hand(p, cVR_MainHand, VR_ADMelee_Flags(kind));
        }
        else
        {
            // (As the console gives it, impulse 150 + its id: collected too. The weapon eased in from the hip holster
            // instead of the other hand, out of sight, is flatswap_test.sh's "holster" run.)
            localcmd(sprintf("wait\nimpulse %g\n", 150 + w));
        }
        VR_FSW_To(1, 72); // (a second: drawn, the stance taken)
        return;
    }
    if(vr_fsw_s == 1)
    {
        VR_FSW_PressWatched();
        VR_FSW_To(2, 0);
        return;
    }
    if(vr_fsw_s == 2)
    {
        if(!VR_FSW_Watched(w, "right to left"))
        {
            return;
        }
        VR_Test_Check(VRGetEntWeapon(p, cVR_OffHand) == w && VRGetEntWeapon(p, cVR_MainHand) == WID_FIST,
            sprintf("weapon %g, stance %g: now in the left hand", w, vr_fsw_stance));
        VR_FSW_PressWatched();
        VR_FSW_To(3, 0);
        return;
    }
    if(vr_fsw_s == 3)
    {
        if(!VR_FSW_Watched(w, "left to right"))
        {
            return;
        }
        VR_Test_Check(VRGetEntWeapon(p, cVR_MainHand) == w && VRGetEntWeapon(p, cVR_OffHand) == WID_FIST,
            sprintf("weapon %g, stance %g: back in the right hand", w, vr_fsw_stance));
        vr_fsw_stance = vr_fsw_stance ? 0 : 1; // (each weapon: the stance off, then on)
        if(!vr_fsw_stance)
        {
            vr_fsw_case = vr_fsw_case + 1;
        }
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

// ---- 11..17: a strip: a picture before the press, then every 5 frames through the swap (0.3 s and the way back), then
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

// ---- 5: a shot never paid for by a clip it hasn't got (his flat report 2026-11-13: "ERROR: weapon clip less than zero"
// firing the shotgun, the spent flare gun in the left hand). Each case: the empty shotgun in the right hand, the spent
// flare gun in the left, 20 shells in reserve; a sequence of the flat keys (as the game's impulses: the swap 48, the
// menu's drops 78, H 47, a holster's trades 60 + 3 i, R 55; case 8 the real swap key, drawn); then each hand holding the
// shotgun pulls its trigger: its clip down by one if it had a round, else a dry click (vr_clip_short, weapons.qc,
// unchanged), and each hand's ammo kind its weapon's (.currentammo, which W_CheckNoAmmo read).
// Cases 10 and 11 set up the two ways a shot got past W_CheckNoAmmo before 2026-11-13 (both "ERROR: weapon clip less
// than zero" then): 10, the right hand's .currentammo mirror left at AID_NONE (the fist's, the spent flare gun's) as a path
// that forgot W_SetCurrentAmmoFor would leave it; 11, a clip of half a round (0.5 passed `> 0`).
float VR_FSW_CLIP_CASES = 12;
float vr_fsw_hs; // case 4, 5: the empty holster used

string(float c) VR_FSW_ClipCaseName =
{
    if(c == 0) return "swap";
    if(c == 1) return "swap twice";
    if(c == 2) return "drop the left gun (menu)";
    if(c == 3) return "drop (H)";
    if(c == 4) return "left gun holstered and drawn";
    if(c == 5) return "right gun holstered and drawn";
    if(c == 6) return "reload, then swap";
    if(c == 7) return "reload, swap mid-reload";
    if(c == 8) return "swap (the key, drawn)";
    if(c == 9) return "swap, reload in the left, swap back";
    if(c == 10) return "the right hand's ammo kind left at none";
    return "half a round in the clip";
};

float(entity p) VR_FSW_ReloadBusy =
{
    return VR_FlatReload_Busy(p, cVR_MainHand) || VR_FlatReload_Busy(p, cVR_OffHand);
};

// The case's step `k` (1, 2 or 3): FALSE when it has none.
float(entity p, float c, float k) VR_FSW_ClipStep =
{
    if(c == 0 || c == 1) { if(k == 1 || (c == 1 && k == 2)) { p.impulse = 48; return TRUE; } return FALSE; }
    if(c == 2) { if(k == 1) { p.impulse = 78; return TRUE; } return FALSE; }
    if(c == 3) { if(k == 1) { p.impulse = 47; return TRUE; } return FALSE; }
    if(c == 4 || c == 5)
    {
        if(k == 1)
        {
            entity oself = self;
            self = p;
            vr_fsw_hs = -1;
            for(float i = 0; i < 6 && vr_fsw_hs < 0; i++)
            {
                if(getHolsterWeapon(VR_FlatHolster_Hotspot(i)) == WID_FIST) { vr_fsw_hs = i; }
            }
            self = oself;
        }
        if(k <= 2 && vr_fsw_hs >= 0) { p.impulse = 60 + 3 * vr_fsw_hs + (c == 4 ? 0 : 2); return TRUE; }
        return FALSE;
    }
    if(c == 6) { if(k == 1) { p.impulse = 55; return TRUE; } if(k == 2) { p.impulse = 48; return TRUE; } return FALSE; }
    if(c == 7) { if(k == 1) { p.impulse = 55; return TRUE; } if(k == 2) { p.impulse = 48; return TRUE; } return FALSE; }
    if(c == 8) { if(k == 1) { VR_FSW_Press(); return TRUE; } return FALSE; }
    if(c == 10) { if(k == 1) { p.currentammo = AID_NONE; return TRUE; } return FALSE; }
    if(c == 11) { if(k == 1) { WeaponInst_SetClip(p.weaponinst, 0.5); return TRUE; } return FALSE; }
    if(k == 1 || k == 3) { p.impulse = 48; return TRUE; }
    if(k == 2) { p.impulse = 55; return TRUE; }
    return FALSE;
};

void(entity p) VR_FSW_Clip =
{
    float c = vr_fsw_case;
    if(vr_fsw_s == 0)
    {
        if(c >= VR_FSW_CLIP_CASES)
        {
            VR_FSW_End();
            return;
        }
        entity oself = self;
        self = p;
        VRGiveEntWeapon(p, cVR_MainHand, WID_SHOTGUN, 0, 0);
        W_SetCurrentAmmoFor(cVR_MainHand);
        VRGiveEntWeapon(p, cVR_OffHand, WID_FLAREGUN, 0, 0);
        W_SetCurrentAmmoFor(cVR_OffHand);
        self = oself;
        p.ammo_shells = 20;
        VR_FSW_To(1, 20);
        return;
    }
    // Steps 1 to 3, each when the last has settled (no reload trip under way, but case 7's swap mid-trip).
    if(vr_fsw_s >= 1 && vr_fsw_s <= 3)
    {
        if(VR_FSW_ReloadBusy(p) && !(c == 7 && vr_fsw_s == 2))
        {
            return; // (the trip's end; FAIL "timed out" past VR_FSW_GIVEUP)
        }
        float k = vr_fsw_s;
        float acted = VR_FSW_ClipStep(p, c, k);
        float settle = 4;
        if(c == 8) { settle = 60; } // (the drawn swap: 0.3 s and its answer)
        if(c == 7 && k == 1) { settle = 8; } // (the trip under way)
        if(acted) { VR_FSW_To(vr_fsw_s + 1, settle); }
        else      { VR_FSW_To(4, 0); }
        return;
    }
    if(vr_fsw_s == 4)
    {
        if(VR_FSW_ReloadBusy(p))
        {
            return;
        }
        entity os = self;
        self = p;
        float fired = 0;
        for(float j = 0; j < 2; j++)
        {
            float h = j == 0 ? cVR_MainHand : cVR_OffHand;
            float w = VRGetEntWeapon(p, h);
            float want = WeaponIdToAmmoId(w, VRGetEntWeaponFlags(p, h));
            float mirror = h == cVR_MainHand ? p.currentammo : p.currentammo2;
            if(c != 10)
            {
                VR_Test_Check(mirror == want, sprintf("case %g (%s), %s hand: weapon %g's ammo kind %g (mirrored %g)", c,
                    VR_FSW_ClipCaseName(c), h == cVR_MainHand ? "right" : "left", w, want, mirror));
            }
            if(w != WID_SHOTGUN)
            {
                continue;
            }
            fired++;
            float c0 = VRGetEntWeaponClip(p, h);
            float short0 = vr_clip_short;
            p.attack_finished = 0;
            p.offhand_attack_finished = 0;
            if(h == cVR_MainHand) { W_Attack(); } else { W_OffHandAttack(); }
            float c1 = VRGetEntWeaponClip(p, h);
            float c1want = c0 >= 1 ? c0 - 1 : c0; // (a round fired, or a dry click, the clip as it was)
            VR_Test_Check(c1 == c1want && vr_clip_short == short0, sprintf("case %g (%s): the %s hand's shotgun fired with clip %g: now %g, %g shots unpaid",
                c, VR_FSW_ClipCaseName(c), h == cVR_MainHand ? "right" : "left", c0, c1, vr_clip_short - short0));
        }
        self = os;
        VR_Test_Check(fired > 0, sprintf("case %g (%s): a hand holds the shotgun (right %g, left %g)", c,
            VR_FSW_ClipCaseName(c), VRGetEntWeapon(p, cVR_MainHand), VRGetEntWeapon(p, cVR_OffHand)));
        VR_FSW_Give(p, cVR_MainHand, WID_FIST);
        VR_FSW_Give(p, cVR_OffHand, WID_FIST);
        vr_fsw_case = c + 1;
        VR_FSW_To(0, 30); // (the shot's animation out)
        return;
    }
};

// ---- 6: the holsters' number keys (his words 2026-11-22; QC vr_flatholster.qc VR_FlatHolster_Cycle / _Put, the engine's
// vr_flatplay.cpp handKeys), each pressed as a key (vr_mock_key): 1 and 2 cycle the left and the right hand through three
// holstered weapons and the empty hand (the other hand's weapon skipped), 5 with an empty hand, 4 back into the holster
// it came from and into the left side's first free one, 4 with every holster full, 1 with every holster full (a trade),
// 6 and 7 drop, 1 with a brick carried; then the rocket launcher in the right hand: the left on its grip, off it with
// 1's shotgun, back on it when 1 empties it again.
float VR_FNK_STEPS = 16;
float vr_fnk_base;  // impulses 86 .. 89 taken and hand drops before the press
float vr_fnk_wl;    // the step's left hand's weapon, expected; and the right's
float vr_fnk_wr;
float vr_fnk_e0;    // the holsters 0 .. 5, expected
float vr_fnk_e1;
float vr_fnk_e2;
float vr_fnk_e3;
float vr_fnk_e4;
float vr_fnk_e5;
string vr_fnk_what; // (a constant)

float(entity p, float i) VR_FNK_Hol =
{
    entity os = self;
    self = p;
    float w = getHolsterWeapon(VR_FlatHolster_Hotspot(i));
    self = os;
    return w;
};

void(entity p, float i, float wid) VR_FNK_SetOne =
{
    entity os = self;
    self = p;
    entity inst = world;
    if(wid != WID_FIST)
    {
        VR_WeaponUtil_EntAddWeapon(p, wid);
        inst = WeaponInst_New(WeaponIdToBaseClipSize(wid));
    }
    VR_FlatHolster_Set(VR_FlatHolster_Hotspot(i), wid, 0, inst);
    self = os;
};

void(entity p, float a, float b, float c, float d, float e, float f) VR_FNK_SetHolsters =
{
    VR_FNK_SetOne(p, 0, a);
    VR_FNK_SetOne(p, 1, b);
    VR_FNK_SetOne(p, 2, c);
    VR_FNK_SetOne(p, 3, d);
    VR_FNK_SetOne(p, 4, e);
    VR_FNK_SetOne(p, 5, f);
};

// What the step should leave: the left hand, the right, the six holsters.
void(float l, float r, float a, float b, float c, float d, float e, float f, string what) VR_FNK_Expect =
{
    vr_fnk_wl = l;
    vr_fnk_wr = r;
    vr_fnk_e0 = a;
    vr_fnk_e1 = b;
    vr_fnk_e2 = c;
    vr_fnk_e3 = d;
    vr_fnk_e4 = e;
    vr_fnk_e5 = f;
    vr_fnk_what = what;
};

void(string key) VR_FNK_Press =
{
    vr_fnk_base = vr_flatholster_keys + vr_flatholster_drops;
    localcmd(sprintf("vr_mock_key %s down\nwait\nwait\nvr_mock_key %s up\n", key, key));
};

float(entity p) VR_FNK_Matches =
{
    return VRGetEntWeapon(p, cVR_OffHand) == vr_fnk_wl && VRGetEntWeapon(p, cVR_MainHand) == vr_fnk_wr &&
           VR_FNK_Hol(p, 0) == vr_fnk_e0 && VR_FNK_Hol(p, 1) == vr_fnk_e1 && VR_FNK_Hol(p, 2) == vr_fnk_e2 &&
           VR_FNK_Hol(p, 3) == vr_fnk_e3 && VR_FNK_Hol(p, 4) == vr_fnk_e4 && VR_FNK_Hol(p, 5) == vr_fnk_e5;
};

// Step `k`'s set-up, its key and what it should leave.
void(entity p, float k) VR_FNK_Step =
{
    float S = WID_SHOTGUN;
    float N = WID_NAILGUN;
    float X = WID_SUPER_SHOTGUN;
    float F = WID_FIST;
    float T = WID_LIGHTNING;
    if(k == 0)
    {
        VR_FSW_Give(p, cVR_MainHand, F);
        VR_FSW_Give(p, cVR_OffHand, F);
        VR_FNK_SetHolsters(p, S, N, F, X, F, F);
        VR_FNK_Expect(S, F, F, N, F, X, F, F, "1: the left hand draws the first holster's shotgun");
        VR_FNK_Press("1");
    }
    if(k == 1) { VR_FNK_Expect(N, F, S, F, F, X, F, F, "1: the nailgun, the shotgun back in its own holster"); VR_FNK_Press("1"); }
    if(k == 2) { VR_FNK_Expect(X, F, S, N, F, F, F, F, "1: the super shotgun (holster 3, the empty 2 passed)"); VR_FNK_Press("1"); }
    if(k == 3) { VR_FNK_Expect(F, F, S, N, F, X, F, F, "1 after the last: the left hand empty, every weapon in its holster"); VR_FNK_Press("1"); }
    if(k == 4) { VR_FNK_Expect(F, S, F, N, F, X, F, F, "2: the right hand draws the shotgun"); VR_FNK_Press("2"); }
    if(k == 5) { VR_FNK_Expect(N, S, F, F, F, X, F, F, "1: the nailgun (the right hand's shotgun skipped)"); VR_FNK_Press("1"); }
    if(k == 6) { VR_FNK_Expect(N, X, S, F, F, F, F, F, "2: the super shotgun, the shotgun back"); VR_FNK_Press("2"); }
    if(k == 7) { VR_FNK_Expect(N, F, S, F, F, X, F, F, "2 after the last: the right hand empty"); VR_FNK_Press("2"); }
    if(k == 8) { VR_FNK_Expect(N, F, S, F, F, X, F, F, "5 with the right hand empty: nothing moves"); VR_FNK_Press("5"); }
    if(k == 9) { VR_FNK_Expect(F, F, S, N, F, X, F, F, "4: the nailgun into the holster it came from"); VR_FNK_Press("4"); }
    if(k == 10)
    {
        VR_FSW_Give(p, cVR_OffHand, T);
        VR_FNK_Expect(F, F, S, N, T, X, F, F, "4: a thunderbolt from elsewhere into the left hip (the left side's first free)");
        VR_FNK_Press("4");
    }
    if(k == 11)
    {
        VR_FNK_SetHolsters(p, S, N, T, X, WID_GRENADE_LAUNCHER, WID_ROCKET_LAUNCHER);
        VR_FSW_Give(p, cVR_OffHand, WID_AXE);
        VR_FNK_Expect(WID_AXE, F, S, N, T, X, WID_GRENADE_LAUNCHER, WID_ROCKET_LAUNCHER, "4 with every holster full: nothing moves");
        VR_FNK_Press("4");
    }
    if(k == 12)
    {
        VR_FNK_Expect(S, F, WID_AXE, N, T, X, WID_GRENADE_LAUNCHER, WID_ROCKET_LAUNCHER, "1 with every holster full: the axe and the shotgun trade");
        VR_FNK_Press("1");
    }
    if(k == 13)
    {
        VR_FNK_Expect(F, F, WID_AXE, N, T, X, WID_GRENADE_LAUNCHER, WID_ROCKET_LAUNCHER, "6: the left hand's shotgun dropped");
        VR_FNK_Press("6");
    }
    if(k == 14)
    {
        VR_FSW_Give(p, cVR_MainHand, S);
        VR_FNK_Expect(F, F, WID_AXE, N, T, X, WID_GRENADE_LAUNCHER, WID_ROCKET_LAUNCHER, "7: the right hand's shotgun dropped");
        VR_FNK_Press("7");
    }
    if(k == 15)
    {
        VR_FNK_SetHolsters(p, S, F, F, F, F, F);
        vr_fsw_bo = VR_FSW_Brick(p, cVR_OffHand);
        VR_FNK_Expect(F, F, S, F, F, F, F, F, "1 with a brick in the left hand: nothing moves, the brick kept");
        VR_FNK_Press("1");
    }
};

void(entity p) VR_FSW_NumKeys =
{
    if(vr_fsw_s == 0)
    {
        if(vr_fsw_case >= VR_FNK_STEPS)
        {
            VR_FSW_To(10, 0);
            return;
        }
        VR_FNK_Step(p, vr_fsw_case);
        VR_FSW_To(1, 0);
        return;
    }
    if(vr_fsw_s == 1)
    {
        if(vr_flatholster_keys + vr_flatholster_drops == vr_fnk_base)
        {
            return; // (the key's impulse not yet taken: the state's give-up catches one never taken)
        }
        string hols = sprintf("%g %g %g %g %g %g", VR_FNK_Hol(p, 0), VR_FNK_Hol(p, 1), VR_FNK_Hol(p, 2), VR_FNK_Hol(p, 3),
            VR_FNK_Hol(p, 4), VR_FNK_Hol(p, 5));
        VR_Test_Check(VR_FNK_Matches(p), sprintf("step %g, %s (left %g, right %g; holsters %s)", vr_fsw_case, vr_fnk_what,
            VRGetEntWeapon(p, cVR_OffHand), VRGetEntWeapon(p, cVR_MainHand), hols));
        if(vr_fsw_case == 15)
        {
            VR_Test_Check(VR_Carry_Held(p, cVR_OffHand) == vr_fsw_bo, "step 15: the brick still in the left hand");
            VR_FSW_DropAll(p);
        }
        vr_fsw_case = vr_fsw_case + 1;
        VR_FSW_To(0, 2);
        return;
    }
    // The two-handed weapon: the rocket launcher alone in the right hand, the left on its grip (flat play's pinToGrip).
    if(vr_fsw_s == 10)
    {
        VR_FSW_Give(p, cVR_OffHand, WID_FIST);
        VR_FSW_Give(p, cVR_MainHand, WID_ROCKET_LAUNCHER);
        VR_FNK_SetHolsters(p, WID_SHOTGUN, WID_FIST, WID_FIST, WID_FIST, WID_FIST, WID_FIST);
        VR_FSW_To(11, 0);
        return;
    }
    if(vr_fsw_s == 11)
    {
        if(!VRGet2HAiming(p))
        {
            return; // (until the left hand takes the grip: the give-up if never)
        }
        VR_Test_Check(TRUE, "2H: the rocket launcher in the right hand, the empty left on its grip");
        VR_FNK_Expect(WID_SHOTGUN, WID_ROCKET_LAUNCHER, WID_FIST, WID_FIST, WID_FIST, WID_FIST, WID_FIST, WID_FIST,
            "2H, 1: the left hand draws the shotgun, the rocket launcher kept in the right");
        VR_FNK_Press("1");
        VR_FSW_To(12, 0);
        return;
    }
    if(vr_fsw_s == 12 || vr_fsw_s == 14)
    {
        if(vr_flatholster_keys + vr_flatholster_drops == vr_fnk_base)
        {
            return;
        }
        VR_Test_Check(VR_FNK_Matches(p), sprintf("%s (left %g, right %g, holster 0 %g)", vr_fnk_what,
            VRGetEntWeapon(p, cVR_OffHand), VRGetEntWeapon(p, cVR_MainHand), VR_FNK_Hol(p, 0)));
        VR_FSW_To(vr_fsw_s + 1, 0);
        return;
    }
    if(vr_fsw_s == 13)
    {
        if(VRGet2HAiming(p))
        {
            return; // (until the left hand, holding the shotgun, is off the grip)
        }
        VR_Test_Check(TRUE, "2H: the left hand off the grip with its shotgun");
        VR_FNK_Expect(WID_FIST, WID_ROCKET_LAUNCHER, WID_SHOTGUN, WID_FIST, WID_FIST, WID_FIST, WID_FIST, WID_FIST,
            "2H, 1 again: the shotgun back in its holster, the left hand empty");
        VR_FNK_Press("1");
        VR_FSW_To(14, 0);
        return;
    }
    if(vr_fsw_s == 15)
    {
        if(!VRGet2HAiming(p))
        {
            return; // (until the empty left hand takes the grip again)
        }
        VR_Test_Check(TRUE, "2H: the empty left hand back on the rocket launcher's grip");
        VR_FSW_Give(p, cVR_MainHand, WID_FIST);
        VR_FNK_SetHolsters(p, WID_FIST, WID_FIST, WID_FIST, WID_FIST, WID_FIST, WID_FIST);
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
        vr_fsw_stance = 0;
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
    if(ask == 3) { VR_FSW_Melee(p, 0); }
    if(ask == 4) { VR_FSW_Melee(p, 6); } // (AD's only)
    if(ask == 5) { VR_FSW_Clip(p); }
    if(ask == 6) { VR_FSW_NumKeys(p); } // (the holsters' number keys)
    if(ask >= 11 && ask <= 17) { VR_FSW_Strip(p, ask); }
};
#endif
