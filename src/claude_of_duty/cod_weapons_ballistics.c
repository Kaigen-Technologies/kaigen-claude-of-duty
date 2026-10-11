// Claude of Duty weapons: terminal ballistics with material-thickness penetration over the engine physics queries.

// the response of each CodSurface to a round (in CodSurface order)
typedef struct CwbSurface {
  f32 pen_depth;   // metres of material a power-1 round fully defeats
  f32 energy_loss; // fraction of the remaining damage lost per pen_depth traversed
  f32 deflect;     // radians of random scatter per pen_depth traversed
} CwbSurface;

hz_internal const CwbSurface cwb_surfaces[COD_SURFACE_COUNT] = {
    [COD_SURFACE_CONCRETE] = {0.055f, 0.62f, 0.055f}, [COD_SURFACE_METAL] = {0.022f, 0.7f, 0.075f},
    [COD_SURFACE_WOOD] = {0.32f, 0.3f, 0.03f},        [COD_SURFACE_DIRT] = {0.26f, 0.45f, 0.05f},
    [COD_SURFACE_SAND] = {0.19f, 0.55f, 0.06f},       [COD_SURFACE_GLASS] = {0.45f, 0.12f, 0.012f},
    [COD_SURFACE_WATER] = {1.1f, 0.5f, 0.09f},        [COD_SURFACE_FOLIAGE] = {3.0f, 0.05f, 0.008f},
    [COD_SURFACE_FABRIC] = {2.2f, 0.06f, 0.01f},      [COD_SURFACE_FLESH] = {0.55f, 0.35f, 0.02f},
    [COD_SURFACE_RUBBER] = {0.28f, 0.4f, 0.04f},      [COD_SURFACE_PLASTER] = {0.7f, 0.12f, 0.02f},
};

#define CWB_MAX_LAYERS 6
#define CWB_EXIT_PROBE 1.6f      // how far past an entry point the exit face is looked for
#define CWB_SHEET_THICKNESS 0.018f // assumed thickness of single-sided geometry
#define CWB_DEFAULT_DROPOFF 0.55f
#define CWB_DEFAULT_RANGE 400.0f
#define CWB_QUERY_LAYERS 4

// every layer a bullet can strike and the filter that picks the categories
typedef struct CwbQuery {
  EcsWorld *w;
  const EcsCollisionLayer *layers[CWB_QUERY_LAYERS];
  u32 count;
  PhysicsQueryFilter filter;
} CwbQuery;

typedef struct CwbHit {
  b32 hit;
  v3 point;
  v3 normal; // faces against the ray
  f32 distance;
  EcsEntity entity;
  i32 sub; // the trimesh triangle, -1 otherwise
  b32 front; // the ray met the face from outside
} CwbHit;

hz_internal CwbQuery cwb_query_make(EcsWorld *w) {
  CwbQuery q = {.w = w};
  const EcsPhysicsWorld *pw = ecs_resource_get(w, EcsPhysicsWorld);
  const EcsCollisionLayer *l[3] = {ecs_physics_get_static_layer(w), ecs_physics_get_layer(w, pw->dynamic_layer),
                                   ecs_physics_get_kinematic_layer(w)};
  for (u32 i = 0; i < 3; i++)
    if (l[i])
      q.layers[q.count++] = l[i];
  const CodHitboxes *hb = ecs_resource_get(w, CodHitboxes);
  if (hb && hb->layer) {
    HzHandleT(CollisionLayerHandle) h;
    mem_cpy(&h, &hb->layer, sizeof(h));
    const EcsCollisionLayer *hl = ecs_physics_get_layer(w, h);
    if (hl)
      q.layers[q.count++] = hl;
  }
  // the query's own category matches any body mask; the body categories pick what a bullet can strike
  q.filter = (PhysicsQueryFilter){.category = 0xFFFFFFFFu,
                                  .mask = COD_COLLIDE_WORLD | COD_COLLIDE_PROP | COD_COLLIDE_HITBOX | COD_COLLIDE_RAGDOLL | COD_COLLIDE_GLASS};
  return q;
}

// a triangle's geometric normal (counter-clockwise outward)
hz_internal v3 cwb_tri_normal(const TriangleCollider *t) {
  return v3_normalize(v3_cross(v3_sub(t->point_b, t->point_a), v3_sub(t->point_c, t->point_a)));
}

