#include "pch.h"
#include "cmdlib.h"
#include "mathlib.h"
#include "bspfile.h"
#include "polylib.h"
#include "threads.h"
#include "blarghrad.h"
//#include "lbmlib.h"

// GLOBALS
vec3_t ambient = { 0, 0, 0 };
int game = 0;
//char gamedir[256];
char moddir[256];
char szTempIn[32];
char szTempOut[32];
int numbounce = 8;
float maxlight = 196.f;
float lightscale = 1.f;
float direct_scale = 0.4f;
float entity_scale = 1.f;
float subdiv = 64.f;
int stopbleed = 1;
int bmodlight = 1;
int splotchfix = 1;
int radorigin = 1;
int bouncefix = 1;
int brightsurf = 1;
int invisfix = 1;
int weightcurve = 1;
int nudgefix = 1;
int shadowfilter = 1;
float saturation = 1.0f;
float stylemin = 1.0f;
float gamma = 1.0f;
int iNonTransFaces = 1;
int iTransFaces = 3;
float chopsky = 0;
float chopwarp = 0;
float choplight = 0;
float chopcurve = 0;
float patch_cutoff = 0;
float g_texscale = 0;

float minlight = 0;
int onlybounce = 0;
int nocurve = 0;
int onlyupdate = 0;
int lightwarp = 0;
int _nocolor_maybe_unweighted = 0;
int dumppatches = 0;
int extrasamples = 0;

qboolean	glview;
qboolean	nopvs;
char		source[1024];

pak_t* gamedir_paks = nullptr;
pak_t* moddir_paks = nullptr;

vec3_t texture_reflectivity[MAX_MAP_TEXINFO];

byte* TODO_malloc_palsize_768; // TODO RENAME - maybe: colormap

shadowmodel_t* g_shadow_world;
projtexture_t* g_proj_textures;

shadowfaces_unk_t* g_shadow_faces[MAX_MAP_FACES];

patch_t		*face_patches[MAX_MAP_FACES];
entity_t	*face_entity[MAX_MAP_FACES];
vec3_t      face_offset[MAX_MAP_FACES];
patch_t		patches[MAX_PATCHES];
unsigned	num_patches;

int		leafparents[MAX_MAP_LEAFS];
int		nodeparents[MAX_MAP_NODES];

directlight_t* directlights[65536];

vec3_t radiosity[MAX_PATCHES];
vec3_t illumination[MAX_PATCHES];

dplane_t backplanes[MAX_MAP_PLANES];
int fakeplanes;

facegroup_t* facegroups;
byte g_hashset_face[MAX_MAP_FACES];

unknownunk_t* g_maybe_vertex_phong[MAX_MAP_VERTS];

suninfo_t the_9_suns[9];
float g_sky_ambient;
float g_sky_surface;
vec3_t vec3_t_021d98a0;
vec3_t vec3_t_021d98b0;
int numdlights;
facelight_t		facelight[MAX_MAP_FACES];

// CODE

void UNREACHABLE(const char* where) {
    Error("ERROR - this should not execute -> %s", where);
}
void NOT_IMPLEMENTED(const char* where) {
    Error("NOT YET IMPLEMENTED -> %s", where);
}


void LoadPakdirs(void)
{
    pak_t *ppVar2;
    int iVar5;
    pak_t *ppVar6;
    char* pszCurDir;

    for (int tryGamedirOrModdir = 0; tryGamedirOrModdir <= 1; tryGamedirOrModdir++) {
        pszCurDir = (tryGamedirOrModdir == 0 ? gamedir : moddir);

        char wildcardPaks[1024];
        sprintf(wildcardPaks, "%s*.pak", pszCurDir);

        _finddata_t finddata;
        int handle = _findfirst(wildcardPaks, &finddata);
        while (handle != -1) {
            pak_t* newpak = (pak_t *)malloc(sizeof(pak_t));
            if (!newpak) {
                Error("LoadPakdirs: (newpak) malloc failed");
            }

            char filename[1024];
            sprintf(filename, "%s%s", pszCurDir, finddata.name);
            FILE* f = SafeOpenRead(filename);
            strcpy(newpak->pakfile, filename);

            // read pakheader_t
            dpackheader_t header;
            fread(&header, sizeof(header), 1, f);
            newpak->numdir = header.dirlen / sizeof(dpackfile_t);
            newpak->dir = (dpackfile_t *)malloc(header.dirlen);
            if (!newpak->dir) {
                Error("LoadPakdirs: (newpak->dir) malloc failed");
            }

            // read size of alloc (numdir*64)
            fseek(f, header.dirofs, 0);
            fread(newpak->dir, 1, header.dirlen, f);
            fclose(f);

            if (tryGamedirOrModdir == 0) {
                if (!gamedir_paks ||
                    (iVar5 = Q_strcasecmp(filename, gamedir_paks->pakfile), ppVar6 = gamedir_paks, 0 < iVar5))
                {
                    newpak->nextpak = gamedir_paks;
                    gamedir_paks = newpak;
                }
                else {
                LAB_0040c456:
                    if (ppVar6) {
                        ppVar2 = ppVar6->nextpak;
                        while ((ppVar2 && (iVar5 = Q_strcasecmp(filename, (char *)ppVar2), iVar5 < 0))) {
                            ppVar6 = ppVar6->nextpak;
                            ppVar2 = ppVar6->nextpak;
                        }
                        newpak->nextpak = ppVar6->nextpak;
                        ppVar6->nextpak = newpak;
                    }
                }
            }
            else {
                if ((moddir_paks != nullptr) &&
                    (iVar5 = Q_strcasecmp(filename, moddir_paks->pakfile), ppVar6 = moddir_paks, iVar5 < 1))
                    goto LAB_0040c456;
                newpak->nextpak = moddir_paks;
                moddir_paks = newpak;
            }
            handle = _findnext(handle, &finddata);
        }
        _findclose(handle);
    }
}

FILE * OpenFileFromDiskOrPak(char *name, int *out_filelength)
{
    char diskpath[1024];

    for (int gamedirOrModdir = 0; gamedirOrModdir <= 1; gamedirOrModdir++) {
        if (gamedirOrModdir == 0 && *moddir == '\0') {
            continue;
        }
        char* dir = (gamedirOrModdir == 0 ? moddir : gamedir);
        sprintf(diskpath, "%s%s", dir, name);
        if (FileExists(diskpath)) {
            FILE* f = SafeOpenRead(diskpath);
            *out_filelength = Q_filelength(f);
            return f;
        }
        pak_t *pak = (gamedirOrModdir == 0 ? moddir_paks : gamedir_paks);
        while (pak) {
            for (unsigned int i = 0; i < pak->numdir; i++) {
                if (!Q_strcasecmp(name, pak->dir[i].name)) {
                    FILE* f = SafeOpenRead(pak->pakfile);
                    fseek(f, pak->dir[i].filepos, 0);
                    *out_filelength = pak->dir[i].filelen;
                    return f;
                }
            }
            pak = pak->nextpak;
        }
    }
    return nullptr;
}

// returns length
int LoadPakFile(char *name, void **bytes)
{
    int length = 0;
    FILE* f = OpenFileFromDiskOrPak(name, &length);
    if (!f) {
        return 0;
    }
    *bytes = malloc((size_t)length);
    if (!bytes) {
        Error("LoadPakFile: malloc failed");
    }
    fread(bytes, length, 1, f);
    fclose(f);
    return length;
}

// TODO
int TryLoadPCX(int txnum) {
    return 0;
    NOT_IMPLEMENTED(__FUNCTION__);
    return 0;
}
int TryLoadTGA(int txnum) {
    return 0;
    NOT_IMPLEMENTED(__FUNCTION__);
    return 0;
}
int TryLoadJPG(int txnum) {
    return 0;
    NOT_IMPLEMENTED(__FUNCTION__);
    return 0;
}
int TryLoadM8(int txnum) {
    return 0;
    NOT_IMPLEMENTED(__FUNCTION__);
    return 0;
}
int TryLoadM32(int txnum) {
    return 0;
    NOT_IMPLEMENTED(__FUNCTION__);
    return 0;
}


void SetTextureReflectivity(int txnum)
{
    if (g_texscale != -1) {
        texture_reflectivity[txnum].x *= g_texscale;
        texture_reflectivity[txnum].y *= g_texscale;
        texture_reflectivity[txnum].z *= g_texscale;
        if (texture_reflectivity[txnum].x <= 1 && texture_reflectivity[txnum].y <= 1 && texture_reflectivity[txnum].z <= 1) {
            ColorNormalize(texture_reflectivity[txnum], texture_reflectivity[txnum]);
        }
    }
    else if (game == 1) {
        float maxrgb = ColorNormalize(texture_reflectivity[txnum], texture_reflectivity[txnum]);
        if (maxrgb < 0.5f) {
            texture_reflectivity[txnum].x *= maxrgb * 2;
            texture_reflectivity[txnum].y *= maxrgb * 2;
            texture_reflectivity[txnum].z *= maxrgb * 2;
        }
    }
}

void PrintTextureReflectivity(int txnum)
{
    if (!verbose)
        return;

    float x = texture_reflectivity[txnum].x;
    float y = texture_reflectivity[txnum].y;
    float z = texture_reflectivity[txnum].z;
    vec3_t local_14;
    local_14.x = x * x;
    local_14.y = y * y;
    local_14.z = z * z;
    ColorNormalize(local_14, local_14);
    printf("%-22s   ref: %.3f %.3f %.3f   emit: %.3f %.3f %.3f\n", texinfo[txnum].texture,
        x, y, z, local_14.x, local_14.y, local_14.z);
}


// returns count of faces
int MakeShadowFaces(shadowmodel_t *shmod)
{
    int resultCount = 0;
    for (int i = 0; i < dmodels[shmod->modelnum].numfaces; i++) {
        int face = dmodels[shmod->modelnum].firstface + i;

        int txflags = texinfo[(int)dfaces[face].texinfo].flags;
        if ((txflags & SURF_SKY) != 0) {
            continue;
        }

        int iVar5 = -1;
        if ((bool)(txflags & SURF_TRANS33) != (bool)(txflags & SURF_TRANS66)) {
            if ((shmod->nonTransFaces != 0) &&
                (((char)txflags < '\0' || ((shmod->modelnum != 0 && (shmod->nonTransFaces < 0)))))) {
                iVar5 = 0; // use nonTransFaces
            }
        }
        else if ((shmod->transFaces != 0) &&
                ((txflags & SURF_NODRAW) ||
                (shmod->modelnum != 0 && (shmod->transFaces < 0)))) {
            iVar5 = 1; // use transFaces
        }

        if (iVar5 == -1) {
            continue;
        }

        shadowfaces_unk_t* shfunk = (shadowfaces_unk_t*)malloc(sizeof(shadowfaces_unk_t));
        if (!shfunk) {
            Error("MakeShadowFaces: malloc failed");
        }
        memset(shfunk, 0, sizeof(shadowfaces_unk_t));
        shfunk->face = face;
        // TODO this field is based on flags... figure out name -- identifies to use trans or nontrans?
        shfunk->UNKNOWN_FIELD_0xC = iVar5;
        int c = iVar5 == 0 ? shmod->nonTransFaces : shmod->transFaces;
        if (c == 3) {
            // TODO - rename - bool=true means to project the texture
            shfunk->maybe_bool = 1;
        }
        shfunk->mins.z = 99999.f;
        shfunk->mins.y = 99999.f;
        shfunk->mins.x = 99999.f;
        shfunk->maxs.z = -99999.f;
        shfunk->maxs.y = -99999.f;
        shfunk->maxs.x = -99999.f;
        for (int j = 0; j < dfaces[face].numedges; j++) {
            unsigned short uVar1;
            int edge = dsurfedges[dfaces[face].firstedge + j];
            if (edge < 0) {
                uVar1 = dedges[-edge].v[1];
            } else {
                uVar1 = dedges[edge].v[0];
            }
            AddPointToBounds(dvertexes[uVar1].point, shfunk->mins, shfunk->maxs);
        }
        shfunk->next = shmod->shadownext;
        shmod->shadownext = shfunk;
        g_shadow_faces[face] = shfunk;
        resultCount++;
    }
    return resultCount;
}


