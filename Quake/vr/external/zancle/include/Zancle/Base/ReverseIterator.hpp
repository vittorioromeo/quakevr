#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include "Zancle/Base/PtrDiffT.hpp"
#include "Zancle/Base/SizeT.hpp"

#include "Zancle/Trait/RemoveCV.hpp"
#include "Zancle/Trait/RemoveReference.hpp"


namespace za
{
////////////////////////////////////////////////////////////
/// \brief Random-access iterator over a contiguous range of `T`, from its last element to its first
///
/// Like `std::reverse_iterator<T*>`: it holds a pointer one past the element
/// it refers to (`base()`), so that `rend()` is the range's first element.
///
////////////////////////////////////////////////////////////
template <typename T>
class [[nodiscard]] ReverseIterator
{
public:
    using value_type      = RemoveCV<T>;
    using difference_type = PtrDiffT;
    using pointer         = T*;
    using reference       = T&;

    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline]] constexpr ReverseIterator() noexcept = default;

    ////////////////////////////////////////////////////////////
    /// \brief Iterator to the element before `base`
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline]] constexpr explicit ReverseIterator(T* const base) noexcept : m_base{base}
    {
    }

    ////////////////////////////////////////////////////////////
    /// \brief Pointer one past the element this iterator refers to
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] constexpr T* base() const noexcept
    {
        return m_base;
    }

    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] constexpr T& operator*() const noexcept
    {
        return *(m_base - 1);
    }

    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] constexpr T* operator->() const noexcept
    {
        return m_base - 1;
    }

    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] constexpr T& operator[](const PtrDiffT n) const noexcept
    {
        return *(m_base - 1 - n);
    }

    ////////////////////////////////////////////////////////////
    [[gnu::always_inline]] constexpr ReverseIterator& operator++() noexcept
    {
        --m_base;
        return *this;
    }

    ////////////////////////////////////////////////////////////
    [[gnu::always_inline]] constexpr ReverseIterator operator++(int) noexcept
    {
        const ReverseIterator previous = *this;
        --m_base;
        return previous;
    }

    ////////////////////////////////////////////////////////////
    [[gnu::always_inline]] constexpr ReverseIterator& operator--() noexcept
    {
        ++m_base;
        return *this;
    }

    ////////////////////////////////////////////////////////////
    [[gnu::always_inline]] constexpr ReverseIterator operator--(int) noexcept
    {
        const ReverseIterator previous = *this;
        ++m_base;
        return previous;
    }

    ////////////////////////////////////////////////////////////
    [[gnu::always_inline]] constexpr ReverseIterator& operator+=(const PtrDiffT n) noexcept
    {
        m_base -= n;
        return *this;
    }

    ////////////////////////////////////////////////////////////
    [[gnu::always_inline]] constexpr ReverseIterator& operator-=(const PtrDiffT n) noexcept
    {
        m_base += n;
        return *this;
    }

    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] friend constexpr ReverseIterator operator+(const ReverseIterator it,
                                                                                            const PtrDiffT n) noexcept
    {
        return ReverseIterator{it.m_base - n};
    }

    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] friend constexpr ReverseIterator operator+(const PtrDiffT n,
                                                                                            const ReverseIterator it) noexcept
    {
        return ReverseIterator{it.m_base - n};
    }

    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] friend constexpr ReverseIterator operator-(const ReverseIterator it,
                                                                                            const PtrDiffT n) noexcept
    {
        return ReverseIterator{it.m_base + n};
    }

    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] friend constexpr PtrDiffT operator-(const ReverseIterator lhs,
                                                                                     const ReverseIterator rhs) noexcept
    {
        return rhs.m_base - lhs.m_base;
    }

    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] friend constexpr bool operator==(const ReverseIterator lhs,
                                                                                  const ReverseIterator rhs) noexcept
    {
        return lhs.m_base == rhs.m_base;
    }

    ////////////////////////////////////////////////////////////
    // Reversed: an iterator further along the reversed range has a smaller base
    [[nodiscard, gnu::always_inline, gnu::pure]] friend constexpr bool operator<(const ReverseIterator lhs,
                                                                                 const ReverseIterator rhs) noexcept
    {
        return rhs.m_base < lhs.m_base;
    }

    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] friend constexpr bool operator>(const ReverseIterator lhs,
                                                                                 const ReverseIterator rhs) noexcept
    {
        return rhs < lhs;
    }

    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] friend constexpr bool operator<=(const ReverseIterator lhs,
                                                                                  const ReverseIterator rhs) noexcept
    {
        return !(rhs < lhs);
    }

    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] friend constexpr bool operator>=(const ReverseIterator lhs,
                                                                                  const ReverseIterator rhs) noexcept
    {
        return !(lhs < rhs);
    }

private:
    T* m_base{nullptr};
};


////////////////////////////////////////////////////////////
/// \brief Reverse iterator to the last element of a contiguous container (anything with `data()` and `size()`)
///
////////////////////////////////////////////////////////////
template <typename Container>
    requires requires(Container& container) {
        container.data();
        container.size();
    }
[[nodiscard, gnu::always_inline]] constexpr auto rbegin(Container& container) noexcept
{
    return ReverseIterator<RemoveReference<decltype(*container.data())>>{container.data() + container.size()};
}


////////////////////////////////////////////////////////////
/// \brief Reverse iterator one before the first element of a contiguous container
///
////////////////////////////////////////////////////////////
template <typename Container>
    requires requires(Container& container) {
        container.data();
        container.size();
    }
[[nodiscard, gnu::always_inline]] constexpr auto rend(Container& container) noexcept
{
    return ReverseIterator<RemoveReference<decltype(*container.data())>>{container.data()};
}


////////////////////////////////////////////////////////////
/// \brief Reverse iterator to the last element of a C-style array
///
////////////////////////////////////////////////////////////
template <typename T, SizeT N>
[[nodiscard, gnu::always_inline]] constexpr ReverseIterator<T> rbegin(T (&array)[N]) noexcept
{
    return ReverseIterator<T>{array + N};
}


////////////////////////////////////////////////////////////
/// \brief Reverse iterator one before the first element of a C-style array
///
////////////////////////////////////////////////////////////
template <typename T, SizeT N>
[[nodiscard, gnu::always_inline]] constexpr ReverseIterator<T> rend(T (&array)[N]) noexcept
{
    return ReverseIterator<T>{array + 0};
}


////////////////////////////////////////////////////////////
/// \brief A contiguous range, iterated from its last element to its first (see `za::reversed`)
///
////////////////////////////////////////////////////////////
template <typename T>
struct [[nodiscard]] ReversedRange
{
    ReverseIterator<T> first;
    ReverseIterator<T> last;

    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] constexpr ReverseIterator<T> begin() const noexcept
    {
        return first;
    }

    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] constexpr ReverseIterator<T> end() const noexcept
    {
        return last;
    }
};


////////////////////////////////////////////////////////////
/// \brief View of `range` (a contiguous container or a C-style array) from its last element to its first
///
/// E.g. `for (const int x : za::reversed(vector))`. Only binds to lvalues:
/// the view does not own the elements, so a temporary container would be
/// destroyed before the loop runs.
///
////////////////////////////////////////////////////////////
template <typename Range>
[[nodiscard, gnu::always_inline]] constexpr auto reversed(Range& range) noexcept
{
    // In the body: GCC rejects compiler-builtin traits (behind `RemoveReference`) in function signatures
    using Element = RemoveReference<decltype(*za::rbegin(range))>;
    return ReversedRange<Element>{za::rbegin(range), za::rend(range)};
}

} // namespace za
