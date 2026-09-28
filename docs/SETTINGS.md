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

Open the menu with the controller's menu button (on Index controllers, the left B button) and go to **Options > VR
Settings**. `menu_vr` in the console opens it directly, and `menu_vr <n>` opens page *n* (0 is VR Settings, 1 is
Advanced VR Options, then the pages in the order listed below; `menu_vr list` prints the numbers).

**Using the menus:** point with the laser from your hand and pull the trigger to click. Drag sliders with the
trigger held. With the left stick (the off hand's), up and down move between rows, left and right change a value; A
selects and B goes back. The right stick (the main hand's) only scrolls or moves between rows: it never changes a
setting, so navigating can't change one by accident (with *Left Handed* on, the sticks swap). *Back to Game* is at the top left, or hold the menu button. Each setting shows a line
of help at the bottom when you select it. In the headset the menus are taller than on the desktop, so more rows show
at once (Menu page > *Menu Height*, `vr_menu_height`: 1.35 times Quake's height, about 35 degrees up and down with
the shipped menu distance and scale).

**Live preview:** changes show in the game as you make them. In single player the game keeps running under the
settings pages, and it pauses while a monster is after you (Menu page > *Live Preview*).

The main page has these sections:

| Section | Settings |
|---|---|
| **Comfort** | Turning (smooth or snap 30/45/90), Turn Speed, Move Towards (head or off hand), Default Speed (run or walk), Stick Deadzone, Teleport and Teleport Range, Room Scale (real movement to game movement) |
| **Body** | Left Handed, Height and *Set Height Now*, World Scale, Floor Offset, Chest Flashlight |
| **Weapons** | Gun Angle, Off Hand Angle, Weapon Grip (hold or sticky), Two-Handed (off, basic, virtual stock), *Weapon Offsets (Held Weapon)*, Two-Handed Hand-Off, Throw Speed, Throw Gravity (real or Quake), Force Grab, Haptics, Crosshair and its size |
| **Display** | HUD (wrist gadget or status bar), Status Bar hand, HUD Scale, Menu Distance and Scale, Desktop Mirror (off, left eye, both eyes), Body (off, torso and arms, full body), Build, Torso, Legs and Shoulders offsets, Holster Models |
| **Headset** | VR on or off, Restart VR, OpenXR Runtime, Render Scale, Upscaling (bilinear, FSR, NIS), Sharpness, Foveated Rendering (off, conservative, balanced, aggressive), Hide Lens Corners |
| **More** | Advanced VR Options |

Notes:

- **Height:** *Set Height Now* measures you while you stand straight. *World Scale* (1.25 by default) makes the
  world bigger or smaller around you. *Floor Offset* moves the floor up or down.
- **Gun Angle / Off Hand Angle:** the pitch between your controller and the gun. If shots go above or below where
  the gun seems to point, change these first. The *Hand/Gun Calibration* page also has the yaw.
- **Render Scale:** from 0.5 to 1.5 times the runtime's resolution. The image is resampled to the headset, so
  dragging it doesn't restart anything.
- **Upscaling:** below Render Scale 1, how the eyes are enlarged to the headset's size. *FSR* (AMD FidelityFX Super
  Resolution 1) and *NIS* (NVIDIA Image Scaling) keep edges and text sharper than *Bilinear*; they run within 40
  degrees of the lens centre (`vr_upscale_radius`), bilinear beyond, where the lenses blur anyway. The menus, HUD and
  wrist log are drawn afterwards at the headset's full resolution. *Sharpness*: the upscaler's sharpening (too much
  makes edges shimmer).
- **Foveated Rendering:** shades the scene coarser towards the edges of the lenses (once per 2x2 pixels, then 4x4),
  which the lenses blur anyway: 18% (conservative) to 64% (aggressive) less GPU time for the world in the desktop
  test headset. NVIDIA GPUs only (variable-rate shading, `GL_NV_shading_rate_image`); elsewhere it does nothing.

## Advanced VR Options

*Advanced VR Options* at the bottom of the main page opens these pages, grouped by topic. Most settings are sliders
with a line of help. Long topics are split into pages of about a screenful each (the Graphics page also links to its
sub-pages).

**Game**

| Page | What's on it |
|---|---|
| **Play** | Go to the VR hub, the tutorial or the firing range; add and kick bots |
| **Gameplay** | Damage (to enemies, to you, self damage, melee), positional damage (headshot, arm and leg multipliers, the headshot sound), knockback per source, knights' swords, explosion rumble, low-health heartbeat, voice notes |
| **Parry, Bash and Headbutt** | Parry angle and reach, unarmed parry, bash (speed, damage, push), their sounds; batting projectiles back (reach, swing speed, timing); headbutt (speed, damage) |
| **Melee** | Swing speed, blow distance, punch multiplier, damage and range multipliers, bloodlust, parry settings (from the original Quake VR) |
| **Gore** | Gore level, blood sprays, splat size, pools, dripping, how long gibs stick; your wounds' drips and floor marks; decals (count and lifetime) and gib blood |
| **Throwing and Physics** | Throw speed (one and two hands), gravity, how the throw is measured, analogue release, aim assist; bounciness, friction, spin, hitbox |
| **Carrying and Gibs** | Carrying ammo and health boxes (how you take a box, pushing, box throw speed and damage); grabbing gibs and heads, their damage, destroying gibs, gibbing corpses |
| **Force Grab** | On/off, distance, aim cone, flick speed and turn, flight time and speed, arc, catch window, pointing particles and haptics, the outline and effects, box size |

**Body and Movement**

| Page | What's on it |
|---|---|
| **Body** | Body mode and build, walking legs (step rate, turning before stepping), armour and wounds, powerups, anchors; the body's placement (torso, legs, eyes), crouch tilt |
| **Arms and Pauldrons** | Body Calibration and what it measured; tweaks on top of it (upper arm, forearm, the shoulders' place, rise and swing; 0: as measured, uncalibrated: the default body), Arm Length (uncalibrated only), stretch, shoulder reach; forearm twist, wrist limits, elbows; pauldrons (style, size, how they follow the arm, placement) |
| **Flashlight** | The chest flashlight: brightness, range, visible beam, shadows, placement on the chest and in the hand |
| **Player Calibration** | World scale, floor offset |
| **Locomotion** | Movement mode, deadzone, stick turning, turn and turn speed, teleport, lean and lean recentre, roomscale jump and threshold, room-scale multiplier, walk speed, run/walk and run multiplier, swimming, **ledge grab (experimental)** |
| **Swimming** | The stick's speed in shallow water, wading and swimming; stroke strength, speed curve, palm, recovery; telling a stroke from the return of the arms (intent threshold, stroke memory, return damping); *Reset Swimming to Defaults* |

**Weapons**

| Page | What's on it |
|---|---|
| **Immersion** | Body picks up items, haptics; holster mode (immersive or quick slots), reloading mode (none, all holsters, hip holsters), holster haptics, weapon cycling, throw mode (immersive, vanish on hit, discard), throw damage and speed, dropped weapon particles; shell casings; enemy and ammo-box weapon drops |
| **Hand/Gun Calibration** | Gun angle and yaw, gun model pitch, scale and height, gun height offset, off-hand pitch and yaw, finger grip bias, auto-close thumb |
| **Weapon Offsets** | The held weapon's settings (see [Weapon offsets](#weapon-offsets)) |
| **Aiming** | Two-handed mode, threshold, virtual stock factor, hand-off; weapon weight (position and turn, with two-handed help) |
| **Hotspots** | The virtual stock's shoulder, and the shoulder, hip and upper (chest) holsters: their positions and reach, with *Show...* switches that draw them. The holster models' size and position. |

**HUD and Menus**

| Page | What's on it |
|---|---|
| **Wrist Gadget** | HUD mode, which arm, size and placement |
| **Screens** | The wrist gadget's screen (level and stats, light, CRT look, glow, messages on the wrist); the weapons' ammo screens (weapon text, margin, CRT look); map boards as CRTs and their hue |
| **Colours** | The Player Effects Hue and saturation (that the force grab, teleport arc, crosshair and menu laser follow); the gadget's screen and casing; the iron sights, force grab, teleport arc, crosshair and menu laser hues; the iron sights' and the force grab's saturation |
| **Status Bar** | HUD mode, status bar hand, scale, offsets and angles |
| **Crosshair** | Crosshair type, depth, size, alpha, hue, height offset |
| **Menu** | Menu scale and distance, VR menu style, laser hue, row spacing, menu height, live preview, reopen where left |

**Graphics**

| Page | What's on it |
|---|---|
| **Graphics** | The [preset](#graphics-presets); anti-aliasing, smooth textures, fence coverage, relit maps, headset gamma and contrast; links to the pages below; the performance profile and memory log |
| **Lights** | Light contrast, coloured lights, dynamic lights (falloff, uncapped, on models, angle); the lights of muzzle flashes, explosions, projectiles, lava nails, lightning beams, torches (count, brightness, shadows) and ammo screens |
| **Shadows** | Shadow-casting dynamic lights and map lights (count, detail, strength), muzzle-flash shadows, your shadow, softness, distance, atlas, bias, statistics; blob shadows |
| **Surfaces** | Light sheen and its anti-aliasing; bump maps (depth, in map light, real light directions, on models); parallax (depth, distance, items, models); detail textures |
| **Liquids** | Waves, reflection, refraction, glints, lava glow, real waves, shoreline foam, heat haze; under water (caustics, the view, wobble); splashes, rings, ripples and water sounds |
| **Post-processing** | Bloom (threshold, size, white and coloured lights, adapt); tone mapping, exposure, colour grade, dither |
| **Models and Effects** | Model lighting, models lit as the world, directional ambient, rim light, hands' least light, weapon reflections; screen, text and sight glow, sight hue; soft particles, decals, gib blood |
| **Particles** | Particles on/off, Quake VR particles, particle multiplier |
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
where the other hand holds it two-handed, the ammo screen, and its weight. **VR Settings > Weapon Offsets (Held
Weapon)** edits the weapon your main hand is holding. Open it while holding the weapon in a game.

- *Edit the Other Hand's Weapon* switches to the off hand. The page shows the weapon held when it was opened, so
  reopen it after changing weapons. With an empty hand, it edits the hand model itself.
- **Weapon in the Hand:** offset X (forward), Y (left), Z (up), pitch, yaw, roll, scale. These move the model, not
  where it aims. The drawn hand stays on the weapon's grip, so the offsets move the hand with the weapon.
- **Weapon Only (Hand Stays):** Weapon Only X, Y, Z move just the weapon while the drawn hand stays where it is. Each
  one changes the offset and the hand's place on the weapon together (by 7/6 of the step, the other way), as if you
  had moved both sliders yourself, so the muzzle, the two-handed grip and the ammo screen move with the weapon.
  They show how far you have moved it since the page opened, and start at 0 each time. Not shown for an empty hand.
- **Hand on the Weapon:** where the drawn hand sits on the grip, or hide it.
- **Muzzle:** where shots and the flash start.
- **Two-Handed:** where the other hand grips the weapon, and where that hand is drawn.
- **Ammo Screen:** position, angles and scale.
- **This Weapon:** *Weight* (how much it lags your hand), **Print Changes to Console** (prints this weapon's
  settings that differ from the defaults, as `vr_wofs_...` lines to copy into a config or send to the author), and
  **Reset This Weapon**.

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
  before. *Reset to defaults* applies them again.

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
| `vr_deadzone` | 25 | stick deadzone, percent |
| `cl_alwaysrun`, `cl_forwardspeed`, `cl_movespeedkey` | | run by default, walk speed, run multiplier |
| `vr_teleport_enabled`, `vr_teleport_range` | 0, 400 | teleport (bind `+teleport`) |
| `vr_roomscale_move_mult` | 1 | room-scale movement ratio |
| `vr_roomscale_jump` | 1 | jump for real |
| `vr_lean_radius` | 12 | how far (units) your head can lean before the body follows |
| `vr_swim` | 1 | swimming with arm strokes |
| `vr_climb` | 0 | ledge grab (experimental) |

### Body and calibration

| Variable | Default | |
|---|---|---|
| `vr_height_calibration` | 1.646 | your height in metres (*Set Height Now*) |
| `vr_world_scale` | 1.25 | size of the world around you |
| `vr_floor_offset` | -21 | floor height |
| `vr_lefthanded` | 0 | swap the main and off hands |
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
| `vr_throw_assist` | 0 | throw aim assist |
| `vr_forcegrab_mode` | 1 | force grab |
| `vr_melee_speed`, `vr_melee_distance` | 2.75 (shipped), 0.2 | how fast (m/s) and how far (m) the wrist must move for a blow |
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
| `vr_player_hue` | 110 (shipped) | the hue of your effects: gadget screen, force grab, teleport arc, crosshair, laser |
| `vr_menu_distance`, `vr_menu_scale` | 69, 0.2 (shipped) | menu placement |
| `vr_menu_spacing`, `vr_menu_height` | 1.8 (shipped), 1.35 | the headset menus' row spacing and height, over Quake's |
| `vr_mirror` | 1 | desktop window: 0 off, 1 left eye, 2 both eyes |

### Graphics

| Variable | Default | |
|---|---|---|
| `vr_graphics_preset` | (not saved) | 0 Off (Quake), 1 Low, 2 Medium, 3 High, 4 Ultra |
| `vr_relit_maps` | 1 | use the relit maps when present (next map) |
| `vr_bloom` | 0.1 (shipped) | bloom strength |
| `vr_light_contrast` | 2.3 (shipped) | how dark the shade is |
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
   *Options > Reset to defaults* runs this file.
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

- Everything: *Options > Reset to defaults*, or quit and delete `quakevr\ironwail.cfg`.
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
