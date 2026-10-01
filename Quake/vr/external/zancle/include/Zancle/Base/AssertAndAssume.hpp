#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include "Zancle/Base/Assert.hpp"
#include "Zancle/Base/Assume.hpp"


////////////////////////////////////////////////////////////
/// \brief Assert a condition in debug mode and let the optimizer assume it
///
/// Expands to a single statement so that the assumption can never escape
/// an enclosing unbraced `if`/`for`/`while` body.
///
////////////////////////////////////////////////////////////
#define ZA_ASSERT_AND_ASSUME(...) \
    do                            \
    {                             \
        ZA_ASSERT(__VA_ARGS__);   \
        ZA_ASSUME(__VA_ARGS__);   \
    } while (false)
