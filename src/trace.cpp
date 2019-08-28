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
#include "pch.h"
#include "cmdlib.h"
#include "mathlib.h"
#include "bspfile.h"
#include "polylib.h"
#include "blarghrad.h"

#define	ON_EPSILON	0.1

typedef struct tnode_s
{
	int		type;
	vec3_t	normal;
	float	dist;
	int		children[2];
	int		pad;
} tnode_t;

tnode_t		*tnodes, *tnode_p;
void* tnodes_alloc_p;

/*
==============
MakeTnode

Converts the disk node structure into the efficient tracing structure
==============
*/
void MakeTnode (int nodenum)
{
	tnode_t			*t;
	dplane_t		*plane;
	int				i;
	dnode_t 		*node;
	
	t = tnode_p++;

	node = dnodes + nodenum;
	plane = dplanes + node->planenum;

	t->type = plane->type;
	VectorCopy (plane->normal, t->normal);
	t->dist = plane->dist;
	
	for (i=0 ; i<2 ; i++)
	{
		if (node->children[i] < 0)
			t->children[i] = (dleafs[-node->children[i] - 1].contents & CONTENTS_SOLID) | (1<<31);
		else
		{
			t->children[i] = tnode_p - tnodes;
			MakeTnode (node->children[i]);
		}
	}
			
}


/*
=============
MakeTnodes

Loads the node structure out of a .bsp file to be used for light occlusion
=============
*/
void MakeTnodes (dmodel_t *bm)
{
    // TODO - special alignment probably not needed, can simplify this...
	// 32 byte align the structs
    int size = (numnodes + 1) * sizeof(tnode_t);
    tnodes_alloc_p = (void*)malloc(size);
    if (!tnodes_alloc_p) {
        Error("MakeTnodes MALLOC failed!  Could not allocate %d bytes.", size);
    }
	tnodes = (tnode_t *) (( (intptr_t)tnodes_alloc_p + 31)&~31);
	tnode_p = tnodes;

	MakeTnode (0);
}

void FreeTnodes()
{
    if (!tnodes_alloc_p) {
        Error("FreeTnodes after free.\n");
    }
    free(tnodes_alloc_p);
    tnodes_alloc_p = nullptr;
    tnodes = nullptr;
}

//==========================================================


int TestLine_r (int node, const vec3_t& start, const vec3_t& stop, vec3_t* out_vec)
{
	tnode_t	*tnode;
	float	front, back;
	vec3_t	mid;
	float	frac;
	int		side;
	int		r;

    if (node & (1 << 31)) {
        if (out_vec) {
            VectorCopy(start, (*out_vec));
        }
        return node & ~(1 << 31);	// leaf node
    }

	tnode = &tnodes[node];
	switch (tnode->type)
	{
	case PLANE_X:
		front = start.x - tnode->dist;
		back = stop.x - tnode->dist;
		break;
	case PLANE_Y:
		front = start.y - tnode->dist;
		back = stop.y - tnode->dist;
		break;
	case PLANE_Z:
		front = start.z - tnode->dist;
		back = stop.z - tnode->dist;
		break;
	default:
		front = (start.x*tnode->normal.x + start.y*tnode->normal.y + start.z*tnode->normal.z) - tnode->dist;
		back = (stop.x*tnode->normal.x + stop.y*tnode->normal.y + stop.z*tnode->normal.z) - tnode->dist;
		break;
	}

	if (front >= -ON_EPSILON && back >= -ON_EPSILON)
		return TestLine_r (tnode->children[0], start, stop, out_vec);
	
	if (front < ON_EPSILON && back < ON_EPSILON)
		return TestLine_r (tnode->children[1], start, stop, out_vec);

	side = front < 0;
	
	frac = front / (front-back);

	mid.x = start.x + (stop.x - start.x)*frac;
	mid.y = start.y + (stop.y - start.y)*frac;
	mid.z = start.z + (stop.z - start.z)*frac;

	r = TestLine_r (tnode->children[side], start, mid, out_vec);
	if (r)
		return r;
	return TestLine_r (tnode->children[!side], mid, stop, out_vec);
}

int TestLine (const vec3_t& start, const vec3_t& stop, vec3_t* out_vec)
{
	return TestLine_r (0, start, stop, out_vec);
}


