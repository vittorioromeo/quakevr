
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Round.hpp"

// vr_menu_pages.inc -- VR Settings pages, included by vr_menu.cpp. The long ones are split into pages of a screenful
// or so (Graphics in seven).

[[nodiscard]] za::Vector<Item> pageMenuSettings()
{
    return {
        row(opt::vr_menu_scale),
        row(opt::vr_menu_distance),
        row(opt::vr_menu_grab),
        row(opt::vr_menu_resolution),
        row(opt::vr_menu_sharpen),
        row(opt::vr_menu_status),
        row(opt::vr_menu_banner_beside),
        row(opt::vr_menu_version),
        row(opt::vr_update_check),
        row(opt::vr_menu_flat_shortcuts),
        row(opt::vr_menu_main_mods),
        row(opt::vr_menu_fine_step),
        row(opt::vr_menu_section_gap),
        row(opt::vr_menu_vr_style),
        open("Laser Hue: Colours", pageIndex(pageColours)).help("Every hue is on HUD and Menus > Colours."),
        row(opt::vr_menu_bigfont),
        row(opt::vr_menu_recolor),
        row(opt::vr_menu_recolor_strength),
        hueSlider("Red Hue", vr_menu_recolor_hue).help("With Blood Red Menus: the hue they turn to. 0 red, the banner's (as shipped); 345 crimson, 20 a fiery red-orange. Player's: the Player Effects Hue."),
        row(opt::vr_menu_recolor_saturation),
        row(opt::vr_menu_spacing),
        row(opt::vr_menu_height),
        row(opt::scr_menubgalpha),
        row(opt::ui_live_preview),
        cycle("Menu Button", vr_menu_button, menuButtonChoices()).help(menuButtonHelp),
        row(opt::vr_menu_remember),
        row(opt::vr_menu_help_wpm),
        row(opt::vr_menu_dropdown),
    };
}

[[nodiscard]] za::Vector<Item> pageCrosshairSettings()
{
    return {
        open("Crosshair: VR Settings", pageIndex(pageVrsHudHapticsSound)).help("The crosshair's kind is on VR Settings."),
        row(opt::vr_crosshair_depth),
        row(opt::vr_crosshair_size),
        row(opt::vr_crosshair_alpha),
        open("Crosshair Hue: Colours", pageIndex(pageColours)).help("Every hue is on HUD and Menus > Colours."),
        row(opt::vr_crosshairy),
    };
}

[[nodiscard]] za::Vector<Item> pageFireParticles()
{
    return {
        row(opt::vr_fire_particles),
        row(opt::vr_fire_particles_count),
        row(opt::vr_fire_particles_frequency),
        row(opt::vr_fire_particles_origin),
        row(opt::vr_fire_particles_speed_min),
        row(opt::vr_fire_particles_speed_max),
        row(opt::vr_fire_particles_up_min),
        row(opt::vr_fire_particles_up_max),
        row(opt::vr_fire_particles_life_min),
        row(opt::vr_fire_particles_life_max),
        row(opt::vr_fire_particles_size),
        row(opt::vr_fire_particles_alpha),
        row(opt::vr_fire_particles_range),
        header("Flames in the Air"),
        row(opt::vr_flame_wobble),
        row(opt::vr_flame_wobble_strength),
        row(opt::vr_flame_wobble_speed),
    };
}

// Explosion chunks in multiplayer (vr_explosion_debris_mp_max), its leftmost step -1: as many as in single player.
[[nodiscard]] Item explosionDebrisMultiplayerSlider()
{
    Item i = row(opt::vr_explosion_debris_mp_max);
    i.negativeLabel = "Single Player's";
    return i;
}

// A separate tuning page for the physical blast fragments and their trails/glows.
[[nodiscard]] za::Vector<Item> pageExplosionDebris()
{
    return {
        row(opt::vr_explosion_debris),
        row(opt::vr_explosion_debris_count),
        row(opt::vr_explosion_debris_speed_min),
        row(opt::vr_explosion_debris_speed_max),
        row(opt::vr_explosion_debris_up),
        row(opt::vr_explosion_debris_size_min),
        row(opt::vr_explosion_debris_size_max),
        row(opt::vr_explosion_debris_life_min),
        row(opt::vr_explosion_debris_life_max),
        row(opt::vr_explosion_debris_bounce),
        row(opt::vr_explosion_debris_max),
        explosionDebrisMultiplayerSlider(),
        row(opt::vr_explosion_debris_trail),
        row(opt::vr_explosion_debris_lights),
        row(opt::vr_explosion_debris_light_radius),
        command("Preview an Explosion", "vr_explosion_debris_test").help("Spawns an explosion effect ahead without blast damage. Requires a loaded map."),
    };
}

[[nodiscard]] za::Vector<Item> pageParticleSettings()
{
    return {
        open("Explosion Debris", pageIndex(pageExplosionDebris)),
        open("Fire Particles", pageIndex(pageFireParticles)),
        row(opt::vr_explosion_particles),
        row(opt::r_particles),
        row(opt::vr_particles),
        row(opt::vr_particle_mult),
    };
}

// (Climbing has its own page; the swimming toggle is on Swimming.)
[[nodiscard]] za::Vector<Item> pageLocomotionSettings()
{
    return {
        row(opt::vr_comfort_preset),
        row(opt::vr_movement_mode),
        row(opt::vr_deadzone),
        row(opt::vr_deadzone_outer),
        row(opt::vr_move_curve),
        row(opt::vr_move_autorun),
        row(opt::vr_move_autorun_threshold),
        row(opt::cl_forwardspeed),
        row(opt::cl_alwaysrun),
        row(opt::cl_movespeedkey),
        row(opt::vr_crouch_speed),
        row(opt::vr_crouch_speed_scale),
        row(opt::vr_slope_walk),
        header("Turning"),
        row(opt::vr_snap_turn),
        row(opt::vr_turn_speed),
        row(opt::vr_enable_joystick_turn),
        header("Teleport"),
        row(opt::vr_teleport_enabled),
        row(opt::vr_teleport_range),
        header("Leaning"),
        row(opt::vr_lean_radius),
        row(opt::vr_lean_recenter),
        row(opt::vr_lean_detect),
        row(opt::vr_lean_turn),
        header("Room Scale"),
        row(opt::vr_roomscale_move_mult),
        row(opt::vr_roomscale_jump),
        row(opt::vr_roomscale_jump_threshold),
    };
}

[[nodiscard]] za::Vector<Item> pageStamina();

// Split from Locomotion: climbing ledges and rungs.
[[nodiscard]] za::Vector<Item> pageClimbing()
{
    return {
        row(opt::vr_climb),
        row(opt::vr_climb_min_height),
        row(opt::vr_climb_fling),
        row(opt::vr_climb_blast_letgo),
        row(opt::vr_climb_leniency),
        row(opt::vr_climb_leniency_air),
        row(opt::vr_climb_air_grab_time),
        row(opt::vr_climb_touch),
        row(opt::vr_climb_over_top),
        row(opt::vr_climb_slide).extend().help("When your body can't follow your hands along a ledge (past your arm's reach, against a wall), the hand slides along it and stays on your controller. Off: it stays where it took hold, and drifts away from your controller."),
        row(opt::vr_physsound_grab),
        row(opt::vr_climb_mantle_height),
        row(opt::vr_climb_mantle_lenient),
        row(opt::vr_climb_mantle_slope),
        row(opt::vr_climb_mantle_grunt),
        cycle("Mantle Grunt Sound", "vr_climb_mantle_grunt_sound", gruntChoices()).help("Which grunt the mantle makes. Hard landing (the default): the strained \"oof\" of landing from a height, so it doesn't sound like a jump. The deeper ones are Quake's played slower. Hear it with Hear Mantle Grunt."),
        command("Hear Mantle Grunt", "vr_climb_mantle_grunt_test").help("Plays the mantle's grunt as set, at full volume."),
        header("Climbing Props"),
        row(opt::vr_climb_props),
        row(opt::vr_climb_prop_min_height),
        header("Climbing Stamina"),
        open("Climbing Stamina: Stamina", pageIndex(pageStamina)).help("What hanging from a hold costs is on Combat > Stamina."),
        row(opt::vr_climb_stamina_slip),
        open("Tired Arms (Stamina)", pageIndex(pageStamina)).help("Low on stamina, your arms shake while you hang (looks only) and your hands get heavy: on the Stamina page."),
        header("Hand on a Hold (Looks Only)"),
        row(opt::vr_climb_hand_out),
        row(opt::vr_climb_hand_up),
        row(opt::vr_climb_hand_side),
        row(opt::vr_climb_hand_turn_blend),
        row(opt::vr_climb_hand_pitch),
        row(opt::vr_climb_hand_yaw),
        row(opt::vr_climb_hand_roll),
    };
}

// Swimming (vr_physics.cpp, "Swimming"): the stick in water, the strokes, and telling a stroke from
// the return of the arms for the next one.
void resetSwimming()
{
    static constexpr const char* names[] = {"vr_swim", "vr_swim_shallow_speed", "vr_swim_wade_speed",
        "vr_swim_stick_speed", "vr_swim_stroke", "vr_swim_stroke_min", "vr_swim_palm", "vr_swim_stroke_assist",
        "vr_swim_max_speed", "vr_swim_look", "vr_swim_recovery", "vr_swim_glide", "vr_swim_power_threshold",
        "vr_swim_power_knee", "vr_swim_power_whole", "vr_swim_intent_memory", "vr_swim_reverse_damp", "vr_swim_reverse_speed",
        "vr_swim_flat_exp", "vr_swim_speed_exp", "vr_swim_palm_dir", "vr_swim_against_palm", "vr_swim_stroke_pitch", "vr_air_supply", "vr_water_jump"};
    for(const char* name : names)
    {
        if(cvar_t* var = Cvar_FindVar(name))
        {
            Cvar_SetQuick(var, var->default_string);
        }
    }
}

[[nodiscard]] za::Vector<Item> pageSwimSettings()
{
    return {
        row(opt::vr_swim),
        row(opt::vr_swim_shallow_speed),
        row(opt::vr_swim_wade_speed),
        row(opt::vr_swim_stick_speed),
        row(opt::vr_air_supply),
        row(opt::vr_water_jump),
        header("Strokes"),
        row(opt::vr_swim_stroke),
        row(opt::vr_swim_speed_exp),
        row(opt::vr_swim_palm),
        row(opt::vr_swim_against_palm),
        row(opt::vr_swim_flat_exp),
        row(opt::vr_swim_recovery),
        row(opt::vr_swim_palm_dir),
        row(opt::vr_swim_stroke_min),
        header("Stroke or Return?"),
        row(opt::vr_swim_power_threshold),
        row(opt::vr_swim_power_knee),
        row(opt::vr_swim_power_whole),
        row(opt::vr_swim_intent_memory),
        row(opt::vr_swim_reverse_damp),
        row(opt::vr_swim_reverse_speed),
        header("Direction and Speed"),
        row(opt::vr_swim_look),
        row(opt::vr_swim_stroke_assist),
        row(opt::vr_swim_stroke_pitch),
        row(opt::vr_swim_glide),
        row(opt::vr_swim_max_speed),
        action("Reset Swimming to Defaults", resetSwimming).help("Every setting on this page back to its default."),
    };
}

// Hand Calibration (Hand/Gun Calibration's first section; hands::calibration): the page shows the off hand's own sliders
// (vr_handcal_off_mirror 0) or not; items() builds it again when that changes.
int handCalPageOwn = -1;

// Match Controller Preview: each hand calibrated (moved only) so that the middle of the empty hand's fist is on the
// Show Controller preview's point; the off hand too when it has its own values.
void matchControllerPreview()
{
    const hands::State& s = hands::current();
    const float cm = 0.01f * units::metresToUnits();
    const bool own = vr_handcal_off_mirror.value == 0.f;
    int matched = 0;
    for(const int hand : {HAND_MAIN, HAND_OFF})
    {
        if(hand == HAND_OFF && !own)
        {
            continue;
        }
        glm::vec3 d;
        if(!s.valid || !vrActive() || !view::previewGripMove(s, hand, d))
        {
            Con_Printf("Match Controller Preview: the %s hand isn't drawn jointed (Jointed Hand on, in the game)\n",
                hand == HAND_MAIN ? "main" : "off");
            continue;
        }
        glm::vec3 f, r, u;
        hands::angleVectors(s.gripRot[hand], f, r, u);
        const glm::vec3 local = glm::vec3{glm::dot(d, f), -glm::dot(d, r), glm::dot(d, u)} / cm;
        cvar_t& x = hand == HAND_MAIN ? vr_handcal_x : vr_handcal_off_x;
        cvar_t& y = hand == HAND_MAIN ? vr_handcal_y : vr_handcal_off_y;
        cvar_t& z = hand == HAND_MAIN ? vr_handcal_z : vr_handcal_off_z;
        const auto set = [](cvar_t& var, float v) {
            Cvar_SetQuick(&var, va("%.2f", za::round((var.value + v) * 100.f) / 100.f + 0.f));
        };
        set(x, local.x);
        set(y, local.y);
        set(z, local.z);
        Con_Printf("Match Controller Preview: %s hand moved %+.2f %+.2f %+.2f cm (now %s %s %s)\n",
            hand == HAND_MAIN ? "main" : "off", local.x, local.y, local.z, x.string, y.string, z.string);
        matched++;
    }
    S_LocalSound(matched ? "misc/menu2.wav" : "misc/menu3.wav");
}

