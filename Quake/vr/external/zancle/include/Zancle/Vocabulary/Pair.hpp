#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include "Zancle/Base/Macros.hpp"

#include "Zancle/Trait/Decay.hpp"


namespace za
{
////////////////////////////////////////////////////////////
/// \brief Two values of possibly different types (like `std::pair`)
///
/// An aggregate: `Pair{1, 2.f}`, `Pair<int, float>{}`, structured
/// bindings, and trivial copies/moves when both members have them.
/// Compared member by member: `==` uses the members' `==`, while the
/// ordering operators are lexicographic and only use the members' `<`
/// (like `std::pair`'s), so a vector of pairs can be sorted and binary
/// searched as is. Where the two values have meaningful names, prefer a
/// struct of its own.
///
////////////////////////////////////////////////////////////
template <typename A, typename B>
struct [[nodiscard]] Pair
{
    ////////////////////////////////////////////////////////////
    // Member data
    ////////////////////////////////////////////////////////////
    A first;
    B second;


    ////////////////////////////////////////////////////////////
    /// \brief Member-wise equality comparison
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] constexpr bool operator==(const Pair& rhs) const = default;


    ////////////////////////////////////////////////////////////
    /// \brief Lexicographic ordering: by `first`, then by `second`
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] constexpr bool operator<(const Pair& rhs) const
    {
        return first < rhs.first || (!(rhs.first < first) && second < rhs.second);
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline]] constexpr bool operator>(const Pair& rhs) const
    {
        return rhs < *this;
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline]] constexpr bool operator<=(const Pair& rhs) const
    {
        return !(rhs < *this);
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline]] constexpr bool operator>=(const Pair& rhs) const
    {
        return !(*this < rhs);
    }
};


////////////////////////////////////////////////////////////
/// \brief Deduction guide enabling `Pair{a, b}` syntax (decays, like `std::pair`'s)
///
////////////////////////////////////////////////////////////
template <typename A, typename B>
Pair(A, B) -> Pair<A, B>;


////////////////////////////////////////////////////////////
/// \brief Make a `Pair` of the decayed types of `a` and `b` (like `std::make_pair`)
///
////////////////////////////////////////////////////////////
template <typename A, typename B>
[[nodiscard, gnu::always_inline]] constexpr auto makePair(A&& a, B&& b)
{
    // `Decay` in the body: GCC rejects built-in traits in function signatures
    return Pair<Decay<A>, Decay<B>>{ZA_FORWARD(a), ZA_FORWARD(b)};
}

} // namespace za
