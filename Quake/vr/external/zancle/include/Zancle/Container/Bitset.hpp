#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include "Zancle/Base/Assert.hpp"
#include "Zancle/Base/Ctzll.hpp"
#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Base/Popcountll.hpp"
#include "Zancle/Base/SizeT.hpp"


namespace za
{
////////////////////////////////////////////////////////////
/// \brief Fixed-size bitset of `N` bits backed by 64-bit words
///
////////////////////////////////////////////////////////////
template <SizeT N>
class [[nodiscard]] Bitset
{
    ////////////////////////////////////////////////////////////
    static_assert(N > 0u);


    ////////////////////////////////////////////////////////////
    static constexpr SizeT bitsPerWord = 64u;
    static constexpr SizeT wordCount   = (N + bitsPerWord - 1u) / bitsPerWord;
    static constexpr SizeT remainder   = N % bitsPerWord;

    // Mask of the bits that are actually live in the trailing word.
    // For `N` exactly divisible by 64, every bit is live; otherwise,
    // only the lowest `remainder` bits.
    static constexpr U64 lastWordMask = (remainder == 0u) ? ~U64{0} : (U64{1} << remainder) - 1u;


    ////////////////////////////////////////////////////////////
    /// \brief Word holding bit `i`
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::const]] static constexpr SizeT wordOf(const SizeT i) noexcept
    {
        return i / bitsPerWord;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Single-bit mask within a word for bit `i`
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::const]] static constexpr U64 maskOf(const SizeT i) noexcept
    {
        return U64{1} << (i % bitsPerWord);
    }


