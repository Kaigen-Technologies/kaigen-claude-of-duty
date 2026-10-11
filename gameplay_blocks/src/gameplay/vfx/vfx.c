#include "vfx.h"
#include "lib/array.h"
#include "lib/memory.h"
#include "lib/random.h"
#include "os/os.h"
#include "rendering/renderer.h"
#include "rendering/features/clustered_lights.h"
#include "rendering/features/decals.h"
#include "shaders/vfx_default_material.h"
#include "shaders/vfx_ambient_material.h"

#define VFX_ROW_VEC4S 20u
#define VFX_TABLE_ELEMS 128u
#define VFX_TABLES_MAX 32u
#define VFX_BEAM_SLOTS_DEFAULT 8u
#define VFX_DECAL_SLOTS_DEFAULT 32u
#define VFX_LIGHTS_DEFAULT 16u
#define VFX_REQUESTS_DEFAULT 4096u
#define VFX_LOOP_POINTS 8u
#define VFX_AMBIENT_MAX 4u
#define VFX_TEXCACHE_MAX 128
#define VFX_MATCACHE_MAX 128

typedef struct VfxTable {
  GpuTexture tex;
  HzHandleT(ParticleMaterial) mat;
  u32 rows;
  b32 lit;
} VfxTable;

typedef struct VfxElemRt {
  HzHandleT(ParticleEmitter) emitter;
  b32 has_emitter;
  quat rotation; // the element's authored cone orientation
  u32 table;
  struct VfxBeam *beams;
  u32 beam_count;
  struct VfxDecalSlot *decals;
  u32 decal_count;
  u32 decal_next;
} VfxElemRt;

typedef struct VfxBeam {
  HzHandleT(Trail) trail;
  b32 live;
  b32 owned; // held by a loop until it stops
  f32 age, life;
  v3 from, to;
  f32 width;
  f32 width_a, width_b;
  f32 fade;
  f32 length, speed;
  ColorF32 color;
} VfxBeam;

typedef struct VfxDecalSlot {
  RendererDecalHandle h;
  b32 created, live;
  f32 age;
  RendererDecalDesc base;
  ColorF32 tint;
  f32 emission;
  f32 heat_decay, fade_start, fade_end;
  f32 last_alpha;
} VfxDecalSlot;

typedef struct VfxLight {
  RendererLocalLightHandle h;
  b32 created, live;
  f32 age, life, curve;
  v3 pos;
  v3 hue;
  f32 candela, radius;
} VfxLight;

struct VfxAsset {
  VfxDesc desc;
  VfxElemRt rt[VFX_ELEM_MAX];
};
HANDLE_ARRAY_TYPEDEF(VfxAsset);

typedef struct VfxLoopPoint {
  v3 at, normal;
  f32 alpha;
  b32 cut;
} VfxLoopPoint;

struct VfxLoop {
  HzHandleT(VfxAsset) asset;
  v3 at, to;
  b32 has_target;
  VfxPlayDesc params;
  f32 rate_scale;
  f32 carry[VFX_ELEM_MAX];
  HzHandleT(Trail) strip[VFX_ELEM_MAX];
  VfxBeam *beam[VFX_ELEM_MAX];
  VfxLoopPoint points[VFX_LOOP_POINTS];
  u32 point_count;
  b32 stopped; // released and removed by the next vfx_world_frame (trail edits are main-lane)
};
HANDLE_ARRAY_TYPEDEF(VfxLoop);

typedef struct VfxPending {
  f32 t;
  HzHandleT(VfxAsset) asset;
  u32 elem;
  u32 seq;
  VfxPlayDesc play;
} VfxPending;

typedef struct VfxDying {
  HzHandleT(Trail) trail;
  f32 remaining;
} VfxDying;

typedef struct VfxAmbientRt {
  b32 live;
  HzHandleT(ParticleMaterial) mat;
  HzHandleT(ParticleEmitter) emitter;
  VfxAmbientDesc desc;
  VfxAmbientLook look;
} VfxAmbientRt;

struct VfxWorld {
  ParticleSystem *ps;
  RendererState *r;
  TrailSystem *trails;
  ArenaAllocator *arena;
  Allocator alloc;
  HandleArray(VfxAsset) assets;
  HandleArray(VfxLoop) loops;
  u32 max_loops;
  u32 ppm;
  VfxTable tables[VFX_TABLES_MAX];
  u32 table_count;
  VfxLight *lights;
  u32 light_count;
  VfxPending *pending;
  u32 pending_len, pending_cap;
  VfxDying *dying;
  u32 dying_len, dying_cap;
  u32 active_slots; // live beams and decals, so a frame with none skips the walk
  f32 light_unit;
  VfxAtmosphere atmosphere;
  v3 camera_pos;
  u32 seq;
  VfxAmbientRt ambient[VFX_AMBIENT_MAX];
  VfxRequests requests;
  VfxRequestQueue queue;
  // one GpuTexture per distinct texture asset: a pack's effects share a handful of sprites
  struct { HzHandle asset; GpuTexture tex; } texcache[VFX_TEXCACHE_MAX];
  u32 texcache_count;
  // one live material per distinct particle material asset
  struct { HzHandle asset; HzHandleT(ParticleMaterial) mat; } matcache[VFX_MATCACHE_MAX];
  u32 matcache_count;
  volatile u32 guard;
};

/* ----------------------------------------------------------------- world */

VfxWorld *vfx_world_create(const VfxWorldDesc *desc) {
  assert_msg(desc && desc->particles, "vfx_world_create: VfxWorldDesc.particles is required");
  ArenaAllocator *arena = arena_create(MB(64), KB(256));
  arena_set_name(arena, "vfx");
  Allocator alloc = make_arena_allocator(arena);
  VfxWorld *w = ALLOC(&alloc, VfxWorld);
  w->arena = arena;
  w->alloc = alloc;
  w->ps = desc->particles;
  w->r = desc->renderer;
  w->trails = desc->trails;
  w->max_loops = desc->max_loops ? desc->max_loops : 32;
  w->ppm = desc->particles_per_material ? desc->particles_per_material : 8192;
  w->light_count = desc->max_lights ? desc->max_lights : VFX_LIGHTS_DEFAULT;
  w->lights = ALLOC_ARRAY(&w->alloc, VfxLight, w->light_count);
  w->pending_cap = desc->max_requests ? desc->max_requests : VFX_REQUESTS_DEFAULT;
  w->pending = ALLOC_ARRAY(&w->alloc, VfxPending, w->pending_cap);
  w->dying_cap = w->max_loops * 4u;
  w->dying = ALLOC_ARRAY(&w->alloc, VfxDying, w->dying_cap);
  w->light_unit = 1.0f;
  w->atmosphere = (VfxAtmosphere){.sun_dir = V3_UP, .sun_color = {1, 1, 1}, .ambient = {0.5f, 0.5f, 0.5f},
                                  .fog_color = {0.6f, 0.65f, 0.7f}};
  ha_init_with_max_capacity(&w->assets, &w->alloc, 8, 1024);
  ha_init_with_max_capacity(&w->loops, &w->alloc, 8, w->max_loops);
  w->queue.q = concurrent_arr_new_alloc(&w->alloc, VfxRequest, w->pending_cap);
  w->queue.world = w;
  w->requests.queue = &w->queue;
  return w;
}

