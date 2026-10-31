// vr_menu_ad_guns.inc -- Combat > Enemy Weapons > Arcane Dimensions' Guns (AD support; QC vr_ad_enemyguns.qc,
// vr_ad_guns_defs.qc): the vr_adgun_* settings. Included by vr_menu.cpp (inside its namespace).

// AD support: Arcane Dimensions' armed soldiers' own guns, dropped as they die (QC vr_ad_enemyguns.qc): every number of
// your use of them, to balance them (the monsters' own attacks stay Arcane Dimensions'). Their placement in the hand is
// the Weapon Offsets' (slots 28-32: our burst rifle's variants, inheriting its slot 22; the crossbow knight's crossbow 33).
[[nodiscard]] za::Vector<Item> pageAdGuns()
{
    return {
        row(opt::vr_adgun_drops),
        header("Fumigator's Poison Sprayer"),
        row(opt::vr_adgun_fumigator_poison),
        row(opt::vr_adgun_fumigator_poisontime),
        header("Crossbow Knight's Crossbow"),
        row(opt::vr_adgun_crossbow_anim),
        row(opt::vr_adgun_crossbow_bolt_sound),
        header("Melee Monsters' Weapons"),
        row(opt::vr_admelee_drops),
        open("Death Sergeant's Flail", pageIndex(pageFlail))
            .help("Its flail, its spiked head swinging on a chain: the chain's length, the head's mass and air drag, its "
                  "blows' damage and speeds, its rattle, its head lying about (also under Combat > Melee: Flail)."),
        header("Flamethrowers' Look"),
        row(opt::vr_adgun_ad_copies),
        row(opt::vr_adgun_fx_scorch),
        row(opt::vr_adgun_plasma_fx),
        row(opt::vr_adgun_hose),
        row(opt::vr_adgun_hose_route),
        row(opt::vr_adgun_hose_holstered),
        row(opt::vr_adgun_tank_world),
        row(opt::vr_adgun_tank_rope),
        row(opt::vr_adgun_hosecut),
        row(opt::vr_adgun_tank_worn),
        row(opt::vr_adgun_bolt_fx),
        // (their tests: Debug > Tests > Arcane Dimensions Monsters, vr_menu_ad_monsters.inc)
    };
}

// AD support: the death sergeant's flail (QC vr_ad_melee.qc VR_ADMELEE_FLAIL; vr_box3d_flail.cpp): its head a body of
// its own on its chain. Combat > Melee > Flail, and Arcane Dimensions > Soldiers' Guns (the melee monsters' weapons).
// (The author's note vrtesthall_2026-10-23_22-41-32: searching "flail" found none of these, a group on the guns' page.)
[[nodiscard]] za::Vector<Item> pageFlail()
{
    return {
        row(opt::vr_flail_chain),
        header("Sound and Lying About"),
        row(opt::vr_flail_world),
        // (its tests: Debug > Tests > Arcane Dimensions Monsters, vr_menu_ad_monsters.inc)
    };
}
