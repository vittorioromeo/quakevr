# Documentation review (2026-10-06)

Every document in the repository was checked against the code on `vr-ironwail` at `c86c27c4`: its purpose, its
last change, and its claims (cvar names and defaults against `vr_cvars.inc` and `vr_defaults.cfg`, commands, file
paths, menu paths against a `menu_vr dump` of the running game, build and test workflows), plus broken links,
contradictions and duplication. Small factual fixes were made directly (branch `agent/docsreview`, one commit per
group of docs); what needs a decision or a larger rewrite is listed below. [docs/README.md](../README.md) is the
new index of every document.

**Follow-up (2026-10-06, the author's answers):** every archive and delete candidate below was removed (one `git rm`;
the index's "Removed documents" says how to read them from git history), after their references were pointed at git
history and their open items moved (performance leads to BACKLOG.md, the hull preload to HULLS.md).

## Summary

- **The player docs had fallen behind the game.** The Map Library and the in-game relighting were in no player doc;
  the settings reference described about 50 of the 150 menu pages, with pages renamed since (Gameplay is now World,
  Parry, Bash and Headbutt is Parry and Bash, Carrying and Gibs is Carrying plus Gibs and Corpses, ledge grab is
  Climbing); a dozen shipped defaults were wrong (World Scale, Menu Height, Water Alpha, bloom, contrast, climbing,
  throw assist...). All fixed.
- **The developer docs predated three changes:** the Visual Studio build compiling the QuakeC (`QvrCompileQC`), the
  fixed 72 Hz server tick (`host_fixedtick`), and the code's growth (PORTING said 16,000 lines of C++20; it is about
  200,000 of C++23 on Zancle). Fixed.
- **`docs/vr-port/` mixes four kinds of document** with nothing telling them apart: living guides, topic notes,
  dated reports (17 performance and review reports from 3 to 6 October) and round logs. The new index groups them;
  the reports and old round logs are archive candidates (below).
