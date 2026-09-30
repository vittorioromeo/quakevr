#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include "Zancle/Base/Assert.hpp"
#include "Zancle/Base/LifetimeAttributes.hpp"
#include "Zancle/Base/SizeT.hpp"

#include "Zancle/Trait/EnableTrivialRelocation.hpp"
#include "Zancle/Trait/IsSame.hpp"
#include "Zancle/Trait/IsTriviallyRelocatable.hpp"


namespace za
{
////////////////////////////////////////////////////////////
/// \brief Aggregate fixed-size array, lightweight `std::array` replacement
///
/// An aggregate type wrapping a C array of size `N`. Supports brace
/// initialization, deduction guides, and propagates trivial
/// relocatability from the element type. Zero-sized arrays are not
/// allowed.
///
/// All accessors assert in debug builds when given an out-of-bounds index.
///
/// Structured bindings (`auto [a, b] = arr;`) are not supported, as
/// they would require specializing the `std::tuple_size` /
/// `std::tuple_element` protocol (and thus including standard headers).
///
////////////////////////////////////////////////////////////
template <typename T, SizeT N>
struct [[nodiscard]] ZA_GSL_OWNER(T) Array
{
    ////////////////////////////////////////////////////////////
    ZA_ENABLE_TRIVIAL_RELOCATION_IF(ZA_IS_TRIVIALLY_RELOCATABLE(T));


    ////////////////////////////////////////////////////////////
    static_assert(N > 0, "Zero-sized arrays are not supported");


    ////////////////////////////////////////////////////////////
    /// \brief Underlying storage; exposed publicly to remain an aggregate
    ///
    ////////////////////////////////////////////////////////////
    T elements[N];


    ///////////////////////////////////////////////////////////
    /// \brief Number of elements in the array (always `N`)
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::const]] static constexpr SizeT size() noexcept
    {
        return N;
    }


    ///////////////////////////////////////////////////////////
    /// \brief Pointer to the underlying contiguous storage
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] constexpr T* data() noexcept ZA_LIFETIMEBOUND
    {
        return elements;
    }


    ///////////////////////////////////////////////////////////
    /// \brief Pointer to the underlying contiguous storage (const overload)
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] constexpr const T* data() const noexcept ZA_LIFETIMEBOUND
    {
        return elements;
    }


    ///////////////////////////////////////////////////////////
    /// \brief Indexed element access (asserts `i < N`)
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] constexpr T& operator[](const SizeT i) noexcept ZA_LIFETIMEBOUND
    {
        ZA_ASSERT(i < N);
        return elements[i];
    }


    ///////////////////////////////////////////////////////////
    /// \brief Indexed element access (const overload)
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] constexpr const T& operator[](const SizeT i) const noexcept ZA_LIFETIMEBOUND
    {
        ZA_ASSERT(i < N);
        return elements[i];
    }


    ///////////////////////////////////////////////////////////
    /// \brief Iterator to the first element
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] constexpr T* begin() noexcept ZA_LIFETIMEBOUND
    {
        return elements;
    }


    ///////////////////////////////////////////////////////////
    /// \brief Iterator to the first element (const overload)
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] constexpr const T* begin() const noexcept ZA_LIFETIMEBOUND
    {
        return elements;
    }


    ///////////////////////////////////////////////////////////
    /// \brief Iterator one past the last element
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] constexpr T* end() noexcept ZA_LIFETIMEBOUND
    {
        return elements + N;
    }


    ///////////////////////////////////////////////////////////
    /// \brief Iterator one past the last element (const overload)
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] constexpr const T* end() const noexcept ZA_LIFETIMEBOUND
    {
        return elements + N;
    }


    ///////////////////////////////////////////////////////////
    /// \brief Constant iterator to the first element
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] constexpr const T* cbegin() const noexcept ZA_LIFETIMEBOUND
    {
        return elements;
    }


    ///////////////////////////////////////////////////////////
    /// \brief Constant iterator one past the last element
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] constexpr const T* cend() const noexcept ZA_LIFETIMEBOUND
    {
        return elements + N;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Element-wise equality comparison
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] constexpr bool operator==(const Array& rhs) const = default;
};


////////////////////////////////////////////////////////////
/// \brief Deduction guide enabling `Array{a, b, c}` syntax
///
/// All elements must have the same type (like `std::array`), so that
/// e.g. `Array{1.0f, 0.1}` is rejected instead of silently deducing
/// `Array<float, 2>` and narrowing the remaining elements.
///
////////////////////////////////////////////////////////////
template <typename T, typename... Elements>
    requires(za::isSame<T, Elements> && ...)
Array(T, Elements...) -> Array<T, 1 + sizeof...(Elements)>;

} // namespace za
