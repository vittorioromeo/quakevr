#ifdef QVR_TESTS
// vr_flipgate_test.qc -- the flip grip's gate (ROUND25.md, "flipgate"; his note 2026-11-15: "whenever I'm holding a sword or
// dagger with two hands, it should only be possible to switch the grip by pressing the button if both hands are on the handle").
// `vr_embed_test 45` (map vrtesthall, god, notarget), by the flip grip test's machinery (vr_flipgrip_test.qc) and the mock hands.
// Each case: the weapon upright in the main hand, the off hand as the case says, then the flip (VR_RevGrip_Flip: the eject
// button's and the flat FLIP GRIP's):
//   0 the dagger in one hand: flips;            4 the knight's sword in one hand: flips;
//   1 the dagger, the off hand on its 2H grip hotspot (the handle): flips;   5 the sword the same: flips;
//   2 the dagger, the off hand near its tip ("anywhere"): refused;           6 the sword the same: refused;
//   3 (the dagger has no blade hotspot: skipped)                             7 the sword, the off hand on its blade (half-sword): refused;
//   8 the dagger held anywhere with vr_revgrip_2h_gate 0 (the old way): flips.
// Refused: the grip unchanged, the refusal counted (vr_revgrip_refused); weapondrawnpose 46's z -1 (1 when it may flip).
// Ends `flipgate: N passed, M failed`.

float vr_fgt_flips;   // vr_revgrip_flips before the case's flip
float vr_fgt_refused; // vr_revgrip_refused before it
float vr_fgt_rev;     // the grip reversed before it
vector vr_fgt_to;     // where the off hand closes

// The main hand's weapon's first hotspot of type `xType` (1 a grip, 2 a blade; -1 none).
float(float xType) VR_FGT_Spot =
{
    for(float i = 0; i < 4; i++)
    {
        if(weaponhotspotinfo(self, cVR_MainHand, i, 0) == xType)
        {
            return i;
        }
    }
    return -1;
};

void() VR_FGT_Begin =
{
    VR_Test_BeginFlags("flipgate", VR_TEST_LOUD);
    cvar_set("vr_knockdown", "0");
    cvar_set("vr_hit_stagger", "0");
    localcmd("tune vr_revgrip_2h_gate 1\n");
    vr_eh_home = VR_Player().origin;
    localcmd("vr_mock_hand_to_absolute 1\n");
    vr_fg_spot = -1;
    vr_et_on = 45;
    vr_et_case = 0;
    VR_ET_To(0);
};

