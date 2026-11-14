#ifdef QVR_TESTS
// vr_saveload_test.qc -- a Test scene through a save and a load (vr_pickup_test 16; Misc/quakevr/saveload_test.sh;
// ROUND25.md, "Save and load after a Test scene"). His report, 2026-11-15 (flat): TEST on the waterdrips item, then a
// quicksave and a quickload: a toolgun in his off hand, the grunts' firing poses bent and one casting the death knight's
// fireballs, and nothing wet any more. Run after `vr_scenario waterdrips` (the vrtesthall pool, two armed grunts, a
// shotgun in the main hand); self-driven, its saved globals carry it across the load:
// 1. the scene placed (the shotgun in the main hand, two armed grunts): the off hand empty, each grunt's gun at its hand;
//    vr_drips_soak (everything near wet) and, a moment later, vr_drips_info (`sltest: wet before` above it: the script
//    reads which things are wet);
// 2. save qvr_saveload, load qvr_saveload;
// 3. half a second after the load: the off hand still empty (no holster drawn by the load's grip), each armed grunt's gun
//    still at its hand and no spell of another kind's (the row cache, vr_armedmon.qc VR_AM_AfterLoad), and vr_drips_info
//    again (`sltest: wet after`: the script checks the wet things are still wet). The verdict line echoed.

float vr_slt_phase;  // (saved: the test goes on after the load) 0 idle, 1 waiting for the scene, 2 soaked, 3 loaded
float vr_slt_until;  // the scene's give-up time
nosave float vr_slt_frames; // frames in the phase (the load resets it: a nosave global starts at 0)
nosave float vr_slt_asked;  // the save and load asked for (the load resets it)

float VR_SLT_HAND_NEAR = 18; // a gun this near its grunt's hand (units; 2.6..10.2 before, 30+ with the bent poses)
float VR_SLT_FRAMES = 36;    // half a second at 72 Hz: the scan's looks at the wet things (ten a second) done

void() VR_SLTest_Start =
{
    VR_Test_BeginFlags("sltest", VR_TEST_LOUD | VR_TEST_ECHO);
    vr_slt_phase = 1;
    vr_slt_until = time + 30;
    vr_slt_frames = 0;
    vr_slt_asked = FALSE;
};

// Each armed humanoid's gun at its hand, and none of the grunts with a spell (another kind's row).
float() VR_SLTest_Grunts =
{
    float n = 0;
    entity e = nextent(world);
    while(e)
    {
        if(e.vr_ag && e.health > 0)
        {
            n++;
            entity q = e.vr_ag_prop;
            float d = q ? vlen(q.origin - VR_AM_Hand(e)) : 0;
            VR_Test_Check(d < VR_SLT_HAND_NEAR, sprintf("armed kind %g at %v: its gun %.1f from its hand", e.vr_ag_kind,
                e.origin, d));
            if(e.vr_ag_kind == 0)
            {
                VR_Test_Check(!VR_AM_HasSpell(e), sprintf("the grunt at %v: no spell", e.origin));
            }
        }
        e = nextent(e);
    }
    return n;
};

void(entity p, string when) VR_SLTest_Hands =
{
    VR_Test_Check(VRGetEntWeapon(p, cVR_MainHand) == WID_SHOTGUN, sprintf("%s: the main hand holds the shotgun (%g)", when,
        VRGetEntWeapon(p, cVR_MainHand)));
    VR_Test_Check(VRGetEntWeapon(p, cVR_OffHand) == WID_FIST, sprintf("%s: the off hand empty (%g, %s)", when,
        VRGetEntWeapon(p, cVR_OffHand), p.weaponmodel2));
};

// Every server frame (VR_PickupTest_Frame).
void() VR_SLTest_Frame =
{
    if(!vr_slt_phase || vr_slt_asked)
    {
        return;
    }
    entity p = VR_Player();
    if(p == world)
    {
        return;
    }
    vr_slt_frames++;
    if(vr_slt_phase == 1)
    {
        float grunts = 0;
        entity e = nextent(world);
        while(e)
        {
            grunts = grunts + (e.vr_ag && e.health > 0);
            e = nextent(e);
        }
        if(VRGetEntWeapon(p, cVR_MainHand) != WID_SHOTGUN || grunts < 2)
        {
            if(time > vr_slt_until)
            {
                VR_Test_Check(FALSE, "timed out: the waterdrips scene not placed (run vr_scenario waterdrips first)");
                vr_slt_phase = 0;
                VR_Test_Verdict();
            }
            return;
        }
        VR_SLTest_Hands(p, "before");
        VR_SLTest_Grunts();
        localcmd("vr_drips_soak\n");
        vr_slt_phase = 2;
        vr_slt_frames = 0;
        return;
    }
    if(vr_slt_frames < VR_SLT_FRAMES)
    {
        return;
    }
    if(vr_slt_phase == 2)
    {
        vr_slt_phase = 3; // (saved so: the load goes on at phase 3)
        vr_slt_asked = TRUE;
        localcmd("echo \"sltest: wet before\"\nvr_drips_info\nvr_wounds_info\nsave qvr_saveload\nload qvr_saveload\n");
        return;
    }
    VR_SLTest_Hands(p, "after the load");
    VR_Test_Check(VR_SLTest_Grunts() >= 2, "after the load: two armed grunts");
    vr_slt_phase = 0;
    localcmd("echo \"sltest: wet after\"\nvr_drips_info\nvr_wounds_info\n");
    VR_Test_Verdict(); // (echoed: after the drips' lines)
};
#endif
