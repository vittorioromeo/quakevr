#ifdef QVR_TESTS
// vr_embed_1108b_test.qc -- `vr_embed_test 29` (embed1108b): a gib skewered on the sword in the hand stays on the drawn blade
// while you turn and move with the stick (his note 2026-11-08, item 5; the engine's vr_held.cpp skewerFrame). Each case puts
// a gib on the blade by a stab (as test 21), then holds the stick for a while; the engine's vr_embed_probe_skew is the most the
// drawn gib was off its place on the drawn blade (cm), over vr_embed_probe_skewn frames.
// 0  a smooth turn (the main stick)
// 1  snap turns (vr_snap_turn 45)
// 2  a move forward (the off stick)
// 3  a smooth turn and a move at once
// 4  the same with vr_carry_local 0 (the gib drawn where the server has it, as before): how far it lagged (no check but that
//    the probe sees it)
// Ends `embed1108b: N passed, M failed`.

float VR_E9_MOST = 0.5;  // cm: the most a drawn gib may be off its place on the drawn blade
float VR_E9_HOLD = 0.75; // s of the stick held (54 frames)
float vr_e9_snap;        // vr_snap_turn as the test began
entity vr_e9_g;
float vr_e9_yaw;         // the player's yaw as the stick went on
vector vr_e9_at;         // and its place

void() VR_E9_Begin =
{
    VR_Test_BeginFlags("embed1108b", VR_TEST_LOUD);
    vr_e9_snap = cvar_hget(cvarh_vr_snap_turn);
    vr_eh_home = VR_Player().origin;
    localcmd("vr_mock_hand_to_absolute 1\n");
    cvar_set("vr_embed_probe", "on");
    vr_et_on = 29;
    vr_et_case = 0;
    VR_ET_To(0);
};

void() VR_E9_Next =
{
    localcmd("vr_mock_stick main 0 0\nvr_mock_stick off 0 0\n");
    cvar_set("vr_carry_local", "1");
    cvar_set("vr_snap_turn", ftos(vr_e9_snap));
    VR_EH_NextCase();
};

void() VR_E9_Step =
{
    entity p = self;
    float t = time - vr_et_t0;
    if(vr_et_case > 4)
    {
        cvar_set("vr_embed_probe", "");
        VR_Test_Verdict();
        vr_et_on = 0;
        return;
    }
    if(vr_et_s == 0)
    {
        setorigin(p, vr_eh_home);
        p.velocity = '0 0 0';
        localcmd("+grabmain\n");
        VR_ET_To(1);
    }
    else if(vr_et_s == 1 && VR_HandGrabUtil_IsHandGrabbing(p, cVR_MainHand) && t > 0.1)
    {
        VR_EH_Give(WID_SWORD, 0);
        VR_ET_To(2);
    }
    else if(vr_et_s == 2 && t > 0.3)
    {
        vr_e9_g = VR_ETS_Gib(p, VR_ETS_OnLine(0));
        VR_ETS_PutOn(vr_e9_g, 0);
        VR_ETS_Blow(p, vr_e9_g, VR_MKIND_STAB, 6);
        VR_ET_To(3);
    }
    else if(vr_et_s == 3 && t > 0.2)
    {
        VR_Test_Check(vr_e9_g.vr_skewer_p == p, sprintf("case %g: a gib on the blade in the hand", vr_et_case));
        if(vr_e9_g.vr_skewer_p != p)
        {
            VR_E9_Next();
            return;
        }
        if(vr_et_case == 1)
        {
            cvar_set("vr_snap_turn", "45");
        }
        if(vr_et_case == 4)
        {
            cvar_set("vr_carry_local", "0");
        }
        float turn = vr_et_case != 2;
        float walk = vr_et_case >= 2;
        localcmd(sprintf("vr_mock_stick main %g 0\nvr_mock_stick off 0 %g\n", turn, walk));
        cvar_set("vr_embed_probe_skew", "-1");
        cvar_set("vr_embed_probe_skewn", "0");
        vr_e9_yaw = p.angles_y;
        vr_e9_at = p.origin;
        VR_ET_To(4);
    }
    else if(vr_et_s == 4 && vr_e9_g.vr_skewer_p == p && t > VR_E9_HOLD)
    {
        float off = cvar_hget(cvarh_vr_embed_probe_skew);
        float n = cvar_hget(cvarh_vr_embed_probe_skewn);
        float turned = fabs(anglemod(p.angles_y - vr_e9_yaw + 180) - 180);
        float moved = vlen(p.origin - vr_e9_at);
        string what = sprintf("(turned %.0f deg, moved %.0f units; %g frames; at most %.2f cm off)", turned, moved, n, off);
        if(vr_et_case == 4)
        {
            VR_Test_Check(n > 10 && off > VR_E9_MOST, sprintf("vr_carry_local 0: the gib lags the drawn blade %s", what));
        }
        else
        {
            float did = vr_et_case == 2 ? moved > 32 : turned > 30;
            VR_Test_Check(did && n > 10 && off >= 0 && off <= VR_E9_MOST,
                sprintf("case %g (%s): the drawn gib stays on the drawn blade %s", vr_et_case,
                vr_et_case == 0 ? "smooth turn" : (vr_et_case == 1 ? "snap turns" : (vr_et_case == 2 ? "a move" : "a turn and a move")), what));
        }
        VR_E9_Next();
    }
    else if(vr_et_s == 4 && vr_e9_g.vr_skewer_p != p)
    {
        VR_Test_Check(FALSE, sprintf("case %g: the gib came off the blade while turning or moving", vr_et_case));
        VR_E9_Next();
    }
};

