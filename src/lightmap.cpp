#pragma once
#include "pch.h"
#include "bspfile.h"
#include "cmdlib.h"
#include "polylib.h"
#include "mathlib.h"
#include "threads.h"
#include "blarghrad.h"

/*
=================================================================

  POINT TRIANGULATION

=================================================================
*/

typedef struct triedge_s
{
    int			p0, p1;
    vec3_t		normal;
    vec_t		dist;
    struct triangle_s	*tri;
} triedge_t;

typedef struct triangle_s
{
    triedge_t	*edges[3];
} triangle_t;

#define	MAX_TRI_POINTS		1024
#define	MAX_TRI_EDGES		(MAX_TRI_POINTS*6)
#define	MAX_TRI_TRIS		(MAX_TRI_POINTS*2)

typedef struct
{
    int			numpoints;
    int			numedges;
    int			numtris;
    dplane_t	*plane;
    dface_t     *face;
    triedge_t	*edgematrix[MAX_TRI_POINTS][MAX_TRI_POINTS];
    patch_t		*points[MAX_TRI_POINTS];
    triedge_t	edges[MAX_TRI_EDGES];
    triangle_t	tris[MAX_TRI_TRIS];
    vec3_t		maybe_origins[MAX_TRI_POINTS];   // argh uses this instead of points[]->origins in some places
} triangulation_t;
#ifdef _M_IX86
static_assert(sizeof(triangulation_t) == 4407316, "");
#endif

/*
===============
AllocTriangulation
===============
*/
triangulation_t	*AllocTriangulation(dplane_t *plane)
{
    triangulation_t	*t;

    t = (triangulation_t*)malloc(sizeof(triangulation_t));
    if (!t) {
        Error("AllocTriangulation: malloc failed");
    }

    t->numpoints = 0;
    t->numedges = 0;
    t->numtris = 0;

    t->plane = plane;

    //	memset (t->edgematrix, 0, sizeof(t->edgematrix));

    return t;
}

/*
===============
FreeTriangulation
===============
*/
void FreeTriangulation(triangulation_t *tr)
{
    free(tr);
}


triedge_t	*FindEdge(triangulation_t *trian, int p0, int p1)
{
    triedge_t	*e, *be;
    vec3_t		v1;
    vec3_t		normal;
    vec_t		dist;

    CHKVAL("FindEdge-p0", p0);
    CHKVAL("FindEdge-p1", p1);

    CHKVAL("FindEdge-exists", !!trian->edgematrix[p0][p1]);
    if (trian->edgematrix[p0][p1])
        return trian->edgematrix[p0][p1];

    if (trian->numedges > MAX_TRI_EDGES - 2)
        Error("trian->numedges > MAX_TRI_EDGES-2");

    VectorSubtract(trian->maybe_origins[p1], trian->maybe_origins[p0], v1);
    VectorNormalize(v1, v1);
    CrossProduct(v1, trian->plane->normal, normal);
    dist = DotProduct(trian->maybe_origins[p0], normal);

    e = &trian->edges[trian->numedges];
    e->p0 = p0;
    e->p1 = p1;
    e->tri = NULL;
    VectorCopy(normal, e->normal);
    e->dist = dist;
    CHKVAL("FindEdge-dist", e->dist);
    trian->numedges++;
    trian->edgematrix[p0][p1] = e;

    be = &trian->edges[trian->numedges];
    be->p0 = p1;
    be->p1 = p0;
    be->tri = NULL;
    VectorSubtract(vec3_origin, normal, be->normal);
    be->dist = -dist;
    CHKVAL("FindEdge-dist2", be->dist);
    trian->numedges++;
    trian->edgematrix[p1][p0] = be;

    return e;
}

triangle_t	*AllocTriangle(triangulation_t *trian)
{
    triangle_t	*t;

    if (trian->numtris >= MAX_TRI_TRIS)
        Error("trian->numtris >= MAX_TRI_TRIS");

    t = &trian->tris[trian->numtris];
    trian->numtris++;

    return t;
}

