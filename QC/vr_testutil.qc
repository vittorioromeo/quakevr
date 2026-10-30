#ifdef QVR_TESTS
// vr_testutil.qc -- the QC tests' shared tally (CODE_STYLE.md, "Canonical helpers"; TESTING.md). A test starts with
// VR_Test_Begin("<tag>"), counts each check with VR_Test_Check(ok, what) (a `<tag>: PASS|FAIL <what>` line) and ends
// with its verdict line (VR_Test_Verdict: `<tag>: <p> passed, <f> failed`, the form run.sh -QuitOn and
// run_baseline.py read). One test runs at a time: the tally is the running test's.

string vr_test_tag;  // the running test's line prefix ("nailtest")
float vr_test_pass;  // its checks passed so far
float vr_test_fail;  // and failed

// A test (or one of its parts) starts: its tag, the tally cleared.
void(string tag) VR_Test_Begin =
{
    vr_test_tag = tag;
    vr_test_pass = 0;
    vr_test_fail = 0;
};

// One check: counted, and its line printed (developer 1).
void(float ok, string what) VR_Test_Check =
{
    if(ok) vr_test_pass++; else vr_test_fail++;
    dprint(sprintf("%s: %s %s\n", vr_test_tag, ok ? "PASS" : "FAIL", what));
};

// The test's verdict line.
void() VR_Test_Verdict =
{
    dprint(sprintf("%s: %g passed, %g failed\n", vr_test_tag, vr_test_pass, vr_test_fail));
};
// The tests' frames (StartFrame's last step, in this order): each reads its request cvar and runs or checks its case.
// (Tests always built: VR_DeadAnim_TestFrame, VR_KdTrig_Frame, stay in StartFrame.)
void() VR_TestsFrame =
{
    MG_TestFrame();
    MG_TriggerTestFrame();
    MG1_H_TestFrame();
    MGH_TestFrame();
    MG3_TestFrame();
    VR_MonstersTest_Frame(); // vr_test_monsters (vr_monsters_test.qc)
    VR_MeleeArcTest_Frame(); // vr_test_meleearc (vr_meleearc_test.qc)
    VR_AimConeTest_Frame(); // vr_test_aimcone (vr_aimcone_test.qc)
    MG3_WeaponsTestFrame(); // vr_mg3_wtest (vr_mg3_weapons_test.qc)
    VR_NailPistol_TestFrame(); // vr_np_test (vr_nailpistol_test.qc)
    VR_NailTest_Frame(); // vr_nail_test (vr_nail_test.qc)
    VR_FlareGun_TestFrame(); // vr_flare_test (vr_flaregun_test.qc)
    VR_AmmoColor_TestFrame(); // vr_ammocolor_test (vr_ammocolor_test.qc)
    VR_Revolver_TestFrame(); // vr_rv_test (vr_revolver_test.qc)
    MG3_MonstersTestFrame(); // vr_mg3_mtest (vr_mg3_monsters_test.qc)
    AD_MonstersTestFrame(); // AD support: vr_ad_mtest (vr_ad_monsters_test.qc)
    MG3_BestiaryTestFrame(); // vr_mg3_btest (vr_mg3_bestiary_test.qc)
    MG3_ShubTestFrame(); // vr_mg3_shubtest (vr_mg3_shub_test.qc)
    MG3_ChthonTestFrame(); // vr_mg3_ctest (vr_mg3_chthon_test.qc)
    VR_SmallGib_TestFrame(); // VR: the small gibs' tests (vr_smallgibs_test, vr_smallgibs_test.qc)
    VR_GibProps_TestFrame(); // VR: every monster's head and gibs as props (vr_gibprops_test, vr_gibprops_test.qc)
    VR_PropImp_TestFrame(); // VR: the props' impact rule (vr_prop_impact_test, vr_propimpact_test.qc)
    VR_TestDispenser_Frame(); // VR: a map's monster dispenser used from the console (vr_test_dispenser <n>, buttons.qc)
    VR_TestKill_Frame(); // VR: the nearest live monster killed by a shot struck where asked (vr_test_kill <n>, combat.qc)
    VR_TestHurt_Frame(); // VR: the nearest live monster hurt by a shot of the player's (vr_test_hurt <damage>, combat_test.qc)
    VR_TestHit_Frame(); // VR: the nearest live monster struck where asked, never killed (vr_test_hit <n>, vr_hitreact.qc)
    VR_DropCapTest_Frame(); // VR: the cap on dropped enemy weapons' test (vr_dropcap_test, vr_dropcap_test.qc)
    VR_AgArmor_TestFrame(); // VR: armour on armed humanoids' test (vr_agarmor_test, vr_agarmor_test.qc)
    VR_CrouchTest_Frame(); // VR: the crouched box's shots test (vr_crouch_test, vr_crouch_test.qc)
    VR_Decap_TestFrame(); // VR: decapitation's tests (vr_decap_test, vr_decap.qc)
    VR_Melee_SweepTestFrame(); // VR: the swept blade's test (vr_melee_sweep_test, vr_melee_sweep_test.qc)
    VR_Limb_TestFrame(); // VR: limb gore's tests (vr_limb_test, vr_limbs_test.qc)
    VR_PickupTest_Frame(); // VR: the weapon pickups' tests (vr_pickup_test, vr_pickup_test.qc)
    VR_DCTest_Frame(); // VR: the weapons' damage credit test (vr_dmgcredit_test, vr_dmgcredit_test.qc)
    VR_AGTest_Frame(); // VR: the armed grunts' tests (vr_agrunt_test, vr_armedgrunt_test.qc)
    VR_Dummy_TestFrame(); // VR: the training dummy's tests (vr_dummy_test, vr_dummy.qc)
    VR_Burn_TestFrame(); // VR: the burning tests (vr_burn_test, vr_burning.qc)
    VR_HotBlade_TestFrame(); // VR: the incandescent blades' tests (vr_hotblade_test, vr_hotblade.qc)
    VR_Shock_TestFrame(); // VR: lightning's lasting shock's test (vr_shock_hit_test, vr_shock.qc)
    VR_DentsTest_Frame(); // VR: every hit dents, the damage paths' test (vr_dents_test, vr_dents_test.qc)
    VR_ARENA_Test_Frame(); // VR: the Survival Arena's tests (vr_arena_test, vr_arena_test.qc)
    VR_ARENA_PERKS_Test_Frame(); // VR: the Survival Arena's perk tests (vr_arena_perks_test, vr_arena_perks_test.qc)
};
#endif