void MakeShadowModels(void)
{
    const char *pcVar1;
    const char *pcVar2;
    shadowfaces_unk_t **ppsVar3;
    bool bVar4;
    int countModelFaces;
    int totalWorldspawnFaces;
    int *pTrans;
    int *pNonTrans;
    int totalModelFaces;

    memset(g_shadow_faces, 0, MAX_MAP_FACES);
    if (iNonTransFaces != -1) {
        totalWorldspawnFaces = 0;
        countModelFaces = 0;
        totalModelFaces = 0;
        for (int i = 0; i < num_entities; i++) {
            entity_t *ent = &entities[i];

            pcVar1 = ValueForKey(ent, "classname");
            if (!strcmp(ValueForKey(ent, "classname"), "worldspawn")) {
                pcVar1 = ValueForKey(ent, "_shadow");
                if ((*pcVar1 != '\0') &&
                    sscanf(pcVar1, "%i %i", &iNonTransFaces, &iTransFaces) == 1) {
                    iTransFaces = iNonTransFaces;
                }
            } else {
                pcVar1 = ValueForKey(ent, "_shadow");
                if (*pcVar1 != '\0') {
                    /* value is modnum, string format ex: "*23" */
                    pcVar2 = ValueForKey(ent, "model");
                    shadowmodel_t *bmodel = (shadowmodel_t*)malloc(sizeof(shadowmodel_t));
                    if (!bmodel) {
                        Error("MakeShadowModels: bmodel malloc failed");
                    }
                    bmodel->shadownext = nullptr;
                    bmodel->modelnum = 0;
                    bmodel->nonTransFaces = 0;
                    bmodel->transFaces = 0;
                    bmodel->modelnum = atoi((char *)(pcVar2 + 1));
                    if (sscanf(pcVar1, "%i %i", &bmodel->nonTransFaces, &bmodel->transFaces) == 1) {
                        bmodel->transFaces = bmodel->nonTransFaces;
                    }
                    bmodel->next = g_shadow_world;
                    g_shadow_world = bmodel;
                    countModelFaces += MakeShadowFaces(bmodel);
                    if (bmodel->nonTransFaces < 0) {
                        bmodel->nonTransFaces = -bmodel->nonTransFaces;
                    }
                    if (bmodel->transFaces < 0) {
                        bmodel->transFaces = -bmodel->transFaces;
                    }
                }
            }
            totalModelFaces = countModelFaces;
        }
        if ((iNonTransFaces != 0) || (iTransFaces != 0)) {
            shadowmodel_t *world = (shadowmodel_t*)malloc(sizeof(shadowmodel_t));
            if (!world) {
                Error("MakeShadowModels: world malloc failed");
            }
            world->shadownext = nullptr;
            world->modelnum = 0;
            world->nonTransFaces = 0;
            world->transFaces = 0;
            world->modelnum = 0;
            world->nonTransFaces = iNonTransFaces;
            world->transFaces = iTransFaces;
            world->next = g_shadow_world;
            g_shadow_world = world;
            totalWorldspawnFaces = MakeShadowFaces(world);
        }
        qprintf("%i shadowfaces found (%i world, %i bmodel)\n", totalWorldspawnFaces + totalModelFaces,
            totalWorldspawnFaces, totalModelFaces);
    }
    return;
}



// if the texture name exists, allocate a texture, else return null.
projtexture_t* CreateProjTexture(char* name, int width, int height)
{
    for (shadowmodel_t* psVar6 = g_shadow_world; psVar6; psVar6 = psVar6->next) {
        for (shadowfaces_unk_t* psVar2 = psVar6->shadownext; psVar2; psVar2 = psVar2->next) {
            if (Q_strcasecmp(name, texinfo[dfaces[psVar2->face].texinfo].texture)) {
                continue;
            }
            projtexture_t* ptex = (projtexture_t*)malloc(sizeof(projtexture_t));
            if (!ptex) {
                Error("CreateProjTexture: projtexture malloc failed");
            }
            ptex->texture32 = (byte*)malloc(width * height * 4);
            if (!ptex->texture32) {
                Error("CreateProjTexture: texture malloc failed");
            }
            ptex->width = width;
            ptex->height = height;
            ptex->has_transparent_pixels = 0;
            return ptex;
        }
    }
    return nullptr;
}


void StoreTextureForProjection(projtexture_t *projtex, char *name)
{
    bool isTextureUsed = projtex->has_transparent_pixels != 0;
    for (shadowmodel_t* local_4 = g_shadow_world; local_4; local_4 = local_4->next) {
        for (shadowfaces_unk_t* psVar2 = local_4->shadownext; psVar2; psVar2 = psVar2->next) {
            if (!Q_strcasecmp(name, texinfo[dfaces[psVar2->face].texinfo].texture)) {
                psVar2->projtex = projtex;
                if (psVar2->maybe_bool) {
                    isTextureUsed = true;
                }
            }
        }
    }
    if (!isTextureUsed) {
        for (shadowmodel_t *psVar3 = g_shadow_world; psVar3; psVar3 = psVar3->next) {
            for (shadowfaces_unk_t* sf = psVar3->shadownext; sf; sf = sf->next) {
                if (sf->projtex == projtex) {
                    sf->projtex = nullptr;
                }
            }
        }
        free(projtex->texture32);
        free(projtex);
        return;
    }
    projtex->next = g_proj_textures;
    g_proj_textures = projtex;
    qprintf("%s stored for projection\n", name);
}

void FreeProjTextures(void)
{
    // clear references to textures
    for (shadowmodel_t* psVar4 = g_shadow_world; psVar4; psVar4 = psVar4->next) {
        for (shadowfaces_unk_t* psVar1 = psVar4->shadownext; psVar1; psVar1 = psVar1->next) {
            psVar1->projtex = 0;
        }
    }

    // free textures
    for (projtexture_t* puVar3 = g_proj_textures; puVar3; puVar3 = puVar3->next) {
        projtexture_t* tmp = puVar3->next;
        free(puVar3->texture32);
        free(puVar3);
        puVar3 = tmp;
    }
}


int CalcTextureReflectivity(int txnum)
{
    char path[1024];
    miptex_t* malloc_bytes;

    if (!TODO_malloc_palsize_768) {
        TODO_malloc_palsize_768 = (byte*)malloc(768);
        if (!TODO_malloc_palsize_768) {
            Error("CalcTextureReflectivity: malloc failed");
        }
        sprintf(path, "pics/colormap.pcx");
        if (!LoadPakFile(path, (void**)&malloc_bytes)) {
            printf("WARNING: Colormap not found - no colored texture lighting\n");
            memset(TODO_malloc_palsize_768, 0xFF, 768);
        }
        else {
            memcpy(TODO_malloc_palsize_768, malloc_bytes, 768);
            free(malloc_bytes);
        }
    }
    sprintf(path, "textures/%s.wal", texinfo[txnum].texture);
    if (!LoadPakFile(path, (void**)&malloc_bytes)) {
        return 0;
    }
    int pixelcount = malloc_bytes->width * malloc_bytes->height;
    int rgbsum[3] = { 0,0,0 };
    int rgbcount = 0;

    projtexture_t* projtex = CreateProjTexture(texinfo[txnum].texture, malloc_bytes->width, malloc_bytes->height);
    byte* pic = &((byte*)malloc_bytes)[malloc_bytes->offsets[0]];
    for (int i = 0; i < pixelcount; i++) {
        byte color = pic[i];
        if (color == 0xff) {
            rgbcount++;
            if (projtex) {
                projtex->texture32[i * 4 + 3] = 0;
                projtex->has_transparent_pixels = 1;
            }
        }
        else {
            for (int i = 0; i < 3; i++) {
                byte chan = TODO_malloc_palsize_768[color * 3 + i];
                rgbsum[i] += chan;
                if (projtex) {
                    projtex->texture32[i * 4 + i] = chan;
                }
            }
            if (projtex) {
                projtex->texture32[i * 4 + 3] = 0xff;
            }
        }
    }
    if (projtex) {
        StoreTextureForProjection(projtex, texinfo[txnum].texture);
    }
    if (rgbcount == pixelcount) {
        VectorClear(texture_reflectivity[txnum]);
    }
    else {
        texture_reflectivity[txnum].x = (float)(rgbsum[0] / (pixelcount - rgbcount)) / 255;
        texture_reflectivity[txnum].y = (float)(rgbsum[1] / (pixelcount - rgbcount)) / 255;
        texture_reflectivity[txnum].z = (float)(rgbsum[2] / (pixelcount - rgbcount)) / 255;
    }
    SetTextureReflectivity(txnum);
    free(malloc_bytes);
    return 1;
}


void CalcTextureReflectivityMain(void)
{
    qprintf("----- Texture Reflectivity -----\n");
    LoadPakdirs();
    texture_reflectivity[0].x = 0.5f;
    texture_reflectivity[0].y = 0.5f;
    texture_reflectivity[0].z = 0.5f;

    vec3_t* pfVar6 = texture_reflectivity;
    for (int txnum = 0; txnum < numtexinfo; txnum++, pfVar6++) {
        texinfo_t* tx = &texinfo[txnum];

        // find if this texture has already been calculated
        int iVar4 = 0;
        for ( ; iVar4 < txnum; iVar4++) {
            if (!Q_strcasecmp(texinfo[txnum].texture, texinfo[iVar4].texture)) {
                pfVar6->x = texture_reflectivity[iVar4].x;
                pfVar6->y = texture_reflectivity[iVar4].y;
                pfVar6->z = texture_reflectivity[iVar4].z;
                break;
            }
        }

        // if first time encountering, calculate ref
        if (iVar4 == txnum) {
            iVar4 = TryLoadTGA(txnum);
            if ((((iVar4 == 0) && (iVar4 = TryLoadJPG(txnum), iVar4 == 0)) &&
                    (iVar4 = TryLoadM32(txnum), iVar4 == 0)) &&
                    (((iVar4 = TryLoadM8(txnum), iVar4 == 0 && (iVar4 = TryLoadPCX(txnum), iVar4 == 0)) &&
                    (iVar4 = CalcTextureReflectivity(txnum), iVar4 == 0)))) {
                printf("Couldn\'t load %s\n", tx->texture);
                pfVar6->x = 0.5f;
                pfVar6->y = 0.5f;
                pfVar6->z = 0.5f;
            }
            else {
                PrintTextureReflectivity(txnum);
            }
        }
    }
   
    for (int gameOrMod = 0; gameOrMod <= 1; gameOrMod++) {
        pak_t* cur = (gameOrMod == 0 ? gamedir_paks : moddir_paks);
        while (cur) {
            pak_t* tmp = cur->nextpak;
            free(cur->dir);
            free(cur);
            cur = tmp;
        }
    }
}

/*
=============
MakeBackplanes
=============
*/
void MakeBackplanes(void)
{
    int		i;

    for (i = 0; i < numplanes; i++)
    {
        backplanes[i].dist = -dplanes[i].dist;
        VectorSubtract(vec3_origin, dplanes[i].normal, backplanes[i].normal);
    }
}


/*
=============
MakeParents
=============
*/
void MakeParents(int nodenum, int parent)
{
    int		i, j;
    dnode_t	*node;

    nodeparents[nodenum] = parent;
    node = &dnodes[nodenum];

    for (i = 0; i < 2; i++)
    {
        j = node->children[i];
        if (j < 0)
            leafparents[-j - 1] = nodenum;
        else
            MakeParents(j, nodenum);
    }
}


/*
===================================================================

TRANSFER SCALES

===================================================================
*/

int	PointInLeafnum(vec3_t point)
{
    int		nodenum;
    vec_t	dist;
    dnode_t	*node;
    dplane_t	*plane;

    nodenum = 0;
    while (nodenum >= 0)
    {
        node = &dnodes[nodenum];
        plane = &dplanes[node->planenum];
        dist = DotProduct(point, plane->normal) - plane->dist;
        if (dist > 0)
            nodenum = node->children[0];
        else
            nodenum = node->children[1];
    }

    return -nodenum - 1;
}


dleaf_t		*PointInLeaf(vec3_t point)
{
    int		num;

    num = PointInLeafnum(point);
    return &dleafs[num];
}


qboolean PvsForOrigin(vec3_t org, byte *pvs)
{
    dleaf_t	*leaf;

    if (!visdatasize)
    {
        memset(pvs, 255, (numleafs + 7) / 8);
        return true;
    }

    leaf = PointInLeaf(org);
    if (leaf->cluster == -1)
        return false;		// in solid leaf

    DecompressVis(dvisdata + dvis->bitofs[leaf->cluster][DVIS_PVS], pvs);
    return true;
}

/*
=============
CollectLight
=============
*/
float CollectLight(void)
{
    float total = 0;

    patch_t	*patch = patches;
    for (int i = 0; i < num_patches; i++, patch++)
    {
        // skys never collect light, it is just dropped
        if (( texinfo[dfaces[patch->facenum].texinfo].flags & SURF_SKY) != 0)
        {
            VectorClear(radiosity[i]);
            VectorClear(illumination[i]);
            continue;
        }

        patch->totallight.x += illumination[i].x / patch->area;
        patch->totallight.y += illumination[i].y / patch->area;
        patch->totallight.z += illumination[i].z / patch->area;
        radiosity[i].x = illumination[i].x * patch->reflectivity.x;
        radiosity[i].y = illumination[i].y * patch->reflectivity.y;
        radiosity[i].z = illumination[i].z * patch->reflectivity.z;

        total += radiosity[i].x + radiosity[i].y + radiosity[i].z;
        VectorClear(illumination[i]);
    }

    return total;
}



/*
=============
ShootLight

Send light out to other patches
  Run multi-threaded
=============
*/
void ShootLight(int patchnum)
{
    int			k, l;
    transfer_t	*trans;
    int			num;
    patch_t		*patch;
    vec3_t		send;

    // this is the amount of light we are distributing
    // prescale it so that multiplying by the 16 bit
    // transfer values gives a proper output value
    for (k = 0; k < 3; k++)
        send.data[k] = radiosity[patchnum].data[k] / 0x10000;
    patch = &patches[patchnum];

    trans = patch->transfers;
    num = patch->numtransfers;

    for (k = 0; k < num; k++, trans++)
    {
        for (l = 0; l < 3; l++)
            illumination[trans->patch].data[l] += send.data[l] * trans->transfer;
    }
}


/*
=============
WriteWorld
=============
*/
void WriteWorld(char *name)
{
    int		i, j;
    FILE		*out;
    patch_t		*patch;
    winding_t	*w;

    out = fopen(name, "w");
    if (!out)
        Error("Couldn't open %s", name);

    for (j = 0, patch = patches; j < num_patches; j++, patch++)
    {
        w = patch->winding;
        fprintf(out, "%i\n", w->numpoints);
        for (i = 0; i < w->numpoints; i++)
        {
            fprintf(out, "%5.2f %5.2f %5.2f %5.3f %5.3f %5.3f\n",
                w->p[i].x,
                w->p[i].y,
                w->p[i].z,
                patch->totallight.x,
                patch->totallight.y,
                patch->totallight.z);
        }
        fprintf(out, "\n");
    }

    fclose(out);
}


