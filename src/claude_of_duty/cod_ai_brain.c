// Claude of Duty AI brain: the state machine, combat, movement, shooting, grenades and the drive into the animator.

#define CAI_CHAR_SPEED 5.0f // the engine character's walk_speed: CharacterInput.move is a fraction of it
#define CAI_BORE_Y 0.095f

// muzzle, ejection port and bore in the actor's bind space (the rifle seated in the right grip)
typedef struct CaiBindWeapon {
  v3 muzzle, eject, bore;
} CaiBindWeapon;

hz_internal CaiBindWeapon cai_bind_weapon(u32 variant) {
  v3 dir = v3_normalize((v3){0.115f, -0.10f, 1.0f});
  v3 origin = {-0.148f, 1.398f, -0.078f};
  v3 grip = {origin.x + dir.x * 0.26f, origin.y + dir.y * 0.26f - 0.095f, origin.z + dir.z * 0.26f};
  v3 x = v3_normalize(v3_cross((v3){0.0f, 1.0f, 0.0f}, dir));
  v3 y = v3_normalize(v3_cross(dir, x));
  f32 barrel = variant == COD_VARIANT_IRREGULAR ? 0.50f : 0.435f;
  CaiBindWeapon w;
  w.bore = dir;
  w.muzzle = v3_add(grip, v3_add(v3_scale(y, CAI_BORE_Y), v3_scale(dir, barrel + 0.012f)));
  w.eject = v3_add(grip, v3_add(v3_scale(x, -0.024f), v3_add(v3_scale(y, CAI_BORE_Y + 0.012f), v3_scale(dir, 0.012f))));
  return w;
}

// the muzzle, its bore and the ejection port, world space: last frame's pose, else the bind pose
hz_internal void cai_muzzle(const CaiCtx *c, v3 *muzzle, v3 *bore, v3 *eject) {
  const CodSoldierBrain *b = c->b;
  if (c->pose->ready) {
    *muzzle = c->pose->muzzle;
    *bore = v3_normalize(c->pose->muzzle_dir);
    *eject = c->pose->eject;
    return;
  }
  CaiBindWeapon w = cai_bind_weapon(b->variant);
  *muzzle = cai_bind_world(b->position, b->yaw, b->scale, w.muzzle);
  *eject = cai_bind_world(b->position, b->yaw, b->scale, w.eject);
  *bore = v3_normalize(cai_rot_y(w.bore, b->yaw));
}

hz_internal b32 cai_reloading(const CodSoldierBrain *b) { return b->reload_left > 0.0f; }

hz_internal void cai_reload(CaiCtx *c, f32 duration) {
  c->b->reload_left = duration;
  c->drive->reload_serial++;
  c->drive->reload_duration = duration;
}

// ---- movement ----

hz_internal void cai_nav_release(CaiCtx *c) {
  CodSoldierBrain *b = c->b;
  if (b->nav_req) {
    ecs_navigation_release(c->w, b->nav_req);
    b->nav_req = 0;
  }
}

// ask the engine navmesh for a route; the answer lands on a later frame (cai_nav_collect). False when no request slot is free
hz_internal b32 cai_goto(CaiCtx *c, v3 dest) {
  CodSoldierBrain *b = c->b;
  cai_nav_release(c);
  b->nav_cover = b->cover >= 0 && cai_hypotf(dest.x - b->cover_pos.x, dest.z - b->cover_pos.z) < 1e-3f;
  // a sighting is a chest or an eye point: the route goes to the floor under it
  dest.y = cai_ground_at(&c->q, c->world, dest.x, dest.z, dest.y + 0.3f);
  NavPathRequest req = {.start = b->position,
                        .finish = dest,
                        .search_half_extents = {2.0f, CAI_NAV_MAX_STEP, 2.0f}, // a goal beside a crate snaps to the floor, not the crate top
                        .waypoint_spacing = HZ_INF,
                        .max_los_distance = 1000.0f};
  NavRequestId id = ecs_navigation_request(c->w, c->sh->nav_key, &req);
  if (!id) {
    b->has_move_target = false;
    return false;
  }
  b->nav_req = id;
  b->path_len = 0;
  b->path_index = 0;
  b->move_target = dest;
  b->has_move_target = true;
  return true;
}

