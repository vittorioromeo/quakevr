// vr_menu_settings.inc -- VR Settings: the main page, its parts (Height, Hands, Holsters, Controls, Locomotion,
// Comfort, Display...), the headset, Advanced and the hubs. Part of vr_menu.cpp (included where it was: the same
// translation unit, so its anonymous namespace).

// VR Settings (Options > VR Settings, the main menu's VR Settings, the corner's): what a new player sets, each in a few
// words, in the order they come to it. Every other setting is under Advanced VR Options (ROUND21.md, "VR Settings for
// first-time players"), and so is each row here, on its topic's page. No link to them here (nor to Search): the main
// menu's Advanced VR row and the corner's Advanced VR and Search buttons open them, and the trees of Search and of the
// boards' menu paths start at both pages (menuRoots). Each of its sections is a page of its own, linked from here
// (vrSettingsParts; ROUND22.md, "VR Settings' sections as submenus"); Reset All to Defaults stays here.
za::Vector<Item> pageMain()
{
    return {
        header("Calibration"),
        open("Height and Hand Calibration", pageIndex(pageVrsHeightHands))
            .help("Your height (Set Height Now measures it), the world's scale, the floor's height, and the drawn hands "
                  "moved and turned on your controllers so they sit where your real hands are."),
        open("Body and Equipment", pageIndex(pageVrsBodyEquipment))
            .help("Fit the body to you (the legs to your height, the shoulders, the torso) and move what you wear: the "
                  "belt's height, the flashlight and the ammo pouch. Your body is shown in front of you while you "
                  "choose."),
        // (The author's note vrragtriggers_2026-10-23_01-28-41: a page of VR Settings itself, not Body and Equipment's.)
        open("Holster Calibration", pageIndex(pageVrsHolsters))
            .help("Where the hip, chest and back holsters sit on you: Holster Setup (drag and drop, the recommended "
                  "way), or each pair's sliders; the seated places; the guns over your shoulders."),

        header("Moving"),
        open("Locomotion and Controls", pageIndex(pageVrsLocoControls))
            .help("What opens the menu besides the menu button, where the stick takes you, running or walking, the "
                  "sticks' deadzone and swap, swimming."),
        open("Comfort, Teleport and Turning", pageIndex(pageVrsComfortTurning))
            .help("Blood Feedback, the vignette, the fade on scripted teleports, the view when you die; teleporting to "
                  "where you point; smooth or snap turning."),

        header("Playing"),
        open("Weapons and Bullet Time", pageIndex(pageVrsWeaponsBulletTime))
            .help("Holding the grip or sticky, two-handed guns, the reloading mode, and what starts and stops bullet time."),
        // (The author, 2026-10-22: its own page here, "easier to find"; Body and Equipment had it.)
        open("Item Collection", pageIndex(pageVrsItemCollection))
            .help("How you pick up ammo, health and backpacks (Item Pickup Mode): carry them by hand and put them away at "
                  "a holster, take them the moment you grip them, or walk into them as in Quake."),
        open("Body and Flashlight", pageIndex(pageVrsBodyFlashlight))
            .help("How much of your body you see, the wrist gadget's arm, leaning, Reset Position, and the flashlight."),
        open("Player Appearance", pageIndex(pageVrsAppearance))
            .help("How you look: the body's build, the skin tone, bracers, gloves (none, fingerless or full, in any "
                  "colour) and the pauldrons, in any combination."),
        open("HUD, Haptics and Sound", pageIndex(pageVrsHudHapticsSound))
            .help("The HUD's kind, the crosshair and Doom on the wrist gadget; how strongly the controllers vibrate; the "
                  "game's and the music's volume and spatial sound."),
        open("Saves and Autosaves", pageIndex(pageSaves))
            .help("Load an autosave (the last few of each map are kept), how often the game autosaves, waiting until it "
                  "is safe (no monster after you, your health not low), and the menu you get when you die."),

        header("Sight"),
        open("Display and Graphics", pageIndex(pageVrsDisplayGraphics))
            .help("The headset's brightness and contrast, the light where no lamp reaches, the eyes' render resolution, "
                  "upscaling and foveated rendering, retro looks, antialiasing, bloom and bump mapping."),
        // (The author, 2026-10-23: presets easy to find, with My Settings to go back to; vr_gfxpresets.cpp.)
        open("Graphics Presets", pageIndex(pageGraphicsPresets))
            .help("Off (Quake), Low, Medium, High (as shipped) or Ultra: all the graphics settings at once, for your "
                  "graphics card. My Settings sets your own back; the first line says which the settings match now."),

        header("Reset"),
        action(resetAllIsArmed() ? "Press Again to Reset All" : "Reset All to Defaults", resetAll)
            .help("Every Quake VR setting back to as it shipped, but not your calibration (height, floor, body and hands: "
                  "Reset Hand Offsets for those). Press it twice: the second time within 3 seconds."),
    };
}

// Saves and Autosaves (player feedback: an autosave made in an ambush trapped them, dying at each respawn): the
// rotating autosaves (vr_continue.cpp), when they wait to be safe (host.c Host_AutosaveUnsafe), the death menu (menu.c).
za::Vector<Item> pageSaves()
{
    return {
        command("Load an Autosave", "menu_autosaves")
            .help("The autosaves kept, newest first: each one's map, your health and armor, how far into the level and "
                  "how long ago (the main menu's Load Autosave row too)."),
        header("Autosaves"),
        row(opt::sv_autosave),
        row(opt::sv_autosave_interval),
        row(opt::vr_autosave_level_start),
        row(opt::vr_autosave_slots),
        row(opt::vr_autosave_list_all),
        header("Only When Safe"),
        row(opt::vr_autosave_safe),
        row(opt::vr_autosave_safe_alerted),
        row(opt::vr_autosave_safe_radius),
        row(opt::vr_autosave_safe_health),
        row(opt::vr_autosave_safe_maxdelay),
        header("When You Die"),
        row(opt::vr_death_menu),
        row(opt::vr_death_menu_delay),
        row(opt::sv_autoload),
    };
}

// VR Settings' sections, merged into the pages linked from it (titled "VR Settings - <section>").
[[nodiscard]] za::Vector<Item> pageVrsHeight()
{
    return {
        row(opt::vr_height_calibration),
        action("Set Height Now", calibrateHeight)
            .help("Stand up straight, look straight ahead and press: your height is taken from the headset."),
        row(opt::vr_world_scale),
        row(opt::vr_floor_offset),
        open("Seated Play", pageIndex(pageSeated))
            .help("Playing sitting down: the game stands you up, the holsters and pouches where you reach them seated, "
                  "the floor in reach, a crouch toggle."),
    };
}