/*
============
TriEdge_r
============
*/
void TriEdge_r(triangulation_t *trian, triedge_t *e)
{
    int		i, bestp;
    vec3_t	v1, v2;
    vec_t	best, ang;
    triangle_t	*nt;

    CHKVAL("TriEdge_r-ep0", e->p0);
    CHKVAL("TriEdge_r-ep1", e->p1);
    CHKVAL("TriEdge_r-normal", e->normal);
    CHKVAL("TriEdge_r-dist", e->dist);

    if (e->tri)
        return;		// allready connected by someone

    // find the point with the best angle
    vec3_t& p0 = trian->maybe_origins[e->p0];
    vec3_t& p1 = trian->maybe_origins[e->p1];

    CHKVAL("TriEdge_r-p0", p0);
    CHKVAL("TriEdge_r-p1", p1);

    best = 1.1f;
    for (i = 0; i < trian->numpoints; i++)
    {
        vec3_t& p = trian->maybe_origins[i];
        CHKVAL("TriEdge_r-p", p);

        // a 0 dist will form a degenerate triangle
        double d = DotProduct(p, e->normal);
        if (d - e->dist <= 0)
            continue;	// behind edge
        CHKVAL("TriEdge_r-firstdot", true);

        VectorSubtract(p0, p, v1);
        VectorSubtract(p1, p, v2);
        if (!VectorNormalize(v1, v1))
            continue;
        if (!VectorNormalize(v2, v2))
            continue;

        if (texinfo[trian->face->texinfo].flags & SURF_CURVE) {
            int otherface_00 = trian->points[e->p1]->facenum;
            int facenum = trian->points[i]->facenum;
            if (!FacesHaveSharedVertexes(facenum, otherface_00)) {
                continue;
            }
            int otherface = trian->points[e->p0]->facenum;
            if ( otherface_00 != otherface &&
                !FacesHaveSharedVertexes(facenum, otherface))
                continue;
        }

        ang = DotProduct(v1, v2);
        if (ang < best)
        {
            CHKVAL("TriEdge_r-setbest", ang);
            best = ang;
            bestp = i;
        }
    }
    CHKVAL("TriEdge_r-best1", best);

    if (best < 1) {
        CHKVAL("TriEdge_r-2", true);
        // edge matches

        // make a new triangle
        nt = AllocTriangle(trian);
        nt->edges[0] = e;
        nt->edges[1] = FindEdge(trian, e->p1, bestp);
        nt->edges[2] = FindEdge(trian, bestp, e->p0);
        for (i = 0; i < 3; i++)
            nt->edges[i]->tri = nt;
        TriEdge_r(trian, FindEdge(trian, bestp, e->p1));
        TriEdge_r(trian, FindEdge(trian, e->p0, bestp));
        return;
    }

    // edge doesn't match anything...

    vec3_t bestnormal;

    best = 99999.f;
    i = 0;
    vec3_t* v3 = &trian->maybe_origins[0];
    for (i = 0; i < trian->numpoints; i++, v3++) {
        if (VectorCompare(*v3, p0))
            continue;
        if (VectorCompare(*v3, p1))
            continue;
        if ((DotProduct((*v3), e->normal) - e->dist) < 0)
            continue;
        if ((texinfo[trian->face->texinfo].flags & SURF_CURVE) &&
            !FacesHaveSharedVertexes(trian->points[e->p1]->facenum, trian->points[i]->facenum))
            continue;

        VectorSubtract((*v3), p1, v1);
        float length = VectorNormalize(v1, v1);
        if (length < best) {
            best = length;
            bestp = i;
            VectorCopy(v1, bestnormal);
        }
    }

    CHKVAL("TriEdge_r-best2", best);

    if (best < 99999.f) {
        VectorSubtract(p0, p1, v2);
        VectorNormalize(v2, v2);
        if (!VectorCompare(bestnormal, v2)) {
            if (!trian->edgematrix[e->p1][bestp])
            {
                TriEdge_r(trian, FindEdge(trian, e->p1, bestp));
            }
            if (!trian->edgematrix[bestp][e->p1])
            {
                TriEdge_r(trian, FindEdge(trian, bestp, e->p1));
            }
        }
    }
}


bool FacesHaveSharedVertexes(int facenum, int otherface)
{
    unsigned short v, v2;

    if (facenum == otherface) {
        CHKVAL("FacesHaveSharedVertexes", true);
        return true;
    }

    for (int j = 0; j < dfaces[facenum].numedges; j++) {
        int edge = dsurfedges[dfaces[facenum].firstedge + j];
        if (edge < 0) {
            v = dedges[-edge].v[1];
        }
        else {
            v = dedges[edge].v[0];
        }

        for (int i = 0; i < dfaces[otherface].numedges; i++) {
            int edge2 = dsurfedges[dfaces[otherface].firstedge + i];
            if (edge2 < 0) {
                v2 = dedges[-edge2].v[1];
            }
            else {
                v2 = dedges[edge2].v[0];
            }
            if (v == v2) {
                CHKVAL("FacesHaveSharedVertexes", true);
                return true;
            }
        }
    }
    CHKVAL("FacesHaveSharedVertexes", false);
    return false;
}


/*
============
TriangulatePoints
============
*/
void TriangulatePoints(triangulation_t *trian)
{
    vec_t	bestd;
    vec3_t	v1;
    int		bp1 = 0, bp2 = 0, i, j;
    triedge_t	*e, *e2;

    CHKVAL("TriangulatePoints-numpts", trian->numpoints);

    if (trian->numpoints < 2)
        return;

    // find the two closest points
    bestd = 99999;
    bool isCurve = (texinfo[trian->face->texinfo].flags & SURF_CURVE);

    for (i = 0; i < trian->numpoints; i++)
    {
        for (j = i + 1; j < trian->numpoints; j++)
        {
            if (isCurve && !FacesHaveSharedVertexes(trian->points[i]->facenum, trian->points[j]->facenum))
                continue;
            
            VectorSubtract(trian->maybe_origins[j], trian->maybe_origins[i], v1);
            float d = VectorLength(v1);
            if (d < bestd)
            {
                bestd = d;
                bp1 = i;
                bp2 = j;
                CHKVAL("TriangulatePoints-newbest", bestd);
                CHKVAL("TriangulatePoints-bp1", bp1);
                CHKVAL("TriangulatePoints-bp2", bp2);
            }
        }
    }
    CHKVAL("TriangulatePoints-next", true);

    e = FindEdge(trian, bp1, bp2);
    e2 = FindEdge(trian, bp2, bp1);
    TriEdge_r(trian, e);
    TriEdge_r(trian, e2);
}

/*
===============
AddPointToTriangulation
===============
*/
void AddPointToTriangulation(patch_t *patch, triangulation_t *trian)
{
    int			pnum;

    pnum = trian->numpoints;
    CHKVAL("AddPointToTriangulation-numpts", pnum);

    if (pnum == MAX_TRI_POINTS)
        Error("trian->numpoints == MAX_TRI_POINTS");
    trian->points[pnum] = patch;
    trian->numpoints++;
}

