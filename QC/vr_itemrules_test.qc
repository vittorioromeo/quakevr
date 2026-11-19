#ifdef QVR_TESTS
// vr_pickup_test 19 (vr_itemrules_test.qc): an item's targets fire only when it is taken (itemTouch, honey_items.qc:
// item_taken). Each item is spawned ahead of you by its map spawn function, placed at once (VRPlaceItemNow), given a
// target that counts its uses, and touched through itemTouch (what a hand, the body and a box carried to a holster
// reach) first in a way that takes nothing, then in one that takes it: a health box at full health, a box of shells at
// the cap, the silver key already held, green armour under better armour, a super shotgun brushed by a hand that
// doesn't grip (then gripped). Not taken: the item is still there and its target has not fired; taken: it fired once.
// With Honey's extensions on (the start map with vr_honey_context 1) a secret health box also counts as found only
// when taken. Verdict "itemtargets: N passed, M failed". Single player, any map (e1m1).

float vr_itt_fired; // the counting target's uses

void() VR_ITT_Use =
{
    vr_itt_fired = vr_itt_fired + 1;
};

// The map's own spawn function of `xClass` at `xAt`, placed now, its target the counting one ("vr_itt_target").
entity(string xClass, vector xAt, float xFlags) VR_ITT_Spawn =
{
    entity oldself = self;
    entity it = spawn();
    setorigin(it, xAt);
    it.classname = xClass;
    it.spawnflags = xFlags;
    it.target = "vr_itt_target";
    self = it;
    if(xClass == "item_health") { item_health(); }
    else if(xClass == "item_shells") { item_shells(); }
    else if(xClass == "item_key1") { item_key1(); }
    else if(xClass == ITEM_NAME_ARMOR1) { item_armor1(); }
    else { weapon_supershotgun(); }
    VRPlaceItemNow();
    self = oldself;
    return it;
};

// `p`'s touch of `xItem` through itemTouch by the main hand (what its handtouch, or a box's carry_use, is just placed).
void(entity p, entity xItem) VR_ITT_Touch =
{
    entity oldself = self, oldother = other;
    p.touchinghand = cVR_MainHand;
    xItem.handtouch_hand = cVR_MainHand;
    xItem.handtouch_ent = p;
    other = p;
    self = xItem;
    itemTouch();
    self = oldself;
    other = oldother;
};

// One item: touched as `xLeave` sets the player (nothing taken), then as `xTake` does (taken).
float vr_itt_case;
void(entity p, float xStep) VR_ITT_Set =
{
    float k = vr_itt_case;
    if(k == 0) { p.health = xStep ? 50 : p.max_health; }
    else if(k == 1) { p.ammo_shells = xStep ? 0 : VR_MaxAmmo(p, AID_SHELLS); }
    else if(k == 2) { if(xStep) { VR_ItemUtil_EntDelItem(p, IID_KEY1); } else { VR_ItemUtil_EntAddItem(p, IID_KEY1); } }
    else if(k == 3) { p.armortype = xStep ? 0 : 0.8; p.armorvalue = xStep ? 0 : 200; }
    else { VRSetMainHandGrabbingBit(p, xStep); }
};

void(entity p, string xClass, string xWhat, float xFlags) VR_ITT_Case =
{
    makevectors(p.v_angle_y * '0 1 0');
    entity it = VR_ITT_Spawn(xClass, p.origin + v_forward * 64, xFlags);
    VR_Test_Check(it.handtouch == itemTouch || it.carry_use == itemTouch,
        sprintf("%s: its pickup goes through itemTouch", xClass));
    float secrets = found_secrets;
    vr_itt_fired = 0;
    VR_ITT_Set(p, 0);
    VR_ITT_Touch(p, it);
    VR_Test_Check(it.model != string_null && vr_itt_fired == 0,
        sprintf("%s touched %s: not taken (%s), its target fired %g times (0)", xClass, xWhat,
        it.model != string_null ? "still there" : "gone", vr_itt_fired));
    if(xFlags & HONEY_SPAWNFLAG_ITEM_SECRET)
    {
        VR_Test_Check(found_secrets == secrets, sprintf("%s: a secret not taken is not found (%g, was %g)", xClass,
            found_secrets, secrets));
    }
    VR_ITT_Set(p, 1);
    VR_ITT_Touch(p, it);
    VR_Test_Check(it.model == string_null && vr_itt_fired == 1,
        sprintf("%s taken: %s, its target fired %g times (1)", xClass, it.model == string_null ? "gone" : "still there",
        vr_itt_fired));
    if(xFlags & HONEY_SPAWNFLAG_ITEM_SECRET)
    {
        VR_Test_Check(found_secrets == secrets + 1, sprintf("%s: the secret taken is found (%g, was %g)", xClass,
            found_secrets, secrets));
    }
    vr_itt_case = vr_itt_case + 1;
};

void() VR_ItemTargetsTest_Run =
{
    VR_Test_BeginFlags("itemtargets", VR_TEST_LOUD);
    entity p = self;
    entity counter = spawn();
    counter.classname = "vr_itt_counter";
    counter.targetname = "vr_itt_target";
    counter.use = VR_ITT_Use;
    float oldHealth = p.health, oldShells = p.ammo_shells, oldType = p.armortype, oldArmor = p.armorvalue;
    float hadKey = VR_ItemUtil_EntHasItem(p, IID_KEY1);
    VR_Test_Check(VRIsHandEmpty(p, cVR_MainHand), "the main hand is empty (for the weapon's grip)");

    vr_itt_case = 0;
    VR_ITT_Case(p, "item_health", "at full health", 0);
    VR_ITT_Case(p, "item_shells", "with shells at the cap", 0);
    VR_ITT_Case(p, "item_key1", "with the key held", 0);
    VR_ITT_Case(p, ITEM_NAME_ARMOR1, "under red armour", 0);
    VR_ITT_Case(p, "weapon_supershotgun", "by a hand that doesn't grip", 0);
    VRSetMainHandGrabbingBit(p, FALSE);
    if(VR_GameUtil_EnableHoneyExtensions())
    {
        vr_itt_case = 0;
        VR_ITT_Case(p, "item_health", "at full health (a Honey secret)", HONEY_SPAWNFLAG_ITEM_SECRET);
    }
    else
    {
        VR_Test_Print("itemtargets: (Honey's extensions off here: the secret case not run)\n");
    }

    remove(counter);
    VRSetEntWeapon(p, cVR_MainHand, WID_FIST);
    p.health = oldHealth;
    p.ammo_shells = oldShells;
    p.armortype = oldType;
    p.armorvalue = oldArmor;
    if(hadKey) { VR_ItemUtil_EntAddItem(p, IID_KEY1); } else { VR_ItemUtil_EntDelItem(p, IID_KEY1); }
    VR_Test_Verdict();
};
#endif