[[nodiscard]] za::Vector<Item> pageVrsHands()
{
    const char* handHelp = "Both hands, mirrored: moves or turns the drawn hands (and what they hold) on your controllers, "
                           "so they sit where your real hands are. Show Controller helps; 0 is the shipped calibration.";
    return {
        toggle("Show Controller", "vr_show_controller")
            .help("Draws your real controllers, see-through, where the game tracks them: line the hands up with them "
                  "below. Not saved."),
        row(opt::vr_menu_hands_x).help(handHelp),
        row(opt::vr_menu_hands_y).help(handHelp),
        row(opt::vr_menu_hands_z).help(handHelp),
        row(opt::vr_menu_hands_pitch),
        row(opt::vr_menu_hands_yaw),
        row(opt::vr_menu_hands_roll).help(handHelp),
        action("Reset Hand Offsets", resetHandOffsets)
            .help("Both hands' moves and turns back to the shipped calibration. Each hand's own values: Advanced VR "
                  "Options > Weapons > Hand/Gun Calibration."),
    };
}

// VR Settings > Holster Calibration (the author, 2026-10-22: the holsters' rows on a page of their own, Holster Setup
// first, the recommended way; the body shown in front of you, menu::bodyClonePage; 2026-10-23: out of Body and
// Equipment, under VR Settings' Calibration header).
[[nodiscard]] za::Vector<Item> pageVrsHolsters()
{
    const char* holsterHelp = "Both holsters of the pair, mirrored: moves them forward, inward (towards your middle) or up "
                              "from their shipped place (0), in Quake's units (about 3 cm). Shown on your body while "
                              "you choose here.";
    return {
        action("Holster Setup (drag and drop)", holsterSetupStart)
            .help("Recommended: the game pauses and every holster and pouch shows as a box on your body. Grip one and "
                  "drag it where you want it; grip it with both hands and spread them to make it bigger (or push the "
                  "stick up or down). Symmetry moves the other one of the pair mirrored. Done keeps it, Cancel puts "
                  "everything back. Seated, it sets the seated places. Single player only."),
        row(opt::vr_menu_holster_hip_x)
            .help(holsterHelp),
        row(opt::vr_menu_holster_hip_y)
            .help(holsterHelp),
        row(opt::vr_menu_holster_hip_z)
            .help(holsterHelp),
        row(opt::vr_menu_holster_chest_x)
            .help(holsterHelp),
        row(opt::vr_menu_holster_chest_y)
            .help(holsterHelp),
        row(opt::vr_menu_holster_chest_z)
            .help(holsterHelp),
        row(opt::vr_menu_holster_back_x)
            .help(holsterHelp),
        row(opt::vr_menu_holster_back_y)
            .help(holsterHelp),
        row(opt::vr_menu_holster_back_z)
            .help(holsterHelp),
        open("Seated Holsters", pageIndex(pageSeatedHolsters))
            .help("Playing seated (Height > Seated Play): where the holsters and pouches go then, on top of these."),
        action("Reset Holsters", resetHolsters)
            .help("Every pair of holsters back to its shipped place. Each pair's own values (its turn and reach too): "
                  "Advanced VR Options > Weapons > Hip Holsters, and > Hotspots."),
        row(opt::vr_holster_weapons_draw_shoulder),
    };
}

// VR Settings > Item Collection (the author, 2026-10-22: out of Body and Equipment, under Playing, easier to find; then
// one switch for the clumsy pair of rows): Item Pickup Mode (vr_menu_pickup_mode, a wrapper: vr_carry and
// vr_carry_autocollect as one), and Backpacks Too, which only matters for Auto-Collect. The detailed rows: Advanced
// VR Options > Carrying and Throwing > Carrying.
int itemCollectionPageMode = -1; // the mode the page was built for (Backpacks Too dimmed or not)

[[nodiscard]] za::Vector<Item> pageVrsItemCollection()
{
    itemCollectionPageMode = static_cast<int>(vr_menu_pickup_mode.value);
    // (2026-10-18: "Auto-collect works. It shouldn't be the default, but it should be an option in the basic VR
    // settings".)
    Item backpacks = cycle("Backpacks Too", vr_carry_autocollect, {{1.f, "Off"}, {2.f, "On"}})
                         .help("Auto-Collect: a backpack you grip or force-grab is taken at once too, as the boxes are. "
                               "Off: a backpack is carried and put away at a holster.");
    if(itemCollectionPageMode != 1)
    {
        backpacks = backpacks.unavailableBecause("Only for the Auto-Collect pickup mode: a backpack you grip or "
                                                 "force-grab is taken at once too, as the boxes are.");
    }
    return {
        row(opt::vr_menu_pickup_mode),
        backpacks,
    };
}

// (The author's note e1m1_2026-10-07_22-40-30: its basics here, after the holsters; the rest of it, its turn, its reach
// and the counter's place, on Advanced VR Options > Weapons > Reloading.)
[[nodiscard]] za::Vector<Item> pageVrsAmmoPouch()
{
    return {
        slider("Ammo Pouch Forward", vr_ammo_pouch_x, -10.f, 10.f, 0.25f, "%+.2f").extend(-40.f, 40.f)
            .help("Immersive reloading's pouch on the front of your belt, where you take rounds and magazines: forward "
                  "(negative: back, round the hips), in Quake's units (about 3 cm). With the body drawn, 0 is on the belly."),
        slider("Ammo Pouch Right", vr_ammo_pouch_y, -10.f, 10.f, 0.25f, "%+.2f").extend(-40.f, 40.f)
            .help("The ammo pouch to your right (negative: left), in Quake's units (about 3 cm)."),
        slider("Ammo Pouch Up", vr_ammo_pouch_z, -10.f, 10.f, 0.25f, "%+.2f").extend(-40.f, 40.f)
            .help("The ammo pouch up (negative: down) from the hip holsters' height, in Quake's units (about 3 cm)."),
        slider("Ammo Pouch Size", vr_ammo_pouch_scale, 0.2f, 3.f, 0.05f, "%.2fx").extend(0.05f, 10.f)
            .help("How big the ammo pouch is drawn (how near a hand must come to reach into it: Advanced VR Options > "
                  "Weapons > Reloading > Reach)."),
        toggle("Ammo Counter", vr_ammo_pouch_counter)
            .help("A small screen on the ammo pouch: how many of what it gives (shells, nails, cells) you have left."),
        // (The wobble's rows: Advanced VR Options > Weapons > Reloading > Ammo Pouch Wobble; the author, 2026-10-22: "too
        // fine-grained" for here.)
        row(opt::vr_ammo_pouch_shells_max),
        row(opt::vr_ammo_pouch_mags_max),
    };
}

// Reset Body and Equipment: the shoulders, the torso, the belt, the torch on it, every holster and the ammo pouch's
// place back to as shipped (not Height nor Body Calibration's measurements).
void resetBodyEquipment()
{
    resetHolsters();
    for(cvar_t* var : {&vr_body_tweak_shoulders_back, &vr_body_tweak_shoulders_out, &vr_body_tweak_shoulders_up,
            &vr_body_torso_back, &vr_body_belt_up, &vr_flashlight_forward, &vr_flashlight_out, &vr_flashlight_up,
            &vr_ammo_pouch_x, &vr_ammo_pouch_y, &vr_ammo_pouch_z, &vr_body_leg_fit, &vr_body_gear_fit})
    {
        Cvar_SetQuick(var, var->default_string);
    }
}

