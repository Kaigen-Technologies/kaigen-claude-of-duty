// Claude of Duty player: the physics queries the movement asks, the ledge probe and the rooted mantle and vault.

hz_internal CodPhys cod_phys_make(EcsWorld *w) {
  return (CodPhys){.w = w, .layers = {ecs_physics_get_static_layer(w), ecs_physics_get_kinematic_layer(w)}};
}

hz_internal u32 cod_entity_surface(EcsWorld *w, EcsEntity e) {
  const CodSurfaceTag *tag = e ? ecs_component_get(w, e, CodSurfaceTag) : NULL;
  return tag ? tag->surface : COD_SURFACE_CONCRETE;
}

// the first segment of the ray into any layer
hz_internal b32 cod_phys_ray(const CodPhys *p, v3 from, v3 to, CodHit *out) {
  *out = (CodHit){0};
  f32 len = v3_distance(from, to);
  f32 best = 2.0f;
  for (u32 l = 0; l < 2; l++) {
    const EcsCollisionLayer *layer = p->layers[l];
    PhysicsRaycastResult r = {0};
    i32 idx = 0;
    if (!layer || !physics_raycast_layer(from, to, (CollisionLayer *)&layer->layer, PHYSICS_QUERY_FILTER_ALL, &r, &idx) ||
        r.fraction >= best)
      continue;
    best = r.fraction;
    *out = (CodHit){true, r.position, r.normal, r.fraction * len, cod_entity_surface(p->w, layer->entities[idx])};
  }
  return out->hit;
}

// the capsule (segment a..b, radius) swept along dir for dist
hz_internal b32 cod_phys_cast(const CodPhys *p, v3 a, v3 b, f32 radius, v3 dir, f32 dist, CodHit *out) {
  *out = (CodHit){0};
  Collider caster = collider_capsule(V3_ZERO, v3_sub(b, a), radius);
  v3 end = v3_add(a, v3_scale(dir, dist));
  f32 best = dist + 1.0f;
  for (u32 l = 0; l < 2; l++) {
    const EcsCollisionLayer *layer = p->layers[l];
    ColliderCastResult r = {0};
    i32 idx = 0;
    if (!layer || !physics_collider_cast_layer(caster, a, quat_identity(), 1.0f, end, &layer->layer, PHYSICS_QUERY_FILTER_ALL, &r, &idx) ||
        r.distance >= best)
      continue;
    best = r.distance;
    *out = (CodHit){true, r.position, r.normal_on_target, r.distance, cod_entity_surface(p->w, layer->entities[idx])};
  }
  return out->hit;
}

// true when the capsule (segment a..b, radius) touches or penetrates anything
hz_internal b32 cod_phys_overlap(const CodPhys *p, v3 a, v3 b, f32 radius) {
  Collider col = collider_capsule(V3_ZERO, v3_sub(b, a), radius);
  for (u32 l = 0; l < 2; l++) {
    const EcsCollisionLayer *layer = p->layers[l];
    ColliderDistanceResult r = {0};
    i32 idx = 0;
    if (layer && physics_distance_between_layer(col, a, quat_identity(), 1.0f, (CollisionLayer *)&layer->layer, 0.0f,
                                                PHYSICS_QUERY_FILTER_ALL, &r, &idx))
      return true;
  }
  return false;
}

// can a capsule of `height` stand with its bottom at (x, y, z)
hz_internal b32 cod_phys_fits(const CodPhys *p, f32 x, f32 y, f32 z, f32 height) {
  f32 r = COD_PLAYER_RADIUS;
  return !cod_phys_overlap(p, (v3){x, y + r, z}, (v3){x, y + m_maxf(height - r, r), z}, r - 0.005f);
}

