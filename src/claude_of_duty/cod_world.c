// Claude of Duty world: the procedural market street, its colliders, practicals and spawns; the RNG stream records once, the rest runs on every lane.

#define CW_MAX_LANES 128u
#define CW_TILE_N 6u
#define CW_TILE_SIZE 48.0f
#define CW_TILE_ORG 144.0f
#define CW_CHUNK_MAX_V 65000u
#define CW_WORLD_ABS_LIMIT 108.0f

typedef enum CwStage {
  CWB_RECORD = 0,
  CWB_EXPAND,
  CWB_PLAN,
  CWB_FILL,
  CWB_BLOB,
  CWB_UPLOAD,
  CWB_DONE,
} CwStage;

// record steps: props, ground, one per building, gate, perimeter, street dressing, building dressing, scatter, lights
#define CW_STEP_PROPS 0u
#define CW_STEP_GROUND 1u
#define CW_STEP_BUILDINGS 2u
#define CW_STEP_GATE (CW_STEP_BUILDINGS + CW_BUILDING_COUNT)
#define CW_STEP_PERIMETER (CW_STEP_GATE + 1)
#define CW_STEP_STREET (CW_STEP_GATE + 2)
#define CW_STEP_DRESS (CW_STEP_GATE + 3)
#define CW_STEP_SCATTER (CW_STEP_GATE + 4)
#define CW_STEP_COUNT (CW_STEP_GATE + 5)

typedef struct CwChunk {
  u32 key;
  b32 no_shadow;
  CwGeo g;
  HzHandleT(RendererSubMesh) sub;
} CwChunk;
DYN_ARRAY_TYPEDEF(CwChunk)

typedef struct CwColUnit {
  u32 group;
  u32 item; // box index, or ccmd index with CW_UNIT_CCMD
  u32 tri;
  u32 voff, ioff;
} CwColUnit;
DYN_ARRAY_TYPEDEF(CwColUnit)

#define CW_UNIT_CCMD 0x80000000u

typedef struct CwColGroup {
  u32 surface;
  u32 nv, ni;
  v3 *verts;
  i32 *idx;
  TriMeshColliderBlob *blob;
} CwColGroup;
DYN_ARRAY_TYPEDEF(CwColGroup)

typedef struct CodWorldBuild {
  ArenaAllocator *arena;
  Allocator al;
  CwA *A;
  CodRng rng;
  CwBuildInfo *infos[32];
  u32 info_count;
  u32 stage;
  u32 step;
  ArenaAllocator *lane_geo[CW_MAX_LANES];
  ArenaAllocator *lane_blob[CW_MAX_LANES];
  // plan outputs
  u32 *cmd_chunk, *cmd_voff, *cmd_ioff;
  u32 *chunk_of; // per (key, casts shadow, tile)
  DynArray(CwChunk) chunks;
  DynArray(CwColUnit) units;
  DynArray(CwColGroup) groups;
  // upload cursors
  u32 up_chunk, up_proto, up_inst, up_group, up_lights;
  b32 protos_uploaded;
  HzHandleT(RendererSubMesh) proto_sub[CWP_COUNT];
  HzHandleT(RendererMaterial) key_mat[CW_KEY_COUNT];
  b32 key_mat_ready[CW_KEY_COUNT];
  // timing
  u64 t_total, t_phase;
  f64 ms_props, ms_ground, ms_buildings, ms_gate, ms_perimeter, ms_street, ms_dress, ms_scatter, ms_expand, ms_plan, ms_fill, ms_blob, ms_upload;
  u64 t_upload_first, upload_work_ticks; // the first upload frame; ticks spent uploading
  u32 upload_frames;
  f32 progress;
  u32 collide_tris;
} CodWorldBuild;

// ------------------------------------------------------------ expansion ------

// ground patches and wall runoff lie ~1 cm over the surface they dress: as casters they only self-shadow it, by a coin flip of bias and texel size
hz_internal b32 cw_spec_is_overlay(u32 kind) { return kind == CWS_PATCH || kind == CWS_RUNOFF; }

// expands one deferred geometry on `al`; pure of its spec, so any lane can run it
hz_internal CwGeo cw_expand_spec(const CwSpec *s, Allocator *al) {
  switch (s->kind) {
  case CWS_CLOTH: return cw_cloth(al, &s->cloth);
  case CWS_RUNOFF: return cw_runoff(al, s->runoff.width, s->runoff.len, s->runoff.amount, s->runoff.cols, s->runoff.rows, s->runoff.wander, s->runoff.seed);
  case CWS_PATCH: return cw_patch(al, s->patch.radii, s->patch.lobes, s->patch.sag);
  case CWS_DRIFT: return cw_drift_berm(al, s->drift.len, s->drift.w, s->drift.h, s->drift.nz, s->drift.seed);
  case CWS_ROCK: return cw_rock(al, s->rock.size, s->rock.detail, s->rock.squash, s->rock.seed);
  case CWS_SPALL: return cw_spall(al, s->spall.w, s->spall.h, s->spall.depth);
  case CWS_PANEL: return cw_wall_panel(al, &s->panel);
  case CWS_CATENARY: return cw_catenary(al, s->cat.from, s->cat.to, s->cat.sag, s->cat.radius, s->cat.seg, s->cat.radial, s->cat.jitter);
  case CWS_TERRAIN: return cw_make_terrain(al);
  case CWS_ROAD: return cw_make_road(al);
  case CWS_CUSHION: {
    CwGeo g = cw_chamfer_box(al, s->cushion.sx, s->cushion.sy, s->cushion.sz, s->cushion.bevel);
    cw_geo_fill_masks(&g, s->cushion.fill.x, s->cushion.fill.y, s->cushion.fill.z);
    return g;
  }
  default: assert_msg(false, "cw: unknown deferred geometry");
  }
  return (CwGeo){0};
}

hz_internal ArenaAllocator *cw_lane_arena(ArenaAllocator **slots, u32 lane, size_t reserve) {
  assert_msg(lane < CW_MAX_LANES, "cw: lane index");
  if (!slots[lane])
    slots[lane] = arena_create(reserve, MB(1));
  return slots[lane];
}

