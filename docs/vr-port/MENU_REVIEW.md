# Menus and settings: review and proposal (research, 2026-10-03)

Research of 2026-10-03; **built on branch `vr-ironwail-menus` the same day** (see "Status" at the end). The author,
2026-10-03: "There is a *lot* of stuff that was added to the menus as part
of the development of the VR mod ... a new player would easily get overwhelmed. There probably also are redundant and
outdated options/CVars that can be safely culled. Please perform some research ... and provide your thought on how to
improve the situation while still leaving freedom to players/developers."

The full inventory (every page and row, every VR cvar with its default, saved or not, pages, read sites and tier) is in
`MENU_INVENTORY.md`. Its counts come from a static parse of the menu builders, so pages built in loops or branches are
approximate. `menu_vr dump` plus `Misc/quakevr/menu_coverage.py` give exact runtime counts. The tiers are a first,
heuristic pass: many rows on gameplay pages are really internal tuning, and need a row-by-row look before anything is
hidden.

## Where it stood (before the rework; see "Status" for what was built)

| What | Count |
|---|---|
| VR menu pages | 126 (6 are only links), up to 5 levels deep; 39 pages at depth 4-5 |
| Rows | ~2,550, of which ~1,740 settings and ~350 actions/commands (~240 on the Debug pages) |
| Pages over 30 rows | 28 (Debug - Tests 154, Debug - Tools 83, Throwing and Physics 78, Gore 71, Carrying 63...). ROUND21.md, "Menus reorganized", aimed at 30 at most; they have grown back |
| VR cvars (`QVR_CVAR`, `vr_cvars.inc`) | 1,681: 1,507 saved, 174 not; 1,523 in a menu, 158 console only (1,704 before the dead and retired ones were removed, 2026-10-03) |
| Per-slot cvars | 8,320 weapon (`vr_wofs_*`), 2,880 prop (`vr_prop_*`), ~295 retro texture |
| Interactive rows with help text | 86% |
| Rows by audience (page-based, rough) | common settings ~117, enthusiast tuning ~1,000, developer/tuning ~970 |
| Cvars by tier (heuristic) | first-run essentials 20, common 60, enthusiast 906, developer 596, saved but on no page 99 (the 23 retired: removed) |

### What a new player met (before the rework)

- **VR Settings opens with "Tuning"**: six links to Weapon Offsets, Weapon Weights, Held Object Offsets/Weights,
  Hand/Gun Calibration and Body Calibration (`vr_menu.cpp` `pageMain`), the author's tools, above Comfort.
- **No volume in VR.** Master and music volume are only in the desktop Options; the VR Sound page is spatial audio
  tuning (HRTF, occlusion rays, reverb update, frame size, limiter).
- **Personal settings buried:** Dominant Eye only on Weapon Offsets - Muzzle and Sights (depth 4); headset gamma and
  contrast on Advanced > Graphics, not Headset.
- **No handedness setting:** split over stick swap, the wrist gadget's arm and the flashlight's side
  (the old single `vr_lefthanded` is gone).
