#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
/// \brief Macro form of `za::clamp` for use in headers that cannot include `Clamp.hpp`
///
/// Beware of multiple-evaluation: `value` is evaluated up to three times,
/// `minValue` and `maxValue` up to twice.
///
////////////////////////////////////////////////////////////
#define ZA_CLAMP(value, minValue, maxValue) \
    ((value) < (minValue) ? (minValue) : ((maxValue) < (value) ? (maxValue) : (value)))
