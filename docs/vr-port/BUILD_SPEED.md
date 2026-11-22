# Build speed

What a Quake VR build costs on the development PC, where the time goes, what has been done about it and what is left.
Earlier rounds (the no-LTO dev build, the VR precompiled header, vr_menu.cpp split into page groups, the hot headers
split): ROUND22.md "Build speed" (three sections) and ROUND24.md "Build speed: the hot headers split". Grep them.

## The build

- Workers and ships: the kit's `build.sh <tree>` (Release x64, no ThinLTO: `-p:QvrNoLto=1`). In order: the QC hash
  (`qc_hash.py`), the QC compile (patched fteqcc; any warning fails), the fteqcc regression tests (only when the
  compiler or the tests change), `qvr_checks all` in the background, MSBuild of `ironwail.sln`, then the checks' and
  the log's results. `ship_any.sh` runs build.sh once; `ship_next.sh` skips the integ build when integ has exactly
  the sources the tree built (`ship_stamp.sh`), so a ship builds once.
- Inside MSBuild, before the engine's compiles: `QvrBuildVersion` (git: `qvr_buildver.h`), `QvrCompileQC` (only when
  a QC input is newer than progs.dat; after build.sh's own compile it never is), `QvrQcTrackingLogs`; after the link
  the run-time DLLs are copied next to the exe.
- 875 translation units: ironwail 616 (472 of them Quake/vr), opus 137, qvr_doom 83, recast 17, zancle 11,
  rnnoise 10, mimalloc 1. Linked by lld-link (no incremental linking: lld has none).

## Measurements (2026-11-22)

The machine: 32 threads, 64 GB, with 3-6 other workers building and running tests the whole time (Windows' CPU load
readings 0-80% from one run to the next). Every number is a wall time from one such run, so read the ranges, not the
digits.

**A process start is the expensive thing here.** One start costs 0.1-2.7 s, and starts are mostly serialised:

| test | serial | in parallel |
|---|---|---|
| 16 x `where.exe /?` (a tiny native exe) | 17.3-18.5 s | 5.6-12.8 s |
| 8 x `clang-cl --version` | 9.5-14.2 s | 4.3-7.2 s |
| 10 x `/usr/bin/true` (Git Bash) | 0.8-1.4 s each | |
| an MSBuild `Exec` of `exit 0` (one cmd.exe) | 0.6-3.9 s | |

Memory (17 GB free, no paging) and disk (1% busy) are not the cause, and Defender's real-time scanning is off. Something
on this PC slows process creation; which program is not known. So every process a build starts costs about a second,
and a build's fixed cost is mostly process starts, not compiling.

**A no-op `build.sh` (old kit script, old project): 22.8-28.3 s**, timed step by step (`PS4='+ $EPOCHREALTIME' bash -x`):

| step | s |
|---|---|
| ~20 small processes in the script (cat, grep, head, tail, mkdir, cygpath, cksum, find, python) and ~10 subshells | ~12 |
| fteqcc (the QC compiled again although nothing changed) | 6.4 |
| MSBuild with nothing to do; of it `QvrBuildVersion` (four `Exec`s: four cmd.exe and four git) | 9.4; 8.9 |
| MSBuild's post-build `copy` of the 15 DLLs (one cmd.exe) | 1.3 |
| `qvr_checks all` (in parallel with MSBuild; its slowest checks: release_qc 3.1, fgd 2.5) | (3.1) |

**A full rebuild** (`-t:Rebuild`, the whole sln, `-ftime-trace`): **350 s wall** for **327 CPU-s** of compiling
(frontend 122, backend 183) under that load. With 32 threads that is ~10 s of compiler work: the other ~340 s are
875 clang-cl starts and waiting for CPU. (On 2026-10-25, on a quieter machine, the same rebuild took 21 s.) The first
build after a worker's tree moves to a new branch is close to this: 5 min 15 s today in this tree.

**Incremental builds** (the full build.sh, 1-3 runs each):

| change | old build.sh, old project (A) | patched build.sh, new project (C) |
|---|---|---|
| nothing | 22.8-28.3 s | 5.5-9.2 s |
| `vr_held.cpp` touched | 32.7-38.1 s | 6.0-10.8 s |
| a QC file edited (a comment added) | 36.0 s | 12.3 s |
| that edit undone | 31.5 s | 14.1 s |
| `vr_hands.hpp` touched (81 including files) | 165.5 s | 98.0 s |

(B, the old build.sh with the new project: 22-68 s no-op, too noisy to separate; the project change alone is measured
at the MSBuild level below.)