// VR Settings > Body and Equipment (ROUND22.md, "Body and Equipment"; players: the flashlight too far back, the ammo
// belt too low, the legs broken by a change of height): the body's fit and the places of what you wear, on one page, the
// body shown in front of you while it is open (menu::bodyClonePage). The ammo pouch's own rows (its page before) are
// here as a part; the holsters' are on VR Settings > Holster Calibration (linked from here before 2026-10-23).
[[nodiscard]] za::Vector<Item> pageVrsBodyEquipment()
{
    const char* cm = "%+.2f m";
    za::Vector<Item> out{
        header("Fit"),
        slider("Height", vr_height_calibration, 1.f, 2.2f, 0.01f, "%.2f m").extend(0.5f, 3.f)
            .help("Your eyes' height standing (Height and Hand Calibration > Set Height Now measures it). The body is "
                  "sized to it; with Legs Fit Your Height the legs still reach the floor if it is off."),
        row(opt::vr_body_leg_fit),
        row(opt::vr_body_gear_fit),
        open("Body Calibration", pageIndex(pageBodyCalibration))
            .help("Measures your arms and shoulders (and your height) from a few poses."),
        header("Shoulders and Torso"),
        slider("Shoulders Back", vr_body_tweak_shoulders_back, -0.1f, 0.1f, 0.005f, cm).extend(-0.3f, 0.3f)
            .help("The shoulders further back (negative: forward), on top of Body Calibration's."),
        slider("Shoulders Out", vr_body_tweak_shoulders_out, -0.1f, 0.1f, 0.005f, cm).extend(-0.3f, 0.3f)
            .help("The shoulders further out to the sides (negative: in)."),
        slider("Shoulders Up", vr_body_tweak_shoulders_up, -0.1f, 0.1f, 0.005f, cm).extend(-0.3f, 0.3f)
            .help("The shoulders higher (negative: lower)."),
        slider("Torso Back", vr_body_torso_back, -0.1f, 0.25f, 0.01f, "%.2f m").extend(-0.5f, 0.5f)
            .help("How far the torso sits behind your neck (negative: in front): further back, looking down shows more "
                  "of the chest and belt; the chest's gear moves with it."),
        header("Belt"),
        slider("Belt Height", vr_body_belt_up, -0.15f, 0.15f, 0.01f, cm).extend(-0.4f, 0.4f)
            .help("Everything on the belt higher (negative: lower): the hip holsters, the ammo pouch, the grenade pouch "
                  "and the flashlight."),
        slider("Flashlight Forward", vr_flashlight_forward, -0.1f, 0.15f, 0.005f, cm).extend(-0.3f, 0.3f)
            .help("The flashlight on the belt further forward (negative: back), out of the belt's front."),
        slider("Flashlight Out", vr_flashlight_out, -0.15f, 0.15f, 0.005f, cm).extend(-0.3f, 0.3f)
            .help("The flashlight further out towards its side (negative: in, towards the buckle)."),
        slider("Flashlight Up", vr_flashlight_up, 0.f, 0.5f, 0.01f, cm).extend(-0.3f, 0.8f)
            .help("The flashlight's clip higher on the body (from the pelvis)."),
    };
    for(Item& it : pageVrsAmmoPouch())
    {
        out.pushBack(it);
    }
    out.pushBack(action("Reset Body and Equipment", resetBodyEquipment)
                     .help("The shoulders, torso, belt, flashlight, holsters (VR Settings > Holster Calibration's) and "
                           "ammo pouch back to as shipped (not Height nor Body Calibration's measurements)."));
    return out;
}

[[nodiscard]] za::Vector<Item> pageVrsControls()
{
    return {cycle("Menu Button", vr_menu_button, menuButtonChoices()).help(menuButtonHelp)};
}

[[nodiscard]] za::Vector<Item> pageVrsLocomotion()
{
    return {
        row(opt::vr_menu_move_towards),
        cycle("Default Speed", "cl_alwaysrun", {{1.f, "Run"}, {0.f, "Walk"}})
            .help("Whether you run or walk; holding the speed button does the other."),
        row(opt::vr_deadzone),
        slider("Stick Response Curve", vr_move_curve, 1.f, 3.f, 0.1f, "%.1f")
            .help("How the moving stick's push becomes speed. 1: straight. Higher: more of the travel for slow, careful "
                  "walking, the top speed still at a full push."),
        toggle("Walk Into Run", vr_move_autorun)
            .help("Walking, pushing the moving stick all the way eases you into a run; a smaller push walks."),
        row(opt::vr_stick_swap),
        cycle("Swimming", vr_swim, {{1.f, "Immersive"}, {0.f, "Vanilla"}})
            .help("Immersive: in water the stick slows and strokes of your hands move you. Vanilla: the stick swims as in "
                  "Quake."),
        toggle("Jump Out of Water", "vr_water_jump") // (the author: here too, as on Movement > Swimming)
            .help("On (Quake's): swimming against an edge with room above it lifts you out. Off: you climb out with your "
                  "hands, grabbing the edge (needs Climbing on; with Climbing off, Quake's jump out stays)."),
    };
}

[[nodiscard]] za::Vector<Item> pageVrsComfort()
{
    return {
        open("Blood Feedback", pageIndex(pageBloodFeedback)).help("Independent peripheral feedback for hits and low health, with timing, strength and directional controls."),
        row(opt::vr_comfort_vignette),
        row(opt::vr_comfort_vignette_strength),
        row(opt::vr_comfort_teleport_fade),
        row(opt::vr_lightstyle_comfort),
        cycle("Death View", vr_death_view, {{0.f, "Off"}, {1.f, "Third Person"}, {2.f, "Immersive"}})
            .help("When you die (not gibbed) your body falls as a ragdoll, thrown by the blow that killed you. Off: no "
                  "body, as in Quake. Third Person: your view stays where your eyes were, your body there to look at. "
                  "Immersive: your view goes into your body's head and follows it as it falls (its place smoothed, its "
                  "turns only sideways, never pitch or roll; the hands hidden; Third Person while a menu is open). Back "
                  "to normal when you respawn. Its fine tuning: Advanced VR Options > Body, Death View."),
    };
}

[[nodiscard]] za::Vector<Item> pageVrsTeleport()
{
    return {
        toggle("Teleport", vr_teleport_enabled)
            .help("Jump to the spot you point at instead of walking there: the gentlest way to get around."),
        row(opt::vr_teleport_range),
    };
}

