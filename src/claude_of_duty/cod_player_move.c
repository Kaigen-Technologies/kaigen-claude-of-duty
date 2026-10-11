// Claude of Duty player: the movement state machine on the engine character.

// one fixed step's working set
typedef struct CodStepCtx {
  EcsWorld *w;
  const CodPhys *phys;
  const Character *c;
  CharacterInput *ci;
  CharacterTuning *tun;
  f32 h;
  b32 e_jump; // the frame's presses, only the first step of a frame holds them
  b32 e_crouch;
  b32 e_prone;
  b32 e_sprint;
  f32 raw_input;
  f32 forward_intent;
  f32 wish_len;
  v3 wish;  // unit, horizontal
  v3 vel;   // the velocity this step builds: x and z are ours, y is the character's
  b32 jumped; // a jump goes to the character this step
  f32 fwd_x; // the yaw basis
  f32 fwd_z;
  f32 right_x;
  f32 right_z;
} CodStepCtx;

hz_internal f32 cod_hypot(f32 x, f32 z) { return m_sqrtf(x * x + z * z); }

hz_internal void cod_move_set_state(CodMove *m, u32 next) {
  if (next == m->state)
    return;
  m->prev_state = m->state;
  m->state = next;
  m->state_time = 0.0f;
}

hz_internal void cod_move_set_stance(CodMove *m, CharacterTuning *tun, u32 stance) {
  CodStanceDef d = cod_stance_def(stance);
  tun->height = d.height;
  tun->step_height = d.step_height;
  if (m->stance != stance)
    m->ev.stance_changed = true;
  m->stance = stance;
}

hz_internal b32 cod_move_fits(const CodStepCtx *s, f32 height) {
  return cod_phys_fits(s->phys, s->c->position.x, s->c->position.y, s->c->position.z, height);
}

// teleport requests are counted so an outside teleport (the --commands hook) is told apart from ours
hz_internal void cod_move_teleport(CodMove *m, CharacterInput *ci, v3 pos, f32 yaw) {
  ci->teleport = true;
  ci->teleport_pos = pos;
  ci->teleport_yaw = yaw;
  m->teleports_requested++;
}

hz_internal void cod_move_tick_timers(CodMove *m, f32 h) {
  m->jump_buffer = m_maxf(0.0f, m->jump_buffer - h);
  m->jump_cooldown = m_maxf(0.0f, m->jump_cooldown - h);
  m->slide_cooldown = m_maxf(0.0f, m->slide_cooldown - h);
  m->mantle_cooldown = m_maxf(0.0f, m->mantle_cooldown - h);
  m->tac_lock = m_maxf(0.0f, m->tac_lock - h);
  m->foot_hold = m_maxf(0.0f, m->foot_hold - h);
  m->ledge_probe_timer = m_maxf(0.0f, m->ledge_probe_timer - h);
  m->lean_probe_timer = m_maxf(0.0f, m->lean_probe_timer - h);
  if (m->grounded) {
    m->coyote = COD_MOVE_COYOTE_TIME;
    m->ground_time += h;
    m->air_time = 0.0f;
  } else {
    m->coyote = m_maxf(0.0f, m->coyote - h);
    m->air_time += h;
    m->ground_time = 0.0f;
  }
}

// ---- stance ----

hz_internal void cod_move_update_stance(CodMove *m, CodStepCtx *s) {
  if (m->sliding) {
    m->stance_want = COD_STANCE_CROUCH;
  } else {
    if (s->e_crouch)
      m->stance_want = m->stance_want == COD_STANCE_CROUCH ? COD_STANCE_STAND : COD_STANCE_CROUCH;
    if (s->e_prone)
      m->stance_want = m->stance_want == COD_STANCE_PRONE ? COD_STANCE_CROUCH : COD_STANCE_PRONE;
    // sprinting always stands you up
    if (m->cmd.sprint_held && s->raw_input > 0.5f && m->stance_want != COD_STANCE_STAND && m->cmd.move.y > 0.5f)
      m->stance_want = COD_STANCE_STAND;
    if (s->e_jump && m->stance_want != COD_STANCE_STAND)
      m->stance_want = COD_STANCE_STAND;
  }
  if (m->stance_want == m->stance)
    return;
  CodStanceDef target = cod_stance_def(m->stance_want);
  // shrinking always succeeds; growing waits for the ceiling, asked again every step
  if (target.height <= cod_stance_def(m->stance).height || cod_move_fits(s, target.height))
    cod_move_set_stance(m, s->tun, m->stance_want);
}