#define CW_BUILD_OF(it) (ecs_sys_get_resource_mut(CodWorld)->build)

// every lane expands its slice of the deferred geometries
ecs_system(cod_world_expand_system, EcsResInOut(CodWorld)) {
  CodWorldBuild *b = CW_BUILD_OF(it);
  CwA *A = b->A;
  Allocator al = make_arena_allocator(cw_lane_arena(b->lane_geo, it->task_thread_idx, MB(768)));
  Range(u32) r = lane_range_for(it->task_thread_idx, it->task_thread_count, A->geos.len);
  for (u32 i = r.min; i < r.max; i++) {
    CwSlot *s = &A->geos.items[i];
    if (s->spec != ~0u)
      s->g = cw_expand_spec(&A->specs.items[s->spec], &al);
  }
}

// ----------------------------------------------------------------- plan -----

hz_internal u32 cw_tile_of(f32 x, f32 z) {
  i32 tx = (i32)m_floor((x + CW_TILE_ORG) / CW_TILE_SIZE), tz = (i32)m_floor((z + CW_TILE_ORG) / CW_TILE_SIZE);
  tx = tx < 0 ? 0 : (tx >= (i32)CW_TILE_N ? (i32)CW_TILE_N - 1 : tx);
  tz = tz < 0 ? 0 : (tz >= (i32)CW_TILE_N ? (i32)CW_TILE_N - 1 : tz);
  return (u32)tz * CW_TILE_N + (u32)tx;
}

hz_internal void cw_plan_statics(CodWorldBuild *b) {
  CwA *A = b->A;
  u32 n = A->cmds.len;
  b->cmd_chunk = ALLOC_ARRAY_NO_ZERO(&b->al, u32, n ? n : 1);
  b->cmd_voff = ALLOC_ARRAY_NO_ZERO(&b->al, u32, n ? n : 1);
  b->cmd_ioff = ALLOC_ARRAY_NO_ZERO(&b->al, u32, n ? n : 1);
  b->chunks = dyn_arr_new_alloc(&b->al, CwChunk, 1024);
  u32 slots = 2u * CW_KEY_COUNT * CW_TILE_N * CW_TILE_N;
  b->chunk_of = ALLOC_ARRAY_NO_ZERO(&b->al, u32, slots);
  for (u32 i = 0; i < slots; i++)
    b->chunk_of[i] = ~0u;
  for (u32 c = 0; c < n; c++) {
    const CwCmd *cmd = &A->cmds.items[c];
    const CwGeo *g = &A->geos.items[cmd->geo].g;
    if (g->nv == 0 || g->ni == 0) {
      b->cmd_chunk[c] = ~0u;
      continue;
    }
    m4 m = m4_mul(A->xform, cmd->xf);
    u32 spec = A->geos.items[cmd->geo].spec;
    b32 no_shadow = spec != ~0u && cw_spec_is_overlay(A->specs.items[spec].kind);
    u32 slot = ((u32)cmd->key * 2u + (no_shadow ? 1u : 0u)) * CW_TILE_N * CW_TILE_N + cw_tile_of(m.m[3][0], m.m[3][2]);
    u32 ch = b->chunk_of[slot];
    if (ch == ~0u || b->chunks.items[ch].g.nv + g->nv > CW_CHUNK_MAX_V) {
      ch = b->chunks.len;
      dyn_arr_append(b->chunks, ((CwChunk){.key = cmd->key, .no_shadow = no_shadow}));
      b->chunk_of[slot] = ch;
    }
    CwChunk *c2 = &b->chunks.items[ch];
    b->cmd_chunk[c] = ch;
    b->cmd_voff[c] = c2->g.nv;
    b->cmd_ioff[c] = c2->g.ni;
    c2->g.nv += g->nv;
    c2->g.ni += g->ni;
  }
  for (u32 i = 0; i < b->chunks.len; i++) {
    CwGeo *g = &b->chunks.items[i].g;
    g->cap_v = g->nv;
    g->cap_i = g->ni;
    g->pos = ALLOC_ARRAY_NO_ZERO(&b->al, v3, g->nv);
    g->nrm = ALLOC_ARRAY_NO_ZERO(&b->al, v3, g->nv);
    g->uv = ALLOC_ARRAY_NO_ZERO(&b->al, v2, g->nv);
    g->col = ALLOC_ARRAY_NO_ZERO(&b->al, v3, g->nv);
    g->idx = ALLOC_ARRAY_NO_ZERO(&b->al, u32, g->ni);
    g->has_col = true;
  }
}

hz_internal u32 cw_group_of(CodWorldBuild *b, u32 surface, u32 tile) {
  (void)b;
  return surface * CW_TILE_N * CW_TILE_N + tile;
}

