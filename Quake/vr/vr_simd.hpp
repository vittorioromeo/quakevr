#pragma once

// The SSE/AVX intrinsics of the engine's vector kernels (the AO bake's rays, the audio mixer): the compiler's own on x86;
// on ARM, SIMDe's versions of the same calls (external/simde/README.md), NEON or plain vector code underneath. The kernels
// keep one source, and their arithmetic stays in the same order (vr.cmake: no fused multiply-add for their files).
#if defined(__aarch64__) || defined(__arm__)
#define SIMDE_ENABLE_NATIVE_ALIASES
#include "external/simde/simde/x86/avx.h"
#else
#include <immintrin.h>
#endif
