#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


#if (defined(__x86_64__) || defined(__i386__)) && __has_builtin(__builtin_ia32_pause)

    ////////////////////////////////////////////////////////////
    #define ZA_CPU_RELAX() __builtin_ia32_pause()

#elif (defined(__aarch64__) || defined(__arm__)) && (defined(__GNUC__) || defined(__clang__))

    ////////////////////////////////////////////////////////////
    #define ZA_CPU_RELAX() __asm__ __volatile__("yield" ::: "memory")

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
