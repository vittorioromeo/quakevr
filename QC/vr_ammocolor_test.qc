// vr_ammocolor_test.qc -- the ammo screens' warning colours (vr_ammo_color_warn; vr_view_weapon.cpp ammoWarning).
//
// vr_ammocolor_test 1: guns put in your hands (the shotgun, the nailgun, the nail pistol, the revolver and AD's
// Widowmaker), their magazine full, at a fifth and empty, the reserve full each time (the loaded rounds count, not the
// reserve); a nailgun in the off hand; then the option off (the screen's colour at empty and low). Each case waits for
// the readout to show it (the engine's vr_ammo_color_seen / _seen2, kept while vr_ammo_color_debug is on: loaded *
// 10000 + capacity * 10 + state) and checks its state: 0 the screen's colour, 1 low, 2 empty. Ends with
// "ammocolor: N passed, M failed".
// vr_ammocolor_test 2: the eyeshot's scene: a shotgun at a fifth in the main hand (low), an empty nailgun in the off hand;
// 3: an empty nailgun in the main hand (the mock's off hand shows its screen edge on).

entity vr_act_player;
float vr_act_case;    // the case under way (-1: none)
float vr_act_frames;  // frames it has waited for the readout
float vr_act_pass, vr_act_fail;
float vr_act_hand, vr_act_loaded, vr_act_size, vr_act_expect;
float vr_act_warn;    // vr_ammo_color_warn as it was
float vr_act_debug;   // vr_ammo_color_debug as it was
float vr_act_grip;    // vr_weapon_grip_mode as it was (1 for the test: the mock's open hands keep the pistols)
float vr_act_npText, vr_act_rvText; // the nail pistol's and the revolver's Ammo Screen as they were (hidden by default:
                                    // shown for the test, the readout path being every gun's)

float VR_ACT_CASES = 19;

// Case `c`'s gun (0 shotgun, 1 nailgun, 2 nail pistol, 3 revolver, 4 the Widowmaker) and fill (0 full, 1 a fifth, 2 empty):
// 0..14 the guns in the main hand; 15, 16 a nailgun in the off hand at a fifth and empty; 17, 18 the option off, a
// shotgun empty and at a fifth.
float(float c) VR_ACT_Gun =
{
    if(c < 15) { return floor(c / 3); }
    if(c < 17) { return 1; }
    return 0;
};

float(float c) VR_ACT_Fill =
{
    if(c < 15) { return c - 3 * floor(c / 3); }
    if(c == 15 || c == 18) { return 1; }
    return 2;
};

float(float g) VR_ACT_Wid =
{
    if(g == 1) { return WID_NAILGUN; }
    if(g == 2) { return WID_NAILPISTOL; }
    if(g == 3) { return WID_REVOLVER; }
    if(g == 4) { return WID_SUPER_SHOTGUN; }
    return WID_SHOTGUN;
};

string(float g) VR_ACT_Name =
{
    if(g == 1) { return "nailgun"; }
    if(g == 2) { return "nail pistol"; }
    if(g == 3) { return "revolver"; }
    if(g == 4) { return "Widowmaker"; }
    return "shotgun";
};

// Gun `g` in `xHand` with `xFill` of its magazine loaded, the reserve full; its magazine's size (0: not set up).
float(float xHand, float g, float xFill) VR_ACT_Give =
{
    entity p = vr_act_player;
    entity os = self;
    self = p;
    p.ammo_shells = p.ammo_nails = p.ammo_rockets = p.ammo_cells = 100;
    VRGiveEntWeapon(p, xHand, VR_ACT_Wid(g), 0, 0);
    if(g == 4)
    {
        entity inst = VRGetEntWeaponInst(p, xHand);
        if(inst != world)
        {
            inst.wi_adup = inst.wi_adup | ad_IT_UPGRADE_SSG; // (AD's upgrade: the super shotgun is the Widowmaker)
        }
    }
    float size = VR_ClipSizeIn(p, xHand);
    float clip = size;
    if(xFill == 1) { clip = max(1, floor(size * 0.2)); }
    if(xFill == 2) { clip = 0; }
    VRSetEntWeaponClip(p, xHand, clip);
    W_SetCurrentAmmoFor(xHand);
    updateAmmoCounters();
    self = os;
    vr_act_loaded = clip;
    return size;
};

void() VR_ACT_End =
{
    dprint(sprintf("ammocolor: %g passed, %g failed\n", vr_act_pass, vr_act_fail));
    cvar_set("vr_ammo_color_warn", ftos(vr_act_warn));
    cvar_set("vr_ammo_color_debug", ftos(vr_act_debug));
    cvar_set("vr_weapon_grip_mode", ftos(vr_act_grip));
    cvar_set("vr_wofs_wpntxtmode_37", ftos(vr_act_npText));
    cvar_set("vr_wofs_wpntxtmode_39", ftos(vr_act_rvText));
    vr_act_case = -1;
};

