// Claude of Duty AI spawning: the soldier entity and its hit volumes, the garrison, grenades, the player's intake, the totals.

#define CAI_CASUALTY_NONE 0

typedef struct CaiHitboxDef {
  u32 zone; // CodHitZone
  u32 bone_a, bone_b;
  f32 radius;
} CaiHitboxDef;

// head, chest, pelvis, arms, legs (the damage scales are cai_zone_scale)
hz_internal const CaiHitboxDef cai_hitbox_defs[CAI_HITBOX_COUNT] = {
    {COD_ZONE_HEAD, COD_BONE_HEAD, COD_BONE_HEAD_TOP, 0.098f},
    {COD_ZONE_TORSO, COD_BONE_SPINE1, COD_BONE_NECK, 0.185f},
    {COD_ZONE_PELVIS, COD_BONE_HIPS, COD_BONE_SPINE1, 0.175f},
    {COD_ZONE_ARM, COD_BONE_UPPER_ARM_R, COD_BONE_HAND_R, 0.072f},
    {COD_ZONE_ARM, COD_BONE_UPPER_ARM_L, COD_BONE_HAND_L, 0.072f},
    {COD_ZONE_LEG, COD_BONE_UP_LEG_R, COD_BONE_FOOT_R, 0.105f},
    {COD_ZONE_LEG, COD_BONE_UP_LEG_L, COD_BONE_FOOT_L, 0.105f},
};

hz_internal const f32 cai_variant_scale[COD_VARIANT_COUNT] = {1.0f, 0.985f, 1.025f};

// everything one spawn needs
typedef struct CaiSpawn {
  u32 variant;
  v3 pos;
  f32 yaw;
  v3 patrol[CAI_MAX_PATROL];
  u32 patrol_count;
  u32 squad; // index + 1
} CaiSpawn;

hz_internal u32 cai_create_squad(CodAiShared *sh) {
  for (u32 i = 0; i < CAI_MAX_SQUADS; i++) {
    if (sh->squads[i].used)
      continue;
    sh->squads[i] = (CaiSquad){.used = true, .id = i + 1, .rng = cod_rng_fork(&sh->rng), .peek_tokens = 1, .grenade_cooldown = 6.0f};
    return i + 1;
  }
  log_error("ai: more than % squads", fmt_u32(CAI_MAX_SQUADS));
  return 0;
}

hz_internal void cai_squad_add(CodAiShared *sh, u32 squad) {
  if (!squad)
    return;
  CaiSquad *sq = &sh->squads[squad - 1];
  sq->member_count++;
  u32 tokens = (u32)m_floorf((f32)sq->member_count * 0.5f + 0.5f);
  sq->peek_tokens = tokens < 1 ? 1 : tokens;
}

