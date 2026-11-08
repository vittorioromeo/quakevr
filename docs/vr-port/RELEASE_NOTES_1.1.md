<!-- DRAFT 2026-11-08 (release pass): full 1.1.0 notes since 1.0.1, from the beta1-4 notes, ROUND23.md and ROUND24.md.
Edit, then pass with make_release.ps1 -Notes docs/vr-port/RELEASE_NOTES_1.1.md (the script appends the Files table).
Delete this comment before publishing: -Publish refuses notes with the DRAFT marker. -->

**Quake VR: UNLEASHED 1.1.0**: the biggest update since 1.0. Enemies now fight back with real weapons, stagger and scramble as active ragdolls, and wear armour you can shoot off; Arcane Dimensions and a new Survival Arena join the campaigns; and the whole game can now be played on a flat screen with mouse and keyboard. Everything from the four 1.1 betas is in, plus a lot since beta 4. Please report bugs: Options > Create Bug Report makes a zip you can attach.

## Highlights

- **Armed humanoids, on by default**: grunts, enforcers, knights and death knights now carry real weapons, including yours, and use them with the player weapons' own code. Grab a grunt's shotgun and fight him for it in a physical tug-of-war, knock it out of his hand with a strike or a shot, then turn it on him. They parry your blows (and you theirs), keep their distance, mind their friends, dual-wield, wear backpacks you can lift off and drop what they hold when they die. Turn it off with Gameplay > Replace All Humanoids (`vr_agrunt_repl 0`).
- **Euphoria-style active ragdolls**: enemies stagger, keep their balance on bent knees, topple on stairs, ledges and from heavy blows, grab ledges and scramble back up. Shove them off a ledge and they backpedal and fight for their footing. Lift them, throw them into their friends, sweep their legs, drag them by the ankles, or land on them from a height.
- **Brutal melee and gore you can hear**: one swing hits everything its edge passes through; neck snaps, judo throws, heads and limbs popping, flesh dents and blade grooves, blood streams, back-splatter, bloody footprints and handprints, and a severed head or limb stays stuck on the axe. A full set of gore sounds, each with its own volume.
- **Flat-screen play (new)**: start the game with `-flatscreenmode` and play Quake VR's physical game with mouse and keyboard: guns held as in VR and aimed at the crosshair, ironsights for every weapon (right click), punches and shoves, a lean (Q / E), a sprint slide, mantling, crouching and lying down (X), the force grab, bullet time (Z) and the toolgun (F7). See "Flat screen" below.
- **Armour plates**: armed humanoids can wear plate armour that dents, cracks with a metal snap and falls off as you shoot it; your own armour shows as plates on your body and sheds as you take hits.
- **Arcane Dimensions, made for VR (experimental)**: AD's maps, monsters and rules played natively (Play > Official Campaigns). Its weapons are VR models: the two-handed Widowmaker Gatling, the Shadow Axe, the Plasma Gun, the crossbow, swords, hammer, mace and the death sergeant's flail, and AD's enemies drop what they carry.
- **Survival Arena (new)**: Play > Maps > Survival Arena, a purpose-built map of waves that grow with you: armed and armoured foes, boss waves with perks, supplies after each wave, banners under the ceiling with the wave, foes left, time and score. A wave editor lets you make and save your own wave lists.
- **New weapons and sights**: the nail pistol, the heavy revolver and the flare gun are now in the game (beside ammo boxes and on crates), the grunts' burst rifle is made anew, and the guns have proper iron sights: the shotguns' sight housings, the super shotgun's notch, the nail pistol's glowing inserts, the lightning and plasma guns' sights. Recoil is procedural and per weapon, gunfire is punchier, and barrels glow with heat.
- **Earned bullet time**: the meter fills from the damage you deal to living enemies.
- **Holsters and loot**: drag-and-drop Holster Setup, magnetic holsters that pull a thrown gun back on a physical chain, an ammo pouch that shows what it holds, and magazines that slide home.
- **Smoother and faster**: map loads much faster (work moved off the main thread), fewer first-time hitches, dynamic resolution, a model depth pre-pass, models lit by every baked light (the model light grid), and a new Graphics Presets page.

