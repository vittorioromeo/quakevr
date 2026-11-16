# Quake VR: UNLEASHED 1.1: patch notes

**Version 1.1**, compared with 1.0 (1.0.0 and the 1.0.1 patch). This file is for 1.1; the release candidate ships it
as-is.

1.1 is the biggest update since 1.0. Enemies now fight back with real weapons you can take off them, stagger and
scramble as active ragdolls and wear armour you can shoot off; Arcane Dimensions and a Survival Arena join the
campaigns; every new game starts with a Run Setup and optional mutators; melee gains daggers, a reverse grip, embedded
blades and a directional parry; and the whole game can now be played on a flat screen with mouse and keyboard. The
highlights come first, then every change by area. Everything from the four 1.1 betas is included.

Your settings carry over: configs from 1.0.x and the 1.1 betas are migrated on first start. Settings you left at an
old default move to the new one, and values you changed stay yours.

## Highlights

- **Armed enemies, on by default.** Grunts, enforcers, knights and death knights carry real weapons, yours included,
  and use them with the player weapons' own code. Disarm them in a physical tug-of-war, by striking the weapon or the
  hand, or by shooting the gun out of their grip, then turn it on them. They parry, keep their distance, mind their
  friends, dual-wield and wear backpacks you can loot; they can disarm you too (an option, off by default for now).
- **Euphoria-style active ragdolls.** Enemies stagger and keep their balance on bent knees, topple on stairs, ledges and
  from heavy blows, grab ledges and scramble back up, and stir when knocked down. Shove, lift, throw, sweep, drag them
  by the ankles, or land on them from a height.
- **Shootable armour.** Armed enemies can wear helmets, cuirasses and limb plates that dent, crack with a metal snap,
  split and fall off by the plate you hit. Your own armour shows as plates on your body and sheds as you take hits.
- **Arcane Dimensions, made for VR (experimental).** AD's maps, monsters and rules played natively, with its weapons
  rebuilt as VR models (the two-handed Widowmaker Gatling, the Shadow Axe, the Plasma Gun, the crossbow, swords,
  hammer, mace and flail). The installer can download it for you.
- **Arena Mode (experimental).** A purpose-built Survival Arena of growing waves: armed and armoured foes, themed
  waves, boss waves with perks, supplies between waves, a score, and a wave editor for your own wave lists.
- **Full flat-screen play.** Start with the new "Flat Screen" shortcut and play Quake VR's physical game with mouse
  and keyboard: simulated hands and guns, ironsights, lean, slide, mantle, lying down, melee and parry, force grab,
  holsters, bullet time, a visor HUD and an optional third-person camera.
- **Run Setup and mutators.** Every new game opens a Run Setup: difficulty, a Classic / Light / Unleashed style, the
  gore preset, your starting gun and melee weapon, and mutators (Melee Only, Glass Cannon, Ironman, Randomized
  Pickups, Randomized Enemies, Enemy Count, Ranged Knights, Wild Weaponry, Armored Foes). One switch gives you Classic
  Quake.
- **Melee, reworked.** One swing hits everything its edge passes through, at any frame rate; a directional parry where
  timing matters; the new dagger; a reverse (blade-down) grip; blades that stick in bodies and can be pulled out or
  used to move the body; skewered gibs; thrown blades that fly point first; a shoulder charge.
- **New weapons and better guns.** The nail pistol, the heavy revolver and the flare gun; a new burst rifle; real iron
  sights; ammo screens built into the guns; procedural per-weapon recoil, barrel heat glow and punchier gunfire.
- **Smarter monsters.** Walking monsters find their way with a navmesh (around drops, props and lifts, and through
  teleporters), and hunting monsters investigate noises and follow your trail.
- **Gore you can see and hear.** Flesh dents, carved bullet holes, blood streams, back-splatter, bloody footprints, cut
  stumps, heads and limbs that pop, and a full set of gore sounds with their own volumes.
- **Faster and smoother.** Much faster map loads, fewer first-time hitches, dynamic resolution, a model depth pre-pass,
  models lit by every baked light, less CPU in big fights, and a Graphics Presets page.
- **Quality of life.** Rotating autosaves and a death menu, Continue rows on Select Campaign, a reorganised VR
  Settings with a Gameplay page, foldable menu sections, Reset This Page, Holster Setup by drag and drop, Item Pickup
  Mode, a Map Library with Slipseer's maps, and Create Bug Report.

## Full list of changes

### Combat and enemies

- Armed humanoids are on by default (Gameplay > Replace All Humanoids): every grunt and enforcer a map spawns is its
  armed version, in Quake, the mission packs and Arcane Dimensions.
