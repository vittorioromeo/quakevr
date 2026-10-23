// vr_menu_automation.inc -- Debug > Automation & Self-Tests: the debug tests meant mostly for headless agent runs
// (scripted scenarios, vr_*_test modes, self-tests, benchmarks, acceptance checks, crash tests), moved off the
// human-oriented Debug pages (ROUND22.md, "Debug > Automation & Self-Tests"). Included by vr_menu.cpp (inside its
// namespace) after pageDebugTests, whose symbols the moved rows use.

za::Vector<Item> pageDebugAutomation()
{
    return {
        header("Official Campaigns and Dawn of the Machine"),
        command("Official Triggers: Acceptance Test", "vr_mg_trigger_test 1")
            .help("Check native target timing, cancellation, lightning, fades and comfortable quake feedback. Reload afterward."),
        command("Official Triggers: Retarget Teleporter", "vr_mg_trigger_test 4")
            .help("Destructive: redirect a visible native teleporter and report the new cached destination. Reload afterward."),
        command("Official Triggers: Lightning Damage", "vr_mg_trigger_test 3")
            .help("Check native positional lightning damage, backwards traces and wetsuit protection in a clear corridor."),
        command("Machine: Hub and Rune Acceptance", "vr_mg_hub_test 1")
            .help("Destructive rune/return checks in the Machine hub. Reload afterward."),
        command("Machine: Electrode and Rune Egg", "vr_mg_hub_test 2")
            .help("Destructive authored mge2m2 puzzle test. Reload afterward."),
        command("Machine: Equipment Carry Setup", "vr_mg_hub_test 20")
            .help("Destructive: seed independent hand/holster magazines for save/carry checks. Hold both grips and reload afterward."),
        command("Machine: Progression Report", "vr_mg_hub_test 3")
            .help("Report runes, return position, final gate and VR equipment."),
        command("Machine: Walk Into the Next Exit", "vr_mg_hub_test 30")
            .help("Put you inside this map's exit (the hub: the next episode's gate, or the final gate), the real trigger "
                  "takes you on; jump presses leave the intermission. The campaign route test's step."),
        command("Machine: Die Here", "vr_mg_hub_test 36")
            .help("God mode off and a killing blow; a jump press respawns (single player: the last save loads)."),
        command("Machine: mge5m2 Trigger Route", "vr_mg_trigger_test 2")
            .help("Destructive authored rune puzzle and quake sequence on mge5m2. Uses real buttons and engine movement. Reload afterward."),
        command("Official World: Fog Report", "vr_mg_world_test 1")
            .help("Print the native campaign's authored world and player fog values."),
        command("Official World: Environment Test", "vr_mg_world_test 2")
            .help("Check native movement/fog and explode a test barrel on e5m1. Developer campaign only; reload the map afterward."),
        command("Official Monsters: Activation Test", "vr_mg_world_test 3")
            .help("Check triggered official monsters, counts and campaign flag isolation. Reload afterward."),
        command("Official Campaign: Progress Report", "vr_mg_world_test 4")
            .help("Print monster counts and both hands' persistent weapon magazines/ids."),
        command("Dopa: Ordinary Health Pickup", "vr_mg_world_test 11")
            .help("Destructive acceptance: set health40 and use a real health box; report the cap. Reload afterward."),
        command("Dopa: Megahealth Carry Setup", "vr_mg_world_test 12")
            .help("Destructive acceptance: use ordinary and megahealth boxes to check save and level carry. Reload afterward."),
        header("Arcane Dimensions"),
        command("Arcane Dimensions: Play a Cinematic Camera", "vr_ad_test 1")
            .help("In an Arcane Dimensions map: its first cinematic camera (misc_camera) plays on you, as its trigger "
                  "would start it. You are moved to its view and back when it ends (its VR presentation is to come)."),
        command("Arcane Dimensions: Cinematic Report", "vr_ad_test 2")
            .help("One adtest: line in the console: the cinematic camera's state, how you are held (move type, solid, "
                  "damage), where you are."),
        command("Arcane Dimensions: Upgrades Report", "vr_ad_test 6")
            .help("One adtest: line: the AD upgrades you have (Shadow Axe, Widowmaker, Plasma Gun) and your hands' "
                  "weapons."),
        command("Arcane Dimensions: Artifacts Test", "vr_ad_test 10; wait; wait; vr_ad_test 11")
            .help("Destructive: the new artifacts but the Nail Piercer given (AD's own touch), then their effects "
                  "measured: the Sharp Shooter's spreads, a blast with and without the Blast Belt (it hurts), the Lava "
                  "Shield, the Air Tank's air, the Jump Boots' jumps (two adrules: lines)."),
        command("Arcane Dimensions: Nail Piercer Test", "vr_ad_test 12")
            .help("Destructive: the Nail Piercer given, two grunts in a line ahead and a nail through the first into the "
                  "second (Arcane Dimensions: Nail Piercer Result reports them)."),
        command("Arcane Dimensions: Nail Piercer Result", "vr_ad_test 13"),
        command("Arcane Dimensions: Burning, Poison, Bleeding", "vr_ad_test 14; wait; wait; vr_ad_test 16; wait; wait; vr_ad_test 18")
            .help("Destructive: AD's three debuffs on you at once (its burning is your fire's: our flames, its time and "
                  "damage); the wrist gadget shows them. Debuffs Report says what they took."),
        command("Arcane Dimensions: Debuffs Report", "vr_ad_test 15; wait; wait; vr_ad_test 17; wait; wait; vr_ad_test 19"),
        command("Arcane Dimensions: Shotgun Hit Test", "vr_ad_test 40") // AD support
            .help("Destructive: freezes an AD grunt 128 units ahead, fires six projectile pellets and reports damage after flight."),
        command("Arcane Dimensions: Super Shotgun Hit Test", "vr_ad_test 41") // AD support
            .help("Destructive: the same frozen grunt test, with fourteen projectile pellets."),
        command("Arcane Dimensions: Burst Rifle Hit Test", "vr_ad_test 42") // AD support
            .help("Destructive: the same frozen grunt test, with one burst rifle projectile round."),
        command("Arcane Dimensions: Ammo Model Test", "vr_ad_test 44") // AD support
            .help("Gives a shotgun and refreshes ammo as AD pickups do; reports the weapon id and model before/after."),
        command("Arcane Dimensions: Ammo Resistance Test", "vr_ad_test 21")
            .help("Destructive: a grunt given AD's resistance keys (shells half, nails all, rockets -50%) hit by a shot, a "
                  "nail, a blow, a rocket, a bolt: in AD maps it resists, elsewhere only AD's own monsters do."),
        command("Arcane Dimensions: Evil Skill Report", "vr_ad_test 22")
            .help("Whether AD's Evil skill plays (Nightmare with Evil Skill on), your health and its cap, megahealth's, "
                  "the axe's blow."),
        command("Arcane Dimensions: Buttons Test", "vr_ad_test 61")
            .help("Press this map's laser button with a hand, a weapon and a thrown prop; check disabled state."),
        command("Arcane Dimensions: Backpacks Test", "vr_ad_test 62")
            .help("Report map backpacks; create an AD dropped bag, test walkover immunity and grab/holster collection."),
        command("Arcane Dimensions: Ladder Movement Test", "vr_ad_test 63")
            .help("Verify tracked hands bypass AD jump/velocity ladder movement only while Hand Climbing and Hand-Climbed "
                  "Ladders are both on (either off: AD's own climb lifts you)."),
        command("Arcane Dimensions: Flat Ladder Test", "vr_ad_test 64")
            .help("Make the nearest wall west of you a ladder with no rungs: grip it anywhere to climb (the fallback for "
                  "flat ladder walls)."),
        command("Arcane Dimensions: Ladder Rung Probes", "vr_climb_try -417 -2387 -70 main; vr_climb_try -417 -2387 -60 main")
            .help("Stand beside ad_crucial's first ladder: report a real rung and the gap above it, using your current grip tolerances."),
        command("Arcane Dimensions: Ladders Report", "vr_ad_test 24")
            .help("This map's trigger_ladders and the first one's volume (climbable by hand: Hand-Climbed Ladders)."),
        command("Arcane Dimensions: Spawnflags and Sight Report", "vr_ad_test 26; wait; wait; vr_ad_test 27")
            .help("Whose meaning this map's spawnflags have (Quake's, Machine Games', Arcane Dimensions'), and where "
                  "monsters look for you (crouched: your real eyes)."),
        header("Scripted Test Pages"),
        open("Stealth AI", pageIndex(pageStealthTests))
            .help("The stealth AI's scenes on e1m1 (Combat > Stealth AI): seeing, hearing, touch, investigating."),
        open("Machine Horde Tests", pageIndex(pageMachineHordeTests))
            .help("Authored waves, currency, physical rewards, revival and saved equipment. Developer arena only."),
        open("Dawn of the Machine Tests", pageIndex(pageMg3Tests))
            .help("MG3 native port: state, saved upgrades and capacities. Dawn of the Machine campaign only."),
        open("Dawn of the Machine: Shub", pageIndex(pageMg3ShubTests))
            .help("Dawn of the Machine's Shub-Niggurath (any campaign with its data; boss2's own): her phases, children and death."),
        open("Dawn of the Machine: Chthon", pageIndex(pageMg3ChthonTests))
            .help("Dawn of the Machine's Chthon in any campaign with its data, and its boss map's fight."),
        header("Spatial Audio"),
        command("Spatial Audio Tests", "vr_snd_test all")
            .help("vr_snd_test: offline renders through the spatial audio (a sound circling the head, behind a wall, a door "
                  "moving, a small room and a hall, a rocket passing, a sound at the ear, the hands' sounds), measured, a "
                  "line each and PASS or FAIL; and the cost of 32 voices. The renders go to the game folder's sound_tests (test_*.wav)."),
        command("Spatial Audio Benchmark (12 s)", "vr_snd_bench 12 menu 8 400")
            .help("vr_snd_bench <seconds> [label] [sounds a second] [orbit units/s]: for 12 s, monsters', weapons' and "
                  "explosions' sounds round you (8 a second) while the listener goes round a circle (every voice moving "
                  "against it); then each stage of the mix's time a frame (median, 95th and 99th percentiles, worst, ms per "
                  "second of sound), the simulations' runs and the sounds' memory, in the console and sound_tests/bench.csv. "
                  "32 Sounds Around You first for more voices (ROUND21.md, \"Spatial audio: optimised\")."),
        command("Record the Mix (2 s)", "vr_snd_capture 2 menu")
            .help("vr_snd_capture: the next 2 seconds of the final mix to the game folder's sound_tests (capture_menu.wav), and its levels "
                  "in the console."),
        command("Record the Game-Time Mix (2 s)", "vr_snd_capture_game 2 menu")
            .help("vr_snd_capture_game: the next 2 game seconds of the effects' mix as at normal speed (in slow motion: "
                  "what the slowed sound sped up is; no music) to sound_tests (capture_game_menu.wav), and its levels."),
        command("Save a Sound's Two Copies", "vr_snd_dump weapons/r_exp3.wav")
            .help("vr_snd_dump <sample>: a loaded sound as Quake holds it (each sample repeated up to the mix's rate) and "
                  "its band-limited copy (what Full-Band Sound plays), to sound_tests (dump_*_held.wav, _full.wav)."),
        command("Save the Sound Scene", "vr_snd_scene_obj")
            .help("vr_snd_scene_obj: the map as Steam Audio sees it, to the game folder's sound_tests (scene.obj)."),
        header("Parry"),
        command("Dragon Parry (Mock Crowbar)",
            "vr_mock_hand main 0.15 1.25 -0.4 0 90 0;+grabright;vr_mock_button main grip 1;wait;impulse 167;"
            "wait;wait;wait;wait;wait;wait;wait;wait;wait;wait;vr_physics_spawn VR_Parry_DragonTest 120;wait;wait;"
            "-grabright;vr_mock_button main grip 0")
            .help("The mock hands only (no headset): the Dragon Parry Sequence with the main hand gripping a crowbar level "
                  "across in front (vr_mock_hand main 0.15 1.25 -0.4 0 90 0), then letting go. Developer 1 prints "
                  "\"parry: monster_dragon with hand 1\" and \"interrupted 1\"."),
        command("Dragon Tail Swing (Pose Check)", "vr_debug_pose_check 1;vr_physics_spawn VR_Parry_DragonSwing 120")
            .help("Developer 1: a dragon's own tail swing, its slash drawn, landing 1 s on; hold a guard to parry it. "
                  "Monster Poses logging on: a body drawn squashed between the attack and the pain pose is logged."),
        command("Same-Frame Parry Hits", "vr_physics_spawn VR_Parry_SameCallbackTest 48")
            .help("Developer 1: a knight deals three 10-damage blows in one callback and checks the restored self and vectors. Grants 500 health. A successful parry should cancel the last two."),
        header("Reloading Scenarios and Self-Test"),
        command("Magazine Reinsertion Boundaries", "vr_reload_test 28; impulse 125")
            .help("Tests held and loose magazine withdrawal thresholds, zero and negative clearance, both guns and eject cooldown. "
                  "Changes the weapons and ammo in your hands; use a test map."),
        command("Run the Self-Test", "vr_reload_test 9; impulse 125")
            .help("Takes, loads, refunds and drops in turn and checks every count: reload: PASS or FAIL lines."),
        command("Toss a Round Into the Off Hand's Gun", "vr_reload_test 10; impulse 125")
            .help("A loose round for it (a shell, the super shotgun's pair, its magazine), lying right, thrown into its "
                  "opening from 8 units out: it goes in by contact (Load Loose Rounds)."),
        command("Lay a Round Sideways at Its Opening", "vr_reload_test 11; impulse 125")
            .help("The same lying sideways right at the opening: it must not go in."),
        command("Lay a Round Under Its Load Point", "vr_reload_test 12; impulse 125")
            .help("On the floor (a shell flat along the gun, a magazine upright): bring the gun down onto it."),
        command("Drop a Round From Above Its Load Point", "vr_reload_test 13; impulse 125")
            .help("From 8 units straight above: turn the gun's opening up first."),
        command("Toss a Round in the Wrong Way Round", "vr_reload_test 16; impulse 125")
            .help("A launcher's round thrown into the off hand's muzzle nose first: it must not go in (a proximity "
                  "grenade does)."),
        command("Hold the Round at the Load Point", "vr_reload_test 17; impulse 125")
            .help("The main hand's round put at the off hand's gun's load point lying the way it goes in (a launcher's: "
                  "butt first): it goes in."),
        command("Hold It There the Wrong Way Round", "vr_reload_test 18; impulse 125")
            .help("The same nose first: a dull tap, it stays in the hand (a proximity grenade goes in)."),
        command("Break the Off Hand's Super Shotgun Open", "vr_reload_test 15; impulse 125")
            .help("As the flick does (Immersive, Break Open on): its barrels take loose pairs."),
        command("Empty and Break Open the Main Hand's Gun", "vr_reload_test 30; impulse 125")
            .help("Pouch reloading with two guns: the main hand's magazine emptied into the reserve and, a super shotgun, "
                  "broken open (the off hand's: Empty the Off Hand's Gun, Break the Off Hand's Super Shotgun Open). "
                  "Then bring each to the ammo pouch."),
        command("Spent Lava Nails or Plasma in the Off Hand's Gun", "vr_reload_test 16; impulse 125")
            .help("The off hand's nailgun, super nailgun or thunderbolt on its other ammo (lava nails, plasma: 100 given), "
                  "its magazine emptied as if fired dry: eject it to see a spent one smoke."),
        command("Toss a Round Into the Lying Gun", "vr_reload_test 21; impulse 125")
            .help("A loose round for the nearest gun lying about, tossed into its opening lying right: it goes in, seen "
                  "sliding in (a shell, a pair, a launcher's round)."),
        command("Drop a Round Onto the Lying Gun", "vr_reload_test 26; impulse 125")
            .help("The same let go of 6 units above its load point, lying right: it falls in where the opening is up "
                  "enough, seen sliding in."),
        command("Toss One Sideways at the Lying Gun", "vr_reload_test 24; impulse 125")
            .help("The same lying sideways a unit out of its opening: it stays out."),
        command("Hold a Round at the Lying Gun", "vr_reload_test 22; impulse 125")
            .help("The main hand takes a round from the pouch (hold the grip), then, again, it is put at the nearest lying "
                  "gun's load point: it goes in."),
        command("The Lying Gun's Magazine Is Solid", "vr_reload_test 27; impulse 125")
            .help("The nearest gun lying about with a magazine in: how far its body is from a point inside its magazine "
                  "(0: the magazine is part of it, Solid Magazines; off, the gun's own body further off)."),
        command("The Held Prop's Shape Against Its Box", "vr_reload_test 25; impulse 125")
            .help("The main hand's prop: how far its shape (as held) and its box are from the off hand's gun's magazine "
                  "and the front of its barrels. The magazine's bump and the super shotgun's hits count by its shape."),
        command("Print the Collision Shapes", "vr_physics_shapes vr_ammo_shell vr_ammo_mag vr_ammo_front thrown_weapon")
            .help("Each loose round's body against its drawn size, each held gun's body against the drawn gun, how deep "
                  "its load point lies inside it, and its convex pieces (how many, how far their hulls lie off the gun; "
                  "one hull's for comparison); the guns lying about too (the console)."),
        header("Weapon Checks"),
        command("Take the Nearest Weapon Pickup", "+grabmain; wait; wait; vr_pickup_test 2; wait; wait; -grabmain")
            .help("vr_pickup_test 2, the main grip held (+grabmain): the weapon pickup nearest you taken into an empty "
                  "hand, as a grip takes it: the hand's weapon, its magazine and the ammo left printed."),
        command("Damage Credit of Every Weapon", "vr_dmgcredit_test 1")
            .help("vr_dmgcredit_test 1: every weapon (the mission packs' and AD's when there) fired from your main hand at a "
                  "target ahead that takes only one weapon's damage (as Honey's weapon-specific buttons): the weapon each "
                  "shot is credited with printed (dmgcredit: ... PASS when its own); about 6 s a weapon."),
        command("Armed Grunts: Every Weapon", "vr_agrunt_test 1")
            .help("vr_agrunt_test 1 (an open map: vrtesthall): an armed grunt with each weapon in turn ahead of you, "
                  "hunting you for 5 s (you are kept alive): its shots, blows, damage to you, clip, reloads printed "
                  "(agtest: ... PASS when it hurt you). 100+i: weapon i alone."),
        command("Armed Grunts: Reload, Disarm, Re-Arm, Drop", "vr_agrunt_test 2")
            .help("vr_agrunt_test 2: a shotgun grunt fires its clip, reloads, is struck on its gun hand (disarmed), "
                  "goes back for its gun and is killed: the gun lies there with its rounds (agtest: ... PASS)."),
        command("Armed Grunts: Spawn a Row (Guns)", "vr_agrunt_test 7")
            .help("vr_agrunt_test 6-9: four armed grunts in a row ahead of you (6 melee weapons, 7 and 8 guns, 9 the rest); "
                  "notarget to look at them. vr_agrunt_test 4 prints every armed grunt's weapon, clip and reserve."),
        command("Armed Grunts: Spawn a Row (Melee)", "vr_agrunt_test 6"),
        command("Armed Grunts: Print Their State", "vr_agrunt_test 4"),
        toggle("Armed Grunts: Log", vr_agrunt_debug)
            .help("Prints each armed grunt's weapon, shots, reloads, disarms and pickups (developer 1; vr_agrunt_debug)."),
        toggle("Load Stuck", vr_grapple_test_stuck)
            .help("What hangs on the rope (a loose hook, a hooked prop) stays where it is, as if snagged: walk away and the "
                  "rope should hold you back."),
        header("Player Hitbox"),
        command("Hitbox Keep Test", "vr_hull_keeptest")
            .help("Builds the map's brushes and compiled hulls again from scratch and prints whether the server's (kept "
                  "from the last load, or built with this one) are the same (vr_hull_keeptest)."),
        command("Hitbox Approach", "vr_hull_approach").help("Prints how close your box gets to what is round you, in 8 directions (from your centre to the surface it stops at; Quake's box: 16 units). vr_hull_approach <classname> [n] does it round an entity."),
        command("Hitbox Bench", "vr_hull_bench").help("Times 20000 random moves (Quake's hull against the brush sweep "
                                                      "and the compiled hull) and prints where they disagree."),
        command("Shots Hit Test", "vr_hull_hittest")
            .help("Prints how many of 1600 grunt-like shots from 300 units round you hit your box at Width Shots Hit "
                  "(vr_hull_hittest [distance] [spread])."),
        command("Hitbox Leaf", "vr_hull_leafdebug")
            .help("Compiles the hull for your box again and describes the leaf you stand in when it is solid (the piece "
                  "of brush that fills it, and that brush): for a wall where there is none (vr_hull_leafdebug <entity>: "
                  "a monster's, with Narrower Monsters on)."),
        command("Hitbox Probe", "vr_hull_probe")
            .help("Prints which of the map's brushes your box is in, and by how much (when you're stuck); vr_hull_probe <entity>: a monster's."),
        command("Shots Hit Test", "vr_hull_hittest")
            .help("Prints how many of 1600 grunt-like shots from 300 units round you hit your box at Width Shots Hit "
                  "(vr_hull_hittest [distance] [spread])."),
        command("Random Walk (60 s)", "god; notarget; vr_hull_walktest 60")
            .help("Walks you around the map at random for 60 seconds (hopping somewhere new every few), then prints how "
                  "often you got stuck or ended up in a wall."),
        header("Teleporters"),
        command("Looking Through A Gate, Or Why Not", "map start; wait120; setpos 232 400 24 0 90 0; wait30; vr_portals_view")
            .help("vr_portals_view: which gate this frame looks through, that gate's box on your screen, and for every "
                  "side in the map why it is not looked through - behind its plane, not in your view, further than the "
                  "range, seen through another gate - or that it is, with its distance and score. It shares the test "
                  "the engine acts on, so the reasons printed are the ones applied."),
        command("Torch Light Views", "vr_portals_lightviews")
            .help("vr_portals_lightviews [x y z]: the views whose torches and flames light this frame - your own, each "
                  "gate's in front of you and the gates seen within those, as deep as Teleporters Within Teleporters - each with its "
                  "eye carried through, and what making them costs. With a point: how far a torch there counts as "
                  "(none: no view sees it)."),
        command("The View Through A Gate, Read Back", "vr_portals_shot")
            .help("vr_portals_shot: the next view drawn through a gate read back from its own targets - its side, the "
                  "eye and the camera it was drawn from, its brightness over the whole view and over the gate's box - "
                  "to portalshots/<map>_<n>.png and its float scene to .pfm, to compare with the eye's own float scene "
                  "(vr_eyeshot 2: eyeshots/<map>_<n>_L.pfm) - in the same frame, the gate's face over the same pixels."),
        command("Against A Gate's Frame", "map start; wait120; setpos 200 1372 24 0 90 0; wait30; vr_portals_info")
            .help("You against the wall beside the first gate's opening (12 units short of its plane): nothing "
                  "teleports you and there is no jump - vr_portals_info says his box reaches 12, that is, stopped."),
        command("In The Opening, Short Of The Plane", "map start; wait120; setpos 232 1372 24 0 90 0; wait30; vr_portals_info")
            .help("You in the first gate's opening, 12 units in front of its plane: not carried yet (his box reaches "
                  "-1: nothing stops you, walk on and you go through)."),
        command("Lean Into A Gate", "map start; wait120; setpos 232 1365 24 0 90 0; wait30; vr_mock_hand head 0 0 -0.5 45 0 0; wait30; vr_portals_info")
            .help("You 19 units from the first gate's plane, the head leaned half a metre through it: no crossing - "
                  "vr_portals_info shows the head past the plane and the torso short of it."),
        command("Through A Gate", "map start; wait120; setpos 232 1330 24 0 90 0; wait10; noclip 0; wait80; vr_mock_stick off 0 0.5; wait20; +jump; wait30; -jump; vr_mock_stick off 0 0; wait30; vr_portals_info")
            .help("Mock movement with collision enabled: approach the first gate and jump into its opening. "
                  "The torso reaches y=1384 before crossing (developer 1: VR portal: carried edict 1 through side 0)."),
        command("Frame Strip Through A Gate", "map start; wait120; setpos 544 1330 24 0 90 0; wait10; noclip 0; wait80; vr_mock_stick off 0 0.5; wait20; +jump; wait36; -jump; vr_screenshot_frames 12; wait20; vr_mock_stick off 0 0")
            .help("The same jump into the middle gate (Normal skill), a screenshot of each of the 12 frames round the "
                  "crossing (vr_screenshot_frames 12: every frame drawn, not only those on a server tick). The room "
                  "beyond must look the same in each: the pentagram's floor over the pit (func_bossgate) was missing "
                  "in the first frame after the crossing until the server sent both rooms while you straddle a gate."),
        command("A Shot Through A Gate", "map start; wait120; setpos 232 1360 24 0 90 0; wait10; vr_physics_fire 10 232 1500 25")
            .help("vr_physics_fire 10: a pellet's trace at a point beyond the first gate: the console says through 1 "
                  "teleporter(s), in at ..., out at ... (shots and thrown props go through as before)."),
        command("The Whole Feature Off", "vr_teleporters 0; map start; wait120; setpos 232 1390 24 0 90 0; wait60; vr_portals_info; vr_teleporters 1")
            .help("vr_teleporters 0: the see-through teleporter feature off entirely, at once (no map reload) - no gate is built or "
                  "looked through, nothing is carried or traced through one. Placed inside the first gate's trigger, nothing "
                  "carries you, the trigger teleports you the old way (a flash, a jump, 0.7 s locked), and "
                  "vr_portals_info says the feature is off. It is turned back on at the end; Graphics > Teleporters has "
                  "the same switch."),
        command("Slide A Crate Through", "map vrteleporters; wait60; setpos -256 536 24 0 90 0; wait5; noclip 0; vr_test_spawn 107; vr_test_spawn_dist 40; impulse 241; wait30; vr_physics_fling nearest 250 90")
            .help("In vrteleporters: a small crate slid along the floor into the flush player gate (8 deep, a wall right "
                  "behind it): it goes through and comes out of the north gallery's gate (it stopped against the wall "
                  "behind the sheet before Box3D's level contacts were clipped at the gate)."),
        command("Quake's Effects Behind A Gate", "map start; wait90; god; notarget; noclip; setpos 1040 1650 -330 0 270 0; wait5; vr_particle_test quake; setpos 1040 1830 -330 0 270 0")
            .help("start's underwater gate: Quake's own explosion particles and sprite (vr_particle_test quake) made "
                  "behind it, then seen from in front: with a see-through gate surface (Graphics > Teleporters > Teleporter "
                  "Stars > Opacity under 1) they must not show over the view through the gate."),
        command("Hidden Staircase Probe", "map start; wait120; setpos 278 1728 24 7 -20 0; wait60; vr_hull_leafdebug")
            .help("Places the player at the reported staircase spot. setpos enables noclip; turn it off before "
                  "testing movement. The cover is func_bossgate (*38). Both shipped start maps already include "
                  "every world leaf in its own PVS (measured 2026-10-04; the probe scripts, Misc/quakevr/pvs/, are in git "
                  "history)."),
        header("Menus and Dialogs"),
        command("New Game Confirmation (3 s)", "vr_test_dialog 3 0")
            .help("Shows the New Game confirmation for 3 seconds (it closes by itself): the game must stay in the world "
                  "while it's up, turn your head to see (vr_test_dialog [seconds] [mock head turn] [eyeshot]). The old "
                  "kind of dialog (SCR_ModalMessage): the menus' own questions have buttons now (Confirmation Buttons)."),
        command("Confirmation Buttons", "vr_test_confirm")
            .help("vr_test_confirm: a question over this menu with two buttons, as VR Calibration's, Quit's and the other "
                  "menus' questions: point the laser at one and pull the trigger (or y, n, Escape, the arrows and Enter). "
                  "The console says which was taken; the game goes on under it."),
        command("List the Standard Menu's Texts", "vr_menu_dump 0")
            .help("vr_menu_dump [0-2]: every page's rows, choices and help texts at that Menu Detail (0 Standard, 1 Advanced, 2 Developer) as MD| lines in the console: the copy review."),
        command("Check the Boards' Menu Paths", "vr_menu_path_check")
            .help("vr_menu_path_check: every menu page this map's boards name, with its path; a missing one prints MENU PATH MISSING."),
        command("Menu Help Fit", "menu_vr helpcheck").help("menu_vr helpcheck [columns]: every VR page's help wrapped as drawn: the pages whose box grew, the help shown in parts, the longest (HELPSUM)."),
        header("Feature Tests and Benchmarks"),
        command("Screen Tap Feedback", "vr_bullettime_tap_feedback_test 1")
            .help("vr_bullettime_tap_feedback_test [1]: the gadget's click and screen glitch of a tap (1: the activation's; "
                  "none: a double tap's first), without tapping."),
        command("Distortion Trails Test", "vr_bullettime_trails_test 5")
            .help("vr_bullettime_trails_test [count] [m/s] [distance]: shots across your view (a test's distortion "
                  "trails, whatever kinds are on): start bullet time first (or Distortion Trails: Always)."),
        command("Companion Map Tests", "vr_pbr_test")
            .help("Checks source hashing, dimensions, content collisions and indexed palette expansion."),
        command("Input Latency: Walk", "vr_inputlag_test key 10")
            .help("vr_inputlag_test key: a W key press sent through SDL once back in the game, then the "
                  "frames and ms to the bound command, the move sent, the server, the view, full speed, the release and "
                  "the stop (in the console). 'turn' and 'mouse' from the console."),
        command("AO Bake Benchmark", "vr_ao_bench").help("vr_ao_bench [name part] [sse] [reference]: the loaded models' occlusion baked again on the main thread and timed (the game stops for a few seconds); in the console, \"sse\" and \"reference\" (the slow brute-force bake, half a minute on the firing range) bake them those ways too and check the bytes are the same."),
        command("Update Notice: Fake 9.9.9", "vr_update_test_version 9.9.9")
            .help("vr_update_test_version 9.9.9: the update notice shows as for a newer release, without asking anyone "
                  "(its page: the feed's, else the releases' latest). Update Notice: Real Version undoes it."),
        command("Update Notice: Real Version", "vr_update_test_version \"\"")
            .help("vr_update_test_version \"\": the feed's own version again (no notice unless it is newer)."),
        header("Toolgun and Boxes"),
        command("Spawn Every Toolgun Entry", "vr_toolgun_spawn_test")
            .help("vr_toolgun_spawn_test: every entry of the spawn panel made once on a ring around you (God Mode first: "
                  "the monsters come too), each printed spawned, failed or unavailable (console). Then Check and Clear "
                  "the Spawn Test."),
        command("Check and Clear the Spawn Test", "vr_toolgun_spawn_test check")
            .help("vr_toolgun_spawn_test check: each one the spawn test made, still there or gone since (console); then "
                  "all of them removed."),
        command("Carry Load Test", "vr_carry_load_test").help("Checks torso collision and carry load curves; reports stick speed, physical and drawn overlap, and two-hand deduplication."),
        command("Boxes' Self-Test", "vr_brush_selftest")
            .help("vr_brush_selftest: boxes, a wedge and a prism written as .map text and read back, and their models' hulls "
                  "tested at points in and out (PASS or FAIL in the console)."),
        header("Small Gibs Tests (developer 1 for each hit)"),
        command("A Grunt Ahead", "vr_test_spawn 0; vr_test_spawn_dist 96; impulse 241").help("A grunt 96 units ahead, facing you: the tests' target (the nearest monster or corpse)."),
        command("A Grunt's Corpse Ahead", "vr_test_spawn 0; vr_test_spawn_dead 1; vr_test_spawn_dist 96; impulse 241; vr_test_spawn_dead 0"),
        toggle("On the Training Dummy", vr_smallgibs_test_dummy)
            .help("vr_smallgibs_test_dummy: the tests below hit the nearest training dummy instead (the firing range's), and the shotgun, nail, blow and chainsaw ones print the gore each sent (gore hits, wound events, blood particles): to compare with a grunt's."),
        command("A Grunt's Ragdoll Ahead", "vr_ragdoll 1; vr_test_spawn 0; vr_test_spawn_dead 1; vr_test_spawn_dist 96; impulse 241; wait; wait; wait; wait; wait; vr_test_spawn_dead 0")
            .help("Ragdolls on (Gibs and Corpses > Ragdolls) and a grunt killed 96 units ahead: he goes limp as he falls."),
        command("Ragdolls' Fastest Since Last", "vr_ragdoll_speeds").help("vr_ragdoll_speeds: each ragdoll's fastest since the last time you asked (its middle's, and any part's, in units/s), then forgotten: strike a body, then ask, to see how far the blow launched it."),
        command("Blast Beside the Nearest Ragdoll", "vr_ragdoll_blast_test").help("vr_ragdoll_blast_test [damage]: a blast's push (no damage) 24 units beside the nearest ragdoll, on your side: it is thrown away from you."),
        command("Ragdoll and Prop Drawn Motion", "vr_drawn_motion_test 90 nearest")
            .help("vr_drawn_motion_test <frames> [<entity> | nearest | held]: over the next 90 frames, how evenly the nearest ragdoll's parts, the nearest loose prop (held: the one in your hand) and your hands move frame to frame (the console: uneven near 0 is smooth; about 2 with stalls: a step shown twice). Throw or hold one first. vr_debug_ragdoll 2: each frame's blend between the steps."),
        command("Drop the Nearest Prop on the Nearest Corpse", "vr_corpse_drop").help("vr_corpse_drop [height]: the loose prop nearest you put 32 units over the nearest corpse, to fall on it (Gibs and Corpses > Corpse Collision)."),
        command("Gib Props: Gib the Nearest", "vr_gibprops_test 1")
            .help("vr_gibprops_test 1: the nearest live monster gibbed; the console (developer 1) says whether its head and gibs are props (grabbable, destroyable). Then the rows below on its head."),
        command("Gib Props: Kill the Nearest Whole", "vr_gibprops_test 2").help("vr_gibprops_test 2: a corpse to gib with the next row."),
        command("Gib Props: Gib the Nearest Corpse", "vr_gibprops_test 3"),
        command("Gib Props: Grab Its Head", "vr_gibprops_test 4").help("vr_gibprops_test 4: whether the head is a rigid body (Box3D), and your main hand takes it."),
        command("Gib Props: Let Go of It", "vr_gibprops_test 5"),
        command("Gib Props: Shoot Its Head", "vr_gibprops_test 6").help("vr_gibprops_test 6: shot for 1000: it bursts into small gibs."),
        command("Shotgun Blasts", "vr_smallgibs_test 1").help("vr_smallgibs_test 1: 200 shotgun blasts at it (vr_smallgibs_test_n), the share that tore small gibs out."),
        command("Super Shotgun Blasts", "vr_smallgibs_test 2"),
        command("Nails", "vr_smallgibs_test 3"),
        command("Axe, Pommel, Sword, Punch", "vr_smallgibs_test 4").help("vr_smallgibs_test 4: blows of 35 (vr_smallgibs_test_dmg) by the axe's blade and pommel, a sword, and punches of 25."),
        command("Shove, Bash, Slap: No Blood", "vr_smallgibs_test 23")
            .help("vr_smallgibs_test 23: 10 shoves, weapon bashes, slaps and punches on it: the gore each sent (gore hits, wound events, blood particles) and small gibs. Shoves, bashes and slaps draw none; the punches, to compare, do."),
        command("Quad Damage", "vr_smallgibs_test 5"),
        command("Chainsaw a Second", "vr_smallgibs_test 6"),
        command("Burst a Gib and a Head", "vr_smallgibs_test 7"),
        command("Brain Chunks Ahead", "vr_smallgibs_test 21")
            .help("vr_smallgibs_test 21: a head pop's brain chunks (Small Gibs > From a Head Pop) burst just ahead of you, to look at, pick up and throw."),
        command("Popped Underfoot", "vr_smallgibs_test 26")
            .help("vr_smallgibs_test 26: three meat and two brain chunks round your feet; you are dropped 8 units (too "
                  "soft: they stay), then 64 (they pop). The console (sgibtest: underfoot)."),
        command("Brain Jiggle Info", "vr_brain_jiggle_info")
            .help("vr_brain_jiggle_info: the brain chunks followed, those wobbling, the knocks that kicked them and the most squash since the last (Small Gibs > Brain Chunk Jiggle)."),
        command("Most Lying About", "vr_smallgibs_test 8").help("vr_smallgibs_test 8: 15 made with Most Lying About 10: the five oldest go."),
        command("Held, Then Let Go", "vr_smallgibs_test 9").help("vr_smallgibs_test 9: one in the off hand for 3 s with Last 1 s, then let go: it waits until it lands."),
        command("Pass Through the Body", "vr_smallgibs_test 10").help("vr_smallgibs_test 10: one from just behind the monster through it at 300 u/s, with the grace and without."),
        command("Throw Gibs at a Wall", "vr_smallgibs_test 12").help("vr_smallgibs_test 12: a gib and a small gib thrown at 220 u/s into the nearest wall stick (Thrown Gibs Stick 1 for it), a gib at 400 bursts."),
        command("Thrown Gibs Stick, by Mass", "vr_smallgibs_test_n 10; vr_smallgibs_test 22")
            .help("vr_smallgibs_test 22 (Step Up to the Wall Ahead first, a wall 220 units wide): gibs of 8, 12 and 20 kg, "
                  "a grunt's and an ogre's head and a small gib thrown at it as hand throws of 2 to 6 m/s make them, 10 each; "
                  "prints how many stuck or burst (developer 1)."),
        command("Gib Impacts: Smash, Tap, Hit", "vr_smallgibs_test 25")
            .help("vr_smallgibs_test 25 (Step Up to the Wall Ahead first): a head held in the off hand smashed into the "
                  "nearest wall at twice Gib Burst Speed (it bursts) and a third (it doesn't); a falling head struck from "
                  "below by the hand and by a blow at 0.6x (it bounces up) and 1.5x (it bursts). The console."),
        command("Prop Impacts: Tap, Hit, Smash, Throw", "god 1; vr_prop_impact_test 1")
            .help("vr_prop_impact_test 1 (god mode on: an explosive box blows up; near a wall): a small crate, an explosive "
                  "box, a crate's piece and a wall with rubble each struck by a blow at 0.4x, 0.75x and 1.2x its Break Speed "
                  "(unhurt, damaged, broken) and with an axe's heft; held and smashed into the wall; thrown into it. The "
                  "console (propimpact: lines)."),
        command("Light Ammo Thrown at a Crate", "vr_prop_impact_test 2")
            .help("vr_prop_impact_test 2: a nailgun magazine, a super nailgun's, a cell, a shell and a small crate each "
                  "thrown three times at 14 m/s into a fresh small crate ahead of you, one a second. The ammo hardly "
                  "scratches it; the crate breaks it. The console (lightthrow: lines)."),
        command("Blasts Push Props: AD and Quake", "vr_prop_impact_test 3")
            .help("vr_prop_impact_test 3: a small crate beside a blast of Arcane Dimensions' radius damage (its Plasma "
                  "Gun's, rockets'...), then one beside Quake's: both throw it alike; then an AD plasma bolt's blast (thrown "
                  "as Plasma Blast Push) and Quake's of 20 (as the plasma threw before). The console (blastpush: lines)."),
        command("Press Down on a Crate", "vr_test_spawn_hold 0; vr_test_spawn 108; vr_test_spawn_dist 48; impulse 241; "
                                         "wait40; vr_prop_press_test main 50 20")
            .help("vr_prop_press_test: a large crate 48 units ahead, then the main hand (empty, a weapon or a prop held) "
                  "lowered onto its top and 20 cm on into it, held, and drawn up (the mock headset's hand; with a real "
                  "one, press on it yourself for the test's 7 seconds). The crate must not sink or fly off; the hand is "
                  "held back on its top (Physics: Pressing Props). The console (presstest: lines, PASS or FAIL)."),
        command("Step Up to the Wall Ahead", "vr_smallgibs_test 15")
            .help("vr_smallgibs_test 15: puts you 64 units (vr_smallgibs_test_dist) from the wall you face, to throw gibs at it (Gibs and Corpses: Thrown Gibs Stick, Speed to Stick)."),
        command("Flight by Situation", "vr_smallgibs_test 13")
            .help("vr_smallgibs_test 13: on the grunt or corpse ahead, each situation in turn (melee, chainsaw, guns, explosions, thrown, a monster's blow, gibbed by a blow and by a rocket, a large gib burst): launch speeds out and up, and how far they lie 2.5 s on (the console)."),
        command("Blow Up a Crowd", "vr_smallgibs_test 14")
            .help("vr_smallgibs_test 14: 24 grunts (vr_smallgibs_test_crowd) and 12 explosive boxes in rings ahead, a rocket's "
                  "blast among them 1 s on: what is left 3 s on; with Network Messages on (Debug), each frame the broadcast "
                  "was full."),
        command("List Small Gibs", "vr_smallgibs_test 11").help("vr_smallgibs_test 11: how many lie about, their size, how many are rigid bodies, asleep, stuck, held."),
        command("Gib a Body Underfoot: a Blow", "vr_smallgibs_test 16")
            .help("vr_smallgibs_test 16: the grunt or corpse nearest put under your feet and gibbed by your blow; 2 s on, the health you and the monsters near lost to its gibs (Fresh Gibs Harmless For; god off)."),
        command("Gib a Body Underfoot: a Shot", "vr_smallgibs_test 17")
            .help("vr_smallgibs_test 17: the same, gibbed by a super shotgun blast's damage."),
        command("Kill a Monster Underfoot", "vr_smallgibs_test 19")
            .help("vr_smallgibs_test 19: the live grunt nearest put under your feet and killed (not gibbed) by your blow; "
                  "4 s on, the health you lost and its drops' flung hits on you (Monster Drops rows; god off)."),
        command("Drop a Monster's Backpack on You", "vr_smallgibs_test 20")
            .help("vr_smallgibs_test 20: a monster's backpack dropped over your head: past Monster Drops Harmless For its "
                  "fall on you still hurts; 3 s on, the health you lost (god off)."),
        command("Throw a Gib Made 1 s Ago", "vr_smallgibs_test 18")
            .help("vr_smallgibs_test 18: a gib made at your feet, thrown at the monster nearest 1 s later: it still hurts it."),
        toggle("Trace Small Gibs", vr_smallgibs_trace)
            .help("vr_smallgibs_trace: each small gib's first 2.5 s in the console (sgibtrace:): where made, its speed and every jump in it, what touches, nudges or strikes it, how far it lay."),
        header("Training Dummy Tests (dummytest: ...)"),
        command("What the Dummy Is", "vr_dummy_test 1")
            .help("vr_dummy_test 1: the training dummy nearest: its enemy, model, box and health, and the zone a level shot at "
                  "its head, body and legs strikes (1 head, 0 body, 2 limbs, 3 legs)."),
        command("Hit the Dummy for 10", "vr_dummy_test 2").help("vr_dummy_test 2: a plain 10-damage hit from you: its health after."),
        command("Hit the Dummy for Its Health", "vr_dummy_test 3")
            .help("vr_dummy_test 3: a hit of just the health it has left: with Dummy Dies it dies as its enemy."),
        command("The Dummy's Health", "vr_dummy_test 4")
            .help("vr_dummy_test 4: its health now and how long since its last hit (it fills up again after Dummy Health Refills)."),
        header("Decapitation Tests (developer 1: decap: ...)"),
        command("A Zombie Ahead", "vr_test_spawn 2; vr_test_spawn_dist 96; impulse 241")
            .help("A zombie 96 units ahead (a map with zombies: the firing range), for the tests below."),
        command("A Mummy Ahead", "vr_test_spawn 14; vr_test_spawn_dist 96; impulse 241")
            .help("A mummy (Dissolution of Eternity) 96 units ahead, for the tests below: Slash at Its Head beheads it (its "
                  "ragdoll; killed otherwise, it gibs)."),
        command("Slash at Its Head", "vr_decap_test 1")
            .help("vr_decap_test 1: the nearest live monster's health 1, a sword's slash across its neck: it is beheaded "
                  "(decaptest: in the console)."),
        command("Axe Slash at Its Head", "vr_decap_test 9").help("vr_decap_test 9: the same with the axe's head."),
        command("Axe Swept Through Its Neck", "vr_decap_test 61")
            .help("vr_decap_test 61: the axe's slash, its blade swept through the neck as a hand's blow finds what it "
                  "strikes (a knocked-down one's ragdoll too: Knock Down the Nearest first): struck or missed, then "
                  "beheaded at health 1."),
        command("Stab at Its Head", "vr_decap_test 2").help("vr_decap_test 2: the blade driven along its line: killed, not beheaded."),
        command("Pommel at Its Head", "vr_decap_test 3").help("vr_decap_test 3: the pommel strikes: killed, not beheaded."),
        command("Slow Slash at Its Head", "vr_decap_test 4").help("vr_decap_test 4: half the Least Swing Speed: not beheaded."),
        command("Slash at Full Health", "vr_decap_test 7")
            .help("vr_decap_test 7: a light slash at the head at full health: a zombie is beheaded, others aren't."),
        command("Slash a Corpse's Head", "vr_decap_test 5").help("vr_decap_test 5: the nearest ragdoll or corpse beheaded by a slash."),
        command("Chainsaw at Its Neck", "vr_decap_test 6").help("vr_decap_test 6: the chainsaw's bar at the neck (health 1)."),
        command("Chainsaw a Corpse's Neck", "vr_decap_test 10").help("vr_decap_test 10: the nearest corpse's head cut by the chainsaw."),
        command("Throw an Axe at Its Head", "vr_decap_test 11; vr_test_axe_up 15; vr_test_axe_at 1; vr_test_axe_dist 70; vr_test_axe 0; impulse 209")
            .help("Its health 1, then an axe thrown edge first at its head from 70 units (a grunt's: Axe Height 15)."),
        command("Gib the Headless Corpse", "vr_decap_test 8")
            .help("vr_decap_test 8: the nearest headless corpse gibbed: no head thrown (the heads counted before and after)."),
        command("Shotgun at Its Head", "vr_decap_test 12")
            .help("vr_decap_test 12: a shotgun blast from your eye at the nearest live monster's head that just kills it: "
                  "its head pops, no head thrown (decaptest: popped 1)."),
        command("Super Shotgun at Its Head", "vr_decap_test 13").help("vr_decap_test 13: the same with a super shotgun blast."),
        command("Super Shotgun at Its Head, Overkill", "vr_decap_test 14")
            .help("vr_decap_test 14: a super shotgun blast at its head at health 1 (it would gib): its head pops, no gibs."),
        command("Lightning at Its Head", "vr_decap_test 15").help("vr_decap_test 15: a lightning bolt at its head at health 1: popped."),
        command("Shotgun at Its Body", "vr_decap_test 16").help("vr_decap_test 16: a shotgun blast at its body at health 1: killed, not popped."),
        command("Shotgun at Its Head, Not Killing", "vr_decap_test 17")
            .help("vr_decap_test 17: a shotgun blast at its head at health 500: not popped (a zombie: popped, dead for good)."),
        command("Lightning at Its Body", "vr_decap_test 18").help("vr_decap_test 18: a lightning bolt at its body at health 1: not popped."),
        command("Sweep the Head Zone", "vr_decap_test 19")
            .help("vr_decap_test 19: blows moved level at the nearest live monster's head from 16 sides, at heights 24 units below "
                  "its head's middle to 16 above: how many meet the melee's beheading zone at each (decapsweep: in the console)."),
        header("Limb Gore Tests (limbtest: ... in the console; developer 1: limbs: ...)"),
        command("A Grunt Ahead", "vr_test_spawn 0; vr_test_spawn_dist 96; impulse 241").help("A grunt 96 units ahead, for the tests below."),
        command("A Grunt's Corpse Ahead", "vr_test_spawn 0; vr_test_spawn_dist 96; vr_test_spawn_dead 1; impulse 241; vr_test_spawn_dead 0")
            .help("A dead grunt 96 units ahead (a ragdoll), for the corpse tests."),
        command("Spawn Its Limbs", "vr_limb_test 1")
            .help("vr_limb_test 1: every limb of the nearest monster's model hung in a row before you: the limb models made "
                  "from its own (vr_limb_models lists them)."),
        command("List Its Limb Models", "vr_limb_models").help("vr_limb_models [model]: the grunt's (or that model's) limbs: each one's triangles, cap and size."),
        command("Cut a Corpse Apart, Ends First", "vr_limb_test 2")
            .help("vr_limb_test 2: the nearest corpse's limbs cut off one by one, hands and shins first, down to the torso, "
                  "then its head: the parts left after each."),
        command("Cut a Corpse Apart, Whole Limbs", "vr_limb_test 3").help("vr_limb_test 3: the same, whole arms and legs at once."),
        command("Slash at a Limb", "vr_limb_test 4")
            .help("vr_limb_test 4: the nearest live monster's health 1, a sword's slash at its forearm: it falls as a ragdoll, "
                  "the forearm flies off."),
        command("Punch a Limb", "vr_limb_test 5").help("vr_limb_test 5: a fist's killing blow there: popped by chance (vr_decap_pop_roll 0: always)."),
        command("Shotgun at a Limb", "vr_limb_test 6").help("vr_limb_test 6: a shotgun blast there that kills."),
        command("Lightning at a Limb", "vr_limb_test 7").help("vr_limb_test 7: a lightning bolt there that kills."),
        command("Explosion by a Limb", "vr_limb_test 9").help("vr_limb_test 9: an explosion beside it that kills: its limbs near it pop by chance."),
        command("Gib It", "vr_limb_test 10").help("vr_limb_test 10: gibbed: its own limbs thrown (Gibbed Bodies Throw Limbs)."),
        command("Slash at Full Health", "vr_limb_test 12").help("vr_limb_test 12: a light slash at a limb at full health: a zombie dies of it, others don't."),
        command("Chance Rates", "vr_limb_test 8").help("vr_limb_test 8: 2000 rolls at chance 0.5 for a head and a limb: the rates with Head Chance and Limb Chance."),
        command("Most Limbs", "vr_limb_test 11").help("vr_limb_test 11: twice Most Limbs Lying About thrown: how many stay."),
        command("Where Its Limbs Map", "vr_limb_test 13").help("vr_limb_test 13: each limb's surface point and the joint a hit there cuts."),
        command("The Limbs Lying About", "vr_limb_test 17").help("vr_limb_test 17: each limb thrown: its model, where it is, how fast (none fallen out of the world)."),
        command("Cut Ends' Fountains", "vr_limb_test 18")
            .help("vr_limb_test 18: each flying limb's (or head's) own fountain: its age, its spout (how far from the piece's "
                  "middle, and from the stump's joint as it was cut), the piece's speed and weight; the drops so far."),
        command("Look Into Its Stump", "vr_limb_test 39")
            .help("vr_limb_test 39: you float (noclip) 14 units from the nearest corpse's first stump, turned to face it from "
                  "the side it faces (look ahead and down first): the cut's flesh up close (noclip again to walk)."),
        command("Look Into the Limb's Cap", "vr_limb_test 40")
            .help("vr_limb_test 40: the last limb thrown hung in the air 10 units before your eyes where you look, its cut "
                  "end turned to you."),
        command("Limb Weights Here", "vr_limb_test 19")
            .help("vr_limb_test 19: every kind of monster on the map: what its head and each limb weigh cut off (kg, share of "
                  "its ragdoll's Mass; * a whole limb) (Limb Gore > Limb Weight)."),
        command("Enemy Laser at the Head", "vr_limb_test 20")
            .help("vr_limb_test 20: an enforcer's laser bolt (an enemy's) into the nearest living monster's head that kills "
                  "it (health 1): popped at Enforcer Laser's chance (vr_decap_pop_roll 0: always)."),
        command("Enemy Laser at a Limb", "vr_limb_test 21").help("vr_limb_test 21: the same at its forearm."),
        command("Repeated Blunt Hits on a Corpse Limb", "vr_limb_test 24")
            .help("vr_limb_test 24: repeated blunt hits detach the nearest corpse's limb without losing the detached piece."),
        command("Repeated Blunt Hits on a Corpse Torso", "vr_limb_test 25")
            .help("vr_limb_test 25: a heavy blunt hit to the corpse's torso gibs the whole corpse."),
        command("One Blunt Hit on a Corpse Limb", "vr_limb_test 28")
            .help("vr_limb_test 28: one blunt hit wears the struck limb down without popping it (save, load, then "
                  "Stored Limb Damage)."),
        command("Stored Limb Damage", "vr_limb_test 29")
            .help("vr_limb_test 29: prints the damage worn into the nearest corpse's test limb (kept across save/load)."),
        command("Wear a Corpse Limb Off: Shotgun", "vr_limb_test 30")
            .help("vr_limb_test 30: shotgun blasts at the nearest corpse's limb until it pops: any weapon's hits wear down "
                  "the piece they strike (its own corpse health), whatever the pop chance (clearest with Limb Chance and "
                  "Head Chance at 0)."),
        command("Wear a Corpse Limb Off: Super Shotgun", "vr_limb_test 31").help("vr_limb_test 31: the same with super shotgun blasts."),
        command("Wear a Corpse Limb Off: Lightning", "vr_limb_test 32").help("vr_limb_test 32: the same with lightning bolts."),
        command("Wear a Corpse Limb Off: Nails", "vr_limb_test 33").help("vr_limb_test 33: the same with nails."),
        command("Wear a Corpse Limb Off: Rifle Laser", "vr_limb_test 34").help("vr_limb_test 34: the same with the enforcer rifle's lasers."),
        command("Wear a Corpse Limb Off: AD Pellets", "vr_limb_test 35").help("vr_limb_test 35: the same with Arcane Dimensions' projectile pellets."),
        command("Wear a Corpse Limb Off: Axe", "vr_limb_test 36").help("vr_limb_test 36: the same with axe blows that do not cut (it is cut off when worn through)."),
        command("Shotgun a Corpse Torso", "vr_limb_test 37").help("vr_limb_test 37: shotgun blasts at the torso take the whole body's health: it gibs, no piece worn."),
        command("Wear a Corpse Head Off: Shotgun", "vr_limb_test 38").help("vr_limb_test 38: shotgun blasts at the corpse's head until it pops."),
        command("Held Prop at a Corpse Limb", "vr_limb_test 26")
            .help("vr_limb_test 26: a blunt hit with a held prop detaches and keeps the corpse's struck limb."),
        command("Axe at a Corpse Limb", "vr_limb_test 27")
            .help("vr_limb_test 27: the axe control for the corpse limb tests cuts and keeps the struck limb."),
        command("Hand to the Last Limb", "vr_limb_test 14").help("vr_limb_test 14: the mock main hand put on the last limb thrown (then grip: vr_mock_button main grip 1; vr_limb_test 15 says if it is held)."),
        header("Crowd Gore Tests (goretest: crowd ... in the console; the benchmarks' gore scenarios)"),
        slider("Crowd Radius", vr_gore_test_crowd, 0.f, 1024.f, 64.f, "%.0f units")
            .help("Above 0, the limb and head tests act on every monster this near you at once, in one frame, quietly (one "
                  "goretest: crowd line): Crowd: ... below, or any test above. 0: the nearest monster only (vr_gore_test_crowd)."),
        command("Spawn a Crowd (16 Grunts)", "vr_physics_spawn monster_army 100 -60;vr_physics_spawn monster_army 100 -20;vr_physics_spawn monster_army 100 20;vr_physics_spawn monster_army 100 60;vr_physics_spawn monster_army 148 -60;vr_physics_spawn monster_army 148 -20;vr_physics_spawn monster_army 148 20;vr_physics_spawn monster_army 148 60;vr_physics_spawn monster_army 196 -60;vr_physics_spawn monster_army 196 -20;vr_physics_spawn monster_army 196 20;vr_physics_spawn monster_army 196 60;vr_physics_spawn monster_army 244 -60;vr_physics_spawn monster_army 244 -20;vr_physics_spawn monster_army 244 20;vr_physics_spawn monster_army 244 60")
            .help("vr_physics_spawn: 16 grunts in a 4 by 4 grid ahead of you (notarget keeps them still)."),
        command("Crowd: Slash a Limb", "vr_limb_test 4").help("vr_limb_test 4 on the crowd (Crowd Radius above 0): each killed by a slash at a limb."),
        command("Crowd: Cut Corpses Apart", "vr_limb_test 3").help("vr_limb_test 3 on the crowd's corpses: every limb and the head cut off."),
        command("Crowd: Gib", "vr_limb_test 10").help("vr_limb_test 10 on the crowd: each gibbed (its limbs thrown: Gibbed Bodies Throw Limbs)."),
        command("Crowd: Head Pops", "vr_decap_test 14").help("vr_decap_test 14 on the crowd: a super shotgun headshot each (head pop by chance: vr_decap_pop_roll 0 pops all)."),
        header("Head Pop Chance Tests (poptest: ... in the console)"),
        slider("Test Range", vr_decap_poptest_dist, 0.5f, 25.f, 0.5f, "%.1f lengths")
            .help("The tests below shoot (or throw) from this many player lengths (56 units) off the nearest live monster's head "
                  "(you are moved there and back; round it till the way is clear) (vr_decap_poptest_dist)."),
        command("Chance Table", "vr_decap_test 40")
            .help("vr_decap_test 40: each weapon's head pop chance at ranges, for all, half and a fifth of the pellets at the head."),
        command("Shotgun Headshot Kill", "vr_decap_test 41").help("vr_decap_test 41: a shotgun blast (no spread) at its head at health 1, from the Test Range."),
        command("Super Shotgun Headshot Kill", "vr_decap_test 42").help("vr_decap_test 42: the same with the super shotgun."),
        command("Lightning Headshot Kill", "vr_decap_test 43").help("vr_decap_test 43: the same with a lightning bolt."),
        command("Super Shotgun Body Kill", "vr_decap_test 44").help("vr_decap_test 44: a super shotgun blast at its body at health 1: never popped."),
        command("Shotgun Rates", "vr_decap_test 45")
            .help("vr_decap_test 45: vr_decap_poptest_n shotgun blasts with its spread at its head from the Test Range, not killing "
                  "it: the mean chance, the rate it would pop, by pellets at the head."),
        command("Super Shotgun Rates", "vr_decap_test 46").help("vr_decap_test 46: the same with the super shotgun."),
        command("Throw a Weapon at Its Head", "vr_decap_test 47")
            .help("vr_decap_test 47: weapon vr_decap_poptest_wid (10: the rocket launcher) thrown at 16 m/s into its head at health 1."),
        command("Throw an Explosive Box at Its Head", "vr_decap_test 48").help("vr_decap_test 48: the same with an explosive box (that never blows up)."),
        command("Blunt Melee Rates", "vr_decap_test 49")
            .help("vr_decap_test 49: each blunt weapon's blow (fist, gun, crowbar, pommel, club, Mjolnir) at its head, soft, medium "
                  "and hard, vr_decap_poptest_n times, not killing it: the chance and the rate it pops."),
        command("Hard Punch Kill", "vr_decap_test 50").help("vr_decap_test 50: a hard punch at its head at health 1 (popped by chance)."),
        command("Hard Gun Butt Kill", "vr_decap_test 51").help("vr_decap_test 51: the same with a gun's blow."),
        command("Hard Crowbar Kill", "vr_decap_test 52").help("vr_decap_test 52: the same with the crowbar."),
        command("Hard Pommel Kill", "vr_decap_test 53").help("vr_decap_test 53: the same with a sword's pommel."),
        command("Hard Club Kill", "vr_decap_test 54").help("vr_decap_test 54: the same with a club."),
        command("Hard Mjolnir Kill", "vr_decap_test 55").help("vr_decap_test 55: the same with Mjolnir's head."),
        command("Nail at Its Head", "vr_decap_test 56").help("vr_decap_test 56: a nail at its head at health 1 (popped only with Quad Damage)."),
        command("Rocket at Its Head", "vr_decap_test 57").help("vr_decap_test 57: the same with a rocket."),
        command("Grenade at Its Head", "vr_decap_test 58").help("vr_decap_test 58: the same with a grenade."),
        command("Give Quad Damage", "impulse 255").help("impulse 255: Quad Damage for 30 s (the tests above: every head kill pops)."),
        header("Burning Tests (developer 1: burning: ...)"),
        command("Set It on Fire (a Torch's Blow)", "vr_burn_test 1")
            .help("vr_burn_test 1: the nearest monster, corpse, crate or crate's piece set on fire (an explosive box: it can't burn) as a lit torch's blow would, where it faces you "
                  "(A Grunt Ahead, A Grunt's Corpse Ahead above). Again: a flame more, the same damage."),
        command("Set It on Fire (a Lava Nail)", "vr_burn_test 2"),
        command("Set It on Fire (a Touch)", "vr_burn_test 3"),
        command("Load Lava Nails", "vr_burn_test 5").help("vr_burn_test 5: the nailgun or super nailgun in the main hand loaded with lava nails (impulse 156 or 157 gives one)."),
        command("A Nail Through a Torch's Flame", "vr_burn_test 6").help("vr_burn_test 6: a nail shot through the flame of the lit torch nearest you: it becomes a lava nail (Nails Through a Flame)."),
        command("Smash the Nearest Crate", "vr_burn_test 8")
            .help("vr_burn_test 8: the nearest crate broken as by your blow: plain pieces, which burn (Pieces Burn)."),
        command("Burn the Nearest Crate Through", "vr_burn_test 9")
            .help("vr_burn_test 9: the nearest crate burns through now (lit first if it isn't): charred pieces."),
        command("A Lava Nail at the Nearest Piece", "vr_burn_test 11")
            .help("vr_burn_test 11: a lava nail of yours fired down at the nearest crate's piece: it catches fire (Pieces Burn), not bursts; a charred one bursts."),
        command("A Nail Through the Nearest Map Flame", "vr_burn_test 12")
            .help("vr_burn_test 12: a nail of yours shot level through the nearest of the map's flames (a flame, a brazier, a wall torch, a candle): it becomes a lava nail (the console says so)."),
        command("Nearest Monster Into a Map Flame", "developer 1; vr_burn_test 15")
            .help("vr_burn_test 15: the monster or body nearest you set against the nearest of the map's flames (woken): it catches fire within a few tenths of a second (Map Flames Burn Monsters; the console says so)."),
        command("Count the Pieces", "vr_burn_test 10").help("vr_burn_test 10: the console: the crates' pieces lying about, how many charred, how many burning."),
        command("Set Yourself on Fire", "developer 1; vr_burn_test 13")
            .help("vr_burn_test 13: you set on fire as a monster's flame or a lava nail would (a hit at your chest): its flames sit on your body as drawn."),
        command("How Far Your Flames Sit Off You", "developer 1; vr_burn_test 14")
            .help("vr_burn_test 14: the console: how far each of your flames sits off your body (its shapes as drawn; 0: on it)."),
        command("How It Burns", "vr_burn_test 4").help("vr_burn_test 4: the nearest monster or corpse: where, its health, its flames and the time it burns yet."),
        header("Incandescent Blades (developer 1: hotblade: ...)"),
        command("Heat the Held Blades", "developer 1; vr_hotblade_test 1")
            .help("vr_hotblade_test 1: the melee weapons in your hands red-hot at once, for all of Stays Hot (Weapons > Incandescent Blades)."),
        command("Into Their Fade", "developer 1; vr_hotblade_test 4").help("vr_hotblade_test 4: half of Fades Over left: watch them fade out."),
        command("Half Heated (Held There)", "developer 1; vr_hotblade_test 5").help("vr_hotblade_test 5: their build-up at half, held there (the look of a blade heating)."),
        command("Quench Them", "developer 1; vr_hotblade_test 2").help("vr_hotblade_test 2: cold at once (no hiss)."),
        command("Strike the Nearest Monster or Corpse", "developer 1; vr_hotblade_test 6")
            .help("vr_hotblade_test 6: a blow of the main hand's weapon (base 20, the heat's extra too) lands on the nearest monster or corpse: a red-hot blade sets it on fire (A Grunt Ahead, A Grunt's Corpse Ahead above)."),
        command("Report", "developer 1; vr_hotblade_test 3").help("vr_hotblade_test 3: the console: each hand's and holster's weapon's heat, glow and time left."),
        command("Blade into the Nearest Flame", "developer 1; noclip; vr_hotblade_test 7")
            .help("vr_hotblade_test 7 (with noclip): you are moved so that the main hand's blade is in the nearest of the map's flames or wall torches. Keep the hand still."),
        command("Blade into the Nearest Lava", "developer 1; noclip; god; vr_hotblade_test 8").help("vr_hotblade_test 8: the same into lava near you."),
        command("Blade into the Nearest Water", "developer 1; noclip; vr_hotblade_test 11").help("vr_hotblade_test 11: the same into slime or water near you (it quenches)."),
        command("Take the Nearest Wall Torch (Off Hand)", "developer 1; vr_hotblade_test 9").help("vr_hotblade_test 9: the nearest torch on its wall into the off hand (keep its grip held)."),
        command("Report What the Client Draws", "vr_hotblade_report")
            .help("vr_hotblade_report: the glowing blades drawn (hands, holsters, lying), the heated part's length and "
                  "its share of the model's vertices that glow, their light, and the embers and trail particles made in the last second."),
        command("Report the Lying Blades", "developer 1; vr_hotblade_test 12")
            .help("vr_hotblade_test 12: each blade lying about or flying: its heat, glow and time left, and the glow sent to the client."),
        command("Throw It at the Nearest Monster", "developer 1; vr_hotblade_test 13")
            .help("vr_hotblade_test 13: the main hand's weapon thrown straight at the nearest monster (hard, hardly falling): "
                  "a red-hot one hits harder and sets it on fire."),
        command("Drop It Here", "developer 1; vr_hotblade_test 14")
            .help("vr_hotblade_test 14: the main hand's weapon let go of where the hand is: watch it glow and cool on the ground."),
        command("Off Hand's Torch to the Blade", "developer 1; vr_hotblade_test 10").help("vr_hotblade_test 10: the off hand's lit torch's flame brought round the main hand's blade (the mock headset's hand)."),
        header("Wall Torches Shot Off Their Walls (developer 1: walltorch: ...)"),
        command("Shoot the Nearest Wall Torch (a Pellet)", "developer 1; vr_test_walltorch_shot 10")
            .help("vr_test_walltorch_shot 10: the wall torch nearest you shot from your eyes with a shotgun's pellets: it comes off its wall and falls lit (VR Settings > Combat > Wall Torches > Shot Off Its Wall)."),
        command("Shoot It (a Nail)", "developer 1; vr_test_walltorch_shot 1")
            .help("vr_test_walltorch_shot 1: the same, with a nail."),
        command("Shoot It (a Rocket)", "developer 1; vr_test_walltorch_shot 0")
            .help("vr_test_walltorch_shot 0: the same, with a rocket."),
        command("Shoot It (a Grenade)", "developer 1; vr_test_walltorch_shot 2")
            .help("vr_test_walltorch_shot 2: the same, with a grenade. A grenade in flight is a thrown box, not a shot: "
                  "only shots and missiles stop at a torch, so its flight goes past it and its blast is what knocks it "
                  "off — see A Blast at It below."),
        command("Shoot It (a Lightning Bolt)", "developer 1; vr_test_walltorch_shot 21")
            .help("vr_test_walltorch_shot 21: the same, with a lightning bolt (LightningDamage, as the lightning gun)."),
        command("A Blast at It", "developer 1; vr_test_walltorch_shot 22")
            .help("vr_test_walltorch_shot 22: a blast at the wall torch nearest you, of a rocket's radius damage "
                  "(T_RadiusDamage): what a rocket's or a grenade's explosion does to the torches near it."),
        command("Throw the Nearest Prop at It", "developer 1; vr_test_walltorch_shot 20")
            .help("vr_test_walltorch_shot 20: the loose prop nearest you sent at the wall torch nearest you, at vr_test_fling_speed m/s: it comes off its wall. Below vr_test_fling_speed, it is not a throw: the torch stays."),
        toggle("Torch Flames to Console", vr_walltorch_debug)
            .help("vr_walltorch_debug: each lit torch you hold or that lies about, twice a second (wtflame: how far upside "
                  "down, its flame's foot, its speed and flattening, what of you its flame touches), and the torches' smoke "
                  "(wtsmoke). 2: every frame."),
        header("Self-Tests and Benchmarks"),
        command("Tracy Pool Self-Test", "vr_tracy_test_memory").help("Exercise realloc, cache frees, hunk reset and memory detail changes."),
        command("QuakeC Loop Bench", "vr_qcexec_bench")
            .help("vr_qcexec_bench: times a QuakeC math function in each loop (in a map) and checks they agree; the "
                  "result in the console."),
        command("Thread Pool Split Bench", "vr_jobs_bench")
            .help("vr_jobs_bench [reps]: a small shared-out loop (2 to 21 items of 0 to 50 microseconds) timed on the "
                  "caller alone and split between threads, the workers asleep between calls: where splitting pays "
                  "(about 15 seconds; the game stops meanwhile)."),
        command("Thread Pool Self-Test", "vr_jobs_test")
            .help("vr_jobs_test: the pool's checks (start and stop, every item once, the main thread helping, busy workers, "
                  "nested waits, the same results whatever the threads), a line each (under a second)."),
        command("Zancle Math Self-Test", "vr_zancle_math_test")
            .help("vr_zancle_math_test: Zancle's math (and the angle wrap) against the standard library's on edge values "
                  "(signed zeros, halves, wrap angles, infinities, NaN), to the last bit; one line."),
        command("SHA-256 Self-Test", "vr_sha256_test")
            .help("vr_sha256_test: the SHA-256 that checks each Map Library download against its index, on the standard "
                  "test vectors (FIPS 180-2's, a million a's, the padding's edges); one line."),
        command("Grasp Bench", "vr_grasp_bench")
            .help("vr_grasp_bench: each hand's grasp solve on what it holds, timed (afresh, and again 1000 times)."),
        command("Heap Self-Test", "vr_heap test")
            .help("vr_heap test: every way the code allocates (malloc, calloc, realloc, strdup, new, aligned new, "
                  "_aligned_malloc) gives mimalloc's blocks, and the C runtime's own blocks go back to it; PASS or FAIL."),
        toggle("Test: No GL Context at the Next VR Start", vr_xr_test_drop_context)
            .help("vr_xr_test_drop_context 1: the next eye swapchain is requested with no GL context current (once): the "
                  "game makes its context current again first and logs it in qvr_openxr.txt. Then vr_restart."),
        header("Crashes"),
        command("Crash: Access Violation", "vr_crash_test av")
            .help("vr_crash_test av: crashes the game now, on purpose, to test the crash report: quakevr/crash/<date>_<time>.txt "
                  "(the stack with function names, the build, the map) and a .dmp beside it, and a dialog naming them. "
                  "The game quits!"),
        command("Crash: On a Worker Thread", "vr_crash_test thread")
            .help("vr_crash_test thread: an access violation on a thread pool worker: the report has that thread's stack. "
                  "The game quits!"),
        command("Crash: Stack Overflow", "vr_crash_test stack")
            .help("vr_crash_test stack: endless recursion on the main thread: the report, from what is left of its stack. "
                  "The game quits!"),
        command("Crash: Fatal Error", "vr_crash_test error")
            .help("vr_crash_test error: a Sys_Error (the \"Quake VR: Unleashed - Error\" dialog, which names the report). "
                  "threaderror: one on a worker thread. The game quits!"),
        command("Crash: Failed Assert", "vr_crash_test assert")
            .help("vr_crash_test assert: a failed assert's report (Zancle's handler; zassert: one in Zancle's library, "
                  "Debug builds with QVR_ZANCLE_DEBUG). The game quits!"),
        command("Crash: abort()", "vr_crash_test abort")
            .help("vr_crash_test abort: abort(), as std::terminate and the CRT's fatal checks end (purecall: a pure virtual "
                  "call). The game quits!"),
        command("Error: Host_Error", "vr_crash_test hosterror")
            .help("vr_crash_test hosterror: an error that ends the game session but not the program: back to the console, "
                  "with its caller's stack printed."),
    };
}
