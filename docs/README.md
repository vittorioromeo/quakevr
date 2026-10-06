# Quake VR documentation

Every document in `docs/`, with one line on what it is for. The [README](../README.md) is the place to start: what
Quake VR is, the short installation and the default controls. New documents go in this index.

**Kinds of document.** *Guides* describe the game as it is now and are kept current. *Topic notes* explain one
system: how it works and why. *Research and plans* were written before or while something was built: their
"Status" line says how much of it exists. *Reports* are dated snapshots (measurements, reviews, feedback batches):
true on their date, not updated afterwards. The *round logs* (`ROUND*.md`) record each feedback round as it
happened; the durable parts belong in a guide or topic note.

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
| [vr-port/TESTING.md](vr-port/TESTING.md) | The playtest guide and the testing tools: the mock headset, scripted motions, tests and checks |
| [vr-port/BENCHMARKS.md](vr-port/BENCHMARKS.md) | The benchmark scenario suite: running it, what a run records, the scenarios |
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
| [vr-port/KNOCKDOWNS_2026-10-04.md](vr-port/KNOCKDOWNS_2026-10-04.md) | Shove knockdowns: live ragdolls that get up |
| [vr-port/PORTAL_AI.md](vr-port/PORTAL_AI.md) | Enemies seeing and shooting through seamless slipgates |
| [vr-port/POSITIONAL_DAMAGE_DEBUG.md](vr-port/POSITIONAL_DAMAGE_DEBUG.md) | The animated hit-zone debug view |
| [vr-port/EXPLOSION_AND_FIRE_EFFECTS.md](vr-port/EXPLOSION_AND_FIRE_EFFECTS.md) | Explosion debris and fire particles |
| [vr-port/MULTIPLAYER.md](vr-port/MULTIPLAYER.md) | Multiplayer: what runs on the server and what on the client |

## Research and plans

| Document | What it is |
|---|---|
| [vr-port/PLAN.md](vr-port/PLAN.md) | The original port plan from Quake VR (QuakeSpasm-Spiked) to Ironwail: principles, decisions, phases. Its [inventory/](vr-port/inventory/) lists the old engine's changes per subsystem |
| [vr-port/EXPANSIONS.md](vr-port/EXPANSIONS.md) | The official expansions (Dimension of the Past, Dimension of the Machine, Dawn of the Machine): audit and port status |
| [vr-port/MG3_PLAN.md](vr-port/MG3_PLAN.md) | Dawn of the Machine (MG3): the native port plan and the author's decisions |
| [vr-port/OFFICIAL_QC_SOURCE.md](vr-port/OFFICIAL_QC_SOURCE.md) | Where the official expansions' QuakeC comes from |
| [vr-port/INSTALLER.md](vr-port/INSTALLER.md) | Installer design, research and the author's decisions; section 13 is the app's phase 1 |
| [vr-port/MENU_REVIEW.md](vr-port/MENU_REVIEW.md) | The menu and settings review and proposal (2026-10-03); [MENU_INVENTORY.md](vr-port/MENU_INVENTORY.md) is its data |
| [vr-port/TEMPORAL.md](vr-port/TEMPORAL.md) | Temporal anti-aliasing and upscaling (TAA, DLSS, FSR): scope and design |
| [vr-port/PICKUP_THINKS.md](vr-port/PICKUP_THINKS.md) | Making idle pickups cheaper (research) |
| [vr-port/PORTING.md](vr-port/PORTING.md) | Porting the VR module to another engine (vkQuake) |
| [vr-port/ZANCLE_REPORT.md](vr-port/ZANCLE_REPORT.md) | Zancle issues and proposals found in the migration, for Zancle's author |

## Reports (dated snapshots)

