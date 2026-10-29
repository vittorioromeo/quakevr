// vr_menu_debug.inc -- Debug: Views, Logging, Profiling, Reports, Tools. Part of vr_menu.cpp (included where it was:
// the same translation unit, so its anonymous namespace).

[[nodiscard]] za::Vector<Item> pageDebug()
{
    return {
        header("Playtesting"),
        open("Checklist", pageIndex(pageChecklist))
            .help("What to test in the headset or give feedback on (quakevr/checklist.txt), ticked as you go. Also the "
                  "menu's corner button."),
        command("Save My Settings as Defaults", "vr_propose_defaults")
            .help("vr_propose_defaults: every setting you changed from its shipped default (weapon and held object "
                  "slots too) written to quakevr/proposed_defaults/<date>_<time>.cfg for review, sorted by menu page, "
                  "the personal and debug ones marked. Nothing changes in the game. Also the menu's corner button "
                  "Save Default (press twice)."),
        toggle("Voice Notes", vr_notes).help("Raise your off hand to your mouth and hold Y to record a note, with a screenshot and where you are; they go to quakevr/notes."),
        open("Cheats and Recording", pageIndex(pageDebugCheats))
            .help("God, noclip, the weapons and powerups; a scene set up for footage: saved and loaded, cleaned of gibs, "
                  "corpses, props, fires and blood, slowed or frozen, the gadget and hands hidden."),
        open("Slow Motion: Recording", pageIndex(pageRecording)).help("Time Scale (slow motion) is on Graphics > Recording."),
        header("Debug"),
        open("Views", pageIndex(pageDebugViews))
            .help("Drawn in the world: physics shapes, hand bones, ledges, grab tests, the skeleton, collisions, foveation, entity boxes."),
        open("Logging", pageIndex(pageDebugLogging))
            .help("Printed as it happens: shots and damage, throws, climbing, swimming, the hook, wounds, physics; trace files."),
        open("Profiling and Memory", pageIndex(pageDebugProfiling))
            .help("The profiler's panel, capture and hitch log; the memory log."),
        open("Reports", pageIndex(pageDebugReports))
            .help("Print once, to the console: the headset's state, the player, physics, weights, ledges, hands and weapons."),
        open("Tools", pageIndex(pageDebugTools))
            .help("Rebuild the ledge map, reload models, the hand or detail textures; save debug images; test effects."),
        open("Tests", pageIndex(pageDebugTests))
            .help("For playing and tuning in the headset: put a monster or a box ahead of you, fire a projectile at you, "
                  "make an ogre throw, hold and shove enemies; god mode, Quad."),
        open("Ragdoll Tool", pageIndex(pageRagtool))
            .help("The active ragdolls' behaviours on a monster of your choice, flat on the desktop: played, slowed, "
                  "stepped, drawn (skeleton, limits, targets, contacts, balance) and tuned live, saved as presets "
                  "(vr_ragtool; launch with -ragtool; docs/vr-port/RAGTOOL.md)."),
        open("Automation & Self-Tests", pageIndex(pageDebugAutomation))
            .help("Scripted scenarios, self-tests, benchmarks, acceptance checks and crash tests, meant mostly for headless "
                  "agent runs (ROUND22.md, \"Debug > Automation & Self-Tests\")."),
    };
}

// Drawn in the world.
za::Vector<Item> pageDebugViews()
{
    return {
        cycle("Body Clone While Tweaking", vr_menu_body_clone, {{0.f, "Off"}, {1.f, "On"}, {2.f, "Every VR Page"}})
            .help("While a page tweaks something worn on your body (the holsters, the pouches, the flashlight on your belt "
                  "or head and its zones, the wrist gadget's mount, the virtual stock), a mirror image of your body beside "
                  "the menu, facing you, with the zones drawn on it too. Every VR Page: on any page (a test). "
                  "vr_menu_body_clone_info prints where it is."),
        slider("Body Clone Distance", vr_menu_body_clone_distance, 0.8f, 3.f, 0.1f, "%.1f m").extend(0.5f, 5.f)
            .help("From your eyes to the image's."),
        cycle("Body Clone Side", vr_menu_body_clone_side,
            {{0.f, "Away from Main Hand"}, {1.f, "Left"}, {2.f, "Right"}, {3.f, "Straight Ahead"}})
            .help("Beside the menu panel, on this side; Straight Ahead: behind the panel (drawn over it)."),
        toggle("Animated Surfaces", vr_anim_surface)
            .help("On: an animated texture's frames (the wall buttons' lit and dim frames) share one surface: the same "
                  "bumps, parallax depth, sheen and detail, only their colours and glow change. Off: each frame its own "
                  "(the relief and sheen pulsed with the frames). At once."),
        command("Retro Textures: List", "vr_retro_list")
            .help("vr_retro_list: prints each model drawn now with its retro textures kind (Graphics > Retro Textures), its "
                  "set and its skin's size, to the console."),
        command("Flame Lights Info", "vr_flame_lights_info")
            .help("vr_flame_lights_info: the flamethrowers' stream lights now (Graphics' Flamethrower Lights): how many "
                  "flames were drawn last frame, the clusters they made, and each light's place, level and reach."),
        command("Lightstyle Rate (5 s)", "vr_lightstyle_rate 5")
            .help("vr_lightstyle_rate 5: for 5 s of game time, each animated lightstyle's steps a second (Quake's 10, "
                  "whatever the frame rate), its swing, changes a second and largest jump a frame, to the console (LSRATE "
                  "lines). Compare Soften Flickering Lights (Comfort) at 0 and on."),
        command("Fog Check", "vr_fog_check")
            .help("vr_fog_check: sets the fog at once at a thousand clock values, and checks a fade with the clock just "
                  "before it began: the fog drawn must be the one set, never NaN (the old division by a zero fade time "
                  "drew Arcane Dimensions' world black for a frame each time it sent its fog). Prints fogcheck: N passed, "
                  "M failed. The fog is put back."),
        command("Light Probe", "vr_light_probe")
            .help("vr_light_probe: the baked light at six points round you, as the map has it and as it is drawn with the "
                  "current Ambient Light and Light Contrast (128 is Quake's full light). The same numbers light a model "
                  "standing there: two settings, compared without looking."),
        toggle("Show Damage Numbers", vr_debug_damage_numbers)
            .help("Every hit on anything that takes damage (monsters, corpses, gibs, crates, props, shootable walls): its "
                  "damage floating where it struck, and printed as the training dummy's (what struck, where, what's left)."),
        toggle("Show Grapple Rope", vr_debug_rope)
            .help("Each grappling hook rope: its drawn chain (points white, pieces green), the corners it wraps round "
                  "(red) and its taut path (yellow), the ends (orange)."),
        toggle("Show Physics Shapes", vr_debug_physics_shapes)
            .help("Draws the physics bodies (Box3D) as wireframes: props awake green, asleep blue, held yellow; doors purple, "
                  "monsters orange, you cyan, hanging pickups grey; red dots where they touch. And each hand's grab reach."),
        toggle("Show Holster Zones", vr_holster_zones_show)
            .help("vr_holster_zones_show: every holster's and pouch's zone as a box: grey idle, blue with a gun in it, yellow "
                  "a candidate (a hand or its gun in it, another deeper), green where a hand is (holding a gun: where it "
                  "goes when let go); white dots: the hand and its gun's grip and body. Sizes: Immersion > Holster Zones."),
        toggle("Show Holster Item Boxes", vr_holster_zones_show_items)
            .help("vr_holster_zones_show_items: each zone's box as for a hand carrying an ammo box, a health box or a "
                  "backpack (vr_holster_zone_item_*), magenta, on you and on the menu's body clone."),
        command("Print Holster Zones", "vr_holster_zones_report")
            .help("vr_holster_zones_report: each zone's middle and size (units from the eyes) and how deep each hand's "
                  "points are in each (under 1: inside), the old spheres' share beside."),
        command("Print Holster Setup", "vr_holstersetup_report")
            .help("vr_holstersetup_report: whether Holster Setup runs (VR Settings > Holster Calibration), "
                  "what is held, and each holster's and pouch's reach point (units from the eyes: forward, right, up) and size."),
        cycle("Show Hit Zones", vr_debug_hitzones, {{0.f, "Off"}, {1.f, "Positional Damage"}, {2.f, "Decapitation"}, {3.f, "Both"}})
            .help("Positional Damage colors the animated model surface: head red, body green, extremities yellow, legs blue. "
                  "Uses the same standing-pose mapping and Head Priority as precise shots and positional melee. "
                  "Precise hits off: box/ray reference zones. Decapitation colors, on the animated model too, where a "
                  "slash beheads (magenta: the melee's zone, Head Size and Neck) and the head that shots, lasers and "
                  "thrown axes pop (red); Both outlines the magenta over Positional Damage's colors. Every head and limb "
                  "test maps the point struck to the standing pose, so these zones move with the model."),
        toggle("Hit Zones Through Walls", vr_debug_hitzones_xray)
            .help("Draws the animated positional regions through walls and the back of the model. Off: only visible surfaces."),
        cycle("Show Gadget Button", vr_debug_gadget_button,
            {{0.f, "Off"}, {1.f, "Its Hit Volume"}, {2.f, "And the Screen Tap"}, {3.f, "And Print Sync"}})
            .help("vr_debug_gadget_button: the wrist gadget's side button's hit volume (green ready, yellow pressed, red "
                  "cooling down) and your fingertip (the drawn index fingertip white, joined to the tuned one that "
                  "presses), its presses printed; And the Screen Tap: also bullet time's tap zone over the screen and the "
                  "striking volume (your hand's surface blue, the gun's butt orange, white on the zone); And Print "
                  "Sync: also prints each frame how far the zone is from the drawn gadget. The settings: HUD and Menus "
                  "> Wrist Gadget."),
        command("Press the Gadget Button", "vr_gadget_button_press")
            .help("vr_gadget_button_press: a press of the wrist gadget's side button without a fingertip (the light; "
                  "six quick ones open Doom, one closes it)."),
        command("Gear Lights Info", "vr_gear_lights_info")
            .help("vr_gear_lights_info: the gear lights' state, the side button's place and your fingertip's distance to "
                  "it, and the stealth AI's light on you now."),
        cycle("Show Hits", vr_debug_hits, {{0.f, "Off"}, {1.f, "Hits"}, {2.f, "Hits and Misses"}})
            .help("Precise hit detection: each hit on a monster's model drawn for a few seconds (the model as it was then, "
                  "the triangle hit in green, the point on the model in red, where the grown model was met in yellow) and "
                  "printed. Hits and Misses: also the shots that went through a monster's box beside its model (orange)."),
        toggle("Show Melee Blades", vr_debug_melee_blades)
            .help("vr_debug_melee_blades: what each hand's blows strike with, live (swords, axes, fists, guns used as clubs, "
                  "torches): the blow line from the hand to the tip (bold; the near end behind the hand thinner), white "
                  "idle, yellow while the hand moves fast enough to strike, red just after a blow landed; the lines the "
                  "sweep tested and the blade's swept surface over the last third of a second, fading; a hammer's or "
                  "mace's head as its box. A red dot where a blow landed, its damage above it."),
        toggle("Show Armed Humanoids' Aim Points", vr_agrunt_aim_exposed_show)
            .help("vr_agrunt_aim_exposed_show: where each armed humanoid aims on you, each time it looks (every 0.1 s): "
                  "yellow sparks on the part of you its gun's line reaches, red where it last aimed when nothing of you "
                  "shows (it holds fire). The setting: Armed Grunts > AI Behaviour > Aim at What They Can Hit."),
        cycle("Show Shot Disarms", vr_agrunt_shotdisarm_debug, {{0.f, "Off"}, {1.f, "Print"}, {2.f, "Print and Volumes"}})
            .help("vr_agrunt_shotdisarm_debug: each shot of yours on an armed grunt printed (shotdisarm: the part it hit, "
                  "weapon, hand or body; the damage adding up; each roll, its chance and result); Print "
                  "and Volumes also shows each held weapon's line (blue-white sparks) and its hand and forearm (sparks). "
                  "The settings: Armed Grunts > Disarming: Shooting It Out."),
        command("Edit Blade Shape (Main Hand)", "vr_bladeedit main")
            .help("vr_bladeedit: the main hand's weapon's melee blade shaped by hand (as Weapon Offsets > Edit Blade "
                  "Shape): its tip, near end, width, curve and head box. With the hand empty: the fist's blow line."),
        command("Pose Studio (Armed Humanoids' Grips)", "vr_posestudio")
            .help("vr_posestudio: the armed grunts' and enforcers' grips set by hand (as Armed Grunts > Grip Editor > "
                  "Drag Mode): walk up to one, grab its weapon or hand, move and turn it. vr_posestudio_report prints "
                  "its parts and grips."),
        command("Pose Studio: Live On/Off", "vr_posestudio_live")
            .help("vr_posestudio_live (the studio running: its panel's Live button): the armed humanoid shown fights in "
                  "place, its frames taped; Pause, scrub (a stick, or < 0.5 s >), Next Glitch, drag the key the readout "
                  "names at that frame. vr_posestudio_live_report prints the tape's playhead: its keys and the glitch "
                  "finder's measures."),
        command("Rig Studio (Flat Screen)", "vr_rigstudio")
            .help("vr_rigstudio: the rigged humanoids' skeletons, skin weights, poses and animations edited on the desktop "
                  "screen, with Blender's mouse (docs/vr-port/RIGSTUDIO.md): a room, an armed grunt frozen in it, an "
                  "outliner, properties, a timeline with keys, new sequences QC can play. Flat screen only (start the "
                  "game with -flatscreenmode); vr_rigstudio_close closes it."),
        command("Rig Studio: Range of Motion (Authored Rig)", "vr_rigstudio;vr_rigstudio_rom on")
            .help("The Rig Studio with its Range of Motion window: the authored grunt rig (quakevr/rigs/vrgrunt.rig2.txt) "
                  "posed joint by joint within its limits (sliders, a limb dragged by its end, Sweep, Random Pose). Its "
                  "held weapons ride its hands (the right's weapon bone, the left hand) at their grip offsets. Flat "
                  "screen only."),
        command("Rig Studio: Test Weapons on the Authored Rig", "vr_rigstudio;vr_rigstudio_rom test weapon")
            .help("vr_rigstudio_rom test weapon: the subject on the authored rig at rest, an arm raised, a wrist bent, the "
                  "left arm, two random poses: each held weapon's grip read back from where it is drawn against its hand "
                  "bone's (within 0.5 units, 2 degrees), a screenshot each; the console says how many passed. Flat screen "
                  "only; pick a dual-wielding subject first (Subject menu) to test the left hand too."),
        command("Rig Studio: Test the Off Hand per Pose", "vr_rigstudio monster_vr_grunt 29;vr_rigstudio_test_offmodes")
            .help("vr_rigstudio_test_offmodes: the nail pistol grunt held in Carry, Aim, Run, Pickup and the Shove, each "
                  "pose's Off Hand set to Auto, On the Gun and Free in turn (on the gun, off it), then saved and read back "
                  "with Carry Free and On the Gun; every key put back after. The console says how many passed. Flat screen "
                  "only."),
        command("Rig Studio: Test Typing in the Fields", "vr_rigstudio;vr_rigstudio_test_typing")
            .help("vr_rigstudio_test_typing: keys, typed characters and clicks sent through the game's input loop into "
                  "the studio's text fields (a sequence's FPS typed, undone, cancelled; the Outliner's search typed; the "
                  "console's key): the console says how many passed. It makes a sequence 'typetest' (not saved). Flat "
                  "screen only; keep the mouse and keyboard still while it runs."),
        command("Rig Studio: Test Keys on the Authored Rig", "vr_rigstudio;vr_rigstudio_test_rig2anim")
            .help("vr_rigstudio_test_rig2anim: the Timeline on the subject's rig (Grunt on the Authored Rig on and a map "
                  "loaded with it: the authored bones): load's chest and right upper arm keyed at frames 3 and 7, a "
                  "forearm bent past its hinge (held within it) and undone, the pose mirrored and undone, saved and read "
                  "back. It WRITES the subject's animation files (vr_rigdata_dir rigstudio_test first keeps them apart); "
                  "the console says how many passed. Flat screen only."),
        command("Check the Authored Grunt Rig", "vr_rig2_check")
            .help("vr_rig2_check: the authored rig swept through every joint's range, joint pairs and 1000 random poses: "
                  "see-through holes, flipped and collapsed triangles, sharp folds, stretch (the console; ends on "
                  "'rig2 check: N passed, M failed'). vr_rig2_check progs/vrgrunt.mdl 0 "
                  "envelope prints how far each joint turns cleanly."),
        cycle("Armed Humanoids' Armour (P0)", vr_agarmor, {{0.f, "None"}, {1.f, "Green"}, {2.f, "Yellow"}, {3.f, "Red"}, {4.f, "One of Each"}})
            .help("vr_agarmor (debug, the armour spike: drawn only, no gameplay): armour shells made from the grunt's own mesh "
                  "(a cuirass and a helmet) on every grunt and enforcer drawn on the authored rig (the Rig Studio's "
                  "subject too), in the player's armour colours. One of Each: green, "
                  "yellow, red by entity number. vr_agarmor_set <entity> <type> [pieces] for one."),
        cycle("Armour Pieces", vr_agarmor_pieces, {{3.f, "Cuirass and Helmet"}, {1.f, "Cuirass"}, {2.f, "Helmet"}})
            .help("vr_agarmor_pieces: which pieces the armoured grunts wear (each piece hidden by collapsing its bones)."),
        command("Check the Armour's Fit", "vr_agarmor_check")
            .help("vr_agarmor_check [model] [random poses]: the armour shells made twice (alike), closed, then measured on "
                  "the authored rig's rest pose, every joint near them swept and 200 random poses: body vertices inside "
                  "a plate and plate vertices inside the body, by neck, shoulders, waist and torso (the console; ends on "
                  "'agarmor check: N passed, M failed'). vr_agarmor_info prints their sizes."),
        command("Print the Authored Rig's Drawing", "vr_rig2_draw_info")
            .help("vr_rig2_draw_info: the Rig Studio's subject on the authored rig, as the renderer draws it beside its old "
                  "rig's .mdl: each model's vertices, triangles, skin and normal map, how often drawn last frame, the retro "
                  "textures' Quake skin size and lighting share (both must match: the same look)."),
        command("Print Blade Shape", "vr_blade_dump")
            .help("vr_blade_dump [slot]: the main hand's weapon's blade shape and tip as vr_weapons.inc lines (* changed)."),
        command("Print Fist Line", "vr_blade_dump fist")
            .help("vr_blade_dump fist: the empty hand's blow line (vr_melee_fist_*) as vr_cvars.inc lines (* changed)."),
        toggle("Show Hand Bones", vr_debug_hand_bones)
            .help("Draws both hands' joints and bones, and what they grip: the finger spheres (green touching, yellow near, red "
                  "sunk in) and the palm's fit (white: where the hand is, cyan: where the grip moved the palm)."),
        cycle("Show Ledges", vr_debug_ledges, {{0.f, "Off"}, {256.f, "Within 256 units"}, {1.f, "Within 512 units"}, {1024.f, "Within 1024 units"}})
            .help("The ledge map round you, what climbing holds: lips green (on moving brushes pink; on props orange, as they "
                  "lie now: Climb Props), a tick out every 8 units (yellow where the drop starts further out). Built even with "
                  "climbing off."),
        toggle("Log Empty Hand Against Held", vr_debug_hand_collide)
            .help("Prints, each frame an empty hand meets the weapon or the thing the other hand holds, how near the real "
                  "and the drawn hand's palm and knuckles are to its surface, how far the drawn hand and the thing are "
                  "held back (Carrying: Empty Hand Against Held Things)."),
        cycle("Show Grab Test", vr_debug_carry, {{0.f, "Off"}, {1.f, "Drawn"}, {2.f, "Drawn and Logged"}, {3.f, "Also Far Fists"}})
            .help("For each hand near something to carry: the box it is drawn in and the fist tested (its spheres; the nearest "
                  "bright). Logged: each grab and held prop's placing printed, carry_trace.txt written, and held props meeting each "
                  "other, a weapon or a wall; also far fists: hands not near anything too."),
        cycle("Climbing Footholds Debug", vr_debug_climb_feet, {{0.f, "Off"}, {1.f, "Log Holds"}, {2.f, "Log Each Frame"}, {3.f, "Logged and Drawn"}})
            .help("Climbing, prints each foothold a foot finds (climbfoot: where, what, how many traces); each frame's feet "
                  "(climbfeet: planted or stepping, their slip, any in a wall); drawn: each foot's hold and the search."),
        cycle("Show Tracked Skeleton", vr_bodytrack_debug, {{0.f, "Off"}, {1.f, "On the Body"}, {2.f, "Beside the Body"}})
            .help("Body tracking: the headset's tracked skeleton drawn (cyan: seen, magenta: guessed by the headset, as "
                  "Quest 3's legs), on your body or beside it."),
        cycle("Skeleton Bones", vr_bodytrack_debug_bones,
            {{0.f, "Tracked"}, {1.f, "Untracked (Our IK)"}, {2.f, "Both Overlapped"}, {3.f, "Body Over Tracked"}})
            .help("Which bones Show Tracked Skeleton draws: the headset's tracked ones (cyan, magenta); our IK's, the body "
                  "posed without the tracking (orange: while they are drawn, the body ignores the tracking); or both "
                  "over each other, to compare. Body Over Tracked: the body's own bones as posed with the tracking "
                  "(yellow) over the tracked ones: the yellow elbow should sit on the magenta one."),
        command("Print Body Tracking Status", "vr_bodytrack_status"),
        command("Print Seated Status", "vr_seated_status")
            .help("vr_seated_status: seated play (VR Settings > Height > Seated Play): the lift, your head and the drawn "
                  "legs over the floor, the leg tracking's weights (0 seated) and where the holsters and pouches are on "
                  "the body."),
        command("Print Body Tracking Stats", "vr_bodytrack_stats")
            .help("vr_bodytrack_stats: the body's errors against the tracked one since the last reset, how far the arms "
                  "stretch while the elbows are fitted (1: the calibrated arm fits the tracked one), and per arm the elbow "
                  "fit (its weight, how often it met a limit) and how far the tracked wrist and shoulder are from ours."),
        command("Reset Body Tracking Stats", "vr_bodytrack_stats reset"),
        command("Record Body Tracking (10 s)", "vr_bodytrack_dump 10")
            .help("Writes 10 seconds of the raw tracked joints, the weights, the elbow fit and the body's joints to "
                  "bodytrack_dump.csv in the game folder (Misc/quakevr/bodytrack_dump_report.py sums it up)."),
        cycle("Show Body Skeleton", vr_body_debug,{{0.f, "Off"}, {1.f, "Skeleton"}, {2.f, "Body Facing You"}, {3.f, "Body From Its Left"}})
            .help("Draws the body's skeleton; or shows the body in front of you, facing you or seen from its left (to check "
                  "its pose and calibration without a mirror)."),
        cycle("Preview Crouch", vr_body_crouch_preview,
            {{-1.f, "Off"}, {0.f, "Standing"}, {0.25f, "Quarter"}, {0.5f, "Half"}, {0.75f, "Three Quarters"}, {1.f, "Full"}})
            .help("vr_body_crouch_preview: the Crouched Pose (Body) blended as if you crouched this deep, standing or not: "
                  "its offsets on the body, the holsters and the ammo pouch (the crouch itself as you are). Off: as deep "
                  "as you crouch. Not saved."),
        command("Print Crouched Pose", "vr_body_crouch_report")
            .help("vr_body_crouch_report: how much of the crouched pose (Body > Crouched Pose Offsets) the body takes now, "
                  "the pelvis, chest, shoulders and elbows, the holsters and the ammo pouch: units from your eyes, forward, "
                  "right, up."),
        command("Print Torso Direction", "vr_torso_report")
            .help("vr_torso_report: the head's yaw, the old and the new torso guesses, the hands' pull and weights."),
        toggle("Log Body Drift", vr_debug_body_error)
            .help("Twice a second in the console: how far the game's body (its feet) has drifted in your room since it was turned "
                  "on (or marked), and how far its facing has turned, with your lean. Stand still in the room: leaning and "
                  "turning or moving with the stick, both should stay near 0 (vr_body_error)."),
        toggle("Log Body Heights", vr_debug_body_heights)
            .help("Twice a second in the console: metres above the body's floor of your eyes (as calibrated and as learnt "
                  "standing), the pelvis, the knees and ankles (and how straight the legs are, 1 straight), the ammo pouch "
                  "and the hip holsters (and how far from the pelvis), the grenade pouch and the chest holsters. Standing "
                  "still, they should stay put; a leg at 1 with its ankle off the floor is too short."),
        command("Mark Body Drift Here", "vr_body_error mark")
            .help("vr_body_error mark: measure the body's drift from where it stands now (stand straight, your feet where they "
                  "are to stay)."),
        cycle("Show Body Collisions", vr_debug_body_collide, {{0.f, "Off"}, {1.f, "Logged"}, {2.f, "Logged and Drawn"}})
            .help("The drawn hands and weapons stopping at each other and the body: each contact printed (and "
                  "body_collide_trace.txt); drawn: the capsules and the pushes."),
        command("Weapons Into the Walls", "vr_gun_wall_depth")
            .help("vr_gun_wall_depth: each hand's weapon as drawn (held by its handle, by a hotspot, anywhere, two-handed): "
                  "how deep it goes into the walls and floors, and how far they hold the hand out, in the console."),
        cycle("Log Hand Offsets", vr_debug_hand_offset, {{0.f, "Off"}, {1.f, "When Moved"}, {2.f, "Every Frame"}})
            .help("Each hand drawn away from where it is tracked, and by what (walls, the weight's spring, models, the body, "
                  "a held thing against a wall, the other hand's weapon or held thing), in the console."),
        open("Flashlight Zones: Flashlight - On a Gun or Head", pageIndex(pageFlashlightMounts)).help("Showing the flashlight zones is on Flashlight > On a Gun or Head."),
        command("Flashlight to Left Hand", "vr_flashlight_give left")
            .help("vr_flashlight_give left: the chest flashlight into the left controller's hand, as if gripped there (that "
                  "hand's grip lets go of it). Its buttons are then that controller's, with either Main Hand."),
        command("Flashlight to Right Hand", "vr_flashlight_give right").help("vr_flashlight_give right: the same, the right hand."),
        command("Clip Flashlight on Right Gun", "vr_flashlight_clip_gun right")
            .help("vr_flashlight_clip_gun right: the flashlight clipped on the gun in the right hand, from wherever it is "
                  "(left: the left hand's). Its place there: Weapon Offsets > Flashlight."),
        command("Clip Flashlight on Head", "vr_flashlight_clip_head right")
            .help("vr_flashlight_clip_head right: the flashlight clipped at your right temple, from wherever it is "
                  "(left: the left one)."),
        command("Flashlight Home", "vr_flashlight_home")
            .help("vr_flashlight_home: the flashlight back on the belt at once, from a hand, a gun or the head (the light "
                  "stays as it was)."),
        command("Flashlight Cord Info", "vr_flashlight_cord_info")
            .help("vr_flashlight_cord_info: in the console, the cord's last frame (Body > Flashlight > Cord): its "
                  "length, its rings and triangles, a chain's links."),
        command("Cell Cord Info", "vr_cellcord_info")
            .help("vr_cellcord_info: in the console, the cell cords (Weapons > Reloading > Cell Cords): each one's weapon, "
                  "its ends (the weapon's bottom, the pouch's cell), loose or plugged in, its length and rings."),
        command("Weapon Settings Inheritance", "vr_wofs_dump")
            .help("vr_wofs_dump: in the console, the weapon slots that inherit another's settings, and from which; "
                  "vr_wofs_dump <slot> ... prints every value a slot uses (\"<-n\": slot n's)."),
        command("Held Heads' Weights", "vr_prop_heads")
            .help("vr_prop_heads: in the console, each monster head's held-object slot, its Mass setting and what it "
                  "weighs (Mass -1: its share of its monster's ragdoll, an AD monster's as the class it is tuned as)."),
        command("Magnetic Holsters Info", "vr_magholster_info")
            .help("vr_magholster_info: in the console, each holster's bound weapon (Immersion > Magnetic Holsters): where it "
                  "is (holstered, held, away and how far), its chains and their links."),
        command("Magnetic Holsters: All Home", "vr_magholster_return")
            .help("vr_magholster_return: every bound weapon lying about flies back to its holster now (if it is empty)."),
        toggle("Magnetic Holster Chain Log", vr_magholster_chain_log)
            .help("vr_magholster_chain_log: each frame, in the console, every magnetic holster chain's worst link stretch (a share), "
                  "and its most movement and jerk off its anchor (mm); a jump that starts it afresh."),
        command("Probe Flashlight", "vr_flashlight_probe menu")
            .help("vr_flashlight_probe: in the console, where the torch is, whether each hand is at it (lit, as the game "
                  "reads the hand, still), whether B/Y would clip it on the head or a gun, and the player's speed and turn."),
        cycle("Show Model Collisions", vr_debug_model_collide, {{0.f, "Off"}, {1.f, "Logged"}, {2.f, "Logged and Drawn"}})
            .help("Held weapons and props stopping at the models' triangles: each hand's push printed; drawn: the rays (grey as tracked, "
                  "green or red as drawn) and the push (yellow)."),
        toggle("Show Foveation", vr_foveated_debug)
            .help("The shading rates of Foveated Rendering in the eyes and the mirror (yellow 2x2, red 4x4) and the upscaler's "
                  "sharp circle (cyan)."),
        cycle("Show Motion Vectors", vr_temporal_debug,
            {{0.f, "Off"}, {1.f, "Vectors"}, {2.f, "Reactive Mask"}, {3.f, "Reprojection Test"}})
            .help("vr_temporal_debug (TAA's inputs, vr_temporal.cpp): Vectors: red/green the motion across/up the eye, blue "
                  "a model's own; Reactive Mask: red translucency and particles, green history thrown away, blue liquids; "
                  "Reprojection Test: the last frame moved by the vectors (it should hold still as you move; red: thrown away)."),
        command("Print Reprojection Error", "vr_temporal_error")
            .help("vr_temporal_error: with Reprojection Test on, each eye's mean error of the last frame moved by the vectors, "
                  "against not moved (0-255), in the console."),
        toggle("Show Parallax Depth", "vr_parallax_debug")
            .help("vr_parallax_debug: where the walls' parallax writes the depth of the carving it shows (Graphics > Surfaces > "
                  "Parallax Depth Write): red, brighter the deeper below the surface; green, brighter the higher over it "
                  "(Parallax Surface Level); dark blue: the surface's own depth."),
        toggle("Parallax Depth Split", "vr_parallax_depth_split")
            .help("vr_parallax_depth_split: with Parallax Depth Write on, only the textures with relief nearer than the "
                  "parallax's reach (vr_parallax_depth_distance) use the depth-writing shader; the rest keep the early depth "
                  "test, which that shader loses (the same picture, about 0.8 ms less in an AD fight). Off: all of it, as before."),
        toggle("Show Entity Boxes", "r_showbboxes")
            .help("Every entity's bounding box, as the game collides with it (monsters, items, missiles, triggers), through "
                  "walls. Single player only."),
    };
}

