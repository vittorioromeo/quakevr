#ifdef QVR_TESTS
// vr_pickup_test 14 (vr_meleeonly_test.qc): Melee Only's refusals (vr_run_meleeonly.qc; his rule of 2026-11-13). Each
// thing is laid ahead of you and offered to the off hand, empty, four ways: the force grab's aim (VR_Forcegrab_IsEligible),
// a grip closing on it (its hand touch, the grip just pressed), a force grab's catch (its hand touch, forcegrabCatching)
// and a walk over it (its touch, the body's fake hand, vr_carry 0: classic pickups). With Melee Only on: a grunt's
// dropped burst rifle, a map's shotgun pickup, a crate's flare gun and a box of shells are never taken nor force grabbed;
// an axe and a dagger lying about are. With it off, the same guns and the box are taken (the paths are the live ones).
// Melee Only set back as it was. Verdict "meleeonly: N passed, M failed". Any map in single player (e1m1).

float VR_MOT_KINDS = 6;

string(float k) VR_MOT_Name =
{
    if(k == 0) { return "a grunt's dropped burst rifle"; }
    if(k == 1) { return "a map's shotgun pickup"; }
    if(k == 2) { return "a crate's flare gun"; }
    if(k == 3) { return "a box of shells"; }
    if(k == 4) { return "an axe lying about"; }
    return "a dagger lying about";
};

// Kind `k` laid 32 units ahead of `pl`, placed as a map places it (an item's PlaceItem run now).
entity(entity pl, float k) VR_MOT_Make =
{
    makevectors(pl.v_angle_y * '0 1 0');
    vector at = pl.origin + v_forward * 32;
    entity w = world;
    if(k == 0 || k == 4 || k == 5)
    {
        float wid = k == 0 ? WID_GRUNTGUN : (k == 4 ? WID_AXE : WID_SWORD);
        float fl = k == 5 ? VR_ADMelee_Flags(VR_SWORDKIND_DAGGER) : 0;
        return CreateThrownWeapon(pl, wid, fl, WeaponInst_New(WeaponIdToBaseClipSize(wid)), at, '0 0 0', '0 0 0', '0 0 0',
            FALSE /* disappear on hit */, FALSE /* deal damage */, 0);
    }
    if(k == 2)
    {
        return VR_FlareGun_Spawn(pl, at, '0 0 90', '0 0 0', VR_FlareGun_Barrels());
    }
    entity oself = self;
    w = spawn();
    setorigin(w, at + '0 0 8');
    self = w;
    if(k == 1)
    {
        VR_PickupTest_SpawnFunc(WID_SHOTGUN);
    }
    else
    {
        w.classname = "item_shells";
        item_shells();
    }
    if(w.think == PlaceItem)
    {
        w.think(); // (placed now, not 0.2 s later)
    }
    if(w.think2 == VR_PickupObj_Place)
    {
        w.think2(); // (made an object now: Item Pickup Mode's)
    }
    self = oself;
    return w;
};