void SampleShadowColor(shadowfaces_unk_t *shfunk, const vec3_t& param_2, vec3_t *out_color, float *out_param_4)
{
    float fVar1;
    float fVar2;
    float fVar3;
    float fVar4;
    float fVar5;
    float fVar6;
    float fVar7;
    float fVar22;

    int tx = dfaces[shfunk->face].texinfo;
    fVar22 = texinfo[tx].vecs[0][0] * param_2.x +
        texinfo[tx].vecs[0][2] * param_2.z + texinfo[tx].vecs[0][1] * param_2.y +
        texinfo[tx].vecs[0][3];

    if (shadowfilter != 0) {

        // bilinear filtering on shadowface projections

        fVar22 = fVar22 - 0.5f;
        fVar3 = (texinfo[tx].vecs[1][2] * param_2.z +
            texinfo[tx].vecs[1][1] * param_2.y + texinfo[tx].vecs[1][0] * param_2.x +
            texinfo[tx].vecs[1][3]) - 0.5f;

        fVar1 = fVar22 - floor(fVar22);
        fVar2 = fVar3 - floor(fVar3);

        fVar22 = 1 - fVar1;
        fVar3 = 1 - fVar2;

        int x = (int)floor(fVar22) % shfunk->projtex->width;
        int y = (int)floor(fVar3) % shfunk->projtex->height;
        int x2 = x + 1;
        int y2 = y + 1;
        if (x < 0) {
            x += shfunk->projtex->width;
        }
        if (y < 0) {
            y += shfunk->projtex->height;
        }
        if (x2 < 0) {
            x2 += shfunk->projtex->width;
        }
        if (y2 < 0) {
            y2 += shfunk->projtex->height;
        }
        int row2 = shfunk->projtex->width * y2;
        int row = shfunk->projtex->width * y;

        int nw = (row + x);
        int ne = (row + x2);
        int sw = (row2 + x);
        int se = (row2 + x2);

        struct rgba_t { byte R, G, B, A; };
        rgba_t* p = (rgba_t*)shfunk->projtex->texture32;

        out_color->x = ((p[sw].R * fVar22 + p[se].R * fVar1) * fVar2 + 
                        (p[nw].R * fVar22 + p[ne].R * fVar1) * fVar3) / 255.f;
        out_color->y = ((p[sw].G * fVar22 + p[se].G * fVar1) * fVar2 +
                        (p[nw].G * fVar22 + p[ne].G * fVar1) * fVar3) / 255.f;
        out_color->z = ((p[sw].B * fVar22 + p[se].B * fVar1) * fVar2 +
                        (p[nw].B * fVar22 + p[ne].B * fVar1) * fVar3) / 255.f;
        *out_param_4 = ((p[sw].A * fVar22 + p[se].A * fVar1) * fVar2 +
                        (p[nw].A * fVar22 + p[ne].A * fVar1) * fVar3) / 255.f;
        return;
    }


    fVar1 = texinfo[tx].vecs[1][0];
    fVar2 = param_2.x;
    fVar3 = texinfo[tx].vecs[1][1];
    fVar4 = param_2.y;
    fVar5 = texinfo[tx].vecs[1][2];
    fVar6 = param_2.z;
    fVar7 = texinfo[tx].vecs[1][3];
    
    int x = (int)floor(fVar22) % shfunk->projtex->width;
    if (x < 0) {
        x += shfunk->projtex->width;
    }

    int y = (int)floor((double)(fVar5 * fVar6 + fVar3 * fVar4 + fVar1 * fVar2 + fVar7)) % shfunk->projtex->height;
    if (y < 0) {
        y += shfunk->projtex->height;
    }

    int i = (shfunk->projtex->width * y + x) * 4;

    out_color->x = shfunk->projtex->texture32[i + 0] / 255.f;
    out_color->y = shfunk->projtex->texture32[i + 1] / 255.f;
    out_color->z = shfunk->projtex->texture32[i + 2] / 255.f;
    *out_param_4 = shfunk->projtex->texture32[i + 3] / 255.f;
}


