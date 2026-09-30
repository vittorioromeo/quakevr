#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


#if __has_builtin(__remove_cv)

    ////////////////////////////////////////////////////////////
    #define ZA_REMOVE_CV(...) __remove_cv(__VA_ARGS__)

#else

namespace za::priv
{
////////////////////////////////////////////////////////////
// clang-format off
template <typename T> struct RemoveCVImpl                   { using type = T; };
template <typename T> struct RemoveCVImpl<const T>          { using type = T; };
template <typename T> struct RemoveCVImpl<volatile T>       { using type = T; };
template <typename T> struct RemoveCVImpl<const volatile T> { using type = T; };
// clang-format on

} // namespace za::priv

    ////////////////////////////////////////////////////////////
    #define ZA_REMOVE_CV(...) typename ::za::priv::RemoveCVImpl<__VA_ARGS__>::type

#endif


namespace za
{
////////////////////////////////////////////////////////////
template <typename T>
using RemoveCV = ZA_REMOVE_CV(T);

} // namespace za