// ---- sprint and tactical sprint ----

hz_internal void cod_move_update_sprint(CodMove *m, CodStepCtx *s) {
  b32 want = m->cmd.sprint_held && s->raw_input > 0.45f && s->forward_intent > COD_MOVE_SPRINT_FORWARD_DOT &&
             m->stance == COD_STANCE_STAND && !m->sliding && m->ads < 0.3f && (m->grounded || m->sprinting);
  if (want) {
    m->sprint_hold += s->h;
    if (m->sprint_hold >= COD_MOVE_SPRINT_START_DELAY)
      m->sprinting = true;
  } else {
    m->sprint_hold = 0.0f;
    m->sprinting = false;
    m->tactical = false;
    m->tac_time = 0.0f;
    m->tac_requested = false;
  }
  if (!m->sprinting)
    return;
  if (m->tac_requested && !m->tactical) {
    m->tactical = true;
    m->tac_time = 0.0f;
  }
  m->tac_requested = false;
  if (m->tactical) {
    m->tac_time += s->h;
    if (m->tac_time > COD_MOVE_TAC_MAX_TIME) {
      m->tactical = false;
      m->tac_lock = COD_MOVE_TAC_RECOVERY;
    }
  }
}

// ---- slide ----

hz_internal void cod_move_slide_finish(CodMove *m) {
  m->sliding = false;
  m->slide_cooldown = COD_SLIDE_COOLDOWN;
  m->slide_time = 0.0f;
  m->stance_want = COD_STANCE_CROUCH;
}

hz_internal void cod_move_do_jump(CodMove *m, CodStepCtx *s) {
  m->jump_buffer = 0.0f;
  m->jump_cooldown = COD_MOVE_JUMP_COOLDOWN;
  m->coyote = 0.0f;
  s->jumped = true;
}

hz_internal void cod_move_begin_slide(CodMove *m, CodStepCtx *s, f32 current_speed) {
  f32 dx = s->vel.x, dz = s->vel.z;
  f32 l = cod_hypot(dx, dz);
  if (l < 0.4f && s->wish_len > 0.1f) {
    dx = s->wish.x;
    dz = s->wish.z;
    l = 1.0f;
  }
  if (l < 1e-4f)
    return;
  dx /= l;
  dz /= l;
  f32 target = m_maxf(COD_SLIDE_MIN_ENTRY, m_minf(COD_SLIDE_ENTRY_SPEED, current_speed * 1.3f));
  s->vel.x = dx * target;
  s->vel.z = dz * target;
  m->slide_dir_x = dx;
  m->slide_dir_z = dz;
  m->slide_side = m->cmd.move.x >= 0.0f ? 1.0f : -1.0f;
  m->slide_time = 0.0f;
  m->sliding = true;
  m->sprinting = false;
  m->tactical = false;
  m->stance_want = COD_STANCE_CROUCH;
  // a slide is a commitment: the capsule drops at once
  cod_move_set_stance(m, s->tun, COD_STANCE_CROUCH);
  cod_move_set_state(m, COD_MOVE_SLIDE);
  m->ev.slide_started = true;
}

