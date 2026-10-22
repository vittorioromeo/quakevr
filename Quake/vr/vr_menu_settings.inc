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
                  "belt's height, the flashlight, the ammo pouch, the hip, chest and back holsters. Your body is shown in "
                  "front of you while you choose."),

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

        header("Reset"),
        action(ui().resetAllArmed ? "Press Again to Reset All" : "Reset All to Defaults", resetAll)
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
        toggle("Autosave", "sv_autosave").help("Saves the game by itself in quiet moments (and as each level starts)."),
        slider("Every", "sv_autosave_interval", 10.f, 300.f, 10.f, "%.0f s")
            .help("About how often it autosaves: it waits for a quiet moment (not hurt, not firing, slowing down)."),
        toggle("As Each Level Starts", "vr_autosave_level_start")
            .help("An autosave a moment after each level starts (a new map, not a loaded game)."),
        slider("Autosaves Kept", "vr_autosave_slots", 1.f, 9.f, 1.f, "%.0f per map")
            .help("How many of each map's latest autosaves are kept: the oldest is replaced by a new one. 1: only the "
                  "last one, overwritten."),
        toggle("List Every Game's", "vr_autosave_list_all")
            .help("Load Autosave and the death menu list the autosaves of every campaign and map package, not only "
                  "the one you are playing (their first row switches it too)."),
        header("Only When Safe"),
        toggle("Wait Until Safe", "vr_autosave_safe")
            .help("An autosave due waits until the checks below pass, then it is made: so it never traps you in a fight."),
        toggle("Not While Hunted", "vr_autosave_safe_alerted")
            .help("Not while a living monster is after you, wherever it is."),
        slider("Monsters in Sight Within", "vr_autosave_safe_radius", 0.f, 1500.f, 50.f, "%.0f units")
            .help("Not while a living monster you can see is this near (a unit is 3 cm; 0: off), awake or not."),
        slider("Health at Least", "vr_autosave_safe_health", 0.f, 100.f, 5.f, "%.0f%%")
            .help("Not while your health is below this share of its maximum (0: off). Healing ends this wait."),
        slider("Longest Wait for Monsters", "vr_autosave_safe_maxdelay", 0.f, 300.f, 10.f, "%.0f s")
            .help("After this long waiting for the monsters (one stuck hunting you), it autosaves anyway; your health "
                  "must still be high enough. 0: waits as long as it takes."),
        header("When You Die"),
        toggle("Death Menu", "vr_death_menu")
            .help("A moment after you die: Respawn (your last save), Restart the Level, or any autosave kept, newest "
                  "first. Back: you stay dead, fire respawns as ever."),
        slider("Death Menu After", "vr_death_menu_delay", 0.5f, 5.f, 0.5f, "%.1f s"),
        cycle("Respawn", "sv_autoload", {{0.f, "Restarts the Level"}, {2.f, "Loads the Last Save"}})
            .help("What respawning does (fire when dead, the death menu's Respawn): load your last save (manual or "
                  "auto), or start the level again."),
    };
}

