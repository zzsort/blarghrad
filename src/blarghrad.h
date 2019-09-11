#pragma once
#include "verify.h"

#pragma warning(disable : 4996)

#define	MAX_PATCHES	65000			// larger will cause 32 bit overflows

#define	MAX_LSTYLES	256
#define	MAX_STYLES	32

// TYPES

typedef enum
{
    emit_surface = 0,
    emit_point = 1,
    emit_spotlight = 2,
    emit_sunlight = 3,
} emittype_t;

struct pak_t {
    char pakfile[1024];
    unsigned int numdir;
    dpackfile_t* dir;
    pak_t* nextpak;
};

struct projtexture_t {
    projtexture_t* next;
    int	width;
    int	height;
    int	has_transparent_pixels;
    byte* texture32;
};

struct shadowfaces_unk_t {
    shadowfaces_unk_t* next;
    projtexture_t* projtex;
    int face;
    int UNKNOWN_FIELD_0xC; // maybe: 0 = nonTrans, 1 = trans. seems to index into shadowmodel_t
    int maybe_bool;
    vec3_t mins;
    vec3_t maxs;
};

struct shadowmodel_t {
    shadowmodel_t* next;
    shadowfaces_unk_t* shadownext;
    int modelnum;
    int nonTransFaces;
    int transFaces; // this field must immediately follow nonTransFaces!
};

struct directlight_t {
    struct directlight_t * m_next;
    int m_emittype; /* 0,1,2,3 */
    float m_intensity;
    int m_style;
    struct vec3_t m_origin;
    struct vec3_t m_color;
    struct vec3_t m_normal;
    float m_cone;
    float m_distance;
    float m_angwait;
    float m_focus;
    int m_falloff;
    dface_t* m_face;
    float m_cap;
    int m_bounce;
    float m_choplight;
};

// the sum of all tranfer->transfer values for a given patch
// should equal exactly 0x10000, showing that all radiance
// reaches other patches
typedef struct
{
    unsigned short	patch;
    unsigned short	transfer;
} transfer_t;

struct facegroup_t {
    byte byte1;
    byte byte2_UNUSED;
    unsigned short start;
    unsigned short end;
};

struct facelight_t {
    int			numsamples;
    vec3_t		*origins;
    int			numstyles;
    int			stylenums[MAX_STYLES];
    vec3_t		*samples[MAX_STYLES];
};

struct lightinfo_t {
    float facedist; /* LOCK 0 */
    vec3_t facenormal; /* LOCK 4 */
    int numsurfpt; /* LOCK 16 */
    vec3_t* surfpt; /* LOCK 20 */
    vec3_t* realpt; /* LOCK 24 */
    vec3_t modelorg; /* LOCK 28 */
    vec3_t texorg; /* LOCK 40 */
    vec3_t worldtotex[2]; /* LOCK 52 */
    vec3_t textoworld[2]; /* lock? */
    float exactmins[2]; /* lock  ?? */
    float exactmaxs[2]; /* lock? */
    int texmins[2]; /* lock?? */
    int texsize[2]; /* LOCK 124 */
    dface_t * face; /* LOCK 136????? */
    int surfnum;
};
#ifdef _M_IX86
static_assert(sizeof(lightinfo_t) == 0x8c, "");
#endif

struct patch_t { /* size is 124!! */
    winding_t * winding; /* LOCK 0 */
    patch_t * next; /* LOCK 4 */
    int numtransfers; /* LOCK 8 */
    transfer_t * transfers; /* LOCK 12 */
    int cluster; /* LOCK 16 */
    vec3_t origin; /* lock 20 maybe */
    dplane_t * plane; /* LOCK 32 */
    vec3_t totallight; /* lock 36 probably */
    float area; /* LOCK 48 */
    vec3_t reflectivity;
    vec3_t baselight; /* REVIEW */
    vec3_t samplelight; /* LOCK 76 */
    int samples; /* LOCK 88 */
    float distance;
    float focus;
    float lightmin;
    int styletable; /* LOCK 104 - prob ptr ary */
    vec3_t normal;
    int facenum; /* LOCK 120 */
};
#ifdef _M_IX86
static_assert(sizeof(patch_t) == 124, "");
#endif

struct unknownunk_t {
    int txLightValue;
    byte liquid_contents;
    byte UNUSED[3];
    vec3_t normal;
    unknownunk_t* next;
};

struct suninfo_t {
    const char* target; // weak ptr into entity
    vec3_t direction;
    vec3_t color;
    int UNUSED;
    double light;
    double diffuse;
    double diffade;
    int style;
    int bool_maybe_sun_is_active;
};

// temporary - remove
void UNREACHABLE(const char* where);
void NOT_IMPLEMENTED(const char* where);


// EXTERN
int TestLine_r(int node, const vec3_t* start, const vec3_t* stop, vec3_t* out_vec);
void MakeTnodes(dmodel_t *bm);
void FreeTnodes();
void MakePatches(void);
void SubdividePatches(void);
bool FacesHaveSharedVertexes(int facenum, int otherface);
int TestLine_shadow(const vec3_t& start, const vec3_t& stop, vec3_t *out_param_3, vec3_t *optional_out_vec);
void CalcPoints(lightinfo_t *l, float sofs, float tofs, int facenum);
void CalcPoints2(lightinfo_t *l, float sofs, float tofs, int facenum);
void CalcFaceVectors(lightinfo_t *l);
void CalcFaceExtents(lightinfo_t *l);
void maybePhongSomething(int facenum, const vec3_t& center, const vec3_t& plane_normal, vec3_t& out_vec_param_4);
void FinalLightFace(int facenum);

void GatherSampleLight(const vec3_t& pos, const vec3_t& realpt, const vec3_t& normal, vec3_t *styletable[MAX_LSTYLES],
    vec3_t *bouncelight, int offset, int mapsize, float lightscale);

void SetQdirFromPath_v2(char *path);
char* SetQdirFromPath_v3(char *path);

extern vec3_t texture_reflectivity[MAX_MAP_TEXINFO];
extern entity_t	*face_entity[MAX_MAP_FACES];
extern vec3_t face_offset[MAX_MAP_FACES];
extern dleaf_t *PointInLeaf(vec3_t point);

extern dplane_t backplanes[MAX_MAP_PLANES];
extern int fakeplanes;
extern patch_t *face_patches[MAX_MAP_FACES];
extern patch_t patches[MAX_PATCHES];
extern unsigned num_patches;

extern byte g_hashset_face[MAX_MAP_FACES];

extern int invisfix;
extern shadowfaces_unk_t* g_shadow_faces[MAX_MAP_FACES];

extern int bmodlight;

extern vec3_t ambient;
extern float subdiv;
extern float chopsky;
extern float chopwarp;
extern float choplight;
extern float chopcurve;
extern int nocurve;
extern int lightwarp;
extern int numbounce;
extern int radorigin;
extern int stopbleed;
extern int onlybounce;
extern int _nocolor_maybe_unweighted;
extern float saturation;
extern float lightscale;
extern float gamma;
extern float maxlight;
extern float minlight;

extern unknownunk_t* g_maybe_vertex_phong[MAX_MAP_VERTS];

extern shadowmodel_t* g_shadow_world;
extern projtexture_t* g_proj_textures;

extern float g_texscale;
extern int shadowfilter;

extern facelight_t facelight[MAX_MAP_FACES];

extern char moddir[256];
extern int game;

extern facegroup_t* facegroups;