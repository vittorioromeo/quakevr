#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include "Zancle/Base/AssertAndAssume.hpp"
#include "Zancle/Base/IntTypes.hpp"


namespace za
{
////////////////////////////////////////////////////////////
/// \brief The xoroshiro128++ pseudo-random bit generator (Blackman & Vigna)
///
/// Extremely fast, with a small state (128 bits) and excellent statistical
/// quality (it passes BigCrush and PractRand): ideal for games, simulations,
/// and visual effects.
///
/// \warning **Not cryptographically secure.** Its whole state can be
///          recovered from two consecutive outputs, after which every past
///          and future output is predictable. Never use it for anything
///          security-related (keys, tokens, nonces, anti-cheat, ...).
///
/// The output sequence for a given seed is fully determined by the
/// algorithm, which is why this type is named after it.
///
/// Satisfies the C++ `UniformRandomBitGenerator` concept. Usable in
/// constant expressions.
///
////////////////////////////////////////////////////////////
class [[nodiscard]] Xoroshiro128PlusPlusBitGenerator
{
public:
    using result_type = U64; //!< Type returned by `operator()` and `next()`
    using SeedType    = U64; //!< Type used for seeding

private:
    ////////////////////////////////////////////////////////////
    /// \brief Rotates the bits of `x` left by `k` positions.
    ///
    /// \param x Value to rotate
    /// \param k Number of positions to rotate
    ///
    /// \return Rotated value
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::const]] static inline constexpr U64 rotl(const U64 x,
                                                                                                 const unsigned k) noexcept
    {
        ZA_ASSERT_AND_ASSUME(k < 64u);
        return (x << k) | (x >> ((64u - k) & 63u));
    }


    ////////////////////////////////////////////////////////////
    /// \brief Implements the SplitMix64 algorithm to initialize state.
    ///
    /// Used internally for seeding the main generator state from a single seed value.
    ///
    /// \param seed Input seed value
    ///
    /// \return A 64-bit pseudo-random number derived from the seed
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline]] static inline constexpr U64 splitmix64(U64& seed) noexcept
    {
        seed += 0x9e'37'79'b9'7f'4a'7c'15ULL;

        U64 z = seed;

        z = (z ^ (z >> 30)) * 0xbf'58'47'6d'1c'e4'e5'b9ULL;
        z = (z ^ (z >> 27)) * 0x94'd0'49'bb'13'31'11'ebULL;

        return z ^ (z >> 31);
    }


    ////////////////////////////////////////////////////////////
    /// \brief Initializes the generator state from a single seed value.
    ///
    /// \param seed The seed value to use.
    ///
    ////////////////////////////////////////////////////////////
    [[gnu::always_inline]] inline constexpr void seedInternal(SeedType seedValue) noexcept
    {
        // Use SplitMix64 to generate the initial 128-bit state
        m_state[0] = splitmix64(seedValue);
        m_state[1] = splitmix64(seedValue);

        // Ensure the initial state is not all zeros, which is invalid for xoroshiro128++
        if (m_state[0] == 0ULL && m_state[1] == 0ULL)
        {
            m_state[0] = DefaultSeed::State0; // Fallback to default non-zero state
            m_state[1] = DefaultSeed::State1;
        }
    }


    ////////////////////////////////////////////////////////////
    /// \brief Advances the state as `polynomial` calls to `next()` would (see `jump` and `longJump`)
    ///
    /// The state transition is linear over GF(2), so advancing by `2^k` steps is a
    /// combination of the next 128 states, selected by a precomputed jump polynomial.
    ///
    ////////////////////////////////////////////////////////////
    inline constexpr void jumpBy(const U64 (&polynomial)[2]) noexcept
    {
        U64 s0 = 0u;
        U64 s1 = 0u;

        for (const U64 word : polynomial)
            for (unsigned int bit = 0u; bit < 64u; ++bit)
            {
                if ((word & (U64{1} << bit)) != 0u)
                {
                    s0 ^= m_state[0];
                    s1 ^= m_state[1];
                }

                (void)next();
            }

        m_state[0] = s0;
        m_state[1] = s1;
    }


    ////////////////////////////////////////////////////////////
    // Constants for the default seed if none is provided
    enum [[nodiscard]] DefaultSeed : U64
    {
        State0 = 123'456'789'123'456'789ULL,
        State1 = 987'654'321'987'654'321ULL
    };


    ////////////////////////////////////////////////////////////
    U64 m_state[2]{}; //!< Internal state of the generator

