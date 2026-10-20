# The ragdoll tool (`-ragtool`, `vr_ragtool`)

A flat-screen workbench for the active ragdolls: pick a monster with a rig, play one of its motor-driven behaviours on
it, watch it slowed, paused or stepped with its skeleton, joint limits, motor targets, contacts and balance drawn over
it, and tune the settings that drive it live, grouped by behaviour, saved as presets you can promote to defaults.

It is the game itself in a mode, not a copy: the behaviours are set going through the same calls the game and its tests
make (`vr_ragdoll_act`, `vr_knockdown_test`, QC's parry stagger, the toolgun physgun's hold standing in for a hand),
and the physics, the drive and the rigs are the game's (`vr_ragactive.cpp`, `vr_ragstruggle.cpp`,
`vr_box3d_knockdown.cpp`, `vr_ragdoll.cpp`, `vr_ad_rigs.inc`). With the tool off nothing of it runs.

Code: `Quake/vr/vr_ragtool.cpp` (the mode, the behaviours, the registry, the commands, the camera and keys),
`Quake/vr/vr_box3d_ragtool.cpp` (the drawing and the measures), `Quake/vr/vr_menu_ragtool.inc` (its menu pages).

## Launching

    ironwail.exe -ragtool

`-ragtool` implies `-flatscreenmode` (no headset session: FLATPLAY.md) and starts the tool once the game is up: it loads
`vr_ragtool_map` (`vrragtriggers`, the Ragdoll Triggers room), makes you a god-mode, notarget onlooker and spawns the
subject 160 units ahead of the start, facing you. Add `-game ad` (with AD installed) for AD's monsters.

From a running game: `vr_ragtool` (console) or **Debug > Ragdoll Tool > Start**; `vr_ragtool here` keeps the map you
are in and stages the subject ahead of where you stand. `vr_ragtool_exit` gives the game back (camera, god mode and
notarget, the time scale).

## Controls (in the game, the menu closed)

| Input | What it does |
|---|---|
| Mouse | orbits the camera round the subject (your view angles turn it) |
| Wheel | zooms (`vr_ragtool_cam_dist`) |
| Tab | the tool's page (Debug > Ragdoll Tool); the game runs on behind it |
| Space | Play: a fresh subject, then the chosen behaviour |
| B / M | the next behaviour / the next model (made at once) |
| R | respawn the subject |
| P / . | pause or run / one server frame (paused) |
| [ / ] | slower / faster (`vr_ragtool_speed`, the time scale) |
| L | loop the behaviour |
| H / G | the hand's hold (chest, head, arm, leg, pelvis) / grab now or let go |
| Arrows, Page Up/Down | move the virtual hand (camera-relative, flat; up and down) |
| Esc | the main menu, as ever |

## The page (Tab)

The status line (model, behaviour and its time, paused, speed, holding), then:

- **Subject**: Model (every monster in the toolgun's spawn list with a ragdoll rig: id1's, the expansions', AD's when
  mounted; `vr_ragtool_models` lists them), Respawn.
- **Behaviour**: Behaviour, Play, Loop, Watch For (a run's length when the behaviour gives none), Push From (degrees
  round him a shove, blow, knockdown or parry comes from: 0 his front, 180 his back), Hold, Grab or Let Go, Hand Up/Down.
- **Time**: Pause, Single Step, Speed.
- **Show**: Skeleton and Frames (each part's axes, red x, green y, blue z, and its bone), Joint Limits (a ball joint's
  cone in cyan, a hinge's arc in orange, the bone as it is now in magenta), Motor Targets (the drive's targets: green as
  strong as the joint, grey limp; the pelvis pin yellow; a hand's or foot's goal red), Contacts (red pressed in, pink
  touching), Centre of Mass and Support (yellow centre of mass; its point on the floor green over the outline of what
  touches the floor, red off it), Physics Shapes, camera distance and offset, Log.
