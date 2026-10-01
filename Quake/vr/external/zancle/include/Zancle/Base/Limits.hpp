#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
// Integer limits
////////////////////////////////////////////////////////////
#if defined(__INT_MAX__) // GCC, Clang (including clang-cl)

    #define ZA_CHAR_BIT        __CHAR_BIT__
    #define ZA_SIGNED_CHAR_MAX __SCHAR_MAX__
    #define ZA_SHORT_MAX       __SHRT_MAX__
    #define ZA_INT_MAX         __INT_MAX__
    #define ZA_LONG_MAX        __LONG_MAX__
    #define ZA_LONG_LONG_MAX   __LONG_LONG_MAX__

    #ifdef __CHAR_UNSIGNED__
        #define ZA_PRIV_CHAR_IS_UNSIGNED
    #endif

#elif defined(_MSC_VER) // Windows data model (LLP64): `long` is 32 bits

    #define ZA_CHAR_BIT        8
    #define ZA_SIGNED_CHAR_MAX 127
    #define ZA_SHORT_MAX       32'767
    #define ZA_INT_MAX         2'147'483'647
    #define ZA_LONG_MAX        2'147'483'647L
    #define ZA_LONG_LONG_MAX   9'223'372'036'854'775'807LL

    #ifdef _CHAR_UNSIGNED // `/J`
        #define ZA_PRIV_CHAR_IS_UNSIGNED
    #endif

#else

    #error "Zancle: unsupported compiler for `Base/Limits.hpp`"

#endif

#define ZA_SIGNED_CHAR_MIN        (-ZA_SIGNED_CHAR_MAX - 1)
#define ZA_UNSIGNED_CHAR_MAX      (ZA_SIGNED_CHAR_MAX * 2 + 1)
#define ZA_SHORT_MIN              (-ZA_SHORT_MAX - 1)
#define ZA_UNSIGNED_SHORT_MAX     (ZA_SHORT_MAX * 2 + 1)
#define ZA_INT_MIN                (-ZA_INT_MAX - 1)
#define ZA_UNSIGNED_INT_MAX       (ZA_INT_MAX * 2U + 1U)
#define ZA_LONG_MIN               (-ZA_LONG_MAX - 1L)
#define ZA_UNSIGNED_LONG_MAX      (ZA_LONG_MAX * 2UL + 1UL)
#define ZA_LONG_LONG_MIN          (-ZA_LONG_LONG_MAX - 1LL)
#define ZA_UNSIGNED_LONG_LONG_MAX (ZA_LONG_LONG_MAX * 2ULL + 1ULL)

#ifdef ZA_PRIV_CHAR_IS_UNSIGNED
    #define ZA_CHAR_MIN 0
    #define ZA_CHAR_MAX ZA_UNSIGNED_CHAR_MAX
#else
    #define ZA_CHAR_MIN ZA_SIGNED_CHAR_MIN
    #define ZA_CHAR_MAX ZA_SIGNED_CHAR_MAX
#endif


////////////////////////////////////////////////////////////
// Floating-point limits
////////////////////////////////////////////////////////////
#if defined(__FLT_MAX__) // GCC, Clang (including clang-cl)

    #define ZA_FLOAT_MAX      __FLT_MAX__
    #define ZA_FLOAT_MIN      __FLT_MIN__
    #define ZA_FLOAT_TRUE_MIN __FLT_DENORM_MIN__
    #define ZA_FLOAT_EPSILON  __FLT_EPSILON__
    #define ZA_FLOAT_MANT_DIG __FLT_MANT_DIG__

    #define ZA_DOUBLE_MAX      __DBL_MAX__
    #define ZA_DOUBLE_MIN      __DBL_MIN__
    #define ZA_DOUBLE_TRUE_MIN __DBL_DENORM_MIN__
    #define ZA_DOUBLE_EPSILON  __DBL_EPSILON__
    #define ZA_DOUBLE_MANT_DIG __DBL_MANT_DIG__

    // Platform-dependent: 64-bit (MSVC ABI, 32-bit ARM), 80-bit (x86), or 128-bit (e.g. AArch64 Linux)
    #define ZA_LONG_DOUBLE_MAX      __LDBL_MAX__
    #define ZA_LONG_DOUBLE_MIN      __LDBL_MIN__
    #define ZA_LONG_DOUBLE_TRUE_MIN __LDBL_DENORM_MIN__
    #define ZA_LONG_DOUBLE_EPSILON  __LDBL_EPSILON__
    #define ZA_LONG_DOUBLE_MANT_DIG __LDBL_MANT_DIG__

