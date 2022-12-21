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

#define	ON_EPSILON	0.01

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

    CHKVAL("MakeTnode", nodenum);

	t = tnode_p++;

	node = dnodes + nodenum;
	plane = dplanes + node->planenum;

	t->type = plane->type;
	VectorCopy (plane->normal, t->normal);
	t->dist = plane->dist;
	
	for (i=0 ; i<2 ; i++)
	{
        if (node->children[i] < 0) {
            t->children[i] = (dleafs[-node->children[i] - 1].contents & CONTENTS_SOLID) | (1 << 31);
            CHKVAL("MakeTnode-lt0", t->children[i]);
        }
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


int TestLine_r (int node, const vec3_t* start_, const vec3_t* stop, vec3_t* out_vec)
{
	tnode_t	*tnode;
	float	front, back;
	vec3_t	mid;
	int		side;
	int		r;

    vec3_t start = *start_;

    while (true)
    {
        if (node & (1 << 31)) {
            if (out_vec) {
                VectorCopy(start, (*out_vec));
                CHKVAL("TestLine_r-outvec", start);
            }
            r = node & ~(1 << 31);	// leaf node
            //CHKVAL("TestLine_r-retleaf", r);
            return r;
        }

        tnode = &tnodes[node];

        //CHKVAL("TestLine_r-start", start);
        //CHKVAL("TestLine_r-stop", stop);
        //CHKVAL("TestLine_r-type", tnode->type);
        //CHKVAL("TestLine_r-dist", tnode->dist);

        switch (tnode->type)
        {
            case PLANE_X:
                front = start.x;
                back = stop->x;
                break;
            case PLANE_Y:
                front = start.y;
                back = stop->y;
                break;
            case PLANE_Z:
                front = start.z;
                back = stop->z;
                break;
            default:
                front = (start.x*tnode->normal.x + start.y*tnode->normal.y + start.z*tnode->normal.z) - tnode->dist;
                back = (stop->x*tnode->normal.x + stop->y*tnode->normal.y + stop->z*tnode->normal.z) - tnode->dist;

                if (front >= -ON_EPSILON && back >= -ON_EPSILON) {
                    //CHKVAL("TestLine_r-c0", true);
                    //return TestLine_r(tnode->children[0], start, stop, out_vec);
                    node = tnode->children[0];
                    continue;
                }
                if (front <= ON_EPSILON && back <= ON_EPSILON) {
                    //CHKVAL("TestLine_r-c1", true);
                    //return TestLine_r(tnode->children[1], start, stop, out_vec);
                    node = tnode->children[1];
                    continue;
                }
                goto skip_sub_dist; //break;
        }

        if (front >= tnode->dist && back >= tnode->dist) {
            //CHKVAL("TestLine_r-c0", true);
            //return TestLine_r(tnode->children[0], start, stop, out_vec);
            node = tnode->children[0];
            continue;
        }

        if (front <= tnode->dist && back <= tnode->dist) {
            //CHKVAL("TestLine_r-c1", true);
            //return TestLine_r(tnode->children[1], start, stop, out_vec);
            node = tnode->children[1];
            continue;
        }

        front -= tnode->dist;
        back -= tnode->dist;

    skip_sub_dist:;

        side = front < 0;

        double frac = front / ((double)front - back);
        mid.x = start.x + (stop->x - start.x)*frac;
        mid.y = start.y + (stop->y - start.y)*frac;
        mid.z = start.z + (stop->z - start.z)*frac;

        r = TestLine_r(tnode->children[side], &start, &mid, out_vec);
        if (r)
            return r;
        
        //return TestLine_r(tnode->children[!side], &mid, stop, out_vec);
        VectorCopy(mid, start);
        node = tnode->children[!side];
    }
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
    texinfo_t& ti = texinfo[tx];

    fVar22 = ti.vecs[0][0] * param_2.x +
             ti.vecs[0][1] * param_2.y +
             ti.vecs[0][2] * param_2.z + 
             ti.vecs[0][3];

    if (shadowfilter != 0) {

        // bilinear filtering on shadowface projections

        fVar22 = fVar22 - 0.5f;
        fVar3 = (ti.vecs[1][0] * param_2.x +
                 ti.vecs[1][1] * param_2.y + 
                 ti.vecs[1][2] * param_2.z +
                 ti.vecs[1][3]) - 0.5f;

        fVar1 = fVar22 - floor(fVar22);
        fVar2 = fVar3 - floor(fVar3);

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
        int row = shfunk->projtex->width * y;
        int row2 = shfunk->projtex->width * y2;

        int nw = (row + x);
        int ne = (row + x2);
        int sw = (row2 + x);
        int se = (row2 + x2);

        fVar22 = 1.0f - fVar1;
        fVar3 = 1.0f - fVar2;

        rgba_t* p = shfunk->projtex->texture32;

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


    fVar1 = ti.vecs[1][0];
    fVar2 = param_2.x;
    fVar3 = ti.vecs[1][1];
    fVar4 = param_2.y;
    fVar5 = ti.vecs[1][2];
    fVar6 = param_2.z;
    fVar7 = ti.vecs[1][3];
    
    int x = (int)floor(fVar22) % shfunk->projtex->width;
    if (x < 0) {
        x += shfunk->projtex->width;
    }

    int y = (int)floor((double)(fVar5 * fVar6 + fVar3 * fVar4 + fVar1 * fVar2 + fVar7)) % shfunk->projtex->height;
    if (y < 0) {
        y += shfunk->projtex->height;
    }

    int i = (shfunk->projtex->width * y + x) * 4;

    out_color->x = shfunk->projtex->texture32[i].R / 255.0f;
    out_color->y = shfunk->projtex->texture32[i].G / 255.0f;
    out_color->z = shfunk->projtex->texture32[i].B / 255.0f;
    *out_param_4 = shfunk->projtex->texture32[i].A / 255.0f;
}


int TestLine_shadowfunk(shadowfaces_unk_t *shfunk, shadowmodel_t *shmod, const vec3_t& mins, const vec3_t& maxs,
    const vec3_t& length, const vec3_t& start, const vec3_t& stop, vec3_t *out_param_8, vec3_t *out_param_9)
{
    float fVar1;
    vec3_t color;
    vec3_t local_48;
    vec3_t local_3c;
    vec3_t tmp, tmp2, tmp3;

    CHKVAL("TestLine_shadowfunk-length", length);

    if ((shfunk->maxs.x <= mins.x) || (shfunk->maxs.y <= mins.y) || (shfunk->maxs.z <= mins.z) ||
        (shfunk->mins.x >= maxs.x) || (shfunk->mins.y >= maxs.y) || (shfunk->mins.z >= maxs.z)) {
        CHKVAL("TestLine_shadowfunk-ret0", true);
        return 0;
    }

    int planenum = dfaces[shfunk->face].planenum;
    dplane_t* pln;
    if (dfaces[shfunk->face].side == 0) {
        pln = &dplanes[planenum];
    } else {
        pln = &backplanes[planenum];
    }
    if (0 <= DotProduct(pln->normal, length)) {
        CHKVAL("TestLine_shadowfunk-ret0", true);
        return 0;
    }
    float dstart = DotProduct(pln->normal, start) - pln->dist;
    float dstop = DotProduct(pln->normal, stop) - pln->dist;
    if (0 <= dstop * dstart) {
        CHKVAL("TestLine_shadowfunk-ret0", true);
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
        VectorSubtract(tmp, local_48, tmp);
        vec3_t c;
        CrossProduct(tmp2, tmp, c);
        float d = DotProduct(tmp3, c);
        if (d > 0 || (d == 0 && (0 <= (c.x * 0.5416f + c.y * 0.7419f) - c.z * 0.3953f))) {
            break;
        }
    }

    if (i != dfaces[shfunk->face].numedges) {
        CHKVAL("TestLine_shadowfunk-ret0", true);
        return 0;
    }
    fVar1 = dstart / (dstart - dstop);
    local_3c.x = (stop.x - start.x) * fVar1 + start.x;
    local_3c.y = (stop.y - start.y) * fVar1 + start.y;
    local_3c.z = (stop.z - start.z) * fVar1 + start.z;

    if (!shfunk->useTransFaces && shmod->nonTransFaces == TextureShadowMode::IgnoreAlpha &&
            (!shfunk->projtex || !shfunk->projtex->has_transparent_pixels))
    {
        VectorClear((*out_param_9));
    }
    else {
        float trans;
        if (shfunk->useTransFaces) {
            if ((texinfo[dfaces[shfunk->face].texinfo].flags & SURF_TRANS33) == 0) {
                trans = 0.66f;
            } else {
                trans = 0.33f;
            }
        } else {
            trans = 1.0f;
        }

        if ((shfunk->projtex && shfunk->projtex->has_transparent_pixels) || 
            shfunk->cast_single_color_shadow_with_tex_alpha)
        {
            CHKVAL2("shadowfun-maybebool", shfunk->cast_single_color_shadow_with_tex_alpha);
            SampleShadowColor(shfunk, local_3c, &color, &dstart);
        }

        if (shfunk->projtex && shfunk->projtex->has_transparent_pixels) {
            trans = (dstart * trans);
        }

        if (!shfunk->cast_single_color_shadow_with_tex_alpha) {
            TextureShadowMode c = shfunk->useTransFaces ? shmod->transFaces : shmod->nonTransFaces;
            if (c == TextureShadowMode::SingleColorWithReflectivityColor) {
                VectorCopy(texture_reflectivity[dfaces[shfunk->face].texinfo], color);
            } else {
                VectorClear(color);
            }
        } else {
            VectorScale(color, g_texscale, color);
            if ((color.x > 1) || (color.y > 1) || (color.z > 1)) {
                ColorNormalize(color, color);
            }
        }

        fVar1 = 1.0f - trans;
        out_param_9->x = out_param_9->x * fVar1 + color.x * trans * out_param_9->x;
        out_param_9->y = out_param_9->y * fVar1 + color.y * trans * out_param_9->y;
        out_param_9->z = out_param_9->z * fVar1 + color.z * trans * out_param_9->z;
        if (((out_param_9->x != 0) || (out_param_9->y != 0)) || (out_param_9->z != 0)) {
            CHKVAL("TestLine_shadowfunk-ret0", true);
            return 0;
        }
    }

    if (out_param_8) {
        VectorCopy(local_3c, (*out_param_8));
    }
    CHKVAL("TestLine_shadowfunk-ret1", true);
    return 1;
}


int TestLine_shadowmodel(const vec3_t& start, const vec3_t& stop, vec3_t* out_vec3, vec3_t* out_param_4)
{
    shadowfaces_unk_t *shfunk;
    shadowmodel_t *shmod;
    vec3_t maxs;
    vec3_t mins;
    vec3_t local_c;

    CHKVAL("TestLine_shadowmodel", true);

    mins.x = mins.y = mins.z = 99999.f;
    maxs.x = maxs.y = maxs.z = -99999.f;

    AddPointToBounds(stop, mins, maxs);
    AddPointToBounds(start, mins, maxs);
    VectorSubtract(stop, start, local_c);

    for (shmod = g_shadow_world; shmod; shmod = shmod->next)
    {
        int n = shmod->modelnum;
        if ((n == 0) ||
            ((mins.x < dmodels[n].maxs.x) && (mins.y < dmodels[n].maxs.y) && (mins.z < dmodels[n].maxs.z) &&
            (maxs.x > dmodels[n].mins.x) && (maxs.y < dmodels[n].mins.y) && (maxs.z < dmodels[n].mins.z)))
        {
            CHKVAL("TestLine_shadowmodel-innerloop", true);
            for (shfunk = shmod->shadownext; shfunk; shfunk = shfunk->next) {
                if (TestLine_shadowfunk(shfunk, shmod, mins, maxs, local_c, start, stop, out_vec3, out_param_4)) {
                    CHKVAL("TestLine_shadowmodel-ret-1", true);
                    return -1;
                }
            }
        }
    }
    CHKVAL("TestLine_shadowmodel-ret-0", true);
    return 0;
}

int TestLine_shadow(const vec3_t& start, const vec3_t& stop, vec3_t *out_param_3, vec3_t *optional_out_vec)
{
    CHKVAL("TestLine_shadow-start", start);
    CHKVAL("TestLine_shadow-stop", stop);

    vec3_t local_c = {1, 1, 1};
    int result = TestLine_r(0, &start, &stop, out_param_3);
    if (result == 0) {
        if (g_shadow_world) {
            result = TestLine_shadowmodel(start, stop, out_param_3, &local_c);
        }
    }
    else {
        VectorClear(local_c);
    }
    if (optional_out_vec) {
        CHKVAL("TestLine_shadow-outvec", local_c);
        VectorCopy(local_c, (*optional_out_vec));
    }

    CHKVAL("TestLine_shadow-ret", result);
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


