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
        header("Fumigator's Poison Sprayer"),
        slider("Poison Damage", "vr_adgun_fumigator_poison", 0.f, 30.f, 0.5f, "%.1f / s").extend(0.f, 200.f)
            .help("The poison a puff leaves on what it touches: its damage a second. 0: none."),
        slider("Poisoned For", "vr_adgun_fumigator_poisontime", 0.f, 15.f, 0.5f, "%.1f s").extend(0.f, 60.f),
        header("Crossbow Knight's Crossbow"),
        toggle("Bolt Laid and String Animated", "vr_adgun_crossbow_anim")
            .help("A bolt lies in the groove while the crossbow is ready (morphing in after a shot) and is the one that "
                  "flies; the string snaps forward and the limbs flex, then it is drawn back for the next."),
        slider("Next Bolt's Sound", "vr_adgun_crossbow_bolt_sound", 0.f, 1.f, 0.05f, "%.2f")
            .help("The soft shimmer as the next bolt morphs into the groove: its volume (0: none)."),
        header("Melee Monsters' Weapons"),
        toggle("Melee Monsters Drop Their Weapons", "vr_admelee_drops")
            .help("The death fury drops its two swords, the death knight, zombie knight and death guard their swords, "
                  "the hunter ogres their hammer and mace, the Dark Prince, the justice knight and the judicator their "
                  "swords, the death sergeant its flail; Rogue's Phantom Swordsman falls as its sword: yours to swing (Weapon Offsets slots 41-46, "
                  "47-51)."),
        open("Death Sergeant's Flail", pageIndex(pageFlail))
            .help("Its flail, its spiked head swinging on a chain: the chain's length, the head's mass and air drag, its "
                  "blows' damage and speeds, its rattle, its head lying about (also under Combat > Melee: Flail)."),
        header("Flamethrowers' Look"),
        slider("AD's Flames Frequency", "vr_adgun_ad_copies", 0.f, 3.f, 1.f, "%.0f")
            .help("AD's flame and poison sprites drawn this many more times, between each and where it came from: a "
                  "fuller stream (drawn only: the flames that burn stay AD's). 0: AD's alone."),
        slider("Scorch Marks", "vr_adgun_fx_scorch", 0.f, 3.f, 0.25f, "%.2fx")
            .help("Scorch marks where the flamethrowers' fire licks a wall or the floor: one where every flame is spent "
                  "(under 1: that share of them; over 1: a little bigger). 0: none. Cheap: with Permanent Decals on, "
                  "each is baked straight into them."),
        slider("Plasma Bolt Arcs", "vr_adgun_plasma_fx", 0.f, 3.f, 0.25f, "%.2fx")
            .help("Small lightning arcs crackling round the plasma bolts (the Plasma Gun's, the plasma soldiers' and "
                  "centurions'). 0: none. Their blue light: Projectile Lights."),
        toggle("Fuel Tank and Hose", "vr_adgun_hose")
            .help("Holding the pyro's flamethrower or the fumigator's poison sprayer: a small fuel tank on your back and "
                  "a hose from it to the gun, swinging as you move (seen in the body's mirror image too). Off: none."),
        cycle("Hose Route", vr_adgun_hose_route, {{0.f, "Hip"}, {1.f, "Shoulder"}})
            .help("The hose's way from the tank to the gun, on the gun's hand's side: through a clip at the hip, or "
                  "over the shoulder."),
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
        toggle("Hose Cut", "vr_adgun_hosecut")
            .help("Slash a blade (a sword, an axe, a knife, the chainsaw) across the hose between a gun and its tank "
                  "lying about to cut it: the gun is spent (it never fires again), the tank a prop of its own you can "
                  "force grab, carry and throw, and shoot to burst with the fuel it kept (a thrown bomb, a trap). Both "
                  "keep a short stub of the hose. Fists and blunt weapons (clubs, the hammer, the mace) don't cut it. "
                  "Off: hoses can't be cut."),
        toggle("Armed Monsters' Tanks", "vr_adgun_tank_worn")
            .help("Armed monsters holding a flamethrower or sprayer wear its tank on their back, hosed to the gun. Shoot "
                  "it enough (Tank Health) and it bursts on them: hurt, set on fire or poisoned, the field round them, and "
                  "their gun spent: they drop it."),
        toggle("Electric Bolts", "vr_adgun_bolt_fx")
            .help("The plasma rifles' bolts (the soldiers', eliminators' and centurions', theirs and yours): a faint "
                  "smoke and electric arcs and sparks along them, at the muzzle and where they strike. Off: AD's smoke."),
        // (their tests: Debug > Tests > Arcane Dimensions Monsters, vr_menu_ad_monsters.inc)
    };
}

// AD support: the death sergeant's flail (QC vr_ad_melee.qc VR_ADMELEE_FLAIL; vr_box3d_flail.cpp): its head a body of
// its own on its chain. Combat > Melee > Flail, and Arcane Dimensions > Soldiers' Guns (the melee monsters' weapons).
// (The author's note vrtesthall_2026-10-23_22-41-32: searching "flail" found none of these, a group on the guns' page.)
[[nodiscard]] za::Vector<Item> pageFlail()
{
    return {
        slider("Flail Chain Length", "vr_flail_chain", 0.2f, 1.f, 0.05f, "%.2f m").extend(0.1f, 2.f)
            .help("The flail's chain, from the handle's end to its spiked head, which hangs and swings on it as a body of "
                  "its own (it falls, bounces off walls, wraps a little round what it meets)."),
        header("Sound and Lying About"),
        toggle("Flail Head Lying About", "vr_flail_world")
            .help("A flail lying about (dropped, thrown, a death sergeant's) keeps its head on its chain: a body of its "
                  "own you can carry, dragged by the handle and dragging it. Off: the handle alone."),
        // (its tests: Debug > Tests > Arcane Dimensions Monsters, vr_menu_ad_monsters.inc)
    };
}
