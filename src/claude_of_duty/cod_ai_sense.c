// Claude of Duty AI senses and damage: sight cones, hearing, suppression, hit zones, death.

// what one soldier's update works on: its own rows plus the frame's read-only shared state
typedef struct CaiCtx {
  EcsWorld *w;
  const CodAiShared *sh;
  const CodWorld *world;
  CaiNav *nav;
  CaiQ q;
  EcsEntity self;
  CodSoldierBrain *b;
  CodSoldierDrive *drive;
  CodSoldierEvents *ev;
  const CodSoldierPose *pose;
  const CaiSquad *sq; // NULL without a squad
  f32 dt;
} CaiCtx;

// damage:dealt from the physics carries no incident direction, so a round's hit never teaches the target where it came from
#define CAI_HIT_HAS_DIR 0

#define CAI_VIEW_RANGE 58.0f
#define CAI_VIEW_COS 0.64278761f // cos 50 degrees: the 100 degree sight cone

hz_internal const f32 cai_zone_scale[COD_ZONE_COUNT] = {4.0f, 1.0f, 1.0f, 0.9f, 0.65f, 0.7f};

hz_internal const char *cai_mode_name(u32 m) {
  switch (m) {
  case COD_AI_IDLE: return "idle";
  case COD_AI_PATROL: return "patrol";
  case COD_AI_ALERT: return "alert";
  case COD_AI_COMBAT: return "combat";
  case COD_AI_SUPPRESSED: return "suppressed";
  case COD_AI_FLANK: return "flank";
  case COD_AI_RETREAT: return "retreat";
  default: return "dead";
  }
}

hz_internal v3 cai_eye(const CodSoldierBrain *b) { return (v3){b->position.x, b->position.y + b->eye_height, b->position.z}; }

hz_internal void cai_set_mode(CaiCtx *c, u32 m) {
  CodSoldierBrain *b = c->b;
  if (b->mode == m)
    return;
  if (c->sh->trace)
    log_info("ai: t=% id % % -> % (health % supp % known age %)", fmt_f32_p(c->sh->time, 3), fmt_u32(b->id), fmt_cstr(cai_mode_name(b->mode)),
             fmt_cstr(cai_mode_name(m)), fmt_f32_p(b->health, 0), fmt_f32_p(b->suppression, 2), fmt_f32_p(b->last_known_age, 1));
  b->mode = m;
  b->state_time = 0.0f;
  if (m != COD_AI_COMBAT && m != COD_AI_SUPPRESSED)
    b->peeking = false;
}

// a gunshot or footstep heard from pos with a given loudness in metres: it turns the head and body but never grants a target
hz_internal void cai_hear(CaiCtx *c, v3 pos, f32 loudness) {
  CodSoldierBrain *b = c->b;
  if (!b->alive)
    return;
  f32 d = v3_distance(b->position, pos);
  if (d > loudness)
    return;
  f32 strength = 1.0f - d / loudness;
  b->alertness = m_maxf(b->alertness, m_minf(1.0f, 0.35f + strength));
  if (b->last_known_age > 1.2f || strength > 0.6f) {
    b->last_known = pos;
    b->last_known_age = m_minf(b->last_known_age, 0.35f);
  }
  b->awareness = m_minf(0.85f, b->awareness + strength * 0.5f);
  if (b->mode == COD_AI_IDLE || b->mode == COD_AI_PATROL)
    cai_set_mode(c, COD_AI_ALERT);
}

// rounds cracking past raise suppression, which drives the flinch and the duck
hz_internal void cai_suppress(CodSoldierBrain *b, f32 amount) {
  if (!b->alive)
    return;
  b->suppression = m_minf(1.6f, b->suppression + amount);
  b->alertness = 1.0f;
}

// the perpendicular distance of a soldier's chest from a shot's line
hz_internal f32 cai_distance_to_ray(v3 point, v3 origin, v3 dir, f32 eye_h) {
  f32 px = point.x - origin.x, py = point.y + eye_h * 0.7f - origin.y, pz = point.z - origin.z;
  f32 t = m_maxf(0.0f, px * dir.x + py * dir.y + pz * dir.z);
  f32 ex = px - dir.x * t, ey = py - dir.y * t, ez = pz - dir.z * t;
  return m_sqrtf(ex * ex + ey * ey + ez * ez);
}

// ---- sight ----

