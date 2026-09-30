#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


#if __has_builtin(__reference_converts_from_temporary)

    ////////////////////////////////////////////////////////////
    #define ZA_REFERENCE_CONVERTS_FROM_TEMPORARY(T, U) __reference_converts_from_temporary(T, U)

#else

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
    #include <type_traits>


    ////////////////////////////////////////////////////////////
    #define ZA_REFERENCE_CONVERTS_FROM_TEMPORARY(T, U) ::std::reference_converts_from_temporary_v<T, U>

#endif


namespace za
{
////////////////////////////////////////////////////////////
/// \brief `true` if `T` is a reference type, and converting an expression of
///        type `U` to it would bind the reference to a temporary object
///
/// Same as `std::reference_converts_from_temporary_v<T, U>`: a non-reference
/// `U` denotes a prvalue (e.g. `T = const int&`, `U = int` is `true`, whereas
/// `U = int&` is `false`).
///
////////////////////////////////////////////////////////////
template <typename T, typename U>
inline constexpr bool referenceConvertsFromTemporary = ZA_REFERENCE_CONVERTS_FROM_TEMPORARY(T, U);

} // namespace za
