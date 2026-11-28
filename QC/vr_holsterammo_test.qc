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

// Removes every loose magazine in the world and returns how many were cleared (VR_AmmoSwitch_Test).
float() VR_AmmoSwitch_ClearMags =
{
    float n = 0;
    entity e = find(world, classname, VR_RELOAD_MAGCLASS);
    while(e)
    {
        entity next = find(e, classname, VR_RELOAD_MAGCLASS);
        remove(e);
        n++;
        e = next;
    }
    return n;
};

// Finds the one loose magazine spawned by an ammo switch (world if none, or more than one).
entity() VR_AmmoSwitch_OnlyMag =
{
    entity first = find(world, classname, VR_RELOAD_MAGCLASS);
    if(!first)
    {
        return world;
    }
    if(find(first, classname, VR_RELOAD_MAGCLASS) != world)
    {
        return world;
    }
    return first;
};

// Checks `xWid` switching from `xFromSec` (0 normal -> alt, 1 alt -> normal) with 0 reserve of the target ammo,
// in `xFlat` (1 flatscreen, 0 VR immersive).
void(float xWid, float xFromSec, float xFlat) VR_AmmoSwitch_CheckMagGun =
{
    string modeTag = xFlat ? "flat" : "VR";
    float oldFlags = xFromSec ? QVR_WPNFLAG_USE_SECONDARY_AMMO : 0;
    float oldAid = WeaponIdToAmmoId(xWid, oldFlags);
    float newAid = WeaponIdToAmmoId(xWid, oldFlags ^ QVR_WPNFLAG_USE_SECONDARY_AMMO);
    float full = WeaponIdToBaseClipSize(xWid);
    float reserve = 50;

    VR_AmmoSwitch_ClearMags();
    VRGiveEntWeapon(self, cVR_MainHand, xWid, oldFlags, full);
    VR_AmmoUtil_SetAmmoCountByAmmoId(self, oldAid, reserve);
    VR_AmmoUtil_SetAmmoCountByAmmoId(self, newAid, 0);
    W_SetCurrentAmmoFor(cVR_MainHand);

    ToggleSecondaryAmmo(cVR_MainHand);

    entity mag = VR_AmmoSwitch_OnlyMag();
    float f = VRGetEntWeaponFlags(self, cVR_MainHand);
    float expectedSkin = xFromSec ? 1 : 0;
    float expectedRounds = xFlat ? 0 : full;
    float expectedReserve = xFlat ? (reserve + full) : reserve;
    string label = sprintf("%s %s (%s -> %s, 0 target reserve)", modeTag, WeaponIdToWeaponName(xWid),
        xFromSec ? "alt" : "own", xFromSec ? "own" : "alt");

    VR_Test_Check(mag != world && mag.vr_ammo_wid == xWid && mag.vr_ammo_aid == oldAid &&
        mag.skin == expectedSkin && mag.vr_ammo_count == expectedRounds,
        sprintf("%s: ejected old mag (aid %g/%g, skin %g/%g, rounds %g/%g)", label,
            mag.vr_ammo_aid, oldAid, mag.skin, expectedSkin, mag.vr_ammo_count, expectedRounds));
    VR_Test_Check((f & QVR_WPNFLAG_NOMAG) && ((f & QVR_WPNFLAG_USE_SECONDARY_AMMO) == (oldFlags ^ QVR_WPNFLAG_USE_SECONDARY_AMMO)) &&
        self.weaponmodel == WeaponIdToModel(xWid, f) && VRGetEntWeaponClip(self, cVR_MainHand) == 0 &&
        self.currentammo == newAid && VR_AmmoUtil_GetAmmoCountByAmmoId(self, oldAid) == expectedReserve &&
        VR_AmmoUtil_GetAmmoCountByAmmoId(self, newAid) == 0,
        sprintf("%s: gun switched without mag (flags %g, model %s, clip %g, oldRes %g/%g)", label, f,
            self.weaponmodel, VRGetEntWeaponClip(self, cVR_MainHand),
            VR_AmmoUtil_GetAmmoCountByAmmoId(self, oldAid), expectedReserve));

    // Firing with 0 alternate ammo must refuse without an unpaid shot.
    float short0 = vr_clip_short;
    self.attack_finished = 0;
    W_Attack();
    VR_Test_Check(vr_clip_short == short0 && VRGetEntWeaponClip(self, cVR_MainHand) == 0,
        sprintf("%s: firing dry with 0 target ammo refuses cleanly", label));

    // Switching back while no magazine is attached must not eject a phantom second magazine.
    VR_AmmoSwitch_ClearMags();
    ToggleSecondaryAmmo(cVR_MainHand);
    float fBack = VRGetEntWeaponFlags(self, cVR_MainHand);
    VR_Test_Check(find(world, classname, VR_RELOAD_MAGCLASS) == world && (fBack & QVR_WPNFLAG_NOMAG) &&
        ((fBack & QVR_WPNFLAG_USE_SECONDARY_AMMO) == oldFlags) &&
        VR_AmmoUtil_GetAmmoCountByAmmoId(self, oldAid) == expectedReserve,
        sprintf("%s: switching back with no mag attached ejects nothing and keeps reserve %g", label,
            VR_AmmoUtil_GetAmmoCountByAmmoId(self, oldAid)));

    VR_AmmoSwitch_ClearMags();
};