// Built with snap turning (Snap Angle shown) or smooth (Turn Speed shown): built again when that changes (items()).
[[nodiscard]] za::Vector<Item> pageVrsTurning()
{
    const bool off = vr_enable_joystick_turn.value == 0.f;
    const bool snap = !off && vr_snap_turn.value > 0.f;
    mainPageSnap = off ? 2 : snap ? 1 : 0;
    za::Vector<Item> list{
        row(opt::vr_menu_turning),
    };
    if(off)
    {
        return list;
    }
    if(snap)
    {
        list.pushBack(cycle("Snap Angle", vr_snap_turn, {{30.f, "30"}, {45.f, "45"}, {90.f, "90"}})
                .help("How far each snap turns you, in degrees."));
    }
    else
    {
        list.pushBack(slider("Turn Speed", vr_turn_speed, 1.f, 8.f, 0.25f, "%.2f").extend()
                .help("How fast smooth turning turns you."));
    }
    return list;
}

[[nodiscard]] za::Vector<Item> pageVrsFlashlight()
{
    return {
        row(opt::vr_flashlight),
        row(opt::vr_flashlight_side),
    };
}

[[nodiscard]] za::Vector<Item> pageVrsLighting()
{
    return {
        slider("Ambient Light", "vr_ambient_light", 0.f, 0.4f, 0.01f, "%.2f").extend()
            .help("Light in the places no lamp reaches: raise it if the levels are too dark to see without the "
                  "flashlight (0: none)."),
        slider("Light Contrast", "vr_light_contrast", 1.f, 2.5f, 0.02f, "%.2f").extend()
            .help("Darker shadows with lamps as bright: higher is moodier (1: Quake's)."),
    };
}

[[nodiscard]] za::Vector<Item> pageVrsWeapons()
{
    return {
        row(opt::vr_weapon_grip_mode),
        row(opt::vr_weapon_load_grace),
        cycle("Two-Handed", vr_2h_mode, {{0.f, "Off"}, {1.f, "Basic"}, {2.f, "Virtual Stock"}})
            .help("Hold a gun with both hands to steady it. Virtual Stock: a gun brought near your shoulder also aims "
                  "from it, as against a real stock."),
        cycle("Reloading Mode", vr_reload_mode, reloadChoices())
            .help("Guns have magazines. Immersive: the shotgun is loaded a shell at a time from the ammo pouch on your "
                  "belt, the other guns at the hip holsters. Simple: a gun held at a hip holster reloads. Disabled: no "
                  "reloading. Only with the Immersive weapon mode."),
    };
}

[[nodiscard]] za::Vector<Item> pageVrsBulletTime()
{
    return {
        cycle("Activation", vr_menu_bullettime, bulletTimeChoices())
            .help("What starts and stops bullet time (the world slowed while the gadget's TIME meter lasts). Wrist "
                  "Gadget: tap its screen hard with your other hand (or the butt of its gun). A thumbstick press: that "
                  "press does only this (never its bound key), and the screen tap does nothing. More: Advanced VR "
                  "Options > Combat > Bullet Time."),
        row(opt::vr_bullettime_meter_mode),
        row(opt::vr_bullettime_tap_gesture),
        row(opt::vr_bullettime_tap_blow_quiet),
    };
}

[[nodiscard]] za::Vector<Item> pageVrsBody()
{
    return {
        row(opt::vr_body_mode),
        row(opt::vr_gadget_arm),
        cycle("Leaning Detection", vr_lean_detect, {{1.f, "On"}, {0.f, "Off"}})
            .help("On: leaning over (your head lower and tilted, your hands by your hips) leaves your feet where they "
                  "stand. Off: the body always slides back under your head. How readily: Advanced VR Options > Movement "
                  "> Locomotion."),
        command("Reset Position", "vr_recenter")
            .help("Puts your body back under your head, and facing where you look, if it was left behind (after "
                  "leaning over something, or walking into a wall)."),
    };
}

[[nodiscard]] za::Vector<Item> pageVrsHaptics()
{
    return {
        row(opt::vr_haptics_strength),
    };
}

[[nodiscard]] za::Vector<Item> pageVrsHud()
{
    return {
        row(opt::vr_hud_mode),
        row(opt::vr_crosshair),
        open("Doom on the Wrist Gadget", pageIndex(pageDoom))
            .help("Play Doom on the gadget's screen while you play Quake: touch it with your fingertip."),
    };
}

[[nodiscard]] za::Vector<Item> pageVrsSound()
{
    return {
        row(opt::volume),
        row(opt::bgmvolume),
        toggle("Spatial Sound", vr_snd_spatial)
            .help("Sounds come from where they are around your head, muffled by walls, with the room's echo. Best with "
                  "headphones."),
    };
}

[[nodiscard]] za::Vector<Item> pageVrsDisplay()
{
    return {
        slider("Headset Gamma", "vr_gamma", 0.5f, 1.5f, 0.02f, "%.2f").extend()
            .help("The headset's brightness: lower is brighter (1: neutral)."),
        slider("Headset Contrast", "vr_contrast", 0.5f, 2.f, 0.02f, "%.2f").extend()
            .help("The headset's contrast (1: neutral)."),
    };
}

[[nodiscard]] za::Vector<Item> pageVrsScaling()
{
    return {
        row(opt::vr_render_scale),
        row(opt::vr_dynres),
        row(opt::vr_dynres_target).advanced(),
        row(opt::vr_dynres_min),
        dynresMaxSlider().advanced(),
        row(opt::vr_upscale),
        row(opt::vr_upscale_sharpness),
        row(opt::vr_foveated),
    };
}

[[nodiscard]] za::Vector<Item> pageVrsGraphics()
{
    return {
        toggle("Retro Textures", vr_retro)
            .help("Detailed textures drawn in Quake's chunky, pixelated look, crisp as you move your head."),
        toggle("Retro Lighting", "vr_retrolight")
            .help("Quake's coarse, banded light and blocky shadows, fixed to the walls. Goes with Retro Textures."),
        cycle("Antialiasing", "vid_fsaa", {{0.f, "Off"}, {2.f, "2x"}, {4.f, "4x"}, {8.f, "8x"}})
            .help("Smoother edges: thin lines such as the gaps between planks stop crawling and breaking into dashes "
                  "as your head moves. 4x (as shipped) does most of it; 8x takes nearly twice 4x's GPU time for little more."),
        row(opt::vr_taa),
        row(opt::vr_taa_sharpen),
        row(opt::gl_texture_anisotropy),
        slider("Bloom", "vr_bloom", 0.f, 1.5f, 0.02f, "%.2f").extend()
            .help("A glow around lamps, glowing panels, flashes and explosions (0: off)."),
        toggle("Tone Mapping", "vr_tonemap")
            .help("Bright lights, lava and explosions keep their colour and detail instead of turning flat yellow and "
                  "white."),
        toggle("Bump Mapping", "vr_normalmaps")
            .help("Walls and floors get bumps and grooves that catch the light. From the next map."),
        row(opt::vr_pbr),
        row(opt::vr_pbr_strength),
        toggle("Parallax Mapping", "vr_parallax")
            .help("Walls and floors look carved: their dark parts sink in as you move your head (needs Bump Mapping)."),
    };
}

