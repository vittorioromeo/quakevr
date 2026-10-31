// vr_menu_physics.inc -- throwing, carrying, burning (wall torches, AD), rocks and bricks, crates, gibs, corpse damage,
// the ragdoll pages. Part of vr_menu.cpp (included where it was: the same translation unit, so its anonymous
// namespace).

[[nodiscard]] za::Vector<Item> pageThrowing()
{
    return {
        row(opt::vr_2h_throw_velocity_mult),
        row(opt::vr_throw_release),
        row(opt::vr_throw_slowmo_stroke_time),
        row(opt::vr_throw_slowmo_real_strength),
        row(opt::vr_throw_spin_from_pose),
        row(opt::vr_angvel_frame),
        header("Throws by Weight"),
        row(opt::vr_throw_wrist_dist),
        header("Physics"),
        row(opt::vr_throw_spin_drag),
        header("Thrown Axes"),
        row(opt::vr_axestick),
        header("Flung Props"),
        row(opt::vr_prop_impact_damage),
        row(opt::vr_prop_impact_players),
        row(opt::vr_gib_spawn_harmless),
        row(opt::vr_prop_drop_grace),
        row(opt::vr_prop_drop_falls_only),
        row(opt::vr_prop_drop_pass_inside),
        row(opt::vr_prop_impact_min_speed),
        row(opt::vr_prop_impact_mult),
        row(opt::vr_prop_impact_speed_max),
        row(opt::vr_prop_impact_weight_curve),
        row(opt::vr_prop_impact_weight_ref),
        row(opt::vr_prop_impact_weight_max),
        row(opt::vr_prop_impact_min_mass),
        header("Shots Push Props"),
        row(opt::vr_shot_push),
        header("Wall Buttons"),
        row(opt::vr_button_throw),
    };
}

// Carrying ammo and health boxes, explosive boxes, armour and pickups.
[[nodiscard]] za::Vector<Item> pageCarrying()
{
    return {
        header("Carrying Boxes"),
        row(opt::vr_carry),
        row(opt::vr_carry_take),
        row(opt::vr_carry_autocollect),
        row(opt::vr_carry_full_cue),
        row(opt::vr_collect_fx),
        row(opt::vr_collect_fx_time),
        row(opt::vr_collect_fx_size),
        row(opt::vr_weapon_grab_drawn),
        row(opt::vr_weapon_grab_slack),
        row(opt::vr_weapon_grab_handle_leniency),
        row(opt::vr_weapon_catch_handle_leniency),
        row(opt::vr_pickup_float_grace),
        row(opt::vr_weapon_grab_hotspots),
        row(opt::vr_weapon_grab_anywhere),
        row(opt::vr_weapon_grab_anywhere_min),
        row(opt::vr_weapon_anygrip_mode),
        row(opt::vr_carry_local),
        row(opt::vr_carry_two_hands),
        row(opt::vr_carry_two_hands_solid),
        row(opt::vr_carry_body_collision),
        row(opt::vr_carry_move_mass),
        row(opt::vr_carry_move_size),
        row(opt::vr_carry_move_min),
        row(opt::vr_held_surface_fit),
        row(opt::vr_grip_fit),
        row(opt::vr_held_collide),
        row(opt::vr_hand_collide_props_keep_out),
        row(opt::vr_hand_collide_fingers),
        row(opt::vr_gun_wall_slide),
        row(opt::vr_held_collide_walls),
        row(opt::vr_model_collide_rest),
        row(opt::vr_held_collide_monsters),
        row(opt::vr_box3d_hand_props),
        row(opt::vr_box3d_hand_push_fist),
        row(opt::vr_box3d_weapon_push),
        row(opt::vr_box3d_gun_pieces),
        row(opt::vr_box3d_throw_grace),
        row(opt::vr_box3d_throw_grace_body),
        row(opt::vr_box3d_press_back),
        row(opt::vr_throw_inherit),
        row(opt::vr_carry_melee_mult),
        open("Flung Props (Throwing and Physics)", pageIndex(pageThrowing))
            .help("Whether props batted, knocked flying or swung on the grapple hurt monsters (and players), and how much."),
        open("Held Object Offsets (Held Prop)", pageIndex(pageHeldObjectOffsets))
            .help("The grip, fingers and melee points of what a hand carries."),
        open("Held Object Weights (Held Prop)", pageIndex(pageHeldObjectWeights))
            .help("The mass, spring and damage of what a hand carries (Aiming: Weight for how weight feels)."),
        header("Physics Sounds"),
        row(opt::vr_physsound),
        open("Climbing Grab Sound: Climbing", pageIndex(pageClimbing)).help("The climbing grab sound is on Movement > Climbing."),
        header("Explosive Boxes"),
        row(opt::vr_explobox_physics),
        row(opt::vr_explobox_impact),
        header("Armour and Pickups"),
        row(opt::vr_armor_wear),
        row(opt::vr_item_objects),
        row(opt::vr_pickup_prop_models),
    };
}

[[nodiscard]] za::Vector<Item> pageBurning();

// Split from Carrying and Gibs: the wall torches you can take.
[[nodiscard]] za::Vector<Item> pageWallTorches()
{
    return {
        row(opt::vr_walltorch),
        row(opt::vr_walltorch_grab_time),
        row(opt::vr_walltorch_forcegrab),
        row(opt::vr_walltorch_die_time),
        open("Burning", pageIndex(pageBurning))
            .help("A lit torch's blow sets what it hits on fire: Combat > Burning (its damage, time and flames)."),
        row(opt::vr_walltorch_relight),
        row(opt::vr_walltorch_shot),
        row(opt::vr_walltorch_shadows),
    };
}

// Combat > Burning (QC vr_burning.qc): what a lit torch, a thrown one or a lava nail sets on fire; crates; nails through
// a torch's flame.
// Combat > Arcane Dimensions (AD support, ARCANE_DIMENSIONS.md decisions 2 and 22): AD's projectile pellets and whose
// projectile rules play (QC vr_ad_bridge.qc AD_Pellets).
[[nodiscard]] za::Vector<Item> pageArcaneDimensions()
{
    return {
        row(opt::vr_ad_projectiles),
        row(opt::vr_ad_pellet_spread),
        row(opt::vr_ad_pellet_speed),
        row(opt::vr_ad_enemy_pellet_speed),
        row(opt::vr_ad_pellet_damage),
        header("Feature Scope"),
        row(opt::vr_ad_ammo_models),
        row(opt::vr_ad_resistance),
        row(opt::vr_ad_upgrades),
        row(opt::vr_ad_evil),
        row(opt::vr_ad_tints),
        row(opt::vr_climb_ladders),
        row(opt::vr_ad_everywhere),
        command("Map Logic / Inventory Override", "")
            .unavailableBecause("AD map logic, key progression and scripted inventory remain AD-map-only. A global override is not supported: it would break triggers, exits and item placement. VR interactions, hit fixes and physics always remain active."),
        open("Soldiers' Guns", pageIndex(pageAdGuns))
            .help("Arcane Dimensions' armed soldiers' own guns, dropped as they die: their damage, ammo and speed."),
        command("AD Status", "vr_ad_info"),
        command("Scope and Ammo Model Test", "vr_ad_test 65")
            .help("Reports the feature scopes, all eight ammo box models and pickup physics; checks both held weapon models stay unchanged."),
    };
}