void vfx_world_destroy(VfxWorld *w) {
  if (!w) return;
  if (w->r) {
    for (u32 i = 0; i < w->light_count; i++)
      if (w->lights[i].created) renderer_destroy_local_light(w->r, w->lights[i].h);
  }
  ha_foreach(&w->assets, VfxAsset, a) {
    for (u32 i = 0; i < a->desc.count; i++) {
      VfxElemRt *rt = &a->rt[i];
      for (u32 k = 0; k < rt->beam_count; k++)
        if (w->trails) trail_destroy(w->trails, rt->beams[k].trail);
      for (u32 k = 0; k < rt->decal_count; k++)
        if (w->r && rt->decals[k].created) renderer_destroy_decal(w->r, rt->decals[k].h);
    }
  }
  for (u32 i = 0; i < w->dying_len; i++)
    if (w->trails) trail_destroy(w->trails, w->dying[i].trail);
  for (u32 i = 0; i < VFX_AMBIENT_MAX; i++)
    if (w->ambient[i].live) particle_material_destroy(w->ps, w->ambient[i].mat);
  arena_destroy(w->arena);
}

void vfx_init(EcsWorld *w, const VfxWorldDesc *desc) {
  // the ECS world outside a scope and the particle system are main-lane state
  assert(is_main_thread());
  VfxWorld *vw = vfx_world_create(desc);
  ecs_resource_set_ref(w, VfxRequests, &vw->requests);
}

VfxWorld *vfx_world(EcsWorld *w) {
  const VfxRequests *rq = ecs_resource_get(w, VfxRequests);
  return rq ? rq->queue->world : NULL;
}

void vfx_set_light_unit(VfxWorld *w, f32 candela_per_unit) { w->light_unit = candela_per_unit; }

hz_internal void vfx_table_write_atmosphere(VfxWorld *w, VfxTable *t) {
  ParticleSystem *ps = w->ps;
  const VfxAtmosphere *a = &w->atmosphere;
  particle_material_set_vec3(ps, t->mat, "sun_dir", a->sun_dir);
  particle_material_set_vec3(ps, t->mat, "sun_color", a->sun_color);
  particle_material_set_vec3(ps, t->mat, "ambient", a->ambient);
  particle_material_set_vec3(ps, t->mat, "fog_color", a->fog_color);
  particle_material_set_vec3(ps, t->mat, "fog_sun_color", a->fog_sun_color);
  particle_material_set_vec4(ps, t->mat, "fog", (v4){a->fog_density, a->fog_height_falloff, 0, 0});
}

void vfx_set_atmosphere(VfxWorld *w, const VfxAtmosphere *atmosphere) {
  w->atmosphere = *atmosphere;
  for (u32 i = 0; i < w->table_count; i++)
    if (w->tables[i].lit) vfx_table_write_atmosphere(w, &w->tables[i]);
}

/* ----------------------------------------------------------- table material */

hz_internal u32 vfx_pool_for(const VfxElemDesc *e, u32 loops) {
  if (e->pool) return e->pool;
  f32 need = e->count + e->rate * e->life.y * 1.1f * (f32)(loops ? loops : 1);
  return (u32)(need * 1.35f) + 4u;
}

// a table material for `tex` with a free row and room for `pool` more particles; a new one when none has
hz_internal VfxTable *vfx_table_for(VfxWorld *w, GpuTexture tex, u32 pool) {
  HzHandle key = HANDLE_CAST(HzHandle, tex);
  for (u32 i = 0; i < w->table_count; i++) {
    VfxTable *t = &w->tables[i];
    if (!handle_equals(HANDLE_CAST(HzHandle, t->tex), key) || t->rows >= VFX_TABLE_ELEMS) continue;
    if (particle_material_can_fit(w->ps, t->mat, pool)) return t;
  }
  if (w->table_count >= VFX_TABLES_MAX) {
    log_error("vfx: % table materials are full (% rows each), the element is dropped", fmt_u32(VFX_TABLES_MAX),
              fmt_u32(VFX_TABLE_ELEMS));
    return NULL;
  }
  ParticleMaterialDesc *d = shader_vfx_default_particle_material_desc();
  particle_material_desc_billboard_defaults(d);
  d->max_particles = pool > w->ppm ? pool : w->ppm;
  VfxTable *t = &w->tables[w->table_count++];
  *t = (VfxTable){.tex = tex, .mat = particle_material_create(w->ps, d)};
  if (handle_is_valid(HANDLE_CAST(HzHandle, tex))) particle_material_set_texture(w->ps, t->mat, "albedo", tex);
  vfx_table_write_atmosphere(w, t);
  return t;
}

hz_internal void vfx_row_set(VfxWorld *w, VfxTable *t, u32 row, u32 k, v4 v) {
  particle_material_set_vec4_idx(w->ps, t->mat, "table", row * VFX_ROW_VEC4S + k, v);
}

hz_internal void vfx_table_write_row(VfxWorld *w, VfxTable *t, u32 row, const VfxElemDesc *e) {
  u32 stops = e->stops < VFX_STOP_MAX ? e->stops : VFX_STOP_MAX;
  f32 times[VFX_STOP_MAX] = {0};
  for (u32 i = 0; i < stops; i++) {
    ColorF32 c = e->color[i].color;
    vfx_row_set(w, t, row, i, (v4){c.r, c.g, c.b, c.a});
    times[i] = m_clampf(e->color[i].t, 0.0f, 1.0f);
  }
  vfx_row_set(w, t, row, 8, (v4){times[0], times[1], times[2], times[3]});
  vfx_row_set(w, t, row, 9, (v4){times[4], times[5], times[6], times[7]});
  vfx_row_set(w, t, row, 10, (v4){e->gravity.x, e->gravity.y, e->gravity.z, e->drag > 0.0f ? e->drag : 1.0f});
  // the table takes the width of each draw; an unset max keeps that end exact
  vfx_row_set(w, t, row, 11, (v4){e->size.x, e->size.y, e->size_max.x != 0.0f ? e->size_max.x - e->size.x : 0.0f,
                                  e->size_max.y != 0.0f ? e->size_max.y - e->size.y : 0.0f});
  vfx_row_set(w, t, row, 12, (v4){e->size_linked ? 1.0f : 0.0f, e->size_ease, e->additive, (f32)stops});
  vfx_row_set(w, t, row, 13, (v4){e->plane_right.x, e->plane_right.y, e->plane_right.z, e->velocity_align});
  vfx_row_set(w, t, row, 14, (v4){e->plane_up.x, e->plane_up.y, e->plane_up.z, e->velocity_stretch});
  // spin is authored as a range; the table takes centre + half-width
  vfx_row_set(w, t, row, 15, (v4){(e->spin.x + e->spin.y) * 0.5f, (e->spin.y - e->spin.x) * 0.5f,
                                  (e->rotation.x + e->rotation.y) * 0.5f, (e->rotation.y - e->rotation.x) * 0.5f});
  vfx_row_set(w, t, row, 16, (v4){e->z_offset, e->soft_fade, e->collide ? 1.0f : 0.0f, e->restitution});
  vfx_row_set(w, t, row, 17, (v4){e->friction, e->damping, (f32)e->render, e->ring_width});
  vfx_row_set(w, t, row, 18, (v4){e->lit, e->emissive, e->ramp_tint ? 1.0f : 0.0f, 0});
  if (e->lit > 0.0f) t->lit = true;
}

/* -------------------------------------------------------------- registration */