// The calibration's moves and rolls back to their defaults (the shipped calibration, which the shipped weapon offsets were
// set over; not the pitch and yaw: they are the aim).
void resetHandMoves()
{
    for(cvar_t* var : {&vr_handcal_x, &vr_handcal_y, &vr_handcal_z, &vr_handcal_roll, &vr_handcal_off_x, &vr_handcal_off_y,
            &vr_handcal_off_z, &vr_handcal_off_roll})
    {
        Cvar_SetQuick(var, var->default_string);
    }
}

// The hand calibration first.
[[nodiscard]] za::Vector<Item> pageHandGunCalibration()
{
    const char* moveHelp = "Moves the whole hand, and what it holds (its aim, muzzle and melee with it), along the controller's "
                           "own axes: X along the handle, Y left, Z up (Show Controller's red, green, blue). Centimetres.";
    const char* rollHelp = "Turns the whole hand, and what it holds, about where it points, through the controller's grip "
                           "(the handle's middle): the right side down. Doesn't change where it aims.";
    za::Vector<Item> list{
        open("Fingers and Collisions", pageIndex(pageFingersCollisions))
            .help("The jointed hand's fingers fitting what it holds, and hands and weapons stopping at walls, weapons and models."),
        header("Hand Calibration"),
        info([]() -> const char* { return "1. Hold the controller as you always do."; }),
        info([]() -> const char* { return "2. Show Controller on, and look at it."; }),
        info([]() -> const char* { return "3. Move and turn the hand until the"; }),
        info([]() -> const char* { return "controller sits in its palm, just where"; }),
        info([]() -> const char* { return "your real one is."; }),
        row(opt::vr_show_controller),
        row(opt::vr_handcal_x).help(moveHelp),
        row(opt::vr_handcal_y).help(moveHelp),
        row(opt::vr_handcal_z).help(moveHelp),
        row(opt::vr_gunangle),
        row(opt::vr_offhandpitch),
        row(opt::vr_gunyaw),
        row(opt::vr_handcal_roll).help(rollHelp),
        row(opt::vr_handcal_off_mirror),
    };
    handCalPageOwn = vr_handcal_off_mirror.value == 0.f ? 1 : 0;
    if(handCalPageOwn)
    {
        list.pushBackMultiple(
            row(opt::vr_handcal_off_x).help(moveHelp),
            row(opt::vr_handcal_off_y).help(moveHelp),
            row(opt::vr_handcal_off_z).help(moveHelp),
            row(opt::vr_offhandyaw),
            row(opt::vr_handcal_off_roll).help(rollHelp)
        );
    }
    list.pushBackMultiple(
        action("Match Controller Preview", matchControllerPreview)
            .help("Moves each hand (X, Y, Z only) so that the middle of its empty fist is on the preview's point (the "
                  "middle of the handle, where OpenXR puts the fist). Then turn it (Pitch, Yaw, Roll) to fit, and move "
                  "it again. The off hand only with its own values; needs the jointed hand."),
        action("Reset Moves and Rolls", resetHandMoves)
            .help("Both hands' X, Y, Z and Roll back to their defaults (the calibration the weapon offsets were set with). "
                  "Pitch and Yaw stay."),
        header("Guns"),
        row(opt::vr_gunmodelscale),
        row(opt::vr_gunmodely),
        row(opt::vr_gun_z_offset),
        row(opt::vr_finger_grip_bias),
        row(opt::vr_finger_auto_close_thumb),
        row(opt::vr_weapon_button_cone),
        row(opt::vr_weapon_button_depth),
        row(opt::vr_ammo_switch_empty_hand)
    );
    return list;
}

// Split from Hand/Gun Calibration: the jointed hand's fingers, and hands and weapons against models.
[[nodiscard]] za::Vector<Item> pageFingersCollisions()
{
    return {
        header("Fingers"),
        row(opt::vr_hand_rig),
        row(opt::vr_hand_fit),
        row(opt::vr_hand_fit_thumb_outside),
        row(opt::vr_hand_fit_thumb_wide),
        header("Against Monsters and Things"),
        row(opt::vr_model_collide),
    };
}

[[nodiscard]] za::Vector<Item> pagePlayerCalibration()
{
    return {
        open("World Scale and Floor Offset: Body and Display", pageIndex(pageBodyDisplay)).help("World Scale and Floor Offset are on Body and Display."),
    };
}

// (The parry settings are on Parry and Bash.) And the headbutt.
[[nodiscard]] za::Vector<Item> pageMeleeSettings()
{
    return {
        row(opt::vr_melee_speed),
        row(opt::vr_melee_pommel_wait),
        row(opt::vr_melee_butt_run),
        row(opt::vr_melee_punch_mult),
        row(opt::vr_melee_slap),
        row(opt::vr_melee_slap_mult),
        row(opt::vr_melee_dmg_multiplier),
        row(opt::vr_melee_dmg_min),
        row(opt::vr_melee_dmg_max),
        row(opt::vr_melee_range_multiplier),
        row(opt::vr_melee_wall_hold),
        row(opt::vr_melee_bloodlust),
        row(opt::vr_melee_bloodlust_mult),
        open("Weapon Damage", pageIndex(pageWeaponDamage))
            .help("Each weapon's base damage: the fist, the axe, the crowbar, Mjolnir, the swords..."),
        open("Incandescent Blades", pageIndex(pageHotBlades))
            .help("A melee weapon held in a flame or lava glows red-hot: harder blows that set what they strike on fire."),
        open("Flail", pageIndex(pageFlail)) // AD support (vr_menu_ad_guns.inc)
            .help("Arcane Dimensions' death sergeant's flail, its spiked head swinging on a chain: the chain's length, the "
                  "head's mass and air drag, its blows' damage and speeds, its rattle, its head lying about."),
        header("Swing Sounds"),
        row(opt::vr_melee_swish),
        row(opt::vr_melee_swish_min),
        row(opt::vr_melee_swish_sub),
        row(opt::vr_melee_swish_full),
        row(opt::vr_melee_swish_gain),
        header("Swing Through Enemies"),
        row(opt::vr_melee_phase),
        row(opt::vr_melee_phase_speed),
        row(opt::vr_melee_phase_time),
        row(opt::vr_melee_phase_hold),
        header("Strike Past the Floor"),
        row(opt::vr_melee_world_pass),
        row(opt::vr_melee_world_pass_length),
        row(opt::vr_melee_world_pass_speed),
        row(opt::vr_melee_world_pass_time),
        row(opt::vr_melee_world_pass_reach),
        header("One Swing, Many Hits"),
        row(opt::vr_melee_multi),
        row(opt::vr_melee_multi_max),
        header("Headbutt"),
        row(opt::vr_headbutt),
        row(opt::vr_headbutt_speed),
    };
}

// And how weight feels (its damage on Weight and Damage).
[[nodiscard]] za::Vector<Item> pageAimingSettings()
{
    return {
        row(opt::vr_2h_mode),
        row(opt::vr_dominant_eye),
        row(opt::vr_2h_handoff),
        row(opt::vr_2h_grip_sound),
        row(opt::vr_2h_angle_threshold),
        row(opt::vr_2h_virtual_stock_factor),
        header("2H Grip Stickiness"),
        row(opt::vr_2h_sticky),
        row(opt::vr_2h_sticky_fast),
        row(opt::vr_2h_sticky_fast_from),
        row(opt::vr_2h_sticky_fast_full),
        row(opt::vr_2h_sticky_fast_hold),
        header("Weight"),
        info([]() -> const char* {
            return "What you hold follows on a spring.";
        }).help("Heavy things lag, overshoot a little and settle, by their mass, balance and length (Weapon Weights, Held "
                "Object Weights): long ones turn slowly, and they droop a little at arm's length. Two hands hold them much "
                "steadier. The drawn hand stays on it, and the aim and melee follow it. Each weapon and prop multiplies the "
                "Spring settings below with its own."),
        open("Weapon Weights (Held Weapon)", pageIndex(pageWeaponWeights)),
        open("Held Object Weights (Held Prop)", pageIndex(pageHeldObjectWeights)),
        open("Weight and Damage", pageIndex(pageWeightDamage)).help("How the weight of what you strike with or throw changes its damage."),
        infoLine(weightReadout, 1),
        infoLine(weightReadout, 0),
        header("Tired Arms"),
        row(opt::vr_weight_stamina),
        open("Tired Arms Shake (Stamina)", pageIndex(pageStamina)).help("Low on stamina, your arms shake too (looks only): on the Stamina page."),
    };
}

// Split from Aiming: the weight of what you strike with or throw, and its damage.
[[nodiscard]] za::Vector<Item> pageWeightDamage()
{
    return {
    };
}

// (The weapons' ammo screens and map boards are on the Screens page, force grab on Force Grab, weapon drops on
// World.)
[[nodiscard]] za::Vector<Item> pageImmersionSettings()
{
    return {
        row(opt::vr_body_interactions),
        row(opt::vr_disablehaptics),
        row(opt::vr_haptics_strength),
        header("Weapons and Holsters"),
        row(opt::vr_holster_mode),
        row(opt::vr_reload_mode),
        open("Holster Zones", pageIndex(pageHolsterZones))
            .help("Where a hand (and the gun it holds) is at a holster or a pouch: a box at each, as drawn, their sizes and "
                  "places, and a wireframe of them."),
        row(opt::vr_holster_haptics),
        row(opt::vr_weapon_cycle_mode),
        open("Throw Damage: Weapon Damage", pageIndex(pageWeaponDamage)).help("The damage of thrown weapons is on Combat > Weapon Damage (All Throws)."),
        row(opt::vr_weapondrop_particles),
        header("Magnetic Holsters"),
        row(opt::vr_magholster),
        row(opt::vr_magholster_button),
        row(opt::vr_magholster_return_time),
        row(opt::vr_magholster_return_distance),
        row(opt::vr_magholster_land_time),
        row(opt::vr_magholster_flight_time),
        row(opt::vr_magholster_style),
        row(opt::vr_magholster_chain_size),
        row(opt::vr_magholster_chain_alpha),
        row(opt::vr_magholster_chain_lod),
        row(opt::vr_magholster_zip_volume),
        row(opt::vr_magholster_draw_volume),
        header("Chain Physics"),
        row(opt::vr_magholster_chain_collide),
        row(opt::vr_magholster_chain_spacing),
        row(opt::vr_magholster_chain_points),
        row(opt::vr_magholster_chain_iterations),
        row(opt::vr_magholster_chain_stiffness),
        row(opt::vr_magholster_chain_radius),
        row(opt::vr_magholster_chain_drag),
        row(opt::vr_magholster_chain_slide),
        row(opt::vr_magholster_chain_carry),
        row(opt::vr_magholster_chain_sway),
        header("Chain Origin"),
        row(opt::vr_magholster_chain_x),
        row(opt::vr_magholster_chain_y),
        row(opt::vr_magholster_chain_z),
        row(opt::vr_magholster_chain_hip_x),
        row(opt::vr_magholster_chain_hip_y),
        row(opt::vr_magholster_chain_hip_z),
        row(opt::vr_magholster_chain_upper_x),
        row(opt::vr_magholster_chain_upper_y),
        row(opt::vr_magholster_chain_upper_z),
        row(opt::vr_magholster_chain_shoulder_x),
        row(opt::vr_magholster_chain_shoulder_y),
        row(opt::vr_magholster_chain_shoulder_z),
        header("Shell Casings"),
        row(opt::vr_shells),
    };
}

[[nodiscard]] za::Vector<Item> pageHeldObjectOffsets();

// Weapons > Reloading > Shotgun: its shells and its loading port.
[[nodiscard]] za::Vector<Item> pageReloadShotgun()
{
    return {
        header("Shells"),
        row(opt::vr_reload_shell_pairs),
        open("Shell Pose: Held Object Offsets", pageIndex(pageHeldObjectOffsets))
            .help("How a shell sits in the fingers: take one from the pouch, then tune it there (its Grip and Fingers). "
                  "The taped pair has its own."),
        header("At the Pouch"),
        row(opt::vr_reload_pouch_shot),
        header("Load Point"),
        row(opt::vr_reload_show_ports),
        row(opt::vr_reload_port_shot_x),
        row(opt::vr_reload_port_shot_y),
        row(opt::vr_reload_port_shot_z),
        row(opt::vr_reload_port_shot_radius),
    };
}

// Weapons > Reloading > Super Shotgun: broken open (phase 2b) and its breech.
[[nodiscard]] za::Vector<Item> pageReloadSuperShotgun()
{
    return {
        header("Break Open"),
        row(opt::vr_reload_ssg_break),
        row(opt::vr_reload_ssg_open_angle),
        row(opt::vr_ssg_fire_anim_speed),
        header("Open by"),
        row(opt::vr_reload_ssg_open_flick),
        row(opt::vr_reload_ssg_flick_open_speed),
        row(opt::vr_reload_ssg_pry),
        row(opt::vr_reload_ssg_pry_angle),
        row(opt::vr_reload_ssg_pry_speed),
        row(opt::vr_reload_ssg_open_hit),
        row(opt::vr_reload_ssg_open_button),
        header("Close by"),
        row(opt::vr_reload_ssg_close_flick),
        row(opt::vr_reload_ssg_flick_close_speed),
        row(opt::vr_reload_ssg_close_flick_loaded),
        row(opt::vr_reload_ssg_close_pry),
        row(opt::vr_reload_ssg_lift_angle),
        row(opt::vr_reload_ssg_lift_hold),
        row(opt::vr_reload_ssg_close_hit),
        row(opt::vr_reload_ssg_close_auto),
        row(opt::vr_reload_ssg_close_button),
        header("Hits (Above and Below)"),
        row(opt::vr_reload_ssg_hit_after_load),
        header("At the Pouch"),
        row(opt::vr_reload_pouch_sshot),
        header("Load Point"),
        row(opt::vr_reload_show_ports),
        row(opt::vr_reload_port_sshot_x),
        row(opt::vr_reload_port_sshot_y),
        row(opt::vr_reload_port_sshot_z),
        row(opt::vr_reload_port_sshot_radius),
    };
}

