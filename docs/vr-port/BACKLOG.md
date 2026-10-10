# Backlog: agreed work for the next rounds

Items the author approved or asked about, not yet started. Each has a note on where it came from.

## Next round (approved)

### Flies on corpses and gibs (the author, 2026-10-02: "add it to the to-do list")

Scourge of Armagon's head flies (one severed head in ten loops `misc/flys.wav`, player.qc HeadThink) were pointless
as they were, so they are off (`vr_head_flies` 0, Gore > Flies on Heads; NOTES.md vrfiringrange_2026-10-02_01-29-37).
To revisit and make interesting: flies on every corpse and gib (not only heads), arriving after it has lain a few
seconds, with fly particles buzzing round it as well as the sound.

### Bloody shotguns' own look (the author, MG3 decision 4, 2026-10-06: "Backlog TODO: distinct bloody textures/skins")

Dawn of the Machine's bloody shotgun and super shotgun (M3-14, `QC/vr_mg3_weapons.qc`) play as intended (0.28 s
refire; 28 pellets) but are drawn as Quake VR's own shotguns: MG3's `v_bloodshot.mdl`/`v_bloodshot2.mdl` are view
models with an arm, made for the flat view. To do: bloody skins for `progs/v_shot.mdl`/`v_shot2.mdl` (a second skin
picked while the bit is set: `MG3_BloodyBits()`), or MG3's models stripped and laid as the Super Axe's are
(`vr_monstermods.cpp superAxePoses`), with their own weapon settings slots.

### Repository chores

## Proposed, waiting on the author

- **Temporal AA, then DLSS/DLAA and FSR 3.1** (`docs/vr-port/TEMPORAL.md`; its stage 1, FSR 1/NIS and foveated
  VRS, shipped in round 20): stage 2a (motion vectors for both eyes) is safe to start any time; the DLSS helper waits
  on TEMPORAL.md's section 12 questions (private use or public, the helper's licence, FSR 3.1, TAA as default AA, 120
  vs 90 Hz with DLSS).
- **The graphics on the flat screen** (asked round 20): most features already draw on the desktop; to do: the
  float scene, tone mapping, grades and bloom for the window, and a check of each feature with VR off.
