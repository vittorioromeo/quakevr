// vr_menu_ad_guns.inc -- Combat > Enemy Weapons > Arcane Dimensions' Guns (AD support; QC vr_ad_enemyguns.qc,
// vr_ad_guns_defs.qc): the vr_adgun_* settings. Included by vr_menu.cpp (inside its namespace).

// AD support: Arcane Dimensions' armed soldiers' own guns, dropped as they die (QC vr_ad_enemyguns.qc): every number of
// your use of them, to balance them (the monsters' own attacks stay Arcane Dimensions'). Their placement in the hand is
// the Weapon Offsets' (slots 28-32: inheriting the grunts' and the enforcers' guns').
[[nodiscard]] za::Vector<Item> pageAdGuns()
{
    return {
        toggle("Soldiers Drop Their Guns", "vr_adgun_drops")
            .help("Arcane Dimensions' armed soldiers drop their own gun as they die: single use, its rounds (or fuel) its "
                  "own, never refilled; empty, it is still a club. The plain soldier and enforcer drop the grunt's burst "
                  "rifle and the enforcer's laser rifle. Off: they drop nothing."),
        header("Rocket Soldier's Launcher"),
        slider("Direct Hit", "vr_adgun_rocket_damage", 0.f, 150.f, 1.f, "%.0f").extend(0.f, 500.f)
            .help("A rocket's damage to what it strikes (Arcane Dimensions' soldier's: 25)."),
        slider("Blast", "vr_adgun_rocket_splash", 0.f, 150.f, 1.f, "%.0f").extend(0.f, 500.f)
            .help("Its blast round where it strikes, falling off with distance (not on what it hit). 0: none."),
        slider("Rocket Speed", "vr_adgun_rocket_speed", 200.f, 2000.f, 25.f, "%.0f u/s").extend(100.f, 5000.f),
        slider("Rockets", "vr_adgun_rocket_ammo", 1.f, 20.f, 1.f, "%.0f").extend(1.f, 100.f),
        slider("Time Between Shots", "vr_adgun_rocket_refire", 0.1f, 3.f, 0.05f, "%.2f s").extend(0.05f, 10.f),
        header("Grenade Soldier's Launcher"),
        slider("Blast", "vr_adgun_grenade_damage", 0.f, 150.f, 1.f, "%.0f").extend(0.f, 500.f)
            .help("A grenade's blast (Arcane Dimensions' monster grenades: 40; your launcher's: 120)."),
        slider("Grenade Speed", "vr_adgun_grenade_speed", 200.f, 1500.f, 25.f, "%.0f u/s").extend(100.f, 3000.f),
        slider("Fuse", "vr_adgun_grenade_fuse", 0.5f, 5.f, 0.1f, "%.1f s").extend(0.2f, 10.f)
            .help("How long a grenade bounces about before it goes off. A monster it strikes sets it off at once."),
        slider("Grenades", "vr_adgun_grenade_ammo", 1.f, 20.f, 1.f, "%.0f").extend(1.f, 100.f),
        slider("Time Between Shots", "vr_adgun_grenade_refire", 0.1f, 3.f, 0.05f, "%.2f s").extend(0.05f, 10.f),
        header("Plasma Soldier's Rifle"),
        slider("Bolt Hit", "vr_adgun_plasma_damage", 0.f, 100.f, 1.f, "%.0f").extend(0.f, 500.f),
        slider("Bolt Blast", "vr_adgun_plasma_splash", 0.f, 100.f, 1.f, "%.0f").extend(0.f, 500.f).help("0: none."),
        slider("Bolt Speed", "vr_adgun_plasma_speed", 200.f, 2000.f, 25.f, "%.0f u/s").extend(100.f, 5000.f),
        slider("Bolts", "vr_adgun_plasma_ammo", 1.f, 80.f, 1.f, "%.0f").extend(1.f, 300.f),
        slider("Time Between Bolts", "vr_adgun_plasma_refire", 0.05f, 2.f, 0.05f, "%.2f s").extend(0.05f, 10.f),
        header("Defender's Shotgun"),
        slider("Pellet Damage", "vr_adgun_defender_damage", 0.f, 20.f, 0.5f, "%.1f").extend(0.f, 100.f),
        slider("Pellets", "vr_adgun_defender_pellets", 1.f, 30.f, 1.f, "%.0f").extend(1.f, 60.f),
        slider("Spread", "vr_adgun_defender_spread", 0.f, 0.4f, 0.01f, "%.2f").extend(0.f, 1.f)
            .help("How wide its pellets spread across (half as much up and down; Arcane Dimensions' defender's: 0.18)."),
        slider("Shots", "vr_adgun_defender_ammo", 1.f, 30.f, 1.f, "%.0f").extend(1.f, 100.f),
        slider("Time Between Shots", "vr_adgun_defender_refire", 0.1f, 3.f, 0.05f, "%.2f s").extend(0.05f, 10.f),
        header("Eliminator's Plasma Gun"),
        slider("Bolt Hit", "vr_adgun_eliminator_damage", 0.f, 100.f, 1.f, "%.0f").extend(0.f, 500.f),
        slider("Bolt Blast", "vr_adgun_eliminator_splash", 0.f, 100.f, 1.f, "%.0f").extend(0.f, 500.f).help("0: none."),
        slider("Bolt Speed", "vr_adgun_eliminator_speed", 200.f, 2000.f, 25.f, "%.0f u/s").extend(100.f, 5000.f),
        slider("Bolts", "vr_adgun_eliminator_ammo", 1.f, 80.f, 1.f, "%.0f").extend(1.f, 300.f),
        slider("Time Between Bolts", "vr_adgun_eliminator_refire", 0.05f, 2.f, 0.05f, "%.2f s").extend(0.05f, 10.f),
        header("Centurion's Plasma Gun"),
        slider("Bolt Hit", "vr_adgun_centurion_damage", 0.f, 100.f, 1.f, "%.0f").extend(0.f, 500.f),
        slider("Bolt Blast", "vr_adgun_centurion_splash", 0.f, 100.f, 1.f, "%.0f").extend(0.f, 500.f).help("0: none."),
        slider("Bolt Speed", "vr_adgun_centurion_speed", 200.f, 2000.f, 25.f, "%.0f u/s").extend(100.f, 5000.f),
        slider("Bolts", "vr_adgun_centurion_ammo", 1.f, 80.f, 1.f, "%.0f").extend(1.f, 300.f),
        slider("Time Between Bolts", "vr_adgun_centurion_refire", 0.05f, 2.f, 0.05f, "%.2f s").extend(0.05f, 10.f),
        header("Pyro's Flamethrower"),
        slider("Flame Damage", "vr_adgun_pyro_damage", 0.f, 30.f, 0.5f, "%.1f").extend(0.f, 200.f)
            .help("Each flame's damage to what it touches (each flame once; about ten a second)."),
        slider("Burns For", "vr_adgun_pyro_burn", 0.f, 10.f, 0.5f, "%.1f s").extend(0.f, 60.f)
            .help("What a flame touches catches fire this long (Combat > Burning's Burn Damage a second). Not under water. "
                  "0: nothing catches fire."),
        slider("Flame Speed", "vr_adgun_pyro_speed", 200.f, 1500.f, 25.f, "%.0f u/s").extend(100.f, 3000.f),
        slider("Reach", "vr_adgun_pyro_range", 64.f, 800.f, 8.f, "%.0f units").extend(32.f, 2000.f),
        slider("Fuel", "vr_adgun_pyro_fuel", 10.f, 400.f, 10.f, "%.0f flames").extend(1.f, 2000.f),
        slider("Time Between Flames", "vr_adgun_pyro_rate", 0.05f, 0.5f, 0.01f, "%.2f s").extend(0.05f, 2.f),
        header("Fumigator's Poison Sprayer"),
        slider("Puff Damage", "vr_adgun_fumigator_damage", 0.f, 30.f, 0.5f, "%.1f").extend(0.f, 200.f),
        slider("Poison Damage", "vr_adgun_fumigator_poison", 0.f, 30.f, 0.5f, "%.1f / s").extend(0.f, 200.f)
            .help("The poison a puff leaves on what it touches: its damage a second. 0: none."),
        slider("Poisoned For", "vr_adgun_fumigator_poisontime", 0.f, 15.f, 0.5f, "%.1f s").extend(0.f, 60.f),
        slider("Puff Speed", "vr_adgun_fumigator_speed", 200.f, 1500.f, 25.f, "%.0f u/s").extend(100.f, 3000.f),
        slider("Reach", "vr_adgun_fumigator_range", 64.f, 800.f, 8.f, "%.0f units").extend(32.f, 2000.f),
        slider("Fuel", "vr_adgun_fumigator_fuel", 10.f, 400.f, 10.f, "%.0f puffs").extend(1.f, 2000.f),
        slider("Time Between Puffs", "vr_adgun_fumigator_rate", 0.05f, 0.5f, 0.01f, "%.2f s").extend(0.05f, 2.f),
        header("Tests"),
        command("Fire Each Gun at a Target", "vr_ad_test 70")
            .help("Each gun into your main hand, fired at a target ahead: what it dealt (needs Arcane Dimensions' data)."),
        command("Drop Each Gun", "vr_ad_test 71"),
        command("Gun Models", "vr_adgun_models"),
    };
}
