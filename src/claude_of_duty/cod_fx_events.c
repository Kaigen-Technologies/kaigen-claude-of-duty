// Claude of Duty effects: the consumers of the weapons', soldiers', player's and explosions' events.

// the decal tiles
typedef enum CodFxDecalTile {
  CF_D_HOLE_CONCRETE = 0,
  CF_D_HOLE_CONCRETE_B,
  CF_D_HOLE_METAL,
  CF_D_HOLE_WOOD,
  CF_D_HOLE_PLASTER,
  CF_D_GLASS_CRACK,
  CF_D_BLOOD_A,
  CF_D_BLOOD_B,
  CF_D_SCORCH,
  CF_D_IMPACT_DIRT,
  CF_D_IMPACT_SAND,
  CF_D_SCRAPE,
  CF_D_RIPPLE,
  CF_D_HOLE_GLASS,
  CF_D_SMUDGE,
  CF_D_TEAR,
  CF_D_COUNT,
} CodFxDecalTile;

// one decal a consumer wants projected; the main lane places it
typedef struct CodFxDecalReq {
  v3 point;
  v3 normal;
  u32 tile;
  f32 size;
  f32 roll;
  f32 life;    // seconds
  f32 fade;    // the share of the life after which it fades out
  f32 opacity;
  f32 max_angle; // degrees the surface may lean from the projection before the mark fades
  f32 depth;
} CodFxDecalReq;
ARRAY_TYPEDEF_ALL(CodFxDecalReq);

typedef struct CodFxDecalQueue {
  ConcurrentArray(CodFxDecalReq) q;
  i32 dropped_atomic;
} CodFxDecalQueue;

// the registered effects and the decal queue every consumer reads; set once, immutable per frame
typedef struct CodFxAssets {
  CodFxDecalQueue *decals;
  HzHandleT(VfxAsset) impact[COD_SURFACE_COUNT];
  HzHandleT(VfxAsset) impact_graze;
  HzHandleT(VfxAsset) muzzle[COD_WEAPON_COUNT];
  HzHandleT(VfxAsset) flash_light;
  HzHandleT(VfxAsset) tracer[CF_TRACER_COUNT];
  HzHandleT(VfxAsset) explosion;
  HzHandleT(VfxAsset) death;
  HzHandleT(VfxAsset) land;
  HzHandleT(VfxAsset) step;
} CodFxAssets;
ECS_COMPONENT_TYPEDEF(CodFxAssets);

// what a consumer needs to raise requests
typedef struct CodFxCtx {
  const VfxRequests *rq;
  const CodFxAssets *as;
  const CodFxState *st;
  const CodLook *look;
  CodFxDecalQueue *decals;
  const CodPhys *phys;
  CodFxViewFlash *view_flash; // the player's pass only
} CodFxCtx;

// a free light first, else the one furthest through its life unless it outranks this flash
hz_internal void cod_fx_view_flash(CodFxViewFlash *pool, v3 at, v3 color, f32 peak, f32 duration, f32 decay, f32 distance, u32 priority) {
  CodFxViewLight *best = NULL;
  f32 best_score = -1e30f;
  for (u32 i = 0; i < COD_FX_VIEW_FLASHES; i++) {
    CodFxViewLight *e = &pool->lights[i];
    f32 score = e->age >= e->duration ? 1e6f : e->age / e->duration - (f32)e->priority * 0.5f;
    if (score > best_score) {
      best_score = score;
      best = e;
    }
  }
  if (!best || (best_score < 1e5f && best->priority > priority))
    return;
  *best = (CodFxViewLight){.position = at, .color = color, .intensity = peak, .distance = distance, .peak = peak, .duration = duration,
                           .decay = decay, .priority = priority};
}