- **No comfort vignette** (the only vignette is bullet time's).
- **No first-run flow:** nothing sends a new player to the calibration room, though its 15 wall-button options
  (`vr_setup.cpp`) already cover most essentials.
- The Checklist playtest button is shown to every player on every page.
- No search, no "show advanced", no per-page reset (only Options > Reset All, and a few one-off resets).

## The proposal: tiers by audience, everything still reachable

1. **A menu detail level: Standard / Advanced / Developer** (a cvar such as `vr_menu_level`). Every row and page gets
   a tier: Standard (first-run essentials and common settings), Advanced (enthusiast gameplay and visual tweaks),
   Developer (debug, tests, profiling, recording, and the content-tuning pages: weapon and prop offsets, hitboxes,
   ragdoll classes, gib internals). Pages with nothing visible are hidden. Default Standard; one row at the bottom of
   every page switches the level. Nothing is deleted; every cvar still works from the console. In Developer, the
   Tuning links return to the top of VR Settings, so the author's workflow is unchanged.
2. **A Standard landing page of ~15-20 rows**: comfort (turning, movement, teleport), handedness, height (with Set
   Height Now), seated or standing, HUD, crosshair, graphics preset, render scale, master and music volume, haptics,
   dominant eye, headset brightness. **A first-run flow**: on first launch, start in the calibration room; a "Run
   Setup Again" entry.
3. **Presets for groups of settings**, so most players never need the advanced pages: Comfort (Comfortable /
   Moderate / Full Freedom), a gameplay feel (Arcade / Default / Realistic: damage, stamina, weapon weight, gore), and
   the existing `vr_graphics_preset`. A setting changed by hand afterwards shows the preset as "Custom".
4. **One home per setting.** 70 cvars are on 2-3 pages, sometimes with other labels or ranges: `vr_snap_turn` is a
   "Turning" choice on one page and a 0-90 "Turn" slider on another; `vr_disablehaptics` is "Haptics On/Off" and
   "Disable Haptics"; `vr_menu_scale` has two ranges; the 8 grenade-pouch rows are on two pages. Keep one; the others
   become links ("→ Locomotion").
5. **Show what was changed.** A mark on settings that differ from their default, "Reset This Page", and a "Changed
   Settings" page listing them all (useful for enthusiasts and for bug reports).

## Culling and consolidating, cheapest first

- **Done (2026-10-03):** the dead and retired cvars below were removed, about 320 in all, with the migrations that
  used them; no compatibility with old configs kept (none was in a shipped cfg). `MENU_INVENTORY.md` section 5a has the
  list.
  - The 21 cvars nothing reads (`vr_physics_engine`, `vr_box3d_weapon_mass`, ten `vr_wpn_*_weight*`,
    `vr_weight_model`/`_props`, `vr_sword_drop`, `vr_carry_reach`, `vr_parry_sound_burst`, `vr_parry_stamina_show`,
    `vr_finger_grip_open`, which was still saved to every config, ...): removed.
  - The ~300 cvars registered only so old configs loaded silently (224 retired weapon-slot, 64 prop-slot, 12
    `vr_retro_body*`, `vr_lefthanded`, `vr_gadget_hand`): removed outright.
- **Now (low risk):**
  - Test-only cvars (`vr_smallgibs_test_*`, `vr_test_weaponinst*`): Developer.
  - **Bug:** `wait5` is not a command (only `wait` is registered), so the Debug ragdoll buttons ("A Grunt's Ragdoll
    Ahead", "A Knight's/An Ogre's Ragdoll There": `vr_menu.cpp` around lines 3665, 3863, 3866) set
    `vr_test_spawn_dead 0` in the same frame instead of five frames later.
- **Soon:**
  - The 99 cvars saved but on no page (throw internals, physics pushes, foveation radii...): constants, or not saved.
  - The 120 menu settings that aren't saved (mostly Debug toggles): check each should reset on restart.
  - Menu rows naming a missing cvar are dropped silently (`vr_menu.cpp` around line 202): warn instead, so a rename
    can't hide a row.
- **Consolidate:** one haptics strength with per-feature overrides under Advanced; the 10 effect hues that default to
  the player's hue become automatic; ragdoll per-class (85 cvars, 7 pages) and per-class monster hitboxes (13, a page
  marked "Prototype") become data tables.
- **Bigger, later:** the 11,200 per-slot weapon and prop cvars to data files the Developer pages edit (content, not
  settings: they swell the config and the cvar list).
- **Guardrails:** extend `menu_coverage.py` into a check: every row has a tier, every setting one home, pages within a
  row budget, no row pointing at a missing cvar. Generate SETTINGS.md from the rows' help text (SETTINGS.md covers 98
  of 1,681 cvars; README still says "Reset to defaults" and "about twenty more pages", and places Height on VR
  Settings).

## Suggested order (best return first)

1. Detail levels and the Standard landing page: the biggest effect on new players, moderate work, low risk.
2. The cheap cleanup: `wait5`, warnings for missing rows (the dead cvars: done).
3. The first-run flow and volume in VR.
4. One home per setting.
5. Presets.
6. Per-slot cvars to data files.

## Status (branch `vr-ironwail-menus`, 2026-10-03)

Built, after "Please proceed with your plan ... on a separate branch" (each step a commit, so it can be taken apart):

1. **Menu Detail** (`vr_menu_level`: Standard, Advanced, Developer): every page has a level (`pages[]`), rows can be
   `.advanced()` or `.developer()`; links and rows above the level are left out, empty headers too; a Menu Detail row
   ends every page. `menu_vr <n>` opens any page; `{menu:...}` paths resolve through every page and add "(Menu Detail:
   Advanced)" when needed. The corner's Advanced VR raises Standard to Advanced; its Checklist button is Developer
   only. A row naming a missing cvar is said once (developer 1). Headless dump: Standard 5 pages, Advanced 64,
   Developer 118 (all but the ones only links reach with a weapon in hand).