// the soldier entity and its hit volumes
hz_internal EcsEntity cai_spawn_soldier(EcsWorld *w, CodAiShared *sh, CodAiState *st, const CaiSpawn *sp) {
  if (sh->next_slot >= CAI_MAX_SOLDIERS) {
    log_error("ai: more than % soldiers", fmt_u32(CAI_MAX_SOLDIERS));
    return ECS_ENTITY_INVALID;
  }
  u32 variant = sp->variant < COD_VARIANT_COUNT ? sp->variant : 0;
  f32 scale = cai_variant_scale[variant];

  // the rng draws, in order: the agent's fork, the variant build's fork on first use, the animator's fork, the timers
  CodRng rng = cod_rng_fork(&sh->rng);
  if (!(sh->variant_seen & (1u << variant))) {
    sh->variant_seen |= 1u << variant;
    cod_rng_fork(&sh->rng);
  }
  cod_rng_u32(&rng);

  CodSoldierBrain b = {0};
  b.id = st->next_id++;
  b.slot = sh->next_slot++;
  b.squad = sp->squad;
  b.mode = COD_AI_IDLE;
  b.rng = rng;
  b.variant = variant;
  b.scale = scale;
  b.height = CAI_SOLDIER_HEIGHT * scale;
  b.radius = CAI_SOLDIER_RADIUS * scale;
  b.eye_height = 1.665f * scale;
  b.alive = true;
  b.health = 100.0f;
  b.position = sp->pos;
  b.yaw = b.target_yaw = sp->yaw;
  b.last_known_age = HZ_INF;
  b.weapon_range = 60.0f;
  b.fire_rate = variant == COD_VARIANT_IRREGULAR ? 8.2f : 10.5f;
  b.burst_cooldown = cod_rng_range(&b.rng, 0.4f, 1.4f);
  b.ammo = 30;
  b.spread = 0.032f;
  b.weapon_damage = 17.0f;
  b.peek_timer = cod_rng_range(&b.rng, 0.5f, 2.5f);
  for (u32 k = 0; k < CAI_COVER_MEMORY; k++)
    b.cover_unreachable[k] = -1;
  b.grenade_cooldown = cod_rng_range(&b.rng, 9.0f, 22.0f);
  b.has_grenade = true;
  b.cover = -1;
  b.cmd_height = b.height;
  b.patrol_count = sp->patrol_count;
  for (u32 i = 0; i < sp->patrol_count && i < CAI_MAX_PATROL; i++)
    b.patrol[i] = sp->patrol[i];
  b.logged_mode = COD_AI_IDLE;

  EcsEntity e = ecs_entity_create(w);
  CodSoldierDrive drive = {.position = sp->pos, .yaw = sp->yaw, .scale = scale, .clip = COD_CLIP_IDLE};
  CodSoldier pub = {.team = COD_TEAM_ENEMY,
                    .variant = variant,
                    .name = fixedstr32_from_cstr("ENEMY"),
                    .health = 100.0f,
                    .alive = true,
                    .position = sp->pos,
                    .yaw = sp->yaw,
                    .head = cai_bind_world(sp->pos, sp->yaw, scale, cod_rig_bind[COD_BONE_HEAD]),
                    .mode = COD_AI_IDLE,
                    .id = b.id};
  ecs_component_set(w, e, CodSoldierBrain, b);
  ecs_component_set(w, e, CodSoldier, pub);
  ecs_component_set(w, e, CodSoldierEventsComp, {0});
  ecs_component_set(w, e, CodSoldierDrive, drive);
  ecs_component_set(w, e, CodSoldierPose, {0});
  quat rot = quat_rotate_y(sp->yaw);
  ecs_component_set(w, e, LocalTransform, {.position = sp->pos, .rotation = rot, .scale = V3_ONE});
  ecs_component_set(w, e, WorldTransform, {.position = sp->pos, .rotation = rot, .scale = V3_ONE});
  // the engine character steps a kinematic capsule at the brain's commanded velocity; its own easing is out of the way
  ecs_component_set(w, e, CharacterTuning,
                    {.mover = CHARACTER_MOVER_KINEMATIC,
                     .move_model = CHARACTER_MOVE_TIME_TO_SPEED,
                     .facing = CHARACTER_FACING_INPUT,
                     .radius = b.radius,
                     .height = b.height,
                     .eye_height = b.eye_height,
                     .walk_speed = CAI_CHAR_SPEED,
                     .run_speed = CAI_CHAR_SPEED,
                     .accel_time = 0.001f,
                     .decel_time = 0.001f,
                     .air_accel_time = 0.001f,
                     .air_decel_time = 0.001f,
                     .gravity = COD_GRAVITY,
                     .max_fall_speed = 55.0f,
                     .slope_limit_deg = 48.0f,
                     .step_height = 0.42f});
  for (u32 i = 0; i < CAI_HITBOX_COUNT; i++) {
    const CaiHitboxDef *def = &cai_hitbox_defs[i];
    v3 a = cai_bind_world(sp->pos, sp->yaw, scale, cod_rig_bind[def->bone_a]);
    v3 c = cai_bind_world(sp->pos, sp->yaw, scale, cod_rig_bind[def->bone_b]);
    EcsEntity h = ecs_entity_create(w);
    ecs_component_set(w, h, CodHitbox, {.soldier = e, .zone = def->zone});
    ecs_component_set(w, h, CodHitboxLink, {.bone_a = def->bone_a, .bone_b = def->bone_b, .radius = def->radius});
    ecs_component_set(w, h, ColliderBody,
                      {.collider = collider_capsule(V3_ZERO, v3_sub(c, a), def->radius * scale),
                       .pos = a,
                       .rot = quat_identity(),
                       .scale = 1.0f,
                       .filter = (PhysicsFilter){.category = COD_COLLIDE_HITBOX, .mask = 0xFFFFFFFFu}});
  }
  return e;
}