// the answered route into the brain's path; no route leaves the soldier holding until its next decision
hz_internal void cai_nav_collect(CaiCtx *c) {
  CodSoldierBrain *b = c->b;
  if (!b->nav_req)
    return;
  NavPathResult status = NAV_PATH_IN_PROGRESS;
  const NavWaypoint *wps = NULL;
  i32 count = 0;
  if (!ecs_navigation_result(c->w, b->nav_req, &status, &wps, &count))
    return;
  if (c->sh->trace)
    log_info("ai: id % route status % waypoints % from % % % to % % %", fmt_u32(b->id), fmt_u32(status), fmt_i32(count), fmt_f32_p(b->position.x, 1),
             fmt_f32_p(b->position.y, 2), fmt_f32_p(b->position.z, 1), fmt_f32_p(b->move_target.x, 1), fmt_f32_p(b->move_target.y, 2),
             fmt_f32_p(b->move_target.z, 1));
  if (status == NAV_PATH_SUCCESS && count > 0) {
    u32 n = (u32)count < CAI_MAX_PATH ? (u32)count : CAI_MAX_PATH;
    for (u32 i = 0; i < n; i++)
      b->path[i] = wps[i].pos;
    // a truncated route still ends where the navmesh ends it
    b->path[n - 1] = wps[count - 1].pos;
    b->path_len = n;
    b->path_index = 0;
    b->move_target = b->path[n - 1];
  } else {
    b->has_move_target = false;
    b->path_len = 0;
  }
  // a cover point the route cannot reach, or reaches only short of it, is remembered and never picked again
  if (b->nav_cover && b->cover >= 0) {
    v3 end = b->path_len ? b->path[b->path_len - 1] : b->position;
    f32 walk = 0.0f;
    for (u32 i = 0; i < b->path_len; i++)
      walk += cai_hypotf(b->path[i].x - (i ? b->path[i - 1].x : b->position.x), b->path[i].z - (i ? b->path[i - 1].z : b->position.z));
    f32 straight = cai_hypotf(b->cover_pos.x - b->position.x, b->cover_pos.z - b->position.z);
    // a detour around a building is no cover under fire
    if (!b->path_len || walk > straight * 1.6f + 3.0f || cai_hypotf(end.x - b->cover_pos.x, end.z - b->cover_pos.z) > 0.6f || m_absf(end.y - b->cover_pos.y) > CAI_NAV_MAX_STEP) {
      b->cover_unreachable[b->cover_unreachable_next++ % CAI_COVER_MEMORY] = b->cover;
      b->cover = -1;
      b->has_move_target = false;
      b->path_len = 0;
      b->repath_timer = 0.0f;
    }
  }
  cai_nav_release(c);
}

// blocked by something low: vault it when it is a ledge and not a wall
hz_internal void cai_try_vault(CaiCtx *c) {
  CodSoldierBrain *b = c->b;
  v3 fwd = {m_sinf(b->yaw), 0.0f, m_cosf(b->yaw)};
  CaiRay low;
  if (!cai_ray(&c->q, (v3){b->position.x, b->position.y + 0.35f, b->position.z}, fwd, 0.85f, &low))
    return;
  if (cai_ray_any(&c->q, (v3){b->position.x, b->position.y + 1.25f, b->position.z}, fwd, 1.1f))
    return; // a wall, not a ledge
  f32 lx = b->position.x + fwd.x * 1.5f, lz = b->position.z + fwd.z * 1.5f;
  f32 y = cai_ground_at(&c->q, c->world, lx, lz, b->position.y + 2.2f);
  if (!m_isfinitef(y) || m_absf(y - b->position.y) > 1.3f)
    return;
  b->vault_cooldown = 2.5f;
  c->drive->vault_serial++;
  c->drive->vault_duration = 0.8f;
  b->vault_from = b->position;
  b->vault_to = (v3){lx, y, lz};
  b->vault_t = 0.0f;
  b->vault_left = 0.8f;
  b->vaulting = true;
  if (c->sh->trace)
    log_info("ai: t=% id % vaults from % % % to % % %", fmt_f32_p(c->sh->time, 3), fmt_u32(b->id), fmt_f32_p(b->vault_from.x, 2), fmt_f32_p(b->vault_from.y, 2),
             fmt_f32_p(b->vault_from.z, 2), fmt_f32_p(lx, 2), fmt_f32_p(y, 2), fmt_f32_p(lz, 2));
}

