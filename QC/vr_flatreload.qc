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
// The guns' own parts, as VR's immersive reloading moves them (his words 2026-11-10; the same functions, vr_reload.qc and
// vr_revolver.qc, so the same sounds, casings and physical magazines, its caps on rounds lying about):
//   - the super shotgun fired empty breaks open by itself vr_flat_ssg_break_delay after the shot (vr_flat_ssg_break:
//     VR_Reload_SsgBreak, its spent shells thrown out; open it only dry-clicks), and the reload snaps it shut
//     vr_flat_reload_close_delay after its rounds go in at the hip (VR_Reload_SsgClose);
//   - a magazine gun (VR_Reload_UsesMag) fired empty drops its magazine vr_flat_mag_eject_delay after its last round
//     (vr_flat_mag_eject: VR_Reload_EjectOut, as VR's eject button, pushed out at vr_flat_mag_eject_speed; the nail pistol's
//     slides out of its grip first); none in (QVR_WPNFLAG_NOMAG, the clients draw none) until the reload seats a new one at
//     the hip. A clip not yet empty reloaded by R keeps its magazine (its rounds back into the reserve, as before);
//   - the revolver's reload swings its cylinder out as the trip starts, its spent shells thrown out (the live ones stay in:
//     vr_rv_keep_live), and home vr_flat_reload_close_delay after the rounds go in (vr_flat_revolver_eject).
// Anything still due when the hand gets to the hip is done there first (an empty gun reloaded at once: R, Auto Reload).
//
// developer 1 prints each step ("flat reload: ...").

.float vr_flatrl;        // the hands on the trip (1 off, 2 main: STAT_QVR_FLATRELOAD)
.float vr_flatrl_due0;   // the off hand's gun fired empty: when it breaks open or drops its magazine (0 nothing due),
.float vr_flatrl_due1;   // the main hand's
.entity vr_flatrl_duei0; // and the gun's record it is for
.entity vr_flatrl_duei1;
.float vr_flatrl_shut0;  // the off hand's trip opened its gun (the super shotgun, the revolver's cylinder): shut it after
.float vr_flatrl_shut1;  // the rounds go in
float vr_rv_keep_live;   // (VR_Revolver_Eject: only the spent shells out)
void(float xHand, entity r, string xHow) VR_Revolver_Open; // (vr_revolver.qc)
void(float xHand, entity r, string xHow) VR_Revolver_Close;
void(float xHand, entity r, string xHow, vector xPush) VR_Revolver_Eject;
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

// Whether the gun in `xHand` of `p` is a super shotgun that breaks open after its shots (a flat-screen player's with clips,
// vr_flat_ssg_break; not the Widowmaker: its shells go as it fires). (Declared in vr_reload.qc: VR_Reload_SsgFired keeps
// its spent shells in, VR_Reload_SsgIsOpen dry-clicks it open.)
float(entity p, float xHand) VR_FlatReload_SsgBreaks =
{
    return VR_FlatPlay(p) && cvar_hget(cvarh_vr_flat_ssg_break) && VRGetEntWeapon(p, xHand) == WID_SUPER_SHOTGUN &&
           !AD_WidowmakerIn(p, xHand) && VRIsWeaponReloadingEnabledFor(p, xHand);
};

// Whether the gun in `xHand` of self drops its magazine when empty (vr_flat_mag_eject; held by its handle).
float(float xHand) VR_FlatReload_MagEjects =
{
    return VR_FlatPlay(self) && cvar_hget(cvarh_vr_flat_mag_eject) && VR_Reload_UsesMag(VRGetEntWeapon(self, xHand)) &&
           !(VRGetEntWeaponFlags(self, xHand) & QVR_WPNFLAG_FOREGRIP_CARRIED) && VRIsWeaponReloadingEnabledFor(self, xHand);
};

// Whether the gun in `xHand` of self is fired empty with something to throw out: a closed super shotgun's spent shells,
// a magazine gun's empty magazine (in, not already sliding out).
float(float xHand) VR_FlatReload_EjectWanted =
{
    if(VRIsHandWeaponless(self, xHand) || VRGetEntWeaponClip(self, xHand) > 0)
    {
        return FALSE;
    }
    float f = VRGetEntWeaponFlags(self, xHand);
    if(VR_FlatReload_SsgBreaks(self, xHand))
    {
        return !(f & QVR_WPNFLAG_SSG_OPEN) && VR_Reload_SsgSpentOf(f) > 0;
    }
    return VR_FlatReload_MagEjects(xHand) && !(f & QVR_WPNFLAG_NOMAG) && !(self.vr_mag_out & VR_FlatReload_Bit(xHand));
};

