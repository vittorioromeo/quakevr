#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include "Zancle/Trait/DeclVal.hpp"
#include "Zancle/Trait/IsSame.hpp"
#include "Zancle/Trait/IsTriviallyCopyable.hpp"
#include "Zancle/Trait/RemoveCV.hpp"


#if __has_builtin(__builtin_is_cpp_trivially_relocatable)

    ////////////////////////////////////////////////////////////
    #define ZA_IS_TRIVIALLY_RELOCATABLE_BY_BUILTIN(...) __builtin_is_cpp_trivially_relocatable(__VA_ARGS__)

#elif __has_builtin(__is_trivially_relocatable)

    ////////////////////////////////////////////////////////////
    #define ZA_IS_TRIVIALLY_RELOCATABLE_BY_BUILTIN(...) __is_trivially_relocatable(__VA_ARGS__)

#else

    ////////////////////////////////////////////////////////////
    #define ZA_IS_TRIVIALLY_RELOCATABLE_BY_BUILTIN(...) false

#endif


namespace za
{
////////////////////////////////////////////////////////////
/// \brief Opt-in customization point for trivial relocation
///
/// `true` if `T` itself (not one of its bases) opted in via
/// `ZA_ENABLE_TRIVIAL_RELOCATION` or `ZA_ENABLE_TRIVIAL_RELOCATION_IF`
/// with a `true` predicate. Checking that `zaPrivTriviallyRelocatableSelf`
/// returns a pointer to `T` itself prevents derived classes (which
/// inherit the members declared by the macros) from silently inheriting
/// the opt-in. Can also be explicitly specialized.
///
////////////////////////////////////////////////////////////
template <typename T>
inline constexpr bool enableTrivialRelocation = requires {
    requires static_cast<bool>(T::enableTrivialRelocation);
    requires ZA_IS_SAME(decltype(declVal<const ZA_REMOVE_CV(T) &>().zaPrivTriviallyRelocatableSelf()),
                        const ZA_REMOVE_CV(T)*);
};

} // namespace za


////////////////////////////////////////////////////////////
#define ZA_IS_TRIVIALLY_RELOCATABLE(...)                                                             \
    (ZA_IS_TRIVIALLY_RELOCATABLE_BY_BUILTIN(__VA_ARGS__) || ZA_IS_TRIVIALLY_COPYABLE(__VA_ARGS__) || \
     ::za::enableTrivialRelocation<ZA_REMOVE_CV(__VA_ARGS__)>) // cv-stripped, so explicit specializations apply to `const T`


namespace za
{
////////////////////////////////////////////////////////////
template <typename T>
inline constexpr bool isTriviallyRelocatable = ZA_IS_TRIVIALLY_RELOCATABLE(T);

} // namespace za


////////////////////////////////////////////////////////////
/// \file
///
/// \brief Detection of trivially relocatable types
///
/// Trivial relocation is the property that a type's value can be
/// transferred between two storage locations by `memcpy` followed by
/// not running its destructor on the source. Zancle containers
/// (`Vector`, `SmallVector`, `InPlaceVector`, etc.) use this property
/// to skip per-element move-and-destroy loops on grow/relocate.
///
/// A type `T` is considered trivially relocatable when any of the
/// following holds:
/// - the compiler reports it as such via a builtin
///   (`__builtin_is_cpp_trivially_relocatable` or
///   `__is_trivially_relocatable`)
/// - it is trivially copyable
/// - it opts in via the `ZA_ENABLE_TRIVIAL_RELOCATION` /
///   `ZA_ENABLE_TRIVIAL_RELOCATION_IF(...)` macros (or by specializing
///   `za::enableTrivialRelocation<T>`)
///
/// User code can opt in via the `ZA_ENABLE_TRIVIAL_RELOCATION` /
/// `ZA_ENABLE_TRIVIAL_RELOCATION_IF(...)` macros (in
/// `Zancle/Trait/EnableTrivialRelocation.hpp`), e.g. when the class
/// manages a heap buffer in a way that survives a `memcpy` (`Vector`
/// itself is the canonical example), or to propagate relocatability
/// from an element type.
///
/// The opt-in is intentionally *not* inherited: a class deriving from
/// an opted-in class must opt in again, as it may add members (e.g.
/// self-pointers) that do not survive a `memcpy`. Likewise, it is not
/// propagated to classes that merely contain opted-in members.
///
////////////////////////////////////////////////////////////