hz_internal ParticleEmitterDesc vfx_emitter_desc(const VfxElemDesc *e, HzHandleT(ParticleMaterial) mat, u32 pool,
                                                 u32 user) {
  f32 life_mid = (e->life.x + e->life.y) * 0.5f;
  if (life_mid <= 0.0f) life_mid = 1.0f;
  f32 life_rand = (e->life.y - e->life.x) / life_mid;
  b32 arc = e->shape == (u32)PARTICLE_SHAPE_ARC;
  quat rotation;
  if (arc) {
    v3 flat = {e->dir.x, 0.0f, e->dir.z};
    rotation = v3_dot(flat, flat) > 1e-8f ? quat_rotate_y(m_atan2f(flat.x, flat.z)) : quat_identity();
  } else {
    v3 dir = v3_dot(e->dir, e->dir) > 1e-8f ? v3_normalize(e->dir) : V3_UP;
    rotation = quat_from_to(V3_FORWARD, dir);
  }
  return (ParticleEmitterDesc){
      .max_particles = pool,
      .life = life_mid,
      .life_randomness = m_clampf(life_rand, 0.0f, 1.0f),
      .size_begin = e->size.x,
      .size_end = e->size.y,
      .size_begin_max = e->size_max.x,
      .size_end_max = e->size_max.y,
      .size_linked = e->size_linked,
      .velocity = {0.0f, (e->vspeed.x + e->vspeed.y) * 0.5f, 0.0f},
      .velocity_random = {e->scatter * 2.0f, e->vspeed.y - e->vspeed.x, e->scatter * 2.0f},
      .gravity = e->gravity,
      .drag = e->drag > 0.0f ? e->drag : 1.0f,
      .shape = e->shape ? (ParticleShapeType)e->shape : PARTICLE_SHAPE_CONE,
      .shape_scale = {RADIANS((e->spread_deg.x + e->spread_deg.y) * 0.5f), 0, 0},
      .shape_radius = e->radius,
      .start_speed = (e->speed.x + e->speed.y) * 0.5f,
      .start_speed_random = (e->speed.y - e->speed.x) * 0.5f,
      .rotation = rotation,
      .uniform_distribution = e->uniform_angle,
      .spawn_offset = {0.0f, 0.0f, -e->back},
      .spawn_height = e->height,
      .color = e->stops > 0 && !e->ramp_tint ? e->color[0].color : (ColorF32){1, 1, 1, 1},
      .material = mat,
      .user = user,
  };
}

hz_internal TrailDesc vfx_trail_desc(const VfxElemDesc *e, b32 beam) {
  const VfxTrailDesc *t = &e->trail;
  return (TrailDesc){
      .max_points = beam ? 2u : (t->max_points ? t->max_points : 64u),
      .min_distance = beam ? 1e-4f : (t->min_distance > 0.0f ? t->min_distance : 0.01f),
      .lifetime = beam ? 0.0f : t->life,
      .width = t->width,
      .color = t->color.a > 0.0f ? t->color : (ColorF32){1, 1, 1, 1},
      .orientation = t->orientation,
      .width_head = beam ? 1.0f : (t->width_scale.x > 0.0f ? t->width_scale.x : 1.0f),
      .width_tail = beam ? 1.0f : (t->width_scale.y > 0.0f ? t->width_scale.y : 1.0f),
      .fade_over_age = !beam && t->fade_over_age,
      .fade_hold = beam ? 0.0f : t->fade_hold,
      .uv_mode = !beam && t->uv_tile > 0.0f ? TRAIL_UV_TILE : TRAIL_UV_STRETCH,
      .uv_tile_length = beam ? 0.0f : t->uv_tile,
      .texture = e->tex,
      .additive = e->additive >= 0.5f,
      .multiply = t->multiply,
      .subdivision = 1,
  };
}

HzHandleT(VfxAsset) vfx_register(VfxWorld *w, const VfxDesc *desc) {
  assert_single_thread_begin("vfx_register");
  TempAllocator ta = tctx_temp_allocator_begin(NULL);
  VfxAsset *a = ALLOC(&ta.allocator, VfxAsset); // ~20 KB: off the stack (wasm frame limit)
  a->desc = *desc;
  if (a->desc.count > VFX_ELEM_MAX) {
    log_warn("vfx: '%' has % elements, VFX_ELEM_MAX is %; the tail is dropped", fmt_cstr(desc->name ? desc->name : "?"),
             fmt_u32(desc->count), fmt_u32(VFX_ELEM_MAX));
    a->desc.count = VFX_ELEM_MAX;
  }
  for (u32 i = 0; i < a->desc.count; i++) {
    const VfxElemDesc *e = &a->desc.elem[i];
    VfxElemRt *rt = &a->rt[i];
    rt->rotation = quat_identity();
    switch (e->kind) {
    case VFX_ELEM_PARTICLES: {
      if (e->count <= 0.0f && e->rate <= 0.0f) break;
      u32 pool = vfx_pool_for(e, desc->loops);
      HzHandleT(ParticleMaterial) mat = e->material;
      u32 user = e->user;
      if (!handle_is_valid(mat)) {
        VfxTable *t = vfx_table_for(w, e->tex, pool);
        if (!t) break;
        u32 row = t->rows++;
        vfx_table_write_row(w, t, row, e);
        mat = t->mat;
        user = row;
        rt->table = (u32)(t - w->tables);
      }
      ParticleEmitterDesc d = vfx_emitter_desc(e, mat, pool, user);
      rt->rotation = d.rotation;
      rt->emitter = particle_emitter_create(w->ps, &d);
      rt->has_emitter = true;
    } break;
    case VFX_ELEM_BEAM: {
      if (!w->trails) {
        log_error("vfx: '%' has a beam element but VfxWorldDesc.trails is NULL", fmt_cstr(desc->name ? desc->name : "?"));
        break;
      }
      u32 n = e->pool ? e->pool : VFX_BEAM_SLOTS_DEFAULT;
      rt->beams = ALLOC_ARRAY(&w->alloc, VfxBeam, n);
      TrailDesc td = vfx_trail_desc(e, true);
      for (u32 k = 0; k < n; k++) rt->beams[k].trail = trail_create(w->trails, &td);
      rt->beam_count = n;
    } break;
    case VFX_ELEM_DECAL: {
      if (!w->r) {
        log_error("vfx: '%' has a decal element but VfxWorldDesc.renderer is NULL", fmt_cstr(desc->name ? desc->name : "?"));
        break;
      }
      u32 n = e->pool ? e->pool : VFX_DECAL_SLOTS_DEFAULT;
      rt->decals = ALLOC_ARRAY(&w->alloc, VfxDecalSlot, n);
      rt->decal_count = n;
    } break;
    case VFX_ELEM_LIGHT:
      if (!w->r) log_error("vfx: '%' has a light element but VfxWorldDesc.renderer is NULL", fmt_cstr(desc->name ? desc->name : "?"));
      break;
    case VFX_ELEM_TRAIL:
      if (!w->trails) log_error("vfx: '%' has a trail element but VfxWorldDesc.trails is NULL", fmt_cstr(desc->name ? desc->name : "?"));
      break;
    }
  }
  HzHandleT(VfxAsset) h = ha_add_t(&w->assets, a, VfxAsset);
  tctx_temp_allocator_end(ta);
  assert_single_thread_end();
  return h;
}

hz_internal void vfx_loop_release(VfxWorld *w, VfxLoop *l);

void vfx_unregister(VfxWorld *w, HzHandleT(VfxAsset) h) {
  assert_single_thread_begin("vfx_unregister");
  VfxAsset *a = ha_get(&w->assets, h, VfxAsset);
  if (a) {
    for (u32 i = 0; i < w->loops.count;) {
      VfxLoop *l = &w->loops.items[i];
      if (!handle_equals(HANDLE_CAST(HzHandle, l->asset), HANDLE_CAST(HzHandle, h))) {
        i++;
        continue;
      }
      HzHandle lh = {.idx = w->loops.dense_to_sparse[i], .gen = w->loops.sparse[w->loops.dense_to_sparse[i]].gen};
      vfx_loop_release(w, l);
      ha_remove(&w->loops, lh);
    }
    for (u32 i = 0; i < a->desc.count; i++) {
      VfxElemRt *rt = &a->rt[i];
      if (rt->has_emitter) particle_emitter_destroy(w->ps, rt->emitter);
      for (u32 k = 0; k < rt->beam_count; k++) {
        if (rt->beams[k].live) w->active_slots--;
        trail_destroy(w->trails, rt->beams[k].trail);
      }
      for (u32 k = 0; k < rt->decal_count; k++) {
        if (rt->decals[k].live) w->active_slots--;
        if (rt->decals[k].created) renderer_destroy_decal(w->r, rt->decals[k].h);
      }
    }
    for (u32 i = 0; i < w->pending_len;) {
      if (handle_equals(HANDLE_CAST(HzHandle, w->pending[i].asset), HANDLE_CAST(HzHandle, h)))
        w->pending[i] = w->pending[--w->pending_len];
      else
        i++;
    }
    ha_remove(&w->assets, h);
  }
  assert_single_thread_end();
}

