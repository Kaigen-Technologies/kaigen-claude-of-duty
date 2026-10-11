#include "scatter.h"
#include "assets/assets.h"
#include "ecs/ecs_physics/ecs_physics.h"
#include "ecs/transform/ecs_transform.h"
#include "lib/assert.h"

#define SCATTER_CID(w, T) ecs_component_ensure((w), sizeof(T), _Alignof(T), #T)
#define SCATTER_CULL_INTERVAL 0.4f
#define SCATTER_CULL_MOVE 10.0f
#define SCATTER_GRID_CELL 8.0f
#define SCATTER_UNBOUNDED 1e15f


typedef struct ScatterSpeciesRt {
  u32 variant_count, tint_count, lod_count;
  ScatterQualityTable lod_distance[SCATTER_LOD_MAX];
  f32 lod_band;
  ScatterQualityTable density_scale;
  ScatterCollider collider;
} ScatterSpeciesRt;

typedef struct ScatterEndless {
  ArenaAllocator *arena;
  Allocator alloc;
  ArenaAllocator *scratch; // one window's candidates and grid, reset every rebuild
  Allocator scratch_alloc;
  f32 cell_size;
  i32 radius_cells;
  u32 seed;
  PgScatterSpecies *pg;
  u32 *pool_size;
  PgScatterHeightFn height;
  PgScatterNormalFn normal;
  void *surface_ctx;
  ScatterWindowFn window;
  void *window_ctx;
  ScatterPlacement *placements; // grouped by species, at most pool_size each
  u32 *species_first;           // [species_count + 1]
  i32 cell_x, cell_z;
  b32 placed;
  u32 generation;
  u32 rebuilds;
  f32 last_us, max_us;
} ScatterEndless;

struct ScatterField {
  b32 active;
  ScatterMode mode;
  b32 follow_camera;
  i32 quality_override;
  f32 margin, cull_interval, cull_move;
  u32 species_count;
  ScatterSpeciesRt *species;
  f32 *min_sq; // [species * SCATTER_LOD_MAX + lod]
  f32 *max_sq;
  i32 applied_quality;
  b32 has_focus;
  v3 focus;
  f32 timer;
  v3 last;
  b32 has_last, force, due, clearing;
  f32 cull_x, cull_z;
  u32 rows;
  ScatterEndless *endless;
};

struct ScatterSink {
  ScatterEndless *e;
  PgScatterGrid *grid;
  DynArray(ScatterPlacement) placed;
};

/* ------------------------------------------------------------------ helpers */

u32 scatter_pick(u32 count, f32 roll) {
  if (count <= 1)
    return 0;
  return MIN(count - 1, (u32)m_floorf(roll * (f32)count));
}

RendererQuality scatter_quality_level(const RendererState *r, i32 quality_override) {
  if (quality_override >= 0)
    return (RendererQuality)MIN(quality_override, (i32)RENDERER_QUALITY_COUNT - 1);
  RendererQuality q = renderer_quality(r);
  return q >= RENDERER_QUALITY_COUNT ? RENDERER_QUALITY_HIGH : q;
}

ScatterPlacement scatter_placement_from_point(const PgScatterPoint *p, const ScatterSpeciesDesc *species) {
  u32 variants = species && species->variant_count ? species->variant_count : 1;
  u32 tints = species && species->tint_count ? species->tint_count : 1;
  return (ScatterPlacement){.species = p->species,
                            .position = p->position,
                            .rotation = p->rotation,
                            .scale = p->scale,
                            .variant = scatter_pick(variants, p->roll.x),
                            .tint = scatter_pick(tints, p->roll.y),
                            .roll = p->roll};
}

hz_internal ScatterState *scatter_state(EcsWorld *w) {
  ScatterState *s = ecs_resource_get_mut(w, ScatterState);
  assert_msg(s, "scatter: call scatter_init first");
  return s;
}

hz_internal ScatterField *scatter_field(const ScatterState *s, u32 field) {
  assert_msg(field < s->field_count, "scatter: field % was never created", fmt_u32(field));
  return &s->fields[field];
}

