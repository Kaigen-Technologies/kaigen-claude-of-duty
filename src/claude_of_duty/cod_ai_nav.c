// Claude of Duty AI navigation: the walkability grid sampled from the static physics layer, A* with string pulling, cover.

hz_internal const i32 CAI_DX[8] = {1, -1, 0, 0, 1, 1, -1, -1};
hz_internal const i32 CAI_DZ[8] = {0, 0, 1, -1, 1, -1, 1, -1};
#define CAI_SQRT2 1.41421356237f

// ---- world queries ----

CaiQ cai_q(EcsWorld *w) {
  CaiQ q = {.layer = ecs_physics_get_static_layer(w)};
  q.filter = (PhysicsQueryFilter){.category = 0xFFFFFFFFu, .mask = COD_COLLIDE_WORLD | COD_COLLIDE_PROP};
  return q;
}

// the closest hit along origin + dir * dist (dir is a unit vector)
b32 cai_ray(const CaiQ *q, v3 origin, v3 dir, f32 dist, CaiRay *out) {
  if (!q->layer)
    return false;
  PhysicsRaycastResult r = {0};
  i32 idx = 0;
  if (!physics_raycast_layer(origin, v3_add(origin, v3_scale(dir, dist)), (CollisionLayer *)&q->layer->layer, q->filter, &r, &idx))
    return false;
  out->point = r.position;
  out->normal = r.normal;
  out->distance = r.fraction * dist;
  return true;
}

b32 cai_ray_any(const CaiQ *q, v3 origin, v3 dir, f32 dist) {
  if (!q->layer)
    return false;
  PhysicsRaycastResult r = {0};
  i32 idx = 0;
  return physics_raycast_layer_any(origin, v3_add(origin, v3_scale(dir, dist)), (CollisionLayer *)&q->layer->layer, q->filter, &r, &idx);
}

// nothing blocks the straight line between two points
b32 cai_los(const CaiQ *q, v3 from, v3 to) {
  v3 d = v3_sub(to, from);
  f32 len = v3_length(d);
  if (len < 1e-6f)
    return true;
  return !cai_ray_any(q, from, v3_scale(d, 1.0f / len), len - 1e-3f);
}

// the floor under (x, z) found from from_y, else the analytic ground
f32 cai_ground_at(const CaiQ *q, const CodWorld *world, f32 x, f32 z, f32 from_y) {
  CaiRay h;
  if (cai_ray(q, (v3){x, from_y, z}, (v3){0.0f, -1.0f, 0.0f}, 80.0f, &h))
    return h.point.y;
  return cod_world_ground_height(world, x, z);
}

// ---- grid ----

hz_internal i32 cai_cell_x(const CaiNav *n, f32 x) { return (i32)m_floorf((x - n->min_x) / CAI_CELL + 0.5f); }
hz_internal i32 cai_cell_z(const CaiNav *n, f32 z) { return (i32)m_floorf((z - n->min_z) / CAI_CELL + 0.5f); }
hz_internal f32 cai_world_x(const CaiNav *n, i32 ix) { return n->min_x + (f32)ix * CAI_CELL; }
hz_internal f32 cai_world_z(const CaiNav *n, i32 iz) { return n->min_z + (f32)iz * CAI_CELL; }
hz_internal b32 cai_inside(const CaiNav *n, i32 ix, i32 iz) { return ix >= 0 && iz >= 0 && ix < (i32)n->nx && iz < (i32)n->nz; }

hz_internal b32 cai_walkable(const CaiNav *n, i32 ix, i32 iz) {
  return cai_inside(n, ix, iz) && n->flags[(u32)iz * n->nx + (u32)ix] != 0;
}