// a 4 ms rise, exponential decay and a tail that lands on zero at the duration
hz_internal void cod_fx_view_flash_update(CodFxViewFlash *pool, f32 dt) {
  for (u32 i = 0; i < COD_FX_VIEW_FLASHES; i++) {
    CodFxViewLight *e = &pool->lights[i];
    if (e->age >= e->duration) {
      e->intensity = 0.0f;
      continue;
    }
    e->age += dt;
    f32 t = e->age, u = m_minf(1.0f, t / e->duration);
    e->intensity = e->age >= e->duration ? 0.0f : e->peak * m_minf(1.0f, t / 0.004f) * m_expf(-e->decay * t) * (1.0f - u * u);
  }
}

hz_internal void cod_fx_decal(const CodFxCtx *c, v3 point, v3 normal, u32 tile, f32 size, f32 roll, f32 life, f32 fade, f32 opacity, f32 max_angle, f32 depth) {
  if (!c->decals) return;
  u32 idx = concurrent_arr_reserve_idx(c->decals->q);
  if (idx >= c->decals->q.cap) {
    atomic_u32_inc_eval(&c->decals->dropped_atomic);
    return;
  }
  c->decals->q.items[idx] = (CodFxDecalReq){point, normal, tile, size, roll, life, fade, opacity, max_angle, depth};
}

hz_internal void cod_fx_request(const CodFxCtx *c, HzHandleT(VfxAsset) h, const VfxPlayDesc *play) {
  if (handle_is_valid(h)) vfx_request(c->rq, h, play);
}

hz_internal f32 cod_fx_seed_f(CodRng *r) { return cod_rng_float(r) * 0.98f; }

hz_internal v3 cod_fx_reflect(v3 i, v3 n) { return v3_sub(i, v3_scale(n, 2.0f * v3_dot(i, n))); }

// the second axis of a surface's recipes: the reflected ray, the glass's forward throw, the flesh's spray
hz_internal v3 cod_fx_alt_axis(u32 surface, v3 n, v3 inc) {
  v3 r = cod_fx_reflect(inc, n);
  switch (surface) {
  case COD_SURFACE_PLASTER: return v3_normalize(v3_add(r, v3_scale(n, 1.3f)));
  case COD_SURFACE_METAL: return v3_normalize(v3_add(v3_scale(r, 0.8f), v3_scale(n, 0.2f)));
  case COD_SURFACE_GLASS: return v3_normalize(v3_sub(v3_scale(inc, 0.8f), v3_scale(n, 0.2f)));
  case COD_SURFACE_FLESH: return v3_normalize(v3_sub(v3_scale(inc, 0.75f), v3_scale(n, 0.25f)));
  case COD_SURFACE_FOLIAGE: return v3_normalize(r);
  default: return v3_normalize(v3_add(r, n));
  }
}

// the bore with its crater, and the much larger structureless wash around it
hz_internal void cod_fx_bullet_hole(const CodFxCtx *c, CodRng *rng, v3 p, v3 n, u32 tile, f32 mn, f32 mx, f32 e, f32 soot, f32 halo_scale, f32 halo_life,
                                    f32 life, f32 max_angle) {
  f32 size = cod_rng_range(rng, mn, mx) * (0.9f + e * 0.1f);
  cod_fx_decal(c, p, n, tile, size, cod_rng_float(rng) * 6.2831853f, life, 0.72f, 1.0f, max_angle, m_maxf(0.03f, size * 0.85f));
  b32 sooty = cod_rng_float(rng) < soot;
  f32 hs = cod_rng_range(rng, 0.18f, 0.3f) * halo_scale;
  f32 op = cod_rng_range(rng, 0.08f, 0.15f) * (sooty ? 0.8f : 1.0f);
  cod_fx_decal(c, p, n, sooty ? CF_D_SCORCH : CF_D_SMUDGE, hs, cod_rng_float(rng) * 6.2831853f, halo_life, 0.4f, op, 74.0f, m_maxf(0.04f, hs * 0.32f));
}