hz_internal void cw_plan_collision(CodWorldBuild *b) {
  CwA *A = b->A;
  u32 slots = (u32)COD_SURFACE_COUNT * CW_TILE_N * CW_TILE_N;
  b->groups = dyn_arr_new_alloc(&b->al, CwColGroup, slots);
  for (u32 i = 0; i < slots; i++)
    dyn_arr_append(b->groups, ((CwColGroup){.surface = i / (CW_TILE_N * CW_TILE_N)}));
  b->units = dyn_arr_new_alloc(&b->al, CwColUnit, A->boxes.len + 8192);

  for (u32 i = 0; i < A->boxes.len; i++) {
    const CwBoxProxy *bx = &A->boxes.items[i];
    m4 m = m4_mul(A->xform, bx->xf);
    if (m_fabsf(m.m[3][0]) > CW_WORLD_ABS_LIMIT || m_fabsf(m.m[3][2]) > CW_WORLD_ABS_LIMIT)
      continue;
    u32 gi = cw_group_of(b, bx->surface, cw_tile_of(m.m[3][0], m.m[3][2]));
    CwColGroup *g = &b->groups.items[gi];
    dyn_arr_append(b->units, ((CwColUnit){.group = gi, .item = i, .tri = ~0u, .voff = g->nv, .ioff = g->ni}));
    g->nv += 8;
    g->ni += 36;
  }
  for (u32 ci = 0; ci < A->ccmds.len; ci++) {
    const CwCmd *cc = &A->ccmds.items[ci];
    const CwGeo *geo = &A->geos.items[cc->geo].g;
    m4 m = m4_mul(A->xform, cc->xf);
    for (u32 t = 0; t < geo->ni / 3; t++) {
      v3 p0 = m4_mulv3(m, geo->pos[geo->idx[t * 3]], 1.0f);
      v3 p1 = m4_mulv3(m, geo->pos[geo->idx[t * 3 + 1]], 1.0f);
      v3 p2 = m4_mulv3(m, geo->pos[geo->idx[t * 3 + 2]], 1.0f);
      f32 ext = m_maxf(m_maxf(m_fabsf(p0.x), m_fabsf(p1.x)), m_maxf(m_fabsf(p2.x), m_maxf(m_fabsf(p0.z), m_maxf(m_fabsf(p1.z), m_fabsf(p2.z)))));
      if (ext > CW_WORLD_ABS_LIMIT)
        continue;
      u32 gi = cw_group_of(b, cc->key, cw_tile_of((p0.x + p1.x + p2.x) / 3.0f, (p0.z + p1.z + p2.z) / 3.0f));
      CwColGroup *g = &b->groups.items[gi];
      dyn_arr_append(b->units, ((CwColUnit){.group = gi, .item = ci | CW_UNIT_CCMD, .tri = t, .voff = g->nv, .ioff = g->ni}));
      g->nv += 3;
      g->ni += 3;
    }
  }
  for (u32 i = 0; i < b->groups.len; i++) {
    CwColGroup *g = &b->groups.items[i];
    if (g->nv == 0)
      continue;
    g->verts = ALLOC_ARRAY_NO_ZERO(&b->al, v3, g->nv);
    g->idx = ALLOC_ARRAY_NO_ZERO(&b->al, i32, g->ni);
    b->collide_tris += g->ni / 3;
  }
}

ecs_system(cod_world_plan_system, EcsResInOut(CodWorld)) {
  CodWorldBuild *b = CW_BUILD_OF(it);
  cw_plan_statics(b);
  cw_plan_collision(b);
}

// ----------------------------------------------------------------- fill -----

// the facade wall's masks: extra grime toward the base of the ground floor
hz_internal void cw_paint_facade(f32 px, f32 py, f32 pz, f32 base_on, f32 *out) {
  f32 base = base_on > 0.0f ? m_maxf(0.0f, 1.0f - py / 1.4f) : 0.0f;
  f32 n = cw_fbm3((f64)px * 0.7, (f64)py * 0.7, (f64)pz * 0.7, 2);
  out[1] = m_minf(1.0f, out[1] + base * base * 0.55f * (0.5f + n));
  out[2] = m_minf(1.0f, out[2] + base * base * 0.4f);
}

hz_internal void cw_paint_partition(f32 py, f32 *out) {
  f32 base = m_maxf(0.0f, 1.0f - py / 1.1f);
  out[1] = m_minf(1.0f, out[1] + base * base * 0.5f);
  out[2] = m_minf(1.0f, out[2] + base * base * 0.35f);
}

// the geometry through the level transform into its chunk, masks maxed in, paint applied in world space
hz_internal void cw_fill_cmd(CodWorldBuild *b, u32 c) {
  CwA *A = b->A;
  u32 ch = b->cmd_chunk[c];
  if (ch == ~0u)
    return;
  const CwCmd *cmd = &A->cmds.items[c];
  const CwGeo *g = &A->geos.items[cmd->geo].g;
  CwGeo *out = &b->chunks.items[ch].g;
  u32 vo = b->cmd_voff[c], io = b->cmd_ioff[c];
  m4 m = m4_mul(A->xform, cmd->xf);
  m3 nm = cw_normal_matrix(m);
  for (u32 v = 0; v < g->nv; v++) {
    v3 p = m4_mulv3(m, g->pos[v], 1.0f);
    v3 n = v3_normalize(m3_mulv3(nm, g->nrm[v]));
    v3 col = g->has_col ? g->col[v] : V3_ZERO;
    if (cmd->has_masks)
      col = (v3){m_maxf(col.x, cmd->masks.x), m_maxf(col.y, cmd->masks.y), m_maxf(col.z, cmd->masks.z)};
    if (cmd->paint == CW_PAINT_FACADE)
      cw_paint_facade(p.x, p.y, p.z, cmd->paint_p, &col.x);
    else if (cmd->paint == CW_PAINT_PARTITION)
      cw_paint_partition(p.y, &col.x);
    out->pos[vo + v] = p;
    out->nrm[vo + v] = n;
    out->uv[vo + v] = g->uv[v];
    out->col[vo + v] = col;
  }
  for (u32 i = 0; i < g->ni; i++)
    out->idx[io + i] = vo + g->idx[i];
}

hz_internal const f32 CW_CUBE_V[8][3] = {{-0.5f, -0.5f, -0.5f}, {0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, -0.5f}, {-0.5f, 0.5f, -0.5f},
                                         {-0.5f, -0.5f, 0.5f},  {0.5f, -0.5f, 0.5f},  {0.5f, 0.5f, 0.5f},  {-0.5f, 0.5f, 0.5f}};
hz_internal const u8 CW_CUBE_I[36] = {0, 2, 1, 0, 3, 2, 4, 5, 6, 4, 6, 7, 0, 1, 5, 0, 5, 4, 3, 7, 6, 3, 6, 2, 0, 4, 7, 0, 7, 3, 1, 2, 6, 1, 6, 5};

