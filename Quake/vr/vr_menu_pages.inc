// vr_menu_pages.inc -- the old Quake VR settings pages (Quake VR's menu.cpp, "Quake VR -
// Settings"), under VR Settings > Advanced VR Options: every setting that still exists in the
// port, with its old range and help. Included by vr_menu.cpp. Settings that do nothing any more
// (the menu's anchoring, MSAA, the old force grab's range and power, the old floating torso,
// voice chat) are left out.

// Old Quake VR settings: "Menu Settings".
[[nodiscard]] std::vector<Item> pageMenuSettings()
{
    return {
        slider("Menu Scale", "vr_menu_scale", 0.05f, 0.6f, 0.01f, "%.2f").help("Scale multiplier for the menu."),
        slider("Menu Distance", "vr_menu_distance", 24.f, 256.f, 1.f, "%.0f").help("Distance of the menu from the anchor point."),
    };
}

// Old Quake VR settings: "Crosshair Settings".
[[nodiscard]] std::vector<Item> pageCrosshairSettings()
{
    return {
        cycle("Crosshair", "vr_crosshair", {{0.f, "Off"}, {1.f, "Dot"}, {2.f, "Laser"}, {3.f, "Soft laser"}}),
        slider("Crosshair Depth", "vr_crosshair_depth", 0.f, 4096.f, 16.f, "%.0f").help("When zero, the crosshair will be projected to the closest wall. Otherwise, the crosshair will be projected at a fixed depth."),
        slider("Crosshair Size", "vr_crosshair_size", 0.f, 32.f, 0.5f, "%.1f").help("Size of the crosshair."),
        slider("Crosshair Alpha", "vr_crosshair_alpha", 0.f, 1.f, 0.05f, "%.2f").help("Transparency of the crosshair. '1' means fully visible, '0' means fully transparent."),
        slider("Crosshair Z Offset", "vr_crosshairy", -10.f, 10.f, 0.05f, "%.2f").help("Z offset applied to the crosshair. Does not affect aiming. Leave to zero unless you changed other weapon options and your aim is off."),
    };
}

// Old Quake VR settings: "Particle Settings".
[[nodiscard]] std::vector<Item> pageParticleSettings()
{
    return {
        toggle("Particle Effects", "r_particles").help("Enable or disable all particle effects."),
        toggle("Quake VR Particles", "vr_particles")
            .help("Textured smoke, sparks, blood and explosions for Quake VR's effects. Off: Quake's own dots."),
        slider("Particle Multiplier", "vr_particle_mult", 0.25f, 4.f, 0.25f, "%.2f")
            .help("How many particles Quake VR's effects make."),
    };
}

// Old Quake VR settings: "Locomotion Settings".
[[nodiscard]] std::vector<Item> pageLocomotionSettings()
{
    return {
        cycle("Movement Mode", "vr_movement_mode", {{1.f, "Follow Head"}, {0.f, "Follow Off Hand"}}).help("Movement options for smooth locomotion."),
        slider("Deadzone", "vr_deadzone", 0.f, 50.f, 5.f, "%.0f%%").help("Controller thumbstick deadzone."),
        toggle("Enable Joystick Turn", "vr_enable_joystick_turn").help("Allows turning using the controller. Keep enabled unless you want to force yourself to turn in real life."),
        slider("Turn", "vr_snap_turn", 0.f, 90.f, 5.f, "%.0f"),
        slider("Turn Speed", "vr_turn_speed", 0.f, 10.f, 0.25f, "%.1f").help("How quickly you turn with the joystick."),
        toggle("Teleportation", "vr_teleport_enabled").help("Enables or disables teleportation. A controller binding is required to use teleportation."),
        slider("Teleport Range", "vr_teleport_range", 100.f, 800.f, 10.f, "%.0f").help("How far you can teleport."),
        toggle("Roomscale Jump", "vr_roomscale_jump").help("Jumping in the room jumps in the game."),
        slider("Jump Threshold", "vr_roomscale_jump_threshold", 0.3f, 2.f, 0.05f, "%.2f m/s")
            .help("How fast the head must rise above the standing height."),
        slider("Roomscale Move Mult.", "vr_roomscale_move_mult", 0.25f, 5.f, 0.25f, "%.1f").help("Real movement to in-game movement. Increase to cover more ground, e.g. with teleportation or little room."),
        slider("Movement Speed", "cl_forwardspeed", 100.f, 400.f, 25.f, "%.0f").help("Walking speed (running is this times the running multiplier)."),
        cycle("Default Speed", "cl_alwaysrun", {{1.f, "Run"}, {0.f, "Walk"}}).help("The speed button switches to the other."),
        slider("Running Mult.", "cl_movespeedkey", 1.f, 3.f, 0.05f, "%.2f").help("How much faster running is than walking."),
    };
}