[[nodiscard]] za::Vector<Item> pageBurning()
{
    return {
        header("Corpses"),
        row(opt::vr_burn_corpses),
        row(opt::vr_burn_lava_bodies),
        row(opt::vr_burn_lava_gib),
        header("What Sets Things on Fire"),
        row(opt::vr_burn_touch),
        row(opt::vr_burn_lava_nails),
        row(opt::vr_burn_nail_convert),
        row(opt::vr_burn_nail_burning),
        row(opt::vr_burn_nail_sound),
        row(opt::vr_burn_map_monsters),
        row(opt::vr_burn_water_douse),
        header("Crates"),
        row(opt::vr_burn_crates),
        row(opt::vr_burn_lava),
        row(opt::vr_burn_crate_break),
        row(opt::vr_burn_crate_char),
        row(opt::vr_burn_pieces),
        header("Torch Flame"),
        open("Fire Particles", pageIndex(pageFireParticles)),
        row(opt::vr_walltorch_drips),
        header("Your Own Torch"),
        row(opt::vr_burn_self),
        row(opt::vr_burn_self_time),
        row(opt::vr_burn_drop),
    };
}

// Split from Carrying and Gibs: rocks and bricks lying about.
[[nodiscard]] za::Vector<Item> pageRocksBricks()
{
    return {
        row(opt::vr_debris),
        row(opt::vr_debris_rocks),
        row(opt::vr_debris_bricks),
        debrisMultiplayerSlider(),
    };
}

// Wooden crates (vr_crates*; vr_crates.cpp, QC vr_crates.qc): where they lie, breaking them, what they hold, hiding.
[[nodiscard]] za::Vector<Item> pageCrates()
{
    return {
        header("Where They Lie"),
        row(opt::vr_crates),
        header("Breaking"),
        row(opt::vr_crate_health),
        row(opt::vr_prop_break_speed),
        row(opt::vr_prop_blow_wear),
        row(opt::vr_prop_break_wood),
        row(opt::vr_prop_break_glass),
        row(opt::vr_prop_break_stone),
        row(opt::vr_prop_break_metal),
        row(opt::vr_prop_break_explosive),
        row(opt::vr_prop_break_piece),
        row(opt::vr_carry_slam),
        header("What They Hold"),
        row(opt::vr_crate_flaregun),
        row(opt::vr_crate_flaregun_max),
        header("Hiding"),
        row(opt::vr_crate_sight),
        row(opt::vr_crate_shield),
    };
}

[[nodiscard]] za::Vector<Item> pageRagdolls(); // (below)
[[nodiscard]] za::Vector<Item> pageRagdollGrunt();
[[nodiscard]] za::Vector<Item> pageRagdollKnight();
[[nodiscard]] za::Vector<Item> pageCorpseDamage();
[[nodiscard]] za::Vector<Item> pageRagdollOgre();
[[nodiscard]] za::Vector<Item> pageRagdollEnforcer();
[[nodiscard]] za::Vector<Item> pageRagdollDeathKnight();
[[nodiscard]] za::Vector<Item> pageRagdollRottweiler();
[[nodiscard]] za::Vector<Item> pageRagdollScrag();
[[nodiscard]] za::Vector<Item> pageRagdollZombie();
[[nodiscard]] za::Vector<Item> pageRagdollMummy();
[[nodiscard]] za::Vector<Item> pageRagdollGremlin();
[[nodiscard]] za::Vector<Item> pageRagdollVore();
[[nodiscard]] za::Vector<Item> pageRagdollCentroid();
[[nodiscard]] za::Vector<Item> pageRagdollShambler();
[[nodiscard]] za::Vector<Item> pageRagdollFiend();

// Split from Carrying and Gibs: taking, throwing and bursting gibs, heads and corpses.
[[nodiscard]] za::Vector<Item> pageGibs()
{
    return {
        row(opt::vr_grab_gibs),
        row(opt::vr_gib_destroy),
        row(opt::vr_corpse_gib),
        row(opt::vr_corpse_nogib),
        open("Corpse Damage and Health", pageIndex(pageCorpseDamage)),
        header("Corpse Collision"),
        row(opt::vr_corpse_collide),
        row(opt::vr_corpse_collide_props),
        row(opt::vr_corpse_collide_thrown),
        row(opt::vr_corpse_collide_held),
        row(opt::vr_corpse_collide_player),
        row(opt::vr_box3d_walk_nudge),
        row(opt::vr_corpse_collide_monsters),
        header("Ragdolls (Experimental)"),
        row(opt::vr_ragdoll),
        open("Ragdoll Settings", pageIndex(pageRagdolls)),
    };
}

// Gibs and Corpses > Corpse Damage and Health (ROUND21.md, "Ragdolls on; corpse health and damage by kind"): how much
// damage it takes to gib a corpse or a ragdoll: each kind of hit's share of its damage (VR_Corpse_DamageKind, QC), each
// monster's corpse health (times Corpse Health, vr_corpse_health_mult).
[[nodiscard]] za::Vector<Item> pageCorpseDamage()
{
    return {
        open("Corpse Health, Never Gib: Gibs and Corpses", pageIndex(pageGibs)).help("Corpse Health and Never Gib Corpses are on Gibs and Corpses."),
    };
}

