#ifdef QVR_TESTS
// vr_revgrip_test.qc -- the reverse ("blade down") grip (vr_revgrip.qc; ROUND25.md, "revgrip"), by the embedded blades' test
// machinery (vr_embed_test.qc: VR_EH_Blow, the mock hand's real blows). `vr_embed_test 42` (map vrtesthall, god, notarget, on your
// feet in the open), the main hand:
//   0 the dagger given upright: drawn upright (the engine's flip 0), its axes in the hand and its blade's way noted;
//   1 a dagger falling blade down (to the little finger's side) caught by the closed fist: held reversed; drawn as the upright
//     grip turned half round in the hand (180 degrees), its blade's line the other way;
//   2 the same blade up (to the thumb's side): caught upright;
//   3 a force grab's catch of a dagger lying blade down: upright (his note 1);
//   4 the eject button (B) on the upright dagger: the grip flips, the engine turning it over frames (vr_revgrip_flip_time), a
//     sound (the flips counted); again, back upright;
//   5 reversed, holstered at the right hip and drawn: still reversed, drawn so at once (his note 3);
//   6 the knight's sword held reversed, stabbed down into a grunt: stuck by its point; the hand on its handle in the body drawn as
//     it held it (the engine's vr_embed_probe_hoff, _hturn: the stuck blade's reversed bit, STAT_QVR_EMBED*).
// Ends `revgrip: N passed, M failed`.

vector vr_rg_f0, vr_rg_u0;   // case 0: the upright dagger's axes in the hand (weapondrawnpose 44, 45)
vector vr_rg_blade;          // ... its blade's way (the melee's line: from the grip to the tip)
float vr_rg_frames;          // case 4: frames since the button
float vr_rg_mid;             // ... frames drawn part way through the flip
float vr_rg_last;            // ... the flip drawn last frame
float vr_rg_back;            // ... frames it went back
float vr_rg_flips0;          // ... the flips counted before

float() VR_RG_Dagger = { return VR_ADMelee_Flags(VR_SWORDKIND_DAGGER); };

vector() VR_RG_Line =
{
    entity b = VR_Blow(cVR_MainHand);
    return normalize(b.mh_far - b.mh_grip);
};

// A dagger at `xAt` going `xVel`, its line (from its handle to its tip) along `xLine` (as VR_ET_ThrowSword lays one).
entity(vector xAt, vector xLine, vector xVel) VR_RG_DaggerAt =
{
    entity t = CreateThrownWeapon(self, WID_SWORD, VR_RG_Dagger(), WeaponInst_New(0), xAt, '0 0 0', '0 0 0', '0 0 0', FALSE, FALSE, 0);
    if(t == world)
    {
        return world;
    }
    vector m0 = normalize(weaponmuzzle(t) - t.origin);
    remove(t);
    vector k = VR_Cross(m0, xLine);
    float deg = atan2(vlen(k), m0 * xLine) * (180 / 3.14159265);
    k = vlen(k) < 0.001 ? '0 0 1' : normalize(k);
    vector fw = VR_AxeStick_Turn('1 0 0', k, deg);
    vector up = VR_AxeStick_Turn('0 0 1', k, deg);
    entity w = CreateThrownWeapon(self, WID_SWORD, VR_RG_Dagger(), WeaponInst_New(0), xAt, VR_AxeStick_Angles(fw, up), xVel, '0 0 0',
        FALSE, FALSE, 0);
    w.vr_wpn_from = self;
    return w;
};

// The angle (degrees) between two turns given by their forward and up axes.
float(vector f0, vector u0, vector f1, vector u1) VR_RG_TurnBetween =
{
    vector l0 = VR_Cross(u0, f0);
    vector l1 = VR_Cross(u1, f1);
    float tr = f0 * f1 + u0 * u1 + l0 * l1;
    float c = max(-1, min(1, (tr - 1) * 0.5));
    return atan2(sqrt(max(0, 1 - c * c)), c) * (180 / 3.14159265);
};

float() VR_RG_Flip =
{
    vector v = weapondrawnpose(self, cVR_MainHand, 46);
    return v_x;
};
float() VR_RG_Reversed = { return (VRGetEntWeaponFlags(self, cVR_MainHand) & QVR_WPNFLAG_REVGRIP) != 0; };