hz_internal void cai_move(CaiCtx *c) {
  CodSoldierBrain *b = c->b;
  const CodAiShared *sh = c->sh;
  f32 dt = c->dt;
  v3 steer = V3_ZERO;
  f32 want = 0.0f;

  cai_nav_collect(c);
  if (b->has_move_target && b->path_index < b->path_len) {
    v3 to = v3_sub(b->path[b->path_index], b->position);
    to.y = 0.0f;
    f32 d = v3_length(to);
    if (d < (b->path_index == b->path_len - 1 ? 0.45f : 0.75f)) {
      b->path_index++;
      if (b->path_index >= b->path_len)
        b->has_move_target = false;
    } else {
      steer = v3_scale(to, 1.0f / d);
      want = b->desired_speed;
    }
  }

  // local avoidance: push off squadmates and steer around them
  for (u32 i = 0; i < sh->roster_count; i++) {
    const CaiRoster *o = &sh->roster[i];
    if (!o->used || !o->alive || o->id == b->id)
      continue;
    f32 dx = b->position.x - o->position.x, dz = b->position.z - o->position.z;
    f32 d2 = dx * dx + dz * dz;
    f32 reach = b->radius + o->radius + 0.42f;
    f32 rr = reach * reach;
    if (d2 > rr || d2 < 1e-6f)
      continue;
    f32 d = m_sqrtf(d2);
    f32 push = (1.0f - d / reach) * 1.5f;
    steer.x += (dx / d) * push;
    steer.z += (dz / d) * push;
    // a tangential bias breaks head-on deadlocks deterministically
    f32 bias = (b->id & 1u) ? 1.0f : -1.0f;
    steer.x += (-dz / d) * push * 0.35f * bias;
    steer.z += (dx / d) * push * 0.35f * bias;
    if (want == 0.0f)
      want = b->desired_speed * 0.35f;
  }
  if (v3_lengthsq(steer) > 1e-6f)
    steer = v3_normalize(steer);

  // speed eases toward the request so starts and stops have weight
  f32 target_speed = want * (b->crouch ? 0.42f : 1.0f) * (1.0f - b->suppression * 0.25f);
  b->speed += (target_speed - b->speed) * m_minf(1.0f, dt * 7.0f);
  if (b->speed < 0.05f)
    b->speed = 0.0f;

  // facing: where we are going, or at the threat when engaged
  b32 engaged = b->mode == COD_AI_COMBAT || b->mode == COD_AI_SUPPRESSED || b->has_target;
  if (engaged && b->last_known_age < 8.0f)
    b->target_yaw = m_atan2f(b->last_known.x - b->position.x, b->last_known.z - b->position.z);
  else if (b->speed > 0.2f)
    b->target_yaw = m_atan2f(steer.x, steer.z);
  f32 dy = b->target_yaw - b->yaw;
  while (dy > M_PI)
    dy -= M_PI * 2.0f;
  while (dy < -M_PI)
    dy += M_PI * 2.0f;
  // a big turn while standing still becomes a real turn-in-place step
  if (m_absf(dy) > 0.9f && b->speed < 0.3f) {
    c->drive->turn_serial++;
    c->drive->turn_dir = dy > 0.0f ? 1.0f : -1.0f;
  }
  f32 turn_rate = b->speed > 0.3f ? 6.5f : 3.4f;
  b->yaw += m_maxf(-turn_rate * dt, m_minf(turn_rate * dt, dy));

  // the engine character integrates this velocity in the fixed steps
  b->cmd_vel = v3_scale(steer, b->speed);
  b->cmd_height = b->crouch ? 1.16f * b->scale : b->height;

  if (b->blocked && b->speed > 1.5f && b->vault_cooldown <= 0.0f && b->grounded && !b->vaulting)
    cai_try_vault(c);
  if (b->blocked && b->speed > 0.5f) {
    b->stuck_timer += dt;
    if (b->stuck_timer > 1.1f) {
      b->stuck_timer = 0.0f;
      b->repath_timer = 0.0f;
      if (b->has_move_target)
        cai_goto(c, b->move_target);
    }
  } else {
    b->stuck_timer = 0.0f;
  }
}

// ---- shooting ----

