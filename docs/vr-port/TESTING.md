# Testing in the headset

The first headset build. Everything below was checked on the desktop with the mock backend. OpenXR itself has
only been checked up to "no headset connected", so expect rough edges: tell me what you see and I will fix it.

## Build and install

The quickest way: `Windows\package-quakevr.ps1 -Build -Fteqcc <path to fteqcc64.exe>` builds everything into
`dist\QuakeVR` (and `dist\QuakeVR.zip`); copy its contents into your Quake folder and run `QuakeVR.bat`.
By hand:

1. Build `Windows/VisualStudio/ironwail.sln`, **Release | x64**. The output is
   `Windows/VisualStudio/Build-ironwail/bin/x64/Release/ironwail.exe`, with `openxr_loader.dll` copied next to it.
2. Build the progs: `QC/build.bat` (set `FTEQCC` to `fteqcc64.exe`). It writes `quakevr/progs.dat`.
3. Put the repository's `quakevr` folder in your Quake directory, next to `id1` (a directory junction works:
   `mklink /J <Quake>\quakevr C:\OHWorkspace\quakevr-iw\quakevr`). `hipnotic` and `rogue` are picked up
   automatically if they are installed.
4. Start the OpenXR runtime you want to use (Virtual Desktop, SteamVR or Oculus) and make it the active runtime.
5. Run:

   ```
   ironwail.exe -basedir <Quake> -game quakevr
   ```

`quakevr/quakevr.cfg` turns VR on (`vr_enabled 1`). The console reports `VR: started openxr backend` or the reason
it could not start; `vr_status` prints the tracking state and `vr_restart` restarts the session (after putting the
headset on, or switching runtimes). `vr_enabled 0` is flat-screen play.

## Controls

