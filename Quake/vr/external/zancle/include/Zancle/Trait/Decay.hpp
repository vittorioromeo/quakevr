#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include "Zancle/HasBuiltin.hpp"


// Clang 19 and GCC 15 report this builtin through __has_builtin, but reject
// invoking it as a type trait in a function signature. Use the portable
// implementation there until their builtin form is supported in this context.
// (Not clang-cl: Quake VR's Windows build takes the builtin as before.)
#if ZA_HAS_BUILTIN(__decay) && !((defined(__clang__) && !defined(_MSC_VER) && __clang_major__ >= 19) || \
    (defined(__GNUC__) && !defined(__clang__) && __GNUC__ >= 15))

    ////////////////////////////////////////////////////////////
    #define ZA_DECAY(...) __decay(__VA_ARGS__)

#else

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
    #include "Zancle/Trait/AddPointer.hpp"
    #include "Zancle/Trait/Conditional.hpp"
    #include "Zancle/Trait/IsConst.hpp"
    #include "Zancle/Trait/RemoveCV.hpp"


namespace za::priv
{
////////////////////////////////////////////////////////////
template <typename U>
struct DecayNonRefImpl
{
    using type = Conditional<ZA_IS_CONST(const U), ZA_REMOVE_CV(U), ZA_ADD_POINTER(U)>;
};


////////////////////////////////////////////////////////////
template <typename U, auto N>
struct DecayNonRefImpl<U[N]>
{
    using type = U*;
};


////////////////////////////////////////////////////////////
template <typename U>
struct DecayNonRefImpl<U[]>
{
    using type = U*;
};


////////////////////////////////////////////////////////////
template <typename T>
struct DecayImpl
{
    using type = typename DecayNonRefImpl<T>::type;
};


////////////////////////////////////////////////////////////
template <typename T>
struct DecayImpl<T&>
{
    using type = typename DecayNonRefImpl<T>::type;
};


////////////////////////////////////////////////////////////
template <typename T>
struct DecayImpl<T&&>
{
    using type = typename DecayNonRefImpl<T>::type;
};

} // namespace za::priv

    ////////////////////////////////////////////////////////////
    #define ZA_DECAY(...) typename ::za::priv::DecayImpl<__VA_ARGS__>::type

#endif


namespace za
{
////////////////////////////////////////////////////////////
template <typename T>
using Decay = ZA_DECAY(T);

} // namespace za
