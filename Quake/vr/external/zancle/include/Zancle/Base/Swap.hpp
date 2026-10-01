#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include "Zancle/Trait/SwapResolution.hpp"


namespace za::priv::swap_adl
{
////////////////////////////////////////////////////////////
// Niebloid (Customization Point Object)
struct SwapFn
{
    ////////////////////////////////////////////////////////////
    using SizeT = decltype(sizeof(0));


    ////////////////////////////////////////////////////////////
    template <typename T, SizeT N>
    [[gnu::always_inline]] static constexpr void operator()(T (&a)[N], T (&b)[N]) noexcept(isNoThrowSwappableV<T>)
    {
        for (SizeT i = 0; i < N; ++i)
            operator()(a[i], b[i]);
    }


    ////////////////////////////////////////////////////////////
    template <typename T>
    [[gnu::always_inline]] static constexpr void operator()(T& a, T& b) noexcept(isNoThrowSwappableV<T>)
    {
        if constexpr (MemberSwappable<T>)
        {
            // Highest priority: explicit member function exists
            a.swap(b);
        }
        else if constexpr (AdlSwappable<T>)
        {
            // A specialized ADL swap exists and is unambiguous
            swap(a, b);
        }
        else
        {
            // No valid specialized swap was found: fall back to move-swap
            T tempA = static_cast<T&&>(a);
            a       = static_cast<T&&>(b);
            b       = static_cast<T&&>(tempA);
        }
    }
};

} // namespace za::priv::swap_adl


namespace za
{
////////////////////////////////////////////////////////////
/// \brief Customization point object that swaps two values
///
/// Behaves like `std::ranges::swap`: prefers a member `swap`, then a
/// well-formed unqualified `swap` found via ADL, and finally falls
/// back to a manual three-step move-swap. Calling
/// `genericSwap(a, b)` is therefore the recommended way to swap
/// arbitrary objects in code that wants to honor user-provided
/// `swap` overloads without needing to write the customary
/// `using std::swap; swap(a, b);` dance.
///
/// It is `noexcept` exactly when the selected operation is (see
/// `za::isNoThrowSwappable`), so exceptions thrown by a user-provided
/// `swap` or by move operations propagate instead of terminating.
///
////////////////////////////////////////////////////////////
inline constexpr priv::swap_adl::SwapFn genericSwap{};


////////////////////////////////////////////////////////////
/// \brief Swap the elements pointed to by two iterators
///
////////////////////////////////////////////////////////////
template <typename ForwardIt1, typename ForwardIt2>
[[gnu::always_inline]] inline constexpr void iterSwap(const ForwardIt1 a, const ForwardIt2 b)
{
    genericSwap(*a, *b);
}


////////////////////////////////////////////////////////////
/// \brief Pairwise swap two ranges of equal length
///
/// \return Iterator to the element past the last element of the second range
///
////////////////////////////////////////////////////////////
template <typename ForwardIt1, typename ForwardIt2>
[[gnu::always_inline]] inline constexpr ForwardIt2 swapRanges(ForwardIt1 first1, const ForwardIt1 last1, ForwardIt2 first2)
{
    for (; first1 != last1; ++first1, ++first2)
        iterSwap(first1, first2);

    return first2;
}

} // namespace za
