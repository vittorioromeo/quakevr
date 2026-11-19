#ifdef QVR_TESTS
// vr_rules_test.qc -- tests of the rulesets' shared adapters (vr_rules_monsters.qc), after every import so they see
// each mod's functions. Dispatched by vr_ad_test 199 (AD_RulesTest), any map.

// One of our monsters at another ruleset's path_corner (corner A, a wait and a delay, then corner B), touched by that
// corner's own touch (AD's, Copper's): it follows it as ours, its think never NULL (Copper's touch set think =
// cu_th_stand, NULL on ours: d1m7's "NULL function"). TRUE: passed.
float(string name, void() touchfn) VR_Rules_PathCornerCase =
{
    entity a = spawn(), b = spawn(), m = spawn();
    a.classname = "path_corner";
    a.targetname = "vr_rules_pc_a";
    a.target = "vr_rules_pc_b";
    a.wait = 1;
    a.delay = 1;
    b.classname = "path_corner";
    b.targetname = "vr_rules_pc_b";
    m.classname = "monster_wizard";
    m.flags = FL_MONSTER | FL_FLY;
    m.health = 80;
    m.th_stand = wiz_stand1;
    m.th_walk = wiz_walk1;
    m.th_run = wiz_run1;
    m.th_die = wiz_die;
    m.goalentity = m.movetarget = a;
    m.think = SUB_Null;
    entity os = self, oo = other;
    self = a;
    other = m;
    touchfn();
    self = os;
    other = oo;
    float ok = m.think && m.goalentity == b && m.movetarget == b;
    VR_Test_Check(ok, sprintf("%s's path_corner: think %s, goal %s", name, m.think ? "set" : "NULL",
        m.goalentity == b ? "the next corner" : "not the next corner"));
    remove(a);
    remove(b);
    remove(m);
    return ok;
};

// vr_ad_test 199: our monster on AD's and Copper's path_corners, and the guard naming a monster with a NULL th_die.
void(entity p) VR_Rules_PathCornerTest =
{
    VR_Test_BeginFlags("rulespc", VR_TEST_LOUD);
    VR_Rules_PathCornerCase("AD", ad_path_corner_touch);
    VR_Rules_PathCornerCase("Copper", cu_path_corner_touch);
    entity g = spawn();
    g.classname = "vr_rules_test_monster";
    g.th_stand = g.th_walk = g.th_run = SUB_Null;
    VR_Test_Check(!VR_Rules_MonsterThCheck(g), "the guard names a NULL th_die (its QC error line above: expected)");
    g.th_die = SUB_Null;
    VR_Test_Check(VR_Rules_MonsterThCheck(g), "the guard is quiet with all four set");
    remove(g);
    VR_Test_Verdict();
};

// ---------------------------------------------------------------------------------------------------------------------
// vr_ad_test 209: the ruleset's numbers (vr_rules_numbers.qc) through our own touches, in the map's ruleset (Copper's in
// a Dwell map, Quake's in e1m1): armour taken or refused, its protection, a second silver key, a monster's and the
// laser's numbers; then two megahealths and their rot 7.5 s later (one rot a player under Copper, one each under
// Quake), the verdict `rulesnum: N passed, M failed`.

nosave entity vr_rn_test_p; // (nosave: the spawn census compares saves)
nosave entity vr_rn_test_m1, vr_rn_test_m2;

// `item`'s touch `fn` with `other` the player, as the engine calls it.
void(entity item, entity p, void() fn) VR_RN_TestTouch =
{
    entity os = self, oo = other;
    self = item;
    other = p;
    fn();
    self = os;
    other = oo;
};

// An item of ours for a touch (a sound every map has; no model).
entity(string cls) VR_RN_TestItem =
{
    entity e = spawn();
    e.classname = cls;
    e.noise = "items/armor1.wav";
    e.netname = "silver key";
    return e;
};

float(entity p, float type, float have, float xType, float xValue) VR_RN_TestArmor =
{
    p.armortype = type;
    p.armorvalue = have;
    return VR_RN_ArmorTake(p, xType, xValue);
};

void() VR_RN_TestRot =
{
    entity p = vr_rn_test_p;
    float cu = CU_Rules();
    // 250 at the second megahealth; ticks at 5, 6 and 7 s: one rot (Copper's) 3 points, two rots 6
    float want = cu ? 247 : 244;
    VR_Test_Check(p.health == want, sprintf("megahealth rot after 7.5 s: health %g (want %g: %s)", p.health, want,
        cu ? "one rot a player" : "one rot a megahealth"));
    p.health = 100;
    p.vr_rn_rot = 0;
    remove(vr_rn_test_m1);
    remove(vr_rn_test_m2);
    VR_ItemUtil_EntDelItem(p, IID_SUPERHEALTH);
    VR_Test_Verdict();
    remove(self);
};