- Knights and death knights are armed humanoids too: they carry their blades and, rarely, guns; the death knight still
  casts his spell, on a cooldown.
- Combat > Armed Grunts sets the odds of each weapon tier, dual wielding, and a gun with a melee weapon (axe or dagger).
- Armed humanoids fire their weapons with the player weapons' code, from each muzzle, using your Weapon Damage settings.
- Disarming by tug-of-war: grab their weapon with one or two hands and pull; they hold on, headbutt, shove or fire at
  you, and their body moves with the tug. A hard pull can lift them off their feet.
- Disarming by a deliberate strike on the weapon or the hand, by a shot to the gun (a point-blank shotgun blast usually
  does it), or by taking the weapon of a downed or ragdolling one.
- A dual wielder loses only the struck weapon; a disarm attempt wakes and staggers its target.
- Disarmed enemies kneel to pick a lost weapon back up, and one holding a weapon that pairs picks up a second.
- They can disarm you by the same rules: precise shots and blows on your gun or hand knock it to the floor (Combat >
  Armed Grunts > Disarming: Your Weapon > You Can Be Disarmed; off by default in 1.1 while it is tuned).
- They parry your blows with their weapon, with preventive and wind-up parries, and their strikes use the whole body.
- Blade on blade: weapons that meet clash and stop each other.
- They keep a fighting distance and their melee spacing, shove you when you are up close, lean into strafes, avoid
  shooting their friends, and aim at whatever part of you shows behind cover.
- They carry ammo and spare magazines and drop them as loot; replaced enemies drop only what they hold and their pack.
- Worn backpacks on the ones carrying ranged weapons: lift one off a living enemy, pull it hard to lift the wearer, or
  loot it from the body.
- Pyro and sprayer grunts wear fuel tanks you can shoot.
- Armour plates on armed humanoids: helmets, cuirasses and limb plates that dent, crack, split in two and fall off by the
  plate a shot meets, with sparks, debris and a metal crack. The death knight's helmet is fitted, with cheek plates.
- An optional taunt when they first see you (Taunt on First Sight, off by default).
- Their held weapon follows the hand when they go ragdoll.
- Active ragdolls: enemies react to hits with their whole body, stagger, fall, and get back up to keep fighting.
- A staggering enemy keeps its balance on its feet: knees bent, stepping, arms flailing.
- Enemies topple as ragdolls when it makes sense: on stairs, ledges and slopes, from close shotgun blasts, strong blows
  and explosions, or when badly wounded (Combat > Ragdoll Triggers).
- Ragdolls grab ledges and pull themselves back up, guard their heads, grab others, and drown in deep water.
- Shoved off a ledge, an enemy backpedals and fights for its footing instead of being launched.
- A knocked-down enemy that is still alive stirs where it lies (a head turn, a twitch, a limb drawn in).
- Enemies that die against a wall reach out to it and slide down it.
- Zombies fall as active ragdolls (Quake, the mission packs, Dimension of the Machine and Arcane Dimensions); a beheaded
  zombie stays dead.
