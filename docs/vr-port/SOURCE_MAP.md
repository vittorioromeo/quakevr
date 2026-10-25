# Source map: which file holds what (the split files)

The biggest Quake/vr files were split by pure moves (ROUND22.md, "Source splits"): the same code, the same symbols,
in files by area. This map lists the parts; each part's first comment says the same.

## How a split file is put together

- **`<module>_internal.hpp`**: what the parts share of the old file's anonymous namespace, in a named namespace
  (`box3d::detail`, `foegrab::detail`, `view_detail`) brought in with a using-directive, so lookup works as before.
  Types and constants are defined there; variables too, `inline` (one object for every part, as before); functions
  are declared there and defined in the part that owns them. Included by the module's own parts alone.
- What one part alone uses stays in that part's anonymous namespace (in box3d's parts nested inside
  `box3d::detail`, so its lookups find the shared names first, as in the one file).
- `.inc` parts (vr_menu): a run of the file `#include`d where it was: the same translation unit, the same order.
- `*_diag.cpp`: a module's diagnostics, tests and debug commands; it registers its own commands.

## vr_box3d (the rigid bodies in Box3D; vr_box3d.hpp is the API)

| file | holds |
|---|---|
| vr_box3d.cpp | the whole's comment; the world's helpers (kinds, flags, densities, masses), step tasks, slots, destroyBody; settings, building and destroying the world; tool joints' sync |
| vr_box3d_internal.hpp | the shared types (World, Slot, RagdollBodies, categories...), state and declarations |
| vr_box3d_mesh.cpp | the world mesh (build, cache), brush models' hulls, guns' pieces and their disk cache, magazines, prop and actor hulls, shape definitions |
| vr_box3d_props.cpp | props: shapes, kinds, bodies made and followed (createBody, follow), fed and written back, syncEntities |
| vr_box3d_corpses.cpp | corpses lying still: when one is, its box or hulls, fed, written back, settled |
| vr_box3d_ragdoll.cpp | ragdolls made (deaths, active deaths, cut deaths), driven (shoves, swimming, climbing, active), shocked, written back |
| vr_box3d_knockdown.cpp | recoveries and getting up (pose fit, stand spot); the stagger, topple, shove, guard and get-up API |
| vr_box3d_ragparts.cpp | blasts on ragdolls, cuts, the players' grabs and holds, points, parts in a box, limbs, limb masses and heads |
| vr_box3d_hands.cpp | throws, gib contacts, born-inside, the hands' spheres, the reach bodies (hands and weapons) |
| vr_box3d_step.cpp | before the step (spin align, lifts, liquids, slides), after it (sounds, falls, impacts, touches), VR_PhysicsFrameEnd, VR_MonsterFell |
| vr_box3d_contacts.cpp | contact filters (self collision, shouldCollide, preSolve), portal copies, carried ragdolls and corpses, pushes and their limits |
| vr_box3d_api.cpp | loads, pins, tool joints, blasts, pushes, shots and rays, held boxes, ropes, level checks, counts and queries |
| vr_box3d_debugdraw.cpp | the debug drawing of bodies, hulls and ragdolls |
| vr_box3d_standing.cpp | players on props (standing, riding, pressing, nudges, unsticking, inside watch), a prop's shape for moves and shots, the SV_ hooks |
| vr_box3d_climb.cpp | climbing props: a held prop's climb shape and its pull |
| vr_box3d_diag.cpp | the physics commands (spawn, piles, blasts, lists, hashes, benches, tests) and their registration |

## vr_foegrab (holding enemies)

| file | holds |
|---|---|
| vr_foegrab.cpp | the holds, the two-hand moves (neck snap, lift, judo throw, leg grabs), carrying, the per-frame logic |
| vr_foegrab_internal.hpp | what the two share (the holds' and diagnostics' state, measures) |
| vr_foegrab_diag.cpp | the neck snap's and two-hand moves' diagnostics (measures, panels, drawings), the test commands, init() (their registration) |

## vr_view (the view entities: weapons, hands, holsters, body)