// Printed as it happens (the console, and the wrist log), or a trace file written every frame.
za::Vector<Item> pageDebugLogging()
{
    return {
        cycle("Developer Messages", "developer", {{0.f, "Off"}, {1.f, "On"}, {2.f, "Verbose"}})
            .help("The game's developer messages: needed by Shots and Damage and the Grappling Hook's log below, and many "
                  "others (melee events, grenades, deflections). Verbose: every frame's melee detail too."),
        header("Logs"),
        toggle("Gore Sounds", vr_gore_sound_debug)
            .help("vr_gore_sound_debug: each gore sound played or dropped (capped, rate-limited, too far, off) and why, "
                  "and a choking monster's cry dropped (Gore > Gore Sounds; vr_gore_sound_stats counts them)."),
        toggle("Bloody Prints", vr_bloodprints_debug)
            .help("vr_bloodprints_debug: each bloody footprint and handprint (where, its way, size, strength; the ankle and "
                  "toe or the wrist it came from), each pool or body that bloodied your feet."),
        cycle("Bullet Time", vr_debug_bullettime, {{0.f, "Off"}, {1.f, "On, Off, Refused, Refills"}, {2.f, "And the Screen Tap"}})
            .help("Bullet time starting, stopping and refused (the meter, the cooldown), each tap and taps that never "
                  "stopped on the screen; And the Screen Tap: every frame near the gadget's screen, how far over it your "
                  "hand (and its gun's butt) is and how fast it comes into it."),
        cycle("Flashlight Flicks", vr_flashlight_flick_debug, {{0.f, "Off"}, {1.f, "Each Flick and Why"}, {2.f, "And the Wrist Every Frame"}})
            .help("vr_flashlight_flick_debug: the wrist flick that turns the held torch over, taken or refused (and why); 2: the held hand's wrist speed every frame."),
        cycle("Chainsaw", vr_debug_chainsaw, {{0.f, "Off"}, {1.f, "Pulls and Cuts"}, {2.f, "And the Bar in Monsters"}})
            .help("The chainsaws' cords (taken, pulled, too slow, let go), their engines (started, stalled) and cuts; "
                  "And the Bar: also each cut's test against what is near, how deep the drawn bar sinks into a monster, "
                  "and the cord's hole drawn. The engine and cut lines need Developer Messages."),
        cycle("Flame Wobble", vr_debug_flame_wobble, {{0.f, "Off"}, {1.f, "Each Push"}, {2.f, "And Every Frame's Lean"}})
            .help("Flames stirred by a fast hand (Graphics > Fire Model Particles > Flames in the Air): each push (the "
                  "flame, the hand, how near and fast); And Every Frame's Lean: each swinging flame's lean every frame."),
        cycle("Wall Buttons", vr_debug_wallbuttons, {{0.f, "Off"}, {1.f, "Each Press"}, {2.f, "And Weapon Lines"}})
            .help("Each button pressed: what pressed it and how (a hand, a held weapon and its line, a thrown thing and "
                  "its speed, stepped on), and where the hand was: how far from the button and from you (a far one is a "
                  "stray press). And Weapon Lines: every frame, each held weapon's line that presses buttons "
                  "(from its pommel or butt to its tip or muzzle). Needs Developer Messages."),
        cycle("Monster Poses", vr_debug_pose_check, {{0.f, "Off"}, {1.f, "Squashed Bodies"}, {3.f, "And a Screenshot"}, {2.f, "Every Frame"}})
            .help("vr_debug_pose_check: a monster drawn squashed between two poses for over 0.2 s (\"posecheck:\"; a "
                  "parried ogre was, for half a second), drawn with a pose out of its model's range or a broken blend; "
                  "And a Screenshot: a picture at each; Every Frame: each monster's drawn poses and how flat (a lot)."),
        toggle("Shots and Damage", vr_debug_shots)
            .help("Each hitscan shot (where it starts, its direction, what its pellets hit, headshots), each damage you deal "
                  "(and when Quad's sound plays) and each prop a shot pushes. Needs Developer Messages."),
        toggle("Missile Hits", vr_debug_missiles)
            .help("Each missile's hit (rockets, nails, grenades, lasers...): what it met, where and the surface's normal; "
                  "a solid prop: its drawn box as it stands (vr_debug_missiles)."),
        cycle("Throws", vr_debug_throw, {{0.f, "Off"}, {1.f, "Each Throw"}, {2.f, "And Its Timing"}})
            .help("Each throw's speed estimate from the hand's motion (and the release's timing); a thrown weapon's spin "
                  "and how far its axis is off end over end (0: a wrist flick's, tip going down; needs Developer Messages)."),
        toggle("Controller Spin", vr_debug_angvel)
            .help("While a controller turns fast: which frame its angular velocity is read in (Controller Spin Frame), "
                  "how well the runtime's and the fixed one follow the turn, the hand's speed before and after."),
        toggle("Highlights", vr_debug_highlights)
            .help("Each moment the highlight log takes (Graphics > Recording > Log Highlights): its time from the sync "
                  "mark, kind, score and what it was done to and with."),
        toggle("Axe Sticks", vr_debug_axestick)
            .help("Each thrown axe's blade striking something: stuck (how fast, how deep, at what angles) or why it bounced; "
                  "its bleeding, its fall, its pull. Needs Developer Messages for the last."),
        cycle("Climbing", vr_climb_debug, {{0.f, "Off"}, {1.f, "Holds"}, {2.f, "Every Frame"}, {3.f, "And Shoulders"}})
            .help("Holds taken, released, mantles; every frame: the body, the hands, the pull, the holds' reach (a lot)."),
        cycle("Hands", vr_debug_hands, {{0.f, "Off"}, {1.f, "When They Change"}, {2.f, "Every Frame"}})
            .help("Each hand's state: the grip, its weapon, what it carries, the force grab, the flashlight, the hotspot, and "
                  "what climbing makes of a grip (holding, free, or why it takes nothing)."),
        toggle("Swim Strokes", vr_swim_debug).help("Each stroke: its peak speed, the power gate, the reverse damping and the push it gave."),
        cycle("Grappling Hook", vr_grapple_debug,
            {{0.f, "Off"}, {1.f, "Bites and Reels"}, {2.f, "And the Rope"}, {3.f, "Every Frame"}, {4.f, "And Every Rope Drawn"}})
            .help("What the hook bites and each reel (mass, class, speeds); the rope 4 times a second, or every frame; and "
                  "each rope drawn every frame (which beam, from where to where: a rope drawn where there is none). Needs "
                  "Developer Messages."),
        toggle("Controller Buttons", vr_debug_buttons)
            .help("Each controller button pressed and let go: the hand, the button, its key and binding, and what took it "
                  "(the posing mode, a voice note, the flashlight); and the controller profile the headset's runtime "
                  "picked for each hand (Virtual Desktop can report Index controllers). For a button that does nothing."),
        command("Restore Controller Bindings", "vr_bindings_reset")
            .help("vr_bindings_reset: the controller buttons' keys bound as Quake VR ships them again (quakevr/"
                  "vr_bindings.cfg: triggers fire, grips grab, A jumps...). For controls that act as a gamepad's (the left "
                  "trigger jumping, the grips changing weapons)."),
        toggle("Other Ports' Startup Configs", vr_foreign_configs)
            .help("vr_foreign_configs: at the start, an autoexec.cfg or config.cfg found only outside Quake VR's own "
                  "folder (in id1 next to a GOG or Steam Quake, a re-release folder: another source port's) runs too. Off "
                  "(the default): only quakevr's own; a skipped one is named in the console. Exec by hand runs any."),
        toggle("Wounds", vr_wounds_debug).help("Each wound painted on a model."),
        cycle("Grasp", vr_debug_grasp, {{0.f, "Off"}, {1.f, "Each Solve"}, {2.f, "Each Finger"}})
            .help("Each grasp solve of the jointed hands (and each finger's stops)."),
        cycle("Put-Away Transition", vr_debug_collect_fx, {{0.f, "Off"}, {1.f, "Each Thing"}, {2.f, "Each Frame"}})
            .help("Each thing put away at a holster or pouch (its model, from where, the holster) and when it has gone "
                  "in; or also each frame's size and distance left."),
        command("Check Last Pose", "vr_pose_check")
            .help("vr_pose_check: after the posing mode, how far what you set is from what you get. A weapon or hotspot: "
                  "hold it. In a holster: holster it there; each holster of that kind holding it is measured."),
        cycle("Physics Sounds", vr_debug_physsound, {{0.f, "Off"}, {1.f, "Each Sound"}, {2.f, "And Each Hit Skipped"}})
            .help("Each knock (the prop, its material and weight, the hit's speed, the volume), each scrape starting and "
                  "stopping and each climbing grab (the hold's texture); or also the hits too soon after the last and each "
                  "scrape grain."),
        cycle("Spatial Audio", vr_debug_snd, {{0.f, "Off"}, {1.f, "Each Second"}, {2.f, "And Each Voice"}})
            .help("Each second: the voices in use, their mix's time and the simulations' (the walls, the room's reverb); or "
                  "also each voice: its sound, distance, occlusion (low, middle, high) and Doppler."),
        cycle("Network Messages", vr_debug_net, {{0.f, "Off"}, {1.f, "When the Broadcast Is Full"}, {2.f, "And Each Second"}})
            .help("vr_debug_net: each frame the unreliable broadcast (particles, sounds, temp entities, wounds, tracers) "
                  "was full: how much QuakeC's writes lost and how much a client was not sent (whole messages only); or "
                  "also each second the most sent: the broadcast, the reliable messages, a client's own data and entities."),
        toggle("Ragdolls", vr_debug_ragdoll)
            .help("vr_debug_ragdoll: each ragdoll made (when it went limp, its parts), each push on it and each blast's throw."),
        cycle("Ragdolls' Look Check", vr_debug_ragdoll_look, {{0.f, "Off"}, {1.f, "Skinned Copy at Rest"}, {2.f, "Its .mdl at Rest"}, {3.f, "Hidden"}})
            .help("vr_debug_ragdoll_look: each ragdoll drawn in its model's rest pose where its entity stands, as its "
                  "skinned copy or as its own .mdl (the live monster's look): the two should look the same. "
                  "vr_ragdoll_draw_info in the console: the renderer's data for each that differ."),
        command("Print the Authored Rig's Arms", "vr_rig2_arms")
            .help("vr_rig2_arms: how the armed grunts' and enforcers' arms sat on the authored rig since it last printed; "
                  "vr_rig2_live_check how a live one's drawn mesh follows its frames, vr_rig2_frames its frames against "
                  "the joints' limits."),
        command("Print the Ragdolls' Drawing", "vr_ragdoll_draw_info")
            .help("vr_ragdoll_draw_info: each ragdoll drawn, the renderer's data for its skinned copy beside its .mdl's "
                  "(light, occlusion group, wounds, retro textures): the values that differ."),
        toggle("Ragdolls' Weapons in Hand", vr_debug_ragdoll_gun)
            .help("vr_debug_ragdoll_gun: every 0.25 s, each ragdoll's held weapon (an enforcer's rifle, a grunt's shotgun, "
                  "a knight's sword, an ogre's chainsaw) as drawn: how far from the part holding it, beside the rest "
                  "pose's (\"gun:\"; the same: in his hand)."),
        toggle("Dead Monsters Animating", vr_debug_deadanim)
            .help("vr_debug_deadanim: each dead monster that animates again printed (its think its AI's stand, walk, run or "
                  "attack again, or its frame changing 3 s after its death: a corpse that gets up or runs in place). Needs "
                  "Developer Messages."),
        cycle("Hit Reaction Log", vr_hit_react_debug, {{0.f, "Off"}, {1.f, "Blows"}, {2.f, "Every Frame"}})
            .help("vr_hit_react_debug: each blow a living monster flinches at printed (the bone struck, its size) and the "
                  "server's slow-down of its steps; Every Frame: its bones' turns each frame too (hittrace lines)."),
        toggle("Active Ragdoll Targets", vr_ragdoll_active_debug)
            .help("vr_ragdoll_active_debug: each active ragdoll's joint targets drawn over it (green as strong as the joint "
                  "is, grey limp), its pelvis pin (yellow) and its hands' and feet's goals (red: a wound held, a step, a "
                  "crawl), what is round him: his probes (cyan, orange where they met something), his hands' goals and "
                  "holds (magenta), the wall he leans on (blue). vr_ragdoll_active_list in the console: each one's "
                  "strengths and what its death does."),
        cycle("Physics Bodies", vr_debug_box3d, {{0.f, "Off"}, {1.f, "Made and Slept"}, {2.f, "Every Awake Body"}})
            .help("Box3D bodies made, woken and put to sleep; or every awake body every frame (a lot). Also each throw: how "
                  "fast, whether it passes through your hands, and how much its velocity and spin changed by the grace's end "
                  "(with Throw Grace 0: watched 0.2 s, to compare)."),
        cycle("Rocks and Bricks Placement", vr_debug_debris, {{0.f, "Off"}, {1.f, "A Line a Map"}, {2.f, "Each Piece"}, {3.f, "Each Spot Rejected"}})
            .help("At the next map load: the pieces placed, the spots, the time; each piece; each spot rejected and why."),
        cycle("Crates Placement", vr_debug_crates, {{0.f, "Off"}, {1.f, "A Line a Map"}, {2.f, "Each Crate"}, {3.f, "Each Spot Rejected"}})
            .help("vr_debug_crates: where the crates went (and why not), each crate's clearance in front of it. Also Developer "
                  "Messages print each crate's damage and breaking, the pieces and what it held."),
        cycle("Arm IK", vr_debug_arm, {{0.f, "Off"}, {1.f, "Print Once"}, {2.f, "Trace File"}})
            .help("Each drawn arm's joints once (shoulder, elbow, wrist in the body's axes, the elbow's swing, the wrist's bend "
                  "and twist, and the swing's cost every 15 degrees: armcost); or arm_trace.txt every frame."),
        cycle("Two-Handed Grip", vr_debug_2h_grip, {{0.f, "Off"}, {1.f, "Taken and Let Go"}, {2.f, "And Where"}})
            .help("Each grip the helping hand takes (a sword's or the crowbar's: below the hand or along the blade) and why "
                  "it let go of a weapon (the check, its number and limit), the stickiness then (Aiming: 2H Grip "
                  "Stickiness), and weapons held anywhere (taken, let go, carried); And Where: also where the grip is "
                  "from it, how far along a blade it holds it, and where an empty hand is on a weapon held, carried or "
                  "lying about (its surface, handle and nearest hotspot: Weapons Anywhere)."),
        toggle("Bot Chatter", vr_verbosebots).help("The bots' thoughts, with bots in the game."),
        toggle("Graphics State", vr_debug_glstate)
            .help("Each texture bind the engine skipped, believing the texture already bound, while the graphics card had "
                  "another there (\"texcache: stale bind\"): a texture deleted without telling the engine's cache, its "
                  "name given to a new one; shows as a blank or black image after a size change. And a clip rectangle "
                  "left on at a frame's end (\"glstate:\"): the menus and console cut off in the window. And the "
                  "framebuffers bound, against the engine's record of them (\"glstate: ... framebuffers\"), at a frame's "
                  "end and at each eye's image acquire (vr_debug_glstate)."),
        header("Trace Files (game folder)"),
        cycle("Grasp Trace", vr_debug_grasp_trace, {{0.f, "Off"}, {1.f, "Main Hand"}, {2.f, "Off Hand"}, {3.f, "Both Hands"}})
            .help("grasp_trace.txt: the drawn hand's joints every frame."),
        cycle("Weight Trace", vr_debug_weight, {{0.f, "Off"}, {1.f, "File"}, {2.f, "File and Console"}})
            .help("weight_trace.txt: each hand's target and drawn pose under the weight, every frame."),
        toggle("Lean Trace", vr_debug_lean)
            .help("lean_trace.txt: the head, the body's box, the lean, the pelvis and the feet every frame, and the lean's cues."),
    };
}

