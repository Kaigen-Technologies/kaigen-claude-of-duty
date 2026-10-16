// Claude of Duty effects: atlases, muzzle flashes, impacts, decals, tracers, explosions, ambience on the engine's vfx block over the particle and trail systems.

#include "gameplay/vfx/vfx.h"
#include "rendering/particle_system.h"
#include "rendering/trail_system.h"
#include "rendering/features/decals.h"
#include "shaders/cod_fx_particles_material.h"
#include "shaders/cod_fx_sprites.h"
#include "shaders/pg_bake_downsample_rgba.h"
#include "shaders/cod_fx_decals.h"
#include "shaders/cod_fx_decal_maps.h"
#include "procgen/pg_dump.h"

#include "cod_fx_parts.c"
#include "cod_fx_impacts.c"
#include "cod_fx_surfaces.c"
#include "cod_fx_recipes.c"
#include "cod_fx_atlas.c"
#include "cod_fx_events.c"
#include "cod_fx_decals.c"

#define CF_DECAL_QUEUE 512
#define CF_DECAL_RING 256u // the engine's visible decal set: the ring never holds more than it can show
#define CF_REQUESTS 1024
#define CF_AMBIENT_BOX 22.0f

// main-lane state of the effects package
typedef struct CodFxRuntime {
  Allocator alloc;
  ParticleSystem *ps;
  TrailSystem *trails;
  VfxWorld *vfx;
  HzHandleT(ParticleMaterial) mat;
  CodFxAtlas atlas;
  VfxAmbient ambient;
  CodFxDecals decals;
  EcsQuery *cam_query;
  EcsQuery *dev_query;
  u32 particle_budget;
  u64 init_t0;
  b32 atlas_failed;
} CodFxRuntime;
ECS_COMPONENT_TYPEDEF(CodFxRuntime);

// reports the effects build into the load gate
ecs_system(cod_fx_load_system, EcsResIn(CodFxState), EcsResInOut(CodLoad)) {
  const CodFxState *st = ecs_sys_get_resource(CodFxState);
  ecs_sys_get_resource_mut(CodLoad)->progress[COD_LOAD_FX] = st->ready ? 1.0f : 0.0f;
}

// registers every effect of the plan: its elements take the one material, its handles land where the plan points
hz_internal void cod_fx_plan_register(CodFxPlan *plan, VfxWorld *vfx, HzHandleT(ParticleMaterial) mat) {
  for (u32 i = 0; i < plan->asset_count; i++) {
    VfxDesc *d = plan->assets[i].desc;
    for (u32 k = 0; k < d->count; k++)
      if (d->elem[k].kind == VFX_ELEM_PARTICLES) d->elem[k].material = mat;
    *plan->assets[i].out = vfx_register(vfx, d);
  }
}

