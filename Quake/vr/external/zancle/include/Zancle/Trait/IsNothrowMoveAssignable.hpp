#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include "Zancle/HasBuiltin.hpp"


#if ZA_HAS_BUILTIN(__is_nothrow_assignable) && ZA_HAS_BUILTIN(__add_lvalue_reference) && \
    ZA_HAS_BUILTIN(__add_rvalue_reference)

    ////////////////////////////////////////////////////////////
    #define ZA_IS_NOTHROW_MOVE_ASSIGNABLE(...) \
        __is_nothrow_assignable(__add_lvalue_reference(__VA_ARGS__), __add_rvalue_reference(__VA_ARGS__))

#else

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
    #include <type_traits>


    ////////////////////////////////////////////////////////////
    #define ZA_IS_NOTHROW_MOVE_ASSIGNABLE(...) ::std::is_nothrow_move_assignable_v<__VA_ARGS__>

#endif


namespace za
{
////////////////////////////////////////////////////////////
template <typename T>
inline constexpr bool isNoThrowMoveAssignable = ZA_IS_NOTHROW_MOVE_ASSIGNABLE(T);

} // namespace za
