// Claude of Duty materials: the 19 procedural surfaces baked on the GPU and the cod_surface material over them.
#pragma once
#include "ecs/ecs.h"
#include "lib/color.h"
#include "lib/handle.h"
#include "lib/hzmath.h"
#include "lib/typedefs.h"
#include "rendering/renderer.h"
#include "claude_of_duty.h"

// the surface library, in its declaration order
typedef enum CodMaterialId {
  COD_MAT_CONCRETE = 0,
  COD_MAT_CONCRETE_FLOOR,
  COD_MAT_BRICK,
  COD_MAT_PLASTER,
  COD_MAT_TILE,
  COD_MAT_ASPHALT,
  COD_MAT_SAND,
  COD_MAT_DIRT,
  COD_MAT_GRAVEL,
  COD_MAT_METAL_RUST,
  COD_MAT_METAL_PAINTED,
  COD_MAT_METAL_BRUSHED,
  COD_MAT_CORRUGATED,
  COD_MAT_WOOD,
  COD_MAT_FABRIC,
  COD_MAT_BURLAP,
  COD_MAT_FOLIAGE,
  COD_MAT_RUBBER,
  COD_MAT_GLASS,
  COD_MAT_COUNT,
} CodMaterialId;

// how a mesh samples its surface
typedef enum CodUvMode {
  COD_UV_TRIPLANAR = 0, // world-space triplanar: no seams, no parallax or de-tiling
  COD_UV_MESH = 1,      // the mesh's own uv, in metres
  COD_UV_PLANAR = 2,    // the world axis the face points along: parallax and de-tiling work; the default for architecture
  COD_UV_LIBRARY = 3,   // the library entry's own mode (a request without a uv mode)
} CodUvMode;

// CodMaterialDesc.override_mask: which of the array overrides replace the library's values (materials opts)
typedef enum CodMaterialOverride {
  COD_OVR_DETAIL = 1u << 0,        // detail: tiles per base tile, normal amount, albedo amount, fade metres
  COD_OVR_MACRO = 1u << 1,         // macro: world scale, albedo, roughness and hue strength
  COD_OVR_WEATHER = 1u << 2,       // weather: dust, rain streak, ground-splash height, cavity grime
  COD_OVR_WEAR = 1u << 3,          // wear: edge wear, grime and extra AO strength from the vertex masks
  COD_OVR_WEAR_MATERIAL = 1u << 4, // roughness, metalness, unused, tint amount where the wear mask is 1
  COD_OVR_DETILE = 1u << 5,        // detile amount (planar and mesh uv only)
  COD_OVR_ROUGHNESS = 1u << 6,     // roughness scale, offset, minimum
  COD_OVR_AO = 1u << 7,            // ao_strength
  COD_OVR_NORMAL = 1u << 8,        // normal_strength
} CodMaterialOverride;

// a material request: a library surface plus per-use overrides
typedef struct CodMaterialDesc {
  u32 id;            // CodMaterialId
  u32 uv_mode;       // CodUvMode
  f32 scale;         // metres per tile (uv multiplier for COD_UV_MESH); 0 = the library's
  ColorF32 tint;     // linear multiplier; a = 0 means none
  f32 parallax;      // < 0 = the library's
  f32 weather;       // weathering strength multiplier; 0 = the library's
  u32 bake_variant;  // 0 = the library bake; else a distinct bake (a paint colour) keyed by bake_color
  u32 bake_color;    // sRGB 0xRRGGBB for a variant bake
  b32 double_sided;
  b32 transparent;   // glass panes
  // additions (all zero = the default)
  b32 vertex_masks;  // the mesh's COLOR stream carries the baked wear, grime, extra AO; without it COLOR is ignored
  b32 local_space;   // project in the object's space with COD_UV_TRIPLANAR (opts.localSpace: the viewmodel)
  u32 bake_size;     // 0 = the library's; else a distinct bake at this size
  u32 bake_seed;     // 0 = the library's
  f32 bake_relief;   // 0 = the library's
  u32 override_mask; // CodMaterialOverride bits
  v4 detail;
  v4 macro;
  v4 weather_v;
  v4 wear;
  v4 wear_material;
  f32 detile;
  v4 roughness;      // x scale, y offset, z minimum
  f32 ao_strength;
  f32 normal_strength;
  u32 emissive;          // sRGB 0xRRGGBB; 0 = none
  f32 emissive_intensity; // scene units (x COD_SCENE_UNIT cd/m2)
  f32 opacity;           // 0 = the library's (three opacity)
  f32 env_intensity;     // reflection strength; 0 = 1
  b32 viewmodel;         // lit by the view rig instead of the world's lights
} CodMaterialDesc;

// main lane: a material's emission at runtime (the lamp lens follows the time of day), scene units as the desc's
void cod_materials_set_emissive(EcsWorld *w, HzHandleT(RendererMaterial) material, u32 emissive, f32 intensity);

// the baked texture set of one surface
typedef struct CodTextureSet {
  GpuTexture albedo; // sRGB albedo + height in alpha
  GpuTexture normal; // tangent-space normal
  GpuTexture orm;    // occlusion, roughness, metalness (alpha: the height residual)
  u32 size;
  f32 world_size; // metres the tile spans
} CodTextureSet;

#define COD_MATERIALS_MAX 256

// what the materials package publishes; cod_materials_get is the only way other packages make a surface material
typedef struct CodMaterials {
  b32 ready;     // every library bake is done; materials can be requested
  f32 progress;  // 0..1 bake progress
  CodTextureSet sets[COD_MAT_COUNT];
  GpuTexture detail;        // the shared micro detail: layer 0 linear micro albedo variation (rgb) and micro height (alpha), layer 1 the micro normal (rg)
  GpuTexture macro;         // the shared macro variation map
  f64 bake_ms;
} CodMaterials;
ECS_COMPONENT_TYPEDEF(CodMaterials);

// the physics/FX surface of a library entry
CodSurface cod_material_surface(u32 id);
// main lane, after CodMaterials.ready: the material for a request, cached by its full desc (same desc, same handle)
HzHandleT(RendererMaterial) cod_materials_get(EcsWorld *w, const CodMaterialDesc *desc);
// a request for a library entry with the defaults: the library's values everywhere (parallax -1), its own projection
CodMaterialDesc cod_material_desc(u32 id);
// main lane: where the ground-splash weathering starts, for every material made and to be made
void cod_materials_set_ground_level(EcsWorld *w, f32 y);

// main lane, app_init: resources, the bake pipelines
void cod_materials_init(EcsWorld *w);
// inside the scope: advances the GPU bakes over the first frames, publishes CodMaterials
void cod_materials_schedule(EcsWorld *w);
// main lane after the scope: records this frame's bake passes (GPU recording is main-lane work)
void cod_materials_frame(EcsWorld *w);
// every lane, before the scope: bulk parallel work (lane_range + balanced lane_sync; lane-uniform conditions only)
void cod_materials_lanes(EcsWorld *w);