// Old Quake VR settings: "Hand/Gun Calibration".
[[nodiscard]] std::vector<Item> pageHandGunCalibration()
{
    return {
        slider("Gun Angle", "vr_gunangle", -180.f, 180.f, 0.75f, "%.1f").help("Pitch offset for the guns/hands. Affects aiming. Change if your controller is not 'aligned' to your gun."),
        slider("Gun Model Pitch", "vr_gunmodelpitch", -90.f, 90.f, 0.25f, "%.1f").help("Pitch offset for gun models. Does NOT affect aiming."),
        slider("Gun Model Scale", "vr_gunmodelscale", 0.1f, 2.f, 0.05f, "%.2f").help("Scale multiplier for gun models. Does NOT affect aiming, but does affect muzzle position (where bullets and projectiles come from)."),
        slider("Gun Model Z Offset", "vr_gunmodely", -5.f, 5.f, 0.1f, "%.1f").help("Z offset for gun models. Does NOT affect aiming."),
        slider("Gun Yaw", "vr_gunyaw", -90.f, 90.f, 0.25f, "%.1f").help("Yaw offset for the guns/hands. Affects aiming. Change if your controller is not 'aligned' to your gun."),
        slider("Gun Z Offset", "vr_gun_z_offset", -30.f, 30.f, 0.25f, "%.1f").help("Z offset for guns/hands. Affects aiming."),
        slider("Off-Hand Pitch", "vr_offhandpitch", -90.f, 90.f, 0.25f, "%.1f").help("Pitch offset for the guns/hands. Only for off-hand. Affects aiming."),
        slider("Off-Hand Yaw", "vr_offhandyaw", -90.f, 90.f, 0.25f, "%.1f").help("Yaw offset for the guns/hands. Only for off-hand. Affects aiming."),
        slider("Finger Grip Bias", "vr_finger_grip_bias", 0.f, 1.f, 0.05f, "%.2f"),
        toggle("Auto close thumb", "vr_finger_auto_close_thumb"),
    };
}

// Old Quake VR settings: "Player Calibration".
[[nodiscard]] std::vector<Item> pagePlayerCalibration()
{
    return {
        slider("World Scale", "vr_world_scale", 0.5f, 2.f, 0.05f, "%.2f").help("Scale of the world. Affects movement, height, and hand positions."),
        slider("Floor Offset", "vr_floor_offset", -200.f, 200.f, 2.5f, "%.0f").help("Offset from the floor. Change if you feel like you're floating or stuck in the floor."),
    };
}

// Old Quake VR settings: "Melee Settings".
[[nodiscard]] std::vector<Item> pageMeleeSettings()
{
    return {
        slider("Swing Speed", "vr_melee_speed", 1.f, 8.f, 0.25f, "%.2f m/s").help("How fast a hand must swing to hit. Damage grows with the speed beyond it, up to three times."),
        slider("Punch Damage Mult.", "vr_melee_punch_mult", 1.f, 3.f, 0.05f, "%.2f").help("Damage of a punch (the fist moving knuckles first) over any other swing."),
        slider("Damage Multiplier", "vr_melee_dmg_multiplier", 0.25f, 15.f, 0.25f, "%.1f").help("Damage multiplier for melee attacks."),
        slider("Range Multiplier", "vr_melee_range_multiplier", 0.25f, 15.f, 0.25f, "%.1f").help("Range multiplier for melee attacks."),
        cycle("Melee Bloodlust", "vr_melee_bloodlust", {{0.f, "On"}, {1.f, "Off"}}).help("When enabled, restores the player's health when performing melee attacks on enemies."),
        slider("Melee Bloodlust Mult.", "vr_melee_bloodlust_mult", 0.05f, 5.f, 0.05f, "%.2f").help("Multiplier for melee bloodlust health restoration."),
    };
}