// the nearest walkable point for a spawn within a step of the anchor's floor (a crate top is a navmesh island), else the physics floor
hz_internal v3 cai_ground_point(const CaiNav *nav, const CaiQ *q, const CodWorld *world, v3 p, f32 anchor_y) {
  i32 ci = cai_nearest(nav, p.x, p.z, anchor_y, true, 6, CAI_NAV_MAX_STEP);
  if (ci >= 0) {
    i32 nx = (i32)nav->nx;
    return (v3){cai_world_x(nav, ci % nx), nav->floor[ci], cai_world_z(nav, ci / nx)};
  }
  p.y = cai_ground_at(q, world, p.x, p.z, anchor_y + 4.0f);
  return p;
}

// two squads on patrol routes drawn from the world's spawn points, far enough from the player to be found
hz_internal u32 cai_populate(EcsWorld *w, CodAiShared *sh, CodAiState *st, const CodWorld *world, v3 player) {
  u32 count = world->spawn_count;
  if (count == 0)
    return 0;
  CaiQ q = cai_q(w);
  u32 made = 0;
  tctx_temp_allocator_scope(ta, NULL) {
    Allocator *al = &ta.allocator;
    u32 *rank = ALLOC_ARRAY_NO_ZERO(al, u32, count);
    f32 *dist = ALLOC_ARRAY_NO_ZERO(al, f32, count);
    u32 n = 0;
    // by distance from the player, farthest first (a stable sort), keeping those beyond 18 m
    for (u32 i = 0; i < count; i++) {
      f32 d = v3_distance(world->spawns[i].position, player);
      if (d <= 18.0f)
        continue;
      u32 k = n++;
      while (k > 0 && dist[k - 1] < d) {
        dist[k] = dist[k - 1];
        rank[k] = rank[k - 1];
        k--;
      }
      dist[k] = d;
      rank[k] = i;
    }
    u32 *by_near = ALLOC_ARRAY_NO_ZERO(al, u32, n + 1);
    f32 *by_dist = ALLOC_ARRAY_NO_ZERO(al, f32, n + 1);
    const u32 squads = 2, per = 3;
    for (u32 qi = 0; qi < squads && qi < n; qi++) {
      u32 squad = cai_create_squad(sh);
      const CodSpawnPoint *anchor = &world->spawns[rank[qi % n]];
      CaiSpawn sp = {.squad = squad};
      // the route: this spawn point and the two next-nearest ones
      sp.patrol[sp.patrol_count++] = anchor->position;
      u32 m = 0;
      for (u32 i = 0; i < n; i++) {
        if (&world->spawns[rank[i]] == anchor)
          continue;
        f32 d = v3_distance(world->spawns[rank[i]].position, anchor->position);
        u32 k = m++;
        while (k > 0 && by_dist[k - 1] > d) {
          by_dist[k] = by_dist[k - 1];
          by_near[k] = by_near[k - 1];
          k--;
        }
        by_dist[k] = d;
        by_near[k] = rank[i];
      }
      for (u32 i = 0; i < m && i < 2; i++)
        sp.patrol[sp.patrol_count++] = world->spawns[by_near[i]].position;

      for (u32 mi = 0; mi < per; mi++) {
        f32 ja = cod_rng_range(&sh->rng, 0.0f, M_PI * 2.0f);
        f32 jr = cod_rng_range(&sh->rng, 0.8f, 3.2f);
        v3 p = {anchor->position.x + m_cosf(ja) * jr, anchor->position.y, anchor->position.z + m_sinf(ja) * jr};
        p = cai_ground_point(sh->nav, &q, world, p, anchor->position.y);
        sp.pos = p;
        sp.variant = (qi * per + mi) % COD_VARIANT_COUNT;
        sp.yaw = anchor->yaw + cod_rng_signed(&sh->rng) * 0.7f;
        if (cai_spawn_soldier(w, sh, st, &sp))
          made++;
        cai_squad_add(sh, squad);
      }
    }
  }
  log_info("ai: garrison: % enemies in 2 squads", fmt_u32(made));
  return made;
}

