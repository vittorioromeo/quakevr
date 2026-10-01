#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include "Zancle/HasBuiltin.hpp"


#if ZA_HAS_BUILTIN(__remove_reference_t) // Clang spelling

    ////////////////////////////////////////////////////////////
    #define ZA_REMOVE_REFERENCE(...) __remove_reference_t(__VA_ARGS__)

#elif ZA_HAS_BUILTIN(__remove_reference) // GCC spelling

    ////////////////////////////////////////////////////////////
    #define ZA_REMOVE_REFERENCE(...) __remove_reference(__VA_ARGS__)

#else

namespace za::priv
{
////////////////////////////////////////////////////////////
// clang-format off
template <typename T> struct RemoveReferenceImpl      { using type = T; };
template <typename T> struct RemoveReferenceImpl<T&>  { using type = T; };
template <typename T> struct RemoveReferenceImpl<T&&> { using type = T; };
// clang-format on

} // namespace za::priv

    ////////////////////////////////////////////////////////////
    #define ZA_REMOVE_REFERENCE(...) typename ::za::priv::RemoveReferenceImpl<__VA_ARGS__>::type

#endif


namespace za
{
////////////////////////////////////////////////////////////
template <typename T>
using RemoveReference = ZA_REMOVE_REFERENCE(T);

} // namespace za
