// vr_menu_ad_guns.inc -- Combat > Enemy Weapons > Arcane Dimensions' Guns (AD support; QC vr_ad_enemyguns.qc,
// vr_ad_guns_defs.qc): the vr_adgun_* settings. Included by vr_menu.cpp (inside its namespace).

// AD support: Arcane Dimensions' armed soldiers' own guns, dropped as they die (QC vr_ad_enemyguns.qc): every number of
// your use of them, to balance them (the monsters' own attacks stay Arcane Dimensions'). Their placement in the hand is
// the Weapon Offsets' (slots 28-32: our burst rifle's variants, inheriting its slot 22; the crossbow knight's crossbow 33).
[[nodiscard]] za::Vector<Item> pageAdGuns()
{
    return {
        toggle("Soldiers Drop Their Guns", "vr_adgun_drops")
            .help("Arcane Dimensions' armed soldiers drop their own gun as they die: single use, its rounds (or fuel) its "
                  "own, never refilled; empty, it is still a club. Where Quake VR has a gun of its kind, its shots are "
                  "ours (the grenade launcher's grenades, the rocket launcher's rockets, our tracers and impacts); its "
                  "look and rate of fire stay its own. The plain soldier and enforcer drop the grunt's burst rifle and "
                  "the enforcer's laser rifle. Off: they drop nothing."),
        header("Rocket Soldier's Launcher"),
        slider("Rockets", "vr_adgun_rocket_ammo", 1.f, 20.f, 1.f, "%.0f").extend(1.f, 100.f)
            .help("The rockets it holds. They are your rocket launcher's rockets (speed, hit, blast: Weapon Damage)."),
        slider("Time Between Shots", "vr_adgun_rocket_refire", 0.1f, 3.f, 0.05f, "%.2f s").extend(0.05f, 10.f),
        header("Grenade Soldier's Launcher"),
        slider("Grenades", "vr_adgun_grenade_ammo", 1.f, 20.f, 1.f, "%.0f").extend(1.f, 100.f)
            .help("The grenades it holds. They are your grenade launcher's grenades (size, bounce, fuse, blast)."),
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
        header("Centurion's Plasma Gun"),
        slider("Bolt Hit", "vr_adgun_centurion_damage", 0.f, 100.f, 1.f, "%.0f").extend(0.f, 500.f)
            .help("The centurion's and the eliminator's plasma gun (one gun: both drop it)."),
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
        header("Crossbow Knight's Crossbow"),
        slider("Bolt Damage", "vr_adgun_crossbow_damage", 1.f, 60.f, 1.f, "%.0f").extend(0.f, 200.f)
            .help("A bolt's damage (Arcane Dimensions' crossbow knight's bolt: 15). A head more, as every shot."),
        slider("Bolt Speed", "vr_adgun_crossbow_speed", 200.f, 2000.f, 25.f, "%.0f u/s").extend(100.f, 4000.f),
        slider("Bolts", "vr_adgun_crossbow_ammo", 1.f, 30.f, 1.f, "%.0f").extend(1.f, 100.f)
            .help("The bolts the crossbow holds as the knight drops it: its own, never refilled."),
        slider("Time Between Shots", "vr_adgun_crossbow_refire", 0.1f, 3.f, 0.05f, "%.2f s").extend(0.05f, 10.f),
        toggle("Bolt Laid and String Animated", "vr_adgun_crossbow_anim")
            .help("A bolt lies in the groove while the crossbow is ready (morphing in after a shot) and is the one that "
                  "flies; the string snaps forward and the limbs flex, then it is drawn back for the next."),
        slider("Next Bolt's Sound", "vr_adgun_crossbow_bolt_sound", 0.f, 1.f, 0.05f, "%.2f")
            .help("The soft shimmer as the next bolt morphs into the groove: its volume (0: none)."),
        header("Melee Monsters' Weapons"),
        toggle("Melee Monsters Drop Their Weapons", "vr_admelee_drops")
            .help("The death fury drops its two swords, the death knight, zombie knight and death guard their swords, "
                  "the hunter ogres their hammer and mace: yours to swing (Weapon Offsets slots 41-46)."),
        slider("Sword Damage", "vr_admelee_sword_damage", 0.25f, 3.f, 0.05f, "%.2fx").extend(0.f, 10.f)
            .help("Their swords' blows, times the knight's sword's (the hell knight's: 1.25)."),
        slider("Hammer and Mace Damage", "vr_admelee_heavy_damage", 0.25f, 4.f, 0.05f, "%.2fx").extend(0.f, 10.f)
            .help("The hunter ogres' hammer's and mace's blows held in both hands, times the knight's sword's."),
        slider("One Hand's Share", "vr_admelee_heavy_1h", 0.f, 1.f, 0.05f, "%.2f")
            .help("The hammer and the mace held in one hand: this share of their damage (their weight also swings "
                  "them slowly in one hand; two hands swing them faster)."),
        slider("Hammer and Mace Stamina", "vr_admelee_heavy_stamina", 1.f, 3.f, 0.05f, "%.2fx").extend(0.f, 5.f)
            .help("Their blows cost this times another weapon's stamina."),
        slider("Hammer and Mace Move Speed", "vr_admelee_heavy_speed", 0.5f, 1.f, 0.05f, "%.2fx").extend(0.2f, 1.f)
            .help("Holding the hammer or the mace in either hand, you move at this times your full speed."),
        header("Health Vials"),
        slider("Health Vial Size", "vr_ad_healthvial_scale", 1.f, 4.f, 0.1f, "%.1fx").extend(0.5f, 6.f)
            .help("AD's health vials and gems drawn this many times bigger (as objects: times the ammo and health "
                  "boxes' scale; their box grows with them). New ones, as they appear."),
        header("Flamethrowers' Look"),
        slider("AD's Flames Opacity", "vr_adgun_stream_alpha", 0.1f, 1.f, 0.05f, "%.2f")
            .help("How opaque AD's own flame and poison sprites are (the pyros' and fumigators', theirs and yours). 1: AD's."),
        slider("AD's Flames Frequency", "vr_adgun_ad_copies", 0.f, 3.f, 1.f, "%.0f")
            .help("AD's flame and poison sprites drawn this many more times, between each and where it came from: a "
                  "fuller stream (drawn only: the flames that burn stay AD's). 0: AD's alone."),
        slider("Our Fire With Them", "vr_adgun_stream_fire", 0.f, 6.f, 1.f, "%.0f")
            .help("Our particles' density: tongues of the torches' fire (green: poison) carried with each of AD's "
                  "flames, a tenth of a second, and green smoke puffing off a poisoned body. 0: AD's alone."),
        slider("Our Fire Frequency", "vr_adgun_fx_rate", 1.f, 4.f, 1.f, "%.0fx")
            .help("How often our tongues are made along each flame: each time spread back along its way, a denser "
                  "stream without gaps (each time as many as Our Fire With Them)."),
        slider("Embers and Smoke", "vr_adgun_fx_embers", 0.f, 3.f, 0.25f, "%.2fx")
            .help("Embers thrown off the flamethrowers' fire, arcing down, and wisps of dark smoke from it. 0: none."),
        slider("Lingering Poison Mist", "vr_adgun_fx_mist", 0.f, 3.f, 0.25f, "%.2fx")
            .help("A faint green mist left hanging where the poison sprayers' poison went. 0: none."),
        slider("Scorch Marks", "vr_adgun_fx_scorch", 0.f, 3.f, 0.25f, "%.2fx")
            .help("Scorch marks where the flamethrowers' fire licks a wall or the floor, this many times as often (1 the "
                  "default; 0 none). Cheap: with Permanent Decals on, each is baked straight into them."),
        toggle("Fuel Tank and Hose", "vr_adgun_hose")
            .help("Holding the pyro's flamethrower or the fumigator's poison sprayer: a small fuel tank on your back and "
                  "a hose from it to the gun, swinging as you move (seen in the body's mirror image too). Off: none."),
        cycle("Hose Route", vr_adgun_hose_route, {{0.f, "Hip"}, {1.f, "Shoulder"}})
            .help("The hose's way from the tank to the gun, on the gun's hand's side: through a clip at the hip, or "
                  "over the shoulder."),
        slider("Tank Forward", "vr_adgun_tank_x", -20.f, 5.f, 0.5f, "%.1f").extend(-40.f, 20.f)
            .help("The tank's place on your back (carried by the chest): forward (more negative: further behind)."),
        slider("Tank Up", "vr_adgun_tank_z", -5.f, 15.f, 0.5f, "%.1f").extend(-20.f, 30.f)
            .help("The tank's height on your back (times your height calibration)."),
        toggle("Holstered Guns' Hoses", "vr_adgun_hose_holstered")
            .help("A holstered flamethrower or poison sprayer has its hose to the tank on your back too. Several carried, "
                  "in the hands or the holsters: one tank, the last one held's colour, every gun's hose to it. Off: only "
                  "the guns in your hands have a tank and hose."),
        toggle("Tank Lying About", "vr_adgun_tank_world")
            .help("A flamethrower or poison sprayer lying about (dropped, thrown, a monster's) has its tank beside it, a "
                  "hose between them and a rope holding them together: grab and drag either and the other follows. A "
                  "hand carries the tank (it can't be force grabbed). Taking the gun puts the tank back on your back; "
                  "dropping it drops the tank too. Off: no tank lying about."),
        slider("Tank Rope", "vr_adgun_tank_rope", 0.4f, 1.6f, 0.05f, "%.2f m")
            .help("How far a tank lying about can be from its gun (the gun's grip to the tank's middle); its hose is "
                  "that long and a little more."),
        slider("Tank Weight", "vr_adgun_tank_mass", 0.5f, 10.f, 0.5f, "%.1f kg")
            .help("A tank lying about's weight: how hard it drags its gun, and its gun it."),
        toggle("Electric Bolts", "vr_adgun_bolt_fx")
            .help("The plasma rifles' bolts (the soldiers', eliminators' and centurions', theirs and yours): a faint "
                  "smoke and electric arcs and sparks along them, at the muzzle and where they strike. Off: AD's smoke."),
        // (their tests: Debug > Tests > Arcane Dimensions Monsters, vr_menu_ad_monsters.inc)
    };
}
