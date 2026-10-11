// Claude of Duty weapons: thrown grenades with a fuse, the dropped magazines, explosions, and the visuals that follow a rigid body.

#define CWP_GRENADE_FUSE 2.35f
#define CWP_GRENADE_RADIUS 0.05f
#define CWP_GRENADE_MASS 0.42f
#define CWP_EXPLOSION_RADIUS 6.5f
#define CWP_EXPLOSION_DAMAGE 120.0f
#define CWP_MAG_LIFETIME 22.0f
#define CWP_MAG_MASS 0.38f
#define CWP_MAG_POOL 2

// the physics body a visual follows; the visual is its own entity so a body needs no renderer state
typedef struct CodVmFollow {
  EcsEntity target;
} CodVmFollow;
ECS_COMPONENT_TYPEDEF(CodVmFollow);

// a rigid body this file owns and still has to dress with its visuals (main lane, after the scope)
typedef struct CodVmProp {
  u32 kind; // 0 grenade, 1 dropped magazine
  u32 weapon;
  b32 attached;
} CodVmProp;
ECS_COMPONENT_TYPEDEF(CodVmProp);

typedef struct CwpGrenadeState {
  EcsQuery *props;   // CodVmProp bodies awaiting visuals
  EcsQuery *bodies;  // every dynamic rigid body, for the blast
  HzHandleT(RendererSubMesh) ball;
  HzHandleT(RendererMaterial) ball_mat;
  EcsEntity mag_body[COD_WEAPON_COUNT][CWP_MAG_POOL];
  f32 mag_age[COD_WEAPON_COUNT][CWP_MAG_POOL];
  u32 mag_next[COD_WEAPON_COUNT];
} CwpGrenadeState;

hz_internal CwpGrenadeState *cwp_gs(EcsWorld *w) {
  CwpRig *r = ecs_resource_get(w, CodWeaponsRig)->rig;
  return (CwpGrenadeState *)r->grenades;
}

void cwp_grenade_init(EcsWorld *w) {
  CwpRig *r = ecs_resource_get(w, CodWeaponsRig)->rig;
  CwpGrenadeState *g = ALLOC(&r->persist, CwpGrenadeState);
  g->props = ecs_query_cached(w, EcsIn(CodVmProp));
  g->bodies = ecs_query_cached(w, EcsIn(RigidBody), EcsIn(WorldTransform), EcsIn(ColliderBody));
  r->grenades = g;
  // the world damping for every rigid body
  EcsRigidBodyWorld *rb = ecs_resource_get_mut(w, EcsRigidBodyWorld);
  if (rb) {
    rb->settings.linear_damping = 0.16f;
    rb->settings.angular_damping = 0.5f;
  }
}

// ---- spawning ----

EcsEntity cod_grenade_spawn(EcsWorld *w, v3 position, v3 velocity, EcsEntity thrower, b32 from_player, b32 flash) {
  EcsEntity e = ecs_entity_create(w);
  ecs_component_set(w, e, ColliderBody,
                    {.collider = collider_sphere(V3_ZERO, CWP_GRENADE_RADIUS),
                     .pos = position,
                     .rot = quat_identity(),
                     .scale = 1.0f,
                     .filter = (PhysicsFilter){.category = COD_COLLIDE_PROP, .mask = COD_COLLIDE_WORLD | COD_COLLIDE_PROP | COD_COLLIDE_GLASS}});
  ecs_component_set(w, e, WorldTransform, {.position = position, .rotation = quat_identity(), .scale = V3_ONE});
  ecs_component_set(w, e, RigidBody,
                    {.velocity = {.linear = velocity},
                     .inverse_mass = 1.0f / CWP_GRENADE_MASS,
                     .friction = 0.7f,
                     .restitution = 0.28f});
  ecs_component_set(w, e, RigidBodySleep, {0});
  ecs_component_set(w, e, CodSurfaceTag, {.surface = COD_SURFACE_METAL});
  ecs_component_set(w, e, CodGrenade, {.from_player = from_player, .thrower = thrower, .fuse = CWP_GRENADE_FUSE, .flash = flash});
  ecs_component_set(w, e, NavObstacleIgnore, {0}); // a thrown grenade never carves the soldiers' navmesh
  ecs_component_set(w, e, CodVmProp, {.kind = 0});
  return e;
}