// Gibs and Corpses > Ragdoll Settings (ROUND21.md, "Ragdolls"): on or off, when they go limp and how many; their physics
// for all (each monster class may have its own: its page); taking them by hand and by force grab; their blood.
[[nodiscard]] za::Vector<Item> pageRagdolls()
{
    return {
        open("Ragdolls On/Off: Gibs and Corpses", pageIndex(pageGibs)).help("Ragdolls on or off is on Gibs and Corpses."),
        row(opt::vr_ragdoll_start),
        row(opt::vr_ragdoll_max),
        row(opt::vr_ragdoll_collide_each),
        row(opt::vr_ragdoll_self_collide),
        header("Active Deaths (Experimental)"),
        open("Test Them", pageIndex(pageRagdollTests))
            .help("Debug > Tests > Ragdoll Behaviours: each behaviour below set going on the nearest grunt, enforcer, "
                  "knight, death knight, ogre or zombie."),
        row(opt::vr_ragdoll_active),
        row(opt::vr_ragdoll_active_strength),
        row(opt::vr_ragdoll_active_fade),
        row(opt::vr_ragdoll_active_hertz),
        row(opt::vr_ragdoll_active_damping),
        row(opt::vr_ragdoll_active_torque),
        row(opt::vr_ragdoll_active_pin),
        row(opt::vr_ragdoll_active_pin_time),
        row(opt::vr_ragdoll_active_stumble),
        row(opt::vr_ragdoll_active_dismember),
        row(opt::vr_ragdoll_active_dismember_time),
        row(opt::vr_ragdoll_active_writhe),
        row(opt::vr_ragdoll_active_balance_time),
        row(opt::vr_ragdoll_active_balance_lean),
        row(opt::vr_ragdoll_active_wound),
        row(opt::vr_ragdoll_active_env),
        row(opt::vr_ragdoll_active_crawl),
        row(opt::vr_ragdoll_active_crawl_time),
        row(opt::vr_ragdoll_active_crawl_back),
        row(opt::vr_ragdoll_active_crawl_force),
        row(opt::vr_ragdoll_active_liquids),
        row(opt::vr_ragdoll_active_paddle),
        row(opt::vr_ragdoll_active_flail),
        header("Hit Reactions (Living Monsters)"),
        row(opt::vr_hit_react),
        row(opt::vr_hit_react_strength),
        row(opt::vr_hit_react_time),
        row(opt::vr_hit_react_slow),
        row(opt::vr_ragdoll_active_stagger),
        row(opt::vr_ragdoll_active_stagger_lean),
        row(opt::vr_ragdoll_active_stagger_steps),
        row(opt::vr_ragdoll_active_stagger_travel),
        row(opt::vr_ragdoll_active_stagger_push),
        row(opt::vr_ragdoll_active_stagger_pin),
        row(opt::vr_ragdoll_active_stagger_blend),
        row(opt::vr_ragdoll_active_stagger_settle),
        row(opt::vr_ragdoll_active_stagger_settle_time),
        row(opt::vr_ragdoll_active_stagger_drop),
        row(opt::vr_ragdoll_active_getup),
        row(opt::vr_ragdoll_active_drown),
        row(opt::vr_ragdoll_active_pullup),
        row(opt::vr_ragdoll_active_grab),
        row(opt::vr_ragdoll_active_guard),
        row(opt::vr_corpse_placed),
        row(opt::vr_ragdoll_active_getup_pin),
        row(opt::vr_ragdoll_active_getup_pace),
        row(opt::vr_ragdoll_active_getup_blend),
        row(opt::vr_ragdoll_active_getup_hold),
        header("Physics (All Monsters)"),
        row(opt::vr_ragdoll_mass),
        row(opt::vr_ragdoll_friction),
        row(opt::vr_ragdoll_joint_friction),
        row(opt::vr_ragdoll_joint_stiffness),
        row(opt::vr_ragdoll_limits),
        row(opt::vr_ragdoll_damping),
        row(opt::vr_ragdoll_blast),
        row(opt::vr_ragdoll_inherit),
        header("Each Monster's Own"),
        open("Grunt", pageIndex(pageRagdollGrunt)),
        open("Knight", pageIndex(pageRagdollKnight)),
        open("Ogre", pageIndex(pageRagdollOgre)),
        open("Enforcer", pageIndex(pageRagdollEnforcer)),
        open("Death Knight", pageIndex(pageRagdollDeathKnight)),
        open("Rottweiler", pageIndex(pageRagdollRottweiler)),
        open("Scrag", pageIndex(pageRagdollScrag)),
        open("Fiend", pageIndex(pageRagdollFiend)),
        open("Shambler", pageIndex(pageRagdollShambler)),
        open("Vore", pageIndex(pageRagdollVore)),
        open("Gremlin", pageIndex(pageRagdollGremlin)),
        open("Centroid", pageIndex(pageRagdollCentroid)).help("A centroid's ragdoll (Scourge of Armagon's scorpion)."),
        open("Zombie", pageIndex(pageRagdollZombie)).help("A zombie's ragdoll: only when its head is cut off (Gore > Decapitation)."),
        open("Mummy", pageIndex(pageRagdollMummy)).help("A mummy's ragdoll (Dissolution of Eternity): only when its head is cut off (Gore > Decapitation)."),
        header("Taking Them"),
        row(opt::vr_ragdoll_grab_reach),
    };
}

// Gibs and Corpses > Ragdoll Settings > Grunt: the grunt's own physics (vr_ragdoll_army_*), each one Global (the one for
// all monsters) or its own.
[[nodiscard]] za::Vector<Item> pageRagdollGrunt()
{
    return {
        classSlider("Go Limp At", vr_ragdoll_army_start, 0.f, 1.f, 0.1f, "%.1f").help("vr_ragdoll_army_start; Global: Go Limp At."),
        classSlider("Mass", vr_ragdoll_army_mass, 20.f, 200.f, 5.f, "%.0f kg").extend(0.f, 1000.f)
            .help("vr_ragdoll_army_mass; Global: Mass."),
        classSlider("Friction", vr_ragdoll_army_friction, 0.1f, 2.f, 0.1f, "%.1f").extend(0.f, 10.f)
            .help("vr_ragdoll_army_friction; Global: Friction."),
        classSlider("Joint Friction", vr_ragdoll_army_joint_friction, 0.f, 10.f, 0.5f, "%.1f N m").extend(0.f, 100.f)
            .help("vr_ragdoll_army_joint_friction; Global: Joint Friction."),
        classSlider("Joint Stiffness", vr_ragdoll_army_joint_stiffness, 0.f, 5.f, 0.25f, "%.2f Hz").extend(0.f, 30.f)
            .help("vr_ragdoll_army_joint_stiffness; Global: Joint Stiffness."),
        classSlider("Joint Limits", vr_ragdoll_army_limits, 0.25f, 1.5f, 0.05f, "%.2fx").extend(0.f, 3.f)
            .help("vr_ragdoll_army_limits; Global: Joint Limits."),
        classSlider("Limb Damping", vr_ragdoll_army_damping, 0.f, 3.f, 0.1f, "%.1f").extend(0.f, 20.f)
            .help("vr_ragdoll_army_damping; Global: Limb Damping."),
        classSlider("Blast Throw", vr_ragdoll_army_blast, 0.f, 5.f, 0.25f, "%.2fx").extend(0.f, 20.f)
            .help("vr_ragdoll_army_blast; Global: Blast Throw."),
        classSlider("Death Motion Kept", vr_ragdoll_army_inherit, 0.f, 2.f, 0.1f, "%.1fx")
            .help("vr_ragdoll_army_inherit; Global: Death Motion Kept."),
        command("All Global", "vr_ragdoll_army_start -1; vr_ragdoll_army_mass -1; vr_ragdoll_army_friction -1; "
                              "vr_ragdoll_army_joint_friction -1; vr_ragdoll_army_joint_stiffness -1; vr_ragdoll_army_limits -1; "
                              "vr_ragdoll_army_damping -1; vr_ragdoll_army_blast -1; vr_ragdoll_army_inherit -1")
            .help("The grunt's ragdoll as all monsters' (Ragdoll Settings)."),
    };
}

// Gibs and Corpses > Ragdoll Settings > Knight: the knight's own physics (vr_ragdoll_knight_*), each one Global (the one for
// all monsters) or its own.
[[nodiscard]] za::Vector<Item> pageRagdollKnight()
{
    return {
        classSlider("Go Limp At", vr_ragdoll_knight_start, 0.f, 1.f, 0.1f, "%.1f").help("vr_ragdoll_knight_start; Global: Go Limp At."),
        classSlider("Mass", vr_ragdoll_knight_mass, 20.f, 200.f, 5.f, "%.0f kg").extend(0.f, 1000.f)
            .help("vr_ragdoll_knight_mass; Global: Mass."),
        classSlider("Friction", vr_ragdoll_knight_friction, 0.1f, 2.f, 0.1f, "%.1f").extend(0.f, 10.f)
            .help("vr_ragdoll_knight_friction; Global: Friction."),
        classSlider("Joint Friction", vr_ragdoll_knight_joint_friction, 0.f, 10.f, 0.5f, "%.1f N m").extend(0.f, 100.f)
            .help("vr_ragdoll_knight_joint_friction; Global: Joint Friction."),
        classSlider("Joint Stiffness", vr_ragdoll_knight_joint_stiffness, 0.f, 5.f, 0.25f, "%.2f Hz").extend(0.f, 30.f)
            .help("vr_ragdoll_knight_joint_stiffness; Global: Joint Stiffness."),
        classSlider("Joint Limits", vr_ragdoll_knight_limits, 0.25f, 1.5f, 0.05f, "%.2fx").extend(0.f, 3.f)
            .help("vr_ragdoll_knight_limits; Global: Joint Limits."),
        classSlider("Limb Damping", vr_ragdoll_knight_damping, 0.f, 3.f, 0.1f, "%.1f").extend(0.f, 20.f)
            .help("vr_ragdoll_knight_damping; Global: Limb Damping."),
        classSlider("Blast Throw", vr_ragdoll_knight_blast, 0.f, 5.f, 0.25f, "%.2fx").extend(0.f, 20.f)
            .help("vr_ragdoll_knight_blast; Global: Blast Throw."),
        classSlider("Death Motion Kept", vr_ragdoll_knight_inherit, 0.f, 2.f, 0.1f, "%.1fx")
            .help("vr_ragdoll_knight_inherit; Global: Death Motion Kept."),
        command("All Global", "vr_ragdoll_knight_start -1; vr_ragdoll_knight_mass -1; vr_ragdoll_knight_friction -1; "
                              "vr_ragdoll_knight_joint_friction -1; vr_ragdoll_knight_joint_stiffness -1; vr_ragdoll_knight_limits -1; "
                              "vr_ragdoll_knight_damping -1; vr_ragdoll_knight_blast -1; vr_ragdoll_knight_inherit -1")
            .help("The knight's ragdoll as all monsters' (Ragdoll Settings)."),
    };
}

