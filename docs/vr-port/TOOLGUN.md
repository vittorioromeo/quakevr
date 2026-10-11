# The toolgun

A debug and sandbox tool held as a pistol, after Garry's Mod's toolgun and physgun (the author's request of
2026-10-09: `kit/briefs/toolgun_request.md`). A weapon like any other: held, holstered, dropped, thrown, taken. It has
no ammunition and fires nothing; its hand's buttons are the tool's. Single player.

## Getting one

- vrfiringrange: lying on the floor north of the crowbar, labelled "toolgun".
- Debug > Cheats and Recording > A Toolgun in Your Hand (`impulse 169`; `impulse 189` puts it in the off hand), or
  Debug > Tools > Toolgun; Debug > Tests > Spawn Pickup Weapons > Toolgun (one on the floor ahead).
- In a map: `weapon_toolgun`, or `func_weapon_grabbable` with weapon 19.

## Controls (the toolgun's hand)

| Button | What it does |
|---|---|
| Trigger | uses the tool (below) on what the gun points at (a beam in the tool's colour shows where) |
| Stick press | the next tool (A / X held: the one before); the gun's screen names it. Not when bullet time has that stick (`vr_bullettime_trigger`) |
| B / Y | opens or closes the toolgun's panel over the gun |
| A / X held | the sticks move and turn what the tool places or holds (no walking or turning meanwhile); what the physgun holds snaps to 15 degree steps |

The other hand: a thin laser points at the panel and its trigger clicks there (off the panel the trigger is its own
again: its weapon fires). While the physgun holds a prop, the other hand's trigger (off the panel) freezes it.

## The panel

Its own small panel (not the VR Settings): `vr_toolgun_panel_size` units wide (6, about 23 cm), over the gun as
`vr_toolgun_menu_up`, `_forward` and `_tilt` place it. The game goes on while it is open: you can walk, and the gun's
trigger still uses the tool. A menu opened meanwhile hides it until it closes.

- **The top row:** the tools' tiles (Spawn, Remove, Physgun, Scale, Joint), each with its icon: a tap switches. More: the
  cheats and the panel's size. Under them a line: the tool and its choice.
- **Under it, only the chosen tool's choices:**
  - Spawn: tabs (Monsters, Props, Weapons, Items) and a grid of thumbnails (each model drawn from its file once,
    `vr_toolgun_icons.cpp`), eight a page (`<`, `>`); a tap chooses. Pick (the next shot picks from the world), Reset
    (the ghost's offsets and turn), Clear.
  - Remove: how; Clear Props Made Since the Map Loaded.
  - Physgun: Turn With the Wrist, Snap (X/A) Off/15/45/90, Sticks Turn Pitch/Yaw/Roll, Unfreeze Everything.
  - Scale: Proportional, Reset Its Size.
  - Joint: the seven joints as icon tiles (the chosen one lit, a line on what it does), Remove Every Joint.
  - More: twenty cheats as buttons (Debug > Cheats and Recording's own rows: God Mode, Noclip, ..., Pause; a switch
    lit while on) and Panel Size.

A / X held, the sticks:

| Stick | Spawn and Physgun | Scale |
|---|---|---|
| the gun's, forward / back | pushes it away / pulls it in | grows / shrinks the aimed prop |
| the gun's, left / right | turns it (about the panel's Sticks Turn: yaw by default) | - |
| the other, up / down | raises / lowers it | - |
| the other, left / right | moves it aside | - |

## The tools (the panel's top row, or the gun's stick press)

- **Spawn.** Choose what on the panel's grid (tabs: monsters, props, weapons, items), or Pick (the next shot chooses
  what it hits). A see-through ghost of it stands where the gun points (on the floor; off a wall), facing you; the
  trigger makes it there, as a map would. Reset puts the ghost back (its offsets and turn); Clear empties the choice.
- **Remove.** What the gun points at glows red; the trigger removes it (as Debug > Cheats' Scene clean-up does: its
  fire out, a head's flies quiet). Never you.
- **Physgun.** Hold the trigger on something to drag it at its distance (a prop's physics body follows the beam and
  shoves the others; a monster or a pickup is carried); let go, it falls with the beam's swing. It turns as your hand
  turns (Turn With the Wrist, `vr_toolgun_physgun_wrist`): it keeps its way to the gun as it was when taken, as if on a
  skewer, so a twist of the wrist turns it and turning yourself turns it with you; the sticks (A / X held) turn it more,
  and meanwhile its angles snap to `vr_toolgun_snap` degree steps (15). A monster or a pickup only turns about the up
  axis, as the hand's way turns. The other hand's trigger freezes a prop where it is (it stays, others land on it); grab
  a frozen prop again and it falls when let go. Unfreeze Everything.
- **Scale.** A physics prop's box is drawn with handles: cyan on its faces, white on its corners. Aim at one (it turns
  orange), hold the trigger and pull or push: the prop grows or shrinks about its middle. Proportional on (the default)
  scales it whole; off, a face handle stretches it along that face's axis alone (corners always whole). A / X held, the
  gun's stick grows and shrinks it. Reset Its Size: the prop aimed at back to its size. 0.1x to 10x.
