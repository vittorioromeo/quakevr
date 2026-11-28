#ifdef QVR_TESTS
//
//
//
// ----------------------------------------------------------------------------
// vr_reload_test 51-54: the ammo follows the rounds loaded (ammomags; ROUND26.md "ammomags"; vr_reload.qc
// VR_Reload_AmmoByMag)
// ----------------------------------------------------------------------------
//
// 51 (at once, impulse 125): immersive reloading in the headset: a nailgun's ammo switch refused; the pouch in halves
// (kind 2 on the right, 2 + 8 on the left, both counts); the left half gives a lava nails' magazine, seated it makes the
// gun fire lava nails (its model, its HUD ammo, a lava_spike out of W_Attack); the right half a plain one. The rocket
// launcher and the grenade launcher: the left half's multi-round refused while plain rounds are in (no mixed loads),
// taken once empty (the launcher fires multi-rounds), then a plain one refused. The simple mode and flat play keep the
// switch (its flags flip) and a whole pouch. Ends with "ammomags: N passed, M failed".
// 52, 53, 54: the mock's end to end (Misc/quakevr/ammomags_test.sh): 52 an empty nailgun in the off hand, the main
// hand empty, 50 nails and 30 lava nails; 53 after the main hand took from the pouch's left half and seated it: the gun
// fires lava nails; 54 after it took from the right half: the hand holds a plain magazine.

float vr_amt_mode, vr_amt_holster, vr_amt_front, vr_amt_flat;

void() VR_AMT_Save =
{
    vr_amt_mode = cvar_hget(cvarh_vr_reload_mode);
    vr_amt_holster = cvar_hget(cvarh_vr_holster_mode);
    vr_amt_front = cvar_hget(cvarh_vr_reload_front);
    vr_amt_flat = self.vr_flatscreen;
    cvar_set("vr_reload_mode", ftos(QVR_RELOADMODE_IMMERSIVE));
    cvar_set("vr_holster_mode", ftos(QVR_WEAPONMODE_IMMERSIVE));
    cvar_set("vr_reload_front", "1");
    cvar_set("vr_reload_mode_nail", "-1");
    cvar_set("vr_reload_mode_gl", "-1");
    cvar_set("vr_reload_mode_rl", "-1");
};

void() VR_AMT_Restore =
{
    cvar_set("vr_reload_mode", ftos(vr_amt_mode));
    cvar_set("vr_holster_mode", ftos(vr_amt_holster));
    cvar_set("vr_reload_front", ftos(vr_amt_front));
    self.vr_flatscreen = vr_amt_flat;
};

// A gun `xWid` (its flags `xFlags`, `xClip` rounds in it) in `xHand` of self.
void(float xHand, float xWid, float xFlags, float xClip) VR_AMT_Give =
{
    VR_WeaponUtil_EntAddWeapon(self, xWid);
    VRGiveEntWeapon(self, xHand, xWid, xFlags, xClip);
    W_SetCurrentAmmoFor(xHand);
};

// What `xHand` of self holds let go of and gone.
void(float xHand) VR_AMT_Drop =
{
    entity r = VR_Carry_Held(self, xHand);
    if(r != world)
    {
        VR_Carry_Release(xHand, r);
        remove(r);
    }
};

// `xHand` of self (empty) takes from the pouch with the hand in its left half (`xLeft`) or its right: the round taken.
entity(float xHand, float xLeft) VR_AMT_Take =
{
    VR_AMT_Drop(xHand);
    vector at = self.ammopouchpos + self.ammopouchy * (xLeft ? 0.5 : -0.5);
    if(xHand == cVR_MainHand)
    {
        self.handpos = at;
    }
    else
    {
        self.offhandpos = at;
    }
    VR_Reload_Take(xHand);
    return VR_Carry_Held(self, xHand);
};

float(string xClass) VR_AMT_Count =
{
    float n = 0;
    entity e = find(world, classname, xClass);
    while(e)
    {
        n++;
        e = find(e, classname, xClass);
    }
    return n;
};