hz_internal void cai_fire_round(CaiCtx *c) {
  CodSoldierBrain *b = c->b;
  CodSoldierEvents *ev = c->ev;
  v3 origin, bore, eject;
  cai_muzzle(c, &origin, &bore, &eject);
  // the cone of fire: worse when suppressed
  f32 spread = b->spread * (1.0f + b->suppression * 1.5f);
  v3 dir = bore;
  dir.x += cod_rng_gauss(&b->rng) * spread;
  dir.y += cod_rng_gauss(&b->rng) * spread * 0.8f;
  dir.z += cod_rng_gauss(&b->rng) * spread;
  dir = v3_normalize(dir);
  c->drive->fire_serial++;
  c->drive->fire_strength = 1.0f;

  // the round: through the world and the hit volumes, and for the player's capsule
  CodRound *round = &ev->round;
  *round = (CodRound){0};
  round->shooter = (CodShooter){.player = false, .soldier = c->self};
  round->weapon = COD_WEAPON_RIFLE;
  round->origin = origin;
  round->dir = dir;
  cod_ballistics_fire(c->w, round, b->weapon_damage, 0.9f, 200.0f);
  v3 end = round->impact_count ? round->impacts[0].point : v3_add(origin, v3_scale(dir, 120.0f));
  if ((b->id + b->ammo) % 3u == 0) {
    round->has_tracer = true;
    round->tracer = (CodTracer){origin, end, 800.0f};
  }
  ev->fired = true;
  ev->shot = (CodShotEvent){.shooter = round->shooter,
                            .weapon = COD_WEAPON_RIFLE,
                            .origin = origin,
                            .dir = dir,
                            .seed = b->id * 2654435761u + c->sh->frame,
                            .viewmodel = false,
                            .view_muzzle = origin};
  // the ejected case
  v3 vel = v3_add(v3_scale((v3){dir.z, 0.55f, -dir.x}, 2.1f), v3_scale(dir, -0.6f));
  ev->ejected = true;
  ev->eject = (CodEjectEvent){.position = eject, .velocity = vel, .rotation = quat_identity(), .weapon = COD_WEAPON_RIFLE, .case_len = 0.0446f,
                              .case_radius = 0.00495f};
}

hz_internal void cai_shoot(CaiCtx *c) {
  CodSoldierBrain *b = c->b;
  f32 dt = c->dt;
  // where the gun points: lead toward the last known spot with human error, chest rather than feet
  if (b->has_target || b->last_known_age < 3.0f) {
    v3 t = {b->last_known.x, b->last_known.y + 0.05f, b->last_known.z};
    f32 dist = v3_distance(b->position, t);
    f32 wobble_t = c->sh->time * 1.7f + (f32)b->id;
    f32 wob = 0.012f + b->suppression * 0.05f;
    t.x += m_sinf(wobble_t) * wob * dist * 0.12f;
    t.y += m_sinf(wobble_t * 1.7f + 1.1f) * wob * dist * 0.08f;
    t.z += m_cosf(wobble_t * 0.8f) * wob * dist * 0.12f;
    b->aim_target = v3_lerp(b->aim_target, t, m_minf(1.0f, dt * 6.0f));
  } else {
    v3 ahead = {b->position.x + m_sinf(b->yaw) * 12.0f, b->position.y + b->eye_height - 0.1f, b->position.z + m_cosf(b->yaw) * 12.0f};
    b->aim_target = v3_lerp(b->aim_target, ahead, m_minf(1.0f, dt * 3.0f));
  }

  if (!b->want_fire || cai_reloading(b) || b->vaulting)
    return;
  if (b->ammo == 0) {
    cai_reload(c, b->variant == COD_VARIANT_IRREGULAR ? 2.9f : 2.35f);
    c->ev->reloaded = true;
    b->ammo = 30;
    return;
  }
  if (b->burst_left <= 0) {
    if (b->burst_cooldown > 0.0f)
      return;
    b->burst_left = cod_rng_int(&b->rng, 3, 7);
    b->burst_cooldown = cod_rng_range(&b->rng, 0.45f, 1.35f) + b->suppression * 0.5f;
  }
  if (b->fire_cooldown > 0.0f)
    return;
  b->fire_cooldown = 1.0f / b->fire_rate;
  b->burst_left--;
  b->ammo--;
  cai_fire_round(c);
}

// a lobbed grenade at the target: the consolidation spawns the body
hz_internal void cai_throw_grenade(CaiCtx *c, v3 target) {
  CodSoldierBrain *b = c->b;
  b->grenade_cooldown = cod_rng_range(&b->rng, 16.0f, 34.0f);
  b->has_grenade = false;
  b->grenade_ask = false;
  v3 muzzle, bore, eject;
  cai_muzzle(c, &muzzle, &bore, &eject);
  f32 dx = target.x - muzzle.x, dz = target.z - muzzle.z;
  f32 dist = m_maxf(0.5f, cai_hypotf(dx, dz));
  f32 g = COD_GRAVITY;
  f32 speed = m_minf(18.0f, m_sqrtf(m_maxf(4.0f, (dist * g) / 0.95f)));
  f32 vy = speed * 0.62f;
  f32 vh = m_minf(speed, dist / m_maxf(0.35f, (2.0f * vy) / g));
  b->grenade_req = true;
  b->grenade_from = muzzle;
  b->grenade_to = (v3){(dx / dist) * vh, vy, (dz / dist) * vh}; // the launch velocity
  c->drive->fire_serial++;
  c->drive->fire_strength = 0.35f;
  c->ev->threw_grenade = true;
  if (c->sh->trace)
    log_info("ai: t=% id % throws a grenade at % % % from % m", fmt_f32_p(c->sh->time, 3), fmt_u32(b->id), fmt_f32_p(target.x, 1), fmt_f32_p(target.y, 1),
             fmt_f32_p(target.z, 1), fmt_f32_p(dist, 1));
}