hz_internal void cai_sense(CaiCtx *c) {
  CodSoldierBrain *b = c->b;
  const CodAiShared *sh = c->sh;
  if (!sh->player_ready)
    return;
  v3 player = sh->player_chest;
  v3 eye = cai_eye(b);
  v3 to = v3_sub(player, eye);
  f32 dist = v3_length(to);
  b32 visible = false;
  if (dist < CAI_VIEW_RANGE) {
    to = v3_scale(to, 1.0f / dist);
    f32 dot = m_sinf(b->yaw) * to.x + m_cosf(b->yaw) * to.z;
    // peripheral vision widens once alerted
    f32 cone = b->has_target ? -0.2f : CAI_VIEW_COS - b->alertness * 0.25f;
    if (dot > cone || dist < 4.5f)
      visible = cai_los(&c->q, eye, player);
  }
  b->target_visible = visible;
  if (visible) {
    // reaction: fast head-on and close, slow at the edge of vision
    f32 rate = 1.0f / m_maxf(0.12f, 0.16f + dist * 0.0075f + (1.0f - b->alertness) * 0.28f);
    b->awareness = m_minf(1.0f, b->awareness + c->dt * rate);
    b->last_known = player;
    b->last_known_age = 0.0f;
    b->alertness = 1.0f;
    if (b->awareness >= 1.0f)
      b->has_target = true;
  } else {
    b->awareness = m_maxf(0.0f, b->awareness - c->dt * 0.35f);
    if (b->has_target && b->last_known_age > 6.5f)
      b->has_target = false;
  }
}

// ---- damage ----

// which side of the body a world point is on: < 0 right, > 0 left
hz_internal f32 cai_side_of(const CodSoldierBrain *b, v3 p) {
  return (p.x - b->position.x) * m_cosf(b->yaw) - (p.z - b->position.z) * m_sinf(b->yaw);
}

hz_internal void cai_die(CaiCtx *c, v3 point, b32 has_point, b32 has_dir, v3 dir, f32 amount, b32 by_player) {
  CodSoldierBrain *b = c->b;
  if (!b->alive)
    return;
  b->alive = false;
  b->mode = COD_AI_DEAD;
  if (b->nav_req) {
    ecs_navigation_release(c->w, b->nav_req);
    b->nav_req = 0;
  }
  b->want_fire = false;
  b->cover = -1;
  b->flank_ask = b->peek_ask = b->grenade_ask = false;
  // impulse is N s: a 5.56 round carries about 4, so the cap keeps a body from crossing the street
  v3 impulse = v3_scale(v3_normalize(has_dir ? dir : (v3){0.0f, 0.0f, 1.0f}), m_minf(5.5f, 1.5f + amount * 0.02f));
  v3 hit_point = has_point ? point : v3_add(b->position, (v3){0.0f, 1.2f, 0.0f});
  CodSoldierDrive *d = c->drive;
  d->death_serial++;
  d->death_impulse = impulse;
  d->death_point = hit_point;
  d->velocity = (v3){b->velocity.x * 0.6f, 0.0f, b->velocity.z * 0.6f};
  b->cmd_vel = V3_ZERO;
  c->ev->killed = true;
  c->ev->killed_by_player = by_player;
  c->ev->death_impulse = impulse;
  c->ev->death_point = hit_point;
  // the engine character, its input and its hit volumes go with the man; the body takes over the pose
  ecs_component_remove(c->w, c->self, Character);
  ecs_component_remove(c->w, c->self, CharacterInput);
  ecs_component_remove(c->w, c->self, CharacterTuning);
  if (c->sh->trace)
    log_info("ai: t=% id % died (killed by %)", fmt_f32_p(c->sh->time, 3), fmt_u32(b->id), fmt_cstr(by_player ? "player" : "other"));
}