// `vr_embed_test 43`: the pictures' scene: the dagger held before your eyes upright, then reversed (a screenshot of each; ends
// `revgripshot: ready`). (vr_rg_scene: vr_embed_test.qc's.)
void() VR_RG_Scene =
{
    entity p = self;
    float t = time - vr_et_t0;
    float h = cVR_MainHand;
    makevectors(p.v_viewangle);
    vector eye = VRGetHeadPos(p);
    if(vr_et_s == 0)
    {
        localcmd("+grabmain\n");
        // (Before your eyes, a little to the right, aiming ahead and up to the right: the blade up, or down behind the fist.)
        localcmd(sprintf("vr_mock_hand_to main %v\n", eye + v_forward * 20 + v_right * 3 + v_up * 2));
        localcmd(sprintf("vr_mock_hand_aim main %v\n", eye + v_forward * 400 + v_right * 200 + v_up * 60));
        VR_ET_To(1);
    }
    else if(vr_et_s == 1 && VR_HandGrabUtil_IsHandGrabbing(p, h) && t > 0.3)
    {
        VR_EH_Give(WID_SWORD, VR_ADMelee_Flags(VR_SWORDKIND_DAGGER));
        VR_ET_To(2);
    }
    else if(vr_et_s == 2 && t > 0.5)
    {
        bprint(sprintf("revgripshot: the tip %.2f units from the hand\n", vlen(VR_Blow(h).mh_far - VRGetHandPos(h))));
        localcmd("screenshot\n");
        VR_ET_To(3);
    }
    else if(vr_et_s == 3 && t > 0.2)
    {
        VRSetEntWeaponFlags(p, h, VR_ADMelee_Flags(VR_SWORDKIND_DAGGER) | QVR_WPNFLAG_REVGRIP);
        VR_ET_To(4);
    }
    else if(vr_et_s == 4 && t > 0.5)
    {
        localcmd("screenshot\n");
        VR_ET_To(5);
    }
    else if(vr_et_s == 5 && t > 0.2)
    {
        bprint(sprintf("revgripshot: the tip %.2f units from the hand\n", vlen(VR_Blow(h).mh_far - VRGetHandPos(h))));
        bprint("revgripshot: ready\n");
        vr_et_on = 0;
    }
};

void() VR_RG_Begin =
{
    VR_Test_BeginFlags("revgrip", VR_TEST_LOUD);
    cvar_set("vr_knockdown", "0");
    cvar_set("vr_hit_stagger", "0");
    cvar_set("vr_embed_probe", "");
    vr_eh_home = VR_Player().origin;
    localcmd("vr_mock_hand_to_absolute 1\n");
    vr_et_on = 42;
    vr_et_case = 0;
    VR_ET_To(0);
};

// Cases 1-3: a dagger caught by the main hand; `xDown`: its blade to the little finger's side; `xForce`: by a force grab's catch.
void(float xDown, float xForce, float t) VR_RG_Catch =
{
    entity p = self;
    float h = cVR_MainHand;
    if(vr_et_s == 0)
    {
        localcmd("-grabmain\n");
        VRSetHandEmpty(p, h);
        W_SetCurrentAmmoFor(h);
        VR_ET_To(1);
    }
    else if(vr_et_s == 1 && t > 0.2 && !VR_HandGrabUtil_IsHandGrabbing(p, h))
    {
        localcmd("+grabmain\n");
        VR_ET_To(2);
    }
    else if(vr_et_s == 2 && VR_HandGrabUtil_IsHandGrabbing(p, h) && xForce)
    {
        // (A force grab's catch: the dagger lying blade down at the hand, taken as the catch takes it.)
        entity d = VR_RG_DaggerAt(VRGetHandPos(h), vr_rg_blade * -1, '0 0 0');
        forcegrabCatching = TRUE;
        wpnthrow_handtouch_impl(h, d, p, VRGetEntWeaponPtr(p, h), VRGetEntWeaponFlagsPtr(p, h));
        forcegrabCatching = FALSE;
        self = p;
        VR_ET_To(3);
    }
    else if(vr_et_s == 2 && VR_HandGrabUtil_IsHandGrabbing(p, h))
    {
        // Falling into the closed fist, its handle first: a mid-air catch (wpnthrow_midair_catch).
        vr_et_w = VR_RG_DaggerAt(VRGetHandPos(h) + '0 0 5', xDown ? vr_rg_blade * -1 : vr_rg_blade, '0 0 -120');
        VR_ET_To(3);
    }
    else if(vr_et_s == 3 && (VRGetEntWeapon(p, h) == WID_SWORD || t > 1))
    {
        string how = xForce ? "a force grab's catch of a dagger lying blade down" :
                     (xDown ? "a dagger caught blade down" : "a dagger caught blade up");
        VR_Test_Check(VRGetEntWeapon(p, h) == WID_SWORD, sprintf("%s: in the hand (holds %g)", how, VRGetEntWeapon(p, h)));
        float want = xDown && !xForce;
        VR_Test_Check(VR_RG_Reversed() == want, sprintf("%s: held %s", how, want ? "reversed" : "upright"));
        VR_ET_To(4);
    }
    else if(vr_et_s == 4 && t > 0.3)
    {
        float want2 = xDown && !xForce;
        float flip = VR_RG_Flip();
        vector f1 = weapondrawnpose(p, h, 44);
        vector u1 = weapondrawnpose(p, h, 45);
        float turn = VR_RG_TurnBetween(vr_rg_f0, vr_rg_u0, f1, u1);
        float along = VR_RG_Line() * vr_rg_blade;
        if(want2)
        {
            VR_Test_Check(flip > 0.99, sprintf("drawn reversed at once (the engine's flip %.2f)", flip));
            VR_Test_Check(turn > 178, sprintf("the hand-to-handle turn is the upright grip's turned half round (%.1f degrees)", turn));
            VR_Test_Check(along < -0.9, sprintf("its blade's line the other way (cos %.2f)", along));
        }
        else
        {
            VR_Test_Check(flip < 0.01 && turn < 2 && along > 0.95, sprintf("drawn upright (flip %.2f, %.1f degrees off the upright grip, "
                "line cos %.2f)", flip, turn, along));
        }
        VR_ET_NextCase();
    }
};