/* ------------------------------------------------------------------ blobs */

hz_internal GpuTexture vfx_texture_for(VfxWorld *w, const AssetSystem *assets, BlobAssetRef(TextureBlobAsset) *ref) {
  GpuTexture none = {0};
  if (!assets || !handle_is_valid(ref->handle)) return none;
  HzHandle key = HANDLE_CAST(HzHandle, ref->handle);
  for (u32 i = 0; i < w->texcache_count; i++)
    if (handle_equals(w->texcache[i].asset, key)) return w->texcache[i].tex;

  TextureBlobAsset *tb = blob_asset_ref_get(TextureBlobAsset, assets, ref);
  if (!tb) return none;
  u32 len = 0;
  u8 *img = blob_asset_ref_resolve_raw(assets, &tb->image, &len);
  if (!img) return none;
  GpuTexture tex = gpu_make_texture(&(GpuTextureDesc){
      .data = img,
      .size = len,
      .data_type = GPU_TEXTURE_DATA_FILE,
      .is_srgb = tb->is_srgb,
      .data_persistent = true,
  });
  if (w->texcache_count < VFX_TEXCACHE_MAX) {
    w->texcache[w->texcache_count].asset = key;
    w->texcache[w->texcache_count].tex = tex;
    w->texcache_count++;
  }
  return tex;
}

hz_internal HzHandleT(ParticleMaterial)
    vfx_material_for(VfxWorld *w, const AssetSystem *assets, BlobAssetRef(ParticleMaterialBlobAsset) *ref) {
  if (!assets || !handle_is_valid(ref->handle)) return (HzHandleT(ParticleMaterial)){0};
  HzHandle key = HANDLE_CAST(HzHandle, ref->handle);
  for (u32 i = 0; i < w->matcache_count; i++)
    if (handle_equals(w->matcache[i].asset, key)) return w->matcache[i].mat;
  ParticleMaterialBlobAsset *mb = blob_asset_ref_get(ParticleMaterialBlobAsset, assets, ref);
  if (!mb) {
    log_warn("vfx: particle material '%' is not loaded; the element falls back to the default",
             fmt_cstr(string_blob_get(&ref->path)));
    return (HzHandleT(ParticleMaterial)){0};
  }
  HzHandleT(ParticleMaterial) m = particle_material_create_from_asset(w->ps, assets, mb);
  if (w->matcache_count < VFX_MATCACHE_MAX) {
    w->matcache[w->matcache_count].asset = key;
    w->matcache[w->matcache_count].mat = m;
    w->matcache_count++;
  }
  return m;
}

HzHandleT(VfxAsset) vfx_register_from_blob(VfxWorld *w, const AssetSystem *assets, VfxBlobAsset *blob) {
  TempAllocator ta = tctx_temp_allocator_begin(NULL);
  VfxDesc *d = ALLOC(&ta.allocator, VfxDesc); // ~20 KB: off the stack (wasm frame limit)
  d->name = string_blob_get(&blob->name);
  Array(VfxElemBlob) els = blob_array_get(VfxElemBlob, &blob->elements);
  u32 n = els.len;
  if (n > VFX_ELEM_MAX) {
    log_warn("vfx: '%' authors % elements, VFX_ELEM_MAX is % -- dropping the tail", fmt_cstr(d->name ? d->name : "?"),
             fmt_u32(els.len), fmt_u32(VFX_ELEM_MAX));
    n = VFX_ELEM_MAX;
  }
  for (u32 i = 0; i < n; i++) {
    VfxElemBlob *e = &els.items[i];
    VfxElemDesc *o = &d->elem[i];
    o->kind = (VfxElemKind)e->kind;
    o->tex = vfx_texture_for(w, assets, &e->texture);
    o->additive = e->additive;
    o->delay = e->delay;
    o->count = e->count;
    o->rate = e->rate;
    o->life = e->life;
    o->speed = e->speed;
    o->size = e->size;
    o->size_ease = e->size_ease;
    o->size_max = e->size_max;
    o->size_linked = e->size_linked;
    o->dir = e->dir;
    o->spread_deg = e->spread_deg;
    o->gravity = e->gravity;
    o->drag = e->drag;
    o->radius = e->radius;
    o->plane_right = e->plane_right;
    o->plane_up = e->plane_up;
    o->velocity_align = e->velocity_align;
    o->spin = e->spin;
    o->rotation = e->rotation;
    o->z_offset = e->z_offset;
    o->shape = e->shape;
    o->pool = e->pool;
    o->emissive = e->emissive;
    o->velocity_stretch = e->velocity_stretch;
    o->soft_fade = e->soft_fade;
    o->collide = e->collide;
    o->restitution = e->restitution;
    o->friction = e->friction;
    o->render = (VfxRender)e->render;
    o->ring_width = e->ring_width;
    o->lit = e->lit;
    o->damping = e->damping;
    o->vspeed = e->vspeed;
    o->height = e->height;
    o->back = e->back;
    o->scatter = e->scatter;
    o->uniform_angle = e->uniform_angle;
    o->scale_mask = e->scale_mask;
    o->light = (VfxLightDesc){.color = e->light_color, .intensity = e->light_intensity, .radius = e->light_radius,
                              .life = e->light_life, .curve = e->light_curve, .offset = e->light_offset};
    o->trail = (VfxTrailDesc){.width = e->trail_width, .width_scale = e->trail_width_scale, .color = e->trail_color,
                              .life = e->trail_life, .fade = e->trail_fade, .length = e->trail_length,
                              .speed = e->trail_speed, .min_distance = e->trail_min_distance,
                              .max_points = e->trail_max_points,
                              .orientation = (TrailOrientation)e->trail_orientation,
                              .fade_over_age = e->trail_fade_over_age};
    o->material = vfx_material_for(w, assets, &e->material);
    Array(VfxStopBlob) st = blob_array_get(VfxStopBlob, &e->color);
    u32 sn = st.len < VFX_STOP_MAX ? st.len : VFX_STOP_MAX;
    for (u32 k = 0; k < sn; k++) {
      o->color[k].t = st.items[k].t;
      o->color[k].color = st.items[k].color;
    }
    o->stops = sn;
  }
  d->count = n;
  HzHandleT(VfxAsset) h = vfx_register(w, d);
  tctx_temp_allocator_end(ta);
  return h;
}

/* ------------------------------------------------------------------- plays */

hz_internal u32 vfx_hash(u32 x) {
  x ^= x >> 16;
  x *= 0x7feb352dU;
  x ^= x >> 15;
  x *= 0x846ca68bU;
  x ^= x >> 16;
  return x;
}

hz_internal f32 vfx_unit_f32(u32 seed) { return (f32)(vfx_hash(seed) >> 8) * (1.0f / 16777216.0f); }

hz_internal f32 vfx_or_one(f32 v) { return v > 0.0f ? v : 1.0f; }