// blood on whatever is behind the body a round went through
hz_internal void cod_fx_blood_behind(const CodFxCtx *c, CodRng *rng, v3 point, v3 inc) {
  if (!c->phys) return;
  CodHit hit;
  if (!cod_phys_ray(c->phys, point, v3_add(point, v3_scale(inc, 2.6f)), &hit)) return;
  f32 size = cod_rng_range(rng, 0.32f, 0.62f);
  f32 op = cod_rng_range(rng, 0.7f, 1.0f);
  cod_fx_decal(c, hit.point, hit.normal, cod_rng_float(rng) < 0.5f ? CF_D_BLOOD_A : CF_D_BLOOD_B, size, cod_rng_float(rng) * 6.2831853f, 90.0f, 0.8f, op, 70.0f,
               m_maxf(0.04f, size * 0.32f));
}

// the decals of a surface
hz_internal void cod_fx_impact_decals(const CodFxCtx *c, CodRng *rng, const CodImpact *im, f32 e, f32 grazing) {
  v3 p = im->point, n = im->normal;
  switch (im->surface) {
  case COD_SURFACE_CONCRETE:
    cod_fx_bullet_hole(c, rng, p, n, cod_rng_float(rng) < 0.5f ? CF_D_HOLE_CONCRETE : CF_D_HOLE_CONCRETE_B, 0.05f, 0.075f, e, 0.4f, 1.1f, 40.0f, 90.0f, 62.0f);
    break;
  case COD_SURFACE_PLASTER: cod_fx_bullet_hole(c, rng, p, n, CF_D_HOLE_PLASTER, 0.045f, 0.07f, e, 0.15f, 1.0f, 40.0f, 90.0f, 62.0f); break;
  case COD_SURFACE_METAL:
    if (grazing > 0.6f) {
      f32 size = cod_rng_range(rng, 0.16f, 0.28f);
      cod_fx_decal(c, p, n, CF_D_SCRAPE, size, cod_rng_float(rng) * 6.2831853f, 70.0f, 0.72f, 1.0f, 62.0f, m_maxf(0.04f, size * 0.32f));
    } else {
      cod_fx_bullet_hole(c, rng, p, n, CF_D_HOLE_METAL, 0.045f, 0.07f, e, 0.85f, 0.8f, 55.0f, 90.0f, 62.0f);
    }
    break;
  case COD_SURFACE_WOOD: cod_fx_bullet_hole(c, rng, p, n, CF_D_HOLE_WOOD, 0.05f, 0.075f, e, 0.5f, 0.85f, 40.0f, 90.0f, 62.0f); break;
  case COD_SURFACE_DIRT:
  case COD_SURFACE_SAND: {
    b32 sand = im->surface == COD_SURFACE_SAND;
    cod_fx_bullet_hole(c, rng, p, n, sand ? CF_D_IMPACT_SAND : CF_D_IMPACT_DIRT, 0.085f, 0.15f, e, 0.0f, sand ? 1.15f : 1.0f, 30.0f, 60.0f, 78.0f);
  } break;
  case COD_SURFACE_GLASS: {
    f32 size = cod_rng_range(rng, 0.3f, 0.55f);
    cod_fx_decal(c, p, n, cod_rng_float(rng) < 0.5f ? CF_D_GLASS_CRACK : CF_D_HOLE_GLASS, size, cod_rng_float(rng) * 6.2831853f, 120.0f, 0.72f, 1.0f, 40.0f,
                 m_maxf(0.04f, size * 0.32f));
  } break;
  case COD_SURFACE_WATER: {
    f32 size = cod_rng_range(rng, 0.45f, 0.7f);
    cod_fx_decal(c, p, n, CF_D_RIPPLE, size, cod_rng_float(rng) * 6.2831853f, 2.6f, 0.15f, 0.8f, 80.0f, m_maxf(0.04f, size * 0.32f));
  } break;
  case COD_SURFACE_FABRIC:
  case COD_SURFACE_RUBBER: {
    f32 size = cod_rng_range(rng, 0.09f, 0.15f);
    cod_fx_decal(c, p, n, CF_D_TEAR, size, cod_rng_float(rng) * 6.2831853f, 80.0f, 0.72f, 1.0f, 62.0f, m_maxf(0.04f, size * 0.32f));
  } break;
  case COD_SURFACE_FLESH: cod_fx_blood_behind(c, rng, p, im->incident); break;
  default: break;
  }
}