// Sections joined on one page, each under a small header (ROUND22.md, "VR Settings submenus merged"). The parts keep
// their own builders (and their own rows); the merged page lists them in turn.
void addVrsPart(za::Vector<Item>& out, const char* title, za::Vector<Item> part)
{
    out.pushBack(header(title));
    for(Item& it : part)
    {
        out.pushBack(it);
    }
}

[[nodiscard]] za::Vector<Item> pageVrsHeightHands()
{
    za::Vector<Item> out;
    addVrsPart(out, "Height", pageVrsHeight());
    addVrsPart(out, "Hands", pageVrsHands());
    return out;
}

[[nodiscard]] za::Vector<Item> pageVrsLocoControls()
{
    za::Vector<Item> out;
    addVrsPart(out, "Locomotion", pageVrsLocomotion());
    addVrsPart(out, "Controls", pageVrsControls());
    return out;
}

[[nodiscard]] za::Vector<Item> pageVrsComfortTurning()
{
    za::Vector<Item> out;
    addVrsPart(out, "Comfort", pageVrsComfort());
    addVrsPart(out, "Teleportation", pageVrsTeleport());
    addVrsPart(out, "Turning", pageVrsTurning());
    return out;
}

[[nodiscard]] za::Vector<Item> pageVrsWeaponsBulletTime()
{
    za::Vector<Item> out;
    addVrsPart(out, "Weapons", pageVrsWeapons());
    addVrsPart(out, "Bullet Time", pageVrsBulletTime());
    return out;
}

[[nodiscard]] za::Vector<Item> pageVrsBodyFlashlight()
{
    za::Vector<Item> out;
    addVrsPart(out, "Body", pageVrsBody());
    addVrsPart(out, "Flashlight", pageVrsFlashlight());
    return out;
}

// The ranger's head on the body placed live (vr_rangerhead.cpp; ROUND22.md, "The ranger's head: sliders"), the body
// clone (vr_bodyclone.cpp) shown while the page is.
[[nodiscard]] za::Vector<Item> pageVrsRangerHead()
{
    return {
        row(opt::vr_body_ranger_head),
        header("Size and Neck"),
        row(opt::vr_body_head_size),
        row(opt::vr_body_head_sink),
        header("Position"),
        row(opt::vr_body_head_forward),
        row(opt::vr_body_head_side),
        row(opt::vr_body_head_up),
        header("Orientation"),
        row(opt::vr_body_head_pitch),
        row(opt::vr_body_head_yaw),
        row(opt::vr_body_head_roll),
        header("Preview"),
        row(opt::vr_menu_body_clone_distance),
        row(opt::vr_menu_body_clone_side),
        command("Reset Head Placement",
            "vr_body_head_size 1.3; vr_body_head_sink 4.5; vr_body_head_forward 0; vr_body_head_side 0; vr_body_head_up 0; "
            "vr_body_head_pitch 0; vr_body_head_yaw 0; vr_body_head_roll 0")
            .help("Back to the shipped size, sink and place."),
    };
}

// How you look: every cosmetic setting of the body and the hands, any combination of them (ROUND22.md, "Gloves;
// Player Appearance"). Their one home: the Body pages link here.
[[nodiscard]] za::Vector<Item> pageVrsAppearance()
{
    return {
        header("Body"),
        row(opt::vr_body_build),
        open("Ranger Head", pageIndex(pageVrsRangerHead))
            .help("The Quake ranger's head on your body (on or off), its size, place and turn, and how far it sinks over "
                  "the neck, with a mirror image of your body beside the menu while the page is open."),
        row(opt::vr_skin_tone),
        row(opt::vr_body_bracers),
        row(opt::vr_hand_shape),
        row(opt::vr_voice),
        row(opt::vr_voice_pitch),
        row(opt::vr_voice_formant),
        command("Hear Voice", "vr_voice_hear")
            .help("One of your voice's sounds, as set now (a different one each press)."),
        header("Gloves"),
        row(opt::vr_gloves),
        row(opt::vr_glove_hue),
        row(opt::vr_glove_saturation),
        row(opt::vr_glove_lightness),
        row(opt::vr_glove_thickness),
        row(opt::vr_glove_paint),
        row(opt::vr_glove_relief),
        header("Nails"),
        row(opt::vr_nails),
        row(opt::vr_nail_hue),
        row(opt::vr_nail_saturation),
        row(opt::vr_nail_lightness),
        row(opt::vr_nail_gloss),
        action("Randomize", randomAppearance)
            .help("A new look at random: skin tone, gloves (none, fingerless or full) and their colour, painted nails "
                  "and their colour, the body shape (feminine one time in four, else your other shape kept) and the hands' "
                  "shape (feminine with the feminine body, else one time in ten) and the voice (feminine nine times in "
                  "ten with the feminine body, else one time in ten). Nothing else changes. Press again for "
                  "another (vr_appearance_random)."),
        header("Pauldrons"),
        row(opt::vr_body_pauldrons),
        row(opt::vr_body_pauldron_style),
        row(opt::vr_body_pauldron_size),
        header("Ghost Hands"),
        row(opt::vr_ghost_hands),
        row(opt::vr_ghost_hands_which),
        row(opt::vr_ghost_hands_alpha),
        row(opt::vr_ghost_hands_lightness),
        row(opt::vr_ghost_hands_tint),
        row(opt::vr_ghost_hands_fade_from),
        row(opt::vr_ghost_hands_fade_range),
        row(opt::vr_ghost_hands_ignore_recoil),
    };
}

[[nodiscard]] za::Vector<Item> pageVrsHudHapticsSound()
{
    za::Vector<Item> out;
    addVrsPart(out, "HUD", pageVrsHud());
    addVrsPart(out, "Haptics", pageVrsHaptics());
    addVrsPart(out, "Sound", pageVrsSound());
    return out;
}

[[nodiscard]] za::Vector<Item> pageVrsDisplayGraphics()
{
    za::Vector<Item> out;
    addVrsPart(out, "Display", pageVrsDisplay());
    addVrsPart(out, "Lighting", pageVrsLighting());
    addVrsPart(out, "Scaling", pageVrsScaling());
    addVrsPart(out, "Graphics", pageVrsGraphics());
    return out;
}


