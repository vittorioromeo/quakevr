#pragma once
// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include "Zancle/Math/ClampMacro.hpp"
#include "Zancle/Math/Fabs.hpp"

#include "Zancle/Base/AssertAndAssume.hpp"
#include "Zancle/Base/SizeT.hpp"

#include "Zancle/Trait/IsFloatingPoint.hpp"
#include "Zancle/Trait/IsSame.hpp"
#include "Zancle/Trait/IsUnsigned.hpp"


////////////////////////////////////////////////////////////
// Assert that converting `x` and `y` (of type `T`) to `U` does not wrap a negative value
#define ZA_PRIV_VEC2_ASSERT_CONVERTIBLE_TO(U)                                                              \
    if constexpr (ZA_IS_UNSIGNED(U) && !ZA_IS_SAME(U, bool) && !ZA_IS_UNSIGNED(T))                         \
    {                                                                                                      \
        ZA_ASSERT_AND_ASSUME(x >= T{0} && y >= T{0} && "cannot convert negative values to unsigned type"); \
    }


namespace za
{
class Angle;

////////////////////////////////////////////////////////////
/// \brief Class template for manipulating
///        2-dimensional vectors
///
////////////////////////////////////////////////////////////
template <typename T>
struct [[nodiscard]] Vec2
{
    ////////////////////////////////////////////////////////////
    /// \brief Construct the vec2 from polar coordinates <i><b>(floating-point)</b></i>
    ///
    /// \param r   Length of vec2 (can be negative)
    /// \param phi Angle from X axis
    ///
    /// Note that this constructor is lossy: calling `length()` and `angle()`
    /// may return values different to those provided in this constructor.
    ///
    /// In particular, these transforms can be applied:
    /// * `Vec2(r, phi) == Vec2(-r, phi + 180_deg)`
    /// * `Vec2(r, phi) == Vec2(r, phi + n * 360_deg)`
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] static constexpr Vec2 fromAngle(T r, Angle phi);


