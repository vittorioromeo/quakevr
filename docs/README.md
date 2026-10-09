# Quake VR documentation

Every document in `docs/`, with one line on what it is for. The [README](../README.md) is the place to start: what
Quake VR is, the short installation and the default controls. New documents go in this index.

**Kinds of document.** *Guides* describe the game as it is now and are kept current. *Topic notes* explain one
system: how it works and why. *Research and plans* were written before or while something was built: their
"Status" line says how much of it exists. *Reports* are dated snapshots (measurements, reviews, feedback batches):
true on their date, not updated afterwards. The *round log* (`ROUND21.md`) records the current feedback round as it
happens; the durable parts belong in a guide or topic note.

## For players

| Document | What it is |
|---|---|
| [INSTALL.md](INSTALL.md) | Installation in detail: OpenXR runtimes, the mission packs and official campaigns, custom maps and mods, HD textures, relit maps, performance, troubleshooting |
| [RELIGHTING.md](RELIGHTING.md) | Relit maps and see-through water: the relight script, and relighting in the game (one map or many, ericw-tools downloaded by the game) |
| [FEATURES.md](FEATURES.md) | Every feature and how to use it |
| [SETTINGS.md](SETTINGS.md) | The menu pages, console variables and config files |
| [vr-port/TEXTURES.md](vr-port/TEXTURES.md) | The HQ texture pack (PNG): contents, credits, licence |
| [vr-port/MODS.md](vr-port/MODS.md) | Other mods in VR: what the compatibility mode does and doesn't do |

## For developers