void() VR_ACT_Start =
{
    float c = vr_act_case;
    float g = VR_ACT_Gun(c);
    float fill = VR_ACT_Fill(c);
    float warn = c < 17;
    cvar_set("vr_ammo_color_warn", warn ? "1" : "0");
    vr_act_hand = (c == 15 || c == 16) ? cVR_OffHand : cVR_MainHand;
    cvar_set(vr_act_hand == cVR_OffHand ? "vr_ammo_color_seen2" : "vr_ammo_color_seen", "-1");
    vr_act_size = VR_ACT_Give(vr_act_hand, g, fill);
    if(g == 4 && vr_act_size <= 2)
    {
        dprint("ammocolor: SKIP the Widowmaker (AD's upgrades are not on here)\n");
        vr_act_case = vr_act_case + 3;
        if(vr_act_case >= VR_ACT_CASES) { VR_ACT_End(); return; }
        VR_ACT_Start();
        return;
    }
    vr_act_expect = 0;
    if(warn)
    {
        if(vr_act_loaded <= 0) { vr_act_expect = 2; }
        else if(vr_act_loaded < 0.25 * vr_act_size) { vr_act_expect = 1; }
    }
    vr_act_frames = 0;
};

void() VR_AmmoColor_TestFrame =
{
    float req = cvar("vr_ammocolor_test");
    if(req)
    {
        cvar_set("vr_ammocolor_test", "0");
        vr_act_player = find(world, classname, "player");
        if(!vr_act_player) { dprint("ammocolor: no player\n"); return; }
        if(req == 2)
        {
            VR_ACT_Give(cVR_MainHand, 0, 1);
            VR_ACT_Give(cVR_OffHand, 1, 2);
            dprint("ammocolor: scene: a shotgun at a fifth in the main hand, an empty nailgun in the off hand\n");
            return;
        }
        if(req == 3)
        {
            VR_ACT_Give(cVR_MainHand, 1, 2);
            dprint("ammocolor: scene: an empty nailgun in the main hand\n");
            return;
        }
        vr_act_pass = vr_act_fail = 0;
        vr_act_warn = cvar("vr_ammo_color_warn");
        vr_act_debug = cvar("vr_ammo_color_debug");
        vr_act_grip = cvar("vr_weapon_grip_mode");
        cvar_set("vr_weapon_grip_mode", "1");
        vr_act_npText = cvar("vr_wofs_wpntxtmode_37");
        vr_act_rvText = cvar("vr_wofs_wpntxtmode_39");
        cvar_set("vr_wofs_wpntxtmode_37", "1");
        cvar_set("vr_wofs_wpntxtmode_39", "1");
        cvar_set("vr_ammo_color_debug", "1");
        vr_act_case = 0;
        VR_ACT_Start();
        return;
    }
    if(vr_act_case < 0 || !vr_act_player)
    {
        return;
    }
    // The readout showing this case: its loaded rounds (0 with the option off) and its magazine's size.
    float seen = cvar(vr_act_hand == cVR_OffHand ? "vr_ammo_color_seen2" : "vr_ammo_color_seen");
    float wantLoaded = cvar("vr_ammo_color_warn") ? vr_act_loaded : 0;
    float loaded = floor(seen / 10000);
    float size = floor((seen - loaded * 10000) / 10);
    float state = seen - loaded * 10000 - size * 10;
    vr_act_frames = vr_act_frames + 1;
    if(seen < 0 || loaded != wantLoaded || size != vr_act_size)
    {
        if(vr_act_frames > 90) // (a safety net: the readout shows a case within a few frames)
        {
            dprint(sprintf("ammocolor: FAIL case %g (%s): timed out, the readout at %g\n", vr_act_case,
                VR_ACT_Name(VR_ACT_Gun(vr_act_case)), seen));
            vr_act_fail = vr_act_fail + 1;
        }
        else
        {
            return;
        }
    }
    else
    {
        float ok = state == vr_act_expect;
        string what = sprintf("case %g: %s, %s hand, %g of %g loaded", vr_act_case, VR_ACT_Name(VR_ACT_Gun(vr_act_case)),
            vr_act_hand == cVR_OffHand ? "off" : "main", vr_act_loaded, vr_act_size);
        dprint(sprintf("ammocolor: %s %s%s: state %g (want %g)\n", ok ? "PASS" : "FAIL", what,
            cvar("vr_ammo_color_warn") ? "" : ", the option off", state, vr_act_expect));
        if(ok) { vr_act_pass = vr_act_pass + 1; } else { vr_act_fail = vr_act_fail + 1; }
    }
    vr_act_case = vr_act_case + 1;
    if(vr_act_case >= VR_ACT_CASES)
    {
        VR_ACT_End();
        return;
    }
    VR_ACT_Start();
};