// Weapons > Reloading > Nailgun: its well, its magazine, holding and pulling it.
[[nodiscard]] za::Vector<Item> pageReloadNailgun()
{
    return {
        header("At the Pouch"),
        row(opt::vr_reload_pouch_nail),
        header("Load Point"),
        row(opt::vr_reload_show_ports),
        row(opt::vr_reload_port_nail_x),
        row(opt::vr_reload_port_nail_y),
        row(opt::vr_reload_port_nail_z),
        row(opt::vr_reload_port_nail_radius),
        header("The Magazine's Top"),
        row(opt::vr_reload_mag_nail_x),
        row(opt::vr_reload_mag_nail_y),
        row(opt::vr_reload_mag_nail_z),
        row(opt::vr_reload_mag_nail_radius),
        open("Magazine Pose in the Hand: Held Object Offsets", pageIndex(pageHeldObjectOffsets))
            .help("How a magazine out of the gun sits in the hand: take one from the pouch, then tune it there."),
        header("Receiver (looks only)"),
        row(opt::vr_reload_well_nail_x),
        row(opt::vr_reload_well_nail_y),
        row(opt::vr_reload_well_nail_z),
        row(opt::vr_reload_well_nail_pitch),
        row(opt::vr_reload_well_nail_yaw),
        row(opt::vr_reload_well_nail_roll),
        header("Holding the Attached Magazine"),
        row(opt::vr_reload_hold_nail_x),
        row(opt::vr_reload_hold_nail_y),
        row(opt::vr_reload_hold_nail_z),
        row(opt::vr_reload_hold_nail_pitch),
        row(opt::vr_reload_hold_nail_yaw),
        row(opt::vr_reload_hold_nail_roll),
        row(opt::vr_reload_hold_nail_fingers),
        row(opt::vr_reload_hold_nail_curl_thumb),
        row(opt::vr_reload_hold_nail_curl_index),
        row(opt::vr_reload_hold_nail_curl_middle),
        row(opt::vr_reload_hold_nail_curl_ring),
        row(opt::vr_reload_hold_nail_curl_pinky),
        header("Pull and Knock Out (All Magazines)"),
        row(opt::vr_reload_pull_reach),
        row(opt::vr_reload_bump_speed),
        row(opt::vr_reload_hit_reach),
    };
}

// Weapons > Reloading > Super Nailgun: its well, its magazine, holding and pulling it.
[[nodiscard]] za::Vector<Item> pageReloadSuperNailgun()
{
    return {
        header("At the Pouch"),
        row(opt::vr_reload_pouch_snail),
        header("Load Point"),
        row(opt::vr_reload_show_ports),
        row(opt::vr_reload_port_snail_x),
        row(opt::vr_reload_port_snail_y),
        row(opt::vr_reload_port_snail_z),
        row(opt::vr_reload_port_snail_radius),
        header("The Magazine's Top"),
        row(opt::vr_reload_mag_snail_x),
        row(opt::vr_reload_mag_snail_y),
        row(opt::vr_reload_mag_snail_z),
        row(opt::vr_reload_mag_snail_radius),
        open("Magazine Pose in the Hand: Held Object Offsets", pageIndex(pageHeldObjectOffsets))
            .help("How a magazine out of the gun sits in the hand: take one from the pouch, then tune it there."),
        header("Receiver (looks only)"),
        row(opt::vr_reload_well_snail_x),
        row(opt::vr_reload_well_snail_y),
        row(opt::vr_reload_well_snail_z),
        row(opt::vr_reload_well_snail_pitch),
        row(opt::vr_reload_well_snail_yaw),
        row(opt::vr_reload_well_snail_roll),
        header("Holding the Attached Magazine"),
        row(opt::vr_reload_hold_snail_x),
        row(opt::vr_reload_hold_snail_y),
        row(opt::vr_reload_hold_snail_z),
        row(opt::vr_reload_hold_snail_pitch),
        row(opt::vr_reload_hold_snail_yaw),
        row(opt::vr_reload_hold_snail_roll),
        row(opt::vr_reload_hold_snail_fingers),
        row(opt::vr_reload_hold_snail_curl_thumb),
        row(opt::vr_reload_hold_snail_curl_index),
        row(opt::vr_reload_hold_snail_curl_middle),
        row(opt::vr_reload_hold_snail_curl_ring),
        row(opt::vr_reload_hold_snail_curl_pinky),
        header("Pull and Knock Out (All Magazines)"),
        row(opt::vr_reload_pull_reach),
        row(opt::vr_reload_bump_speed),
        row(opt::vr_reload_hit_reach),
    };
}

// Weapons > Reloading > Thunderbolt: its well, its cell, holding and pulling it.
[[nodiscard]] za::Vector<Item> pageReloadThunderbolt()
{
    return {
        header("At the Pouch"),
        row(opt::vr_reload_pouch_light),
        header("Load Point"),
        row(opt::vr_reload_show_ports),
        row(opt::vr_reload_port_light_x),
        row(opt::vr_reload_port_light_y),
        row(opt::vr_reload_port_light_z),
        row(opt::vr_reload_port_light_radius),
        header("The Magazine's Top"),
        row(opt::vr_reload_mag_light_x),
        row(opt::vr_reload_mag_light_y),
        row(opt::vr_reload_mag_light_z),
        row(opt::vr_reload_mag_light_radius),
        header("Sparks and Smoke"),
        row(opt::vr_reload_battery_sparks),
        row(opt::vr_reload_battery_smoke_time),
        open("Cell Pose in the Hand: Held Object Offsets", pageIndex(pageHeldObjectOffsets))
            .help("How a cell out of the gun sits in the hand: take one from the pouch, then tune it there."),
        header("Receiver (looks only)"),
        row(opt::vr_reload_well_light_x),
        row(opt::vr_reload_well_light_y),
        row(opt::vr_reload_well_light_z),
        row(opt::vr_reload_well_light_pitch),
        row(opt::vr_reload_well_light_yaw),
        row(opt::vr_reload_well_light_roll),
        header("Holding the Attached Cell"),
        row(opt::vr_reload_hold_light_x),
        row(opt::vr_reload_hold_light_y),
        row(opt::vr_reload_hold_light_z),
        row(opt::vr_reload_hold_light_pitch),
        row(opt::vr_reload_hold_light_yaw),
        row(opt::vr_reload_hold_light_roll),
        row(opt::vr_reload_hold_light_fingers),
        row(opt::vr_reload_hold_light_curl_thumb),
        row(opt::vr_reload_hold_light_curl_index),
        row(opt::vr_reload_hold_light_curl_middle),
        row(opt::vr_reload_hold_light_curl_ring),
        row(opt::vr_reload_hold_light_curl_pinky),
        header("Pull and Knock Out (All Magazines)"),
        row(opt::vr_reload_pull_reach),
        row(opt::vr_reload_bump_speed),
        row(opt::vr_reload_hit_reach),
    };
}

// Weapons > Reloading > Heavy Revolver (QC vr_revolver.qc): its cylinder swung out and home, its shells dropped, spun.
[[nodiscard]] za::Vector<Item> pageReloadRevolver()
{
    return {
        header("Swinging It Out"),
        row(opt::vr_revolver_open_flick),
        row(opt::vr_revolver_open_flick_speed),
        row(opt::vr_revolver_open_flick_angle),
        row(opt::vr_revolver_open_flick_time),
        row(opt::vr_revolver_open_button),
        header("Dropping the Shells"),
        row(opt::vr_revolver_eject_rod),
        row(opt::vr_revolver_eject_rod_reach),
        row(opt::vr_revolver_eject_rod_speed),
        header("Spinning It"),
        row(opt::vr_revolver_spin),
        row(opt::vr_revolver_spin_speed),
        row(opt::vr_revolver_spin_hold),
        header("Swinging It Home"),
        row(opt::vr_revolver_close_flick_speed),
        row(opt::vr_revolver_close_flick_angle),
        row(opt::vr_revolver_close_push),
        row(opt::vr_revolver_close_button),
        header("Load Point"),
        toggle("Show Load Points", "vr_reload_show_ports")
            .help("Draws each held gun's load point (yellow) and its radius (green), to tune the offsets live."),
        row(opt::vr_reload_port_revolver_x),
        row(opt::vr_reload_port_revolver_y),
        row(opt::vr_reload_port_revolver_z),
        row(opt::vr_reload_port_revolver_radius),
    };
}

// Weapons > Reloading > Nail Pistol: its well (the grip's bottom), its magazine, holding and pulling it.
[[nodiscard]] za::Vector<Item> pageReloadNailPistol()
{
    return {
        header("At the Pouch"),
        row(opt::vr_reload_pouch_npistol),
        header("Load Point"),
        row(opt::vr_reload_show_ports),
        row(opt::vr_reload_port_npistol_x),
        row(opt::vr_reload_port_npistol_y),
        row(opt::vr_reload_port_npistol_z),
        row(opt::vr_reload_port_npistol_radius),
        header("The Magazine's Top"),
        row(opt::vr_reload_mag_npistol_x),
        row(opt::vr_reload_mag_npistol_y),
        row(opt::vr_reload_mag_npistol_z),
        row(opt::vr_reload_mag_npistol_radius),
        open("Magazine Pose in the Hand: Held Object Offsets", pageIndex(pageHeldObjectOffsets))
            .help("How a magazine out of the gun sits in the hand: take one from the pouch, then tune it there."),
        header("Holding the Attached Cell"),
        row(opt::vr_reload_hold_npistol_x),
        row(opt::vr_reload_hold_npistol_y),
        row(opt::vr_reload_hold_npistol_z),
        row(opt::vr_reload_hold_npistol_pitch),
        row(opt::vr_reload_hold_npistol_yaw),
        row(opt::vr_reload_hold_npistol_roll),
        row(opt::vr_reload_hold_npistol_fingers),
        row(opt::vr_reload_hold_npistol_curl_thumb),
        row(opt::vr_reload_hold_npistol_curl_index),
        row(opt::vr_reload_hold_npistol_curl_middle),
        row(opt::vr_reload_hold_npistol_curl_ring),
        row(opt::vr_reload_hold_npistol_curl_pinky),
        header("Pull and Knock Out (All Magazines)"),
        row(opt::vr_reload_pull_reach),
        row(opt::vr_reload_bump_speed),
        row(opt::vr_reload_hit_reach),
    };
}

// Weapons > Reloading > Widowmaker (AD support: the super shotgun with AD's upgrade; QC vr_ad_bridge.qc AD_FireWidowmaker):
// an auto shotgun, its tube loaded by the handful at the shell port on its receiver's inner side.
[[nodiscard]] za::Vector<Item> pageReloadWidowmaker()
{
    return {
        header("Barrels"),
        row(opt::vr_ad_widowmaker_motor),
        header("Load Point"),
        toggle("Show Load Points", "vr_reload_show_ports")
            .help("Draws each held gun's load point (yellow) and its radius (green), to tune the offsets live."),
        row(opt::vr_reload_port_widow_x),
        row(opt::vr_reload_port_widow_y),
        row(opt::vr_reload_port_widow_z),
        row(opt::vr_reload_port_widow_radius),
    };
}

// Weapons > Reloading > Plasma Gun (AD support: the thunderbolt with AD's upgrade): its cell's well on its inner side.
[[nodiscard]] za::Vector<Item> pageReloadPlasmaGun()
{
    return {
        header("Load Point"),
        toggle("Show Load Points", "vr_reload_show_ports")
            .help("Draws each held gun's load point (yellow) and its radius (green), a held magazine's reference point and "
                  "radius (orange), and an attached magazine's box (blue), to tune the offsets live."),
        row(opt::vr_reload_port_adplasma_x),
        row(opt::vr_reload_port_adplasma_y),
        row(opt::vr_reload_port_adplasma_z),
        row(opt::vr_reload_port_adplasma_radius),
        header("Receiver (looks only)"),
        row(opt::vr_reload_well_adplasma_x),
        row(opt::vr_reload_well_adplasma_y),
        row(opt::vr_reload_well_adplasma_z),
        row(opt::vr_reload_well_adplasma_pitch),
        row(opt::vr_reload_well_adplasma_yaw),
        row(opt::vr_reload_well_adplasma_roll),
        open("The Cell, Holding It, Pulling It: Thunderbolt", pageIndex(pageReloadThunderbolt))
            .help("The cell is the thunderbolt's: its top, its sparks and smoke, the hand holding it and the pull are there."),
    };
}

// Weapons > Reloading > Launchers: the grenade, proximity and rocket launchers loaded at the muzzle (QC vr_reload.qc,
// "Front-loaded launchers"), each muzzle's load point.
[[nodiscard]] za::Vector<Item> pageReloadLaunchers()
{
    return {
        row(opt::vr_reload_front),
        row(opt::vr_reload_front_angle),
        row(opt::vr_reload_front_hold_pitch),
        open("Round Pose: Held Object Offsets", pageIndex(pageHeldObjectOffsets))
            .help("How a rocket or grenade sits in the hand when taken from where it lies (its Grip). From the ammo pouch, "
                  "every round sits the same way, in the fist's grip channel, turned by Round In Hand above."),
        row(opt::vr_prox_scale),
        row(opt::vr_nail_scale),
        toggle("Show Load Points", "vr_reload_show_ports")
            .help("Draws each held gun's load point (yellow) and its radius (green), to tune the offsets live."),
        header("Grenade Launcher"),
        row(opt::vr_reload_port_gl_x),
        row(opt::vr_reload_port_gl_y),
        row(opt::vr_reload_port_gl_z),
        row(opt::vr_reload_port_gl_radius),
        header("Proximity Launcher"),
        row(opt::vr_reload_port_prox_x),
        row(opt::vr_reload_port_prox_y),
        row(opt::vr_reload_port_prox_z),
        row(opt::vr_reload_port_prox_radius),
        header("Rocket Launcher"),
        row(opt::vr_reload_port_rl_x),
        row(opt::vr_reload_port_rl_y),
        row(opt::vr_reload_port_rl_z),
        row(opt::vr_reload_port_rl_radius),
    };
}