/*
=============
BounceLight
=============
*/
void BounceLight(void)
{
    int		i, j;
    float	added;
    char	name[64];
    patch_t	*p;

    for (i = 0; i < num_patches; i++)
    {
        p = &patches[i];
        for (j = 0; j < 3; j++)
        {
            //			p->totallight[j] = p->samplelight[j];
            radiosity[i].data[j] = p->samplelight.data[j] * p->reflectivity.data[j] * p->area;
        }
    }

    for (i = 0; i < numbounce; i++)
    {
        RunThreadsOn(num_patches, false, ShootLight);
        added = CollectLight();

        qprintf("bounce:%i added:%f\n", i, added);
        if (dumppatches && (i == 0 || i == numbounce - 1))
        {
            sprintf(name, "bounce%i.txt", i);
            WriteWorld(name);
        }
    }
}

/*
=============
MakeTransfers

=============
*/
int	total_transfer;

void MakeTransfers(int i)
{
    int			j;
    vec3_t		delta;
    float		trans;
    int			itrans;
    patch_t		*patch;
    patch_t		*patch2;
    float		total;
    dplane_t	plane;
    vec3_t		origin;
    float		transfers[MAX_PATCHES], *all_transfers;
    int			s;
    int			itotal;
    byte		pvs[(MAX_MAP_LEAFS + 7) / 8];
    int			cluster;

    patch = patches + i;
    total = 0.f;

    VectorCopy(patch->origin, origin);
    plane = *patch->plane;

    if (!PvsForOrigin(patch->origin, pvs))
        return;

    if (patch->area == 0.f) {
        return;
    }

    float splotch_dist = 0.f;
    if (splotchfix != 0) {
        vec3_t mins, maxs;
        WindingBounds(patch->winding, mins, maxs);
        for (int i = 0; i < 3; i++) {
            float d = maxs.data[i] - mins.data[i];
            if (splotch_dist < d) {
                splotch_dist = d;
            }
        }
        splotch_dist *= 0.75f;
    }

    // find out which patch2s will collect light
    // from patch

    all_transfers = transfers;
    patch->numtransfers = 0;
    for (j = 0, patch2 = patches; j < num_patches; j++, patch2++)
    {
        transfers[j] = 0;

        if (j == i)
            continue;

        if (patch2->area == 0.f)
            continue;

        // check pvs bit
        if (!nopvs)
        {
            cluster = patch2->cluster;
            if (cluster == -1)
                continue;
            if (!(pvs[cluster >> 3] & (1 << (cluster & 7))))
                continue;		// not in pvs
        }

        // calculate vector
        VectorSubtract(patch2->origin, origin, delta);
        float dist = VectorNormalize(delta, delta);
        if (!dist)
            continue;	// should never happen

        // relative angles
        float p1_scale = DotProduct(delta, patch->field_0x6c);
        if (bouncefix) {
            if (p1_scale < 0.f) {
                continue;
            }
        }
        float p2_scale = -DotProduct(delta, patch2->field_0x6c);  // TODO - field_0x6c comes from maybePhongSomething. this value is takes the place of patch2->plane->normal, so it is probably related to a plane normal.
        float scale = p2_scale * p1_scale;
        if (scale == 0.f)
            continue;

        if (splotchfix && dist < splotch_dist) {
            /*
            float fVar4 = 1.f / splotch_dist;
            float fVar2 = (splotch_dist - dist) * fVar4;
            float fVar3 = fVar4 * p1_scale * dist;
            float fVar1 = fVar4 * p2_scale * dist;
            scale = (2 * fVar2 + fVar1) * (fVar3 + fVar2);*/

            float a = splotch_dist;

            float p = dist * p2_scale + 2 * (a - dist);
            float q = dist * p1_scale - dist + a;
            scale = p * q / (a * a);

            dist = splotch_dist;
        }

        trans = scale * patch2->area / (dist*dist);
        if (trans <= patch_cutoff)
            continue;

        // check exact transfer
        if (TestLine_shadow(patch->origin, patch2->origin, nullptr, nullptr))
            continue;

        transfers[j] = trans;
        if (trans > 0)
        {
            total += trans;
            patch->numtransfers++;
        }
    }

    // copy the transfers out and normalize
    // total should be somewhere near PI if everything went right
    // because partial occlusion isn't accounted for, and nearby
    // patches have underestimated form factors, it will usually
    // be higher than PI
    if (patch->numtransfers)
    {
        transfer_t	*t;

        if (patch->numtransfers < 0 || patch->numtransfers > MAX_PATCHES)
            Error("Weird numtransfers");
        s = patch->numtransfers * sizeof(transfer_t);
        patch->transfers = (transfer_t*)malloc(s);
        if (!patch->transfers)
            Error("Memory allocation failure");

        //
        // normalize all transfers so all of the light
        // is transfered to the surroundings
        //
        t = patch->transfers;
        itotal = 0;
        for (j = 0; j < num_patches; j++)
        {
            if (transfers[j] <= 0)
                continue;
            itrans = sqrt(transfers[j] / total) * 0x10000;
            itotal += itrans;
            t->transfer = itrans;
            t->patch = j;
            t++;
        }
    }

    // don't bother locking around this.  not that important.
    total_transfer += patch->numtransfers;
}


/*
=============
FreeTransfers
=============
*/
void FreeTransfers()
{
    for (int i = 0; i < num_patches; i++)
    {
        free(patches[i].transfers);
        patches[i].transfers = NULL;
    }
}

void FreeDirectLights()
{
    for (int i = 0; i < 65536; i++) {
        directlight_t* c = directlights[i];
        while (c) {
            directlight_t* tmp = c->m_next;
            free(c);
            c = tmp;
        }
    }
}

void BuildFaceGroups()
{
    facegroups = (facegroup_t *)malloc(numfaces * sizeof(facegroup_t));
    if (!facegroups) {
        Error("BuildFaceGroups: malloc failed");
    }

    for (int i = 0; i < numfaces; i++) {
        if (texinfo[dfaces[i].texinfo].value == 0) {
            facegroups[i].byte1 = 0;
        }
        else {
            if ((texinfo[dfaces[i].texinfo].flags & SURF_LIGHT) == 0) {
                facegroups[i].byte1 = 2;
            }
            else {
                facegroups[i].byte1 = 1;
            }
        }
        facegroups[i].start = (unsigned short)i;
        facegroups[i].end = (unsigned short)i;
    }

    if (0 < numfaces) {
        int i = 0;
        unsigned int facenum = 0;
        unsigned int uVar1;
        do {
            uVar1 = facenum + 1;
            if ((int)uVar1 < numfaces) {
                unsigned int uVar9 = uVar1;
                dface_t* nextface = dfaces + i + 1;
                int j = i;
                do {
                    if (facegroups[i].byte1 == facegroups[j + 1].byte1) {
                        if (facegroups[i].byte1 == 0) {
                            if ((dfaces[i].planenum == nextface->planenum) && (dfaces[i].side == nextface->side)) {
                                if (stopbleed != 0) 
                                    goto LAB_0040588f;
                            LAB_0040589d:
                                unsigned short start = facegroups[i].start;
                                unsigned int end = start;
                                while (end != facenum) {
                                    if (end == uVar9) {
                                        if (end != facenum) 
                                            goto LAB_00405905;
                                        break;
                                    }
                                    end = facegroups[end].end;
                                }
                                unsigned short uVar3 = facegroups[j + 1].end;
                                facegroups[i].start = (unsigned short)uVar9;
                                facegroups[j + 1].end = (unsigned short)facenum;
                                facegroups[start].end = uVar3;
                                facegroups[uVar3].start = start;
                            }
                        }
                        else {
                            if ((texinfo[dfaces[i].texinfo].value == texinfo[nextface->texinfo].value) &&
                                (g_hashset_face[facenum] == g_hashset_face[uVar9])) {
                            LAB_0040588f:
                                if (FacesHaveSharedVertexes(facenum, uVar9))
                                    goto LAB_0040589d;
                            }
                        }
                    }
                LAB_00405905:
                    uVar9 += 1;
                    j++;
                    nextface++;
                } while ((int)uVar9 < numfaces);
            }
            i++;
            facenum++;
        } while ((int)uVar1 < numfaces);
    }
}

// NOTE: original arghrad comments out the Error() call, so it runs but no-ops.
void CheckPatches(void)
{
    int		i;
    patch_t	*patch;

    for (i = 0; i < num_patches; i++)
    {
        patch = &patches[i];
        if (patch->totallight.x < 0 || patch->totallight.y < 0 || patch->totallight.z < 0)
            Error("negative patch totallight\n");
    }
}

/*
=============
AddSampleToPatch

Take the sample's collected light and
add it back into the apropriate patch
for the radiosity pass.

The sample is added to all patches that might include
any part of it.  They are counted and averaged, so it
doesn't generate extra light.
=============
*/
void AddSampleToPatch(vec3_t pos, vec3_t color, int facenum)
{
    patch_t	*patch;
    vec3_t	mins, maxs;
    int		i;

    if (numbounce == 0)
        return;
    if (color.x + color.y + color.z < 3)
        return;

    for (patch = face_patches[facenum]; patch; patch = patch->next)
    {
        // see if the point is in this patch (roughly)
        WindingBounds(patch->winding, mins, maxs);
        for (i = 0; i < 3; i++)
        {
            if (mins.data[i] > pos.data[i] + 16)
                goto nextpatch;
            if (maxs.data[i] < pos.data[i] - 16)
                goto nextpatch;
        }

        // add the sample to the patch
        patch->samples++;
        VectorAdd(patch->samplelight, color, patch->samplelight);
    nextpatch:;
    }

}


/*
=============
BuildFacelights
=============
*/
float	sampleofs[5][2] =
{ {0,0}, {-0.25, -0.25}, {0.25, -0.25}, {0.25, 0.25}, {-0.25, 0.25} };


void BuildFacelights(int facenum)
{
    dface_t	*f;
    lightinfo_t	l[5];
    vec3_t		*styletable[MAX_LSTYLES];
    int			i, j;
    float		*spot;
    patch_t		*patch;
    int			numsamples;
    int			tablesize;
    facelight_t		*fl;

    f = &dfaces[facenum];

    // check non-lit textures
    if (lightwarp && (texinfo[f->texinfo].flags & SURF_WARP))
        return;
    if (texinfo[f->texinfo].flags & SURF_SKY)
        return;
    if (game == 2 && (texinfo[f->texinfo].flags & H2_SURF_TALL_WALL) &&
        (f->styles[0] | f->styles[1] | f->styles[2]))
        return;

    memset(styletable, 0, sizeof(styletable));

    if (extrasamples)
        numsamples = 5;
    else
        numsamples = 1;
    for (i = 0; i < numsamples; i++)
    {
        memset(&l[i], 0, sizeof(l[i]));
        l[i].surfnum = facenum;
        l[i].face = f;
        VectorCopy(dplanes[f->planenum].normal, l[i].facenormal);
        l[i].facedist = dplanes[f->planenum].dist;
        if (f->side)
        {
            VectorSubtract(vec3_origin, l[i].facenormal, l[i].facenormal);
            l[i].facedist = -l[i].facedist;
        }

        // get the origin offset for rotating bmodels
        VectorCopy(face_offset[facenum], l[i].modelorg);

        CalcFaceVectors(&l[i]);
        CalcFaceExtents(&l[i]);
        if (nudgefix == 0)
        {
            CalcPoints(&l[i], sampleofs[i][0], sampleofs[i][1], facenum);
        }
        else
        {
            CalcPoints2(&l[i], sampleofs[i][0], sampleofs[i][1], facenum);
        }
    }

    tablesize = l[0].numsurfpt * sizeof(vec3_t);
    styletable[0] = (vec3_t*)malloc(tablesize);
    if (!styletable[0]) {
        Error("BuildFaceLights: (styletable[0]) malloc failed");
    }
    memset(styletable[0], 0, tablesize);

    vec3_t* bouncelight = (vec3_t*)malloc(tablesize);
    if (!bouncelight) {
        Error("BuildFaceLights: (bouncelight) malloc failed");
    }
    memset(bouncelight, 0, tablesize);

    fl = &facelight[facenum];
    fl->numsamples = l[0].numsurfpt;
    fl->origins = (float*)malloc(tablesize);
    if (!fl->origins) {
        Error("BuildFaceLights: (origins) malloc failed");
    }
    memcpy(fl->origins, l[0].realpt, tablesize);

    for (i = 0; i < l[0].numsurfpt; i++)
    {

        for (j = 0; j < numsamples; j++)
        {
            vec3_t facenormal;

            maybePhongSomething(facenum, l[j].realpt[i], l[0].facenormal, /*out*/facenormal);

            GatherSampleLight(l[j].surfpt[i], l[j].realpt[i], facenormal, styletable, bouncelight, i, tablesize, 1.f / numsamples);
        }

        // contribute the sample to one or more patches
        AddSampleToPatch(l[0].surfpt[i], bouncelight[i], facenum);
    }

    free(bouncelight);

    for (i = 0; i < numsamples; i++) {
        free(l[i].realpt);
        free(l[i].surfpt);
    }

    // average up the direct light on each patch for radiosity
    int patch_count = 0;
    for (patch = face_patches[facenum]; patch; patch = patch->next)
    {
        if (patch->samples)
        {
            VectorScale(patch->samplelight, 1.0 / patch->samples, patch->samplelight);
        }
        else
        {
            //			printf ("patch with no samples\n");
        }
        patch_count++;
    }

    // heretic 2
    if ((game == 2) && ((texinfo[f->texinfo].flags & H2_SURF_TALL_WALL) != 0)) {
        f->styles[0] = 0;
        f->styles[1] = 0;
        f->styles[2] = 0;

        patch_t* p = face_patches[facenum];
        while (p) {
            if (p->samples != 0) {
                dfaces[facenum].styles[0] += p->samplelight.x / patch_count;
                dfaces[facenum].styles[1] += p->samplelight.y / patch_count;
                dfaces[facenum].styles[2] += p->samplelight.z / patch_count;
            }
            p = p->next;
        }
        for (i = 0; i < MAX_LSTYLES; i++) {
            if (styletable[i]) {
                free(styletable[i]);
            }
        }
        free(fl->origins);
        fl->origins = nullptr;
        fl->numsamples = 0;
        return;
    }

    int style = face_patches[facenum]->styletable;
    if (!styletable[style]) {
        styletable[style] = (vec3_t *)malloc(tablesize);
        if (!styletable[style]) {
            Error("BuildFaceLights: (styletable[style]) malloc failed");
        }
        memset(styletable[style], 0, tablesize);
    }

    for (i = 0; i < MAX_LSTYLES; i++)
    {
        if (!styletable[i])
            continue;
        if (fl->numstyles == MAX_STYLES)
            break;
        fl->samples[fl->numstyles] = (float*) styletable[i]; // TODO FIX CAST
        fl->stylenums[fl->numstyles] = i;
        fl->numstyles++;
    }

    // the light from DIRECT_LIGHTS is sent out, but the
    // texture itself should still be full bright

/*    if (face_patches[facenum]->baselight[0] >= DIRECT_LIGHT ||
        face_patches[facenum]->baselight[1] >= DIRECT_LIGHT ||
        face_patches[facenum]->baselight[2] >= DIRECT_LIGHT
        )
    {
        spot = fl->samples[0];
        for (i = 0; i < l[0].numsurfpt; i++, spot += 3)
        {
            VectorAdd(spot, face_patches[facenum]->baselight, spot);
        }
    }*/

    if ((texinfo[f->texinfo].flags & SURF_LIGHT) != 0) {
        vec3_t vStack1748;
        VectorNormalize(face_patches[facenum]->baselight, vStack1748);
        VectorScale(vStack1748, face_patches[facenum]->lightmin, vStack1748);
        vec3_t* pstyle = styletable[style];
        for (i = 0; i < l[0].numsurfpt; i++, pstyle++) {
            if (brightsurf != 0) {
                VectorAdd((*pstyle), face_patches[facenum]->baselight, (*pstyle));
            }
            VectorAdd((*pstyle), vStack1748, (*pstyle));
        }
    }
}

