# Repository cleanup review (2026-10-06)

A review of the tracked files (`git ls-files`: 3,885 files, 234 MB) for old, unused, duplicated or outdated
material. 501 of the files are Ironwail's own (they are in the merge base with `ironwail/master`, 1eabd0df2, and all
501 are still tracked); the other 3,384 are Quake VR's. Nothing was deleted by the review itself; the removals
Vittorio approved were done on 2026-10-07 (below, "Done").

**Confidence:** *certain* = named by no build (MSBuild's `ironwail.vcxproj` and `quakevr.props` globs, CMake's
`vr.cmake`, the Makefiles' `vr.mk`, `QC/progs.src`), no engine or QuakeC code, no map or `.ent`, no menu, no test or
tool that is kept, and verified by building and loading maps without them. *Likely* = unused, but a person might
still want it (a test aid, a one-off review script, a source file). *Check with Vittorio* = a decision about what
the project supports (Win32, the CI, Ironwail's legacy files, how big binaries are stored).

## Totals

| Confidence | Files | Size |
|---|---|---|
| Certain (removed 2026-10-07) | 20 | 0.20 MB |
| Likely | 25 | 0.13 MB |
| Check with Vittorio | ~135 | ~69 MB (50.5 MB of it is `phonon.dll`, which is used: the question is how to store it) |

Most of the repository is in use: every sound, map, model, `textures/`, `gfx/` and `textures_quetoo/` file is
reached (below, "What was checked"). The weight is in a few vendored binaries.

## Done (2026-10-07)

One `git rm` removed 139 files, 19.8 MB (blob sizes): the 20 "certain" rows below (the four `Misc/quakevr/pvs/` files
were already gone with the docs cleanup, 5c4358ea), `Misc/quakevr/repo_cleanup.sh` itself (its list is this doc),
`quakevr/progs/openhand.mdl`, the four third-party `.pdb` files, the Win32 platform (`Windows/SDL2/lib/`,
`Windows/codecs/x86/`, `Windows/curl/lib/x86/`, `Windows/zlib/x86/`, `Quake/Makefile.w32`,
`Quake/build_cross_win32{,-sdl2}.sh`), SDL 1.2 (`Windows/SDL/`), Watcom (`Windows/SDL2/watcom/`,
`Windows/codecs/x86-watcom/`), Code::Blocks (`Windows/CodeBlocks/`, `Linux/CodeBlocks/`) and `.github/workflows/`.
Edits before it: the `OpenHand` trait (`vr_modelmetadata.inc`, `vr_envmap.cpp`); the `Win32` configurations out of
`ironwail.sln`, `ironwail.vcxproj` and `zancle.vcxproj` (x64 only now); `Makefile.w64`'s SDL 1.2 branch defaults to the
system `sdl-config`; `vr.mk`'s and `IRONWAIL_DIFF.md`'s notes; `vrstart.bsp`'s own entity lump rewritten from
`maps/vrstart.ent` (the `.ent` already had Show/Hide Body in their place; with `external_ents 0` the map now matches);
`relight_maps.py`'s two comments; IK.md and MODELS_IN_BLENDER.md. Kept: `Makefile.w64` and its cross scripts (MinGW
x64), CMake (its `Windows/cmake-modules` still name `x86` for 32-bit hosts, which now find no bundled libraries),
`Windows/SDL2/bin/sdl2-config` (Makefile.w64).

**`generated.json` not re-recorded:** 22 records differ, not 30 (the 8 `finger_*`, `hand_base`, `grenade` and
`mervup` records guard only the skins after skin 0, `part`, and match; the review hashed whole files). The 22 are
`hand_rig.*` (0d5c225b, "the author's own edit in Blender") and `vrbody_*_00.tga` (24fa2de6, "the author's repaint"):
this is the guard working. Re-recording them would let `make_hand_rig.py` / `make_vrbody.py` overwrite Vittorio's
edits without a word; as they are, a rerun stops and names them (`--keep-edited` writes the rest).

Verified: build.sh (QC, statics, QC precedence, Release x64, fgdgen: 300 entities) `built`, 0 warnings; MSBuild
Debug x64 built; a mock run loaded e1m1 (`vr_body_mode 1`), vrstart, vrfiringrange, and vrstart with
`external_ents 0` (Show Body in its edicts), with no missing file or error.

## Certain (removed)

| Path | Size | Why it is unused | Evidence |
|---|---|---|---|
| `Misc/quakevr/slipgate_dark_{cmp,diag,here,pair,sweep}.sh` | 5 KB | Diagnostics of the dark slipgate view (fixed in ce1e1ed13, round 21); each says "diagnostic (not a shipped test)" | named nowhere (`git grep`), default `AGENT=slipgate-dark` |
| `Misc/quakevr/slipgate_see_diag.sh`, `slipgate_see_diag2.sh`, `slipgate_see_shot.sh` | 4 KB | Diagnostics of the far-gate view (89ca5735d); `see_shot` says its own readback "was taken out at the end of this round", and it sets cvars that never existed (`vr_eyeshot_mode`, `vr_hand_nudge*`, `vr_gadget_text`, `vr_hand_debug`) | named nowhere; those cvars are in no source and in no commit (`git log -S`) |
| `Misc/quakevr/slipgate_pvs_probe.sh`, `slipgate_pvs_sweep.sh` | 2 KB | Probes of the `start` staircase PVS question (9559e0939), "diagnostic (not a shipped test)"; closed by 88c0f9fd7 ("vr_pvs_selfleaf removed: the missing-self-leaf diagnosis came from a broken BSP reader") | named nowhere |
| `Misc/quakevr/pvs/head_map.sh`, `pvs/probe_head.sh` | 1 KB | Step probes of the same diagnosis (`AGENT=pvs-selfbit`) | named nowhere (FINDINGS.md names bspvis.py, q.py, extract_pak.py, sweep_images.sh, which stay) |
| `Misc/quakevr/pvs/added.txt`, `pvs/profile_run.txt` | 6 KB | A run's output: the leaf list from the broken BSP reader, and a console log | named nowhere |
| `Misc/quakevr/install_notes_oct5_build.ps1` | 1 KB | One-off: installed the October 5 notes build once the game closed | named nowhere |
| `Misc/quakevr/scratch/hz_reach.txt`, `hz_zones.txt` | 5 KB | Output files that `hz_reach.sh` / `hz_zones.sh` overwrite on each run, committed inside a folder `.gitignore` ignores | written by those scripts (`> Misc/quakevr/scratch/hz_*.txt`) |
| `quakevr/progs/vrtorso.mdl` | 172 KB | The old floating torso: its cvars (`vr_vrtorso_*`) were removed with it (IK.md), the body is `vrbody*.md5mesh` | no reference in Quake/, QC/, maps, `.ent`, Misc/ scripts or the Blender add-ons; only docs name it |
| `QC/hip_model.qc`, `QC/hip_spr.qc` | 4 KB | Hipnotic's model and sprite build scripts (`$spritename`, `$frame` directives for modelgen/spritegen): never QuakeC code | not in `QC/progs.src` nor `#include`d; fteqcc never sees them |

## Likely (not in the script: decide, then delete or keep)

| Path | Size | Why | Evidence |
|---|---|---|---|
| (Removed) `quakevr/progs/openhand.mdl` | 72 KB | Never loaded: no precache, no QC, no map; MODELS_IN_BLENDER.md: "aren't used at all" | Only `vr_modelmetadata.inc:101` (`OpenHand` trait, prefix `progs/openhand`) and `vr_envmap.cpp:1022` test the trait: remove those two with the model |
| One-off review scripts tied to a dated notes batch: `Misc/quakevr/body_shock_review.ps1`, `check_hitzones_review.py`, `hitzones_review.ps1`, `check_notes_feedback.py`, `notes_feedback_review.ps1`, `check_notes_oct5.py`, `notes_oct5_review.ps1`, `check_reach_feedback.py`, `reach_review.ps1`, `torch_hits_review.ps1`, `check_portal_ai.py`, `portal_ai_review.ps1`, `portal_ai_reverse_review.ps1`, `particles/check_effects_review.py`, `particles/effects_review.ps1` | 40 KB | Each checks one review's answers (NOTES_FEEDBACK_2026100x, POSITIONAL_DAMAGE_DEBUG, PORTAL_AI, EXPLOSION_AND_FIRE_EFFECTS); none is in TESTING.md's suites | named only by those dated docs: delete together with them if docsreview archives them |
| `Misc/quakevr/scratch/hz_reach.sh`, `hz_zones.sh`, `sgib_check.sh` | 6 KB | Acceptance runs of the hitzone and small-gib work, tracked inside `Misc/quakevr/scratch/` (ignored) | named nowhere; move to `Misc/quakevr/hitzones/` if worth keeping |
| `Misc/quakevr/scratch/make_portal_views.py`, `prepare_review_regressions.py`, `review_regressions.qc` | 10 KB | Same folder; TESTING.md and HITZONES_AND_PORTAL_REVIEW name them | keep, but move out of the ignored `scratch/` (a new file there is silently not added) |
| `Misc/quakevr/boxascii.py`, `boxgrid.py`, `pfmluma.py`, `shotgrid.py` | 5 KB | Image test aids from the portal-view work; `boxluma.py` and `pixdiff_ab.py` (kept) do the same jobs | named nowhere |
| `Misc/quakevr/pvs/diff_shots.sh` (+ `pvs/pngdiff.py`, used only by it) | 4 KB | Screenshot diff of the PVS investigation; `pixdiff_ab.py` covers it | named nowhere else |

## Check with Vittorio

| Path | Size | Question | Evidence |
|---|---|---|---|
| `Quake/vr/external/steamaudio/lib/windows-x64/phonon.dll` | 50.5 MB | Used (`vr_steamaudio.cpp` loads it, `quakevr.props` copies it), but it is 22% of the tree; each update adds 50 MB to the history. Git LFS, or fetch it at build time from the Steam Audio release? | one version in history so far |
| (Removed) `Windows/curl/lib/{x64,x86}/libcurl.pdb`, `Windows/zlib/{x64,x86}/zlib.pdb` (Ironwail's) | 9.9 MB | Third-party debug symbols: no build copies them (the post-build steps copy `*.dll` only; packaging ships `ironwail.pdb` only) | `ironwail.vcxproj` lines 119-223, `package-quakevr.ps1:84` |
| (Removed) Win32 platform: `Windows/SDL2/lib/`, `Windows/codecs/x86/`, `Windows/curl/lib/x86/`, `Windows/zlib/x86/` (Ironwail's) | 5.4 MB | Only the Win32 configurations use them; Quake VR's Win32 build is mock-only (`quakevr.props` links OpenXR for x64 only) and nobody ships it. Drop Win32 from `ironwail.sln`, both `.vcxproj` and `windows_ci.yml`, then these | `quakevr.props:28-44` |
| (Removed) SDL 1.2 and Watcom: `Windows/SDL/` (54 files), `Windows/SDL*/watcom/`, `Windows/codecs/x86-watcom/`, `Windows/CodeBlocks/*.cbp`, `Linux/CodeBlocks/*.cbp` (Ironwail's) | 3.3 MB | In no Quake VR build (MSBuild uses SDL2; the `.cbp` files do not list `Quake/vr`). Kept so far to merge Ironwail cleanly: removing an upstream file conflicts only when upstream changes it | not in `ironwail.vcxproj`, `CMakeLists.txt` or `vr.mk` |
| (Removed) `.github/workflows/*.yml` (Ironwail's, unchanged) | 8 KB | They run on every push to `origin`: `macos_ci.yml` asks for `macos-12` (a retired GitHub runner), Windows builds Win32 too. Delete, or update to x64 + current runners | untouched since the merge base |
| `Quake/anorm_dots.h`, `Quake/gl_warp_sin.h`, `Quake/filenames.h` (Ironwail's) | 30 KB | `#include`d nowhere (listed in the `.vcxproj`/`.cbp` only). Upstream dead code: leave for merges | `git grep` |
| `Quakespasm.html`, `Quakespasm.txt`, `Quakespasm-Music.txt`, `Linux/sgml/`, `Misc/fitzquake*.txt`, `Misc/QuakeSpasm_512.png`, `Windows/QuakeSpasm-old.ico` (Ironwail's) | 0.3 MB | QuakeSpasm/FitzQuake history docs and icons, not Quake VR's readme | upstream |
| (Kept as is, see Done) `Misc/quakevr/generated.json` | 48 KB | 30 of its 316 records no longer match their files: `progs/finger_*.mdl`, `hand_base.mdl` (make_bloody_hands.py), `grenade.mdl`, `mervup.mdl` (make_grenade_skins.py), `hand_rig.*` (make_hand_rig.py), `vrbody_*_00.tga` (make_vrbody.py). The files were changed after generation (the Blender add-ons, later passes): re-record them, or the generators' guard will treat a rerun as overwriting hand edits | sha256 of the committed blobs against the records |
| The shared `.git` (1 GB pack) | ~600 MB | The largest blobs are unreachable from every branch and tag (`Windows/VisualStudio/Report20200506-1121.diagsession`, 572 MB; old `pak10.pak`s, PSDs, `warden.bsp`, `apsp3.bsp`): reflog/stash leftovers. An expired reflog and `git gc` in Vittorio's own clone would reclaim them; nothing to commit | `git rev-list --objects --all` |

## Outdated content (fixes, not removals)

- (Done 2026-10-07: the lump is the `.ent`'s now.) `quakevr/maps/vrstart.bsp`: its own entity lump still had the "Show Torso" / "Hide Torso" buttons for the removed
  `vr_vrtorso_enabled`. Harmless while `maps/vrstart.ent` overrides it (`external_ents 1`, the default); a rebuild of
  the BSP from the `.ent` would drop them.
- (Done.) `Misc/quakevr/relight_maps.py` lines 29 and 1027 named `Misc/quakevr/relight_textures.cfg`; the file is
  `quakevr/relight_textures.cfg` (the code already looks there).
- `quakevr/.gitignore` ignored the tracked `quakevr/relight_textures.cfg` (`/*.cfg`): fixed (an exception, as for
  the other shipped `.cfg` files).
- Tracked files in an ignored folder: `Misc/quakevr/scratch/` (8 files, above). A new file there is not added.

## Docs for docsreview (not edited here)

- (Done 2026-10-07.) `docs/vr-port/IK.md` still opened with "Today's body is `progs/vrtorso.mdl`"; `MODELS_IN_BLENDER.md:233` lists
  `openhand.mdl` and `vrtorso.mdl`; update with the removals.
- `docs/vr-port/inventory/` (8 files, 300 KB, 2026-09-24): the pre-port inventory of the old engine (it describes
  `cl.vrtorso`, `vr_wofs_*`, `vr_vrtorso_*`); historical.
- `Misc/quakevr/pvs/FINDINGS.md` still presents the self-leaf diagnosis that 88c0f9fd7 found came from a broken
  reader.
- Dated reports nothing names: `ALLOCATION_AUDIT_20261005.md`, `EXPLOSION_AND_FIRE_EFFECTS.md`,
  `HITZONES_AND_PORTAL_REVIEW_2026-10-04.md`, `NOTES_FEEDBACK_20261004.md`, `NOTES_FEEDBACK_20261005.md`,
  `OVERDRAW_PROP_TAILS_20261005.md`, `PARTICLES_DECALS_PERFORMANCE_2026-10-03.md`, `POSITIONAL_DAMAGE_DEBUG.md`,
  `SLIPGATE_TORCH_REVIEW_2026-10-04.md`, `TORCH_TWOHAND_CRASH_20261005.md`, `ZANCLE_CONCURRENCY_REVIEW_2026-10-04.md`.
- `docs/vr-port/benchmarks/` (22 files, 1.1 MB): run manifests (`20261005_particle_optimization_manifest.json`
  alone 612 KB); `modelmetadata_20261005/{hands-control,interactions,particle-visual}.json` and `timings.csv` are
  named nowhere.
- `docs/vr-port/ROUND21.md` is 2.1 MB (212 files point into it): split by topic or archive the closed parts.
- `docs/vr-port/CHECKLIST_ARCHIVE_20261005.txt` (32 KB).

## `.gitignore` (committed with this review)

Added: the kit's per-worktree output (`/scratch/`, `/evaltakes/`; `/scratch/` was only in the shared
`info/exclude`), `.vscode/` (only in Vittorio's global ignore before), and in `quakevr/.gitignore` saves at any depth
(`*.sav`), the config's copies (`/ironwail.cfg.*`: the kit's `.baseline`, `.from-tests`, dated backups; 23 worktrees
showed `ironwail.cfg.baseline` as untracked), the engine's debug output (`/highlights/`, `/portalshots/`, `/wounds/`,
`/*_trace.txt`, `/weight_test.csv`, `/decal_atlas.png`, `/grasp_dump.obj`, `/qconsole.log`) and `/maps/autosave/`.
The root file's `quakevr/...` lines moved to `quakevr/.gitignore`. Every path Vittorio's checkout ignored before is
still ignored (checked with `git check-ignore` on its `git status --ignored` list); no tracked file is newly ignored.

**Vittorio's checkout** (`C:\OHWorkspace\quakevr-iw`, listed, not touched): `git status` is clean, nothing untracked.
Ignored local files worth a look: `quakevr/p1test.sav`, `p2.sav`, `p3.sav`, `quakevr/ironwail.cfg.backup_2026-09-29_0222`,
`quakevr/motions/pre_calibration_2026-09-28/`, `quakevr/maps/vrwip.*`, `quakevr/hand_rig.md5mesh.old`,
`Misc/quakevr/scratch/` (8 local scripts and `official-rerelease-qc/`), `build/`, `build-cmake/`, `dist/`.

## What was checked and found in use

- **Engine sources:** every `Quake/*.c` is compiled by MSBuild, or is another platform's (`net_bsd.c`, `net_udp.c`, `pl_osx.m`), or is `#include`d (`lodepng.c` by `image.c`);
  every `Quake/vr/*.cpp` is compiled (`quakevr.props` globs `Quake\vr\*.cpp`); every `Quake/vr` header is
  `#include`d. Vendored: box3d, zancle, glm (92 includes), fsr1, nis (`NIS_Config.h` by `vr_upscale.cpp`; the
  `.glsl.inc` files are `embed_glsl.py`'s output of the `.h` files beside them), steamaudio: all used.
- **QuakeC:** every `.qc` is in `progs.src` but the two hipnotic build scripts above.
- **Game folder (`quakevr/`, 1,653 files):** every sound (by path or numbered stem), map and bmodel (by
  `maps/*.ent`, the `.map` sources, BSP entity lumps), model (precache, modelmetadata, `vr_view.cpp`'s body builds),
  external skin (`<model>_<n>[_norm].png`, `.clean`, md5 `_NN_NN` skins: no orphan), `textures/particle_*`,
  `textures/vr/detail_*`, `gfx/vr/grade_*` (format strings in `vr_particles.cpp`, `vr_detail.cpp`, `vr_tonemap.cpp`)
  is reached. `textures_quetoo/`: all 1,077 files match a texture of id1, hipnotic, rogue or Quake VR's maps (by
  `select_quetoo_maps.py`'s pack names) or a `.mat` (`pwall1_*` by `sym06_2.mat`).
- **Duplicates (same blob):** only intended ones: `progs/vrbody{,_lean,_brawny}.mdl` (244-byte stubs for the md5
  bodies), `Misc/quakevr/src_models/r21/v_{ksword,hksword}.mdl` (the polish pipeline's input equals the shipped
  model), two `.mat` pairs, glm's empty `.inl` files, two SDL `def2lbc.awk`, two licences, one `.wav` pair.
- **Misc/quakevr** (315 files): the generators (`make_*.py`, `improve_weapons*.py`, `polish_weapons.py` and their
  `src_models/` inputs), the tests TESTING.md and BENCHMARKS.md run, the Blender add-ons and the trailer tools are
  current.

## Verification

With the 20 "certain" files moved out of the tree (`git mv` into `scratch/removed-trial/`, moved back after):
`build.sh` (fteqcc, check_statics, check_qc_precedence, MSBuild Release x64, fgdgen --check) printed `built`, 0
warnings, `fgdgen: 300 entities`; a mock-headset run of `map e1m1` (with `vr_body_mode 1`), `map vrstart` and
`map vrfiringrange` loaded every map with no missing-file message, Host_Error or crash.