// Weapons > Reloading > Cord Points: where the cell cords leave their weapons (vr_cellcord.cpp).
[[nodiscard]] za::Vector<Item> pageReloadCellCords()
{
    return {
        command("Cell Cord Info", "vr_cellcord_info")
            .help("vr_cellcord_info: in the console, each cord's weapon, its ends (the weapon's, the cell's), loose or "
                  "plugged in, its length and rings."),
    };
}

// Weapons > Reloading > Per-Weapon Reload Overrides: each gun's own reloading mode (vr_reload_mode_<gun>), or the global one.
[[nodiscard]] za::Vector<Item> pageReloadOverrides()
{
    const auto mode = [](const char* label, const char* cvar) {
        return cycle(label, cvar, {{-1.f, "Use Global"}, {3.f, "Immersive"}, {2.f, "Simple"}, {1.f, "Simple (all holsters)"}, {0.f, "Disabled"}})
            .help("This gun's reloading mode. Use Global: Reloading's mode. Immersive: loaded by hand (the shotgun a "
                  "shell at a time, the nailguns and the thunderbolt a magazine, the launchers a round at the muzzle), "
                  "from the ammo pouch. Simple: reloaded at a hip holster. Simple (all holsters): at any holster. "
                  "Disabled: it fires from your ammo, no magazine. Needs Weapon Mode Immersive.");
    };
    za::Vector<Item> list{
        header("Per-Weapon Reload Overrides"),
        mode("Shotgun", "vr_reload_mode_shot"),
        mode("Super Shotgun", "vr_reload_mode_sshot"),
        mode("Nailgun", "vr_reload_mode_nail"),
        mode("Super Nailgun", "vr_reload_mode_snail"),
        mode("Grenade Launcher", "vr_reload_mode_gl"),
        mode("Rocket Launcher", "vr_reload_mode_rl"),
        mode("Thunderbolt", "vr_reload_mode_light"),
        mode("Nail Pistol", "vr_reload_mode_npistol"),
        mode("Heavy Revolver", "vr_reload_mode_revolver"),
        mode("Widowmaker (Arcane Dimensions)", "vr_reload_mode_widow"),
        mode("Plasma Gun (Arcane Dimensions)", "vr_reload_mode_adplasma"),
    };
    // (Hipnotic's proximity gun only where Hipnotic is installed.)
    const cvar_t* hipnotic = Cvar_FindVar("vr_hipnotic_available");
    if(hipnotic && hipnotic->value != 0.f)
    {
        list.pushBack(mode("Proximity Gun (Hipnotic)", "vr_reload_mode_prox"));
    }
    return list;
}

// Weapons > Reloading (General): the mode, a page per gun, the ammo pouch, what is the same for every gun
// (docs/vr-port/RELOAD.md; QC vr_reload.qc).
[[nodiscard]] za::Vector<Item> pageReloading()
{
    return {
        cycle("Reloading", "vr_reload_mode", {{0.f, "Off"}, {1.f, "Holsters (All)"}, {2.f, "Holsters (Hips)"}, {3.f, "Immersive"}})
            .help("Off: the guns fire from your ammo, no magazines. Holsters: a gun held at a holster (any, or the hips') "
                  "reloads. Immersive: guns are loaded by hand from the ammo pouch on the front of your belt (it gives "
                  "what the gun in your other hand takes): the shotgun a shell at a time into the port under it; the "
                  "nailgun, the super nailgun and the thunderbolt a magazine into its well (out by B/Y, a hard pull, or "
                  "bumped out by the next); the grenade, proximity and rocket launchers a round at a time butt first into "
                  "the muzzle; the other guns reload at the hip holsters for now. A gun brought to the pouch by its port "
                  "or well reloads there too (Reloading at the Pouch: two guns, no free hand). Needs Weapon Mode "
                  "Immersive."),
        open("Per-Weapon Reload Overrides", pageIndex(pageReloadOverrides))
            .help("A reloading mode of its own for each gun (or Use Global): the shotgun loaded by hand while the nailguns reload at the hips, say."),
        header("Weapons"),
        open("Shotgun", pageIndex(pageReloadShotgun)).help("Its shells (pairs, their pose) and its loading port."),
        open("Super Shotgun", pageIndex(pageReloadSuperShotgun)).help("Broken open: each way to open and close it, their thresholds; its breech."),
        open("Nailgun", pageIndex(pageReloadNailgun)).help("Its well, its magazine's top, its receiver, the hand holding the magazine, pulling it out."),
        open("Super Nailgun", pageIndex(pageReloadSuperNailgun)).help("Its well, its magazine's top, its receiver, the hand holding the magazine, pulling it out."),
        open("Thunderbolt", pageIndex(pageReloadThunderbolt)).help("Its well, its cell's top, its receiver, the hand holding the cell, pulling it out."),
        open("Nail Pistol", pageIndex(pageReloadNailPistol)).help("Its well (the grip's bottom), its magazine's top, the hand holding the magazine, pulling it out."),
        open("Heavy Revolver", pageIndex(pageReloadRevolver)).help("Its cylinder swung out (the flick: its force and wrist flick; B/Y) and home, its shells dropped (the muzzle up, the ejector), spun, its load point."),
        open("Launchers", pageIndex(pageReloadLaunchers)).help("The grenade, proximity and rocket launchers loaded at the muzzle: on or off, the round's angle, each muzzle's load point."),
        open("Widowmaker (Arcane Dimensions)", pageIndex(pageReloadWidowmaker)).help("Its tube, the handful of shells the pouch gives for it, its fire rate, its shell port's load point."),
        open("Plasma Gun (Arcane Dimensions)", pageIndex(pageReloadPlasmaGun)).help("Its cell's well on its inner side: the load point and the receiver (the cell itself: the Thunderbolt's page)."),
        header("Cell Cords"),
        row(opt::vr_cellcord_laser),
        row(opt::vr_cellcord_hammer),
        row(opt::vr_cellcord_superaxe),
        open("Cord Points", pageIndex(pageReloadCellCords))
            .help("Where each cord leaves its weapon (off its lowest point), to tune."),
        header("All Guns"),
        row(opt::vr_reload_reinsert_clearance),
        row(opt::vr_reload_reinsert_time),
        row(opt::vr_reload_mag_release),
        row(opt::vr_reload_insert_time),
        row(opt::vr_reload_show_ports),
        row(opt::vr_reload_collide_leniency),
        row(opt::vr_reload_mag_collide),
        row(opt::vr_reload_contact),
        row(opt::vr_reload_contact_leniency),
        row(opt::vr_reload_eject_cooldown),
        row(opt::vr_reload_eject_button),
        header("Reloading at the Pouch"),
        toggle("Shotgun", "vr_reload_pouch_shot")
            .help("Its port brought down onto the shells in the ammo pouch, upright: a shell a touch."),
        toggle("Super Shotgun", "vr_reload_pouch_sshot")
            .help("Broken open, its open barrels touched to the shells in the pouch at any angle: its chambers fill. Its "
                  "page has its own contact and angle."),
        toggle("Nailgun", "vr_reload_pouch_nail")
            .help("Its well empty, brought down onto the magazines in the pouch along them: a fresh one seats."),
        toggle("Super Nailgun", "vr_reload_pouch_snail")
            .help("Its well empty, brought onto the magazines in the pouch along them (the gun on its side): a fresh one "
                  "seats."),
        toggle("Thunderbolt", "vr_reload_pouch_light")
            .help("Its well empty, brought down onto the cells in the pouch along them: a fresh one seats."),
        toggle("Nail Pistol", "vr_reload_pouch_npistol")
            .help("Its grip empty, brought down onto the magazines in the pouch along them: a fresh one seats."),
        row(opt::vr_reload_pouch_mag_slots),
        header("Ammo Pouch"),
        row(opt::vr_ammo_pouch_show),
        row(opt::vr_ammo_pouch_x),
        row(opt::vr_ammo_pouch_y),
        row(opt::vr_ammo_pouch_z),
        row(opt::vr_ammo_pouch_thresh),
        row(opt::vr_ammo_pouch_pitch),
        row(opt::vr_ammo_pouch_yaw),
        row(opt::vr_ammo_pouch_roll),
        row(opt::vr_ammo_pouch_scale),
        row(opt::vr_ammo_pouch_counter),
        row(opt::vr_ammo_screen_text_white),
        row(opt::vr_ammo_pouch_leg_follow),
        // (VR Settings had these until the author's note of 2026-10-22: "too fine-grained" for the basic settings.)
        header("Ammo Pouch Wobble"),
        row(opt::vr_ammo_pouch_wobble_each),
    };
}

[[nodiscard]] za::Vector<Item> pageGraphicsLights();
[[nodiscard]] za::Vector<Item> pageGraphicsShadows();
[[nodiscard]] za::Vector<Item> pageGraphicsSurfaces();
[[nodiscard]] za::Vector<Item> pageGraphicsLiquids();
[[nodiscard]] za::Vector<Item> pageGraphicsFoam();
[[nodiscard]] za::Vector<Item> pageGraphicsTeleporters();
[[nodiscard]] za::Vector<Item> pageGraphicsPost();
[[nodiscard]] za::Vector<Item> pageGraphicsModels();
[[nodiscard]] za::Vector<Item> pageGraphicsLootBeams();
[[nodiscard]] za::Vector<Item> pageGraphicsRetro();
[[nodiscard]] za::Vector<Item> pageGraphicsRetroLight();
[[nodiscard]] za::Vector<Item> pageGraphicsRelighting();
[[nodiscard]] za::Vector<Item> pageTransparencyOptions();
[[nodiscard]] za::Vector<Item> pageRecording(); // vr_menu_recording.inc

[[nodiscard]] za::Vector<Item> pageGraphics();

// VR Settings > Graphics Presets (vr_gfxpresets.cpp): the presets, My Settings, and which the settings match now.
[[nodiscard]] za::Vector<Item> pageGraphicsPresets()
{
    return {
        info(gfxpresets::menuStateLine),
        info(gfxpresets::menuNoteLine),
        header("Presets"),
        action("Off (Quake)", []() { gfxpresets::menuApply(0); })
            .help("Quake's own look: flat dynamic lights, no shadows, bumps, ambient occlusion, bloom or tone mapping, "
                  "plain liquids, no teleporter views. The least GPU time."),
        action("Low", []() { gfxpresets::menuApply(1); })
            .help("For weaker graphics cards: Quake VR's lighting and look with few shadows (2 dynamic lights, none from "
                  "the map), no bumps, parallax, ambient occlusion, refraction, teleporter views or heat haze. About a "
                  "third of High's GPU time."),
        action("Medium", []() { gfxpresets::menuApply(2); })
            .help("Most of it at about half High's GPU time: bumps, parallax (without its depth: things meet the wall's "
                  "surface, not its carved relief), ambient occlusion, every liquid effect and teleporter views, with "
                  "fewer and smaller shadows (4 dynamic lights, 2 map lights)."),
        action("High (as shipped)", []() { gfxpresets::menuApply(3); })
            .help("The game's defaults: everything on, 8 shadowed dynamic lights and 4 map lights out to 3072 units."),
        action("Ultra", []() { gfxpresets::menuApply(4); })
            .help("Past the defaults, for fast graphics cards: 12 shadowed dynamic lights and 6 map lights out to 4096 "
                  "units, more torch lights, particles at full resolution, the finer bloom, deeper parallax steps."),
        header("Your Own"),
        action("My Settings", gfxpresets::menuRestoreMine)
            .help("Your own graphics settings back, exactly as they were before you first picked a preset (or as you "
                  "last saved them). Switching between presets never changes them."),
        action("Save as My Settings", gfxpresets::menuSaveMine)
            .help("Keep the graphics settings as they are now as My Settings, in place of the ones kept. Press it twice."),
        open("Every Graphics Setting", pageIndex(pageGraphics))
            .help("Each setting on its own page (Advanced VR Options > Graphics): change any after picking a preset; "
                  "the line above then says Custom."),
    };
}

// Graphics, in pages: this one, the preset and the image, and links to the others (Memory Log is on Debug).
[[nodiscard]] za::Vector<Item> pageGraphics()
{
    return {
        info(gfxpresets::menuStateLine),
        open("Presets", pageIndex(pageGraphicsPresets))
            .help("Off (Quake), Low, Medium, High (as shipped) and Ultra set the settings on the Graphics pages at once; My Settings sets yours back. Also on VR Settings."),
        header("Image"),
        row(opt::vid_fsaa),
        row(opt::vr_texture_smooth),
        row(opt::gl_texture_anisotropy),
        row(opt::vr_alpha_coverage),
        row(opt::vr_relit_maps),
        open("Relighting", pageIndex(pageGraphicsRelighting)).help("Relight the map you are in, with brighter or dimmer lamps, glowing panels, lava, bounced light and ambient occlusion, and see it at once."),
        row(opt::vr_gamma),
        row(opt::vr_contrast),
        header("More Graphics"),
        open("Lights", pageIndex(pageGraphicsLights)),
        open("Shadows", pageIndex(pageGraphicsShadows)),
        open("Surfaces", pageIndex(pageGraphicsSurfaces)),
        open("Liquids", pageIndex(pageGraphicsLiquids)),
        open("Teleporters", pageIndex(pageGraphicsTeleporters)),
        open("Post-processing", pageIndex(pageGraphicsPost)),
        open("Models and Effects", pageIndex(pageGraphicsModels)),
        open("Loot Beams", pageIndex(pageGraphicsLootBeams)).help("A soft pillar of light over weapons, magazines and (if you want) other things to pick up lying about, so they stand out among corpses and gibs."),
        open("Retro Textures", pageIndex(pageGraphicsRetro)),
        open("Retro Lighting", pageIndex(pageGraphicsRetroLight)),
        open("Particles", pageIndex(pageParticleSettings)),
        open("Transparency", pageIndex(pageTransparencyOptions)),
        open("Recording (Window View)", pageIndex(pageRecording)),
        header("Performance"),
        row(opt::vr_profile),
        row(opt::vr_gadget_fps),
    };
}