// ammomags: in the headset with immersive reloading the rounds loaded set the ammo (vr_reload.qc VR_Reload_AmmoByMag):
// the switch refused, nothing ejected, its flags and clip as they were.
void(float xWid, float xFromSec) VR_AmmoSwitch_CheckRefused =
{
    float oldFlags = xFromSec ? QVR_WPNFLAG_USE_SECONDARY_AMMO : 0;
    float full = WeaponIdToBaseClipSize(xWid);
    VR_AmmoSwitch_ClearMags();
    VRGiveEntWeapon(self, cVR_MainHand, xWid, oldFlags, full);
    W_SetCurrentAmmoFor(cVR_MainHand);
    if(!VR_Reload_AmmoByMag(self, cVR_MainHand))
    {
        return; // (no other ammo in this game: no switch at all)
    }
    ToggleSecondaryAmmo(cVR_MainHand);
    float f = VRGetEntWeaponFlags(self, cVR_MainHand);
    VR_Test_Check(find(world, classname, VR_RELOAD_MAGCLASS) == world && (f & QVR_WPNFLAG_USE_SECONDARY_AMMO) == oldFlags &&
        !(f & QVR_WPNFLAG_NOMAG) && VRGetEntWeaponClip(self, cVR_MainHand) == full,
        sprintf("VR %s (%s): immersive reloading refuses the switch, the rounds loaded set the ammo (flags %g, clip %g)",
        WeaponIdToWeaponName(xWid), xFromSec ? "alt" : "own", f, VRGetEntWeaponClip(self, cVR_MainHand)));
    VR_AmmoSwitch_ClearMags();
};