// the closest hit along origin + dir * dist across every layer
hz_internal b32 cwb_ray(const CwbQuery *q, v3 origin, v3 dir, f32 dist, CwbHit *out) {
  *out = (CwbHit){.distance = dist, .sub = -1};
  v3 end = v3_add(origin, v3_scale(dir, dist));
  f32 best = 2.0f;
  for (u32 l = 0; l < q->count; l++) {
    PhysicsRaycastResult r = {0};
    i32 idx = 0;
    if (!physics_raycast_layer(origin, end, (CollisionLayer *)&q->layers[l]->layer, q->filter, &r, &idx) || r.fraction >= best)
      continue;
    best = r.fraction;
    out->hit = true;
    out->point = r.position;
    out->normal = r.normal;
    out->distance = r.fraction * dist;
    out->entity = q->layers[l]->entities[idx];
    out->sub = r.subcollider_index;
  }
  if (!out->hit)
    return false;
  // convex shapes report the outward normal, so a ray meeting one from inside points along it
  out->front = v3_dot(dir, out->normal) < 0.0f;
  // a triangle reports the normal facing the ray; its winding says which side the ray came from
  const ColliderBody *cb = ecs_component_get(q->w, out->entity, ColliderBody);
  if (cb && cb->collider.type == ColliderType_TriMesh && out->sub >= 0) {
    const TriMeshColliderBlob *blob = cb->collider.trimesh.blob;
    const TriangleCollider *tris = (const TriangleCollider *)blob_array_get_void((BlobPtr *)&blob->triangles);
    out->front = v3_dot(dir, quat_rotate(cb->rot, cwb_tri_normal(&tris[out->sub]))) < 0.0f;
  }
  return true;
}

// the surface an entity reports to a bullet
hz_internal u32 cwb_surface_of(EcsWorld *w, EcsEntity e) {
  if (!e)
    return COD_SURFACE_CONCRETE;
  const CodHitbox *hb = ecs_component_get(w, e, CodHitbox);
  if (hb)
    return COD_SURFACE_FLESH;
  const CodSurfaceTag *tag = ecs_component_get(w, e, CodSurfaceTag);
  return tag ? tag->surface : COD_SURFACE_CONCRETE;
}

typedef struct CwbThickness {
  f32 distance;
  v3 point;
  v3 normal; // outward from the far face
} CwbThickness;

// distance from an entry hit to where the round leaves the material: the back face of the same solid, else a nominal sheet
hz_internal CwbThickness cwb_thickness(const CwbQuery *q, const CwbHit *entry, v3 dir, f32 probe) {
  const f32 eps = 0.0015f;
  v3 o = v3_add(entry->point, v3_scale(dir, eps));
  CwbHit h;
  CwbThickness t;
  if (cwb_ray(q, o, dir, probe, &h) && !h.front && h.entity == entry->entity) {
    t.distance = h.distance + eps;
    t.point = h.point;
    t.normal = v3_dot(h.normal, dir) > 0.0f ? h.normal : v3_negate(h.normal);
    return t;
  }
  // single-sided geometry has no back face: a nominal sheet along the incidence angle
  f32 cs = m_absf(v3_dot(entry->normal, dir));
  f32 d = CWB_SHEET_THICKNESS / m_maxf(0.2f, cs);
  t.distance = d;
  t.point = v3_add(entry->point, v3_scale(dir, d));
  t.normal = v3_negate(entry->normal);
  return t;
}

hz_internal u32 cwb_hash_f32(f32 f, u32 h) {
  u32 u;
  mem_cpy(&u, &f, 4);
  return (h ^ u) * 0x9e3779b1u + 0x7f4a7c15u;
}

// the round's own rng for the deflection, so a replay of the same shot scatters the same way on any lane
hz_internal CodRng cwb_rng(const CodRound *round) {
  u32 h = 0x85ebca6bu;
  h = cwb_hash_f32(round->origin.x, h);
  h = cwb_hash_f32(round->origin.y, h);
  h = cwb_hash_f32(round->origin.z, h);
  h = cwb_hash_f32(round->dir.x, h);
  h = cwb_hash_f32(round->dir.y, h);
  h = cwb_hash_f32(round->dir.z, h);
  return cod_rng_make(h);
}

hz_internal void cwb_push(CodRound *round, v3 point, v3 normal, v3 incident, u32 surface, f32 damage, b32 exit, EcsEntity e) {
  if (round->impact_count >= COD_BALLISTICS_MAX_LAYERS * 2)
    return;
  round->impacts[round->impact_count++] = (CodImpact){point, normal, incident, surface, damage, exit, e};
}

// the player's chest point soldiers aim for
hz_internal v3 cwb_player_chest(EcsWorld *w) {
  const CodPlayerState *p = ecs_resource_get(w, CodPlayerState);
  return v3_add(p->feet, (v3){0.0f, 1.35f, 0.0f});
}

// the player has no physics collider of its own, so its capsule is a distance to the ray
hz_internal void cwb_test_player(EcsWorld *w, CodRound *round, f32 damage, f32 max_t) {
  round->player_miss = -1.0f;
  const CodPlayerState *ps = ecs_resource_get(w, CodPlayerState);
  if (!ps || !ps->ready || ps->dead)
    return;
  v3 p = cwb_player_chest(w);
  v3 d = v3_sub(p, round->origin);
  f32 t = v3_dot(d, round->dir);
  if (t < 0.5f || t > max_t)
    return;
  f32 miss = v3_length(v3_sub(d, v3_scale(round->dir, t)));
  if (miss > 0.42f) {
    if (miss < 1.6f)
      round->player_miss = miss;
    return;
  }
  round->hit_player = true;
  round->hit_damage = damage * (miss < 0.16f ? 1.25f : 1.0f);
  round->hit_point = p;
}