// Gibs and Corpses > Ragdoll Settings > Ogre: the ogre's own physics (vr_ragdoll_ogre_*), each one Global (the one for
// all monsters) or its own.
[[nodiscard]] za::Vector<Item> pageRagdollOgre()
{
    return {
        classSlider("Go Limp At", vr_ragdoll_ogre_start, 0.f, 1.f, 0.1f, "%.1f").help("vr_ragdoll_ogre_start; Global: Go Limp At."),
        classSlider("Mass", vr_ragdoll_ogre_mass, 20.f, 200.f, 5.f, "%.0f kg").extend(0.f, 1000.f)
            .help("vr_ragdoll_ogre_mass; Global: Mass."),
        classSlider("Friction", vr_ragdoll_ogre_friction, 0.1f, 2.f, 0.1f, "%.1f").extend(0.f, 10.f)
            .help("vr_ragdoll_ogre_friction; Global: Friction."),
        classSlider("Joint Friction", vr_ragdoll_ogre_joint_friction, 0.f, 10.f, 0.5f, "%.1f N m").extend(0.f, 100.f)
            .help("vr_ragdoll_ogre_joint_friction; Global: Joint Friction."),
        classSlider("Joint Stiffness", vr_ragdoll_ogre_joint_stiffness, 0.f, 5.f, 0.25f, "%.2f Hz").extend(0.f, 30.f)
            .help("vr_ragdoll_ogre_joint_stiffness; Global: Joint Stiffness."),
        classSlider("Joint Limits", vr_ragdoll_ogre_limits, 0.25f, 1.5f, 0.05f, "%.2fx").extend(0.f, 3.f)
            .help("vr_ragdoll_ogre_limits; Global: Joint Limits."),
        classSlider("Limb Damping", vr_ragdoll_ogre_damping, 0.f, 3.f, 0.1f, "%.1f").extend(0.f, 20.f)
            .help("vr_ragdoll_ogre_damping; Global: Limb Damping."),
        classSlider("Blast Throw", vr_ragdoll_ogre_blast, 0.f, 5.f, 0.25f, "%.2fx").extend(0.f, 20.f)
            .help("vr_ragdoll_ogre_blast; Global: Blast Throw."),
        classSlider("Death Motion Kept", vr_ragdoll_ogre_inherit, 0.f, 2.f, 0.1f, "%.1fx")
            .help("vr_ragdoll_ogre_inherit; Global: Death Motion Kept."),
        command("All Global", "vr_ragdoll_ogre_start -1; vr_ragdoll_ogre_mass -1; vr_ragdoll_ogre_friction -1; "
                              "vr_ragdoll_ogre_joint_friction -1; vr_ragdoll_ogre_joint_stiffness -1; vr_ragdoll_ogre_limits -1; "
                              "vr_ragdoll_ogre_damping -1; vr_ragdoll_ogre_blast -1; vr_ragdoll_ogre_inherit -1")
            .help("The ogre's ragdoll as all monsters' (Ragdoll Settings)."),
    };
}

// Gibs and Corpses > Ragdoll Settings > Enforcer: the enforcer's own physics (vr_ragdoll_enforcer_*), each one Global (the one for
// all monsters) or its own.
[[nodiscard]] za::Vector<Item> pageRagdollEnforcer()
{
    return {
        classSlider("Go Limp At", vr_ragdoll_enforcer_start, 0.f, 1.f, 0.1f, "%.1f").help("vr_ragdoll_enforcer_start; Global: Go Limp At."),
        classSlider("Mass", vr_ragdoll_enforcer_mass, 20.f, 200.f, 5.f, "%.0f kg").extend(0.f, 1000.f)
            .help("vr_ragdoll_enforcer_mass; Global: Mass."),
        classSlider("Friction", vr_ragdoll_enforcer_friction, 0.1f, 2.f, 0.1f, "%.1f").extend(0.f, 10.f)
            .help("vr_ragdoll_enforcer_friction; Global: Friction."),
        classSlider("Joint Friction", vr_ragdoll_enforcer_joint_friction, 0.f, 10.f, 0.5f, "%.1f N m").extend(0.f, 100.f)
            .help("vr_ragdoll_enforcer_joint_friction; Global: Joint Friction."),
        classSlider("Joint Stiffness", vr_ragdoll_enforcer_joint_stiffness, 0.f, 5.f, 0.25f, "%.2f Hz").extend(0.f, 30.f)
            .help("vr_ragdoll_enforcer_joint_stiffness; Global: Joint Stiffness."),
        classSlider("Joint Limits", vr_ragdoll_enforcer_limits, 0.25f, 1.5f, 0.05f, "%.2fx").extend(0.f, 3.f)
            .help("vr_ragdoll_enforcer_limits; Global: Joint Limits."),
        classSlider("Limb Damping", vr_ragdoll_enforcer_damping, 0.f, 3.f, 0.1f, "%.1f").extend(0.f, 20.f)
            .help("vr_ragdoll_enforcer_damping; Global: Limb Damping."),
        classSlider("Blast Throw", vr_ragdoll_enforcer_blast, 0.f, 5.f, 0.25f, "%.2fx").extend(0.f, 20.f)
            .help("vr_ragdoll_enforcer_blast; Global: Blast Throw."),
        classSlider("Death Motion Kept", vr_ragdoll_enforcer_inherit, 0.f, 2.f, 0.1f, "%.1fx")
            .help("vr_ragdoll_enforcer_inherit; Global: Death Motion Kept."),
        command("All Global", "vr_ragdoll_enforcer_start -1; vr_ragdoll_enforcer_mass -1; vr_ragdoll_enforcer_friction -1; "
                              "vr_ragdoll_enforcer_joint_friction -1; vr_ragdoll_enforcer_joint_stiffness -1; vr_ragdoll_enforcer_limits -1; "
                              "vr_ragdoll_enforcer_damping -1; vr_ragdoll_enforcer_blast -1; vr_ragdoll_enforcer_inherit -1")
            .help("The enforcer's ragdoll as all monsters' (Ragdoll Settings)."),
    };
}

