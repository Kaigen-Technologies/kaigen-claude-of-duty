// Claude of Duty soldier bodies: procedural mesh, textures and skin, the animator, hitbox bones and ragdolls.

// the body rng stream: seeded 0xa11ce, one fork for the materials, then one fork per variant and one per animator
#define CAB_SEED 0xa11ceu

// ---- build: every lane takes whole parts (cod_ai_body_lanes) ----

hz_internal void cab_build_job(CabState *st, u32 job, ArenaAllocator *arena) {
  CabVariant *v = st->variant[job >> 16];
  CabSpec *s = &v->spec[job & 0xffffu];
  if (s->kind == CAB_PART_WEAPON) {
    cab_weapon_build(&v->nz, v, arena);
    return;
  }
  s->mesh = cab_mesh(arena);
  cab_part_build(&v->nz, v, s, s->mesh, arena);
}

hz_internal void cab_build_lanes(CabState *st) {
  ArenaAllocator *arena = st->lane_arena[tctx_current()->thread_idx];
  if (is_main_thread())
    st->build_t0 = os_time_now();
  Range(u32) rg = lane_range(st->job_count);
  for (u32 j = rg.min; j < rg.max; j++)
    cab_build_job(st, st->job[j], arena);
  lane_sync();
  // the part sizes are known: the main lane lays out the merged arrays
  if (is_main_thread()) {
    st->geo_ms = os_ticks_to_ms(os_time_diff(os_time_now(), (u64)st->build_t0));
    for (u32 i = 0; i < CAB_VARIANTS; i++)
      cab_variant_layout(&st->persist, st->variant[i]);
  }
  lane_sync();
  Range(u32) mr = lane_range(st->merge_count);
  for (u32 j = mr.min; j < mr.max; j++) {
    CabVariant *v = st->variant[st->merge[j] >> 16];
    cab_merge_part(st, v, &v->spec[v->order[st->merge[j] & 0xffffu]]);
  }
  lane_sync();
  if (is_main_thread()) {
    st->build_ms = os_ticks_to_ms(os_time_diff(os_time_now(), (u64)st->build_t0));
    for (u32 i = 0; i < CAB_VARIANTS; i++) {
      CabVariant *v = st->variant[i];
      aabb b = aabb_invalidate();
      for (u32 k = 0; k < v->vertex_count; k++)
        b = aabb_encapsulate_point(b, v->pos[k]);
      v->bounds = b;
    }
  }
  lane_sync();
}

hz_internal void cab_tex_wide(void *arg) { cab_tex_lanes((CabTexState *)arg); }

// ---- entry points ----

