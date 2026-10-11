// Claude of Duty materials: the procedural surface library baked on the GPU, and the cod_surface material over it.
#include "assets/assets.h"
#include "ecs/renderer/ecs_renderer.h"
#include "gpu/gpu.h"
#include "procgen/pg_geo.h"
#include "shaders/cod_bake_asphalt.h"
#include "shaders/cod_bake_brick.h"
#include "shaders/cod_bake_burlap.h"
#include "shaders/cod_bake_concrete.h"
#include "shaders/cod_bake_corrugated.h"
#include "shaders/cod_bake_detail.h"
#include "shaders/cod_bake_dirt.h"
#include "shaders/cod_bake_fabric.h"
#include "shaders/cod_bake_foliage.h"
#include "shaders/cod_bake_glass.h"
#include "shaders/cod_bake_gravel.h"
#include "shaders/cod_bake_macro.h"
#include "shaders/cod_bake_metal_brushed.h"
#include "shaders/cod_bake_metal_painted.h"
#include "shaders/cod_bake_metal_rust.h"
#include "shaders/cod_bake_normal.h"
#include "shaders/cod_bake_plaster.h"
#include "shaders/cod_bake_rubber.h"
#include "shaders/cod_bake_sand.h"
#include "shaders/cod_bake_tile.h"
#include "shaders/cod_bake_wood.h"
#include "shaders/pg_bake_downsample_mrt.h"
#include "shaders/pg_bake_downsample_rg.h"

#include "cod_materials_lib.c"
#include "cod_materials_bake.c"

// the authored cod_surface material variants (assets/claude_of_duty/materials): projection, side, alpha mask, space, glass, viewmodel (parallax is per material)
#define COD_MATERIAL_VARIANT_COUNT 256u
#define COD_VAR_DS (1u << 3)
#define COD_VAR_ALPHA (1u << 4)
#define COD_VAR_LOCAL (1u << 5)
#define COD_VAR_GLASS (1u << 6)
#define COD_VAR_VIEW (1u << 7)

typedef struct CodMatCacheEntry {
  CodResolved key;
  HzHandleT(RendererMaterial) material;
  u32 slot;
} CodMatCacheEntry;

// main-lane state of the materials package
typedef struct CodMatState {
  CodBakeState bake;
  u32 quality;
  f32 ground_y;
  HzHandleT(MaterialBlobAsset) asset[COD_MATERIAL_VARIANT_COUNT];
  HzHandleT(RendererMaterial) base[COD_MATERIAL_VARIANT_COUNT];
  u32 variants_total;  // authored variants loading
  u32 variants_ready;  // base materials created
  CodMatCacheEntry cache[COD_MATERIALS_MAX];
  u32 cache_count;
  b32 library_logged;
  b32 warned_not_ready;
} CodMatState;
ECS_COMPONENT_TYPEDEF(CodMatState);

#define COD_VARIANT_DIR "claude_of_duty/materials/cod_surface_"

// the asset path of a variant key, or NULL when none is authored
hz_internal const char *cod_variant_file(u32 key) {
  switch (key) {
  case COD_UV_PLANAR: return COD_VARIANT_DIR "planar.hzt";
  case COD_UV_PLANAR | COD_VAR_DS: return COD_VARIANT_DIR "planar_ds.hzt";
  case COD_UV_PLANAR | COD_VAR_DS | COD_VAR_GLASS: return COD_VARIANT_DIR "planar_glass.hzt";
  case COD_UV_PLANAR | COD_VAR_DS | COD_VAR_ALPHA: return COD_VARIANT_DIR "planar_alpha_ds.hzt";
  case COD_UV_MESH: return COD_VARIANT_DIR "mesh.hzt";
  case COD_UV_MESH | COD_VAR_DS: return COD_VARIANT_DIR "mesh_ds.hzt";
  case COD_UV_MESH | COD_VAR_DS | COD_VAR_ALPHA: return COD_VARIANT_DIR "mesh_alpha_ds.hzt";
  case COD_UV_TRIPLANAR: return COD_VARIANT_DIR "tri.hzt";
  case COD_UV_TRIPLANAR | COD_VAR_LOCAL: return COD_VARIANT_DIR "tri_local.hzt";
  case COD_UV_TRIPLANAR | COD_VAR_LOCAL | COD_VAR_VIEW: return COD_VARIANT_DIR "tri_local_view.hzt";
  case COD_UV_TRIPLANAR | COD_VAR_DS: return COD_VARIANT_DIR "tri_ds.hzt";
  case COD_UV_TRIPLANAR | COD_VAR_DS | COD_VAR_GLASS: return COD_VARIANT_DIR "tri_glass.hzt";
  default: return NULL;
  }
}

