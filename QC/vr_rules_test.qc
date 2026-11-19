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
#endif
