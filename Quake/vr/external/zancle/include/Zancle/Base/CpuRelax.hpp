#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include "Zancle/HasBuiltin.hpp"


#if (defined(__x86_64__) || defined(__i386__)) && ZA_HAS_BUILTIN(__builtin_ia32_pause)

    ////////////////////////////////////////////////////////////
    #define ZA_CPU_RELAX() __builtin_ia32_pause()

#elif (defined(__aarch64__) || defined(__arm__)) && (defined(__GNUC__) || defined(__clang__))

    ////////////////////////////////////////////////////////////
    #define ZA_CPU_RELAX() __asm__ __volatile__("yield" ::: "memory")

#elif defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))

    #include <intrin.h>

    ////////////////////////////////////////////////////////////
    #define ZA_CPU_RELAX() _mm_pause()

#elif defined(_MSC_VER) && (defined(_M_ARM64) || defined(_M_ARM))

    #include <intrin.h>

    ////////////////////////////////////////////////////////////
    #define ZA_CPU_RELAX() __yield()

#else

    ////////////////////////////////////////////////////////////
    #define ZA_CPU_RELAX() ((void)0)

#endif


////////////////////////////////////////////////////////////
/// \file
///
/// \brief Portable spin-wait hint (`pause` on x86, `yield` on ARM)
///
/// `ZA_CPU_RELAX()` tells the CPU that the current thread is busy-waiting,
/// which reduces power usage and frees resources for a sibling hyperthread,
/// and (on x86) avoids a memory-order mis-speculation penalty when the
/// awaited value changes. Expands to a no-op on other architectures.
///
////////////////////////////////////////////////////////////