hz_internal u32 cod_variant_key(const CodMatParams *m) {
  u32 key = m->uv_mode & 3u;
  if (m->double_sided || m->transparent) key |= COD_VAR_DS;
  if (m->alpha_mask) key |= COD_VAR_ALPHA;
  if (m->local_space) key |= COD_VAR_LOCAL;
  if (m->transparent) key |= COD_VAR_GLASS;
  if (m->viewmodel) key |= COD_VAR_VIEW;
  return key;
}

// the log name of a slot
hz_internal const char *cod_slot_name(u32 slot) {
  static_assert(COD_MAT_COUNT == 19, "cod_slot_name names the library");
  if (slot < COD_MAT_COUNT) {
    CodSurfaceDef d;
    cod_library(slot, &d);
    return d.name;
  }
  if (slot == COD_SLOT_DETAIL) return "detail";
  if (slot == COD_SLOT_MACRO) return "macro";
  return "variant";
}

CodSurface cod_material_surface(u32 id) {
  switch (id) {
  case COD_MAT_PLASTER: return COD_SURFACE_PLASTER;
  case COD_MAT_ASPHALT: return COD_SURFACE_CONCRETE;
  case COD_MAT_SAND: return COD_SURFACE_SAND;
  case COD_MAT_DIRT:
  case COD_MAT_GRAVEL: return COD_SURFACE_DIRT;
  case COD_MAT_METAL_RUST:
  case COD_MAT_METAL_PAINTED:
  case COD_MAT_METAL_BRUSHED:
  case COD_MAT_CORRUGATED: return COD_SURFACE_METAL;
  case COD_MAT_WOOD: return COD_SURFACE_WOOD;
  case COD_MAT_FABRIC:
  case COD_MAT_BURLAP: return COD_SURFACE_FABRIC;
  case COD_MAT_FOLIAGE: return COD_SURFACE_FOLIAGE;
  case COD_MAT_RUBBER: return COD_SURFACE_RUBBER;
  case COD_MAT_GLASS: return COD_SURFACE_GLASS;
  default: return COD_SURFACE_CONCRETE;
  }
}

CodMaterialDesc cod_material_desc(u32 id) {
  CodMaterialDesc d;
  memzero_struct(&d);
  d.id = id;
  d.uv_mode = COD_UV_LIBRARY;
  d.parallax = -1.0f;
  return d;
}

// reports the bake progress into the load gate
ecs_system(cod_materials_load_system, EcsResIn(CodMaterials), EcsResInOut(CodLoad)) {
  const CodMaterials *m = ecs_sys_get_resource(CodMaterials);
  ecs_sys_get_resource_mut(CodLoad)->progress[COD_LOAD_MATERIALS] = m->ready ? 1.0f : m->progress;
}

// the shader-side texture density of the detail map: derived from the tile scale unless the surface is prop scale or mesh mapped
hz_internal f32 cod_detail_tiles(const CodMatParams *m) {
  if (m->uv_mode == COD_UV_MESH || !(m->detail_world > 0.0f) || m->scale < 0.3f)
    return m->detail.x;
  return m_maxf(1.2f, m->scale / m->detail_world);
}

