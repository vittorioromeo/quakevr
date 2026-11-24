#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include "Zancle/HasBuiltin.hpp"


////////////////////////////////////////////////////////////
#if !ZA_HAS_BUILTIN(__builtin_source_location)
    #error "Compiler does not support __builtin_source_location, which is required by za::SourceLocation"
#endif


////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include "Zancle/Base/AssertAndAssume.hpp"


#ifdef __CLANGD__

    #include <source_location> // IWYU pragma: export

#elif !defined(_GLIBCXX_SRCLOC) && !defined(_LIBCPP_SOURCE_LOCATION) && !defined(_SOURCE_LOCATION_)

    #define _GLIBCXX_SRCLOC         1 // libstdc++
    #define _LIBCPP_SOURCE_LOCATION   // libc++
    #define _SOURCE_LOCATION_         // msstl

    #if __has_include(<__config>)
        #include <__config>

        #define ZA_PRIV_BEGIN_NAMESPACE_STD _LIBCPP_BEGIN_NAMESPACE_STD
        #define ZA_PRIV_END_NAMESPACE_STD   _LIBCPP_END_NAMESPACE_STD
    #else
        #define ZA_PRIV_BEGIN_NAMESPACE_STD \
            namespace std                   \
            {
        #define ZA_PRIV_END_NAMESPACE_STD }
    #endif


// NOLINTBEGIN(readability-identifier-naming, bugprone-reserved-identifier)

ZA_PRIV_BEGIN_NAMESPACE_STD

////////////////////////////////////////////////////////////
/// \brief Lightweight alternative to the standard `std::source_location`
///
/// `__builtin_source_location` requires `std::source_location::__impl`
/// to exist with this exact layout and member names, which are
/// hard-coded in the compiler. This header defines a minimal but
/// complete `std::source_location` while pretending to be the real
/// `<source_location>` header (via the standard libraries' include
/// guards), so that both can coexist in the same translation unit.
/// The layout matches libstdc++ and libc++ (a single `__impl` pointer).
///
////////////////////////////////////////////////////////////
struct source_location
{
private:
    ////////////////////////////////////////////////////////////
    struct __impl
    {
        const char*  _M_file_name;
        const char*  _M_function_name;
        unsigned int _M_line;
        unsigned int _M_column;
    };

    ////////////////////////////////////////////////////////////
    using __builtin_ret_type = decltype(__builtin_source_location());

public:
    ////////////////////////////////////////////////////////////
    [[nodiscard]] static consteval source_location current(__builtin_ret_type ptr = __builtin_source_location()) noexcept
    {
        source_location result;
        result._M_impl = ptr;
        return result;
    }

    ////////////////////////////////////////////////////////////
    constexpr source_location() noexcept = default;

    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] constexpr unsigned int line() const noexcept
    {
        return _M_impl ? _M_impl->_M_line : 0u;
    }

    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] constexpr unsigned int column() const noexcept
    {
        return _M_impl ? _M_impl->_M_column : 0u;
    }

    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] constexpr const char* file_name() const noexcept
    {
        return _M_impl ? _M_impl->_M_file_name : "";
    }

    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] constexpr const char* function_name() const noexcept
    {
        return _M_impl ? _M_impl->_M_function_name : "";
    }

private:
    __builtin_ret_type _M_impl = nullptr;
};

ZA_PRIV_END_NAMESPACE_STD

// NOLINTEND(readability-identifier-naming, bugprone-reserved-identifier)

    #undef ZA_PRIV_BEGIN_NAMESPACE_STD
    #undef ZA_PRIV_END_NAMESPACE_STD

#endif


namespace za
{
////////////////////////////////////////////////////////////
/// \brief Lightweight replacement for `std::source_location`
///
/// Captures the file name, function name, line number, and column
/// at the call site through the `__builtin_source_location` compiler
/// builtin. Avoids the cost of including the `<source_location>`
/// standard header.
///
/// Use `SourceLocation::current()` as a default function argument to
/// capture the caller's location automatically.
///
////////////////////////////////////////////////////////////
struct [[nodiscard]] SourceLocation
{
    using BuiltinTypePtr = decltype(__builtin_source_location());
    BuiltinTypePtr ptr   = nullptr;


    ////////////////////////////////////////////////////////////
    /// \brief Capture the call-site source location (use as a default argument)
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline]] static consteval SourceLocation current(
        BuiltinTypePtr ptr = __builtin_source_location()) noexcept
    {
        return {ptr};
    }


    ////////////////////////////////////////////////////////////
    /// \brief Captured line number
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline]] constexpr unsigned int line() const noexcept
    {
        ZA_ASSERT_AND_ASSUME(ptr != nullptr);
        return ptr->_M_line;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Captured column number
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline]] constexpr unsigned int column() const noexcept
    {
        ZA_ASSERT_AND_ASSUME(ptr != nullptr);
        return ptr->_M_column;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Captured null-terminated file path
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline]] constexpr const char* fileName() const noexcept
    {
        ZA_ASSERT_AND_ASSUME(ptr != nullptr);
        return ptr->_M_file_name;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Captured null-terminated enclosing function name
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline]] constexpr const char* functionName() const noexcept
    {
        ZA_ASSERT_AND_ASSUME(ptr != nullptr);
        return ptr->_M_function_name;
    }
};

} // namespace za