public:
    ////////////////////////////////////////////////////////////
    /// \brief Default constructor. Initializes with a fixed internal seed.
    ///
    ////////////////////////////////////////////////////////////
    explicit constexpr Xoroshiro128PlusPlusBitGenerator() noexcept : m_state{DefaultSeed::State0, DefaultSeed::State1}
    {
        // Ensure default state isn't all zeros (though these constants aren't)
        ZA_ASSERT_AND_ASSUME(m_state[0] != 0 || m_state[1] != 0);
    }


    ////////////////////////////////////////////////////////////
    /// \brief Constructor that initializes the generator with a specific seed.
    ///
    /// \param seed The seed value.
    ///
    ////////////////////////////////////////////////////////////
    explicit constexpr Xoroshiro128PlusPlusBitGenerator(const SeedType seed) noexcept
    {
        seedInternal(seed);
    }


    ////////////////////////////////////////////////////////////
    /// \brief Generates the next 64-bit pseudo-random number.
    ///
    /// \return A 64-bit unsigned integer.
    ///
    /// Implements the core xoroshiro128++ algorithm step.
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten]] inline constexpr result_type next() noexcept
    {
        const U64 s0 = m_state[0];
        U64       s1 = m_state[1];

        const U64 result = rotl(s0 + s1, 17u) + s0; // The '++' scrambler

        s1 ^= s0;

        m_state[0] = rotl(s0, 49u) ^ s1 ^ (s1 << 21);
        m_state[1] = rotl(s1, 28u);

        return result;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Generates the next 64-bit pseudo-random number (UniformRandomBitGenerator interface).
    ///
    /// \return A 64-bit unsigned integer.
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline]] inline constexpr result_type operator()() noexcept
    {
        return next();
    }


    ////////////////////////////////////////////////////////////
    /// \brief Advances the generator as if by 2^64 calls to `next()`
    ///
    /// Copies of a generator, each jumped a different number of times, produce
    /// 2^64 non-overlapping subsequences of length 2^64: e.g. one independent
    /// stream per thread of a parallel computation.
    ///
    ////////////////////////////////////////////////////////////
    inline constexpr void jump() noexcept
    {
        jumpBy({0x2b'd7'a6'a6'e9'9c'2d'dcULL, 0x09'92'cc'af'6a'6f'ca'05ULL});
    }


    ////////////////////////////////////////////////////////////
    /// \brief Advances the generator as if by 2^96 calls to `next()`
    ///
    /// Generates 2^32 starting points, from each of which `jump()` generates
    /// 2^32 non-overlapping subsequences (e.g. one `longJump` per machine,
    /// then one `jump` per thread).
    ///
    ////////////////////////////////////////////////////////////
    inline constexpr void longJump() noexcept
    {
        jumpBy({0x36'0f'd5'f2'cf'8d'5d'99ULL, 0x9c'6e'68'77'73'6c'46'e3ULL});
    }


    ////////////////////////////////////////////////////////////
    /// \brief Returns the minimum value potentially generated (UniformRandomBitGenerator interface).
    ///
    /// \return `0`
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline]] static constexpr result_type min() noexcept
    {
        return 0u;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Returns the maximum value potentially generated (UniformRandomBitGenerator interface).
    ///
    /// \return Maximum value of `result_type` (`U64`)
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline]] static constexpr result_type max() noexcept
    {
        return static_cast<U64>(-1);
    }
};

} // namespace za


////////////////////////////////////////////////////////////
/// \class za::Xoroshiro128PlusPlusBitGenerator
/// \ingroup system
///
/// Raw 64-bit bit generator: prefer `za::FastNonCryptoRng` for uniform
/// integers, floats, and vectors. Use this directly with algorithms that
/// expect a C++ `UniformRandomBitGenerator`.
///
/// \see `za::FastNonCryptoRng`
///
////////////////////////////////////////////////////////////