// 1. a face ahead, 2. its walkable lip, 3. standable ground the capsule can occupy; `feet` is the floor-level capsule bottom
hz_internal u32 cod_ledge_probe(const CodPhys *p, v3 feet, f32 fx, f32 fz, f32 stand, CodLedge *r) {
  r->kind = COD_LEDGE_NONE;
  f32 radius = COD_PLAYER_RADIUS;
  f32 lo = feet.y + m_maxf(0.14f, COD_MANTLE_MIN_HEIGHT * 0.5f) + radius * 0.6f;
  f32 hi = feet.y + m_minf(COD_MANTLE_MAX_HEIGHT + 0.1f, stand - 0.12f);
  if (hi <= lo)
    return COD_LEDGE_NONE;

  CodHit wall;
  if (!cod_phys_cast(p, (v3){feet.x, lo, feet.z}, (v3){feet.x, m_maxf(lo, hi), feet.z}, radius * 0.86f, (v3){fx, 0.0f, fz},
                     COD_MANTLE_REACH, &wall))
    return COD_LEDGE_NONE;
  if (m_fabsf(wall.normal.y) > 0.55f || wall.normal.x * fx + wall.normal.z * fz > -0.3f)
    return COD_LEDGE_NONE;
  f32 wall_dist = wall.distance;
  r->surface = wall.surface;

  // 6 cm of bite lands on the top slab without reaching over a thin lip and finding the floor behind it
  f32 lip_x = feet.x + fx * (wall_dist + radius * 0.86f + 0.06f);
  f32 lip_z = feet.z + fz * (wall_dist + radius * 0.86f + 0.06f);
  f32 above = feet.y + COD_MANTLE_MAX_HEIGHT + 0.35f;
  CodHit top;
  if (!cod_phys_ray(p, (v3){lip_x, above, lip_z}, (v3){lip_x, above - (COD_MANTLE_MAX_HEIGHT + 1.4f), lip_z}, &top) ||
      top.normal.y < 0.62f)
    return COD_LEDGE_NONE;
  f32 top_y = top.point.y;
  f32 obstacle = top_y - feet.y;
  if (obstacle < COD_MANTLE_MIN_HEIGHT || obstacle > COD_MANTLE_MAX_HEIGHT)
    return COD_LEDGE_NONE;
  r->surface = top.surface;

  // a deep top face is mantled onto, a thin obstacle is vaulted clean over
  f32 deep_x = lip_x + fx * COD_MANTLE_LAND_DEPTH;
  f32 deep_z = lip_z + fz * COD_MANTLE_LAND_DEPTH;
  CodHit deep;
  b32 deep_ok = cod_phys_ray(p, (v3){deep_x, top_y + 0.35f, deep_z}, (v3){deep_x, top_y + 0.35f - 2.6f, deep_z}, &deep) &&
                deep.point.y > top_y - 0.14f && deep.normal.y > 0.6f;

  u32 kind = COD_LEDGE_NONE;
  if (deep_ok) {
    if (cod_phys_fits(p, deep_x, top_y + 0.02f, deep_z, stand)) {
      kind = COD_LEDGE_MANTLE;
      r->land = (v3){deep_x, top_y, deep_z};
    } else if (cod_phys_fits(p, lip_x + fx * 0.3f, top_y + 0.02f, lip_z + fz * 0.3f, m_minf(stand, 1.2f))) {
      kind = COD_LEDGE_MANTLE;
      r->land = (v3){lip_x + fx * 0.3f, top_y, lip_z + fz * 0.3f};
    }
  } else {
    f32 over_x = lip_x + fx * (COD_MANTLE_LAND_DEPTH + 0.28f);
    f32 over_z = lip_z + fz * (COD_MANTLE_LAND_DEPTH + 0.28f);
    CodHit floor;
    if (cod_phys_ray(p, (v3){over_x, top_y + 0.3f, over_z}, (v3){over_x, top_y + 0.3f - 3.2f, over_z}, &floor) &&
        floor.normal.y > 0.55f && top_y - floor.point.y < 2.4f) {
      f32 ly = floor.point.y;
      if (cod_phys_fits(p, over_x, ly + 0.02f, over_z, stand)) {
        kind = COD_LEDGE_VAULT;
        r->land = (v3){over_x, ly, over_z};
      }
    }
    if (kind == COD_LEDGE_NONE && cod_phys_fits(p, lip_x + fx * 0.24f, top_y + 0.02f, lip_z + fz * 0.24f, stand)) {
      kind = COD_LEDGE_MANTLE;
      r->land = (v3){lip_x + fx * 0.24f, top_y, lip_z + fz * 0.24f};
    }
  }
  if (kind == COD_LEDGE_NONE)
    return COD_LEDGE_NONE;

  // head clearance directly over the lip: do not climb into a soffit
  if (!cod_phys_fits(p, lip_x, top_y + 0.02f, lip_z, m_minf(stand, 1.15f)))
    return COD_LEDGE_NONE;

  r->kind = kind;
  r->fast = obstacle <= COD_MANTLE_AUTO_VAULT_MAX;
  r->obstacle_height = obstacle;
  r->top_y = top_y;
  r->lip_x = lip_x;
  r->lip_z = lip_z;
  r->distance = wall_dist;
  return kind;
}

