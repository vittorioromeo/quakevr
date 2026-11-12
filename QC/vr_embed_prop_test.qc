#ifdef QVR_TESTS
// vr_embed_prop_test.qc -- vr_embed_test 34 (propblade): a blade in a crate held by its handle, and the other hand on a gib skewered on a
// blade (his notes 2026-11-05 "pulling either the blade or the prop moves both", 2026-11-12 19:52 "the grab should only be possible
// when the visual model actually connects with the hand, and the hand should snap on the surface of the gib"). In the open
// (vrtesthall), god and notarget. The helpers are vr_embed_test.qc's (the blow, the gibs) and vr_embed_1108b_test.qc's (the crate).
//
// 1  a sword stabbed into a small crate, held (VR_Embed_PropSpring: the engine's embedprophold, a joint at the handle): still, the
//    handle is in the real hand across the blade; the hand moved 20 units aside in 1 s: the crate comes, the handle stays in the
//    hand; the wrist turned 20 degrees: the handle stays, the crate isn't thrown about; let go of, the crate pushed away: the blade
//    goes with it, where it went in.
// 0  a gib on the sword in the main hand, the other hand closing its grip 16 units off it, then 1 unit nearer each time: it takes
//    hold only once its fist is at the gib's drawn model (carryreach), and is then drawn on the model's surface (carryfit's, as a
//    hand takes a prop), not on a ball of its box's size.
// Ends `propblade: N passed, M failed`.
float  vr_eq_gap;    // the crate: units the most the handle was from the real hand across the blade, dragged
float  vr_eq_d0;     // ... the wound's distance from the crate's origin as it was let go of
vector vr_eq_dir0;   // ... the blade's way as the wrist began to turn
float  vr_eq_turned; // ... degrees the wrist has turned so far
float  vr_eq_off;    // the gib: units off the blade's line the other hand tries its grip from
float  vr_eq_tries;  // ... grips tried

void() VR_EQ_Begin =
{
    VR_Test_BeginFlags("propblade", VR_TEST_LOUD);
    cvar_set("vr_knockdown", "0");
    vr_ep_props = cvar_hget(cvarh_vr_embed_props);
    vr_eh_home = VR_Player().origin;
    vr_et_on = 34;
    vr_et_case = 0;
    VR_ET_To(0);
};

// The handle of `w` from the real main hand, across its blade (units); along it in vr_eq_along.
float vr_eq_along;
float(entity w) VR_EQ_Across =
{
    vector gap = w.origin - VRGetHandPos(cVR_MainHand);
    vr_eq_along = gap * w.vr_stick_dir;
    return vlen(gap - w.vr_stick_dir * vr_eq_along);
};

