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

/*
===================================================================

  TEXTURE LIGHT VALUES

===================================================================
*/

/*
=======================================================================

MAKE FACES

=======================================================================
*/

/*
=============
WindingFromFace
=============
*/
winding_t	*WindingFromFace(dface_t *f)
{
    int			i;
    int			se;
    dvertex_t	*dv;
    int			v;
    winding_t	*w;

    w = AllocWinding(f->numedges);
    w->numpoints = f->numedges;

    for (i = 0; i < f->numedges; i++)
    {
        se = dsurfedges[f->firstedge + i];
        if (se < 0)
            v = dedges[-se].v[1];
        else
            v = dedges[se].v[0];

        dv = &dvertexes[v];
        VectorCopy(dv->point, w->p[i]);
    }

    RemoveColinearPoints(w);

    return w;
}

/*
=============
BaseLightForFace
=============
*/
void BaseLightForFace(dface_t *f, vec3_t& color)
{
    //
    // check for light emited by texture
    //
    texinfo_t* tx = &texinfo[f->texinfo];
    if (!(tx->flags & SURF_LIGHT) || tx->value == 0)
    {
        VectorClear(color);
        return;
    }

    VectorScale(texture_reflectivity[f->texinfo], tx->value, color);
}

qboolean IsSky(dface_t *f)
{
    UNREACHABLE(__FUNCTION__); //not used?
    texinfo_t *tx = &texinfo[f->texinfo];
    if (tx->flags & SURF_SKY)
        return true;
    return false;
}



entity_t * FindLightEntityByKeyValue(const char *key, const char *value)
{
    for (int i = 0; i < num_entities; i++) {
        entity_t* ent = &entities[i];
        const char* _Str1 = ValueForKey(ent, "classname");
        if (!strncmp(_Str1, "light", 5)) {
            if (!strcmp(value, ValueForKey(ent, key))) {
                return ent;
            }
        }
    }
    return nullptr;
}

void maybeGetVertexLightDirection(int vertexnum, int txLightValue, int face, vec3_t& out_normal)
{
    unknownunk_t *puVar1 = g_maybe_vertex_phong[vertexnum];
    while (true) {
        if (!puVar1) {
            if (dfaces[face].side != 0) {
                VectorCopy(backplanes[dfaces[face].planenum].normal, out_normal);
            } else {
                VectorCopy(dplanes[dfaces[face].planenum].normal, out_normal);
            }
            break;
        }
        /* ArghRad implements Phong shading on a per-surface basis. To use it, give the
           desired surface a light value, but do not set the light flag. This value acts
           like a "curve ID tag". When it lights that face, it will smooth together the
           lighting with any touching faces that share the same "ID tag" light value.
           Two touching surfaces can belong to two different curves simply by using
           different tag values. */
        if ((puVar1->txLightValue == txLightValue) && (puVar1->liquid_contents == g_hashset_face[face])) {
            VectorCopy(puVar1->normal, out_normal);
            break;
        }
        puVar1 = puVar1->next;
    }
    CHKVAL("maybeGetVertexLightDirection", out_normal);
}


