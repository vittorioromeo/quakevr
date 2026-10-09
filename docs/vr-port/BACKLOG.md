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

- **Funding links** (the author, after round 20): copy `.github/FUNDING.yml` from `master` to the port's branch
  (`github: SuperV1234`, `patreon: vittorioromeo`, the PayPal link) and add Ko-fi
  (`ko_fi: vittorioromeovee`, i.e. https://ko-fi.com/vittorioromeovee).

## Proposed, waiting on the author

- **Temporal AA, then DLSS/DLAA and FSR 3.1** (`docs/vr-port/TEMPORAL.md`): stage 1 (motion vectors for both
  eyes) is safe to start any time; the DLSS helper waits on the questions at the end of TEMPORAL.md (private use or
  public, the helper's licence, FSR 3.1, TAA as default AA, 120 vs 90 Hz with DLSS).
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
- **Flashlight optional; a brighter option** (the author, 2026-10-02): make the flashlight completely optional (off
  with no belt torch, no zones, no gadget hints, nothing that assumes it), and give an easy way to raise ambient
  lighting for players who don't want the moody atmosphere and prefer higher visibility (e.g. one "Brightness" or
  "Visibility" preset/slider on the main VR page that raises ambient/minimum light, exposure and tone mapping
  together, rather than many separate graphics settings).
- **Menus and settings for players, not only for tuning** (the author, 2026-10-03): `docs/vr-port/archive/MENU_REVIEW.md` (its data,
  `MENU_INVENTORY.md`, was removed 2026-10-06: git history). Built on branch `vr-ironwail-menus` (archive/MENU_REVIEW.md, "Status"): Menu Detail levels, VR Settings
  for every player with Comfort and Handedness presets and volume, VR Calibration at a first start, one home per
  setting, changed settings marked with Reset This Page and Changed Settings, the `wait5` fix. To test in the headset
  and merge. Left: gameplay-feel presets, a comfort vignette, per-slot cvars to data files (the questions at the end
  of archive/MENU_REVIEW.md).
- **Performance leads left open by the 2026-10-03/05 reports** (the reports were removed 2026-10-06; git history has
  them: `PHYSICS_PERFORMANCE_RESULTS_2026-10-03.md`, "Not done", and `PERFORMANCE_BENCHMARK_20261005.md`, items 4-5):
  - *Hit-box traces:* skip `touchNearby` for props too slow to hurt, after checking the monsters' QC touch functions
    (a leaping dog's or fiend's touch acts on what it meets).
  - *Force-grab search:* an engine builtin (findradius, cone and eligibility in C++), or a coarse cone test on the
    origin before `modelcentre`.
  - *Floating props' equilibrium sleep:* a design decision (they would stop bobbing).
  - *Touch queries:* `FL_EASYHANDTOUCH` grows every prop's box for the hands; prop-to-prop queries could use the
    ungrown box.
  - *Shadows in crowded, brightly lit scenes* (32 overlapping lights: 1.59 ms GPU against 1.15 ms without shadows):
    caster and bone reuse, caching for stable lights and objects, per-light update budgets, resolution by projected
    importance (`vr_lighting.cpp`, dynamic caster collection).
  - *Teleporter views* (about 0.8 ms GPU and CPU for one doorway): the destination's resolution chosen by the
    aperture's projected size, tighter destination draw lists, per-frame entity preparation shared across views.
- **Low priority: review the pickup-thinks research** (the author, 2026-10-03: "save your research regarding the
  nextthink stuff in a document so that we can review it later"): `docs/vr-port/PICKUP_THINKS.md`. Idle pickups think
  every 0.02 s; options to think less without changing gameplay (idle interval with exact deadlines, no thinks while
  carried, the spin left to the engine), and where thinking resolution would be lost (engine-side knocks, unless the
  engine wakes the pickup). Small absolute gain (QuakeC is ~0.07 ms a frame): review, then decide.

### To sort (the author's notes)

- KoFi links in installer, see CircuitLord's TF2 as an example

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
