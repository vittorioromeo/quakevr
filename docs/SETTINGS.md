# Quake VR settings

Everything you can adjust, and where to find it. The [README](../README.md#first-steps-and-basic-tweaking) covers
the first things to set. This page goes further: every menu page, the controls, the console, and the config files.

- [The VR Settings menu](#the-vr-settings-menu)
- [Advanced VR Options](#advanced-vr-options)
- [Graphics presets](#graphics-presets)
- [Weapon offsets](#weapon-offsets)
- [Controls and bindings](#controls-and-bindings)
- [The console](#the-console)
- [Important settings by topic](#important-settings-by-topic)
- [Config files, shipped defaults and resetting](#config-files-shipped-defaults-and-resetting)
- [Diagnostic commands](#diagnostic-commands)

## The VR Settings menu

Open the menu with the controller's menu button (on Index controllers, the left B button) and pick **VR Settings**
(also Options > VR Settings) or **Advanced VR** (the corner's *Advanced VR* button too). `menu_vr` in the console opens it directly, and `menu_vr <n>` opens page *n* (0 is VR Settings, 1 is
Advanced VR Options, then the pages in the order listed below; `menu_vr list` prints the numbers).

**Using the menus:** point with the laser from your hand and pull the trigger to click. Drag sliders with the
trigger held. With the left stick (the off hand's), up and down move between rows, left and right change a value; A
selects and B goes back. The right stick (the main hand's) only scrolls or moves between rows: it never changes a
setting, so navigating can't change one by accident (with *Swap Stick Functions* on, the sticks swap). *Back to Game* is at the top left, or hold the menu button. Each setting shows a line
of help at the bottom when you select it. In the headset the menus are taller than on the desktop, so more rows show
at once (Menu page > *Menu Height*, `vr_menu_height`: 1.6 times Quake's height, about 35 degrees up and down with
the shipped menu distance and scale).

**Menu Detail** (the last row of every page, `vr_menu_level`): *Standard* (the default: what every player sets),
*Advanced* (every gameplay, display and graphics setting: Advanced VR Options and its pages) or *Developer* (also
weapon and prop offsets and weights, ragdolls, hitboxes, the motion recorder, debug and tests, and the corner's
Checklist button). The corner's *Advanced VR* button switches Standard to Advanced. `menu_vr <n>` opens any page,
whatever the level. Each setting has one home page; other pages link to it ("Grenade Pouch: Hip Holsters").

**Search** (the corner's *Search* button, in the headset or with the flat screen's corner buttons): a text box
and a keyboard on the left (the laser or the sticks press its keys; a real keyboard types too), the results on the right,
best first, updated as you type: each with the pages it is on in small letters. It looks at every setting, action and
page, whatever Menu Detail shows: names first, then the help, the pages above and the cvar's name; whole words, word
starts, letters in order and one typo are all matches ("sanp tur" finds *Turning*). Pick a result to open its page on
it (a result above Menu Detail, marked with its level, raises Menu Detail); Back returns to the results.
`vr_menu_search <text>` prints the same results in the console.

**Changes:** a setting changed from its default shows a `*` by its label. *Changed Settings* (Advanced VR Options > Setup)
lists them all, from every page, and lets you change them there. *Reset This Page* at the bottom of a page puts that
page's settings back to their defaults (press it twice, within 3 seconds).

**First start:** with no saved config, VR Calibration starts the first time the headset is on (`vr_setup_pending`): your
height, your body, and the room's buttons for the main options. *Run VR Calibration Again* (Advanced VR Options > Setup) or the
main menu's first row runs it again.

**Live preview:** changes show in the game as you make them. In single player the game keeps running under the
settings pages, and it pauses while a monster is after you (Menu page > *Live Preview*).

The main page is for a first-time player: a few settings in each section, every one with a line of help. Everything
else, and each of these again, is under *Advanced VR Options* (the main menu's *Advanced VR* row or the corner's
*Advanced VR* button, not a row here; at Menu Detail: Standard they raise Menu Detail to Advanced). Search is the
corner's *Search* button.

| Section | Settings |
|---|---|
| **Height Calibration** | Height, *Set Height Now*, World Scale, Floor Offset |
| **Hand Calibration** | Show Controller (the real controllers drawn, to line the hands up; not saved), Hand Forward, Hand Inward, Hand Up (cm), Hand Pitch, Hand Yaw, Hand Roll (degrees), *Reset Hand Offsets* |
| **Locomotion** | Move Towards (Head, Left Hand, Right Hand), Default Speed (run or walk), Stick Deadzone, Swap Stick Functions |
| **Comfort** | Vignette (off, moving and turning, moving only, turning only), Vignette Strength |
| **Teleportation** | Teleport, Teleport Range |
| **Turning** | Turning Mode (smooth or snap), Turn Speed (smooth) or Snap Angle (30, 45, 90: snap) |
| **Flashlight** | Flashlight (on your belt), Flashlight Side (left or right hip) |
| **Lighting** | Ambient Light, Light Contrast |
| **Weapons** | Weapon Grip (hold or sticky), Two-Handed (off, basic, virtual stock) |
| **Body** | Body Type (full, torso and arms, only hands), Wrist Gadget Arm, *Reset Position* |
| **Haptics** | Vibration Strength (0: none) |
| **HUD** | HUD (wrist gadget or status bar), Crosshair |
| **Sound** | Volume, Music Volume, Spatial Sound |
| **Display** | Headset Gamma, Headset Contrast |
| **Scaling** | Render Scale, Upscaling (bilinear, FSR, NIS), Sharpening, Foveated Rendering (off, conservative, balanced, aggressive) |
| **Graphics** | Retro Textures, Retro Lighting, Antialiasing (off, 2x, 4x, 8x), Bloom, Tone Mapping, Bump Mapping, Parallax Mapping |
| **Reset** | *Reset All to Defaults* (press twice within 3 seconds) |

The settings that were on this page before (October 2026) are on their topic's pages: the Comfort preset, Turning
(smooth or snap 30/45/90 in one row), Move Towards with the moving stick's hand, Teleport Range and Room Scale on
**Locomotion**; Handedness on **Body and Display**; Gun Angle and Off Hand Angle on **Hand/Gun Calibration**; Dominant
Eye, Two-Handed and Two-Handed Hand-Off on **Aiming**; Haptics on or off on **Immersion**; Throw Speed and Throw Gravity
on **Carrying and Throwing**; Force Grab on **Force Grab**; the Graphics Preset on **Graphics**. *Body and Display*,
*Headset*, *Sound*, *Tips*, *Changed Settings* and *Run VR Calibration Again* are under Advanced VR Options > **Setup**.

**Body and Display:** Handedness (a preset: right- or left-handed, or Custom), Swap Stick Functions, Wrist Gadget Arm
and Flashlight Side (Handedness sets all three), World Scale, Floor Offset (A), Chest Flashlight, Body (off, torso and
arms, full body), Build, Holster Models, Status Bar hand, Desktop Mirror (off, left eye, both eyes), *Recording (Window
View)*. **Headset:** VR on or off, Restart VR, OpenXR Runtime, Render Scale, Upscaling (bilinear, FSR, NIS), Sharpness,
Foveated Rendering (off, conservative, balanced, aggressive), Hide Lens Corners; Near Clip, Held Items at the Eyes and
Float Depth (A).

Notes:

- **Height:** *Set Height Now* measures you while you stand straight. *World Scale* (1.25 by default) makes the
  world bigger or smaller around you. *Floor Offset* moves the floor up or down.
- **Hand Calibration:** one set for both hands, mirrored: *Inward* and *Yaw* are towards the other hand for each. 0 is
  the shipped calibration (`vr_menu_hands_*` stand for `vr_handcal_x/y/z/roll` with `vr_handcal_off_mirror 1`, and
  `vr_gunangle`/`vr_gunyaw` with `vr_offhandpitch`/`vr_offhandyaw`, each its default plus the row's value). If shots go
  above or below where the gun seems to point, change *Hand Pitch* first. Each hand's own values: Hand/Gun Calibration.
- **Move Towards:** `vr_movement_mode` 1 the head, 2 the left hand, 3 the right hand; 0 (the moving stick's hand,
  as before) shows as that hand here.
- **Vignette:** darkens the view's edges while the sticks move or turn you (a snap turn at once), never for your own
  steps in the room or a teleport (`vr_comfort_vignette`, `vr_comfort_vignette_strength`; off by default).
- **Reset Position** (`vr_recenter`): puts the body back under your head (where its box fits) and turns the torso to
  face where you look.
- **Vibration Strength** (`vr_haptics_strength`, 0 to 2): every vibration times this; 0 none. Haptics on or off
  (`vr_disablehaptics`) is still on Immersion.
- **Reset All to Defaults:** every saved Quake VR setting (`vr_*`) and the other settings on this page back to their
  shipped defaults, but not your calibration: what was measured or fitted to you stays (`vr_height_calibration`,
  `vr_floor_offset`, Body Calibration's `vr_bodycal_*` but its preview, the tweaks on it `vr_body_tweak_*`, the body's
  proportions `vr_body_arm_length`, `vr_body_eye_forward`/`_up`, `vr_body_torso_back`, and both hands' calibration
  `vr_handcal_*`, `vr_gunangle`/`vr_gunyaw`, `vr_offhandpitch`/`vr_offhandyaw`: *Reset Hand Offsets* resets those).
  Also kept: the config's bookkeeping (`*_version`, the tips seen, VR Calibration pending, the pages' places), Menu
  Detail, VR on or off and the OpenXR runtime. World Scale is reset.
- **Render Scale:** from 0.5 to 1.5 times the runtime's resolution. The image is resampled to the headset, so
  dragging it doesn't restart anything.
- **Upscaling:** below Render Scale 1, how the eyes are enlarged to the headset's size. *FSR* (AMD FidelityFX Super
  Resolution 1) and *NIS* (NVIDIA Image Scaling) keep edges and text sharper than *Bilinear*; they run within 40
  degrees of the lens centre (`vr_upscale_radius`), bilinear beyond, where the lenses blur anyway. The menus, HUD and
  wrist log are drawn afterwards at the headset's full resolution. *Sharpening*: the upscaler's sharpening (too much
  makes edges shimmer).
- **Foveated Rendering:** shades the scene coarser towards the edges of the lenses (once per 2x2 pixels, then 4x4),
  which the lenses blur anyway: 18% (conservative) to 64% (aggressive) less GPU time for the world in the desktop
  test headset. NVIDIA GPUs only (variable-rate shading, `GL_NV_shading_rate_image`); elsewhere it does nothing.

## Advanced VR Options

*Advanced VR Options* (Menu Detail: Advanced; also a row of the main menu) opens these pages, grouped by topic as on
its own page: Play, Combat, Movement, Carrying and Throwing, World, Gore, Body, Flashlight, Weapons, HUD and Menus,
Graphics and, at Developer, Debug. Most settings are sliders with a line of help. Long topics are split into pages of
about a screenful each. Pages marked (D) show only at Menu Detail: Developer.

**Play and World**

| Page | What's on it |
|---|---|
| **Play** | Official Campaigns, VR Calibration, the VR hub, the tutorial and the firing range; add and kick bots |
| **World** | Enemies hurt by liquids, ogres aiming grenades; enemy weapons and weapon drops (from enemies and ammo boxes, and their chances); explosion rumble and the low-health heartbeat |

**Combat**

| Page | What's on it |
|---|---|
| **Melee** | Swing speed, pommel and gun-butt strikes, punch, damage and range multipliers, Quad's extra melee, bloodlust; headbutt (speed, damage) |
| **Parry and Bash** | Parry angle, reach, stagger, damage reduction, drop chance and cooldown, unarmed parry; bash and shove (speed, damage, push) and their sounds; counter-attacks (window, damage, glow) |
| **Stamina** | The stamina pool for parries, shoves, strikes and climbing: costs, recovery, the tiring warning, what being exhausted does (damage, shake, slower running) |
| **Batting and Catching** | Batting projectiles back (reach, swing speed, timing, aim assist); catching and shooting grenades, blows setting them off; hand grenades and the grenade pouch |
| **Damage and Knockback** | Precise hit detection and its tolerances; damage to enemies, to you, self damage; positional damage (head, arm and leg multipliers, the headshot sound); knockback per source; hits knocking your hands |
| **Weapon Damage** | Each weapon's damage, the enemies' weapons and throws |
| **Enemy Weapons** | Ogres' chainsaws (fuel, the engine, the cord), grunts' burst rifles, enforcers' laser rifles |
| **Enemy Shoves** | Enemies shoving you when you are too close: delay, cooldown, damage, push |
| **Knockdowns** | Shove knockdowns: the chance (per monster, damage, one or two hands, stamina, over a ledge), time down, struggling, getting up ([vr-port/KNOCKDOWNS_2026-10-04.md](vr-port/KNOCKDOWNS_2026-10-04.md)) |
| **Bullet Time** | Slow motion on demand: trigger (a stick press, a wrist tap, the gadget's button), time scale, duration, recharge, the Sandevistan mode and the look |
| **Burning** | Burn damage and time, flames spreading to monsters, corpses and crates, lava nails, torches, the flames' look |

**Movement**

| Page | What's on it |
|---|---|
| **Locomotion** | Movement speed, default speed and run multiplier, joystick turning, lean and its recentre, room-scale jump, the Comfort preset (the vignette is on VR Settings) |
| **Climbing** | Climbing ledges and rungs hand over hand (on by default): ledges, grab leniency, mantling onto slopes, the grunt, climbing stamina, the hand's pose on a hold |
| **Swimming** | The stick's speed in shallow water, wading and swimming; stroke strength, speed curve, palm, recovery; telling a stroke from the return of the arms; *Reset Swimming to Defaults* |
| **Grappling Hook** | Dissolution of Eternity's hook: the rope (length, physics, thickness), reeling in and out, its buttons, swinging (air drag, top speed), pulling props |
| **Player Hitbox**, **Monster Hitbox** (D) | The smaller player box (and its crouched heights) and the monsters' boxes ([vr-port/HULLS.md](vr-port/HULLS.md)) |

**Carrying and Throwing**

| Page | What's on it |
|---|---|
| **Carrying** | Carrying ammo and health boxes; grabbing weapons by the fist, by their hotspots or anywhere; two-handed carrying; held things colliding with walls, hands and the world |
| **Throwing and Physics** (D) | Two-hand throw speed, how the throw is measured, analogue release, slow-motion throws, spin, throws by weight, aim assist; bounciness, friction, hitbox |
| **Force Grab** | Distance, aim cone, flick speed and turn, flight time and speed, arc, catch window, pointing particles and haptics, the outline and effects, box size |
| **Wall Torches** | Taking torches off walls, force-grabbing them, blows before one dies, burning, lighting again, shooting them off |
| **Rocks and Bricks** | Throwable rocks and bricks placed in maps: where, how many (in a map, in multiplayer, in an area), sizes |
| **Crates** | Breakable wooden crates placed in maps: density, stacking, health, pieces, what's inside, the crowbar, hiding behind them |
| **Gibs and Corpses** | Grabbing gibs and heads, their damage, destroying gibs, hard throws bursting them; gibbing corpses; pushable corpses; ragdolls (and, at Developer, *Ragdolls* and per-monster pages) |
| **Held Object Offsets**, **Held Object Weights** (D) | Where each prop sits in the hand, and its weight |

**Gore**

| Page | What's on it |
|---|---|
| **Gore** | Gore level, blood sprays, splat size, pools, dripping, gibs sticking (and thrown gibs), flies on heads, gib speeds; your wounds, burns and wetness; lightning shock (arcs, convulsions, burn marks, smoke) |
| **Gore - Decapitation** | Beheading by blades, the chainsaw and thrown axes; head pops by shotguns, lightning, lasers, thrown things and blunt blows, and their chances |
| **Gore - Limb Gore** | Limbs cut off or popped: chances, corpses, explosions, limbs lying about, their weight |
| **Small Gibs** (D) | Small gibs per hit and weapon, brain chunks |

**Body and Flashlight**

| Page | What's on it |
|---|---|
| **Body** | Walking legs (step rate, turning before stepping), wading and swimming kicks, armour and wounds, powerups, anchors, body collisions; the body's placement (torso, legs, eyes), crouch tilt |
| **Body - Arms and Pauldrons** (D) | Tweaks on top of Body Calibration (upper arm, forearm, the shoulders' place, rise and swing), arm length and stretch; forearm twist, wrist limits, elbows; pauldrons |
| **Body Calibration** | Measures your body (VR Calibration does too) |
| **Player Calibration** | World scale, floor offset |
| **Flashlight** | The flashlight on your belt: brightness, range, visible beam, shadows, cord, hue; flicking it over; grabbing it, clipping it on a gun or your head; placement (and, at Developer, its grips) |

**Weapons**

| Page | What's on it |
|---|---|
| **Immersion** | Body picks up items, haptics; holster mode (immersive or quick slots), reloading mode, holster haptics, weapon cycling, throw mode, throw damage and speed, dropped weapon particles; shell casings |
| **Hand/Gun Calibration** | Gun angle and yaw, gun model pitch, scale and height, gun height offset, off-hand pitch and yaw, finger grip bias, auto-close thumb |
| **Aiming** | Two-handed mode, threshold, virtual stock factor, hand-off; weapon weight (position and turn, with two-handed help) |
| **Weight and Damage** | How a held thing's weight scales its melee damage (heavier than, lighter than, least and most damage) |
| **Hotspots** | The virtual stock's shoulder, and the shoulder and upper (chest) holsters: their positions and reach, with *Show...* switches that draw them |
| **Hip Holsters** | The hip holsters and the grenade pouch: positions, reach, the holster models |
| **Lightning Gun in Water** | Discharging the lightning gun in water: shock damage to you and others, reach, the electrified water |
| **Weapon Effects** | Programmatic recoil and muzzle flash, enemies' muzzle flashes and smoke, bullet tracers |
| **Weapon Offsets**, **Weapon Weights**, **Fingers and Collisions** (D) | Each held weapon's settings (see [Weapon offsets](#weapon-offsets)) |

**HUD and Menus**

| Page | What's on it |
|---|---|
| **Wrist Gadget** | HUD mode, which arm, size and placement |
| **Screens** | The wrist gadget's screen (level and stats, light, CRT look, glow, messages on the wrist); the weapons' ammo screens (weapon text, margin, CRT look); map boards as CRTs and their hue |
| **Colours** | The Player Effects Hue and saturation (that the force grab, teleport arc, crosshair and menu laser follow); the gadget's screen and casing; the iron sights, force grab, teleport arc, crosshair and menu laser hues; the iron sights' and the force grab's saturation |
| **Status Bar** | HUD mode, status bar hand, scale, offsets and angles |
| **Crosshair** | Crosshair type, depth, size, alpha, hue, height offset |
| **Menu** | Menu scale and distance, VR menu style, laser hue, row spacing, menu height, live preview, reopen where left |

**Sound** (Advanced VR Options > Setup; Volume, Music Volume and Spatial Sound on VR Settings): Steam Audio's spatial sound (HRTF, occlusion, air absorption, room
reverb, sounds from your hands, Doppler, near field), the underwater muffle and the mix limiter.

**Tips** (Advanced VR Options > Setup): the first-time tips (distance, line of sight, delay, time shown, the panel), *Show Tips
Again*.

**Debug** (D): the checklist, voice notes, and the Views, Logging, Profiling and Memory, Reports, Tools and Tests
pages.

**Graphics**

| Page | What's on it |
|---|---|
| **Graphics** | The [preset](#graphics-presets); anti-aliasing, smooth textures, fence coverage, relit maps, headset contrast (gamma is on VR Settings); links to the pages below; the performance profile and the FPS counter on the gadget (the memory log is on Debug - Profiling and Memory) |
| **Relighting** | Relighting maps in the game: this map or many, the light sliders, the progress ([RELIGHTING.md](RELIGHTING.md#relighting-in-the-game)) |
| **Slipgates** | Slipgate views and seamless slipgates (walking through), enemies seeing and shooting through them, the gates' look |
| **Retro Textures**, **Retro Lighting** | The retro look: blocky, palette-snapped textures per category of thing, and banded, dithered or blocky light (presets: Software Quake, Blocky Lightmaps, Banded and Dithered) |
| **Recording** | The desktop window's view for recording (smoothing, zoom, field of view, the spectator camera), slow motion for footage, highlight markers ([vr-port/TRAILER.md](vr-port/TRAILER.md)) |
| **Lights** | Light contrast, coloured lights, dynamic lights (falloff, uncapped, on models, angle); the lights of muzzle flashes, explosions, projectiles, lava nails, lightning beams, torches (count, brightness, shadows) and ammo screens |
| **Shadows** | Shadow-casting dynamic lights and map lights (count, detail, strength), muzzle-flash shadows, your shadow, softness, distance, atlas, bias, statistics; blob shadows |
| **Surfaces** | Light sheen and its anti-aliasing; bump maps (depth, in map light, real light directions, on models); parallax (depth, distance, items, models); detail textures |
| **Liquids** | Waves, reflection, refraction, glints, lava glow, real waves, shoreline foam, heat haze; under water (caustics, the view, wobble); splashes, rings, ripples and water sounds |
| **Post-processing** | Bloom (threshold, size, white and coloured lights, adapt); tone mapping, exposure, colour grade, dither |
| **Models and Effects** | Model lighting, models lit as the world, directional ambient, rim light, hands' least light, weapon reflections; screen, text and sight glow, sight hue; soft particles, decals, gib blood |
| **Particles** | Particles on/off, Quake VR particles, particle multiplier; large fireballs per explosion; *Explosion Debris* (physical chunks) and *Fire Particles* pages |
| **Transparency** | No vis (debug), water, lava, teleporter and slime alpha |

## Graphics presets

*Graphics > Preset* sets the shadows, dynamic lights, model lighting and the look of every Graphics page all at
once. Change single settings afterwards to taste. The preset then shows *Custom*.

| Preset | Shadow-casting dynamic lights | Map lights casting shadows | Shadow quality | Shadow distance |
|---|---|---|---|---|
| Off (Quake) | none | none | – | – |
| Low | 2 | none | 256, 4 taps | 1024 |
| Medium | 4 | 2 | 512, 4 taps | 1536 |
| High | 6 | 4 | 512, 9 taps | 2048 |
| Ultra | 8 | 4 | 1024, 16 taps | 3072 |

*Off (Quake)* also turns off model lighting, per-pixel dynamic lights on models and blob shadows, and restores
Quake's look. The shipped settings go beyond Ultra in places (12 shadow-casting dynamic lights). They were tuned on a
fast PC, so pick a preset if your frame rate suffers.

Everything else on the Graphics pages (bloom, bumps, parallax, detail textures, tone mapping, water, and so on) has
its own switch or slider. For the cost of each, see the profiler in [INSTALL.md](INSTALL.md#performance).

## Weapon offsets

Each weapon model has its own set of settings: where it sits in the hand, where the hand sits on it, the muzzle,
where the other hand holds it two-handed, the ammo screen, and its weight. **Advanced VR > Weapons > Weapon
Offsets (Held Weapon)** (Menu Detail: Developer) edits the weapon your main hand is holding. Open it while holding the weapon in a game.

The main page has the weapon's title, *Edit the Other Hand's Weapon* (every page has it), *Inherit From* (use
another weapon's settings), **Posing Mode** (pose the weapon in your hand, its hotspots, or it in a holster), and a
page for each part of its settings:

- *Edit the Other Hand's Weapon* switches to the off hand. The pages show the weapon held when they were opened, so
  reopen them after changing weapons. With an empty hand, only **The Hand** is offered: it moves the hand model.
- **Hand and Grip:** where the weapon sits in the hand (offset, angles, scale, hide the hand), the tuning aids (Show
  Controller, its laser, the controller preview), the hand and weapon moved together, and the hand moved alone.
- **Fingers:** how the fingers wrap the weapon (automatic or manual curls), each finger's bias, the thumb's place.
- **Muzzle and Sights:** *Align Sights to My Aim*, the dominant eye, the sight line, where shots start (Muzzle) and
  where they go (Shot Pitch and Yaw).
- **Two-Handed and Hotspots:** whether the other hand may hold it, its hotspots (grip, blade, cup) and how that hand is
  drawn there, and the two-handed aim.
- **Virtual Stock:** how the aim turns while the weapon is steadied at your shoulder (*Stock Pitch*, *Yaw*, *Roll*,
  0 by default; `vr_wofs_stock_pitch|yaw|roll_NN`), how far the stock steadies it now, and the virtual stock's
  settings for every weapon.
- **Ammo Screen:** position, angles and scale, shown or hidden.
- **Holstered:** how it lies in each kind of holster, with a preview.
- **Effects:** recoil, muzzle flash and tracers for a model without its own.
- **Weight, Melee and Throwing** opens Weapon Weights: its mass, balance, spring, melee and throw damage.
- **This Weapon:** **Print Changes to Console** (prints this weapon's settings that differ from the defaults, as
  `vr_wofs_...` lines to copy into a config or send to the author), and **Reset This Weapon**.

In the console these are the `vr_wofs_<setting>_<NN>` variables, where `NN` is the weapon's slot. They are saved in
your config like everything else.

## Controls and bindings

Controller buttons are ordinary Quake keys, so they can be bound to any command or alias, in **Options > Key
Setup** (press the controller button when asked for a key) or with `bind` in the console. They are named after a
gamepad's buttons, **by role**: the main hand is the gamepad's right half, the off hand its left half. With *Left
Handed* on, nothing needs rebinding.

| Control | Main hand key | Off hand key | Default (main / off) |
|---|---|---|---|
| Trigger | `RTRIGGER` | `LTRIGGER` | `+attack` / `+offhandattack` |
| Grip | `RSHOULDER` | `LSHOULDER` | `+grabmain` / `+graboff` |
| A / X | `ABUTTON` | `XBUTTON` | `+jump` / `+reloadoff` |
| B / Y | `BBUTTON` | `YBUTTON` | `impulse 10` / `impulse 12` (next / previous weapon) |
| Stick click | `RTHUMB` | `LTHUMB` | `+reloadmain` / `+speed` (run/walk) |
| Stick | turns; up/down are `DPAD_UP` / `DPAD_DOWN` (`+moveup` / `+movedown`: swim up and down) | moves | |
| Menu button | Escape (not rebindable) | | |

- **Index controllers** have no menu button, so their left B opens the menu. **Quest (Touch) controllers under
  SteamVR** reach the game as Index controllers (SteamVR keeps the Touch menu button for its dashboard), so there
  the left Y opens the menu; under Virtual Desktop's VDXR they are Touch controllers and the left menu button opens
  it. **Vive wands** use the trackpads as
  sticks and their clicks as A/X, and the right menu button as B. **Windows Mixed Reality** controllers use the
  trackpad clicks as A/X and the right menu button as B.
- **Useful extra commands to bind:** `+teleport` (with *Teleport* on), `vr_flashlight_toggle`, `+vr_note` (record a
  voice note from any key).
- **Desktop keys** (flat-screen play and testing): `k`/`l` grab with the left/right hand, `n`/`m` reload, `o`/`i`
  flick-reload, and the mouse buttons fire the two hands.
- The default bindings are in `quakevr\vr_bindings.cfg`. They are applied once, on top of any config you had
  before. *Options > Reset All* applies them again.

## The console

Open it with `~` on the desktop keyboard (the desktop window needs focus). Type a variable's name to see its value
and default, and `name value` to set it. Ironwail's tab completion lists the `vr_` variables. Settings are saved
when you quit.

## Important settings by topic

Defaults marked "shipped" are Quake VR's tuned values (from `vr_defaults.cfg`), which differ from the engine's
built-in ones.

### Comfort and movement

| Variable | Default | |
|---|---|---|
| `vr_snap_turn` | 0 | degrees per snap turn; 0 is smooth turning |
| `vr_turn_speed` | 3.25 | smooth turning speed |
| `vr_enable_joystick_turn` | 1 | 0 turns only in real life |
| `vr_movement_mode` | 1 | 1 move towards the head, 0 towards the off hand |
| `vr_deadzone` | 10 | stick deadzone, percent |
| `cl_alwaysrun`, `cl_forwardspeed`, `cl_movespeedkey` | | run by default, walk speed, run multiplier |
| `vr_teleport_enabled`, `vr_teleport_range` | 0, 400 | teleport (bind `+teleport`) |
| `vr_roomscale_move_mult` | 1 | room-scale movement ratio |
| `vr_roomscale_jump` | 1 | jump for real |
| `vr_lean_radius` | 12 | how far (units) your head can lean before the body follows |
| `vr_swim` | 1 | swimming with arm strokes |
| `vr_climb` | 1 (shipped) | climbing: ledges and rungs, hand over hand |

### Body and calibration

| Variable | Default | |
|---|---|---|
| `vr_height_calibration` | 1.646 | your height in metres (*Set Height Now*) |
| `vr_world_scale` | 1.2 (shipped) | size of the world around you |
| `vr_floor_offset` | -21 | floor height |
| `vr_stick_swap` | 0 | 1: the right stick moves and the left turns (*Swap Stick Functions*, on Body and Display; *Handedness* on Body and Display sets it too) |
| `vr_gadget_arm` | 0 | the wrist gadget's arm: 0 left, 1 right |
| `vr_flashlight_side` | 0 | the hip the torch hangs on: 0 left, 1 right |
| `vr_body_mode` | 3 (shipped) | 0 off, 2 torso and arms, 3 full body |
| `vr_body_build` | 1 | 0 lean, 1 athletic, 2 brawny |
| `vr_body_torso_back`, `vr_body_legs_back` | 0.07, 0.3 (shipped) | body placement, metres behind your head |
| `vr_bodycal_upper_arm`, `_forearm`, `_shoulders_back/_up/_out`, `_shoulder_rise`, `_swing` | 0 | Body Calibration's measurements (cm, metres, degrees; 0 lengths: not calibrated) |
| `vr_body_tweak_upper_arm`, `_forearm`, `_shoulders_back/_up/_out`, `_shoulder_rise`, `_swing` | 0 | your tweaks on top of them, the same units (uncalibrated: on the default body) |
| `vr_flashlight` | 1 | the chest flashlight |

### Weapons, throwing and melee

| Variable | Default | |
|---|---|---|
| `vr_gunangle`, `vr_gunyaw` | 39.5, 4 | main hand's weapon pitch and yaw to the controller |
| `vr_offhandpitch`, `vr_offhandyaw` | 40.25, -4 | the same for the off hand |
| `vr_controller_legacy_pose` | 1 | the hands follow the controller pose the original Quake VR was tuned for; 0 uses OpenXR's grip pose as it is |
| `vr_weapon_grip_mode` | 0 | 1 sticky grip |
| `vr_2h_mode` | 2 | two-handed: 0 off, 1 basic, 2 virtual stock |
| `vr_2h_handoff` | 1 | two-handed hand-off |
| `vr_holster_mode` | 0 | 0 immersive holsters, 1 quick slots |
| `vr_reload_mode` | 2 | reloading: 0 none, 1 all holsters, 2 hip holsters |
| `vr_crosshair` | 0 | 0 off, 1 dot, 2 laser, 3 soft laser |
| `vr_weapon_throw_velocity_mult` | 1 | throw speed |
| `vr_throw_gravity` | 9.81 | thrown things' gravity; 0 is Quake's |
| `vr_throw_assist` | 1 | throw aim assist |
| `vr_forcegrab_mode` | 1 | force grab |
| `vr_melee_speed`, `vr_melee_wrist_speed` | 3, 1.1 | how fast (m/s) the striking hand (a weapon's swing 1.25x, a stab 0.75x) and its wrist must move for a blow |
| `vr_parry`, `vr_bash`, `vr_headbutt`, `vr_deflect` | 1 | parry, bash, headbutt, batting projectiles |

### Damage and gore

| Variable | Default | |
|---|---|---|
| `vr_damage_to_enemies`, `vr_damage_to_player`, `vr_damage_self` | 1 | damage multipliers |
| `vr_positional_damage` | 1 | headshots, arm and leg shots |
| `vr_push` | 0.6 (shipped) | all knockback |
| `vr_gore` | 2 | 0 Quake VR's blood only, 1 more, 2 over the top |
| `vr_decals`, `vr_decal_max`, `vr_decal_life` | 1, 1024, 240 (shipped) | blood and scorch marks |

### HUD and menus

| Variable | Default | |
|---|---|---|
| `vr_hud_mode` | 1 | 1 wrist gadget, 0 status bar on a hand |
| `vr_sbar_mode` | 1 | status bar on 1 the off hand, 0 the main hand |
| `vr_player_hue` | 100 (shipped) | the hue of your effects: gadget screen, force grab, teleport arc, crosshair, laser |
| `vr_menu_distance`, `vr_menu_scale` | 100, 0.18 (shipped) | menu placement |
| `vr_menu_spacing`, `vr_menu_height` | 2, 1.6 (shipped) | the headset menus' row spacing and height, over Quake's |
| `vr_mirror` | 1 | desktop window: 0 off, 1 left eye, 2 both eyes |

### Graphics

| Variable | Default | |
|---|---|---|
| `vr_graphics_preset` | (not saved) | 0 Off (Quake), 1 Low, 2 Medium, 3 High, 4 Ultra |
| `vr_relit_maps` | 1 | use the relit maps when present (next map) |
| `vr_bloom` | 0.08 (shipped) | bloom strength |
| `vr_light_contrast` | 2.5 (shipped) | how dark the shade is |
| `vr_normalmaps`, `vr_parallax`, `vr_detail`, `vr_deluxemap` | 1 | bump maps, parallax, detail textures, real light directions |
| `vr_texture_smooth` | 1 | smooth filtering: 1 replacement textures, 2 all textures, 0 as `gl_texturemode` |
| `vr_tonemap`, `vr_exposure`, `vr_grade` | 1 | tone mapping, exposure, colour grade |
| `vr_gamma`, `vr_contrast` | 1 | the headset's own gamma and contrast (the desktop's `gamma`/`contrast` only affect the window) |
| `vid_fsaa` | 4 | anti-aliasing samples (0, 2, 4, 8) |
| `r_wateralpha` | | water transparency (needs water-vised maps) |
| `vr_water_waves` and the other `vr_water_*` | | waves, refraction, caustics, splashes, ripples |

### Headset

| Variable | Default | |
|---|---|---|
| `vr_enabled` | 1 (from `quakevr.cfg`) | VR on or off |
| `vr_xr_runtime` | 0 | 0 system default, 1 Virtual Desktop (VDXR), 2 SteamVR, 3 the manifest in `vr_xr_runtime_json` |
| `vr_render_scale` | 1 | eye resolution multiplier |
| `vr_upscale` | 1 | below render scale 1: 0 bilinear, 1 FSR 1, 2 NIS |
| `vr_upscale_sharpness` | 0.5 | the upscaler's sharpening, 0 to 1 |
| `vr_upscale_radius` | 40 | the upscaler only within this many degrees of the lens centre, bilinear beyond (0: everywhere) |
| `vr_upscale_sharpen_native` | 0 | at render scale 1: FSR's sharpening (RCAS) alone |
| `vr_foveated` | 0 | foveated rendering: 0 off, 1 conservative (full rate within 45 degrees, 2x2 to 60, 4x4 beyond), 2 balanced (35, 50), 3 aggressive (25, 40) |
| `vr_foveated_inner`, `vr_foveated_outer` | 0 | your own angles instead of the preset's (degrees; 0: the preset's) |
| `vr_foveated_debug` | 0 | shows the shading rates (yellow 2x2, red 4x4) and the upscaler's circle (cyan) in the eyes and mirror |
| `vr_visibility_mask` | 1 | hide lens corners |

## Config files, shipped defaults and resetting

Running with `-game quakevr` executes these, in order, from `quakevr\quake.rc`:

1. **`default.cfg`**: Ironwail's default bindings, then `vr_bindings.cfg` (controller bindings), then
   `vr_defaults.cfg` (Quake VR's tuned settings), and a few display defaults (anti-aliasing 4x, gamma, contrast).
   *Options > Reset All* runs this file.
2. **`ironwail.cfg`**: your saved settings. It's written when you quit, and it wins over the defaults.
3. **`quakevr.cfg`**: settings the Quake VR QuakeC needs (gameplay fixes, item scale, opaque lava) and
   `vr_enabled 1`. It runs after your config, so these win.
4. **`autoexec.cfg`**: yours, if you create one. Put your personal overrides here: it runs last.

**Shipped defaults:** each line of `vr_defaults.cfg` is `vr_default <variable> <value>`. This sets the variable and
makes the value its default, so resets and presets go back to it. `vr_savedefaults` rewrites the file from the
current settings that differ from the built-in defaults. Personal settings are left out: your height, the runtime,
the microphone and the per-weapon offsets.

**When defaults change in an update:** a setting still at its old default moves to the new one, once. A setting you
changed yourself is left alone.

**Resetting:**

- Everything: *Options > Reset All*, or quit and delete `quakevr\ironwail.cfg`.
- One variable: `reset <name>` (back to its shipped default).
- One weapon's offsets: *Reset This Weapon* on the Weapon Offsets page.
- The swimming settings: *Reset Swimming to Defaults* on the Swimming page.

## Diagnostic commands

| Command | What it does |
|---|---|
| `vr_status` | Runtime, tracking, eye resolution, hand angles, hotspots, grab and two-handed state |
| `vr_restart` | Restarts the VR session (after starting the runtime or putting the headset on) |
| `vr_dumpview` | The drawn hands, weapons and finger curls |
| `vr_profile 1` / `2`, `vr_profile_dump` | Per-frame CPU and GPU timings to `quakevr\profile\` (2: also on the wrist) |
| `vr_memstats` | GPU and game memory, live OpenGL objects, average frame time. `vr_memstats_log` writes a row a minute. |
| `vr_shadow_stats 1` | Shadow lights, faces and cost each second |
| `vr_shadow_layered 0` / `1`, `vr_shadow_layered_check 20` | Shadow casters drawn a face at a time / once per light (default); both ways compared in one frame |
| `vr_light_test`, `vr_particle_test <0..11>`, `vr_gore_test` | Spawn a light, a particle effect or gore in front of you |
| `vr_body_debug 2` / `3` | A copy of your body's pose in front of you (facing you, or from the side) |
| `vr_show_hip_holsters 1` (and `_upper_`, `_shoulder_`, `vr_show_virtual_stock`) | Show the holsters and the virtual stock |
| `vr_note_devices` | List microphones for voice notes |
| `vr_debug_throw 1` | Print each throw's measurement |
| `developer 1` | More messages: force grab pulls and catches, throws, compatibility mode |

For the rest, the developer notes in [vr-port/](vr-port/) describe each system and its variables:
[TESTING.md](vr-port/TESTING.md) (the playtest guide), [LIGHTING.md](vr-port/LIGHTING.md),
[GRAPHICS.md](vr-port/GRAPHICS.md), [THROWING.md](vr-port/THROWING.md), [IK.md](vr-port/IK.md), and the
`ROUND*.md` notes.
