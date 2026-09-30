#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
/// \brief Macro form of `za::min` for headers that cannot include `MinMax.hpp`
///
/// Beware of multiple-evaluation: arguments may be evaluated more than once.
///
////////////////////////////////////////////////////////////
#define ZA_MIN(a, b) ((b) < (a) ? (b) : (a))


////////////////////////////////////////////////////////////
/// \brief Macro form of `za::max` for headers that cannot include `MinMax.hpp`
///
/// Beware of multiple-evaluation: arguments may be evaluated more than once.
///
////////////////////////////////////////////////////////////
#define ZA_MAX(a, b) ((a) < (b) ? (b) : (a))