// every recipe into the plan, each with its handle slot in the assets resource
hz_internal void cod_fx_build_plan(CodFxPlan *plan, CodFxAssets *as) {
  static const char *surface_names[COD_SURFACE_COUNT] = {"cod_impact_concrete", "cod_impact_metal", "cod_impact_wood", "cod_impact_dirt",
                                                         "cod_impact_sand", "cod_impact_glass", "cod_impact_water", "cod_impact_foliage",
                                                         "cod_impact_fabric", "cod_impact_flesh", "cod_impact_rubber", "cod_impact_plaster"};
  for (u32 s = 0; s < COD_SURFACE_COUNT; s++) {
    VfxDesc *d = cod_fx_plan_asset(plan, surface_names[s], &as->impact[s]);
    switch (s) {
    case COD_SURFACE_CONCRETE: cod_fx_impact_concrete(plan, d); break;
    case COD_SURFACE_PLASTER: cod_fx_impact_plaster(plan, d); break;
    case COD_SURFACE_METAL: cod_fx_impact_metal(plan, d, false); break;
    case COD_SURFACE_WOOD: cod_fx_impact_wood(plan, d); break;
    case COD_SURFACE_DIRT: cod_fx_impact_ground(plan, d, false); break;
    case COD_SURFACE_SAND: cod_fx_impact_ground(plan, d, true); break;
    case COD_SURFACE_GLASS: cod_fx_impact_glass(plan, d); break;
    case COD_SURFACE_WATER: cod_fx_impact_water(plan, d); break;
    case COD_SURFACE_FLESH: cod_fx_impact_flesh(plan, d); break;
    case COD_SURFACE_FOLIAGE: cod_fx_impact_foliage(plan, d); break;
    case COD_SURFACE_FABRIC: cod_fx_impact_soft(plan, d, false); break;
    default: cod_fx_impact_soft(plan, d, true); break;
    }
  }
  cod_fx_impact_metal(plan, cod_fx_plan_asset(plan, "cod_impact_metal_graze", &as->impact_graze), true);
  static const char *muzzle_names[COD_WEAPON_COUNT] = {"cod_muzzle_rifle", "cod_muzzle_smg", "cod_muzzle_pistol"};
  for (u32 w = 0; w < COD_WEAPON_COUNT; w++) {
    CodFxMuzzleProfile pr = cod_fx_muzzle_profile(w);
    cod_fx_recipe_muzzle(plan, cod_fx_plan_asset(plan, muzzle_names[w], &as->muzzle[w]), &pr);
  }
  cod_fx_recipe_flash_light(plan, cod_fx_plan_asset(plan, "cod_flash_light", &as->flash_light));
  static const char *tracer_names[CF_TRACER_COUNT] = {"cod_tracer_core", "cod_tracer_glow", "cod_tracer_head"};
  for (u32 i = 0; i < CF_TRACER_COUNT; i++) cod_fx_recipe_tracer(plan, cod_fx_plan_asset(plan, tracer_names[i], &as->tracer[i]), i);
  cod_fx_recipe_explosion(plan, cod_fx_plan_asset(plan, "cod_explosion", &as->explosion));
  cod_fx_recipe_death(plan, cod_fx_plan_asset(plan, "cod_death", &as->death));
  cod_fx_recipe_land(plan, cod_fx_plan_asset(plan, "cod_land", &as->land));
  cod_fx_recipe_step(plan, cod_fx_plan_asset(plan, "cod_step", &as->step));
}