// The launcher `xWid` in the main hand: the left half's multi-round refused while plain rounds are in, taken once empty,
// then a plain round refused.
void(float xWid) VR_AMT_Launcher =
{
    string name = WeaponIdToWeaponName(xWid);
    VR_AMT_Give(cVR_MainHand, xWid, 0, 2);
    self.ammo_rockets = 10;
    self.ammo_multi_rockets = 5;
    VR_Reload_PlayerFrame();
    float k = xWid == WID_ROCKET_LAUNCHER ? 5 : 6;
    VR_Test_Check(self.vr_pouch_kind == k && self.vr_pouch_kind2 == k + 8 && self.vr_pouch_count2 == 5,
        sprintf("%s: the pouch in halves (kind %g/%g, want %g/%g; %g multi-rounds)", name, self.vr_pouch_kind,
        self.vr_pouch_kind2, k, k + 8, self.vr_pouch_count2));
    entity m = VR_AMT_Take(cVR_OffHand, TRUE);
    VR_Test_Check(m != world && m.vr_ammo_aid == AID_MULTI_ROCKETS && self.ammo_multi_rockets == 4,
        sprintf("%s: the left half gives a multi-round (ammo %g, %g left)", name, m.vr_ammo_aid, self.ammo_multi_rockets));
    VR_Test_Check(!VR_Reload_GunTakesAid(self, cVR_MainHand, AID_MULTI_ROCKETS) && vr_reload_mixed,
        sprintf("%s: refused with 2 rockets in (no mixed loads)", name));
    VRSetEntWeaponClip(self, cVR_MainHand, 0); // (fired empty)
    VR_Test_Check(VR_Reload_GunTakesAid(self, cVR_MainHand, AID_MULTI_ROCKETS), sprintf("%s: taken once empty", name));
    if(m != world)
    {
        VR_Reload_LoadInto(cVR_MainHand, m, TRUE);
    }
    VR_Test_Check(VRGetEntWeaponClip(self, cVR_MainHand) == 1 &&
        (VRGetEntWeaponFlags(self, cVR_MainHand) & QVR_WPNFLAG_USE_SECONDARY_AMMO) && self.currentammo == AID_MULTI_ROCKETS,
        sprintf("%s: loaded, it fires multi-rounds (clip %g, flags %g, ammo %g)", name, VRGetEntWeaponClip(self, cVR_MainHand),
        VRGetEntWeaponFlags(self, cVR_MainHand), self.currentammo));
    entity p2 = VR_AMT_Take(cVR_OffHand, FALSE);
    VR_Test_Check(p2 != world && p2.vr_ammo_aid == AID_ROCKETS && !VR_Reload_GunTakesAid(self, cVR_MainHand, AID_ROCKETS) &&
        vr_reload_mixed, sprintf("%s: the right half's plain round refused with a multi-round in", name));
    VR_AMT_Drop(cVR_OffHand);
};