// The profiler and the memory log (moved from the Debug page).
za::Vector<Item> pageDebugProfiling()
{
    return {
        header("Profiling"),
        cycle("Profiler Panel", vr_profile_overlay, {{0.f, "Off"}, {1.f, "Over the Wrist"}, {2.f, "In Front"}})
            .help("Where each frame's time goes: the game's systems (Box3D, QuakeC, the world's drawing, waiting for the "
                  "headset...), the last second's average and worst in ms, with a bar against the frame's budget; the CPU's, "
                  "then the GPU's. Showing it runs the profiler."),
        cycle("CSV Capture", vr_profile_csv, {{0.f, "Off"}, {1.f, "Recording"}})
            .help("While on, a row a second (each system's time, a column each) into quakevr/profile/systems_<date>_<time>.csv, "
                  "for a spreadsheet: turn it on, play what feels slow, turn it off."),
        cycle("Hitch Log", vr_profile_hitch, {{0.f, "Off"}, {1.5f, "Over 1.5 Frames"}, {2.f, "Over 2 Frames"}, {3.f, "Over 3 Frames"}})
            .help("While the profiler runs (the panel, a capture), frames that take this long go to the console and "
                  "quakevr/profile/hitches_<date>_<time>.csv, with what took their time."),
        command("Capture Frame (RenderDoc)", "vr_renderdoc_capture")
            .help("vr_renderdoc_capture: a RenderDoc capture of the next frame (both eyes: the desktop window's present ends "
                  "the frame), after the delay below; the console and a message show the file's path. Only when the game "
                  "runs under RenderDoc (launched from it): else the console says so. The corner's Capture Frame button "
                  "and the toolgun panel's More page do the same."),
        cycle("Capture Delay", vr_renderdoc_delay, {{0.f, "None"}, {1.f, "1 s"}, {3.f, "3 s"}})
            .help("vr_renderdoc_delay: seconds from the press to the capture, to close the menu or get into position."),
        cycle("Capture Frames", vr_renderdoc_frames, {{1.f, "1 Frame"}, {2.f, "2 Frames"}, {5.f, "5 Frames"}})
            .help("vr_renderdoc_frames: frames in the button's capture (one file)."),
        toggle("Hide Menu While Capturing", vr_renderdoc_hide_menu)
            .help("vr_renderdoc_hide_menu: the menu is closed (and the toolgun panel hidden) for the captured frame, the menu "
                  "reopened on the same page after."),
        command("RenderDoc Status", "vr_renderdoc_status")
            .help("vr_renderdoc_status: whether RenderDoc is attached, the captures taken and the last file's path."),
        toggle("OpenXR Timing Log", vr_xr_log_timing)
            .help("A line a second in quakevr/qvr_openxr.txt while VR runs: frames rendered or shown again, display periods "
                  "missed, each OpenXR call's time (xrWaitFrame, xrEndFrame, the images' acquire and release) and the "
                  "session's state (VISIBLE: the runtime's menu has the controllers)."),
        toggle("Act as if the Runtime's Menu Were Open", vr_debug_runtime_menu)
            .help("vr_debug_runtime_menu 1: the game acts as if SteamVR's dashboard (or Virtual Desktop's, Meta's menu) had "
                  "the focus, on any backend, the mock's too: a single player game pauses and its sounds fade "
                  "(Runtime Menu Pauses); the console says when it paused and resumed, and the game's time."),
        toggle("Act as if the Window Lost Focus", vr_debug_window_unfocused)
            .help("vr_debug_window_unfocused 1: the game acts as if its desktop window had lost the focus to another "
                  "program's: the menus' warning in the top right corner; in VR the mirror presented only "
                  "vr_mirror_unfocused_hz times a second (the headset's frames never slowed); on a flat screen the "
                  "background sleep."),
        cycle("Act as if the Driver Forced V-Sync", "vr_debug_mirror_vsync",
            {{0.f, "Off"}, {60.f, "60 Hz Monitor"}, {75.f, "75 Hz Monitor"}})
            .help("vr_debug_mirror_vsync 60 (any Hz in the console): in VR the desktop window's presents wait as on a "
                  "monitor of that refresh with the GPU driver forcing V-Sync (or G-SYNC on the focused window; GitHub "
                  "issue #79). With Recording > Window Frame Rate at Every Headset Frame the headset then drops to the "
                  "monitor's rate; at Up to the Monitor's (the default) it never slows. vr_debug_clock says the frame "
                  "period, the presents and the swap interval the driver reports."),
        toggle("Act as if the Window Were Minimized", vr_debug_window_hidden)
            .help("vr_debug_window_hidden 1: the game acts as if its desktop window were minimized: the Spectator "
                  "Camera is skipped (Recording > Skip While the Window Is Minimized; the console says so with "
                  "developer 1). vr_debug_window minimize, hide or show does it to the real window."),
        command("Minimize the Window", "vr_debug_window minimize")
            .help("vr_debug_window minimize: the desktop window really minimized (the taskbar brings it back; "
                  "vr_debug_window show)."),
        cycle("Simulate Held Frames", vr_debug_frame_stall_ms, {{0.f, "Off"}, {40.f, "40 ms a Frame"}, {100.f, "100 ms a Frame"}})
            .help("vr_debug_frame_stall_ms: each frame's end waits this long, as if the runtime held the frames (the "
                  "100 ms frames of a VR drop): checks the held-frames lines in quakevr/qvr_openxr.txt (the foreground "
                  "program, the session's state, the frame times), written once when it starts and once when it ends. "
                  "Turn it off at once in the headset: the view stutters."),
        cycle("Detail", vr_profile_detail, {{1.f, "Systems"}, {2.f, "Every Trace and Builtin"}})
            .help("Every Trace and Builtin also times each collision trace and each QuakeC builtin call apart: dearer, to "
                  "split QuakeC's time."),
        cycle("GPU Timing", vr_profile_gpu, {{0.f, "Off"}, {1.f, "Every Frame"}, {2.f, "One Frame in 2"}, {4.f, "One Frame in 4"}, {8.f, "One Frame in 8"}})
            .help("How often the profiler times the GPU: each timed frame stalls it a little. Off: CPU times only."),
        slider("Report Interval", vr_profile_interval, 0.f, 30.f, 1.f, "%.0f s")
            .help("Performance Profile (Graphics): seconds between its reports in the console and its CSV. 0: only on Dump Profile."),
        slider("Artificial GPU Load", vr_dynres_test_load, 0.f, 200.f, 5.f, "%.0f")
            .help("vr_dynres_test_load: extra GPU work in every pixel of each eye (it changes nothing seen), to watch "
                  "Dynamic Resolution drop the scale and raise it back (the status box's Dynamic res line). 0: none."),
        cycle("Dynamic Resolution", vr_dynres, {{0.f, "Off"}, {1.f, "Headset Only"}, {2.f, "Mock Headset Too"}})
            .help("vr_dynres: 2 runs it on the mock headset too (tests); 1, the default, only with a real headset."),
        toggle("Dynamic Resolution Log", vr_dynres_log)
            .help("vr_dynres_log: a console line at each scale change, with the eyes' GPU time and the budget."),
        cycle("GL Debug Groups", vr_gl_markers, {{0.f, "Off"}, {1.f, "On"}, {2.f, "With a Debugger"}})
            .help("vr_gl_markers: each GPU profile scope (eye L/R, depth prepass, shadow maps, post, 2D, xr submit...) "
                  "named as a GL debug group, so RenderDoc and Nsight captures show the passes. With a Debugger: only "
                  "when one (or RenderDoc, Nsight) is in the process. Their count with GL Debug Groups Report."),
        command("GL Debug Groups Report", "vr_gl_markers_report")
            .help("vr_gl_markers_report: the last frame's debug groups (console): how many were opened, the deepest "
                  "nesting, and whether the GL's group stack was back to empty at the frame's start."),
#ifdef QVR_TRACY
        command("Tracy Status", "vr_tracy_status").help("Connection, nesting, dropped zones and GPU query state."),
        command("Tracy Full Capture Preset", "vr_tracy_preset full")
            .help("Every Tracy detail on for a full capture: GPU passes, QuakeC functions, physics per edict, render "
                  "steps per model; memory events off (docs/vr-port/TRACY.md, \"Full capture\")."),
        command("Tracy Detail Off (Defaults)", "vr_tracy_preset off").help("Every Tracy detail cvar back to its default."),
        command("Tracy GPU Detail On", "vr_tracy_gpu_detail 1")
            .help("A GPU zone for every pass, per eye: depth pre-pass, world, brush entities, models, water, sky, "
                  "particles, decals, shadows, TAA, upscale, post, mirror, UI."),
        command("Tracy GPU Detail Off", "vr_tracy_gpu_detail 0"),
        command("Tracy Render Detail On", "vr_tracy_render_detail 1")
            .help("Zones for each model's draw steps (frame, light, instance, smoothing, motion): a few a model an eye."),
        command("Tracy Render Detail Off", "vr_tracy_render_detail 0"),
        command("Tracy QC Detail Functions", "vr_tracy_qc_detail 1")
            .help("A zone per QuakeC function call (no builtins): cheap enough to leave on while timing."),
        command("Tracy QC Detail + Builtins", "vr_tracy_qc_detail 2")
            .help("QuakeC functions and the costlier builtins (not the trivial math/string ones)."),
        command("Tracy QC Detail All", "vr_tracy_qc_detail 3")
            .help("Every QuakeC function and builtin call: for names and call counts; inflates QuakeC's times."),
        command("Tracy QC Detail Off", "vr_tracy_qc_detail 0"),
        command("Tracy Memory Detail On", "vr_tracy_memory_detail 1").help("Snapshot and allocation events for Zone, Hunk and Cache."),
        command("Tracy Memory Detail Off", "vr_tracy_memory_detail 0"),
        command("Tracy Physics Detail On", "vr_tracy_physics_detail 1")
            .help("A zone per server edict a tick, by its movetype: names what each costs, but inflates Server physics "
                  "by about 0.1 ms. The player's own physics is zoned always."),
        command("Tracy Physics Detail Off", "vr_tracy_physics_detail 0"),
#else
        command("Tracy Build Info", "echo Tracy requires the optional Release-Tracy build")
            .help("Build with Misc/quakevr/build_tracy.ps1 for external CPU/GPU captures. Normal builds compile Tracy out."),
#endif
        command("World Fragment Counts", "vr_world_fragments")
            .help("vr_world_fragments [passes]: over the next 16 opaque world passes, the pixels its depth pre-pass and its "
                  "shading run the shader on, and those the shading keeps, in the console (shaded/kept: the work spent on "
                  "hidden pixels; Parallax Depth Split on the Views page is the A/B)."),
        command("Log Next 3 Seconds", "vr_profile_gpu 1; vr_frame_log 270")
            .help("vr_frame_log <frames>: each frame's CPU time (its top scopes), GPU time (its top systems and the "
                  "particles') and the particles (live, within half a metre, how many views they cover, half size), in "
                  "the console and quakevr/profile/framelog_<date>_<time>.txt: press, then punch or burn; a single "
                  "event's cost. (It sets vr_profile_gpu 1: the GPU timed every frame until the game restarts.)"),
        command("Print Report", "vr_profile_report")
            .help("vr_profile_report: the last 5 seconds' table in the console (in qconsole.log with -condebug)."),
        command("Dump Profile", "vr_profile_dump")
            .help("vr_profile_dump: Performance Profile's report now, in the console and its CSV (quakevr/profile/profile_<map>_...)."),
        command("QuakeC Instructions", "profile 30")
            .help("profile 30: the 30 QuakeC functions that ran the most instructions (their own) since the last time, and "
                  "the total; then all are zeroed. Press it, do the thing, press it again. A call over 16 million is a "
                  "runaway loop error."),
        toggle("Time QuakeC Functions", vr_qcprofile)
            .help("vr_qcprofile: each server QuakeC function's time (its own, with the builtins it calls, and with its "
                  "callees), its calls, and each builtin's, for QuakeC Time Report. Costs a little while on."),
        command("QuakeC Time Report", "profile_qc 30")
            .help("profile_qc 30: the 30 QuakeC functions and builtins that took the most time a frame since the last "
                  "report (Time QuakeC Functions on), in the console; then all are zeroed. Press it, do the thing, press "
                  "it again."),
        cycle("QuakeC Loop", vr_qcexec, {{0.f, "Original"}, {1.f, "Fast (goto)"}, {2.f, "Fast (switch)"}})
            .help("vr_qcexec: the QuakeC interpreter's loop. Fast (goto), the default: the same results, errors and "
                  "instruction counts as the original, quicker (docs/vr-port/QC_VM.md). Original: the old loop, to "
                  "compare or to rule it out."),
        command("Edict Index Stats", "vr_edictindex_stats")
            .help("vr_edictindex_stats: the edict index (find() by classname and findflags() on monsters, clients, "
                  "items, lit torches and bodies step straight to the next match): its searches, rebuilds and edicts "
                  "read again since the last time."),
        command("Force Grab Search Stats", "vr_forcegrab_grid_stats")
            .help("vr_forcegrab_grid_stats: the force grab's target search (each hand, every frame, through the area "
                  "grid): the searches and the entities each tested since the last time."),
        cycle("Gun Shapes on Disk", vr_gun_pieces_cache, {{0.f, "Off"}, {1.f, "On"}, {2.f, "Check"}})
            .help("The guns' convex pieces kept on disk (cache/gunpieces) and read back the first time a gun is dropped, "
                  "picked up or holstered in a session, instead of cut again (10-85 ms each: a hitch as the first "
                  "grunt's gun drops). Check: read, then cut anyway and compared (Gun Shapes Stats counts them). Off: "
                  "cut once every session (vr_gun_pieces_cache)."),
        cycle("Gun Shapes at Load", vr_gun_pieces_ahead, {{0.f, "Off"}, {1.f, "On"}})
            .help("A map's load makes the convex pieces of every weapon it can give (lying about, its monsters' drops, "
                  "backpacks, your hands and holsters) from their files, or cuts them (on the job pool), so the first "
                  "pickup mid-game is not a hitch. Off: cut at the first pickup (vr_gun_pieces_ahead)."),
        command("Gun Shapes Stats", "vr_gun_pieces_cache_stats")
            .help("vr_gun_pieces_cache_stats: the guns' pieces this session read from the disk or cut, the files "
                  "written, and with Check on, how many were compared and differed."),
        command("Game State Hash", "vr_bench_statehash")
            .help("vr_bench_statehash: one hash of every entity's QuakeC fields (and which are in use), printed with the "
                  "server's time: the same script on two builds gives the same hash when a change left the game the same."),
        command("Server Random Seed", "sv_random_info")
            .help("sv_random_info: the seed the server's random numbers (QuakeC's random(), the monsters' turns) "
                  "started from at this map's load. sv_random_seed <that number> in the console before a map plays "
                  "its AI the same again (0, the default: the clock's, a new one each load); the client's effects "
                  "never move them."),
        command("Benchmark Capture (10 s)", "vr_bench_begin manual 10s")
            .help("vr_bench_begin manual 10s: the next 10 seconds' frame times (median, 95th and 99th percentiles, worst), "
                  "each GPU pass, the heap events and what there is, into quakevr/profile/bench/manual.json and a line in "
                  "the console (docs/vr-port/BENCHMARKS.md). Stand still and press it."),
        cycle("External Profiler Collects", vr_bench_profiler, {{0.f, "As Started"}, {1.f, "Benchmark Windows"}, {2.f, "Map Loads"}})
            .help("For a CPU profiler run on the game (VTune started paused: -start-paused): Benchmark Windows resumes its "
                  "collection for each Benchmark Capture (vr_bench_begin) alone, Map Loads for each load alone (the map "
                  "command to its first frame drawn), so gameplay and loading are profiled apart (vr_bench_profiler; "
                  "vr_profiler_collect 0|1 by hand)."),
        toggle("Layered Shadow Casters", vr_shadow_layered)
            .help("vr_shadow_layered: each shadow caster drawn once per light into all the faces it reaches (the GPU picks "
                  "the face), instead of once per face: fewer draw calls, the same shadows. Off: a face at a time (also when "
                  "the GPU lacks GL_ARB_shader_viewport_layer_array)."),
        command("Check Layered Shadows", "vr_shadow_layered_check 20")
            .help("vr_shadow_layered_check 20: this frame's shadow maps drawn both ways 20 times, timed (draw calls, CPU and "
                  "GPU ms), then read back and compared texel by texel, in the console. Stand where shadows are."),
        command("Check Shadow Caster Set-up", "vr_shadow_layered_check 20 cache")
            .help("vr_shadow_layered_check 20 cache: this frame's shadow maps drawn with each caster set up again for every "
                  "light, then once a pass over the lights (as the game draws them), 20 times each, timed, then read back "
                  "and compared texel by texel, in the console. Stand where shadows are."),
        cycle("Second Eye's Model Set-up", vr_alias_eye_cache, {{0.f, "Made Again"}, {1.f, "First Eye's"}, {2.f, "Checked"}})
            .help("vr_alias_eye_cache: each animated model's set-up that is the same for both eyes (its pose, place, light "
                  "at its origin, instance data) made in the first eye and reused by the second (First Eye's), made again "
                  "(Made Again), or made again and compared with the first eye's (Checked: see Model Set-up Stats)."),
        toggle("Button Screens' List", vr_setup_screens_cache)
            .help("vr_setup_screens_cache: the buttons with value screens over them (the calibration room's settings, the "
                  "hub's campaigns and Continue) found by a walk of every entity every half second (on), or every frame "
                  "(off, as before: 0.1-0.2 ms a frame in Arcane Dimensions' biggest maps). The same screens either way."),
        toggle("Crates' Placing: Entities First", vr_place_entities_first)
            .help("vr_place_entities_first: a place for a crate, barrel, rock or brick (at a map's start) tested against "
                  "the map's entities before its traces (on), or after them (off, as before). The same places either way; "
                  "a big map starts faster (150 ms in Arcane Dimensions' Sepulcher)."),
        cycle("Wound Painting Budget", vr_wounds_budget, {{0.f, "Off"}, {0.5f, "0.5 ms"}, {1.f, "1 ms"}, {2.f, "2 ms"}})
            .help("vr_wounds_budget: the wounds painted at most this long a frame, the rest of a burst (an explosion's, a "
                  "dismembering's: hundreds at once) painted over the next frames where they struck their bodies, all "
                  "within vr_wounds_budget_wait (0.04 s); Off: all in the frame they came, as before. Wounds (Debug > Info) "
                  "shows the queue."),
        toggle("AD Monsters' Death Assets Early", vr_ad_death_precache)
            .help("vr_ad_death_precache: outside Arcane Dimensions' maps, the first AD monster loads AD's backpack, gibs, "
                  "their sounds, footsteps, explosions and projectiles as it spawns (on: in the map's load, or at the "
                  "toolgun's spawn), or each at its first use as before (off: a 25-38 ms hitch at the first death)."),
        toggle("Keep Crates' and Rocks' Layouts", vr_place_keep)
            .help("vr_place_keep: the crates', rocks' and bricks' last layout kept and taken again when the map, its "
                  "entities and their settings are the same (on: a restart after a death loads faster, 200-250 ms in "
                  "Arcane Dimensions' big maps), or planned again at every load (off, as before). The same layout either way; "
                  "another seed or setting plans anew."),
        toggle("Load Prewarm", vr_load_prewarm)
            .help("vr_load_prewarm: made in a map's load (on), or at their first use as before (off): each model's rig data "
                  "for smooth animation (6-27 ms the first time a monster kind was seen), the reflections' world buffers, the "
                  "guns' screen anchors (10 ms the first time a gun lay near you), your skin masks and glove paint."),
        command("Model Set-up Stats", "vr_alias_eye_cache_stats")
            .help("vr_alias_eye_cache_stats: since the last time, the second eyes' models found set up by the first eye (in "
                  "full, the transform only, not at all) and, while Checked, the parts that came out different (none "
                  "expected); and the first eye's models whose transform came from the shadow pass (Shadow Caster "
                  "Set-up Shared)."),
        cycle("Cull Posed Models", vr_alias_cull_posed, {{0.f, "Off"}, {1.f, "On"}, {2.f, "Checked"}})
            .help("vr_alias_cull_posed: your body, its mirror image, the jointed hands and the ragdolls left out of each "
                  "eye's view and each shadow map face their bones' box does not reach (On), or drawn into all of them "
                  "(Off, as before), or On with each one culled skinned again on the CPU and checked to lie wholly outside "
                  "(Checked: see Model Cull Stats). The same images either way."),
        cycle("Body Depth Pre-pass", vr_body_prepass, {{0.f, "Off"}, {1.f, "On"}})
            .help("vr_body_prepass: your body and its mirror image drawn into the eye's depth first, then shaded once a "
                  "pixel (On), or shaded as drawn, the arms over the torso shaded twice (Off, as before). The same image "
                  "either way."),
        cycle("Model Depth Pre-pass", vr_alias_prepass, {{0.f, "Off"}, {1.f, "On"}})
            .help("vr_alias_prepass: the world's opaque models (monsters, props, gibs, items) drawn into the eye's depth "
                  "first, each batch, then shaded once a pixel (On), or shaded as drawn (Off, as before): crowds, props "
                  "and gore 4-23% less GPU on models at a headset's resolution. The same image either way."),
        toggle("Lazy OIT Clear", vr_oit_lazy_clear)
            .help("vr_oit_lazy_clear: the translucent pass's two buffers cleared at a view's first translucent draw, and "
                  "neither cleared nor resolved in a view with nothing translucent (on), or cleared and resolved in every "
                  "view (off, as before). The same image either way."),
        command("Lazy OIT Clear Stats", "vr_oit_stats")
            .help("vr_oit_stats: since the last time, the views with a translucent pass, how many drew something "
                  "translucent (water, slime, lava, a translucent model or particles) and how many skipped the clear."),
        command("Model Cull Stats", "vr_alias_cull_stats")
            .help("vr_alias_cull_stats: since the last time, the models drawn and culled a frame in the eyes and the "
                  "shadow faces, by model, how many of them posed, and any drawn twice in one view, in the console."),
        cycle("Shadow Caster Set-up Shared", vr_shadow_setup_share, {{0.f, "Off"}, {1.f, "On"}, {2.f, "Checked"}})
            .help("vr_shadow_setup_share: each animated shadow caster set up once for all the frame's shadow lights (its "
                  "whole instance copied into each light's draw, drawn without textures) and the first eye taking the "
                  "shadow pass's pose and transform for the same models (On); only the pose and transform shared by the "
                  "lights (Off); or On with the first eye's worked out again and compared (Checked: see Model Set-up "
                  "Stats). The same images either way."),
        toggle("Keep Light Casters", vr_shadow_caster_cache)
            .help("vr_shadow_caster_cache: each dynamic light's world shadow casters kept between frames while it stays "
                  "where they were collected for (a moving light's collected a little wider); off: collected again every "
                  "frame. The same images either way."),
        command("Check Kept Light Casters", "vr_shadow_layered_check 20 casters")
            .help("vr_shadow_layered_check 20 casters: this frame's shadow maps drawn with each dynamic light's exact world "
                  "casters, then with the wider set kept for a moving light, 20 times each, timed, then read back and "
                  "compared texel by texel, in the console. Stand where shadows are."),
        command("Light Caster Stats", "vr_shadow_caster_stats")
            .help("vr_shadow_caster_stats: since the last time, the dynamic lights' world caster sets kept and collected, "
                  "in the console."),
        toggle("Single-Pass Stereo (Experimental)", vr_stereo_single)
            .help("vr_stereo_single: the opaque world, brush models and the models away from the eyes drawn once for both "
                  "eyes (GL_OVR_multiview2) instead of once per eye; the rest is still drawn per eye. Less CPU per frame, "
                  "the same images (docs/vr-port/STEREO.md). Off with MSAA, or when the driver lacks multiview."),
        command("Single-Pass Stereo Info", "vr_stereo_single_info")
            .help("vr_stereo_single_info: whether the driver can, the frames drawn in a single pass since the last time "
                  "(and why not), the shaders made for it, in the console."),
        command("Load Times", "vr_startup_times")
            .help("vr_startup_times: where the start-up and the last map load spent their time (from the map command to its "
                  "first frame drawn: the stages, then the kinds of work across them), and every load's total."),
        infoLine(warmup::statusLine, 0),
        toggle("Cache Warm-up", vr_cache_warmup)
            .help("On: the derived-data cache is filled in the background (every installed game's models' occlusion bakes) on "
                  "one low-priority worker, paused while a map loads or a frame runs late; it carries on next session where "
                  "it stopped (vr_cache_warmup)."),
        command("Cache Info", "vr_cache_info")
            .help("vr_cache_info: each cached product (normal maps, gun pieces, hulls, occlusion, dequantised poses, rigs): its "
                  "setting, files and size on disk, and this session's reads, misses (in play: the gaps the loads didn't "
                  "prepare), writes and checks; then the warm-up's progress."),
        toggle("Ready What Can Appear", vr_probe_kinds)
            .help("On: the kinds of monster that can appear later on a map (the firing range's dispensers and dummies, monsters "
                  "waiting for a trigger) are made ready as it loads: their models, sounds and compiled hull, so their first "
                  "appearance drops no frame (15-25 ms otherwise). The next map load (vr_probe_kinds)."),
        cycle("Ready the Debug Spawner's", "vr_probe_test_spawn", {{0.f, "Off"}, {1.f, "Its Kind"}, {2.f, "Every Kind"}})
            .help("The debug spawner's monsters (vr_test_spawn, impulse 241) made ready as each map loads too: Its Kind the one "
                  "vr_test_spawn names, Every Kind all 23 (a longer load). The next map load (vr_probe_test_spawn)."),
        slider("Decoded Image Cache", vr_image_cache_mb, 0.f, 2048.f, 64.f, "%.0f MB")
            .help("Image files decoded once are kept for later loads up to this size (a texture pack's world textures and "
                  "material maps: QRP's E1M1 loads 0.5 s faster the second time); the least recently used go first. 0: none "
                  "kept (vr_image_cache_mb)."),
        toggle("Campaign Switch Keeps Models", vr_campaign_keep_models)
            .help("On: a switch of campaign (id1 to a mission pack and back) keeps the models whose files are the same in "
                  "its folders (each file their load looked for is found the same again), instead of loading every one "
                  "again (vr_campaign_keep_models)."),
        command("Kept Models Info", "vr_model_keep_info")
            .help("vr_model_keep_info: how many models the last campaign switch kept, of how many, and the time its check "
                  "took (console)."),
        command("Decoded Image Cache Info", "vr_image_cache_info")
            .help("vr_image_cache_info: the images kept, their size, and since the start how many were found there and how many "
                  "decoded (console). vr_image_cache_clear empties it."),
        toggle("Decode Textures Ahead", vr_texture_prefetch)
            .help("On: a map load decodes its textures' replacement images (their glows, the external pack's maps) on worker "
                  "threads while it uploads them in order: the same pixels, QRP's E1M1 about 0.15 s faster the first time. "
                  "Off: each decoded on the main thread as it is uploaded (vr_texture_prefetch)."),
        command("Textures Decoded Ahead Info", "vr_image_prefetch_info")
            .help("vr_image_prefetch_info: the images the last loads read ahead, how many the loads took (and decoded "
                  "themselves: no worker had come to them), how many they didn't ask for (console)."),
        header("Particles' Fill"),
        toggle("Skip Hidden Particles", vr_particle_saturate)
            .help("vr_particle_saturate: in dense effects the particles are composited in reverse order into a layer of "
                  "their own, and where it is already opaque the ones under it are skipped (the same image within a "
                  "level of rounding). Off: each blended into the scene in turn, to compare with GPU Timing on."),
        slider("Skip From Coverage", vr_particle_saturate_cover, 0.f, 50.f, 1.f, "%.0f views")
            .help("vr_particle_saturate_cover: from how many screens' worth of particles the frame uses it (it costs a "
                  "clear, a few full-screen marks and a blend an eye; it pays from about 7). 0: always."),
        slider("Skip Batches", vr_particle_saturate_batches, 1.f, 32.f, 1.f, "%.0f")
            .help("vr_particle_saturate_batches: the particles drawn in this many batches, the opaque pixels marked "
                  "after each (more: earlier skipping, more marks). 1: nothing skipped."),
        cycle("Opaque At", vr_particle_saturate_opacity, {{0.99f, "99%"}, {0.999f, "99.9%"}, {1.f, "Never (no skipping)"}})
            .help("vr_particle_saturate_opacity: how opaque a pixel must be for the particles under it to be skipped."),
        toggle("Count Wasted Particle Fill", vr_debug_particle_waste)
            .help("vr_debug_particle_waste: the particles' shader counts the fragments it shades and those that show "
                  "nothing or next to nothing (slower while on). Print with Particle Fill Waste Report."),
        command("Particle Fill Waste Report", "vr_particle_waste_report")
            .help("vr_particle_waste_report: since the last report, the particles' shaded fragments and the share that "
                  "were invisible or faint, by texture, for all of them and for the big ones (console)."),
        toggle("Freeze Particles", vr_particle_freeze)
            .help("vr_particle_freeze: the particles stop where they are (still drawn), to compare settings on the same "
                  "frame."),
        toggle("Half-Res In Densest Frames", vr_particle_halfres_dense)
            .help("vr_particle_halfres_dense: half resolution (when allowed) also in the densest frames, those Skip "
                  "Hidden Particles composites in reverse order. Off: retro particles there at full resolution, as "
                  "before."),
        toggle("Half-Res Particles Always", vr_particle_halfres_force)
            .help("vr_particle_halfres_force: the particles drawn at half resolution in every frame with some in view "
                  "(when Half-Res Heavy Particles or Half-Res Retro Particles allow it), not only in dense effects: to "
                  "compare with Freeze Particles."),
        cycle("Half-Res Blend", vr_particle_halfres_upsample,
            {{0.f, "Filtered (before)"}, {1.f, "By Depth"}, {2.f, "By Depth, Edges Shown"}})
            .help("vr_particle_halfres_upsample: how half-resolution particles are blended into the view. By Depth: each "
                  "pixel takes the half-size texels at its own distance (no smoke bled onto a crate's edge in front of "
                  "it). Edges Shown: the pixels that took texels by distance in green. Filtered: the four blended, as "
                  "before."),
        header("Server Tick"),
        toggle("Fixed 72 Hz Server Tick", host_fixedtick)
            .help("host_fixedtick: the server (monsters, physics, your hands' blows) runs in steady 1/72 s ticks at any "
                  "headset rate, the leftover time carried to the next frame. Off: the old way, a server frame once 1/72 s "
                  "had built up: 36 Hz on a 72 Hz headset, 45 at 90, 60 at 120, 48 at 144 (to compare)."),
        command("Server Tick Stats", "host_tickstats")
            .help("host_tickstats: the server's ticks since the last press (console): ticks a second, their lengths, how "
                  "many each frame ran (the last 48 frames as digits), Box3D's steps. Press it, play a while, press it again."),
        header("Threads"),
        toggle("Split Work Between Threads", vr_jobs_parallel)
            .help("The game's thread pool shares out the grasp solve, the liquids' volume, the decal atlas and the models' "
                  "occlusion bakes. Off: the calling thread does all of it (the same results, slower: to compare)."),
        toggle("Sound Mix on the Pool", snd_mixasync)
            .help("snd_mixasync: the sound's mix-ahead painted on the thread pool while the screen updates (the spatial "
                  "audio's voices too), joined before anything else touches the sound. Off: on the main thread in the "
                  "render, as before (the same samples, to compare)."),
        command("Sound Mix Stats", "snd_mixstats reset")
            .help("snd_mixstats: the pool's mixes since the last press (their time, the main thread's waits for them and "
                  "where), the underruns (the device past what was painted: a glitch) and the least mix-ahead left."),
        command("Sounds Cut Off", "snd_cutoffs")
            .help("snd_cutoffs: how many sounds still playing a new one cut off since the start (on their entity's own channel, "
                  "or for want of a free one), and how many were yours. snd_cutoffs reset zeroes them."),
        command("Log Sounds Cut Off", "snd_cutoffs log")
            .help("snd_cutoffs log: each sound cut off from now on printed to the console, with the one that cut it (again: stops)."),
        toggle("Catch Memory Use off the Main Thread", vr_zone_threadcheck)
            .help("The game crashes at once, with the culprit's stack (the crash report), when a thread other than the main one "
                  "uses the hunk, the model cache or the zone (none of them is thread-safe; vr_zone_threadcheck). For "
                  "testing: a map load's crash in its data is often one of these."),
        toggle("Evict Models at Map Start", vr_hitmodel_cachestress)
            .help("vr_hitmodel_cachestress: at the next map's start, each model's data is dropped from the model cache once "
                  "the next is loaded (what a full cache does), before precise hit detection's meshes are made on the "
                  "thread pool. They must come out the same (developer 1: the hash in the console). For testing."),
        cycle("Worker Threads", vr_jobs_threads, {{0.f, "Auto"}, {1.f, "1"}, {2.f, "2"}, {3.f, "3"}, {4.f, "4"}, {8.f, "8"}, {16.f, "16"}})
            .help("The thread pool's workers besides the main thread (Auto: the CPU's threads less one). Changed, the pool is "
                  "made again. -jobs <n> on the command line sets it from the start."),
        toggle("Physics on Threads", vr_box3d_threads)
            .help("Box3D's step (the props' physics) shared between the thread pool and the main thread. Off: the main "
                  "thread steps it alone (the same results: Box3D is deterministic whatever its threads)."),
        cycle("Prop Hulls Ahead", "vr_box3d_hulls_ahead", {{0.f, "Off"}, {1.f, "On"}, {2.f, "On, Checked"}})
            .help("vr_box3d_hulls_ahead: the hulls of the props made in a frame (a gib burst's head, gibs, brains) built side "
                  "by side on the thread pool before their bodies are made, not one after another (the same hulls). "
                  "Checked: each built again on the main thread and compared (vr_physics_hull_stats)."),
        command("Prop Hull Stats", "vr_physics_hull_stats")
            .help("vr_physics_hull_stats: the props' and actors' hulls made this session, their main thread time, the "
                  "slowest, how many were built ahead and taken (and, Checked, compared) (console)."),
        cycle("Physics Threads", vr_box3d_workers, {{0.f, "All"}, {2.f, "2"}, {3.f, "3"}, {4.f, "4"}, {6.f, "6"}, {8.f, "8"}})
            .help("With Physics on Threads: at most this many threads step Box3D, the main thread one of them (All: every "
                  "worker of the pool). Measured on a 24-core CPU: 4 is as fast as 6 or 8 up to ~500 awake bodies (and "
                  "leaves cores to the render thread and the headset's runtime); 8 gains another 20% only from ~1000. "
                  "All is slower than 8."),
        toggle("Physics AVX2", vr_box3d_avx2)
            .help("vr_box3d_avx2: Box3D's contact solver and hull contacts 8 at a time with AVX2, where the CPU has it "
                  "(the console says which at the first map). Off (the default): 4 at a time with SSE2. The same results "
                  "either way; measured in the game (an i9-13900K), AVX2 was no faster with 500 props and a little slower "
                  "with 40 (Box3D alone, a box pile on a box floor, it is 20% faster). Takes effect at the next physics step."),
        cycle("Physics Threads From", vr_box3d_threads_bodies, {{0.f, "Always"}, {100.f, "100 Bodies"}, {150.f, "150 Bodies"}, {200.f, "200 Bodies"}, {300.f, "300 Bodies"}, {500.f, "500 Bodies"}, {1000.f, "1000 Bodies"}})
            .help("With Physics on Threads: shared out only while at least this many bodies are awake (a smaller step is "
                  "faster on the main thread alone: handing it out costs more than it saves). Measured: even at ~100 awake, "
                  "4 threads 15% faster at 125, 25% at 150, 40-50% at 300-500, 2-3x at 1000."),
        command("Physics Step Time", "vr_physics_steptime")
            .help("vr_physics_steptime: Box3D's step time a frame since the last time (average, median, 95th and 99th "
                  "percentiles, worst, ms), the awake bodies and its threads. Run it, play, run it again."),
        command("Physics Step Time by Awake Bodies", "vr_physics_steptime bins")
            .help("vr_physics_steptime bins: the same, then the frames by how many bodies were awake as they began (the "
                  "median and 95th percentile of each range: where threads pay, against Physics Threads From) and Box3D's "
                  "own profile (collide, solve and its stages). Misc/quakevr/box3dmt/pilebench.py runs it over the piles."),
        command("Spawn a Big Prop Pile", "vr_physics_bigpile")
            .help("vr_physics_bigpile [count] [distance] [rocks | bricks | crates | mixed]: Pile Size rocks and bricks "
                  "ahead of you in leaning columns that topple into a pile: more awake bodies than Physics Threads From "
                  "while they fall (Physics on Threads at work). Physics Step Time before and after gives the step's "
                  "time. More piles, a wall of crates and Clear the Piles: Debug > Tests > Physics Stress."),
        command("Thread Pool Info", "vr_jobs_info")
            .help("vr_jobs_info: the pool's workers and what it has run (tasks, loops, the chunks each side took)."),
        command("Thread Pool Sites", "vr_jobs_sites")
            .help("vr_jobs_sites: each shared-out loop's calls, how many split between threads, its items, the chunks "
                  "the caller and the helpers took, and the caller's time a call (average, worst). Then Reset Sites "
                  "and play to see one scene's."),
        command("Thread Pool Sites Reset", "vr_jobs_sites reset")
            .help("vr_jobs_sites reset: every loop's counters back to zero (Thread Pool Sites counts from here)."),
        command("Ragdoll Hand Probe", "vr_ragdoll_hand_probe")
            .help("vr_ragdoll_hand_probe: each hand holding a ragdoll's limb: the limb's lag, the hand drawn off its "
                  "controller onto it, its palm and fingertips from the limb's mesh (cm), the fingers that met it; in a "
                  "tug-of-war, the drawn hand and the controller to its grab on the weapon as drawn."),
        toggle("Armed Ragdolls Keep Their Posed Arms", vr_ragdoll_posed_arms)
            .help("An armed humanoid made a ragdoll (a tug, a stagger, a death) starts with its arms where they were drawn "
                  "holding its weapon, not its animation frame's: no jump of the weapon or of a tug's hand "
                  "(vr_ragdoll_posed_arms; off: as before)."),
        toggle("Armed Humanoids: Shoulder Skin", vr_agarms_shoulder_skin)
            .help("The shoulders' skin from the real joint: the deltoid or pauldron on the arm, its blend into the body "
                  "about the joint, the shoulder's top on the clavicle (vr_agarms_shoulder_skin; read at a map load)."),
        toggle("Armed Humanoids: Shoulder Limits", vr_agarms_limits)
            .help("The arms' IK keeps each upper arm within its shoulder's cone and twist (vr_agarms_shoulder_cone, _twist); "
                  "a hand out of reach within them eases off its grip (vr_agarms_limits; off: as before)."),
        toggle("Armed Humanoids: Upper Arm Clear of the Torso", vr_agarms_arm_clear)
            .help("The upper arm's skin kept out of the torso: the elbow chosen so, else the wrist turned out about the "
                  "shoulder (vr_agarms_arm_clear; off: only the bone's line, as before)."),
        command("Armed Humanoids: Weapon Holds Report", "vr_agholds_report")
            .help("vr_agholds_report: each armed humanoid drawn lately, its weapon and its hands on it (one-handed, two: the "
                  "off hand on its second grip or cupping the other), what the Weapon Holds asked; vr_agholds_info the "
                  "settings' table."),
        toggle("Armed Humanoids: Cup Handles as Second Grips", vr_agarms_cup_handle)
            .help("A weapon's Cup hotspot well ahead of its handle (the Widowmaker's front handle) is where the off hand "
                  "holds it, as yours does (vr_agarms_cup_handle; off: every cup under and round the hand on the handle, "
                  "as before)."),
        command("Armed Humanoids: Arms' Solve Time", "vr_agarms_cost")
            .help("vr_agarms_cost: the arms' solves since the last (one armed humanoid's arms a frame): how many, us each."),
        command("Armed Monsters' Weapons: Drawn vs Server", "vr_agrunt_weapon_probe")
            .help("vr_agrunt_weapon_probe: each armed monster's held weapon as drawn (on its drawn hands) against where the "
                  "server has it (what your grabs and a tug's hold use): cm and degrees apart."),
        command("Grip Fit: Last", "vr_grip_fit_last")
            .help("vr_grip_fit_last: the last grip fit (Keep Fingers Out of Props): the drawn hand's overlap with the prop "
                  "before and after (the fingers apart), the move, its time; then the worst case timed 1000 times."),
        command("Grip Fit: Drawn Hand", "vr_grip_fit_drawn")
            .help("vr_grip_fit_drawn: each hand holding a prop, as drawn now: how many of its mesh's points are in the "
                  "prop's shape and how deep (cm), the hand and the fingers apart."),
        command("Grasp Sweep", "vr_grasp_sweep 5")
            .help("vr_grasp_sweep: each hand's grasp solved every way at 7 places, each on the pool and on one thread (the "
                  "same, to the last bit?), and the afresh solve timed both ways. Long output."),
        header("Memory"),
        cycle("Memory Log", "vr_memstats_log", {{0.f, "Off"}, {30.f, "Every 30 s"}, {60.f, "Every minute"}, {300.f, "Every 5 minutes"}})
            .help("Write memory use and the frame rate to quakevr/profile/memstats_<date>.csv, and after each map load."),
        cycle("Memory Log: GL Count", "vr_memstats_glscan", {{0.f, "Every name"}, {1.f, "Skip free names"}, {2.f, "Both, compared"}})
            .help("vr_memstats_glscan: the log's count of GL objects at each map load. Skip free names (5 ms): the names "
                  "the driver hands out as free are not asked about, the same count; every name (13 ms): each asked up "
                  "to 4096 past the last object; both: compared at every load, printed."),
        toggle("Memory Log: GPU", vr_memstats_log_gpu)
            .help("Also sample the GPU for the Memory Log (its clocks, slowdowns, and each program's use of it: SteamVR, "
                  "Virtual Desktop), on a thread of its own. Off: only while profiling (Performance Profile, the Profiler Panel or its CSV Capture)."),
        command("Print Memory Now", "vr_memstats")
            .help("vr_memstats: video and system memory, the textures and models loaded, the frame times since the last one."),
        command("Video Memory Report", "vr_vram_report diff")
            .help("vr_vram_report [diff] [csv] [all]: the game's GPU memory by category (eye targets, post effects, world, "
                  "lightmaps, model skins, text screens, wounds, buffers...) from every GL object's own size, against "
                  "Windows' count for the process and the other programs (SteamVR, Virtual Desktop). diff: what was made "
                  "and freed since the last report (a leak: a group that keeps growing). csv: every object to quakevr/profile."),
        command("Allocation Sites", "vr_alloc_sites 300")
            .help("vr_alloc_sites [frames] [lines] [peak]: the main thread's C++ and C heap events over the next 300 frames by where they "
                  "were asked for (the commonest first: a frame's, the place, its caller) in the console. To find the buffers "
                  "a frame makes and frees. Includes kinds and requested bytes. Set peak to 1 for the busiest frame's stacks. Tracing affects timings."),
        command("Heap Allocator", "vr_heap")
            .help("vr_heap: what serves malloc/free and new/delete (mimalloc; the C runtime's with -nomimalloc), the "
                  "process's working set and commit, mimalloc's reserved and committed memory, pages, threads and arenas."),
        command("Heap Statistics", "vr_heap stats")
            .help("vr_heap stats: mimalloc's own statistics table and options in the console. Long output."),
        command("Heap Stress", "vr_heap stress 8 2000")
            .help("vr_heap stress [threads] [ms]: 8 threads allocating and freeing for 2 s (a quarter freed by another "
                  "thread), each block checked: million operations a second, the heap's contention. Stalls the game."),
        cycle("Heap: Purge Delay", "vr_heap_purge_delay", {{0.f, "At once"}, {10.f, "10 ms"}, {1000.f, "1 s (mimalloc's)"}, {-1.f, "Never"}})
            .help("vr_heap_purge_delay: how long the heap (mimalloc) keeps freed memory before giving it back to the system. "
                  "At once: the smallest working set. 1 s, mimalloc's own default: map loads about 15% quicker, 0.3 to 1 GB "
                  "more memory held after them."),
        cycle("Heap: Hold During Loads", "vr_heap_load_hold", {{0.f, "Off"}, {60000.f, "On"}})
            .help("vr_heap_load_hold: a map load keeps the memory it frees for its own later allocations, then gives it all "
                  "back at its first frame drawn. Off: freed memory goes back as Purge Delay says (a big map's hitbox "
                  "build much slower: its threads queue on the system's memory calls)."),
        command("Heap: Return Free Memory", "vr_heap collect")
            .help("vr_heap collect: mimalloc returns the memory it holds unused to the system, then vr_heap."),
        header("Startup (GL safe mode)"),
        toggle("GL Safe Mode", vr_glsafe)
            .help("vr_glsafe, from the next start: the optional GL features off (persistent mapped buffers, bindless textures, "
                  "multi-bind, clip control, layered shadow casters, debug output, MSAA, the 8192 shadow atlas), each one "
                  "logged. Also -glsafe, and on by itself after a start that crashed before its first map."),
        toggle("AMD Workarounds", vr_gl_workarounds)
            .help("vr_gl_workarounds, from the next start: on AMD/ATI GPUs bindless textures off, as -nobindless (1.0.0 crashed "
                  "in AMD's driver at start-up with them). Off: every feature the driver offers."),
        command("GL Safe Mode Status", "vr_glsafe_status")
            .help("vr_glsafe_status: whether this run is in safe mode and why, each feature, and the last GL steps "
                  "(quakevr/crash/gl_startup.log has the start-up's)."),
        command("Retry the Full Renderer", "vr_glsafe_retry")
            .help("vr_glsafe_retry: the automatic safe mode (kept after a start that crashed) off from the next start."),
        header("Diagnostics"),
        command("Game Code and Folders", "vr_gamecode")
            .help("vr_gamecode: the folders the game reads from (its base dirs, game dirs and search path) and where its "
                  "game code (progs.dat) comes from: this build's, an older build's quakevr folder or not Quake VR's "
                  "(then weapons are only props). Said at each start too; a player's console dump has it."),
        toggle("Diagnostics Mode", vr_diagnostics)
            .help("vr_diagnostics, from the next start (as -diagnostics or the Diagnostics shortcut): a debug GL context and "
                  "its messages, the OpenXR debug messenger, what is said to a debugger, the console, the crash report and "
                  "the runtimes' logs, all in quakevr/diagnostics/<date>_<time>. Slower: for a bug report only."),
        command("Run Logs Status", "vr_logs_status")
            .help("vr_logs_status: this run's logs folder (quakevr/logs/<date>_<time>, every run, the last 5 kept) and its "
                  "OpenXR debug messages so far. bugreport zips them (Options > Create Bug Report)."),
        command("Diagnostics Status", "vr_diagnostics_status")
            .help("vr_diagnostics_status: whether this run is in diagnostics mode, its folder and the messages so far."),
        toggle("OpenXR Validation Layer", vr_diagnostics_xr_validation)
            .help("vr_diagnostics_xr_validation, from the next VR start, in diagnostics mode only: Khronos' core validation "
                  "layer checks every OpenXR call the game makes (openxr_validation.log in the diagnostics folder). Off: "
                  "not loaded, if the runtime behaves differently with it."),
        header("Startup (OpenXR runtime)"),
        toggle("Runtime Crash Guard", vr_xr_guard)
            .help("vr_xr_guard: a crash inside the OpenXR runtime while the game makes its eye images or stops it is caught "
                  "and logged (qvr_openxr.txt): that VR start fails, Auto tries the next runtime, else the game plays flat. "
                  "Off: the game crashes with its report."),
        toggle("OpenXR API Layers", vr_xr_api_layers)
            .help("vr_xr_api_layers, from the next VR start: off turns off every implicit OpenXR API layer installed "
                  "(overlays, toolkits, capture tools between the game and the runtime) by its own switch. "
                  "vr_xr_runtime_explain lists them, and the other programs' DLLs in the game."),
        toggle("Skip Incompatible API Layers", vr_xr_skip_incompatible_layers)
            .help("vr_xr_skip_incompatible_layers, from the next VR start: the implicit OpenXR API layers known not to "
                  "work with this OpenGL game (OBS's OpenXR mirror: Direct3D only) are turned off by their own switch, "
                  "each said in the console and qvr_openxr.txt. Off: allowed."),
    };
}