// ---- behaviour ----

hz_internal void cai_enter_combat(CaiCtx *c) {
  cai_set_mode(c, COD_AI_COMBAT);
  c->b->cover = -1;
  c->b->repath_timer = 0.0f;
}

hz_internal b32 cai_squad_holds(const CaiSquad *sq, u32 id) {
  for (u32 i = 0; i < sq->holder_count; i++)
    if (sq->holders[i] == id)
      return true;
  return false;
}

hz_internal void cai_combat(CaiCtx *c) {
  CodSoldierBrain *b = c->b;
  const CaiSquad *sq = c->sq;
  f32 dt = c->dt;
  if (!(b->has_target || b->last_known_age < 5.0f)) {
    cai_set_mode(c, COD_AI_ALERT);
    return;
  }
  v3 target = b->last_known;
  f32 dist = v3_distance(b->position, target);

  // wounded and outgunned: fall back
  if (b->health < 34.0f && b->state_time > 1.5f && cod_rng_float(&b->rng) < dt * 0.5f) {
    v3 away = v3_sub(b->position, target);
    away.y = 0.0f;
    away = v3_add(v3_scale(v3_normalize(away), 9.0f), b->position);
    if (cai_goto(c, away)) {
      cai_set_mode(c, COD_AI_RETREAT);
      return;
    }
  }

  // no cover yet, or the current one no longer protects: find one
  if (b->cover < 0 || b->repath_timer <= 0.0f) {
    i32 pick = -1;
    if (c->nav && c->nav->ready)
      pick = cai_cover_pick(c->nav, c->sh, b->position, target, b->id, b->squad, 7.0f, 30.0f, b->cover >= 0 ? 12.0f : 26.0f,
                            b->cover_unreachable, CAI_COVER_MEMORY);
    b->repath_timer = cod_rng_range(&b->rng, 2.2f, 4.5f);
    if (pick >= 0 && pick != b->cover) {
      b->cover = pick;
      b->cover_pos = c->nav->cover[pick].pos;
      cai_goto(c, b->cover_pos);
    }
  }

  // a cover point we cannot reach must not mute the agent for ever: A* may fail, or a path may run out short of the point
  if (b->cover >= 0 && !b->has_move_target && v3_distance(b->position, b->cover_pos) > 0.85f) {
    b->cover = -1;
    b->repath_timer = m_minf(b->repath_timer, 0.6f);
  }

  b32 at_cover = b->cover >= 0 ? v3_distance(b->position, b->cover_pos) < 0.85f : false;
  if (b->cover >= 0 && !at_cover) {
    // moving into position: run, weapon down, no shooting
    b->desired_speed = 4.3f;
    b->crouch = false;
    b->want_fire = false;
    b->aim_weight = 0.35f;
    b->peek_ask = false;
  } else {
    b->desired_speed = 0.0f;
    b->has_move_target = false;
    // peek and shoot, gated by the squad so they alternate: the squad answers an ask before the next frame
    b32 asked_before = b->peek_ask;
    b->peek_ask = sq != NULL;
    b32 allowed = !sq || cai_squad_holds(sq, b->id);
    if (b->peek_timer <= 0.0f && (asked_before || !sq)) {
      b->peeking = allowed && b->target_visible;
      b->peek_timer = b->peeking ? cod_rng_range(&b->rng, 1.1f, 2.4f) : cod_rng_range(&b->rng, 0.7f, 1.8f);
      if (b->peeking && b->cover >= 0) {
        v3 spot;
        b->peek_side = cai_cover_peek(c->nav, &c->q, &c->nav->cover[b->cover], target, b->eye_height, &spot);
        b->cover_pos = spot;
      }
    }
    b->crouch = b->cover >= 0 ? (!c->nav->cover[b->cover].high || !b->peeking) : false;
    b->aim_weight = b->peeking ? 1.0f : 0.55f;
    b->want_fire = b->peeking && b->target_visible && b->has_target && dist < b->weapon_range;
    // suppressing fire at the last known spot even without a clean shot
    if (!b->want_fire && b->has_target && b->last_known_age < 2.2f && b->peeking)
      b->want_fire = cod_rng_float(&b->rng) < 0.35f;
  }

  // flank when the player has been static and we have friends shooting; one flanker, granted by the squad
  if (b->flank_ask) {
    b->flank_ask = false;
    if (sq && sq->flanker == b->id && cai_goto(c, b->flank_dest)) {
      b->cover = -1;
      cai_set_mode(c, COD_AI_FLANK);
      return;
    }
  }
  if (sq && b->state_time > 4.0f && !(b->grenade_cooldown < 0.0f) && sq->flanker == 0 && sq->combat_count >= 2 &&
      cod_rng_float(&b->rng) < dt * 0.25f) {
    f32 side = cod_rng_float(&b->rng) < 0.5f ? 1.0f : -1.0f;
    v3 perp = v3_sub(target, b->position);
    perp.y = 0.0f;
    perp = v3_normalize(perp);
    v3 flank = v3_scale((v3){-perp.z * side, 0.0f, perp.x * side}, cod_rng_range(&b->rng, 8.0f, 15.0f));
    flank = v3_add(v3_add(flank, b->position), v3_scale(perp, 4.0f));
    b->flank_dest = flank;
    b->flank_ask = true;
  }

  // a grenade when the player is pinned and we have line of fire; the squad rations them
  if (b->has_grenade && b->grenade_cooldown <= 0.0f && dist > 8.0f && dist < 26.0f && b->last_known_age < 1.5f) {
    if (!sq || sq->grenade_grant == b->id)
      cai_throw_grenade(c, target);
    else
      b->grenade_ask = true;
  } else {
    b->grenade_ask = false;
  }
}

