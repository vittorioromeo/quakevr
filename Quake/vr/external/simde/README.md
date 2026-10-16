# SIMDe (vendored)

SIMD Everywhere: header-only portable versions of the x86 SSE/AVX intrinsics (Evan Nemerson and contributors). MIT
licence (`COPYING`).

- Upstream: https://github.com/simd-everywhere/simde
- Version: v0.8.2 (tag `v0.8.2`)

Only the `simde/` headers and `COPYING` are vendored, unchanged. Brought in by Kasper Olesen (kblood) for the ARM64 Linux
build (PR #75, the Steam Frame).

Quake VR uses it on ARM only, through `Quake/vr/vr_simd.hpp`: with `SIMDE_ENABLE_NATIVE_ALIASES` the kernels' `_mm_*` and
`_mm256_*` calls (`vr_ao.cpp`, `vr_audio.cpp`) compile to SIMDe's NEON or plain vector code. x86 builds include
`<immintrin.h>` instead and never read these headers.

To update: copy upstream's `simde/` tree and `COPYING` over these (removing files upstream removed).