// Split from VR Settings: its body and display settings (the quick ones; all of them under Advanced VR Options).
[[nodiscard]] za::Vector<Item> pageBodyDisplay()
{
    return {
        header("Sides"),
        row(opt::vr_handedness),
        row(opt::vr_stick_swap),
        cycle("Wrist Gadget Arm", vr_gadget_arm, {{0.f, "Left"}, {1.f, "Right"}}).help("The arm the wrist gadget (the HUD) is on."),
        cycle("Flashlight Side", vr_flashlight_side, {{0.f, "Left"}, {1.f, "Right"}}).help("The side of your belt the torch hangs on (Chest Flashlight; Always on Head: the temple it is worn at)."),

        header("Body"),
        slider("World Scale", vr_world_scale, 0.5f, 2.f, 0.05f, "%.2f").extend(0.25f, 4.f)
            .help("How big the world feels around you (1.25: Quake's sizes as a person sees them)."),
        slider("Floor Offset", vr_floor_offset, -50.f, 30.f, 1.f, "%.0f").extend(-400.f, 400.f).advanced()
            .help("Moves the floor up or down: change it if you feel you are floating or sunk in the floor."),
        cycle("Chest Flashlight", vr_flashlight, {{0.f, "Off"}, {1.f, "Immersive"}, {2.f, "Always on Head"}})
            .help("Immersive: a torch on your belt (Flashlight Side): pull the trigger with an open hand at it to switch it on "
                  "or off; press the grip to take it. B or Y clips it on a gun or on your head. Always on Head: no torch on the "
                  "belt, one worn at your temple, on from the start: the trigger with an open hand at it switches it."),
        cycle("Body", vr_body_mode, {{0.f, "Only Hands"}, {2.f, "Torso and Arms"}, {3.f, "Full Body"}})
            .help("How much of your body you see: only the hands; the torso and arms; or all of it, legs and all."),
        open("Build: Player Appearance", pageIndex(pageVrsAppearance))
            .help("The body's build, skin tone, bracers, gloves and pauldrons are on VR Settings > Player Appearance."),
        row(opt::vr_leg_holster_model_enabled),
        row(opt::vr_holster_weapons_draw_hip),
        row(opt::vr_holster_weapons_draw_upper),
        toggle("Shoulder Holster Weapons", vr_holster_weapons_draw_shoulder)
            .help("Draws the guns in the two holsters on your back, reached over your shoulders (the highest pair; off "
                  "by default: they can get in the way of your view). Off: not drawn; you still take them and put them "
                  "away there."),

        header("Display"),
        row(opt::vr_sbar_mode),
        row(opt::vr_mirror),
        open("Recording (Window View)", pageIndex(pageRecording))
            .help("What the desktop window shows for recording: a steadied mirror or a spectator camera."),
    };
}

// Under OpenXR Runtime: the runtime the backend chose and why (vr_xr_runtime.hpp).
[[nodiscard]] const char* xrRuntimeLine()
{
    const char* line = xrruntime::statusLine();
    return line[0] ? line : "Chosen when VR starts";
}

// Split from VR Settings: the headset.
[[nodiscard]] za::Vector<Item> pageHeadset()
{
    return {
        header("Headset"),
        row(opt::vr_enabled),
        action("Restart VR", restartVr).help("Starts the headset again: if the picture froze or stayed black, or after changing the runtime."),
        row(opt::vr_xr_runtime),
        info(xrRuntimeLine),
        command("Create Bug Report", "bugreport")
            .help("Zips this run's and the last run's logs (the console, the OpenXR runtime's start and messages, the "
                  "crash report if any, the settings, the runtimes' own logs) into quakevr\\bugreports\\<date>.zip, "
                  "and opens the folder on the desktop: attach it to your bug report. After a crash, the Start menu's "
                  "'Quake VR Unleashed (Create Bug Report)' makes it without starting the game."),
        row(opt::vr_xr_runtime_fallback).advanced(),
        row(opt::vr_xr_steamvr_wait).advanced(),
        row(opt::vr_xr_eye_scale).advanced(),
        row(opt::vr_xr_unfocused).advanced(),
        row(opt::vr_xr_unfocused_pause).advanced(),
        row(opt::vr_xr_unfocused_volume).advanced(),
        row(opt::vr_xr_exit_quits).advanced(),
        row(opt::vr_xr_late_acquire).advanced(),
        row(opt::vr_xr_vendor_profiles).advanced(),
        row(opt::vr_xr_log_timing).advanced(),
        slider("Render Scale", vr_render_scale, 0.5f, 1.5f, 0.05f, "%.2f").extend(0.25f, 2.f)
            .help("Eye rendering resolution, times the headset's (SteamVR's resolution included); resampled to it."), // + the size (renderScaleHelp)
        toggle("Dynamic Resolution", vr_dynres)
            .help("When the graphics card can't keep up with the headset, the eyes are drawn smaller for a while (down to "
                  "Dynamic Lowest Scale: VR Settings > Display and Graphics) and upscaled, instead of missing frames; back up "
                  "slowly once it can. With headroom it stays at Render Scale."),
        cycle("Upscaling", vr_upscale, {{0.f, "Bilinear"}, {1.f, "FSR"}, {2.f, "NIS"}})
            .help("Below Render Scale 1: how the eyes are enlarged to the headset's size. FSR (AMD) and NIS (NVIDIA) keep edges and text sharper than bilinear, near the lens centre."),
        slider("Sharpness", vr_upscale_sharpness, 0.f, 1.f, 0.05f, "%.2f")
            .help("The upscaler's sharpening (FSR, NIS). Too much makes edges shimmer."),
        cycle("Foveated Rendering", vr_foveated, {{0.f, "Off"}, {1.f, "Conservative"}, {2.f, "Balanced"}, {3.f, "Aggressive"}})
            .help("Shade the scene coarser towards the edges of the lenses, where they blur anyway: faster. NVIDIA GPUs only (variable-rate shading)."),
        row(opt::vr_visibility_mask),
        row(opt::vr_nearclip).advanced(),
        row(opt::vr_nearclip_held).advanced(),
        row(opt::vr_depth_float).advanced(),
    };
}

// Advanced VR Options: every page, in groups (a page for each group of many, the others here).
za::Vector<Item> pageAdvanced()
{
    return {
        header("Setup"),
        open("Body and Display", pageIndex(pageBodyDisplay)).help("Each hand's side, world scale, the body, the holsters and their guns shown, the status bar, the desktop mirror."),
        open("Headset", pageIndex(pageHeadset)).help("VR on or off, the OpenXR runtime, render scale, upscaling and foveated rendering."),
        open("Sound", pageIndex(pageSound)).help("Spatial audio: sounds around your head (HRTF), muffled by walls, the room's reverb, underwater, your weapons in your hands, Doppler, sounds at your ear."),
        open("Tips", pageIndex(pageTips)).help("Tips for new players, shown once near what they are about: floating by it or on the wrist gadget."),
        open("Changed Settings", pageIndex(pageChanged))
            .help("Every setting you changed from its default, from all the pages, on one page (each marked * where it lives)."),
        command("Run VR Calibration Again", "vr_setup")
            .help("The calibration room and its steps, as at the first start (the main menu's VR Calibration): your height, "
                  "your body, and the main settings on its wall buttons. Ends the game you are in."),
        header("Game"),
        open("Play", pageIndex(pagePlay)),
        open("Combat", pageIndex(pageCombat)).help("Melee, parry and bash, stamina, batting and catching, damage and knockback."),
        open("Arcane Dimensions", pageIndex(pageArcaneDimensions)) // AD support
            .help("Arcane Dimensions tweaks: independent player and enemy pellet speeds, ammo appearance and feature scope."),
        open("Movement", pageIndex(pageMovement)).help("Locomotion, climbing, swimming and the grappling hook."),
        open("Carrying and Throwing", pageIndex(pageCarryingHub))
            .help("Carrying, throwing, force grab, wall torches, rocks and bricks, gibs; what you hold's offsets and weights."),
        open("World", pageIndex(pageGameplay)).help("Monsters, the weapons they drop, weapon drops, rumble and heartbeat."),
        open("Gore", pageIndex(pageGore)),
        header("Body and Weapons"),
        open("Body", pageIndex(pageBody)).help("The body, its arms and pauldrons, body and player calibration, the Death View."),
        open("Flashlight", pageIndex(pageFlashlight)),
        open("Weapons", pageIndex(pageWeaponsHub))
            .help("Weapon offsets and weights, hand/gun calibration, fingers, aiming, weight, holsters and immersion."),
        header("Display"),
        open("HUD and Menus", pageIndex(pageHudHub)).help("Wrist gadget, screens, colours, status bar, crosshair and menu."),
        open("Graphics", pageIndex(pageGraphics)),
        header("Playtesting"),
        open("Motion Recorder", pageIndex(pageMotionRecorder)),
        open("Review Takes", pageIndex(pageReviewTakes)),
        open("Debug", pageIndex(pageDebug))
            .help("Voice notes; debug views, logs and reports; the profiler and memory log; reloads, dumps and tests."),
        info([]() -> const char* { return buildVersionLine(); }),
    };
}