hz_internal void cai_think(CaiCtx *c) {
  CodSoldierBrain *b = c->b;
  if (b->mode != COD_AI_COMBAT)
    b->peek_ask = b->flank_ask = b->grenade_ask = false;
  switch (b->mode) {
  case COD_AI_IDLE:
    b->desired_speed = 0.0f;
    b->crouch = false;
    if (b->has_target)
      cai_enter_combat(c);
    else if (b->patrol_count && b->state_time > 2.5f)
      cai_set_mode(c, COD_AI_PATROL);
    break;

  case COD_AI_PATROL:
    b->crouch = false;
    b->desired_speed = 1.35f;
    if (b->has_target) {
      cai_enter_combat(c);
      break;
    }
    if (!b->has_move_target || v3_distance(b->position, b->move_target) < 1.1f) {
      if (b->patrol_count) {
        v3 p = b->patrol[b->patrol_index % b->patrol_count];
        b->patrol_index++;
        cai_goto(c, p);
      } else {
        cai_set_mode(c, COD_AI_IDLE);
      }
    }
    break;

  case COD_AI_ALERT:
    b->crouch = false;
    b->desired_speed = 1.5f;
    if (b->has_target) {
      cai_enter_combat(c);
      break;
    }
    // move to the last known position, then look around
    if (b->last_known_age < 8.0f && !b->has_move_target)
      cai_goto(c, b->last_known);
    if (b->state_time > 12.0f)
      cai_set_mode(c, b->patrol_count ? COD_AI_PATROL : COD_AI_IDLE);
    break;

  case COD_AI_COMBAT:
    cai_combat(c);
    break;

  case COD_AI_SUPPRESSED:
    b->crouch = true;
    b->desired_speed = 0.0f;
    b->want_fire = false;
    b->peeking = false;
    if (b->suppression < 0.45f)
      cai_set_mode(c, COD_AI_COMBAT);
    break;

  case COD_AI_FLANK:
    b->crouch = false;
    b->desired_speed = 4.4f;
    b->want_fire = false;
    if (!b->has_move_target || v3_distance(b->position, b->move_target) < 1.2f || b->state_time > 7.0f) {
      cai_set_mode(c, COD_AI_COMBAT);
      b->cover = -1;
    }
    if (b->suppression > 1.0f)
      cai_set_mode(c, COD_AI_COMBAT);
    break;

  case COD_AI_RETREAT:
    b->crouch = false;
    b->desired_speed = 4.6f;
    b->want_fire = false;
    if (!b->has_move_target || v3_distance(b->position, b->move_target) < 1.2f)
      cai_set_mode(c, COD_AI_COMBAT);
    if (b->health > 45.0f && b->state_time > 4.0f)
      cai_set_mode(c, COD_AI_COMBAT);
    break;

  default:
    break;
  }
  if (b->suppression > 1.15f && b->mode == COD_AI_COMBAT && b->cover >= 0)
    cai_set_mode(c, COD_AI_SUPPRESSED);
}

// ---- the drive into the animator ----

