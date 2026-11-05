#ifdef QVR_TESTS
//
//
//
// ----------------------------------------------------------------------------
// Magazines sliding home and out (vr_magslide_test 1; Debug > Tests > Nail Pistol; vr_reload.qc VR_Reload_MagOut*,
// vr_view.cpp MagSlide, ROUND24.md)
// ----------------------------------------------------------------------------
//
// Headless, fast mode, a definite end: every check prints "magslide: PASS|FAIL <what>", the run ends on
// "magslide: N passed, M failed". The engine measures the drawn magazine in the gun's frame each frame (vr_test_magslide*).
// A state machine on the game's frames, each step waiting on its own condition; the gun turns all through the slides.
//   The nail pistol in the main hand, then in the off hand (drawn mirrored):
//     1-3 its eject button: out of the count at once, slides out along the grip's line, goes free at its end, popping out
//     4-5 a new magazine: loaded at once, slides home along the line
//     6-10 the button with the hand moving: the hand's velocity inherited, outward still dominant; seated again;
//          then the hand's share capped: at any hand speed, either way, it pops mostly outward
//   11, 21-23 interrupted: the gun leaves the hand mid-slide; the magazine goes free, nothing drawn sliding
//   12, 31-33 interrupted: the player dead mid-slide
//   13, 41-43 interrupted: a new magazine seated mid-slide (the old goes free first); the gun leaving mid-seat
//   15-17 the nailgun: a short seat; its eject button as ever (free at once, no slide)
//   18-20 vr_nailpistol_mag_eject_time and _seat_time 0: at once, no slide

float vr_mst_step;      // 0 idle
float vr_mst_frames;    // frames into the step
float vr_mst_turn;      // frames the gun has been turning
float vr_mst_hand;      // the hand with the gun
float vr_mst_mode0;     // vr_reload_mode_npistol before
float vr_mst_mode1;     // vr_reload_mode_nail before
float vr_mst_grip;      // vr_weapon_grip_mode before
float vr_mst_seen;      // the slide's been seen drawn
float vr_mst_eject0;    // vr_nailpistol_mag_eject_time before
float vr_mst_seat0;     // vr_nailpistol_mag_seat_time before
entity vr_mst_player;
entity vr_mst_r0;       // the last loose one known

string(float xHand) VR_MST_Name =
{
    return xHand == cVR_MainHand ? "main" : "off";
};

// The other hand at its side, the gun's held out, turned this far through (`xTurn` frames in), a turn on all its axes.
void(float xTurn) VR_MST_Pose =
{
    float side = vr_mst_hand == cVR_MainHand ? 0.25 : -0.25;
    localcmd(sprintf("vr_mock_hand %s %g 1.3 -0.35 %g %g %g\n", VR_MST_Name(vr_mst_hand), side, 70 + 20 * sin(xTurn * 0.15),
        xTurn * 3, xTurn * 4));
};

// A gun in the hand under test, magazine in, full; the other hand empty.
void(float xWid, float xClip) VR_MST_Give =
{
    entity p = vr_mst_player;
    entity os = self;
    self = p;
    VR_WeaponUtil_EntAddWeapon(p, xWid);
    VRGiveEntWeapon(p, vr_mst_hand, xWid, 0, xClip);
    W_SetCurrentAmmoFor(vr_mst_hand);
    if(VRGetEntWeapon(p, VRGetOtherHand(vr_mst_hand)) != WID_FIST)
    {
        VRGiveEntWeapon(p, VRGetOtherHand(vr_mst_hand), WID_FIST, 0, 0);
    }
    p.ammo_nails = 200;
    self = os;
};

// A new magazine out of the pouch into the other hand, its top to the well: seated.
void() VR_MST_Seat =
{
    entity p = vr_mst_player;
    entity os = self;
    self = p;
    float other = VRGetOtherHand(vr_mst_hand);
    VR_Reload_Take(other);
    entity r = VR_Carry_Held(p, other);
    if(r)
    {
        VR_Reload_MagAtWell(other, r);
    }
    self = os;
};