void maybePhongSomething(int facenum, const vec3_t& center, const vec3_t& plane_normal, vec3_t& out_vec_param_4) {
    int local_EBP_165;
    int local_98;
    vec3_t mins;
    vec3_t maxs;
    vec3_t local_6c;
    vec3_t afStack96;
    vec3_t local_54;
    vec3_t local_48;
    vec3_t local_3c;
    vec3_t local_30;
    vec3_t local_area;
    vec3_t local_18;
    vec3_t local_c;

    CHKVAL2("maybePhongSomething-facenum", facenum);
    CHKVAL2("maybePhongSomething-center", center);

    VectorCopy(plane_normal, out_vec_param_4);
    if (nocurve) {
        CHKVAL2("maybePhongSomething-ret3", true);
        return;
    }
    short sVar7 = dfaces[facenum].texinfo;
    if ((texinfo[sVar7].flags & SURF_CURVE) == 0) {
        CHKVAL2("maybePhongSomething-ret3", true);
        return;
    }
    switch (dplanes[dfaces[facenum].planenum].type) {
        case PLANE_X:
        case PLANE_ANYX:
            local_98 = 1;
            local_EBP_165 = 2;
            break;
        case PLANE_Y:
        case PLANE_ANYY:
            local_98 = 0;
            local_EBP_165 = 2;
            break;
        case PLANE_Z:
        case PLANE_ANYZ:
            local_98 = 0;
            local_EBP_165 = 1;
            break;
    }

    mins.x = mins.y = mins.z = 99999.f;
    maxs.x = maxs.y = maxs.z = -99999.f;

    for (int i = 0; i < dfaces[facenum].numedges; i++) {
        int edge = dsurfedges[dfaces[facenum].firstedge + i];
        int vert;
        if (edge < 0) {
            vert = dedges[-edge].v[1];
        }
        else {
            vert = dedges[edge].v[0];
        }
        float x = dvertexes[vert].point.data[local_98];
        if (x > maxs.data[local_98]) {
            maxs.data[local_98] = x;
        }
        float y = dvertexes[vert].point.data[local_EBP_165];
        if (y > maxs.data[local_EBP_165]) {
            maxs.data[local_EBP_165] = y;
        }
        if (x < mins.data[local_98]) {
            mins.data[local_98] = x;
        }
        if (y < mins.data[local_EBP_165]) {
            mins.data[local_EBP_165] = y;
        }
    }

    int pdVar13 = local_EBP_165;

    if ((maxs.data[local_EBP_165] - mins.data[local_EBP_165]) < (maxs.data[local_98] - mins.data[local_98])) {

        // TODO this looks wrong
        float ftmp = maxs.data[local_EBP_165];
        float ftmp2 = mins.data[local_EBP_165];
        float fVar9 = mins.data[local_98];
        maxs.data[local_98] = maxs.data[local_98];
        maxs.data[local_EBP_165] = ftmp;
        mins.data[local_98] = fVar9;
        mins.data[local_EBP_165] = ftmp2;

        pdVar13 = local_98;
        local_98 = local_EBP_165;
    }
    CHKVAL("maybePhongSomething-mins", mins);
    CHKVAL("maybePhongSomething-maxs", maxs);
    CHKVAL("maybePhongSomething-pdVar13", pdVar13);

    auto* pfVar2 = &local_6c.data[pdVar13];
    VectorSubtract(center, plane_normal, local_6c);
    CHKVAL("maybePhongSomething-local_6c", local_6c);
    if (*pfVar2 < maxs.data[pdVar13]) {
        if (*pfVar2 <= mins.data[pdVar13]) {
            *pfVar2 = mins.data[pdVar13] + 0.09999999;
        }
    }
    else {
        *pfVar2 = maxs.data[pdVar13] - 0.09999999;
    }
    CHKVAL("maybePhongSomething-pfVar2", *pfVar2);

    // TODO MERGE pdVar13=>local_EBP_165? need to investigate more. this condition found a difference...
    //if (local_EBP_165 != pdVar13) BREAK;


    int txLightValue = texinfo[sVar7].value;
    int local_74 = 0;
    int local_90 = 0;
    if (0 < dfaces[facenum].numedges) {
        int iVar12 = dfaces[facenum].firstedge;
        do {
            int edge = dsurfedges[iVar12 + local_90];
            unsigned int vertexnum, vertexnum_00;
            if (edge < 0) {
                vertexnum_00 = dedges[-edge].v[1];
                vertexnum = dedges[-edge].v[0];
            }
            else {
                vertexnum_00 = dedges[edge].v[0];
                vertexnum = dedges[edge].v[1];
            }

            if ((local_90 == 0) &&
                (local_6c.data[local_98] == dvertexes[vertexnum_00].point.data[local_98]) &&
                local_6c.data[local_EBP_165] == dvertexes[vertexnum_00].point.data[local_EBP_165])
            {
                maybeGetVertexLightDirection(vertexnum_00, txLightValue, facenum, out_vec_param_4);
                CHKVAL("maybePhongSomething-ret1", true);
                return;
            }

            if ((local_6c.data[local_98] == dvertexes[vertexnum].point.data[local_98]) &&
                local_6c.data[local_EBP_165] == dvertexes[vertexnum].point.data[local_EBP_165])
            {
                maybeGetVertexLightDirection(vertexnum, txLightValue, facenum, out_vec_param_4);
                CHKVAL("maybePhongSomething-ret2", true);
                return;
            }

            CHKVAL("maybePhongSomething-v1", dvertexes[vertexnum_00].point);
            CHKVAL("maybePhongSomething-v2", dvertexes[vertexnum].point);
            CHKVAL("maybePhongSomething-local74", local_74);

            if (dvertexes[vertexnum_00].point.data[pdVar13] != dvertexes[vertexnum].point.data[pdVar13]) {
                CHKVAL("maybePhongSomething-vif1", true);

                if ((*pfVar2 < dvertexes[vertexnum].point.data[pdVar13]) || (*pfVar2 > dvertexes[vertexnum_00].point.data[pdVar13])) {
                    CHKVAL("maybePhongSomething-vif2", true);

                    if ((dvertexes[vertexnum_00].point.data[pdVar13] <= *pfVar2) &&
                        (local_6c.data[pdVar13] <= dvertexes[vertexnum].point.data[pdVar13])) {

                        if (local_74 == 0) {
                            VectorCopy(dvertexes[vertexnum].point, local_48);
                            VectorCopy(dvertexes[vertexnum_00].point, local_30);
                            maybeGetVertexLightDirection(vertexnum, txLightValue, facenum, local_3c);
                            maybeGetVertexLightDirection(vertexnum_00, txLightValue, facenum, local_area);
                            local_74 = -1;
                        }
                        else if (local_74 == 1) {
                            VectorCopy(dvertexes[vertexnum].point, afStack96);
                            VectorCopy(dvertexes[vertexnum_00].point, local_18);
                            maybeGetVertexLightDirection(vertexnum, txLightValue, facenum, local_54);
                            maybeGetVertexLightDirection(vertexnum_00, txLightValue, facenum, local_c);
                            break;
                        }
                    }
                }
                else {
                    CHKVAL("maybePhongSomething-vif3", true);
                    if (local_74 == 0) {
                        VectorCopy(dvertexes[vertexnum_00].point, local_48);
                        VectorCopy(dvertexes[vertexnum].point, local_30);
                        maybeGetVertexLightDirection(vertexnum_00, txLightValue, facenum, local_3c);
                        maybeGetVertexLightDirection(vertexnum, txLightValue, facenum, local_area);
                        local_74 = 1;
                    }
                    else if (local_74 == -1) {
                        VectorCopy(dvertexes[vertexnum_00].point, afStack96);
                        VectorCopy(dvertexes[vertexnum].point, local_18);
                        maybeGetVertexLightDirection(vertexnum_00, txLightValue, facenum, local_54);
                        maybeGetVertexLightDirection(vertexnum, txLightValue, facenum, local_c);
                        break;
                    }
                }
            }
            local_90 += 1;
        } while (local_90 < dfaces[facenum].numedges);
    }

    CHKVAL2("maybePhongSomething-lastif", true);

    if (local_90 != dfaces[facenum].numedges) {
        CHKVAL2("maybePhongSomething-lastif2", local_30);
        CHKVAL2("maybePhongSomething-lastif2", local_48);

        if (local_48.data[pdVar13] != local_30.data[pdVar13]) {
            CHKVAL2("maybePhongSomething-lastif3", afStack96);
            CHKVAL2("maybePhongSomething-lastif3", local_18);

            if (afStack96.data[pdVar13] != local_18.data[pdVar13]) {

                CHKVAL2("local_30", local_30);
                CHKVAL2("local_48", local_48);

                float fVar3 = ((local_48.data[pdVar13] - *pfVar2) / (local_48.data[pdVar13] - local_30.data[pdVar13]));
                CHKVAL2("local_a0", fVar3);
                VectorScale(local_3c, (1 - fVar3), mins);
                VectorMA(mins, (double)fVar3, local_area, mins);

                CHKVAL2("local_98", int(local_98 * sizeof(int)));
                fVar3 = local_48.data[local_98] - (local_48.data[local_98] - local_30.data[local_98]) * fVar3;
                CHKVAL2("fVar3-1", fVar3);

                float fVar4 = ((afStack96.data[pdVar13] - *pfVar2) / (afStack96.data[pdVar13] - local_18.data[pdVar13]));
                VectorScale(local_54, (1 - fVar4), maxs);
                VectorMA(maxs, (double)fVar4, local_c, maxs);

                if (local_48.data[local_98] == (afStack96.data[local_98] - local_18.data[local_98]) * fVar4 - afStack96.data[local_98]) {
                    fVar3 += 0.00001f;
                }

                fVar3 = (fVar3 - local_6c.data[local_98]) / (fVar3 - (afStack96.data[local_98] - (afStack96.data[local_98] - local_18.data[local_98]) * fVar4));
                VectorScale(mins, (1 - fVar3), out_vec_param_4);
                VectorMA(out_vec_param_4, (double)fVar3, maxs, out_vec_param_4);
                VectorNormalize(out_vec_param_4, out_vec_param_4);
            }
        }
    }
    CHKVAL2("maybePhongSomething-ret3", true);
}

