# Playtesting in the headset

For the author: building and installing, the controls, voice notes, what to try now and what to do when something
goes wrong. The testing tools (the mock headset, scripted motions, the test scripts and fixtures) are in
[TESTING.md](TESTING.md); what the port does is in [FEATURES.md](../FEATURES.md).

## Build and install

The quickest way: `Windows\package-quakevr.ps1 -Build -Fteqcc <path to fteqcc64.exe>` builds everything into
`dist\QuakeVR` (and `dist\QuakeVR.zip`); copy its contents into your Quake folder and run `QuakeVR.bat`.
By hand:

1. Build `Windows/VisualStudio/ironwail.sln`, **Release | x64**. The output is
   `Windows/VisualStudio/Build-ironwail/bin/x64/Release/ironwail.exe`, with `openxr_loader.dll` copied next to it.
2. The progs: step 1's build already compiles them into `quakevr/progs.dat` (with FTEQCC at `QvrQcCompiler`:
   [BUILDING.md](../BUILDING.md#building-the-quakec)); `QC/build.bat` (set `FTEQCC` to `fteqcc64.exe`) builds only
   them, with the checks.
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
names **by controller**: the main hand (the right controller) is the gamepad's right half, the off hand (the left) its
left half. `vr_stick_swap 1` swaps only what the sticks do (the right one moves).

| Control | Main hand key | Off hand key | Default binding (main / off) |
|---|---|---|---|
| Trigger | `RTRIGGER` | `LTRIGGER` | `+attack` / `+offhandattack` |
| Grip | `RSHOULDER` | `LSHOULDER` | `+grabmain` / `+graboff` |
| A / X (primary) | `ABUTTON` | `XBUTTON` | `+jump` / `+reloadoff`; held in the air with the grappling hook in: its unreel (the key still pressed) |
| B / Y (secondary) | `BBUTTON` | `YBUTTON` | `impulse 10` / `impulse 12` (next weapon); held with the grappling hook out: its reel, whatever it is bound to |
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
| `vr_gunangle`, `vr_offhandpitch` | 70, 40.25 | weapon pitch relative to the controller (70: `vr_defaults.cfg`'s shipped value): Advanced VR > Weapons > Hand/Gun Calibration (both hands at once: VR Settings > Hand Pitch) |
| `vr_world_scale` | 1.2 | |
| `vr_height_calibration`, `vr_floor_offset` | 1.646, -22 | |
| `vr_mirror` | 1 | desktop window: 0 off, 1 on (`vr_window_view`: 0 left eye, 1 left smoothed, 2 spectator, 3 both eyes, 4 right eye, 5 right smoothed) |
| `vr_deadzone` | 10 | stick deadzone, percent |
| `vr_weapon_grip_mode` | 0 | 1 = weapons stay in the hand without holding the grip (issue #31) |

## Throwing

How throws are measured, how strong they come out and how they fly: `docs/vr-port/THROWING.md` (its
section 3.4 lists the settings). `vr_debug_throw 1` prints every throw's estimate; `2` also prints how long
after the peak the release came. `developer 1` prints the spawned velocity, gravity, spin and age.

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
| #20, #14 status bar on the hands | done: on the off hand by default (Options > VR Settings > Status Bar for the main hand) |

## Voice notes while playing

Raise your off hand to your mouth, like a radio, and hold **Y** to talk; let go to save. "REC" and the
note's length show low in your view, and the hand buzzes as a note starts and ends. Away from your mouth
(more than about 30 cm) Y does what it always did; a note shorter than 0.8 s is taken for an accidental press
and dropped. Each note is saved in `quakevr/notes/` with a screenshot and where you were (map,
position, view, health, what each hand held). The microphone is Virtual Desktop's
(`vr_note_device "Virtual Desktop"`; `vr_note_devices` lists them); Gameplay > Voice Notes turns it off.
`+vr_note` records from a bound key too.

To turn the notes into text (Whisper, on your PC; the model is already downloaded):

    python Misc/quakevr/transcribe_notes.py

It transcribes the new notes and writes `quakevr/notes/NOTES.md`, every note with its transcript,
context and screenshot, ready to paste or to point me at.

## What to try now

- **The checklist:** Advanced VR Options > Debug > Checklist (also the menus' corner button) lists what to test in
  the headset or give feedback on, from `quakevr/checklist.txt`; tick each item as you go.
- **What changed lately:** [ROUND21.md](ROUND21.md), newest at the end; each section says what to try.
- **What the port does:** [FEATURES.md](../FEATURES.md).

The round logs before round 21 (`ROUND6.md` to `ROUND20.md`) were removed on 2026-10-06; to read one,
`git log --diff-filter=D -- docs/vr-port/ROUND20.md` gives the commit that removed it and `git show <commit>^:docs/vr-port/ROUND20.md`
prints it.

## If something goes wrong

Add `-condebug` to the command line (or `QuakeVR.bat -condebug`): the console goes to `qconsole.log` in the Quake folder, which
is the most useful thing to send me along with a description. In particular:

- **Nothing in the headset:** look for the `VR:` lines. They say which OpenXR call failed, with its result code.
  `vr_restart` retries after the headset is on or the runtime is running.
- **The picture is wrong** (double vision, wrong scale, swimming): `vr_status` output while it happens, and a
  screenshot of the desktop mirror (`vr_window_view 3` shows both eyes).
- **Hands or weapons are in the wrong place or at the wrong angle:** `vr_status` and `vr_dumpview` while holding the
  pose. Hand Pitch on VR Settings (Hand Calibration) is the first thing to adjust.
- **Fingers wrong on something held** (through it, or stuck open): `vr_debug_grasp 1` prints each grasp solve;
  `vr_grasp_dump main hand.obj` writes the drawn hand and the held model as an .obj to send me. Hand/Gun
  Calibration > Fit Fingers to What You Hold off shows the controller's curls alone, Jointed Hand off the old
  hands.
- **A crash:** the log up to the crash, and what you were doing.
- **Slower than it should be:** VR Settings > Advanced VR Options > Debug > Profiling and Memory: the Profiler
  Panel, a CSV Capture and the Memory Log (what each shows: [BENCHMARKS.md](BENCHMARKS.md), "Profiling in the
  game"). Note the time when it felt slower and send the capture.

`vr_status` shows tracking, hand angles, hotspots and grab and two-handed state; `vr_dumpview` shows the drawn
hands, weapons and finger curls.