// a jump out of a slide keeps the burst, capped so it never compounds into a speed exploit
hz_internal void cod_move_end_slide_into_jump(CodMove *m, CodStepCtx *s) {
  cod_move_slide_finish(m);
  f32 sp = cod_hypot(s->vel.x, s->vel.z);
  f32 cap = COD_PLAYER_SPRINT_SPEED * 1.06f;
  if (sp > cap) {
    s->vel.x *= cap / sp;
    s->vel.z *= cap / sp;
  }
  m->stance_want = COD_STANCE_STAND;
  if (cod_move_fits(s, COD_PLAYER_HEIGHT))
    cod_move_set_stance(m, s->tun, COD_STANCE_STAND);
  cod_move_do_jump(m, s);
}

hz_internal void cod_move_update_slide(CodMove *m, CodStepCtx *s) {
  if (!m->sliding) {
    f32 fast = cod_hypot(s->vel.x, s->vel.z);
    b32 can_start = s->e_crouch && m->sprinting && m->grounded && fast >= COD_SLIDE_MIN_SPEED_TO_START && m->slide_cooldown <= 0.0f &&
                    m->mantle_cooldown <= 0.0f;
    if (can_start)
      cod_move_begin_slide(m, s, fast);
    return;
  }
  m->slide_time += s->h;
  // slide-cancel: a jump out of a slide is the signature CoD movement tech
  if (m->jump_buffer > 0.0f && m->grounded && m->jump_cooldown <= 0.0f) {
    cod_move_end_slide_into_jump(m, s);
    return;
  }
  f32 sp = cod_hypot(s->vel.x, s->vel.z);
  if (s->e_crouch || m->slide_time > COD_SLIDE_DURATION || sp < COD_SLIDE_EXIT_SPEED || (!m->grounded && m->air_time > 0.14f))
    cod_move_slide_finish(m);
}

hz_internal void cod_move_accelerate_slide(CodMove *m, CodStepCtx *s, f32 friction) {
  f32 sp = cod_hypot(s->vel.x, s->vel.z);
  if (sp < 1e-5f)
    return;
  f32 dx = s->vel.x / sp, dz = s->vel.z / sp;

  // steering: lateral authority only, so the slide curves but never pivots
  if (s->wish_len > 0.05f) {
    f32 lat = s->wish.x * -dz + s->wish.z * dx;
    f32 steer = lat * COD_SLIDE_STEER * s->h;
    f32 nx = dx - dz * steer;
    f32 nz = dz + dx * steer;
    f32 l = cod_hypot(nx, nz);
    if (l < 1e-6f)
      l = 1.0f;
    dx = nx / l;
    dz = nz / l;
  }

  // downhill keeps a slide alive, uphill kills it fast
  v3 gn = s->c->ground_normal;
  f32 slope = -(gn.x * dx + gn.z * dz);
  sp += slope * COD_SLIDE_SLOPE_ASSIST * s->h;

  // exponential drag plus a linear brake: the tail must actually terminate
  sp = sp * m_expf(-COD_SLIDE_DRAG * s->h) - COD_SLIDE_BRAKE * s->h;
  sp = m_maxf(sp, 0.0f);

  // surface friction as a rate: sand eats a slide, sheet metal barely touches it
  sp -= sp * m_clampf(friction - 0.55f, 0.0f, 0.8f) * 0.62f * s->h;
  sp = m_maxf(sp, 0.0f);

  s->vel.x = dx * sp;
  s->vel.z = dz * sp;
  m->slide_dir_x = dx;
  m->slide_dir_z = dz;
}

// ---- jump ----

hz_internal void cod_move_update_jump(CodMove *m, CodStepCtx *s) {
  if (m->sliding || s->jumped)
    return;
  if (m->jump_buffer <= 0.0f || m->jump_cooldown > 0.0f)
    return;
  if (!m->grounded && m->coyote <= 0.0f)
    return;
  // you stand up before you jump; if a ceiling forbids it, you do not jump
  if (m->stance != COD_STANCE_STAND) {
    if (!cod_move_fits(s, COD_PLAYER_HEIGHT))
      return;
    cod_move_set_stance(m, s->tun, COD_STANCE_STAND);
    m->stance_want = COD_STANCE_STAND;
  }
  cod_move_do_jump(m, s);
}