// bullet:impact: the surface's recipe at the point, along the round's direction, with the round's energy
hz_internal void cod_fx_impact(const CodFxCtx *c, const CodImpact *im, u32 seed) {
  CodRng rng = cod_rng_make(seed);
  f32 e = m_clampf(0.7f + im->damage / 55.0f, 0.7f, 1.7f);
  if (im->exit) e *= 0.75f;
  v3 inc = v3_length(im->incident) > 0.001f ? v3_normalize(im->incident) : v3_scale(im->normal, -1.0f);
  f32 grazing = 1.0f - m_fabsf(v3_dot(inc, im->normal));
  u32 surface = im->surface < COD_SURFACE_COUNT ? im->surface : COD_SURFACE_CONCRETE;
  v3 alt = cod_fx_alt_axis(surface, im->normal, inc);
  HzHandleT(VfxAsset) h = c->as->impact[surface];
  if (surface == COD_SURFACE_METAL && grazing > 0.55f) h = c->as->impact_graze;
  cod_fx_request(c, h, &(VfxPlayDesc){.at = im->point, .dir = im->normal, .scale = e, .color = {1.0f, 1.0f, 1.0f, 0.5f},
                                      .user = {alt.x, alt.y, alt.z, cod_fx_seed_f(&rng)}});
  cod_fx_impact_decals(c, &rng, im, e, grazing);
}

// weapon:fire: the flash at the crown, its light and, in first person, the light the gun itself catches
hz_internal void cod_fx_muzzle(const CodFxCtx *c, v3 pos, v3 dir, u32 weapon, f32 gain, f32 light, f32 scale, b32 view, u32 seed) {
  CodRng rng = cod_rng_make(seed);
  CodFxMuzzleProfile pr = cod_fx_muzzle_profile(weapon);
  f32 g = gain * (0.86f + cod_rng_float(&rng) * 0.28f);
  f32 sc = pr.scale * scale * (0.88f + cod_rng_float(&rng) * 0.24f);
  v3 bore = v3_normalize(dir);
  u32 w = weapon < COD_WEAPON_COUNT ? weapon : COD_WEAPON_RIFLE;
  v3 light_at = v3_add(pos, v3_scale(bore, 0.1f));
  cod_fx_request(c, c->as->muzzle[w], &(VfxPlayDesc){.at = pos, .dir = bore, .scale = sc, .color = {1.0f, 1.0f, 1.0f, m_minf(g, 2.0f) * 0.5f},
                                                     .user = {0.0f, 0.0f, 0.0f, cod_fx_seed_f(&rng) + (view ? 2.0f : 0.0f)}});
  cod_fx_request(c, c->as->flash_light, &(VfxPlayDesc){.at = light_at, .scale = sc, .intensity_scale = pr.light * 0.18f * g * light});
  if (view && c->view_flash) {
    // the core's blackbody, rolled per shot, peaking at a fixed ratio of the viewmodel key, 4 cm above the bore
    f32 temp = pr.temp * (0.96f + cod_rng_float(&rng) * 0.09f);
    v3 color = cod_fx_blackbody((3800.0f + cod_rng_float(&rng) * 600.0f) * temp);
    f32 strength = g * light * (pr.light / cod_fx_muzzle_profile(COD_WEAPON_RIFLE).light);
    f32 peak = m_maxf(0.04f, c->look->view_key * 0.72f * m_clampf(strength, 0.05f, 2.2f));
    v3 at = v3_add(v3_add(pos, v3_scale(bore, 0.03f)), (v3){0.0f, 0.04f, 0.0f});
    cod_fx_view_flash(c->view_flash, at, color, peak, 0.09f, 8.0f, 1.6f, 2);
  }
}