void(float xHand, float xDue) VR_FlatReload_SetDue =
{
    if(xHand == cVR_MainHand)
    {
        self.vr_flatrl_due1 = xDue;
        self.vr_flatrl_duei1 = VRGetEntWeaponInst(self, xHand);
    }
    else
    {
        self.vr_flatrl_due0 = xDue;
        self.vr_flatrl_duei0 = VRGetEntWeaponInst(self, xHand);
    }
};

// Throws it out now (VR_FlatReload_EjectWanted): the super shotgun broken open, the magazine let out.
void(float xHand, string xWhy) VR_FlatReload_EjectNow =
{
    VR_FlatReload_SetDue(xHand, 0);
    if(!VR_FlatReload_EjectWanted(xHand))
    {
        return;
    }
    string how = strcat("flat-screen play: ", xWhy);
    if(VR_FlatReload_SsgBreaks(self, xHand))
    {
        VR_Reload_SsgBreak(self, xHand, how);
    }
    else
    {
        VR_Reload_EjectOut(self, xHand, max(0, cvar_hget(cvarh_vr_flat_mag_eject_speed)), how);
    }
    dprint(sprintf("flat reload: %s hand: %s (weapon %g, flags %g)\n", VR_FlatReload_HandName(xHand), how,
        VRGetEntWeapon(self, xHand), VRGetEntWeaponFlags(self, xHand)));
};

// Each frame, each hand of a flat-screen player: a gun fired empty breaks open or drops its magazine after its delay
// (the gun changed meanwhile: nothing); a gun loaded with no magazine in or broken open (a weapon cycled in loaded, a
// pickup) has them back, as a loaded gun does.
void(float xHand) VR_FlatReload_EjectFrame =
{
    float main = xHand == cVR_MainHand;
    float due = main ? self.vr_flatrl_due1 : self.vr_flatrl_due0;
    entity dueInst = main ? self.vr_flatrl_duei1 : self.vr_flatrl_duei0;
    if(self.health <= 0 || !VR_FlatReload_EjectWanted(xHand) || (due && dueInst != VRGetEntWeaponInst(self, xHand)))
    {
        if(due)
        {
            VR_FlatReload_SetDue(xHand, 0);
        }
        float f = VRGetEntWeaponFlags(self, xHand);
        if(self.health > 0 && !VR_FlatReload_Busy(self, xHand) && !VRIsHandWeaponless(self, xHand) &&
           VRGetEntWeaponClip(self, xHand) > 0)
        {
            if((f & QVR_WPNFLAG_NOMAG) && VR_FlatReload_MagEjects(xHand) && !(self.vr_mag_out & VR_FlatReload_Bit(xHand)))
            {
                VRSetEntWeaponFlags(self, xHand, f - QVR_WPNFLAG_NOMAG);
            }
            if((f & QVR_WPNFLAG_SSG_OPEN) && VR_FlatReload_SsgBreaks(self, xHand))
            {
                VRSetEntWeaponFlags(self, xHand, VRGetEntWeaponFlags(self, xHand) - QVR_WPNFLAG_SSG_OPEN);
            }
        }
        return;
    }
    if(!due)
    {
        float wait = VR_FlatReload_SsgBreaks(self, xHand) ? cvar_hget(cvarh_vr_flat_ssg_break_delay) : cvar_hget(cvarh_vr_flat_mag_eject_delay);
        due = time + max(0, wait);
        VR_FlatReload_SetDue(xHand, due);
    }
    if(time >= due - 0.001)
    {
        VR_FlatReload_EjectNow(xHand, "fired empty");
    }
};