/*
===============
LerpTriangle
===============
*/
void	LerpTriangle(triangulation_t *trian, triangle_t *t, vec3_t point, vec3_t& color)
{
    vec3_t		base, d1, d2;
    float		x, y, x1, y1, x2, y2;

    patch_t* p1 = trian->points[t->edges[0]->p0];
    patch_t* p2 = trian->points[t->edges[1]->p0];
    patch_t* p3 = trian->points[t->edges[2]->p0];

    VectorCopy(p1->totallight, base);
    VectorSubtract(p2->totallight, base, d1);
    VectorSubtract(p3->totallight, base, d2);

    x = DotProduct(point, t->edges[0]->normal) - t->edges[0]->dist;
    y = DotProduct(point, t->edges[2]->normal) - t->edges[2]->dist;

    x1 = 0;
    y1 = DotProduct(trian->maybe_origins[t->edges[1]->p0], t->edges[2]->normal) - t->edges[2]->dist;

    x2 = DotProduct(trian->maybe_origins[t->edges[2]->p0], t->edges[0]->normal) - t->edges[0]->dist;
    y2 = 0;

    VectorCopy(base, color);
    if (fabs(y1) >= ON_EPSILON)
        VectorMA(color, x / x2, d2, color);
    if (fabs(x2) >= ON_EPSILON)
        VectorMA(color, y / y1, d1, color);
    CHKVAL("LerpTriangle", color);
}

qboolean PointInTriangle(const vec3_t& point, triangle_t *t)
{
    int		i;
    triedge_t	*e;
    vec_t	d;

    CHKVAL("PointInTriangle-pt", point);

    for (i = 0; i < 3; i++)
    {
        e = t->edges[i];
        CHKVAL("PointInTriangle-e->dist", e->dist);
        CHKVAL("PointInTriangle-e->normal", e->normal);
        d = DotProduct(e->normal, point) - e->dist;
        if (d < 0) {
            CHKVAL("PointInTriangle-ret", false);
            return false;	// not inside
        }
    }

    CHKVAL("PointInTriangle-ret", true);
    return true;
}



// name guess
void GetEdgeBetweenCoplanarFaces(dface_t *face1, dface_t *face2, vec3_t *out_edge_p0, vec3_t *out_edge_p1)
{
    for (int i = 0; i < face1->numedges; i++) {
        int edge = dsurfedges[face1->firstedge + i];
        for (int j = 0; j < face2->numedges; j++) {
            if (edge == -dsurfedges[face2->firstedge + j]) {
                int v1, v2;
                if (edge < 0) {
                    v1 = dedges[-edge].v[1];
                    v2 = dedges[-edge].v[0];
                }
                else {
                    v1 = dedges[edge].v[0];
                    v2 = dedges[edge].v[1];
                }
                VectorCopy(dvertexes[v1].point, (*out_edge_p0));
                VectorCopy(dvertexes[v2].point, (*out_edge_p1));
                return;
            }
        }
    }

    /* foreach edge in face1... */
    for (int i = 0; i < face1->numedges; i++) {
        int outer_v1;
        int outer_v2;

        int edge = dsurfedges[face1->firstedge + i];
        if (edge < 0) {
            outer_v1 = dedges[-edge].v[1];
            outer_v2 = dedges[-edge].v[0];
        }
        else {
            outer_v1 = dedges[edge].v[0];
            outer_v2 = dedges[edge].v[1];
        }

        /* foreach edge in face2... */
        for (int j = 0; j < face2->numedges; j++) {
            int edge2 = dsurfedges[face2->firstedge + j];
            int v;
            if (edge2 < 0) {
                v = dedges[-edge2].v[1];
            }
            else {
                v = dedges[edge2].v[0];
            }
            if (outer_v2 == v) {
                vec3_t d0, d1;

                VectorCopy(dvertexes[outer_v2].point, (*out_edge_p0));
                VectorSubtract(dvertexes[outer_v2].point, dvertexes[outer_v1].point, d0);
                VectorNormalize(d0, d0);
                int inner_v2 = outer_v2;
                if (outer_v1 != outer_v2) {
                    do {
                        i = i + 1;
                        if (i == face1->numedges) {
                            i = 0;
                        }
                        edge2 = dsurfedges[i + face1->firstedge];
                        if (edge2 < 0) {
                            outer_v1 = dedges[-edge2].v[1];
                            inner_v2 = dedges[-edge2].v[0];
                        }
                        else {
                            outer_v1 = dedges[edge2].v[0];
                            inner_v2 = dedges[edge2].v[1];
                        }
                    } while (outer_v1 != outer_v2);
                }
                VectorSubtract(dvertexes[inner_v2].point, dvertexes[outer_v1].point, d1);
                VectorNormalize(d1, d1);

                // dead code - review orig
                /*out_edge_p1->x = d1.x + d0.x;
                out_edge_p1->y = d1.y + d0.y;
                out_edge_p1->z = d1.z + d0.z;*/

                out_edge_p1->x = d1.x + d0.x + out_edge_p0->x;
                out_edge_p1->y = d1.y + d0.y + out_edge_p0->y;
                out_edge_p1->z = d1.z + d0.z + out_edge_p0->z;
                return;
            }
        }
    }
}