void() VR_EQ_Crate =
{
    entity p = self;
    float t = time - vr_et_t0;
    if(vr_et_s < 10)
    {
        if(vr_et_s == 0 && vr_ee_target == world)
        {
            setorigin(p, vr_eh_home);
            p.velocity = '0 0 0';
            localcmd("vr_mock_hand_to_absolute 1\n"); // (as test 30 puts the hand)
            vr_ee_target = VR_EP_Crate(1, 50);
        }
        if(VR_EH_Blow(WID_SWORD, 6, 0, FALSE, t))
        {
            VR_ET_To(10);
        }
        return;
    }
    entity c = vr_et_m;
    entity w = VR_EH_Blade();
    if(vr_et_s == 10)
    {
        VR_Test_Check(w != world && w.vr_stick_kind == AXESTICK_PROP && w.vr_stick_ent == c, sprintf("a sword's stab into a crate stays in, held (kind %g)", w.vr_stick_kind));
        if(w == world)
        {
            VR_EP_Next();
            return;
        }
        vr_et_w = w;
        localcmd("vr_mock_hand_aim main off\n");
        VR_ET_To(11);
    }
    else if(vr_et_s == 11 && t > 0.5)
    {
        float still = VR_EQ_Across(vr_et_w);
        VR_Test_Print(sprintf("%s: held, still: the handle %.1f units from the real hand across the blade (%.1f along it: the stab's own, its slack), by its handle %g\n", vr_test_tag, still, vr_eq_along, vr_et_w.vr_hold_byhandle));
        VR_Test_Check(w == vr_et_w && still <= 1.5, sprintf("held still, the handle is in the real hand (%.1f units off across the blade, 1.5 allowed; held %g)", still, w == vr_et_w));
        vr_ep_at = c.origin;
        vr_ep_from = VRGetHandPos(cVR_MainHand);
        vr_eq_gap = 0;
        VR_EH_HandTo(vr_ep_from + VR_AxeStick_Turn(vr_eh_fw, '0 0 1', 90) * 20, 1);
        VR_ET_To(12);
    }
    else if(vr_et_s == 12)
    {
        vr_eq_gap = max(vr_eq_gap, VR_EQ_Across(vr_et_w));
        if(t > 1.6 || w != vr_et_w)
        {
            vector went = c.origin - vr_ep_at;
            went_z = 0;
            float settled = VR_EQ_Across(vr_et_w);
            VR_Test_Print(sprintf("%s: the hand moved 20 units aside in 1 s: the crate came %.1f units, the handle %.1f units from the real hand at most across the blade, %.1f settled (pull %.0f N)\n",
                vr_test_tag, vlen(went), vr_eq_gap, settled, vr_et_w.vr_hold_proppull));
            VR_Test_Check(w == vr_et_w, "dragged aside: still held");
            VR_Test_Check(vlen(went) >= 8, sprintf("a held handle pulls the crate (%.1f units of the hand's 20, at least 8)", vlen(went)));
            VR_Test_Check(settled <= 3, sprintf("the handle stays in the hand (%.1f units off across the blade settled, 3 allowed; %.1f at most on the way)", settled, vr_eq_gap));
            if(w != vr_et_w)
            {
                VR_ET_To(13);
                return;
            }
            vr_eq_dir0 = vr_et_w.vr_stick_dir;
            vr_eq_turned = 0;
            VR_ET_To(15);
        }
    }
    // The wrist turned 20 degrees about the upright, the hand where it is. Measured 2026-11-12: a 40 kg crate on the floor doesn't
    // turn (0.6 degrees): the hold's torque, its 700 N at the blade's length, is less than the floor's friction asks to swing the
    // crate round the handle, as a wrist's is. The handle stays in the hand, the crate isn't thrown about.
    else if(vr_et_s == 15)
    {
        if(vr_eq_turned < 20)
        {
            vr_eq_turned = vr_eq_turned + 1;
            localcmd("vr_mock_hand_turn main 0 1 0\n");
        }
        else if(t > 1 || w != vr_et_w)
        {
            float turnAlong = vr_et_w.vr_stick_dir * vr_eq_dir0;
            float turnedBy = atan2(vlen(vr_et_w.vr_stick_dir - vr_eq_dir0 * turnAlong), turnAlong) * 57.29578;
            float across = VR_EQ_Across(vr_et_w);
            VR_Test_Print(sprintf("%s: the wrist turned 20 degrees: the blade (the crate with it) turned %.1f degrees, the handle %.1f units from the real hand across the blade (pull %.0f N)\n", vr_test_tag, turnedBy, across, vr_et_w.vr_hold_proppull));
            VR_Test_Check(w == vr_et_w && turnedBy <= 20, sprintf("the wrist turned on a blade in a crate: still held, the crate turns no further than the wrist (%.1f of its 20 degrees; held %g)", turnedBy, w == vr_et_w));
            VR_Test_Check(across <= 3, sprintf("the handle still in the hand (%.1f units off across the blade, 3 allowed)", across));
            localcmd("-grabmain\n");
            VR_ET_To(13);
        }
    }
    else if(vr_et_s == 13 && t > 0.4)
    {
        VR_Test_Check(VR_EH_Blade() == world && vr_et_w.vr_stick_kind == AXESTICK_PROP && vr_et_w.vr_stick_ent == c, sprintf("let go of: it stays in the crate (kind %g)", vr_et_w.vr_stick_kind));
        vr_eq_d0 = vlen(vr_et_w.vr_stick_at - c.origin);
        vr_ep_at = c.origin;
        physicspush(c, (c.absmin + c.absmax) * 0.5, vr_eh_fw * 250);
        VR_ET_To(14);
    }
    else if(vr_et_s == 14 && t > 0.6)
    {
        float pushed = vlen(c.origin - vr_ep_at);
        float drift = fabs(vlen(vr_et_w.vr_stick_at - c.origin) - vr_eq_d0);
        VR_Test_Check(pushed > 4 && vr_et_w.vr_stick_kind == AXESTICK_PROP && drift < 1,
            sprintf("pulling the crate drags the blade: pushed %.1f units, where the blade went in stays on it (%.2f units off its place on the crate, 1 allowed)", pushed, drift));
        VR_EP_Next();
    }
};