hz_internal void scatter_copy_species(Allocator *alloc, ScatterField *f, const ScatterSpeciesDesc *species, u32 count) {
  f->species = ALLOC_ARRAY(alloc, ScatterSpeciesRt, count);
  f->min_sq = ALLOC_ARRAY(alloc, f32, count * SCATTER_LOD_MAX);
  f->max_sq = ALLOC_ARRAY(alloc, f32, count * SCATTER_LOD_MAX);
  for (u32 i = 0; i < count; i++) {
    const ScatterSpeciesDesc *d = &species[i];
    u32 lods = d->lod_count ? d->lod_count : 1;
    assert_msg(lods <= SCATTER_LOD_MAX, "scatter: species % has % LODs, the limit is %", fmt_u32(i), fmt_u32(lods),
               fmt_u32(SCATTER_LOD_MAX));
    ScatterSpeciesRt *rt = &f->species[i];
    *rt = (ScatterSpeciesRt){.variant_count = d->variant_count ? d->variant_count : 1,
                             .tint_count = d->tint_count ? d->tint_count : 1,
                             .lod_count = lods,
                             .lod_band = d->lod_band,
                             .density_scale = d->density_scale,
                             .collider = d->collider};
    for (u32 l = 0; l < SCATTER_LOD_MAX; l++)
      rt->lod_distance[l] = d->lod_distance[l];
  }
  f->species_count = count;
}

// the squared xz band of every species and LOD at one quality level
hz_internal void scatter_apply_quality(ScatterField *f, RendererQuality q) {
  for (u32 s = 0; s < f->species_count; s++) {
    const ScatterSpeciesRt *sp = &f->species[s];
    for (u32 l = 0; l < SCATTER_LOD_MAX; l++) {
      u32 at = s * SCATTER_LOD_MAX + l;
      if (l >= sp->lod_count) {
        f->min_sq[at] = SCATTER_UNBOUNDED;
        f->max_sq[at] = 0.0f;
        continue;
      }
      f32 hi = sp->lod_distance[l].value[q];
      f32 lo = l > 0 ? MAX(sp->lod_distance[l - 1].value[q] - sp->lod_band - f->margin, 0.0f) : 0.0f;
      f32 edge = hi > 0.0f ? hi + f->margin : SCATTER_UNBOUNDED;
      f->min_sq[at] = lo * lo;
      f->max_sq[at] = edge * edge;
    }
  }
  f->applied_quality = (i32)q;
  f->force = true;
}

/* ------------------------------------------------------------------- colliders */

hz_internal ScatterCollider scatter_collider_of(const ScatterPlacement *p, const ScatterSpeciesRt *sp) {
  if (p->collider.kind != SCATTER_COLLIDER_NONE)
    return p->collider;
  ScatterCollider c = sp->collider;
  if (c.kind == SCATTER_COLLIDER_NONE)
    return c;
  c.offset = v3_mul(c.offset, p->scale);
  c.radius *= MAX(p->scale.x, p->scale.z);
  c.height *= p->scale.y;
  c.half = v3_mul(c.half, p->scale);
  return c;
}

hz_internal ColliderBody scatter_collider_body(const ScatterPlacement *p, const ScatterCollider *c) {
  ColliderBody b = {.pos = v3_add(p->position, c->offset), .rot = quat_identity(), .scale = 1.0f};
  switch (c->kind) {
  case SCATTER_COLLIDER_CAPSULE:
    b.collider = collider_capsule(V3_ZERO, (v3){0.0f, MAX(c->height - c->radius, 0.0f), 0.0f}, c->radius);
    break;
  case SCATTER_COLLIDER_SPHERE:
    b.collider = collider_sphere(V3_ZERO, c->radius);
    break;
  case SCATTER_COLLIDER_BOX:
    b.rot = p->rotation;
    b.collider = collider_box(V3_ZERO, c->half);
    break;
  default:
    break;
  }
  return b;
}

/* ------------------------------------------------------------------ spawn rows */

// one row per drawn part; `material` columns exist only for species with a tint_param
typedef struct ScatterRows {
  WorldTransform *xfs;
  EcsSubMeshInstance *sis;
  EcsVisibility *vis;
  ScatterRow *rows;
  EcsMaterialInstance *mats;
  u32 count;
} ScatterRows;