// bullet:tracer: three beams, colours in display units
hz_internal void cod_fx_tracer(const CodFxCtx *c, v3 from, v3 to, f32 speed) {
  v3 d = v3_sub(to, from);
  f32 dist = v3_length(d);
  if (dist < 0.35f) return;
  (void)speed;
  v3 dir = v3_scale(d, 1.0f / dist);
  v3 start = v3_add(from, v3_scale(dir, 0.25f));
  f32 u = c->st->unit;
  static const f32 col[CF_TRACER_COUNT][4] = {{1.0f, 0.46f, 0.15f, 21.0f}, {1.0f, 0.285f, 0.08f, 4.0f}, {1.0f, 0.72f, 0.45f, 24.0f}};
  for (u32 i = 0; i < CF_TRACER_COUNT; i++)
    cod_fx_request(c, c->as->tracer[i], &(VfxPlayDesc){.at = start, .to = to, .color = {col[i][0] * col[i][3] * u, col[i][1] * col[i][3] * u, col[i][2] * col[i][3] * u, 1.0f}});
}

// the soot ring under a blast
hz_internal void cod_fx_scorch(const CodFxCtx *c, v3 at, f32 radius) {
  v3 p = at, n = {0.0f, 1.0f, 0.0f};
  CodHit hit;
  if (c->phys && cod_phys_ray(c->phys, (v3){at.x, at.y + 0.4f, at.z}, (v3){at.x, at.y + 0.4f - (radius * 1.5f + 1.0f), at.z}, &hit)) {
    p = hit.point;
    n = hit.normal;
  }
  cod_fx_decal(c, p, n, CF_D_SCORCH, radius * 1.05f, 0.0f, 120.0f, 0.55f, 0.9f, 80.0f, radius * 0.35f);
}

// explosion: fireball, shockwave, debris, smoke column, light, scorch
hz_internal void cod_fx_explosion(const CodFxCtx *c, const CodExplosionEvent *e, u32 seed) {
  CodRng rng = cod_rng_make(seed);
  f32 r = m_maxf(0.6f, e->radius > 0.0f ? e->radius : 5.0f);
  cod_fx_request(c, c->as->explosion, &(VfxPlayDesc){.at = e->position, .scale = r, .intensity_scale = r, .color = {1.0f, 1.0f, 1.0f, 0.5f},
                                                     .user = {0.0f, 0.0f, 0.0f, cod_fx_seed_f(&rng)}});
  cod_fx_scorch(c, e->position, r);
}

// actor:death: a mist thrown up from the body and blood on the ground under it
hz_internal void cod_fx_death(const CodFxCtx *c, v3 point, u32 seed) {
  CodRng rng = cod_rng_make(seed);
  cod_fx_request(c, c->as->death, &(VfxPlayDesc){.at = point, .color = {1.0f, 1.0f, 1.0f, 0.5f}, .user = {0.0f, 0.0f, 0.0f, cod_fx_seed_f(&rng)}});
  CodHit hit;
  if (c->phys && cod_phys_ray(c->phys, (v3){point.x, point.y + 1.0f, point.z}, (v3){point.x, point.y - 6.0f, point.z}, &hit)) {
    f32 size = cod_rng_range(&rng, 0.5f, 0.9f);
    cod_fx_decal(c, hit.point, hit.normal, cod_rng_float(&rng) < 0.5f ? CF_D_BLOOD_A : CF_D_BLOOD_B, size, cod_rng_float(&rng) * 6.2831853f, 120.0f, 0.85f, 1.0f,
                 80.0f, m_maxf(0.04f, size * 0.32f));
  }
}