// the match start or a changed switch: tear the old soldiers down and build the garrison
ecs_system(cai_spawn_system, EcsResInOut(CodAiShared), EcsResInOut(CodAiState), EcsResIn(CodWorld), EcsResIn(CodPlayerState), EcsResIn(CodDev), EcsResIn(EcsPhysicsWorld),
           EcsResIn(EcsNavigationWorld)) {
  CodAiShared *sh = ecs_sys_get_resource_mut(CodAiShared);
  CodAiState *st = ecs_sys_get_resource_mut(CodAiState);
  const CodWorld *world = ecs_sys_get_resource(CodWorld);
  const CodPlayerState *ps = ecs_sys_get_resource(CodPlayerState);
  const CodDev *dev = ecs_sys_get_resource(CodDev);
  if (!sh->nav || !sh->nav->ready || !sh->navmesh_ready || !world->ready || !ps->ready || dev->state != COD_STATE_PLAYING)
    return;
  b32 changed = dev->restart_serial != sh->populated_serial || dev->no_ai != sh->applied_no_ai;
  if (!changed)
    return;

  // the roster holds each brain's request as of its last update (the brains run after this system)
  for (u32 i = 0; i < sh->roster_count; i++) {
    if (!sh->roster[i].used)
      continue;
    if (sh->roster[i].nav_req)
      ecs_navigation_release(it->world, sh->roster[i].nav_req);
    ecs_entity_destroy(it->world, sh->roster[i].entity);
  }
  mem_zero(sh->squads, sizeof(sh->squads));
  sh->next_slot = 0;
  sh->populated_serial = dev->restart_serial;
  sh->applied_no_ai = dev->no_ai;

  v3 player = v3_add(ps->feet, (v3){0.0f, 1.35f, 0.0f});
  if (!dev->no_ai)
    cai_populate(it->world, sh, st, world, player);
}

// ---- hit volumes: each capsule between two bones of its soldier, last frame's pose ----

ecs_system(cai_hitbox_system, EcsInOut(ColliderBody), EcsIn(CodHitbox), EcsIn(CodHitboxLink), EcsExternIn(CodSoldierPose),
           EcsExternIn(CodSoldierDrive), EcsExternIn(CodSoldier)) {
  EcsWorld *w = it->world;
  for (i32 i = 0; i < it->count; i++) {
    const CodHitbox *hb = ecs_sys_get_component(CodHitbox, i);
    const CodSoldier *s = ecs_component_get(w, hb->soldier, CodSoldier);
    // the volumes go with the man: dead, or torn down for a new match
    if (!s || !s->alive) {
      ecs_entity_destroy(w, it->entities[i]);
      continue;
    }
    const CodSoldierPose *pose = ecs_component_get(w, hb->soldier, CodSoldierPose);
    const CodSoldierDrive *drive = ecs_component_get(w, hb->soldier, CodSoldierDrive);
    if (!pose || !drive)
      continue;
    const CodHitboxLink *link = ecs_sys_get_component(CodHitboxLink, i);
    v3 a = cai_bone(pose, drive->position, drive->yaw, drive->scale, link->bone_a);
    v3 b = cai_bone(pose, drive->position, drive->yaw, drive->scale, link->bone_b);
    ColliderBody *cb = ecs_sys_get_component_mut(ColliderBody, i);
    cb->pos = a;
    cb->collider = collider_capsule(V3_ZERO, v3_sub(b, a), link->radius * drive->scale);
  }
}

// ---- consolidations: grenades, the player's intake, the totals ----

// the lobbed grenades the brains asked for; the body is a weapons entity
ecs_system(cai_grenade_system, EcsInOut(CodSoldierBrain)) {
  for (i32 i = 0; i < it->count; i++) {
    CodSoldierBrain *b = ecs_sys_get_component_mut(CodSoldierBrain, i);
    if (!b->grenade_req)
      continue;
    b->grenade_req = false;
    cod_grenade_spawn(it->world, b->grenade_from, b->grenade_to, it->entities[i], false, false);
  }
}

// soldier fire into the player: a hit is damage from the muzzle, rounds cracking past are suppression
ecs_system(cai_intake_system, EcsIn(CodSoldierEventsComp), EcsResIn(CodPlayerState), EcsResIn(CodClock), EcsExternInOut(CodPlayerIntake)) {
  const CodPlayerState *ps = ecs_sys_get_resource(CodPlayerState);
  const CodClock *clock = ecs_sys_get_resource(CodClock);
  if (!ps->ready || !ps->entity)
    return;
  CodPlayerIntake *intake = ecs_component_get_mut(it->world, ps->entity, CodPlayerIntake);
  if (!intake)
    return;
  for (i32 i = 0; i < it->count; i++) {
    const CodSoldierEvents *ev = &ecs_sys_get_component(CodSoldierEventsComp, i)->ev;
    if (ev->frame != clock->frame || !ev->fired)
      continue;
    const CodRound *round = &ev->round;
    if (round->hit_player && round->hit_damage > 0.0f)
      cod_player_intake_damage(intake, round->hit_damage, round->origin);
    if (ps->dead)
      continue;
    // rounds landing within 3.2 m beside or behind the player; those ahead are the player's own
    for (u32 k = 0; k < round->impact_count; k++) {
      v3 d = v3_sub(round->impacts[k].point, ps->eye);
      f32 dist = v3_length(d);
      if (dist > 3.2f)
        continue;
      f32 inv = 1.0f / (dist > 1e-4f ? dist : 1e-4f);
      if (v3_dot(d, ps->forward) * inv > 0.55f)
        continue;
      intake->suppression += 0.28f * (1.0f - dist / 3.2f);
    }
  }
}

