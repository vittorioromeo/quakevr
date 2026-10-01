#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
#if __has_cpp_attribute(assume) // GCC, Clang (including clang-cl)

    #define ZA_ASSUME(...) [[assume(__VA_ARGS__)]]

#elif defined(_MSC_VER) // no `[[assume]]` as of MSVC 19.44

    #define ZA_ASSUME(...) __assume(__VA_ARGS__)

#else

    #define ZA_ASSUME(...) static_cast<void>(0)

#endif


////////////////////////////////////////////////////////////
/// \file
///
/// \brief `ZA_ASSUME(expr);`: let the optimizer assume that `expr` is `true`
///
/// Portable `[[assume(expr)]]`: behavior is undefined if `expr` is
/// `false`, and `expr` is never evaluated. Use as a statement. Prefer
/// `ZA_ASSERT_AND_ASSUME` (`Zancle/Base/AssertAndAssume.hpp`), which
/// also checks the assumption in debug builds.
///
////////////////////////////////////////////////////////////
