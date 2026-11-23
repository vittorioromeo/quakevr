#ifdef QVR_TESTS
// vr_holsterammo_test.qc -- a holstered gun keeps its loaded rounds, every way it goes into a holster and out
// (`vr_reload_test 49; impulse 125`: VR_Reload_Test's branch; Misc/quakevr/holsterammo_test.sh; ROUND25 "holsterammo").
// In one frame, by the game's own functions, `self` the player: for each reloading mode (1 All Holsters, 2 Hip Holsters,
// 3 Immersive) a nailgun with 17 rounds (a partial clip: a reload would show) and 50 nails in the reserve, put away and
// drawn again by each path: a hand let go at an empty holster and drawn by a hand gripping there (VRLetGoAtHolster,
// VRDrawHolsterWeapon), 5 and 2 (VR_FlatHolster_Put, VR_FlatHolster_Cycle), the wheel's EMPTY HAND and its slot (the
// other hand: VR_Wheel_Pick), the wheel trading it for a holstered shotgun and back, the holster menu's swap
// (VR_FlatHolster_Swap). Each: the same rounds, the same magazine state (QVR_WPNFLAG_NOMAG) and the reserve untouched.
// Then the holsters' reload (modes 1 and 2: VRTryReloadWeapon) conserves clip + reserve, and Quick Slots (no clips:
// every shot from the reserve) leaves the reserve alone. (The wheel's button itself, B/Y, is also the magazine's eject:
// the client holds its press back while the wheel may open: vr_input.cpp WheelHold, weaponwheel_test.sh.)
// Lines "holsterammo_test: PASS|FAIL ...", ending "holsterammo_test: N passed, M failed".

float vr_hat_mode; // the reloading mode of the case running

// A check's words, the reloading mode first.
string(string what) VR_HolsterAmmo_What =
{
    return sprintf("mode %g: %s", vr_hat_mode, what);
};

// Every holster emptied, the main hand given a nailgun with `xClip` rounds (its magazine in), the off hand empty, the
// reserve `xNails`.
void(float xClip, float xNails) VR_HolsterAmmo_Setup =
{
    for(float i = 0; i < 6; i++)
    {
        float hs = VR_FlatHolster_Hotspot(i);
        setHolsterWeapon(hs, WID_FIST);
        setHolsterWeaponModel(hs, WeaponIdToModel(WID_FIST, 0));
        setHolsterWeaponFlags(hs, 0);
        VRSetEntHolsterWeaponInst(self, hs, world);
    }
    VRGiveEntWeapon(self, cVR_OffHand, WID_FIST, 0, 0);
    VRGiveEntWeapon(self, cVR_MainHand, WID_NAILGUN, 0, xClip);
    self.items = self.items | IT_NAILGUN | IT_SHOTGUN;
    self.ammo_nails = xNails;
    W_SetCurrentAmmoFor(cVR_MainHand);
    W_SetCurrentAmmoFor(cVR_OffHand);
};

// The holster (0 .. 5) holding `xWid`; -1 none.
float(float xWid) VR_HolsterAmmo_SlotOf =
{
    for(float i = 0; i < 6; i++)
    {
        if(getHolsterWeapon(VR_FlatHolster_Hotspot(i)) == xWid)
        {
            return i;
        }
    }
    return -1;
};

// The nailgun in `xHand` after a path: its rounds `xClip`, its magazine in, the reserve `xNails`.
void(float xHand, float xClip, float xNails, string xPath) VR_HolsterAmmo_Expect =
{
    float wid = VRGetEntWeapon(self, xHand);
    float clip = VRGetEntWeaponClip(self, xHand);
    float nomag = VRGetEntWeaponFlags(self, xHand) & QVR_WPNFLAG_NOMAG;
    VR_Test_Check(wid == WID_NAILGUN && clip == xClip && !nomag && self.ammo_nails == xNails,
        VR_HolsterAmmo_What(sprintf("%s: weapon %g, %g rounds (%g before), magazine %s, reserve %g (%g before)", xPath,
            wid, clip, xClip, nomag ? "OUT" : "in", self.ammo_nails, xNails)));
};