// VR Settings' sections, merged into the pages linked from it (titled "VR Settings - <section>").
[[nodiscard]] za::Vector<Item> pageVrsHeight()
{
    return {
        slider("Height", vr_height_calibration, 1.f, 2.2f, 0.01f, "%.2f m").extend(0.5f, 3.f)
            .help("Your real height: it puts your eyes at the right height in the game and fits the body to you. Set "
                  "Height Now measures it."),
        action("Set Height Now", calibrateHeight)
            .help("Stand up straight, look straight ahead and press: your height is taken from the headset."),
        slider("World Scale", vr_world_scale, 0.5f, 2.f, 0.05f, "%.2f").extend(0.25f, 4.f)
            .help("How big the world feels around you: higher makes it bigger. 1.25: Quake's sizes as a person would "
                  "see them."),
        slider("Floor Offset", vr_floor_offset, -50.f, 30.f, 1.f, "%.0f").extend(-400.f, 400.f)
            .help("Moves the floor up or down: change it if you feel you are floating above it or sunk into it."),
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
        slider("Hand Forward", vr_menu_hands_x, -5.f, 5.f, 0.1f, "%+.1f cm").extend(-20.f, 20.f).help(handHelp),
        slider("Hand Inward", vr_menu_hands_y, -5.f, 5.f, 0.1f, "%+.1f cm").extend(-20.f, 20.f).help(handHelp),
        slider("Hand Up", vr_menu_hands_z, -5.f, 5.f, 0.1f, "%+.1f cm").extend(-20.f, 20.f).help(handHelp),
        slider("Hand Pitch", vr_menu_hands_pitch, -30.f, 30.f, 0.5f, "%+.1f").extend(-90.f, 90.f)
            .help("Both hands: tilts the hands and the guns in them up or down on the controllers, until aiming feels "
                  "natural. Changes where they aim."),
        slider("Hand Yaw", vr_menu_hands_yaw, -30.f, 30.f, 0.5f, "%+.1f").extend(-90.f, 90.f)
            .help("Both hands, mirrored: turns the hands and the guns in them inward or outward on the controllers. "
                  "Changes where they aim."),
        slider("Hand Roll", vr_menu_hands_roll, -30.f, 30.f, 0.5f, "%+.1f").extend(-90.f, 90.f).help(handHelp),
        action("Reset Hand Offsets", resetHandOffsets)
            .help("Both hands' moves and turns back to the shipped calibration. Each hand's own values: Advanced VR "
                  "Options > Weapons > Hand/Gun Calibration."),
    };
}

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
        slider("Hip Holsters Forward", vr_menu_holster_hip_x, -10.f, 10.f, 0.5f, "%+.1f").extend(-40.f, 40.f)
            .help(holsterHelp),
        slider("Hip Holsters Inward", vr_menu_holster_hip_y, -10.f, 10.f, 0.5f, "%+.1f").extend(-40.f, 40.f)
            .help(holsterHelp),
        slider("Hip Holsters Up", vr_menu_holster_hip_z, -10.f, 10.f, 0.5f, "%+.1f").extend(-40.f, 40.f)
            .help(holsterHelp),
        slider("Chest Holsters Forward", vr_menu_holster_chest_x, -10.f, 10.f, 0.5f, "%+.1f").extend(-40.f, 40.f)
            .help(holsterHelp),
        slider("Chest Holsters Inward", vr_menu_holster_chest_y, -10.f, 10.f, 0.5f, "%+.1f").extend(-40.f, 40.f)
            .help(holsterHelp),
        slider("Chest Holsters Up", vr_menu_holster_chest_z, -10.f, 10.f, 0.5f, "%+.1f").extend(-40.f, 40.f)
            .help(holsterHelp),
        slider("Back Holsters Forward", vr_menu_holster_back_x, -10.f, 10.f, 0.5f, "%+.1f").extend(-40.f, 40.f)
            .help(holsterHelp),
        slider("Back Holsters Inward", vr_menu_holster_back_y, -10.f, 10.f, 0.5f, "%+.1f").extend(-40.f, 40.f)
            .help(holsterHelp),
        slider("Back Holsters Up", vr_menu_holster_back_z, -10.f, 10.f, 0.5f, "%+.1f").extend(-40.f, 40.f)
            .help(holsterHelp),
        open("Seated Holsters", pageIndex(pageSeatedHolsters))
            .help("Playing seated (Height > Seated Play): where the holsters and pouches go then, on top of these."),
        action("Reset Holsters", resetHolsters)
            .help("Every pair of holsters back to its shipped place. Each pair's own values (its turn and reach too): "
                  "Advanced VR Options > Weapons > Hip Holsters, and > Hotspots."),
        toggle("Shoulder Holster Weapons", vr_holster_weapons_draw_shoulder)
            .help("Draws the guns in the two holsters on your back, reached over your shoulders (the highest pair). Off: "
                  "out of your view; you still take them and put them away there. The other pairs: Advanced VR Options "
                  "> Body and Display."),
        // (2026-10-22: Carry Ammo and Health also here, next to Auto-Collect, which only matters with it on; also
        // Physics > Carrying Boxes.)
        toggle("Carry Ammo and Health", vr_carry)
            .help("Grip a box or a backpack to carry it, push it with a hand or gun. Off: classic Quake, walking into a "
                  "box takes it (and Auto-Collect has no effect)."),
        // (2026-10-18: "Auto-collect works. It shouldn't be the default, but it should be an option in the basic VR
        // settings"; also Physics > Carrying Boxes.)
        cycle("Auto-Collect", vr_carry_autocollect, {{0.f, "Off"}, {1.f, "Ammo and Health"}, {2.f, "And Backpacks"}})
            .help("Off: a box you grip is put away at a holster (let go of it there). Ammo and Health: an ammo or health "
                  "box is taken the moment a hand grips it or a force grab brings it in, seen shrinking into the hand "
                  "(full up of it: it stays in the hand). And Backpacks: a backpack too."),
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
        slider("Ammo Pouch Wobble", vr_ammo_pouch_wobble, 0.f, 2.f, 0.1f, "%.1fx").extend(0.f, 3.f)
            .help("How much the rounds in the ammo pouch sway and jostle as you walk and turn (0: still): how far they "
                  "go at most."),
        toggle("Ammo Pouch Wobble Each Round", vr_ammo_pouch_wobble_each)
            .help("Each shell or magazine sways on its own (a little quicker or slower than the others; lying shells "
                  "slide and tilt), never through each other. Off: they all sway together, as one block."),
        slider("Ammo Pouch Wobble Springiness", vr_ammo_pouch_wobble_freq, 0.5f, 5.f, 0.1f, "%.1f Hz").extend(0.2f, 8.f)
            .help("How fast the rounds sway back and forth: higher is stiffer and quicker, lower looser and slower."),
        slider("Ammo Pouch Wobble Settle Time", vr_ammo_pouch_wobble_settle, 0.2f, 4.f, 0.1f, "%.1f s").extend(0.1f, 10.f)
            .help("How long the rounds take to settle after a move (their swing down to a twentieth)."),
        slider("Ammo Pouch Wobble Intensity", vr_ammo_pouch_wobble_intensity, 0.f, 3.f, 0.1f, "%.1fx").extend(0.f, 5.f)
            .help("How hard a move swings the rounds (Ammo Pouch Wobble caps how far they go)."),
        slider("Shells Shown in the Pouch", vr_ammo_pouch_shells_max, 1.f, 20.f, 1.f, "%.0f")
            .help("The most shells piled in the ammo pouch: one a shell up to this many."),
        slider("Magazines Shown in the Pouch", vr_ammo_pouch_mags_max, 1.f, 6.f, 1.f, "%.0f")
            .help("The most magazines (or cells) standing in the ammo pouch: up to three in a row, then two rows. Pouch "
                  "reloading takes the one your gun's well meets."),
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
// body shown in front of you while it is open (menu::bodyClonePage). The holsters' and the ammo pouch's own rows (their
// pages before) are here as parts.
[[nodiscard]] za::Vector<Item> pageVrsBodyEquipment()
{
    const char* cm = "%+.2f m";
    za::Vector<Item> out{
        header("Fit"),
        slider("Height", vr_height_calibration, 1.f, 2.2f, 0.01f, "%.2f m").extend(0.5f, 3.f)
            .help("Your eyes' height standing (Height and Hand Calibration > Set Height Now measures it). The body is "
                  "sized to it; with Legs Fit Your Height the legs still reach the floor if it is off."),
        toggle("Legs Fit Your Height", vr_body_leg_fit)
            .help("The legs lengthen or shorten (up to a quarter) so that, standing at the height the headset sees you "
                  "stand at, the feet are on the floor and the knees straight: changing Height no longer breaks the legs "
                  "or needs Body Calibration again. Off: the legs as the body's proportions make them."),
        toggle("Gear Fits Your Body", vr_body_gear_fit)
            .help("The holsters and the pouches are placed for your body's size (Height, World Scale) and the floor, so "
                  "the belt sits on the belt for a tall player too. Off: placed as before (low on a tall body)."),
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
    out.pushBack(header("Holsters"));
    for(Item& it : pageVrsHolsters())
    {
        out.pushBack(it);
    }
    out.pushBack(action("Reset Body and Equipment", resetBodyEquipment)
                     .help("The shoulders, torso, belt, flashlight, holsters and ammo pouch back to as shipped (not Height "
                           "nor Body Calibration's measurements)."));
    return out;
}

[[nodiscard]] za::Vector<Item> pageVrsControls()
{
    return {cycle("Menu Button", vr_menu_button, menuButtonChoices()).help(menuButtonHelp)};
}

[[nodiscard]] za::Vector<Item> pageVrsLocomotion()
{
    return {
        cycle("Move Towards", vr_menu_move_towards, {{1.f, "Head"}, {2.f, "Left Hand"}, {3.f, "Right Hand"}})
            .help("Where pushing the stick forward takes you: where you look (Head), or where that hand points, so you "
                  "can walk one way and look another."),
        cycle("Default Speed", "cl_alwaysrun", {{1.f, "Run"}, {0.f, "Walk"}})
            .help("Whether you run or walk; holding the speed button does the other."),
        slider("Stick Deadzone", vr_deadzone, 0.f, 50.f, 5.f, "%.0f%%")
            .help("How far a stick must be pushed before it does anything. Raise it if you drift or turn without "
                  "touching the stick: worn sticks often need 20-25%."),
        toggle("Swap Stick Functions", vr_stick_swap)
            .help("Off: the left stick moves you and the right one turns. On: the right stick moves, the left turns."),
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
        cycle("Vignette", vr_comfort_vignette, {{0.f, "Off"}, {1.f, "Moving and Turning"}, {2.f, "Moving Only"}, {3.f, "Turning Only"}})
            .help("Darkens the edges of your view while the sticks move or turn you: it eases motion sickness for many. "
                  "Your own steps in the room never do it."),
        slider("Vignette Strength", vr_comfort_vignette_strength, 0.1f, 1.f, 0.1f, "%.1f")
            .help("How dark and how wide the vignette is: at 1 you see through a narrow tunnel."),
        slider("Fade on Scripted Teleports", vr_comfort_teleport_fade, 0.f, 2.f, 0.1f, "%.1f s")
            .help("When the game moves you somewhere else at once (a boss sending you to another arena), your view goes "
                  "black and comes back over this long. 0: no fade."),
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
        slider("Teleport Range", vr_teleport_range, 100.f, 800.f, 50.f, "%.0f").extend(100.f, 3000.f)
            .help("How far a teleport reaches, in Quake's units (one is about 3 cm)."),
    };
}

// Built with snap turning (Snap Angle shown) or smooth (Turn Speed shown): built again when that changes (items()).
[[nodiscard]] za::Vector<Item> pageVrsTurning()
{
    const bool off = vr_enable_joystick_turn.value == 0.f;
    const bool snap = !off && vr_snap_turn.value > 0.f;
    mainPageSnap = off ? 2 : snap ? 1 : 0;
    za::Vector<Item> list{
        cycle("Turning Mode", vr_menu_turning, {{0.f, "Smooth"}, {1.f, "Snap"}, {2.f, "Off"}})
            .help("Smooth: the turning stick turns you steadily. Snap: each push turns you at once by Snap Angle, which "
                  "many find more comfortable. Off: the stick never turns you, in any state (its other uses stay): "
                  "you turn in real life."),
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
        cycle("Flashlight", vr_flashlight, {{0.f, "Off"}, {1.f, "Immersive"}, {2.f, "Always on Head"}})
            .help("Immersive: a torch on your belt: pull the trigger with an open hand at it to switch it on or off; press "
                  "the grip to take it in your hand. Always on Head: worn at your temple, on from the start: pull the "
                  "trigger with an open hand at it to switch it."),
        cycle("Flashlight Side", vr_flashlight_side, {{0.f, "Left"}, {1.f, "Right"}})
            .help("The side of your belt the torch hangs on (Always on Head: the temple it is worn at)."),
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
        cycle("Weapon Grip", vr_weapon_grip_mode, {{0.f, "Hold"}, {1.f, "Sticky"}})
            .help("Hold: keep the grip pressed to hold a weapon. Sticky: a press takes it, another lets it go."),
        slider("Grip Grace After Loading", vr_weapon_load_grace, 0.f, 2.f, 0.05f, "%.2f s")
            .help("After loading a save, keep the restored weapons in your hands this long even without holding the grip. "
                  "Starts when play resumes; 0 disables it."),
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
        cycle("Meter Refill", "vr_bullettime_meter_mode", {{1.f, "Earned by Damage"}, {0.f, "Over Time"}})
            .help("Earned by Damage: the TIME meter never fills by itself; hurting enemies fills it (six grunts' worth "
                  "fills it from empty; less while slowed). Over Time: it fills by itself after each use. Either way it "
                  "is kept from level to level and in saves, full on a new game."),
        cycle("Screen Tap", "vr_bullettime_tap_gesture", {{0.f, "Single Tap"}, {1.f, "Double Tap"}})
            .help("Activation by the Wrist Gadget: Double Tap, two quick taps on its screen (a single one does nothing, "
                  "so a stray knock never starts it); Single Tap, one hard tap."),
        slider("Quiet After Blows", "vr_bullettime_tap_blow_quiet", 0.f, 1.5f, 0.05f, "%.2f s").extend(0.f, 3.f)
            .help("Screen taps don't count for this long after a melee blow of yours lands, or you shove or bash, nor "
                  "while your hands move fast (Calm Hands Speed, Advanced VR Options > Combat > Bullet Time): no slow "
                  "motion by accident in a melee. 0: off."),
    };
}

[[nodiscard]] za::Vector<Item> pageVrsBody()
{
    return {
        cycle("Body Type", vr_body_mode, {{3.f, "Full Body"}, {2.f, "Torso and Arms"}, {0.f, "Only Hands"}})
            .help("How much of your body you see: all of it, legs and all; the torso and arms; or only the hands."),
        cycle("Wrist Gadget Arm", vr_gadget_arm, {{0.f, "Left"}, {1.f, "Right"}})
            .help("The arm the wrist gadget (health, armour and ammo) is on."),
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
        slider("Vibration Strength", vr_haptics_strength, 0.f, 1.f, 0.1f, "%.1f").extend(0.f, 2.f)
            .help("How strongly the controllers vibrate (0: never)."),
    };
}

[[nodiscard]] za::Vector<Item> pageVrsHud()
{
    return {
        cycle("HUD", vr_hud_mode, {{1.f, "Wrist Gadget"}, {0.f, "Status Bar"}})
            .help("Wrist Gadget: health, armour and ammo on your wrist. Status Bar: Quake's, on a hand."),
        cycle("Crosshair", vr_crosshair, {{0.f, "Off"}, {1.f, "Dot"}, {2.f, "Laser"}, {3.f, "Soft laser"}})
            .help("A mark where your gun aims: a dot, or a laser beam from the muzzle."),
        open("Doom on the Wrist Gadget", pageIndex(pageDoom))
            .help("Play Doom on the gadget's screen while you play Quake: touch it with your fingertip."),
    };
}

[[nodiscard]] za::Vector<Item> pageVrsSound()
{
    return {
        slider("Volume", "volume", 0.f, 1.f, 0.05f, "%.2f").help("The game's sounds."),
        slider("Music Volume", "bgmvolume", 0.f, 1.f, 0.05f, "%.2f").help("The music's volume."),
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
        slider("Render Scale", vr_render_scale, 0.5f, 1.5f, 0.05f, "%.2f").extend(0.25f, 2.f)
            .help("Eye rendering resolution, times the headset's. Above 1: smoother edges, slower. Below 1: faster, "
                  "blurrier."),
        toggle("Dynamic Resolution", vr_dynres)
            .help("When the graphics card can't keep up with the headset, the eyes are drawn smaller for a while (down to "
                  "Dynamic Lowest Scale) and upscaled, instead of missing frames; back up slowly once it can. With "
                  "headroom it stays at Render Scale: nothing changes."),
        slider("Dynamic GPU Budget", vr_dynres_target, 0.5f, 1.f, 0.05f, "%.2f").advanced()
            .help("The share of each headset frame the eyes' GPU time may take before the scale drops (0.85: 85 %; the "
                  "rest is left for the headset's runtime and the window). Lower: drops sooner, steadier."),
        slider("Dynamic Lowest Scale", vr_dynres_min, 0.4f, 1.f, 0.05f, "%.2f").extend(0.25f, 2.f)
            .help("The lowest scale Dynamic Resolution goes to under load (times the headset's resolution)."),
        dynresMaxSlider().advanced(),
        cycle("Upscaling", vr_upscale, {{0.f, "Bilinear"}, {1.f, "FSR"}, {2.f, "NIS"}})
            .help("Below Render Scale 1: how the image is enlarged to the headset's size. FSR (AMD) and NIS (NVIDIA) "
                  "keep it sharper than Bilinear."),
        slider("Sharpening", vr_upscale_sharpness, 0.f, 1.f, 0.05f, "%.2f")
            .help("How much the upscaler sharpens the image (FSR, NIS). Too much makes edges shimmer."),
        cycle("Foveated Rendering", vr_foveated, {{0.f, "Off"}, {1.f, "Conservative"}, {2.f, "Balanced"}, {3.f, "Aggressive"}})
            .help("Draws the edges of the view, which the lenses blur anyway, in less detail: faster. NVIDIA graphics "
                  "cards only."),
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
        toggle("Temporal Antialiasing (TAA)", "vr_taa")
            .help("vr_taa (experimental): smooths edges and shimmer by blending each frame with the last ones, following "
                  "how everything moved. Replaces Antialiasing (MSAA) while on. Can leave faint trails behind fast hands."),
        slider("TAA Sharpening", "vr_taa_sharpen", 0.f, 1.f, 0.05f, "%.2f")
            .help("vr_taa_sharpen: sharpens the image after TAA, which softens textures a little (0: off)."),
        cycle("Anisotropic Filtering", "gl_texture_anisotropy", {{1.f, "Off"}, {2.f, "2x"}, {4.f, "4x"}, {8.f, "8x"}, {16.f, "16x"}})
            .help("Keeps floors and walls seen at a glancing angle sharp instead of blurry (smooth-filtered textures). "
                  "Little GPU time; 16x is the sharpest. Capped at what the graphics card supports."),
        slider("Bloom", "vr_bloom", 0.f, 1.5f, 0.02f, "%.2f").extend()
            .help("A glow around lamps, glowing panels, flashes and explosions (0: off)."),
        toggle("Tone Mapping", "vr_tonemap")
            .help("Bright lights, lava and explosions keep their colour and detail instead of turning flat yellow and "
                  "white."),
        toggle("Bump Mapping", "vr_normalmaps")
            .help("Walls and floors get bumps and grooves that catch the light. From the next map."),
        toggle("Generated Companion Maps", "vr_pbr")
            .help("Adds normal and height maps generated for the exact wall or floor image. Keeps authored maps. From the next map."),
        slider("Generated Relief Strength", "vr_pbr_strength", 0.f, 4.f, 0.1f, "%.1fx").extend(0.f, 8.f)
            .help("Makes generated bumps and grooves stronger. 1: the generated shape; 0: flat. Authored normal and height components keep their own strength."),
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
        toggle("Use the Ranger's Head", vr_body_ranger_head)
            .help("The head your body has in the mirror, the third-person views and shadows (never in your own eyes): "
                  "the Quake ranger's, taken from your game's player model, or off for the plain placeholder head. "
                  "Takes effect at the next start; the rows below apply at once."),
        header("Size and Neck"),
        slider("Size", vr_body_head_size, 0.5f, 2.f, 0.05f, "%.2fx")
            .help("How big the head is (1 the first version's size, a little small)."),
        slider("Neck Sink", vr_body_head_sink, 0.f, 12.f, 0.5f, "%.1f cm")
            .help("Lowers the head over the neck, and the neck's top with it, so less of the neck shows."),
        header("Position"),
        slider("Forward", vr_body_head_forward, -10.f, 10.f, 0.25f, "%.2f cm").help("Moves the head forward (back below zero)."),
        slider("Right", vr_body_head_side, -10.f, 10.f, 0.25f, "%.2f cm").help("Moves the head to its right (left below zero)."),
        slider("Up", vr_body_head_up, -10.f, 10.f, 0.25f, "%.2f cm").help("Moves the head up (down below zero), the neck's top with it."),
        header("Orientation"),
        slider("Pitch", vr_body_head_pitch, -45.f, 45.f, 1.f, "%.0f deg").help("Tips the chin down (up below zero), about the bottom of the head."),
        slider("Yaw", vr_body_head_yaw, -45.f, 45.f, 1.f, "%.0f deg").help("Turns the head to its left (right below zero)."),
        slider("Roll", vr_body_head_roll, -45.f, 45.f, 1.f, "%.0f deg").help("Tilts the head to its right (left below zero)."),
        header("Preview"),
        slider("Preview Distance", vr_menu_body_clone_distance, 0.5f, 5.f, 0.1f, "%.1f m")
            .help("How far off the mirror image of your body is drawn while this page (or a page placing what you wear) is open."),
        cycle("Preview Side", vr_menu_body_clone_side, {{0.f, "Away From Main Hand"}, {1.f, "Left"}, {2.f, "Right"}, {3.f, "Ahead"}})
            .help("Where the mirror image stands, beside the menu panel or straight ahead behind it."),
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
        cycle("Build", vr_body_build, {{0.f, "Lean"}, {1.f, "Athletic"}, {2.f, "Brawny"}})
            .help("The shape of the body you see."),
        open("Ranger Head", pageIndex(pageVrsRangerHead))
            .help("The Quake ranger's head on your body (on or off), its size, place and turn, and how far it sinks over "
                  "the neck, with a mirror image of your body beside the menu while the page is open."),
        slider("Skin Tone", vr_skin_tone, -1.f, 1.f, 0.05f, "%.2f")
            .help("Darken or lighten the skin on your hands and arms (and the fingertips out of fingerless gloves). Zero "
                  "keeps your painted skin. Blood, nails, clothes, gloves and gadgets keep their own colours."),
        toggle("Bracers", vr_body_bracers)
            .help("The leather bracers on your forearms. Off: bare forearms with the wrist gadget strapped round the skin, running "
                  "on into the hands (with the jointed hand, vr_hand_rig 1: the default)."),
        header("Gloves"),
        cycle("Gloves", vr_gloves, {{0.f, "None"}, {1.f, "Fingerless"}, {2.f, "Full"}})
            .help("Leather gloves on your hands: fingerless ones leave the fingertips bare (in your skin tone); full ones "
                  "cover every finger. With bare forearms (no bracers) a cuff covers the wrist. On the jointed hands "
                  "(vr_hand_rig 1: the default)."),
        slider("Glove Hue", vr_glove_hue, 0.f, 360.f, 5.f, "%.0f deg")
            .help("The leather's colour round the colour wheel: 0 red, 25 brown, 60 yellow, 120 green, 240 blue, 300 "
                  "purple."),
        slider("Glove Saturation", vr_glove_saturation, 0.f, 1.f, 0.05f, "%.2f")
            .help("How strong the colour is: 0 grey, black or white leather; 1 the purest."),
        slider("Glove Lightness", vr_glove_lightness, 0.f, 1.f, 0.05f, "%.2f")
            .help("0 black, 0.5 the colour at its fullest, 1 white. The hand's own creases and knuckles shade it."),
        slider("Glove Thickness", vr_glove_thickness, 0.f, 4.f, 0.25f, "%.2f mm")
            .help("How thick the leather is: the glove is its own layer over the hand, bending with the fingers, its "
                  "edge a step down to the skin. 0: painted on the skin only (no layer of its own)."),
        toggle("Worn Leather", vr_glove_paint)
            .help("The gloves' leather painted worn: scuffed knuckles, creases over the finger joints, stitched seams, "
                  "scratches (progs/hand_rig_00_00_glove.png: editable, docs/vr-port/GLOVES.md). Off: plain new leather."),
        slider("Leather Relief", vr_glove_relief, 0.f, 2.f, 0.1f, "%.1f")
            .help("How deep the worn leather's relief is in the light: raised stitches in their grooves, the padded "
                  "panels between seams, the creases over the knuckles and joints, the grain. 1 matches the bracers "
                  "and pauldrons; 0 flat."),
        header("Pauldrons"),
        toggle("Pauldrons", vr_body_pauldrons).help("Leather pads over the shoulders and the tops of the arms, as the Quake ranger wears."),
        cycle("Pauldron Style", vr_body_pauldron_style, {{0.f, "Ranger leather"}, {1.f, "Armour colour"}, {2.f, "Steel"}})
            .help("Armour colour: green, yellow or red as the armour you wear (leather without)."),
        slider("Pauldron Size", vr_body_pauldron_size, 0.5f, 1.5f, 0.05f, "%.2fx").extend(0.25f, 4.f)
            .help("Their size (scaled with the build too). Their fit: Advanced VR Options > Body > Arms and Pauldrons."),
        header("Ghost Hands"),
        cycle("Ghost Hands", vr_ghost_hands, {{0.f, "Off"}, {1.f, "Hands"}, {2.f, "Hands and Forearms"}})
            .help("A see-through copy of each hand where your real hand is, whenever the game draws your hand elsewhere: a "
                  "heavy weapon lagging or drooping, a two-handed grip aiming it, a wall stopping it. It fades out as "
                  "the drawn hand comes back to yours."),
        cycle("Ghost Which Hands", vr_ghost_hands_which, {{0.f, "Both"}, {1.f, "Off Hand Only"}})
            .help("Both hands, or only the off hand (the one that steadies a weapon in two hands)."),
        slider("Ghost Opacity", vr_ghost_hands_alpha, 0.05f, 1.f, 0.05f, "%.2f"),
        slider("Ghost Lightness", vr_ghost_hands_lightness, 0.25f, 3.f, 0.05f, "%.2fx")
            .help("Times the light on the ghost: brighter stands out from your drawn hand."),
        slider("Ghost Tint", vr_ghost_hands_tint, 0.f, 1.f, 0.05f, "%.2f").help("0: the hand's own colours; 1: a cold blue."),
        slider("Ghost Hidden Within", vr_ghost_hands_fade_from, 0.f, 20.f, 0.5f, "%.1f cm")
            .help("No ghost while the drawn hand is this close to your real one (its grip, and a point 15 cm ahead of "
                  "it: a turn counts too)."),
        slider("Ghost Fade-In Range", vr_ghost_hands_fade_range, 0.5f, 30.f, 0.5f, "%.1f cm")
            .help("And it fades in over this much further apart."),
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
        cycle("Handedness", vr_handedness, {{0.f, "Custom"}, {1.f, "Right-handed"}, {2.f, "Left-handed"}})
            .help("Sets which stick moves you, the wrist gadget's arm and the flashlight's side together. Left-handed: the right "
                  "stick moves, the gadget on the right arm, the torch on the right (Custom: set apart, below)."),
        toggle("Swap Stick Functions", vr_stick_swap).help("Off: the left stick moves you and the right one turns. On: the right stick moves, the left turns."),
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
        toggle("Holster Models", vr_leg_holster_model_enabled)
            .help("Draws the holsters on your body, with a gun in each one that holds one."),
        toggle("Hip Holster Weapons", vr_holster_weapons_draw_hip)
            .help("Draws the guns in the two holsters on your hips. Off: not drawn; you still take them and put them "
                  "away there."),
        toggle("Chest Holster Weapons", vr_holster_weapons_draw_upper)
            .help("Draws the guns in the two holsters on your chest. Off: not drawn; you still take them and put them "
                  "away there."),
        toggle("Shoulder Holster Weapons", vr_holster_weapons_draw_shoulder)
            .help("Draws the guns in the two holsters on your back, reached over your shoulders (the highest pair; off "
                  "by default: they can get in the way of your view). Off: not drawn; you still take them and put them "
                  "away there."),

        header("Display"),
        cycle("Status Bar", vr_sbar_mode, {{1.f, "Off Hand"}, {0.f, "Main Hand"}}).help("The hand Quake's status bar is on (HUD: Status bar)."),
        toggle("Desktop Mirror", vr_mirror)
            .help("The headset's view in the desktop window. Which eye, both, smoothed or a spectator camera: Recording "
                  "(Window View)."),
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
        toggle("VR", vr_enabled).help("Off: play on the desktop screen, flat, without the headset."),
        action("Restart VR", restartVr).help("Starts the headset again: if the picture froze or stayed black, or after changing the runtime."),
        cycle("OpenXR Runtime", vr_xr_runtime, {{4.f, "Auto"}, {0.f, "System default"}, {1.f, "Virtual Desktop (VDXR)"}, {2.f, "SteamVR"}})
            .help("Which OpenXR runtime runs the headset; VR restarts. Auto: the one whose app is running (Virtual Desktop: the runtime picked in its Streamer's OpenXR Runtime option; VDXR, which skips SteamVR, unless SteamVR is picked there), else the system's. Keep Virtual Desktop's 'Emulate Index controllers' off."),
        info(xrRuntimeLine),
        command("Create Bug Report", "bugreport")
            .help("Zips this run's and the last run's logs (the console, the OpenXR runtime's start and messages, the "
                  "crash report if any, the settings, the runtimes' own logs) into quakevr\\bugreports\\<date>.zip, "
                  "and opens the folder on the desktop: attach it to your bug report. After a crash, the Start menu's "
                  "'Quake VR Unleashed (Create Bug Report)' makes it without starting the game."),
        cycle("Try Other Runtimes", vr_xr_runtime_fallback, {{0.f, "Off"}, {1.f, "On"}, {2.f, "On, SteamVR too"}}).advanced()
            .help("Auto: when the chosen runtime fails to start (no headset), try the other installed ones before playing flat. An idle SteamVR (not the system's runtime) only with 'SteamVR too': trying it starts SteamVR."),
        slider("SteamVR Wait", vr_xr_steamvr_wait, 0.f, 20.f, 1.f, "%.0f s").advanced()
            .help("How long SteamVR, just started, is given to find the headset (through Virtual Desktop or the Link) before it counts as failed and Auto tries the next runtime."),
        slider("Eye Image Size", vr_xr_eye_scale, 0.5f, 1.f, 0.05f, "%.2f").advanced()
            .help("The headset images' size, times the runtime's recommended (each side), at the next VR start. SteamVR's recommended is 1.5 times the panel's pixels by default (its own Render Resolution): 0.82 matches the panel. Smaller images cost less to draw, to copy (SteamVR copies an OpenGL game's every image) and in video memory. The menus' status box shows the eyes' size, the headset panel's if known (vr_xr_panel WxH sets it) and warns over 1.3 times its pixels."),
        cycle("Runtime Menu Open", vr_xr_unfocused, {{1.f, "Freeze the Game's View"}, {0.f, "Keep Rendering"}}).advanced()
            .help("While the runtime's own menu (SteamVR's dashboard, Virtual Desktop's or Meta's menu) has the controllers: the game's last frame stays in view, not rendered again, which leaves the graphics card to the menu; or the game keeps rendering as usual."),
        cycle("Runtime Menu Pauses", vr_xr_unfocused_pause, {{1.f, "Pause the Game"}, {0.f, "Keep Running"}}).advanced()
            .help("While the runtime's own menu has the controllers, a single player game pauses as the game's own menu pauses it (the monsters wait, the game's time stands still) and resumes as you come back; or it keeps running. Multiplayer always runs on."),
        slider("Runtime Menu Volume", vr_xr_unfocused_volume, 0.f, 1.f, 0.1f, "%.1f").advanced()
            .help("The game's sounds while the runtime's menu pauses it: 0 silent, 1 unchanged. The music pauses."),
        cycle("Exit From Runtime Menu", vr_xr_exit_quits, {{1.f, "Quit the Game"}, {0.f, "Play Flat"}}).advanced()
            .help("Exiting the game from the runtime's own menu (SteamVR's dashboard, Virtual Desktop's or Meta's): the game "
                  "quits, your settings saved; or it keeps running flat on the desktop (Restart VR brings the headset back)."),
        toggle("Late Image Acquire", vr_xr_late_acquire).advanced()
            .help("Each eye's headset image is taken from the runtime only once its scene is drawn, so the runtime's wait for it (SteamVR copying the last one out) overlaps the drawing. Off: before the scene, the old way, to compare."),
        toggle("Vendor Controller Profiles", vr_xr_vendor_profiles).advanced()
            .help("Pico, HTC Vive Cosmos and Focus 3, HP Reverb G2 and Samsung Odyssey controllers get their own buttons, "
                  "menu button included, when the runtime offers them. Off: they go as the runtime emulates them (the "
                  "1.0.0 way, if your hands' angle changed). From the next VR start."),
        toggle("OpenXR Timing Log", vr_xr_log_timing).advanced()
            .help("A line a second in quakevr/qvr_openxr.txt while VR runs: frames rendered, frames missed, each OpenXR call's time and the session's state."),
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
        toggle("Hide Lens Corners", vr_visibility_mask)
            .help("Skip the pixels the lenses never show (if the headset gives them): faster, looks the same. Black corners in the desktop mirror."),
        slider("Near Clip", vr_nearclip, 0.02f, 1.f, 0.02f, "%.2f units").extend(0.02f, 4.f).advanced()
            .help("How near the eyes things are still drawn (a unit is 3 cm): lower draws a gun at your face whole. Not below 1 without Float Depth."),
        cycle("Held Items at the Eyes", vr_nearclip_held, {{0.f, "Clipped"}, {1.f, "Never clipped"}, {2.f, "Block the view"}}).advanced()
            .help("A weapon or hand right at your eyes: Never clipped draws its parts nearer than Near Clip too; Block the view also draws its inside when an eye is in it, instead of seeing through it."),
        toggle("Float Depth", vr_depth_float).advanced()
            .help("A 32-bit float depth buffer for the eyes: no flicker far away with a small Near Clip. Applies after Restart VR."),
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
        slider("Throw Speed", vr_weapon_throw_velocity_mult, 0.5f, 3.f, 0.1f, "%.1fx").extend()
            .help("How fast what you throw flies, times your hand's speed."),
        cycle("Throw Gravity", vr_throw_gravity, {{9.81f, "Real"}, {0.f, "Quake"}})
            .help("How thrown things fall: Real, as on Earth; Quake, as Quake's own gravity."),
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
        toggle("Programmatic Recoil", vr_weapon_recoil)
            .help("Each shot kicks the drawn weapon back and tips it up, for the weapons whose Recoil is on (Weapon Offsets > "
                  "Effects: the grunts' burst rifle; the others' models have their own). Looks only: the aim and the shots "
                  "stay where your controller is."),
        slider("Kick Back", vr_recoil_kick, 0.f, 5.f, 0.1f, "%.1f cm").extend(0.f, 20.f)
            .help("How far a shot kicks the weapon back, at a weapon's Recoil Strength 1."),
        slider("Muzzle Rise", vr_recoil_rise, 0.f, 15.f, 0.5f, "%.1f deg").extend(0.f, 45.f)
            .help("How far a shot tips the muzzle up, at a weapon's Recoil Strength 1."),
        header("Shotgun Auto Pump"),
        toggle("Auto Pump", vr_autopump)
            .help("After each shot the shotgun cycles itself: its fore-end is driven back along the guide rods over the "
                  "barrel and springs home, and the spent shell leaves the port as it reaches the back. Off: the fore-end "
                  "stays and the shell leaves as before. Looks only: the fire rate is the same."),
        slider("Auto Pump Time", vr_autopump_time, 0.15f, 0.45f, 0.01f, "%.2f s").extend(0.1f, 0.48f)
            .help("How long the stroke takes, back and home (the shotgun fires again after 0.5 s)."),
        slider("Auto Pump Delay", vr_autopump_delay, 0.f, 0.3f, 0.01f, "%.2f s").extend(0.f, 0.38f)
            .help("How long after the shot the stroke starts: the shot, a moment, then the cycle (0: at once). Cut short so "
                  "that the stroke is home before the shotgun can fire again (0.5 s)."),
        slider("Auto Pump Travel", vr_autopump_travel, 0.5f, 3.2f, 0.1f, "%.1f units").extend(0.f, 3.2f)
            .help("How far back the fore-end goes, in the gun model's units (about 3.8 cm each, as drawn)."),
        slider("Auto Pump Sound", vr_autopump_sound, 0.f, 1.f, 0.1f, "%.1f").help("Volume of its two clacks (0: off)."),
        slider("Auto Pump Haptics", vr_autopump_haptics, 0.f, 2.f, 0.1f, "%.1f")
            .help("Strength of the light ticks in the hand at the back of the stroke and home (0: off)."),
        header("Muzzle Flash"),
        toggle("Programmatic Muzzle Flash", vr_muzzle_flash)
            .help("The shotgun's flash at the muzzle of the weapons whose Muzzle Flash is on (Weapon Offsets > Effects: the "
                  "grunts' burst rifle), following the gun."),
        toggle("Enemies' Muzzle Flashes", vr_muzzle_flash_enemies).help("The grunts' and enforcers' guns flash at their muzzles as they fire."),
        toggle("Enemies' Muzzle Smoke", vr_muzzle_smoke_enemies).help("Puffs of gun smoke out of an enforcer's rifle's and a grunt's gun's muzzle as they fire, as out of your guns'."),
        slider("Enemies' Flash Size", vr_muzzle_flash_enemy_size, 0.2f, 3.f, 0.05f, "%.2fx").extend(0.f, 10.f),
        header("Bullet Tracers"),
        toggle("Bullet Tracers", vr_tracers)
            .help("Hitscan shots (the shotguns, the burst rifle) draw streaks of light flying from the muzzle to what they "
                  "hit. A weapon may differ (Weapon Offsets > Effects: Tracers, and its multipliers)."),
        toggle("Enemies' Tracers", vr_tracers_enemies).help("The grunts' shots too."),
        slider("Tracer Speed", vr_tracer_speed, 20.f, 600.f, 10.f, "%.0f m/s").extend(1.f, 3000.f),
        slider("Tracer Length", vr_tracer_length, 0.1f, 5.f, 0.1f, "%.1f m").extend(0.01f, 50.f),
        slider("Tracer Thickness", vr_tracer_width, 0.2f, 5.f, 0.1f, "%.1f cm").extend(0.05f, 30.f),
        slider("Chance a Pellet", vr_tracer_chance, 0.f, 1.f, 0.05f, "%.2f")
            .help("The chance each pellet (or round) shows a tracer: a shotgun's 6 pellets at 0.3 show about 2."),
        slider("Tracer Red", vr_tracer_r, 0.f, 1.f, 0.05f, "%.2f"),
        slider("Tracer Green", vr_tracer_g, 0.f, 1.f, 0.05f, "%.2f"),
        slider("Tracer Blue", vr_tracer_b, 0.f, 1.f, 0.05f, "%.2f"),
        slider("Tracer Brightness", vr_tracer_brightness, 0.f, 4.f, 0.1f, "%.1fx").extend(0.f, 10.f),
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
        toggle("Lightning Gun in Water", vr_lg_water)
            .help("Fired with your body waist deep, or your hand or the gun's muzzle in water, the lightning gun shocks you and both hands drop what they hold; the shock spreads through the water to what is in it. Fired into water from outside, it electrifies the water round where the beam goes in. Off: Quake's discharge (every cell at once, all round you)."),
        slider("Shock Damage to You", vr_lg_water_self_damage, 0.f, 100.f, 5.f, "%.0f").extend(0.f, 500.f),
        slider("Shock Damage to Others", vr_lg_water_damage, 0.f, 200.f, 5.f, "%.0f").extend(0.f, 1000.f)
            .help("What else is in the water takes this at the gun's muzzle, fading out with distance."),
        slider("Reach", vr_lg_water_radius, 32.f, 1024.f, 16.f, "%.0f units").extend(8.f, 4096.f)
            .help("How far through the water the shock, and the electrified water, reach."),
        slider("Falloff", vr_lg_water_falloff, 0.f, 4.f, 0.25f, "%.2f").extend()
            .help("How the damage fades with distance: 1 evenly to nothing at the reach, 2 and more sooner, 0 not at all."),
        slider("Electrified Water Damage", vr_lg_water_tick_damage, 0.f, 50.f, 1.f, "%.0f").extend(0.f, 300.f)
            .help("Each bolt fired into water from outside (ten a second): damage where it goes in, fading out the same way."),
        slider("Shock Flash", vr_lg_water_flash, 0.f, 1.f, 0.1f, "%.1f")
            .help("The shock's blue flash over the view and the arcs in front of your eyes (the arcs on your arms and body stay)."),
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
        toggle("Incandescent Blades", vr_hotblade)
            .help("Hold a melee weapon's blade in a flame (a torch on its wall or in your other hand, a brazier, the map's "
                  "flames) or dip it in lava: it glows red-hot for a while, hits harder and sets what it strikes on fire. "
                  "The axe (the Shadow Axe too), the knights' swords, the crowbar, the Super Axe and the chainsaw; not "
                  "Mjolnir. The heat is the weapon's: it cools held, holstered or dropped alike."),
        slider("Heating Time", vr_hotblade_heat_time, 0.25f, 10.f, 0.25f, "%.2f s").extend(0.1f, 30.f)
            .help("How long the blade must stay in the flame. Its glow rises meanwhile and it sizzles; taken out early, "
                  "it cools back as fast."),
        slider("Stays Hot", vr_hotblade_time, 5.f, 120.f, 5.f, "%.0f s").extend(1.f, 600.f)
            .help("How long it stays incandescent. A flame again while it glows makes it full again."),
        slider("Fades Over", vr_hotblade_fade, 0.f, 15.f, 0.5f, "%.1f s").extend(0.f, 60.f)
            .help("Its glow is full until these last seconds, then fades out (0: full to the end)."),
        slider("Extra Damage", vr_hotblade_damage, 0.f, 50.f, 1.f, "+%.0f").extend(0.f, 200.f)
            .help("Added to the weapon's base damage on every blow while it glows (then times the blow's strength, as "
                  "its own base: Weapon Damage)."),
        toggle("Sets Enemies on Fire", vr_hotblade_ignite)
            .help("Its blows set living enemies on fire (Combat > Burning: the damage and how long)."),
        toggle("Sets Corpses on Fire", vr_hotblade_ignite_corpses)
            .help("And corpses (with Combat > Burning > Corpses Burn on)."),
        toggle("Water Quenches", vr_hotblade_quench)
            .help("Dipped in water or slime, it cools at once, with a hiss and a puff of steam."),
        header("Look and Sound"),
        slider("Glow Brightness", vr_hotblade_glow, 0.f, 2.f, 0.05f, "%.2f")
            .help("How brightly the heated part of the blade glows (not the handle), held, holstered or lying on the "
                  "ground, and its embers (0: no glow, no light, no embers, no trail)."),
        slider("Glow Hue", vr_hotblade_hue, 0.f, 60.f, 1.f, "%.0f").extend(0.f, 355.f)
            .help("The glow's colour: 0 red, 18 orange-red, 30 orange, 55 yellow."),
        slider("Light", vr_hotblade_light, 0.f, 2.f, 0.05f, "%.2f")
            .help("How bright the light the glowing blade casts round it is; it follows the glow, rising as it heats and "
                  "dying as it fades (0: none)."),
        slider("Light Reach", vr_hotblade_light_radius, 16.f, 200.f, 4.f, "%.0f").extend(8.f, 400.f)
            .help("How far that light reaches, in units, at full glow."),
        slider("Embers per Puff", vr_hotblade_embers, 0.f, 10.f, 1.f, "%.0f").extend(0.f, 32.f)
            .help("How many embers leave the heated part of the blade at a time, from all over it (0: none)."),
        slider("Ember Puffs a Second", vr_hotblade_ember_rate, 1.f, 30.f, 1.f, "%.0f").extend(0.f, 60.f)
            .help("How often they leave it at full glow; fewer as it fades."),
        slider("Swing Trail", vr_hotblade_trail, 0.f, 3.f, 0.1f, "%.1f")
            .help("How much fire and how many sparks a swung (or thrown) glowing blade leaves hanging in the air behind its "
                  "heated part; a faster swing leaves more (0: none)."),
        slider("Trail Lingers", vr_hotblade_trail_linger, 0.1f, 2.f, 0.05f, "%.2f s").extend(0.05f, 5.f)
            .help("How long the trail's fire and sparks hang in the air."),
        slider("Trail From", vr_hotblade_trail_speed, 0.f, 5.f, 0.1f, "%.1f m/s")
            .help("How fast the blade's tip must move to leave a trail (slower: none)."),
        slider("Sounds", vr_hotblade_sound, 0.f, 1.f, 0.1f, "%.1f")
            .help("Volume of the sizzle in the flame, the ring as it turns red-hot and the quench's hiss (0 off)."),
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

int pageIndex(PageBuilder build)
{
    for(int p = 0; p < pageCount; p++)
    {
        if(pages[p].build == build)
        {
            return p;
        }
    }
    return PageMain;
}
