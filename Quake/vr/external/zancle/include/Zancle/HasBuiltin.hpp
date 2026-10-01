#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
#if defined(__has_builtin) // GCC, Clang (including clang-cl)

    #define ZA_HAS_BUILTIN(builtin) __has_builtin(builtin)

#elif defined(_MSC_VER)

    // MSVC has no `__has_builtin`: list the builtins it provides that Zancle relies on.
    // Unlisted names expand to an undefined identifier, which evaluates to `0` in `#if`.
    #define ZA_HAS_BUILTIN(builtin) ZA_PRIV_MSVC_HAS_BUILTIN_##builtin

    #define ZA_PRIV_MSVC_HAS_BUILTIN___builtin_is_constant_evaluated 1
    #define ZA_PRIV_MSVC_HAS_BUILTIN___builtin_launder               1
    #define ZA_PRIV_MSVC_HAS_BUILTIN___builtin_strlen                1 // usable in constant expressions
    #define ZA_PRIV_MSVC_HAS_BUILTIN___builtin_memcmp                1 // usable in constant expressions
    #define ZA_PRIV_MSVC_HAS_BUILTIN___builtin_offsetof 1 // `offsetof` breaks when nested in macro arguments
    #define ZA_PRIV_MSVC_HAS_BUILTIN___assume           1

#else

    #define ZA_HAS_BUILTIN(builtin) 0

#endif


////////////////////////////////////////////////////////////
/// \file
///
/// \brief Portable `__has_builtin`, usable in `#if`
///
/// `ZA_HAS_BUILTIN(__builtin_xyz)` is `__has_builtin(__builtin_xyz)` on
/// GCC and Clang, which is not portable to use unguarded: MSVC does not
/// define `__has_builtin`, and `defined(__has_builtin) && __has_builtin(x)`
/// is still ill-formed there, as the second operand is parsed regardless.
///
/// On MSVC, only the builtins listed above are reported as available;
/// Zancle's fallbacks cover everything else.
///
////////////////////////////////////////////////////////////
