// vr_menu_pages.inc -- the old Quake VR settings pages (Quake VR's menu.cpp, "Quake VR -
// Settings"), under VR Settings > Advanced VR Options: every setting that still exists in the
// port, with its old range and help. Included by vr_menu.cpp. Settings that do nothing any more
// (the menu's anchoring, MSAA, the old force grab's range and power, the old floating torso,
// voice chat) are left out. The long ones are split into pages of a screenful or so (Graphics in
// seven; Immersion's screens and force grab moved to the Screens and Force Grab pages).

// Old Quake VR settings: "Menu Settings".
[[nodiscard]] std::vector<Item> pageMenuSettings()
{
    return {
        slider("Menu Scale", "vr_menu_scale", 0.05f, 0.6f, 0.01f, "%.2f").extend(0.02f, 1.5f).help("Scale multiplier for the menu: the size of one of its pixels, in world units."),
        slider("Menu Distance", "vr_menu_distance", 24.f, 256.f, 1.f, "%.0f").extend(8.f, 600.f).help("Distance of the menu from the anchor point."),
        toggle("VR Menu Style", "vr_menu_vr_style").help("The menus fill the panel with their rows spaced out, with modern sliders and switches, and a laser from your hand points at them: trigger to click and drag."),
        hueSlider("Laser Hue", vr_menu_laser_hue).help("The laser pointer's colour: 35 amber; Player's: the Player Effects Hue (on the Colours page)."),
        slider("Row Spacing", "vr_menu_spacing", 1.f, 2.f, 0.05f, "%.2fx").extend().help("With the VR menu style: how far apart the menus' rows are, over Quake's."),
        slider("Menu Height", "vr_menu_height", 1.f, 2.f, 0.05f, "%.2fx").extend().help("With the VR menu style: how tall the menus are, over Quake's: more rows at once. 1.35 reaches about 35 degrees up and down at the shipped distance, scale and spacing."),
        slider("Menu Background Opacity", "scr_menubgalpha", 0.f, 1.f, 0.05f, "%.2f").help("How dark the panel behind the menus is (0.7 as shipped; the desktop menus' too): lower it to see the game while you tune the graphics. Below about 0.5 the text gets a dark outline, to stay readable."),
        toggle("Live Preview", "ui_live_preview").help("Settings pages show their changes in the game as you make them, and in single player the game keeps running under them (paused while a monster is after you)."),
        toggle("Reopen Where Left", "vr_menu_remember").help("After Back to Game (the button at the menu's top left, or holding the menu button), the menu opens again on the page you left."),
    };
}

// Old Quake VR settings: "Crosshair Settings".
[[nodiscard]] std::vector<Item> pageCrosshairSettings()
{
    return {
        cycle("Crosshair", "vr_crosshair", {{0.f, "Off"}, {1.f, "Dot"}, {2.f, "Laser"}, {3.f, "Soft laser"}}),
        slider("Crosshair Depth", "vr_crosshair_depth", 0.f, 4096.f, 16.f, "%.0f").extend().help("When zero, the crosshair will be projected to the closest wall. Otherwise, the crosshair will be projected at a fixed depth."),
        slider("Crosshair Size", "vr_crosshair_size", 0.f, 32.f, 0.5f, "%.1f").extend().help("Size of the crosshair."),
        slider("Crosshair Alpha", "vr_crosshair_alpha", 0.f, 1.f, 0.05f, "%.2f").help("Transparency of the crosshair. '1' means fully visible, '0' means fully transparent."),
        hueSlider("Crosshair Hue", vr_crosshair_hue).help("Its colour: 0 red; Player's: the Player Effects Hue (on the Colours page)."),
        slider("Crosshair Z Offset", "vr_crosshairy", -10.f, 10.f, 0.05f, "%.2f").extend(-50.f, 50.f).help("Z offset applied to the crosshair. Does not affect aiming. Leave to zero unless you changed other weapon options and your aim is off."),
    };
}

