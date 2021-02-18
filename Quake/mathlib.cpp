/*
Copyright (C) 1996-2001 Id Software, Inc.
Copyright (C) 2002-2009 John Fitzgibbons and others
Copyright (C) 2007-2008 Kristian Duske
Copyright (C) 2010-2014 QuakeSpasm developers
Copyright (C) 2020-2021 Vittorio Romeo

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
// mathlib.c -- math primitives

#include "quakedef.hpp"
#include "util.hpp"
#include "gl_model.hpp"
#include "quakeglm_qmat3.hpp"
#include "sys.hpp"

#include <cstring>

/*-----------------------------------------------------------------*/


//#define DEG2RAD( a ) ( a * M_PI ) / 180.0F
#define DEG2RAD(a) ((a)*M_PI_DIV_180) // johnfitz

qvec3 ProjectPointOnPlane(const qvec3& p, const qvec3& normal)
{
    float d;
    qvec3 n;
    float inv_denom;

    inv_denom = 1.0F / DotProduct(normal, normal);

    d = DotProduct(normal, p) * inv_denom;

    n[0] = normal[0] * inv_denom;
    n[1] = normal[1] * inv_denom;
    n[2] = normal[2] * inv_denom;

    qvec3 dst;
    dst[0] = p[0] - d * n[0];
    dst[1] = p[1] - d * n[1];
    dst[2] = p[2] - d * n[2];
    return dst;
}

/*
** assumes "src" is normalized
*/
qvec3 PerpendicularVector(const qvec3& src)
{
    int pos;
    int i;
    float minelem = 1.0F;
    qvec3 tempvec;

    /*
    ** find the smallest magnitude axially aligned vector
    */
    for(pos = 0, i = 0; i < 3; i++)
    {
        if(fabs(src[i]) < minelem)
        {
            pos = i;
            minelem = fabs(src[i]);
        }
    }
    tempvec[0] = tempvec[1] = tempvec[2] = 0.0F;
    tempvec[pos] = 1.0F;

    /*
    ** project the point onto the plane defined by src
    */
    return glm::normalize(ProjectPointOnPlane(tempvec, src));
}

// johnfitz -- removed RotatePointAroundVector() becuase it's no longer used and
// my compiler fucked it up anyway

// spike -- readded, because it is useful, and my version of gcc has never had a
// problem with it.
[[nodiscard]] qvec3 RotatePointAroundVector(
    const qvec3& dir, const qvec3& point, const float degrees)
{
    qmat3 m;
    qmat3 im;
    qmat3 zrot;
    qmat3 tmpmat;
    qmat3 rot;
    int i;
    qvec3 vr, vup, vf;

    vf[0] = dir[0];
    vf[1] = dir[1];
    vf[2] = dir[2];

    vr = PerpendicularVector(dir);
    vup = CrossProduct(vr, vf);

    m[0][0] = vr[0];
    m[1][0] = vr[1];
    m[2][0] = vr[2];

    m[0][1] = vup[0];
    m[1][1] = vup[1];
    m[2][1] = vup[2];

    m[0][2] = vf[0];
    m[1][2] = vf[1];
    m[2][2] = vf[2];

    memcpy((void*)&im, (const void*)&m, sizeof(im));

    im[0][1] = m[1][0];
    im[0][2] = m[2][0];
    im[1][0] = m[0][1];
    im[1][2] = m[2][1];
    im[2][0] = m[0][2];
    im[2][1] = m[1][2];

    memset((void*)&zrot, 0, sizeof(zrot));
    zrot[0][0] = zrot[1][1] = zrot[2][2] = 1.0F;

    zrot[0][0] = cos(DEG2RAD(degrees));
    zrot[0][1] = sin(DEG2RAD(degrees));
    zrot[1][0] = -sin(DEG2RAD(degrees));
    zrot[1][1] = cos(DEG2RAD(degrees));

    tmpmat = R_ConcatRotations(m, zrot);
    rot = R_ConcatRotations(tmpmat, im);

    qvec3 dst;
    for(i = 0; i < 3; i++)
    {
        dst[i] =
            rot[i][0] * point[0] + rot[i][1] * point[1] + rot[i][2] * point[2];
    }
    return dst;
}

