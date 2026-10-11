// Claude of Duty soldier bodies, package-private: the rig, the procedural build, the material bakes, the animator and the ragdoll (never included by other packages).
#pragma once
#include "lib/array.h"
#include "lib/hzmath.h"
#include "lib/typedefs.h"
#include "procgen/pg_geo.h"
#include "rendering/renderer.h"
#include "rendering/features/decals.h"
#include "cod_ai_body.h"
#include "cod_rng.h"

// ---------------------------------------------------------------- rig ---

// the bind pose the clips are deltas on, derived once from cod_rig_bind
typedef struct CabRig {
  v3 bind_pos[COD_BONE_COUNT];
  quat bind_quat[COD_BONE_COUNT];
  v3 local_pos[COD_BONE_COUNT];
  quat local_quat[COD_BONE_COUNT];
  v3 tail[COD_BONE_COUNT]; // end of the bone: its primary child, or a 7.5 cm stub
  i32 first_child[COD_BONE_COUNT]; // the first bone listed under it, -1 for a leaf
} CabRig;

// ---------------------------------------------------------------- noise ---

// Perlin 3D over a shuffled 256-entry permutation
typedef struct CabNoise {
  u8 *perm; // 512 entries
} CabNoise;

void cab_noise_init(CabNoise *n, CodRng *rng, Allocator *alloc);
f32 cab_n3(const CabNoise *n, f32 x, f32 y, f32 z);
f32 cab_fbm3(const CabNoise *n, f32 x, f32 y, f32 z, u32 oct);

// ------------------------------------------------------------ materials ---

// the nine geometry material sets, in the order their names are first met
typedef enum CabMat {
  CAB_MAT_CLOTH = 0,
  CAB_MAT_GEAR,
  CAB_MAT_BOOT,
  CAB_MAT_RUBBER,
  CAB_MAT_PLATE,
  CAB_MAT_POLYMER,
  CAB_MAT_SKIN,
  CAB_MAT_GLASS,
  CAB_MAT_STEEL,
  CAB_MAT_COUNT,
} CabMat;

// sets and detail tiles
typedef enum CabSet {
  CAB_SET_CAMO_ARID = 0,
  CAB_SET_CAMO_WOODLAND,
  CAB_SET_CAMO_URBAN,
  CAB_SET_NYLON,
  CAB_SET_PLATE,
  CAB_SET_SKIN,
  CAB_SET_POLYMER,
  CAB_SET_STEEL,
  CAB_SET_RUBBER,
  CAB_SET_COUNT,
} CabSet;

#define CAB_BAKE_DETAIL_CLOTH CAB_SET_COUNT
#define CAB_BAKE_DETAIL_NYLON (CAB_SET_COUNT + 1)
#define CAB_BAKE_COUNT (CAB_SET_COUNT + 2)
#define CAB_DETAIL_COUNT 2

#define CAB_TEX_SIZE 512

// tileable value noise
typedef struct CabTileNoise {
  f32 *tab;  // 4096
  u16 *perm; // 4096
} CabTileNoise;

// what a bake's texel function produces before packing
typedef struct CabTexel {
  f32 r, g, b, h, rough, metal, ao;
} CabTexel;

// one bake's maps: the packed mip chains while baking, the GPU textures after (detail tiles use albedo for their one RGBA map)
typedef struct CabBake {
  f32 *height;
  f32 *rough;
  u8 *albedo;
  u8 *normal;
  u8 *orm;
  GpuTexture tex_albedo;
  GpuTexture tex_normal;
  GpuTexture tex_orm;
} CabBake;

typedef struct CabTexState {
  CabTileNoise noise;
  f32 *srgb_decode;
  u8 *srgb_encode;
  f32 *camo_row;
  f32 camo_src_mean[3];
  CabBake bake[CAB_BAKE_COUNT];
  u64 t0;
  f64 ms;
} CabTexState;

void cab_tile_noise_init(CabTileNoise *n, CodRng *rng, Allocator *alloc);
void cab_tex_init(CabTexState *t, Allocator *alloc, CodRng *rng);
void cab_tex_lanes(CabTexState *t);
void cab_tex_upload(CabTexState *t);

// ---------------------------------------------------------------- build ---