// player:land: a ring of dust sized by the fall
hz_internal void cod_fx_land(const CodFxCtx *c, v3 feet, f32 speed, u32 seed) {
  if (speed < 3.2f) return;
  CodRng rng = cod_rng_make(seed);
  f32 strength = m_clampf((speed - 3.0f) / 7.0f, 0.2f, 1.3f);
  f32 n = (7.0f * CF_PSCALE * strength + 2.0f) / (7.0f * CF_PSCALE + 2.0f);
  cod_fx_request(c, c->as->land, &(VfxPlayDesc){.at = feet, .count_scale = n, .speed_scale = strength, .color = {1.0f, 1.0f, 1.0f, strength * 0.5f},
                                                .user = {0.0f, 0.0f, 0.0f, cod_fx_seed_f(&rng)}});
}

// player:footstep: a small puff while running, in 55% of the steps
hz_internal void cod_fx_footstep(const CodFxCtx *c, v3 foot, b32 running, u32 seed) {
  if (!running) return;
  CodRng rng = cod_rng_make(seed);
  if (cod_rng_float(&rng) > 0.55f) return;
  v3 at = {foot.x + cod_rng_signed(&rng) * 0.08f, foot.y, foot.z + cod_rng_signed(&rng) * 0.08f};
  cod_fx_request(c, c->as->step, &(VfxPlayDesc){.at = at, .color = {1.0f, 1.0f, 1.0f, 0.5f}, .user = {0.0f, 0.0f, 0.0f, cod_fx_seed_f(&rng)}});
}

// the daylight factor for the soldiers' flash gains
hz_internal f32 cod_fx_daylight(const CodLook *look) { return m_clampf(m_sinf(m_maxf(0.0f, look->sun_altitude)) * 4.0f, 0.0f, 1.0f); }

hz_internal u32 cod_fx_mix(u32 a, u32 b) {
  u32 x = a * 0x9e3779b9u ^ (b + 0x7f4a7c15u + (a << 6) + (a >> 2));
  x ^= x >> 16;
  x *= 0x85ebca6bu;
  x ^= x >> 13;
  return x;
}

// a round's impacts and tracer
hz_internal void cod_fx_round(const CodFxCtx *c, const CodRound *r, u32 seed) {
  for (u32 i = 0; i < r->impact_count; i++) cod_fx_impact(c, &r->impacts[i], cod_fx_mix(seed, i + 1u));
  if (r->has_tracer) cod_fx_tracer(c, r->tracer.from, r->tracer.to, r->tracer.speed);
}

// a soldier's own events: his flash, his round's impacts and tracer, his death
ecs_system(cod_fx_soldier_system, EcsIn(CodSoldierEventsComp), EcsResIn(VfxRequests), EcsResIn(CodFxAssets), EcsResIn(CodFxState), EcsResIn(CodLook), EcsResIn(CodClock), EcsResIn(EcsPhysicsWorld)) {
  const CodFxState *st = ecs_sys_get_resource(CodFxState);
  if (!st->ready) return;
  const CodClock *clock = ecs_sys_get_resource(CodClock);
  CodPhys phys = cod_phys_make(it->world);
  CodFxCtx c = {ecs_sys_get_resource(VfxRequests), ecs_sys_get_resource(CodFxAssets), st, ecs_sys_get_resource(CodLook),
                ecs_sys_get_resource(CodFxAssets)->decals, &phys, NULL};
  f32 day = cod_fx_daylight(c.look);
  for (i32 i = 0; i < it->count; i++) {
    const CodSoldierEvents *ev = &ecs_sys_get_component(CodSoldierEventsComp, i)->ev;
    if (ev->frame != clock->frame) continue;
    u32 seed = cod_fx_mix(ev->shot.seed, clock->frame);
    if (ev->fired) {
      cod_fx_muzzle(&c, ev->shot.origin, ev->shot.dir, ev->shot.weapon, 0.12f + 0.5f * (1.0f - day), 0.006f + 0.05f * (1.0f - day), 0.8f, false, seed);
      cod_fx_round(&c, &ev->round, seed ^ 0xa5a5u);
    }
    if (ev->killed) cod_fx_death(&c, ev->death_point, seed ^ 0x5a5au);
  }
}