// the nearest walkable cell, searched in rings; use_y rejects other storeys beyond y_tol
i32 cai_nearest(const CaiNav *n, f32 x, f32 z, f32 y, b32 use_y, i32 rings, f32 y_tol) {
  i32 cx = cai_cell_x(n, x), cz = cai_cell_z(n, z);
  if (cai_walkable(n, cx, cz)) {
    i32 i = cz * (i32)n->nx + cx;
    if (!use_y || m_absf(n->floor[i] - y) <= y_tol)
      return i;
  }
  for (i32 ring = 1; ring <= rings; ring++) {
    i32 best = -1;
    f32 best_d = HZ_INF;
    for (i32 dz = -ring; dz <= ring; dz++) {
      for (i32 dx = -ring; dx <= ring; dx++) {
        i32 ax = dx < 0 ? -dx : dx, az = dz < 0 ? -dz : dz;
        if ((ax > az ? ax : az) != ring)
          continue;
        i32 ix = cx + dx, iz = cz + dz;
        if (!cai_walkable(n, ix, iz))
          continue;
        i32 i = iz * (i32)n->nx + ix;
        if (use_y && !(m_absf(n->floor[i] - y) <= y_tol))
          continue;
        f32 d = (f32)(dx * dx + dz * dz);
        if (use_y && m_isfinitef(n->floor[i])) {
          f32 dy = n->floor[i] - y;
          d += dy * dy * 4.0f;
        }
        if (d < best_d) {
          best_d = d;
          best = i;
        }
      }
    }
    if (best >= 0)
      return best;
  }
  return -1;
}

// ---- cover ----

// the best cover for an agent at pos against a threat: the blocker between them, a sensible range, free, close, not bunched with the squad
i32 cai_cover_pick(const CaiNav *n, const CodAiShared *sh, v3 pos, v3 threat, u32 id, u32 squad, f32 min_range, f32 max_range,
                   f32 max_travel, const i32 *skip, u32 skip_count) {
  i32 best = -1;
  f32 best_score = -HZ_INF;
  for (u32 i = 0; i < n->cover_count; i++) {
    const CaiCover *p = &n->cover[i];
    if (n->claim[i] != 0 && n->claim[i] != id)
      continue;
    b32 skipped = false;
    for (u32 k = 0; k < skip_count; k++)
      skipped |= skip[k] == (i32)i;
    if (skipped)
      continue;
    f32 tx = threat.x - p->pos.x, tz = threat.z - p->pos.z;
    f32 dt = cai_hypotf(tx, tz);
    if (dt < 2.5f || dt > 40.0f)
      continue;
    f32 travel = cai_hypotf(p->pos.x - pos.x, p->pos.z - pos.z);
    // never a run longer than the distance to the threat: that runs past it
    if (travel > max_travel || travel > cai_hypotf(threat.x - pos.x, threat.z - pos.z))
      continue;
    f32 prot = (tx / dt) * p->dx + (tz / dt) * p->dz;
    if (prot < 0.25f)
      continue;
    f32 score = prot * 5.0f + (p->high ? 2.2f : 1.0f);
    if (dt < min_range)
      score -= (min_range - dt) * 0.55f;
    else if (dt > max_range)
      score -= (dt - max_range) * 0.28f;
    score -= travel * 0.16f;
    if (squad) {
      for (u32 k = 0; k < sh->roster_count; k++) {
        const CaiRoster *o = &sh->roster[k];
        if (!o->used || o->squad != squad || o->id == id || !o->alive)
          continue;
        f32 d = cai_hypotf(o->position.x - p->pos.x, o->position.z - p->pos.z);
        if (d < 3.2f)
          score -= (3.2f - d) * 1.4f;
      }
    }
    if (score > best_score) {
      best_score = score;
      best = (i32)i;
    }
  }
  return best;
}

// where to lean out of a cover point: the side with a line of sight from the eye to the threat; returns the side (1, -1, 0)
f32 cai_cover_peek(const CaiNav *n, const CaiQ *q, const CaiCover *c, v3 threat, f32 eye_h, v3 *out) {
  (void)n;
  f32 lx = -c->dz, lz = c->dx;
  for (u32 k = 0; k < 3; k++) {
    f32 s = k == 0 ? 1.0f : k == 1 ? -1.0f : 0.0f;
    f32 px = c->pos.x + lx * 0.62f * s, pz = c->pos.z + lz * 0.62f * s;
    if (cai_los(q, (v3){px, c->pos.y + eye_h, pz}, threat)) {
      *out = (v3){px, c->pos.y, pz};
      return s;
    }
  }
  *out = c->pos;
  return 0.0f;
}

// ---- the build: every lane takes rows of the grid, then rows of the cover scan ----

