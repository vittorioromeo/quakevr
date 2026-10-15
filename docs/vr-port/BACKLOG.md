# Backlog: agreed work for the next rounds

Items the author approved or asked about, not yet started. Each has a note on where it came from.

## Next round (approved)

### Active humanoid coverage for AD's soldiers (author, 2026-10-12)

Done 2026-10-15 (branch `agent/adrigs`; ROUND22.md, "Active rigs for AD's humanoids"): AD's soldier, enforcer and
knight variants, the death knights, guard, sergeant, fury, crossbow knight, zombie knight and mummy are driven alive and
dying (stagger, get-up, lift flail, swim, active deaths); the zombie knight's head zone moved onto its head. Left out:
the centurion (hover board) and the skull wizard (a robe, no legs). Awaiting his VR check.

### Pillar/pipe climbing (brace a hand on each side) (23:10:39, 2026-10-14)

Research climbing by bracing a hand on each side of a pillar or pipe and moving upward hand-over-hand. Open.

### Climbing leg animation (legs seek footholds) (23:10:39, 2026-10-14)

Legs should seek plausible support points on walls, ledges, pillars and ladders, so looking down shows believable
assistance during climbing. Open.

### Repository chores

## To manually revisit (author unsure)

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

- **Temporal AA, then DLSS/DLAA and FSR 3.1** (`docs/vr-port/TEMPORAL.md`): stage 2a (motion vectors for both eyes) is
  safe to start any time; the DLSS helper waits
  on TEMPORAL.md's section 12 questions (private use or public, the helper's licence, FSR 3.1, TAA as default AA, 120
  vs 90 Hz with DLSS).

## In progress, 2026-10-15 (other workers; not open here)

Glove customization, Model vertex wobble (2) (rigged monsters' vertices rebuilt from the fitted skeleton). Their texts were removed from
this file; the workers' ROUND22.md sections carry them.

## Proposed, waiting on the author

- **The graphics on the flat screen** (asked round 20): most features already draw on the desktop; to do: the
  float scene, tone mapping, grades and bloom for the window, and a check of each feature with VR off.
- **Lower-end graphics settings before a release** (the author, round 20: "maybe later on before release, you can
  prepare some lower-end graphics settings as well"): the shipped defaults (`quakevr/vr_defaults.cfg`) are the
  author's RTX 4090 settings; the presets (`vr_graphics_preset`) exist, but their low/mid tuning is unverified.
- **`.rtlights` support** (round 20 discussion): load DarkPlaces' hand-authored light lists where they exist to
  drive the shadowed map lights (better placed than the map's light entities).
- **Distance-field AO** (round 20 discussion): only if capsule/box AO leaves obvious gaps.
- **Flashlight optional; a brighter option** (the author, 2026-10-02): left: check that nothing else assumes the
  torch with `vr_flashlight` 0 (gadget hints, tips, tutorial), and whether one "Visibility" control raising ambient
  light, exposure and tone mapping together is still wanted over Ambient Light alone.
- **Menus and settings for players, not only for tuning** (the author, 2026-10-03; `docs/vr-port/archive/MENU_REVIEW.md`):
  left: gameplay-feel presets (or gameplay kept as one tuned design) and the per-slot cvars (`vr_wofs_*`,
  `vr_prop_*`) to data files.
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

### Enemies' own weapons for the player (the author, 2026-10-15)

- **Phantom Swordsman's sword** (Dissolution of Eternity's `monster_sword`, `rogue` QC): make its corpse, or the sword
  it leaves behind, usable as a melee sword.
- **AD enemies with unique weapons:** review AD's monsters that carry their own weapons (beyond the soldiers' guns
  already usable, and separate from "AD soldiers' guns: refine their VR models") and make each usable by the player.

### AD soldiers' guns: refine their VR models (author, 2026-10-15)

His note: "We will eventually refine these models." The eight guns AD's armed soldiers drop are single use and their
own (ROUND22.md, "AD soldiers' guns: their own again, our shots inside"); their models are cut from AD's monster models
by the engine (`<monster>.mdl#gun<n>`, `vr_adgun_models`), low-poly and textured as the monster. Per gun:
- **Rocket soldier's launcher** (`mon_soldier.mdl#gun4`): fires our rocket. Model: a proper launcher shape, a muzzle
  that reads where the rocket leaves.
- **Grenade soldier's launcher** (`mon_soldier.mdl#gun6`): fires our grenade (no longer AD's oversized one). Model: a
  drum or tube that reads as a grenade launcher, distinct from ours.