// Graphics: the light, dynamic lights and the things that make them.
[[nodiscard]] za::Vector<Item> pageGraphicsLights()
{
    return {
        row(opt::vr_light_contrast),
        row(opt::vr_ambient_light),
        row(opt::vr_colored_lights),
        row(opt::vr_dlight_falloff),
        row(opt::vr_dlight_uncapped),
        row(opt::vr_dlight_models),
        row(opt::vr_dlight_angle),
        header("Light Sources"),
        row(opt::vr_flash_scale),
        row(opt::vr_explosion_light_scale),
        row(opt::vr_projectile_lights),
        row(opt::vr_lavanail_lights),
        row(opt::vr_lavagun_light),
        row(opt::vr_lavagun_light_radius),
        row(opt::vr_lavagun_light_flicker),
        row(opt::vr_lavagun_light_idle),
        row(opt::vr_beam_lights),
        row(opt::vr_flame_lights),
        row(opt::vr_flame_light_scale),
        row(opt::vr_beam_arcs),
        row(opt::vr_beam_arcs_spread),
        row(opt::vr_beam_arcs_width),
        row(opt::vr_torch_lights),
        row(opt::vr_torch_light_scale),
        row(opt::vr_torch_light_shadows),
        row(opt::vr_weapon_screen_light),
    };
}

// Graphics: shadows, and the blob shadows standing in for them.
[[nodiscard]] za::Vector<Item> pageGraphicsShadows()
{
    return {
        row(opt::vr_shadow_dlights),
        row(opt::vr_shadow_dlight_size),
        row(opt::vr_shadow_muzzleflash),
        row(opt::vr_shadow_maplights),
        row(opt::vr_shadow_maplight_size),
        row(opt::vr_shadow_maplight_strength),
        row(opt::vr_shadow_self),
        row(opt::vr_shadow_filter),
        row(opt::vr_shadow_distance),
        row(opt::vr_shadow_atlas),
        row(opt::vr_shadow_atlas_grow),
        row(opt::vr_shadow_bias),
        row(opt::vr_shadow_stats),
        header("Ambient Occlusion"),
        row(opt::vr_ao_dynamic),
        row(opt::vr_ao_dynamic_range),
        row(opt::vr_ao_brush),
        row(opt::vr_ao_models),
        header("Blob Shadows"),
        row(opt::vr_blob_shadows),
        row(opt::vr_player_shadows),
        row(opt::vr_entity_shadows),
    };
}

// Graphics: the walls' and models' surfaces: sheen, bumps, parallax, detail.
[[nodiscard]] za::Vector<Item> pageGraphicsSurfaces()
{
    return {
        row(opt::vr_specular),
        row(opt::vr_specular_aa),
        header("Bumps"),
        row(opt::vr_normalmaps),
        row(opt::vr_normalmap_strength),
        row(opt::vr_normalmap_baked),
        row(opt::vr_deluxemap),
        row(opt::vr_normalmap_models),
        row(opt::vr_normalmap_authored),
        header("Parallax"),
        row(opt::vr_parallax),
        row(opt::vr_parallax_depth),
        row(opt::vr_parallax_distance),
        row(opt::vr_parallax_refine),
        row(opt::vr_parallax_grazing),
        row(opt::vr_parallax_depth_write),
        row(opt::vr_parallax_items),
        row(opt::vr_parallax_models),
        row(opt::vr_parallax_authored),
        header("External Maps"),
        row(opt::vr_extmaps),
        row(opt::vr_extmaps_normals),
        row(opt::vr_extmaps_spec),
        row(opt::vr_extmaps_spec_scale),
        row(opt::vr_extmaps_luma),
        row(opt::vr_extmaps_match),
        header("Detail Textures"),
        row(opt::vr_detail),
        row(opt::vr_detail_strength),
        row(opt::vr_detail_distance),
    };
}

// Graphics: retro textures (vr_retro.cpp): high-resolution textures drawn in Quake's chunky look, each kind of thing
// with its own settings (a page each: pageRetroCategory).
[[nodiscard]] za::Vector<Item> pageRetroOverride();
[[nodiscard]] za::Vector<Item> pageRetroAll();

[[nodiscard]] za::Vector<Item> pageGraphicsRetro()
{
    using retro::Category;
    za::Vector<Item> items{
        row(opt::vr_retro),
    };
    const auto kind = [&items](Category c, int page) { items.pushBack(open(retro::categoryLabel(c), page).help(retro::categoryHelp(c))); };
    kind(Category::World, pageIndex(pageRetroCategory<Category::World>));
    kind(Category::Brush, pageIndex(pageRetroCategory<Category::Brush>));
    kind(Category::Items, pageIndex(pageRetroCategory<Category::Items>));
    kind(Category::Props, pageIndex(pageRetroCategory<Category::Props>));
    kind(Category::Gibs, pageIndex(pageRetroCategory<Category::Gibs>));
    kind(Category::SmallGibs, pageIndex(pageRetroCategory<Category::SmallGibs>));
    kind(Category::Weapons, pageIndex(pageRetroCategory<Category::Weapons>));
    kind(Category::Held, pageIndex(pageRetroCategory<Category::Held>));
    kind(Category::Monsters, pageIndex(pageRetroCategory<Category::Monsters>));
    kind(Category::Hands, pageIndex(pageRetroCategory<Category::Hands>));
    kind(Category::Arms, pageIndex(pageRetroCategory<Category::Arms>));
    kind(Category::Torso, pageIndex(pageRetroCategory<Category::Torso>));
    kind(Category::Legs, pageIndex(pageRetroCategory<Category::Legs>));
    kind(Category::Gear, pageIndex(pageRetroCategory<Category::Gear>));
    kind(Category::Decals, pageIndex(pageRetroCategory<Category::Decals>));
    kind(Category::Particles, pageIndex(pageRetroCategory<Category::Particles>));
    kind(Category::Sprites, pageIndex(pageRetroCategory<Category::Sprites>));
    kind(Category::Other, pageIndex(pageRetroCategory<Category::Other>));
    items.pushBack(header("All Kinds"));
    items.pushBack(open("All Categories", pageIndex(pageRetroAll))
        .help("One set of values for every kind at once (or the ones you check), only the settings you check: a consistent "
              "look to try. Copy From takes a kind's values to start from; Live writes each change as you make it."));
    items.pushBack(command("Reset All to Defaults", "vr_retro_reset all").help("vr_retro_reset all: every kind's settings back to their defaults."));
    items.pushBack(header("One Thing: Overrides"));
    items.pushBack(command("Pick: Point, 3 Seconds", "vr_retro_pick hand 3")
        .help("Closes the menu; point your main hand at a model or a wall: what it meets is outlined, and after 3 seconds "
              "its override page opens. A model (a crate, a monster, a gun) or a world texture gets settings of its own "
              "on top of its kind's: stronger on a high-resolution prop, off on one. vr_retro_pick [hand|head] [seconds]."));
    items.pushBack(command("Pick: Look, 3 Seconds", "vr_retro_pick head 3")
        .help("As Pick: Point, with what you look at (the middle of your view)."));
    items.pushBack(open("Edit the Last Picked", pageIndex(pageRetroOverride))
        .help("The override page of what you picked last."));
    items.pushBack(command("List Overrides", "vr_retro_override list")
        .help("vr_retro_override list: the shipped overrides (quakevr/retro_overrides_default.txt) and yours "
              "(quakevr/retro_overrides.txt), to the console."));
    items.pushBack(command("Reload the Override Files", "vr_retro_overrides_reload")
        .help("Reads both files again (after editing them by hand)."));
    return items;
}

[[nodiscard]] const char* retroTargetText()
{
    return retro::targetText();
}

// The retro textures' Smooth Beyond: its leftmost step (any value below 0, stored -1) "Never": no smoothing at any
// distance (vr_retro.cpp fillSet, RetroBegin).
[[nodiscard]] Item neverSlider(Item item)
{
    item.negativeLabel = "Never";
    return item;
}

// Graphics > Retro Textures > Override: the picked model's or texture's override, a mode and a value for each setting
// (Inherit: its kind's; Set: this value; Times: its kind's times this value). Saved to quakevr/retro_overrides.txt a
// second after the last change.
[[nodiscard]] za::Vector<Item> pageRetroOverride()
{
    using retro::Param;
    struct Row
    {
        Param p;
        const char* label;
        float min, max, step;
        const char* format;
        bool toggle;
    };
    const Row rows[] = {
        {Param::On, "On", 0.f, 1.f, 1.f, "", true},
        {Param::Snap, "Snap to Blocks", 0.f, 1.f, 1.f, "", true},
        {Param::Block, "Block Size", 0.25f, 8.f, 0.25f, "%.2f", false},
        {Param::Units, "Block Size in World Units", 0.f, 1.f, 1.f, "", true},
        {Param::Average, "Block Colour: Average", 0.f, 1.f, 1.f, "", true},
        {Param::Soft, "Edge Softness", 0.f, 3.f, 0.25f, "%.2f", false},
        {Param::Fade, "Smooth Beyond", -0.25f, 4.f, 0.25f, "%.2f", false},
        {Param::Palette, "Quake Palette", 0.f, 1.f, 0.05f, "%.2f", false},
        {Param::Dither, "Dither", 0.f, 2.f, 0.05f, "%.2f", false},
        {Param::DitherScale, "Dither Size", 1.f, 8.f, 1.f, "%.0f", false},
        {Param::Bump, "Bumps: Smooth to Blocky", 0.f, 1.f, 0.05f, "%.2f", false},
        {Param::Detail, "Detail Textures", 0.f, 1.f, 0.05f, "%.2f", false},
    };
    za::Vector<Item> items{info(retroTargetText)};
    if(retro::targetHasBoth())
    {
        items.pushBack(cycle("Override For", retro::editKindCvar(), {{0.f, "The Model"}, {1.f, "The Texture"}})
            .help("A brush model (an ammo box, an explosive box) can be overridden as a whole (by its model's name) or "
                  "by the texture you pointed at (everywhere that texture is: the world, other boxes)."));
    }
    if(retro::hasTarget())
    {
        for(const Row& r : rows)
        {
            items.pushBack(header(r.label));
            items.pushBack(cycle("Mode", retro::editMode(r.p), {{0.f, "Inherit"}, {1.f, "Set"}, {2.f, "Times"}})
                .help("Inherit: the kind's setting. Set: the value below instead. Times: the kind's setting times the value "
                      "below (a toggle: both on)."));
            if(r.toggle)
            {
                items.pushBack(toggle("Value", retro::editValue(r.p)));
            }
            else
            {
                const Item value = slider("Value", retro::editValue(r.p), r.min, r.max, r.step, r.format).extend(0.f, 64.f);
                items.pushBack(r.p == Param::Fade ? neverSlider(value) : value); // (Smooth Beyond: Never leftmost)
            }
        }
        items.pushBack(header("This Override"));
        items.pushBack(action("Use the Shipped One", retro::useShipped)
            .help("Removes your override for it: the shipped one (quakevr/retro_overrides_default.txt), if any, applies again."));
        items.pushBack(action("No Override", retro::clearOverride)
            .help("Your override for it inherits every setting (hides a shipped one)."));
        items.pushBack(action("Save Now", retro::saveNow).help("Writes quakevr/retro_overrides.txt now (it is written a second after each change anyway)."));
    }
    items.pushBack(command("Pick Again: Point, 3 Seconds", "vr_retro_pick hand 3"));
    items.pushBack(command("Pick Again: Look, 3 Seconds", "vr_retro_pick head 3"));
    return items;
}