| Document | What it is |
|---|---|
| [BUILDING.md](BUILDING.md) | Building the engine, the QuakeC and the release package; the tool scripts in `Misc/quakevr/` |
| [../Installer/README.md](../Installer/README.md) | The Windows installer (C#, WPF): layout, build, `qvr-setup`, tests, package manifests |
| [vr-port/CODE_STYLE.md](vr-port/CODE_STYLE.md) | Conventions of the VR module: state, Zancle instead of the standard library, scratch buffers and caches |
| [vr-port/PLAYTEST.md](vr-port/PLAYTEST.md) | The playtest guide: build and install, controls, voice notes, what to try now, troubleshooting |
| [vr-port/TESTING.md](vr-port/TESTING.md) | The testing tools: the mock headset, scripted motions, test scripts and checks, fixtures |
| [vr-port/BENCHMARKS.md](vr-port/BENCHMARKS.md) | The benchmark scenario suite (running it, what a run records, the scenarios) and the in-game profiling tools |
| [vr-port/MOTIONS.md](vr-port/MOTIONS.md) | The motion recorder: melee takes, playing them back, the melee evaluation |
| [vr-port/IRONWAIL_DIFF.md](vr-port/IRONWAIL_DIFF.md) | What the port changes in Ironwail's own files, and how to merge a new Ironwail |
| [vr-port/MAPPING.md](vr-port/MAPPING.md) | Making maps for Quake VR in TrenchBroom: the game configuration and the entities |
| [vr-port/MODELS_IN_BLENDER.md](vr-port/MODELS_IN_BLENDER.md) | Editing the body, weapons, gadget, flashlight and other models in Blender |
| [vr-port/HANDS_IN_BLENDER.md](vr-port/HANDS_IN_BLENDER.md) | Editing the jointed hand model in Blender |
| [vr-port/TRAILER.md](vr-port/TRAILER.md) | Tools for trailer footage: highlight markers, a rough cut, slow motion's sound |
| [vr-port/CREDITS.md](vr-port/CREDITS.md) | Everything the port uses or learned from, with licences. Add to it as you go |
| [vr-port/BACKLOG.md](vr-port/BACKLOG.md) | Agreed work not yet started |
| [vr-port/DOCS_REVIEW.md](vr-port/DOCS_REVIEW.md) | The documentation review of 2026-10-06: what is current, what to fix, what to write, what to archive |

## Topic notes (how a system works)

| Document | What it is |
|---|---|
| [vr-port/GRAPHICS.md](vr-port/GRAPHICS.md) | Why Quake looks flat in VR, and the graphics plan that followed |
| [vr-port/LIGHTING.md](vr-port/LIGHTING.md) | Real-time shadows and dynamic lights |
| [vr-port/HULLS.md](vr-port/HULLS.md) | The smaller player hitbox on unmodified maps |
| [vr-port/IK.md](vr-port/IK.md) | The full-body avatar and its IK |
| [vr-port/THROWING.md](vr-port/THROWING.md) | Throwing: the algorithm, the release, the research behind it |
| [vr-port/KNOCKDOWNS.md](vr-port/KNOCKDOWNS.md) | Shove knockdowns: live ragdolls that get up |
| [vr-port/PORTAL_AI.md](vr-port/PORTAL_AI.md) | Enemies seeing and shooting through seamless teleporters |
| [vr-port/POSITIONAL_DAMAGE_DEBUG.md](vr-port/POSITIONAL_DAMAGE_DEBUG.md) | The animated hit-zone debug view |
| [vr-port/EXPLOSION_AND_FIRE_EFFECTS.md](vr-port/EXPLOSION_AND_FIRE_EFFECTS.md) | Explosion debris and fire particles |
| [vr-port/MULTIPLAYER.md](vr-port/MULTIPLAYER.md) | Multiplayer: what runs on the server and what on the client |
| [vr-port/STEALTH.md](vr-port/STEALTH.md) | Stealth AI (idle, alert, hostile): the rules, cvars, cost and tests |
| [vr-port/RELOAD.md](vr-port/RELOAD.md) | Immersive manual reloading: the front pouch, shells, magazines, launchers' rounds |
| [vr-port/MG3.md](vr-port/MG3.md) | Dawn of the Machine (MG3) played natively: inventory, gameplay mapping, tasks M3-01..30, the author's decisions |
| [vr-port/MODEL_METADATA.md](vr-port/MODEL_METADATA.md) | The per-model metadata cache (`vr_modelmetadata.*`) |

## Research and plans

| Document | What it is |
|---|---|
| [vr-port/PLAN.md](vr-port/PLAN.md) | The port from Quake VR (QuakeSpasm-Spiked) to Ironwail: its principles and decisions |
| [vr-port/EXPANSIONS.md](vr-port/EXPANSIONS.md) | The official expansions (Dimension of the Past, Dimension of the Machine, Dawn of the Machine): audit and port status |
| [vr-port/OFFICIAL_QC_SOURCE.md](vr-port/OFFICIAL_QC_SOURCE.md) | Where the official expansions' QuakeC comes from |
| [vr-port/RELEASE_TODO.md](vr-port/RELEASE_TODO.md) | Before the first release: the author's short to-do list |
| [vr-port/RELEASING.md](vr-port/RELEASING.md) | Making and publishing a release: `Misc/release/make_release.ps1`, step by step, and the `latest.json` upload |
| [vr-port/INSTALLER.md](vr-port/INSTALLER.md) | Installer design, research and the author's decisions; section 13 is the app's phase 1 |
| [vr-port/TEMPORAL.md](vr-port/TEMPORAL.md) | Temporal anti-aliasing and upscaling (TAA, DLSS, FSR): scope and design |
| [vr-port/PERF_DECISIONS.md](vr-port/PERF_DECISIONS.md) | Performance decisions from the 2026-10-08 profiling run: trade-offs for the author |
| [vr-port/PICKUP_THINKS.md](vr-port/PICKUP_THINKS.md) | Making idle pickups cheaper (research) |
| [vr-port/PORTING.md](vr-port/PORTING.md) | Porting the VR module to another engine (vkQuake) |
| [vr-port/ZANCLE_REPORT.md](vr-port/ZANCLE_REPORT.md) | Zancle issues and proposals found in the migration, for Zancle's author |

## Reports (dated snapshots)

- [vr-port/ZANCLE_CONCURRENCY_REVIEW_2026-10-04.md](vr-port/ZANCLE_CONCURRENCY_REVIEW_2026-10-04.md): a code
  review of Zancle's concurrency; all five findings fixed (ZANCLE_REPORT.md, B9-B13). To remove once the Zancle branch
  is merged upstream.
- [vr-port/AUDIO_REVIEW.md](vr-port/AUDIO_REVIEW.md): the VR interactions' sound feedback (2026-10-07).
- [vr-port/CVAR_AUDIT.md](vr-port/CVAR_AUDIT.md): every setting checked for being read, ranked for removal (2026-10-06).
- [vr-port/REPO_CLEANUP.md](vr-port/REPO_CLEANUP.md): the tracked files reviewed for unused material (2026-10-06).
- [vr-port/TECHDEBT_2026-10-09.md](vr-port/TECHDEBT_2026-10-09.md): technical debt removed, and what needs the
  author's decision (2026-10-09).

## Round log

[vr-port/ROUND21.md](vr-port/ROUND21.md): the current round, a running log of every change, newest at the end.
Search it for a cvar's or a feature's name to find why it is the way it is. Its sections up to 2026-10-03 are in the
archive below; ROUND21.md's index lists each title and its file.

## Archive (`vr-port/archive/`: finished, kept for reference, not updated)

- [ROUND21_2026-09.md](vr-port/archive/ROUND21_2026-09.md) and [ROUND21_2026-10-01_03.md](vr-port/archive/ROUND21_2026-10-01_03.md):
  ROUND21.md's sections up to 2026-09-30 and of 2026-10-01..03, without their headset checklists.
- [STEALTH_PLAN.md](vr-port/archive/STEALTH_PLAN.md), [RELOAD_PLAN.md](vr-port/archive/RELOAD_PLAN.md) and
  [MG3_PLAN.md](vr-port/archive/MG3_PLAN.md): the plans as written; what was built is in STEALTH.md, RELOAD.md and
  MG3.md, what was left open in BACKLOG.md.
- [MENU_REVIEW.md](vr-port/archive/MENU_REVIEW.md): the menu and settings review and proposal (2026-10-03); the menus
  were rebuilt on 2026-10-07.

## Removed documents (in git history)

Removed on 2026-10-09 ([TECHDEBT_2026-10-09.md](vr-port/TECHDEBT_2026-10-09.md), item 9): `PROFILING_2026-10.md`
(its decision list was followed by PERF_DECISIONS.md), `HITZONES_AND_PORTAL_REVIEW_2026-10-04.md` (all fixed; its one
open P3 is in BACKLOG.md), and `inventory/` with PLAN.md's status, known gaps and phases (the old engine's inventory of
2026-09-24).

