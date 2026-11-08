#ifdef QVR_TESTS
// vr_load_models_test.qc -- every holdable weapon in each hand and each holster through a save and a load
// (vr_pickup_test 12; TESTING.md). A hand's and a holster's model are sent by their precache index (SV_ModelIndex): a
// model the saved game precached late (a derived one: an AD upgrade's "#vr", an AD soldier's gun's "#gun", an AD melee
// weapon's "#melee") must be precached again by the load, before the first frame's stats name it (the Widowmaker's
// "progs/v_shot3.mdl#vr not precached" crash on a quickload, 2026-11-08). The weapons go round the eight slots (the two
// hands, the six holsters), one batch per weapon, each batch saved and loaded (qvr_loadmodels.sav in quakevr/); after
// each load, each slot's weapon and model checked. The engine's "SV_ModelIndex: ... precached late" warning is a
// failure too (Misc/quakevr/load_models_test.sh reads it). Self-driven: the saved globals carry it across the loads.

float vr_lm_active; // (saved: the test goes on after each load)
float vr_lm_batch;  // the batch whose loadout was saved and loaded (-1: none yet)
float vr_lm_n;      // the weapons in the list (VR_LMTest_Count)
nosave float vr_lm_wait;   // a save and a load asked for: the load resets it (a nosave global starts at 0)
nosave float vr_lm_frames; // the frames since the load: its first stats sent before the checks

float vr_lm_wid, vr_lm_flags, vr_lm_up; // VR_LMTest_Item's

#define VR_LM_SLOTS 8
#define VR_LM_CANON 45 // the canonical list: ids 1..31, the AD melee kinds 1..11 (the sword's), the 3 AD upgrades

// Whether model `m` can be loaded: its file (a derived one's: the file it is made from, before its '#') is installed.
float(string m) VR_LMTest_Installed =
{
    if(m == "")
    {
        return FALSE;
    }
    float n = strlen(m);
    for(float i = 0; i < n; i++)
    {
        if(str2chr(m, i) == 35) // '#'
        {
            return fileexists(substring(m, 0, i));
        }
    }
    return fileexists(m);
};

// The canonical list's entry `k` into vr_lm_wid/flags/up; FALSE if it can't be held here (no such weapon, its data
// not installed).
float(float k) VR_LMTest_Canon =
{
    vr_lm_flags = 0;
    vr_lm_up = 0;
    if(k < WID_LAST)
    {
        vr_lm_wid = k + 1;
        if(vr_lm_wid == WID_ADELIMINATOR)
        {
            return FALSE; // (gone: an older save's, the centurion's)
        }
        if(VR_IsADGun(vr_lm_wid) && !VR_ADGun_Available())
        {
            return FALSE;
        }
        return VR_LMTest_Installed(WeaponIdToModel(vr_lm_wid, 0));
    }
    if(k < WID_LAST + VR_ADMELEE_LAST)
    {
        if(!vr_ad_data)
        {
            return FALSE;
        }
        vr_lm_wid = WID_SWORD;
        vr_lm_flags = VR_ADMelee_Flags(k - WID_LAST + 1);
        return VR_LMTest_Installed(WeaponIdToModel(vr_lm_wid, vr_lm_flags));
    }
    float row = k - WID_LAST - VR_ADMELEE_LAST;
    if(!vr_ad_data || row >= AD_UPGRADES_N)
    {
        return FALSE;
    }
    vr_lm_wid = AD_UPGRADES[row].wid;
    vr_lm_up = AD_UPGRADES[row].upgrade;
    return VR_LMTest_Installed(AD_UPGRADES[row].vrmodel);
};

float() VR_LMTest_Count =
{
    float n = 0;
    for(float k = 0; k < VR_LM_CANON; k++)
    {
        n += VR_LMTest_Canon(k);
    }
    return n;
};