2. **VR Settings for every player:** Comfort and Handedness presets (`vr_comfort_preset`, `vr_handedness`: not saved,
   the choice shown follows the settings, Custom when none matches), Height and Set Height Now, Body Calibration,
   Dominant Eye, Volume and Music Volume, HUD, Crosshair, Headset Gamma, the Graphics Preset; the finer rows from
   Advanced, the Tuning links from Developer. Body and Display: the three sides (Sides).
3. **First start:** no saved config sets `vr_setup_pending`; VR Calibration starts the first time the headset tracks
   with no game running. VR Settings > Run VR Calibration Again.
4. **One home per setting:** 64 settings on two or three pages each have one home now, the others link to it. Kept on
   purpose: the Graphics Preset (VR Settings and Graphics), Dominant Eye (VR Settings and beside Align Sights).
   Nothing left the menus (dumps compared).
5. **Changes visible:** `*` by a changed setting, Reset This Page (twice within 3 s), Changed Settings (page 132).
6. **Cleanup:** the Debug ragdoll buttons' `wait5` (not a command) is five `wait`s; the calibration room's board
   pointed at Body and Display > Set Height Now: VR Settings now (the .map, its generator and the .bsp's entity lump);
   README and SETTINGS.md describe the new menus.

7. **Search** (the author's request, the same day): the corner's Search button (and Search Settings on VR Settings)
   opens a page with a text box and a QWERTY keyboard on the left and the results on the right, updated as you type,
   each with its pages in small letters; fuzzy and ranked (labels, then help, pages and cvar names; word starts,
   letters in order, one typo); picking one opens its page on it, Back returns to the results
   (`vr_menu_search.inc`; `vr_menu_search <text>` prints the ranking). First start fixed on the way: with VR on,
   `vr_startgame` goes to the calibration room instead of the hub (the hub was always running, so the frame check
   never fired).
8. **Fixes after the merge** (2026-10-04): a page is built anew each time it is shown and whenever cvars are registered
   (it was kept as first built, so a row could stay missing until Menu Detail was changed); Search matches cvars'
   names too, underscores kept ("vr_gib_health", "gib_health"; the keyboard's `-` is `_` now): below the labels unless
   the name is typed in full, the name shown with the result.
9. **Console** (the author's request, 2026-10-04): the corner's Console button (under Search) opens Quake's console as a
   page: its text above, the line typed and a keyboard (with `-+_";./,=*'`, Tab, Prev and Next, Run) at the bottom. The
   line is the console's own, edited through `Key_Console`/`Char_Console`, so history, Tab completion and its hint are
   the desktop console's (`vr_menu_console.inc`; page 134, `menu_vr 134`).
10. **Tips** (the author's request, 2026-10-04; `vr_tips.cpp`, VR Settings > Tips, page 135): a tip the first time you
   come near something usable and can see it (within `vr_tips_distance`, `vr_tips_view_angle`, in sight of the world:
   `vr_tips_line_of_sight`, for `vr_tips_delay` s). `vr_tips` 1: a CRT screen floating beside it as the map boards'
   (its own image through the screen shader, bezel, glow: `text3d::queueOverlayScreen`, over the scene), turned towards
   the eyes and level or square to the view (`vr_tips_facing`), with a cable of the same screen to it that starts under
   the screen (no seam) and is as thick on screen all along; 2: in the wrist gadget's
   hologram, waiting to be seen (chime and buzz) and then shown `vr_tips_time` s (`gadget::tip`); the panel when there
   is no hologram. Shown tips are kept in `vr_tips_seen`; `vr_tips_reset` (Show Tips Again) and `vr_tips_test [name]`
   (Show the Torch Tip Now, which also prints the nearest candidates' distance, angle and sight). One tip so far: wall
   torches.

Not built (the questions below): gameplay-feel presets, a comfort vignette, per-slot cvars to data files.

## Questions for the author

- Default level Standard, with Developer remembered on the author's machine?
- Gameplay-feel presets, or gameplay kept as one tuned design?
- Add a comfort vignette?
- Renaming and moving rows: acceptable, given the muscle memory of the current layout?
