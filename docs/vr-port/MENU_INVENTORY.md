# Quake VR menu and cvar inventory (read-only research, 2026-10-03)

The data behind `MENU_REVIEW.md` (read that first). Method: static parse of the menu builders, every `QVR_CVAR` in
`Quake/vr/vr_cvars.inc` grepped across `Quake/` and `QC/` with comments stripped, heuristic tiering (the parsing
scripts were one-off and are not kept). Row counts come from a static parse. Pages built in
loops or with if/else branches (Weapon Offsets parts, Retro categories, Body Calibration, Checklist) are approximate.
The engine's own `menu_vr dump` plus `Misc/quakevr/menu_coverage.py` would give exact runtime coverage. ROUND21 used
them and counted 802 options.

## 1. Key numbers

| What | Count |
|---|---|
| VR menu pages (`pages[]`, vr_menu.cpp:4428-4552) | **126** (menu_vr 0..125) |
| ... of which hub pages (only links) | 6 (Advanced VR Options, Combat, Movement, Carrying and Throwing, Weapons, HUD and Menus) |
| Max depth below VR Settings | 5 (Ragdolls - <class>). Depth histogram: d0 1, d1 4, d2 14, d3 68, d4 32, d5 7 |
| Statically parsed rows (settings, actions, headers, links, info) | ~2,550 |
| Setting rows (slider/cycle/toggle/hue/class/weapon-key) | ~1,740 |
| Action/command rows (buttons) | ~351, of which ~240 on the Debug pages |
| Interactive rows with a `.help()` text | ~1,930 of ~2,250 (86%) |
| Pages over 30 rows | 28 (see section 3) |
| Quake VR cvars in `vr_cvars.inc` (`QVR_CVAR`) | **1,681** (+ `vr_backend`; 1,704 before the 23 dead and retired ones were removed, 2026-10-03: section 5a) |
| ... archived (CVAR_ARCHIVE, saved) / not saved | 1,507 / 174 |
| ... reachable from the VR menu (static) | 1,523 (89%) |
| ... console-only | 158 (99 archived "hidden tuning", 59 dev/test) |
| ... with no read anywhere (dead) | 21 retired + 9 test-only, see section 5 |
| Dynamic per-slot cvars, weapons | 260 keys x 32 slots = 8,320 `vr_wofs_<key>_NN` (vr_weapons.inc, vr_weapons.cpp:588; the 224 of 7 retired keys removed) |
| Dynamic per-slot cvars, props | 45 keys x 64 slots = 2,880 `vr_prop_<key>_NN` (vr_props.inc, vr_props.cpp:440; the 64 of the retired key removed) |
| Dynamic retro cvars | 18 categories x 12 params = 216 `vr_retro_<cat>_*`, + 24 `vr_retro_all_*`, 18 `vr_retro_all_to_*`, 25 `vr_retro_edit_*` (the 12 legacy `vr_retro_body*` removed) (vr_retro.cpp:1347-1615) |
| QC-side cvars | **0 new**. QC/vr_cvars.qc takes handles (`VR_CVAR_HMAKE` -> `cvar_hmake`, vr_builtins.cpp:136) on engine-registered cvars. |
| Engine cvars shown in the VR menu | cl_alwaysrun, cl_forwardspeed, cl_movespeedkey, scr_menubgalpha, ui_live_preview, vid_fsaa, r_particles, r_wateralpha/lavaalpha/slimealpha/telealpha, snd_waterfx (+ r_novis, r_showbboxes, developer on Debug) |
| Ironwail desktop options (menu.c:3133-3248, OPTIONS_LIST) | 6 menus, ~90 items. "VR Settings" is row 9 of Options. |
| `vr_defaults.cfg` lines | 148 vr cvars + engine r_/gl_. Every name in the shipped cfgs exists in code (`cfgcheck.py`). |

Tier counts for the 1,681 vr cvars (heuristic, section 4): **A 20 · B 60 · C 906 · C? (archived, console-only) 99 ·
D 596** (the 23 retired ones, R, were removed). By page tier, interactive menu rows are about **B 117 · C 1,002 · D 973**. Nearly half of
everything in the VR menu is developer, tuning or test material.

## 2. Architecture

- **Engine entry.** Ironwail's options list (Quake/menu.c:3133 `OPTIONS_LIST`, macro-generated) has an `OPT_VR` "VR
  Settings" item (menu.c:3145, 4870), and it calls `VR_Menu_Open()`. Main menu row 0 is "VR Calibration" (menu.c:1159,
  1288). It asks for confirmation, then runs `vr_setup`. The `m_vr` state dispatches draw, key and mousemove to
  vr_menu.cpp (menu.c:7409, 7553, 7660).
- **Table-driven, in code.** Each page is a C++ builder `za::Vector<Item> pageX()` that returns a list of `Item`s
  (vr_menu.cpp:81 `struct Item`). The constructors are `header`, `slider` (by `cvar_t&` or by name: a name with no
  cvar is dropped, vr_menu.cpp:202), `classSlider` (-1 = "Global"), `hueSlider` (-1 = "Player's"), `cycle`,
  `toggle`, `action(fn)`, `command("console text")`, `open(label, pageIndex(builder))`, `info`, `infoLine`, `row`.
  The modifiers are `.help(text)` (a line under the list) and `.extend()` (keys can step past the slider's range).
  Builders sit in vr_menu.cpp (7,789 lines) and in vr_menu_pages.inc ("old Quake VR settings"), vr_menu_props.inc
  and vr_menu_recording.inc. Pages are rebuilt when their state changes (the held weapon, the checklist and so on).
- **Page registry.** The `pages[]` array (vr_menu.cpp:4428) holds `{title, builder, home}`. The index is the stable
  `menu_vr <n>` number, so new pages are appended (that is why the numbers look random). `home` defines the tree and
  where Back goes. Pages link each other freely with `open()`, giving many cross-links: Weapon Damage is linked from 3
  pages, Stamina from 3, Held Object Weights from 4.
- **Nesting.** VR Settings (0) holds Tuning links, Comfort, Weapons, and the links Body and Display (49), Headset (50),
  Sound (77) and Advanced VR Options (1). Advanced VR Options holds the groups Game, Body and Weapons, Display and
  Playtesting, with the hubs below them. The Weapon Offsets parts (84-92) and the Retro categories (96-113) are linked
  from loops (`weaponOffsetsPartPages[]` at vr_menu.cpp:4566 and the Retro page).
- **No data files.** There is no JSON or cfg menu definition. The calibration room's wall boards refer to menu paths
  as `{menu:<page title>}`, resolved by `menu::pathTo`/`expandPaths` (vr_menu.hpp). `vr_menu_path_check` validates them.
- **Per-weapon and per-prop pages** edit `vr_wofs_<key>_<slot>` (`weaponOffsetsSlider`/`s(...)`, vr_menu.cpp:5124)
  and `vr_prop_*` for the weapon or prop in hand. They show "hold a weapon in a game to adjust it" otherwise.
- **VR vs desktop.** The same menus serve both. `vr_menu_vr_style` (vr_menuui.cpp:625) redraws Ironwail's and the
  VR pages in the headset as a panel with spaced rows, modern sliders and switches, and a laser pointer. The corner
  buttons are Back to game, Advanced VR, Levels and **Checklist** (a playtest list, shown to every player,
  vr_menuui.cpp:437). Ironwail's desktop menus (Display: resolution/vsync, Controller: gyro/flick stick, Game:
  FOV/view bob/mouse look, Interface...) are **not hidden in VR**, though most of them do nothing in the headset.
  **Master Sound and Music Volume exist only in the desktop Options list.** The VR Sound page has no volume.
- **Search.** Ironwail lists have type-to-search (menu.c:567 `listsearch_t`), keyboard only. The VR pages have no search.
- **Reset to defaults.** It is global only: Options > "Reset All" (menu.c:4852: `resetcfg; exec default.cfg`). Some
  page or section resets exist: Reset Swimming to Defaults (vr_menu_pages.inc:122/180), Reset Hand Moves
  (vr_menu_pages.inc:230), Retro Reset per kind and Reset All (vr_menu_pages.inc:707/883), Reset This Weapon
  (vr_menu.cpp:5275), Holstered/Flashlight Back to 0, Arms "Reset Tweaks" (vr_menu.cpp:1335), Ragdoll "All Global",
  Monster Hitbox "All Match Their Boxes". From the console: `reset <cvar>` and `vr_default`/`vr_savedefaults`
  (vr_cvars.cpp:570-660).
- **Defaults migration.** `defaultChanges[]` (vr_cvars.cpp:41-250, `vr_cfg_version` up to about 84) moves configs
  that still hold an old default to the new one. It is relevant if cvars are renamed or merged.
- **Config saving.** `saveConfigNow` runs when the menu closes (vr_cvars.hpp:22). `personal()` (vr_cvars.cpp:604)
  lists the per-player cvars kept out of vr_defaults.cfg.
- **Other features.** Live Preview (`ui_live_preview`), scroll and position memory per page (`vr_menu_positions`,
  `vr_menu_remember`) and drop-down lists (`vr_menu_dropdown`).

## 3. Pages: size, depth, tier

Rows count every Item the builder emits: headers, links and info lines included. Static parse, so pages with
branches may be over-counted. The retro category pages are built by `retroCategoryItems` (vr_menu_pages.inc:875) and
show 0 here. Each really has about 1 toggle, ~11 settings, ~3 headers and a reset, so about 15 rows x 18 pages.
Long pages (>30 rows): Debug - Tests 154, Debug - Tools 83, Throwing and Physics 78, Gore 71, Carrying 63, Grappling
Hook 52, Small Gibs 52, Burning 47, Debug - Reports 44, Weapon Offsets - Two-Handed 42, - Hand and Grip 40, Body 39,
Weapon Weights 39, Recording 38, Debug - Logging 38, Batting and Catching 36, Held Object Offsets 36, Debug -
Profiling 36, Weapon Damage 35, Player Hitbox 35, Ragdolls 34, Stamina 33, Aiming 33, VR Settings 32, Damage and
Knockback 32, Arms and Pauldrons 32, Retro Lighting 32, Parry and Bash 31. (ROUND21.md:9722 "Menus reorganized"
targeted at most 30 rows per page; pages have grown again since.)

| # | Page | depth | rows | settings | actions | page tier |
|---|---|---|---|---|---|---|
| 0 | VR Settings **(LONG)** | 0 | 32 | 18 | 0 | B |
| 1 | Advanced VR Options | 1 | 18 | 0 | 0 | hub |
| 2 | Play | 2 | 9 | 0 | 7 | B |
| 3 | World | 2 | 13 | 8 | 0 | C |
| 4 | Parry and Bash **(LONG)** | 3 | 31 | 27 | 0 | C |
| 5 | Melee | 3 | 15 | 13 | 0 | C |
| 6 | Motion Recorder | 2 | 11 | 6 | 1 | D |
| 7 | Review Takes | 2 | 11 | 2 | 3 | D |
| 8 | Take | 3 | 20 | 3 | 11 | D |
| 9 | Gore **(LONG)** | 2 | 71 | 61 | 0 | C |
| 10 | Throwing and Physics **(LONG)** | 3 | 78 | 72 | 0 | D |
| 11 | Carrying **(LONG)** | 3 | 63 | 56 | 0 | C |
| 12 | Force Grab | 3 | 17 | 17 | 0 | C |
| 13 | Grappling Hook **(LONG)** | 3 | 52 | 45 | 0 | C |
| 14 | Body **(LONG)** | 2 | 39 | 32 | 0 | C |
| 15 | Body - Arms and Pauldrons **(LONG)** | 3 | 32 | 24 | 1 | D |
| 16 | Body Calibration | 3 | 14 | 2 | 6 | B |
| 17 | Flashlight | 2 | 29 | 22 | 0 | C |
| 18 | Player Calibration | 3 | 2 | 2 | 0 | B |
| 19 | Locomotion | 3 | 20 | 16 | 0 | B |
| 20 | Swimming | 3 | 28 | 24 | 1 | C |
| 21 | Immersion | 3 | 18 | 16 | 0 | C |
| 22 | Hand/Gun Calibration | 3 | 25 | 20 | 2 | C |
| 23 | Weapon Offsets | 3 | 12 | 4 | 3 | D |
| 24 | Aiming **(LONG)** | 3 | 33 | 23 | 0 | C |
| 25 | Hotspots | 3 | 24 | 21 | 0 | C |
| 26 | Wrist Gadget | 3 | 10 | 9 | 0 | B |
| 27 | Screens | 3 | 28 | 23 | 1 | C |
| 28 | Colours | 3 | 16 | 14 | 0 | C |
| 29 | Status Bar | 3 | 9 | 9 | 0 | B |
| 30 | Crosshair | 3 | 6 | 6 | 0 | B |
| 31 | Menu | 3 | 11 | 11 | 0 | C |
| 32 | Graphics | 2 | 23 | 9 | 0 | B |
| 33 | Graphics - Lights | 3 | 23 | 22 | 0 | C |
| 34 | Graphics - Shadows | 3 | 21 | 19 | 0 | C |
| 35 | Graphics - Surfaces | 3 | 27 | 23 | 0 | C |
| 36 | Graphics - Liquids | 3 | 26 | 24 | 0 | C |
| 37 | Graphics - Post-processing | 3 | 12 | 11 | 0 | C |
| 38 | Graphics - Models and Effects | 3 | 22 | 20 | 0 | C |
| 39 | Particles | 3 | 3 | 3 | 0 | C |
| 40 | Transparency | 3 | 6 | 6 | 0 | C |
| 41 | Held Object Offsets **(LONG)** | 3 | 36 | 28 | 2 | D |
| 42 | Weapon Weights **(LONG)** | 3 | 39 | 18 | 4 | D |
| 43 | Held Object Weights | 3 | 22 | 11 | 2 | D |
| 44 | Combat | 2 | 10 | 0 | 0 | hub |
| 45 | Movement | 2 | 6 | 0 | 0 | hub |
| 46 | Carrying and Throwing | 2 | 10 | 0 | 0 | hub |
| 47 | Weapons | 2 | 15 | 0 | 0 | hub |
| 48 | HUD and Menus | 2 | 6 | 0 | 0 | hub |
| 49 | Body and Display | 1 | 26 | 21 | 1 | B |
| 50 | Headset | 1 | 12 | 10 | 1 | B |
| 51 | Damage and Knockback **(LONG)** | 3 | 32 | 27 | 0 | C |
| 52 | Stamina **(LONG)** | 3 | 33 | 27 | 0 | C |
| 53 | Batting and Catching **(LONG)** | 3 | 36 | 31 | 0 | C |
| 54 | Climbing | 3 | 28 | 24 | 1 | C |
| 55 | Wall Torches | 3 | 12 | 11 | 0 | C |
| 56 | Rocks and Bricks | 3 | 12 | 12 | 0 | C |
| 57 | Gibs and Corpses | 3 | 21 | 17 | 0 | C |
| 58 | Flashlight - Low Grip | 3 | 8 | 6 | 0 | D |
| 59 | Flashlight - Overhead Grip | 3 | 8 | 6 | 0 | D |
| 60 | Flashlight - On a Gun or Head | 3 | 18 | 15 | 0 | D |
| 61 | Fingers and Collisions | 3 | 17 | 15 | 0 | D |
| 62 | Weight and Damage | 3 | 8 | 8 | 0 | C |
| 63 | Hip Holsters | 3 | 26 | 24 | 0 | C |
| 64 | Debug | 2 | 11 | 2 | 0 | D |
| 65 | Recording **(LONG)** | 3 | 38 | 31 | 2 | C |
| 66 | Debug - Views | 3 | 27 | 19 | 8 | D |
| 67 | Debug - Logging **(LONG)** | 3 | 38 | 35 | 1 | D |
| 68 | Debug - Profiling and Memory **(LONG)** | 3 | 36 | 13 | 19 | D |
| 69 | Debug - Reports **(LONG)** | 3 | 44 | 0 | 40 | D |
| 70 | Debug - Tools **(LONG)** | 3 | 83 | 3 | 74 | D |
| 71 | Debug - Tests **(LONG)** | 3 | 154 | 35 | 98 | D |
| 72 | Checklist | 3 | 6 | 1 | 1 | D |
| 73 | Player Hitbox **(LONG)** | 3 | 35 | 20 | 11 | D |
| 74 | Monster Hitbox | 3 | 11 | 4 | 4 | D |
| 75 | Lightning Gun in Water | 3 | 9 | 7 | 2 | C |
| 76 | Enemy Weapons | 3 | 21 | 15 | 0 | C |
| 77 | Sound | 1 | 28 | 23 | 1 | C |
| 78 | Crates | 3 | 26 | 22 | 0 | C |
| 79 | Weapon Damage **(LONG)** | 3 | 35 | 26 | 0 | C |
| 80 | Weapon Effects | 3 | 23 | 17 | 1 | C |
| 81 | Enemy Shoves | 3 | 9 | 9 | 0 | C |
| 82 | Chainsaw Engine | 4 | 13 | 10 | 0 | D |
| 83 | Bullet Time | 3 | 28 | 23 | 1 | C |
| 84 | Weapon Offsets - Hand and Grip **(LONG)** | 4 | 40 | 35 | 0 | D |
| 85 | Weapon Offsets - Fingers | 4 | 17 | 16 | 0 | D |
| 86 | Weapon Offsets - Muzzle and Sights | 4 | 16 | 8 | 4 | D |
| 87 | Weapon Offsets - Two-Handed **(LONG)** | 4 | 42 | 36 | 4 | D |
| 88 | Weapon Offsets - Virtual Stock | 4 | 12 | 8 | 0 | D |
| 89 | Weapon Offsets - Ammo Screen | 4 | 9 | 8 | 0 | D |
| 90 | Weapon Offsets - Holstered | 4 | 6 | 3 | 2 | D |
| 91 | Weapon Offsets - Effects | 4 | 18 | 15 | 1 | D |
| 92 | Weapon Offsets - Flashlight | 4 | 12 | 8 | 2 | D |
| 93 | Small Gibs **(LONG)** | 3 | 52 | 47 | 0 | D |
| 94 | Burning **(LONG)** | 3 | 47 | 42 | 0 | C |
| 95 | Graphics - Retro Textures | 3 | 12 | 1 | 5 | C |
| 96 | Retro Textures - World | 4 | 0 | 0 | 0 | D |
| 97 | Retro Textures - Brush Entities | 4 | 0 | 0 | 0 | D |
| 98 | Retro Textures - Item Pickups | 4 | 0 | 0 | 0 | D |
| 99 | Retro Textures - Props and Debris | 4 | 0 | 0 | 0 | D |
| 100 | Retro Textures - Gibs | 4 | 0 | 0 | 0 | D |
| 101 | Retro Textures - Small Gibs | 4 | 0 | 0 | 0 | D |
| 102 | Retro Textures - Weapons in the World | 4 | 0 | 0 | 0 | D |
| 103 | Retro Textures - Held Weapons | 4 | 0 | 0 | 0 | D |
| 104 | Retro Textures - Monsters | 4 | 0 | 0 | 0 | D |
| 105 | Retro Textures - Your Hands | 4 | 0 | 0 | 0 | D |
| 106 | Retro Textures - Your Arms | 4 | 0 | 0 | 0 | D |
| 107 | Retro Textures - Your Torso | 4 | 0 | 0 | 0 | D |
| 108 | Retro Textures - Your Legs | 4 | 0 | 0 | 0 | D |
| 109 | Retro Textures - Your Gear | 4 | 0 | 0 | 0 | D |
| 110 | Retro Textures - Decals | 4 | 0 | 0 | 0 | D |
| 111 | Retro Textures - Particles | 4 | 0 | 0 | 0 | D |
| 112 | Retro Textures - Sprites | 4 | 0 | 0 | 0 | D |
| 113 | Retro Textures - Other Models | 4 | 0 | 0 | 0 | D |
| 114 | Retro Textures - Override | 4 | 12 | 4 | 5 | D |
| 115 | Retro Textures - All Categories | 4 | 16 | 5 | 7 | D |
| 116 | Graphics - Retro Lighting **(LONG)** | 3 | 32 | 23 | 4 | C |
| 117 | Gibs and Corpses - Ragdolls **(LONG)** | 4 | 34 | 22 | 0 | D |
| 118 | Ragdolls - Grunt | 5 | 10 | 9 | 1 | D |
| 119 | Ragdolls - Knight | 5 | 10 | 9 | 1 | D |
| 120 | Gibs and Corpses - Corpse Damage and Health | 4 | 30 | 28 | 0 | D |
| 121 | Ragdolls - Ogre | 5 | 10 | 9 | 1 | D |
| 122 | Ragdolls - Enforcer | 5 | 10 | 9 | 1 | D |
| 123 | Ragdolls - Death Knight | 5 | 10 | 9 | 1 | D |
| 124 | Ragdolls - Rottweiler | 5 | 10 | 9 | 1 | D |
| 125 | Ragdolls - Scrag | 5 | 10 | 9 | 1 | D |

## 4. Tier classification (heuristic, with evidence)

**Rules (classify.py).**
1. **A, first-run essentials.** This is the explicit list. Its evidence is the calibration room's wall-button options
   (vr_setup.cpp:47-63: `vr_snap_turn, vr_turn_speed, vr_movement_mode, vr_stick_swap, cl_alwaysrun,
   vr_teleport_enabled, vr_weapon_grip_mode, vr_gadget_arm, vr_flashlight_side, vr_bodycal_seated, vr_world_scale,
   vr_body_mode, vr_hud_mode, vr_crosshair, vr_climb`), plus height (`vr_height_calibration`) and body calibration
   (vr_setup.hpp steps 1-2). README "First steps" (README.md:178-197) adds `vr_gunangle` (+ `vr_offhandpitch`),
   `vr_xr_runtime`, `vr_render_scale`, `vr_enabled` and, for performance, `vr_graphics_preset`.
2. **Retired (R).** The inc comment said "retired ... kept so saved configs load silently", plus the migration-only
   `vr_lefthanded` and `vr_gadget_hand`. All 23 were removed on 2026-10-03 (section 5a): none is left.
3. **D by name.** The name matches debug|test|mock|show|log|profile|bench|trace|dump|fixed_frames|motion_|notes|
   *_version|*_seed.
4. **Otherwise, the best tier of the pages it appears on.** Page tiers:
   - **B:** VR Settings, Body and Display, Headset, Locomotion, Crosshair, Status Bar, Wrist Gadget, Graphics (top),
     Play, Body/Player Calibration.
   - **C:** the gameplay and visual pages under Advanced (Combat, Movement, Carrying, World, Gore, Body, Flashlight,
     Weapons-hub pages, HUD pages, Graphics subpages, Sound, Menu, Recording, Hand/Gun Calibration).
   - **D:** Debug*, Checklist, Motion Recorder, Review Takes and Take. Also the content-authoring and tuning pages:
     Weapon Offsets and its 9 parts, Weapon Weights, Held Object Offsets/Weights, Fingers and Collisions, Arms and
     Pauldrons, the 3 Flashlight grip/mount pages, Player and Monster Hitbox, Ragdolls and its 7 class pages, Corpse
     Damage and Health, Small Gibs, Chainsaw Engine, Throwing and Physics, and the Retro per-category, Override and
     All pages.
5. **Not in the menu.** Archived ones become "C?" (hidden tuning). Unsaved ones become D.

**Caveat.** Tiers C and D within gameplay pages are coarse. Many C-page rows are really internal tuning, for example
the "Hand on Hold (Looks Only)" sliders on Climbing, "Reverb Update" and "Frame Size" on Sound, and the Speed Curve,
Glide and Power Knee rows on Swimming. A per-row pass is needed before culling.

**Result.** For vr cvars: A 20, B 60, C 906, C? 99, D 596 (R 23 before they were removed). For VR menu interactive rows by page: B 117, C
~1,000, D ~970. Rough category tags for vr cvars (name and page regex; overlaps allowed): gameplay 1,159, graphics
324, dev 117+, controls 79, HUD 77, audio 53, multiplayer 32, comfort 25. Accessibility has no dedicated settings.
The nearest are colours/hues, haptics and Seated position. There is **no comfort vignette** for locomotion. The only
vignette is `vr_bullettime_fx_vignette`.

**The VR Settings landing page (tier mix).**
- It opens with **"Tuning"**: 6 links to Weapon Offsets, Weapon Weights, Held Object Offsets/Weights, Hand/Gun
  Calibration and Body Calibration (vr_menu.cpp:4607-4616). These are mostly author or tuner tools (D), placed above
  the Comfort essentials.
- Then Comfort (A/B), Weapons (A/B), and More (links).
- Height / Set Height Now (A) is one click away, on Body and Display. README.md:181 still says it is in the "Body
  section" of VR Settings.
- The Sound page is linked from the landing page, but it is mostly Steam Audio engine tuning: HRTF anti-aliasing,
  occlusion rays, reverb update, frame size, limiter. It has **no volume**.
- Dominant Eye (`vr_dominant_eye`, a personal setting per vr_cvars.cpp:606) appears only on Weapon Offsets - Muzzle
  and Sights (depth 4).
- Headset gamma/contrast (`vr_gamma`, `vr_contrast`) are on Advanced > Graphics, not on Headset. Brightness and
  Contrast on the desktop Options do not reach the headset (vr_cvars.inc:1237).
- There is no handedness setting. "There is no main hand setting" (vr_setup.hpp:8). Sides are split across
  `vr_stick_swap`, `vr_gadget_arm` and `vr_flashlight_side`. The old single `vr_lefthanded` was removed (2026-10-03).

## 5. Cull and consolidate candidates

### 5a. Dead: registered, read by nothing (comments stripped). REMOVED 2026-10-03

Everything in this section was removed on 2026-10-03 (no compatibility with old configs kept: the heavy development
phase; none was in a shipped cfg). Kept for the record:

| cvar | inc line | evidence |
|---|---|---|
| vr_physics_engine | 213 | "retired (Box3D is the only choice)"; mentioned only in a comment, vr_rigid.cpp:8 |
| vr_box3d_weapon_mass | 231 | retired (each weapon's own mass) |
| vr_wpn_pos_weight, _offset, _mult, _2h_help_offset, _2h_help_mult; vr_wpn_dir_weight (same 5) | 563-572 | retired, "Spring only": the Speed Limit weight model is gone |
| vr_weight_model, vr_weight_props | 573-574 | retired, same |
| vr_sword_drop | 1378 | retired (knights always drop their sword) |
| vr_carry_reach | 1449 | retired (vr_carry_grab_bias); comment-only ref in vr_held.hpp:115 |
| vr_corpse_health | 1511 | retired; read only by the config-84 migration (vr_cvars.cpp:557) |
| vr_parry_sound_burst | 1652 | retired (vr_parry_cooldown) |
| vr_parry_stamina_show | 1663 | retired (vr_gadget_stamina) |
| vr_finger_grip_open | 1829 | retired, **but still CVAR_ARCHIVE**, so it is written to every config |
| vr_hand_collide_props | 1919 | retired; only in defaultChanges (vr_cvars.cpp:119) |

That is 21 vr cvars (19 CVAR_NONE, plus vr_finger_grip_open archived; vr_corpse_health and vr_hand_collide_props are
migration-only). There are also **288 retired per-slot cvars**: 7 weapon keys x 32 (vr_weapons.inc:352-362: weight,
w_posmult, w_dirmult, w_2hposmult, w_2hdirmult, w_hvelmult, w_htvelmult) and 1 prop key x 64 (vr_props.inc:73
"blunt"). Add the 12 `vr_retro_body*` legacy cvars (vr_retro.cpp:1592) and `vr_lefthanded`/`vr_gadget_hand`
(vr_cvars.inc:412-413, which only map old configs to the new side settings, vr_hands.cpp:663-685). All exist so old
configs load silently. All of them (about 320) were removed outright, with the migrations that used them
(vr_cvars.cpp's config-84 corpse health and defaultChanges entries, hands::migrateHandedness, the retro body aliases).
Old motion takes' `dominant hand` and `vr_lefthanded` keys still play back (take parsing in vr_motion_play.cpp, not
cvars).

### 5b. Test or debug only (read only from test code or test QC)
- `vr_smallgibs_test`, `_n`, `_dmg`, `_dummy`, `_crowd`, `_blasts`, `_hand`, `_dist` (vr_cvars.inc:1198-1206; only
  QC/vr_smallgibs_test.qc).
- `vr_test_weaponinst` and `_slot` (1902-1903; only QC/vr_weaponinst_test.qc).
- There are 166 vr cvars named debug/test/mock/show/log/profile/version/... in total. 59 of them are console-only and
  unsaved. The Debug pages (6 pages + Checklist; ~400 rows: Tests 154, Tools 83, Reports 44, Logging 38, Profiling
  36, Views 27) are reachable by every player from Advanced VR Options > Playtesting, as are Motion Recorder, Review
  Takes and Take.

### 5c. Same cvar on several pages: 70 cvars on 2-3 pages
These are mostly deliberate quick copies (ROUND21.md:9800-9806 lists the deliberate ones). In several cases the
**label or range differs** for the same cvar:
- `vr_snap_turn`: "Turning" (cycle Smooth/30/45/90) on VR Settings vs "Turn" (slider 0-90) on Locomotion.
- `vr_turn_speed`: 1-8 vs 0-10.
- `vr_movement_mode`: "Move Towards: Head/Hand" vs "Movement Mode: Follow Head/Follow Off Hand".
- `vr_teleport_enabled`: "Teleport" vs "Teleportation".
- `vr_roomscale_move_mult`: "Room Scale" vs "Roomscale Move Mult.".
- `vr_deadzone`: "Stick Deadzone" vs "Deadzone".
- `vr_gunangle`/`vr_offhandpitch`: "Gun Angle"/"Off Hand Angle" vs "Main/Off Hand Pitch (down)".
- `vr_disablehaptics`: "Haptics On/Off" vs "Disable Haptics", the inverted sense.
- `vr_leg_holster_model_enabled`: "Holster Models" vs "Show Holster Slots".
- `vr_virtual_stock_thresh`: "Virtual Stock Thresh." vs "Stock Distance".
- `vr_weapon_throw_velocity_mult`: "Throw Speed" vs "Throw Speed Mult.".
- `vr_menu_scale`: 0.08-0.3 vs 0.05-0.6.
- `vr_hud_mode`: on 3 pages. `vr_2h_mode`: on 3 pages.
- The grenade pouch block (8 cvars) is duplicated in full on Batting and Catching and Hip Holsters.
- Climbing stamina (3) is on Climbing and Stamina. Decals and gib blood (5) are on Gore and Graphics - Models and
  Effects.
- Body model (4) and menu (3) cvars are on Body/Menu and on Body and Display.

Full list: `where.json` / run section 5c script.

### 5d. Families that could be merged or table-driven
- **Ragdoll per class.** There are 85 `vr_ragdoll_*` cvars: 7 class pages x 9 `classSlider`s with -1 = Global
  (vr_menu.cpp:2827-3030) plus globals. These are developer tuning per monster.
- **Monster hitbox per class.** 13 `vr_mhull_<class>` (vr_cvars.inc:18-30), on a page labelled "Prototype".
- **Damage per weapon.** 74 cvars named damage/dmg. Weapon Damage has 26 settings, and per-enemy weapons
  (chainsaw 21, gruntgun...) add more.
- **Effect hues.** 10 `*_hue` effect hues already default to "Player's" (-1). One Player Hue could cover all of them,
  with overrides tucked away.
- **Haptics.** 9 per-feature haptic strengths (pain, holster, counter, grapple, burn, bullettime, forcegrab...) plus
  the global `vr_disablehaptics` and `vr_holster_haptics` could become one master strength plus a few toggles.
- **Sound volumes.** 47 cvars named sound/volume/snd. Per-effect volumes (mantle grunt, grab sound, bullet-time
  sounds...) could fold into an "Effects volume".
- **Legacy finger offsets.** 27 legacy finger offset cvars: `vr_finger_<finger>_{x,y,z}`, `vr_fingers_*`,
  `vr_fingers_and_base_*` (+offhand) (vr_cvars.inc:1950-1975). They are archived, console-only and read in
  vr_view.cpp:1372-1399. Likely superseded by the per-weapon finger keys and the fitted grasp; needs confirming.
- **Flashlight grip fingers.** 2 x 14 `vr_flashlight_{low,high}_*` duplicate the per-weapon finger model.
- **Offsets.** 127 cvars end in x/y/z/pitch/yaw/roll. Most are placement offsets better set by a "grab and place"
  posing tool, which already exists for weapons (vr_posing.cpp).
- **Throwing internals.** 47 `vr_throw_*` cvars, ~20 of them console-only (`vr_throw_lookahead`, `peak_span`,
  `ang_*`, `gain_lo/hi`, `release_*`, `assist_*`...). Throwing and Physics (78 rows) is the 3rd-largest page.
- **Small gibs.** 56 `vr_smallgibs_*` (Small Gibs page 52 rows). Gore has 71 rows and 35 `vr_gore_*`.
- **Burning.** 35 `vr_burn_*` (page 47 rows) and 20 `vr_walltorch_*`.

### 5e. Hidden archived cvars (C?, 99)
These are archived (saved, shipped-tunable) but on no page. Examples: `vr_compat_muzzle`, `vr_item_float_height`,
the throw internals above, 5 `vr_box3d_*` pushes and substeps, `vr_snd_hrtf_sofa`, `vr_snd_limiter_release`, the
`vr_bullettime_sound_*` paths, `vr_xr_runtime_json`, `vr_upscale_radius`, `vr_foveated_inner/outer`,
`vr_controller_legacy_pose`, `vr_melee_wrist_speed`, `vr_2h_spread_reduction`, `vr_forcegrabbable_*` (4),
`vr_player_stepsize`, 8 `vr_bodycal_*` measurements (set by calibration), and the 27 finger offsets. Most are tuning
constants that could become compiled constants. A few are personal measurements, which is fine to keep hidden.

### 5f. Menu or cfg entries pointing at nothing
- All 157 vr cvars named in shipped cfgs (vr_defaults.cfg, quakevr.cfg, default.cfg, vr_bindings.cfg) exist. Every
  cvar argument of a menu row exists.
- `command()` rows use **`wait5`** (vr_menu.cpp:3596 and 7 Debug - Tests ragdoll rows), but the engine registers only
  `wait` (cmd.c:773) and no `wait5` alias was found. The kit scripts (Misc/quakevr/*.sh) use `waitN` too, so it is
  probably a test-harness token. In the engine it would print "Unknown command". Unverified; worth a check.
- Menu rows that take a cvar by name (`slider("...", "name")`) are silently dropped if the cvar is missing
  (vr_menu.cpp:202). A rename would hide a row without warning.

### 5g. Not saved but in the menu
120 menu settings are CVAR_NONE, so they reset on restart. Most are Debug and Test toggles (intended). Also check
gameplay rows such as `vr_timescale` (Slow Motion) and the `vr_hull_*`/`vr_gameplayfix_*` set by quakevr.cfg.

## 6. Existing helpful infrastructure

- **Graphics presets.** `vr_graphics_preset` -1 Custom / 0 Off..4 Ultra (vr_cvars.inc:1078; applied in
  vr_lighting.cpp:1691-1730; Graphics page row 1, vr_menu_pages.inc:532; documented in SETTINGS.md:126).
- **No comfort, gameplay or "realism" presets.** The nearest thing is the calibration room's per-setting preset
  buttons (`vr_setup_option <key>`, vr_setup.cpp:31-63; 15 options with 2-4 presets each).
- **First-run wizard.** "VR Calibration" is the main menu's first row (menu.c:1159). The `vr_setup` calibration room
  (maps/vrcalibration.bsp) measures height, then body poses (vr_bodycal), and has wall buttons for the 15 main
  options. Boards point to menu paths by title (`{menu:...}`). It is **not launched automatically on first run**: the
  main menu cursor defaults to Single Player (menu.c:1154).
- **Help texts.** About 86% of interactive rows have `.help()`, shown under the list while the row is selected.
- **Resets.** Global "Reset All", plus the scattered page resets listed in section 2. There is no generic
  per-page "reset this page" built from the page's own items. `resetSwimming` hard-codes its list of names.
- **No "show advanced" toggle.** The split is structural: VR Settings vs the Advanced VR Options page. Advanced
  VR is also one click from every menu via the corner button. Debug and Playtesting are always visible, and so is the
  **Checklist** corner button.
- **Docs.**
  - docs/SETTINGS.md (373 lines) walks the VR Settings and Advanced pages, presets, weapon offsets, bindings, configs
    and resets, plus "Important settings by topic". It names 98 of 1,681 vr cvars.
  - docs/FEATURES.md (231 lines) is feature-oriented and names 3 cvars.
  - README.md:161-202 "First steps" is the closest thing to an A-tier list.
- **Doc drift to fix during the redesign.**
  - README calls the reset "Options > Reset to defaults"; the menu says "Reset All".
  - README says Advanced opens "about twenty more pages"; there are 120.
  - README places Height and Body in VR Settings sections that moved to Body and Display.
  - SETTINGS.md:331 says the config is written when you quit; it is also written when the menu closes
    (vr_cvars.hpp:22).
- **Tooling.**
  - `menu_vr list`, `menu_vr dump` and `menu_vr pos` (vr_menu.hpp `command_f`).
  - `Misc/quakevr/menu_coverage.py` compares options before and after a reorganization.
  - `vr_menu_path_check` checks the calibration room's menu paths.
  - `vr_savedefaults` and `vr_default`.
  - `defaultChanges[]` handles default migrations.
  - Together these make a large restructure safe to verify.
- **History.** ROUND21.md:9722 "Menus reorganized" holds the previous redesign's rules: about 30 rows per page, at
  most 3 levels, duplicates removed, page numbers stable. Its open questions: Tuning at the top of VR Settings, and
  whether Weapon Offsets should be split (it has since been split into 9 parts).

## 7. Full menu tree (static parse; `→ link ... [cross-link]` = a link to a page that lives elsewhere in the tree)

- **VR Settings** [menu_vr 0] — 32 rows / 18 settings / 0 actions **LONG** (vr_menu.cpp:4607)
  - — Tuning —
  - → link `Weapon Offsets (Held Weapon)` to Weapon Offsets [cross-link]
  - → link `Weapon Weights (Held Weapon)` to Weapon Weights [cross-link]
  - → link `Held Object Offsets (Held Prop)` to Held Object Offsets [cross-link]
  - → link `Held Object Weights (Held Prop)` to Held Object Weights [cross-link]
  - → link `Hand/Gun Calibration` to Hand/Gun Calibration [cross-link]
  - → link `Body Calibration` to Body Calibration [cross-link]
  - — Comfort —
  - Turning → `vr_snap_turn`
  - Turn Speed → `vr_turn_speed`
  - Move Towards → `vr_movement_mode`
  - Swap Stick Functions → `vr_stick_swap`
  - Default Speed → `cl_alwaysrun`
  - Stick Deadzone → `vr_deadzone`
  - Teleport → `vr_teleport_enabled`
  - Teleport Range → `vr_teleport_range`
  - Room Scale → `vr_roomscale_move_mult`
  - — Weapons —
  - Gun Angle → `vr_gunangle`
  - Off Hand Angle → `vr_offhandpitch`
  - Weapon Grip → `vr_weapon_grip_mode`
  - Two-Handed → `vr_2h_mode`
  - Two-Handed Hand-Off → `vr_2h_handoff`
  - Throw Speed → `vr_weapon_throw_velocity_mult`
  - Throw Gravity → `vr_throw_gravity`
  - Force Grab → `vr_forcegrab_mode`
  - Haptics → `vr_disablehaptics`
  - — More —
  - **Body and Display** [menu_vr 49] — 26 rows / 21 settings / 1 actions (vr_menu.cpp:4647)
    - — Body —
    - Wrist Gadget Arm → `vr_gadget_arm`
    - Flashlight Side → `vr_flashlight_side`
    - Height → `vr_height_calibration`
    - [action] Set Height Now → calibrateHeight()
    - World Scale → `vr_world_scale`
    - Floor Offset → `vr_floor_offset`
    - Chest Flashlight → `vr_flashlight`
    - — Body Model —
    - Body → `vr_body_mode`
    - Build → `vr_body_build`
    - Torso Offset → `vr_body_torso_back`
    - Legs Offset → `vr_body_legs_back`
    - Shoulders Offset → `vr_body_shoulders_back`
    - Holster Models → `vr_leg_holster_model_enabled`
    - — Display —
    - HUD → `vr_hud_mode`
    - Status Bar → `vr_sbar_mode`
    - HUD Scale → `vr_hud_scale`
    - Crosshair → `vr_crosshair`
    - Crosshair Size → `vr_crosshair_size`
    - Menu Distance → `vr_menu_distance`
    - Menu Scale → `vr_menu_scale`
    - Menu Background Opacity → `scr_menubgalpha`
    - Desktop Mirror → `vr_mirror`
    - → link `Recording (Window View)` to Recording [cross-link]
  - **Headset** [menu_vr 50] — 12 rows / 10 settings / 1 actions (vr_menu.cpp:4684)
    - — Headset —
    - VR → `vr_enabled`
    - [action] Restart VR → restartVr()
    - OpenXR Runtime → `vr_xr_runtime`
    - Render Scale → `vr_render_scale`
    - Upscaling → `vr_upscale`
    - Sharpness → `vr_upscale_sharpness`
    - Foveated Rendering → `vr_foveated`
    - Hide Lens Corners → `vr_visibility_mask`
    - Near Clip → `vr_nearclip`
    - Held Items at the Eyes → `vr_nearclip_held`
    - Float Depth → `vr_depth_float`
  - **Sound** [menu_vr 77] — 28 rows / 23 settings / 1 actions (vr_menu.cpp:838)
    - Spatial Audio → `vr_snd_spatial`
    - — Direction —
    - Sounds Around Your Head (HRTF) → `vr_snd_hrtf`
    - HRTF Smoothing → `vr_snd_hrtf_interp`
    - Full-Band Sound → `vr_snd_fullband`
    - HRTF Anti-Aliasing → `vr_snd_antialias`
    - HRTF Volume → `vr_snd_hrtf_gain`
    - Spatial Voices → `vr_snd_voices`
    - — Walls and Rooms —
    - Occlusion → `vr_snd_occlusion`
    - Occlusion Rays → `vr_snd_occlusion_samples`
    - Sound Source Size → `vr_snd_occlusion_radius`
    - Air Absorption → `vr_snd_air`
    - Distance Falloff → `vr_snd_falloff`
    - Room Reverb → `vr_snd_reverb`
    - Reverb Quality → `vr_snd_reverb_quality`
    - Reverb Update → `vr_snd_reverb_interval`
    - Underwater Muffle → `snd_waterfx`
    - — Movement and Nearness —
    - Weapons From Your Hands → `vr_snd_hands`
    - Sounds Follow Things → `vr_snd_follow`
    - Doppler → `vr_snd_doppler`
    - Near Field → `vr_snd_nearfield`
    - — Advanced —
    - Mix Limiter → `vr_snd_limiter`
    - Limiter Ceiling → `vr_snd_limiter_ceiling`
    - Frame Size → `vr_snd_frame`
    - [cmd] Spatial Audio Info → `vr_snd_info`
  - **Advanced VR Options** [menu_vr 1] — 18 rows / 0 settings / 0 actions (vr_menu.cpp:4712)
    - — Game —
    - **Play** [menu_vr 2] — 9 rows / 0 settings / 7 actions (vr_menu.cpp:594)
      - — Maps —
      - [action] VR Calibration → playCalibration()
      - [action] VR Hub → playHub()
      - [action] Tutorial → playTutorial()
      - [action] Firing Range → playFiringRange()
      - — Bots (Multiplayer) —
      - [action] Add Bot (Team 0) → addBotTeam0()
      - [action] Add Bot (Team 1) → addBotTeam1()
      - [action] Kick Bot → kickBot()
    - **Combat** [menu_vr 44] — 10 rows / 0 settings / 0 actions (vr_menu.cpp:4739)
      - **Melee** [menu_vr 5] — 15 rows / 13 settings / 0 actions (vr_menu_pages.inc:370)
        - Swing Speed → `vr_melee_speed`
        - Pommel Strike Wait → `vr_melee_pommel_wait`
        - Gun Butt Travel → `vr_melee_butt_run`
        - Punch Damage Mult. → `vr_melee_punch_mult`
        - Damage Multiplier → `vr_melee_dmg_multiplier`
        - Range Multiplier → `vr_melee_range_multiplier`
        - Quad: Extra Melee Damage → `vr_quad_melee_damage`
        - Quad: Extra Melee Reach → `vr_quad_melee_range`
        - Melee Bloodlust → `vr_melee_bloodlust`
        - Melee Bloodlust Mult. → `vr_melee_bloodlust_mult`
        - → link `Weapon Damage` to Weapon Damage [cross-link]
        - — Headbutt —
        - Headbutt → `vr_headbutt`
        - Headbutt Speed → `vr_headbutt_speed`
        - Headbutt Damage → `vr_headbutt_damage`
      - **Parry and Bash** [menu_vr 4] — 31 rows / 27 settings / 0 actions **LONG** (vr_menu.cpp:972)
        - — Parry —
        - Parry → `vr_parry`
        - Parry Angle → `vr_parry_angle`
        - Parry Reach → `vr_parry_reach`
        - Parry Damage Reduction → `vr_parry_reduction`
        - Parry Drop Chance → `vr_parry_drop_chance`
        - Parry Arm Knock → `vr_parry_wobble`
        - Unarmed Parry → `vr_parry_unarmed`
        - Unarmed Parry Reduction → `vr_parry_unarmed_reduction`
        - Parry Cooldown → `vr_parry_cooldown`
        - Parry Cooldown: Whole Attack → `vr_parry_cooldown_attack`
        - — Bash and Shove —
        - Bash → `vr_bash`
        - Bash Speed → `vr_bash_speed`
        - Shove Speed → `vr_shove_speed`
        - Bash Damage → `vr_bash_damage`
        - Bash Push → `vr_bash_push`
        - Bash and Parry Sounds → `vr_bash_sound`
        - — Counter-Attacks —
        - Counter-Attacks → `vr_counter`
        - Counter Window → `vr_counter_window`
        - Counter Damage → `vr_counter_damage`
        - Counter Sounds → `vr_counter_sound`
        - Counter Window Glow → `vr_counter_glow`
        - Counter Pulses → `vr_counter_haptic`
        - — Training Dummy Attacks —
        - Time Between Blows → `vr_dummy_attack_period`
        - Randomness → `vr_dummy_attack_jitter`
        - Wind-Up → `vr_dummy_attack_windup`
        - Reach → `vr_dummy_attack_reach`
        - Damage → `vr_dummy_attack_damage`
      - **Stamina** [menu_vr 52] — 33 rows / 27 settings / 0 actions **LONG** (vr_menu.cpp:1029)
        - — Parry Stamina —
        - Parry Stamina → `vr_parry_stamina`
        - Stamina → `vr_parry_stamina_max`
        - One-Handed Parry Cost → `vr_parry_stamina_cost`
        - Two-Handed Parry Cost → `vr_parry_stamina_cost_2h`
        - Rest Before Recovering → `vr_parry_stamina_delay`
        - Recovery Rate → `vr_parry_stamina_regen`
        - Tiring Warning → `vr_parry_stamina_warn`
        - Stamina on the Gadget → `vr_gadget_stamina`
        - — Shove and Strike Stamina —
        - Shove Stamina → `vr_shove_stamina`
        - One-Handed Shove Cost → `vr_shove_stamina_cost`
        - Two-Handed Shove Cost → `vr_shove_stamina_cost_2h`
        - Strike Stamina → `vr_strike_stamina`
        - Punch Cost → `vr_strike_stamina_punch`
        - Weapon Strike Cost → `vr_strike_stamina_cost`
        - Two-Handed Strike Cost → `vr_strike_stamina_cost_2h`
        - Exhausted Damage → `vr_stamina_exhausted_damage`
        - Exhausted Knockback → `vr_stamina_exhausted_push`
        - — Climbing Stamina —
        - Climbing Stamina → `vr_climb_stamina`
        - Hanging Cost, One Hand → `vr_climb_stamina_rate`
        - Hanging Cost, Two Hands → `vr_climb_stamina_rate_2h`
        - — Tired Arms —
        - Shake From Stamina → `vr_fatigue_shake_from`
        - Shake Distance → `vr_fatigue_shake`
        - Shake Turn → `vr_fatigue_shake_angle`
        - Shake Speed → `vr_fatigue_shake_speed`
        - Shake Whenever Tired → `vr_fatigue_shake_always`
        - — Tired Legs —
        - Slower When Tired → `vr_stamina_speed`
        - Slowest Run → `vr_stamina_speed_min`
        - → link `Heavy When Tired (Aiming)` to Aiming [cross-link]
      - **Batting and Catching** [menu_vr 53] — 36 rows / 31 settings / 0 actions **LONG** (vr_menu.cpp:1087)
        - — Batting Projectiles —
        - Bat Back Projectiles → `vr_deflect`
        - Batting Reach → `vr_deflect_radius`
        - Batting Swing Speed → `vr_deflect_speed`
        - Batting Timing → `vr_deflect_window`
        - Bash Batting Reach → `vr_bash_deflect_radius`
        - Bash Batting Timing → `vr_bash_deflect_window`
        - Batting Bounce → `vr_deflect_bounce`
        - Batting Aim Assist → `vr_deflect_aim_assist`
        - — Grenades —
        - Catch Grenades → `vr_grenade_catch`
        - Held Grenade Fuse → `vr_grenade_held_fuse`
        - Fuse Resets Every Catch → `vr_grenade_fuse_regrab`
        - Catch Radius → `vr_grenade_catch_radius`
        - Catch Window → `vr_grenade_catch_window`
        - Returned Grenades Hit Like Yours → `vr_grenade_return_full`
        - Shoot Grenades → `vr_grenade_shoot`
        - Grenade Shot Size → `vr_grenade_shoot_pad`
        - Blows Set Grenades Off → `vr_grenade_shoot_melee`
        - Blow Speed to Set Off → `vr_grenade_melee_speed`
        - Weapon Blow Speed to Set Off → `vr_grenade_melee_speed_weapon`
        - Blow Damage to Set Off → `vr_grenade_melee_damage`
        - — Hand Grenades —
        - Hand Grenades → `vr_handgrenade`
        - Arm Hand Grenades → `vr_handgrenade_arm`
        - Hand Grenade Fuse → `vr_handgrenade_fuse`
        - — Grenade Pouch —
        - Show Grenade Pouch → `vr_show_grenade_pouch`
        - Pouch X → `vr_grenade_pouch_x`
        - Pouch Y → `vr_grenade_pouch_y`
        - Pouch Z → `vr_grenade_pouch_z`
        - Pouch Threshold → `vr_grenade_pouch_thresh`
        - Grenade In Hand Pitch → `vr_grenade_pouch_hold_pitch`
        - Grenade In Hand Yaw → `vr_grenade_pouch_hold_yaw`
        - Grenade In Hand Roll → `vr_grenade_pouch_hold_roll`
        - → link `Pouch Turn (Hip Holsters)` to Hip Holsters [cross-link]
      - **Damage and Knockback** [menu_vr 51] — 32 rows / 27 settings / 0 actions **LONG** (vr_menu.cpp:913)
        - — Hit Detection —
        - Precise Hit Detection → `vr_hit_precise`
        - Guns Tolerance → `vr_hit_tolerance_guns`
        - Grappling Hook Tolerance → `vr_hit_tolerance_grapple`
        - Melee Tolerance → `vr_hit_tolerance_melee`
        - Thrown Tolerance → `vr_hit_tolerance_thrown`
        - — Damage —
        - Damage to Enemies → `vr_damage_to_enemies`
        - Damage to You → `vr_damage_to_player`
        - Self Damage → `vr_damage_self`
        - — Positional Damage —
        - Positional Damage → `vr_positional_damage`
        - Headshot Damage → `vr_headshot_mult`
        - Arm Shot Damage → `vr_limbshot_mult`
        - Leg Shot Damage → `vr_legshot_mult`
        - Headshot Sound → `vr_headshot_sound`
        - — Knockback —
        - Knockback → `vr_push`
        - Your Melee Hits → `vr_melee_push`
        - Weapon Hits → `vr_hit_push`
        - Killing Blows → `vr_kill_push`
        - Parry Pushes Enemy → `vr_parry_push_enemy`
        - Parry Pushes You → `vr_parry_push_player`
        - Monsters' Blows Push You → `vr_melee_push_player`
        - — When You're Hit —
        - Hits Knock Your Hands → `vr_pain_knock`
        - Knock per Damage → `vr_pain_knock_strength`
        - Largest Knock → `vr_pain_knock_max`
        - Knock Tip → `vr_pain_knock_tip`
        - Knock Across Your View → `vr_pain_knock_seen`
        - Knock Time → `vr_pain_knock_time`
        - Hit Buzz → `vr_pain_haptics`
      - **Weapon Damage** [menu_vr 79] — 35 rows / 26 settings / 0 actions **LONG** (vr_menu.cpp:764)
        - — Guns —
        - Shotgun → `vr_dmg_shotgun`
        - Double Shotgun → `vr_dmg_super_shotgun`
        - Nailgun → `vr_dmg_nail`
        - Super Nailgun → `vr_dmg_super_nail`
        - Grenade → `vr_dmg_grenade`
        - Rocket → `vr_dmg_rocket`
        - Lightning Gun → `vr_dmg_lightning`
        - — Scourge of Armagon —
        - Proximity Gun → `vr_dmg_proximity`
        - Laser Cannon → `vr_dmg_laser`
        - Mjolnir's Lightning → `vr_dmg_mjolnir_lightning`
        - — Dissolution of Eternity —
        - Lava Nails → `vr_dmg_lava_nail`
        - Super Lava Nails → `vr_dmg_super_lava_nail`
        - Multi-Grenade → `vr_dmg_multi_grenade`
        - Multi-Rocket → `vr_dmg_multi_rocket`
        - Plasma Gun → `vr_dmg_plasma`
        - — Melee —
        - Fist → `vr_dmg_fist`
        - Axe → `vr_dmg_axe`
        - Crowbar → `vr_crowbar_damage`
        - Gun as a Club → `vr_dmg_gun_bash`
        - Mjolnir → `vr_dmg_mjolnir`
        - — Enemy Weapons —
        - Knight's Sword → `vr_sword_damage_mult`
        - Ogre's Chainsaw Swung → `vr_dmg_chainsaw_swing`
        - Ogre's Chainsaw Chain → `vr_chainsaw_damage`
        - Grunt's Burst Rifle → `vr_gruntgun_damage`
        - Enforcer's Laser Rifle → `vr_enfrifle_damage`
        - — Thrown —
        - All Throws → `vr_weapon_throw_damage_mult`
        - — More —
        - → link `Damage and Knockback` to Damage and Knockback [cross-link]
        - → link `Weight and Damage` to Weight and Damage [cross-link]
      - **Enemy Weapons** [menu_vr 76] — 21 rows / 15 settings / 0 actions (vr_menu.cpp:706)
        - — Their Damage —
        - → link `Weapon Damage` to Weapon Damage [cross-link]
        - — Ogres' Chainsaws —
        - Fuel When Dropped → `vr_chainsaw_drop_fuel_min`
        - Chain Fuel Use → `vr_chainsaw_fuel_use`
        - Idle Fuel Use → `vr_chainsaw_idle_fuel_use`
        - Runs On When Let Go → `vr_chainsaw_drop_run`
        - **Chainsaw Engine** [menu_vr 82] — 13 rows / 10 settings / 0 actions (vr_menu.cpp:669)
          - — Exhaust Smoke —
          - Smoke → `vr_chainsaw_smoke`
          - Smoke Opacity → `vr_chainsaw_smoke_alpha`
          - — Shake —
          - In One Hand → `vr_chainsaw_shake`
          - In Two Hands → `vr_chainsaw_shake_2h`
          - On the Ground → `vr_chainsaw_shake_ground`
          - — Pulling the Cord —
          - Pull Shake → `vr_chainsaw_pull_shake`
          - Pull Shake Time → `vr_chainsaw_pull_shake_time`
          - Pull Smoke → `vr_chainsaw_pull_smoke`
          - Pull Sparks → `vr_chainsaw_pull_sparks`
          - Model Handle in the Hand → `vr_chainsaw_model_handle`
        - Blade Sinks In → `vr_chainsaw_overlap`
        - Start Chance → `vr_chainsaw_start_chance`
        - First Pulls Fail → `vr_chainsaw_fail_pulls_min`
        - Up To → `vr_chainsaw_fail_pulls_max`
        - Cord Pull Distance → `vr_chainsaw_pull_distance`
        - Cord Pull Speed → `vr_chainsaw_pull_speed`
        - — Grunts' Burst Rifles —
        - Burst Rifles → `vr_grunt_burst`
        - Grunts' Round Damage → `vr_grunt_burst_damage`
        - Rounds → `vr_gruntgun_ammo`
        - — Enforcers' Laser Rifles —
        - Laser Speed → `vr_enfrifle_speed`
        - Shots → `vr_enfrifle_ammo`
      - **Enemy Shoves** [menu_vr 81] — 9 rows / 9 settings / 0 actions (vr_menu.cpp:641)
        - Enemy Shoves → `vr_enemy_shove`
        - Too Close → `vr_enemy_shove_range`
        - Delay → `vr_enemy_shove_delay`
        - Cooldown → `vr_enemy_shove_cooldown`
        - Damage → `vr_enemy_shove_damage`
        - Push Distance → `vr_enemy_shove_distance`
        - Parry Push Reduction → `vr_enemy_shove_parry_reduction`
        - Enforcer Damage → `vr_enemy_shove_enforcer_damage`
        - Enforcer Push → `vr_enemy_shove_enforcer_distance`
      - **Bullet Time** [menu_vr 83] — 28 rows / 23 settings / 1 actions (vr_menu_recording.inc:132)
        - Bullet Time → `vr_bullettime_enabled`
        - [cmd] Start or Stop It Now → `vr_bullettime`
        - Time Scale → `vr_bullettime_scale`
        - Duration → `vr_bullettime_duration`
        - Recharge Time → `vr_bullettime_recharge`
        - Cooldown → `vr_bullettime_cooldown`
        - Least Meter to Start → `vr_bullettime_min`
        - Sandevistan: You at Full Speed → `vr_bullettime_sandevistan`
        - Your Missiles at Full Speed Too → `vr_sandevistan_missiles`
        - — Look —
        - Look's Strength → `vr_bullettime_fx`
        - Colour Drained → `vr_bullettime_fx_desat`
        - Darkened Edges → `vr_bullettime_fx_vignette`
        - — Wrist Tap —
        - Tap the Wrist → `vr_bullettime_tap`
        - Tap Zone Size → `vr_bullettime_tap_radius`
        - Tap Force → `vr_bullettime_tap_speed`
        - Tap Must Stop → `vr_bullettime_tap_stop`
        - Tap Window → `vr_bullettime_tap_window`
        - Tap While Holding → `vr_bullettime_tap_holding`
        - Tap During Two-Handed Holds → `vr_bullettime_tap_twohanded`
        - — Button —
        - Gadget Button → `vr_bullettime_button`
        - Button Size → `vr_bullettime_button_radius`
        - Fingertip Reach → `vr_bullettime_button_reach`
        - — Both —
        - Ignore Repeats For → `vr_bullettime_trigger_cooldown`
        - Haptic Tick → `vr_bullettime_haptic`
      - **Burning** [menu_vr 94] — 47 rows / 42 settings / 0 actions **LONG** (vr_menu.cpp:2415)
        - Burn Damage → `vr_burn_damage`
        - Burn Time → `vr_burn_time`
        - Flames Spread To → `vr_burn_flames`
        - Most Flames → `vr_burn_flames_max`
        - Spread Time → `vr_burn_spread`
        - Flame Size → `vr_burn_flame_size`
        - Spread Flames' Size → `vr_burn_flame_small`
        - Spread Flames' Lean → `vr_burn_flame_tilt`
        - — Corpses —
        - Corpses Burn → `vr_burn_corpses`
        - Corpse Burn Time → `vr_burn_corpse_time`
        - Corpse Burn Damage → `vr_burn_corpse_damage`
        - — What Sets Things on Fire —
        - Torch Touch → `vr_burn_touch`
        - Lava Nails → `vr_burn_lava_nails`
        - Nails Through a Flame → `vr_burn_nail_convert`
        - Flame's Reach for Nails → `vr_burn_nail_reach`
        - Nail Sizzle Volume → `vr_burn_nail_sound`
        - — Crates —
        - Crates Burn → `vr_burn_crates`
        - Crate Burn Time → `vr_burn_crate_time`
        - Burnt Crates Break → `vr_burn_crate_break`
        - Fire Spreads After → `vr_burn_crate_spread`
        - Touching Within → `vr_burn_crate_gap`
        - Crate Flames → `vr_burn_crate_flames`
        - Crate Flame Size → `vr_burn_crate_flame_size`
        - Burnt Crates' Pieces Charred → `vr_burn_crate_char`
        - Pieces Burn → `vr_burn_pieces`
        - Piece Burn Time → `vr_burn_piece_time`
        - Pieces Catch Within → `vr_burn_piece_gap`
        - Piece Flames → `vr_burn_piece_flames`
        - Piece Flame Size → `vr_burn_piece_flame_size`
        - — Torch Flame —
        - Swing Lean → `vr_walltorch_lean`
        - Flatten When Fast → `vr_walltorch_flatten`
        - Upside Down: Flame Size → `vr_walltorch_inv_size`
        - Upside Down: Brightness → `vr_walltorch_inv_light`
        - Upside Down: Burning Drips → `vr_walltorch_drips`
        - Upside Down: More Smoke → `vr_walltorch_inv_smoke`
        - Torch Smoke → `vr_walltorch_smoke`
        - Torch Smoke Opacity → `vr_walltorch_smoke_alpha`
        - — Your Own Torch —
        - Its Flame Burns You → `vr_burn_self`
        - Catch Fire After → `vr_burn_self_time`
        - Warning Buzz → `vr_burn_self_haptic`
        - Upside Down Burns Your Hand → `vr_burn_drop`
        - Drop After → `vr_burn_drop_time`
    - **Movement** [menu_vr 45] — 6 rows / 0 settings / 0 actions (vr_menu.cpp:4762)
      - **Locomotion** [menu_vr 19] — 20 rows / 16 settings / 0 actions (vr_menu_pages.inc:56)
        - Movement Mode → `vr_movement_mode`
        - Deadzone → `vr_deadzone`
        - Movement Speed → `cl_forwardspeed`
        - Default Speed → `cl_alwaysrun`
        - Running Mult. → `cl_movespeedkey`
        - — Turning —
        - Enable Joystick Turn → `vr_enable_joystick_turn`
        - Turn → `vr_snap_turn`
        - Turn Speed → `vr_turn_speed`
        - — Teleport —
        - Teleportation → `vr_teleport_enabled`
        - Teleport Range → `vr_teleport_range`
        - — Leaning —
        - Lean → `vr_lean_radius`
        - Lean Recentre → `vr_lean_recenter`
        - Lean Detection → `vr_lean_detect`
        - — Room Scale —
        - Roomscale Move Mult. → `vr_roomscale_move_mult`
        - Roomscale Jump → `vr_roomscale_jump`
        - Jump Threshold → `vr_roomscale_jump_threshold`
      - **Climbing** [menu_vr 54] — 28 rows / 24 settings / 1 actions (vr_menu_pages.inc:86)
        - Climbing → `vr_climb`
        - Lowest Ledge → `vr_climb_min_height`
        - Ledge Fling → `vr_climb_fling`
        - Blasts Knock You Off → `vr_climb_blast_letgo`
        - Grab Leniency → `vr_climb_leniency`
        - Mid-Air Leniency → `vr_climb_leniency_air`
        - Mid-Air Grab Window → `vr_climb_air_grab_time`
        - Touch Distance → `vr_climb_touch`
        - Reach Over the Top → `vr_climb_over_top`
        - Hands Slide Along Ledges → `vr_climb_slide`
        - Grab Sound → `vr_physsound_grab`
        - Mantle Grunt → `vr_climb_mantle_grunt`
        - Mantle Grunt Sound → `vr_climb_mantle_grunt_sound`
        - [cmd] Hear Mantle Grunt → `vr_climb_mantle_grunt_test`
        - — Climbing Stamina —
        - Climbing Stamina → `vr_climb_stamina`
        - Hanging Cost, One Hand → `vr_climb_stamina_rate`
        - Hanging Cost, Two Hands → `vr_climb_stamina_rate_2h`
        - Exhausted: Slip Time → `vr_climb_stamina_slip`
        - → link `Tired Arms (Stamina)` to Stamina [cross-link]
        - — Hand on a Hold (Looks Only) —
        - Hand on Hold: Towards You → `vr_climb_hand_out`
        - Hand on Hold: Up → `vr_climb_hand_up`
        - Hand on Hold: Sideways → `vr_climb_hand_side`
        - Hold Rotation Blend → `vr_climb_hand_turn_blend`
        - Hand on Hold: Pitch → `vr_climb_hand_pitch`
        - Hand on Hold: Yaw → `vr_climb_hand_yaw`
        - Hand on Hold: Roll → `vr_climb_hand_roll`
      - **Swimming** [menu_vr 20] — 28 rows / 24 settings / 1 actions (vr_menu_pages.inc:138)
        - Swimming → `vr_swim`
        - Stick in Shallow Water → `vr_swim_shallow_speed`
        - Stick Wading → `vr_swim_wade_speed`
        - Stick Swimming → `vr_swim_stick_speed`
        - Air Supply → `vr_air_supply`
        - — Strokes —
        - Stroke Strength → `vr_swim_stroke`
        - Speed Curve → `vr_swim_speed_exp`
        - Palm Matters → `vr_swim_palm`
        - Stroke Against Palm → `vr_swim_against_palm`
        - Palm Sharpness → `vr_swim_flat_exp`
        - Recovery Push → `vr_swim_recovery`
        - Push Along Palm → `vr_swim_palm_dir`
        - Slowest Push → `vr_swim_stroke_min`
        - — Stroke or Return? —
        - Intent Threshold → `vr_swim_power_threshold`
        - Intent Fade-In → `vr_swim_power_knee`
        - Whole Stroke Counts → `vr_swim_power_whole`
        - Stroke Memory → `vr_swim_intent_memory`
        - Return Damping → `vr_swim_reverse_damp`
        - Reverse Stroke Speed → `vr_swim_reverse_speed`
        - — Direction and Speed —
        - Swim Where You Look → `vr_swim_look`
        - Stroke Steering → `vr_swim_stroke_assist`
        - Stroke Pitch Offset → `vr_swim_stroke_pitch`
        - Glide → `vr_swim_glide`
        - Swim Top Speed → `vr_swim_max_speed`
        - [action] Reset Swimming to Defaults → resetSwimming()
      - **Grappling Hook** [menu_vr 13] — 52 rows / 45 settings / 0 actions **LONG** (vr_menu.cpp:4072)
        - Rope → `vr_grapple_rope`
        - Trigger Released → `vr_grapple_trigger_release`
        - Drop Grace → `vr_grapple_drop_grace`
        - Longest Rope → `vr_grapple_max_length`
        - Shoot the Hook Off → `vr_grapple_shootable`
        - Reel-In Button Time → `vr_grapple_quick_time`
        - Reel-In Button Speed → `vr_grapple_quick_speed`
        - Hook Size → `vr_grapple_hook_scale`
        - — Buttons on the Gun —
        - Back Button Along → `grappleButton(weapons::Key::WpnButtonX)`
        - Back Button Side → `grappleButton(weapons::Key::WpnButtonY)`
        - Back Button Up → `grappleButton(weapons::Key::WpnButtonZ)`
        - Front Button → `vr_grapple_front_button`
        - Front Button Along → `vr_grapple_front_button_x`
        - Front Button Side → `vr_grapple_front_button_y`
        - Front Button Up → `vr_grapple_front_button_z`
        - — Rope —
        - Physical Rope → `vr_grapple_rope_sim`
        - Rope Point Spacing → `vr_grapple_rope_spacing`
        - Rope Precision → `vr_grapple_rope_iterations`
        - Rope Thickness → `vr_grapple_rope_radius`
        - Rope Straight Out of the Hook → `vr_grapple_rope_tail`
        - Rope Depth in the Gun → `vr_grapple_rope_depth`
        - — What Hangs on the Rope —
        - Walking Shared → `vr_grapple_move_share`
        - Air Drag → `vr_grapple_load_drag`
        - Hanging Air Drag → `vr_grapple_hang_drag`
        - Top Speed → `vr_grapple_load_max_speed`
        - Slack When Detached → `vr_grapple_loose_slack`
        - — Reel —
        - Reel Speed → `vr_grapple_reel_speed`
        - Shortest Rope → `vr_grapple_min_length`
        - Loose Hook Goes In At → `vr_grapple_reel_home`
        - Unreel Speed → `vr_grapple_unreel_speed`
        - Unreel Slack → `vr_grapple_unreel_slack`
        - Unreel Button Only When Airborne → `vr_grapple_unreel_airborne`
        - — Props —
        - Prop Reel Speed → `vr_grapple_prop_speed`
        - Light Up To → `vr_grapple_prop_light`
        - Too Heavy From → `vr_grapple_prop_anchor`
        - Dragging: Light Up To → `vr_grapple_tow_light`
        - Dragging Speed → `vr_grapple_tow_speed`
        - — Monsters —
        - Small Up To → `vr_grapple_small_mass`
        - Huge From → `vr_grapple_huge_mass`
        - Small Reel Speed → `vr_grapple_small_speed`
        - Medium Reel Speed → `vr_grapple_medium_speed`
        - Stagger Small Monsters → `vr_grapple_stagger`
        - Stamina a Second → `vr_grapple_stamina`
        - — Feel —
        - Haptics → `vr_grapple_haptics`
        - Slack Rope Hangs → `vr_grapple_sag`
      - **Player Hitbox** [menu_vr 73] — 35 rows / 20 settings / 11 actions, built dynamically **LONG** (vr_menu.cpp:4221)
        - — Walls and Brush Models —
        - Width Against Walls → `vr_hull_width`
        - Method → `vr_hull_method`
        - Doors, Lifts and Walls Too → `vr_hull_brushmodels`
        - — Monsters, Players and Boxes —
        - Width Against Them → `vr_hull_ent_width`
        - Monsters → `vr_hull_monsters`
        - Other Players → `vr_hull_players`
        - Solid Boxes → `vr_hull_boxes`
        - Width Shots Hit → `vr_hull_hit_width`
        - Height Shots Hit: Your Head's → `vr_hull_hit_head`
        - Prop Push Radius → `vr_box3d_player_radius`
        - — Standing on Props —
        - Stand on Boxes → `vr_box3d_player_stand`
        - Steepest Face to Stand On → `vr_box3d_player_slope`
        - Your Weight on Them → `vr_box3d_player_mass`
        - Jump Push → `vr_box3d_player_jump_push`
        - Walking Into Them → `vr_box3d_player_shove`
        - Monsters Shove Them At → `vr_box3d_monster_push_speed`
        - Never Trapped by Them → `vr_box3d_player_unstick`
        - Never Through Them → `vr_box3d_player_hold`
        - Their Real Shape → `vr_box3d_player_shape`
        - Shots Meet Their Shape → `vr_box3d_shot_shape`
        - — Tests —
        - [cmd] Stand on a Box → `vr_physics_player onto misc_explobox`
        - [cmd] Rocket at the Nearest Box → `vr_debug_missiles 1; developer 1; vr_physics_fire 0`
        - [cmd] Nail at the Nearest Box → `vr_debug_missiles 1; vr_physics_fire 1`
        - [cmd] Grenade at the Nearest Box → `vr_debug_missiles 1; vr_physics_fire 2`
        - [cmd] Shot Clip Cost → `vr_physics_shotbench`
        - [cmd] Box Approach → `vr_physics_approach`
        - [cmd] Where You Stand → `vr_physics_player`
        - [cmd] Watch Falling Into Props → `vr_physics_inside 1`
        - [cmd] Hitbox Stats → `vr_hull_stats`
        - [cmd] Hitbox Approach → `vr_hull_approach`
        - [cmd] Random Walk (60 s) → `god; notarget; vr_hull_walktest 60`
      - **Monster Hitbox** [menu_vr 74] — 11 rows / 4 settings / 4 actions, built dynamically (vr_menu.cpp:4328)
        - label → `&cvar`
        - — Monsters' Widths (Prototype) —
        - Narrower Monsters → `vr_mhull`
        - Against Bodies Too → `vr_mhull_ents`
        - Ledges by Their Width → `vr_mhull_ledges`
        - [cmd] All Match Their Boxes → `vr_mhull_reset`
        - — By Class (Its Box) —
        - — Tests —
        - [cmd] Hitbox Stats → `vr_hull_stats`
        - [cmd] Monster Walk (60 s) → `god; notarget; vr_mhull_walktest 60`
        - [cmd] Monster Patrol (60 s) → `notarget; vr_mhull_walktest 60 1 1`
    - **Carrying and Throwing** [menu_vr 46] — 10 rows / 0 settings / 0 actions (vr_menu.cpp:4774)
      - **Carrying** [menu_vr 11] — 63 rows / 56 settings / 0 actions **LONG** (vr_menu.cpp:2178)
        - — Carrying Boxes —
        - Carry Ammo and Health → `vr_carry`
        - Take a Box → `vr_carry_take`
        - Grab Distance Bias → `vr_carry_grab_bias`
        - Explosive Boxes by the Fist → `vr_carry_grab_drawn`
        - Weapons by the Fist → `vr_weapon_grab_drawn`
        - Weapon Grab Slack → `vr_weapon_grab_slack`
        - Weapons by Their Hotspots → `vr_weapon_grab_hotspots`
        - Weapons Anywhere → `vr_weapon_grab_anywhere`
        - Anywhere: Away From Grips → `vr_weapon_grab_anywhere_min`
        - Other Hand Anywhere → `vr_weapon_anygrip_mode`
        - Drawn In the Hand → `vr_carry_local`
        - Two-Handed Carrying → `vr_carry_two_hands`
        - Two-Handed Grab Reach → `vr_carry_two_hands_reach`
        - Two-Handed Hand Drift → `vr_carry_two_hands_drift`
        - Two-Handed Stops at Walls → `vr_carry_two_hands_solid`
        - Fit to the Hand → `vr_held_surface_fit`
        - Fit Gap → `vr_held_fit_gap`
        - Held Things Collide → `vr_held_collide`
        - Collide Give → `vr_held_collide_max`
        - Empty Hand Against Held Things → `vr_hand_collide`
        - Empty Hand Stops Off It → `vr_hand_collide_props_margin`
        - Fingers Rest on Held Things → `vr_hand_collide_fingers`
        - Weapons Slide Along Walls → `vr_gun_wall_slide`
        - Weapon Wall Give → `vr_gun_wall_max`
        - Held Things Stop at Walls → `vr_held_collide_walls`
        - Wall Give → `vr_held_collide_wall_max`
        - Things Rest on Hands → `vr_model_collide_rest`
        - Held Things Stop at Monsters → `vr_held_collide_monsters`
        - Hands Push and Hold Things → `vr_box3d_hand_props`
        - Push Boxes With the Fist → `vr_box3d_hand_push_fist`
        - Weapons Push Things → `vr_box3d_weapon_push`
        - Heaviest Thing Held Up → `vr_box3d_hand_hold_mass`
        - Throw Grace → `vr_box3d_throw_grace`
        - Throw Grace From → `vr_box3d_throw_grace_speed`
        - Throw Grace for the Body → `vr_box3d_throw_grace_body`
        - Hand Push Mass → `vr_box3d_hand_mass`
        - Arm Behind Weapon → `vr_box3d_weapon_arm_mass`
        - Push Force → `vr_box3d_push_force`
        - Push Strength → `vr_carry_nudge`
        - Box Throw Speed → `vr_carry_throw_mult`
        - Box Punch Damage → `vr_carry_melee_mult`
        - Thrown Box Damage → `vr_carry_throw_damage`
        - → link `Flung Props (Throwing and Physics)` to Throwing and Physics [cross-link]
        - → link `Held Object Offsets (Held Prop)` to Held Object Offsets [cross-link]
        - → link `Held Object Weights (Held Prop)` to Held Object Weights [cross-link]
        - — Physics Sounds —
        - Physics Sounds → `vr_physsound`
        - Knocks → `vr_physsound_impact`
        - Quietest Knock → `vr_physsound_min_speed`
        - Loudest Knock From → `vr_physsound_full_speed`
        - Knock Spacing → `vr_physsound_interval`
        - Scrapes → `vr_physsound_scrape`
        - Quietest Scrape → `vr_physsound_scrape_min`
        - Loudest Scrape From → `vr_physsound_scrape_full`
        - Climbing Grab → `vr_physsound_grab`
        - — Explosive Boxes —
        - Physics Explosive Boxes → `vr_explobox_physics`
        - Blow Up On Impact → `vr_explobox_impact`
        - — Armour and Pickups —
        - Armour → `vr_armor_wear`
        - Armour Size → `vr_armor_scale`
        - Weapons and Keys → `vr_item_objects`
      - **Throwing and Physics** [menu_vr 10] — 78 rows / 72 settings / 0 actions **LONG** (vr_menu.cpp:1980)
        - Throw Speed → `vr_weapon_throw_velocity_mult`
        - Two-Hand Throw Speed → `vr_2h_throw_velocity_mult`
        - Throw Gravity → `vr_throw_gravity`
        - Velocity Window → `vr_throw_window`
        - Direction Lookback → `vr_throw_dir_lookback`
        - Lever Arm → `vr_throw_lever_arm`
        - Release Pitch → `vr_throw_pitch`
        - Analog Release → `vr_throw_release`
        - Spin From Controller Turn → `vr_throw_spin_from_pose`
        - Controller Spin Frame → `vr_angvel_frame`
        - Max Speed Gain → `vr_throw_gain_max`
        - — Throws by Weight —
        - Throws by Weight → `vr_throw_mass_model`
        - Light Things' Top Speed → `vr_throw_max_speed`
        - Light Up To → `vr_throw_mass_light`
        - Heavy Falloff → `vr_throw_mass_exp`
        - Soft Limit From → `vr_throw_mass_knee`
        - Two-Hand Strength → `vr_throw_2h_strength`
        - Full Wrist Flick Up To → `vr_throw_flick_mass`
        - Wrist to Controller → `vr_throw_wrist_dist`
        - Heavy Spin Falloff → `vr_throw_spin_mass_exp`
        - Aim Assist → `vr_throw_assist`
        - Assist Cone → `vr_throw_assist_cone`
        - Assist Strength → `vr_throw_assist_strength`
        - — Physics —
        - Bounciness → `vr_throw_restitution`
        - Friction → `vr_throw_friction`
        - Max Spin → `vr_throw_spin_max`
        - Spin Drag → `vr_throw_spin_drag`
        - Spin Alignment → `vr_throw_spin_align`
        - Hitbox → `vr_throw_hitbox`
        - Hit Min Speed → `vr_throw_hit_min_speed`
        - Heavy Hits Scale Below → `vr_throw_hit_top`
        - Damage Eases In Over → `vr_throw_hit_ramp`
        - Damage at Hit Min Speed → `vr_throw_hit_ramp_floor`
        - Two-Handed Throws: No Blows → `vr_throw_2h_nomelee`
        - Two-Handed Throws: Spared → `vr_throw_2h_melee_immune`
        - Gibs Let Go: Other Hand Spares → `vr_gib_letgo_spare`
        - Thrown Gibs Burst on You → `vr_gib_burst_on_thrower`
        - Your Throws Spare You For → `vr_throw_self_grace`
        - — Thrown Axes —
        - Axes Stick → `vr_axestick`
        - Axes Stick in Explosive Boxes → `vr_axestick_metal`
        - Bleeding → `vr_axestick_bleed`
        - Blade Leniency → `vr_axestick_leniency`
        - Stick Speed → `vr_axestick_speed`
        - Stick Angle → `vr_axestick_angle`
        - Stick Incidence → `vr_axestick_incidence`
        - Stick Depth → `vr_axestick_depth`
        - Force Grab Tug → `vr_axestick_tug`
        - — Flung Props —
        - Flung Props Hurt → `vr_prop_impact_damage`
        - Flung Props Hurt Players → `vr_prop_impact_players`
        - Fresh Gibs Harmless For → `vr_gib_spawn_harmless`
        - Monster Drops Harmless For → `vr_prop_drop_grace`
        - Monster Drops Hurt Only Falling → `vr_prop_drop_falls_only`
        - Monster Drops Pass Through Bodies → `vr_prop_drop_pass_inside`
        - Least Speed → `vr_prop_impact_min_speed`
        - Damage → `vr_prop_impact_mult`
        - Most Speed Multiplier → `vr_prop_impact_speed_max`
        - Weight Curve → `vr_prop_impact_weight_curve`
        - Reference Mass → `vr_prop_impact_weight_ref`
        - Most Weight Multiplier → `vr_prop_impact_weight_max`
        - Least Mass → `vr_prop_impact_min_mass`
        - — Shots Push Props —
        - Shot Push → `vr_shot_push`
        - Pellet Push → `vr_shot_push_pellet`
        - Nail Push → `vr_shot_push_nail`
        - Super Nail Push → `vr_shot_push_supernail`
        - Lightning Push → `vr_shot_push_lightning`
        - Shot Push Top Speed → `vr_shot_push_speed`
        - — Wall Buttons —
        - Weapons Press Buttons → `vr_button_weapon`
        - Weapon Press Reach → `vr_button_weapon_reach`
        - Held Props Press Buttons → `vr_button_prop`
        - Prop Press Reach → `vr_button_prop_reach`
        - Thrown Things Press Buttons → `vr_button_throw`
        - Thrown Press Min Speed → `vr_button_throw_speed`
      - **Force Grab** [menu_vr 12] — 17 rows / 17 settings / 0 actions (vr_menu.cpp:4033)
        - Force Grab → `vr_forcegrab_mode`
        - Distance → `vr_forcegrab_distance`
        - Aim Cone → `vr_forcegrab_cone`
        - Flick Speed → `vr_forcegrab_flick_speed`
        - Flick Turn → `vr_forcegrab_flick_turn`
        - Flight Time → `vr_forcegrab_time`
        - Flight Speed → `vr_forcegrab_speed`
        - Arc Height → `vr_forcegrab_arc`
        - Catch Radius → `vr_forcegrab_catch_radius`
        - Catch Early → `vr_forcegrab_catch_early`
        - Catch Late → `vr_forcegrab_catch_late`
        - Catch Blend Time → `vr_forcegrab_catch_blend`
        - Pointing Particles → `vr_forcegrab_eligible_particles`
        - Pointing Haptics → `vr_forcegrab_eligible_haptics`
        - Outline → `vr_forcegrab_outline`
        - Effects → `vr_forcegrab_fx`
        - Ammo/Health Box Size → `vr_forcegrabbable_box_scale`
      - **Wall Torches** [menu_vr 55] — 12 rows / 11 settings / 0 actions (vr_menu.cpp:2375)
        - Take Torches Off Walls → `vr_walltorch`
        - Pull to Take → `vr_walltorch_pull`
        - Grab Reach → `vr_walltorch_reach`
        - Grab Window → `vr_walltorch_grab_time`
        - Force Grab Torches → `vr_walltorch_forcegrab`
        - Blows Before It Dies → `vr_walltorch_hits`
        - Dying Time → `vr_walltorch_die_time`
        - Blow Damage → `vr_walltorch_damage`
        - → link `Burning` to Burning [cross-link]
        - Light Again → `vr_walltorch_relight`
        - Flame Size → `vr_walltorch_flame`
        - Taken Torch Casts Shadows → `vr_walltorch_shadows`
      - **Rocks and Bricks** [menu_vr 56] — 12 rows / 12 settings / 0 actions (vr_menu.cpp:2523)
        - Rocks and Bricks → `vr_debris`
        - Rocks → `vr_debris_rocks`
        - Bricks → `vr_debris_bricks`
        - Chance → `vr_debris_chance`
        - In Corners → `vr_debris_corner`
        - In the Dark → `vr_debris_dark`
        - Most Together → `vr_debris_cluster`
        - Most in a Map → `vr_debris_max`
        - Most in an Area → `vr_debris_area_max`
        - Spacing → `vr_debris_spacing`
        - Size Variation → `vr_debris_size`
        - Layout → `vr_debris_seed`
      - **Crates** [menu_vr 78] — 26 rows / 22 settings / 0 actions (vr_menu.cpp:2554)
        - — Where They Lie —
        - Crates → `vr_crates`
        - Density → `vr_crates_chance`
        - In Corners → `vr_crates_corner`
        - Most in a Map → `vr_crates_max`
        - Spacing → `vr_crates_spacing`
        - Stacked → `vr_crates_stack`
        - Large Ones → `vr_crates_large`
        - Room in Front → `vr_crates_clearance`
        - Away From Things → `vr_crates_margin`
        - Layout → `vr_crates_seed`
        - — Breaking —
        - Health → `vr_crate_health`
        - Breaks On Impact → `vr_crate_impact`
        - Pieces → `vr_crate_pieces`
        - Most Pieces → `vr_crate_piece_max`
        - Pieces Last → `vr_crate_piece_time`
        - — What They Hold —
        - Ammo → `vr_crate_ammo`
        - Health Box → `vr_crate_health_box`
        - Pop Out → `vr_crate_item_pop`
        - Crowbar on Crates → `vr_crate_crowbar`
        - Most Crowbars → `vr_crate_crowbar_max`
        - — Hiding —
        - Crates Hide You → `vr_crate_sight`
        - Held Crate Shields You → `vr_crate_shield`
      - **Gibs and Corpses** [menu_vr 57] — 21 rows / 17 settings / 0 actions (vr_menu.cpp:2629)
        - Gibs and Heads → `vr_grab_gibs`
        - Thrown Gib Damage → `vr_gib_throw_damage`
        - Destroy Gibs → `vr_gib_destroy`
        - Gib Health → `vr_gib_health`
        - Gib Splat Speed → `vr_gib_splat_speed`
        - Gib Corpses → `vr_corpse_gib`
        - Corpse Health → `vr_corpse_health_mult`
        - Never Gib Corpses → `vr_corpse_nogib`
        - **Gibs and Corpses - Corpse Damage and Health** [menu_vr 120] — 30 rows / 28 settings / 0 actions (vr_menu.cpp:2683)
          - Corpse Health → `vr_corpse_health_mult`
          - Never Gib Corpses → `vr_corpse_nogib`
          - — Damage to Corpses, by Weapon —
          - Shotguns and Guns → `vr_corpse_dmg_shots`
          - Nails → `vr_corpse_dmg_nails`
          - Explosions → `vr_corpse_dmg_explosions`
          - Lightning → `vr_corpse_dmg_lightning`
          - Blunt Melee → `vr_corpse_dmg_blunt`
          - Fists → `vr_corpse_dmg_fists`
          - Bladed Melee → `vr_corpse_dmg_blades`
          - Chainsaw → `vr_corpse_dmg_chainsaw`
          - Thrown Props → `vr_corpse_dmg_props`
          - Fire → `vr_corpse_dmg_fire`
          - Bashes and Shoves → `vr_corpse_dmg_bash`
          - Other → `vr_corpse_dmg_other`
          - — Corpse Health, by Monster —
          - Grunt → `vr_corpse_health_grunt`
          - Enforcer → `vr_corpse_health_enforcer`
          - Rottweiler → `vr_corpse_health_dog`
          - Fiend → `vr_corpse_health_fiend`
          - Ogre → `vr_corpse_health_ogre`
          - Knight → `vr_corpse_health_knight`
          - Hell Knight → `vr_corpse_health_hellknight`
          - Vore → `vr_corpse_health_vore`
          - Shambler → `vr_corpse_health_shambler`
          - Scrag → `vr_corpse_health_scrag`
          - Rotfish → `vr_corpse_health_fish`
          - Gremlin (Hipnotic) → `vr_corpse_health_gremlin`
          - Centroid (Rogue) → `vr_corpse_health_scourge`
          - Eel (Rogue) → `vr_corpse_health_eel`
        - — Corpse Collision —
        - Corpses → `vr_corpse_collide`
        - Pushable Corpse Mass → `vr_corpse_collide_mass`
        - Pushable Corpse Friction → `vr_corpse_collide_friction`
        - Props Meet Corpses → `vr_corpse_collide_props`
        - Thrown Things Meet Corpses → `vr_corpse_collide_thrown`
        - Held Things Meet Corpses → `vr_corpse_collide_held`
        - You and Corpses → `vr_corpse_collide_player`
        - Monsters Step Over Corpses → `vr_corpse_collide_monsters`
        - — Ragdolls (Experimental) —
        - Ragdolls → `vr_ragdoll`
        - **Gibs and Corpses - Ragdolls** [menu_vr 117] — 34 rows / 22 settings / 0 actions **LONG** (vr_menu.cpp:2750)
          - Ragdolls → `vr_ragdoll`
          - Go Limp At → `vr_ragdoll_start`
          - Most Ragdolls → `vr_ragdoll_max`
          - Ragdolls Meet Each Other → `vr_ragdoll_collide_each`
          - — Physics (All Monsters) —
          - Mass → `vr_ragdoll_mass`
          - Friction → `vr_ragdoll_friction`
          - Joint Friction → `vr_ragdoll_joint_friction`
          - Joint Stiffness → `vr_ragdoll_joint_stiffness`
          - Joint Limits → `vr_ragdoll_limits`
          - Limb Damping → `vr_ragdoll_damping`
          - Blast Throw → `vr_ragdoll_blast`
          - Death Motion Kept → `vr_ragdoll_inherit`
          - — Each Monster's Own —
          - **Ragdolls - Grunt** [menu_vr 118] — 10 rows / 9 settings / 1 actions (vr_menu.cpp:2827)
            - Go Limp At → `vr_ragdoll_army_start`
            - Mass → `vr_ragdoll_army_mass`
            - Friction → `vr_ragdoll_army_friction`
            - Joint Friction → `vr_ragdoll_army_joint_friction`
            - Joint Stiffness → `vr_ragdoll_army_joint_stiffness`
            - Joint Limits → `vr_ragdoll_army_limits`
            - Limb Damping → `vr_ragdoll_army_damping`
            - Blast Throw → `vr_ragdoll_army_blast`
            - Death Motion Kept → `vr_ragdoll_army_inherit`
            - [cmd] All Global → `vr_ragdoll_army_start -1; vr_ragdoll_army_mass -1; vr_ragdoll_army_friction -1; `
          - **Ragdolls - Knight** [menu_vr 119] — 10 rows / 9 settings / 1 actions (vr_menu.cpp:2856)
            - Go Limp At → `vr_ragdoll_knight_start`
            - Mass → `vr_ragdoll_knight_mass`
            - Friction → `vr_ragdoll_knight_friction`
            - Joint Friction → `vr_ragdoll_knight_joint_friction`
            - Joint Stiffness → `vr_ragdoll_knight_joint_stiffness`
            - Joint Limits → `vr_ragdoll_knight_limits`
            - Limb Damping → `vr_ragdoll_knight_damping`
            - Blast Throw → `vr_ragdoll_knight_blast`
            - Death Motion Kept → `vr_ragdoll_knight_inherit`
            - [cmd] All Global → `vr_ragdoll_knight_start -1; vr_ragdoll_knight_mass -1; vr_ragdoll_knight_friction -1; `
          - **Ragdolls - Ogre** [menu_vr 121] — 10 rows / 9 settings / 1 actions (vr_menu.cpp:2885)
            - Go Limp At → `vr_ragdoll_ogre_start`
            - Mass → `vr_ragdoll_ogre_mass`
            - Friction → `vr_ragdoll_ogre_friction`
            - Joint Friction → `vr_ragdoll_ogre_joint_friction`
            - Joint Stiffness → `vr_ragdoll_ogre_joint_stiffness`
            - Joint Limits → `vr_ragdoll_ogre_limits`
            - Limb Damping → `vr_ragdoll_ogre_damping`
            - Blast Throw → `vr_ragdoll_ogre_blast`
            - Death Motion Kept → `vr_ragdoll_ogre_inherit`
            - [cmd] All Global → `vr_ragdoll_ogre_start -1; vr_ragdoll_ogre_mass -1; vr_ragdoll_ogre_friction -1; `
          - **Ragdolls - Enforcer** [menu_vr 122] — 10 rows / 9 settings / 1 actions (vr_menu.cpp:2914)
            - Go Limp At → `vr_ragdoll_enforcer_start`
            - Mass → `vr_ragdoll_enforcer_mass`
            - Friction → `vr_ragdoll_enforcer_friction`
            - Joint Friction → `vr_ragdoll_enforcer_joint_friction`
            - Joint Stiffness → `vr_ragdoll_enforcer_joint_stiffness`
            - Joint Limits → `vr_ragdoll_enforcer_limits`
            - Limb Damping → `vr_ragdoll_enforcer_damping`
            - Blast Throw → `vr_ragdoll_enforcer_blast`
            - Death Motion Kept → `vr_ragdoll_enforcer_inherit`
            - [cmd] All Global → `vr_ragdoll_enforcer_start -1; vr_ragdoll_enforcer_mass -1; vr_ragdoll_enforcer_friction -1`
          - **Ragdolls - Death Knight** [menu_vr 123] — 10 rows / 9 settings / 1 actions (vr_menu.cpp:2943)
            - Go Limp At → `vr_ragdoll_hknight_start`
            - Mass → `vr_ragdoll_hknight_mass`
            - Friction → `vr_ragdoll_hknight_friction`
            - Joint Friction → `vr_ragdoll_hknight_joint_friction`
            - Joint Stiffness → `vr_ragdoll_hknight_joint_stiffness`
            - Joint Limits → `vr_ragdoll_hknight_limits`
            - Limb Damping → `vr_ragdoll_hknight_damping`
            - Blast Throw → `vr_ragdoll_hknight_blast`
            - Death Motion Kept → `vr_ragdoll_hknight_inherit`
            - [cmd] All Global → `vr_ragdoll_hknight_start -1; vr_ragdoll_hknight_mass -1; vr_ragdoll_hknight_friction -1; `
          - **Ragdolls - Rottweiler** [menu_vr 124] — 10 rows / 9 settings / 1 actions (vr_menu.cpp:2972)
            - Go Limp At → `vr_ragdoll_dog_start`
            - Mass → `vr_ragdoll_dog_mass`
            - Friction → `vr_ragdoll_dog_friction`
            - Joint Friction → `vr_ragdoll_dog_joint_friction`
            - Joint Stiffness → `vr_ragdoll_dog_joint_stiffness`
            - Joint Limits → `vr_ragdoll_dog_limits`
            - Limb Damping → `vr_ragdoll_dog_damping`
            - Blast Throw → `vr_ragdoll_dog_blast`
            - Death Motion Kept → `vr_ragdoll_dog_inherit`
            - [cmd] All Global → `vr_ragdoll_dog_start -1; vr_ragdoll_dog_mass -1; vr_ragdoll_dog_friction -1; `
          - **Ragdolls - Scrag** [menu_vr 125] — 10 rows / 9 settings / 1 actions (vr_menu.cpp:3001)
            - Go Limp At → `vr_ragdoll_wizard_start`
            - Mass → `vr_ragdoll_wizard_mass`
            - Friction → `vr_ragdoll_wizard_friction`
            - Joint Friction → `vr_ragdoll_wizard_joint_friction`
            - Joint Stiffness → `vr_ragdoll_wizard_joint_stiffness`
            - Joint Limits → `vr_ragdoll_wizard_limits`
            - Limb Damping → `vr_ragdoll_wizard_damping`
            - Blast Throw → `vr_ragdoll_wizard_blast`
            - Death Motion Kept → `vr_ragdoll_wizard_inherit`
            - [cmd] All Global → `vr_ragdoll_wizard_start -1; vr_ragdoll_wizard_mass -1; vr_ragdoll_wizard_friction -1; `
          - — Taking Them —
          - Grab Ragdolls → `vr_ragdoll_grab`
          - Grip Strength → `vr_ragdoll_grab_force`
          - Grip Reach → `vr_ragdoll_grab_reach`
          - Hand on the Limb → `vr_ragdoll_grab_fit`
          - Hand Follows Limb → `vr_ragdoll_hand_stick`
          - Hand Turns with Limb → `vr_ragdoll_hand_turn`
          - Throw → `vr_ragdoll_throw`
          - — Blood —
          - Blood Trails → `vr_ragdoll_blood`
          - — Drawing —
          - Smooth Motion → `vr_ragdoll_smooth`
          - Held Limbs Follow the Hand → `vr_ragdoll_held_local`
      - — What You Hold —
      - **Held Object Offsets** [menu_vr 41] — 36 rows / 28 settings / 2 actions, built dynamically **LONG** (vr_menu_props.inc:443)
        - label → `props::cvar(slot, key)`
        - → link `Held Object Weights` to Held Object Weights [cross-link]
        - Size → `Key::Size`
        - — Grip —
        - Grip → `props::cvar(slot, Key::GripMode)`
        - X (forward) → `Key::GripX`
        - Y (left) → `Key::GripY`
        - Z (up) → `Key::GripZ`
        - Pitch (up) → `Key::GripPitch`
        - Yaw (left) → `Key::GripYaw`
        - Roll (right) → `Key::GripRoll`
        - Handle From → `Key::HandleFrom`
        - Handle To → `Key::HandleTo`
        - Handle Tilt → `Key::HandleTilt`
        - — Fingers —
        - Fingers → `props::cvar(slot, Key::FingerManual)`
        - Thumb Curl → `Key::FingerCurlThumb`
        - Index Curl → `Key::FingerCurlIndex`
        - Middle Curl → `Key::FingerCurlMiddle`
        - Ring Curl → `Key::FingerCurlRing`
        - Little Curl → `Key::FingerCurlPinky`
        - Thumb Across → `Key::FingerThumbAcross`
        - Overlap → `Key::Overlap`
        - — Carrying —
        - Two Hands → ``
        - Force Grab → `props::cvar(slot, Key::ForceGrab)`
        - — Melee —
        - Tip X → `Key::TipX`
        - Tip Y → `Key::TipY`
        - Tip Z → `Key::TipZ`
        - Butt X → `Key::ButtX`
        - Butt Y → `Key::ButtY`
        - Butt Z → `Key::ButtZ`
        - — This Prop —
        - [action] Print Changes to Console → heldObjectPrint()
        - [action] Reset This Prop → heldObjectReset()
      - **Held Object Weights** [menu_vr 43] — 22 rows / 11 settings / 2 actions, built dynamically (vr_menu_props.inc:531)
        - label → `props::cvar(slot, key)`
        - [infoLine] heldObjectMassReadout
        - — Weight —
        - Mass → `Key::Mass`
        - Estimated Mass x → `Key::MassScale`
        - Inertia → `Key::Inertia`
        - Centre of Mass X → `Key::ComX`
        - Centre of Mass Y → `Key::ComY`
        - Centre of Mass Z → `Key::ComZ`
        - Throw → `Key::Throw`
        - — Spring (times the Aiming page's) —
        - — Damage —
        - [infoLine] heldObjectDamageReadout
        - [infoLine] heldObjectDamageReadout
        - Melee Damage → `Key::MeleeDamage`
        - Throw Damage → `Key::ThrowDamage`
        - — Thrown —
        - Spin in the Air → `Key::SpinAlign`
        - — This Prop —
        - → link `Held Object Offsets` to Held Object Offsets [cross-link]
        - [action] Print Changes to Console → heldObjectWeightsPrint()
        - [action] Reset This Prop → heldObjectWeightsReset()
    - **World** [menu_vr 3] — 13 rows / 8 settings / 0 actions (vr_menu.cpp:616)
      - — Monsters —
      - Enemies Hurt by Liquids → `vr_enemy_liquid_damage`
      - Ogres Aim Grenades Up and Down → `vr_ogre_aim_height`
      - — Enemy Weapons —
      - → link `Enemy Weapons` to Enemy Weapons [cross-link]
      - — Weapon Drops —
      - Enemy Weapon Drops → `vr_enemy_drops`
      - Enemy Drops Chance → `vr_enemy_drops_chance_mult`
      - Ammo Box Weapon Drops → `vr_ammobox_drops`
      - Ammo Box Drops Chance → `vr_ammobox_drops_chance_mult`
      - — Feel —
      - Explosion Rumble → `vr_explosion_rumble`
      - Low Health Heartbeat → `vr_heartbeat`
    - **Gore** [menu_vr 9] — 71 rows / 61 settings / 0 actions **LONG** (vr_menu.cpp:1756)
      - Gore → `vr_gore`
      - — Hits, Gibs and Corpses —
      - Blood Sprays → `vr_gore_spray`
      - Splat Size → `vr_gore_size`
      - Blood Pools → `vr_gore_pools`
      - Dripping → `vr_gore_drips`
      - Gibs Stick → `vr_gore_stick`
      - Thrown Gibs Stick → `vr_gore_stick_thrown`
      - Speed to Stick → `vr_gore_stick_speed`
      - Flies on Heads → `vr_head_flies`
      - Gib Speed: Melee → `vr_gib_speed_melee`
      - Gib Speed: Light Weapons → `vr_gib_speed_light`
      - Gib Speed: Explosives → `vr_gib_speed_heavy`
      - **Small Gibs** [menu_vr 93] — 52 rows / 47 settings / 0 actions **LONG** (vr_menu.cpp:1678)
        - Small Gibs → `vr_smallgibs`
        - From You Too → `vr_smallgibs_player`
        - — When —
        - Least Damage → `vr_smallgibs_min_damage`
        - Damage for a Sure One → `vr_smallgibs_full_damage`
        - Chance Curve → `vr_smallgibs_curve`
        - One More Each → `vr_smallgibs_damage_per_gib`
        - Most From a Hit → `vr_smallgibs_per_hit`
        - — Chance by Weapon —
        - Shotguns → `vr_smallgibs_shots`
        - Nails → `vr_smallgibs_nails`
        - Blades → `vr_smallgibs_blades`
        - Blunt Blows → `vr_smallgibs_blunt`
        - Props → `vr_smallgibs_props`
        - Explosions → `vr_smallgibs_explosions`
        - Everything Else → `vr_smallgibs_other`
        - Chainsaw: One Each → `vr_smallgibs_saw_interval`
        - With a Gibbing → `vr_smallgibs_gibbing`
        - A Large Gib Bursts Into → `vr_smallgibs_burst`
        - — Flight and Size —
        - Speed → `vr_smallgibs_speed`
        - Up → `vr_smallgibs_up`
        - Pass Through the Body → `vr_smallgibs_grace`
        - Not Pushed Out of Bodies → `vr_smallgibs_pass_inside`
        - Not Batted Away For → `vr_smallgibs_blow_grace`
        - Burst by Blows Meanwhile → `vr_smallgibs_blow_burst`
        - Smallest → `vr_smallgibs_size_min`
        - Largest → `vr_smallgibs_size_max`
        - Mass → `vr_smallgibs_mass`
        - — Flight by Situation —
        - Melee: Speed → `vr_smallgibs_speed_melee`
        - Melee: Up → `vr_smallgibs_up_melee`
        - Chainsaw: Speed → `vr_smallgibs_speed_saw`
        - Chainsaw: Up → `vr_smallgibs_up_saw`
        - Guns: Speed → `vr_smallgibs_speed_guns`
        - Guns: Up → `vr_smallgibs_up_guns`
        - Explosions: Speed → `vr_smallgibs_speed_explosions`
        - Explosions: Up → `vr_smallgibs_up_explosions`
        - Thrown: Speed → `vr_smallgibs_speed_thrown`
        - Thrown: Up → `vr_smallgibs_up_thrown`
        - Everything Else: Speed → `vr_smallgibs_speed_other`
        - Everything Else: Up → `vr_smallgibs_up_other`
        - Gibbing a Live One: Speed → `vr_smallgibs_speed_gibbing`
        - Gibbing a Live One: Up → `vr_smallgibs_up_gibbing`
        - Gibbing a Corpse: Speed → `vr_smallgibs_speed_corpse`
        - Gibbing a Corpse: Up → `vr_smallgibs_up_corpse`
        - A Large Gib Bursts: Speed → `vr_smallgibs_speed_burst`
        - A Large Gib Bursts: Up → `vr_smallgibs_up_burst`
        - — How Many and How Long —
        - Most Lying About → `vr_smallgibs_max`
        - Last → `vr_smallgibs_time`
        - Can Be Destroyed → `vr_smallgibs_destroy`
      - — Wounds on Models —
      - Dynamic Wounds → `vr_wounds`
      - Burns → `vr_wounds_burns`
      - Wet from Liquids → `vr_wounds_wet`
      - Models Kept → `vr_wounds_pool`
      - Your Wounds' Detail → `vr_wounds_own_res`
      - Your Burns' Relief → `vr_wounds_bump_burns`
      - Your Wounds' Depth → `vr_wounds_bump_blood`
      - Blood Opacity → `vr_wounds_blood_alpha`
      - — Your Wounds —
      - Arm Drip Rate → `vr_body_blood`
      - Drop Size → `vr_body_blood_amount`
      - Drips Round Feet → `vr_body_blood_floor`
      - Drops Mark Floor → `vr_body_blood_marks`
      - Floor Mark Size → `vr_body_blood_mark_size`
      - — Bloody Hands and Washing —
      - Gib Blood on Hands → `vr_gore_hands`
      - Water Washes Blood → `vr_gore_wash`
      - Wash Time → `vr_gore_wash_time`
      - Wounds Re-open → `vr_gore_reopen`
      - Re-open Delay → `vr_gore_reopen_delay`
      - Re-open Spread → `vr_gore_reopen_time`
      - — Blood on You and Your Gear —
      - Blood Spatter → `vr_gore_spatter`
      - From Your Blows → `vr_gore_spatter_melee`
      - From the Chainsaw → `vr_gore_spatter_saw`
      - From Close Shots → `vr_gore_spatter_shots`
      - Close Shot Range → `vr_gore_spatter_range`
      - Gibs Striking You → `vr_gore_spatter_gibs`
      - Blood on Weapons and Props → `vr_gore_gear`
      - Clean Weapon Skins → `vr_gore_clean_skins`
      - Holstered Weapons Too → `vr_gore_gear_holstered`
      - Things Lying Near → `vr_gore_gear_nearby`
      - Gibbed Monsters' Drops → `vr_gore_gear_drops`
      - Blood over Your Arms → `vr_gore_spread`
      - — Blood Mist —
      - Mist Amount → `vr_gore_mist`
      - Mist Size → `vr_gore_mist_size`
      - Mist Opacity → `vr_gore_mist_alpha`
      - Mist Lifetime → `vr_gore_mist_life`
      - Mist Spreading → `vr_gore_mist_grow`
      - Mist Drift → `vr_gore_mist_speed`
      - Mist Rise → `vr_gore_mist_rise`
      - Mist Darkness → `vr_gore_mist_dark`
      - Mist Clear of Eyes → `vr_gore_mist_near`
      - — Dying Bodies —
      - Hit While Dying → `vr_corpse_dying`
      - — Training Dummy —
      - Dummy Bleeds → `vr_dummy_gore`
      - Dummy Gibs → `vr_dummy_gib`
      - Dummy Stands Again → `vr_dummy_gib_respawn`
      - — Marks —
      - Decals → `vr_decals`
      - Max Decals → `vr_decal_max`
      - Decal Lifetime → `vr_decal_life`
      - Gib Blood → `vr_gib_blood`
      - Gib Blood Trail → `vr_gib_blood_trail`
    - — Body and Weapons —
    - **Body** [menu_vr 14] — 39 rows / 32 settings / 0 actions **LONG** (vr_menu.cpp:1180)
      - **Body - Arms and Pauldrons** [menu_vr 15] — 32 rows / 24 settings / 1 actions, built dynamically **LONG** (vr_menu.cpp:1285)
        - → link `Body Calibration` to Body Calibration [cross-link]
        - [infoLine] armsMeasured
        - — Arms —
        - Upper Arm → `vr_body_tweak_upper_arm`
        - Forearm → `vr_body_tweak_forearm`
        - [info] armLengthUnused
        - Arm Length → `vr_body_arm_length`
        - Arm Stretch → `vr_body_arm_stretch`
        - Shoulder Reach → `vr_body_shoulder_reach`
        - — Shoulders —
        - Shoulders Back → `vr_body_tweak_shoulders_back`
        - Shoulders Higher → `vr_body_tweak_shoulders_up`
        - Shoulders Wider → `vr_body_tweak_shoulders_out`
        - Shoulder Rise → `vr_body_tweak_shoulder_rise`
        - Shoulder Swing → `vr_body_tweak_shoulder_swing`
        - [action] Reset Tweaks → armsResetTweaks()
        - — Elbows and Wrists (not measured) —
        - Forearm Twist → `vr_body_forearm_twist`
        - Wrist Limits → `vr_body_wrist_limits`
        - Elbow Stays Clear → `vr_body_elbow_lift`
        - Elbows Spread → `vr_body_elbow_spread`
        - Elbow Out → `vr_body_elbow_out`
        - Elbow Back → `vr_body_elbow_back`
        - Elbow From Hand → `vr_body_elbow_hand`
        - — Pauldrons —
        - Pauldrons → `vr_body_pauldrons`
        - Pauldron Style → `vr_body_pauldron_style`
        - Pauldron Size → `vr_body_pauldron_size`
        - Pauldron Follows Arm → `vr_body_pauldron_follow`
        - Pauldron Forward → `vr_body_pauldron_forward`
        - Pauldron Up → `vr_body_pauldron_up`
        - Pauldron Out → `vr_body_pauldron_out`
      - **Body Calibration** [menu_vr 16] — 14 rows / 2 settings / 6 actions, built dynamically (vr_menu.cpp:1421)
        - [infoLine] bodycalIntro
        - [infoLine] bodycalIntro
        - [infoLine] bodycalIntro
        - Position → `vr_bodycal_seated`
        - [action] Apply → bodycalApply()
        - [action] Cancel → bodycalCancel()
        - [action] bodycal::showingNew() → ()
        - Body in Front → `vr_bodycal_preview`
        - [action] bodycal::partial() → ()
        - [action] Start Over → bodycalRestart()
        - [action] Undo → bodycalUndo()
        - [infoLine] bodycalLine
        - — Poses —
        - [row] bodycal::stepRow
      - **Player Calibration** [menu_vr 18] — 2 rows / 2 settings / 0 actions (vr_menu_pages.inc:361)
        - World Scale → `vr_world_scale`
        - Floor Offset → `vr_floor_offset`
      - — Body —
      - Body → `vr_body_mode`
      - Build → `vr_body_build`
      - Walking Legs → `vr_body_walk`
      - Step Rate → `vr_body_step_rate`
      - Turn Before Stepping → `vr_body_turn_step`
      - Wading Heaviness → `vr_body_wade`
      - Swimming Kicks → `vr_body_swim_kick`
      - Swimming Kick Rate → `vr_body_swim_kick_rate`
      - Show Armour and Wounds → `vr_body_state`
      - Wounds Drip Blood → `vr_body_blood`
      - Show Powerups → `vr_body_powerups`
      - Anchors Follow Body → `vr_body_anchors`
      - Hip Holsters Follow Legs → `vr_holster_leg_follow`
      - — Body Collisions —
      - Body Collisions → `vr_body_collide`
      - Pass Through At → `vr_body_collide_pass`
      - Elbows Out of the Torso → `vr_body_collide_elbows`
      - — Torso Direction —
      - Torso Follows → `vr_torso_mode`
      - Head Weight → `vr_torso_head`
      - Head History Weight → `vr_torso_head_history`
      - Head History Span → `vr_torso_head_lag`
      - Both Hands Weight → `vr_torso_hands`
      - One Hand Weight → `vr_torso_one_hand`
      - Hands Down Weight → `vr_torso_hands_down`
      - Side Reach → `vr_torso_side_angle`
      - Turn Deadzone → `vr_torso_deadzone`
      - Turn Speed → `vr_torso_speed`
      - Neck Turn → `vr_torso_neck_max`
      - — Placement —
      - Torso Offset → `vr_body_torso_back`
      - Legs Offset → `vr_body_legs_back`
      - Eyes Forward → `vr_body_eye_forward`
      - Eyes Up → `vr_body_eye_up`
      - Crouch Tilt → `vr_body_crouch_tilt`
    - **Flashlight** [menu_vr 17] — 29 rows / 22 settings / 0 actions (vr_menu.cpp:1545)
      - **Flashlight - Low Grip** [menu_vr 58] — 8 rows / 6 settings / 0 actions, built dynamically (vr_menu.cpp:1590)
        - — In the Hand: Low Grip —
        - Low Grip Forward → `vr_flashlight_low_x`
        - Low Grip Towards Palm → `vr_flashlight_low_y`
        - Low Grip Up → `vr_flashlight_low_z`
        - Low Grip Pitch → `vr_flashlight_low_pitch`
        - Low Grip Yaw → `vr_flashlight_low_yaw`
        - Low Grip Roll → `vr_flashlight_low_roll`
        - — Low Grip: Fingers on the Torch —
      - **Flashlight - Overhead Grip** [menu_vr 59] — 8 rows / 6 settings / 0 actions, built dynamically (vr_menu.cpp:1613)
        - — In the Hand: Overhead Grip —
        - Overhead Grip Forward → `vr_flashlight_high_x`
        - Overhead Grip Towards Palm → `vr_flashlight_high_y`
        - Overhead Grip Up → `vr_flashlight_high_z`
        - Overhead Grip Pitch → `vr_flashlight_high_pitch`
        - Overhead Grip Yaw → `vr_flashlight_high_yaw`
        - Overhead Grip Roll → `vr_flashlight_high_roll`
        - — Overhead Grip: Fingers on the Torch —
      - **Flashlight - On a Gun or Head** [menu_vr 60] — 18 rows / 15 settings / 0 actions (vr_menu.cpp:1636)
        - — Reach Zones —
        - Show Flashlight Zones → `vr_show_flashlight_zones`
        - — On a Gun —
        - On Gun Forward → `vr_flashlight_gun_forward`
        - On Gun Up → `vr_flashlight_gun_up`
        - On Gun Out → `vr_flashlight_gun_out`
        - Gun Zone Along → `vr_flashlight_gun_zone_forward`
        - Gun Zone Up → `vr_flashlight_gun_zone_up`
        - Gun Zone Out → `vr_flashlight_gun_zone_out`
        - Gun Zone Radius → `vr_flashlight_gun_zone_radius`
        - — On the Head —
        - On Head Forward → `vr_flashlight_head_forward`
        - On Head Up → `vr_flashlight_head_up`
        - On Head Out → `vr_flashlight_head_out`
        - Head Zone Forward → `vr_flashlight_head_zone_forward`
        - Head Zone Up → `vr_flashlight_head_zone_up`
        - Head Zone Out → `vr_flashlight_head_zone_out`
        - Head Zone Radius → `vr_flashlight_head_zone_radius`
      - — Flashlight —
      - Chest Flashlight → `vr_flashlight`
      - Brightness → `vr_flashlight_brightness`
      - Range → `vr_flashlight_range`
      - Visible Beam → `vr_flashlight_beam`
      - Beam Quality → `vr_flashlight_beam_quality`
      - Casts Shadows → `vr_flashlight_shadows`
      - Cord → `vr_flashlight_cord`
      - Beam Hue → `vr_flashlight_hue`
      - Beam Saturation → `vr_flashlight_saturation`
      - — Taking and Clipping On —
      - Grab Range → `vr_flashlight_grab_range`
      - Head Clip Range → `vr_flashlight_head_range`
      - Gun Clip Range → `vr_flashlight_gun_range`
      - Clip on Head When Let Go → `vr_flashlight_auto_head`
      - Clip on Gun When Let Go → `vr_flashlight_auto_gun`
      - Clip-On Transition → `vr_flashlight_clip_time`
      - — On the Belt —
      - Lean Out → `vr_flashlight_tilt`
      - Forward → `vr_flashlight_forward`
      - Up → `vr_flashlight_up`
      - Out → `vr_flashlight_out`
      - Side → `vr_flashlight_side`
      - — In the Hand —
      - In Hand Forward → `vr_flashlight_hand_forward`
      - In Hand Up → `vr_flashlight_hand_up`
    - **Weapons** [menu_vr 47] — 15 rows / 0 settings / 0 actions (vr_menu.cpp:4854)
      - — Tuning —
      - **Weapon Offsets** [menu_vr 23] — 12 rows / 4 settings / 3 actions, built dynamically (vr_menu.cpp:5229)
        - — Posing Mode —
        - [action] Pose This Weapon → weaponOffsetsPoseWeapon()
        - Weapon Hand → `vr_pose_weapon_hand`
        - Tuning Offsets on Confirm → `vr_pose_reset_offsets`
        - Shot Pitch (up) → `Key::ShotPitch`
        - Shot Yaw (left) → `Key::ShotYaw`
        - — Settings —
        - → link `fist` to  [cross-link]
        - → link `Weight, Melee and Throwing` to Weapon Weights [cross-link]
        - — This Weapon —
        - [action] Print Changes to Console → weaponOffsetsPrint()
        - [action] Reset This Weapon → weaponOffsetsReset()
        - (child page linked from a loop / dynamically)
        - **Weapon Offsets - Hand and Grip** [menu_vr 84] — 40 rows / 35 settings / 0 actions, built dynamically **LONG** (vr_menu.cpp:5282)
          - — fist —
          - Offset X (forward) → `Key::OffsetX`
          - Offset Y (left) → `Key::OffsetY`
          - Offset Z (up) → `Key::OffsetZ`
          - Pitch → `Key::Pitch`
          - Yaw → `Key::Yaw`
          - Roll → `Key::Roll`
          - Scale → `Key::Scale`
          - Hide Hand → `weapons::cvar(slot, Key::HideHand)`
          - — Tuning Aids —
          - Show Controller → `vr_show_controller`
          - Show Controller Laser → `vr_show_controller_laser`
          - — Controller Preview (Show Controller) —
          - Preview X (red) → `vr_show_controller_x`
          - Preview Y (green) → `vr_show_controller_y`
          - Preview Z (blue) → `vr_show_controller_z`
          - Preview Pitch (up) → `vr_show_controller_pitch`
          - Preview Yaw (left) → `vr_show_controller_yaw`
          - Preview Roll → `vr_show_controller_roll`
          - Off Hand Preview → `vr_show_controller_off_own`
          - Off Hand X (red) → `vr_show_controller_off_x`
          - Off Hand Y (green) → `vr_show_controller_off_y`
          - Off Hand Z (blue) → `vr_show_controller_off_z`
          - Off Hand Pitch (up) → `vr_show_controller_off_pitch`
          - Off Hand Yaw (left) → `vr_show_controller_off_yaw`
          - Off Hand Roll → `vr_show_controller_off_roll`
          - — Hand and Weapon Together —
          - Together X (forward) → `Key::WholeX`
          - Together Y (left) → `Key::WholeY`
          - Together Z (up) → `Key::WholeZ`
          - Together Pitch (up) → `Key::WholePitch`
          - Together Yaw (left) → `Key::WholeYaw`
          - Together Roll → `Key::WholeRoll`
          - — Hand Only —
          - Hand X (forward) → `Key::HandOnlyX`
          - Hand Y (left) → `Key::HandOnlyY`
          - Hand Z (up) → `Key::HandOnlyZ`
          - Hand Pitch (up) → `Key::HandOnlyPitch`
          - Hand Yaw (left) → `Key::HandOnlyYaw`
          - Hand Roll → `Key::HandOnlyRoll`
        - (child page linked from a loop / dynamically)
        - **Weapon Offsets - Fingers** [menu_vr 85] — 17 rows / 16 settings / 0 actions, built dynamically (vr_menu.cpp:5377)
          - — Fingers on the Weapon —
          - Fingers → `weapons::cvar(slot, Key::FingerManual)`
          - Thumb Curl → `Key::FingerCurlThumb`
          - Thumb Across → `Key::FingerThumbAcross`
          - Index Curl → `Key::FingerCurlIndex`
          - Middle Curl → `Key::FingerCurlMiddle`
          - Ring Curl → `Key::FingerCurlRing`
          - Little Curl → `Key::FingerCurlPinky`
          - Overlap → `Key::GripOverlap`
          - Thumb → `Key::FingerThumbBias`
          - Index Finger → `Key::FingerIndexBias`
          - Middle Finger → `Key::FingerMiddleBias`
          - Ring Finger → `Key::FingerRingBias`
          - Little Finger → `Key::FingerPinkyBias`
          - Thumb X (forward) → `Key::FingerThumbX`
          - Thumb Y (palm) → `Key::FingerThumbY`
          - Thumb Z (up) → `Key::FingerThumbZ`
        - (child page linked from a loop / dynamically)
        - **Weapon Offsets - Muzzle and Sights** [menu_vr 86] — 16 rows / 8 settings / 4 actions, built dynamically (vr_menu.cpp:5428)
          - — Align Sights to My Aim —
          - [action] Apply → sightAlignApply()
          - [action] Cancel → sightAlignCancel()
          - [action] Align Sights to My Aim → sightAlignStart()
          - [action] Undo → sightAlignUndo()
          - [infoLine] sightAlignLine
          - [info] sightAlignNone
          - Dominant Eye → `vr_dominant_eye`
          - Captures → `vr_sight_align_captures`
          - Show Sight Line → `vr_show_sight_line`
          - — Muzzle —
          - Muzzle X → `Key::MuzzleOffsetX`
          - Muzzle Y → `Key::MuzzleOffsetY`
          - Muzzle Z → `Key::MuzzleOffsetZ`
          - Shot Pitch (up) → `Key::ShotPitch`
          - Shot Yaw (left) → `Key::ShotYaw`
        - (child page linked from a loop / dynamically)
        - **Weapon Offsets - Two-Handed** [menu_vr 87] — 42 rows / 36 settings / 4 actions, built dynamically **LONG** (vr_menu.cpp:5493)
          - — Other Hand's Grips (Hotspots) —
          - Two-Handed → `weapons::cvar(slot, Key::TwoHMode)`
          - Other Hand Anywhere → `weapons::cvar(slot, Key::AnyGripMode)`
          - Hotspot → `vr_weapon_hotspot`
          - Type → `hk(0)`
          - [action] Pose This Hotspot → weaponOffsetsPoseHotspot()
          - [action] Pose a New Hotspot → weaponOffsetsPoseNewHotspot()
          - Along the Blade → `hk(1)`
          - Blade Grip Ends At → `hk(2)`
          - Hotspot X → `hk(1)`
          - Hotspot Y → `hk(2)`
          - Hotspot Z → `hk(3)`
          - [action] Put It Where the Other Hand Is → weaponOffsetsHotspotAtHand()
          - Hand Pitch → `hk(5)`
          - Hand Yaw → `hk(6)`
          - Hand Roll → `hk(7)`
          - Thumb → `hk(8)`
          - Fingers There → `hk(16)`
          - Thumb Curl There → `hk(17)`
          - Thumb Across There → `hk(22)`
          - Index Curl There → `hk(18)`
          - Middle Curl There → `hk(19)`
          - Ring Curl There → `hk(20)`
          - Little Curl There → `hk(21)`
          - Overlap There → `hk(9)`
          - Held Hand X (forward) → `hk(10)`
          - Held Hand Y (left) → `hk(11)`
          - Held Hand Z (up) → `hk(12)`
          - Held Hand Pitch (up) → `hk(13)`
          - Held Hand Yaw (left) → `hk(14)`
          - Held Hand Roll → `hk(15)`
          - Bias → `hk(4)`
          - Stickiness → `hk(23)`
          - [action] Remove This Hotspot → weaponOffsetsHotspotRemove()
          - Show Hotspots → `vr_show_weapon_hotspots`
          - — Two-Handed Aim —
          - Aim Offset X → `Key::TwoHOffsetX`
          - Aim Offset Y → `Key::TwoHOffsetY`
          - Aim Offset Z → `Key::TwoHOffsetZ`
          - Aim Pitch → `Key::TwoHPitch`
          - Aim Yaw → `Key::TwoHYaw`
          - Aim Roll → `Key::TwoHRoll`
        - (child page linked from a loop / dynamically)
        - **Weapon Offsets - Virtual Stock** [menu_vr 88] — 12 rows / 8 settings / 0 actions, built dynamically (vr_menu.cpp:5629)
          - — Virtual Stock: This Weapon —
          - [infoLine] weaponOffsetsStockReadout
          - Two-Handed → `weapons::cvar(slot, Key::TwoHMode)`
          - Stock Pitch (up) → `Key::StockPitch`
          - Stock Yaw (left) → `Key::StockYaw`
          - Stock Roll (right) → `Key::StockRoll`
          - — Every Weapon —
          - 2H Aiming → `vr_2h_mode`
          - Stock Factor → `vr_2h_virtual_stock_factor`
          - Stock Distance → `vr_virtual_stock_thresh`
          - Show Virtual Stock → `vr_show_virtual_stock`
          - → link `Hotspots (the Shoulders)` to Hotspots [cross-link]
        - (child page linked from a loop / dynamically)
        - **Weapon Offsets - Ammo Screen** [menu_vr 89] — 9 rows / 8 settings / 0 actions, built dynamically (vr_menu.cpp:5666)
          - — Ammo Screen —
          - Screen X → `Key::WpnTextX`
          - Screen Y → `Key::WpnTextY`
          - Screen Z → `Key::WpnTextZ`
          - Screen Pitch → `Key::WpnTextPitch`
          - Screen Yaw → `Key::WpnTextYaw`
          - Screen Roll → `Key::WpnTextRoll`
          - Screen Scale → `Key::WpnTextScale`
          - Ammo Screen → `weapons::cvar(slot, Key::WpnTextMode)`
        - (child page linked from a loop / dynamically)
        - **Weapon Offsets - Holstered** [menu_vr 90] — 6 rows / 3 settings / 2 actions, built dynamically (vr_menu.cpp:5691)
          - label → `weapons::cvar`
          - — Holstered —
          - Holster → `vr_weapon_holster`
          - [action] Pose in This Holster → weaponOffsetsPoseHolster()
          - Preview in Holster → `vr_weapon_holster_preview`
          - [action] Holstered Back to 0 → weaponOffsetsHolsteredReset()
        - (child page linked from a loop / dynamically)
        - **Weapon Offsets - Effects** [menu_vr 91] — 18 rows / 15 settings / 1 actions, built dynamically (vr_menu.cpp:5738)
          - — Effects —
          - Recoil → `weapons::cvar(slot, Key::Recoil)`
          - Recoil Strength → `Key::RecoilStrength`
          - Recoil Return Time → `Key::RecoilTime`
          - Muzzle Flash → `weapons::cvar(slot, Key::Flash)`
          - Flash Size → `Key::FlashSize`
          - Flash Time → `Key::FlashTime`
          - Tracers → `weapons::cvar(slot, Key::Tracers)`
          - Tracer Speed → `Key::TracerSpeed`
          - Tracer Length → `Key::TracerLength`
          - Tracer Thickness → `Key::TracerWidth`
          - Tracer Chance → `Key::TracerChance`
          - Tracer Colour → `weapons::cvar(slot, Key::TracerOwnColour)`
          - Tracer Red → `Key::TracerRed`
          - Tracer Green → `Key::TracerGreen`
          - Tracer Blue → `Key::TracerBlue`
          - [action] Test the Effects → weaponOffsetsTestEffects()
          - → link `Weapon Effects (All Weapons)` to Weapon Effects [cross-link]
        - (child page linked from a loop / dynamically)
        - **Weapon Offsets - Flashlight** [menu_vr 92] — 12 rows / 8 settings / 2 actions, built dynamically (vr_menu.cpp:5783)
          - — Flashlight on This Weapon —
          - Flashlight Can Clip On → `weapons::cvar(slot, Key::TorchClip)`
          - [action] Clip the Flashlight on It → weaponOffsetsClipTorch()
          - Show the Flashlight's Place → `vr_flashlight_mount_preview`
          - Flashlight Forward → `Key::TorchForward`
          - Flashlight Up → `Key::TorchUp`
          - Flashlight Out → `Key::TorchOut`
          - Flashlight Pitch (up) → `Key::TorchPitch`
          - Flashlight Yaw (out) → `Key::TorchYaw`
          - Flashlight Roll (out) → `Key::TorchRoll`
          - [action] Flashlight Back to 0 → weaponOffsetsTorchReset()
          - → link `On a Gun or Head (Every Weapon)` to Flashlight - On a Gun or Head [cross-link]
      - **Weapon Weights** [menu_vr 42] — 39 rows / 18 settings / 4 actions, built dynamically **LONG** (vr_menu_props.inc:146)
        - — title —
        - [action] Edit the Other Hand's Weapon → weaponWeightsOtherHand()
        - — title —
        - [action] Edit the Other Hand's Weapon → weaponWeightsOtherHand()
        - — inheritTitle —
        - label → `weapons::cvar(slot, key)`
        - [infoLine] weaponWeightsHandReadout
        - — Weight —
        - Mass → `Key::Mass`
        - Balance → `Key::Balance`
        - Length → `Key::Span`
        - — Spring (times the Aiming page's) —
        - → link `All Weapons' Spring (Aiming)` to Aiming [cross-link]
        - — Damage —
        - [infoLine] weaponWeightsDamageReadout
        - [infoLine] weaponWeightsDamageReadout
        - Melee Damage → `Key::MeleeDamage`
        - Throw Damage → `Key::ThrowDamage`
        - — Damage Thresholds and Curves —
        - [infoLine] weaponWeightsHitsReadout
        - [infoLine] weaponWeightsHitsReadout
        - Throw Speed Needed → `Key::ThrowMinSpeed`
        - Throw Damage Curve → `Key::ThrowCurve`
        - Melee Speed Needed → `Key::MeleeMinSpeed`
        - Melee Damage Curve → `Key::MeleeCurve`
        - — Thrown —
        - Spin in the Air → `Key::SpinAlign`
        - — Wrenched Out (experimental, every weapon) —
        - [infoLine] weaponWeightsDropReadout
        - Heavy Weapons Wrenched Out → `vr_weight_drop`
        - Heavier Than → `vr_weight_drop_from`
        - Fastest Turn → `vr_weight_drop_speed`
        - Weight Curve → `vr_weight_drop_curve`
        - Two Hands → `vr_weight_drop_2h`
        - Measured Over → `vr_weight_drop_window`
        - Falls At → `vr_weight_drop_velocity`
        - — This Weapon —
        - [action] Print Changes to Console → weaponWeightsPrint()
        - [action] Reset This Weapon → weaponWeightsReset()
      - **Hand/Gun Calibration** [menu_vr 22] — 25 rows / 20 settings / 2 actions, built dynamically (vr_menu_pages.inc:240)
        - → link `Fingers and Collisions` to Fingers and Collisions [cross-link]
        - — Hand Calibration —
        - Show Controller → `vr_show_controller`
        - Main Hand X (forward) → `vr_handcal_x`
        - Main Hand Y (left) → `vr_handcal_y`
        - Main Hand Z (up) → `vr_handcal_z`
        - Main Hand Pitch (down) → `vr_gunangle`
        - Main Hand Yaw (left) → `vr_gunyaw`
        - Main Hand Roll → `vr_handcal_roll`
        - Off Hand → `vr_handcal_off_mirror`
        - Off Hand X (forward) → `vr_handcal_off_x`
        - Off Hand Y (left) → `vr_handcal_off_y`
        - Off Hand Z (up) → `vr_handcal_off_z`
        - Off Hand Pitch (down) → `vr_offhandpitch`
        - Off Hand Yaw (left) → `vr_offhandyaw`
        - Off Hand Roll → `vr_handcal_off_roll`
        - [action] Match Controller Preview → matchControllerPreview()
        - [action] Reset Moves and Rolls → resetHandMoves()
        - — Guns —
        - Gun Model Pitch → `vr_gunmodelpitch`
        - Gun Model Scale → `vr_gunmodelscale`
        - Gun Model Z Offset → `vr_gunmodely`
        - Gun Z Offset → `vr_gun_z_offset`
        - Finger Grip Bias → `vr_finger_grip_bias`
        - Auto close thumb → `vr_finger_auto_close_thumb`
      - **Fingers and Collisions** [menu_vr 61] — 17 rows / 15 settings / 0 actions (vr_menu_pages.inc:310)
        - — Fingers —
        - Jointed Hand → `vr_hand_rig`
        - Fit Fingers to What You Hold → `vr_hand_fit`
        - Palm Fit → `vr_hand_fit_palm`
        - Palm Fit: Weapons → `vr_hand_fit_palm_weapon`
        - Palm Turn → `vr_hand_fit_palm_turn`
        - Thumb Round In-Palm Props → `vr_hand_fit_thumb_outside`
        - Thumb Opens Wide Round Them → `vr_hand_fit_thumb_wide`
        - Fit Overlap: Things → `vr_hand_fit_overlap`
        - Fit Overlap: Hands → `vr_hand_fit_overlap_hands`
        - Pose Blend → `vr_hand_fit_blend`
        - Refit Threshold → `vr_hand_fit_resolve`
        - Fingers Stop at Walls → `vr_hand_walls`
        - — Against Monsters and Things —
        - Weapons Stop at Models → `vr_model_collide`
        - Hands Stop at Models → `vr_model_collide_hands`
        - Model Push Limit → `vr_model_collide_max`
      - — Handling —
      - **Aiming** [menu_vr 24] — 33 rows / 23 settings / 0 actions **LONG** (vr_menu_pages.inc:393)
        - 2H Aiming → `vr_2h_mode`
        - 2H Aiming Threshold → `vr_2h_angle_threshold`
        - 2H Stock Factor → `vr_2h_virtual_stock_factor`
        - 2H Hand-Off → `vr_2h_handoff`
        - — 2H Grip Stickiness —
        - Stickiness → `vr_2h_sticky`
        - While Swinging → `vr_2h_sticky_fast`
        - Swing From → `vr_2h_sticky_fast_from`
        - Swing Full At → `vr_2h_sticky_fast_full`
        - Swing Fades Over → `vr_2h_sticky_fast_hold`
        - — Weight —
        - → link `Weapon Weights (Held Weapon)` to Weapon Weights [cross-link]
        - → link `Held Object Weights (Held Prop)` to Held Object Weights [cross-link]
        - → link `Weight and Damage` to Weight and Damage [cross-link]
        - [infoLine] weightReadout
        - [infoLine] weightReadout
        - — Tired Arms —
        - Heavier When Tired → `vr_weight_stamina`
        - From Stamina → `vr_weight_stamina_from`
        - Most Weight → `vr_weight_stamina_max`
        - Curve → `vr_weight_stamina_curve`
        - Extra Weight → `vr_weight_stamina_add`
        - Empty Hand Weight → `vr_weight_stamina_empty`
        - → link `Tired Arms Shake (Stamina)` to Stamina [cross-link]
        - — Spring —
        - Stiffness → `vr_weight_spring_stiffness`
        - Damping → `vr_weight_spring_damping`
        - Arm Strength → `vr_weight_spring_strength`
        - Sag → `vr_weight_spring_sag`
        - Swing → `vr_weight_spring_inertia`
        - Roll Weight → `vr_weight_spring_roll`
        - Two-Handed Help → `vr_weight_spring_2h`
        - Snap Back Beyond → `vr_weight_spring_snap`
      - **Weight and Damage** [menu_vr 62] — 8 rows / 8 settings / 0 actions (vr_menu_pages.inc:463)
        - Weight Damage Exponent → `vr_weight_damage_exp`
        - Heavier Than → `vr_weight_damage_heavy`
        - Lighter Than → `vr_weight_damage_light`
        - Least Damage → `vr_weight_damage_min`
        - Most Damage → `vr_weight_damage_max`
        - Heavy Leniency → `vr_weight_lenient`
        - Starts At → `vr_weight_lenient_from`
        - Least Speed Factor → `vr_weight_lenient_min`
      - → link `Weapon Damage` to Weapon Damage [cross-link]
      - **Immersion** [menu_vr 21] — 18 rows / 16 settings / 0 actions (vr_menu_pages.inc:492)
        - Body Picks Up Items → `vr_body_interactions`
        - Disable Haptics → `vr_disablehaptics`
        - — Weapons and Holsters —
        - Weapon Mode → `vr_holster_mode`
        - Weapon Reloading Mode → `vr_reload_mode`
        - Holster Haptics → `vr_holster_haptics`
        - Holster Haptic Time → `vr_holster_haptic_time`
        - Draw Blend Time → `vr_weapon_draw_blend`
        - Holster Blend Time → `vr_weapon_holster_blend`
        - Weapon Cycle Mode → `vr_weapon_cycle_mode`
        - Weapon Throw Mode → `vr_weapon_throw_mode`
        - Throw Damage Mult. → `vr_weapon_throw_damage_mult`
        - Throw Speed Mult. → `vr_weapon_throw_velocity_mult`
        - Dropped Wpn Particles → `vr_weapondrop_particles`
        - — Shell Casings —
        - Shell Casings → `vr_shells`
        - Shell Casing Life → `vr_shells_life`
        - Shell Casing Sound → `vr_shells_sound`
      - **Lightning Gun in Water** [menu_vr 75] — 9 rows / 7 settings / 2 actions (vr_menu.cpp:4833)
        - Lightning Gun in Water → `vr_lg_water`
        - Shock Damage to You → `vr_lg_water_self_damage`
        - Shock Damage to Others → `vr_lg_water_damage`
        - Reach → `vr_lg_water_radius`
        - Falloff → `vr_lg_water_falloff`
        - Electrified Water Damage → `vr_lg_water_tick_damage`
        - Shock Flash → `vr_lg_water_flash`
        - [cmd] Test the Shock Effect → `vr_shock_test 0`
        - [cmd] Test the Electrified Water → `vr_shock_test 1`
      - **Weapon Effects** [menu_vr 80] — 23 rows / 17 settings / 1 actions (vr_menu.cpp:4791)
        - — Recoil —
        - Programmatic Recoil → `vr_weapon_recoil`
        - Kick Back → `vr_recoil_kick`
        - Muzzle Rise → `vr_recoil_rise`
        - — Muzzle Flash —
        - Programmatic Muzzle Flash → `vr_muzzle_flash`
        - Enemies' Muzzle Flashes → `vr_muzzle_flash_enemies`
        - Enemies' Muzzle Smoke → `vr_muzzle_smoke_enemies`
        - Enemies' Flash Size → `vr_muzzle_flash_enemy_size`
        - — Bullet Tracers —
        - Bullet Tracers → `vr_tracers`
        - Enemies' Tracers → `vr_tracers_enemies`
        - Tracer Speed → `vr_tracer_speed`
        - Tracer Length → `vr_tracer_length`
        - Tracer Thickness → `vr_tracer_width`
        - Chance a Pellet → `vr_tracer_chance`
        - Tracer Red → `vr_tracer_r`
        - Tracer Green → `vr_tracer_g`
        - Tracer Blue → `vr_tracer_b`
        - Tracer Brightness → `vr_tracer_brightness`
        - — Each Weapon —
        - → link `Weapon Offsets (Held Weapon)` to Weapon Offsets [cross-link]
        - [cmd] Test the Held Weapon's Effects → `vr_weaponfx_test 1 3`
      - — Holsters —
      - **Hotspots** [menu_vr 25] — 24 rows / 21 settings / 0 actions (vr_menu_pages.inc:1084)
        - — Virtual Stock —
        - Show Virtual Stock → `vr_show_virtual_stock`
        - Shoulder X → `vr_shoulder_offset_x`
        - Shoulder Y → `vr_shoulder_offset_y`
        - Shoulder Z → `vr_shoulder_offset_z`
        - Virtual Stock Thresh. → `vr_virtual_stock_thresh`
        - — Shoulder Holsters —
        - Show Shoulder Holst. → `vr_show_shoulder_holsters`
        - Shoulder X → `vr_shoulder_holster_offset_x`
        - Shoulder Y → `vr_shoulder_holster_offset_y`
        - Shoulder Z → `vr_shoulder_holster_offset_z`
        - Shoulder Threshold → `vr_shoulder_holster_thresh`
        - Shoulder Pitch → `vr_shoulder_holster_pitch`
        - Shoulder Yaw → `vr_shoulder_holster_yaw`
        - Shoulder Roll → `vr_shoulder_holster_roll`
        - — Upper Holsters —
        - Show Upper Holsters → `vr_show_upper_holsters`
        - Upper X → `vr_upper_holster_offset_x`
        - Upper Y → `vr_upper_holster_offset_y`
        - Upper Z → `vr_upper_holster_offset_z`
        - Upper Threshold → `vr_upper_holster_thresh`
        - Upper Pitch → `vr_upper_holster_pitch`
        - Upper Yaw → `vr_upper_holster_yaw`
        - Upper Roll → `vr_upper_holster_roll`
      - **Hip Holsters** [menu_vr 63] — 26 rows / 24 settings / 0 actions (vr_menu_pages.inc:1116)
        - Show Hip Holsters → `vr_show_hip_holsters`
        - Hip X → `vr_hip_offset_x`
        - Hip Y → `vr_hip_offset_y`
        - Hip Z → `vr_hip_offset_z`
        - Hip Threshold → `vr_hip_holster_thresh`
        - Hip Pitch → `vr_hip_holster_pitch`
        - Hip Yaw → `vr_hip_holster_yaw`
        - Hip Roll → `vr_hip_holster_roll`
        - — Holster Slots —
        - Show Holster Slots → `vr_leg_holster_model_enabled`
        - Holster Slot Scale → `vr_leg_holster_model_scale`
        - Holster Slot X → `vr_leg_holster_model_x_offset`
        - Holster Slot Y → `vr_leg_holster_model_y_offset`
        - Holster Slot Z → `vr_leg_holster_model_z_offset`
        - — Grenade Pouch —
        - Show Grenade Pouch → `vr_show_grenade_pouch`
        - Pouch X → `vr_grenade_pouch_x`
        - Pouch Y → `vr_grenade_pouch_y`
        - Pouch Z → `vr_grenade_pouch_z`
        - Pouch Threshold → `vr_grenade_pouch_thresh`
        - Pouch Pitch → `vr_grenade_pouch_pitch`
        - Pouch Yaw → `vr_grenade_pouch_yaw`
        - Pouch Roll → `vr_grenade_pouch_roll`
        - Grenade In Hand Pitch → `vr_grenade_pouch_hold_pitch`
        - Grenade In Hand Yaw → `vr_grenade_pouch_hold_yaw`
        - Grenade In Hand Roll → `vr_grenade_pouch_hold_roll`
    - — Display —
    - **HUD and Menus** [menu_vr 48] — 6 rows / 0 settings / 0 actions (vr_menu.cpp:4875)
      - **Wrist Gadget** [menu_vr 26] — 10 rows / 9 settings / 0 actions (vr_menu.cpp:1878)
        - HUD → `vr_hud_mode`
        - Arm → `vr_gadget_arm`
        - Size → `vr_gadget_scale`
        - — Placement —
        - Along the Arm → `vr_gadget_x`
        - Across the Arm → `vr_gadget_y`
        - Height → `vr_gadget_z`
        - Pitch → `vr_gadget_pitch`
        - Yaw → `vr_gadget_yaw`
        - Roll → `vr_gadget_roll`
      - **Screens** [menu_vr 27] — 28 rows / 23 settings / 1 actions (vr_menu.cpp:1903)
        - — Wrist Gadget —
        - Level and Stats → `vr_gadget_show_level`
        - Stamina and Counters → `vr_gadget_stamina`
        - Screen Light → `vr_gadget_light`
        - CRT Look → `vr_gadget_crt`
        - Screen Glow → `vr_screen_glow`
        - Text Glow → `vr_screen_text_glow`
        - — Messages —
        - Game Messages as Hologram → `vr_messages_hologram`
        - Hologram Time → `vr_messages_hologram_time`
        - Hologram Text Size → `vr_messages_hologram_size`
        - Hologram Height → `vr_messages_hologram_height`
        - Hologram Effect → `vr_messages_hologram_effect`
        - [action] Show a Test Message → hologramTestMessage()
        - Messages Only on the Gadget → `vr_messages_hologram_only`
        - Console Messages → `vr_notify_wrist`
        - Console Message Time → `vr_notify_wrist_time`
        - Console Log Height → `vr_notify_wrist_height`
        - Console Log Brightness → `vr_notify_wrist_alpha`
        - — Weapons' Ammo Screens —
        - Weapon Text → `vr_show_weapon_text`
        - Weapon Ammo Screen → `vr_weapon_screen`
        - Ammo Screen Margin → `vr_weapon_screen_padding`
        - Ammo Screen CRT Look → `vr_weapon_screen_crt`
        - Screens on Weapons at Rest → `vr_weapon_screen_idle`
        - — Map Boards —
        - Map Boards as CRTs → `vr_worldtext_crt`
        - Map Board Hue → `vr_worldtext_hue`
      - **Colours** [menu_vr 28] — 16 rows / 14 settings / 0 actions (vr_menu.cpp:1951)
        - Player Effects Hue → `vr_player_hue`
        - Player Effects Saturation → `vr_player_saturation`
        - — Wrist Gadget —
        - Screen Hue → `vr_gadget_screen_hue`
        - Screen Brightness → `vr_gadget_screen_brightness`
        - Screen Background → `vr_gadget_screen_background`
        - Casing Tint → `vr_gadget_tint`
        - Casing Tint Hue → `vr_gadget_tint_hue`
        - — Effects —
        - Weapon Sight Hue → `vr_sight_hue`
        - Weapon Sight Saturation → `vr_sight_saturation`
        - Force Grab Hue → `vr_forcegrab_hue`
        - Force Grab Saturation → `vr_forcegrab_saturation`
        - Teleport Arc Hue → `vr_teleport_hue`
        - Crosshair Hue → `vr_crosshair_hue`
        - Menu Laser Hue → `vr_menu_laser_hue`
      - **Status Bar** [menu_vr 29] — 9 rows / 9 settings / 0 actions (vr_menu_pages.inc:1068)
        - HUD → `vr_hud_mode`
        - Status Bar Mode → `vr_sbar_mode`
        - HUD Scale → `vr_hud_scale`
        - Offset X → `vr_sbar_offset_x`
        - Offset Y → `vr_sbar_offset_y`
        - Offset Z → `vr_sbar_offset_z`
        - Roll → `vr_sbar_offset_roll`
        - Pitch → `vr_sbar_offset_pitch`
        - Yaw → `vr_sbar_offset_yaw`
      - **Crosshair** [menu_vr 30] — 6 rows / 6 settings / 0 actions (vr_menu_pages.inc:31)
        - Crosshair → `vr_crosshair`
        - Crosshair Depth → `vr_crosshair_depth`
        - Crosshair Size → `vr_crosshair_size`
        - Crosshair Alpha → `vr_crosshair_alpha`
        - Crosshair Hue → `vr_crosshair_hue`
        - Crosshair Z Offset → `vr_crosshairy`
      - **Menu** [menu_vr 31] — 11 rows / 11 settings / 0 actions (vr_menu_pages.inc:13)
        - Menu Scale → `vr_menu_scale`
        - Menu Distance → `vr_menu_distance`
        - VR Menu Style → `vr_menu_vr_style`
        - Laser Hue → `vr_menu_laser_hue`
        - Main Menu Lettering → `vr_menu_bigfont`
        - Row Spacing → `vr_menu_spacing`
        - Menu Height → `vr_menu_height`
        - Menu Background Opacity → `scr_menubgalpha`
        - Live Preview → `ui_live_preview`
        - Reopen Where Left → `vr_menu_remember`
        - Drop-Down Lists → `vr_menu_dropdown`
    - **Graphics** [menu_vr 32] — 23 rows / 9 settings / 0 actions (vr_menu_pages.inc:529)
      - Preset → `vr_graphics_preset`
      - — Image —
      - Anti-aliasing → `vid_fsaa`
      - Smooth Textures → `vr_texture_smooth`
      - Fence Coverage → `vr_alpha_coverage`
      - Relit Maps → `vr_relit_maps`
      - Headset Gamma → `vr_gamma`
      - Headset Contrast → `vr_contrast`
      - — More Graphics —
      - **Graphics - Lights** [menu_vr 33] — 23 rows / 22 settings / 0 actions (vr_menu_pages.inc:563)
        - Light Contrast → `vr_light_contrast`
        - Ambient Light → `vr_ambient_light`
        - Coloured Lights → `vr_colored_lights`
        - Dynamic Light Falloff → `vr_dlight_falloff`
        - Uncapped Dynamic Lights → `vr_dlight_uncapped`
        - Dynamic Lights on Models → `vr_dlight_models`
        - Dynamic Light Angle → `vr_dlight_angle`
        - — Light Sources —
        - Muzzle Flash Light → `vr_flash_scale`
        - Explosion Light → `vr_explosion_light_scale`
        - Projectile Light → `vr_projectile_lights`
        - Lava Nail Lights → `vr_lavanail_lights`
        - Lava Gun Light → `vr_lavagun_light`
        - Lava Gun Light Reach → `vr_lavagun_light_radius`
        - Lava Gun Light Flicker → `vr_lavagun_light_flicker`
        - Lava Gun Light at Rest → `vr_lavagun_light_idle`
        - Lightning Beam Lights → `vr_beam_lights`
        - Lightning Arcs → `vr_beam_arcs`
        - Lightning Arc Spread → `vr_beam_arcs_spread`
        - Lightning Arc Width → `vr_beam_arcs_width`
        - Torch Lights → `vr_torch_lights`
        - Torch Light Brightness → `vr_torch_light_scale`
        - Torch Light Shadows → `vr_torch_light_shadows`
        - Ammo Screen Light → `vr_weapon_screen_light`
      - **Graphics - Shadows** [menu_vr 34] — 21 rows / 19 settings / 0 actions (vr_menu_pages.inc:595)
        - Shadowed Dynamic Lights → `vr_shadow_dlights`
        - Dynamic Shadow Detail → `vr_shadow_dlight_size`
        - Muzzle Flash Shadows → `vr_shadow_muzzleflash`
        - Map Light Shadows → `vr_shadow_maplights`
        - Map Shadow Detail → `vr_shadow_maplight_size`
        - Map Shadow Strength → `vr_shadow_maplight_strength`
        - Your Shadow → `vr_shadow_self`
        - Shadow Softness → `vr_shadow_filter`
        - Shadow Distance → `vr_shadow_distance`
        - Shadow Atlas → `vr_shadow_atlas`
        - Shadow Bias → `vr_shadow_bias`
        - Shadow Statistics → `vr_shadow_stats`
        - — Ambient Occlusion —
        - Contact Shadows → `vr_ao_dynamic`
        - Contact Shadow Reach → `vr_ao_dynamic_range`
        - Door and Lift Shadows → `vr_ao_brush`
        - Model Self-Shadowing → `vr_ao_models`
        - — Blob Shadows —
        - Blob Shadows → `vr_blob_shadows`
        - Blob Shadows: You → `vr_player_shadows`
        - Blob Shadows: Things → `vr_entity_shadows`
      - **Graphics - Surfaces** [menu_vr 35] — 27 rows / 23 settings / 0 actions (vr_menu_pages.inc:634)
        - Light Sheen → `vr_specular`
        - Sheen Anti-Aliasing → `vr_specular_aa`
        - — Bumps —
        - Bump Maps → `vr_normalmaps`
        - Bump Depth → `vr_normalmap_strength`
        - Bumps in Map Light → `vr_normalmap_baked`
        - Real Light Directions → `vr_deluxemap`
        - Bumps on Models → `vr_normalmap_models`
        - Authored Model Bumps → `vr_normalmap_authored`
        - — Parallax —
        - Parallax → `vr_parallax`
        - Parallax Depth → `vr_parallax_depth`
        - Parallax Distance → `vr_parallax_distance`
        - Parallax Items Depth → `vr_parallax_items`
        - Parallax Models Depth → `vr_parallax_models`
        - Parallax Depth: Authored Models → `vr_parallax_authored`
        - — External Maps —
        - External Maps → `vr_extmaps`
        - External Bump Maps → `vr_extmaps_normals`
        - External Specular Maps → `vr_extmaps_spec`
        - External Specular Brightness → `vr_extmaps_spec_scale`
        - External Glow Maps → `vr_extmaps_luma`
        - External Maps: Match → `vr_extmaps_match`
        - — Detail Textures —
        - Detail Textures → `vr_detail`
        - Detail Strength → `vr_detail_strength`
        - Detail Distance → `vr_detail_distance`
      - **Graphics - Liquids** [menu_vr 36] — 26 rows / 24 settings / 0 actions (vr_menu_pages.inc:985)
        - Waves → `vr_water_waves`
        - Water Reflection → `vr_water_fresnel`
        - Reflected Room → `vr_water_reflections`
        - Water Refraction → `vr_water_refraction`
        - Water Glints → `vr_water_glints`
        - Lava Glow → `vr_water_lava_glow`
        - Real Waves → `vr_water_geo_waves`
        - Real Wave Height → `vr_water_geo_amplitude`
        - Shoreline Foam → `vr_water_foam`
        - Heat Haze → `vr_heat_haze`
        - — Under Water —
        - Caustics → `vr_water_caustics`
        - Underwater View → `vr_water_underwater`
        - Underwater Wobble → `vr_water_wobble`
        - — Splashes and Ripples —
        - Splashes → `vr_water_splash`
        - Splash Size → `vr_water_splash_size`
        - Splash Ring Speed → `vr_water_splash_ring_speed`
        - Splash Ring Size → `vr_water_splash_ring_size`
        - Ripples → `vr_water_ripples`
        - Ripple Height (Geometry) → `vr_water_ripple_amplitude`
        - Ripple Normal Strength → `vr_water_ripple_normal`
        - Ripple Speed → `vr_water_ripple_speed`
        - Ripple Duration → `vr_water_ripple_decay`
        - Ripple Wavelength → `vr_water_ripple_wavelength`
        - Ripples at Once → `vr_water_ripple_max`
        - Water Sounds → `vr_water_sounds`
      - **Graphics - Slipgates** [menu_vr 135] — 2 rows / 2 settings / 0 actions (vr_menu_pages.inc)
        - Slipgate Views → `vr_portals`
        - Seamless Slipgates → `vr_portals_walk`
      - **Graphics - Post-processing** [menu_vr 37] — 12 rows / 11 settings / 0 actions (vr_menu_pages.inc:1018)
        - Bloom → `vr_bloom`
        - Bloom Threshold → `vr_bloom_threshold`
        - Bloom Size → `vr_bloom_radius`
        - Bloom: White Lights → `vr_bloom_white`
        - Bloom: Coloured Lights → `vr_bloom_color`
        - Bloom: Adapt → `vr_bloom_adapt`
        - — Tone and Colour —
        - Tone Mapping → `vr_tonemap`
        - Exposure → `vr_exposure`
        - Colour Grade → `vr_grade`
        - Grade Strength → `vr_grade_strength`
        - Dither → `vr_dither`
      - **Graphics - Models and Effects** [menu_vr 38] — 22 rows / 20 settings / 0 actions (vr_menu_pages.inc:1039)
        - Model Lighting → `vr_model_lighting`
        - Models Lit as the World → `vr_model_light_parity`
        - Directional Ambient → `vr_model_ambient_dir`
        - Ambient Contrast → `vr_model_ambient_contrast`
        - Rim Light → `vr_rim_light`
        - Hands' Least Light → `vr_viewmodel_minlight`
        - Weapon Reflections → `vr_weapon_reflections`
        - Reflection Strength → `vr_weapon_reflections_strength`
        - — Glow —
        - Screen Glow → `vr_screen_glow`
        - Screen Text Glow → `vr_screen_text_glow`
        - Weapon Sight Glow → `vr_weapon_glow`
        - Weapon Sight Hue → `vr_sight_hue`
        - — Effects —
        - Soft Particles → `vr_soft_particles`
        - Soft Particle Distance → `vr_soft_particles_scale`
        - Decals → `vr_decals`
        - Decal Lifetime → `vr_decal_life`
        - Max Decals → `vr_decal_max`
        - Gib Blood → `vr_gib_blood`
        - Gib Blood Trail → `vr_gib_blood_trail`
        - Ammo Switch Morph → `vr_weapon_morph_time`
      - **Graphics - Retro Textures** [menu_vr 95] — 12 rows / 1 settings / 5 actions, built dynamically (vr_menu_pages.inc:674)
        - Retro Textures → `vr_retro`
        - — Kinds —
        - → link `retro::categoryLabel(c)` to page [cross-link]
        - — All Kinds —
        - **Retro Textures - All Categories** [menu_vr 115] — 16 rows / 5 settings / 7 actions, built dynamically (vr_menu_pages.inc:921)
          - [info] retro::allSummary
          - [action] Apply to Checked Categories → retro::allApplyNow()
          - Live → `retro::allLiveCvar()`
          - Copy From → `retro::allFromCvar()`
          - [action] Copy Its Values → retro::allCopyChosen()
          - — Values —
          - On → `retro::allValue(Param::On)`
          - — Apply These Settings —
          - r → ``
          - [action] Check All Settings → retroAllCheckSettings()
          - [action] Uncheck All Settings → retroAllUncheckSettings()
          - — To These Categories —
          - retro::categoryLabel → ``
          - [action] Check All Categories → retroAllCheckCategories()
          - [action] Uncheck All Categories → retroAllUncheckCategories()
          - [action] Apply to Checked Categories → retro::allApplyNow()
        - [cmd] Reset All to Defaults → `vr_retro_reset all`
        - — One Thing: Overrides —
        - [cmd] Pick: Point, 3 Seconds → `vr_retro_pick hand 3`
        - [cmd] Pick: Look, 3 Seconds → `vr_retro_pick head 3`
        - **Retro Textures - Override** [menu_vr 114] — 12 rows / 4 settings / 5 actions, built dynamically (vr_menu_pages.inc:741)
          - [info] retroTargetText
          - Override For → `retro::editKindCvar()`
          - — r —
          - Mode → `retro::editMode(r.p)`
          - Value → `retro::editValue(r.p)`
          - Value → `retro::editValue(r.p)`
          - — This Override —
          - [action] Use the Shipped One → retro::useShipped()
          - [action] No Override → retro::clearOverride()
          - [action] Save Now → retro::saveNow()
          - [cmd] Pick Again: Point, 3 Seconds → `vr_retro_pick hand 3`
          - [cmd] Pick Again: Look, 3 Seconds → `vr_retro_pick head 3`
        - [cmd] List Overrides → `vr_retro_override list`
        - [cmd] Reload the Override Files → `vr_retro_overrides_reload`
        - (child page linked from a loop / dynamically)
        - **Retro Textures - World** [menu_vr 96] — 0 rows / 0 settings / 0 actions (vr_menu_pages.inc:893)
        - (child page linked from a loop / dynamically)
        - **Retro Textures - Brush Entities** [menu_vr 97] — 0 rows / 0 settings / 0 actions (vr_menu_pages.inc:893)
        - (child page linked from a loop / dynamically)
        - **Retro Textures - Item Pickups** [menu_vr 98] — 0 rows / 0 settings / 0 actions (vr_menu_pages.inc:893)
        - (child page linked from a loop / dynamically)
        - **Retro Textures - Props and Debris** [menu_vr 99] — 0 rows / 0 settings / 0 actions (vr_menu_pages.inc:893)
        - (child page linked from a loop / dynamically)
        - **Retro Textures - Gibs** [menu_vr 100] — 0 rows / 0 settings / 0 actions (vr_menu_pages.inc:893)
        - (child page linked from a loop / dynamically)
        - **Retro Textures - Small Gibs** [menu_vr 101] — 0 rows / 0 settings / 0 actions (vr_menu_pages.inc:893)
        - (child page linked from a loop / dynamically)
        - **Retro Textures - Weapons in the World** [menu_vr 102] — 0 rows / 0 settings / 0 actions (vr_menu_pages.inc:893)
        - (child page linked from a loop / dynamically)
        - **Retro Textures - Held Weapons** [menu_vr 103] — 0 rows / 0 settings / 0 actions (vr_menu_pages.inc:893)
        - (child page linked from a loop / dynamically)
        - **Retro Textures - Monsters** [menu_vr 104] — 0 rows / 0 settings / 0 actions (vr_menu_pages.inc:893)
        - (child page linked from a loop / dynamically)
        - **Retro Textures - Your Hands** [menu_vr 105] — 0 rows / 0 settings / 0 actions (vr_menu_pages.inc:893)
        - (child page linked from a loop / dynamically)
        - **Retro Textures - Your Arms** [menu_vr 106] — 0 rows / 0 settings / 0 actions (vr_menu_pages.inc:893)
        - (child page linked from a loop / dynamically)
        - **Retro Textures - Your Torso** [menu_vr 107] — 0 rows / 0 settings / 0 actions (vr_menu_pages.inc:893)
        - (child page linked from a loop / dynamically)
        - **Retro Textures - Your Legs** [menu_vr 108] — 0 rows / 0 settings / 0 actions (vr_menu_pages.inc:893)
        - (child page linked from a loop / dynamically)
        - **Retro Textures - Your Gear** [menu_vr 109] — 0 rows / 0 settings / 0 actions (vr_menu_pages.inc:893)
        - (child page linked from a loop / dynamically)
        - **Retro Textures - Decals** [menu_vr 110] — 0 rows / 0 settings / 0 actions (vr_menu_pages.inc:893)
        - (child page linked from a loop / dynamically)
        - **Retro Textures - Particles** [menu_vr 111] — 0 rows / 0 settings / 0 actions (vr_menu_pages.inc:893)
        - (child page linked from a loop / dynamically)
        - **Retro Textures - Sprites** [menu_vr 112] — 0 rows / 0 settings / 0 actions (vr_menu_pages.inc:893)
        - (child page linked from a loop / dynamically)
        - **Retro Textures - Other Models** [menu_vr 113] — 0 rows / 0 settings / 0 actions (vr_menu_pages.inc:893)
      - **Graphics - Retro Lighting** [menu_vr 116] — 32 rows / 23 settings / 4 actions **LONG** (vr_menu_pages.inc:1165)
        - Retro Lighting → `vr_retrolight`
        - Block Edge Softness → `vr_retrolight_edge_soft`
        - Level Spacing → `vr_retrolight_spacing`
        - — World: the Baked Light —
        - World → `vr_retrolight_world`
        - Light Levels → `vr_retrolight_world_steps`
        - Level Edges Softness → `vr_retrolight_world_soft`
        - Dither → `vr_retrolight_world_dither`
        - Dither Cell → `vr_retrolight_world_dither_size`
        - Blocky Lightmap → `vr_retrolight_world_lightmap`
        - Lightmap Block → `vr_retrolight_world_luxel`
        - — World: Dynamic Lights —
        - Dynamic Light Levels → `vr_retrolight_world_dyn_steps`
        - Dynamic Light Blocks → `vr_retrolight_world_dyn_block`
        - — Models —
        - Models → `vr_retrolight_models`
        - Model Light Levels → `vr_retrolight_model_steps`
        - Model Level Edges Softness → `vr_retrolight_model_soft`
        - Model Dither → `vr_retrolight_model_dither`
        - Model Dither Cell → `vr_retrolight_model_dither_size`
        - Model Dynamic Light Levels → `vr_retrolight_model_dyn_steps`
        - Model Dynamic Light Blocks → `vr_retrolight_model_dyn_block`
        - — Shadows —
        - Shadow Edges → `vr_retrolight_shadow_filter`
        - Shadow Levels → `vr_retrolight_shadow_steps`
        - Shadow Level Softness → `vr_retrolight_shadow_soft`
        - Shadow Blocks → `vr_retrolight_shadow_block`
        - — Looks —
        - [cmd] Look: Software Quake → `vr_retrolight 1; vr_retrolight_world_steps 64; vr_retrolight_world_soft 0; vr_retrolight_w`
        - [cmd] Look: Blocky Lightmaps → `vr_retrolight 1; vr_retrolight_world_steps 0; vr_retrolight_world_lightmap 1; vr_retroligh`
        - [cmd] Look: Banded and Dithered → `vr_retrolight 1; vr_retrolight_world_steps 8; vr_retrolight_world_dither 1; vr_retrolight_`
        - [cmd] Reset to Defaults → `reset vr_retrolight_spacing; reset vr_retrolight_edge_soft; reset vr_retrolight_world; res`
      - **Particles** [menu_vr 39] — 3 rows / 3 settings / 0 actions (vr_menu_pages.inc:44)
        - Particle Effects → `r_particles`
        - Quake VR Particles → `vr_particles`
        - Particle Multiplier → `vr_particle_mult`
      - **Transparency** [menu_vr 40] — 6 rows / 6 settings / 0 actions (vr_menu_pages.inc:1151)
        - (!) No Vis → `r_novis`
        - Water Alpha → `r_wateralpha`
        - Lava Alpha → `r_lavaalpha`
        - Tele Alpha → `r_telealpha`
        - Slime Alpha → `r_slimealpha`
        - Map's Own Alpha → `vr_map_liquid_alpha`
      - **Recording** [menu_vr 65] — 38 rows / 31 settings / 2 actions **LONG** (vr_menu_recording.inc:9)
        - Window View → `vr_window_view`
        - Desktop Mirror → `vr_mirror`
        - Flat HUD on the Mirror → `vr_window_hud_mirror`
        - Flat HUD on the Spectator Camera → `vr_window_hud_spectator`
        - Hide Head Text on the Mirror → `vr_mirror_hide_hud_text`
        - Hide Head Text on the Spectator Camera → `vr_spectator_hide_hud_text`
        - — Steadying —
        - Smoothing → `vr_window_smooth`
        - Level Horizon → `vr_window_level`
        - — Smoothed Mirror —
        - Zoom → `vr_window_zoom`
        - — Spectator Camera —
        - Field of View → `vr_spectator_fov`
        - Frame Rate → `vr_spectator_rate`
        - Resolution Scale → `vr_spectator_scale`
        - Anti-Aliasing → `vr_spectator_aa`
        - Position Smoothing → `vr_spectator_pos_smooth`
        - — Slow Motion —
        - Time Scale → `vr_timescale`
        - Toggle's Time Scale → `vr_slowmo_scale`
        - Ease In and Out → `vr_timescale_ramp`
        - Slow Sounds → `vr_timescale_sound`
        - Game-Time Sound File → `vr_timescale_wav`
        - Hand Speed Limit → `vr_timescale_hand_speed`
        - Hand Turn Limit → `vr_timescale_hand_spin`
        - Turn in Real Time → `vr_timescale_turn_realtime`
        - Move in Real Time → `vr_timescale_move_realtime`
        - Sandevistan: You at Full Speed → `vr_sandevistan`
        - Bullet Time's Look → `vr_bullettime_fx`
        - — Highlight Markers —
        - Log Highlights → `vr_highlights`
        - [cmd] Sync Mark Now → `vr_highlights_sync`
        - [cmd] Mark This Moment → `vr_highlight_mark`
        - Sync Flash → `vr_highlights_flash`
        - Sync Beep → `vr_highlights_beep`
        - Markers' Frame Rate → `vr_highlights_fps`
        - Least Score for a Marker → `vr_highlights_min_score`
        - Multi-Kill Window → `vr_highlights_multikill`
      - — Performance —
      - Performance Profile → `vr_profile`
      - FPS Counter on the Gadget → `vr_gadget_fps`
    - — Playtesting —
    - **Motion Recorder** [menu_vr 6] — 11 rows / 6 settings / 1 actions, built dynamically (vr_menu.cpp:440)
      - — Record Motions for the Melee —
      - Arm Recorder → `vr_motion_armed`
      - Category → `vr_motion_category`
      - Detail → `vr_motion_detail`
      - [info] motion::labelStatus
      - Record Button → `vr_motion_button`
      - [info] motionNote
      - [info] motionLastSaved
      - [action] Delete Last Take → motion::discardLast()
      - Lead-in → `vr_motion_preroll`
      - Tail → `vr_motion_tail`
    - **Review Takes** [menu_vr 7] — 11 rows / 2 settings / 3 actions, built dynamically (vr_menu.cpp:493)
      - [info] motion::review::summary
      - [info] motion::review::reviewedLine
      - [info] motion::review::evalLine
      - Show → `vr_motion_review_show`
      - Category → `vr_motion_review_category`
      - [action] Re-evaluate Shown → motion::review::reevaluateShown()
      - [action] Stop Re-evaluation → motion::review::stopReevaluation()
      - [action] Undo Last → motion::review::undo()
      - [info] motion::review::lastAction
      - — reviewListHeader —
      - [row] motion::review::rowText
      - (child page linked from a loop / dynamically)
      - **Take** [menu_vr 8] — 20 rows / 3 settings / 11 actions, built dynamically (vr_menu.cpp:530)
        - [infoLine] motion::review::detailLine
        - — Look —
        - [action] Play Ghost → motion::review::playGhost()
        - [action] Stop Ghost → motion::review::stopGhost()
        - Ghost Speed → `vr_motion_review_speed`
        - [action] Replay (Mock Headset) → motion::review::replayMock()
        - — Decide —
        - [action] Keep (Reviewed) → motion::review::keep()
        - [action] Discard → motion::review::discard()
        - [action] Restore → motion::review::restore()
        - Relabel Category → `vr_motion_relabel_category`
        - Relabel Detail → `vr_motion_relabel_detail`
        - [action] Relabel → motion::review::relabel()
        - [action] Undo Last → motion::review::undo()
        - [info] motion::review::lastAction
        - — More —
        - [action] Next Take → motion::review::nextTake()
        - [action] Previous Take → motion::review::previousTake()
        - [action] Re-evaluate This Take → motion::review::reevaluateTake()
        - [info] motion::review::evalLine
    - **Debug** [menu_vr 64] — 11 rows / 2 settings / 0 actions (vr_menu.cpp:3147)
      - — Playtesting —
      - **Checklist** [menu_vr 72] — 6 rows / 1 settings / 1 actions, built dynamically (vr_menu.cpp:3085)
        - [info] checklistSummary
        - Hide Ticked → `vr_checklist_hide_ticked`
        - [action] Reload List → checklistReload()
        - — names —
        - [row] checklistLine
        - [info] checklistEmpty
      - Voice Notes → `vr_notes`
      - Slow Motion → `vr_timescale`
      - — Debug —
      - **Debug - Views** [menu_vr 66] — 27 rows / 19 settings / 8 actions (vr_menu.cpp:3175)
        - External Maps A/B → `vr_extmaps_ab`
        - Retro Textures A/B → `vr_retro_ab`
        - [cmd] Retro Textures: List → `vr_retro_list`
        - Retro Lighting A/B → `vr_retrolight_ab`
        - Ambient Light A/B → `vr_ambient_light_ab`
        - [cmd] Light Probe → `vr_light_probe`
        - Show Damage Numbers → `vr_debug_damage_numbers`
        - Show Grapple Rope → `vr_debug_rope`
        - Show Physics Shapes → `vr_debug_physics_shapes`
        - Show Hits → `vr_debug_hits`
        - Show Hand Bones → `vr_debug_hand_bones`
        - Show Ledges → `vr_debug_ledges`
        - Log Empty Hand Against Held → `vr_debug_hand_collide`
        - Show Grab Test → `vr_debug_carry`
        - Show Body Skeleton → `vr_body_debug`
        - [cmd] Print Torso Direction → `vr_torso_report`
        - Show Body Collisions → `vr_debug_body_collide`
        - Log Hand Offsets → `vr_debug_hand_offset`
        - Log Weapon Wall Collisions → `vr_debug_gun_wall`
        - Show Flashlight Zones → `vr_show_flashlight_zones`
        - [cmd] Flashlight to Left Hand → `vr_flashlight_give left`
        - [cmd] Flashlight to Right Hand → `vr_flashlight_give right`
        - [cmd] Clip Flashlight on Right Gun → `vr_flashlight_clip_gun right`
        - [cmd] Clip Flashlight on Head → `vr_flashlight_clip_head right`
        - [cmd] Flashlight Cord Info → `vr_flashlight_cord_info`
        - [cmd] Probe Flashlight → `vr_flashlight_probe menu`
        - Show Model Collisions → `vr_debug_model_collide`
        - Show Foveation → `vr_foveated_debug`
        - Show Entity Boxes → `r_showbboxes`
      - **Debug - Logging** [menu_vr 67] — 38 rows / 35 settings / 1 actions **LONG** (vr_menu.cpp:3264)
        - Developer Messages → `developer`
        - — Logs —
        - Bullet Time → `vr_debug_bullettime`
        - Chainsaw → `vr_debug_chainsaw`
        - Wall Buttons → `vr_debug_wallbuttons`
        - Shots and Damage → `vr_debug_shots`
        - Missile Hits → `vr_debug_missiles`
        - Throws → `vr_debug_throw`
        - Controller Spin → `vr_debug_angvel`
        - Highlights → `vr_debug_highlights`
        - Axe Sticks → `vr_debug_axestick`
        - Spin in the Air → `vr_debug_spin_align`
        - Climbing → `vr_climb_debug`
        - Hands → `vr_debug_hands`
        - Swim Strokes → `vr_swim_debug`
        - Grappling Hook → `vr_grapple_debug`
        - Controller Buttons → `vr_debug_buttons`
        - Wounds → `vr_wounds_debug`
        - Grasp → `vr_debug_grasp`
        - Holster Draw Blend → `vr_debug_draw_blend`
        - [cmd] Check Last Pose → `vr_pose_check`
        - Physics Sounds → `vr_debug_physsound`
        - Spatial Audio → `vr_debug_snd`
        - Network Messages → `vr_debug_net`
        - Ragdolls → `vr_debug_ragdoll`
        - Physics Bodies → `vr_debug_box3d`
        - Rocks and Bricks Placement → `vr_debug_debris`
        - Crates Placement → `vr_debug_crates`
        - Box Sizes → `vr_debug_item_sizes`
        - Torch Lights → `vr_debug_torch_lights`
        - Arm IK → `vr_debug_arm`
        - Heavy Weapon Wrenched Out → `vr_debug_weight_drop`
        - Two-Handed Grip → `vr_debug_2h_grip`
        - Bot Chatter → `vr_verbosebots`
        - — Trace Files (game folder) —
        - Grasp Trace → `vr_debug_grasp_trace`
        - Weight Trace → `vr_debug_weight`
        - Lean Trace → `vr_debug_lean`
      - **Debug - Profiling and Memory** [menu_vr 68] — 36 rows / 13 settings / 19 actions, built dynamically **LONG** (vr_menu.cpp:3379)
        - — Profiling —
        - Profiler Panel → `vr_profile_overlay`
        - CSV Capture → `vr_profile_csv`
        - Hitch Log → `vr_profile_hitch`
        - Detail → `vr_profile_detail`
        - GPU Timing → `vr_profile_gpu`
        - Report Interval → `vr_profile_interval`
        - [cmd] Print Report → `vr_profile_report`
        - [cmd] Dump Profile → `vr_profile_dump`
        - [cmd] QuakeC Instructions → `profile 30`
        - — Threads —
        - Split Work Between Threads → `vr_jobs_parallel`
        - Worker Threads → `vr_jobs_threads`
        - Physics on Threads → `vr_box3d_threads`
        - Physics Threads → `vr_box3d_workers`
        - Physics Threads From → `vr_box3d_threads_bodies`
        - [cmd] Physics Step Time → `vr_physics_steptime`
        - [cmd] Physics Step Time by Awake Bodies → `vr_physics_steptime bins`
        - [cmd] Spawn a Big Prop Pile → `vr_physics_bigpile`
        - [cmd] Thread Pool Info → `vr_jobs_info`
        - [cmd] Thread Pool Sites → `vr_jobs_sites`
        - [cmd] Thread Pool Sites Reset → `vr_jobs_sites reset`
        - [cmd] Thread Pool Split Bench → `vr_jobs_bench`
        - [cmd] Thread Pool Self-Test → `vr_jobs_test`
        - [cmd] Zancle Math Self-Test → `vr_zancle_math_test`
        - [cmd] Ragdoll Hand Probe → `vr_ragdoll_hand_probe`
        - [cmd] Grasp Bench → `vr_grasp_bench`
        - [cmd] Grasp Sweep → `vr_grasp_sweep 5`
        - — Memory —
        - Memory Log → `vr_memstats_log`
        - Memory Log: GPU → `vr_memstats_log_gpu`
        - [cmd] Print Memory Now → `vr_memstats`
        - [cmd] Allocation Sites → `vr_alloc_sites 300`
        - — Crashes —
        - [cmd] Crash the Game → `vr_debug_crash`
        - [cmd] Fail a Zancle Assert → `vr_debug_crash assert`
      - **Debug - Reports** [menu_vr 69] — 44 rows / 0 settings / 40 actions **LONG** (vr_menu.cpp:3488)
        - — The Game —
        - [cmd] Headset → `vr_status`
        - [cmd] Player → `vr_dumpplayer`
        - [cmd] View → `vr_dumpview`
        - [cmd] Bullet Time Now → `vr_bullettime`
        - [cmd] Slow Motion Clocks → `vr_slowmo_probe`
        - [cmd] Body Calibration → `vr_bodycal_print`
        - [cmd] Wrists and Grips → `vr_bodycal_debug`
        - — World and Physics —
        - [cmd] Physics Props → `vr_physics_list`
        - [cmd] Ragdolls → `vr_ragdoll_list 2`
        - [cmd] Grunt's Ragdoll Rig → `vr_ragdoll_info`
        - [cmd] Corpses in the Physics → `vr_corpse_list`
        - [cmd] Held Props → `vr_carry_check`
        - [cmd] Props in Floors → `vr_physics_sink`
        - [cmd] Props in Walls → `vr_physics_inlevel`
        - [cmd] Prop Approach → `vr_physics_approach`
        - [cmd] Watch Props Entered → `vr_physics_inside 1`
        - [cmd] Props Entered → `vr_physics_inside`
        - [cmd] Weights → `vr_weight_table`
        - [cmd] Throws by Weight → `vr_throw_table`
        - [cmd] Ledges Ahead → `vr_climb_probe`
        - [cmd] Rocks and Bricks → `vr_debris_list`
        - [cmd] Crates → `vr_crates_list`
        - [cmd] Hit Detection → `vr_hitmodel_stats`
        - [cmd] Wounds → `vr_wounds_info`
        - [cmd] Bloody Hands and Washing → `vr_gore_hands_info`
        - [cmd] Decals and Gore → `vr_decal_count`
        - [cmd] Model Lighting → `vr_model_ambient_show`
        - [cmd] Ambient Occlusion → `vr_ao_show`
        - — Hands and Weapons —
        - [cmd] Hand Rig → `vr_hand_rig_info`
        - [cmd] Grasp Spheres → `vr_grasp_spheres`
        - [cmd] Grip Frames → `vr_grip_frame`
        - [cmd] Hotspots Check → `vr_hotspots_check`
        - [cmd] Sight Lines → `vr_sight_lines`
        - [cmd] Sight Check → `vr_sight_check`
        - [cmd] Wrist Gadget → `vr_gadget_info`
        - — Other —
        - [cmd] Limits → `vr_limits`
        - [cmd] Microphones → `vr_note_devices`
        - [cmd] Detail Textures → `vr_detail_list`
        - [cmd] External Maps → `vr_extmaps_stats all`
        - [cmd] Main Menu Lettering → `vr_bigfont`
      - **Debug - Tools** [menu_vr 70] — 83 rows / 3 settings / 74 actions **LONG** (vr_menu.cpp:3548)
        - — Rebuild and Reload —
        - [cmd] Rebuild Ledge Map → `vr_ledges rebuild`
        - [cmd] Reload Models → `vr_model_reload`
        - [cmd] Reload Hand Model → `vr_hand_reload`
        - [cmd] Reload Detail Textures → `vr_detail_reload`
        - — Save to Files (game folder) —
        - [cmd] Wound Masks → `vr_wounds_dump`
        - [cmd] Decal Atlas → `vr_decal_atlas`
        - [cmd] Hand Mesh → `vr_grasp_dump`
        - [cmd] Gadget Screen → `vr_gadget_screen_dump`
        - [cmd] Eye Images (with the UI) → `vr_eyeshot 3`
        - [cmd] Texture Checksums → `imagehash`
        - [cmd] Reflection Map → `vr_envmap_dump`
        - [cmd] Weight Test → `vr_weight_test csv`
        - — Test Effects —
        - [cmd] Blood and Gore → `vr_gore_test`
        - [cmd] Gore Burst → `vr_gore_test burst`
        - [cmd] Blood Mist → `vr_gore_mist_test`
        - [cmd] Gib Blood on Hand → `vr_gore_hands_test main`
        - [cmd] Blood from a Blow → `vr_gore_spatter_test blow`
        - [cmd] List Clean Weapon Skins → `vr_cleanskins`
        - [cmd] Blood from a Blow on Your Prop → `vr_gore_spatter_test prop`
        - [cmd] Blood from a Chainsaw Cut → `vr_gore_spatter_test saw`
        - [cmd] Blood from a Close Shot → `vr_gore_spatter_test shot`
        - [cmd] Marks on Your Main Forearm → `vr_gore_spatter_test arm main`
        - [cmd] Gib Strikes Your Hand → `vr_gore_spatter_test gib`
        - [cmd] Blood from a Swing of Your Prop → `vr_gore_spatter_test propblow`
        - [cmd] Blood on Your Left Hip's Weapon → `vr_gore_spatter_test holster 2`
        - [cmd] A Gibbing Ahead → `vr_gore_spatter_test burst`
        - [cmd] Burn Your Arms → `vr_wounds_test self 4 90 0 12`
        - [cmd] Wound Your Arms → `vr_wounds_test self 1 20 4 14`
        - [cmd] Soak Your Arms → `vr_wounds_test self 9 0 0 52`
        - [cmd] Test Light → `vr_light_test`
        - [cmd] Test Message → `vr_message_test`
        - [cmd] Eject a Casing → `vr_shells_eject`
        - [cmd] Lightning Shock → `vr_shock_test 0`
        - [cmd] Electrified Water → `vr_shock_test 1`
        - [cmd] Mjolnir's Lightning → `impulse 215`
        - — Small Gibs Tests (developer 1 for each hit) —
        - [cmd] A Grunt Ahead → `vr_test_spawn 0; vr_test_spawn_dist 96; impulse 241`
        - [cmd] A Grunt's Corpse Ahead → `vr_test_spawn 0; vr_test_spawn_dead 1; vr_test_spawn_dist 96; impulse 241; vr_test_spawn_d`
        - On the Training Dummy → `vr_smallgibs_test_dummy`
        - [cmd] A Grunt's Ragdoll Ahead → `vr_ragdoll 1; vr_test_spawn 0; vr_test_spawn_dead 1; vr_test_spawn_dist 96; impulse 241; w`
        - [cmd] Blast Beside the Nearest Ragdoll → `vr_ragdoll_blast_test`
        - [cmd] Ragdoll and Prop Drawn Motion → `vr_drawn_motion_test 90 nearest`
        - [cmd] Drop the Nearest Prop on the Nearest Corpse → `vr_corpse_drop`
        - [cmd] Shotgun Blasts → `vr_smallgibs_test 1`
        - [cmd] Super Shotgun Blasts → `vr_smallgibs_test 2`
        - [cmd] Nails → `vr_smallgibs_test 3`
        - [cmd] Axe, Pommel, Sword, Punch → `vr_smallgibs_test 4`
        - [cmd] Quad Damage → `vr_smallgibs_test 5`
        - [cmd] Chainsaw a Second → `vr_smallgibs_test 6`
        - [cmd] Burst a Gib and a Head → `vr_smallgibs_test 7`
        - [cmd] Most Lying About → `vr_smallgibs_test 8`
        - [cmd] Held, Then Let Go → `vr_smallgibs_test 9`
        - [cmd] Pass Through the Body → `vr_smallgibs_test 10`
        - [cmd] Throw Gibs at a Wall → `vr_smallgibs_test 12`
        - [cmd] Step Up to the Wall Ahead → `vr_smallgibs_test 15`
        - [cmd] Flight by Situation → `vr_smallgibs_test 13`
        - [cmd] Blow Up a Crowd → `vr_smallgibs_test 14`
        - [cmd] List Small Gibs → `vr_smallgibs_test 11`
        - [cmd] Gib a Body Underfoot: a Blow → `vr_smallgibs_test 16`
        - [cmd] Gib a Body Underfoot: a Shot → `vr_smallgibs_test 17`
        - [cmd] Kill a Monster Underfoot → `vr_smallgibs_test 19`
        - [cmd] Drop a Monster's Backpack on You → `vr_smallgibs_test 20`
        - [cmd] Throw a Gib Made 1 s Ago → `vr_smallgibs_test 18`
        - Trace Small Gibs → `vr_smallgibs_trace`
        - — Burning Tests (developer 1: burning: ...) —
        - [cmd] Set It on Fire (a Torch's Blow) → `vr_burn_test 1`
        - [cmd] Set It on Fire (a Lava Nail) → `vr_burn_test 2`
        - [cmd] Set It on Fire (a Touch) → `vr_burn_test 3`
        - [cmd] Load Lava Nails → `vr_burn_test 5`
        - [cmd] A Nail Through a Torch's Flame → `vr_burn_test 6`
        - [cmd] Smash the Nearest Crate → `vr_burn_test 8`
        - [cmd] Burn the Nearest Crate Through → `vr_burn_test 9`
        - [cmd] A Lava Nail at the Nearest Piece → `vr_burn_test 11`
        - [cmd] Count the Pieces → `vr_burn_test 10`
        - [cmd] How It Burns → `vr_burn_test 4`
        - Torch Flames to Console → `vr_walltorch_debug`
        - — VR Calibration —
        - [cmd] Run the Calibration Here → `vr_setup here`
        - [cmd] Skip the Calibration Step → `vr_setup_skip`
        - [cmd] Check the Boards' Menu Paths → `vr_menu_path_check`
      - **Debug - Tests** [menu_vr 71] — 154 rows / 35 settings / 98 actions **LONG** (vr_menu.cpp:3676)
        - — Physics Stress —
        - Pile Size → `vr_test_pile_count`
        - Crates in the Wall → `vr_test_pile_crates`
        - [cmd] Pile of Rocks → `vr_physics_bigpile rocks`
        - [cmd] Pile of Bricks → `vr_physics_bigpile bricks`
        - [cmd] Wall of Crates → `vr_physics_bigpile crates`
        - [cmd] Mixed Pile → `vr_physics_bigpile mixed`
        - [cmd] Clear the Piles → `vr_physics_clearpiles`
        - [cmd] Physics Step Time → `vr_physics_steptime`
        - — Spatial Audio —
        - [cmd] Spatial Audio Tests → `vr_snd_test all`
        - [cmd] Spatial Audio Info → `vr_snd_info`
        - [cmd] 32 Sounds Around You → `vr_snd_bench_spawn 32`
        - [cmd] Spatial Audio Benchmark (12 s) → `vr_snd_bench 12 menu 8 400`
        - [cmd] Record the Mix (2 s) → `vr_snd_capture 2 menu`
        - [cmd] Record the Game-Time Mix (2 s) → `vr_snd_capture_game 2 menu`
        - [cmd] Five Explosions at Once → `vr_snd_burst weapons/r_exp3.wav 5`
        - [cmd] A Sound 45 Degrees Right → `vr_snd_play_dir misc/r_tele1.wav 45`
        - [cmd] A Sound 45 Degrees Left → `vr_snd_play_dir misc/r_tele1.wav -45`
        - [cmd] A 22 kHz Sound Behind You → `vr_snd_play_dir vr/torch_out.wav 180`
        - [cmd] A 22 kHz Sound Ahead → `vr_snd_play_dir vr/torch_out.wav 0`
        - [cmd] Save a Sound's Two Copies → `vr_snd_dump weapons/r_exp3.wav`
        - [cmd] Save the Sound Scene → `vr_snd_scene_obj`
        - — Ahead of You —
        - Thing → `vr_test_spawn`
        - Distance → `vr_test_spawn_dist`
        - Into the Main Hand → `vr_test_spawn_hold`
        - As a Corpse → `vr_test_spawn_dead`
        - Box Turned → `vr_test_spawn_yaw`
        - Box Tilted → `vr_test_spawn_tilt`
        - [cmd] Put It There → `impulse 241`
        - [cmd] A Knight's Ragdoll There → `vr_ragdoll 1; vr_test_spawn 5; vr_test_spawn_dead 1; impulse 241; wait5; vr_test_spawn_dea`
        - [cmd] An Ogre's Ragdoll There → `vr_ragdoll 1; vr_test_spawn 1; vr_test_spawn_dead 1; impulse 241; wait5; vr_test_spawn_dea`
        - [cmd] An Enforcer's Ragdoll There → `vr_ragdoll 1; vr_test_spawn 8; vr_test_spawn_dead 1; impulse 241; wait5; vr_test_spawn_dea`
        - [cmd] A Death Knight's Ragdoll There → `vr_ragdoll 1; vr_test_spawn 6; vr_test_spawn_dead 1; impulse 241; wait5; vr_test_spawn_dea`
        - [cmd] A Rottweiler's Ragdoll There → `vr_ragdoll 1; vr_test_spawn 7; vr_test_spawn_dead 1; impulse 241; wait5; vr_test_spawn_dea`
        - [cmd] A Scrag's Ragdoll There → `vr_ragdoll 1; vr_test_spawn 4; vr_test_spawn_dead 1; impulse 241; wait5; vr_test_spawn_dea`
        - [cmd] A Grunt's Ragdoll There → `vr_ragdoll 1; vr_test_spawn 0; vr_test_spawn_dead 1; impulse 241; wait5; vr_test_spawn_dea`
        - [cmd] Go to a Crowbar on a Crate → `vr_crates_goto crowbar`
        - [cmd] Go to the Next Crate → `vr_crates_goto`
        - [cmd] Crate Cover → `impulse 223`
        - [cmd] Can the Grunt See You? → `impulse 224`
        - — Enemy Shoves —
        - [cmd] Shove the Nearest Monster → `impulse 219`
        - [cmd] A Blast in 3 Seconds → `impulse 221`
        - — Chainsaw —
        - [cmd] A Chainsaw in Your Hand → `impulse 164`
        - [cmd] Take the Nearest Chainsaw → `impulse 229`
        - [cmd] Start the Engine → `impulse 230`
        - [cmd] Drop the Chainsaws → `impulse 220`
        - [cmd] Nearly Empty Tank → `impulse 227`
        - [cmd] Report the Chainsaws → `impulse 228`
        - [cmd] Chainsaw Fit → `vr_chainsaw_fit`
        - — Weapon Instances —
        - [cmd] List Your Weapons → `vr_test_weaponinst 0; impulse 120`
        - Holster → `vr_test_weaponinst_slot`
        - [cmd] Holster the Main Hand's → `vr_test_weaponinst 1; impulse 120`
        - [cmd] Draw into the Main Hand → `vr_test_weaponinst 2; impulse 120`
        - [cmd] Drop the Main Hand's → `vr_test_weaponinst 3; impulse 120`
        - [cmd] Take the Nearest Weapon → `vr_test_weaponinst 4; impulse 120`
        - [cmd] Hand Off to the Off Hand → `vr_test_weaponinst 5; impulse 120`
        - [cmd] Take It Back → `vr_test_weaponinst 6; impulse 120`
        - [cmd] Switch Hands → `vr_test_weaponinst 7; impulse 120`
        - — Climbing —
        - [cmd] Climbing Test Map → `map vrclimb`
        - [cmd] To the Jump Wall → `setpos -40 -310 24 0 0 0; noclip`
        - — Crowbar —
        - [cmd] A Crowbar in Your Hand → `impulse 167`
        - [cmd] Drop a Crowbar Ahead → `impulse 217`
        - [cmd] Take the Nearest Crowbar → `impulse 216`
        - [cmd] Report the Crowbars → `impulse 218`
        - [cmd] Hotspot Fit → `vr_hotspot_fit`
        - — Enemy Guns —
        - [cmd] A Grunt's Gun in Your Hand → `impulse 165`
        - [cmd] An Enforcer's Rifle in Your Hand → `impulse 166`
        - [cmd] Take the Nearest Enemy Gun → `impulse 212`
        - [cmd] Report the Enemy Guns → `impulse 213`
        - [cmd] One Shot Left → `impulse 214`
        - [cmd] Weapon Effects Test → `vr_weaponfx_test 1 3`
        - Print Weapon Effects → `vr_debug_weaponfx`
        - — Flung Props —
        - Fling Speed → `vr_test_fling_speed`
        - Fling At → `vr_test_fling_at`
        - Fling Away From It → `vr_test_fling_away`
        - [cmd] Two-Handed Throw → `developer 1; impulse 204`
        - [cmd] Gib in the Off Hand → `developer 1; impulse 252`
        - Which Gib → `vr_test_held_pick`
        - Real Gib → `vr_test_held_destroy`
        - [cmd] Fling the Nearest Prop → `impulse 232`
        - Throw Up Speed → `vr_test_throw_up_speed`
        - Throw Up From Your Body → `vr_test_throw_up_body`
        - [cmd] Throw the Nearest Prop Up → `developer 1; impulse 222`
        - — Thrown Axe —
        - Axe Throw → `vr_test_axe`
        - Axe Speed → `vr_test_axe_speed`
        - Throw Instead → `vr_test_axe_what`
        - Weapon Instead → `vr_test_axe_weapon`
        - Axe Hurts → `vr_test_axe_damage`
        - Axe At → `vr_test_axe_at`
        - Axe Range → `vr_test_axe_dist`
        - [cmd] Throw an Axe → `impulse 209`
        - [cmd] Hand on the Stuck Axe → `impulse 207`
        - [cmd] Report the Axes → `impulse 208`
        - [cmd] Slide the Nearest Prop → `vr_physics_fling nearest 150`
        - — At You —
        - Projectile → `vr_test_projectile`
        - From the Left → `vr_test_projectile_side`
        - [cmd] Fire at Me → `impulse 246`
        - [cmd] Make an Ogre Throw → `impulse 240`
        - Grenade Shot → `vr_test_grenade_shot`
        - [cmd] Shoot the Nearest Grenade → `developer 1; impulse 210`
        - Dud Distance → `vr_test_grenade_dist`
        - Dud Height → `vr_test_grenade_height`
        - Dud Is Yours → `vr_test_grenade_yours`
        - [cmd] Drop a Dud Ahead → `developer 1; impulse 211`
        - — Getting Hit —
        - [cmd] Hit Me From the Left → `vr_pain_test 15 90`
        - [cmd] Hit Me From the Right → `vr_pain_test 15 -90`
        - [cmd] Hit Me From Ahead → `vr_pain_test 15 0`
        - [cmd] Rocket From Ahead → `vr_pain_test 80 0`
        - [cmd] Fall → `vr_pain_test 10 none`
        - Print Hits → `vr_debug_pain`
        - — Grappling Hook —
        - [cmd] Report the Hooks → `impulse 239`
        - [cmd] Print the Ropes → `vr_grapple_rope_dump; vr_grapple_rope_draw_dump`
        - Load Stuck → `vr_grapple_test_stuck`
        - — Stamina —
        - [cmd] Deplete Stamina → `vr_stamina_set 0`
        - [cmd] Nearly Empty → `vr_stamina_set 0.1`
        - [cmd] Half Stamina → `vr_stamina_set 0.5`
        - [cmd] Quarter Stamina → `vr_stamina_set 0.25`
        - [cmd] Restore Stamina → `vr_stamina_set 1`
        - Hold Stamina → `vr_debug_stamina_hold`
        - Print Run Speed → `vr_debug_stamina_speed`
        - — Stuck in Walls —
        - Unstick → `vr_unstick`
        - [cmd] Stuck Info → `vr_stuck_info`
        - — Player Hitbox (Prototype) —
        - → link `Player Hitbox Settings` to Player Hitbox [cross-link]
        - → link `Monster Hitbox Settings` to Monster Hitbox [cross-link]
        - [cmd] Hitbox Stats → `vr_hull_stats`
        - [cmd] Hitbox Approach → `vr_hull_approach`
        - [cmd] Hitbox Bench → `vr_hull_bench`
        - [cmd] Shots Hit Test → `vr_hull_hittest`
        - [cmd] Hitbox Leaf → `vr_hull_leafdebug`
        - [cmd] Hitbox Probe → `vr_hull_probe`
        - [cmd] Shots Hit Test → `vr_hull_hittest`
        - [cmd] Random Walk (60 s) → `god; notarget; vr_hull_walktest 60`
        - — Dialogs —
        - [cmd] New Game Confirmation (3 s) → `vr_test_dialog 3 0`
        - — Cheats —
        - [cmd] God Mode → `god`
        - [cmd] Quad Damage → `impulse 255`
        - [cmd] All Weapons → `impulse 9`

## 8. Full vr cvar table (1,681 rows, the removed ones taken out; tier A/B/C/C?/D/R; read sites exclude vr_cvars.*, menu UI and comments)

| cvar | default | saved | tier | categories | menu page(s) | code/QC read sites | test-only reads | shipped cfg | inc line |
|---|---|---|---|---|---|---|---|---|---|
| `vr_gameplayfix_droptofloor` | `0` | no | D | gameplay | — | 1 | 0 | quakevr.cfg | 6 |
| `vr_gameplayfix_touchsolids` | `0` | no | D | gameplay | — | 1 | 0 | quakevr.cfg | 7 |
| `vr_gameplayfix_missilesize` | `0` | no | D | gameplay | — | 1 | 0 | quakevr.cfg | 8 |
| `vr_hull_width` | `16` | yes | D | gameplay | Player Hitbox | 17 | 0 |  | 9 |
| `vr_hull_method` | `1` | yes | D | gameplay | Player Hitbox | 6 | 0 |  | 10 |
| `vr_hull_ent_width` | `-1` | yes | D | gameplay | Player Hitbox | 2 | 0 |  | 11 |
| `vr_hull_brushmodels` | `1` | yes | D | gameplay | Player Hitbox | 1 | 0 |  | 12 |
| `vr_hull_monsters` | `1` | yes | D | gameplay | Player Hitbox | 1 | 0 |  | 13 |
| `vr_hull_players` | `1` | yes | D | gameplay,multiplayer | Player Hitbox | 1 | 0 |  | 14 |
| `vr_hull_boxes` | `1` | yes | D | gameplay | Player Hitbox | 1 | 0 |  | 15 |
| `vr_hull_hit_width` | `24` | yes | D | gameplay | Player Hitbox | 2 | 0 |  | 16 |
| `vr_hull_hit_head` | `1` | yes | D | gameplay | Player Hitbox | 2 | 0 |  | 17 |
| `vr_mhull` | `1` | yes | D | gameplay | Monster Hitbox | 7 | 0 |  | 18 |
| `vr_mhull_ents` | `1` | yes | D | gameplay | Monster Hitbox | 3 | 0 |  | 19 |
| `vr_mhull_ledges` | `1` | yes | D | gameplay | Monster Hitbox | 2 | 0 |  | 20 |
| `vr_mhull_army` | `-1` | yes | D | gameplay | Monster Hitbox | 2 | 0 |  | 21 |
| `vr_mhull_dog` | `-1` | yes | D | gameplay | Monster Hitbox | 2 | 0 |  | 22 |
| `vr_mhull_ogre` | `-1` | yes | D | gameplay | Monster Hitbox | 2 | 0 |  | 23 |
| `vr_mhull_knight` | `-1` | yes | D | gameplay | Monster Hitbox | 2 | 0 |  | 24 |
| `vr_mhull_hknight` | `-1` | yes | D | gameplay | Monster Hitbox | 2 | 0 |  | 25 |
| `vr_mhull_zombie` | `-1` | yes | D | gameplay | Monster Hitbox | 2 | 0 |  | 26 |
| `vr_mhull_wizard` | `-1` | yes | D | gameplay | Monster Hitbox | 2 | 0 |  | 27 |
| `vr_mhull_demon` | `-1` | yes | D | gameplay | Monster Hitbox | 2 | 0 |  | 28 |
| `vr_mhull_shambler` | `-1` | yes | D | gameplay | Monster Hitbox | 2 | 0 |  | 29 |
| `vr_mhull_shalrath` | `-1` | yes | D | gameplay | Monster Hitbox | 2 | 0 |  | 30 |
| `vr_mhull_enforcer` | `-1` | yes | D | gameplay | Monster Hitbox | 2 | 0 |  | 31 |
| `vr_mhull_fish` | `-1` | yes | D | gameplay | Monster Hitbox | 2 | 0 |  | 32 |
| `vr_mhull_tarbaby` | `-1` | yes | D | gameplay | Monster Hitbox | 2 | 0 |  | 33 |
| `vr_compat_muzzle` | `1` | yes | C? | gameplay | — | 1 | 0 |  | 34 |
| `vr_gameplayfix_tossfall` | `0` | no | D | gameplay | — | 1 | 0 | quakevr.cfg | 35 |
| `vr_pickup_scale` | `1` | no | D | gameplay | — | 2 | 0 | quakevr.cfg | 36 |
| `vr_item_float_height` | `26` | yes | C? | gameplay | — | 2 | 0 |  | 37 |
| `vr_unstick` | `0` | yes | D | controls,dev | Debug - Tests | 6 | 0 | quakevr.cfg | 38 |
| `vr_hit_precise` | `1` | yes | C | gameplay | Damage and Knockback | 7 | 0 |  | 42 |
| `vr_hit_tolerance_guns` | `4` | yes | C | gameplay | Damage and Knockback | 1 | 0 |  | 43 |
| `vr_hit_tolerance_grapple` | `2` | yes | C | gameplay | Damage and Knockback | 1 | 0 |  | 44 |
| `vr_hit_tolerance_melee` | `6` | yes | C | gameplay | Damage and Knockback | 1 | 0 |  | 45 |
| `vr_hit_tolerance_thrown` | `2` | yes | C | gameplay | Damage and Knockback | 1 | 0 |  | 46 |
| `vr_debug_hits` | `0` | no | D | dev | Debug - Views | 6 | 0 |  | 47 |
| `vr_throw_window` | `0.12` | yes | D | graphics | Throwing and Physics | 1 | 0 |  | 53 |
| `vr_throw_lookahead` | `0.01` | yes | C? | gameplay | — | 1 | 0 |  | 54 |
| `vr_throw_peak_span` | `0.017` | yes | C? | gameplay | — | 1 | 0 |  | 55 |
| `vr_throw_lever_arm` | `0.1` | yes | D | gameplay | Throwing and Physics | 1 | 0 |  | 56 |
| `vr_throw_dir_lookback` | `0.04` | yes | D | gameplay | Throwing and Physics | 1 | 0 |  | 57 |
| `vr_throw_ang_threshold` | `6` | yes | C? | gameplay | — | 1 | 0 |  | 58 |
| `vr_throw_ang_factor` | `0.7` | yes | C? | gameplay | — | 1 | 0 |  | 59 |
| `vr_throw_spin_from_pose` | `1` | yes | D | gameplay | Throwing and Physics | 1 | 0 |  | 66 |
| `vr_angvel_frame` | `-1` | yes | D | controls,gameplay | Throwing and Physics | 1 | 0 |  | 72 |
| `vr_angvel_local_pitch` | `-57.5` | yes | C? | controls,gameplay | — | 1 | 0 |  | 75 |
| `vr_debug_angvel` | `0` | no | D | controls,dev | Debug - Logging | 2 | 0 |  | 78 |
| `vr_throw_pitch` | `0` | yes | D | gameplay | Throwing and Physics | 2 | 0 |  | 79 |
| `vr_throw_gravity` | `9.81` | yes | B | gameplay | Throwing and Physics; VR Settings | 6 | 0 |  | 80 |
| `vr_debug_throw` | `0` | no | D | dev | Debug - Logging | 9 | 0 |  | 81 |
| `vr_debug_axestick` | `0` | no | D | controls,dev | Debug - Logging | 12 | 0 |  | 82 |
| `vr_debug_missiles` | `0` | no | D | dev | Debug - Logging | 1 | 0 |  | 83 |
| `vr_debug_shots` | `0` | no | D | dev | Debug - Logging | 17 | 0 |  | 84 |
| `vr_debug_damage_numbers` | `0` | no | D | dev | Debug - Views | 1 | 0 |  | 85 |
| `vr_throw_release` | `1` | yes | D | gameplay | Throwing and Physics | 2 | 0 |  | 90 |
| `vr_throw_release_drop` | `0.3` | yes | C? | gameplay | — | 1 | 0 |  | 91 |
| `vr_throw_release_floor` | `0.35` | yes | C? | gameplay | — | 1 | 0 |  | 92 |
| `vr_throw_release_speed` | `1.5` | yes | C? | gameplay | — | 1 | 0 |  | 93 |
| `vr_throw_grab_press` | `0.7` | yes | C? | controls,gameplay | — | 2 | 0 |  | 94 |
| `vr_throw_gain_max` | `1.5` | yes | D | gameplay | Throwing and Physics | 1 | 0 |  | 98 |
| `vr_throw_gain_lo` | `1.5` | yes | C? | gameplay | — | 1 | 0 |  | 99 |
| `vr_throw_gain_hi` | `6` | yes | C? | gameplay | — | 1 | 0 |  | 100 |
| `vr_throw_weight_influence` | `0.25` | yes | C? | gameplay | — | 1 | 0 |  | 101 |
| `vr_throw_mass_model` | `1` | yes | D | gameplay | Throwing and Physics | 9 | 0 |  | 110 |
| `vr_throw_max_speed` | `28` | yes | D | gameplay | Throwing and Physics | 2 | 0 |  | 111 |
| `vr_throw_mass_light` | `1.5` | yes | D | graphics | Throwing and Physics | 2 | 0 |  | 112 |
| `vr_throw_mass_exp` | `0.9` | yes | D | gameplay | Throwing and Physics | 2 | 0 |  | 113 |
| `vr_throw_mass_knee` | `0.7` | yes | D | gameplay | Throwing and Physics | 2 | 0 |  | 114 |
| `vr_throw_2h_strength` | `2` | yes | D | gameplay | Throwing and Physics | 2 | 0 |  | 115 |
| `vr_throw_flick_mass` | `2.5` | yes | D | gameplay | Throwing and Physics | 3 | 0 |  | 116 |
| `vr_throw_wrist_dist` | `0.07` | yes | D | gameplay | Throwing and Physics | 1 | 0 |  | 117 |
| `vr_throw_spin_mass_exp` | `1` | yes | D | gameplay | Throwing and Physics | 2 | 0 |  | 122 |
| `vr_throw_hitbox` | `6` | yes | D | gameplay | Throwing and Physics | 1 | 0 |  | 123 |
| `vr_throw_hit_min_speed` | `200` | yes | D | gameplay | Throwing and Physics | 2 | 0 |  | 124 |
| `vr_button_weapon` | `1` | yes | D | controls,gameplay | Throwing and Physics | 1 | 0 |  | 126 |
| `vr_button_weapon_reach` | `6` | yes | D | controls,gameplay | Throwing and Physics | 1 | 0 |  | 127 |
| `vr_button_prop` | `1` | yes | D | controls,gameplay | Throwing and Physics | 1 | 0 |  | 128 |
| `vr_button_prop_reach` | `6` | yes | D | controls,gameplay | Throwing and Physics | 1 | 0 |  | 129 |
| `vr_button_throw` | `1` | yes | D | controls,gameplay | Throwing and Physics | 1 | 0 |  | 130 |
| `vr_button_throw_speed` | `150` | yes | D | controls,gameplay | Throwing and Physics | 1 | 0 |  | 131 |
| `vr_debug_wallbuttons` | `0` | no | D | controls,dev | Debug - Logging | 4 | 0 |  | 132 |
| `vr_throw_hit_top` | `15` | yes | D | gameplay | Throwing and Physics | 2 | 0 |  | 140 |
| `vr_throw_hit_ramp` | `0.5` | yes | D | gameplay | Throwing and Physics | 1 | 0 |  | 141 |
| `vr_throw_hit_ramp_floor` | `0.6` | yes | D | gameplay | Throwing and Physics | 1 | 0 |  | 142 |
| `vr_throw_2h_nomelee` | `0.3` | yes | D | gameplay | Throwing and Physics | 3 | 0 |  | 147 |
| `vr_throw_2h_melee_immune` | `0.35` | yes | D | gameplay | Throwing and Physics | 2 | 0 |  | 148 |
| `vr_gib_letgo_spare` | `0.2` | yes | D | gameplay | Throwing and Physics | 1 | 0 |  | 151 |
| `vr_gib_burst_on_thrower` | `0` | yes | D | gameplay | Throwing and Physics | 1 | 0 |  | 152 |
| `vr_axestick` | `1` | yes | D | controls,gameplay | Throwing and Physics | 5 | 0 |  | 155 |
| `vr_axestick_speed` | `2.5` | yes | D | controls,gameplay | Throwing and Physics | 1 | 0 |  | 156 |
| `vr_axestick_angle` | `45` | yes | D | controls,gameplay | Throwing and Physics | 1 | 0 |  | 157 |
| `vr_axestick_incidence` | `65` | yes | D | controls,gameplay | Throwing and Physics | 2 | 0 |  | 158 |
| `vr_axestick_leniency` | `1` | yes | D | controls,gameplay | Throwing and Physics | 1 | 0 |  | 159 |
| `vr_axestick_depth` | `5` | yes | D | controls,gameplay | Throwing and Physics | 1 | 0 |  | 160 |
| `vr_axestick_bleed` | `5` | yes | D | controls,gameplay | Throwing and Physics | 1 | 0 |  | 161 |
| `vr_axestick_tug` | `0.3` | yes | D | controls,gameplay | Throwing and Physics | 1 | 0 |  | 162 |
| `vr_axestick_metal` | `0` | yes | D | controls,gameplay | Throwing and Physics | 2 | 0 |  | 163 |
| `vr_throw_assist` | `1` | yes | D | gameplay | Throwing and Physics | 1 | 0 |  | 167 |
| `vr_throw_assist_cone` | `15` | yes | D | gameplay | Throwing and Physics | 1 | 0 |  | 168 |
| `vr_throw_assist_full` | `4` | yes | C? | gameplay | — | 1 | 0 |  | 169 |
| `vr_throw_assist_strength` | `0.35` | yes | D | gameplay | Throwing and Physics | 1 | 0 |  | 170 |
| `vr_throw_assist_range` | `1200` | yes | C? | gameplay | — | 1 | 0 |  | 171 |
| `vr_throw_assist_speed` | `0.15` | yes | C? | gameplay | — | 1 | 0 |  | 172 |
| `vr_throw_restitution` | `0.25` | yes | D | gameplay | Throwing and Physics | 3 | 0 |  | 175 |
| `vr_throw_friction` | `0.6` | yes | D | gameplay | Throwing and Physics | 3 | 0 | vr_defaults.cfg | 176 |
| `vr_throw_spin_max` | `20` | yes | D | gameplay | Throwing and Physics | 5 | 0 |  | 177 |
| `vr_throw_spin_drag` | `0.3` | yes | D | gameplay | Throwing and Physics | 2 | 0 |  | 178 |
| `vr_throw_spin_align` | `8` | yes | D | gameplay | Throwing and Physics | 1 | 0 |  | 182 |
| `vr_debug_spin_align` | `0` | no | D | dev | Debug - Logging | 1 | 0 |  | 183 |
| `vr_bindings_version` | `0` | yes | D | controls,gameplay | — | 2 | 0 |  | 186 |
| `vr_mock_swing` | `0` | no | D | gameplay | — | 4 | 0 |  | 189 |
| `vr_mock_swing_both` | `0` | no | D | gameplay,multiplayer | — | 1 | 0 |  | 191 |
| `vr_mock_grip_velocity` | `0` | no | D | gameplay | — | 1 | 0 |  | 194 |
| `vr_mock_angvel_local` | `0` | no | D | controls,gameplay | — | 2 | 0 |  | 198 |
| `vr_fixed_frames` | `0` | no | D | gameplay | — | 4 | 0 |  | 201 |
| `vr_fixed_frames_rate` | `72` | no | D | gameplay | — | 1 | 0 |  | 205 |
| `vr_mock_fast` | `0` | no | D | gameplay | — | 3 | 0 |  | 209 |
| `vr_box3d_substeps` | `4` | yes | C? | gameplay | — | 2 | 0 |  | 214 |
| `vr_box3d_threads` | `1` | yes | D | dev | Debug - Profiling and Memory | 1 | 0 |  | 215 |
| `vr_box3d_threads_bodies` | `150` | yes | D | dev | Debug - Profiling and Memory | 1 | 0 |  | 216 |
| `vr_box3d_workers` | `4` | yes | D | dev | Debug - Profiling and Memory | 1 | 0 |  | 217 |
| `vr_box3d_player_push` | `1` | yes | C? | gameplay,multiplayer | — | 1 | 0 |  | 218 |
| `vr_box3d_hand_push_speed` | `3` | yes | C? | gameplay | — | 1 | 0 |  | 219 |
| `vr_box3d_hand_push` | `1` | yes | C? | gameplay | — | 2 | 0 |  | 220 |
| `vr_box3d_hand_push_fist` | `1` | yes | C | gameplay | Carrying | 2 | 0 |  | 221 |
| `vr_box3d_hand_props` | `1` | yes | C | gameplay | Carrying | 2 | 0 |  | 222 |
| `vr_box3d_weapon_push` | `1` | yes | C | gameplay | Carrying | 2 | 0 |  | 223 |
| `vr_box3d_hand_hold_mass` | `20` | yes | C | gameplay | Carrying | 1 | 0 |  | 224 |
| `vr_box3d_throw_grace` | `0.2` | yes | C | gameplay | Carrying | 1 | 0 |  | 225 |
| `vr_box3d_throw_grace_speed` | `0.75` | yes | C | gameplay | Carrying | 1 | 0 |  | 226 |
| `vr_box3d_throw_grace_body` | `1` | yes | C | gameplay | Carrying | 1 | 0 |  | 227 |
| `vr_box3d_hand_mass` | `3` | yes | C | gameplay | Carrying | 4 | 0 |  | 228 |
| `vr_box3d_push_force` | `200` | yes | C | gameplay | Carrying | 2 | 0 |  | 229 |
| `vr_box3d_weapon_arm_mass` | `1` | yes | C | gameplay | Carrying | 3 | 0 |  | 230 |
| `vr_shot_push` | `1` | yes | D | gameplay | Throwing and Physics | 1 | 0 |  | 232 |
| `vr_shot_push_pellet` | `6` | yes | D | gameplay | Throwing and Physics | 1 | 0 |  | 233 |
| `vr_shot_push_nail` | `12` | yes | D | gameplay | Throwing and Physics | 1 | 0 |  | 234 |
| `vr_shot_push_supernail` | `24` | yes | D | gameplay | Throwing and Physics | 1 | 0 |  | 235 |
| `vr_shot_push_lightning` | `6` | yes | D | graphics | Throwing and Physics | 1 | 0 |  | 236 |
| `vr_shot_push_speed` | `10` | yes | D | gameplay | Throwing and Physics | 1 | 0 |  | 237 |
| `vr_box3d_player_radius` | `15` | yes | D | gameplay,multiplayer | Player Hitbox | 2 | 0 |  | 238 |
| `vr_box3d_player_push_speed` | `2.5` | yes | C? | gameplay,multiplayer | — | 2 | 0 |  | 239 |
| `vr_box3d_monster_push_speed` | `3` | yes | D | gameplay | Player Hitbox | 1 | 0 |  | 240 |
| `vr_box3d_player_stand` | `1` | yes | D | gameplay,multiplayer | Player Hitbox | 3 | 0 |  | 241 |
| `vr_box3d_player_slope` | `30` | yes | D | gameplay,multiplayer | Player Hitbox | 1 | 0 |  | 242 |
| `vr_box3d_player_mass` | `80` | yes | D | gameplay,multiplayer | Player Hitbox | 3 | 0 |  | 243 |
| `vr_box3d_player_jump_push` | `0.25` | yes | D | gameplay,multiplayer | Player Hitbox | 2 | 0 |  | 244 |
| `vr_box3d_player_shove` | `0.6` | yes | D | gameplay,multiplayer | Player Hitbox | 2 | 0 |  | 245 |
| `vr_box3d_player_shape` | `1` | yes | D | gameplay,multiplayer | Player Hitbox | 5 | 0 |  | 246 |
| `vr_box3d_shot_shape` | `1` | yes | D | gameplay | Player Hitbox | 4 | 0 |  | 247 |
| `vr_box3d_player_unstick` | `1` | yes | D | controls,gameplay,multiplayer | Player Hitbox | 2 | 0 |  | 248 |
| `vr_box3d_player_hold` | `1` | yes | D | gameplay,multiplayer | Player Hitbox | 3 | 0 |  | 249 |
| `vr_box3d_mesh_junctions` | `1` | no | D | gameplay | — | 3 | 0 |  | 250 |
| `vr_debug_box3d` | `0` | no | D | dev | Debug - Logging | 39 | 0 |  | 251 |
| `vr_physsound` | `2` | yes | C | audio | Carrying | 7 | 0 |  | 253 |
| `vr_physsound_impact` | `1` | yes | C | audio | Carrying | 1 | 0 |  | 254 |
| `vr_physsound_min_speed` | `1.5` | yes | C | audio | Carrying | 3 | 0 |  | 255 |
| `vr_physsound_full_speed` | `10` | yes | C | audio | Carrying | 1 | 0 |  | 256 |
| `vr_physsound_interval` | `0.12` | yes | C | audio | Carrying | 1 | 0 |  | 257 |
| `vr_physsound_scrape` | `0.8` | yes | C | audio | Carrying | 2 | 0 |  | 258 |
| `vr_physsound_scrape_min` | `0.3` | yes | C | audio | Carrying | 1 | 0 |  | 259 |
| `vr_physsound_scrape_full` | `3` | yes | C | audio | Carrying | 1 | 0 |  | 260 |
| `vr_physsound_grab` | `0.7` | yes | C | audio | Carrying; Climbing | 1 | 0 |  | 261 |
| `vr_debug_net` | `0` | no | D | dev | Debug - Logging | 4 | 0 |  | 262 |
| `vr_debug_physsound` | `0` | no | D | audio,dev | Debug - Logging | 1 | 0 |  | 263 |
| `vr_snd_spatial` | `1` | yes | C | audio | Sound | 3 | 0 |  | 265 |
| `vr_snd_hrtf` | `1` | yes | C | audio | Sound | 1 | 0 |  | 266 |
| `vr_snd_hrtf_interp` | `1` | yes | C | audio | Sound | 1 | 1 |  | 267 |
| `vr_snd_hrtf_gain` | `1.5` | yes | C | audio | Sound | 1 | 1 |  | 268 |
| `vr_snd_hrtf_sofa` | `` | yes | C? | audio | — | 1 | 0 |  | 269 |
| `vr_snd_fullband` | `1` | yes | C | audio | Sound | 1 | 3 |  | 270 |
| `vr_snd_antialias` | `1` | yes | C | audio | Sound | 1 | 3 |  | 271 |
| `vr_snd_voices` | `32` | yes | C | audio | Sound | 2 | 0 |  | 272 |
| `vr_snd_limiter` | `1` | yes | C | audio | Sound | 7 | 0 |  | 273 |
| `vr_snd_limiter_ceiling` | `-1` | yes | C | audio | Sound | 3 | 0 |  | 274 |
| `vr_snd_limiter_release` | `0.15` | yes | C? | audio | — | 2 | 0 |  | 275 |
| `vr_snd_frame` | `256` | yes | C | audio | Sound | 1 | 1 |  | 276 |
| `vr_snd_reverb_beside` | `1` | no | D | audio | — | 1 | 0 |  | 277 |
| `vr_snd_occlusion` | `0.8` | yes | C | audio | Sound | 1 | 1 |  | 278 |
| `vr_snd_occlusion_samples` | `8` | yes | C | audio | Sound | 1 | 2 |  | 279 |
| `vr_snd_occlusion_radius` | `0.5` | yes | C | audio | Sound | 1 | 2 |  | 280 |
| `vr_snd_air` | `1` | yes | C | audio | Sound | 1 | 0 |  | 281 |
| `vr_snd_falloff` | `0.75` | yes | C | audio | Sound | 1 | 0 |  | 282 |
| `vr_snd_reverb` | `0.5` | yes | C | audio | Sound | 1 | 0 |  | 283 |
| `vr_snd_reverb_quality` | `1` | yes | C | audio | Sound | 2 | 0 |  | 284 |
| `vr_snd_reverb_interval` | `0.25` | yes | C | audio | Sound | 1 | 0 |  | 285 |
| `vr_snd_hands` | `1` | yes | C | audio | Sound | 1 | 0 |  | 286 |
| `vr_snd_follow` | `1` | yes | C | audio | Sound | 2 | 0 |  | 287 |
| `vr_snd_doppler` | `1` | yes | C | audio | Sound | 1 | 0 |  | 288 |
| `vr_snd_nearfield` | `1.2` | yes | C | audio | Sound | 1 | 1 |  | 289 |
| `vr_debug_snd` | `0` | no | D | dev | Debug - Logging | 2 | 0 |  | 290 |
| `vr_mirror` | `1` | yes | B | graphics | Body and Display; Recording | 4 | 0 |  | 293 |
| `vr_window_view` | `0` | yes | C | graphics | Recording | 1 | 0 |  | 298 |
| `vr_window_smooth` | `0.15` | yes | C | graphics | Recording | 1 | 0 |  | 299 |
| `vr_window_level` | `0` | yes | C | graphics | Recording | 1 | 0 |  | 300 |
| `vr_window_zoom` | `1.2` | yes | C | graphics | Recording | 1 | 0 |  | 301 |
| `vr_spectator_fov` | `90` | yes | C | graphics,multiplayer | Recording | 1 | 0 |  | 302 |
| `vr_spectator_scale` | `0.75` | yes | C | graphics,multiplayer | Recording | 1 | 0 |  | 303 |
| `vr_spectator_rate` | `60` | yes | C | graphics,multiplayer | Recording | 1 | 0 |  | 304 |
| `vr_spectator_aa` | `1` | yes | C | graphics,multiplayer | Recording | 1 | 0 |  | 305 |
| `vr_spectator_pos_smooth` | `0.05` | yes | C | graphics,multiplayer | Recording | 1 | 0 |  | 306 |
| `vr_timescale` | `1` | no | C | dev,graphics | Debug; Recording | 10 | 0 |  | 309 |
| `vr_slowmo_scale` | `0.25` | yes | C | graphics | Recording | 1 | 0 |  | 310 |
| `vr_timescale_ramp` | `0.3` | yes | C | graphics | Recording | 1 | 0 |  | 311 |
| `vr_timescale_sound` | `1` | yes | C | audio,graphics | Recording | 1 | 0 |  | 312 |
| `vr_timescale_wav` | `0` | yes | C | graphics | Recording | 7 | 0 |  | 313 |
| `vr_timescale_hand_speed` | `8` | yes | C | graphics | Recording | 1 | 0 |  | 314 |
| `vr_timescale_hand_spin` | `20` | yes | C | graphics | Recording | 1 | 0 |  | 315 |
| `vr_timescale_turn_realtime` | `1` | yes | C | graphics | Recording | 1 | 0 |  | 316 |
| `vr_timescale_move_realtime` | `0` | yes | C | graphics | Recording | 1 | 0 |  | 317 |
| `vr_sandevistan` | `0` | yes | C | graphics | Recording | 1 | 0 |  | 318 |
| `vr_sandevistan_missiles` | `1` | yes | C | gameplay | Bullet Time | 2 | 0 |  | 319 |
| `vr_bullettime_enabled` | `1` | yes | C | gameplay | Bullet Time | 3 | 0 |  | 322 |
| `vr_bullettime_scale` | `0.3` | yes | C | gameplay | Bullet Time | 1 | 0 |  | 323 |
| `vr_bullettime_duration` | `6` | yes | C | gameplay | Bullet Time | 2 | 0 |  | 324 |
| `vr_bullettime_recharge` | `20` | yes | C | gameplay | Bullet Time | 1 | 0 |  | 325 |
| `vr_bullettime_cooldown` | `2` | yes | C | gameplay | Bullet Time | 1 | 0 |  | 326 |
| `vr_bullettime_min` | `0.25` | yes | C | gameplay | Bullet Time | 2 | 0 |  | 327 |
| `vr_bullettime_sandevistan` | `0` | yes | C | gameplay | Bullet Time | 1 | 0 |  | 328 |
| `vr_bullettime_fx` | `1` | yes | C | graphics | Bullet Time; Recording | 1 | 0 |  | 329 |
| `vr_bullettime_fx_desat` | `0.7` | yes | C | gameplay | Bullet Time | 1 | 0 |  | 330 |
| `vr_bullettime_fx_vignette` | `0.5` | yes | C | comfort,gameplay | Bullet Time | 1 | 0 |  | 331 |
| `vr_bullettime_fx_tint` | `0.85 0.95 1.15` | yes | C? | gameplay | — | 1 | 0 |  | 332 |
| `vr_bullettime_button_radius` | `1.5` | yes | C | controls,gameplay | Bullet Time | 1 | 0 |  | 333 |
| `vr_bullettime_button_reach` | `1` | yes | C | controls,gameplay | Bullet Time | 1 | 0 |  | 334 |
| `vr_bullettime_button` | `1` | yes | C | controls,gameplay | Bullet Time | 1 | 0 |  | 335 |
| `vr_bullettime_tap` | `1` | yes | C | gameplay | Bullet Time | 1 | 0 |  | 337 |
| `vr_bullettime_tap_radius` | `15` | yes | C | gameplay | Bullet Time | 1 | 0 |  | 338 |
| `vr_bullettime_tap_speed` | `1` | yes | C | gameplay | Bullet Time | 1 | 0 |  | 339 |
| `vr_bullettime_tap_stop` | `0.5` | yes | C | gameplay | Bullet Time | 1 | 0 |  | 340 |
| `vr_bullettime_tap_window` | `0.25` | yes | C | graphics | Bullet Time | 1 | 0 |  | 341 |
| `vr_bullettime_tap_holding` | `1` | yes | C | gameplay | Bullet Time | 1 | 0 |  | 342 |
| `vr_bullettime_tap_twohanded` | `0` | yes | C | gameplay | Bullet Time | 1 | 0 |  | 343 |
| `vr_bullettime_trigger_cooldown` | `0.5` | yes | C | controls,gameplay | Bullet Time | 1 | 0 |  | 344 |
| `vr_bullettime_haptic` | `1` | yes | C | controls,gameplay | Bullet Time | 1 | 0 |  | 345 |
| `vr_bullettime_sound_on` | `items/inv1.wav` | yes | C? | audio | — | 1 | 0 |  | 346 |
| `vr_bullettime_sound_off` | `items/inv2.wav` | yes | C? | audio | — | 1 | 0 |  | 347 |
| `vr_bullettime_sound_denied` | `misc/menu2.wav` | yes | C? | audio | — | 1 | 0 |  | 348 |
| `vr_debug_bullettime` | `0` | no | D | dev | Debug - Logging | 8 | 0 |  | 349 |
| `vr_window_hud_mirror` | `1` | yes | C | HUD,graphics | Recording | 1 | 0 |  | 352 |
| `vr_window_hud_spectator` | `0` | yes | C | HUD,graphics,multiplayer | Recording | 1 | 0 |  | 353 |
| `vr_spectator_hide_hud_text` | `1` | yes | C | HUD,graphics,multiplayer | Recording | 1 | 0 |  | 356 |
| `vr_mirror_hide_hud_text` | `0` | yes | C | HUD,graphics | Recording | 2 | 0 |  | 357 |
| `vr_window_log` | `0` | no | D | graphics | — | 1 | 0 |  | 358 |
| `vr_highlights` | `0` | no | C | graphics | Recording | 11 | 0 |  | 361 |
| `vr_highlights_flash` | `0.2` | yes | C | graphics | Recording | 2 | 0 |  | 362 |
| `vr_highlights_beep` | `1` | yes | C | graphics | Recording | 2 | 0 |  | 363 |
| `vr_highlights_fps` | `60` | yes | C | graphics | Recording | 2 | 0 |  | 364 |
| `vr_highlights_min_score` | `2` | yes | C | graphics | Recording | 2 | 0 |  | 365 |
| `vr_highlights_multikill` | `3` | yes | C | graphics | Recording | 2 | 0 |  | 366 |
| `vr_debug_highlights` | `0` | no | D | dev,graphics | Debug - Logging | 1 | 0 |  | 367 |
| `vr_nearclip` | `0.1` | yes | B | graphics | Headset | 1 | 0 |  | 371 |
| `vr_nearclip_held` | `2` | yes | B | graphics | Headset | 1 | 0 |  | 375 |
| `vr_depth_float` | `1` | yes | B | graphics | Headset | 1 | 0 |  | 378 |
| `vr_xr_runtime` | `0` | yes | A | gameplay | Headset | 3 | 0 |  | 382 |
| `vr_xr_runtime_json` | `` | yes | C? | gameplay | — | 2 | 0 |  | 383 |
| `vr_render_scale` | `1` | yes | A | graphics | Headset | 4 | 0 |  | 384 |
| `vr_visibility_mask` | `1` | yes | B | graphics | Headset | 4 | 0 |  | 387 |
| `vr_mock_hidden_area` | `0` | no | D | gameplay | — | 1 | 0 |  | 389 |
| `vr_mock_eye_size` | `1024` | no | D | gameplay | — | 2 | 0 |  | 391 |
| `vr_mock_shake` | `0` | no | D | gameplay | — | 3 | 0 |  | 394 |
| `vr_mock_shake_turn` | `0` | no | D | gameplay | — | 2 | 0 |  | 395 |
| `vr_upscale` | `1` | yes | B | graphics | Headset | 7 | 0 |  | 397 |
| `vr_upscale_sharpness` | `0.5` | yes | B | graphics | Headset | 1 | 0 |  | 398 |
| `vr_upscale_radius` | `40` | yes | C? | graphics | — | 1 | 0 |  | 399 |
| `vr_upscale_sharpen_native` | `0` | yes | C? | graphics | — | 1 | 0 |  | 400 |
| `vr_foveated` | `0` | yes | B | graphics | Headset | 5 | 0 | vr_defaults.cfg | 401 |
| `vr_foveated_inner` | `0` | yes | C? | graphics | — | 1 | 0 |  | 402 |
| `vr_foveated_outer` | `0` | yes | C? | graphics | — | 1 | 0 |  | 403 |
| `vr_foveated_debug` | `0` | no | D | dev,graphics | Debug - Views | 2 | 0 |  | 404 |
| `vr_enabled` | `0` | no | A | gameplay | Headset | 6 | 0 | quakevr.cfg | 406 |
| `vr_stick_swap` | `0` | yes | A | comfort,controls,gameplay | VR Settings | 10 | 0 |  | 409 |
| `vr_fakevr_handroll` | `0` | no | D | gameplay | — | 1 | 0 |  | 414 |
| `vr_crosshair` | `0` | yes | A | HUD | Body and Display; Crosshair | 4 | 0 |  | 415 |
| `vr_crosshair_depth` | `0` | yes | B | HUD | Crosshair | 2 | 0 |  | 416 |
| `vr_crosshair_size` | `1` | yes | B | HUD | Body and Display; Crosshair | 1 | 0 |  | 417 |
| `vr_crosshair_alpha` | `0.85` | yes | B | HUD | Crosshair | 1 | 0 |  | 418 |
| `vr_crosshair_hue` | `-1` | yes | B | HUD | Colours; Crosshair | 1 | 0 |  | 419 |
| `vr_deadzone` | `25` | yes | B | comfort,controls,gameplay | Locomotion; VR Settings | 1 | 0 |  | 420 |
| `vr_controller_legacy_pose` | `1` | yes | C? | controls,gameplay | — | 6 | 0 |  | 424 |
| `vr_gunangle` | `39.5` | yes | A | gameplay | Hand/Gun Calibration; VR Settings | 6 | 0 | vr_bindings.cfg,vr_defaults.cfg | 425 |
| `vr_gunmodelpitch` | `7` | yes | C | gameplay | Hand/Gun Calibration | 4 | 0 |  | 426 |
| `vr_gunmodelscale` | `0.7` | yes | C | gameplay | Hand/Gun Calibration | 4 | 0 |  | 427 |
| `vr_gunmodely` | `1.3` | yes | C | gameplay | Hand/Gun Calibration | 4 | 0 |  | 428 |
| `vr_crosshairy` | `0` | yes | B | HUD | Crosshair | 2 | 0 |  | 429 |
| `vr_world_scale` | `1.25` | yes | A | comfort,gameplay | Body and Display; Player Calibration | 17 | 0 |  | 430 |
| `vr_floor_offset` | `-21` | yes | B | comfort,gameplay | Body and Display; Player Calibration | 7 | 0 |  | 431 |
| `vr_snap_turn` | `0` | yes | A | comfort,gameplay | Locomotion; VR Settings | 3 | 0 |  | 432 |
| `vr_enable_joystick_turn` | `1` | yes | B | comfort,controls,gameplay | Locomotion | 1 | 0 |  | 433 |
| `vr_turn_speed` | `3.25` | yes | A | comfort,gameplay | Locomotion; VR Settings | 2 | 0 |  | 434 |
| `vr_movement_mode` | `1` | yes | A | comfort,gameplay | Locomotion; VR Settings | 2 | 0 |  | 435 |
| `vr_hud_scale` | `0.025` | yes | B | HUD | Body and Display; Status Bar | 1 | 0 | vr_defaults.cfg | 436 |
| `vr_menu_scale` | `0.15` | yes | B | HUD | Body and Display; Menu | 3 | 0 | vr_defaults.cfg | 437 |
| `vr_melee_speed` | `3` | yes | C | gameplay | Melee | 6 | 0 |  | 438 |
| `vr_melee_punch_mult` | `1.25` | yes | C | gameplay | Melee | 1 | 0 |  | 439 |
| `vr_melee_wrist_speed` | `1.1` | yes | C? | gameplay | — | 1 | 0 |  | 440 |
| `vr_melee_pommel_wait` | `0.05` | yes | C | gameplay | Melee | 1 | 0 |  | 441 |
| `vr_melee_butt_run` | `0.45` | yes | C | gameplay | Melee | 2 | 0 |  | 442 |
| `vr_gunyaw` | `4` | yes | C | gameplay | Hand/Gun Calibration | 6 | 0 | vr_bindings.cfg,vr_defaults.cfg | 443 |
| `vr_gun_z_offset` | `-1` | yes | C | gameplay | Hand/Gun Calibration | 2 | 0 |  | 444 |
| `vr_sbar_mode` | `1` | yes | B | HUD | Body and Display; Status Bar | 1 | 0 |  | 445 |
| `vr_hud_mode` | `1` | yes | A | HUD | Body and Display; Status Bar; Wrist Gadget | 4 | 0 |  | 446 |
| `vr_gadget_arm` | `0` | yes | A | HUD | Body and Display; Wrist Gadget | 6 | 0 |  | 447 |
| `vr_gadget_scale` | `1` | yes | B | HUD | Wrist Gadget | 1 | 0 | vr_defaults.cfg | 448 |
| `vr_gadget_x` | `0` | yes | B | HUD | Wrist Gadget | 2 | 0 | vr_defaults.cfg | 449 |
| `vr_gadget_y` | `0` | yes | B | HUD | Wrist Gadget | 1 | 0 | vr_defaults.cfg | 450 |
| `vr_gadget_z` | `0` | yes | B | HUD | Wrist Gadget | 1 | 0 | vr_defaults.cfg | 451 |
| `vr_gadget_pitch` | `0` | yes | B | HUD | Wrist Gadget | 1 | 0 | vr_defaults.cfg | 452 |
| `vr_gadget_yaw` | `0` | yes | B | HUD | Wrist Gadget | 1 | 0 |  | 453 |
| `vr_gadget_roll` | `0` | yes | B | HUD | Wrist Gadget | 1 | 0 |  | 454 |
| `vr_gadget_tint_hue` | `30` | yes | C | HUD | Colours | 1 | 0 | vr_defaults.cfg | 455 |
| `vr_gadget_tint` | `0` | yes | C | HUD | Colours | 1 | 0 |  | 456 |
| `vr_gadget_screen_hue` | `-1` | yes | C | HUD | Colours | 8 | 0 |  | 457 |
| `vr_gadget_screen_brightness` | `1` | yes | C | HUD | Colours | 9 | 0 | vr_defaults.cfg | 458 |
| `vr_gadget_screen_background` | `1` | yes | C | HUD | Colours | 3 | 0 | vr_defaults.cfg | 459 |
| `vr_gadget_show_level` | `1` | yes | C | HUD | Screens | 1 | 0 |  | 460 |
| `vr_gadget_stamina` | `1` | yes | C | HUD | Screens; Stamina | 2 | 0 |  | 461 |
| `vr_gadget_light` | `1` | yes | C | HUD,graphics | Screens | 2 | 0 | vr_defaults.cfg | 462 |
| `vr_gadget_crt` | `1` | yes | C | HUD | Screens | 1 | 0 | vr_defaults.cfg | 463 |
| `vr_player_hue` | `128` | yes | C | HUD,multiplayer | Colours | 3 | 0 | vr_defaults.cfg | 464 |
| `vr_player_saturation` | `1` | yes | C | HUD,multiplayer | Colours | 5 | 0 |  | 465 |
| `vr_sight_hue` | `-1` | yes | C | HUD,graphics | Colours; Graphics - Models and Effects | 4 | 0 |  | 466 |
| `vr_sight_saturation` | `1` | yes | C | HUD | Colours | 2 | 0 | vr_defaults.cfg | 467 |
| `vr_notify_wrist` | `1` | yes | C | HUD | Screens | 2 | 0 |  | 468 |
| `vr_notify_wrist_time` | `8` | yes | C | HUD | Screens | 1 | 0 |  | 469 |
| `vr_notify_wrist_height` | `5` | yes | C | HUD | Screens | 1 | 0 |  | 470 |
| `vr_notify_wrist_alpha` | `0.7` | yes | C | HUD | Screens | 1 | 0 |  | 471 |
| `vr_messages_hologram` | `1` | yes | C | HUD | Screens | 2 | 0 |  | 472 |
| `vr_messages_hologram_time` | `5` | yes | C | HUD | Screens | 1 | 0 |  | 473 |
| `vr_messages_hologram_size` | `1` | yes | C | HUD | Screens | 1 | 0 |  | 474 |
| `vr_messages_hologram_height` | `1` | yes | C | HUD | Screens | 1 | 0 | vr_defaults.cfg | 475 |
| `vr_messages_hologram_effect` | `1` | yes | C | HUD | Screens | 1 | 0 |  | 476 |
| `vr_messages_hologram_only` | `0` | yes | C | HUD | Screens | 1 | 0 | vr_defaults.cfg | 477 |
| `vr_sbar_offset_x` | `-12` | yes | B | HUD | Status Bar | 1 | 0 |  | 478 |
| `vr_sbar_offset_y` | `1` | yes | B | HUD | Status Bar | 1 | 0 |  | 479 |
| `vr_sbar_offset_z` | `-3` | yes | B | HUD | Status Bar | 1 | 0 |  | 480 |
| `vr_sbar_offset_pitch` | `1` | yes | B | HUD | Status Bar | 1 | 0 |  | 481 |
| `vr_sbar_offset_yaw` | `1.6` | yes | B | HUD | Status Bar | 1 | 0 |  | 482 |
| `vr_sbar_offset_roll` | `-0.3` | yes | B | HUD | Status Bar | 1 | 0 |  | 483 |
| `vr_height_calibration` | `1.646099` | yes | A | comfort,gameplay | Body and Display | 14 | 0 |  | 484 |
| `vr_menu_distance` | `80` | yes | B | HUD | Body and Display; Menu | 1 | 0 | vr_defaults.cfg | 485 |
| `vr_menu_vr_style` | `1` | yes | C | HUD | Menu | 1 | 0 |  | 486 |
| `vr_menu_laser_hue` | `-1` | yes | C | HUD | Colours; Menu | 4 | 0 |  | 487 |
| `vr_menu_bigfont` | `1` | yes | C | HUD | Menu | 1 | 0 |  | 488 |
| `vr_menu_spacing` | `1.5` | yes | C | HUD | Menu | 1 | 0 | vr_defaults.cfg | 489 |
| `vr_menu_height` | `1.35` | yes | C | HUD | Menu | 1 | 0 | vr_defaults.cfg | 490 |
| `vr_menu_remember` | `1` | yes | C | HUD | Menu | 1 | 0 |  | 491 |
| `vr_menu_dropdown` | `4` | yes | C | HUD | Menu | 2 | 0 |  | 492 |
| `vr_menu_positions` | `` | yes | C? | HUD | — | 4 | 0 |  | 493 |
| `vr_checklist_hide_ticked` | `1` | yes | D | dev | Checklist | 2 | 0 |  | 494 |
| `vr_melee_dmg_multiplier` | `1.0` | yes | C | gameplay | Melee | 2 | 0 |  | 495 |
| `vr_melee_range_multiplier` | `1.0` | yes | C | gameplay | Melee | 2 | 0 |  | 496 |
| `vr_quad_melee_damage` | `1` | yes | C | gameplay | Melee | 1 | 0 |  | 499 |
| `vr_quad_melee_range` | `1` | yes | C | gameplay | Melee | 1 | 0 |  | 500 |
| `vr_lg_water` | `1` | yes | C | gameplay | Lightning Gun in Water | 3 | 0 |  | 505 |
| `vr_lg_water_self_damage` | `40` | yes | C | gameplay | Lightning Gun in Water | 1 | 0 |  | 506 |
| `vr_lg_water_damage` | `60` | yes | C | gameplay | Lightning Gun in Water | 1 | 0 |  | 507 |
| `vr_lg_water_radius` | `300` | yes | C | gameplay | Lightning Gun in Water | 3 | 0 |  | 508 |
| `vr_lg_water_falloff` | `1` | yes | C | gameplay | Lightning Gun in Water | 1 | 0 |  | 509 |
| `vr_lg_water_tick_damage` | `10` | yes | C | gameplay | Lightning Gun in Water | 1 | 0 |  | 510 |
| `vr_lg_water_flash` | `1` | yes | C | gameplay | Lightning Gun in Water | 1 | 0 |  | 511 |
| `vr_body_interactions` | `0` | yes | C | gameplay | Immersion | 1 | 0 |  | 512 |
| `vr_notes` | `1` | yes | D | dev | Debug | 1 | 0 |  | 513 |
| `vr_note_device` | `Virtual Desktop` | yes | D | gameplay | — | 2 | 0 |  | 514 |
| `vr_motion_armed` | `0` | no | D | dev | Motion Recorder | 5 | 0 |  | 516 |
| `vr_motion_category` | `0` | yes | D | dev | Motion Recorder | 4 | 0 |  | 517 |
| `vr_motion_detail` | `0` | yes | D | dev,graphics | Motion Recorder | 3 | 0 |  | 518 |
| `vr_motion_note` | `` | no | D | gameplay | — | 3 | 0 |  | 519 |
| `vr_motion_button` | `0` | yes | D | controls,dev | Motion Recorder | 1 | 0 |  | 520 |
| `vr_motion_preroll` | `0.5` | yes | D | dev | Motion Recorder | 2 | 0 |  | 521 |
| `vr_motion_tail` | `0.3` | yes | D | dev | Motion Recorder | 2 | 0 |  | 522 |
| `vr_motion_review_show` | `0` | yes | D | dev | Review Takes | 2 | 0 |  | 524 |
| `vr_motion_review_category` | `-1` | yes | D | dev | Review Takes | 2 | 0 |  | 525 |
| `vr_motion_review_speed` | `0.5` | yes | D | dev | Take | 1 | 0 |  | 526 |
| `vr_motion_relabel_category` | `0` | no | D | dev | Take | 7 | 0 |  | 527 |
| `vr_motion_relabel_detail` | `0` | no | D | dev,graphics | Take | 5 | 0 |  | 528 |
| `vr_memstats_log_gpu` | `0` | yes | D | dev | Debug - Profiling and Memory | 1 | 0 |  | 529 |
| `vr_memstats_log` | `60` | yes | D | dev | Debug - Profiling and Memory | 5 | 0 |  | 530 |
| `vr_gadget_fps` | `0` | yes | B | HUD,graphics | Graphics | 1 | 0 |  | 531 |
| `vr_lean_radius` | `12` | yes | B | comfort,gameplay | Locomotion | 1 | 0 | vr_defaults.cfg | 532 |
| `vr_lean_recenter` | `0.5` | yes | B | comfort,gameplay | Locomotion | 1 | 0 |  | 533 |
| `vr_lean_detect` | `1` | yes | B | comfort,gameplay | Locomotion | 4 | 0 |  | 534 |
| `vr_roomscale_jump` | `1` | yes | B | comfort,gameplay | Locomotion | 1 | 0 |  | 535 |
| `vr_roomscale_jump_threshold` | `0.8` | yes | B | comfort,gameplay | Locomotion | 1 | 0 |  | 536 |
| `vr_roomscale_move_mult` | `1.0` | yes | B | comfort,gameplay | Locomotion; VR Settings | 2 | 0 |  | 537 |
| `vr_teleport_enabled` | `0` | yes | A | comfort,gameplay | Locomotion; VR Settings | 2 | 0 |  | 538 |
| `vr_teleport_range` | `400` | yes | B | comfort,gameplay | Locomotion; VR Settings | 2 | 0 |  | 539 |
| `vr_teleport_hue` | `-1` | yes | C | HUD,comfort | Colours | 1 | 0 |  | 540 |
| `vr_2h_mode` | `2` | yes | B | gameplay | Aiming; VR Settings; Weapon Offsets - Virtual Stock | 3 | 0 |  | 541 |
| `vr_2h_angle_threshold` | `0.65` | yes | C | gameplay | Aiming | 2 | 0 |  | 542 |
| `vr_virtual_stock_thresh` | `10` | yes | C | gameplay | Hotspots; Weapon Offsets - Virtual Stock | 2 | 0 |  | 543 |
| `vr_show_virtual_stock` | `0` | no | D | gameplay | Hotspots; Weapon Offsets - Virtual Stock | 1 | 0 |  | 544 |
| `vr_shoulder_offset_x` | `-1` | yes | C | gameplay | Hotspots | 3 | 0 |  | 545 |
| `vr_shoulder_offset_y` | `1.75` | yes | C | gameplay | Hotspots | 3 | 0 |  | 546 |
| `vr_shoulder_offset_z` | `16.0` | yes | C | gameplay | Hotspots | 3 | 0 |  | 547 |
| `vr_2h_virtual_stock_factor` | `0.5` | yes | C | gameplay | Aiming; Weapon Offsets - Virtual Stock | 1 | 0 |  | 548 |
| `vr_2h_sticky` | `1` | yes | C | controls,gameplay | Aiming | 1 | 0 |  | 554 |
| `vr_2h_sticky_fast` | `3.5` | yes | C | controls,gameplay | Aiming | 1 | 0 |  | 555 |
| `vr_2h_sticky_fast_from` | `1.5` | yes | C | controls,gameplay | Aiming | 1 | 0 |  | 556 |
| `vr_2h_sticky_fast_full` | `4` | yes | C | controls,gameplay | Aiming | 1 | 0 |  | 557 |
| `vr_2h_sticky_fast_hold` | `0.6` | yes | C | controls,gameplay | Aiming | 1 | 0 |  | 558 |
| `vr_debug_2h_grip` | `0` | no | D | dev | Debug - Logging | 15 | 0 |  | 559 |
| `vr_weight_spring_stiffness` | `1` | yes | C | gameplay | Aiming | 2 | 0 |  | 575 |
| `vr_weight_spring_damping` | `1` | yes | C | gameplay | Aiming | 2 | 0 |  | 576 |
| `vr_weight_spring_strength` | `1` | yes | C | gameplay | Aiming | 2 | 0 |  | 577 |
| `vr_weight_spring_sag` | `1` | yes | C | gameplay | Aiming | 4 | 0 |  | 578 |
| `vr_weight_spring_inertia` | `1` | yes | C | gameplay | Aiming | 2 | 0 |  | 579 |
| `vr_weight_spring_2h` | `4` | yes | C | gameplay | Aiming | 2 | 0 |  | 580 |
| `vr_weight_spring_roll` | `2.5` | yes | C | gameplay | Aiming | 2 | 0 |  | 581 |
| `vr_weight_spring_snap` | `60` | yes | C | gameplay | Aiming | 3 | 0 |  | 582 |
| `vr_weight_stamina` | `1` | yes | C | gameplay | Aiming | 1 | 0 |  | 583 |
| `vr_weight_stamina_from` | `0.5` | yes | C | gameplay | Aiming | 1 | 0 |  | 584 |
| `vr_weight_stamina_max` | `2.5` | yes | C | gameplay | Aiming | 2 | 0 |  | 585 |
| `vr_weight_stamina_curve` | `2` | yes | C | gameplay | Aiming | 1 | 0 |  | 586 |
| `vr_weight_stamina_add` | `15` | yes | C | gameplay | Aiming | 2 | 0 |  | 587 |
| `vr_weight_stamina_empty` | `15` | yes | C | gameplay | Aiming | 3 | 0 |  | 588 |
| `vr_stamina_speed` | `1` | yes | C | gameplay | Stamina | 1 | 0 |  | 589 |
| `vr_stamina_speed_min` | `0.5` | yes | C | gameplay | Stamina | 1 | 0 |  | 590 |
| `vr_debug_stamina_speed` | `0` | no | D | dev | Debug - Tests | 1 | 0 |  | 591 |
| `vr_weight_throw_mass` | `12` | yes | C? | gameplay | — | 1 | 0 |  | 592 |
| `vr_weight_damage_exp` | `0.4` | yes | C | gameplay | Weight and Damage | 2 | 0 |  | 597 |
| `vr_weight_damage_heavy` | `8` | yes | C | gameplay | Weight and Damage | 2 | 0 |  | 598 |
| `vr_weight_damage_light` | `1.5` | yes | C | graphics | Weight and Damage | 2 | 0 |  | 599 |
| `vr_weight_damage_min` | `0.5` | yes | C | gameplay | Weight and Damage | 2 | 0 |  | 600 |
| `vr_weight_damage_max` | `2.5` | yes | C | gameplay | Weight and Damage | 2 | 0 |  | 601 |
| `vr_weight_lenient` | `0.5` | yes | C | gameplay | Weight and Damage | 2 | 0 |  | 607 |
| `vr_weight_lenient_from` | `5` | yes | C | gameplay | Weight and Damage | 2 | 0 |  | 608 |
| `vr_weight_lenient_min` | `0.25` | yes | C | gameplay | Weight and Damage | 3 | 0 |  | 609 |
| `vr_weight_drop` | `1` | yes | D | gameplay | Weapon Weights | 2 | 0 |  | 615 |
| `vr_weight_drop_from` | `10` | yes | D | gameplay | Weapon Weights | 2 | 0 |  | 616 |
| `vr_weight_drop_speed` | `1900` | yes | D | gameplay | Weapon Weights | 2 | 0 |  | 617 |
| `vr_weight_drop_curve` | `1` | yes | D | gameplay | Weapon Weights | 1 | 0 |  | 618 |
| `vr_weight_drop_2h` | `1.5` | yes | D | gameplay | Weapon Weights | 1 | 0 |  | 619 |
| `vr_weight_drop_window` | `0.06` | yes | D | graphics | Weapon Weights | 1 | 0 |  | 620 |
| `vr_weight_drop_velocity` | `0.4` | yes | D | gameplay | Weapon Weights | 1 | 0 |  | 621 |
| `vr_debug_weight_drop` | `0` | no | D | dev | Debug - Logging | 2 | 0 |  | 622 |
| `vr_debug_weight_stamina` | `-1` | no | D | gameplay | — | 2 | 0 |  | 623 |
| `vr_debug_stamina_hold` | `0` | no | D | dev | Debug - Tests | 2 | 0 |  | 624 |
| `vr_debug_fatigue` | `0` | no | D | gameplay | — | 2 | 0 |  | 625 |
| `vr_debug_pain` | `0` | no | D | dev | Debug - Tests | 3 | 0 |  | 626 |
| `vr_debug_weight` | `0` | no | D | dev | Debug - Logging | 4 | 0 |  | 627 |
| `vr_props_version` | `50` | yes | D | gameplay | — | 3 | 0 |  | 628 |
| `vr_explobox_physics` | `1` | yes | C | gameplay | Carrying | 1 | 0 |  | 630 |
| `vr_explobox_impact` | `14` | yes | C | gameplay | Carrying | 1 | 0 |  | 631 |
| `vr_debris` | `1` | yes | C | gameplay | Rocks and Bricks | 14 | 0 |  | 633 |
| `vr_debris_chance` | `0.1` | yes | C | gameplay | Rocks and Bricks | 1 | 0 |  | 634 |
| `vr_debris_dark` | `0.3` | yes | C | gameplay | Rocks and Bricks | 1 | 0 |  | 635 |
| `vr_debris_corner` | `2.5` | yes | C | gameplay | Rocks and Bricks | 1 | 0 |  | 636 |
| `vr_debris_cluster` | `3` | yes | C | gameplay | Rocks and Bricks | 1 | 0 |  | 637 |
| `vr_debris_max` | `160` | yes | C | gameplay | Rocks and Bricks | 1 | 0 |  | 638 |
| `vr_debris_area_max` | `8` | yes | C | gameplay | Rocks and Bricks | 1 | 0 |  | 639 |
| `vr_debris_area_size` | `384` | yes | C? | gameplay | — | 1 | 0 |  | 640 |
| `vr_debris_spacing` | `56` | yes | C | gameplay | Rocks and Bricks | 1 | 0 |  | 641 |
| `vr_debris_size` | `0.15` | yes | C | gameplay | Rocks and Bricks | 1 | 0 |  | 642 |
| `vr_debris_rocks` | `1` | yes | C | gameplay | Rocks and Bricks | 1 | 0 |  | 643 |
| `vr_debris_bricks` | `1` | yes | C | gameplay | Rocks and Bricks | 1 | 0 |  | 644 |
| `vr_debris_seed` | `0` | yes | D | gameplay | Rocks and Bricks | 1 | 0 |  | 645 |
| `vr_debris_exclude` | `vrfiringrange vrclim` | yes | C? | gameplay | — | 2 | 0 |  | 646 |
| `vr_debris_edicts_left` | `2048` | yes | C? | gameplay | — | 2 | 0 |  | 647 |
| `vr_debug_debris` | `0` | no | D | dev | Debug - Logging | 6 | 0 |  | 648 |
| `vr_crates` | `1` | yes | C | gameplay | Crates | 7 | 0 |  | 651 |
| `vr_crates_chance` | `0.06` | yes | C | gameplay | Crates | 1 | 0 |  | 652 |
| `vr_crates_corner` | `4` | yes | C | gameplay | Crates | 1 | 0 |  | 653 |
| `vr_crates_max` | `24` | yes | C | gameplay | Crates | 1 | 0 |  | 654 |
| `vr_crates_spacing` | `224` | yes | C | gameplay | Crates | 1 | 0 |  | 655 |
| `vr_crates_stack` | `0.25` | yes | C | gameplay | Crates | 1 | 0 |  | 656 |
| `vr_crates_large` | `0.4` | yes | C | gameplay | Crates | 1 | 0 |  | 657 |
| `vr_crates_clearance` | `72` | yes | C | gameplay | Crates | 1 | 0 |  | 658 |
| `vr_crates_margin` | `56` | yes | C | gameplay | Crates | 1 | 0 |  | 659 |
| `vr_crates_seed` | `0` | yes | D | gameplay | Crates | 1 | 0 |  | 660 |
| `vr_crates_exclude` | `vrfiringrange vrclim` | yes | C? | gameplay | — | 2 | 0 |  | 661 |
| `vr_crate_health` | `75` | yes | C | gameplay | Crates | 1 | 0 |  | 662 |
| `vr_crate_impact` | `16` | yes | C | gameplay | Crates | 1 | 0 |  | 663 |
| `vr_crate_pieces` | `12` | yes | C | gameplay | Crates | 1 | 0 |  | 664 |
| `vr_crate_piece_max` | `48` | yes | C | gameplay | Crates | 2 | 0 |  | 665 |
| `vr_crate_piece_time` | `60` | yes | C | gameplay | Crates | 1 | 0 |  | 666 |
| `vr_crate_ammo` | `0.35` | yes | C | gameplay | Crates | 1 | 0 |  | 667 |
| `vr_crate_health_box` | `0.2` | yes | C | gameplay | Crates | 1 | 0 |  | 668 |
| `vr_crate_item_pop` | `3.5` | yes | C | gameplay | Crates | 1 | 0 |  | 669 |
| `vr_crate_crowbar` | `0.1` | yes | C | gameplay | Crates | 1 | 0 |  | 670 |
| `vr_crate_crowbar_max` | `4` | yes | C | gameplay | Crates | 1 | 0 |  | 671 |
| `vr_crate_sight` | `1` | yes | C | gameplay | Crates | 1 | 0 |  | 672 |
| `vr_crate_shield` | `2` | yes | C | gameplay | Crates | 1 | 0 |  | 673 |
| `vr_debug_crates` | `0` | no | D | dev | Debug - Logging | 11 | 0 |  | 674 |
| `vr_debug_item_sizes` | `0` | no | D | dev | Debug - Logging | 1 | 0 |  | 675 |
| `vr_walltorch` | `1` | yes | C | gameplay | Wall Torches | 8 | 0 |  | 677 |
| `vr_walltorch_pull` | `8` | yes | C | gameplay | Wall Torches | 1 | 0 |  | 678 |
| `vr_walltorch_reach` | `15` | yes | C | gameplay | Wall Torches | 1 | 0 |  | 679 |
| `vr_walltorch_grab_time` | `0.6` | yes | C | gameplay | Wall Torches | 1 | 0 |  | 680 |
| `vr_walltorch_forcegrab` | `1` | yes | C | gameplay | Wall Torches | 1 | 0 |  | 681 |
| `vr_walltorch_hits` | `5` | yes | C | gameplay | Wall Torches | 1 | 0 |  | 682 |
| `vr_walltorch_die_time` | `6` | yes | C | gameplay | Wall Torches | 2 | 0 |  | 683 |
| `vr_walltorch_damage` | `12` | yes | C | gameplay | Wall Torches | 1 | 0 |  | 684 |
| `vr_walltorch_relight` | `1` | yes | C | graphics | Wall Torches | 1 | 0 |  | 685 |
| `vr_walltorch_flame` | `1` | yes | C | gameplay | Wall Torches | 1 | 0 |  | 686 |
| `vr_walltorch_shadows` | `1` | yes | C | graphics | Wall Torches | 1 | 0 |  | 687 |
| `vr_walltorch_lean` | `1` | yes | C | comfort,gameplay | Burning | 1 | 0 |  | 689 |
| `vr_walltorch_flatten` | `1` | yes | C | gameplay | Burning | 1 | 0 |  | 690 |
| `vr_walltorch_inv_size` | `1.25` | yes | C | gameplay | Burning | 1 | 0 |  | 691 |
| `vr_walltorch_inv_light` | `1.3` | yes | C | graphics | Burning | 1 | 0 |  | 692 |
| `vr_walltorch_drips` | `5` | yes | C | gameplay | Burning | 2 | 0 |  | 693 |
| `vr_walltorch_smoke` | `1` | yes | C | gameplay | Burning | 1 | 0 |  | 694 |
| `vr_walltorch_smoke_alpha` | `0.45` | yes | C | gameplay | Burning | 1 | 0 |  | 695 |
| `vr_walltorch_inv_smoke` | `2.5` | yes | C | gameplay | Burning | 1 | 0 |  | 696 |
| `vr_walltorch_debug` | `0` | no | D | dev | Debug - Tools | 2 | 0 |  | 697 |
| `vr_burn_damage` | `4` | yes | C | gameplay | Burning | 2 | 0 |  | 699 |
| `vr_burn_time` | `3` | yes | C | gameplay | Burning | 1 | 0 |  | 700 |
| `vr_burn_flames` | `4` | yes | C | gameplay | Burning | 1 | 0 |  | 701 |
| `vr_burn_flames_max` | `7` | yes | C | gameplay | Burning | 2 | 0 |  | 702 |
| `vr_burn_spread` | `0.5` | yes | C | gameplay | Burning | 2 | 0 |  | 703 |
| `vr_burn_flame_size` | `0.8` | yes | C | gameplay | Burning | 1 | 0 |  | 704 |
| `vr_burn_flame_small` | `0.7` | yes | C | gameplay | Burning | 1 | 0 |  | 705 |
| `vr_burn_flame_tilt` | `12` | yes | C | gameplay | Burning | 1 | 0 |  | 706 |
| `vr_burn_corpses` | `1` | yes | C | gameplay | Burning | 2 | 0 |  | 707 |
| `vr_burn_corpse_time` | `8` | yes | C | gameplay | Burning | 2 | 0 |  | 708 |
| `vr_burn_corpse_damage` | `0` | yes | C | gameplay | Burning | 1 | 0 |  | 709 |
| `vr_burn_touch` | `0` | yes | C | gameplay | Burning | 1 | 0 |  | 710 |
| `vr_burn_lava_nails` | `1` | yes | C | gameplay | Burning | 1 | 0 |  | 711 |
| `vr_burn_crates` | `1` | yes | C | gameplay | Burning | 1 | 0 |  | 712 |
| `vr_burn_crate_time` | `10` | yes | C | gameplay | Burning | 1 | 0 |  | 713 |
| `vr_burn_crate_break` | `1` | yes | C | gameplay | Burning | 1 | 0 |  | 714 |
| `vr_burn_crate_spread` | `3` | yes | C | gameplay | Burning | 2 | 0 |  | 715 |
| `vr_burn_crate_gap` | `2` | yes | C | gameplay | Burning | 1 | 0 |  | 716 |
| `vr_burn_crate_flames` | `6` | yes | C | gameplay | Burning | 1 | 0 |  | 717 |
| `vr_burn_crate_flame_size` | `1.25` | yes | C | gameplay | Burning | 1 | 0 |  | 718 |
| `vr_burn_crate_char` | `1` | yes | C | gameplay | Burning | 1 | 0 |  | 719 |
| `vr_burn_pieces` | `1` | yes | C | gameplay | Burning | 1 | 0 |  | 720 |
| `vr_burn_piece_time` | `5` | yes | C | gameplay | Burning | 1 | 0 |  | 721 |
| `vr_burn_piece_gap` | `8` | yes | C | gameplay | Burning | 2 | 0 |  | 722 |
| `vr_burn_piece_flames` | `3` | yes | C | gameplay | Burning | 1 | 0 |  | 723 |
| `vr_burn_piece_flame_size` | `0.7` | yes | C | gameplay | Burning | 1 | 0 |  | 724 |
| `vr_burn_nail_convert` | `1` | yes | C | gameplay | Burning | 2 | 0 |  | 725 |
| `vr_burn_nail_reach` | `10` | yes | C | gameplay | Burning | 1 | 0 |  | 726 |
| `vr_burn_nail_sound` | `0.5` | yes | C | audio | Burning | 1 | 0 |  | 727 |
| `vr_burn_self` | `0` | yes | C | gameplay | Burning | 2 | 0 |  | 728 |
| `vr_burn_self_time` | `0.6` | yes | C | gameplay | Burning | 1 | 0 |  | 729 |
| `vr_burn_self_haptic` | `1` | yes | C | controls,gameplay | Burning | 1 | 0 |  | 730 |
| `vr_burn_drop` | `0` | yes | C | gameplay | Burning | 2 | 0 |  | 731 |
| `vr_burn_drop_time` | `1` | yes | C | gameplay | Burning | 1 | 0 |  | 732 |
| `vr_burn_test` | `0` | no | D | gameplay | — | 2 | 0 |  | 733 |
| `vr_offhandpitch` | `40.25` | yes | B | gameplay | Hand/Gun Calibration; VR Settings | 5 | 0 | vr_bindings.cfg | 734 |
| `vr_offhandyaw` | `-4` | yes | C | gameplay | Hand/Gun Calibration | 5 | 0 | vr_bindings.cfg | 735 |
| `vr_handcal_x` | `0` | yes | C | gameplay | Hand/Gun Calibration | 7 | 0 | vr_defaults.cfg | 741 |
| `vr_handcal_y` | `0` | yes | C | gameplay | Hand/Gun Calibration | 7 | 0 | vr_defaults.cfg | 742 |
| `vr_handcal_z` | `0` | yes | C | gameplay | Hand/Gun Calibration | 7 | 0 | vr_defaults.cfg | 743 |
| `vr_handcal_roll` | `0` | yes | C | gameplay | Hand/Gun Calibration | 6 | 0 |  | 744 |
| `vr_handcal_off_mirror` | `0` | yes | C | graphics | Hand/Gun Calibration | 8 | 0 | vr_defaults.cfg | 745 |
| `vr_handcal_off_x` | `0` | yes | C | gameplay | Hand/Gun Calibration | 6 | 0 | vr_defaults.cfg | 746 |
| `vr_handcal_off_y` | `0` | yes | C | gameplay | Hand/Gun Calibration | 6 | 0 | vr_defaults.cfg | 747 |
| `vr_handcal_off_z` | `0` | yes | C | gameplay | Hand/Gun Calibration | 6 | 0 | vr_defaults.cfg | 748 |
| `vr_handcal_off_roll` | `0` | yes | C | gameplay | Hand/Gun Calibration | 5 | 0 |  | 749 |
| `vr_show_hip_holsters` | `0` | no | D | gameplay | Hip Holsters | 1 | 0 |  | 750 |
| `vr_hip_offset_x` | `-3.5` | yes | C | gameplay | Hip Holsters | 3 | 0 |  | 751 |
| `vr_hip_offset_y` | `7.0` | yes | C | gameplay | Hip Holsters | 1 | 0 | vr_defaults.cfg | 752 |
| `vr_hip_offset_z` | `0` | yes | C | gameplay | Hip Holsters | 1 | 0 |  | 753 |
| `vr_hip_holster_thresh` | `6.5` | yes | C | gameplay | Hip Holsters | 1 | 0 |  | 754 |
| `vr_holster_leg_follow` | `0.4` | yes | C | gameplay | Body | 1 | 0 |  | 755 |
| `vr_show_shoulder_holsters` | `0` | no | D | gameplay | Hotspots | 1 | 0 |  | 756 |
| `vr_shoulder_holster_offset_x` | `-0.5` | yes | C | gameplay | Hotspots | 1 | 0 |  | 757 |
| `vr_shoulder_holster_offset_y` | `2.25` | yes | C | gameplay | Hotspots | 1 | 0 |  | 758 |
| `vr_shoulder_holster_offset_z` | `-0.25` | yes | C | gameplay | Hotspots | 1 | 0 |  | 759 |
| `vr_shoulder_holster_thresh` | `7.8` | yes | C | gameplay | Hotspots | 1 | 0 |  | 760 |
| `vr_show_upper_holsters` | `0` | no | D | gameplay | Hotspots | 1 | 0 |  | 761 |
| `vr_upper_holster_offset_x` | `-4.25` | yes | C | gameplay | Hotspots | 3 | 0 |  | 762 |
| `vr_upper_holster_offset_y` | `7` | yes | C | gameplay | Hotspots | 1 | 0 | vr_defaults.cfg | 763 |
| `vr_upper_holster_offset_z` | `8.5` | yes | C | gameplay | Hotspots | 1 | 0 |  | 764 |
| `vr_upper_holster_thresh` | `6.5` | yes | C | gameplay | Hotspots | 1 | 0 |  | 765 |
| `vr_hip_holster_pitch` | `0` | yes | C | gameplay | Hip Holsters | 1 | 0 |  | 770 |
| `vr_hip_holster_yaw` | `0` | yes | C | gameplay | Hip Holsters | 1 | 0 | vr_defaults.cfg | 771 |
| `vr_hip_holster_roll` | `0` | yes | C | gameplay | Hip Holsters | 1 | 0 |  | 772 |
| `vr_upper_holster_pitch` | `0` | yes | C | gameplay | Hotspots | 1 | 0 | vr_defaults.cfg | 773 |
| `vr_upper_holster_yaw` | `0` | yes | C | gameplay | Hotspots | 1 | 0 | vr_defaults.cfg | 774 |
| `vr_upper_holster_roll` | `0` | yes | C | gameplay | Hotspots | 1 | 0 | vr_defaults.cfg | 775 |
| `vr_shoulder_holster_pitch` | `0` | yes | C | gameplay | Hotspots | 1 | 0 |  | 776 |
| `vr_shoulder_holster_yaw` | `0` | yes | C | gameplay | Hotspots | 1 | 0 |  | 777 |
| `vr_shoulder_holster_roll` | `0` | yes | C | gameplay | Hotspots | 1 | 0 |  | 778 |
| `vr_show_grenade_pouch` | `0` | no | D | gameplay | Batting and Catching; Hip Holsters | 1 | 0 |  | 782 |
| `vr_grenade_pouch_x` | `-7` | yes | C | gameplay | Batting and Catching; Hip Holsters | 2 | 0 |  | 783 |
| `vr_grenade_pouch_y` | `0` | yes | C | gameplay | Batting and Catching; Hip Holsters | 1 | 0 |  | 784 |
| `vr_grenade_pouch_z` | `3` | yes | C | gameplay | Batting and Catching; Hip Holsters | 1 | 0 |  | 785 |
| `vr_grenade_pouch_thresh` | `7` | yes | C | gameplay | Batting and Catching; Hip Holsters | 1 | 0 |  | 786 |
| `vr_grenade_pouch_pitch` | `0` | yes | C | gameplay | Hip Holsters | 1 | 0 |  | 787 |
| `vr_grenade_pouch_yaw` | `0` | yes | C | gameplay | Hip Holsters | 1 | 0 |  | 788 |
| `vr_grenade_pouch_roll` | `0` | yes | C | gameplay | Hip Holsters | 1 | 0 |  | 789 |
| `vr_grenade_pouch_hold_pitch` | `90` | yes | C | gameplay | Batting and Catching; Hip Holsters | 2 | 0 |  | 792 |
| `vr_grenade_pouch_hold_yaw` | `0` | yes | C | gameplay | Batting and Catching; Hip Holsters | 2 | 0 |  | 793 |
| `vr_grenade_pouch_hold_roll` | `0` | yes | C | gameplay | Batting and Catching; Hip Holsters | 2 | 0 |  | 794 |
| `vr_body_mode` | `3` | yes | A | gameplay | Body; Body and Display | 10 | 0 | vr_defaults.cfg | 797 |
| `vr_body_build` | `1` | yes | B | gameplay | Body; Body and Display | 7 | 0 |  | 798 |
| `vr_body_walk` | `1` | yes | C | gameplay | Body | 1 | 0 |  | 799 |
| `vr_body_step_rate` | `2.2` | yes | C | gameplay | Body | 1 | 0 |  | 800 |
| `vr_body_wade` | `1` | yes | C | gameplay | Body | 1 | 0 |  | 801 |
| `vr_body_swim_kick` | `1` | yes | C | gameplay | Body | 1 | 0 |  | 802 |
| `vr_body_swim_kick_rate` | `1.5` | yes | C | gameplay | Body | 1 | 0 |  | 803 |
| `vr_body_turn_step` | `40` | yes | C | gameplay | Body | 1 | 0 |  | 804 |
| `vr_body_state` | `1` | yes | C | gameplay | Body | 2 | 0 |  | 805 |
| `vr_body_powerups` | `1` | yes | C | gameplay | Body | 2 | 0 |  | 806 |
| `vr_body_blood` | `1` | yes | C | gameplay | Body; Gore | 4 | 0 |  | 807 |
| `vr_wounds` | `1` | yes | C | gameplay | Gore | 20 | 0 |  | 808 |
| `vr_wounds_burns` | `1` | yes | C | gameplay | Gore | 4 | 0 |  | 809 |
| `vr_wounds_wet` | `1` | yes | C | gameplay | Gore | 6 | 0 |  | 810 |
| `vr_wounds_pool` | `64` | yes | C | gameplay | Gore | 1 | 0 | vr_defaults.cfg | 811 |
| `vr_wounds_own_res` | `0` | yes | C | gameplay | Gore | 1 | 0 |  | 812 |
| `vr_wounds_bump_burns` | `3` | yes | C | graphics | Gore | 1 | 0 |  | 813 |
| `vr_wounds_bump_blood` | `1.5` | yes | C | graphics | Gore | 1 | 0 |  | 814 |
| `vr_wounds_blood_alpha` | `0.8` | yes | C | gameplay | Gore | 2 | 0 |  | 815 |
| `vr_wounds_debug` | `0` | no | D | dev | Debug - Logging | 22 | 0 |  | 816 |
| `vr_gore_hands` | `1` | yes | C | gameplay | Gore | 6 | 0 |  | 818 |
| `vr_gore_wash` | `1` | yes | C | gameplay | Gore | 2 | 0 |  | 819 |
| `vr_gore_wash_time` | `1.5` | yes | C | gameplay | Gore | 1 | 0 |  | 820 |
| `vr_gore_reopen` | `1` | yes | C | gameplay | Gore | 3 | 0 |  | 821 |
| `vr_gore_reopen_delay` | `4` | yes | C | gameplay | Gore | 3 | 0 |  | 822 |
| `vr_gore_reopen_time` | `1.5` | yes | C | gameplay | Gore | 1 | 0 |  | 823 |
| `vr_gore_spatter` | `1` | yes | C | gameplay | Gore | 3 | 0 |  | 825 |
| `vr_gore_spatter_melee` | `1` | yes | C | gameplay | Gore | 1 | 0 |  | 826 |
| `vr_gore_spatter_saw` | `2` | yes | C | gameplay | Gore | 1 | 0 |  | 827 |
| `vr_gore_spatter_shots` | `1` | yes | C | gameplay | Gore | 1 | 0 |  | 828 |
| `vr_gore_spatter_range` | `64` | yes | C | gameplay | Gore | 1 | 0 |  | 829 |
| `vr_gore_spatter_gibs` | `1` | yes | C | gameplay | Gore | 3 | 0 |  | 830 |
| `vr_gore_gear` | `1` | yes | C | gameplay | Gore | 2 | 0 |  | 831 |
| `vr_gore_clean_skins` | `1` | yes | C | comfort,gameplay | Gore | 3 | 0 |  | 832 |
| `vr_gore_gear_holstered` | `1` | yes | C | gameplay | Gore | 1 | 0 |  | 833 |
| `vr_gore_gear_nearby` | `1` | yes | C | gameplay | Gore | 2 | 0 |  | 834 |
| `vr_gore_gear_drops` | `1` | yes | C | gameplay | Gore | 1 | 0 |  | 835 |
| `vr_gore_spread` | `1` | yes | C | gameplay | Gore | 5 | 0 |  | 836 |
| `vr_gore_mist` | `1` | yes | C | gameplay | Gore | 1 | 0 |  | 838 |
| `vr_gore_mist_size` | `1` | yes | C | gameplay | Gore | 1 | 0 |  | 839 |
| `vr_gore_mist_alpha` | `0.06` | yes | C | gameplay | Gore | 1 | 0 |  | 840 |
| `vr_gore_mist_life` | `2.5` | yes | C | gameplay | Gore | 1 | 0 |  | 841 |
| `vr_gore_mist_grow` | `1` | yes | C | gameplay | Gore | 1 | 0 |  | 842 |
| `vr_gore_mist_speed` | `1` | yes | C | gameplay | Gore | 1 | 0 |  | 843 |
| `vr_gore_mist_rise` | `-2` | yes | C | gameplay | Gore | 1 | 0 |  | 844 |
| `vr_gore_mist_dark` | `0.4` | yes | C | gameplay | Gore | 1 | 0 |  | 845 |
| `vr_gore_mist_near` | `40` | yes | C | gameplay | Gore | 1 | 0 |  | 846 |
| `vr_corpse_dying` | `1` | yes | C | gameplay | Gore | 19 | 0 |  | 847 |
| `vr_body_anchors` | `1` | yes | C | gameplay | Body | 6 | 0 |  | 848 |
| `vr_body_debug` | `0` | no | D | dev | Debug - Views | 12 | 0 |  | 849 |
| `vr_body_eye_forward` | `0.08` | yes | C | gameplay | Body | 4 | 0 |  | 850 |
| `vr_body_eye_up` | `0.08` | yes | C | gameplay | Body | 4 | 0 |  | 851 |
| `vr_body_crouch_tilt` | `25` | yes | C | gameplay | Body | 1 | 0 | vr_defaults.cfg | 852 |
| `vr_body_torso_back` | `0` | yes | B | gameplay | Body; Body and Display | 3 | 0 | vr_defaults.cfg | 853 |
| `vr_torso_mode` | `1` | yes | C | gameplay | Body | 1 | 0 |  | 855 |
| `vr_torso_head` | `1` | yes | C | gameplay | Body | 1 | 0 |  | 856 |
| `vr_torso_head_history` | `1` | yes | C | gameplay | Body | 1 | 0 |  | 857 |
| `vr_torso_hands` | `2.5` | yes | C | gameplay | Body | 1 | 0 |  | 858 |
| `vr_torso_one_hand` | `0.3` | yes | C | gameplay | Body | 1 | 0 |  | 859 |
| `vr_torso_hands_down` | `2` | yes | C | gameplay | Body | 1 | 0 |  | 860 |
| `vr_torso_head_lag` | `0.3` | yes | C | gameplay | Body | 1 | 0 |  | 861 |
| `vr_torso_side_angle` | `45` | yes | C | gameplay | Body | 1 | 0 |  | 862 |
| `vr_torso_deadzone` | `8` | yes | C | comfort,controls,gameplay | Body | 1 | 0 |  | 863 |
| `vr_torso_speed` | `12` | yes | C | gameplay | Body | 1 | 0 |  | 864 |
| `vr_torso_neck_max` | `70` | yes | C | gameplay | Body | 1 | 0 |  | 865 |
| `vr_body_legs_back` | `0` | yes | B | gameplay | Body; Body and Display | 1 | 0 | vr_defaults.cfg | 866 |
| `vr_body_pauldrons` | `1` | yes | D | gameplay | Body - Arms and Pauldrons | 2 | 0 |  | 867 |
| `vr_body_pauldron_style` | `0` | yes | D | gameplay | Body - Arms and Pauldrons | 1 | 0 | vr_defaults.cfg | 868 |
| `vr_body_pauldron_size` | `1` | yes | D | gameplay | Body - Arms and Pauldrons | 1 | 0 |  | 869 |
| `vr_body_pauldron_follow` | `0.35` | yes | D | gameplay | Body - Arms and Pauldrons | 1 | 0 |  | 870 |
| `vr_body_pauldron_forward` | `0` | yes | D | gameplay | Body - Arms and Pauldrons | 1 | 0 |  | 871 |
| `vr_body_pauldron_up` | `0` | yes | D | gameplay | Body - Arms and Pauldrons | 1 | 0 |  | 872 |
| `vr_body_pauldron_out` | `0` | yes | D | gameplay | Body - Arms and Pauldrons | 1 | 0 | vr_defaults.cfg | 873 |
| `vr_flashlight` | `1` | yes | B | graphics | Body and Display; Flashlight | 21 | 0 |  | 875 |
| `vr_flashlight_brightness` | `1` | yes | C | graphics | Flashlight | 3 | 0 |  | 876 |
| `vr_flashlight_range` | `1000` | yes | C | graphics | Flashlight | 1 | 0 |  | 877 |
| `vr_flashlight_shadows` | `1` | yes | C | graphics | Flashlight | 1 | 0 |  | 878 |
| `vr_flashlight_cord` | `3` | yes | C | graphics | Flashlight | 5 | 0 |  | 879 |
| `vr_flashlight_beam` | `0.35` | yes | C | graphics | Flashlight | 1 | 0 | vr_defaults.cfg | 880 |
| `vr_flashlight_beam_quality` | `1` | yes | C | graphics | Flashlight | 1 | 0 |  | 881 |
| `vr_flashlight_tilt` | `8` | yes | C | graphics | Flashlight | 1 | 0 | vr_defaults.cfg | 882 |
| `vr_flashlight_forward` | `0` | yes | C | graphics | Flashlight | 1 | 0 | vr_defaults.cfg | 883 |
| `vr_flashlight_up` | `0` | yes | C | graphics | Flashlight | 1 | 0 | vr_defaults.cfg | 884 |
| `vr_flashlight_out` | `0` | yes | C | graphics | Flashlight | 1 | 0 | vr_defaults.cfg | 885 |
| `vr_flashlight_side` | `0` | yes | A | graphics | Body and Display; Flashlight | 5 | 0 |  | 886 |
| `vr_flashlight_grab_range` | `1` | yes | C | graphics | Flashlight | 1 | 0 |  | 887 |
| `vr_flashlight_head_range` | `1` | yes | C | graphics | Flashlight | 1 | 0 | vr_defaults.cfg | 888 |
| `vr_flashlight_mount_preview` | `1` | yes | D | graphics | Weapon Offsets - Flashlight | 1 | 0 |  | 889 |
| `vr_flashlight_gun_range` | `1` | yes | C | graphics | Flashlight | 1 | 0 |  | 890 |
| `vr_flashlight_auto_head` | `0` | yes | C | graphics | Flashlight | 1 | 0 | vr_defaults.cfg | 891 |
| `vr_flashlight_auto_gun` | `0` | yes | C | graphics | Flashlight | 1 | 0 | vr_defaults.cfg | 892 |
| `vr_flashlight_clip_time` | `0.15` | yes | C | graphics | Flashlight | 1 | 0 |  | 893 |
| `vr_flashlight_hand_forward` | `-0.05` | yes | C | graphics | Flashlight | 1 | 0 |  | 894 |
| `vr_flashlight_hand_up` | `-0.04` | yes | C | graphics | Flashlight | 1 | 0 |  | 895 |
| `vr_flashlight_gun_forward` | `0` | yes | D | graphics | Flashlight - On a Gun or Head | 1 | 0 |  | 896 |
| `vr_flashlight_gun_up` | `0` | yes | D | graphics | Flashlight - On a Gun or Head | 1 | 0 |  | 897 |
| `vr_flashlight_gun_out` | `0` | yes | D | graphics | Flashlight - On a Gun or Head | 1 | 0 |  | 898 |
| `vr_flashlight_head_forward` | `0` | yes | D | graphics | Flashlight - On a Gun or Head | 1 | 0 |  | 899 |
| `vr_flashlight_head_up` | `0` | yes | D | graphics | Flashlight - On a Gun or Head | 1 | 0 |  | 900 |
| `vr_flashlight_head_out` | `0` | yes | D | graphics | Flashlight - On a Gun or Head | 1 | 0 |  | 901 |
| `vr_flashlight_low_x` | `0` | yes | D | graphics | Flashlight - Low Grip | 1 | 0 | vr_defaults.cfg | 905 |
| `vr_flashlight_low_y` | `-0.5` | yes | D | graphics | Flashlight - Low Grip | 1 | 0 | vr_defaults.cfg | 906 |
| `vr_flashlight_low_z` | `0` | yes | D | graphics | Flashlight - Low Grip | 1 | 0 | vr_defaults.cfg | 907 |
| `vr_flashlight_low_pitch` | `0` | yes | D | graphics | Flashlight - Low Grip | 1 | 0 | vr_defaults.cfg | 908 |
| `vr_flashlight_low_yaw` | `0` | yes | D | graphics | Flashlight - Low Grip | 1 | 0 |  | 909 |
| `vr_flashlight_low_roll` | `0` | yes | D | graphics | Flashlight - Low Grip | 1 | 0 |  | 910 |
| `vr_flashlight_high_x` | `0` | yes | D | graphics | Flashlight - Overhead Grip | 1 | 0 | vr_defaults.cfg | 911 |
| `vr_flashlight_high_y` | `-0.5` | yes | D | graphics | Flashlight - Overhead Grip | 1 | 0 | vr_defaults.cfg | 912 |
| `vr_flashlight_high_z` | `2` | yes | D | graphics | Flashlight - Overhead Grip | 1 | 0 | vr_defaults.cfg | 913 |
| `vr_flashlight_high_pitch` | `0` | yes | D | graphics | Flashlight - Overhead Grip | 1 | 0 |  | 914 |
| `vr_flashlight_high_yaw` | `0` | yes | D | graphics | Flashlight - Overhead Grip | 1 | 0 |  | 915 |
| `vr_flashlight_high_roll` | `0` | yes | D | graphics | Flashlight - Overhead Grip | 1 | 0 |  | 916 |
| `vr_flashlight_hue` | `40` | yes | C | HUD,graphics | Flashlight | 1 | 0 |  | 917 |
| `vr_flashlight_saturation` | `0` | yes | C | graphics | Flashlight | 1 | 0 | vr_defaults.cfg | 918 |
| `vr_flashlight_low_fingers` | `0` | yes | D | graphics | Flashlight - Low Grip | 3 | 0 |  | 925 |
| `vr_flashlight_low_overlap` | `0.3` | yes | D | graphics | Flashlight - Low Grip | 2 | 0 | vr_defaults.cfg | 926 |
| `vr_flashlight_low_curl_thumb` | `0.97` | yes | D | graphics | Flashlight - Low Grip | 2 | 0 | vr_defaults.cfg | 927 |
| `vr_flashlight_low_curl_index` | `0.52` | yes | D | graphics | Flashlight - Low Grip | 2 | 0 |  | 928 |
| `vr_flashlight_low_curl_middle` | `0.78` | yes | D | graphics | Flashlight - Low Grip | 2 | 0 |  | 929 |
| `vr_flashlight_low_curl_ring` | `0.8` | yes | D | graphics | Flashlight - Low Grip | 2 | 0 |  | 930 |
| `vr_flashlight_low_curl_pinky` | `0.93` | yes | D | graphics | Flashlight - Low Grip | 2 | 0 |  | 931 |
| `vr_flashlight_low_thumb_across` | `1` | yes | D | graphics | Flashlight - Low Grip | 2 | 0 |  | 932 |
| `vr_flashlight_low_bias_thumb` | `0` | yes | D | graphics | Flashlight - Low Grip | 2 | 0 | vr_defaults.cfg | 933 |
| `vr_flashlight_low_bias_index` | `0` | yes | D | graphics | Flashlight - Low Grip | 2 | 0 | vr_defaults.cfg | 934 |
| `vr_flashlight_low_bias_middle` | `0` | yes | D | graphics | Flashlight - Low Grip | 2 | 0 |  | 935 |
| `vr_flashlight_low_bias_ring` | `0` | yes | D | graphics | Flashlight - Low Grip | 2 | 0 | vr_defaults.cfg | 936 |
| `vr_flashlight_low_bias_pinky` | `0` | yes | D | graphics | Flashlight - Low Grip | 2 | 0 |  | 937 |
| `vr_flashlight_low_thumb_x` | `0` | yes | D | graphics | Flashlight - Low Grip | 2 | 0 | vr_defaults.cfg | 938 |
| `vr_flashlight_low_thumb_y` | `0` | yes | D | graphics | Flashlight - Low Grip | 2 | 0 | vr_defaults.cfg | 939 |
| `vr_flashlight_low_thumb_z` | `0` | yes | D | graphics | Flashlight - Low Grip | 2 | 0 | vr_defaults.cfg | 940 |
| `vr_flashlight_high_fingers` | `0` | yes | D | graphics | Flashlight - Overhead Grip | 3 | 0 |  | 941 |
| `vr_flashlight_high_overlap` | `0.3` | yes | D | graphics | Flashlight - Overhead Grip | 2 | 0 |  | 942 |
| `vr_flashlight_high_curl_thumb` | `0.97` | yes | D | graphics | Flashlight - Overhead Grip | 2 | 0 |  | 943 |
| `vr_flashlight_high_curl_index` | `0.72` | yes | D | graphics | Flashlight - Overhead Grip | 2 | 0 |  | 944 |
| `vr_flashlight_high_curl_middle` | `0.6` | yes | D | graphics | Flashlight - Overhead Grip | 2 | 0 |  | 945 |
| `vr_flashlight_high_curl_ring` | `0.49` | yes | D | graphics | Flashlight - Overhead Grip | 2 | 0 |  | 946 |
| `vr_flashlight_high_curl_pinky` | `1` | yes | D | graphics | Flashlight - Overhead Grip | 2 | 0 |  | 947 |
| `vr_flashlight_high_thumb_across` | `0.73` | yes | D | graphics | Flashlight - Overhead Grip | 2 | 0 |  | 948 |
| `vr_flashlight_high_bias_thumb` | `0` | yes | D | graphics | Flashlight - Overhead Grip | 2 | 0 |  | 949 |
| `vr_flashlight_high_bias_index` | `0` | yes | D | graphics | Flashlight - Overhead Grip | 2 | 0 |  | 950 |
| `vr_flashlight_high_bias_middle` | `0` | yes | D | graphics | Flashlight - Overhead Grip | 2 | 0 |  | 951 |
| `vr_flashlight_high_bias_ring` | `0` | yes | D | graphics | Flashlight - Overhead Grip | 2 | 0 |  | 952 |
| `vr_flashlight_high_bias_pinky` | `0` | yes | D | graphics | Flashlight - Overhead Grip | 2 | 0 |  | 953 |
| `vr_flashlight_high_thumb_x` | `0` | yes | D | graphics | Flashlight - Overhead Grip | 2 | 0 |  | 954 |
| `vr_flashlight_high_thumb_y` | `0` | yes | D | graphics | Flashlight - Overhead Grip | 2 | 0 |  | 955 |
| `vr_flashlight_high_thumb_z` | `0` | yes | D | graphics | Flashlight - Overhead Grip | 2 | 0 |  | 956 |
| `vr_flashlight_head_zone_forward` | `0` | yes | D | graphics | Flashlight - On a Gun or Head | 1 | 0 | vr_defaults.cfg | 965 |
| `vr_flashlight_head_zone_up` | `0.04` | yes | D | graphics | Flashlight - On a Gun or Head | 1 | 0 | vr_defaults.cfg | 966 |
| `vr_flashlight_head_zone_out` | `0` | yes | D | graphics | Flashlight - On a Gun or Head | 1 | 0 |  | 967 |
| `vr_flashlight_head_zone_radius` | `0.1` | yes | D | graphics | Flashlight - On a Gun or Head | 1 | 0 | vr_defaults.cfg | 968 |
| `vr_flashlight_gun_zone_forward` | `0` | yes | D | graphics | Flashlight - On a Gun or Head | 1 | 0 |  | 969 |
| `vr_flashlight_gun_zone_up` | `0` | yes | D | graphics | Flashlight - On a Gun or Head | 1 | 0 |  | 970 |
| `vr_flashlight_gun_zone_out` | `0` | yes | D | graphics | Flashlight - On a Gun or Head | 1 | 0 |  | 971 |
| `vr_flashlight_gun_zone_radius` | `0.12` | yes | D | graphics | Flashlight - On a Gun or Head | 1 | 0 |  | 972 |
| `vr_show_flashlight_zones` | `0` | no | D | dev,graphics | Debug - Views; Flashlight - On a Gun or Head | 2 | 0 |  | 973 |
| `vr_shells` | `1` | yes | C | gameplay | Immersion | 7 | 0 |  | 975 |
| `vr_shells_life` | `20` | yes | C | gameplay | Immersion | 1 | 0 |  | 976 |
| `vr_shells_sound` | `1` | yes | C | audio | Immersion | 3 | 0 |  | 977 |
| `vr_weapon_recoil` | `1` | yes | C | gameplay | Weapon Effects | 2 | 0 |  | 980 |
| `vr_recoil_kick` | `1.5` | yes | C | gameplay | Weapon Effects | 2 | 0 |  | 981 |
| `vr_recoil_rise` | `4` | yes | C | gameplay | Weapon Effects | 2 | 0 |  | 982 |
| `vr_muzzle_flash` | `1` | yes | C | gameplay | Weapon Effects | 4 | 0 |  | 983 |
| `vr_muzzle_flash_enemies` | `1` | yes | C | gameplay | Weapon Effects | 2 | 0 |  | 984 |
| `vr_muzzle_smoke_enemies` | `1` | yes | C | gameplay | Weapon Effects | 1 | 0 |  | 985 |
| `vr_muzzle_flash_enemy_size` | `1` | yes | C | gameplay | Weapon Effects | 1 | 0 |  | 986 |
| `vr_tracers` | `1` | yes | D | gameplay | Weapon Effects | 1 | 0 |  | 987 |
| `vr_tracers_enemies` | `1` | yes | D | gameplay | Weapon Effects | 1 | 0 |  | 988 |
| `vr_tracer_speed` | `150` | yes | D | gameplay | Weapon Effects | 1 | 0 |  | 989 |
| `vr_tracer_length` | `1.5` | yes | D | gameplay | Weapon Effects | 1 | 0 |  | 990 |
| `vr_tracer_width` | `1.2` | yes | D | gameplay | Weapon Effects | 1 | 0 |  | 991 |
| `vr_tracer_chance` | `0.3` | yes | D | gameplay | Weapon Effects | 1 | 0 |  | 992 |
| `vr_tracer_r` | `1` | yes | D | gameplay | Weapon Effects | 1 | 0 |  | 993 |
| `vr_tracer_g` | `0.8` | yes | D | gameplay | Weapon Effects | 1 | 0 |  | 994 |
| `vr_tracer_b` | `0.45` | yes | D | gameplay | Weapon Effects | 1 | 0 |  | 995 |
| `vr_tracer_brightness` | `1` | yes | D | gameplay | Weapon Effects | 1 | 0 |  | 996 |
| `vr_debug_weaponfx` | `0` | no | D | dev | Debug - Tests | 8 | 0 |  | 997 |
| `vr_body_arm_length` | `1` | yes | D | gameplay | Body - Arms and Pauldrons | 4 | 0 |  | 998 |
| `vr_body_arm_stretch` | `1.2` | yes | D | gameplay | Body - Arms and Pauldrons | 3 | 0 |  | 999 |
| `vr_bodycal_upper_arm` | `0` | yes | C? | gameplay | — | 5 | 0 |  | 1007 |
| `vr_bodycal_forearm` | `0` | yes | C? | gameplay | — | 5 | 0 |  | 1008 |
| `vr_bodycal_shoulders_back` | `0` | yes | C? | gameplay | — | 5 | 0 |  | 1009 |
| `vr_bodycal_shoulders_up` | `0` | yes | C? | gameplay | — | 5 | 0 |  | 1010 |
| `vr_bodycal_shoulders_out` | `0` | yes | C? | gameplay | — | 5 | 0 |  | 1011 |
| `vr_bodycal_shoulder_rise` | `0` | yes | C? | gameplay | — | 5 | 0 |  | 1012 |
| `vr_bodycal_shoulder_swing` | `0` | yes | C? | gameplay | — | 5 | 0 |  | 1013 |
| `vr_body_tweak_upper_arm` | `0` | yes | D | gameplay | Body - Arms and Pauldrons | 4 | 0 |  | 1014 |
| `vr_body_tweak_forearm` | `0` | yes | D | gameplay | Body - Arms and Pauldrons | 4 | 0 |  | 1015 |
| `vr_body_tweak_shoulders_back` | `0` | yes | D | gameplay | Body - Arms and Pauldrons | 5 | 0 |  | 1016 |
| `vr_body_tweak_shoulders_up` | `0` | yes | D | gameplay | Body - Arms and Pauldrons | 5 | 0 |  | 1017 |
| `vr_body_tweak_shoulders_out` | `0` | yes | D | gameplay | Body - Arms and Pauldrons | 5 | 0 |  | 1018 |
| `vr_body_tweak_shoulder_rise` | `0` | yes | D | gameplay | Body - Arms and Pauldrons | 5 | 0 |  | 1019 |
| `vr_body_tweak_shoulder_swing` | `0` | yes | D | gameplay | Body - Arms and Pauldrons | 5 | 0 |  | 1020 |
| `vr_body_upper_arm` | `` | no | D | gameplay | — | 1 | 0 |  | 1023 |
| `vr_body_forearm` | `` | no | D | gameplay | — | 1 | 0 |  | 1024 |
| `vr_body_shoulders_back` | `` | no | B | gameplay | Body and Display | 1 | 0 |  | 1025 |
| `vr_body_shoulders_up` | `` | no | D | gameplay | — | 1 | 0 |  | 1026 |
| `vr_body_shoulders_out` | `` | no | D | gameplay | — | 1 | 0 |  | 1027 |
| `vr_body_shoulder_up` | `` | no | D | gameplay | — | 1 | 0 |  | 1028 |
| `vr_body_shoulder_forward` | `` | no | D | gameplay | — | 1 | 0 |  | 1029 |
| `vr_bodycal_seated` | `0` | yes | A | comfort,gameplay | Body Calibration | 9 | 0 |  | 1032 |
| `vr_bodycal_preview` | `1` | yes | B | gameplay | Body Calibration | 2 | 0 |  | 1033 |
| `vr_bodycal_undo` | `` | yes | C? | gameplay | — | 10 | 0 |  | 1034 |
| `vr_setup_test_take` | `` | no | D | gameplay | — | 3 | 0 |  | 1037 |
| `vr_test_modal_answer` | `-1` | no | D | gameplay | — | 3 | 0 |  | 1040 |
| `vr_body_shoulder_reach` | `0.08` | yes | D | gameplay | Body - Arms and Pauldrons | 2 | 0 |  | 1041 |
| `vr_body_forearm_twist` | `0.5` | yes | D | gameplay | Body - Arms and Pauldrons | 1 | 0 |  | 1042 |
| `vr_body_wrist_limits` | `1` | yes | D | gameplay | Body - Arms and Pauldrons | 2 | 0 |  | 1043 |
| `vr_body_elbow_lift` | `4` | yes | D | gameplay | Body - Arms and Pauldrons | 1 | 0 |  | 1044 |
| `vr_body_elbow_spread` | `1` | yes | D | gameplay | Body - Arms and Pauldrons | 1 | 0 |  | 1045 |
| `vr_body_elbow_out` | `0.35` | yes | D | gameplay | Body - Arms and Pauldrons | 1 | 0 |  | 1046 |
| `vr_body_elbow_back` | `0.25` | yes | D | gameplay | Body - Arms and Pauldrons | 1 | 0 |  | 1047 |
| `vr_body_elbow_hand` | `0.4` | yes | D | gameplay | Body - Arms and Pauldrons | 1 | 0 |  | 1048 |
| `vr_holster_haptics` | `2` | yes | C | controls,gameplay | Immersion | 1 | 0 |  | 1049 |
| `vr_holster_haptic_time` | `0.25` | yes | C | controls,gameplay | Immersion | 2 | 0 |  | 1050 |
| `vr_model_lighting` | `1` | yes | C | graphics | Graphics - Models and Effects | 2 | 0 |  | 1051 |
| `vr_model_ambient_dir` | `1` | yes | C | graphics | Graphics - Models and Effects | 2 | 0 |  | 1052 |
| `vr_model_ambient_contrast` | `0.75` | yes | C | graphics | Graphics - Models and Effects | 4 | 0 | vr_defaults.cfg | 1053 |
| `vr_rim_light` | `0.5` | yes | C | graphics | Graphics - Models and Effects | 2 | 0 |  | 1054 |
| `vr_weapon_reflections` | `1` | yes | C | graphics | Graphics - Models and Effects | 5 | 0 |  | 1055 |
| `vr_weapon_reflections_strength` | `1` | yes | C | graphics | Graphics - Models and Effects | 1 | 0 | vr_defaults.cfg | 1056 |
| `vr_relit_maps` | `1` | yes | B | graphics | Graphics | 1 | 0 |  | 1057 |
| `vr_particles` | `1` | yes | C | graphics | Particles | 30 | 0 |  | 1058 |
| `vr_particle_mult` | `1` | yes | C | graphics | Particles | 2 | 0 |  | 1059 |
| `vr_particle_seed` | `0` | no | D | graphics | — | 2 | 0 |  | 1060 |
| `vr_soft_particles` | `1` | yes | C | graphics | Graphics - Models and Effects | 2 | 0 |  | 1061 |
| `vr_soft_particles_scale` | `1` | yes | C | graphics | Graphics - Models and Effects | 2 | 0 |  | 1062 |
| `vr_blob_shadows` | `2` | yes | C | graphics | Graphics - Shadows | 2 | 0 |  | 1063 |
| `vr_player_shadows` | `2` | yes | C | graphics,multiplayer | Graphics - Shadows | 1 | 0 | vr_defaults.cfg | 1064 |
| `vr_entity_shadows` | `1` | yes | C | graphics | Graphics - Shadows | 2 | 0 |  | 1065 |
| `vr_ao_dynamic` | `1` | yes | C | graphics | Graphics - Shadows | 2 | 0 |  | 1067 |
| `vr_ao_dynamic_range` | `2.5` | yes | C | graphics | Graphics - Shadows | 1 | 0 | vr_defaults.cfg | 1068 |
| `vr_ao_brush` | `1` | yes | C | graphics | Graphics - Shadows | 2 | 0 |  | 1069 |
| `vr_ao_models` | `1` | yes | C | graphics | Graphics - Shadows | 2 | 0 | vr_defaults.cfg | 1070 |
| `vr_water_splash_speed` | `150` | yes | C? | gameplay | — | 2 | 0 |  | 1071 |
| `vr_water_splash` | `1` | yes | C | graphics | Graphics - Liquids | 3 | 0 |  | 1072 |
| `vr_water_splash_size` | `1` | yes | C | graphics | Graphics - Liquids | 2 | 0 |  | 1073 |
| `vr_water_splash_ring_speed` | `1` | yes | C | graphics | Graphics - Liquids | 2 | 0 |  | 1074 |
| `vr_water_splash_ring_size` | `1` | yes | C | graphics | Graphics - Liquids | 2 | 0 |  | 1075 |
| `vr_water_sounds` | `1` | yes | C | audio,graphics | Graphics - Liquids | 2 | 0 |  | 1076 |
| `vr_graphics_preset` | `-1` | no | A | graphics | Graphics | 3 | 0 |  | 1078 |
| `vr_shadow_dlights` | `4` | yes | C | graphics | Graphics - Shadows | 4 | 0 | vr_defaults.cfg | 1079 |
| `vr_shadow_dlight_size` | `512` | yes | C | graphics | Graphics - Shadows | 3 | 0 | vr_defaults.cfg | 1080 |
| `vr_shadow_precision` | `1` | yes | C? | graphics | — | 2 | 0 |  | 1081 |
| `vr_shadow_muzzleflash` | `0` | yes | C | graphics | Graphics - Shadows | 2 | 0 | vr_defaults.cfg | 1082 |
| `vr_shadow_maplights` | `2` | yes | C | graphics | Graphics - Shadows | 5 | 0 | vr_defaults.cfg | 1083 |
| `vr_shadow_maplight_size` | `512` | yes | C | graphics | Graphics - Shadows | 3 | 0 | vr_defaults.cfg | 1084 |
| `vr_shadow_maplight_strength` | `0.7` | yes | C | graphics | Graphics - Shadows | 1 | 0 | vr_defaults.cfg | 1085 |
| `vr_shadow_self` | `2` | yes | C | graphics | Graphics - Shadows | 3 | 0 |  | 1086 |
| `vr_shadow_filter` | `1` | yes | C | graphics | Graphics - Shadows | 3 | 0 | vr_defaults.cfg | 1087 |
| `vr_shadow_bias` | `1` | yes | C | graphics | Graphics - Shadows | 1 | 0 |  | 1088 |
| `vr_shadow_distance` | `1536` | yes | C | graphics | Graphics - Shadows | 4 | 0 | vr_defaults.cfg | 1089 |
| `vr_shadow_atlas` | `4096` | yes | C | graphics | Graphics - Shadows | 3 | 0 | vr_defaults.cfg | 1090 |
| `vr_shadow_stats` | `0` | no | C | graphics | Graphics - Shadows | 1 | 0 |  | 1091 |
| `vr_retrolight` | `0` | yes | C | graphics | Graphics - Retro Lighting | 2 | 0 |  | 1093 |
| `vr_retrolight_ab` | `0` | no | D | dev,graphics | Debug - Views | 1 | 0 |  | 1094 |
| `vr_retrolight_spacing` | `1` | yes | C | graphics | Graphics - Retro Lighting | 1 | 0 |  | 1095 |
| `vr_retrolight_edge_soft` | `1` | yes | C | graphics | Graphics - Retro Lighting | 1 | 0 |  | 1096 |
| `vr_retrolight_world` | `1` | yes | C | graphics | Graphics - Retro Lighting | 1 | 0 |  | 1097 |
| `vr_retrolight_world_steps` | `16` | yes | C | graphics | Graphics - Retro Lighting | 1 | 0 |  | 1098 |
| `vr_retrolight_world_soft` | `0` | yes | C | graphics | Graphics - Retro Lighting | 1 | 0 |  | 1099 |
| `vr_retrolight_world_dither` | `0` | yes | C | graphics | Graphics - Retro Lighting | 1 | 0 |  | 1100 |
| `vr_retrolight_world_dither_size` | `1` | yes | C | graphics | Graphics - Retro Lighting | 1 | 0 |  | 1101 |
| `vr_retrolight_world_lightmap` | `1` | yes | C | graphics | Graphics - Retro Lighting | 1 | 0 |  | 1102 |
| `vr_retrolight_world_luxel` | `16` | yes | C | graphics | Graphics - Retro Lighting | 1 | 0 |  | 1103 |
| `vr_retrolight_world_dyn_steps` | `8` | yes | C | graphics | Graphics - Retro Lighting | 1 | 0 |  | 1104 |
| `vr_retrolight_world_dyn_block` | `4` | yes | C | graphics | Graphics - Retro Lighting | 1 | 0 |  | 1105 |
| `vr_retrolight_models` | `1` | yes | C | graphics | Graphics - Retro Lighting | 1 | 0 |  | 1106 |
| `vr_retrolight_model_steps` | `16` | yes | C | graphics | Graphics - Retro Lighting | 1 | 0 |  | 1107 |
| `vr_retrolight_model_soft` | `0` | yes | C | graphics | Graphics - Retro Lighting | 1 | 0 |  | 1108 |
| `vr_retrolight_model_dither` | `0` | yes | C | graphics | Graphics - Retro Lighting | 1 | 0 |  | 1109 |
| `vr_retrolight_model_dither_size` | `1` | yes | C | graphics | Graphics - Retro Lighting | 1 | 0 |  | 1110 |
| `vr_retrolight_model_dyn_steps` | `8` | yes | C | graphics | Graphics - Retro Lighting | 1 | 0 |  | 1111 |
| `vr_retrolight_model_dyn_block` | `0` | yes | C | graphics | Graphics - Retro Lighting | 1 | 0 |  | 1112 |
| `vr_retrolight_shadow_filter` | `1` | yes | C | graphics | Graphics - Retro Lighting | 1 | 0 |  | 1113 |
| `vr_retrolight_shadow_steps` | `0` | yes | C | graphics | Graphics - Retro Lighting | 1 | 0 |  | 1114 |
| `vr_retrolight_shadow_block` | `0` | yes | C | graphics | Graphics - Retro Lighting | 1 | 0 |  | 1115 |
| `vr_retrolight_shadow_soft` | `0` | yes | C | graphics | Graphics - Retro Lighting | 1 | 0 |  | 1116 |
| `vr_profile` | `0` | no | D | graphics | Graphics | 58 | 0 |  | 1117 |
| `vr_profile_interval` | `5` | no | D | dev | Debug - Profiling and Memory | 2 | 0 |  | 1118 |
| `vr_profile_overlay` | `0` | no | D | dev | Debug - Profiling and Memory | 5 | 0 |  | 1119 |
| `vr_profile_gpu` | `4` | no | D | dev | Debug - Profiling and Memory | 6 | 0 |  | 1120 |
| `vr_profile_detail` | `1` | no | D | dev,graphics | Debug - Profiling and Memory | 3 | 0 |  | 1121 |
| `vr_profile_csv` | `0` | no | D | dev | Debug - Profiling and Memory | 10 | 0 |  | 1122 |
| `vr_profile_hitch` | `1.5` | no | D | dev | Debug - Profiling and Memory | 4 | 0 |  | 1123 |
| `vr_dlight_models` | `1` | yes | C | graphics | Graphics - Lights | 4 | 0 |  | 1124 |
| `vr_dlight_angle` | `1` | yes | C | graphics | Graphics - Lights | 2 | 0 |  | 1125 |
| `vr_dlight_uncapped` | `1` | yes | C | graphics | Graphics - Lights | 2 | 0 |  | 1126 |
| `vr_light_contrast` | `2` | yes | C | graphics | Graphics - Lights | 8 | 0 | vr_defaults.cfg | 1127 |
| `vr_decals` | `1` | yes | C | graphics | Gore; Graphics - Models and Effects | 21 | 0 |  | 1128 |
| `vr_decal_max` | `1024` | yes | C | graphics | Gore; Graphics - Models and Effects | 3 | 0 |  | 1129 |
| `vr_decal_life` | `120` | yes | C | graphics | Gore; Graphics - Models and Effects | 1 | 0 | vr_defaults.cfg | 1130 |
| `vr_gib_blood` | `1` | yes | C | graphics | Gore; Graphics - Models and Effects | 4 | 0 |  | 1131 |
| `vr_gib_blood_trail` | `1` | yes | C | graphics | Gore; Graphics - Models and Effects | 2 | 0 | vr_defaults.cfg | 1132 |
| `vr_gore` | `2` | yes | C | gameplay | Gore | 12 | 0 |  | 1133 |
| `vr_gib_speed_melee` | `0.15` | yes | C | gameplay | Gore | 1 | 1 |  | 1135 |
| `vr_gib_speed_light` | `0.25` | yes | C | graphics | Gore | 1 | 0 |  | 1136 |
| `vr_gib_speed_heavy` | `1` | yes | C | gameplay | Gore | 1 | 1 |  | 1137 |
| `vr_gore_spray` | `1` | yes | C | gameplay | Gore | 2 | 0 |  | 1138 |
| `vr_gore_size` | `1` | yes | C | gameplay | Gore | 1 | 0 |  | 1139 |
| `vr_gore_pools` | `1` | yes | C | gameplay | Gore | 3 | 0 |  | 1140 |
| `vr_gore_drips` | `1` | yes | C | gameplay | Gore | 1 | 0 |  | 1141 |
| `vr_gore_stick` | `8` | yes | C | controls,gameplay | Gore | 3 | 0 |  | 1142 |
| `vr_head_flies` | `0` | yes | C | gameplay | Gore | 1 | 0 |  | 1145 |
| `vr_smallgibs` | `1` | yes | D | gameplay | Small Gibs | 3 | 0 |  | 1148 |
| `vr_smallgibs_player` | `1` | yes | D | gameplay,multiplayer | Small Gibs | 2 | 0 |  | 1149 |
| `vr_smallgibs_min_damage` | `10` | yes | D | gameplay | Small Gibs | 1 | 0 |  | 1150 |
| `vr_smallgibs_full_damage` | `35` | yes | D | gameplay | Small Gibs | 1 | 0 |  | 1151 |
| `vr_smallgibs_curve` | `1.7` | yes | D | gameplay | Small Gibs | 1 | 0 |  | 1152 |
| `vr_smallgibs_damage_per_gib` | `30` | yes | D | gameplay | Small Gibs | 1 | 0 |  | 1153 |
| `vr_smallgibs_per_hit` | `6` | yes | D | gameplay | Small Gibs | 1 | 0 |  | 1154 |
| `vr_smallgibs_shots` | `1` | yes | D | gameplay | Small Gibs | 1 | 0 |  | 1155 |
| `vr_smallgibs_nails` | `0.4` | yes | D | gameplay | Small Gibs | 1 | 0 |  | 1156 |
| `vr_smallgibs_blades` | `1.7` | yes | D | gameplay | Small Gibs | 1 | 0 |  | 1157 |
| `vr_smallgibs_blunt` | `0.3` | yes | D | gameplay | Small Gibs | 1 | 0 |  | 1158 |
| `vr_smallgibs_props` | `1.2` | yes | D | gameplay | Small Gibs | 1 | 0 |  | 1159 |
| `vr_smallgibs_explosions` | `1` | yes | D | gameplay | Small Gibs | 1 | 0 |  | 1160 |
| `vr_smallgibs_other` | `0.5` | yes | D | gameplay | Small Gibs | 1 | 0 |  | 1161 |
| `vr_smallgibs_saw_interval` | `0.1` | yes | D | gameplay | Small Gibs | 1 | 1 |  | 1162 |
| `vr_smallgibs_gibbing` | `12` | yes | D | gameplay | Small Gibs | 1 | 0 |  | 1163 |
| `vr_smallgibs_burst` | `4` | yes | D | gameplay | Small Gibs | 1 | 1 |  | 1164 |
| `vr_smallgibs_speed` | `3` | yes | D | gameplay | Small Gibs | 3 | 0 |  | 1165 |
| `vr_smallgibs_up` | `7` | yes | D | gameplay | Small Gibs | 3 | 0 |  | 1166 |
| `vr_smallgibs_speed_melee` | `0.35` | yes | D | gameplay | Small Gibs | 1 | 0 |  | 1168 |
| `vr_smallgibs_up_melee` | `0.5` | yes | D | gameplay | Small Gibs | 1 | 0 |  | 1169 |
| `vr_smallgibs_speed_saw` | `0.5` | yes | D | gameplay | Small Gibs | 1 | 0 |  | 1170 |
| `vr_smallgibs_up_saw` | `0.6` | yes | D | gameplay | Small Gibs | 1 | 0 |  | 1171 |
| `vr_smallgibs_speed_guns` | `0.85` | yes | D | gameplay | Small Gibs | 1 | 0 |  | 1172 |
| `vr_smallgibs_up_guns` | `0.9` | yes | D | gameplay | Small Gibs | 1 | 0 |  | 1173 |
| `vr_smallgibs_speed_explosions` | `1` | yes | D | gameplay | Small Gibs | 1 | 0 |  | 1174 |
| `vr_smallgibs_up_explosions` | `1` | yes | D | gameplay | Small Gibs | 1 | 0 |  | 1175 |
| `vr_smallgibs_speed_thrown` | `1` | yes | D | gameplay | Small Gibs | 1 | 0 |  | 1176 |
| `vr_smallgibs_up_thrown` | `1` | yes | D | gameplay | Small Gibs | 1 | 0 |  | 1177 |
| `vr_smallgibs_speed_other` | `1` | yes | D | gameplay | Small Gibs | 1 | 0 |  | 1178 |
| `vr_smallgibs_up_other` | `1` | yes | D | gameplay | Small Gibs | 1 | 0 |  | 1179 |
| `vr_smallgibs_speed_gibbing` | `1` | yes | D | gameplay | Small Gibs | 1 | 0 |  | 1180 |
| `vr_smallgibs_up_gibbing` | `0.7` | yes | D | gameplay | Small Gibs | 1 | 0 |  | 1181 |
| `vr_smallgibs_speed_corpse` | `1` | yes | D | gameplay | Small Gibs | 1 | 0 |  | 1182 |
| `vr_smallgibs_up_corpse` | `0.7` | yes | D | gameplay | Small Gibs | 1 | 0 |  | 1183 |
| `vr_smallgibs_speed_burst` | `1` | yes | D | gameplay | Small Gibs | 1 | 0 |  | 1184 |
| `vr_smallgibs_up_burst` | `1` | yes | D | gameplay | Small Gibs | 1 | 0 |  | 1185 |
| `vr_smallgibs_pass_inside` | `1` | yes | D | gameplay | Small Gibs | 1 | 0 |  | 1186 |
| `vr_smallgibs_size_min` | `0.65` | yes | D | gameplay | Small Gibs | 1 | 0 |  | 1187 |
| `vr_smallgibs_size_max` | `1.15` | yes | D | gameplay | Small Gibs | 1 | 0 |  | 1188 |
| `vr_smallgibs_mass` | `0.3` | yes | D | gameplay | Small Gibs | 1 | 0 |  | 1189 |
| `vr_smallgibs_grace` | `0.1` | yes | D | gameplay | Small Gibs | 1 | 5 |  | 1190 |
| `vr_smallgibs_blow_grace` | `0.3` | yes | D | gameplay | Small Gibs | 2 | 0 |  | 1191 |
| `vr_smallgibs_blow_burst` | `1` | yes | D | gameplay | Small Gibs | 1 | 0 |  | 1192 |
| `vr_smallgibs_max` | `64` | yes | D | gameplay | Small Gibs | 1 | 4 |  | 1193 |
| `vr_smallgibs_time` | `20` | yes | D | gameplay | Small Gibs | 1 | 2 |  | 1194 |
| `vr_smallgibs_destroy` | `1` | yes | D | gameplay | Small Gibs | 2 | 0 |  | 1195 |
| `vr_gore_stick_thrown` | `0.5` | yes | C | controls,gameplay | Gore | 8 | 2 |  | 1196 |
| `vr_gore_stick_speed` | `180` | yes | C | controls,gameplay | Gore | 4 | 0 |  | 1197 |
| `vr_smallgibs_test` | `0` | no | D | gameplay | — | 0 | 2 |  | 1198 |
| `vr_smallgibs_trace` | `0` | no | D | dev | Debug - Tools | 8 | 1 |  | 1199 |
| `vr_smallgibs_test_n` | `0` | no | D | gameplay | — | 0 | 1 |  | 1200 |
| `vr_smallgibs_test_dmg` | `0` | no | D | gameplay | — | 0 | 1 |  | 1201 |
| `vr_smallgibs_test_dummy` | `0` | no | D | dev | Debug - Tools | 0 | 1 |  | 1202 |
| `vr_smallgibs_test_crowd` | `0` | no | D | gameplay | — | 0 | 1 |  | 1203 |
| `vr_smallgibs_test_blasts` | `0` | no | D | gameplay | — | 0 | 1 |  | 1204 |
| `vr_smallgibs_test_hand` | `0` | no | D | gameplay | — | 0 | 3 |  | 1205 |
| `vr_smallgibs_test_dist` | `0` | no | D | gameplay | — | 0 | 3 |  | 1206 |
| `vr_body_blood_amount` | `1` | yes | C | gameplay | Gore | 3 | 0 |  | 1207 |
| `vr_body_blood_marks` | `1` | yes | C | gameplay | Gore | 2 | 0 |  | 1208 |
| `vr_body_blood_mark_size` | `1` | yes | C | gameplay | Gore | 2 | 0 |  | 1209 |
| `vr_body_blood_floor` | `1` | yes | C | gameplay | Gore | 1 | 0 |  | 1210 |
| `vr_bloom` | `0.3` | yes | C | graphics | Graphics - Post-processing | 5 | 0 | vr_defaults.cfg | 1211 |
| `vr_bloom_threshold` | `0.7` | yes | C | graphics | Graphics - Post-processing | 1 | 0 | vr_defaults.cfg | 1212 |
| `vr_bloom_radius` | `1` | yes | C | graphics | Graphics - Post-processing | 2 | 0 |  | 1213 |
| `vr_flash_scale` | `1` | yes | C | graphics | Graphics - Lights | 3 | 0 | vr_defaults.cfg | 1214 |
| `vr_explosion_light_scale` | `1` | yes | C | graphics | Graphics - Lights | 4 | 0 | vr_defaults.cfg | 1215 |
| `vr_colored_lights` | `1` | yes | C | graphics | Graphics - Lights | 3 | 0 |  | 1216 |
| `vr_projectile_lights` | `1` | yes | C | graphics | Graphics - Lights | 4 | 0 |  | 1217 |
| `vr_lavanail_lights` | `8` | yes | C | graphics | Graphics - Lights | 2 | 0 |  | 1218 |
| `vr_lavagun_light` | `1` | yes | C | graphics | Graphics - Lights | 2 | 0 |  | 1219 |
| `vr_lavagun_light_radius` | `72` | yes | C | graphics | Graphics - Lights | 1 | 0 |  | 1220 |
| `vr_lavagun_light_flicker` | `0.35` | yes | C | graphics | Graphics - Lights | 1 | 0 |  | 1221 |
| `vr_lavagun_light_idle` | `0.4` | yes | C | graphics | Graphics - Lights | 2 | 0 |  | 1222 |
| `vr_beam_lights` | `6` | yes | C | graphics | Graphics - Lights | 2 | 0 |  | 1223 |
| `vr_beam_arcs` | `1` | yes | C | graphics | Graphics - Lights | 1 | 0 |  | 1226 |
| `vr_beam_arcs_spread` | `6` | yes | C | graphics | Graphics - Lights | 1 | 0 |  | 1227 |
| `vr_beam_arcs_width` | `1` | yes | C | graphics | Graphics - Lights | 1 | 0 |  | 1228 |
| `vr_torch_lights` | `8` | yes | C | graphics | Graphics - Lights | 2 | 0 |  | 1229 |
| `vr_torch_light_scale` | `1` | yes | C | graphics | Graphics - Lights | 1 | 0 | vr_defaults.cfg | 1230 |
| `vr_torch_light_shadows` | `0` | yes | C | graphics | Graphics - Lights | 1 | 0 | vr_defaults.cfg | 1231 |
| `vr_weapon_screen_light` | `1` | yes | C | HUD,graphics | Graphics - Lights | 2 | 0 | vr_defaults.cfg | 1232 |
| `vr_screen_glow` | `1` | yes | C | HUD,graphics | Graphics - Models and Effects; Screens | 4 | 0 | vr_defaults.cfg | 1233 |
| `vr_screen_text_glow` | `1` | yes | C | HUD,graphics | Graphics - Models and Effects; Screens | 2 | 0 | vr_defaults.cfg | 1234 |
| `vr_weapon_glow` | `1` | yes | C | graphics | Graphics - Models and Effects | 2 | 0 | vr_defaults.cfg | 1235 |
| `vr_gamma` | `1` | yes | B | graphics | Graphics | 1 | 0 |  | 1237 |
| `vr_contrast` | `1` | yes | B | graphics | Graphics | 1 | 0 |  | 1238 |
| `vr_dlight_falloff` | `1` | yes | C | graphics | Graphics - Lights | 10 | 0 |  | 1239 |
| `vr_specular` | `0.125` | yes | C | graphics | Graphics - Surfaces | 3 | 0 | vr_defaults.cfg | 1240 |
| `vr_specular_aa` | `1` | yes | C | graphics | Graphics - Surfaces | 1 | 0 |  | 1241 |
| `vr_normalmap_cache` | `1` | no | D | graphics | — | 3 | 0 |  | 1242 |
| `vr_normalmaps` | `1` | yes | C | graphics | Graphics - Surfaces | 8 | 0 |  | 1243 |
| `vr_normalmap_strength` | `1` | yes | C | graphics | Graphics - Surfaces | 2 | 0 | vr_defaults.cfg | 1244 |
| `vr_normalmap_baked` | `1` | yes | C | graphics | Graphics - Surfaces | 2 | 0 |  | 1245 |
| `vr_deluxemap` | `1` | yes | C | graphics | Graphics - Surfaces | 1 | 0 |  | 1246 |
| `vr_normalmap_models` | `1` | yes | C | graphics | Graphics - Surfaces | 1 | 0 | vr_defaults.cfg | 1247 |
| `vr_normalmap_authored` | `1` | yes | C | graphics | Graphics - Surfaces | 1 | 0 |  | 1248 |
| `vr_parallax` | `1` | yes | C | graphics | Graphics - Surfaces | 3 | 0 |  | 1249 |
| `vr_parallax_depth` | `3` | yes | C | graphics | Graphics - Surfaces | 2 | 0 | vr_defaults.cfg | 1250 |
| `vr_parallax_distance` | `512` | yes | C | graphics | Graphics - Surfaces | 1 | 0 |  | 1251 |
| `vr_parallax_steps` | `16` | yes | C? | graphics | — | 1 | 0 |  | 1252 |
| `vr_parallax_items` | `1.5` | yes | C | graphics | Graphics - Surfaces | 1 | 0 |  | 1253 |
| `vr_detail` | `1` | yes | C | graphics | Graphics - Surfaces | 6 | 0 |  | 1254 |
| `vr_detail_strength` | `1` | yes | C | graphics | Graphics - Surfaces | 1 | 0 | vr_defaults.cfg | 1255 |
| `vr_detail_distance` | `144` | yes | C | graphics | Graphics - Surfaces | 1 | 0 | vr_defaults.cfg | 1256 |
| `vr_detail_fine` | `1` | yes | C? | graphics | — | 1 | 0 |  | 1257 |
| `vr_parallax_models` | `0` | yes | C | graphics | Graphics - Surfaces | 1 | 0 | vr_defaults.cfg | 1258 |
| `vr_parallax_authored` | `1` | yes | C | graphics | Graphics - Surfaces | 1 | 0 | vr_defaults.cfg | 1259 |
| `vr_extmaps` | `1` | yes | C | graphics | Graphics - Surfaces | 7 | 0 |  | 1260 |
| `vr_extmaps_dir` | `textures_quetoo` | yes | C? | graphics | — | 2 | 0 |  | 1261 |
| `vr_extmaps_normals` | `1` | yes | C | graphics | Graphics - Surfaces | 1 | 0 |  | 1262 |
| `vr_extmaps_spec` | `1` | yes | C | graphics | Graphics - Surfaces | 1 | 0 |  | 1263 |
| `vr_extmaps_spec_scale` | `4` | yes | C | graphics | Graphics - Surfaces | 1 | 0 |  | 1264 |
| `vr_extmaps_luma` | `1` | yes | C | graphics | Graphics - Surfaces | 1 | 0 |  | 1265 |
| `vr_extmaps_green` | `0` | yes | C? | graphics | — | 1 | 0 |  | 1266 |
| `vr_extmaps_match` | `0.5` | yes | C | graphics | Graphics - Surfaces | 1 | 0 |  | 1267 |
| `vr_extmaps_ab` | `0` | no | D | dev,graphics | Debug - Views | 1 | 0 |  | 1268 |
| `vr_retro` | `0` | yes | C | graphics | Graphics - Retro Textures | 12 | 0 |  | 1269 |
| `vr_retro_ab` | `0` | no | D | dev,graphics | Debug - Views | 1 | 0 |  | 1270 |
| `vr_model_light_parity` | `1` | yes | C | graphics | Graphics - Models and Effects | 3 | 0 |  | 1271 |
| `vr_viewmodel_minlight` | `8` | yes | C | graphics | Graphics - Models and Effects | 2 | 0 | vr_defaults.cfg | 1272 |
| `vr_texture_smooth` | `1` | yes | B | graphics | Graphics | 3 | 0 |  | 1273 |
| `vr_alpha_coverage` | `1` | yes | B | graphics | Graphics | 4 | 0 |  | 1274 |
| `vr_bloom_white` | `0.5` | yes | C | graphics | Graphics - Post-processing | 1 | 0 | vr_defaults.cfg | 1275 |
| `vr_bloom_color` | `1.5` | yes | C | graphics | Graphics - Post-processing | 1 | 0 | vr_defaults.cfg | 1276 |
| `vr_bloom_adapt` | `4` | yes | C | graphics | Graphics - Post-processing | 1 | 0 |  | 1277 |
| `vr_tonemap` | `1` | yes | C | graphics | Graphics - Post-processing | 7 | 0 |  | 1279 |
| `vr_exposure` | `1` | yes | C | graphics | Graphics - Post-processing | 1 | 0 |  | 1280 |
| `vr_dither` | `1` | yes | C | graphics | Graphics - Post-processing | 2 | 0 |  | 1281 |
| `vr_grade` | `1` | yes | C | graphics | Graphics - Post-processing | 2 | 0 |  | 1282 |
| `vr_grade_strength` | `1` | yes | C | graphics | Graphics - Post-processing | 1 | 0 |  | 1283 |
| `vr_eyeshot` | `0` | no | D | gameplay | — | 7 | 0 |  | 1284 |
| `vr_map_liquid_alpha` | `0` | yes | C | graphics | Transparency | 2 | 0 |  | 1286 |
| `vr_water_waves` | `1` | yes | C | graphics | Graphics - Liquids | 2 | 0 | vr_defaults.cfg | 1287 |
| `vr_water_fresnel` | `1` | yes | C | graphics | Graphics - Liquids | 2 | 0 | vr_defaults.cfg | 1288 |
| `vr_water_refraction` | `1` | yes | C | graphics | Graphics - Liquids | 2 | 0 | vr_defaults.cfg | 1289 |
| `vr_water_glints` | `1` | yes | C | graphics | Graphics - Liquids | 2 | 0 | vr_defaults.cfg | 1290 |
| `vr_water_lava_glow` | `1` | yes | C | graphics | Graphics - Liquids | 2 | 0 |  | 1291 |
| `vr_water_caustics` | `0.6` | yes | C | graphics | Graphics - Liquids | 2 | 0 | vr_defaults.cfg | 1292 |
| `vr_water_underwater` | `1` | yes | C | graphics | Graphics - Liquids | 4 | 0 |  | 1293 |
| `vr_water_wobble` | `1` | yes | C | comfort,graphics | Graphics - Liquids | 2 | 0 |  | 1294 |
| `vr_water_geo_waves` | `1` | yes | C | graphics | Graphics - Liquids | 3 | 0 |  | 1295 |
| `vr_water_geo_amplitude` | `3` | yes | C | graphics | Graphics - Liquids | 2 | 0 | vr_defaults.cfg | 1296 |
| `vr_water_geo_cell` | `16` | yes | C? | gameplay | — | 2 | 0 |  | 1297 |
| `vr_water_foam` | `1` | yes | C | graphics | Graphics - Liquids | 3 | 0 |  | 1298 |
| `vr_heat_haze` | `1` | yes | C | graphics | Graphics - Liquids | 4 | 0 |  | 1299 |
| `vr_water_ripples` | `1` | yes | C | graphics | Graphics - Liquids | 4 | 0 |  | 1300 |
| `vr_water_ripple_amplitude` | `8` | yes | C | graphics | Graphics - Liquids | 1 | 0 | vr_defaults.cfg | 1301 |
| `vr_water_ripple_normal` | `0.6` | yes | C | graphics | Graphics - Liquids | 1 | 0 | vr_defaults.cfg | 1302 |
| `vr_water_ripple_speed` | `32` | yes | C | graphics | Graphics - Liquids | 1 | 0 |  | 1303 |
| `vr_water_ripple_decay` | `2` | yes | C | graphics | Graphics - Liquids | 1 | 0 |  | 1304 |
| `vr_water_ripple_wavelength` | `64` | yes | C | graphics | Graphics - Liquids | 1 | 0 | vr_defaults.cfg | 1305 |
| `vr_water_ripple_max` | `32` | yes | C | graphics | Graphics - Liquids | 1 | 0 |  | 1306 |
| `vr_positional_damage` | `1` | yes | C | gameplay | Damage and Knockback | 4 | 0 |  | 1307 |
| `vr_headshot_sound` | `0.5` | yes | C | audio | Damage and Knockback | 1 | 0 | vr_defaults.cfg | 1308 |
| `vr_climb` | `0` | yes | A | gameplay | Climbing | 10 | 0 | vr_defaults.cfg | 1309 |
| `vr_climb_min_height` | `30` | yes | C | gameplay | Climbing | 1 | 0 |  | 1310 |
| `vr_climb_fling` | `1` | yes | C | gameplay | Climbing | 1 | 0 |  | 1311 |
| `vr_climb_blast_letgo` | `30` | yes | C | gameplay | Climbing | 1 | 0 |  | 1312 |
| `vr_climb_leniency` | `2` | yes | C | gameplay | Climbing | 2 | 0 |  | 1313 |
| `vr_climb_leniency_air` | `4` | yes | C | gameplay | Climbing | 3 | 0 |  | 1314 |
| `vr_climb_air_grab_time` | `0.3` | yes | C | gameplay | Climbing | 1 | 0 |  | 1315 |
| `vr_climb_mantle_grunt` | `0.6` | yes | C | audio | Climbing | 1 | 0 |  | 1316 |
| `vr_climb_mantle_grunt_sound` | `4` | yes | C | audio | Climbing | 1 | 0 |  | 1317 |
| `vr_climb_touch` | `4.5` | yes | C | gameplay | Climbing | 1 | 0 |  | 1318 |
| `vr_climb_over_top` | `16` | yes | C | gameplay | Climbing | 1 | 0 |  | 1319 |
| `vr_climb_slide` | `1` | yes | C | gameplay | Climbing | 1 | 0 |  | 1320 |
| `vr_climb_overhang_stretch` | `16` | no | D | gameplay | — | 2 | 0 |  | 1321 |
| `vr_climb_hand_out` | `10` | yes | C | gameplay | Climbing | 1 | 0 |  | 1322 |
| `vr_climb_hand_up` | `1` | yes | C | gameplay | Climbing | 1 | 0 | vr_defaults.cfg | 1323 |
| `vr_climb_hand_side` | `7.5` | yes | C | gameplay | Climbing | 1 | 0 |  | 1324 |
| `vr_climb_hand_turn_blend` | `0.15` | yes | C | gameplay | Climbing | 1 | 0 |  | 1325 |
| `vr_climb_hand_pitch` | `60` | yes | C | gameplay | Climbing | 1 | 0 |  | 1326 |
| `vr_climb_hand_yaw` | `10` | yes | C | gameplay | Climbing | 1 | 0 |  | 1327 |
| `vr_climb_hand_roll` | `0` | yes | C | gameplay | Climbing | 1 | 0 |  | 1328 |
| `vr_climb_mover_crush` | `0` | yes | C? | gameplay | — | 1 | 0 |  | 1329 |
| `vr_debug_ledges` | `0` | no | D | dev | Debug - Views | 3 | 0 |  | 1330 |
| `vr_climb_stamina` | `1` | yes | C | gameplay | Climbing; Stamina | 2 | 0 |  | 1331 |
| `vr_climb_stamina_rate` | `5` | yes | C | gameplay | Climbing; Stamina | 3 | 0 |  | 1332 |
| `vr_climb_stamina_rate_2h` | `2` | yes | C | gameplay | Climbing; Stamina | 1 | 0 |  | 1333 |
| `vr_climb_stamina_slip` | `0` | yes | C | gameplay | Climbing | 1 | 0 |  | 1334 |
| `vr_fatigue_shake` | `0.6` | yes | C | gameplay | Stamina | 2 | 0 |  | 1337 |
| `vr_fatigue_shake_angle` | `1.5` | yes | C | gameplay | Stamina | 2 | 0 |  | 1338 |
| `vr_fatigue_shake_from` | `0.4` | yes | C | gameplay | Stamina | 1 | 0 |  | 1339 |
| `vr_fatigue_shake_speed` | `1` | yes | C | gameplay | Stamina | 1 | 0 |  | 1340 |
| `vr_fatigue_shake_always` | `0` | yes | C | gameplay | Stamina | 1 | 0 |  | 1341 |
| `vr_pain_knock` | `1` | yes | C | gameplay | Damage and Knockback | 1 | 0 |  | 1344 |
| `vr_pain_knock_strength` | `0.75` | yes | C | gameplay | Damage and Knockback | 1 | 0 |  | 1345 |
| `vr_pain_knock_max` | `10` | yes | C | gameplay | Damage and Knockback | 2 | 0 |  | 1346 |
| `vr_pain_knock_time` | `0.35` | yes | C | gameplay | Damage and Knockback | 1 | 0 |  | 1347 |
| `vr_pain_knock_tip` | `3` | yes | C | gameplay | Damage and Knockback | 1 | 0 |  | 1348 |
| `vr_pain_knock_seen` | `1` | yes | C | gameplay | Damage and Knockback | 1 | 0 |  | 1349 |
| `vr_pain_haptics` | `1` | yes | C | controls,gameplay | Damage and Knockback | 1 | 0 |  | 1350 |
| `vr_climb_debug` | `0` | no | D | dev | Debug - Logging | 13 | 0 |  | 1351 |
| `vr_swim` | `1` | yes | C | gameplay | Swimming | 1 | 0 |  | 1352 |
| `vr_swim_shallow_speed` | `0.85` | yes | C | gameplay | Swimming | 1 | 0 |  | 1353 |
| `vr_swim_wade_speed` | `0.6` | yes | C | gameplay | Swimming | 1 | 0 |  | 1354 |
| `vr_swim_stick_speed` | `0.2` | yes | C | controls,gameplay | Swimming | 1 | 0 | vr_defaults.cfg | 1355 |
| `vr_swim_stroke_assist` | `0.5` | yes | C | gameplay | Swimming | 1 | 0 |  | 1356 |
| `vr_cfg_version` | `0` | yes | D | gameplay | — | 0 | 0 |  | 1357 |
| `vr_swim_stroke` | `10` | yes | C | gameplay | Swimming | 1 | 0 | vr_defaults.cfg | 1358 |
| `vr_swim_stroke_min` | `0.4` | yes | C | gameplay | Swimming | 1 | 0 |  | 1359 |
| `vr_swim_palm` | `0.6` | yes | C | gameplay | Swimming | 1 | 0 | vr_defaults.cfg | 1360 |
| `vr_swim_max_speed` | `400` | yes | C | gameplay | Swimming | 1 | 0 | vr_defaults.cfg | 1361 |
| `vr_swim_look` | `0.6` | yes | C | gameplay | Swimming | 1 | 0 | vr_defaults.cfg | 1362 |
| `vr_swim_recovery` | `0.1` | yes | C | gameplay | Swimming | 1 | 0 | vr_defaults.cfg | 1363 |
| `vr_swim_glide` | `0.5` | yes | C | gameplay | Swimming | 1 | 0 | vr_defaults.cfg | 1364 |
| `vr_swim_power_threshold` | `1.1` | yes | C | gameplay | Swimming | 1 | 0 |  | 1365 |
| `vr_swim_power_knee` | `0.5` | yes | C | gameplay | Swimming | 1 | 0 |  | 1366 |
| `vr_swim_power_whole` | `1` | yes | C | gameplay | Swimming | 1 | 0 |  | 1367 |
| `vr_swim_intent_memory` | `1.5` | yes | C | gameplay | Swimming | 1 | 0 |  | 1368 |
| `vr_swim_reverse_damp` | `0.85` | yes | C | gameplay | Swimming | 1 | 0 |  | 1369 |
| `vr_swim_reverse_speed` | `1` | yes | C | gameplay | Swimming | 1 | 0 |  | 1370 |
| `vr_swim_flat_exp` | `1` | yes | C | gameplay | Swimming | 1 | 0 |  | 1371 |
| `vr_swim_speed_exp` | `2` | yes | C | gameplay | Swimming | 1 | 0 |  | 1372 |
| `vr_swim_palm_dir` | `0` | yes | C | gameplay | Swimming | 1 | 0 | vr_defaults.cfg | 1373 |
| `vr_swim_against_palm` | `0.25` | yes | C | gameplay | Swimming | 1 | 0 |  | 1374 |
| `vr_swim_stroke_pitch` | `-8` | yes | C | gameplay | Swimming | 1 | 0 |  | 1375 |
| `vr_air_supply` | `2` | yes | C | gameplay | Swimming | 1 | 0 |  | 1376 |
| `vr_swim_debug` | `0` | no | D | dev | Debug - Logging | 1 | 0 |  | 1377 |
| `vr_sword_damage_mult` | `1.5` | yes | C | gameplay | Weapon Damage | 1 | 0 | vr_defaults.cfg | 1379 |
| `vr_crowbar_damage` | `20` | yes | C | gameplay | Weapon Damage | 1 | 0 |  | 1380 |
| `vr_dmg_shotgun` | `4` | yes | C | gameplay | Weapon Damage | 2 | 1 |  | 1384 |
| `vr_dmg_super_shotgun` | `4` | yes | C | gameplay | Weapon Damage | 1 | 1 |  | 1385 |
| `vr_dmg_nail` | `9` | yes | C | gameplay | Weapon Damage | 1 | 1 |  | 1386 |
| `vr_dmg_super_nail` | `18` | yes | C | gameplay | Weapon Damage | 1 | 1 |  | 1387 |
| `vr_dmg_grenade` | `120` | yes | C | gameplay | Weapon Damage | 1 | 0 |  | 1388 |
| `vr_dmg_rocket` | `100` | yes | C | gameplay | Weapon Damage | 1 | 0 |  | 1389 |
| `vr_dmg_lightning` | `30` | yes | C | graphics | Weapon Damage | 1 | 0 |  | 1390 |
| `vr_dmg_proximity` | `95` | yes | C | gameplay | Weapon Damage | 1 | 0 |  | 1391 |
| `vr_dmg_laser` | `18` | yes | C | gameplay | Weapon Damage | 2 | 0 |  | 1392 |
| `vr_dmg_mjolnir_lightning` | `80` | yes | C | graphics | Weapon Damage | 1 | 0 |  | 1393 |
| `vr_dmg_lava_nail` | `15` | yes | C | gameplay | Weapon Damage | 2 | 0 |  | 1394 |
| `vr_dmg_super_lava_nail` | `30` | yes | C | gameplay | Weapon Damage | 2 | 0 |  | 1395 |
| `vr_dmg_multi_grenade` | `90` | yes | C | gameplay | Weapon Damage | 1 | 0 |  | 1396 |
| `vr_dmg_multi_rocket` | `60` | yes | C | gameplay | Weapon Damage | 1 | 0 |  | 1397 |
| `vr_dmg_plasma` | `80` | yes | C | gameplay | Weapon Damage | 2 | 0 |  | 1398 |
| `vr_dmg_fist` | `10` | yes | C | gameplay | Weapon Damage | 1 | 0 |  | 1399 |
| `vr_dmg_axe` | `20` | yes | C | gameplay | Weapon Damage | 1 | 0 |  | 1400 |
| `vr_dmg_gun_bash` | `12` | yes | C | gameplay | Weapon Damage | 1 | 0 |  | 1401 |
| `vr_dmg_chainsaw_swing` | `20` | yes | C | gameplay | Weapon Damage | 1 | 0 |  | 1402 |
| `vr_dmg_mjolnir` | `25` | yes | C | gameplay | Weapon Damage | 1 | 0 |  | 1403 |
| `vr_chainsaw_damage` | `80` | yes | C | gameplay | Weapon Damage | 1 | 1 |  | 1404 |
| `vr_chainsaw_fuel_use` | `5` | yes | C | gameplay | Enemy Weapons | 1 | 0 |  | 1405 |
| `vr_chainsaw_idle_fuel_use` | `0.5` | yes | C | gameplay | Enemy Weapons | 3 | 0 |  | 1406 |
| `vr_chainsaw_overlap` | `6` | yes | C | gameplay | Enemy Weapons | 1 | 0 |  | 1407 |
| `vr_chainsaw_start_chance` | `0.5` | yes | C | gameplay | Enemy Weapons | 1 | 0 |  | 1408 |
| `vr_chainsaw_fail_pulls_min` | `1` | yes | C | gameplay | Enemy Weapons | 1 | 0 |  | 1409 |
| `vr_chainsaw_fail_pulls_max` | `2` | yes | C | gameplay | Enemy Weapons | 1 | 0 |  | 1410 |
| `vr_chainsaw_pull_distance` | `30` | yes | C | gameplay | Enemy Weapons | 1 | 0 |  | 1411 |
| `vr_chainsaw_pull_speed` | `1.2` | yes | C | gameplay | Enemy Weapons | 1 | 0 |  | 1412 |
| `vr_chainsaw_drop_fuel_min` | `40` | yes | C | gameplay | Enemy Weapons | 1 | 0 |  | 1413 |
| `vr_chainsaw_drop_run` | `3` | yes | C | gameplay | Enemy Weapons | 4 | 0 |  | 1414 |
| `vr_chainsaw_smoke` | `12` | yes | D | gameplay | Chainsaw Engine | 2 | 0 |  | 1418 |
| `vr_chainsaw_smoke_alpha` | `0.6` | yes | D | gameplay | Chainsaw Engine | 1 | 0 |  | 1419 |
| `vr_chainsaw_shake` | `2.5` | yes | D | gameplay | Chainsaw Engine | 1 | 0 |  | 1420 |
| `vr_chainsaw_shake_2h` | `1.25` | yes | D | gameplay | Chainsaw Engine | 1 | 0 |  | 1421 |
| `vr_chainsaw_shake_ground` | `0.85` | yes | D | gameplay | Chainsaw Engine | 1 | 0 |  | 1422 |
| `vr_chainsaw_pull_shake` | `3` | yes | D | gameplay | Chainsaw Engine | 1 | 0 |  | 1426 |
| `vr_chainsaw_pull_shake_time` | `0.35` | yes | D | gameplay | Chainsaw Engine | 1 | 0 |  | 1427 |
| `vr_chainsaw_pull_smoke` | `12` | yes | D | gameplay | Chainsaw Engine | 1 | 0 |  | 1428 |
| `vr_chainsaw_pull_sparks` | `24` | yes | D | gameplay | Chainsaw Engine | 1 | 0 |  | 1429 |
| `vr_chainsaw_model_handle` | `1` | yes | D | gameplay | Chainsaw Engine | 1 | 0 |  | 1430 |
| `vr_gruntgun_damage` | `5` | yes | C | audio | Weapon Damage | 1 | 0 |  | 1431 |
| `vr_gruntgun_ammo` | `30` | yes | C | audio | Enemy Weapons | 1 | 0 |  | 1432 |
| `vr_grunt_burst` | `1` | yes | C | audio | Enemy Weapons | 5 | 0 |  | 1433 |
| `vr_grunt_burst_damage` | `5` | yes | C | audio | Enemy Weapons | 1 | 0 |  | 1434 |
| `vr_enfrifle_damage` | `15` | yes | C | gameplay | Weapon Damage | 1 | 0 |  | 1435 |
| `vr_enfrifle_speed` | `1800` | yes | C | gameplay | Enemy Weapons | 1 | 0 |  | 1436 |
| `vr_enfrifle_ammo` | `20` | yes | C | gameplay | Enemy Weapons | 1 | 0 |  | 1437 |
| `vr_debug_chainsaw` | `0` | no | D | dev | Debug - Logging | 14 | 0 |  | 1438 |
| `vr_wofs_version` | `0` | yes | D | gameplay | — | 41 | 0 |  | 1439 |
| `vr_carry_take` | `0` | yes | C | gameplay | Carrying | 2 | 0 |  | 1440 |
| `vr_carry_local` | `1` | yes | C | gameplay | Carrying | 1 | 0 |  | 1441 |
| `vr_held_surface_fit` | `1` | yes | C | gameplay | Carrying | 1 | 0 |  | 1442 |
| `vr_held_collide` | `1` | yes | C | gameplay | Carrying | 1 | 0 |  | 1443 |
| `vr_held_collide_max` | `5` | yes | C | gameplay | Carrying | 1 | 0 |  | 1444 |
| `vr_held_collide_walls` | `1` | yes | C | gameplay | Carrying | 1 | 0 |  | 1445 |
| `vr_held_collide_wall_max` | `40` | yes | C | gameplay | Carrying | 1 | 0 |  | 1446 |
| `vr_held_collide_monsters` | `1` | yes | C | gameplay | Carrying | 1 | 0 |  | 1447 |
| `vr_held_fit_gap` | `0` | yes | C | gameplay | Carrying | 2 | 0 | vr_defaults.cfg | 1448 |
| `vr_carry_grab_bias` | `0` | yes | C | gameplay | Carrying | 1 | 0 |  | 1450 |
| `vr_carry_grab_drawn` | `1` | yes | C | gameplay | Carrying | 1 | 0 |  | 1451 |
| `vr_weapon_grab_drawn` | `1` | yes | C | gameplay | Carrying | 2 | 0 |  | 1452 |
| `vr_weapon_grab_slack` | `5` | yes | C | gameplay | Carrying | 2 | 0 |  | 1453 |
| `vr_weapon_grab_hotspots` | `1` | yes | C | gameplay | Carrying | 3 | 0 |  | 1454 |
| `vr_weapon_grab_anywhere` | `1` | yes | C | gameplay | Carrying | 7 | 0 |  | 1455 |
| `vr_weapon_grab_anywhere_min` | `12` | yes | C | gameplay | Carrying | 2 | 0 |  | 1456 |
| `vr_weapon_anygrip_mode` | `0` | yes | C | controls,gameplay | Carrying | 1 | 0 |  | 1457 |
| `vr_debug_buttons` | `0` | no | D | controls,dev | Debug - Logging | 5 | 0 |  | 1458 |
| `vr_debug_hands` | `0` | no | D | dev | Debug - Logging | 2 | 0 |  | 1459 |
| `vr_debug_carry` | `0` | no | D | dev | Debug - Views | 18 | 0 |  | 1460 |
| `vr_debug_hand_collide` | `0` | no | D | dev | Debug - Views | 2 | 0 |  | 1461 |
| `vr_debug_arm` | `0` | no | D | dev | Debug - Logging | 4 | 0 |  | 1462 |
| `vr_debug_lean` | `0` | no | D | comfort,dev | Debug - Logging | 1 | 0 |  | 1463 |
| `vr_grapple_test_stuck` | `0` | no | D | dev | Debug - Tests | 1 | 0 |  | 1464 |
| `vr_debug_rope` | `0` | no | D | dev | Debug - Views | 3 | 0 |  | 1465 |
| `vr_debug_physics_shapes` | `0` | no | D | dev | Debug - Views | 4 | 0 |  | 1466 |
| `vr_debug_hand_bones` | `0` | no | D | dev | Debug - Views | 2 | 0 |  | 1467 |
| `vr_carry_two_hands` | `1` | yes | C | gameplay | Carrying | 1 | 0 |  | 1468 |
| `vr_carry_two_hands_reach` | `2` | yes | C | gameplay | Carrying | 1 | 0 |  | 1469 |
| `vr_carry_two_hands_drift` | `8` | yes | C | gameplay | Carrying | 3 | 0 |  | 1470 |
| `vr_carry_two_hands_detach` | `3` | yes | C? | gameplay | — | 2 | 0 |  | 1471 |
| `vr_carry_two_hands_solid` | `1` | yes | C | gameplay | Carrying | 3 | 0 |  | 1472 |
| `vr_carry_two_hands_window` | `0.1` | yes | C? | graphics | — | 6 | 0 |  | 1473 |
| `vr_held_fit_gaps` | `` | yes | C? | gameplay | — | 1 | 0 |  | 1474 |
| `vr_forcegrab_outline` | `1` | yes | C | gameplay | Force Grab | 1 | 0 |  | 1475 |
| `vr_forcegrab_fx` | `1` | yes | C | gameplay | Force Grab | 1 | 0 |  | 1476 |
| `vr_forcegrab_hue` | `-1` | yes | C | HUD | Colours | 3 | 0 |  | 1477 |
| `vr_forcegrab_saturation` | `0.7` | yes | C | HUD | Colours | 3 | 0 |  | 1478 |
| `vr_grab_gibs` | `1` | yes | C | gameplay | Gibs and Corpses | 2 | 0 |  | 1479 |
| `vr_carry` | `1` | yes | C | gameplay | Carrying | 7 | 0 |  | 1480 |
| `vr_armor_wear` | `1` | yes | C | gameplay | Carrying | 4 | 0 |  | 1481 |
| `vr_armor_scale` | `0.5` | yes | C | gameplay | Carrying | 1 | 0 |  | 1482 |
| `vr_item_objects` | `1` | yes | C | gameplay | Carrying | 2 | 0 |  | 1483 |
| `vr_carry_nudge` | `1` | yes | C | gameplay | Carrying | 1 | 0 |  | 1484 |
| `vr_carry_throw_mult` | `1` | yes | C | gameplay | Carrying | 1 | 0 |  | 1485 |
| `vr_carry_melee_mult` | `1.5` | yes | C | gameplay | Carrying | 3 | 0 |  | 1486 |
| `vr_carry_throw_damage` | `8` | yes | C | gameplay | Carrying | 2 | 0 |  | 1487 |
| `vr_gib_throw_damage` | `4` | yes | C | gameplay | Gibs and Corpses | 2 | 0 |  | 1488 |
| `vr_prop_impact_damage` | `1` | yes | D | gameplay | Throwing and Physics | 1 | 0 |  | 1493 |
| `vr_prop_impact_players` | `1` | yes | D | gameplay,multiplayer | Throwing and Physics | 1 | 0 |  | 1494 |
| `vr_gib_spawn_harmless` | `0.3` | yes | D | gameplay | Throwing and Physics | 2 | 1 |  | 1495 |
| `vr_prop_drop_grace` | `0.5` | yes | D | gameplay | Throwing and Physics | 2 | 1 |  | 1496 |
| `vr_prop_drop_falls_only` | `1` | yes | D | gameplay | Throwing and Physics | 2 | 1 |  | 1497 |
| `vr_prop_drop_pass_inside` | `1` | yes | D | gameplay | Throwing and Physics | 1 | 1 |  | 1498 |
| `vr_throw_self_grace` | `0.35` | yes | D | gameplay | Throwing and Physics | 2 | 0 |  | 1499 |
| `vr_prop_impact_min_mass` | `1` | yes | D | gameplay | Throwing and Physics | 1 | 0 |  | 1500 |
| `vr_prop_impact_min_speed` | `8` | yes | D | gameplay | Throwing and Physics | 1 | 0 |  | 1501 |
| `vr_prop_impact_mult` | `1` | yes | D | gameplay | Throwing and Physics | 1 | 0 |  | 1502 |
| `vr_prop_impact_speed_max` | `3` | yes | D | gameplay | Throwing and Physics | 1 | 0 |  | 1503 |
| `vr_prop_impact_weight_curve` | `0.5` | yes | D | gameplay | Throwing and Physics | 1 | 0 |  | 1504 |
| `vr_prop_impact_weight_ref` | `3` | yes | D | gameplay | Throwing and Physics | 1 | 0 |  | 1505 |
| `vr_prop_impact_weight_max` | `2.5` | yes | D | gameplay | Throwing and Physics | 1 | 0 |  | 1506 |
| `vr_gib_destroy` | `1` | yes | C | gameplay | Gibs and Corpses | 4 | 0 |  | 1507 |
| `vr_gib_health` | `12` | yes | C | gameplay | Gibs and Corpses | 1 | 0 |  | 1508 |
| `vr_gib_splat_speed` | `250` | yes | C | gameplay | Gibs and Corpses | 3 | 1 |  | 1509 |
| `vr_corpse_gib` | `1` | yes | C | gameplay | Gibs and Corpses | 4 | 0 |  | 1510 |
| `vr_corpse_health_mult` | `1` | yes | C | gameplay | Gibs and Corpses; Gibs and Corpses - Corpse Damage and Health | 1 | 0 |  | 1512 |
| `vr_corpse_nogib` | `0` | yes | C | gameplay | Gibs and Corpses; Gibs and Corpses - Corpse Damage and Health | 3 | 0 |  | 1513 |
| `vr_corpse_health_grunt` | `80` | yes | D | audio | Gibs and Corpses - Corpse Damage and Health | 1 | 0 |  | 1514 |
| `vr_corpse_health_enforcer` | `80` | yes | D | gameplay | Gibs and Corpses - Corpse Damage and Health | 1 | 0 |  | 1515 |
| `vr_corpse_health_dog` | `80` | yes | D | gameplay | Gibs and Corpses - Corpse Damage and Health | 1 | 0 |  | 1516 |
| `vr_corpse_health_fiend` | `180` | yes | D | gameplay | Gibs and Corpses - Corpse Damage and Health | 1 | 0 |  | 1517 |
| `vr_corpse_health_ogre` | `140` | yes | D | gameplay | Gibs and Corpses - Corpse Damage and Health | 1 | 0 |  | 1518 |
| `vr_corpse_health_knight` | `80` | yes | D | gameplay | Gibs and Corpses - Corpse Damage and Health | 1 | 0 |  | 1519 |
| `vr_corpse_health_hellknight` | `160` | yes | D | gameplay | Gibs and Corpses - Corpse Damage and Health | 1 | 0 |  | 1520 |
| `vr_corpse_health_vore` | `220` | yes | D | gameplay | Gibs and Corpses - Corpse Damage and Health | 1 | 0 |  | 1521 |
| `vr_corpse_health_shambler` | `280` | yes | D | gameplay | Gibs and Corpses - Corpse Damage and Health | 1 | 0 |  | 1522 |
| `vr_corpse_health_scrag` | `80` | yes | D | gameplay | Gibs and Corpses - Corpse Damage and Health | 1 | 0 |  | 1523 |
| `vr_corpse_health_fish` | `80` | yes | D | gameplay | Gibs and Corpses - Corpse Damage and Health | 1 | 0 |  | 1524 |
| `vr_corpse_health_gremlin` | `100` | yes | D | gameplay | Gibs and Corpses - Corpse Damage and Health | 1 | 0 |  | 1525 |
| `vr_corpse_health_scourge` | `180` | yes | D | gameplay | Gibs and Corpses - Corpse Damage and Health | 1 | 0 |  | 1526 |
| `vr_corpse_health_eel` | `80` | yes | D | gameplay | Gibs and Corpses - Corpse Damage and Health | 1 | 0 |  | 1527 |
| `vr_corpse_dmg_shots` | `1` | yes | D | gameplay | Gibs and Corpses - Corpse Damage and Health | 1 | 0 |  | 1528 |
| `vr_corpse_dmg_nails` | `1` | yes | D | gameplay | Gibs and Corpses - Corpse Damage and Health | 1 | 0 |  | 1529 |
| `vr_corpse_dmg_explosions` | `1` | yes | D | gameplay | Gibs and Corpses - Corpse Damage and Health | 1 | 0 |  | 1530 |
| `vr_corpse_dmg_lightning` | `1` | yes | D | graphics | Gibs and Corpses - Corpse Damage and Health | 1 | 0 |  | 1531 |
| `vr_corpse_dmg_blunt` | `1` | yes | D | gameplay | Gibs and Corpses - Corpse Damage and Health | 1 | 0 |  | 1532 |
| `vr_corpse_dmg_fists` | `1` | yes | D | gameplay | Gibs and Corpses - Corpse Damage and Health | 1 | 0 |  | 1533 |
| `vr_corpse_dmg_blades` | `1` | yes | D | gameplay | Gibs and Corpses - Corpse Damage and Health | 1 | 0 |  | 1534 |
| `vr_corpse_dmg_chainsaw` | `1` | yes | D | gameplay | Gibs and Corpses - Corpse Damage and Health | 1 | 0 |  | 1535 |
| `vr_corpse_dmg_props` | `1` | yes | D | gameplay | Gibs and Corpses - Corpse Damage and Health | 1 | 0 |  | 1536 |
| `vr_corpse_dmg_fire` | `1` | yes | D | gameplay | Gibs and Corpses - Corpse Damage and Health | 1 | 0 |  | 1537 |
| `vr_corpse_dmg_bash` | `1` | yes | D | gameplay | Gibs and Corpses - Corpse Damage and Health | 1 | 0 |  | 1538 |
| `vr_corpse_dmg_other` | `1` | yes | D | gameplay | Gibs and Corpses - Corpse Damage and Health | 1 | 0 |  | 1539 |
| `vr_corpse_collide` | `4` | yes | C | gameplay | Gibs and Corpses | 2 | 0 |  | 1540 |
| `vr_corpse_collide_mass` | `150` | yes | C | gameplay | Gibs and Corpses | 2 | 0 |  | 1541 |
| `vr_corpse_collide_friction` | `1` | yes | C | gameplay | Gibs and Corpses | 2 | 0 |  | 1542 |
| `vr_corpse_collide_props` | `1` | yes | C | gameplay | Gibs and Corpses | 2 | 0 |  | 1543 |
| `vr_corpse_collide_thrown` | `1` | yes | C | gameplay | Gibs and Corpses | 2 | 0 |  | 1544 |
| `vr_corpse_collide_held` | `1` | yes | C | gameplay | Gibs and Corpses | 2 | 0 |  | 1545 |
| `vr_corpse_collide_player` | `0` | yes | C | gameplay,multiplayer | Gibs and Corpses | 2 | 0 |  | 1546 |
| `vr_corpse_collide_monsters` | `0` | yes | C | gameplay | Gibs and Corpses | 1 | 0 |  | 1547 |
| `vr_ragdoll` | `1` | yes | C | gameplay | Gibs and Corpses; Gibs and Corpses - Ragdolls | 15 | 0 |  | 1548 |
| `vr_ragdoll_start` | `0.3` | yes | D | gameplay | Gibs and Corpses - Ragdolls | 1 | 0 |  | 1549 |
| `vr_ragdoll_max` | `8` | yes | D | gameplay | Gibs and Corpses - Ragdolls | 2 | 0 |  | 1550 |
| `vr_ragdoll_mass` | `80` | yes | D | gameplay | Gibs and Corpses - Ragdolls | 1 | 0 |  | 1551 |
| `vr_ragdoll_friction` | `0.8` | yes | D | gameplay | Gibs and Corpses - Ragdolls | 1 | 0 |  | 1552 |
| `vr_ragdoll_joint_friction` | `1.5` | yes | D | gameplay | Gibs and Corpses - Ragdolls | 1 | 0 |  | 1553 |
| `vr_ragdoll_damping` | `0.4` | yes | D | gameplay | Gibs and Corpses - Ragdolls | 1 | 0 |  | 1554 |
| `vr_ragdoll_blast` | `2` | yes | D | gameplay | Gibs and Corpses - Ragdolls | 1 | 0 |  | 1555 |
| `vr_ragdoll_inherit` | `1` | yes | D | gameplay | Gibs and Corpses - Ragdolls | 1 | 0 |  | 1556 |
| `vr_debug_ragdoll` | `0` | no | D | dev | Debug - Logging | 15 | 0 |  | 1557 |
| `vr_ragdoll_joint_stiffness` | `0` | yes | D | gameplay | Gibs and Corpses - Ragdolls | 1 | 0 |  | 1558 |
| `vr_ragdoll_limits` | `1` | yes | D | gameplay | Gibs and Corpses - Ragdolls | 1 | 0 |  | 1559 |
| `vr_ragdoll_collide_each` | `1` | yes | D | gameplay | Gibs and Corpses - Ragdolls | 2 | 0 |  | 1560 |
| `vr_ragdoll_grab` | `2` | yes | D | gameplay | Gibs and Corpses - Ragdolls | 5 | 0 |  | 1561 |
| `vr_ragdoll_grab_force` | `3000` | yes | D | gameplay | Gibs and Corpses - Ragdolls | 2 | 0 |  | 1562 |
| `vr_ragdoll_grab_reach` | `6` | yes | D | gameplay | Gibs and Corpses - Ragdolls | 3 | 0 |  | 1563 |
| `vr_ragdoll_grab_fit` | `1` | yes | D | gameplay | Gibs and Corpses - Ragdolls | 2 | 0 |  | 1564 |
| `vr_ragdoll_hand_stick` | `12` | yes | D | controls,gameplay | Gibs and Corpses - Ragdolls | 1 | 0 |  | 1565 |
| `vr_ragdoll_hand_turn` | `60` | yes | D | gameplay | Gibs and Corpses - Ragdolls | 1 | 0 |  | 1566 |
| `vr_ragdoll_throw` | `1` | yes | D | gameplay | Gibs and Corpses - Ragdolls | 1 | 0 |  | 1567 |
| `vr_ragdoll_blood` | `1` | yes | D | gameplay | Gibs and Corpses - Ragdolls | 1 | 0 |  | 1568 |
| `vr_ragdoll_smooth` | `1` | yes | D | gameplay | Gibs and Corpses - Ragdolls | 3 | 0 |  | 1569 |
| `vr_ragdoll_held_local` | `1` | yes | D | gameplay | Gibs and Corpses - Ragdolls | 3 | 0 |  | 1570 |
| `vr_ragdoll_army_start` | `-1` | yes | D | gameplay | Ragdolls - Grunt | 1 | 0 |  | 1573 |
| `vr_ragdoll_army_mass` | `80` | yes | D | gameplay | Ragdolls - Grunt | 1 | 0 |  | 1574 |
| `vr_ragdoll_army_friction` | `-1` | yes | D | gameplay | Ragdolls - Grunt | 1 | 0 |  | 1575 |
| `vr_ragdoll_army_joint_friction` | `-1` | yes | D | gameplay | Ragdolls - Grunt | 1 | 0 |  | 1576 |
| `vr_ragdoll_army_joint_stiffness` | `-1` | yes | D | gameplay | Ragdolls - Grunt | 1 | 0 |  | 1577 |
| `vr_ragdoll_army_limits` | `-1` | yes | D | gameplay | Ragdolls - Grunt | 1 | 0 |  | 1578 |
| `vr_ragdoll_army_damping` | `-1` | yes | D | gameplay | Ragdolls - Grunt | 1 | 0 |  | 1579 |
| `vr_ragdoll_army_blast` | `-1` | yes | D | gameplay | Ragdolls - Grunt | 1 | 0 |  | 1580 |
| `vr_ragdoll_army_inherit` | `-1` | yes | D | gameplay | Ragdolls - Grunt | 1 | 0 |  | 1581 |
| `vr_ragdoll_knight_start` | `-1` | yes | D | gameplay | Ragdolls - Knight | 1 | 0 |  | 1582 |
| `vr_ragdoll_knight_mass` | `90` | yes | D | gameplay | Ragdolls - Knight | 1 | 0 |  | 1583 |
| `vr_ragdoll_knight_friction` | `-1` | yes | D | gameplay | Ragdolls - Knight | 1 | 0 |  | 1584 |
| `vr_ragdoll_knight_joint_friction` | `-1` | yes | D | gameplay | Ragdolls - Knight | 1 | 0 |  | 1585 |
| `vr_ragdoll_knight_joint_stiffness` | `-1` | yes | D | gameplay | Ragdolls - Knight | 1 | 0 |  | 1586 |
| `vr_ragdoll_knight_limits` | `-1` | yes | D | gameplay | Ragdolls - Knight | 1 | 0 |  | 1587 |
| `vr_ragdoll_knight_damping` | `-1` | yes | D | gameplay | Ragdolls - Knight | 1 | 0 |  | 1588 |
| `vr_ragdoll_knight_blast` | `-1` | yes | D | gameplay | Ragdolls - Knight | 1 | 0 |  | 1589 |
| `vr_ragdoll_knight_inherit` | `-1` | yes | D | gameplay | Ragdolls - Knight | 1 | 0 |  | 1590 |
| `vr_ragdoll_ogre_start` | `-1` | yes | D | gameplay | Ragdolls - Ogre | 2 | 0 |  | 1591 |
| `vr_ragdoll_ogre_mass` | `200` | yes | D | gameplay | Ragdolls - Ogre | 2 | 0 |  | 1592 |
| `vr_ragdoll_ogre_friction` | `-1` | yes | D | gameplay | Ragdolls - Ogre | 2 | 0 |  | 1593 |
| `vr_ragdoll_ogre_joint_friction` | `-1` | yes | D | gameplay | Ragdolls - Ogre | 2 | 0 |  | 1594 |
| `vr_ragdoll_ogre_joint_stiffness` | `-1` | yes | D | gameplay | Ragdolls - Ogre | 2 | 0 |  | 1595 |
| `vr_ragdoll_ogre_limits` | `-1` | yes | D | gameplay | Ragdolls - Ogre | 2 | 0 |  | 1596 |
| `vr_ragdoll_ogre_damping` | `-1` | yes | D | gameplay | Ragdolls - Ogre | 2 | 0 |  | 1597 |
| `vr_ragdoll_ogre_blast` | `-1` | yes | D | gameplay | Ragdolls - Ogre | 2 | 0 |  | 1598 |
| `vr_ragdoll_ogre_inherit` | `-1` | yes | D | gameplay | Ragdolls - Ogre | 2 | 0 |  | 1599 |
| `vr_ragdoll_enforcer_start` | `-1` | yes | D | gameplay | Ragdolls - Enforcer | 1 | 0 |  | 1600 |
| `vr_ragdoll_enforcer_mass` | `100` | yes | D | gameplay | Ragdolls - Enforcer | 1 | 0 |  | 1601 |
| `vr_ragdoll_enforcer_friction` | `-1` | yes | D | gameplay | Ragdolls - Enforcer | 1 | 0 |  | 1602 |
| `vr_ragdoll_enforcer_joint_friction` | `-1` | yes | D | gameplay | Ragdolls - Enforcer | 1 | 0 |  | 1603 |
| `vr_ragdoll_enforcer_joint_stiffness` | `-1` | yes | D | gameplay | Ragdolls - Enforcer | 1 | 0 |  | 1604 |
| `vr_ragdoll_enforcer_limits` | `-1` | yes | D | gameplay | Ragdolls - Enforcer | 1 | 0 |  | 1605 |
| `vr_ragdoll_enforcer_damping` | `-1` | yes | D | gameplay | Ragdolls - Enforcer | 1 | 0 |  | 1606 |
| `vr_ragdoll_enforcer_blast` | `-1` | yes | D | gameplay | Ragdolls - Enforcer | 1 | 0 |  | 1607 |
| `vr_ragdoll_enforcer_inherit` | `-1` | yes | D | gameplay | Ragdolls - Enforcer | 1 | 0 |  | 1608 |
| `vr_ragdoll_hknight_start` | `-1` | yes | D | gameplay | Ragdolls - Death Knight | 1 | 0 |  | 1609 |
| `vr_ragdoll_hknight_mass` | `130` | yes | D | gameplay | Ragdolls - Death Knight | 1 | 0 |  | 1610 |
| `vr_ragdoll_hknight_friction` | `-1` | yes | D | gameplay | Ragdolls - Death Knight | 1 | 0 |  | 1611 |
| `vr_ragdoll_hknight_joint_friction` | `-1` | yes | D | gameplay | Ragdolls - Death Knight | 1 | 0 |  | 1612 |
| `vr_ragdoll_hknight_joint_stiffness` | `-1` | yes | D | gameplay | Ragdolls - Death Knight | 1 | 0 |  | 1613 |
| `vr_ragdoll_hknight_limits` | `-1` | yes | D | gameplay | Ragdolls - Death Knight | 1 | 0 |  | 1614 |
| `vr_ragdoll_hknight_damping` | `-1` | yes | D | gameplay | Ragdolls - Death Knight | 1 | 0 |  | 1615 |
| `vr_ragdoll_hknight_blast` | `-1` | yes | D | gameplay | Ragdolls - Death Knight | 1 | 0 |  | 1616 |
| `vr_ragdoll_hknight_inherit` | `-1` | yes | D | gameplay | Ragdolls - Death Knight | 1 | 0 |  | 1617 |
| `vr_ragdoll_dog_start` | `-1` | yes | D | gameplay | Ragdolls - Rottweiler | 1 | 0 |  | 1618 |
| `vr_ragdoll_dog_mass` | `40` | yes | D | gameplay | Ragdolls - Rottweiler | 1 | 0 |  | 1619 |
| `vr_ragdoll_dog_friction` | `-1` | yes | D | gameplay | Ragdolls - Rottweiler | 1 | 0 |  | 1620 |
| `vr_ragdoll_dog_joint_friction` | `-1` | yes | D | gameplay | Ragdolls - Rottweiler | 1 | 0 |  | 1621 |
| `vr_ragdoll_dog_joint_stiffness` | `-1` | yes | D | gameplay | Ragdolls - Rottweiler | 1 | 0 |  | 1622 |
| `vr_ragdoll_dog_limits` | `-1` | yes | D | gameplay | Ragdolls - Rottweiler | 1 | 0 |  | 1623 |
| `vr_ragdoll_dog_damping` | `-1` | yes | D | gameplay | Ragdolls - Rottweiler | 1 | 0 |  | 1624 |
| `vr_ragdoll_dog_blast` | `-1` | yes | D | gameplay | Ragdolls - Rottweiler | 1 | 0 |  | 1625 |
| `vr_ragdoll_dog_inherit` | `-1` | yes | D | gameplay | Ragdolls - Rottweiler | 1 | 0 |  | 1626 |
| `vr_ragdoll_wizard_start` | `-1` | yes | D | gameplay | Ragdolls - Scrag | 1 | 0 |  | 1627 |
| `vr_ragdoll_wizard_mass` | `40` | yes | D | gameplay | Ragdolls - Scrag | 1 | 0 |  | 1628 |
| `vr_ragdoll_wizard_friction` | `-1` | yes | D | gameplay | Ragdolls - Scrag | 1 | 0 |  | 1629 |
| `vr_ragdoll_wizard_joint_friction` | `-1` | yes | D | gameplay | Ragdolls - Scrag | 1 | 0 |  | 1630 |
| `vr_ragdoll_wizard_joint_stiffness` | `-1` | yes | D | gameplay | Ragdolls - Scrag | 1 | 0 |  | 1631 |
| `vr_ragdoll_wizard_limits` | `-1` | yes | D | gameplay | Ragdolls - Scrag | 1 | 0 |  | 1632 |
| `vr_ragdoll_wizard_damping` | `-1` | yes | D | gameplay | Ragdolls - Scrag | 1 | 0 |  | 1633 |
| `vr_ragdoll_wizard_blast` | `-1` | yes | D | gameplay | Ragdolls - Scrag | 1 | 0 |  | 1634 |
| `vr_ragdoll_wizard_inherit` | `-1` | yes | D | gameplay | Ragdolls - Scrag | 1 | 0 |  | 1635 |
| `vr_parry` | `1` | yes | C | gameplay | Parry and Bash | 3 | 0 |  | 1636 |
| `vr_parry_reduction` | `0.75` | yes | C | gameplay | Parry and Bash | 2 | 0 |  | 1637 |
| `vr_parry_drop_chance` | `0.5` | yes | C | gameplay | Parry and Bash | 1 | 0 | vr_defaults.cfg | 1638 |
| `vr_parry_angle` | `40` | yes | C | gameplay | Parry and Bash | 1 | 0 |  | 1639 |
| `vr_parry_muzzle_angle` | `45` | yes | C? | gameplay | — | 1 | 0 |  | 1640 |
| `vr_parry_reach` | `1.5` | yes | C | gameplay | Parry and Bash | 1 | 0 |  | 1641 |
| `vr_parry_unarmed` | `1` | yes | C | gameplay | Parry and Bash | 1 | 0 |  | 1642 |
| `vr_parry_unarmed_reduction` | `0.5` | yes | C | gameplay | Parry and Bash | 2 | 0 |  | 1643 |
| `vr_bash` | `1` | yes | C | gameplay | Parry and Bash | 3 | 0 |  | 1644 |
| `vr_bash_speed` | `2` | yes | C | gameplay | Parry and Bash | 4 | 0 |  | 1645 |
| `vr_shove_speed` | `2.4` | yes | C | gameplay | Parry and Bash | 2 | 0 |  | 1646 |
| `vr_bash_damage` | `8` | yes | C | gameplay | Parry and Bash | 1 | 0 |  | 1647 |
| `vr_bash_push` | `1` | yes | C | gameplay | Parry and Bash | 1 | 0 |  | 1648 |
| `vr_bash_sound` | `1` | yes | C | audio | Parry and Bash | 2 | 0 | vr_defaults.cfg | 1649 |
| `vr_parry_cooldown` | `0.4` | yes | C | gameplay | Parry and Bash | 1 | 0 |  | 1650 |
| `vr_parry_cooldown_attack` | `1` | yes | C | gameplay | Parry and Bash | 1 | 0 |  | 1651 |
| `vr_bash_deflect_radius` | `24` | yes | C | gameplay | Batting and Catching | 1 | 0 |  | 1653 |
| `vr_bash_deflect_window` | `0.3` | yes | C | graphics | Batting and Catching | 2 | 0 |  | 1654 |
| `vr_parry_wobble` | `1` | yes | C | gameplay | Parry and Bash | 1 | 0 | vr_defaults.cfg | 1655 |
| `vr_parry_stamina` | `0` | yes | C | gameplay | Stamina | 4 | 0 | vr_defaults.cfg | 1656 |
| `vr_parry_stamina_max` | `100` | yes | C | gameplay | Stamina | 4 | 0 |  | 1657 |
| `vr_parry_stamina_cost` | `30` | yes | C | gameplay | Stamina | 2 | 0 |  | 1658 |
| `vr_parry_stamina_cost_2h` | `12` | yes | C | gameplay | Stamina | 1 | 0 | vr_defaults.cfg | 1659 |
| `vr_parry_stamina_delay` | `2` | yes | C | gameplay | Stamina | 2 | 0 |  | 1660 |
| `vr_parry_stamina_regen` | `25` | yes | C | gameplay | Stamina | 1 | 0 |  | 1661 |
| `vr_parry_stamina_warn` | `1` | yes | C | gameplay | Stamina | 3 | 0 |  | 1662 |
| `vr_strike_stamina` | `1` | yes | C | gameplay | Stamina | 3 | 0 |  | 1664 |
| `vr_strike_stamina_punch` | `4` | yes | C | gameplay | Stamina | 1 | 0 |  | 1665 |
| `vr_strike_stamina_cost` | `8` | yes | C | gameplay | Stamina | 2 | 0 |  | 1666 |
| `vr_strike_stamina_cost_2h` | `6` | yes | C | gameplay | Stamina | 1 | 0 |  | 1667 |
| `vr_shove_stamina` | `1` | yes | C | gameplay | Stamina | 3 | 0 |  | 1668 |
| `vr_shove_stamina_cost` | `15` | yes | C | gameplay | Stamina | 2 | 0 |  | 1669 |
| `vr_shove_stamina_cost_2h` | `20` | yes | C | gameplay | Stamina | 1 | 0 |  | 1670 |
| `vr_stamina_exhausted_damage` | `0.5` | yes | C | gameplay | Stamina | 2 | 0 |  | 1671 |
| `vr_stamina_exhausted_push` | `0.5` | yes | C | gameplay | Stamina | 1 | 0 |  | 1672 |
| `vr_counter` | `1` | yes | C | gameplay | Parry and Bash | 3 | 0 |  | 1673 |
| `vr_counter_window` | `1.5` | yes | C | graphics | Parry and Bash | 1 | 0 | vr_defaults.cfg | 1674 |
| `vr_counter_damage` | `1.5` | yes | C | gameplay | Parry and Bash | 1 | 0 | vr_defaults.cfg | 1675 |
| `vr_counter_sound` | `1` | yes | C | audio | Parry and Bash | 2 | 0 | vr_defaults.cfg | 1676 |
| `vr_counter_glow` | `0` | yes | C | HUD,graphics | Parry and Bash | 2 | 0 | vr_defaults.cfg | 1677 |
| `vr_counter_haptic` | `1` | yes | C | controls,gameplay | Parry and Bash | 1 | 0 |  | 1678 |
| `vr_dummy_attacks` | `0` | no | D | gameplay | — | 10 | 0 |  | 1679 |
| `vr_dummy_attack_period` | `2.5` | yes | C | gameplay | Parry and Bash | 2 | 0 |  | 1680 |
| `vr_dummy_attack_jitter` | `0.4` | yes | C | gameplay | Parry and Bash | 2 | 0 |  | 1681 |
| `vr_dummy_attack_reach` | `80` | yes | C | gameplay | Parry and Bash | 1 | 0 |  | 1682 |
| `vr_dummy_attack_damage` | `10` | yes | C | gameplay | Parry and Bash | 2 | 0 |  | 1683 |
| `vr_dummy_attack_windup` | `0.6` | yes | C | gameplay | Parry and Bash | 1 | 0 |  | 1684 |
| `vr_dummy_gore` | `1` | yes | C | gameplay | Gore | 4 | 0 |  | 1685 |
| `vr_dummy_gib` | `0` | yes | C | gameplay | Gore | 4 | 0 |  | 1686 |
| `vr_dummy_gib_respawn` | `2` | yes | C | gameplay | Gore | 1 | 0 |  | 1687 |
| `vr_leg_holster_model_enabled` | `1` | yes | B | gameplay | Body and Display; Hip Holsters | 2 | 0 |  | 1688 |
| `vr_leg_holster_model_scale` | `0.5` | yes | C | gameplay | Hip Holsters | 3 | 0 |  | 1689 |
| `vr_leg_holster_model_x_offset` | `1` | yes | C | gameplay | Hip Holsters | 3 | 0 |  | 1690 |
| `vr_leg_holster_model_y_offset` | `1.25` | yes | C | gameplay | Hip Holsters | 3 | 0 |  | 1691 |
| `vr_leg_holster_model_z_offset` | `2.25` | yes | C | gameplay | Hip Holsters | 3 | 0 |  | 1692 |
| `vr_holster_mode` | `0` | yes | C | gameplay | Immersion | 4 | 0 |  | 1693 |
| `vr_weapon_throw_mode` | `0` | yes | C | gameplay | Immersion | 1 | 0 |  | 1694 |
| `vr_weapon_grip_mode` | `0` | yes | A | controls,gameplay | VR Settings | 6 | 0 |  | 1697 |
| `vr_weapon_throw_damage_mult` | `0.5` | yes | C | gameplay | Immersion; Weapon Damage | 2 | 0 |  | 1698 |
| `vr_weapon_throw_velocity_mult` | `1.0` | yes | B | gameplay | Immersion; Throwing and Physics; VR Settings | 1 | 0 |  | 1699 |
| `vr_weapon_cycle_mode` | `0` | yes | C | gameplay | Immersion | 2 | 0 |  | 1700 |
| `vr_melee_bloodlust` | `0` | yes | C | gameplay | Melee | 1 | 0 |  | 1701 |
| `vr_melee_bloodlust_mult` | `0.5` | yes | C | gameplay | Melee | 1 | 0 |  | 1702 |
| `vr_enemy_drops` | `0` | yes | C | gameplay | World | 1 | 0 |  | 1703 |
| `vr_enemy_drops_chance_mult` | `1.0` | yes | C | gameplay | World | 1 | 0 |  | 1704 |
| `vr_ammobox_drops` | `0` | yes | C | gameplay | World | 1 | 0 |  | 1705 |
| `vr_ammobox_drops_chance_mult` | `1.0` | yes | C | gameplay | World | 1 | 0 |  | 1706 |
| `vr_forcegrab_mode` | `1` | yes | B | gameplay | Force Grab; VR Settings | 1 | 0 |  | 1707 |
| `vr_forcegrab_distance` | `200` | yes | C | gameplay | Force Grab | 2 | 0 |  | 1708 |
| `vr_forcegrab_cone` | `15` | yes | C | gameplay | Force Grab | 1 | 0 |  | 1709 |
| `vr_forcegrab_flick_speed` | `1.0` | yes | C | gameplay | Force Grab | 1 | 0 |  | 1710 |
| `vr_forcegrab_flick_turn` | `250` | yes | C | gameplay | Force Grab | 1 | 0 |  | 1711 |
| `vr_forcegrab_time` | `0.35` | yes | C | gameplay | Force Grab | 2 | 0 |  | 1712 |
| `vr_forcegrab_speed` | `1000` | yes | C | gameplay | Force Grab | 2 | 0 |  | 1713 |
| `vr_forcegrab_arc` | `0.15` | yes | C | gameplay | Force Grab | 1 | 0 | vr_defaults.cfg | 1714 |
| `vr_forcegrab_catch_radius` | `14` | yes | C | gameplay | Force Grab | 2 | 0 |  | 1715 |
| `vr_forcegrab_catch_early` | `0.35` | yes | C | gameplay | Force Grab | 1 | 0 |  | 1716 |
| `vr_forcegrab_catch_late` | `0.15` | yes | C | gameplay | Force Grab | 1 | 0 |  | 1717 |
| `vr_forcegrab_catch_blend` | `0.15` | yes | C | gameplay | Force Grab | 1 | 0 |  | 1718 |
| `vr_forcegrab_miss_speed` | `0.2` | yes | C? | gameplay | — | 1 | 0 |  | 1719 |
| `vr_forcegrab_eligible_particles` | `1` | yes | C | graphics | Force Grab | 1 | 0 |  | 1720 |
| `vr_forcegrab_eligible_haptics` | `1` | yes | C | controls,gameplay | Force Grab | 1 | 0 |  | 1721 |
| `vr_grapple_rope` | `1` | yes | C | gameplay | Grappling Hook | 1 | 0 |  | 1722 |
| `vr_grapple_reel_speed` | `300` | yes | C | gameplay | Grappling Hook | 1 | 0 |  | 1723 |
| `vr_grapple_min_length` | `40` | yes | C | gameplay | Grappling Hook | 4 | 0 |  | 1724 |
| `vr_grapple_unreel_speed` | `300` | yes | C | gameplay | Grappling Hook | 2 | 0 |  | 1725 |
| `vr_grapple_unreel_airborne` | `1` | yes | C | gameplay | Grappling Hook | 1 | 0 |  | 1726 |
| `vr_grapple_prop_speed` | `300` | yes | C | gameplay | Grappling Hook | 2 | 0 |  | 1727 |
| `vr_grapple_prop_light` | `8` | yes | C | graphics | Grappling Hook | 1 | 0 |  | 1728 |
| `vr_grapple_prop_anchor` | `100` | yes | C | gameplay | Grappling Hook | 3 | 0 |  | 1729 |
| `vr_grapple_small_mass` | `120` | yes | C | gameplay | Grappling Hook | 1 | 0 |  | 1730 |
| `vr_grapple_huge_mass` | `400` | yes | C | gameplay | Grappling Hook | 1 | 0 |  | 1731 |
| `vr_grapple_small_speed` | `400` | yes | C | gameplay | Grappling Hook | 1 | 0 |  | 1732 |
| `vr_grapple_medium_speed` | `160` | yes | C | gameplay | Grappling Hook | 1 | 0 |  | 1733 |
| `vr_grapple_stagger` | `1` | yes | C | gameplay | Grappling Hook | 1 | 0 |  | 1734 |
| `vr_grapple_stamina` | `0` | yes | C | gameplay | Grappling Hook | 1 | 0 |  | 1735 |
| `vr_grapple_haptics` | `1` | yes | C | controls,gameplay | Grappling Hook | 1 | 0 |  | 1736 |
| `vr_grapple_quick_speed` | `1800` | yes | C | gameplay | Grappling Hook | 1 | 0 |  | 1737 |
| `vr_grapple_drop_grace` | `0.12` | yes | C | gameplay | Grappling Hook | 1 | 0 |  | 1738 |
| `vr_grapple_rope_sim` | `1` | yes | C | gameplay | Grappling Hook | 3 | 0 |  | 1739 |
| `vr_grapple_rope_spacing` | `4` | yes | C | gameplay | Grappling Hook | 1 | 0 |  | 1740 |
| `vr_grapple_rope_iterations` | `32` | yes | C | gameplay | Grappling Hook | 1 | 0 |  | 1741 |
| `vr_grapple_rope_radius` | `4` | yes | C | gameplay | Grappling Hook | 2 | 0 |  | 1742 |
| `vr_grapple_trigger_release` | `0` | yes | C | controls,gameplay | Grappling Hook | 2 | 0 |  | 1743 |
| `vr_grapple_max_length` | `1500` | yes | C | gameplay | Grappling Hook | 1 | 0 |  | 1744 |
| `vr_grapple_loose_slack` | `8` | yes | C | gameplay | Grappling Hook | 1 | 0 |  | 1745 |
| `vr_grapple_move_share` | `0.7` | yes | C | gameplay | Grappling Hook | 1 | 0 |  | 1746 |
| `vr_grapple_load_drag` | `1` | yes | C | gameplay | Grappling Hook | 1 | 0 |  | 1747 |
| `vr_grapple_hang_drag` | `1` | yes | C | gameplay | Grappling Hook | 1 | 0 |  | 1748 |
| `vr_grapple_load_max_speed` | `500` | yes | C | gameplay | Grappling Hook | 2 | 0 |  | 1749 |
| `vr_grapple_shootable` | `1` | yes | C | gameplay | Grappling Hook | 1 | 0 |  | 1750 |
| `vr_grapple_front_button` | `1` | yes | C | controls,gameplay | Grappling Hook | 2 | 0 |  | 1751 |
| `vr_grapple_front_button_x` | `6` | yes | C | controls,gameplay | Grappling Hook | 2 | 0 |  | 1752 |
| `vr_grapple_front_button_y` | `0` | yes | C | controls,gameplay | Grappling Hook | 2 | 0 |  | 1753 |
| `vr_grapple_front_button_z` | `0` | yes | C | controls,gameplay | Grappling Hook | 2 | 0 |  | 1754 |
| `vr_grapple_rope_depth` | `2` | yes | C | gameplay | Grappling Hook | 3 | 0 |  | 1755 |
| `vr_grapple_sag` | `1` | yes | C | gameplay | Grappling Hook | 2 | 0 |  | 1756 |
| `vr_grapple_rope_tail` | `6` | yes | C | gameplay | Grappling Hook | 1 | 0 |  | 1757 |
| `vr_grapple_hook_scale` | `0.6` | yes | C | gameplay | Grappling Hook | 1 | 0 |  | 1758 |
| `vr_grapple_tow_light` | `10` | yes | C | graphics | Grappling Hook | 1 | 0 |  | 1759 |
| `vr_grapple_tow_speed` | `320` | yes | C | gameplay | Grappling Hook | 1 | 0 |  | 1760 |
| `vr_grapple_debug` | `0` | no | D | dev | Debug - Logging | 11 | 0 |  | 1761 |
| `vr_grapple_unreel_slack` | `256` | yes | C | gameplay | Grappling Hook | 1 | 0 |  | 1762 |
| `vr_grapple_reel_home` | `5` | yes | C | gameplay | Grappling Hook | 1 | 0 |  | 1763 |
| `vr_grapple_quick_time` | `0.5` | yes | C | gameplay | Grappling Hook | 1 | 0 |  | 1764 |
| `vr_weapondrop_particles` | `1` | yes | C | graphics | Immersion | 1 | 0 |  | 1765 |
| `vr_2h_spread_reduction` | `0.5` | yes | C? | gameplay | — | 1 | 0 |  | 1766 |
| `vr_2h_throw_velocity_mult` | `1.0` | yes | D | gameplay | Throwing and Physics | 1 | 0 |  | 1767 |
| `vr_2h_handoff` | `1` | yes | B | gameplay | Aiming; VR Settings | 1 | 0 |  | 1768 |
| `vr_headbutt` | `1` | yes | C | gameplay | Melee | 3 | 0 |  | 1769 |
| `vr_headbutt_speed` | `1` | yes | C | gameplay | Melee | 2 | 0 | vr_defaults.cfg | 1770 |
| `vr_headbutt_damage` | `32` | yes | C | gameplay | Melee | 1 | 0 |  | 1771 |
| `vr_damage_to_enemies` | `1` | yes | C | gameplay | Damage and Knockback | 2 | 0 |  | 1772 |
| `vr_damage_to_player` | `1` | yes | C | gameplay,multiplayer | Damage and Knockback | 1 | 0 |  | 1773 |
| `vr_damage_self` | `1` | yes | C | gameplay | Damage and Knockback | 1 | 0 |  | 1774 |
| `vr_headshot_mult` | `1.5` | yes | C | gameplay | Damage and Knockback | 4 | 0 |  | 1775 |
| `vr_limbshot_mult` | `0.35` | yes | C | gameplay | Damage and Knockback | 4 | 0 | vr_defaults.cfg | 1776 |
| `vr_legshot_mult` | `0.6` | yes | C | gameplay | Damage and Knockback | 4 | 0 |  | 1777 |
| `vr_melee_push` | `0.5` | yes | C | gameplay | Damage and Knockback | 2 | 0 |  | 1778 |
| `vr_melee_push_player` | `0.6` | yes | C | gameplay,multiplayer | Damage and Knockback | 2 | 0 |  | 1779 |
| `vr_parry_push_enemy` | `1` | yes | C | gameplay | Damage and Knockback | 3 | 0 | vr_defaults.cfg | 1780 |
| `vr_parry_push_player` | `1` | yes | C | gameplay,multiplayer | Damage and Knockback | 3 | 0 | vr_defaults.cfg | 1781 |
| `vr_enemy_shove` | `1` | yes | C | gameplay | Enemy Shoves | 1 | 0 |  | 1782 |
| `vr_enemy_shove_range` | `50` | yes | C | gameplay | Enemy Shoves | 2 | 0 |  | 1783 |
| `vr_enemy_shove_delay` | `0.5` | yes | C | gameplay | Enemy Shoves | 1 | 0 |  | 1784 |
| `vr_enemy_shove_cooldown` | `1.5` | yes | C | gameplay | Enemy Shoves | 1 | 0 |  | 1785 |
| `vr_enemy_shove_damage` | `5` | yes | C | gameplay | Enemy Shoves | 1 | 0 |  | 1786 |
| `vr_enemy_shove_distance` | `128` | yes | C | gameplay | Enemy Shoves | 1 | 0 |  | 1787 |
| `vr_enemy_shove_parry_reduction` | `0.75` | yes | C | gameplay | Enemy Shoves | 1 | 0 |  | 1788 |
| `vr_enemy_shove_enforcer_damage` | `1.5` | yes | C | gameplay | Enemy Shoves | 1 | 0 |  | 1789 |
| `vr_enemy_shove_enforcer_distance` | `1.5` | yes | C | gameplay | Enemy Shoves | 1 | 0 |  | 1790 |
| `vr_push` | `0.5` | yes | C | gameplay | Damage and Knockback | 4 | 0 | vr_defaults.cfg | 1791 |
| `vr_hit_push` | `1` | yes | C | gameplay | Damage and Knockback | 3 | 0 |  | 1792 |
| `vr_kill_push` | `1` | yes | C | gameplay | Damage and Knockback | 3 | 0 |  | 1793 |
| `vr_enemy_liquid_damage` | `1` | yes | C | graphics | World | 2 | 0 |  | 1794 |
| `vr_explosion_rumble` | `1` | yes | C | gameplay | World | 1 | 0 |  | 1795 |
| `vr_heartbeat` | `1` | yes | C | gameplay | World | 1 | 0 |  | 1796 |
| `vr_deflect` | `1` | yes | C | gameplay | Batting and Catching | 5 | 0 |  | 1797 |
| `vr_deflect_radius` | `14` | yes | C | gameplay | Batting and Catching | 1 | 0 |  | 1798 |
| `vr_deflect_speed` | `0.6` | yes | C | gameplay | Batting and Catching | 1 | 0 |  | 1799 |
| `vr_deflect_window` | `0.2` | yes | C | graphics | Batting and Catching | 1 | 0 |  | 1800 |
| `vr_deflect_bounce` | `0.6` | yes | C | gameplay | Batting and Catching | 1 | 0 |  | 1801 |
| `vr_deflect_aim_assist` | `0.5` | yes | C | gameplay | Batting and Catching | 1 | 0 |  | 1802 |
| `vr_grenade_catch` | `2` | yes | C | gameplay | Batting and Catching | 1 | 0 |  | 1803 |
| `vr_grenade_held_fuse` | `2.5` | yes | C | gameplay | Batting and Catching | 2 | 0 |  | 1804 |
| `vr_grenade_fuse_regrab` | `0` | yes | C | gameplay | Batting and Catching | 1 | 0 |  | 1805 |
| `vr_grenade_catch_radius` | `15` | yes | C | gameplay | Batting and Catching | 1 | 0 |  | 1806 |
| `vr_grenade_catch_window` | `0.15` | yes | C | graphics | Batting and Catching | 1 | 0 |  | 1807 |
| `vr_grenade_return_full` | `1` | yes | C | gameplay | Batting and Catching | 1 | 0 |  | 1808 |
| `vr_grenade_shoot` | `1` | yes | C | gameplay | Batting and Catching | 1 | 0 |  | 1809 |
| `vr_grenade_shoot_pad` | `2` | yes | C | gameplay | Batting and Catching | 1 | 0 |  | 1810 |
| `vr_grenade_shoot_melee` | `1` | yes | C | gameplay | Batting and Catching | 2 | 0 |  | 1811 |
| `vr_grenade_melee_speed` | `20` | yes | C | gameplay | Batting and Catching | 1 | 0 |  | 1812 |
| `vr_grenade_melee_speed_weapon` | `34` | yes | C | gameplay | Batting and Catching | 1 | 0 |  | 1813 |
| `vr_grenade_melee_damage` | `10` | yes | C | gameplay | Batting and Catching | 1 | 0 |  | 1814 |
| `vr_handgrenade` | `1` | yes | C | gameplay | Batting and Catching | 2 | 0 |  | 1815 |
| `vr_handgrenade_fuse` | `2.5` | yes | C | gameplay | Batting and Catching | 1 | 0 |  | 1816 |
| `vr_handgrenade_arm` | `0` | yes | C | gameplay | Batting and Catching | 3 | 0 |  | 1817 |
| `vr_ogre_aim_height` | `1` | yes | C | gameplay | World | 1 | 0 |  | 1818 |
| `vr_activestartpaknameidx` | `0` | yes | C? | gameplay | — | 1 | 0 |  | 1819 |
| `vr_verbosebots` | `0` | no | D | dev,multiplayer | Debug - Logging | 5 | 0 |  | 1820 |
| `vr_finger_grip_bias` | `0.0` | yes | C | gameplay | Hand/Gun Calibration | 1 | 0 |  | 1821 |
| `vr_forcegrabbable_ammo_boxes` | `1` | yes | C? | gameplay | — | 1 | 0 |  | 1822 |
| `vr_forcegrabbable_health_boxes` | `1` | yes | C? | gameplay | — | 1 | 0 |  | 1823 |
| `vr_forcegrabbable_box_scale` | `0.25` | yes | C | gameplay | Force Grab | 1 | 0 | vr_defaults.cfg | 1824 |
| `vr_forcegrabbable_return_time_deathmatch` | `4` | yes | C? | gameplay,multiplayer | — | 1 | 0 |  | 1825 |
| `vr_forcegrabbable_return_time_singleplayer` | `0` | yes | C? | gameplay | — | 1 | 0 |  | 1826 |
| `vr_finger_auto_close_thumb` | `1` | yes | C | gameplay | Hand/Gun Calibration | 1 | 0 |  | 1827 |
| `vr_finger_blending_speed` | `50` | yes | C? | gameplay | — | 2 | 0 |  | 1828 |
| `vr_hand_fit` | `1` | yes | D | gameplay | Fingers and Collisions | 2 | 0 |  | 1830 |
| `vr_weapon_hotspot` | `1` | no | D | gameplay | Weapon Offsets - Two-Handed | 2 | 0 |  | 1831 |
| `vr_weapon_holster` | `1` | no | D | gameplay | Weapon Offsets - Holstered | 3 | 0 |  | 1832 |
| `vr_weapon_holster_preview` | `1` | yes | D | gameplay | Weapon Offsets - Holstered | 2 | 0 |  | 1833 |
| `vr_show_weapon_hotspots` | `0` | no | D | gameplay | Weapon Offsets - Two-Handed | 1 | 0 |  | 1834 |
| `vr_show_controller` | `0` | no | D | controls,gameplay | Hand/Gun Calibration; Weapon Offsets - Hand and Grip | 1 | 0 |  | 1835 |
| `vr_show_controller_laser` | `0` | no | D | controls,gameplay | Weapon Offsets - Hand and Grip | 1 | 0 |  | 1836 |
| `vr_dominant_eye` | `0` | yes | D | gameplay | Weapon Offsets - Muzzle and Sights | 1 | 0 |  | 1839 |
| `vr_show_sight_line` | `0` | no | D | gameplay | Weapon Offsets - Muzzle and Sights | 1 | 0 |  | 1840 |
| `vr_sight_align_captures` | `4` | yes | D | gameplay | Weapon Offsets - Muzzle and Sights | 1 | 0 |  | 1841 |
| `vr_show_controller_x` | `0` | yes | D | controls,gameplay | Weapon Offsets - Hand and Grip | 1 | 0 |  | 1846 |
| `vr_show_controller_y` | `0` | yes | D | controls,gameplay | Weapon Offsets - Hand and Grip | 1 | 0 |  | 1847 |
| `vr_show_controller_z` | `0` | yes | D | controls,gameplay | Weapon Offsets - Hand and Grip | 1 | 0 |  | 1848 |
| `vr_show_controller_pitch` | `0` | yes | D | controls,gameplay | Weapon Offsets - Hand and Grip | 1 | 0 |  | 1849 |
| `vr_show_controller_yaw` | `0` | yes | D | controls,gameplay | Weapon Offsets - Hand and Grip | 1 | 0 |  | 1850 |
| `vr_show_controller_roll` | `0` | yes | D | controls,gameplay | Weapon Offsets - Hand and Grip | 1 | 0 |  | 1851 |
| `vr_show_controller_off_own` | `0` | yes | D | controls,gameplay | Weapon Offsets - Hand and Grip | 3 | 0 |  | 1852 |
| `vr_show_controller_off_x` | `0` | yes | D | controls,gameplay | Weapon Offsets - Hand and Grip | 1 | 0 |  | 1853 |
| `vr_show_controller_off_y` | `0` | yes | D | controls,gameplay | Weapon Offsets - Hand and Grip | 1 | 0 |  | 1854 |
| `vr_show_controller_off_z` | `0` | yes | D | controls,gameplay | Weapon Offsets - Hand and Grip | 1 | 0 |  | 1855 |
| `vr_show_controller_off_pitch` | `0` | yes | D | controls,gameplay | Weapon Offsets - Hand and Grip | 1 | 0 |  | 1856 |
| `vr_show_controller_off_yaw` | `0` | yes | D | controls,gameplay | Weapon Offsets - Hand and Grip | 1 | 0 |  | 1857 |
| `vr_show_controller_off_roll` | `0` | yes | D | controls,gameplay | Weapon Offsets - Hand and Grip | 1 | 0 |  | 1858 |
| `vr_pose_weapon_hand` | `1` | no | D | gameplay | Weapon Offsets | 3 | 0 |  | 1859 |
| `vr_pose_solve` | `0` | no | D | gameplay | — | 1 | 0 |  | 1860 |
| `vr_pose_reset_offsets` | `0` | no | D | gameplay | Weapon Offsets | 1 | 0 |  | 1861 |
| `vr_hand_fit_palm` | `5` | yes | D | gameplay | Fingers and Collisions | 1 | 0 |  | 1862 |
| `vr_hand_fit_thumb_wide` | `1` | yes | D | gameplay | Fingers and Collisions | 1 | 0 |  | 1863 |
| `vr_hand_fit_thumb_outside` | `1` | yes | D | gameplay | Fingers and Collisions | 2 | 0 |  | 1864 |
| `vr_hand_fit_palm_weapon` | `3` | yes | D | gameplay | Fingers and Collisions | 1 | 0 |  | 1865 |
| `vr_hand_fit_palm_turn` | `20` | yes | D | gameplay | Fingers and Collisions | 1 | 0 |  | 1866 |
| `vr_hand_fit_overlap` | `0.3` | yes | D | gameplay | Fingers and Collisions | 1 | 0 |  | 1867 |
| `vr_hand_fit_overlap_hands` | `0.6` | yes | D | gameplay | Fingers and Collisions | 1 | 0 |  | 1868 |
| `vr_hand_walls` | `1` | yes | D | gameplay | Fingers and Collisions | 1 | 0 |  | 1869 |
| `vr_model_collide` | `2` | yes | D | gameplay | Fingers and Collisions | 3 | 0 |  | 1870 |
| `vr_model_collide_hands` | `1` | yes | D | gameplay | Fingers and Collisions | 2 | 0 |  | 1871 |
| `vr_model_collide_rest` | `1` | yes | C | gameplay | Carrying | 1 | 0 |  | 1872 |
| `vr_model_collide_max` | `20` | yes | D | gameplay | Fingers and Collisions | 1 | 0 |  | 1873 |
| `vr_debug_model_collide` | `0` | no | D | dev | Debug - Views | 3 | 0 |  | 1874 |
| `vr_gun_wall_slide` | `1` | yes | C | gameplay | Carrying | 1 | 0 |  | 1875 |
| `vr_gun_wall_max` | `40` | yes | C | gameplay | Carrying | 1 | 0 |  | 1876 |
| `vr_debug_gun_wall` | `0` | no | D | dev | Debug - Views | 2 | 0 |  | 1877 |
| `vr_debug_hand_offset` | `0` | no | D | dev | Debug - Views | 2 | 0 |  | 1878 |
| `vr_test_spawn` | `0` | no | D | dev | Debug - Tests | 1 | 0 |  | 1879 |
| `vr_test_spawn_dist` | `56` | no | D | dev | Debug - Tests | 1 | 0 |  | 1880 |
| `vr_test_pile_count` | `500` | no | D | dev | Debug - Tests | 1 | 0 |  | 1881 |
| `vr_test_pile_crates` | `80` | no | D | dev | Debug - Tests | 1 | 0 |  | 1882 |
| `vr_test_spawn_hold` | `0` | no | D | dev | Debug - Tests | 1 | 0 |  | 1883 |
| `vr_test_spawn_dead` | `0` | no | D | dev | Debug - Tests | 4 | 0 |  | 1884 |
| `vr_test_spawn_yaw` | `0` | no | D | dev | Debug - Tests | 1 | 0 |  | 1885 |
| `vr_test_spawn_tilt` | `0` | no | D | dev | Debug - Tests | 1 | 0 |  | 1886 |
| `vr_test_axe` | `0` | no | D | dev | Debug - Tests | 1 | 0 |  | 1887 |
| `vr_test_axe_roll` | `0` | no | D | gameplay | — | 1 | 0 |  | 1888 |
| `vr_test_axe_pitch` | `0` | no | D | gameplay | — | 1 | 0 |  | 1889 |
| `vr_test_axe_yaw` | `0` | no | D | gameplay | — | 1 | 0 |  | 1890 |
| `vr_test_axe_spin_f` | `0` | no | D | gameplay | — | 1 | 0 |  | 1891 |
| `vr_test_axe_spin_l` | `0` | no | D | gameplay | — | 1 | 0 |  | 1892 |
| `vr_test_axe_spin_u` | `0` | no | D | gameplay | — | 1 | 0 |  | 1893 |
| `vr_test_axe_side` | `0` | no | D | gameplay | — | 1 | 0 |  | 1894 |
| `vr_test_axe_loft` | `1` | no | D | gameplay | — | 1 | 0 |  | 1895 |
| `vr_test_axe_what` | `0` | no | D | dev | Debug - Tests | 1 | 0 |  | 1896 |
| `vr_test_axe_speed` | `10` | no | D | dev | Debug - Tests | 2 | 0 |  | 1897 |
| `vr_test_axe_damage` | `1` | no | D | dev | Debug - Tests | 1 | 0 |  | 1898 |
| `vr_test_axe_weapon` | `0` | no | D | dev | Debug - Tests | 2 | 0 |  | 1899 |
| `vr_test_axe_dist` | `0` | no | D | dev | Debug - Tests | 1 | 0 |  | 1900 |
| `vr_test_axe_at` | `0` | no | D | dev | Debug - Tests | 1 | 0 |  | 1901 |
| `vr_test_weaponinst` | `0` | no | D | gameplay | — | 0 | 1 |  | 1902 |
| `vr_test_weaponinst_slot` | `0` | no | D | dev | Debug - Tests | 0 | 1 |  | 1903 |
| `vr_test_held_destroy` | `0` | no | D | dev | Debug - Tests | 1 | 0 |  | 1904 |
| `vr_test_held_pick` | `-1` | no | D | dev | Debug - Tests | 4 | 0 |  | 1905 |
| `vr_test_fling_speed` | `12` | no | D | dev | Debug - Tests | 2 | 0 |  | 1906 |
| `vr_test_fling_at` | `0` | no | D | dev | Debug - Tests | 1 | 0 |  | 1907 |
| `vr_test_fling_away` | `0` | no | D | dev | Debug - Tests | 2 | 0 |  | 1908 |
| `vr_test_projectile` | `0` | no | D | dev | Debug - Tests | 2 | 0 |  | 1909 |
| `vr_test_grenade_shot` | `10` | no | D | dev | Debug - Tests | 1 | 0 |  | 1910 |
| `vr_test_grenade_dist` | `256` | no | D | dev | Debug - Tests | 1 | 0 |  | 1911 |
| `vr_test_grenade_height` | `0` | no | D | dev | Debug - Tests | 1 | 0 |  | 1912 |
| `vr_test_grenade_yours` | `1` | no | D | dev | Debug - Tests | 1 | 0 |  | 1913 |
| `vr_test_throw_up_body` | `0` | no | D | dev | Debug - Tests | 1 | 0 |  | 1914 |
| `vr_test_throw_up_speed` | `10` | no | D | dev | Debug - Tests | 2 | 1 |  | 1915 |
| `vr_test_projectile_side` | `0` | no | D | dev | Debug - Tests | 1 | 0 |  | 1916 |
| `vr_hand_collide` | `5` | yes | C | gameplay | Carrying | 3 | 0 |  | 1917 |
| `vr_hand_collide_fingers` | `1` | yes | C | gameplay | Carrying | 2 | 0 |  | 1918 |
| `vr_hand_collide_props_margin` | `1` | yes | C | gameplay | Carrying | 1 | 0 |  | 1920 |
| `vr_body_collide` | `1` | yes | C | gameplay | Body | 1 | 0 |  | 1921 |
| `vr_body_collide_pass` | `0.7` | yes | C | gameplay | Body | 1 | 0 |  | 1922 |
| `vr_body_collide_elbows` | `1` | yes | C | gameplay | Body | 2 | 0 |  | 1923 |
| `vr_debug_body_collide` | `0` | no | D | dev | Debug - Views | 2 | 0 |  | 1924 |
| `vr_hand_fit_blend` | `0.12` | yes | D | gameplay | Fingers and Collisions | 1 | 0 |  | 1925 |
| `vr_hand_fit_resolve` | `0.3` | yes | D | gameplay | Fingers and Collisions | 1 | 0 |  | 1926 |
| `vr_debug_torch_lights` | `0` | no | D | dev,graphics | Debug - Logging | 1 | 0 |  | 1927 |
| `vr_debug_grasp` | `0` | no | D | dev | Debug - Logging | 4 | 0 |  | 1928 |
| `vr_debug_grasp_trace` | `0` | no | D | dev | Debug - Logging | 2 | 0 |  | 1929 |
| `vr_jobs_threads` | `0` | no | D | dev | Debug - Profiling and Memory | 1 | 0 |  | 1930 |
| `vr_jobs_parallel` | `1` | no | D | dev | Debug - Profiling and Memory | 3 | 0 |  | 1931 |
| `vr_hand_rig` | `1` | yes | D | gameplay | Fingers and Collisions | 1 | 0 |  | 1932 |
| `vr_reload_mode` | `2` | yes | C | gameplay | Immersion | 5 | 0 |  | 1933 |
| `vr_show_weapon_text` | `1` | yes | D | HUD | Screens | 2 | 0 |  | 1934 |
| `vr_weapon_screen` | `1` | yes | C | HUD | Screens | 3 | 0 |  | 1935 |
| `vr_weapon_screen_padding` | `1` | yes | C | HUD | Screens | 1 | 0 |  | 1936 |
| `vr_weapon_screen_crt` | `1` | yes | C | HUD | Screens | 1 | 0 |  | 1937 |
| `vr_weapon_screen_idle` | `1` | yes | C | HUD | Screens | 1 | 0 |  | 1938 |
| `vr_weapon_morph_time` | `0.4` | yes | C | graphics | Graphics - Models and Effects | 1 | 0 |  | 1939 |
| `vr_weapon_draw_blend` | `0.3` | yes | C | gameplay | Immersion | 1 | 0 |  | 1940 |
| `vr_weapon_holster_blend` | `0.3` | yes | C | gameplay | Immersion | 1 | 0 |  | 1941 |
| `vr_debug_draw_blend` | `0` | no | D | dev | Debug - Logging | 2 | 0 |  | 1942 |
| `vr_worldtext_crt` | `1` | yes | C | HUD | Screens | 1 | 0 |  | 1943 |
| `vr_worldtext_hue` | `40` | yes | C | HUD | Screens | 3 | 0 | vr_defaults.cfg | 1944 |
| `vr_disablehaptics` | `0` | yes | B | controls,gameplay | Immersion; VR Settings | 14 | 0 |  | 1945 |
| `vr_spinreload_pitch_speed` | `1100` | yes | C? | gameplay | — | 1 | 0 |  | 1946 |
| `vr_spinreload_x_angular_threshold` | `6.5` | yes | C? | gameplay | — | 1 | 0 |  | 1947 |
| `vr_player_stepsize` | `18.0` | yes | C? | gameplay,multiplayer | — | 1 | 0 |  | 1948 |
| `vr_fingers_and_base_x` | `1.925` | yes | C? | gameplay | — | 1 | 0 |  | 1950 |
| `vr_fingers_and_base_y` | `-2.825` | yes | C? | gameplay | — | 1 | 0 |  | 1951 |
| `vr_fingers_and_base_z` | `-2.075` | yes | C? | gameplay | — | 1 | 0 |  | 1952 |
| `vr_fingers_and_base_offhand_x` | `0.0` | yes | C? | gameplay | — | 1 | 0 |  | 1954 |
| `vr_fingers_and_base_offhand_y` | `0.0` | yes | C? | gameplay | — | 1 | 0 |  | 1955 |
| `vr_fingers_and_base_offhand_z` | `0.0` | yes | C? | gameplay | — | 1 | 0 |  | 1956 |
| `vr_fingers_x` | `-5.05` | yes | C? | gameplay | — | 1 | 0 |  | 1958 |
| `vr_fingers_y` | `-0.1` | yes | C? | gameplay | — | 1 | 0 |  | 1959 |
| `vr_fingers_z` | `-0.1875` | yes | C? | gameplay | — | 1 | 0 |  | 1960 |
| `vr_finger_thumb_x` | `-0.3625` | yes | C? | gameplay | — | 1 | 0 |  | 1962 |
| `vr_finger_thumb_y` | `3.2625` | yes | C? | gameplay | — | 1 | 0 |  | 1963 |
| `vr_finger_thumb_z` | `-1.9375` | yes | C? | gameplay | — | 1 | 0 |  | 1964 |
| `vr_finger_index_x` | `-0.325` | yes | C? | gameplay | — | 1 | 0 |  | 1966 |
| `vr_finger_index_y` | `0.6125` | yes | C? | gameplay | — | 1 | 0 |  | 1967 |
| `vr_finger_index_z` | `-1.825` | yes | C? | gameplay | — | 1 | 0 |  | 1968 |
| `vr_finger_middle_x` | `-0.3625` | yes | C? | gameplay | — | 1 | 0 |  | 1970 |
| `vr_finger_middle_y` | `0.5125` | yes | C? | gameplay | — | 1 | 0 |  | 1971 |
| `vr_finger_middle_z` | `-0.3125` | yes | C? | gameplay | — | 1 | 0 |  | 1972 |
| `vr_finger_ring_x` | `-0.3625` | yes | C? | gameplay | — | 1 | 0 |  | 1974 |
| `vr_finger_ring_y` | `0.7` | yes | C? | gameplay | — | 1 | 0 |  | 1975 |
| `vr_finger_ring_z` | `0.65` | yes | C? | gameplay | — | 1 | 0 |  | 1976 |
| `vr_finger_pinky_x` | `-0.325` | yes | C? | gameplay | — | 1 | 0 |  | 1978 |
| `vr_finger_pinky_y` | `1.15` | yes | C? | gameplay | — | 1 | 0 |  | 1979 |
| `vr_finger_pinky_z` | `1.6375` | yes | C? | gameplay | — | 1 | 0 |  | 1980 |
| `vr_finger_base_x` | `0.0` | yes | C? | gameplay | — | 1 | 0 |  | 1982 |
| `vr_finger_base_y` | `0.0` | yes | C? | gameplay | — | 1 | 0 |  | 1983 |
| `vr_finger_base_z` | `0.0` | yes | C? | gameplay | — | 1 | 0 |  | 1984 |
