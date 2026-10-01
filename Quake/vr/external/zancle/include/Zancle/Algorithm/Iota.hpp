#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


namespace za
{
////////////////////////////////////////////////////////////
/// \brief Fill `[first, last)` with sequentially increasing values, starting from `value`
///
/// Same semantics as `std::iota`: assigns `value`, then `++value`, and so on.
///
////////////////////////////////////////////////////////////
template <typename ForwardIt, typename T>
[[gnu::always_inline]] constexpr void iota(ForwardIt first, const ForwardIt last, T value)
{
    for (; first != last; ++first, ++value)
        *first = value;
}

} // namespace za