void maybeInitPhong()
{
    unknownunk_t *puVar10;
    dplane_t *val;
    float local_60;
    vec3_t local_50;
    vec3_t local_44;
    vec3_t local_2c;

    unsigned short v;

    memset(g_hashset_face, -1, sizeof(g_hashset_face));

    for (int i = 0; i < numfaces; i++) {
        dface_t* face = &dfaces[i];
        if ((texinfo[face->texinfo].value != 0) &&
            ((texinfo[face->texinfo].flags & SURF_LIGHT) == 0)) {
            VectorClear(local_44);
            for (int j = 0; j < face->numedges; j++) {
                int edge = dsurfedges[face->firstedge + j];
                if (edge < 0) {
                    v = dedges[-edge].v[1];
                }
                else {
                    v = dedges[edge].v[0];
                }
                VectorAdd(local_44, dvertexes[v].point, local_44);
            }
            float scale = 1 / (float)face->numedges;
            VectorScale(local_44, scale, local_44);
            if (face->side == 0) {
                val = &dplanes[face->planenum];
            }
            else {
                val = &backplanes[face->planenum];
            }
            VectorMA(local_44, -0.5f, val->normal, local_50);
            int num = PointInLeafnum(local_50);
            if (num != -1) {
                if (face->side == 0) {
                    val = &dplanes[face->planenum];
                }
                else {
                    val = &backplanes[face->planenum];
                }
                VectorMA(local_44, 0.5f, val->normal, local_50);
                num = PointInLeafnum(local_50);

                g_hashset_face[i] = (byte)(dleafs[num].contents & (CONTENTS_LAVA|CONTENTS_SLIME|CONTENTS_WATER|CONTENTS_MIST));
            }
        }
    }

    for (int k = 0; k < numfaces; k++) {
        dface_t* face = &dfaces[k];
        int txLightValue = texinfo[face->texinfo].value;
        if ((txLightValue != 0) && ((texinfo[face->texinfo].flags & SURF_LIGHT) == 0)) {
            texinfo[face->texinfo].flags |= SURF_CURVE;
            for (int j = 0; j < face->numedges; j++) {
                int edge = dsurfedges[face->firstedge + j];
                if (edge < 0) {
                    v = dedges[-edge].v[1];
                }
                else {
                    v = dedges[edge].v[0];
                }
                puVar10 = g_maybe_vertex_phong[v];
                while (puVar10) {
                    if ((puVar10->txLightValue == txLightValue) &&
                        (puVar10->face_bool == (bool)g_hashset_face[k])) 
                        goto LAB_0040eb71;
                    puVar10 = puVar10->next;
                }
                puVar10 = (unknownunk_t *)malloc(sizeof(unknownunk_t));
                puVar10->txLightValue = txLightValue;
                puVar10->face_bool = g_hashset_face[k];
                VectorClear(puVar10->normal);
                puVar10->next = g_maybe_vertex_phong[v];
                g_maybe_vertex_phong[v] = puVar10;

                // TODO - debug original to determine starting value
                local_60 = 0;

            LAB_0040eb71:
                if (weightcurve == 0) {
                    local_60 = 1.f;
                }
                else {
                    int edge2 = dsurfedges[face->firstedge];
                    if (edge2 < 0) {
                        v = dedges[-edge2].v[1];
                    }
                    else {
                        v = dedges[edge2].v[0];
                    }

                    VectorCopy(dvertexes[v].point, local_50);
                    for (int i = 1; i < face->numedges - 1; i++) {
                        int edge3 = dsurfedges[face->firstedge + i];
                        vec3_t vert, d, a;
                        int uVar5;
                        if (edge3 < 0) {
                            uVar5 = dedges[-edge3].v[1];
                            VectorCopy(dvertexes[uVar5].point, vert);
                            uVar5 = dedges[-edge3].v[0];
                        }
                        else {
                            uVar5 = dedges[edge3].v[0];
                            VectorCopy(dvertexes[uVar5].point, vert);
                            uVar5 = dedges[edge3].v[1];
                        }
                        VectorCopy(dvertexes[uVar5].point, local_44);
                        VectorSubtract(local_44, local_50, d);
                        VectorSubtract(vert, local_50, a);
                        CrossProduct(d, a, local_2c);

                        // TODO REVIEW - original code does not appear to initialize local_60
                        local_60 += VectorLength(local_2c) * 0.5f;
                    }

                    if (local_60 < 1)
                        local_60 = 1;

                    local_60 = 1 / sqrt(local_60);
                }
                val = backplanes;
                if (face->side == 0) {
                    val = dplanes;
                }
                VectorMA(puVar10->normal, local_60, val[face->planenum].normal, puVar10->normal);
            }
        }
    }

    for (int i = 0; i < numvertexes; i++) {
        puVar10 = g_maybe_vertex_phong[i];
        while (puVar10) {
            VectorNormalize(puVar10->normal, puVar10->normal);
            puVar10 = puVar10->next;
        }
    }
}



void maybeFreePhong()
{
    for (int i = 0; i < numvertexes; i++) {
        unknownunk_t* puVar2 = g_maybe_vertex_phong[i];
        while (puVar2) {
            unknownunk_t *tmp = puVar2->next;
            free(puVar2);
            puVar2 = tmp;
        }
    }
}

void GetFaceBounds(int facenum, vec3_t& mins, vec3_t& maxs)
{
    unsigned short v;

    mins.x = mins.y = mins.z = 99999.f;
    maxs.x = maxs.y = maxs.z = -99999.f;

    for (int i = 0; i < dfaces[facenum].numedges; i++) {
        int edge = dsurfedges[dfaces[facenum].firstedge + i];
        if (edge < 0) {
            v = dedges[-edge].v[1];
        }
        else {
            v = dedges[edge].v[0];
        }
        AddPointToBounds(dvertexes[v].point, mins, maxs);
    }

    unsigned short uVar1 = facegroups[facenum].start;
    while (uVar1 != facenum) {
        for (int i = 0; i < dfaces[uVar1].numedges; i++) {
            int edge = dsurfedges[dfaces[uVar1].firstedge + i];
            if (edge < 0) {
                v = dedges[-edge].v[1];
            }
            else {
                v = dedges[edge].v[0];
            }
            AddPointToBounds(dvertexes[v].point, mins, maxs);
        }
        uVar1 = facegroups[uVar1].start;
    }
}

entity_t* FindEntityTarget(const char *targetname, const char *optional_classname)
{
    entity_t *ent = entities;
    for (int i = 0; i < num_entities; i++, ent++) {
        if (optional_classname && strcmp(ValueForKey(ent, "classname"), optional_classname)) {
            continue;
        }
        if (!strcmp(ValueForKey(ent, "targetname"), targetname)) {
            return ent;
        }
    }
    return nullptr;
}