// binds the textures that exist; the rest stay on the shader's fallbacks until their bake publishes
hz_internal void cod_material_textures(RendererState *r, HzHandleT(RendererMaterial) mat, const CodTextureSet *set, const CodMaterials *shared) {
  if (handle_is_valid(set->albedo)) material_set_texture(r, mat, "albedo", set->albedo);
  if (handle_is_valid(set->normal)) material_set_texture(r, mat, "normal_map", set->normal);
  if (handle_is_valid(set->orm)) material_set_texture(r, mat, "orm_map", set->orm);
  if (handle_is_valid(shared->detail)) material_set_texture(r, mat, "detail_arr", shared->detail);
  if (handle_is_valid(shared->macro)) material_set_texture(r, mat, "macro_tex", shared->macro);
}

// writes a resolved parameter set and its textures into a material instance
hz_internal void cod_material_apply(RendererState *r, HzHandleT(RendererMaterial) mat, const CodMatParams *m, const CodTextureSet *set,
                                    const CodMaterials *shared, f32 ground_y) {
  f32 tile = m->uv_mode == COD_UV_MESH ? m->scale : 1.0f / m->scale;
  material_set_vec4(r, mat, "tile", (v4){tile, tile, 0.0f, 0.0f});
  material_set_vec4(r, mat, "detail_p", (v4){cod_detail_tiles(m), m->detail.y, m->detail.z, m->detail.w});
  material_set_vec4(r, mat, "macro_p", m->macro);
  material_set_vec4(r, mat, "macro_big", m->macro_big);
  material_set_vec4(r, mat, "patch_p", m->patch);
  material_set_vec4(r, mat, "cloth_p", m->cloth);
  material_set_vec4(r, mat, "parallax_p", (v4){m->parallax, m->parallax_fade_start, m->parallax_fade_end, m->parallax_layers});
  material_set_vec4(r, mat, "weather_p", m->weather);
  material_set_vec4(r, mat, "wear_p", m->wear);
  material_set_vec4(r, mat, "tint_col", m->tint);
  material_set_vec4(r, mat, "dust_col", m->dust_color);
  material_set_vec4(r, mat, "grime_col", m->grime_color);
  material_set_vec4(r, mat, "rust_col", m->rust_color);
  material_set_vec4(r, mat, "wear_col", m->wear_color);
  material_set_vec4(r, mat, "wear_mat", m->wear_material);
  material_set_vec4(r, mat, "rough_p", (v4){m->roughness.x, m->roughness.y, m->detile, m->roughness.z});
  material_set_vec4(r, mat, "misc_a", (v4){m->normal_strength, ground_y, m->ao_strength, m->macro_relief});
  material_set_vec4(r, mat, "misc_b", (v4){(f32)m->vertex_masks, m->opacity, m->env_intensity, 0.0f});
  material_set_vec4(r, mat, "emissive_col", m->emissive);
  cod_material_textures(r, mat, set, shared);
}

// the slot holding the texture set of a resolved bake: the library's when identical, else a variant bake
hz_internal i32 cod_bake_slot_for(CodMatState *s, const CodResolved *res) {
  CodSurfaceDef lib;
  cod_library(res->id, &lib);
  lib.bake.size = cod_bake_size_for(lib.bake.size, s->quality);
  if (mem_cmp(&lib.bake, &res->bake, sizeof(CodBakeDef)) == 0)
    return (i32)res->id;
  CodBakeState *b = &s->bake;
  for (u32 i = 0; i < b->variant_count; i++) {
    CodBakeSlot *k = &b->slot[COD_SLOT_FIRST_VARIANT + i];
    if (mem_cmp(&k->def, &res->bake, sizeof(CodBakeDef)) == 0)
      return (i32)(COD_SLOT_FIRST_VARIANT + i);
  }
  if (b->variant_count >= COD_BAKE_VARIANTS) {
    log_error("materials: more than % distinct bakes, using the library bake of %", fmt_u32(COD_BAKE_VARIANTS), fmt_cstr(lib.name));
    return (i32)res->id;
  }
  u32 index = COD_SLOT_FIRST_VARIANT + b->variant_count++;
  cod_bake_slot_make(b, index, &res->bake, false, true, "cod variant");
  return (i32)index;
}

