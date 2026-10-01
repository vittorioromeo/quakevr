#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


#if __has_builtin(__is_function)

    ////////////////////////////////////////////////////////////
    #define ZA_IS_FUNCTION(...) __is_function(__VA_ARGS__)

#else

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
    #include "Zancle/Trait/IsConst.hpp"
    #include "Zancle/Trait/IsReference.hpp"


    ////////////////////////////////////////////////////////////
    // Only function and reference types cannot be `const`-qualified
    #define ZA_IS_FUNCTION(...) (!ZA_IS_CONST(const __VA_ARGS__) && !ZA_IS_REFERENCE(__VA_ARGS__))

#endif


namespace za
{
////////////////////////////////////////////////////////////
template <typename T>
inline constexpr bool isFunction = ZA_IS_FUNCTION(T);

} // namespace za