// The list's weapon `i` (0..vr_lm_n-1) into vr_lm_wid/flags/up.
void(float i) VR_LMTest_Item =
{
    float seen = 0;
    for(float k = 0; k < VR_LM_CANON; k++)
    {
        if(VR_LMTest_Canon(k))
        {
            if(seen == i)
            {
                return;
            }
            seen++;
        }
    }
};

string() VR_LMTest_ItemName =
{
    string s = sprintf("%g", vr_lm_wid);
    if(vr_lm_flags) s = sprintf("%s/kind %g", s, VR_ADMelee_Kind(vr_lm_flags));
    if(vr_lm_up) s = sprintf("%s/upgrade %g", s, vr_lm_up);
    return s;
};

// The model the item is drawn with (its upgrade's VR model, or its id's).
string() VR_LMTest_ItemModel =
{
    return vr_lm_up ? AD_VRModelName(vr_lm_wid) : WeaponIdToModel(vr_lm_wid, vr_lm_flags);
};

// Slot `s` (0 the main hand, 1 the off hand, 2..7 the holsters 0..5) of `p` given the item (vr_lm_*) as a give does:
// its model made and precached (late: none of e1m1's spawns precaches these), its record, its model string.
void(entity p, float s) VR_LMTest_Put =
{
    VR_ADGun_Placed(vr_lm_wid);
    VR_ADMelee_Placed(vr_lm_wid, vr_lm_flags);
    VR_WeaponUtil_EntAddWeapon(p, vr_lm_wid);
    float clip = WeaponIdToBaseClipSize(vr_lm_wid);
    string m = WeaponIdToModel(vr_lm_wid, vr_lm_flags);
    precache_model(m);
    if(s < 2)
    {
        float hand = s == 0 ? cVR_MainHand : cVR_OffHand;
        VRGiveEntWeapon(p, hand, vr_lm_wid, vr_lm_flags, clip);
        VRGetEntWeaponInst(p, hand).wi_adup = vr_lm_up;
        VR_Scene_HoldGiven(p, hand); // (held, its grip unpressed: the mock's hands grip nothing)
        if(hand == cVR_MainHand) p.weaponmodel = m; else p.weaponmodel2 = m;
        return;
    }
    entity inst = WeaponInst_New(clip);
    inst.wi_adup = vr_lm_up;
    if(s == 2) { p.holsterweapon0 = vr_lm_wid; p.holsterweaponflags0 = vr_lm_flags; p.holsterweaponinst0 = inst; p.holsterweaponmodel0 = m; }
    if(s == 3) { p.holsterweapon1 = vr_lm_wid; p.holsterweaponflags1 = vr_lm_flags; p.holsterweaponinst1 = inst; p.holsterweaponmodel1 = m; }
    if(s == 4) { p.holsterweapon2 = vr_lm_wid; p.holsterweaponflags2 = vr_lm_flags; p.holsterweaponinst2 = inst; p.holsterweaponmodel2 = m; }
    if(s == 5) { p.holsterweapon3 = vr_lm_wid; p.holsterweaponflags3 = vr_lm_flags; p.holsterweaponinst3 = inst; p.holsterweaponmodel3 = m; }
    if(s == 6) { p.holsterweapon4 = vr_lm_wid; p.holsterweaponflags4 = vr_lm_flags; p.holsterweaponinst4 = inst; p.holsterweaponmodel4 = m; }
    if(s == 7) { p.holsterweapon5 = vr_lm_wid; p.holsterweaponflags5 = vr_lm_flags; p.holsterweaponinst5 = inst; p.holsterweaponmodel5 = m; }
};

float vr_lm_got_wid; // VR_LMTest_Slot's
string vr_lm_got_model;