/*
=============
MakePatchForFace
=============
*/
float	totalarea;
void MakePatchForFace(int fn, winding_t *w, entity_t* ent)
{
    dplane_t	*pl;
    vec3_t		color;

    dface_t *f = &dfaces[fn];

    float	area;
    if (((!invisfix || !g_shadow_faces[fn] ||
        (texinfo[f->texinfo].flags & SURF_NODRAW) == 0) ||
        (texinfo[f->texinfo].flags & SURF_LIGHT) == 0) || (fn < dmodels[0].numfaces)) {
        area = WindingArea(w);
    }
    else {
        area = 0;
    }
    totalarea += area;
    CHKVAL2("MakePatchForFace", totalarea);

    patch_t* patch = &patches[num_patches];
    if (num_patches == MAX_PATCHES)
        Error("num_patches == MAX_PATCHES");
    patch->next = face_patches[fn];
    face_patches[fn] = patch;
    patch->facenum = fn;

    patch->winding = w;

    if (f->side)
        patch->plane = &backplanes[f->planenum];
    else
        patch->plane = &dplanes[f->planenum];
    if (face_offset[fn].x || face_offset[fn].y || face_offset[fn].z)
    {	// origin offset faces must create new planes
        if (numplanes + fakeplanes >= MAX_MAP_PLANES)
            Error("numplanes + fakeplanes >= MAX_MAP_PLANES");
        pl = &dplanes[numplanes + fakeplanes];
        fakeplanes++;

        *pl = *(patch->plane);
        pl->dist += DotProduct(face_offset[fn], pl->normal);
        patch->plane = pl;
    }

    WindingCenter(w, patch->origin);
    VectorAdd(patch->origin, patch->plane->normal, patch->origin);
    dleaf_t* leaf = PointInLeaf(patch->origin);
    patch->cluster = leaf->cluster;
    //if (patch->cluster == -1)
    //    qprintf("patch->cluster == -1\n");

    maybePhongSomething(fn, patch->origin, patch->plane->normal, patch->normal);

    patch->area = area;
    if (area != 0 && area < 1)
        patch->area = 1;

    patch->focus = 1;
    patch->distance = 1;
    VectorCopy(texture_reflectivity[f->texinfo], patch->reflectivity);
    VectorClear(patch->baselight);
    VectorClear(patch->totallight);
    patch->lightmin = 0;
    patch->styletable = 0;

    if (((texinfo[f->texinfo].flags & SURF_LIGHT) != 0) && (texinfo[f->texinfo].value != 0)) {

        bool bVar12 = true;
        bool bVar13 = false;
        float fVar25 = ColorNormalize(patch->reflectivity, color);

        float txval = (float)texinfo[f->texinfo].value;

        if (fn >= dmodels[0].numfaces) {
            if (bmodlight == 0) {
                txval = 0;
            }
            else {
                bVar13 = true;
            }
        }

        char strval[16];
        sprintf(strval, "%d", texinfo[f->texinfo].value);
        entity_t* tmp_ent = FindLightEntityByKeyValue("_lighttag", strval);
        if (tmp_ent) {
            bVar13 = true;
            ent = tmp_ent;
        }
        if (txval != 0) {
            if (bVar13) {

                const char* s = ValueForKey(ent, "_color");
                if (*s) {
                    sscanf(s, "%f %f %f", &color.x, &color.y, &color.z);
                    ColorNormalize(color, color);
                    patch->reflectivity.x = color.x * fVar25;
                    patch->reflectivity.y = color.y * fVar25;
                    patch->reflectivity.z = color.z * fVar25;
                }
                bVar13 = !*s;

                float f = FloatForKey(ent, "light");
                if (f == 0 && (f = FloatForKey(ent, "_light"), f == 0)) {
                    f = txval;
                }
                txval = f;

                s = ValueForKey(ent, "_focus");
                if (*s) {
                    patch->focus = atof(s);
                }
                if (patch->focus < 0) {
                    patch->focus = 0;
                }

                patch->lightmin = FloatForKey(ent, "_lightmin");
                patch->styletable = (int)FloatForKey(ent, "style");
                if (patch->styletable == 0) {
                    patch->styletable = (int)FloatForKey(ent, "_style");
                }

                patch->distance = FloatForKey(ent, "_distance");
                if (patch->distance == 0) {
                    patch->distance = 1;
                }

                s = ValueForKey(ent, "_lightswitch");
                if ((*s && (tmp_ent = FindLightEntityByKeyValue("targetname", s), tmp_ent)) && (s = ValueForKey(tmp_ent, "style"), *s))
                {
                    patch->styletable = atoi(s);
                }
            }
            if (txval != 0) {
                VectorScale(patch->reflectivity, txval, patch->baselight);

                for (int i = 0; i < 3; i++)
                    patch->baselight.data[i] *= color.data[i];

                VectorCopy(patch->baselight, patch->totallight);
            }
        }
    }
    num_patches++;
}