void() VR_HolsterAmmo_Test =
{
    VR_Test_BeginFlags("holsterammo_test", VR_TEST_LOUD);
    float oldHolster = cvar_hget(cvarh_vr_holster_mode);
    float oldReload = cvar_hget(cvarh_vr_reload_mode);
    float oldNail = cvar_hget(cvarh_vr_reload_mode_nail);
    float oldShot = cvar_hget(cvarh_vr_reload_mode_shot);
    cvar_set("vr_holster_mode", ftos(QVR_WEAPONMODE_IMMERSIVE));
    cvar_set("vr_reload_mode_nail", "-1");
    cvar_set("vr_reload_mode_shot", "-1");
    float clip = 17;
    float nails = 50;
    for(vr_hat_mode = 1; vr_hat_mode <= 3; vr_hat_mode++)
    {
        cvar_set("vr_reload_mode", ftos(vr_hat_mode));

        // Reached: let go at the empty right hip holster, drawn by the other hand gripping there.
        VR_HolsterAmmo_Setup(clip, nails);
        VRLetGoAtHolster(self, cVR_MainHand, QVR_HS_RIGHT_HIP_HOLSTER, WID_NAILGUN, 0, TRUE);
        VR_Test_Check(getHolsterWeapon(QVR_HS_RIGHT_HIP_HOLSTER) == WID_NAILGUN &&
            VRGetEntHolsterWeaponInst(self, QVR_HS_RIGHT_HIP_HOLSTER).wi_clip == clip,
            VR_HolsterAmmo_What(sprintf("let go at a holster: in it with %g rounds",
                VRGetEntHolsterWeaponInst(self, QVR_HS_RIGHT_HIP_HOLSTER).wi_clip)));
        VRDrawHolsterWeapon(self, cVR_OffHand, QVR_HS_RIGHT_HIP_HOLSTER);
        VR_HolsterAmmo_Expect(cVR_OffHand, clip, nails, "reached: let go at a holster, drawn by a grip there");

        // The keys: 5 puts it away, 2 cycles it back.
        VR_HolsterAmmo_Setup(clip, nails);
        VR_FlatHolster_Put(cVR_MainHand);
        VR_FlatHolster_Cycle(cVR_MainHand);
        VR_HolsterAmmo_Expect(cVR_MainHand, clip, nails, "5 put away, 2 drawn");

        // The wheel: EMPTY HAND, then the other hand takes its slot.
        VR_HolsterAmmo_Setup(clip, nails);
        VR_Wheel_Pick(cVR_MainHand, -1);
        float at = VR_HolsterAmmo_SlotOf(WID_NAILGUN);
        VR_Wheel_Pick(cVR_OffHand, at);
        VR_HolsterAmmo_Expect(cVR_OffHand, clip, nails, sprintf("the wheel's EMPTY HAND, its slot %g drawn by the other hand", at));

        // The wheel's trade: a shotgun (5 shells loaded) in the right shoulder holster drawn, the nailgun put away; back.
        VR_HolsterAmmo_Setup(clip, nails);
        setHolsterWeapon(QVR_HS_RIGHT_SHOULDER_HOLSTER, WID_SHOTGUN);
        setHolsterWeaponModel(QVR_HS_RIGHT_SHOULDER_HOLSTER, WeaponIdToModel(WID_SHOTGUN, 0));
        VRSetEntHolsterWeaponInst(self, QVR_HS_RIGHT_SHOULDER_HOLSTER, WeaponInst_New(5));
        VR_Wheel_Pick(cVR_MainHand, 1);
        float shells = VRGetEntWeaponClip(self, cVR_MainHand);
        VR_Test_Check(VRGetEntWeapon(self, cVR_MainHand) == WID_SHOTGUN && shells == 5,
            VR_HolsterAmmo_What(sprintf("the wheel's trade: the shotgun drawn with %g shells (5)", shells)));
        at = VR_HolsterAmmo_SlotOf(WID_NAILGUN);
        VR_Wheel_Pick(cVR_MainHand, at);
        VR_HolsterAmmo_Expect(cVR_MainHand, clip, nails, sprintf("the wheel's trade and back (slot %g)", at));
        at = VR_HolsterAmmo_SlotOf(WID_SHOTGUN);
        VR_Test_Check(at >= 0 && VRGetEntHolsterWeaponInst(self, VR_FlatHolster_Hotspot(at)).wi_clip == 5,
            VR_HolsterAmmo_What("the wheel's trade: the shotgun back in a holster with its 5 shells"));

        // The holster menu's swap: into the empty left hip holster, out again.
        VR_HolsterAmmo_Setup(clip, nails);
        VR_FlatHolster_Swap(QVR_HS_LEFT_HIP_HOLSTER, cVR_MainHand);
        VR_FlatHolster_Swap(QVR_HS_LEFT_HIP_HOLSTER, cVR_MainHand);
        VR_HolsterAmmo_Expect(cVR_MainHand, clip, nails, "the holster menu's swap, in and out");

        // The holsters' reload (All / Hip Holsters; Immersive loads the nailgun by hand: nothing): from the reserve,
        // clip + reserve the same.
        VR_HolsterAmmo_Setup(clip, nails);
        VRTryReloadWeapon(self, cVR_MainHand, WID_NAILGUN, 0);
        float got = VRGetEntWeaponClip(self, cVR_MainHand);
        float full = VR_ClipSizeIn(self, cVR_MainHand);
        // (Flat-screen play: R and the empty clip only, never a holster: vr_flatreload.qc.)
        float reloads = vr_hat_mode != 3 && !VR_FlatPlay(self);
        VR_Test_Check(got + self.ammo_nails == clip + nails && got == (reloads ? full : clip),
            VR_HolsterAmmo_What(sprintf("the holsters' reload: %g rounds + %g reserve (%g + %g before; full %g)", got,
                self.ammo_nails, clip, nails, full)));
    }

    // Quick Slots (no clips, every shot from the reserve): a copy into a holster, the hand emptied by the wheel, the copy
    // drawn: the reserve untouched.
    vr_hat_mode = 2;
    cvar_set("vr_reload_mode", "2");
    cvar_set("vr_holster_mode", ftos(QVR_WEAPONMODE_CYCLEQUICKSLOT));
    VR_HolsterAmmo_Setup(clip, nails);
    VRLetGoAtHolster(self, cVR_MainHand, QVR_HS_RIGHT_HIP_HOLSTER, WID_NAILGUN, 0, TRUE);
    VR_Wheel_Pick(cVR_MainHand, -1);
    VRDrawHolsterWeapon(self, cVR_MainHand, QVR_HS_RIGHT_HIP_HOLSTER);
    VR_Test_Check(VRGetEntWeapon(self, cVR_MainHand) == WID_NAILGUN && self.ammo_nails == nails &&
        !VRIsWeaponReloadingEnabledFor(self, cVR_MainHand),
        VR_HolsterAmmo_What(sprintf("Quick Slots: the copy drawn, no clips, the reserve %g (%g)", self.ammo_nails, nails)));

    cvar_set("vr_holster_mode", ftos(oldHolster));
    cvar_set("vr_reload_mode", ftos(oldReload));
    cvar_set("vr_reload_mode_nail", ftos(oldNail));
    cvar_set("vr_reload_mode_shot", ftos(oldShot));
    VR_Test_Verdict();
};
#endif
