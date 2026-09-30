#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include "Zancle/Base/SizeT.hpp"


#if defined(__GCC_DESTRUCTIVE_SIZE) && defined(__GCC_CONSTRUCTIVE_SIZE)

namespace za
{
////////////////////////////////////////////////////////////
enum : SizeT
{
    hardwareDestructiveInterferenceSize  = __GCC_DESTRUCTIVE_SIZE,
    hardwareConstructiveInterferenceSize = __GCC_CONSTRUCTIVE_SIZE,
};

} // namespace za

#elif defined(_M_IX86) || defined(_M_X64) || defined(_M_ARM) || defined(_M_ARM64)

namespace za
{
////////////////////////////////////////////////////////////
enum : SizeT
{
    hardwareDestructiveInterferenceSize  = 64u,
    hardwareConstructiveInterferenceSize = 64u,
};

} // namespace za

#else

    #include <new>

namespace za
{
////////////////////////////////////////////////////////////
enum : SizeT
{
    hardwareDestructiveInterferenceSize  = std::hardware_destructive_interference_size,
    hardwareConstructiveInterferenceSize = std::hardware_constructive_interference_size,
};

} // namespace za

#endif


////////////////////////////////////////////////////////////
/// \file
///
/// \brief Hardware interference size constants without including `<new>`
///
/// Defines `hardwareDestructiveInterferenceSize` and
/// `hardwareConstructiveInterferenceSize` (typically the cache line
/// size on the target architecture). Prefers compiler-defined macros
/// when available, falls back to a fixed `64u` on common architectures,
/// and only as a last resort pulls in `<new>` for the standard values.
///
////////////////////////////////////////////////////////////
