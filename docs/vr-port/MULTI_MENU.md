# Several VR menus open at once (research, 2026-10-15)

His note (BACKLOG "His notes, 2026-10-15"): "Several VR menus open at once, each with its own state, e.g. two side by
side for different tasks in parallel." And: "if the same page is open on both menus, will it automatically sync the
values? It should." Scope (his clarification): only settings (cvars, config values) must always agree between panels;
per-page view state (cursor, scroll, expanded or collapsed, drafts, which rows a page shows) need not.

## Status (2026-10-15, worktree `bloodfx`, branch `agent/multimenu`)

- **Stage 1 done**: the panel can be grabbed (ROUND22.md, "Moving the menu panel"): a grip with the laser on its frame
  (a band just outside it, lit under the laser) takes it at once; on its face (`vr_menu_grab 2`, the default) once the
  hand moves `vr_menu_grab_deadzone` (4 cm) with the grip held, not after a stick push or with the trigger held (the
  sliders' fine modifier, as before). Held, it keeps its place in the laser's frame (kept upright); let go, it stays in
  the play space until the menus close. `vr_menu_panel [reset]`, `vr_mock_laser frame`,
  `Misc/quakevr/menu_grab_test.py <agent>`.
- **Stage 2 done**: `MenuInstance` in vr_menu.cpp (page, Back stack and its outside-menu flags, each page's cursor,
  scroll, left anchor and visits, slider/scrollbar drag, slider hold, drop-down, help box and paging, rows' left edge,
  scrollbar x, armed resets, body clone latch, noted place), read through `ui()`; `menu::instanceCount` (1),
  `currentInstance()`, `selectInstance()`. vr_panel.cpp's `MenuPanel` per instance (placement, its yaw when it
  appeared, its canvas handle: the 2D layer's canvas for now); the grab records the menu it holds. Proven unchanged:
  `menu_vr dump` byte-identical (182 pages, 4719 rows), `menu_coverage.py` PASS, the VR and flat navigation logs
  (keys, laser, mouse, drop-down steps, scroll) identical but the build hash.
- **Stage 3 done** (branch `agent/multimenu2`; ROUND22.md, "Several menu panels at once"): `instanceCount` 3; the
  corner's New Panel (headset only) opens another panel beside the first on the current page; Close Panel (the others'
  top button) or Back out of a panel's first page closes it; Back to game hides all (shown again as left when the menu opens, also after a map change; only Close Panel closes one; ROUND22.md, "Menu panels kept"). `selectInstance` swaps `m_state`
  and `m_mousex/y` with the instance (the current one is the focused panel); the first panel is the 2D layer's canvas
  (M_Draw draws it whichever is focused), the others draw into their own canvases after the 2D pass (VR_End2D, GPU scope
  "menu panels"), each through `items()` and `VR_Menu_Draw` (wrappers and presets refreshed per panel). The laser tests
  every panel; the nearest it is on is focused (keys, sticks, Back). Quake's menus opened from another panel open on the
  first (`tidyInstances`). One panel at a time (refused elsewhere, title note): Weapon Offsets and parts (Align Sights),
  Body Calibration, Checklist, Retro All Categories, Review Takes/Take, Search, Console, Map Library. `vr_menu_panels`,
  Debug rows, `Misc/quakevr/menu_panels_test.py`.
