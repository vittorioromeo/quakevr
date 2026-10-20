# Box3D (vendored)

Optional `QvrTracy=true` builds enable upstream `BOX3D_PROFILE` zones. `src/core.h` changes its frame mark to a
named `Box3D` frame set so physics substeps do not duplicate the engine's unnamed frame marks. See `TRACY.md`.

Erin Catto's 3D rigid-body physics engine, the successor to Box2D. MIT licence (`LICENSE`).

- Upstream: https://github.com/erincatto/box3d
- Version: 0.1.0 (alpha) and later work on main (`b3GetVersion` says 0.2.0, the next)
- Commit: `efca0c184d40fd6a82016983f1601c4d23965867` ("Gyro tolerance (#191)"; updated 2026-10-19 from 5d83df83,
  `docs/vr-port/ROUND22.md`, "Box3D updated to upstream efca0c18"). Since #191 a world's `isotropicInertiaTolerance`
  (upstream's default 0.2) leaves the gyroscopic torque out for nearly isotropic bodies; Quake VR's worlds set it to 0,
  so every body but an exactly isotropic one keeps it, as before #191 (`vr_box3d.cpp`, the throw's spin alignment).

Only the library is vendored: `src/` (the C17 sources and their private headers) and `include/box3d/` (the
public C API), unchanged but for local changes marked `QVR` (keep them when updating): the Tracy ones in `src/core.h`
and `src/core.c` (above, and `TRACY.md`), and in `src/mesh_contact.c`
b3ComputeMeshManifolds asks the pre-solve callback for a mesh's contact points too (upstream asks it only in the convex
path, once a contact), so that a prop passing through a teleporter does not collide with the wall behind it
(`vr_box3d.cpp` preSolve, the portal copies). Upstream's samples, tests, benchmarks, docs, shared code and CMake files are left out.

Quake VR uses it in `Quake/vr/vr_box3d.cpp` (see `docs/vr-port/ROUND21.md`, "Box3D
physics"). Build settings (`Windows/VisualStudio/quakevr.props`, `Quake/vr/vr.cmake`):

- compiled as C17 (`/std:c17`), no precompiled header, `/fp:precise` (the engine's) and no FMA contraction
  (Box3D's cross-platform determinism relies on it; MSVC does not contract under `/fp:precise` without
  `/fp:contract`, gcc and clang get `-ffp-contract=off`);
- SIMD: SSE2 on x86-64 (the x64 baseline) and NEON on ARM64, picked by Box3D itself (`src/core.h`), so the build
  stays portable; `BOX3D_DISABLE_SIMD` would select its scalar path. On x86-64 the contact solver's and the hull-hull
  manifolds' wide kernels are compiled twice, from the same `*_wide.inl`: 4 lanes with SSE2 (`src/wide4.c`) and 8
  lanes with AVX2 (`src/wide8.c`); `src/simd.c` checks the CPU (CPUID, and that the OS saves the AVX registers) once,
  and a world uses the AVX2 copy only where it is present, so the exe still runs on a CPU without AVX2. Only
  `wide8.c` is compiled for AVX2: by a pragma around the kernels under clang and gcc (`src/core.h` `B3_AVX2_BEGIN`;
  AVX2 without FMA, so still no contraction), by `/arch:AVX2` on that file alone under MSVC's cl (as upstream's
  CMake). Under clang-cl `wide8.c` gets `/FIintrin.h`: `verstable.h` includes `intrin.h` inside the pragma, where
  clang 19 refuses its declarations ("attribute 'target' multiversioning"; a build fix, the sources unchanged).
  `BOX3D_DISABLE_AVX2` would leave the AVX2 copy out. Quake VR picks with `vr_box3d_avx2` (default 0: the SSE2 copy;
  1: AVX2 where the CPU has it; Debug > Profiling), applied to its world before each step
  (`b3World_EnableSSE2Fallback`); the console says at the first map which one runs (`box3d: Box3D 0.2.0, SSE2
  kernels (4 lanes; vr_box3d_avx2 0)`). Both give the same results to the bit (`vr_physics_mtbench`'s world hash).
  SSE2 by default because, measured in the game, AVX2 was no faster (`docs/vr-port/ROUND22.md`, "Box3D updated");
- threads: the step runs on the engine's job pool (`vr_box3d.cpp`, "Box3D on the pool"): the world is created with
  task callbacks (`enqueueStepTask`, `finishStepTask`) that hand each of Box3D's tasks to a pool worker as a
  `jobs::Task`, or run it on the stepping thread as it waits; Box3D's own scheduler (`src/scheduler.c`) and threads
  are never used. Its worker count (`b3World_SetWorkerCount`, before a step) is `vr_box3d_workers` (default 4, the
  stepping thread one of them) while at least `vr_box3d_threads_bodies` (300) bodies are awake, else 1: the step on
  the stepping thread alone, Box3D's serial path. `vr_box3d_threads 0` (or `vr_jobs_parallel 0`) keeps it at 1.
  The results are the same whatever the count (Box3D is deterministic across threads).

To update: copy upstream's `src/*.c`, `src/*.h`, `src/*.inl`, `include/box3d/*.h` and `LICENSE` over these (removing
files upstream removed or renamed), re-apply the `QVR` changes (diff the vendored `src/` against the old upstream commit to see them all) and change the
commit above. New `.c` files are
picked up by the build's wildcards (`quakevr.props`, `vr.cmake`).