// Old Quake VR settings: "Aiming Settings".
[[nodiscard]] std::vector<Item> pageAimingSettings()
{
    return {
        cycle("2H Aiming", "vr_2h_mode", {{0.f, "Disabled"}, {1.f, "Basic"}, {2.f, "Virtual Stock"}}).help("'Basic' aims between the two hands; 'Virtual Stock' also uses the shoulder when the weapon is close to it (see Hotspots)."),
        slider("2H Aiming Threshold", "vr_2h_angle_threshold", -1.f, 1.f, 0.05f, "%.2f").help("How far out of line the hands may be before two-handed aiming stops. Higher is stricter; -1 never stops."),
        slider("2H Stock Factor", "vr_2h_virtual_stock_factor", 0.f, 1.f, 0.05f, "%.2f").help("How much the virtual stock matters in the final angle calculation for two-handed aiming. Decrease if you want your hands to matter more."),
        toggle("Weighted Weapon Move", "vr_wpn_pos_weight").help("Enable weight simulation for weapon movement."),
        slider("W. Weapon Move Off.", "vr_wpn_pos_weight_offset", 0.05f, 1.f, 0.05f, "%.2f"),
        slider("W. Weapon Move Mult", "vr_wpn_pos_weight_mult", -5.f, 5.f, 0.1f, "%.1f"),
        slider("W. Move 2H Help Off.", "vr_wpn_pos_weight_2h_help_offset", 0.05f, 1.f, 0.05f, "%.2f"),
        slider("W. Move 2H Help Mult", "vr_wpn_pos_weight_2h_help_mult", -5.f, 5.f, 0.1f, "%.1f"),
        toggle("Weighted Weapon Turn", "vr_wpn_dir_weight").help("Enable weight simulation for weapon turning."),
        slider("W. Weapon Turn Off.", "vr_wpn_dir_weight_offset", 0.05f, 1.f, 0.05f, "%.2f"),
        slider("W. Weapon Turn Mult", "vr_wpn_dir_weight_mult", -5.f, 5.f, 0.1f, "%.1f"),
        slider("W. Turn 2H Help Off.", "vr_wpn_dir_weight_2h_help_offset", 0.05f, 1.f, 0.05f, "%.2f"),
        slider("W. Turn 2H Help Mult", "vr_wpn_dir_weight_2h_help_mult", -5.f, 5.f, 0.1f, "%.1f"),
    };
}