void() VR_RG_Frame =
{
    entity p = self;
    float t = time - vr_et_t0;
    float h = cVR_MainHand;
    if(vr_rg_scene)
    {
        VR_RG_Scene();
        return;
    }
    if(t > VR_ET_GIVEUP)
    {
        VR_Test_Check(FALSE, sprintf("timed out: case %g state %g", vr_et_case, vr_et_s));
        VR_EH_NextCase();
        return;
    }
    // ---- 0: upright, noted
    if(vr_et_case == 0)
    {
        if(vr_et_s == 0)
        {
            localcmd("+grabmain\n");
            VR_ET_To(1);
        }
        else if(vr_et_s == 1 && VR_HandGrabUtil_IsHandGrabbing(p, h) && t > 0.1)
        {
            VR_EH_Give(WID_SWORD, VR_RG_Dagger());
            VR_ET_To(2);
        }
        else if(vr_et_s == 2 && t > 0.4)
        {
            vr_rg_f0 = weapondrawnpose(p, h, 44);
            vr_rg_u0 = weapondrawnpose(p, h, 45);
            vr_rg_blade = VR_RG_Line();
            vector may = weapondrawnpose(p, h, 46);
            VR_Test_Check(vlen(vr_rg_f0) > 0.9 && !VR_RG_Reversed() && may_x < 0.01, sprintf("the dagger given: drawn upright (flip %.2f)", may_x));
            VR_Test_Check(may_y == 1, "the dagger may be held reversed (Weapon Offsets' Reverse Grip)");
            VR_Test_Print(sprintf("revgrip: the upright dagger's blade %v\n", vr_rg_blade));
            VR_ET_NextCase();
        }
        return;
    }
    // ---- 1, 2, 3: caught blade down, blade up, by a force grab
    if(vr_et_case >= 1 && vr_et_case <= 3)
    {
        VR_RG_Catch(vr_et_case != 2, vr_et_case == 3, t);
        return;
    }
    // ---- 4: the eject button flips it, over frames; again, back
    if(vr_et_case == 4)
    {
        if(vr_et_s == 0)
        {
            VRSetEntWeaponFlags(p, h, VR_RG_Dagger());
            VR_ET_To(1);
        }
        else if(vr_et_s == 1 && t > 0.3)
        {
            vr_rg_flips0 = vr_revgrip_flips;
            vr_rg_frames = 0;
            vr_rg_mid = 0;
            vr_rg_back = 0;
            vr_rg_last = 0;
            localcmd("vr_mock_button main secondary 1\n");
            VR_ET_To(2);
        }
        else if(vr_et_s == 2 || vr_et_s == 4)
        {
            float f = VR_RG_Flip();
            float target = vr_et_s == 2 ? 1 : 0;
            if(vr_rg_frames > 0)
            {
                vr_rg_mid = vr_rg_mid + (f > 0.01 && f < 0.99);
                vr_rg_back = vr_rg_back + (target ? f < vr_rg_last - 0.001 : f > vr_rg_last + 0.001);
            }
            vr_rg_last = f;
            vr_rg_frames = vr_rg_frames + 1;
            if(vr_rg_frames == 4)
            {
                localcmd("vr_mock_button main secondary 0\n");
            }
            if((vr_rg_frames > 6 && (target ? f > 0.999 : f < 0.001)) || vr_rg_frames > 120)
            {
                string way = target ? "upright to reversed" : "reversed to upright";
                VR_Test_Print(sprintf("revgrip: the flip %s drawn part way on %g frames (%g frames at 72/s for %.2f s)\n", way, vr_rg_mid,
                    vr_rg_frames, cvar_hget(cvarh_vr_revgrip_flip_time)));
                VR_Test_Check(VR_RG_Reversed() == target && vr_revgrip_flips == vr_rg_flips0 + 1,
                    sprintf("the eject button flips the grip %s (once: %g flips)", way, vr_revgrip_flips - vr_rg_flips0));
                VR_Test_Check(vr_rg_mid >= 6 && vr_rg_back == 0 && (target ? f > 0.999 : f < 0.001),
                    sprintf("turned over frames, smoothly: %g frames part way, %g back, ending at %.2f", vr_rg_mid, vr_rg_back, f));
                VR_ET_To(vr_et_s + 1);
            }
        }
        else if(vr_et_s == 3 && t > 0.2)
        {
            vr_rg_flips0 = vr_revgrip_flips;
            vr_rg_frames = 0;
            vr_rg_mid = 0;
            vr_rg_back = 0;
            vr_rg_last = 1;
            localcmd("vr_mock_button main secondary 1\n");
            VR_ET_To(4);
        }
        else if(vr_et_s == 5 && t > 0.2)
        {
            VR_ET_NextCase();
        }
        return;
    }
    // ---- 5: reversed, holstered and drawn
    if(vr_et_case == 5)
    {
        if(vr_et_s == 0)
        {
            VRSetEntWeaponFlags(p, h, VR_RG_Dagger() | QVR_WPNFLAG_REVGRIP);
            VR_ET_To(1);
        }
        else if(vr_et_s == 1 && t > 0.3)
        {
            VRHolsterHandWeapon(p, h, QVR_HS_RIGHT_HIP_HOLSTER);
            VR_Test_Check(VRIsHandWeaponless(p, h) && getHolsterWeapon(QVR_HS_RIGHT_HIP_HOLSTER) == WID_SWORD,
                "reversed, holstered at the right hip");
            VR_ET_To(2);
        }
        else if(vr_et_s == 2 && t > 0.3)
        {
            VRDrawHolsterWeapon(p, h, QVR_HS_RIGHT_HIP_HOLSTER);
            VR_Test_Check(VRGetEntWeapon(p, h) == WID_SWORD && VR_RG_Reversed(), "drawn again: still reversed");
            VR_ET_To(3);
        }
        else if(vr_et_s == 3 && t > 0.05)
        {
            // (A weapon new in the hand is drawn as the server says at once: no turn from upright.)
            float flip = VR_RG_Flip();
            VR_Test_Check(flip > 0.99, sprintf("drawn reversed from its first frames (flip %.2f)", flip));
            VR_EH_NextCase();
        }
        return;
    }
    // ---- 6: the knight's sword reversed, stabbed down into a grunt; the hand on its handle
    if(vr_et_case == 6)
    {
        if(vr_et_s == 0)
        {
            cvar_set("vr_embed_probe", "reset");
        }
        if(vr_et_s < 10)
        {
            if(vr_et_m != world && vr_et_m.health > 0)
            {
                vr_et_m.nextthink = time + 0.5; // (the body stands where it is: as test 40's)
                vr_et_m.velocity = '0 0 0';
            }
            vr_eh_flags = QVR_WPNFLAG_REVGRIP;
            vr_eh_down = 1.2;
            if(VR_EH_Blow(WID_SWORD, 6, 0, FALSE, t))
            {
                entity w = VR_EH_Blade();
                float stuck = w != world && VR_AxeStick_InMonster(w) && w.vr_stick_embed == AXESTICK_POINT;
                VR_Test_Check(stuck, sprintf("a reversed sword's downward stab at 6 m/s is stuck by its point (depth %.1f units)",
                    w != world ? w.vr_stick_depth : 0));
                VR_Test_Check(w == world || (w.weaponflags & QVR_WPNFLAG_REVGRIP), "the stuck blade keeps its reversed grip");
                VR_Test_Check(vr_eh_fw_z < -0.5, sprintf("the blow went down (its way %v)", vr_eh_fw));
                vr_eh_down = 0;
                if(w == world)
                {
                    VR_EH_NextCase();
                    return;
                }
                VR_ET_To(10);
            }
        }
        else if(vr_et_s == 10 && t > 0.6)
        {
            float hoff = cvar_hget(cvarh_vr_embed_probe_hoff);
            float hturn = cvar_hget(cvarh_vr_embed_probe_hturn);
            VR_Test_Check(cvar_hget(cvarh_vr_embed_probe_on) > 0 && hoff >= 0 && hoff < 0.3 && hturn < 2,
                sprintf("in the body: the hand on the reversed handle is drawn as it held the sword (%.2f cm, %.2f degrees off; under "
                "0.3, 2)", hoff, hturn));
            cvar_set("vr_embed_probe", "");
            VR_EH_NextCase();
        }
        return;
    }
    localcmd("-grabmain\nvr_mock_button main secondary 0\n");
    VR_Test_Verdict();
    vr_et_on = 0;
};
#endif
