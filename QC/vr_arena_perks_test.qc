#ifdef QVR_TESTS
// vr_arena_perks_test.qc -- the Survival Arena's perk tests (vr_arena_perks.qc). `vr_arena_perks_test N`
// (Debug > Automation & Self-Tests; VR_ARENA_PERKS_Test_Frame, world.qc's StartFrame), the arena running
// (`vr_arena 1` before the map, or the map "vrarena") and the player kept alive (`god`); each ends on
// `arena_perks: N passed, N failed` (run.sh -QuitOn "arena_perks: [0-9]+ passed").
// - 1 the pool and its knobs: seven distinct perks, each named, described and wearing its own model; the label
//   line; the taken perks kept on the director, never twice; how many are left to offer; the offer count clamped.
// - 2 (more below, as the offer and the perks are built)

// ---------------------------------------------------------------------------
// The test's own state
// ---------------------------------------------------------------------------

float ark_t_case;        // 0 idle; 1, 2, ... running
float ark_t_pass;
float ark_t_fail;
float ark_t_step;
float ark_t_until;
float ark_t_keep[4];     // the cvars the tests set, put back at the end
float ark_t_taken0;      // the perks the run had already taken
string ark_t_prefix;     // its verdict line ("arena_perks"; the full-map check prints `arena_full: ...`)

string(float i) VR_APK_CvarName =
{
    if(i == 0) { return "vr_arena_perks"; }
    if(i == 1) { return "vr_arena_perk_count"; }
    return "";
}
float VR_APK_NCVARS = 2;

void() VR_APK_Save =
{
    float i;
    for(i = 0; i < VR_APK_NCVARS; i++) { ark_t_keep[i] = cvar(VR_APK_CvarName(i)); }
}

void() VR_APK_Restore =
{
    float i;
    for(i = 0; i < VR_APK_NCVARS; i++) { cvar_set(VR_APK_CvarName(i), ftos(ark_t_keep[i])); }
}

// The tests' settings: the perk file's own defaults, so a check says what the perk says and not what a config said.
void() VR_APK_SetDefaults =
{
    cvar_set("vr_arena_perks", "1");
    cvar_set("vr_arena_perk_count", "3");
}

void(float ok, string what) VR_APK_Check =
{
    if(ok) { ark_t_pass = ark_t_pass + 1; return; }
    ark_t_fail = ark_t_fail + 1;
    dprint(sprintf("arena_perks: fail %s\n", what));
}

void() VR_APK_End =
{
    dprint(sprintf("%s: %g passed, %g failed\n", ark_t_prefix, ark_t_pass, ark_t_fail));
    VR_APK_Restore();
    if(ar_mgr) { ar_mgr.ark_taken = ark_t_taken0; } // (the run's perks are the test's, not the player's)
    cvar_set("vr_arena_perks_test", "0");
    ark_t_case = 0;
}

// ---------------------------------------------------------------------------
// Test 1: the pool, its labels and its knobs (nothing in the field needed)
// ---------------------------------------------------------------------------

