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
| B / Y | opens or closes the toolgun's menu, on a small panel over the gun |
| A / X held | the sticks move and turn what the tool places or holds (no walking or turning meanwhile) |

The other hand: its laser points at the menu and its trigger clicks (as in the VR Settings); its Y goes back a page
when the gun is in the right hand (the right hand's B does when the gun is in the left); Back on the menu's first page
closes it. While the physgun holds a prop, the other hand's trigger freezes it.

A / X held, the sticks:

| Stick | Spawn and Physgun | Scale |
|---|---|---|
| the gun's, forward / back | pushes it away / pulls it in | grows / shrinks the aimed prop |
| the gun's, left / right | turns it (about the menu's Sticks Turn It About: yaw by default) | - |
| the other, up / down | raises / lowers it | - |
| the other, left / right | moves it aside | - |

The game goes on while the menu is open (monsters too); you can't walk meanwhile.

## The tools (menu: Tool)

- **Spawn.** Choose what on the menu (Choose What to Spawn: monsters, props, weapons, items), or Pick It From the World
  (the next shot chooses what it hits). A see-through ghost of it stands where the gun points (on the floor; off a wall),
  facing you; the trigger makes it there, as a map would. Reset Offsets and Turn puts the ghost back; Clear the Choice
  empties it.
- **Remove.** What the gun points at glows red; the trigger removes it (as Debug > Cheats' Scene clean-up does: its
  fire out, a head's flies quiet). Never you.
- **Physgun.** Hold the trigger on something to drag it at its distance (a prop's physics body follows the beam and
  shoves the others; a monster or a pickup is carried); let go, it falls with the beam's swing. The other hand's trigger
  freezes a prop where it is (it stays, others land on it); grab a frozen prop again and it falls when let go. Menu:
  Unfreeze Everything.
- **Scale.** A physics prop's box is drawn with handles: cyan on its faces, white on its corners. Aim at one (it turns
  orange), hold the trigger and pull or push: the prop grows or shrinks about its middle. Proportional Scaling on (the
  default) scales it whole; off, a face handle stretches it along that face's axis alone (corners always whole). A / X
  held, the gun's stick grows and shrinks it. Reset Its Size: the prop aimed at back to its size. 0.1x to 10x.
- **Joint.** Choose the joint on the menu (Joint), then shoot a prop and another: Weld (stuck as they are), Ball (they
  turn about the second point), Hinge (about the second face's normal), Slider (along it), Rope (never further apart
  than now; drawn brown), Spring (pulls back to now's distance; drawn steel). Unjoin: a prop shot loses its joints.
  Remove Every Joint. Joints last while both props do (frozen, let go of or scaled, they are made again).

## Settings (cvars)

| Cvar | Default | |
|---|---|---|
| `vr_toolgun_tool` | 0 | 0 spawn, 1 remove, 2 physgun, 3 scale, 4 joint |
| `vr_toolgun_joint` | 0 | 0 weld, 1 ball, 2 hinge, 3 slider, 4 rope, 5 spring, 6 unjoin |
| `vr_toolgun_scale_uniform` | 1 | Proportional Scaling |
| `vr_toolgun_turn_axis` | 1 | the gun's stick turns about 0 pitch, 1 yaw, 2 roll |
| `vr_toolgun_move_speed` | 64 | units a second, a stick pushed all the way |
| `vr_toolgun_turn_speed` | 90 | degrees a second |
| `vr_toolgun_range` | 2048 | units |
| `vr_toolgun_menu_height` | 8 | the menu on the gun: its height (units); Menu Size on its page |
| `vr_toolgun_menu_up`, `_forward`, `_tilt` | 4, 2, 20 | its bottom edge over the controller, ahead of it, its lean back (degrees) |
| `vr_toolgun_screen_scale`, `_up` | 0.02, 3.2 | the gun's little screen (the tool's name): its letters, its height over the hand; 0 none |

Commands: `vr_toolgun_select <n|clear>`, `vr_toolgun_pick`, `vr_toolgun_reset`, `vr_toolgun_menu`,
`vr_toolgun_unfreeze_all`, `vr_toolgun_unjoin_all`, `vr_toolgun_scale_reset`, `vr_toolgun_status` (Debug > Tools >
Toolgun).

## How it is made

- QC: `WID_TOOLGUN` 19 (`IID_TOOLGUN` 49, `HIP_IT_TOOLGUN`), `QC/vr_toolgun.qc` (`weapon_toolgun`,
  `VR_Toolgun_Remove`), `weapons.qc` W_AttackImpl does nothing for it. Weapon settings slot 26 (`vr_wofs_*_27`,
  `vr_weapons.inc`: the grunts' gun's, its Offset moved for the model's bounds; `vr_wofs_version` 40).
- The model: `Misc/quakevr/make_toolgun.py` (mdlgen boxes; the grunts' gun's model space: the grip at the origin).
- The engine: `Quake/vr/vr_toolgun.cpp` (the tools, the aim, the ghost, the glow, the gun's screen), its hooks in
  `vr_input.cpp` (the hand's buttons, the sticks), `vr_panel.cpp` (the menu's panel on the tracked controller),
  `vr_menuui.cpp` (the game runs under it; the mock laser is the other hand's), `vr_menu_toolgun.inc` (its pages; the
  cheats are Debug > Cheats' own rows), `vr_box3d.cpp` (`spawnClass`, pins: a pinned prop is a kinematic Fixture body,
  the toolgun's joints and their re-making), the shaders' red glow (`vr_glsl.h`: glow 2..3).

## Tests (mock headset)

`impulse 169; vr_weapon_grip_mode 1` (the weapon stays in the open mock hand), `vr_mock_hand main 0.1 1.35 -0.3 70 0
0` (aims level; a lower pitch aims down), `vr_mock_button main secondary 1/0` (the menu), `vr_mock_laser <x> <y>` then
`vr_mock_button off trigger 1/0` (the mock laser is the off hand's while the menu is on the gun in the main hand; `menu_vr
rows` gives the rows' tops: click 4 below), `vr_mock_button main trigger 1/0` (the tool), `vr_mock_button main primary
1` with `vr_mock_stick main|off <x> <y>` (the offsets), `vr_toolgun_status`. ROUND21.md, "The toolgun", has the runs.

## Known limits

- With the gun in the right hand, A (jump) is the tool's while you hold it.
- Saved games don't keep frozen props or joints (they fall, unjoined, when loaded).
- Brush-model items (health, ammo boxes, explosive boxes) show their ghost only on maps that already have them loaded.