void() VR_AmmoSwitch_Test =
{
    VR_Test_BeginFlags("ammoswitch_test", VR_TEST_LOUD);
    float oldFlat = self.vr_flatscreen;
    float oldHolster = cvar_hget(cvarh_vr_holster_mode);
    float oldReload = cvar_hget(cvarh_vr_reload_mode);
    float oldFlatReload = cvar_hget(cvarh_vr_flat_reload);
    float oldFlatEject = cvar_hget(cvarh_vr_flat_mag_eject);
    float oldNail = cvar_hget(cvarh_vr_reload_mode_nail);
    float oldSnail = cvar_hget(cvarh_vr_reload_mode_snail);
    float oldLight = cvar_hget(cvarh_vr_reload_mode_light);
    float oldEmptyHand = cvar_hget(cvarh_vr_ammo_switch_empty_hand);

    cvar_set("vr_holster_mode", ftos(QVR_WEAPONMODE_IMMERSIVE));
    cvar_set("vr_reload_mode", ftos(QVR_RELOADMODE_IMMERSIVE));
    cvar_set("vr_flat_reload", "1");
    cvar_set("vr_flat_mag_eject", "1");
    cvar_set("vr_reload_mode_nail", "-1");
    cvar_set("vr_reload_mode_snail", "-1");
    cvar_set("vr_reload_mode_light", "-1");
    cvar_set("vr_ammo_switch_empty_hand", "0");
    VRGiveEntWeapon(self, cVR_OffHand, WID_FIST, 0, 0);
    self.items = self.items | IT_NAILGUN | IT_SUPER_NAILGUN | IT_LIGHTNING | IT_ROCKET_LAUNCHER | IT_GRENADE_LAUNCHER;

    for(float flat = 0; flat <= 1; flat++)
    {
        self.vr_flatscreen = flat;
        for(float fromSec = 0; fromSec <= 1; fromSec++)
        {
            if(!flat)
            {
                // (ammomags: the headset's immersive reloading: no switch, the magazine loaded sets the ammo.)
                VR_AmmoSwitch_CheckRefused(WID_NAILGUN, fromSec);
                VR_AmmoSwitch_CheckRefused(WID_SUPER_NAILGUN, fromSec);
                VR_AmmoSwitch_CheckRefused(WID_LIGHTNING, fromSec);
                continue;
            }
            VR_AmmoSwitch_CheckMagGun(WID_NAILGUN, fromSec, flat);
            VR_AmmoSwitch_CheckMagGun(WID_SUPER_NAILGUN, fromSec, flat);
            VR_AmmoSwitch_CheckMagGun(WID_LIGHTNING, fromSec, flat);
        }
    }

    // Flatscreen edge cases: pending empty eject delay, busy reload trip, and non-mag launchers (RL, GL).
    self.vr_flatscreen = 1;
    VR_AmmoSwitch_ClearMags();
    VRGiveEntWeapon(self, cVR_MainHand, WID_NAILGUN, 0, 0);
    self.ammo_nails = 40;
    self.ammo_lava_nails = 0;
    W_SetCurrentAmmoFor(cVR_MainHand);
    VR_FlatReload_SetDue(cVR_MainHand, time + 0.1);
    ToggleSecondaryAmmo(cVR_MainHand);
    entity delayedMag = VR_AmmoSwitch_OnlyMag();
    VR_Test_Check(delayedMag != world && delayedMag.vr_ammo_aid == AID_NAILS && delayedMag.skin == 0 &&
        (VRGetEntWeaponFlags(self, cVR_MainHand) & QVR_WPNFLAG_NOMAG) && self.vr_flatrl_due1 == 0,
        "flat Nailgun pending empty-eject delay: N flushes and ejects old normal nail mag immediately");
    VR_AmmoSwitch_ClearMags();

    VRGiveEntWeapon(self, cVR_MainHand, WID_NAILGUN, 0, 12);
    self.ammo_nails = 30;
    self.ammo_lava_nails = 0;
    W_SetCurrentAmmoFor(cVR_MainHand);
    VR_FlatReload_Start(cVR_MainHand, "test");
    ToggleSecondaryAmmo(cVR_MainHand);
    entity busyMag = VR_AmmoSwitch_OnlyMag();
    VR_Test_Check(!VR_FlatReload_Busy(self, cVR_MainHand) && busyMag != world && busyMag.vr_ammo_aid == AID_NAILS &&
        busyMag.skin == 0 && self.ammo_nails == 42 && (VRGetEntWeaponFlags(self, cVR_MainHand) & QVR_WPNFLAG_NOMAG),
        "flat Nailgun mid-reload trip: N cancels trip, refunds 12 nails (42), ejects old normal nail mag");
    VR_AmmoSwitch_ClearMags();

    for(float l = 0; l < 2; l++)
    {
        float lw = l ? WID_GRENADE_LAUNCHER : WID_ROCKET_LAUNCHER;
        VRGiveEntWeapon(self, cVR_MainHand, lw, 0, 4);
        self.ammo_rockets = 10;
        self.ammo_multi_rockets = 0;
        W_SetCurrentAmmoFor(cVR_MainHand);
        ToggleSecondaryAmmo(cVR_MainHand);
        float short0 = vr_clip_short;
        self.attack_finished = 0;
        W_Attack();
        VR_Test_Check(find(world, classname, VR_RELOAD_MAGCLASS) == world && self.ammo_rockets == 14 &&
            self.ammo_multi_rockets == 0 && VRGetEntWeaponClip(self, cVR_MainHand) == 0 &&
            (VRGetEntWeaponFlags(self, cVR_MainHand) & QVR_WPNFLAG_USE_SECONDARY_AMMO) && vr_clip_short == short0,
            sprintf("flat %s: refunds 4 rockets (14), 0 multi-rockets, no unpaid shot", WeaponIdToWeaponName(lw)));
        ToggleSecondaryAmmo(cVR_MainHand);
        VR_Test_Check(self.ammo_rockets == 14 && !(VRGetEntWeaponFlags(self, cVR_MainHand) & QVR_WPNFLAG_USE_SECONDARY_AMMO),
            sprintf("flat %s: switching back preserves 14 rockets", WeaponIdToWeaponName(lw)));
    }

    self.vr_flatscreen = oldFlat;
    cvar_set("vr_holster_mode", ftos(oldHolster));
    cvar_set("vr_reload_mode", ftos(oldReload));
    cvar_set("vr_flat_reload", ftos(oldFlatReload));
    cvar_set("vr_flat_mag_eject", ftos(oldFlatEject));
    cvar_set("vr_reload_mode_nail", ftos(oldNail));
    cvar_set("vr_reload_mode_snail", ftos(oldSnail));
    cvar_set("vr_reload_mode_light", ftos(oldLight));
    cvar_set("vr_ammo_switch_empty_hand", ftos(oldEmptyHand));
    VR_Test_Verdict();
};
#endif