    ////////////////////////////////////////////////////////////
    /// \brief Length of the vec2 <i><b>(floating-point)</b></i>.
    ///
    /// If you are not interested in the actual length, but only in comparisons, consider using `lengthSquared()`.
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] constexpr T length() const;


    ////////////////////////////////////////////////////////////
    /// \brief Square of vec2's length.
    ///
    /// Suitable for comparisons, more efficient than `length()`.
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] inline constexpr T lengthSquared() const
    {
        return x * x + y * y;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Vec2 with same direction but length 1 <i><b>(floating-point)</b></i>.
    ///
    /// \pre `*this` is no zero vec2.
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] constexpr Vec2 normalized() const;

    ////////////////////////////////////////////////////////////
    /// \brief Signed angle from `*this` to `rhs` <i><b>(floating-point)</b></i>.
    ///
    /// \return The smallest angle which rotates `*this` in positive
    /// or negative direction, until it has the same direction as `rhs`.
    /// The result has a sign and lies in the range [-180, 180] degrees.
    /// \pre Neither `*this` nor `rhs` is a zero vec2.
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] constexpr Angle angleTo(Vec2 rhs) const;

    ////////////////////////////////////////////////////////////
    /// \brief Signed angle from +X or (1,0) vec2 <i><b>(floating-point)</b></i>.
    ///
    /// For example, the vec2 (1,0) corresponds to 0 degrees, (0,1) corresponds to 90 degrees.
    ///
    /// \return Angle in the range [-180, 180] degrees.
    /// \pre This vec2 is no zero vec2.
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] constexpr Angle angle() const;

    ////////////////////////////////////////////////////////////
    /// \brief Rotate by angle \c phi <i><b>(floating-point)</b></i>.
    ///
    /// Returns a vec2 with same length but different direction.
    ///
    /// In Zancle's default coordinate system with +X right and +Y down,
    /// this amounts to a clockwise rotation by `phi`.
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] constexpr Vec2 rotatedBy(Angle phi) const;


    ////////////////////////////////////////////////////////////
    /// \brief Move towards angle \c phi <i><b>(floating-point)</b></i> by \c r.
    ///
    /// Returns a vec2 starting at the position of the original, but moved by \c r
    /// units in the direction of \c phi.
    ///
    /// In Zancle's default coordinate system with +X right and +Y down,
    /// positive angles are measured clockwise from the +X axis.
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] constexpr Vec2 movedTowards(T r, Angle phi) const;

    ////////////////////////////////////////////////////////////
    /// \brief Projection of this vec2 onto `axis` <i><b>(floating-point)</b></i>.
    ///
    /// \param axis Vec2 being projected onto. Need not be normalized.
    /// \pre `axis` must not have length zero.
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] constexpr Vec2 projectedOnto(Vec2 axis) const;


    ////////////////////////////////////////////////////////////
    /// \brief Clamp the vec2's length to a maximum value <i><b>(floating-point)</b></i>.
    ///
    /// If the vec2's length exceeds `maxLength`, returns a vec2 with the same direction
    /// but with length equal to `maxLength`. Otherwise, returns a copy of the original vec2.
    ///
    /// \param maxLength The maximum length allowed. Must be non-negative.
    ///
    /// \return Vec2 with length clamped to `maxLength`.
    ///
    /// \pre `maxLength >= 0`.
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] constexpr Vec2 clampMaxLength(T maxLength) const;


    ////////////////////////////////////////////////////////////
    /// \brief Clamp the vec2's squared length to a maximum value <i><b>(floating-point)</b></i>.
    ///
    /// If the vec2's squared length exceeds `maxLengthSquared`, returns a vec2 with the same direction
    /// but with squared length equal to `maxLengthSquared`. Otherwise, returns a copy of the original vec2.
    /// This version is more efficient than `clampMaxLength` if you already have squared lengths.
    ///
    /// \param maxLengthSquared The maximum squared length allowed. Must be non-negative.
    ///
    /// \return Vec2 with squared length clamped to `maxLengthSquared`.
    ///
    /// \pre `maxLengthSquared >= 0`.
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] constexpr Vec2 clampMaxLengthSquared(T maxLengthSquared) const;


    ////////////////////////////////////////////////////////////
    /// \brief Clamp the vec2's length to a minimum value <i><b>(floating-point)</b></i>.
    ///
    /// If the vec2's length is less than `minLength`, returns a vec2 with the same direction
    /// but with length equal to `minLength`. Otherwise, returns a copy of the original vec2.
    ///
    /// \param minLength The minimum length allowed. Must be non-negative.
    ///
    /// \return Vec2 with length clamped to `minLength`.
    ///
    /// \pre `minLength >= 0`.
    /// \pre If `minLength > 0`, `*this` must not be the zero vec2.
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] constexpr Vec2 clampMinLength(T minLength) const;


    ////////////////////////////////////////////////////////////
    /// \brief Clamp the vec2's squared length to a minimum value <i><b>(floating-point)</b></i>.
    ///
    /// If the vec2's squared length is less than `minLengthSquared`, returns a vec2 with the same direction
    /// but with squared length equal to `minLengthSquared`. Otherwise, returns a copy of the original vec2.
    /// This version is more efficient than `clampMinLength` if you already have squared lengths.
    ///
    /// \param minLengthSquared The minimum squared length allowed. Must be non-negative.
    ///
    /// \return Vec2 with squared length clamped to `minLengthSquared`.
    ///
    /// \pre `minLengthSquared >= 0`.
    /// \pre If `minLengthSquared > 0`, `*this` must not be the zero vec2.
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] constexpr Vec2 clampMinLengthSquared(T minLengthSquared) const;


    ////////////////////////////////////////////////////////////
    /// \brief Clamp the vec2's length to be within a specific range <i><b>(floating-point)</b></i>.
    ///
    /// If the vec2's length is less than `minLength`, returns a vec2 with the same direction
    /// but with length equal to `minLength`. If the vec2's length is greater than `maxLength`,
    /// returns a vec2 with the same direction but with length equal to `maxLength`.
    /// Otherwise, returns a copy of the original vec2.
    ///
    /// \param minLength The minimum length allowed. Must be non-negative.
    /// \param maxLength The maximum length allowed. Must be >= `minLength`.
    ///
    /// \return Vec2 with length clamped between `minLength` and `maxLength`.
    ///
    /// \pre `0 <= minLength <= maxLength`.
    /// \pre If `minLength > 0`, `*this` must not be the zero vec2.
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] constexpr Vec2 clampLength(T minLength, T maxLength) const;


    ////////////////////////////////////////////////////////////
    /// \brief Clamp the vec2's squared length to be within a specific range <i><b>(floating-point)</b></i>.
    ///
    /// If the vec2's squared length is less than `minLengthSquared`, returns a vec2 with the same direction
    /// but with squared length equal to `minLengthSquared`. If the vec2's squared length is greater than
    /// `maxLengthSquared`, returns a vec2 with the same direction but with squared length equal to
    /// `maxLengthSquared`. Otherwise, returns a copy of the original vec2.
    /// This version is more efficient than `clampLength` if you already have squared lengths.
    ///
    /// \param minLengthSquared The minimum squared length allowed. Must be non-negative.
    /// \param maxLengthSquared The maximum squared length allowed. Must be >= `minLengthSquared`.
    ///
    /// \return Vec2 with squared length clamped between `minLengthSquared` and `maxLengthSquared`.
    ///
    /// \pre `0 <= minLengthSquared <= maxLengthSquared`.
    /// \pre If `minLengthSquared > 0`, `*this` must not be the zero vec2.
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] constexpr Vec2 clampLengthSquared(T minLengthSquared,
                                                                                                 T maxLengthSquared) const;


    ////////////////////////////////////////////////////////////
    /// \brief Returns a perpendicular vec2.
    ///
    /// Returns `*this` rotated by +90 degrees; (x,y) becomes (-y,x).
    /// For example, the vec2 (1,0) is transformed to (0,1).
    ///
    /// In Zancle's default coordinate system with +X right and +Y down,
    /// this amounts to a clockwise rotation.
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] inline constexpr Vec2 perpendicular() const
    {
        return Vec2<T>(-y, x);
    }


    ////////////////////////////////////////////////////////////
    /// \brief Dot product of two vec2s.
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] inline constexpr T dot(const Vec2 rhs) const
    {
        return x * rhs.x + y * rhs.y;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Z component of the cross product of two vec2s.
    ///
    /// Treats the operands as 3D vectors, computes their cross product
    /// and returns the result's Z component (X and Y components are always zero).
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] inline constexpr T cross(const Vec2 rhs) const
    {
        return x * rhs.y - y * rhs.x;
    }


    ////////////////////////////////////////////////////////////
    /// \brief Component-wise multiplication of `*this` and `rhs`.
    ///
    /// Computes `(lhs.x*rhs.x, lhs.y*rhs.y)`.
    ///
    /// Scaling is the most common use case for component-wise multiplication/division.
    /// This operation is also known as the Hadamard or Schur product.
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] inline constexpr Vec2 componentWiseMul(const Vec2 rhs) const
    {
        return Vec2<T>(x * rhs.x, y * rhs.y);
    }


    ////////////////////////////////////////////////////////////
    /// \brief Component-wise division of `*this` and `rhs`.
    ///
    /// Computes `(lhs.x/rhs.x, lhs.y/rhs.y)`.
    ///
    /// Scaling is the most common use case for component-wise multiplication/division.
    ///
    /// \pre Neither component of `rhs` is zero.
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] inline constexpr Vec2 componentWiseDiv(const Vec2 rhs) const
    {
        ZA_ASSERT_AND_ASSUME(rhs.x != 0 && "cannot divide by 0 (x coordinate)");
        ZA_ASSERT_AND_ASSUME(rhs.y != 0 && "cannot divide by 0 (y coordinate)");

        return Vec2<T>(x / rhs.x, y / rhs.y);
    }


    ////////////////////////////////////////////////////////////
    /// \brief Floors the components of the vector <i><b>(floating-point)</b></i>
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] inline constexpr Vec2 componentWiseFloor() const;


    ////////////////////////////////////////////////////////////
    /// \brief Clamps the components of the `*this` to the given range
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] inline constexpr Vec2 componentWiseClamp(const Vec2 mins,
                                                                                                        const Vec2 maxs) const
    {
        return {ZA_CLAMP(x, mins.x, maxs.x), ZA_CLAMP(y, mins.y, maxs.y)};
    }


    ////////////////////////////////////////////////////////////
    /// \brief Clamps the X component of the `*this` between `[minX, maxX]`
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] inline constexpr Vec2 clampX(const T minX, const T maxX) const
    {
        return {ZA_CLAMP(x, minX, maxX), y};
    }


    ////////////////////////////////////////////////////////////
    /// \brief Clamps the Y component of the `*this` between `[minY, maxY]`
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] inline constexpr Vec2 clampY(const T minY, const T maxY) const
    {
        return {x, ZA_CLAMP(y, minY, maxY)};
    }


    ////////////////////////////////////////////////////////////
    /// \brief Clamps the X component of the `*this` to be at least `minX`
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] inline constexpr Vec2 clampMinX(const T minX) const
    {
        return {x < minX ? minX : x, y};
    }


    ////////////////////////////////////////////////////////////
    /// \brief Clamps the Y component of the `*this` to be at least `minY`
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] inline constexpr Vec2 clampMinY(const T minY) const
    {
        return {x, y < minY ? minY : y};
    }


    ////////////////////////////////////////////////////////////
    /// \brief Clamps the X component of the `*this` to be at most `maxX`
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] inline constexpr Vec2 clampMaxX(const T maxX) const
    {
        return {x > maxX ? maxX : x, y};
    }


    ////////////////////////////////////////////////////////////
    /// \brief Clamps the Y component of the `*this` to be at most `maxY`
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] inline constexpr Vec2 clampMaxY(const T maxY) const
    {
        return {x, y > maxY ? maxY : y};
    }


    ////////////////////////////////////////////////////////////
    /// \brief Returns a new vec2 with `newX` instead of the current X component
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] inline constexpr Vec2 withX(const T newX) const
    {
        return {newX, y};
    }


    ////////////////////////////////////////////////////////////
    /// \brief Returns a new vec2 with `newY` instead of the current Y component
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] inline constexpr Vec2 withY(const T newY) const
    {
        return {x, newY};
    }


    ////////////////////////////////////////////////////////////
    /// \brief Returns a new vec2 with `newX` added to the X component
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] inline constexpr Vec2 addX(const T addedX) const
    {
        return {x + addedX, y};
    }


    ////////////////////////////////////////////////////////////
    /// \brief Returns a new vec2 with `addedY` added to the Y component
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] inline constexpr Vec2 addY(const T addedY) const
    {
        return {x, y + addedY};
    }


    ////////////////////////////////////////////////////////////
    /// \brief Returns a new vec2 where components have the absolute values of `*this`
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] inline constexpr Vec2 abs() const
    {
        // `fabs` is a single bitwise `and` (and maps `-0` to `+0`), unlike a compare-and-negate
        if constexpr (ZA_IS_FLOATING_POINT(T))
            return {za::fabs(x), za::fabs(y)};
        else
            return {x < T{0} ? -x : x, y < T{0} ? -y : y};
    }


    ////////////////////////////////////////////////////////////
    /// \brief Convert to another `Vec2` of type `OtherVec2`
    ///
    /// `OtherVec2` must be a `Vec2<...>` type.
    ///
    ////////////////////////////////////////////////////////////
    template <typename OtherVec2>
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] inline constexpr OtherVec2 to() const
    {
        using ValueType = decltype(OtherVec2{}.x);
        ZA_PRIV_VEC2_ASSERT_CONVERTIBLE_TO(ValueType);

        return Vec2<ValueType>{static_cast<ValueType>(x), static_cast<ValueType>(y)};
    }


    ////////////////////////////////////////////////////////////
    /// \brief Convert `*this` to a `Vec2<int>`
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] inline constexpr Vec2<int> toVec2i() const
    {
        return {static_cast<int>(x), static_cast<int>(y)};
    }


    ////////////////////////////////////////////////////////////
    /// \brief Convert `*this` to a `Vec2<float>`
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] inline constexpr Vec2<float> toVec2f() const
    {
        return {static_cast<float>(x), static_cast<float>(y)};
    }


    ////////////////////////////////////////////////////////////
    /// \brief Convert `*this` to a `Vec2<unsigned int>`
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] inline constexpr Vec2<unsigned int> toVec2u() const
    {
        ZA_PRIV_VEC2_ASSERT_CONVERTIBLE_TO(unsigned int);

        return {static_cast<unsigned int>(x), static_cast<unsigned int>(y)};
    }


    ////////////////////////////////////////////////////////////
    /// \brief Convert `*this` to a `Vec2<za::SizeT>`
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] inline constexpr Vec2<za::SizeT> toVec2uz() const
    {
        ZA_PRIV_VEC2_ASSERT_CONVERTIBLE_TO(za::SizeT);

        return {static_cast<za::SizeT>(x), static_cast<za::SizeT>(y)};
    }


    ////////////////////////////////////////////////////////////
    /// \brief Overload of binary `operator==`
    ///
    /// This operator compares strict equality between two vec2s.
    ///
    /// \param rhs Right operand
    ///
    /// \return `true` if `lhs` is equal to `rhs`
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::pure]] constexpr bool operator==(const Vec2& rhs) const = default;


    ////////////////////////////////////////////////////////////
    /// \relates Vec2
    /// \brief Overload of unary `operator-`
    ///
    /// \param rhs Vec2 to negate
    ///
    /// \return Member-wise opposite of the vec2
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::const]] friend constexpr Vec2 operator-(const Vec2 rhs)
    {
        return Vec2(-rhs.x, -rhs.y);
    }


    ////////////////////////////////////////////////////////////
    /// \relates Vec2
    /// \brief Overload of binary `operator+=`
    ///
    /// This operator performs a member-wise addition of both vec2s,
    /// and assigns the result to `lhs`.
    ///
    /// \param lhs  Left operand (a vec2)
    /// \param rhs Right operand (a vec2)
    ///
    /// \return Reference to `lhs`
    ///
    ////////////////////////////////////////////////////////////
    [[gnu::always_inline, gnu::flatten]] friend constexpr Vec2& operator+=(Vec2& lhs, const Vec2 rhs)
    {
        lhs.x += rhs.x;
        lhs.y += rhs.y;

        return lhs;
    }


    ////////////////////////////////////////////////////////////
    /// \relates Vec2
    /// \brief Overload of binary `operator-=`
    ///
    /// This operator performs a member-wise subtraction of both vec2s,
    /// and assigns the result to `lhs`.
    ///
    /// \param lhs  Left operand (a vec2)
    /// \param rhs Right operand (a vec2)
    ///
    /// \return Reference to `lhs`
    ///
    ////////////////////////////////////////////////////////////
    [[gnu::always_inline, gnu::flatten]] friend constexpr Vec2& operator-=(Vec2& lhs, const Vec2 rhs)
    {
        lhs.x -= rhs.x;
        lhs.y -= rhs.y;

        return lhs;
    }


    ////////////////////////////////////////////////////////////
    /// \relates Vec2
    /// \brief Overload of binary `operator+`
    ///
    /// \param lhs  Left operand (a vec2)
    /// \param rhs Right operand (a vec2)
    ///
    /// \return Member-wise addition of both vec2s
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::const]] friend constexpr Vec2 operator+(const Vec2 lhs, const Vec2 rhs)
    {
        return Vec2(lhs.x + rhs.x, lhs.y + rhs.y);
    }


    ////////////////////////////////////////////////////////////
    /// \relates Vec2
    /// \brief Overload of binary `operator-`
    ///
    /// \param lhs  Left operand (a vec2)
    /// \param rhs Right operand (a vec2)
    ///
    /// \return Member-wise subtraction of both vec2s
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::const]] friend constexpr Vec2 operator-(const Vec2 lhs, const Vec2 rhs)
    {
        return Vec2(lhs.x - rhs.x, lhs.y - rhs.y);
    }


    ////////////////////////////////////////////////////////////
    /// \relates Vec2
    /// \brief Overload of binary `operator*`
    ///
    /// \param lhs  Left operand (a vec2)
    /// \param rhs Right operand (a scalar value)
    ///
    /// \return Member-wise multiplication by `rhs`
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::const]] friend constexpr Vec2 operator*(const Vec2 lhs, const T rhs)
    {
        return Vec2(lhs.x * rhs, lhs.y * rhs);
    }


    ////////////////////////////////////////////////////////////
    /// \relates Vec2
    /// \brief Overload of binary `operator*`
    ///
    /// \param lhs  Left operand (a scalar value)
    /// \param rhs Right operand (a vec2)
    ///
    /// \return Member-wise multiplication by `lhs`
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::const]] friend constexpr Vec2 operator*(const T lhs, const Vec2 rhs)
    {
        return Vec2(lhs * rhs.x, lhs * rhs.y);
    }


    ////////////////////////////////////////////////////////////
    /// \relates Vec2
    /// \brief Overload of binary `operator/`
    ///
    /// \param lhs  Left operand (a scalar value)
    /// \param rhs Right operand (a vec2)
    ///
    /// \return `(lhs / rhs.x, lhs / rhs.y)`
    ///
    /// \pre Neither component of `rhs` is zero.
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::const]] friend constexpr Vec2 operator/(const T lhs, const Vec2 rhs)
    {
        ZA_ASSERT_AND_ASSUME(rhs.x != 0 && "Vec2::operator/ cannot divide by 0 (x coordinate)");
        ZA_ASSERT_AND_ASSUME(rhs.y != 0 && "Vec2::operator/ cannot divide by 0 (y coordinate)");

        return Vec2(lhs / rhs.x, lhs / rhs.y);
    }


    ////////////////////////////////////////////////////////////
    /// \relates Vec2
    /// \brief Overload of binary `operator*=`
    ///
    /// This operator performs a member-wise multiplication by `rhs`,
    /// and assigns the result to `lhs`.
    ///
    /// \param lhs  Left operand (a vec2)
    /// \param rhs Right operand (a scalar value)
    ///
    /// \return Reference to `lhs`
    ///
    ////////////////////////////////////////////////////////////
    [[gnu::always_inline, gnu::flatten]] friend constexpr Vec2& operator*=(Vec2& lhs, const T rhs)
    {
        lhs.x *= rhs;
        lhs.y *= rhs;

        return lhs;
    }


    ////////////////////////////////////////////////////////////
    /// \relates Vec2
    /// \brief Overload of binary `operator/`
    ///
    /// \param lhs  Left operand (a vec2)
    /// \param rhs Right operand (a scalar value)
    ///
    /// \return Member-wise division by `rhs`
    ///
    ////////////////////////////////////////////////////////////
    [[nodiscard, gnu::always_inline, gnu::flatten, gnu::const]] friend constexpr Vec2 operator/(const Vec2 lhs, const T rhs)
    {
        ZA_ASSERT_AND_ASSUME(rhs != 0 && "Vec2::operator/ cannot divide by 0");

        return Vec2(lhs.x / rhs, lhs.y / rhs);
    }


    ////////////////////////////////////////////////////////////
    /// \relates Vec2
    /// \brief Overload of binary `operator/=`
    ///
    /// This operator performs a member-wise division by `rhs`,
    /// and assigns the result to `lhs`.
    ///
    /// \param lhs  Left operand (a vec2)
    /// \param rhs Right operand (a scalar value)
    ///
    /// \return Reference to `lhs`
    ///
    ////////////////////////////////////////////////////////////
    [[gnu::always_inline, gnu::flatten]] friend constexpr Vec2& operator/=(Vec2& lhs, const T rhs)
    {
        ZA_ASSERT_AND_ASSUME(rhs != 0 && "Vec2::operator/= cannot divide by 0");

        lhs.x /= rhs;
        lhs.y /= rhs;

        return lhs;
    }


    ////////////////////////////////////////////////////////////
    // Member data
    ////////////////////////////////////////////////////////////
    T x{}; //!< X coordinate of the vec2
    T y{}; //!< Y coordinate of the vec2
};