// Gibs and Corpses > Ragdoll Settings > Death Knight: the death knight's own physics (vr_ragdoll_hknight_*), each one Global (the one for
// all monsters) or its own.
[[nodiscard]] za::Vector<Item> pageRagdollDeathKnight()
{
    return {
        classSlider("Go Limp At", vr_ragdoll_hknight_start, 0.f, 1.f, 0.1f, "%.1f").help("vr_ragdoll_hknight_start; Global: Go Limp At."),
        classSlider("Mass", vr_ragdoll_hknight_mass, 20.f, 200.f, 5.f, "%.0f kg").extend(0.f, 1000.f)
            .help("vr_ragdoll_hknight_mass; Global: Mass."),
        classSlider("Friction", vr_ragdoll_hknight_friction, 0.1f, 2.f, 0.1f, "%.1f").extend(0.f, 10.f)
            .help("vr_ragdoll_hknight_friction; Global: Friction."),
        classSlider("Joint Friction", vr_ragdoll_hknight_joint_friction, 0.f, 10.f, 0.5f, "%.1f N m").extend(0.f, 100.f)
            .help("vr_ragdoll_hknight_joint_friction; Global: Joint Friction."),
        classSlider("Joint Stiffness", vr_ragdoll_hknight_joint_stiffness, 0.f, 5.f, 0.25f, "%.2f Hz").extend(0.f, 30.f)
            .help("vr_ragdoll_hknight_joint_stiffness; Global: Joint Stiffness."),
        classSlider("Joint Limits", vr_ragdoll_hknight_limits, 0.25f, 1.5f, 0.05f, "%.2fx").extend(0.f, 3.f)
            .help("vr_ragdoll_hknight_limits; Global: Joint Limits."),
        classSlider("Limb Damping", vr_ragdoll_hknight_damping, 0.f, 3.f, 0.1f, "%.1f").extend(0.f, 20.f)
            .help("vr_ragdoll_hknight_damping; Global: Limb Damping."),
        classSlider("Blast Throw", vr_ragdoll_hknight_blast, 0.f, 5.f, 0.25f, "%.2fx").extend(0.f, 20.f)
            .help("vr_ragdoll_hknight_blast; Global: Blast Throw."),
        classSlider("Death Motion Kept", vr_ragdoll_hknight_inherit, 0.f, 2.f, 0.1f, "%.1fx")
            .help("vr_ragdoll_hknight_inherit; Global: Death Motion Kept."),
        command("All Global", "vr_ragdoll_hknight_start -1; vr_ragdoll_hknight_mass -1; vr_ragdoll_hknight_friction -1; "
                              "vr_ragdoll_hknight_joint_friction -1; vr_ragdoll_hknight_joint_stiffness -1; vr_ragdoll_hknight_limits -1; "
                              "vr_ragdoll_hknight_damping -1; vr_ragdoll_hknight_blast -1; vr_ragdoll_hknight_inherit -1")
            .help("The death knight's ragdoll as all monsters' (Ragdoll Settings)."),
    };
}

// Gibs and Corpses > Ragdoll Settings > Rottweiler: the rottweiler's own physics (vr_ragdoll_dog_*), each one Global (the one for
// all monsters) or its own.
[[nodiscard]] za::Vector<Item> pageRagdollRottweiler()
{
    return {
        classSlider("Go Limp At", vr_ragdoll_dog_start, 0.f, 1.f, 0.1f, "%.1f").help("vr_ragdoll_dog_start; Global: Go Limp At."),
        classSlider("Mass", vr_ragdoll_dog_mass, 20.f, 200.f, 5.f, "%.0f kg").extend(0.f, 1000.f)
            .help("vr_ragdoll_dog_mass; Global: Mass."),
        classSlider("Friction", vr_ragdoll_dog_friction, 0.1f, 2.f, 0.1f, "%.1f").extend(0.f, 10.f)
            .help("vr_ragdoll_dog_friction; Global: Friction."),
        classSlider("Joint Friction", vr_ragdoll_dog_joint_friction, 0.f, 10.f, 0.5f, "%.1f N m").extend(0.f, 100.f)
            .help("vr_ragdoll_dog_joint_friction; Global: Joint Friction."),
        classSlider("Joint Stiffness", vr_ragdoll_dog_joint_stiffness, 0.f, 5.f, 0.25f, "%.2f Hz").extend(0.f, 30.f)
            .help("vr_ragdoll_dog_joint_stiffness; Global: Joint Stiffness."),
        classSlider("Joint Limits", vr_ragdoll_dog_limits, 0.25f, 1.5f, 0.05f, "%.2fx").extend(0.f, 3.f)
            .help("vr_ragdoll_dog_limits; Global: Joint Limits."),
        classSlider("Limb Damping", vr_ragdoll_dog_damping, 0.f, 3.f, 0.1f, "%.1f").extend(0.f, 20.f)
            .help("vr_ragdoll_dog_damping; Global: Limb Damping."),
        classSlider("Blast Throw", vr_ragdoll_dog_blast, 0.f, 5.f, 0.25f, "%.2fx").extend(0.f, 20.f)
            .help("vr_ragdoll_dog_blast; Global: Blast Throw."),
        classSlider("Death Motion Kept", vr_ragdoll_dog_inherit, 0.f, 2.f, 0.1f, "%.1fx")
            .help("vr_ragdoll_dog_inherit; Global: Death Motion Kept."),
        command("All Global", "vr_ragdoll_dog_start -1; vr_ragdoll_dog_mass -1; vr_ragdoll_dog_friction -1; "
                              "vr_ragdoll_dog_joint_friction -1; vr_ragdoll_dog_joint_stiffness -1; vr_ragdoll_dog_limits -1; "
                              "vr_ragdoll_dog_damping -1; vr_ragdoll_dog_blast -1; vr_ragdoll_dog_inherit -1")
            .help("The rottweiler's ragdoll as all monsters' (Ragdoll Settings)."),
    };
}

// Gibs and Corpses > Ragdoll Settings > Scrag: the scrag's own physics (vr_ragdoll_wizard_*), each one Global (the one for
// all monsters) or its own.
[[nodiscard]] za::Vector<Item> pageRagdollScrag()
{
    return {
        classSlider("Go Limp At", vr_ragdoll_wizard_start, 0.f, 1.f, 0.1f, "%.1f").help("vr_ragdoll_wizard_start; Global: Go Limp At."),
        classSlider("Mass", vr_ragdoll_wizard_mass, 20.f, 200.f, 5.f, "%.0f kg").extend(0.f, 1000.f)
            .help("vr_ragdoll_wizard_mass; Global: Mass."),
        classSlider("Friction", vr_ragdoll_wizard_friction, 0.1f, 2.f, 0.1f, "%.1f").extend(0.f, 10.f)
            .help("vr_ragdoll_wizard_friction; Global: Friction."),
        classSlider("Joint Friction", vr_ragdoll_wizard_joint_friction, 0.f, 10.f, 0.5f, "%.1f N m").extend(0.f, 100.f)
            .help("vr_ragdoll_wizard_joint_friction; Global: Joint Friction."),
        classSlider("Joint Stiffness", vr_ragdoll_wizard_joint_stiffness, 0.f, 5.f, 0.25f, "%.2f Hz").extend(0.f, 30.f)
            .help("vr_ragdoll_wizard_joint_stiffness; Global: Joint Stiffness."),
        classSlider("Joint Limits", vr_ragdoll_wizard_limits, 0.25f, 1.5f, 0.05f, "%.2fx").extend(0.f, 3.f)
            .help("vr_ragdoll_wizard_limits; Global: Joint Limits."),
        classSlider("Limb Damping", vr_ragdoll_wizard_damping, 0.f, 3.f, 0.1f, "%.1f").extend(0.f, 20.f)
            .help("vr_ragdoll_wizard_damping; Global: Limb Damping."),
        classSlider("Blast Throw", vr_ragdoll_wizard_blast, 0.f, 5.f, 0.25f, "%.2fx").extend(0.f, 20.f)
            .help("vr_ragdoll_wizard_blast; Global: Blast Throw."),
        classSlider("Death Motion Kept", vr_ragdoll_wizard_inherit, 0.f, 2.f, 0.1f, "%.1fx")
            .help("vr_ragdoll_wizard_inherit; Global: Death Motion Kept."),
        command("All Global", "vr_ragdoll_wizard_start -1; vr_ragdoll_wizard_mass -1; vr_ragdoll_wizard_friction -1; "
                              "vr_ragdoll_wizard_joint_friction -1; vr_ragdoll_wizard_joint_stiffness -1; vr_ragdoll_wizard_limits -1; "
                              "vr_ragdoll_wizard_damping -1; vr_ragdoll_wizard_blast -1; vr_ragdoll_wizard_inherit -1")
            .help("The scrag's ragdoll as all monsters' (Ragdoll Settings)."),
    };
}