int TestLine_shadowfunk(shadowfaces_unk_t *shfunk, shadowmodel_t *shmod, const vec3_t& mins, const vec3_t& maxs,
    const vec3_t& length, const vec3_t& start, const vec3_t& stop, vec3_t *out_param_8, vec3_t *out_param_9)
{
    float fVar1;
    float fVar2;
    float fVar3;
    float fVar6;
    float fVar7;
    vec3_t color;
    vec3_t local_48;
    vec3_t local_3c;
    vec3_t tmp, tmp2, tmp3;
    float local_30;
    float local_2c;
    float local_24;
    float local_20;
    float local_10;
    float local_c;

    if ((shfunk->maxs.x <= mins.x) || (shfunk->maxs.y <= mins.y) || (shfunk->maxs.z <= mins.z) ||
        (shfunk->mins.x >= maxs.x) || (shfunk->mins.y >= maxs.y) || (shfunk->mins.z >= maxs.z)) {
        return 0;
    }

    int planenum = dfaces[shfunk->face].planenum;
    dplane_t* pln;
    if (dfaces[shfunk->face].side == 0) {
        pln = &dplanes[planenum];
    }
    else {
        pln = &backplanes[planenum];
    }
    if (0 <= DotProduct(pln->normal, length)) {
        return 0;
    }
    float dstart = DotProduct(pln->normal, start) - pln->dist;
    float dstop = DotProduct(pln->normal, stop) - pln->dist;
    if (0 <= dstop * dstart) {
        return 0;
    }

    int iVar11 = dfaces[shfunk->face].numedges;
    int i = 0;
    int* psurfedge = &dsurfedges[dfaces[shfunk->face].firstedge];
    for ( ; i < dfaces[shfunk->face].numedges; i++) {
        int edge = *psurfedge++;
        if (edge < 0) {
            int v = dedges[-edge].v[0];
            VectorCopy(dvertexes[v].point, tmp);
            v = dedges[-edge].v[1];
            VectorCopy(dvertexes[v].point, local_48);
        }
        else {
            int v = dedges[edge].v[0];
            VectorCopy(dvertexes[v].point, local_48);
            v = dedges[edge].v[1];
            VectorCopy(dvertexes[v].point, tmp);
        }
        VectorSubtract(start, local_48, tmp2);
        VectorSubtract(stop, local_48, tmp3);
        tmp.x = tmp.x - local_48.x;
        local_24 = tmp2.y * (tmp.z - local_48.z) - (tmp.y - local_48.y) * tmp2.z;
        local_20 = tmp.x * tmp2.z - (tmp.z - local_48.z) * tmp2.x;
        tmp.y = (tmp.y - local_48.y) * tmp2.x - tmp.x * tmp2.y;
        fVar2 = local_24 * tmp3.x + local_20 * tmp3.y + tmp3.z * tmp.y;

        /*
                             ------- if ---------
                             ST1 => fVar1
                             ST0 => fVar2
        00403f88 060           d8 15 cc        FCOM       dword ptr [FLOAT_ZERO]                           = 0.0
                               71 43 00
        00403f8e 060           df e0           FNSTSW     AX
        00403f90 060           25 00 41        AND        EAX,0x4100
                               00 00
                             jump taken = break
        00403f95 060           74 56           JZ         LAB_00403fed
        00403f97 060           d8 1d cc        FCOMP      dword ptr [FLOAT_ZERO]                           = 0.0
                               71 43 00
        00403f9d 060           df e0           FNSTSW     AX
        00403f9f 060           f6 c4 44        TEST       AH,0x44
                             jump taken = continue
        00403fa2 060           7a 31           JP         LAB_00403fd5
        00403fa4 060           d9 44 24 40     FLD        dword ptr [ESP + local_20]
        00403fa8 060           d8 0d c8        FMUL       dword ptr [FLOAT_004371c8]                       = 0.7419
                               71 43 00
        00403fae 060           d9 44 24 3c     FLD        dword ptr [ESP + local_24]
        00403fb2 060           d8 0d c4        FMUL       dword ptr [FLOAT_004371c4]                       = 0.5416
                               71 43 00
        00403fb8 060           de c1           FADDP
        00403fba 060           d9 c9           FXCH
        00403fbc 060           d8 0d c0        FMUL       dword ptr [FLOAT_004371c0]                       = 0.3953
                               71 43 00
        00403fc2 060           de e9           FSUBP
        00403fc4 060           d8 1d cc        FCOMP      dword ptr [FLOAT_ZERO]                           = 0.0
                               71 43 00
        00403fca 060           df e0           FNSTSW     AX
        00403fcc 060           25 00 01        AND        EAX,0x100
                               00 00
                             ------- if body: jump taken = break -------
        00403fd1 060           74 1e           JZ         LAB_00403ff1
                             continue...
        00403fd3 060           eb 02           JMP        LAB_00403fd7

        */

        // TODO BUG - fVar2 <= 0 prevents fVar2 == 0 ...
        if (fVar2 <= 0 || (fVar2 == 0 && (0 <= (local_24 * 0.54159999 + local_20 * 0.74190003) - tmp.y * 0.39530000))) {
            break;
        }
    }

    if (i != dfaces[shfunk->face].numedges) {
        return 0;
    }
    fVar1 = dstart / (dstart - dstop);
    local_3c.x = (stop.x - start.x) * fVar1 + start.x;
    local_3c.y = (stop.y - start.y) * fVar1 + start.y;
    local_3c.z = (stop.z - start.z) * fVar1 + start.z;

    float trans;
    if (shfunk->UNKNOWN_FIELD_0xC == 0) {
        if ((shmod->nonTransFaces == 1) &&
            (!shfunk->projtex || (shfunk->projtex->has_transparent_pixels == 0)))
        {
            VectorClear((*out_param_9));
            goto LAB_00404265;
        }
        trans = 1;
    }
    else {
        if ((texinfo[dfaces[shfunk->face].texinfo].flags & SURF_TRANS33) == 0) {
            trans = 0.66f;
        }
        else {
            trans = 0.33f;
        }
    }
    if ((shfunk->projtex && (shfunk->projtex->has_transparent_pixels != 0)) || (shfunk->maybe_bool != 0))
    {
        SampleShadowColor(shfunk, local_3c, &color, &dstart);
    }
    if (shfunk->projtex && (shfunk->projtex->has_transparent_pixels != 0)) {
        trans = (dstart * trans);
        if (trans == 0)
            trans = 1.f / (255 * 10);
    }
    if (shfunk->maybe_bool == 0) {
        int c = shfunk->UNKNOWN_FIELD_0xC == 0 ? shmod->nonTransFaces : shmod->transFaces;
        if (c == 2) {
            VectorCopy(texture_reflectivity[dfaces[shfunk->face].texinfo], color);
        }
        else {
            VectorClear(color);
        }
    }
    else {
        VectorScale(color, g_texscale, color);
        if ((color.x > 1) || (color.y > 1) || (color.z > 1)) {
            ColorNormalize(color, color);
        }
    }
    fVar1 = 1 - trans;
    out_param_9->x = out_param_9->x * fVar1 + color.x * trans * out_param_9->x;
    out_param_9->y = out_param_9->y * fVar1 + color.y * trans * out_param_9->y;
    out_param_9->z = out_param_9->z * fVar1 + color.z * trans * out_param_9->z;
    if (((out_param_9->x != 0) || (out_param_9->y != 0)) || (out_param_9->z != 0)) {
        return 0;
    }

LAB_00404265:
    if (out_param_8) {
        VectorCopy(local_3c, (*out_param_8));
    }
    return 1;
}