- **Lower-end graphics settings before a release** (the author, round 20: "maybe later on before release, you can
  prepare some lower-end graphics settings as well"): the shipped defaults (`quakevr/vr_defaults.cfg`) are the
  author's RTX 4090 settings; the presets need tuning for mid and low-end GPUs.
- **`.rtlights` support** (round 20 discussion): load DarkPlaces' hand-authored light lists where they exist to
  drive the shadowed map lights (better placed than the map's light entities).
- **Distance-field AO** (round 20 discussion): only if capsule/box AO leaves obvious gaps.
- **Vore shove** (the author, 2026-10-02: "I would like the vore to also have a shove attack when the player is
  close, later on"): extend the enemy shove (QC/vr_enemyshove.qc) to the vore (shalrath), with its own animation.
- **Flashlight optional; a brighter option** (the author, 2026-10-02): partly there. VR Settings has Flashlight
  (`vr_flashlight` 0: no belt torch, its zones and grabs off, `vr_flashlight.cpp` enabled()) and, since 2026-10-04/07,
  Ambient Light (`vr_ambient_light`, a floor of light) and Light Contrast. Left: check that nothing else assumes the
  torch with it off (gadget hints, tips, tutorial), and whether one "Visibility" control raising ambient light,
  exposure and tone mapping together is still wanted over Ambient Light alone.
- **Menus and settings for players, not only for tuning** (the author, 2026-10-03; `docs/vr-port/archive/MENU_REVIEW.md`):
  the rework (Menu Detail levels, VR Settings for every player, one home per setting, changes marked, Search) was
  merged 2026-10-03 (5830a23d2), and the comfort vignette came with VR Settings' rebuild (2026-10-07,
  `vr_comfort_vignette`). Left: gameplay-feel presets (or gameplay kept as one tuned design) and the per-slot cvars
  (`vr_wofs_*`, `vr_prop_*`) to data files.
- **Performance leads left open by the 2026-10-03/05 reports** (the reports were removed 2026-10-06; git history has
  them: `PHYSICS_PERFORMANCE_RESULTS_2026-10-03.md`, "Not done", and `PERFORMANCE_BENCHMARK_20261005.md`, items 4-5):
  - *Hit-box traces:* skip `touchNearby` for props too slow to hurt, after checking the monsters' QC touch functions
    (a leaping dog's or fiend's touch acts on what it meets).
  - *Floating props' equilibrium sleep:* a design decision (they would stop bobbing).
  - *Touch queries:* `FL_EASYHANDTOUCH` grows every prop's box for the hands; prop-to-prop queries could use the
    ungrown box.
  - *Shadows in crowded, brightly lit scenes* (32 overlapping lights: 1.59 ms GPU against 1.15 ms without shadows):
    bone reuse, caching for stable lights and objects (the dlights' world casters: PERF_DECISIONS.md, "Leads"),
    per-light update budgets, resolution by projected importance (`vr_lighting.cpp`; the casters are set up once a
    pass over the lights since BENCHMARKS.md's "Second follow-up").
  - *Teleporter views* (about 0.8 ms GPU and CPU for one doorway): the destination's resolution chosen by the
    aperture's projected size, tighter destination draw lists, per-frame entity preparation shared across views, the
    shadow atlas drawn once for the portal views (PERF_DECISIONS.md, item 2).
- **Low priority: review the pickup-thinks research** (the author, 2026-10-03: "save your research regarding the
  nextthink stuff in a document so that we can review it later"): `docs/vr-port/PICKUP_THINKS.md`. Idle pickups think
  every 0.02 s; options to think less without changing gameplay (idle interval with exact deadlines, no thinks while
  carried, the spin left to the engine), and where thinking resolution would be lost (engine-side knocks, unless the
  engine wakes the pickup). Small absolute gain (QuakeC is ~0.07 ms a frame): review, then decide.

## Later (the author, 2026-10-11, before 1.0.0: "Make sure everything is in the backlog for later")

### Performance

- **Optimise decals** (PERF_DECISIONS.md 15): in a rocket fight (`play_e1m1_lights`) ~900 marks are 1.34 ms of the
  world pass's 2.67 at 2048 per eye, about half (~2.4 ms at his 2782 eyes); each pixel walks its cell's bucket (up to
  64 marks, a whole 80-byte record each before its early-out). Reading a mark's normal and middle first was tried and
  reverted (no gain, 0.6-0.9% of the pixels changed). `vr_decal_max` lowered to 768 meanwhile (config version 118).
  Leads: fewer marks a bucket, a smaller record or a cheaper early-out, merging overlapping marks.

- **Setup prepares the monsters' guns** (PERF_DECISIONS.md 13): the first session still cuts each monster's dropped gun
  at its first death (grunt 35 ms, knights' swords 19, ogre's chainsaw 84); Setup's preparation run could make each once.

### Map and game loading (ROUND21.md, "Map and game loading", 2026-10-10: each has a drawback)

- **Shader program binaries cached on disk** (`GL_ARB_get_program_binary`, 200 ms of every start): driver bugs with it.
- **Box3D world mesh kept across map changes** (47-50 ms a return to the hub): needs a content key, not name and counts.
- **Normal maps made after the load** (most of a cold first visit's 0.3-0.8 s): flat shading for a moment, then the maps.
- **A thinner memory log GL object count** (`vr_memstats_log`: 13-15 ms of every load): its leak check sees fewer loads.
- **Model loading in parallel** (the first map's 430 ms of alias models): the loaders share the hunk, cache and GL.

### Art (ROUND21.md, "For the author": Blender)

- **Stretched UVs** (re-map and repaint as reuv_shot2.py did the double shotgun's): the lightning and plasma guns' side
  panels, super nailgun, rocket launcher, the grappling hook's front cap, Mjolnir.
- **Normal maps to rebake** (`bake_normals.py`): `v_shot.mdl` and its pump parts, `vr_shell.mdl`, the six magazine
  wells; the crowbar's (400 pixels with z < 0: a clamp in the baker).
- **Body skins' texel density at the wrist**: about a sixth of the hands' (256 x 256 skins): a sharper sleeve.

### Hull build

- **Drop the remaining kept copies** (ROUND21.md, "The hull build's memory"): the last level's kept copies, 550 MB at
  once without the budget (about 240 with it), could go as the levels' between did.

### Test tooling

- **`vrtutorial_playtest.py` room 8**: the mock's hand loading often throws the shells instead of loading them, so the
  range's door stays shut and every later gate fails (the arena is run with `--from arena`).

## Left open by finished plans (2026-10-09)

### Stealth AI: limits (STEALTH.md; open since it was built, 2026-10-08)

- The meter is one per monster, on the most suspicious player in its sight (not a meter per player: the dark player's
  own suspicion isn't kept while the lit one holds it).
- A client's light and lamp are his own client's word (sent in his VR move): a modified client could send "dark".
- Alert monsters walk with Quake's movetogoal (no path finding): a point across a gap or up a ledge ends their walk when
  stuck (3 s without headway), then they search where they are.
- Quake's own relay stays: an idle monster that sees another turn Hostile (FoundTarget's `sight_entity`, a tenth of a
  second) turns Hostile at its enemy too, whatever the light on him (as `vr_stealth_share_*`, but with Quake's sight
  ranges).

### Magazines sliding home (RELOAD.md; noted, not done, 2026-10-08)

A magazine seated at its well drawn sliding the last bit home: a short seat slide, as the shells' slide
into the guns (vr_collectfx.cpp's "into the gun" variant, its path the well's axis).

### Dawn of the Machine: co-op and dm1 (MG3.md, decision 6, 2026-10-06)

MG3 ships single-player first (`soloOnly` 5, as Dopa); its co-op and its deathmatch map (dm1) are for later.

### A turned teleporter's exit bound (HITZONES_AND_PORTAL_REVIEW_2026-10-04, open P3; the review removed 2026-10-09)

The body carried through a gate is checked at the destination with an axis-aligned bound (`vr_portals.cpp`, the
destination body bound), so a gate turned by a non-cardinal angle can block a narrow exit that the body would fit
through. A bound turned with the gate (or a box test in the gate's frame) would fix it.

### Old QuakeC notes (the old engine's "TODO VR" markers, triaged 2026-10-09: TECHDEBT_2026-10-09.md 10)

The notes about organizing, hitboxes and ideas were dropped; these name real, small issues:
- **Rogue's teamplay variants** (`rogue_teamplay.qc`): their item and weapon management is commented out (never
  rewritten for hands and holsters), so `teamplay 3`-style Rogue modes don't work in VR; their impulses
  (`weapons.qc`) are off too.
- **Rogue's lava nails against players** (`rogue_lava_wpn.qc`): Rogue's ignored armor; here armor counts (the save
  and restore of the armor around `T_Damage` is commented out). Deathmatch only.
- **`readytime`** (`vr_defs.qc`): one for every client (no shooting or taking weapons just after a load or a
  spawn); in co-op one player's spawn holds the others for that moment.
- **`.damage_weapon` of missiles** (`orig_mon_soldier.qc`, Honey's `FL_SPECIFICDAMAGE`): set for the grunt's shot;
  the plasma and other missile functions may credit the wrong weapon against targets that take only some.
- **A thrown laser cannon's Quake box** (`weapons.qc`, `WeaponIdToThrowBounds`): the old note said its bounds were
  wrong; unchecked since Box3D took thrown weapons' collision.

## Deferred from his release-day notes (2026-10-10)

- Nailgun and Thunderbolt magazine receivers: inset the bottom face with a silvery border where the magazine goes in,
  like the super nailgun (both look flat now).
- Thunderbolt: the right side's texture stretched and not symmetrical with the left; make it mirror the left. Many
  grooves painted on the texture should be inset in the geometry.
- Tutorial lesson 7: a pillar or wall right after the door so the player faces the fight banner on entering (needs
  geometry + relight).
- Tutorial final arena: the right-side railing is detached from the ledge; move it onto the ledge (func_detail: needs a
  recompile + relight).
