#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include "Zancle/HasBuiltin.hpp"


////////////////////////////////////////////////////////////
/// \file
///
/// \brief `std::remove_cvref` replacement
///
/// `RemoveCVRef<T>` strips both references and top-level cv-qualifiers
/// from `T`. The macro form `ZA_REMOVE_CVREF(T)` prefers the
/// compiler builtin when available; the type alias forwards to it.
///
/// `RemoveCVRefIndirect<T>` is the same trait expressed without the
/// builtin, which is required when the type appears in a function
/// signature, including `requires` constraints: GCC rejects builtin
/// type traits there ("use of built-in trait in function signature").
///
////////////////////////////////////////////////////////////


#if ZA_HAS_BUILTIN(__remove_cvref)

    ////////////////////////////////////////////////////////////
    #define ZA_REMOVE_CVREF(...) __remove_cvref(__VA_ARGS__)

namespace za::priv
{
////////////////////////////////////////////////////////////
template <typename T>
struct RemoveCVRefImpl
{
    using type = ZA_REMOVE_CVREF(T);
};

} // namespace za::priv

#else

namespace za::priv
{
////////////////////////////////////////////////////////////
// clang-format off
template <typename T> struct RemoveCVRefImpl                     { using type = T; };
template <typename T> struct RemoveCVRefImpl<T&>                 { using type = T; };
template <typename T> struct RemoveCVRefImpl<T&&>                { using type = T; };
template <typename T> struct RemoveCVRefImpl<const T>            { using type = T; };
template <typename T> struct RemoveCVRefImpl<const T&>           { using type = T; };
template <typename T> struct RemoveCVRefImpl<const T&&>          { using type = T; };
template <typename T> struct RemoveCVRefImpl<volatile T>         { using type = T; };
template <typename T> struct RemoveCVRefImpl<volatile T&>        { using type = T; };
template <typename T> struct RemoveCVRefImpl<volatile T&&>       { using type = T; };
template <typename T> struct RemoveCVRefImpl<const volatile T>   { using type = T; };
template <typename T> struct RemoveCVRefImpl<const volatile T&>  { using type = T; };
template <typename T> struct RemoveCVRefImpl<const volatile T&&> { using type = T; };
// clang-format on

} // namespace za::priv

    ////////////////////////////////////////////////////////////
    #define ZA_REMOVE_CVREF(...) typename ::za::priv::RemoveCVRefImpl<__VA_ARGS__>::type

#endif


namespace za
{
////////////////////////////////////////////////////////////
template <typename T>
using RemoveCVRef = ZA_REMOVE_CVREF(T);

////////////////////////////////////////////////////////////
template <typename T>
using RemoveCVRefIndirect = typename priv::RemoveCVRefImpl<T>::type; // no builtin, see file docs

} // namespace za