// Printed once, to the console (and the wrist log).
za::Vector<Item> pageDebugReports()
{
    return {
        header("Flatscreen Play"),
        command("Flatscreen Play Status", "vr_flatplay_status")
            .help("-flatscreenmode: the VR gameplay on the desktop screen. Whether the mode is on, its keys, and what it "
                  "stands in for (reloading off, the flashlight worn, 1 and 2 cycling the weapons collected)."),
        toggle("Flatscreen Play Print", vr_flatplay_debug)
            .help("Prints each E and V press, and where the flatscreen hands are, to the console."),
        slider("Flatscreen Hand Reach", vr_flat_reach, 0.25f, 3.f, 0.05f, "%.2f")
            .help("How far the flatscreen hands sit from the shoulders: a held gun out in front of the chest, an empty "
                  "hand at its side."),
        slider("Flatscreen Move Sway", vr_flat_sway, 0.f, 3.f, 0.05f, "%.2f")
            .help("How much the flatscreen hands lag and settle as you start, stop, jump and land (0: none). Looks only: "
                  "the shots go along the crosshair."),
        slider("Flatscreen Walk Bob", vr_flat_bob, 0.f, 3.f, 0.05f, "%.2f")
            .help("How much the flatscreen hands bob as you walk (with your speed) and breathe at rest (0: none). Looks "
                  "only."),
        slider("Flatscreen Turn Lag", vr_flat_turnlag, 0.f, 3.f, 0.05f, "%.2f")
            .help("How much the flatscreen hands trail a turn before catching up, as in modern shooters (0: locked to "
                  "the view). Looks only: the shots go along the crosshair."),
        slider("Flatscreen Turn Lag Recovery", vr_flat_turnlag_speed, 0.1f, 5.f, 0.05f, "%.2fx")
            .help("How fast the flatscreen hands catch up with a turn: higher, quicker and less lag."),
        header("The Game"),
        command("Official Campaign Status", "vr_campaign_status").help("Owned data and language readiness; active native campaign context."),
        command("Dopa Finale Text", "loc_probe $map_dopa_endtext_final").help("Preview the resolved completion text. loc_probe <key> [arguments] also checks formatted expansion strings."),
        command("Campaign File Sources", "vr_campaign_probe").help("Actual resolved sources for VR progs and colliding official maps; vr_campaign_probe <filename> checks any virtual file."),
        command("Mission Pack Status", "vr_pack_status")
            .help("Prints whether Hipnotic and Rogue are available, missing or incomplete/corrupt. Both are optional for the Quake campaign."),
        command("Headset", "vr_status").help("vr_status: the backend, the eyes' sizes, the hidden area, the head's and hands' poses."),
        command("OpenXR Runtime Choice", "vr_xr_runtime_explain")
            .help("vr_xr_runtime_explain: what VR Settings > Headset > OpenXR Runtime chooses now and why: the runtimes installed and running, the order they are tried in, the OpenXR API layers installed and the other programs' DLLs in the game (overlays). Each VR start is logged in qvr_openxr.txt in the game folder (quakevr): what was tried, what the loader loaded, what failed."),
        command("Player", "vr_dumpplayer").help("vr_dumpplayer [client]: a player's VR fields in the game (hands, weapons, hotspots)."),
        command("Models Check", "vr_model_check 1")
            .help("vr_model_check 1: every entity's model index against its model's name, and your models against the "
                  "server's; each mismatch listed (a saved game loaded wrong: buttons drawn as gibs). 0 wrong is right."),
        command("View", "vr_dumpview").help("vr_dumpview: the hands, grips, palms, fingers and every entity drawn in the view (long)."),
        command("Aim Check", "vr_aim_check")
            .help("vr_aim_check: each held gun's shots against its drawn barrel and the controller's aim (degrees: Show "
                  "Controller Laser's red, green, white), their pitches, and the wrist's bend."),
        command("Torch Lights", "vr_torch_lights_list")
            .help("vr_torch_lights_list: the torches' and flames' lights lit now (Rendering > Torch Lights): each one's "
                  "entity (a map's static one or a live one, such as the toolgun's Fire tab's), where, how far it reaches."),
        command("Flame Wobble", "vr_flame_wobble_info")
            .help("vr_flame_wobble_info: the hands' speeds, the flames swinging now (their lean and the most since the "
                  "last time), the nearest flame model and torch fire to you (where to sweep a hand)."),
        command("Chainsaw Cord Hint", "vr_chainsaw_cord_info")
            .help("vr_chainsaw_cord_info: whether you have pulled a chainsaw's cord yet (Combat > Enemy Weapons > "
                  "Chainsaw Engine > Cord Hint), the handle's dangle now and the most since the last time. "
                  "vr_chainsaw_cord_pulled 0|1 sets it, as a pull does."),
        command("Bullet Time Now", "vr_bullettime").help("vr_bullettime: starts or stops bullet time, as the gadget's button."),
        command("Bullet Time Meter", "vr_bullettime_meter")
            .help("vr_bullettime_meter [level]: prints bullet time's meter and how it fills; with a level (0 .. 1) sets it. "
                  "Debug > Bullet Time 1 prints each hit's refill."),
        command("Empty the Bullet Time Meter", "vr_bullettime_meter 0")
            .help("vr_bullettime_meter 0: the meter empty, to try earning it back by hurting enemies."),
        command("Distortion Trails", "vr_bullettime_trails_list")
            .help("vr_bullettime_trails_list: each distortion trail (its kind, places, head, age), their strength now and "
                  "the vertices made."),
        command("Slow Motion Clocks", "vr_slowmo_probe")
            .help("vr_slowmo_probe [classname | number]: the time scale, the server's, real, slowed and client's clocks, the "
                  "player's origin and velocity, the main hand's speed and lag behind the controller (and an entity's)."),
        command("Body Calibration", "vr_bodycal_print").help("vr_bodycal_print: the body calibration's state and result."),
        command("Wrists and Grips", "vr_bodycal_debug").help("vr_bodycal_debug: one line a hand, next frame: the wrist and the grip."),
        header("World and Physics"),
        command("Physics Props", "vr_physics_list").help("vr_physics_list: the props in the physics (more with Physics Bodies logged)."),
        command("Active Ragdolls", "vr_ragdoll_active_list").help("vr_ragdoll_active_list: each active ragdoll (vr_ragdoll_active): how long driven, its legs', torso's, arms' and head's strength, its pelvis pin, what its death does (stumbled, a hand on the wound, crawling); and the drive's cost a frame."),
        command("Strike Holding Hands", "vr_ragdoll_env_hit").help("vr_ragdoll_env_hit [speed]: each dying monster's hand holding on to a wall or a ledge struck as a shot strikes it: it lets go (Gibs and Corpses > Ragdoll Settings > Grab Ledges)."),
        command("Hit the Nearest Monster: Leg", "vr_test_hit 4").help("vr_test_hit <n>: the live monster nearest you struck by a shot of yours (vr_test_hit_damage, never a kill) on its body (1), head (2), an arm (3) or a leg (4): a hit reaction to watch (Gibs and Corpses > Ragdoll Settings, Hit Reactions). vr_test_hit_calm 1: no pain frames, it doesn't turn on you."),
        command("Hit the Nearest Monster: Head", "vr_test_hit 2").help("vr_test_hit 2: the live monster nearest you struck on its head (see Hit the Nearest Monster: Leg)."),
        command("Nearest Monster Guards Its Head", "vr_test_guard 1").help("vr_test_guard 1: the live monster nearest you throws its arms up before its face for 0.6 s, as when your hand swings at its head (Gibs and Corpses > Ragdoll Settings > Guard the Head; standing: Hit Reactions on Limbs; a ragdoll: its body's arms)."),
        command("Hit Reactions Now", "vr_hit_react_info").help("vr_hit_react_info: the monsters flinching now, each blow's age, part, size and whether its bones or its whole model take it."),
        command("Kill the Nearest Monster", "vr_test_kill 1").help("vr_test_kill <n>: the live monster nearest you killed by a shot of yours struck on its body (1), head (2), an arm (3) or a leg (4): an active death to watch (Gibs and Corpses > Ragdoll Settings, Active Deaths)."),
        command("Hurt the Nearest Monster", "vr_test_hurt 15").help("vr_test_hurt <damage>: the live monster nearest you takes that much from a shot of yours (15 here: half a grunt's health); earned bullet time's meter fills by it."),
        command("Cut the Nearest Monster's Leg Off", "vr_test_cut 3").help("vr_test_cut <n>: the live monster nearest you killed by a sword's cut at its left arm (1), right arm (2), left leg (3), right leg (4), both legs (5), left forearm (6), left shin (7) or head (8); n + 10: the dead one nearest you cut so. With React to Lost Limbs: he balances on one leg, holds the stump, writhes, crawls with no legs."),
        command("Cut the Nearest Monster's Arm Off", "vr_test_cut 1").help("vr_test_cut 1: the same at its left arm: his right hand to the stump."),
        command("Cut Both Its Legs Off", "vr_test_cut 5").help("vr_test_cut 5: the same at both legs: he crawls on his arms."),
        command("Ragdolls", "vr_ragdoll_list 2").help("vr_ragdoll_list [1|2]: each ragdoll (how long limp, its parts awake, where it lies; with 1 each part's mass, place and speed and the hands holding it; with 2 also how far its flames are from its limbs), and Box3D's bodies (Gibs and Corpses > Ragdoll Settings)."),
        command("Grunt's Ragdoll Rig", "vr_ragdoll_info").help("vr_ragdoll_info [model]: the rig derived from the grunt's animation: each bone's vertices, joint, pivot and limits, how well the bones fit the frames."),
        command("Smooth Animation Check", "vr_anim_smooth_check").help("vr_anim_smooth_check [model ...]: each rigged monster's frames drawn as Model Animation Smooth draws them (r_lerpmodels 3): the weights at the frames (exact), how near a frame the shape is just off it, how far mid-frame it is from Quake's blend, and how much limbs turning 30 degrees or more shrink mid-frame (Quake's blend, the spline, with the swung limbs)."),
        command("Steady Vertices Check", "vr_mdl_dequant_check").help("vr_mdl_dequant_check [model ...]: each model's vertices dequantised (Steady Model Vertices, vr_mdl_dequant): its animation runs and loops, the largest offset from the stored values (at most half a step), how much the vertices' second differences shrink (all of them; the slow parts, where the file's rounding shows), the loops the game's QuakeC plays (their seams: second differences there and inside, stored, as before and now), how far the normals turn from frame to frame, the time it takes, and the load totals."),
        command("Smooth Animation Models", "vr_anim_smooth_info").help("vr_anim_smooth_info [model [frames]]: the models drawn smoothed: the frames that follow by name or were seen, the rig's bones and how many poses each is fitted too badly in to swing."),
        command("Trace Nearest Monster's Motion", "vr_anim_smooth_trace 180").help("vr_anim_smooth_trace <frames> [<entity> | <model>]: the nearest monster's drawn animation over the next frames (here 180): how much its points' speed changes from frame to frame (median, p95, most), printed at the end. Run it with Model Animation on Quake's Blend, then Smooth, to compare (even frames: vr_fixed_frames 1)."),
        command("Pin Nearest Monster Mid-Swing", "vr_anim_smooth_pin 41 42 0.5 -1 -1 ogre").help("vr_anim_smooth_pin <frame1> <frame2> <blend> [<frame0> <frame3> [<entity> | <model>]]: a monster (here the nearest ogre: its chainsaw swing) drawn held between two frames, to compare Model Animation's settings on it; vr_anim_smooth_pin off lets it go."),
        command("Unpin Monster", "vr_anim_smooth_pin off").help("vr_anim_smooth_pin off: the pinned monster animates again."),
        slider("Limb Fit Limit", "vr_anim_smooth_rig_error", 0.25f, 5.f, 0.25f, "%.2f units").help("vr_anim_smooth_rig_error: a bone fitted to a frame worse than this (rms) isn't swung between it and its neighbours; a vertex further off its bone swings less (none at twice it)."),
        slider("Rebuild Fit Limit", "vr_anim_smooth_rebuild_error", 0.1f, 2.f, 0.1f, "%.1f units").help("vr_anim_smooth_rebuild_error: with Rebuild From Skeleton, a point further than this (rms over the frames) off its bone's fits is rebuilt less (not at twice it); a model with under half its points rebuilt is drawn as before (Smooth Animation Models: each model's counts)."),
        command("Corpse Poses", "vr_corpse_poses").help("vr_corpse_poses [1]: every dead monster and placed corpse: its model, "
                                                         "frame and where that is in its death, a ragdoll or why not; those "
                                                         "drawn mid-death (upright) are counted as not lying. 1: the counts only."),
        command("Corpses in the Physics", "vr_corpse_list").help("vr_corpse_list: each corpse's body (fixed or pushable, box or pose, mass), where it lies, and what touches it (Gibs and Corpses > Corpse Collision)."),
        command("Held Props", "vr_carry_check").help("vr_carry_check: each held prop's place and axes in the hand, drawn vs where the game has it, and the fist's gap to it (cm)."),
        command("Props in Floors", "vr_physics_sink").help("vr_physics_sink: how far each prop sinks into the floor."),
        command("Props in Walls", "vr_physics_inlevel").help("vr_physics_inlevel: how far each prop's box is inside walls, floors and doors (a prop held in both hands pushed into a wall should be at 0)."),
        command("Prop Approach", "vr_physics_approach").help("vr_physics_approach [number | classname]: how close your box gets to the first explosive box's face from 16 directions round it (the same from every side, and as close as to a wall: your half-width)."),
        command("Watch Props Entered", "vr_physics_inside 1").help("vr_physics_inside [1 | 0]: counts the frames you spend inside a solid prop's drawn shape (where none of your moves may go) and prints each time you pass into one; with no argument, the count so far (Misc/quakevr/propphase_sweep.py: the toppled-box sweep)."),
        command("Props Entered", "vr_physics_inside").help("vr_physics_inside: how many times, and frames, you were inside a solid prop's shape since the watch started, and how deep."),
        command("Weights", "vr_weight_table").help("vr_weight_table: the weapons' and props' masses (the level's props too)."),
        command("Throws by Weight", "vr_throw_table").help("vr_throw_table [hand m/s] [flick m/s]: how fast and far each "
            "weapon and prop is thrown, one hand and two, by a hard throw (8 m/s, 2 of it the wrist's flick), with "
            "Throws by Weight off and on; the least speed each hurts at, and the most spin one hand and two give it."),
        command("Ledges Ahead", "vr_climb_probe").help("vr_climb_probe: the ledges 16 to 64 units ahead of you, and why each holds or not."),
        command("Rocks and Bricks", "vr_debris_list").help("vr_debris_list: the rocks and bricks placed in this map."),
        command("Crates", "vr_crates_list").help("vr_crates_list: the crates in this map (health, resting), how many pieces lie about, and any two placed into each other."),
        command("Gear (Hidden When Dead)", "vr_gear_status").help("vr_gear_status: what of the gear is drawn (holstered guns and their parts, holster sleeves, ammo pouch, grenade pouch, flashlight and cord, wrist gadget, body) and whether the HUD is the status bar on a hand: dead, with Hide Gear When Dead, only the status bar."),
        command("Hit Detection", "vr_hitmodel_stats").help("vr_hitmodel_stats: precise hit detection's tests so far (hits, shots through a box beside the model) and their cost."),
        command("Wounds", "vr_wounds_info").help("vr_wounds_info: the wound masks in use."),
        command("Flesh Dents", "vr_gore_dents_info").help("vr_gore_dents_info: the bodies with flesh dents, their dents, the cost of placing them."),
        command("Dent the Nearest Monster: Nail", "vr_wounds_test nearest 2 20 0 6")
            .help("vr_wounds_test nearest 2 20 0 6: a nail's wound and flesh dent on the monster nearest you, from your view (Gore > Flesh Dents)."),
        command("Dent the Nearest Monster: Blade", "vr_wounds_test nearest 3 40 0 10")
            .help("vr_wounds_test nearest 3 40 0 10: a blade's long dent on it."),
        command("Dent the Nearest Monster: Blast", "vr_wounds_test nearest 4 80")
            .help("vr_wounds_test nearest 4 80: a blast's crater on its side facing you, a couple of lesser dents round it."),
        command("Bloody Hands and Washing", "vr_gore_hands_info")
            .help("vr_gore_hands_info: the blood on your hands and body (texels), the wounds kept to re-open, the wash and its re-opening, and the blood on your weapons and props."),
        command("Decals and Gore", "vr_decal_count").help("vr_decal_count: the decals and gore pieces in the world."),
        command("Permanent Decals", "vr_permdecal_info").help("vr_permdecal_info: the marks baked for good (Permanent Decals): faces laid out, pages and atlas slots used, the atlas's layers and memory, marks baked, pages given up when it was full at its most, the flushes' cost."),
        command("Particle Lighting", "vr_particle_light_report").help("vr_particle_light_report: the last frame's lit particles (Lit Particles), their mean light and colour against unlit, and the lightmap traces it took."),
        command("Model Lighting", "vr_model_ambient_show").help("vr_model_ambient_show: the six nearest entities' ambient light."),
        command("Ambient Occlusion", "vr_ao_show").help("vr_ao_show: the ambient occlusion's occluders and bake."),
        command("AO Bakes on Disk", "vr_ao_cache_info").help("vr_ao_cache_info: the models' occlusion bakes read from the disk cache (cache/ao) this session, baked, rejected and written, and the folder's files."),
        toggle("Keep AO Bakes on Disk", vr_ao_cache).help("vr_ao_cache: the models' occlusion bakes kept on disk, so a later session's loads bake nothing (off: baked at every first load; 2 in the console: baked anyway and compared with the file)."),
        header("Hands and Weapons"),
        command("Hand Rig", "vr_hand_rig_info").help("vr_hand_rig_info: the hand's rig against the compiled one."),
        command("Wrists (Bare Forearms)", "vr_body_wrist_info")
            .help("vr_body_wrist_info: per hand, the body drawn (bare or with bracers), the hand's size against the body, how "
                  "far the arm's wrist is from the hand's, where the wrist is, the forearm's direction and the wrist's bend; "
                  "the body's and the hands' light (with bare forearms the hands are lit as the body)."),
        command("Grasp Spheres", "vr_grasp_spheres").help("vr_grasp_spheres: the fingers' and the palm's contact spheres."),
        command("Grip Frames", "vr_grip_frame")
            .help("vr_grip_frame: each hand's grip frame (the palm and the grip channel) that held props are placed by."),
        command("Hotspots Check", "vr_hotspots_check").help("vr_hotspots_check: each weapon's hotspots, checked against its model."),
        command("Weapon Anchors", "vr_anchor_where").help("vr_anchor_where: where the held weapons' anchors are this frame (muzzle, ammo button and screen, magazine and well); vr_anchor_info <model> <anchor> where one binds on its model."),
        command("Sight Lines", "vr_sight_lines").help("vr_sight_lines: every weapon's sight line (Align Sights)."),
        command("Sight Check", "vr_sight_check").help("vr_sight_check: the held weapon's sights against its aim."),
        command("Wrist Gadget", "vr_gadget_info").help("vr_gadget_info: the gadget's pose (once it has been drawn) and the hologram's messages up now."),
        command("View Text Follow", "vr_view_text_status").help("vr_view_text_status: the messages in front of you: the yaw they follow now (vr_view_text_smooth) and the head's."),
        header("Other"),
        command("Limits", "vr_limits")
            .help("vr_limits: every hardcoded limit's usage against its maximum (cvars, memory, models, edicts, lights...)."),
        command("Network: Entities Sent", "vr_net_stats")
            .help("vr_net_stats [reset]: the entities in use, and for each client the entities in sight and sent in its last "
                  "datagram, their bytes and its room (1400 to a remote client), the peak, the mean, the frames that were full."),
        command("Server Rules", "vr_serverrules")
            .help("vr_serverrules: the settings the server judges by for every player (melee timing): their values, and on "
                  "a remote server's client the server's against yours."),
        command("Keyboard Hook", "vr_keyhook_status")
            .help("vr_keyhook_status: whether this game holds the desktop keyboard hook (only with the window's focus) and "
                  "the longest it went unserviced since the last report: every key press on the desktop waits for it."),
        command("Microphones", "vr_note_devices").help("vr_note_devices: the microphones Voice Notes can record from."),
        command("Save Note Context", "vr_note_context")
            .help("vr_note_context: a voice note's context saved now without a sound (quakevr/notes): the .txt (checklist "
                  "item, hands, monsters near, settings changed), the instant replay take, the console's lines, the frame times."),
        command("Note Replay Cost", "vr_note_replay_stats")
            .help("vr_note_replay_stats: the instant replay's cost a frame, its rows and memory, the console and frame "
                  "rings (Voice Notes on: vr_note_replay, vr_note_replay_seconds)."),
        command("Detail Textures", "vr_detail_list").help("vr_detail_list: each texture's detail kind (long)."),
        command("External Maps", "vr_extmaps_stats all")
            .help("vr_extmaps_stats all: each texture of the map and what it got from the external pack (Graphics: External "
                  "Maps): its picture's match, normal, specular and glow maps (long; without 'all' only the totals)."),
        command("Companion Maps", "vr_pbr_stats all")
            .help("Each world texture's source hash, generated companion coverage, and authored normal and height components."),
        command("Animated Surfaces", "vr_extmaps_frames")
            .help("vr_extmaps_frames: each animated texture of the map (the wall buttons' +0basebtn...) frame by frame: the "
                  "frame whose surface it is drawn with, its normal, specular and glow maps and detail, and whether its "
                  "frames share one surface (Animated Surfaces A/B)."),
        command("Relighting: Texture Lights", "vr_relight_lights")
            .help("vr_relight_lights: the lights the map's glowing textures get with Graphics > Relighting's settings, a line "
                  "each texture (its kind, where its glow came from: fullbright pixels or a glow image's file, its lights), "
                  "and the lights into relight_lights.txt (to compare with relight_maps.py --list-glows)."),
        command("Relighting: Status", "vr_relight_status").help("vr_relight_status: the relighting's state (a batch's maps done, each light running: its stage and process id; the progress and time left), how the map in play is lit, the light.exe found."),
        command("Relighting: Tool Lookup", "vr_relight_get_tool status").help("vr_relight_get_tool status: the light.exe found (or not), the folder Download ericw-tools writes, the pinned file (version, size, sha256), its URL and the last download's result. vr_relight_tool_dir points both lookup and download at a test folder; vr_relight_tool_url at a test server."),
        command("Relighting: Batch's Maps", "vr_relight_batch -list").help("vr_relight_batch -list: the maps Graphics > Relighting's Relight These Maps would take (Maps, Episode, Game), with their files and sizes, without relighting them."),
        command("Menu Panel Place", "vr_menu_panel").help("vr_menu_panel: where the menu panel is (MENUPANEL: moved by a hand or not, held, its middle, from the head, the way it faces, its size). vr_menu_panel reset puts it back in front of you."),
        command("Menu Panel Back in Front", "vr_menu_panel reset").help("vr_menu_panel reset: the menu panel, moved by a grip, back in front of your head, as it opens."),
        command("Menu Panels", "vr_menu_panels").help("vr_menu_panels: the menu panels open (MENUPANELS: each one's menu, page, selected row and its value as last drawn). vr_menu_panels open, close <n> or focus <n>: another opened (as the corner's New Panel), one closed or given the keys."),
        command("Open Another Menu Panel", "vr_menu_panels open").help("vr_menu_panels open: another menu panel beside this one, on this page (as the corner's New Panel; three at most, in the headset)."),
        command("Menu Rows", "menu_vr rows").help("menu_vr rows: this page's rows as drawn (MROW: row, top, label), the scroll, the section gap, and whether the laser and mouse find each row where it is drawn."),
        toggle("Menu Links: Print, Do Not Open", vr_menu_link_dryrun)
            .help("vr_menu_link_dryrun: the version box's Support on Ko-fi link (bottom right of the menus) prints its address "
                  "in the console instead of opening the desktop's browser (tests)."),
        command("Check for Updates Now", "vr_update_check_now")
            .help("vr_update_check_now: the release feed (latest.json) read now, whatever the hour's cache and Check for "
                  "Updates say, and what it found in the console; the notice above the version box follows. "
                  "vr_update_url points it at a test server, vr_update_test_version fakes the feed's version."),
        command("Update Check: Status", "vr_update_status")
            .help("vr_update_status: this game's version, the latest the feed gave (where from, how long ago, cached or "
                  "asked), the feeds, the cache file's age, and the notice shown."),
        command("OBS: Status", "vr_obs_status")
            .help("vr_obs_status: OBS's connection (Graphics > Recording > OBS): found or not and why, the address, whether "
                  "a password is set (never the password), obs-websocket's version, the recording's state and time, and "
                  "the menus' row as it reads."),
        cycle("OBS: Process Check", vr_obs_process_check, {{0.f, "Off (Always Connect)"}, {1.f, "On"}, {2.f, "Act as if Running"}})
            .help("vr_obs_process_check: On (the default): on this PC the game connects only while OBS's process "
                  "(obs64.exe) runs, and says when its WebSocket server is off. Off: always tries (a mock server's "
                  "tests). Act as if Running: the hint's test (nothing listening reads as OBS's server off)."),
        command("Main Menu Lettering", "vr_bigfont").help("vr_bigfont: which of the main menu's letters were cut from the menu pictures, and which were left out (a mod's own picture: the menu then shows the picture)."),
    };
}

