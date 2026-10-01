#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include "Zancle/Random/Xoroshiro128PlusPlusBitGenerator.hpp"

#include "Zancle/Geometry/Priv/Vec2Base.hpp"

#include "Zancle/Math/Constants.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/Math/Sqrt.hpp"

#include "Zancle/Base/AssertAndAssume.hpp"
#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Base/MulWide.hpp"

#include "Zancle/Trait/IsIntegral.hpp"
#include "Zancle/Trait/MakeUnsigned.hpp"


namespace za
{
////////////////////////////////////////////////////////////
/// \brief Fast pseudo-random number generator for games, simulations, and visual effects
///
/// Wraps `Xoroshiro128PlusPlusBitGenerator` (excellent statistical quality,
/// sub-nanosecond per number) with uniform integers, floats, and 2D vectors.
///
/// \warning **Not cryptographically secure** ("NonCrypto"): its outputs are
///          predictable by anyone who observes a couple of them. Never use it
///          for anything security-related (keys, tokens, nonces, anti-cheat, ...).
///
/// Deterministic: the same seed always produces the same sequence. Usable
/// in constant expressions.
///
////////////////////////////////////////////////////////////
class [[nodiscard]] FastNonCryptoRng
{
private:
    Xoroshiro128PlusPlusBitGenerator m_engine;

    ////////////////////////////////////////////////////////////
    /// \brief Uniformly distributed integer in `[0, rangeSize)` (`rangeSize > 0`), without bias
    ///
    /// Lemire's nearly divisionless method: the high 64 bits of `random * rangeSize`
    /// are uniform in `[0, rangeSize)`, except for a sliver of `random` values (detected
    /// via the low 64 bits, and redrawn) that would favor some results. That needs
    /// a division only with probability below `rangeSize / 2^64`.
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten]] inline constexpr U64 getUniformBelow(const U64 rangeSize) noexcept
    {
        ZA_ASSERT_AND_ASSUME(rangeSize > 0u);

        U64 high = 0u;
        U64 low  = priv::mulWide(m_engine.next(), rangeSize, high);

        if (low < rangeSize) [[unlikely]]
        {
            const U64 threshold = (U64{0} - rangeSize) % rangeSize; // `2^64 % rangeSize`

            while (low < threshold)
                low = priv::mulWide(m_engine.next(), rangeSize, high);
        }

        return high;
    }