- **Stage 4 done**: both hands at once (each hand's laser moves the mouse of the panel it is on; one mouse button: a
  second hand's click lets go of the first's), each panel's place remembered for the session from the first panel
  (`vr_menu_panel_remember`, on; the first still opens in front of the head), the first kept placed while others are
  open; flat screen and the runtime's own panel show the focused panel's menu only. Not done: positions saved across
  sessions (`vr_menu_positions` keeps the first panel's page places only), the runtime's quad layer showing more than the
  first panel (headset only, as planned), two sliders dragged at once.
- **Kept single (stage 2 decision)**: the keyboard pages' state (Search's `search`, the Console page's `consolePage`,
  the Map Library's `mapPage`): their text input is one at a time (the console's line is Quake's own `key_lines`), and
  their structs mix view state with shared data (Search's index and recent list, the Map Library's caches and stats).
  Stage 3: allow each of them in one panel at a time, or split their view fields (query, results, scroll, hover/focus)
  into a per-instance struct then.

## Settings that must stay in sync between panels (for stage 3)

Every row reads its value from its cvar as it is drawn, and sliders, cycles, toggles and drop-downs write the cvar at
once (no edit buffer; `ui_live_preview` only pauses the game, menu.c), so two panels on the same page agree by
construction. Weapon Offsets/Weights and Held Object pages bind to the edited slot's own cvars (`weapons::cvar`,
`props::cvar`); which weapon/hand/prop is edited is shared (one context for both panels). What holds a copy of a
setting, or a value not yet applied:

