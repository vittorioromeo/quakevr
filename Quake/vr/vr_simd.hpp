#pragma once

// Preserve SSE/AVX arithmetic on ARM through SIMDe's NEON/vector
// implementations. x86 builds continue to use their original intrinsics.
#if defined(__aarch64__) || defined(__arm__)
#define SIMDE_ENABLE_NATIVE_ALIASES
#include "external/simde/simde/x86/avx.h"
#else
#include <immintrin.h>
#endif