Removed on 2026-10-06 once their knowledge was acted on or moved (open performance leads to
[BACKLOG.md](vr-port/BACKLOG.md), the hull preload to [HULLS.md](vr-port/HULLS.md)): the round logs `ROUND6.md` to
`ROUND20.md`; the performance reports of 3-5 October (`PERFORMANCE_REVIEW`, `SHADER_PERFORMANCE_REVIEW`,
`PHYSICS_PERFORMANCE_REVIEW` and `_RESULTS`, `CPU_PERFORMANCE_FOLLOWUP`, `PARTICLES_DECALS_PERFORMANCE`,
`PERFORMANCE_BENCHMARK`, `PERFORMANCE_AUDIT`, `DECAL_`, `PARTICLE_` and `PROP_OPTIMIZATION`, `OVERDRAW_PROP_TAILS`,
`ALLOCATION_AUDIT`, `COMBAT_ALLOCATION_BURSTS`, `HULL_PRELOAD`) with their data in `vr-port/benchmarks/`;
`TELEPORTER_TORCH_REVIEW` and `TORCH_TWOHAND_CRASH` (all fixed); the voice-note batches `NOTES_FEEDBACK_*` (done);
`MENU_INVENTORY.md` (the 2026-10-03 menu inventory; a merge brought it back on 2026-10-07, removed again 2026-10-09; `menu_vr dump`, `Misc/quakevr/menu_coverage.py` and
`cvar_inventory.py` give the current data); `CHECKLIST_ARCHIVE_20261005.txt`; `inventory/_agent_brief.md`; and
`Misc/quakevr/pvs/` (the 2026-10-04 start-map visibility findings and probe scripts: both start maps' PVS is
correct). To read one: `git log --diff-filter=D --oneline -- <path>` gives the commit that removed it, and
`git show <commit>^:<path>` prints it.

## Elsewhere in the repository

- [Misc/quakevr/particles/README.md](../Misc/quakevr/particles/README.md): the explosion particle texture's source and how it was made.
- [quakevr/textures_quetoo/README.md](../quakevr/textures_quetoo/README.md): the Quetoo material maps and their
  authors.
- `Quake/vr/external/*/README.md`: each vendored library's version and licence (Box3D, Steam Audio, Zancle).
- [LICENSE.txt](../LICENSE.txt): the GNU GPL v2. QuakeSpasm's own readmes were removed on 2026-10-10 (in git history; credits in
  [CREDITS.md](vr-port/CREDITS.md)).