entity_t *EntityForModel(int modnum)
{
    int		i;
    char	name[16];

    sprintf(name, "*%i", modnum);
    // search the entities for one using modnum
    for (i = 0; i < num_entities; i++)
    {
        const char* s = ValueForKey(&entities[i], "model");
        if (!strcmp(s, name))
            return &entities[i];
    }

    return &entities[0];
}

/*
=============
MakePatches
=============
*/
void MakePatches(void)
{
    dmodel_t	*mod;
    vec3_t		origin;
    entity_t	*ent;

    qprintf("%i faces\n", numfaces);

    CHKVAL("MakePatches-nummodels", nummodels);
    for (int i = 0; i < nummodels; i++)
    {
        mod = &dmodels[i];
        ent = EntityForModel(i);
        // bmodels with origin brushes need to be offset into their
        // in-use position
        GetVectorForKey(ent, "origin", origin);

        //VectorCopy (vec3_origin, origin);

        // override origin with _lightorigin
        float x = 0, y = 0, z = 0;
        const char* pcVar1 = ValueForKey(ent, "_lightorigin");
        if (pcVar1 && *pcVar1) {
            sscanf(pcVar1, "%f %f %f", &x, &y, &z);
            if (x != 0 || y != 0 || z != 0) {
                origin.x = x - mod->mins.x;
                origin.y = y - mod->mins.y;
                origin.z = z - mod->mins.z;
            }
            else {
                origin.x = x;
                origin.y = y;
                origin.z = z;
            }
        }

        CHKVAL2("MakePatches-numfaces", mod->numfaces);
        for (int j = 0; j < mod->numfaces; j++)
        {
            int fn = mod->firstface + j;
            face_entity[fn] = ent;
            VectorCopy(origin, face_offset[fn]);
            dface_t* f = &dfaces[fn];
            winding_t* w = WindingFromFace(f);
            for (int k = 0; k < w->numpoints; k++)
            {
                VectorAdd(w->p[k], origin, w->p[k]);
            }
            MakePatchForFace(fn, w, ent);
        }
    }

    qprintf("%i square feet\n", (int)(totalarea / 64));
}