// Gibs and Corpses > Ragdoll Settings > Zombie: the zombie's own physics (vr_ragdoll_zombie_*), each one Global (the one for
// all monsters) or its own. Its ragdoll is only made when it is beheaded (QC vr_decap.qc).
[[nodiscard]] za::Vector<Item> pageRagdollZombie()
{
    return {
        classSlider("Go Limp At", vr_ragdoll_zombie_start, 0.f, 1.f, 0.1f, "%.1f").help("vr_ragdoll_zombie_start; Global: Go Limp At."),
        classSlider("Mass", vr_ragdoll_zombie_mass, 20.f, 200.f, 5.f, "%.0f kg").extend(0.f, 1000.f)
            .help("vr_ragdoll_zombie_mass; Global: Mass."),
        classSlider("Friction", vr_ragdoll_zombie_friction, 0.1f, 2.f, 0.1f, "%.1f").extend(0.f, 10.f)
            .help("vr_ragdoll_zombie_friction; Global: Friction."),
        classSlider("Joint Friction", vr_ragdoll_zombie_joint_friction, 0.f, 10.f, 0.5f, "%.1f N m").extend(0.f, 100.f)
            .help("vr_ragdoll_zombie_joint_friction; Global: Joint Friction."),
        classSlider("Joint Stiffness", vr_ragdoll_zombie_joint_stiffness, 0.f, 5.f, 0.25f, "%.2f Hz").extend(0.f, 30.f)
            .help("vr_ragdoll_zombie_joint_stiffness; Global: Joint Stiffness."),
        classSlider("Joint Limits", vr_ragdoll_zombie_limits, 0.25f, 1.5f, 0.05f, "%.2fx").extend(0.f, 3.f)
            .help("vr_ragdoll_zombie_limits; Global: Joint Limits."),
        classSlider("Limb Damping", vr_ragdoll_zombie_damping, 0.f, 3.f, 0.1f, "%.1f").extend(0.f, 20.f)
            .help("vr_ragdoll_zombie_damping; Global: Limb Damping."),
        classSlider("Blast Throw", vr_ragdoll_zombie_blast, 0.f, 5.f, 0.25f, "%.2fx").extend(0.f, 20.f)
            .help("vr_ragdoll_zombie_blast; Global: Blast Throw."),
        classSlider("Death Motion Kept", vr_ragdoll_zombie_inherit, 0.f, 2.f, 0.1f, "%.1fx")
            .help("vr_ragdoll_zombie_inherit; Global: Death Motion Kept."),
        command("All Global", "vr_ragdoll_zombie_start -1; vr_ragdoll_zombie_mass -1; vr_ragdoll_zombie_friction -1; "
                              "vr_ragdoll_zombie_joint_friction -1; vr_ragdoll_zombie_joint_stiffness -1; vr_ragdoll_zombie_limits -1; "
                              "vr_ragdoll_zombie_damping -1; vr_ragdoll_zombie_blast -1; vr_ragdoll_zombie_inherit -1")
            .help("The zombie's ragdoll as all monsters' (Ragdoll Settings)."),
    };
}

// Gibs and Corpses > Ragdoll Settings > Mummy: the mummy's own physics (vr_ragdoll_mummy_*), each one Global (the one
// for all monsters) or its own.
[[nodiscard]] za::Vector<Item> pageRagdollMummy()
{
    return {
        classSlider("Go Limp At", vr_ragdoll_mummy_start, 0.f, 1.f, 0.1f, "%.1f")
            .help("vr_ragdoll_mummy_start; Global: Go Limp At."),
        classSlider("Mass", vr_ragdoll_mummy_mass, 20.f, 200.f, 5.f, "%.0f kg").extend(0.f, 1000.f)
            .help("vr_ragdoll_mummy_mass; Global: Mass."),
        classSlider("Friction", vr_ragdoll_mummy_friction, 0.1f, 2.f, 0.1f, "%.1f").extend(0.f, 10.f)
            .help("vr_ragdoll_mummy_friction; Global: Friction."),
        classSlider("Joint Friction", vr_ragdoll_mummy_joint_friction, 0.f, 10.f, 0.5f, "%.1f N m").extend(0.f, 100.f)
            .help("vr_ragdoll_mummy_joint_friction; Global: Joint Friction."),
        classSlider("Joint Stiffness", vr_ragdoll_mummy_joint_stiffness, 0.f, 5.f, 0.25f, "%.2f Hz").extend(0.f, 30.f)
            .help("vr_ragdoll_mummy_joint_stiffness; Global: Joint Stiffness."),
        classSlider("Joint Limits", vr_ragdoll_mummy_limits, 0.25f, 1.5f, 0.05f, "%.2fx").extend(0.f, 3.f)
            .help("vr_ragdoll_mummy_limits; Global: Joint Limits."),
        classSlider("Limb Damping", vr_ragdoll_mummy_damping, 0.f, 3.f, 0.1f, "%.1f").extend(0.f, 20.f)
            .help("vr_ragdoll_mummy_damping; Global: Limb Damping."),
        classSlider("Blast Throw", vr_ragdoll_mummy_blast, 0.f, 5.f, 0.25f, "%.2fx").extend(0.f, 20.f)
            .help("vr_ragdoll_mummy_blast; Global: Blast Throw."),
        classSlider("Death Motion Kept", vr_ragdoll_mummy_inherit, 0.f, 2.f, 0.1f, "%.1fx")
            .help("vr_ragdoll_mummy_inherit; Global: Death Motion Kept."),
        command("All Global", "vr_ragdoll_mummy_start -1; vr_ragdoll_mummy_mass -1; vr_ragdoll_mummy_friction -1; "
                              "vr_ragdoll_mummy_joint_friction -1; vr_ragdoll_mummy_joint_stiffness -1; vr_ragdoll_mummy_limits -1; "
                              "vr_ragdoll_mummy_damping -1; vr_ragdoll_mummy_blast -1; vr_ragdoll_mummy_inherit -1")
            .help("The mummy's ragdoll as all monsters' (Ragdoll Settings)."),
    };
}