public:
    using SeedType = U64; //!< Type used for seeding

    ////////////////////////////////////////////////////////////
    /// \brief Default constructor. Initializes with a fixed internal seed.
    ///
    ////////////////////////////////////////////////////////////
    explicit constexpr FastNonCryptoRng() noexcept = default;

    ////////////////////////////////////////////////////////////
    /// \brief Constructor that initializes the generator with a specific seed.
    ///
    /// \param seed The seed value.
    ///
    ////////////////////////////////////////////////////////////
    explicit constexpr FastNonCryptoRng(const SeedType seed) noexcept : m_engine{seed}
    {
    }

    ////////////////////////////////////////////////////////////
    /// \brief Generates 64 uniformly distributed pseudo-random bits.
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten]] inline constexpr U64 next() noexcept
    {
        return m_engine.next();
    }

    ////////////////////////////////////////////////////////////
    /// \brief Advances the generator as if by 2^64 calls to `next()`
    ///
    /// Use on copies to get independent, non-overlapping streams (e.g. one per
    /// thread): copy, then `jump()` each copy a different number of times.
    ///
    ////////////////////////////////////////////////////////////
    inline constexpr void jump() noexcept
    {
        m_engine.jump();
    }

    ////////////////////////////////////////////////////////////
    /// \brief Advances the generator as if by 2^96 calls to `next()` (see `Xoroshiro128PlusPlusBitGenerator::longJump`)
    ///
    ////////////////////////////////////////////////////////////
    inline constexpr void longJump() noexcept
    {
        m_engine.longJump();
    }

    ////////////////////////////////////////////////////////////
    /// \brief Generates a uniformly distributed pseudo-random integer in `[min, max]`.
    ///
    /// \tparam T An integral type, up to 64 bits.
    ///
    /// \param min Minimum inclusive value.
    /// \param max Maximum inclusive value.
    ///
    /// \return A pseudo-random integer in the range `[min, max]`, without bias,
    ///         for any range (including the full range of `T`).
    ///
    ////////////////////////////////////////////////////////////
    template <typename T>
    [[nodiscard, gnu::always_inline, gnu::flatten]] inline constexpr T getI(const T min, const T max) noexcept
    {
        static_assert(ZA_IS_INTEGRAL(T));
        static_assert(sizeof(T) <= sizeof(U64));

        ZA_ASSERT_AND_ASSUME(min <= max);

        // Unsigned arithmetic throughout: no signed overflow, even for full ranges
        using UnsignedT = ZA_MAKE_UNSIGNED(T);

        const auto unsignedMin = static_cast<UnsignedT>(min);
        const auto span        = static_cast<U64>(static_cast<UnsignedT>(static_cast<UnsignedT>(max) - unsignedMin));

        // Full 64-bit range: every value is valid, and `span + 1` would wrap to zero
        if constexpr (sizeof(T) == sizeof(U64))
            if (span == ~U64{0}) [[unlikely]]
                return static_cast<T>(m_engine.next());

        return static_cast<T>(static_cast<UnsignedT>(unsignedMin + static_cast<UnsignedT>(getUniformBelow(span + 1u))));
    }

    ////////////////////////////////////////////////////////////
    /// \brief Generates a pseudo-random float within a specified range `[min, max]`.
    ///
    /// \param min Minimum inclusive value.
    /// \param max Maximum inclusive value.
    ///
    /// \return A pseudo-random float in the range `[min, max]`: one of 2^24
    ///         equally likely values, from exactly `min` up to `max`.
    ///
    /// \note For speed, the result is not clamped. When `max - min` is not
    ///       exactly representable, the largest results can round to one ulp
    ///       above `max` (or just below it, leaving `max` itself unreachable).
    ///       `max - min` must not overflow (e.g. `[-FLT_MAX, FLT_MAX]`).
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten]] inline constexpr float getF(const float min, const float max) noexcept
    {
        ZA_ASSERT_AND_ASSUME(min <= max);

        // The top 24 bits `r` fill a float's significand. `r * 2^-24 + r * 2^-48` is `r / (2^24 - 1)`
        // to within 2^-48, without a division: both products are exact, so the sum is rounded once,
        // yielding 2^24 distinct values from exactly `0.f` to exactly `1.f`
        // (Via `I32`: converting from `U64` to `float` takes a slow branchy sequence at `-O0`)
        const auto  r = static_cast<float>(static_cast<I32>(m_engine.next() >> 40u));
        const float t = r * 0x1p-24f + r * 0x1p-48f;

        // Exactly `min` for `t == 0` or `min == max`. (Unlike `min * (1 - t) + max * t`, which is exact at both
        // ends but often strays by an ulp in between, e.g. for 30% of results when `min == max == 0.1f`)
        return min + t * (max - min);
    }

    ////////////////////////////////////////////////////////////
    /// \brief Generates a random 2D vector with components in specified ranges.
    ///
    /// \param mins Vec2 containing minimum inclusive values `(x, y)`.
    /// \param maxs Vec2 containing maximum inclusive values `(x, y)`.
    ///
    /// \return A random Vec2f within the specified bounds.
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten]] inline Vec2f getVec2f(const Vec2f mins, const Vec2f maxs)
    {
        return {getF(mins.x, maxs.x), getF(mins.y, maxs.y)};
    }

    ////////////////////////////////////////////////////////////
    /// \brief Generates a random 2D vector with components between 0 and specified maximums.
    ///
    /// \param maxs Vec2 containing maximum inclusive values `(x, y)`.
    ///
    /// \return A random Vec2f within the range `[0, maxs.x]` and `[0, maxs.y]`.
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten]] inline Vec2f getVec2f(const Vec2f maxs)
    {
        return {getF(0.f, maxs.x), getF(0.f, maxs.y)};
    }

    ////////////////////////////////////////////////////////////
    /// \brief Generates a random point uniformly distributed within a circle.
    ///
    /// \param center Center of the circle.
    /// \param radius Radius of the circle.
    ///
    /// \return A random `Vec2f` inside the specified circle.
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten]] inline Vec2f getPointInCircle(const Vec2f center, const float radius)
    {
        const float angle    = getF(0.f, tau);
        const float distance = radius * ZA_MATH_SQRTF(getF(0.f, 1.f));

        // Compute the point's coordinates using polar-to-Cartesian conversion.
        return {center.x + distance * ZA_MATH_COSF(angle), center.y + distance * ZA_MATH_SINF(angle)};
    }

    ////////////////////////////////////////////////////////////
    /// \brief Generates either `-1.f` or `1.f` with equal probability.
    ///
    /// \return Either `-1.f` or `1.f`.
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten]] inline constexpr float getSignF() noexcept
    {
        return static_cast<float>((m_engine() >> 63u) << 1u) - 1.f;
    }

    ////////////////////////////////////////////////////////////
    /// \brief Generates a random 2D unit vector (direction).
    ///
    /// \return A random `Vec2f` with magnitude `1`.
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten]] inline Vec2f getDirVec2f()
    {
        const float angle = getF(0.f, tau);
        return {ZA_MATH_COSF(angle), ZA_MATH_SINF(angle)};
    }
};

} // namespace za


////////////////////////////////////////////////////////////
/// \class za::FastNonCryptoRng
/// \ingroup system
///
/// Usage example:
/// \code
/// za::FastNonCryptoRng rng{/* seed */ 1234u};
///
/// const int       damage   = rng.getI(5, 10);              // in [5, 10]
/// const float     angle    = rng.getF(0.f, za::tau);       // in [0, tau]
/// const za::Vec2f position = rng.getVec2f({800.f, 600.f}); // in [0, 800] x [0, 600]
///
/// // Independent, non-overlapping streams for parallel work
/// za::FastNonCryptoRng workerRng = rng;
/// workerRng.jump();
/// \endcode
///
/// \see `za::Xoroshiro128PlusPlusBitGenerator`
///
////////////////////////////////////////////////////////////
