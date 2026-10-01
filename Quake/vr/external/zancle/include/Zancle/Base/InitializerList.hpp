#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


#if defined(__CLANGD__) || defined(_MSC_VER)

    // MSVC's STL lays `std::initializer_list` out as two pointers, unlike the definition below (a pointer
    // and a size, like libstdc++'s and libc++'s). Translation units including the headers in different
    // orders would get both definitions of the same type (an ODR violation): an inline function compiled
    // against one layout, called with the other, reads garbage (e.g. a braced-list constructor sizing an
    // allocation from a pointer). The real header is cheap there, so always use it (also with clang-cl).
    #include <initializer_list> // IWYU pragma: export

#elif !defined(_LIBCPP_INITIALIZER_LIST) && !defined(_INITIALIZER_LIST)

    #define _LIBCPP_INITIALIZER_LIST // libcpp
    #define _INITIALIZER_LIST        // libstdc++

namespace std
{
////////////////////////////////////////////////////////////
/// \brief Lightweight alternative to the standard initializer list
///
////////////////////////////////////////////////////////////
template <typename T>
// NOLINTNEXTLINE(readability-identifier-naming)
class initializer_list
{
public:
    ////////////////////////////////////////////////////////////
    using value_type      = T;
    using reference       = const T&;
    using const_reference = const T&;
    using size_type       = decltype(sizeof(0));
    using iterator        = const T*;
    using const_iterator  = const T*;


    ////////////////////////////////////////////////////////////
    constexpr initializer_list() noexcept = default;


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline]] constexpr initializer_list(const T* b, const size_type s) noexcept :
        m_begin(b),
        m_size(s)
    {
    }

    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] constexpr const T* begin() const noexcept
    {
        return m_begin;
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] constexpr const T* end() const noexcept
    {
        return m_begin + m_size;
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] constexpr size_type size() const noexcept
    {
        return m_size;
    }

private:
    const T*  m_begin = nullptr;
    size_type m_size  = 0u;
};


////////////////////////////////////////////////////////////
template <typename T>
[[nodiscard, gnu::always_inline, gnu::pure]] constexpr const T* begin(initializer_list<T> il) noexcept
{
    return il.begin();
}


////////////////////////////////////////////////////////////
template <typename T>
[[nodiscard, gnu::always_inline, gnu::pure]] constexpr const T* end(initializer_list<T> il) noexcept
{
    return il.end();
}

} // namespace std

#endif


namespace za
{
////////////////////////////////////////////////////////////
/// \brief Alias for `std::initializer_list`
///
/// Being the same type, a constructor taking a `za::InitializerList<T>`
/// is still an initializer-list constructor.
///
////////////////////////////////////////////////////////////
template <typename T>
using InitializerList = std::initializer_list<T>;

} // namespace za


////////////////////////////////////////////////////////////
/// \file
///
/// \brief Provides `std::initializer_list` without including `<initializer_list>`
///
/// `std::initializer_list` is implicitly used by the language for
/// brace-init constructors, so the type itself must live in `std`.
/// This header defines the minimal interface that the compiler expects
/// while pretending to be the real `<initializer_list>` header (via
/// the standard libraries' include guards), with the same layout as
/// libstdc++'s and libc++'s. On the MSVC ABI (`_MSC_VER`, including
/// clang-cl), whose layout differs, it includes the real header instead.
///
////////////////////////////////////////////////////////////