// ---- acceleration ----

hz_internal f32 cod_move_target_speed(const CodMove *m) {
  f32 base;
  if (m->sprinting) {
    base = m->tactical ? COD_PLAYER_TAC_SPRINT_SPEED : COD_PLAYER_SPRINT_SPEED;
  } else {
    base = cod_stance_def(m->stance).speed;
    base *= m_lerpf(1.0f, COD_MOVE_ADS_SCALE, m_saturatef(m->ads));
    base *= m->move_speed_scale;
  }
  return base * m_lerpf(1.0f, 0.6f, m_saturatef(m_fabsf(m->lean_amount)));
}

hz_internal void cod_move_accelerate_ground(CodMove *m, CodStepCtx *s, f32 friction) {
  f32 speed = cod_move_target_speed(m) * s->wish_len;
  f32 tx = s->wish.x * speed, tz = s->wish.z * speed;
  f32 dx = tx - s->vel.x, dz = tz - s->vel.z;
  f32 dl = cod_hypot(dx, dz);
  if (dl < 1e-6f)
    return;
  f32 cur = cod_hypot(s->vel.x, s->vel.z);
  f32 rate;
  if (s->raw_input < 0.02f)
    rate = COD_MOVE_STOP_DECEL;
  else if (speed < cur * 0.92f)
    rate = COD_MOVE_GROUND_DECEL;
  else
    rate = COD_MOVE_GROUND_ACCEL;
  // rough ground (sand, dirt) responds a little more sluggishly
  rate *= m_clampf(friction + 0.08f, 0.75f, 1.05f);
  f32 step = rate * s->h;
  if (dl <= step) {
    s->vel.x = tx;
    s->vel.z = tz;
  } else {
    s->vel.x += dx * (step / dl);
    s->vel.z += dz * (step / dl);
  }
}

// a quarter of ground authority, and it may only add speed along the wish direction up to the air cap; momentum is steered, not amplified
hz_internal void cod_move_accelerate_air(CodStepCtx *s) {
  if (s->wish_len < 1e-4f)
    return;
  f32 cap = COD_MOVE_AIR_SPEED_CAP * s->wish_len;
  f32 along = s->vel.x * s->wish.x + s->vel.z * s->wish.z;
  f32 add = cap - along;
  if (add <= 0.0f)
    return;
  f32 accel = COD_MOVE_GROUND_ACCEL * COD_MOVE_AIR_ACCEL_SCALE * s->wish_len * s->h;
  f32 gain = accel < add ? accel : add;
  s->vel.x += s->wish.x * gain;
  s->vel.z += s->wish.z * gain;
}

// ---- mantle and vault ----

// floor-level capsule bottom: the capsule touches the floor
hz_internal v3 cod_move_floor_feet(const Character *c) { return (v3){c->position.x, c->position.y - COD_CHAR_SKIN, c->position.z}; }

