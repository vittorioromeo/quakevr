#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
/// \file
///
/// \brief Forward declaration of `std::hash` to avoid `<functional>`
///
/// Zancle containers and types only need to befriend or specialize
/// `std::hash`, never to use the rest of `<functional>`. This header
/// declares `std::hash` inside the appropriate (versioned) namespace
/// so that user-defined specializations remain ABI-compatible with
/// libc++/libstdc++.
///
////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////
#ifdef __CLANGD__

    #include <functional> // IWYU pragma: export

#else

    #if __has_include(<__config>)
        #include <__config>

_LIBCPP_BEGIN_NAMESPACE_STD

template <typename>
struct hash;

_LIBCPP_END_NAMESPACE_STD

    #else

namespace std
{

template <typename>
struct hash;

} // namespace std

    #endif

#endif