// ---- `vr_embed_test 30` (embedprops): blades in wooden crates and barrels (his notes 2026-11-08, items 13 and 14; vr_embedhold.qc
// VR_Embed_PropHost, VR_Embed_PropSpring), by the mock hand's real blows (VR_EH_Blow) at a crate before you:
// 0  a sword's stab into a small crate stays in, held; the hand moved aside drags the crate; moved up, it can't lift it (one hand,
//    Crates: Pull under its weight); let go of, the blade stays in and rides the crate pushed away.
// 1  a stab, then the hand pulled back along the blade: out, in the hand.
// 2  a stab held, the crate broken: the blade back in the hand.
// 3  an axe's swing into a barrel stays in.
// 4  Crates: Blades Stick off: a stab doesn't stay in.
// Ends `embedprops: N passed, M failed`.
float vr_ep_props;  // vr_embed_props as the test began
vector vr_ep_at;    // the crate's place as the move began
vector vr_ep_off;   // the blade's place from the crate's, let go of
vector vr_ep_from;  // the hand's

void() VR_EP_Begin =
{
    VR_Test_BeginFlags("embedprops", VR_TEST_LOUD);
    cvar_set("vr_knockdown", "0");
    vr_ep_props = cvar_hget(cvarh_vr_embed_props);
    vr_eh_home = VR_Player().origin;
    localcmd("vr_mock_hand_to_absolute 1\n");
    vr_et_on = 30;
    vr_et_case = 0;
    VR_ET_To(0);
};

void() VR_EP_Next =
{
    localcmd(sprintf("tune vr_embed_props %g\n", vr_ep_props));
    vr_ee_target = world; // (VR_ET_Clean removes it: it is vr_et_m)
    VR_EH_NextCase();
};

// A crate (xKind 1 small, 2 large) or a barrel (3) on the floor `xAhead` units before you.
entity(float xKind, float xAhead) VR_EP_Crate =
{
    entity p = self;
    makevectors('0 1 0' * p.angles_y);
    entity c = spawn();
    setorigin(c, p.origin + v_forward * xAhead);
    c.angles_y = p.angles_y;
    c.spawnflags = xKind == 2 ? 1 : 0;
    self = c;
    if(xKind == 3)
    {
        vr_barrel();
    }
    else
    {
        vr_crate();
    }
    self = p;
    return c;
};