| file | holds |
|---|---|
| vr_view.cpp | the view's state, models, entities, finger frames, holsters, posing, magazines and pouches (magMounts: the anchor scripts read it here), body, gadget, buttons, VR_SetupViewEntities |
| vr_view_internal.hpp | the shared state and declarations (view_detail) |
| vr_view_weapon.cpp | the held weapon's set-up (setupWeapon) and its helpers |
| vr_view_hands.cpp | fingers and grasps, pushing against walls and props, the rigged hand, ground and free spots, limb holds, setupHand, drawHand |
| vr_view_diag.cpp | debug and test commands (model reload, grasp dump/bench/sweep, ragdoll hand probe, hotspot fit and checks, anchorWhere, posing checks, dumpView) and view::registerDiagCommands |

## Test and bench commands in `*_diag.cpp` (round 22)

Pure moves, same pattern: the module's test, bench, report and recorder commands (their code and registration) moved
out of the gameplay file; no command renamed, no behaviour changed. Each has a `vr_<module>_internal.hpp` with what
the two files share (namespace `<module>::detail`, brought in with a using-directive; variables `inline`; functions
declared there, defined where they were) and a `registerDiagCommands()` called from the module's own registration
(vr_main.cpp calls `hitmodel::`, `wounds::` and `qza::registerDiagCommands`).

| file | holds |
|---|---|
| vr_hull_diag.cpp | vr_hull_cachetest/_preloadtest/_stats/_keeptest/_bench/_walktest/_probe/_leafdebug/_approach/_hittest, vr_mhull_walktest, the walk drivers (walkTestFrame, monsterWalkFrame); vr_hull.cpp keeps vr_hull_warmcache, vr_mhull_reset, vr_crouch_status |
| vr_wounds_diag.cpp | vr_wounds_test/_dump/_info, vr_gore_hands_test/_info, vr_wrist_blood_test, vr_gore_wash_test, vr_gore_spatter_test |
| vr_jobs_diag.cpp | vr_jobs_info/_sites/_bench/_test |
| vr_weight_diag.cpp | vr_weight_test, vr_weight_table, vr_throw_table |
| vr_hitmodel_diag.cpp | vr_hitmodel_stats/_check/_bench, vr_hitzones_check |
| vr_brushes_diag.cpp | vr_brush_selftest, vr_brush_trace |
| vr_animsmooth_diag.cpp | vr_anim_smooth_pin/_info/_check/_trace |
| vr_zancle_diag.cpp | vr_zancle_math_test (no internal header: it shares nothing) |
| vr_ragdoll_diag.cpp | vr_drawn_motion_test (its recorder, recordMotion, and report) and vr_ragdoll_info; still registered in vr_box3d_diag.cpp |

The build systems take the new files by wildcard (vr.cmake and vr.mk glob `vr/*.cpp`; the VS project's quakevr.props
includes `Quake\vr\*.cpp`), so none lists them. Test and bench registrations still in vr_main.cpp belong to modules not in
this pass (modelcollide, selfcollide, modelmetadata, gore, decals, ...).

## vr_menu (the VR menu; pages as builder functions)

vr_menu.cpp keeps the menu's machinery (page table, presets, navigation, rows, drawing, commands). Its page builders
are in `.inc` files included where they were:

| file | pages |
|---|---|
| vr_menu_gameplay.inc | Gameplay: enemy shoves, melee, holding enemies, knockdowns, chainsaw, enemy weapons, damage, sound, blood feedback, parry, stamina, batting |
| vr_menu_body.inc | Body, Crouch, Arms, Body Calibration, the Flashlight pages |
| vr_menu_gore.inc | Small Gibs, Decapitation, Limb Gore, Gore |
| vr_menu_physics.inc | Throwing, Carrying, Burning (wall torches, AD), rocks and bricks, crates, gibs, corpse damage, the ragdoll pages |
| vr_menu_debug.inc | Debug: Views, Logging, Profiling, Reports, Tools |
| vr_menu_debug_tests.inc | Debug > Tests, spawn weapons, horde and MG3 tests (includes vr_menu_automation.inc) |
| vr_menu_settings.inc | VR Settings: the main page, its parts, the headset, Advanced, the hubs |
| vr_menu_wofs.inc | Weapon Offsets: the editor's actions and pages |

The older ones: vr_menu_props, _pages, _recording, _cheats, _toolgun, _stealth, _ragtests, _ad_monsters, _ad_guns,
_automation, _search, _console, _maps.
