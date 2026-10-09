
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Round.hpp"

// vr_menu_pages.inc -- the old Quake VR settings pages (Quake VR's menu.cpp, "Quake VR -
// Settings"), under VR Settings > Advanced VR Options: every setting that still exists in the
// port, with its old range and help. Included by vr_menu.cpp. Settings that do nothing any more
// (the menu's anchoring, MSAA, the old force grab's range and power, the old floating torso,
// voice chat) are left out. The long ones are split into pages of a screenful or so (Graphics in
// seven; Immersion's screens and force grab moved to the Screens and Force Grab pages).

// Old Quake VR settings: "Menu Settings".
[[nodiscard]] za::Vector<Item> pageMenuSettings()
{
    return {
        slider("Menu Scale", "vr_menu_scale", 0.05f, 0.6f, 0.01f, "%.2f").extend(0.02f, 1.5f).help("Scale multiplier for the menu: the size of one of its pixels, in world units."),
        slider("Menu Distance", "vr_menu_distance", 24.f, 256.f, 1.f, "%.0f").extend(8.f, 600.f).help("Distance of the menu from the anchor point."),
        slider("Menu Resolution", "vr_menu_resolution", 0.f, 3.f, 0.25f, "%.2fx").help("The menus (and the text in front of you in game) are drawn at this many of their pixels to each of the headset's, then shrunk smoothly: sharp letters with even strokes at any distance and scale. 1.5 as shipped; 0: at the desktop window's size, as before (blurry when the headset's pixels are finer than the window's)."),
        slider("Menu Sharpening", "vr_menu_sharpen", 0.f, 1.f, 0.1f, "%.1f").help("With Menu Resolution above 0: the menus shrunk less softly. Higher: crisper letters, the smallest ones shimmering a little as your head moves. 1 as shipped (0.5 before 2026-10-09)."),
        toggle("Status Box", "vr_menu_status").help("A small box in the menus' top right corner: VR, mock VR or flat screen, the runtime (SteamVR, VDXR), the resolution rendered (the runtime's times Eye Image Size times Render Scale; against the headset panel's, with a warning when far over it), the target rate, fps, the CPU's and GPU's milliseconds a frame (and their share of the frame's budget), RAM and VRAM."),
        toggle("Version Label", "vr_menu_version").help("The game's name, version and author, small, in the menus' bottom right corner (where the page leaves room for it). A build not made for a release says \"dev\" after the version."),
        toggle("Check for Updates", "vr_update_check").help("At start-up (at most once an hour) the game asks the release page whether a newer Quake VR is out, and says so above the version label: point at it and press to open its page in your desktop's browser. Only the release list is read; nothing about you or your game is sent. Off: never asked, no notice."),
        toggle("Corner Buttons on a Flat Screen", "vr_menu_flat_shortcuts").help("With VR off: the buttons at the menus' top left (Back to Game, Search, Console, Advanced VR, Levels, Map Library) on the desktop menus too, for the mouse."),
        toggle("Mods on the Main Menu", "vr_menu_main_mods").help("The main menu's Mods row (Ironwail's mods list). Off: only in Options > Mods."),
        slider("Fine Step", "vr_menu_fine_step", 0.01f, 0.5f, 0.01f, "%.2fx").help("A slider's step while you hold a grip (Shift on a flat screen), over its own: 0.1, a tenth, for placing things precisely."),
        slider("Section Gap", "vr_menu_section_gap", 0.f, 2.f, 0.25f, "%.2f rows").help("Space above each section's header on the settings pages (and under the page's title), so the sections stand apart. 0: none."),
        toggle("VR Menu Style", "vr_menu_vr_style").help("The menus fill the panel with their rows spaced out, with modern sliders and switches, and a laser from your hand points at them: trigger to click and drag."),
        open("Laser Hue: Colours", pageIndex(pageColours)).help("Every hue is on HUD and Menus > Colours."),
        toggle("Main Menu Lettering", "vr_menu_bigfont").help("The main menu's rows as text in Quake's own lettering (its letters cut from the menu pictures), VR Calibration among them. Off: Quake's picture of the rows, with VR Calibration above it in the console's letters."),
        toggle("Blood Red Menus", "vr_menu_recolor").help("The menus' browns and tans (the letters, Quake's pictures, sliders, switches and panels) turned blood red, in Quake VR's colours. Off: Quake's own browns."),
        slider("Red Strength", "vr_menu_recolor_strength", 0.f, 1.f, 0.05f, "%.2f").help("With Blood Red Menus: how far the browns turn towards the hue (1 all the way, 0.5 halfway: a rusty red)."),
        hueSlider("Red Hue", vr_menu_recolor_hue).help("With Blood Red Menus: the hue they turn to. 0 red, the banner's (as shipped); 345 crimson, 20 a fiery red-orange. Player's: the Player Effects Hue."),
        slider("Red Saturation", "vr_menu_recolor_saturation", 0.5f, 3.f, 0.1f, "%.1fx").help("With Blood Red Menus: how colourful the red is, over Quake's browns (3 as shipped, the most; as far as the hue's colours go). Their lightness always stays: only the hue changes."),
        slider("Row Spacing", "vr_menu_spacing", 1.f, 2.f, 0.05f, "%.2fx").extend().help("With the VR menu style: how far apart the menus' rows are, over Quake's."),
        slider("Menu Height", "vr_menu_height", 1.f, 2.f, 0.05f, "%.2fx").extend().help("With the VR menu style: how tall the menus are, over Quake's: more rows at once. 1.35 reaches about 35 degrees up and down at the shipped distance, scale and spacing."),
        slider("Menu Background Opacity", "scr_menubgalpha", 0.f, 1.f, 0.05f, "%.2f").help("How dark the panel behind the menus is (0.7 as shipped; the desktop menus' too): lower it to see the game while you tune the graphics. Below about 0.5 the text gets a dark outline, to stay readable."),
        toggle("Live Preview", "ui_live_preview").help("Settings pages show their changes in the game as you make them, and in single player the game keeps running under them (paused while a monster is after you)."),
        toggle("Reopen Where Left", "vr_menu_remember").help("After Back to Game (the button at the menu's top left, or the menu button), the menu opens again on the page you left."),
        slider("Help Reading Speed", "vr_menu_help_wpm", 100.f, 400.f, 10.f, "%.0f wpm").extend(60.f, 600.f).help("The help under a page's rows: one longer than its box shows a part at a time, the bar at its right saying which. Each part stays as long as reading it takes at this many words a minute (200: unhurried)."),
        cycle("Drop-Down Lists", "vr_menu_dropdown", {{0.f, "Never"}, {3.f, "3+ Choices"}, {4.f, "4+ Choices"}, {5.f, "5+ Choices"}, {6.f, "6+ Choices"}, {8.f, "8+ Choices"}}).help("A setting with this many choices or more opens a list of them when clicked (or Enter / A): point at one and click. Left and right still step through them. Never: a click steps to the next."),
    };
}

// Old Quake VR settings: "Crosshair Settings".
[[nodiscard]] za::Vector<Item> pageCrosshairSettings()
{
    return {
        open("Crosshair: VR Settings", pageIndex(pageMain)).help("The crosshair's kind is on VR Settings."),
        slider("Crosshair Depth", "vr_crosshair_depth", 0.f, 4096.f, 16.f, "%.0f").extend().help("When zero, the crosshair will be projected to the closest wall. Otherwise, the crosshair will be projected at a fixed depth."),
        slider("Crosshair Size", "vr_crosshair_size", 0.f, 32.f, 0.5f, "%.1f").extend().help("Size of the crosshair."),
        slider("Crosshair Alpha", "vr_crosshair_alpha", 0.f, 1.f, 0.05f, "%.2f").help("Transparency of the crosshair. '1' means fully visible, '0' means fully transparent."),
        open("Crosshair Hue: Colours", pageIndex(pageColours)).help("Every hue is on HUD and Menus > Colours."),
        slider("Crosshair Z Offset", "vr_crosshairy", -10.f, 10.f, 0.05f, "%.2f").extend(-50.f, 50.f).help("Z offset applied to the crosshair. Does not affect aiming. Leave to zero unless you changed other weapon options and your aim is off."),
    };
}

[[nodiscard]] za::Vector<Item> pageFireParticles()
{
    return {
        toggle("Fire Model Particles", "vr_fire_particles").help("Rising flame tongues from wall torches, map fires, candles, lanterns, burning bodies and taken torches. Keeps the low-poly flame models. Needs Quake VR Particles."),
        slider("Particles per Emission", "vr_fire_particles_count", 0.f, 16.f, 1.f, "%.0f").help("Exact count per source per emission, independent of Particle Multiplier."),
        slider("Emission Frequency", "vr_fire_particles_frequency", 0.f, 30.f, 1.f, "%.0f /s").extend(0.f, 60.f),
        slider("Emission Height", "vr_fire_particles_origin", 0.f, 1.f, 0.05f, "%.2f").help("0: base of the flame; 0.5: centre; 1: tip. Also applies to held torches."),
        slider("Minimum Outward Speed", "vr_fire_particles_speed_min", 0.f, 3.f, 0.05f, "%.2f m/s").extend(0.f, 10.f),
        slider("Maximum Outward Speed", "vr_fire_particles_speed_max", 0.f, 3.f, 0.05f, "%.2f m/s").extend(0.f, 10.f).help("Horizontal speed, away from the flame. Endpoints may be swapped."),
        slider("Minimum Upward Speed", "vr_fire_particles_up_min", 0.f, 3.f, 0.05f, "%.2f m/s").extend(0.f, 10.f),
        slider("Maximum Upward Speed", "vr_fire_particles_up_max", 0.f, 3.f, 0.05f, "%.2f m/s").extend(0.f, 10.f).help("Up in the world, even when a torch is inverted."),
        slider("Minimum Particle Life", "vr_fire_particles_life_min", 0.05f, 2.f, 0.05f, "%.2f s").extend(0.05f, 5.f),
        slider("Maximum Particle Life", "vr_fire_particles_life_max", 0.05f, 2.f, 0.05f, "%.2f s").extend(0.05f, 5.f),
        slider("Fire Particle Size", "vr_fire_particles_size", 0.1f, 4.f, 0.1f, "%.1fx"),
        slider("Fire Particle Opacity", "vr_fire_particles_alpha", 0.f, 1.f, 0.05f, "%.2f"),
        slider("Emission Distance", "vr_fire_particles_range", 0.f, 2048.f, 64.f, "%.0f units").extend(0.f, 4096.f).help("Sources beyond this distance stop emitting; limits the cost on maps with many fires."),
    };
}

// Explosion chunks in multiplayer (vr_explosion_debris_mp_max), its leftmost step -1: as many as in single player.
[[nodiscard]] Item explosionDebrisMultiplayerSlider()
{
    Item i = slider("Most in Multiplayer", "vr_explosion_debris_mp_max", -1.f, 96.f, 1.f, "%.0f")
                 .extend(-1.f, 256.f)
                 .help("The most at once in a multiplayer game (yours as the host). They are the server's: each one in sight "
                       "costs every player's network packets (about 26 bytes a frame while it lives). Single Player's: as "
                       "many as Maximum Active Chunks. 0: none.");
    i.negativeLabel = "Single Player's";
    return i;
}

// A separate tuning page for the physical blast fragments and their trails/glows.
[[nodiscard]] za::Vector<Item> pageExplosionDebris()
{
    return {
        toggle("Physical Explosion Debris", "vr_explosion_debris").help("Small incandescent chunks from every ordinary, coloured or tarbaby explosion: physics objects of the game (the host's) that bounce off the level, doors, lifts, monsters and props, ride lifts, are thrown by later blasts, pass through your hands, and never cause damage."),
        slider("Chunks per Explosion", "vr_explosion_debris_count", 0.f, 32.f, 1.f, "%.0f").extend(0.f, 64.f),
        slider("Minimum Speed", "vr_explosion_debris_speed_min", 0.f, 20.f, 0.25f, "%.2f m/s").extend(0.f, 40.f),
        slider("Maximum Speed", "vr_explosion_debris_speed_max", 0.f, 20.f, 0.25f, "%.2f m/s").extend(0.f, 40.f).help("Speed chosen between Minimum and Maximum; swapped endpoints are accepted."),
        slider("Upward Direction Bias", "vr_explosion_debris_up", 0.f, 2.f, 0.05f, "%.2f").help("0: burst equally in every direction. 1: launches into the upper hemisphere. 2: a narrower upward burst. Speed stays between your chosen limits."),
        slider("Smallest Chunk", "vr_explosion_debris_size_min", 0.8f, 8.f, 0.2f, "%.1f units"),
        slider("Largest Chunk", "vr_explosion_debris_size_max", 0.8f, 8.f, 0.2f, "%.1f units"),
        slider("Minimum Lifetime", "vr_explosion_debris_life_min", 0.1f, 10.f, 0.1f, "%.1f s").extend(0.1f, 30.f),
        slider("Maximum Lifetime", "vr_explosion_debris_life_max", 0.1f, 10.f, 0.1f, "%.1f s").extend(0.1f, 30.f).help("Lifetime chosen independently for each chunk; fades out at the end, even when resting."),
        slider("Bounciness", "vr_explosion_debris_bounce", 0.f, 1.f, 0.05f, "%.2f").help("Share of speed retained away from the surface. 0: no rebound, 1: full rebound. Surface friction also slows sliding."),
        slider("Maximum Active Chunks", "vr_explosion_debris_max", 0.f, 128.f, 1.f, "%.0f").extend(0.f, 256.f).help("Oldest chunks go first when a blast needs room. Reducing this or disabling the effect removes the excess immediately."),
        explosionDebrisMultiplayerSlider(),
        slider("Fire and Smoke Trail", "vr_explosion_debris_trail", 0.f, 3.f, 0.1f, "%.1fx").help("Trail density (0: off). Needs Quake VR Particles. Physical chunks and their lights work with particles off."),
        slider("Maximum Glowing Lights", "vr_explosion_debris_lights", 0.f, 16.f, 1.f, "%.0f").help("Only the nearest chunks emit dynamic light, to keep many blasts inexpensive. Their model skins remain incandescent. These lights do not cast shadows."),
        slider("Glow Light Radius", "vr_explosion_debris_light_radius", 0.f, 120.f, 4.f, "%.0f units").extend(0.f, 200.f),
        command("Preview an Explosion", "vr_explosion_debris_test").help("Spawns an explosion effect ahead without blast damage. Requires a loaded map."),
    };
}

// Old Quake VR settings: "Particle Settings".
[[nodiscard]] za::Vector<Item> pageParticleSettings()
{
    return {
        open("Explosion Debris", pageIndex(pageExplosionDebris)),
        open("Fire Particles", pageIndex(pageFireParticles)),
        slider("Large Fireballs per Explosion", "vr_explosion_particles", 0.f, 16.f, 1.f, "%.0f").help("Exact number of large textured fireballs per explosion, independent of Particle Multiplier. Each gets its own size, rotation, position and velocity; 0 keeps the sparks and smoke."),
        toggle("Particle Effects", "r_particles").help("Enable or disable all particle effects."),
        toggle("Quake VR Particles", "vr_particles")
            .help("Textured smoke, sparks, blood and explosions for Quake VR's effects. Off: Quake's own dots."),
        slider("Particle Multiplier", "vr_particle_mult", 0.25f, 4.f, 0.25f, "%.2f").extend()
            .help("How many particles Quake VR's effects make."),
    };
}