void() VR_EP_Step =
{
    entity p = self;
    float t = time - vr_et_t0;
    if(vr_et_case > 4)
    {
        VR_Test_Verdict();
        vr_et_on = 0;
        return;
    }
    float wid = vr_et_case == 3 ? WID_AXE : WID_SWORD;
    if(vr_et_s < 10)
    {
        if(vr_et_s == 0 && vr_ee_target == world)
        {
            setorigin(p, vr_eh_home);
            p.velocity = '0 0 0';
            vr_ee_target = VR_EP_Crate(vr_et_case == 3 ? 3 : 1, vr_et_case == 3 ? 42 : 50);
            if(vr_et_case == 4)
            {
                localcmd("tune vr_embed_props 0\n");
            }
        }
        if(VR_EH_Blow(wid, 6, 0, vr_et_case == 3, t))
        {
            VR_ET_To(10);
        }
        return;
    }
    entity c = vr_et_m;
    entity w = VR_EH_Blade();
    string what = vr_et_case == 3 ? "an axe's swing into a barrel" : "a sword's stab into a crate";
    if(vr_et_s == 10)
    {
        if(vr_et_case == 4)
        {
            VR_Test_Check(w == world, sprintf("case 4: Crates: Blades Stick off: %s doesn't stay in (blade %g)", what, w != world));
            VR_EP_Next();
            return;
        }
        VR_Test_Check(w != world && w.vr_stick_kind == AXESTICK_PROP && w.vr_stick_ent == c,
            sprintf("case %g: %s stays in, held (kind %g, in %s)", vr_et_case, what, w.vr_stick_kind, w.vr_stick_ent.classname));
        if(w == world || vr_et_case == 3)
        {
            VR_EP_Next();
            return;
        }
        vr_et_w = w;
        vr_ep_at = c.origin;
        vr_ep_from = VRGetHandPos(cVR_MainHand);
        if(vr_et_case == 0)
        {
            VR_EH_HandTo(vr_ep_from + VR_AxeStick_Turn(vr_eh_fw, '0 0 1', 90) * 30, 0.6); // (aside, slowly)
            VR_ET_To(11);
        }
        else if(vr_et_case == 1)
        {
            VR_EH_HandTo(vr_ep_from - vr_eh_fw * 40, 0.35); // (back along it)
            VR_ET_To(20);
        }
        else
        {
            T_Damage(c, p, p, 1000);
            VR_ET_To(30);
        }
        return;
    }
    // ---- 0: dragged aside, not lifted, let go of
    if(vr_et_s == 11 && t > 1)
    {
        vector across = VR_AxeStick_Turn(vr_eh_fw, '0 0 1', 90);
        float moved = (c.origin - vr_ep_at) * across;
        VR_Test_Check(w == vr_et_w && moved > 8, sprintf("the hand moved 30 units aside: the crate dragged %.1f units (held %g, pull %.0f N)", moved, w == vr_et_w, vr_et_w.vr_hold_proppull));
        vr_ep_at = c.origin;
        vr_ep_from = VRGetHandPos(cVR_MainHand);
        VR_EH_HandTo(vr_ep_from + '0 0 30', 0.6);
        VR_ET_To(12);
        return;
    }
    if(vr_et_s == 12 && t > 1)
    {
        float rose = c.origin_z - vr_ep_at_z;
        VR_Test_Check(rose < 6, sprintf("the hand moved 30 units up: one hand can't lift it (it rose %.1f units; %.0f kg; held %g, pull %.0f N)", rose, propvalue(c, "mass"), w == vr_et_w, vr_et_w.vr_hold_proppull));
        localcmd("-grabmain\n");
        VR_ET_To(13);
        return;
    }
    if(vr_et_s == 13 && t > 0.4)
    {
        VR_Test_Check(VR_EH_Blade() == world && vr_et_w.vr_stick_kind == AXESTICK_PROP && vr_et_w.vr_stick_ent == c && !VR_Embed_Held(vr_et_w),
            sprintf("let go of: it stays in the crate (kind %g)", vr_et_w.vr_stick_kind));
        vr_ep_off = vr_et_w.origin - c.origin;
        vr_ep_at = c.origin;
        physicspush(c, (c.absmin + c.absmax) * 0.5, vr_eh_fw * 250);
        VR_ET_To(14);
        return;
    }
    if(vr_et_s == 14 && t > 0.6)
    {
        float went = vlen(c.origin - vr_ep_at);
        float off = vlen((vr_et_w.origin - c.origin) - vr_ep_off);
        VR_Test_Check(went > 4 && vr_et_w.vr_stick_kind == AXESTICK_PROP && off < 12,
            sprintf("the crate pushed away %.1f units: the blade rides it (%.1f units off its place, turned with it)", went, off));
        VR_EP_Next();
        return;
    }
    // ---- 1: pulled out along the blade
    if(vr_et_s == 20 && (w == world || t > 1.2))
    {
        VR_Test_Check(w == world && VRGetEntWeapon(p, cVR_MainHand) == WID_SWORD && vlen(c.origin - vr_ep_at) < 16,
            sprintf("pulled back along the blade: out, in the hand (weapon %g; the crate moved %.1f units)", VRGetEntWeapon(p, cVR_MainHand), vlen(c.origin - vr_ep_at)));
        VR_EP_Next();
        return;
    }
    // ---- 2: the crate broken
    if(vr_et_s == 30 && t > 0.2)
    {
        VR_Test_Check(!VR_Crate_Is(c) && w == world && VRGetEntWeapon(p, cVR_MainHand) == WID_SWORD,
            sprintf("the crate broken: the blade back in the hand (a crate %g, weapon %g)", VR_Crate_Is(c), VRGetEntWeapon(p, cVR_MainHand)));
        vr_et_m = world; // (gone)
        VR_EP_Next();
        return;
    }
};
#endif
