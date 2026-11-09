// ----------------------------------------------------------------------------
// Flat-screen play's reload (-flatscreenmode; docs/vr-port/FLATPLAY.md, "Reloading"; his words 2026-11-08)
// ----------------------------------------------------------------------------
//
// Every gun of a flat-screen player has its clip (vr_flat_reload; VRIsWeaponReloadingEnabled), reloaded as VR's Hip
// Holsters mode reloads it (VRTryReloadWeapon: the same clip and reserve sums, the same sounds), but started only by R
// (vr_flat_key_reload: impulse 55, every hand whose gun can take rounds) or by an empty clip (vr_flat_autoreload). Never
// by a holster his hand passes, a flick or a button (vr_wpnholster.qc's vr_flatreload_go gate).
//
// A reload is a trip: the gun's hand goes down to the nearest hip holster (vr_flat_reload_down seconds; the rounds go in
// as it gets there), stays (vr_flat_reload_hold), and comes back (vr_flat_reload_up). The client draws it from
// STAT_QVR_FLATRELOAD (.vr_flatrl: 1 the off hand, 2 the main; vr_flatreload.cpp), with the same times. The hand
// doesn't fire for the whole trip (weapons.qc). Another weapon in the hand (a switch, Q's swap, a drop, a throw) cancels it:
// before the hip nothing moved, after it the rounds are in.
//
// developer 1 prints each step ("flat reload: ...").

.float vr_flatrl;        // the hands on the trip (1 off, 2 main: STAT_QVR_FLATRELOAD)
.float vr_flatrl_at0;    // the off hand's: when its trip started,
.float vr_flatrl_at1;    // the main hand's
.float vr_flatrl_done0;  // whether its rounds went in
.float vr_flatrl_done1;
.entity vr_flatrl_inst0; // the gun's record it is for
.entity vr_flatrl_inst1;

float(float xHand) VR_FlatReload_Bit =
{
    return xHand == cVR_MainHand ? 2 : 1;
};

string(float xHand) VR_FlatReload_HandName =
{
    return xHand == cVR_MainHand ? "main" : "off";
};

float(entity p, float xHand) VR_FlatReload_Busy =
{
    return (p.vr_flatrl & VR_FlatReload_Bit(xHand)) != 0;
};

float() VR_FlatReload_DownTime = { return max(0.05, cvar_hget(cvarh_vr_flat_reload_down)); };
float() VR_FlatReload_HoldTime = { return max(0, cvar_hget(cvarh_vr_flat_reload_hold)); };
float() VR_FlatReload_UpTime = { return max(0.05, cvar_hget(cvarh_vr_flat_reload_up)); };

// The reserve the gun in `xHand` of self loads from; 0 for a hand with none (empty, a fist, a melee weapon: AID_NONE,
// which VR_AmmoUtil_GetAmmoCountByAmmoId asserts on). Until 2026-11-09 R with nothing to reload (the nail pistol with
// no nails) printed both hands' storage, the empty off hand's too: "Assertion failure! ... at 34".
float(float xHand) VR_FlatReload_Storage =
{
    if(VRIsHandWeaponless(self, xHand)) { return 0; }
    float aid = WeaponIdToAmmoId(VRGetEntWeapon(self, xHand), VRGetEntWeaponFlags(self, xHand));
    return aid == AID_NONE ? 0 : VR_AmmoUtil_GetAmmoCountByAmmoId(self, aid);
};

// Whether the gun in `xHand` of self can take rounds now: a flat-screen player's, reloading (a gun with a clip), held by
// its handle, its clip not full, its reserve not empty (VRTryReloadWeapon's own tests).
float(float xHand) VR_FlatReload_Can =
{
    if(!VR_FlatPlay(self) || !self.ishuman || self.health <= 0 || VRIsHandWeaponless(self, xHand))
    {
        return FALSE;
    }
    float w = VRGetEntWeapon(self, xHand);
    float f = VRGetEntWeaponFlags(self, xHand);
    if(f & QVR_WPNFLAG_FOREGRIP_CARRIED)
    {
        return FALSE;
    }
    return VRIsWeaponReloadingEnabledFor(self, xHand) && VRGetEntWeaponClip(self, xHand) < VR_ClipSizeIn(self, xHand) &&
           PlayerHasAmmoForWeapon(self, w, f);
};