hz_internal void cai_drive(CaiCtx *c) {
  CodSoldierBrain *b = c->b;
  CodSoldierDrive *d = c->drive;
  f32 dt = c->dt;
  // root motion for a vault
  b->cmd_teleport = false;
  if (b->vaulting) {
    b->vault_t += dt / 0.8f;
    f32 t = m_minf(1.0f, b->vault_t);
    b->position = v3_lerp(b->vault_from, b->vault_to, t);
    b->position.y += m_sinf(t * M_PI) * 0.42f;
    b->cmd_teleport = true;
    b->cmd_teleport_pos = b->position;
  }
  d->position = b->position;
  d->yaw = b->yaw;
  d->scale = b->scale;

  b32 moving = b->speed > 0.25f;
  u32 clip;
  if (b->crouch)
    clip = moving ? COD_CLIP_CROUCH_WALK : COD_CLIP_CROUCH_IDLE;
  else if (b->speed > 2.6f)
    clip = COD_CLIP_RUN;
  else if (moving)
    clip = COD_CLIP_WALK;
  else
    clip = b->health < 35.0f ? COD_CLIP_HURT_IDLE : COD_CLIP_IDLE;
  d->clip = clip;
  d->speed = b->speed;
  d->crouch = b->crouch;
  d->has_aim = true;
  d->aim_target = b->aim_target;
  d->has_look = true;
  d->look_target = (b->has_target || b->last_known_age < 4.0f) ? b->last_known : b->aim_target;
  d->aim_weight = b->aim_weight;
  d->suppress = m_minf(1.0f, b->suppression * 0.8f);
  d->lod_irrelevant = b->lod_irrelevant;

  // the animator's one-shot timelines run on the same clock
  if (b->vault_left > 0.0f) {
    b->vault_left -= dt;
    if (b->vault_left <= 0.0f)
      b->vaulting = false;
  }
  if (b->reload_left > 0.0f)
    b->reload_left -= dt;
}

hz_internal void cai_update(CaiCtx *c) {
  CodSoldierBrain *b = c->b;
  f32 dt = c->dt;
  b->state_time += dt;
  b->suppression = m_maxf(0.0f, b->suppression - dt * 0.55f);
  b->fire_cooldown -= dt;
  b->burst_cooldown -= dt;
  b->grenade_cooldown -= dt;
  b->peek_timer -= dt;
  b->repath_timer -= dt;
  b->vault_cooldown -= dt;
  if (b->last_known_age < 1e6f)
    b->last_known_age += dt;
  cai_sense(c);
  cai_think(c);
  cai_move(c);
  cai_shoot(c);
  cai_drive(c);
}

// ---- relevance LOD: nothing this soldier does can reach a pixel ----

hz_internal void cai_relevance(CaiCtx *c) {
  CodSoldierBrain *b = c->b;
  const CodAiShared *sh = c->sh;
  b->lod_irrelevant = false;
  if (!sh->frustum_ok)
    return;
  // the body's bounding sphere inflated 1.45x, grown by a further 4 m
  v3 centre = {b->position.x, b->position.y + 0.9f * b->scale, b->position.z};
  f32 radius = 0.97f * b->scale * 1.45f + 4.0f;
  v4 *planes = (v4 *)sh->planes; // the test only reads them
  if (sphere_in_frustum(planes, centre, radius))
    return;
  // the sun shadow could still reach the frustum: sweep the sphere along the light
  f32 floor_y = c->nav ? -6.0f : -20.0f;
  f32 sun_y = m_maxf(0.06f, sh->sun.y);
  f32 t_max = m_minf(320.0f, (centre.y - floor_y) / sun_y);
  f32 step = m_maxf(2.0f, radius * 0.9f);
  for (f32 t = step; t <= t_max; t += step) {
    v3 p = v3_add(centre, v3_scale(sh->sun, -t));
    if (sphere_in_frustum(planes, p, radius))
      return;
  }
  b->lod_irrelevant = true;
}

// ---- the systems ----

// the engine character's state into the brain: position, velocity, whether the last move was held back
ecs_system(cai_pull_system, EcsInOut(CodSoldierBrain), EcsIn(Character)) {
  for (i32 i = 0; i < it->count; i++) {
    CodSoldierBrain *b = ecs_sys_get_component_mut(CodSoldierBrain, i);
    const Character *ch = ecs_sys_get_component(Character, i);
    if (!b->alive)
      continue;
    b->position = ch->position;
    b->velocity = ch->velocity;
    b->grounded = character_grounded(ch);
    f32 planar = cai_hypotf(ch->velocity.x, ch->velocity.z);
    f32 cmd = cai_hypotf(b->cmd_vel.x, b->cmd_vel.z);
    b->blocked = cmd > 0.5f && planar < cmd * 0.75f;
  }
}