hz_internal void cw_fill_unit(CodWorldBuild *b, u32 ui) {
  CwA *A = b->A;
  const CwColUnit *u = &b->units.items[ui];
  CwColGroup *g = &b->groups.items[u->group];
  if (u->item & CW_UNIT_CCMD) {
    const CwCmd *cc = &A->ccmds.items[u->item & ~CW_UNIT_CCMD];
    const CwGeo *geo = &A->geos.items[cc->geo].g;
    m4 m = m4_mul(A->xform, cc->xf);
    for (u32 k = 0; k < 3; k++) {
      g->verts[u->voff + k] = m4_mulv3(m, geo->pos[geo->idx[u->tri * 3 + k]], 1.0f);
      g->idx[u->ioff + k] = (i32)(u->voff + k);
    }
    return;
  }
  m4 m = m4_mul(A->xform, A->boxes.items[u->item].xf);
  for (u32 k = 0; k < 8; k++)
    g->verts[u->voff + k] = m4_mulv3(m, (v3){CW_CUBE_V[k][0], CW_CUBE_V[k][1], CW_CUBE_V[k][2]}, 1.0f);
  for (u32 k = 0; k < 36; k++)
    g->idx[u->ioff + k] = (i32)(u->voff + CW_CUBE_I[k]);
}

ecs_system(cod_world_fill_system, EcsResInOut(CodWorld)) {
  CodWorldBuild *b = CW_BUILD_OF(it);
  Range(u32) cr = lane_range_for(it->task_thread_idx, it->task_thread_count, b->A->cmds.len);
  for (u32 c = cr.min; c < cr.max; c++)
    cw_fill_cmd(b, c);
  Range(u32) ur = lane_range_for(it->task_thread_idx, it->task_thread_count, b->units.len);
  for (u32 u = ur.min; u < ur.max; u++)
    cw_fill_unit(b, u);
}

// each lane builds the trimesh blobs of its slice of groups
ecs_system(cod_world_blob_system, EcsResInOut(CodWorld)) {
  CodWorldBuild *b = CW_BUILD_OF(it);
  Allocator al = make_arena_allocator(cw_lane_arena(b->lane_blob, it->task_thread_idx, MB(256)));
  Range(u32) r = lane_range_for(it->task_thread_idx, it->task_thread_count, b->groups.len);
  for (u32 i = r.min; i < r.max; i++) {
    CwColGroup *g = &b->groups.items[i];
    if (g->nv)
      g->blob = trimesh_blob_build(g->verts, g->idx, (i32)(g->ni / 3), &al);
  }
}

// --------------------------------------------------------------- record -----

hz_internal f64 cw_phase_ms(CodWorldBuild *b) {
  u64 now = os_time_now();
  f64 ms = os_ticks_to_ms(os_time_diff(now, b->t_phase));
  b->t_phase = now;
  return ms;
}

hz_internal void cw_record_step(CodWorldBuild *b) {
  CwA *A = b->A;
  u32 step = b->step;
  b->t_phase = os_time_now();
  if (step == CW_STEP_PROPS) {
    cw_register_props(A, &b->rng);
    b->ms_props = cw_phase_ms(b);
    for (u32 i = 0; i < CWP_COUNT; i++) {
      const CwProto *p = &A->protos[i];
      for (u32 l = 1; l < p->lod_count; l++)
        log_info("world: % LOD% % -> % tris, error % mm against the source (this step % mm) in % ms", fmt_cstr(CW_PROTO_NAMES[i]), fmt_u32(l),
                 fmt_u32(p->lod_simp[l - 1].tris_before), fmt_u32(p->lod_simp[l - 1].tris_after), fmt_f32_p(p->lod_err[l] * 1000.0f, 2),
                 fmt_f32_p(p->lod_simp[l - 1].error * 1000.0f, 2), fmt_f64_p(p->lod_simp[l - 1].ms, 2));
      if (p->simplify > 0.0f)
        log_info("world: % decimated % -> % tris, % -> % verts, error % mm (limit % mm) in % ms", fmt_cstr(CW_PROTO_NAMES[i]), fmt_u32(p->simp.tris_before),
                 fmt_u32(p->simp.tris_after), fmt_u32(p->simp.verts_before), fmt_u32(p->simp.verts_after), fmt_f32_p(p->simp.error * 1000.0f, 2),
                 fmt_f32_p(p->simplify * 1000.0f, 2), fmt_f64_p(p->simp.ms, 2));
    }
  } else if (step == CW_STEP_GROUND) {
    cw_build_ground(A, &b->rng);
    b->ms_ground = cw_phase_ms(b);
  } else if (step < CW_STEP_GATE) {
    u32 bi = step - CW_STEP_BUILDINGS;
    const CwBuildingSpec *spec = &CW_BUILDINGS[bi];
    CwBuildInfo *info = cw_build_building(A, &b->rng, spec);
    b->infos[bi] = info;
    b->info_count = bi + 1;
    if (spec->collapse) {
      f32 hx = spec->x + cod_rng_range(&b->rng, -2.0f, 2.0f);
      f32 hz = spec->z + cod_rng_range(&b->rng, -2.0f, 2.0f);
      cw_collapse_roof(A, &b->rng, info, hx, hz);
    }
    b->ms_buildings += cw_phase_ms(b);
  } else if (step == CW_STEP_GATE) {
    cw_build_gate(A, &b->rng);
    b->ms_gate = cw_phase_ms(b);
  } else if (step == CW_STEP_PERIMETER) {
    cw_build_perimeter(A, &b->rng);
    b->ms_perimeter = cw_phase_ms(b);
  } else if (step == CW_STEP_STREET) {
    cw_dress_street(A, &b->rng);
    b->ms_street = cw_phase_ms(b);
  } else if (step == CW_STEP_DRESS) {
    cw_dress_buildings(A, &b->rng, b->infos, b->info_count);
    b->ms_dress = cw_phase_ms(b);
  } else if (step == CW_STEP_SCATTER) {
    cw_scatter_debris(A, &b->rng);
    b->ms_scatter = cw_phase_ms(b);
  }
  b->step++;
  b->progress = 0.6f * (f32)b->step / (f32)CW_STEP_COUNT;
  if (b->step == CW_STEP_COUNT)
    b->stage = CWB_EXPAND;
}

ecs_system(cod_world_record_system, EcsResInOut(CodWorld)) {
  CodWorldBuild *b = CW_BUILD_OF(it);
  assert_single_thread_begin("cod_world_record");
  cw_record_step(b);
  assert_single_thread_end();
}