hz_internal ScatterRows scatter_rows_alloc(Allocator *a, u32 cap, b32 with_material) {
  return (ScatterRows){.xfs = ALLOC_ARRAY(a, WorldTransform, cap),
                       .sis = ALLOC_ARRAY(a, EcsSubMeshInstance, cap),
                       .vis = ALLOC_ARRAY(a, EcsVisibility, cap),
                       .rows = ALLOC_ARRAY(a, ScatterRow, cap),
                       .mats = with_material ? ALLOC_ARRAY(a, EcsMaterialInstance, cap) : NULL};
}

hz_internal void scatter_rows_spawn(EcsWorld *w, Allocator *a, const ScatterRows *rows) {
  if (rows->count == 0)
    return;
  u32 n = rows->mats ? 5 : 4;
  EcsSpawnColumn *cols = ALLOC_ARRAY(a, EcsSpawnColumn, n);
  cols[0] = (EcsSpawnColumn){.component = SCATTER_CID(w, WorldTransform), .data = rows->xfs};
  cols[1] = (EcsSpawnColumn){.component = SCATTER_CID(w, EcsSubMeshInstance), .data = rows->sis};
  cols[2] = (EcsSpawnColumn){.component = SCATTER_CID(w, EcsVisibility), .data = rows->vis};
  cols[3] = (EcsSpawnColumn){.component = SCATTER_CID(w, ScatterRow), .data = rows->rows};
  if (rows->mats)
    cols[4] = (EcsSpawnColumn){.component = SCATTER_CID(w, EcsMaterialInstance), .data = rows->mats};
  ecs_spawn_block(w, &(EcsSpawnBlockDesc){.count = rows->count, .columns = cols, .column_count = n});
}

hz_internal const ScatterVariant *scatter_variant(const ScatterSpeciesDesc *sp, u32 variant, u32 tint, u32 lod) {
  u32 tints = sp->tint_count ? sp->tint_count : 1, lods = sp->lod_count ? sp->lod_count : 1;
  return &sp->variants[(variant * tints + tint) * lods + lod];
}

hz_internal void scatter_part_push(ScatterRows *rows, RendererState *r, const ScatterPart *part, HzHandleT(RendererMaterial) material,
                                   WorldTransform xf, ScatterRow tag, b32 visible) {
  u32 i = rows->count++;
  rows->xfs[i] = xf;
  rows->sis[i].instance = renderer_submesh_instantiate(r, &(RendererSubMeshInstanceDesc){
                                                              .submesh = part->submesh,
                                                              .material = material,
                                                              .transform = {xf.position, xf.rotation, xf.scale},
                                                              .casts_shadow = part->casts_shadow,
                                                              .static_shadow = part->static_shadow,
                                                          });
  rows->vis[i] = (EcsVisibility){.visible = visible, .casts_shadow = part->casts_shadow};
  rows->rows[i] = tag;
  if (rows->mats)
    rows->mats[i].instance = material;
}