hz_internal void vfx_emit_particles(VfxWorld *w, VfxAsset *a, u32 i, const VfxPlayDesc *play, v3 at, u32 count) {
  const VfxElemDesc *e = &a->desc.elem[i];
  const VfxElemRt *rt = &a->rt[i];
  if (!rt->has_emitter || count == 0) return;
  b32 turned = v3_dot(play->dir, play->dir) > 1e-8f;
  quat rot = rt->rotation;
  if (turned) rot = quat_mul(quat_from_to(V3_UP, v3_normalize(play->dir)), rot);
  if (play->yaw != 0.0f) rot = quat_mul(quat_rotate_y(play->yaw), rot);
  u32 mask = e->scale_mask ? e->scale_mask : (VFX_SCALE_SPEED | VFX_SCALE_SIZE | VFX_SCALE_HEIGHT | VFX_SCALE_RADIUS);
  f32 k = vfx_or_one(play->scale);
  particle_emitter_burst_at(w->ps, rt->emitter, &(ParticleBurstDesc){
      .count = count,
      .position = at,
      .rotation = rot,
      .color = play->color,
      .delay = e->delay,
      .user = play->user,
      .speed_scale = ((mask & VFX_SCALE_SPEED) ? k : 1.0f) * vfx_or_one(play->speed_scale),
      .size_scale = ((mask & VFX_SCALE_SIZE) ? k : 1.0f) * vfx_or_one(play->size_scale),
      .height_scale = ((mask & VFX_SCALE_HEIGHT) ? k : 1.0f) * vfx_or_one(play->height_scale),
      .radius_scale = ((mask & VFX_SCALE_RADIUS) ? k : 1.0f) * vfx_or_one(play->radius_scale),
  });
}

hz_internal u32 vfx_budgeted(VfxWorld *w, f32 n) {
  f32 v = n * particle_system_budget(w->ps);
  return v <= 0.0f ? 0u : (u32)(v + 0.5f);
}

hz_internal void vfx_spawn_light(VfxWorld *w, const VfxElemDesc *e, const VfxPlayDesc *play, v3 at) {
  if (!w->r) return;
  const VfxLightDesc *l = &e->light;
  VfxLight *slot = NULL;
  for (u32 i = 0; i < w->light_count && !slot; i++)
    if (!w->lights[i].live) slot = &w->lights[i];
  if (!slot) {
    // every light is lit: take the one furthest through its life
    f32 best = -1.0f;
    for (u32 i = 0; i < w->light_count; i++) {
      f32 through = w->lights[i].age / w->lights[i].life;
      if (through > best) {
        best = through;
        slot = &w->lights[i];
      }
    }
  }
  if (!slot->created) {
    slot->h = renderer_create_local_light(w->r, &(RendererLocalLightDesc){.radius = 1.0f, .color = {1, 1, 1}});
    slot->created = true;
  }
  slot->hue = l->color;
  f32 k = vfx_or_one(play->scale);
  slot->candela = l->intensity * w->light_unit * vfx_or_one(play->intensity_scale);
  slot->radius = l->radius * k;
  slot->life = l->life > 0.0f ? l->life : 0.2f;
  slot->curve = l->curve > 0.0f ? l->curve : 2.0f;
  slot->pos = v3_add(at, v3_scale(l->offset, k));
  slot->age = 0.0f;
  slot->live = true;
  renderer_set_local_light(w->r, slot->h, &(RendererLocalLightDesc){.position = slot->pos, .radius = slot->radius,
                                                                     .color = slot->hue, .intensity = slot->candela});
  renderer_set_local_light_visible(w->r, slot->h, true);
}

hz_internal VfxBeam *vfx_beam_take(VfxWorld *w, VfxElemRt *rt) {
  VfxBeam *slot = NULL;
  for (u32 i = 0; i < rt->beam_count && !slot; i++)
    if (!rt->beams[i].live && !rt->beams[i].owned) slot = &rt->beams[i];
  if (!slot) {
    // all in flight: the oldest unowned one gives way
    f32 best = -1.0f;
    for (u32 i = 0; i < rt->beam_count; i++) {
      if (rt->beams[i].owned) continue;
      if (rt->beams[i].age > best) {
        best = rt->beams[i].age;
        slot = &rt->beams[i];
      }
    }
    if (slot) w->active_slots--;
  }
  return slot;
}

hz_internal void vfx_beam_setup(VfxBeam *b, const VfxElemDesc *e, const VfxPlayDesc *play, v3 from, v3 to) {
  const VfxTrailDesc *t = &e->trail;
  f32 k = vfx_or_one(play->scale);
  b->from = from;
  b->to = to;
  b->width = t->width * k;
  b->width_a = t->width_scale.x > 0.0f ? t->width_scale.x : 1.0f;
  b->width_b = t->width_scale.y > 0.0f ? t->width_scale.y : 1.0f;
  b->fade = t->fade;
  b->length = t->length;
  b->speed = t->speed;
  b->color = play->color.a > 0.0f ? play->color : (t->color.a > 0.0f ? t->color : (ColorF32){1, 1, 1, 1});
  f32 dist = v3_length(v3_sub(to, from));
  f32 streak = t->length > 0.0f ? m_minf(t->length, dist) : 0.0f;
  b->life = t->life > 0.0f ? t->life : (t->speed > 0.0f ? (dist + streak) / t->speed : 0.1f);
  b->age = 0.0f;
}

hz_internal void vfx_spawn_beam(VfxWorld *w, VfxElemRt *rt, const VfxElemDesc *e, const VfxPlayDesc *play, v3 at) {
  if (!rt->beam_count) return;
  VfxBeam *b = vfx_beam_take(w, rt);
  if (!b) return;
  vfx_beam_setup(b, e, play, at, play->to);
  b->live = true;
  w->active_slots++;
}

hz_internal void vfx_spawn_decal(VfxWorld *w, VfxElemRt *rt, const VfxElemDesc *e, const VfxPlayDesc *play, v3 at, u32 seq) {
  if (!rt->decal_count) return;
  const VfxDecalDesc *d = &e->decal;
  VfxDecalSlot *s = &rt->decals[rt->decal_next];
  rt->decal_next = (rt->decal_next + 1) % rt->decal_count;
  f32 k = vfx_or_one(play->scale);
  v3 normal = v3_dot(play->dir, play->dir) > 1e-8f ? v3_normalize(play->dir) : V3_UP;
  // one roll and one variant per play, so the elements of an effect stack on each other
  f32 roll = d->roll ? vfx_unit_f32(seq * 2654435761u + 1u) * 2.0f * M_PI : 0.0f;
  u32 variant = d->variants > 1 ? vfx_hash(seq * 2246822519u + 7u) % d->variants : 0u;
  f32 size = d->size.x + (d->size.y - d->size.x) * vfx_unit_f32(seq * 3266489917u + 3u);
  size *= k;
  f32 depth = d->depth > 0.0f ? d->depth : 0.5f;
  s->base = (RendererDecalDesc){
      .position = at,
      .rotation = quat_mul(decal_orient_from_hit(normal, V3_UP), quat_rotate_z(roll)),
      .size = {size, size, depth},
      .albedo = d->variants ? d->albedo[variant] : (HzHandleT(TextureBlobAsset)){0},
      .color = d->color,
      .emission_intensity = d->emission * w->light_unit * vfx_or_one(play->intensity_scale),
      .fade_upper = 1.0f,
      .fade_lower = 1.0f,
      .layers = DECAL_LAYERS_ALL,
      .priority = d->priority,
  };
  if (s->base.color.a <= 0.0f) s->base.color = (ColorF32){1, 1, 1, 1};
  s->tint = s->base.color;
  s->emission = s->base.emission_intensity;
  s->heat_decay = d->heat_decay;
  s->fade_start = d->fade_start;
  s->fade_end = d->fade_end;
  s->age = 0.0f;
  s->last_alpha = s->tint.a;
  if (s->created) {
    renderer_set_decal(w->r, s->h, &s->base);
    renderer_set_decal_visible(w->r, s->h, true);
  } else {
    s->h = renderer_create_decal(w->r, &s->base);
    s->created = true;
  }
  if (!s->live) w->active_slots++;
  s->live = true;
}

