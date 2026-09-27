# Box3D (vendored)

Erin Catto's 3D rigid-body physics engine, the successor to Box2D. MIT licence (`LICENSE`).

- Upstream: https://github.com/erincatto/box3d
- Version: 0.1.0 (alpha)
- Commit: `5643cd81ff07fd0497e3bfcdf04f6425cc8a2e7f` (2026-09-25, "Remove compound mesh child material limit. (#172)")

Only the library is vendored: `src/` (the C17 sources and their private headers) and `include/box3d/` (the
public C API), unchanged. Upstream's samples, tests, benchmarks, docs, shared code and CMake files are left out.

Quake VR uses it in `Quake/vr/vr_box3d.cpp` (`vr_physics_engine 1`; see `docs/vr-port/ROUND21.md`, "Box3D
physics"). Build settings (`Windows/VisualStudio/quakevr.props`, `Quake/vr/vr.cmake`):

- compiled as C17 (`/std:c17`), no precompiled header, `/fp:precise` (the engine's) and no FMA contraction
  (Box3D's cross-platform determinism relies on it; MSVC does not contract under `/fp:precise` without
  `/fp:contract`, gcc and clang get `-ffp-contract=off`);
- SIMD: Box3D picks SSE2 on x86-64 (the x64 baseline) and NEON on ARM64 by itself (`src/core.h`), so the build
  stays portable; `BOX3D_DISABLE_SIMD` would select its scalar path;
- single-threaded: the world is created with `workerCount = 1` and no task callbacks, which Box3D runs on its
  serial path (`b3DefaultAddTaskFcn` in `src/physics_world.c` calls each task inline); no scheduler and no
  threads are ever created (`b3CreateScheduler` is only reached with more than one worker).

To update: copy upstream's `src/*.c`, `src/*.h`, `src/*.inl`, `include/box3d/*.h` and `LICENSE` over these and
change the commit above.