// The retro textures settings after On (vr_retro.hpp's Param), each on the cvar var(p): a kind's page's and All
// Categories'.
template <typename Var, typename Has>
void retroSettingItems(za::Vector<Item>& items, const Var& var, const Has& has)
{
    using retro::Param;
    const struct
    {
        Param p; // a row shown where has(p) (Param::On for headers: always)
        Item item;
    } rows[] = {
        {Param::On, header("Blocks")},
        {Param::Snap,
            toggle("Snap to Blocks", var(Param::Snap))
                .help("The texture is read on a grid of blocks, each one colour. Off: as it was (only the palette and dither below).")},
        {Param::Block,
            slider("Block Size", var(Param::Block), 0.25f, 8.f, 0.25f, "%.2f").extend(0.0625f, 64.f)
                .help("How big a block is: in the texture's own Quake texels (1: Quake's own pixels: on the world a unit at the "
                      "usual scale, on a Quake model its skin's; on our own models their textures' pixels; 2: twice as chunky; 0.5: "
                      "finer than Quake), or in world units (Block Size In).")},
        {Param::Units,
            cycle("Block Size In", var(Param::Units), {{0.f, "Texture's Texels"}, {1.f, "World Units"}})
                .help("Texture's Texels: the blocks follow each texture's own pixels, as Quake drew it (a texture scaled up in the "
                      "map gets bigger blocks). World Units: the same size everywhere, whatever the texture's scale.")},
        {Param::Average,
            cycle("Block Colour", var(Param::Average), {{1.f, "Average"}, {0.f, "Centre Texel"}})
                .help("Average: each block is the average of the high-resolution texels under it (steady, Quake-like). Centre "
                      "Texel: the one texel at its centre (sharper, noisier).")},
        {Param::Soft,
            slider("Edge Softness", var(Param::Soft), 0.f, 3.f, 0.25f, "%.2f px").extend()
                .help("The blocks' edges are blended over this many pixels: 1 is crisp yet steady as your head moves; 0 hard "
                      "(crawls and shimmers in the headset); more is softer.")},
        {Param::Fade,
            neverSlider(slider("Smooth Beyond", var(Param::Fade), -0.25f, 4.f, 0.25f, "%.2f").extend(0.05f, 16.f))
                .help("Far off, where blocks get smaller than a pixel, the texture is drawn as usual (mipmapped) again: how "
                      "many blocks to a pixel it takes (it fades from half that). Lower: smooth sooner. Never (leftmost): "
                      "no smoothing at any distance, the blocks all the way (they shimmer far off).")},
        {Param::Palette,
            header("Palette")},
        {Param::Palette,
            slider("Quake Palette", var(Param::Palette), 0.f, 1.f, 0.05f, "%.2f")
                .help("How far the colours are pulled to Quake's own 256 (0 off, 1 only Quake's colours).")},
        {Param::Dither,
            slider("Dither", var(Param::Dither), 0.f, 2.f, 0.05f, "%.2f").extend()
                .help("An ordered dither before the palette, fixed to the blocks (steady, not on the screen): smoother "
                      "gradients in Quake's colours (needs Quake Palette).")},
        {Param::DitherScale,
            slider("Dither Size", var(Param::DitherScale), 1.f, 8.f, 1.f, "%.0f blocks").extend()
                .help("The dither pattern's cell, in blocks.")},
        {Param::Bump,
            header("Bumps")},
        {Param::Bump,
            slider("Bumps: Smooth to Blocky", var(Param::Bump), 0.f, 1.f, 0.05f, "%.2f")
                .help("The bump and specular maps: 0 smooth (the high-resolution ones), 1 snapped to the same blocks as the "
                      "colour, between: a blend.")},
    };
    for(const auto& r : rows)
    {
        if(has(r.p))
        {
            items.pushBack(r.item);
        }
    }
    if(has(Param::Detail))
    {
        items.pushBack(slider("Detail Textures", var(Param::Detail), 0.f, 1.f, 0.05f, "%.2f")
            .help("How much of the detail textures' fine grain (Graphics > Surfaces) stays on these: 0 none (flat blocks, as "
                  "Quake), 1 all of it. Brush models only (the boxes among Item Pickups and Props)."));
    }
}

// A retro textures category's settings (vr_retro.hpp's Param), the same on every kind's page.
[[nodiscard]] za::Vector<Item> retroCategoryItems(retro::Category c, void (*reset)())
{
    using retro::Param;
    const auto var = [c](Param p) -> cvar_t& { return retro::cvarOf(c, p); };
    za::Vector<Item> items{
        toggle(retro::categoryLabel(c), var(Param::On)).help("Retro textures on these (with Graphics > Retro Textures on)."),
    };
    retroSettingItems(items, var, [c](Param p) { return retro::supports(c, p); });
    items.pushBack(action("Reset to Defaults", reset).help("This kind's settings back to their defaults (vr_retro_reset <kind>)."));
    return items;
}


void retroAllCheckSettings()
{
    retro::allCheckSettings(true);
}

void retroAllUncheckSettings()
{
    retro::allCheckSettings(false);
}

void retroAllCheckCategories()
{
    retro::allCheckCategories(true);
}

void retroAllUncheckCategories()
{
    retro::allCheckCategories(false);
}

// Graphics > Retro Textures > All Categories (vr_retro.hpp's "All Categories"): one set of values, the settings of it to
// apply and the kinds to apply them to. Per-object overrides are untouched.
[[nodiscard]] za::Vector<Item> pageRetroAll()
{
    using retro::Category;
    using retro::Param;
    za::Vector<Choice> kinds;
    for(int c = 0; c < retro::categoryCount; c++)
    {
        kinds.pushBack({static_cast<float>(c), retro::categoryLabel(static_cast<Category>(c))});
    }
    za::Vector<Item> items{
        info(retro::allSummary),
        action("Apply to Checked Categories", retro::allApplyNow)
            .help("Writes the checked settings below (Apply These Settings) to every checked kind (To These Categories). The "
                  "kinds' pages show the result; per-object overrides stay. vr_retro_all_apply."),
        toggle("Live", retro::allLiveCvar())
            .help("While on, each change to a checked setting below is written to the checked kinds as you make it. Off "
                  "again each time the game starts (vr_retro_all_live)."),
        cycle("Copy From", retro::allFromCvar(), kinds).help("The kind whose values Copy Its Values takes."),
        action("Copy Its Values", retro::allCopyChosen)
            .help("The values below become that kind's (a starting point); nothing is applied (vr_retro_all_copy <kind>)."),
        header("Values"),
        toggle("On", retro::allValue(Param::On))
            .help("The kinds' own switches (unchecked by default under Apply These Settings, so your kinds stay on or off)."),
    };
    retroSettingItems(items, [](Param p) -> cvar_t& { return retro::allValue(p); }, [](Param) { return true; });
    struct Row
    {
        Param p;
        const char* label;
    };
    const Row rows[] = {
        {Param::On, "On"},
        {Param::Snap, "Snap"},
        {Param::Block, "Block Size"},
        {Param::Units, "Block Size In (Units)"},
        {Param::Average, "Block Colour (Average)"},
        {Param::Soft, "Edge Softness"},
        {Param::Fade, "Smooth Beyond"},
        {Param::Palette, "Quake Palette"},
        {Param::Dither, "Dither"},
        {Param::DitherScale, "Dither Size"},
        {Param::Bump, "Bumps"},
        {Param::Detail, "Detail Textures"},
    };
    items.pushBack(header("Apply These Settings"));
    for(const Row& r : rows)
    {
        items.pushBack(toggle(r.label, retro::allApply(r.p)).help("Checked: Apply (and Live) write this setting; unchecked: the kinds keep theirs."));
    }
    items.pushBack(action("Check All Settings", retroAllCheckSettings));
    items.pushBack(action("Uncheck All Settings", retroAllUncheckSettings));
    items.pushBack(header("To These Categories"));
    for(int c = 0; c < retro::categoryCount; c++)
    {
        items.pushBack(toggle(retro::categoryLabel(static_cast<Category>(c)), retro::allTarget(static_cast<Category>(c)))
            .help(retro::categoryHelp(static_cast<Category>(c))));
    }
    items.pushBack(action("Check All Categories", retroAllCheckCategories));
    items.pushBack(action("Uncheck All Categories", retroAllUncheckCategories));
    items.pushBack(action("Apply to Checked Categories", retro::allApplyNow).help("As at the top."));
    return items;
}

// Graphics > Loot Beams (vr_lootbeams.cpp): pillars of light over things to pick up.
[[nodiscard]] za::Vector<Item> pageGraphicsLootBeams()
{
    return {
        row(opt::vr_lootbeams),
        header("What Has One"),
        row(opt::vr_lootbeam_weapons),
        row(opt::vr_lootbeam_mags),
        row(opt::vr_lootbeam_mag_min),
        row(opt::vr_lootbeam_ammo),
        row(opt::vr_lootbeam_health),
        row(opt::vr_lootbeam_armor),
        row(opt::vr_lootbeam_powerups),
        row(opt::vr_lootbeam_keys),
        row(opt::vr_lootbeam_backpacks),
        header("Look"),
        row(opt::vr_lootbeam_opacity),
        row(opt::vr_lootbeam_length),
        row(opt::vr_lootbeam_thickness),
        row(opt::vr_lootbeam_saturation),
        row(opt::vr_lootbeam_fade_in),
        row(opt::vr_lootbeam_settle),
        row(opt::vr_lootbeam_near),
        row(opt::vr_lootbeam_far),
        header("Colours"),
        row(opt::vr_lootbeam_hue_weapons),
        row(opt::vr_lootbeam_hue_weapons_empty),
        row(opt::vr_lootbeam_hue_mags),
        row(opt::vr_lootbeam_hue_ammo),
        row(opt::vr_lootbeam_hue_health),
        row(opt::vr_lootbeam_hue_armor),
        row(opt::vr_lootbeam_hue_powerups),
        row(opt::vr_lootbeam_hue_keys),
        row(opt::vr_lootbeam_hue_backpacks),
        row(opt::vr_lootbeam_hue_toolgun).developer(),
        row(opt::vr_lootbeam_toolgun_length).developer(),
    };
}

// Graphics: water, slime and lava.
// Graphics: teleporters that show where they lead, and going through them (vr_portals.cpp).
[[nodiscard]] za::Vector<Item> pageGraphicsTeleporters()
{
    return {
        row(opt::vr_teleporters),
        row(opt::vr_portals),
        row(opt::vr_portals_walk),
        row(opt::vr_portals_monsters),
        row(opt::vr_portals_ai),
        row(opt::vr_teleporter_pair_exits),
        row(opt::vr_teleporter_self_head),
        row(opt::vr_portals_maxviews),
        row(opt::vr_portals_recursion),
        row(opt::vr_portals_recursion_views),
        row(opt::vr_portals_recursion_max),
        row(opt::vr_portals_recursion_scale),
        header("Teleporter Stars"),
        row(opt::vr_teleporter_surface_size),
        row(opt::vr_teleporter_surface_opacity),
        row(opt::vr_teleporter_surface_fade),
    };
}

// Graphics > Liquids > Shoreline Foam Settings: each liquid's foam (vr_water_foam_*, vr_lava_foam_*, vr_slime_foam_*;
// vr_water.cpp foamLook), a section each.
struct FoamRows
{
    cvar_t &on, &opacity, &frequency, &strength, &breathe, &breatheSpeed, &breatheIrregularity, &red, &green, &blue,
        &particles, &particleLife, &particleOpacity, &particleSize, &particleShore, &edge;
    const char* label[17]; // the header, then each row's
};

void addFoamRows(za::Vector<Item>& items, const FoamRows& r)
{
    items.pushBack(header(r.label[0]));
    items.pushBack(toggle(r.label[1], r.on).help("This liquid's shoreline foam on or off (Shoreline Foam, on the Liquids page, is every liquid's)."));
    items.pushBack(slider(r.label[2], r.opacity, 0.f, 1.5f, 0.05f, "%.2fx").extend().help("How opaque the foam is."));
    items.pushBack(slider(r.label[3], r.frequency, 0.25f, 4.f, 0.05f, "%.2fx").extend().help("How finely the foam's edge and flecks are broken up: higher, more and smaller patches; lower, broad ones."));
    items.pushBack(slider(r.label[4], r.strength, 0.25f, 3.f, 0.05f, "%.2fx").extend().help("How far the foam reaches out from the shore and round things standing in it."));
    items.pushBack(slider(r.label[5], r.breathe, 0.f, 1.f, 0.05f, "%.2f").extend().help("Breathing: the foam's band grows and shrinks over time, not all together but stretch by stretch along the shore, so it pulses in and out all round. How much, a share of its width (0 still)."));
    items.pushBack(slider(r.label[6], r.breatheSpeed, 0.1f, 4.f, 0.05f, "%.2fx").extend().help("How fast the foam breathes (1: a breath in about four seconds)."));
    items.pushBack(slider(r.label[7], r.breatheIrregularity, 0.f, 1.f, 0.05f, "%.2f").extend().help("How unevenly it breathes along the shore: 0 all of it together; 1 each stretch in its own time and by its own amount."));
    items.pushBack(slider(r.label[8], r.red, 0.f, 2.f, 0.02f, "%.2f").help("The foam's colour: red (over 1 glows, for lava's crust to bloom)."));
    items.pushBack(slider(r.label[9], r.green, 0.f, 2.f, 0.02f, "%.2f").help("The foam's colour: green."));
    items.pushBack(slider(r.label[10], r.blue, 0.f, 2.f, 0.02f, "%.2f").help("The foam's colour: blue."));
    items.pushBack(slider(r.label[11], r.particles, 0.f, 4.f, 0.1f, "%.1fx").extend().help("Small puffs of foam along the shore where the foam meets the rock, so the edge is not one straight line: how many (1: about 15 a second for each 100 units of shore near you; at most about 600 alive; 0 off)."));
    items.pushBack(slider(r.label[12], r.particleLife, 0.2f, 4.f, 0.1f, "%.1f s").extend().help("How long each foam particle lasts, drifting and fading out."));
    items.pushBack(slider(r.label[13], r.particleOpacity, 0.f, 1.f, 0.05f, "%.2f").extend().help("How opaque the foam particles are."));
    items.pushBack(slider(r.label[14], r.particleSize, 0.25f, 4.f, 0.05f, "%.2fx").extend().help("How big the foam particles are (1: about 3 units across, growing to 9 as they fade)."));
    items.pushBack(slider(r.label[15], r.particleShore, -4.f, 16.f, 0.5f, "%.1f").extend().help("Where the foam particles are born: units out from the line where the liquid meets the rock (0: on it, a little either side, over the hard edge; more: out in the foam band)."));
    items.pushBack(slider(r.label[16], r.edge, 0.f, 12.f, 0.5f, "%.1f").extend().help("A wavy edge: the liquid's edge at the shore drawn back in slow curves that drift along it, the foam following them, so it is not one straight cut. How far, units (0 off)."));
}