void() VR_MST_Next = { vr_mst_step++; vr_mst_frames = 0; vr_mst_seen = 0; };

void(string xPress) VR_MST_Button =
{
    localcmd(sprintf("vr_mock_button %s secondary %s\n", VR_MST_Name(vr_mst_hand), xPress));
};

// The drawn slide just over: on the line every frame, from `xFirst` to `xLast` of its travel, at most `xStepMost` of it a
// frame.
void(string xWhat, float xFirst, float xLast, float xStepMost) VR_MST_CheckSlide =
{
    float full = cvar_hget(cvarh_vr_test_magslide_full);
    float off = cvar_hget(cvarh_vr_test_magslide_off);
    float n = cvar_hget(cvarh_vr_test_magslide_frames);
    float first = cvar_hget(cvarh_vr_test_magslide_first);
    float last = cvar_hget(cvarh_vr_test_magslide_last);
    float step = cvar_hget(cvarh_vr_test_magslide_step);
    VR_Test_Check(n >= 4 && full > 0 && off < 0.02, sprintf("%s, hand %s: on the well's line each of %g frames, the gun turning (%.3f units off at most: want < 0.02)",
        xWhat, VR_MST_Name(vr_mst_hand), n, off));
    VR_Test_Check(fabs(first - xFirst * full) < 0.03 * full && fabs(last - xLast * full) < 0.03 * full, sprintf(
        "%s: from %.2f to %.2f along the line (want %.2f to %.2f of %.2f units)", xWhat, first, last, xFirst * full, xLast * full, full));
    VR_Test_Check(step <= xStepMost * full, sprintf("%s: no jump: at most %.3f units a frame (want <= %.3f)", xWhat, step, xStepMost * full));
};

void() VR_MST_End =
{
    cvar_set("vr_reload_mode_npistol", ftos(vr_mst_mode0));
    cvar_set("vr_reload_mode_nail", ftos(vr_mst_mode1));
    cvar_set("vr_weapon_grip_mode", ftos(vr_mst_grip));
    cvar_set("vr_test_magslide", "0");
    VR_Test_Verdict();
    vr_mst_step = 0;
};