void() VR_EQ_Gib =
{
    entity p = self;
    float t = time - vr_et_t0;
    entity g = vr_ets_g[0];
    float h = cVR_OffHand;
    if(vr_et_s == 0)
    {
        setorigin(p, vr_eh_home);
        p.velocity = '0 0 0';
        cvar_set("vr_embed_skewer_most", "3");
        cvar_set("vr_embed_probe", "reset");
        cvar_set("vr_embed_probe_gibsnap", "-1");
        localcmd("vr_mock_hand_to_absolute 0\n+grabmain\n"); // (as test 21 places the other hand)
        VR_ET_To(10);
    }
    else if(vr_et_s == 10 && VR_HandGrabUtil_IsHandGrabbing(p, cVR_MainHand))
    {
        VR_EH_Give(WID_SWORD, 0);
        VR_Blow(cVR_MainHand).mh_far = '0 0 0';
        VR_ET_To(1); // (its line, from the next frame's melee)
    }
    else if(vr_et_s == 1 && VR_Blow(cVR_MainHand).mh_far != '0 0 0')
    {
        makevectors(p.v_angle);
        vr_ets_g[0] = VR_ETS_Gib(p, VR_ETS_OnLine(0));
        VR_ET_To(2);
    }
    else if(vr_et_s == 2 && g.takedamage)
    {
        VR_ETS_PutOn(g, 0);
        VR_ETS_Blow(p, g, VR_MKIND_STAB, 6);
        VR_Test_Check(g.vr_gib && g.vr_skewer_p == p, "a gib on the blade in the main hand");
        VRSetHandEmpty(p, h);
        vr_eq_off = 16;
        vr_eq_tries = 0;
        VR_ET_To(20);
    }
    else if(vr_et_s == 20 && t > 0.3)
    {
        if(g.vr_skewer_p != p)
        {
            VR_Test_Check(FALSE, "the gib came off the blade before any grip");
            VR_ET_To(40);
            return;
        }
        vr_et_at = VR_ETS_OffHandOn(g, VR_Skewer_Back(g, VR_Skewer_Line(g)), -vr_eq_off); // (on the other hand's own side of the blade)
        VR_ET_To(21);
    }
    else if(vr_et_s == 21 && vlen(VRGetEntHandVel(p, h)) < 0.5 && (vlen(VRGetHandPos(h) - vr_et_at) < 1 || t > 0.5)) // (settled: its jump there is no yank)
    {
        if(vlen(VRGetHandPos(h) - vr_et_at) >= 1)
        {
            VR_Test_Check(FALSE, sprintf("the other hand didn't get to its place (%.1f units from it, %.1f off the line; the hand %v, its place %v, you %v)", vlen(VRGetHandPos(h) - vr_et_at), vr_eq_off, VRGetHandPos(h), vr_et_at, p.origin));
            VR_ET_To(40);
            return;
        }
        localcmd("+graboff\n");
        vr_eq_tries = vr_eq_tries + 1;
        VR_ET_To(22);
    }
    else if(vr_et_s == 22 && t > 0.08)
    {
        if(g.vr_skewer_grip == h + 1)
        {
            vector hp = VRGetHandPos(h);
            float from = vlen(hp - g.vr_skewer_c);
            float r = VR_Skewer_Radius(g);
            // (The hold's place against the hand's: how far the hand goes to lie on the gib; from there the fit asks nothing more.)
            vector hold = g.vr_skewer_c + g.vr_skewer_gripd;
            makevectors(VRGetEntHandRot(p, h));
            float again = vlen(carryfit(g, hold, v_right));
            VR_Test_Print(strcat(sprintf("%s: the other hand took hold of the gib %.1f units off the blade's line, %.1f units from its middle (its box's width %.1f), try %g; ", vr_test_tag, vr_eq_off, from, r, vr_eq_tries),
                sprintf("its hold %.2f units from the hand, %.2f from the gib's middle (the ball's %.1f)\n", vlen(hold - hp), vlen(g.vr_skewer_gripd), r + VR_SKEWER_PALM)));
            VR_Test_Check(vr_eq_tries > 1, sprintf("16 units off it, no hold (%g tries before it took)", vr_eq_tries - 1));
            VR_Test_Check(from <= r + VR_SKEWER_PALM, sprintf("it takes hold only with the hand at the gib as drawn (%.1f units from its middle, its box's width %.1f)", from, r));
            VR_Test_Check(vlen(hold - hp) <= 2 && again < 0.2, sprintf("the hand's hold is on the gib's drawn surface, where the hand is (%.2f units from the hand; the fit from there asks %.2f more)", vlen(hold - hp), again));
            VR_ET_To(30);
        }
        else
        {
            localcmd("-graboff\n");
            VR_ET_To(23);
        }
    }
    else if(vr_et_s == 23 && t > 0.08 && !VR_HandGrabUtil_IsHandGrabbing(p, h))
    {
        vr_eq_off = vr_eq_off - 1;
        if(vr_eq_off < 0)
        {
            VR_Test_Check(FALSE, "the other hand never took hold of the gib, down to the blade's line");
            VR_ET_To(40);
        }
        else
        {
            VR_ET_To(20);
            vr_et_t0 = time - 1; // (no wait: the next try)
        }
    }
    else if(vr_et_s == 30 && t > 0.4) // (the drawn hand gone onto it: the engine's skewerDrawnHand)
    {
        float snap = cvar_hget(cvarh_vr_embed_probe_gibsnap);
        float drawnOff = cvar_hget(cvarh_vr_embed_probe_gib);
        VR_Test_Print(sprintf("%s: as it took hold the drawn hand snapped %.1f cm onto the gib's surface; drawn there since, %.2f cm off at most\n", vr_test_tag, snap, drawnOff));
        VR_Test_Check(snap >= 0 && snap < 6 && drawnOff >= 0 && drawnOff < 0.25, sprintf("the drawn hand snaps onto the gib's surface (%.1f cm, under 6; 9.3 onto the ball before) and stays there (%.2f cm off)", snap, drawnOff));
        localcmd("-graboff\n");
        VR_ET_To(40);
    }
    else if(vr_et_s == 40 && t > 0.1)
    {
        cvar_set("vr_embed_probe", "");
        if(vr_ets_g[0] != world)
        {
            if(vr_ets_g[0].vr_skewer_p != world)
            {
                VR_Skewer_Off(vr_ets_g[0]);
            }
            remove(vr_ets_g[0]);
            vr_ets_g[0] = world;
        }
        VR_EP_Next();
    }
};

void() VR_EQ_Step =
{
    if(vr_et_case == 0)
    {
        VR_EQ_Gib();
    }
    else if(vr_et_case == 1)
    {
        VR_EQ_Crate();
    }
    else
    {
        localcmd("vr_mock_hand_to_absolute 0\n");
        VR_Test_Verdict();
        vr_et_on = 0;
    }
};
#endif