[[nodiscard]] za::Vector<Item> pageGraphicsFoam()
{
    za::Vector<Item> items;
    items.pushBack(row(opt::vr_water_foam));
    addFoamRows(items, {vr_water_foam_on, vr_water_foam_opacity, vr_water_foam_frequency, vr_water_foam_strength,
        vr_water_foam_breathe, vr_water_foam_breathe_speed, vr_water_foam_breathe_irregularity, vr_water_foam_red,
        vr_water_foam_green, vr_water_foam_blue, vr_water_foam_particles, vr_water_foam_particle_life,
        vr_water_foam_particle_opacity, vr_water_foam_particle_size, vr_water_foam_particle_shore, vr_water_foam_edge,
        {"Water", "Water Foam", "Water Opacity", "Water Frequency", "Water Strength", "Water Breathing",
            "Water Breathing Speed", "Water Breathing Irregularity", "Water Red", "Water Green", "Water Blue",
            "Water Particles", "Water Particle Life", "Water Particle Opacity", "Water Particle Size",
            "Water Particle Shore", "Water Wavy Edge"}});
    addFoamRows(items, {vr_lava_foam_on, vr_lava_foam_opacity, vr_lava_foam_frequency, vr_lava_foam_strength,
        vr_lava_foam_breathe, vr_lava_foam_breathe_speed, vr_lava_foam_breathe_irregularity, vr_lava_foam_red,
        vr_lava_foam_green, vr_lava_foam_blue, vr_lava_foam_particles, vr_lava_foam_particle_life,
        vr_lava_foam_particle_opacity, vr_lava_foam_particle_size, vr_lava_foam_particle_shore, vr_lava_foam_edge,
        {"Lava", "Lava Foam", "Lava Opacity", "Lava Frequency", "Lava Strength", "Lava Breathing",
            "Lava Breathing Speed", "Lava Breathing Irregularity", "Lava Red", "Lava Green", "Lava Blue",
            "Lava Particles", "Lava Particle Life", "Lava Particle Opacity", "Lava Particle Size",
            "Lava Particle Shore", "Lava Wavy Edge"}});
    addFoamRows(items, {vr_slime_foam_on, vr_slime_foam_opacity, vr_slime_foam_frequency, vr_slime_foam_strength,
        vr_slime_foam_breathe, vr_slime_foam_breathe_speed, vr_slime_foam_breathe_irregularity, vr_slime_foam_red,
        vr_slime_foam_green, vr_slime_foam_blue, vr_slime_foam_particles, vr_slime_foam_particle_life,
        vr_slime_foam_particle_opacity, vr_slime_foam_particle_size, vr_slime_foam_particle_shore, vr_slime_foam_edge,
        {"Slime", "Slime Foam", "Slime Opacity", "Slime Frequency", "Slime Strength", "Slime Breathing",
            "Slime Breathing Speed", "Slime Breathing Irregularity", "Slime Red", "Slime Green", "Slime Blue",
            "Slime Particles", "Slime Particle Life", "Slime Particle Opacity", "Slime Particle Size",
            "Slime Particle Shore", "Slime Wavy Edge"}});
    return items;
}

[[nodiscard]] za::Vector<Item> pageGraphicsLiquids()
{
    return {
        row(opt::vr_water_waves),
        row(opt::vr_water_fresnel),
        row(opt::vr_water_reflections),
        row(opt::vr_water_refraction),
        row(opt::vr_water_glints),
        row(opt::vr_water_lava_glow),
        row(opt::vr_water_geo_waves),
        row(opt::vr_water_geo_amplitude),
        slider("Shoreline Foam", "vr_water_foam", 0.f, 2.f, 0.1f, "%.1f").extend().help("Foam where water meets walls, steps and things standing in it; a scum on slime, a molten crust on lava (0 off). Each liquid's look: Shoreline Foam settings."),
        open("Shoreline Foam Settings", pageIndex(pageGraphicsFoam)).help("Each liquid's shoreline foam (water, lava, slime): opacity, how fine and how far, its breathing, its colour and the foam particles along the shore."),
        row(opt::vr_heat_haze),
        row(opt::vr_heat_haze_half).extend().help("What the heat haze bends: a copy of the view at half size (cheaper where the haze covers much of the view, standing in fire or over lava; a little softer inside the haze: the presets below High) or at full size (sharp: the default, High and Ultra)."),
        header("Under Water"),
        row(opt::vr_water_caustics),
        row(opt::vr_water_underwater),
        row(opt::vr_water_wobble),
        header("Splashes and Ripples"),
        row(opt::vr_water_splash),
        row(opt::vr_water_splash_size),
        row(opt::vr_water_splash_ring_speed),
        row(opt::vr_water_splash_ring_size),
        row(opt::vr_water_ripples),
        row(opt::vr_water_ripple_amplitude),
        row(opt::vr_water_ripple_normal),
        row(opt::vr_water_ripple_speed),
        row(opt::vr_water_ripple_decay),
        row(opt::vr_water_ripple_wavelength),
        row(opt::vr_water_ripple_max),
        row(opt::vr_water_sounds),
    };
}

// Graphics: over the whole image: bloom, tone mapping, the colour grade, dither.
[[nodiscard]] za::Vector<Item> pageGraphicsPost()
{
    return {
        row(opt::vr_bloom),
        row(opt::vr_bloom_threshold),
        row(opt::vr_bloom_radius),
        row(opt::vr_bloom_white),
        row(opt::vr_bloom_color),
        row(opt::vr_bloom_adapt),
        row(opt::vr_bloom_fast),
        header("Tone and Colour"),
        row(opt::vr_tonemap),
        row(opt::vr_exposure),
        row(opt::vr_grade),
        row(opt::vr_grade_strength),
        row(opt::vr_dither),
    };
}

// Graphics: models' light and reflections, glowing screens and sights, particles and decals.
[[nodiscard]] za::Vector<Item> pageGraphicsModels()
{
    return {
        row(opt::vr_model_lighting),
        row(opt::vr_model_light_parity),
        row(opt::vr_model_ambient_dir),
        row(opt::vr_model_ambient_contrast),
        row(opt::r_lerpmodels),
        row(opt::vr_anim_smooth_tension),
        row(opt::vr_mdl_dequant),
        row(opt::vr_anim_smooth_rig),
        row(opt::vr_anim_smooth_rebuild),
        row(opt::vr_rim_light),
        row(opt::vr_viewmodel_minlight),
        row(opt::vr_weapon_reflections),
        row(opt::vr_weapon_reflections_strength),
        header("Glow"),
        open("Screen Glow: Screens", pageIndex(pageScreens)).help("The screens' glow is on HUD and Menus > Screens."),
        row(opt::vr_weapon_glow),
        open("Weapon Sight Hue: Colours", pageIndex(pageColours)).help("Every hue is on HUD and Menus > Colours."),
        header("Effects"),
        row(opt::vr_particle_light),
        row(opt::vr_soft_particles),
        row(opt::vr_soft_particles_scale),
        row(opt::vr_particle_near_fade),
        row(opt::vr_particle_near_cull),
        row(opt::vr_particle_max_screen),
        row(opt::vr_particle_halfres),
        row(opt::vr_particle_trim),
        row(opt::vr_particle_saturate),
        row(opt::vr_particle_retro_fast),
        row(opt::vr_particle_retro_halfres),
        row(opt::vr_particle_retro_halfres_pixels),
        open("Decals and Gib Blood: Gore", pageIndex(pageGore)).help("Decals and the gibs' blood are on Gore."),
        row(opt::vr_weapon_morph_time),
    };
}

[[nodiscard]] za::Vector<Item> pageHudConfiguration()
{
    return {
        open("HUD and Status Bar Hand: VR Settings, Body and Display", pageIndex(pageVrsHudHapticsSound)).help("The HUD's kind is on VR Settings; the status bar's hand on Body and Display."),
        row(opt::vr_hud_scale),
        row(opt::vr_sbar_offset_x),
        row(opt::vr_sbar_offset_y),
        row(opt::vr_sbar_offset_z),
        row(opt::vr_sbar_offset_roll),
        row(opt::vr_sbar_offset_pitch),
        row(opt::vr_sbar_offset_yaw),
    };
}

// (The hip holsters are on Hip Holsters.)
[[nodiscard]] za::Vector<Item> pageHotspotSettings()
{
    return {
        header("Virtual Stock"),
        row(opt::vr_show_virtual_stock),
        header("Shoulder Holsters"),
        row(opt::vr_show_shoulder_holsters),
        row(opt::vr_shoulder_holster_offset_x),
        row(opt::vr_shoulder_holster_offset_y),
        row(opt::vr_shoulder_holster_offset_z),
        row(opt::vr_shoulder_holster_thresh),
        header("Upper Holsters"),
        row(opt::vr_show_upper_holsters),
        row(opt::vr_upper_holster_offset_x),
        row(opt::vr_upper_holster_offset_y),
        row(opt::vr_upper_holster_offset_z),
        row(opt::vr_upper_holster_thresh),
    };
}

// Immersion > Holster Zones (vr_holsterzones.hpp): the boxes where a hand, and the gun it holds, is at a holster or a
// pouch; each kind's place and shape, its size from its reach (the Threshold sliders, here too).
[[nodiscard]] za::Vector<Item> pageHolsterZones()
{
    za::Vector<Item> items{
        row(opt::vr_holster_zones),
        row(opt::vr_holster_zones_show),
        row(opt::vr_holster_zones_show_items),
        row(opt::vr_holster_zone_hand_point),
        row(opt::vr_holster_zone_handle_radius),
    };
    const auto kind = [&items](const char* title, cvar_t& reach, cvar_t& x, cvar_t& y, cvar_t& z, cvar_t& depth,
                          cvar_t& width, cvar_t& height, cvar_t& item, cvar_t& drag) {
        items.pushBack(header(title));
        items.pushBack(toggle("Drag", drag)
                .help("On: grip inside one of these zones (orange; cyan while held) and move the hand to move it there, "
                      "both of the pair (its Move sliders below follow). Grip it with the other hand too and spread or "
                      "close the hands to resize it. The grip draws no gun there while this is on. Off again on the "
                      "next start."));
        items.pushBack(slider("Reach", reach, 0.f, 30.f, 0.1f, "%.1f").extend(0.f, 100.f)
                .help("Its size (units; its Threshold elsewhere): the box's half depth, width and height are this times "
                      "the three below, stretched to take in both the holster as drawn and its reach point."));
        items.pushBack(slider("Depth", depth, 0.1f, 3.f, 0.05f, "%.2fx").extend(0.02f, 10.f)
                .help("Out of the body's surface there (the hips': sideways), turned as the holster is."));
        items.pushBack(slider("Width", width, 0.1f, 3.f, 0.05f, "%.2fx").extend(0.02f, 10.f)
                .help("Along the body's surface there (the hips': front to back)."));
        items.pushBack(slider("Height", height, 0.1f, 3.f, 0.05f, "%.2fx").extend(0.02f, 10.f).help("Up and down."));
        items.pushBack(slider("Move Forward", x, -15.f, 15.f, 0.25f, "%.2f").extend(-50.f, 50.f)
                .help("Units forward (negative: back), from midway between the holster as drawn and its reach point (the "
                      "box takes in both)."));
        items.pushBack(slider("Move Outwards", y, -15.f, 15.f, 0.25f, "%.2f").extend(-50.f, 50.f)
                .help("Units outwards, away from the body's middle (negative: towards it), mirrored for the left ones; the "
                      "pouches: to the right."));
        items.pushBack(slider("Move Up", z, -15.f, 15.f, 0.25f, "%.2f").extend(-50.f, 50.f).help("Units up (negative: down)."));
        items.pushBack(slider("Item Size", item, 1.f, 3.f, 0.05f, "%.2fx").extend(0.25f, 5.f)
                .help("Carrying an ammo box, a health box or a backpack to put away: the box this much larger about its "
                      "middle (a gun, a key or a grenade: as set above). 1: no larger."));
    };
    kind("Hip Holsters", vr_hip_holster_thresh, vr_holster_zone_hip_x, vr_holster_zone_hip_y, vr_holster_zone_hip_z,
        vr_holster_zone_hip_depth, vr_holster_zone_hip_width, vr_holster_zone_hip_height, vr_holster_zone_item_hip,
        vr_holster_zone_hip_drag);
    kind("Chest Holsters", vr_upper_holster_thresh, vr_holster_zone_upper_x, vr_holster_zone_upper_y,
        vr_holster_zone_upper_z, vr_holster_zone_upper_depth, vr_holster_zone_upper_width, vr_holster_zone_upper_height,
        vr_holster_zone_item_upper, vr_holster_zone_upper_drag);
    kind("Shoulder Holsters", vr_shoulder_holster_thresh, vr_holster_zone_shoulder_x, vr_holster_zone_shoulder_y,
        vr_holster_zone_shoulder_z, vr_holster_zone_shoulder_depth, vr_holster_zone_shoulder_width,
        vr_holster_zone_shoulder_height, vr_holster_zone_item_shoulder, vr_holster_zone_shoulder_drag);
    kind("Ammo Pouch", vr_ammo_pouch_thresh, vr_holster_zone_ammo_x, vr_holster_zone_ammo_y, vr_holster_zone_ammo_z,
        vr_holster_zone_ammo_depth, vr_holster_zone_ammo_width, vr_holster_zone_ammo_height, vr_holster_zone_item_ammo,
        vr_holster_zone_ammo_drag);
    kind("Grenade Pouch", vr_grenade_pouch_thresh, vr_holster_zone_grenade_x, vr_holster_zone_grenade_y,
        vr_holster_zone_grenade_z, vr_holster_zone_grenade_depth, vr_holster_zone_grenade_width,
        vr_holster_zone_grenade_height, vr_holster_zone_item_grenade, vr_holster_zone_grenade_drag);
    return items;
}