int TestLine_shadowmodel(const vec3_t& start, const vec3_t& stop, vec3_t* out_vec3, vec3_t* out_param_4)
{
    shadowfaces_unk_t *shfunk;
    shadowmodel_t *shmod;
    vec3_t maxs;
    vec3_t mins;
    vec3_t local_c;

    mins.x = mins.y = mins.z = 99999.f;
    maxs.x = maxs.y = maxs.z = -99999.f;

    AddPointToBounds(stop, mins, maxs);
    AddPointToBounds(start, mins, maxs);
    local_c.x = stop.x - start.x;
    local_c.y = stop.y - start.y;
    local_c.z = stop.z - start.z;

    for (shmod = g_shadow_world; shmod; shmod = shmod->next)
    {
        int n = shmod->modelnum;
        if ((n == 0) ||
            ((mins.x < dmodels[n].maxs.x) && (mins.y < dmodels[n].maxs.y) && (mins.z < dmodels[n].maxs.z) &&
            (maxs.x > dmodels[n].mins.x) && (maxs.y < dmodels[n].mins.y) && (maxs.z < dmodels[n].mins.z)))
        {
            for (shfunk = shmod->shadownext; shfunk; shfunk = shfunk->next) {
                if (TestLine_shadowfunk(shfunk, shmod, mins, maxs, local_c, start, stop, out_vec3, out_param_4)) {
                    return -1;
                }
            }
        }
    }
}