// the discarded magazine as a real rigid body; called from the weapon system, which owns the pool
hz_internal void cwp_drop_magazine(CwpRig *r) {
  EcsWorld *w = r->world;
  CwpGrenadeState *g = (CwpGrenadeState *)r->grenades;
  const CwpModel *m = &r->models[r->active];
  u32 wp = r->active;
  u32 slot = g->mag_next[wp] % CWP_MAG_POOL;
  g->mag_next[wp]++;
  // reuse the oldest of the two
  if (g->mag_body[wp][slot])
    ecs_entity_destroy(w, g->mag_body[wp][slot]);

  v3 pos = cwp_world_point(r, r->view.mag_pos);
  quat rot = quat_mul(r->cam_rot, quat_mul(r->view.rig_rot, r->view.mag_rot));
  v3 vel = {cod_rng_signed(&r->rng) * 0.25f, -0.7f, cod_rng_signed(&r->rng) * 0.25f};
  vel = v3_add(vel, r->player_velocity);
  v3 spin = {cod_rng_signed(&r->rng) * 14.0f, cod_rng_signed(&r->rng) * 14.0f, cod_rng_signed(&r->rng) * 14.0f};
  // the magazine hangs below its origin: the box sits half its length down
  v3 half = {m->mag_w * 0.5f, m->mag_len * 0.5f, m->mag_d * 0.5f};
  EcsEntity e = ecs_entity_create(w);
  ecs_component_set(w, e, ColliderBody,
                    {.collider = collider_box((v3){0.0f, -half.y, 0.0f}, half),
                     .pos = pos,
                     .rot = rot,
                     .scale = 1.0f,
                     .filter = (PhysicsFilter){.category = COD_COLLIDE_PROP, .mask = COD_COLLIDE_WORLD | COD_COLLIDE_PROP | COD_COLLIDE_GLASS}});
  ecs_component_set(w, e, WorldTransform, {.position = pos, .rotation = rot, .scale = V3_ONE});
  ecs_component_set(w, e, RigidBody,
                    {.velocity = {.linear = vel, .angular = spin},
                     .inverse_mass = 1.0f / CWP_MAG_MASS,
                     .friction = 0.9f,
                     .restitution = 0.18f});
  ecs_component_set(w, e, RigidBodySleep, {0});
  ecs_component_set(w, e, CodSurfaceTag, {.surface = COD_SURFACE_RUBBER});
  ecs_component_set(w, e, CodVmProp, {.kind = 1, .weapon = wp});
  ecs_component_set(w, e, NavObstacleIgnore, {0}); // debris never carves the soldiers' navmesh
  g->mag_body[wp][slot] = e;
  g->mag_age[wp][slot] = 0.0f;
  if (r->events && r->events->eject_count < COD_WEAPON_MAX_EJECTS)
    r->events->ejects[r->events->eject_count++] = (CodEjectEvent){.position = pos, .velocity = vel, .rotation = rot, .spin = spin, .weapon = wp, .magazine = true};
}

// ---- detonation ----

// strength * falloff^2 * mass, pushed away with a 0.35 lift, on dynamic non-ragdoll bodies
hz_internal void cwp_radial_impulse(CwpGrenadeState *g, v3 centre, f32 radius, f32 strength) {
  ecs_query_foreach(g->bodies, bit) {
    const RigidBody *rb = ecs_iter_get_component(&bit, RigidBody, 0);
    const WorldTransform *wt = ecs_iter_get_component(&bit, WorldTransform, 1);
    const ColliderBody *cb = ecs_iter_get_component(&bit, ColliderBody, 2);
    for (i32 i = 0; i < bit.count; i++) {
      if (rb[i].inverse_mass <= 0.0f || (cb[i].filter.category & COD_COLLIDE_RAGDOLL))
        continue;
      v3 d = v3_sub(wt[i].position, centre);
      f32 dist = v3_length(d);
      if (dist > radius)
        continue;
      f32 falloff = 1.0f - dist / radius;
      f32 j = strength * falloff * falloff / rb[i].inverse_mass;
      v3 n = dist > 1e-4f ? v3_scale(d, 1.0f / dist) : (v3){0.0f, 1.0f, 0.0f};
      v3 imp = {n.x * j, n.y * j + j * 0.35f, n.z * j};
      ecs_rigidbody_add_impulse(bit.world, bit.entities[i],
                                (RigidBodyImpulse){.type = RB_IMPULSE_AT_POINT, .point_or_axis = v3_add(wt[i].position, v3_scale(d, 0.05f)), .impulse = imp});
    }
  }
}