// closes the stage a parallel system just ran
ecs_system(cod_world_advance_system, EcsResInOut(CodWorld)) {
  CodWorldBuild *b = CW_BUILD_OF(it);
  u64 now = os_time_now();
  f64 ms = os_ticks_to_ms(os_time_diff(now, b->t_phase));
  b->t_phase = now;
  switch (b->stage) {
  case CWB_EXPAND: b->ms_expand = ms; b->progress = 0.65f; break;
  case CWB_PLAN: b->ms_plan = ms; b->progress = 0.7f; break;
  case CWB_FILL:
    b->ms_fill = ms;
    b->progress = 0.8f;
    for (u32 i = 0; i < CW_MAX_LANES; i++) {
      if (b->lane_geo[i])
        arena_destroy(b->lane_geo[i]);
      b->lane_geo[i] = NULL;
    }
    break;
  case CWB_BLOB: b->ms_blob = ms; b->progress = 0.85f; break;
  default: break;
  }
  b->stage++;
}

// ------------------------------------------------------- per-frame systems ---

// props past their distance are hidden: one distance test per instance
ecs_system(cod_world_cull_system, EcsIn(WorldTransform), EcsIn(CodPropCull), EcsInOut(EcsVisibility), EcsResIn(CodWorld)) {
  v3 eye = ecs_sys_get_resource(CodWorld)->cull_eye;
  for (i32 i = 0; i < it->count; i++) {
    const WorldTransform *wt = ecs_sys_get_component(WorldTransform, i);
    const CodPropCull *pc = ecs_sys_get_component(CodPropCull, i);
    v3 d = v3_sub(wt->position, eye);
    f32 lim = pc->max_dist + pc->radius;
    b32 vis = v3_dot(d, d) < lim * lim;
    if (ecs_sys_get_component(EcsVisibility, i)->visible != vis)
      ecs_sys_get_component_mut(EcsVisibility, i)->visible = vis;
  }
}

// every level of an instance measures the same nearest distance, so exactly one shows
ecs_system(cod_world_lod_system, EcsIn(WorldTransform), EcsIn(CodPropLod), EcsInOut(EcsVisibility), EcsResIn(CodWorld)) {
  const CodWorld *world = ecs_sys_get_resource(CodWorld);
  v3 eye = world->lod_eye;
  f32 k = world->lod_k * COD_LOD_ERROR_SAFETY;
  for (i32 i = 0; i < it->count; i++) {
    const CodPropLod *pl = ecs_sys_get_component(CodPropLod, i);
    f32 dc = v3_length(v3_sub(ecs_sys_get_component(WorldTransform, i)->position, eye));
    f32 d = m_maxf(0.0f, dc - pl->radius - COD_LOD_EYE_MARGIN);
    b32 vis = d >= pl->err_lo * k && (pl->err_hi <= 0.0f || d < pl->err_hi * k) && (pl->max_dist <= 0.0f || dc < pl->max_dist + pl->radius);
    if (ecs_sys_get_component(EcsVisibility, i)->visible != vis)
      ecs_sys_get_component_mut(EcsVisibility, i)->visible = vis;
  }
}

// the bulbs and lamps follow the solar altitude
ecs_system(cod_world_light_system, EcsIn(CodWorldLight), EcsInOut(EcsPointLight), EcsResIn(CodLook)) {
  const CodLook *look = ecs_sys_get_resource(CodLook);
  f32 mix = 1.0f - m_clampf((look->sun_altitude + 0.05f) / 0.16f, 0.0f, 1.0f);
  f32 unit = look->scene_per_unit > 0.0f ? look->scene_per_unit : 1.0f;
  for (i32 i = 0; i < it->count; i++) {
    const CodWorldLight *wl = ecs_sys_get_component(CodWorldLight, i);
    f32 want = (wl->kind == 0 ? 5.0f + 17.0f * mix : 14.0f * mix) * unit;
    if (m_fabsf(ecs_sys_get_component(EcsPointLight, i)->intensity - want) > want * 0.002f + 1e-6f)
      ecs_sys_get_component_mut(EcsPointLight, i)->intensity = want;
  }
}

ecs_system(cod_world_load_system, EcsResIn(CodWorld), EcsResInOut(CodLoad)) {
  const CodWorld *world = ecs_sys_get_resource(CodWorld);
  ecs_sys_get_resource_mut(CodLoad)->progress[COD_LOAD_WORLD] = world->ready ? 1.0f : (world->build ? world->build->progress : 0.0f);
}

// ------------------------------------------------------------------ query ---

aabb cod_world_level_bounds(void) {
  aabb lb = aabb_make((v3){-62.0f, -2.0f, -62.0f}, (v3){62.0f, 26.0f, 62.0f});
  aabb wb = aabb_invalidate();
  for (u32 k = 0; k < 8; k++)
    wb = aabb_encapsulate_point(wb, cod_level_to_world((v3){(k & 1) ? lb.max.x : lb.min.x, (k & 2) ? lb.max.y : lb.min.y, (k & 4) ? lb.max.z : lb.min.z}));
  return wb;
}

f32 cod_world_ground_height(const CodWorld *world, f32 x, f32 z) {
  (void)world;
  f32 dx = x - COD_LEVEL_TX, dz = z - COD_LEVEL_TZ;
  f32 c = m_cosf(COD_LEVEL_YAW), s = m_sinf(COD_LEVEL_YAW);
  return cw_ground_y(c * dx - s * dz, s * dx + c * dz);
}

void cod_world_init(EcsWorld *w) {
  CodWorld world = {0};
  CodWorldBuild *b = NULL;
  ArenaAllocator *arena = arena_create(GB(3), MB(4));
  Allocator al = make_arena_allocator(arena);
  b = ALLOC(&al, CodWorldBuild);
  b->arena = arena;
  b->al = al;
  CodRng root = cod_rng_make(COD_SEED);
  cod_rng_fork(&root);
  cod_rng_fork(&root);
  b->rng = cod_rng_fork(&root);
  b->A = cwa_begin(al);
  world.camera_q = ecs_query_cached(w, EcsIn(Camera));
  b->t_total = os_time_now();
  world.build = b;
  ecs_resource_set(w, CodWorld, world);
}

