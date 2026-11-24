#ifdef QVR_TESTS
// vr_embeddual_test.qc -- vr_embed_test 52 (embeddual): flat dual wielding, an embedded blade keeps its hand (his words 2026-11-21:
// "When dual-wielding a bladed weapon and a gun, if the bladed weapon gets embedded into an enemy, the hand that is supposed to
// wield it stops holding it visually and goes to hold the gun in a 2H pose ... ADS also becomes possible in that state"). Run with
// -flatscreenmode on vrtesthall's floor, god and notarget. An axe in one hand and a shotgun in the other; the mouse button's swing
// (vr_flat_punch) puts the axe into a grunt ahead, held (QC's VR_HOLD_EMBED). The engine's vr_flat_route_probe says the rest
// (Quake/vr/vr_flatplay.cpp: its bits). Each case:
//   before: dual wielding: no hand on the gun, RMB is not the ironsight's;
//   embedded and held: the hand drawn on the blade, still not on the gun (no two-handed grip or stance: VRGet2HAiming no either),
//   the ironsight refused though asked for (vr_flat_aim 1), each mouse button its own hand's;
// then case 0 (the axe in the main hand): it comes out into the hand (VR_Embed_Free, as a pull or a recall ends): dual wielding
// again, no ironsight; case 1 (the axe in the off hand): let go of (VR_Embed_DropHand): the free hand goes onto the gun and the
// ironsight asked for comes up. Ends `embeddual: N passed, M failed`.
float vr_ed_hand;   // the blade's hand this case
float vr_ed_swings; // swings tried this case
float vr_ed_bits;   // the probe's bits, read

void() VR_ED_Begin =
{
    VR_Test_BeginFlags("embeddual", VR_TEST_LOUD);
    vr_eh_home = VR_Player().origin;
    vr_et_on = 52;
    vr_et_case = 0;
    VR_ET_To(0);
};

void(float xHand, float xWid) VR_ED_Give =
{
    VRSetEntWeapon(self, xHand, xWid);
    VRSetEntWeaponFlags(self, xHand, 0);
    VRSetEntWeaponInst(self, xHand, WeaponInst_New(0));
    W_SetCurrentAmmoFor(xHand);
};

string(float xHand) VR_ED_HandName =
{
    return xHand == cVR_MainHand ? "main" : "off";
};

entity() VR_ED_Blade =
{
    return VR_Hand_Kind(self, vr_ed_hand) == VR_HOLD_EMBED ? VR_Hand_BusyEnt(self, vr_ed_hand) : world;
};

float(float xBit) VR_ED_Bit =
{
    return (vr_ed_bits & xBit) ? 1 : 0;
};

void() VR_ED_Next =
{
    localcmd("vr_flat_aim 0\n");
    VR_ET_Clean();
    vr_et_case = vr_et_case + 1;
    VR_ET_To(0);
};