/*
=======================================================================

SUBDIVIDE

=======================================================================
*/

void FinishSplit(patch_t *patch, patch_t *newp)
{
    dleaf_t		*leaf;

    VectorCopy(patch->baselight, newp->baselight);
    VectorCopy(patch->totallight, newp->totallight);
    VectorCopy(patch->reflectivity, newp->reflectivity);
    newp->plane = patch->plane;
    newp->focus = patch->focus;
    newp->styletable = patch->styletable;
    newp->lightmin = patch->lightmin;
    newp->facenum = patch->facenum;
    newp->distance = patch->distance;

    if (patch->area != 0) {
        patch->area = WindingArea(patch->winding);
        newp->area = WindingArea(newp->winding);
        if (patch->area < 1)
            patch->area = 1;
        if (newp->area < 1)
            newp->area = 1;
    }
    else {
        patch->area = 1;
        newp->area = 1;
    }

    WindingCenter(patch->winding, patch->origin);
    VectorAdd(patch->origin, patch->plane->normal, patch->origin);
    leaf = PointInLeaf(patch->origin);
    patch->cluster = leaf->cluster;
    maybePhongSomething(patch->facenum, patch->origin, patch->plane->normal, patch->normal);

    WindingCenter(newp->winding, newp->origin);
    VectorAdd(newp->origin, newp->plane->normal, newp->origin);
    leaf = PointInLeaf(newp->origin);
    newp->cluster = leaf->cluster;
    maybePhongSomething(newp->facenum, newp->origin, newp->plane->normal, newp->normal);
}

