// vr_menu_ad_monsters.inc -- Debug > Tests > Arcane Dimensions Monsters (AD support; docs/vr-port/ARCANE_DIMENSIONS.md
// section 17; QC vr_ad_monsters.qc, vr_ad_monsters_test.qc): AD's monsters ahead of you (the debug spawner's 200..) in a
// map played by AD's rules, and "admtest:" reports (developer 1). Included by vr_menu.cpp (inside its namespace).

// AD support: every spawn menu shares the toolgun's availability checks.
[[nodiscard]] int adSpawnIndex(int choice)
{
    for(int i = 0; i < toolgun::spawnableCount(); ++i)
    {
        if(toolgun::spawnableAD(i) && toolgun::spawnableDebugChoice(i) == choice) { return i; }
    }
    return -1;
}

[[nodiscard]] Item adSpawnRow(int choice, Item item)
{
    const int i = adSpawnIndex(choice);
    if(i < 0) { return item; }
    return toolgun::spawnableAvailable(i) ? item : item.unavailableBecause(
        "Needs Arcane Dimensions data installed and enabled, and outside its maps Additional Content Everywhere. Weapon upgrades also follow Advanced VR Options > Arcane Dimensions > Weapon Upgrades.");
}

[[nodiscard]] Item adSpawnChoices(Item item)
{
    for(int i = 0; i < toolgun::spawnableCount(); ++i)
    {
        if(toolgun::spawnableAD(i))
        {
            item.choices.pushBack({static_cast<float>(toolgun::spawnableDebugChoice(i)), toolgun::spawnableName(i)});
        }
    }
    return item;
}