hz_internal void vfx_play_element(VfxWorld *w, VfxAsset *a, u32 i, const VfxPlayDesc *play, u32 seq) {
  const VfxElemDesc *e = &a->desc.elem[i];
  VfxElemRt *rt = &a->rt[i];
  switch (e->kind) {
  case VFX_ELEM_PARTICLES: {
    f32 n = e->count * vfx_or_one(play->count_scale);
    vfx_emit_particles(w, a, i, play, play->at, e->fixed ? (u32)(n + 0.5f) : vfx_budgeted(w, n));
  } break;
  case VFX_ELEM_LIGHT: vfx_spawn_light(w, e, play, play->at); break;
  case VFX_ELEM_BEAM: vfx_spawn_beam(w, rt, e, play, play->at); break;
  case VFX_ELEM_DECAL: vfx_spawn_decal(w, rt, e, play, play->at, seq); break;
  case VFX_ELEM_TRAIL: break; // a strip follows a loop's points
  }
}

void vfx_play_ex(VfxWorld *w, HzHandleT(VfxAsset) h, const VfxPlayDesc *play) {
  // particle bursts, trails, lights and decals are main-lane edits of their systems
  assert(is_main_thread());
  VfxAsset *a = ha_get(&w->assets, h, VfxAsset);
  if (!a) return;
  u32 seq = ++w->seq;
  for (u32 i = 0; i < a->desc.count; i++) {
    const VfxElemDesc *e = &a->desc.elem[i];
    // a burst carries its own delay; the rest wait here
    if (e->kind != VFX_ELEM_PARTICLES && e->delay > 0.0f) {
      if (w->pending_len < w->pending_cap)
        w->pending[w->pending_len++] = (VfxPending){.t = e->delay, .asset = h, .elem = i, .seq = seq, .play = *play};
      else
        log_warn("vfx: % delayed elements waiting, '%' element % is dropped", fmt_u32(w->pending_cap),
                 fmt_cstr(a->desc.name ? a->desc.name : "?"), fmt_u32(i));
      continue;
    }
    vfx_play_element(w, a, i, play, seq);
  }
}

void vfx_play(VfxWorld *w, HzHandleT(VfxAsset) h, v3 at) { vfx_play_ex(w, h, &(VfxPlayDesc){.at = at}); }

b32 vfx_request(const VfxRequests *rq, HzHandleT(VfxAsset) asset, const VfxPlayDesc *play) {
  VfxRequestQueue *q = rq->queue;
  u32 idx = concurrent_arr_reserve_idx(q->q);
  if (idx >= q->q.cap) {
    atomic_u32_inc_eval(&q->dropped_atomic);
    return false;
  }
  q->q.items[idx] = (VfxRequest){.asset = asset, .play = *play};
  return true;
}

/* ------------------------------------------------------------------- loops */

HzHandleT(VfxLoop) vfx_loop_start(VfxWorld *w, HzHandleT(VfxAsset) h, v3 at) {
  assert_single_thread_begin("vfx_loop_start");
  HzHandleT(VfxLoop) out = {0};
  VfxAsset *a = ha_get(&w->assets, h, VfxAsset);
  if (a && ha_count(&w->loops) >= w->max_loops) {
    log_warn("vfx: % loops live, dropping '%'", fmt_u32(w->max_loops), fmt_cstr(a->desc.name ? a->desc.name : "?"));
    a = NULL;
  }
  if (a) {
    VfxLoop loop = {.asset = h, .at = at, .to = at, .rate_scale = 1.0f};
    for (u32 i = 0; i < a->desc.count; i++) {
      const VfxElemDesc *e = &a->desc.elem[i];
      if (e->kind == VFX_ELEM_BEAM) {
        VfxBeam *b = vfx_beam_take(w, &a->rt[i]);
        if (b) {
          b->owned = true;
          b->live = false; // drawn once the loop has a target
          loop.beam[i] = b;
        }
      }
    }
    out = ha_add_t(&w->loops, &loop, VfxLoop);
  }
  assert_single_thread_end();
  return out;
}

hz_internal void vfx_loop_release(VfxWorld *w, VfxLoop *l) {
  VfxAsset *a = ha_get(&w->assets, l->asset, VfxAsset);
  for (u32 i = 0; a && i < a->desc.count; i++) {
    if (l->beam[i]) {
      if (l->beam[i]->live) w->active_slots--;
      l->beam[i]->owned = false;
      l->beam[i]->live = false;
      trail_clear(w->trails, l->beam[i]->trail);
    }
    if (!handle_is_valid(HANDLE_CAST(HzHandle, l->strip[i]))) continue;
    f32 life = a->desc.elem[i].trail.life;
    // the marks outlive the loop: the strip stays until its points have aged out
    if (life > 0.0f && w->dying_len < w->dying_cap)
      w->dying[w->dying_len++] = (VfxDying){.trail = l->strip[i], .remaining = life + 0.1f};
    else
      trail_destroy(w->trails, l->strip[i]);
  }
}

void vfx_loop_stop(VfxWorld *w, HzHandleT(VfxLoop) h) {
  assert_single_thread_begin("vfx_loop_stop");
  VfxLoop *l = ha_get(&w->loops, h, VfxLoop);
  if (l) l->stopped = true;
  assert_single_thread_end();
}

void vfx_loop_move(VfxWorld *w, HzHandleT(VfxLoop) h, v3 at) {
  VfxLoop *l = ha_get(&w->loops, h, VfxLoop);
  if (l) l->at = at;
}

void vfx_loop_set_rate(VfxWorld *w, HzHandleT(VfxLoop) h, f32 scale) {
  VfxLoop *l = ha_get(&w->loops, h, VfxLoop);
  if (l) l->rate_scale = scale > 0.0f ? scale : 0.0f;
}

void vfx_loop_set_params(VfxWorld *w, HzHandleT(VfxLoop) h, const VfxPlayDesc *params) {
  VfxLoop *l = ha_get(&w->loops, h, VfxLoop);
  if (l) l->params = *params;
}

void vfx_loop_set_target(VfxWorld *w, HzHandleT(VfxLoop) h, v3 to) {
  VfxLoop *l = ha_get(&w->loops, h, VfxLoop);
  if (!l) return;
  l->to = to;
  l->has_target = true;
}

void vfx_loop_trail_point(VfxWorld *w, HzHandleT(VfxLoop) h, v3 at, v3 normal, f32 alpha) {
  VfxLoop *l = ha_get(&w->loops, h, VfxLoop);
  if (!l || l->point_count >= VFX_LOOP_POINTS) return;
  l->points[l->point_count++] = (VfxLoopPoint){.at = at, .normal = normal, .alpha = alpha};
}

void vfx_loop_trail_clear(VfxWorld *w, HzHandleT(VfxLoop) h) {
  // trail edits are main-lane state
  assert(is_main_thread());
  VfxLoop *l = ha_get(&w->loops, h, VfxLoop);
  VfxAsset *a = l ? ha_get(&w->assets, l->asset, VfxAsset) : NULL;
  if (!a || !w->trails) return;
  for (u32 i = 0; i < a->desc.count; i++)
    if (handle_is_valid(HANDLE_CAST(HzHandle, l->strip[i]))) trail_clear(w->trails, l->strip[i]);
  l->point_count = 0;
}

void vfx_loop_trail_cut(VfxWorld *w, HzHandleT(VfxLoop) h) {
  VfxLoop *l = ha_get(&w->loops, h, VfxLoop);
  if (!l || l->point_count >= VFX_LOOP_POINTS) return;
  l->points[l->point_count++] = (VfxLoopPoint){.cut = true};
}

u32 vfx_loop_count(const VfxWorld *w) { return ha_count(&w->loops); }

