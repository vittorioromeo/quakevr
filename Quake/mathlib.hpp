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
#include "glm.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846 // matches value in gcc v2 math.h
#endif

#define M_PI_DIV_180 (M_PI / 180.0) // johnfitz

struct mplane_s;

inline constexpr glm::vec3 vec3_origin{0.f, 0.f, 0.f};

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

// johnfitz -- courtesy of lordhavoc
// QuakeSpasm: To avoid strict aliasing violations, use a float/int union
// instead of type punning.
#define VectorNormalizeFast(_v)                                      \
    {                                                                \
        union                                                        \
        {                                                            \
            float f;                                                 \
            int i;                                                   \
        } _y, _number;                                               \
        _number.f = DotProduct(_v, _v);                              \
        if(_number.f != 0.0)                                         \
        {                                                            \
            _y.i = 0x5f3759df - (_number.i >> 1);                    \
            _y.f = _y.f * (1.5f - (_number.f * 0.5f * _y.f * _y.f)); \
            VectorScale(_v, _y.f, _v);                               \
        }                                                            \
    }

[[nodiscard]] glm::vec3 VectorAngles(const glm::vec3& forward) noexcept; // johnfitz

vec_t VectorLength(vec3_t v);

template <typename T>
void VectorScale(const T& in, const float scale, glm::vec3& out)
{
    out[0] = in[0] * scale;
    out[1] = in[1] * scale;
    out[2] = in[2] * scale;
}

[[nodiscard]] glm::mat3 R_ConcatRotations(
    const glm::mat3& in1, const glm::mat3& in2) noexcept;
void R_ConcatTransforms(float in1[3][4], float in2[3][4], float out[3][4]);

template <typename T>
void AngleVectors(const T& angles, T& forward, T& right, T& up)
{
    float angle = angles[YAW] * (M_PI * 2 / 360);
    assert(!std::isnan(angle));
    assert(!std::isinf(angle));

    const float sy = std::sin(angle);
    const float cy = std::cos(angle);

    angle = angles[PITCH] * (M_PI * 2 / 360);
    assert(!std::isnan(angle));
    assert(!std::isinf(angle));

    const float sp = std::sin(angle);
    const float cp = std::cos(angle);

    angle = angles[ROLL] * (M_PI * 2 / 360);
    assert(!std::isnan(angle));
    assert(!std::isinf(angle));

    const float sr = std::sin(angle);
    const float cr = std::cos(angle);

    forward[0] = cp * cy;
    forward[1] = cp * sy;
    forward[2] = -sp;

    right[0] = (-1 * sr * sp * cy + -1 * cr * -sy);
    right[1] = (-1 * sr * sp * sy + -1 * cr * cy);
    right[2] = -1 * sr * cp;

    up[0] = (cr * sp * cy + -sr * -sy);
    up[1] = (cr * sp * sy + -sr * cy);
    up[2] = cr * cp;
}

int BoxOnPlaneSide(const glm::vec3& emins, const glm::vec3& emaxs, struct mplane_s* plane);
float anglemod(float a);

[[nodiscard]] glm::mat3 RotMatFromAngleVector(const glm::vec3& angles) noexcept;
[[nodiscard]] glm::vec3 AngleVectorFromRotMat(const glm::mat3& mat) noexcept;
[[nodiscard]] glm::mat3 CreateRotMat(
    const int axis, const float angle) noexcept;
void CreateRotMat(int axis, float angle, vec3_t mat[3]);

#define BOX_ON_PLANE_SIDE(emins, emaxs, p)                                    \
    (((p)->type < 3) ? (((p)->dist <= (emins)[(p)->type])                     \
                               ? 1                                            \
                               : (((p)->dist >= (emaxs)[(p)->type]) ? 2 : 3)) \
                     : BoxOnPlaneSide((emins), (emaxs), (p)))

#endif /* __MATHLIB_H */