// name guess
void BuildTriangulationOrigins(triangulation_t *trian)
{
    vec3_t local_5c;
    vec3_t local_50;
    vec3_t local_38;
    vec3_t local_2c;
    vec3_t p0, p1;
    vec3_t local_14;

    dface_t* face = nullptr;

    vec3_t* dest = trian->maybe_origins;
    for (int i = 0; i < trian->numpoints; i++, dest++)
    {
        CHKVAL2("BuildTriangulationOrigins-i", i);

        patch_t* patch = trian->points[i];
        if (patch->plane == trian->plane)
        {
            VectorCopy(patch->origin, (*dest));
            CHKVAL2("BuildTriangulationOrigins-copy", true);
        }
        else
        {
            dface_t* nextface = &dfaces[patch->facenum];
            if (face != nextface) {
                GetEdgeBetweenCoplanarFaces(trian->face, nextface, &p0, &p1);
                CHKVAL2("BuildTriangulationOrigins-p0", p0);
                CHKVAL2("BuildTriangulationOrigins-p1", p1);
                face = nextface;
            }
            CHKVAL2("BuildTriangulationOrigins-fpln", face->planenum);
            CHKVAL2("BuildTriangulationOrigins-side", face->side);
            if (face->side == 0) {
                VectorCopy(dplanes[face->planenum].normal, local_50);
            }
            else {
                VectorCopy(backplanes[face->planenum].normal, local_50);
            }
            VectorSubtract(patch->origin, local_50, local_50);
            CHKVAL2("BuildTriangulationOrigins-local_50", local_50);

            VectorSubtract(p1, p0, local_5c);
            CHKVAL2("BuildTriangulationOrigins-local_5c", local_5c);

            vec3_t tmp;
            VectorSubtract(local_50, p0, tmp);
            float scale = DotProduct(tmp, local_5c) / DotProduct(local_5c, local_5c);
            CHKVAL2("BuildTriangulationOrigins-scale1", scale);
            VectorMA(p0, scale, local_5c, local_2c);
            CHKVAL2("BuildTriangulationOrigins-vectorma1-out", local_2c);
            VectorSubtract(local_50, local_2c, local_14);

            float scale2 = VectorLength(local_14);
            CHKVAL2("BuildTriangulationOrigins-scale2", scale2);
            CrossProduct(trian->plane->normal, local_5c, local_38);
            VectorNormalize(local_38, local_38);
            VectorMA(local_2c, scale2, local_38, local_38);
            CHKVAL2("BuildTriangulationOrigins-vectorma2-out", local_38);
            VectorAdd(trian->plane->normal, local_38, (*dest));
        }

        CHKVAL2("BuildTriangulationOrigins-dest", *dest);
    }
}



/*
===============
SampleTriangulation
===============
*/
void SampleTriangulation(const vec3_t& point, triangulation_t *trian, vec3_t& color)
{
    triangle_t	*t;
    triedge_t	*e, *beste = nullptr;
    vec_t		d, d2, best;
    patch_t		*p0, *p1;
    vec3_t		v1, v2;
    int			i, j;

    CHKVAL2("SampleTriangulation-point", point);

    if (trian->numpoints == 0)
    {
        VectorClear(color);
        CHKVAL2("SampleTriangulation-retvec", color);
        return;
    }
    if (trian->numpoints == 1)
    {
        VectorCopy(trian->points[0]->totallight, color);
        CHKVAL2("SampleTriangulation-retvec", color);
        return;
    }

    // search for triangles
    for (t = trian->tris, j = 0; j < trian->numtris; t++, j++)
    {
        if (!PointInTriangle(point, t))
            continue;

        // this is it
        LerpTriangle(trian, t, point, color);
        CHKVAL2("SampleTriangulation-retvec", color);
        return;
    }

    best = 99999;

    // search for exterior edge
    for (e = trian->edges, j = 0; j < trian->numedges; e++, j++)
    {
        if (e->tri)
            continue;		// not an exterior edge

        d = DotProduct(point, e->normal) - e->dist;
        if (d < 0)
            continue;	// not in front of edge

        vec3_t local_c;
        VectorSubtract(trian->maybe_origins[e->p1], trian->maybe_origins[e->p0], local_c);
        
        //CHKVAL2("SampleTriangulation-lc", local_c);

        vec3_t tmp;
        VectorSubtract(point, trian->maybe_origins[e->p0], tmp);
        d2 = DotProduct(tmp, local_c) / DotProduct(local_c, local_c);

        if (d2 < 0)
            continue;
        if (d2 > 1)
            continue;

        if (d2 < best) {
            best = d2;
            beste = e;
        }
    }

    if (best < 99999) {
        p0 = trian->points[beste->p0];
        p1 = trian->points[beste->p1];

        for (i = 0; i < 3; i++)
            color.data[i] = p0->totallight.data[i] + best * (p1->totallight.data[i] - p0->totallight.data[i]);

        CHKVAL2("SampleTriangulation-retvec", color);
        return;
    }

    CHKVAL2("SampleTriangulation-nobest", true);


    // search for nearest point
    best = 99999;
    p1 = NULL;
    for (j = 0; j < trian->numpoints; j++)
    {
        vec3_t& p = trian->maybe_origins[j];
        CHKVAL2("SampleTriangulation-lastp", p);
        VectorSubtract(point, p, v1);
        d = VectorLength(v1);
        if (d < best)
        {
            best = d;
            p1 = trian->points[j];
        }
    }

    if (!p1)
        Error("SampleTriangulation: no points");

    VectorCopy(p1->totallight, color);
    CHKVAL2("SampleTriangulation-retvec", color);
}

/*
=================================================================

  LIGHTMAP SAMPLE GENERATION

=================================================================
*/

#define	SINGLEMAP	(64*64*4)

