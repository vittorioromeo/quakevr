#ifdef QVR_TESTS
//
//
//
// ----------------------------------------------------------------------------
// vr_np_test 8: the nail pistol's lava nails (nplava; ROUND26.md "nplava"; his words 2026-11-28 item 19)
// ----------------------------------------------------------------------------
//
// A nail pistol in the main hand, nails and lava nails in the pack. Without reloading (vr_reload_mode_npistol 0): its
// ammo switched by its own button (headset: the mock's off hand's fingertip brought to the button, as a finger presses
// it; flat play: N) to its lava nails (its lava model); three lava nails fired into a box: three of the lava nails used
// and none of the nails, each nail the nailguns' lava nail over their plain one times its own damage (as the box's
// health drops); switched back, a plain nail uses a nail. Then (headset only) immersive reloading (mode 3; ammomags):
// the button refused, the pouch in halves (kind 2 + 16 right, 2 + 16 + 8 left), a lava magazine is the magazine prop's
// skin 1, and seated it makes the pistol fire lava nails. Ends with
// "nplava: N passed, M failed"; each step waits on its condition (a give-up after 300 frames is a FAIL "timed out").

float vr_npl_on, vr_npl_step, vr_npl_frames, vr_npl_mode, vr_npl_grip, vr_npl_flat;
float vr_npl_nails0, vr_npl_lava0, vr_npl_health0, vr_npl_shots0, vr_npl_pulls;

void() VR_NPL_Next = { vr_npl_step++; vr_npl_frames = 0; };

void() VR_NPL_End =
{
    VR_NPT_Trigger(0);
    if(vr_np_target) { remove(vr_np_target); vr_np_target = world; }
    cvar_set("vr_reload_mode_npistol", ftos(vr_npl_mode));
    cvar_set("vr_weapon_grip_mode", ftos(vr_npl_grip));
    VR_NPT_Freeze(FALSE);
    VR_Test_Verdict();
    vr_npl_on = FALSE;
};

// Its ammo button pressed: flat play's N (vr_mock_key, the real key's path), else the off hand's fingertip brought in
// to the button from 6 units in front of it, as the reload test's (pressWeaponButtons: a press as it comes within reach).
void(float xFrame) VR_NPL_Press =
{
    if(vr_npl_flat)
    {
        if(xFrame == 1) localcmd("vr_mock_key n down\n");
        if(xFrame == 4) localcmd("vr_mock_key n up\n");
        return;
    }
    if(xFrame == 1) localcmd("vr_mock_hand_to off wbutton front 6\n");
    if(xFrame == 6) localcmd("vr_mock_hand_to off wbutton front 1\n");
};

// A nail pistol's magazine lying loose (a VR_RELOAD_MAGCLASS) not seen before (world: none); `xMark`: every one there
// marked seen instead.
.float vr_npl_seen;
entity(float xMark) VR_NPL_NewMag =
{
    entity e = find(world, classname, VR_RELOAD_MAGCLASS);
    while(e)
    {
        if(xMark) { e.vr_npl_seen = TRUE; }
        else if(e.vr_ammo_wid == WID_NAILPISTOL && !e.vr_npl_seen) { return e; }
        e = find(e, classname, VR_RELOAD_MAGCLASS);
    }
    return world;
};

// A loaded nail pistol in the main hand (its own ammo), the pack's nails and lava nails set.
void() VR_NPL_Give =
{
    entity p = vr_npt_player;
    VR_WeaponUtil_EntAddWeapon(p, WID_NAILPISTOL);
    VRGiveEntWeapon(p, cVR_MainHand, WID_NAILPISTOL, 0, VR_NailPistol_MagSize());
    entity os = self;
    self = p;
    W_SetCurrentAmmoFor(cVR_MainHand);
    self = os;
    p.ammo_nails = 50;
    p.ammo_lava_nails = 30;
};