/*
=============
CreateDirectLights
=============
*/
void CreateDirectLights()
{
    entity_t *ent;
    byte bVar1;
    short sVar2;
    unsigned int uVar3;
    const char *name;
    const char *pcVar4;
    entity_t *ent_00;
    directlight_t *local_EAX_1643;
    dleaf_t *pdVar5;
    const char *targetname;
    const char *pcVar6;
    const char *pcVar7;
    directlight_t *dl;
    int iVar11;
    suninfo_t *psVar12;
    byte *pbVar15;
    const char *pcVar16;
    byte *pbVar17;
    directlight_t *pdVar18;
    bool bVar19;
    float fVar20;
    float fVar21;
    float fVar23;
    int num_surface_lights;
    int num_entity_lights;
    double dStack96;
    float local_50;
    float local_4c;
    float fStack72;
    float fStack68;
    char key_prefix[12];
    char local_28[36];

    num_entity_lights = 0;
    num_surface_lights = 0;
    int j = 0;
    if (0 < num_entities) {
        do {
            ent = entities + j;
            name = ValueForKey(ent, "classname");
            if (!strcmp(name, "worldspawn")) {
                /* loops 9 times */
                int i = 0;
                do {
                    the_9_suns[i].direction;
                    the_9_suns[i].light = 200;
                    VectorClear(the_9_suns[i].color);
                    the_9_suns[i].diffuse = 0;
                    the_9_suns[i].diffade = 1;
                    the_9_suns[i].direction.x = 0;
                    the_9_suns[i].direction.y = 0;
                    the_9_suns[i].direction.z = -1;
                    the_9_suns[i].target = nullptr;
                    the_9_suns[i].style = 0;
                    the_9_suns[i].bool_maybe_sun_is_active = 0;
                    if (i == 0) {
                        sprintf((char *)key_prefix, "_sun");
                    }
                    else {
                        sprintf((char *)key_prefix, "_sun%u", i + 1);
                    }
                    sprintf(local_28, "%s_light", key_prefix);
                    fVar23 = FloatForKey(ent, local_28);
                    if (fVar23 != 0) {
                        the_9_suns[i].light = fVar23;
                        the_9_suns[i].bool_maybe_sun_is_active = 1;
                    }
                    sprintf(local_28, "%s_diffuse", key_prefix);
                    fVar23 = FloatForKey(ent, local_28);
                    if (fVar23 != 0) {
                        if (fVar23 >= the_9_suns[i].light) {
                            the_9_suns[i].diffuse = the_9_suns[i].light;
                        }
                        else {
                            the_9_suns[i].diffuse = fVar23;
                        }
                        the_9_suns[i].bool_maybe_sun_is_active = 1;
                    }
                    sprintf(local_28, "%s_diffade", key_prefix);
                    fVar23 = FloatForKey(ent, local_28);
                    if (fVar23 == 0) {
                        sprintf(local_28, "%s_difwait", key_prefix);
                        fVar23 = FloatForKey(ent, local_28);
                    }
                    if (fVar23 != 0) {
                        the_9_suns[i].diffade = fVar23;
                    }

                    sprintf(local_28, "%s_color", key_prefix);
                    GetVectorForKey(ent, local_28, the_9_suns[i].color);
                    if ((the_9_suns[i].color.x == 0) || (the_9_suns[i].color.y == 0) || (the_9_suns[i].color.z == 0)) {
                        VectorNormalize(the_9_suns[i].color, the_9_suns[i].color);
                        the_9_suns[i].bool_maybe_sun_is_active = 1;
                    }
                    sprintf(local_28, "%s_target", key_prefix);
                    pcVar4 = ValueForKey(ent, local_28);
                    the_9_suns[i].target = pcVar4;
                    if (*pcVar4 == '\0') {
                        sprintf(local_28, "%s", key_prefix);
                        pcVar4 = ValueForKey(ent, local_28);
                        the_9_suns[i].target = pcVar4;
                    }

                    if (*the_9_suns[i].target == '\0') {
                        sprintf(local_28, "%s_angle", key_prefix);
                        pcVar4 = ValueForKey(ent, local_28);
                        if (*pcVar4 != '\0') {
                        LAB_00407f20:
                            sscanf(pcVar4, "%f %f", &local_4c, &local_50);
                            fVar21 = cos(local_50 * Q_PI / 180);
                            the_9_suns[i].direction.x = fVar21 * cos(local_4c * Q_PI / 180);
                            the_9_suns[i].direction.y = fVar21 * sin(local_4c * Q_PI / 180);
                            the_9_suns[i].direction.z = sin(local_50 * Q_PI / 180);
                            VectorNormalize(the_9_suns[i].direction, the_9_suns[i].direction);
                            goto LAB_00407fd9;
                        }
                        sprintf(local_28, "%s_mangle", key_prefix);
                        pcVar4 = ValueForKey(ent, local_28);
                        if (*pcVar4 != '\0')
                            goto LAB_00407f20;
                        sprintf(local_28, "%s_vector", key_prefix);
                        GetVectorForKey(ent, local_28, the_9_suns[i].direction);
                        if ((the_9_suns[i].direction.x == 0) || (the_9_suns[i].direction.y == 0) || (the_9_suns[i].direction.z == 0)) {
                            VectorNormalize(the_9_suns[i].direction, the_9_suns[i].direction);
                            goto LAB_00407fd9;
                        }
                    }
                    else {
                    LAB_00407fd9:
                        the_9_suns[i].bool_maybe_sun_is_active = 1;
                    }

                    sprintf(local_28, "%s_style", key_prefix);
                    targetname = ValueForKey(ent, local_28);
                    if (*targetname) {
                        ent_00 = FindEntityTarget(targetname, "light");
                        if (!ent_00) {
                            iVar11 = atoi(targetname);
                        }
                        else {
                            iVar11 = (int)FloatForKey(ent_00, "style");
                        }
                        the_9_suns[i].style = iVar11;
                        the_9_suns[i].bool_maybe_sun_is_active = 1;
                    }
                    i += 1;
                } while (i < 9);
                g_sky_ambient = 0.00000000;
                pcVar4 = ValueForKey(ent, "_sky_ambient");
                if ((*pcVar4 != '\0') || (pcVar4 = ValueForKey(ent, "_sun_ambient"), *pcVar4 != '\0')) {
                    iVar11 = sscanf(pcVar4, "%f %f %f", &vec3_t_021d98b0.x, &vec3_t_021d98b0.y, &vec3_t_021d98b0.z);
                    if (iVar11 < 3) {
                        g_sky_ambient = vec3_t_021d98b0.x;
                        VectorClear(vec3_t_021d98b0);
                    }
                    else {
                        g_sky_ambient = ColorNormalize(vec3_t_021d98b0, vec3_t_021d98b0);
                    }
                }
                g_sky_surface = 0.00000000;
                pcVar4 = ValueForKey(ent, "_sky_surface");
                if ((*pcVar4 != '\0') || (pcVar4 = ValueForKey(ent, "_sun_surface"), *pcVar4 != '\0')) {
                    iVar11 = sscanf(pcVar4, "%f %f %f", &vec3_t_021d98a0.x, &vec3_t_021d98a0.y, &vec3_t_021d98a0.z);
                    if (iVar11 < 3) {
                        g_sky_surface = vec3_t_021d98a0.x;
                        VectorClear(vec3_t_021d98a0);
                    }
                    else {
                        g_sky_surface = ColorNormalize(vec3_t_021d98a0, vec3_t_021d98a0);
                    }
                }
                if (ambient.x == 0) {
                    if ((pcVar4 = ValueForKey(ent, "_ambient"), *pcVar4 != '\0') ||
                        (pcVar4 = ValueForKey(ent, "light"), *pcVar4 != '\0') ||
                        (pcVar4 = ValueForKey(ent, "_minlight"), *pcVar4 != '\0')) {
                        if (sscanf(pcVar4, "%f %f %f", &ambient.x, &ambient.y, &ambient.z) < 3) {
                            ambient.y = ambient.x;
                            ambient.z = ambient.x;
                        }
                    }
                }
            }
            if (!strcmp(name, "light")) {
                numdlights += 1;
                local_EAX_1643 = (directlight_t *)malloc(sizeof(directlight_t));
                if (!local_EAX_1643) {
                    Error("CreateDirectLights: (entities) malloc failed");
                }
                *local_EAX_1643 = {};
                local_EAX_1643->m_emittype = 1;
                GetVectorForKey(ent, "origin", local_EAX_1643->m_origin);
                pdVar5 = PointInLeaf(local_EAX_1643->m_origin);
                sVar2 = pdVar5->cluster;
                local_EAX_1643->m_next = directlights[(int)sVar2];
                directlights[(int)sVar2] = local_EAX_1643;
                fVar23 = FloatForKey(ent, "light");
                if ((fVar23 == 0) && (fVar23 = FloatForKey(ent, "_light"), (fVar23 == 0))) {
                    fVar23 = 300.00000000;
                }
                local_EAX_1643->m_intensity = entity_scale * fVar23;
                pcVar4 = ValueForKey(ent, "_color");
                if (!pcVar4 || !*pcVar4) {
                    local_EAX_1643->m_color.x = 1;
                    local_EAX_1643->m_color.y = 1;
                    local_EAX_1643->m_color.z = 1;
                }
                else {
                    sscanf(pcVar4, "%f %f %f", &local_EAX_1643->m_color.x, &local_EAX_1643->m_color.y, &local_EAX_1643->m_color.z);
                    ColorNormalize(local_EAX_1643->m_color, local_EAX_1643->m_color);
                }
                local_EAX_1643->m_style = (int)FloatForKey(ent, "_style");
                if (local_EAX_1643->m_style == 0) {
                    local_EAX_1643->m_style = (int)FloatForKey(ent, "style");
                }
                if ((local_EAX_1643->m_style < 0) || (local_EAX_1643->m_style > 255)) {
                    local_EAX_1643->m_style = 0;
                }
                local_EAX_1643->m_cap = FloatForKey(ent, "_cap");
                if (local_EAX_1643->m_cap == 0) {
                    local_EAX_1643->m_cap = FloatForKey(ent, "cap");
                }
                local_EAX_1643->m_bounce = 1;
                targetname = ValueForKey(ent, "_bounce");
                if (((*targetname != 0) ||
                    (targetname = ValueForKey(ent, "bounce"), *targetname != 0)) &&
                    (iVar11 = atoi(targetname), iVar11 == 0)) {
                    local_EAX_1643->m_bounce = 0;
                }
                targetname = ValueForKey(ent, "_falloff");
                if (*targetname == 0) {
                    local_EAX_1643->m_falloff = 0;
                }
                else {
                    iVar11 = atoi(targetname);
                    local_EAX_1643->m_falloff = iVar11;
                }
                if ((local_EAX_1643->m_falloff < 0) || (2 < local_EAX_1643->m_falloff)) {
                    local_EAX_1643->m_falloff = 0;
                }
                local_EAX_1643->m_distance = 1.f;
                if (local_EAX_1643->m_falloff == 0) {
                    pcVar4 = ValueForKey(ent, "_fade");
                    if (((*pcVar4 != '\0') || (pcVar4 = ValueForKey(ent, "wait"), *pcVar4 != '\0')) ||
                        (pcVar4 = ValueForKey(ent, "_wait"), *pcVar4 != '\0')) {
                        local_EAX_1643->m_distance = atof(pcVar4);
                    }
                    if (local_EAX_1643->m_distance < 0.f) {
                        local_EAX_1643->m_distance = 0.f;
                    }
                }
                pcVar4 = ValueForKey(ent, "_distance");
                if (*pcVar4 != '\0') {
                    if (local_EAX_1643->m_falloff == 0) {
                        local_EAX_1643->m_distance = local_EAX_1643->m_intensity / atof(pcVar4);
                    }
                    else {
                        local_EAX_1643->m_distance = atof(pcVar4);
                    }
                }
                pcVar4 = ValueForKey(ent, "_angfade");
                if (*pcVar4 == '\0') {
                    pcVar4 = ValueForKey(ent, "_angwait");
                    if (*pcVar4 == '\0') {
                        local_EAX_1643->m_angwait = 1.f;
                    }
                    else {
                        local_EAX_1643->m_angwait = atof(pcVar4);
                    }
                }
                else {
                    local_EAX_1643->m_angwait = atof(pcVar4);
                }
                if (local_EAX_1643->m_angwait < 0.f) {
                    local_EAX_1643->m_angwait = 0.f;
                }
                pcVar4 = ValueForKey(ent, "_focus");
                if (*pcVar4 == '\0') {
                    local_EAX_1643->m_focus = 1.f;
                }
                else {
                    fVar21 = atof(pcVar4);
                    local_EAX_1643->m_focus = (float)fVar21;
                }
                if (local_EAX_1643->m_focus < 0.f) {
                    local_EAX_1643->m_focus = 0.f;
                }
                targetname = ValueForKey(ent, "target");
                const char* mangle_value = ValueForKey(ent, "_spotangle");
                if (*mangle_value == '\0') {
                    mangle_value = ValueForKey(ent, "_mangle");
                }
                pcVar6 = ValueForKey(ent, "_spotvector");
                pcVar7 = ValueForKey(ent, "_spotpoint");

                bool light_spot = !strcmp(name, "light_spot");
                if (light_spot || *targetname || *pcVar6 || *mangle_value || *pcVar7) {
                    local_EAX_1643->m_emittype = 2;
                    local_EAX_1643->m_cone = FloatForKey(ent, "_cone");
                    if (local_EAX_1643->m_cone == 0.f) {
                        local_EAX_1643->m_cone = 10.f;
                    }
                    local_EAX_1643->m_cone = cos(local_EAX_1643->m_cone * (1 / 180.f) * Q_PI);
                    if (light_spot) {
                        float ang = FloatForKey(ent, "angle");
                        if (ang == -1) {
                            if (ang != -2) {
                                local_EAX_1643->m_normal.z = 0.00000000;
                                fVar21 = ang * (1/180.f) * Q_PI;
                                local_EAX_1643->m_normal.x = cos(fVar21);
                                local_EAX_1643->m_normal.y = sin(fVar21);
                            }
                            else {
                                local_EAX_1643->m_normal.x = 0.f;
                                local_EAX_1643->m_normal.y = 0.f;
                                local_EAX_1643->m_normal.z = -1.f;
                            }
                        }
                        else {
                            local_EAX_1643->m_normal.x = 0.f;
                            local_EAX_1643->m_normal.y = 0.f;
                            local_EAX_1643->m_normal.z = 1.f;
                        }
                    }
                    else {
                        if (*targetname == 0) {
                            if (*mangle_value == '\0') {
                                if (*pcVar6 == '\0') {
                                    if (*pcVar7 == '\0')
                                        goto LAB_0040887d;
                                    vec3_t pt;
                                    GetVectorForKey(ent, "_spotpoint", pt);
                                    VectorSubtract(pt, local_EAX_1643->m_origin, local_EAX_1643->m_normal);
                                }
                                else {
                                    GetVectorForKey(ent, "_spotvector", local_EAX_1643->m_normal);
                                }
                                VectorNormalize(local_EAX_1643->m_normal, local_EAX_1643->m_normal);
                            }
                            else {
                                float yaw = 0.f, pitch = 0.f;
                                sscanf(mangle_value, "%f %f", &yaw, &pitch);
                                fVar21 = cos(pitch * Q_PI / 180);
                                local_EAX_1643->m_normal.x = fVar21 * cos(yaw * Q_PI / 180);
                                local_EAX_1643->m_normal.y = fVar21 * sin(yaw * Q_PI / 180);
                                local_EAX_1643->m_normal.z = sin(pitch * Q_PI / 180);
                                VectorNormalize(local_EAX_1643->m_normal, local_EAX_1643->m_normal);
                            }
                        }
                        else {
                            ent_00 = FindEntityTarget(targetname, nullptr);
                            if (!ent_00) {
                                printf("WARNING: light at (%i %i %i) has missing target\n", (int)local_EAX_1643->m_origin.x, (int)local_EAX_1643->m_origin.y, (int)local_EAX_1643->m_origin.z);
                                local_EAX_1643->m_normal.x = 1;
                                local_EAX_1643->m_normal.y = 0;
                                local_EAX_1643->m_normal.z = 0;
                            }
                            else {
                                vec3_t origin;
                                GetVectorForKey(ent_00, "origin", origin);
                                VectorSubtract(origin, local_EAX_1643->m_origin, local_EAX_1643->m_normal);
                                VectorNormalize(local_EAX_1643->m_normal, local_EAX_1643->m_normal);

                                for (int i = 0; i < 9; i++) {
                                    if (the_9_suns[i].bool_maybe_sun_is_active != 0) {
                                        if (!strcmp(targetname, the_9_suns[i].target)) {
                                            VectorCopy(local_EAX_1643->m_normal, the_9_suns[i].direction);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            LAB_0040887d:
                int i = (int)FloatForKey(ent, "_sun");

                if (i > 0 && i < 9) {

                    i--;

                    if (local_EAX_1643->m_normal.x != 0.f || local_EAX_1643->m_normal.y != 0.f || local_EAX_1643->m_normal.z != 0.f) {
                        VectorCopy(local_EAX_1643->m_normal, the_9_suns[i].direction);
                    }
                    VectorCopy(local_EAX_1643->m_color, the_9_suns[i].color);
                    the_9_suns[i].light = (double)local_EAX_1643->m_intensity;
                    the_9_suns[i].style = local_EAX_1643->m_style;
                    the_9_suns[i].diffuse = FloatForKey(ent, "_diffuse");
                    the_9_suns[i].diffade = FloatForKey(ent, "_diffade");
                    if (the_9_suns[i].diffade == 0) {
                        the_9_suns[i].diffade = (double)local_EAX_1643->m_distance;
                    }
                    the_9_suns[i].bool_maybe_sun_is_active = 1;
                }
                num_entity_lights += 1;
            }
            j += 1;
        } while (j < num_entities);
    }
    j = 0;
    if (num_patches != 0) {
        do {
            bool any_sun = false;
            if ((patches[j].cluster != -1) && (((patches[j].totallight.x < 1 || patches[j].totallight.y < 1) || patches[j].totallight.z < 1))) {
                numdlights += 1;
                dl = (directlight_t *)malloc(sizeof(directlight_t));
                if (!dl) {
                    Error("CreateDirectLights: (surfaces) malloc failed");
                }
                *dl = {};

                dl->m_face = dfaces + patches[j].facenum;
                (dl->m_origin).x = patches[j].origin.x;
                (dl->m_origin).y = patches[j].origin.y;
                (dl->m_origin).z = patches[j].origin.z;
                dl->m_next = directlights[patches[j].cluster];
                directlights[patches[j].cluster] = dl;

                for (int u = 0; u < 9; u++) {
                    any_sun |= the_9_suns[u].bool_maybe_sun_is_active;
                }

                if ( ((texinfo[dl->m_face->texinfo].flags & SURF_SKY) == 0) ||
                    ((!any_sun && (g_sky_ambient == 0)) && (g_sky_surface == 0))) {
                    dl->m_emittype = 0;
                }
                else {
                    dl->m_emittype = 3;
                }
                vec3_t mins, maxs;
                GetFaceBounds(patches[j].facenum, mins, maxs);
                dl->m_choplight = 0.00000000;
                for (iVar11 = 0; iVar11 < 3; iVar11++) {
                    fVar23 = maxs.data[iVar11] - mins.data[iVar11];
                    if (fVar23 > dl->m_choplight) {
                        dl->m_choplight = fVar23;
                    }
                }
                if (dl->m_choplight > choplight) {
                    dl->m_choplight = choplight;
                }
                VectorCopy(patches[j].plane->normal, dl->m_normal);
                dl->m_focus = patches[j].focus;
                dl->m_style = patches[j].styletable;
                dl->m_distance = patches[j].distance;
                dl->m_bounce = 1;
                dl->m_intensity = direct_scale * patches[j].area * ColorNormalize(patches[j].totallight, dl->m_color);
                VectorClear(patches[j].totallight);
                if ((g_sky_surface != 0.f) && !any_sun && (vec3_t_021d98a0.x == 0) && (vec3_t_021d98a0.y == 0) && (vec3_t_021d98a0.z == 0)) {
                    VectorCopy(dl->m_color, vec3_t_021d98a0);
                }
                num_surface_lights++;
            }
            j += 1;
        } while (j < num_patches);
    }
    qprintf("%i direct lights:  %i entity, %i surface\n", numdlights, num_entity_lights, num_surface_lights);
}

/*
=============
GatherSampleLight

Lightscale is the normalizer for multisampling
=============
*/
void GatherSampleLight(const vec3_t& pos, const vec3_t& realpt, const vec3_t& normal, vec3_t *styletable[MAX_LSTYLES],
    vec3_t *bouncelight, int offset, int mapsize, float lightscale)
{
    int				i;
    directlight_t	*l;
    byte			pvs[(MAX_MAP_LEAFS + 7) / 8];
    vec3_t			delta;
    float			dot, dot2;
    float			dist;
    float			scale;
    float			*dest;

    // sun related
    int local_212c[9] = {};
    directlight_t* local_2108[9] = {};
    directlight_t* local_20e4[9] = {};
    int local_20c0[9] = {};
    float local_209c[9] = {};

    vec3_t local_2078[9];
    for (i = 0; i < 9; i++) {
        local_2078[i].x = local_2078[i].y = local_2078[i].z = 1.f;
    }

    vec3_t local_216c;
    local_216c.x = local_216c.y = local_216c.z = 1.f;

    float local_2158 = 0;
    directlight_t* local_215c = nullptr;
    int local_2160;

    // get the PVS for the pos to limit the number of checks
    if (!PvsForOrigin(pos, pvs))
    {
        return;
    }

    int numclusters;
    if (visdatasize == 0) {
        numclusters = 8;
    }
    else {
        numclusters = dvis->numclusters;
    }

    for (i = 0; i < numclusters; i++)
    {
        if (!(pvs[i >> 3] & (1 << (i & 7))))
            continue;


        for (l = directlights[i]; l; l = l->m_next)
        {
        /*  VectorSubtract(l->origin, pos, delta);
            dist = VectorNormalize(delta, delta);
            dot = DotProduct(delta, normal);
            if (dot <= 0.001)
                continue;	// behind sample surface
            */
            VectorSubtract(l->m_origin, realpt, delta);
            if (l->m_emittype == 0 || l->m_emittype == 3)
                VectorSubtract(delta, l->m_normal, delta);
            dist = VectorNormalize(delta, delta);
            dot = DotProduct(delta, normal);
            if (dot <= 0.001)
                continue; // behind sample surface

            bool bVar6 = false;

            float local_21a8; // TODO RENAME scale
            
            // local_21b0 => dist


            switch (l->m_emittype)
            {
                case emit_point: // case 1
                    {
                        // linear falloff
                        // original:
                        //scale = (l->intensity - dist) * dot;

                        if (l->m_falloff == 0) {
                            if (l->m_distance != 1) {
                                dist *= l->m_distance;
                            }
                            if (dist > abs(l->m_intensity)) {
                                continue; // next l  - goto skipadd instead?
                            }
                        }
                        else if (l->m_distance != 1) {
                            if (dist > l->m_distance) {
                                continue; // next l
                            }
                        }

                        if (l->m_angwait != 1) {
                            dot = (1.f - l->m_angwait) + dot * l->m_angwait; // dot=>local_res0
                        }

                        if (l->m_falloff == 0) {
                            if (l->m_intensity >= 0) {
                                local_21a8 = (l->m_intensity - dist) * dot;
                            }
                            else {
                                local_21a8 = (dist + l->m_intensity) * dot;
                            }
                        }
                        else {
                            if (l->m_falloff == 1) {
                                local_21a8 = l->m_intensity / dist * dot;
                            }
                            else {
                                local_21a8 = l->m_intensity / (dist * dist) * dot;
                            }
                        }
                    }
                    break;

                case emit_surface: // case 0
                    /* original:
                    dot2 = -DotProduct(delta, l->normal);
                    if (dot2 <= 0.001)
                        goto skipadd;	// behind light surface
                    scale = (l->intensity / (dist*dist)) * dot * dot2;
                    */

                    if ((l->m_distance == 1) || (dist <= l->m_distance)) {
                        float local_2198_new = 0;
                        dot2 = -DotProduct(delta, l->m_normal);
                        if (texinfo[l->m_face->texinfo].flags & (SURF_SKY|SURF_WARP)) {
                            local_2198_new = 0.1f;
                        }

                        if (dot2 < 0) {
                            if (dist >= l->m_choplight) {
                                continue;
                            }
                            delta.x = (l->m_origin.x - pos.x) - (l->m_normal).x;
                            delta.y = (l->m_origin.y - pos.y) - (l->m_normal).y;
                            delta.z = (l->m_origin.z - pos.z) - (l->m_normal).z;
                            dist = VectorNormalize(delta, delta);
                            dot2 = -DotProduct(delta, l->m_normal);
                            if (dot2 < 0) {
                                continue;
                            }

                            if (texinfo[l->m_face->texinfo].flags & (SURF_SKY|SURF_WARP)) {
                                local_2198_new = 0.2f;
                            }
                        }

                        if (dot2 < local_2198_new) {
                            dot2 = local_2198_new;
                        }

                        float fVar1;
                        if (splotchfix == 0) {
                            fVar1 = 0;
                        }
                        else {
                            fVar1 = l->m_choplight;
                        }

                        if (fVar1 > dist) {
                            float fVar4 = 1 - DotProduct(normal, l->m_normal);
                            if (fVar4 > 1) {
                                fVar4 = 1;
                            }
                            else {
                                fVar4 = sqrt(fVar4);
                            }

                            fVar4 = (fVar1 - dist) / fVar1 * fVar4;
                            dot = dot / fVar1 * dist + fVar4;
                            dot2 = (2 * fVar4) + dot2 * dist / fVar1;

                            dist = fVar1;
                        }

                        if (l->m_focus != 1) {
                            dot2 = dot2 * l->m_focus + (1 - l->m_focus);
                        }

                        local_21a8 = (l->m_intensity / (dist * dist)) * dot2 * dot;
                    }
                    break;
                case emit_spotlight: // case 2
                    {
                        // linear falloff
                        /*original:
                        dot2 = -DotProduct(delta, l->normal);
                        if (dot2 <= l->stopdot)
                            goto skipadd;	// outside light cone
                        scale = (l->intensity - dist) * dot;
                        */
                        if (l->m_falloff == 0) {
                            if (l->m_distance != 1) {
                                dist *= l->m_distance;
                            }
                            if (dist < abs(l->m_intensity)) {
                                continue;
                            }
                        }
                        else {
                            if (l->m_distance != 1) {
                                if (dist > l->m_distance) {
                                    continue;
                                }
                            }
                        }

                        float fVar3 = -DotProduct(delta, l->m_normal);
                        if (fVar3 <= l->m_cone) {
                            continue;
                        }
                        if (l->m_focus != 1) {
                            fVar3 = fVar3 * l->m_focus + (1 - l->m_focus);
                            if (fVar3 < 0) {
                                continue;
                            }
                        }
                        if (l->m_angwait != 1) {
                            dot = (1 - l->m_angwait) + l->m_angwait * dot;
                        }
                        if (l->m_falloff == 0) {
                            if (l->m_intensity >= 0) {
                                local_21a8 = (l->m_intensity - dist) * fVar3 * dot;
                            }
                            else {
                                local_21a8 = (dist + l->m_intensity) * fVar3 * dot;
                            }
                        }
                        else if (l->m_falloff == 1) {
                            local_21a8 = (l->m_intensity / dist) * fVar3 * dot;
                        }
                        else {
                            local_21a8 = (l->m_intensity / (dist * dist)) * fVar3 * dot;
                        }
                    }
                    break;
                case emit_UNKNOWN: // case 3
                    {
                        float fVar3 = -DotProduct(delta, l->m_normal);
                        if (fVar3 < 0) {
                            if (dist >= l->m_choplight) {
                                continue;
                            }
                            delta.x = (l->m_origin.x - pos.x) - l->m_normal.x;
                            delta.y = (l->m_origin.y - pos.y) - l->m_normal.y;
                            delta.z = (l->m_origin.z - pos.z) - l->m_normal.z;
                            dist = VectorNormalize(delta, delta);
                            fVar3 = -DotProduct(delta, l->m_normal);
                            if (fVar3 < -0.001f) {
                                continue;
                            }
                        }

                        float local_2198 = 0.2f;
                        if (fVar3 < local_2198) {
                            fVar3 = local_2198;
                        }

                        if (g_sky_surface != 0) {
                            if (dist < l->m_choplight) {
                                float fVar4 = 1 - DotProduct(normal, l->m_normal);
                                if (fVar4 > 1) {
                                    fVar4 = 1;
                                }
                                else {
                                    fVar4 = sqrt(fVar4);
                                }

                                fVar4 = (l->m_choplight - dist) / l->m_choplight * fVar4;
                                dot = dot / l->m_choplight * dist + fVar4;
                                fVar3 = 2 * fVar4 + fVar3 * dist / l->m_choplight;

                                dist = l->m_choplight;
                            }
                            if (g_sky_surface <= 1) {
                                local_21a8 = l->m_intensity / (dist * dist) * fVar3 * dot;
                            }
                            else {
                                local_21a8 = (((g_sky_surface * l->m_intensity) /
                                    (float)texinfo[l->m_face->texinfo].value) /
                                    (dist * dist)) * fVar3 * dot;
                            }
                        }
                        else {
                            local_21a8 = 0;
                        }

                        for (i = 0; i < 9; i++) {
                            suninfo_t* sun = &the_9_suns[i];
                            if (!sun->bool_maybe_sun_is_active) {
                                continue;
                            }
                            if (local_212c[i]) {
                                continue;
                            }
                            float fVar3 = -DotProduct(sun->direction, normal);
                            if (fVar3 < 0.001) {
                                continue;
                            }
                            vec3_t fStack8504;
                            VectorCopy(sun->direction, fStack8504);
                            VectorNegate(fStack8504);
                            VectorSubtract(l->m_origin, pos, delta);

                            float fVar1 = DotProduct(delta, l->m_normal) /
                                DotProduct(fStack8504, l->m_normal);

                            vec3_t vStack8528;
                            vStack8528.x = fStack8504.x * fVar1 + pos.x;
                            vStack8528.y = fStack8504.y * fVar1 + pos.y;
                            vStack8528.z = fStack8504.z * fVar1 + pos.z;
                            vec3_t vStack8516;
                            VectorSubtract(l->m_origin, vStack8528, vStack8516);

                            float length = VectorNormalize(vStack8516, vStack8516);
                            if ((length * 0.5f <= chopsky) &&
                                !TestLine_shadow(vStack8528, pos, nullptr, &local_2078[i]) &&
                                !TestLine_r(0, l->m_origin, vStack8528, nullptr))
                            {
                                local_2108[i] = l;
                                local_209c[i] = fVar3 * sun->light;
                                if (local_2078[i].x == 1 && local_2078[i].y == 1 && local_2078[i].z == 1) {
                                    local_212c[i] = 1;
                                }
                            }

                            if (sun->diffuse == 0) {
                                continue;
                            }
                            if (local_212c[i]) {
                                continue;
                            }

                            if (!bVar6) {
                                local_2160 = TestLine_shadow(l->m_origin, pos, nullptr, &local_216c);
                                bVar6 = true;
                            }

                            if (local_2160) {
                                continue;
                            }
                            if (local_216c.z != 1 || local_216c.y != 1 || local_216c.z != 1) {
                                continue;
                            }

                            if (sun->diffade == 0 || sun->diffade != 1.875) {
                                length *= sun->diffade;
                            }
                            if (length >= 128.f) {
                                continue;
                            }

                            fVar3 = (128.f - length) / 128.f * fVar3 * sun->diffuse;
                            if (fVar3 > local_20c0[i]) {
                                local_20c0[i] = fVar3;
                                local_20e4[i] = l;
                            }
                        }
                        if (g_sky_ambient == 0) {
                            break;
                        }
                        if (local_215c) {
                            break;
                        }
                        if (!bVar6) {
                            local_2160 = TestLine_shadow(l->m_origin, pos, nullptr, &local_216c);
                            bVar6 = true;
                        }
                        if (local_2160 == 0) {
                            local_2158 = g_sky_ambient;
                            local_215c = l;
                        }

                    }
                    break;
                default:
                    Error("Bad l->type");
            }

            if (l->m_style != 0) {
                if (stylemin > abs(local_21a8)) {
                    continue;
                }
            }
            else if (local_21a8 == 0) {
                continue;
            }

            // TODO rename bVar6 -> did line test

            if (!bVar6 && TestLine_shadow(l->m_origin, pos, nullptr, &local_216c))
                continue;	// occluded

            // TODO rename local_21a8 => scale
            if (l->m_cap != 0 && local_21a8 > l->m_cap) {
                local_21a8 = l->m_cap;
            }

            vec3_t* ofs = styletable[l->m_style] + offset;

            vec3_t local_2184;
            if (l->m_emittype == emit_UNKNOWN) {
                if (vec3_t_021d98a0.x || vec3_t_021d98a0.y || vec3_t_021d98a0.z) {
                    VectorCopy(vec3_t_021d98a0, local_2184);
                }
                else {
                    VectorClear(local_2184);

                    for (i = 0; i < 9; i++) {
                        suninfo_t* sun = &the_9_suns[i];
                        if ((sun->bool_maybe_sun_is_active != 0) && (sun->style == 0)) {
                        
                            vec3_t& color = (sun->color.x || sun->color.y || sun->color.z == 0) ? sun->color : l->m_color;
                            VectorMA(local_2184, sun->light, color, local_2184);
                        }
                    }
                    ColorNormalize(local_2184, local_2184);
                }

                if (local_216c.x != 1 || local_216c.y != 1 || local_216c.z != 1) {
                    local_2184.x *= local_216c.x;
                    local_2184.y *= local_216c.y;
                    local_2184.z *= local_216c.z;
                }
                VectorMA(*ofs, (double)(local_21a8 * lightscale), local_2184, *ofs);
            }
            else {
                if (local_216c.x != 1 || local_216c.y != 1 || local_216c.z != 1) {
                    local_2184.x = local_216c.x * l->m_color.x;
                    local_2184.y = local_216c.y * l->m_color.y;
                    local_2184.z = local_216c.z * l->m_color.z;
                }
                else {
                    VectorCopy(l->m_color, local_2184);
                }
                VectorMA(*ofs, (double)(local_21a8 * lightscale), local_2184, *ofs);
                if ((l->m_bounce == 0) || (l->m_style != 0)) 
                    continue;
            }

            // add some light to it
            VectorMA(bouncelight[offset], local_21a8 * lightscale, local_2184, bouncelight[offset]);

        skipadd:;
        }
    }


    for (i = 0; i < 9; i++) {
        suninfo_t* sun = &the_9_suns[i];
        if (!sun->bool_maybe_sun_is_active) {
            continue;
        }
        if (local_2108[i]) {
            if (styletable[sun->style] == 0) {
                styletable[sun->style] = (vec3_t*)malloc(mapsize);
                if (!styletable[sun->style]) {
                    Error("GatherSampleLight: (sunlight) malloc failed");
                }
                memset(styletable[sun->style], 0, mapsize);
            }

            vec3_t local_2184;
            if (sun->color.x == 0 && sun->color.y == 0 && sun->color.z == 0) {
                VectorCopy(local_2108[i]->m_color, local_2184);
            }
            else {
                VectorCopy(sun->color, local_2184);
            }
            if (local_2078[i].x != 1 || local_2078[i].y != 1 || local_2078[i].z != 1) {
                local_2184.x *= local_2078[i].x;
                local_2184.y *= local_2078[i].y;
                local_2184.z *= local_2078[i].z;
            }
            float fVar3 = lightscale * local_209c[i];
            VectorMA(styletable[sun->style][offset], fVar3, local_2184, styletable[sun->style][offset]);
            if (sun->style == 0) {
                VectorMA(bouncelight[offset], fVar3, local_2184, bouncelight[offset]);
            }
        }

        if (local_20e4[i] != 0 && local_212c[i] == 0) {
            if (!styletable[sun->style]) {
                styletable[sun->style] = (vec3_t*)malloc(mapsize);
                if (!styletable[sun->style]) {
                    Error("GatherSampleLight: (sunlight) malloc failed");
                }
                memset(styletable[sun->style], 0, mapsize);
            }
            vec3_t local_2190;
            if (sun->color.x == 0 && sun->color.y == 0 && sun->color.z == 0) {
                VectorCopy(local_20e4[i]->m_color, local_2190);
            }
            else {
                VectorCopy(sun->color, local_2190);
            }
            if (local_2108[i] != 0 &&
                (local_2078[i].x != 1 || local_2078[i].y != 1 || local_2078[i].z != 1))
            {
                local_2190.x *= (1 - local_2078[i].x);
                local_2190.y *= (1 - local_2078[i].y);
                local_2190.z *= (1 - local_2078[i].z);
            }
            float fVar3 = lightscale * local_20c0[i];
            VectorMA(styletable[sun->style][offset], fVar3, local_2190, styletable[sun->style][offset]);
            if (sun->style == 0) {
                VectorMA(bouncelight[offset], fVar3, local_2190, bouncelight[offset]);
            }
        }
    }


    if (!styletable[0]) {
        styletable[0] = (vec3_t*)malloc(mapsize);
        if (!styletable[0]) {
            Error("GatherSampleLight: (sunlight) malloc failed");
        }
        memset(styletable[0], 0, mapsize);
    }



    if (local_215c != nullptr) {
        vec3_t* pstyle = styletable[offset];
        vec3_t* val = nullptr;
        if (vec3_t_021d98b0.x != 0 || vec3_t_021d98b0.y != 0 || vec3_t_021d98b0.z != 0) {
            VectorMA(*pstyle, (local_2158 * lightscale), vec3_t_021d98b0, *pstyle);
            val = &vec3_t_021d98b0;
        }
        else {
            vec3_t local_2190;
            VectorClear(local_2190);
            for (i = 0; i < 9; i++) {
                suninfo_t* sun = &the_9_suns[i];
                if ((sun->bool_maybe_sun_is_active != 0) && (sun->style == 0)) {
                    val = &sun->color;
                    if (sun->color.x == 0 && sun->color.y == 0 && sun->color.z == 0) {
                        val = &local_215c->m_color;
                    }
                    VectorMA(local_2190, sun->light, *val, local_2190);
                }
            }
            if (local_2190.x == 0 || local_2190.y == 0 || local_2190.z == 0) {
                ColorNormalize(local_2190, local_2190);
                VectorMA(*pstyle, (double)(local_2158 * lightscale), local_2190, *pstyle);
                val = &local_2190;
            }
            else {
                val = &local_215c->m_color;
                VectorMA(*pstyle, (double)(local_2158 * lightscale), *val, *pstyle);
            }
        }
        VectorMA(bouncelight[offset], (local_2158 * lightscale), *val, bouncelight[offset]);
    }
}


void free_shadowfaces(shadowfaces_unk_t *p)
{
    for (shadowfaces_unk_t *tmp; p; p = tmp) {
        tmp = p->next;
        free(p);
    }
}

void maybe_free_shadows()
{
    for (shadowmodel_t* p = g_shadow_world, *tmp; p; p = tmp) {
        free_shadowfaces(p->shadownext);
        tmp = p->next;
        free(p);
    }
}


void RadWorld()
{
    if ((numnodes == 0) || (numfaces == 0)) {
        Error("Empty map");
    }
    MakeBackplanes();
    MakeParents(0, -1);
    MakeTnodes(dmodels);
    if (nocurve == 0) {
        maybeInitPhong();
    }
    BuildFaceGroups();

    // turn each face into a single patch
    MakePatches();

    // subdivide patches to a maximum dimension
    SubdividePatches();

    // create directlights out of patches and lights
    CreateDirectLights();

    // build initial facelights
    RunThreadsOn(numfaces, true, BuildFacelights);
    FreeDirectLights();

    maybeFreePhong();

    if (numbounce > 0) {
        // build transfer lists
        RunThreadsOn(num_patches, true, MakeTransfers);
        qprintf("transfer lists: %5.1f megs\n");
        FreeTnodes();

        // spread light around
        BounceLight();
        FreeTransfers();
        //CheckPatches();
    }
    if (glview != 0) {
        NOT_IMPLEMENTED("WriteGlView"); // WriteGlView();
    }
    FreeProjTextures();
    maybe_free_shadows();
    lightdatasize = 0;
    RunThreadsOn(numfaces, true, FinalLightFace);
    free(facegroups);
}

void UpdateLightmaps(int) {
    NOT_IMPLEMENTED(__FUNCTION__);
}



int main(int argc, char **argv)
{
    const char* usage = "----- Usage -----\n"
        "QRAD.EXE  [-?|-help] [-v] [-dump] [-glview] [-nopvs] [-threads #] [-update]   \n"
        "[-game $] [-gamedir $] [-moddir $] [-bounce #] [-onlybounce] [-lightwarp]     \n"
        "[-extra] [-chop #] [-chopcurve #] [-choplight #] [-chopsky #] [-chopwarp #]   \n"
        "[-direct #] [-entity #] [-gamma #] [-scale #] [-radmin #] [-stylemin #]       \n"
        "[-ambienta|-ambient #] [-maxlighta|-maxlight #] [-minlighta|-minlight #]      \n"
        "[-saturation #] [-nocolor|-nocoloru] [-nobmodlight] [-nobrightsurf] [-nocurve]\n"
        "[-noshadowface] [-noshadowfilter] [-noweightcurve] [-noinvisfix] [-nonudgefix]\n"
        "[-nobouncefix] [-noradorigin] [-nosplotchfix] [-nostopbleed]  BSPFILE.BSP     \n";

    printf("----- ArghRad 3.00T9 by Tim Wright (Argh!) -----\n");
    printf("----- TESTING VERSION, DO NOT REDISTRIBUTE -----\n");
    printf("Modified from original source code by id Software\n\n");
    printf("----- Settings -----\n");

    int i = 1;
    if (argc < 2) {
    LAB_ARGS_FINISHED:
        ThreadSetDefault();

        if (maxlight > 255)
            maxlight = 255;

        if (subdiv > 256)
            subdiv = 256;

        if (choplight == 0)
            choplight = subdiv;

        if (chopsky == 0)
            chopsky = subdiv;

        if (chopwarp == 0)
            chopwarp = subdiv;

        if (chopcurve == 0) {
            chopcurve = std::max(32.f, subdiv);
        }


        if (i != argc - 1) {
            Error(usage);
            return 1;
        }

        double start = I_FloatTime();

        if (gamedir == '\0') {
            SetQdirFromPath_v2(argv[i]);
        }
        if (game == 0) {
            game = 1;
        }
        strcpy(source, SetQdirFromPath_v3(argv[i]));
        StripExtension(source);
        DefaultExtension(source, ".bsp");
        printf("----- Load BSP File -----\n");

        char bsp_filename[1036];
        sprintf(bsp_filename, "%s%s", szTempIn, &source);
        printf("reading %s\n", bsp_filename);
        LoadBSPFile(bsp_filename);
        ParseEntities();
        if (!onlyupdate) {
            MakeShadowModels();
            if (g_texscale <= 0) {
                g_texscale = (game == 1 ? 2.0f : 1.0f);
            }
            CalcTextureReflectivityMain();
            if (visdatasize == 0) {
                printf("no vis information, direct lighting only\n");
                numbounce = 0;
            }
            printf("----- Light World -----\n");
            RadWorld();
        }
        else {
            printf("----- Update Lightmaps -----\n");
            if (lightdatasize == 0) {
                Error("-update used on unlit map");
                return 1;
            }
            RunThreadsOn(numfaces, true, UpdateLightmaps);
        }
        printf("----- Save BSP File -----\n");
        sprintf(bsp_filename, "%s%s", szTempOut, &source);
        printf("writing %s\n", bsp_filename);
        WriteBSPFile(bsp_filename);
        printf("----- Time -----\n");
        double elapsed = I_FloatTime() - start;
        printf("%5.0f seconds elapsed\n", (float)elapsed);
        return 0;
    }

LAB_PROCESS_NEXT_ARG:
    if (!strcmp(argv[i], "-?") ||
        !strcmp(argv[i], "-help")) {
        printf(usage);
        return 0;
    }

    if (!strcmp(argv[i], "-argh")) {
        printf("\n");
        printf("                 dHH                   dHP      dH\n");
        printf("                dHHH                  dH        HH\n");
        printf("               dP HH   dHdHP  dHHHHH  HH dHHh   HH\n");
        printf("           dHHHHHHHH  dHH    dH  dHH  HHdP HH   \"H\n");
        printf("           P dP   HH  PHH    HH dPHH  HHP  HHdP  P\n");
        printf("            dP    HH   HHP   \"HHP HP  HH   HHP\n");
        printf("          dHP     \"HHh HP   n    dP   HP   HP   dP\n");
        printf("                            \"HHHHP\n");
        printf("\n");
        return 0;
    }

    if (!strcmp(argv[i], "-dump")) {
        dumppatches = 1;
        printf(" -dump enabled  (patch info dump)\n");
        goto LAB_CONTINUE;
    }
    if (!strcmp(argv[i], "-bounce")) {
        numbounce = atoi(argv[i + 1]);
        printf(" -bounce set to %d  (radiosity bounces)\n", numbounce);
        i += 1;
        goto LAB_CONTINUE;
    }
    if (!strcmp(argv[i], "-v") ||
        !strcmp(argv[i], "-verbose")) {
        verbose = true;
        printf(" -verbose enabled  (more detailed output)\n");
        goto LAB_CONTINUE;
    }
    if (!strcmp(argv[i], "-extra")) {
        extrasamples = 1;
        printf(" -extra enabled  (extra quality light sampling)\n");
        goto LAB_CONTINUE;
    }
    if (!strcmp(argv[i], "-update")) {
        onlyupdate = 1;
        printf(" -update enabled  (only update existing lightmaps)\n");
        goto LAB_CONTINUE;
    }
    if (!strcmp(argv[i], "-threads")) {
        numthreads = atoi(argv[i + 1]);
        printf(" -threads set to %d  (multiple CPU processes)\n", numthreads);
        i += 1;
        goto LAB_CONTINUE;
    }
    if (!strcmp(argv[i], "-chop")) {
        subdiv = atof(argv[i + 1]);
        printf(" -chop set to %.1f  (surface patch size)\n", subdiv);
        i += 1;
        goto LAB_CONTINUE;
    }
    if (!strcmp(argv[i], "-chopsky")) {
        chopsky = atof(argv[i + 1]);
        printf(" -chopsky set to %.1f  (sky patch size)\n", chopsky);
        i += 1;
        goto LAB_CONTINUE;
    }
    if (!strcmp(argv[i], "-chopwarp")) {
        chopwarp = atof(argv[i + 1]);
        printf(" -chopwarp set to %.1f  (warping patch size)\n", chopwarp);
        i += 1;
        goto LAB_CONTINUE;
    }
    if (!strcmp(argv[i], "-choplight")) {
        choplight = atof(argv[i + 1]);
        printf(" -choplight set to %.1f  (normal light patch size)\n", choplight);
        i += 1;
        goto LAB_CONTINUE;
    }
    if (!strcmp(argv[i], "-chopcurve")) {
        chopcurve = atof(argv[i + 1]);
        printf(" -chopcurve set to %.1f  (curve surface patch size)\n", chopcurve);
        i += 1;
        goto LAB_CONTINUE;
    }
    if (!strcmp(argv[i], "-scale")) {
        lightscale = atof(argv[i + 1]);
        printf(" -scale set to %.4f  (lighting brightness scale)\n", lightscale);
        i += 1;
        goto LAB_CONTINUE;
    }
    if (!strcmp(argv[i], "-direct")) {
        direct_scale *= atof(argv[i + 1]);
        printf(" -direct set to %.4f  (surface lighting brightness scale)\n", direct_scale);
        i += 1;
        goto LAB_CONTINUE;
    }
    if (!strcmp(argv[i], "-entity")) {
        entity_scale *= atof(argv[i + 1]);
        printf(" -entity set to %.4f  (entity lighting brightness scale)\n", entity_scale);
        i += 1;
        goto LAB_CONTINUE;
    }
    if (!strcmp(argv[i], "-texscale")) {
        g_texscale = atof(argv[i + 1]);
        printf(" -texscale set to %.4f  (texture brightness scale)\n", g_texscale);
        i += 1;
        goto LAB_CONTINUE;
    }
    if (!strcmp(argv[i], "-oldtexscale")) {
        g_texscale = -1.0f;
        printf(" -oldtexscale enabled  (old texture brightening method)\n");
        i += 1;
        goto LAB_CONTINUE;
    }
    if (!strcmp(argv[i], "-glview")) {
        glview = 1;
        printf(" -glview enabled  (?)\n");
        goto LAB_CONTINUE;
    }
    if (!strcmp(argv[i], "-nopvs")) {
        nopvs = 1;
        printf(" -nopvs enabled  (stop pvs checking)\n");
        goto LAB_CONTINUE;
    }
    if (!strcmp(argv[i], "-nocolor")) {
        _nocolor_maybe_unweighted = 0;
        if (saturation <= 1) {
            saturation = 0;
        }
        printf(" -nocolor enabled  (lighting converted to greyscale, RGB weighted)\n");
        goto LAB_CONTINUE;
    }
    if (!strcmp(argv[i], "-nocoloru")) {
        _nocolor_maybe_unweighted = 1;
        if (saturation <= 1) {
            saturation = 0;
        }
        printf(" -nocoloru enabled  (lighting converted to greyscale, unweighted)\n");
        goto LAB_CONTINUE;
    }
    if (!strcmp(argv[i], "-saturation")) {
        saturation = atof(argv[i + 1]);
        if (saturation < 0) {
            saturation = 0;
        }
        else if (saturation > 1) {
            saturation = 1;
        }
        printf(" -saturation set to %.3f  (colored light saturation)\n", saturation);
        i += 1;
        goto LAB_CONTINUE;
    }
    if (!strcmp(argv[i], "-ambient") || !strcmp(argv[i], "-ambienta")) {
        ambient.x = atof(argv[i + 1]);
        if (!strcmp(argv[i], "-ambient")) {
            ambient.x *= 128.f;
        }
        ambient.y = ambient.x;
        ambient.z = ambient.x;
        printf(" -ambienta set to %.1f  (global ambient brightness)\n", ambient.x);
        i += 1;
        goto LAB_CONTINUE;
    }
    if (!strcmp(argv[i], "-minlight")) {
        minlight = atof(argv[i + 1]) * 128.f;
        printf(" -minlighta set to %.1f  (minimum lighting brightness)\n", minlight);
        i += 1;
        goto LAB_CONTINUE;
    }
    if (!strcmp(argv[i], "-minlighta")) {
        minlight = atof(argv[i + 1]);
        printf(" -minlighta set to %.1f  (minimum lighting brightness)\n", minlight);
        i += 1;
        goto LAB_CONTINUE;
    }
    if (!strcmp(argv[i], "-maxlight")) {
        maxlight = atof(argv[i + 1]) * 128.f;
        printf(" -maxlighta set to %.1f  (maximum lighting brightness)\n", maxlight);
        i += 1;
        goto LAB_CONTINUE;
    }
    if (!strcmp(argv[i], "-maxlighta")) {
        maxlight = atof(argv[i + 1]);
        printf(" -maxlighta set to %.1f  (maximum lighting brightness)\n", maxlight);
        i += 1;
        goto LAB_CONTINUE;
    }
    if (!strcmp(argv[i], "-gamma")) {
        gamma = atof(argv[i + 1]);
        if (gamma <= 0) {
            gamma = 1.f;
        }
        printf(" -gamma set to %.3f  (gamma compensation)\n", gamma);
        i += 1;
        goto LAB_CONTINUE;
    }
    if (!strcmp(argv[i], "-stylemin")) {
        stylemin = atof(argv[i + 1]);
        printf(" -stylemin set to %.1f  (min brightness for special lightstyles)\n", stylemin);
        i += 1;
        goto LAB_CONTINUE;
    }
    if (!strcmp(argv[i], "-radmin")) {
        patch_cutoff = atof(argv[i + 1]);
        printf(" -radmin set to %.4f  (radiosity minimum cutoff)\n", patch_cutoff);
        i += 1;
        goto LAB_CONTINUE;
    }

    if (!strcmp(argv[i], "-nobmodlight")) {
        bmodlight = 0;
        printf(" -nobmodlight enabled  (brush model lighting disabled)\n");
        goto LAB_CONTINUE;
    }
    if (!strcmp(argv[i], "-nobouncefix")) {
        bouncefix = 0;
        printf(" -nobouncefix enabled  (brush model bounced light fix disabled)\n");
        goto LAB_CONTINUE;
    }
    if (!strcmp(argv[i], "-nobrightsurf")) {
        brightsurf = 0;
        printf(" -nobrightsurf enabled  (surface light face brightening disabled)\n");
        goto LAB_CONTINUE;
    }
    if (!strcmp(argv[i], "-nostopbleed")) {
        stopbleed = 0;
        printf(" -nostopbleed enabled  (bleeding light correction disabled)\n");
        goto LAB_CONTINUE;
    }
    if (!strcmp(argv[i], "-nocurve")) {
        nocurve = 1;
        printf(" -nocurve enabled  (Phong shading disabled)\n");
        goto LAB_CONTINUE;
    }
    if (!strcmp(argv[i], "-nosplotchfix")) {
        splotchfix = 0;
        printf(" -nosplotchfix enabled  (anti-splotch fix disabled for plain face lights)\n");
        goto LAB_CONTINUE;
    }
    if (!strcmp(argv[i], "-noradorigin")) {
        radorigin = 0;
        printf(" -noradorigin enabled  (bmodels with origin brushes not lit by radiosity)\n");
        goto LAB_CONTINUE;
    }
    if (!strcmp(argv[i], "-noinvisfix")) {
        invisfix = 0;
        printf(" -noinvisfix enabled  (light bounces off nodraw faces)\n");
        goto LAB_CONTINUE;
    }
    if (!strcmp(argv[i], "-nonudgefix")) {
        nudgefix = 0;
        printf(" -nonudgefix enabled  (uses old method for nudging edge samples)\n");
        goto LAB_CONTINUE;
    }
    if (!strcmp(argv[i], "-lightwarp")) {
        lightwarp = 1;
        printf(" -lightwarp enabled  (calculate lighting on warp surfaces)\n");
        goto LAB_CONTINUE;
    }
    if (!strcmp(argv[i], "-noweightcurve")) {
        weightcurve = 0;
        printf(" -noweightcurve enabled  (Phong calculations ignore face size)\n");
        goto LAB_CONTINUE;
    }
    if (!strcmp(argv[i], "-noshadowfilter")) {
        shadowfilter = 0;
        printf(" -noshadowfilter enabled  (projected shadows not filtered)\n");
        goto LAB_CONTINUE;
    }

    if (!strcmp(argv[i], "-noshadowface")) {
        printf(" -noshadowface enabled  (shadowfaces disabled)\n");
        iTransFaces = -1;
        iNonTransFaces = -1;
        goto LAB_CONTINUE;
    }

    if (!strcmp(argv[i], "-game")) {
        int iVar5 = Q_strcasecmp(argv[i + 1], "heretic2");
        if (iVar5 != 0) {
            iVar5 = Q_strcasecmp(argv[i + 1], "h2");
            if (iVar5 != 0) {
                iVar5 = Q_strcasecmp(argv[i + 1], "kingpin");
                if (iVar5 != 0) {
                    iVar5 = Q_strcasecmp(argv[i + 1], "kp");
                    if (iVar5 != 0) {
                        game = 1;
                        printf(" -game set to Quake2\n");
                        i += 1;
                        goto LAB_CONTINUE;
                    }
                }
                game = 3;
                printf(" -game set to KingPin\n");
                i += 1;
                goto LAB_CONTINUE;
            }
        }
        game = 2;
        printf(" -game set to Heretic2\n");
        i += 1;
        goto LAB_CONTINUE;
    }
    if (!strcmp(argv[i], "-gamedir")) {
        sprintf(gamedir, "%s", argv[i + 1]);

        if (strlen(gamedir) > 0) {
            if (gamedir[strlen(gamedir) - 1] != '\\') {
                strcat(gamedir, "\\");
            }
        }
        printf(" -gamedir set to %s  (game pak dir)\n", gamedir);
        i += 1;
        goto LAB_CONTINUE;
    }
    if (!strcmp(argv[i], "-moddir")) {
        sprintf(moddir, "%s", argv[i + 1]);

        if (strlen(moddir) > 0) {
            if (moddir[strlen(moddir) - 1] != '\\') {
                strcat(moddir, "\\");
            }
        }
        printf(" -moddir set to %s  (mod pak dir)\n", moddir);
        i += 1;
        goto LAB_CONTINUE;
    }

    if (!strcmp(argv[i], "-onlybounce")) {
        onlybounce = 1;
        printf(" -onlybounce enabled  (only bounced light saved)\n");
        goto LAB_CONTINUE;
    }
    if (!strcmp(argv[i], "-tmpin")) {
        strcpy(szTempIn, "/tmp");
        goto LAB_CONTINUE;
    }

    if (!strcmp(argv[i], "-tmpout")) {
        strcpy(szTempOut, "/tmp");
        goto LAB_CONTINUE;
    }
    goto LAB_ARGS_FINISHED;

LAB_CONTINUE:
    i += 1;
    if (argc <= i) goto LAB_ARGS_FINISHED;
    goto LAB_PROCESS_NEXT_ARG;
}

