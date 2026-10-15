# Several VR menus open at once (research, 2026-10-15)

His note (BACKLOG "His notes, 2026-10-15"): "Several VR menus open at once, each with its own state, e.g. two side by
side for different tasks in parallel." Research only: nothing here is built yet.

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
  cannot be grabbed or moved. Not in the eyes, it goes to the runtime's own quad layer (`copyToRuntimePanel`), one only.
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