// Old Quake VR settings: "Immersion Settings".
[[nodiscard]] std::vector<Item> pageImmersionSettings()
{
    return {
        toggle("Weapon Text", "vr_show_weapon_text").help("Show floating ammunition text attached to weapons"),
        toggle("Positional Damage", "vr_positional_damage").help("Enables positional damage multipliers for headshots and leg shots. Only applies to humanoid enemies."),
        toggle("Body Picks Up Items", "vr_body_interactions").help("Pick up items and level weapons by walking over them. Thrown and dropped weapons still need your hands."),
        toggle("Disable Haptics", "vr_disablehaptics").help("Disable all haptics (vibration)."),
        cycle("Weapon Mode", "vr_holster_mode", {{0.f, "Immersive"}, {1.f, "Quick Slots"}}).help("Immersive: a holster holds one weapon, and taking it out empties it. Quick Slots: holsters never empty (with weapon cycling)."),
        cycle("Weapon Reloading Mode", "vr_reload_mode", {{0.f, "None"}, {1.f, "All Holsters"}, {2.f, "Hip Holsters"}}).help("Enables weapon reloading mechanics. Weapons can be reloaded by moving them towards an holster. Reloading only works with 'immersive' weapon mode."),
        cycle("Holster Haptics", "vr_holster_haptics", {{0.f, "Off"}, {1.f, "Continuous"}, {2.f, "Once"}}).help("Haptic feedback when hovering a usable holster slot."),
        cycle("Weapon Cycle Mode", "vr_weapon_cycle_mode", {{0.f, "Off"}, {1.f, "On"}}).help("Allows or disallows cycling weapons using controller buttons."),
        cycle("Weapon Throw Mode", "vr_weapon_throw_mode", {{0.f, "Immersive"}, {1.f, "Vanish on Hit"}, {2.f, "Discard"}}).help("Immersive: thrown weapons can be picked back up. Vanish on Hit and Discard avoid piles of weapons with Quick Slots."),
        slider("Throw Damage Mult.", "vr_weapon_throw_damage_mult", 0.05f, 5.f, 0.05f, "%.2f").help("Multiplier for weapon throw damage."),
        slider("Throw Speed Mult.", "vr_weapon_throw_velocity_mult", 0.05f, 5.f, 0.05f, "%.2f").help("Multiplier for weapon throw velocity."),
        toggle("Dropped Wpn Particles", "vr_weapondrop_particles").help("Enable particle effects for weapons dropped/thrown on the ground."),
        cycle("Enemy Weapon Drops", "vr_enemy_drops", {{0.f, "When Eligible"}, {1.f, "Always"}, {2.f, "Disabled"}}).help("Controls random enemy weapon drops. 'Eligible' means that the player has obtained a weapon before through a level weapon pickup."),
        slider("Enemy Drops Chance", "vr_enemy_drops_chance_mult", 0.05f, 5.f, 0.05f, "%.2f").help("Multiplier for enemy weapon drops."),
        cycle("Ammo Box Weapon Drops", "vr_ammobox_drops", {{0.f, "When Eligible"}, {1.f, "Always"}, {2.f, "Disabled"}}).help("Controls random ammo box weapon drops. 'Eligible' means that the player has obtained a weapon before through a level weapon pickup."),
        slider("Ammo Box Drops Chance", "vr_ammobox_drops_chance_mult", 0.05f, 5.f, 0.05f, "%.2f").help("Multiplier for ammo box weapon drops."),
        toggle("Force Grab", "vr_forcegrab_mode").help("Point an empty hand at an object, pull the trigger, flick the hand: it flies to you. Grip as it arrives to catch it."),
        toggle("Force Grab Particles", "vr_forcegrab_eligible_particles").help("Sparkles on the object an empty hand points at, that it can pull."),
        toggle("Force Grab Haptics", "vr_forcegrab_eligible_haptics").help("A tick in the hand when it points at a new object it can pull."),
    };
}

// Old Quake VR settings: "Graphical Settings".
[[nodiscard]] std::vector<Item> pageGraphicalSettings()
{
    return {
        cycle("Player Shadows", "vr_player_shadows", {{0.f, "Off"}, {1.f, "Hands"}, {2.f, "Body"}, {3.f, "Both"}}),
        toggle("Monster and Item Shadows", "vr_entity_shadows").help("Soft shadows under monsters and items, away from their light."),
        toggle("Model Lighting", "vr_model_lighting").help("Monsters, items and weapons are shaded from the map's lights, not one fixed direction."),
        toggle("Relit Maps", "vr_relit_maps").help("Maps re-lit with shadows, ambient occlusion and colour, if made (Misc/quakevr/relight_maps.py). Next map."),
        cycle("Anti-aliasing", "vid_fsaa", {{0.f, "Off"}, {2.f, "2x"}, {4.f, "4x"}, {8.f, "8x"}}).help("Multisampling: smoother edges, some GPU time."),
    };
}

// Old Quake VR settings: "Hud Configuration".
[[nodiscard]] std::vector<Item> pageHudConfiguration()
{
    return {
        cycle("HUD", "vr_hud_mode", {{1.f, "Wrist gadget"}, {0.f, "Status bar"}}).help("The settings below apply to the status bar."),
        cycle("Status Bar Mode", "vr_sbar_mode", {{0.f, "Main Hand"}, {1.f, "Off Hand"}}).help("The offsets below apply to the off hand."),
        slider("HUD Scale", "vr_hud_scale", 0.01f, 0.1f, 0.005f, "%.3f"),
        slider("Offset X", "vr_sbar_offset_x", -200.f, 200.f, 1.f, "%.0f"),
        slider("Offset Y", "vr_sbar_offset_y", -200.f, 200.f, 1.f, "%.0f"),
        slider("Offset Z", "vr_sbar_offset_z", -200.f, 200.f, 1.f, "%.0f"),
        slider("Roll", "vr_sbar_offset_roll", -3.14f, 3.14f, 0.05f, "%.2f rad"),
        slider("Pitch", "vr_sbar_offset_pitch", -3.14f, 3.14f, 0.05f, "%.2f rad"),
        slider("Yaw", "vr_sbar_offset_yaw", -3.14f, 3.14f, 0.05f, "%.2f rad"),
    };
}