- **Wrappers** (vr_cvars.inc `vr_menu_turning`, `vr_menu_move_towards`, `vr_menu_hands_*`, `vr_menu_bullettime`):
  unsaved copies shown as the settings they stand for are now. `syncWrappers()` runs in `items()` for any VR Settings
  page; stage 3 must keep fetching each panel's rows with `items()` before drawing it (true today), else a panel shows a
  stale copy. `lastSnapAngle` (Turning Mode's remembered Snap angle) is shared: fine.
- **Presets** (`vr_comfort_preset`, `vr_handedness`, `vr_graphics_preset`: "Custom" when the settings differ):
  `syncPresets()` runs once per `VR_Menu_Draw`; with two panels it must run before each (or once per frame before
  both).
- **Staged results applied by a button**: Align Sights to My Aim (`sightalign`, Apply/Undo; latch
  `weaponOffsetsSightVersion`), Body Calibration (`bodycal`, Apply/Undo; `bodycalVersion`), Retro Textures' "Apply to
  Checked Categories" (the All Categories settings copied into each category on the press). Their state is in the
  shared modules, so both panels show the same pending result; the apply is one action. No change needed, but they
  should not offer Apply twice at once (stage 3: one panel at a time, as the risks below say).
- **Shared text buffers** (`readouts`, `pageTexts`): info rows' texts are written into shared buffers as they are
  asked for; each panel's draw must use them before the other's draw rewrites them (true when panels draw one after
  the other).

Not settings, so not synced (his scope): cursors, scrolls, drop-downs, help paging, the Reset All armed label (baked
into VR Settings' built rows: both panels show it armed), Search's query, the Map Library's filters and picks.

## How the menu is held today

- **Quake's menus** (`menu.c`): one `m_state` (95 uses), `m_return_state`, `key_dest = key_menu`, each menu's cursor a C
  global. The VR pages are one more state (`m_vr`) drawn by `VR_Menu_Draw` and fed by `VR_Menu_Key/Char/Mousemove`.
- **VR pages** (`vr_menu.cpp`, `vr_menu_*.inc`): one global `page` (388 uses in vr_menu.cpp alone) and one Back stack
  `nav`. Per *page* (not per instance): `cursors[]`, `scrolls[]`, `leftAnchors[]`, `visits[]`, the built rows
  `menuPages`, and the "built for" latches (`mainPageSnap`, `weaponOffsets*`, `bodycal*`, `checklistGeneration`...).
  Interaction singletons: `sliderGrab`/`scrollGrab`, `sliderHold`, `dropDown`, `helpBox`/`helpPaging`, `rowsLeft`,
  `resetArmedPage`/`resetAllArmed`, `notedPlace` (vr_menu_positions), and the keyboard pages' `search`, `consolePage`,
  `mapPage`.
- **Panel** (`vr_panel.cpp`): one offscreen canvas holds the *whole* 2D layer (menus, console, HUD). While a menu is
  open it is one quad (`facingQuad`) `vr_menu_distance` ahead, its yaw latched when it opens (`panelYawOffset`); it
  cannot be grabbed or moved (stage 1 since: it can). Not in the eyes, it goes to the runtime's own quad layer (`copyToRuntimePanel`), one only.
- **Input** (`vr_menuui.cpp`): each hand's laser is intersected with that one quad (`panel::menuQuad`), the hit turned
  into window coordinates and sent as Quake's single mouse (`M_Mousemove`, `m_mousex/y`) with `Key_Event(K_MOUSE1)`;
  the sticks and buttons become menu keys. `pointingHand` picks which hand's laser is the mouse. The corner toolbar,
  banner, status box and version label are drawn into the same canvas.
- **Flat screen**: the same code, drawn to the window, the real mouse.
- **Precedent**: the toolgun's panel (`vr_toolgun_panel.cpp`) is already a second, independent panel: its own 256x192
  target, its own quad over the gun, its own laser and trigger handling in the host frame, without `key_dest`.

## What must become per instance

A `MenuInstance { page, nav, cursor/scroll of its page, slider/scroll grab, sliderHold, dropDown, helpBox, helpPaging,
rowsLeft, reset armed, placement (origin, yaw, pitch, scale), canvas target }`, with a `current` instance the existing
code reads through (swap it before building, drawing, or feeding input), so most of the 388 `page` uses become
`cur().page` mechanically. Shared on purpose: the built rows and their latches (a page shown twice shows the same
rows), the settings themselves, Quake's own menus (single: only VR pages open in extra panels), and the keyboard pages
(Search, Console, Map Library: one instance at a time, they own the text input).

## Staged plan and cost (agent-days)

1. **Grab and place the panel** (0.5-1): grip on the panel's frame moves and turns it; its pose kept while open
   (optionally saved). Useful alone, and the basis of placing several.
2. **Instance refactor, one instance** (2-3): the state above into `MenuInstance`; no visible change; proven by
   `menu_vr dump` + `menu_coverage.py`, the menu tests in TESTING.md and the boards' `vr_menu_path_check`.
3. **Second panel** (2-3): a canvas per instance (menus only; the HUD stays on the main canvas), drawn by calling
   `VR_Menu_Draw` once per instance into its target; each quad placed side by side when opened ("Open in New Panel"
   on a link row or a corner button; a close box per panel); the laser tests every quad, the nearest hit owns the
   synthesized mouse and the trigger; Back, the sticks and keys go to the panel last pointed at; the menu button
   closes all. Debug > Tools: open a second panel, `menu_vr instances` dump.
4. **Both hands at once and polish** (1-2): each hand's laser drives its own panel (two cursors, two grabs), positions
   per instance in `vr_menu_positions`, flat screen shows only the focused one (or tiles them), tests.

Total about 6-9 agent-days; a cheaper variant is a **split panel** (two pages side by side on one wide canvas: stage 2
plus a two-column layout instead of stage 3), about 4-5.

## Risks

- The refactor touches nearly every function of vr_menu.cpp: it conflicts with any concurrent menu work; do it alone.
- Stateful pages assume one viewer: Weapon Offsets (the weapon/hotspot edited), Body Calibration and VR Calibration
  (`setup::waitsOnMenu`), Motion Review, Checklist. Allow them in one panel only.
- `key_dest = key_menu` and the single-player pause stay global: with any panel open the game is in "menu" (Live
  Preview as today); walking while a panel is open would need the toolgun panel's model instead.
- GPU/VRAM: each canvas at `vr_menu_resolution` 1.5 is a few MB plus its draw; small, but measure with `vr_profile`.
- The runtime's quad layer fallback shows one panel only; extra panels only in the eyes.
- Quake's menus (Options, Load/Save, Multiplayer) cannot be duplicated without rewriting menu.c; only VR pages can.