HzHandleT(RendererMaterial) cod_materials_get(EcsWorld *w, const CodMaterialDesc *desc) {
  CodMatState *s = ecs_resource_get_mut(w, CodMatState);
  CodMaterials *shared = ecs_resource_get_mut(w, CodMaterials);
  RendererState *r = ecs_resource_get_mut(w, RendererState);
  CodResolved res;
  cod_resolve(desc, s->quality, &res);

  for (u32 i = 0; i < s->cache_count; i++) {
    if (mem_cmp(&s->cache[i].key, &res, sizeof(CodResolved)) == 0)
      return s->cache[i].material;
  }
  if (s->cache_count >= COD_MATERIALS_MAX) {
    log_error("materials: more than % distinct materials requested", fmt_u32(COD_MATERIALS_MAX));
    return (HzHandleT(RendererMaterial)){0};
  }

  u32 key = cod_variant_key(&res.mat);
  if (!cod_variant_file(key)) {
    log_error("materials: no cod_surface variant for key % (surface %), using the plain planar one", fmt_u32(key), fmt_u32(res.id));
    key = COD_UV_PLANAR;
  }
  if (!handle_is_valid(s->base[key])) {
    if (!s->warned_not_ready) {
      s->warned_not_ready = true;
      log_error("materials: cod_materials_get before the surface materials loaded");
    }
    return (HzHandleT(RendererMaterial)){0};
  }
  i32 slot = cod_bake_slot_for(s, &res);
  HzHandleT(RendererMaterial) mat = renderer_create_material_instance(r, s->base[key]);
  cod_material_apply(r, mat, &res.mat, &s->bake.slot[slot].set, shared, s->ground_y);

  CodMatCacheEntry *e = &s->cache[s->cache_count++];
  e->key = res;
  e->material = mat;
  e->slot = (u32)slot;
  return mat;
}

void cod_materials_set_emissive(EcsWorld *w, HzHandleT(RendererMaterial) material, u32 emissive, f32 intensity) {
  // main lane: material writes go to the renderer's main-lane material store
  assert(is_main_thread());
  RendererState *r = ecs_resource_get_mut(w, RendererState);
  if (handle_is_valid(HANDLE_CAST(HzHandle, material)))
    material_set_vec4(r, material, "emissive_col", cod_emissive_radiance(emissive, intensity));
}

void cod_materials_set_ground_level(EcsWorld *w, f32 y) {
  CodMatState *s = ecs_resource_get_mut(w, CodMatState);
  RendererState *r = ecs_resource_get_mut(w, RendererState);
  if (s->ground_y == y)
    return;
  s->ground_y = y;
  for (u32 i = 0; i < s->cache_count; i++) {
    const CodMatParams *m = &s->cache[i].key.mat;
    material_set_vec4(r, s->cache[i].material, "misc_a", (v4){m->normal_strength, y, m->ao_strength, m->macro_relief});
  }
}

void cod_materials_init(EcsWorld *w) {
  ecs_resource_set_zero(w, CodMaterials);
  ecs_resource_set_zero(w, CodMatState);
  CodMatState *s = ecs_resource_get_mut(w, CodMatState);
  s->quality = ecs_resource_get(w, CodSettings)->quality;
  CodBakeState *b = &s->bake;
  b->alloc = make_arena_allocator(arena_create(GB(1), MB(8)));
  cod_bake_pipelines(b);

  // the two shared maps first (every material samples them), then the library in order
  CodBakeDef detail;
  cod_bake_default(&detail, COD_PROG_DETAIL, 0.25f, 0.0034f, 1, cod_bake_size_for(1024, s->quality));
  cod_bake_slot_make(b, COD_SLOT_DETAIL, &detail, true, true, "cod detail");
  CodBakeDef macro;
  cod_bake_default(&macro, COD_PROG_MACRO, 32.0f, 0.5f, 2, cod_bake_size_for(256, s->quality));
  cod_bake_slot_make(b, COD_SLOT_MACRO, &macro, true, false, "cod macro");
  for (u32 id = 0; id < COD_MAT_COUNT; id++) {
    CodSurfaceDef d;
    cod_library(id, &d);
    d.bake.size = cod_bake_size_for(d.bake.size, s->quality);
    cod_bake_slot_make(b, id, &d.bake, false, true, d.name);
  }
  b->library_total = COD_MAT_COUNT + 2;

  AssetSystem *assets = ecs_resource_get_mut(w, AssetSystem);
  for (u32 key = 0; key < COD_MATERIAL_VARIANT_COUNT; key++) {
    const char *file = cod_variant_file(key);
    if (!file)
      continue;
    s->asset[key] = asset_load(MaterialBlobAsset, assets, .path = file);
    s->variants_total++;
  }
}