// main lane: releases and removes the stopped loops, back to front so the swap-remove only moves visited ones
hz_internal void vfx_loops_reap(VfxWorld *w) {
  for (u32 d = w->loops.count; d-- > 0;) {
    VfxLoop *l = &w->loops.items[d];
    if (!l->stopped) continue;
    vfx_loop_release(w, l);
    u32 idx = w->loops.dense_to_sparse[d];
    ha_remove(&w->loops, ((HzHandle){.idx = idx, .gen = w->loops.sparse[idx].gen}));
  }
}

hz_internal void vfx_loops_tick(VfxWorld *w, f32 dt) {
  vfx_loops_reap(w);
  f32 budget = particle_system_budget(w->ps);
  ha_foreach(&w->loops, VfxLoop, l) {
    VfxAsset *a = ha_get(&w->assets, l->asset, VfxAsset);
    if (!a) continue;
    VfxPlayDesc play = l->params;
    play.at = l->at;
    for (u32 i = 0; i < a->desc.count; i++) {
      const VfxElemDesc *e = &a->desc.elem[i];
      switch (e->kind) {
      case VFX_ELEM_PARTICLES: {
        if (e->rate <= 0.0f) break;
        l->carry[i] += e->rate * l->rate_scale * (e->fixed ? 1.0f : budget) * vfx_or_one(play.count_scale) * dt;
        u32 n = (u32)l->carry[i];
        l->carry[i] -= (f32)n;
        vfx_emit_particles(w, a, i, &play, l->at, n);
      } break;
      case VFX_ELEM_BEAM: {
        VfxBeam *b = l->beam[i];
        if (!b || !l->has_target) break;
        play.to = l->to;
        vfx_beam_setup(b, e, &play, l->at, l->to);
        // held: the whole segment at its full width while the loop runs
        b->life = 1.0f;
        b->speed = 0.0f;
        b->length = 0.0f;
        if (!b->live) {
          b->live = true;
          w->active_slots++;
        }
      } break;
      case VFX_ELEM_TRAIL: {
        if (!w->trails) break;
        if (!handle_is_valid(HANDLE_CAST(HzHandle, l->strip[i]))) {
          TrailDesc td = vfx_trail_desc(e, false);
          l->strip[i] = trail_create(w->trails, &td);
        }
        HzHandleT(Trail) t = l->strip[i];
        ColorF32 base = e->trail.color.a > 0.0f ? e->trail.color : (ColorF32){1, 1, 1, 1};
        if (l->params.color.a > 0.0f) base = l->params.color;
        f32 width = e->trail.width * vfx_or_one(l->params.scale);
        for (u32 k = 0; k < l->point_count; k++) {
          const VfxLoopPoint *pt = &l->points[k];
          if (pt->cut) {
            trail_cut(w->trails, t);
            continue;
          }
          ColorF32 c = {base.r, base.g, base.b, base.a * pt->alpha};
          if (e->trail.orientation == TRAIL_ORIENT_AUTHORED) {
            trail_add_point_oriented_ex(w->trails, t, pt->at, pt->normal, width, c);
          } else {
            trail_add_point_ex(w->trails, t, pt->at, width, c);
          }
        }
      } break;
      default: break;
      }
    }
    l->point_count = 0;
  }
}

/* ------------------------------------------------------------------ frame */

hz_internal void vfx_lights_tick(VfxWorld *w, f32 dt) {
  for (u32 i = 0; i < w->light_count; i++) {
    VfxLight *l = &w->lights[i];
    if (!l->live) continue;
    l->age += dt;
    if (l->age >= l->life) {
      l->live = false;
      renderer_set_local_light_visible(w->r, l->h, false);
      continue;
    }
    f32 k = m_powf(1.0f - l->age / l->life, l->curve);
    renderer_set_local_light(w->r, l->h, &(RendererLocalLightDesc){.position = l->pos, .radius = l->radius,
                                                                    .color = l->hue, .intensity = l->candela * k});
  }
}

hz_internal void vfx_beam_draw(VfxWorld *w, VfxBeam *b) {
  v3 span = v3_sub(b->to, b->from);
  f32 len = v3_length(span);
  v3 dir = len > 1e-6f ? v3_scale(span, 1.0f / len) : V3_UP;
  f32 head = len, tail = 0.0f;
  if (b->speed > 0.0f) {
    // the streak enters at the origin, crosses the span and leaves through the target
    f32 travelled = b->speed * b->age;
    f32 streak = b->length > 0.0f ? m_minf(b->length, len) : 0.0f;
    head = m_minf(travelled, len);
    tail = streak > 0.0f ? m_maxf(travelled - streak, 0.0f) : 0.0f;
  }
  f32 u = b->life > 0.0f ? m_clampf(b->age / b->life, 0.0f, 1.0f) : 0.0f;
  f32 alpha = b->fade > 0.0f ? m_powf(1.0f - u, b->fade) : 1.0f;
  f32 width = b->width * m_lerpf(b->width_a, b->width_b, u);
  ColorF32 c = {b->color.r, b->color.g, b->color.b, b->color.a * alpha};
  trail_clear(w->trails, b->trail);
  trail_add_point_ex(w->trails, b->trail, v3_add(b->from, v3_scale(dir, tail)), width, c);
  trail_add_point_ex(w->trails, b->trail, v3_add(b->from, v3_scale(dir, head)), width, c);
}

hz_internal void vfx_slots_tick(VfxWorld *w, f32 dt) {
  ha_foreach(&w->assets, VfxAsset, a) {
    for (u32 i = 0; i < a->desc.count; i++) {
      VfxElemRt *rt = &a->rt[i];
      for (u32 k = 0; k < rt->beam_count; k++) {
        VfxBeam *b = &rt->beams[k];
        if (!b->live) continue;
        if (b->owned) {
          vfx_beam_draw(w, b);
          continue;
        }
        b->age += dt;
        if (b->age >= b->life) {
          b->live = false;
          w->active_slots--;
          trail_clear(w->trails, b->trail);
          continue;
        }
        vfx_beam_draw(w, b);
      }
      for (u32 k = 0; k < rt->decal_count; k++) {
        VfxDecalSlot *s = &rt->decals[k];
        if (!s->live) continue;
        s->age += dt;
        f32 heat = s->heat_decay > 0.0f ? m_expf(-s->heat_decay * s->age) : 1.0f;
        f32 fade = s->fade_end > s->fade_start ? 1.0f - m_smoothstepf(s->fade_start, s->fade_end, s->age) : 1.0f;
        f32 alpha = s->tint.a * heat * fade;
        if (alpha < 1e-3f) {
          s->live = false;
          w->active_slots--;
          renderer_set_decal_visible(w->r, s->h, false);
          continue;
        }
        if (alpha != s->last_alpha) {
          RendererDecalDesc desc = s->base;
          desc.color.a = alpha;
          desc.emission_intensity = s->emission * heat;
          renderer_set_decal(w->r, s->h, &desc);
          s->last_alpha = alpha;
        }
      }
    }
  }
}

hz_internal void vfx_pending_tick(VfxWorld *w, f32 dt) {
  for (u32 i = 0; i < w->pending_len;) {
    VfxPending *p = &w->pending[i];
    p->t -= dt;
    if (p->t > 0.0f) {
      i++;
      continue;
    }
    VfxPending fire = *p;
    w->pending[i] = w->pending[--w->pending_len];
    VfxAsset *a = ha_get(&w->assets, fire.asset, VfxAsset);
    if (a) vfx_play_element(w, a, fire.elem, &fire.play, fire.seq);
  }
}

hz_internal void vfx_dying_tick(VfxWorld *w, f32 dt) {
  for (u32 i = 0; i < w->dying_len;) {
    w->dying[i].remaining -= dt;
    if (w->dying[i].remaining > 0.0f) {
      i++;
      continue;
    }
    trail_destroy(w->trails, w->dying[i].trail);
    w->dying[i] = w->dying[--w->dying_len];
  }
}

