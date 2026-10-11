// MeshBuilder: styled primitives placed by transform or between two points into per-slot batches over pg_geo; finish uploads them and spawns static colliders.
// Main lane, single-threaded: begin, primitives, mesh_builder_finish. The builder must not be copied after begin.
#pragma once

#include "ecs/ecs.h"
#include "ecs/ecs_physics/ecs_physics.h"
#include "lib/array.h"
#include "lib/hzmath.h"
#include "lib/memory.h"
#include "procgen/pg_geo.h"
#include "rendering/renderer.h"

typedef enum MeshBuilderColliderKind {
  MESH_BUILDER_COLLIDER_NONE = 0,
  MESH_BUILDER_COLLIDER_BOX,      // box, capsule, cylinder and sphere primitives: that shape, placed by the transform
  MESH_BUILDER_COLLIDER_SPHERE,
  MESH_BUILDER_COLLIDER_CAPSULE,
  MESH_BUILDER_COLLIDER_CYLINDER, // a trimesh of the cylinder at style.collider_segments (0 = the primitive's)
} MeshBuilderColliderKind;

// a per-vertex weight in 0..1 (pos and normal are in the finished space; normal is the smooth outline normal when the slot has one)
typedef f32 (*MeshBuilderVertexFn)(v3 pos, v3 normal, const void *ctx);
// a per-vertex aux word; `aux` is the style's base word
typedef u32 (*MeshBuilderAuxFn)(v3 pos, v3 normal, u32 aux, const void *ctx);
typedef void (*MeshBuilderDeformFn)(v3 *pos, const void *ctx);

typedef struct MeshBuilderStyle {
  u32 slot;
  v4 color;                        // linear RGBA; w == 0 keeps the source colour (white for a primitive)
  v4 paint;                        // blended over `color` by paint_weight
  MeshBuilderVertexFn paint_weight;
  f32 shade_lo, shade_hi;          // colour multiplier from the part's lowest to highest y; both zero = off
  u32 aux;                         // aux word of every vertex; 0 keeps the source's
  MeshBuilderAuxFn aux_fn;
  MeshBuilderDeformFn deform;      // moves each finished-space vertex; normals are not recomputed
  b32 flat;                        // a face normal per triangle
  MeshBuilderColliderKind collider;
  u32 collider_segments;
  const void *ctx;                 // passed to every fn
} MeshBuilderStyle;

typedef struct MeshBuilderColliderDesc {
  MeshBuilderColliderKind kind;
  v3 center;
  quat rot;
  v3 half;          // box
  f32 radius;       // sphere, capsule
  v3 a, b;          // capsule end points
  f32 radius_top, radius_bottom, height; // cylinder
  u32 segments;
} MeshBuilderColliderDesc;
DYN_ARRAY_TYPEDEF(MeshBuilderColliderDesc)

typedef struct MeshBuilderDesc {
  u32 slot_count;
  u32 streams;      // PG_STREAM_* every slot carries from its first primitive
  u32 vtx_reserve;  // per slot, 0 = 1024
  u32 idx_reserve;  // per slot, 0 = 4096
} MeshBuilderDesc;

typedef struct MeshBuilder {
  ArenaAllocator *arena;
  Allocator alloc;
  u32 slot_count;
  u32 streams;
  u32 vtx_reserve, idx_reserve;
  PgMesh *slots; // [slot_count], begun on first use
  b32 *used;
  PgMesh tmp;
  DynArray(MeshBuilderColliderDesc) colliders;
} MeshBuilder;

// the uploaded result of one slot; parts is empty for a slot nothing was added to
typedef struct MeshBuilderSlotOut {
  HzHandleT(RendererSubMesh) *parts;
  u32 part_count; // more than one when the slot passed the u16 vertex limit
  aabb bounds;
  u32 vertex_count;
  u32 tri_count;
} MeshBuilderSlotOut;

typedef struct MeshBuilderResult {
  MeshBuilderSlotOut *slots;
  u32 slot_count;
  u32 vertex_count;
  u32 part_count;
  u32 collider_count;
} MeshBuilderResult;