[[nodiscard]] za::Vector<Item> adSpawnItems(za::Vector<Item> items, bool weaponsOnly = false)
{
    items.pushBack(header("Arcane Dimensions Pickups"));
#define QVR_AD_SPAWN(section, label, classname, model, lift, rulesOnly, choice) \
    if(!weaponsOnly || !strcmp(section, "Weapons")) \
    { items.pushBack(adSpawnRow(choice, command(label, "vr_test_spawn " #choice "; vr_test_spawn_dist 64; impulse 241"))); }
#define QVR_AD_SPAWN_MAP(section, label, classname, model, lift, choice, key, value, skin) \
    QVR_AD_SPAWN(section, label, classname, model, lift, false, choice)
#include "vr_ad_spawnitems.inc"
#undef QVR_AD_SPAWN_MAP
#undef QVR_AD_SPAWN
#define QVR_AD_GUN(label, model, wid, choice) \
    items.pushBack(adSpawnRow(choice, command(label, "vr_test_spawn " #choice "; vr_test_spawn_dist 64; impulse 241")));
#include "vr_ad_spawnguns.inc"
#undef QVR_AD_GUN
    return items;
}

[[nodiscard]] za::Vector<Item> pageAdMonsterTests()
{
    return adSpawnItems({
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
        command("Snap the Nearest's Neck", "vr_ad_mtest 5")
            .help("Destructive: the nearest awake AD monster's neck snapped as by your hands: it dies by AD's own death "
                  "(a zombie or a mummy for good)."),
        header("Soldiers' Guns (AD installed)"),
        command("Fire Each Gun at a Target", "vr_ad_test 70")
            .help("Each gun into your main hand, fired at a target ahead: what it dealt (adgun test: lines)."),
        command("Drop Each Gun", "vr_ad_test 71").help("Each gun dropped from a soldier's place: its model and clip."),
        command("Their Shots Leaving the Muzzle", "vr_ad_test 90")
            .help("The grenade soldier's launcher fired beside yours (the same grenade?), the rocket soldier's and a "
                  "plasma rifle: each shot 0.3 s on, still flying and clear of the muzzle (adgun shots: lines). Aim "
                  "level."),
        command("Make the Nearest Shooter Fire", "vr_ad_test 92")
            .help("The AD pyro, fumigator or plasma soldier nearest you fires once at the monster or body nearest it (its "
                  "flames, poison, electric bolt: their effects as the guns' you carry). Press again for a stream."),
        command("Crossbow Knight's Crossbow, End to End", "vr_ad_test 93")
            .help("Destructive: the crossbow knight nearest you killed, its crossbow taken, fired, holstered, drawn into "
                  "the other hand and fired (adxb: lines)."),
        command("Freddie's Gun Arm Cut Off", "vr_ad_test 95")
            .help("Destructive: the Freddie nearest you killed and his gun arm cut off: a super nailgun of ours drops "
                  "there, loaded, and goes into your main hand (adfred: lines)."),
        command("Gun Models", "vr_adgun_models").help("The guns made from AD's monster models: triangles, box, muzzle."),
        command("Melee Weapons Dropped: Death Fury", "vr_ad_test 110")
            .help("Destructive: the death fury nearest you killed; its two swords lie, go into both hands and strike a "
                  "target, one hand and two (admelee: lines). 111-115 (console): the death knight, zombie knight, death "
                  "guard, the hammer's and the mace's hunter ogres."),
        command("Melee Weapons: Fury's Swords in Hand", "vr_ad_test 116")
            .help("Both hands get the death fury's swords. 117: the death knight's and zombie knight's; 118: the death "
                  "guard's and the hammer; 119: the mace alone (console)."),
        command("Melee Weapon Models", "vr_admelee_models")
            .help("The melee weapons made from AD's weapon models: triangles, box, tip."),
        command("Stand-In Gun Model", "vr_ad_test 89")
            .help("A gun model the engine cannot make (another AD release): Quake VR's stand-in loads under its name."),
        command("Arm for a Level Change", "vr_ad_test 87")
            .help("The rocket soldier's launcher in your main hand, the plasma soldier's rifle in the left shoulder "
                  "holster (hold the grip): then change level and Check After a Level Change."),
        command("Check After a Level Change", "vr_ad_test 88").help("Both still carried, their models loaded: PASS."),
        header("Ahead of You (AD installed)"),
        // ad_monsters_gen.py: begin (Misc/quakevr/ad_monsters_gen.py writes these)
        adSpawnRow(200, command("A Death Knight Ahead", "vr_test_spawn 200; vr_test_spawn_dist 128; impulse 241"))
            .help("AD's death knight: its ragdoll, beheading, limbs, knockdowns (its death played backwards to get "
                  "up), parries of its sword."),
        adSpawnRow(201, command("A Zombie Knight Ahead", "vr_test_spawn 201; vr_test_spawn_dist 128; impulse 241"))
            .help("AD's zombie knight: only gibbing, beheading or a cut limb kills it for good; knocked down, it "
                  "gets up as AD's own."),
        adSpawnRow(202, command("A Crossbow Knight Ahead", "vr_test_spawn 202; vr_test_spawn_dist 128; impulse 241")),
        adSpawnRow(203, command("A Hunter Ogre Ahead", "vr_test_spawn 203; vr_test_spawn_dist 128; impulse 241")),
        adSpawnRow(204, command("A Spider Ahead", "vr_test_spawn 204; vr_test_spawn_dist 128; impulse 241")),
        adSpawnRow(205, command("A Death Guard Ahead", "vr_test_spawn 205; vr_test_spawn_dist 128; impulse 241")),
        adSpawnRow(206, command("A Skull Wizard Ahead", "vr_test_spawn 206; vr_test_spawn_dist 128; impulse 241")),
        adSpawnRow(207, command("A Lost Soul Ahead", "vr_test_spawn 207; vr_test_spawn_dist 128; impulse 241")),
        adSpawnRow(208, command("A Mace Hunter Ogre Ahead", "vr_test_spawn 208; vr_test_spawn_dist 128; impulse 241")),
        adSpawnRow(209, command("A Boil Ahead", "vr_test_spawn 209; vr_test_spawn_dist 128; impulse 241")),
        adSpawnRow(210, command("A Gaunt Ahead", "vr_test_spawn 210; vr_test_spawn_dist 128; impulse 241")),
        adSpawnRow(211, command("A Rocket Soldier Ahead", "vr_test_spawn 211; vr_test_spawn_dist 128; impulse 241")),
        adSpawnRow(212, command("A Defender Ahead", "vr_test_spawn 212; vr_test_spawn_dist 128; impulse 241")),
        adSpawnRow(213, command("A Hammer Hunter Ogre Ahead", "vr_test_spawn 213; vr_test_spawn_dist 128; impulse 241")),
        adSpawnRow(214, command("A Gargoyle Ahead", "vr_test_spawn 214; vr_test_spawn_dist 128; impulse 241")),
        adSpawnRow(215, command("A Minotaur Ahead", "vr_test_spawn 215; vr_test_spawn_dist 128; impulse 241")),
        adSpawnRow(216, command("A Death Fury Ahead", "vr_test_spawn 216; vr_test_spawn_dist 128; impulse 241")),
        adSpawnRow(217, command("An Eliminator Ahead", "vr_test_spawn 217; vr_test_spawn_dist 128; impulse 241")),
        adSpawnRow(218, command("A Jim Ahead", "vr_test_spawn 218; vr_test_spawn_dist 128; impulse 241")),
        adSpawnRow(219, command("A Swampling Ahead", "vr_test_spawn 219; vr_test_spawn_dist 128; impulse 241")),
        adSpawnRow(220, command("A Centurion Ahead", "vr_test_spawn 220; vr_test_spawn_dist 128; impulse 241")),
        adSpawnRow(221, command("A Voreling Ahead", "vr_test_spawn 221; vr_test_spawn_dist 128; impulse 241")),
        adSpawnRow(222, command("A Stone Golem Ahead", "vr_test_spawn 222; vr_test_spawn_dist 128; impulse 241")),
        adSpawnRow(223, command("A Plasma Soldier Ahead", "vr_test_spawn 223; vr_test_spawn_dist 128; impulse 241")),
        adSpawnRow(224, command("A Wraith Ahead", "vr_test_spawn 224; vr_test_spawn_dist 128; impulse 241")),
        adSpawnRow(225, command("A Death Sergeant Ahead", "vr_test_spawn 225; vr_test_spawn_dist 128; impulse 241")),
        adSpawnRow(226, command("A Drole Ahead", "vr_test_spawn 226; vr_test_spawn_dist 128; impulse 241")),
        adSpawnRow(227, command("A Grenade Soldier Ahead", "vr_test_spawn 227; vr_test_spawn_dist 128; impulse 241")),
        adSpawnRow(228, command("A Bog Lord Ahead", "vr_test_spawn 228; vr_test_spawn_dist 128; impulse 241")),
        adSpawnRow(229, command("A Scorpion Ahead", "vr_test_spawn 229; vr_test_spawn_dist 128; impulse 241")),
        adSpawnRow(230, command("A Seeker Ahead", "vr_test_spawn 230; vr_test_spawn_dist 128; impulse 241")),
        adSpawnRow(231, command("A Justice Knight Ahead", "vr_test_spawn 231; vr_test_spawn_dist 128; impulse 241")),
        adSpawnRow(232, command("A Pyro Ahead", "vr_test_spawn 232; vr_test_spawn_dist 128; impulse 241")),
        adSpawnRow(233, command("AD's Eel Ahead", "vr_test_spawn 233; vr_test_spawn_dist 128; impulse 241")),
        adSpawnRow(234, command("A Freddie Ahead", "vr_test_spawn 234; vr_test_spawn_dist 128; impulse 241")),
        adSpawnRow(235, command("A Fumigator Ahead", "vr_test_spawn 235; vr_test_spawn_dist 128; impulse 241")),
        adSpawnRow(236, command("A Fishing Ogre Ahead", "vr_test_spawn 236; vr_test_spawn_dist 128; impulse 241")),
        adSpawnRow(237, command("Nour (a Boss) Ahead", "vr_test_spawn 237; vr_test_spawn_dist 128; impulse 241")),
        adSpawnRow(238, command("AD's Mummy Ahead", "vr_test_spawn 238; vr_test_spawn_dist 128; impulse 241")),
        adSpawnRow(239, command("An Ice Golem (a Boss) Ahead", "vr_test_spawn 239; vr_test_spawn_dist 128; impulse 241")),
        adSpawnRow(241, command("A Steel Claw Ahead", "vr_test_spawn 241; vr_test_spawn_dist 128; impulse 241")),
        adSpawnRow(242, command("The Judicator (a Boss) Ahead", "vr_test_spawn 242; vr_test_spawn_dist 128; impulse 241")),
        adSpawnRow(245, command("AD's Scrag Ahead", "vr_test_spawn 245; vr_test_spawn_dist 128; impulse 241")),
        adSpawnRow(250, command("AD's Hell Knight Ahead", "vr_test_spawn 250; vr_test_spawn_dist 128; impulse 241")),
        adSpawnRow(251, command("A Gug Ahead", "vr_test_spawn 251; vr_test_spawn_dist 128; impulse 241")),
        adSpawnRow(252, command("A Polyp Ahead", "vr_test_spawn 252; vr_test_spawn_dist 128; impulse 241")),
        adSpawnRow(253, command("A Nail Ogre Ahead", "vr_test_spawn 253; vr_test_spawn_dist 128; impulse 241")),
        adSpawnRow(254, command("A Floyd Automaton Ahead", "vr_test_spawn 254; vr_test_spawn_dist 128; impulse 241")),
        adSpawnRow(255, command("The Deflector Ahead", "vr_test_spawn 255; vr_test_spawn_dist 128; impulse 241")),
        adSpawnRow(256, command("A Dark Prince Ahead", "vr_test_spawn 256; vr_test_spawn_dist 128; impulse 241")),
        adSpawnRow(257, command("Vermis (a Boss) Ahead", "vr_test_spawn 257; vr_test_spawn_dist 128; impulse 241")),
        adSpawnRow(258, command("A Quoth Death Guard Ahead", "vr_test_spawn 258; vr_test_spawn_dist 128; impulse 241")),
        adSpawnRow(259, command("A Death Lord Ahead", "vr_test_spawn 259; vr_test_spawn_dist 128; impulse 241")),
        adSpawnRow(260, command("A Sentinel Ahead", "vr_test_spawn 260; vr_test_spawn_dist 128; impulse 241")),
        // ad_monsters_gen.py: end
    });
}
