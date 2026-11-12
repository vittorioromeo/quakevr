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
- **ragdoll_stagger: the third grunt shoved in a game falls** (ROUND25.md "Always-failing monster tests"): steady
  (balance 1.00) grunts after the first two fall at 0.36-0.39 s, 2 steps, pelvis 53 %; armed or plain, not time-bound.
- **ragdoll_climb**: the enforcer never grabs a lip (falls 184 units, armed or plain); grunt "too weak to rise" (one
  hand), ogre "lost its hold", hell knight "stuck at the lip" on the pull-up. Pre-existing since the phase-0 baseline.
  (agrunt 60, agarmor_5, limb_wear, stealth settled 2026-11-12; autopump, cellcord, animsmooth, xr_runtime and
  secret_hits too: ROUND25.md, "Always-failing monster tests" and "Always-failing tests, part b".)
- **secret_hits "door-punch"**: the fist swung through e1m1's shootable secret door (`*14`) swishes but never hits it (no
  melee event; the quad's trigger is struck): failing since at least 2026-10-30 (ROUND25.md, "Always-failing tests, part b").
- Stale, passing 3 of 3 (2026-11-12, seeded where the runner seeds): agrunt 47/63/66/67/71/72/76/86/209, ragdoll_grab,
  ragdoll_guard, rv_1, judo, nail_1, teleporters_chase, parry_pose, weapon_catch, dead_gear (removed from this list;
  `test_flaky.txt` still names some: take them off after more runs unseeded).
- **Embedded blades** (`vr_embed` on since 2026-11-08): recheck `vr_embed_test` 7, 13 and 21 (failing 2026-11-06);
  real heavy monsters, saves, level change and multiplayer unproven (tests 14-20 written).
- **`vrtutorial_playtest.py` room 8**: the shells' throw is fixed (ROUND25.md "testfix"); still open: the mock's main
  hand never grips the bench's shotgun (stops ~25 units short), so nothing loads; the `weapons` gate and later fail.
- **Tooling:** Tracy GPU zones lost after toggling GPU detail mid-run; some hidden-mock eye/window captures come back
  black; 15 test commands still registered in `vr_main.cpp`; `vr_physsound.cpp` keeps its own copy of the box test
  `R_SurfaceHoldsPoint` replaced for the light.

## 2. The 1.1 release (only on his word; his list is RELEASE_TODO.md)

- Review the installer's settings page (`Misc/installer/settings_page.txt`): rows, order, labels, help; whether "VR or
  flat screen" belongs there (2026-11-06).
- A VR RenderDoc capture to confirm the perf work on real hardware.
- The lower-end presets (`vr_graphics_preset`) checked on real hardware (round 20).
- `vr_flat_chest_down`: default 0.3, his 0 (plates hidden at 0.3 on the flat screen); decide at release prep.

## 3. Flat-screen (FLATPLAY.md)

- **Flat keys bindable from the menu** (2026-11-11): rebind rows for the keys flat play reads itself (`vr_flat_key_*`:
  leans, swap, grab, reload, prone, bullet time, drop, quick grenade, holster menu...), as Quake's own bindings have.
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

- **Lifts finished** (NAVMESH.md 10): on by default after a scene where the player stays put with a reach verdict, and
  the waiting-at-the-top case; trains where they stop.
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
- **Blades embedded in props** (2026-11-05): crates and barrels too; pulling either moves both (a Box3D joint, the pair
  not colliding); unify with blades in monsters and ragdolls where it fits.
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
- **Slipseer in the map library** (2026-11-11, permission granted): scrape slipseer.com's map pages (title, author,
  date, description, screenshots, download, required base game or mod). Polite: rate-limited, cached, a randomized
  human-looking User-Agent, no personal data and no mention of LLMs/AI/bots. Credit Slipseer and each map's author.

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
