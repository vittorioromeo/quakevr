#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


#if __has_builtin(__is_trivially_copyable)

    ////////////////////////////////////////////////////////////
    #define ZA_IS_TRIVIALLY_COPYABLE(...) __is_trivially_copyable(__VA_ARGS__)

#else

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
    #include <type_traits>


    ////////////////////////////////////////////////////////////
    #define ZA_IS_TRIVIALLY_COPYABLE(...) ::std::is_trivially_copyable_v<__VA_ARGS__>

#endif


namespace za
{
////////////////////////////////////////////////////////////
template <typename T>
inline constexpr bool isTriviallyCopyable = ZA_IS_TRIVIALLY_COPYABLE(T);

} // namespace za