void cod_ai_body_init(EcsWorld *w) {
  Allocator persist = make_arena_allocator(arena_create(MB(64), MB(1)));
  CabState *st = ALLOC(&persist, CabState);
  st->world = w;
  st->persist = persist;
  cab_rig_init(&st->rig);
  cab_occluders(st->occ, &st->occ_count);
  st->lane_count = tctx_current()->thread_count;
  st->lane_arena = ALLOC_ARRAY(&persist, ArenaAllocator *, st->lane_count);
  for (u32 i = 0; i < st->lane_count; i++)
    st->lane_arena[i] = arena_create(GB(1), MB(8));

  // the stream order: the materials fork, then each variant's build fork and its animator fork
  CodRng rng = cod_rng_make(CAB_SEED);
  CodRng mat_rng = cod_rng_fork(&rng);
  CodRng tile_rng = cod_rng_fork(&mat_rng);
  st->tex = ALLOC(&persist, CabTexState);
  cab_tex_init(st->tex, &persist, &tile_rng);
  for (u32 i = 0; i < CAB_VARIANTS; i++) {
    CabVariant *v = ALLOC(&persist, CabVariant);
    v->index = i;
    v->def = cab_variant_def(i);
    v->rng = cod_rng_fork(&rng);
    (void)cod_rng_fork(&rng);
    CodRng nrng = cod_rng_fork(&v->rng);
    cab_noise_init(&v->nz, &nrng, &persist);
    // the rng draws the soldier build makes while it adds the pouches, then the rifle's cant
    for (u32 p = 0; p < (v->def->full_carrier ? 3u : 2u); p++) {
      v->pouch_y[p] = cod_rng_range(&v->rng, -0.006f, 0.006f);
      v->pouch_rz[p] = cod_rng_range(&v->rng, -0.05f, 0.05f);
    }
    v->cant = cod_rng_range(&v->rng, -0.10f, -0.03f);
    cab_variant_specs(v);
    cab_variant_anchors(&st->rig, v);
    st->variant[i] = v;
    for (u32 k = 0; k < v->spec_count; k++)
      st->job_count += v->spec[k].job ? 1u : 0u;
    st->merge_count += v->order_count;
  }
  st->job = ALLOC_ARRAY(&persist, u32, st->job_count);
  st->merge = ALLOC_ARRAY(&persist, u32, st->merge_count);
  u32 nj = 0, nm = 0;
  for (u32 i = 0; i < CAB_VARIANTS; i++) {
    CabVariant *v = st->variant[i];
    for (u32 k = 0; k < v->spec_count; k++)
      if (v->spec[k].job)
        st->job[nj++] = i << 16 | k;
    for (u32 k = 0; k < v->order_count; k++)
      st->merge[nm++] = i << 16 | k;
  }
  AssetSystem *assets = ecs_resource_get_mut(w, AssetSystem);
  st->base_asset = asset_load(MaterialBlobAsset, assets, .path = "claude_of_duty/soldiers/cod_ai_body.hzt");
  ecs_component_set_dtor(w, CabAnim, cab_anim_dtor);
  st->ground_query = ecs_query_cached(w, EcsIn(CodSoldierDrive), EcsIn(CodSoldierPose), EcsInOut(CabAnim));
  st->ground_asset = asset_load(MaterialBlobAsset, assets, .path = "claude_of_duty/soldiers/cod_ai_ground.hzt");
  ecs_resource_set(w, CodBodyState, ((CodBodyState){.s = st}));
  ecs_resource_set(w, CodBodyProgress, ((CodBodyProgress){0}));
}

void cod_ai_body_schedule(EcsWorld *w) {
  ecs_schedule_single(w, cab_attach_system);  // instantiating mutates the renderer's shared instance tables
  ecs_schedule_single(w, cab_ragdoll_system); // creates the bodies, joints and entities of a new ragdoll
  ecs_schedule_unsafe_resource_access(w, cab_pose_system); // per soldier in parallel: palette and transform writes touch only that soldier's instances (disjoint ranges, atomic dirty marks)
  ecs_schedule_single(w, cab_ragdoll_gc_system); // destroys the bodies and joints whose soldier is gone
}

void cod_ai_body_lanes(EcsWorld *w) {
  CabState *st = ecs_resource_get(w, CodBodyState)->s;
  if (st->stage != CAB_STAGE_BUILD)
    return;
  cab_build_lanes(st);
  // row bakes with no per-lane storage: every thread, not just the default lanes
  lane_scope_run(cab_tex_wide, st->tex);
  if (is_main_thread())
    st->stage = CAB_STAGE_UPLOAD;
  lane_sync();
}

void cod_ai_body_frame(EcsWorld *w) {
  CabState *st = ecs_resource_get(w, CodBodyState)->s;
  if (st->stage == CAB_STAGE_UPLOAD) {
    if (!st->logged) {
      st->logged = true;
      for (u32 i = 0; i < CAB_VARIANTS; i++) {
        CabVariant *v = st->variant[i];
        log_info("soldier %: % verts % tris", fmt_cstr(v->def->name), fmt_u32(v->vertex_count), fmt_u32(v->index_count / 3));
      }
      log_info("soldiers: geometry % ms, merged and shaded in % ms total, textures % ms", fmt_f64_p(st->geo_ms, 1), fmt_f64_p(st->build_ms, 1),
               fmt_f64_p(st->tex->ms, 1));
    }
    cab_render_stage(w, st);
  }
  if (st->ready)
    cab_ground_frame(w, st);
}