// take a hit of `amount` (post falloff, post zone scale) in a zone at a world point; dir is the round's direction when known
hz_internal void cai_apply_damage(CaiCtx *c, f32 amount, u32 zone, v3 point, b32 has_dir, v3 dir, b32 by_player) {
  CodSoldierBrain *b = c->b;
  if (!b->alive)
    return;
  b->health -= amount;
  b->alertness = 1.0f;
  b->suppression = m_minf(1.6f, b->suppression + 0.35f);
  if (has_dir) {
    v3 from = v3_add(point, v3_scale(dir, -14.0f));
    if (b->last_known_age > 0.5f) {
      b->last_known = from;
      b->last_known_age = 0.4f;
    }
  }
  if (b->mode == COD_AI_IDLE || b->mode == COD_AI_PATROL)
    cai_set_mode(c, COD_AI_ALERT);
  if (b->health <= 0.0f) {
    cai_die(c, point, true, has_dir, dir, amount, by_player);
    return;
  }
  // hit reaction by region, with the side the round came from
  f32 side = 1.0f;
  if (has_dir) {
    f32 s = m_signf(dir.x * m_cosf(b->yaw) - dir.z * m_sinf(b->yaw));
    side = s != 0.0f ? s : 1.0f;
  }
  u32 region = zone == COD_ZONE_HEAD ? COD_ZONE_HEAD : zone == COD_ZONE_ARM ? COD_ZONE_ARM : zone == COD_ZONE_LEG ? COD_ZONE_LEG : COD_ZONE_TORSO;
  CodSoldierDrive *d = c->drive;
  d->hit_serial++;
  d->hit_zone = region;
  d->hit_side = side;
  d->hit_limb = cai_side_of(b, point);
  d->hit_strength = m_minf(1.4f, 0.5f + amount / 45.0f);
  if (zone == COD_ZONE_LEG)
    b->speed *= 0.4f;
}

// damage falloff by the hit's distance from the player: full inside 22 m, tapering to 45 % by 70 m
hz_internal f32 cai_falloff(const CodAiShared *sh, v3 point) {
  if (!sh->player_ready)
    return 1.0f;
  f32 d = v3_distance(sh->player_chest, point);
  return d < 22.0f ? 1.0f : m_maxf(0.45f, 1.0f - (d - 22.0f) * 0.0125f);
}

// the frame's events as this soldier hears and feels them: shots, impacts, footsteps, hits, explosions
hz_internal void cai_events(CaiCtx *c) {
  CodSoldierBrain *b = c->b;
  const CodAiShared *sh = c->sh;
  CodSoldierEvents *ev = c->ev;

  // a gunshot is the loudest thing in the level: everybody hears it, and anyone near the line of fire feels suppressed by it
  for (u32 i = 0; i < sh->shot_count && b->alive; i++) {
    const CaiShot *s = &sh->shots[i];
    cai_hear(c, s->origin, 90.0f);
    f32 d = cai_distance_to_ray(b->position, s->origin, s->dir, b->eye_height);
    if (d < 2.6f)
      cai_suppress(b, 0.45f * (1.0f - d / 2.6f) + 0.12f);
  }
  for (u32 i = 0; i < sh->noise_count && b->alive; i++) {
    f32 d = v3_distance(b->position, sh->noise[i]);
    if (d < 3.2f)
      cai_suppress(b, 0.5f * (1.0f - d / 3.2f));
    else if (d < 12.0f)
      cai_hear(c, sh->noise[i], 12.0f);
  }
  if (sh->footstep && b->alive)
    cai_hear(c, sh->footstep_pos, sh->footstep_running ? 24.0f : 11.0f);

  for (u32 i = 0; i < sh->hit_count; i++) {
    const CaiHit *h = &sh->hits[i];
    if (h->soldier != c->self || !b->alive)
      continue;
    f32 amount = h->damage * cai_falloff(sh, h->point);
    ev->hit = true;
    ev->damage += amount;
    ev->headshot |= h->zone == COD_ZONE_HEAD;
    ev->hit_point = h->point;
    cai_apply_damage(c, amount, h->zone, h->point, CAI_HIT_HAS_DIR, h->dir, h->from_player);
  }

  // a blast: everybody hears it, those in reach with a clear line take damage by the square of their depth in the radius
  for (u32 i = 0; i < sh->explosion_count && b->alive; i++) {
    const CodExplosionEvent *e = &sh->explosions[i];
    f32 radius = e->radius > 0.0f ? e->radius : 5.0f;
    f32 d = v3_distance(b->position, e->position) + 0.001f;
    cai_hear(c, e->position, 120.0f);
    if (d > radius)
      continue;
    v3 eye = cai_eye(b);
    if (!cai_los(&c->q, e->position, eye))
      continue;
    f32 f = 1.0f - d / radius;
    v3 dir = v3_normalize(v3_sub(b->position, e->position));
    cai_suppress(b, 1.4f * f);
    f32 amount = (e->damage > 0.0f ? e->damage : 100.0f) * f * f;
    ev->hit = true;
    ev->damage += amount;
    ev->hit_point = eye;
    cai_apply_damage(c, amount, COD_ZONE_TORSO, eye, true, dir, false); // a blast kill earns no player credit (no damage:dealt)
  }
}