void cod_world_schedule(EcsWorld *w) {
  ecs_schedule_single(w, cod_world_load_system); // writes the CodLoad resource
  CodWorld *world = ecs_resource_get_mut(w, CodWorld);
  CodWorldBuild *b = world->build;
  if (b) {
    switch (b->stage) {
    case CWB_RECORD:
      ecs_schedule_single(w, cod_world_record_system); // the RNG stream is one sequence
      break;
    case CWB_EXPAND:
      ecs_schedule_unsafe_resource_access(w, cod_world_expand_system);
      ecs_schedule_single(w, cod_world_advance_system); // writes the build stage
      break;
    case CWB_PLAN:
      ecs_schedule_single(w, cod_world_plan_system); // sizes and offsets are a prefix scan over every command
      ecs_schedule_single(w, cod_world_advance_system); // writes the build stage
      break;
    case CWB_FILL:
      ecs_schedule_unsafe_resource_access(w, cod_world_fill_system);
      ecs_schedule_single(w, cod_world_advance_system); // writes the build stage
      break;
    case CWB_BLOB:
      ecs_schedule_unsafe_resource_access(w, cod_world_blob_system);
      ecs_schedule_single(w, cod_world_advance_system); // writes the build stage
      break;
    default: break;
    }
  }
  if (world->ready) {
    ecs_schedule(w, cod_world_cull_system);
    ecs_schedule(w, cod_world_lod_system);
    ecs_schedule(w, cod_world_light_system);
  }
}

void cod_world_lanes(EcsWorld *w) { (void)w; }

// ----------------------------------------------------------------- upload ---

hz_internal HzHandleT(RendererSubMesh) cw_upload_geo(RendererState *r, const CwGeo *g) {
  HzHandleT(RendererSubMesh) h = {0};
  tctx_temp_allocator_scope(ta, NULL) {
    Allocator *al = &ta.allocator;
    v4 *colors = ALLOC_ARRAY_NO_ZERO(al, v4, g->nv);
    u16 *idx = ALLOC_ARRAY_NO_ZERO(al, u16, g->ni);
    aabb bb = aabb_invalidate();
    for (u32 i = 0; i < g->nv; i++) {
      colors[i] = g->has_col ? (v4){g->col[i].x, g->col[i].y, g->col[i].z, 1.0f} : (v4){0.0f, 0.0f, 0.0f, 1.0f};
      bb = aabb_encapsulate_point(bb, g->pos[i]);
    }
    for (u32 i = 0; i < g->ni; i++)
      idx[i] = (u16)g->idx[i];
    SubMeshDesc d = {.vertex_count = g->nv,
                     .index_count = g->ni,
                     .index_format = GPU_INDEX_FORMAT_U16,
                     .indices = idx,
                     .positions = g->pos,
                     .normals = g->nrm,
                     .uvs = g->uv,
                     .colors = colors,
                     .bounds = bb};
    h = renderer_upload_submesh(r, &d);
  }
  return h;
}

hz_internal HzHandleT(RendererMaterial) cw_key_material(CodWorldBuild *b, EcsWorld *w, RendererState *r, u32 key) {
  if (b->key_mat_ready[key])
    return b->key_mat[key];
  const CwPaletteEntry *p = cw_palette(key);
  const CwPaletteOpts *o = &cw_palette_opts_table[key];
  CodMaterialDesc d = {.id = p->material,
                       .uv_mode = COD_UV_LIBRARY,
                       .scale = p->scale,
                       .parallax = -1.0f,
                       .double_sided = (p->flags & CW_F_DOUBLE) != 0,
                       .transparent = (p->flags & CW_F_TRANSPARENT) != 0,
                       .vertex_masks = (p->flags & CW_F_NO_MASKS) == 0,
                       .emissive = o->emissive,
                       .emissive_intensity = o->emissive_intensity,
                       .opacity = o->opacity,
                       .env_intensity = o->env_intensity};
  if (o->normal_strength > 0.0f) {
    d.override_mask |= COD_OVR_NORMAL;
    d.normal_strength = o->normal_strength;
  }
  if (o->detile > 0.0f) {
    d.override_mask |= COD_OVR_DETILE;
    d.detile = o->detile;
  }
  if (o->has_weather) {
    d.override_mask |= COD_OVR_WEATHER;
    d.weather_v = o->weather;
  }
  if (o->has_wear) {
    d.override_mask |= COD_OVR_WEAR;
    d.wear = o->wear;
  }
  if (o->has_roughness) {
    // a two-element roughness keeps the default minimum
    d.override_mask |= COD_OVR_ROUGHNESS;
    d.roughness = (v4){o->roughness.x, o->roughness.y, 0.06f, 0.0f};
  }
  if (p->tint) {
    d.tint = (ColorF32){srgb_to_linear((f32)((p->tint >> 16) & 255u) / 255.0f), srgb_to_linear((f32)((p->tint >> 8) & 255u) / 255.0f),
                        srgb_to_linear((f32)(p->tint & 255u) / 255.0f), 1.0f};
  }
  HzHandleT(RendererMaterial) m = cod_materials_get(w, &d);
  if (!handle_is_valid(HANDLE_CAST(HzHandle, m)))
    m = renderer_default_material(r);
  b->key_mat[key] = m;
  b->key_mat_ready[key] = true;
  if (key == CW_LAMP_LENS)
    ecs_resource_get_mut(w, CodWorld)->lamp_lens = m;
  return m;
}

hz_internal void cw_spawn_collider(EcsWorld *w, const CwColGroup *g) {
  EcsEntity e = ecs_entity_create(w);
  u32 cat = g->surface == COD_SURFACE_GLASS ? COD_COLLIDE_GLASS : COD_COLLIDE_WORLD;
  ecs_component_set(w, e, ColliderBody,
                    {.collider = collider_trimesh(g->blob), .pos = V3_ZERO, .rot = quat_identity(), .scale = 1.0f,
                     .filter = (PhysicsFilter){.category = cat, .mask = 0xFFFFFFFFu}});
  ecs_component_set(w, e, StaticBody, {0});
  ecs_component_set(w, e, CodSurfaceTag, {.surface = g->surface});
}