za::Vector<Item> pageCombat()
{
    return {
        open("Melee", pageIndex(pageMeleeSettings)).help("Swings and punches, bloodlust, the headbutt."),
        open("Parry and Bash", pageIndex(pageParryBash)).help("Parrying, bashing and shoving, counter-attacks, the training dummy's blows."),
        open("Stamina", pageIndex(pageStamina)).help("What parries, shoves, blows and hanging from a hold cost, and being exhausted."),
        open("Batting and Catching", pageIndex(pageBatting)).help("Batting projectiles back; catching and returning grenades; hand grenades."),
        open("Damage and Knockback", pageIndex(pageDamage)).help("Hit detection, damage to monsters and to you, headshots, knockback, hits knocking your hands."),
        open("Monster Health", pageIndex(pageMonsterHealth))
            .help("Big monsters (ogres, shamblers, giants, most bosses) appear with less health, by a curve on their "
                  "health pool; small ones keep theirs. The curve, each group's own multiplier, examples."),
        open("Weapon Damage", pageIndex(pageWeaponDamage))
            .help("Every weapon's base damage, to balance them: your guns, the mission packs', melee weapons, the weapons "
                  "monsters drop, thrown weapons."),
        open("Enemy Weapons", pageIndex(pageEnemyWeapons))
            .help("The swords, chainsaws, shotguns and laser rifles monsters drop: their fuel, ammo and handling."),
        open("Enemy Shoves", pageIndex(pageEnemyShoves))
            .help("Grunts and enforcers shove you away when you stand too close: how close, how soon, how hard."),
        open("Armed Grunts (Experimental)", pageIndex(pageArmedGrunts))
            .help("Grunts holding one of your weapons (the toolgun's Armed Grunts, a map's monster_vr_grunt): which "
                  "weapons, their damage to you, their ammo, disarming them."),
        open("Armed Humanoid Replacements", pageIndex(pageArmedRepl))
            .help("A map's grunts and enforcers as armed ones holding their own guns (their balance kept), and weapon "
                  "variety on top. All off by default."),
        open("Armed Humanoids' AI Behaviour", pageIndex(pageArmedAI))
            .help("How armed grunts and enforcers fight: aggressiveness, accuracy, reaction time, fire rate, retreat, "
                  "weapon search, and how much the skill level changes them (Difficulty Impact)."),
        open("Survival Arena", pageIndex(pageArena))
            .help("The map vrarena's waves: how big they are, how many foes are up at once, their boss waves, what a "
                  "wave clear drops and pays, the perks a boss clear offers, and how far the game's difficulty (Skill) "
                  "moves all of it."),
        open("Enemy Melee", pageIndex(pageEnemyMelee))
            .help("Monsters' melee blows: the arc ahead of them where they land and their reach (both growing with "
                  "size), how fast they turn after you while swinging, how close they come; Show Melee Arcs."),
        open("Ragdoll Triggers", pageIndex(pageKdTriggers))
            .help("Hits knock humanoids down as ragdolls by stairs and ledges, on slopes, from a close shotgun blast, a "
                  "strong blow, an explosion or badly hurt: each trigger's settings, Monster Resistance."),
        open("Knockdowns", pageIndex(pageKnockdowns))
            .help("Your shoves can knock monsters down as ragdolls, alive: the chances, how long they stay down, getting up."),
        open("Holding Enemies", pageIndex(pageHoldingEnemies))
            .help("Experimental: grip a living enemy with an empty hand to hold on to it, slow it and pull it a little."),
        open("Monster Groups", pageIndex(pageMonsterGroups))
            .help("Similar monsters of every game share a group (grunts, enforcers, knights, death knights, ogres, "
                  "zombies...): each group's own setting for every ragdoll trigger and every action on it (shove, push, "
                  "hits, leg sweep, leg grabs, lift, neck snap, manhandle, two-hand throw, hold). By group or by action."),
        open("Bullet Time", pageIndex(pageBulletTime))
            .help("The wrist gadget's button slows the world for as long as its meter lasts; Sandevistan; its look."),
        open("Burning", pageIndex(pageBurning))
            .help("What lit torches and lava nails set on fire: the damage (it never stacks), how long, the flames, "
                  "corpses and crates, setting things on fire by touch, nails through a torch's flame."),
        open("Stealth AI", pageIndex(pageStealth))
            .help("Enhanced AI (on/off: Quake's monsters at once). Monsters idle, alert or hostile: seeing you by the light "
                  "on you, hearing noises, investigating, spreading the alarm; sneak attacks."),
        open("Arcane Dimensions", pageIndex(pageArcaneDimensions))
            .help("The Arcane Dimensions mod's projectile pellets: whose rules play where (Auto, Vanilla, Arcane "
                  "Dimensions), their spread, speed and damage."),
    };
}

za::Vector<Item> pageMovement()
{
    return {
        open("Locomotion", pageIndex(pageLocomotionSettings)).help("Moving, turning, teleport, leaning and room scale."),
        open("Climbing", pageIndex(pageClimbing)),
        open("Swimming", pageIndex(pageSwimSettings)),
        open("Grappling Hook", pageIndex(pageGrapple)),
        open("Player Hitbox", pageIndex(pageHitbox)).help("How wide you are against walls, doors, monsters and players."),
        open("Monster Hitbox", pageIndex(pageMonsterHitbox)).help("How wide monsters are against walls (a prototype, off by default)."),
    };
}