public:
    ////////////////////////////////////////////////////////////
    using size_type = SizeT;


    ////////////////////////////////////////////////////////////
    /// \brief Default constructor; all bits zero
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] constexpr Bitset() noexcept = default;


    ////////////////////////////////////////////////////////////
    /// \brief Initialize the lowest 64 bits from `lowBits`
    ///
    /// Bits at index 64+ (if any) are zero. If `N < 64`, the high
    /// bits of `lowBits` outside `[0, N)` are masked off.
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] explicit constexpr Bitset(const U64 lowBits) noexcept
    {
        if constexpr (N >= bitsPerWord)
            m_words[0] = lowBits;
        else
            m_words[0] = lowBits & lastWordMask;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Total number of bits this bitset can hold
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::const]] static constexpr SizeT size() noexcept
    {
        return N;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Read bit `i`
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] constexpr bool test(const SizeT i) const noexcept
    {
        ZA_ASSERT(i < N);
        return (m_words[wordOf(i)] & maskOf(i)) != 0u;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Read bit `i` (alias for `test`)
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] constexpr bool operator[](const SizeT i) const noexcept
    {
        return test(i);
    }


    ////////////////////////////////////////////////////////////
    /// \brief Set bit `i` to 1
    ///
    ////////////////////////////////////////////////////////////
    [[gnu::always_inline]] constexpr void set(const SizeT i) noexcept
    {
        ZA_ASSERT(i < N);
        m_words[wordOf(i)] |= maskOf(i);
    }


    ////////////////////////////////////////////////////////////
    /// \brief Set bit `i` to 0
    ///
    ////////////////////////////////////////////////////////////
    [[gnu::always_inline]] constexpr void reset(const SizeT i) noexcept
    {
        ZA_ASSERT(i < N);
        m_words[wordOf(i)] &= ~maskOf(i);
    }


    ////////////////////////////////////////////////////////////
    /// \brief Toggle bit `i`
    ///
    ////////////////////////////////////////////////////////////
    [[gnu::always_inline]] constexpr void flip(const SizeT i) noexcept
    {
        ZA_ASSERT(i < N);
        m_words[wordOf(i)] ^= maskOf(i);
    }


    ////////////////////////////////////////////////////////////
    /// \brief Set bit `i` to `value`
    ///
    /// Branchless (even at `-O0`): sets the bit if `value` is `true`,
    /// clears it otherwise. Useful when you have a `bool` in hand and
    /// don't want to spell out the if-else.
    ///
    ////////////////////////////////////////////////////////////
    [[gnu::always_inline]] constexpr void setBit(const SizeT i, const bool value) noexcept
    {
        ZA_ASSERT(i < N);

        const SizeT w = wordOf(i);

        // Clear the bit, then OR in `value` (0 or 1) shifted into place.
        m_words[w] = (m_words[w] & ~maskOf(i)) | (U64{value} << (i % bitsPerWord));
    }


    ////////////////////////////////////////////////////////////
    /// \brief Set every bit to 1 (preserves the trailing-zero invariant)
    ///
    ////////////////////////////////////////////////////////////
    [[gnu::always_inline]] constexpr void setAll() noexcept
    {
        for (SizeT w = 0u; w < wordCount; ++w)
            m_words[w] = ~U64{0};

        if constexpr (remainder != 0u)
            m_words[wordCount - 1u] &= lastWordMask;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Set every bit to 0
    ///
    ////////////////////////////////////////////////////////////
    [[gnu::always_inline]] constexpr void resetAll() noexcept
    {
        for (SizeT w = 0u; w < wordCount; ++w)
            m_words[w] = 0u;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Toggle every bit (preserves the trailing-zero invariant)
    ///
    ////////////////////////////////////////////////////////////
    [[gnu::always_inline]] constexpr void flipAll() noexcept
    {
        for (SizeT w = 0u; w < wordCount; ++w)
            m_words[w] = ~m_words[w];

        if constexpr (remainder != 0u)
            m_words[wordCount - 1u] &= lastWordMask;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Number of bits set to 1
    ///
    /// Computed via `ZA_POPCOUNTLL` per word; the trailing-zero
    /// invariant means the last word's unused bits don't contribute.
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::pure]] constexpr SizeT count() const noexcept
    {
        SizeT total = 0u;

        for (SizeT w = 0u; w < wordCount; ++w)
            total += static_cast<SizeT>(ZA_POPCOUNTLL(m_words[w]));

        return total;
    }


    ////////////////////////////////////////////////////////////
    /// \brief `true` iff at least one bit is set
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::pure]] constexpr bool any() const noexcept
    {
        for (SizeT w = 0u; w < wordCount; ++w)
            if (m_words[w] != 0u)
                return true;

        return false;
    }


    ////////////////////////////////////////////////////////////
    /// \brief `true` iff all `N` bits are set
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::pure]] constexpr bool all() const noexcept
    {
        // All non-trailing words must be entirely set; the trailing
        // word must equal `lastWordMask` (which is `~0` when `N`
        // is exactly a multiple of 64).
        for (SizeT w = 0u; w + 1u < wordCount; ++w)
            if (m_words[w] != ~U64{0})
                return false;

        return m_words[wordCount - 1u] == lastWordMask;
    }


    ////////////////////////////////////////////////////////////
    /// \brief `true` iff no bits are set
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] constexpr bool none() const noexcept
    {
        return !any();
    }


    ////////////////////////////////////////////////////////////
    /// \brief Index of the lowest set bit, or `N` if there are none
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::pure]] constexpr SizeT findFirstSet() const noexcept
    {
        for (SizeT w = 0u; w < wordCount; ++w)
            if (m_words[w] != 0u)
                return w * bitsPerWord + static_cast<SizeT>(ZA_CTZLL(m_words[w]));

        return N;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Index of the lowest set bit at position >= `from`, or `N`
    ///
    /// Useful for the loop pattern
    ///   `for (auto i = b.findNextSet(0); i < b.size(); i = b.findNextSet(i + 1))`.
    /// `forEachSet` is usually a cleaner alternative.
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::pure]] constexpr SizeT findNextSet(const SizeT from) const noexcept
    {
        if (from >= N)
            return N;

        SizeT     w    = wordOf(from);
        const U64 word = m_words[w] & (~U64{0} << (from % bitsPerWord));

        if (word != 0u)
            return w * bitsPerWord + static_cast<SizeT>(ZA_CTZLL(word));

        for (++w; w < wordCount; ++w)
            if (m_words[w] != 0u)
                return w * bitsPerWord + static_cast<SizeT>(ZA_CTZLL(m_words[w]));

        return N;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Invoke `fn(i)` for every set bit `i`, in ascending order
    ///
    /// Uses the `word & (word - 1)` lowest-bit-clear trick so each
    /// invocation costs one `ctzll` plus an AND.
    ///
    ////////////////////////////////////////////////////////////
    template <typename F>
    [[gnu::always_inline]] constexpr void forEachSet(F&& fn) const
    {
        for (SizeT w = 0u; w < wordCount; ++w)
        {
            U64 word = m_words[w];

            while (word != 0u)
            {
                const auto bit = static_cast<SizeT>(ZA_CTZLL(word));
                fn(w * bitsPerWord + bit);
                word &= word - 1u; // clear the lowest set bit
            }
        }
    }


    ////////////////////////////////////////////////////////////
    /// \brief Compound bitwise AND with another bitset of the same size
    ///
    ////////////////////////////////////////////////////////////
    [[gnu::always_inline]] constexpr Bitset& operator&=(const Bitset& rhs) noexcept
    {
        for (SizeT w = 0u; w < wordCount; ++w)
            m_words[w] &= rhs.m_words[w];

        return *this;
    }


    ////////////////////////////////////////////////////////////
    [[gnu::always_inline]] constexpr Bitset& operator|=(const Bitset& rhs) noexcept
    {
        for (SizeT w = 0u; w < wordCount; ++w)
            m_words[w] |= rhs.m_words[w];

        return *this;
    }


    ////////////////////////////////////////////////////////////
    [[gnu::always_inline]] constexpr Bitset& operator^=(const Bitset& rhs) noexcept
    {
        for (SizeT w = 0u; w < wordCount; ++w)
            m_words[w] ^= rhs.m_words[w];

        return *this;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Bitwise NOT (preserves the trailing-zero invariant
    ///        via `flipAll`)
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::pure]] constexpr Bitset operator~() const noexcept
    {
        Bitset result = *this;
        result.flipAll();
        return result;
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::pure]] friend constexpr Bitset operator&(Bitset lhs, const Bitset& rhs) noexcept
    {
        return lhs &= rhs;
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::pure]] friend constexpr Bitset operator|(Bitset lhs, const Bitset& rhs) noexcept
    {
        return lhs |= rhs;
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::pure]] friend constexpr Bitset operator^(Bitset lhs, const Bitset& rhs) noexcept
    {
        return lhs ^= rhs;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Shift bits toward higher indices (drops bits past `N-1`)
    ///
    /// `b <<= n` is the bit-numbered analogue of multiplication by
    /// `2^n`: each bit at position `i` moves to position `i + n`,
    /// and bits that would land at `i >= N` are discarded. New low
    /// bits are zero. Maintains the trailing-zero invariant.
    ///
    /// `n >= N` clears the bitset entirely (matching `std::bitset`).
    ///
    ////////////////////////////////////////////////////////////
    [[gnu::always_inline]] constexpr Bitset& operator<<=(const SizeT n) noexcept
    {
        if constexpr (N <= bitsPerWord) // Single-word fast path
            m_words[0] = (n >= N) ? U64{0} : ((m_words[0] << n) & lastWordMask);
        else
            shiftLeftMultiWord(n);

        return *this;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Shift bits toward lower indices (drops bits past 0)
    ///
    /// `b >>= n` is the bit-numbered analogue of division by `2^n`:
    /// each bit at position `i` moves to position `i - n`, and bits
    /// at `i < n` are discarded. New high bits are zero. The
    /// trailing-zero invariant is automatically preserved (right
    /// shift can only zero high bits).
    ///
    /// `n >= N` clears the bitset entirely.
    ///
    ////////////////////////////////////////////////////////////
    [[gnu::always_inline]] constexpr Bitset& operator>>=(const SizeT n) noexcept
    {
        if constexpr (N <= bitsPerWord) // Single-word fast path
            m_words[0] = (n >= N) ? U64{0} : (m_words[0] >> n);
        else
            shiftRightMultiWord(n);

        return *this;
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::pure]] friend constexpr Bitset operator<<(Bitset lhs, const SizeT n) noexcept
    {
        return lhs <<= n;
    }


    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::pure]] friend constexpr Bitset operator>>(Bitset lhs, const SizeT n) noexcept
    {
        return lhs >>= n;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Equality (defaulted; works because the trailing-zero
    ///        invariant guarantees the unused bits are always equal)
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard]] constexpr bool operator==(const Bitset&) const noexcept = default;


    ////////////////////////////////////////////////////////////
    /// \brief Convert the (single-word) bitset to a `U64`
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::pure]] constexpr U64 toU64() const noexcept
        requires(N <= bitsPerWord)
    {
        return m_words[0];
    }