int TestLine_shadow(const vec3_t& start, const vec3_t& stop, vec3_t *out_param_3, vec3_t *optional_out_vec)
{
    vec3_t local_c;

    local_c.x = 1;
    local_c.y = 1;
    local_c.z = 1;
    int result = TestLine_r(0, start, stop, out_param_3);
    if (result == 0) {
        if (g_shadow_world) {
            result = TestLine_shadowmodel(start, stop, out_param_3, &local_c);
        }
    }
    else {
        local_c.z = 0;
        local_c.y = 0;
        local_c.x = 0;
    }
    if (optional_out_vec) {
        optional_out_vec->x = local_c.x;
        optional_out_vec->y = local_c.y;
        optional_out_vec->z = local_c.z;
    }
    return result;
}



/*
==============================================================================

LINE TRACING

The major lighting operation is a point to point visibility test, performed
by recursive subdivision of the line by the BSP tree.

==============================================================================
*/

typedef struct
{
	vec3_t	backpt;
	int		side;
	int		node;
} tracestack_t;


/*
==============
TestLine
==============
*/
qboolean _TestLine (vec3_t start, vec3_t stop)
{
	int				node;
	float			front, back;
	tracestack_t	*tstack_p;
	int				side;
	float 			frontx,fronty, frontz, backx, backy, backz;
	tracestack_t	tracestack[64];
	tnode_t			*tnode;
	
	frontx = start.x;
	fronty = start.y;
	frontz = start.z;
	backx = stop.x;
	backy = stop.y;
	backz = stop.z;
	
	tstack_p = tracestack;
	node = 0;
	
	while (1)
	{
		if (node == CONTENTS_SOLID)
		{
#if 0
			float	d1, d2, d3;

			d1 = backx - frontx;
			d2 = backy - fronty;
			d3 = backz - frontz;

			if (d1*d1 + d2*d2 + d3*d3 > 1)
#endif
				return false;	// DONE!
		}
		
		while (node < 0)
		{
		// pop up the stack for a back side
			tstack_p--;
			if (tstack_p < tracestack)
				return true;
			node = tstack_p->node;
			
		// set the hit point for this plane
			
			frontx = backx;
			fronty = backy;
			frontz = backz;
			
		// go down the back side

			backx = tstack_p->backpt.x;
			backy = tstack_p->backpt.y;
			backz = tstack_p->backpt.z;
			
			node = tnodes[tstack_p->node].children[!tstack_p->side];
		}

		tnode = &tnodes[node];
		
		switch (tnode->type)
		{
		case PLANE_X:
			front = frontx - tnode->dist;
			back = backx - tnode->dist;
			break;
		case PLANE_Y:
			front = fronty - tnode->dist;
			back = backy - tnode->dist;
			break;
		case PLANE_Z:
			front = frontz - tnode->dist;
			back = backz - tnode->dist;
			break;
		default:
			front = (frontx*tnode->normal.x + fronty*tnode->normal.y + frontz*tnode->normal.z) - tnode->dist;
			back = (backx*tnode->normal.x + backy*tnode->normal.y + backz*tnode->normal.z) - tnode->dist;
			break;
		}

		if (front > -ON_EPSILON && back > -ON_EPSILON)
//		if (front > 0 && back > 0)
		{
			node = tnode->children[0];
			continue;
		}
		
		if (front < ON_EPSILON && back < ON_EPSILON)
//		if (front <= 0 && back <= 0)
		{
			node = tnode->children[1];
			continue;
		}

		side = front < 0;
		
		front = front / (front-back);
	
		tstack_p->node = node;
		tstack_p->side = side;
		tstack_p->backpt.x = backx;
		tstack_p->backpt.y = backy;
		tstack_p->backpt.z = backz;
		
		tstack_p++;
		
		backx = frontx + front*(backx-frontx);
		backy = fronty + front*(backy-fronty);
		backz = frontz + front*(backz-frontz);
		
		node = tnode->children[side];		
	}	
}

