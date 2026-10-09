# Round 21: melee from your recordings, fitted hands, the flashlight reworked

The round's working notes, newest at the end: add new sections at the end of this file, as before. This file holds
the work from 2026-10-04 on. The sections up to 2026-10-03 are archived in `archive/` (the index below lists each
title and its file), unchanged apart from their "In the headset" checklists, which were left out. Code comments cite
sections as `ROUND21.md, "<title>"`: a title that is not a section here is in the index.

Where the current state lives:
- What the port does: [FEATURES.md](../FEATURES.md); the settings: the VR Settings menu and `Quake/vr/vr_cvars.inc`.
- What is still to check in the headset: `quakevr/checklist.txt` (the checklist the author ticks).
- What is planned or deferred: [BACKLOG.md](BACKLOG.md).
- How to test: [PLAYTEST.md](PLAYTEST.md) (in the headset) and [TESTING.md](TESTING.md) (scripted, headless).

## Archived sections (index)

### [archive/ROUND21_2026-09.md](archive/ROUND21_2026-09.md)

- [Motion recorder](archive/ROUND21_2026-09.md#motion-recorder)
- [Melee, redesigned](archive/ROUND21_2026-09.md#melee-redesigned)
- [Fitted hands](archive/ROUND21_2026-09.md#fitted-hands)
- [Fitted hands, second pass](archive/ROUND21_2026-09.md#fitted-hands-second-pass)
- [Fitted hands, third pass (tuning)](archive/ROUND21_2026-09.md#fitted-hands-third-pass-tuning)
- [Wrist gadget, hologram, casings, flashlight](archive/ROUND21_2026-09.md#wrist-gadget-hologram-casings-flashlight)
- [C++ audit fixes](archive/ROUND21_2026-09.md#c-audit-fixes)
- [Menu: sliders past their ends](archive/ROUND21_2026-09.md#menu-sliders-past-their-ends)
- [Blunt pommel sound](archive/ROUND21_2026-09.md#blunt-pommel-sound)
- [Two-handed props](archive/ROUND21_2026-09.md#two-handed-props)
- [Held weapons against models](archive/ROUND21_2026-09.md#held-weapons-against-models)
- [Reviewing failing takes](archive/ROUND21_2026-09.md#reviewing-failing-takes)
- [Weapon posing mode](archive/ROUND21_2026-09.md#weapon-posing-mode)
- [Posing a weapon in a holster](archive/ROUND21_2026-09.md#posing-a-weapon-in-a-holster)
- [Box3D physics](archive/ROUND21_2026-09.md#box3d-physics)
- [Carried box after loading a game](archive/ROUND21_2026-09.md#carried-box-after-loading-a-game)
- [After the posing test](archive/ROUND21_2026-09.md#after-the-posing-test)
- [Flashlight tuning](archive/ROUND21_2026-09.md#flashlight-tuning)
- [Particles at high multipliers; long sessions](archive/ROUND21_2026-09.md#particles-at-high-multipliers-long-sessions)
- [Leaning; menu opacity](archive/ROUND21_2026-09.md#leaning-menu-opacity)
- [Body and weapon models improved](archive/ROUND21_2026-09.md#body-and-weapon-models-improved)
- [Hands remodelled](archive/ROUND21_2026-09.md#hands-remodelled)
- [Hand calibration](archive/ROUND21_2026-09.md#hand-calibration)
- [Hand editable in Blender](archive/ROUND21_2026-09.md#hand-editable-in-blender)
- [Forearm, bracer and wrist](archive/ROUND21_2026-09.md#forearm-bracer-and-wrist)
- [AA and liquids; liquid transparency saved](archive/ROUND21_2026-09.md#aa-and-liquids-liquid-transparency-saved)
- [Parry stamina and counter-attacks](archive/ROUND21_2026-09.md#parry-stamina-and-counter-attacks)
- [Grab reach from the fist; two-handed detach; brushing fingers](archive/ROUND21_2026-09.md#grab-reach-from-the-fist-two-handed-detach-brushing-fingers)
- [Items as physics pickups; sinking; spinning shapes](archive/ROUND21_2026-09.md#items-as-physics-pickups-sinking-spinning-shapes)
- [Gadget stats; gadget on the forearm; torch grip](archive/ROUND21_2026-09.md#gadget-stats-gadget-on-the-forearm-torch-grip)
- [Editing the body, the weapons and the gadget in Blender](archive/ROUND21_2026-09.md#editing-the-body-the-weapons-and-the-gadget-in-blender)
- [Arm IK with calibrated hands](archive/ROUND21_2026-09.md#arm-ik-with-calibrated-hands)
- [The flashlight and every other model in Blender](archive/ROUND21_2026-09.md#the-flashlight-and-every-other-model-in-blender)
- [Align Sights to My Aim](archive/ROUND21_2026-09.md#align-sights-to-my-aim)
- [Model bump maps](archive/ROUND21_2026-09.md#model-bump-maps)
- [Enemies hurt by liquids; holster orientation](archive/ROUND21_2026-09.md#enemies-hurt-by-liquids-holster-orientation)
- [Dummy attacks (firing range)](archive/ROUND21_2026-09.md#dummy-attacks-firing-range)
- [Flashlight shadows, cord and hand-over](archive/ROUND21_2026-09.md#flashlight-shadows-cord-and-hand-over)
- [Baked normal maps](archive/ROUND21_2026-09.md#baked-normal-maps)
- [Melee fixes: flashlight, axe on walls, gibs](archive/ROUND21_2026-09.md#melee-fixes-flashlight-axe-on-walls-gibs)
- [Dynamic wounds, burns and wetness](archive/ROUND21_2026-09.md#dynamic-wounds-burns-and-wetness)
- [Climbing with both hands](archive/ROUND21_2026-09.md#climbing-with-both-hands)
- [Body calibration](archive/ROUND21_2026-09.md#body-calibration)
- [Simplification: Box3D only, knights always drop swords](archive/ROUND21_2026-09.md#simplification-box3d-only-knights-always-drop-swords)
- [Body collisions](archive/ROUND21_2026-09.md#body-collisions)
- [Stamina on the gadget; the glow](archive/ROUND21_2026-09.md#stamina-on-the-gadget-the-glow)
- [Arms options after body calibration; holster limits](archive/ROUND21_2026-09.md#arms-options-after-body-calibration-holster-limits)
- [Authored bumps on the body; parallax for authored models](archive/ROUND21_2026-09.md#authored-bumps-on-the-body-parallax-for-authored-models)
- [Defaults: the author's weapon offsets and settings (2026-09-28)](archive/ROUND21_2026-09.md#defaults-the-authors-weapon-offsets-and-settings-2026-09-28)
- [Climbing: hand placement and grab leniency](archive/ROUND21_2026-09.md#climbing-hand-placement-and-grab-leniency)
- [Stamina for shoves and strikes; thrown damage on gibs](archive/ROUND21_2026-09.md#stamina-for-shoves-and-strikes-thrown-damage-on-gibs)
- [Defaults, second pass (2026-09-28 afternoon)](archive/ROUND21_2026-09.md#defaults-second-pass-2026-09-28-afternoon)
- [Per-weapon holstered pose](archive/ROUND21_2026-09.md#per-weapon-holstered-pose)
- [Swimming: air supply; strokes against the palm](archive/ROUND21_2026-09.md#swimming-air-supply-strokes-against-the-palm)
- [Climbing: hand orientation, staying attached, small ledges](archive/ROUND21_2026-09.md#climbing-hand-orientation-staying-attached-small-ledges)
- [Deflection by blows and bashes; catching grenades; ogre aim](archive/ROUND21_2026-09.md#deflection-by-blows-and-bashes-catching-grenades-ogre-aim)
- [Catching versus deflecting; returned grenades; sword bash direction](archive/ROUND21_2026-09.md#catching-versus-deflecting-returned-grenades-sword-bash-direction)
- [Weight: spring model, stamina, held object offsets; explosive boxes](archive/ROUND21_2026-09.md#weight-spring-model-stamina-held-object-offsets-explosive-boxes)
- [Wall torches you can take](archive/ROUND21_2026-09.md#wall-torches-you-can-take)
- [Rocks and bricks](archive/ROUND21_2026-09.md#rocks-and-bricks)
- [Menu: scroll memory and shortcuts](archive/ROUND21_2026-09.md#menu-scroll-memory-and-shortcuts)
- [Climbing: sliding along the wall; throw angle after calibration](archive/ROUND21_2026-09.md#climbing-sliding-along-the-wall-throw-angle-after-calibration)
- [Grappling hook: rope, reel on demand, props and monsters](archive/ROUND21_2026-09.md#grappling-hook-rope-reel-on-demand-props-and-monsters)
- [Spring only; Weapon Weights and Held Object Weights; weight and damage](archive/ROUND21_2026-09.md#spring-only-weapon-weights-and-held-object-weights-weight-and-damage)
- [Menus reorganized](archive/ROUND21_2026-09.md#menus-reorganized)
- [Recording: smoothed mirror and spectator camera](archive/ROUND21_2026-09.md#recording-smoothed-mirror-and-spectator-camera)
- [Quad Damage on melee](archive/ROUND21_2026-09.md#quad-damage-on-melee)
- [Profiling: where the time goes](archive/ROUND21_2026-09.md#profiling-where-the-time-goes)
- [Performance fixes (review, 2026-09-28)](archive/ROUND21_2026-09.md#performance-fixes-review-2026-09-28)
- [First-hit hitch](archive/ROUND21_2026-09.md#first-hit-hitch)
- [Ledge map](archive/ROUND21_2026-09.md#ledge-map)
- [Save/load crash (NUM_FOR_EDICT)](archive/ROUND21_2026-09.md#saveload-crash-num_for_edict)
- [Held props: grip modes, live offsets, palm grip, torch handle](archive/ROUND21_2026-09.md#held-props-grip-modes-live-offsets-palm-grip-torch-handle)
- [Grapple: unreel; rope drawn in one piece](archive/ROUND21_2026-09.md#grapple-unreel-rope-drawn-in-one-piece)
- [Debug menu; quad sound; grenade catch default; no empty-hand deflection](archive/ROUND21_2026-09.md#debug-menu-quad-sound-grenade-catch-default-no-empty-hand-deflection)
- [Hands: both work; props through teleporters; climbing stamina](archive/ROUND21_2026-09.md#hands-both-work-props-through-teleporters-climbing-stamina)
- [Hand grenades from the back pouch](archive/ROUND21_2026-09.md#hand-grenades-from-the-back-pouch)
- [Holster draw blend; holster defaults; body calibration kept](archive/ROUND21_2026-09.md#holster-draw-blend-holster-defaults-body-calibration-kept)
- [Hardcoded limits audit](archive/ROUND21_2026-09.md#hardcoded-limits-audit)
- [Precise hit detection (models, not boxes)](archive/ROUND21_2026-09.md#precise-hit-detection-models-not-boxes)
- [Faster tests and startup](archive/ROUND21_2026-09.md#faster-tests-and-startup)
- [Engine state cleanup (`static std::`) and QuakeC loops](archive/ROUND21_2026-09.md#engine-state-cleanup-static-std-and-quakec-loops)
- [Grappling hook: one persistent system](archive/ROUND21_2026-09.md#grappling-hook-one-persistent-system)
- [Defaults: the author's liquids, bricks, torch and holsters](archive/ROUND21_2026-09.md#defaults-the-authors-liquids-bricks-torch-and-holsters)
- [Force grab: pulled by the handle, eased into the hand](archive/ROUND21_2026-09.md#force-grab-pulled-by-the-handle-eased-into-the-hand)
- [Hand grenades: unarmed look; flung props hit as thrown ones](archive/ROUND21_2026-09.md#hand-grenades-unarmed-look-flung-props-hit-as-thrown-ones)
- [Climbing: the hands in sync past the reach; floating platforms](archive/ROUND21_2026-09.md#climbing-the-hands-in-sync-past-the-reach-floating-platforms)
- [Hands and weapons as bodies; the empty hand; torches lit in hand](archive/ROUND21_2026-09.md#hands-and-weapons-as-bodies-the-empty-hand-torches-lit-in-hand)
- [Tired arms: shaking and heavy hands](archive/ROUND21_2026-09.md#tired-arms-shaking-and-heavy-hands)
- [Defaults: the author's climbing values](archive/ROUND21_2026-09.md#defaults-the-authors-climbing-values)
- [Grapple: a physical rope](archive/ROUND21_2026-09.md#grapple-a-physical-rope)
- [Grenade turn from the pouch; the pouch takes carried pickups](archive/ROUND21_2026-09.md#grenade-turn-from-the-pouch-the-pouch-takes-carried-pickups)
- [Defaults: the author's tired arms; the tired run](archive/ROUND21_2026-09.md#defaults-the-authors-tired-arms-the-tired-run)
- [Flung props: settings, and never you](archive/ROUND21_2026-09.md#flung-props-settings-and-never-you)
- [Pushes by mass; held props meet; a held club never dropped](archive/ROUND21_2026-09.md#pushes-by-mass-held-props-meet-a-held-club-never-dropped)
- [A smaller player hitbox (research and prototype)](archive/ROUND21_2026-09.md#a-smaller-player-hitbox-research-and-prototype)
- [Melee: punches land at once; the empty hammer is quiet](archive/ROUND21_2026-09.md#melee-punches-land-at-once-the-empty-hammer-is-quiet)
- [Bricks in the palm; the grenade pouch's turn; the firing range's prop area](archive/ROUND21_2026-09.md#bricks-in-the-palm-the-grenade-pouchs-turn-the-firing-ranges-prop-area)
- [In-game checklist](archive/ROUND21_2026-09.md#in-game-checklist)
- [Grapple round 2: controls, a rope that wraps, and what hangs on it](archive/ROUND21_2026-09.md#grapple-round-2-controls-a-rope-that-wraps-and-what-hangs-on-it)
- [Held props: no gap after two hands](archive/ROUND21_2026-09.md#held-props-no-gap-after-two-hands)
- [Melee tuned on your new takes (2026-09-29)](archive/ROUND21_2026-09.md#melee-tuned-on-your-new-takes-2026-09-29)
- [Menu button back to the game; Ironwail's HUD; flung props hurt players; the pouch grenade's fixed turn](archive/ROUND21_2026-09.md#menu-button-back-to-the-game-ironwails-hud-flung-props-hurt-players-the-pouch-grenades-fixed-turn)
- [A smaller player hitbox, round 2: everywhere, two methods, a settings page](archive/ROUND21_2026-09.md#a-smaller-player-hitbox-round-2-everywhere-two-methods-a-settings-page)
- [Never stuck: buttons that return into you, and a safety net](archive/ROUND21_2026-09.md#never-stuck-buttons-that-return-into-you-and-a-safety-net)
- [Held props against weapons, monsters and walls; batting by weapon mass](archive/ROUND21_2026-09.md#held-props-against-weapons-monsters-and-walls-batting-by-weapon-mass)
- [Shots push props (2026-09-30)](archive/ROUND21_2026-09.md#shots-push-props-2026-09-30)
- [Standing on props: explosive boxes you can stand on, stack and jump from](archive/ROUND21_2026-09.md#standing-on-props-explosive-boxes-you-can-stand-on-stack-and-jump-from)
- [Standing on props 2: no blow-ups, shoving by weight, never trapped, two hands stop at walls](archive/ROUND21_2026-09.md#standing-on-props-2-no-blow-ups-shoving-by-weight-never-trapped-two-hands-stop-at-walls)
- [Standing on props 3: tilted boxes as drawn, round against boxes](archive/ROUND21_2026-09.md#standing-on-props-3-tilted-boxes-as-drawn-round-against-boxes)
- [Fast melee eval: parallel, hidden, not drawn (2026-09-30)](archive/ROUND21_2026-09.md#fast-melee-eval-parallel-hidden-not-drawn-2026-09-30)
- [Grapple round 3: the unreel on X, both buttons, a rope that lies still](archive/ROUND21_2026-09.md#grapple-round-3-the-unreel-on-x-both-buttons-a-rope-that-lies-still)
- [No flat HUD in the headset; the map's flames light torches; the pouch's turn](archive/ROUND21_2026-09.md#no-flat-hud-in-the-headset-the-maps-flames-light-torches-the-pouchs-turn)
- [Eval determinism and the world mesh made once (2026-09-30)](archive/ROUND21_2026-09.md#eval-determinism-and-the-world-mesh-made-once-2026-09-30)
- [Throws leave the hand clean (2026-09-30)](archive/ROUND21_2026-09.md#throws-leave-the-hand-clean-2026-09-30)
- [Melee speed 3; reloads aren't blows (2026-09-30)](archive/ROUND21_2026-09.md#melee-speed-3-reloads-arent-blows-2026-09-30)
- [Held weapons and props against the level: they rest on a table; things on the palm](archive/ROUND21_2026-09.md#held-weapons-and-props-against-the-level-they-rest-on-a-table-things-on-the-palm)
- [Player hitbox defaults: 16 wide, the compiled hull; shots hit a 24-wide box](archive/ROUND21_2026-09.md#player-hitbox-defaults-16-wide-the-compiled-hull-shots-hit-a-24-wide-box)
- [Flashlight: lit but not taken (2026-09-30)](archive/ROUND21_2026-09.md#flashlight-lit-but-not-taken-2026-09-30)
- [Grapple round 4: X's slack, the phantom rope, reel-in, towing while reeling, the front button's reel (2026-09-30)](archive/ROUND21_2026-09.md#grapple-round-4-xs-slack-the-phantom-rope-reel-in-towing-while-reeling-the-front-buttons-reel-2026-09-30)
- [The ogres' chainsaw](archive/ROUND21_2026-09.md#the-ogres-chainsaw)
- [Monster hitboxes: their own widths against walls (off by default); a compiled hull bug fixed](archive/ROUND21_2026-09.md#monster-hitboxes-their-own-widths-against-walls-off-by-default-a-compiled-hull-bug-fixed)
- [Eval shards' crash; the eval without a map loaded first (2026-09-30)](archive/ROUND21_2026-09.md#eval-shards-crash-the-eval-without-a-map-loaded-first-2026-09-30)
- [Motion recorder: record button choices (2026-09-30)](archive/ROUND21_2026-09.md#motion-recorder-record-button-choices-2026-09-30)
- [Defaults of 2026-09-30; the dialog stuck to the face; the flat HUD on the desktop views; headshot sounds (2026-09-30)](archive/ROUND21_2026-09.md#defaults-of-2026-09-30-the-dialog-stuck-to-the-face-the-flat-hud-on-the-desktop-views-headshot-sounds-2026-09-30)
- [The chainsaw: on the firing range, cuts gibs and heads, a melee weapon, the author's offsets (2026-09-30)](archive/ROUND21_2026-09.md#the-chainsaw-on-the-firing-range-cuts-gibs-and-heads-a-melee-weapon-the-authors-offsets-2026-09-30)
- [Profiling pass: your e2m1 capture (2026-09-30)](archive/ROUND21_2026-09.md#profiling-pass-your-e2m1-capture-2026-09-30)
- [A held prop never pushes its own hand; the empty hand stops at it; boxes taken at the fist (2026-09-30)](archive/ROUND21_2026-09.md#a-held-prop-never-pushes-its-own-hand-the-empty-hand-stops-at-it-boxes-taken-at-the-fist-2026-09-30)
- [Heavy weapons: wrenched out, sticky grips, heavy melee (2026-09-30)](archive/ROUND21_2026-09.md#heavy-weapons-wrenched-out-sticky-grips-heavy-melee-2026-09-30)
- [Wall torches: one model, the old wood; a lenient grab; the quick reel-in follows the rope (2026-09-30)](archive/ROUND21_2026-09.md#wall-torches-one-model-the-old-wood-a-lenient-grab-the-quick-reel-in-follows-the-rope-2026-09-30)
- [The weight spring's snap-back never turned: a whole turn after a wrist snap (2026-09-30)](archive/ROUND21_2026-09.md#the-weight-springs-snap-back-never-turned-a-whole-turn-after-a-wrist-snap-2026-09-30)
- [The chainsaw: two hotspots, recorded sounds (2026-09-30)](archive/ROUND21_2026-09.md#the-chainsaw-two-hotspots-recorded-sounds-2026-09-30)
- [QuakeC: `x = a && b` is `(x = a) && b` (2026-09-30)](archive/ROUND21_2026-09.md#quakec-x--a--b-is-x--a--b-2026-09-30)
- [VR Calibration: a first-time setup and a calibration room (2026-09-30)](archive/ROUND21_2026-09.md#vr-calibration-a-first-time-setup-and-a-calibration-room-2026-09-30)
- [The game's thread pool (Zancle); the grasp solve shared out (2026-09-30)](archive/ROUND21_2026-09.md#the-games-thread-pool-zancle-the-grasp-solve-shared-out-2026-09-30)
- [Pain feedback: hits knock the hands (2026-09-30)](archive/ROUND21_2026-09.md#pain-feedback-hits-knock-the-hands-2026-09-30)
- [Lightning gun in water (2026-09-30)](archive/ROUND21_2026-09.md#lightning-gun-in-water-2026-09-30)
- [Enemy weapons: the grunts' shotguns and the enforcers' laser rifles](archive/ROUND21_2026-09.md#enemy-weapons-the-grunts-shotguns-and-the-enforcers-laser-rifles)
- [The grunts' burst rifles; the enforcer rifle's faster lasers](archive/ROUND21_2026-09.md#the-grunts-burst-rifles-the-enforcer-rifles-faster-lasers)
- [clang-cl: the whole engine, C++23 (trial, 2026-09-30)](archive/ROUND21_2026-09.md#clang-cl-the-whole-engine-c23-trial-2026-09-30)
- [Left-handed: only the off hand (2026-09-30)](archive/ROUND21_2026-09.md#left-handed-only-the-off-hand-2026-09-30)
- [Map load and voice notes on the thread pool (2026-09-30)](archive/ROUND21_2026-09.md#map-load-and-voice-notes-on-the-thread-pool-2026-09-30)
- [Thrown axes stick (2026-09-30)](archive/ROUND21_2026-09.md#thrown-axes-stick-2026-09-30)
- [Prop size; Mjolnir in water; chainsaw pulls; defaults (2026-09-30)](archive/ROUND21_2026-09.md#prop-size-mjolnir-in-water-chainsaw-pulls-defaults-2026-09-30)
- [Climbing: the leniency is the one reach (2026-09-30)](archive/ROUND21_2026-09.md#climbing-the-leniency-is-the-one-reach-2026-09-30)
- [Phasing through a toppled box (2026-09-30)](archive/ROUND21_2026-09.md#phasing-through-a-toppled-box-2026-09-30)
- [Physics sounds: knocks, scrapes, the climbing grab (2026-09-30)](archive/ROUND21_2026-09.md#physics-sounds-knocks-scrapes-the-climbing-grab-2026-09-30)
- [The main menu in Quake's lettering; the laser on the menu before a map (2026-09-30)](archive/ROUND21_2026-09.md#the-main-menu-in-quakes-lettering-the-laser-on-the-menu-before-a-map-2026-09-30)
- [The diff with Ironwail made smaller (2026-09-30)](archive/ROUND21_2026-09.md#the-diff-with-ironwail-made-smaller-2026-09-30)
- [Thrown axes: blade leniency; spin in the air (2026-09-30)](archive/ROUND21_2026-09.md#thrown-axes-blade-leniency-spin-in-the-air-2026-09-30)
- [Spatial audio (Steam Audio) (2026-09-30)](archive/ROUND21_2026-09.md#spatial-audio-steam-audio-2026-09-30)
- [The double shotgun's fore-end: UVs and the hole (2026-09-30)](archive/ROUND21_2026-09.md#the-double-shotguns-fore-end-uvs-and-the-hole-2026-09-30)
- [The crowbar (2026-09-30)](archive/ROUND21_2026-09.md#the-crowbar-2026-09-30)
- [Wooden crates (2026-09-30)](archive/ROUND21_2026-09.md#wooden-crates-2026-09-30)

### [archive/ROUND21_2026-10-01_03.md](archive/ROUND21_2026-10-01_03.md)

- [A crowbar on the crates (2026-10-01)](archive/ROUND21_2026-10-01_03.md#a-crowbar-on-the-crates-2026-10-01)
- [Crowbar follow-ups: weapons taken by the fist, weapons' masses, the chainsaw's blows, a nailgun put back (2026-10-01)](archive/ROUND21_2026-10-01_03.md#crowbar-follow-ups-weapons-taken-by-the-fist-weapons-masses-the-chainsaws-blows-a-nailgun-put-back-2026-10-01)
- [The chainsaw: an empty trigger clicks; zombies go down and are gibbed (2026-10-01)](archive/ROUND21_2026-10-01_03.md#the-chainsaw-an-empty-trigger-clicks-zombies-go-down-and-are-gibbed-2026-10-01)
- [The enemy guns' detail pass; the author's settings for them (2026-10-01)](archive/ROUND21_2026-10-01_03.md#the-enemy-guns-detail-pass-the-authors-settings-for-them-2026-10-01)
- [Zancle migration: the VR code off the standard library (2026-10-01)](archive/ROUND21_2026-10-01_03.md#zancle-migration-the-vr-code-off-the-standard-library-2026-10-01)
- [Flies round a severed head (2026-10-01)](archive/ROUND21_2026-10-01_03.md#flies-round-a-severed-head-2026-10-01)
- [Was the Zancle migration worth it? (2026-10-01)](archive/ROUND21_2026-10-01_03.md#was-the-zancle-migration-worth-it-2026-10-01)
- [Throws with the trigger held; wrenched out falls slower (2026-10-01)](archive/ROUND21_2026-10-01_03.md#throws-with-the-trigger-held-wrenched-out-falls-slower-2026-10-01)
- [Ammo and health boxes drawn at their final size from the first frame (2026-10-01)](archive/ROUND21_2026-10-01_03.md#ammo-and-health-boxes-drawn-at-their-final-size-from-the-first-frame-2026-10-01)
- [Weapon Damage menu (2026-10-01)](archive/ROUND21_2026-10-01_03.md#weapon-damage-menu-2026-10-01)
- [Spatial audio: bilinear crackle, distance, physics volume, the author's defaults (2026-10-01)](archive/ROUND21_2026-10-01_03.md#spatial-audio-bilinear-crackle-distance-physics-volume-the-authors-defaults-2026-10-01)
- [The enemy guns carved, the rifle symmetric (2026-10-01)](archive/ROUND21_2026-10-01_03.md#the-enemy-guns-carved-the-rifle-symmetric-2026-10-01)
- [Misc: mid-air leniency, torch buttons, pain feedback second pass, LG splashes, defaults (2026-10-01)](archive/ROUND21_2026-10-01_03.md#misc-mid-air-leniency-torch-buttons-pain-feedback-second-pass-lg-splashes-defaults-2026-10-01)
- [Torso direction: the head leads, the hands only in front pull (2026-10-01)](archive/ROUND21_2026-10-01_03.md#torso-direction-the-head-leads-the-hands-only-in-front-pull-2026-10-01)
- [Box3D on the pool (2026-10-01)](archive/ROUND21_2026-10-01_03.md#box3d-on-the-pool-2026-10-01)
- [Weapon effects: recoil, muzzle flash, tracers (2026-10-01)](archive/ROUND21_2026-10-01_03.md#weapon-effects-recoil-muzzle-flash-tracers-2026-10-01)
- [Grappling hook: a turned prop is bitten by its real shape (2026-10-01)](archive/ROUND21_2026-10-01_03.md#grappling-hook-a-turned-prop-is-bitten-by-its-real-shape-2026-10-01)
- [Missiles meet a turned prop's shape (2026-10-01)](archive/ROUND21_2026-10-01_03.md#missiles-meet-a-turned-props-shape-2026-10-01)
- [Phasing through a box toppled in both hands (2026-10-01)](archive/ROUND21_2026-10-01_03.md#phasing-through-a-box-toppled-in-both-hands-2026-10-01)
- [Zancle update to 4ed9c3cc (2026-10-01)](archive/ROUND21_2026-10-01_03.md#zancle-update-to-4ed9c3cc-2026-10-01)
- [Prop settings for a held weapon; Estimated Mass x; rocks and bricks 1.25; the thumb round them; force grab misses; the empty hand against a prop (2026-10-01)](archive/ROUND21_2026-10-01_03.md#prop-settings-for-a-held-weapon-estimated-mass-x-rocks-and-bricks-125-the-thumb-round-them-force-grab-misses-the-empty-hand-against-a-prop-2026-10-01)
- [Crates: monsters, damage, held crates, texture (2026-10-01)](archive/ROUND21_2026-10-01_03.md#crates-monsters-damage-held-crates-texture-2026-10-01)
- [The empty hand against the other hand's weapon: the winding read right (2026-10-01)](archive/ROUND21_2026-10-01_03.md#the-empty-hand-against-the-other-hands-weapon-the-winding-read-right-2026-10-01)
- [Props regressions: the "Standing on props 2" tests again (2026-10-01)](archive/ROUND21_2026-10-01_03.md#props-regressions-the-standing-on-props-2-tests-again-2026-10-01)
- [No function-local statics (2026-10-01)](archive/ROUND21_2026-10-01_03.md#no-function-local-statics-2026-10-01)
- [Thrown axes: the blade decides (2026-10-01)](archive/ROUND21_2026-10-01_03.md#thrown-axes-the-blade-decides-2026-10-01)
- [Zancle follow-ups: no UniqueLock, no exceptions, no shared_ptr, buffers in place (2026-10-01)](archive/ROUND21_2026-10-01_03.md#zancle-follow-ups-no-uniquelock-no-exceptions-no-shared_ptr-buffers-in-place-2026-10-01)
- [The author's sound and climbing values; full body; flies (2026-10-01)](archive/ROUND21_2026-10-01_03.md#the-authors-sound-and-climbing-values-full-body-flies-2026-10-01)
- [A chainsaw let go runs on; a thrown lightning gun's beam ends with it (2026-10-01)](archive/ROUND21_2026-10-01_03.md#a-chainsaw-let-go-runs-on-a-thrown-lightning-guns-beam-ends-with-it-2026-10-01)
- [Gib speed by what gibbed it; monster drops harmless at first; damage numbers everywhere (2026-10-01)](archive/ROUND21_2026-10-01_03.md#gib-speed-by-what-gibbed-it-monster-drops-harmless-at-first-damage-numbers-everywhere-2026-10-01)
- [Weapons taken by their hotspots (2026-10-01)](archive/ROUND21_2026-10-01_03.md#weapons-taken-by-their-hotspots-2026-10-01)
- [A thrown axe's spin: the runtime's angular velocity frame (2026-10-01)](archive/ROUND21_2026-10-01_03.md#a-thrown-axes-spin-the-runtimes-angular-velocity-frame-2026-10-01)
- [The runtime's angular velocity frame, fixed for every consumer (2026-10-01)](archive/ROUND21_2026-10-01_03.md#the-runtimes-angular-velocity-frame-fixed-for-every-consumer-2026-10-01)
- [Crates: wood dust, stacks, a grittier texture (2026-10-01)](archive/ROUND21_2026-10-01_03.md#crates-wood-dust-stacks-a-grittier-texture-2026-10-01)
- [The enforcers' rifle: low zeroed sights, its shots along its barrel, recoil, muzzle flashes (2026-10-01)](archive/ROUND21_2026-10-01_03.md#the-enforcers-rifle-low-zeroed-sights-its-shots-along-its-barrel-recoil-muzzle-flashes-2026-10-01)
- [The thumb round a hand grenade; the empty hand meets a weapon as a held prop does (2026-10-01)](archive/ROUND21_2026-10-01_03.md#the-thumb-round-a-hand-grenade-the-empty-hand-meets-a-weapon-as-a-held-prop-does-2026-10-01)
- [Sliding off steep boxes (2026-10-01)](archive/ROUND21_2026-10-01_03.md#sliding-off-steep-boxes-2026-10-01)
- [Climbing: a jump to a ledge from against its wall (2026-10-01)](archive/ROUND21_2026-10-01_03.md#climbing-a-jump-to-a-ledge-from-against-its-wall-2026-10-01)
- [Elbow near the face and the chest (2026-10-01)](archive/ROUND21_2026-10-01_03.md#elbow-near-the-face-and-the-chest-2026-10-01)
- [Multi-grenades from the pouch (2026-10-01)](archive/ROUND21_2026-10-01_03.md#multi-grenades-from-the-pouch-2026-10-01)
- [Shooting grenades (2026-10-01)](archive/ROUND21_2026-10-01_03.md#shooting-grenades-2026-10-01)
- [Grunts and enforcers shove you (2026-10-01)](archive/ROUND21_2026-10-01_03.md#grunts-and-enforcers-shove-you-2026-10-01)
- [Enemy shoves: parry, enforcers, knocked off a hold (2026-10-01)](archive/ROUND21_2026-10-01_03.md#enemy-shoves-parry-enforcers-knocked-off-a-hold-2026-10-01)
- [Weapons held anywhere (2026-10-01)](archive/ROUND21_2026-10-01_03.md#weapons-held-anywhere-2026-10-01)
- [Drop-down lists in the VR menus (2026-10-01)](archive/ROUND21_2026-10-01_03.md#drop-down-lists-in-the-vr-menus-2026-10-01)
- [The running chainsaw smokes and shakes (2026-10-01)](archive/ROUND21_2026-10-01_03.md#the-running-chainsaw-smokes-and-shakes-2026-10-01)
- [Defaults: gibs, pain knock, grenade grips; mantle grunt; prop pile; one empty-hand collision (2026-10-01)](archive/ROUND21_2026-10-01_03.md#defaults-gibs-pain-knock-grenade-grips-mantle-grunt-prop-pile-one-empty-hand-collision-2026-10-01)
- [Combat 4: grenade blows, parry bursts, your own throws (2026-10-01)](archive/ROUND21_2026-10-01_03.md#combat-4-grenade-blows-parry-bursts-your-own-throws-2026-10-01)
- [The chainsaw's cord pulls give feedback; the author's smoke and shake (2026-10-01)](archive/ROUND21_2026-10-01_03.md#the-chainsaws-cord-pulls-give-feedback-the-authors-smoke-and-shake-2026-10-01)
- [Mantle grunt sound, Physics Stress, axe stick speed (2026-10-01)](archive/ROUND21_2026-10-01_03.md#mantle-grunt-sound-physics-stress-axe-stick-speed-2026-10-01)
- [Grips reset with the weapon (2026-10-01)](archive/ROUND21_2026-10-01_03.md#grips-reset-with-the-weapon-2026-10-01)
- [Handedness: separate options (2026-10-01)](archive/ROUND21_2026-10-01_03.md#handedness-separate-options-2026-10-01)
- [The mix's limiter: explosions piling up without the crackle (2026-10-01)](archive/ROUND21_2026-10-01_03.md#the-mixs-limiter-explosions-piling-up-without-the-crackle-2026-10-01)
- [Throws by weight (2026-10-02)](archive/ROUND21_2026-10-01_03.md#throws-by-weight-2026-10-02)
- [Two-handed gib throws that burst (2026-10-02)](archive/ROUND21_2026-10-01_03.md#two-handed-gib-throws-that-burst-2026-10-02)
- [Heavy throws' hits, spin and two-handed throws (2026-10-02)](archive/ROUND21_2026-10-01_03.md#heavy-throws-hits-spin-and-two-handed-throws-2026-10-02)
- [The laser cannon re-mapped and carved (2026-10-01)](archive/ROUND21_2026-10-01_03.md#the-laser-cannon-re-mapped-and-carved-2026-10-01)
- [Zancle update to 534219bf (2026-10-01)](archive/ROUND21_2026-10-01_03.md#zancle-update-to-534219bf-2026-10-01)
- [Combat 5: parry cooldown, gentle releases (2026-10-01)](archive/ROUND21_2026-10-01_03.md#combat-5-parry-cooldown-gentle-releases-2026-10-01)
- [A weapon held anywhere strikes with all of it; a sword's whole blade meets the other weapon (2026-10-02)](archive/ROUND21_2026-10-01_03.md#a-weapon-held-anywhere-strikes-with-all-of-it-a-swords-whole-blade-meets-the-other-weapon-2026-10-02)
- [Slow motion (2026-10-02)](archive/ROUND21_2026-10-01_03.md#slow-motion-2026-10-02)
- [Slow motion: bullet time and Sandevistan (2026-10-02)](archive/ROUND21_2026-10-01_03.md#slow-motion-bullet-time-and-sandevistan-2026-10-02)
- [Spectator camera: the eyes' glow ghost, a wider field of view (2026-10-02)](archive/ROUND21_2026-10-01_03.md#spectator-camera-the-eyes-glow-ghost-a-wider-field-of-view-2026-10-02)
- [Head-locked text out of the recording (2026-10-02)](archive/ROUND21_2026-10-01_03.md#head-locked-text-out-of-the-recording-2026-10-02)
- [Weapon Offsets split; the virtual stock's turn (2026-10-02)](archive/ROUND21_2026-10-01_03.md#weapon-offsets-split-the-virtual-stocks-turn-2026-10-02)
- [Defaults: chainsaw pull, mantle grunt, prop weights, crates (2026-10-02)](archive/ROUND21_2026-10-01_03.md#defaults-chainsaw-pull-mantle-grunt-prop-weights-crates-2026-10-02)
- [Lightning arcs, enforcer rifle smoke, the chainsaw's handle in the hand (2026-10-02)](archive/ROUND21_2026-10-01_03.md#lightning-arcs-enforcer-rifle-smoke-the-chainsaws-handle-in-the-hand-2026-10-02)
- [Combat 6: every melee monster parries; one attack, one parry (2026-10-02)](archive/ROUND21_2026-10-01_03.md#combat-6-every-melee-monster-parries-one-attack-one-parry-2026-10-02)
- [Wall buttons: weapons and thrown things press them (2026-10-02)](archive/ROUND21_2026-10-01_03.md#wall-buttons-weapons-and-thrown-things-press-them-2026-10-02)
- [Zancle update to 304ea6c3 (2026-10-02)](archive/ROUND21_2026-10-01_03.md#zancle-update-to-304ea6c3-2026-10-02)
- [Weight: the wrist's roll (2026-10-02)](archive/ROUND21_2026-10-01_03.md#weight-the-wrists-roll-2026-10-02)
- [Bullet time: the wrist tap (2026-10-02)](archive/ROUND21_2026-10-01_03.md#bullet-time-the-wrist-tap-2026-10-02)
- [Flashlight: lit, gripped, nothing (2026-10-02)](archive/ROUND21_2026-10-01_03.md#flashlight-lit-gripped-nothing-2026-10-02)
- [Taking a carried sword back with the trigger pulled; no flick reload in two hands (2026-10-02)](archive/ROUND21_2026-10-01_03.md#taking-a-carried-sword-back-with-the-trigger-pulled-no-flick-reload-in-two-hands-2026-10-02)
- [Physics threads benchmarked; Physics Stress defaults (2026-10-02)](archive/ROUND21_2026-10-01_03.md#physics-threads-benchmarked-physics-stress-defaults-2026-10-02)
- [Trailer tools: highlight markers and a rough cut (2026-10-02)](archive/ROUND21_2026-10-01_03.md#trailer-tools-highlight-markers-and-a-rough-cut-2026-10-02)
- [Grittier debris, crates and crowbar (2026-10-02)](archive/ROUND21_2026-10-01_03.md#grittier-debris-crates-and-crowbar-2026-10-02)
- [Crates: deeper gaps, edges and nails (2026-10-02)](archive/ROUND21_2026-10-01_03.md#crates-deeper-gaps-edges-and-nails-2026-10-02)
- [Slow motion's sound for editing: the game-time sound file (2026-10-02)](archive/ROUND21_2026-10-01_03.md#slow-motions-sound-for-editing-the-game-time-sound-file-2026-10-02)
- [Flashlight: grabbing on the move; leniency and clipping on when let go (2026-10-02)](archive/ROUND21_2026-10-01_03.md#flashlight-grabbing-on-the-move-leniency-and-clipping-on-when-let-go-2026-10-02)
- [Flashlight: gun clip range; each weapon's own place for it (2026-10-02)](archive/ROUND21_2026-10-01_03.md#flashlight-gun-clip-range-each-weapons-own-place-for-it-2026-10-02)
- [HRTF balance: Quake's 11 kHz lowpass folded the voices' highs down (2026-10-02)](archive/ROUND21_2026-10-01_03.md#hrtf-balance-quakes-11-khz-lowpass-folded-the-voices-highs-down-2026-10-02)
- [Full-band sound: the voices round Quake's 11 kHz lowpass (2026-10-02)](archive/ROUND21_2026-10-01_03.md#full-band-sound-the-voices-round-quakes-11-khz-lowpass-2026-10-02)
- [Gore: bloody hands, washing, dying bodies, blood mist (2026-10-02)](archive/ROUND21_2026-10-01_03.md#gore-bloody-hands-washing-dying-bodies-blood-mist-2026-10-02)
- [Small gibs (2026-10-02)](archive/ROUND21_2026-10-01_03.md#small-gibs-2026-10-02)
- [Melee: a wiggled gun's butt strike (wigglefix, 2026-10-02)](archive/ROUND21_2026-10-01_03.md#melee-a-wiggled-guns-butt-strike-wigglefix-2026-10-02)
- [Flashlight: the gun's zone at its mount; a grab sound; the author's defaults (2026-10-02)](archive/ROUND21_2026-10-01_03.md#flashlight-the-guns-zone-at-its-mount-a-grab-sound-the-authors-defaults-2026-10-02)
- [Near clip: a gun at the eye (2026-10-02)](archive/ROUND21_2026-10-01_03.md#near-clip-a-gun-at-the-eye-2026-10-02)
- [Your own wounds: finer, smooth, with relief (2026-10-02)](archive/ROUND21_2026-10-01_03.md#your-own-wounds-finer-smooth-with-relief-2026-10-02)
- [Defaults: pain knock, throw damage, roll weight; spawn buttons' green; grunts' smoke; swim pitch (misc11, 2026-10-02)](archive/ROUND21_2026-10-01_03.md#defaults-pain-knock-throw-damage-roll-weight-spawn-buttons-green-grunts-smoke-swim-pitch-misc11-2026-10-02)
- [Burning: flames spreading over the body, corpses, touch, lava nails (2026-10-02)](archive/ROUND21_2026-10-01_03.md#burning-flames-spreading-over-the-body-corpses-touch-lava-nails-2026-10-02)
- [Wall torches: a grip on the way takes it (2026-10-02)](archive/ROUND21_2026-10-01_03.md#wall-torches-a-grip-on-the-way-takes-it-2026-10-02)
- [Blood on you, your weapons and props (2026-10-02)](archive/ROUND21_2026-10-01_03.md#blood-on-you-your-weapons-and-props-2026-10-02)
- [Weapon instances: each weapon one record, its id and its magazine; its blood goes with it (2026-10-02)](archive/ROUND21_2026-10-01_03.md#weapon-instances-each-weapon-one-record-its-id-and-its-magazine-its-blood-goes-with-it-2026-10-02)
- [A full broadcast drops whole messages (szoverflow, 2026-10-02)](archive/ROUND21_2026-10-01_03.md#a-full-broadcast-drops-whole-messages-szoverflow-2026-10-02)
- [External material maps: Quetoo's normal, specular and glow maps (2026-10-02)](archive/ROUND21_2026-10-01_03.md#external-material-maps-quetoos-normal-specular-and-glow-maps-2026-10-02)
- [Spatial audio: optimised (2026-10-02)](archive/ROUND21_2026-10-01_03.md#spatial-audio-optimised-2026-10-02)
- [parallelFor sites: what each costs, and the small ones on the caller (2026-10-02)](archive/ROUND21_2026-10-01_03.md#parallelfor-sites-what-each-costs-and-the-small-ones-on-the-caller-2026-10-02)
- [Flashlight: a gritty, rusted torch; a chain for its cord (2026-10-02)](archive/ROUND21_2026-10-01_03.md#flashlight-a-gritty-rusted-torch-a-chain-for-its-cord-2026-10-02)
- [Flashlight: easing onto a gun or the head; which weapons it clips on (2026-10-02)](archive/ROUND21_2026-10-01_03.md#flashlight-easing-onto-a-gun-or-the-head-which-weapons-it-clips-on-2026-10-02)
- [Black patches standing in flames; nails' fizz (2026-10-02)](archive/ROUND21_2026-10-01_03.md#black-patches-standing-in-flames-nails-fizz-2026-10-02)
- [Defaults: the author's config, 2026-10-02 evening](archive/ROUND21_2026-10-01_03.md#defaults-the-authors-config-2026-10-02-evening)
- [Blood on every prop you hold: boxes too (2026-10-02)](archive/ROUND21_2026-10-01_03.md#blood-on-every-prop-you-hold-boxes-too-2026-10-02)
- [Small gibs pushed out of the body they came from; thrown gibs that never stuck (2026-10-02)](archive/ROUND21_2026-10-01_03.md#small-gibs-pushed-out-of-the-body-they-came-from-thrown-gibs-that-never-stuck-2026-10-02)
- [Retro textures, phase 1: the shader and the World (2026-10-02)](archive/ROUND21_2026-10-01_03.md#retro-textures-phase-1-the-shader-and-the-world-2026-10-02)
- [Retro textures, phase 2: every kind of thing drawn (2026-10-02)](archive/ROUND21_2026-10-01_03.md#retro-textures-phase-2-every-kind-of-thing-drawn-2026-10-02)
- [Retro textures, phase 3: per-object overrides and the in-game editor (2026-10-02)](archive/ROUND21_2026-10-01_03.md#retro-textures-phase-3-per-object-overrides-and-the-in-game-editor-2026-10-02)
- [Small gibs batted by the blade; fresh gibs that hurt you (2026-10-02)](archive/ROUND21_2026-10-01_03.md#small-gibs-batted-by-the-blade-fresh-gibs-that-hurt-you-2026-10-02)
- [Quetoo's maps shipped: on by default (2026-10-03)](archive/ROUND21_2026-10-01_03.md#quetoos-maps-shipped-on-by-default-2026-10-03)
- [Monster drops flung into you (2026-10-03)](archive/ROUND21_2026-10-01_03.md#monster-drops-flung-into-you-2026-10-03)
- [Retro textures, phase 4: All Categories, your body by part, decals, particles and sprites (2026-10-03)](archive/ROUND21_2026-10-01_03.md#retro-textures-phase-4-all-categories-your-body-by-part-decals-particles-and-sprites-2026-10-03)
- [Retro lighting: the light in Quake's coarse look (2026-10-03)](archive/ROUND21_2026-10-01_03.md#retro-lighting-the-light-in-quakes-coarse-look-2026-10-03)
- [HQ texture pack as a PNG release (2026-10-03)](archive/ROUND21_2026-10-01_03.md#hq-texture-pack-as-a-png-release-2026-10-03)
- [The training dummy bleeds as a grunt (2026-10-03)](archive/ROUND21_2026-10-01_03.md#the-training-dummy-bleeds-as-a-grunt-2026-10-03)
- [Burning, part 3: charred and burning pieces; explosive boxes are metal (2026-10-03)](archive/ROUND21_2026-10-01_03.md#burning-part-3-charred-and-burning-pieces-explosive-boxes-are-metal-2026-10-03)
- [Blood on holstered weapons, on things lying near, healing in chunky mode, the side a prop struck with (2026-10-03)](archive/ROUND21_2026-10-01_03.md#blood-on-holstered-weapons-on-things-lying-near-healing-in-chunky-mode-the-side-a-prop-struck-with-2026-10-03)
- [Low-poly chain cord; retro Smooth Beyond: Never; held props press wall buttons (2026-10-03)](archive/ROUND21_2026-10-01_03.md#low-poly-chain-cord-retro-smooth-beyond-never-held-props-press-wall-buttons-2026-10-03)
- [Two-handed throws of big gibs (2026-10-03)](archive/ROUND21_2026-10-01_03.md#two-handed-throws-of-big-gibs-2026-10-03)
- [Explosive boxes are metal (2026-10-03)](archive/ROUND21_2026-10-01_03.md#explosive-boxes-are-metal-2026-10-03)
- [Torch flames: upright from the head's top, swings, smoke, burning you (2026-10-03)](archive/ROUND21_2026-10-01_03.md#torch-flames-upright-from-the-heads-top-swings-smoke-burning-you-2026-10-03)
- [Corpse collision (2026-10-03)](archive/ROUND21_2026-10-01_03.md#corpse-collision-2026-10-03)
- [Every prop in both hands (2026-10-03)](archive/ROUND21_2026-10-01_03.md#every-prop-in-both-hands-2026-10-03)
- [Clean weapon skins (2026-10-03)](archive/ROUND21_2026-10-01_03.md#clean-weapon-skins-2026-10-03)
- [Ragdolls (experimental, the grunt only) (2026-10-03)](archive/ROUND21_2026-10-01_03.md#ragdolls-experimental-the-grunt-only-2026-10-03)
- [Ragdolls 2: taking them, piles, blood and fire, the unseen switch, their own page (2026-10-03)](archive/ROUND21_2026-10-01_03.md#ragdolls-2-taking-them-piles-blood-and-fire-the-unseen-switch-their-own-page-2026-10-03)
- [Ragdolls 3: the knight (2026-10-03)](archive/ROUND21_2026-10-01_03.md#ragdolls-3-the-knight-2026-10-03)
- [Ragdolls on; corpse health and damage by kind (2026-10-03)](archive/ROUND21_2026-10-01_03.md#ragdolls-on-corpse-health-and-damage-by-kind-2026-10-03)
- [Ragdolls 4: the ogre, enforcer, death knight, rottweiler and scrag (2026-10-03)](archive/ROUND21_2026-10-01_03.md#ragdolls-4-the-ogre-enforcer-death-knight-rottweiler-and-scrag-2026-10-03)
- [The QC runaway of many blows (2026-10-03)](archive/ROUND21_2026-10-01_03.md#the-qc-runaway-of-many-blows-2026-10-03)
- [Ragdoll masses per monster (2026-10-03)](archive/ROUND21_2026-10-01_03.md#ragdoll-masses-per-monster-2026-10-03)
- [Ragdolls drawn as smoothly as props (2026-10-03)](archive/ROUND21_2026-10-01_03.md#ragdolls-drawn-as-smoothly-as-props-2026-10-03)
- [Two-handed ragdoll throws (2026-10-03)](archive/ROUND21_2026-10-01_03.md#two-handed-ragdoll-throws-2026-10-03)
- [Spectator camera reminder in the menu](archive/ROUND21_2026-10-01_03.md#spectator-camera-reminder-in-the-menu)
- [Hands on held ragdolls (2026-10-03)](archive/ROUND21_2026-10-01_03.md#hands-on-held-ragdolls-2026-10-03)
- [Decapitation (2026-10-03)](archive/ROUND21_2026-10-01_03.md#decapitation-2026-10-03)
- [Head pops: shotgun, super shotgun and lightning gun headshots (2026-10-03)](archive/ROUND21_2026-10-01_03.md#head-pops-shotgun-super-shotgun-and-lightning-gun-headshots-2026-10-03)
- [Shader optimizations: retro and parallax (2026-10-03)](archive/ROUND21_2026-10-01_03.md#shader-optimizations-retro-and-parallax-2026-10-03)
- [Shader optimizations: lights (2026-10-03)](archive/ROUND21_2026-10-01_03.md#shader-optimizations-lights-2026-10-03)
- [Shader optimizations: AO, liquids, bloom (2026-10-03)](archive/ROUND21_2026-10-01_03.md#shader-optimizations-ao-liquids-bloom-2026-10-03)
- [Ragdolls 5: the fiend, shambler, gremlin and mummy (2026-10-03)](archive/ROUND21_2026-10-01_03.md#ragdolls-5-the-fiend-shambler-gremlin-and-mummy-2026-10-03)

## Head pop chance (2026-10-06)

The author: head pops (the section above) happened far too often. They are now by chance, scaling with the damage
(the pellets at the head) and falling with range. Gore > Decapitation > **Head Pop Chance** (QC vr_decap.qc, "Head
pop chance"):
- **By Chance** (`vr_decap_pop_chance` 1; 0: every headshot kill pops, as before).
- A shotgun's or super shotgun's headshot kill pops the head always within **Always Within**
  (`vr_decap_pop_always_range` 3 player lengths; a length is the player's height, 56 units: `VR_DECAP_PLAYER_LENGTH`),
  never at **Never Beyond** (`vr_decap_pop_never_range` 15 lengths, 840 units) or farther, and between by chance:
  scale x (1 - t)^falloff x ((1 - w) + w x share), t the way from the one range to the other, share the blast's
  pellets at the head over the pellets it fired (`vr_decap_pellets`, set by FireBulletsImpl), w **Pellets at the Head**
  (`vr_decap_pop_pellet_weight` 0.75). Super shotgun: **Chance** `vr_decap_pop_ssg_scale` 1.25, **Falloff**
  `vr_decap_pop_ssg_falloff` 1 (straight); shotgun: `vr_decap_pop_sg_scale` 0.75, `vr_decap_pop_sg_falloff` 4 (its tight
  spread keeps all six pellets on the head at range, so its curve has to fall much faster to stay under the super
  shotgun's at every range).
- **Lightning Always Pops** (`vr_decap_pop_lightning_always` 1): any range. Off: the ranges with the super shotgun's falloff.
- **Thrown Things** (`vr_decap_pop_thrown` 1): a blunt weapon (not a sword, an axe or the chainsaw: a thrown axe's edge
  beheads as before) or prop thrown or flung by the player into a head that kills pops it, always when **Always From**
  (`vr_decap_pop_thrown_mass` 4 kg) or heavier, a lighter one at **Lighter's Chance**
  (`vr_decap_pop_thrown_light_chance` 0: never). `VR_Decap_ThrownArm`, from forcegrabbable_touch (thrown weapons and
  props), VR_SolidProp_Impact (crates, explosive boxes) and VR_Prop_Flung. Masses today: rocket launcher 8, super nailgun
  7, Mjolnir 5.5, nailgun 4 (pop); shotgun 3.5 (doesn't); explosive box 80.5.
- Only the killing blow rolls: the chance is kept with the armed blow (`vr_decap_chance`) and rolled when it kills
  (`VR_Decap_Roll`: Killed, a zombie knocked down, a corpse's head shot). `developer 1`: "decap: pop chance 0.42 on
  monster_army (a super shotgun blast, 6 of 14 pellets at the head): rolled 0.61, no pop".
- Unchanged: blunt melee head blows (fists, guns, crowbar, clubs, pommels, Mjolnir swung) still always pop; slashes,
  the chainsaw and thrown axes still behead; nailguns, rockets and grenades never pop.

**Tests** (`vr_decap_test 40..48`, Debug > Gore Tests > Head Pop Chance Tests; `vr_decap_poptest_dist` lengths,
`vr_decap_poptest_n`, `vr_decap_poptest_wid`, `vr_decap_poptest_body`; `vr_decap_pop_roll` fixes the roll), vrfiringrange,
a grunt:
- 40 the chance table. Shotgun, all pellets at the head: 3.5 lengths 0.63, 5 0.36, 7 0.15, 9 0.05, 11 0.01, 13+ 0.
  Super shotgun, all / half / a fifth at the head: 5 lengths 1.00/0.65/0.42, 7 0.83/0.52/0.33, 9 0.62/0.39/0.25,
  11 0.42/0.26/0.17, 13 0.21/0.13/0.08, 15+ 0.
- 41-44 (no spread, health 1): super shotgun at 1 and 3 lengths popped, shotgun at 3 popped; super shotgun at 14.9 lengths
  (chance 0.02) and 20 (0) not popped (gibbed); lightning at 10 and 20 lengths popped; a super shotgun body kill at 2
  lengths not popped.
- 45/46 (200 blasts each with the weapon's spread, not killing; the rate it would pop): super shotgun 3.5 lengths 0.92,
  5 0.43, 7 0.34, 9 0.23, 11 0.14, 13 0.05; shotgun 3.5 0.65, 5 0.24, 7 0.13, 9 0.045, 11 0.005, 13 0. By pellets at
  the head (super shotgun, 7 lengths): 1 pellet 0.26, 3 0.30, 5 0.38 (chance 0.26 .. 0.44 rising with the pellets).
- 47 thrown weapons at the head (16 m/s, health 1): rocket launcher, nailgun, super nailgun, Mjolnir popped; shotgun
  (3.5 kg) not; the rocket launcher at the body not. 48 an explosive box at the head popped, at the body not (gibbed).
- decap_test.sh pop, popoff, popzombie, thrown: as before (close range: always).

- [ ] Super shotgun headshot kills point blank and from 2-3 player lengths (about 4-5 m): the head always pops.
- [ ] From across a room (8-12 lengths): sometimes; more often when the whole blast hits the head.
- [ ] Very long range (15+ lengths, about 26 m) super shotgun headshot kills: never.
- [ ] Shotgun: point blank always; past 3 lengths it pops clearly less often than the super shotgun.
- [ ] Lightning gun headshot kills pop at any range.
- [ ] A rocket launcher or nailgun thrown into a head that kills pops it; a shotgun thrown doesn't.
- [ ] An explosive box or crate thrown into a head that kills pops it.
- [ ] Body kills never pop.
- [ ] The Head Pop Chance rows change it as they say.

## Decapitation 2: a forgiving melee zone, Show Hit Zones, popped bodies stay put (2026-10-04)

The author: melee decapitation was too hard to land; a view of the hit zones to check them; a popped head's ragdoll flew
far too much (the shot's force went into the head).

**Why it was hard** (`vr_decap_test 19`, Debug > Tests > Decapitation Tests > **Sweep the Head Zone**: a striking point,
3 units thick with the melee's tolerance, moved level at the nearest live monster's head axis from 16 sides, at heights
24 below its head's middle to 16 above; where it meets the model, whether that is on the melee's zone): with precise
hits a blow meets the model grown by 9 units (3 + `vr_hit_tolerance_melee` 6), and that place's point on the model is
judged. A level blade at the neck first meets the grown shoulders and trapezius, whose points lie 6-8 units below the
blade and 9-11 out to the side: outside the head sphere. From behind, the grunt's and the enforcer's packs take the blow.

| level blows on the zone | neck (2-10 below the head's middle) | head (0-10 above) | upper chest (12-20 below) |
|---|---|---|---|
| grunt: before / now | 24% / 95% | 92% / 100% | 0% / 39% |
| knight | 21% / 69% | 86% / 94% | 0% / 51% |
| ogre | 38% / 72% | 84% / 91% | 8% / 40% |
| enforcer (his pack) | 14% / 25% | 59% / 60% | 0% / 12% |
| death knight | 24% / 51% | 73% / 82% | 0% / 36% |

(Before: the zone as it was, 1x and no neck; now: 1.25x and 6 units. 1.25x with 8: grunt neck 96%, upper chest 58%; 1.5x
with 8: 99% and 70%.) The price: blows at the top of the chest behead too, as the table's last column shows.

**Gore > Decapitation**, melee only (a slash, the chainsaw's bar, a corpse's head; head pops and thrown axes keep the
head zone, `VR_Decap_OnHead`): **Head Zone Size** (`vr_decap_head_size`, 1.25: the head sphere's radius, VR_HEAD_MARGIN in
with precise hits, times it) and **Neck** (`vr_decap_neck`, 6 units: the zone a capsule from the head's middle down that
far; a ragdoll's neck counts within that many units, at least the 4 it was). QC `VR_Decap_MeleeZone`,
`VR_Decap_OnMeleeHead`; the engine's ragdollhead takes the neck.

**The other checks**, measured on the author's 44 recorded slash takes replayed against the training dummy (his old hand
settings; the struck point of each of the 42 sword blows): its speed 7.0 m/s at the least, 10.4 the tenth percentile,
23.8 the median: **Least Swing Speed** (5 m/s) never refused one. Its motion against the blade's line: 4 of the 40 blows
the melee calls slashes moved within 60 degrees of the line (cosines 0.52 to 0.71), so decapitation called them stabs: a
setting now, **Least Slash Angle** (`vr_decap_slash_angle`, 45 degrees: 1 of the 40 refused; 0 any motion). And 3 of the
42 struck at 0.27-0.33 of the way from the grip to the tip (the blade's root, the melee's blade begins at 0.4): refused as
the hilt; left as it is.

**Head pops**: Gore > Decapitation > Head Shots > **Body's Speed After a Pop** (`vr_decap_pop_body_speed`, 0.1): the
headless ragdoll's parts' motion (and their turning) times this at its first step, after the frame's knocks and pushes
have reached it (ragdolldecap's third argument; vr_box3d.cpp feedRagdoll, `RagdollBodies::settle`). `vr_debug_ragdoll 1`:
"settled after its head popped: fastest part 527 -> 53 u/s". A super shotgun headshot on a grunt (`vr_decap_test 13`):
at 1x his pelvis 417 units away 1.3 s later (off a ledge); at 0.1x 17 units.

**Debug > Show Hit Zones** (`vr_debug_hitzones`: Positional Damage, Decapitation, Both): each live monster's zones as
wireframes on the model standing (as hits are judged: hitmodel_rest), at its drawn place and yaw: positional damage's head
sphere red (VR_HEAD_MARGIN in with precise hits), body green (within 0.35 of its box's width of its middle, above its
origin), legs blue (below it), extremities yellow (out beyond that; drawn to 0.75 of its width, open-ended); decapitation's
melee zone magenta (Head Zone Size, Neck) with the shots' head zone red. QC `VR_Decap_DebugFrame` sends them each server
frame (builtin `debughitzone`), vr_hitmodel.cpp `zonesDraw` draws them each view frame. On a monster in another pose the
zones stay where its standing model has them (the point struck is mapped onto the standing model).

Tests (`bash Misc/quakevr/ragdoll/decap_test.sh <agent> sweep`: the zone swept on six monsters at each Head Zone Size:Neck in
`ZONES`): the table above. Decapitation's cases live, axe, refuse, corpse, saw, zombie, pop, popoff, popzombie: as
before. eval.sh not run (the takes are archived out of `quakevr/motions`).

In VR:
- [ ] Gore > Decapitation has Least Slash Angle, Head Zone Size and Neck.
- [ ] Gore > Decapitation > Head Shots has Body's Speed After a Pop.
- [ ] Slash a grunt across the neck: his head comes off more often than before.
- [ ] Slash a grunt's neck from the side: his head comes off.
- [ ] A slash at a grunt's belly doesn't behead him.
- [ ] Head Zone Size 1 and Neck 0: it is as hard as before.
- [ ] Shotgun a grunt's head: his headless body slumps where he stood.
- [ ] Body's Speed After a Pop 1: the popped body flies as before.
- [ ] Debug > Show Hit Zones > Positional Damage: wireframes on the monsters.
- [ ] Each zone has its colour: head red, body green, extremities yellow, legs blue.
- [ ] Show Hit Zones > Decapitation: a magenta capsule over each head and neck.
- [ ] Changing Head Zone Size grows the magenta capsule.
- [ ] Changing Neck lengthens it.
- [ ] Show Hit Zones > Both: all of them.
- [ ] Show Hit Zones > Off: none.
## Brain chunks (2026-10-04)

The author: a new small gib, a pinkish-grey chunk of brain matter; destroying a head gib or popping a head throws a
customizable number of them.

**The model** (`Misc/quakevr/make_brains.py`, run it then `python Misc/quakevr/bake_normals.py gib_brain1.mdl
gib_brain2.mdl gib_brain3.mdl`): `progs/gib_brain1..3.mdl`, torn lumps of brain 4.4 to 7.2 cm, 1280 triangles each (a
subdivided icosahedron pulled into an ellipsoid and lumped, cut by a rough plane: the torn face, a hard edge round
it). Two skins each: 0 pinkish, 1 greyish (full colour `_0.png`, `_1.png` at 256 x 256; the 8-bit skins in Quake's
mauve, pink, beige and red ramps). The folds are the lines where two value noises of the folds' size cross their
middles (a labyrinth of gyri and narrow sulci, not one noise's nested rings), dark and some bloody in the sulci, fine
vessels over the gyri, blood smeared over it and thicker towards the torn face, which is paler (white matter), rough
and blood-soaked. A baked normal map (`_0_norm.png`, normaltiles.py's "relief" recipe: the generator's own relief,
the folds about 4 mm deep). The engine's gib lists know them by name (`progs/gib` prefix: flesh knocks, the gib
counts; vr_retro.cpp's gib prefixes). The torn face goes onto its plane along each vertex's ray from the middle and
the lumps fade out round its rim (else triangles folded over there: `mdlgen.write_mdl`'s winding check).

**The game** (QC vr_smallgibs.qc, "Brain chunks"): small gibs in all else (`VR_SmallGib_SpawnAs` with a brain:
carried, thrown, bleeding, sticking, squishing, fading, the same cap), a random model and skin, a burst's flight
(`VR_SmallGib_BurstOf`, shared with a large gib's burst), their own size and mass. A head gib destroyed (VR_Gib_Burst:
a gib of `vr_gib_healthmult` 1.5, a head) and a head pop (VR_Decap_PopEffects) throw them besides the burst's small
gibs. Gore > Small Gibs > **Brain Chunks**: **Brain Chunks** (`vr_smallgibs_brains`, on; also off with Small Gibs
off), **From a Head Gib** (`vr_smallgibs_brains_burst`, 6), **From a Head Pop** (`vr_smallgibs_brains_pop`, 8),
**Brain Chunk Size** (`vr_smallgibs_brains_size`, 0.8 times the small gibs'), **Brain Chunk Mass**
(`vr_smallgibs_brains_mass`, 0.15 kg; a small gib 0.3). `developer 1`: "smallgib: brains: 8 (a head pop)". (Brain
chunks fly at the meat gibs' Speed and Up until "Brain chunks' own throw" below: `vr_smallgibs_brains_speed`,
`vr_smallgibs_brains_up`.)

**Debug** > Gore Tests: **Brain Chunks Ahead** (`vr_smallgibs_test 21`: a head pop's worth burst
`vr_smallgibs_test_dist` units ahead, 40, at chest height); 7 (Burst a Gib and a Head) and 11 (List Small Gibs) count
the brain chunks.

Tests:

| case | result |
|---|---|
| test 7: a gib and an ogre's head burst | 16 small gibs, 6 of them brain chunks (From a Head Gib 6) |
| test 21 | 8 brain chunks (From a Head Pop 8) |
| test 11 after both | 36 small gibs, 14 brain chunks, all rigid and asleep, mass 0.15 kg |
| decap test 12 (a shotgun headshot pops a grunt's head) | "smallgib: brains: 8 (a head pop)", popped as before |
| decap test 13 with From a Head Pop 3 | 3 |
| decap test 12 with Brain Chunks off | none, popped as before |
| smallgibs_tests.sh (live grunt) | rates, quad, chainsaw, burst, cap, grace as before |

Mock close-ups (firing range, the mock head lowered): in the air, a cluster of pale pinkish-grey lumps with folds and
blood; on the floor, scattered pinkish and greyish chunks beside the meat. Lighter and paler than the meat gibs, as
brain is.

In VR:
- [ ] Gore > Small Gibs has a Brain Chunks header with Brain Chunks, From a Head Gib, From a Head Pop, Brain Chunk Size
  and Brain Chunk Mass.
- [ ] Shoot a head gib to bursting: brain chunks fly out with the small gibs of meat.
- [ ] Pop a grunt's head with a shotgun headshot: brain chunks fly out.
- [ ] Brain chunks look like brain: pinkish or greyish, folded, wet and bloody.
- [ ] Some chunks are pinkish and some greyish.
- [ ] Their size beside the meat small gibs looks right (Brain Chunk Size).
- [ ] Pick a brain chunk up and throw it: it flies and bleeds as a small gib.
- [ ] A thrown brain chunk sticks to a wall now and then, as small gibs do.
- [ ] A brain chunk squishes when it lands.
- [ ] Brain chunks fade away after a while, as small gibs do.
- [ ] From a Head Gib 0 and From a Head Pop 0 throw none.
- [ ] Brain Chunks off throws none.
- [ ] Debug > Gore Tests > Brain Chunks Ahead bursts some just ahead of you.
## Decapitation 3: a beheaded body keeps its run; decapitation takes (2026-10-04)

The author: "enemy's own movement should not be cancelled": a popped head's ragdoll keeps 0.1x of the killing blow's push,
but the monster's own motion in full. And "decapitation" and "no decapitation" in the motion recorder, to record takes
for tuning which blows behead (the angle of the hit, the bladed part of the weapon).

**Its own motion** (Gore > Decapitation > **Keeps Its Own Motion**, `vr_decap_own_motion`, on): a walking monster's
velocity is none (Quake steps it every 0.1 s), so its ragdoll never carried its run. The engine now follows each
monster's origin (vr_box3d.cpp `watchCorpses`: its last three changes, `World::motion`; `ownMotion`: over the last two,
none after 0.2 s still) and its velocity the frame before. A ragdoll made by `ragdolldecap` starts at its own motion (the
walk, plus its animation's) plus what this frame added to its velocity (the blow's knock); each part's own motion is
kept (`RagdollBodies::ownLin`, `ownAng`; a corpse beheaded: its parts' motion as it lay), and a head pop's settle scales
only the rest: `own + (motion - own) * vr_decap_pop_body_speed`. Off: as before. `vr_debug_ragdoll 1`: "beheaded: its own
motion 161 u/s" and "settled after its head popped: ... pelvis 126 u/s (its own motion 161 u/s)".

| e1m1, super shotgun headshot (`vr_decap_test 13`) | its own motion | pelvis after the pop |
|---|---|---|
| a standing grunt | 0 | 53 u/s (as before) |
| a grunt running at you (three runs) | 161, 114, 125 u/s | 126, 81, 90 u/s (the shot's knock, against his run, 0.1x) |
| the same, Keeps Its Own Motion off | 0 | 44 u/s |
| a slash (`vr_decap_test 1`) on a running grunt | 124 u/s | 124 u/s (before: 13, his animation's) |

**Decapitation takes** (docs/vr-port/MOTIONS.md, "Decapitation takes"): Motion Recorder categories **Decapitation**
(`decapitation`; details the slash's direction, from_behind) and **No Decapitation** (`no_decapitation`; stab, pommel,
hilt, flat, slow, body, shoulder). Each blade blow by the hand on the training dummy (judged as a grunt) or on a monster
that can lose its head is judged by `VR_Decap_Blow`'s rules as if it killed (QC `VR_Decap_Judge`, vr_decap.qc): an event
`decap/yes` or `decap/no` (vr_motion.qc `VR_Motion_Hit`) whose detail says why: the zone (in or out, the distance from the
zone's line against its radius, the height below the head's middle), the part of the weapon (the share of the way from
the grip to the tip, the striking part), the slash angle (off the blade's line), the flat angle (off the edge: the hand's
sides taken as the blade's flats), the tilt (above level), the speed and the melee's kind, then the refusal. The dummy's
console line says "would behead (...)" or "would not behead (...)" for blows at its head or neck; Show Hit Zones >
Decapitation draws the dummy's zone too. expect.cfg: `decapitation decap/yes +melee`, `no_decapitation decap/no
!decap/yes +melee +bash +parrybash` (sword, axe, chainsaw); `+kind`, new: hits of that kind allowed, not required. The
eval table's `events` column has the decap events.

Tests: synthetic takes (`python Misc/quakevr/motion_synth.py decap --out quakevr/motions/decaptest`, new presets) with
`vr_motion_eval decaptest`: 5 of 6 pass. The level and diagonal cuts at the neck behead (slash 74-80 deg, flat 1-9 deg,
15-21 m/s, zone in); the stab doesn't (slash 8 deg: a stab); a cut 35 cm lower doesn't (zone out, 22.3/13.4 off). The flat
slap beheads (flat 78 deg): the rules don't look at the edge yet, the first rule these takes are for. Decapitation's
cases live, refuse, corpse, pop: as before.

In VR:
- [ ] Gore > Decapitation has Keeps Its Own Motion.
- [ ] Shotgun the head of a grunt running at you: his headless body stumbles on towards you.
- [ ] Shotgun a standing grunt's head: his body slumps where he stood.
- [ ] Slash the neck of a grunt running at you: his body carries on a little with his run.
- [ ] Keeps Its Own Motion off: a running grunt's popped body stops dead.
- [ ] Motion Recorder > Category has Decapitation and No Decapitation.
- [ ] Slash the dummy's neck with the sword: the wrist log says "would behead" with the numbers.
- [ ] Stab the dummy's neck: the wrist log says "would not behead" (a stab).
- [ ] Debug > Show Hit Zones > Decapitation draws the dummy's magenta zone.
- [ ] [Record] Decapitation: 3 level slashes across the dummy's neck, left to right.
- [ ] [Record] Decapitation: 3 level slashes across the neck, right to left.
- [ ] [Record] Decapitation: 3 diagonal downward slashes through the neck.
- [ ] [Record] Decapitation: 2 rising backswings through the neck.
- [ ] [Record] Decapitation: 2 slashes with the axe across the neck.
- [ ] [Record] Decapitation: 2 two-handed slashes across the neck.
- [ ] [Record] No Decapitation (Stab): 3 stabs into the neck.
- [ ] [Record] No Decapitation (Pommel): 3 pommel strikes on the head.
- [ ] [Record] No Decapitation (Hilt): 2 hits with the hilt or guard on the head.
- [ ] [Record] No Decapitation (Flat Slap): 3 slaps of the blade's flat against the neck.
- [ ] [Record] No Decapitation (Too Slow): 2 slow cuts across the neck.
- [ ] [Record] No Decapitation (Body Slash): 2 slashes across the chest.
- [ ] [Record] No Decapitation (Shoulder Slash): 2 slashes into the shoulder.

## Finer shadow blocks (2026-10-04)

`Shadow Blocks` (Graphics > Retro Lighting) went down to 1 texel and stopped there: 1 looked too coarse and there was
no way to ask for smaller pixels.

- `retrolight::block()` (Quake/vr/vr_retrolight.cpp) takes a `finest` argument: the same power-of-two ladder, starting
  at `finest` instead of at 1, and 0 (off) for anything below `finest * 0.5`. `finest = 1` (the default) is
  bit-for-bit the old behaviour.
- Only the shadows' lookup passes `0.25f` (`RetroLight[3].z`): `0` off, `1/4`, `1/2`, `1`, `2`, `4`, `8`, `16`. The
  lightmap's grid (still `max(…, 1)`), the world's and the models' dynamic-light grids are unchanged.
- The menu row has its own choice list (`Off, 1/4 texel, 1/2 texel, 1, 2, 4, 8, 16`); the look presets and
  `Reset to Defaults` are unchanged. No shader code changed: `RetroLightGrid` already turns a grid smooth where its
  blocks get smaller than about a pixel on screen, so sub-texel blocks show only where a Quake texel is itself a few
  pixels wide (near surfaces) and fade like every other grid does. That fade point is a shader constant, not a cvar.
- Measured in vrfiringrange (960x540, one torch light, 457 scanlines through the shadowed area): block 1 vs off moves
  27738 px (5.35% of the frame). `0.5` differs from `1` in 25552 px and `0.25` in 26454 px, so the finer values do
  reach the shader. Shadow-edge transitions: 4897 at 1, 4371 at 0.5, 4167 at 0.25 (median run 3 px, p25..p75 2..5) —
  the count does not double per halving in that scene; which step looks right is left to the headset.
  Probe: `Misc/quakevr/scratch/shadow_ab.sh`, numbers in `shadow_ab.txt`.
- `vr_retrolight_shadow_block` is archived: a cfg value in [0.375, 0.75) now maps to 0.5 instead of 1 (the old menu
  could not produce one).

## Brain chunks' own throw; per-enemy gib counts (2026-10-04)

The author wants brain lumps to fly differently from meat gibs (his Speed 3 / Up 7 are right for meat), and each
monster's gore counts tunable one by one, the way `vr_corpse_health_<monster>` already is.

**Brain chunks' own throw** (QC vr_smallgibs.qc's `VR_SmallGib_BurstOf`): a burst takes its Speed and Up from
`vr_smallgibs_brains_speed` (3 m/s) and `vr_smallgibs_brains_up` (7 m/s) when the chunks are brain, from
`vr_smallgibs_speed` / `vr_smallgibs_up` when they are meat. Both are still times the burst's own Flight by Situation
(`vr_smallgibs_speed_burst`, `vr_smallgibs_up_burst`); the defaults are what Speed and Up are now, so nothing changes
until they are moved. Gore > Small Gibs > Brain Chunks: **Brain Chunk Speed**, **Brain Chunk Up** (0..10 m/s,
extended to 30), beside Brain Chunk Size and Brain Chunk Mass. Brain count, size and mass are unchanged.

**Per enemy** (`VR_SmallGib_Mult`): `vr_smallgibs_mult_<monster>` and `vr_smallgibs_brains_mult_<monster>`, the same
monsters in the same order as Corpse Damage and Health's Corpse Health, by Monster (grunt, enforcer, rottweiler,
fiend, ogre, knight, hell knight, vore, shambler, scrag, rotfish, gremlin, centroid, eel, zombie, mummy), all 1. A
monster's counts, times these: what a hit tears out (`VR_SmallGib_Roll`), what flies with its gibs
(`VR_SmallGib_Gibbed`), and what its gibs and head burst into (`VR_SmallGib_Burst`, `VR_SmallGib_Brains`). 0 none, 2
twice as many; `VR_SmallGib_Times` rounds to the nearest whole and never below one where some came. A zombie's and a
mummy's apply beheaded or not (`zombie_die`, `mummy_die`, besides `VR_Decap_ZombieDie`).

- The source is plumbed through: `VR_SmallGib_Burst`, `VR_SmallGib_Brains` and `VR_SmallGib_BurstOf` take the entity
  they came from (`world` for none). The player, a training dummy, a thrown prop and a test gib are not on the list:
  1, as ever. A hit and a gibbing already have their target (`targ`, `self`).
- A monster's identity is its `th_die`, which a gib does not keep, so a gib or head thrown out of a monster carries
  its pair on itself (`VR_SmallGib_CarryMults`, called from `ThrowGib` and `VR_Decap_ThrowHead`; fields
  `vr_sgib_mult`, `vr_sgib_brains_mult`, `vr_sgib_multset`): bursting or destroying that head later counts them as
  that monster's. The head a gibbing throws *is* the monster (`ThrowHead` keeps its `th_die`) and is read live.
- Gore > Small Gibs > **Per Enemy**, a page of its own (`Small Gibs - Per Enemy`): Small Gibs, by Monster, then Brain
  Chunks, by Monster, 0..4x (extended to 20), the same labels and order as Corpse Health, by Monster.
- `configVersion` 85 → 86, with a row for every new cvar. None of them existed at 85, so each takes its compiled
  default, which is what it did then; the rows are the record of what 86 added.

**Tests** (Debug > Tests > A Grunt Ahead / An Enforcer's Ragdoll There, Debug > Gore Tests; each on a fresh map, so
the QC counters start again):

| run | result |
|---|---|
| test 16 (a grunt gibbed underfoot), defaults | `sgibtest: underfoot: gibbed 1, 12 small gibs` (With a Gibbing 12) |
| the same on an enforcer | 12 |
| the same with `vr_smallgibs_mult_grunt 2` | 24 |
| the same on an enforcer, `vr_smallgibs_mult_grunt 2` | 12 (unchanged) |
| decap test 12 (a shotgun headshot pops a grunt's head), defaults, then test 11 | 15 small gibs made, 8 of them brain chunks (From a Head Pop 8) |
| the same with both grunt multipliers 2 | 30 small gibs, 16 brain chunks |
| test 21 with `vr_smallgibs_trace 1`, defaults | 8 brain chunks; launch velocities 176..288 u/s, 158..279 u/s of it up |
| the same with `vr_smallgibs_brains_speed 6; vr_smallgibs_brains_up 0` | the same 8; 146..259 u/s, 0 up — brain flight only, the meat gibs' Speed and Up untouched |

`vr_smallgibs_test 21` and the burst tests pass `world`, so they are not affected by a monster's multipliers.

In VR:
- [ ] Gore > Small Gibs > Brain Chunks has Brain Chunk Speed and Brain Chunk Up; brain chunks fly their own way, the
      meat gibs unchanged.
- [ ] Gore > Small Gibs > Per Enemy lists every monster twice; a grunt's at 2 doubles what he throws, an enforcer's is
      untouched.
- [ ] A beheaded grunt's head, shot to bursting later, still throws his doubled counts.


## Mapper flames burn hands and the walking body (2026-10-05)

`light_flame_large_yellow`, `light_flame_small_yellow`, `light_flame_small_white`, and wall torches still on their walls
now burn you at their visible flames. Hands/arms use `vr_burn_self`, its existing delay and haptic warning, then the
same anchored flames, wounds, damage and duration as a held torch. The physical body uses `vr_burn_touch` and the
immediate ignition of a dropped torch. Tracked torso/head/legs are tested first; the player collision box also keeps
feet touchable with Body Mode below 3. Existing defaults, including the author's 0.6-second delay, are unchanged.
Static flames reuse the existing relighting registry; static wall torches join it when `vr_walltorch 0`. Dynamic
wall torches are sources only while `wt_state == 0`, lit, and modeled: taking one leaves no wall hazard. The flame
capsule builtin requests body shapes between the 0.1-second contact checks even with `vr_body_collide 0`.

QuakeC/FTE pitfall found in the real fixture checks: passing `vr_fire_spot[i]` (vector) followed by
`vr_fire_spot_large[i]` (scalar) directly as call arguments let the latter array getter overwrite the vector's y/z
argument slots; `-96 632 406` arrived as `-96 0 0`. Cache both indexed values in locals before making the call.

Focused check: `bash Misc/quakevr/mapflameburn_test.sh <agent>` (optional second argument filters case names).
It checks both hands, brazier/wall body contact, a static wall torch, toggles, withdrawing before ignition, the
non-flaming stick, clear space, detaching, lit held/dropped and extinguished portable torches, and a 1.5-second delay.
In VR, confirm the warning buzz, burns following the touched hand, foot/body burn placement, and withdrawing from
both e1m2 braziers at `-96 632 406` and `-24 -232 414`; also try wall torch 52 before and after taking it.

## Hand-placed map tips (func_vr_tip) (2026-10-05)

A tip for new players placed by hand in a map, with its own text, range and subject. Alongside the built-in tips
(`tips` in `Quake/vr/vr_tips.cpp`, which are about a client entity the code knows), the map supplies its own list.

**The channel** (the world texts' pattern exactly, `vr_worldtext.cpp`): the map entity is a QC edict and the client
cannot read its spawnkeys, so QC makes the server's list and the engine sends it. `QC/vr_tips.qc`'s `func_vr_tip`
calls the `vr_tip_make` / `vr_tip_set*` builtins (`QC/builtins.qc`, `PF_vr_tip_*` in `vr_builtins.cpp`); the list is
broadcast through the new `QVR_SVC_TIP_*` subcmds (`vr_protocol.hpp`, dispatched in `vr_client.cpp`) and the whole list
is replayed to each client as it spawns (`tips::serverWriteAll` from `VR_WriteClientSpawnState`, `vr_server.cpp`).
`tips::clientReset` (with the client's other state) clears it with the map; `tips::serverReset` with the progs.
The client's map tips join the same frame loop after the built-in ones: same nearness, view angle, line of sight and
delay tests, same floating CRT screen with its cable, same gadget hologram.

**Attached to an entity or a prop**: the tip's `target` (or `targetname`, when `target` is unset) names another
entity's `targetname`; the engine sends that entity's index once (`QVR_SVC_TIP_ENT`, `NUM_FOR_EDICT`; `-1` for the
world = a fixed point) and the client follows `cl_entities[i]` live — its origin and model box as it moves, `msgtime`
saying when it is gone (the panel fades). A tip naming itself follows nothing: it stays at its own origin. A target
placed after the tip in the map file is found again in the map's first frames (`VR_Tip_Retry`, time + 0.1: a loaded
game is laid over the map's first two frames). The server checks the followed entity each frame (`tips::serverFrame`):
freed, or another classname in its slot, the tip's entity becomes `goneEntity` (-2) and it never shows again.

**Seen once**: the key in `vr_tips_seen` is `<mapname>:<tipname>` (`<mapname>#<index>` when it has no name), so the
same name in two maps is two tips; `vr_tips_reset` (VR Settings > Tips > Show Tips Again) clears them with the rest.

**Keys** (`func_vr_tip`, a point entity; the zero defaults mean the player's own VR Settings > Tips values):

| key | default | what it does |
| --- | --- | --- |
| `message` | — | its text; `\n` starts a new line. Without it the tip removes itself. |
| `distance` | 0 | range in units (0: `vr_tips_distance`, 150). |
| `target` / `targetname` | — | another entity's `targetname`: the tip follows it, live. Neither: a fixed point at its origin. |
| `tipname` | — | names the tip: its key in `vr_tips_seen`, and what `vr_tips_test` takes. Falls back to `targetname`. |
| `tip_size` | 0 | its text size (0: `vr_tips_size`; 1 is about 1.5 degrees a character). |
| `tip_delay` | 0 | seconds he must stay near before it shows (0: `vr_tips_delay`). |
| spawnflag 1 `REPEAT` | off | shown every time he comes near, not remembered. |
| spawnflag 2 `HOLOGRAM` | off | in the wrist gadget's hologram instead of the floating screen. A map tip uses the screen whatever `vr_tips` is (1 or 2) unless this is ticked; `vr_tips 0` is no tips at all. |
| spawnflag 4 `ANYANGLE` | off | shown even out of `vr_tips_view_angle` or hidden by the world. |
| worldspawn `_vr_tips_repeat` | 0 | 1: every `func_vr_tip` in this map repeats (a tutorial map). Read by the engine (`mapFlags`): QC never sees `_` keys. |

**In TrenchBroom**: the entity is in `quakevr.fgd` (its QUAKED comment in `QC/vr_tips.qc`, its keys, choices and help
in `Misc/trenchbroom/entities.fgd`, `python Misc/trenchbroom/fgdgen.py` then `install.ps1` with TrenchBroom closed).
Point → place it where the tip is about (its origin is what the cable points at; for a fixed point, put it in the open
air, not in a wall); set `message`; tick the flags. To attach it: give the prop a `targetname` (a `func_button`, a
`vr_crate`, an item) and the tip that name in `target`. To try one in the game: `vr_tips_test <tipname>` (VR Settings
> Tips > List This Map's Tips names them all), and Show Tips Again to see it again.

**In vrstart.ent**: one test tip (`testwelcome`), at a fixed point 100 units from the start, `distance 400`,
`tip_size 1.5`, ANYANGLE — the range and size keys are the ones to try first. Proved headless: with three more test
tips, the log shows `tips: "testfollow" by entity 54` (attached to an `item_health`), `tips: "testfar" at 64 148 280`
(302 units, shown only because its `distance` 400 overrides the 150 default), `testwelcome`, and a fourth at the same
point with `distance 60` never showing; `vr_tips_seen` ends as `vrstart:testfollow vrstart:testfar vrstart:testwelcome`.

## Animated wall buttons keep their relief (vr_extmaps, 2026-10-06)

**The bug**: the red diamond buttons (`+0basebtn`/`+1basebtn`, e1m1, e2m1...) changed shape every tenth of a second:
the lit frame's bevel slid up and sideways. Quetoo's pack (`textures_quetoo`, drawn on QRP's textures) has a normal map
only for an animation's first frame (`basebtn+0_norm`; none for `basebtn+1`, `button+1..3`, `butn+1/2`, `butnn+1/2`,
`planet+1..3`, nor for frames it has no picture of, `+abasebtn`, `+3button`). Those frames fell back to the normal map
made from their own shading, whose bumps and parallax heights differ from the pack's: the relief jumped between the
pack's and the made one as the frames changed.

**The fix** (`Quake/vr/vr_extmaps.cpp`, `siblingNormal`): a frame the pack has no normal map for takes another frame's
of the same animation (its own run first, from `+0`; an alternate's `+a..+j` first), where that frame's picture in the
pack matches the texture drawn as well as `vr_extmaps_match` asks (the same test its own picture passes; 0.5): an
animation's frames share their shape. A frame the pack has no picture of at all gets only that normal map (no specular
map or glow). With the id textures (`qbase`) nothing matches (0.09-0.14), so nothing changes there; textures that are
not animation frames, liquids and sky never take another's. `vr_extmaps_stats all` shows it: `+1basebtn used match
0.93 norm (+0's, 0.91) spec`; `developer 1` logs each one taken or refused.

**Proved headless** (QRP, e1m1, `setpos -30 600 25 0 215 0`, a test light, six shots three frames apart, button
cropped): the `+0` shots are unchanged by the fix (mean difference 0.1-0.2 of 255: run noise 0.3); the `+1` shots
changed by 24; consecutive shots across the frame change differ 12.8 after against 29.5 before. e1m1 takes 5 normal
maps this way (`+1/+2/+3planet` 0.88-0.92, `+1basebtn` and `+abasebtn` 0.91), e1m4 3 (`+1..+3button` 0.93-0.96).
Without the pack (`vr_extmaps 0`) every frame's made map comes from its own shading and the relief stays put.
### Review of the map tips (2026-10-06)

The feature as merged had six bugs, each now fixed in its own commit and proved headless on a copy of vrstart with
test tips (before / after):

- **REPEAT did nothing** (the flag was never read): a Repeat tip was added to `vr_tips_seen` and never shown again. Now
  it stays out of the list and shows again once the player has gone 1.25x its range away and come back.
- **`_vr_tips_repeat` did nothing**: `ED_ParseEdict` drops every key that starts with `_`, so the QC field was always 0.
  The server reads it from the worldspawn (`debris::worldspawnValue(key, absent)`); the QC field is gone.
- **A tipname with a space** never matched its key in the space-separated `vr_tips_seen`: the tip showed every few
  seconds and the list grew each time. Spaces, quotes and semicolons become `_` in the key.
- **A followed entity freed and its slot reused**: the tip showed on whatever took the slot (a removed health's tip on
  an armor spawned there). Now `goneEntity`.
- **A loaded game** lost a late target (the retry ran at 0.5 s, after the save had been laid over the map): the tip
  sat at its own origin. Now the retry runs in the first frames.
- **A demo recorded mid-map** had no map tips (`VR_WriteDemoState` wrote only the world texts): `tips::clientWriteAll`.
- **List This Map's Tips** printed `no tip "list"` with the names: now a real list (`#<n>` for unnamed tips).

Left for a decision (not changed): a tip following an entity the client never receives (an `info_notnull`, a trigger)
never shows; a tip's own `targetname` (the follow-by-targetname form) makes the tip itself a match for every
`find(world, targetname, ...)` of that name (a teleporter's destination, a monster's path) — `target` is the safe key;
`tip_delay 0` cannot mean "at once" (0 is "the player's setting"); `vr_tips_seen` grows with every map tip ever seen
and a config value is read back only up to 1023 characters (`com_token`: a 1199-character list read back ends after
its 83rd key), so after some 50 map tips the newest keys are lost on restart and those tips show again; tip texts are unbounded (64 tips of ~1 KB would overflow a spawning client's
64000-byte message).

## Animated textures: one surface for all their frames (vr_anim_surface, 2026-10-06)

**Still pulsing after the fix above** (Vittorio in VR: better, but the button "pulsates"). Measured per frame with the
new `vr_extmaps_frames [name]` (Debug > Reports > Animated Surfaces: every frame's bound normal map, specular map,
.mat numbers, glow and detail, and whether an animation's frames share them):

- QRP + Quetoo pack, e1m1 `basebtn`: the normal map was already shared (`basebtn+0_norm`, one texture: its heights and
  its green orientation are per file, so those were identical), but the **specular map** was not: `+0` has
  `basebtn+0_spec` (coloured, golden on the plate), `+1` `basebtn+1_spec` (grey, mean difference 29 of 255), `+a`
  none; times `vr_extmaps_spec_scale` (Vittorio's 8) the plate's sheen flashed with the frames. e1m4 `button`: `+0`
  has a specular map and hardness 4.96 (`button+0.mat`), `+1..+3` and `+a` neither. `planet` likewise.
- Made normal maps (original textures, or QRP with `vr_extmaps 0`): each frame's made from its own colours, so its
  own bumps and parallax heights: the lit frame's relief differs (the note above said it "stays put": it does not).
- Detail textures: chosen per texture from its own colours; no frame of e1m1's animations got another kind, but it
  could.

**The fix** (`Quake/vr/vr_extmaps.cpp` `VR_AnimSurfaces`, `surface`; `texture_t::surface`): after Mod_LoadTextures
sequences the animations, each frame gets its animation's lead frame (main frames 0..9, then a..j: the first with a
pack normal map, else with a pack specular map, else the first) when its Quake picture matches the lead's, or a frame
already joined (detail correlation >= 0.5 where they lie, and better there than shifted along s or t: a moving picture
keeps its own; `+6slip`, a brightening glow, 0.46 against `+0` but 0.63 against `+1`). Drawn (`VR_ExtMapsCall`,
`VR_DetailCall`), with `vr_anim_surface 1` (default; at once; Debug > Views > Animated Surfaces), a frame takes the
lead's normal map (made or the pack's), specular map, .mat numbers and detail; only its colour and glow are its own.
id's maps (start, e1-e4, end): every animation's frames join (0.62-1.00), none moves. Cost: 7.8 ms for e1m1 and its 9 item-box models together.

**Proved headless** (e1m1, `setpos -30 600 25 0 215 0`, a side light, 10 shots 5 frames apart per setting, button
crop; shots sorted into dim/lit frames; a mask of where the colours differ, from `r_fullbright 1` shots, dilated 3 px;
run noise 0.05-0.08). Mean difference between the frames outside that mask (fullbright's own residual 0.26):

| case | parallax on: before / after | parallax off: before / after |
|---|---|---|
| QRP + pack | 5.03 (max 127) / 0.24 | 4.92 / 0.25 |
| QRP, `vr_extmaps 0` (made maps) | 9.00 (max 194) / 0.20 | 1.07 / 0.27 |
| id textures | 0.75 / 0.04 | 0.60 / 0.04 |

With the pack the pulse was the specular map (parallax off changes nothing); with made maps mostly the parallax heights.
The start view of e1m1 (no animation in it) is unchanged (mean 0.00004). `vr_extmaps_frames`: e1m1 QRP 2 of 5
animations with one surface before, 5 of 5 after; e1m4 `button` 0 of 1 / 1 of 1; e1m1 id textures 2 / 5.

## Shoved over a ledge: always knocked down (vr_knockdown_ledge, 2026-10-06)

Asked: a shove's knockdown chance is 100% when the shove would carry the monster off a high ledge. `VR_Knockdown_Ledge`
(QC vr_knockdown.qc), called by `VR_Knockdown_Try` once the shove's push is set (VR_Push), when the hit could knock it
down at all (its chance above 0: a weapon's bash at Weapon Bash 0, flying and swimming monsters, ones without a ragdoll
are unchanged). Its reach is the push's own: the hop (2 x up / gravity at its horizontal speed) and the slide after it
(speed^2 / 2 VR_SHOVE_FRICTION), plus 16. Its hull is traced along the way a unit off the floor (the world and brush
entities: a wall or railing before the edge ends it there); every 8 units the ground under the middle of its feet (where
the slide sends it on over) is looked for, and a ledge is no ground within `vr_knockdown_ledge_drop` (64, a bit more than
a player; stairs, a step at a time, are none) below the last point's ground, with room past the edge for its whole hull
(not a crack). Liquids are not ground (traces pass through them): a drop into water, slime or lava counts to the bottom
under it. On a lift or plat the drop is measured from it. Settings: Combat > Knockdowns, **Over a Ledge, Always**
(`vr_knockdown_ledge 1`) and **Ledge Height** (`vr_knockdown_ledge_drop 64`). Test aid: `vr_knockdown_test 20` removes
every monster (Debug > Tests > Enemy Shoves, **Remove Every Monster**).

Checked on vrclimb (`scratch/ledgetest.py`: a grunt by `impulse 244`, shoved by `impulse 219`, `vr_knockdown_chance 0.3`
so the plain chance is 0.43; 20 trials each): toward the trench's edge (48 ahead, a 200-unit drop) 20/20 down, all forced,
all landing in the trench; the same with the toggle off 7/20; shoved away from it 8/20; down the trench stairs (16-unit
steps) 7/20, none forced; on the grab-leniency block (top 48) at Ledge Height 32: toward its open edge 20/20 forced,
toward the thin wall 6 units past its edge 9/20, none forced (the hull's way ends 18 units on); at 64, its open edge 10/20
(48 is no ledge); on vrclimb's plat (top 48) at 32: 20/20 forced. Not checked: a lift while it moves, a drop into liquid.

**Its reach, adjustable** (asked the same day): Combat > Knockdowns, **Ledge Reach** (`vr_knockdown_ledge_reach`, 1:
times the shove's reckoned travel, the hop and the slide; 0.25..3 on the slider) and **Ledge Margin**
(`vr_knockdown_ledge_margin`, 16 units past it; 0..128), replacing the fixed `VR_KD_LEDGE_MARGIN`. The defaults give the
old reach exactly. Checked on vrclimb (the cases above plus new ones, 6 trials each; the debug line's reach and edge):
at the defaults every case as before (reach 241; forced 6/6 toward the trench, the block's open edge at 32 and the plat;
none elsewhere); toward the trench (edge 48 ahead) reach 0.1 (22 units) and 0 + 16 forced none, 0.25 (56) and 0 + 64
forced all; from 64 units further back (edge 128 ahead) the defaults forced all, 0.25 + 16 (72) and 0.25 + 64 (120) none.

## Flat-screen input lag (2026-10-06)

Report: in flat-screen play (keyboard and mouse) WSAD seemed to register half a second late.

**Measured in the game (vr_inputlag_test).** An SDL key or mouse event is pushed into SDL's queue, then each stage's
first frame is timed (250 fps cap, the author's config, `-RealTime`, hidden and visible windows, flat and mock VR):
the bound command (+forward) runs the next frame (+1, 4 ms); the move goes out at the next 72 Hz server tick (+1..+3
frames, 4-12 ms; Ironwail's renderer/network isolation, stock); the server's velocity the same frame; the view moves
+2..+4 frames (8-16 ms); 90% of the speed after 120 ms (Quake's sv_accelerate); the release's -forward the next frame.
The view stays within 6 units of the server's player (one tick). Mouse look: the yaw the next frame (4 ms). The
command buffer was empty, no frame stalls (vr_profile: 4.0 ms frames, worst 7.6). Mock VR: the same. So the engine's
path from SDL's queue to the screen is stock Ironwail's.

**The cause: the desktop keyboard hook.** Ironwail installs a system-wide WH_KEYBOARD_LL hook (sys_sdl_win.c: Caps
Lock, Scroll Lock, Num Lock and Print Screen as game keys) in IN_Init whatever the focus, and takes it off only on a
focus loss. Windows calls such a hook for every key press on the desktop (in any program) and holds the key until the
hooking thread services it, which the game does only while it pumps messages (once a frame, not at all through a
start-up or a map load), up to LowLevelHooksTimeout. A window that is never focused never loses the focus: every test
run's copy (hidden or in the background) kept the hook for its life, and left it unserviced for up to 938 ms at a map
load (`vr_keyhook_status`). With the agents' runs going (eval shards, smoke tests: each one loading maps), the
player's key presses waited for each copy in turn: hundreds of ms in flat play. The mouse and the VR controllers go
through no such hook, so VR play never showed it. Fix (in_sdl.c): the hook is installed at start-up only with the
keyboard's focus (otherwise on SDL_WINDOWEVENT_FOCUS_GAINED, as before). Test copies now report "not installed".
Copies built before this commit (other worktrees, the kit's older builds) still hold it until rebuilt.

**Also fixed: the walk turned a tick late in flat play.** The server walks by the move's head angles
(VR_MoveAngles); flat, they came from the hands' state, taken at the frame's start before the frame's mouse motion,
so a key pressed while turning walked the old way for one server tick: the walk lined up with the view (within 5
degrees) after 84 ms; now 28 ms (`vr_inputlag_test turn`). VR unchanged.

Debug > Reports: Keyboard Hook (`vr_keyhook_status`), Input Latency: Walk (`vr_inputlag_test key`).
## Parallax at grazing angles (2026-10-06)

He found parallax flat from the side. Three things flattened it there: the fade (gone from 70 to 83 degrees off the
normal), the cap on the shift (3 times the depth, reached at 70 degrees), and a step budget that got no bigger as the
ray's run along the surface grew.

**What changed** (`ParallaxUV`, vr_glsl.h; the world, item boxes, models and authored models all share it):
- **Refinement** (`vr_parallax_refine`, 4; 0..8). After the walk, the step that crossed the height field is
  refined by regula falsi, the Illinois way: a secant each time, and an end kept twice in a row is halved, so the
  bracket shrinks from both sides. It is relief mapping's binary search, made surer and faster. 1 is the old shader
  (one secant read). The final guess is interpolated, not read.
- **Grazing fade** (`vr_parallax_grazing`, 86 degrees; 30..90). The effect is gone at that angle and whole 12 degrees
  before it (cosines in the new frame field `Parallax2.yz`). 90 means no fade. 83 is about the old fade.
- **Shift cap** 8 times the depth (`PARALLAX_STRETCH`, was 3). Beyond 83 degrees the ray goes steeper than the eye's,
  so the relief stays and the smear stops growing.
- **Step budget.** The walk takes `vr_parallax_steps` x 0.5 steps looking straight on and up to x 2 at grazing (it
  was x 1 there), at most 64. It never takes more than one step per texel crossed (1.5 unrefined), counted at the mip
  level the reads use (8x anisotropic: max(minor axis, major / 8)). Far away, that means fewer steps.
- Frame data: a new `vec4 Parallax2` after `Parallax`, in both `QVR_FRAMEDATA_FIELDS` and `ALIAS_FRAMEDATA_BUFFER`,
  and `parallax2[4]` in `gpuframedata_t`. Its `w` is free (pixel depth offset, next section).
- Menu: Graphics > Surfaces > Parallax Refinement and Parallax Side Fade. The calibration board has no parallax
  lines, so it needs none (`vr_menu_path_check`: 0 missing).

**Method.** His `ironwail.cfg` (copied, read only) exec'd, `nomonsters 1`, no viewmodel. A/B was done in one run with
`vr_shader_reload 1` (the old function under `#if QVR_SHADER_AB == 1` while testing; removed before the commit).
- **Images**, both eyes at 1440. The e1m1 corridor (`setpos 480 0 88 0 90 0`) and the spawn's walls (`480 -352 88`,
  yaw 352). Only the grazing parts change: the corridor's ceiling, the beam undersides and the lit alcove's ledge,
  0.4-0.7% of the pixels. The parts in full view are unchanged. A shot repeated in the same variant differs by at
  most 1 level. At `vr_parallax_depth 8`, the ceiling's grooves stand out at grazing angles where before they had
  faded.
- **Refinement in isolation** (`vr_parallax_steps 4`, depth 6, linear filtering). The reference is 64 steps with 8
  refinements. The mean difference from it is 2.01 with no refinement and 0.43 with 4. At the default 16 steps, the
  walk plus the last secant are already close: 0 vs 4 differ by under 0.05 mean.
- **Swimming:** a 12-frame sweep along the corridor, 0.25 units a frame, fullbright, left and right eyes. The measure
  is the mean of |f(t) - (f(t-1) + f(t+1)) / 2| over the pixels where the variants differ. Steady motion keeps it low.
  Layer jumps and popping spike it.

  | Variant | 2nd diff (L) | >24 (L) | 2nd diff (R) |
  | --- | --- | --- | --- |
  | old shader | 1.895 | 0.215% | 1.861 |
  | new (refine 4, fade 86) | 1.470 | 0.206% | 1.455 |
  | new, refine 0 | 1.471 | 0.206% | 1.454 |
  | new, no fade (90) | 1.594 | 0.726% | 1.596 |

  With linear filtering: old 1.681, new 1.366, no fade 1.448 (spikes 0.67%). The new shader moves more smoothly
  than the old one, even though it shows relief further round. With no fade at all, the last few degrees pop. That
  is why the default is 86, not 90.
- **Cost** (exclusive runs, eyes 3406 square, his config, GPU world+brush for both eyes, paused, 3 samples of 120
  frames, medians):

  | View | old | new |
  | --- | --- | --- |
  | spawn facing the wall (w270) | 0.910 | 0.960 |
  | spawn, along the corridor (c90) | 0.950 | 0.970 |
  | corridor (480 0 88) | 0.980 | 1.040 |

  Frame GPU 1.87-1.96 -> 1.91-2.02 ms. Parallax as a whole costs 0.13-0.17 ms here (`vr_parallax 0`: 0.83 / 0.88).
  Refinement 0 to 4 adds about 0.03-0.05 ms. The fade angle makes no difference that can be measured.

**For him in VR:** walk along a riveted or panelled wall with the head close to it, with Parallax Side Fade at 83
(old), 86 and 90. Then set Parallax Refinement to 0 and 4 with the walls close by.

## Parallax pixel depth offset (2026-10-06)

`vr_parallax_depth_write` (0 by default; Graphics > Surfaces > Parallax Depth Write) writes the depth of the point
the parallax ray hits, instead of the flat polygon's. Whatever is drawn later then meets the relief where it appears
to be: models, hands, particles (soft particles), liquids (their scene-depth reads), and decal meshes, which stay
in front.

**How** (world and brush models only; `ParallaxUV` sets `ParallaxDist`, the hit's distance past the surface along
the eye's ray, each eye its own):
- **Main pass.** New programs `glprogs.world_pdo[dither]` (solid, not OIT, `PDO 1`), chosen by
  `R_ChooseBModelProgram` only while the setting is on. They write `gl_FragDepth` (`ParallaxFragDepth`: the hit
  through the view's `ViewProj`, with the polygon offset of `CF_USE_POLYGON_OFFSET` and `gl_DepthRange`, never
  nearer than `gl_FragCoord.z`). A shader that writes depth must write it on every path, and under foveated
  rendering's coarse shading it writes one value per block. With it in the ordinary programs, vr_foveated 3 striped
  the periphery even with the setting off (the pre-pass's per-pixel depths failed the block's one value). So the
  ordinary programs never write depth, and with the setting off, everything is as before.
- **Depth pre-pass.** The pre-pass used to write the polygon's depth, and the shading pass then only passes where
  it is that depth or nearer. A hit is further away, so it would fail. So with the setting on, the pre-pass uses
  `glprogs.world_depth_pdo`: a small fragment shader (`QVR_WORLD_DEPTH_FS_MAIN`) writes how far a hit can be at most
  (`ParallaxReach`, times 1.01, plus 0.01: the same numbers as the march's cap). The shading pass's real hit is never
  further, so it passes and writes the real depth. World surfaces closer than that bound shade too, and the depth
  test sorts them out.
- **Not done:**
  - Models (alias). Their shader is used for every model, so it would write depth for all of them, coarse in the
    foveated periphery, and it has no pre-pass. Their relief is small (hands: 0.4 x 1.5 units).
  - Shadow maps. They are drawn from the lights, and the relief stays out of them.
  - World against world. In the BSP, faces don't overlap (a wall stops at the floor), so a floor almost never covers
    a wall's sunken part. The wall/floor junction changes by a handful of pixels.
- **Debug:** `vr_parallax_debug 1` (Debug > Rendering: Show Parallax Depth) draws the world as the depth written.
  Red is a hit, brighter the deeper below the surface (relative to the full depth). Dark blue is the surface's own
  depth (no heights, too far, or faded).

**Checked** (his config, both eyes):
- **On vs off, plain views.** The e1m1 corridor and the spawn wall, his depth 2.5, with his vr_foveated 3 and with
  foveation off: the eyes are identical apart from a handful of pixels (max 0 in L, a few pixels in R).
- **No holes** where the shading pass would fail the pre-pass's bound. Also checked with vid_fsaa 4: 0.02% of
  pixels differ.
- **Debug view:** walls and ceiling write hits, the floor (no heights) its own depth.
- **Not shown in the headless runs:** a model meeting sunken relief. The test monsters and hands didn't end up
  touching a carved wall. Check it in VR.

**Cost** (exclusive runs, eyes 3406 square, GPU world+brush for both eyes, medians of 3 x 120 frames):

| View | off | on |
| --- | --- | --- |
| spawn facing the wall (w270), vr_foveated 3 | 0.96 | 1.06 |
| spawn, along the corridor (c90), vr_foveated 3 | 0.98 | 1.61 |
| corridor (480 0 88), vr_foveated 3 | 1.07 | 1.46 |
| c90, foveation off | 2.80 | 3.77-3.96 |

Where the time goes (c90, foveation off):

| Variant | ms |
| --- | --- |
| Neither part | 2.78 |
| Pre-pass shader only | 3.10 (+0.32) |
| Shading pass writing depth only | 3.52 (+0.74) |
| Both | 3.96 |

The conservative depth layout makes no difference: `depth_less`, `depth_any` and the wrong-way `depth_greater` time
the same (3.77, 3.75, 3.70). So this driver shades every fragment of a shader that writes depth, hidden ones too,
instead of rejecting them early. A smaller depth (0.5) costs the same, so the cost isn't overdraw inside the bound.
The setting is off by default for that reason.

**Ways to make it cheap (not done):**
- Read the pre-pass's depth (a copy, or the attachment behind a texture barrier) and discard surely-hidden fragments
  first. Coarse-shaded and MSAA fragments need care: one check per covered pixel or sample, and quads must keep
  their derivatives.
- Write the hits' distance to an extra colour target in the shading pass (no depth writes, early-Z kept), then
  write the depth in one full-screen pass.

**For him in VR:** turn Parallax Depth Write on and look at the hands, a dropped weapon, gibs and blood on carved
floors, and water against carved walls, with Show Parallax Depth to see where it acts. Decide whether it's worth
0.1-0.6 ms.

## Lightning's lasting shock: arcs, convulsions and burns on the bodies it kills (2026-10-06)

Already there before this change (verified headless, `vr_shock_hit_test 1` on a live grunt): every lightning hit on a
monster or corpse draws Quad-style arcs over its drawn triangles for 0.75 s (`bodyshock`, QVR_SVC_SHOCK kind 3: 12
arcs a frame, 810 triangles) and paints one lightning burn where it strikes (QVR_WOUND_ZAP via `VR_Wound_Hit`).

New (QC `vr_shock.qc`, called once from `VR_LightningHit` after its `T_Damage`):

- A bolt that kills a monster, or strikes one already dead, starts the lasting shock: `bodyshockdeath` sends kind 4
  (`KindBodyDeath`, duration in 1/10 s, up to 25 s) and the client (`vr_shock.cpp drawBodyDeath`) keeps Quad's arm
  arcs crawling over the body as drawn (its ragdoll's skinned mesh or its death frames): 6..22 short crackles off the
  skin a frame plus a few longer limb-to-limb arcs, thinning and dimming as it wears off, random surges, a flickering
  blue light. It ends early if the entity's model changes (gibbed, slot reused); the `#rag` swap is the same body.
  A beam held on a corpse keeps it fresh (re-sent once 0.4 s has worn off).
- The killing bolt paints `vr_shock_burns` (6) lightning burns all round the body (sides at any height, some from
  above); a hit on a corpse paints a burn where it strikes (corpse damage paints none through `VR_Wound_Hit`).
- `.vr_shock_until` / `.vr_shock_len` (engine fields) make the ragdoll convulse (`vr_box3d.cpp`, "Shocked ragdolls"):
  per limb a torque pair (limb +, parent -) drives the relative spin towards 14 rad/s x `vr_shock_seizure`, flipping
  9 times a second about an axis of its own (friction-compensated, capped), plus each jerk 45% of limbs flop up at
  3 m/s (parent pushed back, the floor takes it) and 20% of jerks buck the chest. Full for the first 40% of the
  shock, then easing to zero: lying limbs' own drive alone can't beat the floor's friction, hence the flops.

Cvars (Gore > Lightning's Shock): `vr_shock_death 1`, `vr_shock_death_time 3`, `vr_shock_seizure 1`,
`vr_shock_burns 6`, `vr_shock_arcs 1`. Tests: `vr_shock_hit_test <damage | -1 just kills>` (strikes the nearest
monster/corpse through `VR_LightningHit`), `vr_shock_info` (kind, arcs drawn), `vr_shock_ragdoll_check` (shock left,
limb relative spin, fastest part, worst joint separation, pelvis), `vr_debug_ragdoll 2/3` (drive and flop prints),
`vr_wounds_debug 1` (each burn struck or missed). All on Debug > Tests.

Measured (grunt killed at 96 units, samples every 18 frames; seizure 0 | 1): limb relative spin after landing
0.73, 0.07, 0.00 | 8.7, 6.2, 2.9, 1.5, 0.3, 0.0 rad/s; at rest after: 0 | 0, pelvis still; worst joint separation at
rest 0.07 | 0.19 units (peaks 4.7 | 3.5 during the death fling itself); corpse re-hit: 0 | 5.9 rad/s. Burns struck:
live hit 1, kill 7 (6 + the hit), corpse re-hit 1 (soldier.mdl#rag); shalrath (no rig) kill 7, its arcs for 3 s then
gone. Six shocked ragdolls at once: CPU busy 0.78 vs 0.53 ms (arcs 0.04 ms, box3d 0.08 vs 0.04, the lights and
lines the rest), GPU 0.95 vs 0.92 ms.

Seen in passing: a grunt killed by a 31-damage bolt at 96 units flies ~250 units before landing, with or without the
seizure (the death knock), and `eval.sh` currently fails on a missing motion CSV in the author's checkout.

### Lightning Shock follow-up: on the living too, longer, off the skin (2026-10-06)

Vittorio (vrfiringrange 12:22): the arcs vanished almost at once, were hard to see (mostly inside the bodies), should be
more and further out, start on living monsters as soon as the lightning strikes and carry over to the corpse; only the
convulsions are the corpse's, lasting as long as the arcs. (His game ran a progs.dat from before the shock QC: the
"almost at once" was each hit's own 0.75 s arcs.)

- **Every bolt into a monster, alive or dead** (`VR_Shock_Hit`) starts the lasting shock or refreshes it: sent each bolt
  (no 0.4 s throttle: the client's end and `.vr_shock_until` stay together), `vr_shock_death_time` from the last bolt.
  The client effect follows the entity through its death (the `#rag` swap is the same body); a kill by another weapon
  keeps what is left of it. Burns as before (a kill: `vr_shock_burns` all over; a corpse hit: one where it strikes).
- **Duration on the wire:** kind 4 in 1/4 s (was 1/10 s, capped at 25.5 s): up to 63 s. Menu slider 0.5..30 s
  (extended 0.1..60).
- **Arcs off the skin:** Quake's models wind their triangles so that the cross product points *into* the body; the old
  arcs started 0.5-0.6 units along it, i.e. inside (measured below). `outwardSign` finds the winding from where the
  triangles face against the body's middle. The lasting shock now draws: 5-9 **walking arcs** (each from a point on the
  skin to one ~11 units away, stepping on every 0.04-0.12 s, bowing out by 1.5 + 0.2 x their length so they stay off the
  body over concave parts; points kept as triangle + barycentrics so they follow the animation/ragdoll), 8-22
  **crackles** springing off the skin (feet 1.5-3 units out, 5-12 units long, leaning outwards), 1-4 **long arcs**
  limb to limb (bowed out). Fading 1 .. 0.45 over the shock (was 0.35), thicker (1.0 / 0.9 vs 0.8). Each hit's own
  0.75 s arcs: 1 unit out along the true outside (were 0.5 inside). `maxEffects` 16 -> 32 (a room of shocked monsters).
- **Convulsions** (`vr_box3d.cpp shockRagdoll`): only ragdolls (the dead) as before, now easing with the arcs
  (0.45 + 0.55 x left) and letting go over the last 0.4 s (`shockLetGo`), so they end with the arcs (was: full for
  40%, then smoothstep to ~0 well before the end).
- **Labels:** VR Settings > Gore: header "Lightning Shock", toggle "Lightning Shock", slider "Lightning Shock
  Duration". The cvars keep their names (`vr_shock_death`, `vr_shock_death_time`, ...): no migration.
- **Measuring:** `vr_shock_info` now also measures the next frame's arcs against the body's drawn triangles:
  `bodyshock-arcs: entity points inside mean_out clear1` (share of arc points inside the closed mesh by a 3-ray parity
  test, mean signed distance to the surface in units, share at least 1 unit out).

Measured (grunt at 96 units in vrcalibration, `vr_shock_hit_test 1` alive then `-1`):

| | before | after |
|---|---|---|
| each hit's arcs: inside / mean distance / >= 1 unit out | 0.97 / -0.40 / 0.00 | 0.00 / +0.93..1.01 / 0.42 |
| lasting arcs: inside / mean / >= 1 unit out | 0.40-0.60 / +0.22..1.10 / 0.26-0.45 | 0.05-0.16 / +2.4..3.3 / 0.56-0.84 |
| lasting arcs a frame (fresh .. worn off) | 10-17 (kills only) | 32-41 .. 12 (living and dead) |

Durations: 3 s: living hit -> 2.94 s left, re-hit at +1 s -> 2.97 again, killed (no refresh) -> the corpse's arcs go on
(1.93 left), ragdoll convulses (relspin 3.6 at 0.94 s left, 0.66 at 0.23 s), arcs and convulsions both end at 0.00
(next sample: none). 30 s: living hit, kill at +5 s without refresh: arcs 24.6 s left on the corpse, convulsing
through (shockcheck left 0.52 .. 0.01), sampled every 0.2 s at the end: the last arcs at remaining 0.00 with left 0.00,
then none, both. While alive: `shockcheck: ragdolls=0` (no convulsions). Server/client end within ~0.03 s.

Perf (six living grunts shocked for 30 s, `--exclusive`, real time, two windows each): CPU busy 0.95-0.96 vs 0.61-0.62
ms with `vr_shock_arcs 0` (shock arcs 0.05-0.06 ms, scene lines ~0.14-0.2, the blue lights' dlight shadows ~0.15),
GPU 1.24-1.33 vs 1.00-1.07 ms. (`vr_shock_arcs 0` now skips the triangles too.)

Test in VR: shoot a grunt briefly with the lightning gun: arcs should crawl over it while it fights, carry on as it
dies and its ragdoll convulses until they stop together; Lightning Shock Duration at 30 s; are the arcs too many or too
far out (`vr_shock_arcs`), are the blue lights too much in a dark room. The default stays 3 s (his config has 3).
## Corner buttons on a flat screen (vr_menu_flat_shortcuts, 2026-10-06)

Vittorio: the top-left menu shortcuts also visible and usable in flat-screen mode. With VR off (`vr_menu_flat_shortcuts
1`, default; HUD and Menus > Menu Settings > Corner Buttons on a Flat Screen) every menu has them, for the desktop mouse
(`Quake/vr/vr_menuui.cpp` `ToolbarLayout::row`, `menuui::toolbarShown`/`toolbarRow`).

**Where.** Not the headset's labelled column: at the shipped `scr_menuscale 3` the desktop's menu canvas is the 320x200
menu plus 60 pixels above and below and 50-125 beside it, and that room is used: Ironwail's lists (Levels, Mods) start
at the canvas's left bounds, the VR pages' long labels reach 80-130 pixels left of the menu, the Search, Console and Map
Library pages fill the width. Only the strip along the canvas's top is free everywhere: Ironwail's bounds lists start
10 below it at least, their titles 4 more; the classic menus at the 320x200's top. So a row of the icons along the
top-left edge, 12 pixels tall, 1 below the edge (the headset's icons, buttons 17 wide); the button under the mouse (or
selected with the keys) names itself in a box under it. The row is drawn after the menu; the pages around it unchanged
(pixel diffs, flat, `vr_menu_flat_shortcuts 0` against `1`, 1280x720 and 1024x768 at `scr_menuscale 3` and 1,
1280x800: main, Options, VR Settings, Levels, Single Player, Search, Map Library, Mods, Key Setup differ only in the
row's box) except:
- the Console page: its text starts below the row (2 rows fewer; in the headset it starts right of the column);
- the VR pages' Quake plaque, in a window narrower than 16:9 at the largest menu scale (16:10, 4:3), where the row
  reaches x 16: drawn 12 lower, below the row.

**Mouse and keys.** A click on a button does what the headset's laser does (Back to Game closes the menu; Search,
Console, Advanced VR, Levels, Map Library, Checklist at Menu Detail: Developer open their pages). The mouse lights a
button up only once it has moved over the menus (the menus' mouse stays where it was last, or where the laser left
it). Keys: up from a VR page's first setting (or down from its last, or a stick click on any menu) selects the row's
first button; left and right move along it, Enter presses, down goes back to the page's first setting, Escape gives the
selection back.

**Tests** (`vr_mock_mouse <x> <y> | back | search | console | advanced | levels | maps | checklist [click]`: the desktop
mouse moved there through M_Mousemove, and a K_MOUSE1 click; `vr_mock_key <key>`: a key pressed and released). Flat
1280x720: each button's click opened its page (Search 133, Console 134, Advanced VR Options 1, Levels m_maps, Map
Library 142, Checklist 72; Back to Game closed the menu). Keys: 20 ups on VR Settings selected Back to Game, four rights
Levels, Enter opened it; a right then down went back to VR Settings' first row; a right-stick click on the main menu,
two rights, Enter opened the Console page. `vr_mock_laser`'s names were off by one past Search (`advanced` pointed at
Console; its table had 5 names for 7 buttons): now all seven.

## Spectator camera switch on every menu (2026-10-06)

Vittorio: the "Spectator mode" banner at the bottom always visible, saying on or off, and clickable to switch it. The
reminder at the menus' bottom left (shown only while the window showed the spectator camera) is now on every menu in
the headset (the VR menu style): "Spectator camera: On" with the red recording light, or "Off" with a dim one, the
state in white; it lights up under the laser like the corner buttons (with a tick), and a trigger click switches it
(`Quake/vr/vr_menuui.cpp` `bannerLayout`, `bannerAt`, `toggleSpectator`). On: Window View (`vr_window_view`) to
Spectator Camera, and the desktop mirror (`vr_mirror`) to Left Eye if it was off (the window shows no view without it);
Off: both back as they were (the view the switch found, Left Eye (raw) if it found the camera already on). Where: its
right edge 8, as the column's, with "Spectator: On/Off" where the long text does not fit, else in the corner as before.
Not on a flat screen: the window is the game there, no headset view to mirror or film. `vr_mock_laser spectator` puts
the laser on it.

Tested (mock headset, e1m1, main menu): Window View Smoothed Mirror -> click -> 2 (On) -> click -> 1; Window View 0 and
mirror 0 -> click -> 2 and mirror 1 -> click -> 0 and 0. Eye images (`vr_eyeshot 3`): Off, On under the laser, On.
## Benchmark suite prepared (vr_bench, qvrbench.py, kit bench.sh; 2026-10-06)

For the coming benchmarking and profiling round: 44 fixed scenarios and a runner, no measurements yet
([BENCHMARKS.md](BENCHMARKS.md)).

- **Engine** (`Quake/vr/vr_bench.cpp`): `vr_bench_begin <name> [frames | <seconds>s]` / `vr_bench_end` record every
  frame's period, host time, CPU work (less the runtime's and the swap's waits), the eyes' and the whole 3D refresh's GPU
  time, each always-on phase's CPU and GPU time, traces, draw calls, alias models, main-thread heap events, and every
  16th frame the edicts, monsters alive, Box3D bodies, particles, decals and lights; then write
  `quakevr/profile/bench/<name>.json` (avg, p50, p95, p99, max; hitch counts; the settings). `vr_bench_seed <n>`
  restarts QuakeC's random numbers. Debug > Profiling and Memory > *Benchmark Capture (10 s)*. The profiler's
  always-on phases gain `3D` (SCR_UpdateScreen's 3D refresh: flat mode had no GPU time before), and the profile CSV's
  settings header is shared with the JSON (`profile::settingCvars`, now with the retro, portal, mock and fixed-frame
  cvars).
- **Scenarios** (`Misc/quakevr/bench/qvrbench.py`): idle (E1M1 in VR, flat and QRP; the firing range; the hub flat),
  teleporters (the episode gate's view, off, flat; 24 grunts across it), combat (48 mixed monsters; with the spectator
  camera; in bullet time; 64 monsters' AI alone; scripted jabs into 8 grunts), physics (500 props active and settled,
  32 ragdolls active and settled), effects (dense particles, 3 and 18 explosions a second, 1024 bullet marks streaming,
  4096 blood marks, 32 torches), lights (32 shadowed lights and the control, the flashlight), liquids (the range's
  pool above and under, E1M1's slime, E1M7's lava), textures (QRP parallax on and off), the VR menu open, a map change
  and `timedemo demo1`, and tours of six complex custom maps already on this machine (warden, apsp3, ad_grendel,
  ad_soltower1e, basetohell, vanisch01; `bsp_waypoints.py`), plus `combined`.
- **Runner** (kit `bench.sh`, `bench_maps.ps1`, `bench_sheet.ps1`): N repeats in alternating order, exclusive and
  paced at 90 Hz by default; `summary.md`/`.csv`; `compare` with a noise threshold and notes when the two sets'
  builds or settings differ; `--validate`.
- **Validated**: every scenario twice, fast and shared: all load, set up the same edicts, monsters and bodies in both
  runs (decals within 5%), and write their JSON; the contact sheet shows the intended views. Found on the way:
  `perf_suite.py`'s `vr_retro_particles` no longer exists (its retro particle variants set nothing); `vr_flashlight_give`
  needs a hand (`left`).

## The author's parallax, combat and retro settings as defaults (config 89, 2026-10-06)

His config (snapshots 12:13 and 12:37) against the defaults, for the cvars on Graphics > Surfaces' Parallax rows, the
Combat pages (by the menu: Melee, Parry and Bash, Stamina, Batting and Catching, Damage and Knockback, Weapon Damage,
Enemy Weapons, Enemy Shoves, Knockdowns, Bullet Time, Burning) and Graphics > Retro Textures / Retro Lighting:
- **Parallax:** `vr_parallax_distance` 512 -> 1024, `vr_parallax_grazing` 86 -> 90 (no fade), `vr_parallax_depth_write`
  0 -> 1.
- **Combat:** `vr_pain_knock_strength` 0.75 -> 0.6, `vr_pain_knock_max` 10 -> 8.5, `vr_pain_knock_time` 0.35 -> 0.3,
  `vr_burn_flames_max` 7 -> 12, `vr_burn_self` 0 -> 1, `vr_burn_drop` 0 -> 1, `vr_knockdown_wiggle` 0.7 -> 1,
  `vr_knockdown_wiggle_frequency` 1.2 -> 2.2, `vr_knockdown_wiggle_pause` 1 -> 0, `vr_parry_stagger` 0.35 -> 0.75,
  `vr_counter_damage` 1.2 -> 1.5 (its `vr_default` line dropped from vr_defaults.cfg: the compiled 1.5).
- **Retro textures:** every kind but Liquids (which he left as shipped): Block 0.5 (0.25 for Your Arms, Torso, Legs and
  Particles), Average off, Smooth Beyond Never (-1), Quake Palette 1, Dither 0.5 (`shippedLook`, vr_retro.cpp; the All
  Categories panel and the override editor keep paramInfo's values). **Retro lighting** already matched.
Config 89 moves a setting still at its old default to the new one; changed ones are kept (the retro kinds' by
`retro::migrateShippedLook`). Checked: a fresh config shows all 104 new values; a config at version 88 with the old
values migrates them all, and five custom values (parallax distance 768, largest knock 12, world block 2, hands palette
0.3, parry stagger 0.5) stay. Not changed: `vr_dummy_gib` (0 -> 1 in his config; not on a Combat page).
## Menu status box fits its text; spectator camera preview in the menus (vr_spectator_preview, 2026-10-06)

Vittorio (NOTES.md vrfiringrange_2026-10-06_12-28-18): the top right status box's text ran out of its background.
On a page whose rows are spaced out (`vr_menu_spacing`, the menu canvas's y scaled by it; the main menu keeps 1) the
characters keep their own size (`Draw_KeepMenuGlyphSize`) while the rows were placed in menu units, so they spread
past the box, which the Painter measures in true pixels. `VR_MenuDrawStatus` now places the rows in true pixels too:
the same box on every page (flat screen unchanged). Eye images (`vr_menu_spacing 2`, VR Settings): before, rows twice
as far apart and out of the box above and below; after, inside it; the rest of the eye image identical (pixel diff
only in the status box).

Vittorio (12-29-40): a small preview of the spectator camera in the menus while it is on, to see what will be
recorded. `vr_spectator_preview` (archived, default 1; Graphics > Recording > Spectator Camera > Preview in the Menus):
above the bottom left switch, as wide as it (within the column left of the menu), the window's shape, a tan frame;
smaller where the corner buttons leave less room, none under 48 menu pixels. It is the window's image (the mirror
pass: glow, tone, wobble, bullet time look) drawn small (384 wide, mipmapped) into its own target right after each new
camera image (`vr_stereo.cpp` `makePreview`, only while a menu places it: `menuui::spectatorPreviewWanted`), and drawn
by the eyes over the panel (`vr_menuui.cpp` `placePreview` in the 2D pass, `drawPreview` in the eyes), not into the
canvas: the camera sees the menu in the world, and would film its own preview. So the recording shows the menu
without it. The eyes show the image of the frame before (the camera renders after them).

Cost (e1m1, menu open, `vr_spectator_rate 1`, 250 fps, `vr_profile_gpu 1`): the preview pass 0.039 ms GPU, 0.001 ms
CPU a camera image; the eyes' quads within the HUD panel scope's 0.001 ms. At the default rate (60 images a second)
less. Tested: eye images with the camera Off (identical to before but the status numbers), On (VR Settings page and
main menu: the preview above the switch, matching the window); the window's screenshot has no preview.
## Custom maps: the one that did not start, and the crash (2026-10-06)

The author's seven installed packages (his `<Steam Quake>/cache/maps_installed.txt`), copied into a test base and
each played headless (`maps_play`, the Map Library's Enter, and `map`):

| Package | Start map | Starts | Cause / note |
|---|---|---|---|
| Base To Hell | basetohell | yes | |
| Grendel's Blade | ad_grendel | yes | made for Arcane Dimensions: 523 entities without a spawn function (monsters, breakables), AD's sky |
| Solomon's Tower | ad_soltower1e | yes | Arcane Dimensions: 112 entities without a spawn function |
| November-Schlacht | vanisch01 | yes | its sky missing: installed by an older build (files kept under `vanisch01/`); installing it again fixes it |
| The Warden | warden | yes | Arcane Dimensions (tagged; laid out from the base dir): 216 without a spawn function |
| Ancient | ancient | yes | made for Copper: 17 without a spawn function |
| **Down the Gutter** | (none in the index) | **no** | two bugs, below |

- **Play needed the index's `startmap`, which 941 of the index's 1947 packages lack** (Down the Gutter among them): Play
  printed "does not say which map to start" to the console only. And 402 packages list several (`start e1m1 ...`, a
  speedmap pack's every map): Play passed the whole list to `map`. Now `mapinstall::startMap`: the index's map the
  package holds ("start" first), else the package's own BSPs ("start", else the first by name). `maps_play <sha>
  [map]` starts another; `maps_installed` says what each starts and what it was made for. The Map Library's detail
  says it too ("Play starts plaw01."), Play's failures are said there (not only a sound), and packages made for a mod
  whose progs Quake VR does not run (`madeFor`: the index's game folder, or the AD/Quoth tag) say their monsters and
  items are missing.
- **Then plaw01 stopped at `Host_Error: Mod_LoadModel: progs/s_light2.spr not found`**: `monster_ogre_marksman` (id's
  own classname, an ogre in id's ogre.qc) is Honey's marksman outside the MG campaign, whose model (`progs/mogre.mdl`)
  and crown (`s_light2.spr`) Quake VR does not ship. Without them it is id's ogre again (its classname too). Any map
  with a marksman ogre stopped the same way (EXPANSIONS.md noted it on mge1m1 and MG3 map6). Honey's `misc_tree`
  (`progs/tree1.mdl`, not shipped either) is left out when its model is missing, as `misc_candle` was.
- **The crash** did not reproduce on the current build (each map played and switched between, `map` and Play, save and
  load across packages, an install and an uninstall while maps changed, the index re-read under a Map Library spamming
  keys). His build lacked 140234b3 (the Map Library's results kept pointers into a replaced index: a start-up fetch that
  fell back to the cache, then the page's draw or Play read freed text), the likeliest cause; 7c727f91 (va() and the
  console off the main thread during a map load) the other. One window of the same bug remained: the index is replaced
  at a frame's end and the next frame's keys ran before the page rebuilt its list. `mapsKey`/`mapsMouse` rebuild it
  first now.
- **Diagnostics**: a player's run (not only a test run) writes `qvr_crash.txt` and `qvr_crash.dmp` in the working
  directory when it crashes (the exception filter only; the crash then ends as before, and the last report is kept),
  and the report's second line names the exe's link time and the map last spawned with its map package
  ("exe linked 2026-10-06 12:56; map plaw01, map package Down the Gutter (f2d56926c6082753)").
## Training dummy dies as a grunt; head pop chance checked (2026-10-06)

NOTES.md vrfiringrange, 12:28: the dummy "always seems to just completely gib"; 12:26: head pops "no matter the distance
and no matter what chance".

- **The dummy** (`QC/vr_dummy.qc`): with Gore > Training Dummy > **Dummy Dies** (`vr_dummy_gib`, renamed from "Dummy
  Gibs"; still off as shipped, on in the author's config) a blow that takes a grunt's 30 health (over the run of hits
  less than a second apart, as before) no longer gibs it by its own rule (below -35). `VR_Dummy_Die` makes it the grunt
  it stands for (`monster_army`, its health before the blow, `army_die`, a grunt's box and movetype) and T_DamageImpl
  deals the blow to it as to a grunt: Killed, so a slash at its head beheads it, a shotgun, super shotgun or bolt
  headshot pops its head by the Head pop chance, an overkill below -35 gibs it, otherwise it dies, falls as a ragdoll
  and lies as a corpse (Gib Corpses, decapitating corpses: all a grunt's). The blows are armed on it as on a grunt
  (`VR_Decap_HeadModel`: the dummy has a grunt's head). Kept the dummy's: not counted (`Killed`: `vr_dummy_dead`), no
  backpack or weapon drop (`SoldierDrop`; its own gun still falls from its hands, as a grunt's), the report line ("-
  killed"), and a stand-in standing again `vr_dummy_gib_respawn` s later (its own body under it no longer blocks it).
  The decapitation tests (`vr_decap_test`, Debug > Gore Tests) work on it: its health set below 1000 is the grunt's for
  the blow (1, 9, 6, 12, 13, 14: beheaded or popped, as on a grunt).
- **Head pop chance**: no bug. His game ran a progs.dat built at 11:00, before the chance shipped (11:56): every headshot
  kill popped. Measured with his config (`vr_decap_pop_always_range 2`, `never 12`, SSG 1.25 falloff 1, SG 0.75
  falloff 4, pellet weight 0.75, head share 0.5) and real shots (mock hand, `+attack`) at grunts spawned through the
  firing range's dispenser (`impulse 241`), 24 shots each: pops follow the rolled chance. With his super shotgun scale
  1.25 the chance is capped at 1 out to about 4 player lengths when most pellets strike the head (by design).

| target, weapon, range (units) | kills | pops | rolls (mean chance) | died whole | gibbed |
|---|---|---|---|---|---|
| grunt SSG 150 | 24 | 24 | 2 (0.95) | 0 | 0 |
| grunt SSG 250 | 24 | 15 | 20 (0.70) | 5 | 4 |
| grunt SSG 350 | 24 | 5 | 14 (0.44) | 19 | 0 |
| grunt SSG 450 | 16 | 2 | 8 (0.28) | 14 | 0 |
| grunt SG 150 / 250 / 350 | 24 / 15 / 5 | 16 / 2 / 1 | 24 (0.66) / 15 (0.28) / 5 (0.10) | 8 / 13 / 4 | 0 |
| dummy before, SSG 150 / 250 | 13 / 1 | 0 | 0 | 0 | 13 / 1 |
| dummy before, SG 150 | 0 | 0 | 0 | 0 | 0 |
| dummy after, SSG 150 / 250 / 350 / 450 | 24 / 24 / 16 / 4 | 20 / 7 / 3 / 1 | 16 (0.81) / 14 (0.66) / 10 (0.39) / 4 (0.28) | 3 / 17 / 13 / 3 | 1 / 0 / 0 / 0 |
| dummy after, SG 150 / 250 | 10 / 4 | 5 / 0 | 10 (0.62) / 4 (0.27) | 5 / 4 | 0 |

(The dummy was shot from its side, the grunts head on: the hits differ, the rules are the same.)

## Music read in place from the owned installs, per campaign (2026-10-06)

Why it was silent: the original Steam Quake ships no music at all (its id1/hipnotic/rogue have only paks); the
soundtrack and the mission packs' are loose OGGs in `rerelease/<game>/music/`, which Quake VR never mounts (only
dopa/mg1/mg3 are mounted from the rerelease, so mg3's music already played). Ironwail's bgmusic only looks on the
search path, so id1, hipnotic, rogue, dopa and mg1 maps had no track.

Now (`Quake/vr/vr_music.cpp`, `VR_FindMusicTrack`, called by `BGM_PlayCDtrack`):
- A game folder that is no campaign's (a mod, a map package, quakevr) with the track wins, as the search path had it.
- Else the active campaign's folder (`vr_campaign`), then id1: first where the search path has that folder (loose or
  in a pak), then the owned roots read in place: `VR_OwnedReadRoot` (vr_gamedir.cpp: `ownedRoots()` minus the base
  dirs: Steam/GOG/Epic rerelease, `<basedir>/rerelease`, `<basedir>/../rerelease`), then the GOG original install
  (`<root>/id1/music` or `<root>/music`). Nothing is mounted, copied or written; com_basedirs is untouched.
- Else another campaign's track still on the search path (the old choice), else one console line per track
  (`VR music: no track N ...`, [skipnotify]; tracks 0/1 silent). The old per-map "Couldn't find a cdrip" is gone.
- Files outside the search path open with the new `S_CodecOpenStreamAt` (snd_codec.c: path, offset, length).
  The same-track resume compares the resolved file, so hipnotic's track 4 then id1's track 4 switches.
- Each start logs `VR music: track N -> <file> (from <root>)` to the console. bgmvolume, bgm_extmusic,
  -noextmusic and CD audio first are unchanged.

Verified headless (with -Sound): e1m1 -> rerelease/id1/music/track06.ogg, hip1m1 -> rerelease/hipnotic track04,
r1m1 -> rerelease/rogue track05, e5start/e5m1 (dopa) -> id1 track04/03, mg3 start -> rerelease/mg3 track09 (mounted),
e1m1 twice -> one start (resumed); decoding advanced (145 KB of track06 read in 5 s). `-nosteam -nogog`: one line per
missing track, nothing else. Untested here: a track inside a pak, the GOG original layout (not installed).

## Flashlight cord: the low-poly chain only (2026-10-06)

The author: the low-poly chain and no chain the only cords. Branch `agent/flashcord`.

- **Cord** (Body > Flashlight / Advanced VR Options > Flashlight): None or Low-Poly Chain, nothing else;
  `vr_flashlight_cord` 0 none, 1 the low-poly chain (default 1, compiled and `vr_defaults.cfg`). The coiled cord, the
  plain cable and the smooth chain are gone (`vr_coil.cpp`: the helix and the smooth chain's distance detail;
  `coil::Style::turns`, `coilRadius`, `lowPoly`: `chain` is the low-poly chain now). The plain cable stays as the
  chainsaw's starter cord. The chain's style, physics and swing are unchanged (same line, springs and links).
- **Config 90**: `vr_flashlight_cord` 2, 3 or 4 (1 and 0 already mean the chain and none) -> 1.
- Checked (mock): migration from 89 with 0/1/2/3/4 -> 0/1/1/1/1, a version-34 config's 1 -> 1, a version-90 config
  untouched; e1m1 torch in the left hand, both eyes: 288 rings x 4 sides, 32 links before and after; the eye images
  differ from the old Low-Poly Chain's (max-channel > 2 on 0.6-0.8 % of pixels) less than two runs of the new build do
  (1.3 %). `vr_flashlight_cord 0`: not drawn.

Checklist:

- [ ] Body > Flashlight > Cord: two choices, None and Low-Poly Chain; the chain hangs and swings as before.
## Map download cache capped (vr_maps_cache_mb, 2026-10-06)

The Map Library's zips (<base>/cache/maps/<sha256>.zip, never read again after unpacking) were kept forever. Now
`vr_maps_cache_mb` (archived, default 512; Debug > External Map Index > Download Cache Size) caps them: the oldest (by
last write) are removed before a download (room for its index size), after each job, on the first frame (the config
read: a start-up over the cap is trimmed) and when the cap changes. 0: nothing kept once installed (`maps_get`'s zip is
that job's result: kept until the next trim). A job that fails (download error, cancel, corrupt or refused zip) keeps
no zip. The running job's zip is never removed; uninstalling leaves the cache to the cap. Only names of 64 hex digits +
`.zip` that are files are counted or removed; anything else in the folder is left alone. `maps_cache [trim]` lists the
zips oldest first with the usage (Debug > Download Cache Usage). `files::fileSize` added (vr_files).
Tested headless with a local fake index/zip server (scratch, 127.0.0.1): cap 8 MB over three 3 MB installs (oldest
evicted first), a trickled download protected while the cap went to 0, a non-zip not kept, cap 0 removing the zip after
install, a start-up trim (6 -> 2 MB), and six off-pattern entries (txt, short/non-hex names, .zip.part, a directory
named like a zip) untouched.
## Firing range: buttons that vanished at a distance, labels' boxes too wide (2026-10-06)

**Buttons gone from afar** (NOTES.md vrfiringrange_2026-10-06_13-11-00). No distance cull hid them: the server sends
every entity in the PVS (no distance test), and the client culls brush entities by the frustum only. Ironwail's
gl_zfix (brush entities, not static ones, lose to a world face they are flush with) added 1/1024 to their clip-space
depth. With reversed Z that pushes a point back (1/1024) / near of its distance: 1/4096 at the desktop's near plane of
4 units, but 1/102 at the headset's 0.1 (vr_nearclip; 1/20 at its least, 0.02). A brush entity standing s units
proud of a world face sank behind it from about 102 s units away; vrfiringrange's west row of monster buttons (8
units proud of world panels) from about 820 (measured: seen at 806 units, gone at 910; the platform's sightlines reach
about 930). Now clip z moves towards the far plane's by 1/4096 (QVR_ZFixDepth, vr_glsl.h; the world vertex shader and
the parallax depth write): the push back is 1/4096 of the distance at any near plane, the desktop's as before.

| brush entity s units proud of a world face | gone from (headset, before) | gone from (now) |
|---|---|---|
| button, 8 (vrfiringrange's first row) | ~820 | ~32800 (none on any map) |
| plat or door edge, 4 | ~410 | ~16400 |
| trim, 1 | ~100 | ~4100 |
| item box (b_*.bsp) on a floor | pushed back 1% of its distance into the floor (8 units at 800) | 0.024% (0.2 at 800) |

Not affected: alias models, sprites, particles, static entities. The other distance limits found, none hiding gameplay
entities: shadows (vr_shadow_distance 1536), parallax relief (vr_parallax_distance 1024, fading), detail textures
(vr_detail_distance 144), fire particles (vr_fire_particles_range 1200), haze (1400), wet reflections (1280), dynamic
AO's occluders (2048), gl_farclip 65536; the server's entity list is capped by count (MAX_NET_EDICTS), not distance.

**Labels' boxes wider than their text** (NOTES.md vrfiringrange_2026-10-06_13-12-36). Not the menu status box's fix
(vr_menuui.cpp only). A world text board (vr_worldtext_crt) keeps its handle's largest page and was reset when the
map's model changed; a game directory change (the Map Library's packages) empties the models (Mod_ResetAll) and the
next map reuses another map's slot, so vrfiringrange took vrstart's boards and their widths. Reproduced with `map
vrstart; disconnect; game hipnotic; game quakevr; map vrfiringrange` (labels from the ogre's on 2-4x their text's
width). A board now belongs to the client's world text list (worldtext::clientGeneration, new at each clientReset).
Since round 15 (boards kept by map), seen since game changes became common.
## Release fixes from the installer research (2026-10-06)

The prerequisite fixes of `INSTALLER.md` (section 12), with Vittorio's decisions written into that document
(section 11).

- **Packaging is an allowlist** (`Windows/package-quakevr.ps1`): the game folder is what git tracks under `quakevr/`
  (less `.gitignore` and `motions/`) plus `progs.dat`. `-DryRun` prints the file list (no build, no copy); `-Root`
  lists or packages another checkout. A dry run on the author's checkout: 1670 files, none of its untracked ones
  (`apsp3`, `ad`, `vrwip.*`, `warden.bsp`, `weight_test.csv`, `cache/` 103 files, `sound_tests/`, `bodycal/`, saves).
- **`ironwail.pdb` ships** (full PDB: clang-cl `/Z7`, lld-link `/DEBUG`); the packager fails without it. The crash
  report's DbgHelp now searches the exe's folder, then the working directory (the linker's path is tried first). A copy
  of the exe and pdb in another folder, run from a third folder, with the build's pdb moved away: the stack had names;
  with no pdb anywhere, only offsets.
- **Build version**: `quakevr.props` (`QvrBuildVersion`, before ClCompile) writes `$(IntDir)qvr_buildver.h` with the
  last commit's date and short hash (`-dirty` with uncommitted tracked changes), only when it changes; `VR_BuildVersion`
  (`vr_crash.cpp`; "unknown build" without it). Shown at start, by `version`, as the last line of VR Settings, and in
  `qvr_crash.txt`: `Quake VR 2026-10-06 9460b8e1-dirty, exe linked ...`.
- **Map Library zips are checked against the index's sha256** (`vr_sha256.cpp`, a portable SHA-256) before they are
  kept or unpacked; a mismatch fails that mirror with both hashes and tries the next. `vr_sha256_test` (Debug menu) is
  6/6 on the standard vectors. Fake index (two packages served from 127.0.0.1): the good one installed, the corrupted
  one "its sha256 is 13a7259c..., not the index's 2d5a9b9f... (a corrupted download or a changed file): not unpacked",
  and not cached.
- **Epic Games Store**: `ownedRoots` reads the launcher's manifests (`addEpicRoots`; `-noepic`; `-epicmanifests <dir>`
  for tests). Fake manifests with `-nosteam -nogog`: dopa "ready" from the Epic folder (`rerelease/` layout), mg1/mg3
  found with a root-layout folder; a "Quake II" manifest and a broken one ignored; `-noepic`: all missing.
- **Data-less pack folders read as missing**: `inspectPack` says "incomplete" only when a pak or one of the pack's
  files is there. A base with only `hipnotic/textures/`, `rogue/maps/<other file>` and `mg1/textures/`: before,
  hipnotic, rogue and mg1 "incomplete installation"; after, hipnotic and rogue "missing", and mg1 comes from the Steam
  rerelease (a data-less campaign folder no longer masks a lower root's copy).
## Lightning Shock options and smouldering bodies (vr_shock_living, vr_smoulder*, 2026-10-06)

Asked: an option for the lasting arcs on living enemies (both, or corpses only, to try out); smoke off what the
lightning strikes (living or dead) for 5-6 s to simulate the burns, and off burning enemies and corpses, carrying on a
while after their flames go out.

- **Arcs on the Living** (`vr_shock_living`, default 1 = as since 312f53ef; Gore > Lightning Shock): 0 brings back the
  older rule in `VR_Shock_Hit` (vr_shock.qc): no lasting shock is sent while the monster lives; the killing bolt and
  every bolt into a body start it as before. What a living monster keeps with it off: each hit's own short arcs
  (weapons.qc `bodyshock(t, 0.75)`, kind 3) and the burn where it strikes. Measured (grunt 96 units ahead,
  `vr_shock_hit_test 1` alive, then `-1`): on: kind 3 (0.70 s) and kind 4 (2.94 s) on the living grunt; off: only kind 3
  (0.70 s, then 0.22 s), `lasting_sent=0`; the kill then gives kind 4 (2.94 s) both ways.
- **Smouldering bodies** (`vr_smoulder.cpp`, client-side): a slot per body (entity number, 32 at most) holding when the
  lightning's smoke ends and when its fire goes out and its smoke ends after. Each lightning hit already sends
  QVR_SVC_SHOCK kind 3 (and 4 for the lasting shock) for the body: parsing it starts `vr_smoulder_time` s of smoke
  (refreshed by each bolt). A monster's or corpse's fire (vr_burning.qc `VR_Burn_Smoke`) sends a new builtin
  `bodysmoulder(t, left)` (QVR_SVC_SHOCK kind 5, 1/4 s units, the shock's own small message): when it is lit, again only when
  burning on puts its end at least 0.5 s later, and once when it goes out (0); doused in a liquid sends kind 6 (the smoke
  stops); gone or gibbed sends nothing (the client sees its model change, as the arcs do). It smokes at full strength
  while it burns and `vr_smoulder_burn_time` s after. Strength fades as the square root of the time left; wisps are due
  at 18/s x `vr_smoulder` x strength, each from a random point of the body's triangles as drawn now (so they follow the
  model and the ragdoll), mostly from parts facing up, lifting off along the skin's normal: `particles::smoulderSmoke`
  (CellSmoke like the torch's smoke, but light grey and thinner; `vr_smoulder_alpha`). Not beyond 1500 units; nothing
  saved up while out of sight.
- Cvars: `vr_smoulder` 1 (x, 0 off), `vr_smoulder_time` 5.5 s, `vr_smoulder_burn_time` 4 s, `vr_smoulder_alpha` 0.6.
  Menu: Gore > Lightning Shock > Smoke After Lightning / Smouldering Smoke / Smoke Opacity; Combat > Burning > Smoke
  After Flames. Debug > Gore Tests: Smoke Off the Bodies Near (`vr_smoulder_test [s]`: every monster and body within
  1000 units), Smouldering Bodies (`vr_smoulder_info`: the bodies and the wisps made since the last print).

Measured (wisps made each ~0.72 s, `vr_smoulder_info`):

| case | start | wisps per interval | last wisps | expected end |
|---|---|---|---|---|
| lightning on a living grunt | hit 1.85 | 12 14 12 9 8 6 4 1 0 | 7.22-7.94 | 7.35 |
| lightning on its ragdoll (killed first with the smoke off) | hit ~3.2 | 6 7 6 5 5 4 2 1 0 (density 10/s then) | 8.24-8.96 | 8.7 |
| torch blow on a living grunt (burns till 4.7, out 5.2) | lit 1.74 | 12 13 13 13 13 12 11 10 7 5 1 0 | 8.96-9.68 | 9.2 |
| torch blow on a corpse (8 s, out at ~10.3) | | 7 x 11 at full, then 7 6 5 4 3 0 (10/s then) | 13.4-14.1 | 14.3 |

Perf (7 grunts, 6 smoking, `--exclusive`, real time, 5 s): CPU busy 0.59 vs 0.53 ms with `vr_smoulder 0` (noise level;
the "smoulder" timer is under the report's threshold), GPU vr particles 0.10-0.14 either way; ~180 extra live particles
(825 vs 641). Screenshots (both eyes, `vr_eyeshot 3`): thin grey wisps rising off a burnt corpse and a lightning-killed
ragdoll, the same in both eyes; none with `vr_smoulder 0`.

Test in VR: shoot a grunt briefly with the lightning gun, alive and as a corpse: the smoke should rise thin off the body
for about 5 s, following it as it walks, falls and lies; set a monster on fire with a torch: smoke while it burns, a few
seconds after the flames go out. Is it too faint in dark rooms or too much (Smouldering Smoke, Smoke Opacity)? Try Arcs
on the Living off.
## Lit particles (vr_particle_light, 2026-10-06)

Voice note hip1m1, 13:10: blood particles "always seem to be full bright, even though I am in a very dark area";
particles should match the light round them.

- **What** (`Quake/vr/vr_particles.cpp`, "Lit particles"): every Quake VR particle is now `Lit` or `Emissive`
  (`Particle::lighting`, set by `classify` the first frame it is drawn; a preset may set it). Emissive: glows added to
  the scene, the explosions' and the tarbaby's colour ramps (`Explode`, `Explode2`, `Blob`), lightning, fireballs
  (`CellExplosion`), sparks and embers (`CellSpark`), `CellGlow`, `CellFire`, and everything of a lava splash but its
  steam. Lit: blood (drops, mist, trails, drips, specks), smoke and dust (bullet puffs, gun smoke, smoke trails, wood
  dust, chainsaw and torch smoke), chips and rocks, water splashes (drops, spray, foam, rings), and the wood chips
  (`woodDust`'s spark-shaped chips set `Lit`). The `switch`es over `Type` and `Cell` list every value (a new cell or
  type warns until it is placed).
- **How**: once a frame, before the instances are built (`lightParticles`, profile scope `particle light`), each lit
  particle's light, as the alias models get it (R_SetupAliasLighting): the lightmap under it (R_LightPoint, from 2 units
  up so one lying on the floor finds it) through the world's contrast and fill light (`lighting::lightCurve`, through a
  256-entry table each frame), then this frame's dynamic lights added as the world adds them (`r_lightbuffer`: Quake's
  `radius - distance` or DarkPlaces' falloff by `vr_dlight_falloff`, the flashlight's cone as `VR_SpotCone`; map lights'
  shadow entries and portal lights left out). 1 is Quake's full light, at most 2 (overbright, as the world); the
  colour is multiplied by it (instances and the splashes' pieces lying on the water). One light a particle, on the CPU,
  so both eyes, the spectator and the half-resolution path draw the same. With retro lighting on (and its models), the
  light is put in the models' levels (`vr_retrolight_model_steps`, `vr_retrolight_spacing`); retro particles' palette
  and blocks apply to the lit colour as before. Not the dynamic lights' shadows (a flashlight lights a particle behind
  a monster).
- **Cost**: a particle looks the lightmap up again only after moving 8 units across (16 up or down), from its 16-unit
  cell's kept trace (`lightTraces`, 4096 slots, per map: a burst's or a trail's particles share one); new cells at most
  512 traces a frame. On surfaces lit by a changing light style a quarter of them re-read their texels each frame (the
  flicker; no trace). `particles_dense` (vrfiringrange, ~5300 particles, `--fast`, 2 reps each): CPU `vr particles`
  0.10 ms off, 0.26 ms on; GPU unchanged (8.3 to 8.8 ms both, one 16 ms outlier run). `vr_particle_light_report`
  there: 0.14 ms a frame on average, 0.23 to 0.25 ms in its densest stretch (6500 lit, ~40 to 60 traces a frame).
  The first version cost 0.50 ms: three `pow`s a particle (the contrast) were most of it.
- **Before/after** (e1m1, frozen blood burst, `vr_particle_seed 7`, `vr_eyeshot` both eyes): a dark corner (`setpos 832
  2448 -328`): mean light 0.07, the blood's pixels luma 9.4 unlit, 1.1 lit (the corner's floor draws at 7 to 9 of 128);
  a lit room (`setpos 944 1008 -232`): light 0.99, 27 pixels differ (as before); the dark corner by the head-clipped
  flashlight: light 1.44 (lightmap 0.07, dynamic 1.37), the blood's pixels 34 unlit, 43 lit. Left and right eyes within
  a pixel value of each other. A large blood mist cloud enveloping the head is lit by the head torch as a faint red
  veil (lit at its middle, in the beam).
- **Setting**: `vr_particle_light` (archived, default 1), Graphics > Models and Effects > Effects > **Lit Particles**;
  the graphics preset Off (Quake's look) turns it off. `shadeAt` (the splashes' and mist's light at spawn) is 1 while
  it is on (they are lit as they move). Debug > Reports > **Particle Lighting** (`vr_particle_light_report`): the last
  frame's lit and emissive counts, mean light (lightmap share, dynamic share), colour luma lit against unlit, traces,
  shared, flicker reads, its time and the mean time since the last report.
- **Not touched**: Quake's own particles (`r_part.c`, used without Quake VR's protocol or with `vr_particles 0`) stay
  fullbright; sprites (explosions, bubbles) and decals are lit as before.
- **To test in VR**: blood in a dark corner (dark, not glowing), then under the flashlight (lit in the beam) and a
  muzzle flash; smoke and dust in dark rooms; fire, sparks, explosions, lava splashes and teleport sparks as bright as
  before; Lit Particles off for the old look.
## Training dummy: any enemy, its health over its head (2026-10-06)

The author: "Make gib mode the default ... make the dummy's health customizable ... a health bar or counter ... regens
after a few seconds of inactivity ... choose the type of dummy between all supported enemy types ... behave exactly like
the chosen enemy type ... melee recordings correctly capture these new settings".

- **Dummy Dies on by default** (`vr_dummy_gib 1`; config 90 moves a config still at 0).
- **Health** (`QC/vr_dummy.qc`): Gore > Training Dummy > **Dummy Health** (`vr_dummy_health`, -1 "Its Own": the
  enemy's spawn health, a grunt's 30), **Dummy Health Refills** (`vr_dummy_regen`, 3 s: after the last hit it fills up
  in 0.4 s; 0 never), **Dummy Health Bar** (`vr_dummy_healthbar`, on). The hits take it whole (as T_Damage takes them);
  it replaces the old "a grunt's 30 over a run of hits less than a second apart". At 0 with Dummy Dies it dies as its
  enemy; off, it stays at 0 and its line says "would kill". The board is a world text (the CRT boards) over its head:
  the enemy's name, a 12-cell bar and "health / full", turned (yaw, 4-degree steps) to face the nearest player, sent
  only as it changes; dead, it says "killed (stands again)". It replaced the old fixed "Training dummy" sign (Health
  Bar off: that sign, with the enemy's name).
- **Enemy** (`vr_dummy_type`, Dummy Enemy; `QC/vr_dummy_types.qc`, last in progs.src): 0 grunt, 1 enforcer, 2 knight,
  3 death knight, 4 ogre, 5 fiend, 6 shambler, 7 zombie, 8 vore, 9 scrag, 10 rottweiler, 11 spawn, 12 rotfish, and with
  the packs 13 gremlin, 14 centroid, 15 mummy, 16 wrath, 17 overlord, 18 electric eel (numbers are recorded in takes:
  never renumber). Left out: bosses, the invisible swordsman, the spike mine. The monster's own spawn function makes it
  on a new entity (`VR_Dummy_Make`: its classname set first, spawnflags cleared, not counted, gremlin count and
  deathmatch kept) and then its AI is taken away (th_stand/walk/run/missile/melee/vrshove and use cleared, its think
  the dummy's, MOVETYPE_NONE, health 1000, its death and pain kept aside). So everything keyed on the classname is the
  enemy's: hit zones (PositionalHead), the head it loses (VR_Decap_HeadModel), its gore, its hull (vr_mhull_*), its
  ragdoll, liquids. The live dummy is marked by `.vr_dummy_kind` (1 + its enemy), not its classname: `VR_IsDummy`
  replaced every `classname == "vr_dummy"` (combat, burning, carry, decap, grapple, liquids, small gibs test, weapons:
  the dummy-only head zone and grapple weight rows are gone, its enemy's apply). Its stand loop, pain animation and
  attack frames (vr_dummy_attacks: the first 4/9 the wind-up) and pain sounds are the enemy's own (from each file's
  $frame list). It stands on the floor it was dropped on (its enemy's box's bottom). Another enemy chosen: a new entity
  stands as it within a frame (not mid-blow, not while someone stands where it would); a pack's enemy without the
  pack is tried once a map on a stand-in and falls back to a grunt (said once). Killed: `VR_Dummy_Die` gives it back its
  enemy's death, pain and movetype; anything that kills it past its report (a zombie beheaded whatever its health) goes
  through `Killed` -> `VR_Dummy_Killed` (dies with Dummy Dies, else stands unhurt; the zombie's beheading is skipped on
  a dummy without Dummy Dies). No backpack from any (`DropBackpack`); a grunt's gun still falls.
- **Motion takes** (`vr_motion.cpp`, `vr_motion_play.cpp`, `vr_motion_review.cpp`): the target's class is
  `progs::targetClass`, "vr_dummy" for the dummy whatever its enemy, so old and new takes find it. A take of the dummy
  writes a `dummy` header line (vr_dummy_type, _health, _gore, _gib, _regen); `vr_dummy_type` and `vr_dummy_health` are
  applied by playback (placing settings; absent: 0 and -1) and QC `VR_Dummy_RetypeAll` stands the dummies as the take's
  enemy before placement (restored after, retyped back at the next think). `motion_synth.py --dummy-type N
  [--dummy-health H]`. TESTING.md: how to record per enemy.
- **Tests** (`vr_dummy_test 1..4`, Debug > Gore Tests > Training Dummy Tests). Headless, each of the 19 types:
  `vr_dummy_type n; vr_dummy_test 1; vr_decap_test 1; ...; vr_decap_test 12` from 2.4 m: model and box are the
  monster's (grunt -12..27, ogre -20..40, shambler and vore -32..64, gremlin -12..18, wrath/overlord/eel -16..32), zones
  head 1 / body 0 / legs 3 for grunt, enforcer, knight, death knight, ogre, zombie, scrag, mummy (none for fiend,
  spawn, rotfish, centroid, wraths, eel: no PositionalHead row; the dog's head misses the level shot); the slash at
  health 1 beheads grunt, enforcer, knight, death knight, ogre, fiend, shambler, zombie, scrag, rottweiler, gremlin,
  mummy (vore, spawn, fish, centroid, wraths, eel: killed, no head gib); the shotgun headshot pops grunt, ogre,
  mummy (others beheaded or killed as their own rules); every one died ("killed as its enemy") and stood again 2 s on.
  Health: 2 hits of 10 -> 10/30, full 30/30 3 s after; gib off: "0/30 - would kill"; vr_dummy_health 100 -> 100/100.
  A synthetic slash take written with `--dummy-type 4` played with vr_dummy_type 0: "dummy: stands as Ogre", hits
  "health 188/200", then back to a grunt. Eyeshots: the board reads "Ogre / bar / 180 / 200" in both eyes and the window.
  eval.sh could not run: the canary's takes are gone from quakevr-iw/quakevr/motions (archived); three synthetic grunt
  takes give the old progs' hits (punch 23.8 head, shove 4.8, slash 26.6 head "would behead"; the backhand varies run to
  run with either build).
## Relighting: glow images found as the engine finds them, stronger lamps, relighting in the game (2026-10-06)

The author: "the relight script should also [find] `.png` files"; "it should look for textures in every folder used by
Quake VR"; the pyramid lanterns (vrtutorial, e1m1) and hip1m1's ceiling lights "barely emit any light"; "could we
dynamically relight the maps in-game ... and then reload the maps while still being in-game".

- **Script** (`relight_maps.py`): a texture's glow image is looked for as `Mod_LoadTextures` and `vr_extmaps.cpp` do
  (`SearchPath`, `Glows`): `textures/<map>/` then `textures/`, `_glow` then `_luma`, `.png` then `.tga` then `.jpg`
  (`quakeimage.py`: standard-library PNG, TGA and baseline JPEG readers), in quakevr, rogue, hipnotic and id1 (paks
  before loose files, `--basedir` folders over `--quake`), else Quetoo's (`textures_quetoo`, `--extmaps-dir`).
  `--list-glows --list-textures` logs the folders and each glow image's file. On the test machine's PNG QRP the old
  script found none of QRP's glow images (it read `.tga` only): tlight10, tlight09, light3_7, tele_top... now light.
- **Stronger lamps** (`relight_textures.cfg`, now in `quakevr/`, game data: the engine reads it too): `strength
  fixture=1.4 glow=1.2 liquid=1`; tlight11 (the pyramid lanterns) 1.3; hipnotic's tlight02 and tlight01 1.5, reaching
  1.4 further. `--light-texture-strength` multiplies all. `relight_probe.py` measures the lightmap round each fixture:
  hip1m1 tlight02 29.9 -> 51.7, tlight01 24.2 -> 37.9; vrtutorial tlight11 35.3 -> 44.9 (relit and committed); screens
  of hip1m1's first hall 6.1 -> 15.4 mean, vrtutorial by a lantern 26.3 -> 67.1. tlight11 at 1.6 blew the walls by the
  lanterns out (93 mean). The id maps need relighting with the script to get it (`quakevr/relit` is not in the repo).
- **In the game** (`vr_relight.cpp`, `vr_relight_process.cpp`; *Graphics > Relighting*; `vr_relight_*`): a port of
  `glow_lights` (its sums Python 3.12's compensated ones), run on the map's .bsp (the script's copy if there is one:
  its water-vis kept), `light` in a process of its own (no window, below normal priority, all cores but one, in a job
  object that ends it with the game; `VR_Shutdown` stops it), its log read for the page's progress, the result in
  `<gamedir>/relit_custom/<game>/maps/`, which `VR_ModelFile` loads over `relit/` (`vr_relight_use`), then a save
  (`autosave/relight`, run at once so a script's waits don't delay it) and a load once its file is written; a restart
  where the game can't save. Parity: the lights given to `light` are byte-identical to the script's on e1m1, start,
  e1m6, hip1m1, r1m1 and vrtutorial (QRP base). Times: the texture lights 26-43 ms the first time on a map (glow images
  looked for with the file cache on), 3 ms after (`mem::Cache`); e1m1 relit in 1.1-1.5 s, e2m1 with bounce about 30 s.
  Headless end to end: start, progress lines (`Direct Lighting 85%`, `Indirect Lighting (pass 0) 36%`, `LightGrid`),
  the reload (`Loading game from autosave/relight.sav`, the player where he was, `relit_custom/id1/maps/e1m1.bsp`
  loaded), cancel, and a quit mid-relight (no light.exe left).
- **Licence:** ericw-tools is GPL-3; run as a separate program on files it is an aggregate (docs/RELIGHTING.md,
  "ericw-tools' licence"). `package-quakevr.ps1` ships light.exe, its Embree and oneTBB DLLs, `gpl_v3.txt`,
  `LICENSE-embree.txt` and a notice; the release page must offer ericw-tools 2.0.0-alpha11's source beside the package.
- Fixed on the way: `id_light_values` took a classname's value "light" for the key (a light of "light" "0" whose
  classname came first lost its origin; no id map has one).
## Elbow tucked by the face (vr_body_elbow_tuck, 2026-10-06)

Voice notes vrstart 13:05 and 13:07: aiming down the sights (one or two hands) or holding the axe up by the face, the
elbow "either goes all the way outwards with a very steep and acute angle, or all the way inwards"; it should stay back
and down by the torso, even a little behind it.

- **The cause.** With the hand by the face the arm is folded (the wrist 0.35-0.7 of the arm's length from the
  shoulder), the elbow far off the shoulder-wrist line, and two things swung it round that line: the wrist's ease
  (`easeWrist`: a gun pointed forward with the forearm upright is a 90-100 degree wrist bend, so the elbow swung 50-98
  degrees out by the shoulder or across to the neck to ease it) and the pole's hand term (the hand's roll: a pronated
  axe grip turned it out) plus the wings (Elbows Spread: palm down by the face).
- **The fix** (`vr_avatar.cpp`, `elbowTuck`, `solveArm`, `easeWrist`): a drawn arm is *tucked* (0..1) with the wrist
  within `vr_body_elbow_tuck_near` (0.6) of the arm's length from the shoulder, fading out by `vr_body_elbow_tuck_far`
  (0.9), only with the hand in front of the shoulder and not raised to the forehead (a hand there or behind the head
  lifts the elbow up and out as before). Tucked, the pole turns down and back (`vr_body_elbow_tuck_back` 0.8 per down,
  half the rest's outward, no hand-roll term), the wrist's ease counts the strain at 0.15 and the swing at 7 times (a
  little swing for a hand turned past a wrist's reach, never round to the neck), and by the face or with the hand
  pointing up the wings don't spread it. Everything is continuous in the hand's pose (smoothstep fades; the swing keeps
  its 0.05 s ease). `vr_body_elbow_tuck 0` is the old IK exactly. The body-off forearm (wrist gadget) is unchanged.
- **Menu:** Body > Arms > Elbows and Wrists: Elbow Tuck, Tucked Elbow Back, Tuck Within, Tuck Fades By (archived, kept
  out of `vr_savedefaults` with the other arm settings).
- **What it costs:** the wrist bends more by the face (median strain at the face 0.77 -> 1.22; a pistol 18 cm before
  the eyes pointed forward bends it about 100 degrees, as the forearm stands upright under it). How far back the elbow
  goes is set by where the hand is: with the hand at the face the elbow can't be behind the shoulder (the arm's lengths
  put it 20 cm in front); at the chest it hangs 4-8 cm behind it, where the torso's capsules stop it (Tucked Elbow Back
  past 1 only swings it out of the torso).

Measured (`Misc/quakevr/armik/`, the author's calibration and arm settings, right arm, body axes in cm):

| pose | before: out, down, swing | after |
|---|---|---|
| ADS, hand forward by the right eye (faceR 70 0 0) | 23 in, 0 down (at shoulder height), 87 | 10 out, 23 down, -9 |
| ADS, hand pointing a little down (faceR 40 0 0) | 24 out, 2 above, -55 | 16 out, 8 down, -16 |
| image pose ADS two-handed (look 35 down) | 13 out, 25 down, -27 | 5 out, 28 down, 0 |
| image pose axe by the face (130 -45 -60) | 22 out, 21 down, 0 | 10 out, 29 down, 0 |
| over the head, behind the head, low, far, across, rest | | unchanged (tuck 0) |

Orientation grid (4 places by the face and chest x 45 hand orientations, both arms; the elbow's swing from straight
down-and-back about the shoulder-wrist line): face mean 20 -> 13 degrees, by the right eye 21 -> 9, chin 24 -> 10, chest
24 -> 21; swings past 60 degrees 15 -> 0 by the face, 5 -> 5 at the chest (hands pointed far up, not tucked); elbows
above shoulder height less 8 cm by the face 27 -> 4. Continuity (`vr_mock_play` sweeps, every frame, `vr_debug_arm 2`):
the largest elbow jump per frame from the far reach to the face 1.27 -> 1.16 cm, rolling the hand by the face 5.1 ->
0.9, pitching it 0.94 -> 0.30, from low to over the head 12.4 -> 1.6 (16 -> 1 steps where the elbow jumped more than 3
times the hand's step plus 1 cm). The two arms mirror (mirrored poses: the same ranges, the wrist strain within 0.3, from the swing's ease carried between poses).

- [ ] Aim down the sights one- and two-handed, the shotgun by the eye: the elbow down under the shoulder, not out to the
      side or across the chest. Too stiff (the wrist bent too far)? Lower Elbow Tuck (0.5).
- [ ] Axe up by the face, rolled both ways: the elbow stays down. Reach out from there: it fades back smoothly by
      Tuck Fades By (0.9 arm).
- [ ] Hand on the chest, then a look at the wrist gadget (forearm level, palm down): still spreads like wings.
- [ ] Scratch the back of the head, salute: the elbow lifts up and out as before.
## The author's combat decisions: fiend head zone, ragdolls, axes in props, gibs stick (2026-10-06)

### Fiend head zone (positional damage)

His answer: the fiend gets a head zone. It had none for positional damage (`PositionalHead`, weapons.qc): only
decapitation knew his head (`VR_Decap_HeadZone`'s own branch), so a shot or a blow at it did body damage (x1; the old
"his head is an extremity, 2/3" in "Ragdolls for more monsters" was a blast's falloff, not a zone). Now
`PositionalHead` has his ($stand1: 27 forward, 5 *below* his origin, radius 8, the numbers decapitation used) and
decapitation's own branch is gone (one source). His head lies below his origin, which is his legs' zone too: Head
Priority (`vr_hit_head_priority`, on by default) gives it to the head; with it off a fiend's head reads as legs.
`monster_ogre_marksman` (a map's marksman that keeps its classname: Honey's model present, or a Machine Games
campaign) takes the ogre's zone (decapitation already threw an ogre's head for him, but he had no zone to cut).

Monsters with a head zone (positional damage, so headshots, melee head hits, head pops and beheading): grunt, ogre
(and marksman), enforcer, gremlin, death knight, knight, shambler, vore, zombie, scrag, mummy, fiend (new); the player
and the training dummy. Without: rottweiler (decapitation has his lunging head: beheaded, no headshots), spawn,
rotfish, Chthon, Shub-Niggurath, Rogue's and Hipnotic's others (scourge, sword knight, wraths, morph, dragon).

Tests (e1m1, a fiend 96 units ahead, `vr_debug_damage_numbers 1`): the precise hits' ring (impulse 238): head ring
0/24 headshots before, 15/24 on his model and 7/24 on boxes after (from behind and the sides his shoulders come
first), chest ring unchanged (0 head). `vr_decap_test 17` (shotgun at the head, health 500): "6 pellets, 6 head
(x1.50)"; `vr_decap_test 7` (a sword's slash, full health): "melee: slash ... head (x1.50)"; `vr_decap_test 1`:
beheaded, h_demon thrown at 167 u/s.

In VR:
- [ ] Shoot a fiend in the head (Show Damage Numbers): "head (x1.50)"; slash it: the same, and at a kill his head comes off.

### Ragdolls on by default: already so

His answer: masses and the blast's throw by mass stay; ragdolls on by default. They have been since config 84
(2026-10-03, "Ragdolls on by default"): `vr_ragdoll` "1" compiled in, a saved 0 moved to 1. Nothing to change; checked
again: a config at version 83 with `vr_ragdoll 0`, `vr_migrate_config`: "1"; the test config (version 34): "1"
(default). Every monster with a rig killed with the defaults (`vr_test_spawn_dead 1`, 5 s later `vr_ragdoll_list`):
grunt, ogre, shambler, scrag, knight, death knight, rottweiler, enforcer, fiend and gremlin limp and asleep (4.2-4.6 s
limp); the zombie and the mummy lie down without one, as designed (a ragdoll only when beheaded).

### Thrown axes stick in props, explosive boxes too

His answer: an axe sticks in boxes, crates and props in general; explosive boxes don't catch fire (metal). Every Box3D
prop already took a stuck axe (`vr_axestick.cpp`: the props' ray, kind 2: kept in the prop's frame, it moves and turns
with it; `VR_AxeStick_Think` drops it when the prop goes or its model changes: blown up, broken, a pickup taken)
except the explosive boxes, which rang it off (`vr_axestick_metal` 0). Now **Axes Stick in Explosive Boxes** is on by
default (`vr_axestick_metal` "1"; config 92 moves a saved 0). The only blade with an edge test is the axe's: swords and
other thrown things don't stick, as before. Explosive boxes still never burn (`VR_Burn_Metal`).

Test aid: `vr_test_axe_host` 1 or 2 with impulse 208 (Debug > Tests > Thrown Axe: Push the Props Axes Are In, Break the
Props Axes Are In): each prop an axe is stuck in pushed up and across (`physicspush`), or broken (1000 damage; a pickup
taken by you).

Tests (e1m1, the prop 96 units ahead, `vr_test_axe_at 3`, blade first at 10 m/s, `vr_debug_axestick 1`):

| prop | stuck | pushed: the prop's corner, the axe | broken |
|---|---|---|---|
| explosive box (40 kg) | kind 2, 1.6 units deep | 464,-272,48 -> 517,-271,60; axe 480,-273,80 -> 543,-273,84 (turned with it) | blew up, "Axe falls out of explo_box" |
| small explosive box | kind 2 | 464 -> 515; axe 480 -> 531 | blew up, falls out |
| small crate | kind 2 | 480 -> 539; axe 480 -> 539 | vr_crate_broken, falls out |
| large crate | kind 2 | 480 -> 540; axe 480 -> 540 | vr_crate_broken, falls out |
| item_shells | kind 2 (a sidearm throw) | | |

`vr_burn_test 1` and 3 at an explosive box: "an explosive box (metal) can't burn". The test can't hit a health box
blade first: a horizontal throw at a box 16 units tall meets it with the handle's end (the test's aim, not the rule: a
pickup is a prop as any other).

In VR:
- [ ] Throw the axe into an explosive box, a crate, a health box: it sticks; kick or carry the box, the axe goes with it;
  blow it up or break it, the axe falls.

### Thrown gibs almost always stick to walls

His answer to "gibs too heavy to splat on walls: lower the splat speed or the masses?": lower the splat speed, gibs
should almost always stick. Read as sticking (the masses stay): Speed to Stick is already his 20 u/s
(`vr_gore_stick_speed`), so the misses were elsewhere. Two causes, measured: the roll (Thrown Gibs Stick, his 0.75), and
the hit test (`VR_Gib_Think2`): a hit on a wall was a turn of half the gib's whole speed in one frame along a 40-unit
look ahead its way; a gib lobbed into a wall falls faster than it goes in, its turn was under half, and it slid down and
landed instead (and Box3D's contact can take two frames). Now: what it struck is what lies along its way, else straight
across (its way down meets the floor first); a hit is its speed *into* that surface (at Speed to Stick or more) turned
by a quarter or more in a frame; and **Thrown Gibs Stick** ships at 1 (`vr_defaults.cfg`; config 92 moves his 0.75).
Gib Splat Speed (250, the speed a thrown gib *bursts* at) is unchanged: lowered under 200 his 8 kg gibs and heads would
burst at a hard throw instead of sticking; if bursting was meant, that is the setting. A gib let go of without a throw
(slower than Speed to Stick) is never watched: a drop doesn't stick.

Test: `vr_smallgibs_test 22` (new; Debug > Gore Tests > Thrown Gibs Stick, by Mass): gibs side by side thrown at the
wall as hand throws of 2 to 6 m/s make them (`throwvelocity`, so limited by their mass; 5-25 degrees up), 1.5 s later
stuck, burst or neither. e1m1's start facing south (`setpos 480 -352 88 0 270 0`, Step Up to the Wall Ahead: 64 units),
16 throws each; stuck of 16 before (his 0.75, the old hit test) -> after (release speed u/s):

| gib | 2 m/s | 3 m/s | 4 m/s | 5 m/s | 6 m/s |
|---|---|---|---|---|---|
| gib1 8 kg | 6 -> 15 (64) | 12 -> 16 (107) | 15 -> 16 (161) | 14 -> 16 (190) | 13 -> 16 (195) |
| gib3 12 kg | 9 -> 16 (64) | 13 -> 16 (106) | 13 -> 16 (133) | 14 -> 16 (136) | 12 -> 16 (136) |
| gib2 20 kg | 5 -> 16 (64) | 13 -> 16 (84) | 9 -> 16 (86) | 10 -> 14 (2 burst) (86) | 9 -> 16 (86) |
| grunt head 10 kg | 3 -> 16 (64) | 13 -> 16 (107) | 13 -> 16 (150) | 11 -> 16 (159) | 12 -> 16 (160) |
| ogre head 30 kg | 3 -> 16 (57) | 5 -> 15 (59) | 5 -> 16 (59) | 4 -> 13 (59) | 5 -> 14 (59) |
| small gib 0.3 kg | 6 -> 16 (64) | 11 -> 16 (107) | 13 -> 16 (163) | 15 -> 16 (226) | 0 (16 burst) -> 5 (11 burst) (283) |

(The two 20 kg "burst" at 5 m/s were gone, not burst: 86 u/s can't. The ogre's 30 kg head leaves one hand at 59 u/s
and often lands short of the wall; a small gib thrown at 6 m/s, 283 u/s, is past Gib Splat Speed and bursts, as
designed.) At a wall behind a drop (e1m1's north wall from the start, a ledge before it) the gibs fall short: 50-80%
there, the misses landing in the pit.

In VR:
- [ ] Throw gibs and heads at a wall from a couple of metres, softly and hard: they stick nearly every time; a hard throw
  of a small gib bursts.
## Blunt melee head pops by chance (2026-10-06)
## Blunt melee head pops by chance; Quad Damage always pops (2026-10-06)

The author: "Blunt melee head kills should be chance-based and weapon-based ... very rare for punches/crowbar/gun
butts to headpop", Mjolnir very likely; and "an option (default: on) to make head popping always happen while under
the effects of quad damage, with all guns, melee weapons, and even props/throws".

**Blunt melee head pops** (QC vr_decap.qc, "Blunt melee head pops"; Gore > Decapitation > Head Pop Chance). A blunt
blow's headshot kill (`QVR_DECAP_BLUNT`: a fist, a gun's butt, barrel or pistol-whip, the crowbar, a club, a pommel or
handle end, Mjolnir's head) pops the head at `scale x hardness^curve` (at most 1), rolled once when it kills
(`VR_Decap_Roll`, as the shots'). Class (`VR_Decap_BluntClass`): what the hand holds first (fist, gun, crowbar, club),
then Mjolnir's head (not its pommel), else a pommel. Hardness (0..1): `(1 - w) x` the striking part's speed
(`mh_speed`, m/s) on its way from Soft to Hard Hit Speed `+ w x` the blow's damage (positional head multiplier in; a
quad blow's own melee multiplier too) on its way from Soft to Hard Hit Damage. A corpse's head struck by a blunt blow
rolls the same (its speed the hand's, its damage the corpse strike's), it used to pop always. Slashes, the chainsaw and
thrown axes still cut heads off, sure. By Chance off (`vr_decap_pop_chance 0`): every blunt head kill pops, as before.

| cvar | default | menu row |
|---|---|---|
| `vr_decap_pop_fist_scale` | 0.04 | Fist |
| `vr_decap_pop_gun_scale` | 0.04 | Gun Butt |
| `vr_decap_pop_crowbar_scale` | 0.05 | Crowbar |
| `vr_decap_pop_pommel_scale` | 0.03 | Pommel |
| `vr_decap_pop_club_scale` | 0.25 | Club |
| `vr_decap_pop_mjolnir_scale` | 1.75 | Mjolnir |
| `vr_decap_pop_melee_soft_speed` / `_hard_speed` | 3 / 10 m/s | Soft / Hard Hit Speed |
| `vr_decap_pop_melee_soft_damage` / `_hard_damage` | 10 / 50 | Soft / Hard Hit Damage |
| `vr_decap_pop_melee_damage_weight` | 0.5 | Damage's Weight |
| `vr_decap_pop_melee_curve` | 2 | Hardness Curve |

Measured (`vr_decap_test 49`, 500 rolls a cell, on a grunt and on the training dummy alike; soft 4 m/s, medium 7,
hard 11; damage the weapon's base x 0.6 / 1.2 / 2 x the 1.5 headshot multiplier): chance soft / medium / hard:
fist 0.0002 / 0.006 / 0.023 (rates 0 / 0.010 / 0.024), gun butt 0.0003 / 0.007 / 0.027, crowbar 0.0015 / 0.019 / 0.050,
pommel 0.0009 / 0.011 / 0.030, club 0.002 / 0.046 / 0.170, Mjolnir 0.091 / 0.915 / 1.0 (rates 0.084 / 0.926 / 1.0).
Real killing blows (50-55, health 1): fist, gun, club not popped (rolled), crowbar popped once in two runs,
Mjolnir popped; `vr_decap_pop_roll 0` / `0.9999` force the roll both ways (the fist popped / not; the dummy too).

**Quad Damage always pops** (`vr_decap_pop_quad` 1, Head Pop Chance > Quad Damage: Always Pop). With Quad Damage
(`super_damage_finished`), every headshot kill pops the head: `VR_Decap_Roll` wins whatever the chance (the blunt
chance, the shotgun's ranges, a light thrown thing's), the blast's Head Share is waived (any pellet at the head),
a thrown sword, axe or chainsaw that doesn't stick edge first pops it too, a blade's blow at the head that doesn't cut
(a stab, too slow) pops it (`QVR_DECAP_QUAD`), and any other projectile at the head pops it: `VR_Decap_QuadArm` in
`T_Damage` (nails, rockets, grenades, lava nails, any mission pack projectile; the point is PositionalDamage's this
frame, else the projectile's own place on the target's box, not a blast's way off). A slash still cuts the head off
(not popped). The shots' own toggles (Shotgun, Super Shotgun, Lightning Gun) still apply; Thrown Things off is
overridden. Off: as without Quad.

Measured with Quad (impulse 255) and `vr_decap_pop_roll 0.9999` (every roll lost): blunt kills 50-55 all popped,
a nail (56), a rocket (57), a grenade (58) at the head popped; shotgun and super shotgun at 14 and 15.5 lengths
(past Never Beyond) popped, lightning at 15.5 popped; a 3.5 kg thrown shotgun (47) popped (without Quad: not), an
explosive box popped; the rate tests: 49 every cell 1.0, 45 / 46 (with spread, 12 lengths, chance 0.003 / 0.12)
1.0 of the 195 / 158 blasts with a pellet at the head. Quad on and the option off, or no Quad: unchanged
(nail, rocket at the head not popped, shotgun at 15.5 not popped, the 47 shotgun not popped). The 407d50eb tests
without Quad are unchanged: the chance table (40) identical, 41/42 at 3 lengths popped, 42 at 14.9 and 15.5 not, 43
popped, 44 body not, 45 (7 lengths) rate 0.14, 46 (3 lengths) 0.92, 47 rocket launcher popped. Tests 45/46 now roll
through `VR_Decap_Roll` (so Quad and `vr_decap_pop_roll` count there).

**Tests** (Debug > Gore Tests > Head Pop Chance Tests): 49 Blunt Melee Rates, 50-55 Hard Punch / Gun Butt / Crowbar
/ Pommel / Club / Mjolnir Kill, 56-58 Nail / Rocket / Grenade at Its Head, Give Quad Damage (impulse 255). The tests
stand the target (frame 0) first: a grunt in a pain frame let the made-up blow miss its precise head.

- [ ] Punch, pistol-whip, butt-strike and crowbar grunts to death in the head: heads almost never pop (a few in a
      hundred hard blows); slow taps never.
- [ ] Mjolnir swung into heads: nearly every solid blow pops; a gentle tap rarely.
- [ ] A wall torch swung as a club: now and then.
- [ ] Pick up Quad Damage: every headshot kill pops: shotgun across a room, nails, a rocket or grenade at the head,
      punches, a sword's stab, a thrown light prop or gun. A sword's slash still cuts the head off.
- [ ] Head Pop Chance > Quad Damage: Always Pop off: Quad changes nothing.
## Multiplayer: debris sides and melee timing as server rules (2026-10-06)

The author decided the two open questions of MULTIPLAYER.md: VFX-only debris is client-side, interactable debris is
server-side; melee speed is the server's. Details, the audit table and the numbers: MULTIPLAYER.md, "Debris and
effects" and "Server rules".

- **Audit:** every debris and particle-like spawner was already on its side (explosion chunks, casings, splinters,
  particles, decals on the client; crate pieces, rocks and bricks, gibs, heads, small gibs, rubble on the server). No
  spawner moved; single player is unchanged (e1m1: an explosion 215 -> 214 server entities and 16 client chunks; a
  crate break 215 -> 227, 188 -> 456 B a frame).
- **Rocks and bricks in multiplayer:** `vr_debris_mp_max` (default 0: none, as before; Rocks and Bricks > Most in
  Multiplayer). At 64 (29 pieces in e1m1) a remote client by a cluster went 256 -> 739 B of its 1400 B datagram.
- **Server rules** (`vr_serverrules.cpp`, `QVR_SVC_RULES` = 31): the melee timing cvars are sent to each client with
  its spawn state and on a change. A remote server's client shows them dimmed with the server's value, does not change
  them (Reset This Page skips them), and says so in the help. `vr_serverrules` prints them (Debug > Other).
- **`vr_net_stats [reset]`** (Debug > Other > Network: Entities Sent): entities in use; per client the entities in
  sight and sent, their bytes, the datagram's room, peak, mean and full frames (hook at the end of
  `SV_WriteEntitiesToClient`).
- **Two-game test:** `bash Misc/quakevr/multiplayer/mp_test.sh <agent> [tag] [vr_debris_mp_max]`: a listen server
  (`-listen 2 -port 26010 -ip 127.0.0.1`) and a second game connecting over UDP. `-ip 127.0.0.1` is needed: without
  it the server binds the host name's address (here a virtual adapter's) and `connect 127.0.0.1` gets no answer.
  Without `coop`, both players spawn at `info_player_start` and the client telefrags the host, so the host moves first.
  Result: the client showed "server 3 (yours 2.5)", then 5 the moment the host set 5; a crate break sent the client
  7 -> 18 entities, 176 -> 446 B; an explosion sent no debris entities, and the host and the client each simulated
  16 chunks of their own.
- **Found, not fixed:** `vr_physics_blast`'s explosion effect never reaches the clients (it runs between frames and
  the server frame clears `sv.datagram` first); impulse 232 (fling test) can pick a player's body (80.5 kg) as "the
  nearest prop" in multiplayer; resting pieces made after signon cost ~22 B a frame each (no baseline): a baseline
  sent when a piece comes to rest would cut that (MULTIPLAYER.md).

## Relighting many maps; a progress bar; cancel (2026-10-06)

Request: relight many maps in the game (whole episodes), with a progress bar, cancellable.

- **Batches** (`vr_relight_batch`, Graphics > Relighting > Many Maps; `vr_relight.cpp`, new `vr_relight_maps.cpp`): the
  map in play, an episode (by the map in play's name, `e1m3` -> `e1m`, `hip2m4` -> `hip2m`; or E1..E5 picked), a game
  (id1, hipnotic, rogue, dopa, mg1, mg3 or the map in play's), the Map Library's installed packages
  (`mapinstall::packageFolder`, new) or every map. Maps are found in the search paths' paks and `maps/` folders
  themselves (a campaign not being played, a package not mounted still count) and read from their own file (a pak's
  offset); only playable ones (an `info_player_*` in the entity lump; quakevr's 11 button and prop-table BSPs and the
  `b_*` boxes are not). In the qbase: e1 8 maps, id1 38, rogue 17, hipnotic 18, every map 79 (6 of them quakevr's).
- **Slots** (`vr_relight_process.cpp`): up to 8 light processes, each in the job object; `stopAll` terminates all, then
  waits for each. `vr_relight_parallel` 0 (auto) = 2 from 8 cores: on 32 cores (exclusive runs) e1 at the defaults
  took 7 / 6 / 5 / 5 s at 1 / 2 / 3 / 4 at once, with Bounced Light 1 34 / 33 / 33 / 42 s; two get most of it at
  two thirds of three's memory. light's `-threads` is (cores - 1) / parallel.
- **The settings** are taken when the batch starts (`Look`): moving a slider meanwhile changes nothing. The `.relight`
  gets `hash <fnv64>` of the settings text, light's options (not `-threads`), `relight_textures.cfg` and the map's
  file (the script's relit copy when used): a batch skips a map with the same hash and its .bsp and .lit there
  (`vr_relight_batch_force`, `-force`: not). `vr_relight` (Relight This Map) always relights.
- **Writing**: each output as `.tmp`, the old `.relight` removed first, then `.lit`, `.lux`, `.bsp`, `.relight` renamed
  into place; the work copies (`_work/<game>/<map>.bsp/.lit/.lux`) removed after, the logs kept. Cancel and quit stop
  every light, remove their work copies and write nothing.
- **Progress**: each map weighs its file's size (at least 200 KB); a running one by its stage (`stageFraction`; with
  bounce Direct 5-15 %, Indirect 15-86 %, from `developer 1`'s stage times: e1m1 with bounce 1 Direct at 0.26 s,
  Indirect 0.77 s, LightGrid 4.86 s, end 5.26 s). ETA = elapsed x (1 - p) / p. The page: status line (maps done of
  how many, the time), a bar (`progressBar` item: `menuui::drawProgress` in the VR style, Draw_Fill in Quake's) with
  the percentage and the time left, a line of the maps being lit. Outside the menu (`relight::indicator`,
  `vr_relight_indicator` 1): the wrist gadget's kills/secrets line becomes `RELIGHT 0/8 19% 0:27` over a 1-pixel bar;
  without the gadget (flat screen, `vr_hud_mode 0`) top right of the canvas (`SCR_DrawRelight`).
- **Reload**: the map in play goes first; reloaded when it is done (mid-batch: the save, the load, the other lights
  keep running) or at the end (`vr_relight_batch_reload 1`). Cancel does not reload.

Tested headless: dm4 dm5 dm6 e1m7 (4 relit in 1 s, 3 at once then); again: 4 skipped; `-force`: relit; `vr_relight_ao 1`
and back: relit each time. e1 with bounce, one at a time: progress 10 % (0:02, ETA 0:23), 24 %, 39 %, 51 %, 68 %, 79 %,
88 %, 96 %, 99 %, done in 0:32, e1m1 reloaded where the player was after 5.3 s while the rest went on. Cancel at 0:29
(two at once): e1m1-3 kept, e1m4 and e1m5 no output, no work copies, their light.exe PIDs gone within a second; then e2
started and the game quit at 0:10: no output for e2m1/e2m2, no light.exe after. Page bar in both eyes (`vr_eyeshot 3`),
gadget screen (`vr_gadget_screen_dump`), flat (`vr_enabled 0`). `vr_menu_path_check maps/vrcalibration.map`: 0 missing.
Not tested: the Map Library set with packages installed (none in the qbase: "no maps installed from the Map Library").

- [ ] Relighting > Many Maps: Maps An Episode, Relight These Maps in E1M1: the bar fills, the time left counts down,
      E1M1 reloads after a few seconds while the rest go on; the wrist gadget shows `RELIGHT n/8 ...` under the level name.
- [ ] Cancel halfway: the maps done play relit (E1M2 from its start), the others as before.
- [ ] Relight These Maps again: everything skipped at once ("8 skipped"); change a slider: relit.
- [ ] Every Map with Maps at Once 2 while playing: no hitches beyond a frame at each map's start.
## Corner buttons: VR Settings and Relighting (2026-10-06)

Vittorio: two more shortcuts in the corner bar, "VR Settings" (the regular VR Settings page, not Advanced) and
"Relighting" (Graphics > Relighting). The column (headset) and the flat screen's icon row now have nine buttons: Back
to game, Search, Console, **VR Settings**, Advanced VR, Levels, Map Library, **Relighting**, Checklist (Developer only,
still last) (`Quake/vr/vr_menuui.cpp` `Tool`, `toolLabels`, `toolNames`, `useTool`, `drawToolIcon`;
`Quake/vr/vr_menu.cpp` `menu::jumpToSettings`, `menu::jumpToRelighting`).
- **VR Settings** opens the VR Settings page from any menu (a tick if it is already shown); Back from it goes to
  Options, as from Options > VR Settings. Its icon: a headset (a visor with two lenses and a nose notch, a strap).
- **Relighting** opens Graphics > Relighting from any menu; Menu Detail goes up to Advanced if it was lower (as the
  Advanced VR button does: the page is an Advanced one). Back walks up its place in the tree: Graphics, Advanced VR
  Options, VR Settings. Its icon: a sun.
- `vr_mock_laser` / `vr_mock_mouse` take `settings` and `relighting` too (TESTING.md).

**Tests.** Flat 1280x720: `vr_mock_mouse settings click` from the main menu opened page 0 (VR Settings);
`vr_mock_mouse relighting click` page 145 (Graphics - Relighting, back to 32), then Escape three times: Graphics,
Advanced VR Options, VR Settings. Keys: a stick click on Advanced VR Options, three rights (corner button 3), Enter:
VR Settings; on the main menu, seven rights to button 7, Enter: Relighting. Mock headset: the laser on `settings` and
`relighting` with a trigger click opened the same pages. The hover names show under the row's new icons.

## Corner column further left (2026-10-06)

Vittorio: "The shortcut bar in VR mode should be a bit more to the left, it almost overlaps the rest of the menu." The
headset's labelled column (and the spectator switch and preview under it, which share its right edge) ended at menu x
8: flush with the VR pages' row highlights (x 8) and 4 pixels from the Search and Map Library pages (x 12). Its right
edge is now x -8 (`ToolbarLayout::columnRight`): 16 menu pixels clear of the VR pages, 20 of the Map Library, 24 of
Quake's plaque. On a panel too narrow for that the column goes as far left as the panel lets it (its left edge 4 from
the panel's), its labels kept while it ends no nearer the menu than x 8 (`columnNearest`, where it was); narrower
still, only the icons in the corner, as before (`toolbarLayout`, `bannerLayout`, `placePreview`).
- Checked in the mock headset (eyes): VR Settings, Advanced VR Options, Map Library, Options, main menu at the shipped
  Menu Height 1.35 / Spacing 1.5 and at 2 / 2: a clear gap everywhere; the spectator switch and its preview (on) stay
  under the column, which ends well above them.
- With the two new buttons the column is 32 true pixels taller, and the VR pages' and Ironwail's lists start below it
  as before (`layout().listTop`, `VR_MenuBounds`): about 2 rows fewer on a VR page at the shipped settings. At Menu
  Height 1 with Spacing 1 (the smallest panel) the column is icons only and a VR page shows only a row or two (it
  showed three or four with seven buttons).

## Main menu: VR Settings and Advanced VR rows (2026-10-06)

Vittorio: "VR Settings" and "Advanced VR Settings" rows on the main menu, directly under Options, in the big font.
The main menu is now VR Calibration, Single Player, Multiplayer, Map Library, Options, **VR Settings**, **Advanced
VR**, Mods (when shown), Quit (`Quake/menu.c` `MAIN_VRSETTINGS`, `MAIN_VRADVANCED`, `m_main_labels`).
- **"Advanced VR", not "Advanced VR Settings" (decided here, conservative):** the big font's letters are Quake's
  menu size, and "Advanced VR Settings" is 341 pixels wide: from the rows' x 73 it would end at 414, past the flat
  screen's menu canvas (it ends at about 370) and far right of the menu. "Advanced VR" (198) ends at 271, and is the
  corner button's name for the same page. One word in `m_main_labels` if a longer label is wanted (the headset's panel
  has room for it; a flat screen does not).
- **The rows:** VR Settings opens the VR Settings page, Advanced VR the Advanced VR Options (as the corner button:
  Menu Detail raised to Advanced if lower); Back from the VR Settings goes back to the main menu (its cursor on the
  row) when they were opened from it, to Options otherwise as before (`VR_Menu_OpenFromMain`, `openedFromMainMenu`).
- **The letters:** "Advanced VR" needed A and v alone (the pictures have neither: V was made from Save's v before).
  v is Save's v without a's leg (the same piece as V, unstretched); A is "Join a Game"'s a, two rows taller as R is r
  (its top 1 up, its feet 1 down: a capital's height, rows 1 to 16) (`Misc/quakevr/make_bigfont.py`, the .inc written
  again: two glyphs added, the others unchanged; `vr_bigfont`: 32 of 32 letters cut from Quake 1.06's pak).
- **Height:** nine rows 20 apart end at about 212: inside the flat screen's canvas (320 tall at any menu scale) and
  the headset's panel at the shipped Menu Height 1.35. Where the canvas is shorter (the panel at Menu Height 1 with
  the Mods row) the rows close up to keep the last one inside it (`M_Main_Step`: 18 there, at least 16); the cursor
  and the mouse/laser rows follow (on so small a panel the spectator switch, in the corner, overlaps Quit's first
  letters). The picture fallback (`vr_menu_bigfont 0`, or a mod's own pictures) draws its rows
  one at a time at the same spacing, the new rows in the mods' row's letters.
- **Tests:** flat 1280x720 and 1024x768 and the mock headset: the rows drawn in Quake's lettering, VR Calibration to
  Options unchanged (pixel diff of the eye images: no difference above the new rows); a click on y 140 opened VR
  Settings and Escape returned to the main menu on "VR Settings"; y 160 opened Advanced VR Options (Escape: VR
  Settings, then the main menu on "Advanced VR"); VR Settings opened from Options still goes back to Options. The
  laser on y 140 picks VR Settings with `vr_menu_bigfont 0`; at Menu Height 1 / Spacing 1 the laser on y 189 picks
  Quit.
## Limb gore: every limb cut off or popped, as the head (2026-10-06)

The author's spec: every enemy limb behaves as the head does today (Decapitation, Head pops): cut off by blades, popped
by bullets, blunt blows and lightning, by the same chance rules; on the living only as the killing blow, on corpses and
ragdolls every limb on its own, down to the torso. QC `vr_limbs.qc` (beside `vr_decap.qc`, whose arming it shares),
the engine's `vr_box3d.cpp` ("Limb gore": the cuts), `vr_limbmodel.cpp` (the limb models).

### Data model

- **The limbs are the ragdoll rig's joints** (`vr_ragdoll.cpp` seed tables; 12 monsters: grunt, knight, ogre, enforcer,
  death knight, rottweiler, scrag, zombie, fiend, shambler, gremlin, mummy). A **cut joint** is any bone with a Ball or
  Hinge joint that is not the torso (`pelvis`, the root, and `chest`); cutting it takes the bone and everything on it
  (its subtree: the shoulder the whole arm, the elbow the forearm and hand, the knee the shin and foot, `tail2` the
  scrag's tail from there). Loose bones (the grunt's shotgun) are never limbs. The **head** (the bone `head` and the
  bones on it: the rottweiler's jaw) stays Decapitation's: its zones, its head gib (`h_*.mdl`), its brains.
- **Where a hit lands** (`ragdolllimb(e, p)`): the nearest of the model's vertices to the point, as drawn now (a living
  or dying monster: its frame's pose; a ragdoll: its bodies, skinned) gives the bone struck. The torso: no limb. The
  head's bones: the head (Decapitation decides). Otherwise the joint nearest the point of the bone's own (its pivot)
  and its children's: a hit on the upper arm near the elbow cuts at the elbow, near the shoulder the whole arm.
- **The limb's model**: made at load time from the player's own copy of the monster's .mdl, nothing of id's written
  anywhere (`progs/soldier.mdl#limb4`, Mod_LoadModel's derived files, as the wall torch's flame): the triangles whose
  three corners are on the cut bones, in the rig's rest pose (the .mdl's first frame), centred on its vertices'
  middle; the cut capped (each triangle that crossed the cut with two corners on the limb gives a cap triangle from
  those to the cap's middle) in blood: a strip of the skin's palette's blood reds appended under each skin. Flag
  EF_GIB (a blood trail). A server entity (multiplayer: precached late, as the training dummy's enemies).
- **On the ragdoll**: the cut bones' bodies are destroyed (Box3D takes their joints; the ragdoll collides without
  them), their vertices drawn at the joint (as the beheaded neck's), the stump painted with wounds and spurting a
  fountain as the neck does. `.vr_limbcut` (the cut bones, bits) is kept in saved games: a ragdoll made again is cut
  again (as `.vr_headless`).
- **The limb**: the cut piece flies off as a gib (grabbable, rigid, destroyable, sticking; from where the bones were,
  turned as the cut bone was, its velocity plus the blade's share as a head's) with a few small gibs and blood; a pop
  bursts it where it was (blood mist, gore burst, small gibs, no brains). Detached limbs are capped by `vr_limbs_max`
  (oldest first; Quake's big gibs had no cap of their own).
- **Living monsters**: only the killing blow, armed as a beheading is (`vr_decap_limb`: -1 the head, else the joint):
  no death animation, its ragdoll at once with `vr_limbs_body_speed` of the blow's knock. Zombies and mummies: a lost
  limb kills them for good (`vr_limbs_zombies`; their beheading stays `vr_decap_zombies`).
- **Chances**: the head's rules per weapon class (blunt hardness, the shotguns' ranges and pellets at that limb, a
  thrown thing's mass, lightning), times `vr_limbs_chance_scale` for limbs and `vr_decap_chance_scale` for heads;
  Quad Damage always (`vr_decap_pop_quad`).
- **Explosions** (`vr_limbs_blast`): 0 gibbing as before; 1 an explosion's kill pops the limbs within
  `vr_limbs_blast_radius` of it by chance (`vr_limbs_blast_chance`, falling off with distance), the body left a
  ragdoll instead of gibs when any popped. **Full gibbing** (`vr_gib_limbs`): 0 Quake's gibs only, 1 the monster's own
  limbs thrown besides them (default), 2 instead of the meat gibs.

### What was built

- **Limb models at load time** (decision 1: the runtime way; nothing derived from id art is committed).
  `vr_limbmodel.cpp` answers Mod_LoadModel's derived-file hook (`VR_DerivedModelFile`, as the wall torch's flame) for
  `<monster>.mdl#limb<bone>` and `#limb<bone>m<bones>` (a limb whose end was cut before: the forearm without its hand).
  It reads the player's own .mdl and the ragdoll rig (`vertBone`), keeps the triangles whose three corners are on the
  limb's bones in the rest pose (frame 0), centres them on `ragdoll::limbMiddle`, and caps the cut: each triangle that
  crossed it with two corners on the limb gives a cap triangle from those two to the cap's middle, its texels on an
  8-row strip of the palette's nearest blood reds appended under every skin (one frame, EF_GIB; a zombie's EF_ZOMGIB).
  The grunt's: 32-116 triangles a limb, caps of 6-10; the fiend's up to 238 (`vr_limb_models`). Built in well under a
  millisecond; late-precached the first time a limb flies (the dummy's way), so a server entity in multiplayer (a
  client builds the same model from its own files; a client without that monster's rig can't load it).
- **Ragdoll cuts** (`vr_box3d.cpp`, `cutLimb`: Decapitation's `cutHead` generalized): the joint's subtree loses its
  bodies (Box3D takes their joints: the ragdoll collides without them), every cut bone's `body` entry names the part
  it hangs from now (a hand cut before its forearm is remapped too), its vertices are drawn at the joint as the neck's.
  `ragdollcutlimb`, `ragdolllimb` (the joint a hit cuts: the nearest vertex's bone as drawn, then the nearest of its
  pivot and its children's), `ragdolllimbs`, `limbmodel`, `limbplace`; `ragdollcut(e, 4|5, bone)` a stump.
  `.vr_limbcut` holds the cut bones: a saved game's ragdoll is cut again (read before the head's re-cut, which
  rewrites it).
- **QC** (`vr_limbs.qc`): the head's hooks (`VR_Decap_Blow`, `_Saw`, `_Thrown`, `_Pellet`, `_BlastArm`, `_BoltArm`,
  `_ThrownArm`, `_QuadArm`, `_CorpseBlow`) are now wrappers: the head's (renamed `VR_Decap_Head*`) first, then the limb
  struck, armed with the same globals (`vr_decap_limb`: -1 the head). `VR_Decap_Roll` multiplies by
  `vr_decap_chance_scale` or `vr_limbs_chance_scale` (Quad Damage still always). A blast's pellets are summed per limb
  (`.vr_limb_bdmg[16]`): the limb with most of the blast, if it holds Blast's Head Share of it. A thrown limb is a gib
  (`vr_limb`: grabbable, rigid, destroyable, sticking, carrying its monster's small gib counts), capped by
  `vr_limbs_max` (Quake's big gibs had no cap to share). The fountain (`VR_Decap_FountainThink`) takes a stump's joint.
- **Zombies and mummies** (decision 5): both have rigs (their ragdoll was only made when beheaded); a limb lost kills
  them for good (a zombie whatever the damage, as beheaded: blunt and shots need a knock-down blow, 25), switchable
  apart from their beheading (`vr_limbs_zombies`; `vr_decap_zombies` stays the zombie's head).
- **Explosions** (decision 3): `vr_limbs_blast 1` (default): a blast's kill pops the top limbs (whole arms, legs,
  tails) within `vr_limbs_blast_radius` (96) by chance (`vr_limbs_blast_chance` 0.6 at the blast, falling to 0; times
  Limb Chance; Quad always); any popped, it dies whole as a ragdoll; none, it gibs as before; corpses in a blast lose
  limbs the same way. `vr_gib_limbs 1` (default): a body gibbed throws its own top limbs besides Quake's meat gibs
  (2: instead of them).
- **Held weapons** (decision 4): unchanged: the weapon arm's loss is a killing blow, and the death code drops the gun.
- **Training dummy** (decision 7): it is armed as its enemy, so with Dummy Dies (`vr_dummy_gib`) it loses the limb as
  that enemy (the grunt's forearm, the death knight's).
- **Menu**: Gore > Limb Gore (the toggle, Limb Chance, Head Chance, Corpses, Zombies and Mummies, Body's Speed After,
  Most Limbs Lying About, Explosions Pop Limbs, Explosion Reach and Chance, Gibbed Bodies Throw Limbs); Head Chance on
  Gore > Decapitation too; Debug > Gore Tests > Limb Gore Tests (Spawn Its Limbs and the tests).

### Tests (`bash Misc/quakevr/limbs/limbs_test.sh <agent> [cases]`; firing range, mock headset)

- **Limb models** (`models`): every rigged monster's limbs build (grunt 9, fiend 14, rottweiler 11 with the jaw, scrag
  9 with its tail pieces, ...). Eyeshots: the grunt's limbs hung before you, the same in both eyes; a corpse cut
  apart lies as a bloody torso, its limbs round it with their blood caps.
- **Corpses down to the torso** (`corpse`): the grunt's ends first: 11 parts, then 10, 9, ... 3 after its eight
  joints (masks `#limb9m512` and the like for an upper limb whose end went first), 2 (pelvis and chest) after the head;
  whole limbs first: 11, 9, 7, 5, 3, 2. Every cut a limb gib and a fountain at its stump.
- **Living killing blows** (`live`, every rigged monster: grunt, ogre, zombie, shambler, scrag, knight, death knight,
  rottweiler, enforcer, fiend, gremlin, mummy): a sword's slash at a forearm cuts it (12 of 12: a ragdoll at once,
  health -1, the limb flying, 2-3 small gibs); a fist's pops it (11 of 12; the zombie's arm takes the melee's limb
  multiplier, 15 damage, under its knock-down 25: it gibs as before); a shotgun blast pops it (8 of 12) and a bolt
  (6 of 12) where the shot reaches the arm: the misses are the zombie's knock-down rule (under 25 at a limb)
  and the view's angle (pellets on the torso, the share under
  Blast's Head Share; the bolt striking the torso or the head from that side), not the rules. An explosion beside it pops the limbs near it
  (the body a ragdoll, not gibbed); gibbed, it throws its 3-5 own limbs.
- **Chances** (`chance`, 2000 rolls at 0.5): head 0.493, limb 0.512 at scale 1; 0.250 and 0.248 at 0.5; head 1.000 at
  2; limb 0.000 at 0.
- **Cap** (`cap`): 12 thrown with Most Limbs 6: 6 lie about. **Grab** (`grab`): the off hand on a cut forearm, the
  grip: held, lifted 20 units with the hand. **Save** (`save`): the corpse missing head, arms and legs saved and
  loaded: made again with 2 parts, each cut "made so again", the thrown limbs still there. **Zombie** (`zombie`): a
  10-damage slash at its forearm at full health: cut, dead for good (a ragdoll); `vr_limbs_zombies 0`: untouched.
  **Blast** (`blast`): `vr_limbs_blast 0` gibs (four limbs thrown with the gibs), 1 pops its four limbs, a ragdoll.
- **Dummy**: as a grunt and as a death knight with Dummy Dies, a killing slash at its forearm cuts it off.
- **Performance** (`perf`): 8 dismembered ragdolls and 64 limbs (128 thrown, capped): the physics step 0.127 ms
  mean (p99 0.40, 319 bodies) while they settle, 0.038 ms at rest. A limb model builds in well under a millisecond.
- e1m1 smoke: no errors. QC 0 warnings, statics check clean. eval.sh not run (the melee takes are missing).

### For the author to try in VR

Slash a grunt's forearm and an ogre's leg as the killing blow; punch and shoot limbs (by chance); cut a corpse apart
down to the torso and pick the limbs up; a rocket into a group (Explosions Pop Limbs); gib one (its limbs fly with the
gibs). Limb Chance and Head Chance on Gore > Limb Gore. Open: the explosion reach (96) takes a grunt's whole body from
a blast at its arm; the stump of the body is the limb's skin drawn into the joint (as the neck's), painted with
wounds; the cap of the limb is a flat blood colour.

## Ragdolls 6: the vore and the centroid; the mission packs' variants (2026-10-06)

Asked for: ragdolls for the vore, Hipnotic's scorpion and spider, Rogue's centroid; the Rogue variants checked.

### Which monsters exist (step 1)

The installed packs (`qvr-kit/qbase`: id1 pak0/pak1, hipnotic pak0, rogue pak0) were listed model by model. There is
**no spider** in either mission pack, and **the centroid is Hipnotic's scorpion**: Scourge of Armagon's
`monster_scourge`, `progs/scor.mdl` (235 vertices, 41 frames; its head gib `h_scourg.mdl`). Rogue has no centroid (its
new monsters: mummy, eel, wrath and overlord, guardian (`morph_*.mdl`, 221 vertices), dragon, lava man, phantom
swordsman (`sword.mdl`)). The vore is Quake VR's own `progs/shalrath.mdl` (371 vertices, 36 frames; id's is 208). So
two new rigs: the vore and the centroid.

### The rigs

- **Vore** (`shalrathSeeds`, 14 bones): pelvis, chest, head, jaw (his two tendrils: meshes of their own, on the head,
  so a beheading takes them), upper arms and forearms (elbow hinges folding forward), and three legs of thigh (ball)
  and shin (knee hinge from his bent rest). The back thigh is its knee's knob (its long triangles have no vertices of
  their own): its pivot set at his back by hand, as the jaw's. Clusters 1.20 units rms, bones 1.27 (rig.py 1.28). Death frames 16-22.
  Mass 160 kg (`vr_ragdoll_vore_*`). His head zone (`PositionalHead`) moved from 34 up, radius 9, to 45 up, radius 8:
  Quake VR's model is taller than id's (his head is 40-53 above his origin); the old zone missed the top of his head and
  took his shoulders (the sweep: every contact from 12 below the head's middle to its top is on the zone).
- **Centroid** (`scorSeeds`, 15 bones): pelvis, head (the front of the body, below), the tail in three, each arm (gun
  pod) one bone and its pincers its claw, each of six legs one bone (16 at most). Its legs, arms and pincers are meshes
  of their own whose motions are so alike that the motion clusters mix them (one cluster over two legs): new
  `SeedTable::wholePieces` (each piece but the body one bone's, the seed nearest its middle; the clusters only the
  body's) and rig.py's `"wholepieces"` / `"pN"`. Clusters 0.99 (k 26), bones 1.42. Death frames 36-40. Mass 180 kg
  (`vr_ragdoll_centroid_*`). Capsules on the tail and the claws: a part's mass is its share of the volume, and the flat
  pincers alone weighed 0.3 kg against an arm's 47 (a tail bone 0.09 kg). Head zone 11 forward, 9 below, radius 8.
- Both: Ragdoll Settings > Vore / Centroid (pages at the end of `pages`), Debug > Tests: A Vore's / A Centroid's Ragdoll
  There, decapitation (`VR_Decap_HeadModel`: `h_shal.mdl`, `h_scourg.mdl`), head pops and limb gore come from the rig.
  `decap_test.sh monsters` and `limbs_test.sh` (models, live) include them (Things 10 and 13).

### Bugs found on the way

- **The scourge freed the world.** `scourge_die` removed `self.lastvictim` (its trigger, made at its first think); one
  killed before its first think (a test's corpse; one killed as it spawns) had none, and `remove(world)` freed the
  world entity: every monster and gib after it fell through the floor (the vore made after a dead centroid at -650,
  small gibs at -70). Guarded (`hip_mon_scrge.qc`).
- **A limb with no triangles of its own** (the centroid's first tail bone with the rest of the tail cut off) was
  "available" (it has vertices), its model failed to build and its precache was a Host_Error. `Rig::triBones` (the
  bones each triangle's corners are on) and `limbmodel::available` now ask for a triangle wholly on the limb.

### The variants (step 3)

`vr_test_spawn_flags` (new, a test cvar: the test monster's spawnflags; 2 on a knight or hell knight names it and wakes
it as its map's trigger would). Each killed ahead in the firing range (`scratch` script, `vr_ragdoll_list`):

| Variant | Base model | Ragdoll before (by the code: their rigs untouched) / after (killed) |
|---|---|---|
| Rogue statue knight (`monster_knight`, spawnflags 2) | knight.mdl (skin 1) | yes / yes |
| Rogue statue hell knight (`monster_hell_knight`, 2) | hknight.mdl | yes / yes |
| Rogue multi-grenade ogre (`monster_ogre`, 2) | ogre.mdl | yes / yes |
| Honey's marksman ogre (`monster_ogre_marksman`) | mogre.mdl not shipped: id's ogre, `monster_ogre` | yes / yes |
| Rogue mummy | its own rig (Ragdolls 5) | beheaded only / same |
| Hipnotic gremlin | its own rig | yes / yes |
| Vore, centroid | new rigs | no / yes |

Every rig is matched by its model, so a variant on a rigged model ragdolls whatever its classname (one without its own
Ragdoll Settings row takes the global ones; the multi-grenade ogre is `monster_ogre`). A statue's ragdoll is stone
painted (no decapitation, no limbs: `VR_Decap_HeadModel`, `VR_Limb_Allowed`). No QC death path needed a fix. Rogue's
phantom swordsman (`sword.mdl`), guardian, wrath, eel, dragon and lava man have models of their own (no rig). Hipnotic's
`monster_spikemine` is a stub that shows `demon.mdl` and never dies (not touched).

### Tests

- Each killed ahead: the vore 14 parts, the centroid 15, both asleep within 9 s, nothing below the floor (repeated runs
  each). Gone limp at frame 18 (33% of his death) and 38 (50%: the centroid is solid until its third death frame).
  First drawn against the animated mesh: 3.8 and 4.0 units rms (the ogre 3.4, the grunt 1.9); in slow motion (`slowmo`,
  EYES) 1.5 and 2.3, the frame-to-frame crops with no jump at the switch.
- `decap_test.sh monsters`: both beheaded by a slash, their heads thrown, headless ragdolls (12 and 14 parts left);
  `pop` (MON 10 and 13): shotgun, super shotgun, overkill and lightning headshots pop the head, body shots and a
  headshot that doesn't kill don't.
- `limbs_test.sh models`: 12 and 14 limb models; `live` (KINDS 10 13): slash, fist, shotgun, explosion, gib all cut or
  pop (the centroid's bolt test misses: the tail point it aims at is outside its box); `corpse` (MON 10, 13): cut
  apart down to the torso (2 parts; the centroid's 1), every limb lying on the floor (`vr_limb_test 17`, new: the limbs
  lying about, Debug > Gore Tests > The Limbs Lying About).
- Release build, QC 0 warnings, statics clean (the precedence check's `vr_packutil.qc:50` is not ours); e1m1 smoke
  clean. eval.sh not run (it stops on a take missing from the checkout).

### For the author to try in VR

Kill a vore and a centroid (Hipnotic) and watch them go limp; behead both with a slash; cut a centroid's legs and gun
pods off and pick them up; the Ragdoll Settings rows. Open: the centroid's arms (its gun pods) weigh more than its body
(their hulls' volume); its masses (vore 160, centroid 180) are guesses.
## ericw-tools downloaded in the game (2026-10-06)

Graphics > Relighting's Tool section, when no `light.exe` is found, offers **Download ericw-tools (27.5 MB)**
(`vr_relight_get_tool [force | cancel | status]`; `Quake/vr/vr_relight_tool.cpp`). One pinned file: ericw-tools
2.0.0-alpha11's `ericw-tools-2.0.0-alpha11-win64.zip`, 27,503,991 bytes, SHA-256 `4e5ea11b...0745f` (the author's
local copy and GitHub's agree). Windows only.

- **Job**: a thread of its own (as the Map Library's install): `Download` (libcurl) into memory, its `abort` the cancel
  flag; size and sha256 (`vr_sha256`) checked before anything is written; miniz unpacks only `light.exe`,
  `embree4.dll`, `tbb12.dll`, `tbbmalloc.dll`, `gpl_v3.txt`, `LICENSE-embree.txt`, `README.md` (the package's set, by
  exact name at the zip's root: no entry path can leave the folder) plus a `NOTICE.txt` it writes (source offer, SHA-256)
  into `<dir>.download/`, then moves them file by file into `<dir>`, `light.exe` last (a folder rename fails:
  `Sys_ReplaceFile`'s `MOVEFILE_REPLACE_EXISTING` refuses folders), and removes `.download/`. A failure, a cancel
  (`vr_relight_cancel`, the page's Cancel Download) or a quit (`VR_StopDownloads` and `relight::shutdown`: cancelled,
  3 s, then let go as `mapinstall::finish` does) keeps nothing.
- **Where**: `<user base dir>/quakevr/tools/ericw-tools/` (the package's place; findTool's first look).
  `vr_relight_tool_dir` (testing, not archived) moves it and makes it the only place looked in after
  `vr_relight_tool`; `vr_relight_tool_url` (testing) downloads from elsewhere, still checked against the pin.
- **The author's path** (`C:/OHWorkspace/ericw-tools-2.0.0-alpha11-win64/light.exe`) is now looked in only at Menu
  Detail: Developer (`vr_menu_level` 2), his setting, so his builds keep finding it and players never rely on it.
- **Page**: rebuilt when `relight::toolPageState()` changes (found, downloading, a result): a bar ("4.0 MB of 27.5 MB",
  Checking, Unpacking), "Downloading ericw-tools 2.0.0-alpha11...", Cancel Download; after, a line ("ericw-tools
  2.0.0-alpha11 installed: Relight This Map is ready.", "Download cancelled: nothing kept.", "Download failed: the
  file is not ericw-tools' release (corrupted?)."). The Tool line's lookup (every 2 s) is redone at once when the
  download ends or `vr_relight_tool`, `vr_relight_tool_dir` or Developer change. Developer rows: Tool Lookup in the
  Console; Debug > Reports: Relighting: Tool Lookup.

Tested headless (`vr_relight_tool_dir` an empty scratch folder): from GitHub (twice), downloaded, checked, unpacked
(8 files), then `vr_relight` of e1m7 with it (0.4 s), and every map (79, id1 + hipnotic + rogue + quakevr) relit in
1:52 at the defaults, 2 at once, 32 cores. A local server: `bad.zip` (one byte flipped) refused ("not ericw-tools
2.0.0-alpha11's release ... not unpacked"), no folder made; `slow.zip` cancelled at 1.4 MB and quit at 1.5 MB: no
folder, no `.download/`, the game gone in a second; `good.zip` installed and dm4 relit with it. The page through
downloading, cancelled and installed (screenshots); `menu_vr dump`: the Download row only while none is found.
Menu Detail Standard: not found; Developer: the author's path.

## Lightning on you: Quad's arcs over the struck player (vr_shock_self_*, 2026-10-06)

Asked (e1m5): when the player is shocked, by a shambler for example, show the lightning shock's arcs on him: on the IK
body, the arms, the hands.

- **When:** every lightning bolt into a living player sends a new builtin `playershock(player, damage, at)` (QC
  weapons.qc `VR_LightningHit`, the funnel of `LightningDamage`: a shambler's bolt, a gremlin's, another player's
  lightning gun, Hipnotic's and Rogue's lightning traps), and so does the lightning gun's shock spreading through water
  (`VR_LGWater_Spread`) into another player in it (not one in a wetsuit). The shooter's own discharge under water keeps
  its older effect (kind 0: the flash and arcs, `vr_lg_water_flash`). It is QVR_SVC_SHOCK kind 7 (`KindSelfHit`), sent
  to that player's client only (as kind 0); the damage rides in the radius field. Nothing in the game changes.
- **How long** (client-side, so each player picks): `vr_shock_self_time` (0.8 s) for a shambler's bolt (10 damage), x the
  square root of damage / 10 (0.5..3x: a lightning gun's 30 lasts 1.39 s); each bolt pushes the end on. Strength
  (`power`, 0.5..1.5 by the damage, the largest of the shock's bolts) scales the number of arcs, which thin out to a
  third as it wears off and fade over its last quarter.
- **What** (vr_shock.cpp `drawStruck`, its own functions, its own random numbers: the other effects' as they were):
  short arcs springing off the skin along each limb (`limbArcs`, 4..10 cm, five segments, Quad's colours and widths):
  hands (wrist to fingers) and forearms (elbow to wrist) drawn over all as Quad's are, plus now and then a longer one
  from the fingers to the elbow; on the drawn body (`avatar::skeleton`) the upper arms, the torso (an ellipse round the
  spine, shoulders to hips) and the legs (hip-knee-ankle, when the legs are posed), depth-tested so the body and the
  weapon hide the far side. Without a posed body: hands and forearms (as Quad's guess) and a few round where the chest
  would be. A soft flickering blue dlight round the chest (`vr_shock_self_light`, 0.6; radius 60 + 90 x power units x
  the fade, 0.75..1 flicker).
- **Comfort:** no colour flash over the view and nothing in front of the eyes. An arc within 12 cm of the eyes isn't
  drawn; within 12..25 degrees of the view's middle it dims, fully out only near the face (closer than 25..40 cm); a hand
  looked at at arm's length keeps 60 %.
- **Both eyes and the spectator:** drawn once a frame into the line batches (as Quad's), so in both eyes, the mirror and
  the spectator camera.
- **Burns:** the existing wounds system already paints a lightning burn (`QVR_WOUND_ZAP`) on the player struck by a
  shambler from afar (vr_wounds.qc); no smoke on the player (vr_smoulder is for monsters and bodies).
- Cvars: `vr_shock_self_time` 0.8 (0 off), `vr_shock_self_arcs` 1 (x), `vr_shock_self_light` 0.6. Menu: Gore > Lightning
  Shock > Arcs on You / Arcs on You: Number / Arcs on You: Light. Debug > Gore Tests > Lightning Strikes You
  (`vr_shock_self_test [damage]`, no damage; `vr_shock_self_info`: last frame's arcs by part, the bolts, what is left).

Measured (headless, e1m1, `god`, a shambler spawned 260 units ahead with `vr_test_spawn 3; impulse 241`,
`vr_shock_self_info` every 5 frames): its attack's three bolts (10 each) reached the client (`bolts=3 damage=10
power=0.75`); 19 arcs a frame at first (hands 4, forearms 7, torso 3, legs 5), 13-17 through the shock, 7-10 in its last
0.2 s, gone at 1.1 s; `kept=0` (none near the eyes, head pitched 0). `vr_shock_self_test 30`: 1.39 s, 25-31 arcs a frame
(hands 3-5, forearms 7-10, upper arms 1-4, torso 5-6, legs 5-9). Eye images (`vr_eyeshot 3`: the lines are drawn after
`vr_eyeshot 1` takes its image, so it misses them, Quad's too) show them on both hands and forearms in both eyes, and
looking down on the upper arms and the torso; the spectator camera the same. Quad's arcs (impulse 255) unchanged.

### For the author to try in VR

Let a shambler hit you (god): are the arcs on the hands, arms, torso and legs enough, too many, long enough (Arcs on
You 0.8 s for its bolt)? Is the blue light pleasant or too much (Arcs on You: Light)? Any arcs bothering the eyes when
holding a weapon up to aim?

## Benchmarks: gore and map loads (2026-10-06)

Voice notes e1m5 16:46 and plaw01 17:05: benchmark scenarios for dismemberment and limb popping, and a map loading
scenario set (prepared and validated only: no timing runs yet). BENCHMARKS.md, "Gore" and "Map loads", has the detail.

- **Gore** (`gore` group, 10 scenarios): crowds cut, dismembered, blasted, gibbed (`vr_gib_limbs` 0/1/2) and head-popped
  in one frame, 64 limbs lying about, the cap's churn, the first cut of each of 14 kinds against the second.
  `vr_gore_test_crowd <units>` (QC: `VR_Gore_TestCrowd`; Debug > Gore Tests > Crowd Gore Tests, with Spawn a Crowd)
  runs `vr_limb_test` / `vr_decap_test` on every monster that near at once, quietly, one `goretest: crowd` line.
- **Window parts**: `vr_bench_mark <label>` splits a bench window; the JSON's `marks` give each part's worst and mean
  frame (the blow's frame apart from the falling).
- **Map loads** (`loading` group): `load_e1m1` (cold, warm, restart), `load_e1m1_qrp`, `load_e4m7`, `load_hip1m1` (the
  campaign switch and back), `load_changelevel` (replaces `mapload_e1m2`), `load_warden`, `load_ad_grendel`. The
  engine's load timing (`vr_startup_times`, now Debug > Profiling and Memory > Load Times) starts at the command (`map`,
  `changelevel`, `restart`, `load`: `VR_TimeLoadCommand`, with the map package, campaign and shutdown stages), splits
  the frames before the signon from the first frame drawn, times the ragdoll rigs' warm-up, and hands each load that
  ends in a bench window to the JSON's `loads` (stages, work, the second after). `qvrbench.py loads <dir>` gathers
  them into phases.
- First validation's leads for the load work (not benchmarks): warden waits 1.5-2.9 s for its hulls' build, warm or
  cold; QRP's warm load re-decodes the world's textures (BSP stage 1.7 s, slower than cold); a campaign switch reloads
  every alias model; `vr_gib_limbs 2`'s gib frame is the costliest gore blow (144 ms for 24).
## Your shadow has your head (2026-10-06)

The author (playtest, plaw01 17:07): the shadow of the IK body had no head. The body's neck and head are collapsed in
its skinning (vr_avatar.cpp solveTorso: the eyes are inside them), and the shadow maps drew the same skin. Now the
pose keeps a second skin with the neck and head at full size (`Posed::shadowSkin`), and the shadow maps' alias depth
pass (r_alias.c `R_DrawAliasModelsDepth`, `aliasdepth`) takes it through `VR_AliasShadowBonePoses`. The eye views and
the spectator camera (drawn from the head, so the head stays out of it too) keep the collapsed skin: nothing changes
in them. A light within 30 cm of the eyes leaves the head out (`avatar::shadowLight`, set per light in
`renderLight`): the head-mounted flashlight (9 cm out at the temple) and a hand-held light at the face would
otherwise shade their own beam. `vr_shadow_head` (1; 0 the old headless shadow) turns it off.

Checked: e1m1, facing the start's back wall, `vr_light_test 400 60 -36` behind the head: both eyes show the shoulders'
shadow with a head and neck over them with 1, none with 0; the eyes' views have nothing of the head. The head-mounted
flashlight (`vr_flashlight_clip_head right; vr_flashlight_toggle`): its spot the same with 0 and 1.

## Washing without the triangles' lines (2026-10-06)

The author (vrfiringrange 17:27, screenshot): a partly washed hand showed its triangles as pale lines. The masks are
painted by drawing the model into its skin's layout, its triangles and then their edges as lines (R_PaintAliasWounds:
the texels an edge crosses without covering their middle, read at the islands' edges). Painting takes the most of
what is there and what is painted, so texels covered two or three times don't mind; a wash subtracts, and took two or
three times its amount off the texels on the edges. `washUnder` now attaches a depth-stencil texture (as big as the
largest mask, made once) and passes each texel's first fragment only (stencil EQUAL 0, INCR), cleared for each layer:
each texel is washed once. `vr_gore_wash_once` (1; 0 the old way, for A/B). `vr_gore_wash_test [amount]` (Debug >
Tests > Wash a Quarter Off) washes that much off your hands and body at once, without water.

Checked: vrfiringrange, the main hand close up (`vr_mock_look 35 0; vr_mock_hand main 0.0 1.42 -0.3 -80 0 90`),
`vr_gore_hands_test main 1` twice, then `vr_gore_wash_test 0.25` four times, an eyeshot after each: with 0 the
triangles' edges show at 25% and 50%, with 1 none at any level (the blood's own speckle at 75% in both).

## Relighting: Light Textures 1.2 by default (2026-10-06)

The author relit hip1m1 with Light Textures 1.2 ("Maybe those should be the new defaults"): `vr_relight_strength`
defaults to 1.2 (config 93 moves a config's 1 to 1.2; another value stays), and `relight_maps.py
--light-texture-strength` too (the two give the same lights at their defaults). INSTALLER.md had the VisPatch data in
`<QVR>\tools\vispatch\`, a folder nothing reads: the scripts are in `<QVR>\quakevr\tools\`, and relight_maps.py takes
its VisPatch folder only from `--vis-dir` or `QUAKEVR_VISPATCH`. The data now goes to `<QVR>\quakevr\tools\vispatch\`,
which relight_maps.py takes by itself when neither is given (the in-game relight keeps the water-vis of the script's
copies in `relit\`). Checked: `vr_migrate_config` from 92 with 1 gives 1.2, with 1.5 keeps 1.5, from 93 keeps 1.

## Weapon pickups drawn as the weapons you hold and drop (2026-10-06)

Asked (e1m5): the map's spinning weapons (id's g_*.mdl) did not match the weapons dropped and held (Quake VR's v_*.mdl
props). `vr_pickup_prop_models` (default 1, archived, next map; Settings > Items, "Weapon Pickups Look": "As held and
dropped" / "Classic models") draws every weapon pickup (`impl_weapon_item`: the seven of id1, Hipnotic's Mjolnir, laser
cannon and proximity gun) with `WeaponIdToModel(wid, 0)`, the model a dropped one has, at its size (the shipped
`vr_pickup_scale 0.6` of quakevr.cfg shrinks only the classic models now: it was the whole size mismatch), same skin,
level, turning about its middle. The classic model when the setting is 0 or a weapon has no model of its own; both
models are precached at spawn, so a save made with either setting loads. Rogue's teamplay weapon drop
(`rogue_teamplay.qc`, its own g_ models, IT_ flags) is untouched.

- Spinning: v_ models have no EF_ROTATE. A new field `.vr_pickup_spin` (vr_sys_fields.qc, QVR_FIELD) sends
  `U_QVR_SPIN` (bit 29, no data) while the entity is not a rigid body; the client spins it as EF_ROTATE models
  (`VR_ModelSpins`, cl_main.c's bobjrotate). Box3D fixtures turn their shape with it, and gore's "lies about" counts it.
  The objects' own server spin (forcegrabbable_item_think, `angles_y = 100 * time`) is unchanged.
- Centring: two builtins. `drawnbounds(e, max)` is the drawn box as drawn (a weapon model's own scale and offset, its
  model_scale and model_offset: held::modelBox); `modelbounds` is the raw box, nowhere near a v_ model's drawn one.
  `modeloffsetto(e, at)` is the model_offset that puts the drawn box's middle at `at` (an alias model's offset is in
  stored-vertex units before the weapon scale, so the shift per unit is measured; it is linear). Objects
  (vr_item_objects 1, the default): VR_PickupObj_Place centres it on its origin, its box the drawn box, so it hangs
  at vr_item_float_height by its middle, and a knocked-loose rigid body keeps the same shape. Touch mode
  (vr_item_objects 0): its middle on the origin's vertical, as high as the classic model's middle was drawn
  (times vr_pickup_scale), never under the box's bottom; its touch box stays `-8 -8 -4 .. 8 8 38`. Centred again at its
  first think2: while a map spawns the server is not active and weapons::modelTransform gives the raw model's size.
- Retro textures: v_ models are Weapons in the World already (vr_modelmetadata.cpp), as the dropped ones.

Tests (Debug > Tests > Weapon Pickups; `vr_pickup_test`, QC/vr_pickup_test.qc): 1 every weapon pickup in rows of five
ahead, each with the same weapon dropped beside it (drawn boxes printed: a pickup's and its dropped twin's spans are
equal, e.g. the rocket launcher 38.2 x 8.3 x 10.2 both; before: the g_ model at 0.6); 3 the next pair before you;
2 with `+grabmain`: the nearest pickup taken. e1m1's nailgun, objects and touch mode: "You got the Nailgun", magazine
24, 6 nails left (7b4ed30b's loaded magazine).

To check in VR: the pickups' height and spin in e1m1-e1m5 (objects and touch mode), a pickup knocked loose (it should
fall as the same shape), deathmatch respawn.

## Saved games' models: the firing-range corruption (2026-10-06)

Vittorio died in the firing range, was respawned (the autoload of the last autosave), and every model was wrong:
buttons drawn as head gibs, panels as the vore, torches and flames in odd places; hitting one Host_Errored.

**Cause.** A saved entity's `.modelindex` indexes the precache list of the game that was saved, and a load spawns the
map afresh, precaching in its own order, then puts the saved values back. Vanilla Quake gets away with it (the same
spawn functions precache the same models in the same order); here the list is not the same: the training dummy's spawn
precaches the enemy `vr_dummy_type` names (changed in the menu since: its models now come earlier, every later model
moves), and models are precached late (the dummy's new enemy, limbs cut off, test spawns, boxes), elsewhere or not at
all at load. `rebindLoadedModels` only fixed an index that named nothing or a model not precached; an index that named
*another* precached model was trusted (kept for the ring of shadows' eyes on the player's model), so after a shift every
entity kept its old number: 35 wrong in the firing range after a dummy change (`vr_model_check 1`: the spawn panels'
`func_wall` index = `progs/shalrath.mdl`, the buttons' = `progs/h_shal.mdl`). A `func_wall` with an alias model is a
`SOLID_BSP` without hulls: the hand's trace into it, `Host_Error: SOLID_BSP with a non bsp model (func_wall at 380 -460
17)`, his crash.

**Fix.** Every save now writes, after the light styles, comment lines Quake's parser skips (older builds load the save
as before): `// qvr_save <format> progs <crc> build <build>` and `// qvr_model <i> <name>` for the whole precache list
(`SaveData_Fill` copies it with the rest, the save thread writes it). The load (`VR_ReadSaveInfo` before the old game
ends; `rebindLoadedModels` after the edicts) turns every saved model index into today's index of the same name
(precached now if need be: the client connects afterwards and gets the whole list): each entity's `.modelindex` (the
eyes too), the globals `modelindex_*` (player, eyes, hammer: `CheckPowerups` sets the player's model from them every
frame) and the fields that keep one (`.vr_corpse_model`, `.burn_model`, `.vr_stick_model`: float fields named `*_model`).
A save without the lines (an older build's, his autosaves) is repaired by name: an entity's own `.model` wins over its
saved index; the globals and other fields go by the list its entities make (saved index -> the first such entity's
model). Not remapped: the decapitation/limb fountains' `.cnt` (a modelindex kept to stop when the body is gibbed: at
worst one stops early after a load across a shift).

**Versioning.** Another build's save (its `build` differs from `VR_BuildVersion`): a console warning naming both builds
and both progs CRCs, and a centre print once the player is in ("Saved game from another build"). An older build's
(none written): the same, "from an older build". A save of a newer `VR_SAVE_FORMAT` than this build reads: refused,
the old game kept (console error and a centre print). The format is 1; it goes up only when a save would load wrong in
an older build. The re-release's saves (version 6) say nothing.

**First-cut hitch.** A limb's model was made at its first cut (`progs/soldier.mdl#limb4`: 4-5 ms of the frame in
QuakeC, `quakec_max` 5.6 ms, busy 9.6 ms in the mock's 4 ms frames). `vr_limbs_prebuild 1` (Gore > Limb Gore > Make
Limbs as the Map Loads) makes the whole limbs of every kind of monster the map has as it spawns
(`VR_OnSpawnServerSpawned`, before serverinfo: in every client's list): e1m1 17 limbs of 2 kinds, 237 ms the first
time (normal maps made), 79 ms after (of a ~800 ms load); the firing range's first cut then has no late precache
(`quakec_max` 0.7 ms). A limb whose own end was cut first (`#limb3m8`), and monsters spawned later (test spawns, the
firing range's panels) are still made at their cut. `developer 1` prints each late precache with its time.

**Tests.** `Misc/quakevr/precache/precache_test.sh <agent> [dummy death restart legacy build hitch]`:
`vr_model_check` (Debug > Reports > Models Check) after each step must say 0 wrong for the entities and the client.
Before the fix: dummy 35/36 wrong, death (limb cut, dummy changed, autosave, killed, loaded) 36, legacy 35, and the
Host_Error by a hand put on a button; after: 0 everywhere, no Host_Error, his own 17:37 autosave loads with 0 wrong.
The death case loads the autosave itself: `restart`'s autoload (`Host_AutoLoad`) adds its `load` behind the rest of a
test script, so it fails there ("Autoload failed!"); in the game the buffer is empty and it is the same load.
## Menu help: a taller box, the canvas's width, long help in parts (2026-10-06)

Vittorio (e5m1): "Some of the menu item descriptions seem to be too long for the menu description at the bottom." The
help under a VR page's rows was four lines of 38 characters (152): longer help was cut off. Measured with the new
`menu_vr helpcheck [columns]` (every page built, each row's help wrapped as drawn: HELPPAGE, HELPLONG, HELPSUM lines):
3156 rows have help; at 38 columns 1112 (35%) needed more than four lines, the longest 16 (Gore > Limb Gore's own
row, 472 characters; Tips 317; Menu Detail, on every page, 285).
- The line is as wide as the menu canvas lets it (`helpColumns`: 38 to 50 characters; the mock headset's panel and the
  flat 420-wide canvas both give 50). At 50: 38 rows need more than seven lines, the longest 13.
- The box grows to the page's longest help, four to seven lines (`helpBoxLines`, worked out when the page is built,
  not every frame); the list is a row shorter for each line (every page has Menu Detail's 6-7 lines, so three rows
  fewer: 23 at the shipped Menu Height).
- Help longer than the box shows a part at a time, turning by itself: each part stays 2 s plus its words at
  `vr_menu_help_wpm` (200 words a minute; Menu Settings > Help Reading Speed, 100-400, 60-600 past the ends), then the
  next, then the first again; a thin bar at the box's right shows which part. `developer 1` prints
  `menu help: part 2 of 2 (10 lines, 20.6 s)` as they turn (Limb Gore: 20.6 s, then 10.4 s, measured in real time).
- Search's and the Map Library's own help keep the four-line box (their rows carry none).

## Checklist: Undo Last Tick (2026-10-06)

Vittorio (vrfiringrange): "Can you add an undo button to the checklist in case I accidentally tick an item and I cannot
find it anymore so I can undo the last tick?" Checklist > **Undo Last Tick** (under Hide Ticked): every tick and untick
made this session is kept (`checklist::undo*`, the last 64), and each press puts the last one back as it was, saved at
once; with Hide Ticked on, an item ticked by mistake comes back into the list. Its help says how many are left, what
the next press changes ("Next unticks: <item>") and what the last one undid; the row is dimmed with nothing to undo.
Hide Ticked off already shows the ticked items (dimmed), its help now says so. `vr_checklist undo` does the same from
the console (CLUNDO line).
- Tested with the mock keys (Hide Ticked on): two items ticked (65 open -> 63, the page 269 rows -> 257), Undo twice
  (-> 263 -> 269 rows, 65 open, both items unticked in the ticks file), a third press and `vr_checklist undo` with
  nothing left: CLUNDO -1.

## Search: opened recently, under the keyboard (2026-10-06)

Vittorio (vrfiringrange): "In the search page, can you add a list of recently clicked items at the bottom below the
keyboard? It should keep like the last five or six items I've searched and clicked on." Under Search's keyboard,
**Opened recently:** the last six results opened (a row or a page), the latest first, each with the page it is on in
small letters; one picked (laser, mouse, or the sticks: down from the keyboard's bottom row, A / Enter) opens it as a
result does, and moves it to the top. Kept across starts in `quakevr/search_recent.txt` (ignored by git; one a line:
`row|` or `page|`, the path, ` > `, the label), written at each open; one no longer found (renamed, removed) is left
out of the list shown. As many rows as fit above the help (all six at the shipped Menu Height; the Map Library reuses
the layout without the list). `menu_vr recent [clear]` prints them (SRECENT, with where each is drawn) or clears them.
- Mock headset: "grenade", "turn speed", "flashlight" each opened with Enter, Back to Search: the list Flashlight,
  Turn Speed, Grenade. Restarted: the same three read from the file; the laser on Grenade and the trigger opened Weapon
  Damage on Grenade, now first. Flat (`vr_enabled 0`): `vr_mock_mouse 134 123 click` on Turn Speed opened VR Settings
  on Turn Speed, now first.

## Map Library: Uninstall and Reinstall (2026-10-06)

Vittorio (vanisch01): "I cannot see an uninstall button in the map library. Is it there?" It was not: only the console's
`maps_uninstall`. An installed package selected on the Map Library page (and no download running) now shows
**Uninstall** and **Reinstall** side by side at the foot of its detail (the left column, above the filter bar; the
detail's text stops above them). Each needs a second press within 5 s (`mapsConfirmSeconds`): the first turns its
label into "Uninstall?" / "Reinstall?". Uninstall is `mapinstall::uninstall` (the package's folder in qvr_addons removed,
its files dropped from cache/maps_installed.txt, as maps_uninstall); Reinstall does that, then installs it again as
Install does. The sticks reach them down from the keyboard's bottom row (then down: the bar; right of Reinstall: the
list). `maps_page_stats` also prints where the first row, Uninstall and Reinstall are drawn (for tests).
- Tested with a package faked into the kit's base (Lost Place: a dummy file in its qvr_addons folder and its registry
  line; nothing downloaded): mock headset, laser on Uninstall, trigger: "Uninstall?"; trigger again: "maps: removed 1
  file(s)", the folder gone, the registry its header only, `maps_installed`: nothing installed, the buttons and
  "INSTALLED" gone from the detail. The same with the flat mouse (`vr_mock_mouse 54 211 click` twice). Reinstall was
  only armed, not confirmed (it downloads).

## Gore tweaks: enforcer lasers pop, cut ends spurt, limb weights, shorter living arcs, the beheading zone drawn (2026-10-06)

Five voice notes (hip1m1, vrfiringrange).

### Enforcer lasers pop heads and limbs

An enforcer's laser bolt that kills a monster with a head or limb hit pops it at `vr_decap_pop_laser` (0.85; times Head
or Limb Chance; 0 never). Both bolts that are an enforcer's laser: an enemy's (`Laser_Touch`: an enforcer, or any monster
using `LaunchLaser`, into another monster) and the player's own enforcer's rifle (`VR_RifleLaser_Touch`). Hipnotic's
laser cannon (`HIP_LaserTouch`) is another projectile and is unchanged. The point tested is where the bolt's line meets
the model (precise hits), as the guns'; head first (`VR_Decap_OnHead`), else the limb (`VR_Limb_At`). New kind
`QVR_DECAP_LASER` (11), a pop. Menu: Gore > Decapitation > Head Pop Chance > Enforcer Laser, and Gore > Limb Gore >
Enforcer Laser Pops (the same setting). Tests: `vr_limb_test 20` (head) / `21` (forearm): an enemy enforcer's bolt from
64 units off along a clear line, the target at health 1. Roll 0: both pop; `vr_decap_pop_laser 0`: neither (not armed);
random rolls at 0.85 (each a fresh grunt): heads 34 of 40 kills popped (two runs), limbs 11 of 15.

### The cut end's own fountain

A limb cut off (and a head) spurts a smaller fountain out of its cut end as it flies: `vr_limbs_end_fountain` s (1; the
stump's is `vr_decap_fountain` 2.5), 1-3 drops a tick (the stump's 2-6), pulsing and dying away, gone with the piece. A
limb's spout is its joint in the limb model's space (`limbpiece(e, bone, 1)`: the model is made from the rig); the
head's gib model has a space of its own, so its spout is the neck where it was cut, in the gib's axes (modelpoint
undone). `vr_limb_test 18`: a grunt's forearm, spout 0.00 units from the stump's joint as it is cut, 8.3 from the piece's
middle, carried along (the piece 129 -> 30 -> 52 u/s), 47 drops in 1 s, then gone; after a corpse cut apart, 8
fountains, every spout within 0.13 units of its joint at birth, the head's 0.00. Menu: Limb Gore > Cut End Fountain,
Decapitation > Flying Head's Fountain (the same setting).

### Limb weights

A limb (or head) cut off weighed its model's hull at the density of flesh, whatever its body weighed. Now it weighs its
share of its ragdoll's Mass by the kind of limb (`limbMassShare`, vr_box3d.cpp, from the rig's bone names): a person's
arm 6.3% (upper arm 3.5, forearm 2, hand 0.8), leg 15.5% (thigh 9.5, shin 4.5, foot 1.5), head 7 (a jaw 1 of it); two
arms' and two legs' shares split among as many as the rig has (the rottweiler's four legs 7.75% each, the vore's three
10.3, the centroid's six 5.2); a part the rig lacks weighs on its parent (the grunt's forearm carries the hand: 2.8%);
tails and the rest their hull's share of the body's volume. Times `vr_limbs_mass_scale` (1; 0: the old hull weight). Set
as `.vr_prop_mass` (Box3D), so held and thrown feel follows. A first try with the ragdoll parts' volume shares gave
erratic numbers (a zombie's arm 3.4% and leg 31%, a hell knight's arm 17%) and was dropped. `vr_limb_test 19` (every
monster kind on the map):

| monster (ragdoll kg) | head | whole arm | forearm(+hand) | whole leg | shin(+foot) |
|---|---|---|---|---|---|
| grunt (80) | 5.6 | 5.0 | 2.2 | 12.4 | 4.8 |
| ogre (200) | 14.0 | 12.6 | 5.6 (hand 1.6) | 31.0 | 12.0 |
| rottweiler (40) | 2.8 | (front leg) 3.1 | 1.2 | 3.1 | 1.2 |
| shambler (280) | 19.6 | 17.6 | 7.8 (hand 2.2) | 43.4 | 16.8 |
| scrag (40) | 2.8 | 2.5 | (hand) 0.3 | tail 3.4 (volume) | |
| fiend (140) | 9.8 | 8.8 | 3.9 | 21.7 | 8.4 (foot 2.1); tail 6.2 |
| gremlin (20) | 1.4 | 1.3 | 0.6 | 3.1 | 1.2 |

Whole limbs and head: 50.6% of every humanoid. The grab case (`limbs_test.sh <agent> grab`) still holds and lifts a cut
forearm (2.2 kg).

### Living monsters' arcs half as long

`vr_shock_living_time` (0.5): a living monster's lasting shock lasts that much of `vr_shock_death_time` after the last
bolt. Its death (any kill: `VR_Shock_Killed` from Killed) carries a shock still on to the whole duration from that bolt;
one ended already stays ended. `vr_shock_hit_test 5` on a grunt: 1.5 s left (3 x 0.5); killed 0.5 s later by a plain
blow (`vr_shock_hit_test -2`, new): 1.0 -> 2.5 s left; a corpse struck: 3. Menu: Gore > Lightning Shock > On the Living.

### Show Hit Zones' Decapitation view

The question: is the static magenta capsule what melee beheading uses? No. Every head and limb decision tests the point
struck on the model as animated: `VR_Decap_OnMeleeHead` (slashes, the chainsaw, corpse blows) and `VR_Decap_OnHead`
(head pops, thrown axes, the laser) map it to the standing pose with `hitmodel_rest` (the same triangle's place in the
stand frame) and test it against the standing zone there; limbs (`ragdolllimb`) use the bones posed by the current
frame. The capsule's numbers are right, only the debug view drew it where the monster's head is when standing, not
mapped onto the animated model. (Precise hits off: the static zone is what is used.) Now Decapitation paints the melee
zone (magenta) on the animated surface, as Positional Damage paints its regions (`capsuleZone`: the capsule clipped
exactly, ends as spheres, the middle a 64-sided prism), with the head sphere that shots and thrown axes use (red) over
it; Both outlines the magenta pieces over the positional regions; the wireframe stays for precise hits off.
`vr_hitzones_check` reports `decap_models` and `decap` pieces. `vr_limb_test 22` holds the nearest monster in frame 17
(a grunt lying dead), `23`: its head as animated is 54.5 units from where the standing zone sits; the animated head
point is in the melee zone and the head zone (1, 1) and maps to the head bone, the standing zone's middle is in
neither (0, 0) and maps to a leg (8). Running (frame 2, 6.3 apart): both in.

### For the author to try in VR

Hipnotic or the firing range: let an enforcer shoot another monster in the head (or shoot one with its rifle); cut limbs
off and watch the cut ends spurt as they fly, a head's neck too; pick a forearm, a leg and a head up and throw them
(Limb Weight); shock a living monster and kill it with the axe (its arcs go on); Debug > Show Hit Zones > Decapitation on
a running and a dying monster.
## Bullet time on a stick press (2026-10-06)

Voice note hip1m1 16:49: bullet time started by a thumbstick press instead of the gadget. `vr_bullettime_trigger`
(Combat > Bullet Time > **Trigger**): 0 the gadget (its wrist tap and its button, as before; default), 1 the left
stick's press, 2 the right stick's press (physical sides: HAND_OFF is always the left controller). With a stick chosen,
the gadget's button and the wrist tap do nothing (`bullettime::frame` returns before them), and that stick's press in
the game does only bullet time (`bullettime::stickPress`, called from `vr_input.cpp` after the posing mode and the
motion recorder's record button, before the game's key): its bound key never reaches the game, release included
(taken when its press was). Conflicts: by default LTHUMB is `+speed` (run) and RTHUMB `+reloadmain`; the chosen stick
loses that in the game (the menu's help says so); the other stick keeps its binding. In the menu the press stays the
menu's. The motion recorder's record button (`vr_motion_button` 0, the off stick click, only while armed) still comes
first.

Test (mock, e1m1, `vr_debug_buttons 1`, `vr_debug_bullettime 1`): mode 0, left press: `LTHUMB (bound to "+speed")` to
the game, no bullet time. Mode 1: right press to the game (`+reloadmain`); left press: `bullet time: left stick
press`, on at 0.30x, `taken by bullet time` (press and release). Mode 2: left to the game; right press toggles it off,
taken. The gadget's button could not be exercised headless here: `vr_mock_hand_to main button` says "no gadget shown"
(the gadget's pose is never valid in this mock run, with or without placed hands), so the gadget-off part rests on the
code path.

## Flashlight: turned over by a flick of the wrist (2026-10-06)

Voice note vrfiringrange 17:28 ("turn the torch's direction with a quick flick of the wrist upwards or downwards").
Read as the flashlight (the British torch): held in a hand away from a gun and the head, B/Y turns it over between the
low grip (beam out past the thumb) and the overhead one (out past the little finger); now a sharp flick of the wrist,
up or down, does the same (`flip`, its spin, click and buzz). Not the burning wall torch (nothing to turn there).

`flashlight::flicks()` (vr_main, once a frame after the input, the hands as last placed): each hand's forward and up on
the body (the play space's turn taken out), the turn since the last frame as axis times angle, in real time
(`realtime`: bullet time doesn't change it). A flick: the turn about the hand's right (pitch) at
`vr_flashlight_flick_speed` (600 deg/s) or more; that pitch at least 0.7 of the whole turn (a twist or a sideways wave
isn't one); the hand slower than `vr_flashlight_flick_max_move` (1.5 m/s: swings and punches never flick); one per
`vr_flashlight_flick_cooldown` (0.4 s) and the wrist must slow below half the speed to re-arm (the wrist coming back is
not a second flick); a turn over 120 degrees in one frame is a tracking jump. Only for the holding hand, in the game,
alive; not by a gun or the head (B/Y clips it on there). `vr_flashlight_flick` 1 (on). Menu: the flashlight's page,
**Turning It Over** (Flick to Turn Over, Flick Strength, Flick: Hand Still Below, Flick Cooldown); Debug > Logs >
**Flashlight Flicks** (`vr_flashlight_flick_debug` 1: each flick taken or refused and why; 2: the wrist every frame).

Test (`Misc/quakevr/flashflick/flick_test.py`, mock, e1m1, the off hand holding it, fixed frames): slow wrist up 60
degrees over 60 frames and back, and aiming around (pitch and yaw 1 degree a frame): nothing. Flick up (60 degrees in 6
frames: 644 deg/s): flipped; flick down: flipped; up and straight back: one flip; that twice 40 frames apart: two. Roll
70 in 6 frames, yaw 60 in 6: nothing. The flick with the hand moving 2 m/s: refused (the hand moving). Strength 1200:
the 644 deg/s flick nothing, 90 degrees in 4 frames (1299 deg/s) flipped. Off: nothing. (Mock scripts: `wait N` is one
frame; only run.sh's `-Script` expands `waitN`. `vr_flashlight_give` before the view has run once after a map load is
undone by the flashlight's state restore: the test gives after 150 frames.)

## Climbing: mantling onto sloping tops (2026-10-06)

Voice notes e5start 17:12 and plaw01 17:07 (mantling fails onto a ledge whose top is slightly sloped; gate it). The
ledge map already finds lips of sloping tops (up to `minTopNormal` 0.7) and measures their depth along the plane; the
mantle didn't stand on them: `findMantle`'s box goes to the lip's height (+1), so on a top rising away from the lip
the box sits in the slope at every spot (22 to 38 units in) and the sweep over hits it: "no room to mantle". DOPA's
e5start ledge by the hard teleporter (lip y 560, x 384..408, z 92) rises 4 units in its first 16 to a level top at 96.

Now (`vr_climb.cpp`): the old search is `findMantleLevel`, unchanged and tried first; when it finds nothing,
`findMantleSloped` (with `vr_climb_mantle_lenient` 1, the default): at the same spots and sides, the player's box is let
down onto the top from as high as a slope of `vr_climb_mantle_slope` (40 degrees) could rise under its far side (at
most 48) to as low as it could fall; it must land (not start in solid: a wall or ceiling there is no spot) on a plane
no steeper than that, with the old footing test (not just its edge), and the way must be clear: straight up to the
higher of the lip's height and the landing's (or up from 2 or 4 units further out), then over. A top falling away is
reached at the lip's height and dropped onto. Cost: about 100 box/line traces (0.035 ms) for a full failed search,
retried every 0.15 s as before. Menu: Climbing > **Mantle onto Slopes**, **Steepest Slope**. `vr_climb_debug 1` prints
`climb: sloped mantle spot ...` (how far in, the rise, the slope).

Test map `vrslopes` (`Misc/quakevr/climb/make_vrslopes_map.py --compile`; lips at y 0, z 48, as vrclimb's long ledge;
`setpos <x> -18 24 0 90 0`): level, rising 10/20/30/45 degrees, falling 20, sloping across 15, rising 20 under a slab.
`Misc/quakevr/climb/slopes_test.sh <agent> ["<cvars>"]` runs the mantle play on each (PASS/FAIL against the table below with no cvars) (`climb_plays.py`'s mantle with the hands
at 1.49 m: its default 1.638 no longer reaches vrclimb's lip with the current default body, "no hold").

| top | lenient (default) | `vr_climb_mantle_lenient 0` |
|---|---|---|
| level | mantled (level search), z 73.0 | mantled |
| rising 10 | mantled, sloped spot 22 in, +4.0 | no room |
| rising 20 | mantled, +9.2 | no room |
| rising 30 | mantled, +15.2 | no room |
| rising 45 | no room (steeper than 40) | no room |
| falling 20 | mantled (level search) | mantled |
| across 15 | mantled (level search) | mantled |
| rising 20 under a slab | no room | no room |

`vr_climb_mantle_slope 25`: rising 30 refused. DOPA e5start (`-game dopa`, from the Steam rerelease's dopa pak, in the
test base only), both hands on the lip from `setpos 396 542 71 0 90 0` (hands at 1.90 and 1.94 m): before, "no room to
mantle" (and still with lenient 0); now `sloped mantle spot 22 in, +0 along: the top +2.5` and mantled onto (392.4 582
120). plaw01 (Map Library) isn't in the test base: not run.

## Throws in bullet time: the release's windows in real time (2026-10-06)

Voice note start 16:55 ("throwing speed in bullet time seems a little bit off ... the wrist snapping action feels way
too strong"). In slow motion with the player slowed (bullet time without Sandevistan, `vr_slowmo`),
`timescale::filterHands` slows the runtime's clock (`t.time`) and gives the hands' velocities in the game's time (times
1/scale), which is consistent; but the throw's estimate (`vr_throw.cpp releasePeak`) used its windows in that slowed
clock's seconds: `vr_throw_window`, `_lookahead`, `_peak_span` (the spin over twice it), `_dir_lookback` and the
peak's fit took in 1/scale as much of the real arm's arc (3.3 times at 0.3x). The averaged spin came out about half,
the direction from much earlier in the arc (an overarm throw 16 degrees high, an underarm lob 18 low, uncapped), and
`vr_throw_ang_threshold` (6 rad/s) was met by a 1.8 rad/s real wrist turn, so the wrist's flick was added to throws
whose wrist barely moved. Sandevistan (the player's own time) was already right.

Fix (`vr_throw_slowmo_real_time` 1, default; Throwing page > **Slow Motion: Throws in Real Time**): the windows are
the real seconds times `timescale::handScale()` (the slowed clock's seconds in a real one; 1 in Sandevistan and at full
speed: nothing changes there), and the flick threshold and the analog release's `vr_throw_release_speed` compare real
speeds (times it). 0: as before.

Test (`Misc/quakevr/throw_slowmo/throw_slowmo_test.sh`, `throw_plays.py`'s throws, mock, fixed frames, Gun Angle 70,
`vr_mock_grip_velocity 1`; speed in m/s, elevation):

| | overhand (wrist flick) | lob | flat push | overhand, still wrist |
|---|---|---|---|---|
| full speed | 4.30, -0.7°, spin 34.0 | 4.81, +23.6°, 15.1 | 4.97, +5.0°, 0 | 6.29, +13.9°, 11.3 |
| 0.3x, hands unslowed (caps 0), before | 12.71, +16.2°, 59.7 | 15.47, +4.9°, 34.6 | 16.63, +5.0° | 20.66, +30.8°, 25.3 |
| 0.3x, hands unslowed (caps 0), now | 14.33 (=4.30/0.3), -0.6°, 113.2 (=34.0/0.3) | 16.05, +23.6°, 50.4 | 16.57, +5.0° | 20.96, +13.9°, 37.7 |
| 0.3x, default caps, before | 8.89, +31.9°, 18.8 | 8.23, -24.2°, 11.1 | 8.57, +5.0° | 9.43, backwards (-7.01 y) |
| 0.3x, default caps, now | 8.00, +21.9°, 20.0 | 8.00, +18.4°, 20.0 | 8.48, +5.0° | 7.97, +30.9°, 20.0 |
| Sandevistan, before and now | as full speed | | | |

With the hands unslowed the bullet-time throw is now exactly the full-speed throw in the slowed world (1/scale in game
units: it leaves the hand at the same real speed and direction). With the default caps (`vr_timescale_hand_speed` 8
m/s and `_spin` 20 rad/s of the game's time: the hands slowed with the world) every throw this fast is capped at 8 m/s
(2.4 m/s real at 0.3x) and its direction still follows the slowed hand lagging the controller (21 to 31 degrees up):
that is the slowed hands' design (no supersonic swings), not the estimate; worth the author's opinion (Sandevistan, or
a higher `vr_timescale_hand_speed`, gives full throws).
## MG1 Horde: source death gate, coop checks and wave tables (2026-10-06)

Horde deaths go through the source's release-then-press gate again (a held trigger at death no longer restarts the
arena; one restart per wipe), and a player who leaves no longer counts as alive. New Debug > Tests > Machine Horde
Tests rows: wave monitor, all-players report, team wipe, keyed door check. `Misc/quakevr/check_horde_waves.py`
checks wave budgets and squads against the official tables (5 arenas, all four skills, coop: 0 failures);
`Misc/quakevr/multiplayer/mghorde_mp_test.sh` covers coop revival, shared keys, team wipe and leaving. Details,
the Hunger/re-aggro/axe-chain source findings and which arena to use for each manual test: EXPANSIONS.md, "MG1
Horde follow-up".

## Throws in bullet time: the way the controller moved (2026-10-06)

Follow-up to the section above. The author: "Why is the angle higher? Shouldn't it be the same angle, if I perform the
same motion but slowly?" Two causes:

1. **A throw at real speed** (default caps, `vr_timescale_hand_speed` 8, `_spin` 20): the slowed hand
   (`timescale::filterHands`) can't keep up, and while it is limited its velocity is its catch-up step towards the
   controller, not the arm's motion: an overarm arc's catch-up points at where the controller already is (above and
   ahead), 21 to 31 degrees high. Fix (`vr_throw_slowmo_aim` 1, Throwing page > **Slow Motion: Throw Where You Aim**):
   `filterHands` keeps each controller's own pose with its velocities in the game's time (`timescale::controllerPose`);
   `vr_hands.cpp` samples it into a second history in step with the hand's (`throwing::sample(hand, time, handMotion,
   controller)`; its spin from its own orientation, `poseSpin(.., own)`). The release (`withOwnAim`, `vr_throw.cpp`)
   runs the same estimate on both: the throw keeps the slowed hand's speed (at most the controller's), spin size,
   release point and time, but takes the controller estimate's direction, spin axis and wrist flick (scaled with it).
   Both hands (`estimateBothAt`) likewise. When the samples are the same (full speed, Sandevistan, a hand that kept up)
   nothing changes (bit for bit). The release point stays the hand as drawn: the object leaves the slowed hand where
   the player sees it (up to 0.36-0.44 m behind the controller in these throws).
2. **A throw made slowly with the slowed world** (performed 1/0.3 slower): the windows in real seconds of the section
   above took in 0.3 of its arc (a few degrees lower). Fix (`vr_throw_slowmo_tempo` 1, **Slow Motion: Slow Throws
   Match**): the windows' clock follows the motion (`motionRate`): from the controller's fastest speed around the
   release in the game's time, within what the slowed hand follows (`vr_timescale_hand_speed`, 8 if 0) it moved with
   the world and the windows are the game's seconds (the same throw as at full speed); from 1.5 times that it moved
   faster than the world (real seconds, as before); a blend (log) between. 0: always real seconds.

`vr_debug_throw 1` adds in slow motion: `slow motion: windows x<rate> of the clock's, the controller's way (the slowed
hand's <deg> off it), the hand <m> behind it at the peak`.

Test: `Misc/quakevr/throw_slowmo/throw_slowmo_test.sh <agent> "<cvars>" [stretch] [keyrate]` (new: `stretch` 3.3333333
makes every motion 1/0.3 slower, `keyrate` 1000 a smooth motion; `throw_plays.py --stretch --rate`). Elevation of the
four throws (overhand, lob, flat, overhand with a still wrist), mock, Gun Angle 70; "matched" plays the slow motion at
0.3 of the frame rate, so the motion is sampled at the same points as the full-speed one:

| keys 90 | overhand | lob | flat | overhand0 |
|---|---|---|---|---|
| full speed, 72 fps | -0.7 (4.30 m/s) | +23.6 | +5.0 | +13.9 |
| bullet time, real speed, now | -0.6 (8.00) | +23.6 | +5.0 | +13.9 |
| bullet time, real speed, before (aim 0, tempo 0) | +21.9 | +18.4 | +5.0 | +30.9 |
| bullet time, made slowly, matched (21.6 fps), now | -0.7 (4.30) | +23.6 | +5.0 | +13.9 |
| bullet time, made slowly, matched, before | +12.3 | +20.8 | +5.1 | +8.1 |
| full speed, 90 fps / real speed / slowly matched (27 fps) | +5.2 / +5.1 / +4.4 | +21.5 / +21.5 / +21.6 | 5.0 | +12.7 / +12.7 / +12.7 |
| Sandevistan | as full speed | | | |

With 1000 keys a second: full 72 fps -1.3, +18.0, +5.1, +5.3; real speed -1.2, +18.0, +5.1, +5.4 (before +27.7, -0.2,
+5.3, +36.6); slowly, matched -1.3, +18.0, +5.1, +5.3 (before +30.7, +14.8, +5.7, +2.9). Speeds: real speed capped at
8 m/s of the game's time as designed (2.4 real), spin at 20; made slowly, the full-speed speeds.

Caveat (pre-existing, not changed): the estimate depends on the frame rate the motion is sampled at, through where
the release and the fastest sample land on the arc's flat-topped speed (an overarm arc turns about 7 degrees in 10
ms). Made slowly at 72 fps (sampled 3.3 times as densely), the throws match the full-speed throw at 240 fps rather than
at 72 (overhand0, 1000 keys: +19.4 against +19.5 at 240 fps, +5.3 at 72). The full-speed throw alone moves by up to 14
degrees between 72 and 240 fps with these synthetic arcs; and with `vr_fixed_frames_rate` 240 the scripted `-grabmain`
registered about 37 ms after its time (0 at 72).

## Throws at any frame rate (2026-10-06)

The author: "Ideally throwing should feel the same at any FPS (within reason, let's say between 70-240 FPS)". The
section above measured the same synthetic throw moving by up to 14 degrees between 72 and 240 fps. Causes, all on the
client's estimate (the server side is not frame-dependent: QC places the throw by its age, `weapons.qc` ~6430, and the
server and Box3D step on the fixed 72 Hz server tick; the slowed hands' caps are per second times dt):

1. `vr_throw.cpp releasePeak` anchored everything on the fastest *sample*: on a flat-topped or noisy peak any of
   several, a frame or more off the true peak; the velocity, spin and direction were unweighted sums of the samples
   within so many seconds of it (the direction's 40 ms: 3 samples at 72 fps, centred 14 ms back; 10 at 240, 19 ms
   back; an overarm arc turns about 0.7 degrees a millisecond).
2. The window ran 10 ms past the release (`vr_throw_lookahead`), but the estimate is taken when the move carrying the
   release is built, on a server frame: at 72 fps the release's own frame (never a sample after it), at 90-240 fps
   0-25 ms later (sometimes some).
3. The wrist's lever (the flick's velocity) and the release point were the peak sample's: a flick turns the hand
   about 11 degrees in a frame at 72 fps.
4. The release was the frame that saw the grip open: up to a frame late.
5. `vr_hands.cpp poseSpin` (the spin from the turn between two frames) was stamped at the newer frame: half a frame
   late (7 ms at 72 fps, 2 at 240).
6. The "37 ms late at 240 fps" was mostly the test: a play file's `cmd -grabmain` waits in the command buffer for a
   server frame (`cmd.c` Cbuf_Execute, `host.c` Cbuf_Waited) and the move is built on server frames, so it was 12-25
   ms late; the rest was the peak sample 12 ms before the release. A controller's release is timed on the tracking
   clock (`filterGrips`), which the client uses, so play was not affected by this part. The 1000-key plays also had
   +-0.1 m/s of noise in their velocity (positions written to 0.1 mm).

Fix: the samples are a signal in time (linear between them; the spin at the middle of its turn, `Motion::spinLag`).
The window is `vr_throw_window` before the release, up to it (`vr_throw_lookahead` no longer used). The speed is
smoothed by a line fitted over `vr_throw_peak_span` either side (continuous least squares, weighed by time: a line is
the mean inside the window and unbiased at its end, where a throw released while speeding up has its peak), sampled a
millisecond apart; the peak is the middle of where it is within 2% of its top. The velocity is the smoothed one there,
as fast as a parabola fitted to the speeds over 30 ms says (not at the release), the direction the mean over
`vr_throw_dir_lookback` before it, the spin the mean over twice the span, the lever and release point between the
samples. `filterGrips` puts the release where the analog grip crossed its line between the two frames. Slow motion's
windows (`motionRate`) and both hands (`estimateBothAt`) go through the same code.

Test aids: `vr_mock_play` takes `<t> grip <main|off> <0..1>` (the analog grip, linear between its keys);
`throw_plays.py` lets the grip open as a hand does by default (`--grip 2`: 1 until 15 ms before the release, 0 35 ms
after: it crosses the release's line at the release; `--grip 1` a button step, `0` as before) and writes positions to
1 micrometre (`--digits`, 4 before).

Measured offline (`Misc/quakevr/throw_fps/compare.py exact 0.02`: a replica of the estimate, old and new, on the same plays, which reproduced the engine's numbers at
72 fps to 0.1 degree and the +19.5 overhand0 at 240): speed m/s, elevation, spin rad/s; smooth motion (1000 keys,
exact), the release where the grip crossed its line:

| | overhand | lob | flat | overhand0 |
|---|---|---|---|---|
| before, 72 fps | 4.39, -1.4, 32.3 | 4.88, +17.9, 14.5 | 5.00, +5.0 | 6.34, +14.7, 11.3 |
| before, 90 | 4.80, +1.2, 32.1 | 4.85, +17.7, 14.4 | 5.00, +5.0 | 6.32, +12.9, 11.5 |
| before, 120 | 4.49, +0.1, 33.1 | 4.78, +20.1, 15.1 | 5.00, +5.0 | 6.32, +12.6, 11.4 |
| before, 144 | 4.35, -1.8, 35.0 | 4.77, +19.5, 15.5 | 5.00, +5.0 | 6.32, +12.1, 11.7 |
| before, 240 | 4.32, -3.8, 33.8 | 4.80, +17.6, 15.1 | 5.00, +5.0 | 6.30, +13.9, 11.5 |
| now, 72 | 4.88, -0.6, 32.8 | 4.72, +18.8, 15.5 | 4.98, +5.0 | 6.28, +16.3, 11.5 |
| now, 90 | 4.89, -1.2, 32.9 | 4.72, +18.9, 15.5 | 4.99, +5.0 | 6.29, +16.5, 11.5 |
| now, 120 | 4.89, -0.9, 33.1 | 4.73, +18.9, 15.5 | 4.99, +5.0 | 6.29, +16.4, 11.5 |
| now, 144 | 4.88, -1.1, 33.2 | 4.73, +18.9, 15.6 | 4.99, +5.0 | 6.30, +16.5, 11.5 |
| now, 240 | 4.89, -1.2, 33.3 | 4.73, +18.9, 15.6 | 4.99, +5.0 | 6.30, +16.5, 11.5 |

Spread over 72/90/120/144/240 fps and two frame phases (speed, elevation, spin): before 11%, 6.6 degrees, 13%; now
0.5%, 0.7 degrees, 2.1%. Frame times jittering +-30%: now at most 1.1%, 1.1 degrees, 2%. With the old 0.1 mm plays
(noisy velocity): now at most 2.1%, 2.1 degrees, 2.2% (before 27%, 14 degrees, 13%). A grip let go as a button (a
step) still puts the release up to a frame off (overhand0, released while speeding up: 7 degrees, as before). The
90-key plays (the mock's velocity a stairstep, no runtime's) still flip the flicked overhand between its two speed
peaks (the wrist's and the arm's): an artefact of the mock's held velocities.

At 72 fps the throws change a little: the flicked overhand 4.9 m/s (4.4 before: the release's own speed now counts in
full at the window's end), overhand0 +16 degrees (+5 to +15 before, depending on where the frames fell).

In the engine (b7b1ee7f plus this, Release; `vr_fixed_frames_rate` as given, the server on its own 72 Hz clock: before
the tickrate work on host.c; 1000-key plays with the grip opening as a hand does, `--grip 2`; mock, Gun Angle 70,
`vr_mock_grip_velocity 1`), speed m/s, elevation, spin rad/s:

| | overhand | lob | flat | overhand0 |
|---|---|---|---|---|
| before, 72 fps (the old 1000-key plays) | 4.37, -1.3, 32.3 | 4.82, +18.0, 14.5 | 4.97, +5.1 | 6.36, +5.3, 11.7 |
| before, 240 fps | 4.42, -1.6, 34.2 | 4.70, +22.8, 15.7 | 5.02, +5.1 | 6.33, +19.5, 11.5 |
| now, 72 fps | 4.88, -0.6, 32.8 | 4.72, +18.8, 15.5 | 4.98, +5.0 | 6.28, +16.3, 11.5 |
| now, 90 | 4.89, -1.2, 32.9 | 4.73, +18.9, 15.5 | 4.99, +5.1 | 6.29, +16.5, 11.5 |
| now, 120 | 4.89, -0.9, 33.1 | 4.73, +18.9, 15.5 | 4.99, +5.1 | 6.29, +16.3, 11.5 |
| now, 144 | 4.89, -1.3, 33.2 | 4.73, +18.9, 15.6 | 4.99, +5.1 | 6.30, +16.5, 11.5 |
| now, 240 | 4.89, -1.4, 33.3 | 4.73, +18.9, 15.6 | 4.99, +5.0 | 6.30, +16.6, 11.5 |
| now, 90 fps, frame times +-30% (`vr_fixed_frames_jitter 0.3`) | 4.86, -0.7, 33.0 | 4.72, +18.9, 15.5 | 4.99, +5.1 | 6.30, +16.3, 11.5 |
| now, 240 fps, +-30% | 4.88, -1.3, 33.3 | 4.73, +19.0, 15.6 | 5.00, +5.0 | 6.31, +16.4, 11.5 |
| bullet time at real speed (default caps), 72 / 240 | 8.00, -0.6 / -1.4, 20 | 8.00, +18.8 / +18.9, 20 | 8.00, +5.0 | 8.00, +16.3 / +16.6, 20 |
| bullet time, made slowly (`stretch` 3.33), 72 / 240 | 4.89, -1.4 / -1.5, 20 | 4.73 / 4.76, +18.9 / +17.9, 15.6 | 5.00, +5.0 | 6.30, +16.5, 11.5 |
| Sandevistan, 72 / 240 | as 72 / 240 fps above, to the hundredth | | | |
| 90-key plays (the mock's stairstep velocity), 72 / 240 | 5.01 / 4.91, -4.4 / -5.1, 33 | 4.69 / 4.73, +21.1 / +19.5, 15.4 | 4.97, +5.0 | 6.26 / 6.30, +12.4 / +12.8, 11.6 |

Over 72-240 fps and the jittered runs: speed within 0.6%, direction 0.8 degrees, spin 1.5% (before: 14 degrees at the
most, overhand0). The engine gives the offline replica's numbers to 0.1 degree. Bullet time keeps its design (the
slowed hand's 8 m/s and 20 rad/s caps, the controller's way: `vr_throw_slowmo_aim`; a throw made slowly as at full
speed: `_tempo`; the made-slowly lob 1 degree lower at 240 fps), Sandevistan is full speed's. The 90-key plays now agree
between 72 and 240 fps within 2.2 degrees (14 before); they differ from the smooth plays, being another motion
(velocities held for 11 ms). Test: `throw_plays.py --rate 1000` (the grip opening as a hand does is its
default, `--grip 2`) played with `vr_fixed_frames 1; vr_fixed_frames_rate <fps>; vr_gunangle 70; vr_debug_throw 2;
vr_mock_grip_velocity 1` (`vr_fixed_frames_jitter`, new: uneven frame times, a seeded sequence). e1m1 smoke clean.

In VR: throw overhand, underarm, flat and with a wrist flick at your headset's lowest and highest refresh rates: the
same throw should go the same way and as far. Two-handed throws and throws in bullet time as before.
## Profiling: CPU and GPU, loading and gameplay apart (2026-10-06)

The whole suite with the author's settings (one run each), VTune on the loads alone and on gameplay windows alone,
the engine's GPU scopes and Nsight Systems: PROFILING_2026-10.md (the tables, the hotspots,
a decision list of twelve larger items). Fixed: the hull build's allocations (warden's warm load -15%, ad_grendel's
-16 to -20%, the same trees), the particles' retro light levels (0.2 to 0.1 ms a frame), the suite's gameplay windows
no longer overlapping the load's AO bakes (`vr_ao_finish`). New: `vr_bench_profiler` / `vr_profiler_collect` (VTune
collects only benchmark windows or only map loads; Debug > Profiling and Memory > External Profiler Collects),
`Misc/quakevr/bench/qvrprof.sh`, `vtune_attr.py`.

## Hull build: containers and hulls kept for a reload (2026-10-06)

His questions on the hull build's vectors (clipWinding's two heap vectors, `Winding` and `Poly` as small vectors, a
`za::Vector` audit) and "keep the compiled hulls across a reload of the same map". Measured first (sizes of every
winding, piece, split and candidate list on warden, ad_grendel, e4m7, e1m1; allocations per function), then:
`Winding` = `SmallVector<dvec3, 8>`, a node's pieces `SmallVector<Frag, 4>`, clipWinding's buffers
`SmallVector<..., 64>`, `Poly` kept a `za::Vector` (too big in place), and the tree's plane index kept across its
models' builds (it was rebuilt for every brush model: 5.2 of ad_grendel's 8.5 million load allocations). A big map's
load: 38 M allocations to 4.3 M; warden warm load -39%, ad_grendel -35%; same trees (hashes). Then `vr_hull_keep`
(default 1): brushes and trees kept at a map change under the world's content hash, taken back by the next load of
the same content: warden's `restart` 2134 to 378 ms, ad_grendel's 1283 to 326. `vr_hull_keeptest` checks kept against
fresh; Debug > Keep Hitboxes for Reloads, Hitbox Keep Test. New bench scenario `load_reloads`. Details, tables and
the wider `za::Vector` audit (recommendations only): PROFILING_2026-10.md, "Hull build,
follow-up". For VR: die on warden (or `restart`) and check the reload is quick and walls/doors still block you.

## Server tick rate (2026-10-06)

His words: "Please fix the server tick rate." The melee audit found that the server's rate followed the headset's:
`host.c` ran a server frame once the time built up reached `host_netinterval` and gave it all of that time, but
`host_netinterval` was a float, and 1/72 as a float (0.013888889) is a little over an exact 1/72 s frame (a double):
one 72 Hz frame never reached it, so the server ran every other frame with two frames' time. Measured (mock,
`vr_fixed_frames_rate`, `host_tickstats`; the same take played at each rate):

| headset frames | old: ticks/s | old: tick length | old: Box3D steps a tick | new: ticks/s | new: tick length |
|---|---|---|---|---|---|
| 60 | 60 | 16.7 ms | 1 | 72 (1 frame in 5 runs 2) | 13.89 |
| 72 | **36** | 27.8 | 2 | 72 (one each frame) | 13.89 |
| 72, +-2% jitter | 45.7 (14 or 28 ms) | 13.9-28.2 | 1.58 | 72 | 13.4-14.4 |
| 80 | 40 | 25.0 | 2 | 72 (1 frame in 9 none) | 13.89 |
| 90 | 45 | 22.2 | 1 | 72 (4 frames in 5) | 13.89 |
| 120 | 60 | 16.7 | 1 | 72 (3 in 5) | 13.89 |
| 144 | 48 | 20.8 | 1 | 72 (every other) | 13.89 |
| 144, +-2% | 55.9 | 13.9-21.0 | 1 | 72 (every other) | 13.6-14.1 |
| 240 | 60 | 16.7 | 1 | 72 | 13.89 |
| dedicated server | 16-20 (`sys_ticrate`) | 50-60 | | 72 | 13.5-14.5 |

The tests never saw it: `vr_fixed_frames_rate 72` forced a server frame with each host frame
(`vr_motion_play.cpp serverFrameOverride`), so `vr_motion_eval rate 72` actually ran a 36 Hz server, and
MULTIPLAYER.md's "72 Hz whatever the headset's refresh rate" was wrong for 72-144 Hz headsets.

**Fix** (`host.c`, `Host_FixedTicks`; `host_fixedtick 1`, default):
- `host_netinterval` is a double. The time built up is spent in fixed ticks of 1/72 s, the rest carried to the next
  frame; a frame that is behind runs several (at most `host_fixedtick_max` 8, about 110 ms; past that the time is
  dropped: the game slows after a long hitch instead of stalling to catch up).
- A frame within `host_fixedtick_tolerance` (1 ms) of a whole number of ticks runs them with all of its time spread
  over them (each up to 7% longer or shorter): a 72.1 Hz headset, or one with jittery frame times, gets one tick each
  frame (144 Hz: every other), never a skipped one then a doubled one beating against the 72 Hz clock. The game's
  clock stays the wall clock's.
- Every tick sends a move (`CL_SendCmd`) and runs `Host_ServerFrame`; a frame's later ticks send the same sticks' and
  mice's accumulated move (`cl.pendingcmd`), the hands' poses are the frame's (the server's melee sweeps the first
  tick's move, the later ones see no motion), the room-scale step goes with the first. Walking at 30/60/72/144 fps
  goes 300-313 units a second.
- `host_timescale`, `host_framerate` and slow motion (`vr_timescale`, bullet time, Sandevistan) scale each tick as
  before: at 90 fps, 72 ticks a second either way, the server's clock x0.5 with `host_timescale 0.5` and x0.25 with
  `vr_timescale 0.25`.
- A dedicated server's frame waits for the next tick (at most `sys_ticrate`): 72 Hz, a move from each client used
  each tick, its clients' updates even (down: about the listen server's 18 KB/s instead of 5.7).
- `host_fixedtick 0`: the old way, exactly (the float comparison kept), and a dedicated server at `sys_ticrate`.
- Box3D steps once a tick (its 1/45 s pieces: one piece at 1/72; the old 25-28 ms frames took two).
- `wait` counts frames that ran the server: at 90 fps four frames in five; below 72 fps a frame with two ticks counts
  once.

The client's drawing needed nothing: `CL_LerpPoint` lerps the props between the last two messages at `cl.time`, which
advances with the frames; a ragdoll's drawn pose blends between its steps the same way (`vr_debug_ragdoll 2` at 90
fps: cl.time 11.1 ms a frame, the blend 0.8, 0.6, 0.4, 0.2, 0.0 between 13.9 ms steps, no stall). `vr_drawn_motion_test`
after a blast (40 frames, +-2% jitter): no stalled frames at any rate either way; the pelvis's "uneven" 0.03-0.06 new,
0.02-0.03 old (without jitter 0.022-0.036 vs 0.008-0.015: the old 45 or 48 Hz steps were halved exactly by the
frames, a straight segment over two frames; 72 Hz steps fall at varying phases of the frames, each frame's move
mixing two segments: still far from a stall's 2).

Throws (`throw_slowmo_test.sh`, the four 1000-key plays, frames +-2% jittered): the same at 72, 90, 144, 240 fps and
with either tick (overhand 4.88-4.90 m/s, lob 4.72-4.73, flat 4.98-4.99, overhand0 6.28-6.30; spins within 0.6 rad/s),
the "now" rows of "Throws at any frame rate" (their in-engine sweep, still to run there, is this).

Cost: the server runs 72 times a second instead of 45 at 90 fps: e1m1's start, 90 fps, `vr_profile_report`: the
server 0.16-0.19 ms a frame against 0.13-0.15 (exclusive run).

Test aids: `host_tickstats [reset]` (Debug > Profiling and Memory > Server Tick Stats: ticks a second, their lengths in
half milliseconds, each frame's tick count and the last 48 as digits, the longest run without a tick, the server's
clock against the frames', Box3D's steps), Fixed 72 Hz Server Tick (`host_fixedtick`) beside it;
`vr_fixed_frames_jitter <fraction>` (each frame's time jittered around `vr_fixed_frames_rate`, without drift); at
72 with `host_fixedtick 1` the fixed frames now go through the engine's ticks (one each, of exactly 1/72 s: the same
as the forced frame). `vr_motion_play` and `vr_motion_eval` take `rate <hz> [jitter <fraction>]`: the take resampled at
that rate (its frames' times jittered), the server on the engine's ticks as a headset at that rate gets them, and
`host_tickstats` printed over the take (old: `rate 72` gave 36 ticks a second; new 72). His takes keep their recorded
server frames when played at their own rate (they were recorded at the old cadence: 36 Hz at 72). The melee eval
(`eval.sh`) could not run: the current takes are archived pending new recordings.

### Checklist
- [ ] At 72 Hz and at your highest refresh rate: monsters, gibs and thrown props move as smoothly as before (no
      stepping), and melee lands as before.
- [ ] Debug > Profiling and Memory > Server Tick Stats after a minute of play: about 72 ticks a second, lengths
      13.5-14.0 ms, the frames' digits a steady pattern (72 Hz: all 1; 90: 4 ones in 5; 144: 0 and 1 alternating).
- [ ] Fixed 72 Hz Server Tick off: Server Tick Stats says 36 at 72 Hz (the old way), back on: 72.
- [ ] Bullet time and Sandevistan: still smooth, throws in them as before.
- [ ] A friend on a dedicated server: hands, melee and climbing respond as on a listen server.
## Dawn of the Machine (MG3): foundation (2026-10-06)

Phase A of [MG3.md](MG3.md) (the plan is now in the repo). MG3 stays gated (`nativeReady` false); every
test uses the developer path (`vr_campaign_native mg3`, `-nomapindex`).

- **M3-01 scaffold and entity checker.** `QC/vr_mg3_defs.qc` (`MG3_Campaign()`), `QC/vr_mg3_test.qc` (`vr_mg3_test`,
  unarchived; Debug > Tests > Dawn of the Machine Tests), `Misc/quakevr/check_mg3_entities.py` (reads the owned
  `rerelease/mg3/pak0.pak` read-only: per-map missing classnames and unknown keys, totals; `--expect-missing`,
  `--expect-placements`). Today: 22 maps, 156 classes, **47 missing, 1,397 placements**, 10 unknown keys (33 uses:
  `aggro_target`, `health_target`, `tele_target`, `wave1..3`, `fog_sky_factor`, editor noise). id1's rune logic is off
  for campaign 5: the four-rune finale text, the `info_player_start2` return spot and the deathmatch `start` episode
  cycle (MG3's serverflags mean its own runes and Bloody Nightmare bits); Honey's flag meanings were already off for
  every official campaign (`MG_WorldCampaign`). All 22 BSPs (start..boss2, dm1, both brush models) load with exit 0
  and no Host_Error; e1m1 and MG1 hub (20/0) smoke pass.
- **M3-02 extra saved-state slots.** The engine's extended spawn parms grow from 17..50 to 17..56
  (`lastExtSpawnParm`, `Quake/vr/vr_progs.hpp`); QC `parm51..55` carry the five upgrade masks and `parm56` the bloody
  weapon bits (official parm10..15, which VR's hands/holsters own). Live state is the saved globals `MG3_upgrade_*`,
  `MG3_bloody` (accessors `MG3_UpgradeMask(kind)`, `MG3_SetUpgradeMask`, `MG3_BloodyBits`, `MG3_SetBloodyBits`;
  kinds `MG3_UPGRADE_HEALTH` or `AID_SHELLS..AID_CELLS`); `MG3_EncodeParms` runs first in SetChangeParms (so a dead
  player's SetNewParms keeps them, as upstream), `MG3_DecodeParms` in DecodeLevelParms joins the level-start values
  with the server's (a coop join or respawn never takes back a pickup). SetNewParms leaves them alone, so only a new
  game (`map`, fresh globals) clears them; older saves load them as 0. Measured (`vr_mg3_test 2`/`3`/`1`): seeded
  masks 5/2/8192/16384/1, bloody 3 survive changelevel map1->map2, hub and start; a bit added on map2 is gone after
  `restart` (a death: level-start masks) and back after loading the save made with it; `map map3` clears all.
  Stock e1m1->e1m2 (+ save/load), Dopa e5m1->e5m2 and MG1 mge1m1->mge1m2 (+ save/load, six seeded holster clips/ids)
  carry the same hands, holsters, clips and ids.
- **M3-03 capacity helpers.** `VR_MaxAmmo(e, aid)`, `VR_BaseHealth()`, `VR_MaxHealth(e)`, `VR_MegaHealthCap(e)`
  (`QC/vr_ammoutil.qc`) replace every hard-coded cap: `bound_other_ammo`, `ammo_touch`, the Horde ammo pickup,
  `T_Heal`'s 250 and the megahealth's, SetNewParms' health, PutClientInServer's `max_health`, SetChangeParms' carry
  bounds and the hand grenade pouch (`VR_HGREN_MAX_ROCKETS` is now `VR_HandGrenade_MaxAmmo`). Campaign 5 asks
  `MG3_Capacity(kind)` (`vr_mg3_defs.qc`: single player 50 health, 50/100/20/100 ammunition, +10 per upgrade bit;
  deathmatch id1's 100 and 100/200/100/200; mega cap 500, upstream's); DecodeLevelParms sets `max_health` again once
  the level's upgrades are known (and clamps health, campaign 5 only). Every other campaign gets its old numbers.
  Measured (`vr_mg3_test 4`, Capacity Check, any campaign: caps, overfill + bound, heal from 1): id1 skill 1
  100/250 and 100/200/100/100 (+ 200/100/100 mission-pack); Dopa skill 3 and MG1 skill 3 50/250, Dopa skill 1
  100/250; MG3 map1 50/500 and 50/100/20/100; after seeding the M3-02 masks and a changelevel 70/500 and
  60/110/30/110, health carried at 70 (4/0 each). Regression: Dopa triggers 34/0, world 19/0, health/megahealth
  aids as before; MG1 hub 20/0, mge2m2 puzzle 15/0, mge5m2 route 10/0, Horde 24/0; e1m1 smoke.
- **M3-04 upgrade items.** `QC/vr_mg3_upgrades.qc` (adapted `mg3_upgrades.qc`, GPL header kept):
  `item_upgrade_health/shells/nails/rockets/cells`, campaign 5 only; the map's bit (`MG3_UpgradeFlag`: map1..8,
  secret1..6, map2b = 8192) or the authored `upgrade_flag`; first take sets the bit, +10 capacity and fills it (health
  healed to the new `max_health`); a later visit's is drawn faded (alpha 0.6, upstream) and only says so;
  localized `$mg3_qc_upgrade_*` messages through the engine's sprint with arguments (`MG3_sprint_args`, builtin 24).
  Physical: taken to a holster like keys and runes (`VR_PickupObj_Mark`; walked over when pickups are not objects).
  FGD regenerated (300 entities). Measured (`vr_mg3_test 5`, the real pickup: `carry_use` -> itemTouch): map3 5/5
  taken, caps 50/50/100/20/100 -> 60/60/110/30/110, each filled (8/0); map4 -> 70/70/120/40/120 (8/0); back on
  map3 5 faded, no award, health left at 1 (8/0); map2b bit 8192, 4 kinds (7/0); hub, a save and its load keep
  masks 8204 x4 / 12 and caps 80/80/130/50/120; `map map1` clears. Checker after M3-04: 42 missing, 1,328
  placements. Smoke: e1m1, Dopa triggers 34/0, MG1 hub 20/0. `eval.sh` cannot run here (archived takes missing);
  no melee code changed.
  For VR: on map3 (`vr_campaign_native mg3`, `map map3`) take an upgrade to a holster: message, capacity, refill;
  return to the map to see it faded.
## The author's decisions: Horde deaths, hard gib throws, the rottweiler's head, rocks in multiplayer (2026-10-06)

### Rottweiler head zone (positional damage)

His answer: the rottweiler gets a head zone like the fiend's. `PositionalHead` (weapons.qc) has his now, and
decapitation's own branch for him (`VR_Decap_HeadZone`) is gone: one source for headshots, melee head hits, head pops
and beheading. The numbers are decapitation's (23 forward, 1 up, radius 7): `progs/dog.mdl`'s frames have no names, so
his rest pose (`hitmodel_rest`'s stand frame, `restPoseOf`: the first frame named "stand", else frame 0) is `$attack1`,
lunging, his head level with his origin. Measured on that frame: the snout's front vertices at 29.6 forward, 0.2 up;
the head's (forward of 22) centroid 26.9 forward, 3.4 below (the forelegs reach forward under it in the lunge). In
`$stand1` his head lies 22 to 32 forward and 8 below to 3 above his origin: a hit there maps to the same triangle in
`$attack1`, so the zone fits him standing as well.

Precise hits' ring (`impulse 238`, which now also rings his head with a blade: positional melee's region of the point
it met), e1m1, a rottweiler 96 units ahead (`vr_test_spawn 7`): head ring 0 headshots before (no zone), 18-19/24 on his
model after (7/24 on boxes); blade ring 18/24 head (22-23/24 contacts); chest ring 3 head, 9 limb, 12 legs on his model.
`vr_decap_test 17` (shotgun at his head, health 500): "6 pellets, 6 head (x1.50)"; `vr_decap_test 1`: beheaded,
h_dog thrown at 174 u/s. (`vr_decap_test 7` reads "limb" on him: its blow lands at the zone's middle *inside* the
animated model, and the nearest triangle to that point in `$stand1` is not his head's; a blade meets his surface first,
as the blade ring shows.)

In VR:
- [ ] Shoot a rottweiler in the head, slash it (Show Damage Numbers): "head (x1.50)"; a killing slash beheads him.

### MG1 Horde: a solo death restarts the arena, never the last save

His answer: dying in a Horde game must not load an earlier save. The engine's `restart` (Host_Restart_f) autoloads the
session's last save for a dead single player (`sv_autoload` 2, the default: Host_AutoLoad); Horde's team wipe
(`MGH_DeathThink`, vr_mg_horde.qc) restarted with it, so a save made mid-arena was loaded instead of the fresh arena
the official game gives. Now `restart fresh` skips the autoload (any other argument, or none, is the old command), and
Horde's wipe sends that. Only an active Horde game: every other death (`respawn`'s `restart`, a changelevel to the
same map) still autoloads; coop never did. Another engine ignores the argument and restarts as before.

Tests (horde1, solo, `developer 1`, a save `dz_horde` made first, `vr_mg_horde_test 17` killing the player): the
release-then-press gate (`+attack`, then test 16 to continue after the queued restart): "restart horde1", no
"Autoloading", wave 0, health 100, dead 0. Typed after the same death: `restart` prints "Autoloading..." (the engine's
autoload, kept), `restart fresh` restarts the map without it.

In VR:
- [ ] Horde (horde1): save, die, let go and press fire: the arena restarts from wave 0 (not your save). In e1m1: save,
  die, press: your save loads as before.

### Hard throws burst gibs on walls (Hard Throw Bursts)

His answer to "should hard throws burst on walls rather than stick (lower Gib Splat Speed)?": burst. But a gib's own
speed can't tell a hard throw from a soft one: its mass limits how fast it leaves the hand (`throwvelocity`'s soft
limit), so from about 4 m/s of the hand on every gib of 8 kg or more leaves at the same speed (table: release speeds,
u/s). No Gib Splat Speed bursts a hard throw and keeps a 3-6 m/s one sticking: at 150 (under the 8-20 kg gibs' speeds)
gib1 bursts from 5 m/s, gib3, gib2 and the ogre's head never reliably, and lobs at 2 m/s burst on the floor before
the wall (4-8 of 10). So the throw is judged by the **hand's** speed as it let go (`vr_gib_handspeed`, from the throw
estimate before the mass limit): **Hard Throw Bursts** (`vr_gib_splat_throw`, m/s, default 7; Gibs and Corpses, after
Gib Splat Speed; 0 off): a destroyable gib or head thrown that hard bursts where a softer one would stick (the same
wall hit, `VR_Gib_Think2`: its speed into a wall at Speed to Stick, turned by a quarter). Gib Splat Speed (250) stays:
any gib that fast still bursts on a wall or a monster (a small gib from 6 m/s). 7 m/s is between his measured gib
throws (3.6-4.5 m/s in the takes; the brief's soft-to-normal 3-6) and this file's "hard" throws (9 m/s). A new
setting: no config migration.

`vr_smallgibs_test 22` (Debug > Gore Tests > Thrown Gibs Stick, by Mass; now hand speeds 2 to 10 m/s), e1m1's start
facing south (`setpos 480 -352 88 0 270 0`, test 15), 10 throws each, stuck of 10 (burst):

| gib (release u/s at 2/3/4/6+ m/s) | 2-6 m/s, off (`vr_gib_splat_throw 0`) | 7-10 m/s, off | 2-6 m/s, 7 (new) | 7-10 m/s, 7 (new) |
|---|---|---|---|---|
| gib1 8 kg (64/107/161/195) | 50/50 | 40/40 | 50/50 | 0 (40 burst) |
| gib3 12 kg (64/106/133/136) | 50/50 | 40/40 | 49/50 | 0 (40) |
| gib2 20 kg (64/84/86/86) | 47 (1) | 35 (3) | 50/50 | 0 (40) |
| grunt head 10 kg (64/107/150/160) | 49 | 40/40 | 49 | 0 (40) |
| ogre head 30 kg (57/59/59/59) | 49 | 40/40 | 47 | 0 (38; 2 landed short) |
| small gib 0.3 kg (64/107/163/283) | 41 (9 at 6 m/s) | 0 (40) | 40 (10 at 6 m/s) | 0 (40) |

(Gib Splat Speed 150 and Hard Throw Bursts 0, for the record: stuck of 10 at 2..10 m/s, gib1 6 10 7 0 0 0 0 0 0, gib3
8 10 8 6 8 9 6 5 7, gib2 5 10 9 10 9 10 10 8 10, grunt head 8 10 5 2 3 3 4 3 4, ogre head 4 6 3 5 6 4 1 8 4.)

In VR:
- [ ] Throw gibs and heads at a wall: a soft or normal throw sticks; a hard one (a fast arm) bursts them in a mist,
  heavy ones too.

### Rocks and bricks in multiplayer: as in single player

His answer: Most in Multiplayer (`vr_debris_mp_max`) defaults to "same as single player". The single player cap is
Most in a Map (`vr_debris_max`, 160) and the free entities; multiplayer took the smaller of that and
`vr_debris_mp_max` (0: none). Now -1 (the new default, shown as "Single Player's" at the slider's leftmost step,
Settings > Rocks and Bricks) leaves multiplayer the single player's cap, following Most in a Map; 0 is still none, a
number still caps. Config 94 moves the old default 0 to -1. Measured (e1m1, `vr_debug_debris 1`): single player 34
pieces; a two-player listen server (`-listen 2`, coop) 29 (limit 160), `vr_debris_mp_max 0` none, 16 gives 16; a
config with `vr_debris_mp_max "0"` at version 34 loads as -1. Bandwidth (MULTIPLAYER.md, the mpsplit measurements:
the same 29 pieces): a remote client by the rocks 256 -> 739 B of its 1400 B datagram a frame, about 17 B a piece in
sight, until resting pieces get baselines.

In VR (multiplayer):
- [ ] Host e1m1 for a friend: rocks lie about as in single player; pick one up and throw it, the other player sees it.

## AO bakes: a disk cache, and a faster bake (2026-10-06)

His words: "Can we store them on disk with some sort of hash that correctly detects any modification to the model to
recompute them? Can we optimize the algorithm itself?" (PROFILING_2026-10.md, item 2: 123 models, 9.3 s of 4 pool
threads after the firing range's first load, redone every session.)

**Disk cache** (`vr_ao_cache`, default 1; 0 bakes every time; 2 bakes anyway and compares with the file;
`vr_ao_cache_info`; Debug menu: "AO Bakes on Disk", "Keep AO Bakes on Disk"). Each bake
is a file `<gamedir>/cache/ao/v<BAKE_VERSION>/<key>.ao`, beside the normal maps' cache. The key is the SHA-256
(vr_sha256) of everything the bake reads: `BAKE_VERSION`, the ray count, the reach/nearest/lift shares, the rays'
directions, Quake's 162 vertex normals, and the model's poses (positions and normal indices), triangles (as the bake
sees them), scale and origin. A file holds a 64-byte header (magic `QVRA`, version, vertex and pose counts, the whole
32-byte key, the payload's size and FNV-1a 64) and the bytes; a file whose header, key, size or checksum is wrong is
"rejected", baked again and written over. Reads and writes run on the bake task (never the main thread): hashing,
reading and checking 123 models takes 31-48 ms in all (0.26-0.39 ms each). Writes go to a `.tmp` beside the file and
are renamed. The bake task's first write in each game directory removes the other versions' folders and, past 64 MB,
the oldest files (a full firing-range set is 1.4 MB, 121 files: two models share one bake). No cvar changes a bake;
changing any of its constants changes the key. `BAKE_VERSION` must be bumped when the algorithm's output or the
file's layout changes.

Checked on the firing range (developer 1 prints each model's bytes' FNV): the first session baked 121 and wrote 121
(2 read: same data as another model); the second read 123 from disk, baked 0, `vr_ao_finish` waited for nothing; all
123 FNVs equal to the bakes before the change. One model's last vertex changed, plus three files damaged (a payload
byte, the magic, a truncation): exactly those 4 baked again (3 "rejected"), 119 read. A changed constant (the lift
share, a test build): all baked again. `vr_ao_cache 2`: 123 compared, 0 differed.

**Faster bake, the same bytes.** Per pose, the triangles go into a uniform grid of cubes half the reach wide (each by
its centre; those wider than half the reach in a list tested for every vertex), and a vertex's candidates come from
the cells within the reach plus the cells' widest radius, then pass the very test the old bake used (branch-free:
a quarter pass, unpredictably). The rays then run 8 at a time (AVX, when SDL_HasAVX says so) or 4 (SSE2) through
Möller-Trumbore with the same float operations in the same order as the scalar code (the build has no FMA
contraction), the ray-independent terms (s, q, e2.q) once per candidate; skipped tests are masked lanes, NaNs fall the
same way, and the nearest hit does not depend on the candidates' order. The old bake stays as `bakePoseReference`;
`vr_ao_bench [name part] [sse] [reference]` (Debug menu "AO Bake Benchmark") bakes the loaded models again on the main
thread, one thread, and compares the bytes. `BAKE_VERSION` stays 1 (the same bytes; the old files check out).

| | before | after |
|---|---|---|
| firing range, in game (123 models, 4 threads, wall) | 9988 ms (hknight 943, player 907) | 1160 ms (player 114, hknight 92) |
| warden, in game (164 models) | 4968 ms | 1046 ms |
| firing range, `vr_ao_bench` one thread (112 models) | 24873 ms (reference) | 3149 ms AVX (7.9x), 4586 ms SSE |
| e1m1, `vr_ao_bench` one thread (97 models) | 11902 ms | 1571 ms AVX (7.6x), 2271 ms SSE |

The same bytes: every model's FNV equal to the old bake's on the firing range (123) and warden (164); `vr_ao_bench .
sse reference` 0 differed on the firing range (112) and e1m1 (97); `vr_ao_cache 2` over the files the old bake wrote:
123 compared, 0 differed. A cell half the reach wide was the fastest (a third: 3.7 s, three quarters: 3.4 s).
Second session (warden): 164 read in 265 ms on the bake task (the reads share the disk with the map's load), 0 baked.
## Cvar audit: one dead setting removed, the rest ranked (2026-10-06)

`Misc/quakevr/cvar_inventory.py` lists every Quake VR cvar (2,064) with where it is read and written (C++ logic, QC,
menus, migration, motion recorder, cfgs, tools, docs), its default, flags and last commit; `--dead` lists the ones no
code reads. Since the 2026-10-03 cull every cvar but one had a reader: `vr_throw_lookahead` (documented unused) is
removed, with the QC handle `cvarh_vr_throw_hit_min_speed` that nothing read. Stale config lines for removed cvars are
now dropped quietly (vr_cvars.cpp `retiredCvars`, asked by cmd.c before "Unknown command"). The candidates for
removal or merging (about 160 per-class overrides at their globals, ~25 A/B switches whose new side won, 86 hidden
archived tuning knobs, duplicated campaign status cvars, finished test knobs, the config migration) are ranked in
CVAR_AUDIT.md, none done without the author's yes.

In VR: nothing to test (no behaviour changed); an old config still setting `vr_throw_lookahead` loads with no message.

## His gameplay and look values as defaults (2026-10-07)

INSTALLER.md Appendix A's "Gameplay and look: promote?" table, his values re-read from his config: fire particles
(`vr_fire_particles_alpha` / `_count` / `_origin` / `_size` 1 / 8 / 0.2 / 2.5, were 0.55 / 6 / 0.25 / 2; count and size
in vr_defaults.cfg, the others compiled in), head pops (`vr_decap_pop_always_range` / `_never_range` 2 / 12, were 3 / 15;
`vr_decap_pop_thrown_light_chance` 0.25, was 0), limb grabs (`vr_ragdoll_grab_reach` / `vr_ragdoll_hand_stick` 2 / 2,
were 6 / 12), `vr_messages_hologram_height` 10 (was 5). Config 95 moves a config still at the old default.

- Retro textures: every kind has had his look since config 89 (vr_retro.cpp shippedLook), but the All Categories
  panel (`vr_retro_all_*`) still started at the old values, so applying it as it started undid the look. Its values
  now start from the shipped look (0 / 0.5 / 0.5 / -1 / 1 for average / block / dither / fade / palette); config 95
  moves a panel still at the old values (`retro::migrateAllPanel`; written as values only, never applied).
- The grappling hook's flashlight clip (`vr_wofs_torch_out_18` / `_up_18` -0.035 / 0.075): `vr_wofs_version` 35.
- Gib and head weights (`vr_props_version` 58): gremlin head 18 to 9, gib2 20 to 15, gib3 12 to 10, grunt 10 to 8,
  dog 12 to 9, enforcer 16 to 9, knight 12 to 8, hell knight 17 to 11, ogre 30 to 15, vore 12 to 10, shambler 70 to 65,
  fiend 28 to 18 (gib1 8, player 5, scrag 10, zombie 8, scourge 50 unchanged). A slot still its model's at the old
  weight takes the new one.
- Limb masses: a head cut off a ragdoll (vr_decap.qc, vr_limbs.qc `VR_Limb_SetMass`) weighs 7% of its class's
  `vr_ragdoll_<class>_mass` times `vr_limbs_mass_scale` (1): grunt 5.6, enforcer 7, knight 6.3, hell knight 9.1, ogre
  14, dog 2.8, fiend 9.8 (less a jaw, where its rig has one), shambler 19.6, gremlin 1.4 kg. His lighter `h_*` weights bring the thrown
  heads (the props) nearer those (the ogre's 15 and the hell knight's 11 almost the same); the dog's 9 and the
  shambler's 65 stay well above a cut head's. Nothing changed in the limb shares or the scale (his `vr_limbs_mass_scale`
  is the default 1).

Tested (headless): a config at the old defaults (versions 94 / 57 / 34) takes every new value; one with its own values
keeps them (and takes the rest); a first start has them all; e1m1 loads clean.

In VR: torches' flames (denser, opaque), the messages' hologram higher over the gadget, a limb taken only from close,
heads popping a little less far away, thrown heads lighter; Retro Textures > All Categories shows the shipped look.

## VR Settings for first-time players (2026-10-07)

The author's spec: VR Settings (`menu_vr 0`) holds only what a new player sets, in sections, every row with a line of
help; everything else, and the rows that were there, live on Advanced VR Options' pages. The sections, in order:
Height Calibration, Hand Calibration, Locomotion, Comfort, Teleportation, Turning, Flashlight, Lighting, Weapons, Body,
Haptics, HUD, Sound, Display, Scaling, Graphics, Reset (SETTINGS.md, "The VR Settings menu", lists the rows). Above
them, *Search Settings* and *Advanced VR Options* (a link from Menu Detail: Advanced; at Standard the corner's Advanced
VR action, which raises Menu Detail: the pages' tree still runs through the link, so Search and the board paths find
every page). Not on it, as decided: damage tuning, a main-hand choice, the spectator camera, graphics presets; seated
mode is in the backlog.

Where the old rows went (the dumps before and after, `menu_vr dump` at Menu Detail: Developer, compared cvar by cvar:
every cvar reachable before still is): the Comfort preset, Turning (all four choices), Move Towards (with the moving
stick's hand), Teleport, Teleport Range, Stick Deadzone and Room Scale on Locomotion (whose empty Teleport header and
"Turning, Moving, Teleport: VR Settings" link they replace); Handedness on Body and Display; Gun Angle and Off Hand
Angle on Hand/Gun Calibration (as Main/Off Hand Pitch); Dominant Eye, Two-Handed and Two-Handed Hand-Off on Aiming;
Haptics on Immersion; Throw Speed and Throw Gravity on Carrying and Throwing (and on Throwing and Physics, a Developer
page); Force Grab on Force Grab; Headset Gamma also on Graphics. The links back to VR Settings for these ("Haptics: VR
Settings"...) are gone. Body and Display, Headset, Sound, Tips, Changed Settings, Run VR Calibration Again and the build
line are under Advanced VR Options > Setup (their pages' Back goes there); Official Campaigns is on Play (and under New
Game). The main menu: VR Calibration, VR Settings, Single Player... (the cursor still starts on Single Player).

New settings, each its default the old behaviour:

- **Wrappers** (`vr_menu_turning`, `vr_menu_move_towards`, `vr_menu_hands_x/y/z/pitch/yaw/roll`; not saved): shown as
  the settings they stand for are (synced as VR Settings is built and drawn), and set, they set them; their defaults
  are those settings' defaults, so Reset This Page and Reset All reset them. Turning Mode: smooth, or snap at the angle
  last used (45 at first); Snap Angle (30/45/90, `vr_snap_turn`) shows only with snap, Turn Speed only with smooth (it
  does nothing to snap turns). Move Towards: `vr_movement_mode` 1 head, and new 2 left hand, 3 right hand
  (vr_input.cpp VR_AdjustMove); 0, the moving stick's hand, shows as that hand. The hands: one mirrored set from the
  shipped calibration (0): forward, inward, up in cm (`vr_handcal_x/y/z`, the off hand mirroring: an edit sets
  `vr_handcal_off_mirror 1`), pitch (`vr_gunangle` and `vr_offhandpitch`, each its default plus it), yaw inward
  (`vr_gunyaw` plus it, `vr_offhandyaw` minus it), roll (`vr_handcal_roll`). *Reset Hand Offsets*: all of them, both
  hands, to the defaults.
- **Comfort vignette** (`vr_comfort_vignette` 0 off, 1 moving and turning, 2 moving only, 3 turning only;
  `vr_comfort_vignette_strength` 0.6): there was none (the old Comfort preset was turning, teleport and speed). The
  eyes' post-process darkens the edges (SlowLook.w, beside bullet time's vignette): the clear middle `mix(2, 0.1, s)` in
  r^2, black 0.6 further. s is the strength times how much the sticks move or turn you (the moving stick's push,
  smooth turning's), eased in over 0.08 s and out over 0.25 s; a snap turn gives it at once for 0.3 s. Room-scale
  steps and teleports never do. Measured on e1m1's start (left eye, mean luminance in the ring r^2 0.8-1.2, strength
  0.8): 8.6 standing, 1.8 walking; a snap with Turning only 1.9 against 12.3 a second later; with Moving only the
  same snap 6.4 and 6.4.
- **Vibration Strength** (`vr_haptics_strength` 1, 0 to 2): the OpenXR backend scales every vibration's amplitude;
  0 sends none. `vr_disablehaptics` stays (Immersion).
- **Reset Position** (`vr_recenter`): the body put under the head at the next frame where its box fits (the lean taken
  as a room-scale move), and the torso estimate started over from the head.
- **Reset All to Defaults** (press twice within 3 s): every archived `vr_*` cvar and the other cvars on the page
  (volume, music, default speed, anti-aliasing) to `default_string` (vr_defaults.cfg's), skipping server-locked ones.
  Kept: `*_version`, `vr_tips_seen`, `vr_setup_pending`, `vr_menu_positions`, `vr_menu_level`, `vr_enabled`,
  `vr_xr_runtime`. `developer 1` lists each one reset.

`vr_menu_level`'s shipped default is now 0 (Standard; vr_defaults.cfg had 2). No config migration: a saved value
stays (the author's config holds "2" and keeps it; a config that saved 2 because it was the default keeps it too).

Tests: synthetic clicks (`menu_vr 0 "<row>"`, `vr_mock_key rightarrow|enter`): Turning Mode Snap gives 45, Smooth 0,
Snap again after 30 gives 30; Move Towards Head > Left Hand (2) > Right Hand (3) > Head (1); mode 0 with Swap Stick
Functions shows Right Hand; Hand Yaw +1 gives `vr_gunyaw` 1 and `vr_offhandyaw` -5; Hand Forward +0.1 gives
`vr_handcal_x` -3.9 and mirror 1; Reset Hand Offsets restores them; Reset All: the first press only arms it ("Press
Again to Reset All"), the second resets (snap 90, volume 0.3, vignette, body mode 0, vibration 0.4, height back;
Menu Detail 2 and `vr_cfg_version` 94 kept). `vr_menu_path_check maps/vrcalibration.map`: 14 found, 0 missing (the
board's "VR Settings>Comfort" is now "VR Settings>Turning Mode", the map rebuilt).
## Monsters' heads weigh what a cut head weighs (2026-10-07)

The author: "Please bring the head's weights closer to the ragdoll's." A head cut off a ragdoll (vr_decap.qc,
vr_limbs.qc `VR_Limb_SetMass`) weighs its rig's head share (head and jaw: 7% in every rig) of its class's
`vr_ragdoll_<class>_mass`, times `vr_limbs_mass_scale`; a head thrown whole (`ThrowHead`, the `h_*` props) weighed its
Held Object Offsets Mass, set apart. Now one source: a monster's head prop has Mass **-1**, "Its Monster's" (the Mass
bar's leftmost step, shown only for such a head), which the engine reads as that head cut off its class's ragdoll
(`box3d::headPropMass`: `ragdollClasses` names each class's head model). It follows the ragdoll masses and Limb Weight
live (a change of either weighs the heads lying about again: `updateShapeGeneration` sums them), so the two can't
drift apart; Limb Weight 0 (cut pieces by their volume) leaves them estimated too. A number set in the slot is used as
set. The mummy throws the zombie's head model: weighed as the zombie's. The player's head (no ragdoll class) keeps 5.

| slot | head | class (kg) | before (v58) | now |
|---|---|---|---|---|
| 37 | h_guard | grunt 80 | 8 | 5.6 |
| 38 | h_dog | rottweiler 40 | 9 | 2.8 |
| 39 | h_mega | enforcer 100 | 9 | 7 |
| 40 | h_knight | knight 90 | 8 | 6.3 |
| 41 | h_hellkn | hell knight 130 | 11 | 9.1 |
| 42 | h_ogre | ogre 200 (and marksman) | 15 | 14 |
| 43 | h_wizard | scrag 40 | 10 | 2.8 |
| 44 | h_zombie | zombie 70 (mummy too) | 8 | 4.9 |
| 45 | h_shal | vore 160 | 10 | 11.2 |
| 46 | h_shams | shambler 280 | 65 | 19.6 |
| 47 | h_demon | fiend 140 | 18 | 9.8 |
| 7 | h_grem | gremlin 20 | 9 | 1.4 |
| 8 | h_scourg | centroid 180 | 50 | 12.6 |
| 36 | h_player | (none) | 5 | 5 |

`vr_props_version` 59: a slot still its model's at 58's default takes -1; a config's own weight is kept.
`vr_weight_table` prints each thing's one-hand throw limit (`throw1`, m/s).

What it changes in play: heads are thrown faster (the one-hand limit 28 m/s x (1.5 / kg)^0.9: a shambler's head about
2.8 m/s instead of 0.9, a grunt's 8 instead of 6, a dog's 16 instead of 7.4), weigh less in the hand and strike softer
(the weight's damage curve). A thrown head under `vr_decap_pop_thrown_mass` (4 kg: now the dog's, scrag's, gremlin's)
pops a head on a killing headshot only at `vr_decap_pop_thrown_light_chance` (0.25), not always. Bursting on a wall
goes by the hand's speed (7 m/s), not the mass: unchanged. Grabbing and holding: a lighter prop, nothing else.
## Relighting: See-Through Liquids in the game (2026-10-07)

Graphics > Relighting has **See-Through Liquids** (`vr_relight_seethrough` 1): id's maps relit in the game get
VisPatch's water-vised visibility, as `relight_maps.py --vis-dir` gives its copies. Before, the in-game relight kept the
patch only when it started from the script's copy, and maps the script never made stayed opaque.

- `Quake/vr/vr_relight_vis.cpp` ports `vis_maps.py` (read_vis_file, leaf_shape, vispatch, water_vis): the data file for
  the map's game (`<game>.vis` or `<game>/vispatch.dat`) is looked for in `tools/vispatch` of each game folder (the
  installer's `<QVR>\quakevr\tools\vispatch`, where the script looks too), then `QUAKEVR_VISPATCH`;
  `vr_relight_vispatch_dir` (testing) is then the only place. No Python, no subprocess.
- After `light`, `finishSlot` repacks the map with its own entities and the patch's visibility and leaves
  (`withEntities` takes the patch), the script's order (with_entities, then water_vise). A patch is used only if its
  leaves are the map's (checked at the start and on light's output); otherwise the console says so.
- The patch is in each map's hash (not in the page's settings text): toggling it, or adding the data files, relights
  id's maps in the next batch and skips the others. Off, the source is the map's own file, not the script's copy.
- The `.relight` note has a `liquids:` line (which are see-through, and the VisPatch file used); the start line says
  "see-through liquids after". The patch takes about a millisecond on the game's thread when light ends, so it has no
  progress stage of its own.
- Without the data the row is dimmed and inert, shown off, its help saying what to get and where (new menu
  `Item::unavailableBecause`, drawn and refused as a server-locked row). `vr_relight_vispatch` (Developer row "See-
  Through Data in the Console") lists the places and files and the map in play's liquids as loaded;
  `vr_relight_vispatch check <bsp>...` is `vis_maps.py --check`; `vr_relight_vispatch <game> <map> <in> <out>` patches
  a file as the relight does.

Verified: `vr_relight_vispatch` on the original e1m1, e1m2, e2m1, start, e4m1, hip1m1 and r1m1 (from the paks) gives
files byte-for-byte equal to `vis_maps.vispatch(with_entities(map, own entities))`; a patch for another map is refused.
In-game batches (e1m1, e1m2, e2m1, start; light.exe, Fast shadows): every result has the patch's lumps and the script's
water_vise leaves it unchanged (it is already the script's output for that light result); toggling off relit the 3 maps
opaque, the same setting again skipped all 3, back on relit them see-through. e1m2 loads with water and teleporters
see-through (`contentstransparent`), opaque with the original map; a screenshot over the pool shows its floor through
the water.

In VR: put the VisPatch files in `quakevr\tools\vispatch`, relight an episode (Many Maps) and look into the water of
e1m2's or start's pools; turn See-Through Liquids off and relight: opaque again. Without the files, the row is dimmed.
## Shadow casters drawn once per light (2026-10-07)

PROFILING_2026-10.md's decision 3. A dynamic light's six cube faces were drawn one at a time, each caster set up and
drawn once per face it reached (8 lights: 48 passes, about 7700 draw calls a frame in `combined`). Now
(`vr_shadow_layered 1`, default) each caster is drawn once per light into all its faces: each face a viewport, chosen
by the vertex shader (`GL_ARB_shader_viewport_layer_array`, or the AMD/NV one); the world and brush casters one
indirect multi-draw a light (a command's instances: its faces); the alias casters set up once a light (lerp,
matrices, bones, the head's shadow mesh), their faces chosen by the same tests as before, sorted by model so a
light's same models batch; holey skins a face at a time. Without the extension, or with `vr_shadow_layered 0`, the
old path. `vr_shadow_layered_check [n]` (Debug > Profiling and Memory > Check Layered Shadows) draws both ways in one
frame, times them and compares both atlases texel by texel: 0 texels differ on `combined`, `lights_32`, e1m1 (map
lights, the flashlight) and start's teleporter (lights through it). `combined` (his settings, median of 3): draw calls
7818 to 966, shadow CPU 1.69 to 1.03 ms, shadow GPU 6.7 to 0.6 ms, frame p50 22.7 to 16.6 ms; `lights_32` 3422 to
394, 0.41 to 0.34 ms, 0.48 to 0.09 ms. Details and the table: LIGHTING.md, "Layered shadow casters".

## The exe's icon and window title

The engine exe now carries the Quake VR: Unleashed logo as its icon (`Windows/QuakeVR.ico`: 16, 24, 32, 48, 64,
128 and 256, made by `Misc/quakevr/make_exe_icon.py` from the square logo: premultiplied Lanczos downscales, a mild
colour-only unsharp mask up to 64, alpha untouched; `Windows/QuakeSpasm.ico` stays as Ironwail's). `pl_win.c` loads it
at the system's large and small icon sizes and sets both on the window (class icons and `WM_SETICON`), so the title
bar and taskbar get the 16/24 frames instead of a shrunk 32. The version resource names "Quake VR: Unleashed" (Task
Manager shows it).

The window title is "Quake VR: Unleashed | by Vittorio Romeo" (`WINDOW_TITLE_STRING`), in game too: `cl_titlestats 1`
(the default) now only feeds the Steam status, `cl_titlestats 2` appends Ironwail's level stats after the title. The
console's corner banner says "Quake VR: Unleashed"; the `version` command and the startup log still list QuakeSpasm,
Ironwail and the Quake VR build; error dialogs are titled "Quake VR: Unleashed - Error".

In VR: start the game: the taskbar, alt-tab and the title bar show the logo and the title above.

## Retro particles' fill: skipping what is hidden (2026-10-07)

PROFILING_2026-10 decision item 4 (PERFORMANCE_BENCHMARK_20261005 item 1). The question was whether a faster path
for retro particles could look almost identical to the full-resolution one. It can, without a reduced resolution.

**Where the time goes** (particles_dense, his settings, 2048 eyes, frozen A/B in one process with `vr_profile`
GPU scopes). The full retro particle pass costs 10.1 ms. With the same quads but a constant colour (no texture,
no retro) it costs 8.1 ms. Its colour writes alone, without shading, cost 2.7 ms. Full retro shading without colour
writes costs 8.8 ms. Removing the palette changes nothing (it hides behind the writes). A single tap instead of
the block edges' four saves 2.5 ms of shading. About 8,300 particles, roughly 116 screens of overdraw: the pass is
bound by blending and shading about equally. Neither a cheaper shader alone (at most -25%) nor half resolution
would give a near-identical image. The blocks are 0.25 world units in each particle's own texture space (1-4 px
here, not aligned to the screen), and the half-res path already differs by 0.7-1.1 RGB8 on average.

**What changed** (`vr_gfx_gl.cpp` drawReverseOrder, `vr_particles.cpp` screenCover/reverseOrderThisFrame):
- The particles are composited in reverse order, "under" (`GL_ONE_MINUS_DST_ALPHA, GL_ONE`): the last drawn,
  on top, come first. They go into an RGBA16F target of their own, with the scene's depth/stencil attached, so
  the depth test is the same. They are drawn in 8 batches. After each batch, a full-screen pass marks the pixels
  that are already 99.9% opaque in the stencil's upper six bits (sky uses bit 0 and OIT bit 1). The marks use a
  rotating value, so there is no clear per draw. Later batches fail the stencil test there before any shading.
  Last, the target is blended over the scene in one pass.
- Compositing is associative, so the image is the same up to fp16 rounding order. The skipped particles are under
  at most 0.1% transmittance.
- The particles' quads take their vertex index reversed (`Reverse` uniform). The shaders are otherwise unchanged
  (same rasteriser, derivatives, depth test, soft fade, retro).
- Foveation: `foveated::shadeBoundAsScene` keeps variable-rate shading on the particle target, as on the scene.
  Without it the separate target cost +2.5 ms. The opaque marks and the final blend run at full rate.
- No glGet: the target is R_SetupGL's (VR_DrawSceneTranslucent), its size is `vid`, and the stencil is left as
  the others expect.
- Not used with MSAA scene targets or without a scene framebuffer. In those cases the old path runs.
- On per frame (first view) from `vr_particle_saturate_cover` 10 screens of on-screen particle squares, off below
  two thirds of that. The fixed cost is about 0.13 ms an eye: the clear, 7 marks and the blend. Break-even is about
  7 screens (dense1 6.6: 0.67 to 0.62 ms; floor smoke 8.5: 0.80 to 0.85 ms; light and explosion scenes under 1).

**Cvars**: `vr_particle_saturate` 1 (archived; Graphics > Models and Effects > Skip Hidden Particles; 0 is the
old path, for comparison). Test cvars, also on Debug > Profiling > Particles' Fill: `vr_particle_saturate_cover` 10,
`vr_particle_saturate_batches` 8 (6-10 measured flat), `vr_particle_saturate_growth` 1,
`vr_particle_saturate_opacity` 0.999 (1: no skipping), and Freeze Particles (`vr_particle_freeze`).

**Measured** (bench.sh, his settings, 2048, 3 alternating reps each, medians):

| scenario | vr particles GPU | GPU 3D | frame p50 |
|---|---|---|---|
| particles_dense, before | 10.00 ms | 11.15 | 13.17 |
| particles_dense, after | 5.26 ms | 6.30 | 11.11 |
| combined, before | 7.89 ms | 19.12 | 22.33 |
| combined, after | 4.65 ms | 14.83 | 17.27 |

On the frozen dense fixture (116 screens), in one process: 8.3-8.9 to 2.8 ms. dense4 (35 screens): 3.17 to
1.39 ms. dense2 (14 screens): 1.30 to 0.89 ms.

**Image** (frozen fixtures, both eyes at 2048, eyeshots with skipping vs without; pixels animated between two
captures without skipping are left out): the share of pixels over 2/255 is 0.0004% dense, 0.014% dense4,
0.032% smoke against the floor, 0.017% explosions and 0% blood in e1m1's dark. The mean is 0.007-0.09/255.
Nearly all differences are 1/255 of fp16 rounding (dense: 9% of pixels by 1). Threshold used: under 1% of
pixels differing by over 2/255. With the skipping off (opacity 1) the numbers are the same, so skipping adds
no error. Script: `scratch/vis.py`, `visan2.py` (not committed).

**To test in VR**: dense smoke and blood (rocket and grenade smoke in a corridor, gibs close by) with Skip Hidden
Particles on and off. They should look the same, with no seams at the foveation rings. Also the frame time in
heavy fights.
## Zancle's five concurrency defects fixed, vendored at 2f8a1ca5 (2026-10-07)

`ZANCLE_CONCURRENCY_REVIEW_2026-10-04.md`'s five defects, fixed on a Zancle branch for the author to merge,
`zancle-concurrency-fixes` (off `rebrand_to_zancle`'s `7bd385db`), one commit each, and vendored with
`zancle_vendor.py --update` (5 files changed, none added; no local changes). Details: `ZANCLE_REPORT.md` B9-B13.

| Commit | What | Item |
|---|---|---|
| `0e23061a5` | `ThreadPool`: every enqueue (posts, stop tasks, their reinsertion) aborts with `[[ZANCLE THREADPOOL FAILURE]]` when the queue cannot allocate, in every build, instead of losing the task (and hanging `ParallelForSlots` or the destructor) in Release | B9 (P2 for QVR) |
| `edf931db7` | `ThreadPool`'s constructor stops and joins the workers it started when a later one throws (exception builds); test utility `AlignedAllocationUtil` | B10 |
| `95b4fe0e3` | `Thread`: the entry block is freed when the callable's constructor throws | B11 |
| `1f86cc4c2` | `Thread::getId` is `{}` after `join`/`detach` | B12 |
| `2f8a1ca5b` | `parallelFor`'s queued helpers are bounded: `ParallelForSlots::outstandingHelpersPerWorker` (64) not yet finished per worker; past it a call runs on its caller | B13 |

Zancle's tests: one per fix, each failing (or hanging) before it but B9's (the runner has no death tests; the review's
fault-injected queue shows the abort instead of the loss). The base and system suites pass on MSYS2 clang64 Debug
with ASan/UBSan and UCRT64 Debug and Release (only `FileInputStream`'s temporary-file test fails, outside
`Concurrency`). B13 on the review's stress (100,000 calls, seven parked workers): 107.38 MiB of queue growth and a
54 ms drain before, 0.05 MiB and 0.18 ms after; calls on an idle pool are unchanged (2.3 and ~28 us medians).

Tested (headless): Release and Debug (`QVR_ZANCLE_DEBUG`) builds; e1m1 smoke; warden, ad_grendel and e1m1 loads with
`vr_hull_stats`, liquid, decal and hit-model hashes the same with `vr_jobs_parallel 0` (Release), AO bakes; a relight batch
started and cancelled; `bench.sh --validate` on load_reloads, load_warden and load_ad_grendel; `vr_physics_mtbench`
the same hash at every worker count. The Debug build exits with code 42 a few frames into warden or ad_grendel, with
no message or crash report: a pre-change Debug build (`12d136c0`'s) does the same, so it is not these changes (e1m1
and e1m2 run clean in Debug).

Nothing to see in VR: the changes are failure paths (out of memory, a throwing constructor) and a bound that
ordinary frames never reach.
## mimalloc: the process's heap (2026-10-07)

PROFILING_2026-10.md's item 6 (the load's workers contending on the C runtime's heap): mimalloc v3.5.4 (MIT,
`Quake/vr/external/mimalloc`, its README: why v3) now serves malloc/free, calloc/realloc, `_msize`, `_expand`,
`_recalloc`, the whole `_aligned_*` family, strdup/wcsdup, and through them operator new/delete (vr_alloccount.cpp's
call malloc/free and `_aligned_malloc`/`_aligned_free`), for the engine and every library linked into the executable
(the codecs, miniz, stb_image, lodepng, json.c, Box3D's callbacks, Zancle).

How (`Quake/vr/vr_crtheap.c`): the engine stays on the DLL C runtime (/MD; the codec libraries, SDL2, libcurl, OpenXR
and Steam Audio are built for it), where every call goes through an import pointer (`__imp_malloc`, the headers declare
the functions dllimport). The file defines those functions and their import pointers itself; the linker takes an
object's definitions over ucrt.lib's, so the executable imports no heap function from the C runtime at all
(`llvm-readobj --coff-imports`: before, malloc, free, calloc, realloc, `_aligned_malloc`, `_aligned_free`, `_strdup`;
after, none). Upstream's two Windows overrides were set aside: the redirection DLL (`mimalloc-redirect.dll`, a closed
binary that patches ucrtbase for the whole process, shipped beside a mimalloc DLL) and the static C runtime (/MT,
which the prebuilt codec libraries and the FILE/heap sharing with the DLLs rule out).

Cross-heap safety: the DLLs (SDL2, libcurl, zlib, the OpenXR loader, Steam Audio) keep ucrtbase's heap. A block the C
runtime allocated itself and the engine frees (`_fullpath(NULL)`, `_getcwd(NULL)`, `_dupenv_s`) is sent back to the C
runtime: free, realloc, `_msize`, `_aligned_free` and the rest look the pointer up in mimalloc's page map
(`mi_is_in_heap_region`) and give any other to ucrtbase's own function, counted (`vr_heap`: 0 in every run here). The
other way, a mimalloc block freed by a DLL, cannot be caught: audited, none happens (SDL memory goes back with
`SDL_free`, curl's with its own calls and `curl_slist_free_all`, Steam Audio has no allocator callbacks set, OpenXR
fills the engine's buffers; no iostreams or locale facets, whose objects msvcp140 would delete).

Switches: `-nomimalloc` on the command line or `QVR_MIMALLOC=0` in the environment (read at the first allocation)
forwards every call to ucrtbase (an A/B in one build); MSBuild `-p:QVR_MIMALLOC=false` / CMake `-DQVR_MIMALLOC=OFF`
build without it (the import table has the C runtime's heap again). x64 only in MSBuild (32-bit's import names
differ). CMake elsewhere: mimalloc's own `MI_MALLOC_OVERRIDE` (Linux; not macOS); not built here (configure checked on
Windows only). mimalloc's environment options work as upstream documents (`MIMALLOC_SHOW_STATS=1`, `MIMALLOC_VERBOSE=1`,
`MIMALLOC_PURGE_DELAY=...`).

Console (Debug > Profiling and Memory > Memory): `vr_heap` (the heap, mimalloc's messages and errors, the working set
and commit, mimalloc's reserved/committed memory, threads, arenas), `vr_heap stats` (mimalloc's table and options),
`vr_heap test` (PASS: 11 kinds of block are mimalloc's, contents, zeroing, alignment, `realloc(p, 0)` frees, the C
runtime's own blocks go back to it), `vr_heap stress [threads] [ms]` (threads allocating and freeing, a quarter freed
by another thread, each block checked), `vr_heap collect` (`mi_collect(true)`).

Measured (Release, the kit's bench, realtime and exclusive; the machine shared with other agents' builds, so the
medians of 7 runs for warden and ad_grendel: three sets, two of them interleaved base/mimalloc; 3 for e1m1), the load's
total, ms:

| load | C runtime | mimalloc (1 s purge) | mimalloc (purge at once, the default) |
|---|---|---|---|
| warden cold | 2252 | 1356 (-40%) | 1613 (-28%, n=3) |
| warden warm | 2764 | 1293 (-53%) | 1547 (-44%, n=3) |
| ad_grendel cold | 1568 | 1435 (-8%) | 1440 (-8%, n=3) |
| ad_grendel warm | 1765 | 977 (-45%) | 1200 (-32%, n=3) |
| e1m1 cold / warm / restart | 896 / 245 / 226 | 727 / 216 / 197 | 813 / 222 / 218 |

`combined` (steady frames): CPU a frame 5.7 ms (5.66-8.02) before, 5.6-7.2 after: within the noise; frame p99 31-37 ms
both. `vr_heap stress` (the allocator alone, each block checked): 8 threads 85 million operations a second against
the C runtime's 35 (-nomimalloc, same build); 1 thread 29 against 18.

Memory (realtime, `vr_memstats` 300 frames after each load of e1m1, warden, ad_grendel, e1m1 again; working set MB):
C runtime 323 / 481 / 914 / 860, peak 1302; mimalloc with its default 1 s purge delay 623 / 1645 / 1128 / 954, peak
1645 (freed pages wait for the delay and then for the next allocation on the same heap: the pool's workers go idle
holding them; MIMALLOC_PAGE_FULL_RETAIN=0 and a 10 ms delay barely helped); purge at once 388 / 738 / 660 / 554, peak
1116. So the default is purge at once (vr_crtheap.c sets mimalloc's default before it reads its options;
MIMALLOC_PURGE_DELAY overrides it), `vr_heap_purge_delay` (Debug > Memory, Heap: Purge Delay) changes it in game:
1000 for the quickest loads at 0.3-1 GB more held.

Checked: the executable's imports (none of the C runtime's heap functions; a `QVR_MIMALLOC=false` build has them
again); `vr_heap test` and `vr_alloc_test` PASS with mimalloc, with -nomimalloc and in Debug (MI_DEBUG 2: mimalloc's
asserts, double and invalid free checks); `vr_heap stress 16 3000` PASS; a run through e1m1, the Map Library's index
fetched afresh (`maps_fetch force`: libcurl, 18 MB in 10 calls, parsed), warden, ad_grendel, changelevels: 0 calls on
the C runtime's blocks besides the test's 2, no mimalloc warnings or errors; `bench.sh --validate --scenarios
loading,gore`: 18 scenarios, 0 failing. Debug: e1m1, e1m2, vrfiringrange and the stress clean; `map warden` in
Debug ends with exit 42 (an SDL assertion, SDL_ASSERT=abort) with mimalloc and with -nomimalloc alike: not the heap
(the R_AddBModelCall `num_instances > 0` assertion seen in Debug runs, likely).

In VR:
- [ ] Play a while (a few maps, gore, the Map Library): no crash, nothing odd; Debug > Memory > Heap Allocator says
      mimalloc 3.5.4, 0 calls on the C runtime's blocks, 0 errors.
- [ ] Loads of warden and ad_grendel feel quicker; the frame rate in play unchanged.
## The menus in Quake VR's colours: the banner and blood red (2026-10-07)

The menus now carry the project's branding: Quake's vertical plaque is replaced by the vertical "Quake VR: Unleashed"
logo, and the menus' browns are turned blood red (Menu Settings > **Blood Red Menus**, `vr_menu_recolor` 1).

- **The banner** (`Quake/vr/vr_menubrand.cpp`, `quakevr/gfx/vr/menu_banner.png`): the logo's pixels and alpha exactly
  as authored (667 x 2000), centred on a transparent 768 x 2048 canvas (`Misc/quakevr/make_menu_banner.py <logo>`:
  the engine's mipmaps halve two texels at a time, so every level must have even sides; the transparent texels take
  the nearest opaque colour so the small levels keep clean edges). It is mipmapped and smooth (`Draw_LoadImagePic`,
  an image file's trilinear filter), and only its opaque rectangle (found from the alpha) is drawn.
  - Flat menus (and the VR menus with VR Menu Style off): where the plaque was (`M_DrawPlaque` replaces the nine
    `M_DrawTransPic (16, 4, qplaque)` calls and the VR pages' one), as tall (144), 48 wide, centred at x 30 so its
    right edge stops at the main menu's cursor (x 54). id's qplaque is drawn only if the image is missing.
  - VR Menu Style: the plaque stays left out (the rows reach its column), so the banner stands in the corner buttons'
    column, 8 true pixels under them, 216 true pixels tall (the plaque at the shipped row spacing), the same size and
    place on every menu (`VR_MenuDrawBannerColumn`, M_Draw before the page; `menuui::toolbarLeft` added).
- **Blood red** (`gl_shaders.h` gui shader, `MenuRecolor`; `gl_draw.c` `Draw_SetMenuRecolor`): a true hue change in
  Oklab (lightness, chroma, hue): hues 40 to 115 degrees (Quake's browns, tans, oranges, yellows; fading out to 10 and
  150) rotate towards the target, lightness unchanged (highlights stay bright, shadows dark, the contrast the same),
  chroma times `vr_menu_recolor_saturation` (1.25), less only where sRGB has no such red. Greys, blues, purples, greens
  and reds stay as they are. M_Draw turns it on for what it draws and back after (nested draws keep it); the banner is
  drawn with it off. The game, the HUD and the console under the menu are untouched: before/after screenshots differ
  only in the menu's pixels (flat main menu: 42k pixels, all in the menu; HUD rows identical; VR eye likewise).
- Settings (Menu Settings): `vr_menu_recolor` 1, `vr_menu_recolor_strength` 1 (0..1), `vr_menu_recolor_hue` 0 (HSV
  degrees: 0 is the logo's own red, its median Oklab hue 29; 345 crimson; -1 the player's hue),
  `vr_menu_recolor_saturation` 1.25 (0.5..3).

## Zero-instance water calls (2026-10-07)

Debug builds stopped a few frames into warden and ad_grendel (exit 42 with `SDL_ASSERT=abort`; a modal "Assertion
failure at R_AddBModelCall ... 'num_instances > 0'" without it). The cause was not the layered shadow casters: the
teleporter-reach change (47ff1eb5c, 2026-10-05) made the brush models' batches count an entity's instances from
`bmodel_portal_counts` (2 for a model drawn again through a teleporter), and R_DrawBrushModels_Water got the same line,
but the water pass never fills that array: it read the last brush pass's counts, 0 past that pass's entity count
(warden's lit water: a call with no instances, its remap's instance `num_instances - 1` underflowing) or 2 for a
copied model (a batch one instance too many). Release drew such a batch's water with the wrong instances or not at
all. The water pass counts one instance an entity again (Ironwail's loop), and R_AddBModelCall returns on an empty
batch after its assert. Checked: warden in Debug `exit=42` before, `exit=0` after (and ad_grendel, start, e1m1 with
the flashlight); `vr_shadow_layered_check 5` on warden 0 texels differ in both atlases. TESTING.md, "Debug build
assertions", has the run.
## Cheats and Recording (2026-10-07)

A Debug > Cheats and Recording page (Developer menu level; `menu_vr 150`; `Quake/vr/vr_menu_cheats.inc`): Quake's
cheats and what sets a scene up for footage, existing commands wherever there were some.

**Rows.** The cheats on, at the top (`cheats::stateLine`). Cheats: God, Noclip, Notarget, Fly (Quake's commands), All
Weapons, Ammo and Keys (`impulse 9`), Full Health and Armour (new `impulse 194`: 100 health, red armour 200), Infinite
Ammo (new `vr_cheat_ammo`: QC `VR_Cheat_AmmoFrame` in PlayerPostThink keeps impulse 9's amounts and the hands'
magazines full; not an enemy gun's magazine, not the chainsaw's fuel). Powerups: new impulses 190 to 193 switch Quad,
Pentagram, Ring, Biosuit on for an hour, or off at once (`VR_Cheat_TogglePowerup`: no "burned out" warning); Armagon's
wetsuit and empathy shields (impulses 200, 201, 30 s). Monsters: Kill All Monsters (`impulse 205`, Genocide), Freeze
Monsters (new `vr_freeze_monsters`), Skill (the `skill` cvar). Scene: Quick Save/Load (`save quick`), Save/Load the
Scene (`save scene`: a slot quick saves don't touch), Restart the Map; Clean Up (new `vr_scene_count`,
`vr_scene_clean gibs|corpses|props|fires|decals|wounds|effects|all`); Put a Monster or Prop Ahead (`impulse 241`, the
Thing chosen on Debug > Tests: linked, not repeated), Spawn Pickup Weapons. Time: `vr_slowmo 1/0.5/0.25/0.1`, Pause the
World (`sv_freezenonclients`: everything but the player stops; he moves). Clean Shot: Hide (new `vr_shot_hide`), a link
to Graphics > Recording (window view, spectator camera, slow motion, highlights) and Mark This Moment.

**How.**
- `vr_scene_clean` / `vr_scene_count` (`vr_cheats.cpp`): client commands like `god` (allowed in `sv_user.c`'s list,
  refused in deathmatch). The client's parts are cleared where typed: decals (`decals::clear`, with the gore's pools),
  wounds (`wounds::clear`, `bodyblood::clear`), effects (particles, casings, explosion debris, smoke, fire particles,
  weapon effects, shock arcs, dynamic lights). The server's parts are QC's `VR_Scene_Clean` (`QC/vr_cheats.qc`), which
  removes each thing as the game does: a fire through `VR_Burn_Out`, a head's flies silenced, a small gib through
  `VR_SmallGib_Remove`, a player's old body (`bodyque`, reused) emptied; Box3D drops the bodies and ragdolls of freed
  edicts. What a hand holds is kept. Gibs: `.vr_gib` (gibs, heads, cut heads), `vr_smallgib` (brains too), `vr_limb`,
  the stumps' fountains. Corpses: dead `FL_MONSTER`s and `bodyque`. Props: loose things (rigid, tossed, thrown weapons,
  crate pieces, backpacks) made since the map loaded: the new field `.vr_born` is set by the engine in `ED_Alloc`
  (`VR_OnEdictAlloc`); the map's entities and what is placed as it loads (crates, rocks, bricks) are born by time 1.5;
  the field is saved, so a loaded game knows too. Fires: `vr_burn`.
- `vr_freeze_monsters`: `SV_Physics` skips a living monster's physics and think (`VR_MonsterFrozen`), its `nextthink`
  pushed on by the frame, so it resumes where it stopped (a dog frozen mid-leap stays in the air). A blow still hurts
  and kills it.
- Noclip in the headset (`SV_NoclipMove`, `VR_NoclipAngles`): Quake flew along `.v_angle`, which is the aiming hand
  here (the flight went where the gun pointed, pitched down by the grip angle). With tracked hands it now goes along
  the head's yaw, level, and up or down by the stick's upmove (the moving hand's pitch), as walking and swimming steer.
- `vr_shot_hide` (bits): 1 the gadget (hidden as when inactive: no screen, hologram or glow), 2 the hands, weapons,
  buttons, flashlight and muzzle flashes, 4 the body, pauldrons, holsters and pouch (`hiddenForShot`, vr_view.cpp).
  Only their drawing; they work on.

**Tests (headless, e1m1).** Counts after a gibbed grunt, a corpse, a crate, a smashed crate, a fire and gore: gibs 4
(heads 1), limbs 4, small gibs 12, corpses 1, props 18, fires 1, decals 392, wound masks 5; `gibs` removed 20,
`corpses` 1, `props` 18, `fires` 1, `decals wounds effects` left decals 0, masks 0, particles 11. Map start: every
server count 0 (the map's crates and debris not counted as props). An ogre's ragdoll removed (1 to 0 ragdolls, Box3D
150 to 137 bodies); a gib held in the off hand kept ("1 held in a hand kept"); a wound on you after `wounds` paints
again. Cheats: godmode/notarget/flymode ON; impulse 194 armour 200 at 0.8; impulses 190-193 set and clear the four
item bits; `vr_cheat_ammo 1` puts `give s 7` back to 100. Freeze: a dog's origin unchanged over 60 frames, moving
again once thawed; Pause the World: the dog still while the player flies 64 units. Noclip: stick forward with the head
turned 90 degrees flies along the head (-x), level with the moving hand at 70 degrees. Save/Load the Scene: a corpse
cleaned comes back on load (and the props' `.vr_born` with it). `vr_shot_hide 1`: `vr_gadget_info` "no gadget", back
at 0; 2, 4, 7 hide the hands, the body, all (screenshots). Menu path check: 0 missing.

**Check in the headset.**
- [ ] Noclip: the stick flies you where you look (level), the moving hand's pitch takes you up or down; Fly the same
      with walls.
- [ ] Freeze Monsters mid-fight, walk round a frozen monster, unfreeze: it carries on.
- [ ] Pause the World: you move and turn freely; your weapons may not fire while paused (the game's clock stands).
- [ ] Each Clean Up row on a messy fight scene; Everything leaves the map as loaded (its crates and rocks kept).
- [ ] Hide: Gadget, Hands, Body as named, on the spectator camera too.

## The exe icons: the "QVR:U" logo (2026-10-07)

Vittorio's icon variation of the logo (the emblem with "QVR:U" under it, transparent background) is the icon of both
exes: `docs/images/quakevr-unleashed-icon.webp` (the original, 1254x1254) made into the game's `Windows/QuakeVR.ico`
(the exe resource, and the window's large and small icon through pl_win.c) and the installer's
`Installer/src/QuakeVR.Installer/Assets/app.ico` (ApplicationIcon and MainWindow's Icon) by
`python Misc/quakevr/make_exe_icon.py docs/images/quakevr-unleashed-icon.webp <out.ico>` (C:/Python313 has Pillow
with WebP). The two .ico files are byte-identical. The script now takes `--sizes` and makes 16, 20, 24, 32, 40, 48, 64,
96, 128 and 256 by default (20, 40 and 96 for 125%/150%/200% DPI), the whole logo at each size with the same mild
unsharp mask up to 64. The installer's in-page logos (logo_square.png, logo_wide.png) are unchanged.
Checked: the icon groups extracted from the Release ironwail.exe and QuakeVR-Setup.exe are byte-identical to the
.ico files at all ten sizes. Old and new side by side at 16/24/32/48, 1x and 4x: "QVR:U" reads at 48 and still reads
at 32 (the old "QUAKE VR" was barely legible at 32 and "UNLEASHED" never); at 24 and 16 the text is a smudge in both,
the emblem carries the icon.

## Hand sounds from the hand (AUDIO_REVIEW.md row 1, 2026-10-07)

Only the two weapon channels (1 and 5) played from the hand; everything else a hand does (a parry, a bash, a reload,
drawing and holstering, a dry click, the grenade pouch, a chainsaw's cord, a carry's swish) played from the middle of
the head on `CHAN_AUTO`/`CHAN_BODY`.

- **Engine.** Two new channels, `SND_CHAN_HAND` 8 and `SND_CHAN_HAND2` 9 (`protocol.h`): "any free one" as channel 0
  (`S_AUTOCHANNEL`, `snd_dma.c`: they never cut a sound off), played from the main and the off hand (`vr_audio.cpp`
  `handOf`; the hand itself, `handSoundAt`, where the weapon channels keep the muzzle), in the Steam Audio voices and
  in Quake's own panning alike (`VR_SndSpatialize`), with `vr_snd_hands` (on) as before. `SV_StartSound` takes channels
  0 to 9 (`SND_MAX_CHANNEL`); 8 and up go with `SND_LARGEENTITY` (the channel a byte: FitzQuake's message carried it,
  and the old `SND_LARGESOUND` choice for `channel >= 8` did not). NetQuake's protocol (3 bits) sends them as channel 0.
  Old demos: unchanged messages, parsed as before (`demo1` played); a demo recorded with them plays them back.
- **QC.** `CHAN_HAND`, `CHAN_HAND2` (`defs.qc`) through `VRGetHandChannel(xHand)` (`vr_util.qc`). Converted:
  `combat.qc` a corpse struck (VR_Corpse blow: fisthit/axhit2), the one-handed parry's ring (`vr/parry`, `axhit2`;
  the old `axhit2`/`tink1`); `vr_melee.qc` the chainsaw's swing (was `CHAN_AUTO`), the counter's opening chime (its
  parrying hand; crossed arms stay in the middle), a one-handed bash/shove (`vr/bash`, `bash_parry`, `shove`, or the
  old `axhit2`/`fisthit` + `dland2`; both hands stay in the middle); `weapons.qc` the axe's hit on a body and on a wall,
  the dry click (`gunclick`), the lightning gun's `lstart`, the lava nailgun's `snail`, the throw's swish
  (DropWeaponInHandScaled), holstering (`holster1`: VRPutWeaponInHolster now takes the hand; the quick-slot path's own
  second `holster1` dropped), reload and unload (`reload1`), the hand-off and the hand switch (`holster0`, `sword2`,
  from the hand taking it), taking a carried gun back and drawing from a holster (`holster0`); `vr_carry.qc` a torch
  taken up (`pommel2`), the carry swish, the second hand joining a carry; `vr_grapple.qc` the hook home in the gun
  (`tink1`), the quick-release and detach clicks, the rope's twang (`grapple_taut`), the reel's and unreel's clicks;
  `vr_juice.qc` a deflection (`tink1`, `axhit2`); `vr_enemyguns.qc` a burst's rounds and the dry click;
  `vr_grenade.qc` the pouch (taking one out, putting one back); `vr_chainsaw.qc` the cord's pulls (weak, failed) and
  the dry click. Left in the middle of the head: gasps, the counter's landing, the headbutt, the Quad's sound, item
  pickups (row 13).
- **Debug.** `snd_show 2` prints each channel's place (`at x y z`) and, for a hand's, the hand and how far the sound is
  from it and from the head. `vr_snd_test hands` checks channels 8 and 9 too (from the hand, panned to its side).

Checked (`run.sh -Sound`, `snd_show 2`): the off hand's empty shotgun clicks on channel 9 from the off hand
(`at 471 -342 102 | off hand 471 -342 102 (0.0 off)`); the main hand reaching a holster reloads on channel 8 from it
(`reload1.wav at 491 -348 99 | main hand ... (0.0 off; the head 23.5)`); `vr_snd_test hands` 4 PASS.

In VR: parry with one hand, reload at a holster, holster and draw, dry-fire an empty gun, pull a grenade from the pouch:
each should sound from that hand (turn your head: it stays with the hand).

## Ragdoll impact sounds (AUDIO_REVIEW.md row 2, 2026-10-07)

Bodies fell, tumbled down stairs, were flung and hit by props in silence: Box3D's hit events played only for props
(`vr_box3d.cpp` soundHits). Now a ragdoll's parts and a pushable corpse (`vr_corpse_collide` 2/4: its body now asks
for hit events) knock too, through the props' path (`physsound::hit` with `body`): as flesh (`vr/phys/flesh_*`,
`squish*`; the light set, `vr/squish_s*`, for a part under 1.5 kg), the part's own mass and the contact's speed
(a pelvis of 23 kg lands heavy, a forearm medium, a hand light), from the contact. Per body, not per part: the
frame's loudest part plays, and a body knocks at most every two `vr_physsound_interval` (0.24 s; a hit twice as loud
sooner; the bounce rule as props). Two bodies meeting knock once (the lower-numbered one's), not once each. A prop
hitting a body: the prop's knock and the body's thud. Living monsters knocked down (their ragdoll) likewise.
Detached limbs and heads were already props of flesh (`vr_limb`, heads, small gibs) and keep their knocks.

- `vr_physsound_bodies` (1; 0 off, up to 2): their volume. `vr_physsound_body_min_speed` (2 m/s): a body's slowest
  hit that knocks (props' is 1.5): a settling pile's twitches stay silent. Menu: Carrying > Physics Sounds, **Bodies**
  and **Quietest Body Knock**.
- Test: `Misc/quakevr/ragdoll/ragdoll_sound_test.sh [flat stairs blast pile]`.

Checked (`vr_debug_physsound 1`): a grunt dying on e1m1's floor knocks twice (0.9 kg at 6.5 m/s, 6.8 kg at 9.9);
down vrclimb's stairs 5 knocks over 2 s (0.16-0.42); blasted, 3; a pile of 6 dying on one spot 19 knocks while they
fall (2-6 s), then none in the next 10 s of settling (0 body knocks from 6 to 16 s); cut limbs knock as `vr_limb`
flesh (2-8 kg, medium) and small gibs as light squishes.

In VR: kill a grunt on stairs, throw a body (grab a limb), drop a crate on a corpse, blow up a group: thuds, not
silence; a pile left alone goes quiet.

## Pitch from QuakeC, and pitch variation (AUDIO_REVIEW.md row 3, 2026-10-07)

QC couldn't play a sound at a pitch, so the most frequent sounds (one recording each: `fisthit`, the swings, `tink1`,
`pkup`, the holster and reload clicks, the squishes) sounded identical every time.

- **Engine.** `sound()` takes a sixth argument, the pitch in percent (100 as recorded; DP's and FTE's `speedpct`:
  `PF_sound`, `qcvm->argc > 5`), sent as `SND_PITCH` (`protocol.h`, bit 5: DarkPlaces' `SND_SPEEDUSHORT4000`, a short
  of the rate * 4000 after the attenuation, 0.25 to 4), only when it isn't 1 and not in NetQuake's protocol: other
  sounds' messages are unchanged, and old demos play as before. `SV_WriteSound` writes the message for
  `SV_StartSound`/`SV_StartSoundPitch` and the physics sounds (`vr_physsound.cpp` emit, which wrote its own copy). The
  client keeps it on the channel (`channel_t.pitch`, `S_StartSoundPitch`, `S_CHANPITCH`): Quake's mixer paints such a
  channel at that rate (`SND_PaintChannelRate`, as slow motion, the two multiplied), the voices read at it (times the
  Doppler and slow motion: `VoiceInput.pitch`); `snd_show 2` prints each channel's pitch.
- **QC.** `sound_pitch(e, chan, samp, vol, atten, pct)` (`frikbot/bot.qc`, beside `sound`: the bots hear it too) and
  `VR_SoundVaried(e, chan, samp, vol, atten)` (`vr_util.qc`): at 100 +/- `vr_snd_pitch_jitter` percent at random.
  Every call playing `fisthit`, `axhit1/2`, `ax1`, `knight/sword1/2`, `weapons/tink1`, `weapons/pkup`,
  `holster0/1`, `reload1`, `vr/headshot`, a squish (`VR_Gib_SquishSound`, `squish*`), a melee hit
  (`VR_Melee_HitSoundOn`) or a footstep (`misc/foot1..7`) now goes through it (84 calls), and the swing's whoosh
  (`VR_Melee_Whoosh`).
- **Engine-side sounds:** the physics knocks (props' and bodies') and the casings' tinks (`vr_shells.cpp`, client) vary
  by the same setting.
- `vr_snd_pitch_jitter` (4; percent, 0 off, up to 25): Audio > Movement and Nearness, **Pitch Variation**.

Checked (`-Sound`, `snd_show 2`): walking e1m1, the footsteps at 0.965 to 1.025, the shells' tinks 0.980 to 1.034, a
body's and its gear's knocks 0.968 to 1.031; other sounds (`guncock`, the ambiences) 1.000.

In VR: punch a wall or a grunt a few times, swing a sword in the air: each a little different; Audio > Pitch Variation
0 to compare.
## VR Settings follow-ups: no Advanced or Search rows, Reset All keeps calibration, the menus' red (2026-10-07)

Vittorio's decisions after the VR Settings revamp and the menu colours:

- **No *Search Settings* or *Advanced VR Options* rows** at the top of VR Settings. The main menu's *Advanced VR* row
  and the corner's *Advanced VR* and *Search* buttons open them. Search, the boards' menu paths (`menu::pathTo`) and
  `menu_vr dump` walked the links from VR Settings only, so the Advanced pages would have dropped out: they now walk
  from both roots (`menuRoots`: VR Settings, then Advanced VR Options). Search shows "Advanced VR Options > Combat" as
  before (the entries' paths lost their "VR Settings > "; recent searches saved under the old path are moved over as
  `search_recent.txt` is read); board paths read "Advanced VR > Movement > Locomotion" (what the main menu row and the
  corner button say), without a Menu Detail note for Advanced, which the row raises itself. `vr_menu_path_check
  maps/vrcalibration.map`: 14 found, 0 missing. Back from the Advanced VR Options opened from the main menu's row goes
  back to the main menu (VR Settings no longer links them); from anywhere else, to VR Settings as before (`menu_vr 1`,
  the corner).
- **Reset All to Defaults keeps the calibration** (`keptOnResetAll`): `vr_height_calibration`, `vr_floor_offset`,
  `vr_bodycal_*` but `vr_bodycal_preview` (the measurements, seated, Undo), `vr_body_tweak_*`, `vr_body_arm_length`,
  `vr_body_eye_forward`/`_up`, `vr_body_torso_back`, `vr_handcal_*`, `vr_gunangle`/`vr_gunyaw`,
  `vr_offhandpitch`/`vr_offhandyaw` (Reset Hand Offsets resets the hands). World Scale and the body preview are choices:
  reset. Test: those set off their defaults, Reset All pressed twice by `vr_mock_key enter`: all kept, `vr_world_scale`,
  `vr_bodycal_preview`, `vr_turn_speed`, `vr_menu_recolor_hue` back to their defaults.
- **The menus' red as Vittorio has it:** `vr_menu_recolor_saturation` 3 (was 1.25; hue 0 and strength 1 were the
  defaults already). Config 96 moves a config still at 1.25; a custom value (1.8) stays.
- **Docs:** README, FEATURES, SETTINGS, INSTALL, RELIGHTING and the vr-port notes no longer send players to rows that
  moved off VR Settings (Search Settings, Handedness, Gun Angle, Tips, Sound, Changed Settings, Body Calibration) or
  through "VR Settings > Advanced VR Options". RELIGHTING.md said VisPatch doesn't vis lava: it does (`vis_maps.py
  --check` on the relit maps: 33 of 73 vised for lava, start among them), so `r_lavaalpha 0.9` shows it a little
  through; GRAPHICS.md's "lava is opaque by default (`r_lavaalpha 1`)" was stale too.
## Map tips: the review's open decisions (2026-10-07)

Vittorio's calls on what "Review of the map tips" left open, one commit each.

**The tips seen are a file** (`Quake/vr/vr_tips.cpp`, `SeenList`): `<gamedir>/tips_seen.txt`, one key a line, sorted,
no limit (the `vr_tips_seen` cvar was read back from the config cut at 1023 characters, some 80 keys). Read the first
time a tip asks in a game folder (another `game`: read again); written whole through `tips_seen.txt.tmp` and a rename
as a key is added; Show Tips Again (`vr_tips_reset`) writes it empty. **Migration**: an older config still sets
`vr_tips_seen`; its keys, and the config line's whole value read from `ironwail.cfg` (the keys past the cut), are added
to the file once, and the cvar is emptied. The cvar stays registered for that, no longer archived: the next config
written leaves it out. Reset All keeps the file (it was kept as a cvar). Proved headless: a config with a 2717-character
`vr_tips_seen` (150 keys, `vrstart:testwelcome` last, past the cut) gives `moved to tips_seen.txt (150 keys new, 150 in
all)`, the file's 150 lines and `testwelcome ... seen`; the next run (the baseline config, no `vr_tips_seen`) reads
it back seen, `vr_tips_reset` makes it unseen, it shows again (`tips: "testwelcome"`) and the file is that one key.

**A tip follows only its `target`** (`QC/vr_tips.qc`): `targetname` is only the tip's name (its seen key when it has
no `tipname`), kept in the new field `tip_targetname` and cleared from `.targetname` as the tip spawns, so no
`find(world, targetname, ...)` (a teleporter's destination, a monster's path, a trigger's targets) ever finds a tip.
A tip with no `target` whose old targetname another entity has (made for the old rule) stays at its own origin and
says so in the map's first frame with `developer 1`: `func_vr_tip at -300 -300 300: its targetname "dest1" names a
info_teleport_destination, which it no longer follows ...: set its target to "dest1" to follow it`. FGD: `target` is a
`target_destination` (TrenchBroom draws the link), `targetname` has the tip's own help. `vrstart.ent` gets a second
example tip, `testbutton`, following the Snap Turn (30) button by its `target`. Proved headless on `tiptest` (a copy
of vrstart, its tips in the `.ent`: a tip named `dest1` placed before the `info_teleport_destination dest1`, the
first changelevel trigger made a `trigger_teleport` to it; a tip named like a health box; a tip whose `target` names
the other health box): before, the teleporter put the player at the tip (`Player pos: (-300 -300 300)`) and the
named tips followed entities 13 and 53; after, the player lands at the destination (`64 448 307`), both tips stay
at their origins with the warning, and the `target` tip still `follows entity 52`.

**`tip_delay`: 0 is at once, -1 the player's** (`QC/vr_tips.qc`): the field is a string now (`.string tip_delay`, read
with `stof`, declared in `builtins.qc` as FRIK_FILE's #81), so no key differs from 0: no key, an empty one or below 0
leave the engine's -1 (`vr_tips_delay`, as before); 0 or more is the tip's own delay, 0 showing it the frame he comes
near. The FGD's default is -1. A map that set `tip_delay 0` meaning the player's setting now shows that tip at once.
`vr_tips_test list` prints a tip's own delay (`d0: at 64 448 600, range 64, delay 0 s, any angle`). Proved headless
on `tiptest` with `vr_tips_delay 3`, a tip with `tip_delay 0`, one with `-1`, one without the key, each 30 frames
near then 400 more: before, none was seen after 30 frames; after, `tip_delay 0` shows within the 30 frames
(`tips: "d0"`, `seen`) and the other two still only after the player's 3 s.

## Test runs: quit with no map, and kit fixes (2026-10-07)

A test script with no map loaded (`wait5; toggleconsole; quit`) or after `menu_vr dump` hung at its end until the
kit's 120 s timeout. Without a map the console is up (forced); `toggleconsole` closed it, so `quit` (Host_Quit_f, not
from the console) opened the quit confirmation, which waited for a key forever; `menu_vr` left the VR menu with the
keys the same way. `quit` now quits at once in a test run (`QVR_TEST_BACKGROUND`: the kit's run.ps1 and the motion
review's child copies set it); a player's quit outside the console still asks. Proved headless: the three scripts
(`wait5;toggleconsole;quit`, `menu_vr dump` then `quit`, and with `toggleconsole`) went from TIMEOUT to `exit=0` in
2-3 s; e1m1 with `toggleconsole;quit` still `exit=0`.

The kit's side (outside git; kit/README.md): eval.sh skips canary takes missing from the motions folder and runs the
rest (it failed on the first missing one), never counts `eval_status.csv` as a take, and prints `0 takes` when there
are none; run.sh `-Debug` runs the Debug build (`build.sh <name> --debug`; TESTING.md, "Debug build assertions");
`-Instance k` handles an `id1/maps` that `bench_maps.ps1` made a real folder; `quakevr/tips_seen.txt` is put back
after each run as `ironwail.cfg` is; a relative `-Out` lands in the worktree's `scratch/`.

## GL errors in Debug builds (2026-10-07)

A Debug build's short e1m1 run printed 760 `GL api error [#1282] GL_INVALID_OPERATION ... Invalid component count`
and 16 `GL api undefined [#131222] ... a sampler ... with a texture object (0) with a non-depth format, by a shader
that samples it with a shadow sampler`. To find them, the GL debug callback (gl_vidsdl.c, GL_DebugCallback) now prints
the caller's stack after an error or undefined-behaviour message, once per distinct stack (`GL error caller: ...`;
vr_crash.cpp's VR_DescribeCallers, DbgHelp loaded on the first one, symbols from the .pdb).

- **Invalid component count** (every eye, every frame): vr_bloom.cpp's `pass` set `Params` (`glUniform4f` at location
  0) for every pass, the mean pass too, whose shader has no uniforms. `pass` takes `withParams`; the mean pass leaves
  it out. The failed call changed nothing, so the bloom is the same.
- **Shadow sampler on texture 0**: the world and alias shaders' `ShadowAtlas` / `ShadowStatic` (units 4 and 5,
  `sampler2DShadow`) had no texture when the atlas wasn't made (shadows off, or no shadowed map lights: no static
  atlas), so every draw sampled texture 0. vr_lighting.cpp binds a 1 x 1 depth placeholder (`noAtlas`, made the first
  time it is needed) for a missing atlas. The shaders don't read it then (the shadow flags), so nothing draws
  differently.

Debug runs, `GL api (error|undefined)` count, all 0 after: e1m1; vrstart; vrfiringrange; start, and start at its
teleporters (`setpos 232 1320 24 0 90 0`) in VR with `vr_light_test`, `vr_eyeshot 3`, the spectator and
`vr_portals_maxviews 8`, and flat with `vr_portals_shot`, `r_scale 2`, `viewsize 80`; the bench's `combined` and
`particles_dense`; the menus (`menu_vr`, Debug > Checklist, Recording, the main menu, the console) and no map; both
eyes (`vr_mirror 2`, `vr_eyeshot 1`); the spectator with Bullet Time; flat e1m1 with `r_showtris`, `r_lightmap`,
`r_fullbright`; `vr_profile_overlay 2`, `vr_parallax_debug 1`, `vr_bloom 0`, `vid_restart`; vrcalibration; shadows off
and back on. Release screenshots (e1m1, vrfiringrange, start's teleporters, e1m1 with shadows off) differ from the old
build's no more than two runs of the old build differ from each other.
## The calibration room: calibration only, id's base textures (2026-10-07)

Your request: nothing in `vrcalibration` but calibration (no buttons, weapons or props), a small polished room, good
lighting, a little more interesting geometry, texture alignment checked; then: id's own textures (your decision: the
committed `.bsp` embeds them; the WAD is made from your paks at build time and never committed).

- **The room** (`Misc/quakevr/make_vrcalibration_map.py`, rewritten): an octagonal chamber 320 units (9.75 m) across,
  176 (5.4 m) high. Every side is one `tech14_1` panel (straight sides 128, cut corners 136, their panel fitted), riveted
  pilasters (`tech04_3`) over the joints with a small lamp (`tlight01`) each, a `tech04_1` skirting and rail round a
  `metal4_4` wainscot, a bevelled cornice, `sfloor4_2` floor plates and an `sfloor4_1` ceiling with a stepped coffer: a
  ring of `ceil1_1` light tiles under its step and a `light3_3` panel in its middle. The calibration spot: an octagonal
  pad with our `qvrc_pad` (a glowing ring; `quakevr_dev.wad`). North, ahead of the player: a framed black board, its
  texts (what happens; where height, body, hands and Position are in the menus: `{menu:...}`, 4 paths). South, behind:
  a cased doorway, a short passage and a `*teleport` glow; walking into it (`trigger_changelevel`, no intermission) takes
  you to the vrstart hub. Worldspawn `_vr_debris 0`, `_vr_crates 0` (no rocks or crates).
- **Texture alignment by rule**: the script builds every brush as a convex hull and places each face's texture by a
  rule (world-aligned so coplanar faces continue: the floor and ceiling tiles meet the walls and the coffer at their
  joints, the south side's panel runs on above the doorway; or fitted: each strip fills its band's height exactly, the
  cut corners' panels, the lamps, the board's frame and the spot fill their faces). Compiles clean (qbsp, vis, light
  with bounce, dirt and the light grid; no leaks, no warnings).
- **id's textures**: `Misc/trenchbroom/make_id_wad.py` writes `quakevr/wads/id_textures.wad` (git-ignored) from
  `id1/pak0.pak` and `pak1.pak`: every texture of id's maps (573), each once, the same bytes every time. The map's
  generator reads the textures' sizes from the WADs (and stops, saying how to make it, when the id WAD is missing).
  `make_assets.py` keeps only one new texture, `qvrc_pad` (and no longer fails after writing the WAD when PIL is
  missing).
- **No buttons, so the calibration's text changed** (`vr_setup.cpp`): in the room the menu button **pauses** it: the
  menu opens on Body Calibration's page (its first row is Position: standing or seated) and the calibration starts over
  when the menu closes (elsewhere, `vr_setup here`, it still stops). The intro says "Playing seated? Open the menu: set
  Position to Seated, then close it."; the summary and the console say the glowing doorway behind you leads to the hub;
  a stop says the main menu's VR Calibration runs it again. The main menu's confirmation no longer mentions a main-hand
  step ("your height, then your body").
- **The old room became the test hall**, `maps/vrtesthall.bsp` (`make_vrtesthall_map.py`: the old generator, renamed,
  its title board "VR TEST HALL"): its setting buttons (`vr_setup_option`), pool, climbing platform and pickups. The
  tests that used the old room's water and open floor use it (`physbench.py` water, `parryinterrupt/test.py`;
  TESTING.md). Delete it if you don't want it shipped: the two scripts point at it.
- Checked headless: `vr_setup_pending 1; vr_startgame` loads the room and starts; `togglemenu` pauses on Body
  Calibration (row Position), closing the menu starts over; the height is taken (1.70 m), `vr_setup_skip` past the body,
  done; walking into the doorway loads `vrstart`. `vr_menu_path_check`: the room 4 found, 0 missing; the test hall 14,
  0. Screenshots (flat and the headset's eye) from the spot, the corners, the doorway and the coffer.
- **To try in the headset**: the room's scale and light (the lower wainscot is dark by design: the light comes from the
  coffer and the pilasters' lamps), reading the board from the spot (letters 0.28-0.7), the pause on the menu button and
  Position: Seated, walking out through the doorway.
## Teleporter test map (2026-10-07)

`map vrteleporters` (Debug > Teleporters: Test Map, with rows that put you at the flush, framed, turning and
heights/water gates): teleporter pairs to test walking, props and monsters through. Made by
`Misc/quakevr/teleporters/make_vrteleporters_map.py [--compile]` (the .map, then qbsp, vis and light as MAPPING.md's Full
profile without `-dirt`, for even light to debug by); id's textures (the author's decision: the committed .bsp embeds the
ones it uses) from `quakevr/wads/id_textures.wad`, which `Misc/trenchbroom/make_id_wad.py` makes from the player's own
id1 paks (git-ignored; the same script as the calibration map's). No leak, 28 gate sides built.

- **Rooms.** The hub (start; a sign, a tip, five weapons to grab, ammo). North: the flush galleries, FA's north wall and
  FB's south wall face to face with four gates each, bottoms at the floor: crate (48x48), player (64x96), large
  (128x160: shambler, fiend, vore), very wide (256x128). FB lies behind FA's gates, so a monster chasing you through
  walks straight at them. East of them the framed galleries (GA/GB): the same in protruding frames (id's `slipside`)
  with sills: a crate hatch (sill 24), player (sill 16: a step), player (sill 32: a jump), large and wide (16). West:
  the turns room T (its west and east gates loop into each other: walk west for ever; its north gate comes out of U's
  east wall, 90 degrees; its south-west corner is a 45-degree wall whose gate comes out of U's south wall). South: LV,
  a floor-level gate that comes out over a 128-high platform (stairs back down), and two gates beside a sunken pool that
  come out facing it. Every gate goes both ways; each side's destination stands 48 units out from the other side's face
  (56 framed): clear of its trigger and frame for a shambler-sized monster (Quake's teleport puts monsters there).
- **Props and monsters.** Each room has five buttons (grunt, dog, ogre, shambler, scrag: `func_enemy_dispenser`s by its
  far wall). Crates, ammo boxes by the gates, a crate on a ramp in FA rolling at the crate gate, one floating in the
  pool. `_vr_crates 0`, `_vr_debris 0`: nothing placed at random.
- **Sheet depth.** The large flush pair's `*teleport` sheets are 48 deep, every other gate's 8 (as id's, a wall right
  behind). A prop's box stops against that wall before its middle reaches the gate's plane when its half-width is more
  than the sheet's depth (the split collision at the plane is the player's only): a small crate thrown at 46 u/s (a
  crate's throws are slow) stays at the 8-deep player gate (y 626, its box against the backing) and goes through the
  48-deep one. Fast light things (a shells box at 335 u/s) cross the 8-deep ones. Worth an engine fix (props' collision
  split at the plane as the player's); the map shows both.
- **Views black at some gates (fixed).** R_MarkSurfaces takes the fat PVS round the view's origin when the view leaf
  holds a liquid's or a gate's face; in the view through a gate that origin is the camera carried behind the
  destination (in a wall: an empty PVS), so the world through the gate was black, entities still drawn (the loop, the
  turns, the platform gate: their destinations' leaves touch a gate's face; the galleries' don't). `VR_PortalPVSOrigin`
  (vr_portals.cpp, from r_world.c) takes it round the destination's point instead, the point VR_PortalViewLeaf finds
  its leaf by. `r_novis 1` showed it before the fix.
- **Headless results** (`Misc/quakevr/teleporters/teleporters_test.sh <agent> [walk|throw|chase|views]`): the player walks
  through every player-sized and bigger gate, both ways, flush and framed (sill 16 stepped over); the crate gates and
  the 32 sill stop him, a jump takes him through the 32 one; the loop, 90 and 45 degree turns, the heights (out at z 152
  on the platform) and the pool gates (into the water) carry him with the right yaw. Thrown: above. Chased (the player
  put 150 units past the gate in the north room, the monster 200 behind him): a dog through the flush player gate in 75
  frames, through the sill-16 frame in 60; at the sill-32 frame it bumps, slides along the wall and takes the sill-16
  gate next to it (135 frames: out at that gate's destination); a fiend through the flush large gate in 30 frames and
  the framed large one in 45-90 (one run of three it never left its spot). Grunts (and shamblers, ogres) stand and shoot
  through the gate instead (PORTAL_AI.md: ranged monsters see through one gate and don't walk to you). Views: both
  eyes, every kind of gate (`scratch/teleporter_views.png`).
- **To try in VR:** each gallery's buttons with you on the far side; throwing crates and boxes through the 8-deep and
  48-deep gates; walking the loop; the 45-degree gate's turn; the pool gates.
## Blunt blows sound of what they hit (AUDIO_REVIEW.md row 4, 2026-10-07)

A punch, a gun's butt, a headbutt, a pommel or a club, a struck or held gib, a prop flung or thrown into something
and a thrown weapon's blunt landing all played `fisthit.wav` (one recording) whatever they hit. Each now plays a second,
quieter layer of what it hit, from the hand (its any-free channel, `VRGetHandChannel`) or from the thrown thing
(`vr_crates.qc` VR_Blunt_HitLayer), with the pitch variation (`VR_SoundVaried`):

- a body (a monster, a player, a corpse, a gib or head): `vr/phys/flesh_m1..4` at 0.8 (RMS 9%: about 13 dB under the
  thud's 32%);
- an armoured one (`monster_knight`, `monster_hell_knight`, `monster_enforcer`, their corpses too): `vr/phys/metal_l1..4`
  at 0.8 over the flesh slap at 0.4;
- wood (a crate, its pieces): `vr/phys/wood_m1..4` at 0.8;
- a wall or the floor (the fist's own path; `!takedamage`): `vr/phys/soft_m1..4` at 0.6;
- metal (an explosive box, a door, a lift: `VR_Hit_Metal`): none, its heavy knock already replaces the thud.

Every existing recording; `flesh_m3/4` precached in `world.qc` (the engine's bodies' set has only 1 and 2). The call
sites: `weapons.qc` W_FireAxe, W_GunMelee, W_ChainsawMelee, W_FistMelee (not when a held rock or brick knocks as
itself), a thrown weapon landing; `combat.qc` a corpse struck; `vr_carry.qc` a flung prop, VR_Gib_Blow, a gib struck by
the other hand; `vr_crates.qc` a thrown prop; `client.qc` the headbutt. `vr_snd_hit_layer` (1; 0 none): Audio >
Movement and Nearness, **Blow Material Layer**. `developer 1` prints `blunt hit layer: <sounds> on <class>`.

Checked (`-Sound -RealTime`, `vr_snd_spatial 0`, `snd_show 2`; a shells box flung with `impulse 232`): into a grunt
`fisthit.wav` (pitch 0.975) and `flesh_m4` (0.988) on the box's channel 0; into a knight `fisthit` and `metal_l4`
over `flesh_m3`; into a zombie `fisthit` and `flesh_m1`.

## The foregrip clicks (AUDIO_REVIEW.md row 6, 2026-10-07)

The other hand closing on a weapon's foregrip (a gun's, a sword's grip or blade, a weapon carried off its handle, a
free grip anywhere on it) had no sound and no haptic. Now (`vr_twohand.cpp` gripFeedback, at the end of `apply`, on
the edge of the hand's `helpingHand`): a short metal click from that hand (`vr/phys/grab_metal1..3`, the climbing
hand's and the explosive box's grabs, RMS 9%; client `S_StartSoundPitch` on the player's hand channel, `SND_CHAN_HAND`
or `SND_CHAN_HAND2`, so it plays from the hand and follows it) at `vr_2h_grip_sound`, pitch-varied by
`vr_snd_pitch_jitter`, and a 30 ms pulse (0.45) in that hand; letting go, the click at half that, no pulse. At most one
per hand every 0.25 s (a grip at the edge of its reach). `vr_2h_grip_sound` (0.6; 0 no sound, the pulse stays): Aiming >
**Grip Click**. `developer 1` prints `2h click: <hand> takes hold / lets go`.

Checked (`-Sound -RealTime`, `vr_snd_spatial 0`, `snd_show 2`; a shotgun in the main hand, the off hand put on it at
0.75 with `vr_mock_hand_to off held` and gripped, then let go): `grab_metal1.wav` on channel 9, pitch 0.998, `at 486
-326 102 | off hand 486 -326 102 (0.0 off)`; letting go `grab_metal3.wav` (0.978) the same way.

## Burning bodies crackle (AUDIO_REVIEW.md row 7, 2026-10-07)

A burning monster, corpse, crate or crate piece was silent after the catching whoosh (`vr/torch_light.wav`). Its fire
now crackles (`vr_burning.qc` VR_Burn_Crackle, from VR_Burn_Think): Quake's torch loop (`ambience/fire1.wav`, the wall
torches' crackle; now precached in `world.qc`, as it was only on maps with torches) on the body's own channel
(`CHAN_BURN`, 6, new in `defs.qc`: no monster uses it but Armagon, who is fireproof; 5 and 7 are the bodies' drag
scrapes, below), so it follows the body, heard only
near (`ATTN_STATIC`), at `vr_burn_sound`. It starts 0.3 s after the fire (the whoosh first); in the flames' last 1.5 s
it plays at 0.6 of that, in the last 0.75 s at 0.3 (the loop restarted lower: a three-step fade), back to full if the
body is lit again; every way a fire ends goes through VR_Burn_Out (burnt out, gibbed or removed, doused, a crate broken
or a piece burnt away, the scene cleared), which stops it with `misc/null.wav`. At most `vr_burn_sound_max` fires
crackle at once (`vr_burn_crackles`, recounted over the `vr_burn` entities when it says the most do, so it can't drift):
the others burn silent and look again every 0.25 s, taking a slot as one goes out (not in their own last 1.5 s). None on
you (you cry out: VR_Burn_OnYou). `vr_burn_sound` 0.6 (0 none) and `vr_burn_sound_max` 4: Combat > Burning,
**Crackle Volume** and **Most Crackling**. `developer 1` prints `burning: <class> crackles at <volume> (<n> crackling)`.

Checked (`-Sound -RealTime`, `snd_show 2`, `vr_burn_sound_max 2`, `vr_burn_corpse_time 4`; three things set on fire
with `vr_burn_test 1`): a crate and the first grunt's corpse crackle (`ambience/fire1.wav [L]` on channel 6 of each),
the second corpse stays silent (2 crackling); the first corpse fades 0.36, 0.18, then stops (1 crackling) and goes out;
the second, then in its own last second, doesn't start.

## Your own weapon taken back: a grip, not the pickup chime (AUDIO_REVIEW.md row 8, 2026-10-07)

Taking back a weapon you had thrown, dropped or let go of (or catching it, or pulling it with the force grab) played
Quake's pickup chime (`weapons/pkup.wav`, RMS 32%, the loudest UI-like sound) at 1.0 and a full 0.3 s pulse, as for a
weapon new to you. A weapon leaving a player's hand (`DropWeaponInHandScaled`, both throw modes) now remembers him
(`.vr_wpn_from`, `vr_fields.qc`); taken back by him (`wpnthrow_handtouch_impl`), it plays the hand closing on its
handle instead: `vr/phys/grab_metal1..3` (an axe's `grab_wood1..3`; RMS 9%, 0.17 s) at 0.9 from the hand (its own
channel), pitch-varied, and a 0.12 s pulse at 0.7. Weapons from the level, a monster, a crate or ammo box, the test
spawns and a player's death keep the chime. Drawing from a holster was never the chime (`weapons/holster0/1.wav`).
`developer 1` prints `weapon: <class> taken back (<sound>)`.

Checked (`-Sound -RealTime`, `snd_show 2`): a crowbar dropped ahead (`impulse 217`) taken (`impulse 216`):
`weapons/pkup.wav` on its channel 0; let go of and taken again: `vr/phys/grab_metal1.wav` on the player's channel 8
(the main hand), pitch 0.997; the shotgun let go of and taken by the mock hand (`vr_mock_hand_to main weapon 0.3`):
`grab_metal2.wav` on channel 8.

## Bodies dragged along the floor (AUDIO_REVIEW.md row 2, the drag left from it, 2026-10-07)

A ragdoll dragged by a limb, shoved along or blown across the floor, and a pushable corpse pushed, slid silently (row 2
gave them knocks only). Box3D's scrape detection (`vr_box3d.cpp` noteSlide, the props') is now `bodySlide` for any
body, and `noteBodySlide` looks at each awake part of a ragdoll (not its cut ones) or the corpse's body after it is
written: the part sliding hardest on the level, a door, a fixture or a prop (not on another body or its own parts:
`catCorpse`, a hand or a player), reported to the physics sounds as flesh (`physsound::slide`'s `body`: the soft scrape
grains, `vr/phys/scrape_soft1..4`, on the body's channels 5 and 7, its weight the part's, times `vr_physsound_bodies`
and 0.7 under a prop's scrape). Only while the body as a whole goes along the floor (its parts' mass-weighted level
speed at least 0.4 m/s): a ragdoll crumpling as it dies or settling is all but silent. A body's scrape starts after
0.15 s of sliding (a prop's 0.08), slides 0.2 s apart count as one, it stops only after 0.2 s without one (its parts'
contacts come and go), and never restarts within 0.6 s of stopping (`vr_physsound.cpp` bodyScrapeDelay,
bodySlideGrace, bodyScrapeGap). The burning crackle moved to channel 6 (it was on 7, a scrape channel: a burning body
dragged would have cut it).

Checked (`vr_debug_physsound 1`; `-Sound -RealTime`, `snd_show 2`): a settled grunt's ragdoll blown along with
`vr_ragdoll_blast_test 60`: `scrape starts, flesh at 0.55 m/s`, `vr/phys/scrape_soft2.wav` on channel 5 then
`scrape_soft1` on 7 of the corpse for 2 s, then `scrape stops`; `ragdoll_sound_test.sh flat pile`: a body dying on the
floor shuffles 0.2-0.4 s at volume 0.1 (before the speed gate: 2.5-3 s), the knocks as before. Dragging by a limb uses
the same contacts; its mock (`vr_mock_hand_to off ragdoll near 3` and a grip) failed to take the limb in most runs here
(the stock `ragdoll_test.sh grab` too, once of two): to try in VR.

## Bullet time's "off" sound no longer outlasts a short burst (AUDIO_REVIEW.md row 5, 2026-10-07)

The sounds are still the placeholders (`items/inv1.wav` on, `items/inv2.wav` off: the Ring of Shadows'; Vittorio picks
the real ones), but the 3 s "off" sound ran on long after a short burst. Now (`vr_bullettime.cpp` OffSound): it plays at
most `vr_bullettime_sound_off_max` real seconds (1; 0 the whole sound), then fades out over 0.25 s and stops; started
again (or a denied press), it fades out over 0.06 s at once (the "on" sound, a local sound on the same channel, already
replaced it; this covers an empty `vr_bullettime_sound_on`). The fade lowers its channel's `master_vol` each frame
(advance); the channel is found again each frame by its sound, the player's entity and the local channel -1, so a
channel another sound took is never touched. Menu: Recording > Bullet Time, **End Sound Plays At Most**.
`vr_debug_bullettime 1` prints `the off sound cut <t> s after its fade began`.

Checked (`-Sound -RealTime`, `vr_snd_spatial 0`, `snd_show 2`; `vr_bullettime` on, 40 frames, off): `items/inv2.wav`
at L/R 255 for 1.0 s, then 253 down to 1 over 0.25 s, then gone (`cut 0.25 s after its fade began`); started again
0.3 s after with `vr_bullettime_sound_on ""`: 85, 68, 51, 34, 17, then `cut 0.06 s after`.

## Dawn of the Machine (MG3): world and progression (2026-10-07)

Phase B of [MG3.md](MG3.md) (M3-05..10), following Vittorio's decisions and the agglomeration principle (an
official expansion's entities work in any map when their data is there). MG3 stays gated (`nativeReady` false); MG3
tests use `vr_campaign_native mg3`, `-nomapindex -noaddons`, developer 1.

- **M3-05 map triggers I.** `QC/vr_mg3_triggers.qc` (adapted `mg3_triggers.qc`/`triggers.qc`, GPL header kept), in
  every campaign (none needs MG3's data): `trigger_always` (fires 0.1 s after load), `trigger_door_relay` (toggles its
  func_doors/func_buttons by state; flags 1/2 leave open/closed ones), `trigger_teleport_silent` (moves the player by
  `height`, default -2048, +1 going down; telefrags; what the hands carry comes along: `VR_Carry_Teleported`),
  `trigger_multitouch` (first touch / emptied after 0.2 s; flags 16/32/64), `trigger_explosion_repeater` (after
  `delay`, a 120-damage blast every `wait` + random `pausetime`; flag 4 counts), `trigger_music` (CD track `style`),
  `trigger_heal` (`dmg` every `wait`), plus upstream's unplaced `trigger_doorgroup_relay`, `trigger_quad`,
  `trigger_relay_killmonster` (all official, for other maps). Coop flags 32768/131072 as upstream. Measured
  (`vr_mg3_test 6`, Map Triggers Check, any campaign; `7`, Explosion Repeaters Check): secret6 31/31 silent teleports
  moved by their height into the open, 14/14 door relays toggle their targets; map7 4/4 relays; spawned
  always/multitouch(x3)/heal/music/quad/doorgroup/repeater/killmonster 13-15/0 on secret6, map7, hub, boss, map3 and
  stock e1m1; secret2's 60 repeaters (all `explosions`, count 2..8) all fired, 325 blasts = the counts' sum. Checker:
  35 missing, 1,189 placements (from 42/1,328). Regression: MG3 map1 shared triggers 34/0; Dopa triggers 34/0, world
  19/0; MG1 hub 20/0, Horde 24/0; e1m1.
- **M3-06 map triggers II and keys.** `health_target` (T_Damage, before the blow as upstream: each
  `trigger_health_relay` it names fires once when the monster is hit with less than its fraction, default 0.5, left,
  then is removed), `aggro_target` (FoundTarget: a waking monster uses what it names 0.1 s later, waking sleeping
  monsters at its enemy; every monster sharing the key forgets it). **Upstream ships aggro commented out**, so it is
  off unless `vr_mg3_aggro_groups 1` (archived, default 0; Debug > Tests > Dawn of the Machine Tests > Aggro Groups);
  it follows the key's documentation, not the commented code (which counted the group's other members and so ignored
  a lone monster's target). `trigger_lore`: its text while standing in it, cleared `MG3_LORE_DWELL` (3 s; upstream
  0.5) after leaving (PlayerPostThink), shown by VR's centre print (in view or the wrist hologram); hub rune hints
  switch to their "complete" text, map4's Bloody Nightmare hint goes once it is known. New MG3 serverflag names
  (`MG3_SF_RUNE1..4`, `MG3_SF_BN_ACTIVE/DISCOVERED/NEWGAME`, `MG3_SF_TETTE`). Keys declared: `wave1..3`,
  `tele_target` (the finales' tasks), `comment`, `dirt`, `fog_sky_factor`; the engine reads `fog_sky_factor` as
  Ironwail's worldspawn `skyfog` (gl_sky.c; developer 1 prints "sky fog"). All work in any campaign. Measured
  (`vr_mg3_test 8`, Monster Keys and Lore Check): map3 demon 300/300, relay 0.5: 14 hits of 5%, fired on the first
  hit below half, once (4/0); map8 shambler with two relays (0.75, 0.5) both on their first hit below, once (3/0);
  map8 aggro group `second_room_shambler_01`: off 0 of 1 woke, on 1 of 1 and the key cleared; lore map3/map5
  localized ("Look at the heavens..." for `$mg3_map5_mural`), cleared after the dwell; secret4/dm1 "sky fog 0.2".
  Checker: 33 missing classes, 1,146 placements, 1 unknown key (`property 1`, editor noise on two secret1 lights:
  not a valid field name). Regression as M3-05 (all pass).
- **M3-07 items.** `QC/vr_mg3_items.qc` (adapted `mg3_items.qc`/`items.qc`): `item_armor_shard` (+5 armour, green
  0.3 when none is worn, at most 200; touch pickup like ammo), `item_draught_insight`/`_stupor` (rings that move the
  player by `height`, -2048/+2048 by default, with the teleport flash; OVERRIDE_DEST: to the target destination by
  `teleport_touch`; they stay; carried things come along), `item_artifact_lavasuit` (powerup: 30 s without lava or
  slime damage nor drowning, `lavasuit_finished`, the "wearing out" warning; a holster object like the biosuit;
  authored `wait` respawns it), `item_head_hellknight` (a holster object: Bloody Nightmare ACTIVE|DISCOVERED, skill 3,
  and the new archived `vr_mg3_bn_discovered 1`, which M3-09's menu reads; in a Bloody Nightmare game a megahealth,
  or the bunny once in its new game). MG3 item flags: SPAWNED (4: hidden until a trigger uses it, then back with the
  respawn sound, through Honey's item_unspawn/item_spawn) and DROPTOFLOOR_DISABLE (65536: placed, then stays put),
  campaign 5 only (MG1's upstream has DROPTOFLOOR_DISABLE too; not applied there). Agglomeration: each needs its model
  on the search path (skipped otherwise), the lava suit falls back to the biosuit's model and the head to the hell
  knight's head gib. Measured (`vr_mg3_test 9`, Items Check): shards map3 16 -> 80, map2 25 -> 125, boss 22 -> 110,
  map2b 46 and map4 44 -> 200 (cap), one on yellow 150 -> 155 kept yellow; map2b 6 draughts (5 by height into the
  open, 1 to its target), map2 1 to its target; boss lava suit 30 s, lava and slime hurt before, not while worn, again
  after; taken, back after its wait; map4 head: serverflags 0 -> 192, skill 3, discovered 1, killtarget lore1 gone;
  `changelevel map4` under Bloody Nightmare: the head is a megahealth (7 = 6 + 1); map3's 4 trigger-brought items
  hidden, one appears when used. Checker: 28 missing classes, 701 placements. Regression as before (all pass).
  Unrelated: `map map2` crashed 2 of 7 loads in `vr_hull.cpp:450` (hull walk on a worker thread, before any test
  ran); the other 5 and every other map loaded.
- **M3-08 runes and hub.** Campaign 5's `item_sigil` (`MG3_item_sigil`, vr_mg3_items.qc: end1..4 models,
  `$mg3_qc_rune1..4` centre-printed to everyone, the bit in `style`, spawnflag 128 hidden until a trigger uses it:
  map8's; a holster object); `trigger_rune_relay` (passes a use on 0.1 s later with every rune its flags name) and
  `trigger_rune_counter` (fires with at least `count` runes, default 2), in any campaign. Upstream's
  RemovedRuneCheck (`MG3_RuneRemoved`, vr_mg3_defs.qc): an entity with NOT_IF_<n>_RUNES (262144 << n) for the runes
  home is not there: items (StartItem), monsters (every start, through `MG_MonsterPrepare`, and the spawn functions'
  `MG_MonsterInhibited`), triggers, corpses, intermission views; campaign 5 only. `trigger_door_relay` now also takes
  VR's door classname "door" (id's doors.qc renames func_door; upstream MG3 does not): the M3-05 relay check had passed
  without moving a door, and now counts the doors moved (secret6 24, secret4 11; map7's four only close open doors,
  0 at load). The hub is worldtype 0, so the inventory carries through it (map3 -> hub -> map5: weapons, ammunition,
  holsters kept; keys dropped as on any changelevel). Measured (`vr_mg3_test 10/11/12/13/14`): secret1 -> secret5 ->
  hub -> secret3 -> map8 runes 1, 3, 7, 15 (map8's hidden rune brought out first); the hub's entry check with 2 runes
  opens rune 1 and 2 doors, not 3/4 nor the exit; with 4 all four and the exit (2/0 each); `save`, `map start`
  (serverflags 0), `load`: 15; map1 with 0 runes 88 monsters, 2 corpses, 1 intermission view, with 1 rune 66, 13
  and 1 (the map's two versions; before, both at once); secret2's exit: changelevel to boss (intermission, next map
  boss). Checker: 26 missing classes, 675 placements. Regression: Dopa triggers 34/0, world 19/0, MG1 hub 20/0, Horde
  24/0, e1m1.
- **M3-09 start, skill and Bloody Nightmare.** `trigger_relay_setskill` (hub buttons: 0..3 set the skill and end
  Bloody Nightmare, 4 starts it: skill 3, ACTIVE|DISCOVERED and `vr_mg3_bn_discovered 1`; localized messages) and
  `trigger_bloodynightmare_relay` (flags 1/2/4 require ACTIVE/NEWGAME/DISCOVERED, 8/16/32 their absence), any campaign.
  Campaign 5 (`vr_mg3_defs.qc`): `MG3_BloodyNightmareStrip` in DecodeLevelParms (skill not 3: ACTIVE cleared; else
  the hands' and holsters' level parms lose every weapon but the axe, shotgun, Super Axe (WID_MJOLNIR until M3-11)
  and, with the bloody bit, the super shotgun, with their records; the axe and shotgun go back to their default
  holsters (or the first free) when gone; items = axe|shotgun|best armour (+SSG); ammunition and armour kept);
  boss2 on skill 3 is Bloody Nightmare (upstream's level select); damage (T_DamageDeal): the player's blows on others
  80%, others' on the player 120%; the hub's exit to secret2 goes to boss2 in Bloody Nightmare's new game. **Official
  Campaigns > Dawn of the Machine: Bloody Nightmare** (Vittorio's decision): the row exists only once
  `vr_mg3_bn_discovered` is 1 (found in a game: the hell knight's head or the hub's button; the page is rebuilt when
  it changes); it sets skill 3 and `vr_mg3_bn_start 1` (unarchived) and starts the campaign as its row does (refused
  with its reason while MG3 is gated), and the start map's first second makes the game Bloody Nightmare (centre
  print). The start map's own skill brushes are id's `trigger_setskill` (already there). Measured (`vr_mg3_test`
  15-20): hub at load, serverflags 0: the head, the Bloody Nightmare button and the new-game lore gone, the skill
  buttons kept; with 192 the head and button kept; buttons 4/1/4: 192 + skill 3 + discovered 1, then 128 + skill 1,
  then on (3/0); a seeded loadout (holsters SSG, LG, -, sword, shotgun, Super Axe; yellow 120; ammo) after hub ->
  map3 on Bloody Nightmare: holsters -, -, axe, -, shotgun, Super Axe, armour and ammunition kept (2/0); with the
  bloody bits the super shotgun stays (2/0); `skill 1` + changelevel: ACTIVE cleared, nothing stripped; damage 50 ->
  40 dealt, 10 -> 12 taken on Bloody Nightmare, 50/10 otherwise; menu path (`vr_mg3_bn_start 1`, `skill 3`, `map
  start`): serverflags 192, skill 3, "You activated BLOODY NIGHTMARE difficulty."; `vr_menu_search Bloody Nightmare`:
  the row with discovered 1 (2,999 rows), gone with 0 (2,998); NG+ flags (`vr_mg3_test 19`) and the hub's exit:
  secret2 -> boss2 (plain game: secret2). Checker: 24 missing classes, 660 placements. Regression as before. Seen
  once: a crash in `GL_BuildBModelMarkBuffers` (r_brush.c:828, R_NewMap) on hub -> map3 after a test seeded holster
  ids without weapon records (2 of 5 such runs; 0 of 6 plain hub -> map3, 0 of 5 with records): unrelated engine
  fragility, noted.
- **M3-10 endings and credits.** `MG3_BossEnding` (upstream boss_end; monster_boss_final, M3-24/25, will call it 8 s
  after Chthon dies): a dead single player completes nothing; every player back to 25 shells, no other ammunition,
  no armour (VR: its plating items too); a Bloody Nightmare game goes to its new game (map1, serverflags
  ACTIVE|DISCOVERED|NEWGAME: runes cleared, capacity upgrades cleared, health 50; the bloody weapons stay; the level's
  strip leaves the axe and shotgun), otherwise the next map is `start` (the credits); the finale text
  `$mg3_qc_boss_finale`. Upstream sends the normal ending to the credits, not the hub as the plan's M3-10 line says.
  `MG3_ShubEnding` (upstream oldnew_credits; monster_oldone_new, M3-26/27): its final text (upstream's
  `$map_dopa_endtext_final`), then the credits. ExitIntermission's native completion (disconnect, `menu_credits`)
  covers campaign 5; the credits menu's title is "Dawn of the Machine" for it (menu.c). map8's and start's authored
  `endtext` already showed through the MG framework (checked). Measured (`vr_mg3_test 21/22/13/14`): boss: finale text
  "You did it! ...", stages 1-3, next map start (the credits' command queued); Bloody Nightmare (menu path) + seeded
  upgrades, bloody bits and loadout: next map map1, then serverflags 448, masks 0 (bloody 3 kept), health 50/50,
  ammunition 25/0/0/0, armour 0 (no plating bit), holsters SSG, -, axe, -, shotgun, Super Axe; its hub's exit leads
  to boss2; boss2: "CONGRATULATIONS AND WELL DONE!", next map start; map8 -> hub "You have gained a rune of
  power!..."; start -> map1 "You are drained..."; the credits menu titled Dawn of the Machine (screenshot). Loads:
  start, hub, map1, map4, boss, boss2, secret6 exit 0. Regression as before.

## Map load crash: the cache's LRU list from the pool (2026-10-07)

Seen by the MG3 work: loading MG3's map2 crashed now and then on a pool worker in `vr_hull.cpp:450` (the hull build's
walk: a hull 0 node whose plane number was garbage), and `R_NewMap` crashed in `GL_BuildBModelMarkBuffers`'
`CompareMarkSurface` (garbage marksurfaces; the hub -> map3 crash "after a test seeded holster ids" was this one too:
it came on a plain map2 load as well). Measured before the fix: 8 crashes in 65 map2 loads (fresh processes), always
the same node (25591) with the same garbage plane number, through the hull build, R_NewMap or the GL driver.

- **Not the hull build.** With hull 0's nodes and the planes made read-only (VirtualProtect) for the build, every
  node checked good when the build started, the page still read-only at the crash and the node still wrong: the
  value was written before (at the map's load), into the hunk.
- **The culprit:** the hunk, cache and zone checked for their thread (a crash with the caller's stack): the ragdoll
  rigs' warm-up at every map load (`warmRigs`, vr_ragdoll.cpp) runs `derive` for each monster model on the pool, and
  `derive` called `Mod_Extradata`, whose `Cache_Check` unlinks and relinks the model's block in the cache's LRU list:
  several workers at once corrupted the list (10 of 10 runs flagged it). Through stale LRU links a later cache
  allocation, flush or move wrote cache links into memory the hunk had since given to the map (the world's nodes),
  hence the same deterministic garbage. Not mimalloc, not the hull containers or the kept hulls.
- **Fix:** the alias headers are taken on the main thread (`Mod_Extradata` while the jobs are listed, then again with
  `Cache_Check` once every model is loaded, as a later load may have let an earlier one go) and handed to `derive`,
  which touches no engine state. After: 0 crashes in 50 map2 loads (fresh processes); with the check on, no other
  use off the main thread over e4m7, e1m1, start, hub (`vr_mg3_test 16`) -> map3 (`17`), map2, Release and Debug.
  The rigs are the same (start's zombie/soldier/dog: 12/12/13 bones).
- **`vr_zone_threadcheck`** (default 0; Debug > Threads > Catch Memory Use off the Main Thread): `Z_Malloc`,
  `Z_Free`, `Z_Realloc`, the hunk's allocations and frees, `Cache_Alloc`, `Cache_Check` and `Cache_Free` from any thread
  but the main one crash at once (`Zone_WrongThread`, with the culprit's stack in qvr_crash.txt). For tests: set it
  first in the script (`vr_zone_threadcheck 1; map ...`).
## vrstart2: the island hub at night (2026-10-07)

Your request: a new, much bigger and more polished hub in vrstart's spirit (a grassy island with a rocky beach in a
lake ringed by cliffs, at night, torches and lights under the water), the player starting in a corner and led along
one path (bridges, staircases, natural paths) past the campaign buttons and their teleporter, a settings area, a
makeshift firing range and a ladder; props, banners and tips. Added beside vrstart, not replacing it (yet).

**Made by a script** (`Misc/quakevr/maps/vrstart2_gen.py`, its geometry in `mapgeom.py`; MAPPING.md, "vrstart2"):
reproducible, the .map editable in TrenchBroom (groups), every brush's planes exact (points on a 1/8 grid, planes
from integer arithmetic). Terrain: a height function sampled on a jittered lattice plus the places' outlines,
Delaunay-triangulated (exact predicates; 10,500 triangles), a prism under each. The lake is 8000 units across (244 m):
1800-2200 units (55-67 m) of water between the island and the cliffs, 300 deep (the shelf by the island shallow),
six rock ledges at the cliffs' feet to climb out on, four islets (braziers on two). The cliffs: a noisy ring (bays,
headlands), faces faceted by six rings of points, mountains rising behind to 1300-1900.

**Textures: id's** (the coordinator's change, your decision): `Misc/trenchbroom/make_id_wad.py` extracts every texture
the paks' maps embed into `quakevr/wads/id_textures.wad` (git-ignored); the .bsp embeds the ones used. Planks show
one of woodflr1_2's 16-texel boards each (their texture offsets computed per plank); cliffs at twice the scale; wood
grain along each beam. Ours: the night sky box (`make_vs2_sky.py`: stars, the Milky Way, thin clouds, the moon where
the moonlight comes from; 6 x 1024 PNG, 2.8 MB) and `qvr_target` (a bullseye) in quakevr_dev.wad.

**Night lighting**: moonlight from the north-east (`_sunlight` 230, blue) and a dark blue sky dome, bounced light,
fog; 32 wall torches on posts and brackets (they can be taken, as anywhere), 10 large flames (braziers, the
campfire), lanterns, the teleporter's violet glow, 34 crystal clusters glowing cyan on the lake's floor. The fires and
lamps have no ambient occlusion (`_dirt -1`: with it the pavilion stayed black under its roof).

**Gameplay**: the campaign lecterns use the old hub's commands (`vr_activestartpaknameidx 0/1/2`, so buttons.qc's
"(unavailable)" for a missing mission pack still works; the teleporter is a `trigger_changelevel` to `start`, now
without intermission); Dimension of the Past's lectern runs `vr_campaign_select dopa` (starts at once: the teleporter
only knows the three that share their folders). New engine bits: `VR_IsVrMap` (the four VR maps, replacing three
copies of the list in vr_gamedir.cpp), **`vr_hub_map`** (archived, default `vrstart`; `vrstart2` makes the island the
hub VR starts in and `vr_campaign_hub` returns to), vr_setup_option's `holsters`, `reload`, `twohand`, `tips` (the
old hub's raw cvar buttons, with value screens now), and SELECTED over the chosen campaign lectern. The pavilion: 19
setting buttons on two boards, two rows each. The range: physics guns lying on the benches (shotgun, super shotgun,
nailgun, crowbar), shells, nails, a health box, target boards, crates, the training dummy, an explosive box, rocks
and bricks on a shelf. The ladder: rungs every 20 units from 16 over the ground (vrclimb's heights).

**Performance** (`vr_profile`, exclusive, the start, mock eyes): vrstart 0.65 ms CPU busy, GPU 0.81; vrstart2 GPU
about 1.3 (world+brush 0.6). 88-100k faces (the terrain's prisms cut each other's faces about four times over in the
BSP). Everything is func_detail (as func_walls the faces were no fewer and the dynamic lights' caster search dearer);
ropes and brackets are func_detail_illusionary. CPU busy about 1.5 ms (vrstart 0.65), most of it the dynamic lights' shadow
caster search walking the big world tree (`dlight casters` 0.6 ms); fine against the 11.1 ms budget.

**Tested headless**: the path walked leg by leg with the mock stick (`Misc/quakevr/maps/vrstart2_walktest.py`: 18
legs, pier, stairs, terrace, bridge, pavilion, stairs, range, shore path, the ladder's foot); a swim from the pier to a cliff ledge and out (40 s);
`vr_setup_option turning` pressed by hand (Snap 30), the Scourge of Armagon lectern pressed
(`vr_activestartpaknameidx` 1, its echo), the teleporter's changelevel; a rung held and pulled up (`vr_climb_debug`:
"main hand holds at 132", the body rising); `vr_climb_probe` lists the rungs as ledges;
`vr_menu_path_check maps/vrstart2.map`: 4 found, 0 missing. Notes: the climbing plays (`climb_plays.py ladder`) find
no hold on vrclimb either in this harness (a hand 5 units off); qbsp needs `-maxnodesize 0` (the midsplit's portals
lost visible faces: holes in the cliffs) and `-forcegoodtree` (an invisible bump on the shore path's clip hull).

**The snag on the shore path, found and fixed: nearly coplanar terrain.** The player stopped dead on flat ground
(the shore path near (470, -280..-390), and elsewhere): not at a fixed place (a walk from y -262 stopped at -266, one
from -265 went through), the box free everywhere there (`vr_stuck_test` every unit), with every hull (Quake's,
the compiled 16-wide one, the brush sweep), the body off, the tips off, and on the terrain alone (a map of only its
prisms). The cause: the ground's triangles nearly but not quite coplanar (the noise, the zones' blends, heights
rounded to units), so the hulls' planes cross at tiny angles and a trace can start "solid" at their seams. Proved
with the terrain alone: flattening that area exactly to 56 removed every stop there. **Fix**: the island's ground
heights are multiples of 8 (`GROUND_STEP`, at the end of `height()`; not below 8, so no ground lies in the water's
surface): most neighbouring triangles are now exactly coplanar (one plane), the rest meet at clear angles. A walk
test of 36 legs (24 random 300-unit legs over gentle ground, 12 across the shore path) on the terrain alone: 24
stopped short before, 7 with steps of 4, 3 with steps of 8 (two of them swims in the ravine); a second seed: 2, both
swims. The ground reads as gently faceted; things are placed from the same `height()`.

**To try in VR**: the whole walk at night (is it bright enough? `_sunlight`, the torches' `light` in the script's
`TORCH`); reading the boards and the lecterns; the ladder; the dive; swimming out to the islets and the crystals;
the cliffs from the water. `vr_hub_map vrstart2` to make it the hub.

**Smaller buttons, one texture each** (the author: "the buttons are too big and the texture repeats"): every button
(tutorial, calibration, campaigns, settings) is now 18 square (`BUTTON_SIZE`; they were 28 tall and 30-38 wide, the
32x32 `+0basebtn` at scale 0.5 tiling about twice each way; e1m1's are 32 square at scale 1), centred where it was and
as deep (6, the settings 8). `button_tex()` lays the texture as Valve 220 axes fitted to the box: on the front one copy,
its frame on the face's edges (scale = size / 32, shifts putting texel 0 on the left and top edges); on the sides, top
and bottom the texture's outer 4-texel frame band across the depth from the front edge, the front's fit along the
other way, as if the front were folded round. `+abasebtn` (the pressed frame) is the same size, so it fits the same.
The labels: QC's `func_button` put them 12 over the button's centre (6 in front), which on the small buttons
overlapped the settings' value screens (vr_setup.cpp, 9 over the top). Now a button shorter than 24 has its label
board 2 over its top (its half height from the label's rows and scale, as vr_text3d.cpp draws boards), and keeps the
board's top in `vr_button_label_top`; vr_setup.cpp puts the value screen (and a campaign's SELECTED) 2 clear above
that, never lower than before. Taller buttons (the test hall's, every other map's) are as they were.
## Dawn of the Machine (MG3): weapons (2026-10-07)

Phase C of [MG3.md](MG3.md) (M3-11..14), by Vittorio's decisions of 2026-10-06 and the agglomeration
principle (an expansion's weapons usable in any campaign when its data is there). MG3 stays gated (`nativeReady` false).

### M3-11 The Super Axe

**A weapon of its own.** MG3's `weapon_mjolnir` is not Hipnotic's hammer but a great axe ("Super Axe"). It is now
`WID_SUPERAXE` 18 (`IID_SUPERAXE` 48, `HIP_IT_SUPERAXE` bit 10), beside Mjolnir (`WID_MJOLNIR`, unchanged):
`weapon_superaxe` (FGD), `impulse 168`/`188` (a hand), `QC/vr_mg3_weapons.qc`. An MG3 map's `weapon_mjolnir` spawns as
a Super Axe in campaign 5 only (`hip_items.qc`); Hipnotic's maps keep Mjolnir.

**MG3's model in any campaign.** The engine reads a file of an owned expansion's folder in place, unmounted:
`owned/<folder>/<path>` (`vr_gamedir.cpp` `VR_OwnedFile`, hooked first in `COM_FindFile`: the discovered Dopa/MG1/MG3
root, a loose file or the highest pak that has it; any thread; nothing copied or written; `-1` "not there" for such a
name skips the search path). MG3's own `progs/v_hammer.mdl` is shadowed by Quake VR's (its Mjolnir) even in campaign
5, so the Super Axe is always `owned/mg3/progs/v_hammer.mdl` (and `_glow`, and `g_hammer.mdl` for the classic
pickup); `vr_have_superaxe` (`vr_packutil.qc`) says whether it is there, and `VR_Pack_WeaponAvailable(WID_SUPERAXE)`
removes its pickups and turns a held one into an axe without it (a save from another install). Model traits of an
owned path are its pack path's (`vr_modelmetadata.cpp` `packPathOf`: a view weapon...), its identity its own
(`Mg3SuperAxe`, `Mg3SuperAxeGlow`).

**The arm taken off, laid as the axe.** The model is a view model with the arm holding it (795 vertices: the axe 527,
the arm 268). As it loads (`vr_monstermods.cpp` `superAxePoses`, by its vertex and triangle counts only): every piece
but the largest (joined where vertices coincide) collapsed onto one handle vertex; every pose the first (the other
frames swing it across the view); turned and moved so its handle's end and line are the axe's and its blade faces the
axe's way; requantized, normals turned. Numbers: `Misc/quakevr/fit_superaxe.py` (reads the owned pak, prints):
rotation, move, scale_origin `-3.578 -2.931 -4.576`, head 27 to 49 units up the handle (the axe's 25 to 37), muzzle
at its middle.

**Held as the axe.** Weapon settings slot 24 (`vr_wofs_*_25`) is the axe's, Offset moved by 0.66 x the scale_origins'
difference and the hotspots by the opposite; muzzle anchor 757 (strip order: vertex 745, `vr_anchor_nearest`) and
offset `-0.071 0.21 -0.82` (the handle's line at the head's middle); Mass 3.5, Balance 24, Span 75 (the axe's 2.5, 20,
60). Slot 25 is the glow model, `InheritFrom 25` (its anchors repeated: they never inherit). `vr_wofs_version` 36
resets both (they were unused placeholders). Melee: `VR_MTHING_AXE` (the head an edge; it beheads as the axe;
blades' small gibs; a thrown one is a blade too), sounds MG3's (`hipweap/mjolslap`, `mjoltink`), envmap shine as
Mjolnir's, cells counter as Mjolnir's, no sights.

**Official behaviour.** A blow 40 (`vr_dmg_superaxe`, Combat > Weapon Damage > Dawn of the Machine > Super Axe: twice
the axe's 20, as in MG3), x3 on a zombie, x2 when it kills (it gibs). A second blow on the same monster within
`vr_superaxe_burst_window` (1.5 s; MG3's 0.5 s is a key-press rhythm) fires Mjolnir's lightning burst from where it
struck (15 cells, above water only: MG3 never discharges it; `vr_dmg_mjolnir_lightning` 80, then 3/8) instead of the
blow's damage; another monster in between, a wall or the air ends the chain. The head glows while the burst is ready
(MG3's `v_hammer_glow.mdl`, 15 cells or more; `VR_MG3_Weapons_Frame` puts it out). The pickup with spawnflag 128 drops
its taker by `height` (-2048) as MG3's `touch_teleport_silent`, carrying what the hands hold (`VR_Carry_Teleported`).

**Tests** (`vr_mg3_wtest`, Debug > Tests > Dawn of the Machine Weapons; developer 1).
- e1m1 (id1 campaign, MG3 data owned): `vr_mg3_wtest 2` (two ogres and a zombie; it sets Weapon Grip Mode 1 meanwhile)
  **19/0**: first blow 40, glow, window 1.5; second within it: burst, 50 -> 35 cells, no direct damage, glow out; the
  ogre 160 -> -1 by the lightning; after the window a first blow again; another monster ends the chain; under water
  no burst; 10 cells: no glow, no burst; zombie 120; a killing blow 80.
- `vr_mg3_wtest 3` (pickup ahead, grip held: `+grabright; vr_mock_button main grip 1`) **5/0**: into the main hand,
  gone, spawnflag 128 drop (-15 for height -16), target fired.
- MG3 map2 (`vr_campaign_native mg3; map map2`) `vr_mg3_wtest 4` **5/0**: no `weapon_mjolnir` left; the Super Axe has
  spawnflag 128, height -2048, target `axe_secret`; taken: dropped 2047 units, secrets 0 -> 1 ("You found a secret
  weapon!").
- Real swings (`motion_synth.py slash_horizontal_rtl --weapon superaxe --distance 0.85`, then `slash_overhead`, played
  in vrfiringrange, window 10): "Dummy: 35.5 damage - melee: Super Axe, chop (horizontal) with the head" (decap
  candidate), then the burst: lightning 80, 30, 80... on the dummy, 200 -> 185 cells.
- Changelevel e1m1 -> e1m2 and save/load: the Super Axe in the main hand and holster 0 (`vr_mg3_wtest 5`) kept (18,
  model `owned/mg3/progs/v_hammer.mdl`). Hipnotic hip1m1: Mjolnir (main) and the Super Axe (off) held together;
  `weapon_mjolnir` there is Mjolnir. `eval.sh`: no current takes (skipped).
- Images (kit scratch): `superaxe_hand.png`, `axe_vs_superaxe.png` (the same grip as the axe, the head further up).

**In the headset.**
- [ ] Debug > Tests > Dawn of the Machine Weapons > A Super Axe in Your Hand: the fist on the handle where it holds the
      axe, the blade the axe's way; tune Weapon Offsets (slot 25 in the cvars) if it sits off.
- [ ] Two swings on a monster within 1.5 s: the head glows after the first, the lightning on the second (15 cells).
      Tune Super Axe Burst Window if two real swings feel too slow or too easy.
- [ ] Holster it on the hip and the chest (the axe's holstered poses).
- [ ] MG3 map2: the secret axe drops you into the room below, as in Dawn of the Machine.

### M3-12 Axe buttons

`func_axe_button` (`QC/vr_mg3_weapons.qc`, official `buttons.qc` + `combat.qc`'s check): a `func_button` with health 1
that only a melee blow opens. Upstream checks the selected weapon (axe or Mjolnir); here, by Vittorio's decision 1, any
blow (`VR_IsMeleeBlow`, gating `T_DamageImpl`): a hand's melee (`T_Damage_VRMelee` or `vr_hitkind` MELEE: fists,
axes, the Super Axe, swords, the crowbar, the chainsaw swung, gun butts, a held prop swung), a headbutt, a bash, a
shove, or a thing thrown at it (a thrown weapon, prop or gib: `VR_Button_Thrown`, never a live grenade); its touch
also takes a thrown thing as a wall button's does (`button_touch_any`), never the player's body. A shot, a missile or
a blast does nothing and, to a player, says MG3's `$mg3_qc_axe_button` ("Use the axe"; spawnflag 1 NOMESSAGE: silent;
at most twice a second). FGD regenerated (302 entities). The checker's 33 placements (map6 1, map7 5, map8 18,
secret5 9) now resolve.

Tests: `vr_mg3_wtest 6` on map8 **3/0**: 18 buttons, all shut after a simulated shot and blast each, all 18 open to a
blow (fist, Super Axe, thrown weapon, thrown prop, headbutt in turn); map6 3/0 (1 button). Real path on map6
(`vr_mg3_wtest 7` stands you 28 units before the nearest closed one, facing it): the super shotgun fired at it, "6 on
func_axe_button", "Use the axe", still shut; then `motion_synth.py punch_straight` played `noplace yaw 270`: "damage:
func_axe_button 15.0 by a melee blow ... punch (straight) with the knuckles", opened (`vr_mg3_wtest 8`: state 0).

**In the headset.** [ ] map6's first button: shoot it (the message), then punch it, swing the axe at it, throw a
weapon at it: each opens one (map8 has 18).

### M3-13 The laser cannon

**One weapon, campaign-specific numbers.** MG3's `weapon_laser_gun` is Hipnotic's laser cannon "taken directly from
the hipnotic qc" (official `mg3_weapons.qc`): the same bolts (a bounce keeps 0.9 of the damage; the third touch, or one
in seven, stops it; a second hit on the same monster halves it), the same cell a shot and cadence. The only gameplay
deltas: a bolt's damage 18 -> 15 and a lit bolt's 25 -> 20. So it stays `WID_LASER_CANNON` with
`VR_LaserBoltDamage` (weapons.qc: Weapon Damage's Laser Cannon, times `MG3_LaserScale`: 15/18 and 20/25 in campaign 5,
1 elsewhere). Not ported: MG3's `respawn_ammo` (an out-of-cells laser, lightning gun or nailgun makes the map's
SAFETY_RESPAWN cell boxes come back): an ammo-items feature for every weapon (client.qc), left to the items' task.

**No Hipnotic install needed.** Quake VR ships the held model (`progs/v_laserg.mdl`, its own remodel), the bolt
(`progs/lasrspik.mdl`) and the sounds (`hipweap/laserg.wav`, `laserric.wav`); MG3's own `v_laserg.mdl` is shadowed by
Quake VR's (its offsets are tuned to it). `vr_campaign_probe` on map2b: `v_laserg.mdl`, `lasrspik.mdl`,
`hipweap/laserg.wav` from `quakevr`; `g_laserg.mdl` (the classic pickup) from MG3's `pak0.pak`.

Tests: `vr_mg3_wtest 9` (bolt damage, then 12 bolts fired at the floor ahead, `HIP_LaserTouch`'s new counters
`vr_laser_bounces`/`vr_laser_stops`): e1m1 18/25, 19 bounces at 0.9, 12 stopped (3/0); MG3 map2b 15/20, 18 bounces,
12 stopped (3/0). `vr_mg3_wtest 10` on map2b: the nearest `weapon_laser_gun` taken into the main hand (12,
`progs/v_laserg.mdl`, 30 cells) (2/0). (A bolt started inside your own box strikes you at once: the test fires from
28 units ahead.)

### M3-14 The bloody shotguns

`weapon_bloody_sg` (map1) and `weapon_bloody_ssg` (secret4) (`QC/vr_mg3_weapons.qc`, official `mg3_items.qc`): in
Dawn of the Machine only in a Bloody Nightmare new game (`serverflags & 256`; else the pickup is removed 0.5 s after
the map starts, as upstream's `bloody_weapon_check`; outside campaign 5 a map's one simply spawns). They are physical
weapon pickups of the shotgun and the super shotgun ("Bloody Shotgun", "Bloody Super Shotgun"); taken, they set their
bit in `MG3_bloody` (parm56, M3-02: kept through changelevel, death and saves, cleared by a new game), give upstream's
30 shells (to the cap) and say "You found a secret weapon!"; their targets fire as any pickup's. The bit changes every
shotgun of its kind the player fires, as upstream's does: the bloody shotgun's next shot after 0.28 s instead of 0.5
(`MG3_BloodyShotgunRefire`, weapons.qc W_Attack; reloading unchanged), the bloody super shotgun 28 pellets for 2
shells, spread wider across (`'0.075 0.025 0'` for `'0.035 0.025 0'`: upstream's 0.3/0.14 widening). Drawn as Quake
VR's shotguns: their own bloody skins are a BACKLOG item (decision 4). The Bloody Nightmare strip (axe and shotgun,
plus Mjolnir/the bloody super shotgun) is M3-09's.

Tests (`vr_mg3_wtest` 11 sets both bits, 12 reports, 13 takes the map's (or two spawned) bloody shotguns, 14 sets the
new-game flag, 15 clears the bits):
- e1m1: two spawned and taken **6/0**: bits 0 -> 1 -> 3, shells 25 -> 47 (+8 in the clip) -> 75, refire 0.28. Real
  fire (`vr_debug_shots 1`): the bloody super shotgun "28 world"; the shotgun pressed every 0.3 s six times: 3 shots
  without the bit, 6 with it.
- MG3 map1 (`vr_campaign_native mg3; map map1`): not a Bloody Nightmare game: "Bloody Shotgun removed". With
  `vr_mg3_wtest 14` and `changelevel map1`: it stays, taken **4/0** (bit 1, "You found a secret weapon!"); changelevel
  map2: bits 1 (parm56 1); `map map3` (a new game): 0; the map2 save loaded: 1, the shotgun in hand, 47 shells.

**In the headset.** [ ] A bloody shotgun (Debug > Tests > Dawn of the Machine Weapons > Bloody Bits On, then any
shotgun): the shotgun fires again much sooner; the super shotgun's spread is twice as wide and twice as dense.

### Phase C checks

Checker (`check_mg3_entities.py`) after M3-14: 39 missing classes, 1,293 placements (42 and 1,328 after M3-04:
`func_axe_button` 33, `weapon_bloody_sg`/`_ssg` 2 resolved). Regressions: Dopa e5m1 `vr_mg_trigger_test 1` 34/0, MG1
hub `vr_mg_hub_test 1` 20/0; Hipnotic hip1m1 laser cannon 18/25, 23 bounces (3/0), Mjolnir and the Super Axe held
together; e1m1 smoke; QC 0 warnings, statics, QC precedence and FGD (304 entities) checks pass. `eval.sh`: no current
melee takes (skipped).

## Performance batch: first spawns, the memory log, load allocations, the GPU wait, decoded images, campaign switches (2026-10-07)

The decision list of PROFILING_2026-10.md, items approved by Vittorio, one commit each. Numbers: `bench.sh
--exclusive`, his settings (`his_cfg_20261006_1237.cfg`), medians of 3 unless said; "base" is da5c33be.

### 1. A kind of monster's first appearance (`vr_probe_kinds`, QC vr_probe.qc)

**Why.** The firing range's first ogre, enforcer and death knight dropped a frame as they appeared (`gore_first_cuts`'
`spawn1_*`: 19.7, 23.8, 15.1 ms). Two causes, found with `developer 1` (each `late precache` and `hull: ... compiled
for` line) and the hitch log: their compiled hull built at the first move (the ogre's 40-wide tree 18.7 ms, the
enforcer's 28-wide 16.9 ms: "quake physics", 33 MB of allocations), and the second row's heads and missiles precached
at the spawn (`h_hellkn.mdl` 10 ms, `k_spike.mdl` 5 ms, `h_mega.mdl`...: "quakec"). Precaching at run time is what
modelcorrupt (91b808ca) had to repair for saves; making them ready at the map's load needs none.

**How.** As the map's entities have spawned (`VR_OnSpawnServerSpawned`, still loading), QC's `VR_Probe_Kinds` makes one
monster of each kind that can appear later, on a new entity, counted nowhere (the dummy's `VR_Dummy_Make`; the
dispensers' own spawn chain, now `VR_EnemyDispenser_Spawn`, for the four kinds that are no dummy type). Its spawn
function precaches its models and sounds; the engine then builds the compiled hulls of every monster there (the
probes' sizes with the map's) and frees every entity the probes made (a snapshot of the free slots before: whatever a
spawn function made too), before the first server frame: nothing of them is run, sent or saved. The kinds: the
firing range's dispensers', every type a training dummy can stand as (the menu changes it at any time), monsters
waiting for a trigger (Honey's trigger-spawned, Machine Games' deferred: their size and limbs unknown until then; their
limbs made with the map's), and with `vr_probe_test_spawn` 1/2 the debug spawner's kind or every kind (0 by default:
impulse 241 works on any map). A kind a monster of the map already is needs none. While the probes spawn, QC's
`random()` draws from its own numbers (`VR_ProbeRandom`), so the map's own spawns and first frames draw what they drew
before: the firing range's entities at the benchmark's start are the same, field for field (classnames, models,
origins, angles; without it a weapon rack's draws moved and two more entities stood there).

Limbs: `vr_limbs_prebuild` 1 (the default) makes the map's kinds' (and the waiting monsters') limbs as before; 2 also
the probes' (132 limb models on the firing range: +0.5 s to its first load of a session, warm disk caches, for first
cuts that already fit in the frame at 90 Hz: `cut1_*` medians 11.1-11.7 ms either way), so not the default.

| `gore_first_cuts`, worst frame (median of 3; CPU busy) | base | after |
|---|---|---|
| `spawn1_ogre` | 19.7 (19.7) | 11.4 (4.7) |
| `spawn1_enforcer` | 23.8 (23.8) | 11.1 |
| `spawn1_hknight` | 15.1 (15.1) | 11.1 |
| any `spawn1_*`/`spawn2_*` over 11.5 ms | 3 | 0 |
| firing range, the session's first load (3 runs) | 1272-1301 ms | 1399-1426 ms (+130 ms: 19 probes' models, 4 more hull sizes on the pool) |
| firing range, loaded again | 104-111 ms | 102-104 ms |

The `start` counts (edicts 253, Box3D bodies 142) are the base's. `precache_test.sh` (dummy, death, restart, legacy):
every `vr_model_check` 0 wrong. `developer 1`: `probes: pass 1/2, N entities ... ms` and `probes: kinds ready <bits>,
monsters counted N before, N after` (the same). Debug > Profiling and Memory: Ready What Can Appear, Ready the Debug
Spawner's; Gore > Limb Gore > Make Limbs as the Map Loads is a three-way choice.

### 3. The load's small allocations: debris faces, file lookups

The za::Vector audit's two main-thread sites on a big map's load: `MapFace::pts` (vr_debris.cpp, each world face's
corners as `debris::plan` reads them: a `za::Vector` grown 1-2-4-8) is a `za::SmallVector<glm::vec3, 8>`;
`VR_FileCacheHas` (vr_fscache.cpp: every file lookup while loading made three `za::String`s) folds the path in a
`char[MAX_OSPATH]` on the stack and looks the listings up by `za::StringView` (a transparent hash; a directory's
listing is made, as before, at its first lookup).

| warden, the session's first load (`vr_image_cache_mb 0`, 3 runs) | base | after |
|---|---|---|
| main thread `new` / `delete` over the load (`vr_alloc_sites 8`) | 275-284 K / 288-296 K | 120-121 K / 133 K |
| `malloc` / `free` | 11.0-11.1 K / 8.2-8.5 K | 11.1 K / 8.5 K |
| load total | 2748, 3292, 3363 ms | 2847, 2844, 2894 ms |
| `VR_NewMap: explosion debris` stage | 24.2, 28.5 ms | 26.6 ms (one run of 90.6: the whole load ran slow) |
| "image files looked for, none found" (1456 lookups) | 31.9, 32.5 ms | 31.4 ms |

163 K fewer allocations a warden load (58% of the main thread's `new`); the time they took (a few ms by the audit's
estimate) is inside the loads' run-to-run spread, which the hulls' build on the pool dominates. Behaviour unchanged:
the same lookups (the counts above), the same faces.

### 5. Decoded images kept across loads (`vr_image_cache_mb`, vr_imgcache.cpp)

**Why.** Ironwail frees the world's textures with the map, so every load decodes its image files again: QRP's E1M1 spent
505 ms of an 818 ms warm load decoding the same 312 files, and every map the shipped Quetoo material maps (70 ms on
E1M1), and a campaign switch back to id1 its skins (420 ms).

**How.** `Image_LoadImage` (image.c) asks the cache once it has opened a png, tga or jpg, before decoding it, and gives
it the pixels it decoded (stb_image's buffer, which it freed before: no copy more on a miss). The key is the file as
opened, not its name: the file on disk (`qvr::files::identity`: Windows' volume and file index, elsewhere device and
inode), its last write time and size, the image's offset in it (a pak's entry) and its length, with the name looked up.
A file changed, replaced or touched, another game folder's or pack's file of the same name, another pak: another key,
and the name's old image is dropped when the new one is kept. The decoding has no settings (always RGBA); the textures'
settings act on the caller's copy afterwards. Least recently used first beyond `vr_image_cache_mb` (default 512; one
image at most a quarter of it; 0 none kept, given back). Main thread only, as `Image_LoadImage` (the hunk). Registered
with `mem::Cache` ("decoded images", in vr_memstats' largest).

| load (median of 3; images = "image files found and decoded" ms) | base | after |
|---|---|---|
| E1M1 on QRP, warm | 818 ms (images 505, material maps 126) | **321 ms** (images 38, material maps 54) |
| E1M1 on QRP, cold | 1489 ms (611) | 1299 ms (561) |
| E1M1 warm / restart | 197 / 191 ms (images 40, material maps 69) | 158 / 147 ms (6, 34) |
| hip1m1, then back to E1M1 (a campaign switch) | 985 ms (images 420) | **556 ms** (39) |
| warden warm | 391 ms | 351 ms |

Held: QRP's E1M1 330 MB (472 images with the start-up's). Checked: E1M1 on QRP warm with the cache and without, the
same picture (8 pixels of 518400 differ, by 8 levels at most, as between two runs without it); the shipped material
maps touched while the game ran (`touch quakevr/textures_quetoo/*`): the next load decoded those 94 again and dropped
their old images (93 dropped), the one after found them. `vr_image_cache_info` (Debug > Profiling and Memory: Decoded
Image Cache Info, and its size there), `vr_image_cache_clear`.

### 2. The memory log's row off the frame

**Why.** Each `vr_memstats_log` row (every 60 s by default, and 5 s after a load) took 2-12 ms of the main thread.
Timed by part (`developer 1` now prints each row's): GPU memory query (`glGetIntegerv` of GL_NVX_gpu_memory_info, which
waits for the driver's thread to drain) 0.05 ms on an idle E1M1 but 4.4-11.7 ms in `combined` (and once 199 ms on
E1M1); the CSV's open/append/close 0.3-1.6 ms; the rest 0.1-0.2 ms.

**How.** The GPU's memory is read with NVML (`nvmlDeviceGetMemoryInfo`: the device's total and free, every process's,
as NVX reports them: 24564 MB total, the same used within a few MB) on a worker of the game's pool, half a second
before the row is due (`gpustats::requestVram`; the row takes `latestVram()`); NVX's eviction counts (GL only) are
the ones counted at the map's load with the GL objects (`countGlForLog`, already a load-time GL scan). The file is
written by a pool task (the row's columns moved to it). Without NVML (AMD, Intel) the row asks GL as before. The
in-headset status line's VRAM figure (sampled each second while shown, the same glGet) reads NVML's the same way.

| `combined`, his settings, `vr_memstats_log 5` (4 rows a run, 2 runs) | base | after |
|---|---|---|
| a row's main-thread time | 1.2, 1.5, 5.1, 6.4, 7.6, 9.3, 9.3, 12.3 ms | 0.11-0.16 ms (each run's first: 0.5-1.2 ms) |
| ... of it the GPU memory query | 0.5-11.7 ms | none (NVML on a worker) |

The CSV: the same 103 columns, every row whole; VRAM used/total/free agree with NVX's (5236 vs 5303 MB used between two
runs, total 24564 both).

### 6. A campaign switch keeps the unchanged alias models (`vr_campaign_keep_models`, vr_modelkeep.cpp)

**Why.** A campaign switch (`COM_ReloadVRGame`: id1 to Scourge of Armagon and back, a map package) rebuilds the game
folders and Ironwail then forgets every model (`Mod_ResetAll`, `Cache_Flush`, `TexMgr_NewGame`): hip1m1 and back to
E1M1, 450-650 ms of alias models each way, though here the folders are the same both ways (quakevr, rogue, hipnotic,
id1: the campaign changes which start maps are skipped) and so are the files.

**How.** An alias model's load records every file it looks for, found or not (`COM_FindFile` while
`com_lookups_noted`: its .mdl, .md3/.md5mesh, each skin's external images and normal maps in every format; 10-40 a
model): the pak, its entry's offset and length, or the loose file's path and size, with the write time. At a campaign
switch, once the new folders are in, each lookup is made again (the directory listings cached for it,
`VR_FileCacheEnable`); a model is kept when every one finds the same file again and the ones found nowhere are still
found nowhere, and the palette's and colormap's files are the same (else none is kept). A kept model stays in its slot
(`Mod_ResetAll`; the emptied slots are reused by `Mod_FindName`), its cache entry (`Cache_FlushExcept`), its textures
(`TexMgr_NewGame`; the players' coloured skins, which it owns, are freed first: `R_FreePlayerTextures`) and its
buffers (`GLMesh_DeleteVertexBuffers`); the VR caches holding models empty as before (`VR_OnGameDirChanged`) and are
made again. The `game` command keeps none (it runs the new game's configuration, whose settings may load models
otherwise). The non-kept alias models' buffers are now freed at the reset (they leaked when not in the last map's
precache list).

| `load_hip1m1` (median of 3) | base | with the image cache (5.) | after |
|---|---|---|---|
| back to E1M1 (the switch back) | 985 ms (alias models 647) | 556 ms (271) | **308 ms** (12) |
| hip1m1 from the start (the first switch: nothing to keep) | 962 ms | 785 ms | 810 ms |
| hip1m1 again | 226 ms | 183 ms | 179 ms |
| E1M1 cold / warm / restart (`load_e1m1`, the recording's cost) | 743 / 197 / 191 | 712 / 158 / 147 | 720 / 161 / 147 |

The switch's check: 103-111 models, 3851-4212 lookups, 30-55 ms (without the listings cached: 520-660 ms). Checked: hip1m1
-> E1M1 -> hip1m1 kept 103/103 and 106/106, `vr_model_check` 0 wrong both; a file put in quakevr/ while the game ran
(`progs/soldier.mdl_0_norm.png`, an authored normal map the grunt's load had found nowhere): the next switch kept 91 of
92, the grunt loaded again with it; E1M1 after a switch with and without keeping, the same picture (12-13 pixels differ
by at most 8 levels, as between two runs). `vr_model_keep_info [model]` (the last switch; a model's recorded lookups),
Debug > Profiling and Memory: Campaign Switch Keeps Models, Kept Models Info.

### 4. The GPU wait (`GL_AcquireFrameResources`): measured, not changed

**Asked.** Less CPU burnt in the wait for the frame two back (`glClientWaitSync`: 4.7 of 8 s of the main thread in
`combined` under VTune, PROFILING_2026-10.md), with no latency added: else report.

**Measured with.** Two new per-frame figures in the benchmark JSON (BENCHMARKS.md): `pose_to_submit_ms`, from the
frame's pose (the runtime's frame begun, the tracking sampled) to its `xrEndFrame` (the fence wait lies between), the
latency's proxy; `main_mcycles`, the main thread's CPU cycles a frame (`QueryThreadCycleTime`: a spinning wait counts,
a sleep does not). Tried (not kept): the fence polled (`glClientWaitSync` with no timeout) with 1 a pause loop between
polls, 2 the same then `SwitchToThread` past 200 us, 3 the same with `Sleep(0)`; never a timed sleep. His settings, 90
Hz paced, exclusive, the modes interleaved, 3 runs each (2 for the old shadows):

| `combined` (median of runs) | frame p50 / p99 | pose to submit p50 / p99 | main thread Mcycles a frame | GPU 3D |
|---|---|---|---|---|
| 2048 eyes, driver's wait (as shipped) | 11.11 / 12.26 | 5.94 / 12.20 | 16.6 | 8.04 |
| ... polled, pause | 11.11 / 11.51 | 5.04 / 11.41 | 15.3 | 7.56 |
| ... polled, SwitchToThread | 11.11 / 12.43 | 5.31 / 12.34 | 15.4 | 8.02 |
| ... polled, Sleep(0) | 11.11 / 12.01 | 5.12 / 11.96 | 15.6 | 8.02 |
| 3072 eyes (GPU-bound), driver's wait | 16.80 / 21.42 | 16.23 / 21.36 | 14.9 | 13.71 |
| ... polled, pause | 18.75 / 23.04 | 18.33 / 22.94 | 15.6 | 15.78 |
| ... polled, SwitchToThread | 18.02 / 23.07 | 17.52 / 22.75 | 15.5 | 15.14 |
| ... polled, Sleep(0) | 18.20 / 22.84 | 17.61 / 22.72 | 15.4 | 15.25 |
| 2048, old shadows (`vr_shadow_layered 0`, the profile's state), driver's | 16.31 / 19.92 | 5.78 / 11.44 | 16.1 | 13.74 |
| ... polled, pause / Sleep(0) | 17.16 / 89.24, 15.93 / 20.27 | 6.34 / 15.32, 5.55 / 10.86 | 18.5, 15.5 | 14.89, 13.40 |

`idle_e1m1`: every mode 11.11 / 11.2-11.35 ms, pose to submit 0.60 / 1.0-1.2 ms, 2.7-3.2 Mcycles.

**Findings.** The main thread does not burn the wait today: with the driver's wait its cycles a frame are the same as
polling's (15-17 M, about 3 ms of a 5.5 GHz core, in 11-17 ms frames); a GPU-bound frame (3072 eyes) waits ~10 ms in
the fence and still uses 14.9 M cycles. So NVIDIA's `glClientWaitSync` blocks rather than spins here (VTune's 4.7 s may
have counted the wait as time in the function, or another driver state). With the layered shadows (`combined` no
longer GPU-bound at 2048) the fence wait is short; with the old shadows the frame waits in the runtime's calls and the
swap instead. Polling saved nothing measurable and, GPU-bound, every polled mode came out 1.3-2.1 ms later from pose to
submit at the median (and its GPU 10-15% slower: polling the driver may cost its thread, or the GPU's two clock states;
not separated). No mode is latency-neutral with a gain: nothing shipped (the measuring patch:
`scratch/gpuwait_experiment.patch` and `scratch/gpuwait_experiment_vr_gpuwait.cpp` in the perfbatch worktree). The two
benchmark figures stay.

## Dimension of the Machine (MG1): headless acceptance (2026-10-07)

What [EXPANSIONS.md](EXPANSIONS.md) ("Dimension of the Machine acceptance") records in full: entity coverage
(`check_mg1_entities.py`: 25 maps, 128 classes, 0 missing, 0 unknown keys), the whole campaign route through the
real exits at skill 1 and Nightmare (`mg1_route_test.sh`: 46/46 each), every MG1 monster in the MG1 context
(`mg1_monsters_test.sh`), the coop arena exit and intermission with two processes (`mg1_coop_exit_test.sh`), and the
Dopa/MG1/MG3 regressions. No MG1 gameplay bug turned up; MG1 `nativeReady` is flipped in its own commit (single
player; multiplayer stays on the developer path, as Dopa's).

Test-driver lessons (for whoever writes the next route test):
- **QuakeC's `changelevel` and `localcmd` append to the console buffer**: a script's remaining `wait`s run first, so a
  test that walks into an exit must end there. `vr_mg_route_stage` (default 0, not archived) runs
  `mg1route<stage>.cfg` on each new map's first frame instead (QC `MG1_H_RouteAutorun`, a `nosave` flag); setting it
  in a map runs that map's script at once, so set it before the `map`.
- **`kill` in single player restarts at once** (`ClientKill` -> `respawn`): a death test needs a real death
  (`vr_mg_hub_test 36`) and one release-then-press. A press held over several frames queues one `restart` a frame
  (stock QuakeC), each an autoload; the presser presses for one `wait`.
- An exit trigger's centre can be in a wall (the hub's gates) or behind a shut door: the walk takes a free spot
  inside the trigger (`tracebox` over a grid), else noclip, whose frame still touches the triggers it overlaps.
- QuakeC builtins take 8 parameters: `sprintf` with more prints garbage for the rest (the vr_mg3_test.qc:543 warning).
- Unexplained, harness only: `kill`, then a test request left in its cvar that spawned the presser in the restarted
  map, left the client at signon 3 ("load failed." after 60 s). Neither real play nor the route test does that.

## Dawn of the Machine (MG3): monsters (2026-10-07)

Phase D of [MG3.md](MG3.md) (M3-15..18; mg3d). Decision 5: Dawn of the Machine's monsters are there wherever
its data is (Debug > Tests > Ahead of You > Thing 30.., the training dummy's types), each with Quake VR's treatment.
MG3's sounds and models are read in place from the owned pack (`owned/mg3/...`, M3-11's `VR_OwnedFile`); a sound
precached as `owned/mg3/<path>` is now found too (`VR_OwnedFile` takes `sound/owned/<folder>/<path>`, the name
`S_LoadSound` asks for). Tests: Debug > Tests > Dawn of the Machine Monsters (`vr_mg3_mtest`, "mg3mtest:" lines).

### M3-15 The infected

`monster_army_infected`, `monster_knight_infected`, `monster_enforcer_infected`, `monster_hell_knight_infected`
(`QC/vr_mg3_infected.qc`; official `mg3_*_infected.qc`, `combat.qc`). Stock models, so any campaign. Each is the stock
monster (its classname the stock one's, as upstream: so every class-keyed table is his) marked `.vr_mg3_infected`.
Killed the first time (`Killed`, before it counts or does any of Quake VR's dying: no beheading, ragdoll, corpse or
gore pool), he bursts (the gib sound; the grunt's head thrown as a gib) and gets up as a zombie (grunt, knight) or a
fiend (enforcer, death knight): not upstream's modelindex swap but a real `setmodel`, the zombie's or fiend's box,
class, health, frame functions and knockdown, so the engine's rig, limb gore, hit tests and hulls all follow the new
body. Nothing of the old death carries over: an armed beheading cut is disarmed (`VR_Decap_After` would have taken the
new zombie's head), a knockdown ends (he stands up on the floor under his ragdoll's pelvis; `vr_ragdoll_list`: 0 left).
Counted once: the burst not at all, the zombie's or fiend's death as any monster's. Room for the new body: upstream's
`walkmove(0, 0)`, here on "partial ground" (a body in the air or off a step is not wedged), up to 24 units higher, 24
to a side (crowds: a monster standing over a knocked-down body, or pushed against him: 2 of 6 runs of request 2 failed
before), then in the old body's box (upstream never changes the box); wedged even so, he dies at once, gibbed and
counted, as upstream. Death knights placed as corpses (spawnflags
65536 / 8388608: MG3's `CORPSE_FLAG_A`/`_B`; map2 has 9, map2b 1, secret5 3) lie in their death's last frame, not
solid, until woken (their targetname or a hurt), then rise through their death backwards (MG3's `infected/death1_rev.wav`,
else the death knight's sight sound; tried again every 5 s while something stands in the way). Alive all the while: no
ragdoll or corpse.

Measured (`vr_mg3_mtest`): e1m1 (`vr_ragdoll 1`) request 2 **14/0**: four counted (23 -> 27), the grunt knocked down
first (a ragdoll) and the knight killed by an armed beheading cut: four turned, kills 0 -> 0, zombies 60 health
`'12 12 24'`, fiends 300 `'16 16 24'`, the knight's zombie keeps its head, the grunt's stands (knockdown 0, solid 3);
killed again: kills 0 -> 4, total 27. Request 3 **3/0**: lying (solid 0, frame 53, health 250), woken: risen (solid 3,
running), killed: a fiend. MG3 map2 (skill 3): infected to zombie 10, to fiend 10, lying 7; request 4 killed the 13
present: 13 turned, kills 0 -> 0. Checker after M3-15: 17 missing classes, 411 placements (the four infected classes,
214 placements, gone).

**In the headset.** [ ] An infected grunt or knight (Debug > Tests > Dawn of the Machine Monsters) killed by a sword
cut at the neck: he bursts and a zombie stands up with its head. [ ] map2's lying death knights rise when the fight
reaches them.

### M3-16 The rocket ogre

`monster_ogre_rocket` (`QC/vr_mg3_ogre.qc`; official `ogre.qc`'s `.aflag` branches, `OgreFireRocket`,
`T_OgreMissileTouch`, `ai.qc`'s sight, `client.qc`'s obituary). Quake VR's ogre (`orig_mon_ogre.qc`: frames, knockdown,
chainsaw, drops) drawn with MG3's model read in place (`owned/mg3/progs/ogre_rocket.mdl`; `self.wad`, as the
marksman's), marked `.vr_mg3_mon` 1, its class the ogre's (as upstream). Removed where the data is not
(`VR_Pack_RequireSpawn`). Its own: Armagon's voice (MG3's `armagon/idle1..4`, `pain`, `sight`, `sight2`; "rode a rocket
to hell"), a flinch only when `random() * 200 <= damage`, its pain 2 s longer, no chainsaw dropped (`VR_DropOgreChainsaw`
skipped: it holds a rocket launcher), and at range (`ogre_nail4`) a volley of two rockets: led at the enemy's place
after the flight at 1200, the second 64 units aside, 600 u/s; a hit 40 (zombie 60, shambler 20) and a 40 blast; within
0.2 s of its launch a 20 stab. The rockets are monsters' `MOVETYPE_FLYMISSILE` missiles: batted and parried as any
(`VR_Deflect_IsProjectile`). Never Honey's random multi-grenade boss outside the official campaigns.

**Frame order** (`Misc/quakevr/ragdoll/frameorder.py ogre ogre_rocket`: per-frame height of Quake VR's `ogre.mdl`
against MG3's): 147 frames, id's ogre's order (Quake VR's has 149: two appended). Both stand to 1.0 until 112, fall to
0.42/0.38/0.36 (theirs 0.47/0.46/0.46) at 123-125 ($death12-14), stand again at 126 and lie at 132-135 ($bdeath7-10):
the death ranges match.

**Rig** (`vr_ragdoll.cpp` `ogreRocketSeeds`, `Misc/quakevr/ragdoll/ogre_rocket_bones.json`; `RIG_PAK=<mg3>/pak0.pak
rig.py ogre_rocket`): 982 vertices, 13 bones as the ogre's (the launcher, a box and a long barrel, on the right hand),
clusters 1.06 units rms, bones 1.19 (Quake VR's ogre: 1.36, 1.52); deaths 112-125, 126-135. Head zone 10 forward, 33
up, 9 wide (`Misc/quakevr/ragdoll/headfit.py ogre_rocket 28 2 9`: middle 9.4 0 33.5, 9.7; at 31 and 9.5 a body shot
struck the head: the training dummy's zone test now head 1, body 0, legs 3, as the ogre's). An owned model is always
its `.mdl` (`gl_model.c`: no `.md5`/`.md3` replacement for `owned/` names: MG3's pack has `ogre_rocket.md5mesh`, whose
anim was refused with a warning; the rigs and edits are made for the .mdl's frames). Training dummy type 19 (Dummy
Enemy "Rocket Ogre" when MG3's data is there, `vr_dt_pack` 3); spawner Thing 34.

Measured: `vr_mg3_mtest 5` (e1m1) **7/0**: monster_ogre, the owned model, 200 health, its head zone; made to shoot: a
rocket in flight that `VR_Deflect_IsProjectile` takes, volleys of two (2 or 4 in 1.5 s: it shoots again on its own), the
player 500 -> 412; killed: frame 125, no chainsaw. `vr_limb_models owned/mg3/progs/ogre_rocket.mdl`: 13 bones, 11 limbs
(caps 0..19). `MON=34 decap_test.sh live corpse pop`: beheaded (12 parts left), head pops by shotgun and super shotgun.
`KINDS=34 limbs_test.sh live`: a slash cuts, fist/shotgun/bolt pop, an explosion pops 8, gibbed gibs; `MON=34 corpse`:
cut down to the torso. Knockdown (`vr_knockdown_test 0`, then 1): its ragdoll (982 vertices, 13 bones), up again (state
0, no ragdoll left). Dummy (`vr_dummy_type 19`, `vr_dummy_test 1`/`3`): "Rocket Ogre (monster_ogre) model
owned/mg3/progs/ogre_rocket.mdl", killed as it. MG3 map5 (skill 2): 12 rocket ogres alive; map1's 11 are all "not with
0 runes" (spawnflag 262144: none on a new game's map1, as upstream).

**In the headset.** [ ] A rocket ogre (Debug > Tests > Dawn of the Machine Monsters > A Rocket Ogre Ahead): bat its
rockets back with a sword or a gun; behead it; knock it down. [ ] Its ragdoll: the launcher stays in its right hand.

### M3-17 The demo dog

`monster_demodog` (`QC/vr_mg3_demodog.qc`; official `mg3_demodog.qc`, `client.qc`'s obituary). Quake VR's rottweiler
(`orig_mon_dog.qc`: frames, bite, leap and its parry, knockdown) drawn with MG3's model read in place
(`owned/mg3/progs/dog_explosive.mdl`), `.vr_mg3_mon` 2, its class the rottweiler's (as upstream), removed without the
data. Kamikaze: its leap landing on a player at speed (the rottweiler's bite of it, 10..20) kills it (upstream's 200
from the player), unless the leap was parried (Quake VR's stagger). However it dies, its pack spills: three grenades
(a fourth 30% of the time on Nightmare), tossed about and ahead, 60 each after 2.5 +- 0.25 s, owned by the dog,
catchable as a monster's (`VR_Grenade_Make`); "was blown up by a demo dog". It always bursts (gib3 x3 and its head),
as upstream; beheaded or a limb cut off by the killing blow it lies as its ragdoll instead (Quake VR's treatment, as a
beheaded zombie: `VR_Decap_ZombieDie`; the grenades spill all the same).

**Frame order** (`frameorder.py dog dog_explosive`): 86 frames in id's order, the death runs low at 12-16 and 20-25 in
both. **Rig** (`dogExplosiveSeeds`, `dog_explosive_bones.json`): 915 vertices, the rottweiler's 13 bones (keepHinge on
the lower legs, as his), the bomb pack's two barrels on the pelvis and the chest; clusters 0.58 units rms, bones 0.71
(the rottweiler: 0.62). Its head is the rottweiler's mesh (`headfit.py dog -9 15`: 259 vertices, 24.0 -0.3 -1.3 r 8.6;
the demo dog's 23.9 -0.4 -1.5 r 8.7): the rottweiler's head zone (23, 1, 7) as it is. Training dummy 20 ("Demo Dog"),
spawner Thing 35.

Measured: `vr_mg3_mtest 6` (e1m1) **6/0** (3 runs): a demo dog (monster_dog, the owned model, 25); shot dead beside a
grunt: 3 grenades, burst (its head), kills 0 -> 1; 3.2 s later the grunt 289 -> 187; its leap's landing on the player
(`Dog_JumpTouch` at 400 u/s): dead, 3 grenades, the player 500 -> ~390; beheaded by an armed cut: 3 grenades, lying
headless in its own model. Knockdown: its ragdoll (915 vertices, 13 bones), up again. `MON=35 decap_test.sh live pop`:
beheaded (head and jaw cut off, 11 parts left), popped. `KINDS=35 limbs_test.sh live`: a slash cuts, a fist pops,
gibbed gibs (the shotgun's pop missed: the earlier dog's grenades go off among the next ones). Dummy 20: "Demo Dog
(monster_dog) model owned/mg3/progs/dog_explosive.mdl", zones as the rottweiler's. MG3 map6 (skill 2): 18 demo dogs.
Checker: 15 missing classes, 178 placements.

**In the headset.** [ ] A demo dog leaping at you: parry it (it staggers, no blast) or let it land (it dies on you,
its grenades at your feet); catch one of its grenades and throw it back. [ ] Behead one: it lies headless, the grenades
still spill.

### M3-18 The ranged knight

`monster_ranged_knight` (`QC/vr_mg3_rknight.qc`; official `mg3_rknight.qc`, `ai.qc`'s sight sounds, `client.qc`'s
obituary). A class of its own (as upstream) with its own frame functions, MG3's model read in place
(`owned/mg3/progs/rknight.mdl`, the death knight's frame order), removed without the data. No melee; at range three
fans of MG3's glowing diamonds (`owned/mg3/progs/diamond_trail.mdl`), launched as knight spikes (9; 400 u/s, 600 in a
Bloody Nightmare game: batted, parried, blocked as any monster missile): a rising fan of five (magic a), six across
(magic c, the death knight's), or six then up to eight more scattered (magic b: 60% of its attacks from 300 units, else
20%). Spawnflag 2: it walks to fight. 250 health, gibbed under -40 (the death knight's head). Upstream's
`EF_CANDLELIGHT` on the diamonds is the rerelease's effect only (not in Quake VR's engine): left out.

Quake VR's treatment, wired by class or by its death function (`rknight_die`), as the death knight's: beheading's head
(`VR_Decap_HeadModel`), ragdoll settings (`ragdollClasses`: Ragdoll Settings > Death Knight), hull width
(`vr_mhull_hknight`), grapple weight 150, armour (`VR_Hit_Armoured`), corpse gibbing (`VR_Corpse_Parts`: the death
knight's head and corpse health), small gibs (the death knight's), highlights' score, knockdown (its death backwards,
`VR_Knockdown_SetupReverse` as the death knight's), the obituary ("slain by a Death Knight", upstream's). Its own:
**rig** (`rknightSeeds`, `rknight_bones.json`: 1155 vertices, the death knight's 14 bones, its right claw the hand; the
pauldrons on the upper arms; clusters 0.63 units rms, bones 0.77; deaths 42-53, 54-62; `frameorder.py hknight rknight`:
the same low runs at 51-53 and 61-62); **head zone** 7 forward, 33 up, 6.5 wide (`headfit.py rknight 28 0 6`: 7.7 -1.1
33.0, 5.9: a narrow helmet); training dummy 21, spawner Thing 36.

Measured: `vr_mg3_mtest 7` (e1m1) **7/0**: monster_ranged_knight, the owned model, 250, no melee; its head zone, the
death knight's head; made to cast (magic c): a diamond in flight (MG3's model, `VR_Deflect_IsProjectile`), 10 in 2.3 s
(its own next cast began), the player 500 -> 464; killed: frame 53; another gibbed: `progs/h_hellkn.mdl`. `MON=36
decap_test.sh live pop`: beheaded (13 parts left), head pops by shotgun, super shotgun, lightning. `KINDS=36
limbs_test.sh live`: a slash cuts, fist/shotgun/bolt pop, an explosion pops 8, gibbed gibs. Knockdown: its ragdoll (1155
vertices, 14 bones), up again. Dummy 21: zones head 1, body 0, legs 3 (the death knight's too). MG3 maps (skill 2):
map2 7 rocket ogres and 2 ranged knights, map2b 17 rocket ogres and 7 demo dogs, map4 14/2/5, secret5 9 rocket ogres
and 15 ranged knights with 26 + 9 infected (3 lying): every map loads, exit 0. Checker after M3-18: **14 missing
classes, 135 placements** (M3-15..18 resolved 8 classes, 1,262 placements counted from 411 before M3-16; the rest are
M3-19.. and the bosses).

Demo dog follow-up (M3-17's test): its grenades are counted as they go off (`vr_mg3_demo_blasts`) and the test reports
the nearest blast to the player instead of failing when a random toss lands behind a step (4 of 4 runs hurt him).

Checks (M3-15..18): QC 0 new warnings (the 3 left are `vr_mg3_test.qc:543`'s, from before), statics, QC precedence and
FGD (332 entities) pass; Release built. Regressions: e1m1 `vr_mg3_test 6` 13/0, Dopa e5m1 triggers 34/0 and world
19/0, MG1 hub 20/0 and horde 24/0, MG3 weapons `vr_mg3_wtest 2` 19/0, MG3 items (map2) 5/0, monsters 2/3/5/6/7 all
passing on e1m1 in one run. No melee code changed: `eval.sh` not run.

**In the headset.** [ ] A ranged knight's fans (Debug > Tests > Dawn of the Machine Monsters > A Ranged Knight Ahead):
bat the diamonds back, parry them; behead it (its helmet is small: the head zone is 6.5 wide).

Numbering (additive, to merge with M3-19..23's): spawner Things 30..36 (30-33 the infected, 34 rocket ogre, 35 demo
dog, 36 ranged knight), training dummy types 19-21 (`VR_DUMMY_TYPES` 22), `.vr_mg3_mon` 1-3 (`MG3_MON_*`,
`vr_mg3_ogre.qc`).
## Dawn of the Machine (MG3): monsters II, the orb to Bloody Nightmare (2026-10-07)

Phase D of [MG3.md](MG3.md), M3-19..23 (M3-15..18 are another worker's), by Vittorio's decision 5: MG3's
monsters spawn wherever its data is (any campaign: the Debug spawner, the training dummy), read in place from the owned
pack (`owned/mg3/...`). Numbers kept apart from the other worker's: the spawner's (`vr_test_spawn`, `func_enemy_dispenser`
`weapon`) from **40**, the training dummy's (`vr_dummy_type`) from **30** (19..29 left to theirs; the dummy's tried/available
bits now go past 24 types: a second pair of floats). Tests: `vr_mg3_btest N` (`QC/vr_mg3_bestiary_test.qc`, developer 1;
Debug > Tests > Dawn of the Machine Bestiary).

**Sounds read in place too.** `VR_OwnedFile` (vr_gamedir.cpp) now takes `sound/owned/<folder>/<path>` as that folder's
`sound/<path>`: a QC sound named `owned/mg3/orb/orb_pain.wav` (precache_sound and sound() put `sound/` before it) plays
MG3's file in any campaign. Model traits: `teleporter_eye*` and `shambler_blood.` are monster files
(`vr_modelmetadata.cpp` monsterFiles).

### M3-19 The orb

`monster_orb` (`QC/vr_mg3_orb.qc`, upstream `mg3_orb.qc` and its `ai.qc` branches): a flying eye
(`owned/mg3/progs/teleporter_eye_blink.mdl`: three frames, hover and two blinks), 300 health, the shambler's box. It
blinks and bursts 4-6 spheres (`owned/mg3/progs/rogue/sphere.mdl`, 18 each; 400/450/500 a second by skill, leading you a
quarter of the way) one to three times, then strafes or closes in (the scrag's attack check). **Its second eye**: it sees
you behind it in the same cone as ahead (`infront`); a blocked strafe turns it at you (`ai_run_slide`); sight sound
`boss2/sight.wav`. Hurt (8 s apart, the harder the likelier): eyes shut, three bolts of lightning about it
(`MG3_PainLightning`, upstream's `pain_lightning`, kept for the super shambler and the bosses; Quake VR's beam message
has the beam id byte). Killed: flung (monster_death_use takes its flight: it falls), and from its third death frame the
first thing it touches that is not a trigger nor a projectile (the floor, a wall, you) blows it up (`TE_EXPLOSION2`,
100 radius damage). Counted once. Its spheres pass through orbs, lava men and super shamblers (upstream).
- **Upstream's bug, fixed:** at RANGE_FAR its attack check called `wiz_run1` (left from the scrag it was made from): the
  eye ran the scrag's 14 run frames, which it has not, with the scrag's idle sounds, until its next check. Here
  `orb_run1` (the same 16-unit run).
- **VR:** no ragdoll (a rigid eye: no rig), no head zone, no limbs, no decapitation (none to cut); not a melee monster
  (no parry); grapple mass 120 kg (`VR_Grapple_MonsterMass`: between the small ones you reel in and the huge ones that
  reel you); small gibs and corpse damage as any unknown monster (it never lies as a corpse: it blows up). Its dead
  body can be grabbed while it falls and thrown: it blows up where it lands.
- **Anywhere:** the Debug spawner's Thing 40 "Orb" (Debug > Tests > Thing; `func_enemy_dispenser` 40: "Needs Dawn of the
  Machine (mg3)" without the data), the training dummy's enemy 30 (Weapons > Firing Range > Dummy Enemy, listed when MG3's
  data is there), FGD `monster_orb` (326 entities). Without the data a map's orb is not spawned (`VR_Pack_RequireSpawn`).

Tests (`vr_mg3_btest 2`, e1m1, the id1 campaign with MG3 owned) **11/0**: an orb ahead (its model, 300, counted once,
120 kg, dummy type 30 available); it flies; its eyes: ahead 1, behind 1, aside 0; woken, 21 spheres in 6 s; a hard hit:
its pain (lightning; the first run found the beam message lacking Quake VR's beam id: `Bad server message`, fixed);
killed: counted once, flung (-458 529 260), blown up where it landed (1), still counted once. MG3 map7 (`vr_campaign_native
mg3`): 11 orbs alive (the 12th has a rune flag: `NOT_IF_0_RUNES`); map1's 12 all have rune flags (later visits). Checker
after M3-19: 20 missing classes, 577 placements (21 and 625 before).

**In the headset.** [ ] Debug > Tests > Dawn of the Machine Bestiary > An Orb Ahead: its spheres (dodge them, bat them),
walk behind it (it still sees you), shoot it till it falls and blows up; grab its falling body and throw it at a monster.

### M3-20 Ghosts, sacrifices, the slime

- **`monster_ghost`** (`QC/vr_mg3_ghost.qc`, upstream `mg3_player_ghost.qc`; map1 4, secret5 3): a player's ghost
  (Quake's `progs/player.mdl`, no MG3 data needed) drifting about its home (a few stand loops, then a run of six frames
  at 150 to a point `wait` away, 128 by default), 10 health, flying, not a monster (not counted). A living player's
  touch lays it to rest, and in Quake VR **a hand's touch too** (`handtouch`); so does a blow: one of the player's
  five deaths, then a teleport flash and it is gone. (Upstream's fifth death ends on the fourth's last frame: kept.)
  Named `mg3_ghost_touch` (Honey has a `ghost_touch`).
- **`misc_sacrifice`, `trigger_sacrifice_counter`, `trigger_check_sacrifices`** (`QC/vr_mg3_sacrifice.qc`, upstream
  `mg3_sacrifice.qc`, `mg3_sacrifice_triggers.qc`; map7 1, map8 11 and map8's counter of 8): a hanging victim
  (`owned/mg3/progs/player_hanging_animated.mdl`, swaying through frames 5-75; spawnflag 2 `player_hanging.mdl`, bobbing
  16 units every 4 s and turning 36 degrees a second: upstream's `cos` takes degrees), 100 health (spawnflag 1: only
  its use gibs it), gibbed when used or killed, firing its targets. **Upstream's bug, fixed:** a victim killed fired its
  targets twice (Killed's `monster_death_use`, then the gib's own `SUB_UseTargets`): one shot down counted two on map8's
  counter. Here its death gibs it without firing them again. The counter says "N more" to every player (MG3's
  localized lines) and "complete" when it fires (once; never again).
- **`monster_slime`** (`QC/vr_mg3_slime.qc`, upstream `tarbaby.qc`; map6 1 (not in skills 0-1), map8 1, secret2 1):
  a spawn (`monster_tarbaby`, skin 0, no Rogue mitosis: Quake VR's spawn picks those at random outside the official
  campaigns) with two generations to come (`mg3_slime` 2). Blowing up (`tbaby_die2` calls `MG3_Slime_Split`) it throws
  two or three blobs (the vore's ball model, bouncing, 200 out and 200 up): where one lands it becomes a spawn (80
  health, hunting its parent's enemy) with one generation fewer; on something alive it blows up instead (120, radius);
  a child that starts in a wall goes off harmlessly. A child counts among the level's monsters only once started, so
  every kill matches the count. Quake's models: no MG3 data needed.
- **Anywhere:** Debug spawner 41 Slime, 42 Ghost, 43 Sacrifice (Debug > Tests > Thing; Dawn of the Machine Bestiary's
  rows); training dummy enemy 31 Slime (always listed: Quake's model; killed with Dummy Dies, it splits). FGD: 331
  entities.

Tests (`vr_mg3_btest 3`, e1m1) **16/0** (run three times): a ghost (not counted, flying, a hand's touch set) laid to rest
by your touch, another by a blow, both gone 2.5 s later; a sacrifice ahead struck down: gibbed, its target fired once
(the first run caught upstream's double firing); a counter of 3: 1 left after two, fired on the third, never again; a
slime: 2 blobs, 2 children started, counted (+3 monsters); killed: 4 blobs, each a spawn or a blast; the grandchildren do
not split; all dead: 6 killed of 6 counted. MG3 map8 (`vr_mg3_btest 4`) **3/0**: the 8 victims used in turn (as their
levers would), gibbed, the counter `human_killed` 8 -> 0, its target fired. Reports: map8 11 victims, the counter, 1
slime; secret5 3 ghosts; map1 4 ghosts (0 runes). Checker: 16 missing classes, 551 placements.

**In the headset.** [ ] Dawn of the Machine Bestiary > A Ghost Ahead: reach out and touch it with a hand: it dies and
fades. [ ] A Sacrifice Ahead: punch or cut it till it bursts. [ ] A Slime Ahead: kill it and its children (watch the
blobs arc and become spawns); the kill count should end even.

### M3-21 Dawn of the Machine's lava man

MG3's `monster_lava_man` is Rogue's "with a few tweaks" (upstream `mg3_lavaman.qc` and `combat.qc`): here Rogue's
(`rogue_lavaman.qc`) with `.mg3_lavaman` branches, set for every `monster_lava_man` in campaign 5 and by the new
`monster_lava_man_mg3` (`QC/vr_mg3_lavaman.qc`: any map, MG3's data needed). Rogue's lava man is untouched (r2m3's 4:
MG3's 0). MG3's:
- **Rises when its trigger fires** (a targetname; else at once: upstream dropped Rogue's Sleeping flag 2), the lava
  splash 50 below it (in the lava), and **stays at the lava's surface** (it flies: no drop to the floor).
- **Stands as it throws** (Rogue's steps forward on seven attack frames), its two lava balls from lower hands (90 up,
  65 aside; Rogue's 130/125, 65/75).
- **Its first hit always staggers it**, with Chthon's pain cry (then 5% a hit, 2 s apart, as Rogue's).
- **Takes 0.8 of a blow** but the lightning gun's bolt and the laser cannon's (upstream: the attacker's selected weapon;
  here what struck: a laser bolt, or a shot from a hand holding the lightning gun), **nothing from Chthon**
  (`MG3_LavaManDamage`, from `T_Damage`).
- **Used again once risen it dies** (counted, its targets fired: upstream `lavaman_force_death`); sinks without Rogue's
  blast. Model MG3's own, read in place (`owned/mg3/progs/lavaman.mdl`: no Dissolution of Eternity needed).
- **VR:** a head zone (MG3's model, `$walk1`: 31 forward, 70 up, radius 13; Rogue's has none); grapple mass 3000 (both);
  no ragdoll (it sinks), not a melee monster; lava never burns it (`vr_liquids.qc`, both). Obituaries "was burned to a
  crisp" (MG3's) and, with this commit, MG3's for the slime ("was slimed"), the super shambler and the orb.
- **Anywhere:** Debug spawner 44, training dummy enemy 32 "Lava Man (Dawn of the Machine)" (listed with MG3's data).

Tests (`vr_mg3_btest 5`, e1m1) **13/0**: MG3's ahead (model, flying, 1500 health at skill 1, counted, a use kills it,
its head zone) and Rogue's beside it unchanged (walks, Rogue's model, no head zone); a blow 80 of 100, its first hit
staggers it (frame 80); a laser bolt and a lightning shot 100; nothing from Chthon; Rogue's takes 100; its throw (forced:
e1m1's start walls hide you from where it walks) puts a ball in flight; it never fell; used again: dead, counted once,
sunk and gone. MG3 map2b (`vr_mg3_btest 6`) **4/0**: its 2 lava men waiting for their trigger, risen (flying, MG3's
model, after you), used again: dead, counted, gone. FGD: 332 entities.

**In the headset.** [ ] MG3 map2b's lava men: they rise from the lava, throw standing; the first hit staggers them.
[ ] Bestiary > A Lava Man Ahead: aim at its head (the zone: Debug > Show Hit Zones).

### M3-22 The super shambler (blood shambler)

`monster_super_shambler` (`QC/vr_mg3_supershambler.qc`, upstream `mg3_super_shambler.qc`; map2, map4, map8, secret5 x2):
the shambler's frames on MG3's model (`owned/mg3/progs/shambler_blood.mdl`: 1076 vertices, 96 frames, unnamed; the
shambler's 94 first), 2000 health. Up close it smashes (80%) or claws; each smash sprays 11 plasma balls in a fan
(`owned/mg3/progs/rogue/plasma.mdl`, the knights' spike's touch: 9) and each claw 9 in a block, thrown up or down at you
by height and distance; after three or four blows (Bloody Nightmare one fewer) its lightning. From 110 to 200 units it
claws by the side you are on; further off (and its lightning rested) it casts: a long cast (60%, then 5 s) or a fast one
(3 s), each three bolts reaching 600-1000 units (the shambler's 600), the glow over its hands while it winds up. Half
damage from blasts and rockets as the shambler (`VR_IsShambler`: radius damage, rockets, Rogue's multi-rockets; its
lightning a zap wound from afar). Gibbed below -60 (the shambler's head gib). Dying, the super shamblers it owns die too
(upstream `cleanup_orbs`). Dropped: the KEX achievements. Upstream pathfinding (bot nav) has no Quake VR counterpart:
it moves as the shambler.

**The model is the .mdl.** Ironwail loaded MG3's KEX `shambler_blood.md5mesh` beside it (enhanced models): skeletal
poses that the QC frames, the hit model and a vertex-animation rig cannot read ("not the model its seed table was made
for", now with the reason: pose type 1). An owned expansion's model read in place now refuses its md5 companion
(`VR_ModelReplacementOk`, vr_handrig.cpp: `owned/` paths), so it draws, hits and ragdolls as its .mdl (the plan's "keep
that").

**Its rig** (`vr_ragdoll.cpp` `shamblerBloodSeeds`, `Id::Mg3ShamblerBlood`): the shambler's 13 bones (pelvis, chest,
head, upper arms, forearms (hinges), claws, thighs, shins (hinges; capsules 5.5, 5)), fitted with
`RIG_PAK=<MG3 pak0> rig.py shambler_blood 24` then `shambler_blood_bones.json` (Misc/quakevr/ragdoll): clusters 1.29 units
rms, bones 1.77 (the shambler's 1.21 / 1.57: the hi-poly model's hump and claws bend more). Four seed centres set by hand
where the fit's would hand a cluster to another bone (rig.py's warnings: the belly's front to the head, the hip to the
left thigh). Death frames 83-93. Its ragdoll settings, head gib, knockdown chance, corpse health, small gibs, hull
width: the shambler's (`ragdollClasses` row, `VR_Knockdown_SetupReverse`, `VR_Corpse_Parts`, `VR_SmallGib_Mult`,
`monsterClasses`). Head zone 30 forward, 53 up, radius 13 (its face at the hump's front: the head bone's vertices 33-72
up); parried as a melee monster; grapple 800 kg.

**Anywhere:** Debug spawner 45, training dummy enemy 33 (with MG3's data), Bestiary rows (A Super Shambler Ahead, ... Its
Ragdoll There, the test). FGD 333 entities.

Tests: `vr_mg3_btest 7` (e1m1, ragdolls on) **10/0**: spawned, counted, its head zone and gib, parried, 800 kg, dummy 33;
started: a rig, knocked down as a shambler; a blast at it: half (50 of 100 at most); its smash 11 plasma balls, a claw
9, a fast cast 3 bolts and its glow gone; killed: counted, a ragdoll ("rigged: 13 bones, clusters 1.29, bones 1.77 units
rms, 43.7 ms"; 13 parts asleep in 11 s, 280 kg, nothing below the floor). `decap_test.sh live pop` (MON=45): a slash
beheads it (its head thrown); shotgun, super shotgun, overkill and lightning headshots that kill pop the head; body shots
don't; a left claw cut off on the way. MG3 map8 (`vr_mg3_btest 8`, save, load, `9`) **2/0 + 2/0**: its health relay
(read before a blow, as upstream: the blow after the threshold fires it) fired at 899 of 2000; after the save and load
899, its model and death; killed, counted once. map2's has rune flags (later visits). Screenshot (kit scratch
`ss_orb.png`): the blood shambler and an orb in e1m1. Checker: 15 missing classes, 546 placements.

**In the headset.** [ ] Bestiary > A Super Shambler Ahead: parry its claws, dodge the plasma sprays, its lightning from
afar; shoot its head (the zone), behead it with a slash; kill it and push his ragdoll about (Ragdoll Settings > Shambler).

### M3-23 Quake's monsters in Dawn of the Machine, and Bloody Nightmare

Upstream's changes to the stock monsters (MG1 -> MG3 diffs: soldier 28 lines, enforcer 106, hknight 39, wizard 42,
shambler 74, shalrath 89, zombie 18, ogre 306 (most of it the rocket ogre: M3-16's), tarbaby 171 (the slime: M3-20),
the rest Horde fades), campaign 5 only (`QC/vr_mg3_bn.qc`, one call each in `orig_mon_*.qc`, `combat.qc`, `items.qc`):
- **No backpacks** from grunts, enforcers and ogres (`DropBackpack` drops none for a monster in campaign 5; Quake VR's
  enemy weapon drops stay).
- **Skill 3:** grunts (100) and enforcers (200) shrug off light blows (no pain when random() x that > damage); the
  nightmare pain rest (5 s after a pain) no longer holds zombies (they must flinch to go down) and Chthon; shamblers
  cast lightning after three or four blows (`MG3_ShamblerCasts`, reset as they cast).
- **Vores' balls** (any skill) steer every 0.1 s (Quake's 0.2), nudged apart from each other within 32 units, at 285 on
  skill 3 (Quake's 350, MG3's "bringing back the pain").
- **Zombies:** spawnflag 8388608 hangs one (still in its first pain frame, as the crucified; secret3's 2), spawnflag 128
  throws flesh only up close (secret5's 7).
- **Bloody Nightmare** (`MG3_BloodyNightmare`: campaign 5, serverflags 64): an enforcer fires a second laser beside each
  (15 damage, 600, alternating sides); a death knight's spikes fly at 500 (300); a scrag's spikes bring one or two more
  each (MG3's `w_spike2.mdl`, read in place); an ogre's volley brings two more grenades 70% of the time, 0.3 s apart,
  thrown higher or lower by where you are and up to 64 aside (upstream loops the volley's frames; here a helper throws
  them while the ogre lives: its frames untouched, M3-16 edits them).
- Not ported: the fish's idle sound rate limit (sound only), upstream's commented-out vore trail, the hell knight's
  infected offset fix (M3-15's).

Tests (`vr_mg3_btest 10`; `11` sets Bloody Nightmare): MG3 map1 at skill 3 in Bloody Nightmare **11/0**: no backpack;
the grunt and the enforcer flinched 0 of 20 light blows; the fiend's pain rest 5 s, the zombie's none; the shambler cast
after 4 blows; 1 more laser; a spike at 500; the vore's ball steered at 285 every 0.1 s; six scrag spikes brought 4 more;
the ogre's burst 2 more grenades; a hanging zombie; a close-thrower. MG3 map1 at skill 1 (no Bloody Nightmare) 11/0;
e1m1 at skill 3 (the id1 campaign: none of it) **10/0** (a backpack, 20 of 20 flinches, both pain rests 5 s, no cast,
no extras, 300). Regressions: Dopa triggers 34/0, MG1 hub 20/0, MG3 weapons 19/0, e1m1, hip1m1 and r1m1 load clean.

**In the headset.** [ ] Dawn of the Machine in Bloody Nightmare (the hub's skill 4): enforcers' double lasers, ogres'
triple volleys, scrags' spike fans; zombies flinch at every hit on skill 3.

### Checks after M3-19..23

Checker: **15 missing classes, 546 placements** (21 and 625 before M3-19: `monster_orb` 48, `monster_ghost` 7,
`misc_sacrifice` 12, `trigger_sacrifice_counter` 1, `monster_slime` 6, `monster_super_shambler` 5 resolved). Release
build, QC 0 warnings (the 3 of `vr_mg3_test.qc`'s 10-argument sprintf fixed in M3-19), statics, QC precedence and FGD
(333 entities) pass. `eval.sh` not run (no melee change). Test saves in the worktree's game folder: `mg3btss.sav`.

## Dawn of the Machine (MG3): the Shub finale (2026-10-07)

Phase D of [MG3.md](MG3.md), M3-26/27 (M3-24/25, Chthon, are another worker's). Numbers kept apart from
theirs: the Debug spawner's from **60**, the training dummy's from **50** (the dummy's tried/available bits: a third pair
of floats for 48 on). Tests: `vr_mg3_shubtest N` (`QC/vr_mg3_shub_test.qc`, developer 1; Debug > Tests > Dawn of the
Machine: Shub): 1 report, 2 phases and children, 3 zombies and pillars, 4 death and the credits, 5 the training dummy.

### M3-26 Shub and her children

`monster_oldone_new` (`QC/vr_mg3_shub.qc`, from upstream `monsters/mg3_oldone_new.qc`): id's Shub model, 12000 health,
killable. Her 46-frame loop faces you and attacks on frames 14/29/44: a volley of diamonds (one row; two in phase 4),
every third attack her phase's child in turn (1: spammer, swiper; 2: vortex, swiper, spammer; 3: blasters, three
swipers; 4: spammer, seeker, swiper, blasters); her autoguns on frames 21-25 and 36-40 (the c-th of a run up to her
phase + 2 and skill + 1, none on skill 0); a shub zombie on 23 and 45 (M3-27). Phases: her first wound 1, then under
3/4, 1/2, 1/4 of her health 2, 3, 4 (a seeker at 2; her `wave1` key's target fired at 2, `wave2`'s at 3 and at 4, as
upstream: boss2 sets only wave2). Each change: her thrash (1.5 s, no damage taken: `T_DamageDeal`'s early return on
`.mg3_shub_immune`, upstream combat.qc's boss_immune) ending in a sphere of 100 spheres (skill 3: as many volleys as
her phase). Children: the spammer (13 plasma lobs; each a splash where it lands, 2 s later a lightning bolt and a blast,
100 radius, Shub included, as upstream), the swiper (a lightning beam swept 180 degrees in 1.3 s: 25, 100 to a shub
zombie), the blasters and the vortex (eyes, `teleporter_eye.mdl`, 120 health, counted as they appear, 72 spheres
spiralled at you then a burst; where one appears anything is telefragged, a player or Shub there kills the newcomer),
the seeker (300 health, chasing, its touch 500, not counted). Killed: immune, her children told to finish (the
upstream spammers and swipers, left thinking nothing, now removed), a zombie cleaner (one a 0.2-0.7 s), a last sphere,
three thrashes, the lights dimmed to `a`, then 50 gibs up and about, lights back, CD track 3 and `MG3_ShubEnding`
(M3-10: `$map_dopa_endtext_final`, next map start, the credits). A single player dead by then completes nothing (she
idles, as upstream). VR: her and her eyes' beams carry Quake VR's beam id byte (`VR_ParseBeamEntity`); rerelease
`EF_CANDLELIGHT` (64) is masked out by the engine for this progs (`PR_FindSupportedEffects`) and not set; no shove or
knockback moves her (`VR_Push`), 10000 kg to the grapple, out of liquid rules, an obituary ("became one with
Shub-Niggurath", her eyes too); a grenade striking her deals its damage to her and blasts round her (upstream
weapons.qc; her box is far bigger than her body). Rigid models, no rig: no ragdoll or head zone (as the orb).

**Anywhere** (decision 5): Debug spawner Things 60 Shub (free: `.mg3_shub_free`, killed she bursts but ends nothing), 62
an eye (`monster_shub_eye`), 63 the seeker (`monster_shub_seeker`); training dummy 50 Shub (free), 51 her eye. She
raises zombies only where the map has `info_szombie_spawn`.

Measured: boss2 (`vr_campaign_native mg3`) `vr_mg3_shubtest 2` **31/0**: her model, 12000, 10000 kg, awake, unmoved by a
700 shove, phases 1-4 at 11990/8990/5990/2990 with the thrash's immunity (a 100 hit ignored), a seeker at 2, waves 1/2/3,
each phase's rotation of children exact, a volley between, none while immune, autoguns 2 of 5 at skill 1, 5 eyes
counted; spammers' 52 plasma (13 each), the swipers gone, a fresh blaster pair spiralling spheres, one killed and one
spent (72 shots), both counted dead, every plasma accounted for (blown up where it landed, on something alive, or still
about). e1m1 (a free Shub ahead) **31/0**. `vr_mg3_shubtest 4` boss2 **9/0**: killed, counted once, immune, her
children gone, 5 s of thrashes, 50 gibs, intermission with next map start and the final text, the presses to the text
and on: "Credits: native campaign end presentation opened" (a run left at the menu); e1m1's free Shub ends nothing.
Training dummy (`5`, e1m1) **6/0**. Checker: boss2 2 missing (`func_breakable` 8, `info_szombie_spawn` 36: M3-27), all
maps 7 missing classes, 55 placements (8 and 56 before). Regression: `vr_mg3_btest 2` 11/0, `7` 10/0, `vr_mg3_mtest 2`
14/0, MG1 hub 20/0, e1m1 smoke exit 0; QC 0 warnings, statics, precedence, FGD (343 entities) pass. Found while testing: the eyes of one attack rotation run at once share their spots
and telefrag each other (upstream's spawn_boss_tdeath), so the test kills them before their next frame.

### M3-27 Her zombies, the pillars, the ending

**Shub zombies** (`QC/vr_mg3_shub_zombie.qc`, from upstream `monsters/mg3_shub_zombie.qc`): she raises one on frames 23
and 45 of her loop at one of the map's `info_szombie_spawn` (boss2: 36, in a ring 163-245 units round her), none past 33
alive; a point used rests 8 s, and the pick is upstream's (a random index under the count of rested points, counted
over all). A shub zombie is Quake VR's `monster_zombie` with `.mg3_szombie`: the zombie's rig, ragdoll, head zone,
beheading, limbs, knockdowns, chainsaw and Super Axe rules come with it; it lies where it rose (`$paine13`, not solid)
and gets up 7 s later where there is room (the zombie's own paine11/12), hunting you, its flesh thrown only up close
(Quake VR's MG3 close-throw zombie: upstream's melee "missile"). Her spheres pass it; its flesh passes her and her eyes
(`ZombieGrenadeTouch`, upstream SzombieGrenadeTouch); her swiper does it 100; her death's cleaner gibs them one by one.
Killed whole it throws its head too (upstream's shub zombie threw none). Debug spawner Thing 61 (`monster_szombie`).

**The "ceilings" are pillars.** boss2's 8 `func_breakable` (upstream in `mg3_oldone_new.qc`) are columns round her, 206
to 494 units tall, some with items on top (red armour, shards). Any damage sinks one 20 units (20 a second for a second
from its last hit), its 10000 health restored each hit (only a single 10000 blow, a telefrag, removes one). Her
diamonds, spheres, beams and plasma blasts hit them all fight long, so they come down over it: after `vr_mg3_shubtest
2` and 70 s more of her phase 4 (god mode, at the arrival point) they had sunk 189 to 477 units. Nothing raises them;
standing on one you go down with it, and when its top passes the floor the floor holds you (Quake's pusher, as upstream):
no trap. The plan's "lower on phases" is this: no phase drives them.

**The ending** (M3-26's death; M3-10's `MG3_ShubEnding`): see above; `vr_mg3_shubtest 4` now also raises two zombies
first and finds them gibbed by her cleaner.

Measured: boss2 `vr_mg3_shubtest 3` **14/0**: 36 spawns unused, 8 pillars (solid pushers, 10000); one raised (lying at
a spawn point now resting, 60, counted, after you), at most 33 alive (40 asked), her sphere and its flesh pass each
other, 20 of 33 up 8 s later (the rest wait for room: several share points), a 100 kills one, all killed and counted;
a pillar hit sinks 20 and stops, a blast beside one sinks it, you on pillar `*52` ridden down 17 hits until its top
passed the floor, then on the floor (feet -15.97, on the ground, not in solid). `vr_mg3_shubtest 4` **10/0** (her
zombies gibbed), the credits reached. A live fight (test 2, then 70 s, god mode): no error, 10 shub zombies up, 618
edicts. Debug spawner 61 on e1m1: a shub zombie, counted. Checker: boss2 **0 missing**; all maps 5 missing classes, 11
placements. Regression: `vr_mg3_btest 2` 11/0, `7` 10/0, `vr_mg3_mtest 2` 14/0, MG1 hub 20/0, e1m1 smoke exit 0; QC 0
warnings, FGD 346 entities. For VR QA: the pillars sinking under you (comfort), her zombies' rise and their rigs, her
beam and blasts, the lights going out at her death.

## Dawn of the Machine (MG3): the Chthon finale (2026-10-07)

### M3-24 Chthon: spawn, fits and phases, waves, the second arena

`monster_boss_final` (`QC/vr_mg3_chthon.qc`, upstream `boss_final.qc`; boss.bsp): id's Chthon (`progs/boss.mdl`, classname
`monster_boss` as upstream, `.mg3_chthon` 1) waits under the lava until his map's trigger uses him (`chthon_count` ->
`chthon`, which also opens the arena's doors and plays track 12), then rises at whoever woke him: 12000 health. Killable
(spawnflag 2, the map's): fans of MG3's spheres from either hand (4/8/11/15 by skill, along the ground), and from his
second phase on, after a throw, a volley 30% of the time (15-25 blasts of 0-6 spheres on skill 0 .. 30-40 on 3, at 10 a
second, leading you a quarter of the way, spreading as it goes); without spawnflag 2, id's lava balls. Six fits as he
is hurt (first blood, then 83, 66, 50, 33, 16%: shock a or b, lightning from his body), immune during each, none while
he rises or throws a volley (skill > 0), several thresholds in one blow give one fit (upstream). After a fit his phase
steps up: 2 fires `wave1` (`rein1`: relays open the doors of the knights' pits 1-7 s on), 3 fires `tele_target`
(`boss_tele1`) and moves every player to his own `info_boss_teleport_first` point (popped up, turned, a teleport flash,
what his hands carry along: `VR_Carry_Teleported`) while Chthon sinks (frame 0), frozen and immune, until
`trigger_boss_teleport` (the lower room's countdown, 10 + 5 s after the player lands) moves him to
`info_boss_teleport_boss` (he rises there) and the players to the `info_boss_teleport_second` points; 4 fires `wave2`, 5
`wave3` and the spiral (four arms of plasma from above his head, 5 degrees a step every 0.15 s, 0.2 on skills 0-2, half
the steps skipped on 0-1, while he lives). Damage (killable): none during a fit or from lava men, 0.8 of anything but the
lightning gun's bolt and a laser cannon's (`MG3_ChthonDamage`, read by T_Damage as the lava man's); a grenade that
touches him puts its whole blast into him (upstream GrenadeTouch: his middle is 116 units above his feet). Killed:
counted once (upstream counts him twice), three rings of 72 spheres (1, 3, 5 s on), he sinks, his targets used up to
five times in all (upstream), 49 gibs up and out over the lava (upstream get_org's directions: Quake VR's gibs, grabbable,
sticking, bleeding).

**Fixed on the way (the map needs them):** MG3's `trigger_teleport` spawnflag 8 (with a targetname: off until its first
use; upstream 2026) was missing: the lower room's teleporter (`tele_c2`) was on from the start and threw the player
straight into the second arena, skipping the countdown; boss.bsp is the only map that uses it. MG3's
`SPAWNFLAG_NO_CONTENTS_DAMAGE` (16384) was missing: the 48 knights of the three waves stand in lava pits behind the
doors and burned to death before the fight (`VR_Liquid_Immune`, campaign 5; MG1's source has the flag too, left as it
was).

**Quake VR:** a head zone (80 forward, 240 up, radius 45: `PositionalHead`, his head and jaw on walk1, 210-276 up); the
precise hit model's standing pose for models that begin under the lava (frame 0 `rise*`, no `stand*`: id's boss.mdl and
MG3's lavaman.mdl, whose M3-21 head zone was fitted on walk1) is now their first `walk*` frame (`vr_hitmodel.cpp`
`restPoseOf`); no head to cut off, no ragdoll (he sinks, then bursts); his spheres are any monster's missiles (batted);
grapple 5000 kg and lava immunity as id's (classname). The KEX candle light on every other sphere is not drawn (no such
effect here). **Anywhere:** Debug spawner 50 (killable, woken at you once placed; `mg3_chthon` 2: never the ending),
training dummy 40, Debug > Tests > Dawn of the Machine: Chthon. Without the second arena's points (any other map) phase 3
fires `tele_target` and he fights on where he is.

Tests (`vr_mg3_ctest`, developer 1): `2` in e1m1 **43/0**: the spawner's (killable, counted, head zone 80/240, no
decapitation, 5000 kg, dummy 40), woken: rising (nothing while he rises); a plain blow 80 of 100 and fit 1 (immune,
nothing during it); phase 1: the lightning gun's bolt 100, nothing from a lava man, 8 spheres thrown; shots on his drawn
model: zone 1 at his head, 0 at his chest (`hitmodel_segment`); fits 2-6 at 9837, 7797, 5877 (shock b), 3837, 1797, each
phase's target fired once (wave1, tele_target with no arena: he fights on, wave2, wave3 + the spiral); killed: counted
once, gone under, 49 gibs, a ring of 72 spheres and two more to come, his target used once (every fit had), no ending
9.5 s on; a second one killed before any fit: his target used 5 times. `3` on boss.bsp (`vr_campaign_native mg3; map
boss`) **42/0**: his waves and teleport target, 4 + 4 + 1 points, trigger_boss_teleport BOSS on `tele`, 17 lava suits at
skill 1 (of 21), the player walked into the trigger: woken at him, track 12; every fit; wave1: 12 of rein1's doors open;
phase 3: the player at a first point, he sank, immune; 15.6 s on (the countdown) he rose at his point (2496 2400 72) and
the player at a second point; wave2 and wave3: 9 doors each; killed: 49 gibs, the first ring. Checker: boss **0 missing**;
all maps **3 missing classes, 45 placements** (8 and 56 before). Regressions: `vr_mg3_btest 2` 11/0, `7` 10/0,
`vr_mg3_mtest 2` 14/0, MG1 hub `vr_mg_hub_test 1` 20/0, e1m1 smoke; QC 0 warnings, statics, FGD (345 entities).

**In the headset.** [ ] Debug > Tests > Dawn of the Machine: Chthon > Chthon Ahead (an open map): dodge his sphere fans
and volleys, shoot his head (the zone), bat a sphere back, throw a grenade at him; his fits' lightning; kill him: the
rings, the gibs flying over you.

### M3-25 Chthon: the boss teleport's comfort fade, the music, the ending

- **Comfort fade** (`Quake/vr/vr_comfortfade.cpp`, new): `vr_comfort_fade [seconds]` turns the view black and brings it
  back over `vr_comfort_teleport_fade` seconds (default **0.6**, 0 off; Locomotion > Comfort > Fade on Scripted
  Teleports), in real time, drawn as the bonus colour shift (the eyes' blend, as the lightning-in-water flash), its last
  share cleared at the end rather than left to the bonus flash's slow decay; `vr_comfort_fade_info` prints its state.
  QC `VR_ComfortFade(p)` (vr_mg3_chthon.qc) stuffs it to a human player's client; Chthon's moves of the players (phase
  3 to the first points, trigger_boss_teleport to the second) use it. Measured: a 1 s fade 0.56 s in: 114 of 255
  (alpha 0.45); after it 0.
- **Music:** the map's `trigger_music` (track 12) is on `chthon` with Chthon's wake (M3-05's trigger; upstream changes no
  track at his death).
- **The ending:** the map's Chthon (`mg3_chthon` 1) in Dawn of the Machine, once sunk (death10), calls M3-10's
  `MG3_BossEnding` 8 s on (upstream boss_end): the finale text `$mg3_qc_boss_finale`, then the credits (`start`), or in a
  Bloody Nightmare game its new game (map1, serverflags 448, upgrades cleared); a dead single player completes nothing.
  The Debug spawner's Chthon, or a map's elsewhere, ends nothing. Shub's death (M3-27, `MG3_ShubEnding`) is the Bloody
  Nightmare new game's own end (boss2); Chthon's Bloody Nightmare branch leads to it through map1 .. hub -> boss2.

Tests: `vr_mg3_ctest 3` on boss.bsp **46/0** (M3-24's 42, the two fades, the finale text in the intermission, next map
`start`); `vr_mg3_test 14` then leaves the intermission: the credits (campaign 5's native completion, M3-10). `vr_mg3_ctest
4` (the same fight in a Bloody Nightmare game: serverflags 64 + 128 set first) **46/0**: the blows at 0.8 (64 of 100),
the ending: next map map1, serverflags 448 (`vr_mg3_test 14` leaves the intermission towards it). `vr_mg3_ctest 2` (e1m1) 43/0
(no ending). Rebased onto the Shub finale (M3-26/27): checker **0 missing classes, 0 placements** on all 22 maps;
`vr_mg3_ctest 2/3/4` 43/0, 46/0, 46/0, `vr_mg3_shubtest 2` 31/0, `vr_mg3_btest 2` 11/0, `vr_mg3_mtest 2` 14/0, MG1 hub 20/0.
Headless note: the kit's `wait N` (with a space) is one frame; `waitN` is N frames.

**In the headset.** [ ] Dawn of the Machine's boss map (or Debug > Tests > Dawn of the Machine: Chthon > The Boss Map's
Fight): at his third fit you are moved to the lower room, and 15 s later to the second arena: the view goes black and
comes back (Locomotion > Comfort > Fade on Scripted Teleports: try 0.6 and 1.2 s); kill him: the finale text, then the
credits.

## vrstart2's load: profiled and fixed (2026-10-07)

Your request: vrstart2 takes very long for a cold start; profile it (VTune) and optimise map loading. Measured with
`vr_startup_times` (run.sh, fast, exclusive; median of 3) and the new bench scenario `load_vrstart2` (cold, warm,
restart: `bash kit/bench.sh <agent> --scenarios load_vrstart2`); VTune with `qvrprof.sh ... load_vrstart2` (MODE=2,
loads only).

| load (ms) | before | after |
|---|---|---|
| cold, every disk cache empty (first start ever) | 20,800 | 8,800 |
| cold, hull files not there yet (a new build of vr_hull.cpp) | 20,500 | 8,800 |
| cold, disk caches there (every later start) | 20,500 | 1,180 (bench: 1,040) |
| warm (`map vrstart2` again) | 2,405 | 310 (bench: 283) |
| restart (a death's reload) | 2,372 | 275 (bench: 261) |

Where it went (VTune, the three loads, 256 s of CPU): the world's hull trees (the player's 16 wide and the monsters'
24, 28, 40: four trees of 1.2-1.4 million nodes from 33,682 brushes, built at once on 32 threads, 16.4 s waited), of
which 94 s in VirtualAlloc/VirtualFree (mimalloc giving freed pages back at once, `vr_heap_purge_delay 0`, and asking
for them again: 32 threads queueing on the kernel), then `choose`, `clipWinding`, `splitPoly`, `finish`; and the
liquids' wave mesh, 2.16 s on every load (warm and restart too).

Fixed, a commit each (the outputs the same bytes: tree hashes, the mesh's, the ledges', `vr_hull_keeptest` PASS):

1. **Liquids' wave mesh**: the rim samples' "inside another face" test and the pins' nearest rim segment looked up
   in xy buckets (`XYBuckets`) instead of every face or segment of the group: 2,160 to 118 ms a load.
2. **Heap held during loads** (`vr_heap_load_hold`, default 60000 ms; Debug > Memory, Heap: Hold During Loads): from
   a load's start to its first frame drawn the purge delay is raised, so the load's freed memory is used again; at
   the end all of it is given back (`mi_collect(true)`) and the delay put back. Cold 19.4 to 10.9 s; mimalloc's
   committed memory after the load the same (2,555 MB against 2,638).
3. **choose()**: the pieces' planes counted in a table the size of their faces (the pool's ~2,000 builders a tree
   each made and zeroed 5 MB arrays of all the tree's planes): 10.87 to 10.43 s.
4. **Hull disk cache** (`vr_hull_cache`, default 1; Debug > Tests, Hitboxes on Disk; HULLS.md, "Kept on disk"):
   trees that took 250 ms or more written to `cache/hulls/<build>/<world>_<box>.hul`, read at the next load (24 ms a
   tree): 10.4 s to 1.3 s. Checked on read; a corrupted file is rejected, rebuilt and rewritten; 2 compares (4 of 4
   the same); e1m1 and e4m7 write nothing.
5. **Box3D world mesh**: the T-junction search per face on the pool, each nearby cell looked up once: 573 to 154 ms
   (its wait at the first server frames 149 ms to 0): 1,320 to 1,182 ms.
6. **Ledges**: the lip pieces found on the pool: 109 to 17 ms: 1,182 to 1,145 ms.
7. **choose()**: the pieces' bounds laid out an array per axis, branch-free counts: first build 10.42 to 9.57 s.
8. **The brushes grown on the pool** (`growAllOnPool`): the table's answers to the brushes' own planes found first in
   order, runs of 256 brushes grown with them, each brush's asks then put to the table in order and kept if every
   answer has the same values (99.7%; the rest grown again in order): a tree's grow 1.2-1.8 s to 0.4-0.8 s.

Not done (ranked by what they would save):

- **The trees' first build, 8-9 s** (only once per vr_hull.cpp build now): ~150 s of CPU in `choose`, `clipWinding`,
  `finish`, `splitPoly` and Face copies (240-byte faces with 8 inline double points). Its peak memory is 11 GB
  (process working set): each shared unit copies its input pieces (`Unit::input`, kept only for a redo that never
  happened here). A map-side alternative: vrstart2's 33,682 brushes are hull 0's solid leaves (the terrain's prisms
  cut by the BSP); building from the map's own brushes (BSPX BRUSHLIST, qbsp `-wrbrushes`) would be ~11k brushes, but
  changes what the hull is built from.
- **Alias models, 420-460 ms** of every first load of a session (142 models, their normal maps 230 ms even with the
  cache; 1.2 s with it empty): loaded one by one on the main thread.
- **The wave mesh, 104 ms on every load** (warm and restart too): it could be kept for a reload of the same map.
- **"map: campaign (game folders)" 112 ms** and the first map's normal maps (cache empty: 780 ms for 110 skins).
- The world's brushes (362 ms, `recoverClips`' `boxInTree`) are on the pool and hidden by the spawn.

Map-side: nothing changed in vrstart2 (the fixes are the engine's; the look and the paths the same: the walk test
18 of 18 legs, screenshots).

## Immersive manual reloading, phase 1: the ammo pouch and the shotgun (2026-10-07)

The author's request: a third reloading mode, loading by hand from a pouch on the belt (design, accounting rules,
models, menu, tests and phases 2-4: RELOAD.md). Phase 1:

- **The mode.** `vr_reload_mode` 3 "Immersive" (0 Off, 1 All Holsters, 2 Hip Holsters as before), the shipped default;
  `vr_cfg_version` 97 moves a config still at the old default (2) to it (Off and All Holsters stay). Immersion's row,
  the setup boards' "Reloading" option (vr_setup.cpp) and the new **Weapons > Reloading** page have it. In 3 the guns
  that load by hand (QC `VR_Reload_ManualFor`: the shotgun) get no holster, button or flick reload
  (`VRTryReloadWeapon`); every other gun reloads at the hip holsters, as in 2. The shotgun you start a game with comes
  loaded from your shells (`SetNewParms`; the holsters' reload filled it as it was first drawn), as map pickups do.
- **The ammo pouch** (`vrpouch_ammo.mdl`, make_ammo_pouch.py: make_pouch.py's leather, wider and shallower, shells
  standing brass up; frame 1 empty, by your shells): on the front of the belt between the hip holsters, placed as they
  are (vr_body.cpp `ammoPouchPosition`: the belly's ring with the body, carried by the pelvis; the hips' height and X
  without it), drawn and lit by vr_view.cpp `setupAmmoPouch`, reached as hotspot `HS_AMMO_POUCH` 12 (the one a hand is
  most within, holsters and pouches alike). `vr_ammo_pouch_x/y/z/thresh/pitch/yaw/roll/scale/show`. Climbing, the
  flashlight, self-collision and the QC holster lists know the new hotspot.
- **The loading port.** polish_weapons.py `loading_port`: under the shotgun's receiver, between the trigger guard and
  the pump: a black floor a step below the keel, a worn steel frame standing out round it, the brass lifter at its
  back (parts added; the old vertices, triangles and anchors as they were). vr_view.cpp's `loadPorts` table has its
  point in model space; the client places it as drawn (like the muzzle) and sends it with each move
  (`VrMove::loadPort` -> `.loadportpos`, `.offloadportpos`).
- **The shells** (QC vr_reload.qc; `vr_shell_live.mdl`, `vr_shell_pair.mdl` from make_shell.py: the spent shell's head
  and hull closed by a star crimp; two side by side in grey tape). An empty hand gripping at the pouch takes a shell for
  the gun in the OTHER hand (with `vr_reload_shell_pairs`, a taped pair if two are left), out of the reserve at once; it
  is a carried prop (prop slots 49 and 50, `vr_props_version` 60: held fixed in the fist, its crimp out in front, the
  fingers fitted round it; tuned in Held Object Offsets). Held within `vr_reload_port_leniency` (4 units) of that gun's
  port it goes into the magazine (the gun's record); a pair with one place left loads one and leaves a single shell; a
  full gun takes nothing (a dull tap). Let go of at the pouch (or a holster, or the trigger pressed: its pickup) it is
  refunded, up to the most you carry; anywhere else it drops, a Box3D prop (tinks: the spent shells' sounds) that can be
  taken again by hand (`vr_reload_grab_slack` 3 cm: it lies flat under the lowest the fist gets) or force grabbed. At
  most `vr_reload_loose_max` (24) lie about (the oldest not in a hand goes) and each fades after
  `vr_reload_loose_time` (90 s): then it is lost. One in a hand at a level's end is refunded. The empty pouch gives
  nothing: soft pats and a dull knock. Sounds (make_sounds.py): `reload_pouch.wav`, `reload_shell_in.wav`,
  `reload_empty.wav`; haptics and volume scale by `vr_reload_haptics`, `vr_reload_volume`.
- **Tests.** Debug > Tests > Reloading (`vr_reload_test <step>; impulse 125`: take, load, drop, put back, empty the
  gun, report, the self-test); `vr_reload_debug` prints the steps; `vr_mock_hand_to <hand> ammopouch | lport [units]`.
  `Misc/quakevr/reload/reload_test.sh <agent>`: the self-test (27 of 27), the mock hands' take / port / drop / regrab
  / refund (6 of 6) and Hip Holsters unchanged (3 of 3). The loaded map guns' transfer and pickup tests pass; the e1m1
  smoke, the weapon instance steps, `vr_menu_path_check` (0 missing) too; QC 0 warnings.

Open for the author: the shell's pose in the fist (the defaults are a first fit, Held Object Offsets has them); the
pouch's place and size; whether a shell let go of at a hip holster should go back into the pouch (it does: it is a
pickup) or drop.
## vrstart2: the author's screenshots of 2026-10-07 (holes, lanterns, rope, barrels...)

Vittorio's 22 annotated shots (the list and my reading of each: the agent's `scratch/notes/flaws.md`). Fixed:

**Holes** (12 of the shots: grey triangles in the ground, slivers in the cliffs, wedges on the lake's floor) and the
**foam lines and triangles on the water** (2: the shoreline foam drew round holes in the water's surface) were one
fault: faces missing from the compiled map. ericw-tools 2.0-alpha11's qbsp said "519 sides not found" (with
`-verbose`: "couldn't find portal side at ..."): a portal between air and solid with no brush side to make its face.
A ray test over the BSP (rays from random open points to the first change of contents, the hit point checked against
the world's faces on that plane: 20 000 rays, 88 hits in 79 places) found them; screenshots aimed along its rays show
them. Every one had, within a few dozen units, two neighbouring terrain prisms whose tops were 0.002-0.006 degrees
from coplanar (integer heights; the island's in steps of 8): the wedge between the planes thinner than the
compilers' epsilons. 0.18.1's qbsp lost faces at the same places (10 hits). Two changes:
- `terrain_planes` (vrstart2_gen.py): neighbours whose tops come within `TERRAIN_SNAP` (0.5 unit) of each other's
  planes share one plane exactly (a prism's top written as three points of the group's plane, `mapgeom.prism(top=)`);
  exactly coplanar neighbours are a group from the start, the smaller group joins the larger's if every corner is
  within the snap. 1397 of 10 518 tops moved, by under half a unit (a step, never a gap: the prisms are solid to the
  floor). 0.18.1's qbsp: 0 holes (120 000 rays); 2.0's still 45 hits ("417 sides not found": its losses elsewhere
  too, by rocks and crystals on the lake's floor, at no near-coplanar pair).
- **qbsp is now ericw-tools 0.18.1's** (`DEFAULT_QBSP`, `--qbsp`; `-bsp2`), vis and light still 2.0's. Its liquids
  are TEX_SPECIAL (unlit, and never cut into faces small enough for a lightmap): the lake is now tiles of 160 units
  (`WATER_TILE`, every other one's texture shifted a whole copy so no two merge) and `lit_liquids` clears the flag on
  the water's texinfos before light, so the water is lit as before (5728 lit faces, extents at most 160). The teleporter's
  `*teleport` stays unlit (its face is 304 wide); it looks the same. qbsp takes 40-70 s (2.0: 240 s); the full compile
  about 13 minutes.

**Lanterns** ("should emit light", the pier's and the tower's): their glass is `qvr_lantern` (quakevr_dev.wad: warm
fullbright texels behind an iron frame), so they glow at night; the pier's light 180 -> 240, the tower's 150 -> 220
and now beside it (over it, the lantern's own top shaded the deck round its post). (A light inside the glass, the
glass func_detail_illusionary, lit nothing: light counts those faces as shadow casters.)
**The pier's rope** ("abruptly ends"): the second row of pilings rises to short bollards and the ropes from the end's
bollards run to them. **SETTINGS banner** ("covered by torch"): over the pavilion's way in (z +92, centred), not in
front of the north torch post. **Targets** ("misaligned texture"): one bullseye fitted to the 52-unit face (scale
52/64, offsets from the face's corner). **Islets' braziers** ("gap"): the pillar sunk 64 into the rock (was 8; the
islet's top slopes away under it). **Barrels** ("would be cool if these were new physics props like the wooden
crates"): `vr_barrel` (QC vr_crates.qc), a crate but for its model (make_crates.py's vr_barrel.mdl: staves, iron hoops,
three skins and a normal map), a Box3D body as the crates are (the hull of its model: lying, it rolls when pushed);
Held Object Offsets slot 61 (30 kg; `vr_props_version` 62); Debug > Tests > Thing: Barrel, Barrel Lying.

**Tested**: the ray test 0 hits (120 000 rays); before/after shots from his places (`scratch/views/contact_sheet.png`
in the agent's worktree); the walk test 18 of 18 legs; the buttons pressed by hand (settings' Turning, the Scourge
lectern, the tutorial: `vr_debug_wallbuttons` "pressed by player: hand 1", as on the old map); e1m1's smoke test; a
barrel spawned upright and lying in e1m1, pushed by the hand (it rolled); the load (exclusive, 4 runs each, alternating): cold (a
new process, the hull files there) 1163 -> 1183 ms median (1138-1212 against 1162-1209), warm (the map again, the same
process) 283 -> 286 ms; the hull files load in 6 ms (were 8).

## Leaning through stick turns: the body kept on the real one (2026-10-07)

Report: leaning, then turning or moving with the stick, put the game's body out of step with the real one.

**Cause.** The lean (`hands::lean`, the head off the middle of the player's box, vr_hands.cpp) is kept in the world's
axes, and a stick turn turns the play space about the head (`addTurn`): the head stays, the box stays, the lean stays
pointing where it did in the world. The real body, though, is a lean behind the head in the room, so it swings round
the head with the play space. After a half turn leaning forward 24 cm, the game had the body 49 cm off the real one
(in front of the head instead of behind it): the drawn body leant backwards, its arms reaching back to the hands, the
holsters and anchors off with it; straightening up, the head went "further out", the body walked or slid after it
(about 2 s, vr_lean_recenter), and against a wall or a ledge it never came back (25 cm for good). The lean's learnt
hands (`LeanSense::handsRef`, also in the world's axes) were turned wrong too. Stick movement alone was fine (the box
moves, the lean is relative to it); the torso's yaw is kept in play-space degrees and was fine.

**Fix (vr_lean_turn, default 2).** Stick turns go through `hands::stickTurn`: the lean and the learnt hands turn with the
play space. 2: a smooth turn keeps the head where it is and swings the box round under it (room-scale walking, where
the box fits and has floor under it, the recentring's own checks); else, and for snaps (the view jumps anyway), the
head swings round the body, which stays. 1: always round the body. 0: the old way. The portal crossing's turn keeps
its own lean turn (`addTurn`, unchanged); server yaws (teleports, spawns) still reset the lean.

**Measure.** `vr_body_error [mark]` and Debug > Views > Log Body Drift (`vr_debug_body_error`): where the game has the
body (its box) in the room and which way it faces there, against the mark. Standing still in the room, it should stay
near 0 whatever the stick does. `Misc/quakevr/lean/lean_turn_test.sh <agent>` (pos cm after the turn / straightened /
2.5 s later):

| case | vr_lean_turn 0 | vr_lean_turn 2 |
|---|---|---|
| lean fwd, smooth 180 | 48.9 / 25.4 / 0 | 1.1 / 1.1 / 0 |
| lean fwd, smooth 90 | 34.3 / 25.4 / 0 | 1.1 / 1.1 / 0 |
| lean fwd, snap 90 | 34.6 / 25.4 / 0 | 1.1 / 1.1 / 0 |
| lean fwd, snap 2x90 | 48.9 / 25.4 / 0 | 1.1 / 1.1 / 0 |
| lean right, smooth 180 | 43.1 / 25.4 / 0 | 0.8 / 0.8 / 0 |
| lean fwd, stick move | 1.1 / 1.1 / 0 | 1.1 / 1.1 / 0 |
| lean right, turn + move | 43.2 / 25.4 / 0 | 0.8 / 0.8 / 0 |
| at a wall, lean to it, smooth 180 | 44.0 / 25.4 / **25.4** | 0 / 0 / 0 |

The 1 cm left is the lean's first instants, walked before they are known as a lean (vr_lean_detect): head and hands
alone can't tell a lean from a step at once. The head during a smooth 180 at the default speed with a 24 cm lean:
mode 2 holds it within 1.5 units (the box's move reaches the server a frame later; it catches up when the turn stops),
mode 1 swings it 15 units. Turning and moving into e1m1's walls turns the torso 6-11 degrees (the hands stopped at the
wall pull it, `stopAtWall`), with or without a lean, in either mode: not the turn's.

## Immersive manual reloading, phase 2: magazines (2026-10-07)

RELOAD.md's phase 2: the nailgun, the super nailgun and the thunderbolt take magazines (immersive mode; the lava
nailguns and the plasma gun too, with their ammo).

- **Models** (make_mags.py; normal maps baked): the nailgun's a box magazine under the receiver ahead of the trigger
  guard, raked forward (the old nailgun pickup's leg), brown with stamped ribs, a steel base plate, nail heads in its
  lips; the super nailgun's a bigger box out of its outer side (right, mirrored in the left hand), 37 degrees up, a
  window down its top face showing the nails; the thunderbolt's an octagonal blued cell hung under its body, bronze
  bands, a copper contact. Each as a prop (`vr_mag_nail/snail/light.mdl`: prop slots 51-53, `vr_props_version` 61: in
  the palm, one hand) and as drawn in its gun (`vr_mag_on_<gun>.mdl`, made in that gun's model space; the engine gives
  it the gun's Scale and offsets, vr_weapons.cpp `makeModelTransform`, and corrects for the bounds' corner the Scale
  is applied about; it moves with the gun's firing kick by an anchor vertex).
- **State.** The magazine's rounds are the gun's record's clip (it stays with the gun); none in is the weapon flag
  `QVR_WPNFLAG_NOMAG` (16), which travels with the gun's flags (holsters, throws, level changes). The client draws the
  attached magazine from the hands' and holsters' flag stats and, for a gun lying about, a new entity bit `U_QVR_NOMAG`
  (vr_view.cpp `setupMagazines`).
- **The pouch** gives the magazine of the gun in the other hand, holding min(its size, the reserve). Held within
  `vr_reload_mag_leniency` (5 units) of the gun's well it seats if the gun has none. With one in, only a **bump** seats
  it: the hands meeting at `vr_reload_bump_speed` (1.2 m/s) or more knocks the old one out (flying, its count kept) and
  seats the new one; slower, nothing happens (a dull tap).
- **Ejects:** (1) B/Y on the gun's controller (`vr_reload_eject_button`; weapon cycling keeps the button off a magazine
  gun then); (2) the other hand gripping within `vr_reload_pull_reach` (6 units) of the magazine holds it (as a
  foregrip: nothing comes out), and pulling it away from the gun at `vr_reload_pull_speed` (1.5 m/s) with the wrist
  turning at `vr_reload_pull_snap` (200 deg/s) takes it out into that hand (a freshly pulled one doesn't re-seat until
  it has left the well); (3) the bump. Out, a magazine is a round as the shells are: taken again, force grabbed,
  refunded at the pouch (a part-used one its count), under the loose rounds' cap and fade. Switching a lava nailgun's
  ammo drops its magazine.
- **Multiplayer (phase 1's flag fixed):** a new stat `STAT_QVR_RELOADMODE` is the server's mode as it applies; the client
  draws the ammo pouch, the magazines and the ammo screens' clip by it, not by its own `vr_reload_mode`.
- Sounds (make_sounds.py): `reload_mag_in.wav`, `reload_mag_out.wav`. Weapons > Reloading has a Magazines group (Well
  Leniency, Eject Button, Pull Speed, Pull Wrist Snap, Pull Reach, Bump Speed); Debug > Tests > Reloading: Nailgun in the
  Off Hand, Eject the Off Hand's Magazine (`vr_reload_test 6`).
- **Tests:** reload_test.sh: the self-test 47 of 47 (the magazines' 20 checks among them), phase 1's 9 mock checks, and 9
  magazine ones by the mock hands (B/Y drop, take, seat, a gentle pull that holds, a hard pull with a snap, the part-used
  refund, a second seat, a slow meeting that does nothing, the bump). Loaded map guns' transfer and pickups pass, the
  e1m1 smoke and the weapon instance steps, `vr_menu_path_check`; QC 0 warnings.

Open for the author: the magazines' size and place on each gun; the pull and bump thresholds; the pouch still shows
shells whatever the gun (frames per round kind are phase 3's).

## Immersive reloading: the author's first notes (2026-10-07)

His voice notes on phase 1, one commit each:

1. **The pouch rides the legs** (`vr_ammo_pouch_leg_follow`, default 1; Reloading > Follow Legs): with the full body it
   moves with both thighs' animation as the belt does, half each, as the hip holsters do with `vr_holster_leg_follow`
   (vr_body.cpp `onBothThighs`).
2. **Adjustable load points** (Reloading > Load Points): each gun's port or well moved by
   `vr_reload_port_<shot|nail|snail|light>_x/y/z` (the gun's model units; the lava nailguns' and the plasma gun's with
   theirs); **Show Load Points** (`vr_reload_show_ports`) draws the point (yellow), its acceptance range (green: Port or
   Well Leniency) and a well's pull reach (blue), live.
3. **Collision Leniency** (`vr_reload_collide_leniency`, 12 cm): a held shell or magazine may go that far into the
   other hand's gun's box (Held Things Collide keeps them apart by the boxes: the gun's goes down to its grip, so the
   shell stopped short of the port under the receiver) before they are kept apart (vr_held.cpp).
4. **Ammo boxes into the pouch** (QC `VR_Reload_PouchTakesBox`): an ammo box (shells, nails, rockets, cells, the
   mission packs', the horde's) let go of at the ammo pouch goes in, whatever `vr_carry_take` says, with its rustle.
5. **The real count**: the pouch's model (make_ammo_pouch.py, 14 frames) shows what it gives, one in sight per round
   left: 1-5 shells, 1-3 nailgun magazines, 1-2 super nailgun magazines, 1-3 cells (a part-filled one counting); empty,
   it falls in (vr_view.cpp `ammoPouchFrame`).
6. **The counter** (`vr_ammo_pouch_counter`, on; Reloading > Counter, X/Y/Z, Pitch/Yaw/Roll, Size): the reserve of what
   the pouch gives, on a small screen as the guns' ammo counters, above the pouch facing the eyes as they look down.
7. and 8. **By ammo, not gun; the last kind kept** (QC `VR_Reload_PouchKind`, `VR_Reload_PlayerFrame`): the pouch gives
   the other hand's gun's ammo (any shotgun, the super shotgun too: shells, though it has no immersive rules: a shell
   brought to it does nothing; a nailgun, super nailgun, thunderbolt: its magazine); with nothing of the kind in the
   other hand (empty, a melee weapon) the last kind held (the main hand's first; shells at first), to take, throw and put
   back. The server sends what it gives and how many are left (`STAT_QVR_POUCHKIND`, `STAT_QVR_POUCHCOUNT`). The
   rocket and grenade launchers give nothing yet (phases 3-4).

Also: rebased on the wrist gadget redesign (45425224): `STAT_QVR_RELOADMODE` and the new two stats follow its
`STAT_QVR_AMMOTYPE`. Tests: reload_test.sh 24 of 24 (the self-test 54 of 54; section 5: the legs, a load point moved,
Collision Leniency 0 against 12, an ammo box with `vr_carry_take 1`, the pouch's frame for 50 nails), the loaded map
guns', the e1m1 smoke, `vr_menu_path_check`; QC 0 warnings.
## A far button pressed at a map load (2026-10-07)

Your report (seen by a headless agent on vrstart2, and on the old vrstart): at a map load the off hand sometimes
"pressed" a button far from it. Logged with the press's hand, its position and distances (`vr_debug_wallbuttons 1` now
prints them under each press), the presses were never the hand's touch: they were **the engine's line from a hand to its
muzzle** (`weaponTouches`: "pressed by player: the line from hand 0 to its muzzle", the hand 53 units from the button,
the muzzle near the world's origin). Two ways that line crossed the map on a load's first frames:

- **The client's muzzle was the last render's.** Moves are sent before the frame's render, so a move carries the muzzles
  (and the loading ports) the view placed at the previous render, with the hands as they are now. After a map load the
  last render was the old map's, or this map's first one with the player's entity still at the world's origin (it is
  placed by the first entity update): the empty hand's muzzle (the fist is a "weapon" with a muzzle) sat at about
  (+-9, 10, 21), the hand at the spawn. A teleport left it behind the same way for a frame. The view now records where
  each hand was when it placed them (`hands::State::placedFrom`, at the end of `VR_SetupViewEntities`) and the move
  carries the muzzle, a carried gun's tip and the port along by how far the hand has gone since (vr_client.cpp
  `handMuzzle`). In play this also takes the one frame of hand travel off the muzzle's lag (its turn still lags a frame,
  as before).
- **The server's fallback, before the client's first move on the map,** traced from `.handpos` (the player's origin,
  set at the spawn) to `.muzzlepos`, never set: the world's origin. `PutClientInServer` now sets the muzzles and ports
  to the origin with the hands (client.qc).
- And a guard: a line from a hand to its muzzle longer than 4 m (`weaponLineMaxMetres`; no weapon is that long) touches
  nothing (`developer 1` prints it). It never fires after the two fixes.

Hand touches themselves (buttons, pickups, weapons lying about, carried boxes: `handTouches`, `handTouch`) were never
wrong at a load: the hands are placed from the move's own origin and moved with the body (`rebaseHands`); what used the
muzzle line was the wall buttons and the explosive boxes (`vr_wpntouch`), and the QC's muzzle (`.muzzlepos`).

**Tested** (`Misc/quakevr/buttons/stray_press_test.sh <agent>`): vrstart2 and vrstart loaded 5 times each (hands at
rest), and a save made on the pavilion with the off hand held up by the Turning button loaded 20 times. Before: the 20
loads pressed Turning each time (20 of 20; the 10 map loads and a save on vrstart 0, their lines blocked or missing
buttons); after: 0 of 30, no line over 4 m, the same with `-RealTime`, and 5 fresh processes (`+map vrstart2`, then the save) 0. Real presses
right after a spawn or a load still work (Turning by the off hand 20 frames after the load, the tutorial button 20 frames
into the map, the Scourge lectern); the walk test 18 of 18; e1m1's smoke test. (eval.sh: no current melee takes.)

**To try in VR:** load vrstart2 and vrstart a few times, from the menu, a save and the teleporters (come back from the
tutorial or the firing range): no button presses itself; the buttons by the start still press at once.

## White text on the ammo screens (2026-10-07)

Your note: the ammo screens' text in the wrist gadget's white, the frames kept green, the pouch counter too.
`vr_ammo_screen_text_white` (1, on; Graphics' ammo screen rows "White Ammo Screen Text", and Weapons > Reloading
"White Counter Text", the same setting): the guns' ammo screens and the ammo pouch's counter draw their numbers near-white
in the gadget's brightened console font (`gadget::whitened`, `gadget::useBrightFont`: the gadget's own mechanism, now
shared; its `drawText` uses it too); 0 the screen's colour as before (between: a mix). The face, the frame's glow and the
CRT's static stay the screen's colour: as a CRT (`vr_weapon_screen_crt`) the screen's image is drawn in its own colours
(Shade::Screen's trueColor, as the gadget's), its face pre-lit as the one-colour shader showed it; without the CRT look
the glyphs go in a batch of their own in the bright font. Pictures: `scratch/ammo_text_white.png` (gun screen off/on,
pouch counter off/on).
## The menus' corner column moved left; the rows from the top (2026-10-07)

Your note: in the headset the shortcut column (Back to game, Search, ... Relighting) and the vertical Quake VR banner
under it often overlapped the menus; move them left, and use the room freed at the top for more rows.

- **The column (its buttons, the banner under them, the spectator switch at the bottom) now stands clear left of what
  the menu draws.** Its right edge was at menu x -8, but a VR page's labels are right-aligned to the values' column and
  reach left of Quake's 320 columns (a 26-character label from x -32, a 38-character one from -128), and Ironwail's
  lists (Levels, Mods) span the canvas's middle from about -136: they ran under the buttons and the banner. Its right
  edge is now at x -136 (`ToolbarLayout::columnRight`) on almost every page, the same place each time for the laser; a
  page whose text reaches further left (`menu::contentLeft`: its longest label, a long header, the help's width) moves
  it further, 8 clear: Status Bar's and Wrist Gadget's long links, Debug - Tools' long headers, Debug - Views, a few
  others (most at Menu Detail: Developer). Ironwail's lists are kept narrower in the headset so that they start right of
  the column's usual place (`VR_MenuBounds`, now with their left and width): the column stays put on Levels and Mods.
  On a narrow panel (no room left of the menu) it still falls back to the corner's icons and the rows below them.
- **The rows start under the page's title, not below the buttons**, wherever the column is beside the menu
  (`menuui::toolbarBeside`): the VR pages show 27-28 rows instead of 22-24 (VR Settings 23 -> 28 at Developer, 24 ->
  28 at Standard; Graphics 22 -> 27; Melee 22 -> 27; Debug - Tools 22 -> 27), Levels, Mods, Options' lists and the key
  bindings about 4-5 more, Search and the Map Library's keyboard and results higher by as much.
- **The status box in the top right corner stays clear:** a menu reaching right under it (Ironwail's lists, Search, the
  Map Library) starts below it (`menuui::statusBottom`, the box taken as 40 characters wide at least so that a line
  growing by a digit does not move the rows). Levels' title now sits under it; the VR pages end left of it.
- The flat screen's row of icons along the top is unchanged (it never overlapped).
- `menu_vr pos` prints the layout: where the menu's text starts (menu x), the buttons' right edge and bottom, beside or
  over the menu, and on a VR page the rows' top and how many are shown.

**Tested** with the mock (vr_eyeshot 3, the left eye's panel): Main, Options, Levels, VR Settings, Status Bar, Graphics,
Map Library, Checklist, Debug - Tools (a long help) and the key bindings before and after; nothing overlaps the column,
the banner, the status box or the spectator switch. The laser clicks the moved buttons (Advanced VR from Status Bar,
whose column is further left; Checklist from Debug - Tools; Levels; the spectator switch). The flat screen's menus
unchanged (VR Settings and the Map Library start where they did). `vr_menu_path_check maps/vrcalibration.map`: 0
missing. e1m1's smoke test.

**To try in VR:** open the menu on a few pages (VR Settings, a long Advanced page, Levels, the Map Library): the column
and the banner stand left of the menu, more rows show at once, and the corner's buttons are as easy to hit with the
laser. Status Bar and Wrist Gadget (their long "... VR Settings, Body and Display" links) move the column further left:
say if that jump bothers you (the alternative is a fixed place left of every page, further from the menu).
## Immersive reloading: rounds 2 and 3 (2026-10-07)

The author's second and third rounds of VR notes on immersive reloading (worktree `reload`).

- **Shells**: spent shells are drawn plainly apart: a darker, scuffed and sooty hull, dulled brass, a dented black primer (make_shell.py `paint_spent`). A shell
  coming within reach of a full shotgun's port clicks "can't" once (`reload_blocked.wav`, a short haptic), again only
  after it left the port's range; the last shell in plays a heavier "full" knock (`reload_full.wav`). The insert sound
  `reload_shell_in.wav` is duller (a muffled "shk-chk"), still synthesized (make_sounds.py) under that name.
  Open-source candidates for the author to pick (nothing downloaded): see the round's report.
- **Load points**: each gun has its own point and radius (`vr_reload_port_<shot|nail|snail|light>_x/y/z/_radius`:
  4, 1, 1, 1); the old `vr_reload_port_leniency` and `vr_reload_mag_leniency` are gone. A magazine's reference point is
  its top (`vr_reload_mag_<nail|snail|light>_x/y/z`, radius `_radius` 0.5): it seats when that top is within the gun's
  radius plus its own of the gun's point (where the seated magazine's top sits). Show Load Points draws both.
- **Magazines**: taken from the pouch top up, in the loading pose (props 50-52 Grip X/Y/Z, GripMode 1; the author's
  sizes 0.55/0.45/0.45 as defaults; `vr_props_version` 63 takes the shipped slots). The super nailgun's magazine now
  stands perpendicular to the ridged face it attaches to. Each magazine gun has a visible receiver (`vr_magwell_on_<gun>.mdl`,
  make_mags.py `magwell`), drawn on the gun at its well, moved and turned by `vr_reload_well_<gun>_x/y/z/pitch/yaw/roll`
  (looks only). Lava nails' magazines are fiery (skin 1: reds, glowing nails); plasma cells' bluish; the pouch too.
- **The magazine is the two-handed grip** on the nailgun, super nailgun and thunderbolt: their 2H hotspot is the seated
  magazine; gripping it aims two-handed and holds it in; it comes out into that hand only by a hard pull
  (`vr_reload_pull_speed` 2.5 m/s), a wrist snap (`vr_reload_pull_snap` 600 deg/s) or the hands moved apart
  (`vr_reload_pull_apart` 10 units, about 40 cm); a gentle pull keeps it.
- **Hits**: a seated magazine pops out when hit at `vr_reload_bump_speed` (2 m/s) or more by a fist, a held prop or
  magazine, or the other gun (its line from the hand to the muzzle) within `vr_reload_hit_reach` (4 units) of its top;
  it flies along the blow with the gun's own speed. Not within half a second of a seat (the hand that pushed it in
  follows through by the well). The bump reload is two steps: a magazine meeting a full well only knocks the old one out;
  it seats once taken away (`vr_reload_collide_leniency + 4` units) and brought back.
- **Collision**: a hand holding a round from the pouch goes through the other hand near that hand's gun's port
  (vr_selfcollide.cpp; `vr_reload_collide_leniency` now 30 cm): pushed apart, the gun's port moved away from the round.
- **Grips on the rising edge** (`vr_2h_grip_edge` 1, Aiming, "Grip Must Close On It"): a two-handed grip on any gun
  takes hold only when the grip closes on its hotspot; a fist already closed moved onto it doesn't.
- **The pouch counter** turns with the pouch (its look and up from the pouch's frame), not the head.
- **The ammo button** is pressed only by a fingertip coming at it from its front, within `vr_weapon_button_cone` (50)
  degrees of the way its face looks (Hand/Gun Calibration, "Ammo Button Cone"; 180: from anywhere).
- Tests: reload_test.sh section 6 (TESTING.md); the self-test 56 of 56.

## Immersive reloading: the recorded shell insert (2026-10-07)

The author picked zer0_sol's "Shotgun Reload Sound effects" (OpenGameArt, CC0; docs/vr-port/CREDITS.md) for the shell
insert. `Misc/quakevr/make_reload_shell_sounds.py` cuts three takes from the downloaded MP3s (not in the repository) into
`reload_shell_in.wav`, `reload_shell_in_2.wav` and `reload_shell_in_3.wav` (44100 Hz 16-bit mono, 0.18-0.23 s); QC
`VR_Reload_ShellInSound` picks one at random per insert (the pitch jitter on top), so a tube filled shell by shell does
not repeat. The synthesized insert is gone from make_sounds.py.

- **What is cut**: every shell in the takes is two clusters, the shell handled at the port (rattling clicks) and then
  pushed into the tube (a scrape rising as the spring gives, ending in the shell latch's click on its rim, -80 dB floor
  between). Only the push: 80 ms of scrape before the click, 100-150 ms after it (two takes keep the thumb's small tick
  110 ms after the click), faded in 15 ms and out 30 ms. The click lands at 80 ms, where `reload_full.wav`'s second
  knock and thud fall, so the last shell's click and the full knock coincide.
- **Level**: the takes are bright (40-70% of the energy above 6 kHz, nothing below 300 Hz; the synthesized insert was
  74% below 300 Hz), so they are matched by A-weighted loudness, not RMS: -14.6 dB over the loud frames, as the
  synthesized insert (Quake's weapons/guncock.wav -18.1, the magazine seat -12.3); peaks soft-limited under -1 dBFS
  (about 1% of the samples). Kept at 44100 Hz: 5-8% of the clicks' energy is above 11 kHz.
- **Not replaced**: the "full" cue and the "can't" click stay synthesized. "Shell in Chamber" ends in the action
  closing, a 250 ms bright triple click: over the last insert it smears the latch's click and sounds like the gun being
  cycled; the synthesized full knock is low (78% below 300 Hz) and gives the bright recording weight under it. "Shell
  in Chamber"'s first click (0.51 s, a single dull click, 80 ms) would suit the "can't" click if the synthesized one
  sounds out of place next to the recording.

Test in VR: fill the shotgun shell by shell (the three takes alternate, none too loud or quiet next to the pouch and the
magazines), the last shell's click with the full knock under it.

## Installer sounds crackled (2026-10-07)

Vittorio heard crackling in the installer's UI sounds. Two causes, measured (`QuakeVR-Setup --screenshots <dir> --extras`,
report.txt: "device rings" and "offline mix").

- **Underruns (the crackle).** `SoundEngine` queued 4 buffers of 512 frames (46 ms) on wave-out. With the fire's loop
  and a click every 120 ms while the live window animated, that ring played 20.1 s of audio in 26.1 s (the device's
  own position, `waveOutGetPosition`): 430 underruns (every buffer done at a refill), a ~14 ms gap every ~60 ms. The
  event was right (CALLBACK_EVENT on `_deviceEvent`, ~870 device wakes; the old 50 ms timeout was not the cause: with a
  20 ms timeout the 46 ms ring still drained 428 times). Windows' wave-out, emulated over WASAPI, starves with 50 ms or
  less queued: 5 x 441 (50 ms) played 21.7 s in 26.0 s, and there WHDR_DONE missed it (one underrun counted; the
  emulation holds the last buffer while it starves), 6 x 441 (60 ms) held with one buffer to spare, 8 x 441 (80 ms)
  with six. Now 8 x 441 (10 ms each, the audio engine's period; a click waits at most 80 ms), the wait 20 ms. Reading
  the device's position at every refill hid the gaps (the emulation's timing changes), so the harness reads it once
  at the end.
- **The mixer's own steps (clicks).** Quake's 8-bit sounds start and end off zero (first/last samples up to 0.07, DC
  up to 0.03; the 8-bit conversion, `(b - 128) / 128`, is centred), a fourth voice of one sound was cut mid-play (a
  burst of typing: `misc/menu1` is 507 ms), the loop jumped from its last sample to its first, and the mute cut the
  output. An offline render of a scripted 9 s session with Quake's sounds (clicks, 26 keystrokes 55 ms apart, the
  install's sounds, a mute): the mixer's own largest step 0.027 with 20 over 0.01, now 0.001 and none. Now every
  voice fades in over 2 ms and out over its last 4 ms, a stolen voice fades out over 6 ms, the loop's last 40 ms is
  crossfaded into its start (`SoundMixer.Seamless`), the master volume ramps over 10 ms (the thread keeps sending
  until it reaches 0), and the limiter is linear to 0.6 then a tanh bend (the old knee `s - 0.15 s^3` jumped from 0.85
  to 1 at |s| = 1; no session reached it: peak 0.44).

The mixer moved to the core (`Core/Audio/SoundMixer.cs`, no device) with a self-test ("sounds: the mixer never steps
the output"); `SoundEngine` keeps the device, counts `Underruns`, `MinQueued`, `TimeoutWakes` and played versus
streamed seconds.

Test on the PC: the installer's clicks, a burst of typing in the folder box, and the mute button, over the fire.
## Re-gripping a head held in both hands (2026-10-07)

`twohand_regrip_test.sh` had the grunt's head held in both hands once, not 3 times ("Every prop in both hands"). Bisected
(the head case as the oracle, 773c0cb7 good): **6b655110** (October 5 notes), the heads' Size 0.7 (props v57). A prop
sits where it was fitted to the drawn fist, not to the fist's test spheres; the smaller head left the hand that held it
2.58 cm off its surface, past Two-Handed Grab Reach (2 cm), so the hand that let go of it could not grip it again where
it was ("the other hand is not within reach"). Not the test: in VR the same hand at the same place was refused.

Fix (vr_carry2h.cpp): when one hand lets go of a prop held in both (keep), where it held it (its grip, in the prop's
frame) is kept; that hand gripping again within Two-Handed Grab Reach of it takes the prop, whatever its size or fit.
The surface test is unchanged (and still the only test for a first second-hand grip). Forgotten when held in both again,
the entity is freed, or the map changes. vr_debug_carry 2 prints "carry2h: off hand 0.31 cm from where it let go of it".

Test: twohand_regrip_test.sh 3 of 3 runs: every prop held in both 3, kept by one 2, moved at most 0.01 units; the same
with vr_2h_grip_edge 0 (weapons only: props never used it); the off hand moved 5 cm away before gripping: refused.
reload_test.sh section 1 (56/56), grip_state_test.py PASS, e1m1 smoke clean.

In VR:
- [ ] Hold a monster's head in both hands; let go with one hand and grip it again where it is: taken, no jump.
## Shotgun auto pump (2026-10-07)

The author: "an auto-slide-pump animation after every shot (properly synchronized with the used shell ejection)", with a
rail system on the shotgun that makes it believable the gun cycles itself (worktree `shotpump`).

- **The model** (polish_weapons.py `auto_pump`, `split_auto_pump`; anchors unchanged, the loading port and its load point
  as they were): two polished steel guide rods along the fore-end's shoulders either side of the barrel (seen over the
  fore-end from the side and from above), from a blued actuator housing on each side of the receiver's front (a stepped,
  dark front where the rod comes out) to a yoke clamped round the barrel ahead of the fore-end (a band and a lug out to
  each rod). The fore-end is the id model's ribbed rings, the last three (the first, by the receiver, is gone: room for
  the stroke, and the rods show there), with a shoe round each rod on its first and last ring, a strap under each rod
  between them and a short action bar back from the rear shoe towards the housing. The id skin painted the rings on the
  fore-end's body under them: the rings got their own copy of their texels (6 rows added under the skin) and the body's
  stripes are painted over in the dark between them, so nothing stays behind as the rings slide. +432 triangles
  (v_shot.mdl 1216 -> 1648). v_shot.mdl keeps everything at rest (holstered, lying, thrown); the moving fore-end is
  also written apart, `progs/vr_pump_on_v_shot.mdl`, and the gun without it, `progs/vr_pumpbody_on_v_shot.mdl` (same
  header, frames and skin; their normal maps are v_shot's, copied by bake_normals.py: normalmaps.py `SHARED`).
- **The stroke** (vr_autopump.cpp): each shot (QVR_SVC_FIRED) starts one; game time (cl.time: slowed in bullet time,
  the same at any frame rate). Back over its first 35% (fast, easing into the back), held 10%, home over the rest (from
  rest, faster and faster, home at full speed). While it runs the hand's shotgun is drawn as the two parts (vr_view.cpp
  `setupPumps`: copies of the gun's entity, the fore-end slid back along the gun's model x; the gun's own entity not
  drawn, its frame blending copied back from the body's copy each frame), only for Quake VR's own v_shot.mdl (a mod's,
  with other frames or header, is drawn as it is).
- **The shell** leaves the port in the frame the fore-end reaches the back (vr_shells.cpp waits for the stroke the shot
  started instead of the QC's 0.22 s), thrown back a little with the action (0.4 of the stroke's average speed back).
  With Auto Pump off it leaves 0.22 s after the shot, as before.
- **Feel**: a light tick in the hand at the back and home (`vr_autopump_haptics` 1); two clacks, the slide unlocking
  and starting back as the stroke starts, the slide slamming home timed so its loudest moment is the stroke's end
  (`vr_autopump_sound` 0.5; cut from zer0_sol's CC0 "Rack.mp3" by make_autopump_sounds.py: CREDITS.md).
- **Settings** (Weapons > Weapon Effects, "Shotgun Auto Pump"): Auto Pump (`vr_autopump` 1), Time (`vr_autopump_time`
  0.3 s; 0.1-0.48, under the shotgun's 0.5 s refire), Travel (`vr_autopump_travel` 2.5 model units, about 9.5 cm drawn;
  0-3.2), Sound, Haptics. Debug > Tests: Hold the Auto Pump (`vr_autopump_hold` 0..1, the stroke held there for a look),
  Print Weapon Effects (`vr_debug_weaponfx` 1: each stroke's start, back, home and the shell's eject; 2: the travel each
  frame).
- Tests: `Misc/quakevr/autopump_test.sh` (TESTING.md).
  - Its "home 0.3 s" and "the shell 0.22 s" checks failed about 1 run in 4: awk's difference of the printed
    milliseconds (2.775 - 2.475 = 0.29999999999999982), not the timing (home on its due time in every run). They allow
    half a millisecond now.
- For the author in VR: the look of the rods and the stroke (Travel, Time), the clacks' volume against the shot, and
  whether the off hand on the fore-end (two-handed) should ride with it (it stays where the controller is now).
## Menu tweaks: VR Settings rows, section gaps, the flat banner, the main menu, Back where you came from, fine sliders

**VR Settings** (`pageMain`): Weapons > **Reloading Mode** (Immersive 3 / Simple 2 / Disabled 0 on `vr_reload_mode`; a
config's All Holsters, 1, is shown as "Simple (all holsters)" while it is set and kept until another is picked);
Locomotion > **Swimming** (Immersive / Vanilla on `vr_swim`); Body > **Leaning Detection** (`vr_lean_detect` 1/0; a
config's 1.5 shows On); a **Bullet Time** section, **Activation**: Wrist Gadget / Left Thumbstick Press / Right Thumbstick
Press (`vr_menu_bullettime`, a wrapper over `vr_bullettime_trigger`, `_tap`, `_button`, `_enabled`: a stick leaves the
gadget doing nothing, as Combat > Bullet Time's Trigger; the gadget sets its tap and button on; a config's other
combination is shown as it is: Off, tap only, button only, neither); under Hand Calibration a **Holster Calibration**
section: Forward / Inward / Up for the hip (`vr_hip_offset_*`), chest (`vr_upper_holster_offset_*`) and back
(`vr_shoulder_holster_offset_*`) pairs, wrappers `vr_menu_holster_<pair>_<x|y|z>` from the shipped place (Inward is the
offset's Y less; one setting per pair, the left mirrored, as always), the pair drawn on the body while its slider is
chosen (vr_body.cpp queueDebug), and **Reset Holsters**. The calibration room's board was left as it is (holsters are not
part of the calibration); `vr_menu_path_check maps/vrcalibration.map`: 4 found, 0 missing.

**Section gaps** (`vr_menu_section_gap`, rows, 0.75; HUD and Menus > Menu > Section Gap, 0 to 2): a gap above each header
of a VR page (the first too, under the title). The list still scrolls a row at a time; the rows shown are counted from
the scroll with the gaps (rowsFrom), and the furthest scroll (maxScroll), the scroll that shows the cursor
(scrollShowing), the scrollbar, the stick, the drop-down lists' rows and the mouse/laser's rows (rowAt: nothing in a
gap) all use the same layout (rowTop). Checked with `menu_vr rows` at 0, 0.75 and 2, in the headset and flat.

**Flat banner**: the vertical Quake VR banner moves left clear of the menu's text, as the headset's column does: VR pages
by `menu::contentLeft` (long labels, help), Ironwail's lists by `M_TextLeft`; narrower where the canvas's edge leaves too
little room, left out under 40 pixels tall (Status Bar's long links).

**Main menu**: Map Library is **Download Maps**, with **Play Custom Map** (Ironwail's Levels) under it; the rows in four
groups with a gap above each (M_Main_Layout: 20 apart and 10 more per group where the canvas has room, closer where it
has not; the keys skip a hidden row; the mouse and laser select a row only where it is drawn). **Mods** is hidden unless
`vr_menu_main_mods` (0; Menu > Mods on the Main Menu); Options > Mods has it. The lettering's D (d two rows taller) and w
(v with its right stroke twice more) are made by make_bigfont.py. The Multiplayer menu says multiplayer is untested and
not expected to work properly.

**Back where you came from** (vr_menu.cpp, NavStack): the VR pages keep the way to the page shown as a stack (pages, and
below them the menu outside they were entered from: main menu rows, Single Player > Official Campaigns, Options > VR
Settings, a corner button over any menu). Back pops it; a place already on the stack is gone back to rather than added
again (no loops); with nothing under the page, up the tree (the VR Settings: Options). Ironwail's Levels opened by a jump
(Play Custom Map, the corner's Levels, also from a VR page) go back there (`VR_NavJump`, `VR_NavEntered`, `VR_NavBack`);
from their own way in, their own Back. A Search result's page goes up its tree (its natural parent), not back to Search.
The mouse or laser on a corner button no longer moves the menu's own selection (it used to jump to the first row as
the laser went to the corner), so Back finds the row it left. Tested (flat, scripted): Single Player > Official
Campaigns, main > VR Settings, main > Advanced VR > Gore and back twice, main > Play Custom Map, main > corner Search,
Graphics > corner Levels, Options > VR Settings, Single Player > corner Advanced VR, Levels > corner Search and back
twice, a Search result (Relighting: back to Graphics, Advanced VR Options, the main menu).

**Fine sliders**: while a grip is held in the headset (the grips do nothing else in the menus) or Shift on a flat
screen, a slider steps by `vr_menu_fine_step` of its step (0.1; Menu > Fine Step) and shows the decimals that takes; the
steps' grid is the fine one, so a fine-tuned value keeps its fine part on whole steps. Held Object Offsets > X: 0.5 a
step, 0.05 with Shift or a grip; 0.55 saved as "0.55". Every slider's help ends with the modifier.

Test in VR: the main menu's groups and Play Custom Map (and Back from the Levels); Back from Official Campaigns and from
a corner button; the new VR Settings rows and Holster Calibration (the pair shown on the body while its slider is
chosen); Section Gap 0, 0.75 and 2 on a long page; a grip held while moving a Held Object Offsets slider.

## Foveated rendering: white seams on models in the periphery (2026-10-07)

With vr_foveated on and no MSAA (vid_fsaa 0), models in the coarse-shaded rings (the super nailgun's barrels, the
pentagram of protection) showed dotted white lines along some triangle edges. Cause: under GL_NV_shading_rate_image a
2x2 or 4x4 fragment's inputs are interpolated at its block's centre, which can lie past the triangle's edge, so the
interpolated values run on beyond their vertices' range. The model's own occlusion (in_vao, vr_ao_models: 0 at deeply
occluded vertices) went below 0 there and `sqrt(in_vao)` (the dynamic lights' share) gave NaN; NVIDIA's clamp turns NaN
into its upper bound, so the pixel came out at the scene's brightest. Evidence: with the author's settings, vr_ao_models
0 alone removed the lines (68 of 68 bright pixels in the gun crop); toggling parallax, normal maps, retro, retro light,
rim light and specular AA did not; `centroid` on the skin's coordinates changed nothing (no MSAA: centroid is ignored).

Fix (vr_glsl.h, QVR_ALIAS_FS_LIGHT): in_vao clamped to 0..1 before its use. Eyeshots at 1440 px, paused (the same
frame), the author's config, foveated 3 vs 0: the gun's barrels 65 near-white pixels before, 0 after; the pentagram 144
before, 0 after; foveated 0 unchanged. Left: two or three single coloured pixels on the gun's silhouette (the skin's
coordinates extrapolated past the triangle into other texels: coarse shading's own, not fixable with centroid without
MSAA). Cost: one clamp. Foveation still pays: e1m1 start, 2016 px eyes, GPU eyes 3.3-3.8 ms at foveated 3 against
4.2-4.5 at 0 (world+brush 1.2-1.7 against 2.5-3.0). Scripts in the worktree's scratch (fovshot.sh, pentshot.sh).

In VR:
- [ ] Foveated rendering on, hold the super nailgun and look past it (and at a pentagram of protection) with the corner
  of your eye: no white lines on its edges.

## Release script (2026-10-07)

Vittorio asked for one script that prepares and publishes a release, with no branch name in it (`vr-ironwail` will
become the main branch). `Misc/release/make_release.ps1 -Version x.y.z [-DryRun | -Publish]` (and `make_release.sh`
for Git Bash); the steps for him are in [RELEASING.md](RELEASING.md). It reuses what was there:
`package-quakevr.ps1` (new `-Dist`, `-NoZip`, `-Version`), `Misc/quakevr/make_release.py` (the zip and latest.json)
and the installer's tests. New on the way:

- The version is stamped for the build only: `/p:QvrReleaseVersion` (quakevr.props) makes the engine say
  `1.0.0 (2026-10-07 afd53921)`; the manifest and latest.json carry the same text; the installer gets `/p:Version`.
  Nothing is committed; the annotated tag records it.
- The installer's single-file publish failed (IL3000, warnings as errors) on the `Assembly.Location` test in
  `MainViewModel.OwnSetupFiles`, which is exactly the "am I a single file?" test: suppressed there with a comment.
  The publish also needs `IncludeNativeLibrariesForSelfExtract` (WPF's native DLLs inside): one 59 MB exe.
- `qvr-setup feed --file latest.json --assets <folder>`: the feed parsed as the installer parses it, each file's size
  and SHA-256 checked (exit 1 on a mismatch).
- Checks run on the artifacts themselves: the zip against `package-quakevr.ps1 -DryRun`'s allowlist and for id's
  files; the packaged `QuakeVR-Setup.exe` installs the zip with its off-screen harness, offline, into the output
  folder, and `qvr-setup verify` checks it; the packaged `ironwail.exe` (from the zip, with linked id paks) loads
  `start` with the mock headset and names the build in `version`.
- Tested without publishing (version 0.9.0, nothing tagged): the full local run passes; `-Publish` is refused by the
  preconditions (no upstream, no ericw-tools source zip); a temporary upstream showed the "not pushed" and "tag on
  another commit" (local v0.8.2, origin's v0.0.8) checks.

Open: the tags v0.8.0 to v0.8.2 exist only in the local repository (not on GitHub), so the generated notes run from
v0.8.2 (about 1900 commits, capped at 150): write the first release's notes by hand (`-Notes`). The newest GitHub
release is the HQ texture pack (`textures-2026-10-03`); a game release published as latest takes over
`releases/latest/download/latest.json`, which is what the installer wants.

## Immersive reloading: the super shotgun broken open (2026-10-07)

Phase 2b of RELOAD.md (worktree `reload`), with immersive reloading on and Weapons > Reloading > Super Shotgun >
Break Open (`vr_reload_ssg_break`, 1):

- **Fired, its shells stay in** (QC `QVR_WPNFLAG_SSG_SPENT`: 0-2 spent shells, in the weapon's flags; travels with the
  gun). No holster, button or flick reload for it.
- **Breaking it open** throws the spent shells out (the engine's casings, out of the chambers as drawn open, with a
  pop: `reload_ssg_eject.wav`) and any live ones as rounds lying about (taken again or refunded at the pouch). It stays
  open (`QVR_WPNFLAG_SSG_OPEN`): drawn open, the trigger only clicks (`gunclick.wav`). Two ways:
  - the flick (the old flick reload's gesture or `+flickreload*`): no reload and no spin any more, it only breaks open;
  - the pry: both hands on it (the two-handed grip on its fore-end, the grip held), the front hand's angle below the
    back controller's aim going up by `vr_reload_ssg_pry_angle` (30 degrees) from its least since they took hold, at
    `vr_reload_ssg_pry_speed` (100 deg/s) or faster: the front hand pushing the barrels down, or the stock lifted. The
    engine detects it (vr_flick.cpp: the server's hand angles are the two-handed aim, which follows the hands' line) and
    sends it as the gun hand's flick bit for a moment. A steady two-handed aim moves both together: it never pries.
- **Loading**: the pouch always gives it a taped pair (`vr_shell_pair.mdl`); open, the pair's middle at its breech
  (`vr_reload_port_sshot_x/y/z`, radius `vr_reload_port_sshot_radius` 3; the point turns down with the barrels; Show
  Load Points draws it) loads both. **One chamber free** (a live shell picked up from the floor loaded first): the pair
  loads one and the other stays in the hand (as the shotgun's pairs); shut, a pair is refused with the "can't" click.
- **Closing**: the flick again (`vr_reload_ssg_close_flick` 1), the barrels lifted back with both hands (the reverse
  pry, as far and as fast: `vr_reload_ssg_close_pry` 1), or by itself once both are loaded (`vr_reload_ssg_close_auto`,
  0). The close: `reload_ssg_close.wav` and a heavier buzz in both hands.
- **The model**: `Misc/quakevr/make_ssg_open.py` cuts `v_shot2.mdl` at its hinge into `vr_ssg_frame_on_v_shot2.mdl`
  (the frame, its open front closed by a standing breech with two firing pins) and `vr_ssg_barrels_on_v_shot2.mdl` (the
  barrels and fore-end, their back closed by a breech face with two chamber mouths; skins 0, 1, 2: the chambers empty,
  one, both loaded with brass heads and primers), Quake palette, normal maps baked. Closed, the gun is drawn as itself;
  open (and opening, closing: 450 and 900 deg/s, smooth at any frame rate) as its two parts, the barrels turned
  `vr_reload_ssg_open_angle` (35) down about the hinge (vr_view.cpp setupSsgParts); held or holstered (lying in the
  world it shows shut). Its two-handed grip turns down with the fore-end.
- **Sounds**: the open and the close cut from zer0_sol's CC0 pack (`make_reload_ssg_sounds.py`; CREDITS.md), the
  eject pop synthesised.
- **Immersive off** (or Break Open off): the flick reloads it as before (the self-test and reload_test.sh check it).
- Tests: the self-test 66 of 66 (its super shotgun section); reload_test.sh section 7 (46 of 46 in all).
- Not done: a gun lying in the world is drawn shut even when open; its muzzle point (the aim line) stays where the shut
  barrels are while open (it can't fire then).

## Put-away transition (2026-10-07)

The author: a transition when an item is collected into a holster or the ammo pouch: instead of vanishing, the thing
becomes smaller and fits into the holster, then disappears (worktree `collectfx`).

- **What:** a box or backpack let go of at a holster (into the pack), a key, rune, suit or the horn taken at a holster,
  an ammo box or a round let go of at the ammo pouch, an unarmed hand grenade put back in the grenade pouch. Not a box
  taken with the trigger away from a holster (nowhere to go), not armour (worn).
- **Gameplay unchanged:** the server takes it at once as before (the pickup, its sounds, the ammo or key given), then QC
  (vr_carry.qc `VR_CollectFx_Note` before the take keeps its model and pose, `VR_CollectFx_Send` once it is gone; in
  `VR_Carry_Take`, `VR_Reload_LetGo`, `VR_HandGrenade_LetGo`) calls the `collectfx` builtin, which sends that player's
  client QVR_SVC_COLLECT (32: hand, hotspot, entity, model index, origin, angles; reliable, to that player alone). Other
  players see it vanish as before.
- **The drawing** (vr_collectfx.cpp): the message comes with the update that no longer has the entity, so the client
  still holds it as drawn last frame (in the hand, vr_held.cpp) and copies it (model, frame, skin, scale, its networked
  scale and offset), else builds it from the server's pose. The copy's middle goes from where it was into the
  holster's or pouch's point (body::holsterPosition, ammoPouchPosition, pouchPosition) while it shrinks to
  `vr_collect_fx_size` of itself, both on an ease-in (t^2), over `vr_collect_fx_time` of vr_gametime (slowed in bullet
  time; the same at any frame rate). The start is kept as an offset from the target, placed on the body every frame, so
  it follows the body. The shrink is applied about the origin first in vr_render.cpp's applyPre (all the item's own
  transforms inside it) and the origin set so its drawn middle is where it should be. The box copies cast shadows as the
  boxes do (vr_lighting.cpp collectBrushes takes brush casters from cl_entities only; without it a held box's self-shadow
  vanished at the release, a brightness pop). Cleared on a new map and a load; skipped in demos.
- **Settings:** Carrying and Throwing > Carrying ("Put-Away Transition" `vr_collect_fx` 1, "Put-Away Time"
  `vr_collect_fx_time` 0.25 s, "Put-Away End Size" `vr_collect_fx_size` 0.2). Debug > Logging: "Put-Away
  Transition" (`vr_debug_collect_fx` 1 each thing, 2 each frame). Test aids: `vr_mock_hand_to <hand> holster <0..5>` and
  `grenadepouch`; Debug > Tests > Thing: Silver Key (`vr_test_spawn 113`).
- **Seen:** with the hand at the holster the thing's middle is only a few units off the holster's point, so it is mostly
  the shrink, with a small drop into the point. Test: Misc/quakevr/collectfx_test.sh.
## The marksman ogre outside Honey (2026-10-07)

"I can't spawn the marksman ogre in the firing range. The console says I need the Honey model."
- `monster_ogre_marksman` is Honey's marksman (its model `progs/mogre.mdl`, its crown, far sight, lobbed grenades, its
  chainsaw's extra damage to ogres: `VR_IsHoneyMarksman`, by its `.wad`) only in a Honey map (`vr_honey_context`, or
  one of `VR_GameUtil_InHoneyMap`'s) with Honey's data there. Anywhere else (the firing range, id's maps, a third-party
  map such as Down the Gutter, Dimension of the Machine) it is Dimension of the Machine's marksman, as id's ogre.qc has
  it: id's ogre model and behaviour under the marksman's classname. Before, outside Dimension of the Machine every
  Honey branch took any marksman, and without Honey's model a map's marksman became a `monster_ogre`.
- The Debug spawner's Marksman Ogre (Thing 20) and the dispensers no longer ask for Honey's model.
- Test aid (Debug > Tests): `vr_marksman_test` 1 the nearest ogre (model, Honey's or not, health, enemy, grenades), 2
  killed, 3 its body. e1m1: it spawns with id's model, hunts and chainsaws the player (100 to -3), dies, ragdolls;
  vrfiringrange: spawns, dies, ragdolls (the range's spawned ogres, plain ones too, don't notice the player); MG1 hub
  (`vr_mg_hub_test 38`): unchanged, 20/0 on `vr_mg_hub_test 1`.
- Not tested: Honey's own marksman (no Honey data in the test base); the logic there is unchanged.

## A cap on dropped enemy weapons (2026-10-07)

"A global limit of maybe 32 or 64 on the ground; when we reach the limit, we start deleting them in a circular fashion.
Only the one-use-only weapons."
- The grunts' burst rifles, the enforcers' laser rifles and the ogres' chainsaws that monsters dropped (their weapon
  record's `.vr_enemy_drop`, set as a monster drops one: it stays with the gun through hands, holsters and drops) lying
  about: at most `vr_enemy_weapon_drop_max` (48; 0: no limit; Combat > Enemy Weapons > Most Lying About). Past it, as
  one more is made (`CreateThrownWeapon` -> `VR_EnemyWeapons_MakeRoom`, vr_enemyguns.qc), the oldest (`.vr_drop_born`,
  saved: the order survives a load) fades away in 0.4 s (nothing can take it meanwhile). Spared, the next oldest going
  instead: one carried, pulled by a force grab, or moving faster than 40 u/s (in flight). One in a hand or a holster is
  no prop: never counted. A map's or a dispenser's weapons (the firing range's chainsaw), every other drop and prop stay.
- Not kept: the mark across a level change (a carried enemy gun's record is remade there: it is no longer capped).
- Test (Debug > Tests > Enemy Weapon Drop Cap): `vr_dropcap_test 1` (60 grunts spawned ahead and killed one by one,
  the first rifle taken into the gripped main hand, a chainsaw thrown up as the oldest, three other weapons dropped):
  vrfiringrange, the most lying at any tick 48 of 48, the others 3 of 3, the held one held, the flying chainsaw kept
  until it landed, then the first to go; saved and loaded, 10 more: the oldest (4.20 .. 5.30) went in order, 48 of 48.

## Melee phasing (2026-10-07)

The author: melee attacks sometimes don't seem to register because of the collision with the enemy's model; while a
swing is fast enough, the enemy's collision with the hand and what it holds should be disabled for a split second, as
an option to try (worktree `meleephase`).

- **What stops the hand at a monster:** only vr_model_collide (vr_modelcollide.cpp, "Hands Stop at Models", "Held
  Things Stop at Monsters"): the hand, its weapon and a prop held alone are *drawn* held out of the monsters' drawn
  triangles (up to `vr_model_collide_max` 20 cm; deeper, less; none at twice that). It is drawn only: endView takes the
  push back out of what the game reads, so the server's melee (QC vr_melee.qc VR_Melee_Sweep: the tracked hand's
  points swept against the model as drawn, grown by the melee tolerance; a sweep starting inside counts, t 0) tests
  the tracked hand whatever is drawn. The server's own hand placement (vr_handpose.cpp stopAtWall) already lets monsters
  through (ROUND15's fix for swords jerked back at monsters' boxes); Box3D's hand and weapon bodies meet props only;
  the carried props' QC traces are walls only. So no blow is lost to the stop itself: the headless punches below hit
  once with it on or off. What it does is show the swing stopped at the surface (the push eases in within ~12 ms)
  while the blow is tested further in, and a blow that misses or lands on another point than it looks.
- **The option** (Combat > Melee > Swing Through Enemies; vr_modelcollide.cpp updatePhase): `vr_melee_phase` 1 (0 by
  default: as before, for A/B): while the hand's grip goes at least `vr_melee_phase_speed` (2.25 m/s: a stab's least
  speed, 0.75 x Swing Speed 3; the controller's velocity, the player's own movement left out) and for
  `vr_melee_phase_time` (0.15 s) after it slowed, the monsters (Kind::Monster: FL_MONSTER or FL_CLIENT, corpses with
  them) are left out of that hand's test: its push let go at once. Walls (vr_handpose.cpp) and things lying about
  (vr_model_collide 2) still stop it. Fists and the melee weapons (the axe, Mjolnir and the Super Axe, the swords, the
  crowbar, the chainsaw: by model) always; a gun or a prop held alone with `vr_melee_phase_hold` 1 (default). After
  the phase a hand left inside a monster is eased back out (time constant 0.06 s instead of 0.012 s) until it reaches
  where the push puts it (at most 0.5 s), not snapped out in a frame. Hits are untouched: once per swing as before (the
  rearm rules, VR_Melee_Rearm). Debug > Views > Show Model Collisions (`vr_debug_model_collide` 1) also prints
  "melee phase main: on/off at t, m/s" and marks the lines "phasing" or "released".
- **Test:** Misc/quakevr/melee_phase_test.sh (7 of 7; fixed 90 Hz frames, mock punches with a closed fist,
  vr_melee_push 0): a 5.3 m/s punch through the training dummy, off: 1 hit, drawn held out up to 5.7 units; on: 1 hit,
  0.00 units drawn while phasing, then eased out over 22 frames (each closing at most 18% of the gap), out 0.23 s
  after; a 0.5 m/s push into it, on: 0 hits, held out up to 6.0 units, no phasing; a punch ending inside a grunt, off
  and on: 1 hit each, on eased out (at most 17% a frame); the shotgun swung into it with `vr_melee_phase_hold` 0: not
  phased (held out 5.3 units), 1: phased.
- **In the headset:** Combat > Melee > Swing Through Enemies on: punch and swing at grunts and the dummy, with the
  follow-through you'd use. Does the fist or the blade going through feel better or worse than stopping at the body,
  and do blows feel more reliable? Try Swing Through Speed lower (pushes pass through too) or higher (hard blows only),
  and Follow-Through longer if the hand is seen stopping inside at the end of a swing.
## Held props jittered over props on the floor

Your note: a prop in the hand (an ammo box, a torch) walked over boxes lying on the floor went up and down, as if it met
them; your theory: the bigger prop's original hitbox kept after the prop is made smaller. Branch `agent/heldjit`.

- **The cause** (your theory, on the client): `held::wallFrame` (a prop held in one hand drawn out of the walls) traces
  `worldtrace::world` with `ownFiles` (for the prop table, a func_wall of its own file), and so met every brush model of
  its own file lying round: the health, ammo and explosive boxes. Their hull is the model's own, at the origin, unscaled
  and unturned: a box of shells lying on the floor is drawn 4.8 units across about its origin (`model_scale` -0.8,
  `model_offset` -12) while its hull is a 24-unit cube from its origin up and aside, up to the height of a hand held
  low. The held prop (and the hand with it) was drawn pushed up onto that ghost box and let back down as the walk went
  on. Not the server: the floor boxes' `mins/maxs` (-2.4..2.4) match what is drawn, the held prop's physical place
  stayed 0.00 cm off its hand's place (`VR_Carry_Follow` traces nomonsters, which these are not), the player never
  stepped up on them (his z flat, but the step off the range's ledge), and the hand pushes were none but the walls'.
- **The fix** (`vr_trace.cpp`): `worldtrace::world` skips rigid bodies (`U_QVR_NOROTATE`: the things lying round,
  pushed in Box3D, as the held-props notes always meant) and any brush model drawn scaled or offset (VR or Ironwail
  scale): the hull isn't what is drawn. The prop table, doors and lifts are unchanged (a gib lowered onto the table:
  still held out of it, 50 wall lines naming `maps/vr_proptable.bsp`). Explosion debris (`vr_explosiondebris.cpp`, the
  other `ownFiles` caller) no longer bounces off the boxes' ghost hulls either.
- **Debug**: `vr_debug_carry 1`'s wall line now names the wall's entity, its model's box and its drawn box
  (`worldtrace::world` takes `hitEntity`); `vr_carry_check` prints the player's and the hand's z.
- **Numbers** (`Misc/quakevr/held_over_props_test.sh`, hand at 0.75 m on the right, 150 frames walking at 186 u/s):
  the held prop's drawn place in the hand, peak to peak, before -> after: shells box over 1 box 0 -> 0 (it missed the
  one box), 3 boxes 12.0 cm (16 pushes) -> 0, 5 boxes 15.4 cm (26) -> 0; torch over 3 boxes 46.2 cm (43) -> 0, over 5
  42.3 cm (93) -> 0. Floor boxes still taken by the fist ("carry: taken") and stacked three high asleep
  (`vr_physics_stack`, 4.7-4.8 units apart); twohand_regrip_test.sh 7 of 7; e1m1 smoke clean.
- In the headset: walk over the firing range's ammo boxes holding a box or a torch low: steady in the hand.

## Versions and the menus' version label (2026-10-07)

Asked: a versioning scheme, and "Quake VR: Unleashed - vX.X" over "by Vittorio Romeo" in the menus' bottom right corner.

- **One source of truth:** `VERSION` at the repository's root (`0.9.0`; semver, prereleases `-beta.N`). The engine
  (quakevr.props writes `QVR_VERSION`, `QVR_VERSION_DEV` and `QVR_BUILD_VERSION` into `qvr_buildver.h`; vr.cmake and
  vr.mk pass `QVR_VERSION`), the installer (Directory.Build.props: was 0.1.0, now VERSION's), a hand-made package's
  manifest and the release script read it. Builds are dev builds (`0.9.0-dev (2026-10-07 afd53921)`) except the release
  script's (`/p:QvrReleaseVersion`, refused unless it is VERSION's). `VR_Version`/`VR_VersionIsDev` (vr_crash.cpp).
- **Release script:** `-Version` optional (default VERSION's); another one stops it unless `-BumpVersion`, which commits
  VERSION alone ("Version x.y.z"; refused on a dirty tree or an older version; with `-DryRun` only said). The safer
  flow: nothing is changed or committed without the explicit switch. RELEASING.md, "Versions" (when to bump what).
- **Starting version 0.9.0:** the old Quake VR's tags reached v0.8.2; 1.0.0 is left for the release called finished.
- **The label** (vr_menubrand.cpp, VR_MenuDrawVersion; `vr_menu_version` 1, HUD and Menus > Menu: "Version Label"):
  two right-aligned lines at the status box's size (7 in the headset, 5 flat), the canvas's corner 4 in, in the menus'
  tan (turned red), the title at 85%, the author at 60%, a dev build's "-dev" at 45%. "vX.X": `vMAJOR.MINOR` while
  PATCH is 0, the whole version otherwise (v0.9.1, v1.0.0-beta.1). Drawn before the page (a drop-down over it hides
  it). Never over the menu: `qvr::menu::contentRightBelow` (VR pages: the rows to their scrollbar's box, the help at its
  widest; Search, Map Library, console keyboards; Quake/Ironwail menus via menu.c's `M_ContentExtent`: 320 x 200,
  Options' and the bindings' list to their last row, Ironwail's lists the menu's bounds) and the status box; where they
  reach under it, it is left out. A VR page whose rows or help reach under it (on a flat screen: 50-column help on a
  420-wide canvas) ends above it (`layout`, `versionLabelClearance`): VR unchanged (24/25 rows on VR Settings and Debug -
  Tools, label on or off); flat 20/20 rows (22/23 with it off). Left out on flat Key Bindings and Levels (their lists
  reach the corner). `menu_vr pos` prints it.
## A frame seen through after a teleporter (2026-10-07)

Walking through a teleporter, one frame round the crossing showed the room beyond wrong: brush entities missing (start's
pentagram floor, a func_bossgate, gone over the pit), or at 90-120 Hz a frame drawn from inside the gate's wall, and
then the view held still for one or two frames. Three causes, found with a frame strip (`vr_screenshot_frames`, below)
and per-frame prints of the client's lerp:

- **The server carried the player a tick late.** `VR_PortalClientCross` ran only before the move (VR_ClientSpecialMove),
  so the tick whose move took the torso past the gate's plane sent him there, not yet carried. The server's PVS for
  that message (`SV_WriteEntitiesToClient`, origin + view_ofs) was then behind the gate, so `VR_PortalAddPVS` skipped
  it and the destination's entities were not sent, while the client's eye was already through (`eyeThrough`, drawn from
  the destination): the room seen without its doors, lifts and floors for a frame. At a higher frame rate the client's
  lerped body could itself reach past the plane uncarried: its eye, not through, drawn from inside the gate's wall.
  Now the crossing is also tried after the move (`VR_ClientRoomscaleMove` with no room-scale move; with one it already
  was), so a message never has him past the plane uncarried. Same crossings (teleporter_cross_test.sh, teleporters_test.sh
  walk: every case's side and end place as before), one tick earlier.
- **`VR_PortalAddPVS` sends both rooms while he straddles a gate** (within `kStraddle` = 32 units of its plane, over
  its opening, either way through): a body a sill or a frame keeps half through, an eye still behind the exit's face
  just after the crossing (drawn from the source).
- **The client snapped to the newer place.** The client draws a tick behind (lerping from the older message's place to
  the newer); a jump of over 100 units was Quake's teleport, no lerp: the view leapt a tick ahead and held for a frame.
  `VR_PortalLerpFrom` (CL_RelinkEntities): when the older place carried through a gate lands within 64 units of the
  newer, the lerp goes on from it carried (and the yaw turned by the gate's). The hands also took the gate's turn
  through `addTurn`, which made them again in the middle of the frame from the carried body (a frame early, then the
  same frame twice): the turn is now kept for the next frame's hands (`setServerYaw`).

Frame strips (`Misc/quakevr/teleporters/teleport_frames_test.sh <agent> <rate> <fade> [start e1m1 flush]`: every frame
drawn round the crossing, the share of pixels changed frame to frame and a transient score, 4 frames written out):
start at 72 Hz, the hole frame's transient 2.2% before, 0.4% after; vrteleporters' flush gate at 120 Hz, a 59% frame
(the gate's wall) and two still frames before, an even 11-16% a frame after; e1m1's gate and the comfort fade at 0.6:
no transient (0.04%). What is left at a crossing is one step of
lighting: the room through the gate is a little darker (17.5 against 18.9 mean luminance at start): the shimmer
(vr_teleporter_surface_opacity, half of it) and the torches' lights in the view through (r_dynamic 0: no step), not
looked into. `vr_screenshot_frames <n>` (Debug > Teleporters > Frame Strip Through A Gate): a screenshot of each of the
next n frames drawn (a `wait` waits for a server tick and skips frames over 72 Hz), each with the time and the
player's place printed.
## Knockdowns: the get-up's jitter (2026-10-07)

Report: a knocked-down monster getting up jittered, as if replaying keyframes. Measured with a new debug aid,
`vr_knockdown_debug 2` (Combat > Knockdowns, Print Rolls: "And Get-Ups' Motion"; 3: each frame's): from the moment it
starts getting up, the client's drawn vertices frame by frame (ragdoll.cpp `watchGetup`/`recordGetup`): their mean
speed, the fastest frame, the frames that went back on the one before, and the switch from the ragdoll to the animated
model. Debug > Tests > Enemy Shoves: Knock Down the Nearest, Get Them Up Now (`vr_knockdown_test 0`/`1`).

Not the 72 Hz tick: `host_fixedtick 0` measured the same. Three causes, all on the client, all fixed:

- **The move replayed at every frame (the jitter).** Getting up moves a monster's origin to where it stands (up to
  88-98 units here, under the 100-unit teleport cut, so the client lerps it). A stepping entity's move lerp
  (R_SetupEntityTransform) ran from the move to the *latest* message's lerpfinish, which is its next frame's: the get-up's
  frames (0.125 s at Get-Up Speed 0.8, so they carry a lerpfinish) each pushed the end of the finished move later, and the
  body was drawn back along it and slid forward again, at every frame (the ogre: 16-27 units back, then forward, at each
  of its frames). Now the end is the one the moving message sent (`movelerpfinish`; `R_MoveLerpBlend`, also used by
  vr_ao and vr_modelcollide's copies).
- **The switch from the ragdoll to the animation flashed the old frame.** While the skinned ragdoll model is drawn,
  R_SetupAliasFrame doesn't run for the monster's own model, so its lerp stayed at the frame it was knocked down in (a
  standing one); drawn again it lerped from that frame to the get-up's (the grunt: a 21-unit jump and back). Now
  swapModels keeps its lerp up with its frames (`trackPose`).
- **Frame lerps wandered by the lerpfinish's rounding** (1/255 s, sent again in each message): the pose went back and
  forth a little at every message, several frames' worth in bullet time (the ogre: 35 frames back). The frame lerp's end
  is now the one the message that changed its pose sent (`animlerpfinish`, `R_FrameLerpFinish`).

Also the blend from the ragdoll (vr_box3d.cpp `updateRecoveries`) lerps the frames by the server's think times (from the
think that set the frame to the next, at the message's time) instead of from the step's time over the get-up interval:
it lagged a step (each frame's end skipped) and, in bullet time, held still.

Numbers (e1m1, grunt / ogre / knight / fiend; the fastest frame against the mean speed; frames back; the switch frame's
speed against the frames' before): before 84x / 78x / 54x / 84x, 2 / 2 / 0 / 2 back, switch 60x / 83x / 16x / 44x; after
8x / 12x / 11x / 7x (the ragdoll's blend itself), 0 / 1 / 0 / 0 back, switch 2.1x / 2.7x / 1.3x / 1.5x. Bullet time
(vr_timescale 0.25): back frames 3 / 35 / 0 / 18 before the frame-lerp fix, 0 / 1 / 0 / 0 after; the switch there is
still 9-22x a frame's (tiny) motion: the skinned rig's fit against the .mdl's vertex animation (about a unit), as when a
ragdoll is made.
## Immersive reloading: magazines, both grips, the pull (2026-10-07)

The author's notes from VR on the magazine guns (worktree `magfix`).

- **Both two-handed grips.** The magazine had replaced the gun's own two-handed hotspot (its grip moved to the magazine's
  middle): the front grip was gone. Now the magazine is a second grip beside the gun's own (a free hotspot index,
  vr_view.cpp `magSpot`): the other hand takes whichever it is nearer (the magazine by its box, below). Held on the
  front grip, the magazine is left alone; on the magazine, it is held (and pulled out from there).
- **The magazine's shape.** The client works out a box round the attached magazine's model (`vr_mag_on_<gun>.mdl`'s
  vertices along its length, `magazineShape`), places it with the gun as drawn (held or carried; taken back to the
  tracked hands as the muzzles are) and sends it with each move (`VrMove::magBox` -> `.magbox*`, `.offmagbox*`). An
  empty hand within **Pull Reach** of that box (now leniency on top of it, down to 0), nearer it than the gun's own grip
  (or a carried gun's handle), is on it: hotspot **HS_MAGAZINE** (13). Gripping there holds it (QC latch), and the
  two-handed grip takes it there. **Hits** anywhere on the box knock it out (the fist, a held prop, the other gun's line
  sampled along it): `knocked out by a hit at N m/s, D units from it, T from its top`.
- **However the gun is held.** The magazine works the same for a gun held by its handle in either hand and for one
  carried anywhere on it (the hand-off; QVR_WPNFLAG_FOREGRIP_CARRIED): the pouch gives its magazine (or shells), its
  well takes one, the other hand pulls it out, a hit knocks it out, B/Y drops it.
- **The pull (what was wrong).** "Out at once, or never, whatever I set":
  1. The wrist snap read `.handavel`, which is the *throw's* spin estimate: while the grip is held, "as if let go now"
     (the spin at the last speed peak of the window); for a hand not gripping, the spin of its last release, kept until
     the next grip. The difference of two such numbers is noise: after a quick reach for the magazine (or a gun hand
     whose last release spun), over any threshold at once; otherwise never.
  2. The hold latched only within Pull Reach of the well's top (a point), while the two-handed grip took the magazine
     5.5 units round its middle. Measured on the magazine's surface: 1.2-1.3 units off the top at its feed end, 2.0-2.3 at
     its middle, 2.6-4.7 at its far end. At his Pull Reach 2, holding it at its middle or below never latched: the
     two-handed grip held a magazine that could then never come out.
  3. The hand reaching to hold it at Knock Out Speed knocked it out before the grip latched (he had raised Knock Out
     Speed to 6.5).
  4. The pull's speed was taken along the line between the hands (from the handle), not the magazine's way out: the
     thunderbolt's cell (ahead, below) pulled straight down counted a fraction.
  Now the snap is the hand's turn against the gun hand's (their poses, over 40 ms windows: `VR_Reload_SnapRate`, the
  same at any frame rate), the pull the hands' relative speed (m/s, the controllers') along the magazine's length out of
  its well, the latch the hotspot, and a hand gripping onto the magazine is a hold, not a hit. Apart is still the
  hands' distance (the controllers') against what it was as the grip took hold. Units: speeds m/s, distances world
  units, snap degrees/s.
- **The hand on it.** Holding the attached magazine, the hand is turned the least from the tracked hand so that its
  grip channel lies along the magazine where it holds it (`alignChannel`, kept 1.5 units in from its ends), and the
  fingers close round the magazine's model (not the gun's). Per magazine, in its gun's page: Hand X/Y/Z, Pitch/Yaw/Roll
  (`vr_reload_hold_<nail|snail|light>_*`, looks only) and Fingers Set by Hand with five curls.
- **The ammo button.** "About once in a hundred": it was checked every 0.2 s, so a fingertip coming at it was first
  seen well inside its reach, often past its face, and refused as from behind. Now every frame (it leaves again 0.5
  units further out; 0.2 s between presses), Ammo Button Cone 80 (was 50) and a new **Ammo Button Depth** 0.5 (the
  cone's tip that far behind the button's middle). Mock approaches, a straight line in at a quarter unit a frame, four
  ways round: 0-60 degrees off its face 20 of 20 (before: 0 of 20), 75-90 degrees 8 of 8, 120-180 0 of 20.
- **Menus.** Weapons > Reloading is General (the mode, Show Load Points, Collision Leniency, Eject Button, the pouch,
  feedback, dropped rounds) and a page per gun: Shotgun, Super Shotgun (phase 2b's rows), Nailgun, Super Nailgun,
  Thunderbolt (each its well, its magazine's top, its pose in the hand, its receiver, the hand holding it, and the pull
  and knock settings, shared by the three). Ranges wider (Pull Reach and Knock Out Reach from 0). Debug > Tests >
  Reloading: Reload Prints 0-3 (3: each empty hand's distance off the other gun's magazine box), Show Load Points (the
  magazine's box in blue).
- **The author's values** (his ironwail.cfg) as defaults, `vr_cfg_version` 98: pouch counter X 2.25, Z 2, Size 0.6,
  Follow Legs 0.5; port and well radii 1.5; Collision Leniency 4; Knock Out Reach 2; Pull Reach 2; Pull Speed 2; Pull
  Wrist Snap 300 (his 225 was set against the broken reading); Knock Out Speed 3 (his 6.5 was set against the reach that
  knocked it out; 6.5 migrates to 3); the live shell's grip Y 0, Z -1 (`vr_props_version` 64).
- Tests: reload_test.sh sections 8 and 9 (TESTING.md). Mock: `vr_mock_hand_to <hand> mag <along> [<out>]`,
  `vr_mock_hand_to <hand> wbutton <degrees> [units] [azimuth]`.

In VR:
- [ ] Nailgun, super nailgun, thunderbolt: take the front grip, then the magazine: both aim two-handed.
- [ ] Hold the magazine and move gently: it stays. Yank it out along its length, snap the wrist, or move the hands
  apart: out into the hand. Tune Pull Speed / Wrist Snap / Apart / Reach in the gun's page.
- [ ] Hand-off the gun to the off hand (let go of the handle while the other holds the front grip): pull its magazine
  with the free hand, load one from the pouch.
- [ ] Punch the magazine's far end: it pops out.
- [ ] The hand on the magazine: does it look right? Tune Hand X/Y/Z and the turn per gun.
- [ ] The ammo button from the front, as you press it: does it press every time? From behind: never.

## Map Library: no 200 MB limit on packages (2026-10-07)

Asked: the Map Library would not download the Liminal Spaces Jam (510,755,797 bytes in the index, 1013 files):
"the package is 487 MB (up to 200 MB)". The limits were `vr_mapinstall.hpp`'s constants: `maxZipBytes` 200 MB (the
index's size before the download, and the bytes as they arrived) and `maxUnpackedBytes` 400 MB (the zip's files), with
`maxFiles` 4000; the whole zip was downloaded into memory, hashed, written to the cache, then unpacked from memory, with
every root pak held in memory until the end. Now:

- **No size limit by default.** `vr_maps_max_download_mb` (0 = none; Debug > External Map Index > Largest Download)
  refuses a larger package before and during its download, for whoever wants one. A server that sends over twice the
  index's size (and 16 MB) is stopped as not the package (the sha256 would fail anyway; the disk never fills).
- **Streamed to disk**: each mirror's transfer goes to `cache/maps/<sha256>.zip.part`, hashed as it arrives
  (`sha256::Hasher`, incremental; `vr_sha256_test` 12/12: the 6 vectors whole and fed in 1-97 byte pieces), renamed to
  `<sha256>.zip` once it matches the index (the sha256 check kept: a mismatch fails that mirror and tries the next).
  The unpacking reads the zip from its file (miniz's reader on a FILE*, seeks only when needed); one file in memory at
  a time, root paks taken out of the zip one at a time after the loose files. A part file is removed when its job
  fails, and a stale one (the game quit mid-download) at the next start-up. Trims never count it.
- **Free disk space checked** (`files::freeSpace`: GetDiskFreeSpaceExW / statvfs): before the download, the zip and as
  much again for its files (installing) plus `diskMargin` 64 MB on the cache's volume; before the unpacking, the files'
  own size plus 64 MB on the package folder's volume. The message: "not enough disk space: unpacking its files needs
  200.0 MB and 64.0 MB to spare, and 200.0 MB is free on <folder>". A write that fails mid-download says how much was
  free. `vr_maps_debug_free_mb` (Debug > External Map Index > Pretend Free Disk Space) simulates a low disk.
- **Zip-bomb guard by ratio**, not size: files that unpack to more than 100 times the zip (`maxUnpackRatio`, past
  `unpackRatioFloor` 256 MB) are refused ("1024.0 MB from a zip of 1020 KB (1028 to 1; up to 100 to 1): refused as a
  zip bomb"). Real packages are 1-10 to 1. `maxFiles` 4000 -> 20000.
- **The cache trim** already skipped the running job's zip; the part file isn't a cache name. Tested with the cap at
  1 MB, then 2 MB in the middle of a 300 MB install: installed, its zip removed only after the job.

Test: `python Misc/quakevr/maplibrary_large_test.py <agent>` (packages generated in scratch/maplib/, a test index
written over the agent's cached one and put back, served from 127.0.0.1:8766; one real-time run): low disk refused
before the download, `vr_maps_max_download_mb 100` refused, a wrong sha256 refused, a 1 GB-of-zeros bomb refused, low
disk (200 MB) refused before unpacking a 24 MB zip of 200 MB, a 300 MB stored package (incompressible) installed
("2 file(s) written (download 1842 ms, unpack 869 ms)"), then uninstalled and trimmed: 7/7.

- [ ] Map Library: Liminal Spaces Jam (487 MB): downloads, installs, plays (its start map).

## vrstart2 on ericw-tools 2.0 again: the qbsp holes' cause, leaner geometry, compile presets (2026-10-07)

Your request: vrstart2 compiled with 2.0's qbsp like everything else (no 0.18.1, no `lit_liquids`), the cause of 2.0's
lost faces fixed in the generator, fewer brushes, detail where right, a fast and a final compile preset.

**Why 2.0 lost faces.** Its qbsp makes faces from the BSP's portals and decides contents by flooding through them
(the fill). Slivers thinner than its epsilons break both: a portal whose brush side it cannot find gets no face ("N
sides not found"), and a missing portal lets the fill turn whole regions of air solid (slabs standing in the air, no
faces: most of the holes, often far from the sliver that caused them; deterministic but chaotic, any edit moves them).
Found by bisecting small regions of the map (a ddmin over brushes, qbsp on each subset) down to 2-15 brushes:
- terrain neighbours nearly coplanar (the old wedges), or a fraction of a unit apart along their edge: the old
  `terrain_planes` itself made those steps (a shared plane meets the neighbour's corner 0.1-0.5 off);
- 2,500 water tiles (0.18's lighting workaround) overlapping the terrain: 766 holes with 2.0;
- faces of one brush folded a fraction of a degree (points snapped to the 1/8 grid: crystals, beams, logs);
- corners a fraction of a unit through the ground or the water (boulders, logs, the cliffs' feet: a cliff corner 2 units
  over the water meets the waterline 0.3 units away);
- pine cones sharing one tip; rope pieces meeting at a degree or two; nearly level faces beside level ones;
- runs of nearly straight terrain edges (the jittered lattice's rows, the paths' edges): nearly parallel vertical
  planes meeting at a corner (a thin phantom slab stood out of the ground beside the pavilion's path);
- the fill itself: the default midsplit and the fill both made phantom solid; -forcegoodtree and -nofill none.

**Fixes** (mapgeom.py, vrstart2_gen.py): `terrain_mesh` (no steps: each prism's top through its own corners, exact
reals; nearly coplanar neighbours, under a unit across or 1.5 degrees, made exactly coplanar by moving a corner, as far
as 3 units across a slope; the island's walkable ground pinned: its steps of 8 are what keep the player from
snagging; tops within 0.6 degrees of level levelled; corners within 4 units across the slope of the water's surface put
on it), then exactly coplanar triangles of one texture merged into convex prisms; `unbend` (points moved 2-4 units off
nearly straight runs, at most 6); `simplify_points` (greedy insertion away from the island: lake floor 3 units,
cliffs 2, mountains 6: only 86 points go, the noise needs the rest); `hull` merges folds under 3 degrees, turns faces
within 1.5 degrees of an axis to it, settles corners within 1.5 units of the ground or the water 2 units clear; pines'
cones end three quarters up the cone above; boulders' undersides 4 under the ground; ropes in 2 pieces (were 3-5); the
water one brush; the sealing floor's outer faces skip. **Compile** (`compile_map`): qbsp twice at once, spliced
(`bsp_splice.py`): hull 0 from `-nofill -noclip -forcegoodtree -tjunc rotate`, the clipping hulls from a normal run
(unfilled they were 18 million clipnodes, a 250 MB .bsp). `-tjunc rotate`: 2.0's default cut 16,000 more faces.
Presets `--preset fast|final` (`--help`, MAPPING.md); `--check N` runs the new hole test, `bsp_holes.py`.

| | before (0.18.1 qbsp) | after (2.0) |
|---|---|---|
| .map brushes | 14,399 (2,500 water tiles, 10,518 terrain) | 10,432 (9,073 terrain prisms) |
| world faces | 76,869 | 81,198 |
| hull 0 leaves / nodes / clipnodes | 35,200 / 56,832 / 124,036 | 52,467 / 110,393 / 173,372 |
| .bsp | 18.1 MB | 23.1 MB |
| holes (bsp_holes.py) | 0 in 1,000,000 rays | 1 hit in 300,000 (a sub-unit sliver on the lake floor), 0 in another 300,000 |
| qbsp "sides not found" | (0.18: n/a; 2.0 on the old map: 435) | 71 (none of them a hole the rays find) |
| final compile | qbsp 39 s, vis 9 s, light 356 s: 6m50 | qbsp 19 s and 285 s (parallel), vis 16 s, light 458 s: 12m55 |
| fast compile | qbsp 37 s, (no vis), light 23 s: 1m07 | qbsp 23 s and 239 s, vis 16 s, light 58 s: 5m32 |

(Compile times on the shared machine: +-20%.) The hole count is not 0 in every build: 2.0's slivers are chaotic and
every variant tried had 0-9 hits in 600,000 rays (0.18.1: 0 in a million); this one had the fewest.

**Tested**: the ray test (above); before/after shots from 11 fixed places (`scratch/contact_vs2bsp.png`: before, after,
difference x4; mean differences 0.5-3.5 per channel against 0.4-1.8 between two runs of the same map, the campfire view
16 against 9: flames, smoke and a barrel's random skin; the teleporter's surface is lit now, as 2.0 always made it);
the walk test 18 of 18 three times (the old map also misses a leg now and then: 1 of 4 runs); the tutorial, a lectern
and Turning pressed by hand (`vr_debug_wallbuttons`: the same three buttons as on the old map); the 20 barrels and
crates resting within 2 units of where they rested; 289 recovered clip brushes (180); e1m1's smoke test. **Loads**
(exclusive, 4 alternating runs, hull files there): cold 1,240 ms (old 1,212), warm 310 ms (old 282): the water's
surface is 7,500 faces (5,700: its wave mesh +15 ms) and hull 0 is twice the nodes.

### Back to qbsp 0.18.1, the cleaned-up map kept (the author's decision)

qbsp is 0.18.1's again (`-bsp2 -splitturb`, one run; `bsp_splice.py` kept, unused), vis and light 2.0's. `-splitturb`
cuts the water's faces to 240 units and leaves them lit: no `lit_liquids` patch, the water one brush. Fixed on the way:
`hull`'s axis turn put faces facing -x/-y/-z at the mirrored coordinate (8 of the terrace's stones were broken brushes:
0.18 said "Couldn't create brush faces"; 2.0 had dropped them silently). Results (final preset):

| | shipped (0.18.1, old map) | now (0.18.1, cleaned map) |
|---|---|---|
| .map brushes | 14,399 | 10,432 |
| faces / leaves / clipnodes | 77,042 / 35,200 / 124,036 | 71,398 / 32,307 / 151,675 |
| .bsp | 18.1 MB | 17.7 MB |
| holes (bsp_holes.py, 1,000,000 rays) | 0 | 0 |
| final compile | qbsp 39 s, vis 9 s, light 356 s | qbsp 31 s, vis 7 s, light 399 s (7m32) |
| fast compile | qbsp 37 s, light 23 s (no vis) | qbsp 36 s, vis 7 s, light 40 s (1m34) |
| load cold / warm (exclusive, 4 alternating) | 1,196 / 285 ms | 1,256 / 299 ms |

The load is 14 ms slower warm and about 60 cold: `VR_NewMap: liquids` (the water's wave mesh) takes 117 ms against 102
(5,278 water faces cut along the BSP by -splitturb, against the old 2,500 160-unit tiles; -subdivide 160 made it 128);
the rest is the same. Tested: walk test 18 of 18 in 4 of 5 runs (one run missed the terrace and bridge legs: the old map
also misses one now and then), the three buttons pressed by hand as before, the 20 barrels and crates within 2 units of
their old rest, `vr_menu_path_check` 0 missing, e1m1's smoke test. The contact sheet (`scratch/contact_vs2bsp_q018.png`
in the agent's worktree): the same look (differences 0.6-3.8 per channel, the campfire's 12 its flames, smoke and a
barrel's random skin) except the teleporter's surface, now lit and showing its texture (as the first 2.0 builds drew it;
0.18 without -splitturb left it unlit and dark).

## vrstart2 becomes vrstart; the old hub is vrstart_old (2026-10-07)

The author's request: the island is the hub. `git mv`: `vrstart.bsp`/`.ent` -> `vrstart_old.*`, `vrstart2.bsp/.lit/.lux/.map`
-> `vrstart.*`, `vrstart2_gen.py` -> `vrstart_gen.py` (MAPNAME `vrstart`; the .map the same but its header line),
`vrstart2_walktest.py` -> `vrstart_walktest.py`. Engine: `VR_HubMap()` is `vrstart`, or `vrstart_old` when `vr_hub_map`
names it (default `vrstart`); `VR_IsVrMap` knows `vrstart_old` (and `vrstart2`); **`VR_MapAlias`** (SV_SpawnServer):
`vrstart2` loads `vrstart`, so an old save made on the island, a bind or a script still work (the same map: the save's
entities fit). `vr_cfg_version` 99: a config's `vr_hub_map vrstart2` becomes `vrstart` (without it, it would be
`vrstart` anyway). Seen tips: `tips_seen.txt` keys `vrstart2:...` count as `vrstart:...` (the island's tips stay seen;
the old hub's tip names are different). Debug > Tests > Hubs > The Old Hub (`vr_campaign_hub vrstart_old`). The
tutorial's, calibration room's, test hall's and example map's ways back already said `map vrstart`: they lead to the
island now. Bench scenario `load_vrstart2` -> `load_vrstart`; `stray_press_test.sh` loads vrstart and vrstart_old; docs
(MAPPING, TESTING, FEATURES, GRAPHICS, HULLS, RELOAD_PLAN, EXPANSIONS), the checklist, code comments and the FGD's
help follow.

**Not aliased: a save made on the old hub** (mapname `vrstart`) now loads the island with the old hub's entities
(wrong). The hub is rarely saved in; the alias would need to tell the two apart (the save's entity count, say).

**Tested**: a fresh start (`vr_startgame`) and `vr_campaign_hub` land in vrstart; `vr_campaign_hub vrstart_old` loads
the old hub; `map vrstart2` loads vrstart; `changelevel vrstart` from the calibration room; a save whose map was
renamed to vrstart2 loads on vrstart; a config at vr_cfg_version 98 with `vr_hub_map vrstart2` comes up `vrstart` (99);
`vrstart2:vs2_welcome` in tips_seen.txt shows as seen in vrstart; the walk test 18 of 18 (twice), the three buttons, the
bench scenario `load_vrstart` validates (3 loads, the same twice), `vr_menu_path_check maps/vrstart.map` 0 missing,
e1m1's smoke test.
## Immersive reloading: the author's notes on the super shotgun, and the shells sliding in (2026-10-07)

- **The rear sight's ring** (a piece of its own over the cut) is on the barrels part now: it swings open with them
  (make_ssg_open.py; it floated on the frame).
- **The sights' colour**: the guns' parts drawn instead of the gun (the shotgun's auto pump parts, the open super
  shotgun's) were not recoloured as the gun's sights are (vr_sights.cpp lists the models by name) nor glowed as it
  (vr_weapon_glow's boost asked the part's own slot): the sights turned the painted orange-red as the shotgun pumped and
  the super shotgun opened. Both now (weapons::slotForPart: a part's gun's slot).
- **Each way to open and close it is its own switch** (Weapons > Reloading > Super Shotgun: "Open by" and "Close by"):
  open by the flick (`vr_reload_ssg_open_flick`), the pry (`vr_reload_ssg_pry`), a hit from above (`_open_hit`), B/Y
  (`_open_button`); close by the flick (`_close_flick`), lifting the barrels (`_close_pry`), a hit from below
  (`_close_hit`), by itself once loaded (`_close_auto`, off). Every opening throws every shell out, spent and live.
- **Thresholds**: the flick's own speeds to open and to close (`vr_reload_ssg_flick_open_speed`, `_close_speed`: 650
  deg/s; the classic flick reload's 6.5 rad/s was 372: small flicks opened and shut it); the pry the author's 60 deg at
  250 deg/s (and the open angle his 45); the lift its own angle, speed and hold (`_lift_angle` 60, `_lift_speed` 300
  deg/s, `_lift_hold` 0.2 s: a jolt doesn't shut it; it was far too easy); the hits their speed and angle off the gun's
  own down or up (`_hit_open_speed` 3 m/s, `_hit_open_angle` 40, `_hit_close_*` the same) within `_hit_reach` (6 units)
  of the front 40% of the barrels (turned down, open), from that side (where it came from 50 ms before), not by a round
  in the hand nor in the 0.6 s after one went in (the loading hand drawing back), nor while both hands hold it (the pry
  and the lift are theirs).
- The pry is sent on the other (front) hand's flick bit, so the server tells it from a flick (QC VR_Reload_SsgFrame).
- **The hand on the open barrels** turns down with them (vr_view.cpp: the held hotspot's world turn).
- **The pouch** gives a single shell when one chamber is loaded (a pair only for two empty ones).
- **The shotgun's loading port** is a well now (polish_weapons.py loading_port: a steel housing 0.9 deep under the keel,
  its inner walls lined from dull steel at the mouth to black up at a black ceiling; parts can't be cut out of the old
  mesh without moving the anchors). The load point is where it was.
- **The shells slide into the gun** (`vr_reload_insert_time`, 0.13 s; vr_collectfx.cpp's "into the gun" variant, the
  collect message's hotspot 240): the round is loaded at the contact as before (the count, the sound, the haptics) and
  the hand lets go; its copy slides to the load point and on (up the shotgun's well into its tube; into the super
  shotgun's chambers), at its size, carried by the gun in its model space (it follows the gun as it moves), then is gone
  inside it. A magazine's seat slide is noted for later (RELOAD.md).
- Mock: `vr_mock_turn_velocity 1` (the hands' angular velocity from their turns). Tests: reload_test.sh sections 7 and 8
  (75 checks in all), the self-test 67 of 67.
## Flashlight cord: the coiled cord back, as a choice (2026-10-07)

The author: some people liked the coiled cord; restore it, not as the default. Branch `agent/coilcord`.

- **Cord** (Body > Flashlight): None / Chain / Coiled, `vr_flashlight_cord` 0 / 1 / 2; the default stays 1 (the
  low-poly chain, compiled and `vr_defaults.cfg`); no config migration (a config's 0 or 1 means what it did).
- **Coiled** is the cord removed in 8b90a36d, as it was: `coil::Style::turns` (64) and `coilRadius` (6.5 mm) back,
  the helix round the same simulated line in `Cord::build` (its turns keep their wire's length: stretched, they open
  out and the coil narrows; tapered over 1.5 cm into each end), 3.8 mm dark wire (albedo 0.14), 8/6/4 segments a turn
  and 6/5/4 sides by distance. Its line, springs, mass and relaxed length (0.243 m) are the chain's, so it hangs,
  stretches and swings as the chain does; the clip, the flick turn-over and hand-to-hand passing move its ends as they
  do the chain's. Like the chain it is drawn in the opaque scene only (neither casts a shadow). The chainsaw's starter
  cord (turns 0, a plain cable) is unchanged.
- Checked (mock, e1m1, torch in the left hand): `scratch/cord_compare.png` (chain relaxed / stretched over coiled
  relaxed / stretched); coiled 513 rings x 6 sides (6.1k triangles) near, chain 405-459 rings x 4 sides; `vr_profile`
  "flashlight cord" CPU 0.026-0.041 ms coiled vs 0.019-0.031 chain, the frame's GPU time the same (0.84-0.88 ms);
  flick_test with Coiled flips as with the chain; flash_grab_test `all` with Coiled fails the same spots as with the
  chain (mounted 6, returning 3: not the cord's).

Checklist:

- [ ] Body > Flashlight > Cord: None, Chain, Coiled; Chain the default. Coiled: springy, its turns opening as you pull
  the torch away, sagging and swinging; flick the torch over and pass it hand to hand.

## Flashlight grab test: stale since the saved attachment (2026-10-07)

`flash_grab_test.py all` failed at HEAD (6c253416), the same each run (3 of 3): mounted 12 spots "lit and not taken"
(6 printed), returning 3, and 9 more checks the earlier report missed (timing 1, options 4 "let go, onhead", gunzone 2
"let go at the butt, ongun", clipon 3). Not a VR regression: the test went stale.

- **Culprit: a236bb0d** ("Address October 4 VR playtest notes", 2026-10-04): the flashlight's on/off and attachment are
  saved and restored, so turning the flashlight off (`vr_flashlight 0`, the `!enabled()` branch of `setupView`) no
  longer puts it back on the belt. The test reset the lamp between trials with `vr_flashlight 0; wait2;
  vr_flashlight 1`; since then a lamp clipped on stayed on the head or gun into the next trial (the options, gunzone and
  clipon failures), and a lamp let go of was still springing home at the next spot (1751 of the 8019 `mounted` probes:
  mode returning). Bisected with `gunzone` as the oracle (dcb6d715, `all` passing, to HEAD; deterministic). A first
  bisect with `returning` as the oracle named 4be5e5bf, a PVS experiment's revert, by chance: those failures sit at the
  very edge of the reach and flip with any change.
- **The edge failures:** the probe runs before the view that precedes the mock press, so its "lit" is a frame stale; the
  tail of the flight home creeps a millimetre or two a frame, and a hand 9.0x cm off saw it lit, dark in the next view,
  and pressed on that (`reachesLamp` at the press: 9.126 cm, reach 9.0, the last view's lit 0). The press agrees with
  the view just before it, which is what a player sees: lit implies taken holds in VR.
- **Fix (test and test aids):** `vr_flashlight_home` (new; Debug > Views > Flashlight Home): the torch back on the belt at
  once, the light as it was; the test calls it before each `vr_flashlight 0` reset. The engine also remembers whether
  the lamp was lit for a hand at its last press (`vr_flashlight_probe` prints `presslit`, `presses`) and the test judges
  a press by that when the probe after it shows one more press (the returning trials press during the flight by
  design). A hysteresis on the lit state (lit to 1 cm past the reach once lit) was tried and dropped: a lamp creeping
  away still leaves any band a frame before the press. VR behaviour unchanged.
- **timing's t1_4_2.2_-6** (the head turned -45, 2.2 m/s, the grip pressed 6 frames, 18 cm, before the lamp): the
  press was nearer the left chest holster (hotspot 8) than the lamp, so the game's grip won there (a draw: by design,
  `gameGripWins`) and no late grip started; run alone, a degree of the torso's turn the other way, it takes the lamp.
  The probe now also prints `pressgame` (the game's grip won at the last press) and `timing` counts such a trial as
  "pressed at a holster (the game's)", not wrong. Its bisect is no use: before b4767f23 the worktree's tracked
  `quakevr/ironwail.cfg.baseline` (an October 4 config) was the runs' config, after it whatever the last run left.
- Runs without `quakevr/ironwail.cfg.baseline` in the worktree (the kit's, untracked) keep each run's cvars in
  `ironwail.cfg`: moving it aside for a bisect polluted later runs until it was put back.
## Reloading: the firing range notes of 10-07 (2026-10-07)

The author's notes vrfiringrange_2026-10-07_22-01-29 .. 22-14-33 (reload_test.sh section 10 checks each).

- **The super shotgun isn't broken open while it fires** (22-07-20, "major"). Opened during its firing animation, the
  gun was drawn in its two parts copied from the gun's entity, whose frame blending the renderer no longer moved on (it
  wasn't drawn): frozen at the firing frame, its muzzle flash held, open. Now every way to open it (flick, pry, hit,
  B/Y) does nothing until its animation is over (QC `VR_Reload_SsgFrame`: weapon frame 0; the log says "is still firing
  (frame N): not opened"), and the open parts are always drawn at rest (frame 0, no blending: vr_view.cpp `setSsgPart`).
  The magazine guns have no such race: ejected mid-burst their clip is empty and the animation ends by itself
  (`player_nail_BaseImpl`, `player_light1`); their magazine is drawn at frame 0 already.
- **A dropped super shotgun shows its state** (22-08-37). A super shotgun prop broken open is sent with
  `U_QVR_SSGOPEN` (the protocol's spare `U_UNUSED21`, the VR bits above 24 being full; a byte: its loaded chambers) and
  drawn open in its two parts as in the hands (vr_view.cpp `setupWorldSsgs`, the first 4 lying about; their own
  entities taken out of the frame's list), at the open angle, the barrels' skin its loaded chambers. `vr_reload_debug 1`
  with `developer 1` prints "ssg: a super shotgun lying open" once a second.
- **The super nailgun's magazine on its left, its ammo button on its right** (22-10-39). make_mags.py makes the
  magazine and its well on the right face as before and mirrors them onto the left one (seat (7.2, 5.44, 1.2), middle
  y 9.79: vr_view.cpp `loadPorts`, `magMounts`; the kick anchors 196 and 254, the mirrors of 192 and 250); the ammo
  button's Y and Roll mirrored (slots 4 and 12: -1.5, 65.4), and weapon settings version 37 takes them where a config
  still has the old ones. In the left hand the gun is drawn mirrored: the magazine stays on the inner side.
- **The thunderbolt's cell sparks and smokes** (22-14-04, 22-14-33). Seated or taken out (the button, the pull, a
  knock), a cell throws `vr_reload_battery_sparks` (14; 0 none) small blue-white sparks at the gun's well (QC
  `VR_Reload_CellSparks`; the new particle preset 17, ContactSparks: glowing, streaked, gone in 0.1-0.4 s). Taken out
  spent (empty), it smokes for `vr_reload_battery_smoke_time` (7 s; 0 never), held or lying about: a thin grey wisp off
  its top every tenth of a second, thinning out towards the end (`VR_Reload_CellSmoke` from the round's think; preset 18,
  BatterySmoke, each wisp `vr_reload_battery_smoke_alpha` 0.45 opaque). Weapons > Reloading > Thunderbolt, "Sparks and
  Smoke". The log: "the cell's contact sparks (seated|taken out)", "a spent cell smoking for 7 s", "the spent cell
  stopped smoking after 7.1 s: 52 wisps".
- **The shotgun's loading port no longer shimmers** (22-01-29). Its housing's four walls were boxes whose inner faces
  lay in the very planes of the well's lining (the MDL's vertex steps, 0.22 units along the gun and 0.03 across, put
  both on the same values): 29 pairs of faces in one plane, z-fighting. The housing is now its outer faces and its rim
  at the mouth only (polish_weapons.py `loading_port`, mitred at the corners), the lining its inner faces; the lining
  ends at the mouth (it stood 0.08 out past it). Checked: no coplanar overlapping pair left in the port
  (29 before); seen from below between two frames 202 pixels changed (511 with the old model). Other same-facing
  coplanar pairs remain elsewhere on v_shot.mdl (by the trigger at x 8, along the top at z 4.4, at the muzzle): not
  touched.
- **The author's tweaks are the defaults** (22-08-51; his config of 22:46 against the shipped defaults). Config version
  100 (`vr_cvars.cpp` defaultChanges: a config still holding the old default takes the new one):
  `vr_ammo_pouch_counter` 0 (was 1), `vr_ammo_pouch_x` 3.02 (0), `vr_decap_pop_sg_falloff` 1.5 (4),
  `vr_flashlight_flick_speed` 800 (600), `vr_knockdown_ledge_drop` 16 (64), `_margin` 24 (16), `_reach` 1.25 (1),
  `vr_reload_pull_snap` 225 (300), `vr_reload_ssg_flick_close_speed` 400 (650), `vr_reload_ssg_open_flick` 0 (1: the
  flick no longer opens the super shotgun), `vr_weapon_button_cone` 50 (80), `vr_weapon_throw_damage_mult` 0.35 (0.5).
  Held object settings version 65: the gremlin's head at Size 0.6 (slot 7). His weapon settings match the shipped ones
  (but for the super nailgun's button, mirrored above). Left as they are: bookkeeping (`vr_cfg_version`,
  `vr_props_version`, `vr_bindings_version`, `vr_xr_runtime`), his body (`vr_height_calibration`, `vr_bodycal_*`, the
  arms' `vr_body_elbow_back/hand/lift`: the player's), the motion recorder's (`vr_motion_*`), the menus' state and
  looks (`vr_menu_positions`, `_level` 2, `_scale` 0.16, `_distance`, `_fine_step`), the desktop window
  (`vr_window_view` 0, `vr_spectator_fov` 120, `vr_mirror_hide_hud_text` 1), performance (`vr_foveated` 2,
  `vr_detail` 0), comfort (`vr_comfort_vignette_strength` 0.5), slider noise (`vr_ammo_pouch_scale` 0.999,
  `vr_relight_strength` 1.1988), and the held object slots the game filled for him (`vr_prop_id_33`, `_54` to `_59`).
  reload_test.sh's super shotgun runs set the flick on (and its close speed 650) and its ammo button runs cone 80, as
  they test those ways.

## Stale in-game relights (relit_custom) no longer override a changed map

- **Bug**: `vrstart` loaded the old hub after the island took its name: `relit_custom/quakevr/maps/vrstart.bsp`, an
  in-game relight of the old hub, won by name (VR_ModelFile), and its `.relight` held only the settings hash.
- **Fix**: the `.relight` gets `source <game>/maps/<map>.bsp <size> <wyhash>` (the map's own file, also when light was
  given relight_maps.py's relit/ copy). VR_ModelFile uses a relit_custom copy only when `relight::customCurrent`
  finds the file the game loads as `maps/<map>.bsp` has that size and hash (read and hashed once per map and session:
  the answer is kept by the original's place, size and write time and the copy's write times). Otherwise the copy's
  four files move to `relit_custom/_stale/<game>/maps/` (an older stale copy there replaced) and one console line
  says so. A `.relight` without the line (all made before this) counts as stale: a relight is ~1 s a map. A batch
  skips a map only when the hash and the source line both match (`relitAlready`), so it relights changed maps and
  old-format ones.
- **Checked, unchanged**: relight_maps.py's `relit/` copies (id1, hipnotic, rogue only) keep a `.relit` stamp that
  hashes the map with the command, which the script checks; the engine does not verify them. The disk caches
  (hulls, AO, textures, images) are keyed by content or the opened file's identity. The installer's first-start
  marker only starts a batch.
- **Test**: a test map relit, then its .bsp replaced by another map: the load uses the original, moves the copy aside,
  prints the line; unchanged map: copy used; old-format `.relight`: moved aside on load, relit by a batch; batch on a
  changed map: relit, not skipped; e1m1 (a .pak map from relit/'s copy): relit, then loaded as current.
## Evening notes of 10-07: reloading after a map change, quiet fists, strict catches (2026-10-08)

- **Immersive reloading lost after a teleporter** (e1m2_2026-10-07_22-43-20). The client clears its stats on
  `svc_serverinfo` (CL_ClearState), but the server only sends a stat when it differs from what it sent last
  (`client->oldstats_*`), and kept those across a changelevel: `STAT_QVR_RELOADMODE` stayed 3, was never resent, and
  read 0 on the new map (no pouch, no magazines; the cvar still said Immersive; changing the mode resent it). A loaded
  game reconnects, so it was fine. `SV_SendServerinfo` now forgets what it sent. Any stat equal on both maps had it.
  Test: `Misc/quakevr/stats_levelchange_test.sh` (the pouch on e1m1, after `changelevel e1m2`, after a save and load).
- **A closed fist woke monsters** (e1m2_2026-10-07_22-44-33). The trigger on a fist (and on a melee weapon the tracked
  hands swing) ran `W_AttackImpl`, which sets `show_hostile` ("wake monsters up") before finding it has nothing to
  fire. Now skipped for those; a real swing or punch sets it where it whooshes (`VR_Melee_Whoosh`) or lands
  (`VR_Melee_Landed`). Test: `Misc/quakevr/fist_alert_test.sh` (show_hostile read from saves: the fist closed and held
  none; a mock punch and a shotgun shot set it).
- **Weapons caught from 20 cm off** (hip1m1_2026-10-07_22-33-15, vrfiringrange_2026-10-07_22-49-20). The server took a
  thrown weapon when the fist touched its drawn shape within `vr_weapon_grab_slack` (5 cm, meant for a gun lying flat),
  or else by the hand's 5-unit box at its point (the front top of the fist) against the box round the handle with the
  easy-touch bonus: a crowbar floating in the air was taken with the fist 7-9 cm off it, from 15 cm over it. Now only by
  the fist on it (`vr_carry_grab_bias`, as props), the slack only for one lying on something (`lyingWeapon`: resting
  with something under it within 8 units, or stuck); `vr_weapon_grab_box 1` (Hands > Lenient Weapon Catch) brings the
  box back. The view's anywhere tests (a weapon lying about; the other hand's weapon) measure the fist's gap to the
  drawn shape (`fistSurfaceGap`) instead of the hand's point within 6 cm, which was the open fingertips' reach. A force
  grab's catch doesn't go through the touch test (unchanged). Test: `Misc/quakevr/weapon_catch_test.sh`: in the air
  (sv_gravity 0) the fist lowered 1-2 cm a step: not taken at 4.71, 3.02, 1.38, 0.30 cm, taken at -0.11; the old catch
  takes it from 12 cm over; lying on the floor taken at 3.69 cm (slack 5), as before.
- **Spinning pickups' magazines off the gun** (e1m1_2026-10-07_22-39-10). A map's weapon drawn as its prop spins about
  its middle by its `model_offset` (VR_WeaponPickup_Centre); a lying gun's magazine (and well, and an open super
  shotgun's parts) were placed and drawn without the gun's networked transform: 7.1 units off on the nailgun, 9.7 on the
  super nailgun. `ViewEntity::netEntity` draws a part with its world entity's scale and offset. `vr_reload_debug 1`
  prints each lying gun's magazine seat against the gun's once a second. Test: `Misc/quakevr/pickup_mag_test.sh` (0.00
  units for both nailguns' pickups and dropped ones).
- **Slider steps off round values** (vrfiringrange_2026-10-07_22-23-40). Every step snapped to the fine steps' grid
  (step x `vr_menu_fine_step`); his fine step had drifted to 0.0999, so plain steps landed on 0.6998, 0.7997. Plain
  steps snap to the slider's own step again (as before the fine steps), fine ones to the fine grid. Config version 101
  moves `vr_menu_fine_step` 0.0999 to 0.1. Test aid `vr_menu_slider_step <cvar> <steps> [fine]`; test
  `Misc/quakevr/slider_step_test.sh`.
- Flashlight Side: Left / Right (it hangs on the torso). VR Settings: an Ammo Pouch section after Holster Calibration
  (Forward, Right, Up, Size, Ammo Counter over `vr_ammo_pouch_x/y/z/scale/counter`). Detail textures off by default
  (`vr_detail` 0, config version 101; the presets Medium and up set it to its default).
## Reloading: loose rounds load by contact (2026-10-08)

The author's notes vrfiringrange_2026-10-07_22-15-30, 22-17-08, 22-17-26 and hip1m1_2026-10-07_22-33-03: shells go into
the shotgun's port and the open super shotgun's barrels, and magazines into the nailgun's, super nailgun's and
thunderbolt's wells, by contact as loose props too (dropped from above, thrown, or the gun brought down onto one lying on
a table), lying the right way; first find why the rounds collide with the gun before they reach it.

- **The collision shapes, measured** (`vr_physics_shapes [classname...]`, new; Debug > Tests > Reloading, "Print the
  Collision Shapes"). The rounds' Box3D bodies are their drawn size: a shell 2.30 x 0.68 x 0.68 units (7.3 x 2.2 x 2.2
  cm, its Size 1.25), the taped pair 2.30 x 1.33 x 0.68, the magazines 2.07 x 1.11 x 3.65 (nailgun), 2.23 x 1.09 x 4.25
  (super nailgun), 1.66 x 1.44 x 3.19 (cell). Each held gun's reach body (its Box3D hull) is its drawn model's size to
  0.04 units. Not too big, then; the cause is the hull being convex: it fills the concave port and wells, and the load
  point lies inside it: the shotgun's 1.24 units (3.9 cm), the super shotgun's 0.56 (1.8 cm), the nailgun's 0.89 (2.8
  cm), the super nailgun's 0.16 (0.5 cm), the thunderbolt's 1.91 (6.0 cm). A shell resting on the shotgun under its
  port has its middle 1.6 units from the load point (radius 1.5); a cell can't reach the thunderbolt's at all. What
  was too big: the magazines' Quake boxes (their model's bounds, not sized by their Size 0.45 to 0.55: twice the drawn
  magazine, e.g. 3.76 x 2.02 x 6.64 for the nailgun's), now sized as drawn as the shells' already were
  (`VR_Reload_SetModel`).
- **Loading by contact** (QC `VR_Reload_LooseFrame`, each hand each frame). A loose round (not in a hand, not flying to
  one) goes into the hand's gun when it fits (`VR_Reload_LooseFits`: its ammo; a magazine its gun's and none in; a
  shell: the gun not full, the super shotgun open), lies the way it goes in (within `vr_reload_contact_angle`, 35
  degrees: a shell's open end along the tube or the open barrels, a magazine's feed end up the well; sideways never),
  is on the side the opening faces, and comes within the load point's radius (a magazine's radius added) plus
  `vr_reload_contact_leniency` (6 cm) of it, along its way that frame (a falling round moves a few units a frame).
  It goes in as a held one does (`VR_Reload_LoadInto`, `VR_Reload_SeatMagInto`: the same sounds, the gun's haptic,
  the shells' slide-in drawn; "a loose round by contact" in the log). Within that leniency more again it passes through
  the gun's hull (`.vr_ammo_pass`, `.vr_ammo_passer`: the engine's `reachSkips` skips its contacts with that hand's
  gun), so the hull covering the port doesn't knock it away. One just out of a gun (a magazine dropped by B/Y or knocked
  out, the super shotgun's live shells thrown out: `.vr_ammo_fresh`) goes in again only once it has left. Weapons >
  Reloading: "Load Loose Rounds" (`vr_reload_contact` 1), "Loose Leniency", "Loose Angle".
- **The engine sends each gun's way in** (`VrMove::loadPortAxis`, `loadPortFace` -> `.loadportaxis`, `.loadportface`,
  and the off hand's): the tube's or the barrels' forward (turned down with them open) or the seated magazine's feed end
  (its mount's middle to its seat), and the opening's outward way (the shotgun's port down, the breech and the wells
  back along the axis), from the drawn gun (vr_view.cpp, beside the load point).
- **The gun brought down onto one**: the shotgun's stock and grip are lower than its port, so tilted up (muzzle up 9
  degrees) the gun meets the floor with its port 3.8 units above a shell lying there; receiver lowest (muzzle a little
  down) 2.2, within the 3.07 the leniency gives. Magazines stand upright: the nailgun and the thunderbolt level, the
  super nailgun rolled onto its side (its well on its left).
- Tests: `Misc/quakevr/reload/contact_test.sh` (22 checks, all pass): the shapes; per gun a round tossed into its
  opening (in), one lying sideways at it (out), one dropped from above into it turned up (in), the gun brought down onto
  one on the floor (in); the super shotgun shut (out) and open (a pair tossed and one dropped, 2 each); Load Loose Rounds
  off (out). Test steps `vr_reload_test` 10 to 15 (`impulse 125`): toss, sideways, on the floor, from above, the loose
  rounds' report, the super shotgun broken open (Debug > Tests > Reloading).
## Props batch: the firing range notes of 10-07 night (2026-10-08)

The author's notes vrfiringrange_2026-10-07_22-55-53 .. 23-08-07, 23-51-46 and start_2026-10-07_23-42-39.

### The rocket ogre drops its chainsaw
Dawn of the Machine's rocket ogre (owned/mg3/progs/ogre_rocket.mdl) holds a chainsaw in his right hand (a piece of its
own, 94 vertices: 0..47, 649..652, 940..981; the launcher is the tube on his left forearm, which the old comments took for
a chainsaw-less launcher). vr_monstermods.cpp hides it in his death frames 112..135 as the ogre's, so his ragdoll makes it
a loose bone, hidden; his ragdoll's right hand bone moved onto his fist (rig.py had put it on the chainsaw: with the saw
gone it got no vertices and the ragdoll was refused). ogre_die drops a chainsaw for him too (the drop cap as any).
Checked: `vr_mg3_mtest 5` (7 passed, "its chainsaw dropped (1)"); the log's "the chainsaw (94 vertices) hidden in 24
death frames" and "rigged: 14 bones (2 loose)".

### Heavy props thrown hurt by their mass
A barrel thrown with both hands hurt for about 14: heavy things leave the hand slow (3 m/s), and weightdamage's curve
flattens. VR_Thrown_HeavyMult: props over vr_throw_heavy_from (8 kg) deal (mass / 8)^vr_throw_heavy_exp (1), at most
vr_throw_heavy_max (5) times more; never weapons (their Weapon Weights). Misc/quakevr/prop_2h_test.sh (mock hands, a
barrel or crate in both hands, at an ogre 80 units ahead, the push at 6 m/s): the barrel 12.4 -> 46.7, a small crate
10.7 -> 53.6. A two-handed blow with it stays about 90.

### Barrels about the maps
The crate planner (vr_crates.cpp) places barrels too: vr_crates_barrels (0.3) of the spots (rolled once a spot, its own
random numbers: the crates' draws are unchanged) get an upright barrel, or, standing alone, one lying on a facet of its
belly along the wall (vr_crates_barrel_lying 0.3; rolled a 24th of a turn so a facet is down). Stacks mix them: an
upright barrel on a crate or a barrel, a small crate on a barrel (its middle within 0.15 of the barrel's radius). No
crowbar on a barrel, nor on a crate standing on one (one such crate tipped 4 degrees and slid 2.7 units). Tight maps get
more barrels than the share: their smaller footprint fits where a crate fails. vr_crates_list now says how far each lies
from its planned place. Settled (all within 2 units after 15 s): e1m1, e1m3, e2m2, e3m1 at the defaults; e1m1, e1m2,
e2m1, e3m2, e4m1 with half barrels and every one stacked (26 mixed stacks).

### Wrist flicks in bullet time
A flick that threw 0.9 m at full speed threw 7.9 m in bullet time. Two causes: the controller's own estimate is in the
game's time (1/scale as fast), capped only by the slowed hand's 8 m/s, which a flick stays under; and motionRate took a
flick (little linear speed) for a throw made slowly with the world, its windows three times too long (its spin averaged
down). vr_throw_slowmo_flick_spin (40 rad/s of the game's time): a controller turned faster was a real-speed throw;
vr_throw_slowmo_flick (1): the flick's share of the throw (the estimate's flick over its speed) is kept at the time scale
(the player's real time) for a real-speed throw, the arm's share as before. Misc/quakevr/throw_slowmo/flick_slowmo_test.sh
(three flicks about the wrist, the overhand without and with its wrist; metres at 45 degrees):

| throw      | full speed | bullet time before | after | made slowly with the world |
|------------|-----------:|-------------------:|------:|---------------------------:|
| flick_fast |       2.06 |               9.57 |  3.22 |                       1.75 |
| flick      |       0.92 |               7.86 |  1.22 |                       0.92 |
| flick_soft |       0.28 |               2.48 |  2.06 |                       0.28 |
| overhand0  |       4.03 |               6.51 |  5.81 |                       4.05 |
| overhand   |       2.54 |               6.51 |  2.82 |                       2.43 |

The slow-tempo column is unchanged by the fix. A soft flick (12 rad/s real) stays under the spin threshold and is still
taken as slow (35 fixed it partly but moved the slow overhand by 11%). The overhand with its wrist snap loses its flick's
amplification (its model's flick is 91% of it); the arm alone (overhand0) is within 6% of before.

### Overhead slams
A crate or barrel held in both hands over the head (the hands' middle vr_carry_slam_height 10 cm over the eyes), then
within vr_carry_slam_window (1 s) swung down at vr_carry_slam_speed (3 m/s) or more, mostly downwards, onto anything
(its box swept a frame and 30 ms ahead: a monster, a prop, a wall, the floor) breaks at once; what it meets takes
vr_carry_slam_damage (2) times a punch with it in hand, times its speed over the least (at most twice). Let go of from
over the head, its first hard landing (vr_crate_slam_throw_speed 3 m/s, within 3 s) takes vr_crate_slam_throw_damage
(0.55) of its full health: two such throws break it. prop_2h_test.sh: slams onto the floor (barrel 5.4 m/s, crate 8.4),
an ogre (101.8 damage) and a crate (both broke) break; the same swing from the chest doesn't; thrown down twice, broken at
the second (41.2 each of 75). vr_carry_slam 0: off.

### Thrown props keep the player's motion
A thrown weapon takes the thrower's velocity (MakeThrown); props didn't. VR_Carry_Release adds vr_carry_throw_inherit (1)
of it (a missile or not still judged on the hand's speed). A gib let go of while running: 320 u/s ahead running forward,
320 back running backward.

### Wood in lava burns
VR_Burn_LavaFrame (every 0.2 s): a crate, a barrel or an uncharred piece whose bottom is in lava is lit there
(VR_BURN_LAVA), and burns on in it (VR_Burn_Think puts fires out in water and slime only, for wood). vr_burn_lava 1. A
crate and a barrel dropped into e1m7's lava: lit at once, burnt through at 12 s, charred pieces.

## Swing Through Enemies on by default (2026-10-08)

The author's note (vrfiringrange 23-52-45): "Make the new melee settings the defaults. I enabled Swing Through Enemies to
make melee feel more responsive." His config against the effective defaults (`vr_cvars.inc` plus `vr_defaults.cfg`),
every melee and combat setting (`vr_melee_*`, `vr_bash*`, `vr_shove*`, `vr_parry*`, `vr_headbutt*`, `vr_decap*`,
`vr_knockdown*`, `vr_stamina*`, `vr_gore*`...): only the three phasing settings differ, all promoted (config version
102, each moved only where a config still holds the old default):

| Setting | Was | Now (his) |
|---|---|---|
| `vr_melee_phase` | 0 | 1 |
| `vr_melee_phase_speed` | 2.25 | 4 (his 3.996: slider noise) |
| `vr_melee_phase_time` | 0.15 | 0.35 (his 0.34965) |

## Slaps (2026-10-08)

The author's note (vrfiringrange 23-52-45): a blow needed a closed hand; a fully open hand swung registered nothing, which
felt weird. Wanted: a slap, weaker than a punch, never popping a head "or anything like that", with its own sound, the
damage tweakable, and the open-hand shoves (one hand, two hands) always shoves.

- **What is a slap** (QC `vr_melee.qc` `VR_Melee_Slaps`): the punch's sweep and tests (the fist's point, `VR_Melee_Decide`:
  as fast as a punch, `VR_MELEE_RUN`, the wrist moved, not wiggled), for an open hand (`VR_Melee_OpenHand`: empty, the
  grip not held, no flashlight, not steadying a weapon, not climbing) that is not in a shove's push (`mh_pushing`) and
  goes across: its velocity within 60 degrees of sideways (`VR_SLAP_ACROSS` 0.5: not a pat down or up) and not out from
  its shoulder (`VR_SLAP_RADIAL` 0.7; a shove's palm extends the arm at 0.82 and more, `VR_BASH_PALM_RADIAL`; between
  the two: neither). A palm pushed out ahead is a shove however fast, and an open hand thrust forward palm sideways is
  nothing (as before). The arm's line from the shoulder is `VR_Melee_ArmOut`, shared with `VR_Bash_Extends`.
- **What it does**: kind `VR_MKIND_SLAP` ("slap", its own motion category and `melee/slap` event); damage a punch's times
  `vr_melee_slap_mult` (0.5); only on what takes damage (`VR_Melee_SlapLands`: not walls, not loose gibs or heads). No
  head pop or beheading (`VR_Decap_HeadBlow`), no limb off (`VR_Limb_Blow`), no small gibs (`VR_SmallGib_*`), no blood
  splash, and no overkill: a slap that kills leaves the monster at health -1 (`T_DamageImpl`), so it never gibs. Monsters
  are woken only when it lands (an open hand never whooshes). `vr_melee_slap 0`: an open hand never strikes (as before).
- **Sound**: `vr/slap1..3.wav`, synthesized (`make_sounds.py slap`: a bright crack, the palm then the fingers 4.5 ms
  later, over a light smack of flesh and a small thump; three a little apart in pitch, never the same twice in a row).
- **Settings**: Melee Settings > Slaps (`vr_melee_slap` 1), Slap Damage Mult. (`vr_melee_slap_mult` 0.5).
- **Motion takes**: the recorder's "Expected Slap" category (forehand, backhand; `expect.cfg`: `melee/slap`), and
  `motion_synth.py slap_forehand | slap_backhand | no_hit_slow_slap`.

Checked (headless, the dummy 1.15 m ahead): `slap_forehand` and `slap_backhand` slap (15.4 at x1.03, 9.9 m/s; with
`vr_melee_slap_mult 1` 30.8, a punch at 10 m/s 30.2), `no_hit_slow_slap` (0.9 s) and `no_hit_wave` nothing (no stroke,
no push: nothing woken), `palm_shove_1h`/`_2h` shoves only, `punch_straight` a punch; an open hand thrust forward at 8
m/s nothing; `vr_melee_slap 0` nothing. A grunt slapped at `vr_melee_dmg_multiplier 20`: dead, "decap: no: a slap", no
gibs; punched the same: a pop roll and gibbed. (The synthetic takes' eval verdicts read FAIL for the hits that do
happen: their events name the dummy's enemy, `monster_army`, the verdict wants `vr_dummy`; not from this change.)
## Teleporters: exits on their gates, props through, held objects, the force grab's beam (2026-10-08)

The author's vrteleporters notes (23-10-11 .. 23-15-34) and start's 23-44-08. Headless checks of each:
`Misc/quakevr/teleporters/teleporter_edges_test.sh <agent> [clip|push|held|cross|grab|cull|particles|all]` (one line
each, with what it must say; about two minutes for all). Debug aid: `vr_portals_debug_split` (Debug > Teleporters, Print
Gate Cuts): each frame the entities drawn cut by a gate (its plane, how far through, its middle in the room it is in),
-1 also the main hand's held object every frame, N entity N; the force grab's beam end; each thrown or rigid thing's
middle and why a gate did not take it.

- **Out a step past the gate (23-13-44): the code, not the map.** The seamless mapping took everything to the
  destination marker, which vrteleporters stands 48 units out from the paired gate (56 framed) so that Quake's teleport
  puts monsters clear of that gate's trigger. So the player popped out 48 units past the gate he seemed to walk out of,
  the view through showed the room from there, and the exit plane stood in the open room. `pairExits` (build): a side
  whose destination stands in front of another gate of its size (its aperture's width and height within 4 units,
  facing the way one walks out, within 128 units, its middle within 64 of where the aperture lands: sills included)
  comes out of that gate's face (`vr_teleporter_pair_exits` 1, Graphics > Teleporters, Exits On Paired Gates). All 28
  vrteleporters sides pair; the flush player gate carries 640 -> 928 (was 641 -> 977). Monsters still teleport to the
  marker (Quake's), so a monster comes out a step past the gate.
- **Props cut far from a gate (23-10-11) and a held prop cut on the way (23-11-57)** were that exit plane in the open
  room: a crate resting on it was cut and its back half drawn again at the entrance. Paired, it is the gate's face. An
  exit with no gate of its own (an id map's one-way teleporter) now splits only what moves out of it (VR_PortalAlias:
  its last two messages' places; splitBounds: the body's velocity).
- **Pushing a crate through (23-10-57; the 8-deep gates' known issue).** The portal copies' clip planes reached Box3D
  only through its pre-solve callback, which Box3D asks for convex contacts alone, once a contact; the level is one mesh
  shape, so a prop's contacts with the wall behind a gate were never clipped. A marked local change to Box3D
  (`src/mesh_contact.c`, its README): mesh contacts ask the callback point by point; `preSolve` answers the level's
  mesh by the copies' planes only. VR_PortalToss: the destination test turns the box with the gate and insets it 4
  units (a crate on the floor had its Quake box under the floor: "all solid", never carried, and slid on into the
  wall), and a prop already past the plane, still straddling it and moving in, is carried. A small crate slid at 60
  u/s into the 8-deep player gate comes out at y 994 (was stopped at 627); the big crate through the player, large and
  wide gates; teleporters_test.sh throw's 46 u/s crate now crosses the 8-deep gate.
- **A held prop's hitch (23-11-57).** What the hands hold is drawn where the tracked hands are, but it was drawn
  through a gate only while straddling it: wholly past the plane it was drawn behind the gate's surface (unseen) until
  the player crossed. Held objects (`held::drawnInHands`) now reach through as the hands and guns do, from the hands'
  own body (`hands::State::playerOrigin`: the view entity's lerped origin is already carried on the crossing frame,
  one more frame behind the surface). Walking into the player gate holding a shells box: its middle's largest step a
  frame 0.8 units, none drawn inside the wall.
- **The force grab's beam (23-15-34)** went to the box's place in the room beyond; it now goes to its image behind
  the gate's surface (`portals::pullImageSeen`, the server's own pullImage), into the gate to the box seen there. A
  box force-grabbed through the large gate crosses and is caught.
- **Torch fire seen in a gate (start 23-44-08).** With `vr_teleporter_surface_opacity` under 1 (his 0.3) the gate's
  surface is drawn in the translucent pass and writes no depth: particles behind it were drawn over the view through
  it. The particles' fragment shader drops what lies behind a gate shown in this view, seen through its aperture
  (VR_PortalFrameData). Quake's own particles (`vr_particles 0`) and translucent sprites are not hidden so (not done).
- **Found in review.** A brush prop (ammo, health box) half through a gate seen from the exit with the entrance out
  of view lost its half coming out (R_SortEntities culled it by its own place: R_BModelPortalSplit keeps it).
  CL_RelinkEntities lerped an entity between two gates under 100 units apart straight through the wall between them
  (VR_PortalLerpFrom now also over 24 units a tick, when the carried place is twice as near). Shots through a gate
  started at the marker 48 units out (an enemy right in front of the exit was skipped) and monsters' sight through a
  gate saw the player 48 units off: both use the paired mapping now.
- **Not done:** monsters through gates (Quake's teleport, to the marker), ragdolls and corpses through gates (no
  portal copies: a corpse falling into a gate meets the wall behind), recursive gate views and the head seen through a
  gate (note 23-12-32).
- **To try in VR:** walk into vrteleporters' flush and framed gates slowly and stop half through (you are in both
  rooms, no gap at the far gate); push a crate on the floor through a gate with the hands or the body (Debug >
  Teleporters, Slide A Crate Through, shows it); carry a box through the loop room's gates (never cut before it reaches
  the gate, no hitch); force grab a box lying in the room beyond a gate (the beam goes into the gate); start's
  underwater gate by the episode 4 teleporter with torch fire behind it.

## A stale vrstart.ent broke the island: .ent files pinned to their .bsp, plain ones checked (2026-10-08)

A player building the latest commit got `Host_Error: Mod_LoadModel: *28 not found` loading `vrstart`. The island's
`vrstart.bsp` has 28 brush models (*0..*27); the repo tracked the old hub's `maps/vrstart.ent` (its entities, up to
*36) until a120d0b3 moved it to `vrstart_old.ent`. Ironwail loads `maps/<map>@<crc>.ent` (the CRC of the .bsp's entity
lump), else a plain `maps/<map>.ent` for any .bsp of that name, so a leftover `vrstart.ent` (an old copy of quakevr
copied over, a zip, an untracked file) put the old hub's entities on the island.

- **Pinned:** our shipped `.ent` files are named for their .bsp: `vrstart_old@3e00.ent`, `vrfiringrange@3647.ent`
  (`git mv`, contents unchanged). `Misc/quakevr/entfile.py` (Quake's CRC_Block in Python) prints each map's pinned name
  (`python Misc/quakevr/entfile.py`), finds a map's `.ent` and writes it under the .bsp's current CRC (renaming the old
  one). `relight_quakevr_maps.py` (which rewrites the .bsp's entity lump, so its CRC) and `make_prop_area.py` use it.
  The package takes git's files, so it needs no change.
- **Engine guard** (gl_model.c `Mod_LoadEntities`, a Quake VR addition): a plain `<map>.ent` whose `"model" "*N"` is
  past the .bsp's submodel count is ignored, the map's own entities used, with one warning: `maps/vrstart.ent doesn't
  match this map (references *36, the map has 28): ignored; delete or update it`. A plain .ent that fits still applies.
- **Installer:** an update already removes the files the previous version's manifest listed and this one doesn't
  (unless the player changed them; InstallEngine.cs), so an installer-made install drops the old `vrstart.ent` itself.
  Folders copied by hand or from a zip keep it: the guard covers those.

Tested headless: the old hub's entities as a plain `quakevr/maps/vrstart.ent`: the warning, the island loads with 149
edicts (as without the file), its path walked (vrstart_walktest.py: 18 of 18 legs) and a pavilion button pressed by
hand (`buttons/stray_press_test.sh`: 30 loads, 0 stray presses, 1 real press); `vrstart_old` loads
`vrstart_old@3e00.ent` (83 edicts; 80 from its own lump with `external_ents 0`); `vrfiringrange` loads
`vrfiringrange@3647.ent` (268; 186 without); a valid plain `e1m1.ent` (one more item_health, a new message) applies
(218 edicts, 217 without); `make_prop_area.py --ent-only` rewrites the pinned file unchanged.


## Death hides the gear; vrstart's lecterns, settings wall and crates (2026-10-08)

The author's notes vrfiringrange_2026-10-08_00-02-05 and 00-02-55, vrstart_2026-10-07_23-32-40, 23-34-05, 23-34-22 and
23-36-45.

- **Dead, the gear hidden** (`vr_dead_hide_gear`, on; HUD and Menus > Screens > Hide Gear When Dead): while your health
  is 0 or less (in game, not in an intermission: `body::gearHiddenForDeath`) no holstered guns nor holster sleeves
  (`setupHolsters`), no ammo pouch (`setupAmmoPouch`), no wrist gadget (`gadget::active()` false); the HUD is then
  Quake's status bar on a hand, as with HUD: Status Bar (`vr_panel.cpp` `handSbar`). All back when you respawn (single
  player: `restart`) or load a save. Not hidden: the belt's flashlight, the grenade pouch on the back. Tests:
  `vr_gear_status` (Debug > Reports > Gear: what was drawn, and whether the status bar is on a hand) and `impulse 195`
  (Debug > Cheats and Recording > Die Now: armour, god mode and the Pentagram set aside, not gibbed). Alive 2 guns, 4
  sleeves, pouch 1, gadget 1, status bar 0; dead all 0 and status bar 1; after `restart` and after `load` as alive;
  dead with the setting off as alive.
  Follow-up (the author: the belt flashlight and the back grenade pouch hide too): the grenade pouch (`setupPouch`) now
  hides with the rest. The flashlight already did, whatever the setting (`flashlight::setupView`: not alive, no model,
  no cord, no lens nor beam, its lights killed), so the "Not hidden" above was wrong. Worn and checked: the holstered
  guns' parts (magazines, wells, open super shotguns, buttons: they follow the gun), the ammo pouch's grenades (follow
  the pouch), the gadget's straps (follow the gadget), the body and its pauldrons (hidden dead as ever). `vr_gear_status`
  prints them all; `Misc/quakevr/dead_gear_test.sh` (9 checks: alive with the torch in a hand, dead, respawned, dead
  again, loaded, the setting off).
- **The main menu**: a gap above Quit, as between its other groups (`M_Main_GroupStart`; `M_Main_Layout` counts the
  gaps).
- **Dimension of the Past's lectern** started the campaign at once because the campaign has its own game folder and
  the teleporter's changelevel can't rebuild the folders (`VR_CanChangeCampaignMap` refused it). Now it selects
  (`vr_activestartpaknameidx 3`, SELECTED over it) and the teleporter, at a hub, runs `vr_campaign_select dopa` (at most
  once in 2 s while the gate is touched); a 3 left over on any other map is ignored ("start" is the running campaign's).
  Test: on vrstart `vr_activestartpaknameidx 3` leaves the map vrstart; `changelevel start` then loads e5start
  (vr_campaign 3, the hub selector back to 0); on e1m1 with 3, `changelevel start` loads Quake's start.
- **The settings pavilion**: a tenth button on the south wall, SWIMMING (`vr_setup_option swim`: vr_swim Immersive /
  Vanilla). TORCH SIDE already read Left / Right (30509383).
- **Crates**: the range's two stacks put their top crate into the two under it (`crates::putPlaced` traced the level
  only) and lane 3's two were 32 apart (they touch turned). `putPlaced` now rests a crate on one placed before it whose
  middle is under its origin; the stacks' lower pair are 44 apart. `vr_crates_list` ends with the pairs whose turned
  boxes overlap (vrstart: 5 pairs before, 0 after).
- Test note: a wall button's command (QC `localcmd`) runs after every command already queued, so a `-Script` can't see
  its effect before it ends: press it to see `vr_debug_wallbuttons` name it, and run its command to test the command.

## Support files hosted once: assets-2026-10-08 (2026-10-08)

The HD texture pack and ericw-tools' zips now live on their own GitHub release, `assets-2026-10-08` (not latest), and
releases link them instead of re-uploading them. `Misc/release/support_assets.json` lists its tag, URL template and
each file's size and SHA-256 (checked against GitHub's API `digest`s on 2026-10-08).

- **make_release.ps1 / make_release.py**: latest.json's `hdtextures` is the hosted pack by default (its URL first, then
  any non-GitHub, non-local `-UrlBase` with the support tag; nothing copied). The release notes link it and "Source of
  ericw-tools' light.exe: <hosted src zip>"; `-Publish` no longer needs `-EricwSource` (nor `QVR_ERICW_SRC`, dropped as
  a default). Before a real build it reads the support release from the API (size, digest) and HEADs each URL; a
  mismatch is a PROBLEM for `-Publish`, a warning otherwise; skipped with `-DryRun`/`-Local`. Overrides: `-Textures`,
  `-EricwSource` (upload a copy), `-NoTextures`. `-Local` keeps the GitHub URL for hdtextures and says so (ticking HD
  textures in a local test downloads the real pack). `qvr-setup feed --file ... --assets ... --hosted hdtextures`
  checks the rest of the folder and reports the hosted one.
- **Installer**: no hard-coded texture or ericw URLs existed (textures come from latest.json with its size/SHA-256;
  ericw-tools ships in the package; VisPatch stays on SourceForge), so nothing changed beyond the CLI's `--hosted`.
- **In-game Download ericw-tools** (`vr_relight_tool.cpp`): mirrors in order, the support release first, then
  ericw-tools' own; a mirror that fails (HTTP error, or a file that is not the pinned one) passes to the next, and the
  console names each failure. `vr_relight_tool_url` takes several URLs separated by spaces. Verified with a local
  server: 404, then a truncated zip (hash mismatch), then the real zip installed.

## Reloading: the launchers loaded at the muzzle (2026-10-08)

The author's note vrfiringrange_2026-10-07_22-18-57: the rocket, grenade and proximity launchers front loaded: a rocket,
grenade or proximity grenade from the ammo pouch (the special ammo too, in the launcher's other ammo mode) put towards
the front of the muzzle, sliding in as the shotgun shells do; thrown there, or lying on a table the right way and the
launcher brought to it, too. Only the orientation precise: rockets and grenades butt first, a proximity grenade any way.
This does RELOAD.md's phases 3 and 4 (at the muzzle, not a port under the launchers or the rocket launcher's back
end as planned).

- **The rule** (QC vr_reload.qc, "Front-loaded launchers"; `vr_reload_front` 1, Immersive only: Simple and the others
  unchanged). The three launchers load by hand (`VR_Reload_ManualFor`: no holster or button reload for them), a round at
  a time up to their magazine (4, as before: a conservative choice, the author may want 1 for the rocket launcher). The
  pouch gives the round of the launcher in the other hand by its ammo mode (`WeaponIdToAmmoId`): a grenade or a
  multi-grenade, a proximity grenade, a rocket or a multi-rocket (the multi-rockets need Dissolution of Eternity's pack,
  as the launcher's mode does), its ammo out of the reserve at once. Changing the ammo mode unloads the magazine into the
  reserve, as before. A round is its own entity class (`vr_ammo_front`, `.vr_ammo_front` the launcher it fits): it goes
  only into its launcher (a rocket brought to the grenade launcher stays out), and a launcher takes only its rounds.
- **Butt first.** A round's reference point is its butt (`VR_Reload_RoundRef`: its model's back end along its +x; a
  proximity grenade's middle); within the muzzle's radius of the load point it goes in when it lies within
  `vr_reload_front_angle` (35) degrees of the barrel, its nose out of the muzzle. Held the wrong way round: a dull tap
  and the blocked sound, once until it leaves the muzzle ("the wrong way round" in the log). Loose ones as the shells by
  contact (`VR_Reload_LooseFrame`): the looser of the two angles, Load Loose Rounds and its leniency.
- **The muzzles** (the engine's `LoadPort` table, vr_view.cpp: `front` ports): the middle of the barrel's mouth, measured
  on the models (the grenade launcher's at x 31.4, z 5.9 of v_rock.mdl, the multi one's alike, the proximity launcher's
  the same; the rocket launcher's tube at x 56.1, z 4.55 of v_rock2.mdl, the multi one's alike), the way in back along
  the barrel, the opening facing forward; `vr_reload_port_gl/prox/rl_x/y/z` move them, `_radius` (2 units) their reach.
  `view::loadPath` slides a round's middle to the muzzle, then 4.5 and 9 model units into the grenade launchers (3.4 world
  units in all), 8 into the proximity launcher, 8 and 16 into the rocket launcher's tube.
- **The rounds** (make_rounds.py, new: `vr_round_rocket.mdl` 30 cm, an olive body, a yellow band, a red nose, a nozzle and
  four fins at the butt, skin 1 the multi-rocket's dark body and red band; `vr_round_grenade.mdl` 12 by 9 cm, a brass case
  with a rim and a primer at the butt, an olive body, a rounded nose, skin 1 the multi-grenade's red; `vr_round_prox.mdl` a
  dark ball with six spikes and a red lens band, 8 cm; normal maps baked). Their own models (the `LiveRound` trait: a
  round's grab slack, hard and metal in Box3D, Held Things Collide's leniency), so missile.mdl, grenade.mdl and
  proxbomb.mdl keep their sizes in flight. Prop slots 54-56 (vr_props_version 66): the rocket Along the Handle by the
  nose half of its middle (its butt below the little finger, to go into the muzzle), the grenades in the palm.
- **Lying about** a launcher's round is a grenade shots set off (`VR_GrenShot_Make`; `.vr_ammo_boom`, in vr_fields.qc:
  `VR_GrenShot_SetOff` runs it, as a Quake grenade's think): a shot, a strong blow or a blast sets it off with the
  launcher grenade's blast (`VR_Reload_RoundBoom`: let go of, no longer a round, `GrenadeExplode`), the doing of who set it
  off; in a hand shots don't meet it; dropped or thrown it doesn't go off. Weaker blows bat it.
- **The pouch** showed no rounds for them then (since: "One grenade from either pouch" below) (its frames are the shells' and magazines': kinds 5 rockets, 6 grenades, 7
  proximity grenades draw its empty frame; its counter counts them). Left for the pouch's own work.
- Weapons > Reloading > **Launchers** (new page): Load at the Muzzle, Round Angle, Round Pose (Held Object Offsets), Show
  Load Points, each launcher's Muzzle X/Y/Z/Radius. Debug > Tests > Reloading: Rocket Launcher in the Off Hand, Toss a
  Round in the Wrong Way Round (`vr_reload_test 16`), Hold the Round at the Load Point (17) and the Wrong Way Round (18);
  step 19 shoots each launcher's round lying about.
- Tests: `Misc/quakevr/reload/front_test.sh` (32 checks, all pass), per launcher: the pouch to the muzzle by the mock hands (the reserve
  one less, in, drawn sliding in: collect fx into hotspot 240), the hand upside down (refused; the proximity grenade in),
  test step 18, tossed butt first (in), nose first (out; the ball in), across the muzzle (out), dropped from above into
  it turned up (in), lying on the floor and the launcher brought to it (drawn back, down, pushed forward along it: in;
  straight down, the barrel lands on the butt); the other ammo mode (a multi-grenade, a multi-rocket from the
  multi-rockets, loaded); a rocket lying about shot (test step 19: it goes off); a rocket at the grenade launcher's muzzle (out); Simple mode and Load at the Muzzle off (no
  rocket from the pouch, the hip holster reloads: clip 4).

## Reloading and spent guns: the night notes of 10-07/08 (2026-10-08)

The author's notes grenedin_2026-10-08_00-05-36, 00-07-22, vrfiringrange_2026-10-07_23-54-46, 23-58-30,
vrfiringrange_2026-10-08_00-00-42, 00-01-10, and his typed note on the pouch's shells. Tests: `reload_test.sh` section 11.

- **The super nailgun's well flush on its face** (23-58-30: "slightly off ... too big ... popping out"). The well stood
  1.2 units off the face (its collar ran 1.1 units out along the magazine) and reached 0.53 past the face's lower edge:
  the seat (7.2, 5.44, 1.2) was 0.6 units below the middle of the flat band it sits on (v_nail2.mdl frame 0: the body's
  upper left face is flat only in its lower band, from (y 5.74, z 0.28) up to (4.60, 3.24), 3.18 units across; above it
  the face is ribbed). make_mags.py now seats it in the band's middle (7.2, 5.17, 1.76), square to the band's own normal
  (0, 0.933, 0.359), with a thinner collar (wall 0.24) sunk into the body: only its mouth's rim stands 0.12 off the face,
  0.16 inside both edges (`ssg_checks.py snailwell`: proud 0.15, over -0.16; before: 1.19, 0.53). The load point and the
  magazine's middle (the two-handed grip, the hit box) moved with it (vr_view.cpp `loadPorts`, `magMounts`: middle
  (7.2, 9.56, 3.45)). The lava gun's the same.
- **The super shotgun's firing animation faster** (23-54-46: since it can't be broken open while it fires, opening
  after a shot felt slow). Its six shot frames (shared with the shotgun's, player.qc) are paced by
  `vr_ssg_fire_anim_speed` (1.4: 40% faster, 0.43 s instead of id's 0.6 s; `player_shot_pace`, the super shotgun only;
  its rate of fire stays 0.7 s). Weapons > Reloading > Super Shotgun, "Firing Animation Speed". The log says how soon
  after the shot it broke open: 0.59, 0.42, 0.31 s at 1, 1.4 and 2 (B/Y pressed every other frame).
- **A held prop hits the super shotgun's barrels open and shut** (grenedin 00-05-36: only a fist or a gun did). A held
  prop counted by its middle, which a prop of any size keeps out of the 6 units of Hit Reach as its surface meets the
  barrels; it now counts by the nearest point of its box to the barrels' front (`VR_Reload_BoxNearest`). Test aid:
  `vr_test_held_hand 1` (Debug > Tests, "Into the Main Hand") puts impulse 252's gib or prop in the main hand. A head
  held in the main hand brought to 20 cm over the barrels opens the gun, to 20 cm under shuts it; a fist stopping as far
  off does neither.
  - The same for knocking out a magazine (nailgun, super nailgun, thunderbolt; `VR_Reload_HitFrame`): a held prop counts
    by its box's point nearest the magazine's length. A held magazine still counts by its middle and top: its box is
    square to the world and larger than it, so the bump knocked the old one out from 10 units off and the new one then
    seated on the same approach (reload_test.sh "a hit: the old one knocked out", 1 run in 6 failed). Test: a head
    brought fast to 6 units off the nailgun's magazine (the hand 5.5 off it, past Hit Reach 4; its box 1.2) knocks it
    out; the fist as far off doesn't.
- **Spent lava nail magazines smoke** as the spent thunderbolt cells do (vrfiringrange 00-00-42): a magazine of lava
  nails (the nailgun's or the super nailgun's) taken out empty smokes off its top for Spent Cell Smoke
  (`vr_reload_battery_smoke_time`, 7 s), held or lying about; plain nails' don't. Test step `vr_reload_test 16` (Debug >
  Tests > Reloading, "Spent Lava Nails or Plasma in the Off Hand's Gun"): the off hand's gun on its other ammo, its
  magazine fired dry.
- **A spent magazine doesn't go back into the pouch** (vrfiringrange 00-01-10): a magazine taken out empty and let go of
  at the pouch falls from the hand (`VR_Reload_IsSpentMag`: refused by `VR_Reload_Refund`, so also never taken as a
  pickup or at a holster); a part-used one still goes back, its rounds refunded.
- **A spent enemy gun smokes and crackles** (grenedin 00-07-22): the shot that empties a grunt's burst rifle or an
  enforcer's laser rifle starts its cues on the gun's record (its weapon_inst, which goes with it between hand and
  ground; vr_enemyguns.qc `VR_EnemyGun_Spent`, its think every 0.1 s): wisps of the spent cells' smoke off its muzzle in
  the hand, off its middle lying about (holstered: none) for `vr_enemygun_spent_smoke` (5 s); lightning's lasting arcs
  all over it, as over a struck corpse, for `vr_enemygun_spent_crackle` (2.5 s), sent again for what is left as it moves
  between a hand and the ground; the corpses' crackle (misc/power.wav) at `vr_enemygun_spent_volume` (0.25; corpses'
  0.5) as it starts. In the hand the arcs need a new shock kind, `KindGunShock` (8; QC `QVR_SHOCK_GUN` through
  `watershock`, sent to that player's client only): the client draws a body's lasting shock over the gun drawn in that
  hand (`view::heldWeapon`), its arcs 0.35 the size (a body's would dwarf the gun); dropped, the lying gun is an entity
  and takes `bodyshockdeath` as a corpse does. `vr_shock_info` lists the hands' ones ("gun in hand N"). Combat > Enemy
  Weapons, "Spent Rifles": Spent Smoke, Spent Crackle, Crackle Volume. `vr_debug_shots 1` prints where it is and when
  its cues end.
- **The pouch's shells a shell's size, in a tidy row** (the author's typed note: "overly big and not entirely
  symmetric"): make_ammo_pouch.py's shells were 0.52 units in hull radius (twice a real 12-gauge shell's at the size a
  held one is drawn), leaning and standing out at random; now 0.33 (rim 0.37: make_shell.py's 0.99 and 1.12 cm at its
  1.25 Size), evenly 0.95 apart, fanning out symmetrically (-6, -3, 0, 3, 6 degrees), the middle one standing highest.
  `ssg_checks.py pouchshells`: across the middle one 0.68 units (was 1.07), the row's distance from its mirror image
  0.000 (was 0.226). Normal map rebaked.
## Installer: the first-start relight never repeats an install's work; Play mutes the installer (2026-10-08)

Vittorio, after `make_release.ps1 -Local -RunInstaller`: "If we relighted all maps during the installation, we shouldn't
repeat the process when the game is started for the first time", and "pressing any Play button should mute the
installer".

- **What the installer does with the relight ticked:** it relights nothing itself (no `light.exe`, no
  `relight_maps.py`; VisPatch's data is only copied into `quakevr\tools\vispatch`). It only writes the game's marker
  (`quakevr\relight_on_first_start.txt`); the game's first start removes it and runs `vr_relight_batch everything`, which
  skips every map whose `.relight` has the same settings hash and source line. So there is no installer output for the
  game to redo, and nothing for `relight::customCurrent` to move to `_stale` (no installer-made copies without a
  source line). A fresh `-RunInstaller` sandbox (`%TEMP%\QuakeVR-test-<version>-<time>`) starts with no relit maps, so
  each new sandbox relights everything again: that is per install folder, by design.
- **Verified** (scratch sandboxes, `qvr-setup install --package --sandbox --relight`, then the installed `ironwail.exe`
  started as Play starts it, mock headset): first start "relighting 83 maps", 82 relit in about 2 minutes, then
  `vrstart` alone ran for over 28 minutes (light's second phase still estimating 13 minutes when the 30-minute test
  stopped it); 0 copies in `_stale`. An update install over it with the relight ticked again (marker rewritten), then a
  start: only `vrstart` (the unfinished one) relit, the other 82 skipped, `e1m1` loaded with its copy (no "out of date"
  line), 0 in `_stale`. Relight unticked: no marker, no batch at the first start (the player opted out; Graphics >
  Relighting does it later).
- **Changed:** the Play page's "Relit maps" note counts the copies the game made before
  (`FirstStartRelight.RelitCopies`: `relit_custom\<game>\maps\**\*.relight`, not `_stale` or `_work`) and, when there
  are some, says they are kept and only new or changed maps are relit; the marker's text and the game's console line
  say that maps relit with these settings already are skipped. Self-test: the installer writes no relit maps, the
  count, an update and an uninstall keep the game's copies.
- **Play mutes the installer:** both Play buttons start the game, then `UiSounds.MuteForGame`: the speaker button's
  flag set (its icon shows it), every sound and the fire's loop faded out over 0.3 s (`SoundMixer.MasterFadeSeconds`;
  the button's own mute stays 10 ms), not saved to `ui.json` (the next run starts as the button was last set), and no
  unmute when the game exits (he asked for muted). The Thanks page has no Play button; shortcuts start the game
  without the installer. The harness presses both buttons through their automation peers with the start recorded on an
  offline engine (`PlayMuteCheck`: peak 315 before, 365 half-way (the click), 0 at the fade's end and in the next
  second) and fails otherwise; the self-test checks the 0.3 s fade (heard half-way, silent at the end, no step).
- **Open:** `vrstart`'s in-game relight takes half an hour here (0 lights from its textures, 66 suns, a 17 MB map); the
  Play page's "about a minute" for a first relight is far off while it does.

## The notify lines: the game's messages only, the console's log stays in the console (2026-10-08)

- The author: console log messages showed on the wrist HUD, not only the game's; they should be in the console alone
  by default. The lines were already tagged at their source (console.c: `Con_ServerPrint` marks svc_print's lines,
  `NotifyLine::game` = a server's line that is not the engine's own reply, `engineLine`).
- `vr_hud_console_log` (default 0; HUD and Menus > Screens > "Console Log on the HUD"): 0 shows only the game's
  messages (the server's prints: pickups, deaths, chat, coop/deathmatch prints) in the gadget's log, in view
  (`vr_notify_wrist` 0/2) and at the top of the flat screen (`Con_DrawNotify`, the same filter: `VR_ConsoleLogLine`);
  1 also the engine's own lines (warnings, settings changed, command output: as before). The console keeps everything.
  Centre prints are unchanged (hologram / centre of view).
- `vr_notify_info` (Debug menu "Notify Lines Info"; also "Test Console Line"): prints the notify lines shown now, in
  view and in the gadget's log. Headless e1m1 (`say`, `vr_message_test print/console`, `exec` of a missing file):
  0: view and wrist "You got the shells", "player: hello there" only; 1: also "couldn't exec ...", the test console
  line and the startup "VR: foveated rendering ... available". Coop 1 / maxplayers 4: the server's `say` still shows.
## Relighting: a batch passes over Quake VR's own maps (2026-10-08)

The first start's `vr_relight_batch everything` relit 82 maps in about 2 minutes, then spent over 28 on `vrstart`
(BSP2, ~90k faces, lit by `vrstart_gen.py`'s final preset). Our maps ship lit as they are meant to be: relighting them
only costs time.

- **Which maps are ours** (`maps::Source::prelit`, `vr_relight_maps.cpp` `ownMap`): the worldspawn's
  `"_qvr_prelit" "1"`, now written by every pipeline that compiles or lights them (`vrstart_gen.py` WORLD_KEYS,
  `make_vrcalibration_map.py`, `make_vrtesthall_map.py`, `relight_quakevr_maps.py`); or, for the maps compiled before
  the key (none of the shipped .bsp files has it yet: recompiling vrstart takes half an hour, and a changed entity
  lump unpins a map's `<map>@<crc>.ent`), a loose map of the `quakevr` folder's own `maps/` named `vr*`: every map we
  ship is (vrstart, vrstart_old, vrtutorial, vrfiringrange, vrcalibration, vrtesthall, vrteleporters, vrslopes, vrclimb,
  vrexample). The prefix rather than a list: a new test map is covered without an engine change. A Map Library
  package's maps are in their own folder, so never caught by it.
- **Where they are skipped:** `vr_relight_batch` of a set (episode, game, library, everything, the page's choice):
  each listed as `Relight: vrstart skipped: already lit (Quake VR's own map; vr_relight relights it)`, counted in the
  start line and the summary (`..., 10 of Quake VR's own skipped (already lit)`). `-own` takes them. The map in play
  (`map`), maps named (`vr_relight_batch vrtutorial`) and `vr_relight` (Relight This Map) still relight them.
- **Times:** each map's light time is kept; the batch's end prints them all, the slowest first, and the maps over 60 s
  (also flagged as each ends: `Relight: x took 75 s, over 60 s: a slow map to relight`).
- **Test aid:** `vr_relight_whendone <commands>` runs them when the batch ends (scripted runs: `...;vr_relight_whendone
  quit`; the kit's run.ps1 splits `-Script` on every `;`, quoted or not).
- **Installer:** the Play page's first-start note says "a few minutes, about two on a fast PC" (was "about a minute").
- **Verified** (kit qbase, 32 cores): `vr_relight_batch everything -force`: the 10 `vr*` maps listed as skipped,
  `73 maps relit, 10 of Quake VR's own skipped (already lit) in 1:06` (slowest hip1m3 4.0 s; none over 60 s);
  `-own -list` gives 83. `vr_relight vrtutorial` still relights it (2.1 s; reverted after). vrstart loads its shipped
  light (no relit copy); e1m1 smoke fine.

## Stealth AI: Idle, Alert, Hostile (2026-10-08)

The author's request (stealth mechanics in the shared monster AI, all optional, on by default): the design, the rules,
every cvar and the exclusions are in `docs/vr-port/STEALTH.md`. Combat > Stealth AI (its first row **Enhanced AI**,
`vr_ai_enhanced`, switches all of it: off is Quake's AI at once, mid-map too); Debug > Tests > Stealth AI.

- QC `vr_stealth.qc` (+ `vr_stealth_defs.qc`, `vr_stealth_test.qc`): hooks in ai.qc (FindTarget's sighting of a player
  goes to the suspicion meter; ai_stand / ai_walk run the Alert phases; FoundTarget clears Alert and spreads the alarm;
  ai_run gives up a player lost for 6 s), combat.qc (sneak multiplier, bodies, explosions' noise), weapons.qc (shots'
  noise, grazing lines), vr_melee.qc (a landed blow's noise), client.qc (running, landing, touch, the gem's readings),
  vr_enemyshove.qc (an idle monster no longer shoves, i.e. notices, you), player.qc (gibs noticed), world.qc (the switch).
- Engine `vr_stealth.cpp`: `stealthlight` (R_LightPoint plus the dynamic lights, not the host's own flashlight's),
  `flashlightbeam` (vr_flashlight.cpp keeps the beam as last lit: `beamNow`), `pvsvisible`; vr_physsound.cpp calls
  `VR_Stealth_PropNoise` for each knock it plays; the wrist gadget's gem (`STAT_QVR_STEALTH`, QC `.stl_hud`).
- Map-scripted wakeups stay Quake's: monster_use, spawn-angry, damage, MG3 aggro groups call FoundTarget directly.
- Tests (`vr_stealth_test 1` on e1m1 after `god; vr_test_spawn 0; vr_test_spawn_dist 150; impulse 241`): 14 scenes pass
  (dark in front: Idle; lit: Hostile in 3.1 s standing still; half lit: a glimpse (Alert) at 2.2 s; running behind it:
  Hostile; a noise behind a wall: unheard, in the open: Alert; touch; a map trigger; sneak x1.25 (25 vs 20); grazing
  40 units: Alert, 250: not; a gib seen; the switch off mid-Alert: reset, Quake's sight, on again: dark is safe; an
  investigation 226 units behind it: walks within 71, searches, back to 24 units of its post; lost behind walls 7 s:
  Alert at the last sighting). `vr_stealth_test 2`: a crate knocked behind it (a real knock through the physics sounds):
  heard, walked to within 62. `vr_stealth_test 32` with the lamp on the head: on its back Alert at the lens, in its face
  Hostile. `vr_stealth_test 26`: the light where a map's monsters and pickups stand (e1m1-e1m3 mostly 32-96: the Dark
  and Fully Lit defaults 20 and 80).
- Found on the way: a new map's props settle with knocks in the first seconds (they woke e1m1's monsters): prop noises
  before 4 s of level time are ignored. Quake's movetogoal keeps stepping the way it faces while it can, so a walk to a
  point is re-aimed at it each second (the return walk went the wrong way for 26 s).
## Teleporters: your head through a gate, gates within gates, monsters and ragdolls through (2026-10-08)

The author's vrteleporters note 23-12-32 and the last section's "not done" list. Headless checks:
`Misc/quakevr/teleporters/teleporter_edges_test.sh <agent> [head|recursion|quake|ragdoll|monster|all]`.

- **Your head through a gate (23-12-32).** The eyes' views collapse the body's head and neck (solveTorso); seen through
  a gate (the loop room shows your back) the body was headless. In a gate's view the body is drawn with the shadow maps'
  skin (head and neck as modelled), not when that view is drawn from within 30 cm of the eyes (an eye half through a
  gate looking back). `vr_teleporter_self_head` 1 (Graphics > Teleporters, Your Head In Gates). head: facing the loop's west
  gate (2048-pixel eyes), the head makes 62..80 pixels in a box round it, 0 with the views off.
- **Gates within gates (23-12-32).** A camera's views are drawn before it, each view's own views first, deepest first,
  each depth into its own texture array (`stereo::renderPortals`/`renderPortal`; `portals::levels`, `path`,
  `drawDepth`: the render hooks act on the view being drawn). A view's views are those seen from its destination's
  leaf, beyond its exit, within its box on the screen. In a view the teleport faces are drawn (its views; the deepest
  views' faces their shimmer). The oblique near plane stands half a unit beyond the exit: the exit gate's own faces
  facing back (a liquid's faces are made both ways) lay in it and covered the whole view, dimming it. The server sends
  the rooms of the gates seen from a destination too. Settings (Graphics > Teleporters): `vr_portals_recursion` 2 (gates
  deep beyond the first; 0 as before: faces in a view left out), `vr_portals_recursion_views` 2 (gates shown in a view
  through a gate; deeper the best one alone), `vr_portals_recursion_max` 4 (views within views an eye, the gate looked
  at most first), `vr_portals_recursion_scale` 2 (each gate deeper drawn at half the pixels a side: `r_refdef.scale`).
  `vr_portals_view` prints each depth's views. recursion: facing the loop's west gate 40 units out (4096-pixel eyes),
  the green signs stacked in the middle of the left eye: 1, 2, 3 at recursion 0, 1, 2.
- **Cost** (2048-pixel eyes, the loop gate filling the view: its view sees the far loop gate and T's north gate;
  settings interleaved in one run, medians of four): CPU busy a frame 2.15 ms at recursion 0, 5.65 at 1, 6.56 at 2,
  6.99 at 3 (the fourth gate deep is out of range); scale 1 at recursion 2: 7.87; one view a gate (views 1): 5.68.
  Profiled, an eye: the first view 0.6 ms CPU, 0.6 GPU; its two views one deeper 1.3 CPU, 0.7 GPU; two more 0.6 CPU,
  0.4 GPU. start's episode gate (no gate within it): 4.59 -> 4.98. The machine was busy (other agents' builds and
  relights): the frame times were noisy, the interleaved medians agree.
- **Quake's particles, sprites and see-through models behind a gate** (not done last round): with a see-through gate
  surface (no depth written) they showed over the view. BehindShownGate (vr_glsl.h) in their fragment shaders (Quake's
  particles, sprites; alias models with alpha under 1; the alias shaders' frame block reaches the gates under its own
  names). Test aid: `vr_particle_test quake` (Quake's explosion particles and its explosion sprite, held two seconds),
  Debug > Teleporters: Quake's Effects Behind A Gate. quake: behind start's underwater gate 0..2 pixels over the gate
  (671 with the test off), in front of it 15000 (a control).
- **Ragdolls and corpses through gates** (not done last round). A ragdoll's parts get portal copies through the gate
  one of its parts goes into (each part over that aperture; one against the frame has none; the parts' boxes 4 units
  in: lying on the floor they reach under a floor-level gate's bottom edge), a dead monster's own body
  (`vr_corpse_collide`) too; `carryRagdolls`: a ragdoll whose pelvis goes in through an aperture in a step is carried
  whole, every part by the gate's mapping (`portals::crossedGate`). ragdoll: a dead grunt blown head first into FA's
  player and large gates: carried, pelvis at y 1100..1170 (three runs each); `vr_portals_walk 0`: against the wall
  behind the gate. Copies for the whole ragdoll's box only (the first try) stopped it at the player gate half the runs.
- **Monsters through paired gates** (not done last round): `vr_portals_monsters` 1 (Graphics > Teleporters, Monsters
  Walk Through). A monster's box goes into a paired gate's aperture (VR_PortalBodyMove, as a player's) and
  VR_PortalMonsterCross (SV_Physics_Step, after its think) carries it as its torso crosses (ideal_yaw turned too);
  QuakeC's teleport_touch leaves it to the engine when its box goes in as it stands, a step up or already straddling;
  else, and at unpaired gates (id maps' teleporters), Quake's teleport. monster: a dog chasing through the flush
  player gate comes out at y 932 (the face 928), a 32-unit run step unfolded; off: 976, a 76-unit jump.
  teleporters_test.sh chase: all through as before (out 937..966).
- **Not done:** a non-ragdoll corpse falling (MOVETYPE_STEP, dead) is not carried (ragdolls are on by default); the
  torch lights of rooms two gates deep are not selected (their lightmaps are); a monster's navigation stays local (it
  goes through a gate only when its way to its goal leads into it).
- **To try in VR:** in vrteleporters' loop (Debug > Teleporters > Into the Loop) look into the west gate: your back with
  its head, and yourself again a gate further and once more; check the frame rate there and with Gates Within Gates
  0..3; start's underwater gate with Opacity 0.3 and an explosion behind it (Quake's particles: Particles off); kill a
  grunt in front of FA's large gate and shoot or throw the corpse in; let a dog chase you through the flush player gate
  (it walks out of the far face, not a step out).

## One grenade from either pouch; the launchers' rounds in the ammo pouch (2026-10-08)

The author: the front pouch shows rockets, grenades and proximity mines as real rounds, as many as there is ammo; the
back pouch's grenades load into the launchers, the front pouch's arm by hand, "the exact same prop/entity, maybe with a
different default grip". Tests: `Misc/quakevr/reload/pouchgren_test.sh` (14 checks).

- **One entity** (QC vr_grenade.qc `VR_HandGrenade_Make`): both pouches give the same thing, a hand grenade
  (`.vr_hgren`: the pin, the fuse, the catch, the throw as before) that is also a launcher's round while unarmed
  (classname `vr_ammo_front`, `.vr_ammo_front` its launcher, `.vr_ammo_aid` its ammo: vr_reload.qc loads it butt first,
  takes it back at the ammo pouch, loose by contact, its shot/blast detonation). Armed (`VR_HandGrenade_GoLive`: the
  pin, or a dud set off) it takes its Quake classname (`grenade`, `MultiGrenade`, `proximity_grenade`) and no longer
  loads, nor goes back into a pouch. vr_reload.qc `VR_Reload_MakeRound` makes it for the grenade and proximity
  launchers; rockets stay rounds only. Ammo as before: one off the rockets (the multi-rockets) as it is taken, back as
  it is put in either pouch or at a level's end; loaded, it is the launcher's clip.
- **Its model: the launcher's round** (make_rounds.py), not progs/grenade.mdl: Quake's grenade is 8 by 3 units, wider
  than the launcher's bore, and has no butt; the round fits the muzzle. Decision: the back pouch's grenade looks like
  the round now (the author's grip fit for progs/grenade.mdl, slot 4, stays for caught ogre grenades). New skins:
  vr_round_grenade.mdl 2 and 3 the grenade and multi-grenade armed (the stripe round the body glowing red, amber; 0 and
  1 a yellow and a dark stripe), vr_round_prox.mdl 1 armed (its lenses lit; unarmed now dull). The engine smokes an
  armed one (vr_particles.cpp `VR_RoundTrail`, cl_main.c: the models have no trail flag, so a round lying about never
  smokes). Its bounce is the engine's metal knock (QC's weapons/bounce.wav skipped for it). Normal maps rebaked.
- **The two grips**: from the back pouch it comes out turned along the hand to be thrown (vr_grenade_pouch_hold_*, as
  before); from the ammo pouch, the round's own grip (In the Palm), ready for the muzzle. Brought to the muzzle as it
  comes out of the back pouch it lies across the barrel (the dull tap): the hand turns it butt first.
- **Which one the back pouch gives**: the round of the grenade or proximity launcher in the other hand (by its ammo
  mode: the multi-grenade), else a grenade; B/Y held: the multi-grenade, or with no multi-rockets a proximity grenade if
  the player has the proximity launcher, else a grenade.
- **A proximity grenade armed** has no fuse (its lenses lit, no ticking); let go of, it is the proximity launcher's mine
  (`VR_HandGrenade_Mine`: W_FireProximityGrenade's think, touch and life, counted in NumProximityGrenades, now declared in
  vr_grenade.qc), drawn as the round. Unarmed and shot it goes off with the launcher grenade's blast, as the round did.
- **The ammo pouch's rounds** (make_ammo_pouch.py, vr_view.cpp `ammoPouchFrame`): frames 14-16 rockets (up to 3), 17-20
  grenades (4), 21-24 proximity grenades (4), standing nose up, as many as the reserve has, spaced evenly about the
  middle (make_rounds.py's meshes at 0.55, 0.75, 0.7); skin 1 (STAT_QVR_POUCHKIND's 8, now also for the multi-rockets)
  the multi-rocket's and multi-grenade's colours. 25 frames, 6982 vertices (the file 0.97 MB, was 0.38).
- Test aids: `vr_dumpview` prints each view entity's skin; the reload test report (`vr_reload_test 0`) prints the
  pouch's count, the mines out and health.
- Unchecked in VR: the back pouch's grenade's turn in the hand as a round (vr_grenade_pouch_hold_* were fitted to
  grenade.mdl); the round's weight in a throw (0.5 kg, slot 55; grenade.mdl's 1.2).
- Also: `vr_reload_test 16` was two steps (the launchers' round tossed the wrong way round shadowed the spent lava
  magazine's, so reload_test.sh's two lava smoke checks failed): the first now only with a launcher in the off hand.

## Stealth AI: the gaps closed (2026-10-08)

The coordinator's follow-up to "Stealth AI" (above): real shots, coop, a review, the cost. STEALTH.md has the rules
as they now stand (its Status lists what remains).

- **Real shots heard** (`vr_stealth_test 100`, `stealth_tests.sh gun`): the earlier runs fired nothing because no weapon
  was in the hand; `vr_weapon_grip_mode 1; impulse 9; impulse 150+id` puts one there and `+attack` with
  `vr_mock_button main trigger 1` fires it (as fist_alert_test.sh). Every weapon's real trigger pull (shotgun, super
  shotgun, both nailguns, grenade and rocket launchers, lightning) is heard by a grunt at 0.8 of its reach in the open,
  not at 1.2, not at 0.8 behind a wall (x0.5 round it); the noise's reach is the table's (1100, 1300, 800, 900, 600, 900,
  900). The rockets and grenades are taken away at once in the test (their blasts are scene 101's).
- **Explosions by size** (101): 50 damage heard at 800 (near yes, 1.2 no, behind a wall no), 200 at 1600 (all three).
  New `vr_stealth_noise_blasts` (1, Combat > Stealth AI, Explosions Heard), the plan's table corrected (8 a point of
  damage, at least 800; it said 6 and 600).
- **Coop** (`stealth_mp_test.sh`, listen server and a client over UDP, ~90 s): each client measures the light on its own
  player and sends it with its lamp's beam in its VR move (`VrMove::light`, `lampLit` + lens, axis, range, cone: the
  move block grows by a byte, a float and 32 bytes while lit); QC `clientlight(player)`, `flashlightbeam` for any player.
  The meter is on the most suspicious player in sight (the turn-taking sightings no longer average the players: the lit
  one is spotted in 3.2-4.0 s with the dark one beside him, either way round, as alone). A lamp's glare lights only its
  holder (`stl_lit_by`: it spotted the dark player standing by the lamp's holder). The client's lamp on a grunt's back:
  Alert at his lens (0 units off). The scene turns the client's lamp off and on by `stuffcmd`.
- **Bugs found and fixed:**
  - Rogue's invisible swordsman (`monster_sword`: its walk is its stand) overflowed the QC stack on the first noise
    (Stand started its walk, its walk is ai_stand, which started its walk...): an Alert monster's own stand/walk switch is
    guarded (`stl_anim_guard`), and a kind that doesn't walk searches where it stands.
  - Dormant monsters (no damage taken or not solid: Rogue's statue knights before their trigger, the Guardian before it
    rises, monsters waiting to be spawned in) were woken by noise, touch, a beam or another's alarm (a statue's idle "?"
    sound, `FoundTarget` on contact): they keep Quake's AI until their map wakes them.
  - An investigating monster walked into lava or slime level with the floor (Quake's step check lets it): the walk now
    stops at the edge (`vr_stealth_test 108` on e1m7: it walked 6.6 s up to the lava and stopped 49 units from the first
    dry place; placed at the edge it died in it before).
  - A notarget player's shots and blows were heard (his running wasn't): none of his own noises are now.
  - The crate scene (`vr_stealth_test 2`) failed when run first on a map (the props' first 4 s are unheard) or when the
    crate flew long: it waits.
  - A marker entity whose monster was removed without dying leaked; it now goes.
  - The Alert log's sprintf was built on every alarm with the log off.
- **Water:** a noise across a water surface (one under, the other not) is muffled as round a wall. Monsters standing in
  water investigate as on land; swimmers keep Quake's AI.
- **Checked, fine:** save and load mid-investigation (`vr_stealth_test 104`, save, load, `105`: it walked on, searched and
  came back to 21 units of its post); a changelevel mid-alert (no error; a shot on the new map heard at once);
  infighting (`107`: the struck grunt fights the other, nothing shared, and after it died it stands Idle, not seeing him
  in the dark); every kind the kit has (`106`: id1's, hipnotic's and rogue's walking monsters dark-idle, knock-alert,
  lit-spotted in 3.1-5.3 s; scrags and wraths left to Quake's AI; MG1, MG3 and the dopa monsters aren't in the kit:
  untested); demo playback (demo1-3, both game stacks: no server runs, so no stealth QC at all: 0 stealth log lines at
  `vr_stealth_debug 2`; the client sends no move then, its light measurement returns -1).
- **Cost** (`vr_stealth_test 103`, a new "stealth" profiler scope round the QC's entry points, `stealthprofile`):
  0.018 ms a server frame with 45 grunts watching you and hearing your steps (16 spotted you; worst 0.11); 0.062 ms with 60, ten dead and fifty
  investigating them (worst 0.51 ms, the frame a knock is heard by them all); under the 0.2 ms budget, so nothing was
  optimized beyond the players' list kept per frame (the beam check no longer searches the edicts by name per monster).
- Tests: `bash Misc/quakevr/stealth_tests.sh <agent> all` (39 PASS, ~70 s), `bash
  Misc/quakevr/multiplayer/stealth_mp_test.sh <agent>` (7 PASS), and the earlier `vr_stealth_test 1` / `2` (14 PASS).
  Debug > Tests > Stealth AI has every scene.

## Teleporter monster tests: deterministic again; a spawned monster no longer forgets the player it just saw (2026-10-08)

- **Cause** of `teleporter_edges_test.sh monster` printing "never through" (both `vr_portals_monsters` 1 and 0) and of
  `teleporters_test.sh chase` never reaching the north room: the stealth AI's meter. The tests give the dog 40 frames to
  spot a still, lit player 200 units ahead; the meter fills at about 0.3 a second there (seconds), so the dog was still
  Idle when he was moved behind the wall, and dogs don't look through gates (only ranged monsters do, PORTAL_AI.md).
  Both tests now run with Quake's sight (`vr_stealth_meter 0`; the rest of the stealth AI stays on): they test the
  crossing, the stealth AI has its own tests.
- **The old flakiness** ("now and then the dog doesn't see the player", the S2 note): walkmonster_start_go (and the
  fly/swim ones) calls th_stand(), whose FindTarget can spot the player at once for a monster spawned mid-map (a
  dispenser, the test spawner, a trigger-spawned one); its next lines then set `enemy = world` (Honey's not-angry
  branch), leaving it running with no enemy, back to standing until a later look (0.1-0.6 s on). If the player had
  gone behind the wall by then, it never saw him again. Fixed in monsters.qc: that branch leaves an enemy just found
  (`self.think == self.th_run`). Map-start monsters are unchanged (no player to see then).
- **Results**: monster section five runs: through at y 932 (a 32 step; once 935, a 16 step) with walking, 976 (a 64-101
  jump) without, every run; 12 more parallel runs of each, all through. chase five runs: the flush player dog 30-60
  frames, sill-16 dog 30-450, fiends 45-60, every run; the sill-32 dog 4 of 5 (it slides to the sill-16 gate: pathing);
  the grunt never (it shoots through the gate, as designed). stealth_tests.sh all 39 PASS, `vr_stealth_test 1` 0 failed.
- **Not done (stealth through gates)**: a ranged monster's meter fills through a gate (visible()'s gate route), but its
  Alert point is the player's real origin (behind the wall in its room's terms), not his image, and a noise is heard
  only in the room it is made in (findradius round it, walls between). Mapping the point to the image needs the walk's
  marker, post and lost-at point carried when the monster crosses (VR_Portal_Crossed), or it would walk back and forth.
  (Done: the next section.)

## Stealth AI through teleporters (2026-10-08)

The three gaps above, closed (STEALTH.md "Teleporters" has the rules; new `vr_stealth_gates 1`, Combat > Stealth AI >
Through Teleporters; off: sight through gates is ranged monsters' only and noises stay in their room, as before).
- **Seen through a gate**: the Alert point (a glimpse; a Hostile one's last sighting, `stl_lost_at`) is the player's
  image in the gate, with the gate (`stl_via`: the face it walks into, an exit's pair face; minus the gate when it can't
  walk through). The walk goes into the face's middle (48 behind its plane) until it crosses; a gate it can't walk
  through: up to its face, then it searches.
- **Crossing**: the engine's VR_Portal_Crossed now gets the gate's yaw, the face gone into and the face back (players'
  crossings too; `Side::pair` kept by pairExits). VR_Stealth_Crossed carries the point, post (and yaw), last sighting,
  walk check and marker: the point walked for is in its room now, the others beyond the face back (its return walks back
  through, then on along its path or to its post).
- **Noise**: VR_Stealth_NoiseGates: each active paired gate whose exit the noise is in front of: the monsters on the
  gate's side hear its image, the way's length through the aperture (held to its edge: x `vr_stealth_noise_wall`), each
  room's half through VR_Stealth_NoiseLine (the room rules: walls, solid, props, water). Alert at the image via the gate.
- **Every monster sees through paired gates** under the stealth rules (visible(); FindTarget's portal_ai_client); a
  melee one Hostile at a player it sees only through a gate runs through (VR_Stealth_ChaseGate, before its attack checks:
  no jumps at his real place). Ranged ones as before (stay and shoot).
- Engine builtins `portal_ai_gateinfo(gate, what)` (count, flags 1 active / 2 paired / 4 monsters walk through, face) and
  `portal_ai_gate(gate, what, p)` (middle, normal, p held to the aperture, p carried through).
- **Tests**: `stealth_tests.sh <agent> gates` (QC vr_stealth_test3.qc, `vr_stealth_test 110` on vrteleporters from room U;
  Debug > Tests > Stealth AI > Through Teleporters): 11 PASS, the same numbers in 5 runs: the grunt Alert at the image
  (-1280 908, 0 off) via T's north gate after 1.2 s, through, within 63 of his spot, back at its post after 47.3 s with 2
  crossings; a knock (600 reach; 975 the straight way, 459 through the gate) the same (48.8 s), not heard with
  `vr_stealth_gates 0` nor at 0.85 of the 459; the dog through to him in 4.4 s (meter, then Hostile, through), with
  `vr_stealth_gates 0` its meter 0; `vr_ai_enhanced 0`: the dog no gate, the grunt Hostile through it. stealth_tests.sh
  all 50 PASS, `vr_stealth_test 1` 0 failed, teleporter_edges_test.sh all and teleporters_test.sh chase as before (the grunt
  still shoots through and stays).
- One hop only: a point two gates away is walked to through the first gate (then searched there).

## Backlog fixes: hit models and the cache, spawner angles, mapless runs, corpses through gates, torches two gates deep (2026-10-08)

- **Precise hit detection's map-start meshes and the model cache** (vr_hitmodel.cpp afterLoad): the loop that gathered
  the meshes to make on the thread pool loaded each model's data (Mod_Extradata) and kept its header; a later load could
  let an earlier model's data go from the cache, and the pool then read a stale header. Now every model's data is loaded
  first, then the headers are taken again with Cache_Check (loads nothing), as ragdoll's warmRigs does; one gone by then
  is made by meshOf on the main thread. Test aid `vr_hitmodel_cachestress 1` (default 0; Debug > Threads > Evict Models
  at Map Start) drops each model's data from the cache once the next is loaded. Test: `developer 1;
  vr_zone_threadcheck 1; vr_hitmodel_cachestress 0|1; map e1m1; map e1m2`: the same hashes either way (e1m1 3f87ac75,
  e1m2 c457bcba), no thread catch.
- **Monster dispensers face their "angle"** (buttons.qc func_enemy_dispenser_use): the monster made takes the
  dispenser's yaw before its spawn function (walkmonster_start's ideal_yaw comes from it); before, always 0 (east).
  vrfiringrange's and vrteleporters' dispensers are at 0 (no change); vrexample's grunt at 180 now faces west. Test aid
  `vr_test_dispenser <n>` (console; default 0): the map's n-th dispenser used as its button would, `test dispenser:`
  line. Test: `developer 1; map vrexample; vr_test_dispenser 1`: `spawn_grunt (yaw 180) made monster_army yaw 180`.
- **Test runs use no network; a cancelled download never waits on a host name lookup** (host_cmd.c, vr_mapindex.cpp).
  Not reproduced here: 40+ mapless runs (`quit`, `toggleconsole;quit`, `togglemenu;quit`, menus, `disconnect`, crash
  paths, qbase/qrp, -RealTime, 4 instances in parallel, quits during a live index fetch at 2-58 s) all `exit=0` in
  2-3 s; the 2026-10-07 Host_Quit_f fix covers the quit confirmation. What remained network-bound at a short run's quit:
  Ironwail's add-on list download (Modlist_ShutDown waits for its thread with no limit) and the map index's start-up
  fetch, both started at every launch; with no network a host name lookup can take many seconds, and libcurl's
  threaded resolver is waited for when a transfer is removed (EXPANSIONS.md also saw a map-index libcurl worker crash
  in such runs). Now: a test run (`QVR_TEST_BACKGROUND`, set by the kit) skips the add-on list (`Add-on server disabled
  (test run)`; `-addons <url>` still fetches) and reads the map index's cache only at start-up (`maps_fetch` still
  fetches); every Download() sets `CURLOPT_QUICK_EXIT` (libcurl 8.10: a cancelled transfer lets a pending lookup's
  thread go instead of joining it). Players' launches unchanged apart from QUICK_EXIT. Tested: mapless
  `wait5;toggleconsole;quit` exit=0 in 2 s with both lines in the log; `maps_fetch` reads/fetches as before.
- **Corpses through gates** (vr_box3d.cpp carryCorpses, after carryRagdolls): a dead monster's own body that is not a
  ragdoll (vr_corpse_collide 2/4, pushable: Box3D moves it) is carried whole when its middle goes in through a paired
  gate's aperture: its body moved and turned by the gate's mapping, its speed and spin turned, its entity written there
  (writeCorpse; a box's yaw turned). Its portal copy already let it past the wall behind the gate, so before it went into
  that wall's far side and stayed there. `vr_physics_fling` now moves a pushable corpse (its body's velocity; a blast
  does not move a 150 kg corpse). Test: `teleporter_edges_test.sh <agent> corpse` (vr_ragdoll 0, a dead grunt flung 300
  north into FA's player and large gates): 1 crossing each, ending at y 983..1194 (> 928, two runs); `vr_portals_walk 0`
  control 606..650; with the carry off it ended at 884 and 660 (inside the wall's far side, 0 crossings). The ragdoll
  section as before (1097, 1169; controls 616, 674). Not done: corpses of the fixed kinds (vr_corpse_collide 0/1/3:
  Quake moves them, and a corpse there never flies).
- **Torch lights through gates within gates** (vr_portals.cpp prepareLightViews, lightDistance): the views whose
  torches and flames light (VR_TorchLights) now go as deep as the views drawn (vr_portals_recursion: maxDepth), each
  round the gates in front of the last round's carried eyes, in their PVS, near (kRange) and beyond the exit they look
  out of (as VR_PortalAddPVS's rooms), breadth first, at most 16 views (was 9, one gate deep). A view through a gate
  now counts a torch only in front of its exit (its plane facing the room it looks into): in an unvised map such as
  vrteleporters every room is in every PVS and a carried eye lands near unrelated rooms (a torch at 200 800 counted at
  845 units from T through the loop, now 1482: out of reach). Cost: 1.1 us a frame for 16 views in vrteleporters' T
  (0.3 us for 5 before). Debug aid `vr_portals_lightviews [x y z]` (Debug > Teleporters > Torch Light Views): the views,
  their depth, side and eye, the cost, a point's torch distance. Tested from T (-1280 700, facing U's gate):
  recursion 0: 5 views, depth 1 (as before); recursion 2: 16, depth 3. vrteleporters has no third room (T and U, FA and
  FB lead to each other), so no torch there is lit only two gates deep; a map chaining three rooms would show it.
## Dawn of the Machine (MG3): the full-campaign route sweep (2026-10-08)

M3-29 of [MG3.md](MG3.md); the results table is in EXPANSIONS.md, "Dawn of the Machine route sweep".
`bash Misc/quakevr/mg3_route_test.sh <agent> [main] [bn] [exits]` (about 9 min): the entity checker (22 BSPs, 0
missing), the language gate (static and the owned table), then three headless legs on the owned data:
- **main** (skill 1, 93/0): start's skill brush, every chapter and secret map in order through their real exits, the
  four runes and four hub returns (doors and exit as the runes say), secret2, Chthon's fight and death, the finale text,
  the credits. Hands, holsters, magazines, ids, upgrade masks and capacities seeded in map1 are carried through every
  later step; each first visit saves, loads and dies for real (the respawn's autoload), and the three reports agree.
- **bn** (29/0): the hub's skill buttons to Bloody Nightmare, its strip after a changelevel, Chthon's death into its new
  game (map1, serverflags 448, masks cleared), secret1's rune, the hub's exit to secret2 leading to boss2, Shub's death,
  the final text, the credits.
- **exits** (71/0): all 34 exits of the 20 playable BSPs (both of the hub's exits to each chapter) arrive in their map.

Fixed: `Quake/vr/vr_loc_mg3.inc` (the language gate) lacked `$mg3_qc_boss_finale` and `$map_dopa_endtext_final`, the
two endings' texts (now 248 identifiers; the owned table has both); the route presser of `QC/vr_mg_hub_test.qc` (MG1's
and MG3's sweeps) ran its end script as soon as a finale text appeared, whose waits kept the next press's +jump in the
command buffer: now it runs it after the credits' commands are queued. New Debug > Tests > Dawn of the Machine Tests
rows: Route Carry Report (`vr_mg3_test 31`), Seed Hands and Holsters (32), Give the Four Runes (36).

**In the headset.** [ ] Dawn of the Machine, either ending: at the finale text a button press brings the credits.
[ ] A hub return from a secret map: you arrive at the hub's start with your hands' and holsters' weapons as you left.
## vrtutorial2: the new tutorial, a generated military base (2026-10-08)

**Asked:** a polished tutorial map made like `vrstart` (a generator, id textures, a day sky with its sun, aligned
textures, curved halls, 0 holes), 12 lessons and an arena, heavily scripted, the official tutorial (first start, hub,
menu) with the old `vrtutorial` still loadable.

**Done.** `Misc/quakevr/maps/vrtutorial2_gen.py` writes `quakevr/maps/vrtutorial2.map` (MAPPING.md, "vrtutorial2", has
the build, the lessons table and the start flow). Final preset: 9 s to compile, 12,580 faces (12,222 world, 4,660
leaves), 0 holes in 1,000,000 rays (`bsp_holes.py`), no "sides not found" or "couldn't create brush faces". Worldspawn
`_qvr_prelit 1`. Day sky `qvrday` (`make_day_sky.py`); arrows and hazard stripes in `quakevr_dev.wad`.
- New QC (`vr_tutorial.qc`): `trigger_vr_health_gate`, `func_vr_spawner`, `func_vr_target`, `func_vr_restock`,
  `info_vr_checkpoint`; `vr_crate` `contents`/`target`; `func_vr_tip` TRIGGERED (8). FGD regenerated.
- Start flow: `vr_tutorial_started` (archived, 0; configs from before version 103 get 1). `vr_startgame`, the first
  calibration's exit, the hub's and the menu's VR TUTORIAL all run `skill 0; map vrtutorial2`; the map sets it to 1.
  vrstart.bsp's button edited in place (`bsp_set_entities.py`; `vrstart_gen.py` matches).
- Mock autopilot for scripted tests: `vr_mock_walk_to`, `vr_mock_turn_to`, `vr_mock_hand_aim`. Debug > Tests >
  Tutorial: the map, the old one, First Start Again, Go to a Lesson (`vr_tutorial_goto`).

**Tested** (`vrtutorial2_playtest.py`): the playthrough to the hub passes 38 of 38 gates on Easy without god mode
(health 87 after the fight, 62 after the arena), and 38 of 38 with god mode from the fight on; softlock checks 9 of 9
(death at a checkpoint with the keycard kept, a triggered tip through save and load, ANOTHER ENEMY's limit, bench
restocks). Start flow: a new config's `vr_startgame` goes to vrtutorial2 at skill 0 (a done one to vrstart); a first
start runs calibration, then vrtutorial2 at skill 0; `vr_migrate_config` from 102 sets 1. Loads (exclusive): cold
903 ms, warm 131 ms. Arena waves: ~1.0 ms CPU, ~1.0 ms GPU, worst 6.8 ms. e1m1 smoke, vrstart walk 18 of 18, menu
path checks 0 missing, QC 0 warnings, FGD check clean.
- A test fix: a keycard knocked off its float onto the floor is taken crouched from 44 units (a lean carries the body
  onto it, and a body's touch isn't a hand's).

**For VR:** the scale of the courtyards and the climb (ladder rungs every 20, the 112 jump wall's ledge), reaching the
floating keycard, the throw at the barred button, the arena's pace on Easy, the brightness by day.
**Open:** the dark course (lesson 9) relies on the flashlight entirely; whether a dim fill light is wanted.

## The Super Axe in the firing range (2026-10-08)

Vittorio: "Can you add the super axe to vrfiringrange?" Dawn of the Machine's Super Axe (WID_SUPERAXE 18,
vr_mg3_weapons.qc) now lies in vrfiringrange on the aisle floor between the axe's weapon pad (x -488) and Mjolnir's
(x -424), at -456 -872, as the swords lie north of the last pads: a `func_weapon_grabbable` with weapon 18, added by
`Misc/quakevr/make_prop_area.py --ent-only` as the entity file's last entity (no entity before it renumbered: the
dummy stays 137, the long explosive box 207; the .ent keeps its pin, `vrfiringrange@3647.ent`, since the .bsp is not
changed). Its model and sounds are MG3's, read in place from the owned pack; without that data
`CreateThrownWeapon` makes nothing (`VR_Pack_WeaponAvailable`), and, as no weapon pad is labelled, nothing says so.
No QC change. Like every weapon in the range it is placed once as the map loads (none restock).

**Tested** (kit base, MG3 data found: `vr_mg3_wtest 1` "superaxe data 1"): taken from the floor by the main hand
(`vr_mock_hand_to main weapon 0.3`, grip held: main hand weapon 18), holstered in the right shoulder and drawn back
(`vr_test_weaponinst 1`/`2` with slot 4), a mock swing on the training dummy ("Dummy: 60 damage - melee: Super Axe"),
thrown at it ("thrown weapon: Super Axe, 12.5 m/s ... killed"), and `motion_synth.py slash_horizontal_rtl --weapon
superaxe --distance 0.85` after the pickup ("chop (horizontal) with the head"). Without the data (`-nosteam -nogog
-noepic`): "superaxe data 0", no weapon 18 lying about, no errors. e1m1 smoke; menu path check 0 missing.
- Test note: `set vr_test_weaponinst 1; impulse 120` ran as step 0 here; the plain `vr_test_weaponinst 1` works.

**For VR:** whether the aisle spot reads well (it is darker than the pads), and taking it off the floor.

## Wrist flicks in bullet time: the arm tells the tempo (2026-10-08)

vrfiringrange_2026-10-08_10-30-08: an upward wrist flick, the hand nearly still, went much further in bullet time than
at full speed. The props batch's fix (vr_throw_slowmo_flick_spin 40) told a real-speed flick from one made slowly with
the world by the controller's spin in the game's time; a gentle flick (12 rad/s real, 40 of the game's at 0.3x) sat
at the threshold and still went 6 to 10 times as far, and the part of a flick the old share model left to the arm
(the flick model's speed over the throw's: 54 to 85% for a pure wrist turn) stayed in the game's time.

No spin threshold can do better: a gentle flick at real speed and a fast one made slowly with the world are the same
motion. So the arm tells the tempo (vr_throw.cpp motionRate, `Tempo`): the arm's motion is the wrist's, the
controller's point's velocity less what the hand's turn gives it 7 cm ahead of the wrist (armVelAt: vr_throw_wrist_dist
along the throws' forward). A throw the wrist carries (the arm's fastest speed around the release under
vr_throw_slowmo_flick_arm, 0.4, of it plus the wrist turn's at the held object; under 0.3 wholly, between a blend) is a
wrist flick: made at real speed, all of it (the arm's small drift with it) kept in the player's real time, not capped
by the slowed hand (whose 20 rad/s turn limit a fast flick's 130 game rad/s is many times). A throw the arm carries is
judged as before (the controller's speed against vr_timescale_hand_speed; its flick share and the hand's cap as
before). A hand that didn't lag its controller (a gentle flick under the turn limit) no longer skips the scaling.
vr_throw_slowmo_flick_spin is retired (vr_cvars.cpp retiredCvars); the menu's "Flick Spin" row is now "Wrist Flick
Below". Two-hand throws and full speed are untouched.

Misc/quakevr/throw_slowmo/flick_slowmo_compare.sh runs flick_slowmo_test.sh three ways in one table (metres at 45
degrees). The test now keys the flicks on the point the throw's velocity is of (vr_controller_legacy_pose 0 for them:
the legacy raw point is centimetres off the grip, so the wrist the mock turned about wasn't 7 cm behind it) with the
controller pitched as the throws' frame takes it (39.5), adds upward flicks (gentle ~12, medium ~24, fast ~40 rad/s)
and two with the hand drifting up at 0.5 m/s, and keys the overhands at 1000 a second (at 90 they moved by 20% with
where the frames fell when the flicks before them changed):

| throw       | full | bullet before | ratio | after | ratio | made slowly before | after |
|-------------|-----:|--------------:|------:|------:|------:|-------------------:|------:|
| flick_fast  | 1.37 |          2.40 |  1.75 |  1.37 |  1.00 |               1.44 |  0.21 |
| flick       | 0.75 |          0.98 |  1.31 |  0.75 |  1.00 |               0.87 |  0.10 |
| flick_soft  | 0.30 |          1.70 |  5.67 |  0.30 |  1.00 |               0.31 |  0.01 |
| up_gentle   | 0.26 |          1.66 |  6.38 |  0.26 |  1.00 |               0.28 |  0.01 |
| up_medium   | 0.78 |          1.04 |  1.33 |  0.78 |  1.00 |               0.90 |  0.10 |
| up_fast     | 1.05 |          2.93 |  2.79 |  1.05 |  1.00 |               1.61 |  0.25 |
| up_gentle_d | 0.44 |          3.27 |  7.43 |  0.44 |  1.00 |               0.47 |  0.02 |
| up_medium_d | 0.94 |          1.80 |  1.91 |  0.94 |  1.00 |               1.23 |  0.13 |
| overhand0   | 4.02 |          5.83 |  1.45 |  5.83 |  1.45 |               4.06 |  4.06 |
| overhand    | 2.45 |          2.67 |  1.09 |  2.67 |  1.09 |               2.39 |  2.39 |

throw_slowmo_test.sh (keyrate 1000) in bullet time at real speed and made slowly: all eight throws bit for bit as
before. The trade (the same-motion ambiguity): a flick made slowly with the world now goes as gently as the player
really flicked (the "made slowly" column), where it went as at full speed. A slow-tempo arm throw with a big wrist snap
(the overhand: the arm 0.48 of it) stays the arm's; a gentle flick with the hand moving over about 0.7 m/s real starts
to count as the arm's (wholly from about 1.1 m/s; then amplified as an arm throw is). In VR: flick things up and forward in bullet time, gently and
hard, hand still and moving a little: each should land about where it does at full speed; overhand and lob throws as
before.

## Nudges in bullet time: the hand's travel tells (2026-10-08)

vrfiringrange_2026-10-08_14-20-31, 14-39-26: flicks felt right in bullet time, but a tiny upward hand movement (wrist
straight or flicking) threw massively high, barely at all at full speed; and a deliberate slow throw made with the
slowed world must still go as at full speed. A nudge's speed is under what the slowed hand follows, so motionRate took
it as made slowly with the world and kept it in the game's time: 1/0.3 as fast, 11 to 14 times as far.

His rule: the hand's travel tells. vr_throw.cpp strokeOf measures the stroke on the controller's own samples: from the
fastest speed in the release's window back while the speed stays over a fifth of it (and 0.1 m/s real), at most two
real seconds, and on to the release; its path (metres) and length (real seconds). Under vr_throw_slowmo_short_travel
(0.15 m) the throw is a nudge (Tempo::nudge 1): its windows in real time and its speed and spin the controller's real
ones (the game's times the time scale), as strong as the same motion at full speed; from vr_throw_slowmo_long_travel
(0.2 m) the bullet-time scaling as before; between a blend. The path is to the release, which comes about 60% into a
lob (the lobs' 40/50/60 cm read 0.23/0.32/0.35 m); his "20 to 25 cm" of whole motion is about 15 to 20 to the release,
hence the defaults. The duration bounds the stroke and is printed. vr_throw_slowmo_real_strength 1 (default 0): every
bullet-time throw as in real time, no scaling (one made slowly goes as slowly). Both hands' throws too. Menu: Hands >
throwing's slow-motion rows "Nudge Below", "Arm Throw From", "Throws As In Real Time". vr_debug_throw 1 prints "the
stroke X m in Y s, a nudge xN".

flick_plays.py adds nudges (the wrist straight, the hand up 5/10/15 cm over 0.15 s, let go at 60%; one of 10 cm with an
upward flick) and lobs (the arm forward and up 40/50/60 cm over 0.25 s). flick_slowmo_compare.sh, metres at 45 degrees:

| throw     | full | bullet before | ratio | after | ratio | slowly before | after | real_strength 1: bullet, slowly |
|-----------|-----:|--------------:|------:|------:|------:|--------------:|------:|--------------------------------:|
| nudge5    | 0.04 |          0.56 | 14.00 |  0.04 |  1.00 |          0.04 |  0.00 | 0.04, 0.00 |
| nudge10   | 0.16 |          2.22 | 13.88 |  0.16 |  1.00 |          0.16 |  0.01 | 0.16, 0.01 |
| nudge15   | 0.35 |          4.74 | 13.54 |  0.35 |  1.00 |          0.35 |  0.03 | 0.35, 0.03 |
| nudge10_f | 0.88 |          7.68 |  8.73 |  0.88 |  1.00 |          0.90 |  0.04 | 0.88, 0.04 |
| lob40     | 0.91 |          6.67 |  7.33 |  6.67 |  7.33 |          0.92 |  0.92 | 0.91, 0.08 |
| lob50     | 1.42 |          6.52 |  4.59 |  6.52 |  4.59 |          1.43 |  1.43 | 1.42, 0.13 |
| lob60     | 2.04 |          6.52 |  3.20 |  6.52 |  3.20 |          2.06 |  2.06 | 2.04, 0.19 |
| overhand0 | 4.02 |          5.83 |  1.45 |  5.83 |  1.45 |          4.06 |  4.06 | 4.02, 0.33 |
| overhand  | 2.45 |          2.67 |  1.09 |  2.67 |  1.09 |          2.41 |  2.41 | 2.45, 0.22 |

The eight flicks: unchanged in every column (bullet 1.00x). throw_slowmo_test.sh (keyrate 1000), bullet time at real
speed and made slowly: the four throws the same as with vr_throw_slowmo_short_travel 0 (the old code path; their
strokes 0.33 to 0.60 m). The trade: a nudge made slowly with the world now goes as gently as it was made. In VR: in
bullet time nudge things up a little (wrist straight, and with a small flick): each should go about as at full speed;
slow lobs and overhands made with the slowed world as at full speed; try "Throws As In Real Time".

## Slaps whoosh (2026-10-08)

vrfiringrange_2026-10-08_10-48-14: slaps worked, but swung there was no sound at all, landing or not. A slap now
whooshes as a punch or a swing does (QC vr_melee.qc `VR_Melee_Whoosh`): once a motion, as fast as it would land
(`VR_Melee_LandNeed`, a punch's), with the same run-up, wrist and wiggle tests, for an open hand that slaps
(`VR_Melee_Slaps`: across, not out from the shoulder, not a shove's push), two frames in a row (one frame's speed is
no slap: the synthetic slow slap's 2 m/s read 4.1 where its 90 Hz frames beat against the 72 Hz server's). Its own
swish, lighter than the knights' blade swings the punches use: `vr/slap_whoosh1..2.wav` (make_sounds.py
`slap_whoosh`: band-passed air sweeping up to about 2 kHz as the hand passes and falling after it, a little finger
flutter, 0.26 s, peak 0.55), taken in turn. Its stroke event is a slap's ("stroke", the slap category). As a punch's
whoosh does, it wakes monsters (Slaps above: "woken only when it lands" no longer holds). `developer 1` prints
"melee sound: vr/slap_whooshN.wav (a slap's whoosh, speed, lands from)".

Checked (motion_synth.py takes, vr_motion_eval with developer 1): slap_forehand (9.9 m/s) and slap_backhand (20.0) whoosh
then slap; slap_forehand with the dummy 3 m off (no contact) whooshes; no_hit_slow_slap, no_hit_wave, no_hit_slow_punch
nothing; palm_shove_1h a shove, punch_straight a punch, neither a slap's whoosh. In VR: slap at the dummy and at the air
(a light swish each time, a blade's swish for a punch as before); wave and pat slowly: silent.
## vrtutorial2: the author's first VR play, fixed (2026-10-08)

His voice notes (11), each fixed in `Misc/quakevr/maps/vrtutorial2_gen.py`:
1. **Turned floor arrows** showed the next copy of the arrow in their corners (an axis-aligned square round a turned
   texture): a turned arrow is now a sheet turned with it (`mapgeom.hull`), half a unit inside its one copy.
2. **The locomotion settings** are one row of 8 at chest height (46; the upper of two rows took a jump), the board over
   them at 116.
3. **Lamps over windows:** room 3's upper wall lamps moved between the windows (x 3352). `check_fixtures()` now fails the
   build on any fitting, button, lamp or text board within 4 units of a courtyard window, or a board over a doorway (it
   also found room 7's lesson board hanging in its doorway from room 6: moved beside it).
4. **Wall lamps' sides** showed slices of the lamp texture: only the front is `tlight01` (fitted), the sides plain metal.
5. The turned arrow over pool B's edge removed: the swim's arrows start at the steps, pointing at the door.
6. **Wrist tip** `t2_wrist` where the fall lands in room 5: look at your wrist, the gadget shows your health.
7. Room 5's GRAB/FORCE GRAB board moved right of the exit door (it covered it).
8. **The force-grab shelf** is in the north-east corner, round its column, 64 up (was 100, 32 off the wall), a wall lamp
   and a fill light over it (the corner was dark).
9. **The darkness tips showed through closed doors** (a tip's sight test ignores doors): `t2_torch` waits for a trigger
   in the lit hall (`r9v_in`), `t2_flip` for one inside the course (`r9_in`). `vr_tips_test list`: from room 8
   "waiting for its trigger"; in the hall the torch tip live, the flip tip waiting; in the course both live.
10. **The dark course** (still pitch black): skirting, steel plate `metal4_4` to 80 (the blocks' sides), a rail, riveted
    panels `tech10_2` over it (was `twall2_1`, computers).
11. **The jump wall** 12 lower: the ladder block's top 144 -> 156, the ledge 100 over it (was 112). At a jump's top the
    hand at its natural 1.9 m pose is now 3 units over the lip (before, 9 under it).

New entities are written last (`with late():`) so the others keep their edict numbers.

**Tested:** `--preset final`, 0 holes in 1,000,064 rays, no qbsp warnings but the deathmatch one; the playthrough 38 of 38
(`--god`); `--from jump --ledge` (new): a jump alone stays on the block, a grip at a 2.2 m reach (72 over the block,
at the wall's face) holds nothing, the jump-and-catch climbs; softlock checks 9 of 9; e1m1 smoke; menu path checks
0 missing. Before/after views of each note: `scratch/vrtutorial2_fixes_sheet.png` (worktree vrtut2).

**The playtest's robustness** (any change to the map re-rolls its physics: the crate's card, the fights): a lying
weapon is taken at its origin (`take_lying_weapon`: its box is a 3-unit cube there; a point along its length,
"weapon 0.53", missed it), the rifle approached from the west of the fight, the arena's knocked-gun retake lets the
grip go first (a new press is what takes), and the walk out of the arena goes round the pit (it walked down its stairs).

**For VR:** the settings row's height; the corner shelf's sight line; the new wrist tip's wording; the jump wall (needs a
jump, not too hard now?); the dark course's new walls under the flashlight.
## Shadows fade, never pop: the vrstart brazier (2026-10-08)

**Report:** in vrstart, by the campaign terrace's west brazier (beside the Quake lectern), the brazier's shadow on the
floor (its bowl's octagon) popped in a few steps forward and out a few steps back.

**Cause (confirmed with `vr_debug_torch_lights`, setpos -1360 y 136 facing 78 degrees):** not the map lights. The
shadow is the brazier flame's own flickering light (`VR_TorchLights`, a dynamic light), and `vr_torch_light_shadows 4`
let the 4 *nearest* of the `vr_torch_lights 8` cast shadows, ranked by raw distance with no hysteresis among the chosen.
At y -360 the brazier (torch 4) was the 4th nearest; at y -400 the staircase's top torch (4110, 265 units away against
its 320) took its place, and the shadow switched off in one frame (the light itself kept lighting). Dynamic-light
shadows (`selectDlights`) had no fade at all: chosen or not, at once.

**Fix (vr_lighting.cpp, vr_emissive.cpp, vr_glsl.h):**
- Torch shadows by importance: the lit torch's reach over its distance (how large what it lights looks), the shadowed
  ones kept at least a second (x2) and until another is 30% more important.
- Every dynamic light's shadow has a strength (`DlightSlot::strength`, to the shaders as `gpulight_t.shadow2.y`, the
  share it lets through; both dlight shadow lookups `mix` towards 1): chosen or dropped, it fades over 0.4 s, keeping
  its atlas tile until faded (at most 2 lights past `vr_shadow_dlights`). A light that has just appeared (explosion,
  muzzle flash, a key new in its slot) has its shadow at once. The `vr_shadow_dlights` choice got the same hold (1 s,
  x2) and a 30% hysteresis (was 25%).
- Distance: every shadow fades out over the last 15% of `vr_shadow_distance` (dynamic and map lights).
- Map lights: fade 0.4 s (was 0.25 s), held 1 s, the score kept positive so the 1.3 factor favours the shown ones (a
  near-zero or negative light-at-viewer score made the factor useless).
- `vr_shadow_stats 2` (Graphics > Shadows > Shadow Statistics: "Each Frame, Per Light"): each frame, every shadowed
  light, + chosen / - fading, and its shadow's strength.

**Verified:** `Misc/quakevr/shadow_pop_test.py` (walks back and forth by the brazier, checks no strength changes faster
than a 0.4 s fade): short walk (y -280..-458, 3 times) PASS, 0 pops; long walk (to y -1002) 0 pops, the brazier's shadow
cross-fading with the stair torch's (5 tiles during the fade). Frame strip, static positions y -200..-400, before | after:
the brazier's shadow at y -400 is kept (floor under it 45.7 -> 41.6 mean). `vr_light_test` x32 in the range: 8 shadows,
steady at 1.0. Bench validate: lights_32, lights_32_noshadows, flashlight_e1m1, torches_32, explosions_storm, 0 failing.
e1m1 smoke clean.

**Map lights' cost** (new bench scenarios `maplights_vrstart` and `maplights_e2m1`, with `--settings` files; exclusive,
90 Hz paced, 2016 px eyes, 3 x 600 frames; GPU 3D ms):

| scene | 0 | 2 | 3 | 4 |
|---|---|---|---|---|
| vrstart terrace | 2.056 | 2.106 | 2.148 | 2.159 |
| e2m1 start | 1.532 | 1.618 | 1.618 | 1.618 |
| e1m1 start (idle_e1m1) | 1.757 | 1.812 | 1.841 | 1.842 |

2 -> 4 costs 0.05 ms at most (shadow maps 0.057 -> 0.063 ms, the rest the world shader's extra lights). The shipped
default is already 4 (`vr_defaults.cfg`; the compiled-in 2 and LIGHTING.md's table predate it): kept.
## Grenades back to the original models, smaller; the rocket in flight smaller; the super nailgun's receiver (2026-10-08)

The author's notes vrfiringrange_2026-10-08_10-33-00 .. 10-39-35.

- **The super nailgun's receiver turned out** (10-33-00: "flipped 180 degrees roll, so the receiver looks backwards ... it
  doesn't look properly attached"). Its collar went into the body, only its lip out on the face (the "sunk" well of
  23-58-30). make_mags.py now makes it that collar rolled 180 degrees about its seat: the lighter lip a flange flush on
  the face at the seat, the collar out along the magazine (0.98 units of it; 0.12 into the body), so the magazine goes
  into a receiver on the gun. The seat, the load point and the magazine's middle are unchanged (the well is drawn only).
  `ssg_checks.py snailwell` prints how far it goes into the body too: proud 1.01, over -0.16, into 0.12 (v_nail2;
  v_lava2 0.99, -0.16, 0.14); reload_test.sh section 11 checks proud 0.5..1.2, over <= 0, into > 0. Unchanged:
  `vr_reload_well_snail_*` (0: the author's are 0 too).
- **The grenades are Quake's own models again** (10-34-52 .. 10-37-30: "I want it to look exactly the same", the
  ammunition the same as what the launcher shoots). The pouches' grenade (either pouch: QC vr_grenade.qc
  `VR_HandGrenade_Make`) is `progs/grenade.mdl`, the multi-grenade `progs/mervup.mdl` (Dissolution of Eternity's, as the
  multi-grenade launcher shoots), the proximity grenade `progs/proxbomb.mdl` (Scourge of Armagon's), held, lying about,
  going into a launcher and in the ammo pouch, as in flight. Muted with the pin in (skin 1, make_grenade_skins.py:
  proxbomb.mdl has one now too, its red bands unlit), skin 0 once armed (the band lit, the smoke trail:
  `VR_GrenadeTrail` gives skin 1 of any of the three none). The rocket stays make_rounds.py's (10-39-35).
- **Their size: `vr_grenade_scale` 0.78** (Weapons > Reloading > Launchers, "Grenade Size"; 10-37-30: "around 25%
  smaller ... both in the ammunition and in the projectile form"). The three models are drawn at it wherever they are:
  the launchers' shots, the ogres' grenades, the pouches' grenades, the ammo pouch's (vr_props.cpp `size`/`drawnSize`:
  a model's own scale times its Held Object Offsets Size, so their Box3D bodies, hit models, held boxes and grip
  offsets follow; QC takes their boxes from `drawnbounds`: `VR_Grenade_Setup`'s touch box, `VR_GrenShot_Make`'s shot
  radius, `VR_Reload_RoundRef`'s butt). Measured (`developer 1` logs "grenade: <class> (<model>) drawn <size>" as each
  becomes shootable): the ammo pouch's grenade and the launcher's shot both 6.28 x 2.33 x 2.21 units (Quake's 8.05 x
  2.98 x 2.83 at 0.78); the multi-grenade 2.65 x 2.75 x 6.09; the proximity grenade 7.39 x 7.53 x 8.46.
- **The rocket in flight smaller: `vr_rocket_scale` 0.65** ("Rocket Size in Flight"; 10-39-35: "a little bit smaller
  so that it more closely matches the ammunition"): progs/missile.mdl (22.5 units long with its flame, its body 3
  across) drawn at 0.65: its body 1.95 across, the ammo pouch's rocket's 1.55. Every missile.mdl (the ogres' and the
  other monsters' rockets too); the multi-rockets (rockup.mdl, already small) unchanged.
- **Butt first** (the author's decision stands): the grenade's butt is its flat end (-x; its nose the tapered end, +x,
  out of the muzzle), the multi-grenade's its bottom (-z: mervup.mdl stands along its z, both ends alike; QC
  `VR_Reload_RoundAxis` takes its +z, `VR_Reload_RoundRef` its lowest z; the engine's `heldRoundRef` the same). The dull
  tap the wrong way round as before; the proximity grenade any way round.
- **The ammo pouch** (make_ammo_pouch.py, vr_view.cpp `setupAmmoPouchGrenades`): Quake's models are the user's game's,
  not ours to bake into the pouch's frames, so its frames 17-24 are now its full front with no grenades (the file
  0.62 MB, was 0.97) and the engine draws up to 3 grenades standing in it, nose up, muted, spaced evenly about its
  middle: the grenade at 0.9 and the multi-grenade at 0.8 of their size in the hand (whole, they went through its
  leather), the proximity grenades at 0.36 (Quake's ball is near the pouch's width). Frames, skins and counts as before
  (pouchgren_test.sh section 1 unchanged). Rockets as before.
- **Held**: the grenade and multi-grenade by their prop slots (4 and 5: the author's In the Palm fits, his grip offsets
  now scaled with the grenade), the proximity grenade by slot 56, now `progs/proxbomb.mdl` In the Palm, 1.2 kg as the
  others (it held vr_round_prox.mdl; vr_props_version 67). Quake's grenades are no LiveRound models: they get its
  leniencies as rounds (Held Things Collide, the grab slack, the self-collision exception: `modelmeta::isQuakeGrenade`),
  the grenade's Box3D rules (`isGrenade`: hard, Quake's bounce, the proximity grenade too), Quake's bounce sound (QC
  plays weapons/bounce.wav for them again; the engine's metal knock only for make_rounds.py's models), and are not
  pushed by an empty hand's body (vr_carry.qc, the proximity grenade too).
- **Kept to switch back**: make_rounds.py still makes `vr_round_grenade.mdl` (skin 1 the multi-grenade, 2 and 3
  armed) and `vr_round_prox.mdl` (skin 1 armed), in the repo; `make_ammo_pouch.py --generated-rounds` bakes them into
  the pouch's frames as before. To switch: point QC's `VR_RELOAD_GRENADE`, `_MULTI`, `_PROX` at them (and
  `VR_HandGrenade_Look` at their skins), empty vr_view.cpp's `pouchGrenades`, set `vr_grenade_scale` 1.
- **The multi-grenade from the ammo pouch** is made turned so that its z lies where the grenade's x would
  (`VR_HandGrenade_Make`: In the Palm keeps the turn it is taken at), so it comes out ready for the muzzle as the
  grenade does (front_test.sh's multi-grenade check failed without it: it lay across the barrel).
- Test aids: test steps 17/18 (held at the load point) turn the multi-grenade by its z; the `drawn` log line above.
## Guns in convex pieces; guns lying about load; held props hit with their shape (2026-10-08)

The author's notes vrfiringrange_2026-10-08_10-28-15 (a shell resting above the shotgun beside its receiver, as if on
air, unable to get into the port), 10-31-35 (no magazine or shell goes into a gun lying on a table or the floor: "the
weapon prop state and the held weapon state should provide most of the same functionality") and 10-42-18 (a held prop
popped a magazine off or opened the super shotgun by its box, not its shape).

### The guns' collision in convex pieces (vr_convex.cpp)
- **The cause**: each gun's Box3D body (held: its reach body; lying: its prop body) was one convex hull of the drawn
  model, which spans the air between the parts: over the shotgun's receiver from the hammer and rear sight to the
  barrel, across its port and the magazine wells. A round lying on it rested on that hull.
- **Now** (`vr_box3d_gun_pieces`, 12; VR Settings, Weapons Push Things' page: "Guns' Shape", One Hull / 6 / 12 / 20
  pieces): every gun (held, and lying as `thrown_weapon` / `weapon_*`) is a body of convex pieces that follow the drawn
  gun. `convex::decompose`: the model's triangles voxelised (cell 0.12 units, at most 200 cells along the gun), the
  outside flooded from the grid's border (the rest is the solid), each outside cell's distance from the solid (a two-pass
  chamfer). A piece is a box of model space; its hull is that of the triangles clipped to the box and of the solid's
  cells on its cut faces (the pieces meet across a cut). Its gap: the most its hull's surface (sampled a cell apart) lies
  off the solid. The piece with the largest gap is cut in two (along the model's axes, on a cell border) where the two
  halves' hulls hold the least volume: the three cuts each way whose halves' solid's boxes hold the least are shortlisted
  from the voxels, then their hulls made; until every gap is within 0.3 units or the pieces run out. Hulls are kept
  within Box3D's 128 edges (at most 44 vertices, fewer if it refuses: `convex::hull`).
- Made once a session per gun model and frame (`gunShapes`, a `mem::Cache`: made again on a game dir change or a model
  reload), the hulls once a map; the off hand's are the main hand's mirrored. A lying gun's pieces weigh what its one
  hull did (their density scaled). The swept swing (`sweepReach`) sweeps the pieces' hull and asks each piece for the
  nearest point; the arrays of a body's shapes are `maxBodyShapes` (32) long.
- `vr_physics_shapes` prints each gun's pieces: how many, how long they took, the most their hulls (and one hull) lie off
  the drawn gun, their volume; `thrown_weapon` added to Debug > Tests > Reloading's "Print the Collision Shapes".

- **Measured** (vr_physics_shapes, each held gun; units, a unit 3.2 cm): the most a hull's surface lies off the drawn gun,
  one hull -> 12 pieces: shotgun 2.25 -> 0.30, super shotgun 2.22 -> 0.36, nailgun 1.72 -> 0.57, super nailgun 1.16 ->
  1.15 (the grooves between its four barrels; its well is open: its load point was 0.16 inside the hull, now outside it),
  thunderbolt 2.11 -> 0.52. Volume (cubic units) one hull -> pieces (the voxelised gun): shotgun 159.6 -> 93.5 (94.0),
  nailgun 271 -> 136 (129), thunderbolt 315 -> 144 (144). The load point inside the body: shotgun 1.24 -> 0.06,
  thunderbolt 1.91 -> 0.04. Made in 35 to 56 ms a gun, once a session.
- **The note's case**: the shotgun turned port up, a shell dropped from 8 units above its load point: with the pieces it
  goes in with Loose Leniency 0 (it needed 3 cm with one hull: it rested on the hull over the port). The super shotgun
  open, muzzle down, a pair dropped into its barrels: 5 cm (3 with one hull: its body is the shut gun's, the barrels
  drawn turned down are not, and the pair now falls past where the hull was). The magazines dropped up into their wells:
  in at Leniency 0 with either body (the gun's radius and the magazine's own reach the seat). Loose Leniency stays 6 cm.
- **Performance** (gunshape_test.sh, exclusive: six guns lying in a heap, a held shotgun swept through them for 120
  frames): Box3D's step 0.052 ms a frame on average (worst 0.42) against one hull's 0.013 (0.07); the hands' reach
  sync 0.001 against 0.004.

### Guns lying about load as held ones do (vr_reload.qc VR_Reload_PropsFrame)
- Each frame (StartFrame) each round, loose or in a hand, against each gun lying about (`thrown_weapon`) that loads by
  hand: its load point is the engine's (`loadportof(e, ssgOpen)`: view::modelLoadPort, the held guns' table, through the
  prop's drawn transform, angles and origin; the super shotgun's turned down with its barrels when it lies open) into its
  .loadportpos/.loadportaxis/.loadportface. A round held within the gun's radius (a magazine's added) goes in (the
  wrong way round, full or shut: a dull tap, once); a loose one as by contact into a held gun (within the radius and the
  Loose Leniency, from the opening's side, lying within Loose Angle; within the leniency more it passes through the gun's
  body: `.vr_ammo_passgun`, the engine's preSolve). Shells into the shotgun and the open super shotgun, a magazine into
  an empty well (it clears QVR_WPNFLAG_NOMAG: the client draws it seated), a launcher's round into its muzzle. The rounds
  go into the weapon's record (`.weaponinst.wi_clip`): the hand that picks it up has them.
- Held-only still: the magazine's pull and B/Y's eject (a gun must be held to pull against, or its button pressed), the
  bump that knocks a magazine out and the hits that break the super shotgun open or shut (the other hand's blows at the
  gun one hand holds: a lying gun has no hand to move against), the flick, the ammo button's press (the hand's own gun's).
- Test steps (Debug > Tests > Reloading): `vr_reload_test 20` the off hand's gun let go of, 21 a round tossed into the
  nearest lying gun, 22 the main hand's round at its load point (it takes one from the pouch first: run it twice, grip
  held), 23 the lying guns' report, 24 a round lying sideways at it.

### A held prop hits with its shape (shapenearest)
- QC's `VR_Reload_BoxNearest` (the prop's box, square to the world) is gone: the magazine's bump (`VR_Reload_HitFrame`)
  and the super shotgun's hits (`VR_Reload_SsgHit`) ask `shapenearest(e, a, b)`: the point of its Box3D body (hull or
  pieces, by GJK against the segment) nearest the magazine's length or the barrels' front, a held one where the hand
  holds it this frame (its body follows at the step: a fast blow's frame asked where it was). Without a body, its box.
  Held magazines and rounds still count by their middle and top.
- Test step `vr_reload_test 25` prints both for the main hand's prop: the guard's head brought 6 units off the nailgun's
  magazine is 3.5 off it by its shape, 1.2 by its box (Hit Reach 2: the box's near miss, now no hit); brought 4 off, its
  shape is 1.7 off and it knocks the magazine out.

**Tested**: `Misc/quakevr/reload/gunshape_test.sh` (new, 14 checks, all pass: the pieces against one hull per gun; the
note's shell at Leniency 0, in with the pieces and out with one hull; lying guns: a round tossed into the shotgun,
nailgun, thunderbolt, open super shotgun and grenade launcher goes in, one held at the shotgun's, super nailgun's and
rocket launcher's load point goes in, one lying sideways stays out, the shut super shotgun taps; the Box3D step's time);
`contact_test.sh` 22 of 22, `front_test.sh` 32 of 32, `reload_test.sh` 93 of 93 (its held prop magazine check now brings
the head to 4 units, and a new check: brought to 6, its box within Hit Reach and its shape not, the magazine stays in).
QC 0 warnings; build, statics and FGD checks clean; menu path check 0 missing. eval.sh: no current melee takes.

**For VR:** a shell dropped or set on the shotgun's receiver falls into the port; nothing balances on air over a gun;
guns lying on a table: shells, magazines and launcher rounds by hand and tossed in; a held prop (a head) must now
really touch a magazine or the super shotgun's barrels to pop or open it.
**Open:** the super shotgun broken open keeps its shut body (its barrels drawn turned down have none of their own);
its pair dropped in needs 5 cm of leniency (3 with one hull).

## Teleporters: the shimmer fades out up close; the step of light at a crossing (2026-10-08)

The step of light left at a crossing ("A frame seen through after a teleporter": the room through the gate a little darker
than the room itself). **The shimmer over the view through a gate now fades out over the eye's last 30 cm to the gate's
plane** (`vr_teleporter_surface_fade` 0.3, metres; Graphics > Teleporters > Teleporter Stars > Fade Up Close; 0: it stays to the
end): its share over the view (0.12 times `vr_teleporter_surface_opacity`) times the eye's distance in front of the plane
over the fade's (LiquidShade, the eye's distance from LiquidPortal; `TeleportLook.z`, in units). Gone at the crossing.

Measured with `teleport_frames_test.sh` (it now prints each frame's mean luminance), the frame before the crossing
against the frame after:

| Case | Opacity | Before | After |
| --- | --- | --- | --- |
| vrteleporters' flush gate, walked into, 120 Hz | 1.0 | 79.1 -> 84.2 (a 6% step) | 79.4, 81.0, 83.1, 84.9 -> 84.2 (a ramp over the last 3 frames) |
| | 0.3 | 84.4 -> 84.2 | 84.5, 84.4, 84.9, 85.5, 86.0 -> 84.2 |
| start's middle gate, jumped into, 72 Hz | 1.0 | 20.1 -> 21.2 | 20.3 -> 21.2 |
| | 0.3 | 19.8 -> 21.2 | 19.8 -> 21.2 |

- At start the whole-frame step is mostly not the view through: the frame before still shows start's own darker floor
  below the gate's sill. The gate's part of the picture (its upper 60%) goes 27.5 -> 28.3, about the walk's own rise
  from frame to frame (+0.5).
- **What is left: the view through a gate is about 2.5% brighter than the room itself** (vrteleporters, the shimmer
  faded: 86.3 against 84.2 a frame later; the 0.3 shimmer had hidden it by dimming it as much). Found by turning
  things off one at a time (the step at the crossing, 2.1 with the defaults):

| Off | Step |
| --- | --- |
| `vr_tonemap 0` | none (89.2 -> 89.4) |
| `vr_light_contrast 1` | none (62.4 -> 62.6); 0.4 with `vr_specular 0.5` |
| `vr_normalmaps 0; vr_bloom 0` | none (78.9 -> 78.7) |
| `vr_normalmaps 0`, `vr_normalmap_baked 0` | 0.9 |
| `vr_specular 0`, `vr_deluxemap 0` | 0.9 |
| `vr_bloom 0` | 1.4 |
| `vr_specular 0.5` | 2.7 |
| `vid_fsaa 0` | 1.8 |
| no change: `r_dynamic 0`, `vr_shadow_dlights 0; vr_shadow_maplights 0`, `vr_ao_dynamic 0; vr_ao_brush 0`, `fog 0`, `vr_parallax 0`, `vr_relight_glows 0`, `vr_specular_aa 0`, `vr_ambient_light 0` | 2.1 |

  So the baked light through a gate comes out a little brighter where the contrast (`vr_light_contrast` 2) takes lamps
  above Quake's full light, its sheen from the deluxemaps (`vr_specular`) stronger, and bloom and the float scene's tone
  curve show it (with `vr_tonemap 0` it is clipped away). Mostly on the walls near the lamps (the picture's top: up to
  +17 there, nothing on the floor). Not the shimmer, the torches' or other dynamic lights, shadows, AO or fog. Its cause
  in the gate view's draw (both use the same world shader and frame constants) is not found yet: next, read the gate
  view's and the eye's float scenes over the same pixels (`vr_portals_shot` against `vr_eyeshot 2`).
## vrstart: lecterns for Dimension of the Machine and Dawn of the Machine (2026-10-08)

The author: "Add the missing lecterns to vrstart." MG1 and MG3 are ready (single player, `nativeReady`) but the hub
had no way in. The terrace now has six lecterns, two rows of three either side of the way to the gate: id's Quake,
Scourge of Armagon and Dissolution of Eternity on the left, the rerelease's Dimension of the Past, Dimension of the
Machine and Dawn of the Machine on the right (`vrstart_gen.py` `LECTERNS_X` -164 -106 -48 48 106 164 from the gate,
`LECTERN_HALF` 20: 40 wide under a 46-wide cap, were 52/60; the way to the gate stays 50 wide and the row ends
within the curb and short of the braziers, as the four did).

- **Selection**: the new lecterns run `vr_activestartpaknameidx 4` and `5` (SELECTED over them, as the others);
  `VR_CanChangeCampaignMap` already handled any index 3 and over (at a hub the teleporter's changelevel runs
  `vr_campaign_select mg1` / `mg3`, which starts the campaign's `start`); only comments changed in C++.
- **Unavailable**: QC buttons.qc's two copies of the check are one function, `button_campaign_unavailable` (1, 2 the
  mission packs; 3, 4, 5 `vr_dopa_status`, `vr_mg1_status`, `vr_mg3_status` != 1): "(unavailable)" under the label and
  the press refused ("Campaign unavailable...").
- **Map**: rebuilt (final preset): 0 holes in 1,000,064 rays.
- **Tested headless**: each new lectern pressed by the mock hand (`vr_debug_wallbuttons`: pressed by hand; its echo
  "... selected: step into the teleporter"; the map stays vrstart; Dimension of the Past's too); `vr_activestartpaknameidx
  4` / `5` then `changelevel start` (the teleporter's command): map start, `vr_campaign` 4 / 5, game
  `id1;hipnotic;rogue;mg1;quakevr` / `...mg3...`; the real teleporter walked into with 4 and with 5 (the selector ran,
  the campaign's start loaded); with `-nosteam -nogog -noepic` (no rerelease data) the three rerelease lecterns read
  "(unavailable)" and a press leaves the selector at 0; images of the row (owned, unowned, MG3 selected); the walk test
  18 of 18; `vr_menu_path_check maps/vrstart.map` 4 found, 0 missing.
- Test note: `setpos` always turns noclip on (no trigger touched until `noclip` again), and the teleporter's
  changelevel, like a button's command, is queued after the whole `-Script`: test the gate with `changelevel start`,
  or end the script without `quit` and read the log (`-Timeout`).

## Teleporters: the view through a gate as bright as the room it shows (2026-10-08)

The 2.5% left at a crossing (the section above: the room through a gate brighter than the room itself, on the lamp-lit
walls). **Cause: the gate's faces clipped the view through at 1.** They are translucent liquid faces (`r_telealpha`
times `vr_teleporter_surface_opacity`, under 1 whatever the opacity), drawn through Ironwail's order-independent
transparency (`r_oit` 1), whose output clamped every colour to 0..1 (`OIT_OUTPUT`, and its resolve's `LinearToGamma`).
The view through is a float scene (`vr_tonemap`) with lamp-lit walls up to about 2, as the room is when seen directly;
on the gate each channel was cut at 1, so an orange-lit wall turned yellower and, past the tone curve (which rolls the
brightest channel off keeping the hue), came out brighter than the same wall rolled off whole. Hence what made it go
away: `vr_tonemap 0` (all clipped at 1 anyway), `vr_light_contrast 1` (no lamps above 1), normal maps, sheen and bloom
off (less above 1). `r_oit 0` (blended, no clamp) shows it gone as well.

**Fixed**: the world's and the liquids' shaders keep up to `SceneTone.x` in the transparency pass too (`OIT_MAX`; the
models' and sprites' shaders keep 1 as before), and the resolve no longer clamps above (a unorm target, the window's,
clamps by itself). Same cost. Translucent water, slime and lava lit above 1 now roll off in the eyes as the opaque
world does (lava's glow above 1 was meant, the shader's comment says so; it was being clipped).

**Measured**, the gate view's float scene against the eye's (`vr_portals_shot` now also writes a `.pfm`, and prints the
camera the view was drawn from; Debug > Teleporters > The View Through A Gate, Read Back), vrteleporters' flush gate, the
eye 10 units in front, the same frame:

| | Gate face / view, wall pixels 0.75-1 | 1-1.5 | Above 1 on the face |
| --- | --- | --- | --- |
| Before | 0.86 (clipped) | (none) | 0 pixels |
| After (and `r_oit 0` before) | 0.962 | 0.979 | 80 000 pixels |

(0.96: the shimmer's share at that distance, the same over the whole range.) `teleport_frames_test.sh`, mean luminance
of the frames before the crossing -> the one after:

| Case | Before | After |
| --- | --- | --- |
| flush, 120 Hz, opacity 0.3 | 84.5, 84.4, 84.9, 85.5, 86.0 -> 84.2 | 82.6, 83.0, 83.5, 83.9 -> 84.2 |
| flush, 120 Hz, opacity 1.0 | 79.4, 81.0, 83.1, 84.9 -> 84.2 | 78.2, 79.6, 81.4, 83.0 -> 84.2 |
| start, 72 Hz, 0.3 / 1.0 | 19.8 / 20.3 -> 21.2 | 20.2 / 20.3 -> 21.2 (the gate's part 27.5 -> 28.3, as before) |

No overshoot any more: the frames before rise to the room's level as the shimmer fades out (`vr_teleporter_surface_fade`)
and the last step is the fade's own (at opacity 1.0 its 12% still over the last frame, 1.2).

Also: `teleporter_edges_test.sh`'s eye images (head, recursion) are read from the kit's base for the agent (the game's
`eyeshots/`), not the worktree's `quakevr/`, which does not exist ("No such file"); now all its sections pass (recursion
2 counted a 2-pixel speck as a fourth sign: `teleporter_signs.py` ignores runs a row tall).

- [ ] Walk into a teleporter whose room is lit by lamps (vrteleporters' flush gate, the start map's): no flash of brighter
  walls at the crossing; the walls through the gate look as they do once through.
- [ ] Translucent water near a bright lamp and lava seen through water: nothing turns white or blotchy.

## Explosion debris as Box3D bodies (2026-10-08)

The author's decision: the explosions' incandescent chunks become the server's physics objects, as the rocks and bricks
lying about are (vr_debris.cpp), instead of the client's own models moved by seven line traces a step each (which never
slept: PERF_DECISIONS.md item 3).

- **Where they come from:** every explosion QuakeC broadcasts (its temp entity `TE_EXPLOSION`, `TE_EXPLOSION2` or
  `TE_TAREXPLOSION`: rockets, grenades, explosive boxes, the ogres', the tarbabies', the mission packs') is seen whole as it
  is written (vr_server.cpp `VR_BroadcastWritten`, the broadcast's temp-entity parser) and launches its chunks at the
  server frame's end (`explosiondebris::serverFrame`). The client no longer makes any (the particle preset and the three
  `VR_*Explosion` hooks lost their spawns); `vr_explosion_debris_test` (the menu's Preview an Explosion, the benchmarks)
  draws the explosion on the client and queues a listen server's chunks.
- **What they are:** entities of `progs/vr_explosion_debris.mdl` (precached by QC `VR_Debris_Precache`), `.vr_rigid`
  `MOVETYPE_BOUNCE` `SOLID_NOT` props with their own mass (`.vr_prop_mass` 0.02 kg) and a serial (`.vr_xdebris`). Box3D
  (vr_box3d.cpp `isChunk`, `addChunkShape`) gives each a sphere as wide as the chunk (its box), bouncing as
  `vr_explosion_debris_bounce`, with rolling resistance; continuous collision against the world always, and as a bullet
  (against doors, lifts and props) while it moves more than a third of its size a step (the props' rule); asleep at
  rest (the small props' threshold, 0.15 m/s). Its category `catChunk`: it meets the level, doors, lifts, monsters,
  players, props and corpses, never the hands or what they hold; a contact with one makes no sound (physsound: none), no
  touch and no impact (`chunkPair`: a chunk at 9 m/s would knock a crate's sound or set off an explosive box's hard
  hit), and no hit box along its flight (`touchNearby` skipped). So they ride lifts and plats, later blasts throw them
  (`physicsblast`), they knock props without moving them (20 g into a 6 kg health box: it stays where it was).
- **The same launch:** a random direction biased up (`vr_explosion_debris_up`), speed, size and life between the
  settings' ends, a random spin (±600 degrees/s an axis), out along its way to a free point for a blast at a wall (or
  not that chunk). Size: the entity's `scale` (a byte on the wire, the sixteenths the client's drawn scale was); the fade:
  its `alpha` over its last half second (a third of a short life), then the engine frees it (SUB_Remove a second later
  only if the list lost it: a saved game's are found again at its first frame). At most `vr_explosion_debris_max` (96,
  hard cap 256), the oldest retired first, never leaving fewer than 512 free entities; in multiplayer at most
  `vr_explosion_debris_mp_max` (new, 24; -1 as single player, 0 none: `vr_debris_mp_max`'s semantics; MULTIPLAYER.md).
  The settings keep their meaning; count, speeds, up, sizes, lives, bounce and the maxima are the server's now (the
  host's), trail and lights the client's.
- **Drawn:** ordinary entities, interpolated as any. The fire trail is drawn each frame from where the chunk was drawn
  last to where it is drawn now (`VR_ExplosionDebrisTrail`, called by `CL_RelinkEntities` for an entity without a trail
  of Quake's, before its `trailorg` moves on), so it is smooth at the render rate; the nearest
  `vr_explosion_debris_lights` glow. How hot a chunk is comes from its age against the life settings' middle (the client
  doesn't know its life) and its alpha once it fades.
- **Tests:** `Misc/quakevr/explosion_debris_test.sh <agent>` (all PASS): the cap (ten explosions of 12 under a cap of 40
  leave 40, 80 retired) and the life (none 5.5 s later); multiplayer (`maxplayers 4`: 10, -1 gives 40, 0 none); three
  dropped on vrtesthall's floor asleep within 2 s; 1900 units/s into the south and west panels and down onto the table,
  and into vrclimb's lift side-on (a kinematic body): each stays on its near side; one on vrclimb's lift rises with it
  (41 -> 121 in 10 s), one on its plat goes down 40 with it; a blast throws resting ones; chunks into a health box leave
  it where it was. A saved game keeps its chunks (found again at load, then they end). A real QuakeC explosion (an explosive box set off by `vr_physics_blast`) launches its chunks. Debug >
  Tests > Physics Stress: Explosion Debris Ahead, Explosion Debris List (`vr_explosion_debris_list`: each chunk's place,
  speed, resting or moving and on what); `vr_explosion_debris_launch <x y z> <vx vy vz> [size] [life]` for one chunk.
- **Measured** (`bench.sh` explosions_storm, combat_48, ai_crowd_64, combined; the base build and this one interleaved,
  3 rounds each, medians, ms a frame; two A/B sets, other agents building in between, so whole-frame times are noisy and
  the phases are the numbers to read). explosions_storm (18 explosions a second, the pool full at 96): the server
  0.23 -> 0.40 (SV_Physics 0.19 -> 0.34), the client's view entities 0.34 -> 0.15 (the traces gone), CPU busy p50 1.63
  -> 1.64: about even; entities 256 -> 350, Box3D bodies awake 29 -> 123. Box3D's step alone (`vr_physics_steptime`, e1m1,
  96 chunks in the air): 0.017 -> 0.14 ms (108 awake). combat_48: server +0.09..0.18, view entities -0.06..-0.10;
  ai_crowd_64 (the ogres' grenades among 64 monsters): server +0.13..0.30; combined (no chunks: its `vr_physics_blast`s'
  explosion messages are written by a console command and cleared before the frame sends them, so neither build made
  chunks there): unchanged. Traces a frame in the storm: 65 -> 68 (91 before a launch in the open took one trace instead
  of eight). The local client's datagram in the storm: 87 -> 2560 B a frame (96 chunks, about 26 B each while they live:
  no baseline); a remote client's is capped by `vr_explosion_debris_mp_max` (24: about 630 B of its 1400).
- **A future option (not done):** a separate client-only Box3D world for purely visual physics: these chunks, the spent
  shell casings, sparks, small gore. It would be built from `cl.worldmodel` (in single player its mesh shared read-only
  with the server world's, which builds the same one), with kinematic proxies for the brush entities (doors and lifts at
  their client entities' interpolated places) and for the server's props near the view. Effects there need no networking
  (no entity slots, no datagram bytes, no multiplayer cap) and behave the same in single player and multiplayer; the price
  is a second broadphase and step on the client, and props that never feel them (one-way).
## QuakeC's scans through an index (2026-10-08)

The author asked whether builtins could speed up the QuakeC that shows in profiles (`VR_EnemyShove_Target`,
`VR_Burn_MapFrame`, `VR_Grapple_WorldFrame`, `VR_Burn_NailFrame`...). Measured with the new per-function timer
(`vr_qcprofile`, `profile_qc`: TESTING.md) on combined, combat_48, ai_crowd_64, explosions_storm and secret2 awake:
most of those functions are one loop, `for(e = find(world, classname, "x"); e; e = find(e, classname, "x"))` or
`findflags(...)`, and their cost is the walk of every edict (an edict is ~5 KB with Quake VR's fields: each step a
new page; secret2's ~1800 edicts walked ~50 times a frame).

**The edict index** (`Quake/vr/vr_edictindex.cpp`, `vr_edictindex` 1): the same builtins, the same answers, without
the walk; no QuakeC changed (it stays the policy).
- `find(start, classname, s)`: a bitset of edicts per classname text; the next set bit after `start`. A classname
  whose string's text can change without a store (a temp string, a zoned one, an engine pointer; only strings of the
  progs or made once by `PR_AllocString`, the map's and saves', count as fixed) puts its edict in a "volatile" set
  compared at the search with find()'s own strcmp, in edict order with the rest.
- `findflags(start, field, flags)` on .flags (all but the bits the engine sets in C: on ground, partial ground,
  water jump, god, notarget, jump released) and on `wt_state`, `stl_notice`, `vr_letgo_fall`, `vr_throw_self`,
  `MG_registered` (fields no engine code writes): a bitset per bit. Any other field or bit walks as before.
- Kept exact as edicts change: QuakeC stores into a field through OP_ADDRESS then OP_STOREP, so OP_ADDRESS of a watched
  field records the address (`qcvm->fieldwatch`) and the STOREP into it marks the edict to be read again (after the
  store: a search between the two still sees the old value, as the walk would). An address never stored into is read
  at the end of the outermost call. The engine's changes: an edict freed or taken, cleared, parsed from a map or a
  save, a client's fields cleared, Box3D's spawned props' classnames (`VR_EdictIndex_Touch`); a load, a map or the
  progs: everything read again at the next search. Not covered (none in the QuakeC): a pointer to a watched field
  kept past the call that took it.
- Check: `vr_edictindex_verify 1` walks as well on every search and prints `vr_edictindex ERROR` on a difference.
  0 differences in 839,217 searches of secret2's fight, and in stealth_tests.sh all (47 PASS), mapflameburn_test.sh
  (all PASS), reload_test.sh (67 passed), the wall torch shot/grab tests, an enemy shove and a grapple reel (same
  output as `vr_edictindex 0`).
- Costs, `profile_qc` medians of 2 (timer on, exclusive):

| scenario | find + findflags (ms a frame) | QuakeC a frame |
|---|---|---|
| mg3_secret2_awake | 0.324 -> 0.025 | 1.09 -> 0.74 |
| combined | 0.030 -> 0.006 | 0.49 -> 0.41 |
| combat_48 | 0.020 -> 0.005 | 0.25 -> 0.24 |
| ai_crowd_64 | 0.023 -> 0.007 | (fight noise) |
| explosions_storm | 0.016 -> 0.004 | 0.15 -> 0.14 |

  secret2's top functions, own time: `VR_Stealth_LookAbout` 0.140 -> 0.062, `VR_EnemyShove_Target` 0.040,
  `VR_Reload_NextRound` 0.035, `VR_Throw_SelfFrame` 0.028, `VR_Burn_MapFrame` 0.023, `MG_WorldFrame` 0.016 -> under
  0.002; `VR_Stealth_Frame` 0.026 -> 0.004, `VR_Liquids_Frame` 0.031 -> 0.017. Without the timer the server phase
  4.44 -> 3.39 ms (medians of 4, noisy: each fight differs).
- **`findflagsinview`** (a builtin; builtins.qc): after the index, `VR_Stealth_LookAbout`'s cost was its QuakeC
  test of each lit torch and body (box centre, `vlen`, `normalize(...) * v_forward`, ~135 a call). The builtin steps
  through findflags() and skips those out of range or out of the cone itself, in QuakeC's float steps (OP_ADD_V,
  OP_MUL_VF, OP_SUB_V, PF_vlen's doubles, PF_normalize's, OP_MUL_V; no FP contraction), so the same entities reach
  the QuakeC's own tests (`wt_fire`, the monster itself, an old body) and its trace. `vr_edictindex_verify 1` also runs
  QuakeC's old loop beside it (`VR_Stealth_InView`) and prints `vr_edictindex ERROR` on a difference: none on secret2's
  fight nor in stealth_tests.sh all (50 PASS). secret2: `VR_Stealth_LookAbout` with its callees 0.062 -> 0.010 ms.
- Left (PERF_DECISIONS.md, 12): the force grab's `findportalcone` (0.13 ms on secret2), `findradius`, the engine's
  traces and steps, and `VR_Prop_Flung`'s unprinted `dprint(sprintf())` (0.034 ms: a behaviour question).
- Debug > Profiling and Memory: Edict Index, Verify Edict Index, Edict Index Stats.

## Crouching: a smaller box (2026-10-08)

Your note (vrteleporters_2026-10-08_14-46-51): crouching in real life should let you walk through a small teleporter
and any low opening; the box stayed standing. Now crouched (your eyes under 36 units over your feet; half crouches 44
and 52 too), your box is that tall against the map, bodies and shots, and you keep it until there is room to stand.
Monsters' bullets aim lower at you crouched. The details: HULLS.md, "Crouching".

- Engine: `Quake/vr/vr_hull.cpp` (the crouched heights, `updateCrouch`, `moveBox`/`entBox`/`touchBox`/`hitBox`/
  `playerBoxFits`, the trees compiled at load, `vr_crouch_status`), `vr_server.cpp` (each move; `.vrbits0` bit 23),
  `vr_cvars.inc` (`vr_crouch_hull` 1, `vr_crouch_height` 36, `vr_crouch_step` 8, `vr_crouch_test`), `vr_menu.cpp`
  (Movement > Player Hitbox > Crouching; Debug > Tests > Crouch Shots, Crouch Status).
- QuakeC: `vr_defs.qc` (`QVR_VRBITS0_CROUCHED`), `vr_crates.qc` (`VR_Crate_ShotAim`: no higher than 60% of
  `vr_crouch_height`), `vr_crouch_test.qc` (`vr_crouch_test <n>`).
- Map: vrteleporters' crouching room east of the hub (`make_vrteleporters_map.py`): a 40-high tunnel and a 48-high gap
  through a wall, cover 32 and 48 high with a monster panel beyond, and a 48x48 teleporter pair (east wall <-> south
  wall, either side of the tunnel). 0 holes.
- Numbers (`crouch_test.sh`, mock headset 1.7 m standing, 1.3 m half, 1.0 m crouched): the tunnel stops you standing at
  its mouth (x 552) and lets you through crouched (x 1014); with `vr_crouch_hull 0` crouched stops too. The 48 gap:
  standing stops, half crouched (box 44) through. Standing up inside the tunnel keeps the 36 box (standfits 0), standing
  once walked out. The small teleporter: standing stops at 1144, crouched carried (out of the south wall's). Halfway in,
  standing up gives the 44 box. A grunt beyond the 32-high cover, 12 bullets: crouched behind it 0 damage (aim 21.6
  units over your feet), standing 48 (aim 35.7), crouched in the open 36. A crouched 20 s random walk on e1m1: stuck 0,
  embedded 0. Load: three more compiled trees (e2m2 25-31 ms each on the pool, 428 KB each).
- To try in VR: the crouching room (signs from the hub's east door): crouch into the tunnel, stand up inside (you stay
  low until out), the gap with half a crouch, the small teleporters, a grunt from the panel while you crouch behind the
  low wall. Whether 36 for the lowest box (eyes about 1.1 m for a 1.7 m eye height) and the steps feel right.
- Found on the way (not the crouch's): a player put straddling a teleporter's plane by `setpos` falls through it
  uncarried when he walks on, standing too (vrteleporters' flush player gate).
## The gadget's side button: gear lights; bullet time from a screen tap (2026-10-08)

Vittorio (map1_2026-10-08_14-07-47, vrfiringrange_2026-10-08_14-40-16): bullet time fired by accident (melee, incidental
contact with the wrist); the side button should be a light switch for stealth instead.

- **Bullet time: only a hard tap on the gadget's screen** (vr_bullettime.cpp, `tapScreen`). The striking point is the
  other hand's middle (`hands::palmPoint`) or, holding a gun, its butt (`view::heldWeaponButt`: the middle of the drawn
  points within a unit of its rearmost end along the muzzle-handle line; `vr_bullettime_tap_butt 1`). It must be over
  the screen (`vr_bullettime_tap_margin` 2 cm past its edges), come straight into it (`vr_bullettime_tap_angle` 40
  degrees off its normal at most) at `vr_bullettime_tap_speed` 1.2 m/s or more (into the screen, against the screen's
  own point under it: the gadget arm's velocity and turn subtracted), then stop on it (within `vr_bullettime_tap_depth`
  6 cm of its face, its speed into it down to `vr_bullettime_tap_stop` of the peak within `vr_bullettime_tap_window`).
  Velocities are taken in real time (`timescale::handScale`), so a tap stops bullet time at the same force it starts
  it. The wrist tap (`vr_bullettime_tap_radius`) and the button's bullet time (`vr_bullettime_button*`) are gone.
- **Gear lights** (vr_gearlights.cpp): the same button (the inner one on the lower edge) pressed by the other hand's
  fingertip (`vr_gadget_button_reach` 4 cm ahead of the hand) toggles `vr_gear_lights` (archived) with a click
  (`vr/gadget_click_on.wav`, `_off.wav`, from `make_gadget_sounds.py`: make_flashlight.py's click, higher and lighter)
  and a tick in both hands; bindable `vr_gear_lights_toggle`. Off, over 0.15 s: the gadget's two lights and the ammo
  screens' spot lights and the screens' glows at `vr_gear_lights_dim` (0.05), the screens' faces (the gadget's palette,
  the ammo screens' text and ink, the hologram) at `vr_gear_lights_screen_dim` (0.35). The flashlight keeps its switch;
  the lava nailguns' glow is lava, not gear, and is left. The stealth AI's light on the player (`stealth::lightAt`)
  counts every dynamic light but the flashlight's, so the dim reaches it with no change there.
- **The button's hit volume**: a sphere of `vr_gadget_button_size` (3 cm) round the button's middle, moved by
  `vr_gadget_button_x/y/z` (cm along the screen's right, up, out), cut by a plane half a radius behind its middle (a
  fingertip over the screen never presses it), and no press while a screen tap is under way or half a second after one
  (`bullettime::tapping`). A press is the fingertip's arrival; it re-arms 1.5 radii out; `vr_gadget_button_cooldown`
  (0.6 s) ignores a press after one. `vr_debug_gadget_button` (Debug > Show Gadget Button, and on the Screens page): 1
  draws it (green ready, yellow pressed, red cooling down) and the fingertip, prints presses; 2 also the screen tap's
  zone. `vr_gear_lights_info` (Debug > Gear Lights Info): the state, the button, the stealth light on you and the
  lights within reach of you.
- Menu: HUD and Menus > Screens > Gear Lights: the Gadget's Side Button (every setting above); Combat > Bullet Time >
  Screen Tap; the Activation row's choices lose "tap only"/"button only".
- Mock: `vr_mock_hand_to <hand> screen|screenbutt <cm over> [<cm across>]`; `vr_mock_hand_glide <s>` glides a
  `vr_mock_hand_to` move over that long and reports its exact velocity (stepped moves report the fast-mode frames'
  velocities, and the body's settling moves the gadget a few cm after a step).
- **Tests** (`Misc/quakevr/gadget_tap_test.sh <agent>`): a hard straight tap by the hand (3.2 m/s) and by the gun's butt
  (3.2 m/s) and a 31-degree strike (2.3 m/s) start or stop it; a soft touch (0.23 m/s), a firm one under the threshold
  (0.93 m/s), a swing across 3 cm over the screen (10 m/s), one stopping on it (8 m/s) and a 50-degree strike (3.1 m/s)
  don't. The button: pressed (off), pressed again at once (ignored, cooling down), after the cooldown (on). The stealth
  light with the gadget raised to be read: 103.71 (its dynamic share 0.71) on, 103.04 (0.04) off: the gear's lights
  are a small share of what the stealth AI sees (128 is full light; you are unseen at or below 20). The gadget's
  screen texture: mean 44.6 -> 21.9, brightest 255 -> 109.
- To try in VR: is a deliberate tap easy (Tap Force 1.2 m/s, Straightness 40 degrees), and do melee swings, blocks and
  two-handed holds never start it? Is the button easy to find and press without the screen tap pressing it? Is the dim
  right (Lights When Dimmed 0.05, Screens When Dimmed 0.35)?

## Bullet time's distortion trails (2026-10-08)

Vittorio (vrfiringrange_2026-10-08_14-24-40): in bullet time, F.E.A.R.'s distorted trails behind bullets, nails and
other projectiles, a refraction ribbon fading along its length; on by default, tweakable, in stereo, cheap.

- `vr_bttrails.cpp`: a trail is the places its projectile passed and when (real time, the trails' own clock from
  `VR_AdvanceTime`). Entities: `VR_DistortionTrail` from `CL_RelinkEntities` (after `VR_ProjectileLight`), by model:
  nails (`progs/spike.mdl`, `s_spike.mdl`, new modelmeta ids `Spike`, `SSpike`; `lspike`, `lasrspik`), rockets
  (`EF_ROCKET`; Chthon's lava balls too), grenades (`EF_GRENADE` and Quake's grenade models, not a hand grenade with its
  pin in: `VR_GrenadeTrail`), monsters' (`EF_TRACER*`, `EF_ZOMGIB`, `laser.mdl`). A trail starts only for one flying
  at 200 units/s or more of the game's time (its last two messages), ends when it is not relinked, and a new one
  starts after a jump of over 256 units (a teleporter) or a new model. Hitscan: `vr_weaponfx.cpp parseTracer` hands each
  pellet over before the tracers' chance (tracer drawn or not), its head flying from the muzzle (a grunt's from his
  gun's) at the tracers' speed in `cl.time`, as the tracer does. A grunt's or enforcer's bullet is a monster's.
- Drawing: after the heat haze (`VR_DrawHeatHaze` calls it; the haze's scene copy is now `haze::copyScene`, shared).
  The ribbons are made once a frame (on the first eye, facing it; the spectator camera makes its own): pieces of at
  most 24 units, cut at the trail's life and length, narrow at the projectile and widening over 28 units, wider with
  age; faded in over 10 units behind the projectile, out with age, out within 40 units of the eye (a shot of yours
  starts there) and where it is seen end on. The fragment shader bends what is behind as a rippling glass rod: a shift
  in the world across the ribbon (drawn in to its core, plus ripples anchored to the trail's odometer), projected in
  each eye, so both eyes see the same bend; not from what is in front of it (the scene's distances); blended in at its
  edges. Depth tested, no depth write; drawn before the tracers, so the tracer stays sharp. None in views through a
  teleporter (`portals::viewing`). Foveation shades it as the scene.
- Only in bullet time, easing in over 0.15 s and out over `vr_bullettime_trails_fade` (0.6 s) after it ends; trails
  already there keep following their projectiles while it fades. Not with the recording's slow motion.
- Cvars (Combat > Bullet Time > Distortion Trails): `vr_bullettime_trails` 1 (0 off, 2 always),
  `_strength` 1.25, `_life` 0.7 s, `_length` 4 m, `_width` 20 cm (rockets and grenades x2, monsters' x1.5), `_fade`
  0.6 s, `_hitscan`, `_nails`, `_explosives`, `_enemy` 1. Debug: Distortion Trails Test (`vr_bullettime_trails_test
  [count] [m/s] [distance]`: shots across the view), Distortion Trails (`vr_bullettime_trails_list`).
- Verified (mock, 1024 eyes, paused for same-frame A/B with `_strength 0`): test shots change 1.54% of the left eye's
  pixels and 1.56% of the right's; real missiles (`vr_physics_fire` nails, rockets, a laser, a grenade) 1.55% / 1.59%;
  the diffs sit on the same trails in both eyes. Fades out 0.77 -> 0.06 -> 0 (trails gone) over 40 frames after bullet
  time ends; `vr_bullettime_trails 0` clears them.
- Cost (2048 eyes, 90 Hz paced, exclusive, `vr_profile`; the trails run inside the "heat haze" pass):
  `bullettime_trails_64` (new scenario: the pool of 64 full, across the view) 0.024 ms GPU avg (0.16 max), 0.010 ms
  CPU; `combat_48_bullettime` 0.121 ms GPU with trails vs 0.041 without (the ogres' explosions' haze), so +0.08 ms
  (max 0.23 vs 0.12), CPU +0.004 ms; `explosions_storm_bullettime` (new: no projectiles) nothing. Off: the pass does
  not run. (`bench.sh compare` of the same runs moved unrelated passes 10-28% between the two labels: run-to-run GPU
  state, not the trails.)
## Player ragdolls: the Death View (2026-10-08)

- **Death View** (`vr_death_view`, default 1 Third Person; VR Settings > Comfort): dying whole (not gibbed: health not
  under -40), the player leaves a ragdoll body of his own (QC `vr_deathdoll.qc` `VR_DeathDoll_Make`, from PlayerDie
  before the death hop). 0 Off: as before. 1 Third Person: the view stays where the eyes were (no death hop now), the
  body there to look at, its head on. 2 Immersive (`vr_deathview.cpp`): the view in the body's head bone (a point 0.7 of
  the way from its pivot to its end: the eyes), drawn headless for you (`ragdoll::hideHeadOf`); your own head's motion
  since is added (turned with the view), its place smoothed (`vr_death_view_smooth` 0.15 s), its yaw following the
  head's (`vr_death_view_turn` 1, at most `vr_death_view_turn_speed` 60 deg/s; the head's forward plus its crown
  flattened, held when that is short: no spikes when the face looks up), never pitch or roll; a fade from black going
  in and on respawning (`vr_death_view_fade` 0.5 s); the hands, the gear and the hand's status bar not drawn. A body gone
  (gibbed, retired) leaves the view where it was until respawn.
- **The body**: `progs/player.mdl` in the pose he was in, `FL_MONSTER`, health -1, a corpse at once (`VR_Corpse_Arm`):
  the engine makes it a ragdoll at once (a corpse lying still: `wantsRagdoll`), gibbed by enough damage as a grunt's
  corpse (`VR_Corpse_Parts`: `h_player.mdl`). Its motion: his, with the killing blow's push (T_Damage's, damage x 8,
  recorded in `.vr_deathdoll_push`) times `vr_death_ragdoll_push` (1), at most 400 units/s (a rocket's push was
  2000+). A blast throws it too (vr_box3d.cpp's recent blasts). It plays the player's death animation for the clients
  that draw no ragdoll. Its `.vr_deathdoll` is its player; the player's `.vr_deathdoll_body` it (cleared in
  PutClientInServer). Respawning in coop or deathmatch, no copy to the body queue (the ragdoll is the body).
  VR_Knockdown_MakeRoom retires a dead player's own body only once he is up.
- **The rig** (vr_ragdoll.cpp `playerSeeds`, Misc/quakevr/ragdoll/player_bones.json; rig.py `LOOSE_PIECES`): 11 bones
  derived from Quake VR's player.mdl (733 vertices, 144 frames; rest pose $axrun1). His axe and gun are pieces of their
  own, each in his hands or on his back by the frame: `SeedTable::looseWeapons` makes them loose and `Rig::hideLoose`
  hides them in every ragdoll (clusters 0.69, bones 0.82 units rms; 25 ms).
- **Multiplayer**: the dead player is not sent to the other clients while his body lies there (sv_main.c,
  `VR_SV_HiddenFromOthers`): they see the body. Ragdolls are drawn by a listen server only, as the monsters': a remote
  client sees the body's death animation, and his Immersive view is Third Person's. Saving while dead: not handled.
- **Tests** (headless, e1m1): Off: no body, not hidden. Third Person, impulse 196 (Die Now, Blown Back, new): body 203 a
  ragdoll of 11 parts, the head drawn, pushed back (pelvis 455 against 465 still). Immersive: in the body's head (the
  view 1.0 unit from its eyes: half the eyes' spacing), head hidden, turned 75-80 degrees as it rolled. Impulse 197 (Die
  Now, Gibbed, new): no body. Coop, Immersive, respawn: health 100, the view normal, not hidden, the body left lying.
  Debug > Cheats: Die Now Blown Back, Die Now Gibbed, Death View Status (`vr_death_view_status`).
## The menus' Ko-fi link and corner boxes (2026-10-08)

- **The version box** (`Quake/vr/vr_menubrand.cpp`, VR_MenuDrawVersion): the bottom right corner's "Quake VR: Unleashed -
  v0.9" and "by Vittorio Romeo" now sit in a rounded box like the status box's (the same border and fill, 4 true pixels
  from the canvas's right and bottom edges, the lines 4 inside it), with a third row a little lower: Ko-fi's cup
  (`quakevr/gfx/vr/kofi_symbol.png`, the installer's `Assets/kofi_symbol.png` byte for byte, drawn in its own colours)
  and "Support on Ko-fi". The box is as wide as its longest line; the menus keep clear of it as they kept clear of the
  label (versionLabelClearance: now the whole box, 3 rows).
- **The link:** lit like the corner's buttons (highlight edge and fill, text white) under the laser (a haptic tick when
  the laser gets to it) or the desktop mouse once it has moved over the menus; it takes its row and on to the canvas's
  right and bottom edges. A press (the trigger's K_MOUSE1, a left click; down only, not repeats) plays misc/menu2.wav,
  pulses the hand, and opens https://ko-fi.com/vittorioromeovee in the desktop's browser (SDL_OpenURL); for 3 s the
  row reads "Opened on your desktop" (headset) or "Opened in your browser" (flat). A second press within 1.5 s is taken
  and does nothing (a double click, both triggers): the page opens once. The menu's own selection stays (the mouse on
  it is like on a corner button: VR_MenuMouseOnButtons), and clicking it is checked before the corner buttons' keys, so
  it works with the corner buttons off (vr_menu_flat_shortcuts 0).
- **Tests:** `vr_menu_link_dryrun 1` (Debug > Tools > Menu Links: Print, Do Not Open) prints `menu link: opening <url>
  (<count>) (dry run: no browser)` instead of opening the browser; the kit's hidden runs (QVR_TEST_HIDDEN) never open it
  either. `vr_mock_laser kofi`, `vr_mock_mouse kofi [click]` point at it; `menu_vr pos` prints `version link "<text>" at
  x y, takes ..., (lit), opened <n>`. Verified (mock headset, then VR off): laser on it lit, trigger pressed once and
  then again at once: one `opening` line (1) and the row reads "Opened on your desktop"; flat: the mouse on it lit, two
  clicks at once: one more line (2); a click elsewhere still opens Single Player.
- **The flat screen's row of icons** (vr_menuui.cpp, ToolbarLayout::rowCorner) is now 4 true pixels below the canvas's
  top (was 1), as the status box and the version box are from their edges; it ends at 16, clear of the lists' titles at
  16:10 and 4:3 (Levels, Options checked). The headset's column was 4 from the top already.
- **The cup slanted and wrapped (fixed):** the engine's CPU mipmapper (gl_texmgr.c, TexMgr_MipMapW) averaged a level
  as one stream of pixel pairs, so an odd width (the cup is 321 x 258) started each next-level row half a texel further
  on: every mip level was sheared (the left edge moved ~65 of 321 texels top to bottom, the handle wrapping round to
  the left), and at ~10 px the menus draw only those levels. Odd widths and heights now take an exact box filter row
  by row (TexMgr_MipMapOdd); even sizes are untouched (bit for bit). The cup is the only shipped image with an odd
  size; a mod's odd-sized external textures were sheared the same way and are fixed too.
## Out of the water by hand; Dawn of the Machine's armour shards as armour; the author's settings (2026-10-08)

- **Jump Out of Water** (map1_13-55-35; `vr_water_jump`, Swimming page, default 1: Quake's). Quake lifts a swimmer
  at the surface out of the water when there is solid at his waist ahead and room at eye level (QC `CheckWaterJump`).
  Off, it doesn't: the player climbs out with the hands. A lip of the ledge map is climbing's own (vr_climb.cpp: take
  it, pull, mantle); a bank that is no ledge (a slope, a rock: the ledge map wants a 32-unit drop) climbs by
  `VR_WaterClimb_HandPull`: an empty hand gripping with ground (normal z 0.7 or more) right under it, pulled down at
  0.3 m/s or more, lifts the player as Quake's jump does, towards that hand and high enough to clear the top under it.
  Only while climbing is on (`vr_climb`): with it off, Quake's jump out stays, or a pool with high sides would have no
  way out. Bots keep Quake's. Kept on by default: off is a harder game (a bank the hands can't take means swimming to
  another way out), and Quake's lift is what every map's pools were made for. `developer 1` prints `waterjump:` lines.
  Tests (mock, `vr_campaign_native mg3`, map1's pool at `setpos 64 1000 -160 0 90 0`, stick forward, jump held to stay
  at the surface; its north lip 24 units over the water): on, out onto the lip (origin z -104); off, held at the wall
  in the water (y 1080, z -147); off with `vr_climb 0`, out (Quake's). Off, both hands taking the lip and pulling down
  0.3 m or 0.8 m: climbing's mantle, onto the lip (60 1110 -104). map1's rock bank by `-280 2232 -214` (swim north): on,
  out; off, stays; the hand pull fires there (`waterjump: climbed out by hand`) but the rock overhangs the water, and
  only Quake's way along it (west, with the stick) gets over it.
- **Armour shards as armour** (map1_14-01-08). Dawn of the Machine's `item_armor_shard` is an armour object now
  (`VR_PickupObj_Mark(QVR_PICKUPOBJ_ARMOUR)`, with `vr_armor_wear`): it floats until a hand takes or knocks it or a force
  grab pulls it, then it is a physics object; walking over it no longer takes it. Let go of over the torso (as armour),
  or at a holster or a pouch (`VR_Armor_AtTorso`: a shard also at `VR_Carry_AtHolster`), it is taken: its own pickup,
  5 armour up to 200 (green when none is worn). At 200 it is not taken ("Your armor is full"; `VR_Armor_IsBetter`: short
  of 200). Drawn at the pickups' size (`vr_pickup_scale`), not the armour's half size: its box is 2 x 7 x 10 units.
  Tests (mock, `vr_campaign_native mg3`, map1, `setpos -224 -30 56 0 90 0`, `vr_mock_hand_to main nearest item_armor_shard 4`, grip): taken by the main hand and let go
  of at the right hip holster, the ammo pouch and the chest: armour 5 each; let go of in front: dropped, armour 0;
  walked through (the row, stick forward): armour 0. `vr_mg3_test 9` (map1): 5 passed, among them "a shard is an armour
  object" (force-grabbable, wearable, the object's hand touch) and 27 shards adding 5 each.
- **The author's tweaks are the defaults** (vrfiringrange_14-16-19, "I tweaked quite a few settings and hotspots"; his
  config of 15:40 against a `resetall; writeconfig` dump of the shipped defaults). Config version 104
  (`vr_cvars.cpp` defaultChanges: a config still holding the old default takes the new one):
  `vr_enemygun_spent_crackle` 1 (was 2.5), `vr_enemygun_spent_volume` 0.5 (0.25), `vr_snd_pitch_jitter` 10 (4),
  `vr_ssg_fire_anim_speed` 1.75 (1.4: the super shotgun's firing animation 0.34 s), `vr_stealth_corpses` 700 (600),
  `vr_stealth_meter_time` 1 (1.5), `vr_stealth_run_speed` 280 (250), `vr_stealth_torch` 500 (400),
  `vr_teleporter_surface_opacity` 0.5 (0.3, vr_defaults.cfg), `vr_reload_port_shot_radius` 1.6 (1.5). Weapon settings
  version 38: the Super Axe (slot 24, `vr_wofs_*_25`; its glowing twin inherits it) Offset Z -3.92 (0.046) and its
  first hotspot at 0.238 -1.952 -1.670, turned 4.40 / 3.50 / 2.74 (1.217 -2.025 1.354, 0 0 0). Held object settings
  version 68: the grenade (slot 4, progs/grenade.mdl) Grip X -0.3, Grip Y -1.1 (0, 0), Overlap 0.2 cm (0.75). Each where
  the config still holds the old value. Left as they are: bookkeeping (`vr_cfg_version`, `vr_wofs_version`,
  `vr_props_version`, `vr_bindings_version`, `vr_xr_runtime`, `vr_tutorial_started`), his body (`vr_height_calibration`,
  `vr_bodycal_*`, `vr_body_elbow_back/hand/lift`), the motion recorder's (`vr_motion_*`), the menus (`vr_menu_positions`,
  `_level`, `_scale`, `_distance`), the desktop window and engine screen (`vr_window_view`, `vr_spectator_fov`,
  `vr_mirror_hide_hud_text`, `fov`, `contrast`, `gamma`, `sensitivity`, `scr_*scale`, `scr_centerprintbg`,
  `scr_menubgstyle`, `vid_*`), performance (`vr_foveated` 2), comfort (`vr_comfort_vignette_strength` 0.5), the stick's
  dead zone (`vr_deadzone` 10: his controllers'; 25 is safer for worn sticks), Ironwail's `sv_gameplayfix_random` 0,
  slider noise (`vr_ammo_pouch_scale` 0.999, `_x` 3.021975, `vr_melee_phase_speed` 3.996, `_time` 0.34965,
  `vr_relight_strength` 1.1988), props slots 33 and 57-63 given ids (crowbar, sword, nailguns, shotgun, lightning gun,
  axe) with every other key at its default (a slot taken when he held them: nothing to ship), and three settings this
  build doesn't have (`vr_throw_slowmo_long_travel` 0.2, `_short_travel` 0.15, `_real_strength` 0: another branch's).
  Tests: his config with these at their old defaults, and the kit's baseline (config 34), each come out at the new
  values after a map loads (config 104, weapons 38, props 68); stealth_tests.sh all: 50 PASS, 0 FAIL.
## Holding enemies (2026-10-08)

Vittorio (vrfiringrange_2026-10-08_14-36-52): hold on to living enemies with your hands. Experimental, on by default
(`vr_foegrab 1`; Combat > Holding Enemies). Engine `Quake/vr/vr_foegrab.cpp` (the holds), QC `QC/vr_foegrab.qc` (the
hold by kind of enemy, the touch's wake, the feel), `QC/vr_enemyshove.qc` (shoves while held), `QC/vr_melee.qc` (no
blows from a holding hand).

- **Taking hold.** An empty hand (no weapon, carried thing, force grab, flashlight; not gripping at a holster or pouch;
  not on a ledge: the climb comes first) grips while its fist (the jointed hand's spheres, and the palm's middle) touches
  a living monster's model as drawn (precise hits' model: `vr_hit_precise` must be on), within `vr_foegrab_leniency`
  (1.5 cm), or is in it (25 cm deep at most). A grip pressed up to 0.2 s before the touch still takes hold (reaching
  while closing the hand). The spot taken is the model's point nearest the touching sphere (`hitmodel::nearest`).
  Taking hold is a touch: a sleeping or idle monster spots you and turns hostile (`VR_Stealth_Spot`, with or without the
  stealth AI), and the hand buzzes.
- **The spot.** It moves with the monster's body (its origin and yaw) and with its animation eased (0.12 s, at most
  20 cm from the body's place): a limb's swing is followed, a pose's jump glides. The drawn hand is put with its palm on
  it (`STAT_QVR_FOEGRAB*`, eased on in 0.05 s and off in 0.12 s), its controller's turn kept.
- **The hold.** One hand's hold by kind (`VR_FoeGrab_Size` from the grapple's masses, a vore and a super wrath counted
  huge): `vr_foegrab_strength_small` 0.85 (grunt, enforcer, zombie, knight, mummy, dog, scrag...), `_medium` 0.25 (fiend,
  ogre, ...), `_large` 0.04 (shambler, vore and bigger); or by mass (`vr_foegrab_by_mass 1`: full at
  `vr_foegrab_mass_light` 100 kg, none at `_mass_heavy` 650). Two hands: 1 - (1 - a)(1 - b) (a grunt: 0.98).
- **Its effect** (each server frame's end, before the precise hits keep the poses): the monster's own flat movement
  since the last frame (its steps, a slide, a leap) and its turning are cut by `vr_foegrab_slow` (0.8) times the hold;
  the hands pull their spots towards them, flat, at `vr_foegrab_drag` (4/s) times the hold, at most
  `vr_foegrab_drag_speed` (60 units/s). Moved as a monster steps (`SV_movestep`: up steps, never off a ledge).
- **Letting go:** the grip let go; the tracked palm more than `vr_foegrab_break` (35 cm) from the held spot for 0.25 s,
  or twice that at once; the monster dead, knocked down, gone or moved over 64 units in a frame; the hand busy with
  something else; the option off.
- **Shoves** (`vr_foegrab_shove`): 0 Resisted, a held grunt's or enforcer's shove's push is cut by
  `vr_foegrab_shove_resist` (0.9) times the hold, and the player's slide carries the held monster along (so the hold
  survives it); 1 Breaks Free, the shove pushes fully and every hand holding it lets go (`.vr_foegrab_letgo`).
- **QuakeC:** the holding hands' grips are hidden from the QC (`.vrbits0`'s grab bits, as the climb's) from the take to
  the grip's release; `.vr_foegrab_hands` (player) names the holding hands, `.vr_foegrab_hold` (monster) how firmly it
  is held.

Tests (headless, `vrfiringrange`, a grunt 44 units to the left, its first step taken, the main hand reaching in with
the grip pressed; `vr_foegrab_walk_test 110 0.8` walks the nearest monster away from you for 0.8 s):

| case | moved (of 88 units) | notes |
| --- | --- | --- |
| option off | 78.4 | nothing taken ("grips: off") |
| grunt, one hand (0.85) | 8.7 | stretch 16 cm; the hand moved 30 units away: let go (104 cm) |
| grunt, two hands (0.98) | 3.4 | |
| shambler (0.04), 110 units/s for 1 s | | let go after 0.33 s, pulled 1 m away |
| shove, Resisted | | push cut 76%: 30 units instead of 128; still held after (stretch 13 cm) |
| shove, Breaks Free | | pushed 128 units, "it broke free" |

Debug > Tests > Holding Enemies: Who Is Held? (`vr_foegrab_status`, the drawn hands too), Walk the Nearest Away, Log
Holds (`vr_foegrab_debug` 1 holds taken and let go and why, a grip that found none and how near; 2 each frame; 3 the
steps). Saved games keep no holds (the fields cleared on load).

**The training dummy** (vrfiringrange_2026-10-08_22-28-01, "null function" on grabbing it): taking hold spotted
the held enemy (`VR_Stealth_Spot` -> FoundTarget -> HuntTarget), which set the dummy's think to its `th_run`, which it
has none of: the next think ran a null function. The touch's wake now skips the dummy and anything without a
`th_run`. The dummy is held like any enemy, the hand on its model as `vr_dummy_type` has it (an ogre's: hold 0.25), and
is never moved by the hold (it stands on its spot: `vr_dummy_think` put it back each 0.05 s, a jitter). A new type
chosen while held is a new entity: the hand lets go. Tested: a grunt dummy and an ogre dummy held, no error.

**The hold moves them now** (vrfiringrange_2026-10-08_22-31-08: "barely moving them even at the highest settings").
The pull was only a spring from the spot to the hand: walking back holding a grunt, the hand got ahead of it faster than
the spring brought it, and past `vr_foegrab_break` (his 20 cm) the hand let go after a quarter of a second. A held enemy
now also follows its hands' own move since the last frame (flat; a pull, the player walking off with it) by
`vr_foegrab_follow` (1, Combat > Holding Enemies > Follow the Hand) times its hold, at most 480 units/s; the spring
takes up the rest (`vr_foegrab_drag` 4 -> 10/s, `_drag_speed` 60 -> 200 units/s). A shove's slide's carry is now
the part the follow leaves (1 - follow). Headless, `vrfiringrange`, held by the main hand, walking back 0.5 s
(`vr_mock_stick off 0 -1`):

| case | the enemy moved | the hold |
| --- | --- | --- |
| grunt, before (defaults) | 4.5 units | let go after 0.60 s (75 cm) |
| grunt, before (his: drag 20, speed 300, break 20) | 17 units | let go after 0.22 s (42 cm) |
| grunt, now (either) | 208 units, with the player | held (stretch 0.3 cm) |
| ogre (hold 0.25), now | 14 units | let go after 0.32 s |

**The two-hand throw** (vrfiringrange_2026-10-08_22-31-08: "a judo throw"; `vr_foegrab_throw 1`, Combat > Holding
Enemies > Two-Hand Throw). Both hands holding one enemy, turned over hard, knock it down that way: the shove's
knockdown (`VR_Knockdown_Start`: it falls as a ragdoll, lies, gets up), for sure, never by chance; both hands let go
(a thump in each). One hand never throws (it holds: slows and pulls). Engine `throwCheck` (vr_foegrab.cpp), QC
`QC/vr_foegrab_throw.qc`.

- **The turn** (the hands' own velocities, not the player's walk; metres, m/s): two parts added. The hands turning it
  between them as a wheel, (d x dv) / |d|^2 (d from the off palm to the main one, dv their velocities' difference: one
  shoulder pushed down, the other pulled up), and both together toppling it about its box's middle, (r x v) / |r|^2 (r
  to the palms' middle, v their mean velocity: its top pulled towards you or pushed to a side); each length at least
  10 cm. Its level part only (a twist about the vertical tips nothing over), in degrees/s: at `vr_foegrab_throw_twist`
  (150) or more, with the hands' mean speed at `vr_foegrab_throw_speed` (1 m/s) or more, after both hands have held
  0.15 s, it throws. Its direction: the turn's axis x up (where its top goes). Straight away from you (within 45
  degrees) is a shove's and does nothing, unless `vr_foegrab_throw_away 1`. A turn that didn't throw waits 0.5 s.
- **Who** (his tiers): `vr_foegrab_throw_always` (grunt, enforcer, zombie, knight, rottweiler, mummy, and the word
  `infected`: Dawn of the Machine's infected, `.vr_mg3_infected`); `vr_foegrab_throw_when_hurt` (death knight, the
  ogres: `monster_ogre`, `_marksman`, `_rocket`; fiend, ranged knight): only below
  `vr_foegrab_throw_hurt` (0.4) of its full health, never above; any other kind never (shambler, vore, bosses...;
  spawn, slime and scorpion too since 2026-10-09, his call: they have no knockdown get-up; vr_cfg_version 114 moves a
  config still holding the old list), nor anything flying or swimming. The lists are classnames (console). Or by mass
  (`vr_foegrab_throw_by_mass 1`: always up to `_mass_always` 140 kg, when hurt up to `_mass_hurt` 300, never heavier;
  vores, overlords, spawns, slimes and centroids never).
  Thrown along the turn at `vr_foegrab_throw_push` (220 units/s, times `vr_knockdown_push`) and `_lift` (100) up. A
  kind with no knockdown set up (its get-up: `.vr_kd_chance_h`), no ragdoll or no room can't be thrown now (a short
  low buzz in both hands, as for one too strong).
- **The training dummy** is never knocked down (it stands): over its head "thrown", "hurt it below 40%" or "never
  thrown", by the kind it stands as (`vr_dummy_type`) and its health bar, and the console prints the turn and speed of
  every try, for practising the motion.
- **Tuning:** `vr_foegrab_debug 1` (Debug > Tests > Holding Enemies > Log Holds) prints each try and, when both hands'
  hold ends, its hardest turn and the hands' speed then; 2 every frame's turn with each palm and velocity. `Who Is
  Held?` shows the hardest turn so far. `vr_foegrab_hurt <share>` (Hurt the Held to 30%, Heal the Held) sets the held
  (else the nearest) enemy's health.

Tests (headless, `vrfiringrange`, the monster spawned 44-60 units ahead facing you, `notarget`, both hands gripping
its chest, then a `vr_mock_play` of the hands' move: 30 cm in 0.15 s, 2 m/s; `scratch/throwtest.py` in the agent's
worktree):

| case | turn, hands' speed | result |
| --- | --- | --- |
| grunt, main hand up and off hand down | 505 deg/s, 2 m/s | thrown to your left (ended 87 units left), both hands let go |
| grunt, the other way | 511 deg/s | thrown to your right |
| grunt, both pulled back towards you (and a little down) | 188 deg/s, 2.1 m/s | thrown towards you |
| grunt, both pushed straight ahead | 171 deg/s | no throw (away from you: a shove's) |
| grunt, the left twist over 1 s (0.3 m/s) | 77 deg/s at its hardest | no throw |
| grunt, the left twist with one hand holding | | no throw, held |
| grunt, `vr_foegrab_throw 0` | | no throw, held |
| grunt, by mass | 504 deg/s | thrown |
| knight, left / right | 511 / 505 deg/s | thrown (to the right: ended 27 units right) |
| ogre at full health (by kind; by mass) | 505 / 511 deg/s | not hurt enough, held |
| ogre at 30% (`vr_foegrab_hurt 0.3`) | 511 deg/s | thrown to your left |
| shambler at 10% (hands on its arms; leniency 50 cm to reach) | 200 deg/s | never thrown (its kind) |
| training dummy as a grunt | 841 deg/s | "thrown" shown, hands let go, it stands |

**The throw topples it over its feet** (his note, 2026-10-09: "pushed towards the throw direction but stays upright. It
should spin towards the throw direction with the feet as a pivot, like a sweep"). Thrown (not a shove: its knockdown is
unchanged), the ragdoll just made is turned over about its feet (engine `box3d::ragdollTopple`, called by vr_foegrab.cpp
`throwDown`): its feet are the parts in the lowest quarter of its height, the pivot their middle on the floor; every part
gets `vr_foegrab_throw_topple` (120 deg/s) about the level axis through the pivot across the throw (w x r: the head along
the throw, the feet nothing), and the throw's launch (push and lift) shared out by height (the feet none, the top all:
the push itself becomes a turn about the feet). The feet are held (level motion zeroed each frame) for
`vr_foegrab_throw_topple_hold` (0.3 s). The hands' twist about the vertical spins it on its middle, times
`vr_foegrab_throw_spin` (0.5; at most 720 deg/s). Topple 0: pushed whole as before. Combat > Holding Enemies > Topple,
Feet Held, Twist Spin. Test: `vr_foegrab_throw_test [way 0 left, 1 right, 2 towards you, 3 away] [twist deg/s]` throws
the nearest monster as the hands would and traces its fall (torso lean pelvis-to-head from upright, feet moved, head
along the throw; also after a real throw with `vr_foegrab_debug 1`); Debug > Tests > Holding Enemies > Throw the
Nearest Left / Right / at You.

| (firing range, 60 units ahead) | lean at 0.1 s | at 0.5 s | feet moved (most) | lies |
| --- | --- | --- | --- | --- |
| grunt, before (topple 0) | 28 deg (its pose's; slid 24 units upright) | 98 | 90 units | towards the throw |
| grunt, topple 120 | 57 | 87 (110 at 0.2 s) | 7.5 | towards the throw, head 29 units past the feet |
| knight, before / after | 9 / 36 | 60 / 88 | 93 / 8 | towards the throw |
| ogre at 30%, before / after | 26 / 55 | 96 / 117 | 93 / 11 | towards the throw |
| grunt, 400 deg/s twist | | 87 | 7.9 | spun 200 deg/s as it fell |

(topple 240: 72 deg at 0.1 s, 133 at 0.2: rather a slam.)

**Knocked-down enemies struggle, visibly** (his note, 2026-10-09: "knocked-down ragdolls don't move";
`vr_knockdown_wiggle` 1, `_frequency` 2.2 Hz, `_pause` 0). Why nothing moved: the drive (box3d `feedRagdoll`) turned
only the chest and the head, 5 degrees either way, with at most 2.7 N m (1.8 times the joints' friction, 1.5 N m):
too weak to lift anything lying on the floor; the chest sat 3.85 degrees off its rest, still (measured: every part's
mean spin 0.008-0.014 rad/s from 0.4 to 1.8 s down, its deflection 3.84-3.86 degrees throughout). Now every joint is
driven about the pose it settled in: the chest curls 12 degrees, the head nods 18, the arms and legs kick 30 (a hinge
about its axis, a ball across its bone), each at its own pace (0.75-1.25 times the frequency, its own phase); a drive of
its inertia (a rod, m L^2 / 3) times a 3 Hz spring, near critically damped, at most 1.8 times the joints' friction plus
1.5 times its weight's lever (it lifts a limb off the floor); the parent takes the opposite torque. A pose more than
the swing plus 25 degrees off its rest (a throw, a hand) is its new rest. Measured (`vr_knockdown_test 9`, a grunt down
5 s, every 0.28 s from 1.1 s): mean spin 2.1-3.6 rad/s, the joints 21-24 degrees off rest, the parts moving 0.7-1.3
units on average per sample (most 1.5-2.5); with `vr_knockdown_wiggle 0` the parts settle to 0.00 within 1.4 s.
`vr_knockdown_test 9` now also prints `strugglemotion` (the parts' movement since its last call).

**A knocked-down enemy loses its head and limbs to the hands' blows** (his note, 2026-10-09: decapitation and
dismemberment didn't work on a knocked-down grunt; a dead one's ragdoll was easy). Why: the hands' blows never struck a
knocked-down monster at all. It is not solid (`SOLID_NOT_BUT_TOUCHABLE`): the blows' traces pass it (no
`MOVE_HITGIBS`), their box search (`VR_Melee_Sweep`) took only solid things and loose gibs, and the corpse blows
(`VR_Corpse_StrikeFrame`, with their own beheading) take only the dead and dying. Shots struck it (they trace with
`MOVE_HITGIBS`). Now the box search (`VR_Melee_BoxTarget`, vr_melee.qc) takes a living knocked-down monster too, its
ragdoll struck as drawn (vr_hit_precise), and the blow is a live one's (`T_Damage_VRMelee`): beheaded or a limb cut by
the same rules as standing (the blade, its speed, the head and neck zone on the pose mapped to the standing model, a
kill needed for a head as standing). Test: `vr_decap_test 61` (Debug > Gore Tests, Axe Swept Through Its Neck): the
axe's blade swept through the nearest live monster's neck as the blows' search finds what it strikes, then the blow at
health 1. Before: standing struck and beheaded; knocked down "the sweep misses". After: both struck and beheaded (the
knocked-down head thrown from the floor, 23 units up). `vr_limb_test 4` (a killing slash at a forearm) cuts the limb
standing and knocked down; `vr_decap_test 7` (not killing) beheads neither.
| training dummy as an ogre (full health) | 842 deg/s | not hurt enough |

Seen: the knight thrown to the left ended 26 units towards the player and 7 to the right, lying (its ragdoll; the
grunt's went 87 units left): worth a look in VR whether a knight falls the way it is thrown.

Seen while testing (not changed): a monster spawned by `vr_physics_spawn` (or `impulse 244`) stands 15-16 units lower
than the floor its first step (SV_movestep) puts it on, in e1m1 and vrfiringrange alike.

**A hand holding an enemy force grabs nothing** (his note vrfiringrange_2026-10-09_10-55: the judo throw's actions
overlap the force grab's, he pulled things by accident). The QC never sees a holding hand's grip (masked, so it picks
nothing else up), so to the force grab it was an open hand: its trigger locked on to whatever it pointed at and the
throw's swing was a flick. Now (`VR_Forcegrab_HandFrame`, vr_wpnforcegrab.qc) a hand holding an enemy
(`.vr_foegrab_hands`) has no force grab target: not while it holds, not for 0.5 s after it lets go (the throw's
follow-through; `cVR_Forcegrab_AfterFoe`), and not until its trigger, if held, is let go (the pull's rearm). Test
(`scratch/fg1.sh`: vrfiringrange, a health box 200 units ahead and a grunt 44, the main hand aimed at the box and
gripping the grunt, `+attack`, `vr_debug_hands 1`): holding with the trigger held, target none; let go with it still
held, none for the second after; pressed again, target the box, locked. With `vr_foegrab 0` (the grip takes nothing):
let go of the grip with the trigger held, locked on the box at once (14 ms).

**The judo throw sweeps the feet** (his note vrfiringrange_2026-10-09_10-56: in place of "feet stay put", a slider for
the feet's speed, opposite the topple: thrown left, the feet go a little right, so it spins in place in the air).
`vr_foegrab_throw_topple_hold` is gone (a retired name: an old config's line is dropped quietly); in its place
`vr_foegrab_throw_feet_speed` (60 units/s; Combat > Holding Enemies > Feet Speed, 0-300, to 1000). `box3d::ragdollTopple`:
the feet (the lowest quarter's parts) are held at that speed back against the throw (level; they may rise) until the
body has turned a quarter (at most 0.5 s), then fly on; the whole turns at topple + feet speed / height about a pivot
raised above the feet to match (feet speed / turn, at most half its height), so its top goes as fast as before and the
turn is faster. 0: the feet held still 0.5 s (his Feet Held), the turn about the floor under them, as before.
Measured (`scratch/topple_sweep.sh`: vrfiringrange, a grunt 60 units ahead, `vr_foegrab_throw_test 0`, his topple 300,
lift 170, push 190; the trace now prints the feet's move along the throw and up):

| feet speed | turn set | lean at 0.1 / 0.2 / 0.3 / 0.5 s | feet along the throw at 0.2 / 0.5 s (up at 0.2) | feet moved, most |
| --- | --- | --- | --- | --- |
| 0 | 300 deg/s | 72 / 138 / 171 / 131 | +9.2 / +9.0 (25.6) | (held, then dragged) |
| 40 | 348 | 70 / 128 / 162 / 95 | -0.3 / -6.9 (24.4) | 19.3 |
| 60 (default) | 373 | 72 / 131 / 165 / 100 | -4.1 / -13.9 (24.7) | 30.7 |
| 80 | 396 | 75 / 135 / 167 / 105 | -7.9 / -21.4 (25.2) | 41.7 |
| 120 | 444 | 82 / 145 / 166 / 102 | -15.1 / -35.4 (26.2) | 60.3 |

At topple 120 (the old default), feet 0 / 80: lean 47 / 59 at 0.1 s, 94 / 117 at 0.2 s; feet +5.0 / -9.6 at 0.2 s. An
ogre at 30%, feet 0 / 60: lean 70 / 71 at 0.1 s; feet +10.3 / -3.1 at 0.2 s, +13.9 / -5.6 at 0.5 s. (Held for 0.5 s
whatever the turn, the feet kept going back after it had turned over: 80 units/s took them 30 units back by 0.5 s and
lay it head 18 units past them; held to the quarter turn, 21 units.)

**The judo throw's grunt** (his note vrfiringrange_2026-10-09_10-56). A throw that knocks the enemy down (or the
training dummy's "would be thrown") plays the player's grunt on his voice channel: `vr_foegrab_throw_grunt` (volume,
0.6; 0 off), `vr_foegrab_throw_grunt_sound` (2, the jump's grunt deeper: an effort, told apart from the mantle's pain
grunt) from the mantle's list (vr_climb.cpp `climb::grunt`, `climb::gruntSample`; the menus' `gruntChoices`, one list
for both pickers). Combat > Holding Enemies > Throw Grunt, Throw Grunt Sound, Hear Throw Grunt
(`vr_foegrab_throw_grunt_test`). Test (`vr_foegrab_debug 1`, `vr_foegrab_throw_test`): "throw grunt
vr/derived/plyrjmp8_low.wav 0.60", with sound 4 "player/pain2.wav", with volume 0 none; the mantle's grunt and its
test command unchanged (the same code).

**The author's settings of the morning of 2026-10-09 are the defaults** (his note vrfiringrange_2026-10-09_10-56: "I've
tweaked values for the judo throws and the struggling effect; they feel much better"). His config of 11:10 against
the shipped defaults (vr_cvars.inc with vr_defaults.cfg over it); a value not in the evening's lists (above, "The
author's settings of the evening of 2026-10-08") is one he changed since. Config version 109 (`vr_cvars.cpp`
defaultChanges: a config still holding the old default takes the new one):

- The two-hand throw: `vr_foegrab_throw_lift` 170 (was 100), `_push` 190 (220), `_speed` 2.5 (1: the hands' mean
  speed to throw), `_spin` 0.55 (0.5), `_topple` 300 (120), `_twist` 140 (150). His Feet Held 0.5 (0.3) is the still
  feet's hold (Feet Speed 0), above.
- The knocked-down struggle: `vr_knockdown_wiggle` 1.5 (1), `_frequency` 1.5 (2.2 Hz), `_pause` 0.5 (0: bursts);
  the time down `vr_knockdown_time_min` 1.75 (1.25, vr_defaults.cfg), `_time_max` 3.5 (2.5, vr_defaults.cfg).
- Bullet time's screen tap: `vr_bullettime_tap_angle` 10 (40 degrees), `_depth` 6.5 (6), `_height` 0.75 (1), `_margin`
  1 (2), `_speed` 0.5 (1.2 m/s), `_stop` 1 (0.5: the impact on arrival), `_window` 0.5 (0.25), `_z` -3.5 (0); its
  trails: `vr_bullettime_trails_fade` 1 (0.6), `_length` 15 (4 m), `_life` 2 (0.7 s), `_width` 8 (20 cm).
- Barrels: `vr_crates_barrels` 0.15 (0.3), `vr_crates_barrel_lying` 0.15 (0.3).
- The gadget's side button: `vr_gadget_button_cooldown` 0.2 (0.6), `_size` 1.25 (3 cm), `_y` 0.5 (0).
- Front reloading: `vr_reload_front_angle` 40.5 (45), `vr_reload_front_hold_pitch` 45 (90: the round tipped 45
  degrees in the fist, not upright).

Left as they are: `vr_death_view` 1 (third person; Immersive, 2, has been the default since config 106: a mode, not a
tuning), `vr_xr_runtime` 1 (his runtime; Auto is the new default), the foe grab's `vr_foegrab_break` 20, `_drag` 20,
`_drag_speed` 300, `_leniency` 1 (left out last evening as settings turned up while testing it broken, and not the
throw's), and the evening's lists (bookkeeping, body, motion recorder, menus, desktop window, `vr_foveated`,
`vr_comfort_vignette_strength`, slider noise, props slots). Weapon and held object settings not compared this time.
Tests that measure against the old thresholds set them: `gadget_tap_test.sh` (the tap's eight; with his values the 0.93
m/s touch taps and the 31-degree diagonal doesn't), `reload/front_test.sh` (angle 45, hold pitch 90). The bench's
trails case (`qvrbench.py`, 8 shots every 10 frames) now draws 15 m trails living 2 s.

## Reloading on the move, the auto pump's delay, guns lying about, spent rifles, the Super Axe, smaller mines (2026-10-08)

The author's notes of 2026-10-08 afternoon (map1 13-56 to 14-00, vrfiringrange 14-14 to 14-35).

**Reloading while walking** (map1 14-00-24, "the major bug"): standing, a shell went in where the receiver was drawn;
walking forward he had to reach well ahead of it. The server tested a held round where the last frame had left it
(`VR_Carry_HandFrame` ran `VR_Reload_HeldFrame` before `VR_Carry_Follow`) against this frame's load point (sent with
the move, moved with the body by `rebaseHands`): a frame's walk apart, 4.4 units at run speed, against the shotgun's
1.5-unit radius. The test now runs after the round followed the hand. Measured headless (a shell held 12 units off
the drawn port): 12.0 every frame standing, running, strafing and turning (12.8 running before; `reload_test.sh` 12).
Lifts and slopes move the hands and the port together (`rebaseHands` before PostThink).

**The auto pump's delay** (13-58-48): the shot, a moment, then the stroke: `vr_autopump_delay` 0.15 s (Weapons >
Weapon Effects > Auto Pump Delay), cut so that delay and stroke end by 0.48 s (the shotgun refires at 0.5). The shell
still leaves as the fore-end reaches the back (`autopump::rearTime` matches the shot, not the stroke's start).

**Guns lying about** (14-18-22): a shell loaded by hand into a gun lying as a prop now slides in as into a held gun
(`collectfx` hotspot 241, `QVR_CFX_INTO_PROP`, the message's entity the gun; `view::propGun`, `view::modelLoadPath`:
the held path's, from the lying gun's drawn transform, a super shotgun lying open turned with its barrels). Loose
rounds by contact still go in at once (no player to send it to). A lying gun's ammo screen shows its clip and size
alone ("1/8"): a weapon prop's clip goes with its uid (`U_QVR_WEAPONUID`: a long and now a byte). Held or holstered,
the screen is as before (clip and size over the reserve).

**Spent enemy rifles** (13-56-46): while the crackle lasts in the hand the gun shakes (`vr_enemygun_spent_shake` 0.3
units, 4 degrees a unit, fading with its arcs; drawn only, its Box3D body follows the drawn gun) and buzzes
(`vr_enemygun_spent_haptics` 0.35, a 40 ms pulse every 60 ms). `vr_shock_info` prints its shake and buzzes.

**The Super Axe without cells** (14-14-01): no dry click, as Mjolnir: its cells are only its burst's.

**Proximity mines 40% smaller** (14-30-12): `vr_prox_scale` 0.47, their own (the other grenades keep
`vr_grenade_scale` 0.78), everywhere `drawnSize` reaches (flight, stuck, hand, pouches, launcher, boxes).

Tests: `reload_test.sh` 12, `autopump_test.sh` 5, `gunshape_test.sh` 5, `pouchgren_test.sh` 6, new
`empty_melee_test.sh` and `spentshake_test.sh`.
## Stealth AI: the author's notes of 2026-10-08 (see-through walls, the meters shown, the hunt, dogs, props)

His notes (map1_14-09-24, 14-10-31, 14-10-59, 14-12-27, vrteleporters_14-42-42); STEALTH.md has the rules, Debug >
Tests > Stealth AI the scenes (`vr_stealth_test 120`-`129`, QC `vr_stealth_test4.qc`; `Misc/quakevr/stealth_tests.sh`).

- **See-through walls** (`vr_stealth_seethrough 1`, Combat > Stealth AI > See-Through Walls): grates, fences, webs and
  glass let sight, light and sound through. New builtin `traceseethrough` (vr_stealth.cpp): traceline on through a face
  with a `{` texture (`SURF_DRAWFENCE`; the face found by the physics sounds' surface walk, now shared:
  `physsound::surfaceOnSegment`) or an entity whose `alpha` is under 1, out of its far side (its hull 0; a model's box).
  The stealth AI's sight (point_visible), noise, beam, torch, body and alarm lines use it. Found on the way: a noise of
  the world's (blasts) was traced ignoring world, which skips every entity world owns (doors, func_walls, crates): it
  now ignores the hearer. Test 120 (`stealth_tests.sh seethrough`): 5 PASS (opaque: meter 0, not heard; alpha 0.5:
  meter 0.86, heard; switch off: 0, not; fence textures: 0.86, heard; not: 0, not). No map the kit has carries a solid
  `{` brush (warden's webs are illusionary), so the fence path is tested on a door model with its textures taken for
  fences (`vr_stealth_test_fence`); the world's own fences go through the same code.
- **The meters shown** (`vr_stealth_debug_meters 0`, `vr_stealth_debug_meters_range 2000`; Debug > Tests > Stealth AI >
  Meters Over Monsters): each living monster's state (and Alert step, Hostile's seconds unseen) and its suspicion meter
  as a coloured bar over it, drawn by the host (vr_stealth.cpp `debugFrame`, the overlay texts the profiler's panel
  uses: not depth tested, so seen through walls). Checked on e1m1 (a grunt ahead, the light forced to 60): "ALERT walk
  0.32" over it, a yellow bar a third full; the map's other monsters "idle 0.00".
- **The hunt** (his note: "they lose interest too quickly"). A Hostile monster out of sight of its player follows his
  trail (`vr_stealth_chase_trail 1`): to where it saw him last, then ahead along his way (`vr_stealth_chase_predict
  1.5` s of his speed, which is now his moves between its sightings, smoothed, since a VR player's own walk has no
  velocity), then Quake's chase at him; it gives up after `vr_stealth_lose_time` 20 s unseen (was a fixed 6), or 5 s
  with him beyond `vr_stealth_lose_far` 1500 units, and searches where the trail ended. Combat > Stealth AI >
  Investigating: Gives Up After, Sooner Beyond, Follows Your Trail, Guesses Ahead. The meters overlay shows the trail's
  step and the seconds unseen. Test 122 (`stealth_tests.sh hunt`): its last spot reached (2 units), its guess 334 units
  on along his dart, gave up at 8.01 s (8), far: 2.00 s (2); 3 PASS. `vr_stealth_test 1`: 13 PASS (scene 13 now runs
  with the trail off and 6 s, the old rule).
- **Dogs jumping as they turn.** Cause found in the drawing, not the AI: a stepping monster's move is drawn over the
  0.1 s to its next think (R_SetupEntityTransform's move lerp), and a new move begun before the last one was drawn out
  (the server moved it again a frame or two after a step: a dog's 64-unit run steps, then a nudge, a leap's fall, a
  turn's settle) restarted the lerp from that move's end: the dog snapped forward the rest of its step at once. New
  `R_MoveLerpStart` (r_alias.c): a new move starts from where the last one is drawn now (`vr_monster_lerp_continue 1`,
  Gameplay > Monsters > Smooth Monster Steps; 0: Quake's drawing); the AO, model-hit and ragdoll mirrors of the lerp use
  it too. The AI's own steps are untouched (the dogs' erratic 16-64 unit hops stay). New debug log
  `vr_debug_drawn_moves progs/dog` ("drawnmove:" lines: a jump between frames, the move lerps begun early). Test 123
  (`stealth_tests.sh dogs`, a dog chasing him round a circle for 30 s, fixed frames): Quake's drawing 11 moves begun
  early (the worst snap 7.1 units), 1 jump between frames; with the fix 0 jumps. The headless scene snaps rarely; the
  real-time one (250 fps) logged 10 in 70 s of 10 units each. Not reproduced here: a large snap ("as if teleporting");
  A/B in VR with Smooth Monster Steps.
- **Props in the way** (his note: enemies stuck on crates and barrels). Box3D's solid props are SOLID_BBOX: Quake's
  movetogoal meets them and only tries other ways at random. New `QC/vr_ai_props.qc` (`VR_AI_MoveToGoal`, in ai_run's
  chase, ai_walk's path and the stealth walks): a solid prop swept straight ahead is pushed (`physicspush` by the
  monster's weight; `vr_ai_prop_push 1`) and walked round (to the side nearer its goal with room; `vr_ai_props 1`;
  Combat > Stealth AI > Getting Round). Test 124 (`stealth_tests.sh props`, vrteleporters room T, a knight 500 units off
  behind a row of five large crates): round them in 3.7 s (Quake's way 8.4 s). Its first form waited for movetogoal
  to fail: it never did (its random tries move the monster), so the prop is looked for before each step.

## Cell cords: the laser cannon, Mjolnir and the Super Axe on a coiled cord to the pouch (2026-10-08)

The author (vrfiringrange_2026-10-08_14-33-07, 14-33-36, 14-34-47): the laser cannon (no magazine) held gets the
flashlight's coiled cord from its bottom to the ammo pouch, as if it drained the pouch's cells; the pouch shows the
cells, faithful to the count; no cells, the cord falls loose and plugs back in on a pickup; two cannons, two cords;
optionally the same for Mjolnir and the Super Axe (both draw cells); each an option on the Reloading page. Branch
`agent/coils`.

- **Settings** (Weapons > Reloading > Cell Cords): Laser Cannon Cord `vr_cellcord_laser` 1, Mjolnir Cord
  `vr_cellcord_hammer` 0, Super Axe Cord `vr_cellcord_superaxe` 0 (the melee weapons' off: a swung hammer on a cord
  is the author's call; on, they work the same, from the end of the handle). Cord Points (a page): each weapon's
  `vr_cellcord_<laser|hammer|superaxe>_x/y/z` (model units off its end) and Cell Cord Info (`vr_cellcord_info`, also
  on the Debug page beside Flashlight Cord Info).
- **The pouch** (QC `VR_Reload_Corded`, `VR_Reload_PlayerFrame`): a corded weapon held in either hand (its cord on,
  Immersive reloading) makes the pouch's kind cells with the lightning gun's magazine (`vr_pouch_wid` WID_LIGHTNING):
  it shows the lightning gun's cells standing in it (frames 11-13: one a 36 cells, up to 3, the empty pouch at 0)
  and an empty hand takes one of those. It wins over a gun in the other hand (else the cord would have no cell).
- **The cord** (`vr_cellcord.cpp`): `coil::coiled()` (the flashlight's Coiled, now shared), from the middle of the
  weapon model's lowest vertices (frame 0, within a unit of the lowest: the laser cannon's underside at model
  (52.6, -0.1, -24.1), Mjolnir's and the Super Axe's handle ends at (1.4, 1.0, -3.7) and (0.5, 0.1, -4.6)), leaving
  it downwards, to the copper contact on a cell's top (`view::ammoPouchCell`: make_ammo_pouch.py MAGS[4], model
  (1.8, -2.6/0/2.6, 2.9)), with a small rubber boot over the plug. One cord: the first cell (the body's right);
  two: the main hand's the first, the off hand's the last shown (one cell: both on it).
- **Loose** (`coil::Cord::update` with `aLoose`: the first end's stub a free mass, the end itself straight past it;
  the body's movement still carries the line, as for the flashlight's): no cells shown, the plug comes off and the
  cord hangs and swings off the weapon. Cells again (or another cell, the count changing which one), the plug flies
  back onto its contact in 0.3 s (smoothstep, from where it hung). No pouch drawn (reloading not Immersive, Show
  off, dead): no cord.
- **Checked** (mock): `Misc/quakevr/cellcord_test.sh` 10 checks (held: the pouch shows 3 cells, plugged into cell 0,
  the line's ends 0.00 units off the weapon's end and the contact; no cells: loose, the plug 10.8 units under the
  weapon's end after 2 s; cells again: plugging in from 24.7 units, then plugged; two cannons: cells 0 and 2; the
  option off: none; Mjolnir and the Super Axe on: plugged). Screenshots (start, looking down): held, two cannons, no
  cells (both loose, the pouch empty), one cell (both on it); Mjolnir and the Super Axe from their handles' ends.
  `vr_profile` (exclusive, beside the flashlight's Coiled in the other hand): "cell cords build" 0.03 ms a cord
  (max 0.10), the flashlight's "flashlight cord" 0.03 ms (max 0.18); two cords 0.06 ms. 513 rings x 6 sides near
  (6.1k triangles) a cord, as the flashlight's.

Checklist:

- [ ] Weapons > Reloading > Cell Cords: the laser cannon held, a coiled cord from its underside into a cell in the
  pouch (the pouch shows cells while it is held: one a 36, up to 3); fire it dry: the plug comes off and the cord
  hangs off the gun; pick up cells: it plugs back in. Two laser cannons: two cords (two cells or more: one each).
- [ ] Mjolnir Cord, Super Axe Cord (off by default): the same from the end of the handle; worth keeping on?
- [ ] Cord Points: where each cord leaves its weapon, if the underside spot is wrong.

## The in-game update notice (2026-10-08)

The author's design: the game tells you when a newer release is out, from the installer's own feed.

- **The check** (`Quake/vr/vr_update.cpp`): GitHub's `releases/latest/download/latest.json` only (the author's
  decision: no vittorioromeo.com mirror for the game), read at start-up on a thread of its own (the engine's
  `Download`, as the map index), at most once an hour: a younger answer is read back from
  `<base>/cache/update_check.txt` (keyed by the feeds asked; a failed check writes nothing, and an older cached
  answer of the same feeds stands in). Its `version` ("0.9.1 (2026-10-08 abcdef12)") is compared with `VERSION`
  as semver (a "v" and the " (date hash)" ignored, prereleases older than their release, numeric identifiers
  numerically; a feed version that is not MAJOR.MINOR.PATCH gives no notice). Never at start-up in the kit's test
  runs (`QVR_TEST_BACKGROUND`). Failures are silent but with `developer`.
- **The notice** (`vr_menubrand.cpp`): "Update available: Quake VR: Unleashed X.Y.Z" in a box like the version box,
  3 px above it, right-aligned; where the menu reaches under one row (the flat main menu's Advanced VR), two rows
  ("Update available:" over the name) as narrow as the version box; VR pages keep clear of it as of the version box
  (`versionLabelClearance`, the two-row box). One link: lit under the laser or mouse, a press opens the feed's `page`
  (make_release.py now writes `https://github.com/<repo>/releases/tag/<tag>`; none for a local release) or
  `releases/latest`, with the Ko-fi link's 1.5 s repeat guard, "Opened on your desktop" and dry run. Drawn only with
  the version box. `menu.c` gained `M_ContentRightBelow`: the main menu's rows as drawn (they reach the canvas's
  bottom on a flat screen; `M_ContentExtent` said 200).
- **Settings**: `vr_update_check 1` (Advanced VR > HUD and Menus > Menu > Check for Updates; 0: no check, no notice),
  `vr_update_url` (test feeds, ';' or space separated), `vr_update_test_version` (stands in for the feed's).
  Commands: `vr_update_status`, `vr_update_check_now [startup]` (startup: as at start-up, the cache honoured),
  `vr_update_compare a b`; `vr_mock_laser update`, `vr_mock_mouse update [click]`. Debug > (with Menu Links) Check
  for Updates Now, Update Check: Status, Update Notice: Fake 9.9.9 / Real Version. README: "Network and privacy".
  RELEASING.md: "Which release is Latest" (game releases always Latest; asset releases never).
- **Checked**: `Misc/quakevr/update_notice_test.py` 18 checks against a local server (newer: notice drawn, a double
  press opens the page once; same and older: no notice; 404: silent, status says why, the next feed answers; the
  cache: one request for two start-up checks and after a restart, asked again past an hour; `vr_update_check 0`: no
  request, no notice; test runs: no start-up check). `vr_update_compare` on 1.0.0-beta.1 < 1.0.0,
  1.0.0-alpha.beta > 1.0.0-alpha.1, beta.11 > beta.2, v0.10.0 > 0.9.9, "2026-10-06" unreadable. The real feed
  today: HTTP 404 (no game release marked Latest yet): no notice.

Checklist:

- [ ] Debug > Update Notice: Fake 9.9.9: the box above the version box in the headset (one row) and on the desktop
  main menu (two rows, clear of Advanced VR); point and pull the trigger: the releases page opens on the desktop.
- [ ] After the next game release (marked Latest), an older build shows the notice within the hour.

## Teleporters: shooting yourself, stuck behind a gate (2026-10-08)

Vittorio (vrteleporters, 14:44 and 14:46): in the loop you see yourself through the gate, and a shot at your image
should hit you; and once, pushing a crate into a gate, he got stuck inside the wall behind it.

**Shooting yourself.** A shot through a gate ignored its shooter the whole way: the trace's pass entity (`self`) is
skipped by `SV_ClipToLinks`, on the far side too, and a missile never meets its owner (Quake's owner rule).
- `MOVE_HITPASS` (8192, world.h; QC `MOVE_HITPASS`): the pass entity itself, and its owner, are met. `VR_PortalTrace`
  adds it to each piece after a crossing, and to the whole trace when a player's shot starts at his image (his muzzle
  held through a gate: carried to the far side by `reachAlong`, so it starts more than 72 units from his box; QC
  `portal_from_image()`). The lightning's far piece (and a bolt from a muzzle held through) strikes with it
  (`vr_lightning_hitpass`, weapons.qc).
- A missile carried through a gate (`VR_PortalToss`) is marked (`.vr_gate_crossed`, its time) and from then on meets
  its owner (`VR_PortalHitsOwner` in `SV_MoveRun`; QC `VR_OwnerSpared` in the player's missiles' touches: nails, super
  nails, rockets, grenades, the enforcer's lasers, its and the rifle's). Other monsters' projectiles keep Quake's rule.
- A small missile (a nail: its box shallower than its middle's way to the plane) was carried wholly behind the exit's
  plane, into the wall there, and died on it at once (missiles through gates never came out). It now comes out just in
  front of the exit, where its path crossed.
- Self-damage is Quake's: armour, god mode, Quad, teamplay. The lightning gun's 600 units only reach round the loop
  room (640 wide) from close to the gate; the test fires it 23 units from the gate with the hand 0.4 m forward.

`teleporter_selfhit_test.sh`: health 100 then pellet 97, nail 94, rocket -11, shotgun 92, nailgun 91, lightning 70, the
shotgun held through the gate 92, god mode 100.

**Stuck behind a gate.** Two ways to be left in the wall behind a gate, both found by the new fuzz
(`teleporter_stuck_fuzz.py`: crates pushed into the flush, large, wide and loop gates by random walks, back-steps,
sidesteps, jumps, flings and in-and-back bounces; `vr_portals_stuck` asks the engine after each):
1. **The crossing cooldown.** After a crossing no other crossing was allowed for 0.5 s, but the split body (each half
   colliding in its own room) still let the torso past the plane. Stepping straight back into the gate he had just
   come out of (or walking on into the one he had just backed out of) took the torso past the plane uncarried, and at
   24 units past it the split ended: the full box inside the wall (`portal stuck: 1 at -256 900, torso -27.7`, behind
   the exit after a quick in-out-in). Quake's `SV_CheckStuck` then held him at the last free place, a straddle he could
   only slide back out of into the wall. The fuzz on the old build: 2 of 80 trials stuck at the end (torso -25 behind
   an exit). **Fixed**: no cooldown (one crossing a tick): a carried body comes out in front of the exit moving away
   from it, nothing bounces back. Monsters' cooldown went too (the same split).
2. **Backing out of a gate.** With his torso past the plane but not carried (a crossing refused: blocked by the crate
   at the exit, or the cooldown), walking back out did not move him at all: `VR_PortalBodyMove` skipped any gate whose
   plane the torso was behind while moving back out of it ("a sheet's opposite face"), so the full box collided, inside
   the wall. Torso 12 and 20 past the plane, stick back: frozen at y 652 / 660. **Fixed**: that skip only applies when
   the body is in front of the gate's other face (a two-sided sheet's side turned the other way); now he walks back out
   (y 520 / 528).

**The safety net** (`vr_portals_unstick`, default 1; Debug > Teleporters > Get Out of a Teleporter's Wall): each tick,
a walking player whose box is in the world's solid where he stands (`SV_Move` from his origin to itself: a gate's
split included) with his torso by a gate's plane over its aperture (from half his box's depth in front to his box's
depth and 32 behind) is got out at once: torso past the plane, carried on through (`crossPlayer`, a little further on
if the exit holds him), else put back in front of it, else carried after all. `vr_portals_stuck` (Debug > Teleporters >
Stuck in a Teleporter's Wall?) prints the state and the count. Noclip is left alone.

`teleporter_unstick_test.sh`: setpos 30 and 44 into the wall: carried on (y 958, 972, unstuck 1); 20 in (a straddle): stays;
4 in front: stays; back out from 12 and 20: out (y 520, 528); the rebound (in, out, in, then back through the exit):
carried each time, never stuck. Off (`vr_portals_unstick 0`): Quake's `SV_CheckStuck` puts him back where he stood
before setpos (the control).

**The fuzz after the fixes**: seeds 21 and 5, 80 trials each: 0 stuck (seed 21 before: 2). The safety net fired in 4
of the 160 trials (75 and 20 ticks), each time a 1-unit nudge back out of an exit whose frame the player was pressed
into after his body left its aperture's footprint (a sidestep at the exit, a crate in the way): the split there ends
once his body no longer fits it, his trailing half still in the wall. Harmless (Quake's `SV_CheckStuck` did the same
with `oldorigin`), but a sign that the split at an exit could keep him to the aperture as the entry's does (open).

(The fuzz's runs are packed under 7000 characters of script: a longer `-Script` is cut short on its way to the game,
Windows' command line, and the run waits out its timeout; 80 trials take about 8 minutes at `vr_mock_eye_size 160`.)

## Vittorio's decisions of 2026-10-08: 1.0.0, the deadzone, MG3's ghosts, the kill frame, dprints, spawn facing, the force grab's search

- **Version 1.0.0** (`VERSION`; RELEASING.md's examples; RELEASE_TODO.md ticked): the first release is 1.0.0. The
  console's line reads `1.0.0-dev (...)` in dev builds, the menus' corner `v1.0`.
- **Stick deadzone 10** (was 25; `vr_deadzone`): configVersion 105 moves configs still at 25 (a config at any other
  value keeps it). The Stick Deadzone rows' help: worn sticks that drift often need 20-25%.
- **MG3's ghosts see-through** (`vr_mg3_ghost_alpha` 0.4, Debug > Tests > Dawn of the Machine Tests: Ghosts' Opacity;
  QC vr_mg3_ghost.qc): entity alpha, read again as a ghost stands and runs (a change shows at once); 1 draws it solid
  with its shadow, as before. Translucent entities cast no shadow and get no ambient occlusion (vr_lighting.cpp,
  vr_shadows.cpp, vr_ao.cpp). The upstream source (`mg3_player_ghost.qc`) isn't in the worktree; the port's note says
  upstream sets no alpha, so 0.4 is a choice: a faint figure, its outline still readable (a screenshot pair in e1m1
  beside a grunt). Its death is its own animation and the teleport flash: `vr_mg3_btest 3` checks the alpha and that a
  200 blow makes no small gibs, cut limbs or ragdoll.
- **The kill frame** (PERF_DECISIONS.md 10) and **the dprints** (12): weapon ids without walking the records; the
  dprint(sprintf()) of props, small gibs, limbs, the dropped guns' cap and debris formatted only with developer on.
- **Spawn facing** (`vr_test_spawn_facing`, Debug > Tests > Ahead of You: Facing): 0 as each spawner did (Put It There
  and impulse 244 facing you, `vr_physics_spawn` its spawn's angle 0), 1 towards you, 2 away (it looks where you look:
  trailer shots from behind), 3 the spawn's own angle; set before the spawn function, so `ideal_yaw` agrees.
  Checked in e1m1: each mode's three spawns at 270 / 90 / 0 with the player at 90.
- **"Monsters spawned ahead stand 15-16 units low"** (seen while testing, above): not a spawn-height bug. In e1m1 and
  vrfiringrange alike the player starts on a step 15-16 units above the floor ahead of him: `droptofloor` (vanilla's
  hull trace and Quake VR's point traces agree) puts the monster on that lower floor, and its first step towards the
  player climbs the step. Spawned beside the player on his step (24 units ahead in vrfiringrange) it stands level.
- **The force grab's search** (PERF_DECISIONS.md 11): through the area grid, the same chain; `vr_forcegrab_grid`,
  `_verify`, `_stats`.
## The trailer scene: vrtrailer (2026-10-08)

A small map for recording the trailer's opening shot in VR (MAPPING.md, "vrtrailer", has the layout, the lighting
numbers and the shot commands): a wooden bridge across a night lake, torch pillars in pairs out of the water, a stone
pedestal half way with Dawn of the Machine's Super Axe lying on it, a grunt on the far islet looking out over the water.
`Misc/quakevr/maps/vrtrailer_gen.py` imports vrstart_gen.py's parts (planks, logs, railings, pines, boulders, torches,
lanterns, materials, sky, moonlight, fog; its cliffs at 1/1.75 scale) and compiles as vrstart does.

- **Oblivious until hurt** (`vr_oblivious 1` on any monster; QC vr_fields.qc): FindTarget returns at once, no enemy
  shove, `VR_Stealth_Applies` false (no meter, noise, touch, body, beam); T_Damage clears it. `VR_Trailer_Take`
  (vr_trailer.qc, from T_Damage): a player's blow armed to behead an oblivious monster deals at least its health, so the
  cut that beheads always kills (decapitation itself is unchanged: the head's sphere, the blade, the speed).
- **func_weapon_grabbable** spawnflags: 1 AS_PLACED (lies as its "angles" turn it, not any way), 2 BY_HANDLE (a hand
  closing anywhere on it takes it by its handle, `vr_grab_by_handle`: a weapon carried by a hotspot is "held off its
  handle" and never beheads, VR_Decap_Slash's `mh_free`). `VR_WeaponGrabbable_Make` lays it again.
- **info_vr_trailer** (vr_trailer.qc): finds the scene's grunt and axe a second in; `vr_trailer_reset 1` (Debug > Tests
  > Trailer Scene) for retakes; `vr_trailer_log 1`; the player starts empty-handed in this map (DecodeLevelParms:
  `VR_Trailer_StripParms`); recording mode on there.
- **Recording mode** (`vr_recording_clean`, archived, 0: Graphics > Recording > Recording Mode; `vr_recording_clean_map`,
  set by QC at every map load, 1 in vrtrailer): no tips (vr_tips.cpp frame), no wrist-gadget log or hologram
  (vr_gadget.cpp logShown, hologramOn), no head-locked text in the eyes, mirror or spectator camera (vr_stereo.cpp);
  `qvr::recordingClean()`.
- **Tested**: `Misc/quakevr/vrtrailer_test.sh <agent>` (headless, stealth AI on): walked to the pedestal, the main
  fist on the axe: taken by its handle; walked to 30 units behind the grunt with the axe (23 log lines, all "enemy
  worldspawn, think stand, stl 0, oblivious 1"); the off hand on the handle (two hands: "2h grip: off hand took it");
  the swing (`vrtrailer_swing.mock`, `vrtrailer_swing.py`: hands 0.40 m under the eye, 0.62 m out): "monster_army
  beheaded by player (a slash at ~20 m/s)", health -1; reset: grunt alive and oblivious, axe at z 74.2, player at the
  start. 9/9, four runs; also with `vr_ai_enhanced 0`. Sweeps of the synthetic swing (a grunt 30 units ahead): reach
  0.6-0.65 m with the hands 0.34-0.42 m under the eye behead him; lower strikes his shoulders (killed, not beheaded);
  a shorter reach misses; a longer one strikes his head with the handle ("the hilt or handle", not beheaded). e1m1
  smoke; QC 0 warnings; FGD check; `vr_menu_path_check maps/vrcalibration.map` 0 missing.
- **For the take (the author)**: grab the axe anywhere (it comes by its handle); come up close behind him; swing so the
  axe's head (not its handle) crosses his head or upper neck, not his shoulders (his idle pose holds the rifle up: a
  low swing hits his arm). `vr_trailer_reset 1` between takes (or Debug > Tests > Trailer Scene > Reset Trailer Scene).

## Teleporters, not slipgates (2026-10-08)

Vittorio: in the Quake universe a slipgate is id's machine; the surfaces that take you elsewhere are teleporters.
Everything of ours that called them slipgates says teleporters now: code, comments, QuakeC, menus (Graphics >
Teleporters, its switch Quake VR Teleporters, Teleporter Stars; the Debug pages), console output, the test scripts
(`Misc/quakevr/teleporters/`: `teleporter_edges_test.sh`, `teleporter_selfhit_test.sh`, `teleporter_unstick_test.sh`,
`teleporters_test.sh`, `make_vrteleporters_map.py`, ...; `Misc/quakevr/teleporter_{off,cross}_test.sh`,
`teleporter_off_profile.sh`), the bench scenarios (`teleporter_start`, ...), the docs and the maps' boards and tips.
Player-visible text that said "portal" says teleporter too ("Portal Stars" is Teleporter Stars); `vr_portals_*` and
`VR_Portal*` keep the name of the technique.

- **Settings:** `vr_slipgates` is `vr_teleporters`, `vr_slipgate_{pair_exits,self_head,surface_size,surface_opacity,
  surface_fade}` are `vr_teleporter_*`. The old names stay silent aliases (`VR_CvarAlias`, asked by `Cvar_FindVar` when
  a name is not found): they read and set the new settings, so an old config's lines set them as it is executed (the
  next write has the new names: that is the migration, no config version needed), and binds, scripts and benchmark
  setups work. They are never listed, completed or written.
- **The test map:** `vrslipgates` is `vrteleporters` (`.map`, `.bsp`, `.lit`, `.lux` moved with git). `VR_MapAlias`
  loads it for `map vrslipgates` and for a save made there (tested: a save's map line edited to `vrslipgates` loads
  `vrteleporters`); its welcome tip seen under the old names counts as seen (vr_tips.cpp).
- **The maps' text:** vrstart (the campaign board, the lecterns' "step into the teleporter", the tip), vrtutorial2
  (the arena's way-out board) and vrteleporters (its board, tip and title): the generators say teleporter, and each
  regenerated `.map` differs from the committed one in those text lines only, so the BSPs' entity lumps were edited in
  place (`bsp_set_entities.py --from-file`, as for vrstart's tutorial button) instead of a recompile: the geometry,
  lightmaps, `.lit` and `.lux` are byte for byte the ones tested before.
- **What still says slipgate:** id's (e1m1's name, the Slipgate Complex; the finale texts in client.qc; the mission
  packs' `$map_walk_slipgate_exit`), the aliases above and their notes, quoted commit subjects, and the names of
  scripts deleted before the rename (REPO_CLEANUP.md).

## Loose rounds slide into the gun too (2026-10-08)

Vittorio: a shell hand-fed into a shotgun lying as a prop slid in, but one dropped onto it or thrown into its receiver
just vanished. The slide (vr_collectfx.cpp's "into the gun", `collectfx` hotspot 241 `QVR_CFX_INTO_PROP`) went to the
loading player's client only, and a loose round loading by contact has no player, so `VR_Reload_PropSlide` sent nothing.

- **Into a gun lying about:** `collectfx` with hotspot 241 now goes to every client that can see the gun
  (`server::sendCollectSeen`: `SV_VisibleToClient`, the gun's fat PVS from each client's eye), whoever `self` is, from a
  hand or a loose round alike. The client draws it as before: the round's model from the server's pose, along the
  lying gun's load path in its model space (`view::propGun`, `modelLoadPath`), carried by the gun. Other players now
  see a hand-fed one slide in too. Shells, the super shotgun's pair (lying open) and the launchers' rounds slide;
  magazines seat at once, as from a hand (they have no slide either way).
- **Into a held gun:** a loose round by contact already slid in on the holder's client (`VR_Reload_LoadInto` sends
  `QVR_CFX_INTO_GUN` with the other hand); checked, unchanged. Other players still see it vanish (their view of
  someone's held gun has no load path).
- **Test aid:** `vr_reload_test 26` (Debug > Tests > Reloading, "Drop a Round Onto the Lying Gun"): a loose round
  for the nearest lying gun let go of 6 units above its load point. `gunshape_test.sh` section 6: tossed into and
  dropped onto the lying shotgun, the pair into the open super shotgun, a grenade into the lying launcher (each slides
  from its port to the end of the path, 33 frames drawn), the nailgun's magazine seats with no slide, a loose shell
  into the held shotgun slides as before.

## Guns lying about taken by the handle; the super shotgun with pouch shells in hand (2026-10-08)

**Taken by the handle.** The author: "very hard to grab guns by the main handle while they're in prop form ... without
force grabbing". Headless (a shotgun let go of, the off hand stepped down over its handle a unit at a time): the closing
hand nudged the gun, a Box3D prop, awake from then on, so not FL_ONGROUND, so not "lying" (vr_physics.cpp lyingWeapon):
vr_weapon_grab_slack's 5 cm dropped to 0, and the fist pushed it on ahead of itself (the gap held at 2.6-4.9 cm, never
taken; pushed further, 40 units across the floor). Now a weapon barely moving (under 100 units/s) with the floor under
its handle lies too, and **vr_weapon_grab_handle_leniency** (5 cm; Hands > Handle Grab Leniency) adds to the slack when
the hand's point is within vr_weapon_grab_anywhere_min (12 cm) of the handle (its origin): taken by the handle with the
fist up to 10 cm off it. In the air nothing changes (no floor under it, or flying faster: the catch is by the fist on
it, slack 0), nor a force grab (its catch runs the weapon's handtouch itself, VR_Forcegrab_Catch, not handOn).
weapon_catch_test.sh checks 4-5: taken at 7.7 cm (allowed 10), and with the leniency 0 the slack's 5 alone, not taken
from 10 cm over it. Check 3 (the crowbar, gripped at its middle, its handle) now sets the leniency 0 to test the slack.

**The super shotgun hit with shells in the hand.** The author: still unable to hit it open or shut with the off hand
holding shells from the pouch. VR_Reload_SsgHit returned at once for any round the other hand held (it was there so a
pair brought to the breech wouldn't count as a hit). Now a held round hits as the fist holding it or by its own shape
(shapenearest), whichever is nearer the barrels' front half. Shut, it breaks it open (a shut gun's chambers take nothing:
the tap at the breech is unchanged). Open, the round within the port's radius plus 4 units of the chambers loads and
never hits (the load wins); a fast hit on the front half of the barrels from below elsewhere shuts it, the pair kept in
the hand. reload_test.sh: a pouch pair hits it open from above, shut from below, kept (10).

**Shut straight after loading.** The author: after loading fresh shells into the open super shotgun, a long wait before
he could flick it shut. No timer stood in the flick's way; two things did (headless, real time, the mock's turns
reporting their angular velocity). (1) The flick is measured from the hand's up while it was still (vr_flick.cpp
restUp), and "still" was under 1.5 rad/s (86 deg/s): a gun hand still turning as the pair went in (bringing the gun back
up) kept its up from before, so the flick had to swing past that before it counted, and did nothing until the hand had
paused (a pair loaded with the gun hand turning 94 deg/s, flicked: never shut). Now **vr_reload_ssg_flick_rest** (180
deg/s, at most 3/4 of the flick's speed; Weapons > Reloading > Flick Rest Speed) for the super shotgun that breaks open,
and the up is tracked whether it may flick or not (it wasn't while shut with Open by Flick off, his setting). (2) A flick
seen late in its swing set its bit for one client frame, lost among the moves the server reads at its tick (seen: "flick
reload" printed, the gun stayed open): the bit is now held 0.12 s, as the pry's is. And a hit opened or shut it only
0.6 s after a shell went in (the loading hand drawing back): **vr_reload_ssg_hit_after_load** 0.13 s, the slide-in's
time (Hit After Loading). vr_reload_debug 2 prints the flick's speed and how far the barrel is towards the up at rest.
reload_test.sh: loaded with the gun hand turning 150 deg/s and flicked 0.15 s later, shut; a hit from below straight
after the load shuts it (with 0.6 it doesn't).

## OBS's recording from the menus (2026-10-08)

His request: when OBS is open, a row in the headset's menus above the spectator camera's switch that says whether
OBS is recording and starts or stops it. `Quake/vr/vr_obs.cpp` is a small obs-websocket v5 client (OBS 28+ ships the
server: Tools > WebSocket Server Settings > Enable WebSocket server, port 4455): TCP (winsock), the WebSocket handshake
and framing, JSON (json.c), the v5 authentication (base64(sha256(base64(sha256(password + salt)) + challenge)),
`vr_sha256`). It runs on a thread of its own (a loop that blocks on its socket); the main thread only copies the cvars
over and the state back under a lock (no allocation a frame).

- When: the thread starts the first time a menu opens with `vr_obs 1`, and asks at once, then every 5 s while a menu is
  open. On Windows it first looks for OBS's process (obs64.exe, obs32.exe, obs.exe; Toolhelp): none, nothing is tried.
  A connection stays open while OBS keeps it. A refused password is not tried again on its own (OBS logs each one): a
  new `vr_obs_password`, a press on the row, `vr_obs_connect` or a menu opened again tries once more. The kit's test
  runs never reach for OBS on their own (`vr_obs_connect` opts in).
- The row (`vr_menuui.cpp` `obsBannerLayout`, `obsBannerAt`; drawn as the switch, right edge with it, a light red while
  recording, dimmed while paused): `OBS: Not recording`, `OBS: Recording 00:12:34` (short: `OBS: REC 00:12:34`),
  `OBS: Paused ...`, `OBS: Starting...`/`Stopping...`; hints `OBS found: enable its WebSocket server` (OBS's process
  runs, nothing listens), `OBS: password needed`, `OBS: wrong password` (closed with 4009). Hidden when OBS is not
  found, `vr_obs 0`, or before the first GetRecordStatus answers. A press: ToggleRecord (a hint's row: ask again
  now); a second press within 1.5 s does nothing (as the menus' links). The time is OBS's outputDuration, run on
  locally between GetRecordStatus every 2 s while a menu is open; RecordStateChanged events (Outputs subscription)
  follow starts and stops. The spectator preview sits above both rows.
- Cvars: `vr_obs` 1, `vr_obs_host` 127.0.0.1, `vr_obs_port` 4455, `vr_obs_password` "" (all archived: the password is
  kept in the config in plain text, never printed), `vr_obs_process_check` 1 (0 always try; 2 test aid: act as if OBS
  ran). Commands: `vr_obs_status`, `vr_obs_toggle` (bindable), `vr_obs_connect`. Graphics > Recording > OBS (the
  switch with the how-to, the status line, Start or Stop Recording, Connect to OBS Now); Debug > Tools: OBS: Status,
  OBS: Process Check. `vr_mock_laser obs` points at the row.

Tested: `Misc/quakevr/obs_test.py <agent>` against `Misc/quakevr/obs_mock_server.py` (Hello/Identify with and without
a password, GetRecordStatus, ToggleRecord, RecordStateChanged; standard library only), 15/15: hidden with nothing
listening; the hint with the process check faked; Not recording, two presses within 1.5 s one toggle, Recording
00:00:04, pressed again Not recording, the mock quitting hides it; password needed, wrong password (one 4009, not
retried in 6 s), the right one identified; the password never in the output. Frames (`--exclusive`, vr_bench 450
frames each): period p99 4.21 ms off, 4.05 trying a port nothing listens on, 4.03 connected and recording; the main
thread's worst CPU work 0.47 / 0.30 / 0.29 ms. `obs_test.py --shot`: the eyes with the row (vr_eyeshot 3).
In VR (his part): OBS's WebSocket server on, the row's text and its press, a recording started and stopped.
## Gadget fingertip, tap zone, trails in VR, Death View menus (2026-10-08)

Vittorio's notes vrfiringrange_2026-10-08_22-35-47 .. 22-44-48.

- **The side button's fingertip** (vr_gearlights.cpp): it was the hand's tracked point plus `vr_gadget_button_reach`
  (4 cm) forward, far off the drawn index finger (his screenshot). Now the jointed hand's index fingertip as drawn
  (`view::drawnIndexTip`: `grasp::fingerPoints`' tip through the rig's placement and the palm's fit, noted at the end of
  `setupRigHand` in the hand's tracked frame, used the next frame), `vr_gadget_fingertip_drawn` 1 (0, or no jointed
  hand: the old reach). Tuning: `vr_gadget_fingertip_x/y/z` (cm forward, outward, up; outward mirrored on the off
  hand), `_pitch/_yaw/_roll` (turned round the hand's point first); the button's face tilt `vr_gadget_button_pitch`
  (out of the screen) and `_yaw` (along its width): the cut plane and the press side turn with it. Show the Button draws
  the drawn fingertip (white) joined to the tuned one, and a short line the way the face points. HUD and Menus > Wrist
  Gadget. Mock (open hand): the fingertip 5.0 cm forward, 1.9 right, 3.6 below the hand's point (the reach: 4, 0, 0).
- **The screen tap's zone** (vr_bullettime.cpp `tapZone`): the screen moved (`vr_bullettime_tap_x/y/z`, cm) and sized
  (`vr_bullettime_tap_width/_height`, shares of the screen's, 1), the margin and depth as before; Show Gadget Button:
  And the Screen Tap draws it and the striking points (blue). `vr_gear_lights_info` prints the zone. A tap 3 cm right
  of the middle counts with the shipped zone, not with a tenth of it; gadget_tap_test.sh unchanged. The Screen Tap rows
  are together again (Distortion Trails had been put among them).
- **Distortion trails in VR**: the end-on fade (sine of the angle the trail is seen at, 0.1 .. 0.35) faded your own
  shots: in VR they leave the gun a hand's width or two off the eyes and run away from them, so past about 2.5 m the
  sine is under 0.1, and the near-eye fade (to 40 units) took the rest: a faint hint by the muzzle, nothing behind the
  pellets. The headless checks fired across the view. Now a trail fades only where its line runs through the eye
  (within half of its half width: its facing is undefined there) and near the eye within 32 units. Reproduced with the
  mock hand at the hip firing the nailgun and the shotgun in bullet time, paused (`vr_bullettime_trails_list` now prints
  the places strong enough to see): nail 3 of 13 -> 12 of 14 (strongest 0.48 -> 0.90), pellets 27 of 80 -> 58 of 72;
  per eye (vr_eyeshot) trails on vs bend 0: nail 0.44% / 0.31% of the pixels, pellets 0.92% / 0.84%. Test shots across
  the view unchanged (12 of 18).
- **Death View**: VR Settings keeps only the switch; Turn With the Body, Turn Speed, Smoothing, Fade, the new Out for
  Menus and Your Body: Killing Blow's Push are on Advanced > Body, Death View. Immersive is the default (config version
  106: a config still at 1 takes 2). A menu or the console open while dead in Immersive moves the view out to Third
  Person's place over `vr_death_view_menu_time` (0.3 s, smoothstep), its turn eased out too; past half way the hands,
  the gear and the body's head are drawn again; back in as it closes. Mock: inhead 1 -> 0.86 -> 0.30 -> 0 (camera 71
  units from the head's eyes), closed 0.12 -> 0.65 -> 1 (1 unit).

## Swimming strokes sound as water, not as slaps (2026-10-08)

His note (vrfiringrange_2026-10-08_22-26-37): swimming with the hands played the slap's sound and the swing's swish;
he wanted a stroke to sound like water moved by the hands, the slaps silent under water, a real punch kept.

- **No slaps under water** (QC vr_melee.qc `VR_Melee_HandUnderwater`: the grip in water, slime or lava):
  `VR_Melee_Slaps` is false there, so an open hand under water neither slaps nor whooshes, and (as a slap's whoosh
  did) no longer wakes monsters (`show_hostile`). A closed fist's punch still lands, with its sound and its wake.
- **No whoosh under water**: `VR_Melee_Whoosh` plays nothing for a hand under water (a punch, a weapon: no air to
  swish); the swing still counts (the motion event, `show_hostile`). `developer` prints `melee sound: none (...)`.
- **Each hand's stroke heard** (vr_physics.cpp `strokeFeedback`): once a stroke past its power gate (as before), but
  now as the hand passes its fastest (below 92% of its peak, or as the stroke ends), each hand on its own (0.25 s
  apart; before, one sound for both hands, 0.3 s), as loud as the stroke was fast. Near the surface (within 10
  units), the recorded strokes and a splash, as before; deeper, new synthesised sounds of water swept aside
  (make_sounds.py `swim_stroke`: muffled churning noise and a few bubbles): `vr/swim_soft1..3` below 2.2 m/s,
  `vr/swim_hard1..3` above. Volume: `vr_water_sounds`.
- **Stealth**: swim strokes make no stealth noise (decision: quiet strokes, a swim past monsters stays possible; a
  slap's or punch's whoosh woke them, an open hand under water now doesn't).
- Water entry splashes of a hand slapping the surface stay (vrfiringrange's pool: the hands going in and out).

Test: `Misc/quakevr/swim/swim_sound_test.sh <agent>` (10 checks: a dry slap whooshes and wakes; the strokes heard
once each (14 with the defaults below), no whoosh, no wake; an open hand swept under water silent, no wake, its stroke heard (swim_hard, 0.9); a
punch under water counts, without whoosh).

## The author's settings of the evening of 2026-10-08 are the defaults

His note (vrfiringrange_2026-10-08_22-20-23): "As usual I've been tweaking lots of values, please make them the new
defaults". His config of 23:12 against a `resetall; writeconfig` dump (with vr_defaults.cfg's values over it); a value
not in the list of the morning's promotion (above, "Out of the water by hand; ...") is one he changed since.
Config version 107 (`vr_cvars.cpp` defaultChanges: a config still holding the old default takes the new one):

- Swimming: `vr_air_supply` 2.5 (was 2), `vr_swim_flat_exp` 1.5 (1), `vr_swim_glide` 0.7 (0.6, vr_defaults.cfg),
  `vr_swim_max_speed` 400 (500, vr_defaults.cfg), `vr_swim_stroke` 10 (12, vr_defaults.cfg), `vr_swim_stroke_min` 1
  (0.4), `vr_swim_stroke_pitch` -10 (-8), `vr_water_jump` 0 (1: out of the water by the hands, with climbing on).
- The immersive death view's tuning: `vr_death_view_fade` 0.2 (0.5), `_smooth` 0.25 (0.15), `_turn` 0 (1),
  `_turn_speed` 30 (60); the mode itself Immersive since config 106 (the Death View menus' change, above).
- Reloading: `vr_autopump_delay` 0.25 (0.15), `vr_reload_front_angle` 45 (35), `vr_reload_port_gl_radius` 2.1 (2),
  `_prox_radius` 2.1 (2), `_rl_radius` 2.5 (2), `vr_reload_ssg_hit_close_angle` 35 (40), `_hit_close_speed` 2.5 (3),
  `vr_reload_ssg_lift_hold` 0 (0.2: a jolt up now shuts it too), `_lift_speed` 250 (300).
- `vr_weapon_grab_slack` 0 (5: he found it took weapons with the hand off them). A weapon lying on the floor is now
  taken off its handle only by the fist on it, and at its handle within `vr_weapon_grab_handle_leniency` (5 cm) alone;
  the headless shotgun-handle take saw gaps of 2.6-7.7 cm, so a lying gun may need a second try: Handle Grab Leniency
  10 would give the handle back its 10 cm. weapon_catch_test.sh sets the slack to 5 where it tests the slack.
- Weapon settings version 39: the nailgun's foregrip (slot 3, `vr_wofs_hs1_*_04`) at 4.95 -2.13 -3.56 (3.35 -2.01
  -4.13), turned -10.19 / -6.64 / 6.34 (0 0 0), each key where the config still holds the old default.

Left as they are: the foe grab's (`vr_foegrab_break` 20, `_drag` 20, `_drag_speed` 300, `_leniency` 1: settings
turned up while testing it broken; its own new defaults came with its fix), `vr_forcegrab_mode` 0 (the force grab off: a feature switch, not a tuning), `vr_teleport_enabled` 1 and
`cl_alwaysrun` 0 (the Comfortable comfort preset's), `vr_timescale_wav` 1 (Log Highlights' second recording),
`vr_bullettime_tap_speed` 1 (the default before 15:50, when it became 1.2: his config kept the old value, not a new
tweak), the nailgun's third hotspot (slot 3 `hs3_*`: placed, but its type is none), `gl_texture_anisotropy` 16,
`vr_spectator_scale` 1.5, the props slots given ids (33, 57-64), and the morning's list (bookkeeping, body, motion
recorder, menus, desktop window, `vr_foveated`, `vr_comfort_vignette_strength` 0.4995, slider noise).
Tests: the kit's baseline config (34) and his config with these at their old defaults (105/38) both come out at the
new values after a map loads (config 107, weapons 39), a value of his own (`vr_swim_stroke` 15) kept;
weapon_catch_test.sh 6/6; swim_sound_test.sh 10/10; reload_test.sh 100/100 (its jolt check sets Lift Hold 0.2).

## Seated magazines are solid (2026-10-09)

The author's note vrfiringrange_2026-10-08_22-25-59: a prop held in the other hand, and the empty other hand, passed
through a magazine seated in a gun. A seated magazine (the nailguns', the thunderbolt's cell; drawn as its own model,
vr_mag_on_<gun>.mdl, in the gun's model space) is now solid with its gun in every system the gun is
(**vr_reload_mag_collide** 1; Weapons > Reloading, All Guns: "Solid Magazines"; 0 as before):
- **Box3D** (the props it pushes, both held and lying): its own hull (its model's vertices through the gun's drawn
  transform: held::magazineVertices / drawnMagazineVertices; 32 vertices at most), one more shape on the held gun's reach
  body (ReachKey::mag: the body made again when it comes out or goes in; the swept swing sweeps it too) and on a lying
  gun's prop body (Slot::mag: made again when a magazine is pulled or seated; the gun weighs as before). Kept a world
  (propHulls, by model and box). Not for a carried gun or the map's spinning pickups (fixtures).
- **The empty other hand** (vr_hand_collide, vr_view.cpp pushAgainst): the magazine's triangles as a part of the gun: each
  of the hand's points held out of whichever it is in (or nearer); vr_debug_hand_collide prints "mag real / drawn".
- **A prop in the other hand** (vr_held.cpp meetFrame): the magazine's box (view::DrawnWeapon::magBox, as drawn in the
  hand) besides the gun's; the deeper wins. The round leniency (Collision Leniency) is for the gun's box only (the well
  is taken). vr_debug_carry prints "(a weapon's magazine)". Most of the nailguns' magazines lay inside the gun's own box
  already; what sticks out of it (below the nailgun's) was passed through.
- **The other weapon** (vr_selfcollide.cpp): the magazine's capsules fitted as the weapon's, with the weapon's.
- Unchanged: the grip on it (an empty hand on it is not free: not pushed), the pull and B/Y, the knock-out (the server's
  tracked hands), contact loading (an empty well has no magazine).
- Not done: a held gun pushed into a monster or a thing lying about (vr_modelcollide.cpp) tests the gun's own vertices,
  not its magazine.

Tests: `magcollide_test.sh` (new, 11 checks, each against the setting off): the palm 1 unit into the nailgun's magazine's
side is drawn 0.65 units off it, pushed back 3.2 (off: in it, not pushed); a held grenade 0.5 of the magazine's length
under it meets it (off: nothing); the held nailgun's body "12 convex pieces and its magazine"; the nailgun, super
nailgun and thunderbolt lying: a point in the magazine 0 units from the body (off 1.69, 1.80, 1.76); the hand gripping
each magazine holds it, drawn 0.002 units off its tracked place. Debug > Tests > Reloading "The Lying Gun's Magazine Is
Solid" (vr_reload_test 27); vr_mock_hand_to magpalm / magheld. Box3D step, six guns in a heap swept by the held nailgun
(exclusive): 0.050 ms a frame (worst 0.49) against 0.048 (0.32) off. contact_test 22/22, gunshape_test 22/22,
reload_test 100/100.
**For VR:** the empty hand and a held prop against a seated magazine (all three guns), the two guns crossed at the
magazine, a nailgun lying on the floor (it may now lie on its magazine or tip over it), the magazine still gripped,
pulled out, knocked out and loaded.

## Ragdolls' skins as sharp as the living monster's (2026-10-09)

His note: a grunt's ragdoll looks much more pixelated than the living grunt. Cause: retro textures (vr_retro 1, his
setting). A block's size is the skin's in Quake texels (vr_retro.cpp skinSize): a .mdl's own size, but a quarter of the
texture's for any skeletal (IQM) or MD3 model (painted at four times a Quake skin's density). A ragdoll's skinned copy
("<model>#rag", vr_ragdoll.cpp VR_SyntheticModel) is skeletal with the .mdl's own 8-bit skin, so its blocks were four
Quake texels wide: vr_retro_list showed `soldier.mdl skin 256x256` alive and `soldier.mdl#rag skin 64x64`. Every
ragdoll had it (ogre, knight, dog, enforcer: the same quarter), not the grunt alone; retro lighting's model grid
(VR_RetroLightSkinScale) took the same quarter. Now modelmeta::quakeSkin (a .mdl, or a model with the Ragdoll trait)
keeps the .mdl's size for both: the ragdolls list 256x256 (grunt, knight, dog), 512x256 (ogre), 576x384 (enforcer), as
alive. Eyeshots (vrfiringrange, the grunt from 1.6 m): the body's Laplacian sd 20.3 before, 24.1 after, 25.4 alive.
Textures, samplers and texture coordinates were already the .mdl's (the same gltexture_t).
Tests: `ragdoll_test.sh <agent> retro` (new case: the living grunt's and his ragdoll's skin sizes, equal).
**For VR:** with retro textures on, kill a grunt, an ogre and a knight: the corpse's skin as detailed as alive.
## The super shotgun's blood broken open (2026-10-08)

His note vrfiringrange_2026-10-08_22-03-33: the blood on the super shotgun vanished as its barrels opened ("a
different texture"). Broken open, the gun is drawn as make_ssg_open.py's two parts, other entities with their own
models whose skin is the gun's with 36 rows added under it for the breech plates; the wound masks are keyed by entity,
so the parts showed none. Now the parts read the gun's mask (`view::ssgPartSource`: a hand's, a holster's, a lying
prop's gun), its height read as the parts' skin is taller (the gun's texels where they were, the plates' rows past
its region, clear); blood that strikes it open is painted (and washed) through the parts as drawn
(`view::ssgPartsOf`), into the same mask. Test: reload_test.sh section 7 (shut, open, shut: the gun's own blood open
78% of shut here; 0% before). `vr_gore_spatter_test propoff` (Debug menu, Gore) bloodies what the off hand holds.

## The super shotgun hit with shells in the hand as with it empty (2026-10-08)

His note vrfiringrange_2026-10-08_22-11-35: opening and shutting the super shotgun with the other hand felt perfect empty,
but holding pouch shells it took more force and a more precise hit. The differences found (VR_Reload_SsgHit and around):

- The hit point: empty, the hand's point; holding a round, the nearer of the hand's point and the round's own shape
  (shapenearest), whose side test (where it came from 50 ms before) read off the round. Now a round changes nothing:
  the hand's point, as empty.
- Load before hit: open, a round within the port's radius **plus 4 units** of the chambers refused every hit. The
  barrels' front 40% (the hit zone) starts about 7 units from the breech, so the refusal reached it: hits from below
  near the breech never shut it with shells in the hand (the mock: none of the swings at 0.6 and 0.75 of the gun did).
  Now only within the port's own radius, where the round loads this very frame (VR_Reload_HeldFrame).
- The haptic: shut by a hit, the hitting hand buzzed only if empty; now holding a round too.
- The same: the speed (the hand's tracked velocity: no weight lag worth a mention, 0.1 kg, the spring 0.02 cm off),
  the reach, the angle checks, the timing (Hit After Loading only after a load, which an empty hand never makes).

Left as it is (looks only, not the hit): an empty hand is drawn held off the gun's surface (vr_hand_collide, its
mesh) with a buzz as it meets it; a hand holding a round has the held things' box collision instead (the round against
the gun's box, vr_reload_collide_leniency 4 cm into it: so a round reaches a port under a receiver). Worth his look.

Test: reload_test.sh section 7, the same glided swings (exact speed, the same tracked poses after the same visit to the
pouch) empty and holding a pair: from below near the breech and at the front, from above, at 2.5, 3.8 and 7.6 m/s: the
same outcome and speed.

## The ammo pouch's launcher rounds held the same way every time (2026-10-08)

His note vrfiringrange_2026-10-08_22-18-19: as the back pouch's grenades, the rounds taken from the ammo pouch for the
launchers (a rocket, a grenade, a multi-grenade or multi-rocket, a proximity grenade) should come out held the same
way, customizable, at an angle easy to load. Before, each was placed by its own Held Object Offsets grip (the grenades
in the palm, the rocket along the handle at its own turn; the multi-grenade's model stands along its z): the mock
measured the rocket's long axis about 60 degrees off the grenade's, pointing back. Now QC's carryfrontpouch (vr_grip.cpp serverFromPouch,
the grenade pouch's carrypouch's twin): every round, whatever its grip, its middle in the fist's grip channel, its long
axis along the hand's forward, nose ahead, then turned about its middle by vr_reload_front_hold_pitch / _yaw / _roll
(Reloading > Launchers: Round In Hand Pitch, Yaw, Roll; mirrored for the left hand), placed again at once when they
change, until a regrip (then its grip as before). Default pitch 90: standing in the fist, nose up out of its thumb
side, butt down by the little finger, so with the thumb turned forward the round lies along the barrel, butt to the
muzzle (the grenade pouch's 90, too). A round picked up where it lies keeps its Held Object Offsets grip. Test:
front_test.sh section 5 (every kind, three hand turns: nose up in the hand, (0, 0, 1); pitch 0: along the forward).

## Parried monsters drawn squashed (2026-10-09)

Your note: "Sometimes when I parry an enemy, its body becomes all distorted and flattened."

### Cause

A parry (`VR_Parry_Interrupt`, QC `combat.qc`) puts the monster in its first pain frame and holds it there for the
stagger (`vr_parry_stagger`, 0.75 s) by setting its next think to the stagger's end. The engine draws a frame change
over the time to the entity's next think (FitzQuake's lerpfinish: `sv_phys.c` sends it whenever that isn't 0.1 s;
`R_SetupAliasFrame` blends over it). So the monster was drawn morphing from the attack frame it was parried in to the
pain frame over the whole 0.75 s. An `.mdl` lerp is a straight line between two vertex sets, and between two unrelated
poses (an ogre's overhead smash and its pain crouch) the way passes through shapes much smaller than either: half the
size on an axis. Quake's own lerps pass through such shapes too, but in 0.1 s (a pain cutting into an attack). Here it
was held at that size for half a second. Not a pose index, frame group or ragdoll problem: the poses were in range and
the matrix was even every frame.

### Fix

`VR_Parry_Hold` (combat.qc): a staggered monster's think runs every 0.1 s until the stagger ends (`VR_Parry_Recover`
checks the time and re-arms), never once at its end. Its frame lerp is Quake's 0.1 s again: it snaps into the pain pose
and holds it. The three holds that set the think to the stagger's end use it: the interrupt itself, `ai_run`'s guard
(pain during a stagger) and the dragon's `dragon_check_attack`. The stagger's length, its recovery and the cancelled
hits are unchanged.

### Debug: Monster Poses (`vr_debug_pose_check`, Debug > Logs)

`vr_posecheck.cpp`, called by `R_DrawAliasModel` on each standard draw of a monster `.mdl` (20+ frames, not a player
or a view model): the drawn blend's extents against the smaller of its two poses' and against the model's smallest over
all its poses. A frame under 0.8 (or 0.85 of the model's) is squashed. 1 logs a squash held over 0.2 s ("posecheck: ...
squashed between its poses too long", then "squashed N s in all (worst r)"), a pose out of the model's range, a blend not
in 0..1, a matrix scaling it unevenly. 2: every frame of every monster (two lines a frame: both eyes). 3: as 1 with a
screenshot at each. `vr_debug_pose_check_model progs/ogre` narrows it to one model.

### Tests (`Misc/quakevr/parry_pose_test.sh <agent> [frames] [kinds]`)

vrcalibration, `god`, the stealth meter off, the crowbar held across, each kind attacking for 2400 frames, every blow
parried (`vr_parry_stamina 0`):

| Kind | Parries | Held squashed before | After |
|---|---|---|---|
| Ogre (1) | 24 | 20, 0.37-0.59 s each, to 0.49 of its size | 0 |
| Overlord (17) | 24 | 7, 0.47 s (0.72) | 0 |
| Hell knight (6) | 18 | 4 | 0 |
| Death knight (33) | 18 | 4 | 0 |
| Knight (5) | 2 | 2 | 0 |
| Dog (7), fiend (9), phantom swordsman (15) | 24, 25, 20 | 0 | 0 |

The parry counts are the same before and after. An ogre parried mid-smash (frame 51 to its pain frame 67): before, the
blend ran 0 to 1 over 3.12..3.78 s (squashed to half, 0.50, for 0.5 s); after, over 3.12..3.22 s. Pictures (the
worktree's scratch): `before_ogre.png` (top left: the ogre 0.2 s into it, head sunk into its shoulders, its saw arm
folded into its body), `after_ogre.png` (the same moment: the pain pose).

## OpenXR runtime: Auto (2026-10-09)

Your request: choose the OpenXR runtime automatically, the one whose app is running, and try the others when it fails.

### What it does

VR Settings > Headset > OpenXR Runtime has a new choice, **Auto** (`vr_xr_runtime 4`), now the default; System default
(0), Virtual Desktop (VDXR) (1), SteamVR (2) and a manifest (3, console) stay, and each of those loads that runtime only.
Config version 108 moves a config still at the old default (0) to Auto; one at 1, 2 or 3 keeps it.

Auto (`Quake/vr/vr_xr_runtime.cpp`) reads the installed runtimes (HKLM `...\OpenXR\1\AvailableRuntimes`, enabled ones;
Virtual Desktop's, SteamVR's and Meta's usual places when not listed), the system's active one (`ActiveRuntime`) and the
running processes, and orders the runtimes whose manifests exist:

1. Virtual Desktop (VDXR) while `VirtualDesktop.Streamer.exe` runs. Whether a headset is connected through the Streamer
   isn't visible from outside without loading VDXR, so VDXR's own `xrGetSystem` is that check: with no headset it fails
   and the next is tried (the Streamer often runs in the tray of a PC whose headset is on SteamVR).
2. The system's active runtime, when its app runs (the tie between SteamVR and Meta both running).
3. Meta's (`OVRServer_x64.exe`), then SteamVR's (`vrserver.exe` or `vrmonitor.exe`), when running.
4. The system's active runtime (with nothing running, it is first: "nothing running: the system's active runtime").
5. The others installed: VDXR, Meta, unknown runtimes; SteamVR's only with `vr_xr_runtime_fallback 2` (trying an idle
   SteamVR starts SteamVR and its windows, only to find no headset).

The backend (`OpenXrBackend::start`) tries them in turn: any failure (`xrCreateInstance`, `xrGetSystem`, the session,
the swapchains) stops and destroys everything and goes on to the next; after the last, VR is off (flat), as before.
`vr_xr_runtime_fallback 0` plays flat after the first. An `XR_RUNTIME_JSON` the game was started with still wins (that
runtime only). The console says the choice and why (`OpenXR runtime choice: Auto: Virtual Desktop (VDXR) - Streamer
running`), each attempt (`OpenXR: trying SteamVR (running): <manifest>`) and its failure; the menu shows the outcome
under the row (`Auto: SteamVR - running (Virtual Desktop (VDXR) failed)`, or `Auto: none started (flat)`). Restart VR
chooses again (after starting or closing a VR app).

### The loader and more than one runtime in a process

The OpenXR loader (1.1.63, vendored) loads the runtime at the first call that needs it and unloads it when its last
instance is destroyed (or `xrCreateInstance` fails); it reads `XR_RUNTIME_JSON` again at the next load. Verified with a
fake runtime DLL (`Misc/quakevr/fakexr`): in one process the game loaded VDXR's fake, failed its `xrGetSystem`,
destroyed it (the DLL unloaded), loaded SteamVR's (its `xrCreateInstance` failing), then Meta's, each DLL loaded and
unloaded in turn. Real runtimes may keep threads or services of their own past `FreeLibrary`; Restart VR has always
done the same unload and reload, so this is no new path.

### Debug and tests

`vr_xr_runtime_explain` (Debug > Reports > OpenXR Runtime Choice): what Auto sees and the order, without loading
anything. `vr_xr_test*` fake the system and fail attempts (`vr_xr_test_fail`, also with real runtimes);
`Misc/quakevr/xr_runtime_test.sh <agent>`: 25 headless checks (TESTING.md, "OpenXR runtime choice"). The installer's
Virtual Desktop note now says the game picks VDXR by itself while the Streamer runs.

### To try in the headset

- Virtual Desktop connected, Streamer running, SteamVR closed: the game starts on VDXR (console: `Auto: Virtual Desktop
  (VDXR) - Streamer running`; the menu line under OpenXR Runtime says the same).
- Virtual Desktop connected and SteamVR running (started from VD): still VDXR.
- The Streamer running without the headset connected, SteamVR running with another headset (or Link with the Meta app):
  VDXR fails (`xrGetSystem`), then SteamVR (or Meta) starts. How long VDXR takes to say no is worth a note.
- Quest Link only (Meta app running, VD closed): Meta's runtime.
- `vr_xr_test_fail virtualdesktop; vr_restart` with VD connected and SteamVR running: SteamVR takes over in the same
  session (the loader's reload with real runtimes); `vr_xr_test_fail ""; vr_restart` goes back to VDXR.

### Virtual Desktop's own runtime setting (2026-10-09)

Your question: the Streamer lets you choose VDXR or SteamVR as the OpenXR runtime; does Auto see it? It does now.

Where VD keeps it: `"OpenXRRuntime"` in `%ProgramData%\Virtual Desktop\StreamerSettings.json`, a number of the
Streamer's enum `VirtualDesktop.Interfaces.OpenXRRuntime`: 0 Automatic, 1 SteamVR, 2 VDXR (read from the .NET metadata
of `VirtualDesktop.Streamer.exe` 1.34.23; yours is 2, VDXR). Nothing in the registry under `Virtual Desktop, Inc.`
holds it. The Streamer hands the choice to VD's service (`SetOpenXRRuntime`, in `VirtualDesktop.Service.exe`), which
very likely sets the system's `ActiveRuntime` to match (yours is VDXR's), but its strings are obfuscated, so whether it
switches `ActiveRuntime` to SteamVR's (and what Automatic does exactly) wasn't verified; the file says the choice
directly anyway. VD sets no `XR_RUNTIME_JSON` that the game could see (none in the Streamer's strings).

What Auto does with it, only while the Streamer runs (`Quake/vr/vr_xr_runtime.cpp`, `autoOrder`):

- SteamVR: SteamVR's runtime first, running or not (loading it starts SteamVR, which reaches the headset through VD's
  driver), then VDXR (`Auto: SteamVR - Virtual Desktop set to SteamVR`). SteamVR not installed: VDXR (`Streamer
  running, set to SteamVR (not installed)`).
- VDXR or Automatic: VDXR first, as before (`Streamer running, set to VDXR`).
- File missing, no key, an unknown value: as before (`Streamer running`).

`vr_xr_runtime_explain` prints the setting and where it came from (`Virtual Desktop's OpenXR runtime setting: VDXR:
VDXR first` / `OpenXRRuntime 2 in C:\ProgramData\Virtual Desktop\StreamerSettings.json`); the menu line under OpenXR
Runtime carries the reason (`Auto: SteamVR - Virtual Desktop set to SteamVR`). `vr_xr_test_vd_runtime` overrides it
(also without `vr_xr_test`): a number (-1 unknown, 0 Automatic, 1 SteamVR, 2 VDXR) or another StreamerSettings.json to
read; with `vr_xr_test 1` and the cvar empty, no file is read. `xr_runtime_test.sh`: 14 new checks (39 in all, 0
failed), the parser on fake files (a number, a name with CRLF, no key, no file) and SteamVR's fake loaded before
VDXR's through the real loader. On this PC (headless, real system): `OpenXRRuntime 2` read, VDXR first.

To try: in the Streamer's Options pick SteamVR as the OpenXR runtime, connect, start the game (Auto): SteamVR starts
and runs the game (console: `Auto: SteamVR - Virtual Desktop set to SteamVR`); pick VDXR again: VDXR, no SteamVR.
Worth a note: what Automatic does on VD's side, and whether `ActiveRuntime` changes when you switch (Debug > Reports >
OpenXR Runtime Choice shows the system's active runtime).

### "It always loads VDXR; forcing SteamVR crashes" (2026-10-09)

Your report: VD switched to SteamVR, SteamVR started, the game still loads VDXR; Auto again picks VDXR; forcing SteamVR
(`vr_xr_runtime 2`) crashes. Found from your four crash dumps of 16:45-16:48 (`%LOCALAPPDATA%\CrashDumps`) and the
`qvr_crash.txt` in the Steam Quake folder (your build of 16:41, `45b660ad-dirty`: it has the VD setting reader).

- **VDXR at every start: your Visual Studio debugger arguments.** `Windows/VisualStudio/ironwail.vcxproj.user`
  (every configuration) has `-basedir "...\Steam\steamapps\common\Quake" -game quakevr +vr_xr_runtime 1`. `+` commands
  run after the config, so each launch forces VDXR over the menu's Auto (your config holds `vr_xr_runtime "4"`).
  Remove `+vr_xr_runtime 1` from Project Properties > Debugging > Command Arguments. The game now says so:
  `vr_xr_runtime_explain` and the log print `set on the command line (+vr_xr_runtime 1): set again at every start, over
  the menu's choice`, the menu line reads `chosen on the command line`.
- **The crash: VDXR's d3d11.dll unloaded under NVIDIA's OpenGL driver.** All four dumps: an access violation in
  `nvapi64_impl.dll`, called from a `nvoglv64.dll` thread (no game code on the stack), reading `0x7ff91f799148`, inside
  the unloaded `d3d11.dll` (`0x7ff91f5c0000-0x7ff91f81f000`; the unloaded list: `virtualdesktop-openxr.dll`,
  `VirtualDesktop.LibOVRRT64_1.dll`, `XR_APILAYER_NOVENDOR_OBSMirror`, `d3d11.dll`, `D3DCOMPILER_47.dll`). VDXR renders
  with D3D11 and shares the game's OpenGL images through NVIDIA's GL/D3D interop; leaving (any VR restart away from
  VDXR: SteamVR chosen, Auto chosen, Auto's fallback past VDXR) frees d3d11.dll and the driver's thread reads it later.
  Fix: before a runtime is unloaded the game keeps `d3d11.dll`, `dxgi.dll`, `d3d12.dll`, `vulkan-1.dll` loaded for
  good if loaded (`GetModuleHandleEx` pin; `vr_xr_keep_graphics_dlls 1`, 0 the old way, for tests). Not reproducible
  headless (no NVIDIA interop in the fake runtime); the fake runtime loading and freeing d3d11.dll shows it unloads
  without the pin and stays with it.
- **Auto picking VDXR after you chose it in game:** Auto put SteamVR first (headless with your real files, Streamer
  running: `Auto: SteamVR - Virtual Desktop set to SteamVR`, then VDXR, then Meta), so SteamVR failed or the switch
  crashed. One likely failure: SteamVR just started says there is no headset (`XR_ERROR_FORM_FACTOR_UNAVAILABLE`) until
  VD's driver finds it, and Auto went on to VDXR at once. Now SteamVR is asked again for `vr_xr_steamvr_wait` s (5;
  Advanced > Headset > SteamVR Wait) while it says so.
- **A log of every VR start:** `quakevr/qvr_openxr.txt` (rewritten at the game's first VR start, added to at each
  restart, each line written at once): the command line, `XR_RUNTIME_JSON` as the game started and as it is now, the
  graphics DLLs loaded, `vr_xr_runtime_explain`'s report, each attempt (`XR_RUNTIME_JSON` set, the runtime the loader
  loaded by name, whether that is the manifest's library, else a warning), every failed call with its code, the DLLs
  kept, the stops. Debug > Reports > OpenXR Runtime Choice says where it is.

VD sets no `XR_RUNTIME_JSON` (none at the game's start in your dumps' command line or now); `ActiveRuntime` is now
SteamVR's (VD's service switched it); SteamVR isn't listed under `AvailableRuntimes` (found by its active entry and
its usual place). `xr_runtime_test.sh`: 49 checks, 0 failed (new: d3d11.dll kept and not, the log, SteamVR asked again
4-6 times in 1 s and VDXR once, the command-line notice, a relative `library_path`).
## The gadget's screen tap and side button in sync with the drawn gadget (2026-10-09)

His notes vrfiringrange_2026-10-09_11-01-21, 11-05-38: moving or turning with the stick, the side button and the
bullet-time tap zone lag behind or run ahead of the drawn gadget, so presses don't match what he sees.

**Why.** Both were tested in `VR_BeginFrame` against `gadget::pose()`, which the view sets while it draws: the gadget of
the frame before, against the hands of this one. Running at full speed that put the tapping hand about 4 to 9 cm off
the drawn screen on average (15 cm at most), smooth turning up to 31 cm, a snap turn up to 37 to 50 cm (the whole arc of
the turn for one frame). The debug drawing had the same lag (drawn before the gadget was placed).

**Now.** `gadgetTouches` (vr_view.cpp) runs once a frame right after `setupGadget`, in `VR_SetupViewEntities`: the
screen tap (`bullettime::viewFrame`) and the side button (`gearlights::viewFrame`) are tested against the gadget placed
this frame and the hands as the view draws them (after the move and the turn, knocks, body collisions; the index
fingertip as drawn this frame). Not while posing. The debug drawing follows, as tested. `VR_BeginFrame` keeps only the
lights' easing. The mock's targets (`vr_mock_hand_to ... screen|button`) are taken from the gadget as last drawn: the
hand's move since then (`bullettime::movedSinceView`) is allowed for, so a target set while running lands where asked.

**Debug.** `vr_debug_gadget_button 3` (Show Gadget Button: And Print Sync) prints each frame how far the zone is from
the screen on the drawn gadget entity (its origin, angles and scale) and how far off the old frame-start test would have
been. `Misc/quakevr/gadget_sync_test.sh <agent>` (summed up by `gadget_sync_summary.py`): standing, running,
strafing while smooth turning, snap turning, jumping while running: the zone is 0.0000 cm off the drawn screen in every
frame (the old test: the numbers above); then taps while running, running and turning, strafing and turning, and a
button press while running and turning, all counted.

**To try in the headset.** Run, strafe and turn (smooth and snap; a lift) while tapping the screen and pressing the side
button: both should land exactly where the drawn gadget is.

## The screen tap: the whole hand and the gun's butt strike (2026-10-09)

His note: with an empty hand the whole hand (palm, fingers, knuckles, back of the hand) should tap; with a gun, its
butt and the hand. It was two points: the palm's middle (`hands::palmPoint`, inside the hand: hence the old 6 cm depth)
and the middle of the gun's rearmost unit (`view::heldWeaponButt`).

**Now** (vr_bullettime.cpp `strikers`): the striking volume is the hand as drawn this frame, its mesh's vertices
(`view::drawnHandSurface`: the jointed hand posed as drawn, 455 points; the old six models: three 3.2 cm spheres along
the hand), plus, holding a gun with `vr_bullettime_tap_butt`, every drawn point of the gun within
`vr_bullettime_tap_butt_depth` (4 cm) of its rearmost end (`view::heldWeaponButtRegion`). Each point has its own
velocity (the hand's, its turn included) and a radius (0 for a vertex). The rules are the same, per point: over the zone,
straight in (`vr_bullettime_tap_angle`), fast enough (`vr_bullettime_tap_speed`), the peak taken over all points; the
impact: any point on the face (within `vr_bullettime_tap_depth`, now measured from the surface; his 6.5 cm stays the
default, config 109) while the fastest point slowed (`vr_bullettime_tap_stop`). The message names what struck, by the
part its vertex follows most (the rig's joints; the palm's vertices by side: the palm or the back of the hand):
"screen tapped by the knuckles (and the fingers)". Each point's part: `view::HandPart`.

**Debug.** Show Gadget Button: And the Screen Tap draws the striking volume (the hand's surface light blue, the gun's
butt orange, any point on the zone white; a sphere's radius as a ring). `vr_gear_lights_info` prints the way into the
screen in the tapping hand's frame and the volume's size; `vr_debug_bullettime 2` the nearest point's part and the
butt's distance. The mock: `vr_mock_hand_to <hand> screen <cm> [<side>] [<part>]` places that part's point nearest the
screen (palmskin, back, knuckles, fingers, tips, thumb, butt).

**Tests** (`Misc/quakevr/gadget_tap_test.sh`, cases H, I, E2): the hand turned so one part leads, then a 3.3 m/s strike:
a flat palm slap (the fingers' pads land first), the back of the hand, the fingertips, the thumb, the knuckles of a fist,
the gun's butt leading all tap, each named; swings across 3 cm over with the back of the hand or the knuckles, and the
fingertips 50 degrees off straight, don't. The old cases (A..G) unchanged.

**His settings** (the defaults since config 109): `vr_bullettime_tap_depth 6.5` and `vr_bullettime_tap_z -3.5`, tuned
for the palm's middle: measured from the surface they are more forgiving (the zone 3.5 cm into his arm, the hand's
surface within 6.5 cm over it, so about 3 cm over the face). Kept; he may want the depth nearer 3 now.

**To try in the headset.** Tap the screen with the palm, the fingertips, the knuckles, the back of the hand, the gun's
butt; swing across the screen with each (no tap). With And the Screen Tap on, the blue points should sit on the drawn
hand.

## The screen tap: a double tap (2026-10-09)

His request: a Double Tap gesture for bullet time, two taps on the gadget's screen in quick succession, with an empty
hand, a gun (the hand and its butt) or a prop (the prop and the hand).

**Settings.** `vr_bullettime_tap_gesture` 0 Single Tap (default, as before) / 1 Double Tap;
`vr_bullettime_tap_double_window` 0.4 s (the second tap within it of the first); `vr_bullettime_tap_double_speed`
0.4 m/s (each tap's least speed into the screen, instead of `vr_bullettime_tap_speed`, his 0.5). Menu: Combat > Bullet Time >
Screen Tap: Gesture, Double Tap Window, Double Tap Force. Single stays the default: a double tap is slower to start bullet
time in a fight, and the single tap's rules already keep accidents out; the author can switch.

**How.** Each tap follows the single tap's rules (straightness, stop, window, the zone and depth) at the double tap's
speed. The first starts the window (a faint tick in the tapping hand; `bullet time: first tap (...)`), the second
toggles bullet time (`double tap: screen tapped by ...`); none in time: `no second tap within 0.40 s`. Between taps the
volume must lift: off the face, or drawn back out of it at 0.3 m/s or more (in both gestures now: a bounce, or a hand left
resting on the screen, is never a second tap). A held prop's shape (`view::heldPropSurface`: its grasp shape as drawn)
is now part of the striking volume, with the hand ("the held prop").

**Tests.** `Misc/quakevr/gadget_doubletap_test.sh <agent>` (the old thresholds pinned, as gadget_tap_test.sh's, and a
double tap's 0.8 m/s): two quick taps (1.7 m/s, 0.2 s apart) turn it on and again
off; two lighter taps (1.0 m/s, under the single tap's 1.2) on; one tap and two taps 0.8 s apart do nothing; two swings
across nothing; with a gun (knuckles and butt) and holding a health pack, on. The single-tap test is unchanged.

**To try in the headset.** Gesture: Double Tap; double tap with the palm, the knuckles, the gun's butt, a held box; a
single tap and a slow pair should do nothing; melee swings across the gadget should never count.
## The menu sharp in the headset (2026-10-09)

His note (vrstart_2026-10-09_10-42-28): in VR the menu looks blurry, as if smoothed.

**How it was drawn:** the 2D pass (menus, console, head-locked text) went into an offscreen canvas the size of the
desktop window (`vid.width` x `vid.height`, vr_panel.cpp), sampled bilinearly, no mipmaps, onto the panel quad in each
eye image (after the post-processing, at the image's full size: no MSAA, foveation or upscale on it), and the runtime
resamples the eye image once more for the lenses. So the menu's sharpness depended on the window: a 1920x1080 window
gives the shipped panel (115 x 205 units at 100: about 60 degrees tall) about as many pixels as a native Quest 3 eye
image, but fewer than Virtual Desktop's higher resolutions (magnified 1.3-1.5x: smoothed), and the test runs' 960x540
window drops glyph pixels outright (a menu pixel 0.84 canvas pixels: "Dack Iolsters"). The menu's 8-pixel letters are
also nearest-scaled by a non-whole factor (1.69 canvas pixels a menu pixel at 1080), so their strokes alternate 1 and
2 pixels before the smoothing.

**Now** (`vr_menu_resolution`, 1.5; `vr_menu_sharpen`, 0.5; VR Settings > Advanced > Menu Settings: Menu Resolution,
Menu Sharpening): in the eyes the canvas is sized from the eye images' own pixel density (their fov's tangents and
height, noted each eye: `panel::noteEyeImage`): `vr_menu_resolution` canvas pixels to each eye pixel across the panel
seen head-on (the menu panel's height or the in-game text panel's, the larger, so that opening a menu doesn't remake
it), in steps of 64 rows, at most 4096 across (its shape the window's: the 2D pass lays out on the window's virtual
screen). It has mipmaps, rebuilt each frame, sampled trilinearly with 8x anisotropy and a mipmap bias of
`-vr_menu_sharpen`. 0 keeps the old window-sized canvas. The engine's 2D pass takes the canvas's pixels for its
viewport, clip rectangles and its sub-pixel shift (`VR_CanvasPixels`: GL_Set2D, Draw_SetClipRect, Draw_Transform2);
the runtime's own panel before a map keeps the window's size.

**Measured** (mock eyes 2048 at the mock's 92-degree fov, his menu settings, eye images with the UI, a 640x420 block of
menu text; edge steepness = the mean of the steepest 1% of luminance steps, higher sharper; spread = how much it changes
over four 0.26-pixel head turns, the shimmer):

| canvas | edge steepness | spread |
|---|---|---|
| window 960x540 (test runs' window) | 63.7 | 0.3% |
| 0.7 eye pixels (his 1080 window at a Virtual Desktop-like eye) | 76.3 | 0.1% |
| 1.06 (his window at a native Quest 3 eye) | 87.8 | 0.7% |
| 2, no sharpening | 69.2 | 0.7% |
| **1.5, sharpen 0.5 (new default)** | **94.2** | **0.4%** |
| 2, sharpen 1 | 100.7 | 1.0% |
| 3, sharpen 1 | 87.4 | 0.3% |

Supersampled with plain trilinear (2, no sharpening) is softer than 1:1: its mip level 1 is bilinearly magnified again.
GPU: the 2D pass and the panel's draw cost the same within noise at 0, 1.5 and 2 (`vr_profile`, menu open: 2D 1.64,
1.77, 1.37 ms GPU, noise; hud panel 0.05 ms).

**Fixed on the way:** `gfx::ensureTarget` (and releaseTarget, destroyTexture) deleted textures with glDeleteTextures,
leaving the engine's bound-texture cache holding the name; glGenTextures handed the same name back, GL_BindNative
skipped the bind, TexStorage went to the default texture and the remade target was incomplete (drew nothing: the canvas
went black after its first resize). Now GL_DeleteNativeTexture. A clip rectangle left on by the window's 2D pass is
turned off at the canvas's start.

**Not done (the next step if still soft):** the runtime resamples the eye image for the lenses (a second smoothing no
eye-image setting avoids). A quad composition layer for the menu (the runtime samples the canvas itself, once, at the
display's pixels; Meta's advice for text) would remove it, but the laser and its dot, drawn in the eyes, would then be
under the panel: they would have to move into the canvas.

### To try in the headset

- Open the menu: letters crisper than before, strokes even. Menu Resolution 0 vs 1.5 to compare; Menu Sharpening 0 / 0.5
  / 1 (1: crispest, may shimmer slightly on the small text as the head moves).
- The console and centre prints in game (same canvas): crisp too.

## Shimmer at the pier and the bridge (2026-10-09)

His note (vrstart_2026-10-09_10-43-52): edges shimmer a lot in VR, on the bridge's planks a bit further out; MSAA 8x,
Retro Textures off, Retro Lighting off and more anisotropy don't fix it. A limitation of the resolution?

**Measured** with `Misc/quakevr/shimmer_test.py` (new; TESTING.md): mock eyes 2048 (about a native Quest 3's pixels per
degree), his graphics settings (every r_/gl_/vid_fsaa/vr_ graphics cvar of his ironwail.cfg), at his spot on the pier
(-1201 -1195 48, along the pier, 8 degrees down), paused, the left eye at six head turns 0.03 degrees (half a pixel)
apart; screen dithering off. "Pops": the share of pixels changing by more than 16 (32) of 255 between turns, pixels
switching on and off rather than sliding: the shimmer. Region: the planks 200 to 600 units off (`--region`).

| setting (over his) | pops>16 | pops>32 |
|---|---|---|
| his (MSAA off) | 0.28% | 0.13% |
| Antialiasing 4x | 0.38% | **0.01%** |
| Antialiasing 8x | 0.19% | 0.02% |
| Render Scale 1.5 / 2 | 0.28% / 0.40% | 0.11% / 0.11% |
| Retro Textures off (his GL_NEAREST_MIPMAP_LINEAR) | 0.73% | 0.19% |
| Smooth Textures: All | 1.41% | 0.74% |
| Smooth All, Retro off, Parallax off | 0.14% | 0.12% |
| Bump Mapping off / specular 0 / bumps smooth (vr_retro_world_bump 0) | 0.26% / 0.28% / 0.22% | 0.13% |
| Parallax off, Retro Lighting off, Foveated off or Aggressive, retro soft 2, fade 4 | 0.25-0.28% | 0.13-0.14% |

**The cause:** the planks are separate brushes, 11 units wide with 1-unit gaps (vrstart_gen.py `deck`), over
darkness: from about 200 units the gaps are thinner than a pixel, so each is drawn as broken dashes that jump along
it as the head moves (the pops over 32). That is geometry, not textures: MSAA removes it (4x: 0.13% to 0.01%; 8x is
no better), supersampling barely (4 samples in a grid miss a line a quarter of a pixel thick). The textures add the
smaller pops (16-32): Retro Textures already halve them (off, with his nearest filtering: 2.6 times more); Smooth
Textures: All makes it worse (parallax mapping shows on the planks, wavy). Bumps, specular, parallax, retro lighting
and foveation change little.

**GPU** (`vr_profile`, the pier, 2048 eyes, the scene's "3D"): MSAA off 2.05 ms, 4x 2.96, 8x 5.47 (both eyes).

**His session's frame pacing** (his memstats_2026-10-09_10-42-04.csv, during these notes, 120 Hz): 116.8 frames a
second, 543 and 763 frames a minute late (8-11%), while the game used 1.8 ms of CPU and 3.8 ms of GPU a frame: the
misses are the runtime's or the stream's (Virtual Desktop), not the game's. A missed refresh is reprojected; thin
high-contrast lines are where that shows as edges wobbling, whatever the anti-aliasing. 8x's extra GPU time would make
misses more likely.

**Answer:** mostly the resolution (1-unit gaps at mid range are under a pixel) and fixable in the game with MSAA: his
config has it off (`vid_fsaa 0`); 4x, as shipped, removes almost all the gaps' crawl in these images. If it still
shimmers with 4x in the headset, the rest comes after the game: Virtual Desktop's sharpening (it sharpens the very
edges that crawl), its video encoding, and the reprojected frames above.

**Changed:** the Antialiasing rows' help says what it fixes and that 4x does most of it (VR Settings, Graphics >
Image). No engine change for this one: nothing in the game's shading measured as a cause worth a default.

**Not done (options, his call):** a subfloor under the decks (vrstart_gen.py `deck`: dark wood 1-2 units under the
plank tops instead of the void) would cut the gaps' contrast, and so their crawl at any MSAA, and what the stream's
encoder has to carry (needs the map rebuilt and relit). A temporal anti-aliasing pass is not warranted by these numbers
(after MSAA the pops left are small) against its ghosting under head motion. A tonemap-weighted MSAA resolve would help
edges against bright highlights; the pier's surfaces are mostly below 1 (luminance median 0.52, 90th percentile 0.81),
so little here.

### To try in the headset

- Antialiasing 4x (VR Settings, Anti-aliasing): look along the pier and at the bridge, move the head slowly: the plank
  gaps should stay whole lines instead of dashes. Then 0 to compare.
- If it still shimmers: in Virtual Desktop, Sharpening at 0 and a higher bitrate; the menu's status box (Menu Settings >
  Status Box) shows whether frames are being missed.

## A shove's knockdown topples over the feet, as the judo throw (2026-10-09)

His ask: a ragdoll knocked down by a shove (one or two open palms) falls head and chest first along the shove as a
thrown one does, turning a bit less than the throw, its feet staying where they stood rather than sliding.

- **Shared code:** `box3d::ragdollTopple` (the throw's) does it; two new parameters, both defaulting to the throw's
  values so the throw is unchanged: `hold` (s its feet are held with no sweep; the throw 0.5) and `launch` (the share of
  the knockdown's launch its top keeps, its parts by height, the feet none; the throw 1). `foegrab::shoveTopple` reads the
  shove's cvars and reuses the throw's trace (`shove trace:` lines, `vr_knockdown_debug 1`). QC: `VR_Knockdown_Try` gets
  the shove's push (two hands 1, one `VR_BASH_ONE_PUSH` 0.7, a counter more, tired less) and calls the new builtin
  `ragdollshovetopple(e, dir, strength)` after `VR_Knockdown_Start`. Shoved over a ledge (`vr_knockdown_ledge`) it is
  pushed whole as before, so it still tumbles off.
- **Cvars (Combat > Knockdowns, under Launch):** `vr_knockdown_shove_topple` 180 deg/s (60% of the throw's 300, times
  the strength; 0 off: pushed whole as before), `vr_knockdown_shove_topple_push` 0.5 (Topple Push: with 1 the shove's
  push alone, all at the top, turned a grunt over as fast as the throw does: 74/128 deg at 0.1/0.2 s even at 1 deg/s),
  `vr_knockdown_shove_feet_speed` 0 (as the throw's Feet Speed), `vr_knockdown_shove_feet_hold` 0.3 s.
- **Numbers** (vrtesthall, 64 units ahead, `vr_knockdown_test 21`/`22`, torso tilt from upright at 0.1/0.2/0.3 s, the
  most; feet and pelvis travel along the shove after 1.5 s, units):

  | | off (before) | topple 180, push 0.5 |
  |---|---|---|
  | grunt 1 hand | 10/12/10, most 96; feet 108, pelvis 115 | 28/81/86, most 95; feet 4, pelvis 14 |
  | grunt 2 hands | 17/17/19, most 133; feet 210, pelvis 196 | 71/110/85, most 116; feet -2, pelvis 6 |
  | knight 1 hand | 8/10/29, most 91; feet 94, pelvis 110 | 37/79/88, most 95; feet 5, pelvis 24 |
  | knight 2 hands | 8/9/14, most 127; feet 230, pelvis 214 | 50/120/95, most 120; feet 7, pelvis 27 |
  | enforcer 1 hand | 23/33/54, most 107; feet 106, pelvis 110 | 39/73/87, most 89; feet 22, pelvis 29 |
  | enforcer 2 hands | 11/11/21, most 168; feet 253, pelvis 238 | 51/101/89, most 101; feet 14, pelvis 21 |
  | judo throw, grunt | 71/126/145, most 147; feet -28 (swept back) | (unchanged) |

  So a shoved-down enemy no longer slides 100-250 units along the floor: it falls over where it stood, its head about
  30 units further on. Topple Push brings back more travel (and a faster turn).
- **Tests:** Debug > Tests, "Shove the Nearest Down, One Hand" / "Two Hands" (`vr_knockdown_chance 100;
  vr_knockdown_test 21` / `22`: a real `VR_Bash_Hit` shove of the nearest monster).
- **To try in VR:** shove grunts, knights and enforcers down with one hand and with two (Knockdowns' Chance 100): head
  and chest go first along the shove, the feet stay put, a two-handed shove turns them faster; a shove off a ledge still
  sends them over.
## Texture deletes and the engine's bound-texture cache (2026-10-09)

Follow-up to "The menu sharp in the headset": bloom (`vr_bloom.cpp` destroy), the shadow atlases (`vr_lighting.cpp`
destroy), the wound masks (`vr_wounds.cpp` releaseTexture, ensureWashStencil) and Ironwail's light clusters
(`gl_rlight.c`) deleted textures with a raw glDeleteTextures. GL_BindNative caches the texture on units 0-3; a deleted
name left there and handed back by glGenTextures (the driver gives the same names back at once: bloom's 861..867 again
after a resize) makes the next bind of it on that unit a no-op while GL has 0 there. All now GL_DeleteNativeTexture.

**Debug > Logging > Graphics State** (`vr_debug_glstate 1`): each skipped bind is checked against GL's binding
("texcache: stale bind #n skipped"), and once a frame each unit's cached texture is checked against GL ("texcache: stale
entry"). Headless, with the cache made to hold a bloom target as it is remade (a temporary bind, not committed) and
vr_render_scale 1 -> 0.7: before, "stale bind #1 skipped: unit 0, texture 861 (GL has 0)" and "VR bloom: framebuffer
incomplete" (bloom off until restart); after, none. The real flows (render scale 1/0.7/1, vr_shadow_atlas 2048/4096,
vr_wounds_own_res 512/0, vid_restart) print nothing either way: in them something else had bound those units first, so
the fix closes a latent case.

**The clip rectangle.** No path in the engine or our QC leaves it on today: Draw_SetClipRect's users (the scoreboard's
scrolling level name, CSQC's drawsetcliparea) pair it with Draw_ResetClipping, and the VR passes (shadow atlas, haze,
trails, upscale) turn theirs off; the shadow pass's own glDisable each frame would also hide a leak from the eyes. A
mod's QC calling drawsetcliparea without drawresetcliparea would leave it on, and then the canvas's draw into the window
(menus, console) is cut to it. Now each 2D pass starts unclipped (GL_Set2D) and the pass ends with Draw_ResetClipping
(SCR_UpdateScreen), besides beginCanvas's own. `vr_debug_glstate 1` prints "glstate: clip rectangle left on at the
frame's end". Headless, e1m1 with the console down and a clip rectangle injected at the 2D pass's end (temporary, not
committed): before, 85 "left on" lines and the console gone from the window (8.1% of the shot differing, its top half);
after, none, and the shot identical to one without the injection (0 pixels).

## A parried monster staggers alive, not frozen (2026-10-09)

Your note (vrfiringrange_2026-10-09_12-45-15): the squash fix (above, "Parried monsters drawn squashed") made the
staggered enemy look like a freeze frame: it snapped into its first pain frame and held it for the whole stagger.

Now (`VR_Parry_Pose`, combat.qc) the stagger steps through the first `vr_parry_stagger_frames` (3) frames of the pain
animation its th_pain started, a tenth of a second each (consecutive frames only: no long lerp between unrelated poses,
so no squash), then rocks back and forth over the last two, 0.15 s a frame, until it recovers. Its body also sways,
dazed: roll up to `vr_parry_stagger_sway` (3) degrees and pitch 0.6 of it, two slow sines out of step (the angles,
lerped by the engine as its moves are), set back when it recovers, dies or runs again. A th_pain that doesn't change its
frame (the spawn, the Guardian, a refused pain), and the dragon, keep the old hold; the dragon never sways (it banks).
Every melee monster's pain animation is 3 frames or more (the knight's shortest), so 3 stays inside it. Menu: Combat,
under Parry Stagger: Stagger Frames (1 is the old hold), Stagger Sway.

Tests: `parry_pose_test.sh` (2400 frames each, every blow parried): ogre 24 parries, overlord 24, hell knight 18, kind 33
18, knight 2: 0 held squashed, 0 broken poses for all. The ogre's frames (vr_debug_pose_check 2): parried in its smash
(51) to pain 67, then 68, 69, 68, 69, 68 at 0.10/0.10/0.15/0.15/0.15 s, recovered to its run; its painb (72-74), painc
(75-77), paind (81-83) the same; the flattest drawn blend 0.81 (the smash-to-pain step, 0.1 s, as before). Pictures:
the worktree's `scratch/parry_after_lit.png` (fullbright, 0.1 s apart).

## A shove over a ledge turns the body as it falls (2026-10-09)

His note: a shove that knocks an enemy over a ledge skipped the topple (above, "A shove's knockdown topples over the
feet"), so its ragdoll went over stiffly upright, not turning at all.

Now `ragdollshovetopple(e, dir, strength, ledge)` gets the ledge (QC `VR_Knockdown_Try`), and `foegrab::shoveTopple`
calls `box3d::ragdollTopple` with a new `whole` mode: every part keeps all of the shove's launch and none is held (so
its feet go over the edge with the rest), and the turn about the floor under its feet is added on top, at
`vr_knockdown_shove_ledge_topple` (150 deg/s, times the shove's strength as the plain topple; 0: as before, no turn).
Menu: Combat > Knockdowns, **Topple Over a Ledge**, after Topple Feet Held. The plain topple is unchanged.

Numbers (vrclimb, a grunt 48 units from the trench's edge, `vr_knockdown_test 22`/`21` with `vr_knockdown_debug 1`,
the shove trace's torso tilt / feet travel along the shove, feet height, every 0.1 s):

| | 0.1 s | 0.3 s | 0.5 s | 0.7 s | 0.8 s | lands |
|---|---|---|---|---|---|---|
| before (0), two hands | 17 / 40, +5 | 17 / 111, -8 | 17 / 182, -52 | 17 / 252, -126 | 17 / 291, -183 | tilt 130 after the landing, feet 403 on |
| 150, two hands | 9 / 45, +6 | 31 / 126, -6 | 70 / 208, -47 | 101 / 298, -124 | 110 / 341, -172 | 98, feet 415 on, in the trench (-214) |
| 150, one hand | 6 / 31, +2 | 17 / 83, -12 | 41 / 136, -57 | 66 / 192, -130 | 79 / 224, -185 | 89, feet 268 on, in the trench |

It goes over as far as before (further: the turn's push at its top) and turns over steadily as it falls, landing on
its back or front rather than tipping only when it hits the floor.
## The author's settings of the afternoon of 2026-10-09 are the defaults (2026-10-09)

His notes vrfiringrange_2026-10-09_12-29-53, 12-40-36, 12-42-10 ("all bullet time values"). His config of 13:06
against the shipped defaults (vr_cvars.inc with vr_defaults.cfg over it, the engine's own for the rest). Config version
110 (`vr_cvars.cpp` defaultChanges: a config still holding the old default takes the new one; one the player changed
keeps it):

- Bullet time's screen tap: `vr_bullettime_tap_gesture` 1 (Double Tap; was Single), `_angle` 80 (10 degrees),
  `_butt_depth` 6 (4 cm), `_depth` 5 (6.5 cm), `_double_speed` 0.2 (0.4 m/s), `_double_window` 0.8 (0.4 s), `_height`
  0.85 (0.75), `_margin` 0.5 (1 cm), `_width` 0.95 (1). Combat > Bullet Time's Double Tap Force bar starts at 0.1 m/s now.
- The stealth AI: `vr_stealth_graze` 72 (64), `_light_dark` 16 (20), `_light_bright` 64 (80), `_lose_time` 14 (20 s),
  `_meter_time` 0.5 (1 s), `_meter_decay` 0.15 (0.2), `_noise_blasts` 1.75 (1), `_noise_guns` 1.5 (1), `_noise_props`
  1500 (1200), `_noise_wall` 0.6 (0.5).
- Holding enemies: `vr_foegrab_drag` 20 (10), `_drag_speed` 300 (200), `_break` 20 (35 cm), `_leniency` 1 (1.5 cm).
- The engine: `gl_texture_anisotropy` 16 (Ironwail's 8), as `vr_default` in vr_defaults.cfg's engine section with a
  110 change for configs at 8 (the driver's most caps it: gl_texmgr.c).

Already shipped, nothing to change: `vr_messages_hologram_only` 1 and `_height` 10 (vr_defaults.cfg since 2026-09-28),
`r_wateralpha` 0.3, `r_lavaalpha` 0.9, `r_slimealpha` 0.6, `r_telealpha` 0.9 (vr_defaults.cfg, config 45/60),
`host_maxfps` 250 (Ironwail's own default). host_maxfps and VR: with a headset the runtime paces the frames
(Host_GetFrameInterval); 250 only caps a frame's interval at 4 ms, never reached at 72-144 Hz, and over 72 the server runs
its fixed tick (host_netinterval), as before. r_telealpha and `vr_teleporter_surface_opacity`: multiplied
(GL_WaterAlphaForEntityTextureType): with seamless teleporters on, the shimmer's share is 0.9 x 0.3 = 0.27 over the view
through the gate; with them off, the shimmer at 0.9 over Quake's own surface.

Left as they are (machine, session, desktop or slider noise): `contrast` 1.2, `gamma` 0.95 (his display), `fov`,
`sensitivity`, `volume`, `vid_*` (his monitor), `scr_*scale` 3, `scr_menubgstyle`, `scr_centerprintbg`,
`ui_live_preview` (the desktop window's menus), `gl_texturemode` GL_NEAREST_MIPMAP_LINEAR (a look: Retro is its own
setting), `vr_body_elbow_back`/`_hand`/`_lift` (his arms: personal), `vr_menu_level`, `vr_menu_scale`,
`vr_menu_distance`, `vr_menu_positions`, `vr_mirror_hide_hud_text`, `vr_spectator_fov`, `vr_spectator_scale`,
`vr_window_view` (the desktop window), `vr_foveated`, `vr_xr_runtime`, `vr_comfort_vignette_strength` (0.4995),
`vr_ammo_pouch_scale` (0.999), `vr_ammo_pouch_x` (3.021975), `vr_melee_phase_speed` (3.996), `vr_melee_phase_time`
(0.34965), `vr_relight_strength` (1.1988): slider noise. Weapon and held object settings not compared.

Tests: `config110_test.sh` (a config of 109 at the old defaults takes every new one; vr_stealth_graze 50 and anisotropy
4 kept). `gadget_tap_test.sh` (also the single tap, width 1, butt depth 4), `gadget_doubletap_test.sh` (width 1, butt
depth 4) and `gadget_sync_test.sh` (single tap) pin what they measure against; all pass. The double tap cases with the
new defaults unpinned: J1, J4 (0.92 m/s), K, L on; J2, J3 (0.8 s apart: the window's edge), J5 nothing. stealth_tests.sh
gun, blast, hunt: PASS.

**VR Settings > Bullet Time: Screen Tap** (his note): the Single Tap / Double Tap choice (`vr_bullettime_tap_gesture`)
under Activation on the basic page too (also Combat > Bullet Time > Screen Tap > Gesture). `vr_menu_search screen tap`
finds it on VR Settings; `vr_menu_path_check maps/vrcalibration.map`: 0 missing.

**The screen tap's click and glitch** (his note): as a tap registers on the gadget's screen (a double tap's first, and
the tap that starts or stops bullet time, whether it then starts or is refused), the gadget clicks from its screen and
its screen glitches for a moment (vr_gadget.cpp `tapFeedback`, called from vr_bullettime.cpp `tapScreen`). The clicks
are synthesised (`make_sounds.py`: `vr/gadget_tap.wav`, a dry 2.6 kHz tick of 40 ms for the first tap;
`vr/gadget_tap_on.wav`, two blips rising 1.8 then 2.7 kHz, 0.11 s, for the activation), played on an entity number
of their own at the screen and kept there as the arm moves (as the message chime). The glitch is the CRT shader's own
(bands torn sideways, the colours split, the picture dimmed; drawn whether the CRT look is on or not), held then falling
over its last third: `vr_bullettime_tap_glitch` 1 (0 off .. 2; a first tap 0.7 of it), `vr_bullettime_tap_glitch_time`
0.12 s (the activation a quarter longer), `vr_bullettime_tap_sound` 0.6 (volume, 0 off). Combat > Bullet Time > Screen
Tap: Tap Click Volume, Tap Glitch, Tap Glitch Time, Try: First Tap, Try: Activation Tap; Debug's Screen Tap Feedback
(`vr_bullettime_tap_feedback_test [1]`). `vr_debug_bullettime 1` prints each feedback and the frames its glitch was
drawn in. Test `gadget_tap_feedback_test.sh` (with `-Sound` the clicks load and play): a double tap gives the first
tap's (glitch 0.70 for 0.12 s, 60 draws) then the activation's (1.00 for 0.15 s, 76 draws); Single Tap the activation's
only; both off: no click, no glitch drawn; the test command both. A headless shot before, during and after: the screen
torn only during.
## An ejected magazine stays out, whatever the other hand holds (2026-10-09)

The author's note vrfiringrange_2026-10-09_12-33-54: the super nailgun in the main hand, the nailgun in the off hand,
B/Y on the off hand: the magazine popped out and straight back in. The cause: a magazine just out of its gun is
`.vr_ammo_fresh` (not back in by contact until it has left the well), and VR_Reload_LooseFrame cleared that per hand,
from the hand's own gun's load point: the main hand's gun, far from the falling magazine, called it gone, and the next
frame the off hand's gun took it back. Any gun that loads by hand in the other hand did it, the same gun in both hands
too (and the super shotgun's live shells thrown out on breaking it open could go back the same way); with the other hand
empty it didn't (its frame does nothing). Headless, 17 pairs (each of the nailgun, super nailgun and thunderbolt in the
off hand with each in the main, either hand's B/Y, the other hand empty): before, 7 went straight back in (every
nailgun's eject with a gun in the other hand); after, none.

Now `VR_Reload_FreshLeft`: fresh only clears once the round is away from the load points of both of the player's guns
(their radius, the magazine's, twice Loose Leniency and 4 units) and `vr_reload_eject_cooldown` (0.35 s; the Reloading
page, "Ejected Stays Out") has passed; a round from another player's gun keeps the cooldown alone. A gun lying about
takes none in that cooldown either (ejecting over a gun on a table). `VR_Reload_MarkFresh` stamps every way out (B/Y,
the pull into the hand, the knock-out, the bump, the super shotgun's live shells). Brought back to a well after that, it
seats as before (contact_test.sh, reload_test.sh unchanged).

## An ejected magazine leaves as it sat (2026-10-09)

The author's note vrfiringrange_2026-10-09_12-34-55: the super nailgun's magazine fell out flat, not as it sat in the
gun's side. The round was made at the hand's angles (`VRGetEntHandRot`), which are view angles (pitch down positive)
given to a model (pitch up positive), and say nothing of how the magazine sits: right for the nailgun's and the cell's
(under the gun) only with the gun level, its pitch mirrored otherwise (headless, the hand pitched 49 degrees: the
nailgun's feed end 86 degrees off the seated one's), and the super nailgun's (in the side, 21 degrees up) always wrong.
Now `VR_Reload_SeatedMagAngles` builds them from the seated magazine's drawn box (the engine's `.magbox*`: its feed end
the round's +z, across it the round's x, the super nailgun's the other way round: make_mags.py's mounts), and B/Y pushes
it 40 u/s out of the well (`VR_Reload_MagOutWay`, never up: the super nailgun's out of its side and a little down; the
nailgun's and the cell's down as before). With the gun level the nailgun's comes out as it did. `vr_reload_debug 1` adds
to the "out of the gun" line its feed end along the seated one's (1 the same) and the push.

eject_test.sh: the pairs (above), and each gun in each hand at two pitches: the feed end along the seated one's 1.00 in
all 12 (the hand's angles, as before, at one pose: -0.12 to 0.28); the push 1.00 along the well's way out for the
nailgun and the cell, 0.70 to 0.82 for the super nailgun (its well tilts up), 14 to 34 u/s down, never up.

## The lying guns' parts seen from far (2026-10-09)

The author's note vrfiringrange_2026-10-09_12-35-45: the magazines and ammo screens of the guns on the range's tables
popped in and out a short way off. setupWorldWeapons gave them (the magazine and its well, the ammo screen, the button)
to the 6 guns nearest the head within 320 units, a constant; vrfiringrange has 32 guns lying about, 380 to 960 units
from the spawn: none showed any. Now `vr_weapon_world_attach_range` (2500 units) and `vr_weapon_world_attach_max` (48,
the pool's size; HUD and Menus > Screens, under Weapons' Ammo Screens: "Lying Weapons' Parts Range", "Lying Weapons With
Parts"); the ammo screens' image pool 64 (made only when wanted).

Cost (exclusive, vrfiringrange's spawn, 600 frames, twice): the frame's CPU 0.745/0.774 ms with none, 0.795/0.779 at 16,
0.807/0.849 with all 32 (3D: 0.443/0.448 -> 0.519/0.542); the GPU 0.861-0.889 ms all three, within its noise.
worldparts_test.sh: from the spawn and from 1500 units over the tables, all 32 screens and the 6 magazines (the farthest
955 and 1608 units off); at the old 320 and 6, none from either; the range at 1000 or 400 cuts them there.
## The author's melee, throwing and menu settings of 2026-10-09 are the defaults (2026-10-09)

His notes vrfiringrange_2026-10-09_15-02-52 (the screen tap's sounds), 15-45-02 ("melee and throwing ... more viable and
impactful"), e5m4 15-35-30 (throwing in bullet time), vrstart 15-18-36 (the menus' sharpness). His config of 15:50
against the shipped defaults (a `resetcfg; writeconfig` dump of this build: vr_cvars.inc with vr_defaults.cfg over it,
the engine's own for the rest). Config version 111 (`vr_cvars.cpp` defaultChanges: a config still holding the old
default takes the new one; one the player changed keeps it):

- Melee: `vr_melee_speed` 3.2 (3 m/s), `vr_melee_dmg_multiplier` 1.1 (1), `vr_melee_bloodlust_mult` 0.35 (0.5),
  `vr_quad_melee_damage` 1.1 (1), `vr_bash_damage` 10 (8), `vr_counter_damage` 1.75 (1.5; it also scales a bash's
  knockback), `vr_headbutt_damage` 24 (32), `vr_parry_stagger` 0.8 (0.75 s), `vr_parry_stamina_cost` 20 (30),
  `vr_parry_unarmed_reduction` 0.45 (0.5), `vr_strike_stamina_punch` 6 (4), `vr_strike_stamina_cost_2h` 12 (6).
- Enemy weapons in the player's hands: `vr_sword_damage_mult` 1.1 (1, vr_defaults.cfg), `vr_chainsaw_damage` 100 (80),
  `vr_dmg_chainsaw_swing` 22 (20), `vr_dmg_laser` 20 (18), `vr_enfrifle_damage` 16 (15), `vr_gruntgun_damage` 6 (5).
- Throwing: `vr_2h_throw_velocity_mult` 1.3 (1), `vr_weapon_throw_damage_mult` 0.4 (0.35), `vr_weight_damage_exp`
  0.375 (0.4), `vr_weight_lenient` 0.515 (0.5: an odd step, kept as he set it), `vr_gib_spawn_harmless` 0.5 (0.3 s),
  `vr_prop_drop_grace` 0.75 (0.5 s).
- Throwing in bullet time: `vr_throw_slowmo_flick` 0.9 (1), `vr_throw_slowmo_short_travel` 0.25 (0.15 m),
  `vr_throw_slowmo_long_travel` 0.5 (0.2 m).
- The screen tap's click: `vr_bullettime_tap_sound` 0.4 (0.6).
- The menus: `vr_menu_sharpen` 1 (0.5), `vr_menu_scale` 0.25 (0.18) and `vr_menu_distance` 150 (100) (vr_defaults.cfg;
  about the same angular size, further off: listed as his own last time, promoted now with his "tweaked the menu
  settings").

Not promoted: the shove, knockdown and parry push distances (redesigned in parallel): his `vr_knockdown_push` 1.3,
`vr_knockdown_shove_feet_hold` 0.05, `vr_knockdown_shove_ledge_topple` 100, `vr_knockdown_shove_topple` 0.01,
`vr_knockdown_shove_topple_push` 1. Personal, machine or slider noise: `contrast`, `gamma`, `fov`, `sensitivity`,
`volume`, `vid_*`, `scr_*`, `ui_live_preview`, `vr_menu_level` 2 (Developer), `vr_menu_positions`,
`vr_mirror_hide_hud_text`, `vr_spectator_*`, `vr_window_view`, `vr_foveated`, `vr_xr_runtime`, `vr_body_elbow_*`,
`vr_bodycal_*`, `vr_height_calibration`, `vr_tutorial_started`, `vr_motion_*` (Review Takes), `vr_ammo_pouch_*`,
`vr_comfort_vignette_strength`, `vr_melee_phase_*`, `vr_relight_strength` (x0.999 slider noise). Weapon and held
object settings compared too: the nailgun's hotspot 3 offsets (`vr_wofs_hs3_*_04`: its type is 0, none: inert) and
eight slots named for view models (`vr_prop_id_33`, `_57`..`_64` but `_61`: v_shot2, v_ksword, v_nail, ...; every
other value of theirs the defaults): nothing to promote, no settings version changed.

Test `config111_test.sh`: a config of 110 at the old defaults takes every new one; vr_melee_speed 2.5 and
vr_menu_scale 0.3 kept.
## Two-handed throws hurt more (2026-10-09)

His note vrfiringrange_2026-10-09_15-44-39: a weapon or prop thrown with both hands hurt no more than one thrown with
one (both hands only add speed for heavy things: vr_throw_2h_strength, vr_2h_throw_velocity_mult). `vr_throw_2h_damage`
1.25: a thing thrown with both hands has its hits' damage times this (QC `VR_Thrown_2hMult` in `VR_Thrown_Damage`, so
thrown weapons, props, boxes, gibs and crates alike). A thrown thing's `.vr_throw_2h` says its last throw was two-handed:
set on the thrown weapon where DropWeaponInHandScaled calls VR_Throw_TwoHanded (two hands, a real throw), and on a prop by
VR_Carry_Release (hands 2); every prop throw (VR_Carry_Throw) clears it first. Menus: Carrying and Throwing > Throwing and
Physics > Two-Hand Throw Damage (next to Two-Hand Throw Speed), and Combat > Weapon Damage > Thrown > Two-Hand Throws.
The throw hit's debug lines (developer 1) end with `hands N: xM`.

Test `throw_2h_damage_test.sh` (mock hands, an ogre 120 units ahead, the same push at 6 m/s with one hand and with
both; vr_throw_2h_strength 1 and vr_2h_throw_velocity_mult 1 so both hit at the same speed): a box of shells 287 u/s,
4.9 then 6.1 before where (x1.25); the shotgun (the off hand on its foregrip) 273 and 272 u/s, 13.6 then 17.0 (x1.25).
With vr_throw_2h_damage 1: 13.6 both.

## Select Campaign on the main menu (2026-10-09)

The author's note vrstart_2026-10-09_14-57-46: Single Player's "Official Campaigns" row is now the main menu's "Select
Campaign", right under Single Player (Single Player, Select Campaign, Multiplayer, in the playing group): it opens the
Official Campaigns page (`VR_OpenCampaignSelector`, as before), whose Back goes to the main menu with the cursor on the
row (NavStack: the outside menu it was entered from). Single Player has Quake's rows only again (New Game, Load, Save,
and Levels where Ironwail shows it). The page keeps its title (VR Settings' Play > Official Campaigns, Search, the hub's
board and the calibration boards' `{menu:Official Campaigns}` unchanged); the credits' row too. The row's letters are
the main menu's (vr_bigfont: every one there already), "SELECT CAMPAIGN" in the small capitals where a mod's pictures
leave the main menu as pictures. Test: `menu_vr pos` on the main menu, down once ("Select Campaign"), Enter (page 143
"Official Campaigns", back to menu 1), Escape (main, row "Select Campaign").

## The menus' corner buttons and keys larger for the laser (2026-10-09)

The author's notes vrstart_2026-10-09_14-57-46 .. 14-59-52: the corner's buttons were hard to hit and easy to misclick. In the headset
(ToolbarLayout, vr_menuui.cpp): the top left column's buttons 20 true pixels tall (were 14), 4 apart (were 2), their icon
and label 6 from the button's ends (were 4 and 5; the label 5 from the icon); the bottom left rows (OBS's, the spectator
camera's switch) the same height, gap and padding. Each still takes the clicks halfway to the next (no dead spots), and
4 from the panel's edges as the status and version boxes. The flat screen's row of icons is as it was. Search's and the
console's keys (and Search's text box, the console's line) 17 tall (were 14). The Map Library's page 490 across (was
460) and its keyboard 0.53 of it (was 0.5): the keys 13% wider, the list's column as wide as before; their height as
before (as large as fit, 18 at most).

On a VR page the column's bottom is now y 36 (was 5): beside the page, the page's rows do not move (their top is the
page's own); the banner under the column a little shorter. Where the column is over the menu (a narrow panel) the rows
start below it, 62 true pixels lower than before. Screens: the worktree's scratch/before_N, after_N (main menu, a VR page,
Search, the console, the Map Library), obs_after_0 (OBS's row, a mock OBS recording). obs_test.py: 14 of 15 (the frame
stall check while trying a dead port, timing only, failed under a loaded machine; every row and press check passed).

## Typing in the console no longer stalls (2026-10-09)

The author's note r1m2_2026-10-09_15-33-55: typing in the console, "v" first, lagged badly. Each key typed updates the
completion hint (Con_TabComplete, TABCOMPLETE_AUTOHINT), which built the whole list of matches (every cvar, command
and alias containing the text) sorted as it went: each match walked along the list to its place (q_strnaturalcmp), so
n matches cost n squared / 2 comparisons. "v" matches 14523 names (the vr_ cvars): 555 ms a key. Now the matches are
kept as found and sorted only when the list is wanted (Tab: Con_FinishTabList, a stable merge sort, a name found again
counted on its first as before); the hint needs none of it (the match when it is the only one, else the common part,
bash_partial, as before). Behaviour the same: the hint and Tab's list (names, types, counts, order) hashed against the
old way's for every beginning of "vr_s", "sv_g", "map e1", "bind m", "a" and "vr_console_complete_b": all the same.

`vr_console_complete_bench <text> [runs]` (Debug > Tools > Console Completion Timing) times both ways (exclusive):

| typed | matches | a key, now | the old way | Tab's list, now | the old way |
|---|---|---|---|---|---|
| v | 14523 | 0.48 ms | 569 ms | 2.4 ms | 601 ms |
| vr_ | 14413 | 0.55 ms | 570 ms | 2.3 ms | 594 ms |
| vr_s | 366 | 0.43 ms | 0.96 ms | 0.76 ms | 0.76 ms |
| a | 6000 | 0.70 ms | 83 ms | 1.4 ms | 81 ms |

What is left a key is the scan itself (each name searched for the text, about 0.35 ms). Tab with thousands of matches
still prints them all to the console (as before). `vr_mock_key text <letters>` types letters as a keyboard's text input
does (Char_Event: the console's line, Search's box), for tests.

## Ragdolls in liquids (2026-10-09)

The author's notes start_2026-10-09_15-27-59 and vrtesthall_2026-10-09_15-29-13: ragdolls (dead) and knocked-down
monsters fell into water, slime and lava at full gravity, as if there were none, and took the whole fall on the bottom
(a knocked-down grunt thrown into vrtesthall's pool from 200 units over it: "landed at 708 u/s, 65 damage", gibbed).

- Each ragdoll part (and a pushable corpse's one body, ragdolls off) in a liquid is lifted by how deep it is in it
  (`submerged`, the floating props' column test), its weight times the liquid's Float at full depth
  (`vr_ragdoll_float_water` 1.05, `_slime` 1.15, `_lava` 1.4): above 1 it floats, below it sinks. The torso (the pelvis,
  the chest, a spine) floats 1.15 times that, the head and limbs 0.9: a body floats face down. Armour sinks: the knights
  and death knights 0.85, the enforcer 0.92 (`ownFloat`). Given again for the step's later pieces (`liftAgain`).
- Drag (`vr_ragdoll_drag_water` 1.5, `_slime` 3, `_lava` 5, 1/s): each part's motion damped by how deep it is, times
  1 + its speed / 60 u/s (the quadratic part is the splash: a fall into it is braked as it goes in), its spin twice as
  fast. Asleep parts are left alone (a body floating still sleeps). All six at 0: as before.
- Fall damage through liquid (`vr_liquid_fall_cushion` 48 units, Gameplay > Liquid Breaks Falls): a monster landing
  under that much water, slime or lava takes no fall damage, under less the speed times the share of it that is dry
  (`liquidFallShare`): a knocked-down monster's ragdoll at the landing's point (callFalls), a standing one at its feet
  (`VR_MonsterFell`, SV_Physics_Step: it fell through water at full speed, Quake has no drag for monsters).
- Rows: VR Settings > Gibs and Corpses > Ragdolls, "In Water, Slime and Lava". `vr_debug_ragdoll 2` prints each frame
  a ragdoll's pelvis is in a liquid ("in liquid <contents>: pelvis z, speed up, across").

Measured (Misc/quakevr/ragdoll/liquid_test.sh; dead grunts dropped from about 200 units over the surface):
- water (vrtesthall, 120 deep): goes in at 535 u/s, at 160 u/s 50 units down, stops 75 to 85 units down (never touches
  the bottom), then rises at 15 to 20 u/s and floats, its back 2 units out (before: on the bottom at -123). A knight
  sinks at 40 u/s and lies on the bottom.
- slime (e3m1): 452 u/s in, stops 35 units down, floats. Lava (e1m7): 342 u/s in, floats.
- a live grunt knocked down over the pool, and one falling in standing: health 30, no fall (before: 65 and 63 damage,
  gibbed and dead). Knocked down into slime: no fall; the slime burns it (vr_enemy_liquid_damage) and it floats dead.
- Cost: the water-and-hits phase with 8 ragdolls lying on e1m1's floor 0.034 ms a frame, 0.033 with it all off.

## Bodies burn in lava (2026-10-09)

The same notes: a ragdoll or a knocked-down monster thrown or falling into lava should catch fire and in the end be
destroyed. `VR_Burn_LavaBodies` (vr_burning.qc, from VR_Burn_LavaFrame every 0.2 s; `vr_burn_lava_bodies` 1, Combat >
Burning > Bodies Burn in Lava): a monster's ragdoll (dead or knocked down) or corpse whose middle's bottom, origin or
(ragdoll) head is in lava catches fire there (`VR_Burn_Ignite`, VR_BURN_LAVA), lit again while it burns less than a
second on, so it burns as long as it lies in it; VR_Burn_Think no longer puts out a fire in lava (only wood's was kept
burning there). After `vr_burn_lava_gib` s in it (4; Burnt Through in Lava, 0 never) it bursts in embers and smoke: a
knocked-down one still alive is killed by it (health + 100: its death code's gibs), a corpse gibbed as a corpse
(`VR_Corpse_Gib`; one VR_Corpse_Parts doesn't know is removed in the puff; vr_corpse_nogib keeps it). A monster
standing in lava is still VR_Liquids_Frame's (burnt by its damage, not set on fire). With the engine's lava float
(vr_ragdoll_float_lava 1.4) the body burns on the surface, in sight.

Measured (liquid_test.sh lava, kdlava; e1m7): a dead grunt in at 342 u/s, floats, lit at once, more flames as it burns,
gibbed 4 s after it went in; a live grunt knocked down into it dies of the lava within 0.2 s, burns, gibbed 4 s on.
liquid_test.sh runs all the cases (water, knight, kd, fall, slime, kdslime, lava, kdlava; OLD=1 as before).

## A shove's knockdown: travel and a quarter turn (2026-10-09)

His notes (vrfiringrange_2026-10-09_15-11-36, 15-47-01, 15-48-52, vrstart 15-16-00): with the topple sliders (above, "A
shove's knockdown topples over the feet") a shoved-down enemy fell on the spot, and more push made it travel but spin
several times in the air. Wanted: a two-handed shove that knocks one down carries it nearly as far as one that doesn't
(somewhat less), clearly away from him, head and chest first with the feet lagging a little, turning only a quarter
(standing to flat) over the shove and landing flat.

- **The drive** (`box3d::ragdollShove`, then `driveShove` each frame; QC `ragdollshovetopple(e, dir, strength, ledge,
  reach)`, `reach` the units this shove would carry it standing: `VR_Shove_Reach`, the hop and the slide, before
  Launch cuts its velocity). Its travel and its turn are apart:
  - travel: its pelvis is to end Travel x reach on. The turn about its feet carries the pelvis its own height on; the
    drive moves it the rest, starting at most at the shove's own speed and slowing evenly to none over at least the
    turn's time. Its middle's level motion is eased to that (sideways to none), plus a pull towards where its pelvis
    should be by then (floor friction otherwise left it 15% short), every part alike.
  - turn: until Topple Time and 0.25 s more, every part eased towards turning about its feet (their middle now) at the
    rate of the curve 2u^2 - u^3 of Topple Angle (u the share of the time gone: slow to start, fastest past halfway,
    still turning as it lands), corrected by how far its torso (pelvis to head) leans off the curve (8/s), never past
    Max Spin; their own spin eased to the same (no turn about the vertical). Its feet Feet Lag of the travel's speed
    behind its middle, its top as far ahead, fading out as it lands.
  - Each part is eased a share 1 - e^(-dt / 0.06 s) of the way each frame (its joints still give); the knocked-down
    struggle starts once the drive is done.
- **Cvars** (Combat > Knockdowns, after Launch): `vr_knockdown_shove_travel` 0.8 (Travel), `_topple_angle` 90 (Topple
  Angle), `_topple_time` 0.6 s (Topple Time), `_feet_lag` 0.3 (Feet Lag), `_max_spin` 300 deg/s (Max Spin). Retired:
  `vr_knockdown_shove_topple`, `_topple_push`, `_feet_speed`, `_feet_hold` (a config's lines are dropped quietly). Launch
  (`vr_knockdown_push`) is now the fastest its travel starts. The judo throw is unchanged (ragdollTopple).
- **Debug > Tests > Enemy Shoves**: "Shove the Nearest, Standing, One Hand / Two Hands" (`vr_knockdown_chance 0`; the
  console's `shove slide: ... went N units standing`, what Travel is a share of). The shove trace (`vr_knockdown_debug
  1`) now prints its whole turn (summed every 0.1 s: two turns over would be 720 where the tilt is at most 180), its
  pelvis's travel, when its head first dropped below its pelvis's start height and when its feet first moved 8 units.
- **Numbers** (vrtesthall, 64 units ahead, `vr_knockdown_test 21`/`22`, the trace after 1.5 s; standing: the same
  shove at chance 0):

  | | standing went | down: pelvis went | share | tilt most | whole turn | head below the pelvis's start |
  |---|---|---|---|---|---|---|
  | grunt 1 hand | 104 | 82 | 0.79 | 92 | 99 | 0.38 s |
  | grunt 2 hands | 219 | 178 | 0.81 | 98 | 124 | 0.41 s |
  | knight 1 hand | 106 | 85 | 0.80 | 90 | 87 | 0.38 s |
  | knight 2 hands | 224 | 182 | 0.81 | 91 | 89 | 0.39 s |
  | enforcer 1 hand | 90 | 75 | 0.83 | 94 | 83 | 0.41 s |
  | enforcer 2 hands | 193 | 159 | 0.82 | 105 | 112 | 0.44 s |

  Before (the old sliders' defaults): the pelvis went 6-29 units, the tilt most 89-120. Every one lies head along the
  shove; the whole turn includes the pose's settling and the landing (the tilt from upright is the turn proper). Its
  feet end 10-20 units behind its pelvis (the lag and the quarter turn about them). Pictures: the worktree's
  `scratch/seq_grunt2.png` (a grunt shoved with two hands, seen from the side, about 0.15 s apart; `scratch/seq_enforcer1.png` an enforcer, one hand).

## A shove over a ledge: less far, less turn (2026-10-09)

His note (same session): the ledge shove is mostly fine but flies off too far; a little less turn.

- **Ledge Push** (`vr_knockdown_shove_ledge_push`, new, 0.7; Combat > Knockdowns after Topple Over a Ledge): its
  launch along the shove times this. So that it still always goes over, the drive (`driveShove`, ledge mode) keeps its
  middle going at that speed (at least 80 u/s) until it is past the edge (QC `VR_Knockdown_Ledge` now keeps the edge's
  distance plus its half width, `vr_kd_ledge_past`, passed as the builtin's reach) or its middle is below the floor it
  stood on.
- **Its turn**: Topple Over a Ledge (`vr_knockdown_shove_ledge_topple`) 150 -> 100 deg/s (his own value; config 112 moves
  a config still at 150). Until it lands, its turn (its parts' about its middle, as one) is eased towards that rate
  until its torso leans Topple Angle (90), then held there, never past Max Spin: before, tipping over the lip flipped
  it (a one-handed shove's torso reached 178 degrees, upside down).
- **Numbers** (vrclimb, `setpos -150 80 24 0 90 0`, a grunt ahead, the trench's edge 128 units on, a 192 drop; the
  shove trace's feet along the shove, units):

  | | lands at | feet at the end | past the edge | tilt most |
  |---|---|---|---|---|
  | two hands, before (push 1, 150 deg/s) | 0.92 s | 414 | 286 | 107 |
  | two hands, now (0.7, 100) | 1.22 s | 313 | 185 (-35%) | 108 |
  | one hand, before | 1.30 s | 248 | 120 | 178 |
  | one hand, now | 1.50 s | 240 | 112 (-7%) | 97 |

  A one-handed shove barely made it over before (its launch half spent on the floor), so it goes about as far; it now
  lies flat before the edge and is slid off it (kept at 0.7 of its launch, 180 u/s). Both land on their back or front.

## Shove distance: a base and each hand's (2026-10-09)

His ask (same notes): how far his shoves push enemies back, knocked down or not, adjustable as a base and a one- and a
two-hand multiplier, and 20% less far by default.

- The base is **Shove and Bash Push** (`vr_bash_push`, was Bash Push; Combat > Parry, Bash and Shove), 1 -> 0.9: the
  push (520 u/s and a 140 hop, times Knockback, divided by the monster's size) sets both the hop and the slide, so the
  distance goes as its square (0.81). Config 112 moves a config still at 1.
- New **One-Hand Push** (`vr_shove_push_onehand`, 0.7, was the constant `VR_BASH_ONE_PUSH` for the knockback; it still
  scales a one-handed bat of a projectile) and **Two-Hand Push** (`vr_shove_push_twohand`, 1). QC `VR_Shove_HandsPush`.
  A counter's push and a tired shove's cut multiply on top as before. A knocked-down body goes Knockdowns' Travel of
  the same shove's distance, so it follows.
- **Numbers** (vrtesthall, `vr_knockdown_chance 0; vr_knockdown_test 21/22`, `shove slide: ... went N units standing`):
  grunt 104 -> 84 (one hand), 219 -> 167 (two hands); knight 106 -> 86, 224 -> 180; enforcer 90 -> 73, 193 -> 154
  (19-24% less). Knocked down by the same shoves (Travel 0.8), the pelvis went 65/141 (grunt), 67/146 (knight), 60/127
  (enforcer): 0.77-0.85 of the standing distance, the tilt at most 91-102.

## Parry pushback about half as far (2026-10-09)

His ask (same notes): how far a successful parry pushes the enemy back, adjustable apart from the shoves, about half
as far by default (it makes room, but a counter-attack can't reach it).

- **Parry Pushback** (Combat > Parry, Bash and Shove, Parry: `vr_parry_push_enemy`, also Knockback's Parry Pushes
  Enemy) 0.8 -> 0.55 (vr_defaults.cfg; config 112 moves a config still at 0.8). The push (a weapon's 320 u/s and a 120
  hop, crossed arms' 260 and 90, times Knockback 0.6) sets both the hop and the slide, so the distance goes about as the
  square: a weapon parry's reckoned travel (the hop and the slide, as `VR_Shove_Reach`) 45 -> 21 units (0.47x); crossed
  arms' 29 -> 7 (its push now under the slide's 100 u/s: the hop alone). Its push of you (`vr_parry_push_player`) is
  unchanged. Not measured in a real parry (the mock can't hold a guard in time); the migration and the defaults were
  checked in game (`vr_cfg_version 111` with the old values, `vr_migrate_config`: 0.55, 0.9, 100).

## Main menu groups: Select Campaign heads the playing group (2026-10-09)

The author moved Select Campaign above Single Player (his commit); the gap stayed above Single Player, so Select
Campaign sat with the VR rows. `M_Main_GroupStart` now starts the playing group at `MAIN_CAMPAIGNS`: [VR Calibration,
VR Settings], gap, [Select Campaign, Single Player, Multiplayer], gap, the maps... One layout serves the headset's panel
and the flat screen (text or picture rows). The cursor still starts on row 2, now Select Campaign (the playing group's
first row; the author's order).

## Window View: both eyes, either eye raw or smoothed (2026-10-09)

Graphics > Recording > Window View said "Left Eye (raw)" while the window showed both eyes: vr_window_view 0 drew
whatever vr_mirror chose (the shipped vr_mirror 2: both). The window's view is now vr_window_view's alone, and
vr_mirror only turns the mirror on or off (Body and Display > Desktop Mirror: a toggle; 2 counts as on).
vr_window_view (window::ViewSetting): 0 Left Eye (raw), 1 Left Eye (smoothed), 2 Spectator Camera (as before), 3 Both
Eyes (raw), 4 Right Eye (raw), 5 Right Eye (smoothed). The menu lists them in the order Both, Left raw, Left smoothed,
Right raw, Right smoothed, Spectator. The smoothed right eye is the smoothed mirror of the right eye's image: its
steadied head follows that eye's own orientation (canted lenses), its crop that eye's field of view and hidden area;
the filter starts afresh when the eye changes. The spectator camera still follows the left eye's orientation, as before.

Config version 112: a config with vr_mirror 2 (or more) takes vr_mirror 1, and with it vr_window_view 0 becomes 3
(Both Eyes: what it showed); 1 and 2 keep their meaning; vr_mirror 0 is untouched. vr_defaults.cfg: vr_mirror 1. The
teleporter tests' `vr_mirror 2;vr_window_view 0` are `vr_mirror 1;vr_window_view 3`; INSTALL.md and TESTING.md say
`vr_window_view 3` for both eyes. Checked (e1m1, mock headset): the same view twice is identical (diff 0), the left and
the right eye differ (raw 2.75, smoothed 2.63 mean abs per channel: the eyes' parallax).

## Stuck on stairs, a fiend stuck on a bridge (2026-10-09)

**The fiend (note e5m4_2026-10-09_15-34-31).** Dimension of the Past's e5m4: the fiend patrolling the bridge
(`monster_demon1` at 0 400 -8, path corners dem1p1/dem1p2) stood at the bridge's foot for good. Its box was 20 units
into the ground (origin z -36; Quake's hull 1 is free from -16 up there). Cause: `vr_gameplayfix_droptofloor` (on in
quakevr.cfg) dropped every entity by point traces from its box's centre and corners and rested the box's feet on the
first floor found under its centre: on rough ground and by the bridge's ramp (inside the box's footprint) that put the
box deep into what it moves against, every move from there started in solid, and it never moved. Fixes:

- `VR_DropToFloor` (vr_gameplay.cpp): a solid body (SOLID_SLIDEBOX, SOLID_BBOX: monsters, solid boxes) is swept with its
  box first, as Quake does (through `SV_Move`: its narrow box or its hull); the points only when that sweep starts in
  something, and for items and triggers as before. The fiend now drops to z -16 and patrols the bridge (y 412 to 892,
  z up to 40, within 8 s).
- **Unstick Monsters** (`vr_unstick_monsters` 1, `vr_unstick_monsters_time` 1 s; Debug > Tests > Stuck in Walls): a live
  walking body (SOLID_SLIDEBOX, MOVETYPE_STEP, health > 0; not a player: `vr_unstick` does those) found inside the map
  or a brush model as its own moves meet them, and not moving (2 units) for that long, is moved to the nearest free spot
  (the players' search: 26 directions, out to 48 units; free of monsters first, else of the map) and falls from there.
  One box test per monster every 0.2 s (`VR_UnstickMonster`, from `SV_Physics_Step`). `developer 1` prints each;
  `vr_stuck_info` counts them. `vr_stuck_sink [edict] [depth]` (Sink the Nearest Monster) puts a monster 24 units into
  the floor: e5m4's fiend sunk 30 was freed after 1.1 s (moved 0 0 30) and went on patrolling; e1m1's nearest grunt sunk
  24 freed after 1.0 s; with the setting off both stay. First 6 s of e1m1-e1m3, e2m1, e3m1, e4m1, Dopa's e5m1-e5m7 and
  MG1's mge1m1-mge5m1: nothing to free except one lying zombie on e5m3, 0.12 units into the floor.

**The stairs (notes start_2026-10-09_15-30-46, 15-31-16).** MG1's start map, the steps at -260 7 -24: 8-unit steps
with a clip brush laid over them as a 26.6-degree ramp (hull 1 is a smooth slope there; hull 0 has the steps). Walking
up them slowly (a third of the stick) the player stopped two thirds of the way up the first step and crawled at
5 units a second; a jump there went 20 units and stopped again. With Quake's box (`vr_hull_width 0`) he walked up.

- Cause: Quake's friction doubles (`sv_edgefriction`) when a point 16 units ahead at the feet has no floor within 34
  units under it. The narrow box (16 wide) stands 4 units lower on a slope than Quake's 32 box (its uphill bottom edge
  is nearer its centre), so on the clip ramp its feet are under the steps' noses and that point was inside the next step:
  a trace starting in solid "finds no floor", double friction every frame (11 units a second a frame), more than a
  slow stick adds (12, less what the slope takes). `vr_hull_edge_probe` (1; Movement > Player Hitbox > Ledge Test Fix,
  `VR_HullOverDropoff`): the point is half the narrow box's width ahead (its leading edge, where Quake's geometry puts it
  for its own box), and a point inside solid has floor under it.
- Even with Quake's box, landing from that jump on the ramp at a third of the stick he crawled again: on a walkable
  slope Quake's gravity leaves about 5 units a second downhill each frame, which friction then takes with the walk's
  own speed; from a standstill under about 75 units a second of wish speed (a quarter stick) you never get going up a
  26-degree slope. `vr_slope_walk` (1; Movement > Locomotion > Walk Up Slopes Slowly; `VR_GroundGravity`): on walkable
  ground (the last floor plane met, normal z 0.7-0.9999, within 0.1 s) only gravity's part into the slope is kept. Flat
  floors are Quake's exactly; walking down slopes is a little slower than Quake's (it added that drift).
- Numbers (headless, 0.3 stick from -181 7 -8, prints every 5 frames): before, stopped at x -246 (z -1) and 0.37 units
  per 5 frames after; now up and on the landing (z 24) at x -299 in 85 frames, also at y -40 and 40; the jump at the
  foot lands at the top; at 0.5 stick both ways fine. `vr_hull_walktest 60 7` on e1m1, e1m3, e2m2 with and without the
  two settings: stuck 0, embedded 0, outside 0 either way.
- Debug: `vr_stuck_trace <dx> <dy> <dz> [edict]` (Trace Ahead) prints where the box stops and the plane met, and
  Quake's hull's answer; `vr_debug_walkmove 1` (Print Walk Moves) prints each walk move, its bumps, the step up and
  the ledge test.
- Not fixed: with Method Brush Sweep (`vr_hull_method 0`, not the default) the player walks under the recovered clip
  ramp's low end and stops against the first step's riser (moving up from there meets the ramp's underside).
## vr_physics_blast's explosion sent (2026-10-09)

`vr_physics_blast <x> <y> <z> [damage]` (the tests' explosion: QC's T_RadiusDamage at a point, used by about 15 test
scripts and menus) showed no explosion and made no explosion chunks: a console command runs before the server frame,
and the frame's `SV_ClearDatagram` emptied the temp entity it had written into `sv.datagram` before anything was sent.
Now it queues the message (`server::queueBroadcast`, vr_server.cpp: up to 16 whole messages, 512 bytes): the next
frame's broadcast opens with it, right after the clear, with its boundary marked and, as for QuakeC's own
explosions, its chunks launched (`noteExplosion`). Checked: e1m1, `vr_physics_blast 500 -255 50 1`: 12 chunks live and
drawn, the client's fireball (before: 0 and none); `explosion_debris_test.sh` check 7.

## The dragon's parry test with the mock hands (2026-10-09)

Debug > Tests > **Dragon Parry Sequence** never parried with the mock hands: the guns' guard pose (`vr_mock_hand main
0.15 1.25 -0.4 70 90 0`, the parry tests' `GUARD`) holds a crowbar upright (its blade is some 70 degrees off the hand's
forward: 83 degrees off level, `impulse 249`), and under the default grip mode (Hold) the mock hand let go of it unless
its grip was held. Nothing was wrong with the dragon's parry: the crowbar held level across (`0 90 0`, grip held)
parries the tail (492/492, interrupted, route recovered). New: **Dragon Parry (Mock Crowbar)** (that pose and grip,
then the sequence) and **Dragon Tail Swing (Pose Check)** (QC `VR_Parry_DragonSwing`: the dragon's own slash,
`dragon_melee1..10`, drawn before its tail lands and is parried; `VR_Parry_DragonTest` strikes the frame it spawns, so
no attack pose was ever drawn). `parryinterrupt/test.py` gained `dragon_crowbar`; `parry_pose_test.sh` the `dragon` kind.

The squash fix (d92812c0e, `VR_Parry_Hold`) on the dragon, `vr_debug_pose_check 2` on the swing: the slash's last
frame (28) to the pain pose (62) drawn over 0.748 s with the old single think at the stagger's end, 0.094 s now. The
dragon's poses never collapse (its smallest extent stays 0.90 of its two poses' either way), so no "held squashed" was
logged before or after; the time drawn between them is the fix. Also: `parryinterrupt/test.py`'s animation cases were
flaky (a still player under the stealth meter: the knight noticed him within 150 frames or not); it runs with
`vr_stealth_meter 0` now (6 of 6 runs pass), and writes its logs to the worktree's scratch, not its root.

## Test suite pass (2026-10-09): teleporter chase verdicts, argument fixes

- `teleporters_test.sh chase`: **the grunt never reaching the north room is by design** (PORTAL_AI.md: a ranged monster
  that sees you through a gate shoots through it and stays; only melee monsters run through, `VR_Stealth_ChaseGate`);
  checked: 18 of 32 snapshots in its shooting frames (81-89), at its post 150 units short of the gate. The chase now
  prints PASS/FAIL a case and exits 1 on a FAIL: the grunt passes when it stays and shoots; each dog and fiend gets up to
  `TRIES` (3) runs and passes when one gets through within the 480 frames. They are random: the same run repeated
  diverges from the first snapshot (the fiend's leaps), 3 of 18 single runs didn't get through (a fiend once went the
  other way, south to y -614). Quake's `random()` shares the C library's `rand()` with the client's effects, and fast
  mode draws frames by the wall clock, so its draws differ from run to run. Not changed: a server-only random stream
  (seeded at map load) would make these tests deterministic; that is the author's call. (Done, approved: "Server
  random numbers: their own stream", below.)
- `teleport_frames_test.sh <agent>` alone ran the agent's name as a case (`shift 3` with one argument shifts nothing);
  `parry_pose_test.sh <agent>` the same as a kind (`shift 2`). Both take `set -- "${@:N}"` now.
- `climb/slopes_test.sh` ran in another agent's kit folder (`movetweaks`) with a play from its scratch: it takes the
  agent now, writes the mantle play (hands at 1.49 m) to its own scratch, and checks ROUND21's table with no cvars
  (8 of 8: mantled level, up 10/20/30, down 20, across 15; no room up 45 and under the slab).

## SteamVR slower than VDXR: where the time goes, the dashboard (2026-10-09)

- His two captures (Quest 3 over Virtual Desktop, RTX 4090, 120 Hz, vrstart then start): `systems_2026-10-09_17-40-27`
  VDXR, `systems_2026-10-09_17-42-22` SteamVR (`qvr_openxr.txt`: the 17:42:09 start loaded SteamVR/OpenXR 2.17.10).
  | | VDXR | SteamVR |
  |---|---|---|
  | fps (mean / median) | 119.4 / 120 | 89.2 / 98.8 (28 for 9 s; 0 for 5 s) |
  | GPU, eyes (memstats `gpu_eyes_ms`, vrstart / start) | 3.2 / 3.7 ms | 6.8 / 5.3 ms |
  | GPU in the runtime's calls (acquire + submit) | 0.05-0.3 ms | 1.8-2.1 ms |
  | CPU busy | 2.1 ms | 2.9 ms |
  | xrWaitFrame | 4.7 ms (the pacing) | 0.1 ms |
  | xr acquire + release (CPU) | 0.01 ms | 1.3-1.6 ms |
  | xrEndFrame | 0.9 ms | 6.9 ms mean, 25 ms in the bad seconds, one of 3.1 s |
  | video memory free | 4.9 GB | 1.0-1.1 GB, 600 MB evicted during the run |
- Causes. (1) SteamVR's eye images are 3292x3524 (`vrserver.txt`: the driver's 2688x2880, "Clamping render target
  scale to 1.5x total area": SteamVR's Render Resolution on Auto), 1.5 times the pixels: the eyes' GPU time doubles
  (Quake's share of the GPU 37% at 120 fps on VDXR, 50% at ~108 on SteamVR). (2) SteamVR's OpenGL path copies every
  released image into its own textures (the GPU time in the acquire and submit scopes, the CPU in release) and paces
  in xrEndFrame, not xrWaitFrame. (3) The 28-fps seconds and the 3-second stall: `vrcompositor.txt` says
  "WaitForAcquire timed out ... before the driver took the sync texture" (Virtual Desktop's SteamVR driver late taking
  the compositor's frame) at exactly those times, with 1 GB of video memory free (SteamVR, its dashboard, fpsVR's
  overlays and Virtual Desktop's driver on top of the game). The compositor's own summary: 2706 of 10711 frames
  reprojected. (4) With the dashboard open the game rendered as usual (no focus handling).
- Done: `vr_xr_unfocused` (1; Advanced > Headset > Runtime Menu Open): while the session is VISIBLE, not FOCUSED
  (SteamVR's dashboard, Virtual Desktop's or Meta's menu), the eyes aren't rendered: the projection layer shows the
  swapchains' last released images with the poses they were rendered for (the spec: a layer shows its swapchain's last
  released image), world-locked; the window keeps its last image. Fake headset: 4.9 ms a frame held vs ~22 ms rendered
  on the shared test machine. `vr_xr_late_acquire` (1; Late Image Acquire): each eye's image acquired only after its
  scene and glow are drawn (they go to the eye's own targets), before the post-processing writes it: SteamVR's wait for
  the image (its copy out of the last one) overlaps the scene's drawing instead of stalling before it. Eye images
  pixel-identical to the old order (mock, `vr_eyeshot 3`: 1 channel of 3 M off by 3, the dither). `vr_xr_eye_scale` (1;
  Eye Image Size, next VR start): the swapchains at that fraction of the runtime's recommended size each side (0.82:
  SteamVR's 150% back to the panel's pixels), which also shrinks SteamVR's copies and its video memory.
- The log (`quakevr/qvr_openxr.txt`): the extensions offered and enabled, the system's properties, the eyes'
  recommended and largest sizes, the swapchain formats offered (in the runtime's order) and the one chosen, each
  swapchain made (size, format, image count, usage), the layers, the eye size and the settings; each session state
  change with the time (VISIBLE = the runtime's menu); and with `vr_xr_log_timing` (1; also Debug > Profiling) a line a
  second: frames rendered / shown again / with the panel / empty, shouldRender off, display periods missed
  (predictedDisplayTime), the frame period, and each call's ms a frame and worst (poll, wait, begin, sync, acquire,
  waitimage, release, end).
- Tests: the fake runtime has a headset now (`FAKEXR_HEADSET=fakexr_steam`, `FAKEXR_EYE`, `FAKEXR_UNFOCUS=a-b` frames
  VISIBLE; OpenGL swapchains in the game's context; at the session's end the log counts the layers, and an invalid
  projection layer, without a released image, fails the frame); `xr_runtime_test.sh` part 4 (3 checks).
- Not ours, for him: SteamVR's per-app Render Resolution (100% = 2688x2880) or Eye Image Size 0.82; fpsVR and other
  overlays off; VDXR remains the cheaper path (no compositor copy, no driver hand-off).
## The toolgun (2026-10-09)

"Implement a Garry's Mod-like toolgun for debugging and sandbox gameplay purposes [...] spawnable in the debug menu and
available in vrfiringrange [...] clicking the magazine eject button for that hand (Y/B) would display a special in-game
menu UI attached to the toolgun [...] Entity/prop/weapon spawning [...] remover [...] move/rotate [...] Prop
transformation tool [...] Prop joint tool [...] Quick cheats/utilities menu" (`kit/briefs/toolgun_request.md`).
Controls, settings and the code's map: **docs/vr-port/TOOLGUN.md**.

**The weapon.** `WID_TOOLGUN` 19 (ammoless, `weapon_toolgun`, `func_weapon_grabbable` weapon 19, impulse 169/189),
its model `progs/v_toolgun.mdl` from `Misc/quakevr/make_toolgun.py` (808 vertices: a dark iron body over a leather
grip, brass bands round a short emitter, a cyan crystal in brass prongs on top, cyan rune lines, a crystal lens at the
muzzle: Quake's fullbright 244..246), weapon settings slot 26 (the grunts' gun's, the Offset moved by the bounds'
corner: 0.348 0.924 -0.135; muzzle anchor 614; no hotspots; `vr_wofs_version` 40). In vrfiringrange north of the
crowbar (`make_prop_area.py --ent-only`, labelled); Debug > Cheats > A Toolgun in Your Hand, Debug > Tests > Spawn
Pickup Weapons > Toolgun, Debug > Tools > Toolgun.

**Its buttons** (`vr_input.cpp` → `toolgun::button`, before the voice notes and the flashlight): its hand's trigger, B/Y
and X/A are the tool's (taken, no key); with X/A held the sticks are its offsets (`toolgun::sticksTaken`: no walking, no
turning). The other hand's trigger freezes what the physgun holds; its Y goes back a page in the menu (the gun in the
right hand).

**Its menu** is the VR Settings: pages 167 Toolgun, 168 Toolgun - Spawn, 169 Toolgun - Cheats and Utilities
(`vr_menu_toolgun.inc`), opened from the game by B/Y; while one shows and the toolgun is held, the panel is drawn over the
gun (`vr_panel.cpp` toolgunQuad: the tracked controller's frame, `vr_toolgun_menu_height` 8 units high, its bottom 4
over the controller, leaning back 20 degrees) and the laser's hit-test uses it; the game runs under it
(`VR_MenuRunsGame`). Back from its first page closes it. The cheats page is Debug > Cheats and Recording's own rows,
filtered by label (their commands and help as there).

**The tools** (`vr_toolgun.cpp`): the aim is the drawn hand's (the barrel's way) from the muzzle; what it meets is the
nearest entity box before the world (not you, nor what your hands hold).
- Spawn: a list of 46 (12 monsters, 7 props, 14 weapons, 13 items) or the picker; the ghost a translucent temp entity
  glowing in the hue (`entityGlow` on the ghost's entity), lifted onto the floor by the entry's height, facing you;
  made by `box3d::spawnClass` (vr_physics_spawn's spawn, factored out: the classname, a model, a float key).
- Remove: the target glows red (the shaders' glow: 2..3 is red, `vr_glsl.h` alias and world); QC `VR_Toolgun_Remove`
  → `VR_Scene_Remove`.
- Physgun: a prop is pinned (`box3d::setPinned`: `kindOf` makes it a Fixture, a kinematic body following its entity:
  it shoves the others) and its entity moved along the beam each frame; a monster or a pickup is moved (held up); let go,
  unpinned with the beam's eased velocity; the other trigger keeps it pinned (frozen).
- Scale: the prop's model box scaled by `model_scale` about `model_scale_origin` (set to the model's middle at its first
  scaling), turned with it; 6 face and 8 corner handles; a held handle's drag is the beam's point at the handle's
  distance along the handle's line from the box's middle as taken (stable: the box moving under it doesn't feed back);
  its Quake box grown with it; FL_ONGROUND cleared so the remade body settles.
- Joint: records in box3d (`ToolJointRecord`: the bodies' frames, kept outside the world), made again by
  `syncToolJoints` whenever a body is made again (frozen, let go of, scaled, a new world) and both bodies exist, one of
  them dynamic; dropped with either entity (`onEdictFree` → `box3d::toolForget`). Weld, ball (spherical), hinge
  (revolute about the second face's normal), slider (prismatic along it), rope (distance joint, spring on with 0 Hz,
  limit to its length), spring (2 Hz). Ropes and springs drawn (`forEachToolJoint`).

**Tests** (mock headset, vrfiringrange, `vr_weapon_grip_mode 1`; scripts in the agent's scratch):
- Menu: B → page 167 on the gun (`menu_vr pos`), the off hand's mock laser clicks "Choose What to Spawn" → page 168, a
  row → "toolgun: spawn Enforcer"; Cheats and Utilities → God Mode → "godmode ON"; off Y → back to 167; again → closed.
- Spawn: a grunt at the ghost (146 -553 42), X/A and the sticks 40 frames → the ghost moved 36 units on, 91 aside, turned
  35 degrees, the player not moved; a crate there; the picker on the grunt → "picked monster_army"; health, a rock.
- Remove: the grunt glowing red, removed; the crate removed.
- Physgun: a crate taken at 149 units, lifted to z 91, pushed to 176 units (X/A, the stick), frozen (still at z 90.9 60
  frames later, pinned); a grunt carried to z 88 and dropped (z 41).
- Scale: the stick to 1.75x (lifted out of the floor); the side face handle dragged 6 degrees: 2.10x (expected 2.1);
  Proportional off: 1.00 1.58 1.00; reset: back to 1 and settled on the floor.
- Joint: weld, then the first crate dragged up 31 units: the second came with it (33 → 64), stayed hanging when the first
  was frozen, fell when unjoined; rope: the second hung 44 units up below the first; ball, hinge, slider, spring: made
  (1 joint each). (Crates break when yanked: `vr_crate_health 0` in the rope test.)
- Buttons (`vr_debug_buttons 1`): the shotgun's trigger reaches the game; with the toolgun its trigger and A are "taken
  by the toolgun", the off hand's X not.
- QC 0 warnings, statics and FGD checks pass. The melee eval: no current takes (skipped).

**In the headset.**
- [ ] Take the toolgun in the firing range; B/Y: the menu over the gun, readable, the other hand's laser clicks; tune
  Menu Size, `vr_toolgun_menu_up` / `_tilt` if it sits wrong.
- [ ] The gun in the hand: the grip in the fist (the Offset is computed, not fitted: Weapon Offsets slot 27 if off).
- [ ] Spawn a monster and a crate; move and turn the ghost with X/A and the sticks.
- [ ] Physgun a crate around, freeze it in the air with the other trigger, stack another on it.
- [ ] Scale a crate by a face and a corner; weld two crates and carry one; rope one under a frozen one.

## Server random numbers: their own stream (2026-10-09)

- Why: QuakeC's `random()` (`PF_random`) and the monsters' movetogoal turns (`sv_move.c`) drew from the C library's
  `rand()`, which the client's effects share (Quake's particles, dynamic lights' flicker, decals, a beam's `srand` each
  frame) and `Host_Frame` stirs once a frame. Any difference in what the client drew moved the AI: the infight scene
  (`vr_stealth_test 107`, fixed frames, the same script) gave other state hashes with `vr_decals 0`
  (a61c.../a6ee.../caf9... against 79ae.../350c.../95ae...), and the teleporter chase needed three tries.
- Now (`Quake/vr/vr_srvrandom.cpp`): the server has its own stream, Zancle's `FastNonCryptoRng` (Xoroshiro128++).
  `VR_ServerRandom()` gives `rand()`'s range (0..0x7fff) and `PF_random` makes its float as before
  (`sv_gameplayfix_random 1`: never exactly 0 or 1), so the game plays the same; only which numbers come out differs.
  The client keeps `rand()`. The probes' own numbers while a map loads (`VR_ProbeRandom`) are unchanged.
- Seeding: at every map load (`SV_SpawnServer`, before anything spawns: a new map, a changelevel, a loaded game), from
  `sv_random_seed` (not archived; tests) when it isn't 0, else from the clock (normal play: a new sequence each load;
  `developer 1` prints the seed, `sv_random_info` too, and `sv_random_seed <it>` replays it). `vr_bench_seed`, a motion
  take's start and `vr_motion_eval`'s map loads seed it as they did `srand` (and arm the next map load once);
  `vr_hull`'s chase test seeds it for movetogoal's turns. Explosion debris (server Box3D props) take a seed derived
  from it (was the clock's) unless `vr_particle_seed` is set.
- Saves: the stream's state isn't stored; a loaded game is a map load and is reseeded as above (a fixed seed: every load
  of a save plays on the same; 0: differently each time, as before). Saves keep the format other engines read.
- Kept on `rand()`: purely visual or audible picks (particles, decals, water sounds' and knocks' variants), `randmap`.
- Checked (infight scene, `sv_random_seed 5`, hashes at 6.27/10.44/14.60 s): the same three hashes plain, with
  `vr_test_crand 777` (C library draws) between them, and with `vr_decals 0`; the dogs scene (123, seed 7) the same
  plain and with decals off plus 5000 draws; at 144 Hz client frames against 90 every QuakeC field hashes the same but
  the hands' per-frame motion history (`mh_*`, `handthrowpos`: input, not randomness); `sv_random_seed 0` twice: other
  hashes each run; a save loaded twice (999 C draws between): the same hash 300 frames on.
- Tests: `teleporters_test.sh chase` runs fixed frames with `sv_random_seed` SEED+try-1 (the same verdicts every run:
  all six PASS on the first try, output identical twice); `stealth_tests.sh` sets `sv_random_seed ${SEED:-1}`.
- Debug > Profiling and Memory: Server Random Seed (`sv_random_info`) next to Game State Hash.


## Crouched pose (2026-10-09)

The author (vrfiringrange_2026-10-09_17-51-39, 17-52-32, vrteleporters_18-03-53): crouched, the torso hid the ammo pouch
and the holsters and didn't look like his body: his torso is further back, his shoulders back. The body now takes a
crouched pose on top of the standing one, all of it set for a full crouch and blended by how deep the body crouches (the
skeleton's own depth: 0 standing, 1 the pelvis at squatting height; solveTorso).

- **Share** (avatar::crouchPoseWeight): `vr_body_crouch_pose_strength` (1) times the depth to the power
  `vr_body_crouch_pose_curve` (1: half a crouch, half). `vr_body_crouch_preview` (-1 off; 0..1, not saved) sets the depth
  the blend takes, standing or not, so it can be tuned looking down or with the body in front (Debug: Show Body Skeleton).
- **Body** (vr_avatar.cpp): `vr_body_crouch_torso_back` (0.2 m; was 0.05) / `_up` / `_right` move the trunk (pelvis, spine, chest;
  the legs follow the pelvis); `_torso_pitch` / `_yaw` / `_roll` turn the spine and chest about the spine's base;
  `_pelvis_back` / `_up` / `_pitch` the hips on top; `_shoulders_back` (0.03 m) / `_up` / `_out` and `_shoulders_swing` /
  `_shrug` (degrees, the collarbone about the base of the neck); `_elbow_out` / `_back` add to the elbow's pole
  (vr_body_elbow_out/back); `_eye_forward` / `_eye_up` add to the neck's place under the eyes.
- **Holsters and the ammo pouch** (vr_body.cpp crouchShift, vr_view.cpp holsterTurn): they ride the trunk and the pelvis
  as before (the Follower carries them: its standing reference never has the crouched pose), and each pair
  (`vr_hip_holster_crouch_*`, `vr_upper_holster_crouch_*`, `vr_shoulder_holster_crouch_*`) and the pouch
  (`vr_ammo_pouch_crouch_*`) has its own x/y/z (units forward, outwards (the pouch: right), up, in the body's facing) and
  pitch/yaw/roll (as the Hotspots turns), blended the same; the hotspots move with them.
- Body Calibration's upright chest and the Follower's standing reference are solved without it.
- Menu: Body > Crouched Pose (Strength, Curve, Preview Crouch) and its page Crouched Pose Offsets (every offset);
  Debug > Views: Preview Crouch and Print Crouched Pose (`vr_body_crouch_report`: the share, pelvis, chest, shoulders,
  elbows, holsters and pouch, units from the eyes forward/right/up).
- Test: `vr_body_crouch_preview 0 / 0.5 / 1` standing: share 0 / 0.5 / 1, the pelvis -4.44 / -5.18 / -5.92 units
  forward of the eyes (0.05 m back at full), the hip holsters and the pouch 0.74 units back per half; head at 1.0 m
  (`vr_mock_hand head 0 1.0 0`): share 0.90; strength 0 is the old pose exactly (share 0).
- **His values are the defaults** (his note vrfiringrange_2026-10-09_18-52-47, config version 115, `vr_cvars.cpp`
  defaultChanges: a config still at the old defaults takes them): `vr_body_crouch_torso_back` 0.05 → 0.2,
  `vr_body_crouch_shoulders_out` 0 → 0.02, `vr_hip_holster_crouch_x` / `_y` / `_roll` 0 → 6 / 3 / 40,
  `vr_ammo_pouch_crouch_x` / `_z` / `_pitch` 0 → 2 / 7 / 29 (`vr_body_crouch_tilt` was 20 already, vr_defaults.cfg; the
  strength and curve, the upper and shoulder holsters unchanged). Test: `Misc/quakevr/config116_test.sh` (PASS).

## Body calibration history (2026-10-09)

The author (same notes): keep the previous calibrations and let him go back to any. Every change of the calibration's
settings (Apply, Undo, and the new Revert) puts the settings it replaced in a list, the last four, kept in
`bodycal_history.txt` in the game folder next to the config (across restarts; git-ignored). A calibration is the height
(standing), the measurements (`vr_bodycal_*`) and the tweaks (`vr_body_tweak_*`). Each entry has when it was applied
(`-`: not known, the settings before any calibration the list saw) and when it was replaced.

- Menu: Body Calibration > Previous Calibrations, newest first: "Revert: <date>, arms <upper> + <forearm> cm, eyes
  <height> m" (the help: every value). Reverting makes it the settings again; the ones it replaces go in the list in its
  place, and Undo takes the revert back (vr_bodycal_undo). The config is saved at once, as Apply does.
- Console: `vr_bodycal_history` lists them (1 the newest) with their settings, `vr_bodycal_revert <n>` reverts.
- The file also keeps the settings the list last had as the current ones: changed otherwise since (the console, an old
  config put back, as the kit does after each run), they go in the list too, so none is lost.
- Test (the author's sessions refitted: `vr_bodycal_refit bodycal/2026-09-29_00-41-19.txt; vr_bodycal_apply`, then
  `..._01-52-37.txt`): applied 28.4/23.8 cm then 26.2/24.8; `vr_bodycal_revert 1` put back 28.4/23.8, shoulders back
  -0.043, rise 14.9, height 1.5430; the next run (the config back to the kit's baseline) listed both and reverted to the
  first entry's settings, the list then 28.4 and 26.2. Undo after an Apply lists the undone one.
## A dog's leap drawn smoothly; flaky checks (2026-10-09)

- **The snap after a dog's leap** (stealth_tests.sh dogs, frame 48: 9.7 units in a frame with
  `vr_monster_lerp_continue 1`). A dog is MOVETYPE_STEP in the air too: SV_Physics_Step moves it every server frame
  (free fall), but its moves were drawn to its next think (0.1 s, Quake's U_STEP lerp). Each frame's move began
  before the last was drawn: with the drawing continued from where it is drawn, the dog fell behind (about 26 units by
  the leap's end) and caught up in a frame or two as its think came. Quake's drawing (0) snapped a little every
  frame instead. Now a stepping monster in the air (not on the ground, not a flyer or a swimmer) is sent its move's
  time as one server frame (U_LERPFINISH, `VR_StepLerpInterval`, vr_server.cpp; only with `vr_monster_lerp_continue`
  1, so 0 stays Quake's): drawn one server frame behind, as Quake draws missiles. Landing, the next step starts from
  where it is drawn. Same scene, A/B (the server half off): leap frames' farthest drawn step 6.8 -> 3.7 units, the
  landing 9.7 -> 3.6 (the run's usual 1.3 a frame); moves begun early 62 -> 6 (worst 20.8 -> 5.2 units); dogs: 0
  jumps, barrels not pinned. `vr_debug_drawn_moves`' summary adds the farthest drawn in a frame (and its model frame).
- **obs_test.py's no-stall check** (an absolute 11 ms for the worst CPU work, failed under load with no OBS): relative
  to the same session's `vr_obs 0` run (p99 within 30% + 1.5 ms, worst within 3x or +25 ms; a refused connection on
  the main thread would be ~2 s), measured again once on a stall. Here the off run's own worst was 25 ms; 15 of 15.
- **grip_gap forcegrab's sixth catch** (nothing caught, 2 of 4 runs): the test, not the force grab. On the wall clock
  a loaded machine's frame over 0.1 s (Host_FilterTime's clamp) left the server behind the take for good (a failing
  run: the third pull arrived after its step's check, the next three never flew, the frames ran out before the
  take's end); and the sixth grip came 0.13 s after the box arrived, on `vr_forcegrab_catch_late`'s 0.15 s edge (the
  flick fires as the hand starts up: it arrives ~0.09 s earlier than the take's keys suggest). Now on a fixed clock
  (`vr_fixed_frames 1` at 250 Hz: the server's 72 Hz ticks under it) and the grips 0.04 s apart (0.08 s late at
  most): 6 of 6 runs pass (each with three late catches).
- `box3dmt/physbench.sh <agent> [count]` passed the agent and the count on to run.sh (`shift 3` with fewer arguments
  shifts nothing): `set -- "${@:4}"`.

## vrstart: a terrain triangle never drawn (2026-10-09)

Note vrstart_2026-10-09_18-36-42: a black triangle in the grass by the pines on the hill above the firing range
(from 341 -594 125). The face was in the BSP (the ray test, 0 holes in a million rays, looks for missing faces), but
no leaf listed it in its marksurfaces, so the renderer never drew it. Cause: the terrain triangle (205 -574 112,
199 -524 112, 260 -576 104) and its neighbour were 0.004 units apart across their planes (0.007 degrees): both
apexes on the walkable ground's pinned 8-unit steps, so `terrain_mesh` couldn't move either, and qbsp 0.18.1 put the
triangle on the neighbour's plane and listed only the neighbour. The two hairline pairs on the map (the other at
1216 492: a second unlisted face, unseen yet) were the two unlisted faces; the next closest pair, 0.006 units, was
fine.

- **Fix** (`mapgeom.terrain_mesh`, `tiny` 0.1): a pair left under 0.1 units apart is made exactly coplanar even at a
  pinned corner (a move under 0.1 units, onto no level, leaving no top nearly level and no more hairline pairs): 17
  pairs made coplanar, 7 left (0.019 units apart at the closest, 5x the failing one). vrstart rebuilt (final).
- **bsp_holes.py**: every world face must be listed by a leaf ("unlisted faces": exact, the whole map, a second), and
  each ray's hit must be on a face listed by a leaf the ray's start leaf sees by vis's PVS ("undrawn hits"; a face
  listed only by a neighbouring leaf is common, ~450 in 300,000 rays, and fine). Before: 2 unlisted faces, 2 undrawn
  hits in 300,000 rays (both places); after: 0, 0 and 0 missing in 1,000,000. `--rays 0` runs the face check alone;
  a compile always runs it. A compile now stops when qbsp finds textures missing (no `id_textures.wad`: every face
  a checkerboard, which the hole test doesn't see).
- vrtrailer.bsp has one unlisted face (17 909 -7, just under the water); its generator gets the fix on its next
  rebuild (10 hairline pairs moved there); vrtrailer2 and vrtutorial2: 0.

## The tutorial finalized: vrtutorial, no map aliases (2026-10-09)

His notes vrtutorial2_2026-10-09_18-22-02 .. 18-32-29 and vrstart_18-36-01.

- **Tips always shown in the tutorial**: its worldspawn `_vr_tips_repeat` 1 (the engine's `mapFlags`): every tip shows
  again each time he comes near it (gone 1.25x its range away first), seen before or not.
  **Replaced (his correction)**: not every time he comes near, but every time the tutorial is *started*. Its worldspawn
  now has `_vr_tips_reset_on_start` 1 (any map can): at each spawn of the map not from a saved game (`map`, changelevel,
  `restart`, the hub's and the menu's VR TUTORIAL, a new game into it) the engine (`tips::serverMapStarted`, from
  `VR_OnSpawnServerAfterLoad`) forgets the map's keys in `tips_seen.txt` (`vrtutorial:...`, `vrtutorial#n`); within the
  play each tip shows once; loading a save keeps what that play has shown. vrtutorial.bsp's entity lump edited in place
  (`bsp_set_entities.py`), the .map and `vrtutorial_gen.py` the same; FGD worldspawn key, MAPPING. Test (one run): play,
  `t2_move` shown and seen; save; `map vrtutorial` -> "1 forgotten", not seen, shown again once; `load` -> still seen;
  `changelevel vrtutorial` -> forgotten again.
- **World scale 1.2 is normal**: `vr_defaults.cfg` had 1.2 since 2026-10-04, but the compiled default and the World
  Scale wall buttons' "normal" (tutorial, hub: `vr_setup.cpp`) were 1.25. Now 1.00 / 1.20 normal / 1.50; vr_cfg_version
  115 moves a config at 1.25 to 1.2.
- **Value screens still while pressed**: the screen over a setting button (and SELECTED over a campaign lectern) was
  placed from the button's box as it moved, so it rode the press into the wall; it is placed from the box at rest now
  (less the move from QC's `pos1`). The buttons' own labels (QC's worldtext boards) never moved.
- **Texts**: lesson 3 says you can also jump for real (`vr_roomscale_jump`); FLASHLIGHT SIDE (was TORCH SIDE, tutorial
  and hub), the flashlight "on your torso" (was "at your hip"); the grenade pouch "on your lower back" (was "the small
  of your back": also the menu's help); the trap: a grenade dropped with its pin in is "a trap you can shoot to set
  off". All entity text: the BSPs' entity lumps edited in place (`bsp_set_entities.py`, geometry and light kept) and
  checked against the generated `.map` (every entity's keys the same).
- **Final names**: vrtutorial2 is `vrtutorial` (its .map, .bsp, .lit, .lux, `vrtutorial_gen.py`,
  `vrtutorial_playtest.py`); the old tutorial's files and the old hub (`vrstart_old.bsp`, `vrstart_old@3e00.ent`) are
  removed, as are `VR_MapAlias` (vrstart2, vrslipgates loaded under their new names), the tips' renamed-map keys, the
  vr_cfg_version 99 `vr_hub_map` migration and `vr_hub_map` itself (retired: the hub is vrstart). Saves made in
  vrtutorial2, vrstart2, vrslipgates or the old vrtutorial no longer load (dev saves). `relight_quakevr_maps.py` no
  longer relights the old tutorial; `stray_press_test.sh` loads vrtutorial in place of vrstart_old.

## The west stairs crouched (2026-10-09)

**Notes start_2026-10-09_17-58-56, 17-59-28.** MG1's start: after the ledge test fix the north, east and south steps
walked, the west ones (foot at -232 -11 -8, up westward) still not: he stood at -247 -11 -8, at the first step's riser
under the clip ramp, and a jump there hit an invisible wall. Headless with the standing box they walked; with the head
lowered 15 cm (`vr_mock_hand head 0 1.45 0`: eyes 47.7 units over the feet, Crouching's 52-unit box) the box went under
the ramp and stopped at -248 -11 -8, his spot (`vr_crouch_status`: box 52, standfits 0).

- Cause: a recovered clip brush's faces that don't face the open (`recoverClips`, vr_hull.cpp) were kept where hull 1
  has them, grown by Quake's 56-tall box. The ramp's lowest piece is a wedge on the floor: its bottom face is the
  floor's top grown by Quake's box (centre 28 over it). A crouched box's centre on that floor is lower (26 over it for
  52, 18 for 36), so it passed under the wedge (2 to 10 units of room) to the first real step.
- Fix: those faces move with the box's height as the floor or ceiling past them does (`Plane::growsZ`: their height's
  part of Quake's box out, the box's in; Quake's and the standing narrow box's height unchanged), only where hull 1 stays
  solid 32 units past the face (a floor's or ceiling's grown slab). Without that test the stairs' clip piece under the
  landing (its top a cut inside the landing's slab) rose out of the floor: the 36 box stood 4 units over the landing.
  Such pieces' bounds take 32 32 16 more.
- Checked (0.3 stick from 181 units out, every 5 frames; boxes standing, 52 and 36; all four sets): all up to the
  landing (z 24), no stall on the ramp; the jump at the west foot lands on top at every height. `vr_hull_walktest 60 7`
  with the box 44 crouched on e1m1, e1m3, e2m2 and MG1's start: stuck 0, embedded 0, outside 0.
- Not fixed (narrow box, any height): at the west ramp's top the box meets the landing's edge 0.7 units over the ramp's
  end, while airborne over the crest (Quake steps up only from the ground): its speed along drops to 0 and builds up
  again (a moment's stop at a slow walk; sometimes a 6-unit dip into the notch first). The east, north and south ramps
  end over their landings. The same width version of the fix (faces against solid pushed by Quake's box's width less
  the box's, capped at the piece's top) took the hitch away, but made 2 to 16 stuck frames in e2m2's clip brushes in
  `vr_hull_walktest 60 7` crouched (0 without): left out.

## You burn after lava (2026-10-09)

**Note start_2026-10-09_17-59-40** (monsters already burn on after lava: 18-01-31). `vr_burn_lava_player` (3 s;
Burning > You Burn After Lava, 0 off; `VR_Burn_LavaPlayers`, vr_burning.qc, from `VR_Burn_LavaFrame` every 0.2 s): a
live player in lava is set on fire (the torch-touch path: flames on the body at the lava's height, round it) and lit
again each look while in it, so out of it he burns on for that long (Burn Damage, 4 a second, in 0.5 s ticks). In lava
the fire does no damage of its own (`VR_Burn_Think`: a client with watertype lava), so lava's damage is id's as before.
Water puts it out as any fire. Not lit with the pentagram or the biosuit. Checked on e1m7 (`setpos 710 160 40`, 0.4 s
in, then to the start): in lava only lava's 10-point hits; out, 2 a tick at 80, 78 .. 72, out after 3 s.

## Knocked down into water: getting up (2026-10-09)

**Note vrstart_2026-10-09_17-55-49.** A knockdown floating in water never got up: the get-up's place (`standSpot`,
vr_box3d.cpp) wants a floor within 128 units under the body and within 24 of it at each place tried; vrstart's sea
is 136 to 1024 deep, so none, every 0.5 s for good. In vrtesthall's pool (120 deep) it stood up on the bottom, under
water, and drowned 5 s later (its breath had run since it went in face down).

- Lying in water, slime or lava (its pelvis or just over it in one), `standSpot` looks for a floor anywhere under each
  place with the monster's eyes (0.8 of its height over its feet) out of the liquid: the shallows' bottom or the bank,
  reached from up to 96 units over the body (vrstart's island stands 56 over its sea; never through a wall or ceiling),
  out to `vr_knockdown_water_search` (256; Knockdowns > Search in Water); the box tried up to 18 units over a slope.
  `vr_knockdown_debug 2` prints each try's counts (a wall between, no floor, too deep, no room).
- None for `vr_knockdown_water_giveup` s (3; Give Up in Water; 0 never) after its time to get up: it gets up where it
  floats (`ragdollgetup(..., anywhere)`: its box clear there or around it, floor or none), a walking monster again,
  not on the ground: it sinks, walks the bottom and can drown (vr_liquids.qc, 12 s of breath from when its head went
  under). Never a floating ragdoll for good.
- Breath: a knockdown face down in water has its head under (`VR_Liquid_Level`: the ragdoll's head), so its 12 s run
  as for a standing one under water; it is up (2.5-4.5 s) long before, and out on a bank it breathes again.
- Checked (`liquid_test.sh <agent> kd kdslime kdlava kdsea kddeep`): vrstart, knocked off the island's south shore
  (948 -1160): up after 3.1 s, 64 units off on the slope, feet in the water; far out (948 -2580): 543 places all too
  deep, up where it floated after 6.6 s, sank to -270; vrtesthall's pool (time 8 s): up on the edge, 16 units off,
  out of the water (before: on the bottom, drowned); slime and lava as before (killed; burnt through and gibbed).
## Confirmation dialogs: the headset's frames, buttons, the main menu's VR rows (2026-10-09)

- **The game drawn in odd colours, flickering, while a confirmation was up** (NOTES.md vrstart_2026-10-09_18-21-02;
  VR Calibration's): SCR_ModalMessage's loop draws the headset's frames itself (VR_ModalMessageFrame), with no host
  frame, so `host_framecount` stood still. What is made once a frame and kept for it was not made again: the
  particles' records (and ropes' and bent meshes' rings) are uploaded into the frame's own GL_Upload space, and every
  dialog frame drew the records uploaded before the dialog opened, from a buffer that two frames later held other
  data (garbage positions, sizes and colours). Each dialog frame is now a frame of its own (`host_framecount` counted,
  one more after the last). `vr_debug_glstate 1` prints such draws (`glstate: particles drawn from an earlier frame's
  upload`; `gl_frameres_serial`, the frames drawn, in gl_rmisc.c): a 1 s dialog over smoke and an explosion printed it
  every frame (200 of 205) before, never after. The mock's eye images showed no garbage either way (what lay there
  happened to draw much the same); to check in the headset.
- **Buttons on the menus' questions** (the same note): the menus' confirmations (VR Calibration, New Game over a game,
  Options > Reset All, Quit in the headset) are now a menu of their own over the one they came from (`m_confirm`,
  menu.c `M_Confirm`), not SCR_ModalMessage's loop: the game's frames go on under them, and they have two buttons, the
  action's (Start, New Game, Reset, Quit) and Cancel, pointed at with the laser and taken with the trigger (the
  mouse on a flat screen); y, n, Escape, B and the arrows with Enter or A still answer. The action's button is
  selected first (A or Enter takes it, as A answered yes before). The corner's buttons don't take the laser or keys
  while one is up. Reset All still cancels itself after 15 s. Quit's message (`cl_confirmquit 2`'s jokes too) without
  its "Yes No" line. Flat screens too (their keys as before, and the mouse clicks the buttons). Left on
  SCR_ModalMessage (fixed above): the video mode's "keep it?" (flat only), "Load last save?" (sv_autoload 1; 2 by
  default asks nothing) and `vr_test_dialog`. Tests: `vr_mock_laser yes|no` (and `vr_mock_mouse yes|no click`),
  `vr_test_modal_answer` answers these too, `vr_test_confirm` (Debug's Dialogs: Confirmation Buttons) opens a test one.
  Checked headless: Cancel and Start by the laser (main menu, VR Calibration starts), Quit cancelled and taken (the
  game quits), New Game cancelled by the laser and taken by `vr_test_modal_answer 1`, Reset All cancelled by the laser
  and by its 15 s, a flat screen's mouse click, `n` typed, one opened in game (closed back to the game).
- **The main menu's VR rows** (notes vrstart_2026-10-09_18-20-50, 18-37-10): VR Calibration, **VR Tutorial**, **VR
  Hub**, VR Settings, together in the first group (VR Settings kept with them, last: the places first). Each of the
  three asks first (M_Confirm, the game in progress ending said when one runs): Start / Go and Cancel. VR Tutorial
  runs the Play page's Tutorial (`playTutorial`, vr_menu.cpp, its map named there only), VR Hub its VR Hub
  (`vr_campaign_hub`: `vr_hub_map`). The cursor still opens on Select Campaign. The lettering's capital T (none in
  id's pictures) is t, Multiplayer's small capital, its bar 2 rows higher (make_bigfont.py). Two rows more: where the
  menu's canvas is too short for them 15 apart (a flat screen's 200 rows; not reached at the test windows' sizes nor
  the headset's Menu Height 1, which give 16) the rows go to 13 apart with no gaps, their letters drawn smaller
  (`VR_BigFont_DrawScaled`). Checked headless: both rows' dialogs (Cancel by the laser, VR Hub's Go by the laser, VR
  Tutorial's Start by `vr_test_modal_answer 1`: "Quake VR: Tutorial" loads), `vr_menu_path_check` 0 missing.

## Toolgun: the joint choice crash (2026-10-09)

The author, 18:53, his Release build of e1b7d17f0: "the game crashes as I was selecting a joint type in the physgun".
No qvr_crash.txt: the crash was not the engine's. Windows' Application log at 18:53:37: ironwail.exe faulting in
**nvoglv64.dll** (NVIDIA's OpenGL driver), exception 0xc0000409, with the driver's own event "Unable to recover from a
kernel exception. The application must close. Error code: 3 (subcode 7)" and the System log's nvlddmkm event 153
("Error occurred on GPUID: 100", the same as 2026-09-30 12:45): the GPU faulted (a TDR or a page fault) and the driver
ended the process with a fast-fail, which no unhandled-exception filter sees. qvr_openxr.txt's last two seconds show the
frame getting heavier first (xrEndFrame 0.4 → 1.3 → 3.9 ms on average, the wait for the next frame 5.4 → 1.8 ms).

The toolgun's side, checked headless with the mock laser on the gun's menu, Release and Debug (Zancle asserts, the
debug CRT heap), his own config exec'd too: every joint choice from every other through the Joint row's list (a
drop-down: 7 choices), with the physgun holding a crate (and the gun swung meanwhile), with a joint half made, the Tool
list switched to Joint and back mid-drag, every kind made then unjoined, the gun in either hand; also six joints of
every kind on one pair dragged and swung hard (no runaway: the crates came to rest), a frozen jointed crate removed,
and 800-step random sessions of buttons, laser clicks, sticks and tools. No assert, no fault, every pick set
`vr_toolgun_joint` to the choice clicked. Nothing in the joint choice reaches the GPU but the gun's screen text (not
drawn while the menu is open) and the menu's own drop-down (older, used on many pages).

Regression test: `Misc/quakevr/toolgun_joint_menu_test.sh <agent> [--debug]` (both hands: 70 picks, 0 wrong, tool
list 4/2, 6 of 6 kinds joined, 6 removed; PASS on Release and Debug).

A lead, not fixed: the ammo screens' images (vr_text3d.cpp renderScreens) are kept by their place in the frame's queue,
so a screen text coming or going (the toolgun's screen as its menu opens or closes, the guns nearest you changing)
moves every later screen to another image and remakes its texture (ensureTarget, with mipmaps). His memstats of that
session (profile/memstats_2026-10-09_18-42-43.csv): render targets remade 180 → 1695 in ten minutes in the firing range
(each of 14+ "ammo screen" images 40 to 130 times), against about 2 a minute in the tutorial; VRAM 19-20 GB of 24.5.
Whether that churn upset the driver is not known.

In the headset: the toolgun menu's joint list again (Physgun active, a crate held); if the driver error comes back,
the time and Windows' Application log entry tell it apart from an engine crash (which writes qvr_crash.txt).
## The runtime's menu pauses the game (2026-10-09)

- **Pause** (`vr_xr_unfocused_pause` 1; Advanced > Headset > Runtime Menu Pauses: Pause the Game / Keep Running):
  while the runtime's own menu has the focus (the session VISIBLE, not FOCUSED: SteamVR's dashboard, Virtual
  Desktop's or Meta's menu; `Backend::runtimeMenuOpen`) a single player game pauses as the game's own menu pauses it:
  `Host_ServerFrame` skips `SV_Physics` and `SV_RunClients` the player's think (`VR_RuntimeMenuPause`), so the game's
  time stands still; the slow motion meter waits too. It resumes as the focus comes back. Multiplayer runs on (the
  server isn't one player's to pause): only the held frame (`vr_xr_unfocused`). Decided each host frame in
  `VR_BeginFrame` after the runtime's events (`runtimeMenuFrame`, vr_main.cpp), before the server's frame.
- **Sounds**: the whole mix faded to `vr_xr_unfocused_volume` (0: silent; Runtime Menu Volume) over a tenth of a
  second (`audio::setDuck`, applied first in `VR_SndLimit`, with or without the spatial audio; no click), back as it
  resumes; the music paused (`BGM_Pause`; resumed unless the game was paused with `pause` meanwhile).
- The console says each change: "VR: the runtime's menu has the focus" (with why the game runs on, if it does),
  "VR: the game paused at <time>", "VR: the game resumed after <s>, <n> frames: game time <a> -> <b>; the sounds
  down to <gain>".
- Debug: `vr_debug_runtime_menu 1` (Debug > Profiling: Act as if the Runtime's Menu Were Open) acts as if the
  runtime's menu had the focus, on any backend (the mock's too), for the pause and the fade (not the held frame).
- Test: `xr_runtime_test.sh` part 5, on the fake runtime (FAKEXR_UNFOCUS=200-300, with sound): paused at 2.412 s,
  resumed after 0.40 s and 100 frames with the game's time still 2.412, the sounds down to 0.00; the debug pause
  later at 8.089 (the time ran on in between); `vr_xr_unfocused_pause 0`: "the game runs on", never paused. Part 4's
  check split: the second's timing line with the frames shown again may come after the focus is back (it did, with
  the paused game's quicker frames).

## The eyes' size against the headset's panel, in the status box (2026-10-09)

- Eye Image Size stays one global setting. The menus' status box (top right; `vr_menu_status`) now says, in VR:
  "Eyes WxH (n Mpx)" (the size rendered), "= runtime's WxH xA xB" (the runtime's recommended size times Eye Image
  Size, as the images came out, times Render Scale), and with the panel known "Panel WxH: runtime P%, eyes Q%" (the
  runtime's own supersampling, SteamVR's Render Resolution or Virtual Desktop's quality, and the eyes' rendered pixels,
  as shares of the panel's).
- Over `vr_xr_res_warn` (1.3) times the panel's pixels (the panel unknown: over `vr_xr_res_warn_mpx`, 6 million an
  eye) a warning, white on a red band (a hue alone is lost under the menus' tint): "! Eyes 2.2x the panel's pixels:
  lower" / "! Eye Image Size (Advanced: Headset)" (or "Render Scale, Eye Image Size" when that is over 1) / "! or
  SteamVR's Render Resolution" (Virtual Desktop's quality, Meta's, by the runtime's name).
- The panel: `vr_xr_panel` "WxH" (archived; "" by the headset's name), else looked up by OpenXR's systemName (its
  letters and digits: Quest 3S, 3, Pro, 2, 1; Rift S, Rift; Index; Vive Pro 2, Pro, Vive; Reverb G2; Pico 4; Bigscreen
  Beyond; Pimax Crystal). SteamVR names the driver, not the headset ("SteamVR/OpenXR : oculus"): there it takes
  `vr_xr_panel` (a Quest 3: `vr_xr_panel 2064x2208`), else the 6 Mpx budget. Virtual Desktop's and Meta's
  names are expected to carry the model (not checked on them: worth a look at qvr_openxr.txt's "system" line).
- `vr_status` prints the same lines ("status: ..."). The fake runtime takes a name (`FAKEXR_SYSTEM`);
  `xr_runtime_test.sh` part 6: a Quest 3 at 3096x3312 (150% each side): "runtime 225%, eyes 225%", the warning;
  `vr_xr_eye_scale 0.66`: "eyes 98%", none. By hand: an unknown name, no panel line; `vr_xr_panel 500x500` with Render
  Scale 1.2: "runtime 154%, eyes 221%", the Render Scale hint; "Quest 2 (VDXR)" found (1832x1920).

## Video memory (2026-10-09)

His "19-20 GB of VRAM used" is the whole GPU's: the game holds ~1.3 GB at his eyes, the rest is other programs
(Resolve.exe 14 GB on this machine now). PERF_DECISIONS.md, "Video memory", has the breakdown and the options.

- `vr_vram_report [diff] [csv] [all]` (Debug > Memory > Video Memory Report): every GL texture, renderbuffer and
  buffer sized from GL's own description, by category and group (GL labels; the VR module's textures labelled now),
  the largest, and Windows' count for the process and each other program. `diff`: made/freed since the last report.
- The memory log: `vram_quake_mb` (ours), `vram_programs` (the others holding the most), `screen_draws`; vr_memstats
  prints ours and the others'; the status box: "VRAM used/total GB (game X.X)".
- Ammo screens: their images found by their text, not the screens' order (the world's guns are nearest first: a step
  reordered them, each slot drawn again and remade when its size changed: ~1700 targets in his 10 minutes). Walk test
  (54 steps along vrfiringrange's racks): targets made +450 -> +0, images drawn +450 -> +0; screens with the same
  text share an image (20 screens, 2 images).

To check in VR: the ammo screens on the firing range's guns as you walk past and pick them up (each showing its own
count, no wrong text for a frame more than before); `vr_vram_report` in his session: the "programs on the GPU" line.