void scatter_static_spawn(EcsWorld *w, RendererState *r, u32 field, const ScatterSpeciesDesc *species, const ScatterPlacement *p,
                          u32 count) {
  // renderer instances are created here, which is main-lane state
  assert(is_main_thread());
  ScatterState *s = scatter_state(w);
  ScatterField *f = scatter_field(s, field);
  assert_msg(f->mode == SCATTER_STATIC, "scatter: field % is endless", fmt_u32(field));
  Allocator *ca = ecs_cmd_allocator(w);

  u32 plain_cap = 0, tinted_cap = 0, collider_cap = 0;
  for (u32 i = 0; i < count; i++) {
    const ScatterSpeciesDesc *sd = &species[p[i].species];
    const ScatterSpeciesRt *rt = &f->species[p[i].species];
    u32 parts = 0;
    for (u32 l = 0; l < rt->lod_count; l++)
      parts += scatter_variant(sd, p[i].variant, p[i].tint, l)->part_count;
    if (sd->tint_param)
      tinted_cap += parts;
    else
      plain_cap += parts;
    if (scatter_collider_of(&p[i], rt).kind != SCATTER_COLLIDER_NONE)
      collider_cap++;
  }
  ScatterRows plain = scatter_rows_alloc(ca, plain_cap, false);
  ScatterRows tinted = scatter_rows_alloc(ca, tinted_cap, true);
  ColliderBody *bodies = ALLOC_ARRAY(ca, ColliderBody, collider_cap);
  u32 body_count = 0;

  for (u32 i = 0; i < count; i++) {
    const ScatterPlacement *pl = &p[i];
    const ScatterSpeciesDesc *sd = &species[pl->species];
    const ScatterSpeciesRt *rt = &f->species[pl->species];
    WorldTransform xf = {.position = pl->position, .rotation = pl->rotation, .scale = pl->scale};
    for (u32 l = 0; l < rt->lod_count; l++) {
      const ScatterVariant *v = scatter_variant(sd, pl->variant, pl->tint, l);
      ScatterRow tag = {.x = pl->position.x, .z = pl->position.z, .field = field, .species = pl->species, .lod = l};
      for (u32 k = 0; k < v->part_count; k++) {
        const ScatterPart *part = &v->parts[k];
        if (sd->tint_param) {
          HzHandleT(RendererMaterial) mi = renderer_create_material_instance(r, part->material);
          material_set_vec4(r, mi, sd->tint_param, (vec4){pl->instance_tint.x, pl->instance_tint.y, pl->instance_tint.z, pl->instance_tint.w});
          scatter_part_push(&tinted, r, part, mi, xf, tag, false);
        } else {
          scatter_part_push(&plain, r, part, part->material, xf, tag, false);
        }
      }
    }
    ScatterCollider c = scatter_collider_of(pl, rt);
    if (c.kind != SCATTER_COLLIDER_NONE)
      bodies[body_count++] = scatter_collider_body(pl, &c);
  }
  scatter_rows_spawn(w, ca, &plain);
  scatter_rows_spawn(w, ca, &tinted);
  if (body_count > 0) {
    ScatterCollision *tags = ALLOC_ARRAY(ca, ScatterCollision, body_count);
    for (u32 i = 0; i < body_count; i++)
      tags[i].field = field;
    StaticBody *marker = ALLOC(ca, StaticBody);
    EcsSpawnColumn *cols = ALLOC_ARRAY(ca, EcsSpawnColumn, 3);
    cols[0] = (EcsSpawnColumn){.component = SCATTER_CID(w, ColliderBody), .data = bodies};
    cols[1] = (EcsSpawnColumn){.component = SCATTER_CID(w, StaticBody), .data = marker, .constant = true};
    cols[2] = (EcsSpawnColumn){.component = SCATTER_CID(w, ScatterCollision), .data = tags};
    ecs_spawn_block(w, &(EcsSpawnBlockDesc){.count = body_count, .columns = cols, .column_count = 3});
  }
  f->rows += plain.count + tinted.count;
  f->force = true;
}

/* ----------------------------------------------------------------------- sink */

void scatter_sink_place(ScatterSink *s, const ScatterPlacement *p) { dyn_arr_append(s->placed, *p); }

void scatter_sink_reserve_disc(ScatterSink *s, f32 x, f32 z, f32 radius) { pg_scatter_grid_reserve_disc(s->grid, x, z, radius); }

void scatter_sink_reserve_box(ScatterSink *s, f32 x, f32 z, f32 half_x, f32 half_z, f32 yaw) {
  pg_scatter_grid_reserve_box(s->grid, x, z, half_x, half_z, yaw);
}

/* ------------------------------------------------------------- endless field */

hz_internal f32 scatter_density_scale(const ScatterSpeciesRt *sp, RendererQuality q) {
  const f32 *v = sp->density_scale.value;
  b32 any = false;
  for (u32 i = 0; i < RENDERER_QUALITY_COUNT; i++)
    any = any || v[i] != 0.0f;
  return any ? v[q] : 1.0f;
}

