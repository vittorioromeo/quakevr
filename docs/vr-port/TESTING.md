# Testing in the headset

The first headset build. Everything below was checked on the desktop with the mock backend. OpenXR itself has
only been checked up to "no headset connected", so expect rough edges: tell me what you see and I will fix it.

## Build and install

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
| `vr_gunangle` | 0 | weapon pitch relative to the controller: **please tune this**, the old default was tied to OpenVR poses |
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
| #20, #14 status bar on the hands | P6 (HUD) |

## Not there yet

Teleport, two-handed aiming, finger tracking, holster hover highlights, body yaw from the hands, flick reload
(P5 part 2); the VR menus and the status bar on the hands (P6); particle presets, world text, the crosshair and
the smaller beam model (P7).