// the match totals: who is alive, the score, the AI cost
ecs_system(cai_totals_system, EcsIn(CodSoldier), EcsIn(CodSoldierBrain), EcsIn(CodSoldierEventsComp), EcsResInOut(CodAiState), EcsResIn(CodClock),
           EcsResIn(CodAiShared)) {
  CodAiState *st = ecs_sys_get_resource_mut(CodAiState);
  const CodClock *clock = ecs_sys_get_resource(CodClock);
  b32 trace_rows = ecs_sys_get_resource(CodAiShared)->trace && clock->frame % 60 == 0;
  for (i32 i = 0; i < it->count; i++) {
    const CodSoldier *s = ecs_sys_get_component(CodSoldier, i);
    const CodSoldierBrain *b = ecs_sys_get_component(CodSoldierBrain, i);
    const CodSoldierEvents *ev = &ecs_sys_get_component(CodSoldierEventsComp, i)->ev;
    if (trace_rows && b->alive)
      log_info("ai: soldier % mode % pos % % % vel % blocked % path %/% cover % hp % target %/% peek % fire % ammo %", fmt_u32(b->id), fmt_u32(b->mode), fmt_f32_p(b->position.x, 2),
               fmt_f32_p(b->position.y, 2), fmt_f32_p(b->position.z, 2), fmt_f32_p(m_sqrtf(b->velocity.x * b->velocity.x + b->velocity.z * b->velocity.z), 2),
               fmt_b32(b->blocked), fmt_u32(b->path_index), fmt_u32(b->path_len), fmt_i32(b->cover), fmt_f32_p(b->health, 0),
               fmt_b32(b->has_target), fmt_b32(b->target_visible), fmt_b32(b->peeking), fmt_b32(b->want_fire), fmt_u32(b->ammo));
    st->agents++;
    if (s->alive) {
      st->alive++;
      v3 chest = v3_sub(s->head, (v3){0.0f, 0.38f, 0.0f});
      f32 d2 = v3_lengthsq(v3_sub(chest, ecs_sys_get_resource(CodAiShared)->player_chest));
      if (d2 < st->nearest_d2) {
        st->nearest_d2 = d2;
        st->nearest_chest = chest;
        st->has_nearest = true;
      }
    }
    if (ev->frame == clock->frame && ev->killed && ev->killed_by_player)
      st->kills_by_player++;
    st->brain_us += b->cost_us;
    if (b->lod_irrelevant && b->alive)
      st->lod_irrelevant++;
  }
}

// resets the per-frame totals before the soldiers are counted
ecs_system(cai_totals_reset_system, EcsResInOut(CodAiState), EcsResIn(CodAiShared), EcsResIn(CodClock), EcsResIn(CodPlayerState)) {
  CodAiState *st = ecs_sys_get_resource_mut(CodAiState);
  const CodPlayerState *ps = ecs_sys_get_resource(CodPlayerState);
  if (ps->ready && ps->dead && !st->player_was_dead)
    st->player_deaths++;
  st->player_was_dead = ps->ready && ps->dead;
  const CodAiShared *sh = ecs_sys_get_resource(CodAiShared);
  const CodClock *clock = ecs_sys_get_resource(CodClock);
  if (sh->trace && st->agents > 0 && clock->frame % 60 == 0)
    log_info("ai: frame % agents % alive % lod % brain % us (sum of soldier updates)", fmt_u32(clock->frame), fmt_u32(st->agents), fmt_u32(st->alive),
             fmt_u32(st->lod_irrelevant), fmt_f32_p(st->brain_us, 1));
  st->agents = 0;
  st->alive = 0;
  st->has_nearest = false;
  st->nearest_d2 = HZ_INF;
  st->lod_irrelevant = 0;
  st->brain_us = 0.0f;
}