private:
    ////////////////////////////////////////////////////////////
    /// \brief Multi-word implementation of `operator<<=`
    ///
    /// Deliberately not `always_inline`, to keep call sites small.
    ///
    ////////////////////////////////////////////////////////////
    constexpr void shiftLeftMultiWord(const SizeT n) noexcept
    {
        if (n >= N)
        {
            resetAll();
            return;
        }

        if (n == 0u)
            return;

        const SizeT wordShift = n / bitsPerWord;
        const SizeT bitShift  = n % bitsPerWord;

        if (bitShift == 0u)
        {
            // Pure word shift, high-to-low to allow in-place rewriting.
            for (SizeT i = wordCount; i-- > wordShift;)
                m_words[i] = m_words[i - wordShift];
        }
        else
        {
            const SizeT inv = bitsPerWord - bitShift;

            // For i > wordShift: combine the shifted source word with
            // the carry-over from the next-lower source word.
            for (SizeT i = wordCount; i-- > wordShift + 1u;)
                m_words[i] = (m_words[i - wordShift] << bitShift) | (m_words[i - wordShift - 1u] >> inv);

            // For i == wordShift: only the shifted-in m_words[0]; no carry.
            m_words[wordShift] = m_words[0] << bitShift;
        }

        // Zero the now-vacated low words.
        for (SizeT i = 0u; i < wordShift; ++i)
            m_words[i] = 0u;

        // Trailing-zero invariant: any bits that crossed the high end
        // must be masked off.
        if constexpr (remainder != 0u)
            m_words[wordCount - 1u] &= lastWordMask;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Multi-word implementation of `operator>>=`
    ///
    /// Deliberately not `always_inline`, to keep call sites small.
    ///
    ////////////////////////////////////////////////////////////
    constexpr void shiftRightMultiWord(const SizeT n) noexcept
    {
        if (n >= N)
        {
            resetAll();
            return;
        }

        if (n == 0u)
            return;

        const SizeT wordShift = n / bitsPerWord;
        const SizeT bitShift  = n % bitsPerWord;

        if (bitShift == 0u)
        {
            // Pure word shift, low-to-high to allow in-place rewriting.
            for (SizeT i = 0u; i + wordShift < wordCount; ++i)
                m_words[i] = m_words[i + wordShift];
        }
        else
        {
            const SizeT inv = bitsPerWord - bitShift;

            // For i + wordShift + 1 < wordCount: combine the shifted
            // source word with the carry-down from the next-higher one.
            for (SizeT i = 0u; i + wordShift + 1u < wordCount; ++i)
                m_words[i] = (m_words[i + wordShift] >> bitShift) | (m_words[i + wordShift + 1u] << inv);

            // For i + wordShift == wordCount - 1: only the shifted-down
            // trailing word; no carry-down.
            m_words[wordCount - 1u - wordShift] = m_words[wordCount - 1u] >> bitShift;
        }

        // Zero the now-vacated high words.
        for (SizeT i = wordCount - wordShift; i < wordCount; ++i)
            m_words[i] = 0u;
    }


    ////////////////////////////////////////////////////////////
    // Member data
    ////////////////////////////////////////////////////////////
    U64 m_words[wordCount]{};
};

} // namespace za


////////////////////////////////////////////////////////////
/// \file
///
/// \brief Fixed-size bitset, lightweight `std::bitset` substitute
///
/// `Bitset<N>` packs `N` bits into an array of 64-bit words, with
/// the unused bits in the trailing word held to zero as an internal
/// invariant. This makes `count`, `any`, `all`, `none`, `==`, and
/// the bitwise-compound ops trivially loop-and-mask, with no extra
/// per-call masking step.
///
/// Differences vs. `std::bitset`:
/// - No string conversions (avoids `<string>` / `<string_view>`).
/// - No proxy `operator[]` for writes; use `set(i)` / `reset(i)` /
///   `setBit(i, v)` instead. `operator[]` is read-only.
/// - Adds `findFirstSet` / `findNextSet` / `forEachSet` for the
///   typical "iterate the live bits" pattern.
/// - `toU64()` is constrained to `N <= 64` (no exception path).
///
////////////////////////////////////////////////////////////