hz_internal void cw_spawn_light(EcsWorld *w, v3 level_pos, f32 radius, ColorF32 color, f32 intensity, u32 kind) {
  v3 p = cod_level_to_world(level_pos);
  EcsEntity e = ecs_entity_create(w);
  ecs_component_set(w, e, WorldTransform, {.position = p, .rotation = quat_identity(), .scale = (v3){1.0f, 1.0f, 1.0f}});
  ecs_component_set(w, e, EcsPointLight, {.color = color, .radius = radius, .intensity = intensity});
  ecs_component_set(w, e, CodWorldLight, {.kind = kind});
}

hz_internal f32 cw_proto_radius(const CwGeo *g) {
  f32 r2 = 0.0f;
  for (u32 i = 0; i < g->nv; i++)
    r2 = m_maxf(r2, v3_dot(g->pos[i], g->pos[i]));
  return m_sqrtf(r2);
}

hz_internal void cw_finish(CodWorldBuild *b, EcsWorld *w, CodWorld *world) {
  CwA *A = b->A;
  // the practicals: the first 20 bare bulbs and every lamp, drawn after the geometry; light colours are sRGB (the renderer linearizes)
  ColorF32 bulb = {1.0f, 0xc0 / 255.0f, 0x7a / 255.0f, 1.0f};
  ColorF32 lamp = {1.0f, 0xb7 / 255.0f, 0x65 / 255.0f, 1.0f};
  u32 nb = A->interior_lights.len < 20 ? A->interior_lights.len : 20;
  for (u32 i = 0; i < nb; i++)
    cw_spawn_light(w, A->interior_lights.items[i], 13.0f, bulb, 0.0f, 0);
  for (u32 i = 0; i < A->lamp_anchors.len; i++) {
    v3 p = A->lamp_anchors.items[i];
    cw_spawn_light(w, (v3){p.x, p.y - 0.12f, p.z}, 22.0f, lamp, 0.0f, 1);
  }

  world->spawn_count = 0;
  for (u32 i = 0; i < CW_SPAWN_COUNT && i < COD_WORLD_MAX_SPAWNS; i++)
    world->spawns[world->spawn_count++] = (CodSpawnPoint){cod_level_to_world((v3){CW_SPAWNS[i].x, 0.0f, CW_SPAWNS[i].z}), CW_SPAWNS[i].yaw + COD_LEVEL_YAW};

  world->bounds = cod_world_level_bounds();

  world->building_count = 0;
  for (u32 i = 0; i < b->info_count && i < COD_WORLD_MAX_BUILDINGS; i++) {
    const CwBuildInfo *info = b->infos[i];
    const CwBuildingSpec *s = info->spec;
    CodBuildingInfo *o = &world->buildings[world->building_count++];
    o->center = cod_level_to_world((v3){s->x, info->roof_y * 0.5f, s->z});
    o->half_size = (v2){s->w * 0.5f, s->d * 0.5f};
    o->yaw = COD_LEVEL_YAW;
    o->floors = s->floors;
    o->floor_height = info->roof_y / (f32)s->floors;
    o->enterable = s->enterable;
    o->level_rect = (v4){s->x, s->z, s->w, s->d};
    o->collapsed = s->collapse || s->ruin;
    o->room_top = (s->has_setback ? info->floor_y[s->setback_from] : info->roof_y) - 0.06f;
  }
  world->street = (v4){CW_STREET_KERB, CW_STREET_ZMIN, CW_STREET_ZMAX, 0.0f};
  // an alley authored with z0 > z1 is never open
  world->alley_count = 0;
  for (u32 i = 0; i < CW_ALLEY_COUNT && world->alley_count < COD_WORLD_MAX_ALLEYS; i++)
    if (CW_ALLEYS[i].z0 < CW_ALLEYS[i].z1 && CW_ALLEYS[i].x0 < CW_ALLEYS[i].x1)
      world->alleys[world->alley_count++] = (v4){CW_ALLEYS[i].x0, CW_ALLEYS[i].z0, CW_ALLEYS[i].x1, CW_ALLEYS[i].z1};
}