- **Plasma soldier's rifle** (`mon_soldier.mdl#gun8`), **eliminator's** (`mon_enforcer.mdl#gun6`) and **centurion's**
  (`mon_centurion.mdl#gun0`) plasma guns: AD's plasma bolt with our hit. Models: three distinct energy guns; maybe a
  glowing cell or coil (a fullbright skin) and a charge readout of their single-use rounds.
- **Defender's shotgun** (`mon_enforcer.mdl#gun3`): our pellets. Model: a wide-barrelled riot gun.
- **Pyro's flamethrower** (`mon_pyro.mdl#gun0`) and **fumigator's poison sprayer** (`mon_fumigator.mdl#gun0`): AD's
  flames and puffs. Models: a fuel tank or canister that shows the fuel left.
For all: grip and muzzle offsets checked in the headset, a pickup outline, the spent look (VR_EnemyGun_Spent) kept.

### Performance

- **Optimise decals** (PERF_DECISIONS.md 15): in a rocket fight (`play_e1m1_lights`) ~900 marks are 1.34 ms of the
  world pass's 2.67 at 2048 per eye, about half (~2.4 ms at his 2782 eyes); each pixel walks its cell's bucket (up to
  64 marks, a whole 80-byte record each before its early-out). Reading a mark's normal and middle first was tried and
  reverted (no gain, 0.6-0.9% of the pixels changed). `vr_decal_max` lowered to 768 meanwhile (config version 118).
  Left: the options with a visible change (PERF_DECISIONS.md 15), for him.

### Map and game loading (ROUND22.md, "Load-time speedups without their drawbacks (1.0.1, 2026-10-10, worktree `load101`)")

- **Model loading in parallel** (the calibration room's alias models, now 270 of the 430 ms: texture uploads, skins'
  decoding and copies, md5 replacements): the loaders share the hunk, cache and GL; would need each loader split into
  a decode into its own buffers (on the pool) and a commit in order on the main thread. Two exact pieces done instead.

### Art (ROUND21.md, "For the author": Blender)