/*
================
CalcFaceExtents

Fills in s->texmins[] and s->texsize[]
also sets exactmins[] and exactmaxs[]
================
*/
void CalcFaceExtents(lightinfo_t *l)
{
    dface_t *s;
    vec_t	mins[2], maxs[2], val;
    int		i, j, e;
    dvertex_t	*v;
    texinfo_t	*tex;
    vec3_t		vt;

    s = l->face;

    mins[0] = mins[1] = 999999;
    maxs[0] = maxs[1] = -99999;

    tex = &texinfo[s->texinfo];

    for (i = 0; i < s->numedges; i++)
    {
        e = dsurfedges[s->firstedge + i];
        if (e >= 0)
            v = dvertexes + dedges[e].v[0];
        else
            v = dvertexes + dedges[-e].v[1];

        //		VectorAdd (v->point, l->modelorg, vt);
        VectorCopy(v->point, vt);

        for (j = 0; j < 2; j++)
        {
            // TODO - fix vec cast
            val = DotProduct(vt, (*(vec3_t*)&tex->vecs[j])) + tex->vecs[j][3];
            if (val < mins[j])
                mins[j] = val;
            if (val > maxs[j])
                maxs[j] = val;
        }
    }

    for (i = 0; i < 2; i++)
    {
        l->exactmins[i] = mins[i];
        l->exactmaxs[i] = maxs[i];

        mins[i] = floor(mins[i] / 16);
        maxs[i] = ceil(maxs[i] / 16);

        l->texmins[i] = mins[i];
        l->texsize[i] = maxs[i] - mins[i];
        if (l->texsize[0] * l->texsize[1] > SINGLEMAP / 4)	// div 4 for extrasamples
            Error("Surface to large to map");
    }
}

/*
================
CalcFaceVectors

Fills in texorg, worldtotex. and textoworld
================
*/
void CalcFaceVectors(lightinfo_t *l)
{
    texinfo_t	*tex;
    int			i, j;
    vec3_t	texnormal;
    vec_t	distscale;
    vec_t	dist, len;
    int			w, h;

    tex = &texinfo[l->face->texinfo];

    // convert from float to double
    for (i = 0; i < 2; i++)
        for (j = 0; j < 3; j++)
            l->worldtotex[i].data[j] = tex->vecs[i][j];

    // calculate a normal to the texture axis.  points can be moved along this
    // without changing their S/T
    texnormal.x = tex->vecs[1][1] * tex->vecs[0][2]
        - tex->vecs[1][2] * tex->vecs[0][1];
    texnormal.y = tex->vecs[1][2] * tex->vecs[0][0]
        - tex->vecs[1][0] * tex->vecs[0][2];
    texnormal.z = tex->vecs[1][0] * tex->vecs[0][1]
        - tex->vecs[1][1] * tex->vecs[0][0];
    VectorNormalize(texnormal, texnormal);

    // flip it towards plane normal
    distscale = DotProduct(texnormal, l->facenormal);
    if (!distscale)
    {
        qprintf("WARNING: Texture axis perpendicular to face\n");
        distscale = 1;
    }
    if (distscale < 0)
    {
        distscale = -distscale;
        VectorSubtract(vec3_origin, texnormal, texnormal);
    }

    // distscale is the ratio of the distance along the texture normal to
    // the distance along the plane normal
    distscale = 1 / distscale;

    for (i = 0; i < 2; i++)
    {
        len = VectorLength(l->worldtotex[i]);
        dist = DotProduct(l->worldtotex[i], l->facenormal);
        dist *= distscale;
        VectorMA(l->worldtotex[i], -dist, texnormal, l->textoworld[i]);
        VectorScale(l->textoworld[i], (1 / len)*(1 / len), l->textoworld[i]);
    }


    // calculate texorg on the texture plane
    for (i = 0; i < 3; i++)
        l->texorg.data[i] = -tex->vecs[0][3] * l->textoworld[0].data[i] - tex->vecs[1][3] * l->textoworld[1].data[i];

    // project back to the face plane
    dist = DotProduct(l->texorg, l->facenormal) - l->facedist - 1;
    dist *= distscale;
    VectorMA(l->texorg, -dist, texnormal, l->texorg);

    // compensate for org'd bmodels
    VectorAdd(l->texorg, l->modelorg, l->texorg);

    // total sample count
    h = l->texsize[1] + 1;
    w = l->texsize[0] + 1;
    l->numsurfpt = w * h;
}


/*
=================
CalcPoints2
=================
*/

struct unkpoints_t {
    vec3_t first;
    vec3_t second;
    vec3_t third;
    vec3_t fourth;
};

