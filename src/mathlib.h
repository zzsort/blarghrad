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

#ifndef __MATHLIB__
#define __MATHLIB__

// mathlib.h

#define	SIDE_FRONT		0
#define	SIDE_ON			2
#define	SIDE_BACK		1
#define	SIDE_CROSS		-2

#define	Q_PI	3.14159265358979323846

extern vec3_t vec3_origin;

#define	EQUAL_EPSILON	0.001

qboolean VectorCompare(const vec3_t& v1, const vec3_t& v2);

#define DotProduct(v0,v1) (v0.x*v1.x+v0.y*v1.y+v0.z*v1.z)
#define VectorSubtract(a,b,c) {c.x=a.x-b.x;c.y=a.y-b.y;c.z=a.z-b.z;}
#define VectorAdd(a,b,c) {c.x=a.x+b.x;c.y=a.y+b.y;c.z=a.z+b.z;}
#define VectorCopy(a,b) {b.x=a.x;b.y=a.y;b.z=a.z;}
#define VectorScale(a,b,c) {c.x=b*a.x;c.y=b*a.y;c.z=b*a.z;}
#define VectorClear(v) {v={0,0,0};}
#define	VectorNegate(a) {a.x=-a.x;a.y=-a.y;a.z=-a.z;}

vec_t Q_rint(vec_t in);
vec_t _DotProduct(vec3_t v1, vec3_t v2);
void _VectorSubtract(vec3_t va, vec3_t vb, vec3_t& out);
void _VectorAdd(vec3_t va, vec3_t vb, vec3_t& out);
void _VectorCopy(vec3_t in, vec3_t& out);
void _VectorScale(vec3_t v, vec_t scale, vec3_t& out);

float VectorLength(vec3_t v);

void VectorMA(const vec3_t& ofs, double scale, const vec3_t& val, vec3_t& out);

void CrossProduct(const vec3_t& v1, const vec3_t& v2, vec3_t& cross);
vec_t VectorNormalize(vec3_t in, vec3_t& out);
vec_t ColorNormalize(vec3_t in, vec3_t& out);
void VectorInverse(vec3_t& v);

void ClearBounds(vec3_t& mins, vec3_t& maxs);
void AddPointToBounds(vec3_t v, vec3_t& mins, vec3_t& maxs);

#endif