void() VR_ED_Step =
{
    entity p = self;
    float t = time - vr_et_t0;
    float gun = VRGetOtherHand(vr_ed_hand);
    if(vr_et_case > 1)
    {
        VR_Test_Verdict();
        vr_et_on = 0;
        return;
    }
    if(vr_et_s == 0 && !VR_FlatPlay(p))
    {
        // (a flat-screen test: VR play has no mouse button's swing, so the axe never goes in; one clear failure, not 4 swings a case)
        VR_Test_Check(FALSE, "flat play off: run with -flatscreenmode (Misc/quakevr/embeddual_test.sh)");
        VR_Test_Verdict();
        vr_et_on = 0;
        return;
    }
    if(vr_et_s == 0)
    {
        vr_ed_hand = vr_et_case == 0 ? cVR_MainHand : cVR_OffHand;
        gun = VRGetOtherHand(vr_ed_hand);
        vr_ed_swings = 0;
        setorigin(p, vr_eh_home);
        p.velocity = '0 0 0';
        p.ammo_shells = 50;
        VR_ED_Give(vr_ed_hand, WID_AXE);
        VR_ED_Give(gun, WID_SHOTGUN);
        vr_et_m = VR_ET_Grunt(44);
        VR_Test_Print(sprintf("%s: case %g: the axe in the %s hand, the shotgun in the %s hand (flat screen %g)\n", vr_test_tag, vr_et_case,
            VR_ED_HandName(vr_ed_hand), VR_ED_HandName(gun), VR_FlatPlay(p)));
        VR_ET_To(1);
        return;
    }
    if(vr_et_s == 1 && t > 0.6) // (the weapons taken up: their pose eased in)
    {
        localcmd("vr_flat_route_probe\n");
        VR_ET_To(2);
        return;
    }
    if(vr_et_s == 2 && t > 0.05)
    {
        vr_ed_bits = cvar_hget(cvarh_vr_flat_route_seen);
        VR_Test_Check(!VR_ED_Bit(256) && !VR_ED_Bit(16), sprintf("case %g, before: dual wielding, no hand on the gun, RMB not the ironsight's (bits %g)", vr_et_case, vr_ed_bits));
        VR_ET_To(3);
        return;
    }
    if(vr_et_s == 3)
    {
        if(VR_ED_Blade() != world)
        {
            VR_ET_To(4);
            return;
        }
        if(vr_ed_swings == 0 || t > 1.2) // (a swing and its way back; one that missed or didn't stay in: again)
        {
            if(vr_ed_swings >= 4)
            {
                VR_Test_Check(FALSE, sprintf("case %g: the axe's swing stays in the grunt, held (%g swings)", vr_et_case, vr_ed_swings));
                VR_ED_Next();
                return;
            }
            if(vr_et_m == world || vr_et_m.health <= 0)
            {
                vr_et_m = VR_ET_Grunt(44);
            }
            vr_et_m.health = 1000;
            localcmd(sprintf("vr_flat_punch %s\n", VR_ED_HandName(vr_ed_hand)));
            vr_ed_swings = vr_ed_swings + 1;
            vr_et_t0 = time;
        }
        return;
    }
    entity w = VR_ED_Blade();
    if(vr_et_s == 4 && t > 0.4) // (stuck: the hold settled)
    {
        VR_Test_Check(w != world, sprintf("case %g: the axe stuck in the grunt, held by the %s hand (swing %g)", vr_et_case, VR_ED_HandName(vr_ed_hand), vr_ed_swings));
        localcmd("vr_flat_aim 1\n");
        VR_ET_To(5);
        return;
    }
    if(vr_et_s == 5 && t > 0.5) // (past the ironsight's travel, were it to come up)
    {
        localcmd("vr_flat_route_probe\n");
        VR_ET_To(6);
        return;
    }
    if(vr_et_s == 6 && t > 0.05)
    {
        vr_ed_bits = cvar_hget(cvarh_vr_flat_route_seen);
        float onBlade = VR_ED_Bit(vr_ed_hand == cVR_MainHand ? 512 : 1024);
        VR_Test_Check(w != world && onBlade, sprintf("case %g, embedded: the %s hand is drawn on the blade it holds (held %g, drawn on it %g)", vr_et_case,
            VR_ED_HandName(vr_ed_hand), w != world, onBlade));
        VR_Test_Check(!VR_ED_Bit(256) && !VRGet2HAiming(p), sprintf("case %g, embedded: the gun stays one-handed, no hand on it (bits %g, 2H aiming %g)", vr_et_case, vr_ed_bits, VRGet2HAiming(p)));
        VR_Test_Check(!VR_ED_Bit(32) && !VR_ED_Bit(16), sprintf("case %g, embedded: the ironsight refused though asked for, RMB not its (bits %g)", vr_et_case, vr_ed_bits));
        VR_Test_Check(VR_ED_Bit(1) && VR_ED_Bit(8), sprintf("case %g, embedded: each mouse button its own hand's, LMB the main, RMB the off (bits %g)", vr_et_case, vr_ed_bits));
        if(w != world)
        {
            if(vr_et_case == 0)
            {
                VR_Embed_Free(w, "the test: pulled out");
            }
            else
            {
                VR_Embed_DropHand(w, vr_ed_hand, "the test: let go");
            }
        }
        vr_et_w = w;
        VR_ET_To(7);
        return;
    }
    if(vr_et_s == 7 && t > 0.8) // (the hand's ease off the blade, and onto the gun or into the ironsight)
    {
        localcmd("vr_flat_route_probe\n");
        VR_ET_To(8);
        return;
    }
    if(vr_et_s == 8 && t > 0.05)
    {
        vr_ed_bits = cvar_hget(cvarh_vr_flat_route_seen);
        if(vr_et_case == 0)
        {
            VR_Test_Check(VRGetEntWeapon(p, vr_ed_hand) == WID_AXE && w == world, sprintf("case 0, pulled out: the axe is in the main hand again (weapon %g)", VRGetEntWeapon(p, vr_ed_hand)));
            VR_Test_Check(!VR_ED_Bit(256) && !VR_ED_Bit(32) && !VR_ED_Bit(16), sprintf("case 0, pulled out: dual wielding again, no hand on the gun, no ironsight (bits %g)", vr_ed_bits));
        }
        else
        {
            VR_Test_Check(w == world && VRIsHandWeaponless(p, vr_ed_hand) && vr_et_w != world && vr_et_w.vr_stick_kind != 0,
                sprintf("case 1, let go: the off hand is empty, the axe still in the grunt (stuck %g)", vr_et_w != world && vr_et_w.vr_stick_kind != 0));
            VR_Test_Check(VR_ED_Bit(256) && VR_ED_Bit(32), sprintf("case 1, let go: the free hand goes onto the gun, the ironsight asked for comes up (bits %g)", vr_ed_bits));
        }
        VR_ED_Next();
        return;
    }
};
#endif