// `pl`'s off hand (empty) offered kind `k` by way `how` (1 a grip, 2 a force grab's catch, 3 a walk over it): whether it
// was taken (in the hand, held, or gone into the pack). The thing removed and the hand emptied after.
float(entity pl, float k, float how) VR_MOT_Offer =
{
    float h = cVR_OffHand;
    entity oldOther = other;
    entity oself = self;
    if(!VRIsHandWeaponless(pl, h))
    {
        VRSetHandEmpty(pl, h);
    }
    pl.ammo_shells = 0;
    entity w = VR_MOT_Make(pl, k);
    if(w == world || w.model == string_null)
    {
        return FALSE;
    }
    float bits = pl.vrbits0;
    float grabT = pl.offhand_lastgrabtime;
    float relT = pl.offhand_lastreleasetime;
    float oldHand = pl.touchinghand;
    float catching = forcegrabCatching;
    if(how == 1)
    {
        pl.vrbits0 = (pl.vrbits0 | QVR_VRBITS0_OFFHAND_GRABBING) - (pl.vrbits0 & QVR_VRBITS0_OFFHAND_PREVGRABBING);
        pl.offhand_lastgrabtime = time; // (the grab started now: VR_HandGrabUtil_StartedHandGrabbing)
        pl.offhand_lastreleasetime = time - 1;
    }
    forcegrabCatching = how == 2;
    pl.touchinghand = how == 3 ? cVR_FakeHand : h;
    w.handtouch_ent = pl;
    w.handtouch_hand = h;
    self = w;
    other = pl;
    if(how == 3)
    {
        // (Classic pickups, Item Pickup Mode Vanilla: walking into it takes it, as flat play's walk does.)
        float carry = cvar_hget(cvarh_vr_carry);
        cvar_set("vr_carry", "0");
        if(w.handtouch) { w.handtouch(); } // (the body's touch: the engine's, with the fake hand)
        cvar_set("vr_carry", ftos(carry));
    }
    else if(w.classname == "thrown_weapon")
    {
        wpnthrow_handtouch_ent_hand(w, h);
    }
    else if(w.handtouch)
    {
        w.handtouch();
    }
    self = oself;
    other = oldOther;
    forcegrabCatching = catching;
    pl.touchinghand = oldHand;
    pl.vrbits0 = bits;
    pl.offhand_lastgrabtime = grabT;
    pl.offhand_lastreleasetime = relT;
    float inHand = !VRIsHandWeaponless(pl, h);
    float held = w.model != string_null && w.carry_player == pl;
    float took = inHand || held || w.model == string_null || pl.ammo_shells > 0;
    if(held)
    {
        VR_Hand_Release(pl, h, "test");
    }
    if(inHand)
    {
        VRSetHandEmpty(pl, h);
        self = pl;
        W_SetCurrentAmmo();
        self = oself;
    }
    if(w.model != string_null)
    {
        w.carry_player = world;
        remove(w);
    }
    pl.ammo_shells = 0;
    return took;
};

// Whether the force grab would aim at kind `k` (laid ahead, removed after).
float(entity pl, float k) VR_MOT_Eligible =
{
    entity w = VR_MOT_Make(pl, k);
    if(w == world || w.model == string_null)
    {
        return FALSE;
    }
    float ok = VR_Forcegrab_IsEligible(w);
    remove(w);
    return ok;
};

void() VR_MeleeOnlyTest_Run =
{
    entity pl = self;
    VR_Test_BeginFlags("meleeonly", VR_TEST_LOUD);
    float was = cvar_hget(cvarh_vr_run_meleeonly);
    float holds = VRGetEntWeapon(pl, cVR_OffHand);
    float holdFlags = VRGetEntWeaponFlags(pl, cVR_OffHand);
    cvar_set("vr_run_meleeonly", "1");
    for(float k = 0; k < VR_MOT_KINDS; ++k)
    {
        float melee = k >= 4;
        string s = VR_MOT_Name(k);
        string want = melee ? "taken" : "refused";
        VR_Test_Check(VR_MOT_Eligible(pl, k) == melee, sprintf("melee only: %s, force grab %s", s, want));
        VR_Test_Check(VR_MOT_Offer(pl, k, 1) == melee, sprintf("melee only: %s, hand grab %s", s, want));
        VR_Test_Check(VR_MOT_Offer(pl, k, 2) == melee, sprintf("melee only: %s, force grab's catch %s", s, want));
        if(k == 1 || k == 3)
        {
            VR_Test_Check(!VR_MOT_Offer(pl, k, 3), sprintf("melee only: %s, walked over: refused", s));
        }
    }
    VR_Test_Check(VR_Run_RefusesFire(WID_GRUNTGUN) && VR_Run_RefusesFire(WID_FLAREGUN) && !VR_Run_RefusesFire(WID_SWORD),
        "melee only: the burst rifle and the flare gun only click, the sword strikes");
    cvar_set("vr_run_meleeonly", "0");
    for(float j = 0; j < 4; ++j)
    {
        string t = VR_MOT_Name(j);
        VR_Test_Check(VR_MOT_Eligible(pl, j), sprintf("off: %s, force grab taken", t));
        VR_Test_Check(VR_MOT_Offer(pl, j, 1), sprintf("off: %s, hand grab taken", t));
        if(j == 1 || j == 3)
        {
            VR_Test_Check(VR_MOT_Offer(pl, j, 3), sprintf("off: %s, walked over: taken", t));
        }
    }
    cvar_set("vr_run_meleeonly", ftos(was));
    if(holds != VRGetEntWeapon(pl, cVR_OffHand))
    {
        VRSetEntWeapon(pl, cVR_OffHand, holds);
        VRSetEntWeaponFlags(pl, cVR_OffHand, holdFlags);
    }
    VR_Test_Verdict();
};
#endif
