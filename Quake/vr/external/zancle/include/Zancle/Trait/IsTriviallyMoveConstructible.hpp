#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


#if __has_builtin(__is_trivially_constructible) && __has_builtin(__add_rvalue_reference)

    ////////////////////////////////////////////////////////////
    #define ZA_IS_TRIVIALLY_MOVE_CONSTRUCTIBLE(...) \
        __is_trivially_constructible(__VA_ARGS__, __add_rvalue_reference(__VA_ARGS__))

#else

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
    #include <type_traits>


    ////////////////////////////////////////////////////////////
    #define ZA_IS_TRIVIALLY_MOVE_CONSTRUCTIBLE(...) ::std::is_trivially_move_constructible_v<__VA_ARGS__>

#endif


namespace za
{
////////////////////////////////////////////////////////////
template <typename T>
inline constexpr bool isTriviallyMoveConstructible = ZA_IS_TRIVIALLY_MOVE_CONSTRUCTIBLE(T);

} // namespace za
