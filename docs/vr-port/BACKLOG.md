# Backlog: agreed work for the next rounds

Work the author approved or asked about, not yet done, one or two lines each, with the doc that holds the detail.
Remove an item when it ships (git history keeps it); keep new ones as short. Last cleanup 2026-11-12 (ROUND25.md).
Staying here by his decision (2026-11-05): pickup thinks, heart gib and berserk pack, TAA/DLSS.

## 1. Open bugs and failing tests

- **`vr_flatswap_test 4`** (AD's six blades through the hand swap, needs `-game ad`): never run; it checks nothing in vrtesthall, so it needs a spot on an AD map, then to pass with AD
  mounted (ROUND24.md, "The hand swap: melee weapons stay in sight").
- **visor_test "AD's two custom keys"**: needs AD in the test's base, or the check skipped without it.
- **Random run to run** (re-run 3 times each on 2026-11-12, ROUND25.md "Test triage B"; flatpose and flatswing seeded
  and read earlier, ROUND25.md "testfix": flatpose's last version unrun): `vr_systems_test 15` (1 of 3 fails: the
  blast's body never knocks the torch off; the body's flight varies: UNKNOWN), ragdoll_ragtest (1 of 3: "catch: NOT begun in 3.0 s"), reload (1 of 3: "a hit: the
  old one knocked out, the new one not seated by the same touch").
- **ragdoll_stagger: what is left** (the lip fixed, ROUND25.md "realfix_stagger"): the grunt's beaten backpedal falls
  at 0.43 s after 2 steps instead of giving way after 3 (F, not G; on level ground, every trial); in one combined
  batched run (lip rates steady backpedal) the 8th steady grunt (balance 0.52) and monster 2's 8th fell, and the lip
  case got 5/8, while lip alone gets 8/8 (batched or not): something between a batch's runs.
- **ragdoll_climb** (22 of 24 since agent/realfix_climb, 2026-11-12; the enforcer and the ogre fixed): pull grunt
  "slid back off the lip" (its pelvis sinks onto the lip's corner going in, stands up on the top anyway), pull hell
  knight "stuck at the lip" (one hand: the other's reach times out after the pull began; its chest stays under the lip).
  ROUND25.md, "Ragdoll climb: the enforcer's grab, the pull's chest, the ogre's swing".
  (agrunt 60, agarmor_5, limb_wear, stealth settled 2026-11-12; autopump, cellcord, animsmooth, xr_runtime and
  secret_hits too: ROUND25.md, "Always-failing monster tests" and "Always-failing tests, part b".)
- Stale, passing 3 of 3 (2026-11-12, seeded where the runner seeds): agrunt 47/63/66/67/71/72/76/86/209, ragdoll_grab,
  ragdoll_guard, rv_1, judo, nail_1, teleporters_chase, parry_pose, weapon_catch, dead_gear (removed from this list;
  `test_flaky.txt` still names some: take them off after more runs unseeded).
- **Embedded blades** (`vr_embed` on since 2026-11-08): recheck `vr_embed_test` 7, 13 and 21 (failing 2026-11-06);
  real heavy monsters, saves, level change and multiplayer unproven (tests 14-20 written).
- **Embedded blades, the handle hold** (2026-11-12, ROUND25.md "embedgrip"): his VR check of the handle staying in the
  hand; a stab's own gap along the blade now moves the body to the hand (keep it as slack if that reads wrong); the
  off hand's pose on a handle isn't probed; `vr_embed_test` 7 had 2 more failures in one run with the hold on.
- **`vrtutorial_playtest.py` room 8**: the shells' throw is fixed (ROUND25.md "testfix"); still open: the mock's main
  hand never grips the bench's shotgun (stops ~25 units short), so nothing loads; the `weapons` gate and later fail.
- **Tooling:** Tracy GPU zones lost after toggling GPU detail mid-run; some hidden-mock eye/window captures come back
  black; 15 test commands still registered in `vr_main.cpp`; `vr_physsound.cpp` keeps its own copy of the box test
  `R_SurfaceHoldsPoint` replaced for the light.
- **Baseline 2026-11-12, still failing alone** (ROUND25.md "basefix1112": first-bad merges by bisect, alone, seed 1):
  agrunt 16's hunt check (the drawn grip 2.7-2.8 units off the drawn hand hull, limit 2.5; its vertex checks fixed,
  ROUND25.md "agfails2"), stealth props_round (knight within 77
  units; since housing_nailpistol: likely a seeded run's butterfly), vrtrailer (no beheading: fails alone at 330f70409
  too). Older: agrunt 3, 91, 92; 213 needs AD's start (the baseline runs it in vrtesthall: a runner fix).

## 2. The 1.1 release (only on his word; his list is RELEASE_TODO.md)

- Review the installer's settings page (`Misc/installer/settings_page.txt`): rows, order, labels, help; whether "VR or
  flat screen" belongs there (2026-11-06).
- A VR RenderDoc capture to confirm the perf work on real hardware.
- The lower-end presets (`vr_graphics_preset`) checked on real hardware (round 20).
- `vr_flat_chest_down`: default 0.3, his 0 (plates hidden at 0.3 on the flat screen); decide at release prep.

## 3. Flat-screen (FLATPLAY.md)

- **Procedural reloads, on by default** (2026-11-05): e.g. the shotgun rolls toward the off hand, which shuttles between
  pouch and port until full or interrupted; recorded motions where they fit.
- **Recorded motions for flat melee and reloads** (2026-11-05): phase 1 recorder shipped (ROUND24.md, "Animation
  recorder, phase 1"); phase 2 after he records.
- **Hotkeys for the remaining VR interactions** (2026-11-05): audit what is left (parry, ammo switch, throw, shove,
  climb...) and propose a key or leave it VR-only.
- **Left-handed mode** (2026-11-05): the mouse buttons' routing by hand (`flatplay::routeFire`) mirrored.
- **The window's graphics** (round 20): float scene, tone mapping, grades and bloom on the window; check each feature
  with VR off.

## 4. Monsters' movement: the navmesh (NAVMESH.md)

- **Teleporters** (NAVMESH.md 16): walkers path through teleporters (one-way far links, shut ones closed, paired gates
  walked through when the box fits; NAVMESH.md 17: lining up in front of a wall teleporter, ranged monsters walking
  through a gate between shots, the plan drawn through, the `vr_nav_teletest` metric). Left: a scene on a map whose
  teleporter is gated by a button; AD's own `trigger_teleport` states (STARTOFF, estate) beyond the targetname rule;
  `nav_tele` (a foe out of sight is hunted by the stealth AI's trail, not the mesh: give its trail a way through a
  teleporter he was seen going into); the debug view didn't show in a headless screenshot (not even the mesh).
- **Lifts** (NAVMESH.md 10, 16): on by default; the reach scene (`nav_lift`) and the waiting-at-the-top case
  (`nav_lift_down`: the lift is called up). Left: a scene for a train (every id train with stops is triggered first:
  e2m6's elevator, e3m4's).
- **Flyers and swimmers**: a 3D grid of open space for scrags and fish (NAVMESH.md 7).
- **Flanking and cover**: goal slots round the player, extra cost on polygons other monsters' paths use, cover use
  (NAVMESH.md 7, 11).
- **The tower dog fall**: `vr_arena_test 41`'s last capped trial (1 of 60): a dog leaves the south-west tower's top
  within 1.3 s and dies of the 256-unit fall; how it goes over the edge is not found (NAVMESH.md 15).

## 5. Survival Arena (ARENA_DIRECTOR.md)

- **Arena geometry that changes between waves** (his request, round 24): the layout altered between waves, the navmesh
  following it (movers are already flagged blocked: NAVMESH.md 5).

## 6. Armed humanoids (the key feature; ARMED_MONSTERS.md)

- **The enforcer's real backpack** (2026-11-02): today a lift zone baked in vrenforcer.mdl (ARMED_MONSTERS.md, "Open
  items moved from BACKLOG.md").
- **AD's humanoids armed, then their worn packs** (2026-11-02; same section).
- **Crouching and crawling** (2026-11-05): a design study. A shot leg cripples, both legs crawl while still fighting
  (unarmed: retreat); generic for every humanoid, IK and aim right in any stance; leaning at corners and ledges.
- **Gremlins armed** (2026-11-02): Armagon's gremlin already steals and fires the player's gun; check its rig first.

## 7. Combat and feel

- **Crossed-sword parry** (2026-10-18): two blades crossed in front of the body count as a parry. After the AD melee work.
- **Blades embedded in props** (2026-11-05): done 2026-11-12 (ROUND25.md "propblade": a joint at the handle, shared with
  the ragdolls' handle hold). Left: his VR check; a test of a re-grip off the handle; the crate carried by the other hand
  while its blade is held (the hand lets go at arm's reach, as before).
- **From Quake Combat+ v5.7** (2026-10-16; ideas only, nothing of theirs reusable): hit-confirm audio layering;
  wounded monsters bleeding a trail, big ones split and crawling.
- **Flies on corpses and gibs** (2026-10-02): flies and their particles on every body after a few seconds; Armagon's
  head flies are off (`vr_head_flies` 0).
- **Active ragdoll research** (2026-10-22): FLINCH_RETROSPECTIVE.md, "Open items moved from BACKLOG.md".

## 8. Weapons, content and art

- **Ammo type follows the loaded magazine** (2026-10-16): a lava-nail magazine fires lava nails.
- **AD enemies' weapons for the player, the Deflector's gun, and AD soldiers' gun models** (2026-10-15/23):
  ARCANE_DIMENSIONS.md, "Open items moved from BACKLOG.md".
- **Bloody shotguns' own look** (MG3 decision 4): bloody skins for `v_shot`/`v_shot2` while `MG3_BloodyBits()` is set,
  or MG3's models laid out as the Super Axe's (`QC/vr_mg3_weapons.qc`).
- **AD inventory** (2026-10-12): the custom keys' names on the gadget's AD page and the jump boots' power level.
- **Reliefs:** generate the expansions' missing ones as AD's; the AD relief authoring pilot (10-20 textures) unstarted.
- **Art:** stretched UVs on the grappling hook's claws and a few faces of the rocket launcher, plasma gun, lava super
  nailgun and multi rocket launcher (ROUND21.md; ask whether they still show); body skins' wrist texel density (needs
  his repaint at a higher resolution).

## 9. Refactors, right after 1.1

1. Formatting: clang-format (Zancle configs) on first-party code, `// clang-format off/on` round hand-aligned tables,
   then clang-tidy fixes; one commit in `.git-blame-ignore-revs`. Never Ironwail's code.
2. Ironwail extraction: about 1,600 lines of VR code out of Ironwail files into `Quake/vr/` (pure moves), the VR main
   menu out of `menu.c`, the VR heap macro remap; `IRONWAIL_DIFF.md` refreshed.
Each verified by an unchanged physics hash, melee eval and `vr_menu_path_check`.

## 10. Big features (each needs its own decision)

- **Dynamic spawns: pick ideas** (2026-11-09): go through DYNAMIC_SPAWNS.md with him (its idea list and "Systems ideas
  from the 2026-11-09 audit"); the prerequisites are built.
- **Multiplayer update** (after 1.1): co-op rules, NAT traversal, voice chat, our VR systems synchronised, from
  Quakespasm-VR (GPL): MULTIPLAYER.md, "Open items moved from BACKLOG.md".
- **Seated play** (2026-10-18): holster and pouch zones laid out for a seated body, picked from the seated calibration.
- **Easier modelling** (2026-11-05): weapons have source .blends with export on save (MODELS_IN_BLENDER.md); left: the
  format audit and the other models.
- **glTF (.glb) for the hand and body models** (after 1.1; 2026-11-03): a cgltf loader filling the MD5 loader's
  structure, MD5 as fallback (ask before downloading cgltf). Weapons and other .mdl models stay.
- **Pillar and pipe climbing** (2026-10-14): a hand braced each side, hand over hand. Research.
- **Rendering:** TAA tuning for the headset, then DLSS/DLAA and FSR 3.1 (TEMPORAL.md, section 12's questions);
  `.rtlights` support; distance-field AO only if capsule/box AO leaves gaps; a foveation check and a GL-call audit before
  any Vulkan decision (VULKAN_MIGRATION.md); QuakeRay (github.com/f1ames0ff/QuakeRay) unreviewed.
- **Platforms:** Linux (WSL build in progress), Steam Frame (with kblood), standalone Quest (OpenXR on Android,
  GLES/Vulkan, mobile budget), bHaptics (2026-10-11).

## 11. Performance and parked

- **Performance, only when a capture makes one the top cost:** decals (PERF_DECISIONS.md 15), parallax option B and
  depth write, hit-box traces, shadows in crowded light, teleporter views, model loading in parallel, kept level copies:
  PERF_DECISIONS.md, "Open items moved from BACKLOG.md". Pickup thinks: PICKUP_THINKS.md (low priority).
- **Stealth AI limits:** STEALTH.md, "Open items moved from BACKLOG.md".
- **Dawn of the Machine's co-op and dm1** (MG3.md, decision 6): later.
- **Old QuakeC notes** (TECHDEBT_2026-10-09.md 10): Rogue's teamplay variants don't work in VR (item handling
  commented out); `readytime` is one for every client (one co-op player's spawn holds the others).
