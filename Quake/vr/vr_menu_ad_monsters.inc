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
#define QVR_AD_GUN(label, model, wid, choice, flags) \
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
        command("Crossbow's Bolt and String Info", "vr_adgun_crossbow_info")
            .help("For each hand holding the crossbow: its last shot, bolts left, and the bolt in its groove from the "
                  "muzzle along the shot (where the shot leaves from: QC's VR_ADXBOW_GROOVE_*)."),
        slider("Crossbow Held After a Shot", "vr_adgun_crossbow_anim_at", -1.f, 2.5f, 0.01f, "%.2f s")
            .help("Holds the crossbows in your hands this long after a shot, loaded: the string's snap, the limbs' "
                  "wobble, the draw and the next bolt's morph, frame by frame. -1: as they are."),
        command("Freddie's Gun Arm Cut Off", "vr_ad_test 95")
            .help("Destructive: the Freddie nearest you killed and his gun arm cut off: a super nailgun of ours drops "
                  "there, loaded, and goes into your main hand; his body then gibbed drops no second one (adfred: lines)."),
        command("Freddie Gibbed Whole", "vr_ad_test 102")
            .help("Destructive: the Freddie nearest you gibbed, his arm not cut: one super nailgun of ours drops, "
                  "loaded, and goes into your main hand (adfred: lines)."),
        command("Gun Models", "vr_adgun_models").help("The guns made from AD's monster models: triangles, box, muzzle."),
        command("Gun Variants", "vr_adgun_variants")
            .help("The soldiers' guns as our burst rifle's variants (the models they drop now): each one's triangles, "
                  "box (whether the rifle's corner held: the burst rifle's weapon settings hold them) and time to make; "
                  "their add-ons' mesh: open edges (see-through), doubled edges, triangles wound inward, inner walls "
                  "(vr_adgun_variants 1 lists each open or doubled edge)."),
        command("Hold the Flamethrower", "vr_weapon_grip_mode 1; vr_ad_test 79")
            .help("The pyro's flamethrower in your main hand: its fuel tank on your back, the hose to the gun (VR Settings > "
                  "Combat > Enemy Weapons > Arcane Dimensions' Guns > Fuel Tank and Hose)."),
        command("Hold the Poison Sprayer", "vr_weapon_grip_mode 1; vr_ad_test 80")
            .help("The fumigator's poison sprayer in your main hand, its green tank and hose."),
        command("Flamethrowers on the Nearest Monster", "vr_ad_test 91")
            .help("Destructive: the flamethrower, then the poison sprayer, fired from your main hand at the monster nearest "
                  "you (its health raised): it must burn, then be poisoned, with no blood, gore or wounds; then a shot's "
                  "damage on it, which bleeds (adgun fx: lines; 3 PASS)."),
        command("Fuel Hose Info", "vr_adgun_hose_info")
            .help("vr_adgun_hose_info: in the console, each hand's hose (its gun, its ends, its length, how low it hangs), "
                  "its rings drawn (and in the body's mirror image), its cost a frame, and how deep it has lain in your "
                  "arms since the last time (the arms' capsules)."),
        toggle("Fuel Hose Avoids the Arms", "vr_adgun_hose_arms")
            .help("The hose kept out of your drawn arms (the upper arm and forearm as capsules), going round them. Off: "
                  "it passes through them (to compare; Fuel Hose Info measures how deep)."),
        command("Poison Sprayer Lying With Its Tank", "vr_ad_test 158")
            .help("A poison sprayer made lying 2 m ahead (as the toolgun's spawner makes one), its tank beside it, roped "
                  "and hosed to it (Arcane Dimensions' Guns > Tank Lying About)."),
        command("Kick the Nearest Fuelled Gun", "vr_ad_test 159")
            .help("The flamethrower or sprayer lying nearest you kicked hard sideways: its rope drags its tank after it "
                  "(Fuel Hose Info: the rope's length and how far apart they are)."),
        command("Fuel Tanks Count", "vr_ad_test 150")
            .help("In the console: the tanks lying about, the fuelled guns lying about, the pairs, tanks without their gun "
                  "and guns without a tank (both 0 when right), each pair's distance; Fuel Hose Info has the ropes and the "
                  "world hoses."),
        command("Melee Weapons Dropped: Death Fury", "vr_ad_test 110")
            .help("Destructive: the death fury nearest you killed; its two swords lie, go into both hands and strike a "
                  "target, one hand and two (admelee: lines). 111-115 (console): the death knight, zombie knight, death "
                  "guard, the hammer's and the mace's hunter ogres; 120-122: AD's hell knight, the death lord, "
                  "Quoth's death guard (Quake VR's hell knight's sword)."),
        command("Melee Weapons: Fury's Swords in Hand", "vr_ad_test 116")
            .help("Both hands get the death fury's swords. 117: the death knight's and zombie knight's; 118: the death "
                  "guard's and the hammer; 119: the mace and the knight's own sword (console)."),
        command("Melee Weapon Models", "vr_admelee_models")
            .help("The melee weapons made from AD's weapon models: triangles, box, tip. 'vr_admelee_models write' "
                  "(console): each made file into the game folder (admelee_<n>.mdl)."),
        command("Fury's Sword: Curve Check", "vr_ad_test 126")
            .help("The death fury's sword in your main hand (Fury's Swords in Hand first): its melee points on its curved "
                  "blade against the straight line, and a sweep past the bow (admelee curve: PASS). Curved Blades off: "
                  "the line (FAIL, to compare)."),
        command("Fury's Sword: Drawn Spine", "vr_admelee_spine")
            .help("The sword's blade as drawn in each hand, at the points the melee checks (admelee spine: lines), to "
                  "compare with Curve Check's."),
        toggle("Curved Blades", "vr_melee_curved_blades")
            .help("A curved sword (the death fury's) strikes along its curve; off: along the straight line from the hand "
                  "to its tip, as before."),
        command("Holster Refusal Check", "vr_ad_test 127")
            .help("Destructive (your left hip holster): the hammer and the mace let go at it are refused and fall; the "
                  "fury's sword goes in (admelee holster: PASS)."),
        command("Projectile Hit Check", "vr_ad_test 130")
            .help("Destructive: a grunt spawned ahead, struck by projectile kind scratch1 (1 a nail, 6 AD's crossbow bolt, "
                  "2 a grenade...: vr_projhit_test.qc) in case scratch2 (0 a head kill, 1 a corpse's arm, 2 a corpse's "
                  "torso, 3 knocked down): projhit: lines (headless, limbs, moved, health)."),
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
        adSpawnRow(261, command("AD's Knight Ahead", "vr_test_spawn 261; vr_test_spawn_dist 128; impulse 241")),
        // ad_monsters_gen.py: end
    });
}