/*
==================
BoxOnPlaneSide

Returns 1, 2, or 1 + 2
==================
*/
int BoxOnPlaneSide(const qvec3& emins, const qvec3& emaxs, mplane_t* p)
{
    float dist1;
    float dist2;
    int sides;

#if 0 // this is done by the BOX_ON_PLANE_SIDE macro before calling this
      // function
// fast axial cases
	if (p->type < 3)
	{
		if (p->dist <= emins[p->type])
			return 1;
		if (p->dist >= emaxs[p->type])
			return 2;
		return 3;
	}
#endif

    // general case
    switch(p->signbits)
    {
        case 0:
            dist1 = p->normal[0] * emaxs[0] + p->normal[1] * emaxs[1] +
                    p->normal[2] * emaxs[2];
            dist2 = p->normal[0] * emins[0] + p->normal[1] * emins[1] +
                    p->normal[2] * emins[2];
            break;
        case 1:
            dist1 = p->normal[0] * emins[0] + p->normal[1] * emaxs[1] +
                    p->normal[2] * emaxs[2];
            dist2 = p->normal[0] * emaxs[0] + p->normal[1] * emins[1] +
                    p->normal[2] * emins[2];
            break;
        case 2:
            dist1 = p->normal[0] * emaxs[0] + p->normal[1] * emins[1] +
                    p->normal[2] * emaxs[2];
            dist2 = p->normal[0] * emins[0] + p->normal[1] * emaxs[1] +
                    p->normal[2] * emins[2];
            break;
        case 3:
            dist1 = p->normal[0] * emins[0] + p->normal[1] * emins[1] +
                    p->normal[2] * emaxs[2];
            dist2 = p->normal[0] * emaxs[0] + p->normal[1] * emaxs[1] +
                    p->normal[2] * emins[2];
            break;
        case 4:
            dist1 = p->normal[0] * emaxs[0] + p->normal[1] * emaxs[1] +
                    p->normal[2] * emins[2];
            dist2 = p->normal[0] * emins[0] + p->normal[1] * emins[1] +
                    p->normal[2] * emaxs[2];
            break;
        case 5:
            dist1 = p->normal[0] * emins[0] + p->normal[1] * emaxs[1] +
                    p->normal[2] * emins[2];
            dist2 = p->normal[0] * emaxs[0] + p->normal[1] * emins[1] +
                    p->normal[2] * emaxs[2];
            break;
        case 6:
            dist1 = p->normal[0] * emaxs[0] + p->normal[1] * emins[1] +
                    p->normal[2] * emins[2];
            dist2 = p->normal[0] * emins[0] + p->normal[1] * emaxs[1] +
                    p->normal[2] * emaxs[2];
            break;
        case 7:
            dist1 = p->normal[0] * emins[0] + p->normal[1] * emins[1] +
                    p->normal[2] * emins[2];
            dist2 = p->normal[0] * emaxs[0] + p->normal[1] * emaxs[1] +
                    p->normal[2] * emaxs[2];
            break;
        default:
            dist1 = dist2 = 0; // shut up compiler
            Sys_Error("BoxOnPlaneSide:  Bad signbits");
            break;
    }

#if 0
	int		i;
	qvec3	corners[2];

	for (i=0 ; i<3 ; i++)
	{
		if (plane->normal[i] < 0)
		{
			corners[0][i] = emins[i];
			corners[1][i] = emaxs[i];
		}
		else
		{
			corners[1][i] = emins[i];
			corners[0][i] = emaxs[i];
		}
	}
	dist = DotProduct (plane->normal, corners[0]) - plane->dist;
	dist2 = DotProduct (plane->normal, corners[1]) - plane->dist;
	sides = 0;
	if (dist1 >= 0)
		sides = 1;
	if (dist2 < 0)
		sides |= 2;
#endif

    sides = 0;
    if(dist1 >= p->dist)
    {
        sides = 1;
    }
    if(dist2 < p->dist)
    {
        sides |= 2;
    }

#ifdef PARANOID
    if(sides == 0) Sys_Error("BoxOnPlaneSide: sides==0");
#endif

    return sides;
}

// johnfitz -- the opposite of AngleVectors.  this takes forward and generates
// pitch yaw roll
// TODO: take right and up vectors to properly set yaw and roll
[[nodiscard]] qvec3 VectorAngles(const qvec3& forward) noexcept
{
    qvec3 temp, res;

    temp[0] = forward[0];
    temp[1] = forward[1];
    temp[2] = 0;
    res[PITCH] = -atan2(forward[2], glm::length(temp)) / M_PI_DIV_180;
    res[YAW] = atan2(forward[1], forward[0]) / M_PI_DIV_180;
    res[ROLL] = 0;

    return res;
}

float VectorLength(vec3_t v)
{
    return sqrt(DotProduct(v, v));
}

[[nodiscard]] qmat3 RotMatFromAngleVector(const qvec3& angles) noexcept
{
    const auto [fwd, right, up] = quake::util::getAngledVectors(angles);

    qmat3 res;
    res[0] = fwd;
    res[1] = right * -1._qf; // flip y so (0,0,0) produces identity!
    res[2] = up;
    return res;
}