hz_internal void vfx_ambient_tick(VfxWorld *w, const Camera *camera) {
  for (u32 i = 0; i < VFX_AMBIENT_MAX; i++) {
    VfxAmbientRt *am = &w->ambient[i];
    if (!am->live) continue;
    ParticleSystem *ps = w->ps;
    HzHandleT(ParticleMaterial) m = am->mat;
    const VfxAmbientLook *lk = &am->look;
    particle_material_set_float(ps, m, "time", lk->time);
    particle_material_set_float(ps, m, "night", lk->night);
    particle_material_set_float(ps, m, "density", particle_system_budget(ps));
    particle_material_set_float(ps, m, "unit", lk->unit > 0.0f ? lk->unit : 1.0f);
    particle_material_set_vec3(ps, m, "camera_pos", camera->pos);
    particle_material_set_vec3(ps, m, "camera_fwd", camera_forward(camera));
    particle_material_set_vec3(ps, m, "sun_dir", lk->sun_dir);
    particle_material_set_vec3(ps, m, "sun_color", lk->sun_color);
    particle_material_set_vec4(ps, m, "wind", (v4){lk->wind.x, lk->wind.y, lk->wind_strength, 0.0f});
  }
}

void vfx_world_frame(VfxWorld *w, const Camera *camera, f32 dt) {
  // particle bursts, trails, lights and decals are main-lane edits of their systems
  assert(is_main_thread());
  VfxRequestQueue *q = &w->queue;
  u32 n = concurrent_arr_len(q->q);
  u32 dropped = (u32)q->dropped_atomic;
  if (dropped) log_warn("vfx: % plays past the % the request buffer holds this frame were dropped", fmt_u32(dropped), fmt_u32(q->q.cap));
  if (n > q->q.cap) n = q->q.cap;
  for (u32 i = 0; i < n; i++) vfx_play_ex(w, q->q.items[i].asset, &q->q.items[i].play);
  atomic_u32_write_relaxed(&q->q.len_atomic, 0);
  q->dropped_atomic = 0;

  vfx_pending_tick(w, dt);
  vfx_loops_tick(w, dt);
  vfx_lights_tick(w, dt);
  if (w->active_slots) vfx_slots_tick(w, dt);
  vfx_dying_tick(w, dt);

  w->camera_pos = camera->pos;
  for (u32 i = 0; i < w->table_count; i++)
    if (w->tables[i].lit) particle_material_set_vec3(w->ps, w->tables[i].mat, "camera_pos", camera->pos);
  vfx_ambient_tick(w, camera);
}

void vfx_frame(EcsWorld *w, const Camera *camera, f32 dt) {
  const VfxRequests *rq = ecs_resource_get(w, VfxRequests);
  if (rq) vfx_world_frame(rq->queue->world, camera, dt);
}

/* ---------------------------------------------------------------- ambient */

VfxAmbient vfx_ambient_create(VfxWorld *w, const VfxAmbientDesc *desc) {
  assert_single_thread_begin("vfx_ambient_create");
  VfxAmbient out = {0};
  u32 slot = VFX_AMBIENT_MAX;
  for (u32 i = 0; i < VFX_AMBIENT_MAX && slot == VFX_AMBIENT_MAX; i++)
    if (!w->ambient[i].live) slot = i;
  if (slot == VFX_AMBIENT_MAX) {
    log_error("vfx: % ambient volumes are live", fmt_u32(VFX_AMBIENT_MAX));
  } else {
    VfxAmbientRt *am = &w->ambient[slot];
    am->desc = *desc;
    u32 count = desc->count ? desc->count : 900u;
    ParticleMaterialDesc *d = shader_vfx_ambient_particle_material_desc();
    particle_material_desc_billboard_defaults(d);
    d->max_particles = count;
    am->mat = particle_material_create(w->ps, d);
    ParticleSystem *ps = w->ps;
    f32 box = desc->box > 0.0f ? desc->box : 70.0f;
    particle_material_set_vec4(ps, am->mat, "volume",
                               (v4){box, desc->height_day > 0.0f ? desc->height_day : 5.0f,
                                    desc->height_night > 0.0f ? desc->height_night : 2.5f, desc->sway > 0.0f ? desc->sway : 0.8f});
    particle_material_set_vec4(ps, am->mat, "motion", (v4){2.0f, 0.4f, 0.6f, 0.02f});
    particle_material_set_vec4(ps, am->mat, "size_px", (v4){desc->size_px > 0.0f ? desc->size_px : 70.0f, 1.0f,
                                                            desc->size_px_max > 0.0f ? desc->size_px_max : 24.0f, 2.0f});
    particle_material_set_vec3(ps, am->mat, "day_color", desc->day_color);
    particle_material_set_vec3(ps, am->mat, "night_color", desc->night_color);
    particle_material_set_vec4(ps, am->mat, "glint", (v4){desc->glint_base, desc->glint_gain, desc->glint_exponent > 0.0f ? desc->glint_exponent : 5.0f, 0});
    particle_material_set_vec4(ps, am->mat, "blink", (v4){1.2f, 1.5f, 0.55f, 0});
    particle_material_set_vec4(ps, am->mat, "fade", (v4){0.8f, 3.0f, 0.3f, 0.5f});
    ParticleEmitterDesc ed = {
        .max_particles = count,
        .life = 1000.0f, // the shader never lets them die
        .shape = PARTICLE_SHAPE_POINT,
        .rotation = quat_identity(),
        .material = am->mat,
    };
    am->emitter = particle_emitter_create(ps, &ed);
    particle_emitter_burst_at(ps, am->emitter, &(ParticleBurstDesc){.count = count, .position = V3_ZERO});
    am->live = true;
    am->look = (VfxAmbientLook){.unit = 1.0f, .sun_dir = V3_UP, .sun_color = {1, 1, 1}};
    out = (VfxAmbient){.slot = slot + 1u};
  }
  assert_single_thread_end();
  return out;
}

void vfx_ambient_set(VfxWorld *w, VfxAmbient h, const VfxAmbientLook *look) {
  if (h.slot == 0 || h.slot > VFX_AMBIENT_MAX) return;
  VfxAmbientRt *am = &w->ambient[h.slot - 1u];
  if (am->live) am->look = *look;
}

void vfx_ambient_destroy(VfxWorld *w, VfxAmbient h) {
  assert_single_thread_begin("vfx_ambient_destroy");
  if (h.slot != 0 && h.slot <= VFX_AMBIENT_MAX) {
    VfxAmbientRt *am = &w->ambient[h.slot - 1u];
    if (am->live) {
      particle_material_destroy(w->ps, am->mat);
      *am = (VfxAmbientRt){0};
    }
  }
  assert_single_thread_end();
}

/* ------------------------------------------------------------------- vary */

void vfx_desc_vary(VfxDesc *desc, u64 seed, f32 amount) {
  Random rng = random_create(seed);
  f32 a = m_clampf(amount, 0.0f, 0.35f);
  u32 n = desc->count < VFX_ELEM_MAX ? desc->count : VFX_ELEM_MAX;
  for (u32 i = 0; i < n; i++) {
    VfxElemDesc *e = &desc->elem[i];
    f32 k = 1.0f + random_f32_range(&rng, -a, a);
    e->count *= k;
    e->speed.x *= k;
    e->speed.y *= k;
    f32 kb = 1.0f + random_f32_range(&rng, -a, a);
    f32 ke = 1.0f + random_f32_range(&rng, -a, a);
    e->size.x *= kb;
    e->size.y *= ke;
    e->size_max.x *= kb;
    e->size_max.y *= ke;
    f32 lk = 1.0f + random_f32_range(&rng, -a * 0.5f, a * 0.5f);
    e->life.x *= lk;
    e->life.y *= lk;
  }
}