void cod_materials_schedule(EcsWorld *w) {
  ecs_schedule_single(w, cod_materials_load_system); // writes the CodLoad resource
}

void cod_materials_frame(EcsWorld *w) {
  CodMatState *s = ecs_resource_get_mut(w, CodMatState);
  CodMaterials *m = ecs_resource_get_mut(w, CodMaterials);
  CodBakeState *b = &s->bake;
  RendererState *r = ecs_resource_get_mut(w, RendererState);
  AssetSystem *assets = ecs_resource_get_mut(w, AssetSystem);

  // the base materials, as their assets land
  for (u32 key = 0; key < COD_MATERIAL_VARIANT_COUNT; key++) {
    if (!cod_variant_file(key) || handle_is_valid(s->base[key]) || !asset_is_ready(assets, s->asset[key]))
      continue;
    s->base[key] = renderer_create_material_from_asset(r, assets, asset_get(assets, s->asset[key], MaterialBlobAsset));
    s->variants_ready++;
  }

  // the bake queue: in order, as their pipelines compile, within the frame's texel budget
  u32 texels = 0;
  u32 published = 0;
  cod_bake_publish(b, &published, cod_slot_name);
  while (b->queue_head != b->queue_tail && texels < COD_BAKE_FRAME_TEXELS) {
    u32 index = b->queue[b->queue_head % COD_SLOT_COUNT];
    CodBakeSlot *k = &b->slot[index];
    if (cod_bake_slot_failed(b, k)) {
      log_error("materials: the bake pipelines of % failed to compile", fmt_cstr(cod_slot_name(index)));
      k->state = COD_SLOT_FAILED;
    } else if (cod_bake_slot_ready(b, k)) {
      if (!b->first_record)
        b->first_record = os_time_now();
      if (!cod_bake_record(b, k, cod_slot_name(index)))
        break;
      b->last_record = os_time_now();
      b->cpu_ms_total += k->record_ms;
      texels += k->def.size * k->def.size;
    } else {
      break;
    }
    b->queue_head++;
  }
  cod_bake_collect_timers(b, cod_slot_name);

  if (published) {
    m->detail = b->slot[COD_SLOT_DETAIL].layers;
    m->macro = b->slot[COD_SLOT_MACRO].set.albedo;
    for (u32 id = 0; id < COD_MAT_COUNT; id++)
      m->sets[id] = b->slot[id].set;
    for (u32 i = 0; i < s->cache_count; i++)
      cod_material_textures(r, s->cache[i].material, &b->slot[s->cache[i].slot].set, m);
  }

  m->progress = (f32)b->library_baked / (f32)b->library_total;
  b32 done = b->library_baked == b->library_total && s->variants_ready == s->variants_total;
  if (done && !m->ready) {
    m->ready = true;
    m->bake_ms = os_ticks_to_ms(os_time_diff(b->last_record, b->first_record));
    log_info("materials: % surfaces and shared maps recorded in % ms (cpu recording % ms)", fmt_u32(b->library_total),
             fmt_f32_p((f32)m->bake_ms, 1), fmt_f32_p((f32)b->cpu_ms_total, 1));
  }
}

void cod_materials_lanes(EcsWorld *w) { (void)w; }
