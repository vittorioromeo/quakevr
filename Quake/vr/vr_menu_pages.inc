// vr_menu_pages.inc -- the old Quake VR settings pages (Quake VR's menu.cpp, "Quake VR -
// Settings"), under VR Settings > Advanced VR Options: every setting that still exists in the
// port, with its old range and help. Included by vr_menu.cpp. Settings that do nothing any more
// (the menu's anchoring, MSAA, the old force grab's range and power, the old floating torso,
// voice chat) are left out.

// Old Quake VR settings: "Menu Settings".
[[nodiscard]] std::vector<Item> pageMenuSettings()
{
    return {
        slider("Menu Scale", "vr_menu_scale", 0.05f, 0.6f, 0.01f, "%.2f").help("Scale multiplier for the menu: the size of one of its pixels, in world units."),
        slider("Menu Distance", "vr_menu_distance", 24.f, 256.f, 1.f, "%.0f").help("Distance of the menu from the anchor point."),
        toggle("VR Menu Style", "vr_menu_vr_style").help("The menus fill the panel with their rows spaced out, with modern sliders and switches, and a laser from your hand points at them: trigger to click and drag."),
        slider("Row Spacing", "vr_menu_spacing", 1.f, 2.f, 0.05f, "%.2fx").help("With the VR menu style: how far apart the menus' rows are, over Quake's."),
        toggle("Live Preview", "ui_live_preview").help("Settings pages show their changes in the game as you make them, and in single player the game keeps running under them (paused while a monster is after you)."),
        toggle("Reopen Where Left", "vr_menu_remember").help("After Back to Game (the button at the menu's top left, or holding the menu button), the menu opens again on the page you left."),
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
        slider("Lean", "vr_lean_radius", 0.f, 14.f, 1.f, "%.0f units").help("How far your head may lean off the middle of your body's box before the body follows: get your face near walls and over railings. 0: the body always under the head."),
        slider("Lean Recentre", "vr_lean_recenter", 0.f, 2.f, 0.1f, "%.1f m/s").help("How fast the body slides back under your head when it can (not into walls, not over ledges)."),
        toggle("Roomscale Jump", "vr_roomscale_jump").help("Jumping in the room jumps in the game."),
        slider("Jump Threshold", "vr_roomscale_jump_threshold", 0.3f, 2.f, 0.05f, "%.2f m/s")
            .help("How fast the head must rise above the standing height."),
        slider("Roomscale Move Mult.", "vr_roomscale_move_mult", 0.25f, 5.f, 0.25f, "%.1f").help("Real movement to in-game movement. Increase to cover more ground, e.g. with teleportation or little room."),
        slider("Movement Speed", "cl_forwardspeed", 100.f, 400.f, 25.f, "%.0f").help("Walking speed (running is this times the running multiplier)."),
        cycle("Default Speed", "cl_alwaysrun", {{1.f, "Run"}, {0.f, "Walk"}}).help("The speed button switches to the other."),
        slider("Running Mult.", "cl_movespeedkey", 1.f, 3.f, 0.05f, "%.2f").help("How much faster running is than walking."),
        toggle("Swimming", "vr_swim").help("In water the stick slows and strokes of your hands move you (more on the Swimming page)."),
        toggle("Ledge Grab (Experimental)", "vr_climb").help("Grip a ledge with an empty hand to hang from it; pull down to climb onto it. Only ledges, not walls."),
        slider("Lowest Ledge", "vr_climb_min_height", 0.f, 64.f, 2.f, "%.0f units").help("How far above your feet a ledge must be to grab it."),
        slider("Ledge Fling", "vr_climb_fling", 0.f, 2.f, 0.1f, "%.1f").help("How much letting go of a ledge flings you with your hands' motion."),
    };
}