za::Vector<Item> pageCarryingHub()
{
    return {
        open("Carrying", pageIndex(pageCarrying)).help("Ammo and health boxes, explosive boxes, armour and pickups."),
        open("Throwing and Physics", pageIndex(pageThrowing)),
        open("Force Grab", pageIndex(pageForceGrab)),
        open("Wall Torches", pageIndex(pageWallTorches)),
        open("Rocks and Bricks", pageIndex(pageRocksBricks)),
        open("Crates", pageIndex(pageCrates)).help("Wooden crates by the walls: break them, carry them, hide behind them."),
        open("Gibs and Corpses", pageIndex(pageGibs)),
        header("What You Hold"),
        open("Held Object Offsets (Held Prop)", pageIndex(pageHeldObjectOffsets)),
        open("Held Object Weights (Held Prop)", pageIndex(pageHeldObjectWeights)),
    };
}

// The programmatic weapon effects (vr_weaponfx.cpp): their global settings; each weapon's are in Weapon Offsets > Effects.
za::Vector<Item> pageWeaponEffects()
{
    return {
        header("Recoil"),
        row(opt::vr_weapon_recoil),
        row(opt::vr_recoil_amount),
        row(opt::vr_recoil_2h_kick),
        row(opt::vr_recoil_2h_rise),
        row(opt::vr_recoil_2h_time),
        row(opt::vr_recoil_2h_haptic),
        header("Shotgun Auto Pump"),
        row(opt::vr_autopump_time),
        row(opt::vr_autopump_delay),
        row(opt::vr_autopump_sound),
        row(opt::vr_autopump_haptics),
        header("Muzzle Flash"),
        row(opt::vr_muzzle_flash),
        row(opt::vr_muzzle_flash_enemies),
        row(opt::vr_muzzle_smoke_enemies),
        header("Gunfire Particles"),
        row(opt::vr_gunfx),
        row(opt::vr_gunfx_sparks),
        row(opt::vr_gunfx_spark_amount),
        row(opt::vr_gunfx_spark_size),
        row(opt::vr_gunfx_spark_speed),
        row(opt::vr_gunfx_smoke),
        row(opt::vr_gunfx_smoke_amount),
        row(opt::vr_gunfx_smoke_size),
        row(opt::vr_gunfx_smoke_speed),
        row(opt::vr_gunfx_smoke_direction),
        row(opt::vr_gunfx_smoke_alpha),
        row(opt::vr_gunfx_port),
        row(opt::vr_gunfx_backblast),
        row(opt::vr_gunfx_heat),
        row(opt::vr_gunfx_wisps),
        row(opt::vr_gunfx_wisp_delay),
        row(opt::vr_gunfx_wisp_time),
        row(opt::vr_gunfx_wisp_fade),
        row(opt::vr_gunfx_wisp_width),
        row(opt::vr_gunfx_wisp_spread),
        row(opt::vr_gunfx_wisp_amount),
        row(opt::vr_gunfx_wisp_opacity),
        row(opt::vr_gunfx_wisp_rise),
        row(opt::vr_gunfx_impacts),
        row(opt::vr_gunfx_ozone),
        row(opt::vr_gunfx_enemies),
        header("Bullet Tracers"),
        row(opt::vr_tracers),
        row(opt::vr_tracers_enemies),
        header("Each Weapon"),
        open("Weapon Offsets (Held Weapon)", pageIndex(pageWeaponOffsets))
            .help("The held weapon's own: its Effects section turns its recoil and flash on, and changes its tracers."),
        command("Test the Held Weapon's Effects", "vr_weaponfx_test 1 3")
            .help("vr_weaponfx_test 1 3: the main hand's weapon kicks and flashes as if it fired, with 3 tracers (no shot)."),
    };
}

// The lightning gun in water (vr_lg_water*; QC weapons.qc VR_LGWater_*, vr_shock.cpp).
za::Vector<Item> pageLightningWater()
{
    return {
        row(opt::vr_lg_water),
        command("Test the Shock Effect", "vr_shock_test 0").help("Shows the shock's flash and arcs, without the damage."),
        command("Test the Electrified Water", "vr_shock_test 1").help("Arcs on the water below the point ahead of you, for 3 seconds."),
    };
}

za::Vector<Item> pageWeaponsHub()
{
    return {
        header("Tuning"),
        open("Weapon Offsets (Held Weapon)", pageIndex(pageWeaponOffsets)),
        open("Weapon Weights (Held Weapon)", pageIndex(pageWeaponWeights)),
        open("Hand/Gun Calibration", pageIndex(pageHandGunCalibration)),
        open("Fingers and Collisions", pageIndex(pageFingersCollisions)),
        header("Handling"),
        open("Aiming", pageIndex(pageAimingSettings)).help("Two-handed aiming; how weight feels: the spring, tired arms."),
        open("Weight and Damage", pageIndex(pageWeightDamage)),
        open("Weapon Damage", pageIndex(pageWeaponDamage)).help("Every weapon's base damage, to balance them."),
        open("Incandescent Blades", pageIndex(pageHotBlades))
            .help("Melee weapons held in a flame or lava glow red-hot for a while: harder blows that set enemies on fire."),
        open("Immersion", pageIndex(pageImmersionSettings)).help("Holsters, reloading, throwing weapons, shell casings, haptics."),
        open("Reloading", pageIndex(pageReloading)).help("Reloading's mode; the ammo pouch; a page per gun (its load point, its magazine, holding and pulling it)."),
        open("Lightning Gun in Water", pageIndex(pageLightningWater)).help("The shock fired under water, and electrified water."),
        open("Weapon Effects", pageIndex(pageWeaponEffects)).help("Recoil, muzzle flashes and bullet tracers."),
        header("Holsters"),
        open("Hotspots", pageIndex(pageHotspotSettings)).help("The virtual stock, the shoulder and upper holsters."),
        open("Hip Holsters", pageIndex(pageHipHolsters)).help("The hip holsters and their slots' models; the grenade pouch at your back."),
    };
}

// Weapons > Incandescent Blades (QC vr_hotblade.qc; the glow: vr_meleehud.cpp hotGlow).
za::Vector<Item> pageHotBlades()
{
    return {
        row(opt::vr_hotblade),
        row(opt::vr_hotblade_time),
        row(opt::vr_hotblade_fade),
        row(opt::vr_hotblade_ignite),
        row(opt::vr_hotblade_ignite_corpses),
        row(opt::vr_hotblade_quench),
        header("Look and Sound"),
        row(opt::vr_hotblade_glow),
        row(opt::vr_hotblade_light),
        row(opt::vr_hotblade_embers),
        row(opt::vr_hotblade_trail),
    };
}

za::Vector<Item> pageHudHub()
{
    return {
        open("Wrist Gadget", pageIndex(pageGadget)),
        open("Screens", pageIndex(pageScreens)),
        open("Colours", pageIndex(pageColours)),
        open("Status Bar", pageIndex(pageHudConfiguration)),
        open("Crosshair", pageIndex(pageCrosshairSettings)),
        open("Menu", pageIndex(pageMenuSettings)),
    };
}