void cod_fx_init(EcsWorld *w) {
  RendererState *r = ecs_resource_get_mut(w, RendererState);
  ecs_resource_set_zero(w, CodFxState);
  ecs_resource_set_zero(w, CodFxViewFlash);
  ecs_resource_set_zero(w, CodFxAssets);
  ecs_resource_set_zero(w, CodFxRuntime);
  CodFxRuntime *s = ecs_resource_get_mut(w, CodFxRuntime);
  CodFxAssets *as = ecs_resource_get_mut(w, CodFxAssets);
  s->init_t0 = os_time_now();
  s->alloc = make_arena_allocator(arena_create(MB(16), MB(1)));
  // the start level's budget sizes the sprite atlas and the motes (load-time); the decal ring follows every level
  CodQualityPreset preset = cod_quality_preset(MIN((u32)renderer_quality(r), (u32)COD_QUALITY_ULTRA));
  s->particle_budget = preset.particle_budget;

  CodFxPlan *plan = ALLOC(&s->alloc, CodFxPlan);
  plan->alloc = &s->alloc;
  cod_fx_build_plan(plan, as);

  // the pools: every element's pool plus the ambient motes
  u32 motes = (u32)m_clampf((f32)s->particle_budget * 0.06f, 96.0f, 600.0f);
  s->ps = particle_system_create(&(ParticleSystemInitDesc){.renderer = r,
                                                           .sample_count = renderer_sample_count(r),
                                                           .max_total_particles = plan->pool_sum + motes + 64u,
                                                           .max_emitters = plan->element_count + 16u,
                                                           .max_materials = 4,
                                                           .max_emitters_per_material = plan->element_count + 4u});
  particle_system_attach_to_renderer(s->ps, r);
  s->trails = trail_system_create(&(TrailSystemDesc){.renderer = r, .max_trails = 3 * 24 + 8, .max_points_per_trail = 4, .alloc = &s->alloc});
  trail_system_attach_to_renderer(s->trails, r);
  vfx_init(w, &(VfxWorldDesc){.particles = s->ps, .renderer = r, .trails = s->trails, .max_loops = 4, .max_requests = CF_REQUESTS, .max_lights = 24,
                              .particles_per_material = plan->pool_sum + 64u});
  s->vfx = vfx_world(w);
  particle_system_set_floor(s->ps, &(ParticleFloorDesc){.plane_y = 0.03f});

  // the one material every particle element draws with; each element selects its row of the table
  ParticleMaterialDesc *md = shader_cod_fx_particles_particle_material_desc();
  particle_material_desc_billboard_defaults(md);
  md->max_particles = plan->pool_sum;
  s->mat = particle_material_create(s->ps, md);
  for (u32 row = 0; row < plan->row_count; row++)
    for (u32 k = 0; k < CF_ROW_VEC4S; k++) particle_material_set_vec4_idx(s->ps, s->mat, "table", row * CF_ROW_VEC4S + k, plan->rows[row].v[k]);
  cod_fx_plan_register(plan, s->vfx, s->mat);
  log_info("fx: % effects, % particle elements, % particles pooled, % table rows", fmt_u32(plan->asset_count), fmt_u32(plan->element_count),
           fmt_u32(plan->pool_sum), fmt_u32(plan->row_count));

  s->ambient = vfx_ambient_create(s->vfx, &(VfxAmbientDesc){.count = motes,
                                                              .box = CF_AMBIENT_BOX,
                                                              .height_day = 3.0f,
                                                              .height_night = 3.0f,
                                                              .sway = 0.4f,
                                                              .size_px = 4.5f,
                                                              .size_px_max = 10.0f,
                                                              .day_color = {0.147f, 0.141f, 0.132f},
                                                              .night_color = {0.03f, 0.029f, 0.027f},
                                                              .glint_base = 1.0f,
                                                              .glint_gain = 0.0f});

  // the decal requests of every consumer
  CodFxDecalQueue *dq = ALLOC(&s->alloc, CodFxDecalQueue);
  dq->q = concurrent_arr_new_alloc(&s->alloc, CodFxDecalReq, CF_DECAL_QUEUE);
  as->decals = dq;

  s->cam_query = ecs_query_cached(w, EcsIn(Camera));
  s->dev_query = ecs_query_cached(w, EcsIn(CodDevSwitches));
  cod_fx_decals_init(&s->decals, &s->alloc, ecs_resource_get_mut(w, AssetSystem), CF_DECAL_RING);
  cod_fx_decals_set_limit(&s->decals, r, MIN(preset.decal_budget, CF_DECAL_RING));
  cod_fx_atlas_pipelines(&s->atlas, s->particle_budget >= 10000 ? 1024u : 512u);
}

void cod_fx_schedule(EcsWorld *w) {
  ecs_schedule_single(w, cod_fx_load_system);   // writes the CodLoad resource
  ecs_schedule(w, cod_fx_soldier_system);       // per soldier: requests only, the queues take them from any lane
  ecs_schedule_single(w, cod_fx_player_system);    // resources with no entity: one pass over the player's events; owns the view flash pool
  ecs_schedule_single(w, cod_fx_explosion_system); // one resource: one pass over the frame's explosions
}

void cod_fx_lanes(EcsWorld *w) { (void)w; }

