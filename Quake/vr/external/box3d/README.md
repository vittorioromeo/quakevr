# Box3D (vendored)

Optional `QvrTracy=true` builds enable upstream `BOX3D_PROFILE` zones. `src/core.h` changes its frame mark to a
named `Box3D` frame set so physics substeps do not duplicate the engine's unnamed frame marks. See `TRACY.md`.

Erin Catto's 3D rigid-body physics engine, the successor to Box2D. MIT licence (`LICENSE`).

- Upstream: https://github.com/erincatto/box3d
- Version: 0.1.0 (alpha) and later work on main (`b3GetVersion` says 0.2.0, the next)
- Commit: `042be585b58d7eebcafecb287053d2448c116b06` ("Vehicle tech (#241)"; updated 2026-11-26 from 1767150d,
  `docs/vr-port/ROUND26.md`, "box3dup3"; from efca0c18 on 2026-11-20, `docs/vr-port/ROUND25.md`, "box3dup"; from
  5d83df83 on 2026-10-19, `docs/vr-port/ROUND22.md`). Since #236 the body's motion locks are gone: a body's
  `fixedRotation` locks every turn, a motion lock joint the rest (a pushable corpse turning about its yaw alone is held
  by one to the world's `anchor` body, `vr_box3d_props.cpp`). Since #237 a spherical joint's motor velocity is in joint
  frame A, no longer the world's (`vr_ragactive.cpp`'s drive). Since #238 b3CreateHull fails (NULL) on a hull thinner
  than the larger of `B3_LINEAR_SLOP` and 0.1% of its extent; its hulls are otherwise the same bytes (the gun pieces'
  cache compared the same). Since #191 a world's
  `isotropicInertiaTolerance` (upstream's default 0.2) leaves the gyroscopic torque out for nearly isotropic bodies;
  Quake VR's worlds set it to 0, so every body but an exactly isotropic one keeps it, as before #191 (`vr_box3d.cpp`,
  the throw's spin alignment). Since #228 the enqueue callback gets a `b3TaskType` instead of a name
  (`enqueueStepTask`); Quake VR keeps the push model (its enqueue and finish callbacks), not the new pull model. Since
  #212 mesh contacts handle ghost collisions fully for meshes built with `identifyEdges` (the world mesh is:
  `vr_box3d_mesh.cpp`); since #227 a contact keeps at most `B3_MAX_MANIFOLDS` (8) manifolds.

Only the library is vendored: `src/` (the C17 sources and their private headers) and `include/box3d/` (the
public C API), unchanged but for local changes marked `QVR` or `Quake VR` (keep them when updating; diff the vendored
files against the upstream commit above to see them all). Upstream's samples, tests, benchmarks, docs, shared code and
CMake files are left out. The changes:

- Tracy (`src/core.h`, `src/core.c`): the frame mark above, and the default pool's allocations through `vr_tracy.h`
  (replayed on an on-demand reconnect; `TRACY.md`).
- Mesh contacts and the pre-solve callback (`src/mesh_contact.c`): b3CollideMeshTriangles (called by
  b3ComputeMeshManifolds, which passes it the world and shape B's transform; NULL from b3CollideMeshAndHull and the
  like) asks the pre-solve callback for a mesh's contact points too, point by point (upstream asks it only in the
  convex path, once a contact), so that a prop passing through a teleporter does not collide with the wall behind it
  (`vr_box3d.cpp` preSolve, the portal copies).
- The pre-solve callback's `separation` (`include/box3d/types.h` b3PreSolveFcn, `src/contact.c`, `src/solver.c`,
  `src/mesh_contact.c`): the contact's least separation (0 from continuous collision): the hands pressing on props
  (`notePress`).
- Elliptical cones (`include/box3d/types.h` b3SphericalJointDef `coneAngleY`, `src/joint.h`, `src/joint.c`,
  `src/spherical_joint.c`, `src/recording.c`, `src/recording.h` (recording major version 8: upstream took 7 for its
  motion lock joints in #236), `src/replay.c`): a
  spherical joint's cone half angle toward frame A's y axis, `coneAngle` then toward x (the authored rig's balls,
  `vr_rig2.cpp`).
- b3CreateHullWith (`src/hull.c`, `include/box3d/collision.h`): b3CreateHull with its working memory and result from
  the caller's allocator (b3CreateHull calls the same code with b3Alloc / b3Free), so that the disarm hulls made each
  frame use a scratch arena (`vr_disarmhull.cpp`).
- Long manifold arrays (`src/physics_world.h`, `src/physics_world.c`, `include/box3d/box3d.h`): arrays longer than
  `B3_POOLED_MANIFOLD_COUNT` (4) come from b3Alloc instead of a block allocator of their own count (upstream makes one
  pool for every count up to the longest a contact had, each 512 arrays of its count, kept until the world goes; #227
  capped the count at 8, so the pools' worst is now bounded, and #236 made the 8 allocators up front, empty, but the
  change stays: a block of 512 eight-manifold arrays for one long contact), freed at the world's end, counted in
  `b3World_DumpMemoryStats`, and `b3World_GetManifoldMemory` (`vr_physics_mem`, its pools those holding blocks;
  `docs/vr-port/MP_PLAN.md` B21).
- b3Body_GetAllContactData (`src/body.c`, `include/box3d/box3d.h`): b3Body_GetContactData's every contact, touching or
  not (`vr_physics_frametime`'s split of the ragdolls' contacts: ROUND25.md, "ragperf2").
- b3World_EnableMeshGhostHandling (`src/physics_world.h`, `src/physics_world.c`, `src/mesh_contact.c`,
  `include/box3d/box3d.h`): #212's ghost collision handling for mesh contacts on (upstream's default) or off (as
  `B3_FORCE_GHOST_COLLISIONS` 1) per world, at run time. Quake VR sets it before each step from `vr_box3d_mesh_ghosts`
  (default 1, on: upstream's handling; off, the active-ragdoll test's crawling ogre kept 76 J after its drive). The
  climbs' drop with it on (17 of 24 against 21 before the update) was the climb's own "not hanging" judgement, not the
  contacts: fixed in `vr_ragactive_hold.cpp`, 21 of 24 on (ROUND25.md, "box3dup" and "box3dup2").

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
files upstream removed or renamed), re-apply the `QVR` changes (diff the vendored `src/` and `include/` against the old upstream commit to see them all;
`git merge-file -p --diff3 <new upstream file> <old upstream file> <vendored file>` re-applies a file's, conflicts
marked) and change the commit above. New `.c` files are
picked up by the build's wildcards (`quakevr.props`, `vr.cmake`).