hz_internal b32 cod_move_try_ledge(CodMove *m, CodStepCtx *s) {
  if (m->mantle_cooldown > 0.0f || m->sliding)
    return false;
  if (s->wish_len < 0.35f || s->forward_intent < 0.4f)
    return false;
  if (m->stance == COD_STANCE_PRONE || m->ledge_probe_timer > 0.0f)
    return false;

  f32 sp = cod_hypot(s->vel.x, s->vel.z);
  // only probe when something is in the way, when descending onto a lip, or when jump asked for it
  b32 pressing = m->cmd.jump_held || s->e_jump;
  b32 blocked_now = m->blocked && sp > 0.3f;
  b32 descending = !m->grounded && s->c->velocity.y - COD_GRAVITY * s->h < 1.0f;
  // the step-up lifts the capsule onto a knee-high box without a blocked move, so closing at speed must probe too
  b32 closing = m->grounded && sp >= COD_MANTLE_AUTO_SPEED;
  if (!(blocked_now || descending || closing || (pressing && m->grounded)))
    return false;
  // the probe rate scales with speed: a fixed 20 Hz probe skips the narrow window in which a vault is still possible
  m->ledge_probe_timer = m_clampf(0.1f / m_maxf(1.5f, sp), 0.008f, 0.05f);

  CodLedge *r = &m->ledge;
  v3 feet = cod_move_floor_feet(s->c);
  u32 kind = cod_ledge_probe(s->phys, feet, s->wish.x, s->wish.z, COD_PLAYER_HEIGHT, r);
  if (kind == COD_LEDGE_NONE)
    return false;
  b32 auto_ok = r->fast && sp >= COD_MANTLE_AUTO_SPEED;

  // a proactive vault has to be certain, or a staircase turns into a series of animations
  if (closing && !blocked_now && !pressing) {
    f32 reach = COD_MANTLE_PROACTIVE_DISTANCE + sp * COD_MANTLE_PROACTIVE_LOOKAHEAD;
    if (r->distance > reach || r->obstacle_height < s->tun->step_height + 0.07f)
      return false;
  }
  // low obstacles are cleared automatically at speed; anything taller is an explicit action
  if (!auto_ok && !pressing)
    return false;

  f32 side = m->cmd.move.x >= 0.0f ? 1.0f : -1.0f;
  cod_mantle_begin(&m->mantle, r, feet, s->wish.x, s->wish.z, side, sp);
  m->jump_buffer = 0.0f;
  m->sprinting = false;
  m->tactical = false;
  m->intent_vel = (v2){0};
  m->ev.mantled = true;
  m->ev.vaulted = kind == COD_LEDGE_VAULT;
  m->ev.mantle_kind = kind;
  m->ev.mantle_height = r->obstacle_height;
  cod_move_set_state(m, kind == COD_LEDGE_VAULT ? COD_MOVE_VAULT : COD_MOVE_MANTLE);
  // the rooted climb takes the capsule: it stands still this step
  cod_move_teleport(m, s->ci, s->c->position, s->c->yaw);
  return true;
}

// one step of the rooted climb: the capsule is driven along the curve by teleport, the destination was proven clear
hz_internal void cod_move_step_mantle(CodMove *m, CodStepCtx *s) {
  // you cannot peek round a corner with both hands on a ledge
  m->lean_amount = cod_approach(m->lean_amount, 0.0f, COD_LEAN_RATE, s->h);
  m->lean_off_x = s->right_x * m->lean_amount * COD_LEAN_OFFSET;
  m->lean_off_z = s->right_z * m->lean_amount * COD_LEAN_OFFSET;
  b32 alive = cod_mantle_step(&m->mantle, s->h);
  m->intent_vel = (v2){0};
  m->was_grounded = m->grounded;
  m->grounded = false;
  if (alive) {
    v3 p = m->mantle.pos;
    v3 at = {p.x, p.y + COD_CHAR_SKIN, p.z};
    cod_move_teleport(m, s->ci, at, s->c->yaw);
    m->feet = at;
    m->rooted = true;
    return;
  }
  CodMantle *mt = &m->mantle;
  v3 land = {mt->land.x, mt->land.y + COD_CHAR_SKIN, mt->land.z};
  cod_move_teleport(m, s->ci, land, s->c->yaw);
  s->ci->add_velocity = (v3){mt->fx * mt->exit_speed, 0.0f, mt->fz * mt->exit_speed};
  cod_mantle_end(mt);
  m->mantle_cooldown = COD_MANTLE_COOLDOWN;
  m->step_distance = 0.0f;
  m->foot_hold = COD_FOOT_LAND_HOLD;
  m->suppress_landing = true;
  m->rooted = false;
}

// ---- the fixed step: decisions and the velocity the character block integrates ----

