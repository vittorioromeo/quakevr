// vr_menu_ad_monsters.inc -- Debug > Tests > Arcane Dimensions Monsters (AD support; docs/vr-port/ARCANE_DIMENSIONS.md
// section 17; QC vr_ad_monsters.qc, vr_ad_monsters_test.qc): AD's monsters ahead of you (the debug spawner's 200..) in a
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
        // ad_monsters_gen.py: begin (Misc/quakevr/ad_monsters_gen.py writes these)
        command("A Death Knight Ahead", "vr_test_spawn 200; vr_test_spawn_dist 128; impulse 241")
            .help("AD's death knight: its ragdoll, beheading, limbs, knockdowns (its death played backwards to get "
                  "up), parries of its sword."),
        command("A Zombie Knight Ahead", "vr_test_spawn 201; vr_test_spawn_dist 128; impulse 241")
            .help("AD's zombie knight: only gibbing, beheading or a cut limb kills it for good; knocked down, it "
                  "gets up as AD's own."),
        command("A Crossbow Knight Ahead", "vr_test_spawn 202; vr_test_spawn_dist 128; impulse 241"),
        command("A Hunter Ogre Ahead", "vr_test_spawn 203; vr_test_spawn_dist 128; impulse 241"),
        command("A Spider Ahead", "vr_test_spawn 204; vr_test_spawn_dist 128; impulse 241"),
        command("A Death Guard Ahead", "vr_test_spawn 205; vr_test_spawn_dist 128; impulse 241"),
        command("A Skull Wizard Ahead", "vr_test_spawn 206; vr_test_spawn_dist 128; impulse 241"),
        command("A Lost Soul Ahead", "vr_test_spawn 207; vr_test_spawn_dist 128; impulse 241"),
        command("A Mace Hunter Ogre Ahead", "vr_test_spawn 208; vr_test_spawn_dist 128; impulse 241"),
        command("A Boil Ahead", "vr_test_spawn 209; vr_test_spawn_dist 128; impulse 241"),
        command("A Gaunt Ahead", "vr_test_spawn 210; vr_test_spawn_dist 128; impulse 241"),
        command("A Rocket Soldier Ahead", "vr_test_spawn 211; vr_test_spawn_dist 128; impulse 241"),
        command("A Defender Ahead", "vr_test_spawn 212; vr_test_spawn_dist 128; impulse 241"),
        command("A Hammer Hunter Ogre Ahead", "vr_test_spawn 213; vr_test_spawn_dist 128; impulse 241"),
        command("A Gargoyle Ahead", "vr_test_spawn 214; vr_test_spawn_dist 128; impulse 241"),
        command("A Minotaur Ahead", "vr_test_spawn 215; vr_test_spawn_dist 128; impulse 241"),
        command("A Death Fury Ahead", "vr_test_spawn 216; vr_test_spawn_dist 128; impulse 241"),
        command("An Eliminator Ahead", "vr_test_spawn 217; vr_test_spawn_dist 128; impulse 241"),
        command("A Jim Ahead", "vr_test_spawn 218; vr_test_spawn_dist 128; impulse 241"),
        command("A Swampling Ahead", "vr_test_spawn 219; vr_test_spawn_dist 128; impulse 241"),
        command("A Centurion Ahead", "vr_test_spawn 220; vr_test_spawn_dist 128; impulse 241"),
        command("A Voreling Ahead", "vr_test_spawn 221; vr_test_spawn_dist 128; impulse 241"),
        command("A Stone Golem Ahead", "vr_test_spawn 222; vr_test_spawn_dist 128; impulse 241"),
        command("A Plasma Soldier Ahead", "vr_test_spawn 223; vr_test_spawn_dist 128; impulse 241"),
        command("A Wraith Ahead", "vr_test_spawn 224; vr_test_spawn_dist 128; impulse 241"),
        command("A Death Sergeant Ahead", "vr_test_spawn 225; vr_test_spawn_dist 128; impulse 241"),
        command("A Drole Ahead", "vr_test_spawn 226; vr_test_spawn_dist 128; impulse 241"),
        command("A Grenade Soldier Ahead", "vr_test_spawn 227; vr_test_spawn_dist 128; impulse 241"),
        command("A Bog Lord Ahead", "vr_test_spawn 228; vr_test_spawn_dist 128; impulse 241"),
        command("A Scorpion Ahead", "vr_test_spawn 229; vr_test_spawn_dist 128; impulse 241"),
        command("A Seeker Ahead", "vr_test_spawn 230; vr_test_spawn_dist 128; impulse 241"),
        command("A Justice Knight Ahead", "vr_test_spawn 231; vr_test_spawn_dist 128; impulse 241"),
        command("A Pyro Ahead", "vr_test_spawn 232; vr_test_spawn_dist 128; impulse 241"),
        command("AD's Eel Ahead", "vr_test_spawn 233; vr_test_spawn_dist 128; impulse 241"),
        command("A Freddie Ahead", "vr_test_spawn 234; vr_test_spawn_dist 128; impulse 241"),
        command("A Fumigator Ahead", "vr_test_spawn 235; vr_test_spawn_dist 128; impulse 241"),
        command("A Fishing Ogre Ahead", "vr_test_spawn 236; vr_test_spawn_dist 128; impulse 241"),
        command("Nour (a Boss) Ahead", "vr_test_spawn 237; vr_test_spawn_dist 128; impulse 241"),
        command("AD's Mummy Ahead", "vr_test_spawn 238; vr_test_spawn_dist 128; impulse 241"),
        command("An Ice Golem (a Boss) Ahead", "vr_test_spawn 239; vr_test_spawn_dist 128; impulse 241"),
        command("A Steel Claw Ahead", "vr_test_spawn 241; vr_test_spawn_dist 128; impulse 241"),
        command("The Judicator (a Boss) Ahead", "vr_test_spawn 242; vr_test_spawn_dist 128; impulse 241"),
        command("AD's Scrag Ahead", "vr_test_spawn 245; vr_test_spawn_dist 128; impulse 241"),
        command("AD's Hell Knight Ahead", "vr_test_spawn 250; vr_test_spawn_dist 128; impulse 241"),
        // ad_monsters_gen.py: end
    };
}