// The player's rounds of ammo `xAid`: the pack's and those in the main hand's gun when it fires that ammo.
float(float xAid) VR_NPL_Total =
{
    entity p = vr_npt_player;
    float aid = WeaponIdToAmmoId(WID_NAILPISTOL, VRGetEntWeaponFlags(p, cVR_MainHand));
    float clip = (VRGetEntWeaponFlags(p, cVR_MainHand) & QVR_WPNFLAG_NOMAG) ? 0 : VRGetEntWeaponClip(p, cVR_MainHand);
    return VR_AmmoUtil_GetAmmoCountByAmmoId(p, xAid) + (aid == xAid ? clip : 0);
};

// Whether the main hand's gun can fire: flat play's switch ejects its magazine (VR_FlatReload_AmmoSwitch), so a
// magazine of the new ammo is seated here (its rounds from the pack: what a reload does; flat play's R itself is the
// flat reload tests'). Checked: the switch left it empty, its rounds back in the pack.
float() VR_NPL_Ready =
{
    entity p = vr_npt_player;
    float flags = VRGetEntWeaponFlags(p, cVR_MainHand);
    if(!vr_npl_flat || (VRGetEntWeaponClip(p, cVR_MainHand) >= 1 && !(flags & QVR_WPNFLAG_NOMAG)))
    {
        return TRUE;
    }
    VR_Test_Check((flags & QVR_WPNFLAG_NOMAG) != 0, "flat play: N ejected the magazine (the gun empty)");
    float aid = WeaponIdToAmmoId(WID_NAILPISTOL, flags);
    float n = min(VR_NailPistol_MagSize(), VR_AmmoUtil_GetAmmoCountByAmmoId(p, aid));
    VR_AmmoUtil_AddAmmoCountByAmmoId(p, aid, -n);
    VRSetEntWeaponClip(p, cVR_MainHand, n);
    VRSetEntWeaponFlags(p, cVR_MainHand, flags - QVR_WPNFLAG_NOMAG);
    return FALSE;
};