## ⚠ Experimental (off by default or not finished)

These are in the build for testing. Expect bugs, and expect their balance and settings to change.

- **Arcane Dimensions support (experimental)**: start it from Play > Official Campaigns, after a prompt saying it is still experimental. Armed humanoids appear in AD's maps too.
- **Embedded blades (off)**: swords, axes and thrown blades stick in bodies and corpses; pull them out, or move the body by the blade. Gameplay > Embedded Blades (`vr_embed 1`). Sticking is still inconsistent and blades come loose too easily.
- **Pick Up a Second (off)**: an armed humanoid holding a weapon that pairs picks up a second one for its other hand (`vr_agrunt_dual_pickup 1`).
- **Creator and developer tools (flat-screen or dev only)**: the Rig Studio, Pose Studio and Grip Editor, the Weapon Studio and Armour Studio (flat-screen windows), the Ragdoll Tool (`-ragtool`), the animation recorder and the Blade Shape editor. They are meant for tuning, not for play.
- Still experimental: single-pass stereo, TAA, full-body tracking (Quest 3 body tracking through Virtual Desktop).

## Armed humanoids

- Every grunt and enforcer a map spawns is its armed version (Gameplay > Replace All Humanoids), in Quake, the mission packs and Arcane Dimensions; knights and death knights carry their blades and, rarely, guns. Combat > Armed Grunts (Experimental) sets the odds of each weapon tier, dual wielding and a gun with an axe.
- Disarming: a physical tug-of-war with one or two hands (they hold on, headbutt, shove or fire at you while you pull), a deliberate strike on the weapon or the hand, a shot to the gun, or grabbing the weapon of a downed one. A dual wielder loses only the struck weapon.
- Directional parry: the only parry now, for you and for them. A good parry meets the blow's line with yours, and turning your weapon into it at the last moment counts as well timed. Dog and fiend lunges are parried with a level weapon.
- Worn backpacks on the ones carrying ranged weapons: lift them off a living enemy, rip them off in a hard pull, or loot them from the body. Replaced enemies never drop extra weapons, only what they hold and their pack.
- Armour plates on some: helmets, cuirasses and limb plates that dent, crack and fall by the plate a shot meets.
- Their melee keeps its spacing, their shots leave from each muzzle, their footsteps are heavy, and the held weapon follows the hand when they go ragdoll.

## Combat and melee

- Melee no longer depends on frame rate (72, 90 and 120 Hz play the same); a swing hits up to 4 things and a wall only once.
- Shoulder charge: run at an enemy side-on, shoulder leading, to knock it down. Bodies you throw topple the enemies they hit.
- Parry balance by weapon, grip and timing against each monster group's Parry Poise: a block, a flinch or a stagger.
- Enemy melee under one set of rules: blows land only in front of the attacker, so sidesteps work (Combat > Enemy Melee, with Show Melee Arcs).
- Shooting yourself (headset only): your shots and missiles hit your own body if you point a weapon at it.
- Projectiles keep less of your motion: strafing no longer throws nails and rockets off your aim.
- Close quarters brakes your momentum; weapon clash between your own two weapons; your two hands stay where they gripped on close two-handed holds.
- Heavy weapons have weight: they lag, droop and kick in one hand; two hands steady them.
- Monster Groups (Combat > Monster Groups) set every ragdoll trigger and action per group across Quake, the mission packs and mods.

## Weapons and reloading

- The nail pistol (magazine-fed, moving slide, slide lock, auto re-rack), the heavy revolver (swing-out cylinder, single shells) and the flare gun (flares that stick and light the room) are found beside ammo boxes and on crates.
- New iron sights on the shotguns, super shotgun, nail pistol, lightning gun and plasma gun; the shotgun and nailgun remodelled with new skins; the grunts' burst rifle made anew.
- Procedural recoil per weapon (kick, rise and recovery), two-handed recoil for any grip, barrel heat glow, punchier gunfire sounds and haptics, surface impact sounds.
- Shotguns load two shells at a time; pouch reloading loads a magazine gun from where it is drawn; magazines slide home and out.
- The Plasma Gun's cell holds 16.