// Rebuilds, reloads, debug images, test effects.
za::Vector<Item> pageDebugTools()
{
    return {
        header("Blood Feedback"),
        command("Blood Feedback Report", "vr_blood_info"),
        command("Damage Protocol Preview", "vr_blood_test packet right").help("Round-trip a 20-damage hit through the real packet reader, including existing hand feedback and Screen Tints. Cosmetic debug preview."),
        command("Preview Hit Left", "vr_blood_test left"),
        command("Preview Hit Right", "vr_blood_test right"),
        command("Preview Hit Front", "vr_blood_test front"),
        command("Preview Hit Rear", "vr_blood_test rear"),
        command("Preview Hit Above", "vr_blood_test up"),
        command("Preview Hit Below", "vr_blood_test down"),
        command("Preview Neutral Hit", "vr_blood_test neutral"),
        command("Preview Armor Hit", "vr_blood_test armor"),
        command("Clear Hit Preview", "vr_blood_test clear"),
        header("Rebuild and Reload"),
        command("Rebuild Ledge Map", "vr_ledges rebuild").help("vr_ledges rebuild: the map's ledges found again (a moment), with their count."),
        command("Reload Models", "vr_model_reload")
            .help("vr_model_reload: every model read again from its file (edited in Blender), with the hands' and collisions' caches."),
        command("Reload Hand Model", "vr_hand_reload").help("vr_hand_reload: the jointed hand (progs/hand_rig.md5mesh) read again."),
        command("Reload Glove Paint", "vr_gloves_reload")
            .help("vr_gloves_reload: the gloves' painted leather (progs/hand_rig_00_00_glove.png, _glove_height.png) read "
                  "again, as edited in an image editor (docs/vr-port/GLOVES.md). A map change reads it again too."),
        command("Reload Detail Textures", "vr_detail_reload").help("vr_detail_reload: the detail textures' settings read again, the textures rebuilt."),
        command("Reload Shaders", "vr_shader_reload")
            .help("vr_shader_reload [n]: the engine's shaders compiled again (n: QVR_SHADER_AB, 0 by default; a shader change's A/B in one run, paused, for images and timings)."),
        header("Model Previews"),
        command("Nail Pistol Art", "vr_physics_spawn viewthing reach; viewmodel progs/vr_nailpistol_preview.mdl")
            .help("The nail pistol's art before it is a weapon (Misc/quakevr/models_src/nailpistol/README.md): its preview "
                  "model floats ahead at arm's reach, at the size it would be in your hand (a viewthing given the model: "
                  "until the map changes, player.mdl is drawn as it too). The scenario nailpistol_art adds the nailgun and "
                  "the shotgun beside it."),
        command("Flare Gun Art", "vr_physics_spawn viewthing reach; viewmodel progs/vr_flaregun_preview.mdl")
            .help("The disposable double-barrelled flare gun's art before it is a weapon (Misc/quakevr/models_src/flaregun/"
                  "README.md): its preview model floats ahead at arm's reach, at the size it would be in your hand (a "
                  "viewthing given the model: until the map changes, player.mdl is drawn as it too). The scenario "
                  "flaregun_art adds the nail pistol and the shotgun beside it."),
        command("Heavy Revolver Art", "vr_physics_spawn viewthing reach; viewmodel progs/vr_revolver_preview.mdl")
            .help("The heavy revolver's art before it is a weapon (Misc/quakevr/models_src/revolver/README.md): its "
                  "preview model, six shells chambered, floats ahead at arm's reach at the size it would be in your "
                  "hand (a viewthing given the model: until the map changes, player.mdl is drawn as it too). The "
                  "scenario revolver_art adds the shotgun beside it."),
        command("Heavy Revolver Art: Swung Out", "vr_physics_spawn viewthing reach; viewmodel progs/vr_revolver_preview.mdl; viewframe 1")
            .help("The same, its cylinder swung out on its crane (the chambered shells' heads showing); viewframe 2: "
                  "the hammer cocked, viewframe 0: back to rest."),
        command("Widowmaker Art", "vr_physics_spawn viewthing reach; viewmodel progs/vr_widowmaker_preview.mdl")
            .help("AD's Widowmaker as made by hand after Vittorio's mockup (Misc/quakevr/models_src/widowmaker/README.md): its "
                  "preview model floats ahead at arm's reach, at the size it is in your hand (a viewthing given the model: "
                  "until the map changes, player.mdl is drawn as it too). The scenario widowmaker_art also puts one in "
                  "your hand."),
        command("Widowmaker: Hand-Made Model", "vr_ad_widowmaker_art 1")
            .help("vr_ad_widowmaker_art 1 (default): the Widowmaker drawn as the hand-made model (after his mockup). Reloads it."),
        command("Widowmaker: Model Made from AD's", "vr_ad_widowmaker_art 0")
            .help("vr_ad_widowmaker_art 0: the Widowmaker drawn as the one made from AD's view model as it loads, for A/B. "
                  "Reloads it."),
        header("Save to Files (game folder)"),
        command("Wound Masks", "vr_wounds_dump").help("vr_wounds_dump: each model's wound mask, to wounds/mask_<n>_<model>.png."),
        command("Decal Atlas", "vr_decal_atlas").help("vr_decal_atlas: the decals' atlas, to decal_atlas.png."),
        command("Hand Mesh", "vr_grasp_dump").help("vr_grasp_dump: the main hand as drawn, to grasp_dump.obj (for Blender)."),
        command("Gadget Screen", "vr_gadget_screen_dump").help("vr_gadget_screen_dump: the wrist gadget's screen, to screenshots/gadget_screen.png."),
        command("Eye Images (with the UI)", "vr_eyeshot 3").help("vr_eyeshot 3: both eyes as the headset shows them (the HUD panel, the menu), to eyeshots/<map>_<n>_L.png and _R.png."),
        command("Texture Checksums", "imagehash").help("imagehash: every loaded texture's checksum as the GPU holds it, to imagehash.txt, and their sum in the console (two texture packs, or TGA and PNG files, compared)."),
        command("Reflection Map", "vr_envmap_dump").help("vr_envmap_dump: the held weapon's reflection map, to envmap.tga (Weapon Reflections on)."),
        command("Weight Test", "vr_weight_test csv").help("vr_weight_test: the weight's spring on test cases (swings, wrist snaps, wrist steps: pitch, yaw and roll), tables in the console and weight_test.csv."),
        command("Heavy Handling Test", "vr_heavy_test").help("vr_heavy_test: every weapon's weight class and what it makes of it (Weapon Weights > Heavy Handling): droop in one hand and two, an abrupt wrist snap's lag with the struggle and without, the extreme snap that wrenches it out, a one-handed shot's kick and recovery."),
        command("Heavy Wobble Test", "vr_heavy_wiggle").help("vr_heavy_wiggle [slot] [motion hands]: each heavy weapon's steadiness, one-handed and two: the wrist wiggled (yaw +-30..60 degrees at 2-5 Hz, roll, pitch), flicked, a blow stopped dead: its swing over the wrist's (gain), fastest turn, swings back and forth once still (ring), degrees still off 0.3 s after (late)."),
        toggle("Ghost Hands (test aid)", vr_ghost_hands).help("vr_ghost_hands: each hand drawn see-through where your real controller is when the game draws it elsewhere (Player Appearance > Ghost Hands)."),
        slider("Ghost Hands Test Offset", vr_ghost_hands_test_offset, 0.f, 30.f, 1.f, "%.0f cm").help("vr_ghost_hands_test_offset: moves each ghost hand this far sideways, to see it apart from the drawn hand without a heavy weapon or a wall."),
        header("External Map Index (vr_mapindex.cpp)"),
        command("Map Index Stats", "maps_stats")
            .help("maps_stats: how many packages the external index holds, by type / game_mode / map_size, how many carry their own progs.dat (left out unless the setting below is on), what the fetch cost, and where its cache is."),
        command("List Maps (newest first)", "maps_list limit=20").help("maps_list [text] [type=map] [mode=singleplayer] [size=large] [sort=date|bytes|title] [limit=20] [progs=1] [oldest=1]: the index as a table, newest first (maps_info <sha256> for one package)."),
        command("List Large Maps", "maps_list type=map size=large sort=bytes limit=40").help("maps_list type=map size=large sort=bytes: the big single-player maps, largest first."),
        command("Fetch the Map Index", "maps_fetch force")
            .help("maps_fetch force: Quaddicted's index fetched again now, on its own thread (the cached copy forgotten). Nothing waits for it; maps_stats says what happened."),
        toggle("Include Packages with progs.dat", vr_maps_allow_progs)
            .help("vr_maps_allow_progs: packages that ship their own progs.dat replace the game's code, so they are left out of the list by default. They are in the index either way (maps_info shows them, maps_stats counts them)."),
        slider("Download Cache Size", vr_maps_cache_mb, 0.f, 4096.f, 64.f, "%.0f MB").extend(0.f, 65536.f)
            .help("vr_maps_cache_mb: the downloaded packages' zips (cache/maps/) kept up to this size; past it the oldest are removed (before a download, at start-up, and when this is lowered). 0: none kept once a package is installed. Installed maps are not affected."),
        slider("Largest Download", vr_maps_max_download_mb, 0.f, 4096.f, 64.f, "%.0f MB").extend(0.f, 65536.f)
            .help("vr_maps_max_download_mb: a package whose zip is over this size is refused (the index's size, and the bytes as they arrive). 0: no limit (the default). The free disk space is checked either way, before the download and before the unpacking."),
        slider("Pretend Free Disk Space", vr_maps_debug_free_mb, 0.f, 4096.f, 64.f, "%.0f MB").extend(0.f, 65536.f)
            .help("vr_maps_debug_free_mb: test aid - the map installer is told the disk has this much free (a low disk simulated: its message, before the download or the unpacking). 0: the disk's real free space."),
        command("Download Cache Usage", "maps_cache")
            .help("maps_cache [trim]: the download cache's zips, oldest first (the first removed when it is over the size above), and how much of it they use. maps_cache trim: trimmed to the size now."),
        command("Map Browser Costs", "maps_page_stats")
            .help("maps_page_stats: the Map Library page - how many times its list was built and what it cost, what a frame of the page costs, and its layout, and where its first row, Uninstall and Reinstall are drawn. The list is built when the text, a filter or the index changes, never per frame."),
        command("Clear Search's Recent List", "menu_vr recent clear")
            .help("menu_vr recent [clear]: the Search page's results opened recently (quakevr/search_recent.txt), printed with where each is drawn, or cleared."),
        command("Open the Map Browser", "maps_page")
            .help("maps_page [text]: the Map Library page (the corner's Maps button, Single Player > Map Library), with the text typed in. maps_install <sha> gets a package, maps_play <sha> starts it."),
        header("Test Effects"),
        command("Blood and Gore", "vr_gore_test").help("vr_gore_test: blood and gore 64 units ahead, as a 40 damage hit."),
        command("Gore Burst", "vr_gore_test burst").help("vr_gore_test burst: a body bursting into gibs 64 units ahead."),
        command("Gore Sounds: a Crowd's Tears", "vr_gore_sound_test tear 12 0.06")
            .help("vr_gore_sound_test tear 12 0.06: twelve limbs torn off round you in under a second: at most three at "
                  "once, the nearest (Gore Sounds log; vr_gore_sound_stats)."),
        command("Gore Sounds: Each Kind", "vr_gore_sound_test spurt; vr_gore_sound_test pour; vr_gore_sound_test choke; "
                "vr_gore_sound_test tear; vr_gore_sound_test squelch; vr_gore_sound_test step; vr_gore_sound_test slap; "
                "vr_gore_sound_test splash")
            .help("vr_gore_sound_test <kind>: one of each gore sound round you, all at once (the choke even with Chokes off)."),
        command("Gore Sounds: Counts", "vr_gore_sound_stats").help("vr_gore_sound_stats: each gore sound kind played and dropped since the map began."),
        command("Blood Pool Under You", "vr_bloodprints_test pool")
            .help("vr_bloodprints_test pool: a pool of blood under you: step out of it for the splash, the wet steps and "
                  "bloody footprints."),
        command("Bloody Feet", "vr_bloodprints_test feet")
            .help("vr_bloodprints_test feet: your feet bloodied as from a pool: walk to leave Footprints prints."),
        command("Bloody Hands", "vr_bloodprints_test hands")
            .help("vr_bloodprints_test hands: both hands soaked in blood: press them on a wall, the floor or a ledge for "
                  "handprints."),
        command("Handprint on the Wall Ahead", "vr_bloodprints_test wall 40")
            .help("vr_bloodprints_test wall [height]: a handprint, fingers up, on the wall you face, 40 units over your "
                  "feet: stand at a ledge (a step's riser, a crate-high wall) to see it wrap onto the top."),
        command("Handprint on the Prop Ahead", "vr_bloodprints_test prop 24")
            .help("vr_bloodprints_test prop [height]: a bloody palm's blood on the prop (a crate, a barrel, a box) you "
                  "face, 24 units over your feet."),
        command("Bloody Prints: Info", "vr_bloodprints_info")
            .help("vr_bloodprints_info: the prints your feet have left, your hands' blood, the prints laid since the map began."),
        command("Back-splatter: Shotgun", "vr_gore_test 30 pellets").help("vr_gore_test 30 pellets: a shotgun blast's blood 64 units ahead, its back-splatter close behind."),
        command("Blood Streams on Monster Ahead", "vr_gore_stream_test 3").help("vr_gore_stream_test 3: three bullet holes pouring blood on the monster or corpse nearest the middle of the view."),
        command("Knock Teeth Out", "impulse 198").help("impulse 198: the monster spawned last (impulse 241) punched in the face as hard as can be: its teeth fly (Gore > Knocked-out Teeth)."),
        command("Decal Stress: Splatters", "vr_decal_stress 64 48").help("vr_decal_stress [count] [size] [splatter|pool|wall|run|stack|cluster]: 64 marks laid the same way each time (tests compare two builds' images); here splatters on the floor ahead."),
        toggle("Show Permanent Decal Pages", vr_permdecal_show).help("vr_permdecal_show: the baked marks' pages drawn unlit and green, the marks on them (where the layer has texels; a mark on a door moves with it)."),
        command("Decal Stress: Pools", "vr_decal_stress 64 48 pool").help("vr_decal_stress 64 48 pool: 64 pools spreading on the floor ahead (over 6 seconds, darkening)."),
        command("Decal Stress: Wall", "vr_decal_stress 64 32 wall").help("vr_decal_stress 64 32 wall: 64 splatters on the wall ahead."),
        command("Decal Stress: Runs", "vr_decal_stress 64 16 run").help("vr_decal_stress 64 16 run: 64 runs spreading down the wall ahead (over 3 seconds)."),
        command("Decal Stress: Stacked Pools", "vr_decal_stress 1 112 stack; vr_decal_stress 64 40 cluster; vr_decal_stress 64 40 cluster").help("A big pool 96 units ahead, then 128 smaller pools piled round it (as gibs' pools piled up): every pool shows, the older under the newer, with no square holes (decal_stack_test.sh)."),
        command("Carved Marks", "vr_decal_carve_test").help("vr_decal_carve_test: where you look (a wall or the floor), a mark of each weapon carved (Carved Holes and Craters): pellets, nails, super nails, lava nails and a blast's crater."),
        command("Carved Marks: 192", "vr_decal_carve_test 64; wait; vr_decal_carve_test 64; wait; vr_decal_carve_test 64").help("vr_decal_carve_test 64 (three times): 192 pellet, nail and super nail holes in a lattice where you look, for their cost (vr_profile)."),
        command("Melee Marks", "vr_decal_carve_test melee").help("vr_decal_carve_test melee: where you look, the melee marks (Melee Marks): blunt dents of 10, 20 and 40 damage, a sword's slashes across, slanting and up, an axe's chop and a stab. Try each Carve Style on them."),
        command("Melee Marks: 128", "vr_decal_carve_test melee 64; wait; vr_decal_carve_test melee 64").help("vr_decal_carve_test melee 64 (twice): 128 dents and slashes in a lattice where you look, for their cost (vr_profile)."),
        command("Carved Marks: Look Grazing", "vr_decal_carve_test look graze 64").help("vr_decal_carve_test look graze 64: you moved to look at the last carved marks laid from 64 units, 15 degrees over their surface (look head-on: vr_decal_carve_test look head 40)."),
        command("Blood Mist", "vr_gore_mist_test").help("vr_gore_mist_test: a bleed's blood mist 64 units ahead (Gore > Blood Mist)."),
        command("Blood in Water", "vr_water_blood_test").help("vr_water_blood_test [distance] [amount]: a cloud of blood on the water or slime below a point 96 units ahead (Gore > Blood in Water)."),
        command("Blood Spray Ahead", "vr_water_blood_spray").help("vr_water_blood_spray [distance] [count]: a hit's blood 64 units ahead; under water it slows into sinking clouds (Gore > Blood in Water)."),
        command("Blood in Water: Clouds", "vr_water_blood_info").help("vr_water_blood_info: the blood clouds on the liquids now (where, how big, how strong), in the console."),
        command("Shoreline Foam Particles", "vr_water_foam_info").help("vr_water_foam_info: the shore's foam particles (Graphics > Liquids > Shoreline Foam): the map's shore segments, those near you, the last frame's spawned, how many the rates ask to be alive (capped at 600) and their cost, in the console."),
        command("Gib Blood on Hand", "vr_gore_hands_test main").help("vr_gore_hands_test main: a gib's blood on the main hand, as taking one (Gore > Bloody Hands and Washing)."),
        command("Wrist Blood from Hand", "vr_wrist_blood_test hand main").help("A wound across the hand/arm seam, starting on the main hand."),
        command("Wrist Blood from Arm", "vr_wrist_blood_test arm main").help("A wound across the hand/arm seam, starting on the main arm."),
        command("Wash a Quarter Off", "vr_gore_wash_test 0.25").help("vr_gore_wash_test 0.25: a quarter of the blood washed off your hands and body at once, as water over them (Gore > Bloody Hands and Washing); hurt, your wounds re-open after, bleeding back in (Re-open Bleed)."),
        command("Wash All Off", "vr_gore_wash_test 1").help("vr_gore_wash_test 1: all the blood washed off your hands and body at once; hurt, your wounds re-open after, bleeding back in over Re-open Bleed (watch them with Bloody Hands and Washing)."),
        command("Blood from a Blow", "vr_gore_spatter_test blow").help("vr_gore_spatter_test blow: a blow's blood thrown onto what the main hand holds, the hand and the arm (Gore > Blood on You and Your Gear)."),
        command("List Clean Weapon Skins", "vr_cleanskins").help("vr_cleanskins: the weapon skins with a clean version (a patch beside the model: progs/<model>_<skin>.clean), whether it applies to your files, how often it was applied (Gore > Clean Weapon Skins)."),
        command("Blood from a Blow on Your Prop", "vr_gore_spatter_test prop").help("vr_gore_spatter_test prop: a blow on what the main hand holds (a weapon, a box, a crate, a brick), on its side facing you: the blood on held props (Gore > Blood on You and Your Gear)."),
        command("Blood from a Blow on the Off Hand's Prop", "vr_gore_spatter_test propoff").help("vr_gore_spatter_test propoff: as Blood from a Blow on Your Prop, on what the off hand holds (the super shotgun's blood stays on it broken open and shut)."),
        command("Blood from a Chainsaw Cut", "vr_gore_spatter_test saw").help("vr_gore_spatter_test saw: a chainsaw cut's spray just ahead of the main hand."),
        command("Blood from a Close Shot", "vr_gore_spatter_test shot").help("vr_gore_spatter_test shot: a shot hitting 40 units ahead of your eyes."),
        command("Marks on Your Main Forearm", "vr_gore_spatter_test arm main").help("vr_gore_spatter_test arm main: three bleeding marks on the main forearm alone: none on the other arm (chunky or fine)."),
        command("Gib Strikes Your Hand", "vr_gore_spatter_test gib").help("vr_gore_spatter_test gib: a gib flying into your main hand."),
        command("Blood from a Swing of Your Prop", "vr_gore_spatter_test propblow").help("vr_gore_spatter_test propblow: what the main hand holds swung away from you into a monster: the far side (the one that struck) takes the blood; a box lists its sides."),
        command("Blood on Your Left Hip's Weapon", "vr_gore_spatter_test holster 2").help("vr_gore_spatter_test holster <0..5>: a hit's blood just out from a holster's weapon (2 the left hip, 3 the right): holstered weapons take blood (Holstered Weapons Too)."),
        command("A Gibbing Ahead", "vr_gore_spatter_test burst").help("vr_gore_spatter_test burst [distance] [size]: a monster gibbed 48 units ahead: the weapons and props lying near take its blood, what lies right there soaked (Things Lying Near, Gibbed Monsters' Drops)."),
        command("Log Weapon Blood as Drawn", "vr_wounds_debug 4").help("vr_wounds_debug 4: each bloody weapon drawn (in a hand, a holster, lying about, or the parts drawn in its place: the auto pump's, the open super shotgun's, the model an ammo switch morphs out of), once a frame, with its blood or without (\"wounds: drawn ... blood none\"). Debug > Wounds turns it off."),
        command("Burn Your Arms", "vr_wounds_test self 4 90 0 12").help("vr_wounds_test self 4 90 0 12: an explosion's burns on your front and the arms held before you (Gore > Your Wounds' Detail)."),
        command("Wound Your Arms", "vr_wounds_test self 1 20 4 14").help("vr_wounds_test self 1 20 4 14: a shot's bleeding wound at your arms' height, held before your chest."),
        command("Soak Your Arms", "vr_wounds_test self 9 0 0 52").help("vr_wounds_test self 9 0 0 52: wet as from water up to your chest; dries in about 25 seconds."),
        command("Test Light", "vr_light_test").help("vr_light_test: a white light 48 units ahead for 5 seconds."),
        command("Test Bullet Time Empty Notice", "vr_bullettime_meter 0; vr_bullettime").help("vr_bullettime_meter 0; vr_bullettime: bullet time pressed with its meter empty (earned bullet time: the gadget's hologram says how to refill it)."),
        command("Test Message", "vr_message_test").help("vr_message_test: a message in the gadget's hologram (once the gadget has been drawn)."),
        command("Test Console Line", "vr_message_test console Test console log line").help("vr_message_test console <text>: an engine log line, as a warning: in the notify lines only with HUD and Menus > Screens > Console Log on the HUD (vr_hud_console_log 1)."),
        command("Notify Lines Info", "vr_notify_info").help("vr_notify_info: prints the notify lines shown now, in view and in the gadget's log (which of the game's messages and the console's log pass vr_hud_console_log)."),
        cycle("Gadget Screen Readings", "vr_gadget_test_state",
            {{0.f, "Real"}, {1.f, "Low"}, {2.f, "Exhausted, Counter"}, {3.f, "Hanging, Bullet Time"}, {4.f, "Relighting"}, {5.f, "Every Item"},
             {6.f, "Arcane Dimensions Row"}})
            .help("vr_gadget_test_state: the wrist gadget's screen shows made-up readings, to see each state of its layout: low health, ammo and "
                  "stamina; no stamina with a counter's window open; hanging with bullet time running; maps being relit; every key, powerup and sigil; "
                  "Arcane Dimensions' artifacts, a timer, custom keys and debuffs (its icons need its data installed)."),
        command("Eject a Casing", "vr_shells_eject").help("vr_shells_eject: a spent casing out of the held weapon's port."),
        command("Lightning Shock", "vr_shock_test 0").help("vr_shock_test 0: the lightning gun's shock in water (the flash, the arcs over your arms and body), without the damage."),
        command("Lightning Strikes You", "vr_shock_self_test 10; vr_shock_self_info")
            .help("vr_shock_self_test [damage]: struck by a bolt of that much (10 a shambler's, 30 a lightning gun's): Quad's arcs over "
                  "your hands, arms and body (Gore > Lightning Shock > Arcs on You), without the damage. vr_shock_self_info: the arcs "
                  "on you last frame by part (console)."),
        command("Quad Damage On You", "impulse 190; vr_powerup_info")
            .help("impulse 190: Quad Damage on (an hour) or off; vr_powerup_info: its look (Body > Quad Damage Look), the glow on your "
                  "hands and body and its lights last frame (console)."),
        command("Pentagram On You", "impulse 191; vr_powerup_info")
            .help("impulse 191: the pentagram of protection on (an hour) or off: its red glow (Body > Pentagram Glow); vr_powerup_info as above."),
        command("Biosuit On You", "impulse 193; vr_powerup_info")
            .help("impulse 193: the biosuit on (an hour) or off: its green glow (Body > Biosuit Glow); vr_powerup_info as above. "
                  "With more than one power-up the glows take turns."),
        command("Electrified Water", "vr_shock_test 1").help("vr_shock_test 1 [radius] [seconds]: arcs on the water below the point 128 units ahead."),
        command("Lightning Bolt at the Nearest", "vr_shock_hit_test 30")
            .help("vr_shock_hit_test <damage>: a lightning gun's bolt from your eyes into the nearest monster or corpse: its arcs and a burn "
                  "where it strikes; it keeps Quad's arcs crawling over it (Lightning Shock), alive or dead, a ragdoll convulsing, a kill charred."),
        command("Lightning Kill the Nearest", "vr_shock_hit_test -1").help("vr_shock_hit_test -1: the same, just enough to kill it (not to gib it)."),
        command("Throw a Brick at the Nearest", "vr_dents_test 1; vr_gore_dents_info")
            .help("vr_dents_test 1: a brick thrown into the nearest monster or corpse (a thrown prop's touch): its wound and its blunt "
                  "dent; vr_gore_dents_info the dents (console). 2: a hand's blow with what you hold, 3: a pommel strike, 4: a sword "
                  "held by its blade struck pommel first into a corpse (the ragdoll strike's path), 5: the same by its handle."),
        command("Pommel Strike the Nearest Corpse", "vr_dents_test 4; vr_gore_dents_info")
            .help("vr_dents_test 4: a sword held by its blade swung pommel first into the nearest corpse or ragdoll, as the ragdoll "
                  "strike's path has it: its damage, a wound and a round blunt dent; vr_gore_dents_info the dents (console)."),
        command("Shotgun Pellets into the Nearest", "vr_dents_test 6")
            .help("vr_dents_test 6: five pellets' wounds round where you look at the nearest monster, corpse or training dummy "
                  "(no damage): their dents and blood streams (at Stream Chance; the dummy's with Dummy Dents and Dummy Blood Streams)."),
        command("Blood Streams: Where They Start", "vr_gore_streams_info")
            .help("vr_gore_streams_info: each stream pouring, how far its start is from the dented surface at its wound, and how far "
                  "the vertex nearest the hit (the binding before) is; and the worst stretch drawn since the last time (a piece's "
                  "length over its length as poured: about 1 smooth, high a ribbon), the pieces faded and broken for it, the cuts "
                  "where the wound jumped (console)."),
        command("Clear the Dummy's Dents and Streams", "vr_dummy_gore_reset")
            .help("vr_dummy_gore_reset: every training dummy's dents, blood streams and painted wounds gone at once."),
        command("Shocked Bodies", "vr_shock_info; vr_shock_ragdoll_check")
            .help("vr_shock_info: the bodies with arcs on them (kind 3 a hit's, 4 lasting; the arcs drawn; next frame: how far off the "
                  "skin they lie, bodyshock-arcs); vr_shock_ragdoll_check: each "
                  "ragdoll's shock left, its limbs' turning speed, its fastest part and its joints' stretch (console)."),
        command("Smoke Off the Bodies Near", "vr_smoulder_test 10")
            .help("vr_smoulder_test [seconds]: every monster and body within 1000 units smokes that long as a lightning bolt's burns "
                  "would (Smouldering Smoke); vr_smoulder_info: the smoking bodies and the wisps made since the last print."),
        command("Smouldering Bodies", "vr_smoulder_info")
            .help("vr_smoulder_info: the smoking bodies (lightning's smoke left, the fire's flames out in, the smoke left) and the wisps made since the last print (console)."),
        command("Mjolnir's Lightning", "impulse 215").help("impulse 215: Mjolnir in the main hand strikes its lightning now, "
                                                            "as a blow does (15 cells). In water: the shock, with its damage."),
        header("VR Calibration"),
        command("Run the Calibration Here", "vr_setup here")
            .help("vr_setup here: VR Calibration's steps (height, body, main hand) in this map, now."),
        command("Skip the Calibration Step", "vr_setup_skip").help("vr_setup_skip: its next step at once (not Body Calibration's poses)."),
        cycle("Calibration Trace", "vr_setup_debug", {{0.f, "Off"}, {1.f, "Steps"}, {2.f, "Every Frame"}})
            .help("vr_setup_debug: each step as it starts and, at the summary, how many frames the stickman showed and "
                  "was hidden since it first showed (a hidden frame printed as it happens). Every Frame: one line a frame."),
        header("Toolgun"),
        command("A Toolgun in Your Hand", "impulse 169").help("impulse 169: the toolgun in the main hand (189: the off hand)."),
        toggle("Spawn With a Toolgun", vr_debug_spawn_toolgun)
            .help("vr_debug_spawn_toolgun: each time you spawn (a new level, a respawn), a toolgun in the first empty holster "
                  "(the hips, the chest, the shoulders; right first) unless you carry one. Weapon Mode Immersive."),
        command("Toolgun Spawn Catalog", "vr_toolgun_spawnlist").help("Lists every AD spawn entry and whether it is available in this map."), // AD support
        command("Armed Humanoid Lists", "vr_toolgun_armed_list")
            .help("vr_toolgun_armed_list: the toolgun's Armed Humanoid page's lists: the kinds (the armed monsters' table's "
                  "rows) and every weapon's code and name, held two-handed (2h), one-handed (1h: it pairs) or not (-) by "
                  "each kind (console). vr_toolgun_select_armed <kind> <weapon> <weapon2> chooses a loadout."),
        toggle("Armed Tiles on the Spawn Grid", vr_toolgun_armed_tiles)
            .help("vr_toolgun_armed_tiles: the Monsters tab also lists the Armed Grunt and Armed Enforcer tiles, one a weapon "
                  "(the Armed Humanoid page makes them all)."),
        command("Trace the Chosen Spawn", "vr_toolgun_spawn_trace")
            .help("vr_toolgun_spawn_trace [entry] [frames]: the spawn panel's choice made ahead of you, its model, scale, "
                  "box and place on the server and the client printed the frame it is made and the next 4 (a size that "
                  "changes after it shows, a model the client doesn't draw), then removed (console)."),
        command("Toolgun Status", "vr_toolgun_status")
            .help("vr_toolgun_status: its hand and tool, where it aims and what it meets (its place, pinned or loose, its joints), "
                  "the spawn choice and ghost (its offsets, turn, and which hand holds it), what the physgun holds, the scale "
                  "gizmo, the joints (console)."),
        command("Unfreeze Everything", "vr_toolgun_unfreeze_all").help("vr_toolgun_unfreeze_all: every prop the physgun froze falls again."),
        command("Remove Every Joint", "vr_toolgun_unjoin_all").help("vr_toolgun_unjoin_all: the toolgun's joints, all of them."),
        command("Open or Close Its Panel", "vr_toolgun_menu")
            .help("vr_toolgun_menu: the toolgun's panel over the gun, as its B or Y (with the toolgun in a hand; shown when this "
                  "menu closes)."),
        command("Next Tool", "vr_toolgun_next").help("vr_toolgun_next: the next tool, as the gun's stick press (-1: the one before)."),
        command("Panel Widgets", "vr_toolgun_panel_dump")
            .help("vr_toolgun_panel_dump: the panel's state, where the laser meets it, and its widgets' names and places "
                  "(its 256 x 192 pixels), for vr_toolgun_mock_laser (console)."),
        command("Thumbnails", "vr_toolgun_icons")
            .help("vr_toolgun_icons: the spawn grid's thumbnails made, failed and waiting, and the time they took (console)."),
        header("Boxes (Brush Tool)"),
        command("A Box Ahead", "vr_brush_here 64")
            .help("vr_brush_here [size] [height] [texture] [distance]: a box of the map's kind on the floor ahead (stand on it, "
                  "climb it, shoot it; the toolgun's Brush tool moves, sizes, paints and removes it)."),
        command("A Ledge to Climb Ahead", "vr_brush_here 96 48 \"\" 72")
            .help("vr_brush_here 96 48: a box 48 high just ahead, its top a ledge to take with both hands and mantle onto."),
        command("Undo the Last Brush Edit", "vr_brush_undo")
            .help("vr_brush_undo: the last brush made, changed or removed in this map taken back (64 deep; the Brush tool's "
                  "panel has it too)."),
        command("Save This Map's Brushes", "vr_brush_save")
            .help("vr_brush_save [name]: every brush of this map to <game folder>/brushes/<map>.brushes (plain .map text with "
                  "a header; saved games keep their brushes themselves)."),
        command("Load This Map's Brushes", "vr_brush_load")
            .help("vr_brush_load [name] [add]: the brushes of <game folder>/brushes/<map>.brushes made, this map's own "
                  "removed first (\"add\": kept)."),
        toggle("Load Brushes with the Map", vr_brush_autoload)
            .help("vr_brush_autoload: a map started (not a saved game) gets the brushes of its brush file (Save This Map's "
                  "Brushes)."),
        toggle("Bake the Brushes' Light", vr_brush_bake)
            .help("vr_brush_bake: each brush's faces lit from the map's lights in the game (shadows, the sun, flickering lights "
                  "in their own lightstyle), a few ms a frame; off: the flat light where each face is."),
        command("Bake Every Brush Again", "vr_brush_rebake all")
            .help("vr_brush_rebake [id | all]: the brushes' light laid out and baked again (after changing the bake's settings)."),
        command("Bake a Map Copy with Its Brushes", "vr_brush_bake_map")
            .help("vr_brush_bake_map [new_name]: saves a new playable map with these brushes, lit by ericw-tools (their light and their "
                  "shadows on the world, as Relighting makes them; minutes). The console gives the new map name. Existing maps "
                  "and the scene you are editing stay unchanged; load the copy when ready."),
        command("The Lights the Bake Sees", "vr_brush_lights")
            .help("vr_brush_lights [x y z | styled]: the map's lights, sun and minimum as the brush bake reads them; near a "
                  "point, the five nearest and whether the sun is seen there (console)."),
        command("List the Boxes", "vr_brush_list")
            .help("vr_brush_list: the boxes made in this map: their entity, revision, texture, bounds and the marks on them "
                  "(console). vr_brush_text <id>: one's .map text."),
        command("Remove Every Box", "vr_brush_delete all").help("vr_brush_delete all: every box made in this map, gone."),
        command("Brush Tool Status", "vr_toolgun_brush_status")
            .help("vr_toolgun_brush_status: the Brush tool's mode, grid and texture, what it holds and its ghost (console)."),
        toggle("Shade the Boxes' Faces", vr_brush_shade)
            .help("vr_brush_shade: a box's flat light shaded by each face's way (top full, sides 0.8, bottom 0.55); off, every "
                  "face as the light where it is. For boxes made after the change."),
    };
}