/*
=============
SubdividePatch

Chops the patch only if its local bounds exceed the max size
=============
*/
void SubdividePatch(patch_t *patch)
{
    UNREACHABLE(__FUNCTION__);

    winding_t *w, *o1, *o2;
    vec3_t	mins, maxs, total;
    vec3_t	split;
    vec_t	dist;
    int		i, j;
    vec_t	v;
    patch_t	*newp;

    w = patch->winding;
    mins.x = mins.y = mins.z = 99999;
    maxs.x = maxs.y = maxs.z = -99999;
    for (i = 0; i < w->numpoints; i++)
    {
        for (j = 0; j < 3; j++)
        {
            v = w->p[i].data[j];
            if (v < mins.data[j])
                mins.data[j] = v;
            if (v > maxs.data[j])
                maxs.data[j] = v;
        }
    }
    VectorSubtract(maxs, mins, total);
    for (i = 0; i < 3; i++)
        if (total.data[i] > (subdiv + 1))
            break;
    if (i == 3)
    {
        // no splitting needed
        return;
    }

    //
    // split the winding
    //
    VectorCopy(vec3_origin, split);
    split.data[i] = 1;
    dist = (mins.data[i] + maxs.data[i])*0.5;
    ClipWindingEpsilon(w, split, dist, ON_EPSILON, &o1, &o2);

    //
    // create a new patch
    //
    if (num_patches == MAX_PATCHES)
        Error("MAX_PATCHES");
    newp = &patches[num_patches];
    num_patches++;

    newp->next = patch->next;
    patch->next = newp;

    patch->winding = o1;
    newp->winding = o2;

    FinishSplit(patch, newp);

    SubdividePatch(patch);
    SubdividePatch(newp);
}


/*
=============
DicePatch

Chops the patch by a global grid
=============
*/
void DicePatch(patch_t *patch)
{
    winding_t *w, *o1, *o2;
    vec3_t	mins, maxs;
    vec3_t	split;
    vec_t	dist;
    int		i;
    patch_t	*newp;

    CHKVAL("DicePatch-area", patch->area);
    CHKVAL("DicePatch-facenum", patch->facenum);

    float val;
    w = patch->winding;
    WindingBounds(w, mins, maxs);
    int txflags = texinfo[dfaces[patch->facenum].texinfo].flags;

    for (i = 0; i < 3; i++) {
        CHKVAL("DicePatch-txflags", txflags);

        if (!(txflags & SURF_LIGHT)) {
            if ((txflags & SURF_CURVE)) {
                CHKVAL("DicePatch-chopcurve", ((mins.data[i] + 1) / chopcurve));
                val = chopcurve;
            }
            else {
                CHKVAL("DicePatch-subdiv", true);
                val = subdiv;
            }
        }
        else {
            if (!(txflags & SURF_SKY)) {
                if (!(txflags & SURF_WARP)) {
                    CHKVAL("DicePatch-choplight", true);
                    val = choplight;
                }
                else {
                    CHKVAL("DicePatch-chopwarp", true);
                    val = chopwarp;
                }
            }
            else {
                CHKVAL("DicePatch-chopsky", true);
                val = chopsky;
            }
        }

        if (floor((mins.data[i] + 1) / val) < floor((maxs.data[i] - 1) / val)) {
            break;
        }
        CHKVAL("DicePatch-nobreak", true);
    }
    CHKVAL("DicePatch-i", i);
    if (i == 3)
    {
        // no splitting needed
        return;
    }

    //
    // split the winding
    //
    VectorCopy(vec3_origin, split);
    split.data[i] = 1;
    dist = val * (1 + floor((mins.data[i] + 1) / val));
    CHKVAL("DicePatch-dist", dist);
    ClipWindingEpsilon(w, split, dist, ON_EPSILON, &o1, &o2);

    //
    // create a new patch
    //
    if (num_patches == MAX_PATCHES)
        Error("MAX_PATCHES");
    newp = &patches[num_patches];
    num_patches++;

    newp->next = patch->next;
    patch->next = newp;

    patch->winding = o1;
    newp->winding = o2;

    FinishSplit(patch, newp);

    DicePatch(patch);
    DicePatch(newp);
}


/*
=============
SubdividePatches
=============
*/
void SubdividePatches(void)
{
    int		i, num;

    if (subdiv < 1)
        return;

    num = num_patches;	// because the list will grow
    for (i = 0; i < num; i++)
    {
        //		SubdividePatch (&patches[i]);
        DicePatch(&patches[i]);
    }
    qprintf("%i patches after subdivision\n", num_patches);
}

//=====================================================================