// the player's weapon, landing and footsteps: resources with no entity to iterate, one pass
ecs_system(cod_fx_player_system, EcsResIn(CodWeaponState), EcsResIn(CodPlayerState), EcsResIn(VfxRequests), EcsResIn(CodFxAssets), EcsResIn(CodFxState),
           EcsResIn(CodLook), EcsResIn(EcsPhysicsWorld), EcsResInOut(CodFxViewFlash)) {
  const CodFxState *st = ecs_sys_get_resource(CodFxState);
  if (!st->ready) return;
  // eight terms at most: the clock is settled by the scope's first flush
  const CodClock *clock = ecs_resource_get(it->world, CodClock);
  CodPhys phys = cod_phys_make(it->world);
  CodFxViewFlash *view_flash = ecs_sys_get_resource_mut(CodFxViewFlash);
  CodFxCtx c = {ecs_sys_get_resource(VfxRequests), ecs_sys_get_resource(CodFxAssets), st, ecs_sys_get_resource(CodLook),
                ecs_sys_get_resource(CodFxAssets)->decals, &phys, view_flash};
  const CodWeaponEvents *we = &ecs_sys_get_resource(CodWeaponState)->events;
  for (u32 i = 0; i < we->shot_count && i < COD_WEAPON_MAX_SHOTS; i++) {
    const CodShotEvent *s = &we->shots[i];
    cod_fx_muzzle(&c, s->viewmodel ? s->view_muzzle : s->origin, s->dir, s->weapon, 1.0f, 1.0f, 1.0f, s->viewmodel, cod_fx_mix(s->seed, clock->frame));
  }
  for (u32 i = 0; i < we->round_count && i < COD_WEAPON_MAX_ROUNDS; i++) cod_fx_round(&c, &we->rounds[i], cod_fx_mix(clock->frame, 0x100u + i));
  const CodPlayerEvents *pe = &ecs_sys_get_resource(CodPlayerState)->events;
  if (pe->landed) cod_fx_land(&c, pe->land_position, pe->land_speed, cod_fx_mix(clock->frame, 0x200u));
  if (pe->footstep) cod_fx_footstep(&c, pe->footstep_position, pe->footstep_running, cod_fx_mix(clock->frame, 0x300u));
  cod_fx_view_flash_update(view_flash, clock->live ? clock->dt : 0.0f);
}

// the frame's explosions: one resource, one pass
ecs_system(cod_fx_explosion_system, EcsResIn(CodExplosions), EcsResIn(VfxRequests), EcsResIn(CodFxAssets), EcsResIn(CodFxState), EcsResIn(CodLook),
           EcsResIn(CodClock), EcsResIn(EcsPhysicsWorld)) {
  const CodFxState *st = ecs_sys_get_resource(CodFxState);
  if (!st->ready) return;
  const CodClock *clock = ecs_sys_get_resource(CodClock);
  CodPhys phys = cod_phys_make(it->world);
  CodFxCtx c = {ecs_sys_get_resource(VfxRequests), ecs_sys_get_resource(CodFxAssets), st, ecs_sys_get_resource(CodLook),
                ecs_sys_get_resource(CodFxAssets)->decals, &phys, NULL};
  const CodExplosions *ex = ecs_sys_get_resource(CodExplosions);
  for (u32 i = 0; i < ex->count && i < COD_MAX_EXPLOSIONS; i++)
    if (!ex->items[i].flash) cod_fx_explosion(&c, &ex->items[i], cod_fx_mix(clock->frame, 0x400u + i));
}