- **Stretched UVs left** (ROUND21.md, "Patch 1.0.1: stretched skins re-mapped"): the grappling hook's claws (10% of it:
  their strips do not unwrap flat), the rocket launcher's and plasma gun's few faces whose strips would not unwrap
  flat; the same few on the lava super nailgun and multi rocket launcher (re-mapped too: ROUND22.md, "Patch 1.0.1:
  the lava super nailgun and multi rocket launcher re-mapped").
- **Body skins' texel density at the wrist**: about a sixth of the hands' (256 x 256 skins). The skins are the
  author's own repaint (commit 24fa2de6d): a re-map or a bigger skin only resamples his paint; a sharper sleeve needs
  him to paint the wrist and bracer at a higher resolution (or make_vrbody.py's painter at 512, losing his repaint).

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
- **`readytime`** (`vr_defs.qc`): one for every client (no shooting or taking weapons just after a load or a
  spawn); in co-op one player's spawn holds the others for that moment.

## Deferred from his release-day notes (2026-10-10)

- bHaptics support (vest, arms, head, hands: hits by direction, recoil, explosions, melee impacts) (the author,
  2026-10-11).
- Standalone Quest version (Android/Quest native build: OpenXR on Android, GLES/Vulkan renderer path, performance
  budget for the mobile GPU, packaging/sideloading) (the author, 2026-10-11).

TODO: linux support

## Rendering feasibility from 2026-10-12 voice notes

- **AD relief authoring pilot:** the pilot of 10-20 textures (reviewed height masks, `_bump`/`_norm` sidecars) is not
  started.
- **Optional vertex breathing reduction:** the dequantisation shipped; the per-bone rigid-fit option is the wobble
  item (2), in progress 2026-10-15 (another worker).
- **Hidden mock blank readback:** some isolated eye/window captures are entirely black even with flat lightstyles;
  investigate capture/target validity separately if needed. This is not evidence of the reported lamp flicker.
### Native AD inventory and interface (author, 2026-10-12 11:27:41)

Audited and the gadget's AD page added (ROUND22.md, same section). Left: the custom keys' names on it (strings, not
stats), the jump boots' power level, coop.

## Tracy review leftovers (2026-10-14)

- Tracy VR code (`Quake/vr/vr_tracy.cpp` and its hooks) uses `std::` containers, mutexes and atomics: move to Zancle
  per CODE_STYLE (Tracy build only; not a mechanical swap).
- The bench JSON / profile CSV settings list gained 7 `vr_pbr`/`vr_parallax` entries with Tracy commit d5ba4af43
  (harmless), and the capture metadata prefix scan still repeats those names: dedupe.
- Run VR profiler sessions (the `Release-TracyFull` build, with call stacks) and optimize the game.

## Open from round 22 (2026-10-14; credits ran out)

- **Neck snap:** Hand Line kept, Velocity removed (2026-10-15, ROUND22.md "Neck snap: the Hand Line only"). Still open: a wrist-roll-only test; `ragdoll/stagger_test.sh judo` under it.
- **AD humanoids and the neck snap / lift:** PARTLY done. Done: the centurion and fumigator are in
  `vr_foegrab_lift_classes` (config 134, cf44e08a3) and `judo_test.py adneck` (4f18290ce; ROUND22.md, "Neck snap: strict
  zone, diagnostic, slow motion, crack") lifted every AD humanoid, snapped 12 of 15 heads standing and measured held
  knocked-down pyro, fumigator and death knight. Left: the zombie knight's head sits 3-5 units to the side of its head
  zone (headfit 2.8 3.1 21.1; positional damage's zones have no sideways offset); the eliminator's and fumigator's heads
  were not taken from the zone's sides; AD's humanoids have no driven rigs, so a lift does not flail them; the
  centurion hovers (never knocked down nor lifted) and its snap is untested.
- (Done 2026-10-15, fastship: walk 2500 frames) **Body Inset vs vore shoves:** `voreshove_test.sh` now gets 3-4 shoves (needs 4) at inset 4: vores stand closer and die to their own splash; restore the test's margin.
- (Done 2026-10-15 except hotblade throw, unreproduced alone) **Flaky tests:** judo_test `hold` "held on" drag (moves 4-12 units by landing; ~1 in 3 under the 4-unit bar); hotblade_test `throw_cold`/`throw_hot` (axe sometimes arrives under 6.3 m/s after a bounce).
- (Done 2026-10-15 except Quoth long torch flame position) **Incandescent blades leftovers:** weapons start cold after a level change; Quoth's long wall torch is no fire spot (flame position unknown); force-grab/counter/toolgun glows override the heat glow on an aimed dropped blade.
- (Done 2026-10-15, ROUND22 "Main menu layout") **Main menu:** flat menu limited to Quake's 200 rows, clear of the corner buttons and notice box; VR menu with a Continue row checked after vertical centring (centred, no overlap). Open: the flat rows are tight (13 apart, no group gaps) on a short canvas.

## Test suite speed-up leftovers (2026-10-14, branch agent/fasttests)

- (Done 2026-10-15, worktree fastship: next merged in, the 28 batched suites proven again alone vs batched, 9361 s ->
  1646 s; TESTING.md "The batched suites, proven again on next".)
- Redo the conversions that were left uncommitted in worktree qvr-agents/fasttests (19 more release-suite scripts, 3
  Python suites, the kit README line): lost, that worktree was reused for another branch. (Runs built from earlier
  runs' output are batched: each runs as a batch of one, testbatch.sh.)
- Finish the full release suite's old-way baseline (stopped at 33 of 43) and record before/after.
- brush_test.py: not batched yet (uses saves and screenshots every run).
- guard/decap are batched with their frames drawn and voreshove with real-time frames (17, 32 and 15 s batched):
  frames not drawn / unpaced only if worthwhile (it changed their verdicts).

## Permanent decals leftovers (2026-10-14; design and status: docs/vr-port/PERM_DECALS.md)

- Phase 3: instanced brush models (b_*.bsp item boxes share one model) and props (alias models): per-entity layers
  with a unique UV unwrap, or keep projected decals with a cap.
- Phase 4: coop: clients bake from the replicated decal events; a mid-game joiner gets a snapshot or a fresh start.
- (Done 2026-10-15, `agent/permdecals3`: the atlas grows by layers up to `vr_permdecal_atlas_max`; past that the
  least recently baked page still gives its slot up. Downsampling old pages there is the remaining idea.)
- (Done 2026-10-15, `agent/permdecals4`: baked marks read on the texture's blocks with retro textures and nearest
  texture mode; gore's late marks on doors and lifts baked when they show; a stamp whose pages were all dropped kept.
  Marks on brush-tool boxes stay live by decision: PERM_DECALS.md, Decisions.)
- The layer is not pulled to Quake's palette (Retro Palette); a mark on both the world and a brush-tool box stays live
  whole (its world part too). Small, only if he asks.

## QuakeC VM overhead, short of a JIT (2026-10-14; profile first with the Tracy build's QC zones)

- Generic native predicates: QC registers a filter once (string or field/op/value triples, e.g. "flags & FL_MONSTER &&
  vr_liquid_next <= time && takedamage"), the engine compiles it; `findfilter(start, filterId)` walks edicts natively
  (generalises findflagsdue, d05011ec3).
- Native loops calling back into QC: `forall(filterId, func)` iterates and filters natively, entering the VM (via its
  function index, PR_ExecuteProgram) only for matches.
- Natively maintained entity lists (monsters, burning, ...) kept current as flags change, so loops visit members only.
- Faster interpreter (pr_exec.c): threaded dispatch (computed goto), fused superinstructions (field load + compare),
  fewer bounds checks in hot opcodes; typically 1.5-2x for all QC; verify with the QC regression suite.
- Biggest step, only if QC still dominates after the above: ahead-of-time translation of our own progs.dat to C at
  build time (VM kept as fallback for mods with their own progs); must match VM semantics, saves, debugging.
- Check what fteqcc supports beyond plain function pointers before relying on QC "lambdas".

## Playtest voice notes, 2026-10-14 (deferred by the author)

The author explicitly deferred every request below; only the blood-vignette coverage adjustment is current work.
All 21 recordings were transcribed and reviewed chronologically before triage. Times below identify source recordings.

### Arcane Dimensions and expansion regressions

- **AD stealth integration (22:13:00):** enemies seem to notice the player immediately rather than using the new stealth AI. Investigate native AD enemy integration; do not treat this observation as a confirmed cause.
- **AD enemy grenade launcher (22:14:26):** dropped soldier launcher produces oversized old-model grenades that sometimes stick in its muzzle. Use the newer projectile model and player-launcher-equivalent size/physics/clearance. Recorded weapon 21, `progs/mon_soldier.mdl#gun6`, in vrfiringrange near (8, -680, 41).
- **AD enemy weapon polish (22:16:03):** all enemy weapons need proper models, particles and general polish, beyond the launcher-specific issue above.
- **Expansion relief coverage (23:04:41):** some Scourge of Armagon textures appear to lack relief. Audit whether authored maps are missing or whether AD generation/loading caused a regression; preserve authored relief priority.
- **Armagon torso collision (23:16:08):** on hipend, shots hit the boss's legs but seem to pass through its torso. Investigate projectile/hit collision, recorded near (923, -676, 152).

### Throwing, grabbing and living ragdolls

- **Bullet-time throws (22:31:57):** small upward nudges can launch objects too far, while full throws feel wrong; available settings did not produce a good feel. Revisit velocity/time-scale handling and consistency.
- **Mid-air trick catches (22:31:57):** add appropriate main-handle grab leniency for re-catching tossed/spinning weapons, such as an axe; current timing/handle precision is too finicky.
- **Enemy occlusion of force grab (22:37:38):** bodies should block force-grab line of sight so physical enemy interaction does not accidentally grab an object behind the enemy.
- **Bullet-time lift/neck-snap responsiveness (22:38:26):** both interactions feel hard and unresponsive in slow motion; investigate gesture/velocity checks versus time scaling.
- (Partly: cvar vr_box3d_walk_nudge added; nudge does not fire yet, ROUND22 "Small fixes") **Walking nudges (r2m1, 22:48:15):** legs should gently push nearby/underfoot ragdolls without those bodies blocking the player's walking movement; avoid excessive pushes.
- **Head/wall slams and live-ragdoll access (dm2, 22:55:25):** allow grabbing a living enemy and smashing its head into a wall to kill it. Keep lifting as one way to activate ragdoll interaction, but investigate additional natural ways to enter that state.

### Neck-snap tuning and diagnostics

- **Strict head/neck grab region (dm2, 22:55:25 amended 22:55:58):** unintended snaps occur with hands far from the head or on shoulders. Restrict eligibility to the actual head/neck; shoulder holds must not count.
- **Crack sound impact (22:39:40):** even maximum volume feels weak/background. Investigate source position, attenuation/occlusion and sound treatment so a successful snap has a powerful crack.
- **In-world diagnostic toggle (23:01:26):** add to the Neck Snap menu, matching the selected algorithm. Show head/neck bounds, hand/twist vectors, angles, speed/strength and thresholds with arrows/bars on a selected or nearest enemy. Diagnose why twist-speed/angle setting changes seem ineffective and results feel inconsistent/random.

### Maps, presentation and future interactions


### Faster QuakeC interpreter: research notes (2026-10-14, Quake/pr_exec.c)

- Today: one switch over 66 opcodes in while(1); per statement ++profile with a runaway check (0x1000000) and an
  `if (qcvm->trace)` test; OPA/OPB/OPC recompute `&qcvm->globals[st->a]` per access (likely reloaded after stores:
  clang-cl in MSVC mode has no strict aliasing); QVR hooks: OP_ADDRESS world + fieldwatch checks, Tracy/vr_qcprofile/
  vr_profile_fine branches on calls (keep them).
- Steps (cheapest first): (1) cache globals/edicts in locals, separate traced loop, runaway counted on backward jumps
  and calls with profile totals kept exact (~0.5 day, ~1.2-1.4x); (2) computed-goto dispatch (clang only; ~0.5 day,
  ~1.2-1.3x); (3) pre-decoded statements per VM ({handler, operand pointers}) built at progs load, original statements
  kept for errors/PR_PrintStatement/xstatement/profile (~2 days, most of a ~2x); (4) superinstructions for the hot
  pairs profiling finds (field load + IF/IFNOT or compare, ADDRESS + STORE; ~1 day).
- Isolation: new loop in a VR file (e.g. vr_qcexec.c) with one `// QVR` hook in PR_ExecuteProgram and a cvar to fall
  back to the original loop. Behaviour identical: same results, runaway/error messages, profile counts; verify with
  the QC-heavy suites (ragdoll, AD, melee eval) and a seeded e1m1 log compared against the old loop.
- First measure QC's share of the frame on a busy map (vr_qcprofile, Tracy QC zones).
- 2026-10-15 (worktree `qcvm`): steps 0-2 done (vr_qcexec; QC_VM.md): the interpreter is ~2% of a 90 Hz frame, so
  steps 3-4 were left out; fewer statements (native hot functions, the filters above) would pay more.

## Model vertex wobble (2026-10-14)

- Cause: .mdl stores each frame's vertices as 3 bytes (0-255 per axis) times a per-model scale/offset, so positions
  snap to 1/255 of the model's extent (~0.25 units on a ~60-unit model), independently per frame: the rippling. Linear
  blending between frames adds limb shrinking/swimming.
- (2) Rigged monsters: in progress 2026-10-15 (another worker).
- (3) Higher-precision formats (MD3 16-bit, IQM/MD5 float; the 2021 rerelease ships float models): check what the
  engine already loads before relying on it; covers only models that have replacements.

## Other

- His words: "I feel that enemies like knights often get too close and go through the player when they perform a melee attack, which can be disorienting and make it difficult for the player to parry or counterattack. Is this something specific to knights? Should we add some sort of minimum radius distance from the player that the enemy will be smoothly pushed back into when performing a melee attack?"

## His notes, 2026-10-15

- More gore: thin blood streams from bullet holes in enemies; teeth flying out when punching faces; real-time vertex
  deformation showing flesh chunks torn away; thinner, longer-range blood splatters onto surfaces behind a shot enemy.