// one soldier variant
typedef struct CabVariantDef {
  const char *name;
  u32 camo; // CabSet camo
  v3 cloth_tint;
  v3 gear_tint;
  v3 plate_tint;
  v3 skin_tint;
  b32 helmet;
  b32 helmet_cover;
  v3 helmet_tint;
  b32 goggles;
  b32 goggles_down;
  b32 face_wrap;
  b32 mask_hard;
  b32 head_wrap;
  b32 shades;
  b32 knee_pads;
  b32 full_carrier;
  b32 long_weapon; // 'ak'
  f32 bulk;
  f32 scale;
} CabVariantDef;

// every part the soldier is assembled from
typedef enum CabKind {
  CAB_PART_JACKET = 0,
  CAB_PART_PELVIS,
  CAB_PART_COLLAR,
  CAB_PART_SHOULDER_CAP,
  CAB_PART_SLEEVE,
  CAB_PART_ELBOW_PAD,
  CAB_PART_LEG,
  CAB_PART_POUCH,
  CAB_PART_KNEE_PAD,
  CAB_PART_BOOT,
  CAB_PART_BOOT_SOLE,
  CAB_PART_BOOT_LACES,
  CAB_PART_CARRIER,
  CAB_PART_WEBBING,
  CAB_PART_BELT,
  CAB_PART_HEAD,
  CAB_PART_NOSE,
  CAB_PART_EAR,
  CAB_PART_EYE,
  CAB_PART_NECK,
  CAB_PART_FACE_WRAP,
  CAB_PART_HELMET,
  CAB_PART_HELMET_HW,
  CAB_PART_CHIN_STRAP,
  CAB_PART_GOGGLE_FRAME,
  CAB_PART_GOGGLE_STRAP,
  CAB_PART_GOGGLE_LENS,
  CAB_PART_SCARF,
  CAB_PART_SHADE_FRAME,
  CAB_PART_SHADE_LENS,
  CAB_PART_GLOVE,
  CAB_PART_KNUCKLE,
  CAB_PART_WEAPON, // steel, polymer, rubber, glass in one job (the four parts after it only borrow its meshes)
  CAB_PART_WPN_STEEL,
  CAB_PART_WPN_POLY,
  CAB_PART_WPN_RUBBER,
  CAB_PART_WPN_GLASS,
  CAB_PART_SLING,
} CabKind;

#define CAB_CAND_MAX 9
#define CAB_SPECS_MAX 80
#define CAB_POUCHES 3
#define CAB_RADII_MAX 7

// a pouch
typedef struct CabPouch {
  f32 hx, hy, hz;
  f32 x, y, z;
  f32 rx, ry, rz;
  f32 lid_tilt;
  f32 bend;
} CabPouch;

// one part: the mesh recipe and the skin, vertex-tint and texture settings
typedef struct CabSpec {
  u32 kind;       // CabKind
  u32 mat;        // CabMat
  i32 bone;       // rigid bone, or -1 for a smooth bind
  u32 cand_count;
  u32 cand[CAB_CAND_MAX];
  f32 bias[CAB_CAND_MAX];
  v3 colour;
  f32 grime;
  f32 dirt;
  f32 dust;
  f32 wear;
  v3 p0, p1, p2;  // limb chain, or the glove frame: wrist, grip axis, palm normal
  v3 bend;        // the direction a limb tube bunches toward
  f32 side;
  f32 scalar;     // the jacket's bulk
  u32 rings;
  u32 seg;
  f32 fold;
  f32 crease;
  u32 radii_count;
  f32 radii[CAB_RADII_MAX];
  b32 flag;       // goggles down
  CabPouch pouch;
  b32 job;        // the kind builds geometry (the weapon pieces only borrow the weapon job's meshes)
  PgMesh *mesh;   // built by a lane
  u32 vo;         // first vertex in the variant's arrays
  u32 vn;
  u32 io;         // first index
  u32 in;
} CabSpec;

// the geometry of one material set: a contiguous run of the variant's vertices and indices
typedef struct CabGroup {
  u32 mat; // CabMat
  u32 vstart;
  u32 vcount;
  u32 istart;
  u32 icount;
} CabGroup;

// outputs, in the weapon frame baked to bind space
typedef struct CabWeapon {
  PgMesh steel;
  PgMesh polymer;
  PgMesh rubber;
  PgMesh glass;
  v3 muzzle;
  v3 bore_origin;
  v3 ejection;
  v3 stock_top;
  v3 foregrip;
  v3 mag_bottom;
} CabWeapon;

// weapon anchors in HandR bind-local space (Animator constructor)
typedef struct CabAnchors {
  v3 bore;
  v3 muzzle;
  v3 foregrip;
  v3 mag;
  v3 eject;
} CabAnchors;