hz_internal void scatter_endless_rebuild(ScatterField *f, i32 cx, i32 cz) {
  ScatterEndless *e = f->endless;
  u64 t0 = os_time_now();
  ALLOC_RESET(&e->scratch_alloc);
  f32 reach = (f32)e->radius_cells * e->cell_size;
  v2 center = {((f32)cx + 0.5f) * e->cell_size, ((f32)cz + 0.5f) * e->cell_size};
  v2 half = {reach + e->cell_size * 0.5f, reach + e->cell_size * 0.5f};
  ScatterWindow win = {.min = v2_sub(center, half), .max = v2_add(center, half), .cell_x = cx, .cell_z = cz};

  ScatterSink sink = {.e = e, .grid = pg_scatter_grid_create(&e->scratch_alloc, win.min, win.max, SCATTER_GRID_CELL)};
  sink.placed = dyn_arr_new_alloc(&e->scratch_alloc, ScatterPlacement, 256);
  if (e->window)
    e->window(e->window_ctx, &win, &sink);

  RendererQuality q = (RendererQuality)f->applied_quality;
  PgScatterSpecies *pg = ALLOC_ARRAY(&e->scratch_alloc, PgScatterSpecies, f->species_count);
  for (u32 i = 0; i < f->species_count; i++) {
    pg[i] = e->pg[i];
    pg[i].density *= scatter_density_scale(&f->species[i], q);
  }
  PgScatterOut out = {0};
  pg_scatter(&(PgScatterDesc){.min = win.min,
                              .max = win.max,
                              .seed = e->seed,
                              .species = pg,
                              .species_count = f->species_count,
                              .height = e->height,
                              .normal = e->normal,
                              .surface_ctx = e->surface_ctx,
                              .grid = sink.grid},
             &e->scratch_alloc, &out);

  // placements grouped by species, each capped at its pool
  u32 *counts = ALLOC_ARRAY(&e->scratch_alloc, u32, f->species_count);
  for (u32 i = 0; i < sink.placed.len; i++)
    counts[sink.placed.items[i].species]++;
  for (u32 i = 0; i < f->species_count; i++)
    counts[i] += out.species_start[i + 1] - out.species_start[i];
  e->species_first[0] = 0;
  for (u32 i = 0; i < f->species_count; i++) {
    if (counts[i] > e->pool_size[i])
      log_warn("scatter: endless species % wants % placements, its pool holds %", fmt_u32(i), fmt_u32(counts[i]), fmt_u32(e->pool_size[i]));
    e->species_first[i + 1] = e->species_first[i] + MIN(counts[i], e->pool_size[i]);
  }
  u32 *filled = ALLOC_ARRAY(&e->scratch_alloc, u32, f->species_count);
  for (u32 i = 0; i < sink.placed.len; i++) {
    const ScatterPlacement *pl = &sink.placed.items[i];
    u32 s = pl->species;
    if (filled[s] < e->pool_size[s] && e->species_first[s] + filled[s] < e->species_first[s + 1])
      e->placements[e->species_first[s] + filled[s]++] = *pl;
  }
  for (u32 i = 0; i < out.count; i++) {
    ScatterPlacement pl = scatter_placement_from_point(&out.points[i], NULL);
    u32 s = pl.species;
    if (filled[s] < e->pool_size[s] && e->species_first[s] + filled[s] < e->species_first[s + 1])
      e->placements[e->species_first[s] + filled[s]++] = pl;
  }
  e->placed = true;
  e->cell_x = cx;
  e->cell_z = cz;
  e->generation++;
  e->rebuilds++;
  f32 us = (f32)os_ticks_to_ms(os_time_diff(os_time_now(), t0)) * 1000.0f;
  e->last_us = us;
  e->max_us = MAX(e->max_us, us);
}

/* ------------------------------------------------------------------ creation */

void scatter_init(EcsWorld *w) {
  // the world's resources are main-lane state until the first scope
  assert(is_main_thread());
  ScatterState st = {0};
  st.alloc = make_arena_allocator(arena_create(MB(2), KB(64)));
  st.fields = ALLOC_ARRAY(&st.alloc, ScatterField, SCATTER_FIELD_MAX);
  ecs_resource_set(w, ScatterState, st);
}