[[nodiscard]] qvec3 CrossProduct(const qvec3& v1, const qvec3& v2)
{
    qvec3 cross;
    cross[0] = v1[1] * v2[2] - v1[2] * v2[1];
    cross[1] = v1[2] * v2[0] - v1[0] * v2[2];
    cross[2] = v1[0] * v2[1] - v1[1] * v2[0];
    return cross;
}

[[nodiscard]] qvec3 AngleVectorFromRotMat(const qmat3& mat) noexcept
{
    qvec3 out;

    out[1] = -atan2(mat[0][0], mat[0][1]) / M_PI_DIV_180 + 90;
    out[0] =
        atan2(sqrt(mat[0][0] * mat[0][0] + mat[0][1] * mat[0][1]), mat[0][2]) /
            M_PI_DIV_180 -
        90;
    out[2] = 0;

    const auto unrolled = RotMatFromAngleVector(out);

    out[2] =
        -atan2(glm::dot(unrolled[1], mat[1]), glm::dot(unrolled[2], mat[1])) /
            M_PI_DIV_180 +
        90;

    return out;
}

[[nodiscard]] qmat3 CreateRotMat(const int axis, const qfloat angle) noexcept
{
    const qvec3 angles{
        axis == 0 ? angle : 0, axis == 1 ? angle : 0, axis == 2 ? angle : 0};

    return RotMatFromAngleVector(angles);
}

/*
================
R_ConcatRotations
================
*/
[[nodiscard]] qmat3 R_ConcatRotations(
    const qmat3& in1, const qmat3& in2) noexcept
{
    qmat3 res;

    res[0][0] =
        in1[0][0] * in2[0][0] + in1[0][1] * in2[1][0] + in1[0][2] * in2[2][0];
    res[0][1] =
        in1[0][0] * in2[0][1] + in1[0][1] * in2[1][1] + in1[0][2] * in2[2][1];
    res[0][2] =
        in1[0][0] * in2[0][2] + in1[0][1] * in2[1][2] + in1[0][2] * in2[2][2];
    res[1][0] =
        in1[1][0] * in2[0][0] + in1[1][1] * in2[1][0] + in1[1][2] * in2[2][0];
    res[1][1] =
        in1[1][0] * in2[0][1] + in1[1][1] * in2[1][1] + in1[1][2] * in2[2][1];
    res[1][2] =
        in1[1][0] * in2[0][2] + in1[1][1] * in2[1][2] + in1[1][2] * in2[2][2];
    res[2][0] =
        in1[2][0] * in2[0][0] + in1[2][1] * in2[1][0] + in1[2][2] * in2[2][0];
    res[2][1] =
        in1[2][0] * in2[0][1] + in1[2][1] * in2[1][1] + in1[2][2] * in2[2][1];
    res[2][2] =
        in1[2][0] * in2[0][2] + in1[2][1] * in2[1][2] + in1[2][2] * in2[2][2];

    return res;
}

/*
================
R_ConcatTransforms
================
*/
void R_ConcatTransforms(float in1[3][4], float in2[3][4], float out[3][4])
{
    out[0][0] =
        in1[0][0] * in2[0][0] + in1[0][1] * in2[1][0] + in1[0][2] * in2[2][0];
    out[0][1] =
        in1[0][0] * in2[0][1] + in1[0][1] * in2[1][1] + in1[0][2] * in2[2][1];
    out[0][2] =
        in1[0][0] * in2[0][2] + in1[0][1] * in2[1][2] + in1[0][2] * in2[2][2];
    out[0][3] = in1[0][0] * in2[0][3] + in1[0][1] * in2[1][3] +
                in1[0][2] * in2[2][3] + in1[0][3];
    out[1][0] =
        in1[1][0] * in2[0][0] + in1[1][1] * in2[1][0] + in1[1][2] * in2[2][0];
    out[1][1] =
        in1[1][0] * in2[0][1] + in1[1][1] * in2[1][1] + in1[1][2] * in2[2][1];
    out[1][2] =
        in1[1][0] * in2[0][2] + in1[1][1] * in2[1][2] + in1[1][2] * in2[2][2];
    out[1][3] = in1[1][0] * in2[0][3] + in1[1][1] * in2[1][3] +
                in1[1][2] * in2[2][3] + in1[1][3];
    out[2][0] =
        in1[2][0] * in2[0][0] + in1[2][1] * in2[1][0] + in1[2][2] * in2[2][0];
    out[2][1] =
        in1[2][0] * in2[0][1] + in1[2][1] * in2[1][1] + in1[2][2] * in2[2][1];
    out[2][2] =
        in1[2][0] * in2[0][2] + in1[2][1] * in2[1][2] + in1[2][2] * in2[2][2];
    out[2][3] = in1[2][0] * in2[0][3] + in1[2][1] * in2[1][3] +
                in1[2][2] * in2[2][3] + in1[2][3];
}