// The trip's gun shut again (the super shotgun snapped shut, the revolver's cylinder home), if the trip opened it.
void(float xHand, string xWhy) VR_FlatReload_Shut =
{
    float main = xHand == cVR_MainHand;
    if(!(main ? self.vr_flatrl_shut1 : self.vr_flatrl_shut0))
    {
        return;
    }
    if(main)
    {
        self.vr_flatrl_shut1 = FALSE;
    }
    else
    {
        self.vr_flatrl_shut0 = FALSE;
    }
    string how = strcat("flat-screen play: ", xWhy);
    float w = VRGetEntWeapon(self, xHand);
    if(w == WID_SUPER_SHOTGUN && (VRGetEntWeaponFlags(self, xHand) & QVR_WPNFLAG_SSG_OPEN))
    {
        VR_Reload_SsgClose(self, xHand, how);
    }
    else if(w == WID_REVOLVER) // (swung out by this trip: VR_FlatReload_Start)
    {
        VR_Revolver_Close(xHand, VRGetEntWeaponInst(self, xHand), how);
    }
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
    // The gun opened for its rounds: the super shotgun broken open (already, or by the hip: VR_FlatReload_EjectFrame), the
    // revolver's cylinder swung out now, its spent shells thrown out.
    float shut = FALSE;
    float w = VRGetEntWeapon(self, xHand);
    if(VR_FlatReload_SsgBreaks(self, xHand))
    {
        shut = TRUE; // (VR_FlatReload_Shut: if it is open by then)
    }
    else if(w == WID_REVOLVER && cvar_hget(cvarh_vr_flat_revolver_eject))
    {
        entity r = VRGetEntWeaponInst(self, xHand);
        VR_Revolver_Open(xHand, r, "flat-screen play: the reload");
        vr_rv_keep_live = TRUE;
        VR_Revolver_Eject(xHand, r, "flat-screen play: the reload", '0 0 0');
        vr_rv_keep_live = FALSE;
        shut = TRUE;
    }
    if(xHand == cVR_MainHand)
    {
        self.vr_flatrl_shut1 = shut;
    }
    else
    {
        self.vr_flatrl_shut0 = shut;
    }
    dprint(sprintf("flat reload: start %s hand (%s): weapon %g, clip %g of %g, storage %g\n", VR_FlatReload_HandName(xHand), xWhy,
        VRGetEntWeapon(self, xHand), VRGetEntWeaponClip(self, xHand), VR_ClipSizeIn(self, xHand), VR_FlatReload_Storage(xHand)));
};

// The trip over (`xDone`: its rounds went in: a gun it opened shut now, if not yet; else, cancelled: left as it is, the
// revolver's cylinder swinging home by itself, vr_revolver.qc).
void(float xHand, float xDone) VR_FlatReload_Stop =
{
    if(xDone)
    {
        VR_FlatReload_Shut(xHand, "the reload's end");
    }
    if(xHand == cVR_MainHand)
    {
        self.vr_flatrl_shut1 = FALSE;
    }
    else
    {
        self.vr_flatrl_shut0 = FALSE;
    }
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
    if(VR_FlatPlay(self))
    {
        VR_FlatReload_EjectFrame(xHand);
    }
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
        VR_FlatReload_Stop(xHand, FALSE);
        dprint(sprintf("flat reload: %s hand cancelled: the weapon changed (%.2f s; weapon %g, clip %g of %g, storage %g)\n",
            VR_FlatReload_HandName(xHand), time - at, VRGetEntWeapon(self, xHand), VRGetEntWeaponClip(self, xHand),
            VR_ClipSizeIn(self, xHand), VR_FlatReload_Storage(xHand)));
        return;
    }

    float done = main ? self.vr_flatrl_done1 : self.vr_flatrl_done0;
    if(!done && time >= at + VR_FlatReload_DownTime() - 0.001)
    {
        // At the hip: what the empty gun still had to throw out goes first (its delay not up yet), then the rounds go in,
        // as VR's hip holster puts them in (VRTryReloadWeapon, its sounds and haptics), a new magazine seated with them.
        VR_FlatReload_EjectNow(xHand, "fired empty, reloading");
        vr_flatreload_go = TRUE;
        VRTryReloadWeapon(self, xHand, VRGetEntWeapon(self, xHand), VRGetEntWeaponFlags(self, xHand));
        vr_flatreload_go = FALSE;
        float f = VRGetEntWeaponFlags(self, xHand);
        if((f & QVR_WPNFLAG_NOMAG) && VRGetEntWeaponClip(self, xHand) > 0)
        {
            VR_Reload_MagOutRelease(self, xHand, "flat-screen play: the reload"); // (the nail pistol's, still sliding out)
            VRSetEntWeaponFlags(self, xHand, f - QVR_WPNFLAG_NOMAG);
            VR_Reload_Sound(self, VRGetGunChannel(xHand), "vr/reload_mag_in.wav", 1);
        }
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
    // Its gun shut again a moment after the rounds (the super shotgun, the revolver's cylinder), within the trip.
    float closeAt = min(max(0, cvar_hget(cvarh_vr_flat_reload_close_delay)), VR_FlatReload_HoldTime() + VR_FlatReload_UpTime() * 0.5);
    if((main ? self.vr_flatrl_done1 : self.vr_flatrl_done0) && time >= at + VR_FlatReload_DownTime() + closeAt - 0.001)
    {
        VR_FlatReload_Shut(xHand, "the reload");
    }
    if(time >= at + VR_FlatReload_DownTime() + VR_FlatReload_HoldTime() + VR_FlatReload_UpTime() - 0.001)
    {
        VR_FlatReload_Stop(xHand, TRUE);
        dprint(sprintf("flat reload: %s hand done (%.2f s): clip %g of %g, storage %g\n", VR_FlatReload_HandName(xHand), time - at,
            VRGetEntWeaponClip(self, xHand), VR_ClipSizeIn(self, xHand), VR_FlatReload_Storage(xHand)));
    }
};