Performance:
[PROFILING_2026-10.md](vr-port/PROFILING_2026-10.md) (the latest: CPU and GPU, loading and gameplay apart),
[PERFORMANCE_BENCHMARK_20261005.md](vr-port/PERFORMANCE_BENCHMARK_20261005.md),
[PERFORMANCE_AUDIT_20261005.md](vr-port/PERFORMANCE_AUDIT_20261005.md),
[PERFORMANCE_REVIEW_2026-10-03.md](vr-port/PERFORMANCE_REVIEW_2026-10-03.md),
[CPU_PERFORMANCE_FOLLOWUP_2026-10-03.md](vr-port/CPU_PERFORMANCE_FOLLOWUP_2026-10-03.md),
[SHADER_PERFORMANCE_REVIEW_2026-10-03.md](vr-port/SHADER_PERFORMANCE_REVIEW_2026-10-03.md),
[PHYSICS_PERFORMANCE_REVIEW_2026-10-03.md](vr-port/PHYSICS_PERFORMANCE_REVIEW_2026-10-03.md),
[PHYSICS_PERFORMANCE_RESULTS_2026-10-03.md](vr-port/PHYSICS_PERFORMANCE_RESULTS_2026-10-03.md),
[PARTICLES_DECALS_PERFORMANCE_2026-10-03.md](vr-port/PARTICLES_DECALS_PERFORMANCE_2026-10-03.md),
[PARTICLE_OPTIMIZATION_20261005.md](vr-port/PARTICLE_OPTIMIZATION_20261005.md),
[DECAL_OPTIMIZATION_20261005.md](vr-port/DECAL_OPTIMIZATION_20261005.md),
[PROP_OPTIMIZATION_20261005.md](vr-port/PROP_OPTIMIZATION_20261005.md),
[OVERDRAW_PROP_TAILS_20261005.md](vr-port/OVERDRAW_PROP_TAILS_20261005.md),
[MODEL_METADATA_20261005.md](vr-port/MODEL_METADATA_20261005.md),
[HULL_PRELOAD_20261005.md](vr-port/HULL_PRELOAD_20261005.md),
[ALLOCATION_AUDIT_20261005.md](vr-port/ALLOCATION_AUDIT_20261005.md),
[COMBAT_ALLOCATION_BURSTS_20261005.md](vr-port/COMBAT_ALLOCATION_BURSTS_20261005.md).
Their raw data is in [vr-port/benchmarks/](vr-port/benchmarks/).

Reviews and bugs:
[HITZONES_AND_PORTAL_REVIEW_2026-10-04.md](vr-port/HITZONES_AND_PORTAL_REVIEW_2026-10-04.md),
[SLIPGATE_TORCH_REVIEW_2026-10-04.md](vr-port/SLIPGATE_TORCH_REVIEW_2026-10-04.md),
[ZANCLE_CONCURRENCY_REVIEW_2026-10-04.md](vr-port/ZANCLE_CONCURRENCY_REVIEW_2026-10-04.md),
[TORCH_TWOHAND_CRASH_20261005.md](vr-port/TORCH_TWOHAND_CRASH_20261005.md).

Voice-note feedback batches:
[NOTES_FEEDBACK_20261004.md](vr-port/NOTES_FEEDBACK_20261004.md),
[NOTES_FEEDBACK_20261005.md](vr-port/NOTES_FEEDBACK_20261005.md),
[NOTES_FEEDBACK_20261005_BATCH2.md](vr-port/NOTES_FEEDBACK_20261005_BATCH2.md);
the checklist's ticked items: [CHECKLIST_ARCHIVE_20261005.txt](vr-port/CHECKLIST_ARCHIVE_20261005.txt).

## Round logs

[ROUND6.md](vr-port/ROUND6.md) to [ROUND20.md](vr-port/ROUND20.md), and [ROUND21.md](vr-port/ROUND21.md) (the
current round, a running log of every change, newest at the end). Search them for a cvar's or a feature's name to
find why it is the way it is.

## Elsewhere in the repository

- [Misc/quakevr/particles/README.md](../Misc/quakevr/particles/README.md): the explosion particle texture's source and how it was made.
- [Misc/quakevr/pvs/FINDINGS.md](../Misc/quakevr/pvs/FINDINGS.md): slipgate and hidden-staircase visibility measurements on `start` (2026-10-04).
- [quakevr/textures_quetoo/README.md](../quakevr/textures_quetoo/README.md): the Quetoo material maps and their
  authors.
- `Quake/vr/external/*/README.md`: each vendored library's version and licence (Box3D, Steam Audio, Zancle).
- [LICENSE.txt](../LICENSE.txt): the GNU GPL v2. `Quakespasm.txt` and `Quakespasm-Music.txt` are QuakeSpasm's own
  readmes, inherited through Ironwail.