void(entity p, float s) VR_LMTest_Slot =
{
    if(s == 0) { vr_lm_got_wid = p.weapon; vr_lm_got_model = p.weaponmodel; }
    if(s == 1) { vr_lm_got_wid = p.weapon2; vr_lm_got_model = p.weaponmodel2; }
    if(s == 2) { vr_lm_got_wid = p.holsterweapon0; vr_lm_got_model = p.holsterweaponmodel0; }
    if(s == 3) { vr_lm_got_wid = p.holsterweapon1; vr_lm_got_model = p.holsterweaponmodel1; }
    if(s == 4) { vr_lm_got_wid = p.holsterweapon2; vr_lm_got_model = p.holsterweaponmodel2; }
    if(s == 5) { vr_lm_got_wid = p.holsterweapon3; vr_lm_got_model = p.holsterweaponmodel3; }
    if(s == 6) { vr_lm_got_wid = p.holsterweapon4; vr_lm_got_model = p.holsterweaponmodel4; }
    if(s == 7) { vr_lm_got_wid = p.holsterweapon5; vr_lm_got_model = p.holsterweaponmodel5; }
};

// Batch `b`: slot s holds the list's weapon (b + s) mod n (each weapon in every slot once over the n batches).
void(entity p, float b) VR_LMTest_SetBatch =
{
    for(float s = 0; s < VR_LM_SLOTS; s++)
    {
        VR_LMTest_Item(mod(b + s, vr_lm_n));
        VR_LMTest_Put(p, s);
    }
    AD_SyncWeaponModels(p); // (the upgraded ones drawn as themselves: their "#vr" models made and precached)
};

void(entity p, float b) VR_LMTest_CheckBatch =
{
    for(float s = 0; s < VR_LM_SLOTS; s++)
    {
        VR_LMTest_Item(mod(b + s, vr_lm_n));
        VR_LMTest_Slot(p, s);
        string want = VR_LMTest_ItemModel();
        float ok = vr_lm_got_wid == vr_lm_wid && vr_lm_got_model == want;
        if(!ok)
        {
            VR_Test_Check(FALSE, sprintf("batch %g slot %g: weapon %s, model %s after the load (got weapon %g, model %s)",
                b, s, VR_LMTest_ItemName(), want, vr_lm_got_wid, vr_lm_got_model));
        }
        else
        {
            vr_test_pass++;
        }
    }
};

// vr_pickup_test 12: the test begins (e1m1 or any map; AD's data, vr_ad_upgrades 2 for the upgrades).
void() VR_LMTest_Start =
{
    VR_Test_BeginFlags("loadmodels", VR_TEST_LOUD);
    vr_lm_n = VR_LMTest_Count();
    string names = "";
    for(float i = 0; i < vr_lm_n; i++)
    {
        VR_LMTest_Item(i);
        names = sprintf("%s %s", names, VR_LMTest_ItemName());
    }
    VR_Test_Print(sprintf("loadmodels: %g weapons (id/AD melee kind/AD upgrade):%s\n", vr_lm_n, names));
    if(vr_lm_n < VR_LM_SLOTS)
    {
        VR_Test_Check(FALSE, "fewer weapons than slots");
        VR_Test_Verdict();
        return;
    }
    vr_lm_active = TRUE;
    vr_lm_batch = -1;
    vr_lm_wait = FALSE;
    vr_lm_frames = 0;
};

// Every server frame (VR_PickupTest_Frame): the next step once the load's first frames ran.
void() VR_LMTest_Frame =
{
    if(!vr_lm_active || vr_lm_wait)
    {
        return;
    }
    entity p = VR_Player();
    if(p == world)
    {
        return;
    }
    vr_lm_frames++;
    if(vr_lm_frames < 3) // (the load's first frames: every hand's and holster's model sent by its index)
    {
        return;
    }
    vr_lm_frames = 0;
    if(vr_lm_batch >= 0)
    {
        VR_LMTest_CheckBatch(p, vr_lm_batch);
    }
    vr_lm_batch++;
    if(vr_lm_batch >= vr_lm_n)
    {
        vr_lm_active = FALSE;
        VR_Test_Verdict();
        return;
    }
    VR_LMTest_SetBatch(p, vr_lm_batch);
    vr_lm_wait = TRUE;
    localcmd("save qvr_loadmodels\nload qvr_loadmodels\n");
};
#endif