#undef ZA_PRIV_VEC2_ASSERT_CONVERTIBLE_TO

// Aliases for the most common types
using Vec2i  = Vec2<int>;
using Vec2u  = Vec2<unsigned int>;
using Vec2f  = Vec2<float>;
using Vec2uz = Vec2<za::SizeT>;

} // namespace za


////////////////////////////////////////////////////////////
/// \class za::Vec2
/// \ingroup system
///
/// `za::Vec2` is a simple class that defines a mathematical
/// vector with two coordinates (x and y). It can be used to
/// represent anything that has two dimensions: a size, a point,
/// a velocity, a scale, etc.
///
/// The API provides basic arithmetic (addition, subtraction, scale), as
/// well as more advanced geometric operations, such as dot/cross products,
/// length and angle computations, projections, rotations, etc.
///
/// The template parameter T is the type of the coordinates. It
/// can be any type that supports arithmetic operations (+, -, /, *)
/// and comparisons (==, !=), for example int or float.
/// Note that some operations are only meaningful for vec2s where T is
/// a floating point type (e.g. float or double), often because
/// results cannot be represented accurately with integers.
/// The method documentation mentions "(floating-point)" in those cases.
///
/// You generally don't have to care about the templated form (`za::Vec2<T>`),
/// the most common specializations have special type aliases:
/// \li `za::Vec2<float>` is `za::Vec2f`
/// \li `za::Vec2<int>` is `za::Vec2i`
/// \li `za::Vec2<unsigned int>` is `za::Vec2u`
///
/// The `za::Vec2` class has a simple interface, its x and y members
/// can be accessed directly (there are no accessors like setX(), getX()).
///
/// Usage example:
/// \code
/// za::Vec2f v(16.5f, 24.f);
/// v.x = 18.2f;
/// float y = v.y;
///
/// za::Vec2f w = v * 5.f;
/// za::Vec2f u;
/// u = v + w;
///
/// float s = v.dot(w);
///
/// bool different = (v != u);
/// \endcode
///
/// Note: for 3-dimensional vectors, see `za::Vec3`.
///
////////////////////////////////////////////////////////////