// Old Quake VR settings: "Hotspot Settings".
[[nodiscard]] std::vector<Item> pageHotspotSettings()
{
    return {
        toggle("Show Virtual Stock", "vr_show_virtual_stock"),
        slider("Shoulder X", "vr_shoulder_offset_x", -50.f, 50.f, 0.5f, "%.1f"),
        slider("Shoulder Y", "vr_shoulder_offset_y", -50.f, 50.f, 0.5f, "%.1f"),
        slider("Shoulder Z", "vr_shoulder_offset_z", -50.f, 50.f, 0.5f, "%.1f"),
        slider("Virtual Stock Thresh.", "vr_virtual_stock_thresh", 0.f, 30.f, 0.1f, "%.1f"),
        toggle("Show Shoulder Holst.", "vr_show_shoulder_holsters"),
        slider("Shoulder X", "vr_shoulder_holster_offset_x", -50.f, 50.f, 0.5f, "%.1f"),
        slider("Shoulder Y", "vr_shoulder_holster_offset_y", -50.f, 50.f, 0.5f, "%.1f"),
        slider("Shoulder Z", "vr_shoulder_holster_offset_z", -50.f, 50.f, 0.5f, "%.1f"),
        slider("Shoulder Threshold", "vr_shoulder_holster_thresh", 0.f, 30.f, 0.1f, "%.1f"),
        toggle("Show Hip Holsters", "vr_show_hip_holsters"),
        slider("Hip X", "vr_hip_offset_x", -50.f, 50.f, 0.5f, "%.1f"),
        slider("Hip Y", "vr_hip_offset_y", -50.f, 50.f, 0.5f, "%.1f"),
        slider("Hip Z", "vr_hip_offset_z", -50.f, 50.f, 0.5f, "%.1f"),
        slider("Hip Threshold", "vr_hip_holster_thresh", 0.f, 30.f, 0.1f, "%.1f"),
        toggle("Show Upper Holsters", "vr_show_upper_holsters"),
        slider("Upper X", "vr_upper_holster_offset_x", -50.f, 50.f, 0.5f, "%.1f"),
        slider("Upper Y", "vr_upper_holster_offset_y", -50.f, 50.f, 0.5f, "%.1f"),
        slider("Upper Z", "vr_upper_holster_offset_z", -50.f, 50.f, 0.5f, "%.1f"),
        slider("Upper Threshold", "vr_upper_holster_thresh", 0.f, 30.f, 0.1f, "%.1f"),
        toggle("Show Holster Slots", "vr_leg_holster_model_enabled"),
        slider("Holster Slot Scale", "vr_leg_holster_model_scale", 0.1f, 2.f, 0.05f, "%.2f"),
        slider("Holster Slot X", "vr_leg_holster_model_x_offset", -100.f, 100.f, 0.5f, "%.1f"),
        slider("Holster Slot Y", "vr_leg_holster_model_y_offset", -100.f, 100.f, 0.5f, "%.1f"),
        slider("Holster Slot Z", "vr_leg_holster_model_z_offset", -100.f, 100.f, 0.5f, "%.1f"),
    };
}

// Old Quake VR settings: "Transparency Options".
[[nodiscard]] std::vector<Item> pageTransparencyOptions()
{
    return {
        toggle("(!) No Vis", "r_novis").help("NOT RECOMMENDED: water transparency in maps without vis data, by drawing the whole map every frame. Vispatch maps instead."),
        slider("Water Alpha", "r_wateralpha", 0.f, 1.f, 0.1f, "%.1f"),
        slider("Lava Alpha", "r_lavaalpha", 0.f, 1.f, 0.1f, "%.1f"),
        slider("Tele Alpha", "r_telealpha", 0.f, 1.f, 0.1f, "%.1f"),
        slider("Slime Alpha", "r_slimealpha", 0.f, 1.f, 0.1f, "%.1f"),
    };
}

