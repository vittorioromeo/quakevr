#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include "Zancle/HasBuiltin.hpp"


#if ZA_HAS_BUILTIN(__is_trivially_assignable) && ZA_HAS_BUILTIN(__add_lvalue_reference) && \
    ZA_HAS_BUILTIN(__add_rvalue_reference)

    ////////////////////////////////////////////////////////////
    #define ZA_IS_TRIVIALLY_MOVE_ASSIGNABLE(...) \
        __is_trivially_assignable(__add_lvalue_reference(__VA_ARGS__), __add_rvalue_reference(__VA_ARGS__))

#else

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
    #include <type_traits>


    ////////////////////////////////////////////////////////////
    #define ZA_IS_TRIVIALLY_MOVE_ASSIGNABLE(...) ::std::is_trivially_move_assignable_v<__VA_ARGS__>

#endif


namespace za
{
////////////////////////////////////////////////////////////
template <typename T>
inline constexpr bool isTriviallyMoveAssignable = ZA_IS_TRIVIALLY_MOVE_ASSIGNABLE(T);

} // namespace za