// Old Quake VR settings: "Locomotion Settings" (climbing on its own page; the swimming toggle on Swimming).
[[nodiscard]] za::Vector<Item> pageLocomotionSettings()
{
    return {
        cycle("Comfort", vr_comfort_preset, {{0.f, "Custom"}, {1.f, "Comfortable"}, {2.f, "Moderate"}, {3.f, "Full Freedom"}})
            .help("Sets Turning, Teleport and Default Speed together. Comfortable: snap turns of 45 degrees, teleport, walk. "
                  "Moderate: snap turns of 30, teleport, run. Full Freedom: smooth turning, run, no teleport. Change them after "
                  "to taste (Custom)."),
        cycle("Move Towards", vr_movement_mode, {{1.f, "Head"}, {0.f, "Moving stick's hand"}, {2.f, "Left hand"}, {3.f, "Right hand"}})
            .help("Where the moving stick takes you: where you look (Head), or where a hand points. Moving stick's hand: the "
                  "left one, or the right with Swap Stick Functions."),
        slider("Stick Deadzone", vr_deadzone, 0.f, 50.f, 5.f, "%.0f%%")
            .help("How far a stick must be pushed before it does anything. Raise it if you drift or turn without "
                  "touching the stick: worn sticks often need 20-25%."),
        slider("Movement Speed", "cl_forwardspeed", 100.f, 400.f, 25.f, "%.0f").extend().help("Walking speed (running is this times the running multiplier)."),
        cycle("Default Speed", "cl_alwaysrun", {{1.f, "Run"}, {0.f, "Walk"}}).help("The speed button switches to the other."),
        slider("Running Mult.", "cl_movespeedkey", 1.f, 3.f, 0.05f, "%.2f").extend().help("How much faster running is than walking."),
        toggle("Walk Up Slopes Slowly", vr_slope_walk)
            .help("Standing on a slope you can walk on, gravity only presses you into it, none of it pulls you downhill: "
                  "with the stick pushed only a little you still walk up slopes and stairs with a smooth ramp over them "
                  "from a standstill. Off: Quake's (under about a quarter stick you never got going up one; going down "
                  "you ran a little faster). Flat floors are the same either way (vr_slope_walk)."),
        header("Turning"),
        cycle("Turning", vr_snap_turn, {{0.f, "Smooth"}, {30.f, "Snap 30"}, {45.f, "Snap 45"}, {90.f, "Snap 90"}})
            .help("Smooth: the turning stick turns you steadily (Turn Speed). Snap: each push turns you at once by the angle."),
        slider("Turn Speed", vr_turn_speed, 1.f, 8.f, 0.25f, "%.2f").extend().help("Smooth turning's speed."),
        toggle("Enable Joystick Turn", "vr_enable_joystick_turn").help("Allows turning using the controller. Keep enabled unless you want to force yourself to turn in real life."),
        header("Teleport"),
        toggle("Teleport", vr_teleport_enabled).help("Jump to the spot you point at instead of walking there."),
        slider("Teleport Range", vr_teleport_range, 100.f, 800.f, 50.f, "%.0f").extend(100.f, 3000.f)
            .help("How far a teleport reaches, in Quake's units (one is about 3 cm)."),
        header("Leaning"),
        slider("Lean", "vr_lean_radius", 0.f, 14.f, 1.f, "%.0f units").extend().help("How far your head may lean off the middle of your body's box before the body follows: get your face near walls and over railings. 0: the body always under the head."),
        slider("Lean Recentre", "vr_lean_recenter", 0.f, 2.f, 0.1f, "%.1f m/s").extend().help("How fast the body slides back under your head when it can (not into walls, not over ledges), at least; with Lean Detection, faster the further off it is, and not while you lean. 0: never."),
        slider("Lean Detection", "vr_lean_detect", 0.f, 2.f, 0.1f, "%.1fx").help("Tells a lean from walking in the room. Leaning (your head lower and tilted the way you lean, your hands left by your hips), your feet and hips stay where you stand and your back tilts; walking, the body follows your head. 0: off, the body always slides back under your head; higher: more readily a lean."),
        cycle("Lean on Turns", vr_lean_turn, {{0.f, "Off"}, {1.f, "Round the Body"}, {2.f, "Round the Head"}})
            .help("Turning with the stick while you lean keeps the lean on your body: the game's body stays where your real one stands. Round the Head: smooth turns keep your view where it is and swing the body round under it (where it fits), snap turns swing your head round the body. Round the Body: every turn swings your head round the body. Off: the old way (the body ended up leaning the wrong way, or off to a side, until it slid back)."),
        header("Room Scale"),
        slider("Room Scale", vr_roomscale_move_mult, 0.5f, 2.f, 0.1f, "%.1fx").extend(0.2f, 5.f)
            .help("How far walking in your room moves you in the game (1: as far)."),
        toggle("Roomscale Jump", "vr_roomscale_jump").help("Jumping in the room jumps in the game."),
        slider("Jump Threshold", "vr_roomscale_jump_threshold", 0.3f, 2.f, 0.05f, "%.2f m/s").extend()
            .help("How fast the head must rise above the standing height."),
    };
}

[[nodiscard]] za::Vector<Item> pageStamina();