void CalcPoints2(lightinfo_t *l, float sofs, float tofs, int facenum)
{
    int iVar11;
    vec3_t planenormal;
    vec3_t vStack3172;
    vec3_t vStack3160;
    vec3_t local_c30;
    vec3_t local_c20;

    int planenum = dfaces[facenum].planenum;
    if (dfaces[facenum].side == 0) {
        VectorCopy(dplanes[planenum].normal, planenormal);
    }
    else {
        VectorCopy(backplanes[planenum].normal, planenormal);
    }

    vec3_t center;
    VectorClear(center);
    unkpoints_t upts[MAX_POINTS_ON_WINDING];

    for (int i = 0; i < dfaces[facenum].numedges; i++) {
        unkpoints_t* p = &upts[i];
        int edge = dsurfedges[dfaces[facenum].firstedge + i];
        unsigned short v1, v2;
        if (edge < 0) {
            v1 = dedges[-edge].v[1];
            v2 = dedges[-edge].v[0];
        }
        else {
            v1 = dedges[edge].v[0];
            v2 = dedges[edge].v[1];
        }
        VectorAdd(planenormal, dvertexes[v1].point, p->first);
        VectorAdd(planenormal, dvertexes[v2].point, p->second);
        VectorSubtract(p->second, p->first, p->fourth);
        VectorCopy(p->fourth, p->fourth);
        CrossProduct(p->fourth, planenormal, p->third);
        VectorNormalize(p->third, p->third);
        VectorAdd(center, p->first, center);
    }

    VectorScale(center, (1.0 / dfaces[facenum].numedges), center);
    CHKVAL("CalcPoints2-center", center);

    int h = l->texsize[1] + 1;
    int w = l->texsize[0] + 1;
    l->numsurfpt = w * h;
    l->surfpt = (vec3_t*)malloc(l->numsurfpt * sizeof(vec3_t));
    if (!l->surfpt) {
        Error("CalcPoints2: (surfpt) malloc failed");
    }
    l->realpt = (vec3_t*)malloc(l->numsurfpt * sizeof(vec3_t));
    if (!l->realpt) {
        Error("CalcPoints2: (realpt) malloc failed");
    }


    float starts = l->texmins[0] * 16;
    float startt = l->texmins[1] * 16;
    float step = 16;

    vec3_t* surf = l->surfpt;
    vec3_t *realpt = l->realpt;

    for (int t = 0; t < h; t++)
    {
        float ut = startt + (t + tofs)*step;
        for (int s = 0; s < w; s++, surf++, realpt++)
        {
            float us = starts + (s + sofs)*step;

            for (int j = 0; j < 3; j++) {
                realpt->data[j] = l->texorg.data[j] + l->textoworld[0].data[j] * us
                    + l->textoworld[1].data[j] * ut;
            }

            VectorCopy((*realpt), (*surf));
            VectorSubtract(center, (*surf), local_c30);
            VectorNormalize(local_c30, local_c30);
            VectorSubtract((*surf), local_c30, (*surf));

            int i = 0;
            {
                unkpoints_t* p = &upts[i];
                for (; i < dfaces[facenum].numedges; i++, p++) {
                    vec3_t tmp;
                    VectorSubtract((*surf), p->first, tmp);
                    float dot = DotProduct(tmp, p->third);
                    if (dot < 0)
                        break;
                }
            }

            CHKVAL("CalcPoints2-i", i);

            if (i == dfaces[facenum].numedges) {
                VectorCopy((*realpt), (*surf));
            }
            else {
                i = 0;
                float bestd = 99999;
                int besti = -1;
                unkpoints_t* p = &upts[0];
                for (i = 0; i < dfaces[facenum].numedges; i++, p++) {
                    vec3_t tmp;
                    VectorSubtract((*surf), p->first, tmp);
                    float d = DotProduct(tmp, tmp);
                    if (d < bestd) {
                        bestd = d;
                        besti = i;
                    }

                    if (DotProduct(tmp, p->third) < 0) {
                        float fStack3200 = DotProduct(p->fourth, tmp) / DotProduct(p->fourth, p->fourth);
                        if (fStack3200 <= 1 && fStack3200 >= 0)
                        {
                            VectorMA(p->first, (double)fStack3200, p->fourth, vStack3160);
                            besti = i;
                            break;
                        }
                    }
                }

                if (i == dfaces[facenum].numedges) {
                    VectorCopy(upts[besti].first, vStack3160);
                }
                VectorSubtract(center, vStack3160, vStack3172);
                VectorNormalize(vStack3172, vStack3172);
                VectorAdd(vStack3172, vStack3160, vStack3160);
                if (!g_shadow_faces[facenum]) {
                    //CHKVAL("CalcPoints2-tlr1", true);
                    CHKVAL("CalcPoints2-tlr1-start", vStack3160);
                    CHKVAL("CalcPoints2-tlr1-stop", (*surf));
                    iVar11 = TestLine_r(0, vStack3160, *surf, /*out*/ &local_c20);
                }
                else {
                    CHKVAL("CalcPoints2-tlsh1-start", vStack3160);
                    CHKVAL("CalcPoints2-tlsh1-stop", (*surf));
                    iVar11 = TestLine_shadow(vStack3160, *surf, /*out*/ &local_c20, nullptr);
                }
                if (iVar11 == 0) {
                    VectorMA(*surf, -1.1f, planenormal, vStack3172);

                    // TODO remove
                    if (i == dfaces[facenum].numedges) {
                        CHKVAL("CalcPoints2-tlr2", true);
                    }

                    if ((i == dfaces[facenum].numedges) &&
                        (iVar11 = TestLine_r(0, *surf, vStack3172, /*out*/ &local_c20), iVar11 == 0)) {
                        VectorCopy(vStack3160, (*surf));
                    }
                    else {
                        VectorCopy((*realpt), (*surf));
                    }

                    dleaf_t *leaf = PointInLeaf(*surf);
                    if ((leaf->contents & CONTENTS_SOLID)) {
                        VectorAdd(local_c30, (*surf), (*surf));
                    }
                }
                else {
                    VectorSubtract(center, local_c20, vStack3172);
                    VectorNormalize(vStack3172, vStack3172);
                    VectorAdd(local_c20, vStack3172, (*surf));
                }
            }
            CHKVAL("CalcPoints2-surf", (*surf));
        }
    }
}