// the pool of an endless field: one entity per slot and part of each species' first variant
hz_internal void scatter_endless_spawn_pool(EcsWorld *w, RendererState *r, u32 field, const ScatterField *f, const ScatterEndless *e,
                                            const ScatterSpeciesDesc *species) {
  tctx_temp_allocator_scope(ta, NULL) {
    Allocator *a = &ta.allocator;
    u32 cap = 0;
    for (u32 i = 0; i < f->species_count; i++)
      cap += e->pool_size[i] * species[i].variants[0].part_count;
    ScatterRows rows = scatter_rows_alloc(a, cap, false);
    ScatterPoolSlot *slots = ALLOC_ARRAY(a, ScatterPoolSlot, cap);
    for (u32 i = 0; i < f->species_count; i++) {
      const ScatterVariant *v = &species[i].variants[0];
      for (u32 k = 0; k < e->pool_size[i]; k++) {
        for (u32 p = 0; p < v->part_count; p++) {
          slots[rows.count] = (ScatterPoolSlot){.field = field, .species = i, .slot = k};
          scatter_part_push(&rows, r, &v->parts[p], v->parts[p].material,
                            (WorldTransform){.rotation = quat_identity(), .scale = V3_ONE}, (ScatterRow){0}, false);
        }
      }
    }
    if (rows.count > 0) {
      EcsSpawnColumn *cols = ALLOC_ARRAY(a, EcsSpawnColumn, 4);
      cols[0] = (EcsSpawnColumn){.component = SCATTER_CID(w, WorldTransform), .data = rows.xfs};
      cols[1] = (EcsSpawnColumn){.component = SCATTER_CID(w, EcsSubMeshInstance), .data = rows.sis};
      cols[2] = (EcsSpawnColumn){.component = SCATTER_CID(w, EcsVisibility), .data = rows.vis};
      cols[3] = (EcsSpawnColumn){.component = SCATTER_CID(w, ScatterPoolSlot), .data = slots};
      ecs_spawn_block(w, &(EcsSpawnBlockDesc){.count = rows.count, .columns = cols, .column_count = 4});
    }
  }
}

u32 scatter_field_create(EcsWorld *w, RendererState *r, const ScatterFieldDesc *d) {
  // renderer instances and the world outside a scope are main-lane state
  assert(is_main_thread());
  ScatterState *s = scatter_state(w);
  assert_msg(s->field_count < SCATTER_FIELD_MAX, "scatter: more than % fields", fmt_u32(SCATTER_FIELD_MAX));
  assert_msg(d->species && d->species_count > 0 && d->species_count <= SCATTER_SPECIES_MAX, "scatter: % species",
             fmt_u32(d->species_count));
  u32 id = s->field_count++;
  ScatterField *f = &s->fields[id];
  *f = (ScatterField){.active = true,
                      .mode = d->mode,
                      .follow_camera = d->follow_camera,
                      .quality_override = d->quality_override,
                      .margin = d->lod_margin,
                      .cull_interval = d->cull_interval > 0.0f ? d->cull_interval : SCATTER_CULL_INTERVAL,
                      .cull_move = d->cull_move > 0.0f ? d->cull_move : SCATTER_CULL_MOVE,
                      .applied_quality = -1,
                      .force = true};
  scatter_copy_species(&s->alloc, f, d->species, d->species_count);
  scatter_apply_quality(f, scatter_quality_level(r, d->quality_override));
  if (d->mode == SCATTER_STATIC)
    return id;

  const ScatterEndlessDesc *ed = &d->endless;
  assert_msg(ed->cell_size > 0.0f && ed->radius_cells > 0 && ed->pg && ed->pool_size,
             "scatter: an endless field needs a cell size, a radius, pg species and pool sizes");
  ScatterEndless *e = ALLOC(&s->alloc, ScatterEndless);
  f->endless = e;
  e->arena = arena_create(MB(2), KB(64));
  e->alloc = make_arena_allocator(e->arena);
  e->scratch = arena_create(MB(8), KB(256));
  e->scratch_alloc = make_arena_allocator(e->scratch);
  e->cell_size = ed->cell_size;
  e->radius_cells = ed->radius_cells;
  e->seed = ed->seed;
  e->height = ed->height;
  e->normal = ed->normal;
  e->surface_ctx = ed->surface_ctx;
  e->window = ed->window;
  e->window_ctx = ed->window_ctx;
  e->pg = ALLOC_ARRAY(&e->alloc, PgScatterSpecies, d->species_count);
  e->pool_size = ALLOC_ARRAY(&e->alloc, u32, d->species_count);
  e->species_first = ALLOC_ARRAY(&e->alloc, u32, d->species_count + 1);
  u32 total = 0;
  for (u32 i = 0; i < d->species_count; i++) {
    e->pg[i] = ed->pg[i];
    e->pool_size[i] = ed->pool_size[i];
    total += ed->pool_size[i];
  }
  e->placements = ALLOC_ARRAY(&e->alloc, ScatterPlacement, total);
  scatter_endless_spawn_pool(w, r, id, f, e, d->species);
  return id;
}