// Split from Locomotion: climbing ledges and rungs.
[[nodiscard]] za::Vector<Item> pageClimbing()
{
    return {
        toggle("Climbing", "vr_climb").help("Grip a ledge or a rung with either empty hand, or both, to hang from it; pull to climb hand over hand, shimmy along a ledge, pull up onto it. Edges only, not flat walls."),
        slider("Lowest Ledge", "vr_climb_min_height", 0.f, 64.f, 2.f, "%.0f units").extend().help("How far above your feet a ledge must be to grab it."),
        slider("Ledge Fling", "vr_climb_fling", 0.f, 2.f, 0.1f, "%.1f").extend().help("How much letting go of a ledge flings you with your hands' motion."),
        slider("Blasts Knock You Off", "vr_climb_blast_letgo", 0.f, 100.f, 5.f, "%.0f damage").extend(0.f, 300.f).help("Hanging from a ledge or a rung, an explosion this strong or more (its damage to you; a rocket's is up to 120) makes you let go, and throws you as it would on the ground. 0: never. A grunt's or an enforcer's shove that lands always does."),
        slider("Grab Leniency", "vr_climb_leniency", 0.f, 30.f, 1.f, "%.0f cm").extend().help("How far from a ledge or a rung a grip still takes it: the nearest one within this distance of your hand (not through walls, behind you or below your reach). 0: only a hand touching it (Touch Distance)."),
        slider("Mid-Air Leniency", "vr_climb_leniency_air", 0.f, 15.f, 0.5f, "+%.1f cm").extend().help("Extra Grab Leniency while you are in the air, neither on the ground nor holding on: after a jump or while falling, so a ledge you leap at is easier to catch. 0: the same as on the ground."),
        slider("Mid-Air Grab Window", "vr_climb_air_grab_time", 0.f, 1.f, 0.05f, "%.2f s").extend().help("In the air (after a jump, falling), a grip pressed away from any ledge keeps reaching for one this long while you hold it: press at the top of a jump and your hand takes the ledge as it comes down onto it. 0: only at the moment you press."),
        slider("Touch Distance", "vr_climb_touch", 0.f, 15.f, 0.5f, "%.1f cm").extend().help("How close your hand must be to a ledge's top, or to the face under its lip, to count as touching it: the whole reach at Grab Leniency 0 (the leniency adds its distance round this)."),
        slider("Reach Over the Top", "vr_climb_over_top", 0.f, 40.f, 1.f, "%.0f units").extend().help("Over the top of a ledge, how far in from the drop a hand touching the top still holds it (up to how deep the top is). Less: only a hand near the edge grabs."),
        toggle("Hands Slide Along Ledges", "vr_climb_slide").extend().help("When your body can't follow your hands along a ledge (past your arm's reach, against a wall), the hand slides along it and stays on your controller. Off: it stays where it took hold, and drifts away from your controller."),
        slider("Grab Sound", "vr_physsound_grab", 0.f, 1.f, 0.1f, "%.1f").help("A small slap as your hand takes a hold, of what the hold is made of: wood, muted metal or stone (0 off; also Carrying: Physics Sounds)."),
        toggle("Mantle onto Slopes", "vr_climb_mantle_lenient")
            .help("Pulling yourself up onto a ledge whose top isn't level (rising or falling away from the edge, or "
                  "sloping along it) stands you on the slope, up to Steepest Slope. Off: only level tops, as before."),
        slider("Steepest Slope", "vr_climb_mantle_slope", 10.f, 60.f, 5.f, "%.0f deg")
            .help("The steepest top a mantle stands you on with Mantle onto Slopes (Quake walks up to about 45)."),
        slider("Mantle Grunt", "vr_climb_mantle_grunt", 0.f, 1.f, 0.1f, "%.1f").help("A small grunt as you pull yourself up onto a ledge (0 off)."),
        cycle("Mantle Grunt Sound", "vr_climb_mantle_grunt_sound", gruntChoices()).help("Which grunt the mantle makes. Hard landing (the default): the strained \"oof\" of landing from a height, so it doesn't sound like a jump. The deeper ones are Quake's played slower. Hear it with Hear Mantle Grunt."),
        command("Hear Mantle Grunt", "vr_climb_mantle_grunt_test").help("Plays the mantle's grunt as set, at full volume."),
        header("Climbing Stamina"),
        open("Climbing Stamina: Stamina", pageIndex(pageStamina)).help("What hanging from a hold costs is on Combat > Stamina."),
        slider("Exhausted: Slip Time", "vr_climb_stamina_slip", 0.f, 5.f, 0.25f, "%.2f s").extend().help("With no stamina left: 0, your hands let go at once. More: they can't pull you up any more, you sink until your arms are straight, and they slip off after this long."),
        open("Tired Arms (Stamina)", pageIndex(pageStamina)).help("Low on stamina, your arms shake while you hang (looks only) and your hands get heavy: on the Stamina page."),
        header("Hand on a Hold (Looks Only)"),
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
        toggle("Swimming", "vr_swim").help("In water the stick slows and strokes of your hands move you."),
        slider("Stick in Shallow Water", "vr_swim_shallow_speed", 0.f, 1.f, 0.05f, "%.2f").help("The stick's speed with your feet in water."),
        slider("Stick Wading", "vr_swim_wade_speed", 0.f, 1.f, 0.05f, "%.2f").help("Waist deep, standing on the bottom."),
        slider("Stick Swimming", "vr_swim_stick_speed", 0.f, 1.f, 0.05f, "%.2f").help("Under water or off the bottom: swim with your hands."),
        slider("Air Supply", "vr_air_supply", 1.f, 4.f, 0.25f, "%.2fx")
            .help("How long you can stay under water before you start drowning: times Quake's 12 seconds (1.5x: 18 s). Once drowning, the damage comes as ever. The Biosuit's air is unchanged."),
        toggle("Jump Out of Water", "vr_water_jump")
            .help("On (Quake's): swimming against an edge with room above it lifts you out. Off: you climb out with your hands, grabbing the edge (needs Climbing on; with Climbing off, Quake's jump out stays)."),
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
        slider("Stroke Pitch Offset", "vr_swim_stroke_pitch", -30.f, 30.f, 1.f, "%+.0f deg").extend(-90.f, 90.f)
            .help("Tilts every stroke's push up (positive) or down (negative), as the last adjustment: if strokes carry you upward a little too much, a few degrees down. 0: as they are."),
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

// Old Quake VR settings: "Hand/Gun Calibration", with the hand calibration first.
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
        toggle("Show Controller", "vr_show_controller")
            .help("Draws each real controller, translucent, where the game tracks it, with its axes (red along the handle, "
                  "green left, blue up) and its point (white: the middle of the handle); a yellow line to the hand's "
                  "point when the calibration moves it. If the preview itself is off your real controller, line it up "
                  "first (Weapon Offsets > Controller Preview). Not saved."),
        slider("Main Hand X (forward)", "vr_handcal_x", -5.f, 5.f, 0.1f, "%+.1f cm").extend().help(moveHelp),
        slider("Main Hand Y (left)", "vr_handcal_y", -5.f, 5.f, 0.1f, "%+.1f cm").extend().help(moveHelp),
        slider("Main Hand Z (up)", "vr_handcal_z", -5.f, 5.f, 0.1f, "%+.1f cm").extend().help(moveHelp),
        slider("Main Hand Pitch (Gun Angle)", vr_gunangle, -30.f, 90.f, 2.5f, "%.1f").extend(-180.f, 180.f)
            .help("Gun Angle (vr_gunangle): how the main hand and the guns in it are tilted on the controller: set it so "
                  "aiming feels natural. Changes where it aims."),
        slider("Off Hand Pitch (Off Hand Angle)", vr_offhandpitch, -30.f, 90.f, 2.5f, "%.1f").extend(-180.f, 180.f)
            .help("Off Hand Angle (vr_offhandpitch): the same for the off hand."),
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
        list.pushBackMultiple(
            slider("Off Hand X (forward)", "vr_handcal_off_x", -5.f, 5.f, 0.1f, "%+.1f cm").extend().help(moveHelp),
            slider("Off Hand Y (left)", "vr_handcal_off_y", -5.f, 5.f, 0.1f, "%+.1f cm").extend().help(moveHelp),
            slider("Off Hand Z (up)", "vr_handcal_off_z", -5.f, 5.f, 0.1f, "%+.1f cm").extend().help(moveHelp),
            slider("Off Hand Yaw (left)", "vr_offhandyaw", -30.f, 30.f, 0.25f, "%+.2f").extend(-180.f, 180.f)
                .help("Off-Hand Yaw (vr_offhandyaw): turns the whole hand, and what it holds, left about the "
                      "controller's tracked point. Changes where it aims."),
            slider("Off Hand Roll", "vr_handcal_off_roll", -30.f, 30.f, 0.25f, "%+.2f").extend(-180.f, 180.f).help(rollHelp)
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
        slider("Gun Model Pitch", "vr_gunmodelpitch", -90.f, 90.f, 0.25f, "%.1f").extend(-180.f, 180.f).help("Pitch offset for gun models. Does NOT affect aiming."),
        slider("Gun Model Scale", "vr_gunmodelscale", 0.1f, 2.f, 0.05f, "%.2f").extend(0.02f, 5.f).help("Scale multiplier for gun models. Does NOT affect aiming, but does affect muzzle position (where bullets and projectiles come from)."),
        slider("Gun Model Z Offset", "vr_gunmodely", -10.f, 10.f, 0.1f, "%.1f").extend(-50.f, 50.f).help("Z offset for gun models. Does NOT affect aiming."),
        slider("Gun Z Offset", "vr_gun_z_offset", -30.f, 30.f, 0.25f, "%.1f").extend(-150.f, 150.f).help("Z offset for guns/hands. Affects aiming."),
        slider("Finger Grip Bias", "vr_finger_grip_bias", 0.f, 1.f, 0.05f, "%.2f"),
        toggle("Auto close thumb", "vr_finger_auto_close_thumb"),
        slider("Ammo Button Cone", "vr_weapon_button_cone", 10.f, 180.f, 5.f, "%.0f deg")
            .help("The ammo button on a gun is pressed only by the other fingertip coming at it from its front, within "
                  "this many degrees of the way it faces; never from behind (180: from anywhere)."),
        slider("Ammo Button Depth", "vr_weapon_button_depth", 0.f, 3.f, 0.1f, "%.1f units").extend(0.f, 10.f)
            .help("How far behind the button's middle the cone above starts: a fingertip coming in flat, just over the "
                  "button's face, still presses it (0: the cone from its middle).")
    );
    return list;
}

// Split from Hand/Gun Calibration: the jointed hand's fingers, and hands and weapons against models.
[[nodiscard]] za::Vector<Item> pageFingersCollisions()
{
    return {
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
        slider("Thumb Round In-Palm Props", "vr_hand_fit_thumb_outside", 0.f, 3.f, 0.25f, "%.2f cm").extend()
            .help("A rock, a brick or a grenade held In the Palm has the thumb round its outside, never tucked between your "
                  "palm and it: its base may sink into it as far as this to lie along it. 0: as any held thing."),
        toggle("Thumb Opens Wide Round Them", "vr_hand_fit_thumb_wide")
            .help("With Thumb Round In-Palm Props: when the thumb would sink into the thing at its usual turns (a hand "
                  "grenade's side), it opens wider and wraps round its outside instead. Off: it lies along it, sunk in."),
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
    };
}

// Old Quake VR settings: "Player Calibration".
[[nodiscard]] za::Vector<Item> pagePlayerCalibration()
{
    return {
        open("World Scale and Floor Offset: Body and Display", pageIndex(pageBodyDisplay)).help("World Scale and Floor Offset are on Body and Display."),
    };
}

// Old Quake VR settings: "Melee Settings" (its parry settings are on Parry and Bash); and the headbutt.
[[nodiscard]] za::Vector<Item> pageMeleeSettings()
{
    return {
        slider("Swing Speed", "vr_melee_speed", 1.f, 8.f, 0.25f, "%.2f m/s").extend().help("How fast the striking hand must move to hit: a punch, a pommel strike; a weapon's swing a quarter faster, a stab three quarters of it. Any part of the blade then hits. At this speed a hit is weak; twice it, a full hit; faster, harder still."),
        slider("Pommel Strike Wait", "vr_melee_pommel_wait", 0.f, 0.2f, 0.01f, "%.2f s").extend().help("How long the touch of a pommel, a handle's end or a gun's butt waits for the blade or head to reach the same thing, so a cut from close by counts as a cut. Only pommel and butt strikes wait: punches and blades land at once. Lower: pommel strikes land sooner, but close cuts count as pommel strikes more often."),
        slider("Gun Butt Travel", "vr_melee_butt_run", 0.2f, 0.8f, 0.05f, "%.2f m").extend(0.f, 1.5f).help("How far a gun must be carried at something within 0.3 s for its butt to strike: a gun wiggled in place flings its butt fast without going anywhere. 0.2: as any blow."),
        slider("Punch Damage Mult.", "vr_melee_punch_mult", 1.f, 3.f, 0.05f, "%.2f").extend().help("Damage of every punch: straight, hook, uppercut or overhead alike."),
        toggle("Slaps", "vr_melee_slap").help("Swing an open hand across into an enemy to slap it: weaker than a punch, it never pops a head, cuts a limb or draws blood. Pushing an open palm out ahead is still a shove (one hand or both). Off: an open hand never strikes."),
        slider("Slap Damage Mult.", "vr_melee_slap_mult", 0.f, 1.f, 0.05f, "x%.2f").extend(0.f, 3.f).help("A slap's damage, times a punch's."),
        slider("Damage Multiplier", "vr_melee_dmg_multiplier", 0.25f, 15.f, 0.25f, "%.1f").extend().help("Damage multiplier for melee attacks."),
        slider("Range Multiplier", "vr_melee_range_multiplier", 0.25f, 15.f, 0.25f, "%.1f").extend().help("Range multiplier for melee attacks."),
        slider("Quad: Extra Melee Damage", "vr_quad_melee_damage", 1.f, 4.f, 0.05f, "x%.2f").extend().help("With Quad Damage, melee does this much more on top of the x4 every weapon gets. 1: like any weapon (x4 in all); the old Quake VR had 2.75 (x11 in all)."),
        slider("Quad: Extra Melee Reach", "vr_quad_melee_range", 1.f, 1.5f, 0.01f, "x%.2f").extend().help("With Quad Damage, melee reaches this much further. 1: the same reach as without Quad; the old Quake VR had 1.23."),
        cycle("Melee Bloodlust", "vr_melee_bloodlust", {{0.f, "On"}, {1.f, "Off"}}).help("When enabled, restores the player's health when performing melee attacks on enemies."),
        slider("Melee Bloodlust Mult.", "vr_melee_bloodlust_mult", 0.05f, 5.f, 0.05f, "%.2f").extend().help("Multiplier for melee bloodlust health restoration."),
        open("Weapon Damage", pageIndex(pageWeaponDamage))
            .help("Each weapon's base damage: the fist, the axe, the crowbar, Mjolnir, the swords..."),
        header("Swing Through Enemies"),
        toggle("Swing Through Enemies", "vr_melee_phase").help("While your hand swings fast, your fist and the weapon in it pass through enemies instead of being drawn stopped at their bodies (Hands Stop at Models). Walls and things lying about still stop them. The blow is tested where your real hand goes either way: this shows the swing where it is tested. After the swing the hand is eased back out of the enemy. Off: stopped at enemies however fast (as before)."),
        slider("Swing Through Speed", "vr_melee_phase_speed", 1.f, 6.f, 0.25f, "%.2f m/s").extend(0.25f, 10.f).help("How fast the hand must move to pass through. 4 (the default): a hard blow's speed; 2.25: a stab's least speed at Swing Speed 3 (a punch 3, a weapon's swing 3.75). Lower: slower pushes pass through too; higher: only hard blows."),
        slider("Swing Through Follow-Through", "vr_melee_phase_time", 0.f, 0.5f, 0.01f, "%.2f s").extend(0.f, 2.f).help("How long the hand still passes through after it slowed down, so a blow's end isn't stopped inside the enemy."),
        toggle("Swing Through: Guns and Props", "vr_melee_phase_hold").help("Also a gun or a prop held in one hand, swung as a club. Off: only fists and melee weapons (the axe, Mjolnir, the swords, the crowbar, the chainsaw)."),
        header("Headbutt"),
        toggle("Headbutt", vr_headbutt).help("Lunge your head at something to headbutt it."),
        slider("Headbutt Speed", vr_headbutt_speed, 0.4f, 3.f, 0.05f, "%.2f m/s").extend().help("How fast the head must lunge (towards where you look)."),
        slider("Headbutt Damage", vr_headbutt_damage, 5.f, 100.f, 1.f, "%.0f").extend(),
    };
}

// Old Quake VR settings: "Aiming Settings"; and how weight feels (its damage on Weight and Damage).
[[nodiscard]] za::Vector<Item> pageAimingSettings()
{
    return {
        cycle("Two-Handed", vr_2h_mode, {{0.f, "Off"}, {1.f, "Basic"}, {2.f, "Virtual stock"}})
            .help("Hold a gun with both hands to steady it. Virtual stock: a gun brought near your shoulder also aims from it."),
        cycle("Dominant Eye", vr_dominant_eye, {{0.f, "Right"}, {1.f, "Left"}})
            .help("The eye that looks along the sights: pointing at something with both eyes open, the one that stays on "
                  "it when you close the other."),
        toggle("Two-Handed Hand-Off", vr_2h_handoff).help("Letting go with the hand holding a two-handed weapon leaves it in the other hand: a sword changes hands; a gun hangs from its foregrip until a hand takes its handle."),
        slider("Grip Click", vr_2h_grip_sound, 0.f, 1.f, 0.1f, "%.1f")
            .help("Your other hand taking hold of a weapon's foregrip clicks softly from that hand (and pulses in it); "
                  "letting go, a quieter click. How loud (0: no sound, the pulse stays)."),
        slider("2H Aiming Threshold", "vr_2h_angle_threshold", -1.f, 1.f, 0.05f, "%.2f").help("How far out of line the hands may be before two-handed aiming stops. Higher is stricter; -1 never stops."),
        toggle("Grip Must Close On It", "vr_2h_grip_edge").help("The other hand takes a weapon's two-handed grip only by closing its grip there; a fist already closed moved onto it doesn't take hold (so a hand reloading can pass by)."),
        slider("2H Stock Factor", "vr_2h_virtual_stock_factor", 0.f, 1.f, 0.05f, "%.2f").help("How much the virtual stock matters in the final angle calculation for two-handed aiming. Decrease if you want your hands to matter more."),
        header("2H Grip Stickiness"),
        slider("Stickiness", vr_2h_sticky, 0.5f, 4.f, 0.05f, "%.2fx").extend(0.1f, 20.f)
            .help("Once your other hand holds a weapon's grip, how far it may go off it (20 units), how far out of line "
                  "(2H Aiming Threshold) and past the muzzle, before it lets go: times this, always (and times the "
                  "hotspot's own Stickiness: Weapon Offsets)."),
        slider("While Swinging", vr_2h_sticky_fast, 0.f, 5.f, 0.1f, "+%.1fx").extend(0.f, 20.f)
            .help("And this many times more while your hands swing (a two-handed melee blow): back to the above as they "
                  "slow, so a weapon held still lets go as before. 0: off."),
        slider("Swing From", vr_2h_sticky_fast_from, 0.f, 5.f, 0.1f, "%.1f m/s").extend(0.f, 20.f)
            .help("The faster hand's speed it starts growing from."),
        slider("Swing Full At", vr_2h_sticky_fast_full, 0.5f, 10.f, 0.1f, "%.1f m/s").extend(0.1f, 30.f)
            .help("The faster hand's speed at which all of it applies."),
        slider("Swing Fades Over", vr_2h_sticky_fast_hold, 0.f, 2.f, 0.05f, "%.2f s").extend(0.f, 10.f)
            .help("How long it takes to fade once the hands slow (a swing's end and the way back)."),
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
        toggle("Heavier When Tired", vr_weight_stamina)
            .help("As your stamina (Parry Stamina) runs low, what you hold gets heavier."),
        slider("From Stamina", vr_weight_stamina_from, 0.1f, 1.f, 0.05f, "%.2f")
            .help("Above this share of your stamina the weight is unchanged; below it, it grows smoothly to the most at none."),
        slider("Most Weight", vr_weight_stamina_max, 1.f, 4.f, 0.1f, "%.1fx").extend(1.f, 10.f).help("Times the weight with no stamina left."),
        slider("Curve", vr_weight_stamina_curve, 0.5f, 4.f, 0.1f, "%.1f").extend(0.1f, 10.f)
            .help("How it grows from the threshold to none: 1 in a straight line; 2 (the default) gently at first, then faster."),
        slider("Extra Weight", vr_weight_stamina_add, 0.f, 20.f, 0.5f, "%.1f kg").extend(0.f, 50.f)
            .help("Kilograms added to what you hold with no stamina left (on the same curve), on top of Most Weight's times: light things get heavy too. 0: only the times."),
        slider("Empty Hand Weight", vr_weight_stamina_empty, 0.f, 20.f, 0.5f, "%.1f kg").extend(0.f, 50.f)
            .help("Tired, an empty hand follows your controller as if it held this much with no stamina left (on the same curve): it lags and droops a little. Not a hand holding a ledge or steadying a weapon. 0: an empty hand never gets heavy."),
        open("Tired Arms Shake (Stamina)", pageIndex(pageStamina)).help("Low on stamina, your arms shake too (looks only): on the Stamina page."),
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
        slider("Roll Weight", vr_weight_spring_roll, 0.f, 4.f, 0.1f, "%.1fx").extend(0.f, 20.f)
            .help("Times the weight of what you hold about its length, for a roll of your wrist (palm up, palm down): "
                  "higher follows it more slowly (2: 41% slower). A weapon is turned by the forearm's twist, weaker than "
                  "the wrist's bend; Weapon Weights has each weapon's own."),
        slider("Two-Handed Help", vr_weight_spring_2h, 1.f, 8.f, 0.5f, "%.1fx").extend(1.f, 20.f)
            .help("Times the stiffness and strength with both hands on it (4: twice as quick)."),
        slider("Snap Back Beyond", vr_weight_spring_snap, 10.f, 100.f, 5.f, "%.0f cm").extend(1.f, 1000.f)
            .help("Left further behind than this (or three times as many degrees; past 165 degrees always), it is put back in your hand at once."),
    };
}

// Split from Aiming: the weight of what you strike with or throw, and its damage.
[[nodiscard]] za::Vector<Item> pageWeightDamage()
{
    return {
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
            .help("Heavy things can't be swung or thrown fast: their strikes and throws count at lower speeds, times "
                  "(Starts At / mass)^this. 0: off; 0.5: the rocket launcher (8 kg) 0.79 of the speed, the laser cannon "
                  "and the chainsaw (15 kg) 0.58; 1: in proportion. The damage comes from the weight's curve above."),
        slider("Starts At", vr_weight_lenient_from, 1.f, 40.f, 0.5f, "%.1f kg").extend(0.1f, 500.f)
            .help("Things up to this heavy (5 kg: the fists, the axe, the swords, the shotguns, the nailgun) strike and hit "
                  "when thrown at the usual speeds."),
        slider("Least Speed Factor", vr_weight_lenient_min, 0.05f, 1.f, 0.05f, "%.2fx")
            .help("The lowest share of the usual speeds a heavy thing's strikes and throws need."),
    };
}

// Old Quake VR settings: "Immersion Settings" (its weapons' ammo screens and map boards are on the
// Screens page now, its force grab settings on Force Grab, its weapon drops on World).
[[nodiscard]] za::Vector<Item> pageImmersionSettings()
{
    return {
        toggle("Body Picks Up Items", "vr_body_interactions").help("Pick up items and level weapons by walking over them. Thrown and dropped weapons still need your hands."),
        cycle("Haptics", vr_disablehaptics, {{0.f, "On"}, {1.f, "Off"}}).help("The controllers' vibrations, all of them."),
        slider("Vibration Strength", vr_haptics_strength, 0.f, 1.f, 0.1f, "%.1f").extend(0.f, 2.f)
            .help("How strongly the controllers vibrate (0: never)."),
        header("Weapons and Holsters"),
        cycle("Weapon Mode", "vr_holster_mode", {{0.f, "Immersive"}, {1.f, "Quick Slots"}}).help("Immersive: a holster holds one weapon, and taking it out empties it. Quick Slots: holsters never empty (with weapon cycling)."),
        cycle("Weapon Reloading Mode", "vr_reload_mode", {{0.f, "None"}, {1.f, "All Holsters"}, {2.f, "Hip Holsters"}, {3.f, "Immersive"}}).help("Weapons have magazines. All Holsters, Hip Holsters: a gun held at a holster reloads. Immersive: the shotgun is loaded by hand, a shell at a time from the ammo pouch on your belt; the other guns reload at the hip holsters (Weapons > Reloading). Reloading only works with 'immersive' weapon mode."),
        cycle("Holster Haptics", "vr_holster_haptics", {{0.f, "Off"}, {1.f, "Continuous"}, {2.f, "Once"}}).help("Haptic feedback when hovering a usable holster slot."),
        slider("Holster Haptic Time", "vr_holster_haptic_time", 0.05f, 0.6f, 0.05f, "%.2f s").extend().help("How long the buzz lasts as a hand reaches a holster (Holster Haptics: Once)."),
        slider("Draw Blend Time", "vr_weapon_draw_blend", 0.f, 1.f, 0.05f, "%.2f s").help("A gun drawn from a holster eases from its holstered pose into your hand over this time, turning the shortest way (0: at once). It fires at once: only its look eases."),
        slider("Holster Blend Time", "vr_weapon_holster_blend", 0.f, 1.f, 0.05f, "%.2f s").help("A gun put in a holster eases from your hand into its holstered pose over this time (0: at once)."),
        toggle("Weapon Cycle Mode", "vr_weapon_cycle_mode").help("Allows or disallows cycling weapons using controller buttons."),
        cycle("Weapon Throw Mode", "vr_weapon_throw_mode", {{0.f, "Immersive"}, {1.f, "Vanish on Hit"}, {2.f, "Discard"}}).help("Immersive: thrown weapons can be picked back up. Vanish on Hit and Discard avoid piles of weapons with Quick Slots."),
        open("Throw Damage: Weapon Damage", pageIndex(pageWeaponDamage)).help("The damage of thrown weapons is on Combat > Weapon Damage (All Throws)."),
        slider("Throw Speed", vr_weapon_throw_velocity_mult, 0.5f, 3.f, 0.1f, "%.1fx").extend()
            .help("How fast what you throw flies, times your hand's speed."),
        toggle("Dropped Wpn Particles", "vr_weapondrop_particles").help("Enable particle effects for weapons dropped/thrown on the ground."),
        header("Shell Casings"),
        toggle("Shell Casings", "vr_shells").help("The shotgun throws out a spent shell each shot, the super shotgun its two as you reload (a flick reload flings them). They bounce, roll and lie on the floor."),
        slider("Shell Casing Life", "vr_shells_life", 5.f, 120.f, 5.f, "%.0f s").extend().help("How long spent shells lie on the floor before fading out."),
        slider("Shell Casing Sound", "vr_shells_sound", 0.f, 1.f, 0.1f, "%.1f").help("Volume of their tink as they land (0 off)."),
    };
}

[[nodiscard]] za::Vector<Item> pageHeldObjectOffsets();

// Weapons > Reloading > Shotgun: its shells and its loading port.
[[nodiscard]] za::Vector<Item> pageReloadShotgun()
{
    return {
        header("Shells"),
        toggle("Shell Pairs", "vr_reload_shell_pairs")
            .help("The pouch gives two shells taped together: both go into the shotgun at once (one, if only one fits)."),
        open("Shell Pose: Held Object Offsets", pageIndex(pageHeldObjectOffsets))
            .help("How a shell sits in the fingers: take one from the pouch, then tune it there (its Grip and Fingers). "
                  "The taped pair has its own."),
        header("Load Point"),
        toggle("Show Load Points", "vr_reload_show_ports")
            .help("Draws each held gun's load point (yellow) and its radius (green), a held magazine's reference point and "
                  "radius (orange), and an attached magazine's box (blue: where a hand holds it, within Pull Reach, and "
                  "where a hit knocks it out), to tune the offsets live."),
        slider("Port X", "vr_reload_port_shot_x", -10.f, 10.f, 0.1f, "%.1f").extend()
            .help("Moves the shotgun port's load point forward (the gun's model units; negative: the other way)."),
        slider("Port Y", "vr_reload_port_shot_y", -10.f, 10.f, 0.1f, "%.1f").extend()
            .help("Moves the shotgun port's load point left (the gun's model units; negative: the other way)."),
        slider("Port Z", "vr_reload_port_shot_z", -10.f, 10.f, 0.1f, "%.1f").extend()
            .help("Moves the shotgun port's load point up (the gun's model units; negative: the other way)."),
        slider("Port Radius", "vr_reload_port_shot_radius", 0.f, 10.f, 0.25f, "%.2f units").extend(0.f, 40.f)
            .help("How near the shotgun port's load point a held round must come to go in (a magazine: its reference point, within this plus its own radius)."),
    };
}

// Weapons > Reloading > Super Shotgun: broken open (phase 2b) and its breech.
[[nodiscard]] za::Vector<Item> pageReloadSuperShotgun()
{
    return {
        header("Break Open"),
        toggle("Break Open", "vr_reload_ssg_break")
            .help("With Immersive: its fired shells stay in its barrels. Break it open (the ways below) to throw them "
                  "out, spent and live (live ones to take again); it stays open, can't fire, and its chambers take the "
                  "pouch's taped pair at its breech (a single shell when one chamber is loaded). Off: the flick reloads it "
                  "as before."),
        slider("Open Angle", "vr_reload_ssg_open_angle", 0.f, 90.f, 1.f, "%.0f deg")
            .help("How far its barrels drop about the hinge when it is open (looks; its load point goes with them)."),
        slider("Firing Animation Speed", "vr_ssg_fire_anim_speed", 0.5f, 3.f, 0.1f, "%.1fx").extend(0.25f, 5.f)
            .help("How fast its firing animation plays (1: id's, 0.6 s). It breaks open only once the animation is over: "
                  "faster, it opens sooner after a shot."),
        header("Open by"),
        toggle("Open by Flick", "vr_reload_ssg_open_flick").help("A flick of the wrist (the muzzle snapped up) breaks it open."),
        slider("Flick Open Speed", "vr_reload_ssg_flick_open_speed", 200.f, 1500.f, 25.f, "%.0f deg/s")
            .help("How fast the wrist must turn to flick it open (higher: only a hard flick)."),
        toggle("Open by Pry", "vr_reload_ssg_pry")
            .help("Holding it with both hands (the two-handed grip on its fore-end), push the barrels down with the front "
                  "hand against the back hand (or lift the stock): it breaks open."),
        slider("Pry Angle", "vr_reload_ssg_pry_angle", 10.f, 90.f, 1.f, "%.0f deg")
            .help("How far the front hand must go down from the back hand's aim (or the stock up) to pry it open."),
        slider("Pry Speed", "vr_reload_ssg_pry_speed", 20.f, 600.f, 10.f, "%.0f deg/s")
            .help("How quickly: a steady two-handed aim never pries it open."),
        toggle("Open by a Hit from Above", "vr_reload_ssg_open_hit")
            .help("A fist, a held thing or the other gun hitting the front of its barrels from above (the gun's own up, "
                  "as you hold it) forces it open."),
        slider("Hit Open Speed", "vr_reload_ssg_hit_open_speed", 0.5f, 8.f, 0.1f, "%.1f m/s").extend(0.f, 20.f)
            .help("How hard a hit from above must be to force it open."),
        slider("Hit Open Angle", "vr_reload_ssg_hit_open_angle", 10.f, 90.f, 1.f, "%.0f deg")
            .help("How far off straight down (the gun's own down) it may come from."),
        toggle("Open by B/Y", "vr_reload_ssg_open_button").help("The B/Y button of the controller holding it breaks it open."),
        header("Close by"),
        toggle("Close by Flick", "vr_reload_ssg_close_flick").help("Open, a flick snaps it shut."),
        slider("Flick Close Speed", "vr_reload_ssg_flick_close_speed", 200.f, 1500.f, 25.f, "%.0f deg/s")
            .help("How fast the wrist must turn to flick it shut."),
        slider("Flick Rest Speed", "vr_reload_ssg_flick_rest", 60.f, 400.f, 10.f, "%.0f deg/s")
            .help("Under this the wrist holding it counts as still: a flick is measured from how it was then. Higher: a flick "
                  "counts straight after loading even if the gun hand was still moving."),
        toggle("Close by Lifting the Barrels", "vr_reload_ssg_close_pry")
            .help("Open and held with both hands, the front hand lifting the barrels back up snaps it shut."),
        slider("Lift Angle", "vr_reload_ssg_lift_angle", 10.f, 90.f, 1.f, "%.0f deg")
            .help("How far the front hand must come back up (from its lowest) to lift it shut."),
        slider("Lift Speed", "vr_reload_ssg_lift_speed", 20.f, 600.f, 10.f, "%.0f deg/s")
            .help("How quickly it must come up."),
        slider("Lift Hold", "vr_reload_ssg_lift_hold", 0.f, 1.f, 0.05f, "%.2f s")
            .help("How long it must stay lifted that far before it shuts (a jolt doesn't)."),
        toggle("Close by a Hit from Below", "vr_reload_ssg_close_hit")
            .help("Open, a fist, a held thing or the other gun hitting the front of its barrels from below snaps it shut."),
        slider("Hit Close Speed", "vr_reload_ssg_hit_close_speed", 0.5f, 8.f, 0.1f, "%.1f m/s").extend(0.f, 20.f)
            .help("How hard a hit from below must be to snap it shut."),
        slider("Hit Close Angle", "vr_reload_ssg_hit_close_angle", 10.f, 90.f, 1.f, "%.0f deg")
            .help("How far off straight up (the gun's own up) it may come from."),
        toggle("Close When Loaded", "vr_reload_ssg_close_auto")
            .help("Open, it snaps shut by itself once both chambers are loaded."),
        header("Hits (Above and Below)"),
        slider("Hit Reach", "vr_reload_ssg_hit_reach", 1.f, 15.f, 0.5f, "%.1f units")
            .help("How near the front half of its barrels a hit (from above or below) must land."),
        slider("Hit After Loading", "vr_reload_ssg_hit_after_load", 0.f, 1.f, 0.01f, "%.2f s")
            .help("How long after a shell goes in before a hit opens or shuts it (the loading hand drawing back is no hit)."),
        header("Load Point"),
        toggle("Show Load Points", "vr_reload_show_ports")
            .help("Draws each held gun's load point (yellow) and its radius (green), a held magazine's reference point and "
                  "radius (orange), and an attached magazine's box (blue: where a hand holds it, within Pull Reach, and "
                  "where a hit knocks it out), to tune the offsets live."),
        slider("Breech X", "vr_reload_port_sshot_x", -10.f, 10.f, 0.1f, "%.1f").extend()
            .help("Moves the super shotgun's load point (its breech face, between the chambers) forward (the gun's model "
                  "units, as closed; it turns with the barrels open)."),
        slider("Breech Y", "vr_reload_port_sshot_y", -10.f, 10.f, 0.1f, "%.1f").extend()
            .help("Moves the super shotgun's load point left (the gun's model units)."),
        slider("Breech Z", "vr_reload_port_sshot_z", -10.f, 10.f, 0.1f, "%.1f").extend()
            .help("Moves the super shotgun's load point up (the gun's model units)."),
        slider("Breech Radius", "vr_reload_port_sshot_radius", 0.f, 10.f, 0.25f, "%.2f units").extend(0.f, 40.f)
            .help("How near the breech a held pair's middle must come to go in (Show Load Points draws it)."),
    };
}

// Weapons > Reloading > Nailgun: its well, its magazine, holding and pulling it.
[[nodiscard]] za::Vector<Item> pageReloadNailgun()
{
    return {
        header("Load Point"),
        toggle("Show Load Points", "vr_reload_show_ports")
            .help("Draws each held gun's load point (yellow) and its radius (green), a held magazine's reference point and "
                  "radius (orange), and an attached magazine's box (blue: where a hand holds it, within Pull Reach, and "
                  "where a hit knocks it out), to tune the offsets live."),
        slider("Well X", "vr_reload_port_nail_x", -10.f, 10.f, 0.1f, "%.1f").extend()
            .help("Moves the nailgun well's load point forward (the gun's model units; negative: the other way)."),
        slider("Well Y", "vr_reload_port_nail_y", -10.f, 10.f, 0.1f, "%.1f").extend()
            .help("Moves the nailgun well's load point left (the gun's model units; negative: the other way)."),
        slider("Well Z", "vr_reload_port_nail_z", -10.f, 10.f, 0.1f, "%.1f").extend()
            .help("Moves the nailgun well's load point up (the gun's model units; negative: the other way)."),
        slider("Well Radius", "vr_reload_port_nail_radius", 0.f, 10.f, 0.25f, "%.2f units").extend(0.f, 40.f)
            .help("How near the nailgun well's load point a held round must come to go in (a magazine: its reference point, within this plus its own radius)."),
        header("The Magazine's Top"),
        slider("Top X", "vr_reload_mag_nail_x", -10.f, 10.f, 0.1f, "%.1f").extend()
            .help("Moves the nailgun magazine's reference point (its top, its feed end) along its gun (its model units)."),
        slider("Top Y", "vr_reload_mag_nail_y", -10.f, 10.f, 0.1f, "%.1f").extend()
            .help("Moves the nailgun magazine's reference point (its top, its feed end) to its left (its model units)."),
        slider("Top Z", "vr_reload_mag_nail_z", -10.f, 10.f, 0.1f, "%.1f").extend()
            .help("Moves the nailgun magazine's reference point (its top, its feed end) up its length (its model units)."),
        slider("Top Radius", "vr_reload_mag_nail_radius", 0.f, 6.f, 0.25f, "%.2f units").extend(0.f, 20.f)
            .help("The nailgun magazine's own radius round its reference point (added to the well's)."),
        open("Magazine Pose in the Hand: Held Object Offsets", pageIndex(pageHeldObjectOffsets))
            .help("How a magazine out of the gun sits in the hand: take one from the pouch, then tune it there."),
        header("Receiver (looks only)"),
        slider("Receiver X", "vr_reload_well_nail_x", -6.f, 6.f, 0.05f, "%.2f").extend()
            .help("Moves the nailgun receiver (the collar round the magazine's top, drawn on the gun) forward, to sit flush (the gun's model units; looks only)."),
        slider("Receiver Y", "vr_reload_well_nail_y", -6.f, 6.f, 0.05f, "%.2f").extend()
            .help("Moves the nailgun receiver (the collar round the magazine's top, drawn on the gun) left, to sit flush (the gun's model units; looks only)."),
        slider("Receiver Z", "vr_reload_well_nail_z", -6.f, 6.f, 0.05f, "%.2f").extend()
            .help("Moves the nailgun receiver (the collar round the magazine's top, drawn on the gun) up, to sit flush (the gun's model units; looks only)."),
        slider("Receiver Pitch", "vr_reload_well_nail_pitch", -90.f, 90.f, 1.f, "%.0f").extend(-180.f, 180.f)
            .help("Turns the nailgun receiver about its seat (looks only)."),
        slider("Receiver Yaw", "vr_reload_well_nail_yaw", -90.f, 90.f, 1.f, "%.0f").extend(-180.f, 180.f)
            .help("Turns the nailgun receiver about its seat (looks only)."),
        slider("Receiver Roll", "vr_reload_well_nail_roll", -90.f, 90.f, 1.f, "%.0f").extend(-180.f, 180.f)
            .help("Turns the nailgun receiver about its seat (looks only)."),
        header("Holding the Attached Magazine"),
        slider("Hand X", "vr_reload_hold_nail_x", -6.f, 6.f, 0.1f, "%.1f").extend()
            .help("The other hand holding the attached magazine is drawn moved forward (units, the gun's aim; mirrored for the main hand). Looks only."),
        slider("Hand Y", "vr_reload_hold_nail_y", -6.f, 6.f, 0.1f, "%.1f").extend()
            .help("The other hand holding the attached magazine is drawn moved left (units, the gun's aim; mirrored for the main hand). Looks only."),
        slider("Hand Z", "vr_reload_hold_nail_z", -6.f, 6.f, 0.1f, "%.1f").extend()
            .help("The other hand holding the attached magazine is drawn moved up (units, the gun's aim; mirrored for the main hand). Looks only."),
        slider("Hand Pitch", "vr_reload_hold_nail_pitch", -180.f, 180.f, 5.f, "%.0f")
            .help("And turned (degrees), once its grip lies along the magazine where it holds it. Looks only."),
        slider("Hand Yaw", "vr_reload_hold_nail_yaw", -180.f, 180.f, 5.f, "%.0f")
            .help("And turned (degrees), once its grip lies along the magazine where it holds it. Looks only."),
        slider("Hand Roll", "vr_reload_hold_nail_roll", -180.f, 180.f, 5.f, "%.0f")
            .help("And turned (degrees), once its grip lies along the magazine where it holds it. Looks only."),
        toggle("Fingers Set by Hand", "vr_reload_hold_nail_fingers")
            .help("Off: the fingers close round the magazine as it is. On: each finger at its curl below."),
        slider("Thumb Curl", "vr_reload_hold_nail_curl_thumb", 0.f, 1.f, 0.05f, "%.2f")
            .help("0 open, 1 closed (Fingers Set by Hand)."),
        slider("Index Curl", "vr_reload_hold_nail_curl_index", 0.f, 1.f, 0.05f, "%.2f")
            .help("0 open, 1 closed (Fingers Set by Hand)."),
        slider("Middle Curl", "vr_reload_hold_nail_curl_middle", 0.f, 1.f, 0.05f, "%.2f")
            .help("0 open, 1 closed (Fingers Set by Hand)."),
        slider("Ring Curl", "vr_reload_hold_nail_curl_ring", 0.f, 1.f, 0.05f, "%.2f")
            .help("0 open, 1 closed (Fingers Set by Hand)."),
        slider("Little Curl", "vr_reload_hold_nail_curl_pinky", 0.f, 1.f, 0.05f, "%.2f")
            .help("0 open, 1 closed (Fingers Set by Hand)."),
        header("Pull and Knock Out (All Magazines)"),
        slider("Pull Speed", "vr_reload_pull_speed", 0.f, 10.f, 0.1f, "%.1f m/s").extend(0.f, 30.f)
            .help("Grip a gun's magazine with the other hand: it is a two-handed grip, and it stays in. It comes out "
                  "into that hand only pulled out of its well (along it) this fast, or with the wrist snap or the "
                  "hands apart below."),
        slider("Pull Wrist Snap", "vr_reload_pull_snap", 0.f, 2000.f, 25.f, "%.0f deg/s").extend(0.f, 10000.f)
            .help("A wrist turning this fast against the gun's hand while holding the magazine takes it out too."),
        slider("Pull Apart", "vr_reload_pull_apart", 0.f, 40.f, 0.5f, "%.1f units").extend(0.f, 200.f)
            .help("And the hands moved this much further apart while holding it (about 40 cm at 10)."),
        slider("Pull Reach", "vr_reload_pull_reach", 0.f, 12.f, 0.25f, "%.2f units").extend(0.f, 40.f)
            .help("How far off the magazine (its box, as drawn) a hand closing its grip may be and still hold it "
                  "(0: on it). Nearer the gun's own two-handed grip, the hand takes that instead."),
        slider("Knock Out Speed", "vr_reload_bump_speed", 0.f, 15.f, 0.1f, "%.1f m/s").extend(0.f, 50.f)
            .help("A hit on a seated magazine this hard pops it out: a punch, a held prop, another magazine (the bump "
                  "reload: the new one is then seated at the well), or the other gun."),
        slider("Knock Out Reach", "vr_reload_hit_reach", 0.f, 10.f, 0.25f, "%.2f units").extend(0.f, 40.f)
            .help("How near the magazine (anywhere on it) a hit must land."),
    };
}

// Weapons > Reloading > Super Nailgun: its well, its magazine, holding and pulling it.
[[nodiscard]] za::Vector<Item> pageReloadSuperNailgun()
{
    return {
        header("Load Point"),
        toggle("Show Load Points", "vr_reload_show_ports")
            .help("Draws each held gun's load point (yellow) and its radius (green), a held magazine's reference point and "
                  "radius (orange), and an attached magazine's box (blue: where a hand holds it, within Pull Reach, and "
                  "where a hit knocks it out), to tune the offsets live."),
        slider("Well X", "vr_reload_port_snail_x", -10.f, 10.f, 0.1f, "%.1f").extend()
            .help("Moves the super nailgun well's load point forward (the gun's model units; negative: the other way)."),
        slider("Well Y", "vr_reload_port_snail_y", -10.f, 10.f, 0.1f, "%.1f").extend()
            .help("Moves the super nailgun well's load point left (the gun's model units; negative: the other way)."),
        slider("Well Z", "vr_reload_port_snail_z", -10.f, 10.f, 0.1f, "%.1f").extend()
            .help("Moves the super nailgun well's load point up (the gun's model units; negative: the other way)."),
        slider("Well Radius", "vr_reload_port_snail_radius", 0.f, 10.f, 0.25f, "%.2f units").extend(0.f, 40.f)
            .help("How near the super nailgun well's load point a held round must come to go in (a magazine: its reference point, within this plus its own radius)."),
        header("The Magazine's Top"),
        slider("Top X", "vr_reload_mag_snail_x", -10.f, 10.f, 0.1f, "%.1f").extend()
            .help("Moves the super nailgun magazine's reference point (its top, its feed end) along its gun (its model units)."),
        slider("Top Y", "vr_reload_mag_snail_y", -10.f, 10.f, 0.1f, "%.1f").extend()
            .help("Moves the super nailgun magazine's reference point (its top, its feed end) to its left (its model units)."),
        slider("Top Z", "vr_reload_mag_snail_z", -10.f, 10.f, 0.1f, "%.1f").extend()
            .help("Moves the super nailgun magazine's reference point (its top, its feed end) up its length (its model units)."),
        slider("Top Radius", "vr_reload_mag_snail_radius", 0.f, 6.f, 0.25f, "%.2f units").extend(0.f, 20.f)
            .help("The super nailgun magazine's own radius round its reference point (added to the well's)."),
        open("Magazine Pose in the Hand: Held Object Offsets", pageIndex(pageHeldObjectOffsets))
            .help("How a magazine out of the gun sits in the hand: take one from the pouch, then tune it there."),
        header("Receiver (looks only)"),
        slider("Receiver X", "vr_reload_well_snail_x", -6.f, 6.f, 0.05f, "%.2f").extend()
            .help("Moves the super nailgun receiver (the collar round the magazine's top, drawn on the gun) forward, to sit flush (the gun's model units; looks only)."),
        slider("Receiver Y", "vr_reload_well_snail_y", -6.f, 6.f, 0.05f, "%.2f").extend()
            .help("Moves the super nailgun receiver (the collar round the magazine's top, drawn on the gun) left, to sit flush (the gun's model units; looks only)."),
        slider("Receiver Z", "vr_reload_well_snail_z", -6.f, 6.f, 0.05f, "%.2f").extend()
            .help("Moves the super nailgun receiver (the collar round the magazine's top, drawn on the gun) up, to sit flush (the gun's model units; looks only)."),
        slider("Receiver Pitch", "vr_reload_well_snail_pitch", -90.f, 90.f, 1.f, "%.0f").extend(-180.f, 180.f)
            .help("Turns the super nailgun receiver about its seat (looks only)."),
        slider("Receiver Yaw", "vr_reload_well_snail_yaw", -90.f, 90.f, 1.f, "%.0f").extend(-180.f, 180.f)
            .help("Turns the super nailgun receiver about its seat (looks only)."),
        slider("Receiver Roll", "vr_reload_well_snail_roll", -90.f, 90.f, 1.f, "%.0f").extend(-180.f, 180.f)
            .help("Turns the super nailgun receiver about its seat (looks only)."),
        header("Holding the Attached Magazine"),
        slider("Hand X", "vr_reload_hold_snail_x", -6.f, 6.f, 0.1f, "%.1f").extend()
            .help("The other hand holding the attached magazine is drawn moved forward (units, the gun's aim; mirrored for the main hand). Looks only."),
        slider("Hand Y", "vr_reload_hold_snail_y", -6.f, 6.f, 0.1f, "%.1f").extend()
            .help("The other hand holding the attached magazine is drawn moved left (units, the gun's aim; mirrored for the main hand). Looks only."),
        slider("Hand Z", "vr_reload_hold_snail_z", -6.f, 6.f, 0.1f, "%.1f").extend()
            .help("The other hand holding the attached magazine is drawn moved up (units, the gun's aim; mirrored for the main hand). Looks only."),
        slider("Hand Pitch", "vr_reload_hold_snail_pitch", -180.f, 180.f, 5.f, "%.0f")
            .help("And turned (degrees), once its grip lies along the magazine where it holds it. Looks only."),
        slider("Hand Yaw", "vr_reload_hold_snail_yaw", -180.f, 180.f, 5.f, "%.0f")
            .help("And turned (degrees), once its grip lies along the magazine where it holds it. Looks only."),
        slider("Hand Roll", "vr_reload_hold_snail_roll", -180.f, 180.f, 5.f, "%.0f")
            .help("And turned (degrees), once its grip lies along the magazine where it holds it. Looks only."),
        toggle("Fingers Set by Hand", "vr_reload_hold_snail_fingers")
            .help("Off: the fingers close round the magazine as it is. On: each finger at its curl below."),
        slider("Thumb Curl", "vr_reload_hold_snail_curl_thumb", 0.f, 1.f, 0.05f, "%.2f")
            .help("0 open, 1 closed (Fingers Set by Hand)."),
        slider("Index Curl", "vr_reload_hold_snail_curl_index", 0.f, 1.f, 0.05f, "%.2f")
            .help("0 open, 1 closed (Fingers Set by Hand)."),
        slider("Middle Curl", "vr_reload_hold_snail_curl_middle", 0.f, 1.f, 0.05f, "%.2f")
            .help("0 open, 1 closed (Fingers Set by Hand)."),
        slider("Ring Curl", "vr_reload_hold_snail_curl_ring", 0.f, 1.f, 0.05f, "%.2f")
            .help("0 open, 1 closed (Fingers Set by Hand)."),
        slider("Little Curl", "vr_reload_hold_snail_curl_pinky", 0.f, 1.f, 0.05f, "%.2f")
            .help("0 open, 1 closed (Fingers Set by Hand)."),
        header("Pull and Knock Out (All Magazines)"),
        slider("Pull Speed", "vr_reload_pull_speed", 0.f, 10.f, 0.1f, "%.1f m/s").extend(0.f, 30.f)
            .help("Grip a gun's magazine with the other hand: it is a two-handed grip, and it stays in. It comes out "
                  "into that hand only pulled out of its well (along it) this fast, or with the wrist snap or the "
                  "hands apart below."),
        slider("Pull Wrist Snap", "vr_reload_pull_snap", 0.f, 2000.f, 25.f, "%.0f deg/s").extend(0.f, 10000.f)
            .help("A wrist turning this fast against the gun's hand while holding the magazine takes it out too."),
        slider("Pull Apart", "vr_reload_pull_apart", 0.f, 40.f, 0.5f, "%.1f units").extend(0.f, 200.f)
            .help("And the hands moved this much further apart while holding it (about 40 cm at 10)."),
        slider("Pull Reach", "vr_reload_pull_reach", 0.f, 12.f, 0.25f, "%.2f units").extend(0.f, 40.f)
            .help("How far off the magazine (its box, as drawn) a hand closing its grip may be and still hold it "
                  "(0: on it). Nearer the gun's own two-handed grip, the hand takes that instead."),
        slider("Knock Out Speed", "vr_reload_bump_speed", 0.f, 15.f, 0.1f, "%.1f m/s").extend(0.f, 50.f)
            .help("A hit on a seated magazine this hard pops it out: a punch, a held prop, another magazine (the bump "
                  "reload: the new one is then seated at the well), or the other gun."),
        slider("Knock Out Reach", "vr_reload_hit_reach", 0.f, 10.f, 0.25f, "%.2f units").extend(0.f, 40.f)
            .help("How near the magazine (anywhere on it) a hit must land."),
    };
}

// Weapons > Reloading > Thunderbolt: its well, its cell, holding and pulling it.
[[nodiscard]] za::Vector<Item> pageReloadThunderbolt()
{
    return {
        header("Load Point"),
        toggle("Show Load Points", "vr_reload_show_ports")
            .help("Draws each held gun's load point (yellow) and its radius (green), a held magazine's reference point and "
                  "radius (orange), and an attached magazine's box (blue: where a hand holds it, within Pull Reach, and "
                  "where a hit knocks it out), to tune the offsets live."),
        slider("Well X", "vr_reload_port_light_x", -10.f, 10.f, 0.1f, "%.1f").extend()
            .help("Moves the thunderbolt well's load point forward (the gun's model units; negative: the other way)."),
        slider("Well Y", "vr_reload_port_light_y", -10.f, 10.f, 0.1f, "%.1f").extend()
            .help("Moves the thunderbolt well's load point left (the gun's model units; negative: the other way)."),
        slider("Well Z", "vr_reload_port_light_z", -10.f, 10.f, 0.1f, "%.1f").extend()
            .help("Moves the thunderbolt well's load point up (the gun's model units; negative: the other way)."),
        slider("Well Radius", "vr_reload_port_light_radius", 0.f, 10.f, 0.25f, "%.2f units").extend(0.f, 40.f)
            .help("How near the thunderbolt well's load point a held round must come to go in (a magazine: its reference point, within this plus its own radius)."),
        header("The Magazine's Top"),
        slider("Top X", "vr_reload_mag_light_x", -10.f, 10.f, 0.1f, "%.1f").extend()
            .help("Moves the thunderbolt cell's reference point (its top, its feed end) along its gun (its model units)."),
        slider("Top Y", "vr_reload_mag_light_y", -10.f, 10.f, 0.1f, "%.1f").extend()
            .help("Moves the thunderbolt cell's reference point (its top, its feed end) to its left (its model units)."),
        slider("Top Z", "vr_reload_mag_light_z", -10.f, 10.f, 0.1f, "%.1f").extend()
            .help("Moves the thunderbolt cell's reference point (its top, its feed end) up its length (its model units)."),
        slider("Top Radius", "vr_reload_mag_light_radius", 0.f, 6.f, 0.25f, "%.2f units").extend(0.f, 20.f)
            .help("The thunderbolt cell's own radius round its reference point (added to the well's)."),
        header("Sparks and Smoke"),
        slider("Contact Sparks", "vr_reload_battery_sparks", 0.f, 40.f, 1.f, "%.0f").extend(0.f, 200.f)
            .help("Sparks a cell throws as it makes or breaks contact: seated in the gun, or taken out (0: none)."),
        slider("Spent Cell Smoke", "vr_reload_battery_smoke_time", 0.f, 20.f, 0.5f, "%.1f s").extend(0.f, 120.f)
            .help("Seconds a spent cell (taken out empty) smokes from its top, in the hand or lying about; a spent "
                  "magazine of lava nails too (0: never)."),
        slider("Smoke Opacity", "vr_reload_battery_smoke_alpha", 0.f, 1.f, 0.05f, "%.2f")
            .help("How thick each wisp of a spent cell's smoke is as it leaves it."),
        open("Cell Pose in the Hand: Held Object Offsets", pageIndex(pageHeldObjectOffsets))
            .help("How a cell out of the gun sits in the hand: take one from the pouch, then tune it there."),
        header("Receiver (looks only)"),
        slider("Receiver X", "vr_reload_well_light_x", -6.f, 6.f, 0.05f, "%.2f").extend()
            .help("Moves the thunderbolt receiver (the collar round the magazine's top, drawn on the gun) forward, to sit flush (the gun's model units; looks only)."),
        slider("Receiver Y", "vr_reload_well_light_y", -6.f, 6.f, 0.05f, "%.2f").extend()
            .help("Moves the thunderbolt receiver (the collar round the magazine's top, drawn on the gun) left, to sit flush (the gun's model units; looks only)."),
        slider("Receiver Z", "vr_reload_well_light_z", -6.f, 6.f, 0.05f, "%.2f").extend()
            .help("Moves the thunderbolt receiver (the collar round the magazine's top, drawn on the gun) up, to sit flush (the gun's model units; looks only)."),
        slider("Receiver Pitch", "vr_reload_well_light_pitch", -90.f, 90.f, 1.f, "%.0f").extend(-180.f, 180.f)
            .help("Turns the thunderbolt receiver about its seat (looks only)."),
        slider("Receiver Yaw", "vr_reload_well_light_yaw", -90.f, 90.f, 1.f, "%.0f").extend(-180.f, 180.f)
            .help("Turns the thunderbolt receiver about its seat (looks only)."),
        slider("Receiver Roll", "vr_reload_well_light_roll", -90.f, 90.f, 1.f, "%.0f").extend(-180.f, 180.f)
            .help("Turns the thunderbolt receiver about its seat (looks only)."),
        header("Holding the Attached Cell"),
        slider("Hand X", "vr_reload_hold_light_x", -6.f, 6.f, 0.1f, "%.1f").extend()
            .help("The other hand holding the attached cell is drawn moved forward (units, the gun's aim; mirrored for the main hand). Looks only."),
        slider("Hand Y", "vr_reload_hold_light_y", -6.f, 6.f, 0.1f, "%.1f").extend()
            .help("The other hand holding the attached cell is drawn moved left (units, the gun's aim; mirrored for the main hand). Looks only."),
        slider("Hand Z", "vr_reload_hold_light_z", -6.f, 6.f, 0.1f, "%.1f").extend()
            .help("The other hand holding the attached cell is drawn moved up (units, the gun's aim; mirrored for the main hand). Looks only."),
        slider("Hand Pitch", "vr_reload_hold_light_pitch", -180.f, 180.f, 5.f, "%.0f")
            .help("And turned (degrees), once its grip lies along the cell where it holds it. Looks only."),
        slider("Hand Yaw", "vr_reload_hold_light_yaw", -180.f, 180.f, 5.f, "%.0f")
            .help("And turned (degrees), once its grip lies along the cell where it holds it. Looks only."),
        slider("Hand Roll", "vr_reload_hold_light_roll", -180.f, 180.f, 5.f, "%.0f")
            .help("And turned (degrees), once its grip lies along the cell where it holds it. Looks only."),
        toggle("Fingers Set by Hand", "vr_reload_hold_light_fingers")
            .help("Off: the fingers close round the cell as it is. On: each finger at its curl below."),
        slider("Thumb Curl", "vr_reload_hold_light_curl_thumb", 0.f, 1.f, 0.05f, "%.2f")
            .help("0 open, 1 closed (Fingers Set by Hand)."),
        slider("Index Curl", "vr_reload_hold_light_curl_index", 0.f, 1.f, 0.05f, "%.2f")
            .help("0 open, 1 closed (Fingers Set by Hand)."),
        slider("Middle Curl", "vr_reload_hold_light_curl_middle", 0.f, 1.f, 0.05f, "%.2f")
            .help("0 open, 1 closed (Fingers Set by Hand)."),
        slider("Ring Curl", "vr_reload_hold_light_curl_ring", 0.f, 1.f, 0.05f, "%.2f")
            .help("0 open, 1 closed (Fingers Set by Hand)."),
        slider("Little Curl", "vr_reload_hold_light_curl_pinky", 0.f, 1.f, 0.05f, "%.2f")
            .help("0 open, 1 closed (Fingers Set by Hand)."),
        header("Pull and Knock Out (All Magazines)"),
        slider("Pull Speed", "vr_reload_pull_speed", 0.f, 10.f, 0.1f, "%.1f m/s").extend(0.f, 30.f)
            .help("Grip a gun's magazine with the other hand: it is a two-handed grip, and it stays in. It comes out "
                  "into that hand only pulled out of its well (along it) this fast, or with the wrist snap or the "
                  "hands apart below."),
        slider("Pull Wrist Snap", "vr_reload_pull_snap", 0.f, 2000.f, 25.f, "%.0f deg/s").extend(0.f, 10000.f)
            .help("A wrist turning this fast against the gun's hand while holding the magazine takes it out too."),
        slider("Pull Apart", "vr_reload_pull_apart", 0.f, 40.f, 0.5f, "%.1f units").extend(0.f, 200.f)
            .help("And the hands moved this much further apart while holding it (about 40 cm at 10)."),
        slider("Pull Reach", "vr_reload_pull_reach", 0.f, 12.f, 0.25f, "%.2f units").extend(0.f, 40.f)
            .help("How far off the magazine (its box, as drawn) a hand closing its grip may be and still hold it "
                  "(0: on it). Nearer the gun's own two-handed grip, the hand takes that instead."),
        slider("Knock Out Speed", "vr_reload_bump_speed", 0.f, 15.f, 0.1f, "%.1f m/s").extend(0.f, 50.f)
            .help("A hit on a seated magazine this hard pops it out: a punch, a held prop, another magazine (the bump "
                  "reload: the new one is then seated at the well), or the other gun."),
        slider("Knock Out Reach", "vr_reload_hit_reach", 0.f, 10.f, 0.25f, "%.2f units").extend(0.f, 40.f)
            .help("How near the magazine (anywhere on it) a hit must land."),
    };
}

// Weapons > Reloading > Launchers: the grenade, proximity and rocket launchers loaded at the muzzle (QC vr_reload.qc,
// "Front-loaded launchers"), each muzzle's load point.
[[nodiscard]] za::Vector<Item> pageReloadLaunchers()
{
    return {
        toggle("Load at the Muzzle", "vr_reload_front")
            .help("Immersive: the grenade, proximity and rocket launchers take a round at a time from the ammo pouch (a "
                  "grenade, a proximity grenade, a rocket; in their other ammo mode the multi-grenade or multi-rocket), "
                  "put butt first into the muzzle, or thrown in, or the launcher brought to one lying about. Off: they "
                  "reload at the hip holsters."),
        slider("Round Angle", "vr_reload_front_angle", 5.f, 80.f, 5.f, "%.0f deg")
            .help("How far off the barrel a rocket or grenade may lie and still go in, butt first (its nose out of the "
                  "muzzle). The wrong way round never loads: a dull tap. A proximity grenade goes in any way round."),
        slider("Round In Hand Pitch", "vr_reload_front_hold_pitch", -180.f, 180.f, 5.f, "%.0f deg")
            .help("A rocket or grenade (any of them: multi, proximity) taken from the ammo pouch, the same way every time: its "
                  "nose tipped up in your hand (negative: down), about its middle. 90: standing in the fist, nose up by the "
                  "thumb, butt down by the little finger (thumb forward, it lies along the barrel, butt to the muzzle)."),
        slider("Round In Hand Yaw", "vr_reload_front_hold_yaw", -180.f, 180.f, 5.f, "%.0f deg")
            .help("A round taken from the ammo pouch: its nose turned left in your hand (negative: right), mirrored for the "
                  "left hand."),
        slider("Round In Hand Roll", "vr_reload_front_hold_roll", -180.f, 180.f, 5.f, "%.0f deg")
            .help("A round taken from the ammo pouch: rolled right in your hand (negative: left), mirrored for the left hand."),
        open("Round Pose: Held Object Offsets", pageIndex(pageHeldObjectOffsets))
            .help("How a rocket or grenade sits in the hand when taken from where it lies (its Grip). From the ammo pouch, "
                  "every round sits the same way, in the fist's grip channel, turned by Round In Hand above."),
        slider("Grenade Size", "vr_grenade_scale", 0.5f, 1.f, 0.01f, "%.2f").extend(0.25f, 2.f)
            .help("The grenades' size (Quake's grenade, the multi-grenade): the same in the hand, in the pouches, going "
                  "into a launcher and in flight, their hit boxes with them. 1: Quake's size."),
        slider("Proximity Mine Size", "vr_prox_scale", 0.3f, 1.f, 0.01f, "%.2f").extend(0.2f, 2.f)
            .help("The proximity grenade's own size, the same everywhere (in the hand, the pouches, its launcher, in flight "
                  "and stuck), its hit boxes with it. 1: Quake's size; the other grenades' 0.78."),
        slider("Rocket Size in Flight", "vr_rocket_scale", 0.5f, 1.f, 0.01f, "%.2f").extend(0.25f, 2.f)
            .help("The rocket launcher's rocket in flight, nearer the size of the ammo pouch's rocket. 1: Quake's size."),
        toggle("Show Load Points", "vr_reload_show_ports")
            .help("Draws each held gun's load point (yellow) and its radius (green), to tune the offsets live."),
        header("Grenade Launcher"),
        slider("Muzzle X", "vr_reload_port_gl_x", -10.f, 10.f, 0.1f, "%.1f").extend()
            .help("Moves the grenade launcher's load point forward (the gun's model units; negative: back into the barrel). "
                  "The multi-grenade launcher's too."),
        slider("Muzzle Y", "vr_reload_port_gl_y", -10.f, 10.f, 0.1f, "%.1f").extend().help("Left (negative: right)."),
        slider("Muzzle Z", "vr_reload_port_gl_z", -10.f, 10.f, 0.1f, "%.1f").extend().help("Up (negative: down)."),
        slider("Muzzle Radius", "vr_reload_port_gl_radius", 0.f, 10.f, 0.25f, "%.2f units").extend(0.f, 40.f)
            .help("How near the muzzle's load point a grenade's butt must come to go in."),
        header("Proximity Launcher"),
        slider("Muzzle X", "vr_reload_port_prox_x", -10.f, 10.f, 0.1f, "%.1f").extend()
            .help("Moves the proximity launcher's load point forward (the gun's model units; negative: back)."),
        slider("Muzzle Y", "vr_reload_port_prox_y", -10.f, 10.f, 0.1f, "%.1f").extend().help("Left (negative: right)."),
        slider("Muzzle Z", "vr_reload_port_prox_z", -10.f, 10.f, 0.1f, "%.1f").extend().help("Up (negative: down)."),
        slider("Muzzle Radius", "vr_reload_port_prox_radius", 0.f, 10.f, 0.25f, "%.2f units").extend(0.f, 40.f)
            .help("How near the muzzle's load point a proximity grenade's middle must come to go in."),
        header("Rocket Launcher"),
        slider("Muzzle X", "vr_reload_port_rl_x", -10.f, 10.f, 0.1f, "%.1f").extend()
            .help("Moves the rocket launcher's load point forward (the gun's model units; negative: back). The "
                  "multi-rocket launcher's too."),
        slider("Muzzle Y", "vr_reload_port_rl_y", -10.f, 10.f, 0.1f, "%.1f").extend().help("Left (negative: right)."),
        slider("Muzzle Z", "vr_reload_port_rl_z", -10.f, 10.f, 0.1f, "%.1f").extend().help("Up (negative: down)."),
        slider("Muzzle Radius", "vr_reload_port_rl_radius", 0.f, 10.f, 0.25f, "%.2f units").extend(0.f, 40.f)
            .help("How near the muzzle's load point a rocket's butt must come to go in."),
    };
}

// Weapons > Reloading > Cord Points: where the cell cords leave their weapons (vr_cellcord.cpp).
[[nodiscard]] za::Vector<Item> pageReloadCellCords()
{
    return {
        command("Cell Cord Info", "vr_cellcord_info")
            .help("vr_cellcord_info: in the console, each cord's weapon, its ends (the weapon's, the cell's), loose or "
                  "plugged in, its length and rings."),
        header("Laser Cannon"),
        slider("Forward", "vr_cellcord_laser_x", -40.f, 40.f, 0.5f, "%.1f").extend()
            .help("Moves where the cord leaves the laser cannon forward (the gun's model units; negative: back). 0: the "
                  "middle of its lowest points."),
        slider("Left", "vr_cellcord_laser_y", -20.f, 20.f, 0.5f, "%.1f").extend(),
        slider("Up", "vr_cellcord_laser_z", -20.f, 20.f, 0.5f, "%.1f").extend(),
        header("Mjolnir"),
        slider("Forward", "vr_cellcord_hammer_x", -40.f, 40.f, 0.5f, "%.1f").extend()
            .help("Moves where the cord leaves Mjolnir (0: the end of its handle)."),
        slider("Left", "vr_cellcord_hammer_y", -20.f, 20.f, 0.5f, "%.1f").extend(),
        slider("Up", "vr_cellcord_hammer_z", -20.f, 20.f, 0.5f, "%.1f").extend(),
        header("Super Axe"),
        slider("Forward", "vr_cellcord_superaxe_x", -40.f, 40.f, 0.5f, "%.1f").extend()
            .help("Moves where the cord leaves the Super Axe (0: the end of its handle)."),
        slider("Left", "vr_cellcord_superaxe_y", -20.f, 20.f, 0.5f, "%.1f").extend(),
        slider("Up", "vr_cellcord_superaxe_z", -20.f, 20.f, 0.5f, "%.1f").extend(),
    };
}

// Weapons > Reloading (General): the mode, a page per gun, the ammo pouch, what is the same for every gun
// (docs/vr-port/RELOAD_PLAN.md; QC vr_reload.qc).
[[nodiscard]] za::Vector<Item> pageReloading()
{
    return {
        cycle("Reloading", "vr_reload_mode", {{0.f, "Off"}, {1.f, "Holsters (All)"}, {2.f, "Holsters (Hips)"}, {3.f, "Immersive"}})
            .help("Off: the guns fire from your ammo, no magazines. Holsters: a gun held at a holster (any, or the hips') "
                  "reloads. Immersive: guns are loaded by hand from the ammo pouch on the front of your belt (it gives "
                  "what the gun in your other hand takes): the shotgun a shell at a time into the port under it; the "
                  "nailgun, the super nailgun and the thunderbolt a magazine into its well (out by B/Y, a hard pull, or "
                  "bumped out by the next); the grenade, proximity and rocket launchers a round at a time butt first into "
                  "the muzzle; the other guns reload at the hip holsters for now. Needs Weapon Mode Immersive."),
        header("Weapons"),
        open("Shotgun", pageIndex(pageReloadShotgun)).help("Its shells (pairs, their pose) and its loading port."),
        open("Super Shotgun", pageIndex(pageReloadSuperShotgun)).help("Broken open: each way to open and close it, their thresholds; its breech."),
        open("Nailgun", pageIndex(pageReloadNailgun)).help("Its well, its magazine's top, its receiver, the hand holding the magazine, pulling it out."),
        open("Super Nailgun", pageIndex(pageReloadSuperNailgun)).help("Its well, its magazine's top, its receiver, the hand holding the magazine, pulling it out."),
        open("Thunderbolt", pageIndex(pageReloadThunderbolt)).help("Its well, its cell's top, its receiver, the hand holding the cell, pulling it out."),
        open("Launchers", pageIndex(pageReloadLaunchers)).help("The grenade, proximity and rocket launchers loaded at the muzzle: on or off, the round's angle, each muzzle's load point."),
        header("Cell Cords"),
        toggle("Laser Cannon Cord", "vr_cellcord_laser")
            .help("The laser cannon held, a coiled cord (the flashlight's Coiled) from its bottom into a cell in the ammo "
                  "pouch, as if it drew on the pouch's cells (the pouch gives and shows cells while it is held). No cells "
                  "left: the cord comes off and hangs loose off the gun; pick up cells and it plugs back in. A laser "
                  "cannon in each hand: two cords. Needs Reloading Immersive (the pouch)."),
        toggle("Mjolnir Cord", "vr_cellcord_hammer")
            .help("The same cord on Mjolnir (Hipnotic's hammer), from the end of its handle: it draws cells too."),
        toggle("Super Axe Cord", "vr_cellcord_superaxe")
            .help("The same cord on the Super Axe (MG3), from the end of its handle: it draws cells too."),
        open("Cord Points", pageIndex(pageReloadCellCords))
            .help("Where each cord leaves its weapon (off its lowest point), to tune."),
        header("All Guns"),
        slider("Shell Slide In", "vr_reload_insert_time", 0.f, 0.5f, 0.01f, "%.2f s")
            .help("A shell (the super shotgun's pair) let go of at a gun's load point is drawn sliding into it for this "
                  "long, following the gun (0: gone at once). It is loaded at once all the same."),
        toggle("Show Load Points", "vr_reload_show_ports")
            .help("Draws each held gun's load point (yellow) and its radius (green), a held magazine's reference point and "
                  "radius (orange), and an attached magazine's box (blue: where a hand holds it, within Pull Reach, and "
                  "where a hit knocks it out), to tune the offsets live."),
        slider("Collision Leniency", "vr_reload_collide_leniency", 0.f, 60.f, 1.f, "%.0f cm").extend(0.f, 200.f)
            .help("A shell or magazine in your hand is drawn kept off the gun in the other hand (Held Things Collide): it "
                  "may go this far into the gun's box first, to reach the port under the shotgun or a magazine well."),
        toggle("Solid Magazines", "vr_reload_mag_collide")
            .help("A magazine seated in a gun is solid as the gun is: the other hand stops at it, empty or holding "
                  "something, the other gun meets it, and it pushes things about (a gun lying about too). Grabbing it "
                  "still holds it and pulls it out. Off: they pass through it."),
        toggle("Load Loose Rounds", "vr_reload_contact")
            .help("A shell or magazine lying about, dropped or thrown goes into the gun it touches at its load point, lying "
                  "the right way: drop shells into the shotgun's port or the open super shotgun's barrels, a magazine "
                  "feed end up into its well, or bring the gun down onto one standing on a table."),
        slider("Loose Leniency", "vr_reload_contact_leniency", 0.f, 20.f, 1.f, "%.0f cm").extend(0.f, 60.f)
            .help("A loose round goes in this much further from the load point than one in your hand (the gun's "
                  "collision covers its port and well), and passes through the gun within this much more again."),
        slider("Loose Angle", "vr_reload_contact_angle", 5.f, 80.f, 5.f, "%.0f deg")
            .help("How far off the way it goes in a loose round may lie and still load (a shell's open end along the "
                  "tube or the barrels, a magazine's feed end up the well). Sideways never loads."),
        slider("Ejected Stays Out", "vr_reload_eject_cooldown", 0.f, 2.f, 0.05f, "%.2f s")
            .help("A magazine just ejected (or the super shotgun's live shells thrown out) goes back in by contact only "
                  "once it has left both guns' wells and this long has passed."),
        toggle("Eject Button", "vr_reload_eject_button")
            .help("The B/Y button of the controller holding a nailgun, super nailgun or thunderbolt drops its magazine."),
        header("Ammo Pouch"),
        toggle("Show", "vr_ammo_pouch_show").help("Draw the pouch (off: it still works where it is)."),
        slider("X", "vr_ammo_pouch_x", -10.f, 10.f, 0.25f, "%.2f").extend()
            .help("Forward (negative: back, round the hips). With the body drawn, 0 is on the belly."),
        slider("Y", "vr_ammo_pouch_y", -10.f, 10.f, 0.25f, "%.2f").extend().help("To the right (negative: left)."),
        slider("Z", "vr_ammo_pouch_z", -10.f, 10.f, 0.25f, "%.2f").extend().help("Up, from the hip holsters' height."),
        slider("Reach", "vr_ammo_pouch_thresh", 0.5f, 20.f, 0.25f, "%.2f").extend(0.f, 40.f)
            .help("How near a hand must come to reach into it. The hip holsters beside it win where a hand is nearer them."),
        slider("Pitch", "vr_ammo_pouch_pitch", -180.f, 180.f, 5.f, "%.0f")
            .help("Its top tipped away from the body."),
        slider("Yaw", "vr_ammo_pouch_yaw", -180.f, 180.f, 5.f, "%.0f"),
        slider("Roll", "vr_ammo_pouch_roll", -180.f, 180.f, 5.f, "%.0f"),
        slider("Size", "vr_ammo_pouch_scale", 0.2f, 3.f, 0.05f, "%.2fx").extend(0.05f, 10.f)
            .help("As drawn (its reach is Reach's)."),
        toggle("Counter", "vr_ammo_pouch_counter")
            .help("A small screen on the pouch: how many of what it gives (shells, nails, cells) you have left."),
        slider("Counter X", "vr_ammo_pouch_counter_x", -10.f, 10.f, 0.25f, "%.2f").extend().help("Out of the body."),
        slider("Counter Y", "vr_ammo_pouch_counter_y", -10.f, 10.f, 0.25f, "%.2f").extend().help("To the right."),
        slider("Counter Z", "vr_ammo_pouch_counter_z", -10.f, 10.f, 0.25f, "%.2f").extend().help("Up."),
        slider("Counter Pitch", "vr_ammo_pouch_counter_pitch", -180.f, 180.f, 5.f, "%.0f")
            .help("Its turn off facing your eyes as you look down at the pouch."),
        slider("Counter Yaw", "vr_ammo_pouch_counter_yaw", -180.f, 180.f, 5.f, "%.0f"),
        slider("Counter Roll", "vr_ammo_pouch_counter_roll", -180.f, 180.f, 5.f, "%.0f"),
        slider("Counter Size", "vr_ammo_pouch_counter_scale", 0.1f, 5.f, 0.1f, "%.1fx").extend(0.05f, 10.f),
        toggle("White Counter Text", "vr_ammo_screen_text_white")
            .help("The counter's number near-white, as the wrist gadget's (the guns' ammo screens' too: the same setting)."),
        slider("Follow Legs", "vr_ammo_pouch_leg_follow", 0.f, 1.f, 0.05f, "%.2f")
            .help("With the full body, how much the pouch moves with your legs as you walk, as the belt does (the hip "
                  "holsters' Follow Legs likewise). 0: fixed on the belly."),
        header("Feedback"),
        slider("Haptics", "vr_reload_haptics", 0.f, 3.f, 0.1f, "%.1fx")
            .help("Taking a shell, putting it back, loading it, and the empty pouch's dull knock (0: none)."),
        slider("Sounds", "vr_reload_volume", 0.f, 3.f, 0.1f, "%.1fx")
            .help("The pouch's rustle, the port's click (0: none)."),
        header("Dropped Rounds"),
        slider("Most Lying About", "vr_reload_loose_max", 1.f, 128.f, 1.f, "%.0f").extend(1.f, 1024.f)
            .help("Shells dropped or thrown lie about, to be taken again (or force grabbed); past this many the oldest goes."),
        slider("Grab Leniency", "vr_reload_grab_slack", 0.f, 20.f, 0.5f, "%.1f cm").extend(0.f, 50.f)
            .help("A dropped shell is small and lies flat: the fist takes it this far from it."),
        slider("Fade After", "vr_reload_loose_time", 0.f, 600.f, 10.f, "%.0f s").extend(0.f, 3600.f)
            .help("Seconds a dropped shell lies about before it fades away (0: never). A shell gone is a shell lost."),
    };
}

[[nodiscard]] za::Vector<Item> pageGraphicsLights();
[[nodiscard]] za::Vector<Item> pageGraphicsShadows();
[[nodiscard]] za::Vector<Item> pageGraphicsSurfaces();
[[nodiscard]] za::Vector<Item> pageGraphicsLiquids();
[[nodiscard]] za::Vector<Item> pageGraphicsTeleporters();
[[nodiscard]] za::Vector<Item> pageGraphicsPost();
[[nodiscard]] za::Vector<Item> pageGraphicsModels();
[[nodiscard]] za::Vector<Item> pageGraphicsRetro();
[[nodiscard]] za::Vector<Item> pageGraphicsRetroLight();
[[nodiscard]] za::Vector<Item> pageGraphicsRelighting();
[[nodiscard]] za::Vector<Item> pageTransparencyOptions();
[[nodiscard]] za::Vector<Item> pageRecording(); // vr_menu_recording.inc

// Old Quake VR settings: "Graphical Settings", in pages: this one, the preset and the image, and links
// to the others (Memory Log is on Debug).
[[nodiscard]] za::Vector<Item> pageGraphics()
{
    return {
        cycle("Preset", "vr_graphics_preset", {{-1.f, "Custom"}, {0.f, "Off (Quake)"}, {1.f, "Low"}, {2.f, "Medium"}, {3.f, "High"}, {4.f, "Ultra"}})
            .help("Sets the settings on all the Graphics pages: lights, shadows, surfaces, liquids, post-processing, models and effects. Change them after to taste."),
        header("Image"),
        cycle("Anti-aliasing", "vid_fsaa", {{0.f, "Off"}, {2.f, "2x"}, {4.f, "4x"}, {8.f, "8x"}}).help("Multisampling: smoother edges; thin lines such as the gaps between planks stop crawling as your head moves. 4x (as shipped) does most of it; 8x takes nearly twice 4x's GPU time for little more."),
        cycle("Smooth Textures", "vr_texture_smooth", {{0.f, "As gl_texturemode"}, {1.f, "Replacements"}, {2.f, "All"}})
            .help("Smooth filtering for high-resolution replacement textures, or for all textures."),
        cycle("Anisotropic Filtering", "gl_texture_anisotropy", {{1.f, "Off"}, {2.f, "2x"}, {4.f, "4x"}, {8.f, "8x"}, {16.f, "16x"}})
            .help("Keeps floors and walls seen at a glancing angle sharp instead of blurry (smooth-filtered textures). "
                  "Little GPU time; 16x is the sharpest. Capped at what the graphics card supports."),
        toggle("Fence Coverage", "vr_alpha_coverage").help("Fences and grates keep their bars at a distance instead of thinning out and crawling; with Anti-aliasing (MSAA) their edges are smoothed too."),
        toggle("Relit Maps", "vr_relit_maps").help("Maps re-lit with shadows, ambient occlusion and colour, if made (Misc/quakevr/relight_maps.py, or Relighting below). Next map."),
        open("Relighting", pageIndex(pageGraphicsRelighting)).help("Relight the map you are in, with brighter or dimmer lamps, glowing panels, lava, bounced light and ambient occlusion, and see it at once."),
        slider("Headset Gamma", "vr_gamma", 0.5f, 1.5f, 0.02f, "%.2f").extend()
            .help("The headset's brightness: lower is brighter (1 neutral). The desktop's gamma is not used."),
        slider("Headset Contrast", "vr_contrast", 0.5f, 2.f, 0.02f, "%.2f").extend().help("The headset's own contrast (1 neutral)."),
        header("More Graphics"),
        open("Lights", pageIndex(pageGraphicsLights)),
        open("Shadows", pageIndex(pageGraphicsShadows)),
        open("Surfaces", pageIndex(pageGraphicsSurfaces)),
        open("Liquids", pageIndex(pageGraphicsLiquids)),
        open("Teleporters", pageIndex(pageGraphicsTeleporters)),
        open("Post-processing", pageIndex(pageGraphicsPost)),
        open("Models and Effects", pageIndex(pageGraphicsModels)),
        open("Retro Textures", pageIndex(pageGraphicsRetro)),
        open("Retro Lighting", pageIndex(pageGraphicsRetroLight)),
        open("Particles", pageIndex(pageParticleSettings)),
        open("Transparency", pageIndex(pageTransparencyOptions)),
        open("Recording (Window View)", pageIndex(pageRecording)),
        header("Performance"),
        cycle("Performance Profile", "vr_profile", {{0.f, "Off"}, {1.f, "Record"}, {2.f, "Record and show"}})
            .help("Record CPU and GPU time per effect into quakevr/profile/ every 5 seconds (and show the profiler's panel over the wrist: Debug > Profiling has more). Not kept after a restart."),
        cycle("FPS Counter on the Gadget", "vr_gadget_fps", {{0.f, "Off"}, {1.f, "Basic"}, {2.f, "Detailed"}})
            .help("Under the wrist gadget: the frame rate and the CPU's and GPU's milliseconds a frame (our work, the eyes' drawing; not the runtime's waits). Basic: one line, averaged over half a second. Detailed: now, the last quarter second's average, the last 5 seconds' least and most, the frames that missed a refresh (Late), and graphs of the last 3 seconds against the frame budget (the dotted line); figures over the budget are highlighted."),
    };
}

// Graphics: the light, dynamic lights and the things that make them.
[[nodiscard]] za::Vector<Item> pageGraphicsLights()
{
    return {
        slider("Light Contrast", "vr_light_contrast", 1.f, 2.5f, 0.02f, "%.2f").extend().help("Darker shade, lamps as bright: the level's own light sources carry the scene (1: Quake's)."),
        slider("Ambient Light", "vr_ambient_light", 0.f, 0.4f, 0.01f, "%.2f").extend().help("The room's own fill light: how much light the places no lamp reaches have, so a map reads clearly lit without the flashlight. A companion to Light Contrast, which still shapes the shade at any level of it (0: none)."),
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
        slider("Lightning Arcs", "vr_beam_arcs", 0.f, 3.f, 0.1f, "%.1fx").extend().help("Quad Damage's electric arcs crackle along the lightning's beams (the lightning gun, shamblers, Chthon): how many (1: one every 24 units or so; 0 off)."),
        slider("Lightning Arc Spread", "vr_beam_arcs_spread", 2.f, 32.f, 1.f, "%.0f").extend().help("How far the arcs reach from the beam, units."),
        slider("Lightning Arc Width", "vr_beam_arcs_width", 0.5f, 4.f, 0.1f, "%.1fx").extend().help("How thick the arcs are, times Quad Damage's on the arms."),
        slider("Torch Lights", "vr_torch_lights", 0.f, 16.f, 1.f, "%.0f").extend().help("Torches and flames flicker a small warm light onto the room: how many at once, the nearest (0 off)."),
        slider("Torch Light Brightness", "vr_torch_light_scale", 0.f, 16.f, 0.25f, "%.2fx").extend().help("How bright the torches' flickering light is, on top of the map's own light."),
        slider("Torch Light Shadows", "vr_torch_light_shadows", 0.f, 4.f, 1.f, "%.0f").extend().help("How many of the nearest torches cast shadows (each takes one of the dynamic lights' shadows; 0 none)."),
        slider("Ammo Screen Light", "vr_weapon_screen_light", 0.f, 3.f, 0.1f, "%.1fx").extend().help("Your weapons' ammo screens cast a small light in their colour on what is in front of them (0 off)."),
    };
}

// Graphics: shadows, and the blob shadows standing in for them.
[[nodiscard]] za::Vector<Item> pageGraphicsShadows()
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
        cycle("Shadow Statistics", "vr_shadow_stats", {{0.f, "Off"}, {1.f, "Each Second"}, {2.f, "Each Frame, Per Light"}})
            .help("Print what is shadowed and its GPU time, once a second, to the console. Each frame: also every shadowed light and its shadow's strength (+ chosen, - fading out), to check shadows fade rather than pop."),
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
[[nodiscard]] za::Vector<Item> pageGraphicsSurfaces()
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
        slider("Parallax Refinement", "vr_parallax_refine", 0.f, 8.f, 1.f, "%.0f").extend().help("Steps finding exactly where your view meets the carving: smooth edges seen from the side instead of stairs and layers that slide as you move (0 none; 1 the old look; 4 the default)."),
        slider("Parallax Side Fade", "vr_parallax_grazing", 60.f, 90.f, 1.f, "%.0f").extend().help("How far round to the side, in degrees from straight on, walls still look carved before it fades out (90, the default: never fades; 86 before; 83 the old look; lower if seen-from-the-side walls swim)."),
        toggle("Parallax Depth Write", "vr_parallax_depth_write").help("The carving has real depth: monsters, items, hands, particles and water meet the sunken parts where they look to be, instead of at the flat wall. On by default; costs a few tenths of a millisecond a frame."),
        slider("Parallax Items Depth", "vr_parallax_items", 0.f, 4.f, 0.25f, "%.2f").extend().help("How deep the ammo and health boxes look carved, in their own units (scaled with the box; 0 off)."),
        slider("Parallax Models Depth", "vr_parallax_models", 0.f, 2.f, 0.125f, "%.3f").extend().help("How deep monsters, pickups and weapons without baked maps look carved, their heights guessed from the skin's paint, in their own units (0 off, the default: on Quake's blocky skins it bends the texels)."),
        slider("Parallax Depth: Authored Models", "vr_parallax_authored", 0.f, 2.f, 0.25f, "%.2f").extend().help("How deep models with authored bump maps look carved (your hands and body, the gadget, the flashlight, the weapons): their real baked relief, 1 as deep as baked, 0 off."),
        header("External Maps"),
        toggle("External Maps", "vr_extmaps").help("Quetoo's normal, specular and glow maps for Quake's textures (shipped: quakevr/textures_quetoo, CC BY-SA 4.0; or another pack in vr_extmaps_dir), where its pictures match the textures drawn. Next map."),
        toggle("External Bump Maps", "vr_extmaps_normals").help("Its normal maps instead of the bumps made from the textures (where it has them)."),
        toggle("External Specular Maps", "vr_extmaps_spec").help("Its specular maps: how much and in what colour each texel glints (Light Sheen), and each material's hardness."),
        slider("External Specular Brightness", "vr_extmaps_spec_scale", 0.f, 8.f, 0.25f, "%.2f").extend().help("How bright its specular maps make the sheen (4: about as bright as without, on average)."),
        toggle("External Glow Maps", "vr_extmaps_luma").help("Its glow maps on textures that have none of their own."),
        slider("External Maps: Match", "vr_extmaps_match", 0.f, 1.f, 0.05f, "%.2f").extend().help("How closely the pack's own picture must match the texture drawn for its maps to be used (0: always; a repainted picture's bumps don't fit). Next map."),
        header("Detail Textures"),
        toggle("Detail Textures", "vr_detail").help("A fine grain of stone, metal, wood or grime over walls and floors close by, where Quake's textures turn into big blurry texels."),
        slider("Detail Strength", "vr_detail_strength", 0.f, 2.f, 0.1f, "%.1f").extend().help("How strongly the detail textures show."),
        slider("Detail Distance", "vr_detail_distance", 64.f, 384.f, 16.f, "%.0f").extend().help("How far away, in Quake units, the detail textures still show (they fade in closer than this)."),
    };
}

