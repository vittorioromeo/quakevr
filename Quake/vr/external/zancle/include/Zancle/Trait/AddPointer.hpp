#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


#if __has_builtin(__add_pointer)

    ////////////////////////////////////////////////////////////
    #define ZA_ADD_POINTER(...) __add_pointer(__VA_ARGS__)

#else

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
    #include "Zancle/Trait/VoidT.hpp"


namespace za::priv
{
////////////////////////////////////////////////////////////
template <typename T, typename = void>
struct AddPointerImpl // not pointable (e.g. `void() const`): unchanged
{
    using type = T;
};


////////////////////////////////////////////////////////////
template <typename T>
struct AddPointerImpl<T, VoidT<T*>>
{
    using type = T*;
};


////////////////////////////////////////////////////////////
template <typename T>
struct AddPointerImpl<T&>
{
    using type = T*;
};


////////////////////////////////////////////////////////////
template <typename T>
struct AddPointerImpl<T&&>
{
    using type = T*;
};

} // namespace za::priv

    ////////////////////////////////////////////////////////////
    #define ZA_ADD_POINTER(...) typename ::za::priv::AddPointerImpl<__VA_ARGS__>::type

#endif


namespace za
{
////////////////////////////////////////////////////////////
template <typename T>
using AddPointer = ZA_ADD_POINTER(T);

} // namespace za