void() VR_MagSlide_TestFrame =
{
    float req = cvar_hget(cvarh_vr_magslide_test);
    if(req)
    {
        cvar_set("vr_magslide_test", "0");
        vr_mst_player = VR_Player();
        if(!vr_mst_player) { dprint("magslide: no player\n"); return; }
        if(req == 2)
        {
            vr_mst_hand = cVR_MainHand;
            VR_MST_Seat();
            return;
        }
        VR_Test_Begin("magslide");
        vr_mst_mode0 = cvar_hget(cvarh_vr_reload_mode_npistol);
        vr_mst_mode1 = cvar_hget(cvarh_vr_reload_mode_nail);
        vr_mst_grip = cvar_hget(cvarh_vr_weapon_grip_mode);
        vr_mst_eject0 = cvar_hget(cvarh_vr_nailpistol_mag_eject_time);
        vr_mst_seat0 = cvar_hget(cvarh_vr_nailpistol_mag_seat_time);
        cvar_set("vr_reload_mode_npistol", "3");
        cvar_set("vr_reload_mode_nail", "3");
        cvar_set("vr_weapon_grip_mode", "1");
        vr_mst_hand = cVR_MainHand;
        vr_mst_step = 1;
        vr_mst_frames = 0;
        vr_mst_seen = 0;
        vr_mst_turn = 0;
        vr_mst_r0 = vr_reload_out_r;
    }
    if(!vr_mst_step) return;
    entity p = vr_mst_player;
    float bit = vr_mst_hand == cVR_MainHand ? 2 : 1;
    float s = vr_mst_step;
    float f = ++vr_mst_frames;
    if(f > 72 * 5) // (a safety net only: a stuck step)
    {
        VR_Test_Check(FALSE, sprintf("step %g timed out (hand %s: magazine in %g, clip %g, out bits %g, client mode %g)", s, VR_MST_Name(vr_mst_hand), VR_Reload_MagIn(p, vr_mst_hand), VRGetEntWeaponClip(p, vr_mst_hand), p.vr_mag_out, cvar_hget(cvarh_vr_test_magslide_mode)));
        VR_MST_End();
        return;
    }
    vr_mst_turn++;
    float eject = cvar_hget(cvarh_vr_nailpistol_mag_eject_time);
    float seat = cvar_hget(cvarh_vr_nailpistol_mag_seat_time);
    float pop = cvar_hget(cvarh_vr_nailpistol_mag_eject_speed);
    float share = cvar_hget(cvarh_vr_nailpistol_mag_eject_hand);
    float full = cvar_hget(cvarh_vr_test_magslide_full);
    float mode = cvar_hget(cvarh_vr_test_magslide_mode);
    // The slides' fastest: their easing's twice the mean, a quarter over for the rounding of the frames.
    float ejectStep = 2.5 / (72 * max(eject - 1 / 72, 0.01));
    float seatStep = 2.5 / (72 * max(seat, 0.01));

    if(s == 1 || s == 6 || s == 11 || s == 12 || s == 13)
    {
        // A loaded nail pistol, held still a moment (or, 6, moving: its hand reports it): then its eject button.
        if(f == 1)
        {
            VR_MST_Give(WID_NAILPISTOL, 12);
            localcmd(sprintf("vr_test_magslide %g\n", vr_mst_hand == cVR_MainHand ? 1 : 2));
            VR_MST_Pose(0);
            return;
        }
        if(f < 12) { return; }
        if(s == 6 && f == 12)
        {
            localcmd(sprintf("vr_mock_hand_glide 0.6\nvr_mock_hand_to %s by 40 0 -10\n", VR_MST_Name(vr_mst_hand)));
            return;
        }
        if(s == 6 && f < 18) { return; }
        VR_MST_Button("1");
        vr_mst_step = s == 1 ? 2 : s == 6 ? 7 : s == 11 ? 21 : s == 12 ? 31 : 41;
        vr_mst_frames = 0;
        vr_mst_seen = 0;
        vr_mst_r0 = vr_reload_out_r;
        return;
    }
    if(s == 2 || s == 7 || s == 21 || s == 31 || s == 41)
    {
        // Out of the count at once, nothing loose yet.
        if(!(p.vr_mag_out & bit)) { if(s != 7) { VR_MST_Pose(vr_mst_turn); } return; }
        VR_Test_Check(!VR_Reload_MagIn(p, vr_mst_hand) && VRGetEntWeaponClip(p, vr_mst_hand) == 0 && vr_reload_out_r == vr_mst_r0, sprintf(
            "the button, hand %s: out of the count at once (clip %g), not loose yet", VR_MST_Name(vr_mst_hand), VRGetEntWeaponClip(p, vr_mst_hand)));
        VR_MST_Button("0");
        vr_mst_step = s == 2 ? 3 : s == 7 ? 8 : s == 21 ? 22 : s == 31 ? 32 : 42;
        vr_mst_frames = 0;
        return;
    }
    if(s == 3 || s == 8)
    {
        if(s == 3) { VR_MST_Pose(vr_mst_turn); }
        if(p.vr_mag_out & bit) { return; }
        // Free: the slide over; the loose one where it had slid to, out along the grip.
        VR_MST_CheckSlide("sliding out", 0, 1, ejectStep);
        entity r = vr_reload_out_r;
        vector way = vr_reload_out_way;
        VR_Test_Check(r != vr_mst_r0 && r.vr_ammo_count == 12 && r.vr_ammo_wid == WID_NAILPISTOL, sprintf("free at its end: a magazine of %g", r.vr_ammo_count));
        vector end = '1 0 0' * cvar_hget(cvarh_vr_test_magslide_x) + '0 1 0' * cvar_hget(cvarh_vr_test_magslide_y) + '0 0 1' * cvar_hget(cvarh_vr_test_magslide_z);
        VR_Test_Check(vlen(vr_reload_out_at - end) < 1.5, sprintf("the loose one where the slide ended: %.2f units from the magazine last drawn (want < 1.5)", vlen(vr_reload_out_at - end)));
        vector hand = vr_reload_out_hand * share;
        float along = vr_reload_out_vel * way;
        if(s == 3)
        {
            VR_Test_Check(fabs(along - pop) < 1 + vlen(hand), sprintf("it pops out along the grip's line at %.0f u/s (want %.0f)", along, pop));
        }
        else
        {
            vector got = vr_reload_out_vel - way * pop;
            VR_Test_Check(vlen(hand) > 10 && vlen(got - hand) < 1, sprintf("the hand's velocity inherited: %.1f u/s of the hand's %.1f (share %g; %.1f off)",
                vlen(got), vlen(vr_reload_out_hand), share, vlen(got - hand)));
            VR_Test_Check(along > vlen(hand), sprintf("outward dominant: %.0f u/s along the grip against the hand's %.0f", along, vlen(hand)));
            localcmd("vr_mock_hand_glide 0\n");
        }
        vr_mst_step = s == 3 ? 4 : 9;
        vr_mst_frames = 0;
        return;
    }
    if(s == 4 || s == 9)
    {
        // A new one seated: loaded at once.
        VR_MST_Seat();
        VR_Test_Check(VR_Reload_MagIn(p, vr_mst_hand) && VRGetEntWeaponClip(p, vr_mst_hand) == 12, sprintf("seated, hand %s: loaded at once (clip %g)",
            VR_MST_Name(vr_mst_hand), VRGetEntWeaponClip(p, vr_mst_hand)));
        vr_mst_step = s == 4 ? 5 : 10;
        vr_mst_frames = 0;
        vr_mst_seen = 0;
        return;
    }
    if(s == 5 || s == 10)
    {
        VR_MST_Pose(vr_mst_turn);
        if(mode == 1) { vr_mst_seen = 1; return; }
        if(!vr_mst_seen) { return; }
        VR_MST_CheckSlide("seating", 1, 0, seatStep);
        if(s == 5)
        {
            vr_mst_step = 6;
            vr_mst_frames = 0;
            return;
        }
        // (Both hands done?)
        if(vr_mst_hand == cVR_MainHand)
        {
            VRGiveEntWeapon(p, vr_mst_hand, WID_FIST, 0, 0);
            localcmd("vr_mock_hand main\n");
            vr_mst_hand = cVR_OffHand;
            vr_mst_step = 1;
            vr_mst_frames = 0;
            return;
        }
        VRGiveEntWeapon(p, vr_mst_hand, WID_FIST, 0, 0);
        localcmd("vr_mock_hand off\n");
        vr_mst_hand = cVR_MainHand;
        // The cap: at any hand speed, either way: mostly outward.
        entity os9 = self;
        vector hv = p.handvel;
        vector w9 = '0 0 -1';
        p.handvel = '0 0 40';
        vector v9 = VR_Reload_MagOutVel(p, cVR_MainHand, w9);
        VR_Test_Check(v9 * w9 > vlen(v9 - w9 * pop) && vlen(v9 - w9 * pop) <= pop * vr_reload_out_hand_most + 0.01, sprintf(
            "the hand fast against it: %.0f u/s still along the grip, the hand's %.0f (at most %.2f of the pop)", v9 * w9, vlen(v9 - w9 * pop), vr_reload_out_hand_most));
        p.handvel = '0 0 -40';
        v9 = VR_Reload_MagOutVel(p, cVR_MainHand, w9);
        VR_Test_Check(v9 * w9 > pop, sprintf("the hand fast with it: %.0f u/s along the grip", v9 * w9));
        p.handvel = hv;
        self = os9;
        vr_mst_step = 11;
        vr_mst_frames = 0;
        return;
    }
    if(s == 22)
    {
        // Mid-slide, the gun leaves the hand (holstered, dropped, swapped): free now, nothing drawn sliding.
        if(mode == 2 && !vr_mst_seen) { vr_mst_seen = 1; return; }
        if(!vr_mst_seen) { return; }
        VRGiveEntWeapon(p, vr_mst_hand, WID_FIST, 0, 0);
        vr_mst_step = 23;
        vr_mst_frames = 0;
        return;
    }
    if(s == 23)
    {
        // (A frame for the player's frame, a frame for the client's.)
        if(f < 3) { return; }
        VR_Test_Check(!(p.vr_mag_out & bit) && vr_reload_out_r != vr_mst_r0 && vr_reload_out_r.vr_ammo_count == 12 && mode == 0, sprintf(
            "the gun gone mid-slide: free at once (a magazine of %g), nothing sliding (the client's mode %g)", vr_reload_out_r.vr_ammo_count, mode));
        vr_mst_step = 12;
        vr_mst_frames = 0;
        return;
    }
    if(s == 32)
    {
        // Mid-slide, its player dead.
        if(mode == 2 && !vr_mst_seen) { vr_mst_seen = 1; return; }
        if(!vr_mst_seen) { return; }
        entity os32 = self;
        self = p;
        float hp = p.health;
        p.health = -1;
        VR_Reload_MagOutFrame();
        p.health = hp;
        self = os32;
        VR_Test_Check(!(p.vr_mag_out & bit) && vr_reload_out_r != vr_mst_r0 && vr_reload_out_r.vr_ammo_count == 12, sprintf(
            "its player dead mid-slide: free at once (a magazine of %g)", vr_reload_out_r.vr_ammo_count));
        vr_mst_step = 33;
        vr_mst_frames = 0;
        return;
    }
    if(s == 33)
    {
        if(f < 3) { return; }
        VR_Test_Check(mode == 0, sprintf("nothing drawn sliding after (the client's mode %g)", mode));
        vr_mst_step = 13;
        vr_mst_frames = 0;
        return;
    }
    if(s == 42)
    {
        // Mid-slide, a new one seated: the old one goes free first, the new one seats.
        if(mode == 2 && !vr_mst_seen) { vr_mst_seen = 1; return; }
        if(!vr_mst_seen) { return; }
        VR_MST_Seat();
        VR_Test_Check(!(p.vr_mag_out & bit) && vr_reload_out_r != vr_mst_r0 && vr_reload_out_r.vr_ammo_count == 12 && VR_Reload_MagIn(p, vr_mst_hand) &&
            VRGetEntWeaponClip(p, vr_mst_hand) == 12, sprintf("a new one seated mid-slide: the old free (%g), the new in (clip %g)",
            vr_reload_out_r.vr_ammo_count, VRGetEntWeaponClip(p, vr_mst_hand)));
        vr_mst_step = 43;
        vr_mst_frames = 0;
        vr_mst_seen = 0;
        return;
    }
    if(s == 43)
    {
        // And mid-seat, the gun leaves the hand: nothing left sliding.
        if(mode == 1 && !vr_mst_seen) { vr_mst_seen = 1; VRGiveEntWeapon(p, vr_mst_hand, WID_FIST, 0, 0); return; }
        if(!vr_mst_seen) { return; }
        if(f < 4) { return; }
        VR_Test_Check(mode == 0, sprintf("the gun gone mid-seat: nothing drawn sliding (the client's mode %g)", mode));
        vr_mst_step = 15;
        vr_mst_frames = 0;
        return;
    }
    if(s == 15)
    {
        // The nailgun: a short seat.
        if(f == 1)
        {
            VR_MST_Give(WID_NAILGUN, 0);
            VRSetEntWeaponFlags(p, vr_mst_hand, VRGetEntWeaponFlags(p, vr_mst_hand) | QVR_WPNFLAG_NOMAG);
            VR_MST_Pose(0);
            return;
        }
        if(f < 12) { return; }
        entity os15 = self;
        self = p;
        VR_Reload_Take(VRGetOtherHand(vr_mst_hand));
        entity m15 = VR_Carry_Held(p, VRGetOtherHand(vr_mst_hand));
        if(m15) { VR_Reload_MagAtWell(VRGetOtherHand(vr_mst_hand), m15); }
        self = os15;
        VR_Test_Check(VR_Reload_MagIn(p, vr_mst_hand) && VRGetEntWeaponClip(p, vr_mst_hand) > 0, sprintf("the nailgun: loaded at once (clip %g)", VRGetEntWeaponClip(p, vr_mst_hand)));
        vr_mst_step = 16;
        vr_mst_frames = 0;
        vr_mst_seen = 0;
        return;
    }
    if(s == 16)
    {
        VR_MST_Pose(vr_mst_turn);
        if(mode == 1) { vr_mst_seen = 1; return; }
        if(!vr_mst_seen) { return; }
        VR_MST_CheckSlide("the nailgun seating", 1, 0, 2.5 / (72 * cvar_hget(cvarh_vr_reload_mag_seat_time)));
        VR_MST_Button("1");
        vr_mst_r0 = vr_reload_out_r;
        vr_mst_step = 17;
        vr_mst_frames = 0;
        return;
    }
    if(s == 17)
    {
        // Its button as ever: free at once, no slide.
        if(VR_Reload_MagIn(p, vr_mst_hand)) { return; }
        VR_Test_Check(!p.vr_mag_out && vr_reload_out_r == vr_mst_r0 && f < 10, sprintf("the nailgun's button: free at once as ever, no slide (out bits %g)", p.vr_mag_out));
        VR_MST_Button("0");
        vr_mst_step = 18;
        vr_mst_frames = 0;
        return;
    }
    if(s == 18)
    {
        // The times 0: no slide either way, and the nail pistol's pop all the same.
        if(f == 1)
        {
            localcmd("tune vr_nailpistol_mag_eject_time 0\ntune vr_nailpistol_mag_seat_time 0\n");
            VR_MST_Give(WID_NAILPISTOL, 12);
            return;
        }
        if(f < 12) { return; }
        VR_MST_Button("1");
        vr_mst_r0 = vr_reload_out_r;
        vr_mst_step = 19;
        vr_mst_frames = 0;
        return;
    }
    if(s == 19)
    {
        if(VR_Reload_MagIn(p, vr_mst_hand)) { return; }
        VR_Test_Check(!p.vr_mag_out && vr_reload_out_r != vr_mst_r0 && vr_reload_out_r.vr_ammo_count == 12 && fabs(vr_reload_out_vel * vr_reload_out_way - pop) < 1 + vlen(vr_reload_out_hand * share),
            sprintf("Eject Time 0: free at once, a magazine of %g, popping out at %.0f u/s", vr_reload_out_r.vr_ammo_count, vr_reload_out_vel * vr_reload_out_way));
        VR_MST_Button("0");
        VR_MST_Seat();
        vr_mst_step = 20;
        vr_mst_frames = 0;
        vr_mst_seen = 0;
        return;
    }
    if(s == 20)
    {
        if(mode != 0) { vr_mst_seen = 1; }
        if(f < 6) { return; }
        VR_Test_Check(!vr_mst_seen && VR_Reload_MagIn(p, vr_mst_hand), "Seat Time 0: home at once, no slide");
        localcmd(sprintf("tune vr_nailpistol_mag_eject_time %g\ntune vr_nailpistol_mag_seat_time %g\n", 0.14, 0.12));
        localcmd("vr_mock_hand main\n");
        VR_MST_End();
        return;
    }
};
#endif