// the fuses; a detonation is a CodExplosions event, a shove on the bodies in reach, and the grenade is gone
ecs_system(cwp_grenade_system, EcsResIn(CodWeaponsRig), EcsResInOut(CodExplosions), EcsInOut(CodGrenade), EcsIn(WorldTransform),
           EcsExternIn(RigidBody), EcsExternIn(ColliderBody)) {
  const CodClock *clock = ecs_resource_get(it->world, CodClock);
  CwpRig *r = ecs_sys_get_resource(CodWeaponsRig)->rig;
  CodExplosions *ex = ecs_sys_get_resource_mut(CodExplosions);
  CwpGrenadeState *g = (CwpGrenadeState *)r->grenades;
  f32 dt = clock->live ? clock->dt : 0.0f;
  for (i32 i = 0; i < it->count; i++) {
    CodGrenade *gr = ecs_sys_get_component_mut(CodGrenade, i);
    if (gr->detonated)
      continue;
    gr->fuse -= dt;
    if (gr->fuse > 0.0f)
      continue;
    gr->detonated = true;
    v3 at = ecs_sys_get_component(WorldTransform, i)->position;
    if (ex->count < COD_MAX_EXPLOSIONS)
      ex->items[ex->count++] = (CodExplosionEvent){.position = at,
                                                   .radius = CWP_EXPLOSION_RADIUS,
                                                   .damage = CWP_EXPLOSION_DAMAGE,
                                                   .impulse = CWP_EXPLOSION_DAMAGE * 0.9f,
                                                   .flash = gr->flash,
                                                   .from_player = gr->from_player};
    cwp_radial_impulse(g, at, CWP_EXPLOSION_RADIUS, CWP_EXPLOSION_DAMAGE * 0.9f * 0.06f);
    ecs_entity_destroy(it->world, it->entities[i]);
  }
}

// ---- the dropped magazines' lifetime (single thread: it runs inside the weapon system) ----

hz_internal void cwp_mag_retire(CwpRig *r, f32 dt) {
  CwpGrenadeState *g = (CwpGrenadeState *)r->grenades;
  for (u32 wp = 0; wp < COD_WEAPON_COUNT; wp++) {
    for (u32 k = 0; k < CWP_MAG_POOL; k++) {
      if (!g->mag_body[wp][k])
        continue;
      g->mag_age[wp][k] += dt;
      if (g->mag_age[wp][k] > CWP_MAG_LIFETIME) {
        ecs_entity_destroy(r->world, g->mag_body[wp][k]);
        g->mag_body[wp][k] = 0;
      }
    }
  }
}

// ---- visuals ----

// the follow: a visual copies its body's transform and goes with it
ecs_system(cwp_follow_system, EcsIn(CodVmFollow), EcsInOut(LocalTransform), EcsInOut(WorldTransform), EcsInOut(EcsVisibility)) {
  for (i32 i = 0; i < it->count; i++) {
    EcsEntity target = ecs_sys_get_component(CodVmFollow, i)->target;
    const WorldTransform *src = ecs_entity_is_alive(it->world, target) ? ecs_component_get(it->world, target, WorldTransform) : NULL;
    if (!src) {
      ecs_entity_destroy(it->world, it->entities[i]);
      continue;
    }
    LocalTransform *lt = ecs_sys_get_component_mut(LocalTransform, i);
    WorldTransform *wt = ecs_sys_get_component_mut(WorldTransform, i);
    lt->position = wt->position = src->position;
    lt->rotation = wt->rotation = src->rotation;
    lt->scale = wt->scale = V3_ONE;
    EcsVisibility *vis = ecs_sys_get_component_mut(EcsVisibility, i);
    if (!vis->visible)
      vis->visible = true;
  }
}

// the icosahedron's vertices and faces
hz_internal v3 cwp_ico_vertex(u32 i) {
  const f32 t = (1.0f + m_sqrtf(5.0f)) * 0.5f;
  switch (i) {
  case 0: return (v3){-1.0f, t, 0.0f};
  case 1: return (v3){1.0f, t, 0.0f};
  case 2: return (v3){-1.0f, -t, 0.0f};
  case 3: return (v3){1.0f, -t, 0.0f};
  case 4: return (v3){0.0f, -1.0f, t};
  case 5: return (v3){0.0f, 1.0f, t};
  case 6: return (v3){0.0f, -1.0f, -t};
  case 7: return (v3){0.0f, 1.0f, -t};
  case 8: return (v3){t, 0.0f, -1.0f};
  case 9: return (v3){t, 0.0f, 1.0f};
  case 10: return (v3){-t, 0.0f, -1.0f};
  default: return (v3){-t, 0.0f, 1.0f};
  }
}

hz_internal const u8 cwp_ico_faces[60] = {0, 11, 5, 0, 5, 1, 0, 1, 7, 0, 7, 10, 0, 10, 11, 1, 5, 9, 5, 11, 4, 11, 10, 2, 10, 7, 6, 7, 1, 8,
                                          3, 9, 4, 3, 4, 2, 3, 2, 6, 3, 6, 8, 3, 8, 9, 4, 9, 5, 2, 4, 11, 6, 2, 10, 8, 6, 7, 9, 8, 1};

hz_internal void cwp_ball_vert(CwpGeo *g, u32 *n, f32 radius, v3 p, v3 fn) {
  g->pos[*n] = v3_scale(p, radius);
  g->nrm[*n] = fn;
  g->col[*n] = (v4){1.0f, 1.0f, 1.0f, 1.0f};
  g->idx[*n] = (u16)*n;
  (*n)++;
}

