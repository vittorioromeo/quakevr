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
| `vr_gunangle`, `vr_offhandpitch` | 39.5, 40.25 | weapon pitch relative to the controller (the shipped values, applied to OpenXR's grip pose, which points along the fist): **check this first** (Options > VR Settings > Gun Angle) |
| `vr_world_scale` | 1.25 | |
| `vr_height_calibration`, `vr_floor_offset` | 1.646, -21 | |
| `vr_mirror` | 1 | desktop window: 0 off, 1 left eye, 2 both eyes |
| `vr_deadzone` | 25 | stick deadzone, percent |
| `vr_weapon_grip_mode` | 0 | 1 = weapons stay in the hand without holding the grip (issue #31) |

## Throwing: what changed

Throws were imprecise for three reasons, all fixed:

1. **Too fast.** The QC multiplied the hand's speed (m/s) by 120 units/s, about 3.7 times true scale at the
   default world scale, so small errors in timing or direction turned into big misses. A thrown weapon now leaves the hand as fast as the hand
   moved (scaled by `vr_world_scale`, the per-weapon weight and `vr_weapon_throw_velocity_mult`).
2. **Wrong gravity.** At VR scale, Quake's gravity is about 3 g. Thrown weapons now fall at `vr_throw_gravity`
   (9.81 m/s², 0 = Quake gravity) until they first hit something, so arcs look and land like real throws.
3. **Late, laggy velocity.** Letting go of the grip happens tens of milliseconds after the release point, when the
   hand is already slowing. The old engine averaged 15 frames, which made this worse and depended on the frame
   rate. The release velocity now comes from the runtime's own controller velocity (IMU-fused). It is the peak
   within the last `vr_throw_window` (0.1 s), averaged over `vr_throw_peak_span` (25 ms), plus the wrist-flick
   term angular velocity × `vr_throw_lever_arm` (0.1 m). Spin comes from the real angular velocity too.

Damage is normalised, so the same hand speed deals the same damage as before.

To compare, `vr_debug_throw 1` prints every throw's estimate. `developer 1` also prints the spawned velocity,
gravity and spin. Things to try:

- If throws feel short or weak, raise `vr_weapon_throw_velocity_mult` (1.2–1.5 is common in VR games), or set
  `vr_throw_gravity` lower.
- Short, sharp flicks: `vr_throw_window 0.06`. Long, wind-up throws: `0.15`.
- The old behaviour, for comparison: `vr_throw_algorithm 0`, `vr_throw_gravity 0`, `vr_weapon_throw_velocity_mult 3.66`.

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

- **Body** (new): the old floating torso is replaced by a body whose arms reach your hands and which crouches and
  leans with your head (Options > VR Settings > Body: Off / Torso / Torso and arms / Full body). To see the whole
  pose, `vr_body_debug 2` (facing you) or `3` (from the side) shows a copy in front of you. Things to tell me:
  - where the elbows go when you aim, reload or reach behind you;
  - whether looking down at your chest feels right (`vr_body_torso_back`, metres the torso sits behind your neck);
  - whether crouching looks right (`vr_body_crouch_legs`).

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
`vr_mock_stick <main|off> <x> <y>` `vr_mock_hand <main|off|head> <x> <y> <z>` and `vr_mock_look <pitch> <yaw>` drive it; `vr_mock_swing <period>`
swings the main hand for throwing tests.

Tuning the body: `vr_show_hip_holsters 1`, `vr_show_upper_holsters 1`, `vr_show_shoulder_holsters 1` and
`vr_show_virtual_stock 1` mark where the holsters and the virtual stock's shoulders are (green while a hand is
there); move them with the `vr_*_offset_*` cvars.