// Swimming (vr_physics.cpp, "Swimming"): the stick in water, the strokes, and telling a stroke from
// the return of the arms for the next one.
void resetSwimming()
{
    static const char* const names[] = {"vr_swim", "vr_swim_shallow_speed", "vr_swim_wade_speed",
        "vr_swim_stick_speed", "vr_swim_stroke", "vr_swim_stroke_min", "vr_swim_palm", "vr_swim_stroke_assist",
        "vr_swim_max_speed", "vr_swim_look", "vr_swim_recovery", "vr_swim_glide", "vr_swim_power_threshold",
        "vr_swim_power_knee", "vr_swim_power_whole", "vr_swim_intent_memory", "vr_swim_reverse_damp", "vr_swim_reverse_speed",
        "vr_swim_flat_exp", "vr_swim_speed_exp", "vr_swim_palm_dir"};
    for(const char* name : names)
    {
        if(cvar_t* var = Cvar_FindVar(name))
        {
            Cvar_SetQuick(var, var->default_string);
        }
    }
}

[[nodiscard]] std::vector<Item> pageSwimSettings()
{
    return {
        toggle("Swimming", "vr_swim").help("In water the stick slows and strokes of your hands move you."),
        slider("Stick in Shallow Water", "vr_swim_shallow_speed", 0.f, 1.f, 0.05f, "%.2f").help("The stick's speed with your feet in water."),
        slider("Stick Wading", "vr_swim_wade_speed", 0.f, 1.f, 0.05f, "%.2f").help("Waist deep, standing on the bottom."),
        slider("Stick Swimming", "vr_swim_stick_speed", 0.f, 1.f, 0.05f, "%.2f").help("Under water or off the bottom: swim with your hands."),
        header("Strokes"),
        slider("Stroke Strength", "vr_swim_stroke", 0.f, 30.f, 1.f, "%.0f").help("How far a stroke of your hand moves you."),
        slider("Speed Curve", "vr_swim_speed_exp", 1.f, 3.f, 0.25f, "%.2f")
            .help("How the push grows with the hand's speed. 1: in proportion. 2: as water's drag, so a brisk stroke outweighs a slower return by far. 3: even more."),
        slider("Palm Matters", "vr_swim_palm", 0.f, 1.f, 0.1f, "%.1f").help("How much more a flat palm (or back of the hand) pushes than a hand meeting the water at an angle."),
        slider("Palm Sharpness", "vr_swim_flat_exp", 0.5f, 4.f, 0.25f, "%.2f")
            .help("How flat the palm must be to push fully. 1: pushing falls off evenly as the hand turns; 2-3: only a really flat palm pushes well."),
        slider("Recovery Push", "vr_swim_recovery", 0.f, 1.f, 0.05f, "%.2f").help("How much a hand going edge first (bringing it back for the next stroke) pushes."),
        slider("Push Along Palm", "vr_swim_palm_dir", 0.f, 1.f, 0.1f, "%.1f")
            .help("0: you go against the hand's motion. 1: away from where the palm faces, so sweeping the hands in with palms facing each other pushes sideways (the two cancel), not back."),
        slider("Slowest Push", "vr_swim_stroke_min", 0.f, 1.f, 0.05f, "%.2f m/s").help("A hand slower than this pushes nothing at all (tracking jitter, drifting hands)."),
        header("Stroke or Return?"),
        slider("Intent Threshold", "vr_swim_power_threshold", 0.f, 3.f, 0.1f, "%.1f m/s")
            .help("A stroke moves you only if the hand's top speed during it passes this: a brisk stroke swims, a relaxed return of the arms does nothing. 0: every motion pushes."),
        slider("Intent Fade-In", "vr_swim_power_knee", 0.f, 2.f, 0.1f, "%.1f m/s")
            .help("Above the threshold, strokes fade in to full power over this much more speed. 0: all or nothing."),
        toggle("Whole Stroke Counts", "vr_swim_power_whole")
            .help("On: once a stroke passes the threshold, all of it pushes, its slow start too. Off: only from the moment it passes."),
        slider("Stroke Memory", "vr_swim_intent_memory", 0.f, 4.f, 0.25f, "%.2f s")
            .help("How long each hand remembers the way of its last full stroke, to tell the next return from a stroke back the other way. 0 off."),
        slider("Return Damping", "vr_swim_reverse_damp", 0.f, 1.f, 0.05f, "%.2f")
            .help("How much a stroke against the remembered one is damped when it is slower than the Reverse Stroke Speed below (0.3 slower or more: fully). A deliberate stroke back, as brisk, is not damped."),
        slider("Reverse Stroke Speed", "vr_swim_reverse_speed", 0.5f, 2.f, 0.05f, "%.2fx")
            .help("How fast, over the stroke it reverses, a stroke the other way must be to count fully. 1: as fast. 1.2: 20% faster (returns never swim you back). 0.8: a bit slower is enough."),
        header("Direction and Speed"),
        slider("Swim Where You Look", "vr_swim_look", 0.f, 1.f, 0.1f, "%.1f").help("You swim where you look (look up to rise): this much of a push sideways of it is lost."),
        slider("Stroke Steering", "vr_swim_stroke_assist", 0.f, 1.f, 0.1f, "%.1f").help("Strokes push this much more towards where the stick points, and as much less against it."),
        slider("Glide", "vr_swim_glide", 0.f, 1.f, 0.1f, "%.1f").help("How far you glide between strokes (less of the water's drag)."),
        slider("Swim Top Speed", "vr_swim_max_speed", 100.f, 800.f, 25.f, "%.0f"),
        action("Reset Swimming to Defaults", resetSwimming).help("Every setting on this page back to its default."),
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
        slider("Swing Speed", "vr_melee_speed", 1.f, 8.f, 0.25f, "%.2f m/s").help("The least speed of the wrist in a blow (a swung weapon: a quarter more): a weak hit. Damage grows with the speed: a good punch is 1.6 times this, a good swing 1.8 times."),
        slider("Blow Distance", "vr_melee_distance", 0.05f, 0.5f, 0.05f, "%.2f m").help("How far the wrist must travel in a blow to hit: wrist flicks and wiggles don't. Wider blows (up to twice this) hit a little harder."),
        slider("Punch Damage Mult.", "vr_melee_punch_mult", 1.f, 3.f, 0.05f, "%.2f").help("Damage of a straight punch (where you face) over a slap; overhead blows land between."),
        slider("Damage Multiplier", "vr_melee_dmg_multiplier", 0.25f, 15.f, 0.25f, "%.1f").help("Damage multiplier for melee attacks."),
        slider("Range Multiplier", "vr_melee_range_multiplier", 0.25f, 15.f, 0.25f, "%.1f").help("Range multiplier for melee attacks."),
        cycle("Melee Bloodlust", "vr_melee_bloodlust", {{0.f, "On"}, {1.f, "Off"}}).help("When enabled, restores the player's health when performing melee attacks on enemies."),
        toggle("Parry", "vr_parry").help("Hold a weapon sideways in front of you to block monsters' melee blows."),
        slider("Parry Damage Reduction", "vr_parry_reduction", 0.f, 1.f, 0.05f, "%.2f").help("Share of a parried blow's damage taken away."),
        slider("Parry Drop Chance", "vr_parry_drop_chance", 0.f, 1.f, 0.05f, "%.2f").help("Chance a one-handed parry knocks the weapon out of your hand (two hands: never)."),
        slider("Parry Angle", "vr_parry_angle", 15.f, 80.f, 5.f, "%.0f deg").help("How far the weapon may be off square to the blow."),
        slider("Parry Arm Knock", "vr_parry_wobble", 0.f, 2.f, 0.1f, "%.1f").help("How much a parried blow knocks your hand and arm."),
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
        toggle("Weapon Ammo Screen", "vr_weapon_screen").help("The ammunition text on a small screen on the weapon (colours from the wrist gadget's screen)."),
        slider("Ammo Screen Margin", "vr_weapon_screen_padding", 0.f, 2.f, 0.1f, "%.1f"),
        slider("Ammo Screen CRT Look", "vr_weapon_screen_crt", 0.f, 2.f, 0.1f, "%.1fx").help("Scanlines, a slight flicker, faint static and now and then a glitch, as on the wrist gadget's screen (0 off)."),
        toggle("Map Boards as CRTs", "vr_worldtext_crt").help("The text boards in maps (the tutorial's, the start map's) are CRT screens with glowing text, as the wrist gadget's. Off: plain text."),
        slider("Map Board Hue", "vr_worldtext_hue", 0.f, 355.f, 5.f, "%.0f").help("Their colour: 40 amber, 128 green, 200 blue, 0 red."),
        toggle("Body Picks Up Items", "vr_body_interactions").help("Pick up items and level weapons by walking over them. Thrown and dropped weapons still need your hands."),
        toggle("Disable Haptics", "vr_disablehaptics").help("Disable all haptics (vibration)."),
        cycle("Weapon Mode", "vr_holster_mode", {{0.f, "Immersive"}, {1.f, "Quick Slots"}}).help("Immersive: a holster holds one weapon, and taking it out empties it. Quick Slots: holsters never empty (with weapon cycling)."),
        cycle("Weapon Reloading Mode", "vr_reload_mode", {{0.f, "None"}, {1.f, "All Holsters"}, {2.f, "Hip Holsters"}}).help("Enables weapon reloading mechanics. Weapons can be reloaded by moving them towards an holster. Reloading only works with 'immersive' weapon mode."),
        cycle("Holster Haptics", "vr_holster_haptics", {{0.f, "Off"}, {1.f, "Continuous"}, {2.f, "Once"}}).help("Haptic feedback when hovering a usable holster slot."),
        slider("Holster Haptic Time", "vr_holster_haptic_time", 0.05f, 0.6f, 0.05f, "%.2f s").help("How long the buzz lasts as a hand reaches a holster (Holster Haptics: Once)."),
        toggle("Weapon Cycle Mode", "vr_weapon_cycle_mode").help("Allows or disallows cycling weapons using controller buttons."),
        cycle("Weapon Throw Mode", "vr_weapon_throw_mode", {{0.f, "Immersive"}, {1.f, "Vanish on Hit"}, {2.f, "Discard"}}).help("Immersive: thrown weapons can be picked back up. Vanish on Hit and Discard avoid piles of weapons with Quick Slots."),
        slider("Throw Damage Mult.", "vr_weapon_throw_damage_mult", 0.05f, 5.f, 0.05f, "%.2f").help("Multiplier for weapon throw damage."),
        slider("Throw Speed Mult.", "vr_weapon_throw_velocity_mult", 0.05f, 5.f, 0.05f, "%.2f").help("Multiplier for weapon throw velocity."),
        toggle("Dropped Wpn Particles", "vr_weapondrop_particles").help("Enable particle effects for weapons dropped/thrown on the ground."),
        toggle("Shell Casings", "vr_shells").help("The shotgun throws out a spent shell each shot, the super shotgun its two as you reload (a flick reload flings them). They bounce, roll and lie on the floor."),
        slider("Shell Casing Life", "vr_shells_life", 5.f, 120.f, 5.f, "%.0f s").help("How long spent shells lie on the floor before fading out."),
        slider("Shell Casing Sound", "vr_shells_sound", 0.f, 1.f, 0.1f, "%.1f").help("Volume of their tink as they land (0 off)."),
        cycle("Enemy Weapon Drops", "vr_enemy_drops", {{0.f, "When Eligible"}, {1.f, "Always"}, {2.f, "Disabled"}}).help("Controls random enemy weapon drops. 'Eligible' means that the player has obtained a weapon before through a level weapon pickup."),
        slider("Enemy Drops Chance", "vr_enemy_drops_chance_mult", 0.05f, 5.f, 0.05f, "%.2f").help("Multiplier for enemy weapon drops."),
        cycle("Ammo Box Weapon Drops", "vr_ammobox_drops", {{0.f, "When Eligible"}, {1.f, "Always"}, {2.f, "Disabled"}}).help("Controls random ammo box weapon drops. 'Eligible' means that the player has obtained a weapon before through a level weapon pickup."),
        slider("Ammo Box Drops Chance", "vr_ammobox_drops_chance_mult", 0.05f, 5.f, 0.05f, "%.2f").help("Multiplier for ammo box weapon drops."),
        toggle("Force Grab", "vr_forcegrab_mode").help("Point an empty hand at an object, pull the trigger, flick the hand: it flies to you. Grip as it arrives to catch it."),
        toggle("Force Grab Particles", "vr_forcegrab_eligible_particles").help("Sparkles on the object an empty hand points at, that it can pull."),
        toggle("Force Grab Haptics", "vr_forcegrab_eligible_haptics").help("A tick in the hand when it points at a new object it can pull."),
        slider("Force Grab Outline", "vr_forcegrab_outline", 0.f, 2.f, 0.1f, "%.1f").help("The soft glow round the object a hand points at (0 off)."),
        toggle("Force Grab Effects", "vr_forcegrab_fx").help("A faint beam to what you point at, a crackling tendril when locked on, a trail behind what flies to you."),
    };
}