void() VR_FGT_Frame =
{
    entity p = self;
    float t = time - vr_et_t0;
    float h = cVR_MainHand;
    float off = cVR_OffHand;
    float c = vr_et_case;
    if(c > 8)
    {
        localcmd("tune vr_revgrip_2h_gate 1\n");
        localcmd("-grabmain\n-graboff\n");
        VR_Test_Verdict();
        vr_et_on = 0;
        return;
    }
    if(t > VR_ET_GIVEUP)
    {
        VR_Test_Check(FALSE, sprintf("timed out: case %g state %g", c, vr_et_s));
        localcmd("-graboff\n");
        localcmd("tune vr_revgrip_2h_gate 1\n");
        VR_EH_NextCase();
        return;
    }
    float sword = c >= 4 && c <= 7;
    float k = c == 8 ? 2 : c - (sword ? 4 : 0); // 0 one hand, 1 the 2H hotspot, 2 anywhere, 3 the blade
    float refuse = (k == 2 || k == 3) && c != 8;
    string what = sprintf("%s, %s", sword ? "the knight's sword" : "the dagger",
        k == 0 ? "one hand" : (k == 1 ? "the off hand on its 2H grip hotspot" : (k == 2 ? "the off hand near its tip (anywhere)" : "the off hand on its blade (half-sword)")));
    if(c == 8)
    {
        what = strcat(what, ", vr_revgrip_2h_gate 0");
    }
    if(c == 3)
    {
        VR_Test_Print("flipgate: (case 3: the dagger has no blade hotspot: no half-sword grip to test)\n");
        VR_EH_NextCase();
        return;
    }
    vector eye = VRGetHeadPos(p);
    makevectors(p.v_viewangle);
    vector before = eye + v_forward * 20 + v_right * 4 - v_up * 10; // (the main hand's place, before you, a little low)
    // Held by the other hand as the case means: the dagger anywhere by a free grip (weaponanygrip 2); else two-handed. (The knight's
    // sword's blade hotspot runs to its tip: a hand on its blade away from the handle holds it by the blade, the half-sword grip,
    // which is his "anywhere" on a sword.)
    float held = VRGet2HAiming(p);
    if(k == 2 && !sword)
    {
        held = weaponanygrip(p, off) == 2;
    }
    if(vr_et_s == 0)
    {
        localcmd((c == 8) ? "tune vr_revgrip_2h_gate 0\n" : "tune vr_revgrip_2h_gate 1\n");
        localcmd(sprintf("-graboff\nvr_mock_hand_to off %v\nvr_mock_hand_to main %v\nvr_mock_hand_aim main %v\n+grabmain\n",
            before - v_right * 30 - v_up * 10, before, before + v_forward * 400));
        VR_ET_To(1);
    }
    else if(vr_et_s == 1 && VR_HandGrabUtil_IsHandGrabbing(p, h) && t > 0.2)
    {
        VR_EH_Give(WID_SWORD, sword ? 0 : VR_RG_Dagger());
        VR_ET_To(2);
    }
    else if(vr_et_s == 2 && t > 0.4)
    {
        // The off hand's place: a hotspot (1 the grip, 3 the blade), or near the tip (2: away from the handle and the hotspots).
        vector to = '0 0 0';
        if(k == 1 || k == 3)
        {
            float spot = VR_FGT_Spot(k == 1 ? 1 : 2);
            VR_Test_Check(spot >= 0, sprintf("%s: the hotspot there (%g)", what, spot));
            if(spot < 0)
            {
                VR_EH_NextCase();
                return;
            }
            to = weaponhotspot(p, h, spot);
        }
        else if(k == 2)
        {
            entity b = VR_Blow(h);
            to = b.mh_grip + (b.mh_far - b.mh_grip) * (sword ? 0.2 : 0.6);
            VR_Test_Print(sprintf("flipgate: (%s: the blade %.1f long, the off hand %.1f from the hand)\n", what, vlen(b.mh_far - b.mh_grip),
                vlen(to - VRGetHandPos(h))));
        }
        if(k == 0)
        {
            VR_ET_To(30);
        }
        else
        {
            vr_fgt_to = to;
            localcmd(sprintf("vr_mock_hand_to off %v\n", to));
            VR_ET_To(10);
        }
    }
    else if(vr_et_s == 10 && t > 0.2)
    {
        localcmd(sprintf("vr_mock_hand_to off %v\n+graboff\n", vr_fgt_to));
        VR_ET_To(11);
    }
    else if(vr_et_s == 11 && (held || t > 1))
    {
        VR_Test_Check(held, sprintf("%s: held two-handed so (aiming %g, the off hand's grip %g)",
            what, VRGet2HAiming(p), weaponanygrip(p, off)));
        VR_ET_To(30);
    }
    else if(vr_et_s == 30 && t > 0.2)
    {
        vector may = weapondrawnpose(p, h, 46);
        VR_Test_Check(may_z == (refuse ? -1 : 1), sprintf("%s: the grip %s now (weapondrawnpose 46 z %g)", what, refuse ? "may not flip" : "may flip", may_z));
        vr_fgt_flips = vr_revgrip_flips;
        vr_fgt_refused = vr_revgrip_refused;
        vr_fgt_rev = VR_RG_Reversed();
        VR_RevGrip_Flip(h); // (the eject button's flip: VR_RevGrip_HandFrame; the flat holster menu's FLIP GRIP)
        float flipped = VR_RG_Reversed() != vr_fgt_rev;
        if(refuse)
        {
            VR_Test_Check(!flipped && vr_revgrip_flips == vr_fgt_flips && vr_revgrip_refused == vr_fgt_refused + 1,
                sprintf("%s: the flip refused (grip %s, flips %g, refused %g)", what, VR_RG_Reversed() ? "reversed" : "upright",
                vr_revgrip_flips - vr_fgt_flips, vr_revgrip_refused - vr_fgt_refused));
        }
        else
        {
            VR_Test_Check(flipped && vr_revgrip_flips == vr_fgt_flips + 1 && vr_revgrip_refused == vr_fgt_refused,
                sprintf("%s: flips (grip %s, flips %g, refused %g)", what, VR_RG_Reversed() ? "reversed" : "upright",
                vr_revgrip_flips - vr_fgt_flips, vr_revgrip_refused - vr_fgt_refused));
        }
        VR_ET_To(31);
    }
    else if(vr_et_s == 31 && t > 0.3)
    {
        // (After the flip's turn: a two-handed hold by the handle still held, the off hand still on its grip.)
        VR_Test_Print(sprintf("flipgate: (%s: after the flip, aiming two-handed %g, the off hand's grip %g)\n", what, VRGet2HAiming(p),
            weaponanygrip(p, off)));
        localcmd("-graboff\n");
        localcmd("tune vr_revgrip_2h_gate 1\n");
        VR_EH_NextCase();
    }
};
#endif