/*
=================
CalcPoints

For each texture aligned grid point, back project onto the plane
to get the world xyz value of the sample point
=================
*/
void CalcPoints(lightinfo_t *l, float sofs, float tofs, int facenum)
{

    int h = l->texsize[1] + 1;
    int w = l->texsize[0] + 1;
    l->numsurfpt = w * h;

    l->surfpt = (vec3_t*)malloc(w * h * sizeof(vec3_t));
    if (!l->surfpt) {
        Error("CalcPoints: (surfpt) malloc failed");
    }
    l->realpt = (vec3_t*)malloc(l->numsurfpt * sizeof(vec3_t));
    if (!l->realpt) {
        Error("CalcPoints: (realpt) malloc failed");
    }
    vec3_t* surf = l->surfpt;
    vec3_t *realpt = l->realpt;
    float mids = (l->exactmaxs[0] + l->exactmins[0]) / 2;
    float midt = (l->exactmaxs[1] + l->exactmins[1]) / 2;

    vec3_t facemid;
    for (int j = 0; j < 3; j++)
        facemid.data[j] = l->texorg.data[j] + l->textoworld[0].data[j] * mids + l->textoworld[1].data[j] * midt;


    float starts = l->texmins[0] * 16;
    float startt = l->texmins[1] * 16;
    float step = 16;


    for (int t = 0; t < h; t++)
    {
        float ut = startt + (t + tofs)*step;
        for (int s = 0; s < w; s++, surf++, realpt++)
        {
            float us = starts + (s + sofs)*step;

            for (int j = 0; j < 3; j++) {
                realpt->data[j] = l->texorg.data[j] + l->textoworld[0].data[j] * us
                    + l->textoworld[1].data[j] * ut;
            }

            // if a line can be traced from surf to facemid, the point is good
            for (int i = 0; i < 6; i++) {
                // calculate texture point
                for (int j = 0; j < 3; j++)
                    surf->data[j] = l->texorg.data[j] + l->textoworld[0].data[j] * us
                    + l->textoworld[1].data[j] * ut;

                dleaf_t* leaf = PointInLeaf(*surf);
                if (leaf->contents != CONTENTS_SOLID) {

                    if (!g_shadow_faces[facenum]) {
                        if (!TestLine_r(0, facemid, *surf, nullptr)) {
                            break;  // got it
                        }
                    }
                    else {
                        if (!TestLine_shadow(facemid, *surf, 0, nullptr)) {
                            break;  // got it
                        }
                    }
                }

                // nudge it
                if (i & 1)
                {
                    if (us > mids)
                    {
                        us -= 8;
                        if (us < mids)
                            us = mids;
                    }
                    else
                    {
                        us += 8;
                        if (us > mids)
                            us = mids;
                    }
                }
                else
                {
                    if (ut > midt)
                    {
                        ut -= 8;
                        if (ut < midt)
                            ut = midt;
                    }
                    else
                    {
                        ut += 8;
                        if (ut > midt)
                            ut = midt;
                    }
                }
            }
        }
    }
}


