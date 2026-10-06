# mimalloc (vendored)

Microsoft's general-purpose allocator (Daan Leijen, Microsoft Research). MIT licence (`LICENSE`).

- Upstream: https://github.com/microsoft/mimalloc
- Version: v3.5.4 (tag `v3.5.4`, commit `f8401befa675adb13b98decababd7fdc59572477`, 2026-09-30)
- Why v3: upstream's readme names v3 the recommended release ("simplifies the lock-free design of previous versions
  and improves sharing of memory between threads"); v2 is "stable, legacy". The engine's loads free on one thread
  what another allocated (the pool's workers), which v3's design is for.

Only the library is vendored: `src/` (all of it: `src/static.c` includes the other files) and `include/`, unchanged.
Upstream's tests, docs, IDE and CMake files and `bin/` (the prebuilt `mimalloc-redirect.dll` and `minject`) are left
out.

Quake VR uses it as the process's C heap: malloc/free, calloc/realloc, `_aligned_malloc` and the rest, strdup, and
through them operator new/delete (`Quake/vr/vr_alloccount.cpp`'s call malloc/free and `_aligned_malloc`/`_aligned_free`).
How it takes over: `Quake/vr/vr_crtheap.c` (Windows: it defines those functions and their import pointers, so the
executable imports none of them from the C runtime; elsewhere mimalloc's own `MI_MALLOC_OVERRIDE`). See
`docs/vr-port/ROUND21.md`, "mimalloc". Build settings (`Windows/VisualStudio/quakevr.props`, `Quake/vr/vr.cmake`):

- `src/static.c` alone, as C17, no precompiled header, in the executable itself (not an archive);
- `MI_DEBUG` as mimalloc chooses it: 0 with `NDEBUG` (Release), 2 otherwise (Debug: its asserts, double and invalid
  free detection); no other defines (no `MI_OPT_ARCH`: the x64 baseline, as the rest of the engine);
- MSBuild's `QVR_MIMALLOC` (default true on x64; `msbuild -p:QVR_MIMALLOC=false`) and CMake's `QVR_MIMALLOC` (default
  ON except macOS and 32-bit Windows) build without it. At run time on Windows, `-nomimalloc` on the command line or
  `QVR_MIMALLOC=0` in the environment sends every call to the C runtime's heap instead (an A/B in one build).
- mimalloc's own environment options work as upstream documents them (`MIMALLOC_SHOW_STATS=1`, `MIMALLOC_VERBOSE=1`,
  ...). The console: `vr_heap` (`vr_heap stats`, `test`, `stress`, `collect`).

To update: copy upstream's `src/` and `include/` trees and `LICENSE` over these (removing files upstream removed) and
change the version above; then `vr_heap test` and `vr_heap stress` in the game, and check that the executable still
imports no heap function from the C runtime (`llvm-readobj --coff-imports ironwail.exe`: no malloc, free, calloc,
realloc, `_aligned_*` or `_strdup`).