// Old Quake VR settings: "Graphical Settings".
[[nodiscard]] std::vector<Item> pageGraphicalSettings()
{
    return {
        cycle("Blob Shadows", "vr_blob_shadows", {{0.f, "Off"}, {1.f, "Always"}, {2.f, "Auto"}})
            .help("Dark discs under you and things, a cheap stand-in for shadows. Auto: only where real shadows (Lights and Shadows below) do not fall."),
        cycle("Blob Shadows: You", "vr_player_shadows", {{0.f, "Off"}, {1.f, "Hands"}, {2.f, "Body"}, {3.f, "Both"}}),
        toggle("Blob Shadows: Things", "vr_entity_shadows").help("Blob shadows under monsters and items, away from their light."),
        toggle("Model Lighting", "vr_model_lighting").help("Monsters, items and weapons are shaded from the map's lights, not one fixed direction."),
        toggle("Relit Maps", "vr_relit_maps").help("Maps re-lit with shadows, ambient occlusion and colour, if made (Misc/quakevr/relight_maps.py). Next map."),
        cycle("Anti-aliasing", "vid_fsaa", {{0.f, "Off"}, {2.f, "2x"}, {4.f, "4x"}, {8.f, "8x"}}).help("Multisampling: smoother edges, some GPU time."),

        slider("Light Contrast", "vr_light_contrast", 1.f, 2.5f, 0.02f, "%.2f").help("Darker shade, lamps as bright: the level's own light sources carry the scene (1: Quake's)."),
        slider("Bloom", "vr_bloom", 0.f, 1.5f, 0.02f, "%.2f").help("Glow around lamps, glowing panels and buttons, flashes and explosions (0 off)."),
        slider("Bloom Threshold", "vr_bloom_threshold", 0.3f, 1.f, 0.01f, "%.2f").help("How bright a thing must be to glow."),
        slider("Bloom Size", "vr_bloom_radius", 0.25f, 3.f, 0.05f, "%.2f"),
        slider("Bloom: White Lights", "vr_bloom_white", 0.f, 2.f, 0.02f, "%.2fx").help("Glow of white and pale lights (lamps, flashes), times Bloom."),
        slider("Bloom: Coloured Lights", "vr_bloom_color", 0.f, 3.f, 0.05f, "%.2fx").help("Glow of coloured lights (red buttons, blue panels), times Bloom."),
        slider("Bloom: Adapt", "vr_bloom_adapt", 0.f, 10.f, 0.25f, "%.2f").help("How much bloom weakens when much of the view glows (a brightly lit map); 0: never."),
        slider("Muzzle Flash Light", "vr_flash_scale", 0.f, 3.f, 0.05f, "%.2fx").help("How far your shots light up the room (0 off)."),
        slider("Explosion Light", "vr_explosion_light_scale", 0.f, 3.f, 0.05f, "%.2fx"),
        toggle("Decals", "vr_decals").help("Blood splats, gib trails, scorch marks and bullet chips on walls and floors."),
        slider("Decal Lifetime", "vr_decal_life", 10.f, 600.f, 10.f, "%.0f s"),
        slider("Max Decals", "vr_decal_max", 64.f, 2048.f, 64.f, "%.0f"),
        toggle("Gib Blood", "vr_gib_blood").help("Gibs and heads leave a trail of blood drops and splat where they hit walls and floors. Off: Quake's trail."),
        slider("Gib Blood Trail", "vr_gib_blood_trail", 0.f, 3.f, 0.25f, "%.2fx").help("How dense their trail of blood and drops is (0 none)."),
        toggle("Coloured Lights", "vr_colored_lights").help("Warm muzzle flashes, rockets and explosions; the lightning gun's blue."),
        slider("Projectile Light", "vr_projectile_lights", 0.f, 2.f, 0.05f, "%.2fx").help("Hell knight flames, scrag spit, vore balls and lasers light up the room, and where they hit (0 off)."),
        slider("Ammo Screen Light", "vr_weapon_screen_light", 0.f, 3.f, 0.1f, "%.1fx").help("Your weapons' ammo screens cast a small light in their colour on what is in front of them (0 off)."),
        slider("Screen Glow", "vr_screen_glow", 0.f, 3.f, 0.1f, "%.1fx").help("The wrist gadget's and your weapons' screens glow softly round their edges (0 off)."),
        slider("Screen Text Glow", "vr_screen_text_glow", 0.f, 3.f, 0.1f, "%.1fx").help("The text, numbers and icons on those screens glow: bright whitish cores, a soft halo (0 off)."),
        slider("Weapon Sight Glow", "vr_weapon_glow", 0.f, 3.f, 0.1f, "%.1fx").help("Your weapons' glowing parts, as the shotgun's sights, shine brighter and glow (0: as Quake)."),
        cycle("Dynamic Light Falloff", "vr_dlight_falloff", {{0.f, "Quake"}, {1.f, "DarkPlaces"}})
            .help("DarkPlaces: strong lights, full near and smoothly gone at the edge. Quake: dimmer, fading linearly all the way."),
        slider("Light Sheen", "vr_specular", 0.f, 0.5f, 0.025f, "%.3f").help("Dynamic lights glint off walls and models (0 off)."),
        toggle("Bump Maps", "vr_normalmaps").help("Bumps made from the textures, shown by the map's light and dynamic lights. Next map."),
        slider("Bump Depth", "vr_normalmap_strength", 0.f, 3.f, 0.05f, "%.2f"),
        slider("Bumps in Map Light", "vr_normalmap_baked", 0.f, 2.f, 0.1f, "%.1f").help("How much the bumps shade the map's own light, lit from where it seems to come from (0: only dynamic lights show them)."),
        slider("Bumps on Models", "vr_normalmap_models", 0.f, 2.f, 0.1f, "%.1f").help("How much the bumps shade models' own light (monsters, pickups, weapons; held ones half), lit from the map's lights (times Bumps in Map Light; 0: only dynamic lights show them)."),
        toggle("Parallax", "vr_parallax").help("Walls and floors look carved: their dark parts sink in and shift as you move your head (needs Bump Maps)."),
        slider("Parallax Depth", "vr_parallax_depth", 0.5f, 6.f, 0.25f, "%.2f").help("How deep the darkest parts of the textures sink, in Quake units."),
        slider("Parallax Distance", "vr_parallax_distance", 128.f, 2048.f, 64.f, "%.0f").help("How far away walls still look carved (further costs more)."),
        slider("Parallax Items Depth", "vr_parallax_items", 0.f, 4.f, 0.25f, "%.2f").help("How deep the ammo and health boxes look carved, in their own units (scaled with the box; 0 off)."),
        slider("Parallax Models Depth", "vr_parallax_models", 0.f, 2.f, 0.125f, "%.3f").help("How deep weapons, pickups, monsters and your body look carved, in their own units (0 off, the default: on Quake's blocky skins it bends the texels)."),
        toggle("Models Lit as the World", "vr_model_light_parity").help("Monsters and items as bright as the floor they stand on (off: Quake's brighter models)."),
        slider("Hands' Least Light", "vr_viewmodel_minlight", 0.f, 32.f, 1.f, "%.0f").help("How lit your hands and weapons stay in the dark (Quake: 24)."),
        cycle("Smooth Textures", "vr_texture_smooth", {{0.f, "As gl_texturemode"}, {1.f, "Replacements"}, {2.f, "All"}})
            .help("Smooth filtering for high-resolution replacement textures, or for all textures."),
        slider("Headset Gamma", "vr_gamma", 0.5f, 1.5f, 0.02f, "%.2f").help("The headset's own gamma (lower is brighter; 1 neutral). The desktop's is not used."),
        slider("Headset Contrast", "vr_contrast", 0.5f, 2.f, 0.02f, "%.2f").help("The headset's own contrast (1 neutral)."),

        header("Water and Liquids"),
        slider("Waves", "vr_water_waves", 0.f, 2.f, 0.1f, "%.1f").help("Waves on water, slime and lava, in the lighting (the surfaces stay flat). 0: Quake's flat liquids."),
        slider("Water Reflection", "vr_water_fresnel", 0.f, 2.f, 0.1f, "%.1f").help("Water is clearer looking straight down and reflects the room at grazing angles."),
        slider("Water Refraction", "vr_water_refraction", 0.f, 2.f, 0.1f, "%.1f").help("What is under the water is bent by the waves (needs no anti-aliasing and r_oit 1)."),
        slider("Water Glints", "vr_water_glints", 0.f, 2.f, 0.1f, "%.1f").help("Light glinting off the waves."),
        slider("Lava Glow", "vr_water_lava_glow", 0.f, 2.f, 0.1f, "%.1f").help("Lava glows, its hot parts brightest, pulsing slowly."),
        slider("Caustics", "vr_water_caustics", 0.f, 1.5f, 0.05f, "%.2f").help("Dappled light on floors and walls under water."),
        slider("Underwater View", "vr_water_underwater", 0.f, 1.5f, 0.1f, "%.1f").help("With your head under: the liquid's fog and tint, a little blur. 0: Quake's colour shift."),
        slider("Underwater Wobble", "vr_water_wobble", 0.f, 2.f, 0.1f, "%.1f").help("A slow wobble of the view under water. Turn it down or off if it makes you uneasy."),
        toggle("Real Waves", "vr_water_geo_waves").help("The surfaces of water, slime and lava rise and fall in long swells (still at the walls). Off: flat, the waves only in the lighting."),
        slider("Real Wave Height", "vr_water_geo_amplitude", 0.5f, 8.f, 0.5f, "%.1f").help("How high the swells rise, in units (lava's are bigger and slower, slime's smaller)."),
        slider("Splashes", "vr_water_splash", 0.f, 2.f, 0.1f, "%.1f").help("Drops, foam and ripples where shots, thrown things, you and your hands hit water, slime or lava, and as you wade (0 off)."),
        slider("Water Sounds", "vr_water_sounds", 0.f, 1.f, 0.1f, "%.1f").help("Volume of splashes, shots into water, wading and swimming strokes (0 off: Quake's own splash sounds only)."),

        header("Lights and Shadows"),
        cycle("Preset", "vr_graphics_preset", {{-1.f, "Custom"}, {0.f, "Off (Quake)"}, {1.f, "Low"}, {2.f, "Medium"}, {3.f, "High"}, {4.f, "Ultra"}})
            .help("Sets everything below (and the model options above). Change them after to taste."),
        slider("Shadowed Dynamic Lights", "vr_shadow_dlights", 0.f, 12.f, 1.f, "%.0f")
            .help("Explosions, rockets and other dynamic lights casting shadows, the most important first (0 off)."),
        cycle("Dynamic Shadow Detail", "vr_shadow_dlight_size", {{128.f, "128"}, {256.f, "256"}, {512.f, "512"}, {1024.f, "1024"}})
            .help("Largest shadow face size, in texels; farther lights get smaller ones."),
        toggle("Muzzle Flash Shadows", "vr_shadow_muzzleflash").help("Your own muzzle flashes cast shadows too (brief; costs more)."),
        slider("Map Light Shadows", "vr_shadow_maplights", 0.f, 8.f, 1.f, "%.0f")
            .help("Map lights near you casting the shadows of monsters and of you (0 off)."),
        cycle("Map Shadow Detail", "vr_shadow_maplight_size", {{256.f, "256"}, {512.f, "512"}, {1024.f, "1024"}})
            .help("Their shadow face size, in texels."),
        slider("Map Shadow Strength", "vr_shadow_maplight_strength", 0.f, 1.f, 0.05f, "%.2f")
            .help("How much of a map light those shadows take away."),
        cycle("Your Shadow", "vr_shadow_self", {{0.f, "None"}, {1.f, "Body"}, {2.f, "Body and hands"}})
            .help("What of you casts map lights' shadows."),
        cycle("Shadow Softness", "vr_shadow_filter", {{0.f, "Hard (1 tap)"}, {1.f, "Soft (4 taps)"}, {2.f, "Softer (9)"}, {3.f, "Softest (16)"}}),
        slider("Shadow Distance", "vr_shadow_distance", 256.f, 4096.f, 128.f, "%.0f")
            .help("Lights farther than this from you cast no shadows."),
        cycle("Shadow Atlas", "vr_shadow_atlas", {{2048.f, "2048"}, {4096.f, "4096"}, {8192.f, "8192"}})
            .help("Room for all shadows at once: smaller fits fewer or coarser ones (64 MB at 4096)."),
        slider("Shadow Bias", "vr_shadow_bias", 0.f, 4.f, 0.25f, "%.2f")
            .help("Higher: fewer speckles on lit surfaces; lower: shadows start closer to what casts them."),
        toggle("Dynamic Lights on Models", "vr_dlight_models").help("Per pixel, by the angle, and shadowed (off: Quake's flat brightening)."),
        slider("Dynamic Light Angle", "vr_dlight_angle", 0.f, 1.f, 0.25f, "%.2f")
            .help("How much surfaces facing away from a dynamic light get less of it (0: Quake's)."),
        toggle("Uncapped Dynamic Lights", "vr_dlight_uncapped").help("Dynamic lights add fully to bright walls (Quake caps them)."),
        toggle("Shadow Statistics", "vr_shadow_stats").help("Print what is shadowed and its GPU time, once a second, to the console."),
        cycle("Performance Profile", "vr_profile", {{0.f, "Off"}, {1.f, "Record"}, {2.f, "Record and show"}})
            .help("Record CPU and GPU time per effect into quakevr/profile/ every 5 seconds (and show the top costs over the wrist). Not kept after a restart."),
        cycle("Memory Log", "vr_memstats_log", {{0.f, "Off"}, {30.f, "Every 30 s"}, {60.f, "Every minute"}, {300.f, "Every 5 minutes"}})
            .help("Write memory use and the frame rate to quakevr/profile/memstats_<date>.csv, and after each map load."),
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

