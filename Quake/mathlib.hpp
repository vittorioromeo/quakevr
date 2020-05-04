/*
Copyright (C) 1996-2001 Id Software, Inc.
Copyright (C) 2002-2009 John Fitzgibbons and others
Copyright (C) 2007-2008 Kristian Duske
Copyright (C) 2010-2014 QuakeSpasm developers
Copyright (C) 2020-2020 Vittorio Romeo

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#ifndef __MATHLIB_H
#define __MATHLIB_H

// mathlib.h

#include <math.h>
#include "quakeglm.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846 // matches value in gcc v2 math.h
#endif

#define M_PI_DIV_180 (M_PI / 180.0) // johnfitz

struct mplane_s;

inline constexpr qvec3 vec3_zero{0.f, 0.f, 0.f};

#define nanmask (255 << 23) /* 7F800000 */
#if 0                       /* macro is violating strict aliasing rules */
#define IS_NAN(x) (((*(int*)(char*)&x) & nanmask) == nanmask)
#else
static inline int IS_NAN(float x)
{
    union
    {
        float f;
        int i;
    } num;
    num.f = x;
    return ((num.i & nanmask) == nanmask);
}
#endif

#define Q_rint(x) \
    ((x) > 0 ? (int)((x) + 0.5) : (int)((x)-0.5)) // johnfitz -- from joequake

#define DotProduct(x, y) (x[0] * y[0] + x[1] * y[1] + x[2] * y[2])
#define DoublePrecisionDotProduct(x, y) \
    ((double)x[0] * y[0] + (double)x[1] * y[1] + (double)x[2] * y[2])
#define VectorSubtract(a, b, c) \
    {                           \
        c[0] = a[0] - b[0];     \
        c[1] = a[1] - b[1];     \
        c[2] = a[2] - b[2];     \
    }
#define VectorAdd(a, b, c)  \
    {                       \
        c[0] = a[0] + b[0]; \
        c[1] = a[1] + b[1]; \
        c[2] = a[2] + b[2]; \
    }
#define VectorCopy(a, b) \
    {                    \
        b[0] = a[0];     \
        b[1] = a[1];     \
        b[2] = a[2];     \
    }

[[nodiscard]] inline qvec3 safeNormalize(const qvec3& in)
{
    const auto length = glm::length(in);
    return length != 0.f ? in / length : in;
}

[[nodiscard]] qvec3 VectorAngles(const qvec3& forward) noexcept; // johnfitz

float VectorLength(vec3_t v);

[[nodiscard]] qmat3 R_ConcatRotations(
    const qmat3& in1, const qmat3& in2) noexcept;
void R_ConcatTransforms(float in1[3][4], float in2[3][4], float out[3][4]);

inline qvec3 AngleVectorsOnlyFwd(const qvec3& angles) noexcept
{
    const auto yawRadians = glm::radians(angles[YAW]);
    assert(!std::isnan(yawRadians));
    assert(!std::isinf(yawRadians));

    const auto sy = std::sin(yawRadians);
    const auto cy = std::cos(yawRadians);

    const auto pitchRadians = glm::radians(angles[PITCH]);
    assert(!std::isnan(pitchRadians));
    assert(!std::isinf(pitchRadians));

    const auto sp = std::sin(pitchRadians);
    const auto cp = std::cos(pitchRadians);

    return {cp * cy, cp * sy, -sp};
}

inline void AngleVectors(
    const qvec3& angles, qvec3& forward, qvec3& right, qvec3& up) noexcept
{
    const auto yawRadians = glm::radians(angles[YAW]);
    assert(!std::isnan(yawRadians));
    assert(!std::isinf(yawRadians));

    const auto sy = std::sin(yawRadians);
    const auto cy = std::cos(yawRadians);

    const auto pitchRadians = glm::radians(angles[PITCH]);
    assert(!std::isnan(pitchRadians));
    assert(!std::isinf(pitchRadians));

    const auto sp = std::sin(pitchRadians);
    const auto cp = std::cos(pitchRadians);

    const auto rollRadians = glm::radians(angles[ROLL]);
    assert(!std::isnan(rollRadians));
    assert(!std::isinf(rollRadians));

    const auto sr = std::sin(rollRadians);
    const auto cr = std::cos(rollRadians);

    forward[0] = cp * cy;
    forward[1] = cp * sy;
    forward[2] = -sp;

    right[0] = (-1.f * sr * sp * cy + -1.f * cr * -sy);
    right[1] = (-1.f * sr * sp * sy + -1.f * cr * cy);
    right[2] = -1.f * sr * cp;

    up[0] = (cr * sp * cy + -sr * -sy);
    up[1] = (cr * sp * sy + -sr * cy);
    up[2] = cr * cp;
}

int BoxOnPlaneSide(
    const qvec3& emins, const qvec3& emaxs, struct mplane_s* plane);
float anglemod(float a);

[[nodiscard]] qmat3 RotMatFromAngleVector(const qvec3& angles) noexcept;
[[nodiscard]] qvec3 AngleVectorFromRotMat(const qmat3& mat) noexcept;
[[nodiscard]] qmat3 CreateRotMat(const int axis, const qfloat angle) noexcept;

#define BOX_ON_PLANE_SIDE(emins, emaxs, p)                                    \
    (((p)->type < 3) ? (((p)->dist <= (emins)[(p)->type])                     \
                               ? 1                                            \
                               : (((p)->dist >= (emaxs)[(p)->type]) ? 2 : 3)) \
                     : BoxOnPlaneSide((emins), (emaxs), (p)))

#endif /* __MATHLIB_H */