#elif defined(_MSC_VER) // IEEE 754 binary32/binary64; `long double` is `double`

    #define ZA_FLOAT_MAX      0x1.fffffep+127F
    #define ZA_FLOAT_MIN      0x1p-126F
    #define ZA_FLOAT_TRUE_MIN 0x1p-149F
    #define ZA_FLOAT_EPSILON  0x1p-23F
    #define ZA_FLOAT_MANT_DIG 24

    #define ZA_DOUBLE_MAX      0x1.fffffffffffffp+1023
    #define ZA_DOUBLE_MIN      0x1p-1022
    #define ZA_DOUBLE_TRUE_MIN 0x1p-1074
    #define ZA_DOUBLE_EPSILON  0x1p-52
    #define ZA_DOUBLE_MANT_DIG 53

    #define ZA_LONG_DOUBLE_MAX      0x1.fffffffffffffp+1023L
    #define ZA_LONG_DOUBLE_MIN      0x1p-1022L
    #define ZA_LONG_DOUBLE_TRUE_MIN 0x1p-1074L
    #define ZA_LONG_DOUBLE_EPSILON  0x1p-52L
    #define ZA_LONG_DOUBLE_MANT_DIG 53

#else

    #error "Zancle: unsupported compiler for `Base/Limits.hpp`"

#endif


////////////////////////////////////////////////////////////
// Infinities and quiet NaNs (constant expressions: GCC, Clang, and MSVC all provide these builtins)
////////////////////////////////////////////////////////////
#define ZA_FLOAT_INFINITY __builtin_huge_valf()
#define ZA_FLOAT_NAN      __builtin_nanf("0")

#define ZA_DOUBLE_INFINITY __builtin_huge_val()
#define ZA_DOUBLE_NAN      __builtin_nan("0")

// Exact conversions from `double` (MSVC has no `__builtin_huge_vall` or `__builtin_nanl`)
#define ZA_LONG_DOUBLE_INFINITY static_cast<long double>(__builtin_huge_val())
#define ZA_LONG_DOUBLE_NAN      static_cast<long double>(__builtin_nan("0"))


////////////////////////////////////////////////////////////
/// \file
///
/// \brief Limits of the fundamental types, without `<climits>`, `<cfloat>`, or `<limits>`
///
/// Each `ZA_<TYPE>_<LIMIT>` macro has the value and the type of the
/// corresponding standard macro (e.g. `ZA_INT_MAX` is `INT_MAX`,
/// `ZA_UNSIGNED_LONG_LONG_MAX` is `ULLONG_MAX`, `ZA_DOUBLE_TRUE_MIN` is
/// `DBL_TRUE_MIN`). Types are spelled out: `SIGNED_CHAR`, `UNSIGNED_CHAR`,
/// `CHAR`, `SHORT`, `UNSIGNED_SHORT`, `INT`, `UNSIGNED_INT`, `LONG`,
/// `UNSIGNED_LONG`, `LONG_LONG`, `UNSIGNED_LONG_LONG`, `FLOAT`, `DOUBLE`,
/// and `LONG_DOUBLE`.
///
/// As in `<cfloat>`, `ZA_FLOAT_MIN` is the smallest *normalized positive*
/// value; the lowest value is `-ZA_FLOAT_MAX`.
///
/// `ZA_<TYPE>_INFINITY` and `ZA_<TYPE>_NAN` (for `FLOAT`, `DOUBLE`, and
/// `LONG_DOUBLE`) are `std::numeric_limits<T>::infinity()` and
/// `std::numeric_limits<T>::quiet_NaN()`.
///
/// Values come from the compiler's predefined macros (GCC, Clang), or
/// from the fixed data model of MSVC on Windows.
///
////////////////////////////////////////////////////////////