void cod_world_frame(EcsWorld *w) {
  CodWorld *world = ecs_resource_get_mut(w, CodWorld);
  // the lens follows the lamps' mix
  f32 mix = 1.0f - m_clampf((ecs_resource_get(w, CodLook)->sun_altitude + 0.05f) / 0.16f, 0.0f, 1.0f);
  if (handle_is_valid(HANDLE_CAST(HzHandle, world->lamp_lens)) && m_fabsf(mix - world->lamp_mix) > 0.01f) {
    world->lamp_mix = mix;
    cod_materials_set_emissive(w, world->lamp_lens, 0xffc47a, 9.0f * mix);
  }
  Camera *cam = ecs_query_get_singleton(Camera, world->camera_q);
  if (cam) {
    world->cull_eye = cam->pos;
    if (!world->lod_eye_set || v3_length(v3_sub(cam->pos, world->lod_eye)) > COD_LOD_EYE_STEP) {
      world->lod_eye = cam->pos;
      world->lod_eye_set = true;
    }
    // an error e spans COD_LOD_PIXELS of the display at distance e * proj[1][1] * display_h / (2 * COD_LOD_PIXELS)
    RendererState *rs = ecs_resource_get_mut(w, RendererState);
    u32 rw = 0, rh = 0;
    renderer_render_size(rs, &rw, &rh);
    f32 scale = renderer_render_scale(rs);
    f32 display_h = scale > 0.0f ? (f32)rh / scale : (f32)rh;
    world->lod_k = cam->proj.m[1][1] * display_h / (2.0f * COD_LOD_PIXELS);
  }
  CodWorldBuild *b = world->build;
  if (!b)
    return;
  world->progress = b->progress;
  if (b->stage != CWB_UPLOAD)
    return;
  const CodMaterials *mats = ecs_resource_get(w, CodMaterials);
  if (!mats->ready)
    return;
  RendererState *r = ecs_resource_get_mut(w, RendererState);
  CwA *A = b->A;
  u64 t0 = os_time_now();
  // a load-screen slice: load frames already run ~85 ms (audio and nav share them), and the world is the critical path
  u64 budget_ticks = (u64)(os_ticks_to_ms(1) > 0.0 ? 24.0 / os_ticks_to_ms(1) : 1);
  if (b->upload_frames++ == 0)
    b->t_upload_first = t0;

  while (b->up_chunk < b->chunks.len && os_time_diff(os_time_now(), t0) < budget_ticks) {
    CwChunk *c = &b->chunks.items[b->up_chunk++];
    c->sub = cw_upload_geo(r, &c->g);
    EcsEntity e = ecs_renderer_spawn(w, r, &(EcsRendererSpawnDesc){.submesh = c->sub, .material = cw_key_material(b, w, r, c->key),
                                                                   .transform = {.position = V3_ZERO, .rotation = quat_identity(), .scale = (v3){1.0f, 1.0f, 1.0f}},
                                                                   .casts_shadow = !c->no_shadow, .static_shadow = true});
    (void)e;
    world->static_tris += c->g.ni / 3;
  }
  if (b->up_chunk < b->chunks.len) {
    b->progress = 0.85f + 0.07f * (f32)b->up_chunk / (f32)b->chunks.len;
    b->upload_work_ticks += os_time_diff(os_time_now(), t0);
    return;
  }
  while (b->up_proto < CWP_COUNT) {
    CwProto *p = &A->protos[b->up_proto];
    if (p->defined && p->count > 0 && p->geo.nv > 0) {
      b->proto_sub[b->up_proto] = cw_upload_geo(r, &p->geo);
      for (u32 l = 1; l < p->lod_count; l++)
        p->lod_sub[l - 1] = cw_upload_geo(r, &p->lods[l - 1]);
    }
    b->up_proto++;
  }
  while (b->up_inst < A->insts.len && os_time_diff(os_time_now(), t0) < budget_ticks) {
    const CwInst *in = &A->insts.items[b->up_inst++];
    const CwProto *p = &A->protos[in->proto];
    if (!p->defined || p->geo.nv == 0)
      continue;
    m4 m = m4_mul(A->xform, cw_trs(in->pos.x, in->pos.y, in->pos.z, in->ry, 1.0f, 1.0f, 1.0f, in->rx, in->rz));
    Transform tr = {.position = (v3){m.m[3][0], m.m[3][1], m.m[3][2]}, .rotation = quat_from_m4(m), .scale = in->scale};
    f32 radius = cw_proto_radius(&p->geo) * m_maxf(in->scale.x, m_maxf(in->scale.y, in->scale.z));
    HzHandleT(RendererMaterial) mat = cw_key_material(b, w, r, p->key);
    for (u32 l = 0; l < p->lod_count; l++) {
      EcsEntity e = ecs_renderer_spawn(w, r, &(EcsRendererSpawnDesc){.submesh = l == 0 ? b->proto_sub[in->proto] : p->lod_sub[l - 1], .material = mat,
                                                                     .transform = tr, .casts_shadow = p->cast_shadow, .static_shadow = true, .hidden = l > 0});
      if (p->lod_count > 1)
        ecs_component_set(w, e, CodPropLod,
                          {.err_lo = l == 0 ? 0.0f : p->lod_err[l], .err_hi = l + 1 < p->lod_count ? p->lod_err[l + 1] : 0.0f, .max_dist = p->max_dist, .radius = radius});
      else if (p->max_dist > 0.0f)
        ecs_component_set(w, e, CodPropCull, {.max_dist = p->max_dist, .radius = radius});
    }
    world->instances++;
    world->instance_tris += p->geo.ni / 3;
  }
  if (b->up_inst < A->insts.len) {
    b->progress = 0.92f + 0.07f * (f32)b->up_inst / (f32)A->insts.len;
    b->upload_work_ticks += os_time_diff(os_time_now(), t0);
    return;
  }
  for (u32 i = 0; i < b->groups.len; i++)
    if (b->groups.items[i].nv)
      cw_spawn_collider(w, &b->groups.items[i]);
  cw_finish(b, w, world);
  b->upload_work_ticks += os_time_diff(os_time_now(), t0);

  b->ms_upload = os_ticks_to_ms(os_time_diff(os_time_now(), b->t_phase));
  log_info("world: upload waited % ms for the materials, then % frames over % ms, % ms of upload work",
           fmt_f64_p(os_ticks_to_ms(os_time_diff(b->t_upload_first, b->t_phase)), 0), fmt_u32(b->upload_frames),
           fmt_f64_p(os_ticks_to_ms(os_time_diff(os_time_now(), b->t_upload_first)), 0), fmt_f64_p(os_ticks_to_ms(b->upload_work_ticks), 0));
  world->build_ms = os_ticks_to_ms(os_time_diff(os_time_now(), b->t_total));
  log_info("world: record props % ground % buildings % gate % perimeter % street % dress % scatter % ms", fmt_f64_p(b->ms_props, 0),
           fmt_f64_p(b->ms_ground, 0), fmt_f64_p(b->ms_buildings, 0), fmt_f64_p(b->ms_gate, 0), fmt_f64_p(b->ms_perimeter, 0),
           fmt_f64_p(b->ms_street, 0), fmt_f64_p(b->ms_dress, 0), fmt_f64_p(b->ms_scatter, 0));
  log_info("world: expand % plan % fill % blobs % upload % ms; total % ms", fmt_f64_p(b->ms_expand, 0), fmt_f64_p(b->ms_plan, 0),
           fmt_f64_p(b->ms_fill, 0), fmt_f64_p(b->ms_blob, 0), fmt_f64_p(b->ms_upload, 0), fmt_f64_p(world->build_ms, 0));
  log_info("world: % static tris in % batches, % instanced tris in % instances, % collision tris, % buildings, % geometries, % inst records",
           fmt_u32(world->static_tris), fmt_u32(b->chunks.len), fmt_u32(world->instance_tris), fmt_u32(world->instances),
           fmt_u32(b->collide_tris), fmt_u32(world->building_count), fmt_u32(A->geos.len), fmt_u32(A->insts.len));

  world->progress = 1.0f;
  world->ready = true;
  b->stage = CWB_DONE;
  world->build = NULL;
  arena_destroy(b->arena);
}
