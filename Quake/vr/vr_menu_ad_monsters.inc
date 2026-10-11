// vr_menu_ad_monsters.inc -- Debug > Tests > Arcane Dimensions Monsters (AD support; docs/vr-port/ARCANE_DIMENSIONS.md
// section 16; QC vr_ad_monsters.qc, vr_ad_monsters_test.qc): AD's monsters ahead of you (the debug spawner's 200..) in a
// map played by AD's rules, and "admtest:" reports (developer 1). Included by vr_menu.cpp (inside its namespace).

[[nodiscard]] za::Vector<Item> pageAdMonsterTests()
{
    return {
        header("Arcane Dimensions Monsters"),
        command("Monsters Report", "vr_ad_mtest 1")
            .help("In an Arcane Dimensions map: its AD monsters by class, how many have Quake VR's treatment (registered) "
                  "and a ragdoll rig (one admtest: line each, developer 1)."),
        command("Nearest Monster Report", "vr_ad_mtest 2")
            .help("The nearest AD monster: its head zone, head gib, corpse, knockdown, rig, and its state (ragdoll, "
                  "knocked down, headless, limbs cut, corpse)."),
        command("Kill the Nearest Whole", "vr_ad_mtest 3")
            .help("Destructive: the nearest AD monster killed by a blow of its health: it dies whole, a ragdoll, then a "
                  "corpse to gib."),
        command("Gib the Nearest", "vr_ad_mtest 4")
            .help("Destructive: the nearest AD monster or corpse hit for 1000: a corpse is gibbed (its own head and gib)."),
        header("Ahead of You (an Arcane Dimensions map)"),
        command("A Death Knight Ahead", "vr_test_spawn 200; vr_test_spawn_dist 128; impulse 241")
            .help("AD's death knight: its ragdoll, beheading, limbs, knockdowns (its death played backwards to get up), "
                  "parries of its sword."),
        command("A Rocket Soldier Ahead", "vr_test_spawn 211; vr_test_spawn_dist 128; impulse 241"),
        command("A Plasma Soldier Ahead", "vr_test_spawn 223; vr_test_spawn_dist 128; impulse 241"),
        command("A Grenade Soldier Ahead", "vr_test_spawn 227; vr_test_spawn_dist 128; impulse 241"),
    };
}