## Flat screen

- `-flatscreenmode` plays the full game without a headset: the hands and guns are drawn and simulated as in VR, posed per weapon (pistols, rifles, shoulder launchers held differently) and aimed at the crosshair from the muzzle.
- Mouse: with one weapon, left click fires and right click aims down the sights (hold, or a toggle in Desktop Screen Play); dual wielding, each button fires one hand.
- Keys: Q / E lean (also round corners and over the gun at a wall), 3 swaps hands, crouch at a run slides, X lies down, V melee, G drop, mouse 4 or R force grab, mouse 5 shove, Z bullet time, F7 the toolgun; hold jump while looking at a ledge in reach to mantle onto it.
- Walls push the gun's muzzle up instead of through them; crouching eases; swimming and crouch speeds; hit feedback on screen; Vomit Mode on flat deaths.

## Body, holsters and movement

- Lying down and crawling: get low in your headset and you lie down and crawl, pulling yourself with your arms (flat screen: X). Swimming tilts the body.
- Your legs fit your height, your gear fits your body, your feet step in time with your run, and your footsteps follow your speed.
- Holster Setup by drag and drop; magnetic holsters; seated play with its own holster layout and a crouch toggle.
- Climbing: mantles once your head clears the ledge, footholds on props.
- Player Appearance: gloves in Quake's art style, a feminine body build, hand shape and voice, painted nails, ghost hands, and a Randomize button.

## Gore, fire and water

- Gore sounds with their own volumes, bloody footprints and handprints, cut flesh on stumps, a head or limb stuck on the axe.
- Lightning burn marks, lightning on water, flamethrower scorch marks and firelight, flames on bodies put out by water.
- Blood in water, shoreline foam for each liquid.

## Menus, saves and comfort

- VR Settings > Gameplay: every big gameplay switch on one page (armed humanoids, knockdowns, embedded blades, parry, bullet time, gore, the arena in any map).
- The installer offers a settings page after installing.
- Autosaves and a death menu (Respawn, Restart, your autosaves).
- Item Pickup Mode (Immersive, Auto-Collect, Vanilla), Soften Flickering Lights, a smoother thumbstick.
- Fonts upscaled smoothly, with an outline and a drop shadow (Classic keeps Quake's look).
- Your config now holds only what you changed, so new defaults reach you.
- Create Bug Report zips your logs, crash reports and settings.

## Graphics and performance

- Faster map loads: skins' normal maps on worker threads, fewer failed file lookups, smoother rigs prepared at once.
- Models lit by all the baked lights (the model light grid), lit liquids lighting models.
- Dynamic resolution, a model depth pre-pass, shadow atlas sized per map, cheaper bloom and particles.
- Less CPU per frame in big fights (area-grid searches, fewer allocations, the arms' IK).

## Fixes

- A crash holding one enemy with both hands; a crash on quickload; crashes in The Matriarch of Decay and The Sacrifice; Shub-Niggurath's teleporter after a save.
- Enemies stay active ragdolls after loading a save; sounds no longer cut each other off.
- OBS Mirror's OpenXR layer is skipped at startup; a focused desktop window no longer slows the headset.
- Many smaller fixes and cleanups.

## Known issues

- **Arcane Dimensions**: the sky and some lights may flicker in VR when you turn or move; the hub's lower Quake slipgates go nowhere.
- **Armed humanoids**: a knocked-down one that aims high may pull its arm in.
- **Flat screen**: wrists can bend sharply on some holds, and a gun's end can poke a few centimetres into a wall at the limit.
- The installer and the game are not code-signed, so Windows SmartScreen may warn you (More info > Run anyway).

## Notes

- Your settings carry over: configs from 1.0.x and the 1.1 betas are migrated on first start. Settings you left at an old default move to the new one, and values you changed stay yours. Armed humanoid replacements are now on even if a beta config had them off at the old default.
- Full details: `docs/vr-port/ROUND22.md`, `ROUND23.md` and `ROUND24.md` in the repository.