hz_internal void cod_move_reset(CodMove *m, CharacterTuning *tun) {
  cod_mantle_end(&m->mantle);
  m->mantle.t = 0.0f;
  m->sliding = false;
  m->sprinting = false;
  m->tactical = false;
  m->tac_requested = false;
  m->tac_time = 0.0f;
  m->sprint_hold = 0.0f;
  m->stance_want = COD_STANCE_STAND;
  m->rooted = false;
  m->suppress_landing = false;
  m->lean_amount = 0.0f;
  m->lean_off_x = 0.0f;
  m->lean_off_z = 0.0f;
  m->step_distance = 0.0f;
  m->bob_phase = 0.0f;
  m->foot_hold = 0.0f;
  m->jump_buffer = 0.0f;
  m->edge_jump = m->edge_crouch = m->edge_prone = m->edge_sprint = false;
  m->blocked = false;
  m->intent_vel = (v2){0};
  m->prev_vy = 0.0f;
  cod_move_set_stance(m, tun, COD_STANCE_STAND);
  cod_move_set_state(m, COD_MOVE_STAND);
}

hz_internal void cod_move_step(CodMove *m, EcsWorld *w, const CodPhys *phys, const Character *c, CharacterInput *ci, CharacterTuning *tun,
                               f32 h) {
  CodStepCtx s = {.w = w, .phys = phys, .c = c, .ci = ci, .tun = tun, .h = h};
  // a teleport that was not ours (the --commands hook) restarts the machine where the character now is
  u32 external = c->teleports - m->teleports_seen;
  u32 ours = m->teleports_requested - m->teleports_acked;
  if (external > ours) {
    cod_move_reset(m, tun);
    m->feet = m->prev_feet = c->position;
    m->ev.reset = true;
  }
  m->teleports_seen = c->teleports;
  m->teleports_acked = m->teleports_requested;

  // a rendered frame holds 0..N fixed steps but one key press: the first step of the frame takes the edges
  s.e_jump = m->edge_jump;
  s.e_crouch = m->edge_crouch;
  s.e_prone = m->edge_prone;
  s.e_sprint = m->edge_sprint;
  m->edge_jump = m->edge_crouch = m->edge_prone = m->edge_sprint = false;

  m->prev_feet = m->feet;
  m->rooted = false;
  ci->heading = m->yaw;
  ci->move = (v2){0};
  ci->run = false;

  m->state_time += h;
  cod_move_tick_timers(m, h);
  f32 sy = m_sinf(m->yaw), cy = m_cosf(m->yaw);
  s.fwd_x = -sy;
  s.fwd_z = -cy;
  s.right_x = cy;
  s.right_z = -sy;

  if (m->mantle.active) {
    cod_move_step_mantle(m, &s);
    return;
  }

  // wish direction with directional speed weighting: slower sideways, slower still backwards
  f32 mx = m->cmd.move.x, my = m->cmd.move.y;
  s.raw_input = cod_hypot(mx, my);
  f32 sx = mx * COD_MOVE_STRAFE_SCALE;
  f32 sz = my >= 0.0f ? my : my * COD_MOVE_BACK_SCALE;
  s.wish_len = cod_hypot(sx, sz);
  if (s.wish_len > 1e-5f) {
    v3 wish = {s.fwd_x * sz + s.right_x * sx, 0.0f, s.fwd_z * sz + s.right_z * sx};
    f32 l = cod_hypot(wish.x, wish.z);
    s.wish = (v3){wish.x / l, 0.0f, wish.z / l};
    s.wish_len = m_minf(s.wish_len, 1.0f);
  } else {
    s.wish_len = 0.0f;
  }
  s.forward_intent = s.raw_input > 1e-4f ? my / s.raw_input : 0.0f;
  s.vel = c->velocity;

  // discrete decisions, in priority order
  cod_move_update_stance(m, &s);
  cod_move_update_sprint(m, &s);
  cod_move_update_slide(m, &s);
  cod_move_update_jump(m, &s);

  f32 friction = 0.9f;
  if (m->grounded)
    friction = cod_surface_friction(cod_entity_surface(w, c->ground_entity));
  if (m->sliding)
    cod_move_accelerate_slide(m, &s, friction);
  else if (m->grounded && !s.jumped)
    cod_move_accelerate_ground(m, &s, friction);
  else
    cod_move_accelerate_air(&s);

  // ledge detection happens before the move, so we never fight the wall
  if (cod_move_try_ledge(m, &s))
    return;

  // the character block integrates: its velocity is replaced by ours through the sanctioned launch input
  ci->add_velocity = (v3){s.vel.x - c->velocity.x, 0.0f, s.vel.z - c->velocity.z};
  ci->jump = s.jumped;
  m->intent_vel = (v2){s.vel.x, s.vel.z};
  m->prev_vy = c->velocity.y - COD_GRAVITY * h;
}