// Split from Hotspots: the hip holsters and their slots' models.
[[nodiscard]] za::Vector<Item> pageHipHolsters()
{
    return {
        row(opt::vr_show_hip_holsters),
        row(opt::vr_hip_offset_x),
        row(opt::vr_hip_offset_y),
        row(opt::vr_hip_offset_z),
        row(opt::vr_hip_holster_thresh),
        header("Holster Slots"),
        open("Holster Models: Body and Display", pageIndex(pageBodyDisplay)).help("Showing the holsters' models is on Body and Display."),
        row(opt::vr_leg_holster_model_scale),
        row(opt::vr_leg_holster_model_x_offset),
        row(opt::vr_leg_holster_model_y_offset),
        row(opt::vr_leg_holster_model_z_offset),
        header("Grenade Pouch"),
        row(opt::vr_show_grenade_pouch),
        row(opt::vr_grenade_pouch_x),
        row(opt::vr_grenade_pouch_y),
        row(opt::vr_grenade_pouch_z),
        row(opt::vr_grenade_pouch_thresh),
        row(opt::vr_grenade_pouch_hold_pitch),
        row(opt::vr_grenade_pouch_hold_yaw),
        row(opt::vr_grenade_pouch_hold_roll),
    };
}

[[nodiscard]] za::Vector<Item> pageTransparencyOptions()
{
    return {
        row(opt::r_novis),
        row(opt::r_wateralpha),
        row(opt::r_lavaalpha),
        row(opt::r_telealpha),
        row(opt::r_slimealpha),
        row(opt::vr_map_liquid_alpha),
    };
}

// What the Relighting page showed of the tool when built (relight::toolPageState: found, a download running): a change
// rebuilds it (items()).
int relightToolState = -1;

// Graphics > Relighting (vr_relight.cpp; docs/RELIGHTING.md, "Relighting in the game"): ericw-tools' light on the map you
// are in, in the background, with these settings; then the map reloaded where you are. Or many maps (Many Maps): an
// episode, a game, the Map Library's, every one, a few side by side, the bar showing how far. With no light.exe found,
// Tool offers the download of ericw-tools (vr_relight_tool.cpp).
[[nodiscard]] za::Vector<Item> pageGraphicsRelighting()
{
    relightToolState = relight::toolPageState();
    [[maybe_unused]] const bool found = (relightToolState & 1) != 0;
    const bool downloading = (relightToolState & 2) != 0;
    const bool seeThrough = (relightToolState & 8) != 0; // VisPatch's data found (See-Through Liquids usable)
    za::Vector<Item> list{
        info(relight::statusLine),
        progressBar(relight::progress, relight::progressText),
        info(relight::detailLine),
        info(relight::mapLine),
        command("Relight This Map", "vr_relight").help("Relights the map you are in with the settings below (a few seconds to a minute, in the background; you can keep playing), then reloads it where you are. The result is kept: the map plays with it from now on."),
        command("Cancel", "vr_relight_cancel").help("Stops the relighting running now: the maps it finished keep their new light, the ones it was lighting keep the light they had (nothing half-made is left)."),
        command("Remove This Map's Relight", "vr_relight_revert").help("The map you are in plays with its other light from its next start: relight_maps.py's relit map, if made, else its own."),
        row(opt::vr_relight_use),
        row(opt::vr_relight_reload),
        header("Many Maps"),
        row(opt::vr_relight_batch_set),
        row(opt::vr_relight_batch_episode),
        row(opt::vr_relight_batch_game),
        command("Relight These Maps", "vr_relight_batch").help("Relights the maps chosen above one after another in the background, while you play (an episode takes a few minutes, every map an hour or more with Bounced Light). The bar above shows how far, and the time left; Cancel stops it. Maps relit with these same settings already are skipped."),
        row(opt::vr_relight_batch_force),
        row(opt::vr_relight_parallel),
        row(opt::vr_relight_batch_reload),
        row(opt::vr_relight_indicator),
        header("Lamps and Glowing Textures"),
        row(opt::vr_relight_strength),
        row(opt::vr_relight_lamps),
        row(opt::vr_relight_glows),
        row(opt::vr_relight_liquids),
        header("The Map's Light"),
        row(opt::vr_relight_maplights),
        row(opt::vr_relight_sunlight),
        row(opt::vr_relight_bounce),
        row(opt::vr_relight_ao),
        row(opt::vr_relight_minlight),
        row(opt::vr_relight_quality),
        seeThrough ? row(opt::vr_relight_seethrough)
                   : toggle("See-Through Liquids", "vr_relight_seethrough").unavailableBecause("Needs VisPatch's data files, not part of Quake VR: id1.vis, hipnotic.vis and rogue.vis (in id1_vis.tgz, hipnotic_vis.tgz and rogue_vis.tgz, sourceforge.net/projects/vispatch, \"vispatch data\" 1.0) in quakevr/tools/vispatch. Without them id's liquids stay opaque (except in maps relight_maps.py made see-through)."),
        command("Defaults", "vr_relight_defaults").help("Every setting on this page back to the relight script's look."),
        header("Tool"),
        info(relight::toolLine),
    };
    if(downloading)
    {
        list.pushBack(progressBar(relight::tool::progress, relight::tool::progressText));
        list.pushBack(info(relight::tool::statusLine));
        list.pushBack(command("Cancel Download", "vr_relight_get_tool cancel").help("Stops the download: nothing of it is kept."));
    }
    else
    {
        if(relight::tool::statusLine()[0])
        {
            list.pushBack(info(relight::tool::statusLine)); // (the last download's result: a job's end rebuilds the page)
        }
#ifdef _WIN32 // (the release's light.exe: elsewhere, build ericw-tools and set vr_relight_tool)
        if(!found)
        {
            list.pushBack(command("Download ericw-tools (27.5 MB)", "vr_relight_get_tool")
                    .help("Relighting needs ericw-tools' light, which this copy of Quake VR does not have. Downloads ericw-tools "
                          "2.0.0-alpha11 from its GitHub release (27.5 MB, checked against its known SHA-256) into "
                          "quakevr/tools/ericw-tools: light.exe, its libraries and their licences (GPL-3, Apache-2.0)."));
        }
#endif
    }
    list.pushBack(command("Status in the Console", "vr_relight_status").developer());
    list.pushBack(command("Tool Lookup in the Console", "vr_relight_get_tool status").developer().help("vr_relight_get_tool status: the light.exe found, where the download goes, the pinned file (version, size, sha256) and the last download's result."));
    list.pushBack(command("See-Through Data in the Console", "vr_relight_vispatch").developer().help("vr_relight_vispatch: where VisPatch's data files (See-Through Liquids) were looked for and found, and which liquids of the map in play were loaded see-through. vr_relight_vispatch check <file.bsp> tells it for any map; vr_relight_vispatch_dir points the lookup at a test folder."));
    list.pushBack(command("Write the Texture Lights", "vr_relight_lights").developer().help("Writes the lights this map's textures get with these settings to relight_lights.txt and lists them in the console (to compare with relight_maps.py --list-glows)."));
    return list;
}

// Graphics > Retro Lighting (vr_retrolight.cpp; ROUND21.md, "Retro lighting"): the light in Quake's coarse look, to go
// with the retro textures. Every grid is fixed to the surfaces (the faces' texels, the skins'), never to the screen.
[[nodiscard]] za::Vector<Item> pageGraphicsRetroLight()
{
    const za::Vector<Choice> blocks{{0.f, "Off"}, {1.f, "1"}, {2.f, "2"}, {4.f, "4"}, {8.f, "8"}, {16.f, "16"}};
    // The shadows' may go finer than a texel (vr_retrolight.cpp's block(), 1/4 the finest): smaller pixels, still on
    // the texture's grid.
    const za::Vector<Choice> shadowBlocks{{0.f, "Off"}, {0.25f, "1/4 texel"}, {0.5f, "1/2 texel"}, {1.f, "1"},
                                          {2.f, "2"}, {4.f, "4"}, {8.f, "8"}, {16.f, "16"}};
    const za::Vector<Choice> cells{{1.f, "1"}, {2.f, "2"}, {4.f, "4"}};
    return {
        row(opt::vr_retrolight),
        row(opt::vr_retrolight_edge_soft),
        row(opt::vr_retrolight_spacing),
        header("World: the Baked Light"),
        row(opt::vr_retrolight_world),
        row(opt::vr_retrolight_world_steps),
        row(opt::vr_retrolight_world_soft),
        row(opt::vr_retrolight_world_dither),
        cycle("Dither Cell", "vr_retrolight_world_dither_size", cells).help("The dither's cells, in Quake texels."),
        row(opt::vr_retrolight_world_lightmap),
        row(opt::vr_retrolight_world_luxel),
        header("World: Dynamic Lights"),
        row(opt::vr_retrolight_world_dyn_steps),
        cycle("Dynamic Light Blocks", "vr_retrolight_world_dyn_block", blocks)
            .help("Dynamic lights found once a block of this many Quake texels (aligned with the texture's blocks): "
                  "pixelated pools of light. Off: per pixel."),
        header("Models"),
        row(opt::vr_retrolight_models),
        row(opt::vr_retrolight_model_steps),
        row(opt::vr_retrolight_model_soft),
        row(opt::vr_retrolight_model_dither),
        cycle("Model Dither Cell", "vr_retrolight_model_dither_size", cells).help("The dither's cells, in the skin's Quake texels."),
        row(opt::vr_retrolight_model_dyn_steps),
        cycle("Model Dynamic Light Blocks", "vr_retrolight_model_dyn_block", blocks)
            .help("Dynamic lights found once a block of the skin's texels. Off: per pixel."),
        header("Shadows"),
        row(opt::vr_retrolight_shadow_filter),
        row(opt::vr_retrolight_shadow_steps),
        row(opt::vr_retrolight_shadow_soft),
        cycle("Shadow Blocks", "vr_retrolight_shadow_block", shadowBlocks)
            .help("Shadows looked up once a block of this many texels (the face's or the skin's): blocky shadow edges on "
                  "the texture's grid. Off: per pixel (or the dynamic lights' blocks). 1/4 and 1/2 are sub-texel: the "
                  "blocks are finer than the texture, so the shadows stay pixelated with smaller pixels. Like every grid "
                  "here they turn smooth where a block gets smaller than a pixel on screen, so below 1 only shows where "
                  "the texels themselves are a few pixels wide (near surfaces)."),
        header("Looks"),
        command("Look: Software Quake",
            "vr_retrolight 1; vr_retrolight_world_steps 64; vr_retrolight_world_soft 0; vr_retrolight_world_dither 0; "
            "vr_retrolight_world_lightmap 1; vr_retrolight_world_luxel 1; vr_retrolight_world_dyn_steps 64; "
            "vr_retrolight_world_dyn_block 1; vr_retrolight_model_steps 64; vr_retrolight_model_dyn_steps 64; "
            "vr_retrolight_shadow_filter 1; vr_retrolight_shadow_steps 0; vr_retrolight_shadow_block 0")
            .help("As Quake's software renderer: each texel lit on its own, 64 light levels."),
        command("Look: Blocky Lightmaps",
            "vr_retrolight 1; vr_retrolight_world_steps 0; vr_retrolight_world_lightmap 1; vr_retrolight_world_luxel 16; "
            "vr_retrolight_world_dyn_steps 0; vr_retrolight_world_dyn_block 16; vr_retrolight_model_steps 0; "
            "vr_retrolight_model_dyn_steps 0; vr_retrolight_shadow_filter 2; vr_retrolight_shadow_steps 0; vr_retrolight_shadow_block 0")
            .help("GLQuake with nearest filtering: the light in luxel squares, the dynamic lights too; blocky shadows."),
        command("Look: Banded and Dithered",
            "vr_retrolight 1; vr_retrolight_world_steps 8; vr_retrolight_world_dither 1; vr_retrolight_world_dither_size 1; "
            "vr_retrolight_world_lightmap 0; vr_retrolight_world_dyn_steps 6; vr_retrolight_world_dyn_block 2; "
            "vr_retrolight_model_steps 8; vr_retrolight_model_dither 1; vr_retrolight_shadow_steps 2; vr_retrolight_shadow_block 2")
            .help("Few light levels, dithered between, on smooth lightmaps: a print-like look."),
        command("Reset to Defaults",
            "reset vr_retrolight_spacing; reset vr_retrolight_edge_soft; reset vr_retrolight_world; reset vr_retrolight_world_steps; reset vr_retrolight_world_soft; "
            "reset vr_retrolight_world_dither; reset vr_retrolight_world_dither_size; reset vr_retrolight_world_lightmap; "
            "reset vr_retrolight_world_luxel; reset vr_retrolight_world_dyn_steps; reset vr_retrolight_world_dyn_block; "
            "reset vr_retrolight_models; reset vr_retrolight_model_steps; reset vr_retrolight_model_soft; reset vr_retrolight_model_dither; "
            "reset vr_retrolight_model_dither_size; reset vr_retrolight_model_dyn_steps; reset vr_retrolight_model_dyn_block; "
            "reset vr_retrolight_shadow_filter; reset vr_retrolight_shadow_steps; reset vr_retrolight_shadow_block; reset vr_retrolight_shadow_soft")
            .help("Every setting on this page back to its default (the switch stays as it is)."),
    };
}
