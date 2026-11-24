#ifdef QVR_TESTS
// vr_flatthrust_test.qc -- flat-screen V thrusts with every stabbing blade (flatthrust, his decision 2026-11-24; ROUND25.md
// "flatthrust"): V (vr_flat_punch v) with a blade stuck in by its point (weapons.txt embed 1) is the spear's thrust
// (vr_flatplay.cpp thrustsWith / thrustPose), a stab by the tip that sticks under VR's embed rules, and a fresh press of
// the fire button pulls it out (VR_Embed_FlatPull). With -flatscreenmode only (map vrtesthall, god, notarget):
//   `vr_embed_test 58`: the knight's sword, the dagger, the spear, then the dagger in a flipped grip (a stab down), each at a
//     grunt ahead: V hurts it, the blade stuck by its point, fire presses pull it out, back in the hand; then the axe: V
//     still its swing (the script checks the engine's "overhead swing" line).
//   `vr_embed_test 59`: the same with what the main hand already holds (Dwell's Crystal Lance: vr_ad_test 207 first).
// Each case prints "flatthrust: case <name>" before its V, so the script can pair it with the engine's punch line.
// Ends `flatthrust: N passed, M failed`.

float vr_ft_hp;     // the grunt's health before the blow
float vr_ft_n;      // fire presses so far (the pull)
float vr_ft_wid;    // the case's weapon (0: what the hand holds)
float vr_ft_flags;  // and its flags
float vr_ft_ahead;  // the grunt this far ahead
float vr_ft_stuck;  // the blade stuck in the case
string vr_ft_name;

void() VR_FT_Case =
{
    float c = vr_et_case;
    vr_ft_wid = WID_SWORD;
    vr_ft_flags = 0;
    vr_ft_ahead = 40;
    if(vr_et_on == 59) { vr_ft_wid = 0; vr_ft_ahead = 42; vr_ft_name = "held (the Lance)"; return; }
    if(c == 0) { vr_ft_name = "sword"; return; }
    if(c == 1) { vr_ft_flags = VR_ADMelee_Flags(VR_SWORDKIND_DAGGER); vr_ft_ahead = 32; vr_ft_name = "dagger"; return; }
    if(c == 2) { vr_ft_flags = VR_ADMelee_Flags(VR_SWORDKIND_SPEAR); vr_ft_ahead = 42; vr_ft_name = "spear"; return; }
    if(c == 3)
    {
        vr_ft_flags = VR_ADMelee_Flags(VR_SWORDKIND_DAGGER) | QVR_WPNFLAG_REVGRIP;
        vr_ft_ahead = 30;
        vr_ft_name = "dagger flipped";
        return;
    }
    vr_ft_wid = WID_AXE;
    vr_ft_name = "axe";
};

float() VR_FT_Last = { return vr_et_on == 59 ? 0 : 4; };

void(float xOn) VR_FT_Begin =
{
    VR_Test_BeginFlags("flatthrust", VR_TEST_LOUD);
    cvar_set("vr_knockdown", "0"); // (a blow's chance of one takes the body from the blade)
    cvar_set("vr_hit_stagger", "0"); // (a blow's own chance of a stagger moves the body off the stab)
    vr_eh_home = VR_Player().origin;
    vr_et_on = xOn;
    vr_et_case = 0;
    VR_ET_To(0);
};

void() VR_FT_Frame =
{
    entity p = self;
    float t = time - vr_et_t0;
    float h = cVR_MainHand;
    entity w = VR_EH_Blade();
    if(!VR_FlatPlay(p))
    {
        VR_Test_Check(FALSE, "flat-screen play only (-flatscreenmode)");
        VR_Test_Verdict();
        vr_et_on = 0;
        return;
    }
    if(t > VR_ET_GIVEUP)
    {
        VR_Test_Check(FALSE, sprintf("timed out: case %s state %g", vr_ft_name, vr_et_s));
        VR_EH_NextCase();
        return;
    }
    if(vr_et_case > VR_FT_Last())
    {
        VR_Test_Verdict();
        vr_et_on = 0;
        return;
    }
    if(vr_et_m != world && vr_et_m.health > 0)
    {
        vr_et_m.nextthink = time + 0.5; // (the body stands where it is, its blow's anger put off)
        vr_et_m.velocity = '0 0 0';
    }
    if(vr_et_s == 0)
    {
        VR_FT_Case();
        setorigin(p, vr_eh_home - '0 40 0'); // (back from the hall's obstacle ahead of where you began)
        p.velocity = '0 0 0';
        if(vr_ft_wid != 0)
        {
            VR_EH_Give(vr_ft_wid, vr_ft_flags);
        }
        vr_et_m = VR_ET_Grunt(vr_ft_ahead);
        makevectors('0 1 0' * p.v_viewangle_y);
        setorigin(vr_et_m, p.origin + v_forward * vr_ft_ahead - '0 0 1');
        vr_ft_n = 0;
        vr_ft_stuck = 0;
        VR_ET_To(1);
    }
    else if(vr_et_s == 1 && t > 0.3 && VRGetEntWeapon(p, h) != WID_FIST) // (drawn, a flip turned over)
    {
        vr_ft_hp = vr_et_m.health;
        dprint(sprintf("flatthrust: case %s\n", vr_ft_name));
        localcmd("vr_flat_punch v\n");
        VR_ET_To(2);
    }
    else if(vr_et_s == 2 && ((w != world) || (t > 0.9))) // (stuck, or the thrust's 0.64 s over)
    {
        float dmg = vr_ft_hp - vr_et_m.health;
        if(vr_ft_wid == WID_AXE)
        {
            dprint(sprintf("flatthrust: the axe's V did %g damage\n", dmg));
            VR_EH_NextCase();
            return;
        }
        VR_Test_Check(dmg > 0, sprintf("%s: V's thrust hurts a grunt %g ahead (%g damage)", vr_ft_name, vr_ft_ahead, dmg));
        vr_ft_stuck = ((w != world) && (w.vr_stick_embed == AXESTICK_POINT));
        VR_Test_Check(vr_ft_stuck, sprintf("%s: stuck in it by its point (depth %.1f units)", vr_ft_name, (w != world) ? w.vr_stick_depth : 0));
        VR_ET_To(3);
    }
    else if(vr_et_s == 3 && w != world && t > 0.75 && vr_ft_n < 8) // (the last pull's 0.65 s over)
    {
        localcmd("+attack\n"); // (a fresh press of the fire button pulls a stuck blade: VR_Embed_FlatPull)
        vr_ft_n = vr_ft_n + 1;
        VR_ET_To(4);
    }
    else if(vr_et_s == 4 && t > 0.1)
    {
        localcmd("-attack\n"); // (let go: the next press is a fresh one)
        VR_ET_To(3);
    }
    else if(vr_et_s == 3 && (w == world || vr_ft_n >= 8) && t > 0.75)
    {
        if(vr_ft_stuck)
        {
            VR_Test_Check(w == world && VRGetEntWeapon(p, h) != WID_FIST,
                sprintf("%s: fire pulls it out (%g presses), back in the hand", vr_ft_name, vr_ft_n));
        }
        VR_EH_NextCase();
    }
};
#endif