- **ROUND21.md (26,865 lines) holds most of the project's design knowledge** and no topic doc covers it: ragdolls,
  gore, melee, physics and props, climbing, the grappling hook, spatial audio, the retro look... (see "Missing
  documents").
- **One finding for the code, not the docs:** `quakevr/vr_defaults.cfg` ships `vr_menu_level "2"` (Menu Detail:
  Developer), while the README, SETTINGS.md and the cvar's own default (0) say players start at Standard. See
  "Needs the author".

## Inventory

Status: **current** (checked, nothing wrong), **fixed** (outdated, corrected in this review), **needs author**
(outdated in a way only the author can settle), **snapshot** (dated; true on its date, not to be updated),
**archive** (worth keeping out of the way), **delete** (no value left).

### Repository root and player docs

| Doc | Status | Notes |
|---|---|---|
| `README.md` | fixed | Added the Map Library, in-game relighting, climbing (was "ledge grab, experimental"), the belt flashlight, the VS build compiling the QuakeC, the docs index, and the credits for Box3D, Steam Audio, Zancle, FrikBot and Quaddicted. Removed "virtual keyboard" from "not carried over" (Search and Console have one). Still says Menu Detail starts at Standard (see "Needs the author"). |
| `docs/INSTALL.md` | fixed | Added a Map Library section, the in-game relighting, `ironwail.pdb` and `ericw-tools\` in the package table, the crash files (`qvr_crash.txt`/`.dmp`), the Menu Detail note, Official campaigns in the contents. Water Alpha 0.6 to 0.3, lava's 0.9, "Reset to defaults" to *Reset All*, `-noegs` to `-noepic` (the campaign lookup only checks `-noepic`). |
| `docs/RELIGHTING.md` | fixed | "A tool that does all this is planned" replaced (the game relights an episode, a game or every map); the shipped `light.exe`; the transparency defaults (water 0.3, slime 0.6, tele and lava 0.9) and their menu path. Open: line 75 says the re-release's maps aren't used, while INSTALL.md shows `--quake <re-release folder>`; check which is right. |
| `docs/FEATURES.md` | fixed | Fifteen wrong menu paths (Gameplay, Gibs and Heads, Wrist Gadget > Colours, Body > Flashlight...), left-handed mode now exists, Knights Drop Swords is gone, aim assist is on, ledge grab is Climbing, the torch is on the belt. Added: enemies' weapons, the hook's rope, crates, rocks, wall torches, ragdolls, knockdowns, stamina, bullet time, decapitation and limb gore, burning and lightning shock, tips, search and console, the retro look, spatial sound, official campaigns, the Map Library. |
| `docs/SETTINGS.md` | fixed | The Advanced VR Options tables regrouped as the menu groups them (Play and World, Combat, Movement, Carrying and Throwing, Gore, Body and Flashlight, Weapons, HUD and Menus, Graphics, Sound, Tips, Debug), with 30 pages added, Developer-only pages marked; shipped defaults corrected (world scale 1.2, menu height 1.6, menu distance 100 and scale 0.18, spacing 2, hue 100, bloom 0.08, contrast 2.5, climb 1, throw assist 1); *Reset All*. Every `vr_*` default in its tables now matches the code (checked by script). Still a hand-kept list: see "A settings reference generated from the code". |
| `docs/BUILDING.md` | fixed | The QuakeC section: the VS build compiles it (`QvrCompileQC`, `-p:QvrQcCompiler=`; the default path is the author's), `build.bat` adds the precedence and FGD checks. Tool table: the check scripts, `qvrbench.py`, `menu_coverage.py`, `make_vrcalibration_map.py`; not all scripts are Python. |
| `LICENSE.txt` | current | GPL v2. |
| `Quakespasm.txt`, `Quakespasm-Music.txt` | current | QuakeSpasm's readmes, inherited through Ironwail; leave them (upstream files). |
| `Misc/quakevr/particles/README.md` | current | The explosion particle texture's source. |
| `Misc/quakevr/pvs/FINDINGS.md` | snapshot | The 2026-10-04 start-map visibility investigation; correct. Archive with its probe scripts when convenient. |
| `quakevr/textures_quetoo/README.md` | current | Counts and cvars match. |
| `Quake/vr/external/*/README.md` | current | Box3D, Steam Audio, Zancle provenance. |
| `Installer/README.md` | current | Merged during this review (`ae805d67`, `2493007e`); new, so only linked from BUILDING and the index. The player docs don't mention the installer yet: add it to the README and INSTALL.md when it is published. |

### Developer guides and topic notes (`docs/vr-port/`)

| Doc | Status | Notes |
|---|---|---|
| `CODE_STYLE.md` | current | The statics rule, `mem::Scratch`/`Cache`, the checks: verified. |
| `TESTING.md` | fixed | The "first headset build, untested on OpenXR" intro replaced; "the kit" explained (it was used 20 times, never defined); step 2 knows the VS build makes `progs.dat`. Its tick text already matched the fixed tick. Overlaps BENCHMARKS (profiling) and BUILDING (mock commands). |
| `BENCHMARKS.md` | current | Every command and file exists. |
| `MOTIONS.md` | current | |
| `MAPPING.md` | current | |
| `HANDS_IN_BLENDER.md`, `MODELS_IN_BLENDER.md` | current | |
| `TEXTURES.md`, `TRAILER.md`, `MODS.md` | current | |
| `CREDITS.md` | fixed | Added Zancle (zlib; moodycamel's queue BSD), FrikBot X (public domain) and Quaddicted (the Map Library's source). |
| `IRONWAIL_DIFF.md` | fixed (partly) | Added the current totals (91 files, +5209 -722, 749 hunks) and noted `progs.h` is modified again. The per-file table is still the 74-file state: regenerate it when Ironwail is next merged. |
| `PORTING.md` | fixed (partly) | Size and language corrected (C++23 on Zancle, ~200k lines, 91 engine files); its other counts (70 files, 14,000 lines reused, `vr_gfx_gl.cpp` 400 lines, now 1703) are from September. Rewrite only if a vkQuake port becomes real. |
| `PLAN.md` | snapshot (labelled) | Status table is 2026-09-25 ("untested on HMD", "not ported: menu laser, model shadows"): marked as a snapshot pointing to FEATURES.md. Keep with `inventory/` as the port's history. |
| `MULTIPLAYER.md` | fixed | The 72 Hz tick recommendation and P0-6's tick item marked done (`host_fixedtick`). P0-6's crash site (`vr_imgprefetch.cpp` `workers`) no longer exists: whether the dedicated server's crash on quit is gone is unverified. |
| `GRAPHICS.md` | fixed | `relight_textures.cfg` path; "why it looks flat today" labelled as before round 15. Research background: keep. |
| `LIGHTING.md` | fixed | `vr_dlight_uncapped` defaults to 1. Missing `vr_shadow_head` (1, your shadow's head) in its table. |
| `HULLS.md` | current | |
| `IK.md` | fixed | Step 6 (legs) done; the body is `vrbody.md5mesh`; `vr_body_mode` 3 is the default and 1 is no longer in the menu; `vr_body_torso_back` 0.07 shipped; `vr_body_arm_stretch` 1.2. |
| `THROWING.md` | fixed | `vr_throw_release` (not `_release_mode`), drop 0.3, lever arm 0.1, the assist's real defaults (on, cone 15, strength 0.35; no `_gaze`), the never-built `vr_debug_throw_dump` marked. Sections 1-3 are 2026-09-24 research. |
| `TEMPORAL.md` | fixed | Stage 1 (FSR/NIS, foveated) shipped in round 20; TAA/DLSS not built (still in BACKLOG). |
| `KNOCKDOWNS_2026-10-04.md` | fixed | Added the later ledge and wiggle cvars. A lasting topic note: rename to `KNOCKDOWNS.md` (links: none to fix). |
| `PORTAL_AI.md` | current | Fold into a slipgates note one day. |
| `POSITIONAL_DAMAGE_DEBUG.md` | current | A 28-line debug-view note; merge into a positional-damage note (below). |
| `EXPLOSION_AND_FIRE_EFFECTS.md` | fixed | "On branch `codex/explosion-debris`" replaced: merged. |
| `PICKUP_THINKS.md` | current (research) | Unbuilt; its QC line numbers drifted. Keep while in the backlog. |
| `MENU_REVIEW.md` | fixed | Its before-the-rework sections labelled as such; the Status section is right. |
| `MENU_INVENTORY.md` | snapshot | 2026-10-03: 126 pages and 1,681 cvars; now 150 pages and 2,060 cvars, its line references stale, 25 pages missing. Generated by throwaway scripts. Archive; replace with a generated reference. |
| `INSTALLER.md` | fixed (partly) | Section 13 describes the merged app. Fixed: the tools folder, pack counts (100/131), config version 94, decision 5 done. Section 9 still recommends Inno Setup while the app is WPF, and the plan assumes the in-game relight gives see-through water (it has no VisPatch step): needs the author when the installer is merged. |
| `EXPANSIONS.md` | fixed (partly) | The "MG3 decisions unanswered" line updated; one cp1252 byte made UTF-8. As a log it is accurate, but it has no current-status summary at the top, and tasks 3-14 carry no done marks (3-9 are done per later sections). |
| `MG3_PLAN.md` | fixed | Status line: phase A built, the decisions section overrides section 4 (M3-11's Mjolnir branch and M3-12's axe-only rule contradict decisions 1 and 2). |
| `OFFICIAL_QC_SOURCE.md` | current | |
| `BACKLOG.md` | current | Its trailing "TODO" stub renamed. |
| `ZANCLE_REPORT.md` | current | The living list of Zancle issues for its author. |
| `inventory/` | snapshot | The old engine's changes per subsystem (2026-09-24), linked from PLAN.md. `_agent_brief.md` is an agent prompt nobody links: delete candidate. |

### Reports (dated snapshots)

| Doc | Status | Notes |
|---|---|---|
| `PROFILING_2026-10.md` | snapshot (labelled) | The newest performance report; measured before the fixed tick (now said at the top). Its "Reproduce" commands repeat BENCHMARKS'. |
| `PERFORMANCE_REVIEW_2026-10-03.md`, `SHADER_PERFORMANCE_REVIEW_2026-10-03.md`, `PHYSICS_PERFORMANCE_REVIEW_2026-10-03.md`, `PHYSICS_PERFORMANCE_RESULTS_2026-10-03.md`, `CPU_PERFORMANCE_FOLLOWUP_2026-10-03.md`, `PARTICLES_DECALS_PERFORMANCE_2026-10-03.md`, `PERFORMANCE_BENCHMARK_20261005.md`, `PERFORMANCE_AUDIT_20261005.md`, `DECAL_OPTIMIZATION_20261005.md`, `PARTICLE_OPTIMIZATION_20261005.md`, `PROP_OPTIMIZATION_20261005.md`, `OVERDRAW_PROP_TAILS_20261005.md`, `ALLOCATION_AUDIT_20261005.md`, `COMBAT_ALLOCATION_BURSTS_20261005.md` | archive | The performance cluster. Most items were acted on; still open: PHYSICS_RESULTS' "not done" list (hit-box traces, force-grab search), PERFORMANCE_BENCHMARK's shadow and portal priorities, OVERDRAW's trimming default. Four of them link code by absolute local paths (`C:/OHWorkspace/quakevr-iw/...:line`, 146 links), which work nowhere else. |
| `HULL_PRELOAD_20261005.md` | archive | Done; its design belongs in HULLS.md (a paragraph). |
| `MODEL_METADATA_20261005.md` | keep, rename | Describes a lasting design (`vr_modelmetadata.*`) and PORTING links it: rename to `MODEL_METADATA.md`. |
| `HITZONES_AND_PORTAL_REVIEW_2026-10-04.md` | checked 2026-10-06 | Its status table: every finding fixed by `98864d26` (2026-10-04), the HANDOFF/QUEUE corrections obsolete; one P3 left (the body's axis-aligned bound at a turned gate's exit, `vr_portals.cpp`). |
| `SLIPGATE_TORCH_REVIEW_2026-10-04.md` | archive | All five findings fixed, it says. |
| `ZANCLE_CONCURRENCY_REVIEW_2026-10-04.md` | updated 2026-10-07 | Its status table: all five defects fixed on Zancle's branch `zancle-concurrency-fixes` (one commit each, vendored at `2f8a1ca5`; to merge into `rebrand_to_zancle`); in ZANCLE_REPORT as B9-B13. |
| `TORCH_TWOHAND_CRASH_20261005.md` | archive or delete | One fixed crash; ROUND21 has it too. |
| `NOTES_FEEDBACK_20261004.md`, `NOTES_FEEDBACK_20261005.md`, `NOTES_FEEDBACK_20261005_BATCH2.md` | archive | Voice-note batches, implemented. |
| `CHECKLIST_ARCHIVE_20261005.txt` | delete candidate | An old copy of the runtime `checklist.txt`'s ticked items; nothing links it. |
| `benchmarks/` | archive with the reports | 16 JSON/CSV files and `modelmetadata_20261005/`, evidence for the 5 October reports; no script reads them. |

### Round logs

| Doc | Status | Notes |
|---|---|---|
| `ROUND6.md` .. `ROUND13.md` | archive | 56-225 lines each; only TESTING.md links them. |
| `ROUND14.md` .. `ROUND20.md` | archive (fix links) | Linked from TESTING, LIGHTING, BUILDING, CREDITS, and from code comments (ROUND15 `vr_handpose.cpp`, ROUND16 `vr_twohand.hpp`, ROUND17 `vr_ambient`/`detail`/`envmap`/`tonemap` and `make_detail.py`, ROUND18 `make_sounds.py`, ROUND20 `vr_ao.hpp`): a move needs those paths updated. |
| `ROUND21.md` | current (log) | Not edited here. Its durable knowledge should move into topic notes (below); then it can be split by month or archived at the round's end. |

## Needs the author

1. **Menu Detail on first start:** `vr_defaults.cfg:41` ships `vr_menu_level "2"` (Developer). The cvar defaults to 0
   and every player doc says Standard. If it was saved from your machine by `vr_savedefaults`, it should go back to
   0 (or the line removed) before a release. Not changed here (cvarclean and the defaults are code).
2. **RELIGHTING.md line 75 vs INSTALL.md:** settled 2026-10-06. The script relights the maps of the folder `--quake`
   names (the Steam Quake folder: the original maps; the `rerelease` folder: the re-release's), the game the maps it
   plays; both drop the re-release's worldspawn light settings, so INSTALL's "brighter" was wrong. Both docs fixed.
3. **Archive or delete** (below): done 2026-10-06, all removed (git history keeps them).
4. **Open review findings:** checked 2026-10-06 (each review now has a status table). HITZONES_AND_PORTAL: all fixed
   (`98864d26`) but one P3. ZANCLE_CONCURRENCY: all five fixed on Zancle's `zancle-concurrency-fixes` (2026-10-07, vendored;
   ZANCLE_REPORT B9-B13): the branch's merge into `rebrand_to_zancle` is yours.
5. **INSTALLER.md:** its Inno Setup recommendation (section 9) against the WPF app, and the see-through water a
   Python-free install would lose (the in-game relighting has no VisPatch step).

## Missing documents

In order of value. Each would be written from ROUND21.md's sections and the code, so ROUND21 stops being the only
place the knowledge lives.

1. **A settings reference generated from the code** (`docs/SETTINGS_REFERENCE.md`): every page, row, cvar,
   default and help line, from `menu_vr dump` (the `MDPAGE`/`MDROW` lines `menu_coverage.py` already reads) and
   `vr_cvars.inc`'s comments, regenerated by a script and checked by `build.sh` like the FGD. SETTINGS.md then keeps
   only the guided part (the main page, presets, offsets, config files). It would have caught every wrong default
   and page name found here. A doc-defaults check (`| `vr_x` | value |` rows against `vr_cvars.inc` and
   `vr_defaults.cfg`) is about 30 lines of Python and could run in the same step.
2. **A contributor and agent workflow guide** (`docs/vr-port/WORKFLOW.md`): the kit (`build.sh`, `run.sh`, `eval.sh`,
   `bench.sh`, `new_agent.sh`, worktrees, the scratch folder), the rules (no function-local statics, the QC
   precedence check, the calibration-board path check, debug tools into the Debug pages, his tweaks become
   defaults), and how a round runs (ROUND notes, checklist, voice notes). Today these live only in the kit's README
   outside the repository and in agent prompts; TESTING.md says "the kit" twenty times without defining it.
3. **An architecture overview of the VR layer** (`docs/vr-port/ARCHITECTURE.md`): the module's parts (`vr_api.h`, the
   OpenXR and mock backends, rendering, input, body/IK, Box3D physics, props, menus, effects, audio, the thread
   pool), how the engine calls in (the `// QVR` hooks), the QuakeC side (builtins, the progs ABI, spawn parms), the
   client/server split (with MULTIPLAYER.md), and where state lives (CODE_STYLE). PLAN.md and PORTING.md hold
   September pieces of this.
4. **Gameplay systems notes**, one each, from ROUND21:
   - `RAGDOLLS.md` (ragdolls 1-6, masses, grabbing limbs; absorb KNOCKDOWNS);
   - `GORE.md` (wounds, blood, gibs and small gibs, brain chunks, decapitation and head pops, limb gore, washing;
     absorb POSITIONAL_DAMAGE_DEBUG and the precise hit zones);
   - `MELEE.md` (the blow model, parry, bash, counters, deflection, stamina, batting; MOTIONS.md stays the recorder);
   - `PHYSICS.md` (Box3D, standing on props, phasing, the physics threads, props: crates, rocks, the crowbar,
     explosive boxes; holding and weight);
   - `CLIMBING_AND_GRAPPLE.md`, `FLASHLIGHT_AND_GADGET.md`, `AUDIO.md` (Steam Audio), `RETRO.md` (the retro textures
     and lighting), `BULLET_TIME.md`.
5. **A testing and benchmark quick start** (one page at the top of TESTING.md or its own file): build, run a scripted
   headless test, run the melee eval, run a benchmark scenario, read `vr_profile`; pointing into TESTING,
   BENCHMARKS and MOTIONS for the rest. Today the three docs overlap (TESTING's "Profiling" repeats BENCHMARKS;
   PROFILING_2026-10's "Reproduce" repeats both) and none is the entry point.
6. **`PERFORMANCE.md`**: the standing knowledge of the 17 performance reports (what costs what, what was fixed, what
   is still open, the budgets), so the reports can be archived.

## Not worth keeping where they are

To archive under `docs/vr-port/archive/` (a move, links fixed): the 14 performance reports listed above with
`benchmarks/`, HULL_PRELOAD, SLIPGATE_TORCH_REVIEW, TORCH_TWOHAND_CRASH, the three NOTES_FEEDBACK files, MENU_INVENTORY,
ROUND6 to ROUND20 (with the code comments' paths), and `Misc/quakevr/pvs/FINDINGS.md` with its scripts. After their
open items move: HITZONES_AND_PORTAL_REVIEW, ZANCLE_CONCURRENCY_REVIEW.

To delete: `CHECKLIST_ARCHIVE_20261005.txt` (an old copy of runtime data) and `inventory/_agent_brief.md` (an agent
prompt).

Done 2026-10-06: all of these were deleted rather than archived (the author: "we can recover them through git
history"); `MODEL_METADATA_20261005.md` and `PROFILING_2026-10.md` stay.

## Coordination

- `cvarclean` (CVAR_AUDIT.md) may remove cvars these docs name: SETTINGS.md's tables, FEATURES.md, LIGHTING.md,
  THROWING.md, KNOCKDOWNS and IK.md name cvars by hand. After its removals, rerun the doc-defaults check (or grep the
  removed names in `docs/`).
- `repoclean` (REPO_CLEANUP.md) owns the repository layout: the archive moves above may fit its plan.