- The enforcer, zombie and mummy ragdolls (and AD's variants) bend at a proper knee.
- Monster Groups (Combat > Monster Groups) set every ragdoll trigger and action per group (Grunts, Knights, Ogres,
  Zombies, Bosses...), shared by similar monsters across Quake, the mission packs and mods.
- Held enemies struggle against your grip: arms over their head, slaps, squirms, voices, a look at you; breaking free
  pushes you back. Dragged by the legs, they struggle too.
- A corpse or downed enemy held in front of you works as a body shield against the monsters' shots.
- Enemy melee under one set of rules: blows land only in front of the attacker, so sidesteps work, and bigger monsters
  swing wider (Combat > Enemy Melee, with Show Melee Arcs and Enemy Melee Speed).
- Monsters keep out of your head while they attack you.
- Monsters' ranged shots follow where they face: turn away from them and they must turn to hit you.
- Big monsters have a little less health, up to 20% for the biggest (Combat > Monster Health).
- Mini bosses and elite knights resist knockdowns and ragdoll tricks better.
- Walking monsters path with a navmesh: around drops, props and lifts, and through every teleporter they fit, both ways;
  ranged monsters follow you through gates too.
- Stealth AI: a hunting monster investigates noises, hears clues, and follows your trail, through teleporters too.
  A touch no longer gives you away.
- Ogres take care not to hit their friends.
- The Machine Games' infected drop their weapons as they turn; a gibbed Freddie drops his super nailgun.

### Melee and throwing

- Melee no longer depends on frame rate: a swing does the same at 72, 90 and 120 Hz.
- One swing hits everything its edge passes through (up to 4 things, corpses included), and a wall only once.
- Swept blades: the whole blade connects along its swing, and heavy heads strike with their whole volume.
- Melee damage: a flatter curve, sword tip speed counted, a hard cap per blow.
- Melee moves ragdolls by their mass: corpses and downed enemies are no longer flung across the room.
- Melee on walls and floors connects over the whole contact.
- Beheading is judged where the blade meets the flesh.
- Directional parry, the only parry for you and for enemies: a weapon across the blow's line always blocks; turning it
  into the blow at the last moment staggers the attacker far harder. Dog and fiend lunges are parried with a level
  weapon.
- Parry balance by weapon, grip and timing against each monster group's Parry Poise: a block, a flinch or a stagger.
  Parried enemies stagger back as ragdolls.
- Shoulder charge (replaces the tackle): run at an enemy side-on, shoulder leading, to knock it down (Charge Leniency
  sets how strict it is).
- Shoves: running shoves carry your speed, two-handed shoves hit 25% harder, a shove off a ledge tips the enemy over;
  shoves, slaps and bashes draw no blood.
- Neck snaps (a twist along the hand's line), judo throws in the direction you throw, held throws and lifts.
- Leg sweeps: a staff through the shins, a judo foot sweep, strong blows to the legs, short weapons, from any side.
- Leg grabs: pull a humanoid's legs out or lift it by them, with one hand or two; a body dragged by the legs is held
  firmly once it lies down.
- Stomp: landing on a standing monster knocks it down; crushing from a height pops heads and limbs.
- Bodies you throw topple the enemies they hit, by weight; two hands throw heavy things farther.
- Batting a projectile back takes a real swing, not a stray wrist move.
- Close quarters brakes your momentum.
- Weapon clash: your own two weapons collide with each other.
- Heavy weapons have weight: the laser cannon, hammer, mace, rocket launcher, Widowmaker and chainsaw lag, droop and
  kick in one hand; two hands steady them and swing heavy blows.
- Embedded blades (on by default, Gameplay > Embedded Blades): swords, axes, the dagger and thrown blades stick in
  bodies and corpses; pull them out, or move the body by the blade (Blade Hold Strength). Blades left in a body keep
  their blood; a blade in a crate holds by its handle.
- Skewering: gibs stay on the blade that pierced them; a gib pulled off the blade stays in your hand; a gib you throw
  can be skewered in the air; a severed head or limb stays stuck on the axe.
- The dagger: a short blade beside the swords and in the toolgun, a starting melee weapon; it sticks a third as deep as
  a sword and throws point first. Armed humanoids carry it too.
- The reverse grip: press B (Y) holding a sword or the dagger to turn it blade down for stabbing down. The flip turns
  about your fist and carries through two-handed holds (mirrored), passing between hands, drops, throws, and taking a
  reversed blade off a table or out of a body; catch a tossed dagger blade down and you hold it reversed. Held in two
  hands, it flips only with both hands on the handle (not with the other hand on the blade or anywhere else).
- Thrown swords and daggers come round point first; a blade thrown blade first sinks deep and kills whole.
- The thrown dagger settles point first more readily, however it leaves the hand, with a gentler pull than a sword's.
- Thrown swords and daggers hit hard but never gib, however fast or whichever way they strike (Quad still does).
- One consistent throw for props and weapons.
- Grab floating pickups out of the air, and catch things in mid-air.
- Passing a weapon between hands eases across.
- Closing a hand on a gun resting on it takes it; a torch on your torso can be taken whatever the other hand holds.
- A closed fist that strikes a wall torch takes it.
- Punches land on shootable doors, and the hand stops at walls.
- Crates need two strong hits to break; light ammo no longer breaks them.
- Bloodlust heals only from enemies, not from crates and props.
- Earned bullet time: the meter fills from the damage you deal to living enemies (a little faster on Easy, slower on
  Hard and Nightmare); pressing it empty says so on your wrist.
- Grenades: one fuse for every grenade, launched grenades tick, a lit grenade caught out of the air restarts its fuse,
  and grenades bounce off knocked-down bodies.

### Weapons and guns

- New: the nail pistol (magazine-fed, moving slide, slide lock, auto re-rack, glowing sight inserts, an off-hand cup
  grip), found beside ammo boxes and on crates.
- New: the heavy revolver (swing-out cylinder, single shells, a flick to open and close, a push to eject, a hammer that
  cocks fully before it falls).
- New: the flare gun (flares that set things alight, stick to walls and light the room), on crates.
- The grunts' burst rifle is made anew.
- The shotgun and nailgun are remodelled with new skins; the super shotgun's open model is cleaned up.
- Iron sights: the shotguns' sight housings, the super shotgun's notch, the nail pistol's glowing inserts, the lightning
  and plasma guns' sights.
- Ammo screens built into the guns in their own housings (shotguns, nail pistol, nailguns, lightning gun, grenade and
  rocket launchers, the multi-rocket, AD's Widowmaker and Plasma Gun): the magazine count big and bright, the total
  small; it works in the left hand too. The lava nailguns and the plasma lightning gun have the same housings and
  screens.
- Procedural recoil per weapon (kick, rise and recovery) replaces the baked kick; two-handed recoil for any grip; the
  muzzle rises about the gun's own axis.
- Barrel heat: barrels glow with sustained fire (Barrel Heat settings).
- Punchier gunfire sounds and haptics.
- Shotguns load two shells at a time.
- Pouch reloading loads a magazine gun from the magazine where it is drawn; magazines slide home and out; magazines
  that fall into the pouch are kept.
- Per-weapon reload modes (their own menu page).
- Nailgun and super nailgun magazines show their top nails, which disappear as they empty.
- The ammo pouch shows what it holds: shells, two rows of magazines, grenades and rockets.
- Nails are smaller and faster, catch fire passing through any flame, and bleed and dent what they hit.
- The Plasma Gun's cell holds 16.
- Weapons held off their handle collide with the level.
- A rolled gun keeps its barrel and sights rigid with the hand.
- Gun Model Pitch is merged into Hand Pitch (your value carries over).
- Projectiles keep less of your motion: strafing no longer throws nails and rockets off your aim.
- Shooting yourself (headset only): your shots hit your own body if you point a weapon at it; your own flare sets you
  on fire.
- Thrown magazines hit as light props.
- Spent enemy guns and empty chainsaws lose their loot beam and can't be force-grabbed; empty magazines are not
  force-grabbed either.
- The rocket and multi-rocket launchers' vents are real recesses.
- Arcane Dimensions' weapons as VR models: the Widowmaker (a two-handed, three-barrelled Gatling auto shotgun with a
  foregrip that aims two-handed), the Shadow Axe, the Plasma Gun, the crossbow (the bolt laid in the groove, an animated
  string), the flamethrower, the poison sprayer and other gun variants.
- AD's melee weapons made anew: swords (the Phantom Swordsman's, the Dark Prince's, the justice knight's, the
  judicator's, the Death Fury's, the zombie knight's), the hammer and the mace (heavier now), and the death sergeant's
  flail with a physical ball and chain.
- AD fuel tanks: a dropped flamethrower or sprayer lies with its tank and hose; shoot the tank for a fire field or a
  poison cloud, cut the hose with a blade, or throw the tank.
- AD's weapons can be used in any map and taken from the toolgun.

### Gore, fire and water

- Flesh dents that stay on corpses; every hit dents.
- Carved bullet holes, nail holes and blast craters in walls; melee leaves blunt dents and blade grooves.
- Blood streams from wounds that follow the ragdoll; blade melee opens them too.
- Back-splatter on walls, teeth from face punches, heads and limbs popped by impacts and stomps.
- Bloody footprints and handprints; cut flesh on stumps and severed limbs.
- Wounded enemies drip blood; wet things drip water.
- Gibs follow one rule whether thrown, held or struck; gibs pop underfoot; breakable props follow the same rule.
- Corpse limbs wear off under any weapon; the buzzsaw cuts ragdolls.
- Blood marks no longer vanish mid-fight, and piled-up blood no longer leaves square holes.
- Peripheral hit and low-health blood feedback, with an adjustable vignette.
- Gore preset: Full, Minimal, Off or My Settings (Gore > Gore Preset, also in Run Setup).
- Flamethrowers light the room with flickering firelight, leave scorch marks, and fast hands stir flames.
- Map flames set monsters and bodies alight; water puts out flames and lava nails.
- Lightning leaves burn marks and arcs across water.
- Blood in water: bloody clouds on the surface, slower under water.
- Shoreline foam for each liquid: spray for water, a glowing crust for lava, green scum for slime.
- An even wet coat from liquids, with its glint (no longer on monsters and bodies over a liquid on a bridge or lift
  that never touched it).

### Movement, climbing and body

- Lying down and crawling: get low and you lie down and crawl, pulling yourself with your arms.
- Swimming tilts the body.
- Body and Equipment: your legs fit your standing height, your gear fits your body, your feet step in time with your
  run, and your footsteps follow your speed.
- Standing up under a low ceiling works.
- Crouching is slower and silent.
- Climbing: a mantle starts once your head clears the ledge; your feet find footholds while you climb; crates, barrels
  and stacks can be climbed from any side.
- Seated play: the game raises you to standing height, keeps your legs planted, and adds a seated holster layout, a
  floor reach and a crouch toggle (VR Settings > Height and Hand Calibration > Seated Play).
- Full-body tracking (experimental): Quest 3 body tracking through Virtual Desktop drives your legs, trunk and elbows.
- VR Calibration: Stand Tall's eye height becomes your height, with Undo; standing skips the separate height step.
- Holster Setup: place the holsters and the ammo pouch by drag and drop, with a mirror image of your body.
- Magnetic holsters bind the weapon in your hand at an empty holster and pull a thrown gun back on a physical chain.
- Hide holstered weapons per pair.
- Pickups: bigger zones for ammo, health and backpacks, a cue when you are full, optional auto-collect into the palm,
  and soft light pillars over weapons and magazines lying about.
- Smooth + Teleport: walk with one stick and teleport with the other.
- Sliding in VR (experimental, off by default): while running, pull the turning stick straight down to slide, the
  same slide as on the desktop screen (VR Settings > Locomotion > Sliding).
- Your hand gives when you press on a braced prop, and hands rest on props instead of sinking in.
- Player Appearance: gloves (fingerless or full, any colour, worn leather, or in Quake's art style), skin tone, a
  feminine body build, hand shape and voice, painted nails, Ghost Hands, and a Randomize button.
- Your body wears the detailed Quake ranger head (seen in mirrors, shadows and the third-person camera), wounded where
  struck (Appearance > Ranger Head).
- Your armour shows as plates on your body and sheds as you take hits.
- Quad Damage, the Pentagram and the Biosuit make you glow.
- Vomit Mode (off by default): after you die, the view rides your body's head.

### Flat-screen play

- `-flatscreenmode` plays the full game without a headset; the installer adds a "Quake VR Unleashed (Flat Screen)"
  shortcut to the desktop (its own checkbox, on by default) and the Start menu.
- The hands and guns are drawn and simulated as in VR, posed per weapon (pistols, rifles, shoulder launchers) and aimed
  at the crosshair from the muzzle.
- Mouse: with one weapon, left click fires and right click aims down the sights (hold or toggle); dual wielding, each
  button fires one hand. Some guns (the nailgun, nail pistol, crossbow) zoom instead.
- ADS sensitivity: a multiplier and a match distance.
- Keys: Q / E lean (round corners and over the gun at a wall), 3 swaps hands, crouch at a run slides, X lies down,
  V melee, G drops, mouse 4 or R force grabs, mouse 5 shoves, Z bullet time, F7 the toolgun, the middle button the
  holster menu; hold jump at a ledge to mantle onto it.
- Ladders made of rungs or stacked ledges (like the tutorial's) climb on a flat screen: walk into one and the hands take
  its rungs hand over hand; forward climbs (looking down, or back: down), jump pushes off, and you climb over the top or
  back off it onto the ladder (Desktop Screen Play > Ladders).
- Every flat-play key can be bound from the menu (Desktop Screen Play > Flat Keys).
- Flat reloading with the guns' own parts, a quick grenade, the drop key and the hand swap.
- Melee: punches, shoves and overhead swings at the crosshair; a two-handed stance with an empty off hand; the right
  mouse button raises a parry guard.
- The slide has stamina, carries a charge and pushes props; the lean turns the arms with the torso and the mouse
  follows its roll.
- Force grab: palm first, held props in the fist and usable as weapons.
- The holster menu: drag weapons between hands and holsters, drop both, unload.
- The visor HUD (on by default): the view through a helmet visor in the wrist gadget's style, with panels at its
  edges, its own crosshair, centre messages in a popup, the log on top, and a glitch when you are hit.
- An optional over-the-shoulder third-person camera (Desktop Screen Play > Third-Person Camera, off by default).
- Speed FOV, hit feedback, blood at the camera, Vomit Mode on flat deaths.
- Walls push the gun's muzzle up instead of through them; crouching eases; crouch and swimming speeds.
- The menus use the headset's layout in a panel with the mouse as the laser (Menu > VR Menu Layout on a Flat Screen),
  with zoom buttons and a Quit dialog.
- Flat-screen TAA and antialiasing options, and flat tone mapping.
- Classic style works on a flat screen too.

### Menus and settings

- VR Settings reorganised into fewer pages and submenus, with a Gameplay page holding every big gameplay switch.
- Several menu panels at once; they stay where you put them and stay when the menu closes. Grip just outside a panel's
  edge to move it, and push or pull it in depth.
- Menu Look (HUD and Menus): long pages fold into sections, a Search result opens with its row centred and glowing,
  choices such as Difficulty are rows of pictures, and the mouse wheel scrolls a page.
- Menu Detail (Standard, Advanced, Developer) sets how much the settings pages show.
- Reset This Page in every settings page's footer (press twice), on the weapon and held-object offset pages too.
- Graphics Presets page with a My Settings entry.
- Gameplay presets (Classic / Light / Unleashed / My Settings) and Classic Quake: no reloading, plain grunts, Quake's
  look, Classic recoil; switching back restores your settings.
- Autosaves: three rotating autosaves per map, made only when it is safe; Load Autosave on the main and pause menus;
  Autosave Now.
- A death menu with Respawn, Restart the Level and your autosaves.
- Select Campaign shows each campaign with its Continue row; Arcane Dimensions and Arena Mode sit with the campaigns,
  tagged experimental.
- Map Library: Slipseer's maps beside Quaddicted's (a Source filter), screenshots for both, a Continue button for
  installed maps and a difficulty pick on Play.
- Item Pickup Mode: Immersive (default), Auto-Collect or Vanilla, and a Carry Ammo and Health toggle.
- Turning Mode Off.
- Fonts upscaled smoothly, with an outline and a drop shadow, per kind of text (Classic keeps Quake's look); world text,
  ammo screens and holograms follow the style.
- Text floating in your view follows your head smoothly.
- The main menu centred with the VR banner beside the buttons; quicksave and quickload shortcuts in the menu.
- The build number and date in the menu corner.
- Your config holds only what you changed, so new defaults reach you; Save My Settings as Defaults.
- Old and duplicate options retired; configs carrying them stay quiet.
- Create Bug Report (Options, VR Settings > Headset, and a Start menu shortcut) zips your logs, crash reports and
  settings; every run keeps its logs.
- The installer offers a settings page after installing, and an optional Arcane Dimensions download.

### Campaigns, maps and modes

- Arcane Dimensions (experimental): Play > Official Campaigns, after a prompt. AD's maps, monsters, items and rules
  played natively, with Quake VR's physics, ragdolls, gore and melee.
- AD's monsters get active ragdolls, hit reactions (a flinch, a buckled knee, a limp), stealth and drops: knights,
  death knights, hell knights, death guards and furies drop the weapons they carry, the crossbow knight his crossbow,
  ogres their chainsaws.
- AD's health vials heal as Quake VR health and are easy to grip; AD pickups can be taken by hand; AD explosions throw
  props and ragdolls; AD's flamethrower, poison and plasma bolts have new effects; AD maps load faster.
- Armed humanoids appear in AD's maps too.
- Arena Mode (experimental, Select Campaign > Arena Mode, with its own short Run Setup): the vrarena map's waves grow
  with you; a budget director gives each wave a theme (Melee Rush, Ranged Siege, Elite, Knightfall...) and sends foes in
  groups, with surges and ambushes.
- Arena: armed and armoured foes with every armed humanoid weapon, boss waves with perks, supplies and armour shards
  after each wave, countdown beeps, banners with the wave, foes left, time and score, the last foes seen through walls,
  foes that use the stairs and towers. No saving in Arena Mode.
- The arena in any map (Gameplay), and an Arena Wave Editor to make and save your own wave lists.
- Run Setup on every new game (New Game, a campaign's row, the Map Library's Play, the Levels list, the hub's
  teleporters): Start first, the difficulty where a map has no skill halls, Style, Gore, Armed Humanoids, Smarter AI,
  Starting Gun (Shotgun, Nail Pistol, Heavy Revolver, Nothing) and Starting Melee Weapon (Axe, Dagger, Nothing).
- Mutators: Melee Only, Glass Cannon, Ironman (no saving; a death restarts the level), Randomized Pickups and
  Randomized Enemies (Balanced or Anything Goes), Enemy Count (1x to 3x), and with armed humanoids: Ranged Knights,
  Wild Weaponry and Armored Foes.
- A save remembers its run; your last Run Setup choices are kept.
- The hub has a Classic Quake board.
- Monsters walk through every teleporter they fit, seamlessly.
- Crates and rocks keep their layout when you restart the same map.
- The tutorial gives a tip at the keycard.

### Graphics and performance

- Models lit by every baked light (the model light grid); lit liquids light models; an Ambient Light Level setting.
- Smoother monster animation: no vertex wobble, smooth model animation by default.
- Dynamic resolution: the eyes' resolution drops when the GPU falls behind, never above your render scale.
- A model depth pre-pass and your body drawn depth first, so each pixel is shaded once.
- The shadow atlas sized per map; shadow casters shared between lights and batched; off-screen bodies and gear not drawn
  or shadowed.
- Cheaper bloom; lighter particles (half resolution in dense bursts, faded near your eyes, size-capped).
- Much faster map loads: work moved to worker threads, fewer failed file lookups, rigs, hulls and normal maps prepared
  at load.
- Fewer first-time hitches: shaders, the first gore and new monster sizes are prepared while the map loads; no more
  freeze the first time decals meet a translucent window in AD's maps.
- Physics, particles and gore on the thread pool, more threads only in big fights where it pays.
- Less CPU per frame in big fights (area-grid searches, fewer allocations, a cheaper arm IK) and in maps full of moving
  platforms (vkQuake's pusher grid); a faster QuakeC interpreter.
- The sound mix moved off the main thread.
- Memory is one heap: the old `-zone`, `-heapsize` and `-minmemory` options are no longer needed (they are ignored).
- Graphics settings: rows shown by mode, Resolution Scale in steps, Heat Haze Detail and parallax depth by preset, a
  parallax surface level for props on carved floors.
- Temporal anti-aliasing (experimental), fractional sample shading and specular antialiasing.
- Single-pass stereo (experimental).
- Eye-tracked foveation (experimental, off by default): on headsets whose runtime offers eye gaze (e.g. Quest Pro via
  Virtual Desktop) on NVIDIA GPUs, full detail follows where you look (Graphics > Eye-Tracked Foveation).
- Comfort > Soften Flickering Lights smooths animated lights (off by default).
- The spectator camera is skipped while the desktop window is minimised.

### Audio

- Gore sounds: stump spurts, splatter, tearing, squelches, wet steps, gib slaps and drips, each with its own volume
  (wet chokes off by default); silent with gore off.
- Punchier gunfire, surface impact sounds by surface type, a second shell sound when shotguns load two.
- Swing sounds follow your speed: soft whooshes for slow swings, louder for fast blows.
- Heavy footsteps for enemies and armed humanoids, from real recordings; your footsteps' volume follows your speed.
- A metal crack when an armour plate breaks; smash sounds and thuds for ragdolls; louder wooden crates.
- A feminine voice option.
- Your weapon knocked out of your hand: a metal clang where it flies and a short alert only you hear (with a buzz on
  that controller in VR), its volume in Disarming: Your Weapon.
- Sounds no longer cut each other off; late sounds are kept in saves; fire crackles start again after loading.
- Looping sounds (a chainsaw's engine, flamethrowers, fires) always stop with their owner: no chainsaw idling on for
  ever after its knight was disarmed, shoved down, killed or gibbed.

### Tools and modding

- The toolgun spawns every expansion's monsters, items and weapons (Scourge of Armagon, Dissolution of Eternity, the
  machine games, Arcane Dimensions), torches and fires, at their final size, from its own panel with a ghost you hold.
- The toolgun's Brush tool builds brushes with shapes, rotation, baked light and saving.
- The physgun moves ragdolls naturally by the part it holds.
- Creator tools (flat screen or developer): the Rig Studio, Pose Studio and Grip Editor, the Weapon Studio, the Armour
  Studio, the Ragdoll Tool (`-ragtool`), the animation recorder and the Blade Shape editor.
- Recording: takes and clips for trailers, and a DaVinci Resolve title kit (embossed metal feature titles with blood
  drips and soft drop shadows). While OBS records, Ghost Hands, debug markers and tips are left out
  (Graphics > Recording > Clean Recording).
- Voice notes with context and an instant replay, and an in-game Checklist with Test buttons, for testers.
- Every model's source is a Blender file in the repository, with an add-on that exports on save.

### Accessibility and comfort

- Seated play with its own holster layout, floor reach and a crouch toggle.
- Ghost Hands: a translucent hand at your real controller, fading in near it.
- Soften Flickering Lights, Turning Mode Off, a thumbstick with a round deadzone, a response curve and a smoother change
  from walk to run.
- Smooth + Teleport locomotion.
- The super shotgun can be closed with B / Y.
- Item Pickup Mode's Vanilla (walk over items) and Auto-Collect.
- Show Melee Arcs to see where enemy blows land.
- The flat-screen mode and the flat-screen menu, for play without a headset.

### Bug fixes

- A thrown blade stuck in an enemy that dies from the throw no longer stays floating in the air: it falls when it can't
  stay with the body (a grunt's gun gone from its hands).
- The Matriarch of Decay: the palace button no longer crashes the game.
- The Sacrifice: its two vores no longer crash the game; the Rune no longer appears before the sacrifices.
- Shub-Niggurath's teleporter takes you to the spiked ball again after loading a save.
- Enemies are active ragdolls again after loading a save.
- A crash when an Arcane Dimensions gargoyle's fireballs hit you.
- A crash on quickload.
- A crash holding one enemy with both hands.
- A crash switching campaign with the Levels list on a panel.
- A crash around map loads while a motion take was recording.
- A crash saving in some maps.
- A crash from a sprite freed under a touch.
- VR no longer drops to 10 fps when the game window loses focus.
- A focused desktop window no longer slows the headset to the monitor's refresh rate (GitHub issue #79).
- OBS Mirror's OpenXR layer, which crashed some headsets at startup, is skipped.
- Arcane Dimensions: the sky and world no longer flash dark when you turn or move (a fog bug).
- Arcane Dimensions hub: the original Quake slipgates on the lower level work.
- Arcane Dimensions: the healing lockout, the flamethrower's missing sound, the Dark Prince's ragdoll triggers, flames on
  the body, the hunter ogre's and death sergeant's lost ragdolls, a beheaded zombie knight getting up again.
- Sounds (footsteps, nearby sounds) no longer cut each other off.
- Late sounds are kept in saves; fire crackles restart after loading.
- A snapped neck stays loose after loading a save.
- A stagger on a step or crate no longer drops the enemy; no freeze at the end of a stagger; get-ups on slopes work.
- No more accidental slow motion, or get-ups that glide.
- A hand holding an ogre's or death knight's ledge lets go when struck.
- A dead fiend no longer leaps at you.
- Glowing specks on the guns (Model Warp's half-texel shift, model-pack skins from other folders).
- The super shotgun's shot is no longer refilled by the hip holster.
- With the feminine voice, the landing grunt uses it too.
- The magnetic holster's chain no longer stretches on fast swings.
- Two-handed grips close together on a crate no longer pull one hand in.
- A turned teleporter's exit turns your body with the gate.
- The body no longer stays crouched after calibrating.
- The death menu opens in VR, in front of you.
- Running and thumbstick walking hitches, and a slightly angled walk direction.
- A teleport is no longer lost with the headset above 72 Hz; quick button taps between two server frames are no longer
  lost.
- The wet glint comes back after a quickload.
- Monster vertex wobble and the idle knight's jolt with smooth animation.
- The changelevel error and a cold first frame.
- A QuakeC string search that always reported a match (affects mods).
- Physics engine updated with upstream fixes.
- The grunts' burst rifle is named "Burst Rifle" everywhere (it was "grunt's shotgun" in the holster menu).
- Test hall: command buttons (NEW GAME, VR HUB) are pressed only by you and what you threw; a monster or a blast-flung weapon no longer restarts the game.
- The toolgun's Armed Humanoid page can pair a gun with an axe, dagger or other one-handed blade, as the armed
  replacements do.
- Grunts holding a gun and an axe hurt you and can knock your weapon out again when you hold a weapon in front of you
  (their axe blows near your weapon were held back and never landed); a parry across the blow still blocks it.
- Flat-screen: B switches the weapon to its other ammo (rocket and grenade launchers, thunderbolt), rebindable on
  the Flat Keys page.
- One-way teleporters (start's and id's maps): leaning back across where one comes out no longer hides your hands or
  shows you yourself in the teleporter you came through.
- Flat-screen: hip fire through a see-through teleporter hits what the crosshair is on beyond it (the guns aimed at
  the teleporter's surface, and the shot went off once through).
- Many smaller fixes and cleanups.

### Known issues

- **Arcane Dimensions** is experimental: expect bugs and balance changes.
- **Armed humanoids**: a knocked-down one that aims high may pull its arm in.
- **Flat screen**: wrists can bend sharply on some holds, and a gun's end can poke a few centimetres into a wall at the
  limit.
- **Multiplayer**: not yet tested with 1.1's new systems (the Multiplayer menu says so); co-op and deathmatch may have
  rough edges.
- The installer and the game are not code-signed, so Windows SmartScreen may warn you (More info > Run anyway).