/* ------------------------------------------------------------------ accessors */

void scatter_field_clear(ScatterState *s, u32 field) { scatter_field(s, field)->clearing = true; }

void scatter_field_set_quality(ScatterState *s, u32 field, i32 quality_override) {
  ScatterField *f = scatter_field(s, field);
  f->quality_override = quality_override;
  f->applied_quality = -1;
}

void scatter_field_set_focus(ScatterState *s, u32 field, v3 focus) {
  ScatterField *f = scatter_field(s, field);
  f->focus = focus;
  f->has_focus = true;
}

u32 scatter_field_row_count(const ScatterState *s, u32 field) { return scatter_field(s, field)->rows; }

const ScatterPlacement *scatter_field_placements(const ScatterState *s, u32 field, u32 *count) {
  const ScatterField *f = scatter_field(s, field);
  assert_msg(f->endless, "scatter: field % is not endless", fmt_u32(field));
  *count = f->endless->species_first[f->species_count];
  return f->endless->placements;
}

u32 scatter_field_rebuilds(const ScatterState *s, u32 field) {
  const ScatterEndless *e = scatter_field(s, field)->endless;
  return e ? e->rebuilds : 0;
}

void scatter_field_rebuild_cost(const ScatterState *s, u32 field, f32 *last_us, f32 *max_us) {
  const ScatterEndless *e = scatter_field(s, field)->endless;
  *last_us = e ? e->last_us : 0.0f;
  *max_us = e ? e->max_us : 0.0f;
}

/* -------------------------------------------------------------------- systems */

// the camera every following field measures from
ecs_system(scatter_camera_system, EcsIn(Camera), EcsResInOut(ScatterState)) {
  if (it->count == 0)
    return;
  ScatterState *s = ecs_sys_get_resource_mut(ScatterState);
  s->camera = ecs_sys_get_component(Camera, 0)->pos;
  s->has_camera = true;
}

// per field: the quality level, the throttle of a static field, the window of an endless one
ecs_system(scatter_tick_system, EcsResInOut(ScatterState), EcsResIn(RendererState)) {
  ScatterState *s = ecs_sys_get_resource_mut(ScatterState);
  const RendererState *r = ecs_sys_get_resource(RendererState);
  for (u32 i = 0; i < s->field_count; i++) {
    ScatterField *f = &s->fields[i];
    f->due = false;
    RendererQuality q = scatter_quality_level(r, f->quality_override);
    if ((i32)q != f->applied_quality)
      scatter_apply_quality(f, q);
    if (f->follow_camera && s->has_camera) {
      f->focus = s->camera;
      f->has_focus = true;
    }
    if (!f->has_focus)
      continue;
    if (f->mode == SCATTER_ENDLESS) {
      ScatterEndless *e = f->endless;
      i32 cx = (i32)m_floorf(f->focus.x / e->cell_size), cz = (i32)m_floorf(f->focus.z / e->cell_size);
      if (!e->placed || cx != e->cell_x || cz != e->cell_z || f->force) {
        scatter_endless_rebuild(f, cx, cz);
        f->force = false;
      }
      continue;
    }
    f->timer -= it->delta_time;
    v3 d = v3_sub(f->focus, f->last);
    if (!f->force && f->has_last && f->timer > 0.0f && v3_dot(d, d) < f->cull_move * f->cull_move)
      continue;
    f->timer = f->cull_interval;
    f->last = f->focus;
    f->has_last = true;
    f->force = false;
    f->cull_x = f->focus.x;
    f->cull_z = f->focus.z;
    f->due = true;
  }
}