void(float xHand, string xWhy) VR_FlatReload_Start =
{
    self.vr_flatrl = self.vr_flatrl | VR_FlatReload_Bit(xHand);
    if(xHand == cVR_MainHand)
    {
        self.vr_flatrl_at1 = time;
        self.vr_flatrl_done1 = FALSE;
        self.vr_flatrl_inst1 = VRGetEntWeaponInst(self, xHand);
    }
    else
    {
        self.vr_flatrl_at0 = time;
        self.vr_flatrl_done0 = FALSE;
        self.vr_flatrl_inst0 = VRGetEntWeaponInst(self, xHand);
    }
    dprint(sprintf("flat reload: start %s hand (%s): weapon %g, clip %g of %g, storage %g\n", VR_FlatReload_HandName(xHand), xWhy,
        VRGetEntWeapon(self, xHand), VRGetEntWeaponClip(self, xHand), VR_ClipSizeIn(self, xHand), VR_FlatReload_Storage(xHand)));
};

void(float xHand) VR_FlatReload_Stop =
{
    self.vr_flatrl = self.vr_flatrl - (self.vr_flatrl & VR_FlatReload_Bit(xHand));
};

// R (impulse 55): every hand whose gun can take rounds, both with a gun in each (dual wield reloads both at once).
void() VR_FlatReload_Key =
{
    float n = 0;
    for(float h = 0; h < 2; ++h)
    {
        float hand = h == 0 ? cVR_MainHand : cVR_OffHand;
        if(!VR_FlatReload_Busy(self, hand) && VR_FlatReload_Can(hand))
        {
            VR_FlatReload_Start(hand, "R");
            ++n;
        }
    }
    if(!n)
    {
        dprint(sprintf("flat reload: R: nothing to reload (main: weapon %g clip %g of %g storage %g; ",
            VRGetEntWeapon(self, cVR_MainHand), VRGetEntWeaponClip(self, cVR_MainHand), VR_ClipSizeIn(self, cVR_MainHand),
            VR_FlatReload_Storage(cVR_MainHand)));
        dprint(sprintf("off: weapon %g clip %g of %g storage %g; reloading %g)\n", VRGetEntWeapon(self, cVR_OffHand),
            VRGetEntWeaponClip(self, cVR_OffHand), VR_ClipSizeIn(self, cVR_OffHand), VR_FlatReload_Storage(cVR_OffHand),
            VRIsWeaponReloadingEnabled(self)));
    }
};

// The weapon cycle (1 and 2: weapons.qc CycleWeaponCommandImpl, the weapons as an inventory) puts `xWeapon` (flags
// `xWeaponFlags`) of `xHand` away: its clip's rounds go back into the reserve (the hand's magazine record is carried to the
// next gun, VRCycleEntWeaponInst, so they'd be lost, or loaded into the wrong gun). A trip under way is cancelled by the
// change (VR_FlatReload_HandFrame).
void(float xHand, float xWeapon, float xWeaponFlags) VR_FlatReload_CycleOut =
{
    if(!VR_FlatPlay(self) || xWeapon == WID_FIST || !VRIsWeaponReloadingEnabledForInst(self, xWeapon, VRGetEntWeaponInst(self, xHand)))
    {
        return;
    }
    float clip = VRGetEntWeaponClip(self, xHand);
    if(clip > 0)
    {
        VR_AmmoUtil_AddAmmoCountByAmmoId(self, WeaponIdToAmmoId(xWeapon, xWeaponFlags), clip);
        VRSetEntWeaponClip(self, xHand, 0);
    }
    dprint(sprintf("flat reload: %s hand put weapon %g away: %g rounds back into the reserve\n", VR_FlatReload_HandName(xHand),
        xWeapon, clip));
};

