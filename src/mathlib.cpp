/*
===========================================================================
Copyright (C) 1997-2006 Id Software, Inc.

This file is part of Quake 2 Tools source code.

Quake 2 Tools source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake 2 Tools source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake 2 Tools source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

// mathlib.c -- math primitives
#include "pch.h"
#include "cmdlib.h"
#include "mathlib.h"

vec3_t vec3_origin = { 0,0,0 };


float VectorLength(vec3_t v)
{
    int		i;
    double	length;

    length = v.x*v.x + v.y*v.y + v.z*v.z;
    length = sqrt(length);		// FIXME

    return length;
}

qboolean VectorCompare(const vec3_t& v1, const vec3_t& v2)
{
    if (fabs(v1.x - v2.x) > EQUAL_EPSILON || 
        fabs(v1.y - v2.y) > EQUAL_EPSILON || 
        fabs(v1.z - v2.z) > EQUAL_EPSILON)
        return false;

    return true;
}

vec_t Q_rint(vec_t in)
{
    return floor(in + 0.5);
}

void VectorMA(const vec3_t& ofs, double scale, const vec3_t& val, vec3_t& out)
{
    out.x = val.x * (float)scale + ofs.x;
    out.y = val.y * (float)scale + ofs.y;
    out.z = val.z * (float)scale + ofs.z;
}

void CrossProduct(vec3_t v1, vec3_t v2, vec3_t& cross)
{
    cross.x = v1.y * v2.z - v1.z * v2.y;
    cross.y = v1.z * v2.x - v1.x * v2.z;
    cross.z = v1.x * v2.y - v1.y * v2.x;
}

vec_t _DotProduct(vec3_t v1, vec3_t v2)
{
    return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
}

void _VectorSubtract(vec3_t va, vec3_t vb, vec3_t& out)
{
    out.x = va.x - vb.x;
    out.y = va.y - vb.y;
    out.z = va.z - vb.z;
}

void _VectorAdd(vec3_t va, vec3_t vb, vec3_t& out)
{
    out.x = va.x + vb.x;
    out.y = va.y + vb.y;
    out.z = va.z + vb.z;
}

void _VectorCopy(vec3_t in, vec3_t& out)
{
    out.x = in.x;
    out.y = in.y;
    out.z = in.z;
}

void _VectorScale(vec3_t v, vec_t scale, vec3_t& out)
{
    out.x = v.x * scale;
    out.y = v.y * scale;
    out.z = v.z * scale;
}

vec_t VectorNormalize(vec3_t in, vec3_t& out)
{
    vec_t	length, ilength;

    length = sqrt(in.x*in.x + in.y*in.y + in.z*in.z);
    if (length == 0)
    {
        VectorClear(out);
        return 0;
    }

    ilength = 1.0 / length;
    out.x = in.x * ilength;
    out.y = in.y * ilength;
    out.z = in.z * ilength;

    return length;
}

vec_t ColorNormalize(vec3_t in, vec3_t& out)
{
    float	max, scale;

    max = in.x;
    if (in.y > max)
        max = in.y;
    if (in.z > max)
        max = in.z;

    if (max == 0)
        return 0;

    scale = 1.0 / max;

    VectorScale(in, scale, out);

    return max;
}



void VectorInverse(vec3_t& v)
{
    v.x = -v.x;
    v.y = -v.y;
    v.z = -v.z;
}

void ClearBounds(vec3_t& mins, vec3_t& maxs)
{
    mins.x = mins.y = mins.z = 99999;
    maxs.x = maxs.y = maxs.z = -99999;
}

void AddPointToBounds(vec3_t v, vec3_t& mins, vec3_t& maxs)
{
    float val = v.x;
    if (val < mins.x)
        mins.x = val;
    if (val > maxs.x)
        maxs.x = val;

    val = v.y;
    if (val < mins.y)
        mins.y = val;
    if (val > maxs.y)
        maxs.y = val;

    val = v.z;
    if (val < mins.z)
        mins.z = val;
    if (val > maxs.z)
        maxs.z = val;
}