void cod_ballistics_fire(EcsWorld *w, CodRound *round, f32 damage, f32 penetration, f32 max_dist) {
  round->impact_count = 0;
  round->hit_soldier = 0;
  round->hit_player = false;
  round->player_miss = -1.0f;
  v3 dir = v3_normalize(round->dir);
  round->dir = dir;
  CwbQuery q = cwb_query_make(w);
  CodRng rng = cwb_rng(round);
  f32 range = max_dist > 0.0f ? max_dist : CWB_DEFAULT_RANGE;
  f32 total = range;
  f32 remaining = range;
  f32 power = penetration;
  f32 dropoff = round->dropoff > 0.0f ? round->dropoff : CWB_DEFAULT_DROPOFF;
  f32 base_damage = damage;
  v3 start = round->origin, o = round->origin;
  EcsEntity ignore_soldier = round->shooter.player ? 0 : round->shooter.soldier;
  b32 got_soldier = false;

  for (u32 layer = 0; layer < CWB_MAX_LAYERS && remaining > 0.01f; layer++) {
    CwbHit hit;
    if (!cwb_ray(&q, o, dir, remaining, &hit))
      break;
    // the shooter's own hit volumes are not a target
    const CodHitbox *own = ignore_soldier ? ecs_component_get(w, hit.entity, CodHitbox) : NULL;
    if (own && own->soldier == ignore_soldier) {
      o = v3_add(hit.point, v3_scale(dir, 0.05f));
      remaining -= hit.distance + 0.05f;
      layer--;
      continue;
    }
    f32 travelled = v3_distance(hit.point, start);
    f32 range01 = m_minf(1.0f, travelled / total);
    f32 range_mul = 1.0f - (1.0f - dropoff) * range01 * range01;
    u32 si = cwb_surface_of(w, hit.entity);
    const CwbSurface *props = &cwb_surfaces[si < COD_SURFACE_COUNT ? si : 0];
    cwb_push(round, hit.point, hit.normal, dir, si, damage * range_mul, false, hit.entity);

    const CodHitbox *hb = ecs_component_get(w, hit.entity, CodHitbox);
    if (hb && !got_soldier) {
      got_soldier = true;
      round->hit_soldier = hb->soldier;
      round->hit_zone = hb->zone;
      round->hit_damage = damage * range_mul;
      round->hit_point = hit.point;
    }

    // can the round get through?
    f32 budget = props->pen_depth * power;
    if (budget <= 1e-4f)
      break;
    CwbThickness thick = cwb_thickness(&q, &hit, dir, m_minf(CWB_EXIT_PROBE, remaining));
    if (thick.distance > budget)
      break; // the round stops in the material
    f32 frac = thick.distance / budget;
    f32 ex_damage = damage * range_mul * m_maxf(0.05f, 1.0f - props->energy_loss * frac);
    cwb_push(round, thick.point, thick.normal, dir, si, ex_damage, true, hit.entity);

    // degrade and continue
    damage *= m_maxf(0.05f, 1.0f - props->energy_loss * frac);
    power *= m_maxf(0.0f, 1.0f - frac);
    if (power < 0.02f || damage < 1.0f)
      break;
    if (props->deflect > 0.0f) {
      f32 spread = props->deflect * frac;
      // an orthonormal frame around the direction, then a gaussian yaw and pitch
      v3 up = m_absf(dir.y) > 0.9f ? (v3){1.0f, 0.0f, 0.0f} : (v3){0.0f, 1.0f, 0.0f};
      v3 r = v3_normalize(v3_cross(up, dir));
      v3 s = v3_cross(dir, r);
      f32 a = cod_rng_gauss(&rng) * spread;
      f32 b = cod_rng_gauss(&rng) * spread;
      dir = v3_normalize(v3_add(dir, v3_add(v3_scale(r, a), v3_scale(s, b))));
    }
    // step past the exit face and keep flying
    const f32 eps = 0.004f;
    o = v3_add(thick.point, v3_scale(dir, eps));
    remaining -= hit.distance + thick.distance + eps;
  }

  if (!round->shooter.player) {
    f32 end_t = round->impact_count ? v3_distance(round->origin, round->impacts[0].point) : 200.0f;
    cwb_test_player(w, round, base_damage, end_t);
  }
}

void cod_ballistics_apply_impulses(EcsWorld *w, const CodRound *round) {
  assert_single_thread_begin("cod_ballistics_apply_impulses");
  for (u32 i = 0; i < round->impact_count; i++) {
    const CodImpact *im = &round->impacts[i];
    if (im->exit || !im->entity || !ecs_component_get(w, im->entity, RigidBody))
      continue;
    const ColliderBody *cb = ecs_component_get(w, im->entity, ColliderBody);
    // a ragdoll bone takes the ragdoll shove, a prop the 6 * damage * 0.02 push
    f32 j = cb && (cb->filter.category & COD_COLLIDE_RAGDOLL) ? im->damage * 0.9f : 6.0f * im->damage * 0.02f;
    ecs_rigidbody_add_impulse(w, im->entity, (RigidBodyImpulse){.type = RB_IMPULSE_AT_POINT, .point_or_axis = im->point,
                                                              .impulse = v3_scale(im->incident, j)});
  }
  assert_single_thread_end();
}