void(entity p) VR_RN_Test =
{
    VR_Test_BeginFlags("rulesnum", VR_TEST_LOUD);
    float cu = CU_Rules();
    VR_Test_Print(sprintf("rulesnum: ruleset %g (%s)\n", VR_Ruleset(), cu ? "Copper's numbers" : "Quake's numbers"));

    // armour: Copper's breakpoints and kept excess; Quake's type x value
    float t;
    t = VR_RN_TestArmor(p, VR_RN_ArmorType(1), 59, VR_RN_ArmorType(0), 100);
    VR_Test_Check(t == (cu ? 100 : 0), sprintf("green over yellow 59: %g (Copper: taken under its 60; Quake: 0.6 x 59 > 30)", t));
    t = VR_RN_TestArmor(p, VR_RN_ArmorType(1), 61, VR_RN_ArmorType(0), 100);
    VR_Test_Check(t == 0, sprintf("green over yellow 61: %g (refused by both)", t));
    t = VR_RN_TestArmor(p, VR_RN_ArmorType(0), 180, VR_RN_ArmorType(1), 150);
    VR_Test_Check(t == (cu ? 165 : 150), sprintf("yellow over green 180: %g (Copper keeps half the excess: 165)", t));
    t = VR_RN_TestArmor(p, VR_RN_ArmorType(0), 300, VR_RN_ArmorType(2), 200);
    VR_Test_Check(t == (cu ? 225 : 200), sprintf("red over green 300: %g (Copper keeps a quarter: 225)", t));
    p.armortype = p.armorvalue = 0;
    entity a = VR_RN_TestItem(ITEM_NAME_ARMORINV);
    VR_RN_TestTouch(a, p, armor_touch);
    VR_Test_Check(p.armorvalue == 200 && p.armortype == (cu ? 0.7 : 0.8),
        sprintf("red armour touched: %g at %g (Copper's 0.7, Quake's 0.8)", p.armorvalue, p.armortype));
    remove(a);
    p.armortype = p.armorvalue = 0;
    VR_ItemUtil_EntDelItem(p, IID_ARMOR3);

    // keys: a second silver key (Copper: a stack of 2; Quake: refused)
    float hadKey = VR_ItemUtil_EntHasItem(p, IID_KEY1);
    VR_ItemUtil_EntDelItem(p, IID_KEY1);
    entity k1 = VR_RN_TestItem("item_key1"), k2 = VR_RN_TestItem("item_key1");
    k1.vr_itemId = k2.vr_itemId = IID_KEY1;
    VR_RN_TestTouch(k1, p, key_touch);
    VR_RN_TestTouch(k2, p, key_touch);
    float keys = VR_RN_KeyCount(p, IID_KEY1, VR_ItemUtil_EntHasItem(p, IID_KEY1));
    VR_Test_Check(keys == (cu ? 2 : 1), sprintf("two silver keys touched: %g carried", keys));
    remove(k1);
    remove(k2);
    p.worldtype = 0;
    if(!hadKey) VR_ItemUtil_EntDelItem(p, IID_KEY1);

    // monsters: a grunt with Quake's 30 (Copper's 35), a knight's 75 (72), the laser's 15 (12)
    entity m = spawn();
    m.classname = "monster_army";
    m.health = 30;
    VR_RN_MonsterNumbers(m);
    VR_Test_Check(m.health == (cu ? 35 : 30), sprintf("a grunt's health: %g", m.health));
    m.classname = "monster_knight";
    m.health = 75;
    VR_RN_MonsterNumbers(m);
    VR_Test_Check(m.health == (cu ? 72 : 75), sprintf("a knight's health: %g", m.health));
    m.health = 90;
    VR_RN_MonsterNumbers(m);
    VR_Test_Check(m.health == 90, "a knight with health of its own keeps it");
    remove(m);
    VR_Test_Check(VR_RN_LaserDamage() == (cu ? 12 : 15), sprintf("the laser's damage: %g", VR_RN_LaserDamage()));

    // two megahealths at 100: 200, then the cap 250; their rot checked 7.5 s later
    p.health = 100;
    vr_rn_test_p = p;
    vr_rn_test_m1 = VR_RN_TestItem("item_health");
    vr_rn_test_m2 = VR_RN_TestItem("item_health");
    vr_rn_test_m1.healtype = vr_rn_test_m2.healtype = 2;
    vr_rn_test_m1.healamount = vr_rn_test_m2.healamount = 100;
    VR_RN_TestTouch(vr_rn_test_m1, p, health_touch);
    VR_RN_TestTouch(vr_rn_test_m2, p, health_touch);
    VR_Test_Check(p.health == 250, sprintf("two megahealths from 100: %g (cap 250)", p.health));
    entity c = spawn();
    c.classname = "vr_rn_test";
    c.think = VR_RN_TestRot;
    c.nextthink = time + 7.5;
};
#endif