// Gibs and Corpses > Ragdoll Settings > Gremlin: the gremlin's own physics (vr_ragdoll_gremlin_*), each one Global (the one
// for all monsters) or its own.
[[nodiscard]] za::Vector<Item> pageRagdollGremlin()
{
    return {
        classSlider("Go Limp At", vr_ragdoll_gremlin_start, 0.f, 1.f, 0.1f, "%.1f")
            .help("vr_ragdoll_gremlin_start; Global: Go Limp At."),
        classSlider("Mass", vr_ragdoll_gremlin_mass, 20.f, 200.f, 5.f, "%.0f kg").extend(0.f, 1000.f)
            .help("vr_ragdoll_gremlin_mass; Global: Mass."),
        classSlider("Friction", vr_ragdoll_gremlin_friction, 0.1f, 2.f, 0.1f, "%.1f").extend(0.f, 10.f)
            .help("vr_ragdoll_gremlin_friction; Global: Friction."),
        classSlider("Joint Friction", vr_ragdoll_gremlin_joint_friction, 0.f, 10.f, 0.5f, "%.1f N m").extend(0.f, 100.f)
            .help("vr_ragdoll_gremlin_joint_friction; Global: Joint Friction."),
        classSlider("Joint Stiffness", vr_ragdoll_gremlin_joint_stiffness, 0.f, 5.f, 0.25f, "%.2f Hz").extend(0.f, 30.f)
            .help("vr_ragdoll_gremlin_joint_stiffness; Global: Joint Stiffness."),
        classSlider("Joint Limits", vr_ragdoll_gremlin_limits, 0.25f, 1.5f, 0.05f, "%.2fx").extend(0.f, 3.f)
            .help("vr_ragdoll_gremlin_limits; Global: Joint Limits."),
        classSlider("Limb Damping", vr_ragdoll_gremlin_damping, 0.f, 3.f, 0.1f, "%.1f").extend(0.f, 20.f)
            .help("vr_ragdoll_gremlin_damping; Global: Limb Damping."),
        classSlider("Blast Throw", vr_ragdoll_gremlin_blast, 0.f, 5.f, 0.25f, "%.2fx").extend(0.f, 20.f)
            .help("vr_ragdoll_gremlin_blast; Global: Blast Throw."),
        classSlider("Death Motion Kept", vr_ragdoll_gremlin_inherit, 0.f, 2.f, 0.1f, "%.1fx")
            .help("vr_ragdoll_gremlin_inherit; Global: Death Motion Kept."),
        command("All Global", "vr_ragdoll_gremlin_start -1; vr_ragdoll_gremlin_mass -1; vr_ragdoll_gremlin_friction -1; "
                              "vr_ragdoll_gremlin_joint_friction -1; vr_ragdoll_gremlin_joint_stiffness -1; vr_ragdoll_gremlin_limits -1; "
                              "vr_ragdoll_gremlin_damping -1; vr_ragdoll_gremlin_blast -1; vr_ragdoll_gremlin_inherit -1")
            .help("The gremlin's ragdoll as all monsters' (Ragdoll Settings)."),
    };
}

// Gibs and Corpses > Ragdoll Settings > Vore: the vore's own physics (vr_ragdoll_vore_*), each one Global (the one
// for all monsters) or its own.
[[nodiscard]] za::Vector<Item> pageRagdollVore()
{
    return {
        classSlider("Go Limp At", vr_ragdoll_vore_start, 0.f, 1.f, 0.1f, "%.1f")
            .help("vr_ragdoll_vore_start; Global: Go Limp At."),
        classSlider("Mass", vr_ragdoll_vore_mass, 20.f, 200.f, 5.f, "%.0f kg").extend(0.f, 1000.f)
            .help("vr_ragdoll_vore_mass; Global: Mass."),
        classSlider("Friction", vr_ragdoll_vore_friction, 0.1f, 2.f, 0.1f, "%.1f").extend(0.f, 10.f)
            .help("vr_ragdoll_vore_friction; Global: Friction."),
        classSlider("Joint Friction", vr_ragdoll_vore_joint_friction, 0.f, 10.f, 0.5f, "%.1f N m").extend(0.f, 100.f)
            .help("vr_ragdoll_vore_joint_friction; Global: Joint Friction."),
        classSlider("Joint Stiffness", vr_ragdoll_vore_joint_stiffness, 0.f, 5.f, 0.25f, "%.2f Hz").extend(0.f, 30.f)
            .help("vr_ragdoll_vore_joint_stiffness; Global: Joint Stiffness."),
        classSlider("Joint Limits", vr_ragdoll_vore_limits, 0.25f, 1.5f, 0.05f, "%.2fx").extend(0.f, 3.f)
            .help("vr_ragdoll_vore_limits; Global: Joint Limits."),
        classSlider("Limb Damping", vr_ragdoll_vore_damping, 0.f, 3.f, 0.1f, "%.1f").extend(0.f, 20.f)
            .help("vr_ragdoll_vore_damping; Global: Limb Damping."),
        classSlider("Blast Throw", vr_ragdoll_vore_blast, 0.f, 5.f, 0.25f, "%.2fx").extend(0.f, 20.f)
            .help("vr_ragdoll_vore_blast; Global: Blast Throw."),
        classSlider("Death Motion Kept", vr_ragdoll_vore_inherit, 0.f, 2.f, 0.1f, "%.1fx")
            .help("vr_ragdoll_vore_inherit; Global: Death Motion Kept."),
        command("All Global", "vr_ragdoll_vore_start -1; vr_ragdoll_vore_mass -1; vr_ragdoll_vore_friction -1; "
                              "vr_ragdoll_vore_joint_friction -1; vr_ragdoll_vore_joint_stiffness -1; vr_ragdoll_vore_limits -1; "
                              "vr_ragdoll_vore_damping -1; vr_ragdoll_vore_blast -1; vr_ragdoll_vore_inherit -1")
            .help("The vore's ragdoll as all monsters' (Ragdoll Settings)."),
    };
}