void() VR_APK1_Pool =
{
    float i;
    float bit;
    float seen = 0;
    float distinct = TRUE;

    for(i = 0; i < ARK_POOL_N; i++)
    {
        bit = VR_ARK_PoolBit(i);
        if(bit == 0) { distinct = FALSE; }
        if((seen & bit) != 0) { distinct = FALSE; }
        seen = seen | bit;
    }
    VR_APK_Check(distinct, "the pool holds seven distinct perks");
    VR_APK_Check(VR_ARK_PoolBit(ARK_POOL_N) == 0, "the pool has nothing past its last rung");

    // Named, described, and each pedestal wears a model of its own.
    float named = TRUE;
    for(i = 0; i < ARK_POOL_N; i++)
    {
        bit = VR_ARK_PoolBit(i);
        if(VR_ARK_Name(bit) == "") { named = FALSE; }
        if(VR_ARK_What(bit) == "") { named = FALSE; }
        if(VR_ARK_Model(bit) == "") { named = FALSE; }
        float j;
        for(j = i + 1; j < ARK_POOL_N; j++)
        {
            if(VR_ARK_Model(bit) == VR_ARK_Model(VR_ARK_PoolBit(j))) { named = FALSE; }
        }
    }
    VR_APK_Check(named, "every perk is named, described, and wears a model no other perk wears");

    // The line a pedestal is named by: the perk's name, then what it does.
    float labelled = TRUE;
    for(i = 0; i < ARK_POOL_N; i++)
    {
        bit = VR_ARK_PoolBit(i);
        string lab = VR_ARK_Label(bit);
        string nm = VR_ARK_Name(bit);
        if(strlen(lab) <= strlen(nm)) { labelled = FALSE; }
        if(substring(lab, 0, strlen(nm)) != nm) { labelled = FALSE; }
    }
    VR_APK_Check(labelled, "a pedestal's line reads its name and then what it does");

    // The knobs.
    cvar_set("vr_arena_perk_count", "9");
    VR_APK_Check(VR_ARK_Opt_Count() == ARK_POOL_N, "the offer count is clamped to the pool's size");
    cvar_set("vr_arena_perk_count", "0");
    VR_APK_Check(VR_ARK_Opt_Count() == 1, "at least one perk is offered");
    cvar_set("vr_arena_perk_count", "3");
    VR_APK_Check(VR_ARK_Opt_Count() == 3, "three perks are offered by default");

    cvar_set("vr_arena_perks", "0");
    VR_APK_Check(!VR_ARK_Opt_On(), "vr_arena_perks 0 turns the offer off");
    cvar_set("vr_arena_perks", "1");
    VR_APK_Check(VR_ARK_Opt_On(), "vr_arena_perks 1 (the default) leaves the offer on");
}

void() VR_APK1_State =
{
    float i;

    ar_mgr.ark_taken = 0;
    VR_APK_Check(!VR_ARK_Has(ARK_QUAD), "a fresh run has taken no perk");
    VR_APK_Check(VR_ARK_CountTaken() == 0, "a fresh run counts no perk taken");
    VR_APK_Check(VR_ARK_CountLeft() == ARK_POOL_N, "a fresh run may offer every perk in the pool");

    VR_APK_Check(VR_ARK_Take(ARK_QUAD), "a perk can be taken");
    VR_APK_Check(VR_ARK_Has(ARK_QUAD), "the director keeps the perk the player took");
    VR_APK_Check(!VR_ARK_Take(ARK_QUAD), "the same perk is not taken twice");
    VR_APK_Check(VR_ARK_CountTaken() == 1, "and the run holds exactly one");
    VR_APK_Check(!VR_ARK_CanOffer(ARK_QUAD), "a perk already taken is not offered again");
    VR_APK_Check(VR_ARK_CanOffer(ARK_FEED), "a perk not taken still may be offered");

    for(i = 0; i < ARK_POOL_N; i++) { VR_ARK_Take(VR_ARK_PoolBit(i)); }
    VR_APK_Check(VR_ARK_CountLeft() == 0, "a run that took every perk has nothing left to offer");
}

void() VR_APK1_Run =
{
    if(!ar_mgr)
    {
        VR_APK_Check(FALSE, "the arena's director is not in this map (vr_arena 1 before the map, or vrarena)");
        return;
    }
    VR_APK1_Pool();
    VR_APK1_State();
}

// ---------------------------------------------------------------------------
// The frame (world.qc's StartFrame)
// ---------------------------------------------------------------------------

void() VR_ARENA_PERKS_Test_Frame =
{
    float c = cvar("vr_arena_perks_test");

    if(c == 0)
    {
        if(ark_t_case) { VR_APK_Restore(); ark_t_case = 0; dprint("arena_perks: test stopped\n"); }
        return;
    }

    if(ark_t_case == 0)
    {
        ark_t_case = c;
        ark_t_pass = 0;
        ark_t_fail = 0;
        ark_t_step = 0;
        ark_t_until = time + 30;
        ark_t_prefix = "arena_perks";
        VR_APK_Save();
        ark_t_taken0 = 0;
        if(ar_mgr) { ark_t_taken0 = ar_mgr.ark_taken; }
        VR_APK_SetDefaults();
        dprint(sprintf("arena_perks: test %g begin\n", c));

        if(c == 1)
        {
            VR_APK1_Run();
            VR_APK_End();
            return;
        }

        VR_APK_Check(FALSE, "no such arena perk test");
        VR_APK_End();
        return;
    }

    if(ark_t_case != c) { return; } // (a different case asked for while one runs: the running one finishes)

    if(time > ark_t_until)
    {
        VR_APK_Check(FALSE, "the test timed out");
        VR_APK_End();
        return;
    }
}
#endif