// ---- after the character moved ----

hz_internal void cod_move_emit_footstep(CodMove *m, EcsWorld *w, const CodPhys *phys, const Character *c) {
  f32 sy = m_sinf(m->yaw), cy = m_cosf(m->yaw);
  f32 lateral = m->foot_left ? -COD_FOOT_LATERAL : COD_FOOT_LATERAL;
  f32 fx = c->position.x + cy * lateral;
  f32 fz = c->position.z - sy * lateral;
  f32 floor_y = c->position.y - COD_CHAR_SKIN;
  v3 at = {fx, floor_y, fz};
  u32 surface = cod_entity_surface(w, c->ground_entity);
  // the surface under the foot, not under the capsule centre: a step half on a kerb sounds like the kerb
  CodHit hit;
  if (cod_phys_ray(phys, (v3){fx, floor_y + 0.35f, fz}, (v3){fx, floor_y + 0.35f - COD_FOOT_PROBE, fz}, &hit)) {
    at.y = hit.point.y;
    surface = hit.surface;
  }
  CodPlayerEvents *e = &m->ev;
  e->footstep = true;
  e->footstep_left = m->foot_left;
  e->footstep_running = m->hspeed >= COD_FOOT_RUN_SPEED;
  e->footstep_surface = surface;
  e->footstep_speed = m->hspeed;
  e->footstep_stance = m->stance;
  e->footstep_position = at;
  m->steps_pending++;
  m->step_running = e->footstep_running;
  m->step_stance = m->stance;
}

hz_internal f32 cod_move_probe_lean(const CodMove *m, const CodPhys *phys, const Character *c, f32 side) {
  f32 sy = m_sinf(m->yaw), cy = m_cosf(m->yaw);
  f32 eye = c->position.y - COD_CHAR_SKIN + cod_stance_def(m->stance).eye;
  for (u32 i = 0; i < 3; i++) {
    f32 amt = 1.0f - (f32)i * 0.33f;
    f32 dx = cy * side * COD_LEAN_OFFSET * amt;
    f32 dz = -sy * side * COD_LEAN_OFFSET * amt;
    v3 a = {c->position.x + dx, eye - 0.22f, c->position.z + dz};
    v3 b = {c->position.x + dx, eye + 0.06f, c->position.z + dz};
    if (!cod_phys_overlap(phys, a, b, COD_LEAN_PROBE_RADIUS))
      return amt;
  }
  return 0.0f;
}

hz_internal void cod_move_update_lean(CodMove *m, const CodPhys *phys, const Character *c, f32 h) {
  f32 want = (m->cmd.lean_right ? 1.0f : 0.0f) - (m->cmd.lean_left ? 1.0f : 0.0f);
  if (m->sprinting || m->sliding || !m->grounded || m->stance == COD_STANCE_PRONE)
    want = 0.0f;
  // validated against the world at ~30 Hz: the camera must never poke through a wall
  if (m->lean_probe_timer <= 0.0f) {
    m->lean_probe_timer = 1.0f / 30.0f;
    m->lean_allowed = want == 0.0f ? 0.0f : cod_move_probe_lean(m, phys, c, want);
  }
  m->lean_amount = cod_approach(m->lean_amount, want * m->lean_allowed, COD_LEAN_RATE, h);
  if (m_fabsf(m->lean_amount) < 1e-4f)
    m->lean_amount = 0.0f;
  f32 off = m->lean_amount * COD_LEAN_OFFSET;
  m->lean_off_x = m_cosf(m->yaw) * off;
  m->lean_off_z = -m_sinf(m->yaw) * off;
}