Controller buttons are Quake keys (issue #12), so everything can be rebound from the console or Ironwail's
bindings menu (press the controller button when asked for a key), aliases included. They reuse the gamepad key
names **by role**: the main hand is the gamepad's right half, the off hand its left half, so `vr_lefthanded 1`
needs no rebinding.

| Control | Main hand key | Off hand key | Default binding (main / off) |
|---|---|---|---|
| Trigger | `RTRIGGER` | `LTRIGGER` | `+attack` / `+offhandattack` |
| Grip | `RSHOULDER` | `LSHOULDER` | `+grabmain` / `+graboff` |
| A / X (primary) | `ABUTTON` | `XBUTTON` | `+jump` / `+reloadoff` |
| B / Y (secondary) | `BBUTTON` | `YBUTTON` | `impulse 10` / `impulse 12` (next weapon) |
| Stick click | `RTHUMB` | `LTHUMB` | `+reloadmain` / `+speed` |
| Stick | turn; up/down are `DPAD_UP`/`DPAD_DOWN` (`+moveup`/`+movedown`: swim) | move | |
| Menu button | Escape (not rebindable) | | |

Controllers without some of these: Index has no menu button, so its left B opens the menu. Vive wands use the
trackpads as sticks and their clicks as A/X, and the right menu button as B. WMR uses the trackpad clicks as A/X
and the right menu button as B. In menus both sticks navigate, A selects and B goes back.

The defaults live in `quakevr/vr_bindings.cfg`. They are applied once (`vr_bindings_version`) on top of whatever
config was saved before, including one inherited from `id1`; "Reset to defaults" applies them again.

Useful settings:

| Cvar | Default | |
|---|---|---|
| `vr_snap_turn` | 0 | degrees per snap; 0 = smooth turning at `vr_turn_speed` |
| `vr_controller_legacy_pose` | 1 | the hands follow the controller pose the old engine used (SteamVR's raw pose, rebuilt from OpenXR's grip pose for Touch/Quest and Index controllers), so the old tuned offsets line up; 0 uses the grip pose as is |
| `vr_gunangle`, `vr_offhandpitch` | 39.5, 40.25 | weapon pitch relative to the controller (the shipped values, tuned for that raw pose): Options > VR Settings > Gun Angle |
| `vr_world_scale` | 1.25 | |
| `vr_height_calibration`, `vr_floor_offset` | 1.646, -21 | |
| `vr_mirror` | 1 | desktop window: 0 off, 1 left eye, 2 both eyes |
| `vr_deadzone` | 25 | stick deadzone, percent |
| `vr_weapon_grip_mode` | 0 | 1 = weapons stay in the hand without holding the grip (issue #31) |

## Throwing: what changed

Reworked after the research in `docs/vr-port/THROWING.md`: how the throw is measured, how strong it comes out,
where the weapon starts, and how it flies. Most values are starting points: please tell me how they feel.

**Measuring the throw** (as Half-Life: Alyx):
- **Release point:** the release velocity is the controller's own velocity (from the runtime) where it was
  fastest, in a window around the moment you let go (`vr_throw_window` 0.12 s before, `vr_throw_lookahead`
  10 ms after), averaged over `vr_throw_peak_span` (17 ms) around that peak.
- **Frozen at release:** it is taken once, when you let go, on the headset's clock, so the network rate can no
  longer slide the window past the peak.
- **Wrist flicks:** a clear flick (spin above `vr_throw_ang_threshold`, 6 rad/s) adds 70% of the spin's
  velocity at the weapon's centre.

**Letting go** (`vr_throw_release 1`): the runtime's grip button lets go late. During a throw (hand faster than
`vr_throw_release_speed`, 1.5 m/s) the weapon now leaves the hand as soon as the grip eases 30% below its
firmest (`vr_throw_release_drop`), and it always does below 35% (`vr_throw_release_floor`). Grabbing needs 70%
(`vr_throw_grab_press`). **Tell me if weapons ever drop while you swing them without meaning to throw.**

**Strength:**
- **Gain:** slow movements (drops, passing a weapon between hands) are 1:1. Real throws get up to 1.5×
  (`vr_throw_gain_max`), rising smoothly from 1.5 to 6 m/s.
- **Weight:** heavy weapons are only slightly slower now (`vr_throw_weight_influence` 0.25; before, they were
  thrown at 40–60% of the hand's speed).
- **Two-handed:** two-handed throws are no longer 40% stronger (`vr_2h_throw_velocity_mult` 1).
- **Tuning:** if throws are still short, raise `vr_throw_gain_max` or lower `vr_throw_gravity`.

**Start position:** the weapon starts where it would be had it left the hand at the release point, not where the
hand followed through to.

**Flight** (the engine, `vr_rigid.cpp`):
- **Spin:** the hand's real spin, capped at `vr_throw_spin_max` (20 rad/s). It used to be applied as rates on
  each angle, which tumbled wildly.
- **Gravity:** the same true-scale gravity for the whole flight. It used to jump to Quake's 2.5 g after the first
  touch.
- **Bounces:** with `vr_throw_restitution` (0.25) and friction (`vr_throw_friction` 0.5).
- **Resting:** the weapon turns onto its nearest flat side (`vr_throw_settle_rate`) and stays still. No more
  wiggling.
- **Hit box:** a 6-unit hit box (`vr_throw_hitbox`) against monsters, so throws that look like hits are hits.

**Aim assist** (`vr_throw_assist 1`, off by default): bends a throw by up to 80% onto the best monster or
breakable within 12° of it, on the arc that reaches it. With `developer 1` it marks the target it picked.

**Debugging:** `vr_debug_throw 1` prints every throw's estimate; `2` also prints how long after the peak the release
came. `developer 1` prints the spawned velocity, gravity, spin and age.

## GitHub issues

| Issue | State in this port |
|---|---|
| #31 weapons should stick to hands | `vr_weapon_grip_mode 1`: grip, let go and the weapon stays; grip again and open the hand to throw it (or holster it) |
| #53 stuck on steps | not reproduced in a quick test (walking off and back up the steps behind the start.bsp spawn, flat and mock VR): please try the E1M1 spot from the issue |
| #37 force-grabbed BSP items become solid | the QC never makes items solid, and the new engine never blocks on `SOLID_NOT_BUT_TOUCHABLE`: please confirm in the headset |
| #34 weapon knockback too strong | the rendered hand follows the weapon's firing animation: please check how it feels |
| #67 / #40 hands shaking (frame cap, after a break) | OpenVR pose-timing problems; OpenXR predicts poses for the displayed frame: please confirm |
| #64, #70, #19, #49, #52 | old engine/renderer (SteamVR keyboard, lighting, animated textures, mission-pack launch, black screen): gone with Ironwail |
| #12 missing bindings | done: the controller buttons are Quake keys (see Controls) |
| #20, #14 status bar on the hands | done: on the off hand by default (see HUD below) |

## What to try

- **New in this round** (details in `docs/vr-port/ROUND10.md`):
  - **The DarkPlaces look:** darker shade, flat model lighting, strong coloured flashes and explosions, a sheen
    and bumps under dynamic lights, smooth QRP textures, bloom stronger on coloured lights and weaker on white
    (and on brightly lit maps). Every part has a switch on the Graphics page.
  - **See-through water** in the relit maps.
  - **Leaning:** walk or lean up to a wall or railing: your head gets close and over it before the body follows
    (Locomotion: Lean, Lean Recentre).
  - **Training dummy** in vrfiringrange: every hit's damage, kind and body part in the console and as a floating
    number.
  - **Held boxes** keep up when you move; **knockback** has a base and a setting per source; **thrown boxes and
    gibs** hurt less (and only when fast); gibs by hand only; **heads** can be picked up; gibs bleed and splat;
    wounded arms drip; a meatier **headshot** sound.

- **Previous round** (`docs/vr-port/ROUND9.md`):
  - **Boxes:** a held box turns with your hand about where you hold it; it is grabbed only when your hand is
    on it; bounces turn the right way; it comes to rest flat on the floor.
  - **Swords:** the grip is centred on the blade, thicker and square in section.
  - **Bloom** is subtler by default, with fine slider steps; **headbutts** are easier to land.
  - **Shotgun held by the middle:** not reproduced; please describe when it happens.

- **Previous round** (`docs/vr-port/ROUND8.md`):
  - **Unarmed parry:** cross your arms in an X in front of you as a blow lands.
  - **Bash:** in a guard (a weapon held across in front, or both hands together, crossed or not), drive forward
    hard: little damage, the monster is thrown back and staggers. Also a two-handed shove with empty hands.
  - **Force grab:** what you point at glows softly; a faint beam when aiming, a crackling energy tendril when
    locked on and while it flies to you, with a sparkle trail. (Force Grab: Outline, Effects.)
  - **Decals:** blood pools and spatter, gib blood trails, scorch marks, bullet chips (Graphics: Decals).
  - **Gibs and heads:** pick them up, throw them (they hurt), force-grab them.
  - **Pickup sparkles** are faint and slow.

- **Previous round** (`docs/vr-port/ROUND7.md`):
  - **The look:** darker rooms lit by their lamps; your shots light the room up (coloured by the weapon);
    lamps, buttons and panels glow (bloom), and glowing textures light the walls round them (your relit maps
    were re-made). Graphics: Light Contrast, Bloom, Muzzle Flash Light, Explosion Light, Coloured Lights;
    "Off (Quake)" restores Quake's look.
  - **Gameplay page** (Advanced VR Options > Gameplay): damage multipliers (to enemies, to you, self),
    headshot/arm/leg multipliers, the headshot sound (now a clear crack), push-back, feel options, headbutt,
    knights' swords.
  - **Push-back:** your melee blows and headbutts push monsters (bodies too), their blows push you, parries
    push both apart; heavy shots shove monsters and killing blows throw the bodies.
  - **Headbutt:** lunge your head at a monster (towards where you look).
  - **Bat back projectiles:** swing a weapon or fist through a spike, laser, spit or grenade. Please try.
  - **Haptics:** hits felt on the side they come from, explosions rumble, a heartbeat at low health.
  - **Knights' swords:** a grip and pommel, the knight's own look, held like the axe.
  - **Boxes:** grabbed only when your hand touches them; put in your pack by letting go at a holster (hip or
    shoulder); the trigger is an option (Throwing and Physics > Take a Box). Their shadows are their size.
  - **Swimming:** the stick is 20% under water; strokes push more where the stick points.
  - **Blob shadows** are named so, and Auto (off where real shadows fall) by default.

- **Previous round** (`docs/vr-port/ROUND6.md`):
  - **Carrying boxes:** grip an ammo or health box to hold it, pull the trigger to take it, let go to throw it
    (thrown hard it hurts). A hand or gun touching a box without gripping nudges it. While a hand holds a box it
    cannot take a weapon; punching with a box in hand hits harder. Advanced > Throwing and Physics.
  - **Knights drop their swords** (knights and hell knights, gibbed or not): pick one up for a melee weapon with
    more reach and damage than the axe; dead knights no longer hold theirs. Advanced > Melee.
  - **Parrying:** hold a weapon sideways in front of you to block a monster's melee blow: less damage, a clang,
    sparks, your arm is knocked; one-handed, it may be knocked out of your hand. Advanced > Melee.
  - **Swimming:** wading is a little slower; under water or off the bottom the stick barely moves you, and
    swimming strokes (palm first) do. Advanced > Locomotion.
  - **Pauldrons** on your shoulders and the **ranger's clothes** on the body (olive vest, belt, camouflage
    trousers with thigh plates, tall boots). Pauldron style, size, position and how much they follow the arm:
    Advanced > Body.
  - **Headshots** measured properly whatever way the monster faces, with a quiet tick
    (Advanced > Gameplay, Headshot Sound). The tick never played before (its sound was not loaded).
  - **Ammo screen** behind each weapon's ammo counter (Advanced > HUD).
  - **Weapons, keys, armour and powerups** float at torso height.
  - **Swing sounds** are back, except when the hand moves down (reaching for a holster).
  - **Shoulder position** (Body: Shoulders Back/Up/Width); the **body is hidden while dead**; the **green armour**
    on the body matches the pickup's colour; **ammo and health boxes** have shadows; floating items **splash**
    only when they hit the water fast; a force-grabbed box missed near a wall no longer falls out of the level.

- **Previous round:**
  - **Real-time shadows and dynamic lights** (`docs/vr-port/LIGHTING.md`; Advanced > Graphical Settings > Lights
    and Shadows, with a Preset from Off to Ultra; defaults are Medium):
    - explosions and rockets cast shadows (and stop lighting through walls);
    - the map lights near you cast the shadows of monsters and of you (body and hands) onto the floor and walls;
    - dynamic lights light models per pixel, by angle, shadowed.
    - `vr_light_test` puts a light in front of you; `vr_shadow_stats 1` prints the cost each second. Please tell me
      the frame timing on your headset at Medium and Ultra, and any shadow speckles or light leaks you see.
  - **Graphics** (`docs/vr-port/GRAPHICS.md`, "Done"; Advanced > Graphical Settings):
    - **Re-lit maps:** softer shadows, ambient occlusion in corners, some bounced light, coloured light. Made on
      your machine by `Misc/quakevr/relight_maps.py` (already run for you); Relit Maps off compares.
    - **Model lighting:** monsters, items, weapons, hands and body are shaded from the map's lights.
    - **Shadows under monsters and items**, away from their light.
    - **The muzzle flash lights up your gun**, not a point in front of your chest.
    - **Anti-aliasing** setting (4x for new configs; yours is off, `vid_fsaa 4` to try).
  - **Crouching** keeps the hips under you and tilts the back forward (Advanced > Body: Crouch Tilt), instead of
    pulling the torso back.
  - **Bloody hands:** the hands and fingers get bloodier with the arms.
  - **Quad damage arcs** reshape every frame, with more of them and now and then a longer one.
  - **Grappling hook rope and lightning beam** start at the gun as drawn, every frame (they trailed behind it at
    the server's rate); the rope no longer twists randomly.
  - **Reloading:** a gun's swing makes no whoosh unless it hits something (the whoosh was what you heard when
    reaching down to reload), and a downward swing now ignores walls and floors from a shallower angle.
  - **Other mods in VR:** `-game quakevr -game <mod>` runs another mod's progs in VR (compatibility mode): you
    aim with your hand, its weapons fire from your gun, you move by your head, walk the room and teleport. No
    off-hand weapons, holsters or hand pickups there yet. See `docs/vr-port/MODS.md`; please try a mod you like.
  - **Particles:** your old textured particle system is back (smoke, sparks, blood, explosions, force grab and
    pickup sparkles), for Quake's own impacts and explosions too. Advanced > Particles: Quake VR Particles,
    Particle Multiplier. `vr_particle_test <0..11>` spawns one in front of you.
  - **Body state:** your torso shows the armour you wear (green, yellow, red plates) and your arms get bloodier as
    you are hurt; quad damage sparks around your hands and forearms, the pentagram makes you glow red, the ring
    fades you. Advanced > Body: Show Armour and Wounds, Show Powerups.
  - **Wrist gadget options:** Advanced > Wrist Gadget: arm, size, position and rotation, casing tint, screen colour.
  - **Default Speed: Run/Walk** in VR Settings (the speed button switches to the other). The stick now runs at the
    old speed (it walked at half of it) and moves as fast in every direction.
  - **Force grab:** one object at a time per hand (let go of the trigger before pulling another).
  - **Backpacks** no longer spin (their model's rotate flag).
  - **Reloading** (reaching down to a hip holster) no longer hits the floor or a wall as a melee swing.
  - **Old port features back:** VR actions in Options > Key Setup; the status bar shows the ammo count; grenade
    trails; roomscale jump (`vr_roomscale_jump`); a click in the controller for menu presses; Advanced > Play (hub,
    tutorial, firing range, bots); desktop keys for the VR actions after "Reset to defaults".
  - **Settings cleanup:** 15 settings that did nothing, the old floating torso (Body "Torso" is now the body with
    arms) and the old throw algorithms are gone. Fixed: the bloodlust toggle was inverted, drop chances were
    applied twice, the hub's Torso/HUD/Shadows buttons.

- **Previous round:**
  - **Force grab** (rewritten, like Half-Life: Alyx): point an empty, open hand at a weapon, backpack, ammo or
    health box (or, in single player, a weapon lying in the level). It sparkles. Pull the trigger to lock on,
    then flick your hand back or up. It flies to your hand in an arc and arrives in about half a second. Close
    your hand (grip) as it arrives to catch it; too early or too late and it drops at your feet. It flies through
    walls, so it cannot get stuck. Tuning: VR Settings > Advanced VR Options > Force Grab. `developer 1` prints
    each pull, catch and miss.
  - **Melee:** any swing faster than `vr_melee_speed` (3 m/s) hits once, whatever its direction; damage grows
    with speed, and punches (knuckles first) do 25% more (`vr_melee_punch_mult`). Tell me if weak swings still
    hit, or real punches don't.
  - **Advanced VR Options** (bottom of VR Settings): your old Quake VR settings pages (everything that still
    exists, with the old ranges and help), plus Body, Throwing and Physics, and Force Grab pages.
    `menu_vr <n>` opens a page directly.
  - **Posture:** VR Settings > Torso Offset and Legs Offset move the torso and the feet back (or forward)
    separately.
  - **Wrist gadget:** now over the back of the forearm, and it reads like a watch: raise your forearm across your
    chest, and the text runs towards your fingers.
  - **Ammo and health boxes** are small (`vr_forcegrabbable_box_scale` 0.25), their touch box is the box you see,
    and they can be force-grabbed.
  - **Backpacks** now come to rest instead of spinning on the ground. `impulse 243` (single player) drops a
    backpack of your ammo in front of you to try it.
  - **Hands:** the wrist end of the hand model is tapered, so it stays inside the bracer.

- **Previous round:**
  - **Wrist gadget:** the HUD is now a device strapped over the back of your off-hand forearm. Raise your forearm
    across your chest, like reading a watch. VR Settings > HUD switches back to the status bar.
  - **Body:** VR Settings > Build picks the body: Lean, Athletic or Brawny. Full body (VR Settings > Body) walks
    as you move (`vr_body_walk`).
  - **Pickups:** weapons, armour, powerups and keys are smaller and lie on the floor (`vr_pickup_scale` 0.6 in
    `quakevr.cfg`), so you crouch to take them.
  - **Physics:** thrown weapons and backpacks are real rigid bodies. `vr_debug_throw 3` prints their state.
  - **Near clipping:** things close to your face are no longer cut away (`vr_nearclip`).

- **Body** (new): the old floating torso is replaced by a body whose arms reach your hands and which crouches and
  leans with your head (Options > VR Settings > Body: Off / Torso / Torso and arms / Full body). To see the whole
  pose, `vr_body_debug 2` (facing you) or `3` (from the side) shows a copy in front of you. Things to tell me:
  - where the elbows go when you aim, reload or reach behind you;
  - whether looking down at your chest feels right (`vr_body_torso_back`, metres the torso sits behind your neck);
  - whether crouching looks right (`vr_body_crouch_tilt`, degrees the back tilts forward in a full crouch).

  The tuning cvars are listed in `docs/vr-port/IK.md`. Holsters and the virtual stock now follow the body when you
  crouch or lean; `vr_body_anchors 0` restores the old placement for comparison.

- **Walking around the room** moves you in the game (with collision), as in the old engine
  (`vr_roomscale_move_mult`).
- **Holsters:** bring a hand to a hip, the chest or a shoulder. The holster lights up while hovered; let go of a
  weapon there to holster it, grip there to draw. Bringing both hands together passes a weapon between them.
- **Two-handed aiming:** with a gun in one hand, grip its foregrip with the other (empty) hand: the hand snaps
  onto the gun. Weapons trail the hand a little depending on their weight (`vr_wpn_pos_weight`,
  `vr_wpn_dir_weight`); hands and barrels stop at walls. With a hand
  near the shoulder, the virtual stock steadies the aim (`vr_2h_mode`, `vr_virtual_stock_thresh`).
- **Flick reload:** with the super shotgun, flick the wrist to snap it open (`vr_spinreload_x_angular_threshold`).
- **Teleport:** `vr_teleport_enabled 1` and bind a button, e.g. `bind LTHUMB +teleport`. Aim with the off hand,
  release on a blue spot.
- **Fingers** curl with the trigger (index), the grip (middle to pinky) and the thumb resting on a button or stick.
- **Movement:** `vr_movement_mode 0` moves where the off hand points instead of the head; in both modes, pointing
  the off hand up or down while pushing forward swims up or down.

- **HUD:** the status bar is on the off hand (Options > VR Settings > Status Bar for the main hand); centre
  prints and messages float in front of you. Each weapon shows its ammo (and clip) on the weapon itself.
- **Crosshair:** Options > VR Settings > Crosshair: a dot, a laser or a soft laser from each muzzle.
- **VR Settings:** Options > VR Settings (or `menu_vr`) has the comfort, body, weapon and display settings;
  the sticks move and change, A selects, B goes back. "Set Height Now" calibrates the height while standing.

`vr_status` shows tracking, hand angles, hotspots and grab and two-handed state; `vr_dumpview` shows the drawn
hands, weapons and finger curls.

## If something goes wrong

Add `-condebug` to the command line (or `QuakeVR.bat -condebug`): the console goes to `qconsole.log` in the Quake folder, which
is the most useful thing to send me along with a description. In particular:

- **Nothing in the headset:** look for the `VR:` lines. They say which OpenXR call failed, with its result code.
  `vr_restart` retries after the headset is on or the runtime is running.
- **The picture is wrong** (double vision, wrong scale, swimming): `vr_status` output while it happens, and a
  screenshot of the desktop mirror (`vr_mirror 2` shows both eyes).
- **Hands or weapons are in the wrong place or at the wrong angle:** `vr_status` and `vr_dumpview` while holding the
  pose. Gun Angle in VR Settings is the first thing to adjust.
- **A crash:** the log up to the crash, and what you were doing.

## Testing without a headset

`vr_backend mock; vr_enabled 1` runs everything with a pretend headset. `vr_mock_button <main|off> <trigger|grip|primary|secondary|stickclick|menu> <0|1>`,
`vr_mock_stick <main|off> <x> <y>` `vr_mock_hand <main|off|head> <x> <y> <z> [<pitch> <yaw> <roll>]` and `vr_mock_look <pitch> <yaw>` drive it; `vr_mock_swing <period>`
swings the main hand for throwing tests.

Tuning the body: `vr_show_hip_holsters 1`, `vr_show_upper_holsters 1`, `vr_show_shoulder_holsters 1` and
`vr_show_virtual_stock 1` mark where the holsters and the virtual stock's shoulders are (green while a hand is
there); move them with the `vr_*_offset_*` cvars.