// And the gun it takes comes loaded from the reserve, as a pickup does (items.qc): no trip for a weapon switch.
void(float xHand) VR_FlatReload_CycleIn =
{
    if(!VR_FlatPlay(self) || VRIsHandWeaponless(self, xHand) || !VRIsWeaponReloadingEnabledFor(self, xHand))
    {
        return;
    }
    float clip = VRGetEntWeaponClip(self, xHand);
    float n = max(0, min(VR_ClipSizeIn(self, xHand) - clip, VR_FlatReload_Storage(xHand)));
    if(n > 0)
    {
        VR_AmmoUtil_DelAmmoCountByAmmoId(self, WeaponIdToAmmoId(VRGetEntWeapon(self, xHand), VRGetEntWeaponFlags(self, xHand)), n);
        VRSetEntWeaponClip(self, xHand, clip + n);
    }
    dprint(sprintf("flat reload: %s hand took weapon %g: clip %g of %g, storage %g\n", VR_FlatReload_HandName(xHand),
        VRGetEntWeapon(self, xHand), VRGetEntWeaponClip(self, xHand), VR_ClipSizeIn(self, xHand), VR_FlatReload_Storage(xHand)));
};

// Each server frame, each hand (vr_wpnholster.qc's player frame): the trip's steps, its cancel, and the empty clip's start.
void(float xHand) VR_FlatReload_HandFrame =
{
    if(!VR_FlatReload_Busy(self, xHand))
    {
        if(cvar_hget(cvarh_vr_flat_autoreload) && !VRIsHandWeaponless(self, xHand) && VRGetEntWeaponClip(self, xHand) <= 0 &&
           VR_FlatReload_Can(xHand))
        {
            VR_FlatReload_Start(xHand, "auto: the clip is empty");
        }
        return;
    }

    float main = xHand == cVR_MainHand;
    float at = main ? self.vr_flatrl_at1 : self.vr_flatrl_at0;
    entity inst = main ? self.vr_flatrl_inst1 : self.vr_flatrl_inst0;
    if(!VR_FlatPlay(self) || self.health <= 0 || VRIsHandWeaponless(self, xHand) || VRGetEntWeaponInst(self, xHand) != inst ||
       (VRGetEntWeaponFlags(self, xHand) & QVR_WPNFLAG_FOREGRIP_CARRIED))
    {
        VR_FlatReload_Stop(xHand);
        dprint(sprintf("flat reload: %s hand cancelled: the weapon changed (%.2f s; weapon %g, clip %g of %g, storage %g)\n",
            VR_FlatReload_HandName(xHand), time - at, VRGetEntWeapon(self, xHand), VRGetEntWeaponClip(self, xHand),
            VR_ClipSizeIn(self, xHand), VR_FlatReload_Storage(xHand)));
        return;
    }

    float done = main ? self.vr_flatrl_done1 : self.vr_flatrl_done0;
    if(!done && time >= at + VR_FlatReload_DownTime() - 0.001)
    {
        // At the hip: the rounds go in, as VR's hip holster puts them in (VRTryReloadWeapon, its sounds and haptics).
        vr_flatreload_go = TRUE;
        VRTryReloadWeapon(self, xHand, VRGetEntWeapon(self, xHand), VRGetEntWeaponFlags(self, xHand));
        vr_flatreload_go = FALSE;
        if(main)
        {
            self.vr_flatrl_done1 = TRUE;
        }
        else
        {
            self.vr_flatrl_done0 = TRUE;
        }
        dprint(sprintf("flat reload: %s hand at the hip (%.2f s): clip %g of %g, storage %g\n", VR_FlatReload_HandName(xHand),
            time - at, VRGetEntWeaponClip(self, xHand), VR_ClipSizeIn(self, xHand), VR_FlatReload_Storage(xHand)));
    }
    if(time >= at + VR_FlatReload_DownTime() + VR_FlatReload_HoldTime() + VR_FlatReload_UpTime() - 0.001)
    {
        VR_FlatReload_Stop(xHand);
        dprint(sprintf("flat reload: %s hand done (%.2f s): clip %g of %g, storage %g\n", VR_FlatReload_HandName(xHand), time - at,
            VRGetEntWeaponClip(self, xHand), VR_ClipSizeIn(self, xHand), VR_FlatReload_Storage(xHand)));
    }
};