hz_internal void cod_move_resolve_state(CodMove *m, const Character *c) {
  if (m->mantle.active)
    return;
  u32 next;
  if (m->sliding)
    next = COD_MOVE_SLIDE;
  else if (!m->grounded)
    next = c->velocity.y > 0.35f ? COD_MOVE_JUMP : COD_MOVE_FALL;
  else if (m->stance == COD_STANCE_PRONE)
    next = COD_MOVE_PRONE;
  else if (m->stance == COD_STANCE_CROUCH)
    next = COD_MOVE_CROUCH;
  else if (m->sprinting)
    next = m->tactical ? COD_MOVE_TAC_SPRINT : COD_MOVE_SPRINT;
  else
    next = COD_MOVE_STAND;
  cod_move_set_state(m, next);
}

hz_internal void cod_move_post(CodMove *m, EcsWorld *w, const CodPhys *phys, const Character *c, f32 h) {
  if (m->rooted) {
    m->grounded = false;
    return;
  }
  m->feet = c->position;
  m->was_grounded = m->grounded;
  m->grounded = c->ground == CHARACTER_GROUND_ON_GROUND;
  m->speed = v3_length(c->velocity);
  m->hspeed = cod_hypot(c->velocity.x, c->velocity.z);

  v3 delta = v3_sub(m->feet, m->prev_feet);
  f32 want = cod_hypot(m->intent_vel.x, m->intent_vel.y) * h;
  if (want > 1e-4f) {
    f32 got = (delta.x * m->intent_vel.x + delta.z * m->intent_vel.y) / (want / h);
    // lifted onto a step is not blocked: the step-up carries the capsule over knee-high obstacles
    b32 stepped = delta.y > 0.04f;
    m->blocked = got < want * 0.6f && !stepped;
  } else {
    m->blocked = false;
  }

  if (c->jumps != m->jumps_seen) {
    m->jumps_seen = c->jumps;
    m->ev.jumped = true;
    m->ev.jump_position = m->feet;
    cod_move_set_state(m, COD_MOVE_JUMP);
  }

  if (m->grounded && !m->was_grounded) {
    if (m->suppress_landing) {
      m->suppress_landing = false;
    } else {
      f32 impact = m_maxf(c->landings != m->landings_seen ? c->land_speed : 0.0f, -m_minf(0.0f, m->prev_vy));
      m->ev.landed = true;
      m->ev.land_speed = impact;
      m->ev.land_surface = cod_entity_surface(w, c->ground_entity);
      m->ev.land_position = m->feet;
      m->foot_hold = COD_FOOT_LAND_HOLD;
      m->step_distance = 0.0f;
      if (m->sliding)
        cod_move_slide_finish(m);
    }
  }
  m->landings_seen = c->landings;

  f32 moved = cod_hypot(delta.x, delta.z);
  CodStanceDef def = cod_stance_def(m->stance);
  if (m->grounded && !m->sliding) {
    m->step_distance += moved;
    f32 stride = def.stride * (m->sprinting ? 1.28f : 1.0f);
    // one footfall is pi of bob phase, so the camera's horizontal extreme and the footstep are the same event by construction
    m->bob_phase += moved / stride * COD_PI;
    if (m->bob_phase > COD_PI * 4.0f)
      m->bob_phase -= COD_PI * 4.0f;
    if (m->step_distance >= stride && m->hspeed > 0.55f && m->foot_hold <= 0.0f) {
      m->step_distance -= stride;
      m->foot_left = !m->foot_left;
      cod_move_emit_footstep(m, w, phys, c);
    }
  } else if (!m->grounded) {
    m->step_distance = def.stride * 0.55f;
  }

  cod_move_update_lean(m, phys, c, h);
  cod_move_resolve_state(m, c);
}