- **Joint.** Choose the joint on the panel's tiles, then shoot a prop and another: Weld (stuck as they are), Ball (they
  turn about the second point), Hinge (about the second face's normal), Slider (along it), Rope (never further apart
  than now; drawn brown), Spring (pulls back to now's distance; drawn steel). Unjoin: a prop shot loses its joints.
  Remove Every Joint. Joints last while both props do (frozen, let go of or scaled, they are made again).

## Settings (cvars)

| Cvar | Default | |
|---|---|---|
| `vr_toolgun_tool` | 0 | 0 spawn, 1 remove, 2 physgun, 3 scale, 4 joint |
| `vr_toolgun_joint` | 0 | 0 weld, 1 ball, 2 hinge, 3 slider, 4 rope, 5 spring, 6 unjoin |
| `vr_toolgun_scale_uniform` | 1 | Proportional |
| `vr_toolgun_turn_axis` | 1 | the gun's stick turns about 0 pitch, 1 yaw, 2 roll |
| `vr_toolgun_move_speed` | 64 | units a second, a stick pushed all the way |
| `vr_toolgun_turn_speed` | 90 | degrees a second |
| `vr_toolgun_range` | 2048 | units |
| `vr_toolgun_panel_size` | 6 | the panel's width (units; its height three quarters of it); Panel Size on More |
| `vr_toolgun_menu_up`, `_forward`, `_tilt` | 4, 2, 20 | the panel's bottom edge over the controller, ahead of it, its lean back (degrees) |
| `vr_toolgun_laser_size` | 0.3 | the other hand's laser at the panel: its width and its spot, times the menus' laser's |
| `vr_toolgun_physgun_wrist` | 1 | what the physgun holds turns with the hand (0: only the sticks turn it) |
| `vr_toolgun_snap` | 15 | A / X held, what the physgun holds turns in steps of this many degrees (0 never) |
| `vr_toolgun_screen_scale`, `_up` | 0.02, 3.2 | the gun's little screen (the tool's name): its letters, its height over the hand; 0 none |

Commands: `vr_toolgun_select <n|clear>`, `vr_toolgun_pick`, `vr_toolgun_reset`, `vr_toolgun_menu` (the panel),
`vr_toolgun_next [-1]`, `vr_toolgun_unfreeze_all`, `vr_toolgun_unjoin_all`, `vr_toolgun_scale_reset`,
`vr_toolgun_status`, `vr_toolgun_panel_dump`, `vr_toolgun_icons` (Debug > Tools > Toolgun); `vr_toolgun_mock_laser`
(tests).

## How it is made

- QC: `WID_TOOLGUN` 19 (`IID_TOOLGUN` 49, `HIP_IT_TOOLGUN`), `QC/vr_toolgun.qc` (`weapon_toolgun`,
  `VR_Toolgun_Remove`), `weapons.qc` W_AttackImpl does nothing for it. Weapon settings slot 26 (`vr_wofs_*_27`,
  `vr_weapons.inc`: the grunts' gun's, its Offset moved for the model's bounds; `vr_wofs_version` 40).
- The model: `Misc/quakevr/make_toolgun.py` (mdlgen boxes; the grunts' gun's model space: the grip at the origin).
- The engine: `Quake/vr/vr_toolgun.cpp` (the tools, the aim, the ghost, the glow, the gun's screen, the physgun's
  wrist), its hooks in `vr_input.cpp` (the hand's buttons, the sticks); `vr_toolgun_panel.cpp` (the panel: one layout
  function run twice a frame, for the laser's input in the host frame and for its image in the 2D pass's end, drawn
  with the VR menus' widgets: `vr_menuui.cpp` paintButton, paintSwitch, paintSlider, drawLaser; shown in each eye from
  `vr_panel.cpp`); `vr_toolgun_icons.cpp` (the thumbnails: each model's file read again and drawn by a small CPU
  rasteriser, made a few a frame and kept); `vr_menu_toolgun.inc` (the More page's cheats: Debug > Cheats' own rows by
  their labels); `vr_box3d.cpp` (`spawnClass`, pins: a pinned prop is a kinematic Fixture body, the toolgun's joints
  and their re-making), the shaders' red glow (`vr_glsl.h`: glow 2..3).

## Tests (mock headset)

`impulse 169; vr_weapon_grip_mode 1` (the weapon stays in the open mock hand), `vr_mock_hand main 0.0 1.5 -0.3 70 0
0` (aims level, the panel in front of the eyes; a lower pitch aims down), `vr_mock_button main secondary 1/0` (the
panel), `vr_toolgun_mock_laser <x> <y>` (its 256 x 192 pixels from the top left) or `vr_toolgun_mock_laser <name>` (a
widget's middle: its label, as `vr_toolgun_panel_dump` lists them) then `vr_mock_button off trigger 1/0` (the laser
hand's: the off hand's while the gun is in the main hand), `vr_mock_button main stickclick 1/0` (the next tool),
`vr_mock_button main trigger 1/0` (the tool), `vr_mock_button main primary 1` with `vr_mock_stick main|off <x> <y>`
(the offsets), `vr_toolgun_status` (the tool, the panel, the gun's screen, what the physgun holds and its angles).

- `Misc/quakevr/toolgun_panel_test.sh <agent> [--shots]`: the panel's tiles hit by their pixels, the tools cycled by the
  stick press (the gun's screen each time), the Joint and Hinge tiles, God Mode, the Panel Size slider dragged, the
  physgun turning a crate with the hand's roll and yaw, snapping, and a grunt by its yaw alone (ROUND22.md, "The
  toolgun's own panel").
- `Misc/quakevr/toolgun_joint_menu_test.sh <agent> [--debug]`: the joint tiles clicked with the laser (every choice from
  every other, 70 picks) after the physgun held a crate and with a joint half made, each kind then made and unjoined,
  the gun in either hand (ROUND21.md, "Toolgun: the joint choice crash").

## Known limits

- With the gun in the right hand, A (jump) is the tool's while you hold it.
- Saved games don't keep frozen props or joints (they fall, unjoined, when loaded).
- Brush-model items (health, ammo boxes, explosive boxes) show their ghost only on maps that already have them loaded
  (their thumbnails are drawn from their files whatever the map).