// the scope's gate: asks for the build once the world's colliders have settled into the static layer
void cai_nav_gate(EcsWorld *w) {
  CodAiNavGate *g = ecs_resource_get_mut(w, CodAiNavGate);
  if (g->stage != CAI_NAV_WAIT)
    return;
  const CodWorld *world = ecs_resource_get(w, CodWorld);
  b32 layer = ecs_physics_get_static_layer(w) != NULL;
  g->settled = world->ready && layer ? g->settled + 1 : 0;
  if (g->settled >= 3) {
    g->stage = CAI_NAV_BUILD;
    g->t0 = os_time_now();
  }
}

// every lane, before the scope; the stage is the same on every lane because only the gate (inside a scope) writes it
void cai_nav_build_lanes(EcsWorld *w) {
  CodAiNavGate *g = ecs_resource_get_mut(w, CodAiNavGate);
  if (g->stage != CAI_NAV_BUILD)
    return;
  const CodWorld *world = ecs_resource_get(w, CodWorld);
  CaiQ q = cai_q(w);
  ThreadContext *tc = tctx_current();
  u32 lane = tc->thread_idx, lanes = tc->thread_count;
  assert_msg(lanes <= CAI_MAX_LANES, "cai: more lanes than the nav tables hold");

  if (is_main_thread()) {
    Allocator al = make_arena_allocator(g->arena);
    CaiNav *n = ALLOC(&al, CaiNav);
    v3 lo = v3_sub(world->bounds.min, (v3){2.0f, 2.0f, 2.0f}), hi = v3_add(world->bounds.max, (v3){2.0f, 2.0f, 2.0f});
    n->min_x = m_floorf(lo.x / CAI_CELL) * CAI_CELL;
    n->min_z = m_floorf(lo.z / CAI_CELL) * CAI_CELL;
    n->nx = (u32)m_maxf(1.0f, m_ceilf((hi.x - n->min_x) / CAI_CELL));
    n->nz = (u32)m_maxf(1.0f, m_ceilf((hi.z - n->min_z) / CAI_CELL));
    n->top_y = hi.y + 4.0f;
    u32 cells = n->nx * n->nz;
    n->flags = ALLOC_ARRAY(&al, u8, cells);
    n->enclosure = ALLOC_ARRAY(&al, u8, cells);
    n->floor = ALLOC_ARRAY_NO_ZERO(&al, f32, cells);
    for (u32 i = 0; i < cells; i++)
      n->floor[i] = -HZ_INF;
    n->cover_cell = ALLOC_ARRAY(&al, CaiCover, cells);
    n->cover_has = ALLOC_ARRAY(&al, u8, cells);
    n->lanes = lanes;
    g->nav = n;
  }
  lane_sync();
  CaiNav *n = g->nav;
  i32 nx = (i32)n->nx;
  u64 t_grid = os_time_now();

  // grid rows: one down ray for the floor, one up ray for the clearance, four chest-high probes for the shoulders
  Range(u32) rows = lane_range(n->nz);
  u32 walk = 0;
  for (u32 iz = rows.min; iz < rows.max; iz++) {
    for (u32 ix = 0; ix < n->nx; ix++) {
      u32 i = iz * n->nx + ix;
      f32 x = cai_world_x(n, (i32)ix), z = cai_world_z(n, (i32)iz);
      CaiRay down;
      if (!cai_ray(&q, (v3){x, n->top_y, z}, (v3){0.0f, -1.0f, 0.0f}, n->top_y + 30.0f, &down))
        continue;
      n->floor[i] = down.point.y;
      if (down.normal.y < CAI_NAV_SLOPE_COS)
        continue;
      f32 fy = down.point.y;
      CaiRay up;
      u8 flag;
      if (!cai_ray(&q, (v3){x, fy + 0.25f, z}, (v3){0.0f, 1.0f, 0.0f}, CAI_NAV_HEIGHT - 0.2f, &up))
        flag = 1;
      else if (up.distance > CAI_NAV_CROUCH_HEIGHT - 0.25f)
        flag = 2;
      else
        continue;
      u32 blocked = 0;
      for (u32 d = 0; d < 4; d++) {
        v3 dir = {d == 0 ? 1.0f : d == 1 ? -1.0f : 0.0f, 0.0f, d == 2 ? 1.0f : d == 3 ? -1.0f : 0.0f};
        if (cai_ray_any(&q, (v3){x, fy + 0.95f, z}, dir, CAI_NAV_RADIUS + 0.06f))
          blocked++;
      }
      if (blocked >= 3)
        continue;
      n->flags[i] = flag;
      n->enclosure[i] = (u8)blocked;
      walk++;
    }
  }
  n->lane_walk[lane] = walk;
  lane_sync();
  if (is_main_thread()) {
    for (u32 l = 0; l < lanes; l++)
      n->walkable += n->lane_walk[l];
    n->grid_ms = os_ticks_to_ms(os_time_diff(os_time_now(), t_grid));
  }
  u64 t_cover = os_time_now();

  // cover: a walkable cell beside a blocker whose low ray hits within reach; one point per cell, kept in row order
  Range(u32) crow = lane_range(n->nz > 2 ? n->nz - 2 : 0);
  u32 found = 0;
  for (u32 r = crow.min; r < crow.max; r++) {
    i32 iz = (i32)r + 1;
    for (i32 ix = 1; ix < nx - 1; ix++) {
      if (!cai_walkable(n, ix, iz))
        continue;
      u32 i = (u32)iz * n->nx + (u32)ix;
      if (n->enclosure[i] == 0) {
        b32 adj = false;
        for (u32 d = 0; d < 4 && !adj; d++)
          if (!cai_walkable(n, ix + CAI_DX[d], iz + CAI_DZ[d]))
            adj = true;
        if (!adj)
          continue;
      }
      f32 x = cai_world_x(n, ix), z = cai_world_z(n, iz), y = n->floor[i];
      for (u32 d = 0; d < 8; d++) {
        f32 inv = d < 4 ? 1.0f : 1.0f / CAI_SQRT2;
        f32 dx = (f32)CAI_DX[d] * inv, dz = (f32)CAI_DZ[d] * inv;
        CaiRay low;
        if (!cai_ray(&q, (v3){x, y + 0.55f, z}, (v3){dx, 0.0f, dz}, CAI_COVER_REACH, &low))
          continue;
        b32 high = cai_ray_any(&q, (v3){x, y + 1.32f, z}, (v3){dx, 0.0f, dz}, CAI_COVER_REACH);
        n->cover_cell[i] = (CaiCover){.pos = {x, y, z}, .dx = dx, .dz = dz, .high = high, .dist = low.distance};
        n->cover_has[i] = 1;
        found++;
        break;
      }
    }
  }
  n->lane_cover[lane] = found;
  lane_sync();
  if (is_main_thread()) {
    u32 total = 0;
    for (u32 l = 0; l < lanes; l++)
      total += n->lane_cover[l];
    Allocator al = make_arena_allocator(g->arena);
    n->cover_count = total;
    n->cover = ALLOC_ARRAY(&al, CaiCover, total > 0 ? total : 1);
    n->claim = ALLOC_ARRAY(&al, u32, total > 0 ? total : 1);
  }
  lane_sync();
  u32 offset = 0;
  for (u32 l = 0; l < lane; l++)
    offset += n->lane_cover[l];
  for (u32 r = crow.min; r < crow.max; r++) {
    u32 iz = r + 1;
    for (u32 ix = 1; ix + 1 < n->nx; ix++) {
      u32 i = iz * n->nx + ix;
      if (n->cover_has[i])
        n->cover[offset++] = n->cover_cell[i];
    }
  }
  lane_sync();
  if (is_main_thread()) {
    n->cover_ms = os_ticks_to_ms(os_time_diff(os_time_now(), t_cover));
    n->ready = true;
    g->stage = CAI_NAV_READY;
    f64 total_ms = os_ticks_to_ms(os_time_diff(os_time_now(), t_grid));
    log_info("ai: nav % x % cells, % walkable, % cover points; grid % ms, cover % ms, total % ms on % lanes", fmt_u32(n->nx), fmt_u32(n->nz),
             fmt_u32(n->walkable), fmt_u32(n->cover_count), fmt_f64_p(n->grid_ms, 1), fmt_f64_p(n->cover_ms, 1),
             fmt_f64_p(total_ms, 1), fmt_u32(lanes));
  }
}