// up first, then in: the shape of pulling yourself onto a roof
hz_internal void cod_mantle_eval_mantle(CodMantle *m, f32 u) {
  f32 rise = cod_smootherstep(u / 0.62f);
  f32 settle = cod_smootherstep((u - 0.58f) / 0.42f);
  f32 peak = m->top_y + 0.06f;
  f32 y = m->start.y + (peak - m->start.y) * rise;
  y += (m->land.y - peak) * settle;

  // 18 % of the horizontal travel happens during the rise (hands reaching in), the rest once the hips clear the lip
  f32 lead = cod_smoothstep(u / 0.62f) * 0.18f;
  f32 pull = cod_smootherstep((u - 0.42f) / 0.58f) * 0.82f;
  f32 h = m_saturatef(lead + pull);
  m->pos = (v3){m->start.x + (m->land.x - m->start.x) * h, y, m->start.z + (m->land.z - m->start.z) * h};

  f32 load = m_sinf(m_saturatef(u / 0.34f) * COD_PI) * (1.0f - cod_smoothstep((u - 0.5f) / 0.5f));
  m->cam_y = -0.075f * load + 0.03f * m_sinf(m_saturatef((u - 0.55f) / 0.45f) * COD_PI);
  m->cam_forward = 0.05f * m_sinf(m_saturatef(u) * COD_PI);
  m->cam_pitch = -5.2f * COD_DEG * load + 2.1f * COD_DEG * cod_smoothstep((u - 0.6f) / 0.4f);
  m->cam_roll = m->side * 3.2f * COD_DEG * m_sinf(m_saturatef(u) * COD_PI);
}

// a single low arc that carries momentum through: a hurdle, not a climb
hz_internal void cod_mantle_eval_vault(CodMantle *m, f32 u) {
  f32 h = cod_smoothstep(u);
  f32 clearance = m->top_y + 0.14f;
  f32 up_t = m_saturatef(u / 0.42f);
  f32 down_t = m_saturatef((u - 0.42f) / 0.58f);
  f32 y = m->start.y + (clearance - m->start.y) * cod_smootherstep(up_t);
  if (u > 0.42f)
    y += (m->land.y - clearance) * cod_smootherstep(down_t);
  m->pos = (v3){m->start.x + (m->land.x - m->start.x) * h, y, m->start.z + (m->land.z - m->start.z) * h};

  f32 arc = m_sinf(m_saturatef(u) * COD_PI);
  m->cam_y = -0.045f * arc;
  m->cam_forward = 0.04f * arc;
  m->cam_pitch = -3.1f * COD_DEG * arc;
  m->cam_roll = m->side * 2.3f * COD_DEG * arc;
}

hz_internal void cod_mantle_eval(CodMantle *m, f32 u) {
  if (m->kind == COD_LEDGE_VAULT)
    cod_mantle_eval_vault(m, u);
  else
    cod_mantle_eval_mantle(m, u);
}

hz_internal f32 cod_mantle_progress(const CodMantle *m) { return m->duration > 0.0f ? m_saturatef(m->t / m->duration) : 1.0f; }

// commits a probed ledge; `feet` is the floor-level capsule bottom
hz_internal void cod_mantle_begin(CodMantle *m, const CodLedge *ledge, v3 feet, f32 fx, f32 fz, f32 side, f32 speed) {
  *m = (CodMantle){.active = true,
                   .kind = ledge->kind,
                   .start = feet,
                   .top_y = ledge->top_y,
                   .land = ledge->land,
                   .height = ledge->obstacle_height,
                   .fx = fx,
                   .fz = fz,
                   .side = side != 0.0f ? side : 1.0f};
  if (ledge->kind == COD_LEDGE_VAULT) {
    m->duration = ledge->fast ? COD_MANTLE_VAULT_TIME : COD_MANTLE_VAULT_TIME * 1.45f;
    m->exit_speed = m_maxf(2.6f, speed * 0.88f);
  } else if (ledge->fast) {
    m->duration = COD_MANTLE_VAULT_TIME * 1.12f;
    m->exit_speed = m_maxf(1.8f, speed * 0.72f);
  } else {
    // tall mantles are slower: the weight of hauling yourself up
    f32 f = m_saturatef((ledge->obstacle_height - COD_MANTLE_AUTO_VAULT_MAX) / (COD_MANTLE_MAX_HEIGHT - COD_MANTLE_AUTO_VAULT_MAX));
    m->duration = COD_MANTLE_MANTLE_TIME + (COD_MANTLE_HIGH_TIME - COD_MANTLE_MANTLE_TIME) * f;
    m->exit_speed = 1.35f;
  }
  cod_mantle_eval(m, 0.0f);
}

hz_internal void cod_mantle_end(CodMantle *m) {
  m->active = false;
  m->kind = COD_LEDGE_NONE;
  m->cam_y = 0.0f;
  m->cam_forward = 0.0f;
  m->cam_pitch = 0.0f;
  m->cam_roll = 0.0f;
}

// advances the curve; true while still climbing
hz_internal b32 cod_mantle_step(CodMantle *m, f32 dt) {
  if (!m->active)
    return false;
  m->t += dt;
  cod_mantle_eval(m, cod_mantle_progress(m));
  return m->t < m->duration;
}