// the bake and the registrations land on the first frames; later frames feed the lighting and run the systems
void cod_fx_frame(EcsWorld *w) {
  CodFxRuntime *s = ecs_resource_get_mut(w, CodFxRuntime);
  CodFxState *st = ecs_resource_get_mut(w, CodFxState);
  RendererState *r = ecs_resource_get_mut(w, RendererState);
  const CodLook *look = ecs_resource_get(w, CodLook);
  const CodClock *clock = ecs_resource_get(w, CodClock);
  ParticleSystem *ps = s->ps;

  if (!s->atlas.recorded) {
    if (cod_fx_atlas_pipelines_failed(&s->atlas)) {
      if (!s->atlas_failed) log_error("fx: the sprite bake pipelines failed to compile");
      s->atlas_failed = true;
      ecs_resource_get_mut(w, CodLoad)->failed = true;
      return;
    }
    if (!cod_fx_atlas_pipelines_ready(&s->atlas)) return;
    cod_fx_atlas_record(&s->atlas);
    particle_material_set_texture(ps, s->mat, "albedo", s->atlas.tex);
    st->bake_ms = os_ticks_to_ms(os_time_diff(os_time_now(), s->init_t0));
    st->ready = true;
    log_info("fx: effects ready % ms after init", fmt_f32_p((f32)st->bake_ms, 1));
  }
  cod_fx_atlas_collect(&s->atlas);

  // scene units into display units: scene_per_unit cd/m2 each, pre-exposed
  f32 unit = look->scene_per_unit * renderer_pre_exposure(r);
  st->unit = unit;
  vfx_set_light_unit(s->vfx, look->scene_per_unit);
  v3 key = {look->key_color.r / look->scene_per_unit, look->key_color.g / look->scene_per_unit, look->key_color.b / look->scene_per_unit};
  f32 sun_i = m_maxf(key.x, m_maxf(key.y, key.z));
  f32 a = m_clampf(sun_i * 0.22f, 0.02f, 3.0f);
  particle_material_set_float(ps, s->mat, "unit", unit);
  particle_material_set_vec3(ps, s->mat, "sun_dir", look->key_dir);
  particle_material_set_vec3(ps, s->mat, "sun_color", key);
  particle_material_set_vec3(ps, s->mat, "amb_top", (v3){a * 0.78f, a * 0.92f, a * 1.25f});
  particle_material_set_vec3(ps, s->mat, "amb_bottom", (v3){a * 0.5f, a * 0.44f, a * 0.38f});

  cod_fx_decals_poll(&s->decals, ecs_resource_get(w, AssetSystem));
  CodQualityPreset level = cod_quality_preset(MIN((u32)renderer_quality(r), (u32)COD_QUALITY_ULTRA));
  cod_fx_decals_set_limit(&s->decals, r, MIN(level.decal_budget, CF_DECAL_RING));
  cod_fx_decals_frame(&s->decals, r, ecs_resource_get(w, CodFxAssets)->decals, clock->time);
  const CodDevSwitches *sw = ecs_query_get_singleton(CodDevSwitches, s->dev_query);
  if (sw) cod_fx_decal_dump_frame(&s->decals, sw->fx_dump);
  st->live_decals = s->decals.live;
  Camera *cam = ecs_query_get_singleton(Camera, s->cam_query);
  if (!cam) return;
  f32 sun_factor = m_clampf(sun_i / 4.3f, 0.0f, 1.6f);
  vfx_ambient_set(s->vfx, s->ambient,
                  &(VfxAmbientLook){.time = clock->time, .night = look->night, .unit = unit, .sun_dir = look->key_dir, .sun_color = {sun_factor, sun_factor, sun_factor}});
  vfx_frame(w, cam, clock->dt);
  particle_system_update(ps, &(ParticleSystemFlushDesc){.delta_time = clock->dt, .camera = cam});
  trail_system_update(s->trails, clock->dt);
}

void cod_fx_prepare(EcsWorld *w, const Camera *cam) {
  trail_system_prepare(ecs_resource_get(w, CodFxRuntime)->trails, cam);
}