hz_internal void cwp_ball_tri(CwpGeo *g, u32 *n, f32 radius, v3 a, v3 b, v3 c) {
  v3 fn = v3_normalize(v3_cross(v3_sub(b, a), v3_sub(c, a)));
  cwp_ball_vert(g, n, radius, a, fn);
  cwp_ball_vert(g, n, radius, b, fn);
  cwp_ball_vert(g, n, radius, c, fn);
}

// an icosphere (radius, 1 subdivision): 80 flat-shaded faces
hz_internal HzHandleT(RendererSubMesh) cwp_upload_ball(RendererState *rs, f32 radius) {
  HzHandleT(RendererSubMesh) h = {0};
  tctx_temp_allocator_scope(ta, NULL) {
    Allocator *al = &ta.allocator;
    CwpGeo g = {.nv = 240, .ni = 240};
    g.pos = ALLOC_ARRAY_NO_ZERO(al, v3, 240);
    g.nrm = ALLOC_ARRAY_NO_ZERO(al, v3, 240);
    g.uv = ALLOC_ARRAY(al, v2, 240);
    g.col = ALLOC_ARRAY_NO_ZERO(al, v4, 240);
    g.idx = ALLOC_ARRAY_NO_ZERO(al, u16, 240);
    u32 n = 0;
    for (u32 f = 0; f < 20; f++) {
      v3 a = v3_normalize(cwp_ico_vertex(cwp_ico_faces[f * 3])), b = v3_normalize(cwp_ico_vertex(cwp_ico_faces[f * 3 + 1])),
         c = v3_normalize(cwp_ico_vertex(cwp_ico_faces[f * 3 + 2]));
      v3 ab = v3_normalize(v3_add(a, b)), bc = v3_normalize(v3_add(b, c)), ca = v3_normalize(v3_add(c, a));
      cwp_ball_tri(&g, &n, radius, a, ab, ca);
      cwp_ball_tri(&g, &n, radius, b, bc, ab);
      cwp_ball_tri(&g, &n, radius, c, ca, bc);
      cwp_ball_tri(&g, &n, radius, ab, bc, ca);
    }
    h = cwp_upload_geo(rs, &g);
  }
  return h;
}

// main lane after the scope: the bodies this file created get their visuals
hz_internal void cwp_attach_props(EcsWorld *w, CwpRig *r) {
  CwpGrenadeState *g = (CwpGrenadeState *)r->grenades;
  CwpScene *s = r->scene;
  RendererState *rs = ecs_resource_get_mut(w, RendererState);
  ecs_query_foreach(g->props, it) {
    const CodVmProp *props = ecs_iter_get_component(&it, CodVmProp, 0);
    for (i32 i = 0; i < it.count; i++) {
      if (props[i].attached)
        continue;
      EcsEntity body = it.entities[i];
      u32 kind = props[i].kind, weapon = props[i].weapon;
      ecs_component_get_mut(w, body, CodVmProp)->attached = true;
      if (kind == 0) {
        if (!handle_is_valid(g->ball)) {
          g->ball = cwp_upload_ball(rs, 0.045f);
          g->ball_mat = renderer_create_material_instance(rs, s->base[CWP_BASE_LIT]);
          material_set_vec4(rs, g->ball_mat, "color", cwp_srgb4(0x2c3226));
          material_set_float(rs, g->ball_mat, "metallic", 0.85f);
          material_set_float(rs, g->ball_mat, "smoothness", 0.38f);
        }
        EcsEntity v = ecs_renderer_spawn(w, rs,
                                         &(EcsRendererSpawnDesc){.submesh = g->ball,
                                                                 .material = g->ball_mat,
                                                                 .transform = {.position = V3_ZERO, .rotation = quat_identity(), .scale = V3_ONE},
                                                                 .casts_shadow = true,
                                                                 .hidden = true});
        ecs_component_set(w, v, CodVmFollow, {.target = body});
      } else {
        for (u32 k = 0; k < s->mag_count[weapon]; k++) {
          EcsEntity v = ecs_renderer_spawn(w, rs,
                                           &(EcsRendererSpawnDesc){.submesh = s->mag_sub[weapon][k],
                                                                   .material = s->mat[s->mag_mat[weapon][k]],
                                                                   .transform = {.position = V3_ZERO, .rotation = quat_identity(), .scale = V3_ONE},
                                                                   .casts_shadow = true,
                                                                   .hidden = true,
                                                                   .sort_priority = cwp_mat_priority(s->mag_mat[weapon][k])});
          ecs_component_set(w, v, CodVmFollow, {.target = body});
        }
      }
    }
  }
}