typedef struct MeshBuilderFinishDesc {
  RendererState *renderer;
  EcsWorld *world;          // NULL only when no collider was added
  Allocator *alloc;         // the result, upload handles and trimesh collider blobs live as long as it does
  f32 uv_per_metre;         // 0 = 1
  PhysicsFilter collider_filter; // the layer the static bodies are created on; zero = the default layer
} MeshBuilderFinishDesc;

// sRGB 0xRRGGBB to a linear style colour
v4 mesh_builder_hex(u32 hex);
// placement of a +Y-aligned primitive between two points; len is their distance
m4 mesh_builder_segment(v3 from, v3 to, f32 *out_len);

void mesh_builder_begin(MeshBuilder *b, ArenaAllocator *arena, const MeshBuilderDesc *desc);
// the CPU mesh of a slot (begun on demand), for builders that write PgMesh directly or read it back
PgMesh *mesh_builder_slot_mesh(MeshBuilder *b, u32 slot);

// size in metres, centred
void mesh_builder_box(MeshBuilder *b, const MeshBuilderStyle *st, m4 xform, v3 size);
// rounded box of 2 segments
void mesh_builder_round_box(MeshBuilder *b, const MeshBuilderStyle *st, m4 xform, v3 size, f32 bevel);
// unit radius; the transform's scale gives the radii
void mesh_builder_sphere(MeshBuilder *b, const MeshBuilderStyle *st, m4 xform, u32 width_segments, u32 height_segments);
// unit radius polyhedron (octa, icosa, ...)
void mesh_builder_polyhedron(MeshBuilder *b, const MeshBuilderStyle *st, m4 xform, PgPolyhedronKind kind, u32 detail);
// along +Y, centred; length is the straight part
void mesh_builder_capsule(MeshBuilder *b, const MeshBuilderStyle *st, m4 xform, f32 radius, f32 length, u32 cap_segments,
                          u32 radial_segments);
void mesh_builder_capsule_between(MeshBuilder *b, const MeshBuilderStyle *st, v3 from, v3 to, f32 radius, u32 cap_segments,
                                  u32 radial_segments);
// along +Y, centred
void mesh_builder_cylinder(MeshBuilder *b, const MeshBuilderStyle *st, m4 xform, const PgCylinderDesc *d);
void mesh_builder_cylinder_between(MeshBuilder *b, const MeshBuilderStyle *st, v3 from, v3 to, f32 radius_from, f32 radius_to,
                                   u32 radial_segments);
// radius at `base`, a point at `tip`
void mesh_builder_cone_between(MeshBuilder *b, const MeshBuilderStyle *st, v3 base, v3 tip, f32 radius, u32 radial_segments);
// pg_revolve of a (radius, y) profile
void mesh_builder_lathe(MeshBuilder *b, const MeshBuilderStyle *st, m4 xform, const v2 *profile, u32 count, u32 segments);
void mesh_builder_torus(MeshBuilder *b, const MeshBuilderStyle *st, m4 xform, const PgTorusDesc *d);
void mesh_builder_sheet(MeshBuilder *b, const MeshBuilderStyle *st, m4 xform, PgSheetFn surface, void *ctx, f32 thickness,
                        u32 seg_u, u32 seg_v);
// any PgMesh under the transform and style (its colour/aux streams are kept where the style leaves them unset)
void mesh_builder_add_pg(MeshBuilder *b, const MeshBuilderStyle *st, m4 xform, const PgMesh *src);

// collision without geometry (a wall's hidden collider, a trigger-free blocker); centre/rot are world space
void mesh_builder_collider_box(MeshBuilder *b, v3 center, quat rot, v3 half);
void mesh_builder_collider_cylinder(MeshBuilder *b, v3 center, quat rot, f32 radius_top, f32 radius_bottom, f32 height,
                                    u32 segments);
void mesh_builder_collider_sphere(MeshBuilder *b, v3 center, f32 radius);
void mesh_builder_collider_capsule(MeshBuilder *b, v3 a, v3 b_end, f32 radius);

// main lane: uploads every used slot, spawns the static bodies, returns the handles in desc->alloc
MeshBuilderResult mesh_builder_finish(MeshBuilder *b, const MeshBuilderFinishDesc *desc);