void() VR_NPL_Frame =
{
    entity p = vr_npt_player;
    vr_npl_frames++;
    if(vr_npl_frames > 300)
    {
        VR_Test_Check(FALSE, sprintf("step %g timed out", vr_npl_step));
        VR_NPL_End();
        return;
    }
    float lava = VRGetEntWeaponFlags(p, cVR_MainHand) & QVR_WPNFLAG_USE_SECONDARY_AMMO;
    if(vr_npl_step == 0)
    {
        cvar_set("vr_reload_mode_npistol", vr_npl_flat ? "-1" : "0"); // (flat play: its own reload, R)
        if(!vr_npl_flat) localcmd("vr_mock_hand main 0.03 1.69 -0.20 70 0 0\n"); // (aimed ahead: npaim's first pose)
        VR_NPL_Give();
        VR_NPL_Next();
        return;
    }
    if(vr_npl_step == 1)
    {
        if(vr_npl_frames < 3) return;
        entity os1 = self;
        self = p;
        VR_Test_Check(VR_HasOtherAmmo(cVR_MainHand), "the nail pistol has another ammo (its lava nails) to switch to");
        self = os1;
        VR_Test_Check(!lava && p.weaponmodel == "progs/v_nailpistol.mdl", sprintf("it starts on its nails (%s)", p.weaponmodel));
        VR_NPL_Next();
        return;
    }
    if(vr_npl_step == 2)
    {
        VR_NPL_Press(vr_npl_frames);
        if(!lava || vr_npl_frames < 8) return; // (the key let go, or the finger in, first)
        VR_Test_Check(p.weaponmodel == "progs/v_nailpistol_lava.mdl", sprintf("%s: its lava nails, its lava look (%s)",
            vr_npl_flat ? "N" : "its button pressed", p.weaponmodel));
        VR_Test_Check(p.currentammo == AID_LAVA_NAILS, sprintf("its ammo the lava nails (ammo id %g)", p.currentammo));
        if(!vr_npl_flat) localcmd("vr_mock_hand_to off wbutton front 9\n"); // (the finger away again)
        VR_NPL_Next();
        return;
    }
    if(vr_npl_step == 3)
    {
        if(!VR_NPL_Ready() || vr_npl_frames < 50) return; // (the finger out of the button's reach; a seated magazine racked)
        VR_NPT_Target(64);
        vr_npl_nails0 = VR_NPL_Total(AID_NAILS);
        vr_npl_lava0 = VR_NPL_Total(AID_LAVA_NAILS);
        vr_npl_health0 = vr_np_target.health;
        vr_npl_shots0 = vr_np_lavashots;
        vr_np_lavahits = 0;
        vr_npl_pulls = 0;
        VR_NPL_Next();
        return;
    }
    if(vr_npl_step == 4)
    {
        // Three pulls of the trigger (semi-automatic: a pull a shot), each held 4 frames, let go 30 (its cooldown).
        float ph = vr_npl_frames % 34;
        if(vr_npl_pulls < 3)
        {
            if(ph == 1) { VR_NPT_Trigger(1); }
            if(ph == 5) { VR_NPT_Trigger(0); vr_npl_pulls++; }
            return;
        }
        if(vr_np_lavahits < 3 && vr_npl_frames < 200) return; // (the last nail's flight)
        float shots = vr_np_lavashots - vr_npl_shots0;
        float each = cvar_hget(cvarh_vr_dmg_nailpistol) * cvar_hget(cvarh_vr_dmg_lava_nail) / max(1, cvar_hget(cvarh_vr_dmg_nail));
        float dealt = vr_npl_health0 - vr_np_target.health;
        VR_Test_Check(shots == 3, sprintf("three pulls: %g lava nails fired", shots));
        VR_Test_Check(vr_npl_lava0 - VR_NPL_Total(AID_LAVA_NAILS) == 3 && VR_NPL_Total(AID_NAILS) == vr_npl_nails0,
            sprintf("lava nails %g -> %g, nails %g -> %g (want 3 lava used, no nails)", vr_npl_lava0,
            VR_NPL_Total(AID_LAVA_NAILS), vr_npl_nails0, VR_NPL_Total(AID_NAILS)));
        VR_Test_Check(vr_np_lavahits == 3, sprintf("%g lava nails hit the box", vr_np_lavahits));
        VR_Test_Check(fabs(vr_np_lastdmg - each) < 0.01, sprintf("a lava nail's damage %g (want %g: vr_dmg_nailpistol %g x lava %g / nail %g)",
            vr_np_lastdmg, each, cvar_hget(cvarh_vr_dmg_nailpistol), cvar_hget(cvarh_vr_dmg_lava_nail), cvar_hget(cvarh_vr_dmg_nail)));
        VR_Test_Check(fabs(dealt - 3 * each * vr_np_lastfactor) < 0.05, sprintf("the box took %g (want %g)", dealt, 3 * each * vr_np_lastfactor));
        remove(vr_np_target);
        vr_np_target = world;
        VR_NPL_Next();
        return;
    }
    if(vr_npl_step == 5)
    {
        VR_NPL_Press(vr_npl_frames);
        if(lava || vr_npl_frames < 8) return;
        VR_Test_Check(p.weaponmodel == "progs/v_nailpistol.mdl", sprintf("pressed again: its nails, its own look (%s)", p.weaponmodel));
        if(!vr_npl_flat) localcmd("vr_mock_hand_to off wbutton front 9\n");
        vr_npl_pulls = 0;
        VR_NPL_Next();
        return;
    }
    if(vr_npl_step == 6)
    {
        if(!vr_npl_pulls)
        {
            if(vr_npl_frames < 12 || !VR_NPL_Ready()) return;
            vr_npl_nails0 = VR_NPL_Total(AID_NAILS);
            vr_npl_lava0 = VR_NPL_Total(AID_LAVA_NAILS);
            vr_npl_pulls = vr_npl_frames;
        }
        if(vr_npl_frames == vr_npl_pulls + 40) VR_NPT_Trigger(1); // (a magazine just seated: its slide racks first)
        if(vr_npl_frames == vr_npl_pulls + 44) VR_NPT_Trigger(0);
        if(vr_npl_frames < vr_npl_pulls + 70) return;
        VR_Test_Check(vr_npl_nails0 - VR_NPL_Total(AID_NAILS) == 1 && VR_NPL_Total(AID_LAVA_NAILS) == vr_npl_lava0,
            sprintf("a plain shot: nails %g -> %g, lava nails %g -> %g", vr_npl_nails0, VR_NPL_Total(AID_NAILS), vr_npl_lava0,
            VR_NPL_Total(AID_LAVA_NAILS)));
        if(vr_npl_flat) { VR_NPL_End(); return; } // (immersive reloading is the headset's)
        cvar_set("vr_reload_mode_npistol", "3");
        VR_NPL_Give();
        VR_NPL_Next();
        return;
    }
    if(vr_npl_step == 7)
    {
        if(vr_npl_frames < 3) return;
        VR_NPL_NewMag(TRUE);
        localcmd("impulse 43\n"); // (the main hand's weapon button: its press, refused: ammomags)
        VR_NPL_Next();
        return;
    }
    if(vr_npl_step == 8)
    {
        // (ammomags: immersive reloading in the headset: the rounds loaded set the ammo, the button refused, nothing out.)
        if(vr_npl_frames < 10) return;
        VR_Test_Check(!lava && VR_NPL_NewMag(FALSE) == world && !(VRGetEntWeaponFlags(p, cVR_MainHand) & QVR_WPNFLAG_NOMAG),
            "immersive: the button refused (the magazine loaded sets the ammo), its magazine kept in");
        VR_NPL_Next();
        return;
    }
    if(vr_npl_step == 9)
    {
        if(vr_npl_frames < 3) return; // (the pouch's kinds: VR_Reload_PlayerFrame)
        VR_Test_Check(p.vr_pouch_aid == AID_NAILS && p.vr_pouch_kind == 2 + 16 && p.vr_pouch_kind2 == 2 + 16 + 8,
            sprintf("the pouch in halves: nail pistol magazines right, lava ones left (kinds %g, %g: want 18, 26)",
            p.vr_pouch_kind, p.vr_pouch_kind2));
        entity r = VR_Reload_MakeRound(WID_NAILPISTOL, AID_LAVA_NAILS, VR_NailPistol_MagSize(), p.origin + '0 0 40', '0 0 0');
        VR_Test_Check(r.skin == 1 && r.model == "progs/vr_mag_npistol.mdl", sprintf("a lava magazine: %s skin %g (want 1)", r.model, r.skin));
        // (Its magazine out, the lava one seated: it fires lava nails.)
        VRSetEntWeaponClip(p, cVR_MainHand, 0);
        VRSetEntWeaponFlags(p, cVR_MainHand, VRGetEntWeaponFlags(p, cVR_MainHand) | QVR_WPNFLAG_NOMAG);
        VR_Reload_SeatMagInto(cVR_MainHand, r, FALSE);
        VR_Test_Check(VRGetEntWeaponFlags(p, cVR_MainHand) & QVR_WPNFLAG_USE_SECONDARY_AMMO,
            sprintf("the lava magazine seated: lava nails (%s)", p.weaponmodel));
        VR_NPL_End();
        return;
    }
};