// Old Quake VR settings: "Particle Settings".
[[nodiscard]] std::vector<Item> pageParticleSettings()
{
    return {
        toggle("Particle Effects", "r_particles").help("Enable or disable all particle effects."),
        toggle("Quake VR Particles", "vr_particles")
            .help("Textured smoke, sparks, blood and explosions for Quake VR's effects. Off: Quake's own dots."),
        slider("Particle Multiplier", "vr_particle_mult", 0.25f, 4.f, 0.25f, "%.2f").extend()
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
        slider("Turn", "vr_snap_turn", 0.f, 90.f, 5.f, "%.0f").extend(),
        slider("Turn Speed", "vr_turn_speed", 0.f, 10.f, 0.25f, "%.1f").extend().help("How quickly you turn with the joystick."),
        toggle("Teleportation", "vr_teleport_enabled").help("Enables or disables teleportation. A controller binding is required to use teleportation."),
        slider("Teleport Range", "vr_teleport_range", 100.f, 800.f, 10.f, "%.0f").extend(100.f, 3000.f).help("How far you can teleport."),
        slider("Lean", "vr_lean_radius", 0.f, 14.f, 1.f, "%.0f units").extend().help("How far your head may lean off the middle of your body's box before the body follows: get your face near walls and over railings. 0: the body always under the head."),
        slider("Lean Recentre", "vr_lean_recenter", 0.f, 2.f, 0.1f, "%.1f m/s").extend().help("How fast the body slides back under your head when it can (not into walls, not over ledges), at least; with Lean Detection, faster the further off it is, and not while you lean. 0: never."),
        slider("Lean Detection", "vr_lean_detect", 0.f, 2.f, 0.1f, "%.1fx").help("Tells a lean from walking in the room. Leaning (your head lower and tilted the way you lean, your hands left by your hips), your feet and hips stay where you stand and your back tilts; walking, the body follows your head. 0: off, the body always slides back under your head; higher: more readily a lean."),
        toggle("Roomscale Jump", "vr_roomscale_jump").help("Jumping in the room jumps in the game."),
        slider("Jump Threshold", "vr_roomscale_jump_threshold", 0.3f, 2.f, 0.05f, "%.2f m/s").extend()
            .help("How fast the head must rise above the standing height."),
        slider("Roomscale Move Mult.", "vr_roomscale_move_mult", 0.25f, 5.f, 0.25f, "%.1f").extend().help("Real movement to in-game movement. Increase to cover more ground, e.g. with teleportation or little room."),
        slider("Movement Speed", "cl_forwardspeed", 100.f, 400.f, 25.f, "%.0f").extend().help("Walking speed (running is this times the running multiplier)."),
        cycle("Default Speed", "cl_alwaysrun", {{1.f, "Run"}, {0.f, "Walk"}}).help("The speed button switches to the other."),
        slider("Running Mult.", "cl_movespeedkey", 1.f, 3.f, 0.05f, "%.2f").extend().help("How much faster running is than walking."),
        toggle("Swimming", "vr_swim").help("In water the stick slows and strokes of your hands move you (more on the Swimming page)."),
        toggle("Climbing", "vr_climb").help("Grip a ledge or a rung with either empty hand, or both, to hang from it; pull to climb hand over hand, shimmy along a ledge, pull up onto it. Edges only, not flat walls."),
        slider("Lowest Ledge", "vr_climb_min_height", 0.f, 64.f, 2.f, "%.0f units").extend().help("How far above your feet a ledge must be to grab it."),
        slider("Ledge Fling", "vr_climb_fling", 0.f, 2.f, 0.1f, "%.1f").extend().help("How much letting go of a ledge flings you with your hands' motion."),
        slider("Grab Leniency", "vr_climb_leniency", 0.f, 30.f, 1.f, "%.0f cm").extend().help("A grip that just misses a ledge or a rung takes the nearest one within this distance of your hand (not through walls, behind you or below your reach). 0: only where your hand is."),
        slider("Hand on Hold: Towards You", "vr_climb_hand_out", -10.f, 10.f, 0.5f, "%.1f cm").extend().help("Where your hand is drawn on a ledge or a rung: back from the lip, towards you (negative: deeper). Looks only; the climbing is the same."),
        slider("Hand on Hold: Up", "vr_climb_hand_up", -10.f, 10.f, 0.5f, "%.1f cm").extend().help("Where your hand is drawn on a hold: up (negative: down). Looks only."),
        slider("Hand on Hold: Sideways", "vr_climb_hand_side", -10.f, 10.f, 0.5f, "%.1f cm").extend().help("Where your hand is drawn on a hold: along the edge, outwards (negative: towards the other hand). Looks only."),
        slider("Hold Rotation Blend", "vr_climb_hand_turn_blend", 0.f, 1.f, 0.05f, "%.2f").extend().help("How your hand is turned on a hold: 0 facing the ledge (Hand on Hold: Pitch, Yaw, Roll), 1 as your controller is. Looks only."),
        slider("Hand on Hold: Pitch", "vr_climb_hand_pitch", -30.f, 120.f, 5.f, "%.0f deg").extend().help("The hand facing a hold, its knuckles tilted up: 0 the palm down on the top, 90 the palm against the face under the lip. Looks only."),
        slider("Hand on Hold: Yaw", "vr_climb_hand_yaw", -60.f, 60.f, 5.f, "%.0f deg").extend().help("The hand facing a hold, its knuckles turned towards the other hand (negative: away). Looks only."),
        slider("Hand on Hold: Roll", "vr_climb_hand_roll", -90.f, 90.f, 5.f, "%.0f deg").extend().help("The hand facing a hold, rolled about its knuckles, the thumb's side up (negative: down). Looks only."),
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
        "vr_swim_flat_exp", "vr_swim_speed_exp", "vr_swim_palm_dir", "vr_swim_against_palm", "vr_air_supply"};
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
        slider("Air Supply", "vr_air_supply", 1.f, 4.f, 0.25f, "%.2fx")
            .help("How long you can stay under water before you start drowning: times Quake's 12 seconds (1.5x: 18 s). Once drowning, the damage comes as ever. The Biosuit's air is unchanged."),
        header("Strokes"),
        slider("Stroke Strength", "vr_swim_stroke", 0.f, 30.f, 1.f, "%.0f").extend().help("How far a stroke of your hand moves you."),
        slider("Speed Curve", "vr_swim_speed_exp", 1.f, 3.f, 0.25f, "%.2f").extend()
            .help("How the push grows with the hand's speed. 1: in proportion. 2: as water's drag, so a brisk stroke outweighs a slower return by far. 3: even more."),
        slider("Palm Matters", "vr_swim_palm", 0.f, 1.f, 0.1f, "%.1f").help("How much more a hand meeting the water flat pushes than one meeting it at an angle. Either side of the hand: which side leads is Stroke Against Palm."),
        slider("Stroke Against Palm", "vr_swim_against_palm", 0.f, 1.f, 0.05f, "%.2f")
            .help("How much a stroke counts when the back of your hand leads (a backhand, or turning a hand round to reposition it) instead of the palm pushing the water. 0: ignored. 1: as a palm stroke. Blends smoothly through the hand edge-on."),
        slider("Palm Sharpness", "vr_swim_flat_exp", 0.5f, 4.f, 0.25f, "%.2f").extend()
            .help("How flat the palm must be to push fully. 1: pushing falls off evenly as the hand turns; 2-3: only a really flat palm pushes well."),
        slider("Recovery Push", "vr_swim_recovery", 0.f, 1.f, 0.05f, "%.2f").help("How much a hand going edge first (bringing it back for the next stroke) pushes."),
        slider("Push Along Palm", "vr_swim_palm_dir", 0.f, 1.f, 0.1f, "%.1f")
            .help("0: you go against the hand's motion. 1: away from where the palm faces, so sweeping the hands in with palms facing each other pushes sideways (the two cancel), not back."),
        slider("Slowest Push", "vr_swim_stroke_min", 0.f, 1.f, 0.05f, "%.2f m/s").help("A hand slower than this pushes nothing at all (tracking jitter, drifting hands)."),
        header("Stroke or Return?"),
        slider("Intent Threshold", "vr_swim_power_threshold", 0.f, 3.f, 0.1f, "%.1f m/s").extend()
            .help("A stroke moves you only if the hand's top speed during it passes this: a brisk stroke swims, a relaxed return of the arms does nothing. 0: every motion pushes."),
        slider("Intent Fade-In", "vr_swim_power_knee", 0.f, 2.f, 0.1f, "%.1f m/s").extend()
            .help("Above the threshold, strokes fade in to full power over this much more speed. 0: all or nothing."),
        toggle("Whole Stroke Counts", "vr_swim_power_whole")
            .help("On: once a stroke passes the threshold, all of it pushes, its slow start too. Off: only from the moment it passes."),
        slider("Stroke Memory", "vr_swim_intent_memory", 0.f, 4.f, 0.25f, "%.2f s").extend()
            .help("How long each hand remembers the way of its last full stroke, to tell the next return from a stroke back the other way. 0 off."),
        slider("Return Damping", "vr_swim_reverse_damp", 0.f, 1.f, 0.05f, "%.2f")
            .help("How much a stroke against the remembered one is damped when it is slower than the Reverse Stroke Speed below (0.3 slower or more: fully). A deliberate stroke back, as brisk, is not damped."),
        slider("Reverse Stroke Speed", "vr_swim_reverse_speed", 0.5f, 2.f, 0.05f, "%.2fx").extend()
            .help("How fast, over the stroke it reverses, a stroke the other way must be to count fully. 1: as fast. 1.2: 20% faster (returns never swim you back). 0.8: a bit slower is enough."),
        header("Direction and Speed"),
        slider("Swim Where You Look", "vr_swim_look", 0.f, 1.f, 0.1f, "%.1f").help("You swim where you look (look up to rise): this much of a push sideways of it is lost."),
        slider("Stroke Steering", "vr_swim_stroke_assist", 0.f, 1.f, 0.1f, "%.1f").help("Strokes push this much more towards where the stick points, and as much less against it."),
        slider("Glide", "vr_swim_glide", 0.f, 1.f, 0.1f, "%.1f").help("How far you glide between strokes (less of the water's drag)."),
        slider("Swim Top Speed", "vr_swim_max_speed", 100.f, 800.f, 25.f, "%.0f").extend(),
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
            Cvar_SetQuick(&var, va("%.2f", std::round((var.value + v) * 100.f) / 100.f + 0.f));
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

// Old Quake VR settings: "Hand/Gun Calibration", with the hand calibration first.
[[nodiscard]] std::vector<Item> pageHandGunCalibration()
{
    const char* moveHelp = "Moves the whole hand, and what it holds (its aim, muzzle and melee with it), along the controller's "
                           "own axes: X along the handle, Y left, Z up (Show Controller's red, green, blue). Centimetres.";
    const char* rollHelp = "Turns the whole hand, and what it holds, about where it points, through the controller's grip "
                           "(the handle's middle): the right side down. Doesn't change where it aims.";
    std::vector<Item> list{
        header("Hand Calibration"),
        info([]() -> const char* { return "1. Hold the controller as you always do."; }),
        info([]() -> const char* { return "2. Show Controller on, and look at it."; }),
        info([]() -> const char* { return "3. Move and turn the hand until the"; }),
        info([]() -> const char* { return "controller sits in its palm, just where"; }),
        info([]() -> const char* { return "your real one is."; }),
        toggle("Show Controller", "vr_show_controller")
            .help("Draws each real controller, translucent, where the game tracks it, with its axes (red along the handle, "
                  "green left, blue up) and its point (white: the middle of the handle); a yellow line to the hand's "
                  "point when the calibration moves it. If the preview itself is off your real controller, line it up "
                  "first (Weapon Offsets > Controller Preview). Not saved."),
        slider("Main Hand X (forward)", "vr_handcal_x", -5.f, 5.f, 0.1f, "%+.1f cm").extend().help(moveHelp),
        slider("Main Hand Y (left)", "vr_handcal_y", -5.f, 5.f, 0.1f, "%+.1f cm").extend().help(moveHelp),
        slider("Main Hand Z (up)", "vr_handcal_z", -5.f, 5.f, 0.1f, "%+.1f cm").extend().help(moveHelp),
        slider("Main Hand Pitch (down)", "vr_gunangle", 0.f, 80.f, 0.25f, "%.2f").extend(-180.f, 180.f)
            .help("Gun Angle (vr_gunangle): turns the whole hand, and what it holds, down about the controller's tracked "
                  "point. Changes where it aims."),
        slider("Main Hand Yaw (left)", "vr_gunyaw", -30.f, 30.f, 0.25f, "%+.2f").extend(-180.f, 180.f)
            .help("Gun Yaw (vr_gunyaw): turns the whole hand, and what it holds, left about the controller's tracked "
                  "point. Changes where it aims."),
        slider("Main Hand Roll", "vr_handcal_roll", -30.f, 30.f, 0.25f, "%+.2f").extend(-180.f, 180.f).help(rollHelp),
        cycle("Off Hand", "vr_handcal_off_mirror", {{1.f, "Mirrors the Main Hand"}, {0.f, "Its Own Values"}})
            .help("Mirrors: the off hand takes the main hand's values mirrored (Y, Yaw and Roll the other way). Its Own "
                  "Values: its own sliders, below."),
    };
    handCalPageOwn = vr_handcal_off_mirror.value == 0.f ? 1 : 0;
    if(handCalPageOwn)
    {
        list.insert(list.end(), {
            slider("Off Hand X (forward)", "vr_handcal_off_x", -5.f, 5.f, 0.1f, "%+.1f cm").extend().help(moveHelp),
            slider("Off Hand Y (left)", "vr_handcal_off_y", -5.f, 5.f, 0.1f, "%+.1f cm").extend().help(moveHelp),
            slider("Off Hand Z (up)", "vr_handcal_off_z", -5.f, 5.f, 0.1f, "%+.1f cm").extend().help(moveHelp),
            slider("Off Hand Pitch (down)", "vr_offhandpitch", 0.f, 80.f, 0.25f, "%.2f").extend(-180.f, 180.f)
                .help("Off-Hand Pitch (vr_offhandpitch): turns the whole hand, and what it holds, down about the "
                      "controller's tracked point. Changes where it aims."),
            slider("Off Hand Yaw (left)", "vr_offhandyaw", -30.f, 30.f, 0.25f, "%+.2f").extend(-180.f, 180.f)
                .help("Off-Hand Yaw (vr_offhandyaw): turns the whole hand, and what it holds, left about the "
                      "controller's tracked point. Changes where it aims."),
            slider("Off Hand Roll", "vr_handcal_off_roll", -30.f, 30.f, 0.25f, "%+.2f").extend(-180.f, 180.f).help(rollHelp),
        });
    }
    list.insert(list.end(), {
        action("Match Controller Preview", matchControllerPreview)
            .help("Moves each hand (X, Y, Z only) so that the middle of its empty fist is on the preview's point (the "
                  "middle of the handle, where OpenXR puts the fist). Then turn it (Pitch, Yaw, Roll) to fit, and move "
                  "it again. The off hand only with its own values; needs the jointed hand."),
        action("Reset Moves and Rolls", resetHandMoves)
            .help("Both hands' X, Y, Z and Roll back to their defaults (the calibration the weapon offsets were set with). "
                  "Pitch and Yaw stay."),
        header("Guns"),
        slider("Gun Model Pitch", "vr_gunmodelpitch", -90.f, 90.f, 0.25f, "%.1f").extend(-180.f, 180.f).help("Pitch offset for gun models. Does NOT affect aiming."),
        slider("Gun Model Scale", "vr_gunmodelscale", 0.1f, 2.f, 0.05f, "%.2f").extend(0.02f, 5.f).help("Scale multiplier for gun models. Does NOT affect aiming, but does affect muzzle position (where bullets and projectiles come from)."),
        slider("Gun Model Z Offset", "vr_gunmodely", -10.f, 10.f, 0.1f, "%.1f").extend(-50.f, 50.f).help("Z offset for gun models. Does NOT affect aiming."),
        slider("Gun Z Offset", "vr_gun_z_offset", -30.f, 30.f, 0.25f, "%.1f").extend(-150.f, 150.f).help("Z offset for guns/hands. Affects aiming."),
        slider("Finger Grip Bias", "vr_finger_grip_bias", 0.f, 1.f, 0.05f, "%.2f"),
        toggle("Auto close thumb", "vr_finger_auto_close_thumb"),
        header("Fingers"),
        toggle("Jointed Hand", "vr_hand_rig")
            .help("The hand as one model with three joints a finger and an opposable thumb: the fingers curl smoothly and "
                  "wrap what they hold. Off: the palm and five finger models as before (their six curl frames)."),
        toggle("Fit Fingers to What You Hold", "vr_hand_fit")
            .help("With the jointed hand: the fingers close on what the hand holds (weapons, grabbed things) and stop on its "
                  "surface, never through it and never floating off it. Off: they curl as the controller says."),
        slider("Palm Fit", "vr_hand_fit_palm", 0.f, 10.f, 0.5f, "%.1f cm").extend()
            .help("How far the palm may move to lie against a held thing (a box, a gib, the torch; never more), eased in "
                  "over a tenth of a second. 0: the hand stays where the controller is."),
        slider("Palm Fit: Weapons", "vr_hand_fit_palm_weapon", 0.f, 10.f, 0.5f, "%.1f cm").extend()
            .help("On a weapon (and a hotspot the other hand holds): how far along the grip and onto it the palm's place is "
                  "searched for where the fingers hold best. The same place every time the weapon is taken; the weapon's "
                  "Hand Only offset moves it."),
        slider("Palm Turn", "vr_hand_fit_palm_turn", 0.f, 45.f, 5.f, "%.0f deg").extend()
            .help("How far the palm may turn toward the surface of a held thing (not a weapon's grip)."),
        slider("Fit Overlap: Things", "vr_hand_fit_overlap", 0.f, 1.5f, 0.1f, "%.1f cm").extend()
            .help("How far the fingers and palm may sink into a held thing (a box, a gib, the torch): snug, with no gap, "
                  "and steadier. A weapon has its own (Weapon Offsets: Overlap, and each hotspot's)."),
        slider("Fit Overlap: Hands", "vr_hand_fit_overlap_hands", 0.f, 2.f, 0.1f, "%.1f cm").extend()
            .help("How far a hand cupping the other (a two-handed pistol grip) may sink into it."),
        slider("Pose Blend", "vr_hand_fit_blend", 0.f, 0.3f, 0.01f, "%.2f s").extend()
            .help("How long the fingers take to ease into a new fit. 0: at once."),
        slider("Refit Threshold", "vr_hand_fit_resolve", 0.f, 2.f, 0.1f, "%.1f cm").extend()
            .help("How far the hand must move on what it holds before the fingers are fitted again (smaller: follows "
                  "closer; larger: steadier)."),
        toggle("Fingers Stop at Walls", "vr_hand_walls")
            .help("The fingers bend back where they would go into walls and floors."),
        slider("Hands Brush Weapons", "vr_hand_collide", 0.f, 10.f, 0.5f, "%.1f cm").extend()
            .help("The free hand is pushed aside by the other hand's weapon, up to this far, eased, and then passes "
                  "through. 0: never pushed."),
        toggle("Fingers Brush Weapons", "vr_hand_collide_fingers")
            .help("With Hands Brush Weapons: the free hand's fingers rest on the other hand's weapon or bend out of it, "
                  "instead of the whole hand being pushed by its fingertips."),
        header("Against Monsters and Things"),
        cycle("Weapons Stop at Models", "vr_model_collide", {{0.f, "Off"}, {1.f, "Monsters"}, {2.f, "Monsters and Objects"}})
            .help("A weapon held into a monster, a corpse or another player (and, with Objects, a box, gib or weapon on the "
                  "ground) is drawn stopped at its surface: the model as drawn, not its much bigger box. Drawn only: hits "
                  "and shots still come from your hand."),
        toggle("Hands Stop at Models", "vr_model_collide_hands")
            .help("The same for an empty hand (a fist, an open hand)."),
        slider("Model Push Limit", "vr_model_collide_max", 5.f, 50.f, 1.f, "%.0f cm").extend(1.f, 100.f)
            .help("How far the weapon or hand is held out of a model. Pushed deeper, it is held out less and less, and at "
                  "twice this it lets go and passes through (a monster walking into you)."),
    });
    return list;
}

// Old Quake VR settings: "Player Calibration".
[[nodiscard]] std::vector<Item> pagePlayerCalibration()
{
    return {
        slider("World Scale", "vr_world_scale", 0.5f, 2.f, 0.05f, "%.2f").extend(0.25f, 4.f).help("Scale of the world. Affects movement, height, and hand positions."),
        slider("Floor Offset", "vr_floor_offset", -200.f, 200.f, 2.5f, "%.0f").extend(-400.f, 400.f).help("Offset from the floor. Change if you feel like you're floating or stuck in the floor."),
    };
}

// Old Quake VR settings: "Melee Settings".
[[nodiscard]] std::vector<Item> pageMeleeSettings()
{
    return {
        slider("Swing Speed", "vr_melee_speed", 1.f, 8.f, 0.25f, "%.2f m/s").extend().help("How fast the striking hand must move to hit: a punch, a pommel strike; a weapon's swing a quarter faster, a stab three quarters of it. Any part of the blade then hits. At this speed a hit is weak; twice it, a full hit; faster, harder still."),
        slider("Punch Damage Mult.", "vr_melee_punch_mult", 1.f, 3.f, 0.05f, "%.2f").extend().help("Damage of every punch: straight, hook, uppercut or overhead alike."),
        slider("Damage Multiplier", "vr_melee_dmg_multiplier", 0.25f, 15.f, 0.25f, "%.1f").extend().help("Damage multiplier for melee attacks."),
        slider("Range Multiplier", "vr_melee_range_multiplier", 0.25f, 15.f, 0.25f, "%.1f").extend().help("Range multiplier for melee attacks."),
        cycle("Melee Bloodlust", "vr_melee_bloodlust", {{0.f, "On"}, {1.f, "Off"}}).help("When enabled, restores the player's health when performing melee attacks on enemies."),
        toggle("Parry", "vr_parry").help("Hold a weapon (sword, axe or gun, one hand or two) level across in front of you to block monsters' melee blows."),
        slider("Parry Damage Reduction", "vr_parry_reduction", 0.f, 1.f, 0.05f, "%.2f").help("Share of a parried blow's damage taken away."),
        slider("Parry Drop Chance", "vr_parry_drop_chance", 0.f, 1.f, 0.05f, "%.2f").help("Chance a one-handed parry knocks the weapon out of your hand (two hands: never). Not used with Parry Stamina on (Parry, Bash and Headbutt), which replaces it."),
        slider("Parry Angle", "vr_parry_angle", 15.f, 80.f, 5.f, "%.0f deg").extend().help("How far the weapon may be tilted off level (up or down) and still parry."),
        slider("Parry Reach", "vr_parry_reach", 0.5f, 2.5f, 0.1f, "%.1f m").extend().help("How far in front of you a held weapon still parries."),
        slider("Parry Arm Knock", "vr_parry_wobble", 0.f, 2.f, 0.1f, "%.1f").extend().help("How much a parried blow knocks your hand and arm."),
        slider("Melee Bloodlust Mult.", "vr_melee_bloodlust_mult", 0.05f, 5.f, 0.05f, "%.2f").extend().help("Multiplier for melee bloodlust health restoration."),
    };
}

// Old Quake VR settings: "Aiming Settings".
[[nodiscard]] std::vector<Item> pageAimingSettings()
{
    return {
        cycle("2H Aiming", "vr_2h_mode", {{0.f, "Disabled"}, {1.f, "Basic"}, {2.f, "Virtual Stock"}}).help("'Basic' aims between the two hands; 'Virtual Stock' also uses the shoulder when the weapon is close to it (see Hotspots)."),
        slider("2H Aiming Threshold", "vr_2h_angle_threshold", -1.f, 1.f, 0.05f, "%.2f").help("How far out of line the hands may be before two-handed aiming stops. Higher is stricter; -1 never stops."),
        slider("2H Stock Factor", "vr_2h_virtual_stock_factor", 0.f, 1.f, 0.05f, "%.2f").help("How much the virtual stock matters in the final angle calculation for two-handed aiming. Decrease if you want your hands to matter more."),
        toggle("2H Hand-Off", "vr_2h_handoff").help("Letting go with the hand holding a two-handed weapon leaves it in the other hand: a sword changes hands; a gun hangs from its foregrip, unusable, until a hand takes its handle."),
        header("Weight"),
        info([]() -> const char* {
            return "What you hold follows on a spring.";
        }).help("Heavy things lag, overshoot a little and settle, by their mass, balance and length (Weapon Weights, Held "
                "Object Weights): long ones turn slowly, and they droop a little at arm's length. Two hands hold them much "
                "steadier. The drawn hand stays on it, and the aim and melee follow it. Each weapon and prop multiplies the "
                "Spring settings below with its own."),
        open("Weapon Weights (Held Weapon)", pageIndex(pageWeaponWeights)),
        open("Held Object Weights (Held Prop)", pageIndex(pageHeldObjectWeights)),
        infoLine(weightReadout, 1),
        infoLine(weightReadout, 0),
        header("Tired Arms"),
        toggle("Heavier When Tired", vr_weight_stamina)
            .help("As your stamina (Parry Stamina) runs low, what you hold gets heavier."),
        slider("From Stamina", vr_weight_stamina_from, 0.1f, 1.f, 0.05f, "%.2f")
            .help("Above this share of your stamina the weight is unchanged; below it, it grows smoothly to the most at none."),
        slider("Most Weight", vr_weight_stamina_max, 1.f, 4.f, 0.1f, "%.1fx").extend(1.f, 10.f).help("Times the weight with no stamina left."),
        slider("Curve", vr_weight_stamina_curve, 0.5f, 4.f, 0.1f, "%.1f").extend(0.1f, 10.f)
            .help("How it grows from the threshold to none: 1 in a straight line; 2 (the default) gently at first, then faster."),
        header("Spring"),
        slider("Stiffness", vr_weight_spring_stiffness, 0.25f, 4.f, 0.05f, "%.2fx").extend(0.05f, 20.f)
            .help("How firmly your arm pulls what it holds to your hand: higher lags less."),
        slider("Damping", vr_weight_spring_damping, 0.2f, 2.f, 0.05f, "%.2f").extend(0.f, 5.f)
            .help("1: it settles without swinging back. Lower: it overshoots and wobbles; higher: it creeps in."),
        slider("Arm Strength", vr_weight_spring_strength, 0.f, 4.f, 0.1f, "%.1fx").extend(0.f, 20.f)
            .help("The most force and torque your arm and wrist give it: a heavy thing can't be swung as fast as your hand "
                  "goes. 0: unlimited."),
        slider("Sag", vr_weight_spring_sag, 0.f, 3.f, 0.1f, "%.1fx").extend(0.f, 10.f)
            .help("How much heavy things droop under their weight (more at arm's length)."),
        slider("Swing", vr_weight_spring_inertia, 0.f, 2.f, 0.1f, "%.1fx").extend(0.f, 5.f)
            .help("How much a moving hand swings what it holds about the grip (its weight trailing behind)."),
        slider("Two-Handed Help", vr_weight_spring_2h, 1.f, 8.f, 0.5f, "%.1fx").extend(1.f, 20.f)
            .help("Times the stiffness and strength with both hands on it (4: twice as quick)."),
        slider("Snap Back Beyond", vr_weight_spring_snap, 10.f, 100.f, 5.f, "%.0f cm").extend(1.f, 1000.f)
            .help("Left further behind than this (or three times as many degrees), it is put back in your hand at once."),
        header("Weight and Damage"),
        slider("Weight Damage Exponent", vr_weight_damage_exp, 0.f, 1.f, 0.05f, "%.2f").extend(0.f, 3.f)
            .help("How much the mass of what you strike with or throw changes the damage: 0 none; 0.4 gently (twice as "
                  "heavy: 1.3 times the damage); 1 in proportion. Weapons and props also have their own multipliers."),
        slider("Heavier Than", vr_weight_damage_heavy, 1.f, 30.f, 0.5f, "%.1f kg").extend(0.1f, 500.f)
            .help("Things heavier than this deal more damage (8 kg: every weapon deals its own; an explosive box more)."),
        slider("Lighter Than", vr_weight_damage_light, 0.1f, 10.f, 0.1f, "%.1f kg").extend(0.01f, 500.f)
            .help("Things lighter than this deal less damage (a torch, a flashlight, a small gib). The same as Heavier "
                  "Than: one reference mass, everything heavier more and lighter less."),
        slider("Least Damage", vr_weight_damage_min, 0.1f, 1.f, 0.05f, "%.2fx").extend(0.f, 1.f)
            .help("The weight never takes more than this off the damage."),
        slider("Most Damage", vr_weight_damage_max, 1.f, 5.f, 0.1f, "%.1fx").extend(1.f, 20.f)
            .help("The weight never adds more than this to the damage."),
        slider("Heavy Leniency", vr_weight_lenient, 0.f, 2.f, 0.05f, "%.2f").extend(0.f, 5.f)
            .help("Heavy things can't be swung or thrown fast: their strikes and throws count at lower speeds. 0: off; "
                  "0.75: four times as heavy as Starts At, 0.35 of the speed."),
        slider("Starts At", vr_weight_lenient_from, 1.f, 40.f, 0.5f, "%.1f kg").extend(0.1f, 500.f)
            .help("Things up to this heavy (10 kg: every weapon) strike and hit when thrown at the usual speeds."),
        slider("Least Speed Factor", vr_weight_lenient_min, 0.05f, 1.f, 0.05f, "%.2fx")
            .help("The lowest share of the usual speeds a heavy thing's strikes and throws need."),
    };
}

// Old Quake VR settings: "Immersion Settings" (its weapons' ammo screens and map boards are on the
// Screens page now, its force grab settings on Force Grab).
[[nodiscard]] std::vector<Item> pageImmersionSettings()
{
    return {
        toggle("Body Picks Up Items", "vr_body_interactions").help("Pick up items and level weapons by walking over them. Thrown and dropped weapons still need your hands."),
        toggle("Disable Haptics", "vr_disablehaptics").help("Disable all haptics (vibration)."),
        header("Weapons and Holsters"),
        cycle("Weapon Mode", "vr_holster_mode", {{0.f, "Immersive"}, {1.f, "Quick Slots"}}).help("Immersive: a holster holds one weapon, and taking it out empties it. Quick Slots: holsters never empty (with weapon cycling)."),
        cycle("Weapon Reloading Mode", "vr_reload_mode", {{0.f, "None"}, {1.f, "All Holsters"}, {2.f, "Hip Holsters"}}).help("Enables weapon reloading mechanics. Weapons can be reloaded by moving them towards an holster. Reloading only works with 'immersive' weapon mode."),
        cycle("Holster Haptics", "vr_holster_haptics", {{0.f, "Off"}, {1.f, "Continuous"}, {2.f, "Once"}}).help("Haptic feedback when hovering a usable holster slot."),
        slider("Holster Haptic Time", "vr_holster_haptic_time", 0.05f, 0.6f, 0.05f, "%.2f s").extend().help("How long the buzz lasts as a hand reaches a holster (Holster Haptics: Once)."),
        toggle("Weapon Cycle Mode", "vr_weapon_cycle_mode").help("Allows or disallows cycling weapons using controller buttons."),
        cycle("Weapon Throw Mode", "vr_weapon_throw_mode", {{0.f, "Immersive"}, {1.f, "Vanish on Hit"}, {2.f, "Discard"}}).help("Immersive: thrown weapons can be picked back up. Vanish on Hit and Discard avoid piles of weapons with Quick Slots."),
        slider("Throw Damage Mult.", "vr_weapon_throw_damage_mult", 0.05f, 5.f, 0.05f, "%.2f").extend().help("Multiplier for weapon throw damage."),
        slider("Throw Speed Mult.", "vr_weapon_throw_velocity_mult", 0.05f, 5.f, 0.05f, "%.2f").extend().help("Multiplier for weapon throw velocity."),
        toggle("Dropped Wpn Particles", "vr_weapondrop_particles").help("Enable particle effects for weapons dropped/thrown on the ground."),
        header("Shell Casings"),
        toggle("Shell Casings", "vr_shells").help("The shotgun throws out a spent shell each shot, the super shotgun its two as you reload (a flick reload flings them). They bounce, roll and lie on the floor."),
        slider("Shell Casing Life", "vr_shells_life", 5.f, 120.f, 5.f, "%.0f s").extend().help("How long spent shells lie on the floor before fading out."),
        slider("Shell Casing Sound", "vr_shells_sound", 0.f, 1.f, 0.1f, "%.1f").help("Volume of their tink as they land (0 off)."),
        header("Weapon Drops"),
        cycle("Enemy Weapon Drops", "vr_enemy_drops", {{0.f, "When Eligible"}, {1.f, "Always"}, {2.f, "Disabled"}}).help("Controls random enemy weapon drops. 'Eligible' means that the player has obtained a weapon before through a level weapon pickup."),
        slider("Enemy Drops Chance", "vr_enemy_drops_chance_mult", 0.05f, 5.f, 0.05f, "%.2f").extend().help("Multiplier for enemy weapon drops."),
        cycle("Ammo Box Weapon Drops", "vr_ammobox_drops", {{0.f, "When Eligible"}, {1.f, "Always"}, {2.f, "Disabled"}}).help("Controls random ammo box weapon drops. 'Eligible' means that the player has obtained a weapon before through a level weapon pickup."),
        slider("Ammo Box Drops Chance", "vr_ammobox_drops_chance_mult", 0.05f, 5.f, 0.05f, "%.2f").extend().help("Multiplier for ammo box weapon drops."),
    };
}

[[nodiscard]] std::vector<Item> pageGraphicsLights();
[[nodiscard]] std::vector<Item> pageGraphicsShadows();
[[nodiscard]] std::vector<Item> pageGraphicsSurfaces();
[[nodiscard]] std::vector<Item> pageGraphicsLiquids();
[[nodiscard]] std::vector<Item> pageGraphicsPost();
[[nodiscard]] std::vector<Item> pageGraphicsModels();
[[nodiscard]] std::vector<Item> pageTransparencyOptions();

// Old Quake VR settings: "Graphical Settings", in pages: this one, the preset and the image, and links
// to the others.
[[nodiscard]] std::vector<Item> pageGraphics()
{
    return {
        cycle("Preset", "vr_graphics_preset", {{-1.f, "Custom"}, {0.f, "Off (Quake)"}, {1.f, "Low"}, {2.f, "Medium"}, {3.f, "High"}, {4.f, "Ultra"}})
            .help("Sets the settings on all the Graphics pages: lights, shadows, surfaces, liquids, post-processing, models and effects. Change them after to taste."),
        header("Image"),
        cycle("Anti-aliasing", "vid_fsaa", {{0.f, "Off"}, {2.f, "2x"}, {4.f, "4x"}, {8.f, "8x"}}).help("Multisampling: smoother edges, some GPU time."),
        cycle("Smooth Textures", "vr_texture_smooth", {{0.f, "As gl_texturemode"}, {1.f, "Replacements"}, {2.f, "All"}})
            .help("Smooth filtering for high-resolution replacement textures, or for all textures."),
        toggle("Fence Coverage", "vr_alpha_coverage").help("Fences and grates keep their bars at a distance instead of thinning out and crawling; with Anti-aliasing (MSAA) their edges are smoothed too."),
        toggle("Relit Maps", "vr_relit_maps").help("Maps re-lit with shadows, ambient occlusion and colour, if made (Misc/quakevr/relight_maps.py). Next map."),
        slider("Headset Gamma", "vr_gamma", 0.5f, 1.5f, 0.02f, "%.2f").extend().help("The headset's own gamma (lower is brighter; 1 neutral). The desktop's is not used."),
        slider("Headset Contrast", "vr_contrast", 0.5f, 2.f, 0.02f, "%.2f").extend().help("The headset's own contrast (1 neutral)."),
        header("More Graphics"),
        open("Lights", pageIndex(pageGraphicsLights)),
        open("Shadows", pageIndex(pageGraphicsShadows)),
        open("Surfaces", pageIndex(pageGraphicsSurfaces)),
        open("Liquids", pageIndex(pageGraphicsLiquids)),
        open("Post-processing", pageIndex(pageGraphicsPost)),
        open("Models and Effects", pageIndex(pageGraphicsModels)),
        open("Particles", pageIndex(pageParticleSettings)),
        open("Transparency", pageIndex(pageTransparencyOptions)),
        header("Performance"),
        cycle("Performance Profile", "vr_profile", {{0.f, "Off"}, {1.f, "Record"}, {2.f, "Record and show"}})
            .help("Record CPU and GPU time per effect into quakevr/profile/ every 5 seconds (and show the top costs over the wrist). Not kept after a restart."),
        cycle("Memory Log", "vr_memstats_log", {{0.f, "Off"}, {30.f, "Every 30 s"}, {60.f, "Every minute"}, {300.f, "Every 5 minutes"}})
            .help("Write memory use and the frame rate to quakevr/profile/memstats_<date>.csv, and after each map load."),
        cycle("FPS Counter on the Gadget", "vr_gadget_fps", {{0.f, "Off"}, {1.f, "Basic"}, {2.f, "Detailed"}})
            .help("Under the wrist gadget: the frame rate and the CPU's and GPU's milliseconds a frame (our work, the eyes' drawing; not the runtime's waits). Basic: one line, averaged over half a second. Detailed: now, the last quarter second's average, the last 5 seconds' least and most, the frames that missed a refresh (Late), and graphs of the last 3 seconds against the frame budget (the dotted line); figures over the budget are highlighted."),
    };
}

// Graphics: the light, dynamic lights and the things that make them.
[[nodiscard]] std::vector<Item> pageGraphicsLights()
{
    return {
        slider("Light Contrast", "vr_light_contrast", 1.f, 2.5f, 0.02f, "%.2f").extend().help("Darker shade, lamps as bright: the level's own light sources carry the scene (1: Quake's)."),
        toggle("Coloured Lights", "vr_colored_lights").help("Warm muzzle flashes, rockets and explosions; the lightning gun's blue."),
        cycle("Dynamic Light Falloff", "vr_dlight_falloff", {{0.f, "Quake"}, {1.f, "DarkPlaces"}})
            .help("DarkPlaces: strong lights, full near and smoothly gone at the edge. Quake: dimmer, fading linearly all the way."),
        toggle("Uncapped Dynamic Lights", "vr_dlight_uncapped").help("Dynamic lights add fully to bright walls (Quake caps them)."),
        toggle("Dynamic Lights on Models", "vr_dlight_models").help("Per pixel, by the angle, and shadowed (off: Quake's flat brightening)."),
        slider("Dynamic Light Angle", "vr_dlight_angle", 0.f, 1.f, 0.25f, "%.2f")
            .help("How much surfaces facing away from a dynamic light get less of it (0: Quake's)."),
        header("Light Sources"),
        slider("Muzzle Flash Light", "vr_flash_scale", 0.f, 3.f, 0.05f, "%.2fx").extend().help("How far your shots light up the room (0 off)."),
        slider("Explosion Light", "vr_explosion_light_scale", 0.f, 3.f, 0.05f, "%.2fx").extend(),
        slider("Projectile Light", "vr_projectile_lights", 0.f, 2.f, 0.05f, "%.2fx").extend().help("Hell knight flames, scrag spit, vore balls and lasers light up the room, and where they hit (0 off)."),
        slider("Lava Nail Lights", "vr_lavanail_lights", 0.f, 16.f, 1.f, "%.0f").extend().help("How many lava nails light up the room at once, the nearest; sized by Projectile Light (0 none)."),
        slider("Lava Gun Light", "vr_lavagun_light", 0.f, 3.f, 0.1f, "%.1fx").extend().help("The nailguns loaded with lava nails glow from their barrels, lighting your hand and what is near (0 off)."),
        slider("Lava Gun Light Reach", "vr_lavagun_light_radius", 16.f, 200.f, 4.f, "%.0f").extend().help("How far the lava guns' glow reaches."),
        slider("Lava Gun Light Flicker", "vr_lavagun_light_flicker", 0.f, 1.f, 0.05f, "%.2f").help("How much the lava guns' glow flickers (0 steady)."),
        slider("Lava Gun Light at Rest", "vr_lavagun_light_idle", 0.f, 1.f, 0.05f, "%.2fx").help("Lava guns in your holsters or lying on the floor glow too, this much of a held one's (0 not)."),
        slider("Lightning Beam Lights", "vr_beam_lights", 0.f, 12.f, 1.f, "%.0f").extend().help("Lightning beams light the room along their length with this many flickering lights (0 off)."),
        slider("Torch Lights", "vr_torch_lights", 0.f, 16.f, 1.f, "%.0f").extend().help("Torches and flames flicker a small warm light onto the room: how many at once, the nearest (0 off)."),
        slider("Torch Light Brightness", "vr_torch_light_scale", 0.f, 16.f, 0.25f, "%.2fx").extend().help("How bright the torches' flickering light is, on top of the map's own light."),
        slider("Torch Light Shadows", "vr_torch_light_shadows", 0.f, 4.f, 1.f, "%.0f").extend().help("How many of the nearest torches cast shadows (each takes one of the dynamic lights' shadows; 0 none)."),
        slider("Ammo Screen Light", "vr_weapon_screen_light", 0.f, 3.f, 0.1f, "%.1fx").extend().help("Your weapons' ammo screens cast a small light in their colour on what is in front of them (0 off)."),
    };
}

// Graphics: shadows, and the blob shadows standing in for them.
[[nodiscard]] std::vector<Item> pageGraphicsShadows()
{
    return {
        slider("Shadowed Dynamic Lights", "vr_shadow_dlights", 0.f, 12.f, 1.f, "%.0f").extend()
            .help("Explosions, rockets and other dynamic lights casting shadows, the most important first (0 off)."),
        cycle("Dynamic Shadow Detail", "vr_shadow_dlight_size", {{128.f, "128"}, {256.f, "256"}, {512.f, "512"}, {1024.f, "1024"}})
            .help("Largest shadow face size, in texels; farther lights get smaller ones."),
        toggle("Muzzle Flash Shadows", "vr_shadow_muzzleflash").help("Your own muzzle flashes cast shadows too (brief; costs more)."),
        slider("Map Light Shadows", "vr_shadow_maplights", 0.f, 8.f, 1.f, "%.0f").extend()
            .help("Map lights near you casting the shadows of monsters and of you (0 off)."),
        cycle("Map Shadow Detail", "vr_shadow_maplight_size", {{256.f, "256"}, {512.f, "512"}, {1024.f, "1024"}})
            .help("Their shadow face size, in texels."),
        slider("Map Shadow Strength", "vr_shadow_maplight_strength", 0.f, 1.f, 0.05f, "%.2f")
            .help("How much of a map light those shadows take away."),
        cycle("Your Shadow", "vr_shadow_self", {{0.f, "None"}, {1.f, "Body"}, {2.f, "Body and hands"}})
            .help("What of you casts map lights' shadows."),
        cycle("Shadow Softness", "vr_shadow_filter", {{0.f, "Hard (1 tap)"}, {1.f, "Soft (4 taps)"}, {2.f, "Softer (9)"}, {3.f, "Softest (16)"}}),
        slider("Shadow Distance", "vr_shadow_distance", 256.f, 4096.f, 128.f, "%.0f").extend()
            .help("Lights farther than this from you cast no shadows."),
        cycle("Shadow Atlas", "vr_shadow_atlas", {{2048.f, "2048"}, {4096.f, "4096"}, {8192.f, "8192"}})
            .help("Room for all shadows at once: smaller fits fewer or coarser ones (64 MB at 4096)."),
        slider("Shadow Bias", "vr_shadow_bias", 0.f, 4.f, 0.25f, "%.2f").extend()
            .help("Higher: fewer speckles on lit surfaces; lower: shadows start closer to what casts them."),
        toggle("Shadow Statistics", "vr_shadow_stats").help("Print what is shadowed and its GPU time, once a second, to the console."),
        header("Ambient Occlusion"),
        slider("Contact Shadows", "vr_ao_dynamic", 0.f, 2.f, 0.1f, "%.1f").extend()
            .help("Monsters, items, gibs and your body darken the floor and walls close to them, and each other: soft shadows where things touch (0 off)."),
        slider("Contact Shadow Reach", "vr_ao_dynamic_range", 1.25f, 5.f, 0.25f, "%.2f").extend().help("How far round a thing its darkening reaches, in its own sizes."),
        slider("Door and Lift Shadows", "vr_ao_brush", 0.f, 2.f, 0.1f, "%.1f").extend().help("Doors, lifts, platforms and the item boxes darken the walls and floors close to them (the map's light never saw them; 0 off)."),
        slider("Model Self-Shadowing", "vr_ao_models", 0.f, 2.f, 0.1f, "%.1f").extend().help("Monsters, weapons and pickups are darker in their own creases: armpits, under guns, grooves (worked out as each model loads; 0 off)."),
        header("Blob Shadows"),
        cycle("Blob Shadows", "vr_blob_shadows", {{0.f, "Off"}, {1.f, "Always"}, {2.f, "Auto"}})
            .help("Dark discs under you and things, a cheap stand-in for shadows. Auto: only where the real shadows (above) do not fall."),
        cycle("Blob Shadows: You", "vr_player_shadows", {{0.f, "Off"}, {1.f, "Hands"}, {2.f, "Body"}, {3.f, "Both"}}),
        toggle("Blob Shadows: Things", "vr_entity_shadows").help("Blob shadows under monsters and items, away from their light."),
    };
}

// Graphics: the walls' and models' surfaces: sheen, bumps, parallax, detail.
[[nodiscard]] std::vector<Item> pageGraphicsSurfaces()
{
    return {
        slider("Light Sheen", "vr_specular", 0.f, 0.5f, 0.025f, "%.3f").extend().help("Dynamic lights glint off walls and models (0 off)."),
        toggle("Sheen Anti-Aliasing", "vr_specular_aa").help("Distant bumps' sheen and the water's glints stay steady instead of sparkling as your head moves."),
        header("Bumps"),
        toggle("Bump Maps", "vr_normalmaps").help("Bumps made from the textures, shown by the map's light and dynamic lights. Next map."),
        slider("Bump Depth", "vr_normalmap_strength", 0.f, 3.f, 0.05f, "%.2f").extend(),
        slider("Bumps in Map Light", "vr_normalmap_baked", 0.f, 2.f, 0.1f, "%.1f").extend().help("How much the bumps shade the map's own light, lit from where it seems to come from (0: only dynamic lights show them)."),
        toggle("Real Light Directions", "vr_deluxemap").help("Relit maps know where their light comes from at every point (.lux): the bumps are lit from there and the lamps glint off them. Off: guessed from where the light gets brighter (always so on maps without it)."),
        slider("Bumps on Models", "vr_normalmap_models", 0.f, 2.f, 0.1f, "%.1f").extend().help("How much the bumps shade models' own light (monsters, pickups, weapons; held ones half), lit from the map's lights (times Bumps in Map Light; 0: only dynamic lights show them)."),
        slider("Authored Model Bumps", "vr_normalmap_authored", 0.f, 2.f, 0.1f, "%.1f").extend().help("How deep models' authored normal maps are (a skin's _norm or _bump file: a model pack's, or our own models'); 1 as made. Held ones too."),
        header("Parallax"),
        toggle("Parallax", "vr_parallax").help("Walls and floors look carved: their dark parts sink in and shift as you move your head (needs Bump Maps)."),
        slider("Parallax Depth", "vr_parallax_depth", 0.5f, 6.f, 0.25f, "%.2f").extend().help("How deep the darkest parts of the textures sink, in Quake units."),
        slider("Parallax Distance", "vr_parallax_distance", 128.f, 2048.f, 64.f, "%.0f").extend().help("How far away walls still look carved (further costs more)."),
        slider("Parallax Items Depth", "vr_parallax_items", 0.f, 4.f, 0.25f, "%.2f").extend().help("How deep the ammo and health boxes look carved, in their own units (scaled with the box; 0 off)."),
        slider("Parallax Models Depth", "vr_parallax_models", 0.f, 2.f, 0.125f, "%.3f").extend().help("How deep monsters, pickups and weapons without baked maps look carved, their heights guessed from the skin's paint, in their own units (0 off, the default: on Quake's blocky skins it bends the texels)."),
        slider("Parallax Depth: Authored Models", "vr_parallax_authored", 0.f, 2.f, 0.25f, "%.2f").extend().help("How deep models with authored bump maps look carved (your hands and body, the gadget, the flashlight, the weapons): their real baked relief, 1 as deep as baked, 0 off."),
        header("Detail Textures"),
        toggle("Detail Textures", "vr_detail").help("A fine grain of stone, metal, wood or grime over walls and floors close by, where Quake's textures turn into big blurry texels."),
        slider("Detail Strength", "vr_detail_strength", 0.f, 2.f, 0.1f, "%.1f").extend().help("How strongly the detail textures show."),
        slider("Detail Distance", "vr_detail_distance", 64.f, 384.f, 16.f, "%.0f").extend().help("How far away, in Quake units, the detail textures still show (they fade in closer than this)."),
    };
}

// Graphics: water, slime and lava.
[[nodiscard]] std::vector<Item> pageGraphicsLiquids()
{
    return {
        slider("Waves", "vr_water_waves", 0.f, 2.f, 0.1f, "%.1f").extend().help("Waves on water, slime and lava, in the lighting (the surfaces stay flat). 0: Quake's flat liquids."),
        slider("Water Reflection", "vr_water_fresnel", 0.f, 2.f, 0.1f, "%.1f").extend().help("Water is clearer looking straight down and reflects the room at grazing angles."),
        slider("Water Refraction", "vr_water_refraction", 0.f, 2.f, 0.1f, "%.1f").extend().help("What is under the water is bent by the waves (needs r_oit 1)."),
        slider("Water Glints", "vr_water_glints", 0.f, 2.f, 0.1f, "%.1f").extend().help("Light glinting off the waves."),
        slider("Lava Glow", "vr_water_lava_glow", 0.f, 2.f, 0.1f, "%.1f").extend().help("Lava glows, its hot parts brightest, pulsing slowly."),
        toggle("Real Waves", "vr_water_geo_waves").help("The surfaces of water, slime and lava rise and fall in long swells (still at the walls). Off: flat, the waves only in the lighting."),
        slider("Real Wave Height", "vr_water_geo_amplitude", 0.5f, 24.f, 0.5f, "%.1f").extend().help("How high the swells rise, in units (lava's are bigger and slower, slime's smaller)."),
        slider("Shoreline Foam", "vr_water_foam", 0.f, 2.f, 0.1f, "%.1f").extend().help("Foam where water meets walls, steps and things standing in it; a scum on slime, a glowing rim on lava (0 off)."),
        slider("Heat Haze", "vr_heat_haze", 0.f, 1.f, 0.1f, "%.1f").help("The air shimmers over lava, round explosions and over flames (0 off)."),
        header("Under Water"),
        slider("Caustics", "vr_water_caustics", 0.f, 1.5f, 0.05f, "%.2f").extend().help("Dappled light on floors and walls under water."),
        slider("Underwater View", "vr_water_underwater", 0.f, 1.5f, 0.1f, "%.1f").extend().help("With your head under: the liquid's fog and tint, a little blur. 0: Quake's colour shift."),
        slider("Underwater Wobble", "vr_water_wobble", 0.f, 2.f, 0.1f, "%.1f").extend().help("A slow wobble of the view under water. Turn it down or off if it makes you uneasy."),
        header("Splashes and Ripples"),
        slider("Splashes", "vr_water_splash", 0.f, 2.f, 0.1f, "%.1f").extend().help("Drops, foam and ripples where shots, thrown things, you and your hands hit water, slime or lava, and as you wade: how many (0 off)."),
        slider("Splash Size", "vr_water_splash_size", 0.5f, 2.5f, 0.1f, "%.1f").extend().help("How big the splashes' drops, spray, foam and rings are."),
        slider("Splash Ring Speed", "vr_water_splash_ring_speed", 0.3f, 2.f, 0.1f, "%.1f").extend().help("How fast the rings on the surface spread (1: riding the ripples' crest)."),
        slider("Splash Ring Size", "vr_water_splash_ring_size", 0.f, 2.5f, 0.1f, "%.1f").extend().help("How big the rings start and how long they last, so how far they get (0: none)."),
        toggle("Ripples", "vr_water_ripples").help("Splashes, wading and strokes send rings of waves across the surface, in its lighting and, with Real Waves' grid, its shape."),
        slider("Ripple Height (Geometry)", "vr_water_ripple_amplitude", 0.f, 24.f, 0.5f, "%.1f").extend().help("How high a hand's slap's ripples rise in the surface itself, in units (a shot's about half, a body's two to three times)."),
        slider("Ripple Normal Strength", "vr_water_ripple_normal", 0.f, 2.f, 0.05f, "%.2f").extend().help("How steep the ripples look in the lighting (glints, reflection, refraction), times the geometry's slope: 1 as shaped, 0 the shape only."),
        slider("Ripple Speed", "vr_water_ripple_speed", 10.f, 100.f, 2.f, "%.0f").extend().help("How fast the rings of ripples spread, in units a second (lava's a third, slime's slower)."),
        slider("Ripple Duration", "vr_water_ripple_decay", 0.3f, 4.f, 0.1f, "%.1f").extend().help("Seconds for the ripples to die down to a third."),
        slider("Ripple Wavelength", "vr_water_ripple_wavelength", 16.f, 192.f, 4.f, "%.0f").extend().help("Units between the ripples' crests. In the geometry from two of its 16-unit cells up (vr_water_geo_cell), fully from three; below that in the lighting only."),
        slider("Ripples at Once", "vr_water_ripple_max", 1.f, 32.f, 1.f, "%.0f").extend().help("How many ripples can spread at once; a new one takes the weakest's place. Fewer costs less GPU time when much is splashing."),
        slider("Water Sounds", "vr_water_sounds", 0.f, 1.f, 0.1f, "%.1f").help("Volume of splashes, shots into water, wading and swimming strokes (0 off: Quake's own splash sounds only)."),
    };
}

// Graphics: over the whole image: bloom, tone mapping, the colour grade, dither.
[[nodiscard]] std::vector<Item> pageGraphicsPost()
{
    return {
        slider("Bloom", "vr_bloom", 0.f, 1.5f, 0.02f, "%.2f").extend().help("Glow around lamps, glowing panels and buttons, flashes and explosions (0 off)."),
        slider("Bloom Threshold", "vr_bloom_threshold", 0.3f, 1.f, 0.01f, "%.2f").extend().help("How bright a thing must be to glow."),
        slider("Bloom Size", "vr_bloom_radius", 0.25f, 3.f, 0.05f, "%.2f").extend(),
        slider("Bloom: White Lights", "vr_bloom_white", 0.f, 2.f, 0.02f, "%.2fx").extend().help("Glow of white and pale lights (lamps, flashes), times Bloom."),
        slider("Bloom: Coloured Lights", "vr_bloom_color", 0.f, 3.f, 0.05f, "%.2fx").extend().help("Glow of coloured lights (red buttons, blue panels), times Bloom."),
        slider("Bloom: Adapt", "vr_bloom_adapt", 0.f, 10.f, 0.25f, "%.2f").extend().help("How much bloom weakens when much of the view glows (a brightly lit map); 0: never."),
        header("Tone and Colour"),
        toggle("Tone Mapping", "vr_tonemap").help("Bright lights, flashes, lava and explosions keep their colour and detail instead of clipping to yellow and white (a filmic shoulder). The rest is unchanged. Off: Quake's."),
        slider("Exposure", "vr_exposure", 0.5f, 2.f, 0.05f, "%.2f").extend().help("Brightness before tone mapping (1: as Quake)."),
        cycle("Colour Grade", "vr_grade", {{0.f, "Off"}, {1.f, "Quake VR"}, {2.f, "Cold"}, {3.f, "Warm"}, {4.f, "By Episode"}})
            .help("A subtle colour mood. Quake VR: a touch of contrast, cool shadows, warm highlights. By Episode: cold, mossy, warm, violet for episodes 1 to 4."),
        slider("Grade Strength", "vr_grade_strength", 0.f, 1.f, 0.05f, "%.2f"),
        cycle("Dither", "vr_dither", {{0.f, "Off"}, {1.f, "Per Eye"}, {2.f, "Per Eye, Moving"}, {3.f, "Same in Both Eyes"}})
            .help("A faint noise of one step that hides banding in dark gradients (fog, dark rooms, under water)."),
    };
}

// Graphics: models' light and reflections, glowing screens and sights, particles and decals.
[[nodiscard]] std::vector<Item> pageGraphicsModels()
{
    return {
        toggle("Model Lighting", "vr_model_lighting").help("Monsters, items and weapons are shaded from the map's lights, not one fixed direction."),
        toggle("Models Lit as the World", "vr_model_light_parity").help("Monsters and items as bright as the floor they stand on (off: Quake's brighter models)."),
        toggle("Directional Ambient", "vr_model_ambient_dir").help("Monsters, items, weapons and hands take the light around them: brighter on the side facing lit walls, darker facing shade."),
        slider("Ambient Contrast", "vr_model_ambient_contrast", 0.f, 1.5f, 0.05f, "%.2f").extend().help("How much brighter the lit side is than the shaded one (0: one brightness all round)."),
        slider("Rim Light", "vr_rim_light", 0.f, 1.f, 0.05f, "%.2f").help("A faint light round the edges of monsters and things, in the light around them, so they stand out from dark backgrounds (0 off)."),
        slider("Hands' Least Light", "vr_viewmodel_minlight", 0.f, 32.f, 1.f, "%.0f").extend().help("How lit your hands and weapons stay in the dark (Quake: 24)."),
        toggle("Weapon Reflections", "vr_weapon_reflections").help("The metal of your weapons (barrels, blades) reflects the room around you, dimmer in the dark."),
        slider("Reflection Strength", "vr_weapon_reflections_strength", 0.f, 2.f, 0.1f, "%.1fx").extend().help("How strongly your weapons' metal reflects the room."),
        header("Glow"),
        slider("Screen Glow", "vr_screen_glow", 0.f, 3.f, 0.1f, "%.1fx").extend().help("The wrist gadget's and your weapons' screens glow softly round their edges (0 off)."),
        slider("Screen Text Glow", "vr_screen_text_glow", 0.f, 3.f, 0.1f, "%.1fx").extend().help("The text, numbers and icons on those screens glow: bright whitish cores, a soft halo (0 off)."),
        slider("Weapon Sight Glow", "vr_weapon_glow", 0.f, 3.f, 0.1f, "%.1fx").extend().help("Your weapons' glowing parts, as the shotgun's sights, shine brighter and glow (0: as Quake)."),
        hueSlider("Weapon Sight Hue", vr_sight_hue).help("The shotgun's and double shotgun's iron sights: 30 their own orange, 0 red, 120 green, 240 blue; Player's: the Player Effects Hue (also on the Colours page, with their saturation)."),
        header("Effects"),
        toggle("Soft Particles", "vr_soft_particles").help("Smoke, fire, blood and explosions fade out where they meet walls and floors instead of cutting hard lines through them."),
        slider("Soft Particle Distance", "vr_soft_particles_scale", 0.25f, 3.f, 0.25f, "%.2fx").extend().help("How far in front of walls and floors they start fading (bigger: softer, but thin puffs against a wall fainter)."),
        toggle("Decals", "vr_decals").help("Blood splats, gib trails, scorch marks and bullet chips on walls and floors."),
        slider("Decal Lifetime", "vr_decal_life", 10.f, 600.f, 10.f, "%.0f s").extend(),
        slider("Max Decals", "vr_decal_max", 64.f, 4096.f, 64.f, "%.0f").extend(),
        toggle("Gib Blood", "vr_gib_blood").help("Gibs and heads leave a trail of blood drops and splat where they hit walls and floors. Off: Quake's trail."),
        slider("Gib Blood Trail", "vr_gib_blood_trail", 0.f, 3.f, 0.25f, "%.2fx").extend().help("How dense their trail of blood and drops is (0 none)."),
        slider("Ammo Switch Morph", "vr_weapon_morph_time", 0.f, 1.f, 0.05f, "%.2f s").extend().help("Switching a gun's ammo with its button morphs it into its other look (lava, multi-rockets, plasma) over this long, a glowing seam sweeping over it (0: at once)."),
    };
}

// Old Quake VR settings: "Hud Configuration".
[[nodiscard]] std::vector<Item> pageHudConfiguration()
{
    return {
        cycle("HUD", "vr_hud_mode", {{1.f, "Wrist gadget"}, {0.f, "Status bar"}}).help("The settings below apply to the status bar."),
        cycle("Status Bar Mode", "vr_sbar_mode", {{0.f, "Main Hand"}, {1.f, "Off Hand"}}).help("The offsets below apply to the off hand."),
        slider("HUD Scale", "vr_hud_scale", 0.01f, 0.1f, 0.005f, "%.3f").extend(0.005f, 0.3f),
        slider("Offset X", "vr_sbar_offset_x", -200.f, 200.f, 1.f, "%.0f").extend(-1000.f, 1000.f),
        slider("Offset Y", "vr_sbar_offset_y", -200.f, 200.f, 1.f, "%.0f").extend(-1000.f, 1000.f),
        slider("Offset Z", "vr_sbar_offset_z", -200.f, 200.f, 1.f, "%.0f").extend(-1000.f, 1000.f),
        slider("Roll", "vr_sbar_offset_roll", -3.14f, 3.14f, 0.05f, "%.2f rad").extend(),
        slider("Pitch", "vr_sbar_offset_pitch", -3.14f, 3.14f, 0.05f, "%.2f rad").extend(),
        slider("Yaw", "vr_sbar_offset_yaw", -3.14f, 3.14f, 0.05f, "%.2f rad").extend(),
    };
}

// Old Quake VR settings: "Hotspot Settings".
[[nodiscard]] std::vector<Item> pageHotspotSettings()
{
    return {
        toggle("Show Virtual Stock", "vr_show_virtual_stock"),
        slider("Shoulder X", "vr_shoulder_offset_x", -50.f, 50.f, 0.5f, "%.1f").extend(-200.f, 200.f),
        slider("Shoulder Y", "vr_shoulder_offset_y", -50.f, 50.f, 0.5f, "%.1f").extend(-200.f, 200.f),
        slider("Shoulder Z", "vr_shoulder_offset_z", -50.f, 50.f, 0.5f, "%.1f").extend(-200.f, 200.f),
        slider("Virtual Stock Thresh.", "vr_virtual_stock_thresh", 0.f, 30.f, 0.1f, "%.1f").extend(0.f, 100.f),
        toggle("Show Shoulder Holst.", "vr_show_shoulder_holsters"),
        slider("Shoulder X", "vr_shoulder_holster_offset_x", -50.f, 50.f, 0.5f, "%.1f").extend(-200.f, 200.f),
        slider("Shoulder Y", "vr_shoulder_holster_offset_y", -50.f, 50.f, 0.5f, "%.1f").extend(-200.f, 200.f),
        slider("Shoulder Z", "vr_shoulder_holster_offset_z", -50.f, 50.f, 0.5f, "%.1f").extend(-200.f, 200.f),
        slider("Shoulder Threshold", "vr_shoulder_holster_thresh", 0.f, 30.f, 0.1f, "%.1f").extend(0.f, 100.f),
        slider("Shoulder Pitch", "vr_shoulder_holster_pitch", -180.f, 180.f, 5.f, "%.0f deg").help("Tips the holster's top away from your body (negative: towards it), the gun in it too."),
        slider("Shoulder Yaw", "vr_shoulder_holster_yaw", -180.f, 180.f, 5.f, "%.0f deg").help("Turns the holster's face outwards (negative: inwards), mirrored for the left one."),
        slider("Shoulder Roll", "vr_shoulder_holster_roll", -180.f, 180.f, 5.f, "%.0f deg").help("Tips the holster's top outwards (negative: inwards; a hanging gun's muzzle goes the other way), mirrored for the left one."),
        toggle("Show Hip Holsters", "vr_show_hip_holsters"),
        slider("Hip X", "vr_hip_offset_x", -50.f, 50.f, 0.5f, "%.1f").extend(-200.f, 200.f)
            .help("Forward (negative: back), units. With the body drawn, the default (-3.5) is on the belly; further back, the holsters go round your hips to your back."),
        slider("Hip Y", "vr_hip_offset_y", -50.f, 50.f, 0.5f, "%.1f").extend(-200.f, 200.f),
        slider("Hip Z", "vr_hip_offset_z", -50.f, 50.f, 0.5f, "%.1f").extend(-200.f, 200.f),
        slider("Hip Threshold", "vr_hip_holster_thresh", 0.f, 30.f, 0.1f, "%.1f").extend(0.f, 100.f),
        slider("Hip Pitch", "vr_hip_holster_pitch", -180.f, 180.f, 5.f, "%.0f deg").help("Tips the holster's top away from your body (negative: towards it), the gun in it too."),
        slider("Hip Yaw", "vr_hip_holster_yaw", -180.f, 180.f, 5.f, "%.0f deg").help("Turns the holster's face outwards (negative: inwards), mirrored for the left one."),
        slider("Hip Roll", "vr_hip_holster_roll", -180.f, 180.f, 5.f, "%.0f deg").help("Tips the holster's top outwards (negative: inwards; a hanging gun's muzzle goes the other way), mirrored for the left one."),
        toggle("Show Upper Holsters", "vr_show_upper_holsters"),
        slider("Upper X", "vr_upper_holster_offset_x", -50.f, 50.f, 0.5f, "%.1f").extend(-200.f, 200.f)
            .help("Forward (negative: back), units. With the body drawn, the default (-4.25) is on the chest; further back, the holsters go round your ribs to your back."),
        slider("Upper Y", "vr_upper_holster_offset_y", -50.f, 50.f, 0.5f, "%.1f").extend(-200.f, 200.f),
        slider("Upper Z", "vr_upper_holster_offset_z", -50.f, 50.f, 0.5f, "%.1f").extend(-200.f, 200.f),
        slider("Upper Threshold", "vr_upper_holster_thresh", 0.f, 30.f, 0.1f, "%.1f").extend(0.f, 100.f),
        slider("Upper Pitch", "vr_upper_holster_pitch", -180.f, 180.f, 5.f, "%.0f deg").help("Tips the holster's top away from your body (negative: towards it), the gun in it too."),
        slider("Upper Yaw", "vr_upper_holster_yaw", -180.f, 180.f, 5.f, "%.0f deg").help("Turns the holster's face outwards (negative: inwards), mirrored for the left one."),
        slider("Upper Roll", "vr_upper_holster_roll", -180.f, 180.f, 5.f, "%.0f deg").help("Tips the holster's top outwards (negative: inwards; a hanging gun's muzzle goes the other way), mirrored for the left one."),
        toggle("Show Holster Slots", "vr_leg_holster_model_enabled"),
        slider("Holster Slot Scale", "vr_leg_holster_model_scale", 0.1f, 2.f, 0.05f, "%.2f").extend(0.02f, 5.f),
        slider("Holster Slot X", "vr_leg_holster_model_x_offset", -100.f, 100.f, 0.5f, "%.1f").extend(-400.f, 400.f),
        slider("Holster Slot Y", "vr_leg_holster_model_y_offset", -100.f, 100.f, 0.5f, "%.1f").extend(-400.f, 400.f),
        slider("Holster Slot Z", "vr_leg_holster_model_z_offset", -100.f, 100.f, 0.5f, "%.1f").extend(-400.f, 400.f),
    };
}

// Old Quake VR settings: "Transparency Options".
[[nodiscard]] std::vector<Item> pageTransparencyOptions()
{
    return {
        toggle("(!) No Vis", "r_novis").help("NOT RECOMMENDED: water transparency in maps without vis data, by drawing the whole map every frame. Vispatch maps instead."),
        slider("Water Alpha", "r_wateralpha", 0.f, 1.f, 0.1f, "%.1f").help("How see-through water is (1: opaque). Only in maps vised for it (the relit maps are)."),
        slider("Lava Alpha", "r_lavaalpha", 0.f, 1.f, 0.1f, "%.1f").help("How see-through lava is (1: opaque; 0: as Water Alpha). Only in maps vised for it."),
        slider("Tele Alpha", "r_telealpha", 0.f, 1.f, 0.1f, "%.1f").help("How see-through teleporters are (1: opaque; 0: as Water Alpha). Only in maps vised for it."),
        slider("Slime Alpha", "r_slimealpha", 0.f, 1.f, 0.1f, "%.1f").help("How see-through slime is (1: opaque; 0: as Water Alpha). Only in maps vised for it."),
        toggle("Map's Own Alpha", "vr_map_liquid_alpha").help("Maps that set their liquids' alpha themselves (worldspawn keys) use theirs instead of the sliders above."),
    };
}