// an ambient-occlusion proxy capsule
typedef struct CabOcc {
  v3 a, b;
  f32 r, k;
} CabOcc;

#define CAB_OCC_MAX 40

// a built variant: the merged skinned geometry and the anchors the animator needs
typedef struct CabVariant {
  const CabVariantDef *def;
  u32 index;
  CodRng rng;     // the variant's build stream
  CabNoise nz;
  f32 pouch_y[CAB_POUCHES];
  f32 pouch_rz[CAB_POUCHES];
  f32 cant;
  CabSpec spec[CAB_SPECS_MAX];
  u32 spec_count;
  CabWeapon weapon;
  u32 order[CAB_SPECS_MAX]; // spec indices grouped by material
  u32 order_count;
  // merged arrays
  u32 vertex_count;
  u32 index_count;
  v3 *pos;
  v3 *nrm;
  v2 *uv;
  v4 *col;
  u16 *ji;  // 4 per vertex
  v4 *jw;
  u32 *idx;
  CabGroup group[CAB_MAT_COUNT];
  u32 group_count;
  aabb bounds;
  f64 build_ms;
  CabAnchors anchors;
  HzHandleT(RendererSubMesh) sub[CAB_MAT_COUNT];
  HzHandleT(RendererMaterial) mat[CAB_MAT_COUNT];
  aabb envelope; // model-local cull box that holds every pose
} CabVariant;

#define CAB_VARIANTS 3

// in key order: the CodSoldier.variant values the brain publishes
const CabVariantDef *cab_variant_def(u32 variant);
// the number of distinct occlusion proxies
#define CAB_OCCLUDERS_MAX 40

// ---- cod_ai_body_geo.c: the loft toolkit ----

typedef struct CabRing {
  const v2 *pts; // k profile points in the ring's XZ plane
  v3 o;
  quat q;
  v2 s;
} CabRing;

PgMesh *cab_mesh(ArenaAllocator *arena);
void cab_super_ellipse(v2 *out, f32 rx, f32 rz, f32 n, u32 seg);
void cab_ellipse_profile(v2 *out, f32 rx, f32 rz, u32 seg);
void cab_loft(PgMesh *out, const CabRing *rings, u32 ring_count, u32 k, b32 cap_start, b32 cap_end);
void cab_path_frames(quat *out, const v3 *points, u32 n, v3 up_ref);
void cab_tube(PgMesh *out, const v3 *points, u32 n, const v2 *profiles, u32 k, v3 up_ref, b32 cap_start, b32 cap_end);
void cab_ribbon(PgMesh *out, const v3 *points, u32 n, f32 width, f32 thick, u32 seg, v3 up, b32 upright);
void cab_box_round(PgMesh *out, f32 hx, f32 hy, f32 hz, f32 n, u32 seg, u32 rows, f32 round_y);
void cab_ellipsoid(PgMesh *out, f32 rx, f32 ry, f32 rz, u32 seg, u32 rows, f32 v0, f32 v1);
void cab_compute_normals(PgMesh *m);
void cab_weld_normals(PgMesh *m);
void cab_place(PgMesh *m, f32 x, f32 y, f32 z, f32 rx, f32 ry, f32 rz);
void cab_bend_y(PgMesh *m, f32 radius, f32 centre_z);
void cab_transform(PgMesh *m, m4 xform);

// ---- cod_ai_body_parts.c ----

void cab_part_build(const CabNoise *nz, const CabVariant *v, const CabSpec *s, PgMesh *out, ArenaAllocator *arena);
void cab_weapon_build(const CabNoise *nz, CabVariant *v, ArenaAllocator *arena);
void cab_weapon_anchors(CabWeapon *w, b32 long_weapon, f32 cant);

// each cab_* builder below is dispatched by cab_part_build

// ------------------------------------------------------------- the state ---

typedef enum CabStage {
  CAB_STAGE_BUILD = 0, // every lane builds the geometry and bakes (cod_ai_body_lanes)
  CAB_STAGE_UPLOAD,    // main lane: meshes and textures to the GPU, materials
  CAB_STAGE_DONE,
} CabStage;

// the body package's private runtime, one allocation behind the CodBodyState resource
#define CAB_GROUND_CAP 32
// the grounding sprites of one soldier: a body ellipse and a lobe per boot
typedef struct CabGroundSlot {
  EcsEntity owner;
  b32 used;
  HzHandleT(RendererSubMeshInstance) h[3];
} CabGroundSlot;