// one soldier's update: events, call-outs, then the update (sense, think, move, shoot, drive)
ecs_system(cai_brain_system, EcsInOut(CodSoldierBrain), EcsInOut(CodSoldierDrive), EcsInOut(CodSoldierEventsComp), EcsIn(CodSoldierPose),
           EcsResIn(CodAiShared), EcsResIn(EcsPhysicsWorld), EcsExternIn(ColliderBody), EcsResIn(EcsNavigationWorld)) {
  const CodAiShared *sh = ecs_sys_get_resource(CodAiShared);
  if (!sh->live)
    return;
  CaiCtx c = {.w = it->world, .sh = sh, .world = ecs_resource_get(it->world, CodWorld), .nav = sh->nav, .q = cai_q(it->world), .dt = sh->dt};
  for (i32 i = 0; i < it->count; i++) {
    CodSoldierBrain *b = ecs_sys_get_component_mut(CodSoldierBrain, i);
    if (!b->alive)
      continue;
    u64 t0 = os_time_now();
    CodSoldierEvents *ev = &ecs_sys_get_component_mut(CodSoldierEventsComp, i)->ev;
    *ev = (CodSoldierEvents){.frame = sh->frame};
    c.self = it->entities[i];
    c.b = b;
    c.drive = ecs_sys_get_component_mut(CodSoldierDrive, i);
    c.ev = ev;
    c.pose = ecs_sys_get_component(CodSoldierPose, i);
    c.sq = b->squad ? &sh->squads[b->squad - 1] : NULL;
    // another soldier held the cover point first: pick again
    if (sh->roster[b->slot].used && sh->roster[b->slot].cover_lost) {
      b->cover = -1;
      b->repath_timer = 0.0f;
    }
    cai_relevance(&c);
    cai_events(&c);
    if (b->alive) {
      // a squadmate's call-out gives a direction to check, never a free kill
      const CaiSquad *sq = c.sq;
      if (sq && sq->has_contact && sq->contact_age < 4.0f && !b->has_target && b->last_known_age > 1.5f) {
        b->last_known = sq->contact;
        b->last_known_age = 0.9f + cod_rng_float(&b->rng) * 0.8f;
        b->alertness = 1.0f;
        if (b->mode == COD_AI_IDLE || b->mode == COD_AI_PATROL)
          cai_set_mode(&c, COD_AI_ALERT);
      }
      cai_update(&c);
    }
    b->cost_us = (f32)(os_ticks_to_ms(os_time_diff(os_time_now(), t0)) * 1000.0);
  }
}

// the brain's commands into the engine character: steering, capsule height, the vault's placement
ecs_system(cai_push_system, EcsIn(CodSoldierBrain), EcsInOut(CharacterInput), EcsInOut(CharacterTuning)) {
  for (i32 i = 0; i < it->count; i++) {
    const CodSoldierBrain *b = ecs_sys_get_component(CodSoldierBrain, i);
    if (!b->alive)
      continue;
    CharacterInput *in = ecs_sys_get_component_mut(CharacterInput, i);
    in->move = (v2){b->cmd_vel.x / CAI_CHAR_SPEED, -b->cmd_vel.z / CAI_CHAR_SPEED};
    in->heading = 0.0f;
    in->face_yaw = b->yaw;
    if (b->cmd_teleport) {
      in->teleport = true;
      in->teleport_pos = b->cmd_teleport_pos;
      in->teleport_yaw = b->yaw;
    }
    if (b->cmd_height > 0.0f && ecs_sys_get_component(CharacterTuning, i)->height != b->cmd_height)
      ecs_sys_get_component_mut(CharacterTuning, i)->height = b->cmd_height;
  }
}

// the public per-soldier state the other packages read
ecs_system(cai_publish_system, EcsIn(CodSoldierBrain), EcsIn(CodSoldierPose), EcsInOut(CodSoldier)) {
  for (i32 i = 0; i < it->count; i++) {
    const CodSoldierBrain *b = ecs_sys_get_component(CodSoldierBrain, i);
    const CodSoldierPose *pose = ecs_sys_get_component(CodSoldierPose, i);
    CodSoldier *s = ecs_sys_get_component_mut(CodSoldier, i);
    s->health = b->health;
    s->alive = b->alive;
    s->ragdoll = !b->alive && pose->ragdoll;
    s->position = b->position;
    s->yaw = b->yaw;
    s->head = cai_bone(pose, b->position, b->yaw, b->scale, COD_BONE_HEAD);
    s->mode = b->mode;
  }
}