- **Parameters**: one page per group (below), each with Reset This Group; Menu Detail's Reset This Page works too.
- **Presets**: Save, Load, Reset All, List Settings, Status.

The lines are drawn over the flat view (x-ray), as a headset draws them in its eye's UI pass.

## Behaviours (`vr_ragtool_behaviours`, `vr_ragtool_play <name>`)

Each starts on a fresh, standing subject. stand, stagger (shove stagger and balance walk), stagger_fall, shove1 /
shove2 (the game's own one- and two-hand shove from Push From), knockdown, getup, rollover (knocked down from behind, on
his face, then up at once: the get-up's roll over), crawl, crawl_legless, death, stumble, flinch, knee, hit_stagger (a
strong blow's stagger), parry (QC's `VR_Parry_Ragdoll`), struggle / lift / drag (knocked down, then held by the virtual
hand at Hold; lift raises it 40 units, drag pulls it 64 towards Push From: Hold Arm for the arm drag's upright), flail
(taken by the chest and lifted: the flail in the hands), guard, lava (the dying flail), drown, throw.

The virtual hand is the toolgun physgun's hold (a spring from a kinematic point to the part); while the tool runs the
struggle takes it as the player's hand, and it keeps the subject down as a player's hand does (its knockdown timer pushed
on while held). Move it with the arrows and Page Up/Down, or `vr_ragtool_hand <dx> <dy> <dz>`.

A behaviour that will not start on a class says so in the console (`ragact: ... NOT begun`): that is the game's answer
for that monster, not the tool's.

## The log

Each run ends with one line (`vr_ragtool_log 1`):

    ragtool: run 3 stagger on Grunt (done): 5.01 s, ragdoll 95% of frames, tracking error mean 30.00 most 68.05 deg,
    strength 0.57, pelvis travel 130.2 u, lowest 4.0 u over the floor, end 17.3 u over it

Tracking error is the driven joints' mean miss of their motor targets (the bone's direction against its target's).
`vr_ragtool_log 2` adds a sample every half second. The behaviours' rolls depend on the time and the entity, so a run
varies a little: compare a few runs before and after an edit.

## Parameters and presets

The registry (`params` in `vr_ragtool.cpp`) lists each setting with its group and slider range; `vr_ragtool_params
[group]` prints them with their defaults when changed. Groups: Motors, Joint Groups, Stagger and Balance, Shove,
Knockdown and Get-Up, Death and Crawl, Struggle, Parry and Hit Stagger, Flail, Liquids and Hand.

Joint Groups are new settings, each 1 by default (the game unchanged): `vr_ragdoll_active_hertz_<group>`,
`vr_ragdoll_active_damping_<group>`, `vr_ragdoll_active_torque_<group>` scale the drive's spring rate, damping and most
force for the legs, torso, arms and head (`ragactive::Channel`); `vr_ragdoll_limits_<group>` scales the joints' limits
(cone, twist, a hinge's range) with `vr_ragdoll_limits` — set as the ragdoll is made, so Respawn to see it. The others are
the game's existing cvars.

`vr_ragtool_save [name]` writes every registry setting to `<game dir>/ragtool/<name>.cfg` (`preset` by default), the
changed ones marked `// default "..."`; `vr_ragtool_load [name]` reads it back; `vr_ragtool_reset [group|all]` restores
defaults. A preset is a plain cfg: `exec ragtool/<name>.cfg` works too. To make a preset the shipped default, its marked
lines are the values to put in `vr_cvars.inc`.

## Headless checks

`scratch/ragtool_test.py` in the agent worktree (not committed) ran: `-ragtool` launch flat, the grunt and AD's death
knight through every behaviour, a parameter edit (the legs' motors off) against three default runs, and a preset
save/reset/load round trip. Commands for scripts: `vr_ragtool_play <name>`, `vr_ragtool_spawn <class|label|row>`,
`vr_ragtool_status`, `vr_ragtool_pause [0|1]`, `vr_ragtool_step [frames]`.
