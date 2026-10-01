#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include "Zancle/HasBuiltin.hpp"


#if ZA_HAS_BUILTIN(__make_unsigned)

    ////////////////////////////////////////////////////////////
    #define ZA_MAKE_UNSIGNED(...) __make_unsigned(__VA_ARGS__)

#else

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
    #include "Zancle/Trait/Conditional.hpp"
    #include "Zancle/Trait/IsEnum.hpp"


namespace za::priv
{
////////////////////////////////////////////////////////////
// Unsigned integer type with the smallest rank of at least `unsigned long long`
// having the same `sizeof` as `T`: 16-byte enumerations (e.g. `enum class E : __int128`)
// map to `unsigned __int128`, as with Clang's `__make_unsigned`
    #ifdef __SIZEOF_INT128__
template <typename T>
using UnsignedLongLongOrWider = Conditional<sizeof(T) == sizeof(unsigned long long), unsigned long long, __uint128_t>;
    #else
template <typename T>
using UnsignedLongLongOrWider = unsigned long long;
    #endif


////////////////////////////////////////////////////////////
// Unsigned integer type with the smallest rank having the same `sizeof` as `T`
template <typename T>
using UnsignedOfSameSize = Conditional<
    sizeof(T) == sizeof(unsigned char),
    unsigned char,
    Conditional<sizeof(T) == sizeof(unsigned short),
                unsigned short,
                Conditional<sizeof(T) == sizeof(unsigned int),
                            unsigned int,
                            Conditional<sizeof(T) == sizeof(unsigned long), unsigned long, UnsignedLongLongOrWider<T>>>>>;


////////////////////////////////////////////////////////////
// Primary template handles enumerations (same as the builtin and `std::make_unsigned`)
template <typename T>
struct MakeUnsignedImpl
{
    static_assert(ZA_IS_ENUM(T), "`MakeUnsigned` requires an integral (non-`bool`) or enumeration type");
    using type = UnsignedOfSameSize<T>;
};


////////////////////////////////////////////////////////////
// cv-qualifiers are preserved via partial specializations rather than builtins,
// as GCC rejects builtin type traits in function signatures (e.g. `requires`)
// clang-format off
template <typename T> struct MakeUnsignedImpl<const T>          { using type = const typename MakeUnsignedImpl<T>::type; };
template <typename T> struct MakeUnsignedImpl<volatile T>       { using type = volatile typename MakeUnsignedImpl<T>::type; };
template <typename T> struct MakeUnsignedImpl<const volatile T> { using type = const volatile typename MakeUnsignedImpl<T>::type; };
// clang-format on


////////////////////////////////////////////////////////////
// clang-format off
template <> struct MakeUnsignedImpl<char8_t>            { using type = UnsignedOfSameSize<char8_t>; };
template <> struct MakeUnsignedImpl<char16_t>           { using type = UnsignedOfSameSize<char16_t>; };
template <> struct MakeUnsignedImpl<char32_t>           { using type = UnsignedOfSameSize<char32_t>; };
template <> struct MakeUnsignedImpl<wchar_t>            { using type = UnsignedOfSameSize<wchar_t>; };
template <> struct MakeUnsignedImpl<         char>      { using type = unsigned char; };
template <> struct MakeUnsignedImpl<  signed char>      { using type = unsigned char; };
template <> struct MakeUnsignedImpl<unsigned char>      { using type = unsigned char; };
template <> struct MakeUnsignedImpl<  signed short>     { using type = unsigned short; };
template <> struct MakeUnsignedImpl<unsigned short>     { using type = unsigned short; };
template <> struct MakeUnsignedImpl<  signed int>       { using type = unsigned int; };
template <> struct MakeUnsignedImpl<unsigned int>       { using type = unsigned int; };
template <> struct MakeUnsignedImpl<  signed long>      { using type = unsigned long; };
template <> struct MakeUnsignedImpl<unsigned long>      { using type = unsigned long; };
template <> struct MakeUnsignedImpl<  signed long long> { using type = unsigned long long; };
template <> struct MakeUnsignedImpl<unsigned long long> { using type = unsigned long long; };

    #ifdef __SIZEOF_INT128__
template <> struct MakeUnsignedImpl<__int128_t>         { using type = __uint128_t; };
template <> struct MakeUnsignedImpl<__uint128_t>        { using type = __uint128_t; };
    #endif
// clang-format on

} // namespace za::priv

    ////////////////////////////////////////////////////////////
    #define ZA_MAKE_UNSIGNED(...) typename ::za::priv::MakeUnsignedImpl<__VA_ARGS__>::type

#endif


namespace za
{
////////////////////////////////////////////////////////////
template <typename T>
using MakeUnsigned = ZA_MAKE_UNSIGNED(T);

} // namespace za