**Top 20 translation units** (CPU seconds in clang, from `-ftime-trace`; summed with a small script,
ClangBuildAnalyzer-style):

| TU | total s | frontend s | backend s |
|---|---|---|---|
| vr_menu | 10.46 | 1.76 | 8.67 |
| imgui | 4.42 | 1.09 | 3.31 |
| vr_agarmor | 3.63 | 0.36 | 3.24 |
| vr_view | 3.57 | 0.60 | 2.93 |
| imgui_draw | 3.26 | 0.38 | 2.87 |
| static (mimalloc) | 3.09 | 0.43 | 2.65 |
| vr_hull | 2.85 | 0.71 | 2.11 |
| imgui_widgets | 2.79 | 0.83 | 1.95 |
| vr_menu_pages_debug | 2.55 | 1.03 | 1.51 |
| image | 2.10 | 0.13 | 1.95 |
| vr_motion_review | 1.98 | 0.56 | 1.39 |
| vr_rigstudio | 1.89 | 0.29 | 1.57 |
| vr_agarmor_check | 1.80 | 0.19 | 1.57 |
| vr_ragdoll | 1.72 | 0.34 | 1.27 |
| vr_menu_pages_props | 1.71 | 0.84 | 0.86 |
| vr_nav | 1.71 | 0.37 | 1.26 |
| vr_avatar | 1.66 | 0.40 | 1.08 |
| vr_box3d_ragdoll | 1.66 | 0.53 | 1.06 |
| vr_view_hands | 1.61 | 0.44 | 1.13 |
| vr_menu_pages_settings | 1.61 | 0.97 | 0.63 |

**Top 20 headers by inclusion cost** (parse time summed over every TU that parses it, nested headers counted in their
includers too; the precompiled headers' contents count once, in the PCH's own TU). Input for the include trimming
(the zancle worker); this branch changes no source include.

| header | total s | TUs | per TU ms |
|---|---|---|---|
| um/windows.h | 11.3 | 49 | 231 |
| vr/vr_box3d_internal.hpp | 6.7 | 26 | 257 |
| immintrin.h | 6.3 | 31 | 203 |
| vr/vr_cvars.hpp | 5.6 | 395 | 14 |
| intrin.h | 5.0 | 25 | 199 |
| x86intrin.h | 4.9 | 25 | 195 |
| vr/vr_menu_internal.hpp | 4.7 | 8 | 588 |
| vr/vr_cvars.inc | 4.5 | 399 | 11 |
| vr/vr_flatplay.hpp | 4.2 | 55 | 76 |
| vr/vr_engine.hpp | 3.8 | 8 | 474 |
| um/winsock2.h | 3.8 | 9 | 421 |
| span | 3.7 | 55 | 67 |
| xutility | 3.5 | 65 | 54 |
| um/winbase.h | 3.4 | 50 | 69 |
| vr/vr_tunables.hpp | 3.3 | 175 | 19 |
| vr/vr_tunables.inc | 3.1 | 353 | 9 |
| vr/external/glm/glm.hpp | 3.1 | 8 | 388 |
| net_sys.h | 3.1 | 6 | 511 |
| vr/vr_backend.hpp | 2.9 | 228 | 13 |
| vr/vr_aggrip.hpp | 2.6 | 19 | 138 |

Notes for trimming: windows.h/winsock2.h come in through 42 files under Quake/ that include them directly; the
intrinsics headers through vr_ao.cpp, vr_crash.cpp, vr_findradius.cpp, vr_sha256.cpp and Box3D; vr_box3d_internal.hpp
(26 includers, 257 ms each) is the heaviest VR header that is not in a PCH; vr_cvars.hpp + vr_cvars.inc (~395 TUs,
25 ms each) and vr_tunables.hpp + .inc (~175-353 TUs) are cheap per file but everywhere. The heaviest template
instantiations are glm's (mat4, vec3, operator*: 0.6-0.7 s each in total), small. Backend (codegen) time is larger
than frontend time (183 vs 122 CPU-s): vr_menu (8.7 s backend at /Ob1) is the single long pole.

## What landed (branch agent/buildaudit)