void() VR_NPL_Begin =
{
    vr_npt_player = VR_Player();
    if(!vr_npt_player) { dprint("nplava: no player\n"); return; }
    VR_Test_Begin("nplava");
    if(!vr_have_lava)
    {
        VR_Test_Check(FALSE, "no lava nails here (Rogue's lava spike model)");
        VR_Test_Verdict();
        return;
    }
    vr_npl_flat = VR_FlatPlay(vr_npt_player);
    vr_npl_mode = cvar_hget(cvarh_vr_reload_mode_npistol);
    vr_npl_grip = cvar_hget(cvarh_vr_weapon_grip_mode);
    cvar_set("vr_weapon_grip_mode", "1"); // (the mock's hands keep what they hold)
    vr_npt_player.flags = vr_npt_player.flags | FL_NOTARGET;
    VR_NPT_Freeze(TRUE);
    vr_npl_step = 0;
    vr_npl_frames = 0;
    vr_npl_on = TRUE;
};

// VR_NailPistol_TestFrame's: a frame of the test while it runs (TRUE), else FALSE.
float() VR_NPL_Tick =
{
    if(!vr_npl_on) return FALSE;
    entity os = self;
    self = vr_npt_player;
    VR_NPL_Frame();
    self = os;
    return TRUE;
};
#endif