typedef struct CabState {
  EcsWorld *world;
  Allocator persist;
  CabRig rig;
  CabTexState *tex;
  CabVariant *variant[CAB_VARIANTS];
  CabOcc occ[CAB_OCC_MAX];
  u32 occ_count;
  ArenaAllocator **lane_arena;
  u32 lane_count;
  u32 stage; // CabStage
  u32 job_count;
  u32 *job;  // variant << 16 | spec
  u32 merge_count;
  u32 *merge; // variant << 16 | position in the variant's order
  u64 build_t0;
  f64 build_ms;
  f64 geo_ms;
  f64 shade_ms;
  b32 logged;
  EcsQuery *ground_query;
  CabGroundSlot ground[CAB_GROUND_CAP];
  HzHandleT(MaterialBlobAsset) ground_asset;
  HzHandleT(RendererMaterial) gbase;
  HzHandleT(RendererMaterial) gmat[2]; // body, foot
  HzHandleT(RendererSubMesh) gquad;
  b32 gtex_ready;
  // renderer objects (main lane)
  HzHandleT(MaterialBlobAsset) base_asset;
  HzHandleT(RendererMaterial) base;
  b32 ready; // meshes, materials and models are on the GPU
  b32 tex_uploaded;
  f32 progress;
  u64 ragdoll_group; // next ragdoll serial
} CabState;

// ---------------------------------------------------------------- animator ---

// additive euler deltas in degrees per bone and the hip offset, filled by the clip functions
typedef struct CabPoser {
  v3 d[COD_BONE_COUNT];
  v3 hip;
  f32 w;
} CabPoser;

// the per-soldier animator runtime (Animator): blend state, one-shot timers, the pose in actor space
ECS_COMPONENT_TYPEDEF(CabAnim);
typedef struct CabAnim {
  b32 enabled;
  u32 variant;
  u32 clip;
  u32 prev_clip;
  f32 blend;
  f32 phase;
  f32 recoil_t, recoil_k;
  f32 hit_t, hit_side, hit_k;
  u32 hit_region;
  f32 reload_t, reload_dur;
  f32 vault_t, vault_dur;
  f32 turn_t, turn_dir;
  u32 seen_fire, seen_hit, seen_reload, seen_vault, seen_turn, seen_death;
  f32 accum;
  u32 skip;
  b32 posed;
  i32 ground_slot; // the contact-occlusion decals' slot, -1 none
  b32 want_ragdoll; // the death serial changed: the ragdoll system builds the bodies from this pose
  b32 ragdolled;
  u32 dbg;
  HzHandleT(RendererSubMeshInstance) child[CAB_MAT_COUNT];
  u32 child_count;
  CabPoser poser;
  v3 lpos[COD_BONE_COUNT];
  quat lrot[COD_BONE_COUNT];
  v3 apos[COD_BONE_COUNT];
  quat arot[COD_BONE_COUNT];
  v3 muzzle, muzzle_dir, eject;
  f32 foot_y[2];
  v3 foot_n[2];
} CabAnim;

// the ground probe of the foot IK: the static layer and the actor's frame
typedef struct CabProbe {
  const EcsCollisionLayer *layer;
  v3 gpos;
  quat grot;
  f32 scale;
} CabProbe;

void cab_rig_init(CabRig *rig);
void cab_variant_specs(CabVariant *v);
void cab_occluders(CabOcc *out, u32 *count);

void cab_anim_reset(CabAnim *a, const CodSoldierDrive *d, u32 variant);
void cab_anim_state(CabAnim *a, const CodSoldierDrive *d);
void cab_anim_update(CabAnim *a, const CabRig *rig, const CabAnchors *anc, const CodSoldierDrive *d, f32 dt, const CabProbe *pc);

// the ragdoll's bodies: one capsule rigid body per bone, owned by the soldier entity
ECS_COMPONENT_TYPEDEF(CabRagdoll);
typedef struct CabRagdoll {
  b32 active;
  EcsEntity body[COD_BONE_COUNT];
  f32 half_len[COD_BONE_COUNT];
} CabRagdoll;

// tags a ragdoll body or joint with its soldier so a destroyed soldier takes them along
ECS_COMPONENT_TYPEDEF(CabRagdollPart);
typedef struct CabRagdollPart {
  EcsEntity owner;
} CabRagdollPart;

ECS_COMPONENT_TYPEDEF(CodBodyState);
typedef struct CodBodyState {
  CabState *s;
} CodBodyState;