// a static row inside its LOD's xz band; writes only the rows whose visibility flips
ecs_system(scatter_cull_system, EcsIn(ScatterRow), EcsInOut(EcsVisibility), EcsResIn(ScatterState)) {
  const ScatterState *s = ecs_sys_get_resource(ScatterState);
  for (i32 i = 0; i < it->count; i++) {
    const ScatterRow *row = ecs_sys_get_component(ScatterRow, i);
    const ScatterField *f = &s->fields[row->field];
    if (!f->due)
      continue;
    f32 dx = row->x - f->cull_x, dz = row->z - f->cull_z;
    f32 d2 = dx * dx + dz * dz;
    u32 at = row->species * SCATTER_LOD_MAX + row->lod;
    b32 visible = d2 >= f->min_sq[at] && d2 < f->max_sq[at];
    if (ecs_sys_get_component(EcsVisibility, i)->visible != visible)
      ecs_sys_get_component_mut(EcsVisibility, i)->visible = visible;
  }
}

// pooled slots of an endless field take the new window's placements, the unused ones hide
ecs_system(scatter_pool_system, EcsInOut(ScatterPoolSlot), EcsInOut(WorldTransform), EcsInOut(EcsVisibility), EcsResIn(ScatterState)) {
  const ScatterState *s = ecs_sys_get_resource(ScatterState);
  for (i32 i = 0; i < it->count; i++) {
    const ScatterPoolSlot *slot = ecs_sys_get_component(ScatterPoolSlot, i);
    const ScatterEndless *e = s->fields[slot->field].endless;
    if (slot->applied == e->generation)
      continue;
    u32 first = e->species_first[slot->species], count = e->species_first[slot->species + 1] - first;
    b32 used = slot->slot < count;
    if (used) {
      const ScatterPlacement *p = &e->placements[first + slot->slot];
      *ecs_sys_get_component_mut(WorldTransform, i) =
          (WorldTransform){.position = p->position, .rotation = p->rotation, .scale = p->scale};
    }
    if (ecs_sys_get_component(EcsVisibility, i)->visible != used)
      ecs_sys_get_component_mut(EcsVisibility, i)->visible = used;
    ecs_sys_get_component_mut(ScatterPoolSlot, i)->applied = e->generation;
  }
}

ecs_system(scatter_row_clear_system, EcsIn(ScatterRow), EcsResIn(ScatterState)) {
  const ScatterState *s = ecs_sys_get_resource(ScatterState);
  for (i32 i = 0; i < it->count; i++)
    if (s->fields[ecs_sys_get_component(ScatterRow, i)->field].clearing)
      ecs_entity_destroy(it->world, it->entities[i]);
}

ecs_system(scatter_collision_clear_system, EcsIn(ScatterCollision), EcsResIn(ScatterState)) {
  const ScatterState *s = ecs_sys_get_resource(ScatterState);
  for (i32 i = 0; i < it->count; i++)
    if (s->fields[ecs_sys_get_component(ScatterCollision, i)->field].clearing)
      ecs_entity_destroy(it->world, it->entities[i]);
}

// the clear request lasts one frame, after the clear systems ran
ecs_system(scatter_clear_done_system, EcsResInOut(ScatterState)) {
  ScatterState *s = ecs_sys_get_resource_mut(ScatterState);
  for (u32 i = 0; i < s->field_count; i++) {
    if (!s->fields[i].clearing)
      continue;
    s->fields[i].clearing = false;
    s->fields[i].rows = 0;
  }
}

void scatter_schedule(EcsWorld *w) {
  ecs_schedule_single(w, scatter_camera_system); // writes the resource (one camera)
  ecs_schedule_single(w, scatter_tick_system);   // one throttle per field, one window rebuild
  ecs_schedule(w, scatter_cull_system);
  ecs_schedule(w, scatter_pool_system);
  ecs_schedule(w, scatter_row_clear_system);
  ecs_schedule(w, scatter_collision_clear_system);
  ecs_schedule_single(w, scatter_clear_done_system);
}