// Gibs and Corpses > Ragdoll Settings > Centroid: the centroid's own physics (vr_ragdoll_centroid_*), each one Global (the one
// for all monsters) or its own.
[[nodiscard]] za::Vector<Item> pageRagdollCentroid()
{
    return {
        classSlider("Go Limp At", vr_ragdoll_centroid_start, 0.f, 1.f, 0.1f, "%.1f")
            .help("vr_ragdoll_centroid_start; Global: Go Limp At."),
        classSlider("Mass", vr_ragdoll_centroid_mass, 20.f, 200.f, 5.f, "%.0f kg").extend(0.f, 1000.f)
            .help("vr_ragdoll_centroid_mass; Global: Mass."),
        classSlider("Friction", vr_ragdoll_centroid_friction, 0.1f, 2.f, 0.1f, "%.1f").extend(0.f, 10.f)
            .help("vr_ragdoll_centroid_friction; Global: Friction."),
        classSlider("Joint Friction", vr_ragdoll_centroid_joint_friction, 0.f, 10.f, 0.5f, "%.1f N m").extend(0.f, 100.f)
            .help("vr_ragdoll_centroid_joint_friction; Global: Joint Friction."),
        classSlider("Joint Stiffness", vr_ragdoll_centroid_joint_stiffness, 0.f, 5.f, 0.25f, "%.2f Hz").extend(0.f, 30.f)
            .help("vr_ragdoll_centroid_joint_stiffness; Global: Joint Stiffness."),
        classSlider("Joint Limits", vr_ragdoll_centroid_limits, 0.25f, 1.5f, 0.05f, "%.2fx").extend(0.f, 3.f)
            .help("vr_ragdoll_centroid_limits; Global: Joint Limits."),
        classSlider("Limb Damping", vr_ragdoll_centroid_damping, 0.f, 3.f, 0.1f, "%.1f").extend(0.f, 20.f)
            .help("vr_ragdoll_centroid_damping; Global: Limb Damping."),
        classSlider("Blast Throw", vr_ragdoll_centroid_blast, 0.f, 5.f, 0.25f, "%.2fx").extend(0.f, 20.f)
            .help("vr_ragdoll_centroid_blast; Global: Blast Throw."),
        classSlider("Death Motion Kept", vr_ragdoll_centroid_inherit, 0.f, 2.f, 0.1f, "%.1fx")
            .help("vr_ragdoll_centroid_inherit; Global: Death Motion Kept."),
        command("All Global", "vr_ragdoll_centroid_start -1; vr_ragdoll_centroid_mass -1; vr_ragdoll_centroid_friction -1; "
                              "vr_ragdoll_centroid_joint_friction -1; vr_ragdoll_centroid_joint_stiffness -1; vr_ragdoll_centroid_limits -1; "
                              "vr_ragdoll_centroid_damping -1; vr_ragdoll_centroid_blast -1; vr_ragdoll_centroid_inherit -1")
            .help("The centroid's ragdoll as all monsters' (Ragdoll Settings)."),
    };
}

// Gibs and Corpses > Ragdoll Settings > Shambler: the shambler's own physics (vr_ragdoll_shambler_*), each one Global (the one
// for all monsters) or its own.
[[nodiscard]] za::Vector<Item> pageRagdollShambler()
{
    return {
        classSlider("Go Limp At", vr_ragdoll_shambler_start, 0.f, 1.f, 0.1f, "%.1f")
            .help("vr_ragdoll_shambler_start; Global: Go Limp At."),
        classSlider("Mass", vr_ragdoll_shambler_mass, 20.f, 200.f, 5.f, "%.0f kg").extend(0.f, 1000.f)
            .help("vr_ragdoll_shambler_mass; Global: Mass."),
        classSlider("Friction", vr_ragdoll_shambler_friction, 0.1f, 2.f, 0.1f, "%.1f").extend(0.f, 10.f)
            .help("vr_ragdoll_shambler_friction; Global: Friction."),
        classSlider("Joint Friction", vr_ragdoll_shambler_joint_friction, 0.f, 10.f, 0.5f, "%.1f N m").extend(0.f, 100.f)
            .help("vr_ragdoll_shambler_joint_friction; Global: Joint Friction."),
        classSlider("Joint Stiffness", vr_ragdoll_shambler_joint_stiffness, 0.f, 5.f, 0.25f, "%.2f Hz").extend(0.f, 30.f)
            .help("vr_ragdoll_shambler_joint_stiffness; Global: Joint Stiffness."),
        classSlider("Joint Limits", vr_ragdoll_shambler_limits, 0.25f, 1.5f, 0.05f, "%.2fx").extend(0.f, 3.f)
            .help("vr_ragdoll_shambler_limits; Global: Joint Limits."),
        classSlider("Limb Damping", vr_ragdoll_shambler_damping, 0.f, 3.f, 0.1f, "%.1f").extend(0.f, 20.f)
            .help("vr_ragdoll_shambler_damping; Global: Limb Damping."),
        classSlider("Blast Throw", vr_ragdoll_shambler_blast, 0.f, 5.f, 0.25f, "%.2fx").extend(0.f, 20.f)
            .help("vr_ragdoll_shambler_blast; Global: Blast Throw."),
        classSlider("Death Motion Kept", vr_ragdoll_shambler_inherit, 0.f, 2.f, 0.1f, "%.1fx")
            .help("vr_ragdoll_shambler_inherit; Global: Death Motion Kept."),
        command("All Global", "vr_ragdoll_shambler_start -1; vr_ragdoll_shambler_mass -1; vr_ragdoll_shambler_friction -1; "
                              "vr_ragdoll_shambler_joint_friction -1; vr_ragdoll_shambler_joint_stiffness -1; vr_ragdoll_shambler_limits -1; "
                              "vr_ragdoll_shambler_damping -1; vr_ragdoll_shambler_blast -1; vr_ragdoll_shambler_inherit -1")
            .help("The shambler's ragdoll as all monsters' (Ragdoll Settings)."),
    };
}

// Gibs and Corpses > Ragdoll Settings > Fiend: the fiend's own physics (vr_ragdoll_demon_*), each one Global (the one
// for all monsters) or its own.
[[nodiscard]] za::Vector<Item> pageRagdollFiend()
{
    return {
        classSlider("Go Limp At", vr_ragdoll_demon_start, 0.f, 1.f, 0.1f, "%.1f")
            .help("vr_ragdoll_demon_start; Global: Go Limp At."),
        classSlider("Mass", vr_ragdoll_demon_mass, 20.f, 200.f, 5.f, "%.0f kg").extend(0.f, 1000.f)
            .help("vr_ragdoll_demon_mass; Global: Mass."),
        classSlider("Friction", vr_ragdoll_demon_friction, 0.1f, 2.f, 0.1f, "%.1f").extend(0.f, 10.f)
            .help("vr_ragdoll_demon_friction; Global: Friction."),
        classSlider("Joint Friction", vr_ragdoll_demon_joint_friction, 0.f, 10.f, 0.5f, "%.1f N m").extend(0.f, 100.f)
            .help("vr_ragdoll_demon_joint_friction; Global: Joint Friction."),
        classSlider("Joint Stiffness", vr_ragdoll_demon_joint_stiffness, 0.f, 5.f, 0.25f, "%.2f Hz").extend(0.f, 30.f)
            .help("vr_ragdoll_demon_joint_stiffness; Global: Joint Stiffness."),
        classSlider("Joint Limits", vr_ragdoll_demon_limits, 0.25f, 1.5f, 0.05f, "%.2fx").extend(0.f, 3.f)
            .help("vr_ragdoll_demon_limits; Global: Joint Limits."),
        classSlider("Limb Damping", vr_ragdoll_demon_damping, 0.f, 3.f, 0.1f, "%.1f").extend(0.f, 20.f)
            .help("vr_ragdoll_demon_damping; Global: Limb Damping."),
        classSlider("Blast Throw", vr_ragdoll_demon_blast, 0.f, 5.f, 0.25f, "%.2fx").extend(0.f, 20.f)
            .help("vr_ragdoll_demon_blast; Global: Blast Throw."),
        classSlider("Death Motion Kept", vr_ragdoll_demon_inherit, 0.f, 2.f, 0.1f, "%.1fx")
            .help("vr_ragdoll_demon_inherit; Global: Death Motion Kept."),
        command("All Global", "vr_ragdoll_demon_start -1; vr_ragdoll_demon_mass -1; vr_ragdoll_demon_friction -1; "
                              "vr_ragdoll_demon_joint_friction -1; vr_ragdoll_demon_joint_stiffness -1; vr_ragdoll_demon_limits -1; "
                              "vr_ragdoll_demon_damping -1; vr_ragdoll_demon_blast -1; vr_ragdoll_demon_inherit -1")
            .help("The fiend's ragdoll as all monsters' (Ragdoll Settings)."),
    };
}