// Graphics: retro textures (vr_retro.cpp): high-resolution textures drawn in Quake's chunky look, each kind of thing
// with its own settings (a page each: pageRetroCategory).
template <retro::Category C>
[[nodiscard]] za::Vector<Item> pageRetroCategory();
[[nodiscard]] za::Vector<Item> pageRetroOverride();
[[nodiscard]] za::Vector<Item> pageRetroAll();

[[nodiscard]] za::Vector<Item> pageGraphicsRetro()
{
    using retro::Category;
    za::Vector<Item> items{
        toggle("Retro Textures", vr_retro)
            .help("High-resolution textures (QRP's, a pack's) drawn in Quake's look: on a coarse grid of blocks, each the average of "
                  "the texels under it, with crisp edges that don't crawl as your head moves, optionally in Quake's 256 colours. "
                  "Each kind of thing has its own settings below. Not a filter over the image: the textures themselves."),
        header("Kinds"),
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

template <retro::Category C>
void retroResetCategory()
{
    Cbuf_AddText(va("vr_retro_reset %s\n", retro::categoryKey(C)));
}

template <retro::Category C>
za::Vector<Item> pageRetroCategory()
{
    return retroCategoryItems(C, retroResetCategory<C>);
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

// Graphics: water, slime and lava.
// Graphics: teleporters that show where they lead, and going through them (vr_portals.cpp).
[[nodiscard]] za::Vector<Item> pageGraphicsTeleporters()
{
    return {
        toggle("Quake VR Teleporters", "vr_teleporters").help("Teleporters show where they lead in VR, flat screen and the spectator camera. Movement, shots and dynamic light go through them. Off: Quake's plain teleporters and their starry surfaces. Takes effect at once, no map reload."),
        slider("Teleporter Views", "vr_portals", 0.f, 1.f, 0.1f, "%.1f").extend().help("Teleporters show where they lead: the place they take you to, live, through their shimmer (the scene is drawn again for the visible gates). 0: Quake's plain shimmer."),
        toggle("Seamless Teleporters", "vr_portals_walk").help("With Teleporter Views on, you walk through teleporters into the place you saw, keeping your speed and the way you face (no flash, no pause), and rockets, grenades, nails, thrown props and gunfire go through them. Off: Quake's teleport."),
        toggle("Monsters Walk Through", "vr_portals_monsters").help("With Seamless Teleporters on, monsters walk through a teleporter that comes out of another gate's face as you do, coming out of that face. Off (and at other gates, or for a monster too big for the opening): Quake's teleport, which puts them at the destination marker, a step out."),
        toggle("Enemies See and Shoot Through", "vr_portals_ai").help("Ranged enemies spot players through an active aperture and aim through it. The gate and both rooms must be clear; closing it cancels delayed shots. One gate per sight line. Melee and navigation stay local."),
        toggle("Exits On Paired Gates", "vr_teleporter_pair_exits").help("A teleporter whose destination stands in front of another gate of its size (a two-way pair) brings you, props and the view out of that gate's face, so you are halfway through both as you cross. Off: out at the destination marker, which can stand a step out from the gate."),
        toggle("Your Head In Gates", "vr_teleporter_self_head").help("Seeing yourself through a teleporter (the loop room's gates show your back), your body has its head. Your own eyes never draw it. Off: headless there too."),
        slider("Visible Gates", "vr_portals_maxviews", 1.f, 8.f, 1.f, "%.0f").help("Maximum destination views drawn per camera (default 4). Each visible gate renders another scene; lower this for VR performance."),
        slider("Gates Within Gates", "vr_portals_recursion", 0.f, 3.f, 1.f, "%.0f").help("A teleporter seen through a teleporter shows where it leads too, this many gates deep (the loop room's gates: you, again and again). Each costs another scene, drawn smaller. 0: gates seen through a gate are left out."),
        slider("Gates In A Gate's View", "vr_portals_recursion_views", 1.f, 8.f, 1.f, "%.0f").help("How many of the gates seen through a gate show where they lead (the nearest and largest). Deeper, only the best one does."),
        slider("Most Gates Within Gates", "vr_portals_recursion_max", 0.f, 16.f, 1.f, "%.0f").help("At most this many gates seen through gates show where they lead, for each eye: the gate you look at most first. Each costs another scene."),
        slider("Deeper Gates' Resolution", "vr_portals_recursion_scale", 1.f, 4.f, 1.f, "1/%.0f").help("Each gate deeper is drawn with this many times fewer pixels a side (2: a quarter of the pixels, then a sixteenth). 1: all at full resolution."),
        header("Teleporter Stars"),
        slider("Size", "vr_teleporter_surface_size", 1.f, 2.f, 0.02f, "%.2fx").help("Enlarges the single star layer on the entrance plane. No extra star quads are drawn inside the destination view. The collision opening keeps its size."),
        slider("Opacity", "vr_teleporter_surface_opacity", 0.f, 1.f, 0.05f, "%.2f").help("How much entrance shimmer covers the view through the teleporter. 0: a clear view; 1: the original shimmer."),
        slider("Fade Up Close", "vr_teleporter_surface_fade", 0.f, 1.f, 0.05f, "%.2f m").help("The shimmer over the destination view fades out as your eyes come this close to the gate, and is gone as you cross, so the room does not brighten in a step as you arrive. 0: it stays to the end."),
    };
}

[[nodiscard]] za::Vector<Item> pageGraphicsLiquids()
{
    return {
        slider("Waves", "vr_water_waves", 0.f, 2.f, 0.1f, "%.1f").extend().help("Waves on water, slime and lava, in the lighting (the surfaces stay flat). 0: Quake's flat liquids."),
        slider("Water Reflection", "vr_water_fresnel", 0.f, 2.f, 0.1f, "%.1f").extend().help("Water is clearer looking straight down and reflects the room at grazing angles."),
        slider("Reflected Room", "vr_water_reflections", 0.f, 1.f, 0.1f, "%.1f").extend().help("Water and slime mirror the room around them (walls, lights, the sky) instead of a plain sheen: a picture of the world made at the water's surface nearest you. How strongly the water reflects is Water Reflection. 0: off."),
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
[[nodiscard]] za::Vector<Item> pageGraphicsPost()
{
    return {
        slider("Bloom", "vr_bloom", 0.f, 1.5f, 0.02f, "%.2f").extend().help("Glow around lamps, glowing panels and buttons, flashes and explosions (0 off)."),
        slider("Bloom Threshold", "vr_bloom_threshold", 0.3f, 1.f, 0.01f, "%.2f").extend().help("How bright a thing must be to glow."),
        slider("Bloom Size", "vr_bloom_radius", 0.25f, 3.f, 0.05f, "%.2f").extend(),
        slider("Bloom: White Lights", "vr_bloom_white", 0.f, 2.f, 0.02f, "%.2fx").extend().help("Glow of white and pale lights (lamps, flashes), times Bloom."),
        slider("Bloom: Coloured Lights", "vr_bloom_color", 0.f, 3.f, 0.05f, "%.2fx").extend().help("Glow of coloured lights (red buttons, blue panels), times Bloom."),
        slider("Bloom: Adapt", "vr_bloom_adapt", 0.f, 10.f, 0.25f, "%.2f").extend().help("How much bloom weakens when much of the view glows (a brightly lit map); 0: never."),
        toggle("Bloom: Fast", "vr_bloom_fast").help("Adds the glow with one texture read a pixel instead of four, its smoothing done on the small glow image: about 0.03 ms a frame cheaper at 2048 x 2048 eyes (some 0.08 ms at your headset's), the halos' edges up to 3% different. Off: as before."),
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
[[nodiscard]] za::Vector<Item> pageGraphicsModels()
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
        open("Screen Glow: Screens", pageIndex(pageScreens)).help("The screens' glow is on HUD and Menus > Screens."),
        slider("Weapon Sight Glow", "vr_weapon_glow", 0.f, 3.f, 0.1f, "%.1fx").extend().help("Your weapons' glowing parts, as the shotgun's sights, shine brighter and glow (0: as Quake)."),
        open("Weapon Sight Hue: Colours", pageIndex(pageColours)).help("Every hue is on HUD and Menus > Colours."),
        header("Effects"),
        toggle("Lit Particles", "vr_particle_light").help("Blood, smoke, dust, debris and water take the light where they are: dark in dark corners, lit by your flashlight, muzzle flashes and explosions. Fire, sparks and glows stay bright. Off: all as bright as their colours, as in Quake."),
        toggle("Soft Particles", "vr_soft_particles").help("Smoke, fire, blood and explosions fade out where they meet walls and floors instead of cutting hard lines through them."),
        slider("Soft Particle Distance", "vr_soft_particles_scale", 0.25f, 3.f, 0.25f, "%.2fx").extend().help("How far in front of walls and floors they start fading (bigger: softer, but thin puffs against a wall fainter)."),
        toggle("Half-Res Heavy Particles", "vr_particle_halfres").help("When smoke, fire and blood fill much of the view, the particles are drawn at half resolution and blended in: a quarter of their cost on the GPU, a little softer."),
        toggle("Trim Particle Margins", "vr_particle_trim").help("Trim empty margins of larger fire and smoke sprites. Off by default: the extra work can cost more than it saves for small particles. Keeps filtering and retro blocks; uncertain cases keep their full shape."),
        toggle("Skip Hidden Particles", "vr_particle_saturate").help("Particles are composited in reverse order into a layer of their own, and where thick smoke or blood is already opaque the particles under it are skipped: the same image, less GPU work in dense effects. Off: each blended into the scene in turn, as before."),
        toggle("Fast Retro Particles", "vr_particle_retro_fast").help("Optimize full-resolution retro particles while keeping palette, dither and edge filtering. Off compares the original full-resolution shader. Retro half resolution always uses the fast shader."),
        toggle("Half-Res Retro Particles", "vr_particle_retro_halfres").help("Optional: render large retro particles at half resolution during heavy effects. Keeps their palette and world-space grid, with softer edges. Small particles stay at full resolution."),
        slider("Retro Half-Res Size", "vr_particle_retro_halfres_pixels", 0.f, 256.f, 8.f, "%.0f px").help("Minimum size from particle centre to edge for half resolution. Zero draws all in their existing order; splitting sizes draws the small particles over the large ones."),
        open("Decals and Gib Blood: Gore", pageIndex(pageGore)).help("Decals and the gibs' blood are on Gore."),
        slider("Ammo Switch Morph", "vr_weapon_morph_time", 0.f, 1.f, 0.05f, "%.2f s").extend().help("Switching a gun's ammo with its button morphs it into its other look (lava, multi-rockets, plasma) over this long, a glowing seam sweeping over it (0: at once)."),
    };
}

// Old Quake VR settings: "Hud Configuration".
[[nodiscard]] za::Vector<Item> pageHudConfiguration()
{
    return {
        open("HUD and Status Bar Hand: VR Settings, Body and Display", pageIndex(pageMain)).help("The HUD's kind is on VR Settings; the status bar's hand on Body and Display."),
        slider("HUD Scale", "vr_hud_scale", 0.01f, 0.1f, 0.005f, "%.3f").extend(0.005f, 0.3f),
        slider("Offset X", "vr_sbar_offset_x", -200.f, 200.f, 1.f, "%.0f").extend(-1000.f, 1000.f),
        slider("Offset Y", "vr_sbar_offset_y", -200.f, 200.f, 1.f, "%.0f").extend(-1000.f, 1000.f),
        slider("Offset Z", "vr_sbar_offset_z", -200.f, 200.f, 1.f, "%.0f").extend(-1000.f, 1000.f),
        slider("Roll", "vr_sbar_offset_roll", -3.14f, 3.14f, 0.05f, "%.2f rad").extend(),
        slider("Pitch", "vr_sbar_offset_pitch", -3.14f, 3.14f, 0.05f, "%.2f rad").extend(),
        slider("Yaw", "vr_sbar_offset_yaw", -3.14f, 3.14f, 0.05f, "%.2f rad").extend(),
    };
}

// Old Quake VR settings: "Hotspot Settings" (the hip holsters on Hip Holsters).
[[nodiscard]] za::Vector<Item> pageHotspotSettings()
{
    return {
        header("Virtual Stock"),
        toggle("Show Virtual Stock", "vr_show_virtual_stock"),
        slider("Shoulder X", "vr_shoulder_offset_x", -50.f, 50.f, 0.5f, "%.1f").extend(-200.f, 200.f),
        slider("Shoulder Y", "vr_shoulder_offset_y", -50.f, 50.f, 0.5f, "%.1f").extend(-200.f, 200.f),
        slider("Shoulder Z", "vr_shoulder_offset_z", -50.f, 50.f, 0.5f, "%.1f").extend(-200.f, 200.f),
        slider("Virtual Stock Thresh.", "vr_virtual_stock_thresh", 0.f, 30.f, 0.1f, "%.1f").extend(0.f, 100.f),
        header("Shoulder Holsters"),
        toggle("Show Shoulder Holst.", "vr_show_shoulder_holsters"),
        slider("Shoulder X", "vr_shoulder_holster_offset_x", -50.f, 50.f, 0.5f, "%.1f").extend(-200.f, 200.f),
        slider("Shoulder Y", "vr_shoulder_holster_offset_y", -50.f, 50.f, 0.5f, "%.1f").extend(-200.f, 200.f),
        slider("Shoulder Z", "vr_shoulder_holster_offset_z", -50.f, 50.f, 0.5f, "%.1f").extend(-200.f, 200.f),
        slider("Shoulder Threshold", "vr_shoulder_holster_thresh", 0.f, 30.f, 0.1f, "%.1f").extend(0.f, 100.f),
        slider("Shoulder Pitch", "vr_shoulder_holster_pitch", -180.f, 180.f, 5.f, "%.0f deg").help("Tips the holster's top away from your body (negative: towards it), the gun in it too."),
        slider("Shoulder Yaw", "vr_shoulder_holster_yaw", -180.f, 180.f, 5.f, "%.0f deg").help("Turns the holster's face outwards (negative: inwards), mirrored for the left one."),
        slider("Shoulder Roll", "vr_shoulder_holster_roll", -180.f, 180.f, 5.f, "%.0f deg").help("Tips the holster's top outwards (negative: inwards; a hanging gun's muzzle goes the other way), mirrored for the left one."),
        header("Upper Holsters"),
        toggle("Show Upper Holsters", "vr_show_upper_holsters"),
        slider("Upper X", "vr_upper_holster_offset_x", -50.f, 50.f, 0.5f, "%.1f").extend(-200.f, 200.f)
            .help("Forward (negative: back), units. With the body drawn, the default (-4.25) is on the chest; further back, the holsters go round your ribs to your back."),
        slider("Upper Y", "vr_upper_holster_offset_y", -50.f, 50.f, 0.5f, "%.1f").extend(-200.f, 200.f),
        slider("Upper Z", "vr_upper_holster_offset_z", -50.f, 50.f, 0.5f, "%.1f").extend(-200.f, 200.f),
        slider("Upper Threshold", "vr_upper_holster_thresh", 0.f, 30.f, 0.1f, "%.1f").extend(0.f, 100.f),
        slider("Upper Pitch", "vr_upper_holster_pitch", -180.f, 180.f, 5.f, "%.0f deg").help("Tips the holster's top away from your body (negative: towards it), the gun in it too."),
        slider("Upper Yaw", "vr_upper_holster_yaw", -180.f, 180.f, 5.f, "%.0f deg").help("Turns the holster's face outwards (negative: inwards), mirrored for the left one."),
        slider("Upper Roll", "vr_upper_holster_roll", -180.f, 180.f, 5.f, "%.0f deg").help("Tips the holster's top outwards (negative: inwards; a hanging gun's muzzle goes the other way), mirrored for the left one."),
    };
}

// Split from Hotspots: the hip holsters and their slots' models.
[[nodiscard]] za::Vector<Item> pageHipHolsters()
{
    return {
        toggle("Show Hip Holsters", "vr_show_hip_holsters"),
        slider("Hip X", "vr_hip_offset_x", -50.f, 50.f, 0.5f, "%.1f").extend(-200.f, 200.f)
            .help("Forward (negative: back), units. With the body drawn, the default (-3.5) is on the belly; further back, the holsters go round your hips to your back."),
        slider("Hip Y", "vr_hip_offset_y", -50.f, 50.f, 0.5f, "%.1f").extend(-200.f, 200.f),
        slider("Hip Z", "vr_hip_offset_z", -50.f, 50.f, 0.5f, "%.1f").extend(-200.f, 200.f),
        slider("Hip Threshold", "vr_hip_holster_thresh", 0.f, 30.f, 0.1f, "%.1f").extend(0.f, 100.f),
        slider("Hip Pitch", "vr_hip_holster_pitch", -180.f, 180.f, 5.f, "%.0f deg").help("Tips the holster's top away from your body (negative: towards it), the gun in it too."),
        slider("Hip Yaw", "vr_hip_holster_yaw", -180.f, 180.f, 5.f, "%.0f deg").help("Turns the holster's face outwards (negative: inwards), mirrored for the left one."),
        slider("Hip Roll", "vr_hip_holster_roll", -180.f, 180.f, 5.f, "%.0f deg").help("Tips the holster's top outwards (negative: inwards; a hanging gun's muzzle goes the other way), mirrored for the left one."),
        header("Holster Slots"),
        open("Holster Models: Body and Display", pageIndex(pageBodyDisplay)).help("Showing the holsters' models is on Body and Display."),
        slider("Holster Slot Scale", "vr_leg_holster_model_scale", 0.1f, 2.f, 0.05f, "%.2f").extend(0.02f, 5.f),
        slider("Holster Slot X", "vr_leg_holster_model_x_offset", -100.f, 100.f, 0.5f, "%.1f").extend(-400.f, 400.f),
        slider("Holster Slot Y", "vr_leg_holster_model_y_offset", -100.f, 100.f, 0.5f, "%.1f").extend(-400.f, 400.f),
        slider("Holster Slot Z", "vr_leg_holster_model_z_offset", -100.f, 100.f, 0.5f, "%.1f").extend(-400.f, 400.f),
        header("Grenade Pouch"),
        toggle("Show Grenade Pouch", "vr_show_grenade_pouch").help("A marker at the pouch the size of its reach, green while a hand is there. Hand grenades, and these sliders too, are on Batting and Catching."),
        slider("Pouch X", "vr_grenade_pouch_x", -50.f, 50.f, 0.5f, "%.1f").extend(-200.f, 200.f)
            .help("Forward (negative: back), units. With the body drawn, the default (-7) is on the belt on your lower back; forward, it goes round your hips."),
        slider("Pouch Y", "vr_grenade_pouch_y", -50.f, 50.f, 0.5f, "%.1f").extend(-200.f, 200.f).help("To your right (negative: left), units."),
        slider("Pouch Z", "vr_grenade_pouch_z", -50.f, 50.f, 0.5f, "%.1f").extend(-200.f, 200.f).help("Up (negative: down), units: 0 is the hip holsters' height."),
        slider("Pouch Threshold", "vr_grenade_pouch_thresh", 0.f, 30.f, 0.1f, "%.1f").extend(0.f, 100.f).help("How near the pouch a hand must be to take a grenade from it (or put one back)."),
        slider("Pouch Pitch", "vr_grenade_pouch_pitch", -180.f, 180.f, 5.f, "%.0f deg").help("Tips the pouch's top away from your back (negative: towards it)."),
        slider("Pouch Yaw", "vr_grenade_pouch_yaw", -180.f, 180.f, 5.f, "%.0f deg").help("Turns the pouch's face to your right (negative: left)."),
        slider("Pouch Roll", "vr_grenade_pouch_roll", -180.f, 180.f, 5.f, "%.0f deg").help("Tips the pouch's top to your right (negative: left)."),
        slider("Grenade In Hand Pitch", "vr_grenade_pouch_hold_pitch", -180.f, 180.f, 5.f, "%.0f deg").help("A grenade taken from the pouch: its front tipped up in your hand (negative: down), about its middle."),
        slider("Grenade In Hand Yaw", "vr_grenade_pouch_hold_yaw", -180.f, 180.f, 5.f, "%.0f deg").help("A grenade taken from the pouch: its front turned left in your hand (negative: right), mirrored for the left hand."),
        slider("Grenade In Hand Roll", "vr_grenade_pouch_hold_roll", -180.f, 180.f, 5.f, "%.0f deg").help("A grenade taken from the pouch: rolled right in your hand (negative: left), mirrored for the left hand."),
    };
}

// Old Quake VR settings: "Transparency Options".
[[nodiscard]] za::Vector<Item> pageTransparencyOptions()
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
        toggle("Use In-game Relights", "vr_relight_use").help("Maps relit here are used over those relight_maps.py made (next map). Off: the relit maps of the script, or the maps' own light."),
        toggle("Reload Where You Are", "vr_relight_reload").help("After relighting the map you are in, it is reloaded at once, where you are (a quick save and load; where the game can't be saved, the map restarts). Off: you see the new light from its next start."),
        header("Many Maps"),
        cycle("Maps", "vr_relight_batch_set", {{0.f, "This Map"}, {1.f, "An Episode"}, {2.f, "A Game"}, {3.f, "Map Library's"}, {4.f, "Every Map"}})
            .help("What Relight These Maps relights, with the settings below: the map you are in, an episode (E1M1 to E1M8...), a whole game (Quake, a mission pack, a campaign), every map installed from the Map Library, or every map there is."),
        cycle("Episode", "vr_relight_batch_episode", {{0.f, "The One You're In"}, {1.f, "E1"}, {2.f, "E2"}, {3.f, "E3"}, {4.f, "E4"}, {5.f, "E5 (Past)"}})
            .help("Maps: An Episode. The one the map you are in belongs to (E1M3: E1; a mission pack's HIP2M1: its second), or one of Quake's."),
        cycle("Game", "vr_relight_batch_game", {{0.f, "The One You're In"}, {1.f, "Quake"}, {2.f, "Armagon"}, {3.f, "Eternity"}, {4.f, "Past"}, {5.f, "Machine"}, {6.f, "Dawn"}})
            .help("Maps: A Game. The one the map you are in is from, or Quake, Scourge of Armagon, Dissolution of Eternity, Dimension of the Past, Dimension of the Machine, Dawn of the Machine (when installed)."),
        command("Relight These Maps", "vr_relight_batch").help("Relights the maps chosen above one after another in the background, while you play (an episode takes a few minutes, every map an hour or more with Bounced Light). The bar above shows how far, and the time left; Cancel stops it. Maps relit with these same settings already are skipped."),
        toggle("Relight Unchanged Maps Too", "vr_relight_batch_force").help("Off: a map relit with the same settings before (and the same map file and texture rules) is skipped. On: every map is relit again."),
        slider("Maps at Once", "vr_relight_parallel", 0.f, 8.f, 1.f, "%.0f").help("How many maps are lit side by side (light's threads shared between them). 0: two on a computer with 8 cores or more, else one. More finishes a batch of small maps a little sooner; maps with Bounced Light gain nothing, and four are slower."),
        cycle("Reload the Map You're In", "vr_relight_batch_reload", {{0.f, "When It's Done"}, {1.f, "At the End"}})
            .help("When the batch relights the map you are in (it goes first) and Reload Where You Are is on: reload it as soon as it is done, or only when the whole batch is."),
        toggle("Progress Outside the Menu", "vr_relight_indicator").help("While maps are being relit, a line on the wrist gadget's screen (on a flat screen, in the corner): maps done, percentage and the time left."),
        header("Lamps and Glowing Textures"),
        slider("Light Textures", "vr_relight_strength", 0.f, 4.f, 0.1f, "%.1fx").extend().help("How bright everything a texture lights is: lamps, light panels, glowing buttons and panels, lava (1: the relight script's)."),
        slider("Lamps and Light Panels", "vr_relight_lamps", 0.f, 4.f, 0.1f, "%.1fx").extend().help("The light fixtures' light, times Light Textures: lamps, light panels and strips, the pyramid lanterns."),
        slider("Glowing Panels and Buttons", "vr_relight_glows", 0.f, 4.f, 0.1f, "%.1fx").extend().help("Buttons, computer panels, runes and teleporters' glow on what is round them, times Light Textures."),
        slider("Lava and Slime", "vr_relight_liquids", 0.f, 4.f, 0.1f, "%.1fx").extend().help("The red glow lava casts on the walls round it, and slime's faint green one, times Light Textures."),
        header("The Map's Light"),
        slider("Map Lights", "vr_relight_maplights", 0.f, 4.f, 0.1f, "%.1fx").extend().help("The map's own lights (the mapper's), as far, this much brighter."),
        slider("Sunlight", "vr_relight_sunlight", 0.f, 4.f, 0.1f, "%.1fx").extend().help("The sun and the sky's light, in maps that have them (outdoor maps; not id's)."),
        slider("Bounced Light", "vr_relight_bounce", 0.f, 2.f, 0.1f, "%.1f").help("Light bouncing off the walls fills the shade: brighter, flatter rooms (0 none, the relight script's moodier look). Takes longer."),
        slider("Ambient Occlusion", "vr_relight_ao", 0.f, 3.f, 0.1f, "%.1f").help("Corners and crevices darker (1.5: the relight script's; 0 none)."),
        slider("Minimum Light", "vr_relight_minlight", 0.f, 64.f, 4.f, "%.0f").help("No place darker than this (0 none): an easy way out of pitch-black corners, at some cost to contrast."),
        cycle("Shadow Quality", "vr_relight_quality", {{0.f, "Fast"}, {1.f, "Smooth"}}).help("Smooth: soft shadow edges (the relight script's). Fast: about four times quicker, harder edges."),
        seeThrough ? toggle("See-Through Liquids", "vr_relight_seethrough").help("id's maps (Quake and its two mission packs) relit with see-through water, slime and teleporters: VisPatch's visibility data put into them, so you see what is under and beyond (how clear: Transparency's alphas). Off: their liquids stay opaque. Maps made with see-through water keep it either way.")
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
        toggle("Retro Lighting", "vr_retrolight")
            .help("The light in Quake's coarse look: the baked light in a few levels (bands), the lightmap's blocky grid, "
                  "dynamic lights in levels and on blocks, hard blocky shadows. Fixed to the surfaces, so nothing swims "
                  "as your head moves. Goes with Retro Textures. Debug > Views > Retro Lighting A/B compares."),
        slider("Block Edge Softness", "vr_retrolight_edge_soft", 0.f, 3.f, 0.25f, "%.2f px")
            .help("The blocks' edges (the lightmap's, the dynamic lights', the shadows'): a ramp this many pixels wide, so they "
                  "don't crawl as your head moves. 0: hard nearest (crawls). Blocks smaller than a pixel turn smooth."),
        cycle("Level Spacing", "vr_retrolight_spacing", {{0.f, "Even in Light (Quake's)"}, {1.f, "Even in Brightness"}})
            .help("How the light levels below are spread: evenly in light, as Quake's colormap (few levels crush the shade to "
                  "black), or evenly in brightness (more levels in the shade, the bands look even)."),
        header("World: the Baked Light"),
        toggle("World", "vr_retrolight_world").help("Retro lighting on the map and its brush models (doors, lifts, boxes)."),
        slider("Light Levels", "vr_retrolight_world_steps", 0.f, 64.f, 1.f, "%.0f")
            .help("The baked light in this many levels a unit of the lightmap (bands). 0: smooth. 64: Quake's software "
                  "renderer's own 64 levels (subtle); 8 to 16: clear bands. Each colour keeps its hue."),
        slider("Level Edges Softness", "vr_retrolight_world_soft", 0.f, 1.f, 0.05f, "%.2f")
            .help("The bands' edges blurred over this share of a level: 0 hard (anti-aliased over a pixel), 1 smooth steps."),
        slider("Dither", "vr_retrolight_world_dither", 0.f, 1.f, 0.05f, "%.2f")
            .help("An ordered (Bayer) dither between the levels, fixed to the texels: the bands break into a pattern "
                  "instead of a hard line. Fades out where its cells get smaller than a pixel."),
        cycle("Dither Cell", "vr_retrolight_world_dither_size", cells).help("The dither's cells, in Quake texels."),
        toggle("Blocky Lightmap", "vr_retrolight_world_lightmap")
            .help("The lightmap read on its grid of blocks instead of smoothly: Quake's chunky light."),
        cycle("Lightmap Block", "vr_retrolight_world_luxel",
            {{1.f, "1 texel (software Quake)"}, {2.f, "2 texels"}, {4.f, "4 texels"}, {8.f, "8 texels"}, {16.f, "16 texels (a luxel)"}})
            .help("The blocks, in Quake texels: 16 is a luxel (GLQuake with nearest filtering: big squares); 1 lights each "
                  "texel on its own (Quake's software renderer: soft gradients in texel steps, aligned with the textures)."),
        header("World: Dynamic Lights"),
        slider("Dynamic Light Levels", "vr_retrolight_world_dyn_steps", 0.f, 64.f, 1.f, "%.0f")
            .help("The flashlight, muzzle flashes, explosions and the other dynamic lights in this many levels a unit "
                  "(their pools in rings). 0: smooth."),
        cycle("Dynamic Light Blocks", "vr_retrolight_world_dyn_block", blocks)
            .help("Dynamic lights found once a block of this many Quake texels (aligned with the texture's blocks): "
                  "pixelated pools of light. Off: per pixel."),
        header("Models"),
        toggle("Models", "vr_retrolight_models").help("Retro lighting on models: monsters, items, weapons, your body."),
        slider("Model Light Levels", "vr_retrolight_model_steps", 0.f, 64.f, 1.f, "%.0f")
            .help("A model's own light (the light grid's, per vertex) in this many levels a unit (as the world's). 0: smooth."),
        slider("Model Level Edges Softness", "vr_retrolight_model_soft", 0.f, 1.f, 0.05f, "%.2f").help("As the world's."),
        slider("Model Dither", "vr_retrolight_model_dither", 0.f, 1.f, 0.05f, "%.2f").help("As the world's, fixed to the skin's texels."),
        cycle("Model Dither Cell", "vr_retrolight_model_dither_size", cells).help("The dither's cells, in the skin's Quake texels."),
        slider("Model Dynamic Light Levels", "vr_retrolight_model_dyn_steps", 0.f, 64.f, 1.f, "%.0f").help("As the world's."),
        cycle("Model Dynamic Light Blocks", "vr_retrolight_model_dyn_block", blocks)
            .help("Dynamic lights found once a block of the skin's texels. Off: per pixel."),
        header("Shadows"),
        cycle("Shadow Edges", "vr_retrolight_shadow_filter", {{0.f, "As Graphics > Shadows"}, {1.f, "Hard"}, {2.f, "Blocky"}})
            .help("Hard: one tap, no soft filtering. Blocky: the shadow map's own texels, as squares."),
        slider("Shadow Levels", "vr_retrolight_shadow_steps", 0.f, 8.f, 1.f, "%.0f")
            .help("A shadow's darkness in levels: 1, lit or not (a hard edge along its middle); 2 or more, steps. 0: smooth."),
        slider("Shadow Level Softness", "vr_retrolight_shadow_soft", 0.f, 1.f, 0.05f, "%.2f")
            .help("Those steps' edges blurred over this share of a level."),
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