1. **The version step in one `Exec`** (`quakevr.props`, `QvrBuildVersion`): one cmd.exe running three git calls (log
   for the date and hashes, rev-list for the count, status for "-dirty") instead of four `Exec`s (four cmd.exe, four
   gits). Same header text: the 8-character hash of `QVR_BUILD_VERSION` is what `--abbrev=8` gives (the 7-character
   abbreviation when git had to lengthen it, else the full hash's first 8). No-op MSBuild, 3 runs each: the step's
   median 2.9 s -> 1.9 s (1.5/10.0/2.9 -> 3.3/1.9/1.6). It runs before any compile, in every build: build.sh, F5, ships.
2. **The run-time DLLs by MSBuild's `Copy`** (`QvrCopyRuntimeDlls`, before `PostBuildEvent`): only the DLLs that
   changed, no cmd.exe. Was the post-build event's `copy` of all 15 on every build: 163-225 ms (1.3 s under heavy load)
   -> 11-12 ms. Same 15 files in `bin\x64\Release`.
3. **`/p:QvrSkipQcCompile=1`** (`ironwail.vcxproj`, `QvrCompileQC`): for a caller that compiles the QC itself (the
   patched build.sh below). Without it nothing changes.

Checked: the header identical in its clean and dirty forms, build.sh `built` (qvr_checks pass, 0 QC warnings), smoke
run (`map e1m1`) exit 0.

## Proposed

**The kit's build.sh and qc_hash.py: `C:\OHWorkspace\qvr-kit\scratch\buildaudit_kit.patch`** (`git -C qvr-kit apply`;
measured above as C, it is most of the gain: no-op 23-28 s -> 5.5-9 s, a one-file edit 33-38 s -> 6-11 s):
- The QC compile is skipped when the last one compiled the same inputs (the QC hash) with the same compiler and flags,
  without an error or warning, and progs.dat is not newer than that compile's stamp (`scratch/.qc_built`); then it
  says "QC unchanged since its last clean compile: progs.dat kept". The shipped-compiler comparison still forces a
  compile whenever it is due. `qc_hash.py` now also hashes what the QC compile reads beyond QC/: the engine headers the
  QC includes (`Quake/vr/*_shared.h`, `vr_weapon_ids.h`) and the tree's fteqcc, so the precedence check and the
  compiler comparison rerun on those changes too (they didn't before: a stale-check bug).
- MSBuild gets `-p:QvrSkipQcCompile=1`, so a touched-but-unchanged QC file is not compiled there either.
- Stamps read with `read`, outputs scanned with bash regexes and `mapfile`, `cygpath` replaced by string edits, the
  fteqcc test and qvr_checks staleness by `-nt` tests: no cat, grep, head, tail, cksum, find or cygpath unless there is
  something to print. The remaining processes in a no-op build: python (the hash), qvr_checks, MSBuild.
- Risk: the skip trusts the hash; a QC input outside it (none known: progs.src lists only QC/ files and the two header
  globs) would not trigger a compile. Emptying `scratch/.qc_built` (or any QC edit) forces one. The fteqcc test stamp
  moved to time stamps (`scratch/.qcc_tests_pass_t`): a compiler replaced by an older-dated copy would not rerun them
  (git always writes new times).

**Not done, for the author:**
- **Find what slows process creation** (the biggest lever by far: ~1 s a process, serialised; a full rebuild is 875
  clang-cl starts). Something hooks process starts (endpoint protection, monitoring or RGB software: iCUE and
  Afterburner top the CPU list). A system setting, so not touched here. Expected: the full rebuild back towards 21 s,
  every build's fixed cost under 3 s.
- **Unity (jumbo) builds** for Quake/vr and the libraries (opus 137 files, qvr_doom 83): ~10x fewer clang-cl starts,
  so most of a branch switch's 5 minutes. Costs: a one-file edit recompiles its whole chunk (a few seconds of CPU),
  and file-local names (anonymous namespaces, statics, `using`) collide across files, which needs source renames:
  wait for the include/std:: trimming to land, then try it on one folder.
- **ccache/sccache-style caching** (clang-cl works with sccache): a tree switching branches would fetch objects
  compiled by any other tree. Needs an install. Large gain for branch switches (every worker task starts with one).
- **`qvr_checks`'s release_qc** compiles the QC again on every build (3.1 s of a CPU, in parallel with MSBuild, so off
  the critical path today); it could key on the QC hash as the precedence check does once MSBuild's own fixed cost
  falls under 3 s.
- **The version step without git** when HEAD and the index haven't changed (reading `.git` files from MSBuild): saves
  the remaining ~1-2 s, but "dirty" still needs `git status`; small gain, more fragile. Not proposed.
- lld-link's `/DEBUG:GHASH` (with `-gcodeview-ghash`): the link is 1-2 s; small gain.