/*
=============
FinalLightFace

Add the indirect lighting on top of the direct
lighting and save into final map format
=============
*/
void FinalLightFace(int facenum)
{
    dface_t		*f;
    int			i, j, k, st;
    vec3_t		lb;
    patch_t		*patch;
    triangulation_t	*trian = nullptr;
    facelight_t	*fl;
    float		local_minlight;
    float		max, newmax;
    byte		*dest;
    int			pfacenum;
    vec3_t		facemins, facemaxs;

    CHKVAL("FinalLightFace-facenum", facenum);
    f = &dfaces[facenum];
    fl = &facelight[facenum];

    if (!lightwarp && (texinfo[f->texinfo].flags & SURF_WARP)) {
        return;
    }
    if (texinfo[f->texinfo].flags & SURF_SKY) {
        return;
    }
    if (game == 2 && texinfo[f->texinfo].flags & H2_SURF_TALL_WALL) {
        return;
    }

    // texture is lit

    ThreadLock();
    f->lightofs = lightdatasize;
    lightdatasize += fl->numstyles*(fl->numsamples * 3);

// add green sentinals between lightmaps
#if 0
    lightdatasize += 64 * 3;
    for (i = 0; i < 64; i++)
        dlightdata[lightdatasize - (i + 1) * 3 + 1] = 255;
#endif

    if (lightdatasize > MAX_MAP_LIGHTING)
        Error("MAX_MAP_LIGHTING");
    ThreadUnlock();

    f->styles[0] = 0;
    f->styles[1] = f->styles[2] = f->styles[3] = 0xff;

    //
    // set up the triangulation
    //
    CHKVAL("FinalLightFace-numbounce", numbounce);
    if (numbounce > 0)
    {
        ClearBounds(facemins, facemaxs);
        for (i = 0; i < f->numedges; i++)
        {
            int ednum = dsurfedges[f->firstedge + i];
            if (ednum >= 0)
                AddPointToBounds(dvertexes[dedges[ednum].v[0]].point,
                    facemins, facemaxs);
            else
                AddPointToBounds(dvertexes[dedges[-ednum].v[1]].point,
                    facemins, facemaxs);
        }

        if (radorigin != 0) {
            VectorAdd(facemins, face_offset[facenum], facemins);
            VectorAdd(facemaxs, face_offset[facenum], facemaxs);
        }
        CHKVAL("FinalLightFace-facemins", facemins);
        CHKVAL("FinalLightFace-facemaxs", facemaxs);

        dplane_t *pln;
        if (dfaces[facenum].side == 0) {
            pln = dplanes;
        }
        else {
            pln = backplanes;
        }

        CHKVAL("FinalLightFace-fplnum", f->planenum);
        trian = AllocTriangulation(&pln[f->planenum]);

        patch = face_patches[facenum];
        while (patch) {
            CHKVAL("FinalLightFace-parea", patch->area);
            if (patch->area != 0) {
                AddPointToTriangulation(patch, trian);
            }
            patch = patch->next;
        }
        CHKVAL("FinalLightFace-addpointend", true);

        // for all faces on the plane, add the nearby patches
        // to the triangulation
        int uVar17 = facegroups[facenum].start;
        int otherface;
        while (otherface = uVar17, otherface != facenum)
        {
            CHKVAL("FinalLightFace-otherface", otherface);

            if (((facegroups[facenum].byte1 != 1) ||
                (f->planenum == dfaces[otherface].planenum &&
                (dfaces[facenum].side == dfaces[otherface].side))) &&
                    ((((texinfo[dfaces[facenum].texinfo].flags & SURF_CURVE) == 0) && (stopbleed == 0)) ||
                FacesHaveSharedVertexes(facenum, otherface))) 
            {
                CHKVAL("FinalLightFace-loop1", true);
                patch = face_patches[otherface];
                while (patch) {
                    CHKVAL("FinalLightFace-patchloop2", patch->area);
                    if (patch->area != 0) {
                        int txflags = texinfo[dfaces[patch->facenum].texinfo].flags;
                        int iVar11 = 0;
                        for ( ; iVar11 < 3; iVar11++) {
                            float fVar3 = (facemins.data[iVar11] - patch->origin.data[iVar11]) / 2;
                            float fVar2 = (patch->origin.data[iVar11] - facemaxs.data[iVar11]) / 2;

                            if ((txflags & SURF_LIGHT) == 0) {
                                if (txflags & SURF_CURVE) {
                                    if (fVar2 > chopcurve || fVar3 > chopcurve)
                                        break;
                                }
                                else if (fVar2 > subdiv || fVar3 > subdiv)
                                    break;
                            }
                            else {
                                if ((txflags & SURF_SKY) == 0) {
                                    if ((txflags & SURF_WARP) == 0) {
                                        if (fVar2 > choplight || fVar3 > choplight)
                                            break;
                                    }
                                    else if (fVar2 > chopwarp || fVar3 > chopwarp)
                                        break;
                                }
                                else if (fVar2 > chopsky || fVar3 > chopsky)
                                    break;
                            }
                        }
                        CHKVAL("FinalLightFace-patchloop2-i", iVar11);
                        if (iVar11 == 3) {
                            AddPointToTriangulation(patch, trian);
                        }
                    }
                    patch = patch->next;
                }
                CHKVAL("FinalLightFace-patchloop2-end", true);
            }
            uVar17 = facegroups[otherface].start;
        }

        trian->face = f;
        BuildTriangulationOrigins(trian);
        for (int j = 0; j < trian->numpoints; j++) {
            memset(trian->edgematrix[j], 0, trian->numpoints * sizeof(triedge_t*));
        }
        TriangulatePoints(trian);
    }

    //
    // sample the triangulation
    //

    //CHK_ENABLE();

    vec3_t eambient;
    VectorClear(eambient);

    local_minlight = 0;

    // _minlight allows models that have faces that would not be
    // illuminated to receive a mottled light pattern instead of
    // black
    const char* name = ValueForKey(face_entity[facenum], "classname");
    if (strcmp(name, "worldspawn")) {
        name = ValueForKey(face_entity[facenum], "_ambient");
        if (*name) {
            if (sscanf(name, "%f %f %f", &eambient.x, &eambient.y, &eambient.z) < 3) {
                eambient.y = eambient.x;
                eambient.z = eambient.x;
            }
        }
        local_minlight = FloatForKey(face_entity[facenum], "_minlighta");
        if (local_minlight == 0) {
            local_minlight = 128 * FloatForKey(face_entity[facenum], "_minlight");
        }
    }

    dest = &dlightdata[f->lightofs];
    if (fl->numstyles > 4) {
        fl->numstyles = 4;
        printf("face with too many lightstyles: (%f %f %f)\n",
            face_patches[facenum]->origin.x, face_patches[facenum]->origin.y, face_patches[facenum]->origin.z);
    }

    for (i = 0; i < fl->numstyles; i++)
    {
        f->styles[i] = fl->stylenums[i];
        CHKVAL("FinalLightFace-flnumsamp", fl->numsamples);
        for (int j = 0; j < fl->numsamples; j++)
        {
            VectorCopy(fl->samples[i][j], lb);
            CHKVAL("FinalLightFace-lb", lb);

            if (i == 0)
            {
                if (numbounce > 0) {
                    vec3_t color;
                    SampleTriangulation(fl->origins[j], trian, color);
                    if (onlybounce)
                    {
                        VectorCopy(color, lb);

                        if (texinfo[f->texinfo].flags & SURF_CURVE)
                        {
                        }
                        else
                        {
                            VectorClear(lb);
                        }
                    }
                    else
                    {
                        VectorAdd(color, lb, lb);
                    }
                }
                lb.x += ambient.x + eambient.x;
                lb.y += ambient.y + eambient.y;
                lb.z += ambient.z + eambient.z;
            }
            else if (onlybounce)
            {
                VectorClear(lb);
            }

            if (saturation != 1)
            {
                float r = lb.x / 255.f * lb.x / 255.f;
                float g = lb.y / 255.f * lb.y / 255.f;
                float b = lb.z / 255.f * lb.z / 255.f;
                float w;
                if (_nocolor_maybe_unweighted == 1) {
                    w = (r + g + b) / 3;
                }
                else {
                    w = r * 0.299 + g * 0.588 + b * 0.113;
                }
                w = (1 - saturation) * w;
                lb.x = sqrt(saturation * r + w) * 255.f;
                lb.y = sqrt(saturation * g + w) * 255.f;
                lb.z = sqrt(saturation * b + w) * 255.f;
            }

            VectorScale(lb, lightscale, lb);

            if (gamma != 1) {
                lb.x = pow(lb.x / 255.f, 1 / gamma) * 255.f;
                lb.y = pow(lb.y / 255.f, 1 / gamma) * 255.f;
                lb.z = pow(lb.z / 255.f, 1 / gamma) * 255.f;
            }

            // we need to clamp without allowing hue to change
            for (k = 0; k < 3; k++)
                if (lb.data[k] < 1)
                    lb.data[k] = 1;

            max = lb.x;
            if (lb.y > max)
                max = lb.y;
            if (lb.z > max)
                max = lb.z;
            newmax = max;
            if (newmax < 0)
                newmax = 0;		// roundoff problems
            if (newmax < local_minlight)
            {
                newmax = local_minlight + (rand() % 48);
            }
            if (newmax > maxlight)
                newmax = maxlight;

            for (k = 0; k < 3; k++)
            {
                *dest++ = lb.data[k] * newmax / max;
            }
        }
    }

    //CHK_DISABLE();

    if (numbounce > 0) {
        free(trian);
    }
}