void() VR_AmmoMags_Test =
{
    VR_Test_BeginFlags("ammomags", VR_TEST_LOUD);
    VR_AMT_Save();
    vector pos0 = self.ammopouchpos;
    vector y0 = self.ammopouchy;
    if(self.ammopouchz == '0 0 0')
    {
        self.ammopouchpos = self.origin + '8 0 -4'; // (no pouch from the client yet: one in front of the belly, its +y left)
        self.ammopouchy = '0 3 0';
    }
    float step = cvar_hget(cvarh_vr_reload_test);
    VRGiveEntWeapon(self, cVR_OffHand, WID_FIST, 0, 0);
    if(step == 52)
    {
        VRGiveEntWeapon(self, cVR_MainHand, WID_FIST, 0, 0);
        VR_AMT_Give(cVR_OffHand, WID_NAILGUN, QVR_WPNFLAG_NOMAG, 0);
        self.ammo_nails = 50;
        self.ammo_lava_nails = 30;
        VR_Test_Check(VR_Reload_AmmoByMag(self, cVR_OffHand), "ready: an empty nailgun in the off hand, its ammo by its magazine");
        VR_Test_Verdict();
        return; // (the settings kept for the mock's moves; the script puts them back)
    }
    if(step == 53 || step == 54)
    {
        float f = VRGetEntWeaponFlags(self, cVR_OffHand);
        if(step == 53)
        {
            VR_Test_Check((f & QVR_WPNFLAG_USE_SECONDARY_AMMO) && !(f & QVR_WPNFLAG_NOMAG) && self.currentammo2 == AID_LAVA_NAILS &&
                VRGetEntWeaponClip(self, cVR_OffHand) > 0 && self.ammo_lava_nails < 30 && self.ammo_nails == 50,
                sprintf("the left half's magazine seated: the gun fires lava nails (flags %g, ammo %g, clip %g; lava %g, nails %g)",
                f, self.currentammo2, VRGetEntWeaponClip(self, cVR_OffHand), self.ammo_lava_nails, self.ammo_nails));
        }
        else
        {
            entity h = VR_Carry_Held(self, cVR_MainHand);
            VR_Test_Check(h != world && h.vr_ammo_aid == AID_NAILS && h.vr_ammo_wid == WID_NAILGUN && h.skin == 0,
                sprintf("the right half gives a plain nailgun magazine (ammo %g, skin %g)", h.vr_ammo_aid, h.skin));
            VR_AMT_Restore();
        }
        VR_Test_Verdict();
        return;
    }

    // A nailgun, empty, in the main hand; the pouch for it.
    VR_AMT_Give(cVR_MainHand, WID_NAILGUN, QVR_WPNFLAG_NOMAG, 0);
    self.ammo_nails = 50;
    self.ammo_lava_nails = 30;
    if(!VR_Reload_AmmoByMag(self, cVR_MainHand))
    {
        VR_Test_Check(FALSE, "the nailgun's ammo follows its magazine (no lava nails in this game?)");
        VR_AMT_Restore();
        VR_Test_Verdict();
        return;
    }
    float f0 = VRGetEntWeaponFlags(self, cVR_MainHand);
    ToggleSecondaryAmmo(cVR_MainHand);
    VR_Test_Check(VRGetEntWeaponFlags(self, cVR_MainHand) == f0, "the ammo switch refused (the rounds loaded set the ammo)");
    VR_Reload_PlayerFrame();
    VR_Test_Check(self.vr_pouch_kind == 2 && self.vr_pouch_kind2 == 10 && self.vr_pouch_count == 50 && self.vr_pouch_count2 == 30,
        sprintf("the pouch in halves: nailgun magazines right (kind %g, %g nails), lava ones left (kind %g, %g)",
        self.vr_pouch_kind, self.vr_pouch_count, self.vr_pouch_kind2, self.vr_pouch_count2));
    entity r = VR_AMT_Take(cVR_OffHand, TRUE);
    float size = VR_ClipSizeIn(self, cVR_MainHand);
    VR_Test_Check(r != world && r.vr_ammo_aid == AID_LAVA_NAILS && r.vr_ammo_wid == WID_NAILGUN && r.skin == 1 &&
        r.vr_ammo_count == min(size, 30) && self.ammo_lava_nails == 30 - r.vr_ammo_count && self.ammo_nails == 50,
        sprintf("the left half gives a lava magazine (ammo %g, %g in it, skin %g; lava %g, nails %g)", r.vr_ammo_aid,
        r.vr_ammo_count, r.skin, self.ammo_lava_nails, self.ammo_nails));
    VR_Test_Check(r != world && VR_Reload_MagFits(self, cVR_MainHand, r), "it fits the nailgun firing plain nails");
    if(r != world)
    {
        VR_Reload_SeatMagInto(cVR_MainHand, r, TRUE);
    }
    float f1 = VRGetEntWeaponFlags(self, cVR_MainHand);
    VR_Test_Check((f1 & QVR_WPNFLAG_USE_SECONDARY_AMMO) && !(f1 & QVR_WPNFLAG_NOMAG) && self.currentammo == AID_LAVA_NAILS &&
        self.weaponmodel == "progs/v_lava.mdl" && VRGetEntWeaponClip(self, cVR_MainHand) == min(size, 30),
        sprintf("seated: it fires lava nails (flags %g, ammo %g, %s, clip %g)", f1, self.currentammo, self.weaponmodel,
        VRGetEntWeaponClip(self, cVR_MainHand)));
    float spikes = VR_AMT_Count("lava_spike");
    float clip = VRGetEntWeaponClip(self, cVR_MainHand);
    self.attack_finished = 0;
    W_Attack();
    VR_Test_Check(VR_AMT_Count("lava_spike") > spikes && VRGetEntWeaponClip(self, cVR_MainHand) < clip,
        sprintf("fired: a lava nail out (%g lava_spike, was %g), clip %g -> %g", VR_AMT_Count("lava_spike"), spikes, clip,
        VRGetEntWeaponClip(self, cVR_MainHand)));
    entity r2 = VR_AMT_Take(cVR_OffHand, FALSE);
    VR_Test_Check(r2 != world && r2.vr_ammo_aid == AID_NAILS && r2.skin == 0 && VR_Reload_MagFits(self, cVR_MainHand, r2),
        sprintf("the right half gives a plain magazine that fits (ammo %g)", r2.vr_ammo_aid));
    // (Its lava magazine out as an eject leaves it: then the plain one seats and it fires nails again.)
    VRSetEntWeaponClip(self, cVR_MainHand, 0);
    VRSetEntWeaponFlags(self, cVR_MainHand, VRGetEntWeaponFlags(self, cVR_MainHand) | QVR_WPNFLAG_NOMAG);
    if(r2 != world)
    {
        VR_Reload_SeatMagInto(cVR_MainHand, r2, TRUE);
    }
    VR_Test_Check(!(VRGetEntWeaponFlags(self, cVR_MainHand) & QVR_WPNFLAG_USE_SECONDARY_AMMO) && self.currentammo == AID_NAILS &&
        self.weaponmodel == "progs/v_nail.mdl", sprintf("the plain one seated: nails again (%s)", self.weaponmodel));

    VR_AMT_Launcher(WID_ROCKET_LAUNCHER);
    VR_AMT_Launcher(WID_GRENADE_LAUNCHER);

    // The simple mode (the hip holsters) and flat play: the switch as before, a whole pouch.
    cvar_set("vr_reload_mode", "2");
    VR_AMT_Give(cVR_MainHand, WID_NAILGUN, 0, 10);
    VR_Reload_PlayerFrame();
    VR_Test_Check(!VR_Reload_AmmoByMag(self, cVR_MainHand) && self.vr_pouch_kind2 == 0, "simple mode: no halves");
    ToggleSecondaryAmmo(cVR_MainHand);
    VR_Test_Check(VRGetEntWeaponFlags(self, cVR_MainHand) & QVR_WPNFLAG_USE_SECONDARY_AMMO, "simple mode: the switch works");
    cvar_set("vr_reload_mode", ftos(QVR_RELOADMODE_IMMERSIVE));
    self.vr_flatscreen = 1;
    VR_AMT_Give(cVR_MainHand, WID_NAILGUN, 0, 10);
    VR_Test_Check(!VR_Reload_AmmoByMag(self, cVR_MainHand), "flat play: the ammo by N, not by the magazine");
    ToggleSecondaryAmmo(cVR_MainHand);
    VR_Test_Check(VRGetEntWeaponFlags(self, cVR_MainHand) & QVR_WPNFLAG_USE_SECONDARY_AMMO, "flat play: N switches");
    self.vr_flatscreen = vr_amt_flat;

    VR_AMT_Drop(cVR_OffHand);
    self.ammopouchpos = pos0;
    self.ammopouchy = y0;
    VR_AMT_Restore();
    VR_Test_Verdict();
};
#endif
